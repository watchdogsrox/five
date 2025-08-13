// Rage headers
#include "script/wrapper.h"
#include "diag/output.h"
#include "rline/rlstats.h"
#include "net/nethardware.h"
#include "rline/ros/rlroscommon.h"
#include "rline/ros/rlros.h"
#include "rline/rl.h"
#include "rline/rltitleid.h"
#include <time.h>
#include "script/array.h"

// Framework headers
#include "fwnet/netscgameconfigparser.h"
#include "fwnet/netleaderboardcommon.h"
#include "fwnet/netleaderboardread.h"
#include "fwnet/netleaderboardwrite.h"
#include "fwnet/netleaderboardmgr.h"
#include "fwtl/regdrefs.h"

// Stats headers
#include "Stats/StatsMgr.h"
#include "Stats/StatsInterface.h"
#include "Stats/StatsDataMgr.h"
#include "Stats/StatsUtils.h"
#include "Stats/StatsTypes.h"
#include "Stats/stats_channel.h"

// Network Headers
#include "Network/Network.h"
#include "Network/Sessions/NetworkSession.h"
#include "Network/NetworkInterface.h"
#include "Network/players/NetworkPlayerMgr.h"
#include "Network/NetworkInterface.h"
#include "Network/Live/livemanager.h"
#include "Network/Live/NetworkTelemetry.h"
#include "Network/Live/NetworkDebugTelemetry.h"
#include "Network/Live/NetworkSCPresenceUtil.h"
#if RSG_GEN9
#include "network/Live/NetworkLandingPageStatsMgr.h"
#endif // RSG_GEN9
#include "Network/SocialClubServices/GamePresenceEvents.h"
#include "Network/Stats/networkleaderboardsessionmgr.h"
#include "Network/Stats/NetworkLeaderboardCommon.h"
#include "Network/Stats/NetworkStockMarketStats.h"
#include "Network/SocialClubServices/SocialClubCommunityEventMgr.h"
#include "Network/Cloud/Tunables.h"

#if __NET_SHOP_ACTIVE
#include "Network/Shop/Catalog.h"
#endif //__NET_SHOP_ACTIVE
#include "Network/Shop/VehicleCatalog.h"

// Game headers
#include "event/EventNetwork.h"
#include "Game/cheat.h"
#include "Scene/world/GameWorld.h"
#include "Peds/PlayerInfo.h"
#include "Script/script.h"
#include "fwsys/timer.h"
#include "Debug/bugstar/BugstarIntegrationSwitch.h"
#include "script/script_channel.h"
#include "script/script_helper.h"
#include "modelinfo/PedModelInfo.h"
#include "Peds/ped.h"
#include "script/commands_network.h"
#include "SaveLoad/savegame_defines.h"
#include "commands_stats.h"
#include "weapons/components/WeaponComponentManager.h"
#include "weapons/components/WeaponComponent.h"
#include "weapons/info/WeaponInfoManager.h"
#include "weapons/info/ItemInfo.h"
#include "script/handlers/GameScriptHandlerNetwork.h"
#include "control/stuntjump.h"

#include "Stats/MoneyInterface.h"

#if IS_GEN9_PLATFORM || GEN9_UI_SIMULATION_ENABLED
#include "peds/CriminalCareer/CriminalCareerService.h"
#endif

SCRIPT_OPTIMISATIONS()
FRONTEND_SCRIPT_OPTIMISATIONS()
SAVEGAME_OPTIMISATIONS()

XPARAM(nompsavegame);
PARAM(spewStatsUsage, "Spew Stats usage commands.");
XPARAM(nonetwork);
XPARAM(lan);
XPARAM(cloudForceAvailable);
PARAM(debugSaveRequests, "Debug stats SAVE requests.");
PARAM(disableOnlineStatsAreSynched, "Disable checks for all mp server authoritative stats being synched.");

RAGE_DEFINE_SUBCHANNEL(script, stats, DIAG_SEVERITY_DEBUG3)
#undef __script_channel
#define __script_channel script_stats

RAGE_DEFINE_SUBCHANNEL(stat, savemgr_script, DIAG_SEVERITY_DEBUG3, DIAG_SEVERITY_DEBUG1)
#undef __stat_channel
#define __stat_channel stat_savemgr_script

namespace rage
{
	extern const rlTitleId* g_rlTitleId;
};

/// PURPOSE:
///    These represent commom save types so that we can choose the importance of the save.
enum
{
	//Default save for freemode wher the event is not identifiable.
	SAVETYPE_DEFAULT

	//These are critical save types that represent important events
	,SAVETYPE_XP
	,SAVETYPE_CASH
	,SAVETYPE_END_MATCH
	,SAVETYPE_END_GAMER_SETUP
	,SAVETYPE_END_SHOPPING
	,SAVETYPE_END_GARAGE
};


//Save migration status data.
class scrSaveMigrationStatusData
{
public:
	scrValue        m_InProgress;
	scrTextLabel63  m_State;
	scrValue        m_TransactionId;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrSaveMigrationStatusData);


class MatchStartInfo
{
public:
	scrValue   m_matchType;
	scrValue   m_hashedMac;
	scrValue   m_posixTime;
	scrValue   m_oncalljointime;
	scrValue   m_oncalljoinstate;
	scrValue   m_missionDifficulty;
	scrValue   m_MissionLaunch;
	scrValue   m_RootContentId;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(MatchStartInfo);

//Platform Save migration data.
class scrMigrationSaveData
{
public:
	scrValue m_totalProgressMadeInSp;
	scrValue m_numberOfChars;
	scrValue m_lastchar;
	scrTextLabel63 m_GamerName;
	scrTextLabel63 m_GamerHandle;
	scrTextLabel63 m_pvc;
	scrTextLabel63 m_evc;
	scrTextLabel63 m_bank;
	scrValue m_PADDING_SizeOfColArray0; //This is necessary to match the way the memory structure as laid out in SCRIPT
	scrTextLabel63 m_wallet[MAX_NUM_MP_CHARS];
	scrValue m_PADDING_SizeOfColArray1; //This is necessary to match the way the memory structure as laid out in SCRIPT
	scrValue m_xp[MAX_NUM_MP_CHARS];
	scrValue m_PADDING_SizeOfIsActvie; //This is necessary to match the way the memory structure as laid out in SCRIPT
	scrValue m_isactive[MAX_NUM_MP_CHARS];
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrMigrationSaveData);

class JobActivityInfo
{
public:
	//Integer Values
	scrValue   m_Cash;
	scrValue   m_BetCash;
	scrValue   m_CashStart;
	scrValue   m_CashEnd;
	scrValue   m_MissionLaunch;
	scrValue   m_1stHashMac;
	scrValue   m_1stPosixTime;

	//Boolean  Values
	scrValue   m_isTeamDeathmatch;
	scrValue   m_leftInProgress;
	scrValue   m_dnf; // did not finish
	scrValue   m_aggregateScore;

	//Unsigned Values (must be > 0)
	scrValue   m_XP;
	scrValue   m_HighestKillStreak;
	scrValue   m_Kills;
	scrValue   m_Deaths;
	scrValue   m_Suicides;
	scrValue   m_Rank;
	scrValue   m_CrewId;
	scrValue   m_VehicleId;
	scrValue   m_betWon;
	scrValue   m_survivedWave;
	scrValue   m_TeamId;
	scrValue   m_MatchType;
    scrValue   m_raceType;
    scrValue   m_raceSubtype;
	scrValue   m_matchResult;
	scrValue   m_jpEarned;
	scrValue   m_numLaps;
	scrValue   m_playlistID;
	scrValue   m_celebrationanim;
	scrValue   m_quickplayanim;
	scrValue   m_StuntLaunchCorona;

	//Boolean  Values
	scrValue   m_ishead2head;
	scrValue   m_isPlaylistChallenge;
	scrValue   m_jobVisibilitySetInCorona;
	scrValue   m_weaponsfixed;
	scrValue   m_sessionVisible;
	scrValue   m_jobDifficulty;
	scrValue   m_outfitStyle;
	scrValue   m_outfitChoiceType;
	scrValue   m_playlistHashMac;
	scrValue   m_playlistPosixTime;

	scrValue   m_totalPoints;
	scrValue   m_largeTargetsHit;
	scrValue   m_mediumTargetsHit;
	scrValue   m_smallTargetsHit;
	scrValue   m_tinyTargetsHit;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(JobActivityInfo);

class JobBInfo
{
public:
	//Bool indicating whether the player is the host
	scrValue   m_ishost;
	//Role the player took on
	scrValue   m_role;
	//Cash cut percentage
	scrValue   m_cashcutpercentage;
	//Number of kills
	scrValue   m_kills;
	//Number of deaths
	scrValue   m_deaths;
	//Clothes worn
	scrValue   m_clothes;

	scrValue  m_Cash;
	scrValue  m_CashStart;
	scrValue  m_CashEnd;
	scrValue  m_MissionLaunch;
	scrValue  m_XP;
	scrValue  m_Suicides;
	scrValue  m_Rank;
	scrValue  m_CrewId;
	scrValue  m_TeamId;
	scrValue  m_matchResult;
	scrValue  m_jpEarned;
	scrValue  m_celebrationanim;
	scrValue  m_quickplayanim;
	scrValue  m_sessionVisible;
	scrValue  m_leftInProgress;
	scrValue  m_leaderscashcutpercentage;

	//GENERAL
	scrValue  m_lesterCalled;        //Received Lester Phone Call?	�Bool
	scrValue  m_heistRootConstentId; //Heist root content ID	INT
	scrValue  m_TimePlanningBoard;   //Time Spent on Planning Board	�Int
	scrValue  m_outfitChoiceBy;      //Outfits: Leader choice or crew choice?	�INT
	scrValue  m_outfitChoiceType;    //Outfits: Owned or pre-sets and Outfits: Presets - which preset for which role	�INT
	scrValue  m_outfitStyle;         //overall clothing setup chosen by the leader	�Int
	scrValue  m_difficulty;          //Difficulty set in corona	�Int
	scrValue  m_1stperson;           //NG: 1st person set in corona � forced or allowed choice?	INT
	scrValue  m_medal;               //Which Role got Which Medal	�INT
	scrValue  m_teamLivesUsed;       //Number of Team Lives used?	�Int
	scrValue  m_failureReason;       //Reason for failure	�Int
	scrValue  m_failureRole;         //Which role caused failure	�Int
	scrValue  m_usedQuickRestart;    //0 = no restart used, 1 = Full restart, 2 = Checkpoint	�Int
	scrValue  m_usedTripSkip;        //Used skip trip?	 Bool

	//SPECIFIC MISSIONS
	scrValue  m_spookedCops;   //Prison  - Station - spooked cops?	�bool
	scrValue  m_cashLost;      //Pacific - Finale � amount of Cash lost from being shot	�Int
	scrValue  m_cashPickedUp;  //Pacific - Finale - Cash picked up in cash pick up minigame	�Int

	//ON MISSION 'MINIGAMES'
 	scrValue  m_minigameTimeTaken0;
	scrValue  m_minigameNumberOfTimes0;
	scrValue  m_minigameTimeTaken1;
	scrValue  m_minigameNumberOfTimes1;

	scrValue   m_heistSessionHashMac;
	scrValue   m_heistSessionIdPosixTime;

	scrValue   m_maskChoice;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(JobBInfo);

class JobLTSInfo
{
public:
	//Number of rounds set
	scrValue   m_roundsset;
	//number of rounds played
	scrValue   m_roundsplayed;

	scrValue  m_Cash;
	scrValue  m_BetCash;
	scrValue  m_CashStart;
	scrValue  m_CashEnd;
	scrValue  m_MissionLaunch;
	scrValue  m_1stHashMac;
	scrValue  m_1stPosixTime;
	scrValue  m_XP;
	scrValue  m_HighestKillStreak;
	scrValue  m_Kills;
	scrValue  m_Deaths;
	scrValue  m_Suicides;
	scrValue  m_Rank;
	scrValue  m_CrewId;
	scrValue  m_betWon;
	scrValue  m_TeamId;
	scrValue  m_matchResult;
	scrValue  m_jpEarned;
	scrValue  m_playlistID;
	scrValue  m_celebrationanim;
	scrValue  m_quickplayanim;
	scrValue  m_ishead2head;
	scrValue  m_isPlaylistChallenge;
	scrValue  m_jobVisibilitySetInCorona;
	scrValue  m_weaponsfixed;
	scrValue  m_sessionVisible;
	scrValue  m_leftInProgress;
	scrValue  m_playlistHashMac;
	scrValue  m_playlistPosixTime;
	scrValue   m_outfitStyle;
	scrValue   m_outfitChoiceType;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(JobLTSInfo);

class JobLTSRoundInfo
{
public:
	//Number of rounds ended number
	scrValue   m_roundNumber;
	//number of rounds played so far
	scrValue   m_roundsplayed;

	scrValue   m_Cash;
	scrValue   m_BetCash;
	scrValue   m_CashStart;
	scrValue   m_CashEnd;
	scrValue   m_MissionLaunch;
	scrValue   m_1stHashMac;
	scrValue   m_1stPosixTime;
	scrValue   m_XP;
	scrValue   m_Kills;
	scrValue   m_Deaths;
	scrValue   m_Suicides;
	scrValue   m_Rank;
	scrValue   m_CrewId;
	scrValue   m_betWon;
	scrValue   m_TeamId;
	scrValue   m_matchResult;
	scrValue   m_jpEarned;
	scrValue   m_playlistID;
	scrValue   m_leftInProgress;
	scrValue   m_playlistHashMac;
	scrValue   m_playlistPosixTime;
	scrValue   m_outfitStyle;
	scrValue   m_outfitChoiceType;

};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(JobLTSRoundInfo);

class sMissionEnded
{
public:
	scrValue m_missionId;
	scrValue m_missionTypeId;
	scrValue m_matchId1;
	scrValue m_matchId2;
	scrValue m_bossId1;
	scrValue m_bossId2;
	scrValue m_starterBossId1;
	scrValue m_starterBossId2;
	scrValue m_PlayerParticipated;
	scrValue m_TimeStart;
	scrValue m_TimeEnd;
	scrValue m_Won;
	scrValue m_EndingReason;
	scrValue m_CashEarned;
	scrValue m_RpEarned;
	scrValue m_TargetsKilled;
	scrValue m_InnocentsKilled;
	scrValue m_Deaths;
	scrValue m_LauncherRank;
	scrValue m_MCPointsEarnedFromWinning;
	scrValue m_TimeTakenToComplete;
	scrValue m_TimeTakenForObjective;
	scrValue m_PropertyDamageValue;
	scrValue m_LongestWheelieTime;
	scrValue m_NumberOfCarsKicked;
	scrValue m_PlayersLeftInProgress;
	scrValue m_Location;
	scrValue m_InvitesSent;
	scrValue m_InvitesAccepted;
	scrValue m_BossKilled;
	scrValue m_GoonsKilled;
	scrValue m_PackagesGathered;
	scrValue m_FriendlyAiDeaths;
	scrValue m_RaceToLocationLaunched;
	scrValue m_CratesDestroyed;
	scrValue m_AIKilled;
	scrValue m_AlliesSpawned;
	scrValue m_AlliesDroppedOff;
	scrValue m_VansSpawned;
	scrValue m_VansDestroyed;
	scrValue m_TypeOfBusinesS;
	scrValue m_OtherBusinessOwned;
	scrValue m_StartVolume;
	scrValue m_EndVolume;
	scrValue m_StartSupplyCapacity;
	scrValue m_EndSupplyCapacity;
	scrValue m_SuppliesDestroyed;
	scrValue m_SuppliesDelivered;
	scrValue m_IsMissionAfterFailedDefend;
	scrValue m_CashSpentOnResupplies;
	scrValue m_ProductDestroyed;
	scrValue m_ProductDelivered;
	scrValue m_UpgradesOwned;
	scrValue m_FromHackerTruck;
	scrValue m_properties;
	scrValue m_properties2;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(sMissionEnded);


class sImpExpMissionEnded
{
public:
	scrValue m_missionId;
	scrValue m_missionTypeId;
	scrValue m_matchId1;
	scrValue m_matchId2;
	scrValue m_bossId1;
	scrValue m_bossId2;
	scrValue m_starterBossId1;
	scrValue m_starterBossId2;
	scrValue m_PlayerParticipated;
	scrValue m_TimeStart;
	scrValue m_TimeEnd;
	scrValue m_Won;
	scrValue m_EndingReason;
	scrValue m_CashEarned;
	scrValue m_RpEarned;
	scrValue m_TargetsKilled;
	scrValue m_InnocentsKilled;
	scrValue m_Deaths;
	scrValue m_LauncherRank;
	scrValue m_TimeTakenToComplete;
	scrValue m_PlayersLeftInProgress;
	scrValue m_Location;
	scrValue m_InvitesSent;
	scrValue m_InvitesAccepted;
	scrValue m_BossKilled;
	scrValue m_GoonsKilled;
	scrValue m_CrateAreasDestroyed;
	scrValue m_Vehicle;
	scrValue m_VehicleStyle;
	scrValue m_VehicleHealth;
	scrValue m_VehiclePoached;
	scrValue m_VehiclePoachedMethod;
	scrValue m_Vehicle2;
	scrValue m_Vehicle2Style;
	scrValue m_Vehicle2Health;
	scrValue m_Vehicle2Poached;
	scrValue m_Vehicle2PoachedMethod;
	scrValue m_Vehicle3;
	scrValue m_Vehicle3Style;
	scrValue m_Vehicle3Health;
	scrValue m_Vehicle3Poached;
	scrValue m_Vehicle3PoachedMethod;
	scrValue m_Vehicle4;
	scrValue m_Vehicle4Style;
	scrValue m_Vehicle4Health;
	scrValue m_Vehicle4Poached;
	scrValue m_Vehicle4PoachedMethod;
	scrValue m_BuyerChosen;
	scrValue m_WasMissionCollection;
	scrValue m_Collection1;
	scrValue m_Collection2;
	scrValue m_Collection3;
	scrValue m_Collection4;
	scrValue m_Collection5;
	scrValue m_Collection6;
	scrValue m_Collection7;
	scrValue m_Collection8;
	scrValue m_Collection9;
	scrValue m_Collection10;
	scrValue m_StartVolume;
	scrValue m_EndVolume;
	scrValue m_MissionType;
	scrValue m_FromHackerTruck;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(sImpExpMissionEnded);


class sGunRunningMissionEnded
{
public:
	scrValue m_missionId;
	scrValue m_missionTypeId;
	scrValue m_matchId1;
	scrValue m_matchId2;
	scrValue m_bossId1;
	scrValue m_bossId2;
	scrValue m_PlayerParticipated;
	scrValue m_PlayerRole;
	scrValue m_TimeStart;
	scrValue m_TimeEnd;
	scrValue m_Won;
	scrValue m_EndingReason;
	scrValue m_CashEarned;
	scrValue m_RpEarned;
	scrValue m_TargetsKilled;
	scrValue m_InnocentsKilled;
	scrValue m_Deaths;
	scrValue m_LauncherRank;
	scrValue m_TimeTakenToComplete;
	scrValue m_PlayersLeftInProgress;
	scrValue m_Location;
	scrValue m_InvitesSent;
	scrValue m_InvitesAccepted;
	scrValue m_BossKilled;
	scrValue m_GoonsKilled;
	scrValue m_SuppliesStolen;
	scrValue m_SuppliesOwned;
	scrValue m_ProductsOwned;
	scrValue m_OwnBunker;
	scrValue m_OwnFiringRange;
	scrValue m_OwnGunLocker;
	scrValue m_OwnCaddy;
	scrValue m_OwnTrailer;
	scrValue m_OwnLivingArea;
	scrValue m_OwnPersonalQuarter;
	scrValue m_OwnCommandCenter;
	scrValue m_OwnArmory;
	scrValue m_OwnVehModArmory;
	scrValue m_OwnVehStorage;
	scrValue m_OwnEquipmentUpgrade;
	scrValue m_OwnSecurityUpgrade;
	scrValue m_OwnStaffUpgrade;
	scrValue m_ResuppliesPassed;
	scrValue m_FromHackerTruck;
	scrValue m_properties;
	scrValue m_properties2;
	scrValue m_missionCategory;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(sGunRunningMissionEnded);


class sSmugglerMissionEnded
{
public:
	scrValue m_missionId;
	scrValue m_missionTypeId;
	scrValue m_matchId1;
	scrValue m_matchId2;
	scrValue m_bossId1;
	scrValue m_bossId2;
	scrValue m_PlayerParticipated;
	scrValue m_PlayerRole;
	scrValue m_TimeStart;
	scrValue m_TimeEnd;
	scrValue m_Won;
	scrValue m_EndingReason;
	scrValue m_CashEarned;
	scrValue m_RpEarned;
	scrValue m_TargetsKilled;
	scrValue m_InnocentsKilled;
	scrValue m_Deaths;
	scrValue m_LauncherRank;
	scrValue m_TimeTakenToComplete;
	scrValue m_PlayersLeftInProgress;
	scrValue m_Location;
	scrValue m_InvitesSent;
	scrValue m_InvitesAccepted;
	scrValue m_BossKilled;
	scrValue m_GoonsKilled;
	scrValue m_SuppliesStolen;
	scrValue m_SuppliesOwned;
	scrValue m_ResuppliesPassed;
	scrValue m_OwnHangar;
	scrValue m_OwnFurniture;
	scrValue m_OwnFlooring;
	scrValue m_OwnStyle;
	scrValue m_OwnLivingArea;
	scrValue m_OwnLighting;
	scrValue m_OwnWorkshop;
	scrValue m_AttackType;
	scrValue m_BossType;
	scrValue m_MembersParticipated;
	scrValue m_HangarSlot1;
	scrValue m_HangarSlot2;
	scrValue m_HangarSlot3;
	scrValue m_HangarSlot4;
	scrValue m_HangarSlot5;
	scrValue m_HangarSlot6;
	scrValue m_HangarSlot7;
	scrValue m_HangarSlot8;
	scrValue m_HangarSlot9;
	scrValue m_HangarSlot10;
	scrValue m_Vehicle1Health;
	scrValue m_Vehicle2Health;
	scrValue m_Vehicle3Health;
	scrValue m_SourceAnyType;
	scrValue m_ContrabandType;
	scrValue m_FromHackerTruck;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(sSmugglerMissionEnded);

class sFreemodePrepEnded
{
public:
	scrValue m_missionId;
	scrValue m_missionTypeId;
	scrValue m_matchId1;
	scrValue m_matchId2;
	scrValue m_bossId1;
	scrValue m_bossId2;
	scrValue m_PlayerParticipated;
	scrValue m_PlayerRole;
	scrValue m_TimeStart;
	scrValue m_TimeEnd;
	scrValue m_Won;
	scrValue m_EndingReason;
	scrValue m_CashEarned;
	scrValue m_RpEarned;
	scrValue m_TargetsKilled;
	scrValue m_InnocentsKilled;
	scrValue m_Deaths;
	scrValue m_LauncherRank;
	scrValue m_TimeTakenToComplete;
	scrValue m_PlayersLeftInProgress;
	scrValue m_Location;
	scrValue m_InvitesSent;
	scrValue m_InvitesAccepted;	
	scrValue m_HeistSessionID;
	scrValue m_OwnBase;
	scrValue m_OwnCannon;
	scrValue m_OwnSecurityRoom;
	scrValue m_OwnLounge;
	scrValue m_OwnLivingQuarters;
	scrValue m_OwnTiltRotor;
	scrValue m_OrbitalCannonShots;
	scrValue m_OrbitalCannonKills;
	scrValue m_AssasinationLevel1Calls;
	scrValue m_AssasinationLevel2Calls;
	scrValue m_AssasinationLevel3Calls;
	scrValue m_PrepCompletionType;
	scrValue m_TimeSpentHacking;
	scrValue m_FailedStealth;
	scrValue m_QuickRestart;
	scrValue m_StarterBossId1;
	scrValue m_StarterBossId2;
	scrValue m_SessionID;
	scrValue m_BossType;
	scrValue m_AttackType;
	scrValue m_TimeTakenForObjective;
	scrValue m_EntitiesStolen;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(sFreemodePrepEnded);

class sInstancedHeistEnded
{
public:

	JobBInfo m_JobInfos;
	scrValue m_bossId1;
	scrValue m_bossId2;
	scrValue m_BossType;
	scrValue m_AttackType;
	scrValue m_OwnBase;
	scrValue m_OwnCannon;
	scrValue m_OwnSecurityRoom;
	scrValue m_OwnLounge;
	scrValue m_OwnLivingQuarters;
	scrValue m_OwnTiltRotor;
	scrValue m_OrbitalCannonShots;
	scrValue m_OrbitalCannonKills;
	scrValue m_AssasinationLevel1Calls;
	scrValue m_AssasinationLevel2Calls;
	scrValue m_AssasinationLevel3Calls;
	scrValue m_ObserveTargetCalls;
	scrValue m_PrepCompletionType;
	scrValue m_FailedStealth;
	scrValue m_QuickRestart;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(sInstancedHeistEnded);

class sDarCheckpoint
{
public:

	scrValue m_StartLocation;
	scrValue m_Checkpoint1Complete;
	scrValue m_TimeTakenToCompleteCheckpoint1;
	scrValue m_Checkpoint2Complete;
	scrValue m_TimeTakenToCompleteCheckpoint2;
	scrValue m_Checkpoint3Complete;
	scrValue m_TimeTakenToCompleteCheckpoint3;
	scrValue m_Checkpoint4Complete;
	scrValue m_TimeTakenToCompleteCheckpoint4;
	scrValue m_EndLocation;
	scrValue m_DarAcquired;
	scrValue m_TotalTimeSpent;
	scrValue m_CashEarned;
	scrValue m_RPEarned;
	scrValue m_Replay;
	scrValue m_FailedReason;
	scrValue m_RockstarAccountIndicator;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(sDarCheckpoint);

class sEnterSessionPack
{
public:
	
	scrValue m_Enterp_Prop;
	scrValue m_Enterp_Veh;
	scrValue m_Enterp_Weapon;
	scrValue m_Enterp_Tattoo1;
	scrValue m_Enterp_Tattoo2;
	scrValue m_Enterp_Clothing1;
	scrValue m_Enterp_Clothing2;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(sEnterSessionPack);

class sGunRunningRnD
{
public:
	scrValue m_bossId1;
	scrValue m_bossId2;
	scrValue m_StartorEnd;
	scrValue m_UpgradeType;
	scrValue m_staffPercentage;
	scrValue m_supplyAmount;
	scrValue m_productionAmount;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(sGunRunningRnD);

class sBusinessBattleEnded
{
public:
	scrValue m_missionId;
	scrValue m_missionTypeId;
	scrValue m_bossId1;					//BossId
	scrValue m_bossId2;					//BossId
	scrValue m_matchId1;				//MatchId
	scrValue m_matchId2;				//MatchId
	scrValue m_playerParticipated;      //Whether  or not player participated 
	scrValue m_timeStart;				//Time work started
	scrValue m_timeEnd;					//Time work ended
	scrValue m_won;						//won/lost
	scrValue m_endingReason;            //Reason for script ending - timer ran out, left session, etc
	scrValue m_cashEarned;				//Cash Earned.
	scrValue m_rpEarned;				//RP Earned.
	scrValue m_bossKilled;				//Boss's killed
	scrValue m_goonsKilled;             //Goons killed
	scrValue m_targetLocation;
	scrValue m_launchedByBoss;          //Whether  or not work was launched by this Boss
	scrValue m_starterBossId1;
	scrValue m_starterBossId2;
	scrValue m_playerrole;
	scrValue m_targetskilled;
	scrValue m_timetakentocomplete;
	scrValue m_playersleftinprogress;
	scrValue m_bosstype;
	scrValue m_attacktype;
	scrValue m_packagesstolen;
	scrValue m_participationLevel;
	scrValue m_entitiesstolentype;
	scrValue m_ownnightclub;
	scrValue m_objectivelocation;
	scrValue m_deliverylocation;
	scrValue m_collectiontype;
	scrValue m_enemytype;
	scrValue m_enemyvehicle;
	scrValue m_eventitem;
	scrValue m_iscop;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(sBusinessBattleEnded);

class sYatchEnded
{
public:
	scrValue m_bossId1;
	scrValue m_bossId2;
	scrValue m_matchId1;
	scrValue m_matchId2;
	scrValue m_missionId;
	scrValue m_missionTypeId;
	scrValue m_isboss;
	scrValue m_bosstype;
	scrValue m_playerrole;
	scrValue m_playerParticipated;
	scrValue m_won;
	scrValue m_launchMethod;             // where this mission was launched from (from blip, phoning captain, pause menu)
	scrValue m_endingReason;
	scrValue m_cashEarned;
	scrValue m_rpEarned;
	scrValue m_bossKilled;
	scrValue m_goonsKilled;
	scrValue m_yachtLocation;
	scrValue m_targetskilled;
	scrValue m_timetakentocomplete;
	scrValue m_playersleftinprogress;
	scrValue m_waterBombs;              // number of water bombs dropped(mission Bluffs on Fire)
	scrValue m_lockOn;                  // number of times the player got locked - on(mission Kraken Skulls)
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(sYatchEnded);

class sWarehouseMissionEnded
{
public:
	scrValue m_missionId;
	scrValue m_missionTypeId;
	scrValue m_matchId1;
	scrValue m_matchId2;
	scrValue m_bossId1;
	scrValue m_bossId2;
	scrValue m_PlayerParticipated;
	scrValue m_PlayerRole;
	scrValue m_TimeStart;
	scrValue m_TimeEnd;
	scrValue m_Won;
	scrValue m_EndingReason;
	scrValue m_CashEarned;
	scrValue m_RpEarned;
	scrValue m_TargetsKilled;
	scrValue m_InnocentsKilled;
	scrValue m_Deaths;
	scrValue m_LauncherRank;
	scrValue m_TimeTakenToComplete;
	scrValue m_PlayersLeftInProgress;
	scrValue m_Location;
	scrValue m_InvitesSent;
	scrValue m_InvitesAccepted;
	scrValue m_BossKilled;
	scrValue m_GoonsKilled;
	scrValue m_SuppliesStolen;
	scrValue m_SuppliesOwned;
	scrValue m_ProductsOwned;
	scrValue m_ownnightclub;
	scrValue m_numberoftechnicians;
	scrValue m_ownequipmentupgrade;
	scrValue m_ownhackertruck;
	scrValue m_ownwarehouseb2;
	scrValue m_ownwarehouseb3;
	scrValue m_ownwarehouseb4;
	scrValue m_ownwarehouseb5;
	scrValue m_ownmissilelauncher;
	scrValue m_owndronestation;
	scrValue m_ownhackerbike;
	scrValue m_ownvehicleworkshop;
	scrValue m_quickrestart;
	scrValue m_bosstype;
	scrValue m_attacktype;
	scrValue m_deliveryproduct;
	scrValue m_enemygroup;
	scrValue m_ambushvehicle;
	scrValue m_deliveryvehicle;
	scrValue m_productssellchoice;
	scrValue m_shipmentsize;
	scrValue m_totalunitssold;
	scrValue m_timeusingdrone;
	scrValue m_dronedestroyed;
	scrValue m_totaldronetases;
	scrValue m_iscop;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(sWarehouseMissionEnded);

class sNightclubMissionEnded
{
public:
	scrValue m_missionId;
	scrValue m_missionTypeId;
	scrValue m_matchId1;
	scrValue m_matchId2;
	scrValue m_bossId1;
	scrValue m_bossId2;
	scrValue m_PlayerParticipated;
	scrValue m_PlayerRole;
	scrValue m_TimeStart;
	scrValue m_TimeEnd;
	scrValue m_Won;
	scrValue m_EndingReason;
	scrValue m_CashEarned;
	scrValue m_RpEarned;
	scrValue m_TargetsKilled;
	scrValue m_InnocentsKilled;
	scrValue m_Deaths;
	scrValue m_LauncherRank;
	scrValue m_TimeTakenToComplete;
	scrValue m_PlayersLeftInProgress;
	scrValue m_Location;
	scrValue m_InvitesSent;
	scrValue m_InvitesAccepted;
	scrValue m_BossKilled;
	scrValue m_GoonsKilled;
	scrValue m_SuppliesStolen;
	scrValue m_SuppliesOwned;
	scrValue m_ProductsOwned;
	scrValue m_ownnightclub;
	scrValue m_ownadditionalstaff;
	scrValue m_ownadditionalsecurity;
	scrValue m_ownhackertruck;
	scrValue m_quickrestart;
	scrValue m_bosstype;
	scrValue m_attacktype;
	scrValue m_collectiontype;
	scrValue m_enemygroup;
	scrValue m_ambushvehicle;
	scrValue m_deliverylocation;
	scrValue m_properties;
	scrValue m_properties2;
	scrValue m_launchMethod;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(sNightclubMissionEnded);

class sStoneHatchetEnd
{
public:
	scrValue m_timespent;
	scrValue m_result;
	scrValue m_cashearned;
	scrValue m_rpearned;
	scrValue m_Location;
	scrValue m_Target;
	scrValue m_TargetX;
	scrValue m_TargetY;
	scrValue m_TargetEvasionChoice;
	scrValue m_damagedealt;
	scrValue m_damagerecieved;
	scrValue m_capturedorkilled;
	scrValue m_chestfound;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(sStoneHatchetEnd);

class sCasinoStoryMissionEndedMetric
{
public:
    scrValue m_MatchCreator;
    scrValue m_SessionType;
    scrValue m_PlaylistId;
    scrValue m_Kills;
    scrValue m_Deaths;
    scrValue m_Cash;
    scrValue m_CashStart;
    scrValue m_CashEnd;
    scrValue m_MissionLaunch;
    scrValue m_Difficulty;
    scrValue m_FirstPerson;
    scrValue m_Medal;
    scrValue m_TeamLivesUsed;
    scrValue m_FailureReason;
    scrValue m_UsedQuickRestart;
    scrValue m_IsHost;
    scrValue m_IsSessionVisible;
    scrValue m_LeftInProgress;
    scrValue m_SpookedCops;
    scrValue m_PlayingTime;
    scrValue m_XP;
    scrValue m_Suicides;
    scrValue m_Rank;
    scrValue m_CrewId;
    scrValue m_Result;
    scrValue m_JobPoints;
    scrValue m_UsedVoiceChat;
    scrValue m_HeistSessionId;
    scrValue m_ClosedJob;
    scrValue m_PrivateJob;
    scrValue m_FromClosedFreemode;
    scrValue m_FromPrivateFreemode;
	scrValue m_BossUUID;
	scrValue m_BossUUID2;
    scrValue m_BossType;
    scrValue m_FailedStealth;
    scrValue m_MissionID;
    scrValue m_MissionVariation;
    scrValue m_OwnPenthouse;
    scrValue m_OwnGarage;
    scrValue m_OwnOffice;
    scrValue m_HouseChipsEarned;
	scrValue m_RestartPoint;
	scrValue m_LaunchedByPlayer;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(sCasinoStoryMissionEndedMetric);

class sDispatchEndedMetric
{
public:
	scrValue m_MatchCreator;
	scrValue m_SessionType;
	scrValue m_PlaylistId;
	scrValue m_launchMethod;		// how this mission was launched; accepted/requested dispatch call
	scrValue m_endingReason;		// reason this mission ended; arrest, kill, die, leave
	scrValue m_timeTakenToComplete;	// time taken to complete
	scrValue m_targetsKilled;		// how many hostile mission peds were killed
	scrValue m_cashEarned;			// amount of cash earned
	scrValue m_rpEarned;			// amount of RP earned
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(sDispatchEndedMetric);


class sCasinoMetric
{
public:
	scrValue m_gameType;
	scrValue m_matchType;
	scrValue m_tableID;
	scrValue m_handID;
	scrValue m_endReason;
	scrValue m_allIn;
	scrValue m_chipsDelta;
	scrValue m_finalChipBalance;
	scrValue m_totalPot;
	scrValue m_playersAtTable;
	scrValue m_viewedLegalScreen;
	scrValue m_ownPenthouse;
	scrValue m_betAmount1;
	scrValue m_betAmount2;
	scrValue m_winAmount;
	scrValue m_win;
	scrValue m_cheat;
	scrValue m_timePlayed;
	scrValue m_isHost;
	scrValue m_hostID;
	scrValue m_membership;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(sCasinoMetric);

class sCasinoChipMetric
{
public:
    scrValue m_action;
    scrValue m_transactionID;
    scrValue m_cashAmount;
    scrValue m_chipsAmount;
    scrValue m_reason;
    scrValue m_cashBalance;
    scrValue m_chipBalance;
	scrValue m_item;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(sCasinoChipMetric);

class sRouletteMetric
{
public:
	sCasinoMetric m_casinoMetric;
	scrValue m_iBetStraight;
	scrValue m_iBetSplit;
	scrValue m_iBetStreet;
	scrValue m_iBetCorner;
	scrValue m_iBetFiveNumber;
	scrValue m_iBetSixLine;
	scrValue m_oBetRed;
	scrValue m_oBetBlack;
	scrValue m_oBetOddNumber;
	scrValue m_oBetEvenNumber;
	scrValue m_oBetLowBracket;
	scrValue m_oBetHighBracket;
	scrValue m_oBetDozen;
	scrValue m_oBetColumn;
	scrValue m_winningNumber;
	scrValue m_winningColour;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(sRouletteMetric);

class sBlackjackMetric
{
public:
	sCasinoMetric m_casinoMetric;
	scrValue m_stand;
	scrValue m_hit;
	scrValue m_double;
	scrValue m_split;
	scrValue m_outcome;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(sBlackjackMetric);

class sThreeCardPokerMetric
{
public:
	sCasinoMetric m_casinoMetric;
	scrValue m_pairPlus;
	scrValue m_outcome;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(sThreeCardPokerMetric);

class sSlotMachineMetric
{
public:
	sCasinoMetric m_casinoMetric;
	scrValue m_machineStyle;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(sSlotMachineMetric);

class sInsideTrackMetric
{
public:
	sCasinoMetric m_casinoMetric;
	scrValue m_gameChoice;
	scrValue m_horseChoice;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(sInsideTrackMetric);

class sLuckySevenMetric
{
public:
	sCasinoMetric m_casinoMetric;
	scrValue m_rewardType;
	scrValue m_rewardItem;
	scrValue m_rewardAmount;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(sLuckySevenMetric);

class sCasinoMetricLight
{
public:
	scrValue m_matchType;
	scrValue m_tableID;
	scrValue m_endReason;
	scrValue m_chipsDelta;
	scrValue m_finalChipBalance;
	scrValue m_duration;
	scrValue m_viewedLegalScreen;
	scrValue m_betAmount1;
	scrValue m_betAmount2;
	scrValue m_cheatCount;
	scrValue m_isHost;
	scrValue m_hostID;
	scrValue m_handsPlayed;
	scrValue m_winCount;
	scrValue m_loseCount;
	scrValue m_membership;

};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(sCasinoMetricLight);

class sBlackjackMetricLight
{
public:
	sCasinoMetricLight m_casinoMetric;
	scrValue m_dealerBlackjackCount;
	scrValue m_playerBlackjackCount;
	scrValue m_surrenderCount;
	scrValue m_bustCount;
	scrValue m_7CardCharlieCount;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(sBlackjackMetricLight);

class sSlotMachineMetricLight
{
public:
	sCasinoMetricLight m_casinoMetric;
	scrValue m_machineStyle;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(sSlotMachineMetricLight);

class sInsideTrackMetricLight
{
public:
	sCasinoMetricLight m_casinoMetric;
	scrValue m_individualGameCount;
	scrValue m_mainEventCount;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(sInsideTrackMetricLight);

class sFreemodeCasinoMissionEnded
{
public:
	scrValue m_missionId;
	scrValue m_missionTypeId;
	scrValue m_matchId1;
	scrValue m_matchId2;
	scrValue m_bossId1;
	scrValue m_bossId2;
	scrValue m_PlayerParticipated;
	scrValue m_PlayerRole;
	scrValue m_TimeStart;
	scrValue m_TimeEnd;
	scrValue m_Won;
	scrValue m_EndingReason;
	scrValue m_CashEarned;
	scrValue m_RpEarned;
	scrValue m_TargetsKilled;
	scrValue m_InnocentsKilled;
	scrValue m_Deaths;
	scrValue m_LauncherRank;
	scrValue m_TimeTakenToComplete;
	scrValue m_PlayersLeftInProgress;
	scrValue m_Location;
	scrValue m_InvitesSent;
	scrValue m_InvitesAccepted;
	scrValue m_BossKilled;
	scrValue m_GoonsKilled;
	scrValue m_missionVariation;
	scrValue m_launchMethod;
	scrValue m_ownPenthouse;
	scrValue m_ownGarage;
	scrValue m_ownOffice;
	scrValue m_houseChipsEarned;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(sFreemodeCasinoMissionEnded);

////////////////////////////////////////////////////////////////////////// Heist3

// Matching STRUCT_HEIST3_DRONE
class Heist3DroneInfo
{
public:
	scrValue m_missionName;
	scrValue m_playthroughId;
	scrValue m_endReason;
	scrValue m_time;
	scrValue m_totalDroneTases;
	scrValue m_totalDroneTranq;
	scrValue m_nano;
	scrValue m_cpCollected;
	scrValue m_targetsKilled;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(Heist3DroneInfo);

// Matching STRUCT_ARCADE_CABINET
class ArcadeCabinetInfo
{
public:
	scrValue m_gameType;
	scrValue m_matchId;
	scrValue m_numPlayers;
	scrValue m_level;
	scrValue m_powerUps;
	scrValue m_kills;
	scrValue m_timePlayed;
	scrValue m_score;
	scrValue m_reward;
	scrValue m_challenges;
	scrValue m_location;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(ArcadeCabinetInfo);

// Matching STRUCT_HEIST3_FINALE
class Heist3FinaleInfo
{
public:
	scrValue m_playthroughId;
	scrValue m_missionId;
	scrTextLabel31 m_publicContentId;
	scrValue m_playthroughHashMac;
	scrValue m_bossId1;
	scrValue m_bossId2;
	scrValue m_bosstype;
	scrValue m_launcherRank;
	scrValue m_playerRole;
	scrValue m_endingReason;
	scrValue m_replay;
	scrValue m_rpEarned;
	scrValue m_difficult;
	scrValue m_timeTakenToComplete;
	scrValue m_checkpoint;
	scrValue m_playCount;
	scrValue m_approachBoard;
	scrValue m_approachDirect;
	scrValue m_wCrew;
	scrValue m_wLoadout;
	scrValue m_dCrew;
	scrValue m_vehicleGetaway;
	scrValue m_vehicleSwap;
	scrValue m_hCrew;
	scrValue m_outfitIn;
	scrValue m_outfitOut;
	scrValue m_mask;
	scrValue m_vehicleSwapped;
	scrValue m_useEMP;
	scrValue m_useDrone;
	scrValue m_useThermite;
	scrValue m_useKeycard;
	scrValue m_hack;
	scrValue m_cameras;
	scrValue m_accessPoints;
	scrValue m_vaultTarget;
	scrValue m_vaultAmt;
	scrValue m_dailyCashRoomAmt;
	scrValue m_depositBoxAmt;
	scrValue m_percentage;
	scrValue m_deaths;
	scrValue m_targetsKilled;
	scrValue m_innocentsKilled;
	scrValue m_buyerLocation;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(Heist3FinaleInfo);

// Matching STRUCT_HEIST3_PREP
class Heist3PrepInfo
{
public:
	scrValue m_playthroughId;
	scrValue m_missionName;
	scrValue m_playthroughHashMac;
	scrValue m_playCount;
	scrValue m_approach;
	scrValue m_type;
	scrValue m_bossId1;
	scrValue m_bossId2;
	scrValue m_bosstype;
	scrValue m_launcherRank;
	scrValue m_playerRole;
	scrValue m_playerParticipated;
	scrValue m_difficult;
	scrValue m_timeTakenToComplete;
	scrValue m_endingReason;
	scrValue m_isRivalParty;
	scrValue m_cashEarned;
	scrValue m_rpEarned;
	scrValue m_location;
	scrValue m_invitesSent;
	scrValue m_invitesAccepted;
	scrValue m_deaths;
	scrValue m_targetsKilled;
	scrValue m_innocentsKilled;
	scrValue m_crewMember;
	scrValue m_unlockYohan;
	scrValue m_unlockCharlie;
	scrValue m_unlockChester1;
	scrValue m_unlockChester2;
	scrValue m_unlockZach;
	scrValue m_unlockPatrick;
	scrValue m_unlockAvi;
	scrValue m_unlockPaige;
	scrValue m_accessPoints;
	scrValue m_shipmentsDestroyed;
	scrValue m_vaultTarget;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(Heist3PrepInfo);

// Matching STRUCT_MISSION_VOTE
class MissionVoteInfo
{
public:
	scrTextLabel31 m_mid;
	scrValue m_numPlayers;
	scrValue m_voteChoice;
	scrValue m_result;
	scrValue m_timeOnline;
	scrValue m_voteDiff;
	scrValue m_endBoard;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(MissionVoteInfo);

// Matching STRUCT_NJVS_VOTE
class NjvsVoteInfo
{
public:
	scrTextLabel31 m_mid;
	scrValue m_numPlayers;
	scrTextLabel31 m_voteChoice;
	scrTextLabel31 m_result;
	scrValue m_timeOnline;
	scrValue m_voteDiff;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(NjvsVoteInfo);

////////////////////////////////////////////////////////////////////////// Cops & Crooks

// Matching STRUCT_CNC_ROUND
class CncRoundInfo
{
public:
	scrValue m_matchId1;
	scrValue m_matchId2;
	scrValue m_roundNumber;
	scrValue m_team;
	scrValue m_role;
	scrValue m_gameRank;
	scrValue m_progRank;
	scrValue m_roleChange;
	scrValue m_spawnLoc;
	scrValue m_escapeLoc;
	scrValue m_roundStartTime;
	scrValue m_endReasonStgOne;
	scrValue m_endReasonStgTwo;
	scrValue m_escapeResult;
	scrValue m_durationStgOne;
	scrValue m_durationStgTwo;
	scrValue m_launchMethod;
	scrValue m_jip;
	scrValue m_win;
	scrValue m_xpEarned;
	scrValue m_streetCrimeCount;
	scrValue m_jobCount;
	scrValue m_jobsRemaining;
	scrValue m_copPoints;
	scrValue m_arrests;
	scrValue m_moneyEarned;
	scrValue m_moneyStashed;
	scrValue m_moneyBanked;
	scrValue m_crooksKilled;
	scrValue m_copsKilled;
	scrValue m_deaths;
	scrValue m_copKiller;
	scrValue m_incapacitations;
	scrValue m_moneyMaxHeld;
	scrValue m_endurance;
	scrValue m_spawnGroup;
	scrValue m_escapeTime;
	scrValue m_spectateTime;
	scrValue m_durationVehicleStgOne;
	scrValue m_durationVehicleStgTwo;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(CncRoundInfo);

// Matching STRUCT_CNC_LOBBY
class CncLobbyInfo
{
public:
	scrValue m_matchId1;
	scrValue m_matchId2;
	scrValue m_team;
	scrValue m_gameRank;
	scrValue m_progRank;
	scrValue m_copRole;
	scrValue m_copWeapons;
	scrValue m_copClothing;
	scrValue m_copTattoos;
	scrValue m_copEmote;
	scrValue m_copVehicle;
	scrValue m_copAbilities;
	scrValue m_crookRole;
	scrValue m_crookWeapons;
	scrValue m_crookClothing;
	scrValue m_crookTattoos;
	scrValue m_crookEmote;
	scrValue m_crookVehicle;
	scrValue m_crookAbilities;
	scrValue m_endReason;
	scrValue m_joinFrom;
	scrValue m_copSlot;
	scrValue m_crookSlot;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(CncLobbyInfo);

// Matching STRUCT_CNC_ABILITY
class CncAbilityInfo
{
public:
	scrValue m_matchId1;
	scrValue m_matchId2;
	scrValue m_team;
	scrValue m_role;
	scrValue m_roundNumber;
	scrValue m_stage;
	scrValue m_ability;
	scrValue m_vehicle;
	scrValue m_wantedLvl;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(CncAbilityInfo);

// Matching STRUCT_CNC_INCAPACITATE
class CncIncapacitateInfo
{
public:
	scrValue m_matchId1;
	scrValue m_matchId2;
	scrValue m_incType;
	scrValue m_crookRole;
	scrValue m_crookProgRank;
	scrValue m_crookVehicle;
	scrValue m_copId;
	scrValue m_copRole;
	scrValue m_copProgRank;
	scrValue m_copWeapon;
	scrValue m_copVehicle;
	scrValue m_copKiller;
	scrValue m_wantedLvl;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(CncIncapacitateInfo);

// Matching STRUCT_CNC_JUSTICE
class CncJusticeInfo
{
public:
	scrValue m_matchId1;
	scrValue m_matchId2;
	scrValue m_roundNumber;
	scrValue m_stage;
	scrValue m_crookId;
	scrValue m_crookRole;
	scrValue m_crookProgRank;
	scrValue m_copRole;
	scrValue m_copProgRank;
	scrValue m_crookEndurance;
	scrValue m_copKiller;
	scrValue m_cashLost;
	scrValue m_wantedLvl;
	scrValue m_cashPenalty;
	scrValue m_type;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(CncJusticeInfo);

// Matching STRUCT_CNC_WORK
class CncWorkInfo
{
public:
	scrValue m_matchId1;
	scrValue m_matchId2;
	scrValue m_jobId;
	scrValue m_roundNumber;
	scrValue m_role;
	scrValue m_workType;
	scrValue m_workName;
	scrValue m_endReason;
	scrValue m_dropOff;
	scrValue m_amount;
	scrValue m_bonus;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(CncWorkInfo);

// Matching STRUCT_CNC_SWITCH
class CncSwitchInfo
{
public:
	scrValue m_matchId1;
	scrValue m_matchId2;
	scrValue m_roundNumber;
	scrValue m_prevRole;
	scrValue m_newRole;
	scrValue m_newWeapons;
	scrValue m_newClothing;
	scrValue m_newTattoos;
	scrValue m_newEmote;
	scrValue m_newVehicle;
	scrValue m_slot;
	scrValue m_cashLost;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(CncSwitchInfo);

////////////////////////////////////////////////////////////////////////// Leaderboards

class LeaderboardGamerHandle
{
public:
	scrValue  m_GamerHandle[13];
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(LeaderboardGamerHandle);

class LeaderboardClanIds
{
public:
	scrValue  m_ClanIds;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(LeaderboardClanIds);

class LeaderboardGroup
{
public:
	//Name of the category
	scrTextLabel31  m_Category;
	//ID of the group within the category
	scrTextLabel31  m_Id;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(LeaderboardGroup);

class LeaderboardGroupHandle
{
public:
	LeaderboardGroupHandle() {m_NumGroups.Int = 0;}

	scrValue   m_NumGroups;
	scrValue   m_PADDING_SizeOfColArray; //This is necessary to match the way the memory structure as laid out in SCRIPT
	LeaderboardGroup m_Group[RL_LEADERBOARD2_MAX_GROUPS];
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(LeaderboardGroupHandle);

class LeaderboardRowData
{
public:
	//The padding for the gamer handle is included in the array because the commands that 
	// deal with the gamer handle expect that extra int at the start.
	//Script will never and should never use the handle or it will get an array overrun.
	scrValue                 m_GamerHandle[13];
	scrTextLabel23           m_GamerName;
	LeaderboardGroupHandle   m_GroupSelector;
	scrValue                 m_ClanId;
	scrTextLabel31           m_ClanName;
	scrTextLabel7            m_ClanTag;
	scrValue                 m_Rank;
	scrValue                 m_NumColumnValues;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(LeaderboardRowData);

class LeaderboardUpdateData
{
public:
	scrValue   m_LeaderboardId;
	scrValue   m_ClanId;
	LeaderboardGroupHandle m_GroupHandle;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(LeaderboardUpdateData);

class LeaderboardReadData
{
public:
	scrValue   m_LeaderboardId;
	scrValue   m_LeaderboardType;
	scrValue   m_ClanId;
	LeaderboardGroupHandle m_GroupHandle;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(LeaderboardReadData);

class LeaderboardReadInfo
{
public:
	scrValue   m_LeaderboardId;
	scrValue   m_LeaderboardType;
	scrValue   m_LeaderboardReadIndex;
	scrValue   m_ReturnedRows;
	scrValue   m_TotalRows;
	scrValue   m_LocalGamerRowNumber;
	scrValue   m_LocalGamerRankInt;
	scrValue   m_LocalGamerRankFloat;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(LeaderboardReadInfo);

class LeaderboardCachedDisplayData
{
public:
	static const int MAX_COLUMNS = 32;

public:
	scrValue                m_Id;
	scrTextLabel63          m_GamerName;
	scrTextLabel63          m_CoDriverName;
	LeaderboardGamerHandle  m_GamerHandle;
	LeaderboardGamerHandle  m_CoDriverHandle;
	scrValue                m_CustomVehicle;
	scrValue                m_Rank;
	scrValue                m_RowFlags;
	scrValue                m_NumColumns;
	scrValue                m_ColumnsBitSets;
	scrValue                m_Padding1;
	scrValue                m_fColumnData[MAX_COLUMNS];
	scrValue                m_Padding2;
	scrValue                m_iColumnData[MAX_COLUMNS];
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(LeaderboardCachedDisplayData);

class LeaderboardPredictionRow
{
public:
	static const int MAX_COLUMNS = 32;

public:
	scrValue                m_Id;
	scrValue                m_NumColumns;
	scrValue                m_ColumnsBitSets;
	scrValue                m_Padding1;
	scrValue                m_fColumnData[MAX_COLUMNS];
	scrValue                m_Padding2;
	scrValue                m_iColumnData[MAX_COLUMNS];
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(LeaderboardPredictionRow);

class ItemHashArray
{
public:
	scrValue   m_padding;    // Padding
	scrValue m_Hashs[MetricAMBIENT_MISSION_CRATEDROP::MAX_CRATE_ITEM_HASHS];
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(ItemHashArray);

class VideoClipMetric
{
public:
	scrValue m_characterHash;
	scrValue m_timeOfDay;
	scrValue m_weather;
	scrValue m_wantedLevel;
	scrValue m_pedDensity;
	scrValue m_vehicleDensity;
	scrValue m_restrictedArea;
	scrValue m_invulnerability;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(VideoClipMetric);

//////////////////////////////////////////////////////////////////////////

class scrFmEventAmbientMission
{
public:
	scrValue m_uid0;                  //Unique Mission ID - for that instance, shared between participating players.
	scrValue m_uid1;                  //Unique Mission ID - for that instance, shared between participating players.
	scrValue m_playersNotified;       //Players Notified.
	scrValue m_playersParticipating;  //Number of Players Participating, who collected a crate in any given drop.
	scrValue m_playersLeftInProgress; //Players who left in progress.
	scrValue m_endReason;             //Reason why script ended.
	scrValue m_cashEarned;            //Cash Earned.
	scrValue m_rpEarned;              //RP Earned.
	scrValue m_notifiedTime;          //Notification PosixTime.
	scrValue m_startTime;             //Start PosixTime.
	scrValue m_timeTakenToComplete;   //Time Taken to complete.
	scrValue m_timeTakenForObjective; //Time between notifications being sent and someone actually completing of the objectives.
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrFmEventAmbientMission);

class scrFmEventAmbMission_Challenges : public scrFmEventAmbientMission
{
public:
	scrValue m_challengeType; //Challenge type (12 in total) 
	scrValue m_challengeScore; 
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrFmEventAmbMission_Challenges);

class scrFmEventAmbMission_VehicleTarget : public scrFmEventAmbientMission
{
public:
	scrValue m_vehicleType;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrFmEventAmbMission_VehicleTarget);

class scrFmEventAmbMission_UrbanWarfare : public scrFmEventAmbientMission
{
public:
	scrValue m_vehicleType;
	scrValue m_waveReached;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrFmEventAmbMission_UrbanWarfare);

class scrFmEventAmbMission_CheckpointCollection : public scrFmEventAmbientMission
{
public:
	scrValue m_quarterMapChosen;        //Quarter of the map chosen
	scrValue m_numCheckpointsCollected; //Number of checkpoints collected
	//Location of any Checkpoints NOT collected
	scrValue m_notCollectedCheckpoints_0;
	scrValue m_notCollectedCheckpoints_1;
	scrValue m_notCollectedCheckpoints_2;
	scrValue m_notCollectedCheckpoints_3;
	scrValue m_notCollectedCheckpoints_4;
	scrValue m_notCollectedCheckpoints_5;
	scrValue m_notCollectedCheckpoints_6;
	scrValue m_notCollectedCheckpoints_7;
	scrValue m_notCollectedCheckpoints_8;
	scrValue m_notCollectedCheckpoints_9;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrFmEventAmbMission_CheckpointCollection);

class scrFmEventAmbMission_PlaneDrop : public scrFmEventAmbientMission
{
public:
	scrValue m_routeTaken;         //Route taken
	scrValue m_numCratesCollected; //Number of crates collected
	scrValue m_numCratesDropped;   //Number of crates dropped
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrFmEventAmbMission_PlaneDrop);

class scrFmEventAmbMission_ATOB : public scrFmEventAmbientMission
{
public:
	scrValue m_vehicleType;   //Vehicle Type
	scrValue m_parTimeBeaten; //Par Time Beaten?
	scrValue m_type;          //Hash of type of Time Trial the player engaged with - Time Trial FME, RC Time Trial, Hao Time Trial
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrFmEventAmbMission_ATOB);

class scrFmEventAmbMission_TargetAssassination : public scrFmEventAmbientMission
{
public:
	scrValue m_targetsKilled; //Targets killed
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrFmEventAmbMission_TargetAssassination);

class scrFmEventAmbMission_KingoftheCastle : public scrFmEventAmbientMission
{
public:
	scrValue m_numOfKings;        //Number of times briefcase exchanged hands
	scrValue m_killsAsKing;       //Kills as King
	scrValue m_deathsAsKing;      //Deaths as King
	scrValue m_areaControlPoints; //Area Control Points
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrFmEventAmbMission_KingoftheCastle);

class scrFmEventAmbMission_CriminalDamage : public scrFmEventAmbientMission
{
public:
	scrValue m_propertyDamageValue; //Total Property Damage value per player ($)
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrFmEventAmbMission_CriminalDamage);

class scrFmEventAmbMission_PasstheParcel : public scrFmEventAmbientMission
{
public:
	scrValue m_numTimesVehicleExchangeHands; //Number of times vehicle exchanged hands
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrFmEventAmbMission_PasstheParcel);

class scrFmEventAmbMission_HotProperty : public scrFmEventAmbientMission
{
public:
	scrValue m_numTimesBriefcaseExchangeHands; //Number of times briefcase exchanged hands
	scrValue m_timeBriefecaseHeld;             //Time briefcase was held for
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrFmEventAmbMission_HotProperty);

class scrFmEventAmbMission_DeadDrop : public scrFmEventAmbientMission
{
public:
	scrValue m_numTimesBagExchangeHands; //Number of times bag exchanged hands
	scrValue m_timeBagHeld;              //Time bag was held for
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrFmEventAmbMission_DeadDrop);

class scrFmEventAmbMission_Infection : public scrFmEventAmbientMission
{
public:
	scrValue m_numPlayersInfected;         //Number of player�s infection is spread to
	scrValue m_numInfectedPlayerKills;     //Number of infected player kills
	scrValue m_numOfUninfectedPlayerkills; //Number of uninfected player kills
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrFmEventAmbMission_Infection);

class scrFmEventAmbMission_CompetitiveUrbanWarfare : public scrFmEventAmbientMission
{
public:
	scrValue m_waveReached;    //Wave reached
	scrValue m_killsPerPlayer; //Kills per player
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrFmEventAmbMission_CompetitiveUrbanWarfare);

class scrFmEventAmbMission_HuntBeast : public scrFmEventAmbientMission
{
public:
	scrValue m_landmarksCollected; //Landmarks collected (up to 10)?
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrFmEventAmbMission_HuntBeast);

class scrPIMenuHideSettings 
{
public:
	scrValue m_bitsetopt1;
	scrValue m_bitsetopt2;
	scrValue m_bitsetopt3;
	scrValue m_bitsetopt4;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrPIMenuHideSettings);

//////////////////////////////////////////////////////////////////////////

class scrBwBossWork
{
public:
	scrValue m_bossId1;					//BossId
	scrValue m_bossId2;					//BossId
	scrValue m_matchId1;				//MatchId
	scrValue m_matchId2;				//MatchId
	scrValue m_playerParticipated;      //Whether  or not player participated 
	scrValue m_timeStart;				//Time work started
	scrValue m_timeEnd;					//Time work ended
	scrValue m_won;						//won/lost
	scrValue m_endingReason;            //Reason for script ending - timer ran out, left session, etc
	scrValue m_cashEarned;				//Cash Earned.
	scrValue m_rpEarned;				//RP Earned.
	scrValue m_bossKilled;				//Boss's killed
	scrValue m_goonsKilled;             //Goons killed
	scrValue m_deaths;					//Deaths
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrBwBossWork);

class scrBwBossOnBossDeathMatch : public scrBwBossWork
{
public:
	scrValue m_launchedByBoss;          //Whether  or not work was launched by this Boss
	scrValue m_invitesSent;             //Invites sent - to OTHER bosses
	scrValue m_invitesAccepted;			//Invites accepted by other bosses
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrBwBossOnBossDeathMatch);

class scrBwYatchAttack : public scrBwBossWork
{
public:
	scrValue m_launchedByBoss;          //Whether  or not work was launched by this Boss
	scrValue m_totalCapturingScore;     //Total Capturing score
	scrValue m_totalUnderAttackScore;     //Total Under Attack score
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrBwYatchAttack);

class scrBwHuntTheBoss : public scrBwBossWork
{
public:
	scrValue m_launchedByBoss;          //Whether  or not work was launched by this Boss
	scrValue m_variationPlayed;     //Variation played
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrBwHuntTheBoss);

class scrBwPutOutAHit : public scrBwBossWork
{
public:
	scrValue m_hitMethodSelected;   //Hit method selected
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrBwPutOutAHit);

class scrBwSightseer : public scrBwBossWork
{
public:
	scrValue m_launchedByBoss;          //Whether  or not work was launched by this Boss
	scrValue m_numberLocationToWin;     //Number of locations needed to win
	scrValue m_totalLocation;	        //Total location collected
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrBwSightseer);

class scrBwAssault : public scrBwBossWork
{
public:
	scrValue m_launchedByBoss;          //Whether  or not work was launched by this Boss
	scrValue m_locationSelected;        //Location selected
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrBwAssault);

class scrBwBellyOfTheBeast: public scrBwBossWork
{
public:
	scrValue m_launchedByBoss;          //Whether  or not work was launched by this Boss
	scrValue m_vehicleType;		        //Vehicle type
	scrValue m_pickupLocation;		    //Pickup Location selected
	scrValue m_deliveryLocation;		//Delivery Location selected
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrBwBellyOfTheBeast);

class scrBwHeadhunter: public scrBwBossWork
{
public:
    scrValue m_enemiesKilled;
    scrValue m_targetLocation;
    scrValue m_numberOfTargetsKilled;
    scrValue m_launchedByBoss;          //Whether  or not work was launched by this Boss
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrBwHeadhunter);

class scrBwFragileGoods: public scrBwBossWork
{
public:
    scrValue m_destroyDisconnectedRatio; // Trailer destroyed/ total trailer disconnects
    scrValue m_targetLocation;
    scrValue m_startLocation;
    scrValue m_rivalGangDestination;    // Whether AI chase fires
    scrValue m_launchedByBoss;          //Whether  or not work was launched by this Boss
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrBwFragileGoods);

class scrBwAirFreight: public scrBwBossWork
{
public:
    scrValue m_containerDisconnected;
    scrValue m_targetLocation;
    scrValue m_containerLocation;
    scrValue m_launchedByBoss;          //Whether  or not work was launched by this Boss
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrBwAirFreight);


class scrBcBossChallenge
{
public:
	scrValue m_bossId1;					//BossId
	scrValue m_bossId2;					//BossId
	scrValue m_matchId1;				//MatchId
	scrValue m_matchId2;				//MatchId
	scrValue m_playerParticipated;      //Whether  or not player participated 
	scrValue m_timeStart;				//Time work started
	scrValue m_timeEnd;					//Time work ended
	scrValue m_won;						//won/lost
	scrValue m_endingReason;            //Reason for script ending - timer ran out, left session, etc
	scrValue m_cashEarned;				//Cash Earned.
	scrValue m_rpEarned;				//RP Earned.
	scrValue m_bossKilled;				//Boss's killed
	scrValue m_goonsKilled;             //Goons killed
	scrValue m_wagerSet;				//Wager set
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrBcBossChallenge);

class scrBcCarJacking: public scrBcBossChallenge
{
public:
	scrValue m_totalScore;		//Total score
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrBcCarJacking);

class scrBcSmashAndGrab: public scrBcBossChallenge
{
public:
	scrValue m_deaths;			//Deaths
	scrValue m_storesHeldUp;	//number of stores held up
	scrValue m_deliveriesMade;	//number of deliveries made
	scrValue m_cashLost;		//cash lost (through deaths) ($)
	scrValue m_cashDelivered;	//cash delivered ($)
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrBcSmashAndGrab);

class scrBcProtectionRacket: public scrBcBossChallenge
{
public:
	scrValue m_deaths;				//Deaths
	scrValue m_cashBagsCollected;	//cash bags collected
	scrValue m_cashBagsDelivered;	//cash bags delivered
	scrValue m_pickupQuadrant;		//Pickup Quadrant
	scrValue m_deliveryQuadrant;	//Delivery Quadrant
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrBcProtectionRacket);

class scrBcMostWanted: public scrBcBossChallenge
{
public:
	scrValue m_deaths;			//Deaths
	scrValue m_timeLasted;		//Time lasted with wanted level
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrBcMostWanted);

class scrBcFindersKeepers: public scrBcBossChallenge
{
public:
	scrValue m_deaths;				//Deaths
	scrValue m_packagesCollected;	//number of packages collected
	scrValue m_quadrantSelected;	//Quadrant selected
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrBcFindersKeepers);

class scrBcRunningRiot: public scrBcBossChallenge
{
public:
	scrValue m_deaths;			//Deaths
	scrValue m_numberOfKills;	//number of kills
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrBcRunningRiot);

class scrBcPointToPoint: public scrBcBossChallenge
{
public:
	scrValue m_deaths;			//Deaths
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrBcPointToPoint);

class scrBcCashing: public scrBcBossChallenge
{
public:
    scrValue m_machinesHacked;
    scrValue m_failedMinigamesRatio;
    scrValue m_wantedLevelReached;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrBcCashing);

class scrBcSalvage: public scrBcBossChallenge
{
public:
    scrValue m_checkpointsCollected;
    scrValue m_rebreathersCollected;
    scrValue m_deaths;			//Deaths
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrBcSalvage);


class scrContrabandMission
{
public:
    scrValue m_bossId1;					//BossId
    scrValue m_bossId2;					//BossId
    scrValue m_matchId1;				//MatchId
    scrValue m_matchId2;				//MatchId
    scrValue m_playerParticipated;      //Whether  or not player participated 
    scrValue m_timeStart;				//Time work started
    scrValue m_timeEnd;					//Time work ended
    scrValue m_won;						//won/lost
    scrValue m_endingReason;            //Reason for script ending - timer ran out, left session, etc
    scrValue m_cashEarned;				//Cash Earned.
    scrValue m_rpEarned;				//RP Earned.
    scrValue m_location;				//Location selected
    scrValue m_type;                    //Type of Mission Launched (small medium, large)
    scrValue m_subtype;                 //Subtype of Mission Launched 
    scrValue m_warehouseOwned;			//What Warehouses the Boss owns (bit set)
    scrValue m_warehouseOwnedCount;		//number of Warehouses the Boss owns
    scrValue m_failureReason;			//Failure reason
	scrValue m_missionId;				//MissionId
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrContrabandMission);

class scrBuyContrabandMission: public scrContrabandMission
{
public:
    scrValue m_startVolume;			    //Contraband volume at start of mission
    scrValue m_endVolume;			    //Contraband volume at end of mission
    scrValue m_startWarehouseCapacity;	//Warehouse capacity % at start of mission
    scrValue m_endWarehouseCapacity;	//Warehouse capacity % at end of mission
    scrValue m_contrabandDestroyed;     //Amount of contraband destroyed
	scrValue m_contrabandDelivered;     //Amount of contraband delivered
	scrValue m_FromHackerTruck;			//whether the mission was launched from the Hacker Truck
	scrValue m_properties;
	scrValue m_properties2;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrBuyContrabandMission);

class scrSellContrabandMission: public scrBuyContrabandMission
{
public:
    scrValue m_dropOffLocation;		    //Drop off location
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrSellContrabandMission);

class scrDefendContrabandMission: public scrBuyContrabandMission
{
public:
    scrValue m_enemiesKilled;		    //Enemies killed
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrDefendContrabandMission);

class scrRecoverContrabandMission: public scrDefendContrabandMission
{
public:
    scrValue m_lostDestroyedOrRecovered;    //Whether contraband was lost, destroyed or recovered
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrRecoverContrabandMission);

class DupeDetection
{
public:
	scrValue  m_locationBlocked;   // Where they were blocked (vehicle entry, mod shop entry, selling option)
	scrValue  m_reasonBlocked;     // Why they were blocked (which of the 3 lists. previous 5 veh sold, last 5 plates to be changed, plates of last 10 ‘known dupes’)
	scrValue  m_vehicleModel;      // Vehicle model
	scrValue  m_dupesSold;         // If a limited number of allowed dupes had been sold, and if so, how many.
	scrValue  m_blockedVehicles;   // Number of vehicles owned by player which match blocked vehicle’s license plate
	scrValue  m_ownedVehicles;     // Total number of owned vehicles
	scrValue  m_garageSpace;       // Total number of personal garage spaces
	scrValue  m_exploitLevel;      // Exploit Level of player
	scrValue  m_levelIncrease;     // Did we apply an exploit level increase? If so, how much
	scrValue  m_iFruit;            // Does the player have the iFruit app?
	scrValue  m_spareSlot1;        // spare slot
	scrValue  m_spareSlot2;        // spare slot
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(DupeDetection);

class scrArenaWarsEnd
{
public:
	scrValue m_sessionid;
	scrValue m_publiccontentid;
	scrValue m_cashearned;
	scrValue m_launchmethod;
	scrValue m_leftinprogress;
	scrValue m_xp;
	scrValue m_kills;
	scrValue m_deaths;
	scrValue m_suicides;
	scrValue m_rank;
	scrValue m_vehicleid;
	scrValue m_matchduration;
	scrValue m_playingtime;
	scrValue m_result;
	scrValue m_premiumEvent;
	scrValue m_skillLevel;
	scrValue m_endreason;
	scrValue m_killer;
	scrValue m_killervehicle;
	scrValue m_matchtype;
	scrValue m_matchname;
	scrValue m_checkpointsCollected;
	scrValue m_bonusTime;
	scrValue m_finishPosition;
	scrValue m_teamType;
	scrValue m_teamId;
	scrValue m_personalVehicle;
	scrValue m_flagsStolen;
	scrValue m_flagsDelivered;
	scrValue m_totalPoints;
	scrValue m_goalsScored;
	scrValue m_suddenDeath;
	scrValue m_winConditions;
	scrValue m_damageDealt;
	scrValue m_damageReceived;
	scrValue m_gladiatorKills;
	scrValue m_bombTime;
	scrValue m_spectatorTime;
	scrValue m_totalTagIns;
	scrValue m_timeEliminated;
	scrValue m_lobbyModLivery;
	scrValue m_lobbyModArmor;
	scrValue m_lobbyModWeapon;
	scrValue m_lobbyModMine;
	scrValue m_controlTurret;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrArenaWarsEnd);

class scrArcadeLoveMatch
{
public:
	scrValue matchId1;
	scrValue matchId2;
	scrValue score;
	scrValue flying;
	scrValue stamina;
	scrValue shooting;
	scrValue driving;
	scrValue stealth;
	scrValue maxHealth;
	scrValue trueLove;
	scrValue nemesis;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrArcadeLoveMatch);

// Matching STRUCT_CNC_UNLOCK
class CncUnlockInfo
{
public:
	scrValue m_item;
	scrValue m_itemType;
	scrValue m_itemRank;
	scrValue m_itemRole;
	scrValue m_progRank;
	scrValue m_tknSpend;
	scrValue m_tknBalance;
	scrValue m_moneySpend;
	scrValue m_moneyBalance;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(CncUnlockInfo);

// Matching STRUCT_HEIST4_PREP
class Heist4PrepInfo
{
public:
	scrValue m_playthroughId;
	scrValue m_missionName;
	scrValue m_sessionId;
	scrValue m_hashmac;
	scrValue m_playCount;
	scrValue m_type;
	scrValue m_bossId1;
	scrValue m_bossId2;
	scrValue m_bosstype;
	scrValue m_launcherRank;
	scrValue m_playerRole;
	scrValue m_playerParticipated;
	scrValue m_difficult;
	scrValue m_timeTakenToComplete;
	scrValue m_endingReason;
	scrValue m_isRivalParty;
	scrValue m_cashEarned;
	scrValue m_rpEarned;
	scrValue m_location;
	scrValue m_invitesSent;
	scrValue m_invitesAccepted;
	scrValue m_deaths;
	scrValue m_targetsKilled;
	scrValue m_innocentsKilled;
	scrValue m_mandatory;
	scrValue m_primTar;
	scrValue m_properties;
	scrValue m_entrances;
	scrValue m_approaches;
	scrValue m_grappleLoc;
	scrValue m_uniLoc;
	scrValue m_cutterLoc;
	scrValue m_secLootLoc;
	scrValue m_miscActions;
	scrValue m_properties2;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(Heist4PrepInfo);

// Matching STRUCT_HEIST4_FINALE
class Heist4FinaleInfo
{
public:
	scrValue m_playthroughId;
	scrValue m_missionId;
	scrValue m_sessionId;
	scrTextLabel31 m_publicContentId;
	scrValue m_hashmac;
	scrValue m_bossId1;
	scrValue m_bossId2;
	scrValue m_bosstype;
	scrValue m_launcherRank;
	scrValue m_playerRole;
	scrValue m_endingReason;
	scrValue m_replay;
	scrValue m_rpEarned;
	scrValue m_difficult;
	scrValue m_timeTakenToComplete;
	scrValue m_checkpoint;
	scrValue m_playCount;
	scrValue m_deaths;
	scrValue m_targetsKilled;
	scrValue m_innocentsKilled;
	scrValue m_properties;
	scrValue m_properties2;
	scrValue m_percentage;
	scrValue m_tod;
	scrValue m_wLoadout;
	scrValue m_outfits;
	scrValue m_suppressors;
	scrValue m_supCrewBoard;
	scrValue m_islApproach;
	scrValue m_islEscBoard;
	scrValue m_primTar;
	scrValue m_weather;
	scrValue m_weapDisrup;
	scrValue m_supCrewActual;
	scrValue m_miscActions;
	scrValue m_compEntrance;
	scrValue m_primTarEntrance;
	scrValue m_compExit;
	scrValue m_interestItemUsed;
	scrValue m_islEscActual;
	scrValue m_failedStealth;
	scrValue m_specCam;
	scrValue m_moneyEarned;
	scrValue m_secLoot;
	scrValue m_secTarCash;
	scrValue m_secTarCocaine;
	scrValue m_secTarWeed;
	scrValue m_secTarGold;
	scrValue m_secTarPaint;
	scrValue m_bagCapacity;
	scrValue m_CashBlocksStolen;
	scrValue m_CashValueFinal;
	scrValue m_CashBlocksFinal;
	scrValue m_CashBlocksSpace;
	scrValue m_CocaBlocksStolen;
	scrValue m_CocaValueFinal;
	scrValue m_CocaBlocksFinal;
	scrValue m_CocaBlocksSpace;
	scrValue m_WeedBlocksStolen;
	scrValue m_WeedValueFinal;
	scrValue m_WeedBlocksFinal;
	scrValue m_WeedBlocksSpace;
	scrValue m_GoldBlocksStolen;
	scrValue m_GoldValueFinal;
	scrValue m_GoldBlocksFinal;
	scrValue m_GoldBlocksSpace;
	scrValue m_PaintBlocksStolen;
	scrValue m_PaintValueFinal;
	scrValue m_PaintBlocksFinal;
	scrValue m_PaintBlocksSpace;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(Heist4FinaleInfo);

// Matching STRUCT_HUB_ENTRY
class HubEntryInfo
{
public:
	scrValue m_type;
	scrValue m_properties;
	scrValue m_access;
	scrValue m_cost;
	scrValue m_bossId1;
	scrValue m_bossId2;
	scrValue m_playerRole;
	scrValue m_pulled;
	scrValue m_invited;
	scrValue m_properties2;
	scrValue m_vehicle;
	scrValue m_crewId;
	scrValue m_private;
	scrValue m_vehicleType;
	scrValue m_hubId;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(HubEntryInfo);

// Matching STRUCT_DJ_MISSION_ENDED
class DjMissionEndedInfo
{
public:
	scrValue m_missionTypeId;
	scrValue m_missionId;
	scrValue m_matchId;
	scrValue m_bossId1;
	scrValue m_bossId2;
	scrValue m_bosstype;
	scrValue m_playerRole;
	scrValue m_launcherRank;
	scrValue m_location;
	scrValue m_playerParticipated;
	scrValue m_won;
	scrValue m_endingReason;
	scrValue m_timeTakenToComplete;
	scrValue m_cashEarned;
	scrValue m_rpEarned;
	scrValue m_chipsEarned;
	scrValue m_itemEarned;
	scrValue m_targetsKilled;
	scrValue m_innocentsKilled;
	scrValue m_deaths;
	scrValue m_bottlesDelivered;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(DjMissionEndedInfo);

class RaceToPointInfo
{
public:
	scrValue m_LocationHash;
	scrValue m_MatchId;
	scrValue m_NumPlayers;
	scrValue m_RaceWon;
	scrVector m_RaceStartPos;
	scrVector m_RaceEndPos;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(RaceToPointInfo);

// Matching STRUCT_ROBBERY_PREP
class RobberyPrepInfo
{
public:
	scrValue m_playthroughID;
	scrValue m_missionName;
	scrValue m_sessionID;
	scrValue m_strandID;
	scrValue m_type;
	scrValue m_bossId1;
	scrValue m_bossId2;
	scrValue m_bosstype;
	scrValue m_launcherRank;
	scrValue m_playerRole;
	scrValue m_playerParticipated;
	scrValue m_timeTakenToComplete;
	scrValue m_timeLimit;
	scrValue m_endingReason;
	scrValue m_isRivalParty;
	scrValue m_cashEarned;
	scrValue m_rpEarned;
	scrValue m_location;
	scrValue m_invitesSent;
	scrValue m_invitesAccepted;
	scrValue m_deaths;
	scrValue m_targetsKilled;
	scrValue m_innocentsKilled;
	scrValue m_properties;
	scrValue m_properties2;
	scrValue m_failedStealth;
	scrValue m_deliveryLocation;
	scrValue m_hashmac;
	scrValue m_vehicleStolen;
	scrValue m_vehicleCount;
	scrValue m_outfits;
	scrValue m_entrance;
	scrValue m_missionCategory;
	scrValue m_won;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(RobberyPrepInfo);

// Matching STRUCT_ROBBERY_FINALE
class RobberyFinaleInfo
{
public:
	scrValue m_playthroughID;
	scrValue m_missionID;
	scrValue m_sessionID;
	scrValue m_strandID;
	scrTextLabel31 m_publicContentID;
	scrValue m_bossId1;
	scrValue m_bossId2;
	scrValue m_bosstype;
	scrValue m_launcherRank;
	scrValue m_playerRole;
	scrValue m_endingReason;
	scrValue m_replay;
	scrValue m_rpEarned;
	scrValue m_timeTakenToComplete;
	scrValue m_checkpoint;
	scrValue m_deaths;
	scrValue m_targetsKilled;
	scrValue m_innocentsKilled;
	scrValue m_properties;
	scrValue m_properties2;
	scrValue m_failedStealth;
	scrValue m_wLoadout;
	scrValue m_outfits;
	scrValue m_moneyEarned;
	scrValue m_vehicleBoard;
	scrValue m_hashmac;
	scrValue m_missionCategory;
	scrValue m_won;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(RobberyFinaleInfo);

// Matching STRUCT_EXTRA_EVENT
class ExtraEventInfo
{
public:
	scrValue m_missionName;
	scrValue m_pursuer;
	scrValue m_sessionID;
	scrValue m_endingReason;
	scrValue m_onFoot;
	scrValue m_time;
	scrValue m_timeLimit;
	scrValue m_pursuerHealth;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(ExtraEventInfo);

// Matching STRUCT_CARCLUB_POINTS
class CarclubPointsInfo
{
public:
	scrValue m_bossId1;
	scrValue m_bossId2;
	scrValue m_match_id;
	scrValue m_method;
	scrValue m_amount;
	scrValue m_itemBought;
	scrValue m_streak;
	scrValue m_memberLevel;
	scrValue m_levelUp;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(CarclubPointsInfo);

class sFreemodeMissionEnd
{
public:
        scrValue m_groupID;              // group ID; this would enable us to track those players who play this mission together
        scrValue m_missionCategory;      // which set of missions these are (security contracts, payphone hits etc.)
        scrValue m_missionName;          // mission name
        scrValue m_sessionID;            // session ID
        scrValue m_type;                 // type of mission, for example, normal or VIP security contracts
        scrValue m_location;             // mission variation
        scrValue m_launchMethod;         // launch method
        scrValue m_bossId1;              // unique ID of the Boss instance
        scrValue m_bossId2;
        scrValue m_bosstype;             // type of org: Securoserv, MC, unaffiliated
        scrValue m_launcherRank;         // rank of the player launching
        scrValue m_playerRole;           // similar to previous metrics, the player's role: CEO, bodyguard, biker prospect etc.
        scrValue m_playerParticipated;   // whether the player participated
        scrValue m_timeTakenToComplete;  // time taken to complete, in seconds, for consistency
        scrValue m_timeLimit;            // time limit, in seconds (for consistency); null if not applicable
        scrValue m_won;                  // whether the mission was completed successfully
        scrValue m_endingReason;         // similar to previous metrics: won, lost, boss_left etc.
        scrValue m_isRivalParty;         // is the party completing the mission the same as the party that launched it; null if not applicable
        scrValue m_cashEarned;           // money earned
        scrValue m_rpEarned;             // RP earned
        scrValue m_deaths;               // how many times the player died
        scrValue m_targetsKilled;        // number of mission critical/aggressive peds killed
        scrValue m_innocentsKilled;      // number of innocent peds killed
        scrValue m_properties;           // existing bitset of type of properties owned: casino penthouse, gunrunning bunker, smuggler hangar, terrorbyte, submarine etc.
        scrValue m_properties2;          // bitset of type of properties owned; future proofing
        scrValue m_failedStealth;        // whether the player failed stealth and alerted the enemies
        scrValue m_difficulty;           // mission difficulty: easy, normal, hard, hardest
        scrValue m_bonusKillMethod;      // bonus kill method needed on this mission (payphone hits): taxi, golf outfit etc.; null if not applicable
        scrValue m_bonusKillComplete;    // whether the player completed the bonus kill for this mission; null if not applicable
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(sFreemodeMissionEnd);

class sInstMissionEndInfo
{
public:
	scrValue m_groupID;
	scrValue m_missionCategory;
	scrValue m_missionName;
	scrValue m_sessionID;
	scrValue m_type;
	scrValue m_location;
	scrValue m_launchMethod;
	scrTextLabel31 m_publicContentID;
	scrValue m_bossId1;
	scrValue m_bossId2;
	scrValue m_bosstype;
	scrValue m_launcherRank;
	scrValue m_playerRole;
	scrValue m_timeTakenToComplete;
	scrValue m_timeLimit;
	scrValue m_won;
	scrValue m_endingReason;
	scrValue m_cashEarned;
	scrValue m_rpEarned;
	scrValue m_deaths;
	scrValue m_targetsKilled;
	scrValue m_innocentsKilled;
	scrValue m_properties;
	scrValue m_properties2;
	scrValue m_failedStealth;
	scrValue m_difficulty;
	scrValue m_playerCount;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(sInstMissionEndInfo);

class sHubExitInfo
{
public:
	scrValue m_hubID;
	scrValue m_type;
	scrValue m_bossId1;
	scrValue m_bossId2;
	scrValue m_playerRole;
	scrValue m_pulled;
	scrValue m_crewId;
	scrValue m_duration;
	scrValue m_dre;
	scrValue m_playerCount;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(sHubExitInfo);

///////////////////////////////////////////////////////////////////////////////

// Matching STRUCT_LP_NAV
class LpNavInfo
{
public:
	scrValue m_tab;
	scrValue m_hoverTile;
	scrValue m_leftBy;
	scrValue m_clickedTile;
	scrValue m_exitLp;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(LpNavInfo);

///////////////////////////////////////////////////////////////////////////////

// Matching STRUCT_INVENTORY
class InventoryInfo
{
public:
	scrValue m_action;
	scrValue m_reason;
	scrValue m_crewId;
	scrValue m_location;
	scrValue m_shopType;
	scrValue m_itemCategory;
	scrValue m_itemHash;
	scrValue m_itemDelta;
	scrValue m_purchaseID;
	scrValue m_takeAll;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(InventoryInfo);

// Matching STRUCT_MAP_NAV
class MapNavInfo
{
public:
	scrValue m_TUBlips;
	scrValue m_cloudMissBlips;
	scrValue m_seriesBlips;
	scrValue m_collectBlips;
	scrValue m_rank;
	scrValue m_leftBy;
	scrValue m_waypoint;
};
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(MapNavInfo);

///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////

//Pointer to the current leaderboard being setup for statistics submissions.
static fwRegdRef< class netLeaderboardWrite > s_LeaderboardWrite;

//Pointer to the current leaderboard being setup for statistics reads.
static fwRegdRef< class netLeaderboardRead > s_LeaderboardRead;

//Used for packed stats
#define NUM_PACKED_BOOL_PER_STAT 64
#define NUM_PACKED_INT_PER_STAT  8

static u64 s_cashStart = 0;

namespace stats_commands
{
#if __BANK
	bool  CheckForExtraStatSpew(CStatsDataMgr::StatsMap::Iterator& statIter)
	{
		static const rlProfileStatsPriority IMPORTANT_FLUSH_PRIORITY = 1;

		bool result = false;

		const sStatData* stat = 0;

		if (statIter.GetKey().IsValid())
		{
			stat = *statIter;
		}

		if (stat)
		{
			if (stat->DebugStatChanges() || CStatsMgr::GetStatsDataMgr().Bank_ScriptSpew() || stat->GetDesc().GetFlushPriority() >= IMPORTANT_FLUSH_PRIORITY) 
			{
				result = true;
				scriptDebugf1("%s : Changing stat %s value. " ,CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKey().GetName());
				NOTFINAL_ONLY( stat->ToString(); )
				scrThread::PrePrintStackTrace();
			}
		}

		return result;
	}
#endif // __BANK

	void CommandStatAssert(const bool ASSERT_ONLY(ret), const char* ASSERT_ONLY(command), const int ASSERT_ONLY(keyHash), const char* ASSERT_ONLY(keyName))
	{
		if (!CTheScripts::GetPlayerIsInAnimalForm())//Exception for Animal form...
		{
			scriptAssertf(ret, "%s : %s - Failed for Stat with key=\"%d, %s\".", CTheScripts::GetCurrentScriptNameAndProgramCounter(), command, keyHash, keyName);
		}
	}

	int CommandStatHashId(const char* pkey)
	{
		scriptAssertf(pkey, "%s : STAT_HASH_ID - Null Key", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if(!pkey)
			return 0;

		const StatId keyHash = STAT_ID(pkey);
		if(!StatsInterface::IsKeyValid(keyHash))
		{
			scriptAssertf(0, "%s : STAT_HASH_ID - Stat with key=\"%s\" does not exist.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pkey);
			return 0;
		}

		return keyHash.GetHash();
	}

	const char* CommandStatGetName(const int BANK_ONLY(keyHash))
	{
#if __BANK
		if(!StatsInterface::IsKeyValid(keyHash))
		{
			scriptAssertf(0, "%s : STAT_GET_NAME - Stat with key=\"%d\" does not exist.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), keyHash);
			return "UNKNOWN_STAT_ID";
		}

		return StatsInterface::GetKeyName(keyHash);
#else 
		scriptAssertf(0, "%s : STAT_GET_NAME - Command is dev only.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return "STAT_UNKNOWN";
#endif // __BANK
	}

	bool CommandStatIsKeyValid(const int keyHash)
	{
		return StatsInterface::IsKeyValid(keyHash);
	}

	bool CommandStatSetInt (const int keyHash, const int data, bool ASSERT_ONLY(coderAssert))
	{
		bool ret = false;

		if (STAT_CASINO_CHIPS.GetHash() == (u32)keyHash)
		{
			scriptErrorf("%s:STAT_SET_INT - Stat %s cant be changed by script", CTheScripts::GetCurrentScriptNameAndProgramCounter(), atHashString(keyHash).TryGetCStr());
			return ret;
		}

		if (!CStatsUtils::IsStatsTrackingEnabled())
		{
			scriptDebugf1("%s : Stats are disabled", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			return ret;
		}

		CStatsDataMgr& StatsDataMgr = CStatsMgr::GetStatsDataMgr();

		const sStatData* stat = 0;

		CStatsDataMgr::StatsMap::Iterator statIter;
		if (StatsDataMgr.StatIterFind(keyHash, statIter))
		{
			if (statIter.GetKey().IsValid())
			{
				stat = *statIter;
			}
		}

		scriptAssertf(stat, "%s : STAT_SET_INT - Stat with key=\"%d\" does not exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), keyHash);
		if (stat)
		{
			if (stat->GetDesc().GetIsOnlineData())
			{
				scriptAssertf(!StatsInterface::CloudFileLoadPending(0), "%s : STAT_SET_INT - Trying to change Stat with key=\"%s\" but stats are not loaded.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());
				scriptAssertf(!StatsInterface::CloudFileLoadFailed(0),  "%s : STAT_SET_INT - Trying to change Stat with key=\"%s\" but stats loaded have failed.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());
				if(StatsInterface::CloudFileLoadPending(0) || StatsInterface::CloudFileLoadFailed(0))
				{
					return ret;
				}
			}

			if (PARAM_spewStatsUsage.Get())
			{
				scriptDebugf1("STAT_SET_INT - stat %s, value=%d", statIter.GetKeyName(), data);
			}

			if (STAT_CHAR_MISSION_PASSED.GetHash() == (u32)keyHash)
			{
				CStatsMgr::PlayerCompleteMission();
			}
			else if (STAT_CHAR_MISSION_FAILED.GetHash() == (u32)keyHash)
			{
				CStatsMgr::PlayerFailedMission();
			}
			else if (STAT_RP_GIFT.GetHash() == (u32)keyHash)
			{
				scriptAssertf(0 == data, "%s : STAT_SET_INT - Trying to change Stat with key=\"%s\" but the value='%d' is not 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName(), data);
				if (0 == data)
					StatsInterface::IncrementStat(STAT_RP_GIFT_RECEIVED, (float)StatsInterface::GetIntStat(STAT_RP_GIFT));
			}

#if __BANK
			if (CheckForExtraStatSpew(statIter)) scriptDebugf1("Stat \"%s\" NEW value=\"%d\"", statIter.GetKeyName(), data);
#endif // __BANK

			scriptAssertf(!stat->GetIsCodeStat() || !coderAssert, "%s : STAT_SET_INT - Stat \"%s\" is updated in code.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());

			u32 updateflags = STATUPDATEFLAG_ASSERT_SCRIPT_UPDATE;
			if (stat->GetDesc().GetFlushPriority() > 0)
			{
				updateflags = STATUPDATEFLAG_DEFAULT_SCRIPT_UPDATE;
			}

			switch (stat->GetType())
			{
			case STAT_TYPE_INT:
				{
					ret = StatsDataMgr.SetStatIterData(statIter, (void*)&data, stat->GetSizeOfData(), updateflags);
				}
				break;

			case STAT_TYPE_INT64:
				{
					s64 tempdata = static_cast<s64>(data);
					ret = StatsDataMgr.SetStatIterData(statIter, &tempdata, stat->GetSizeOfData(), updateflags);
				}
				break;

			case STAT_TYPE_UINT8:
				{
					u8 tempdata = static_cast< u8 >(data);
					ret = StatsDataMgr.SetStatIterData(statIter, &tempdata, stat->GetSizeOfData(), updateflags);
				}
				break;

			case STAT_TYPE_UINT16:
				{
					u16 tempdata = static_cast< u16 >(data);
					ret = StatsDataMgr.SetStatIterData(statIter, &tempdata, stat->GetSizeOfData(), updateflags);
				}
				break;

			case STAT_TYPE_UINT32:
				{
					u32 tempdata = static_cast< u32 >(data);
					ret = StatsDataMgr.SetStatIterData(statIter, &tempdata, stat->GetSizeOfData(), updateflags);
				}
				break;

			case STAT_TYPE_UINT64:
				{
					u64 tempdata = static_cast< u64 >(data);
					ret = StatsDataMgr.SetStatIterData(statIter, &tempdata, stat->GetSizeOfData(), updateflags);
				}
				break;

			default:
				scriptAssertf(0, "%s : STAT_SET_INT - Invalid stat=\"%s\" - type=\"%s\"", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName(), stat->GetTypeLabel());
				break;
			}

			//We need to update the current Stats model prefix.
			if (ret && STAT_MPPLY_LAST_MP_CHAR == statIter.GetKey())
			{
				scriptAssertf(!StatsInterface::CloudFileLoadPending(0) && !StatsInterface::CloudFileLoadFailed(0), "%s - Assuming a MULTIPLAYER MODEL - but stats are not loaded.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
				if(!StatsInterface::CloudFileLoadPending(0) && !StatsInterface::CloudFileLoadFailed(0))
				{
					StatsInterface::SetStatsModelPrefix( FindPlayerPed() );
				}
			}

			CommandStatAssert(ret, "STAT_SET_INT", keyHash, statIter.GetKeyName());
		}

		return ret;
	}

	bool CommandStatSetFloat (const int keyHash, const float data, bool ASSERT_ONLY(coderAssert))
	{
		bool ret = false;

		if (STAT_CASINO_CHIPS.GetHash() == (u32)keyHash)
		{
			scriptErrorf("%s:STAT_SET_INT - Stat %s cant be changed by script", CTheScripts::GetCurrentScriptNameAndProgramCounter(), atHashString(keyHash).TryGetCStr());
			return ret;
		}

		if (!CStatsUtils::IsStatsTrackingEnabled())
		{
			scriptDebugf1("%s : Stats are disabled", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			return ret;
		}

		CStatsDataMgr& StatsDataMgr = CStatsMgr::GetStatsDataMgr();

		const sStatData* stat = 0;

		CStatsDataMgr::StatsMap::Iterator statIter;
		if (StatsDataMgr.StatIterFind(keyHash, statIter))
		{
			if (statIter.GetKey().IsValid())
			{
				stat = *statIter;
			}
		}

		scriptAssertf(stat, "%s : STAT_SET_FLOAT - Stat with key=\"%d\" does not exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), keyHash);
		if (stat)
		{
			if (stat->GetDesc().GetIsOnlineData())
			{
				scriptAssertf(!StatsInterface::CloudFileLoadPending(0), "%s : STAT_SET_FLOAT - Trying to change Stat with key=\"%s\" but stats are not loaded.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());
				scriptAssertf(!StatsInterface::CloudFileLoadFailed(0),  "%s : STAT_SET_FLOAT - Trying to change Stat with key=\"%s\" but stats loaded have failed.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());
				if(StatsInterface::CloudFileLoadPending(0) || StatsInterface::CloudFileLoadFailed(0))
				{
					return ret;
				}
			}

			if (PARAM_spewStatsUsage.Get()) scriptDebugf1("STAT_SET_FLOAT - stat %s, value=%f", statIter.GetKeyName(), data);

			scriptAssertf(!stat->GetIsCodeStat() || !coderAssert, "%s : STAT_SET_FLOAT - Stat \"%s\" is updated in code.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());

			ret = stat->GetIsBaseType(STAT_TYPE_FLOAT);
			scriptAssertf(ret, "%s : STAT_SET_FLOAT - Invalid stat=\"%s\" - type=\"%s\"", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName(), stat->GetTypeLabel());

			if (ret)
			{
#if __BANK
				if (CheckForExtraStatSpew(statIter)) scriptDebugf1("Stat \"%s\" NEW value=\"%f\"", statIter.GetKeyName(), data);
#endif // __BANK

				u32 updateflags = STATUPDATEFLAG_ASSERT_SCRIPT_UPDATE;
				if (stat->GetDesc().GetFlushPriority() > 0)
				{
					updateflags = STATUPDATEFLAG_DEFAULT_SCRIPT_UPDATE;
				}

				ret = StatsDataMgr.SetStatIterData(statIter, (void*)&data, stat->GetSizeOfData(), updateflags);
			}

			CommandStatAssert(ret, "STAT_SET_FLOAT", keyHash, statIter.GetKeyName());
		}

		return ret;
	}

	bool CommandStatSetBool  (const int keyHash, const bool data, bool ASSERT_ONLY(coderAssert))
	{
		bool ret = false;

		if (!CStatsUtils::IsStatsTrackingEnabled())
		{
			scriptDebugf1("%s : Stats are disabled", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			return ret;
		}

		CStatsDataMgr& StatsDataMgr = CStatsMgr::GetStatsDataMgr();

		const sStatData* stat = 0;

		CStatsDataMgr::StatsMap::Iterator statIter;
		if (StatsDataMgr.StatIterFind(keyHash, statIter))
		{
			if (statIter.GetKey().IsValid())
			{
				stat = *statIter;
			}
		}

		scriptAssertf(stat, "%s : STAT_SET_BOOL - Stat with key=\"%d\" does not exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), keyHash);
		if(stat)
		{
			if (stat->GetDesc().GetIsOnlineData())
			{
				scriptAssertf(!StatsInterface::CloudFileLoadPending(0), "%s : STAT_SET_BOOL - Trying to change Stat with key=\"%s\" but stats are not loaded.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());
				scriptAssertf(!StatsInterface::CloudFileLoadFailed(0),  "%s : STAT_SET_BOOL - Trying to change Stat with key=\"%s\" but stats loaded have failed.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());
				if(StatsInterface::CloudFileLoadPending(0) || StatsInterface::CloudFileLoadFailed(0))
				{
					return ret;
				}
			}

			if (PARAM_spewStatsUsage.Get()) scriptDebugf1("STAT_SET_BOOL - stat %s, value=%s", statIter.GetKeyName(), data?"true":"false");

			scriptAssertf(!stat->GetIsCodeStat() || !coderAssert, "%s : STAT_SET_BOOL - Stat \"%s\" is updated in code.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());
			ret = stat->GetIsBaseType(STAT_TYPE_BOOLEAN);
			scriptAssertf(ret, "%s : STAT_SET_BOOL - Invalid stat=\"%s\" - type=\"%s\"", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName(), stat->GetTypeLabel());
			
			if (ret)
			{
#if __BANK
				if (CheckForExtraStatSpew(statIter)) scriptDebugf1("Stat \"%s\" NEW value=\"%s\"", statIter.GetKeyName(), data ? "True" : "False");
#endif // __BANK

				u32 updateflags = STATUPDATEFLAG_ASSERT_SCRIPT_UPDATE;
				if (stat->GetDesc().GetFlushPriority() > 0)
				{
					updateflags = STATUPDATEFLAG_DEFAULT_SCRIPT_UPDATE;
				}

				ret = StatsDataMgr.SetStatIterData(statIter, (void*)&data, stat->GetSizeOfData(), updateflags);

				CommandStatAssert(ret, "STAT_SET_BOOL", keyHash, statIter.GetKeyName());
			}
		}

		return ret;
	}

	bool CommandStatSetGxtValue(const int keyHash, const char* data, bool ASSERT_ONLY(coderAssert))
	{
		bool ret = false;

		if (!CStatsUtils::IsStatsTrackingEnabled())
		{
			scriptDebugf1("%s : Stats are disabled", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			return ret;
		}

		if(!SCRIPT_VERIFY(data, "STAT_SET_GXT_LABEL - Label is empty"))
			return ret;

		if(!TheText.DoesTextLabelExist(data))
		{
			scriptAssertf(0, "%s : STAT_SET_GXT_LABEL - Label \"%s\" for stat=\"%s\" doesnt exist.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), data, StatsInterface::GetKeyName(keyHash));
			return ret;
		}

		CStatsDataMgr& StatsDataMgr = CStatsMgr::GetStatsDataMgr();

		const sStatData* stat = 0;

		CStatsDataMgr::StatsMap::Iterator statIter;
		if (StatsDataMgr.StatIterFind(keyHash, statIter))
		{
			if (statIter.GetKey().IsValid())
			{
				stat = *statIter;
			}
		}

		scriptAssertf(stat, "%s : STAT_SET_GXT_LABEL - Stat with key=\"%d\" does not exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), keyHash);
		if(stat)
		{
			if (stat->GetDesc().GetIsOnlineData())
			{
				scriptAssertf(!StatsInterface::CloudFileLoadPending(0), "%s : STAT_SET_GXT_LABEL - Trying to change Stat with key=\"%s\" but stats are not loaded.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());
				scriptAssertf(!StatsInterface::CloudFileLoadFailed(0),  "%s : STAT_SET_GXT_LABEL - Trying to change Stat with key=\"%s\" but stats loaded have failed.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());
				if(StatsInterface::CloudFileLoadPending(0) || StatsInterface::CloudFileLoadFailed(0))
				{
					return ret;
				}
			}

			if (PARAM_spewStatsUsage.Get()) scriptDebugf1("STAT_SET_GXT_LABEL - stat %s, value=%s", statIter.GetKeyName(), data);

			scriptAssertf(!stat->GetIsCodeStat() || !coderAssert, "%s : STAT_SET_GXT_LABEL - Stat \"%s\" is updated in code.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());

			ret = stat->GetIsBaseType(STAT_TYPE_TEXTLABEL);
			scriptAssertf(ret, "%s : STAT_SET_GXT_LABEL - Invalid stat=\"%s\" - type=\"%s\"", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName(), stat->GetTypeLabel());

			if (ret)
			{
#if __BANK
				if (CheckForExtraStatSpew(statIter)) scriptDebugf1("Stat \"%s\" NEW value=\"%s:%d\"", statIter.GetKeyName(), data, atStringHash(data));
#endif // __BANK

				int stringKeyHash = atStringHash(data);

				u32 updateflags = STATUPDATEFLAG_ASSERT_SCRIPT_UPDATE;
				if (stat->GetDesc().GetFlushPriority() > 0)
				{
					updateflags = STATUPDATEFLAG_DEFAULT_SCRIPT_UPDATE;
				}

				ret = StatsDataMgr.SetStatIterData(statIter, &stringKeyHash, sizeof(stringKeyHash), updateflags);
				
				CommandStatAssert(ret, "STAT_SET_GXT_LABEL", keyHash, statIter.GetKeyName());
			}
		}

		return ret;
	}

	bool CommandStatSetDate  (const int keyHash, int& data, int sizeOfData, bool ASSERT_ONLY(coderAssert))
	{
		bool ret = false;

		if (!CStatsUtils::IsStatsTrackingEnabled())
		{
			scriptDebugf1("%s : Stats are disabled", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			return ret;
		}

		CStatsDataMgr& StatsDataMgr = CStatsMgr::GetStatsDataMgr();

		const sStatData* stat = 0;

		CStatsDataMgr::StatsMap::Iterator statIter;
		if (StatsDataMgr.StatIterFind(keyHash, statIter))
		{
			if (statIter.GetKey().IsValid())
			{
				stat = *statIter;
			}
		}

		scriptAssertf(stat, "%s : STAT_SET_DATE - Stat with key=\"%d\" does not exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), keyHash);
		if(stat)
		{
			if (stat->GetDesc().GetIsOnlineData())
			{
				scriptAssertf(!StatsInterface::CloudFileLoadPending(0), "%s : STAT_SET_DATE - Trying to change Stat with key=\"%s\" but stats are not loaded.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());
				scriptAssertf(!StatsInterface::CloudFileLoadFailed(0),  "%s : STAT_SET_DATE - Trying to change Stat with key=\"%s\" but stats loaded have failed.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());
				if(StatsInterface::CloudFileLoadPending(0) || StatsInterface::CloudFileLoadFailed(0))
				{
					return ret;
				}
			}

			if (PARAM_spewStatsUsage.Get()) scriptDebugf1("STAT_SET_DATE - stat %s, value=", statIter.GetKeyName());

			scriptAssertf(!stat->GetIsCodeStat() || !coderAssert, "%s : STAT_SET_DATE - Stat \"%s\" is updated in code.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());

			ret = stat->GetIsBaseType(STAT_TYPE_DATE);
			scriptAssertf(ret, "%s : STAT_SET_DATE - Invalid stat=\"%s\" - type=\"%s\"", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName(), stat->GetTypeLabel());

			if (ret)
			{
				sizeOfData *= sizeof(scrValue); // Script returns size in script words

				const scrValue* dataNew = (const scrValue*)&data;
				int size = 0;

				int year  = dataNew[size].Int; ++size;
				int month = dataNew[size].Int; ++size;
				int day   = dataNew[size].Int; ++size;
				int hour  = dataNew[size].Int; ++size;
				int min   = dataNew[size].Int; ++size;
				int sec   = dataNew[size].Int; ++size;
				int mill  = dataNew[size].Int; ++size;

				scriptAssertf((int)sizeof(scrValue)*size == sizeOfData,   "%s : STAT_SET_DATE - Invalid struct size=\"%d\", should be \"%" SIZETFMT "d\"", CTheScripts::GetCurrentScriptNameAndProgramCounter(), sizeOfData, sizeof(scrValue)*size);

				if (scriptVerifyf(year     <= 4095, "%s : STAT_SET_DATE - Invalid year=\"%d\", max is \"%d\"", CTheScripts::GetCurrentScriptNameAndProgramCounter(), year, 4095)
					&& scriptVerifyf(month <= 15,   "%s : STAT_SET_DATE - Invalid month=\"%d\", max is \"%d\"", CTheScripts::GetCurrentScriptNameAndProgramCounter(), month, 15)
					&& scriptVerifyf(day   <= 31,   "%s : STAT_SET_DATE - Invalid day=\"%d\", max is \"%d\"", CTheScripts::GetCurrentScriptNameAndProgramCounter(), day, 31)
					&& scriptVerifyf(hour  <= 31,   "%s : STAT_SET_DATE - Invalid hour=\"%d\", max is \"%d\"", CTheScripts::GetCurrentScriptNameAndProgramCounter(), hour, 31)
					&& scriptVerifyf(min   <= 63,   "%s : STAT_SET_DATE - Invalid min=\"%d\", max is \"%d\"", CTheScripts::GetCurrentScriptNameAndProgramCounter(), min, 63)
					&& scriptVerifyf(sec   <= 63,   "%s : STAT_SET_DATE - Invalid sec=\"%d\", max is \"%d\"", CTheScripts::GetCurrentScriptNameAndProgramCounter(), sec, 63)
					&& scriptVerifyf(mill  <= 1023, "%s : STAT_SET_DATE - Invalid mill=\"%d\", max is \"%d\"", CTheScripts::GetCurrentScriptNameAndProgramCounter(), mill, 1023))
				{
#if __BANK
					if (CheckForExtraStatSpew(statIter)) scriptDebugf1("Stat \"%s\" NEW value=\"%d:%d:%d:%d:%d:%d:%d\"", statIter.GetKeyName(), year , month , day , hour , min , sec , mill);
#endif // __BANK

					u32 updateflags = STATUPDATEFLAG_ASSERT_SCRIPT_UPDATE;
					if (stat->GetDesc().GetFlushPriority() > 0)
					{
						updateflags = STATUPDATEFLAG_DEFAULT_SCRIPT_UPDATE;
					}

					u64 date = StatsInterface::PackDate(year , month , day , hour , min , sec , mill);
					ret = StatsDataMgr.SetStatIterData(statIter, &date, sizeof(u64), updateflags);

					CommandStatAssert(ret, "STAT_SET_DATE", keyHash, statIter.GetKeyName());
				}
			}
		}

		return ret;
	}

	bool CommandStatSetString(const int keyHash, const char* data, bool ASSERT_ONLY(coderAssert))
	{
		bool ret = false;

		if (!CStatsUtils::IsStatsTrackingEnabled())
		{
			scriptDebugf1("%s : Stats are disabled", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			return ret;
		}

		if(!SCRIPT_VERIFY(data, "STAT_SET_STRING - String is empty"))
			return ret;

#if __ASSERT
		static const StatId_char s_stringStatsAllowList[] =
		{
			StatId_char("CHAR_NAME"),
			StatId_char("GB_GANG_NAME"),
			StatId_char("GB_OFFICE_NAME"),
			StatId_char("YACHT_NAME"),
			StatId_char("YACHT_NAME2"),
		};
		bool isAllowed = false;
		for (const auto& stat : s_stringStatsAllowList)
		{
			if (stat.GetHash() == (u32)keyHash)
			{
				isAllowed = true;
				break;
			}
		}
		if (!isAllowed && TheText.DoesTextLabelExist(data))
		{
			scriptAssertf(0, "%s : STAT_SET_STRING - stat=\"%s\" a Label exists for this string \"%s\", consider using the Label type instead.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), StatsInterface::GetKeyName(keyHash), data);
		}
#endif

		CStatsDataMgr& StatsDataMgr = CStatsMgr::GetStatsDataMgr();

		const sStatData* stat = 0;

		CStatsDataMgr::StatsMap::Iterator statIter;
		if (StatsDataMgr.StatIterFind(keyHash, statIter))
		{
			if (statIter.GetKey().IsValid())
			{
				stat = *statIter;
			}
		}

		scriptAssertf(stat, "%s : STAT_SET_STRING - Stat with key=\"%d\" does not exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), keyHash);
		if (stat)
		{
			if (stat->GetDesc().GetIsOnlineData())
			{
				scriptAssertf(!StatsInterface::CloudFileLoadPending(0), "%s : STAT_SET_STRING - Trying to change Stat with key=\"%s\" but stats are not loaded.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());
				scriptAssertf(!StatsInterface::CloudFileLoadFailed(0),  "%s : STAT_SET_STRING - Trying to change Stat with key=\"%s\" but stats loaded have failed.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());
				if(StatsInterface::CloudFileLoadPending(0) || StatsInterface::CloudFileLoadFailed(0))
				{
					return ret;
				}
			}

			if (PARAM_spewStatsUsage.Get()) scriptDebugf1("STAT_SET_STRING - stat %s, value=%s", statIter.GetKeyName(), data);

			scriptAssertf(!stat->GetIsCodeStat() || !coderAssert, "%s : STAT_SET_STRING - Stat \"%s\" is updated in code.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());

			ret = stat->GetIsBaseType(STAT_TYPE_STRING);
			scriptAssertf(ret, "%s : STAT_SET_STRING - Invalid stat=\"%s\" - type=\"%s\"", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName(), stat->GetTypeLabel());

			if (ret)
			{
#if __BANK
				if (CheckForExtraStatSpew(statIter)) scriptDebugf1("Stat \"%s\" NEW value=\"%s\"", statIter.GetKeyName(), data);
#endif // __BANK

				u32 updateflags = STATUPDATEFLAG_ASSERT_SCRIPT_UPDATE;
				if (stat->GetDesc().GetFlushPriority() > 0)
				{
					updateflags = STATUPDATEFLAG_DEFAULT_SCRIPT_UPDATE;
				}

				char* text = const_cast<char*>(data);
				ret = StatsDataMgr.SetStatIterData(statIter, text, stat->GetSizeOfData(), updateflags);

				CommandStatAssert(ret, "STAT_SET_STRING", keyHash, statIter.GetKeyName());
			}
		}

		return ret;
	}

	bool CommandStatSetPos(const int keyHash, float X, float Y, float Z, bool ASSERT_ONLY(coderAssert))
	{
		bool ret = false;

		if (!CStatsUtils::IsStatsTrackingEnabled())
		{
			scriptDebugf1("%s : Stats are disabled", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			return ret;
		}

		CStatsDataMgr& StatsDataMgr = CStatsMgr::GetStatsDataMgr();

		const sStatData* stat = 0;

		CStatsDataMgr::StatsMap::Iterator statIter;
		if (StatsDataMgr.StatIterFind(keyHash, statIter))
		{
			if (statIter.GetKey().IsValid())
			{
				stat = *statIter;
			}
		}

		scriptAssertf(stat, "%s : STAT_SET_POS - Stat with key=\"%d\" does not exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), keyHash);
		if (stat)
		{
			if (stat->GetDesc().GetIsOnlineData())
			{
				scriptAssertf(!StatsInterface::CloudFileLoadPending(0), "%s : STAT_SET_POS - Trying to change Stat with key=\"%s\" but stats are not loaded.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());
				scriptAssertf(!StatsInterface::CloudFileLoadFailed(0),  "%s : STAT_SET_POS - Trying to change Stat with key=\"%s\" but stats loaded have failed.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());
				if(StatsInterface::CloudFileLoadPending(0) || StatsInterface::CloudFileLoadFailed(0))
				{
					return ret;
				}
			}

			if (PARAM_spewStatsUsage.Get()) scriptDebugf1("STAT_SET_POS - stat %s, value=%f,%f,%f", statIter.GetKeyName(), X, Y, Z);

			scriptAssertf(!stat->GetIsCodeStat() || !coderAssert, "%s : STAT_SET_POS - Stat \"%s\" is updated in code.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());

			ret = stat->GetIsBaseType(STAT_TYPE_POS);
			scriptAssertf(ret, "%s : STAT_SET_POS - Invalid stat=\"%s\" - type=\"%s\"", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName(), stat->GetTypeLabel());

			if (ret)
			{
#if __BANK
				if (CheckForExtraStatSpew(statIter)) scriptDebugf1("Stat \"%s\" NEW value=\"%f:%f:%f\"", statIter.GetKeyName(), X, Y, Z);
#endif // __BANK

				u32 updateflags = STATUPDATEFLAG_ASSERT_SCRIPT_UPDATE;
				if (stat->GetDesc().GetFlushPriority() > 0)
				{
					updateflags = STATUPDATEFLAG_DEFAULT_SCRIPT_UPDATE;
				}

				u64 position = StatsInterface::PackPos(X, Y, Z);
				ret = StatsDataMgr.SetStatIterData(statIter, &position, stat->GetSizeOfData(), updateflags);

				CommandStatAssert(ret, "STAT_SET_POS", keyHash, statIter.GetKeyName());
			}
		}

		return ret;
	}

	bool CommandStatSetMaskedInt(const int keyHash, int data, int shift, int numberOfBits, bool coderAssert)
	{
		u32 flags = STATUPDATEFLAG_DEFAULT_SCRIPT_UPDATE;

#if __NET_SHOP_ACTIVE
		netCatalog& catalogInst = netCatalog::GetInstance();
		const atMap < int, sPackedStatsScriptExceptions >& packedExc = catalogInst.GetScriptExceptions();

		const sPackedStatsScriptExceptions* exc = packedExc.Access(keyHash);
		if (exc && exc->Find(shift))
		{
			flags = STATUPDATEFLAG_DEFAULT;
		}
#endif // __NET_SHOP_ACTIVE

		bool ret = false;

		if (!CStatsUtils::IsStatsTrackingEnabled())
		{
			scriptDebugf1("%s : Stats are disabled", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			return ret;
		}

		CStatsDataMgr& StatsDataMgr = CStatsMgr::GetStatsDataMgr();

		const sStatData* stat = 0;

		CStatsDataMgr::StatsMap::Iterator statIter;
		if (StatsDataMgr.StatIterFind(keyHash, statIter))
		{
			if (statIter.GetKey().IsValid())
			{
				stat = *statIter;
			}
		}

		scriptAssertf(stat, "%s : STAT_SET_MASKED_INT - Stat with key=\"%d\" does not exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), keyHash);
		if (stat)
		{
			if (stat->GetDesc().GetIsOnlineData())
			{
				scriptAssertf(!StatsInterface::CloudFileLoadPending(0), "%s : STAT_SET_MASKED_INT - Trying to change Stat with key=\"%s\" but stats are not loaded.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());
				scriptAssertf(!StatsInterface::CloudFileLoadFailed(0),  "%s : STAT_SET_MASKED_INT - Trying to change Stat with key=\"%s\" but stats loaded have failed.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());
				if(StatsInterface::CloudFileLoadPending(0) || StatsInterface::CloudFileLoadFailed(0))
				{
					return ret;
				}
			}

			if (PARAM_spewStatsUsage.Get()) scriptDebugf1("STAT_SET_MASKED_INT - stat %s, value=%d", statIter.GetKeyName(), data);

#if __BANK
			if (CheckForExtraStatSpew(statIter)) scriptDebugf1("Stat \"%s\" NEW value=\"%d:%d:%d\"", statIter.GetKeyName(), data, shift, numberOfBits);
#endif // __BANK

			ret = StatsInterface::SetMaskedInt(keyHash, data, shift, numberOfBits, coderAssert, flags);

			CommandStatAssert(ret, "STAT_SET_MASKED_INT", keyHash, StatsInterface::GetKeyName(keyHash));
		}

		return ret;
	}

	bool CommandStatSetUserId(const int keyHash, const char* data, bool ASSERT_ONLY(coderAssert))
	{
		bool ret = false;

		if (!CStatsUtils::IsStatsTrackingEnabled())
		{
			scriptDebugf1("%s : Stats are disabled", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			return ret;
		}

		if(!SCRIPT_VERIFY(data, "STAT_SET_USER_ID - String is empty"))
			return ret;

		CStatsDataMgr& StatsDataMgr = CStatsMgr::GetStatsDataMgr();

		const sStatData* stat = 0;

		CStatsDataMgr::StatsMap::Iterator statIter;
		if (StatsDataMgr.StatIterFind(keyHash, statIter))
		{
			if (statIter.GetKey().IsValid())
			{
				stat = *statIter;
			}
		}

		scriptAssertf(stat, "%s : STAT_SET_USER_ID - Stat with key=\"%d\" does not exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), keyHash);
		if (stat)
		{
			if (stat->GetDesc().GetIsOnlineData())
			{
				scriptAssertf(!StatsInterface::CloudFileLoadPending(0), "%s : STAT_SET_USER_ID - Trying to change Stat with key=\"%s\" but stats are not loaded.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());
				scriptAssertf(!StatsInterface::CloudFileLoadFailed(0),  "%s : STAT_SET_USER_ID - Trying to change Stat with key=\"%s\" but stats loaded have failed.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());
				if(StatsInterface::CloudFileLoadPending(0) || StatsInterface::CloudFileLoadFailed(0))
				{
					return ret;
				}
			}

			if (PARAM_spewStatsUsage.Get()) scriptDebugf1("STAT_SET_USER_ID - stat %s, value=%s", statIter.GetKeyName(), data);

			const u32 dataSize = ustrlen(data);
			scriptAssertf(dataSize <= stat->GetSizeOfData(), "%s : STAT_SET_USER_ID - Stat with key=\"%s\", data=\"%s\" is bigger than %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName(), data, stat->GetSizeOfData());

			if (dataSize <= stat->GetSizeOfData())
			{
				scriptAssertf(!stat->GetIsCodeStat() || !coderAssert, "%s : STAT_SET_USER_ID - Stat \"%s\" is updated in code.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());

				scriptAssertf(stat->GetIsBaseType(STAT_TYPE_USERID), "%s : STAT_SET_USER_ID - Invalid stat=\"%s\" - type=\"%s\"", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName(), stat->GetTypeLabel());

				if (stat->GetIsBaseType(STAT_TYPE_USERID))
				{
#if __BANK
					if (CheckForExtraStatSpew(statIter)) scriptDebugf1("Stat \"%s\" NEW value=\"%s\"", statIter.GetKeyName(), data);
#endif // __BANK

					u32 updateflags = STATUPDATEFLAG_ASSERT_SCRIPT_UPDATE;
					if (stat->GetDesc().GetFlushPriority() > 0)
					{
						updateflags = STATUPDATEFLAG_DEFAULT_SCRIPT_UPDATE;
					}

					char* text = const_cast<char*>(data);

					scriptDebugf1("%s:STAT_SET_USER_ID - Setup user id in Stat (%s) - %s.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName(), text);

					ret = StatsDataMgr.SetStatIterData(statIter, text, dataSize, updateflags);

					CommandStatAssert(ret, "STAT_SET_USER_ID", keyHash, statIter.GetKeyName());
				}
			}
		}

		return ret;
	}

	bool CommandStatSetCurrentPosixTime(const int keyHash, bool ASSERT_ONLY(coderAssert))
	{
		bool ret = false;

		if (!CStatsUtils::IsStatsTrackingEnabled())
		{
			scriptDebugf1("%s : Stats are disabled", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			return ret;
		}

		CStatsDataMgr& StatsDataMgr = CStatsMgr::GetStatsDataMgr();

		const sStatData* stat = 0;

		CStatsDataMgr::StatsMap::Iterator statIter;
		if (StatsDataMgr.StatIterFind(keyHash, statIter))
		{
			if (statIter.GetKey().IsValid())
			{
				stat = *statIter;
			}
		}

		scriptAssertf(stat, "%s : STAT_SET_CURRENT_POSIX_TIME - Stat with key=\"%d\" does not exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), keyHash);
		if (stat)
		{
			if (stat->GetDesc().GetIsOnlineData())
			{
				scriptAssertf(!StatsInterface::CloudFileLoadPending(0), "%s : STAT_SET_CURRENT_POSIX_TIME - Trying to change Stat with key=\"%s\" but stats are not loaded.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());
				scriptAssertf(!StatsInterface::CloudFileLoadFailed(0),  "%s : STAT_SET_CURRENT_POSIX_TIME - Trying to change Stat with key=\"%s\" but stats loaded have failed.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());
				if(StatsInterface::CloudFileLoadPending(0) || StatsInterface::CloudFileLoadFailed(0))
				{
					return ret;
				}
			}

			scriptAssertf(!stat->GetIsCodeStat() || !coderAssert, "%s : STAT_SET_CURRENT_POSIX_TIME - Stat \"%s\" is updated in code.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());
			scriptAssertf(stat->GetIsBaseType(STAT_TYPE_UINT64), "%s : STAT_SET_CURRENT_POSIX_TIME - Invalid stat=\"%s\" - type=\"%s\"", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName(), stat->GetTypeLabel());
			if (stat->GetIsBaseType(STAT_TYPE_UINT64))
			{
				u64 data = rlGetPosixTime();

				BANK_ONLY( if (PARAM_spewStatsUsage.Get()) scriptDebugf1("STAT_SET_CURRENT_POSIX_TIME - stat %s, value=%" I64FMT "d", statIter.GetKeyName(), data); )
				BANK_ONLY( if (CheckForExtraStatSpew(statIter)) scriptDebugf1("Stat \"%s\" NEW value=\"%" I64FMT "d\"", statIter.GetKeyName(), data); )

				u32 updateflags = STATUPDATEFLAG_ASSERT_SCRIPT_UPDATE;
				if (stat->GetDesc().GetFlushPriority() > 0)
				{
					updateflags = STATUPDATEFLAG_DEFAULT_SCRIPT_UPDATE;
				}

				scriptDebugf1("%s:STAT_SET_CURRENT_POSIX_TIME - Setup user id in Stat (%s) - %" I64FMT "d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName(), data);

				ret = StatsDataMgr.SetStatIterData(statIter, &data, stat->GetSizeOfData(), updateflags);

				CommandStatAssert(ret, "STAT_SET_CURRENT_POSIX_TIME", keyHash, statIter.GetKeyName());
			}
		}

		return ret;
	}

	bool CommandGetRemotePlayerProfileStatValue(const int keyHash, const CNetGamePlayer* player, void* data, const char* ASSERT_ONLY(command))
	{
		bool ret = false;

		if (scriptVerify(player && !player->IsLocal()))
		{
			const sStatData* stat = 0;

			CStatsDataMgr::StatsMap::Iterator statIter;
			if (CStatsMgr::GetStatsDataMgr().StatIterFind(keyHash, statIter))
			{
				if (statIter.GetKey().IsValid())
				{
					stat = *statIter;
				}
			}

			scriptAssertf(stat, "%s : %s - Stat with key=\"%d\" does not exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), command, keyHash);
			if (stat)
			{
				scriptAssertf(player->GetGamerInfo().IsValid(), "%s : %s - Invalid Gamer Info"
										,CTheScripts::GetCurrentScriptNameAndProgramCounter()
										,command);
				scriptAssertf(stat->GetDesc().GetIsProfileStat(), "%s : %s - Stat \"%s\" is not a profile stat"
															,CTheScripts::GetCurrentScriptNameAndProgramCounter()
															,command
															,statIter.GetKeyName());
				scriptAssertf(stat->GetDesc().GetReadForRemotePlayers(), "%s : %s - Stat \"%s\" is not a stat read in for remote players"
															,CTheScripts::GetCurrentScriptNameAndProgramCounter()
															,command
															,statIter.GetKeyName());

				if (player->GetGamerInfo().IsValid() && stat->GetDesc().GetIsProfileStat() && stat->GetDesc().GetReadForRemotePlayers())
				{
					const rlProfileStatsValue* value = CStatsMgr::GetStatsDataMgr().GetRemoteProfileStat(player->GetGamerInfo().GetGamerHandle(), keyHash);
					scriptAssertf(value, "%s : %s - Stat \"%s\" is not found"
													,CTheScripts::GetCurrentScriptNameAndProgramCounter()
													,command
													,statIter.GetKeyName());

					if (value)
					{
						ret = true;

						if (RL_PROFILESTATS_TYPE_INT32 == value->GetType())
						{
							int datavalue = value->GetInt32();
							sysMemCpy(data, &datavalue, sizeof(int));
						}
						else if (RL_PROFILESTATS_TYPE_FLOAT == value->GetType())
						{
							float datavalue = value->GetFloat();
							sysMemCpy(data, &datavalue, sizeof(float));
						}
						else if (RL_PROFILESTATS_TYPE_INT64 == value->GetType())
						{
							s64 datavalue = value->GetInt64();
							sysMemCpy(data, &datavalue, sizeof(s64));
						}
						else
						{
							ret = false;
						}
					}
				}

				ASSERT_ONLY( CommandStatAssert(ret, command, keyHash, statIter.GetKeyName()); )
			}
		}

		return ret;
	}

	bool CommandStatGetInt(const int keyHash, int& data, int UNUSED_PARAM(playerindex))
	{
		bool ret = false;

		const sStatData* stat = 0;

		CStatsDataMgr::StatsMap::Iterator statIter;
		if (CStatsMgr::GetStatsDataMgr().StatIterFind(keyHash, statIter))
		{
			if (statIter.GetKey().IsValid())
			{
				stat = *statIter;
			}
		}

		scriptAssertf(stat, "%s : STAT_GET_INT - Stat with key=\"%d\" does not exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), keyHash);
		if (stat)
		{
			if (stat->GetDesc().GetIsOnlineData())
			{
				scriptAssertf(!StatsInterface::CloudFileLoadPending(0), "%s : STAT_GET_INT - Trying to Get Stat with key=\"%s\" but stats are not loaded.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());
				scriptAssertf(!StatsInterface::CloudFileLoadFailed(0),  "%s : STAT_GET_INT - Trying to Get Stat with key=\"%s\" but stats loaded have failed.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());
				if(StatsInterface::CloudFileLoadPending(0) || StatsInterface::CloudFileLoadFailed(0))
				{
					return ret;
				}
			}

			if (PARAM_spewStatsUsage.Get()) scriptDebugf1("STAT_GET_INT - stat %s   value %d", statIter.GetKeyName(), stat->GetInt());

			ret = true;
			data = stat->GetInt();

			scriptAssertf(stat->GetIsBaseType(STAT_TYPE_INT) || stat->GetIsBaseType(STAT_TYPE_UINT8)
				|| stat->GetIsBaseType(STAT_TYPE_UINT16) || stat->GetIsBaseType(STAT_TYPE_UINT32) 
				|| stat->GetIsBaseType(STAT_TYPE_UINT64) || stat->GetIsBaseType(STAT_TYPE_DATE)
				|| stat->GetIsBaseType(STAT_TYPE_INT64),
				"%s : STAT_GET_INT - Invalid stat=\"%s\" - type=\"%s\"", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName(), stat->GetTypeLabel());
		}

#if __BANK
		if (CStatsMgr::GetStatsDataMgr().Bank_ScriptTestStatGetFail())
		{
			data = -1;
		}
#endif // __BANK

		return ret;
	}

	bool CommandStatGetFloat (const int keyHash, float& data, int UNUSED_PARAM(playerindex))
	{
		bool ret = false;

		const sStatData* stat = 0;

		CStatsDataMgr::StatsMap::Iterator statIter;
		if (CStatsMgr::GetStatsDataMgr().StatIterFind(keyHash, statIter))
		{
			if (statIter.GetKey().IsValid())
			{
				stat = *statIter;
			}
		}

		scriptAssertf(stat, "%s : STAT_GET_FLOAT - Stat with key=\"%d\" does not exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), keyHash);
		if(stat)
		{
			if (stat->GetDesc().GetIsOnlineData())
			{
				scriptAssertf(!StatsInterface::CloudFileLoadPending(0), "%s : STAT_GET_FLOAT - Trying to Get Stat with key=\"%s\" but stats are not loaded.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());
				scriptAssertf(!StatsInterface::CloudFileLoadFailed(0),  "%s : STAT_GET_FLOAT - Trying to Get Stat with key=\"%s\" but stats loaded have failed.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());
				if(StatsInterface::CloudFileLoadPending(0) || StatsInterface::CloudFileLoadFailed(0))
				{
					return ret;
				}
			}

			if (PARAM_spewStatsUsage.Get()) scriptDebugf1("STAT_GET_FLOAT - stat %s", statIter.GetKeyName());

			ret = stat->GetIsBaseType(STAT_TYPE_FLOAT);
			scriptAssertf(ret, "%s : STAT_GET_FLOAT - Invalid stat=\"%s\" - type=\"%s\"", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName(), stat->GetTypeLabel());

			if (ret)
			{
				ret  = true;
				data = stat->GetFloat();
			}

			CommandStatAssert(ret, "STAT_GET_FLOAT", keyHash, statIter.GetKeyName());
		}

#if __BANK
		if (CStatsMgr::GetStatsDataMgr().Bank_ScriptTestStatGetFail())
		{
			ret = false;
		}
#endif // __BANK

		return ret;
	}

	bool CommandStatGetBool  (const int keyHash, int& data, int UNUSED_PARAM(playerindex))
	{
		bool ret = false;

		data = 0; // Set all four bytes to 0 - FALSE in the scripting language

		const sStatData* stat = 0;

		CStatsDataMgr::StatsMap::Iterator statIter;
		if (CStatsMgr::GetStatsDataMgr().StatIterFind(keyHash, statIter))
		{
			if (statIter.GetKey().IsValid())
			{
				stat = *statIter;
			}
		}

		scriptAssertf(stat, "%s : STAT_GET_BOOL - Stat with key=\"%d\" does not exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), keyHash);
		if (stat)
		{
			if (stat->GetDesc().GetIsOnlineData())
			{
				scriptAssertf(!StatsInterface::CloudFileLoadPending(0), "%s : STAT_GET_BOOL - Trying to Get Stat with key=\"%s\" but stats are not loaded.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());
				scriptAssertf(!StatsInterface::CloudFileLoadFailed(0),  "%s : STAT_GET_BOOL - Trying to Get Stat with key=\"%s\" but stats loaded have failed.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());
				if(StatsInterface::CloudFileLoadPending(0) || StatsInterface::CloudFileLoadFailed(0))
				{
					return ret;
				}
			}

			if (PARAM_spewStatsUsage.Get()) scriptDebugf1("STAT_GET_BOOL - stat %s", statIter.GetKeyName());

			ret = stat->GetIsBaseType(STAT_TYPE_BOOLEAN);
			scriptAssertf(ret, "%s : STAT_GET_BOOL - Invalid stat=\"%s\" - type=\"%s\"", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName(), stat->GetTypeLabel());

			if (ret)
			{
				ret = true;

				if (stat->GetBoolean())
				{
					data = 1; //or shift right 24 bits - 1 = TRUE in the scripting language
				}
			}

			CommandStatAssert(ret, "STAT_GET_BOOL", keyHash, statIter.GetKeyName());
		}

#if __BANK
		if (CStatsMgr::GetStatsDataMgr().Bank_ScriptTestStatGetFail())
		{
			ret = false;
		}
#endif // __BANK

		return ret;
	}

	bool CommandStatGetGxtLabel(const int keyHash, int& data, int UNUSED_PARAM(playerindex))
	{
		bool ret = false;

		const sStatData* stat = 0;

		CStatsDataMgr::StatsMap::Iterator statIter;
		if (CStatsMgr::GetStatsDataMgr().StatIterFind(keyHash, statIter))
		{
			if (statIter.GetKey().IsValid())
			{
				stat = *statIter;
			}
		}

		scriptAssertf(stat, "%s : STAT_GET_GXT_LABEL - Stat with key=\"%d\" does not exist.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), keyHash);
		if (stat)
		{
			if (stat->GetDesc().GetIsOnlineData())
			{
				scriptAssertf(!StatsInterface::CloudFileLoadPending(0), "%s : STAT_GET_GXT_LABEL - Trying to Get Stat with key=\"%s\" but stats are not loaded.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());
				scriptAssertf(!StatsInterface::CloudFileLoadFailed(0),  "%s : STAT_GET_GXT_LABEL - Trying to Get Stat with key=\"%s\" but stats loaded have failed.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());
				if(StatsInterface::CloudFileLoadPending(0) || StatsInterface::CloudFileLoadFailed(0))
				{
					return ret;
				}
			}

			if (PARAM_spewStatsUsage.Get()) scriptDebugf1("STAT_GET_GXT_LABEL - stat %s", statIter.GetKeyName());

			ret = stat->GetIsBaseType(STAT_TYPE_TEXTLABEL);
			scriptAssertf(ret, "%s : STAT_GET_GXT_LABEL - Invalid Stat %s type %s type.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName(), stat->GetTypeLabel());

			if (ret)
			{
				data = stat->GetInt();
				ret = true;
			}

			CommandStatAssert(ret, "STAT_GET_GXT_LABEL", keyHash, statIter.GetKeyName());
		}

#if __BANK
		if (CStatsMgr::GetStatsDataMgr().Bank_ScriptTestStatGetFail())
		{
			ret = false;
		}
#endif // __BANK

		return ret;
	}

	bool CommandStatGetDate  (const int keyHash, int& data, int sizeOfData, int UNUSED_PARAM(playerindex))
	{
		bool ret = false;

		const sStatData* stat = 0;

		CStatsDataMgr::StatsMap::Iterator statIter;
		if (CStatsMgr::GetStatsDataMgr().StatIterFind(keyHash, statIter))
		{
			if (statIter.GetKey().IsValid())
			{
				stat = *statIter;
			}
		}

		scriptAssertf(stat, "%s : STAT_GET_DATE - Stat with key=\"%d\" does not exist.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), keyHash);
		if (stat)
		{
			if (stat->GetDesc().GetIsOnlineData())
			{
				scriptAssertf(!StatsInterface::CloudFileLoadPending(0), "%s : STAT_GET_DATE - Trying to Get Stat with key=\"%s\" but stats are not loaded.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());
				scriptAssertf(!StatsInterface::CloudFileLoadFailed(0),  "%s : STAT_GET_DATE - Trying to Get Stat with key=\"%s\" but stats loaded have failed.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());
				if(StatsInterface::CloudFileLoadPending(0) || StatsInterface::CloudFileLoadFailed(0))
				{
					return ret;
				}
			}

			if (PARAM_spewStatsUsage.Get()) scriptDebugf1("STAT_GET_DATE - stat %s", statIter.GetKeyName());

			ret = stat->GetIsBaseType(STAT_TYPE_DATE);
			scriptAssertf(ret, "%s : STAT_GET_DATE - Invalid Stat %s type %s type.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName(), stat->GetTypeLabel());

			if (ret)
			{
				sizeOfData *= sizeof(scrValue); // Script returns size in script words

				u64 date = 0;

				date = stat->GetUInt64();
				ret = true;

				if (scriptVerify(ret))
				{
					int year, month, day, hour, min, sec, mill;
					year = month = day = hour = min = sec = mill = 0;

					StatsInterface::UnPackDate(date, year , month , day , hour , min , sec , mill);

					scrValue* dataNew = (scrValue*)&data;
					int size = 0;
					dataNew[size].Int = year ; ++size;
					dataNew[size].Int = month; ++size;
					dataNew[size].Int = day  ; ++size;
					dataNew[size].Int = hour ; ++size;
					dataNew[size].Int = min  ; ++size;
					dataNew[size].Int = sec  ; ++size;
					dataNew[size].Int = mill ; ++size;

					scriptAssertf((int)sizeof(scrValue)*size == sizeOfData,   "%s : STAT_GET_DATE - Invalid struct size=\"%d\", should be \"%" SIZETFMT "d\"", CTheScripts::GetCurrentScriptNameAndProgramCounter(), sizeOfData, sizeof(scrValue)*size);
				}
			}

			CommandStatAssert(ret, "STAT_GET_DATE", keyHash, statIter.GetKeyName());
		}

#if __BANK
		if (CStatsMgr::GetStatsDataMgr().Bank_ScriptTestStatGetFail())
		{
			ret = false;
		}
#endif // __BANK

		return ret;
	}

	const char* CommandStatGetString(const int keyHash, int UNUSED_PARAM(playerindex))
	{
		const sStatData* stat = 0;

		CStatsDataMgr::StatsMap::Iterator statIter;
		if (CStatsMgr::GetStatsDataMgr().StatIterFind(keyHash, statIter))
		{
			if (statIter.GetKey().IsValid())
			{
				stat = *statIter;
			}
		}

		scriptAssertf(stat, "%s : STAT_GET_STRING - Stat with key=\"%d\" does not exist.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), keyHash);
		if (stat)
		{
			if (stat->GetDesc().GetIsOnlineData())
			{
				scriptAssertf(!StatsInterface::CloudFileLoadPending(0), "%s : STAT_GET_STRING - Trying to Get Stat with key=\"%s\" but stats are not loaded.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());
				scriptAssertf(!StatsInterface::CloudFileLoadFailed(0),  "%s : STAT_GET_STRING - Trying to Get Stat with key=\"%s\" but stats loaded have failed.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());
				if(StatsInterface::CloudFileLoadPending(0) || StatsInterface::CloudFileLoadFailed(0))
				{
					return "STAT_UNKNOWN";
				}
			}

			if (PARAM_spewStatsUsage.Get()) scriptDebugf1("STAT_GET_STRING - stat %s", statIter.GetKeyName());

			scriptAssertf(stat->GetIsBaseType(STAT_TYPE_STRING), "%s : STAT_GET_STRING - Invalid Stat %s type %s type.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName(), stat->GetTypeLabel());

			if (stat->GetIsBaseType(STAT_TYPE_STRING))
			{
				return stat->GetString();
			}
		}

		return "STAT_UNKNOWN";
	}

	bool CommandStatGetPos(const int keyHash, float& X, float& Y, float& Z, int UNUSED_PARAM(playerindex))
	{
		bool ret = false;

		const sStatData* stat = 0;

		CStatsDataMgr::StatsMap::Iterator statIter;
		if (CStatsMgr::GetStatsDataMgr().StatIterFind(keyHash, statIter))
		{
			if (statIter.GetKey().IsValid())
			{
				stat = *statIter;
			}
		}

		scriptAssertf(stat, "%s : STAT_GET_POS - Stat with key=\"%d\" does not exist.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), keyHash);
		if (stat)
		{
			if (stat->GetDesc().GetIsOnlineData())
			{
				scriptAssertf(!StatsInterface::CloudFileLoadPending(0), "%s : STAT_GET_POS - Trying to Get Stat with key=\"%s\" but stats are not loaded.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());
				scriptAssertf(!StatsInterface::CloudFileLoadFailed(0),  "%s : STAT_GET_POS - Trying to Get Stat with key=\"%s\" but stats loaded have failed.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());
				if(StatsInterface::CloudFileLoadPending(0) || StatsInterface::CloudFileLoadFailed(0))
				{
					return ret;
				}
			}

			if (PARAM_spewStatsUsage.Get()) scriptDebugf1("STAT_GET_POS - stat %s", statIter.GetKeyName());

			ret = stat->GetIsBaseType(STAT_TYPE_POS);
			scriptAssertf(ret, "%s : STAT_GET_POS - Invalid Stat=\"%s\" type.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());

			if (ret)
			{
				u64 pos = 0;

				pos = stat->GetUInt64();
				ret = true;

				if (scriptVerify(ret))
				{
					float fx, fy, fz;
					StatsInterface::UnPackPos(pos, fx, fy, fz);
					X = fx;
					Y = fy;
					Z = fz;
				}
			}

			CommandStatAssert(ret, "STAT_GET_POS", keyHash, statIter.GetKeyName());
		}

#if __BANK
		if (CStatsMgr::GetStatsDataMgr().Bank_ScriptTestStatGetFail())
		{
			ret = false;
		}
#endif // __BANK

		return ret;
	}

	bool CommandStatGetMaskedInt(const int keyHash, int& data, int shift, int numberOfBits, int UNUSED_PARAM(playerindex))
	{
		bool ret = false;

		const sStatData* stat = 0;

		CStatsDataMgr::StatsMap::Iterator statIter;
		if (CStatsMgr::GetStatsDataMgr().StatIterFind(keyHash, statIter))
		{
			if (statIter.GetKey().IsValid())
			{
				stat = *statIter;
			}
		}

		scriptAssertf(stat, "%s : STAT_GET_MASKED_INT - Stat with key=\"%d\" does not exist.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), keyHash);
		if (stat)
		{
			ret = StatsInterface::GetMaskedInt(keyHash, data, shift, numberOfBits, -1);

			CommandStatAssert(ret, "STAT_GET_MASKED_INT", keyHash, statIter.GetKeyName());

#if __BANK
			if (CStatsMgr::GetStatsDataMgr().Bank_ScriptTestStatGetFail())
			{
				ret = false;
			}
#endif // __BANK
		}

		return ret;
	}

	const char* CommandStatGetUserId(const int keyHash)
	{
		if (PARAM_spewStatsUsage.Get()) scriptDebugf1("STAT_GET_USER_ID - stat %d", keyHash);

		const sStatData* stat = 0;

		CStatsDataMgr::StatsMap::Iterator statIter;
		if (CStatsMgr::GetStatsDataMgr().StatIterFind(keyHash, statIter))
		{
			if (statIter.GetKey().IsValid())
			{
				stat = *statIter;
			}
		}

		scriptAssertf(stat, "%s : STAT_GET_USER_ID - Stat with key=\"%d\" does not exist.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), keyHash);
		if (stat)
		{
			if (stat->GetDesc().GetIsOnlineData())
			{
				scriptAssertf(!StatsInterface::CloudFileLoadPending(0), "%s : STAT_GET_USER_ID - Trying to Get Stat with key=\"%s\" but stats are not loaded.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());
				scriptAssertf(!StatsInterface::CloudFileLoadFailed(0),  "%s : STAT_GET_USER_ID - Trying to Get Stat with key=\"%s\" but stats loaded have failed.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());
				if(StatsInterface::CloudFileLoadPending(0) || StatsInterface::CloudFileLoadFailed(0))
				{
					return "STAT_UNKNOWN";
				}
			}

			scriptAssertf(stat->GetIsBaseType(STAT_TYPE_USERID), "%s : STAT_GET_USER_ID - Invalid Stat %s type %s type.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName(), stat->GetTypeLabel());
			if(stat->GetIsBaseType(STAT_TYPE_USERID))
			{
				static char s_userIdStr[128];
				s_userIdStr[0] = '\0';
				if (stat->GetUserId(s_userIdStr, stat->GetSizeOfData()))
				{
					return s_userIdStr;
				}
			}
		}

		return "STAT_UNKNOWN";
	}

	int CommandStatGetPosixTimeDifference( const int keyHash )
	{
		int ret = 0;

		const sStatData* stat = 0;

		CStatsDataMgr::StatsMap::Iterator statIter;
		if (CStatsMgr::GetStatsDataMgr().StatIterFind(keyHash, statIter))
		{
			if (statIter.GetKey().IsValid())
			{
				stat = *statIter;
			}
		}

		scriptAssertf(stat, "%s : STAT_GET_POSIX_TIME_DIFFERENCE - Stat with key=\"%d\" does not exist.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), keyHash);
		if (stat)
		{
			s64 posixTime = (s64)(rlGetPosixTime());
			if (stat->GetIsBaseType(STAT_TYPE_INT))
			{
				ret = (int)(posixTime - (s64)StatsInterface::GetIntStat(keyHash));
			}
			else if (stat->GetIsBaseType(STAT_TYPE_INT64))
			{
				ret = (int)(posixTime - (s64)StatsInterface::GetInt64Stat(keyHash));
			}
		}

		return ret;
	}

	void CommandStatNetworkIncrementOnSuicide(const int keyHash, float amount)
	{
		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s : STAT_NETWORK_INCREMENT_ON_SUICIDE - Not in a network game.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (NetworkInterface::IsGameInProgress())
		{
			CPed* playerPed = FindPlayerPed();
			scriptAssertf(playerPed, "%s : STAT_NETWORK_INCREMENT_ON_SUICIDE - Player does not exist.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			
			if (playerPed)
			{
				scriptAssertf(playerPed->IsFatallyInjured(), "%s : STAT_NETWORK_INCREMENT_ON_SUICIDE - Player is not dead.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
				if (playerPed->IsFatallyInjured())
				{
					if (PARAM_spewStatsUsage.Get()) scriptDebugf1("STAT_NETWORK_INCREMENT_ON_SUICIDE - stat %d", keyHash);
					scriptAssertf(StatsInterface::IsKeyValid(keyHash), "%s : STAT_NETWORK_INCREMENT_ON_SUICIDE - Stat with key=\"%d\" does not exist.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), keyHash);
					StatsInterface::IncrementStat(keyHash, amount);
				}
			}
		}
	}

	void  CommandStatSetCheatIsActive()
	{
		scriptAssertf(!NetworkInterface::IsGameInProgress(), "%s : STAT_SET_CHEAT_IS_ACTIVE - Single Player comamnd.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!NetworkInterface::IsGameInProgress())
		{
			CStatsMgr::SetCheatIsActive(true);
		}
	}

	void CommandStatIncrement(const int keyHash, float amount)
	{
		if (PARAM_spewStatsUsage.Get()) scriptDebugf1("STAT_INCREMENT - stat %d", keyHash);

		if (STAT_CASINO_CHIPS.GetHash() == (u32)keyHash)
		{
			scriptErrorf("%s:STAT_SET_INT - Stat %s cant be changed by script", CTheScripts::GetCurrentScriptNameAndProgramCounter(), atHashString(keyHash).TryGetCStr());
			return;
		}

		scriptAssertf(StatsInterface::IsKeyValid(keyHash), "%s : STAT_INCREMENT - Stat with key=\"%d\" does not exist.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), keyHash);
		StatsInterface::IncrementStat(keyHash, amount);
		CStatsMgr::DisplayScriptStatUpdateMessage(INCREMENT_STAT, keyHash, amount);
	}

	void CommandStatDecrement(const int keyHash, float amount)
	{
		if (PARAM_spewStatsUsage.Get()) scriptDebugf1("STAT_DECREMENT - stat %d", keyHash);

		if (STAT_CASINO_CHIPS.GetHash() == (u32)keyHash)
		{
			scriptErrorf("%s:STAT_SET_INT - Stat %s cant be changed by script", CTheScripts::GetCurrentScriptNameAndProgramCounter(), atHashString(keyHash).TryGetCStr());
			return;
		}

		scriptAssertf(StatsInterface::IsKeyValid(keyHash), "%s : STAT_DECREMENT - Stat with key=\"%d\" does not exist.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), keyHash);
		StatsInterface::DecrementStat(keyHash, amount);
		CStatsMgr::DisplayScriptStatUpdateMessage(DECREMENT_STAT, keyHash, amount);
	}

	void CommandStatSetGreater(const int keyHash, float value)
	{
		scriptAssertf(StatsInterface::IsKeyValid(keyHash), "%s : STAT_SET_GREATER - Stat with key=\"%d\" does not exist.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), keyHash);

		if (STAT_CASINO_CHIPS.GetHash() == (u32)keyHash)
		{
			scriptErrorf("%s:STAT_SET_INT - Stat %s cant be changed by script", CTheScripts::GetCurrentScriptNameAndProgramCounter(), atHashString(keyHash).TryGetCStr());
			return;
		}

		StatsInterface::SetGreater(keyHash, value);
		CStatsMgr::DisplayScriptStatUpdateMessage(INCREMENT_STAT, keyHash, value);
	}

	void CommandStatSetLesser(const int keyHash, float value)
	{
		if (PARAM_spewStatsUsage.Get()) scriptDebugf1("STAT_SET_LESSER - stat %d", keyHash);

		scriptAssertf(StatsInterface::IsKeyValid(keyHash), "%s : STAT_SET_LESSER - Stat with key=\"%d\" does not exist.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), keyHash);

		if (STAT_CASINO_CHIPS.GetHash() == (u32)keyHash)
		{
			scriptErrorf("%s:STAT_SET_INT - Stat %s cant be changed by script", CTheScripts::GetCurrentScriptNameAndProgramCounter(), atHashString(keyHash).TryGetCStr());
			return;
		}

		StatsInterface::SetLesser(keyHash, value);
		CStatsMgr::DisplayScriptStatUpdateMessage(INCREMENT_STAT, keyHash, value);
	}

	bool CommandStatGetSupportsLabels(const int keyHash)
	{
		if (PARAM_spewStatsUsage.Get()) scriptDebugf1("STAT_GET_SUPPORTS_LABELS - stat %d", keyHash);

		scriptAssertf(StatsInterface::IsKeyValid(keyHash), "%s : STAT_GET_SUPPORTS_LABELS - Stat with key=\"%d\" does not exist.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), keyHash);
		return StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_TEXTLABEL);
	}

	bool CommandStatGetSupportsString(const int keyHash)
	{
		if (PARAM_spewStatsUsage.Get()) scriptDebugf1("STAT_GET_SUPPORTS_STRINGS - stat %d", keyHash);

		scriptAssertf(StatsInterface::IsKeyValid(keyHash), "%s : STAT_GET_SUPPORTS_STRINGS - Stat with key=\"%d\" does not exist.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), keyHash);
		return StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_STRING);
	}

	bool CommandStatSetLicensePlate( const int keyHash, const char* data )
	{
		bool ret = false;

		if (!CStatsUtils::IsStatsTrackingEnabled())
		{
			scriptDebugf1("%s : Stats are disabled", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			return ret;
		}

		if(!SCRIPT_VERIFY(data, "STAT_SET_LICENSE_PLATE - String is empty"))
			return ret;

		CStatsDataMgr& StatsDataMgr = CStatsMgr::GetStatsDataMgr();

		const sStatData* stat = 0;

		CStatsDataMgr::StatsMap::Iterator statIter;
		if (StatsDataMgr.StatIterFind(keyHash, statIter))
		{
			if (statIter.GetKey().IsValid())
			{
				stat = *statIter;
			}
		}

		scriptAssertf(stat, "%s : STAT_SET_LICENSE_PLATE - Stat with key=\"%d\" does not exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), keyHash);
		if (stat)
		{
			const u32 bufSize = static_cast<u32>(strlen(data));
			scriptAssertf(bufSize <= 8, "%s : STAT_SET_LICENSE_PLATE - Stat=\"%s\", Set License plate \"%s\" size \"%d\" is too big.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName(), data, bufSize);
			if (bufSize > 8)
				return ret;

			if (!scriptVerify(stat->GetDesc().GetIsOnlineData()))
				return ret;

			scriptAssertf(!StatsInterface::CloudFileLoadPending(0), "%s : STAT_SET_LICENSE_PLATE - Trying to change Stat with key=\"%s\" but stats are not loaded.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());
			scriptAssertf(!StatsInterface::CloudFileLoadFailed(0),  "%s : STAT_SET_LICENSE_PLATE - Trying to change Stat with key=\"%s\" but stats loaded have failed.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());
			if(StatsInterface::CloudFileLoadPending(0) || StatsInterface::CloudFileLoadFailed(0))
				return ret;

			if (PARAM_spewStatsUsage.Get()) scriptDebugf1("STAT_SET_LICENSE_PLATE - stat %s, value=%s", statIter.GetKeyName(), data);

			scriptAssertf(stat->GetIsBaseType( STAT_TYPE_UINT64 ), "%s : STAT_SET_LICENSE_PLATE - Invalid stat=\"%s\" - type=\"%s\"", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName(), stat->GetTypeLabel());
			scriptAssertf(stat->GetDesc().GetSynched(), "%s : STAT_SET_LICENSE_PLATE - Stat %s is not synched with the server.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());
			if (stat->GetIsBaseType( STAT_TYPE_UINT64 ) && stat->GetDesc().GetSynched())
			{
#if __BANK
				if (CheckForExtraStatSpew(statIter)) scriptDebugf1("Stat \"%s\" NEW value=\"%s\"", statIter.GetKeyName(), data);
#endif // __BANK

				u64 licensevalue = 0;

				u32 shiftcounter = 0;
				for (u32 i=0; i<bufSize; i++,shiftcounter++)
				{
					const u16 dataValue = static_cast< u16 >( data[i] );

					u32 shift = shiftcounter*8;

					u64 oldValue = 0;
					oldValue = licensevalue;

					u64 dataInU64 = ((u64)dataValue) << (shift); //Data shifted to the position it should be
					u64 maskInU64 = ((u64)0xFF) << (shift);      //Shifted to the position were we want the bits changed

					oldValue = oldValue & ~maskInU64; //Old value with 0 on the bits we want to change
					licensevalue = oldValue | dataInU64;
				}

				scriptDebugf1("%s : STAT_SET_LICENSE_PLATE - Stat with key=\"%s\" has license plate=\"%s\".", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName(), data);

				ret = StatsDataMgr.SetStatIterData(statIter, &licensevalue, stat->GetSizeOfData(), STATUPDATEFLAG_DEFAULT_SCRIPT_UPDATE);

				CommandStatAssert(ret, "STAT_SET_LICENSE_PLATE", keyHash, statIter.GetKeyName());
			}
		}

		return ret;
	}

	const char* CommandStatGetLicensePlate( const int keyHash )
	{
		const sStatData* stat = 0;

		CStatsDataMgr::StatsMap::Iterator statIter;
		if (CStatsMgr::GetStatsDataMgr().StatIterFind(keyHash, statIter))
		{
			if (statIter.GetKey().IsValid())
			{
				stat = *statIter;
			}
		}

		scriptAssertf(stat, "%s : STAT_GET_LICENSE_PLATE - Stat with key=\"%d\" does not exist.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), keyHash);
		if (stat)
		{
			if (!scriptVerify(stat->GetDesc().GetIsOnlineData()))
				return "UNKNOWN";

			scriptAssertf(!StatsInterface::CloudFileLoadPending(0), "%s : STAT_GET_LICENSE_PLATE - Trying to Get Stat with key=\"%s\" but stats are not loaded.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());
			scriptAssertf(!StatsInterface::CloudFileLoadFailed(0),  "%s : STAT_GET_LICENSE_PLATE - Trying to Get Stat with key=\"%s\" but stats loaded have failed.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());
			if(StatsInterface::CloudFileLoadPending(0) || StatsInterface::CloudFileLoadFailed(0))
				return "UNKNOWN";

			if (PARAM_spewStatsUsage.Get()) scriptDebugf1("STAT_GET_LICENSE_PLATE - stat %s", statIter.GetKeyName());

			scriptAssertf(stat->GetIsBaseType( STAT_TYPE_UINT64 ), "%s : STAT_GET_LICENSE_PLATE - Invalid Stat %s type %s type.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName(), stat->GetTypeLabel());
			scriptAssertf(stat->GetDesc().GetSynched(), "%s : STAT_GET_LICENSE_PLATE - Stat %s is not synched with the server.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());

			if (stat->GetIsBaseType( STAT_TYPE_UINT64 ) && stat->GetDesc().GetSynched())
			{
				static char s_tmpBuf[16];
				sysMemSet(s_tmpBuf, 0, sizeof(s_tmpBuf));

				u64 licenseplate = stat->GetUInt64();

				u32 shiftcounter = 0;
				for (u32 i=0; i<8; i++,shiftcounter++)
				{
					u32 shift = shiftcounter*8;

					u64 mask        = (1 << 8) - 1;
					u64 maskShifted = (mask << shift);

					int data = 0;
					data = (int)((licenseplate & maskShifted) >> shift);
					statAssert(data<=255);

					s_tmpBuf[i] = static_cast< char >(data);
				}

				scriptDebugf1("%s : STAT_GET_LICENSE_PLATE - Stat with key=\"%s\" has license plate=\"%s\".", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName(), s_tmpBuf);

				return s_tmpBuf;
			}
		}

		return "UNKNOWN";
	}


	/*
	    META STATS COMMANDS
	*/

	float CommandGetPercentShotsInCover(const char* szStatPrefix)
	{
		if (PARAM_spewStatsUsage.Get()) scriptDebugf1("STAT_GET_PERCENT_SHOTS_IN_COVER - stat %s", szStatPrefix);

		float fPercentage = 0.0f;

		StatId totalStatId;
		StatId shotsStatId;

		if (szStatPrefix[0] == '\0')
		{
			totalStatId = STAT_ENTERED_COVER.GetStatId();
			shotsStatId = STAT_ENTERED_COVER_AND_SHOT.GetStatId();
		}
		else
		{
			char szStatString[MAX_STAT_LABEL_SIZE];
			
			szStatString[0] = '\0';
			safecatf(szStatString, MAX_STAT_LABEL_SIZE, "%s_%s", szStatPrefix, "ENTERED_COVER");
			totalStatId = STAT_ID(szStatString);

			szStatString[0] = '\0';
			safecatf(szStatString, MAX_STAT_LABEL_SIZE, "%s_%s", szStatPrefix, "ENTERED_COVER_AND_SHOT");
			shotsStatId = STAT_ID(szStatString);
		}

		if(StatsInterface::IsKeyValid(totalStatId) && StatsInterface::IsKeyValid(shotsStatId))
		{
			float fTotal = static_cast<float>(StatsInterface::GetIntStat(totalStatId));
			float fShots = static_cast<float>(StatsInterface::GetIntStat(shotsStatId));
			fPercentage = (fShots / fTotal) * 100.0f; 
		}
		else
		{
			if (szStatPrefix[0] != '\0')
			{
				scriptAssertf(0, "%s : STAT_GET_PERCENT_SHOTS_IN_COVER - Invalid prefix \"%s\" provided.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), szStatPrefix);
			}
		}

		return fPercentage;
	}

	float CommandGetPercentShotsInCrouch(const char* szStatPrefix)
	{
		if (PARAM_spewStatsUsage.Get()) scriptDebugf1("STAT_GET_PERCENT_SHOTS_IN_CROUCH - stat %s", szStatPrefix);

		float fPercentage = 0.0f;

		StatId totalStatId;
		StatId shotsStatId;

		if (szStatPrefix[0] == '\0')
		{
			CPed* pPlayerPed = FindPlayerPed();
			if (pPlayerPed)
			{
				totalStatId = STAT_CROUCHED.GetStatId();
				shotsStatId = STAT_CROUCHED_AND_SHOT.GetStatId();
			}
		}
		else
		{
			char szStatString[MAX_STAT_LABEL_SIZE];

			szStatString[0] = '\0';
			safecatf(szStatString, MAX_STAT_LABEL_SIZE, "%s_%s", szStatPrefix, "CROUCHED");
			totalStatId = STAT_ID(szStatString);

			szStatString[0] = '\0';
			safecatf(szStatString, MAX_STAT_LABEL_SIZE, "%s_%s", szStatPrefix, "CROUCHED_AND_SHOT");
			shotsStatId = STAT_ID(szStatString);
		}

		if(StatsInterface::IsKeyValid(totalStatId) && StatsInterface::IsKeyValid(shotsStatId))
		{
			float fTotal = static_cast<float>(StatsInterface::GetIntStat(totalStatId));
			float fShots = static_cast<float>(StatsInterface::GetIntStat(shotsStatId));
			fPercentage = (fShots / fTotal) * 100.0f; 
		}
		else
		{
			if (szStatPrefix[0] != '\0')
			{
				scriptAssertf(0, "%s : STAT_GET_PERCENT_SHOTS_IN_CROUCH - Invalid prefix \"%s\" provided.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), szStatPrefix);
			}
		}

		return fPercentage;
	}

	
	/*
	    PLAYSTATS COMMANDS
	*/

	class MetricNPC_INVITE : public MetricPlayStat
	{
		RL_DECLARE_METRIC(NPC_INVITE, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

		static const u32 TEXT_SIZE = MetricPlayStat::MAX_STRING_LENGTH;

	private:
		char m_npcname[TEXT_SIZE];

	public:
		explicit MetricNPC_INVITE(const char* npcname)
		{
			m_npcname[0] = '\0';
			if(statVerify(npcname))
			{
				formatf(m_npcname, COUNTOF(m_npcname), npcname);
			}
		}

		virtual bool Write(RsonWriter* rw) const
		{
			return MetricPlayStat::Write(rw) && rw->WriteString("npc", m_npcname);
		}
	};

#if !__FINAL

	class MetricDebugNPC_INVITE : public MetricNPC_INVITE
	{
		RL_DECLARE_METRIC(NPC_INVITE, TELEMETRY_CHANNEL_JOB, LOGLEVEL_DEBUG1);

	public:
		explicit MetricDebugNPC_INVITE(const char* npcname) : MetricNPC_INVITE(npcname) {}
	};

#endif// !__FINAL

	void  CommandPlayStatsNpcInvite(const char* npcname)
	{
		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_NPC_INVITE: Is a multi player only metric", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(NetworkInterface::IsInFreeMode(), "%s - PLAYSTATS_NPC_INVITE: not in freemode", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if (NetworkInterface::IsGameInProgress() && NetworkInterface::IsInFreeMode())
		{
			bool inviteTelemetryActive = false;
			Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("TELEMETRY_PRESENCE_INVITE_ACTIVE", 0x74f81046), inviteTelemetryActive);

			if (inviteTelemetryActive)
			{
				MetricNPC_INVITE m(npcname);
				CNetworkTelemetry::AppendMetric(m);
			}
#if !__FINAL
			else
			{
				MetricDebugNPC_INVITE m(npcname);
				CNetworkDebugTelemetry::AppendDebugMetric(&m);
			}
#endif// !__FINAL
		}
	}

	class MetricAWARD_XP : public MetricPlayStat
	{
		RL_DECLARE_METRIC(AWARD_XP, TELEMETRY_CHANNEL_AMBIENT, LOGLEVEL_VERYHIGH_PRIORITY);

	private:
		int m_amount;
		int m_type;
		int m_category;
		u32 m_playtime;
		int m_slot;

	public:
		explicit MetricAWARD_XP(const int amount, const int type, const int category) : m_amount(amount), m_type(type), m_category(category), m_playtime(0)
		{
			m_playtime = (u32)StatsInterface::GetUInt64Stat(STAT_TOTAL_PLAYING_TIME.GetHash());
			m_slot = StatsInterface::GetCurrentMultiplayerCharaterSlot();
		}

		virtual bool Write(RsonWriter* rw) const
		{
			return MetricPlayStat::Write(rw)
				&& rw->WriteInt("amt", m_amount)
				&& rw->WriteUns("type", (u32)m_type)
				&& rw->WriteUns("cat", (u32)m_category)
				&& rw->WriteUns("pt", m_playtime)
				&& rw->WriteInt("slot", m_slot);
		}
	};
	class MetricREMOVEAWARD_XP : public MetricAWARD_XP
	{
		RL_DECLARE_METRIC(REMOVE_XP, TELEMETRY_CHANNEL_AMBIENT, LOGLEVEL_VERYHIGH_PRIORITY);
	public:
		explicit MetricREMOVEAWARD_XP(const int amount, const int type, const int category) : MetricAWARD_XP(amount, type, category) {;}
	};

	void  CommandPlayStatsAwardXP(const int amount, const int type, const int category)
	{
		scriptAssertf(NetworkInterface::IsLocalPlayerOnline(), "%s : PLAYSTATS_AWARD_XP - Trying to shop without being online.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(!StatsInterface::CloudFileLoadPending(STAT_MP_CATEGORY_DEFAULT), "%s : PLAYSTATS_AWARD_XP - Trying to shop without loading the savegame.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(!StatsInterface::CloudFileLoadFailed(STAT_MP_CATEGORY_DEFAULT), "%s : PLAYSTATS_AWARD_XP - Trying to shop but stats loaded have failed.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if (amount >= 0)
			CNetworkTelemetry::AppendMetric(MetricAWARD_XP(amount, type, category));
		else
			CNetworkTelemetry::AppendMetric(MetricREMOVEAWARD_XP(amount, type, category));
	}

	class MetricGAMETYPE_GENERIC : public MetricPlayStat
	{
		RL_DECLARE_METRIC(GENERIC, TELEMETRY_CHANNEL_AMBIENT, LOGLEVEL_VERYHIGH_PRIORITY);
	public:
		MetricGAMETYPE_GENERIC(const char* type, int data)
			: MetricPlayStat()
			, m_data(data)
		{
			safecpy(m_type, type);
		}

		virtual bool Write(RsonWriter* rw) const
		{
			return this->MetricPlayStat::Write(rw)
				&& rw->WriteString("type", m_type)
				&& rw->WriteInt("d", m_data);
			;
		}

	protected:
		char m_type[32];
		int	 m_data;
	};

	void  CommandPlayStatsGenericMetric(const char* type, const int data)
	{
		CNetworkTelemetry::AppendMetric(MetricGAMETYPE_GENERIC(type, data));
	}

	class MetricRANK_UP : public MetricPlayStat
	{
		RL_DECLARE_METRIC(RANK_UP, TELEMETRY_CHANNEL_AMBIENT, LOGLEVEL_HIGH_PRIORITY);

	private:
		u32 m_rank;
		u32 m_playtime;
		s64 m_totalEVC;

	public:
		explicit MetricRANK_UP(const u32 rank) : m_rank(rank), m_playtime(0), m_totalEVC(0)
		{
			m_playtime = (u32)StatsInterface::GetUInt64Stat(STAT_TOTAL_PLAYING_TIME.GetHash());
			m_totalEVC = StatsInterface::GetInt64Stat(STAT_TOTAL_EVC.GetStatId());
		}

		virtual bool Write(RsonWriter* rw) const
		{
			return MetricPlayStat::Write(rw)
				&& rw->WriteUns("rank", m_rank)
				&& rw->WriteUns("time", m_playtime)
				&& rw->WriteUns64("evc", (u64)m_totalEVC);
		}
	};

	void  CommandPlayStatsRankUp(const int rank)
	{
		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_RANK_UP: Is a multi player only metric", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(NetworkInterface::IsInFreeMode(), "%s - PLAYSTATS_RANK_UP: not in freemode", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(rank > 0, "%s - PLAYSTATS_RANK_UP: invalid rank = %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), rank);
		if (NetworkInterface::IsGameInProgress() && NetworkInterface::IsInFreeMode() && rank > 0)
		{
			CNetworkTelemetry::AppendMetric(MetricRANK_UP((u32)rank));
		}
	}
	

	class MetricSTART_OFFLINEMODE: public MetricPlayStat
	{
		RL_DECLARE_METRIC(OFFLINEMODE, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);
	};

	void  CommandPlayStatsOfflineMode( )
	{
		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_PLAY_IN_OFFLINEMODE: Is a multi player only metric", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(NetworkInterface::IsInFreeMode(), "%s - PLAYSTATS_PLAY_IN_OFFLINEMODE: not in freemode", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if (NetworkInterface::IsGameInProgress() && NetworkInterface::IsInFreeMode())
		{
			CNetworkTelemetry::AppendMetric(MetricSTART_OFFLINEMODE());
		}
	}


	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////


	class FmEventAmbientMission : public MetricPlayStat
	{
		RL_DECLARE_METRIC(LRAMBM, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		//Common data
		int m_uid0;                  //Unique Mission ID - for that instance, shared between participating players.
		int m_uid1;                  //Unique Mission ID - for that instance, shared between participating players.
		int m_playersNotified;       //Players Notified.
		int m_playersParticipating;  //Number of Players Participating, who collected a crate in any given drop.
		int m_playersLeftInProgress; //Players who left in progress.
		int m_endReason;             //Reason why script ended.
		int m_cashEarned;            //Cash Earned.
		int m_rpEarned;              //RP Earned.
		int m_notifiedTime;          //Notification PosixTime.
		int m_startTime;             //Start PosixTime.
		int m_timeTakenToComplete;   //Time Taken to complete.
		int m_timeTakenForObjective; //Time between notifications being sent and someone actually completing of the objectives.

	public:
		FmEventAmbientMission() : m_uid0(0)
								 , m_uid1(0)
								 , m_playersNotified(0)
								 , m_playersParticipating(0)
								 , m_playersLeftInProgress(0)
								 , m_endReason(0)
								 , m_cashEarned(0)
								 , m_rpEarned(0)
								 , m_notifiedTime(0)
								 , m_startTime(0)
								 , m_timeTakenToComplete(0)
								 , m_timeTakenForObjective(0)
		{;}

		void From(scrFmEventAmbientMission* data)
		{
			m_uid0                  = data->m_uid0.Int;                 
			m_uid1                  = data->m_uid1.Int;                 
			m_playersNotified       = data->m_playersNotified.Int;      
			m_playersParticipating  = data->m_playersParticipating.Int; 
			m_playersLeftInProgress = data->m_playersLeftInProgress.Int;
			m_endReason             = data->m_endReason.Int;            
			m_cashEarned            = data->m_cashEarned.Int;           
			m_rpEarned              = data->m_rpEarned.Int;             
			m_notifiedTime          = data->m_notifiedTime.Int;         
			m_startTime             = data->m_startTime.Int;            
			m_timeTakenToComplete   = data->m_timeTakenToComplete.Int;  
			m_timeTakenForObjective = data->m_timeTakenForObjective.Int;
		}

		virtual bool Write(RsonWriter* rw) const
		{
			const u64 uid0 = ((u64)m_uid0) << 32;
			const u64 uid1 = ((u64)m_uid1);

			return MetricPlayStat::Write(rw)
				&& rw->WriteInt64("uid", uid0|uid1          )
				&& rw->WriteInt("a", m_playersNotified      )
				&& rw->WriteInt("b", m_playersParticipating )
				&& rw->WriteInt("c", m_playersLeftInProgress)
				&& rw->WriteInt("d", m_endReason            )
				&& rw->WriteInt("e", m_cashEarned           )
				&& rw->WriteInt("f", m_rpEarned             )
				&& rw->WriteInt("g", m_notifiedTime         )
				&& rw->WriteInt("h", m_startTime            )
				&& rw->WriteInt("i", m_timeTakenToComplete  )
				&& rw->WriteInt("j", m_timeTakenForObjective);
		}
	};

	//////////////////////////////////////////////////////////////////////////

	//Challenges
	class FmEventAmbMission_Challenges : public FmEventAmbientMission
	{
		RL_DECLARE_METRIC(LRCHA, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		//Challenge type (12 in total) 
		int m_challengeType;
		float m_challengeScore;

	public:
		 FmEventAmbMission_Challenges() : m_challengeType(0) {;}
		 
		 virtual bool Write(RsonWriter* rw) const 
		 {
			 return FmEventAmbientMission::Write(rw) 
				 && rw->WriteInt("l", m_challengeType)
				 && rw->WriteFloat("s", m_challengeScore);
		 }
	};

	void  CommandPlayStatsFmEventAmbMission_Challenges(scrFmEventAmbMission_Challenges* data)
	{
		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_FM_EVENT_CHALLENGES: Is a multi player only metric", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(data, "%s - PLAYSTATS_FM_EVENT_CHALLENGES: NULL data.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if (NetworkInterface::IsGameInProgress() && data)
		{
			FmEventAmbMission_Challenges m;
			m.From(data);
			m.m_challengeType = data->m_challengeType.Int;
			m.m_challengeScore = data->m_challengeScore.Float;
			APPEND_METRIC_FLUSH(m, false);
		}
	}

	//////////////////////////////////////////////////////////////////////////


	//## Hot Target & Helicopter Hot Target:
	class FmEventAmbMission_VehicleTarget : public FmEventAmbientMission
	{
		RL_DECLARE_METRIC(LRVEHT, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		int m_vehicleType;

	public:
		FmEventAmbMission_VehicleTarget() : m_vehicleType(0) {;}

		virtual bool Write(RsonWriter* rw) const 
		{
			return FmEventAmbientMission::Write(rw) 
				&& rw->WriteInt("l", m_vehicleType);
		}
	};

	void  CommandPlayStatsFmEventAmbMission_VehicleTarget(scrFmEventAmbMission_VehicleTarget* data)
	{
		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_FM_EVENT_VEHICLETARGET: Is a multi player only metric", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(data, "%s - PLAYSTATS_FM_EVENT_VEHICLETARGET: NULL data.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if (NetworkInterface::IsGameInProgress() && data)
		{
			FmEventAmbMission_VehicleTarget m;
			m.From(data);
			m.m_vehicleType = data->m_vehicleType.Int;
			APPEND_METRIC_FLUSH(m, false);
		}
	}

	//////////////////////////////////////////////////////////////////////////

	//## Urban Warfare:
	class FmEventAmbMission_UrbanWarfare : public FmEventAmbientMission
	{
		RL_DECLARE_METRIC(LRUW, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		int m_vehicleType; //Vehicle Type
		int m_waveReached; //Wave reached

	public:
		FmEventAmbMission_UrbanWarfare() : m_vehicleType(0), m_waveReached(0) {;}

		virtual bool Write(RsonWriter* rw) const 
		{
			return FmEventAmbientMission::Write(rw) 
				&& rw->WriteInt("l", m_vehicleType)
				&& rw->WriteInt("m", m_waveReached);
		}
	};

	void  CommandPlayStatsFmEventAmbMission_UrbanWarfare(scrFmEventAmbMission_UrbanWarfare* data)
	{
		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_FM_EVENT_URBANWARFARE : Is a multi player only metric", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(data, "%s - PLAYSTATS_FM_EVENT_URBANWARFARE: NULL data.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if (NetworkInterface::IsGameInProgress() && data)
		{
			FmEventAmbMission_UrbanWarfare m;
			m.From(data);
			m.m_vehicleType = data->m_vehicleType.Int;
			m.m_waveReached = data->m_waveReached.Int;
			APPEND_METRIC_FLUSH(m, false);
		}
	}

	//////////////////////////////////////////////////////////////////////////

	//## Checkpoint Collection:
	class FmEventAmbMission_CheckpointCollection : public FmEventAmbientMission
	{
		RL_DECLARE_METRIC(LRCHC, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		int m_quarterMapChosen;        //Quarter of the map chosen
		int m_numCheckpointsCollected; //Number of checkpoints collected
		//Location of any Checkpoints NOT collected
		int m_notCollectedCheckpoints_0;
		int m_notCollectedCheckpoints_1;
		int m_notCollectedCheckpoints_2;
		int m_notCollectedCheckpoints_3;
		int m_notCollectedCheckpoints_4;
		int m_notCollectedCheckpoints_5;
		int m_notCollectedCheckpoints_6;
		int m_notCollectedCheckpoints_7;
		int m_notCollectedCheckpoints_8;
		int m_notCollectedCheckpoints_9;

	public:
		FmEventAmbMission_CheckpointCollection() 
			: m_quarterMapChosen(0)
			, m_numCheckpointsCollected(0) 
			, m_notCollectedCheckpoints_0(0)
			, m_notCollectedCheckpoints_1(0)
			, m_notCollectedCheckpoints_2(0)
			, m_notCollectedCheckpoints_3(0)
			, m_notCollectedCheckpoints_4(0)
			, m_notCollectedCheckpoints_5(0)
			, m_notCollectedCheckpoints_6(0)
			, m_notCollectedCheckpoints_7(0)
			, m_notCollectedCheckpoints_8(0)
			, m_notCollectedCheckpoints_9(0)
		{;}

		virtual bool Write(RsonWriter* rw) const 
		{
			return FmEventAmbientMission::Write(rw)
				&& rw->WriteInt("l", m_quarterMapChosen         )
				&& rw->WriteInt("m", m_numCheckpointsCollected  )
				&& rw->WriteInt("n", m_notCollectedCheckpoints_0)
				&& rw->WriteInt("o", m_notCollectedCheckpoints_1)
				&& rw->WriteInt("p", m_notCollectedCheckpoints_2)
				&& rw->WriteInt("q", m_notCollectedCheckpoints_3)
				&& rw->WriteInt("r", m_notCollectedCheckpoints_4)
				&& rw->WriteInt("s", m_notCollectedCheckpoints_5)
				&& rw->WriteInt("t", m_notCollectedCheckpoints_6)
				&& rw->WriteInt("u", m_notCollectedCheckpoints_7)
				&& rw->WriteInt("v", m_notCollectedCheckpoints_8)
				&& rw->WriteInt("x", m_notCollectedCheckpoints_9);
		}
	};

	void  CommandPlayStatsFmEventAmbMission_CheckpointCollection(scrFmEventAmbMission_CheckpointCollection* data)
	{
		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_FM_EVENT_CHECKPOINTCOLLECTION: Is a multi player only metric", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(data, "%s - PLAYSTATS_FM_EVENT_CHECKPOINTCOLLECTION: NULL data.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if (NetworkInterface::IsGameInProgress() && data)
		{
			FmEventAmbMission_CheckpointCollection m;
			m.From(data);
			m.m_quarterMapChosen          = data->m_quarterMapChosen.Int;
			m.m_numCheckpointsCollected   = data->m_numCheckpointsCollected.Int;
			m.m_notCollectedCheckpoints_0 = data->m_notCollectedCheckpoints_0.Int;
			m.m_notCollectedCheckpoints_1 = data->m_notCollectedCheckpoints_1.Int;
			m.m_notCollectedCheckpoints_2 = data->m_notCollectedCheckpoints_2.Int;
			m.m_notCollectedCheckpoints_3 = data->m_notCollectedCheckpoints_3.Int;
			m.m_notCollectedCheckpoints_4 = data->m_notCollectedCheckpoints_4.Int;
			m.m_notCollectedCheckpoints_5 = data->m_notCollectedCheckpoints_5.Int;
			m.m_notCollectedCheckpoints_6 = data->m_notCollectedCheckpoints_6.Int;
			m.m_notCollectedCheckpoints_7 = data->m_notCollectedCheckpoints_7.Int;
			m.m_notCollectedCheckpoints_8 = data->m_notCollectedCheckpoints_8.Int;
			m.m_notCollectedCheckpoints_9 = data->m_notCollectedCheckpoints_9.Int;
			APPEND_METRIC_FLUSH(m, false);
		}
	}

	//## Plane Drop:
	class FmEventAmbMission_PlaneDrop : public FmEventAmbientMission
	{
		RL_DECLARE_METRIC(LRPD, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		int m_routeTaken;                   //Route taken
		int m_numCratesCollected;           //Number of crates collected
		int m_numCratesDropped;             //Number of crates dropped

	public:
		FmEventAmbMission_PlaneDrop() : m_routeTaken(0), m_numCratesCollected(0), m_numCratesDropped(0){;}

		virtual bool Write(RsonWriter* rw) const 
		{
			return FmEventAmbientMission::Write(rw)
				&& rw->WriteInt("l", m_routeTaken        )
				&& rw->WriteInt("m", m_numCratesCollected)
				&& rw->WriteInt("n", m_numCratesDropped  );
		}
	};

	void  CommandPlayStatsFmEventAmbMission_PlaneDrop(scrFmEventAmbMission_PlaneDrop* data)
	{
		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_FM_EVENT_PLANEDROP: Is a multi player only metric", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(data, "%s - PLAYSTATS_FM_EVENT_PLANEDROP: NULL data.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if (NetworkInterface::IsGameInProgress() && data)
		{
			FmEventAmbMission_PlaneDrop m;
			m.From(data);
			m.m_routeTaken         = data->m_routeTaken.Int;
			m.m_numCratesCollected = data->m_numCratesCollected.Int;
			m.m_numCratesDropped   = data->m_numCratesDropped.Int;
			APPEND_METRIC_FLUSH(m, false);
		}
	}

	//## A to B Time Trial:
	class FmEventAmbMission_ATOB : public FmEventAmbientMission
	{
		RL_DECLARE_METRIC(LRATOB, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		int m_vehicleType;   //Vehicle Type
		int m_parTimeBeaten; //Par Time Beaten?
		int m_type;          //Hash of type of Time Trial the player engaged with - Time Trial FME, RC Time Trial, Hao Time Trial

	public:
		FmEventAmbMission_ATOB() : m_vehicleType(0), m_parTimeBeaten(0), m_type(0) {;}

		virtual bool Write(RsonWriter* rw) const 
		{
			return FmEventAmbientMission::Write(rw)
				&& rw->WriteInt("l", m_vehicleType)
				&& rw->WriteInt("m", m_parTimeBeaten)
				&& rw->WriteInt("n", m_type);
		}
	};

	void  CommandPlayStatsFmEventAmbMission_ATOB(scrFmEventAmbMission_ATOB* data)
	{
		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_FM_EVENT_ATOB: Is a multi player only metric", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(data, "%s - PLAYSTATS_FM_EVENT_ATOB: NULL data.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if (NetworkInterface::IsGameInProgress() && data)
		{
			FmEventAmbMission_ATOB m;
			m.From(data);
			m.m_vehicleType   = data->m_vehicleType.Int;
			m.m_parTimeBeaten = data->m_parTimeBeaten.Int;
			m.m_type          = data->m_type.Int;
			APPEND_METRIC_FLUSH(m, false);
		}
	}

	//## A to B Time Trial:
	class FmEventAmbMission_PENNEDIN : public FmEventAmbientMission
	{
		RL_DECLARE_METRIC(LRPENNEDIN, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		virtual bool Write(RsonWriter* rw) const 
		{
			return FmEventAmbientMission::Write(rw);
		}
	};

	//## Penned In:
	void  CommandPlayStatsFmEventAmbMission_PennedIn(scrFmEventAmbientMission* data)
	{
		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_FM_EVENT_PENNEDIN: Is a multi player only metric", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(data, "%s - PLAYSTATS_FM_EVENT_PENNEDIN: NULL data.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if (NetworkInterface::IsGameInProgress() && data)
		{
			FmEventAmbMission_PENNEDIN m;
			m.From(data);
			APPEND_METRIC_FLUSH(m, false);
		}
	}

	//## Multi-Target Assassination:
	class FmEventAmbMission_TargetAssassination : public FmEventAmbientMission
	{
		RL_DECLARE_METRIC(LRMTA, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		int m_targetsKilled; //Targets killed

	public:
		FmEventAmbMission_TargetAssassination() : m_targetsKilled(0){;}

		virtual bool Write(RsonWriter* rw) const 
		{
			return FmEventAmbientMission::Write(rw) 
				&& rw->WriteInt("l", m_targetsKilled);
		}
	};

	void  CommandPlayStatsFmEventAmbMission_TargetAssassination(scrFmEventAmbMission_TargetAssassination* data)
	{
		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_FM_EVENT_TARGETASSASSINATION: Is a multi player only metric", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(data, "%s - PLAYSTATS_FM_EVENT_TARGETASSASSINATION: NULL data.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if (NetworkInterface::IsGameInProgress() && data)
		{
			FmEventAmbMission_TargetAssassination m;
			m.From(data);
			m.m_targetsKilled = data->m_targetsKilled.Int;
			APPEND_METRIC_FLUSH(m, false);
		}
	}

	//## Pass the Parcel:
	class FmEventAmbMission_PasstheParcel : public FmEventAmbientMission
	{
		RL_DECLARE_METRIC(LRPTP, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		int m_numTimesVehicleExchangeHands; //Number of times vehicle exchanged hands

	public:
		FmEventAmbMission_PasstheParcel() : m_numTimesVehicleExchangeHands(0) {;}

		virtual bool Write(RsonWriter* rw) const 
		{
			return FmEventAmbientMission::Write(rw) 
				&& rw->WriteInt("l", m_numTimesVehicleExchangeHands);
		}
	};

	void  CommandPlayStatsFmEventAmbMission_PasstheParcel(scrFmEventAmbMission_PasstheParcel* data)
	{
		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_FM_EVENT_PASSTHEPARCEL: Is a multi player only metric", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(data, "%s - PLAYSTATS_FM_EVENT_PASSTHEPARCEL: NULL data.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if (NetworkInterface::IsGameInProgress() && data)
		{
			FmEventAmbMission_PasstheParcel m;
			m.From(data);
			m.m_numTimesVehicleExchangeHands = data->m_numTimesVehicleExchangeHands.Int;
			APPEND_METRIC_FLUSH(m, false);
		}
	}

	//## Hot Property
	class FmEventAmbMission_HotProperty : public FmEventAmbientMission
	{
		RL_DECLARE_METRIC(LRHOTP, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		int m_numTimesBriefcaseExchangeHands; //Number of times briefcase exchanged hands
		int m_timeBriefecaseHeld;             //Time briefcase was held for

	public:
		FmEventAmbMission_HotProperty() : m_numTimesBriefcaseExchangeHands(0), m_timeBriefecaseHeld(0) {;}

		virtual bool Write(RsonWriter* rw) const 
		{
			return FmEventAmbientMission::Write(rw) 
				&& rw->WriteInt("l", m_numTimesBriefcaseExchangeHands)
				&& rw->WriteInt("m", m_timeBriefecaseHeld);
		}
	};

	void  CommandPlayStatsFmEventAmbMission_HotProperty(scrFmEventAmbMission_HotProperty* data)
	{
		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_FM_EVENT_HOTPROPERTY: Is a multi player only metric", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(data, "%s - PLAYSTATS_FM_EVENT_HOTPROPERTY: NULL data.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if (NetworkInterface::IsGameInProgress() && data)
		{
			FmEventAmbMission_HotProperty m;
			m.From(data);
			m.m_numTimesBriefcaseExchangeHands = data->m_numTimesBriefcaseExchangeHands.Int;
			m.m_timeBriefecaseHeld             = data->m_timeBriefecaseHeld.Int;
			APPEND_METRIC_FLUSH(m, false);
		}
	}

	//## Dead Drop
	class FmEventAmbMission_DeadDrop : public FmEventAmbientMission
	{
		RL_DECLARE_METRIC(LRDEADD, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		int m_numTimesBagExchangeHands; //Number of times bag exchanged hands
		int m_timeBagHeld;              //Time bag was held for

	public:
		FmEventAmbMission_DeadDrop() : m_numTimesBagExchangeHands(0), m_timeBagHeld(0) {;}

		virtual bool Write(RsonWriter* rw) const 
		{
			return FmEventAmbientMission::Write(rw) 
				&& rw->WriteInt("l", m_numTimesBagExchangeHands)
				&& rw->WriteInt("m", m_timeBagHeld);
		}
	};

	void  CommandPlayStatsFmEventAmbMission_DeadDrop(scrFmEventAmbMission_DeadDrop* data)
	{
		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_FM_EVENT_DEADDROP: Is a multi player only metric", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(data, "%s - PLAYSTATS_FM_EVENT_DEADDROP: NULL data.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if (NetworkInterface::IsGameInProgress() && data)
		{
			FmEventAmbMission_DeadDrop m;
			m.From(data);
			m.m_numTimesBagExchangeHands = data->m_numTimesBagExchangeHands.Int;
			m.m_timeBagHeld              = data->m_timeBagHeld.Int;
			APPEND_METRIC_FLUSH(m, false);
		}
	}
	
	//## King of the Castle
	class FmEventAmbMission_KingoftheCastle : public FmEventAmbientMission
	{
		RL_DECLARE_METRIC(LRKOFC, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		int m_numOfKings;        //Number of times briefcase exchanged hands
		int m_killsAsKing;       //Kills as King
		int m_deathsAsKing;      //Deaths as King
		int m_areaControlPoints; // Area Control Points

	public:
		FmEventAmbMission_KingoftheCastle() : m_numOfKings(0), m_killsAsKing(0), m_deathsAsKing(0), m_areaControlPoints(0) {;}

		virtual bool Write(RsonWriter* rw) const 
		{
			return FmEventAmbientMission::Write(rw) 
				&& rw->WriteInt("l", m_numOfKings)
				&& rw->WriteInt("m", m_killsAsKing)
				&& rw->WriteInt("n", m_deathsAsKing)
				&& rw->WriteInt("o", m_areaControlPoints);
		}
	};

	void  CommandPlayStatsFmEventAmbMission_KingoftheCastle(scrFmEventAmbMission_KingoftheCastle* data)
	{
		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_FM_EVENT_KINGOFTHECASTLE: Is a multi player only metric", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(data, "%s - PLAYSTATS_FM_EVENT_KINGOFTHECASTLE: NULL data.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if (NetworkInterface::IsGameInProgress() && data)
		{
			FmEventAmbMission_KingoftheCastle m;
			m.From(data);
			m.m_numOfKings  = data->m_numOfKings.Int;
			m.m_killsAsKing = data->m_killsAsKing.Int;
			m.m_deathsAsKing = data->m_deathsAsKing.Int;
			m.m_areaControlPoints = data->m_areaControlPoints.Int;
			APPEND_METRIC_FLUSH(m, false);
		}
	}
	
	//## Blast Corps
	class FmEventAmbMission_CriminalDamage : public FmEventAmbientMission
	{
		RL_DECLARE_METRIC(LRBLASTC, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		int m_propertyDamageValue; //Total Property Damage value per player ($)

	public:
		FmEventAmbMission_CriminalDamage() : m_propertyDamageValue(0) {;}

		virtual bool Write(RsonWriter* rw) const 
		{
			return FmEventAmbientMission::Write(rw) 
				&& rw->WriteInt("l", m_propertyDamageValue);
		}
	};

	void  CommandPlayStatsFmEventAmbMission_CriminalDamage(scrFmEventAmbMission_CriminalDamage* data)
	{
		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_FM_EVENT_CRIMINALDAMAGE: Is a multi player only metric", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(data, "%s - PLAYSTATS_FM_EVENT_CRIMINALDAMAGE: NULL data.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if (NetworkInterface::IsGameInProgress() && data)
		{
			FmEventAmbMission_CriminalDamage m;
			m.From(data);
			m.m_propertyDamageValue = data->m_propertyDamageValue.Int;
			APPEND_METRIC_FLUSH(m, false);
		}
	}

	//## Infection
	class FmEventAmbMission_Infection : public FmEventAmbientMission
	{
		RL_DECLARE_METRIC(LRINFECTION, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		int m_numPlayersInfected;         //Number of player�s infection is spread to
		int m_numInfectedPlayerKills;     //Number of infected player kills
		int m_numOfUninfectedPlayerkills; //Number of uninfected player kills

	public:
		FmEventAmbMission_Infection() : m_numPlayersInfected(0), m_numInfectedPlayerKills(0), m_numOfUninfectedPlayerkills(0) {;}

		virtual bool Write(RsonWriter* rw) const 
		{
			return FmEventAmbientMission::Write(rw) 
				&& rw->WriteInt("l", m_numPlayersInfected)
				&& rw->WriteInt("m", m_numInfectedPlayerKills)
				&& rw->WriteInt("n", m_numOfUninfectedPlayerkills);
		}
	};

	void  CommandPlayStatsFmEventAmbMission_Infection(scrFmEventAmbMission_Infection* data)
	{
		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_FM_EVENT_INFECTION: Is a multi player only metric", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(data, "%s - PLAYSTATS_FM_EVENT_INFECTION: NULL data.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if (NetworkInterface::IsGameInProgress() && data)
		{
			FmEventAmbMission_Infection m;
			m.From(data);
			m.m_numPlayersInfected         = data->m_numPlayersInfected.Int;
			m.m_numInfectedPlayerKills     = data->m_numInfectedPlayerKills.Int;
			m.m_numOfUninfectedPlayerkills = data->m_numOfUninfectedPlayerkills.Int;
			APPEND_METRIC_FLUSH(m, false);
		}
	}

	//## Competitive Urban Warfare
	class FmEventAmbMission_CompetitiveUrbanWarfare : public FmEventAmbientMission
	{
		RL_DECLARE_METRIC(LRCUW, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		int m_waveReached;    //Wave reached
		int m_killsPerPlayer; //Kills per player

	public:
		FmEventAmbMission_CompetitiveUrbanWarfare() : m_waveReached(0), m_killsPerPlayer(0) {;}

		virtual bool Write(RsonWriter* rw) const 
		{
			return FmEventAmbientMission::Write(rw) 
				&& rw->WriteInt("l", m_waveReached)
				&& rw->WriteInt("m", m_killsPerPlayer);
		}
	};

	void  CommandPlayStatsFmEventAmbMission_CompetitiveUrbanWarfare(scrFmEventAmbMission_CompetitiveUrbanWarfare* data)
	{
		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_FM_EVENT_COMPETITIVEURBANWARFARE: Is a multi player only metric", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(data, "%s - PLAYSTATS_FM_EVENT_COMPETITIVEURBANWARFARE: NULL data.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if (NetworkInterface::IsGameInProgress() && data)
		{
			FmEventAmbMission_CompetitiveUrbanWarfare m;
			m.From(data);
			m.m_waveReached    = data->m_waveReached.Int;
			m.m_killsPerPlayer = data->m_killsPerPlayer.Int;
			APPEND_METRIC_FLUSH(m, false);
		}
	}

	//## Hunt the beast:
	class FmEventAmbMission_HuntBeast : public FmEventAmbientMission
	{
		RL_DECLARE_METRIC(LRHBEAST, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		int m_landmarksCollected; //Landmarks collected (up to 10)?

	public:
		FmEventAmbMission_HuntBeast() {;}

		virtual bool Write(RsonWriter* rw) const 
		{
			return FmEventAmbientMission::Write(rw) 
				&& rw->WriteInt("l", m_landmarksCollected);
		}
	};

	void  CommandPlayStatsFmEventAmbMission_HuntBeast(scrFmEventAmbMission_HuntBeast* data)
	{
		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_FM_EVENT_HUNTBEAST: Is a multi player only metric", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(data, "%s - PLAYSTATS_FM_EVENT_HUNTBEAST: NULL data.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if (NetworkInterface::IsGameInProgress() && data)
		{
			FmEventAmbMission_HuntBeast m;
			m.From(data);
			m.m_landmarksCollected = data->m_landmarksCollected.Int;
			APPEND_METRIC_FLUSH(m, false);
		}
	}

	///////////////////////////////////////////////////////////////////////////////////////////////

	class Bw_BossWork : public MetricPlayStat
	{
		RL_DECLARE_METRIC(BWBW, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

	public:

		u64 m_localbossId;				//Local Unique BossID
		u64 m_bossId;					//Unique BossID
		u64 m_matchId;					//Unique Match Id
		int m_playerParticipated;		//Whether  or not player participated 
		int m_timeStart;				//Time work started
		int m_timeEnd;					//Time work ended
		int m_won;						//won/lost
		int m_endingReason;				//Reason for script ending - timer ran out, left session, etc
		int m_cashEarned;				//Cash Earned.
		int m_rpEarned;					//RP Earned.
		int m_bossKilled;				//Boss's killed
		int m_goonsKilled;				//Goons killed
		int m_deaths;					//Deaths
		u64 m_headerMid;				//Telemetry header MatchId

		Bw_BossWork() 
			: m_bossId(0)
			, m_playerParticipated(0)
			, m_timeStart(0)
			, m_timeEnd(0)
			, m_won(0)
			, m_endingReason(0)
			, m_cashEarned(0)
			, m_rpEarned(0)
			, m_bossKilled(0)
			, m_goonsKilled(0)
			, m_deaths(0)
			, m_localbossId(StatsInterface::GetUInt64Stat(StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID")))
			, m_matchId(0)
			, m_headerMid(0)
		{;}

		void From(scrBwBossWork* data)
		{
			const u64 bid0 = ((u64)data->m_bossId1.Int) << 32;
			const u64 bid1 = (0x00000000FFFFFFFF&(u64)data->m_bossId2.Int);

			m_bossId = bid0|bid1;
			
			const u64 mid0 = ((u64)data->m_matchId1.Int) << 32;
			const u64 mid1 = ((u64)data->m_matchId2.Int);

			m_matchId = mid0|mid1;
			scriptAssertf(m_matchId != 0, "%s: Bw_BossWork - MatchId should be different from 0", CTheScripts::GetCurrentScriptNameAndProgramCounter());

			ASSERT_ONLY(m_headerMid = static_cast<s64>(CNetworkTelemetry::GetMatchHistoryId());)
			scriptAssertf(m_headerMid == 0, "%s: Bw_BossWork - header mission Id is set", CTheScripts::GetCurrentScriptNameAndProgramCounter());

			m_playerParticipated      = data->m_playerParticipated.Int;
			m_timeStart				  = data->m_timeStart.Int;
			m_timeEnd				  = data->m_timeEnd.Int;
			m_won					  = data->m_won.Int;
			m_endingReason            = data->m_endingReason.Int;
			m_cashEarned              = data->m_cashEarned.Int;
			m_rpEarned                = data->m_rpEarned.Int;
			m_bossKilled              = data->m_bossKilled.Int;
			m_goonsKilled             = data->m_goonsKilled.Int;
			m_deaths				  = data->m_deaths.Int;
		}

		virtual bool Write(RsonWriter* rw) const
		{
			scriptDebugf1("%s: Bw_BossWork - headerMid = %" SIZETFMT "d       matchId = %" SIZETFMT "d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), m_headerMid, m_matchId);

			bool result = scriptVerify(m_matchId != 0);

			result = result && scriptVerify(m_headerMid == 0);

			bool isBoss = m_bossId != 0 && m_bossId == m_localbossId;

			return result && MetricPlayStat::Write(rw)
				&& rw->WriteInt64("bid", m_bossId)
				&& rw->WriteInt64("mid", m_matchId)
				&& rw->WriteBool("isboss", isBoss)
				&& rw->WriteInt("a", m_playerParticipated)
				&& rw->WriteInt("b", m_timeStart)
				&& rw->WriteInt("c", m_timeEnd)
				&& rw->WriteInt("d", m_won)
				&& rw->WriteInt("e", m_endingReason)
				&& rw->WriteInt("f", m_cashEarned)
				&& rw->WriteInt("g", m_rpEarned)
				&& rw->WriteInt("h", m_bossKilled)
				&& rw->WriteInt("i", m_goonsKilled)
				&& rw->WriteInt("j", m_deaths);
		}
	};

	// Boss on Boss Deathmatch
	class Bw_BossOnBossDeathMatch : public Bw_BossWork
	{
		RL_DECLARE_METRIC(BWBOBDM, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		bool m_launchedByBoss;          //Whether  or not work was launched by this Boss
		int m_invitesSent;              //Invites sent - to OTHER bosses
		int m_invitesAccepted;			//Invites accepted by other bosses

	public:
		Bw_BossOnBossDeathMatch() : Bw_BossWork() {;}

		void From(scrBwBossOnBossDeathMatch* data)
		{
			Bw_BossWork::From(data);
			m_launchedByBoss		= data->m_launchedByBoss.Int != 0;                 
			m_invitesSent			= data->m_invitesSent.Int;                 
			m_invitesAccepted		= data->m_invitesAccepted.Int;
		}

		virtual bool Write(RsonWriter* rw) const 
		{
			return Bw_BossWork::Write(rw) 
				&& rw->WriteBool("k", m_launchedByBoss)
				&& rw->WriteInt("l", m_invitesSent)
				&& rw->WriteInt("m", m_invitesAccepted);
		}
	};

	void  CommandPlayStatsBw_BossOnBossDeathMatch(scrBwBossOnBossDeathMatch* data)
	{
		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_BW_BOSSONBOSSDEATHMATCH: Is a multi player only metric", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(data, "%s - PLAYSTATS_BW_BOSSONBOSSDEATHMATCH: NULL data.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if (NetworkInterface::IsGameInProgress() && data)
		{
			Bw_BossOnBossDeathMatch m;
			m.From(data);
			if(scriptVerifyf(m.m_bossId != 0, "%s - PLAYSTATS_BW_BOSSONBOSSDEATHMATCH - BossId should be different from 0", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
			{
				APPEND_METRIC(m);
			}
		}
	}

	// Yatch Attack
	class Bw_YatchAttack : public Bw_BossWork
	{
		RL_DECLARE_METRIC(BWYA, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		bool m_launchedByBoss;          //Whether  or not work was launched by this Boss
		int m_totalCapturingScore;              //Total Capturing score
		int m_totalUnderAttackScore;			//Total Under Attack score

	public:
		Bw_YatchAttack() : Bw_BossWork() {;}

		void From(scrBwYatchAttack* data)
		{
			Bw_BossWork::From(data);
			m_launchedByBoss		= data->m_launchedByBoss.Int != 0;                
			m_totalCapturingScore		= data->m_totalCapturingScore.Int;                 
			m_totalUnderAttackScore		= data->m_totalUnderAttackScore.Int;
		}

		virtual bool Write(RsonWriter* rw) const 
		{
			return Bw_BossWork::Write(rw) 
				&& rw->WriteBool("k", m_launchedByBoss)
				&& rw->WriteInt("l", m_totalCapturingScore)
				&& rw->WriteInt("m", m_totalUnderAttackScore);
		}
	};

	void  CommandPlayStatsBw_YatchAttack(scrBwYatchAttack* data)
	{
		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_BW_YATCHATTACK: Is a multi player only metric", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(data, "%s - PLAYSTATS_BW_YATCHATTACK: NULL data.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if (NetworkInterface::IsGameInProgress() && data)
		{
			Bw_YatchAttack m;
			m.From(data);
			APPEND_METRIC(m);
		}
	}

	// Hunt the Boss
	class Bw_HuntTheBoss : public Bw_BossWork
	{
		RL_DECLARE_METRIC(BWHTB, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		bool m_launchedByBoss;          //Whether  or not work was launched by this Boss
		int m_variationPlayed;          //Variation played

	public:
		Bw_HuntTheBoss() : Bw_BossWork() {;}

		void From(scrBwHuntTheBoss* data)
		{
			Bw_BossWork::From(data);
			m_launchedByBoss		= data->m_launchedByBoss.Int != 0;                  
			m_variationPlayed		= data->m_variationPlayed.Int;
		}

		virtual bool Write(RsonWriter* rw) const 
		{
			return Bw_BossWork::Write(rw) 
				&& rw->WriteBool("k", m_launchedByBoss)
				&& rw->WriteInt("l", m_variationPlayed);
		}
	};

	void  CommandPlayStatsBw_HuntTheBoss(scrBwHuntTheBoss* data)
	{
		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_BW_HUNT_THE_BOSS: Is a multi player only metric", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(data, "%s - PLAYSTATS_BW_HUNT_THE_BOSS: NULL data.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if (NetworkInterface::IsGameInProgress() && data)
		{
			Bw_HuntTheBoss m;
			m.From(data);
			APPEND_METRIC(m);
		}
	}

	// Put out a Hit
	class Bw_PutOutAHit : public Bw_BossWork
	{
		RL_DECLARE_METRIC(BWPOAH, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		int m_hitMethodSelected;          //Hit method selected

	public:
		Bw_PutOutAHit() : Bw_BossWork() {;}

		void From(scrBwPutOutAHit* data)
		{
			Bw_BossWork::From(data);             
			m_hitMethodSelected		= data->m_hitMethodSelected.Int;
		}

		virtual bool Write(RsonWriter* rw) const 
		{
			return Bw_BossWork::Write(rw) 
				&& rw->WriteInt("k", m_hitMethodSelected);
		}
	};

	void  CommandPlayStatsBw_PutOutAHit(scrBwPutOutAHit* data)
	{
		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_BW_PUT_OUT_A_HIT: Is a multi player only metric", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(data, "%s - PLAYSTATS_BW_PUT_OUT_A_HIT: NULL data.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if (NetworkInterface::IsGameInProgress() && data)
		{
			Bw_PutOutAHit m;
			m.From(data);
			APPEND_METRIC(m);
		}
	}

	// Sightseer
	class Bw_Sightseer : public Bw_BossWork
	{
		RL_DECLARE_METRIC(BWSS, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		bool m_launchedByBoss;          //Whether  or not work was launched by this Boss
		int m_numberLocationToWin;     //Number of locations needed to win
		int m_totalLocation;	        //Total location collected

	public:
		Bw_Sightseer() : Bw_BossWork() {;}

		void From(scrBwSightseer* data)
		{
			Bw_BossWork::From(data);             
			m_launchedByBoss			= data->m_launchedByBoss.Int != 0; 
			m_numberLocationToWin		= data->m_numberLocationToWin.Int;
			m_totalLocation				= data->m_totalLocation.Int;
		}

		virtual bool Write(RsonWriter* rw) const 
		{
			return Bw_BossWork::Write(rw) 
				&& rw->WriteBool("k", m_launchedByBoss)
				&& rw->WriteInt("l", m_numberLocationToWin)
				&& rw->WriteInt("m", m_totalLocation);
		}
	};

	void  CommandPlayStatsBw_Sightseer(scrBwSightseer* data)
	{
		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_BW_SIGHTSEER: Is a multi player only metric", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(data, "%s - PLAYSTATS_BW_SIGHTSEER: NULL data.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if (NetworkInterface::IsGameInProgress() && data)
		{
			Bw_Sightseer m;
			m.From(data);
			APPEND_METRIC(m);
		}
	}

	// Assault
	class Bw_Assault : public Bw_BossWork
	{
		RL_DECLARE_METRIC(BWA, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		bool m_launchedByBoss;      //Whether  or not work was launched by this Boss
		int m_locationSelected;     //Location selected

	public:
		Bw_Assault() : Bw_BossWork() {;}

		void From(scrBwAssault* data)
		{
			Bw_BossWork::From(data);             
			m_launchedByBoss		= data->m_launchedByBoss.Int != 0; 
			m_locationSelected		= data->m_locationSelected.Int;
		}

		virtual bool Write(RsonWriter* rw) const 
		{
			return Bw_BossWork::Write(rw) 
				&& rw->WriteBool("k", m_launchedByBoss)
				&& rw->WriteInt("l", m_locationSelected);
		}
	};

	void  CommandPlayStatsBw_Assault(scrBwAssault* data)
	{
		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_BW_ASSAULT: Is a multi player only metric", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(data, "%s - PLAYSTATS_BW_ASSAULT: NULL data.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if (NetworkInterface::IsGameInProgress() && data)
		{
			Bw_Assault m;
			m.From(data);
			APPEND_METRIC(m);
		}
	}

	// BellyOfTheBeast
	class Bw_BellyOfTheBeast : public Bw_BossWork
	{
		RL_DECLARE_METRIC(BWBOTB, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		bool m_launchedByBoss;          //Whether  or not work was launched by this Boss
		int m_vehicleType;		        //Vehicle type
		int m_pickupLocation;		    //Pickup Location selected
		int m_deliveryLocation;			//Delivery Location selected

	public:
		Bw_BellyOfTheBeast() : Bw_BossWork() {;}

		void From(scrBwBellyOfTheBeast* data)
		{
			Bw_BossWork::From(data);             
			m_launchedByBoss		= data->m_launchedByBoss.Int != 0; 
			m_vehicleType			= data->m_vehicleType.Int;
			m_pickupLocation		= data->m_pickupLocation.Int;
			m_deliveryLocation		= data->m_deliveryLocation.Int;
		}

		virtual bool Write(RsonWriter* rw) const 
		{
			return Bw_BossWork::Write(rw) 
				&& rw->WriteBool("k", m_launchedByBoss)
				&& rw->WriteInt("l", m_vehicleType)
				&& rw->WriteInt("m", m_pickupLocation)
				&& rw->WriteInt("n", m_deliveryLocation);
		}
	};

	void  CommandPlayStatsBw_BellyOfTheBeast(scrBwBellyOfTheBeast* data)
	{
		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_BW_BELLY_OF_THE_BEAST: Is a multi player only metric", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(data, "%s - PLAYSTATS_BW_BELLY_OF_THE_BEAST: NULL data.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if (NetworkInterface::IsGameInProgress() && data)
		{
			Bw_BellyOfTheBeast m;
			m.From(data);
			APPEND_METRIC(m);
		}
	}


    // Headhunter
    class Bw_Headhunter : public Bw_BossWork
    {
        RL_DECLARE_METRIC(BWHEADHUNTER, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

    public:
        int m_enemiesKilled;
        int m_targetLocation;
        int m_numberOfTargetsKilled;
        bool m_launchedByBoss;          //Whether  or not work was launched by this Boss

    public:
        Bw_Headhunter() : Bw_BossWork() {;}

        void From(scrBwHeadhunter* data)
        {
            Bw_BossWork::From(data);             
            m_enemiesKilled		        = data->m_enemiesKilled.Int; 
            m_targetLocation			= data->m_targetLocation.Int;
            m_numberOfTargetsKilled		= data->m_numberOfTargetsKilled.Int;
            m_launchedByBoss		    = data->m_launchedByBoss.Int != 0; 
        }

        virtual bool Write(RsonWriter* rw) const 
        {
            return Bw_BossWork::Write(rw) 
                && rw->WriteInt("k", m_enemiesKilled)
                && rw->WriteInt("l", m_targetLocation)
                && rw->WriteInt("m", m_numberOfTargetsKilled)
                && rw->WriteBool("n", m_launchedByBoss);
        }
    };

    void  CommandPlayStatsBw_Headhunter(scrBwHeadhunter* data)
    {
        scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_BW_HEAD_HUNTER: Is a multi player only metric", CTheScripts::GetCurrentScriptNameAndProgramCounter());
        scriptAssertf(data, "%s - PLAYSTATS_BW_HEAD_HUNTER: NULL data.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

        if (NetworkInterface::IsGameInProgress() && data)
        {
            Bw_Headhunter m;
            m.From(data);
            APPEND_METRIC(m);
        }
    }

    // Fragile Goods
    class Bw_FragileGooods : public Bw_BossWork
    {
        RL_DECLARE_METRIC(BWFRAGILEG, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

    public:
        float m_destroyDisconnectedRatio; // Trailer destroyed/ total trailer disconnects
        int m_targetLocation;
        int m_startLocation;
        int m_rivalGangDestination;     // Whether AI chase fires
        bool m_launchedByBoss;          //Whether  or not work was launched by this Boss

    public:
        Bw_FragileGooods() : Bw_BossWork() {;}

        void From(scrBwFragileGoods* data)
        {
            Bw_BossWork::From(data);             
            m_destroyDisconnectedRatio	= data->m_destroyDisconnectedRatio.Float; 
            m_targetLocation			= data->m_targetLocation.Int;
            m_startLocation		        = data->m_startLocation.Int;
            m_rivalGangDestination		= data->m_rivalGangDestination.Int; 
            m_launchedByBoss		    = data->m_launchedByBoss.Int != 0; 
        }

        virtual bool Write(RsonWriter* rw) const 
        {
            return Bw_BossWork::Write(rw) 
                && rw->WriteFloat("k", m_destroyDisconnectedRatio)
                && rw->WriteInt("l", m_targetLocation)
                && rw->WriteInt("m", m_startLocation)
                && rw->WriteInt("n", m_rivalGangDestination)
                && rw->WriteBool("o", m_launchedByBoss);
        }
    };

    void  CommandPlayStatsBw_FragileGoods(scrBwFragileGoods* data)
    {
        scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_BW_FRAGILE_GOODS: Is a multi player only metric", CTheScripts::GetCurrentScriptNameAndProgramCounter());
        scriptAssertf(data, "%s - PLAYSTATS_BW_FRAGILE_GOODS: NULL data.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

        if (NetworkInterface::IsGameInProgress() && data)
        {
            Bw_FragileGooods m;
            m.From(data);
            APPEND_METRIC(m);
        }
    }

    // Air Freight
    class Bw_AirFreight : public Bw_BossWork
    {
        RL_DECLARE_METRIC(BWAIRFREIGHT, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

    public:
        int m_containerDisconnected;
        int m_targetLocation;
        int m_containerLocation;
        bool m_launchedByBoss;          //Whether  or not work was launched by this Boss

    public:
        Bw_AirFreight() : Bw_BossWork() {;}

        void From(scrBwAirFreight* data)
        {
            Bw_BossWork::From(data);             
            m_containerDisconnected	= data->m_containerDisconnected.Int; 
            m_targetLocation		= data->m_targetLocation.Int;
            m_containerLocation		= data->m_containerLocation.Int; 
            m_launchedByBoss		= data->m_launchedByBoss.Int != 0; 
        }

        virtual bool Write(RsonWriter* rw) const 
        {
            return Bw_BossWork::Write(rw) 
                && rw->WriteInt("k", m_containerDisconnected)
                && rw->WriteInt("l", m_targetLocation)
                && rw->WriteInt("m", m_containerLocation)
                && rw->WriteBool("n", m_launchedByBoss);
        }
    };

    void  CommandPlayStatsBw_AirFreight(scrBwAirFreight* data)
    {
        scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_BW_AIR_FREIGHT: Is a multi player only metric", CTheScripts::GetCurrentScriptNameAndProgramCounter());
        scriptAssertf(data, "%s - PLAYSTATS_BW_AIR_FREIGHT: NULL data.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

        if (NetworkInterface::IsGameInProgress() && data)
        {
            Bw_AirFreight m;
            m.From(data);
            APPEND_METRIC(m);
        }
    }


	class Bc_BossChallenge : public MetricPlayStat
	{
		RL_DECLARE_METRIC(BCBC, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

	public:

		u64 m_localbossId;				//Local Unique BossID
		u64 m_bossId;					//Unique BossID
		u64 m_matchId;					//Unique Match Id
		int m_playerParticipated;		//Whether  or not player participated 
		int m_timeStart;				//Time work started
		int m_timeEnd;					//Time work ended
		int m_won;						//won/lost
		int m_endingReason;				//Reason for script ending - timer ran out, left session, etc
		int m_cashEarned;				//Cash Earned.
		int m_rpEarned;					//RP Earned.
		int m_bossKilled;				//Boss's killed
		int m_goonsKilled;				//Goons killed
		int m_wagerSet;					//Wager Set

		Bc_BossChallenge() 
			: m_bossId(0)
			, m_playerParticipated(0)
			, m_timeStart(0)
			, m_timeEnd(0)
			, m_won(0)
			, m_endingReason(0)
			, m_cashEarned(0)
			, m_rpEarned(0)
			, m_bossKilled(0)
			, m_goonsKilled(0)
			, m_wagerSet(0)
			, m_localbossId(StatsInterface::GetUInt64Stat(StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID")))
			, m_matchId(0)
		{;}

		void From(scrBcBossChallenge* data)
		{
			const u64 bid0 = ((u64)data->m_bossId1.Int) << 32;
			const u64 bid1 = (0x00000000FFFFFFFF&(u64)data->m_bossId2.Int);

			m_bossId                  = bid0|bid1;
			scriptAssertf(m_bossId != 0, "%s: Bw_BossWork - BossId should be different from 0", CTheScripts::GetCurrentScriptNameAndProgramCounter());

			const u64 mid0 = ((u64)data->m_matchId1.Int) << 32;
			const u64 mid1 = ((u64)data->m_matchId2.Int);

			m_matchId                  = mid0|mid1;
			scriptAssertf(m_matchId != 0, "%s: Bc_BossChallenge - MatchId should be different from 0", CTheScripts::GetCurrentScriptNameAndProgramCounter());

			ASSERT_ONLY(u64 headerMid = static_cast<s64>(CNetworkTelemetry::GetMatchHistoryId());)
			scriptAssertf(headerMid == 0, "%s: Bc_BossChallenge - header mission Id is set", CTheScripts::GetCurrentScriptNameAndProgramCounter());

			m_playerParticipated      = data->m_playerParticipated.Int;                 
			m_timeStart				  = data->m_timeStart.Int;      
			m_timeEnd				  = data->m_timeEnd.Int; 
			m_won					  = data->m_won.Int;
			m_endingReason            = data->m_endingReason.Int;            
			m_cashEarned              = data->m_cashEarned.Int;           
			m_rpEarned                = data->m_rpEarned.Int;             
			m_bossKilled              = data->m_bossKilled.Int;         
			m_goonsKilled             = data->m_goonsKilled.Int;            
			m_wagerSet				  = data->m_wagerSet.Int;  
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = scriptVerify(m_bossId != 0) && scriptVerify(m_matchId != 0);

			u64 headerMid = static_cast<s64>(CNetworkTelemetry::GetMatchHistoryId());
			result = result && scriptVerify(headerMid == 0);

			return result && MetricPlayStat::Write(rw)
				&& rw->WriteInt64("bid", m_bossId)
				&& rw->WriteInt64("mid", m_matchId)
				&& rw->WriteBool("isboss", m_bossId == m_localbossId)
				&& rw->WriteInt("a", m_playerParticipated)
				&& rw->WriteInt("b", m_timeStart)
				&& rw->WriteInt("c", m_timeEnd)
				&& rw->WriteInt("d", m_won)
				&& rw->WriteInt("e", m_endingReason)
				&& rw->WriteInt("f", m_cashEarned)
				&& rw->WriteInt("g", m_rpEarned)
				&& rw->WriteInt("h", m_bossKilled)
				&& rw->WriteInt("i", m_goonsKilled)
				&& rw->WriteInt("j", m_wagerSet);
		}
	};


	// Carjacking
	class Bc_CarJacking: public Bc_BossChallenge
	{
		RL_DECLARE_METRIC(BCCJ, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		int m_totalScore;		        //Total Score

	public:
		Bc_CarJacking() : Bc_BossChallenge() {;}

		void From(scrBcCarJacking* data)
		{
			Bc_BossChallenge::From(data);             
			m_totalScore		= data->m_totalScore.Int; 
		}

		virtual bool Write(RsonWriter* rw) const 
		{
			return Bc_BossChallenge::Write(rw) 
				&& rw->WriteInt("k", m_totalScore);
		}
	};

	void  CommandPlayStatsBc_CarJacking(scrBcCarJacking* data)
	{
		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_BC_CAR_JACKING: Is a multi player only metric", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(data, "%s - PLAYSTATS_BC_CAR_JACKING: NULL data.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if (NetworkInterface::IsGameInProgress() && data)
		{
			Bc_CarJacking m;
			m.From(data);
			APPEND_METRIC(m);
		}
	}

	// Smash and Grab
	class Bc_SmashAndGrab: public Bc_BossChallenge
	{
		RL_DECLARE_METRIC(BCSAG, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		int m_deaths;		        //Deaths
		int m_storesHeldUp;			//number of stores held up
		int m_deliveriesMade;		//number of deliveries made
		int m_cashLost;				//cash lost (through deaths) ($)
		int m_cashDelivered;		//cash delivered ($)

	public:
		Bc_SmashAndGrab() : Bc_BossChallenge() {;}

		void From(scrBcSmashAndGrab* data)
		{
			Bc_BossChallenge::From(data);             
			m_deaths			= data->m_deaths.Int; 
			m_storesHeldUp		= data->m_storesHeldUp.Int; 
			m_deliveriesMade	= data->m_deliveriesMade.Int; 
			m_cashLost			= data->m_cashLost.Int; 
			m_cashDelivered		= data->m_cashDelivered.Int; 
		}

		virtual bool Write(RsonWriter* rw) const 
		{
			return Bc_BossChallenge::Write(rw) 
				&& rw->WriteInt("k", m_deaths)
				&& rw->WriteInt("l", m_storesHeldUp)
				&& rw->WriteInt("m", m_deliveriesMade)
				&& rw->WriteInt("n", m_cashLost)
				&& rw->WriteInt("o", m_cashDelivered);
		}
	};

	void  CommandPlayStatsBc_SmashAndGrab(scrBcSmashAndGrab* data)
	{
		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_BC_SMASH_AND_GRAB: Is a multi player only metric", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(data, "%s - PLAYSTATS_BC_SMASH_AND_GRAB: NULL data.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if (NetworkInterface::IsGameInProgress() && data)
		{
			Bc_SmashAndGrab m;
			m.From(data);
			APPEND_METRIC(m);
		}
	}

	// Protection Racket
	class Bc_ProtectionRacket: public Bc_BossChallenge
	{
		RL_DECLARE_METRIC(BCPR, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		int m_deaths;				//Deaths
		int m_cashBagsCollected;	//cash bags collected
		int m_cashBagsDelivered;	//cash bags delivered
		int m_pickupQuadrant;		//Pickup Quadrant
		int m_deliveryQuadrant;		//Delivery Quadrant

	public:
		Bc_ProtectionRacket() : Bc_BossChallenge() {;}

		void From(scrBcProtectionRacket* data)
		{
			Bc_BossChallenge::From(data);             
			m_deaths				= data->m_deaths.Int; 
			m_cashBagsCollected		= data->m_cashBagsCollected.Int; 
			m_cashBagsDelivered		= data->m_cashBagsDelivered.Int; 
			m_pickupQuadrant		= data->m_pickupQuadrant.Int; 
			m_deliveryQuadrant		= data->m_deliveryQuadrant.Int; 
		}

		virtual bool Write(RsonWriter* rw) const 
		{
			return Bc_BossChallenge::Write(rw) 
				&& rw->WriteInt("k", m_deaths)
				&& rw->WriteInt("l", m_cashBagsCollected)
				&& rw->WriteInt("m", m_cashBagsDelivered)
				&& rw->WriteInt("n", m_pickupQuadrant)
				&& rw->WriteInt("o", m_deliveryQuadrant);
		}
	};

	void  CommandPlayStatsBc_ProtectionRacket(scrBcProtectionRacket* data)
	{
		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_BC_PROTECTION_RACKET: Is a multi player only metric", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(data, "%s - PLAYSTATS_BC_PROTECTION_RACKET: NULL data.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if (NetworkInterface::IsGameInProgress() && data)
		{
			Bc_ProtectionRacket m;
			m.From(data);
			APPEND_METRIC(m);
		}
	}

	// Protection Racket
	class Bc_MostWanted: public Bc_BossChallenge
	{
		RL_DECLARE_METRIC(BCMW, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		int m_deaths;			//Deaths
		int m_timeLasted;		//Time lasted with wanted level

	public:
		Bc_MostWanted() : Bc_BossChallenge() {;}

		void From(scrBcMostWanted* data)
		{
			Bc_BossChallenge::From(data);             
			m_deaths			= data->m_deaths.Int; 
			m_timeLasted		= data->m_timeLasted.Int; 
		}

		virtual bool Write(RsonWriter* rw) const 
		{
			return Bc_BossChallenge::Write(rw) 
				&& rw->WriteInt("k", m_deaths)
				&& rw->WriteInt("l", m_timeLasted);
		}
	};

	void  CommandPlayStatsBc_MostWanted(scrBcMostWanted* data)
	{
		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_BC_MOST_WANTED: Is a multi player only metric", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(data, "%s - PLAYSTATS_BC_MOST_WANTED: NULL data.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if (NetworkInterface::IsGameInProgress() && data)
		{
			Bc_MostWanted m;
			m.From(data);
			APPEND_METRIC(m);
		}
	}

	// Finders Keepers
	class Bc_FindersKeepers: public Bc_BossChallenge
	{
		RL_DECLARE_METRIC(BCFK, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		int m_deaths;			//Deaths
		int m_packagesCollected;	//number of packages collected
		int m_quadrantSelected;		// Quadrant selected

	public:
		Bc_FindersKeepers() : Bc_BossChallenge() {;}

		void From(scrBcFindersKeepers* data)
		{
			Bc_BossChallenge::From(data);             
			m_deaths				= data->m_deaths.Int; 
			m_packagesCollected		= data->m_packagesCollected.Int; 
			m_quadrantSelected      = data->m_quadrantSelected.Int;
		}

		virtual bool Write(RsonWriter* rw) const 
		{
			return Bc_BossChallenge::Write(rw) 
				&& rw->WriteInt("k", m_deaths)
				&& rw->WriteInt("l", m_packagesCollected)
				&& rw->WriteInt("m", m_quadrantSelected);
		}
	};

	void  CommandPlayStatsBc_FindersKeepers(scrBcFindersKeepers* data)
	{
		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_BC_FINDERS_KEEPERS: Is a multi player only metric", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(data, "%s - PLAYSTATS_BC_FINDERS_KEEPERS: NULL data.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if (NetworkInterface::IsGameInProgress() && data)
		{
			Bc_FindersKeepers m;
			m.From(data);
			APPEND_METRIC(m);
		}
	}

	// Running Riot
	class Bc_RunningRiot: public Bc_BossChallenge
	{
		RL_DECLARE_METRIC(BCRR, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		int m_deaths;			//Deaths
		int m_numberOfKills;	//number of kills

	public:
		Bc_RunningRiot() : Bc_BossChallenge() {;}

		void From(scrBcRunningRiot* data)
		{
			Bc_BossChallenge::From(data);             
			m_deaths				= data->m_deaths.Int; 
			m_numberOfKills		= data->m_numberOfKills.Int; 
		}

		virtual bool Write(RsonWriter* rw) const 
		{
			return Bc_BossChallenge::Write(rw) 
				&& rw->WriteInt("k", m_deaths)
				&& rw->WriteInt("l", m_numberOfKills);
		}
	};

	void  CommandPlayStatsBc_RunningRiot(scrBcRunningRiot* data)
	{
		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_BC_RUNNING_RIOT: Is a multi player only metric", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(data, "%s - PLAYSTATS_BC_RUNNING_RIOT: NULL data.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if (NetworkInterface::IsGameInProgress() && data)
		{
			Bc_RunningRiot m;
			m.From(data);
			APPEND_METRIC(m);
		}
	}

	// Point To Point
	class Bc_PointToPoint: public Bc_BossChallenge
	{
		RL_DECLARE_METRIC(BCPTP, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		int m_deaths;			//Deaths

	public:
		Bc_PointToPoint() : Bc_BossChallenge() {;}

		void From(scrBcPointToPoint* data)
		{
			Bc_BossChallenge::From(data);             
			m_deaths				= data->m_deaths.Int; 
		}

		virtual bool Write(RsonWriter* rw) const 
		{
			return Bc_BossChallenge::Write(rw) 
				&& rw->WriteInt("k", m_deaths);
		}
	};

	void  CommandPlayStatsBc_PointToPoint(scrBcPointToPoint* data)
	{
		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_BC_POINT_TO_POINT: Is a multi player only metric", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(data, "%s - PLAYSTATS_BC_POINT_TO_POINT: NULL data.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if (NetworkInterface::IsGameInProgress() && data)
		{
			Bc_PointToPoint m;
			m.From(data);
			APPEND_METRIC(m);
		}
	}

    // Cashing
    class Bc_Cashing: public Bc_BossChallenge
    {
        RL_DECLARE_METRIC(BCCASHING, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

    public:
        int m_machinesHacked;
        float m_failedMinigamesRatio;
        int m_wantedLevelReached;

    public:
        Bc_Cashing() : Bc_BossChallenge() {;}

        void From(scrBcCashing* data)
        {
            Bc_BossChallenge::From(data);             
            m_machinesHacked			= data->m_machinesHacked.Int; 
            m_failedMinigamesRatio		= data->m_failedMinigamesRatio.Float; 
            m_wantedLevelReached		= data->m_wantedLevelReached.Int; 
        }

        virtual bool Write(RsonWriter* rw) const 
        {
            return Bc_BossChallenge::Write(rw) 
                && rw->WriteInt("k", m_machinesHacked) 
                && rw->WriteFloat("l", m_failedMinigamesRatio) 
                && rw->WriteInt("m", m_wantedLevelReached);
        }
    };

    void  CommandPlayStatsBc_Cashing(scrBcCashing* data)
    {
        scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_BC_CASHING: Is a multi player only metric", CTheScripts::GetCurrentScriptNameAndProgramCounter());
        scriptAssertf(data, "%s - PLAYSTATS_BC_CASHING: NULL data.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

        if (NetworkInterface::IsGameInProgress() && data)
        {
            Bc_Cashing m;
            m.From(data);
            APPEND_METRIC(m);
        }
    }

    // Salvage
    class Bc_Salvage: public Bc_BossChallenge
    {
        RL_DECLARE_METRIC(BCSALVAGE, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

    public:
        int m_checkpointsCollected;
        int m_rebreathersCollected;
        int m_deaths;			//Deaths

    public:
        Bc_Salvage() : Bc_BossChallenge() {;}

        void From(scrBcSalvage* data)
        {
            Bc_BossChallenge::From(data);             
            m_checkpointsCollected		= data->m_checkpointsCollected.Int; 
            m_rebreathersCollected		= data->m_rebreathersCollected.Int; 
            m_deaths				    = data->m_deaths.Int; 
        }

        virtual bool Write(RsonWriter* rw) const 
        {
            return Bc_BossChallenge::Write(rw) 
                && rw->WriteInt("k", m_checkpointsCollected) 
                && rw->WriteInt("l", m_rebreathersCollected) 
                && rw->WriteInt("m", m_deaths);
        }
    };

    void  CommandPlayStatsBc_Salvage(scrBcSalvage* data)
    {
        scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_BC_SALVAGE: Is a multi player only metric", CTheScripts::GetCurrentScriptNameAndProgramCounter());
        scriptAssertf(data, "%s - PLAYSTATS_BC_SALVAGE: NULL data.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

        if (NetworkInterface::IsGameInProgress() && data)
        {
            Bc_Salvage m;
            m.From(data);
            APPEND_METRIC(m);
        }
    }

    // Exec Telemtry



    class MetricSpentPiCustomLoadout : public MetricPlayStat
    {
        RL_DECLARE_METRIC(PI_LOADOUT, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

    public:
        MetricSpentPiCustomLoadout(u32 value) 
            : MetricPlayStat()
            , m_value(value)
        {	
        }

        virtual bool Write(RsonWriter* rw) const
        {
            return MetricPlayStat::Write(rw)
                && rw->WriteUns("v", m_value);
        }

        u32 m_value;
    };

    void CommandPlayStatsPiCustomLoadout(int value)
    {
        // we dont want to spam the server
        static u64 s_lastMetricSent = 0;
        static const u64 SEND_PERIOD = 300; // 5 minutes
        static int s_metricSentCount = 0;
        static const int MAX_PER_PERIOD = 5;

        if(s_lastMetricSent + SEND_PERIOD <= rlGetPosixTime())
        {
            s_metricSentCount = 0;
            s_lastMetricSent = rlGetPosixTime();
        }
        if(s_metricSentCount < MAX_PER_PERIOD)
        {
            s_metricSentCount++;
            MetricSpentPiCustomLoadout m(value);
            APPEND_METRIC(m);
        }
    }

    class ContrabandMission : public MetricPlayStat
    {
        RL_DECLARE_METRIC(CONTRABAND, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

    public:

        u64 m_localbossId;				//Local Unique BossID
        u64 m_bossId;					//Unique BossID
        u64 m_matchId;					//Unique Match Id
        int m_playerParticipated;		//Whether  or not player participated 
        int m_timeStart;				//Time work started
        int m_timeEnd;					//Time work ended
        int m_won;						//won/lost
        int m_endingReason;				//Reason for script ending - timer ran out, left session, etc
        int m_cashEarned;				//Cash Earned.
        int m_rpEarned;					//RP Earned.
        int m_location;				    //Location selected
        int m_type;                     //Type of Mission Launched (small medium, large)
        int m_subtype;                  //Subtype of Mission Launched 
        int m_warehouseOwned;			//What Warehouses the Boss owns (bit set)
        int m_warehouseOwnedCount;		//Number of Warehouses the Boss owns
        int m_failureReason;			//Failure reason
        u64 m_headerMid;				//Telemetry header MatchId
		int m_missionId;				//Mission Id

        ContrabandMission() 
            : m_bossId(0)
            , m_playerParticipated(0)
            , m_timeStart(0)
            , m_timeEnd(0)
            , m_won(0)
            , m_endingReason(0)
            , m_cashEarned(0)
            , m_rpEarned(0)
            , m_location(0)
            , m_type(0)
            , m_subtype(0)
            , m_warehouseOwned(0)
            , m_warehouseOwnedCount(0)
            , m_failureReason(0)
            , m_localbossId(StatsInterface::GetUInt64Stat(StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID")))
            , m_matchId(0)
            , m_headerMid(0)
			, m_missionId(0)
        {;}

        void From(scrContrabandMission* data)
        {
            const u64 bid0 = ((u64)data->m_bossId1.Int) << 32;
            const u64 bid1 = (0x00000000FFFFFFFF&(u64)data->m_bossId2.Int);

            m_bossId                  = bid0|bid1;
            scriptAssertf(m_bossId != 0, "%s: Bw_BossWork - BossId should be different from 0", CTheScripts::GetCurrentScriptNameAndProgramCounter());

            const u64 mid0 = ((u64)data->m_matchId1.Int) << 32;
            const u64 mid1 = ((u64)data->m_matchId2.Int);

            m_matchId = mid0|mid1;
            scriptAssertf(m_matchId != 0, "%s: Bw_BossWork - MatchId should be different from 0", CTheScripts::GetCurrentScriptNameAndProgramCounter());

            ASSERT_ONLY(m_headerMid = static_cast<s64>(CNetworkTelemetry::GetMatchHistoryId());)
            scriptAssertf(m_headerMid == 0, "%s: Bw_BossWork - header mission Id is set", CTheScripts::GetCurrentScriptNameAndProgramCounter());

            m_playerParticipated      = data->m_playerParticipated.Int;
            m_timeStart				  = data->m_timeStart.Int;
            m_timeEnd				  = data->m_timeEnd.Int;
            m_won					  = data->m_won.Int;
            m_endingReason            = data->m_endingReason.Int;
            m_cashEarned              = data->m_cashEarned.Int;
            m_rpEarned                = data->m_rpEarned.Int;
            m_location                = data->m_location.Int;
            m_type                    = data->m_type.Int;
            m_subtype                 = data->m_subtype.Int;
            m_warehouseOwned          = data->m_warehouseOwned.Int;
            m_warehouseOwnedCount     = data->m_warehouseOwnedCount.Int;
			m_failureReason           = data->m_failureReason.Int;
			m_missionId               = data->m_missionId.Int;
        }

        virtual bool Write(RsonWriter* rw) const
        {
            bool result = scriptVerify(m_bossId != 0);
            
            result = result && scriptVerify(m_headerMid == 0);

            return result && MetricPlayStat::Write(rw)
                && rw->WriteInt64("bid", m_bossId)
                && rw->WriteBool("isboss", m_bossId == m_localbossId)
                && rw->WriteInt64("mid", m_matchId)
                && rw->WriteInt("a", m_playerParticipated)
                && rw->WriteInt("b", m_timeStart)
                && rw->WriteInt("c", m_timeEnd)
                && rw->WriteInt("d", m_won)
                && rw->WriteInt("e", m_endingReason)
                && rw->WriteInt("f", m_cashEarned)
                && rw->WriteInt("g", m_rpEarned)
                && rw->WriteInt("h", m_location)
                && rw->WriteInt("i", m_type)
                && rw->WriteInt("ii", m_subtype)
                && rw->WriteInt("j", m_warehouseOwned)
                && rw->WriteInt("jj", m_warehouseOwnedCount)
                && rw->WriteInt("k", m_failureReason)
				&& rw->WriteInt("misid", m_missionId);
        }
    };



    class BuyContrabandMission : public ContrabandMission 
    {
        RL_DECLARE_METRIC(BUY_CONTRABAND, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

    public:

        int m_startVolume;			        //Contraband volume at start of mission
        int m_endVolume;			        //Contraband volume at end of mission
        int m_startWarehouseCapacity;	    //Warehouse capacity % at start of mission
        int m_endWarehouseCapacity;	        //Warehouse capacity % at end of mission
        int m_contrabandDestroyed;			//Amount of contraband destroyed
		int m_contrabandDelivered;			//Amount of contraband delivered
		int m_FromHackerTruck;				//whether the mission was launched from the Hacker Truck
		int m_properties;
		int m_properties2;


        BuyContrabandMission() 
            : ContrabandMission()
            , m_startVolume(0)
            , m_endVolume(0)
            , m_startWarehouseCapacity(0)
            , m_endWarehouseCapacity(0)
            , m_contrabandDestroyed(0)
            , m_contrabandDelivered(0)
			, m_FromHackerTruck(0)
			, m_properties(0)
			, m_properties2(0)
        {;}

        void From(scrBuyContrabandMission* data)
        {
            ContrabandMission::From(data);
            m_startVolume               = data->m_startVolume.Int;
            m_endVolume                 = data->m_endVolume.Int;
            m_startWarehouseCapacity    = data->m_startWarehouseCapacity.Int;
            m_endWarehouseCapacity      = data->m_endWarehouseCapacity.Int;
            m_contrabandDestroyed       = data->m_contrabandDestroyed.Int;
			m_contrabandDelivered		= data->m_contrabandDelivered.Int;
			m_FromHackerTruck = data->m_FromHackerTruck.Int;
			m_properties = data->m_properties.Int;
			m_properties2 = data->m_properties2.Int;
        }

        virtual bool Write(RsonWriter* rw) const
        {
            return ContrabandMission::Write(rw)
                && rw->WriteInt("l", m_startVolume)
                && rw->WriteInt("m", m_endVolume)
                && rw->WriteInt("n", m_startWarehouseCapacity)
                && rw->WriteInt("o", m_endWarehouseCapacity)
				&& rw->WriteInt("y", m_contrabandDestroyed)
				&& rw->WriteInt("z", m_contrabandDelivered)
				&& rw->WriteBool("aa", (m_FromHackerTruck == 1))
				&& rw->WriteInt("ab", m_properties)
				&& rw->WriteInt("ac", m_properties2);
        }
    };

    void  CommandPlayStatsBuyContrabandMission(scrBuyContrabandMission* data)
    {
        scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_BUY_CONTRABAND_MISSION: Is a multi player only metric", CTheScripts::GetCurrentScriptNameAndProgramCounter());
        scriptAssertf(data, "%s - PLAYSTATS_BUY_CONTRABAND_MISSION: NULL data.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

        if (NetworkInterface::IsGameInProgress() && data)
        {
            BuyContrabandMission m;
            m.From(data);
            APPEND_METRIC(m);
        }
    }


    class SellContrabandMission : public BuyContrabandMission 
    {
        RL_DECLARE_METRIC(SELL_CONTRABAND, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

    public:

        int m_dropOffLocation;

        SellContrabandMission() 
            : BuyContrabandMission()
            , m_dropOffLocation(0)
        {;}

        void From(scrSellContrabandMission* data)
        {
            BuyContrabandMission::From(data);
            m_dropOffLocation               = data->m_dropOffLocation.Int;
        }

        virtual bool Write(RsonWriter* rw) const
        {
            return BuyContrabandMission::Write(rw)
                && rw->WriteInt("p", m_dropOffLocation);
        }
    };

    void  CommandPlayStatsSellContrabandMission(scrSellContrabandMission* data)
    {
        scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_SELL_CONTRABAND_MISSION: Is a multi player only metric", CTheScripts::GetCurrentScriptNameAndProgramCounter());
        scriptAssertf(data, "%s - PLAYSTATS_SELL_CONTRABAND_MISSION: NULL data.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

        if (NetworkInterface::IsGameInProgress() && data)
        {
            SellContrabandMission m;
            m.From(data);
            APPEND_METRIC(m);
        }
    }


    class DefendContrabandMission : public BuyContrabandMission
    {
        RL_DECLARE_METRIC(DEFEND_CONTRABAND, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

    public:

        int m_enemiesKilled;

        DefendContrabandMission() 
            : BuyContrabandMission()
            , m_enemiesKilled(0)
        {;}

        void From(scrDefendContrabandMission* data)
        {
            BuyContrabandMission::From(data);
            m_enemiesKilled               = data->m_enemiesKilled.Int;
        }

        virtual bool Write(RsonWriter* rw) const
        {
            return BuyContrabandMission::Write(rw)
                && rw->WriteInt("p", m_enemiesKilled);
        }
    };

    void  CommandPlayStatsDefendContrabandMission(scrDefendContrabandMission* data)
    {
        scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_DEFEND_CONTRABAND_MISSION: Is a multi player only metric", CTheScripts::GetCurrentScriptNameAndProgramCounter());
        scriptAssertf(data, "%s - PLAYSTATS_DEFEND_CONTRABAND_MISSION: NULL data.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

        if (NetworkInterface::IsGameInProgress() && data)
        {
            DefendContrabandMission m;
            m.From(data);
            APPEND_METRIC(m);
        }
    }

    class RecoverContrabandMission : public DefendContrabandMission
    {
        RL_DECLARE_METRIC(RECOVER_CONTRABAND, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

    public:

        int m_lostDestroyedOrRecovered;

        RecoverContrabandMission() 
            : DefendContrabandMission()
            , m_lostDestroyedOrRecovered(0)
        {;}

        void From(scrRecoverContrabandMission* data)
        {
            DefendContrabandMission::From(data);
            m_lostDestroyedOrRecovered               = data->m_lostDestroyedOrRecovered.Int;
        }

        virtual bool Write(RsonWriter* rw) const
        {
            return DefendContrabandMission::Write(rw)
                && rw->WriteInt("q", m_lostDestroyedOrRecovered);
        }
    };

    void  CommandPlayStatsRecoverContrabandMission(scrRecoverContrabandMission* data)
    {
        scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_RECOVER_CONTRABAND_MISSION: Is a multi player only metric", CTheScripts::GetCurrentScriptNameAndProgramCounter());
        scriptAssertf(data, "%s - PLAYSTATS_RECOVER_CONTRABAND_MISSION: NULL data.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

        if (NetworkInterface::IsGameInProgress() && data)
        {
            RecoverContrabandMission m;
            m.From(data);
            APPEND_METRIC(m);
        }
    }


    class HitContrabandDestroyLimit : public MetricPlayStat
    {
        RL_DECLARE_METRIC(DESTROY_LIMIT, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

    public:
        
        HitContrabandDestroyLimit(int amount) 
            : MetricPlayStat()
            , m_amount(amount)
        {;}

        virtual bool Write(RsonWriter* rw) const
        {
            return MetricPlayStat::Write(rw)
                && rw->WriteInt("a", m_amount);
        }

    private:

        int m_amount;
    };

    void  CommandPlayStatsHitContrabandDestroyLimit(int amount)
    {
        scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_HIT_CONTRABAND_DESTROY_LIMIT: Is a multi player only metric", CTheScripts::GetCurrentScriptNameAndProgramCounter());

        if (NetworkInterface::IsGameInProgress())
        {
            HitContrabandDestroyLimit m(amount);
            APPEND_METRIC(m);
        }
    }


	///////////////////////////////////////////////////////////////////////////////////////////////

	class PIMenuHideSettings : public MetricPlayStat
	{
		RL_DECLARE_METRIC(PIMENU, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		int m_bitsetopt1;
		int m_bitsetopt2;
		int m_bitsetopt3;
		int m_bitsetopt4;

	public:
		PIMenuHideSettings() : m_bitsetopt1(0) , m_bitsetopt2(0)  , m_bitsetopt3(0) , m_bitsetopt4(0) {;}

		void From(scrPIMenuHideSettings* data)
		{
			m_bitsetopt1 = data->m_bitsetopt1.Int;                 
			m_bitsetopt2 = data->m_bitsetopt2.Int;                 
			m_bitsetopt3 = data->m_bitsetopt3.Int;
			m_bitsetopt4 = data->m_bitsetopt4.Int;
		}

		virtual bool Write(RsonWriter* rw) const
		{
			return MetricPlayStat::Write(rw)
				&& rw->WriteInt("opt1", m_bitsetopt1)
				&& rw->WriteInt("opt2", m_bitsetopt2)
				&& rw->WriteInt("opt3", m_bitsetopt3)
				&& rw->WriteInt("opt4", m_bitsetopt4);
		}
	};

	void  CommandPlayStatsPIMenuHideSettings(scrPIMenuHideSettings* data)
	{
		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_PIMENU_HIDE_OPTIONS: Is a multi player only metric", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(data, "%s - PLAYSTATS_PIMENU_HIDE_OPTIONS: NULL data.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if (NetworkInterface::IsGameInProgress() && data)
		{
			PIMenuHideSettings m;
			m.From(data);
			APPEND_METRIC_FLUSH(m, false);
		}
	}

	class MetricACTIVITY_DONE : public MetricPlayerCoords
	{
		RL_DECLARE_METRIC(ACTIVITY_DONE, TELEMETRY_CHANNEL_AMBIENT, LOGLEVEL_HIGH_PRIORITY);

	private:
		u32 m_location;
		u32 m_activity;

	public:
		explicit MetricACTIVITY_DONE(const u32 location, const u32 activity) : m_location(location), m_activity(activity) { }

		virtual bool Write(RsonWriter* rw) const
		{
			return MetricPlayerCoords::Write(rw)
				&& rw->WriteUns("location", m_location)
				&& rw->WriteUns("activity", m_activity);
		}
	};

	void  CommandPlayStatsActivity(const int location, const int activity)
	{
		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_ACTIVITY_DONE: Is a multi player only metric", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(NetworkInterface::IsInFreeMode(), "%s - PLAYSTATS_ACTIVITY_DONE: not in freemode", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if (NetworkInterface::IsGameInProgress() && NetworkInterface::IsInFreeMode())
		{
			CNetworkTelemetry::AppendMetric(MetricACTIVITY_DONE((u32)location, (u32)activity));
		}
	}


	class MetricLEAVE_JOBCHAIN : public MetricPlayStat
	{
		RL_DECLARE_METRIC(LEAVE_JOBCHAIN, TELEMETRY_CHANNEL_AMBIENT, LOGLEVEL_HIGH_PRIORITY);

	private:
		u64 m_chainid;
		u64 m_matchid;
		int m_leavereason;

	public:
		explicit MetricLEAVE_JOBCHAIN(const u64 chainid, const u64 matchid, const int leavereason) : m_chainid(chainid), m_matchid(matchid), m_leavereason(leavereason) { }

		virtual bool Write(RsonWriter* rw) const
		{
			return MetricPlayStat::Write(rw)
				&& rw->WriteInt64("cid", m_chainid)
				&& rw->WriteInt64("mid", m_matchid)
				&& rw->WriteInt("r", m_leavereason);
		}
	};

	void   CommandPlayStatsLeaveJobChain(int macaddresshash, int posixtime, int currmacaddresshash, int currposixtime, int leavereason)
	{
		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_LEAVE_JOB_CHAIN: Is a multi player only metric", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(NetworkInterface::IsInFreeMode(), "%s - PLAYSTATS_LEAVE_JOB_CHAIN: not in freemode", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if (NetworkInterface::IsGameInProgress() && NetworkInterface::IsInFreeMode())
		{
			u64 chainmatchid = ((u64)macaddresshash) << 32;
			chainmatchid |= posixtime & 0xFFFFFFFF;

			u64 currmatchid = ((u64)currmacaddresshash) << 32;
			currmatchid |= currposixtime & 0xFFFFFFFF;

			CNetworkTelemetry::AppendMetric(MetricLEAVE_JOBCHAIN(chainmatchid, currmatchid, leavereason));
		}
	}

	// PURPOSE
	//     Registers with the playstats the fact that a mission has started with 
	//     a name string (the string that points into the text file).
	void  CommandPlayStatsMissionStarted(const char *pMissionName, const int variant, const int checkpoint, const bool replaying)
	{
		scriptAssertf(!NetworkInterface::IsNetworkOpen(), "%s - PLAYSTATS_MISSION_STARTED: Is a single player only metric", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!NetworkInterface::IsNetworkOpen())
		{
			scriptAssertf(variant >= 0, "%s - PLAYSTATS_MISSION_STARTED: invalid variant=%d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), variant);
			scriptAssertf(checkpoint >= 0, "%s - PLAYSTATS_MISSION_STARTED: invalid checkpoint=%d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), checkpoint);
			NOTFINAL_ONLY( CNetworkDebugTelemetry::AppendMetricsMissionStarted(pMissionName); )
			CNetworkTelemetry::MissionStarted(pMissionName, variant, checkpoint, replaying);

			bool updateMissionPresence = true;
			Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("UPDATE_MISSION_NAME_PRESENCE", 0x1a44cfdb), updateMissionPresence);
			if (updateMissionPresence)
				NetworkSCPresenceUtil::UpdateCurrentMission(pMissionName);

			s_cashStart = MoneyInterface::GetVCWalletBalance() + MoneyInterface::GetVCBankBalance();
		}
	}

	void  CommandPlayStatsRandomMissionDone(const char *pMissionName, const int outcome, const int timespent, const int attempts)
	{
		scriptAssertf(!NetworkInterface::IsNetworkOpen(), "%s - PLAYSTATS_RANDOM_MISSION_DONE: Is a single player only metric", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!NetworkInterface::IsNetworkOpen())
		{
			scriptAssertf(outcome >= 0, "%s - PLAYSTATS_RANDOM_MISSION_DONE: invalid outcome=%d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), outcome);
			CNetworkTelemetry::RandomMissionDone(pMissionName, outcome, timespent, attempts);
		}
	}

	void  CommandPlayStatsMissionOver(const char *pMissionName, const int variant, const int checkpoint, const bool failed, const bool canceled, const bool skipped)
	{
		scriptAssertf(!NetworkInterface::IsNetworkOpen(), "%s - PLAYSTATS_MISSION_OVER: Is a single player only metric", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!NetworkInterface::IsNetworkOpen())
		{
			scriptAssertf(variant >= 0, "%s - PLAYSTATS_MISSION_OVER: invalid variant=%d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), variant);
			scriptAssertf(checkpoint >= 0, "%s - PLAYSTATS_MISSION_OVER: invalid checkpoint=%d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), checkpoint);
			NOTFINAL_ONLY( CNetworkDebugTelemetry::AppendMetricsMissionOver(pMissionName); )
			CNetworkTelemetry::MissionOver(pMissionName, variant, checkpoint, failed, canceled, skipped);

			bool updateMissionPresence = true;
			Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("UPDATE_MISSION_NAME_PRESENCE", 0x1a44cfdb), updateMissionPresence);
			if (updateMissionPresence)
				NetworkSCPresenceUtil::UpdateCurrentMission(NULL);
		}
	}

	void  CommandPlayStatsMissionCheckpoint(const char *pMissionName, const int variant, const int previousCheckpoint, const int checkpoint)
	{
		scriptAssertf(!NetworkInterface::IsNetworkOpen(), "%s - PLAYSTATS_MISSION_CHECKPOINT: Is a single player only metric", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!NetworkInterface::IsNetworkOpen())
		{
			scriptAssertf(variant >= 0, "%s - PLAYSTATS_MISSION_CHECKPOINT: invalid variant=%d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), variant);
			scriptAssertf(previousCheckpoint >= 0, "%s - PLAYSTATS_MISSION_CHECKPOINT: invalid previousCheckpoint=%d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), previousCheckpoint);
			scriptAssertf(checkpoint >= 0, "%s - PLAYSTATS_MISSION_CHECKPOINT: invalid checkpoint=%d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), checkpoint);
			CNetworkTelemetry::MissionCheckpoint(pMissionName, variant, previousCheckpoint, checkpoint);
		}
	}

	void  CommandPlayStatsRosBet(const int amount, const int activity, const int playerIndex, float commission)
	{
		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_ROS_BET: Not in multiplayer game", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (NetworkInterface::IsGameInProgress())
		{
			CPed* playerPed = FindPlayerPed();
			scriptAssertf(playerPed, "%s - PLAYSTATS_ROS_BET: No Player ped", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			scriptAssertf(amount > 0, "%s - PLAYSTATS_ROS_BET: Invalid amount %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), amount);
			if (playerPed && amount > 0)
			{
				CNetGamePlayer* pPlayer = CTheScripts::FindNetworkPlayer(playerIndex);
				scriptAssertf(pPlayer && pPlayer->GetPlayerPed(), "%s - PLAYSTATS_ROS_BET: Invalid player Index %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), playerIndex);
				if (pPlayer && pPlayer->GetPlayerPed())
				{
					CNetworkTelemetry::MakeRosBet(playerPed, pPlayer->GetPlayerPed(), amount, activity, commission);
				}
			}
		}
	}

	void  CommandPlayStatsRaceCheckpoint(const int vehicleId
										,const int checkpointId
										,const int racePos
										,const int raceTime
										,const int checkpointTime)
	{
		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_RACE_CHECKPOINT: Not in multiplayer game", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (NetworkInterface::IsGameInProgress())
		{
			scriptAssertf(checkpointId>=0, "%s - PLAYSTATS_RACE_CHECKPOINT: Invalid value for checkpointId=%d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), checkpointId);
			scriptAssertf(racePos>=0, "%s - PLAYSTATS_RACE_CHECKPOINT: Invalid value for racePos=%d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), racePos);
			scriptAssertf(raceTime>=0, "%s - PLAYSTATS_RACE_CHECKPOINT: Invalid value for raceTime=%d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), raceTime);
			scriptAssertf(checkpointTime>=0, "%s - PLAYSTATS_RACE_CHECKPOINT: Invalid value for checkpointTime=%d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), checkpointTime);

			CNetworkTelemetry::RaceCheckpoint((u32)vehicleId,(u32)checkpointId,(u32)racePos,(u32)raceTime,(u32)checkpointTime);
		}
	}

	void  CommandPlayStatsPostMatchBlob(const int xpEarned
										,const int cashEarned
										,const int highestKillStreak
										,const int numberOfKills
										,const int numberOfDeaths
										,const int numberOfSuicides
										,const int scoreboardRanking
										,const int teamId
										,const int crewId
										,const int vehicleId
										,const int /*numberOfHeadshots*/
										,const int cashEarnedFromBets
										,const int cashAtStart
										,const int cashAtEnd
										,const int betWon
										,const int survivedWave)
	{
		scriptAssertf(0, "%s - PLAYSTATS_POST_MATCH_BLOB: Commands deprecated - Use command instead PLAYSTATS_JOB_ACTIVITY_END", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_POST_MATCH_BLOB: Not in multiplayer game", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		scriptAssertf(         xpEarned>=0, "%s - PLAYSTATS_POST_MATCH_BLOB: Invalid value for xpEarned=%d",          CTheScripts::GetCurrentScriptNameAndProgramCounter(), xpEarned);
		scriptAssertf(highestKillStreak>=0, "%s - PLAYSTATS_POST_MATCH_BLOB: Invalid value for highestKillStreak=%d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), highestKillStreak);
		scriptAssertf(    numberOfKills>=0, "%s - PLAYSTATS_POST_MATCH_BLOB: Invalid value for numberOfKills=%d",     CTheScripts::GetCurrentScriptNameAndProgramCounter(), numberOfKills);
		scriptAssertf(   numberOfDeaths>=0, "%s - PLAYSTATS_POST_MATCH_BLOB: Invalid value for numberOfDeaths=%d",    CTheScripts::GetCurrentScriptNameAndProgramCounter(), numberOfDeaths);
		scriptAssertf( numberOfSuicides>=0, "%s - PLAYSTATS_POST_MATCH_BLOB: Invalid value for numberOfSuicides=%d",  CTheScripts::GetCurrentScriptNameAndProgramCounter(), numberOfSuicides);
		scriptAssertf(scoreboardRanking>=0, "%s - PLAYSTATS_POST_MATCH_BLOB: Invalid value for scoreboardRanking=%d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), scoreboardRanking);
		scriptAssertf(           teamId>=-1, "%s - PLAYSTATS_POST_MATCH_BLOB: Invalid value for teamId=%d",            CTheScripts::GetCurrentScriptNameAndProgramCounter(), teamId);
		scriptAssertf(           crewId>=0, "%s - PLAYSTATS_POST_MATCH_BLOB: Invalid value for crewId=%d",            CTheScripts::GetCurrentScriptNameAndProgramCounter(), crewId);

		if (NetworkInterface::IsGameInProgress())
		{
			scriptAssertf(xpEarned>=0, "%s - PLAYSTATS_POST_MATCH_BLOB: Invalid value for xpEarned=%d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), xpEarned);
			scriptAssertf(highestKillStreak>=0, "%s - PLAYSTATS_POST_MATCH_BLOB: Invalid value for highestKillStreak=%d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), highestKillStreak);
			scriptAssertf(numberOfKills>=0, "%s - PLAYSTATS_POST_MATCH_BLOB: Invalid value for numberOfKills=%d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), numberOfKills);
			scriptAssertf(numberOfDeaths>=0, "%s - PLAYSTATS_POST_MATCH_BLOB: Invalid value for numberOfDeaths=%d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), numberOfDeaths);
			scriptAssertf(numberOfSuicides>=0, "%s - PLAYSTATS_POST_MATCH_BLOB: Invalid value for numberOfSuicides=%d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), numberOfSuicides);
			scriptAssertf(scoreboardRanking>=0, "%s - PLAYSTATS_POST_MATCH_BLOB: Invalid value for scoreboardRanking=%d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), scoreboardRanking);
			//scriptAssertf(numberOfHeadshots>=0, "%s - PLAYSTATS_POST_MATCH_BLOB: Invalid value for numberOfHeadshots=%d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), numberOfHeadshots);
			//scriptAssertf(cashEarnedFromBets>=0, "%s - PLAYSTATS_POST_MATCH_BLOB: Invalid value for cashEarnedFromBets=%d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), cashEarnedFromBets);

			CNetworkTelemetry::PostMatchBlob((u32)xpEarned
												,cashEarned
												,(u32)highestKillStreak
												,(u32)numberOfKills
												,(u32)numberOfDeaths
												,(u32)numberOfSuicides
												,(u32)scoreboardRanking
												,teamId
												,crewId
												,(u32)vehicleId
												,cashEarnedFromBets
												,cashAtStart
												,cashAtEnd
												,betWon
												,survivedWave);
		}
	}

	int   CommandPlayStatsCreateMatchHistoryId( )
	{
		scriptAssertf(0, "%s - PLAYSTATS_CREATE_MATCH_HISTORY_ID: This command is deprecated, use instead PLAYSTATS_CREATE_MATCH_HISTORY_ID_2", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		const u64 historyMatchId = rlGetPosixTime();
		return (int)(historyMatchId & 0x00000000ffffffff);
	}

	bool   CommandPlayStatsCreateMatchHistoryId2(int& hashedMac, int& posixTime)
	{
		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_CREATE_MATCH_HISTORY_ID_2: Not in multiplayer game", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (NetworkInterface::IsGameInProgress())
		{
			const rlRosCredentials& cred = rlRos::GetCredentials( NetworkInterface::GetLocalGamerIndex() );
			if(cred.IsValid() && InvalidPlayerAccountId != cred.GetPlayerAccountId())
			{
				hashedMac = cred.GetPlayerAccountId();
				posixTime = (rlGetPosixTime() & 0xFFFFFFFF);
				return true;
			}

			u8 mac[6];
			if(netHardware::GetMacAddress(mac))
			{
				hashedMac = atDataHash((char*)mac, sizeof(mac));
				posixTime = (rlGetPosixTime() & 0xFFFFFFFF);
				return true;
			}
		}

		return false;
	}


	void  CommandPlayStatsMatchStarted(const char* matchCreator, const char* uniqueMatchId, MatchStartInfo* info)
	{
		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_MATCH_STARTED: Not in multiplayer game", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(matchCreator, "%s - PLAYSTATS_MATCH_STARTED: NO match creator set", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(uniqueMatchId, "%s - PLAYSTATS_MATCH_STARTED: NO unique match ID set", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(info, "%s - PLAYSTATS_MATCH_STARTED: NO info", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if (NetworkInterface::IsGameInProgress() && matchCreator && uniqueMatchId && info)
		{
			scriptAssertf(info->m_oncalljointime.Int >= 0, "%s - PLAYSTATS_MATCH_STARTED:  oncalljointime must be >= 0", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			scriptAssertf(info->m_oncalljoinstate.Int >= 0, "%s - PLAYSTATS_MATCH_STARTED: oncalljoinstate must be >= 0", CTheScripts::GetCurrentScriptNameAndProgramCounter());

			u64 matchHistoryId = ((u64)info->m_hashedMac.Int) << 32;
			matchHistoryId |= info->m_posixTime.Int & 0xFFFFFFFF;

			unsigned numFriendsTotal, numFriendsInSameTitle, numFriendsInMySession = 0;
			CLiveManager::GetFriendsStats(numFriendsTotal, numFriendsInSameTitle, numFriendsInMySession);

			MetricJOB_STARTED::sMatchStartInfo matchStartInfo;
			matchStartInfo.m_numFriendsTotal       = (u32)numFriendsTotal;
			matchStartInfo.m_numFriendsInSameTitle = (u32)numFriendsInSameTitle;
			matchStartInfo.m_numFriendsInMySession = (u32)numFriendsInMySession;
			matchStartInfo.m_oncalljointime        = (u32)info->m_oncalljointime.Int;
			matchStartInfo.m_oncalljoinstate       = (u32)info->m_oncalljoinstate.Int;
			matchStartInfo.m_missionDifficulty     = (u32)info->m_missionDifficulty.Int;
			matchStartInfo.m_MissionLaunch		   = (u32)info->m_MissionLaunch.Int;

			scriptAssertf(matchHistoryId, "%s - PLAYSTATS_MATCH_STARTED: matchHistoryId is 0, m_hashedMac='%d' | m_posixTime='%d'", CTheScripts::GetCurrentScriptNameAndProgramCounter(), info->m_hashedMac.Int, info->m_posixTime.Int);

			CNetworkTelemetry::SessionMatchStarted(matchCreator, uniqueMatchId, (u32)info->m_RootContentId.Int, matchHistoryId, matchStartInfo);

			SCPRESENCEUTIL.Script_SetPresenceAttributeInt(ATSTRINGHASH("mp_curr_gamemode",0x53938A49), info->m_matchType.Int);

			s_cashStart = MoneyInterface::GetVCWalletBalance() + MoneyInterface::GetVCBankBalance();
		}
	}

	void  CommandPlayStatsMatchEnded(int matchType, const char* matchCreator, const char* uniqueMatchId, bool isTeamDeathmatch, int raceType, bool leftInProgress)
	{
		scriptAssertf(0, "%s - PLAYSTATS_MATCH_ENDED: Commands deprecated - Use command instead PLAYSTATS_JOB_ACTIVITY_END", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_MATCH_ENDED: Not in multiplayer game", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (NetworkInterface::IsGameInProgress())
		{
			scriptAssertf(matchCreator, "%s - PLAYSTATS_MATCH_ENDED: NO match creator set", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			scriptAssertf(uniqueMatchId, "%s - PLAYSTATS_MATCH_ENDED: NO unique match ID set", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			CNetworkTelemetry::SessionMatchEnded(matchType, matchCreator, uniqueMatchId, isTeamDeathmatch, raceType, leftInProgress);
			SCPRESENCEUTIL.Script_SetPresenceAttributeInt(ATSTRINGHASH("mp_curr_gamemode",0x53938A49), -1);
			CNetwork::GetLeaderboardMgr().GetLeaderboardWriteMgr().MatchEnded();
		}
	}

	void  CommandPlayStatsShopItem(int itemHash, int cashSpent, int shopName, int extraItemHash, int colorHash)
	{
#if __ASSERT
		if (NetworkInterface::IsGameInProgress())
			scriptAssertf(0, "%s - PLAYSTATS_SHOP_ITEM: Commands deprecated in the NETWORK GAME - Use command instead NETWORK_BUY_ITEM.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
#endif // __ASSERT

		if (!NetworkInterface::IsGameInProgress())
		{
			scriptAssertf(cashSpent >= 0, "%s - PLAYSTATS_SHOP_ITEM: cashSpent %d must be >= 0", CTheScripts::GetCurrentScriptNameAndProgramCounter(), cashSpent);
			CNetworkTelemetry::ShopItem((u32)itemHash, (u32)cashSpent, (u32)shopName, (u32)extraItemHash, (u32)colorHash);
		}
	}

	void  CommandPlayStatsCrateDropMissionDone(int ambientMissionId, int xpEarned, int cashEarned, int weaponHash, int otherItemsHash, int enemiesKilled, ItemHashArray* _SpecialItemHashs, bool _CollectedBodyArmour)
	{
		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_CRATE_DROP_MISSION_DONE: Not in multiplayer game", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (NetworkInterface::IsGameInProgress())
		{
			u32 itemHashs[MetricAMBIENT_MISSION_CRATEDROP::MAX_CRATE_ITEM_HASHS];
			for(u32 i=0; i<MetricAMBIENT_MISSION_CRATEDROP::MAX_CRATE_ITEM_HASHS; i++)
			{
				itemHashs[i]=_SpecialItemHashs->m_Hashs[i].Int;
			}
			
			scriptAssertf(xpEarned>0 || cashEarned>0 || weaponHash!=0 || otherItemsHash!=0 || enemiesKilled>0, "%s - PLAYSTATS_CRATE_DROP_MISSION_DONE: At least one item needs to be > 0, xpEarned=%d || cashEarned=%d || weaponHash=%d || otherItemsHash=%d || enemiesKilled=%d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), xpEarned, cashEarned, weaponHash, otherItemsHash, enemiesKilled);
			CNetworkTelemetry::MultiplayerAmbientMissionCrateDrop(ambientMissionId, xpEarned, cashEarned, weaponHash, otherItemsHash, enemiesKilled, itemHashs, _CollectedBodyArmour);
		}
	}

	void  CommandPlayStatsCrateCreated(float X, float Y, float Z)
	{
		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_CRATE_CREATED: Not in multiplayer game", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (NetworkInterface::IsGameInProgress())
		{
#if __ASSERT
			CGameScriptHandlerNetComponent* pNetComponent = CTheScripts::GetCurrentGtaScriptThread()->m_NetComponent;
			if (!pNetComponent)
			{
				if (!CTheScripts::GetCurrentGtaScriptHandlerNetwork())
					scriptAssertf(0, "%s: PLAYSTATS_CRATE_CREATED - This script has not been set as a network script", CTheScripts::GetCurrentScriptNameAndProgramCounter());
				else
					scriptAssertf(0,  "%s: PLAYSTATS_CRATE_CREATED - This script is not in a NETSCRIPT_PLAYING state (NETWORK_GET_SCRIPT_STATUS is not returning a playing state)", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			}
			else
			{
				scriptAssertf(pNetComponent->IsHostLocal(), "%s - PLAYSTATS_CRATE_CREATED: only the host should call this", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			}
#endif // __ASSERT

			CNetworkTelemetry::MultiplayerAmbientMissionCrateCreated(X, Y, Z);
		}
	}

	void  CommandPlayStatsHoldUpMissionDone(int ambientMissionId, int xpEarned, int cashEarned, int shopkeepersKilled)
	{
		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_HOLD_UP_MISSION_DONE: Not in multiplayer game", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (NetworkInterface::IsGameInProgress())
		{
			CNetworkTelemetry::MultiplayerAmbientMissionHoldUp(ambientMissionId, xpEarned, cashEarned, shopkeepersKilled);
		}
	}
	void  CommandPlayStatsImportExportMissionDone(int ambientMissionId, int xpEarned, int cashEarned, int vehicleIndex)
	{
		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_IMPORT_EXPORT_MISSION_DONE: Not in multiplayer game", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (NetworkInterface::IsGameInProgress())
		{
			CVehicle* vehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(vehicleIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
			if (scriptVerifyf(vehicle, "%s:PLAYSTATS_IMPORT_EXPORT_MISSION_DONE - no vehicle with this id=%d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), vehicleIndex))
			{
				CNetworkTelemetry::MultiplayerAmbientMissionImportExport(ambientMissionId, xpEarned, cashEarned, vehicle->GetModelIndex());
			}
		}
	}
	void  CommandPlayStatsSecurityVanMissionDone(int ambientMissionId, int xpEarned, int cashEarned)
	{
		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_SECURITY_VAN_MISSION_DONE: Not in multiplayer game", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (NetworkInterface::IsGameInProgress())
		{
			CNetworkTelemetry::MultiplayerAmbientMissionSecurityVan(ambientMissionId, xpEarned, cashEarned);
		}
	}
	void  CommandPlayStatsRaceToPointMissionDone(int ambientMissionId, int xpEarned, int cashEarned, RaceToPointInfo* info)
	{
		scriptAssertf(info, "%s - PLAYSTATS_RACE_TO_POINT_MISSION_DONE: NULL info.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!info) return;

		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_RACE_TO_POINT_MISSION_DONE: Not in multiplayer game", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (NetworkInterface::IsGameInProgress())
		{
			MetricAMBIENT_MISSION_RACE_TO_POINT::sRaceToPointInfo rtopInfo;
			rtopInfo.m_LocationHash = (u32)info->m_LocationHash.Int;
			rtopInfo.m_MatchId = (u32)info->m_MatchId.Int;
			rtopInfo.m_NumPlayers = (u32)info->m_NumPlayers.Int;
			rtopInfo.m_RaceWon = info->m_RaceWon.Int != 0;
			rtopInfo.m_RaceStartPos = Vector3(info->m_RaceStartPos);
			rtopInfo.m_RaceEndPos = Vector3(info->m_RaceEndPos);

			CNetworkTelemetry::MultiplayerAmbientMissionRaceToPoint(ambientMissionId, xpEarned, cashEarned, rtopInfo);
		}
	}
	void  CommandPlayStatsProstitutesMissionDone(int ambientMissionId, int xpEarned, int cashEarned, int cashSpent, int numberOfServices, bool playerWasTheProstitute)
	{
		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_PROSTITUTE_MISSION_DONE: Not in multiplayer game", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (NetworkInterface::IsGameInProgress())
		{
			scriptAssertf(cashSpent >= 0, "%s - PLAYSTATS_PROSTITUTE_MISSION_DONE: cashSpent %d must be >= 0", CTheScripts::GetCurrentScriptNameAndProgramCounter(), cashSpent);
			scriptAssertf(cashEarned >= 0, "%s - PLAYSTATS_PROSTITUTE_MISSION_DONE: cashEarned %d must be >= 0", CTheScripts::GetCurrentScriptNameAndProgramCounter(), cashEarned);
			scriptAssertf(numberOfServices >= 0, "%s - PLAYSTATS_PROSTITUTE_MISSION_DONE: numberOfServices %d must be >= 0", CTheScripts::GetCurrentScriptNameAndProgramCounter(), numberOfServices);
			CNetworkTelemetry::MultiplayerAmbientMissionProstitute(ambientMissionId, xpEarned, (u32)cashEarned, (u32)cashSpent, (u32)numberOfServices, playerWasTheProstitute);
		}
	}

	void  CommandPlayStatsAcquiredHiddenPackage(int id)
	{
		CNetworkTelemetry::AcquiredHiddenPackage(id);
	}

	void  CommandPlayStatsWebsiteVisited(int id, const int timespent)
	{
		scriptAssertf(timespent >= 0, "%s - PLAYSTATS_WEBSITE_VISITED: timespent %d must be >= 0", CTheScripts::GetCurrentScriptNameAndProgramCounter(), timespent);
		CNetworkTelemetry::WebsiteVisited(id, timespent);
	}

	void  CommandPlayStatsFriendsActivity(int character, int outcome)
	{
		CNetworkTelemetry::FriendsActivityDone(character, outcome);
	}

	void  CommandPlayStatsOddJobDone(int UNUSED_PARAM(timespent), int UNUSED_PARAM(oddJobId), int UNUSED_PARAM(extraOddJobId))
	{
		// DEPRECATED COMMAND
	}

	void  CommandAppendCreatorUsageMetric(int timeSpent, int numberOfTries, int numberOfEarlyQuits, bool isCreating, int missionId)
	{
		CNetworkTelemetry::AppendCreatorUsage((u32)timeSpent, (u32)numberOfTries, (u32)numberOfEarlyQuits, isCreating, (u32)missionId);
	}

	void   CommandPlayStatsHeistSaveCheat(const int value, const int secondValue=0)
	{
		CNetworkTelemetry::AppendHeistSaveCheat(value, secondValue);
	}

	void   CommandAppendStarterPackApplied(const int item, const int amount)
	{
		CNetworkTelemetry::AppendStarterPackApplied(item, amount);
	}

	void   CommandAppendLastVehMetric(scrArrayBaseAddr& data, int& fromhandleData, int& tohandleData)
	{
		int numItems = scrArray::GetCount(data);
		int* pArray = scrArray::GetElements<int>(data);
		scriptAssertf(MetricLAST_VEH::NUM_FIELDS >= numItems, "%s - APPEND_LAST_VEH_METRIC: Too many fields (%d). The metric only supports %d fields, ask code to bump it.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), numItems, MetricLAST_VEH::NUM_FIELDS);

		// fill in leaderboard handle
		rlGamerHandle fromGamer;
		if(!CTheScripts::ImportGamerHandle(&fromGamer, fromhandleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "APPEND_LAST_VEH_METRIC")))
			return;
		rlGamerHandle toGamer;
		if(!CTheScripts::ImportGamerHandle(&toGamer, tohandleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "APPEND_LAST_VEH_METRIC")))
			return;

		MetricLAST_VEH metric;
		for(int i=0; i<MetricLAST_VEH::NUM_FIELDS && i<numItems; i++)
		{
			if(pArray[i] != 0)
			{
				metric.m_fields[i] = pArray[i];
			}
		}

		fromGamer.ToString(metric.m_FromGH, COUNTOF(metric.m_FromGH));
		toGamer.ToString(metric.m_ToGH, COUNTOF(metric.m_ToGH));

		CNetworkTelemetry::AppendSecurityMetric(metric);
	}

	class MetricAWARD_BADSPORT : public MetricPlayStat
	{
		RL_DECLARE_METRIC(AWARDBS, TELEMETRY_CHANNEL_AMBIENT, LOGLEVEL_HIGH_PRIORITY);

	private:
		int m_evenID;

	public:
		explicit MetricAWARD_BADSPORT(const int evenID) : m_evenID(evenID) {;}

		virtual bool Write(RsonWriter* rw) const 
		{ 
			return MetricPlayStat::Write(rw)
				&& rw->WriteInt("id", m_evenID);
		}
	};

	void   CommandAppendAwardBadSport(int evenID)
	{
		scriptAssertf(evenID >= 0, "%s - PLAYSTATS_AWARD_BAD_SPORT: evenID='%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), evenID);
		MetricAWARD_BADSPORT m(evenID);
		APPEND_METRIC(m);
	}

	void   CommandAppendDirectorModeMetric(const VideoClipMetric* values)
	{
		MetricDIRECTOR_MODE::VideoClipMetrics m;
		m.m_characterHash=values->m_characterHash.Int;
		m.m_timeOfDay=values->m_timeOfDay.Int;
		m.m_weather=values->m_weather.Int;
		m.m_wantedLevel= (values->m_wantedLevel.Int==1);
		m.m_pedDensity=values->m_pedDensity.Int;
		m.m_vehicleDensity=values->m_vehicleDensity.Int;
		m.m_restrictedArea= (values->m_restrictedArea.Int==1);
		m.m_invulnerability= (values->m_invulnerability.Int==1);
		CNetworkTelemetry::AppendDirectorMode(m);
	}

	void  CommandPlayStatsPropChange(const int pedIndex, const int position, const int newPropIndex, const int newTextIndex)
	{
		CPed* ped = CTheScripts::GetEntityToModifyFromGUID< CPed >( pedIndex );
		scriptAssertf(ped, "%s: PLAYSTATS_PROP_CHANGE - Invalid ped index %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pedIndex);

		if(ped)
		{
			scriptAssertf(ped->GetBaseModelInfo(), "%s: PLAYSTATS_PROP_CHANGE - Invalid GetBaseModelInfo for ped index %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pedIndex);
			scriptAssertf(newPropIndex > -1, "%s: PLAYSTATS_PROP_CHANGE - newPropIndex must be >-1", CTheScripts::GetCurrentScriptNameAndProgramCounter());

			if(newPropIndex > -1 && ped->GetBaseModelInfo())
			{
				const bool isValid = CPedPropsMgr::IsPropValid(ped, (eAnchorPoints)position, newPropIndex, newTextIndex);
				scriptAssertf(isValid, "%s: PLAYSTATS_PROP_CHANGE - Invalid Prop. anchor: %d prop: %d tex: %d (%s)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), position, newPropIndex, newTextIndex, ped->GetModelName());

				if(isValid)
				{
					CNetworkTelemetry::ChangePlayerProp(ped->GetBaseModelInfo()->GetHashKey(), position, newPropIndex, newTextIndex);
				}
			}
		}
	}

	void  CommandPlayStatsClothChange(const int pedIndex, const int componentID, const int drawableID, const int textureID, const int paletteID)
	{
		CPed* ped = CTheScripts::GetEntityToModifyFromGUID< CPed >( pedIndex );
		scriptAssertf(ped, "%s: PLAYSTATS_CLOTH_CHANGE - Invalid ped index %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pedIndex);

		if(ped)
		{
			scriptAssertf(ped->GetBaseModelInfo(), "%s: PLAYSTATS_CLOTH_CHANGE - Invalid GetBaseModelInfo for ped index %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pedIndex);
			if(ped->GetBaseModelInfo())
			{
				CNetworkTelemetry::ChangedPlayerCloth(ped->GetBaseModelInfo()->GetHashKey(), componentID, drawableID, textureID, paletteID);
			}
		}
	}

	void  CommandPlayStatsWeaponModChange(const int weaponHash, const int modIdTo, const int modIdFrom)
	{
		if(SCRIPT_VERIFY(weaponHash != 0, "PLAYSTATS_WEAPON_MODE_CHANGE - Weapon hash is invalid"))
		{
			const CItemInfo* itemInfo = CWeaponInfoManager::GetInfo(weaponHash);
			if(scriptVerifyf(itemInfo, "Weapon hash [%d] does not exist in data", weaponHash))
			{
				if(CModelInfo::GetBaseModelInfoFromHashKey(itemInfo->GetModelHash(), NULL))
				{
#if __ASSERT
					if (modIdTo != 0)   scriptAssertf(CWeaponComponentManager::GetInfo(modIdTo), "Weapon component modIdTo [%d] does not exist in data", modIdTo);
					if (modIdFrom != 0) scriptAssertf(CWeaponComponentManager::GetInfo(modIdFrom), "Weapon component modIdFrom [%d] does not exist in data", modIdFrom);
#endif

					CNetworkTelemetry::ChangeWeaponMod(weaponHash, modIdFrom, modIdTo);
				}
			}
		}
	}

	void  CommandPlayStatsCheatApplied(const char* cheatString)
	{
		scriptAssertf(!NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_CHEAT_APPLIED: Single Player only", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!NetworkInterface::IsGameInProgress())
		{
			scriptAssertf(cheatString, "%s - PLAYSTATS_CHEAT_APPLIED: no cheat string set", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			if (cheatString)
			{
				CNetworkTelemetry::CheatApplied(cheatString);
			}
		}
	}

	class MetricIDLEKICK : public MetricPlayStat
	{
		RL_DECLARE_METRIC(IDLEKICK, TELEMETRY_CHANNEL_JOB, LOGLEVEL_VERYLOW_PRIORITY);

	private:
		u32 m_idleTime;

	public:
		MetricIDLEKICK(const u32 idletime) : m_idleTime(idletime) {}

		virtual bool Write(RsonWriter* rw) const
		{
			return MetricPlayStat::Write(rw) 
				&& rw->WriteUns("idle", m_idleTime);
		}
	};
	void  CommandPlayStatsIdleKick(const int idleTime)
	{
		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_IDLE_KICK: Multiplayer Player only", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (NetworkInterface::IsGameInProgress())
		{
			scriptAssertf(idleTime, "%s - PLAYSTATS_IDLE_KICK: Time %d must be > 0", CTheScripts::GetCurrentScriptNameAndProgramCounter(), idleTime);
			if (idleTime > 0)
			{
				MetricIDLEKICK m((u32)idleTime);
				CNetworkTelemetry::AppendMetric(m);
			}
		}
	}

	bool  CommandStatGetCoderStat(const int ASSERT_ONLY(keyHash))
	{
		bool ret = false;

#if __ASSERT
		const sStatData* stat = 0;

		CStatsDataMgr::StatsMap::Iterator statIter;
		if (CStatsMgr::GetStatsDataMgr().StatIterFind(keyHash, statIter))
		{
			if (statIter.GetKey().IsValid())
			{
				stat = *statIter;
			}
		}

		scriptAssertf(stat, "%s : STAT_GET_CODER_STAT - Stat with key=\"%d\" does not exist.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), keyHash);

		ret = (stat && stat->GetIsCodeStat());

#endif // __ASSERT

		return ret;
	}

	void   CommandPlayStatsJobActivityEnd(const char* creator, const char* matchId, JobActivityInfo* info, const char* playlistid)
	{
		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_JOB_ACTIVITY_END: Not in multiplayer game", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if (!NetworkInterface::IsGameInProgress())
			return;

		scriptAssertf(creator, "%s:PLAYSTATS_JOB_ACTIVITY_END - NULL creator.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!creator)
			return;
		scriptAssertf(ustrlen(creator)<=MetricPlayStat::MAX_STRING_LENGTH, "%s - PLAYSTATS_JOB_ACTIVITY_END: Invalid creator string size %s, max size is %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), creator, MetricPlayStat::MAX_STRING_LENGTH);

		scriptAssertf(matchId, "%s:PLAYSTATS_JOB_ACTIVITY_END - NULL matchId.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!matchId)
			return;
		scriptAssertf(ustrlen(matchId)<=MetricPlayStat::MAX_STRING_LENGTH, "%s - PLAYSTATS_JOB_ACTIVITY_END: Invalid matchId string size %s, max size is %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), matchId, MetricPlayStat::MAX_STRING_LENGTH);

		scriptAssertf(info, "%s:PLAYSTATS_JOB_ACTIVITY_END - NULL info.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!info)
			return;

		sJobInfo jobInfo;

		jobInfo.m_dataU64[jobInfo.m_u64used] = ((u64)info->m_1stHashMac.Int) << 32;
		jobInfo.m_dataU64[jobInfo.m_u64used] |= info->m_1stPosixTime.Int & 0xFFFFFFFF;
		++jobInfo.m_u64used; gnetAssert(jobInfo.m_u64used <= sJobInfo::NUM_U64);
		jobInfo.m_dataU64[jobInfo.m_u64used] = ((u64)info->m_playlistHashMac.Int) << 32;
		jobInfo.m_dataU64[jobInfo.m_u64used] |= info->m_playlistPosixTime.Int & 0xFFFFFFFF;
		++jobInfo.m_u64used; gnetAssert(jobInfo.m_u64used <= sJobInfo::NUM_U64);


		s64 cashStart = s_cashStart;
		s_cashStart = 0;
		s64 cashEnd = MoneyInterface::GetVCWalletBalance() + MoneyInterface::GetVCBankBalance();
		jobInfo.m_dataU64[jobInfo.m_u64used] = cashStart;
		++jobInfo.m_u64used; gnetAssert(jobInfo.m_u64used <= sJobInfo::NUM_U64);
		jobInfo.m_dataU64[jobInfo.m_u64used] = cashEnd;
		++jobInfo.m_u64used; gnetAssert(jobInfo.m_u64used <= sJobInfo::NUM_U64);

		jobInfo.m_dataINTS[jobInfo.m_intsused] = info->m_Cash.Int;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = info->m_BetCash.Int;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = info->m_CashStart.Int;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = info->m_CashEnd.Int;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = info->m_MissionLaunch.Int;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = info->m_jobDifficulty.Int;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(info->m_outfitStyle.Int >= -1) ? info->m_outfitStyle.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(info->m_outfitChoiceType.Int >= 0) ? info->m_outfitChoiceType.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);

		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(info->m_totalPoints.Int >= 0) ? info->m_totalPoints.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(info->m_largeTargetsHit.Int >= 0) ? info->m_largeTargetsHit.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(info->m_mediumTargetsHit.Int >= 0) ? info->m_mediumTargetsHit.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(info->m_smallTargetsHit.Int >= 0) ? info->m_smallTargetsHit.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(info->m_tinyTargetsHit.Int >= 0) ? info->m_tinyTargetsHit.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);

		jobInfo.m_dataBOOLEANS[jobInfo.m_boolused] = (info->m_isTeamDeathmatch.Int) != 0 ? true : false;
		++jobInfo.m_boolused; gnetAssert(jobInfo.m_boolused <= sJobInfo::NUM_BOOLS);
		jobInfo.m_dataBOOLEANS[jobInfo.m_boolused] = (info->m_leftInProgress.Int  ) != 0 ? true : false;
		++jobInfo.m_boolused; gnetAssert(jobInfo.m_boolused <= sJobInfo::NUM_BOOLS);
		jobInfo.m_dataBOOLEANS[jobInfo.m_boolused] = (info->m_ishead2head.Int  ) != 0 ? true : false;
		++jobInfo.m_boolused; gnetAssert(jobInfo.m_boolused <= sJobInfo::NUM_BOOLS);
		jobInfo.m_dataBOOLEANS[jobInfo.m_boolused] = (info->m_isPlaylistChallenge.Int  ) != 0 ? true : false;
		++jobInfo.m_boolused; gnetAssert(jobInfo.m_boolused <= sJobInfo::NUM_BOOLS);
		jobInfo.m_dataBOOLEANS[jobInfo.m_boolused] = (info->m_jobVisibilitySetInCorona.Int) != 0 ? true : false;
		++jobInfo.m_boolused; gnetAssert(jobInfo.m_boolused <= sJobInfo::NUM_BOOLS);
		jobInfo.m_dataBOOLEANS[jobInfo.m_boolused] = (info->m_weaponsfixed.Int) != 0 ? true : false;
		++jobInfo.m_boolused; gnetAssert(jobInfo.m_boolused <= sJobInfo::NUM_BOOLS);
		jobInfo.m_dataBOOLEANS[jobInfo.m_boolused] = (info->m_sessionVisible.Int) != 0 ? true : false;
		++jobInfo.m_boolused; gnetAssert(jobInfo.m_boolused <= sJobInfo::NUM_BOOLS);
		jobInfo.m_dataBOOLEANS[jobInfo.m_boolused] = (info->m_dnf.Int) != 0 ? true : false;
		++jobInfo.m_boolused; gnetAssert(jobInfo.m_boolused <= sJobInfo::NUM_BOOLS);
		jobInfo.m_dataBOOLEANS[jobInfo.m_boolused] = (info->m_aggregateScore.Int) != 0 ? true : false;
		++jobInfo.m_boolused; gnetAssert(jobInfo.m_boolused <= sJobInfo::NUM_BOOLS);

		jobInfo.m_dataU32[jobInfo.m_u32used]  = scriptVerify(info->m_XP.Int                >= 0) ? static_cast< u32 > (info->m_XP.Int               ) : 0;
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used]  = scriptVerify(info->m_HighestKillStreak.Int >= 0) ? static_cast< u32 > (info->m_HighestKillStreak.Int) : 0;
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used]  = scriptVerify(info->m_Kills.Int             >= 0) ? static_cast< u32 > (info->m_Kills.Int            ) : 0;
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used]  = scriptVerify(info->m_Deaths.Int            >= 0) ? static_cast< u32 > (info->m_Deaths.Int           ) : 0;
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used]  = scriptVerify(info->m_Suicides.Int          >= 0) ? static_cast< u32 > (info->m_Suicides.Int         ) : 0;
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used]  = scriptVerify(info->m_Rank.Int              >= 0) ? static_cast< u32 > (info->m_Rank.Int             ) : 0;
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used]  = scriptVerify(info->m_CrewId.Int            >= 0) ? static_cast< u32 > (info->m_CrewId.Int           ) : 0;
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used]  = static_cast< u32 > (info->m_VehicleId.Int);
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used]  = scriptVerify(info->m_betWon.Int            >= 0) ? static_cast< u32 > (info->m_betWon.Int           ) : 0;
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used]  = static_cast< u32 > (info->m_survivedWave.Int     );
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = static_cast< u32 > (info->m_TeamId.Int           );
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = scriptVerify(info->m_MatchType.Int         >= 0) ? static_cast< u32 > (info->m_MatchType.Int        ) : 0;
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = scriptVerify(info->m_raceType.Int          >= 0) ? static_cast< u32 > (info->m_raceType.Int         ) : 0;
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = fwTimer::GetSystemTimeInMilliseconds() - CNetworkTelemetry::GetMatchStartTime();
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = (u32)StatsInterface::GetUInt64Stat(STAT_TOTAL_PLAYING_TIME.GetHash());
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = scriptVerify(info->m_matchResult.Int       >= 0) ? static_cast< u32 > (info->m_matchResult.Int      ) : 0;
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = scriptVerify(info->m_jpEarned.Int		  >= 0) ? static_cast< u32 > (info->m_jpEarned.Int		 ) : 0;
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = scriptVerify(info->m_numLaps.Int		    >= 0) ? static_cast< u32 > (info->m_numLaps.Int		     ) : 0;
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = static_cast< u32 > (info->m_playlistID.Int);
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = static_cast< u32 > (info->m_celebrationanim.Int);
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = static_cast< u32 > (info->m_quickplayanim.Int);
        ++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
        jobInfo.m_dataU32[jobInfo.m_u32used] = static_cast< u32 > (info->m_raceSubtype.Int);
        ++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = static_cast< u32 > (info->m_StuntLaunchCorona.Int);
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);

		jobInfo.m_MatchCreator[0] = '\0';
		safecpy(jobInfo.m_MatchCreator,  creator, COUNTOF(jobInfo.m_MatchCreator));

		jobInfo.m_UniqueMatchId[0] = '\0';
		safecpy(jobInfo.m_UniqueMatchId, matchId, COUNTOF(jobInfo.m_UniqueMatchId));

		MetricJOB_ENDED m(jobInfo, playlistid);
		APPEND_METRIC_FLUSH( m, true );
		CNetworkTelemetry::SessionMatchEnd( );
	}

	void   CommandPlayStatsJobBEnd(const char* creator, const char* matchId, JobBInfo* info, const char* playlistid)
	{
		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_JOB_BEND: Not in multiplayer game", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if (!NetworkInterface::IsGameInProgress())
			return;

		scriptAssertf(creator, "%s:PLAYSTATS_JOB_BEND - NULL creator.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!creator)
			return;
		scriptAssertf(ustrlen(creator)<=MetricPlayStat::MAX_STRING_LENGTH, "%s - PLAYSTATS_JOB_BEND: Invalid creator string size %s, max size is %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), creator, MetricPlayStat::MAX_STRING_LENGTH);

		scriptAssertf(matchId, "%s:PLAYSTATS_JOB_BEND - NULL matchId.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!matchId)
			return;
		scriptAssertf(ustrlen(matchId)<=MetricPlayStat::MAX_STRING_LENGTH, "%s - PLAYSTATS_JOB_BEND: Invalid matchId string size %s, max size is %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), matchId, MetricPlayStat::MAX_STRING_LENGTH);

		scriptAssertf(info, "%s:PLAYSTATS_JOB_BEND - NULL info.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!info)
			return;

		sJobInfo jobInfo;

		jobInfo.m_dataU64[jobInfo.m_u64used] = ((u64)info->m_heistSessionHashMac.Int) << 32;
		jobInfo.m_dataU64[jobInfo.m_u64used] |= info->m_heistSessionIdPosixTime.Int & 0xFFFFFFFF;
		++jobInfo.m_u64used; gnetAssert(jobInfo.m_u64used <= sJobInfo::NUM_U64);


		s64 cashStart = s_cashStart;
		s_cashStart = 0;
		s64 cashEnd = MoneyInterface::GetVCWalletBalance() + MoneyInterface::GetVCBankBalance();
		jobInfo.m_dataU64[jobInfo.m_u64used] = cashStart;
		++jobInfo.m_u64used; gnetAssert(jobInfo.m_u64used <= sJobInfo::NUM_U64);
		jobInfo.m_dataU64[jobInfo.m_u64used] = cashEnd;
		++jobInfo.m_u64used; gnetAssert(jobInfo.m_u64used <= sJobInfo::NUM_U64);

		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(info->m_role.Int >= 0) ? info->m_role.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(info->m_cashcutpercentage.Int >= 0) ? info->m_cashcutpercentage.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] =  scriptVerify(info->m_kills.Int >= 0) ? info->m_kills.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(info->m_deaths.Int >= 0) ? info->m_deaths.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(info->m_clothes.Int >= 0) ? info->m_clothes.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = info->m_Cash.Int;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = info->m_CashStart.Int;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(info->m_CashEnd.Int >= 0) ? info->m_CashEnd.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(info->m_MissionLaunch.Int >= 0) ? info->m_MissionLaunch.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(info->m_leaderscashcutpercentage.Int >= 0) ? info->m_leaderscashcutpercentage.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = info->m_heistRootConstentId.Int;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(info->m_TimePlanningBoard.Int >= 0) ? info->m_TimePlanningBoard.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(info->m_outfitChoiceBy.Int >= 0) ? info->m_outfitChoiceBy.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(info->m_outfitChoiceType.Int >= 0) ? info->m_outfitChoiceType.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(info->m_outfitStyle.Int >= -1) ? info->m_outfitStyle.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(info->m_difficulty.Int >= 0) ? info->m_difficulty.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(info->m_1stperson.Int >= 0) ? info->m_1stperson.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(info->m_medal.Int >= 0) ? info->m_medal.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(info->m_teamLivesUsed.Int >= 0) ? info->m_teamLivesUsed.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(info->m_failureReason.Int >= 0) ? info->m_failureReason.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(info->m_failureRole.Int >= 0) ? info->m_failureRole.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(info->m_usedQuickRestart.Int >= 0) ? info->m_usedQuickRestart.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(info->m_cashLost.Int >= 0) ? info->m_cashLost.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(info->m_cashPickedUp.Int >= 0) ? info->m_cashPickedUp.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(info->m_minigameTimeTaken0.Int >= 0) ? info->m_minigameTimeTaken0.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(info->m_minigameNumberOfTimes0.Int >= 0) ? info->m_minigameNumberOfTimes0.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(info->m_minigameTimeTaken1.Int >= 0) ? info->m_minigameTimeTaken1.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(info->m_minigameNumberOfTimes1.Int >= 0) ? info->m_minigameNumberOfTimes1.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(info->m_maskChoice.Int >= 0) ? info->m_maskChoice.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);

		jobInfo.m_dataU32[jobInfo.m_u32used] = fwTimer::GetSystemTimeInMilliseconds() - CNetworkTelemetry::GetMatchStartTime();
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = (u32)StatsInterface::GetUInt64Stat(STAT_TOTAL_PLAYING_TIME.GetHash());;
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = (u32)info->m_XP.Int;
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = (u32)info->m_Suicides.Int;
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = (u32)info->m_Rank.Int;
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = (u32)info->m_CrewId.Int;
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = (u32)info->m_TeamId.Int;
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = (u32)info->m_matchResult.Int;
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = (u32)info->m_jpEarned.Int;
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = (u32)info->m_celebrationanim.Int;
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = (u32)info->m_quickplayanim.Int;
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);

		jobInfo.m_dataBOOLEANS[jobInfo.m_boolused] = (info->m_ishost.Int != 0) ? true : false;
		++jobInfo.m_boolused; gnetAssert(jobInfo.m_boolused <= sJobInfo::NUM_BOOLS);
		jobInfo.m_dataBOOLEANS[jobInfo.m_boolused] = (info->m_sessionVisible.Int != 0) ? true : false;
		++jobInfo.m_boolused; gnetAssert(jobInfo.m_boolused <= sJobInfo::NUM_BOOLS);
		jobInfo.m_dataBOOLEANS[jobInfo.m_boolused] = (info->m_leftInProgress.Int != 0) ? true : false;
		++jobInfo.m_boolused; gnetAssert(jobInfo.m_boolused <= sJobInfo::NUM_BOOLS);
		jobInfo.m_dataBOOLEANS[jobInfo.m_boolused] = (info->m_lesterCalled.Int != 0) ? true : false;
		++jobInfo.m_boolused; gnetAssert(jobInfo.m_boolused <= sJobInfo::NUM_BOOLS);
		jobInfo.m_dataBOOLEANS[jobInfo.m_boolused] = (info->m_spookedCops.Int != 0) ? true : false;
		++jobInfo.m_boolused; gnetAssert(jobInfo.m_boolused <= sJobInfo::NUM_BOOLS);
		jobInfo.m_dataBOOLEANS[jobInfo.m_boolused] = (info->m_usedTripSkip.Int != 0) ? true : false;
		++jobInfo.m_boolused; gnetAssert(jobInfo.m_boolused <= sJobInfo::NUM_BOOLS);

		jobInfo.m_MatchCreator[0] = '\0';
		safecpy(jobInfo.m_MatchCreator,  creator, COUNTOF(jobInfo.m_MatchCreator));

		jobInfo.m_UniqueMatchId[0] = '\0';
		safecpy(jobInfo.m_UniqueMatchId, matchId, COUNTOF(jobInfo.m_UniqueMatchId));

		//Instantiate & append the metric.
		MetricJOBBENDED m(jobInfo, playlistid);
		APPEND_METRIC_FLUSH( m, true );
		CNetworkTelemetry::SessionMatchEnd( );
	}

	void   CommandPlayStatsJobLtsEnd(const char* creator, const char* matchId, JobLTSInfo* info, const char* playlistid)
	{
		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_JOB_LTS_END: Not in multiplayer game", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if (!NetworkInterface::IsGameInProgress())
			return;

		scriptAssertf(creator, "%s:PLAYSTATS_JOB_LTS_END - NULL creator.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!creator)
			return;
		scriptAssertf(ustrlen(creator)<=MetricPlayStat::MAX_STRING_LENGTH, "%s - PLAYSTATS_JOB_LTS_END: Invalid creator string size %s, max size is %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), creator, MetricPlayStat::MAX_STRING_LENGTH);

		scriptAssertf(matchId, "%s:PLAYSTATS_JOB_LTS_END - NULL matchId.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!matchId)
			return;
		scriptAssertf(ustrlen(matchId)<=MetricPlayStat::MAX_STRING_LENGTH, "%s - PLAYSTATS_JOB_LTS_END: Invalid matchId string size %s, max size is %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), matchId, MetricPlayStat::MAX_STRING_LENGTH);

		scriptAssertf(info, "%s:PLAYSTATS_JOB_LTS_END - NULL info.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!info)
			return;

		sJobInfo jobInfo;

		jobInfo.m_dataU64[jobInfo.m_u64used] = ((u64)info->m_1stHashMac.Int) << 32;
		jobInfo.m_dataU64[jobInfo.m_u64used] |= info->m_1stPosixTime.Int & 0xFFFFFFFF;
		++jobInfo.m_u64used; gnetAssert(jobInfo.m_u64used <= sJobInfo::NUM_U64);
		jobInfo.m_dataU64[jobInfo.m_u64used] = ((u64)info->m_playlistHashMac.Int) << 32;
		jobInfo.m_dataU64[jobInfo.m_u64used] |= info->m_playlistPosixTime.Int & 0xFFFFFFFF;
		++jobInfo.m_u64used; gnetAssert(jobInfo.m_u64used <= sJobInfo::NUM_U64);

		s64 cashStart = s_cashStart;
		s_cashStart = 0;
		s64 cashEnd = MoneyInterface::GetVCWalletBalance() + MoneyInterface::GetVCBankBalance();
		jobInfo.m_dataU64[jobInfo.m_u64used] = cashStart;
		++jobInfo.m_u64used; gnetAssert(jobInfo.m_u64used <= sJobInfo::NUM_U64);
		jobInfo.m_dataU64[jobInfo.m_u64used] = cashEnd;
		++jobInfo.m_u64used; gnetAssert(jobInfo.m_u64used <= sJobInfo::NUM_U64);

		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(info->m_roundsset.Int >= 0) ? info->m_roundsset.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(info->m_roundsplayed.Int >= 0) ? info->m_roundsplayed.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(info->m_Cash.Int >= 0) ? info->m_Cash.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(info->m_BetCash.Int >= 0) ? info->m_BetCash.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = info->m_CashStart.Int;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = info->m_CashEnd.Int;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(info->m_MissionLaunch.Int >= 0) ? info->m_MissionLaunch.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(info->m_outfitChoiceType.Int >= 0) ? info->m_outfitChoiceType.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(info->m_outfitStyle.Int >= -1) ? info->m_outfitStyle.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);

		jobInfo.m_dataU32[jobInfo.m_u32used] = fwTimer::GetSystemTimeInMilliseconds() - CNetworkTelemetry::GetMatchStartTime();
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = (u32)StatsInterface::GetUInt64Stat(STAT_TOTAL_PLAYING_TIME.GetHash());
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = (u32)info->m_XP.Int;
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = (u32)info->m_HighestKillStreak.Int;
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = (u32)info->m_Kills.Int;
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = (u32)info->m_Deaths.Int;
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = (u32)info->m_Suicides.Int;
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = (u32)info->m_Rank.Int;
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = (u32)info->m_CrewId.Int;
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = (u32)info->m_betWon.Int;
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = (u32)info->m_TeamId.Int;
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = (u32)info->m_matchResult.Int;
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = (u32)info->m_jpEarned.Int;
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = (u32)info->m_playlistID.Int;
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = (u32)info->m_celebrationanim.Int;
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = (u32)info->m_quickplayanim.Int;
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);

		jobInfo.m_dataBOOLEANS[jobInfo.m_boolused] = info->m_ishead2head.Int != 0 ? true : false;
		++jobInfo.m_boolused; gnetAssert(jobInfo.m_boolused <= sJobInfo::NUM_BOOLS);
		jobInfo.m_dataBOOLEANS[jobInfo.m_boolused] = info->m_isPlaylistChallenge.Int != 0 ? true : false;
		++jobInfo.m_boolused; gnetAssert(jobInfo.m_boolused <= sJobInfo::NUM_BOOLS);
		jobInfo.m_dataBOOLEANS[jobInfo.m_boolused] = info->m_jobVisibilitySetInCorona.Int != 0 ? true : false;
		++jobInfo.m_boolused; gnetAssert(jobInfo.m_boolused <= sJobInfo::NUM_BOOLS);
		jobInfo.m_dataBOOLEANS[jobInfo.m_boolused] = info->m_weaponsfixed.Int != 0 ? true : false;
		++jobInfo.m_boolused; gnetAssert(jobInfo.m_boolused <= sJobInfo::NUM_BOOLS);
		jobInfo.m_dataBOOLEANS[jobInfo.m_boolused] = info->m_sessionVisible.Int != 0 ? true : false;
		++jobInfo.m_boolused; gnetAssert(jobInfo.m_boolused <= sJobInfo::NUM_BOOLS);
		jobInfo.m_dataBOOLEANS[jobInfo.m_boolused] = info->m_leftInProgress.Int != 0 ? true : false;
		++jobInfo.m_boolused; gnetAssert(jobInfo.m_boolused <= sJobInfo::NUM_BOOLS);

		jobInfo.m_MatchCreator[0] = '\0';
		safecpy(jobInfo.m_MatchCreator,  creator, COUNTOF(jobInfo.m_MatchCreator));

		jobInfo.m_UniqueMatchId[0] = '\0';
		safecpy(jobInfo.m_UniqueMatchId, matchId, COUNTOF(jobInfo.m_UniqueMatchId));

		//Instantiate & append the metric.
		MetricJOBLTS_ENDED m(jobInfo, playlistid);
		APPEND_METRIC_FLUSH( m, true );
		CNetworkTelemetry::SessionMatchEnd( );
	}

	void   CommandPlayStatsJobLtsRoundEnd(const char* creator, const char* matchId, JobLTSRoundInfo* info, const char* playlistid)
	{
		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s - PLAYSTATS_JOB_LTS_ROUND_END: Not in multiplayer game", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if (!NetworkInterface::IsGameInProgress())
			return;

		scriptAssertf(creator, "%s:PLAYSTATS_JOB_LTS_ROUND_END - NULL creator.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!creator)
			return;
		scriptAssertf(ustrlen(creator)<=MetricPlayStat::MAX_STRING_LENGTH, "%s - PLAYSTATS_JOB_LTS_ROUND_END: Invalid creator string size %s, max size is %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), creator, MetricPlayStat::MAX_STRING_LENGTH);

		scriptAssertf(matchId, "%s:PLAYSTATS_JOB_LTS_ROUND_END - NULL matchId.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!matchId)
			return;
		scriptAssertf(ustrlen(matchId)<=MetricPlayStat::MAX_STRING_LENGTH, "%s - PLAYSTATS_JOB_LTS_ROUND_END: Invalid matchId string size %s, max size is %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), matchId, MetricPlayStat::MAX_STRING_LENGTH);

		scriptAssertf(info, "%s:PLAYSTATS_JOB_LTS_ROUND_END - NULL info.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!info)
			return;

		sJobInfo jobInfo;

		jobInfo.m_dataU64[jobInfo.m_u64used] = ((u64)info->m_1stHashMac.Int) << 32;
		jobInfo.m_dataU64[jobInfo.m_u64used] |= info->m_1stPosixTime.Int & 0xFFFFFFFF;
		++jobInfo.m_u64used; gnetAssert(jobInfo.m_u64used <= sJobInfo::NUM_U64);
		jobInfo.m_dataU64[jobInfo.m_u64used] = ((u64)info->m_playlistHashMac.Int) << 32;
		jobInfo.m_dataU64[jobInfo.m_u64used] |= info->m_playlistPosixTime.Int & 0xFFFFFFFF;
		++jobInfo.m_u64used; gnetAssert(jobInfo.m_u64used <= sJobInfo::NUM_U64);

		s64 cashStart = s_cashStart;
		s_cashStart = 0;
		s64 cashEnd = MoneyInterface::GetVCWalletBalance() + MoneyInterface::GetVCBankBalance();
		jobInfo.m_dataU64[jobInfo.m_u64used] = cashStart;
		++jobInfo.m_u64used; gnetAssert(jobInfo.m_u64used <= sJobInfo::NUM_U64);
		jobInfo.m_dataU64[jobInfo.m_u64used] = cashEnd;
		++jobInfo.m_u64used; gnetAssert(jobInfo.m_u64used <= sJobInfo::NUM_U64);

		jobInfo.m_dataINTS[jobInfo.m_intsused] = info->m_roundNumber.Int; 
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = info->m_roundsplayed.Int; 
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = info->m_Cash.Int; 
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = info->m_BetCash.Int; 
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = info->m_CashStart.Int; 
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = info->m_CashEnd.Int; 
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = info->m_MissionLaunch.Int; 
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(info->m_outfitChoiceType.Int >= 0) ? info->m_outfitChoiceType.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(info->m_outfitStyle.Int >= -1) ? info->m_outfitStyle.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);

		jobInfo.m_dataU32[jobInfo.m_u32used] = fwTimer::GetSystemTimeInMilliseconds() - CNetworkTelemetry::GetMatchStartTime();
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = (u32)StatsInterface::GetUInt64Stat(STAT_TOTAL_PLAYING_TIME.GetHash());;
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = (u32)info->m_XP.Int; 
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = (u32)info->m_Kills.Int; 
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = (u32)info->m_Deaths.Int; 
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = (u32)info->m_Suicides.Int; 
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = (u32)info->m_Rank.Int; 
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = (u32)info->m_CrewId.Int; 
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = (u32)info->m_betWon.Int; 
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = (u32)info->m_TeamId.Int; 
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = (u32)info->m_matchResult.Int; 
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = (u32)info->m_jpEarned.Int; 
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = (u32)info->m_playlistID.Int; 
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);


		jobInfo.m_dataBOOLEANS[jobInfo.m_boolused] = (info->m_leftInProgress.Int != 0) ? true : false;
		++jobInfo.m_boolused; gnetAssert(jobInfo.m_boolused <= sJobInfo::NUM_BOOLS);

		jobInfo.m_MatchCreator[0] = '\0';
		safecpy(jobInfo.m_MatchCreator,  creator, COUNTOF(jobInfo.m_MatchCreator));

		jobInfo.m_UniqueMatchId[0] = '\0';
		safecpy(jobInfo.m_UniqueMatchId, matchId, COUNTOF(jobInfo.m_UniqueMatchId));

		//Instantiate & append the metric.
		MetricJOBLTS_ROUND_ENDED m(jobInfo, playlistid);
		APPEND_METRIC( m );
	}

	class MetricQUICKFIX_TOOL : public MetricPlayStat
	{
		RL_DECLARE_METRIC(QUICKFIX_TOOL, TELEMETRY_CHANNEL_AMBIENT, LOGLEVEL_HIGH_PRIORITY);

		static const u32 TEXT_SIZE = MetricPlayStat::MAX_STRING_LENGTH;

	private:
		char m_itemname[TEXT_SIZE];
		int m_element;

	public:
		explicit MetricQUICKFIX_TOOL(const int element, const char* itemname) : m_element(element)
		{
			m_itemname[0] = '\0';
			if(statVerify(itemname))
			{
				formatf(m_itemname, COUNTOF(m_itemname), itemname);
			}
		}

		virtual bool Write(RsonWriter* rw) const
		{
			return MetricPlayStat::Write(rw) 
				&& rw->WriteInt("element", m_element) 
				&& rw->WriteString("item", m_itemname);
		}
	};

	void   CommandPlayStatsQuickFixTool(const int element, const char* ItemName)
	{
		scriptAssertf(NetworkInterface::IsLocalPlayerOnline(), "%s - PLAYSTATS_QUICKFIX_TOOL: IsLocalPlayerOnline Failed", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!NetworkInterface::IsLocalPlayerOnline())
			return;

		scriptAssertf(ItemName, "%s:PLAYSTATS_QUICKFIX_TOOL - NULL ItemName.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!ItemName)
			return;
		scriptAssertf(ustrlen(ItemName)<=MetricPlayStat::MAX_STRING_LENGTH, "%s - PLAYSTATS_QUICKFIX_TOOL: Invalid ItemName string size %s, max size is %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), ItemName, MetricPlayStat::MAX_STRING_LENGTH);

		MetricQUICKFIX_TOOL m(element, ItemName);
		CNetworkTelemetry::AppendMetric(m);
	}

	void   CommandPlayStatsSetOnlineJoinType(const int type)
	{
		scriptAssertf(type>=0, "%s - PLAYSTATS_SET_JOIN_TYPE: invalid type (%d) must be >=0", CTheScripts::GetCurrentScriptNameAndProgramCounter(), type);
		scriptAssertf(type<=255, "%s - PLAYSTATS_SET_JOIN_TYPE: invalid type (%d) must be <=255", CTheScripts::GetCurrentScriptNameAndProgramCounter(), type);
		CNetworkTelemetry::SetOnlineJoinType((u8)type);
	}

	//////////////////////////////////////////////////////////////////////////
	// LEADERBOARDS COMMANDS

	const char* GET_RL_LEADERBOARD_TYPE(const rlLeaderboard2Type type)
	{
		if (type == RL_LEADERBOARD2_TYPE_PLAYER)
		{
			return "TYPE_PLAYER";
		}
		else if (type == RL_LEADERBOARD2_TYPE_CLAN)
		{
			return "TYPE_CLAN";
		}
		else if (type == RL_LEADERBOARD2_TYPE_CLAN_MEMBER)
		{
			return "TYPE_CLAN_MEMBER";
		}
		else if (type == RL_LEADERBOARD2_TYPE_GROUP)
		{
			return "TYPE_GROUP";
		}
		else if (type == RL_LEADERBOARD2_TYPE_GROUP_MEMBER)
		{
			return "TYPE_GROUP_MEMBER";
		}

		return "TYPE_UNKNOWN";
	}

	const Leaderboard* SCRIPT_VERIFY_LEADERBOARD(const int id, const char* ASSERT_ONLY(command))
	{
		const Leaderboard* lbConf = GAME_CONFIG_PARSER.GetLeaderboard(id);
		scriptAssertf(lbConf, "%s: %s - Invalid Leaderboard id=%d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), command, id);

		return lbConf;
	}

	void INIT_LEADERBOARD2_GROUP_SELECTOR(rlLeaderboard2GroupSelector& toGroupSelector, const LeaderboardGroupHandle& fromGroupSelector)
	{
		if(scriptVerify(fromGroupSelector.m_NumGroups.Int <= RL_LEADERBOARD2_MAX_GROUPS))
		{
			toGroupSelector.m_NumGroups = fromGroupSelector.m_NumGroups.Int;
			for (int i=0; i<toGroupSelector.m_NumGroups; i++)
			{
				safecpy(toGroupSelector.m_Group[i].m_Category, fromGroupSelector.m_Group[i].m_Category);
				safecpy(toGroupSelector.m_Group[i].m_Id, fromGroupSelector.m_Group[i].m_Id);
			}
		}
	}

	void INIT_GROUP_SELECTORS_FROM_SCRIPT_ARRAY(rlLeaderboard2GroupSelector* toGroupSelector, int& toNumGroups, LeaderboardGroupHandle* pScriptArray_LBGroups, const int numGroups, const char* ASSERT_ONLY(command))
	{
		if (scriptVerify(toGroupSelector) && scriptVerify(pScriptArray_LBGroups) && 0 < numGroups && numGroups <= MAX_LEADERBOARD_READ_GROUPS)
		{
			//Account for the padding at the front of the array that holds the length of the array.
			ASSERT_ONLY(int scriptNumOfItems = *((int*)(pScriptArray_LBGroups)));
			scriptAssertf(scriptNumOfItems >= numGroups, "%s:%s - Script array isn't big enough (%d) for the maxNumRows (%d)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), command, scriptNumOfItems, numGroups);

			//Now we bounce the pointer over one to point to the actual start of the array
			pScriptArray_LBGroups = (LeaderboardGroupHandle*)((int*)(pScriptArray_LBGroups) + 1);

			toNumGroups = numGroups;
			for (int rowIndex=0; rowIndex < numGroups; rowIndex++)
			{
				toGroupSelector[rowIndex].m_NumGroups = pScriptArray_LBGroups[rowIndex].m_NumGroups.Int;

				scriptAssertf(toGroupSelector[rowIndex].m_NumGroups > 0, "%s:%s - m_NumGroups for rowIndex=%d is 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), command, rowIndex);
				for (int i=0; i<toGroupSelector[rowIndex].m_NumGroups; i++)
				{
					scriptAssertf(strlen(pScriptArray_LBGroups[rowIndex].m_Group[i].m_Category) > 0, "%s:%s - m_Category for rowIndex=%d is empty.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), command, rowIndex);
					safecpy(toGroupSelector[rowIndex].m_Group[i].m_Category, pScriptArray_LBGroups[rowIndex].m_Group[i].m_Category);

					scriptAssertf(strlen(pScriptArray_LBGroups[rowIndex].m_Group[i].m_Id) > 0, "%s:%s - m_Id for rowIndex=%d is empty.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), command, rowIndex);
					safecpy(toGroupSelector[rowIndex].m_Group[i].m_Id, pScriptArray_LBGroups[rowIndex].m_Group[i].m_Id);
				}
			}
		}
	}

	bool INIT_GAMER_HANDLES_FROM_SCRIPT_ARRAY(rlGamerHandle* toHandles, scrArrayBaseAddr& fromHandles, const u32 numHandles ASSERT_ONLY(, const char* command))
	{
		u8* pData = scrArray::GetElements<u8>(fromHandles);

		if (pData && numHandles <= MAX_LEADERBOARD_ROWS)
		{
			for (u32 i=0; i<numHandles; i++, pData += GAMER_HANDLE_SIZE)
			{
				if(!CTheScripts::ImportGamerHandle(&toHandles[i], pData ASSERT_ONLY(, command)))
				{
					return false;
				}
			}

			return true;
		}

		return false;
	}

	bool INIT_CLAN_IDS_FROM_SCRIPT_ARRAY(rlClanId* toClanIds, LeaderboardClanIds* pArray_LBClanIds, const int nClanIds ASSERT_ONLY(, const char* command))
	{
		if (pArray_LBClanIds && nClanIds <= MAX_LEADERBOARD_CLAN_IDS)
		{
			//Account for the padding at the front of the array that holds the length of the array.
			ASSERT_ONLY(int scriptNumOfItems = *((int*)(pArray_LBClanIds)));
			scriptAssertf(scriptNumOfItems >= nClanIds, "%s:%s Script array isn't big enough (%d) for the maxNumRows (%d)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), command, scriptNumOfItems, nClanIds);

			//Now we bounce the pointer over one to point to the actual start of the array
			pArray_LBClanIds = (LeaderboardClanIds*)((int*)(pArray_LBClanIds) + 1);

			for (int rowIndex=0; rowIndex<nClanIds; rowIndex++)
			{
				if (pArray_LBClanIds[rowIndex].m_ClanIds.Int == RL_INVALID_CLAN_ID)
				{
					return false;
				}

				toClanIds[rowIndex] = (rlClanId)pArray_LBClanIds[rowIndex].m_ClanIds.Int;
			}
		}

		return true;
	}

	int CommandLeaderboardsGetNumberOfColumns(const int id, const int UNUSED_PARAM(type))
	{
		int columns = 0;

		const Leaderboard* lbConf = SCRIPT_VERIFY_LEADERBOARD(id, "LEADERBOARDS_GET_NUMBER_OF_COLUMNS");
		if (lbConf)
		{
			columns = lbConf->m_columns.GetCount();
		}

		return columns;
	}

	int CommandLeaderboardsGetColumnId(const int id, const int UNUSED_PARAM(type), int columnIndex)
	{
		int columnId = 0;

		const Leaderboard* lbConf = SCRIPT_VERIFY_LEADERBOARD(id, "LEADERBOARDS_GET_COLUMN_ID");
		if (lbConf && lbConf->m_columns.GetCount() > columnIndex && columnIndex >= 0)
		{
			columnId = lbConf->m_columns[columnIndex].m_aggregation.m_gameInput.m_inputId;
		}

		return columnId;
	}

	int CommandLeaderboardsGetColumnType(const int id, const int UNUSED_PARAM(type), int columnIndex)
	{
		int columnType = -1;

		const Leaderboard* lbConf = SCRIPT_VERIFY_LEADERBOARD(id, "LEADERBOARDS_GET_COLUMN_TYPE");
		if (lbConf && lbConf->m_columns.GetCount() > columnIndex && columnIndex >= 0)
		{
			const LeaderboardInput* input = GAME_CONFIG_PARSER.GetInputById( lbConf->m_columns[columnIndex].m_aggregation.m_gameInput.m_inputId );
			scriptAssertf(input, "%s, LEADERBOARDS_GET_COLUMN_TYPE - Leaderboard %s, Invalid input id=%d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), lbConf->GetName(), lbConf->m_columns[columnIndex].m_aggregation.m_gameInput.m_inputId);
			if (input)
			{
				columnType = input->m_dataType;
			}
		}

		return columnType;
	}

	bool CommandLeaderboardsCanRead(const int playerIndex)
	{
		bool ret = false;

		scriptAssertf(NetworkInterface::IsGameInProgress(), "%s: %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), "LEADERBOARDS_CAN_READ - Network game not in progress");

		const netPlayer* player = CTheScripts::FindNetworkPlayer(playerIndex);
		scriptAssertf(player, "%s: %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), "LEADERBOARDS_CAN_READ - Invalid Player Index.");

		if (player && NetworkInterface::IsGameInProgress())
		{
			const rlGamerHandle& handle = player->GetGamerInfo().GetGamerHandle();

			if (SCRIPT_VERIFY(handle.IsValid(), "LEADERBOARDS_CAN_READ - Invalid player Handle."))
			{
				CNetworkReadLeaderboards& readmgr = CNetwork::GetLeaderboardMgr().GetLeaderboardReadMgr();
				ret = readmgr.CanReadStatistics(handle);
			}
		}

		return ret;
	}

	void CommandLeaderboardsReadClearAll()
	{
		netLeaderboardRead* lbread = s_LeaderboardRead.Get();
		scriptAssertf(!lbread, "%s:LEADERBOARDS_READ_CLEAR_ALL - Leaderboard - Missing call to LEADERBOARDS2_READ_GET_ROW_DATA_END to Leaderboard Id = %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), lbread->GetId());
		if (lbread)
		{
			scriptDebugf1("LEADERBOARDS_READ_CLEAR_ALL - clear pointer.");
			s_LeaderboardRead = 0;
		}

		scriptDebugf1("LEADERBOARDS_READ_CLEAR_ALL - called.");
		CNetwork::GetLeaderboardMgr().GetLeaderboardReadMgr().ClearAll();
	}

	void CommandLeaderboardsReadClear(const int id, const int type, const int lbIndex)
	{
		netLeaderboardRead* lbread = s_LeaderboardRead.Get();
		if (lbread)
		{
			scriptAssertf(lbread->GetId() != (u32)id, "%s:LEADERBOARDS_READ_CLEAR - Leaderboard - Missing call to LEADERBOARDS2_READ_GET_ROW_DATA_END to Leaderboard Id = %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), lbread->GetId());
			if(lbread->GetId() != (u32)id)
			{
				scriptDebugf1("LEADERBOARDS_READ_CLEAR - clear pointer.");
				s_LeaderboardRead = 0;
			}
		}

		OUTPUT_ONLY( const Leaderboard* lbConf = GAME_CONFIG_PARSER.GetLeaderboard(id); )
		scriptDebugf1("LEADERBOARDS_READ_CLEAR - Leaderboard: id=%s,%d,type=%d,lbIndex=%d", lbConf ? lbConf->GetName() : "", id, type, lbIndex);

		CNetwork::GetLeaderboardMgr().GetLeaderboardReadMgr().ClearLeaderboardRead(id, (rlLeaderboard2Type)type, lbIndex);
	}

	bool CommandLeaderboardsReadAnyPending()
	{
		bool pending = false;

		const CNetworkReadLeaderboards& readmgr = CNetwork::GetLeaderboardMgr().GetLeaderboardReadMgr();
		const netLeaderboardReadMgr::netLeaderboardReadListNode* node = readmgr.GetHead();

		while (node && !pending)
		{
			const netLeaderboardRead* lb = node->Data;
			if (gnetVerify(lb))
			{
				pending = !lb->Finished();
			}

			node = node->GetNext();
		}

		return pending;
	}

	bool CommandLeaderboardsReadPending(const int id, const int type, const int lbIndex)
	{
		bool pending = false;

		const Leaderboard* lbConf = SCRIPT_VERIFY_LEADERBOARD(id, "LEADERBOARDS_READ_PENDING");
		if (lbConf)
		{
			scriptDebugf3("LEADERBOARDS_READ_PENDING - Leaderboard: id=%s,%d,type=%d,lbIndex=%d", lbConf->GetName(), id, type, lbIndex);

			CNetworkReadLeaderboards& readmgr = CNetwork::GetLeaderboardMgr().GetLeaderboardReadMgr();
			if (-1 == lbIndex)
			{
				const unsigned count = readmgr.CountOfLeaderboards(id, (rlLeaderboard2Type)type);

				for (unsigned i=0; i<count && !pending; i++)
				{
					const netLeaderboardRead* lb = readmgr.GetLeaderboardByType(id, (rlLeaderboard2Type)type, i);
					if (scriptVerify(lb))
					{
						pending = !lb->Finished();
					}
				}
			}
			else
			{
				const netLeaderboardRead* lb = readmgr.GetLeaderboardByType(id, (rlLeaderboard2Type)type, lbIndex);
				if (scriptVerify(lb))
				{
					pending = !lb->Finished();
				}
			}
		}

		return pending;
	}

	bool CommandLeaderboardsReadExists(const int id, const int type, const int lbIndex)
	{
		bool exists = false;

		const Leaderboard* lbConf = SCRIPT_VERIFY_LEADERBOARD(id, "LEADERBOARDS_READ_EXISTS");
		if (lbConf)
		{
			scriptDebugf3("LEADERBOARDS_READ_EXISTS - Leaderboard: id=%s,%d,type=%d,lbIndex=%d", lbConf->GetName(), id, type, lbIndex);

			CNetworkReadLeaderboards& readmgr = CNetwork::GetLeaderboardMgr().GetLeaderboardReadMgr();
			if (-1 == lbIndex)
			{
				const unsigned count = readmgr.CountOfLeaderboards(id, (rlLeaderboard2Type)type);

				for (unsigned i=0; i<count && !exists; i++)
				{
					const netLeaderboardRead* lb = readmgr.GetLeaderboardByType(id, (rlLeaderboard2Type)type, i);
					if (lb)
					{
						exists = true;
					}
				}
			}
			else
			{
				const netLeaderboardRead* lb = readmgr.GetLeaderboardByType(id, (rlLeaderboard2Type)type, lbIndex);
				if (lb)
				{
					exists = true;
				}
			}
		}

		return exists;
	}

	bool CommandLeaderboardsReadSuccessful(const int id, const int type, const int lbIndex)
	{
		const Leaderboard* lbConf = SCRIPT_VERIFY_LEADERBOARD(id, "LEADERBOARDS_READ_SUCCESSFUL");
		if (lbConf)
		{
			const netLeaderboardRead* lb = CNetwork::GetLeaderboardMgr().GetLeaderboardReadMgr().GetLeaderboardByType(id, (rlLeaderboard2Type)type, lbIndex);
			scriptAssertf(lb, "%s, LEADERBOARDS_READ_SUCCESSFUL - Leaderboard <id=%s,%d,type=%d,lbIndex=%d> is not being read.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), lbConf->GetName(), id, type, lbIndex);
			if (lb)
			{
				return lb->Finished() && lb->Suceeded();
			}
		}

		return false;
	}

	bool CommandLeaderboardsReadSuccessfulByHandle(const int id, const int type, int& handleData, const int lbIndex)
	{
		if(!SCRIPT_VERIFY_LEADERBOARD(id, "LEADERBOARDS_READ_SUCCESSFUL_BY_HANDLE"))
			return false;

		// fill in leaderboard handle
		rlGamerHandle hGamer;
		if(!CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "LEADERBOARDS_READ_SUCCESSFUL_BY_HANDLE")))
			return false;

		const netLeaderboardRead* lb = CNetwork::GetLeaderboardMgr().GetLeaderboardReadMgr().GetLeaderboardByType(id, (rlLeaderboard2Type)type, lbIndex);
		scriptAssertf(lb, "%s, LEADERBOARDS_READ_SUCCESSFUL - Leaderboard < id=%d, type=%d, lbIndex=%d > is not being read.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), id, type, lbIndex);
		if (lb && lb->IsGamerPresent(hGamer))
		{
			return lb->Finished() && lb->Suceeded();
		}

		return FALSE;
	}

	bool CommandLeaderboards2ReadSessionPlayersByRow(LeaderboardReadData* in_lbData, LeaderboardGroupHandle* pArray_LBGroups, const int numGroups)
	{
		scriptAssertf(in_lbData, "%s, LEADERBOARDS2_READ_SESSION_PLAYERS_BY_ROW - LeaderboardReadData is NULL", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!in_lbData)
			return false;

		const Leaderboard* lbConf = SCRIPT_VERIFY_LEADERBOARD(in_lbData->m_LeaderboardId.Int, "LEADERBOARDS2_READ_SESSION_PLAYERS_BY_ROW");
		if(!lbConf)
			return false;

		scriptAssertf(NetworkInterface::IsLocalPlayerOnline(), "%s, LEADERBOARDS2_READ_SESSION_PLAYERS_BY_ROW - Local Player is not Online", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!NetworkInterface::IsLocalPlayerOnline())
			return false;

		scriptDebugf1("LEADERBOARDS2_READ_SESSION_PLAYERS_BY_ROW - Add Leaderboard read for id=%s, type=%s", lbConf->GetName(), GET_RL_LEADERBOARD_TYPE((rlLeaderboard2Type)in_lbData->m_LeaderboardType.Int));

		rlLeaderboard2GroupSelector groupSelector;
		INIT_LEADERBOARD2_GROUP_SELECTOR(groupSelector, in_lbData->m_GroupHandle);

		rlLeaderboard2GroupSelector groupSelectors[MAX_LEADERBOARD_READ_GROUPS];
		int numberOfGroups = 0;
		INIT_GROUP_SELECTORS_FROM_SCRIPT_ARRAY(groupSelectors, numberOfGroups, pArray_LBGroups, numGroups, "LEADERBOARDS2_READ_SESSION_PLAYERS_BY_ROW");

		CNetworkReadLeaderboards& readmgr = CNetwork::GetLeaderboardMgr().GetLeaderboardReadMgr();

		return readmgr.ReadByGamer((u32)in_lbData->m_LeaderboardId.Int
									,(rlLeaderboard2Type)in_lbData->m_LeaderboardType.Int
									,groupSelector
									,(rlClanId)in_lbData->m_ClanId.Int
									,(u32)numberOfGroups, groupSelectors) != NULL;
	}

	bool CommandLeaderboards2ReadFriendsByRow(LeaderboardReadData* in_lbData, LeaderboardGroupHandle* pArray_LBGroups, const int numGroups, bool includeLocalPlayer, int friendStartIndex, int numFriends)
	{
		scriptAssertf(in_lbData, "%s, LEADERBOARDS2_READ_FRIENDS_BY_ROW - LeaderboardReadData is NULL", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!in_lbData)
			return false;

		const Leaderboard* lbConf = SCRIPT_VERIFY_LEADERBOARD(in_lbData->m_LeaderboardId.Int, "LEADERBOARDS2_READ_FRIENDS_BY_ROW");
		if(!lbConf)
			return false;

		scriptAssertf(NetworkInterface::IsLocalPlayerOnline(), "%s, LEADERBOARDS2_READ_FRIENDS_BY_ROW - Local Player is not Online", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!NetworkInterface::IsLocalPlayerOnline())
			return false;

		const u32 LB_MAX_NUM_FREINDS = 100;

		scriptAssertf(0 < numFriends, "%s, LEADERBOARDS2_READ_FRIENDS_BY_ROW - Number of friends %d must be > 0", CTheScripts::GetCurrentScriptNameAndProgramCounter(), numFriends);
		if (0 >= numFriends)
		{
			numFriends = LB_MAX_NUM_FREINDS;
		}

		scriptAssertf(numFriends <= LB_MAX_NUM_FREINDS, "%s, LEADERBOARDS2_READ_FRIENDS_BY_ROW - Number of friends %d must be <= %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), numFriends, LB_MAX_NUM_FREINDS);
		if (numFriends > LB_MAX_NUM_FREINDS)
		{
			numFriends = LB_MAX_NUM_FREINDS;
		}

		scriptDebugf1("LEADERBOARDS2_READ_FRIENDS_BY_ROW - Add Leaderboard read for id=%s, type=%s, numFriends=%d, startindex=%d", lbConf->GetName(), GET_RL_LEADERBOARD_TYPE((rlLeaderboard2Type)in_lbData->m_LeaderboardType.Int), numFriends, friendStartIndex);

		rlLeaderboard2GroupSelector groupSelector;
		INIT_LEADERBOARD2_GROUP_SELECTOR(groupSelector, in_lbData->m_GroupHandle);

		rlLeaderboard2GroupSelector groupSelectors[MAX_LEADERBOARD_READ_GROUPS];
		int numberOfGroups = 0;
		INIT_GROUP_SELECTORS_FROM_SCRIPT_ARRAY(groupSelectors, numberOfGroups, pArray_LBGroups, numGroups, "LEADERBOARDS2_READ_FRIENDS_BY_ROW");

		CNetworkReadLeaderboards& readmgr = CNetwork::GetLeaderboardMgr().GetLeaderboardReadMgr();

		return readmgr.ReadByFriends((u32)in_lbData->m_LeaderboardId.Int
										,(rlLeaderboard2Type)in_lbData->m_LeaderboardType.Int
										,groupSelector
										,(rlClanId)in_lbData->m_ClanId.Int
										,(u32)numberOfGroups, groupSelectors
										,includeLocalPlayer
										,friendStartIndex
										,numFriends) != NULL;
	}

	bool CommandLeaderboards2ReadByRow(LeaderboardReadData* in_lbData
										,scrArrayBaseAddr& array_LBGamerHandles, const int nGamerHandles
										,LeaderboardClanIds* pArray_LBClanIds, const int nClanIds
										,LeaderboardGroupHandle* pArray_LBGroups, const int numGroups)
	{
		scriptAssertf(in_lbData, "%s, LEADERBOARDS2_READ_BY_ROW - LeaderboardReadData is NULL", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!in_lbData)
			return false;

		const Leaderboard* lbConf = SCRIPT_VERIFY_LEADERBOARD(in_lbData->m_LeaderboardId.Int, "LEADERBOARDS2_READ_BY_ROW");
		if(!lbConf)
			return false;

		scriptAssertf(NetworkInterface::IsLocalPlayerOnline(), "%s, LEADERBOARDS2_READ_BY_ROW - Local Player is not Online", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!NetworkInterface::IsLocalPlayerOnline())
			return false;

		scriptDebugf1("LEADERBOARDS2_READ_BY_ROW - Add Leaderboard read for id=%s, type=%s", lbConf->GetName(), GET_RL_LEADERBOARD_TYPE((rlLeaderboard2Type)in_lbData->m_LeaderboardType.Int));

		rlLeaderboard2GroupSelector groupSelector;
		INIT_LEADERBOARD2_GROUP_SELECTOR(groupSelector, in_lbData->m_GroupHandle);

		rlLeaderboard2GroupSelector groupSelectors[MAX_LEADERBOARD_READ_GROUPS];
		int numberOfGroups = 0;
		INIT_GROUP_SELECTORS_FROM_SCRIPT_ARRAY(groupSelectors, numberOfGroups, pArray_LBGroups, numGroups, "LEADERBOARDS2_READ_BY_ROW");

		rlGamerHandle hGamerHandles[MAX_LEADERBOARD_ROWS];
		if (nGamerHandles > 0)
		{
			scriptAssertf(nGamerHandles <= MAX_LEADERBOARD_ROWS, "%s, LEADERBOARDS2_READ_BY_ROW - Invalid number of GamerHandles='%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), nGamerHandles);
			if (nGamerHandles > MAX_LEADERBOARD_ROWS)
				return false;

			if (!INIT_GAMER_HANDLES_FROM_SCRIPT_ARRAY(hGamerHandles, array_LBGamerHandles, (u32)nGamerHandles ASSERT_ONLY(, "LEADERBOARDS2_READ_BY_ROW")))
			{
				return false;
			}
		}

		rlClanId hClanIds[MAX_LEADERBOARD_CLAN_IDS];
		if (pArray_LBClanIds && nClanIds <= MAX_LEADERBOARD_ROWS)
		{
			if (!INIT_CLAN_IDS_FROM_SCRIPT_ARRAY(hClanIds, pArray_LBClanIds, nClanIds ASSERT_ONLY(, "LEADERBOARDS2_READ_BY_ROW")))
			{
				return false;
			}
		}

		CNetworkReadLeaderboards& readmgr = CNetwork::GetLeaderboardMgr().GetLeaderboardReadMgr();
		return readmgr.AddReadByRow(NetworkInterface::GetLocalGamerIndex()
									,(u32)in_lbData->m_LeaderboardId.Int
									,(rlLeaderboard2Type)in_lbData->m_LeaderboardType.Int
									,groupSelector
									,(rlClanId)in_lbData->m_ClanId.Int
									,(u32)nGamerHandles, hGamerHandles
									,(u32)nClanIds,  hClanIds 
									,(u32)numberOfGroups, groupSelectors) != NULL;
	}

	bool CommandLeaderboards2ReadByHandle(LeaderboardReadData* in_lbData, int& handleData)
	{
		scriptAssertf(in_lbData, "%s, LEADERBOARDS2_READ_BY_HANDLE - LeaderboardReadData is NULL", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!in_lbData)
			return false;

		const Leaderboard* lbConf = SCRIPT_VERIFY_LEADERBOARD(in_lbData->m_LeaderboardId.Int, "LEADERBOARDS2_READ_BY_HANDLE");
		if(!lbConf)
			return false;

		scriptAssertf(NetworkInterface::IsLocalPlayerOnline(), "%s, LEADERBOARDS2_READ_BY_HANDLE - Local Player is not Online", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!NetworkInterface::IsLocalPlayerOnline())
			return false;

		scriptDebugf1("LEADERBOARDS2_READ_BY_HANDLE - Add Leaderboard read for id=%s, type=%s", lbConf->GetName(), GET_RL_LEADERBOARD_TYPE((rlLeaderboard2Type)in_lbData->m_LeaderboardType.Int));

#if !__NO_OUTPUT
		{
			int* handleDataOut = (&handleData);
			scriptDebugf1("LEADERBOARDS2_READ_BY_HANDLE - Handle Data: ");
			for (int i=0; i<SCRIPT_GAMER_HANDLE_SIZE; i++)
				scriptDebugf1(".............. handleData[%d] = '%d'", i, handleDataOut[i]);
		}
#endif //!__NO_OUTPUT

		// fill in leaderboard handle
		rlGamerHandle hGamer;
		if(!CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "LEADERBOARDS_READ_BY_HANDLE")))
			return false;

		rlLeaderboard2GroupSelector groupSelector;
		INIT_LEADERBOARD2_GROUP_SELECTOR(groupSelector, in_lbData->m_GroupHandle);

		CNetworkReadLeaderboards& readmgr = CNetwork::GetLeaderboardMgr().GetLeaderboardReadMgr();
		return readmgr.AddReadByRow(NetworkInterface::GetLocalGamerIndex()
									,(u32)in_lbData->m_LeaderboardId.Int
									,(rlLeaderboard2Type)in_lbData->m_LeaderboardType.Int
									,groupSelector
									,(rlClanId)in_lbData->m_ClanId.Int
									,1
									,&hGamer
									,0, 0 
									,0, 0) != NULL;
	}

	bool CommandLeaderboards2ReadByRank(LeaderboardReadData* in_lbData, const int rankStart, const int numRows)
	{
		scriptAssertf(in_lbData, "%s, LEADERBOARDS2_READ_BY_RANK - LeaderboardReadData is NULL", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!in_lbData)
			return false;

		const Leaderboard* lbConf = SCRIPT_VERIFY_LEADERBOARD(in_lbData->m_LeaderboardId.Int, "LEADERBOARDS2_READ_BY_RANK");
		if(!lbConf)
			return false;

		scriptAssertf(NetworkInterface::IsLocalPlayerOnline(), "%s, LEADERBOARDS2_READ_BY_RANK - Local Player is not Online", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!NetworkInterface::IsLocalPlayerOnline())
			return false;

		scriptDebugf1("LEADERBOARDS2_READ_BY_RANK - Add Leaderboard read for id=%s, type=%s", lbConf->GetName(), GET_RL_LEADERBOARD_TYPE((rlLeaderboard2Type)in_lbData->m_LeaderboardType.Int));

		rlLeaderboard2GroupSelector groupSelector;
		INIT_LEADERBOARD2_GROUP_SELECTOR(groupSelector, in_lbData->m_GroupHandle);

		CNetworkReadLeaderboards& readmgr = CNetwork::GetLeaderboardMgr().GetLeaderboardReadMgr();
		return readmgr.AddReadByRank(NetworkInterface::GetLocalGamerIndex()
									,(u32)in_lbData->m_LeaderboardId.Int
									,(rlLeaderboard2Type)in_lbData->m_LeaderboardType.Int
									,groupSelector
									,(rlClanId)in_lbData->m_ClanId.Int
									,numRows
									,rankStart) != NULL;
	}

	bool CommandLeaderboards2ReadByRadius(LeaderboardReadData* in_lbData, const int radius, int& pivotGamerHandle)
	{
		scriptAssertf(in_lbData, "%s, LEADERBOARDS2_READ_BY_RADIUS - LeaderboardReadData is NULL", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!in_lbData)
			return false;

		const Leaderboard* lbConf = SCRIPT_VERIFY_LEADERBOARD(in_lbData->m_LeaderboardId.Int, "LEADERBOARDS2_READ_BY_RADIUS");
		if(!lbConf)
			return false;

		scriptAssertf(NetworkInterface::IsLocalPlayerOnline(), "%s, LEADERBOARDS2_READ_BY_RADIUS - Local Player is not Online", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!NetworkInterface::IsLocalPlayerOnline())
			return false;

		scriptDebugf1("LEADERBOARDS2_READ_BY_RADIUS - Add Leaderboard read for id=%s, type=%s", lbConf->GetName(), GET_RL_LEADERBOARD_TYPE((rlLeaderboard2Type)in_lbData->m_LeaderboardType.Int));

		rlLeaderboard2GroupSelector groupSelector;
		INIT_LEADERBOARD2_GROUP_SELECTOR(groupSelector, in_lbData->m_GroupHandle);

		rlGamerHandle hGamer;
		if ((rlLeaderboard2Type)in_lbData->m_LeaderboardType.Int    == RL_LEADERBOARD2_TYPE_PLAYER 
			|| (rlLeaderboard2Type)in_lbData->m_LeaderboardType.Int == RL_LEADERBOARD2_TYPE_CLAN_MEMBER 
			|| (rlLeaderboard2Type)in_lbData->m_LeaderboardType.Int == RL_LEADERBOARD2_TYPE_GROUP_MEMBER)
		{
			bool result = CTheScripts::ImportGamerHandle(&hGamer, pivotGamerHandle, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "LEADERBOARDS2_READ_BY_RADIUS"));
			if(!result)
			{
				scriptAssertf(0, "%s, LEADERBOARDS2_READ_BY_RADIUS - Invalid pivotGamerHandle", CTheScripts::GetCurrentScriptNameAndProgramCounter());
				return false;
			}
		}

		CNetworkReadLeaderboards& readmgr = CNetwork::GetLeaderboardMgr().GetLeaderboardReadMgr();

		return readmgr.AddReadByRadius(NetworkInterface::GetLocalGamerIndex()
										,(u32)in_lbData->m_LeaderboardId.Int
										,(rlLeaderboard2Type)in_lbData->m_LeaderboardType.Int
										,groupSelector
										,radius
										,hGamer
										,(rlClanId)in_lbData->m_ClanId.Int) != NULL;
	}


	bool CommandLeaderboards2ReadByScoreInt(LeaderboardReadData* in_lbData, const int pivotScore, const int numRows)
	{
		scriptAssertf(in_lbData, "%s, LEADERBOARDS2_READ_BY_SCORE_INT - LeaderboardReadData is NULL", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!in_lbData)
			return false;

		const Leaderboard* lbConf = SCRIPT_VERIFY_LEADERBOARD(in_lbData->m_LeaderboardId.Int, "LEADERBOARDS2_READ_BY_SCORE_INT");
		if(!lbConf)
			return false;

		scriptAssertf(NetworkInterface::IsLocalPlayerOnline(), "%s, LEADERBOARDS2_READ_BY_SCORE_INT - Local Player is not Online", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!NetworkInterface::IsLocalPlayerOnline())
			return false;

		scriptDebugf1("LEADERBOARDS2_READ_BY_RANK - Add Leaderboard read for id=%s, type=%s", lbConf->GetName(), GET_RL_LEADERBOARD_TYPE((rlLeaderboard2Type)in_lbData->m_LeaderboardType.Int));

		rlLeaderboard2GroupSelector groupSelector;
		INIT_LEADERBOARD2_GROUP_SELECTOR(groupSelector, in_lbData->m_GroupHandle);

		CNetworkReadLeaderboards& readmgr = CNetwork::GetLeaderboardMgr().GetLeaderboardReadMgr();
		return readmgr.AddReadByScoreInt(NetworkInterface::GetLocalGamerIndex()
			,(u32)in_lbData->m_LeaderboardId.Int
			,(rlLeaderboard2Type)in_lbData->m_LeaderboardType.Int
			,groupSelector
			,(rlClanId)in_lbData->m_ClanId.Int
			,numRows
			,pivotScore) != NULL;
	}

	bool CommandLeaderboards2ReadByScoreFloat(LeaderboardReadData* in_lbData, const float pivotScore, const int numRows)
	{
		scriptAssertf(in_lbData, "%s, LEADERBOARDS2_READ_BY_SCORE_FLOAT - LeaderboardReadData is NULL", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!in_lbData)
			return false;

		const Leaderboard* lbConf = SCRIPT_VERIFY_LEADERBOARD(in_lbData->m_LeaderboardId.Int, "LEADERBOARDS2_READ_BY_SCORE_FLOAT");
		if(!lbConf)
			return false;

		scriptAssertf(NetworkInterface::IsLocalPlayerOnline(), "%s, LEADERBOARDS2_READ_BY_SCORE_FLOAT - Local Player is not Online", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!NetworkInterface::IsLocalPlayerOnline())
			return false;

		scriptDebugf1("LEADERBOARDS2_READ_BY_RANK - Add Leaderboard read for id=%s, type=%s", lbConf->GetName(), GET_RL_LEADERBOARD_TYPE((rlLeaderboard2Type)in_lbData->m_LeaderboardType.Int));

		rlLeaderboard2GroupSelector groupSelector;
		INIT_LEADERBOARD2_GROUP_SELECTOR(groupSelector, in_lbData->m_GroupHandle);

		CNetworkReadLeaderboards& readmgr = CNetwork::GetLeaderboardMgr().GetLeaderboardReadMgr();
		return readmgr.AddReadByScoreFloat(NetworkInterface::GetLocalGamerIndex()
			,(u32)in_lbData->m_LeaderboardId.Int
			,(rlLeaderboard2Type)in_lbData->m_LeaderboardType.Int
			,groupSelector
			,(rlClanId)in_lbData->m_ClanId.Int
			,numRows
			,pivotScore) != NULL;
	}

	bool CommandLeaderboards2ReadByPlatform(LeaderboardReadData* in_lbData, const char* gamerHandler, const char* platform)
	{
		scriptAssertf(in_lbData, "%s, LEADERBOARDS2_READ_BY_PLAFORM - LeaderboardReadData is NULL", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!in_lbData)
			return false;

		const Leaderboard* lbConf = SCRIPT_VERIFY_LEADERBOARD(in_lbData->m_LeaderboardId.Int, "LEADERBOARDS2_READ_BY_PLAFORM");
		if(!lbConf)
			return false;

		scriptAssertf(NetworkInterface::IsLocalPlayerOnline(), "%s, LEADERBOARDS2_READ_BY_PLAFORM - Local Player is not Online", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!NetworkInterface::IsLocalPlayerOnline())
			return false;

		scriptAssertf(gamerHandler, "%s, LEADERBOARDS2_READ_BY_PLAFORM - gamerHandler is NULL.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(platform, "%s, LEADERBOARDS2_READ_BY_PLAFORM - platform is NULL.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		scriptDebugf1("LEADERBOARDS2_READ_BY_PLAFORM - Add Leaderboard read for id=%s, type=%s", lbConf->GetName(), GET_RL_LEADERBOARD_TYPE((rlLeaderboard2Type)in_lbData->m_LeaderboardType.Int));

		rlLeaderboard2GroupSelector groupSelector;
		INIT_LEADERBOARD2_GROUP_SELECTOR(groupSelector, in_lbData->m_GroupHandle);

		CNetworkReadLeaderboards& readmgr = CNetwork::GetLeaderboardMgr().GetLeaderboardReadMgr();
		return readmgr.AddReadByPlatform(NetworkInterface::GetLocalGamerIndex()
										,(u32)in_lbData->m_LeaderboardId.Int
										,groupSelector
										,gamerHandler
										,platform) != NULL;

	}

	static s64 s_colAttAlreadyCalculated = 0; //If bit is set means that the column result is calculated.
	static s64 s_colAttIsCondSet         = 0; //If bit is set means that the columns value has been accepted.

	int GetColumnIdIndex(const Leaderboard& lbConf, const int columnId)
	{
		int index = -1;

		for (int i=0; i<lbConf.m_columns.GetCount() && index == -1; i++)
		{
			if (lbConf.m_columns[i].m_id == columnId)
			{
				index = i;
				break;
			}
		}

		statAssert(index >= 0);
		if (index < 0) index = 0;

		return index;
	}

	bool  CALCULATE_INT_COLUMN_VALUE(LeaderboardPredictionRow* in_original
									,LeaderboardPredictionRow* in_from_match
									,LeaderboardPredictionRow* out_predicted
									,const eLeaderboardColumnAggregationType type
									,const int idx)
	{
		bool result = false;

		Assert(in_original->m_ColumnsBitSets.Int & (1<<idx));
		Assert(type != AggType_Ratio);

		if (type == AggType_Sum)
		{
			result = true;
			out_predicted->m_iColumnData[idx].Int = in_original->m_iColumnData[idx].Int + in_from_match->m_iColumnData[idx].Int;
		}
		else if (type == AggType_Max)
		{
			out_predicted->m_iColumnData[idx].Int = in_original->m_iColumnData[idx].Int;
			if (in_original->m_iColumnData[idx].Int < in_from_match->m_iColumnData[idx].Int)
			{
				result = true;
				out_predicted->m_iColumnData[idx].Int = in_from_match->m_iColumnData[idx].Int;
			}
		}
		else if (type == AggType_Min)
		{
			out_predicted->m_iColumnData[idx].Int = in_original->m_iColumnData[idx].Int;
			if (in_original->m_iColumnData[idx].Int > in_from_match->m_iColumnData[idx].Int)
			{
				result = true;
				out_predicted->m_iColumnData[idx].Int = in_from_match->m_iColumnData[idx].Int;
			}
		}
		else if (type == AggType_Last)
		{
			result = true;
			out_predicted->m_iColumnData[idx].Int = in_from_match->m_iColumnData[idx].Int;
		}

		return result;
	}

	bool CALCULATE_FLOAT_COLUMN_VALUE(LeaderboardPredictionRow* in_original
										,LeaderboardPredictionRow* in_from_match
										,LeaderboardPredictionRow* out_predicted
										,const eLeaderboardColumnAggregationType type
										,const int idx)
	{
		bool result = false;

		Assert(!(in_original->m_ColumnsBitSets.Int & (1<<idx)));
		Assert(type != AggType_Ratio);

		if (type == AggType_Sum)
		{
			result = true;
			out_predicted->m_fColumnData[idx].Float = in_original->m_fColumnData[idx].Float + in_from_match->m_fColumnData[idx].Float;
		}
		else if (type == AggType_Max)
		{
			out_predicted->m_fColumnData[idx].Float = in_original->m_fColumnData[idx].Float;
			if (in_original->m_fColumnData[idx].Float < in_from_match->m_fColumnData[idx].Float)
			{
				result = true;
				out_predicted->m_fColumnData[idx].Float = in_from_match->m_fColumnData[idx].Float;
			}
		}
		else if (type == AggType_Min)
		{
			out_predicted->m_fColumnData[idx].Float = in_original->m_fColumnData[idx].Float;
			if (in_original->m_fColumnData[idx].Float > in_from_match->m_fColumnData[idx].Float)
			{
				result = true;
				out_predicted->m_fColumnData[idx].Float = in_from_match->m_fColumnData[idx].Float;
			}
		}
		else if (type == AggType_Last)
		{
			result = true;
			out_predicted->m_fColumnData[idx].Float = in_from_match->m_fColumnData[idx].Float;
		}

		return result;
	}

	void CALCULATE_RATIO_COLUMN_VALUE(LeaderboardPredictionRow* out_predicted, const int ordinal, const int numerator, const int denominator)
	{
		Assert(!( out_predicted->m_ColumnsBitSets.Int & (1<<ordinal) ));

		float numeratorvalue = 0.0f;
		if (!(out_predicted->m_ColumnsBitSets.Int & (1<<numerator)))
		{
			numeratorvalue = out_predicted->m_fColumnData[numerator].Float;
		}
		else
		{
			numeratorvalue = (float)out_predicted->m_iColumnData[numerator].Int;
		}

		float denominatorvalue = 0.0f;
		if (!(out_predicted->m_ColumnsBitSets.Int & (1<<denominator)))
		{
			denominatorvalue = out_predicted->m_fColumnData[denominator].Float;
		}
		else
		{
			denominatorvalue = (float)out_predicted->m_iColumnData[denominator].Int;
		}

		if (denominatorvalue == 0.0f)
		{
			denominatorvalue = 1.0f;
		}

		out_predicted->m_fColumnData[ordinal].Float = numeratorvalue/denominatorvalue;
	}

	void RankPrediction(const Leaderboard& lbConf, LeaderboardPredictionRow* in_original, LeaderboardPredictionRow* in_from_match, LeaderboardPredictionRow* out_predicted, const int ordinal)
	{
		if (s_colAttAlreadyCalculated & (1LL<<ordinal))
			return;

		if (!scriptVerify(ordinal<lbConf.m_columns.GetCount()))
			return;

		const eLeaderboardColumnAggregationType type = lbConf.m_columns[ordinal].m_aggregation.m_type;
		if (AggType_Ratio == type && scriptVerify(lbConf.m_columns[ordinal].m_aggregation.m_columnInput.GetCount() == 2))
		{
			int numerator   = GetColumnIdIndex(lbConf, lbConf.m_columns[ordinal].m_aggregation.m_columnInput[0].m_columnId);
			int denominator = GetColumnIdIndex(lbConf, lbConf.m_columns[ordinal].m_aggregation.m_columnInput[1].m_columnId);

			if (!(s_colAttAlreadyCalculated & (1LL<<numerator)))
			{
				RankPrediction(lbConf, in_original, in_from_match, out_predicted, numerator);
			}

			if (!(s_colAttAlreadyCalculated & (1LL<<denominator)))
			{
				RankPrediction(lbConf, in_original, in_from_match, out_predicted, denominator);
			}

			s_colAttIsCondSet |= (1LL<<ordinal);

			CALCULATE_RATIO_COLUMN_VALUE(out_predicted, ordinal, numerator, denominator);

			scriptDebugf1(".........   Set Column=%s, type=%d, value=%f", lbConf.m_columns[ordinal].GetName(), type, out_predicted->m_fColumnData[ordinal].Float);
		}
		else
		{
			//Check if it is conditional
			int conditionalOnIndex = -1;
			if (lbConf.m_columns[ordinal].m_aggregation.m_conditionalOnColumnId != 0xFFFF)
			{
				conditionalOnIndex = GetColumnIdIndex(lbConf, lbConf.m_columns[ordinal].m_aggregation.m_conditionalOnColumnId);
			}

			if (conditionalOnIndex == -1 || (s_colAttIsCondSet & (1LL<<conditionalOnIndex)))
			{
				if (!(in_original->m_ColumnsBitSets.Int & (1LL<<ordinal)))
				{
					if(CALCULATE_FLOAT_COLUMN_VALUE(in_original, in_from_match, out_predicted, type, ordinal))
					{
						s_colAttIsCondSet |= (1LL<<ordinal);
						scriptDebugf1(".........   Set Column=%s, type=%d, value=%f", lbConf.m_columns[ordinal].GetName(), type, out_predicted->m_fColumnData[ordinal].Float);
					}
				}
				else
				{
					if(CALCULATE_INT_COLUMN_VALUE(in_original, in_from_match, out_predicted, type, ordinal))
					{
						s_colAttIsCondSet |= (1LL<<ordinal);
						scriptDebugf1(".........   Set Column=%s, type=%d, value=%d", lbConf.m_columns[ordinal].GetName(), type, out_predicted->m_iColumnData[ordinal].Int);
					}
				}
			}
			//Conditional on a Min/Max value that did not change value.
			else if(conditionalOnIndex > -1)
			{
				if (!(in_original->m_ColumnsBitSets.Int & (1<<ordinal)))
				{
					out_predicted->m_fColumnData[ordinal].Float = in_original->m_fColumnData[ordinal].Float;
					scriptDebugf1(".........   Set Column=%s, type=%d, value=%f", lbConf.m_columns[ordinal].GetName(), type, out_predicted->m_fColumnData[ordinal].Float);
				}
				else
				{
					out_predicted->m_iColumnData[ordinal].Int = in_original->m_iColumnData[ordinal].Int;
					scriptDebugf1(".........   Set Column=%s, type=%d, value=%d", lbConf.m_columns[ordinal].GetName(), type, out_predicted->m_iColumnData[ordinal].Int);
				}
			}
		}

		s_colAttAlreadyCalculated |= (1LL<<ordinal);
	}

	bool CommandLeaderboards2ReadRankPrediction(LeaderboardPredictionRow* in_original, LeaderboardPredictionRow* in_from_match, LeaderboardPredictionRow* out_predicted)
	{
		bool result = false;

		scriptAssertf(in_original, "%s:LEADERBOARDS2_READ_RANK_PREDICTION - Null Pointer in_original.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!in_original)
			return result;
		scriptAssertf(in_from_match, "%s:LEADERBOARDS2_READ_RANK_PREDICTION - Null Pointer in_from_match.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!in_from_match)
			return result;
		scriptAssertf(out_predicted, "%s:LEADERBOARDS2_READ_RANK_PREDICTION - Null Pointer in_from_match.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!out_predicted)
			return result;

		out_predicted->m_Id.Int             = in_original->m_Id.Int;
		out_predicted->m_ColumnsBitSets.Int = in_original->m_ColumnsBitSets.Int;
		out_predicted->m_NumColumns.Int     = in_original->m_NumColumns.Int;

		for (int i=0; i<LeaderboardPredictionRow::MAX_COLUMNS; i++)
		{
			out_predicted->m_iColumnData[i].Int   = -1;
			out_predicted->m_fColumnData[i].Float = -1.0f;
		}

		const Leaderboard* lbConf = SCRIPT_VERIFY_LEADERBOARD(in_original->m_Id.Int, "LEADERBOARDS2_READ_RANK_PREDICTION");
		if(!scriptVerify(lbConf))
			return result;

		scriptDebugf1("LEADERBOARDS2_READ_RANK_PREDICTION - id=%s, %d", lbConf->GetName(), in_original->m_Id.Int);
		scriptDebugf1("......... NumColumns=%d", in_original->m_NumColumns.Int);
		scriptDebugf1("..... ColumnsBitSets=%d", in_original->m_ColumnsBitSets.Int);

		scriptAssertf(lbConf->m_columns.GetCount() == in_original->m_NumColumns.Int, "%s:LEADERBOARDS2_READ_RANK_PREDICTION - Invalid number of columns %d, expected %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), in_original->m_NumColumns.Int, lbConf->m_columns.GetCount());
		if (lbConf->m_columns.GetCount() != in_original->m_NumColumns.Int)
			return result;

		s_colAttAlreadyCalculated = 0;
		s_colAttIsCondSet         = 0;

		scriptAssertf(lbConf->m_columns.GetCount() <= LeaderboardPredictionRow::MAX_COLUMNS, "%s:LEADERBOARDS2_READ_RANK_PREDICTION - Invalid number of columns %d, mas is %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), lbConf->m_columns.GetCount(), LeaderboardPredictionRow::MAX_COLUMNS);
		if (lbConf->m_columns.GetCount() > LeaderboardPredictionRow::MAX_COLUMNS)
			return result;

		//Setup all columns except ratio columns.
		for (int i=0; i<lbConf->m_columns.GetCount(); i++)
		{
			RankPrediction(*lbConf, in_original, in_from_match, out_predicted, i);
		}

		return true;
	}

#if __BANK

	void  TestCommandLeaderboards2ReadRankPrediction()
	{
		LeaderboardPredictionRow in_original;
		LeaderboardPredictionRow in_from_match;
		LeaderboardPredictionRow out_predicted;

		in_original.m_ColumnsBitSets.Int   = 73; //0 0100 1001
		in_from_match.m_ColumnsBitSets.Int = 73; //0 0100 1001

		in_original.m_Id.Int               = 203;
		in_from_match.m_Id.Int             = 203;
		in_original.m_NumColumns.Int       = 9;
		in_from_match.m_NumColumns.Int     = 9;
		int    istartvalue = 1;
		float  fstartvalue = 1.0f;
		for (int i=0; i<in_original.m_NumColumns.Int; i++)
		{
			in_original.m_iColumnData[i].Int = istartvalue; in_original.m_fColumnData[i].Float = fstartvalue;
			istartvalue += 1; fstartvalue += 1.0f;
			in_from_match.m_iColumnData[i].Int = istartvalue; in_from_match.m_fColumnData[i].Float = fstartvalue;
			istartvalue += 1; fstartvalue += 1.0f;
			out_predicted.m_iColumnData[i].Int = 0; out_predicted.m_fColumnData[i].Float = 0.0f;
		}
		scriptVerify( CommandLeaderboards2ReadRankPrediction(&in_original, &in_from_match, &out_predicted) );

		in_original.m_Id.Int               = 912;
		in_from_match.m_Id.Int             = 912;
		in_original.m_NumColumns.Int       = 10;
		in_from_match.m_NumColumns.Int     = 10;
		istartvalue = 1;
		fstartvalue = 1.0f;
		for (int i=0; i<in_original.m_NumColumns.Int; i++)
		{
			in_original.m_iColumnData[i].Int = istartvalue; in_original.m_fColumnData[i].Float = fstartvalue;
			istartvalue += 1; fstartvalue += 1.0f;
			in_from_match.m_iColumnData[i].Int = istartvalue; in_from_match.m_fColumnData[i].Float = fstartvalue;
			istartvalue += 1; fstartvalue += 1.0f;
			out_predicted.m_iColumnData[i].Int = 0; out_predicted.m_fColumnData[i].Float = 0.0f;
		}
		scriptVerify( CommandLeaderboards2ReadRankPrediction(&in_original, &in_from_match, &out_predicted) );
	}

#endif

	bool CommandLeaderboards2ReadGetRowDataStart(LeaderboardReadInfo* info)
	{
		bool ret = false;

		scriptAssertf(info, "%s, LEADERBOARDS2_READ_GET_ROW_DATA_START - NULL STRUCT LeaderboardReadInfo", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (info)
		{
			info->m_ReturnedRows.Int = 0;
			info->m_TotalRows.Int    = 0;
			info->m_LocalGamerRowNumber.Int = -1;

			const Leaderboard* lbConf = SCRIPT_VERIFY_LEADERBOARD(info->m_LeaderboardId.Int, "LEADERBOARDS2_READ_GET_ROW_DATA_START");
			if (lbConf)
			{
				const netLeaderboardRead* lb = CNetwork::GetLeaderboardMgr().GetLeaderboardReadMgr().GetLeaderboardByType(info->m_LeaderboardId.Int, (rlLeaderboard2Type)info->m_LeaderboardType.Int, info->m_LeaderboardReadIndex.Int);
				scriptAssertf(lb, "%s:LEADERBOARDS2_READ_GET_ROW_DATA_START - Invalid leaderboard - not found, id=%d, type=%d, lbIndex=%d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), info->m_LeaderboardId.Int, info->m_LeaderboardType.Int, info->m_LeaderboardReadIndex.Int);

				if (scriptVerify(lb) && scriptVerify(lb->Suceeded()))
				{
					scriptDebugf1("LEADERBOARDS2_READ_GET_ROW_DATA_START: lb id=\"%s : 0x%08x\", type=\"%s : %d\""
										,lbConf->GetName()
										,lb->GetId()
										,lb->GetReadTypeName()
										,lb->GetReadType());

					s_LeaderboardRead = const_cast< netLeaderboardRead* >( lb );

					info->m_ReturnedRows.Int = lb->GetNumRows();
					info->m_TotalRows.Int    = lb->GetNumTotalRows();

					for (int i=0; i<lb->GetNumRows() && info->m_LocalGamerRowNumber.Int==-1; i++)
					{
						const rage::rlLeaderboard2Row* row = lb->GetRow(i);
						if (row && row->m_GamerHandle.IsValid() && row->m_GamerHandle.IsLocal())
						{
							info->m_LocalGamerRowNumber.Int = i;
							break;
						}
					}

					//Check for rank player prediction
					if (info->m_LocalGamerRowNumber.Int == -1)
					{
						int rankcolindex = -1;
						for (int i=0; i<lbConf->m_columns.GetCount() && rankcolindex == -1; i++)
						{
							if (lbConf->m_columns[i].m_isRakingColumn)
							{
								rankcolindex = i;
								break;
							}
						}

						if (rankcolindex != -1)
						{
							const bool isInt64 = (RL_STAT_TYPE_INT32 == lb->GetColumnType(rankcolindex) 
														|| RL_STAT_TYPE_INT64 == lb->GetColumnType(rankcolindex));

							for (int i=0; i<lb->GetNumRows() && info->m_LocalGamerRowNumber.Int==-1; i++)
							{
								const rage::rlLeaderboard2Row* row = lb->GetRow(i);
								if (row && row->m_GamerHandle.IsValid() && !row->m_GamerHandle.IsLocal() && row->m_NumValues > rankcolindex)
								{
									if (isInt64)
									{
										if (static_cast< s64 >(info->m_LocalGamerRankInt.Int) >= row->m_ColumnValues[rankcolindex].Int64Val)
										{
											info->m_LocalGamerRowNumber.Int = i;
											break;
										}
									}
									else
									{
										if (static_cast< double >(info->m_LocalGamerRankFloat.Float) >= row->m_ColumnValues[rankcolindex].DoubleVal)
										{
											info->m_LocalGamerRowNumber.Int = i;
											break;
										}
									}
								}
							}
						}
					}

					ret = true;
				}
			}

		}

		return ret;
	}

	void CommandLeaderboards2ReadGetRowDataEnd()
	{
		scriptDebugf1("LEADERBOARDS2_READ_GET_ROW_DATA_END - called.");

		netLeaderboardRead* lbread = s_LeaderboardRead.Get();

		scriptAssertf(lbread, "%s:LEADERBOARDS2_READ_GET_ROW_DATA_END - Start get row data has not been started.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (lbread)
		{
			scriptDebugf1("LEADERBOARDS2_READ_GET_ROW_DATA_END - clear pointer.");

			s_LeaderboardRead = 0;
		}
	}

	void INIT_LEADERBOARD2_ROW_DATA(LeaderboardRowData& toLbRow, const rage::rlLeaderboard2Row* fromRow)
	{
		if (scriptVerify(fromRow))
		{
			OUTPUT_ONLY( char buf[RL_MAX_GAMER_HANDLE_CHARS]; )
			scriptDebugf1("........ GamerName   = \"%s\"", fromRow->m_GamerDisplayName);

			if (fromRow->m_GamerHandle.IsValid())
			{
				scriptDebugf1("........ GamerHandle = \"%s\"", fromRow->m_GamerHandle.ToString(buf, RL_MAX_GAMER_HANDLE_CHARS));
			}

			scriptDebugf1("........ Clan        = \"%d\", \"%s\", \"%s\"", (int)fromRow->m_ClanId, fromRow->m_ClanName, fromRow->m_ClanTag);
			scriptDebugf1("........ Rank        = \"%d\"", fromRow->m_Rank);
			scriptDebugf1("........ Groups      = \"%d\"", fromRow->m_GroupSelector.m_NumGroups);
			for (int i=0; i<fromRow->m_GroupSelector.m_NumGroups; i++)
			{
				scriptDebugf1(".............. Group[%d] = \"%s\"", i, fromRow->m_GroupSelector.m_Group[i].m_Category);
				scriptDebugf1(".............. Group[%d] = \"%s\"", i, fromRow->m_GroupSelector.m_Group[i].m_Id);
			}
			scriptDebugf1("........ Number Rows = \"%d\"", fromRow->m_NumValues);

			if (fromRow->m_GamerHandle.IsValid())
			{
				CTheScripts::ExportGamerHandle(&fromRow->m_GamerHandle, toLbRow.m_GamerHandle[0].Int, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "LEADERBOARDS2_READ_GET_ROW_DATA"));
			}

			sysMemSet(toLbRow.m_GamerName, 0, sizeof(toLbRow.m_GamerName));
			safecpy(toLbRow.m_GamerName, fromRow->m_GamerDisplayName);

#if __XENON
			if (fromRow->m_GamerHandle.IsValid() && fromRow->m_GamerHandle.IsLocal())
			{
				if(CGameWorld::GetMainPlayerInfo())
				{
					const char* localname = CGameWorld::GetMainPlayerInfo()->m_GamerInfo.GetName();
					if( stricmp(fromRow->m_GamerDisplayName, localname) != 0 )
					{
						sysMemSet(toLbRow.m_GamerName, 0, sizeof(toLbRow.m_GamerName));
						safecpy(toLbRow.m_GamerName, localname);
					}
				}
			}
#endif

			sysMemSet(toLbRow.m_ClanName, 0, sizeof(toLbRow.m_ClanName));
			safecpy(toLbRow.m_ClanName, fromRow->m_ClanName);

			sysMemSet(toLbRow.m_ClanTag, 0, sizeof(toLbRow.m_ClanTag));
			safecpy(toLbRow.m_ClanTag, fromRow->m_ClanTag);

			toLbRow.m_GroupSelector.m_NumGroups.Int = fromRow->m_GroupSelector.m_NumGroups;
			for (int i=0; i<toLbRow.m_GroupSelector.m_NumGroups.Int; i++)
			{
				safecpy(toLbRow.m_GroupSelector.m_Group[i].m_Category, fromRow->m_GroupSelector.m_Group[i].m_Category);
				safecpy(toLbRow.m_GroupSelector.m_Group[i].m_Id, fromRow->m_GroupSelector.m_Group[i].m_Id);
			}

			toLbRow.m_ClanId.Int          = (int)fromRow->m_ClanId;
			toLbRow.m_Rank.Int            = fromRow->m_Rank;
			toLbRow.m_NumColumnValues.Int = fromRow->m_NumValues;
		}
	}

	bool  CommandLeaderboards2ReadGetRowDataInfo(const int rowIndex, LeaderboardRowData* rowInfo)
	{
		netLeaderboardRead* lbread = s_LeaderboardRead.Get();
		scriptAssertf(lbread, "%s:LEADERBOARDS2_READ_GET_ROW_DATA_INFO - Get integer row data has not been started.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		bool result = false;

		if (!lbread)
			return result;

		if (!scriptVerify(rowInfo))
			return result;

		sysMemSet(rowInfo->m_GamerHandle, 0, COUNTOF(rowInfo->m_GamerHandle));
		sysMemSet(rowInfo->m_GamerName,   0, sizeof(rowInfo->m_GamerName));
		sysMemSet(rowInfo->m_ClanName,    0, sizeof(rowInfo->m_ClanName));
		sysMemSet(rowInfo->m_ClanTag,     0, sizeof(rowInfo->m_ClanTag));
		rowInfo->m_ClanId.Int = 0;
		rowInfo->m_Rank.Int   = 0;
		rowInfo->m_NumColumnValues.Int = 0;
		rowInfo->m_GroupSelector.m_NumGroups.Int = 0;

		const rage::rlLeaderboard2Row* row = lbread->GetRow(rowIndex);
		if (!scriptVerify(row))
			return result;

		result = true;

		scriptDebugf1(".... Row=\"%d\"", rowIndex);
		INIT_LEADERBOARD2_ROW_DATA(*rowInfo, row);

		return result;
	}

	int  CommandLeaderboards2ReadGetRowDataInt(const int rowIndex, const int columnIndex)
	{
		netLeaderboardRead* lbread = s_LeaderboardRead.Get();
		scriptAssertf(lbread, "%s:LEADERBOARDS2_READ_GET_ROW_DATA_INT - Get integer row data has not been started.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		int statValue = -1;

		if (!lbread)
			return statValue;

		const rage::rlLeaderboard2Row* row = lbread->GetRow(rowIndex);
		if (!scriptVerify(row))
			return statValue;

		const Leaderboard* lbconfig = GAME_CONFIG_PARSER.GetLeaderboard(lbread->GetId());
		scriptAssertf(lbconfig, "%s:LEADERBOARDS2_READ_GET_ROW_DATA_INT - Leaderboard id=%d is invalid, not found.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), lbread->GetId());
		if (!lbconfig)
			return statValue;

		scriptAssertf(columnIndex < row->m_NumValues, "%s:LEADERBOARDS_READ_GET_ROW_DATA_INT - Leaderboard=%s\
						 Invalid Column Index - Num Values=\"%d\", Column=\"%d\""
						 ,CTheScripts::GetCurrentScriptNameAndProgramCounter()
						 ,lbconfig->GetName()
						 ,row->m_NumValues
						 ,columnIndex);

		if (columnIndex < row->m_NumValues)
		{
			const u32 columnId = lbread->GetColumnId(columnIndex);

			for (int i=0; i<lbconfig->m_columns.GetCount(); i++)
			{
				if (columnId == lbconfig->m_columns[i].m_id)
				{
					switch(lbread->GetColumnType(columnIndex))
					{
					case RL_STAT_TYPE_INT32:
					case RL_STAT_TYPE_INT64:
						scriptDebugf1("........ stat ordinal=\"%d\", Column=\"%s:%d\"(%s), value=\"%" I64FMT "d\""
							,columnIndex
							,lbconfig->m_columns[i].GetName()
							,lbconfig->m_columns[i].m_id
							,lbread->GetColumnTypeName(columnIndex)
							,row->m_ColumnValues[columnIndex].Int64Val);

						statValue = (int)row->m_ColumnValues[columnIndex].Int64Val;
						break;

					default:
						statValue = (int)row->m_ColumnValues[columnIndex].DoubleVal;
						scriptAssertf(0, "%s:LEADERBOARDS_READ_GET_ROW_DATA_INT - Leaderboard=%s\
										 Invalid column TYPE - Column=\"%s:%d\"(%s)"
										 ,CTheScripts::GetCurrentScriptNameAndProgramCounter()
										 ,lbconfig->GetName()
										 ,lbconfig->m_columns[i].GetName()
										 ,lbconfig->m_columns[i].m_id
										 ,lbread->GetColumnTypeName(columnIndex));
						break;
					}

					return statValue;
				}
			}

			scriptAssertf(0, "%s:LEADERBOARDS_READ_GET_ROW_DATA_INT - Leaderboard=%s\
							 Column ID not found - Num Values=\"%d\", Column=\"%d:%d\""
							 ,CTheScripts::GetCurrentScriptNameAndProgramCounter()
							 ,lbconfig->GetName()
							 ,row->m_NumValues
							 ,columnIndex
							 ,columnId);
		}

		return statValue;
	}

	float  CommandLeaderboards2ReadGetRowDataFloat(const int rowIndex, const int columnIndex)
	{
		netLeaderboardRead* lbread = s_LeaderboardRead.Get();
		scriptAssertf(lbread, "%s:LEADERBOARDS_READ_GET_ROW_DATA_FLOAT - Get integer row data has not been started.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		float statValue = -1.0f;

		if (!lbread)
			return statValue;

		const Leaderboard* lbconfig = GAME_CONFIG_PARSER.GetLeaderboard(lbread->GetId());
		if (!scriptVerify(lbconfig))
			return statValue;

		const rage::rlLeaderboard2Row* row = lbread->GetRow(rowIndex);
		if (!scriptVerify(row))
			return statValue;

		scriptAssertf(columnIndex < row->m_NumValues, "%s:LEADERBOARDS_READ_GET_ROW_DATA_FLOAT - Leaderboard=%s\
													  Invalid Column Index - Num Values=\"%d\", Column=\"%d\""
													  ,CTheScripts::GetCurrentScriptNameAndProgramCounter()
													  ,lbconfig->GetName()
													  ,row->m_NumValues
													  ,columnIndex);

		if (columnIndex < row->m_NumValues)
		{
			const u32 columnId = lbread->GetColumnId(columnIndex);

			for (int i=0; i<lbconfig->m_columns.GetCount(); i++)
			{
				if (columnId == lbconfig->m_columns[i].m_id)
				{
					switch(lbread->GetColumnType(columnIndex))
					{
					case RL_STAT_TYPE_DOUBLE:
					case RL_STAT_TYPE_FLOAT:
						scriptDebugf1("........ stat ordinal=\"%d\", Column=\"%s:%d\"(%s), value=\"%f\""
										,columnIndex
										,lbconfig->m_columns[i].GetName()
										,lbconfig->m_columns[i].m_id
										,lbread->GetColumnTypeName(columnIndex)
										,row->m_ColumnValues[columnIndex].DoubleVal);

						statValue = (float)row->m_ColumnValues[columnIndex].DoubleVal;
						break;

					default:
						statValue = (float)row->m_ColumnValues[columnIndex].Int64Val;
						scriptAssertf(0, "%s:LEADERBOARDS_READ_GET_ROW_DATA_FLOAT - Leaderboard=%s\
										 Invalid column TYPE - Column=\"%s:%d\"(%s)"
										 ,CTheScripts::GetCurrentScriptNameAndProgramCounter()
										 ,lbconfig->GetName()
										 ,lbconfig->m_columns[i].GetName()
										 ,lbconfig->m_columns[i].m_id
										 ,lbread->GetColumnTypeName(columnIndex));
						break;
					}

					return statValue;
				}
			}

			scriptAssertf(0, "%s:LEADERBOARDS_READ_GET_ROW_DATA_FLOAT - Leaderboard=%s\
							 Column ID not found - Num Values=\"%d\", Column=\"%d:%d\""
							 ,CTheScripts::GetCurrentScriptNameAndProgramCounter()
							 ,lbconfig->GetName()
							 ,row->m_NumValues
							 ,columnIndex
							 ,columnId);

		}

		return statValue;
	}

	void CommandLeaderboardsWriteAddColumn(const int inputId, const int ivalue, const float fvalue)
	{
		if (!rlRos::HasPrivilege(NetworkInterface::GetLocalGamerIndex(), RLROS_PRIVILEGEID_LEADERBOARD_WRITE))
		{
			scriptErrorf("Can not make any leaderboard submissions - RLROS_PRIVILEGEID_LEADERBOARD_WRITE.");
			return;
		}

		netLeaderboardWrite* lbwrite = s_LeaderboardWrite.Get();
		scriptAssertf(lbwrite, "%s, LEADERBOARDS_WRITE_ADD_COLUMN - No valid leaderboard to write.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if (lbwrite)
		{
			const Leaderboard* lbConf = GAME_CONFIG_PARSER.GetLeaderboard( lbwrite->m_LeaderboardId );
			scriptAssertf(lbConf, "%s, LEADERBOARDS_WRITE_ADD_COLUMN - Invalid leaderboard id=%d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), lbwrite->m_LeaderboardId);
			if (!lbConf)
				return;

			scriptAssertf(lbwrite->PendingFlush(), "%s, LEADERBOARDS_WRITE_ADD_COLUMN - leaderboard %s, is not pending FLUSH.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), lbConf->GetName());
			if (!lbwrite->PendingFlush())
				return;

			const LeaderboardInput* input = GAME_CONFIG_PARSER.GetInputById(inputId);
			scriptAssertf(input, "%s, LEADERBOARDS_WRITE_ADD_COLUMN - Leaderboard %s, Invalid input id=%d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), lbConf->GetName(), inputId);
			if (!input)
				return;

			scriptAssertf(!lbwrite->ColumnExists(inputId), "%s, LEADERBOARDS_WRITE_ADD_COLUMN - Leaderboard %s, Input id=%s(%d) Already exists.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), lbConf->GetName(), input->GetName(), input->m_id);
			if (lbwrite->ColumnExists(inputId))
				return;

			int columnIdx = -1;
			for (int i=0; i<lbConf->m_columns.GetCount() && columnIdx < 0; i++)
			{
				if (inputId == lbConf->m_columns[i].m_aggregation.m_gameInput.m_inputId)
				{
					columnIdx = i;
					scriptAssertf(lbConf->m_columns[i].m_aggregation.m_type != AggType_Ratio, "%s, LEADERBOARDS_WRITE_ADD_COLUMN - Leaderboard %s, Input id=%s(%d), Invalid Aggregation type - RATIO.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), lbConf->GetName(), input->GetName(), input->m_id);
				}
			}

			scriptAssertf(columnIdx >= 0, "%s, LEADERBOARDS_WRITE_ADD_COLUMN - Leaderboard %s, Input id=%s(%d), Column Index not Found.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), lbConf->GetName(), input->GetName(), input->m_id);
			if (columnIdx < 0)
				return;

			scriptAssertf(lbConf->m_columns[columnIdx].m_aggregation.m_type != AggType_Ratio, "%s, LEADERBOARDS_WRITE_ADD_COLUMN - Leaderboard %s, column index=%d with Input id=%s(%d), Invalid Aggregation type - RATIO.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), lbConf->GetName(), columnIdx, input->GetName(), input->m_id);
			if (lbConf->m_columns[columnIdx].m_aggregation.m_type == AggType_Ratio)
				return;

			rlStatValue statValue;
			switch(input->m_dataType)
			{
			case InputDataType_int: 
			case InputDataType_long: 
				statValue.Int64Val = ivalue;
				break;
			case InputDataType_double: 
				statValue.DoubleVal = fvalue;
				break;
			default: 
				scriptAssertf(0, "%s, LEADERBOARDS_WRITE_ADD_COLUMN - Leaderboard %s, Invalid column data Type, index=%d with Input id=%s(%d), Invalid Aggregation type.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), lbConf->GetName(), columnIdx, input->GetName(), input->m_id);
				break;
			}

			lbwrite->AddColumn(statValue, inputId);
		}
	}

	void CommandLeaderboardsWriteAddColumnLong(const int inputId, const int value_a, const int value_b)
	{
		if (!rlRos::HasPrivilege(NetworkInterface::GetLocalGamerIndex(), RLROS_PRIVILEGEID_LEADERBOARD_WRITE))
		{
			scriptErrorf("Can not make any leaderboard submissions - RLROS_PRIVILEGEID_LEADERBOARD_WRITE.");
			return;
		}

		netLeaderboardWrite* lbwrite = s_LeaderboardWrite.Get();
		scriptAssertf(lbwrite, "%s, LEADERBOARDS_WRITE_ADD_COLUMN_LONG - No valid leaderboard to write.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if (lbwrite)
		{
			const Leaderboard* lbConf = GAME_CONFIG_PARSER.GetLeaderboard( lbwrite->m_LeaderboardId );
			scriptAssertf(lbConf, "%s, LEADERBOARDS_WRITE_ADD_COLUMN_LONG - Invalid leaderboard id=%d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), lbwrite->m_LeaderboardId);
			if (!lbConf)
				return;

			scriptAssertf(lbwrite->PendingFlush(), "%s, LEADERBOARDS_WRITE_ADD_COLUMN_LONG - leaderboard %s, is not pending FLUSH.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), lbConf->GetName());
			if (!lbwrite->PendingFlush())
				return;

			const LeaderboardInput* input = GAME_CONFIG_PARSER.GetInputById(inputId);
			scriptAssertf(input, "%s, LEADERBOARDS_WRITE_ADD_COLUMN_LONG - Leaderboard %s, Invalid input id=%d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), lbConf->GetName(), inputId);
			if (!input)
				return;

			scriptAssertf(!lbwrite->ColumnExists(inputId), "%s, LEADERBOARDS_WRITE_ADD_COLUMN_LONG - Leaderboard %s, Input id=%s(%d) Already exists.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), lbConf->GetName(), input->GetName(), input->m_id);
			if (lbwrite->ColumnExists(inputId))
				return;

			int columnIdx = -1;
			for (int i=0; i<lbConf->m_columns.GetCount() && columnIdx < 0; i++)
			{
				if (inputId == lbConf->m_columns[i].m_aggregation.m_gameInput.m_inputId)
				{
					columnIdx = i;
					scriptAssertf(lbConf->m_columns[i].m_aggregation.m_type != AggType_Ratio, "%s, LEADERBOARDS_WRITE_ADD_COLUMN_LONG - Leaderboard %s, Input id=%s(%d), Invalid Aggregation type - RATIO.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), lbConf->GetName(), input->GetName(), input->m_id);
				}
			}

			scriptAssertf(columnIdx >= 0, "%s, LEADERBOARDS_WRITE_ADD_COLUMN_LONG - Leaderboard %s, Input id=%s(%d), Column Index not Found.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), lbConf->GetName(), input->GetName(), input->m_id);
			if (columnIdx < 0)
				return;

			scriptAssertf(lbConf->m_columns[columnIdx].m_aggregation.m_type != AggType_Ratio, "%s, LEADERBOARDS_WRITE_ADD_COLUMN_LONG - Leaderboard %s, column index=%d with Input id=%s(%d), Invalid Aggregation type - RATIO.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), lbConf->GetName(), columnIdx, input->GetName(), input->m_id);
			if (lbConf->m_columns[columnIdx].m_aggregation.m_type == AggType_Ratio)
				return;

			rlStatValue statValue;
			switch(input->m_dataType)
			{
			case InputDataType_int:
			case InputDataType_long:
				statValue.Int64Val = ((u64)value_a) << 32;
				statValue.Int64Val |= value_b & 0xFFFFFFFF;
				break;
			case InputDataType_double:
			default: 
				scriptAssertf(0, "%s, LEADERBOARDS_WRITE_ADD_COLUMN_LONG - Leaderboard %s, Invalid column data Type, index=%d with Input id=%s(%d), Invalid Aggregation type.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), lbConf->GetName(), columnIdx, input->GetName(), input->m_id);
				break;
			}

			lbwrite->AddColumn(statValue, inputId);
		}
	}

	bool CommandLeaderboards2WriteLeaderboardDataForEventType(const LeaderboardUpdateData* in_lbData, const char* activeEventType)
	{
		if (!rlRos::HasPrivilege(NetworkInterface::GetLocalGamerIndex(), RLROS_PRIVILEGEID_LEADERBOARD_WRITE))
		{
			scriptErrorf("Can not make any leaderboard submissions - RLROS_PRIVILEGEID_LEADERBOARD_WRITE.");
			return false;
		}

		scriptAssertf(in_lbData, "%s, LEADERBOARDS2_WRITE_DATA - LeaderboardUpdateData is NULL", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (in_lbData)
		{
			const Leaderboard* lbConf = GAME_CONFIG_PARSER.GetLeaderboard(in_lbData->m_LeaderboardId.Int);
			scriptAssertf(lbConf, "%s, LEADERBOARDS2_WRITE_DATA - INVALID Leadeboard ID=\"%d\"", CTheScripts::GetCurrentScriptNameAndProgramCounter(), in_lbData->m_LeaderboardId.Int);

			if (lbConf)
			{
				scriptAssertf(NetworkInterface::IsLocalPlayerOnline(), "%s, LEADERBOARDS2_WRITE_DATA - Local Player is not Online", CTheScripts::GetCurrentScriptNameAndProgramCounter());
				if (NetworkInterface::IsLocalPlayerOnline())
				{
					rlLeaderboard2GroupSelector groupSelector;
					groupSelector.m_NumGroups = in_lbData->m_GroupHandle.m_NumGroups.Int;
					for (int i=0; i<groupSelector.m_NumGroups; i++)
					{
						safecpy(groupSelector.m_Group[i].m_Category, in_lbData->m_GroupHandle.m_Group[i].m_Category);
						safecpy(groupSelector.m_Group[i].m_Id, in_lbData->m_GroupHandle.m_Group[i].m_Id);
					}

					//If valid add the Social Club event category info.
					if (SocialClubEventMgr::Get().IsEventActive(activeEventType) && statVerify(groupSelector.m_NumGroups < RL_LEADERBOARD2_MAX_GROUPS))
					{
						for (int i=0; i<lbConf->m_categoryGroups.GetCount(); i++)
						{
							// If the CategoryPermutation contains 'ScEvent'
							if (lbConf->m_categoryGroups[i].m_value.IndexOf("ScEvent") >= 0)
							{
								snprintf(groupSelector.m_Group[groupSelector.m_NumGroups].m_Category, RL_LEADERBOARD2_CATEGORY_MAX_CHARS, "%s", "ScEvent");
								snprintf(groupSelector.m_Group[groupSelector.m_NumGroups].m_Id, RL_LEADERBOARD2_GROUP_ID_MAX_CHARS, "%d", SocialClubEventMgr::Get().GetActiveEventCode(activeEventType));
								++groupSelector.m_NumGroups;

								scriptDebugf1("Adding 'ScEvent' group for event code %d  (type %s)", SocialClubEventMgr::Get().GetActiveEventCode(activeEventType), activeEventType?activeEventType:"NULL");

								break;
							}
						}
					}

					CNetworkWriteLeaderboards& writemgr = CNetwork::GetLeaderboardMgr().GetLeaderboardWriteMgr();
					s_LeaderboardWrite = writemgr.AddLeaderboard(in_lbData->m_LeaderboardId.Int, groupSelector, in_lbData->m_ClanId.Int);
				}
			}
		}

		return (s_LeaderboardWrite.Get() != NULL);
	}

	bool CommandLeaderboards2WriteLeaderboardData(const LeaderboardUpdateData* in_lbData)
	{
		return CommandLeaderboards2WriteLeaderboardDataForEventType(in_lbData, NULL);
	}

	void  CommandLeaderboardsClearCacheData()
	{
		scriptDebugf1("LEADERBOARDS_CLEAR_CACHE_DATA");
		CNetwork::GetLeaderboardMgr().GetLeaderboardReadMgr().ClearCachedDisplayData();
	}

	void  CommandLeaderboardsClearCachedDisplayDataById(const int id)
	{
		scriptDebugf1("LEADERBOARDS_CLEAR_CACHE_DATA_ID");
		CNetwork::GetLeaderboardMgr().GetLeaderboardReadMgr().ClearCachedDisplayDataById(id);
	}

	bool CommandLeaderboardsCacheDataRow(LeaderboardCachedDisplayData* info)
	{
		bool result = false;

		scriptAssertf(info, "%s:LEADERBOARDS_CACHE_DATA_ROW - NULL data.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!info)
			return result;

		CLeaderboardScriptDisplayData::sRowData row;
		sysMemSet(row.m_GamerName, 0, sizeof(row.m_GamerName));
		safecpy(row.m_GamerName, info->m_GamerName);

		int& handleData = info->m_GamerHandle.m_GamerHandle[0].Int;
		if (!CTheScripts::ImportGamerHandle(&row.m_GamerHandle, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "LEADERBOARDS_CACHE_DATA_ROW")))
			return result;

		scriptAssertf(row.m_GamerHandle.IsValid(), "%s:LEADERBOARDS_CACHE_DATA_ROW - GamerHandle not valid.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!row.m_GamerHandle.IsValid())
			return result;

		sysMemSet(row.m_CoDriverName, 0, sizeof(row.m_CoDriverName));
		row.m_CoDriverHandle.Clear();
		if ( ustrlen(info->m_CoDriverName) > 0)
		{
			safecpy(row.m_CoDriverName, info->m_CoDriverName);

			handleData = info->m_CoDriverHandle.m_GamerHandle[0].Int;
			if (!CTheScripts::ImportGamerHandle(&row.m_CoDriverHandle, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "LEADERBOARDS_CACHE_DATA_ROW")))
				return result;

			scriptAssertf(row.m_CoDriverHandle.IsValid(), "%s:LEADERBOARDS_CACHE_DATA_ROW - CoDriverHandle not valid.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			if (!row.m_CoDriverHandle.IsValid())
				return result;
		}

		row.m_CustomVehicle = info->m_CustomVehicle.Int!=0;
		row.m_Rank = info->m_Rank.Int;

		scriptAssertf(info->m_NumColumns.Int <= LeaderboardCachedDisplayData::MAX_COLUMNS, "%s:LEADERBOARDS_CACHE_DATA_ROW - NumColumns %d > %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), info->m_NumColumns.Int, LeaderboardCachedDisplayData::MAX_COLUMNS);
		if (info->m_NumColumns.Int > LeaderboardCachedDisplayData::MAX_COLUMNS)
			return result;

		row.m_RowFlags = info->m_RowFlags.Int;
		
		scriptDebugf1("LEADERBOARDS_CACHE_DATA_ROW - id=%d", info->m_Id.Int);
		scriptDebugf1(".......... GamerName=%s", row.m_GamerName);
		scriptDebugf1("....... CoDriverName=%s", row.m_CoDriverName);
		scriptDebugf1("............... Rank=%d", row.m_Rank);
		scriptDebugf1("..... ColumnsBitSets=%d", info->m_ColumnsBitSets.Int);
		scriptDebugf1("........... RowFlags=%d", row.m_RowFlags);

		for (int i=0; i<info->m_NumColumns.Int; i++)
		{
			CLeaderboardScriptDisplayData::sStatValue value;

			if (info->m_ColumnsBitSets.Int & (1<<i))
			{
				value.m_IntVal = info->m_iColumnData[i].Int;
				scriptDebugf1(".......... Column %d=%d,%d", i, value.m_IntVal, info->m_iColumnData[i].Int);
			}
			else
			{
				value.m_FloatVal = info->m_fColumnData[i].Float;
				scriptDebugf1(".......... Column %d=%f,%f", i, value.m_FloatVal, info->m_fColumnData[i].Float);
			}

			row.m_Columns.Push(value);
		}

		scriptDebugf1("......... NumColumns=%d", row.m_Columns.GetCount());

		result = CNetwork::GetLeaderboardMgr().GetLeaderboardReadMgr().CacheScriptDisplayData(info->m_Id.Int, info->m_ColumnsBitSets.Int, row);

		return result;
	}

	bool  CommandLeaderboardsGetCacheExists(const int id)
	{
		const CNetworkReadLeaderboards::LeaderboardScriptDisplayDataList& data = CNetwork::GetLeaderboardMgr().GetLeaderboardReadMgr().GetDisplayData();

		for (int i=0; i<data.GetCount(); i++)
		{
			if (id == data[i].m_Id)
			{
				return true;
			}
		}

		return false;
	}

	int  CommandLeaderboardsGetCacheTime(const int id)
	{
		const CNetworkReadLeaderboards::LeaderboardScriptDisplayDataList& data = CNetwork::GetLeaderboardMgr().GetLeaderboardReadMgr().GetDisplayData();

		for (int i=0; i<data.GetCount(); i++)
		{
			if (id == data[i].m_Id)
			{
				return (sysTimer::GetSystemMsTime() - data[i].m_Time);
			}
		}

		scriptAssertf(0, "%s:LEADERBOARDS_GET_CACHE_TIME - Invalid Id=%d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), id);

		return MAX_INT32;
	}

	int  CommandLeaderboardsGetCacheNumberOfRows(const int id)
	{
		const CNetworkReadLeaderboards::LeaderboardScriptDisplayDataList& data = CNetwork::GetLeaderboardMgr().GetLeaderboardReadMgr().GetDisplayData();

		for (int i=0; i<data.GetCount(); i++)
		{
			if (id == data[i].m_Id)
			{
				return data[i].m_Rows.GetCount();
			}
		}

		scriptAssertf(0, "%s:LEADERBOARDS_GET_CACHE_NUMBER_OF_ROWS - Invalid Id=%d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), id);

		return 0;
	}

	bool CommandLeaderboardsGetCacheDataRow(const int id, const int rowIndex, LeaderboardCachedDisplayData* info)
	{
		const CNetworkReadLeaderboards::LeaderboardScriptDisplayDataList& data = CNetwork::GetLeaderboardMgr().GetLeaderboardReadMgr().GetDisplayData();

		bool result = false;

		int index = -1;
		for (int i=0; i<data.GetCount() && index<0; i++)
		{
			if (id == data[i].m_Id)
			{
				index = i;
				break;
			}
		}

		scriptAssertf(index > -1, "%s:LEADERBOARDS_GET_CACHE_DATA_ROW - Invalid Id=%d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), id);
		if (index < 0)
			return result;

		const CLeaderboardScriptDisplayData& lb = data[index];

		scriptAssertf(lb.m_Rows.GetCount() > rowIndex, "%s:LEADERBOARDS_GET_CACHE_DATA_ROW - Id=%d Invalid rowIndex=%d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), id, rowIndex);
		if (lb.m_Rows.GetCount() <= rowIndex)
			return result;

		result = true;

		const CLeaderboardScriptDisplayData::sRowData& row = lb.m_Rows[rowIndex];

		info->m_Id.Int = lb.m_Id;
		info->m_ColumnsBitSets.Int = lb.m_ColumnsBitSets;
		info->m_CustomVehicle.Int = row.m_CustomVehicle;
		info->m_Rank.Int = row.m_Rank;
		info->m_NumColumns.Int = row.m_Columns.GetCount();

		sysMemSet(info->m_GamerName, 0, sizeof(info->m_GamerName));
		safecpy(info->m_GamerName, row.m_GamerName);
		CTheScripts::ExportGamerHandle(&row.m_GamerHandle, info->m_GamerHandle.m_GamerHandle[0].Int, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "LEADERBOARDS_GET_CACHE_DATA_ROW"));

		sysMemSet(info->m_CoDriverName, 0, sizeof(info->m_CoDriverName));
		if (ustrlen(row.m_CoDriverName) > 0)
		{
			safecpy(info->m_CoDriverName, row.m_CoDriverName);

			if (row.m_CoDriverHandle.IsValid())
			{
				CTheScripts::ExportGamerHandle(&row.m_CoDriverHandle, info->m_CoDriverHandle.m_GamerHandle[0].Int, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "LEADERBOARDS_GET_CACHE_DATA_ROW"));
			}
		}

		for (int i=0; i<LeaderboardCachedDisplayData::MAX_COLUMNS; i++)
		{
			info->m_iColumnData[i].Int = -1;
			info->m_fColumnData[i].Float = -1.0f;
		}

		info->m_RowFlags.Int = row.m_RowFlags;

		scriptDebugf1("LEADERBOARDS_GET_CACHE_DATA_ROW - id=%d", info->m_Id.Int);
		scriptDebugf1(".......... GamerName=%s", info->m_GamerName);
		scriptDebugf1("....... CoDriverName=%s", info->m_CoDriverName);
		scriptDebugf1("............... Rank=%d", info->m_Rank.Int);
		scriptDebugf1("......... NumColumns=%d", info->m_NumColumns.Int);
		scriptDebugf1("..... ColumnsBitSets=%d", info->m_ColumnsBitSets.Int);
		scriptDebugf1("........... RowFlags=%d", info->m_RowFlags.Int);

		for (int i=0; i<row.m_Columns.GetCount(); i++)
		{
			if (info->m_ColumnsBitSets.Int & (1<<i))
			{
				info->m_iColumnData[i].Int = row.m_Columns[i].m_IntVal;
				scriptDebugf1(".......... Column %d=%d,%d", i, info->m_iColumnData[i].Int, row.m_Columns[i].m_IntVal);
			}
			else
			{
				info->m_fColumnData[i].Float = row.m_Columns[i].m_FloatVal;
				scriptDebugf1(".......... Column %d=%f,%f", i, info->m_fColumnData[i].Float, row.m_Columns[i].m_FloatVal);
			}
		}

		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	// COMMUNITY STATS

	bool CommandStatCommunityStartSynch()
	{
		CStatsMgr::GetStatsDataMgr().UpdateCommunityStatValues();
		return true;
	}

	bool CommandStatCommunitySynchPending()
	{
		return false;
	}

	bool CommandStatCommunitySynchFailed()
	{
		return false;
	}

	bool CommandStatCommunityGetHistory(const int keyHash, const int depth, float &returnValue)
	{
		return CStatsMgr::GetStatsDataMgr().GetCommunityStatsMgr().GetCommunityStatDepthValue(keyHash, &returnValue, depth);
	}

	void CommandStatClearAllOnlineCharacterStats(const int prefix)
	{
		scriptDebugf1("%s:STAT_RESET_ALL_ONLINE_CHARACTER_STATS - Command Called on prefix='%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), prefix);
		CStatsMgr::GetStatsDataMgr().ResetAllOnlineCharacterStats(prefix, false, false);

		CProfileSettings& settings = CProfileSettings::GetInstance();
		if (settings.AreSettingsValid())
		{
			switch (prefix)
			{
			//case PS_GRP_MP_5:
			//case PS_GRP_MP_6:
			case PS_GRP_MP_7:
				scriptDebugf1("%s:STAT_RESET_ALL_ONLINE_CHARACTER_STATS - CLEAR LOCAL DIRTY READ TIMESTAMPS, DEFAULT_, prefix='%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), prefix);
				settings.Set(CProfileSettings::MP_CLOUD_POSIXTIME_HIGH32, 0);
				settings.Set(CProfileSettings::MP_CLOUD_POSIXTIME_LOW32, 0);
				settings.Set(CProfileSettings::MP_FLUSH_POSIXTIME_LOW32,  0);
				settings.Set(CProfileSettings::MP_FLUSH_POSIXTIME_HIGH32, 0);

#if IS_GEN9_PLATFORM && __LANDING_PAGE_READ_PF
				NETWORK_LANDING_PAGE_STATS_MGR.ResetCachedStats();
#endif // IS_GEN9_PLATFORM && __LANDING_PAGE_READ_PF
				break;

			case PS_GRP_MP_0: //Stats with name perfixed MP0_
				scriptDebugf1("%s:STAT_RESET_ALL_ONLINE_CHARACTER_STATS - CLEAR LOCAL DIRTY READ TIMESTAMPS, MP0_, prefix='%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), prefix);
				settings.Set(CProfileSettings::MP_CLOUD0_POSIXTIME_HIGH32,  0);
				settings.Set(CProfileSettings::MP_CLOUD0_POSIXTIME_LOW32 ,  0);
				break;

			case PS_GRP_MP_1: //Stats with name perfixed MP1_
				scriptDebugf1("%s:STAT_RESET_ALL_ONLINE_CHARACTER_STATS - CLEAR LOCAL DIRTY READ TIMESTAMPS, MP1_, prefix='%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), prefix);
				settings.Set(CProfileSettings::MP_CLOUD1_POSIXTIME_HIGH32,  0);
				settings.Set(CProfileSettings::MP_CLOUD1_POSIXTIME_LOW32 ,  0);
				break;

			case PS_GRP_MP_2: //Stats with name perfixed MP2_
				scriptDebugf1("%s:STAT_RESET_ALL_ONLINE_CHARACTER_STATS - CLEAR LOCAL DIRTY READ TIMESTAMPS, MP2_, prefix='%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), prefix);
				settings.Set(CProfileSettings::MP_CLOUD2_POSIXTIME_HIGH32,  0);
				settings.Set(CProfileSettings::MP_CLOUD2_POSIXTIME_LOW32 ,  0);
				break;

			case PS_GRP_MP_3: //Stats with name perfixed MP3_
				scriptDebugf1("%s:STAT_RESET_ALL_ONLINE_CHARACTER_STATS - CLEAR LOCAL DIRTY READ TIMESTAMPS, MP3_, prefix='%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), prefix);
				settings.Set(CProfileSettings::MP_CLOUD3_POSIXTIME_HIGH32,  0);
				settings.Set(CProfileSettings::MP_CLOUD3_POSIXTIME_LOW32 ,  0);
				break;

			default:
				break;
			}

			settings.Write(true);
		}

		//PS_GRP_MP_7 is the magic number to know that we have cleared all. 
		//    - Lets try to call a Synchronize or SetSynchronized 
		if (prefix == PS_GRP_MP_7)
		{
			CProfileStats& profileStatsMgr = CLiveManager::GetProfileStatsMgr();
			if (!profileStatsMgr.Synchronized( CProfileStats::PS_SYNC_MP ))
			{
#if !__FINAL
				MoneyInterface::SetValidateMoneyCount();
				CStatsSavesMgr& saveMgr = CStatsMgr::GetStatsDataMgr().GetSavesMgr();
				scriptAssertf(!saveMgr.DirtyProfileStatsReadDetected( ), "%s:STAT_RESET_ALL_ONLINE_CHARACTER_STATS - Make sure u call STAT_CLEAR_DIRTY_READ_DETECTED on CTRL+H.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
				scriptAssertf(!saveMgr.DirtyCloudReadDetected( ), "%s:STAT_RESET_ALL_ONLINE_CHARACTER_STATS - Make sure u call STAT_CLEAR_DIRTY_READ_DETECTED on CTRL+H.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
				saveMgr.ClearDirtyRead();
#endif // !__FINAL

				profileStatsMgr.SetSynchronized( CProfileStats::PS_SYNC_MP NOTFINAL_ONLY(, true) );
			}
		}
	}

	void CommandStatLocalClearAllOnlineCharacterStats(const int prefix)
	{
		scriptDebugf1("%s:STAT_LOCAL_RESET_ALL_ONLINE_CHARACTER_STATS - Command Called on prefix='%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), prefix);
		CStatsMgr::GetStatsDataMgr().ResetAllOnlineCharacterStats(prefix, false, true);

		//PS_GRP_MP_7 is the magic number to know that we have cleared all. 
		//    - Lets try to call a Synchronize or SetSynchronized 
		if (prefix == PS_GRP_MP_7)
		{
			CProfileStats& profileStatsMgr = CLiveManager::GetProfileStatsMgr();
			if (!profileStatsMgr.Synchronized( CProfileStats::PS_SYNC_MP ))
			{
				profileStatsMgr.SetSynchronized( CProfileStats::PS_SYNC_MP );
			}
		}
	}

	void CommandStatClearAllSinglePlayerStats()
	{
#if !__FINAL
		scriptDebugf1("%s:STAT_RESET_ALL_SINGLE_PLAYER_STATS - Command Called.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		CStatsMgr::GetStatsDataMgr().ResetAllStats( true, true/*dirtyProfile*/ );
#endif
	}

	int CommandStatGetNumberOfDays(const int keyHash)
	{
		int hours = 0;

		const sStatData* stat = 0;

		CStatsDataMgr::StatsMap::Iterator statIter;
		if (CStatsMgr::GetStatsDataMgr().StatIterFind(keyHash, statIter))
		{
			if (statIter.GetKey().IsValid())
			{
				stat = *statIter;
			}
		}

		scriptAssertf(stat, "%s : STAT_GET_NUMBER_OF_DAYS - Stat with key=\"%d\" does not exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), keyHash);
		if (stat)
		{
			scriptAssertf(stat->GetIsBaseType(STAT_TYPE_UINT32) || stat->GetIsBaseType(STAT_TYPE_UINT64), "%s : STAT_GET_NUMBER_OF_DAYS - Stat %s type is not u32 nor u64", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());

			if (stat->GetIsBaseType(STAT_TYPE_UINT32))
			{
				hours = CStatsUtils::ConvertToDays(static_cast< u64 >(stat->GetUInt32()));
			}
			else if (stat->GetIsBaseType(STAT_TYPE_UINT64))
			{
				hours = CStatsUtils::ConvertToDays(static_cast< u64 >(stat->GetUInt64()));
			}
		}

		return hours;
	}

	int CommandStatGetNumberOfHours(const int keyHash)
	{
		int hours = 0;

		const sStatData* stat = 0;

		CStatsDataMgr::StatsMap::Iterator statIter;
		if (CStatsMgr::GetStatsDataMgr().StatIterFind(keyHash, statIter))
		{
			if (statIter.GetKey().IsValid())
			{
				stat = *statIter;
			}
		}

		scriptAssertf(stat, "%s : STAT_GET_NUMBER_OF_HOURS - Stat with key=\"%d\" does not exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), keyHash);
		if (stat)
		{
			scriptAssertf(stat->GetIsBaseType(STAT_TYPE_UINT32) || stat->GetIsBaseType(STAT_TYPE_UINT64), "%s : STAT_GET_NUMBER_OF_HOURS - Stat %s type is not u32 nor u64", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());

			if (stat->GetIsBaseType(STAT_TYPE_UINT32))
			{
				hours = CStatsUtils::ConvertToHours(static_cast< u64 >(stat->GetUInt32()));
			}
			else if (stat->GetIsBaseType(STAT_TYPE_UINT64))
			{
				hours = CStatsUtils::ConvertToHours(static_cast< u64 >(stat->GetUInt64()));
			}
		}

		return hours;
	}

	int CommandStatGetNumberOfMinutes(const int keyHash)
	{
		int hours = 0;

		const sStatData* stat = 0;

		CStatsDataMgr::StatsMap::Iterator statIter;
		if (CStatsMgr::GetStatsDataMgr().StatIterFind(keyHash, statIter))
		{
			if (statIter.GetKey().IsValid())
			{
				stat = *statIter;
			}
		}

		scriptAssertf(stat, "%s : STAT_GET_NUMBER_OF_MINUTES - Stat with key=\"%d\" does not exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), keyHash);
		if (stat)
		{
			scriptAssertf(stat->GetIsBaseType(STAT_TYPE_UINT32) || stat->GetIsBaseType(STAT_TYPE_UINT64), "%s : STAT_GET_NUMBER_OF_MINUTES - Stat %s type is not u32 nor u64", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());

			if (stat->GetIsBaseType(STAT_TYPE_UINT32))
			{
				hours = CStatsUtils::ConvertToMins(static_cast< u64 >(stat->GetUInt32()));
			}
			else if (stat->GetIsBaseType(STAT_TYPE_UINT64))
			{
				hours = CStatsUtils::ConvertToMins(static_cast< u64 >(stat->GetUInt64()));
			}
		}

		return hours;
	}

	void CommandsStatTrackAverageTimePerSession()
	{
		u64 average = (u64)(StatsInterface::GetUInt64Stat(STAT_TOTAL_PLAYING_TIME) / StatsInterface::GetIntStat(STAT_NUMBER_OF_SESSIONS_FM));
		StatsInterface::SetStatData(STAT_AVERAGE_TIME_PER_SESSON, average);
	}

	int CommandStatGetNumberOfSeconds(const int keyHash)
	{
		int hours = 0;

		const sStatData* stat = 0;

		CStatsDataMgr::StatsMap::Iterator statIter;
		if (CStatsMgr::GetStatsDataMgr().StatIterFind(keyHash, statIter))
		{
			if (statIter.GetKey().IsValid())
			{
				stat = *statIter;
			}
		}

		scriptAssertf(stat, "%s : STAT_GET_NUMBER_OF_SECONDS - Stat with key=\"%d\" does not exist", CTheScripts::GetCurrentScriptNameAndProgramCounter(), keyHash);
		if (stat)
		{
			scriptAssertf(stat->GetIsBaseType(STAT_TYPE_UINT32) || stat->GetIsBaseType(STAT_TYPE_UINT64), "%s : STAT_GET_NUMBER_OF_SECONDS - Stat %s type is not u32 nor u64", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statIter.GetKeyName());

			if (stat->GetIsBaseType(STAT_TYPE_UINT32))
			{
				hours = CStatsUtils::ConvertToSeconds(static_cast< u64 >(stat->GetUInt32()));
			}
			else if (stat->GetIsBaseType(STAT_TYPE_UINT64))
			{
				hours = CStatsUtils::ConvertToSeconds(static_cast< u64 >(stat->GetUInt64()));
			}
		}

		return hours;
	}

	int CommandStatMillisecondsConvertToDays(const int milliseconds)
	{
		return CStatsUtils::ConvertToDays(milliseconds);
	}
	int CommandStatMillisecondsConvertToHours(const int milliseconds)
	{
		return CStatsUtils::ConvertToHours(milliseconds);
	}
	int CommandStatMillisecondsConvertToMinutes(const int milliseconds)
	{
		return CStatsUtils::ConvertToMins(milliseconds);
	}
	int CommandStatMillisecondsConvertToSeconds(const int milliseconds)
	{
		return CStatsUtils::ConvertToSeconds(milliseconds);
	}
	int CommandStatGetMillisecondsToNearestSecond(const int milliseconds)
	{
		return CStatsUtils::ConvertToMilliseconds(milliseconds);
	}

	void CommandStatGetTimeAsDate(int milliseconds, int& data)
	{
		scrValue* dataNew = (scrValue*)&data;
		int size = 0;
		dataNew[size].Int = 0; ++size;
		dataNew[size].Int = 0; ++size;
		dataNew[size].Int = CStatsUtils::ConvertToDays(milliseconds); ++size;
		dataNew[size].Int = CStatsUtils::ConvertToHours(milliseconds); ++size;
		dataNew[size].Int = CStatsUtils::ConvertToMins(milliseconds); ++size;
		dataNew[size].Int = CStatsUtils::ConvertToSeconds(milliseconds); ++size;
		dataNew[size].Int = CStatsUtils::ConvertToMilliseconds(milliseconds); ++size;
	}

	bool CommandStatLoadPending(const int slot)
	{
		const bool pending = StatsInterface::LoadPending(slot);

#if !__FINAL
		static bool s_isPending = false;

		if (pending && !s_isPending)
			statDebugf1("%s : STAT_LOAD_PENDING - command called. slot='%d', pending='%s'", CTheScripts::GetCurrentScriptNameAndProgramCounter(), slot, pending?"true":"false");
		else if (!pending && s_isPending)
			statDebugf1("%s : STAT_LOAD_PENDING - command called. slot='%d', pending='%s'", CTheScripts::GetCurrentScriptNameAndProgramCounter(), slot, pending?"true":"false");

		s_isPending = pending;
#endif // !__FINAL

		return pending;
	}

	bool CommandStatSavePending()
	{
		const bool pending = StatsInterface::SavePending();

#if !__FINAL
		static bool s_isPending = false;

		if (pending && !s_isPending)
			statDebugf1("%s : STAT_SAVE_PENDING - command called. pending='%s'", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pending?"true":"false");
		else if (!pending && s_isPending)
			statDebugf1("%s : STAT_SAVE_PENDING - command called. pending='%s'", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pending?"true":"false");

		s_isPending = pending;
#endif // !__FINAL

		return pending;
	}

	bool CommandStatSavePendingOrRequested()
	{
		CStatsSavesMgr& savemgr = CStatsMgr::GetStatsDataMgr().GetSavesMgr();
		const bool pending = (savemgr.IsSaveInProgress() || savemgr.PendingSaveRequests());

#if !__FINAL
		static bool s_isPending = false;

		if (pending && !s_isPending)
			statDebugf1("%s : STAT_SAVE_PENDING_OR_REQUESTED - command called. pending='%s'", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pending?"true":"false");
		else if (!pending && s_isPending)
			statDebugf1("%s : STAT_SAVE_PENDING_OR_REQUESTED - command called. pending='%s'", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pending?"true":"false");

		s_isPending = pending;
#endif // !__FINAL

		return pending;
	}

	void CommandStatDeleteSlot(const int slot)
	{
		scriptAssertf(slot >= STAT_MP_CATEGORY_DEFAULT && slot <= STAT_MP_CATEGORY_MAX, "%s:STAT_DELETE_SLOT - Invalid slot=%d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), slot);

		statDebugf1("%s : STAT_DELETE_SLOT - command called on slot=%d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), slot);

		CStatsSavesMgr& savemgr = CStatsMgr::GetStatsDataMgr().GetSavesMgr();
		savemgr.ClearLoadPending(slot);
		savemgr.ClearLoadFailed(slot);
	}

	void CommandStatClearSlotForReload(const int slot)
	{
		scriptAssertf(slot >= 0 && slot <= STAT_MP_CATEGORY_MAX, "%s:STAT_CLEAR_SLOT_FOR_RELOAD - Invalid slot=%d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), slot);
		if (slot >= 0 && slot <= STAT_MP_CATEGORY_MAX)
		{
			if (PARAM_nonetwork.Get() || PARAM_lan.Get() || PARAM_nompsavegame.Get() || PARAM_cloudForceAvailable.Get())
				return;

			CStatsSavesMgr& saveMgr = CStatsMgr::GetStatsDataMgr().GetSavesMgr();

			statDebugf1("%s : STAT_CLEAR_SLOT_FOR_RELOAD - command called on slot=%d, IsLoadPending='%s', CloudLoadFailed='%s'", CTheScripts::GetCurrentScriptNameAndProgramCounter(), slot, saveMgr.IsLoadPending(slot)?"True":"False", saveMgr.CloudLoadFailed(slot)?"True":"False");

			saveMgr.SetLoadPending(slot);
			saveMgr.ClearLoadFailed(slot);

			{
				const char* statName = "_SaveMpTimestamp_0";
				switch (slot)
				{
				case STAT_MP_CATEGORY_DEFAULT: statName = "_SaveMpTimestamp_0"; break;
				case STAT_MP_CATEGORY_CHAR0:   statName = "_SaveMpTimestamp_1"; break;
				case STAT_MP_CATEGORY_CHAR1:   statName = "_SaveMpTimestamp_2"; break;
				}
				StatId statTimestamp(statName);

				//Setup save timestamp
				StatsInterface::SetStatData(statTimestamp, 0, STATUPDATEFLAG_DIRTY_PROFILE);
			}
		}
	}

	bool CommandStatLoad(const int slot)
	{
		bool result = false;

		scriptAssertf( CProfileSettings::GetInstance().AreSettingsValid(),  "%s : STAT_LOAD - CProfileSettings invalid, signin just happened ?", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		scriptAssertf(CLiveManager::CheckOnlinePrivileges(), "%s:STAT_LOAD - Player doesnt have online previleges", CTheScripts::GetCurrentScriptNameAndProgramCounter());
        if (!CLiveManager::CheckOnlinePrivileges())
            return result;

		scriptAssertf(!StatsInterface::LoadPending(-1), "%s : STAT_LOAD - Load in progress. slot=%d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), slot);
		scriptAssertf(!StatsInterface::SavePending(), "%s : STAT_LOAD - Save in progress. slot=%d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), slot);

		if (!StatsInterface::LoadPending(-1) && !StatsInterface::SavePending())
		{
			result = true;

			if (STAT_INVALID_SLOT == slot || StatsInterface::CloudFileLoadPending(slot))
			{
				scriptDebugf1("%s : STAT_LOAD - command called on slot=%d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), slot);
				if (StatsInterface::CloudFileLoadPending(slot) && gnetVerify(NetworkInterface::IsCloudAvailable()))
				{
					statDebugf1("%s : STAT_LOAD - command called on slot=%d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), slot);
					result = StatsInterface::Load(STATS_LOAD_CLOUD, slot);

					if (!result)
					{
						statErrorf("%s : STAT_LOAD - command called on slot=%d - FAILED.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), slot);
					}
				}
			}
		}

		return result;
	}

	void  CommandStatSetBlockSaveRequests(const bool blocksave)
	{
		statDebugf1("%s : STAT_SET_BLOCK_SAVES - command called. Value=%s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), blocksave ? "TRUE" : "FALSE");
		CStatsSavesMgr& saveMgr = CStatsMgr::GetStatsDataMgr().GetSavesMgr();
		saveMgr.SetBlockSaveRequests(blocksave);

		if (blocksave)
		{
			saveMgr.CancelAndClearSaveRequests( );
		}
	}

	bool  CommandStatGetBlockSaveRequests()
	{
		return StatsInterface::GetBlockSaveRequests();
	}

	void  CommandStatForceCloudProfileDownloadAndOverwriteLocalStats()
	{
		statDebugf1("%s : FORCE_CLOUD_MP_STATS_DOWNLOAD_AND_OVERWRITE_LOCAL_SAVE - command called.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		CStatsMgr::GetStatsDataMgr().ResynchServerAuthoritative(true);
		CLiveManager::GetProfileStatsMgr().ForceMPProfileStatsGetFromCloud();
		CStatsMgr::GetStatsDataMgr().GetSavesMgr().ClearDirtyRead();
		CStatsMgr::GetStatsDataMgr().GetSavesMgr().ClearPendingFlushRequests( true );
	}

	bool CommandStatCloudFileSaveFailed(const int slot)
	{
		scriptAssertf(slot>=-1 && slot <= STAT_MP_CATEGORY_MAX, "%s:STAT_CLOUD_SLOT_SAVE_FAILED - Invalid slot=%d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), slot);

		bool failed = false;

		//Failed for Cloud Saves?
		if (slot>=0 && slot <= STAT_MP_CATEGORY_MAX)
		{
			CStatsSavesMgr& saveMgr = CStatsMgr::GetStatsDataMgr().GetSavesMgr();
			failed = saveMgr.CloudSaveFailed(slot);
		}
		else if (-1 == slot)
		{
			CStatsSavesMgr& saveMgr = CStatsMgr::GetStatsDataMgr().GetSavesMgr();
			for(int i=STAT_MP_CATEGORY_DEFAULT; i<=STAT_MP_CATEGORY_MAX && !failed; i++)
			{
				failed = saveMgr.CloudSaveFailed( i );
			}
		}

		//Failed for Profile Stats?
		if (!failed)
			failed = CLiveManager::GetProfileStatsMgr().FlushFailed();

#if !__NO_OUTPUT
		static bool s_failed = false;
		if(failed)
		{
			s_failed = true;
			statDebugf1("%s : STAT_CLOUD_SLOT_SAVE_FAILED - command called, failed='%s'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), failed?"True":"False");
		}
		else if (s_failed)
		{
			s_failed = false;
			statDebugf1("%s : STAT_CLOUD_SLOT_SAVE_FAILED - command called, failed='%s'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), failed?"True":"False");
		}
#endif

		return failed;
	}

	int CommandStatCloudFileSaveFailedCode(const int slot)
	{
		scriptAssertf(slot>=STAT_MP_CATEGORY_DEFAULT && slot<=STAT_MP_CATEGORY_MAX, "%s:STAT_CLOUD_SLOT_SAVE_FAILED_CODE - Invalid slot=%d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), slot);

		int failurecode = 0;

		if (slot >= 0 && slot <= STAT_MP_CATEGORY_MAX)
		{
			CStatsSavesMgr& saveMgr = CStatsMgr::GetStatsDataMgr().GetSavesMgr();

			if ( saveMgr.CloudSaveFailed(slot) )
				failurecode = saveMgr.GetSaveFailedCode(slot);
		}

		if (failurecode == 0)
		{
			if(CLiveManager::GetProfileStatsMgr().FlushFailed())
				failurecode = HTTP_CODE_ANYSERVERERROR;
		}

		return failurecode;
	}

	void CommandStatClearPendingSaves(const int slot)
	{
		scriptAssertf(slot>=-1 && slot<=STAT_MP_CATEGORY_MAX, "%s:STAT_CLEAR_PENDING_SAVES - Invalid slot=%d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), slot);

		statWarningf("%s : STAT_CLEAR_PENDING_SAVES - command called, slot='%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), slot);

		if (slot >= 0 && slot <= STAT_MP_CATEGORY_MAX)
		{
			CStatsSavesMgr& saveMgr = CStatsMgr::GetStatsDataMgr().GetSavesMgr();
			saveMgr.ClearPendingSaveRequests(slot);
		}
		else if (-1 == slot)
		{
			CStatsSavesMgr& saveMgr = CStatsMgr::GetStatsDataMgr().GetSavesMgr();

			for(int i=STAT_MP_CATEGORY_DEFAULT; i<=STAT_MP_CATEGORY_MAX; i++)
				saveMgr.ClearPendingSaveRequests( i );
		}
	}

	bool CommandStatDirtyReadDetected()
	{
		CStatsSavesMgr& saveMgr = CStatsMgr::GetStatsDataMgr().GetSavesMgr();

		if (saveMgr.DirtyProfileStatsReadDetected( ))
		{
			statDebugf1("%s : STAT_LOAD_DIRTY_READ_DETECTED - command called Dirty Reads have been detected - ON PROFILE STATS.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			return true;
		}

		if (saveMgr.DirtyCloudReadDetected( ))
		{
			statDebugf1("%s : STAT_LOAD_DIRTY_READ_DETECTED - command called Dirty Reads have been detected - ON CLOUD SAVES.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			return true;
		}

		return false;
	}
	
	void CommandStatClearDirtyReadDetected()
	{
		statDebugf1("%s : STAT_CLEAR_DIRTY_READ_DETECTED - command called CLEAR Dirty Reads have been detected.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		CStatsMgr::GetStatsDataMgr().GetSavesMgr().ClearDirtyRead();
	}

	void CommandStatGetLocalSaveTimestamp(int& year, int& month, int& day, int& hour, int& min, int& sec)
	{
		//Set Local value for last MP flush.
		CProfileSettings& settings = CProfileSettings::GetInstance();
		scriptAssertf(settings.AreSettingsValid(), "%s : STAT_GET_LOCAL_SAVE_TIME - command called Profile Settings are not valid.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		u64 posixTime = 0;

		CStatsSavesMgr& saveMgr = CStatsMgr::GetStatsDataMgr().GetSavesMgr();

		//Profile Stats Dirty Read
		if (saveMgr.DirtyProfileStatsReadDetected( ))
		{
			u64 low32 = 0;
			u64 high32 = 0;

			if(settings.Exists(CProfileSettings::MP_FLUSH_POSIXTIME_LOW32))
				low32 = static_cast< u64 >( settings.GetInt(CProfileSettings::MP_FLUSH_POSIXTIME_LOW32) );
			if(settings.Exists(CProfileSettings::MP_FLUSH_POSIXTIME_HIGH32))
				high32 = static_cast< u64 >( settings.GetInt(CProfileSettings::MP_FLUSH_POSIXTIME_HIGH32) );

			posixTime = high32 << 32;
			posixTime = posixTime | low32;
		}

		//Cloud Dirty Read
		else if (saveMgr.DirtyCloudReadDetected( ) && scriptVerify(-1 < saveMgr.GetDirtyCloudReadFile( )))
		{
			posixTime = saveMgr.GetLocalCloudDirtyReadValue((u32)saveMgr.GetDirtyCloudReadFile( ));
		}

		year = 0;month = 0;day = 0;hour = 0;min = 0;sec = 0;

		time_t date = (time_t)posixTime;
		struct tm* ptm;
		ptm = gmtime(&date);

		if (ptm)
		{
			year  = ptm->tm_year+1900;
			month = ptm->tm_mon+1;
			day   = ptm->tm_mday;
			hour  = ptm->tm_hour+1;
			min   = ptm->tm_min;
			sec   = ptm->tm_sec;
		}

		statDebugf1("%s : STAT_GET_LOCAL_SAVE_TIME - Local Time='%" I64FMT "d', date='%d:%d:%d, %d-%d-%d'",CTheScripts::GetCurrentScriptNameAndProgramCounter(), posixTime,hour,min,sec,day,month,year);
	}

	void CommandStatProfileStatsLastServerTimestamp(int& year, int& month, int& day, int& hour, int& min, int& sec)
	{
		u64 posixTime = CStatsMgr::GetStatsDataMgr().GetSavesMgr().GetDirtyReadServerTime( );

		year = 0;month = 0;day = 0;hour = 0;min = 0;sec = 0;

		time_t date = (time_t)posixTime;
		struct tm* ptm;
		ptm = gmtime(&date);

		if (ptm)
		{
			year  = ptm->tm_year+1900;
			month = ptm->tm_mon+1;
			day   = ptm->tm_mday;
			hour  = ptm->tm_hour+1;
			min   = ptm->tm_min;
			sec   = ptm->tm_sec;
		}

		statDebugf1("%s : STAT_GET_PROFILE_STATS_SERVER_SAVE_TIME - Server Time='%" I64FMT "d', date='%d:%d:%d, %d-%d-%d'",CTheScripts::GetCurrentScriptNameAndProgramCounter(), posixTime,hour,min,sec,day,month,year);
	}

	class MetricFailureToSynchProfileStats : public MetricPlayStat
	{
		RL_DECLARE_METRIC(PS_SYNCH_FAIL, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);
	};

	bool CommandStatGetIsSafeToProgressToMpFromSp()
	{
		statDebugf1("%s : STAT_GET_LOAD_SAFE_TO_PROGRESS_TO_MP_FROM_SP.",CTheScripts::GetCurrentScriptNameAndProgramCounter());

		CStatsSavesMgr& saveMgr = CStatsMgr::GetStatsDataMgr().GetSavesMgr();

		if (saveMgr.IsLoadInProgress( -1 ))
		{
			scriptErrorf("STAT_GET_LOAD_SAFE_TO_PROGRESS_TO_MP_FROM_SP - Load is still in progress.");
			return false;
		}

		if (saveMgr.IsLoadPending( STAT_MP_CATEGORY_DEFAULT ))
		{
			scriptErrorf("STAT_GET_LOAD_SAFE_TO_PROGRESS_TO_MP_FROM_SP - Cloud Load Pending for default slot.");
			return false;
		}
		if (saveMgr.CloudLoadFailed( STAT_MP_CATEGORY_DEFAULT ))
		{
			scriptErrorf("STAT_GET_LOAD_SAFE_TO_PROGRESS_TO_MP_FROM_SP - Cloud Load Failed for default slot.");
			return false;
		}

		const int selectedSlot = StatsInterface::GetCurrentMultiplayerCharaterSlot() + 1;
		if (saveMgr.IsLoadPending( selectedSlot ))
		{
			scriptErrorf("STAT_GET_LOAD_SAFE_TO_PROGRESS_TO_MP_FROM_SP - Cloud Load Pending for slot '%d'.", selectedSlot);
			return false;
		}

		if (saveMgr.CloudLoadFailed( selectedSlot ))
		{
			scriptErrorf("STAT_GET_LOAD_SAFE_TO_PROGRESS_TO_MP_FROM_SP - Cloud Load Failed for slot '%d'.", selectedSlot);
			return false;
		}

		CProfileStats& profileStatsMgr = CLiveManager::GetProfileStatsMgr();
		if (!profileStatsMgr.Synchronized( CProfileStats::PS_SYNC_MP ))
		{
			scriptErrorf("STAT_GET_LOAD_SAFE_TO_PROGRESS_TO_MP_FROM_SP - Profile Stats not Synchronized.");
			return false;
		}

		int errorCode = 0;
		if (profileStatsMgr.SynchronizedFailed( CProfileStats::PS_SYNC_MP, errorCode ))
		{
			scriptErrorf("STAT_GET_LOAD_SAFE_TO_PROGRESS_TO_MP_FROM_SP - Synchronized Failed.");
			return false;
		}

		bool checkOnlineStatsAreSynched = true;
		Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("DISABLE_ONLINE_CHECK_FOR_SAFE_TO_GO_TO_MP", 0xa22197e7), checkOnlineStatsAreSynched);

#if !__FINAL
		if (PARAM_disableOnlineStatsAreSynched.Get())
		{
			// get environment - pushed this into non-final only. The environment is not actually set when we first
			// request a bunch of cloud files so we use RL_ENV_UNKNOWN but this will change when we receive our ticket
			// so the actual cached file will be different.
#if !RSG_PC
#if RSG_GEN9
			rlEnvironment nativeEnvironment = rlGetEnvironment();
#else
			rlEnvironment nativeEnvironment = rlGetNativeEnvironment();
#endif // RSG_GEN(
#endif // !RSG_PC

#if RSG_NP
			if (nativeEnvironment == rlEnvironment::RL_ENV_UNKNOWN)
			{
				nativeEnvironment = (rlNpEnvironment::RLNP_ENV_SP_INT == g_rlNp.GetEnvironment()) ? rlEnvironment::RL_ENV_DEV : rlEnvironment::RL_ENV_UNKNOWN;
			}
#endif //RSG_NP

#if !RSG_PC
			if (nativeEnvironment == rlEnvironment::RL_ENV_DEV && g_rlTitleId && g_rlTitleId->m_RosTitleId.GetEnvironment() == rlRosEnvironment::RLROS_ENV_DEV)
#endif // !RSG_PC
			{
				scriptWarningf("STAT_GET_LOAD_SAFE_TO_PROGRESS_TO_MP_FROM_SP - PARAM_disableOnlineStatsAreSynched.");
				checkOnlineStatsAreSynched = false;
			}
		}
		else if (PARAM_nompsavegame.Get() || PARAM_lan.Get() || PARAM_cloudForceAvailable.Get())
		{
			scriptWarningf("STAT_GET_LOAD_SAFE_TO_PROGRESS_TO_MP_FROM_SP - PARAM_nompsavegame.Get() || PARAM_lan.Get() || PARAM_cloudForceAvailable.Get().");
			checkOnlineStatsAreSynched = false;
		}
#endif // !__FINAL

		if (checkOnlineStatsAreSynched && !CStatsMgr::GetStatsDataMgr().GetAllOnlineStatsAreSynched())
		{
			scriptErrorf("STAT_GET_LOAD_SAFE_TO_PROGRESS_TO_MP_FROM_SP - Not all stats are synched.");

			MetricFailureToSynchProfileStats m;
			CNetworkTelemetry::AppendMetric(m);

			return false;
		}

		return true;
	}

	void CommandStatSetCanSaveDuringJob(const int savetype)
	{
		CStatsSavesMgr& saveMgr = CStatsMgr::GetStatsDataMgr().GetSavesMgr();
		saveMgr.SetCanSaveDuringJob((eSaveTypes)savetype);
	}

	bool CommandStatGetCanSaveDuringJob(const int savetype)
	{
		CStatsSavesMgr& saveMgr = CStatsMgr::GetStatsDataMgr().GetSavesMgr();
		return saveMgr.GetCanSaveDuringJob((eSaveTypes)savetype);
	}

    bool CheckStatSaveOnlinePermissions()
    {
        static unsigned PROMOTION_GRACE_TIME_MS = 30000;
        return CLiveManager::CheckOnlinePrivileges() || NetworkInterface::HadOnlinePermissionsPromotionWithinMs(PROMOTION_GRACE_TIME_MS);
    }

	bool CommandStatSave(const int slot, const bool ASSERT_ONLY(doAssert), const int savetype, const int saveReason)
	{
		scriptAssertf(!doAssert, "%s:STAT_SAVE - Assert requested on Save", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		scriptAssertf(-1 < slot && slot < MAX_NUM_MP_CLOUD_FILES, "%s:STAT_SAVE - Invalid slot=%d, type=%d, reason='%d'", CTheScripts::GetCurrentScriptNameAndProgramCounter(), slot, savetype, saveReason);

		bool result = false;

        // allow a save when we *had* a promotion
		scriptAssertf(CheckStatSaveOnlinePermissions(), "%s:STAT_SAVE slot=%d, type=%d, reason='%d' - Player doesnt have online privileges", CTheScripts::GetCurrentScriptNameAndProgramCounter(), slot, savetype, saveReason);
        if (!CheckStatSaveOnlinePermissions())
            return result;

		const int selectedSlot = StatsInterface::GetCurrentMultiplayerCharaterSlot( ) + 1;

		bool statsLoaded = !StatsInterface::CloudFileLoadPending(0) && !StatsInterface::CloudFileLoadPending(selectedSlot);

		if (statsLoaded)
		{
			if (slot > 0 && slot != selectedSlot)
			{
				statsLoaded = !StatsInterface::CloudFileLoadPending(slot);

				if (!statsLoaded)
				{
#if !__FINAL
					scriptAssertf(0, "%s:STAT_SAVE slot=%d, type=%s, reason='%d' - Missing load:", CTheScripts::GetCurrentScriptNameAndProgramCounter(), slot, GET_STAT_SAVETYPE_NAME((eSaveTypes)savetype), saveReason);
					scriptErrorf(" Load status for slot %d, type=%s: Cloud=%s", slot, GET_STAT_SAVETYPE_NAME((eSaveTypes)savetype), StatsInterface::CloudFileLoadPending(slot) ? "Pending" : "Done");
#endif
				}
			}
		}
		else
		{
			scriptAssertf(0, "%s:STAT_SAVE slot=%d, type=%s, reason='%d' - Missing load:", CTheScripts::GetCurrentScriptNameAndProgramCounter(), slot, GET_STAT_SAVETYPE_NAME((eSaveTypes)savetype), saveReason);
			scriptErrorf("Load status for slot %d: Cloud=%s", 0, StatsInterface::CloudFileLoadPending(0) ? "Pending" : "Done");
			scriptErrorf("Load status for slot %d: Cloud=%s", selectedSlot, StatsInterface::CloudFileLoadPending(selectedSlot) ? "Pending" : "Done");
		}

		if (NetworkInterface::IsLocalPlayerOnline() && statsLoaded && -1 < slot)
		{
			NOTFINAL_ONLY(statDebugf1("%s : STAT_SAVE - command called on slot=%d, type=%s, reason='%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), slot, GET_STAT_SAVETYPE_NAME((eSaveTypes)savetype), saveReason));

#if __BANK
			if (PARAM_debugSaveRequests.Get()) 
			{
				scrThread::PrePrintStackTrace();
			}
#endif

			//If we are going back to single player we have to clear all scars stats data.
			if (savetype == STAT_SAVETYPE_END_SESSION)
			{
				StatsInterface::ClearPedScarData( );
			}

			result = StatsInterface::Save(STATS_SAVE_CLOUD, slot, (eSaveTypes)savetype, 0, saveReason);
		}
#if !__FINAL
		else if(!NetworkInterface::IsLocalPlayerOnline()) scriptErrorf("%s : STAT_SAVE - Local Player not online. slot=%d, type=%s, reason='%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), slot, GET_STAT_SAVETYPE_NAME((eSaveTypes)savetype), saveReason);
		else if(-1 >= slot)                               scriptErrorf("%s : STAT_SAVE - Invalid slot=%d, type=%s, reason='%d'", CTheScripts::GetCurrentScriptNameAndProgramCounter(), slot, GET_STAT_SAVETYPE_NAME((eSaveTypes)savetype), saveReason);
#endif

		return result;
	}

	bool CommandStatSlotIsLoaded(const int slot)
	{
		return (!StatsInterface::CloudFileLoadPending(slot));
	}

	bool CommandStatCloudFileLoadFailed(const int slot)
	{
		scriptAssertf(slot>-1 && slot <= STAT_MP_CATEGORY_MAX, "%s:STAT_CLOUD_SLOT_LOAD_FAILED - Invalid slot=%d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), slot);

		const bool failed = StatsInterface::CloudFileLoadFailed(slot);

		if (failed)
			scriptDebugf1("%s : STAT_CLOUD_SLOT_LOAD_FAILED - Cloud Load of slot=%d, FAILED.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), slot);

		return failed;
	}

	int CommandStatCloudFileLoadFailedCode(const int slot)
	{
		scriptAssertf(slot>-1 && slot <= STAT_MP_CATEGORY_MAX, "%s:STAT_CLOUD_SLOT_LOAD_FAILED_CODE - Invalid slot=%d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), slot);

		int failurecode = LOAD_FAILED_REASON_NONE;

		const bool failed = StatsInterface::CloudFileLoadFailed(slot);
		if (failed)
		{
			failurecode = CStatsMgr::GetStatsDataMgr().GetSavesMgr().GetLoadFailedCode(slot);
			scriptDebugf1("%s : STAT_CLOUD_SLOT_LOAD_FAILED_CODE - Cloud Load of slot=%d, failurecode=%d, FAILED.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), slot, failurecode);
		}

		return failurecode;
	}

	bool CommandStatSetCloudSaveService(const int service)
	{
		statDebugf1("%s : STAT_SET_CLOUD_SAVE_SERVICE - command called on slot=%d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), service);

		scriptAssertf(NetworkInterface::HasValidRockstarId() || service != RL_CLOUD_ONLINE_SERVICE_SC, "%s:STAT_SET_CLOUD_SAVE_SERVICE - Local Gamer doesnt have a valid RockStar Id, ie, this account is not linked to a social club account", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if(NetworkInterface::HasValidRockstarId() || service != RL_CLOUD_ONLINE_SERVICE_SC)
		{
			StatsInterface::SetCloudSavegameService((rlCloudOnlineService)service);
			return true;
		}

		return false;
	}

	void  CommandStatSetCloudLoadFailed(const int NOTFINAL_ONLY(slot), const bool NOTFINAL_ONLY(failed))
	{
#if !__FINAL
		statDebugf1("%s : STAT_SET_CLOUD_SLOT_LOAD_FAILED - command called on slot=%d, failed=%s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), slot, failed?"True":"False");

		scriptAssertf(slot >= 0 && slot <= STAT_MP_CATEGORY_MAX, "%s:STAT_SET_CLOUD_SLOT_LOAD_FAILED - Invalid slot=%d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), slot);

		CStatsSavesMgr& saveMgr = CStatsMgr::GetStatsDataMgr().GetSavesMgr();

		if (failed && !saveMgr.CloudLoadFailed(slot))
		{
			scriptDebugf1("%s - Command STAT_SET_CLOUD_SLOT_LOAD_FAILED - called - SET FAILED TRUE for slot=%d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), slot);
			saveMgr.SetLoadFailed(slot, LOAD_FAILED_REASON_FAILED_TO_LOAD);
		}
		else if (!failed && saveMgr.CloudLoadFailed(slot))
		{
			scriptDebugf1("%s - Command STAT_SET_CLOUD_SLOT_LOAD_FAILED - called - SET FAILED FALSE for slot=%d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), slot);
			saveMgr.ClearLoadFailed(slot);
		}
#endif // !__FINAL
	}

	void CommandPresenceEventUpdateStat_Int(int statId, int value, int value2)
	{
		CStatUpdatePresenceEvent::Trigger((u32)statId, value, value2, "", SendFlag_AllFriends);
	}

	void CommandPresenceEventUpdateStat_Float(int statId, float value, int value2)
	{
		CStatUpdatePresenceEvent::Trigger((u32)statId, value, value2, "", SendFlag_AllFriends);
	}

	void CommandPresenceEventUpdateStat_IntString(int statId, int value, int value2, const char* str)
	{
		if(!scriptVerifyf(strlen(str) < 64, "PRESENCE_EVENT_UPDATESTAT_INT_WITH_STRING received string data with too many chars[%d] '%s'", istrlen(str), str))
		{
			return;
		}
		CStatUpdatePresenceEvent::Trigger((u32)statId, value, value2, str, SendFlag_AllFriends);
	}

	void CommandPresenceEventUpdateStat_FloatString(int statId, float value, int value2, const char* str)
	{
		if(!scriptVerifyf(strlen(str) < 64, "PRESENCE_EVENT_UPDATESTAT_FLOAT_WITH_STRING received string data with too many chars[%d] '%s'", istrlen(str), str))
		{
			return;
		}

		CStatUpdatePresenceEvent::Trigger((u32)statId, value, value2, str, SendFlag_AllFriends);
	}

	bool CommandIsGameRunningWithNoMpSaveGameParam()
	{
		if (PARAM_nompsavegame.Get())
		{
			return true;
		}

		return false;
	}

	int CommandPackedStatGetBoolStatIndex(const int statEnum)
	{
		return (int)floor((float)(statEnum/NUM_PACKED_BOOL_PER_STAT));
	}

	int CommandPackedStatGetIntStatIndex(const int statEnum)
	{
		return (int)floor((float)(statEnum/NUM_PACKED_INT_PER_STAT));
	}

	bool CommandPackedStatGetPackedBool(const int statEnum, const int overrideCharSlot)
	{
		if (NetworkInterface::IsNetworkOpen())
		{
			scriptAssertf(!StatsInterface::CloudFileLoadPending(0), "%s : GET_PACKED_STAT_BOOL_CODE - Trying to Access Stat with statEnum=\"%d\" but stats are not loaded.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statEnum);
			scriptAssertf(!StatsInterface::CloudFileLoadFailed(0),  "%s : GET_PACKED_STAT_BOOL_CODE - Trying to Access Stat with statEnum=\"%d\" but stats loaded have failed.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statEnum);
			if(StatsInterface::CloudFileLoadPending(0) || StatsInterface::CloudFileLoadFailed(0))
			{
				return false;
			}
		}

		scriptAssertf(packed_stats::GetIsBooleanType(statEnum), "%s - GET_PACKED_STAT_BOOL_CODE : statEnum '%d' is not of type boolean.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statEnum);
		if (packed_stats::GetIsBooleanType(statEnum))
		{
#if __ASSERT
			scriptAssertf(overrideCharSlot == packed_stats::USE_CURRENT_CHAR_SLOT || StatsInterface::ValidCharacterSlot(overrideCharSlot),
				"%s - GET_PACKED_STAT_BOOL_CODE : statEnum '%d', is not a character stat or has an invalid override value='%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statEnum, overrideCharSlot);
#endif //__ASSERT

			int value = 0;
			bool result = packed_stats::GetPackedValue(value, statEnum, overrideCharSlot);
			scriptAssertf(result, "%s - GET_PACKED_STAT_BOOL_CODE : Failed to set statEnum '%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statEnum);

#if __BANK
			if (CStatsMgr::GetStatsDataMgr().Bank_ScriptTestStatGetFail())
			{
				result = false;
			}
#endif // __BANK

			if (result)
			{
				if (value > 0)
				{
					return true;
				}

				return false;
			}
		}

		return false;
	}

	int CommandPackedStatGetPackedInt(const int statEnum, const int overrideCharSlot)
	{
		if (NetworkInterface::IsNetworkOpen())
		{
			scriptAssertf(!StatsInterface::CloudFileLoadPending(0), "%s : GET_PACKED_STAT_INT_CODE - Trying to Access Stat with statEnum=\"%d\" but stats are not loaded.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statEnum);
			scriptAssertf(!StatsInterface::CloudFileLoadFailed(0),  "%s : GET_PACKED_STAT_INT_CODE - Trying to Access Stat with statEnum=\"%d\" but stats loaded have failed.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statEnum);
			if(StatsInterface::CloudFileLoadPending(0) || StatsInterface::CloudFileLoadFailed(0))
			{
				return 0;
			}
		}

		scriptAssertf(packed_stats::GetIsIntType(statEnum), "%s - GET_PACKED_STAT_INT_CODE : statEnum '%d' is not of type integer.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statEnum);
		if (packed_stats::GetIsIntType(statEnum))
		{
#if __ASSERT
			scriptAssertf(overrideCharSlot == packed_stats::USE_CURRENT_CHAR_SLOT || (StatsInterface::ValidCharacterSlot(overrideCharSlot)),
				"%s - GET_PACKED_STAT_INT_CODE : statEnum '%d', is not a character stat or has an invalid override value='%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statEnum, overrideCharSlot);
#endif //__ASSERT

			int value = 0;

			bool result = packed_stats::GetPackedValue(value, statEnum, overrideCharSlot);
			scriptAssertf(result, "%s - GET_PACKED_STAT_INT_CODE : Failed to set statEnum '%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statEnum);

#if __BANK
			if (CStatsMgr::GetStatsDataMgr().Bank_ScriptTestStatGetFail())
			{
				result = false;
			}
#endif // __BANK

			if (result)
			{
				return value;
			}
		}

		return 0;
	}

	void CommandPackedStatSetPackedBool(const int statEnum, const bool value, const int overrideCharSlot)
	{
		scriptAssertf(packed_stats::GetIsBooleanType(statEnum), "%s - SET_PACKED_STAT_BOOL_CODE : statEnum '%d' is not of type boolean.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statEnum);
		if (packed_stats::GetIsBooleanType(statEnum))
		{
#if __ASSERT
			scriptAssertf(overrideCharSlot == packed_stats::USE_CURRENT_CHAR_SLOT || (StatsInterface::ValidCharacterSlot(overrideCharSlot)),
				"%s - SET_PACKED_STAT_BOOL_CODE : statEnum '%d', is not a character stat or has an invalid override value='%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statEnum, overrideCharSlot);
#endif //__ASSERT

			ASSERT_ONLY(const bool result = ) packed_stats::SetPackedValue((int)value, statEnum, overrideCharSlot);
			if (PARAM_spewStatsUsage.Get()) scriptDebugf1("SET_PACKED_STAT_BOOL_CODE - statEnum=\"%d\", value=%d", statEnum, (int)value);
			scriptAssertf(result, "%s - SET_PACKED_STAT_BOOL_CODE : Failed to set statEnum '%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statEnum);
		}
	}

	void CommandPackedStatSetPackedInt(const int statEnum, const int value, const int overrideCharSlot)
	{
		scriptAssertf(packed_stats::GetIsIntType(statEnum), "%s - SET_PACKED_STAT_INT_CODE : statEnum '%d' is not of type boolean.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statEnum);
		if (packed_stats::GetIsIntType(statEnum))
		{
#if __ASSERT
			scriptAssertf(overrideCharSlot == packed_stats::USE_CURRENT_CHAR_SLOT || (StatsInterface::ValidCharacterSlot(overrideCharSlot)),
				"%s - SET_PACKED_STAT_INT_CODE : statEnum '%d', is not a character stat or has an invalid override value='%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statEnum, overrideCharSlot);
#endif //__ASSERT

			ASSERT_ONLY(const bool result = ) packed_stats::SetPackedValue(value, statEnum, overrideCharSlot);
			if (PARAM_spewStatsUsage.Get()) scriptDebugf1("SET_PACKED_STAT_INT_CODE - statEnum=\"%d\", value=%d", statEnum, (int)value);
			scriptAssertf(result, "%s - SET_PACKED_STAT_INT_CODE : Failed to set statEnum '%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), statEnum);
		}
	}

	int CommandGetPackedBoolStatKey(const int stat, const bool singlePlayer, const bool isCharacater, const int charindex)
	{
		if (charindex >= MAX_NUM_MP_CHARS)
		{
			scriptAssertf(0, "%s:GET_PACKED_BOOL_STAT_KEY - Invalid stat slot: stat=%d, singlePlayer=%s, isCharacater=%s, charindex=%d"
				,CTheScripts::GetCurrentScriptNameAndProgramCounter()
				,stat
				,singlePlayer?"True":"False"
				,isCharacater?"True":"False"
				,charindex);

			return 0;
		}

		char key[MAX_STAT_LABEL_SIZE];

		int statindex = CommandPackedStatGetBoolStatIndex(stat);

		if (isCharacater)
		{
			snprintf(key, MAX_STAT_LABEL_SIZE, "%s%d%s%d", singlePlayer?"SP":"MP", charindex, "_PSTAT_BOOL", statindex);
		}
		else
		{
			snprintf(key, MAX_STAT_LABEL_SIZE, "%s%s%d", singlePlayer?"SP":"MP", "_PSTAT_BOOL", statindex);
		}

		StatId statKey = HASH_STAT_ID(key);

		scriptAssertf(CommandStatIsKeyValid(statKey), "%s:GET_PACKED_BOOL_STAT_KEY - Invalid stat key: [%s] stat=%d, singlePlayer=%s, isCharacater=%s, charindex=%d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), key, stat, singlePlayer?"True":"False", isCharacater?"True":"False", charindex);

		return statKey.GetHash();
	}

	int CommandGetPackedIntStatKey(const int stat, const bool singlePlayer, const bool isCharacater, const int charindex)
	{
		if (charindex >= MAX_NUM_MP_CHARS)
		{
			scriptAssertf(0, "%s:GET_PACKED_INT_STAT_KEY - Invalid stat slot: stat=%d, singlePlayer=%s, isCharacater=%s, charindex=%d"
				,CTheScripts::GetCurrentScriptNameAndProgramCounter()
				,stat
				,singlePlayer?"True":"False"
				,isCharacater?"True":"False"
				,charindex);

			return 0;
		}

		char key[MAX_STAT_LABEL_SIZE];

		const int statindex = CommandPackedStatGetIntStatIndex(stat);

		if (isCharacater)
		{
			snprintf(key, MAX_STAT_LABEL_SIZE, "%s%d%s%d", singlePlayer?"SP":"MP", charindex, "_PSTAT_INT", statindex);
		}
		else
		{
			snprintf(key, MAX_STAT_LABEL_SIZE, "%s%s%d", singlePlayer?"SP":"MP", "_PSTAT_INT", statindex);
		}

		StatId statKey = HASH_STAT_ID(key);

		scriptAssertf(CommandStatIsKeyValid(statKey), "%s:GET_PACKED_INT_STAT_KEY - Invalid stat key: [%s] stat=%d, singlePlayer=%s, isCharacater=%s, charindex=%d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), key, stat, singlePlayer?"True":"False", isCharacater?"True":"False", charindex);

		return statKey.GetHash();
	}

	int CommandGetPackedTUBoolStatKey(const int stat, const bool singlePlayer, const bool isCharacater, const int charindex)
	{
		if (charindex >= MAX_NUM_MP_CHARS)
		{
			scriptAssertf(0, "%s:GET_PACKED_TU_BOOL_STAT_KEY - Invalid stat slot: stat=%d, singlePlayer=%s, isCharacater=%s, charindex=%d"
				,CTheScripts::GetCurrentScriptNameAndProgramCounter()
				,stat
				,singlePlayer?"True":"False"
				,isCharacater?"True":"False"
				,charindex);

			return 0;
		}

		char key[MAX_STAT_LABEL_SIZE];

		int statindex = CommandPackedStatGetBoolStatIndex(stat);

		if (isCharacater)
		{
			snprintf(key, MAX_STAT_LABEL_SIZE, "%s%d%s%d", singlePlayer?"SP":"MP", charindex, "_TUPSTAT_BOOL", statindex);
		}
		else
		{
			snprintf(key, MAX_STAT_LABEL_SIZE, "%s%s%d", singlePlayer?"SP":"MP", "_TUPSTAT_BOOL", statindex);
		}

		StatId statKey = HASH_STAT_ID(key);

		scriptAssertf(CommandStatIsKeyValid(statKey), "%s:GET_PACKED_TU_BOOL_STAT_KEY - Invalid stat key: [%s] stat=%d, singlePlayer=%s, isCharacater=%s, charindex=%d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), key, stat, singlePlayer?"True":"False", isCharacater?"True":"False", charindex);

		return statKey.GetHash();
	}

	int CommandGetPackedTUIntStatKey(const int stat, const bool singlePlayer, const bool isCharacater, const int charindex)
	{
		if (charindex >= MAX_NUM_MP_CHARS)
		{
			scriptAssertf(0, "%s:GET_PACKED_TU_INT_STAT_KEY - Invalid stat slot: stat=%d, singlePlayer=%s, isCharacater=%s, charindex=%d"
				,CTheScripts::GetCurrentScriptNameAndProgramCounter()
				,stat
				,singlePlayer?"True":"False"
				,isCharacater?"True":"False"
				,charindex);

			return 0;
		}

		char key[MAX_STAT_LABEL_SIZE];

		const int statindex = CommandPackedStatGetIntStatIndex(stat);

		if (isCharacater)
		{
			snprintf(key, MAX_STAT_LABEL_SIZE, "%s%d%s%d", singlePlayer?"SP":"MP", charindex, "_TUPSTAT_INT", statindex);
		}
		else
		{
			snprintf(key, MAX_STAT_LABEL_SIZE, "%s%s%d", singlePlayer?"SP":"MP", "_TUPSTAT_INT", statindex);
		}

		StatId statKey = HASH_STAT_ID(key);

		scriptAssertf(CommandStatIsKeyValid(statKey), "%s:GET_PACKED_TU_INT_STAT_KEY - Invalid stat key: [%s] stat=%d, singlePlayer=%s, isCharacater=%s, charindex=%d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), key, stat, singlePlayer?"True":"False", isCharacater?"True":"False", charindex);

		return statKey.GetHash();
	}


	int CommandGetPackedNGBoolStatKey(const int stat, const bool singlePlayer, const bool isCharacater, const int charindex, const char* baseKey)
	{
		scriptAssertf(baseKey, "%s:GET_PACKED_NG_BOOL_STAT_KEY - baseKey is NULL.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (baseKey)
		{
			char key[MAX_STAT_LABEL_SIZE];

			int statindex = CommandPackedStatGetBoolStatIndex(stat);

			if (isCharacater)
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "%s%d%s%d", singlePlayer?"SP":"MP", charindex, baseKey, statindex);
			}
			else
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "%s%s%d", singlePlayer?"SP":"MP", baseKey, statindex);
			}

			StatId statKey = HASH_STAT_ID(key);

			scriptAssertf(CommandStatIsKeyValid(statKey), "%s:GET_PACKED_NG_BOOL_STAT_KEY - Invalid stat key: [%s] stat=%d, singlePlayer=%s, isCharacater=%s, charindex=%d, baseKey=%s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), key, stat, singlePlayer?"True":"False", isCharacater?"True":"False", charindex, baseKey);

			return statKey.GetHash();
		}

		return 0;
	}

	int CommandGetPackedNGIntStatKey(const int stat, const bool singlePlayer, const bool isCharacater, const int charindex, const char* baseKey)
	{
		scriptAssertf(baseKey, "%s:GET_PACKED_NG_INT_STAT_KEY - Profile Settings are not available.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (baseKey)
		{
			char key[MAX_STAT_LABEL_SIZE];

			const int statindex = CommandPackedStatGetIntStatIndex(stat);

			if (isCharacater)
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "%s%d%s%d", singlePlayer?"SP":"MP", charindex, baseKey, statindex);
			}
			else
			{
				snprintf(key, MAX_STAT_LABEL_SIZE, "%s%s%d", singlePlayer?"SP":"MP", baseKey, statindex);
			}

			StatId statKey = HASH_STAT_ID(key);

			scriptAssertf(CommandStatIsKeyValid(statKey), "%s:GET_PACKED_NG_INT_STAT_KEY - Invalid stat key: [%s] stat=%d, singlePlayer=%s, isCharacater=%s, charindex=%d, baseKey=%s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), key, stat, singlePlayer?"True":"False", isCharacater?"True":"False", charindex, baseKey);

			return statKey.GetHash();
		}

		return 0;
	}

	bool CommandGetBoolMasked(const int keyHash, const int bitField, const int playerIndex)
	{
		int data = 0;

		if (CommandStatGetMaskedInt(keyHash, data, bitField, 1, playerIndex))
		{
			if (0 != data)
			{
				return true;
			}
		}

		return false;
	}

	bool CommandSetBoolMasked(const int keyHash, const bool dataValue, const int bitField,  const bool coderReset)
	{
		int data = 0;

		if (dataValue)
		{
			data = 1;
		}

		if (CommandStatSetMaskedInt(keyHash, data, bitField, 1, coderReset))
		{
			return true;
		}

		return false;
	}

	void CommandCheckPackedStatsEndValue(int BANK_ONLY(value))
	{
		BANK_ONLY(netCatalog::CheckPackedStatsEndvalue(value));
	}

	bool  CommandGetPlayerHasDrivenAllVehicles()
	{
		return CStatsMgr::GetHasDrivenAllVehicles();
	}

	void  CommandSetProfileSettingHasPostedAllVehiclesDriven()
	{
		CProfileSettings& settings = CProfileSettings::GetInstance();
		scriptAssertf(settings.AreSettingsValid(), "%s:SET_HAS_POSTED_ALL_VEHICLES_DRIVEN - Profile Settings are not available.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if(settings.AreSettingsValid() && !settings.Exists(CProfileSettings::FACEBOOK_POSTED_ALL_VEHICLES_DRIVEN))
		{
			settings.Set(CProfileSettings::FACEBOOK_POSTED_ALL_VEHICLES_DRIVEN,1);
			settings.Write();
		}
	}

	void CommandSetProfileSettingPrologueComplete()
	{
		CProfileSettings& settings = CProfileSettings::GetInstance();
		scriptAssertf(settings.AreSettingsValid(), "%s:SET_PROFILE_SETTING_PROLOGUE_COMPLETE - Profile Settings are not available.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if(settings.AreSettingsValid() && !settings.Exists(CProfileSettings::PROLOGUE_COMPLETE))
		{
			settings.Set(CProfileSettings::PROLOGUE_COMPLETE,1);
			settings.Write();
		}
	}

	void CommandSetProfileSettingSpChopMissionComplete()
	{
		CProfileSettings& settings = CProfileSettings::GetInstance();
		scriptAssertf(settings.AreSettingsValid(), "%s:SET_PROFILE_SETTING_SP_CHOP_MISSION_COMPLETE - Profile Settings are not available.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if(settings.AreSettingsValid() && !settings.Exists(CProfileSettings::SP_CHOP_MISSION_COMPLETE))
		{
			settings.Set(CProfileSettings::SP_CHOP_MISSION_COMPLETE,1);
			settings.Write();
		}
	}


	void CommandSetProfileSettingRacesDone(int newvalue)
	{
		CProfileSettings& settings = CProfileSettings::GetInstance();
		scriptAssertf(settings.AreSettingsValid(), "%s:SET_PROFILE_SETTING_CREATOR_RACES_DONE - Profile Settings are not available.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if(settings.AreSettingsValid() && newvalue > settings.GetInt(CProfileSettings::MP_AWD_CREATOR_RACES_DONE))
		{
			settings.Set(CProfileSettings::MP_AWD_CREATOR_RACES_DONE, newvalue);
			settings.Write();
		}
	}

	void CommandSetProfileSettingDMDone(int newvalue)
	{
		CProfileSettings& settings = CProfileSettings::GetInstance();
		scriptAssertf(settings.AreSettingsValid(), "%s:SET_PROFILE_SETTING_CREATOR_DM_DONE - Profile Settings are not available.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if(settings.AreSettingsValid() && newvalue > settings.GetInt(CProfileSettings::MP_AWD_CREATOR_DM_DONE))
		{
			settings.Set(CProfileSettings::MP_AWD_CREATOR_DM_DONE, newvalue);
			settings.Write();
		}
	}

	void CommandSetProfileSettingCTFDone(int newvalue)
	{
		CProfileSettings& settings = CProfileSettings::GetInstance();
		scriptAssertf(settings.AreSettingsValid(), "%s:SET_PROFILE_SETTING_CREATOR_CTF_DONE - Profile Settings are not available.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if(settings.AreSettingsValid() && newvalue > settings.GetInt(CProfileSettings::MP_AWD_CREATOR_CTF_DONE))
		{
			settings.Set(CProfileSettings::MP_AWD_CREATOR_CTF_DONE, newvalue);
			settings.Write();
		}
	}

	void CommandStatSetProfileSettingValue(int profilesetting, int newvalue)
	{
		CProfileSettings& settings = CProfileSettings::GetInstance();
		scriptAssertf(settings.AreSettingsValid(), "%s:STAT_SET_PROFILE_SETTING_VALUE - Profile Settings are not available.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if(settings.AreSettingsValid())
		{
			switch(profilesetting)
			{
			case CProfileSettings::MP_CREATOR_RACES_SAVED:
			case CProfileSettings::MP_CREATOR_DM_SAVED:
			case CProfileSettings::MP_CREATOR_CTF_SAVED:
				{
					if (newvalue > settings.GetInt(static_cast<CProfileSettings::ProfileSettingId>(profilesetting)))
					{
						settings.Set(static_cast<CProfileSettings::ProfileSettingId>(profilesetting), newvalue);
						settings.Write();
					}
				}
				break;

			default:
				scriptAssertf(false, "%s:STAT_SET_PROFILE_SETTING_VALUE - Invalid Profile Settings %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), profilesetting);
			}
		}
	}

#if RSG_GEN9
	void CommandCharacterCreationOutfitSelected(const s32 iOutfit)
	{
#if GEN9_STANDALONE_ENABLED
		scriptDebugf1("%s:STATS_CHARACTER_CREATION_OUTFIT_SELECTED : iOutfit %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iOutfit);

#if IS_GEN9_PLATFORM
		if (MoneyInterface::IsWindfallAvailable())
#endif // IS_GEN9_PLATFORM
		{
			if (SCriminalCareerService::IsInstantiated())
			{
				SCriminalCareerService::GetInstance().SetOutfitSelected(iOutfit);
			}
		}

#endif // GEN9_STANDALONE_ENABLED
	}
#endif // RSG_GEN9

	void CommandCompletedCharacterCreation(const int charindex)
	{
#if GEN9_STANDALONE_ENABLED
		scriptDebugf1("%s:STATS_COMPLETED_CHARCTER_CREATION : charindex %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), charindex);

#if !__NO_OUTPUT
		scrThread::PrePrintStackTrace();
#endif

#if IS_GEN9_PLATFORM
		if (MoneyInterface::IsWindfallAvailable())
#endif // IS_GEN9_PLATFORM
		{
			if (SCriminalCareerService::IsInstantiated())
			{
				if (SCriminalCareerService::GetInstance().HasAChosenCareer() &&
					SCriminalCareerService::GetInstance().HasAnyItemsInShoppingCart())
				{
#if IS_GEN9_PLATFORM
					const int c_spentCash = SCriminalCareerService::GetInstance().GetTotalCostInShoppingCart();
					ASSERT_ONLY(const bool c_incrementSuccess = )MoneyInterface::DecrementWindfallBalance(c_spentCash, true);
					scriptAssertf(c_incrementSuccess, "%s:STATS_COMPLETED_CHARCTER_CREATION Failed to remove spent cash %d remaining Windfall cash to charindex %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), c_spentCash, charindex);
#endif // IS_GEN9_PLATFORM
					SCriminalCareerService::GetInstance().SetCareerWindfallStats(charindex);
					SCriminalCareerService::GetInstance().PurchaseItemsInShoppingCartWithWindfall(charindex);

					// Set the stat on the character to indicate it was created via the windfall flow
					StatId c_statId;
					switch (charindex)
					{
					case 0:
						c_statId = STAT_MP0_WINDFALL_CHAR;
						break;
					case 1:
						c_statId = STAT_MP1_WINDFALL_CHAR;
						break;
					default:
						scriptAssertf(0, "%s:STATS_COMPLETED_CHARCTER_CREATION Failed to assign windfall character stat as invalid character slot provided : charindex %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), charindex);
						break;
					}
					if (c_statId.IsValid() && scriptVerifyf(StatsInterface::IsKeyValid(c_statId), "%s:STATS_COMPLETED_CHARCTER_CREATION windfall character stat is missing and has not been set : charindex %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), charindex))
					{
						if (StatsInterface::GetIsBaseType(c_statId, STAT_TYPE_BOOLEAN))
						{
							StatsInterface::SetStatData(c_statId, true, STATUPDATEFLAG_ASSERTONLINESTATS);
						}
						else
						{
							scriptWarningf("%s:STATS_COMPLETED_CHARCTER_CREATION windfall character stat not a boolean type : charindex %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), charindex);
						}
					}
					
#if IS_GEN9_PLATFORM
					MoneyInterface::SetWindfallCompleted();
#endif // IS_GEN9_PLATFORM
				}
				else
				{
					scriptWarningf("%s:STATS_COMPLETED_CHARCTER_CREATION: Windfall redemtion failed : HasAChosenCareer %d HasAnyItemsInShoppingCart %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), SCriminalCareerService::GetInstance().HasAChosenCareer(), SCriminalCareerService::GetInstance().HasAnyItemsInShoppingCart());
				}
				SCriminalCareerService::GetInstance().ResetChosenMpCharacterSlot();
			}
			else
			{
				scriptDebugf1("%s:STATS_COMPLETED_CHARCTER_CREATION: Windfall redemtion skipped SCriminalCareerService does not exist", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			}
		}
#else
		(void)charindex;
#endif // GEN9_STANDALONE_ENABLED
	}

	void  CommandSetJobActivityStarted(int id, int character)
	{
		CProfileSettings& settings = CProfileSettings::GetInstance();
		scriptAssertf(settings.AreSettingsValid(), "%s:SET_JOB_ACTIVITY_ID_STARTED - Profile Settings are not available.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if(settings.AreSettingsValid())
		{
			switch (character)
			{
			case 0: settings.Set(CProfileSettings::JOB_ACTIVITY_ID_CHAR0, id); settings.Write(); break;
			case 1: settings.Set(CProfileSettings::JOB_ACTIVITY_ID_CHAR1, id); settings.Write(); break;
			case 2: settings.Set(CProfileSettings::JOB_ACTIVITY_ID_CHAR2, id); settings.Write(); break;
			case 3: settings.Set(CProfileSettings::JOB_ACTIVITY_ID_CHAR3, id); settings.Write(); break;
			case 4: settings.Set(CProfileSettings::JOB_ACTIVITY_ID_CHAR4, id); settings.Write(); break;
			default:
				break;
			}
		}

		s_cashStart = MoneyInterface::GetVCWalletBalance() + MoneyInterface::GetVCBankBalance();
	}

	void CommandMigrateClearForRestart()
	{
		scriptWarningf("%s:STAT_MIGRATE_CLEAR_FOR_RESTART - is deprecated.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}

	bool CommandMigrateMultiplayerSavegameStart(const char* UNUSED_PARAM(sourceplatform))
	{
		scriptWarningf("%s:STAT_MIGRATE_SAVEGAME_START - is deprecated.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return true;
	}

	int CommandMigrateMultiplayerSavegameGetStatus()
	{
		enum 
		{
			MIGRATE_SAVE_NONE = 0
			,MIGRATE_SAVE_PENDING = 13 // rlMigrateSave::ERROR_END
			,MIGRATE_SAVE_FINISHED
			//Catch all error
			,MIGRATE_SAVE_FAILED
		};
		scriptDebugf1("%s:STAT_MIGRATE_SAVEGAME_GET_STATUS - is deprecated.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return MIGRATE_SAVE_FINISHED;
	}

	bool CommandMigrateMultiplayerSavegameCheckStart( )
	{
		scriptDebugf1("%s:STAT_MIGRATE_CHECK_GET_IS_PLATFORM_AVAILABLE - is deprecated.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return true;
	}

	bool CommandMigrateMultiplayerSavegameCheckAlreadyDone( )
	{
		scriptDebugf1("%s:STAT_MIGRATE_CHECK_ALREADY_DONE - is deprecated.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return true;
	}


	//Possible error codes for migration failure.
	//TODO: Leaving this here for now as game logic is using it
	enum MigrationErrorCodes
	{
		MIGRATION_ERROR_NONE,
		MIGRATION_ERROR_ALREADY_DONE,
		MIGRATION_ERROR_NOT_AVAILABLE
	};

	int CommandMigrateMultiplayerSavegameGetPlatformIsAvailable(int UNUSED_PARAM(platform))
	{
		scriptDebugf1("%s:STAT_MIGRATE_CHECK_GET_IS_PLATFORM_AVAILABLE - is deprecated.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return MIGRATION_ERROR_NOT_AVAILABLE;
	}

	//Save migration error codes.
	enum eMIGRATION_STATUS
	{
		MIGRATION_STATUS_INVALID = -1
		,MIGRATION_STATUS_NONE
		,MIGRATION_STATUS_AVAILABLE
		,MIGRATION_STATUS_RUNNING
		,MIGRATION_STATUS_FAILED
		,MIGRATION_STATUS_ERROR_ALREADY_DONE
		,MIGRATION_STATUS_ERROR_NOT_AVAILABLE
	};

	eMIGRATION_STATUS  CommandMigrateMultiplayerSavegameGetPlatformStatus(const int UNUSED_PARAM(platform), scrMigrationSaveData* UNUSED_PARAM(out_pMigrationData))
	{
		scriptDebugf1("%s:STAT_MIGRATE_CHECK_GET_PLATFORM_STATUS - is deprecated.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
#if __BANK
		if (PARAM_debugSaveRequests.Get())
		{
			scrThread::PrePrintStackTrace();
		}
#endif
		return MIGRATION_STATUS_NONE;
	}

	bool  CommandGetPlayerHasPlayedLastGen( )
	{
		scriptDebugf1("%s:STAT_GET_PLAYER_HAS_PLAYED_SP_LAST_GEN - Command is depricated.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
#if __BANK
		if (PARAM_debugSaveRequests.Get())
		{
			scrThread::PrePrintStackTrace();
		}
#endif
		return false;
	}

	bool  CommandStatSaveMigrationStatusStart()
	{
		scriptDebugf1("%s:STAT_SAVE_MIGRATION_STATUS_START - Command is depricated.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
#if __BANK
		if (PARAM_debugSaveRequests.Get())
		{
			scrThread::PrePrintStackTrace();
		}
#endif
		return true;
	}

	int  CommandStatGetSaveMigrationStatus(scrSaveMigrationStatusData* UNUSED_PARAM(out_pMigrationData))
	{
		enum eSaveMigrationsStatus
		{
			//This means we can start a check.
			SMS_NONE,
			SMS_FAILED,
			SMS_CANCELED,
			SMS_SUCCEDDED,

			//This means it is currently in progress.
			SMS_PENDING,

			//This means this account already has been used in a 
			//  save migration and we saved that info in the profile settings. Account is locked from further migrations 
			//  so no need to check it anymore.
			SMS_SKIP_ACCOUNT_ALREADY_USED,

			//This means there is an issue and the game is asserting (either not online of profile settings are not valid)
			SMS_SKIP_INVALID_STATUS
		};
		scriptDebugf1("%s:STAT_GET_SAVE_MIGRATION_STATUS - Command is depricated.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
#if __BANK
		if (PARAM_debugSaveRequests.Get())
		{
			scrThread::PrePrintStackTrace();
		}
#endif
		return SMS_SUCCEDDED;
	}

	bool CommandStatSaveMigrationCancelPendingOperation( )
	{
		scriptDebugf1("%s:STAT_SAVE_MIGRATION_CANCEL_PENDING_OPERATION - Command is depricated.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
#if __BANK
		if (PARAM_debugSaveRequests.Get())
		{
			scrThread::PrePrintStackTrace();
		}
#endif
		return true;
	}

	int  CommandStatGetCancelSaveMigrationStatus( )
	{
		scriptDebugf1("%s:STAT_GET_CANCEL_SAVE_MIGRATION_STATUS - Command is depricated.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
#if __BANK
		if (PARAM_debugSaveRequests.Get())
		{
			scrThread::PrePrintStackTrace();
		}
#endif
		return netStatus::NET_STATUS_SUCCEEDED;
	}

	bool CommandStatSaveMigrationConsumeContent(const int UNUSED_PARAM(contentId), const char* UNUSED_PARAM(sourcePlatform), const char* UNUSED_PARAM(srcGamerHandle))
	{
		scriptDebugf1("%s:STAT_SAVE_MIGRATION_CONSUME_CONTENT - Command is depricated.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
#if __BANK
		if (PARAM_debugSaveRequests.Get())
		{
			scrThread::PrePrintStackTrace();
		}
#endif
		return true;
	}

	int  CommandStatGetConsumeContentSaveMigrationStatus(int& UNUSED_PARAM(errorcode))
	{
		scriptDebugf1("%s:STAT_GET_SAVE_MIGRATION_CONSUME_CONTENT_STATUS - Command is depricated.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
#if __BANK
		if (PARAM_debugSaveRequests.Get())
		{
			scrThread::PrePrintStackTrace();
		}
#endif
		return netStatus::NET_STATUS_SUCCEEDED;
	}

	void  CommandSetFreemodePrologueDone(int id, int character)
	{
		CProfileSettings& settings = CProfileSettings::GetInstance();
		scriptAssertf(settings.AreSettingsValid(), "%s:SET_FREEMODE_PROLOGUE_DONE - Profile Settings are not available.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if(settings.AreSettingsValid())
		{
			switch (character)
			{
			case 0: settings.Set(CProfileSettings::FREEMODE_PROLOGUE_COMPLETE_CHAR0, id); settings.Write(); break;
			case 1: settings.Set(CProfileSettings::FREEMODE_PROLOGUE_COMPLETE_CHAR1, id); settings.Write(); break;
			case 2: settings.Set(CProfileSettings::FREEMODE_PROLOGUE_COMPLETE_CHAR2, id); settings.Write(); break;
			case 3: settings.Set(CProfileSettings::FREEMODE_PROLOGUE_COMPLETE_CHAR3, id); settings.Write(); break;
			case 4: settings.Set(CProfileSettings::FREEMODE_PROLOGUE_COMPLETE_CHAR4, id); settings.Write(); break;
			default:
				scriptAssertf(0, "%s:SET_FREEMODE_PROLOGUE_DONE - Invalid character %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), character);
				break;
			}

			scriptDebugf1("%s:SET_FREEMODE_PROLOGUE_DONE - Set FREEMODE_PROLOGUE_COMPLETE_CHAR%d with id='%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), character, id);
		}
	}

	void CommandSetFreemodeStrandProgressionStatus(int profileSetting, int newValue)
	{
		CProfileSettings& settings = CProfileSettings::GetInstance();

		if (scriptVerifyf(settings.AreSettingsValid(), "%s:SET_FREEMODE_STRAND_PROGRESSION_STATUS - Profile Settings are not available.", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			switch(profileSetting)
			{
				case CProfileSettings::FREEMODE_STRAND_PROGRESSION_STATUS_CHAR0 :
				case CProfileSettings::FREEMODE_STRAND_PROGRESSION_STATUS_CHAR1 :
				{
					settings.Set(static_cast<CProfileSettings::ProfileSettingId>(profileSetting), newValue);
					settings.Write();
				}
				break;

			default:
				scriptAssertf(false, "%s:SET_FREEMODE_STRAND_PROGRESSION_STATUS - Invalid Profile Settings %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), profileSetting);
			}
		}
	}

	void  CommandEnableStatsTracking()
	{
		scriptDebugf1("%s:STAT_ENABLE_STATS_TRACKING - Command called.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		CStatsUtils::EnableStatsTracking();
	}
	void  CommandDisableStatsTracking()
	{
		scriptDebugf1("%s:STAT_DISABLE_STATS_TRACKING - Command called.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		CStatsUtils::DisableStatsTracking();
	}
	bool  CommandIsStatsTrackingEnabled()
	{
		return CStatsUtils::IsStatsTrackingEnabled();
	}

	bool  CommandStatRollbackSaveMigration()
	{
		scriptDebugf1("%s:STAT_ROLLBACK_SAVE_MIGRATION - Command called - SUCCEDDED.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return true;
	}

	bool  CommandStartRecordStat(const int keyHash, int recordPolicy)
	{
		scriptDebugf1("%s:STAT_START_RECORD_STAT (StatId = %d , Policy = %d) - Command called.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), keyHash, recordPolicy);
		if(scriptVerifyf(!CStatsMgr::IsRecordingStat(), "%s:STAT_START_RECORD_STAT Failed - Already recording, call stop before calling start again.", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			CStatsMgr::StartRecordingStat( (CStatsMgr::RecordStat) keyHash, (CStatsMgr::RecordStatPolicy) recordPolicy);
			return true;
		}
		return false;
	}

	bool  CommandStopRecordStat()
	{
		scriptDebugf1("%s:STAT_STOP_RECORD_STAT - Command called.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if(CStatsMgr::IsRecordingStat())
		{
			CStatsMgr::StopRecordingStat();
			return true;
		}
		return false;
	}

	bool  CommandGetRecordedStatValue(float& value)
	{
		return CStatsMgr::GetRecordedStatValue(value);
	}

	bool CommandIsRecordingStat()
	{
		return CStatsMgr::IsRecordingStat();
	}

	int  CommandGetCurrentNearMissNoCrashValue()
	{
		return (int)CStatsMgr::GetCurrentNearMissNoCrash();
	}

	int  CommandGetCurrentNearMissNoCrashPreciseValue()
	{
		return (int)CStatsMgr::GetCurrentNearMissNoCrashPrecise();
	}

	float  CommandGetCurrentRearWheelDistance()
	{
		return CStatsMgr::GetCurrentRearWheelDistance();
	}

	float  CommandGetCurrentFrontWheelDistance()
	{
		return CStatsMgr::GetCurrentFrontWheelDistance();
	}

	float CommandGetCurrentJumpDistance()
	{
		return CStatsMgr::GetCurrentJumpDistance();
	}

	float CommandGetLastJumpHeight()
	{
		return CStatsMgr::GetLastJumpHeight();
	}

	float CommandGetCurrentDriveNoCrashDistance()
	{
		return CStatsMgr::GetDriveNoCrashDist();
	}

	float CommandGetCurrentSpeed()
	{
		return CStatsMgr::GetCurrentSpeed();
	}

	int CommandGetCurrentFlips()
	{
		return (int) CStatsMgr::GetVehicleFlipsAccumulator();
	}

	int CommandGetCurrentSpins()
	{
		return (int) CStatsMgr::GetVehicleSpinsAccumulator();
	}

	float CommandGetCurrentDrivingReverseDistance()
	{
		return CStatsMgr::GetCurrentDrivingReverseDistance();
	}

	float CommandGetCurrentSkydivingDistance()
	{
		return CStatsMgr::GetCurrentSkydivingDistance();
	}

	float CommandGetCurrentFootAltitude()
	{
		return CStatsMgr::GetCurrentFootAltitude();
	}

	float CommandGetFlyingCounter()
	{
		return CStatsMgr::GetFlyingCounter();
	}

	float CommandGetFlyingDistance()
	{
		return CStatsMgr::GetFlyingDistance();
	}

	bool CommandGetFlyingAltitude(float& value)
	{
		return CStatsMgr::GetFlyingAltitude(value);
	}

	bool CommandIsPlayerVehicleAboveOcean()
	{
		return CStatsMgr::IsPlayerVehicleAboveOcean();
	}

	float CommandGetPlaneBarrelRollCounter()
	{
		return CStatsMgr::GetPlaneBarrelRollCounter();
	}

	float CommandGetVehicleBailDistance()
	{
		return CStatsMgr::GetVehicleBailDistance();
	}
	float CommandGetFreeFallAltitude()
	{
		return CStatsMgr::GetFreeFallAltitude();
	}
	float CommandGetLastPlayersVehiclesDamages()
	{
		return CStatsMgr::GetLastPlayersVehiclesDamages();
	}
	float CommandGetBoatDistanceOnGround()
	{
		return CStatsMgr::GetBoatDistanceOnGround();
	}

    float CommandGetWallridingDistance()
    {
        return CStatsMgr::GetWallridingDistance();
    }

	void  CommandResetCurrentNearMissNoCrash()
	{
		scriptDebugf1("%s:STAT_RESET_CURRENT_NEAR_MISS_NOCRASH - Command called.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		CStatsMgr::ResetCurrentNearMissNoCrash();
	}

	void  CommandSetProfileSettingHasSpecialEditionContent(int value)
	{
		CProfileSettings& settings = CProfileSettings::GetInstance();
		scriptAssertf(settings.AreSettingsValid(), "%s:SET_HAS_SPECIALEDITION_CONTENT - Profile Settings are not available.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if(settings.AreSettingsValid())
		{
			scriptDebugf1("%s:SET_HAS_SPECIALEDITION_CONTENT - Command called - value='%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), value);
			settings.Set(CProfileSettings::GAMER_HAS_SPECIALEDITION_CONTENT, value);
			settings.Write();
		}
	}

	void  CommandSetSaveMigrationTransactionIdWarning(int value)
	{
		CProfileSettings& settings = CProfileSettings::GetInstance();
		scriptAssertf(settings.AreSettingsValid(), "%s:SET_SAVE_MIGRATION_TRANSACTION_ID_WARNING - Profile Settings are not available.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if(settings.AreSettingsValid())
		{
			scriptDebugf1("%s:SET_SAVE_MIGRATION_TRANSACTION_ID_WARNING - Command called - value='%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), value);
			settings.Set(CProfileSettings::SAVE_MIGRATION_TRANSACTION_ID_WARNING, value);
			settings.Write();
		}
	}

	void  CommandGetBossGoonUUID(int characterSlot, int& valueA, int& valueB)
	{
		valueA = -1;
		valueB = -1;

		scriptAssertf(!StatsInterface::CloudFileLoadPending(STAT_MP_CATEGORY_DEFAULT) && !StatsInterface::CloudFileLoadFailed(STAT_MP_CATEGORY_DEFAULT), "%s: GET_BOSS_GOON_UUID - Failed - Stats are not loaded.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if(StatsInterface::CloudFileLoadPending(STAT_MP_CATEGORY_DEFAULT) || StatsInterface::CloudFileLoadFailed(STAT_MP_CATEGORY_DEFAULT))
			return;

		scriptAssertf(characterSlot >= 0 && characterSlot < MAX_NUM_MP_CHARS, "%s: GET_BOSS_GOON_UUID - Failed - Invalid character='%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), characterSlot);
		if(characterSlot < 0 || characterSlot >= MAX_NUM_MP_CHARS)
			return;

		const u64 value = StatsInterface::GetBossGoonUUID(characterSlot);

		valueA = 0;
		valueB = 0;

		valueB = static_cast<int>(value&0x00000000FFFFFFFF);
		valueA = static_cast<int>(value>>32);

		scriptDebugf1("%s:GET_BOSS_GOON_UUID - Command called - valueA='%d', valueB='%d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), valueA, valueB);
	}

	class MetricBecomeBoss : public MetricPlayStat
	{
		RL_DECLARE_METRIC(SBB, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

	public:

		MetricBecomeBoss(u64 bossId, int version) 
            : m_bossId(bossId)
            , m_version(version)
        {;}

		virtual bool Write(RsonWriter* rw) const
		{
			return (m_bossId != 0)
                && MetricPlayStat::Write(rw)
                && rw->WriteInt64("bid", m_bossId)
                && rw->WriteInt64("v", m_version);
		}

    private:

		u64 m_bossId;					// Unique BossID
        int m_version;                  // Boss and Goon version
	};

	void  CommandStartBeingBoss(int version = 1)
    {
        const StatId statId = StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID");
        u64 uuidValue = StatsInterface::GetUInt64Stat(statId);
        const StatId versionStatId = StatsInterface::GetStatsModelHashId("BOSS_GOON_VERSION");
        ASSERT_ONLY(int versionValue = StatsInterface::GetIntStat(versionStatId);)

        scriptAssertf(0 == uuidValue, "%s: START_BEING_BOSS - Failed - BOSS_GOON_UUID already has a value < %" I64FMT "d >.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), uuidValue);
        scriptAssertf(0 == versionValue, "%s: START_BEING_BOSS - Failed - BOSS_GOON_VERSION already has a value < %d >.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), versionValue);

		//make sure its not set.
		if (0 == uuidValue)
		{
			const bool success = rlCreateUUID(&uuidValue);
			scriptAssertf(success, "%s: START_BEING_BOSS - Failed to create the UUID - uuidValue='%" SIZETFMT "d'..", CTheScripts::GetCurrentScriptNameAndProgramCounter(), uuidValue);

			if (success)
			{
				MetricBecomeBoss m(uuidValue, version);
				APPEND_METRIC(m);
				StatsInterface::SetStatData(statId, uuidValue);
                StatsInterface::SetStatData(versionStatId, version);
			}
			else
			{
				scriptErrorf("%s: START_BEING_BOSS - Failed to create UUID.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			}
		}
		scriptDebugf1("%s:START_BEING_BOSS - Command called - BossId : %" I64FMT "d (version %d).", CTheScripts::GetCurrentScriptNameAndProgramCounter(), uuidValue, version);
	}
	
	class MetricBecomeGoon : public MetricPlayStat
	{
		RL_DECLARE_METRIC(SBG, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

	public:

		MetricBecomeGoon(u64 bossId, int inviteMode) 
			: m_bossId(bossId)
			, m_inviteMode(inviteMode)		
		{;}

		virtual bool Write(RsonWriter* rw) const
		{
			return (m_bossId != 0)
				&& MetricPlayStat::Write(rw)
				&& rw->WriteInt64("bid", m_bossId)
				&& rw->WriteInt("im", m_inviteMode);
		}

	private:

		u64 m_bossId;					//Unique BossID
		int m_inviteMode;				// Method of Invite (Nearby, Friends, Crew, Individual)
	};

	void CommandStartBeingGoon(int bossId1, int bossId2, int inviteMode = 0)
	{
		const u64 bid0 = ((u64)bossId1) << 32;
		const u64 bid1 = (0x00000000FFFFFFFF&(u64)bossId2);

		u64 bossId = bid0|bid1;

		scriptAssertf(bossId != 0, "%s: START_BEING_GOON - Failed - Invalid BossId %" I64FMT "d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), bossId);
		scriptAssertf(inviteMode >= 0, "%s: START_BEING_GOON - Failed - Incorrect Invite mode value %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), inviteMode);

		//make sure the bossId is valid
		if (bossId != 0 && inviteMode >= 0)
		{
			MetricBecomeGoon m(bossId, inviteMode);
			APPEND_METRIC(m);
		}
		scriptDebugf1("%s:START_BEING_GOON - Command called - BossId : %" I64FMT "d / Invite mode : %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), bossId, inviteMode);
	}

	class MetricEndBeingBoss : public MetricPlayStat
	{
		RL_DECLARE_METRIC(EBB, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

	public:

		u64 m_bossId;					//Unique BossID
		int m_timesPlayedAsBoss;		//Times played as Boss previously
		int m_timesPlayedAsGoon;		//Times played as Goon previously
		int m_fmSessionsEntered;		//Freemode sessions entered
		int m_jobSessionsEntered;		//Job sessions entered
		int m_nearbyInvites; 
		int m_friendInvites;
		int m_crewInvites;
		int m_individualInvites;
		int m_lfwInvites;
		int m_goonInvitesAccepted;		//Number of Goon invites accepted
		int m_goonsQuit;				//Number of Goon quit.
		int m_goonsTerminationBounties;	//Number of Goons given Termination Bounties.
		int m_goonsSacked;				//Number of Goons sacked
		int m_endingReason;				//Reasons for ending Step down/ expired when online/ expired when offline
		int m_totalTime;				//Total Time of Boss Period
		int m_time;						//Time being a boss for this period

		MetricEndBeingBoss(	u64 bossId, 
							int timesPrevPlayedAsBoss, 
							int timesPrevPlayedAsGoon, 
							int bossPeriod, 
							int fmSession, 
							int jobSession, 
							int nearbyInvites, 
							int friendInvites,
							int crewInvites,
							int individualInvites,
							int lfwInvites, 
							int goonInvitesAccepted, 
							int goonQuit, 
							int goonTermBounty, 
							int goonSacked, 
							int endingReason,
							int time) 
			: m_bossId(bossId)
			, m_timesPlayedAsBoss(timesPrevPlayedAsBoss)
			, m_timesPlayedAsGoon(timesPrevPlayedAsGoon)
			, m_fmSessionsEntered(fmSession)
			, m_jobSessionsEntered(jobSession)
			, m_nearbyInvites(nearbyInvites) 
			, m_friendInvites(friendInvites)
			, m_crewInvites(crewInvites)
			, m_individualInvites(individualInvites)
			, m_lfwInvites(lfwInvites)
			, m_goonInvitesAccepted(goonInvitesAccepted)
			, m_goonsQuit(goonQuit)
			, m_goonsTerminationBounties(goonTermBounty)
			, m_goonsSacked(goonSacked)
			, m_endingReason(endingReason)
			, m_totalTime(bossPeriod)
			, m_time(time)
		{;}

		virtual bool Write(RsonWriter* rw) const
		{
			return (m_bossId != 0)
				&& MetricPlayStat::Write(rw)
				&& rw->WriteInt64("bid", m_bossId)
				&& rw->WriteInt("a", m_timesPlayedAsBoss)
				&& rw->WriteInt("b", m_timesPlayedAsGoon)
				&& rw->WriteInt("c", m_fmSessionsEntered)
				&& rw->WriteInt("d", m_jobSessionsEntered)
				&& rw->WriteInt("e", m_nearbyInvites)
				&& rw->WriteInt("f", m_friendInvites)
				&& rw->WriteInt("g", m_crewInvites)
				&& rw->WriteInt("h", m_individualInvites)
				&& rw->WriteInt("i", m_lfwInvites)
				&& rw->WriteInt("j", m_goonInvitesAccepted)
				&& rw->WriteInt("k", m_goonsQuit)
				&& rw->WriteInt("l", m_goonsTerminationBounties)
				&& rw->WriteInt("m", m_goonsSacked)
				&& rw->WriteInt("n", m_endingReason)
				&& rw->WriteInt("o", m_totalTime)
				&& rw->WriteInt("p", m_time);
		}
	};


	void  CommandClearBossGoonUUID()
	{
		scriptAssertf(0, "%s: CLEAR_BOSS_GOON_UUID - COMMAND DEPRECATED, use END_BEING_BOSS instead", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		const StatId statId = StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID");

		scriptDebugf1("%s:CLEAR_BOSS_GOON_UUID - Command called - value='%" SIZETFMT "d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), StatsInterface::GetUInt64Stat(statId));

		const u64 value = 0;
		StatsInterface::SetStatData(statId, value);

		scriptAssertf(0, "%s:CLEAR_BOSS_GOON_UUID - This command is deprecated, please use END_BEING_BOSS", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}

	void  CommandEndBeingBoss(int endingReason, int time)
	{
        const StatId statId = StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID");
        const StatId versionStatId = StatsInterface::GetStatsModelHashId("BOSS_GOON_VERSION");

		const u64 previousValue = StatsInterface::GetUInt64Stat(statId);

		scriptAssertf(previousValue != 0, "%s:END_BEING_BOSS - No BossId set", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if(previousValue != 0)
		{
			const StatId countBossStatId = StatsInterface::GetStatsModelHashId("TIMES_PREV_PLAY_AS_BOSS");
			const int timesPrevPlayedAsBoss = StatsInterface::GetIntStat(countBossStatId);

			const StatId countGoonStatId = StatsInterface::GetStatsModelHashId("GBTELTIMESPLAYEDGOONPREV");
			const int timesPrevPlayedAsGoon = StatsInterface::GetIntStat(countGoonStatId);

			const StatId bossPeriodStatId = StatsInterface::GetStatsModelHashId("GBTELTOTAL_TIME_BOSS");
			const int bossPeriod = StatsInterface::GetIntStat(bossPeriodStatId);

			const StatId fmSessionStatId = StatsInterface::GetStatsModelHashId("GBTELNOFM_SESSIONS_ENT");
			const int fmSession = StatsInterface::GetIntStat(fmSessionStatId);

			const StatId jobSessionStatId = StatsInterface::GetStatsModelHashId("GBTELNOJOB_SESSIONS_ENT");
			const int jobSession = StatsInterface::GetIntStat(jobSessionStatId);
			
			const StatId goonInvitesAcceptedStatId = StatsInterface::GetStatsModelHashId("GBTELNOGOON_INVITES_ACP");
			const int goonInvitesAccepted = StatsInterface::GetIntStat(goonInvitesAcceptedStatId);

			const StatId goonQuitStatId = StatsInterface::GetStatsModelHashId("GBTELNOGOON_QUIT");
			const int goonQuit = StatsInterface::GetIntStat(goonQuitStatId);

			const StatId goonTermBountyStatId = StatsInterface::GetStatsModelHashId("GBTELNOGOON_GIVTERMBOUN");
			const int goonTermBounty = StatsInterface::GetIntStat(goonTermBountyStatId);

			const StatId goonSackedStatId = StatsInterface::GetStatsModelHashId("GBTELNOGOON_SACKED");
			const int goonSacked = StatsInterface::GetIntStat(goonSackedStatId);


			const StatId nearbyInvitesStatId = StatsInterface::GetStatsModelHashId("BOSS_NEARBY_INVITE_COUNT");
			const int nearbyInvites = StatsInterface::GetIntStat(nearbyInvitesStatId);

			const StatId friendInvitesStatId = StatsInterface::GetStatsModelHashId("BOSS_FRIEND_INVITE_COUNT");
			const int friendInvites = StatsInterface::GetIntStat(friendInvitesStatId);

			const StatId crewInvitesStatId = StatsInterface::GetStatsModelHashId("BOSS_CREW_INVITE_COUNT");
			const int crewInvites = StatsInterface::GetIntStat(crewInvitesStatId);

			const StatId individualInvitesStatId = StatsInterface::GetStatsModelHashId("BOSS_IND_INVITE_COUNT");
			const int individualInvites = StatsInterface::GetIntStat(individualInvitesStatId);

			const StatId lfwInvitesStatId = StatsInterface::GetStatsModelHashId("BOSS_LFW_INVITE_COUNT");
			const int lfwInvites = StatsInterface::GetIntStat(lfwInvitesStatId);

			MetricEndBeingBoss m(	previousValue, 
									timesPrevPlayedAsBoss, 
									timesPrevPlayedAsGoon, 
									bossPeriod, 
									fmSession, 
									jobSession, 
									nearbyInvites, 
									friendInvites,
									crewInvites,
									individualInvites,
									lfwInvites,
									goonInvitesAccepted, 
									goonQuit, 
									goonTermBounty, 
									goonSacked, 
									endingReason,
									time);
			APPEND_METRIC(m);

			StatsInterface::SetStatData(countBossStatId, timesPrevPlayedAsBoss+1);		
			StatsInterface::SetStatData(fmSessionStatId, 0);			
			StatsInterface::SetStatData(jobSessionStatId, 0);
			StatsInterface::SetStatData(goonQuitStatId, 0);	
			StatsInterface::SetStatData(goonTermBountyStatId, 0);		
			StatsInterface::SetStatData(goonSackedStatId, 0);

			StatsInterface::SetStatData(nearbyInvitesStatId, 0);
			StatsInterface::SetStatData(friendInvitesStatId, 0);
			StatsInterface::SetStatData(crewInvitesStatId, 0);
			StatsInterface::SetStatData(individualInvitesStatId, 0);
			StatsInterface::SetStatData(lfwInvitesStatId, 0);
		}

		scriptDebugf1("%s:END_BEING_BOSS - Command called - value='%" SIZETFMT "d'.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), previousValue);

		const u64 value = 0;
        StatsInterface::SetStatData(statId, value);
        StatsInterface::SetStatData(versionStatId, (const int) 0);
	}

	class MetricEndBeingGoon : public MetricPlayStat
	{
		RL_DECLARE_METRIC(EBG, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

	public:

		u64 m_bossId;					//Unique BossID
		int m_timesPlayedAsBoss;		//Times played as Boss previously
		int m_timesPlayedAsGoon;		//Times played as Goon previously
		int m_goonInvitesAccepted;      //Number of Goon invites accepted
		int m_totalGoonTime;			//Total Time played as Goon
		int m_fmSessionsEntered;		//Freemode sessions entered
		int m_jobSessionsEntered;		//Job sessions entered
		int m_endingReason;				//Reasons for ending Step down/ expired when online/ expired when offline
		int m_rpBonus;					//Reasons for ending Step down/ expired when online/ expired when offline
		int m_time;						//time being a Goon for this period

		MetricEndBeingGoon(	u64 bossId, 
							int timesPrevPlayedAsBoss, 
							int timesPrevPlayedAsGoon, 
							int goonInvitesAccepted, 
							int totalGoonTime,
							int fmSession, 
							int jobSession, 
							int endingReason,
							int rpBonus,
							int time) 
			: m_bossId(bossId)
			, m_timesPlayedAsBoss(timesPrevPlayedAsBoss)
			, m_timesPlayedAsGoon(timesPrevPlayedAsGoon)
			, m_goonInvitesAccepted(goonInvitesAccepted)
			, m_totalGoonTime(totalGoonTime)
			, m_fmSessionsEntered(fmSession)
			, m_jobSessionsEntered(jobSession)
			, m_endingReason(endingReason)
			, m_rpBonus(rpBonus)
			, m_time(time)
		{;}

		virtual bool Write(RsonWriter* rw) const
		{
			return (m_bossId != 0)
				&& MetricPlayStat::Write(rw)
				&& rw->WriteInt64("bid", m_bossId)
				&& rw->WriteInt("a", m_timesPlayedAsBoss)
				&& rw->WriteInt("b", m_timesPlayedAsGoon)
				&& rw->WriteInt("c", m_fmSessionsEntered)
				&& rw->WriteInt("d", m_jobSessionsEntered)
				&& rw->WriteInt("e", m_endingReason)
				&& rw->WriteInt("f", m_rpBonus)
				&& rw->WriteInt("g", m_totalGoonTime)
				&& rw->WriteInt("h", m_time)
				&& rw->WriteInt("i", m_goonInvitesAccepted);
		}
	};
	

	void  CommandEndBeingGoon(int bossId1, int bossId2, int endingReason, int rpBonus, int time = 0)
	{
		const u64 bid0 = ((u64)bossId1) << 32;
		const u64 bid1 = (0x00000000FFFFFFFF&(u64)bossId2);

		u64 bossId = bid0|bid1;

		scriptAssertf(bossId != 0, "%s:END_BEING_GOON - Invalid BossId", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if(bossId != 0)
		{
			const StatId countBossStatId = StatsInterface::GetStatsModelHashId("TIMES_PREV_PLAY_AS_BOSS");
			const int timesPrevPlayedAsBoss = StatsInterface::GetIntStat(countBossStatId);

			const StatId countGoonStatId = StatsInterface::GetStatsModelHashId("GBTELTIMESPLAYEDGOONPREV");
			const int timesPrevPlayedAsGoon = StatsInterface::GetIntStat(countGoonStatId);

			const StatId totalGoonTimeStatId = StatsInterface::GetStatsModelHashId("GBTELTOTAL_TIME_GOON");
			const int totalGoonTime = StatsInterface::GetIntStat(totalGoonTimeStatId);

			const StatId fmSessionStatId = StatsInterface::GetStatsModelHashId("GBTELNOFM_SESSIONS_ENT");
			const int fmSession = StatsInterface::GetIntStat(fmSessionStatId);

			const StatId jobSessionStatId = StatsInterface::GetStatsModelHashId("GBTELNOJOB_SESSIONS_ENT");
			const int jobSession = StatsInterface::GetIntStat(jobSessionStatId);

			const StatId goonInvitesAcceptedStatId = StatsInterface::GetStatsModelHashId("GBTELNOGOON_INVITES_ACP");
			const int goonInvitesAccepted = StatsInterface::GetIntStat(goonInvitesAcceptedStatId);

			MetricEndBeingGoon m(bossId, timesPrevPlayedAsBoss, timesPrevPlayedAsGoon, goonInvitesAccepted, totalGoonTime, fmSession, jobSession, endingReason, rpBonus, time);
			APPEND_METRIC(m);


			StatsInterface::SetStatData(countGoonStatId, timesPrevPlayedAsGoon+1);				
			StatsInterface::SetStatData(fmSessionStatId, 0);			
			StatsInterface::SetStatData(jobSessionStatId, 0);	
		}

		scriptDebugf1("%s:END_BEING_GOON - Command called with BossId = %" SIZETFMT "d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), bossId);
	}


	class MetricHiredLimo : public MetricPlayStat
	{
		RL_DECLARE_METRIC(HL, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

		u64 m_bossId;

	public:

		MetricHiredLimo(u64 bossId)
			: m_bossId(bossId)
		{;}

		virtual bool Write(RsonWriter* rw) const
		{
			return MetricPlayStat::Write(rw)
				&& rw->WriteInt64("bid", m_bossId);
		}
	};

	void  CommandHiredLimo(int bossId1, int bossId2)
	{
		const u64 bid0 = ((u64)bossId1) << 32;
		const u64 bid1 = (0x00000000FFFFFFFF&(u64)bossId2);

		u64 bossId = bid0|bid1;

		scriptAssertf(bossId != 0, "%s:HIRED_LIMO - Invalid BossId", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if(bossId != 0)
		{
			MetricHiredLimo m(bossId);
			APPEND_METRIC(m);
		}

		scriptDebugf1("%s:HIRED_LIMO - Command called.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}

	class MetricOrderBossVehicle : public MetricPlayStat
	{
		RL_DECLARE_METRIC(OBV, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

		u64 m_bossId;
		int m_vehicleId;
		u64 m_localbossId;

	public:

		MetricOrderBossVehicle(u64 bossId, int vehicleId) : m_bossId(bossId)
			, m_vehicleId(vehicleId)
			, m_localbossId(StatsInterface::GetUInt64Stat(StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID")))
		{;}

		virtual bool Write(RsonWriter* rw) const
		{
			return MetricPlayStat::Write(rw)
				&& rw->WriteInt64("bid", m_bossId)
				&& rw->WriteBool("isboss", m_bossId == m_localbossId)
				&& rw->WriteInt("v", m_vehicleId);
		}
	};

	void  CommandOrderBossVehicle(int bossId1, int bossId2, int vehicleId)
	{
		const u64 bid0 = ((u64)bossId1) << 32;
		const u64 bid1 = (0x00000000FFFFFFFF&(u64)bossId2);

		u64 bossId = bid0|bid1;
        scriptAssertf(bossId != 0, "%s:ORDER_BOSS_VEHICLE - Invalid BossId", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if(bossId != 0)
		{
			MetricOrderBossVehicle m(bossId, vehicleId);
			APPEND_METRIC(m);
		}

		scriptDebugf1("%s:ORDER_BOSS_VEHICLE - Command called with vehicle %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), vehicleId);
	}

	class MetricChangeUniform : public MetricPlayStat
	{
		RL_DECLARE_METRIC(CU, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

		u64 m_bossId;
		int m_uniform;

	public:

		MetricChangeUniform(u64 bossId, int uniform) : m_bossId(bossId)
			, m_uniform(uniform)
		{;}

		virtual bool Write(RsonWriter* rw) const
		{
			return MetricPlayStat::Write(rw)
				&& rw->WriteInt64("bid", m_bossId)
				&& rw->WriteInt("u", m_uniform);
		}
	};

	void  CommandChangeUniform(int bossId1, int bossId2, int uniform)
	{
		const u64 bid0 = ((u64)bossId1) << 32;
		const u64 bid1 = (0x00000000FFFFFFFF&(u64)bossId2);

		u64 bossId = bid0|bid1;

		scriptAssertf(bossId != 0, "%s:CHANGE_UNIFORM - Invalid BossId", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if(bossId != 0)
		{
			MetricChangeUniform m(bossId, uniform);
			APPEND_METRIC(m);
		}

		scriptDebugf1("%s:CHANGE_UNIFORM - Command called with uniform %d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), uniform);
	}

	class MetricChangeGoonLookingForWork : public MetricPlayStat
	{
		RL_DECLARE_METRIC(CGLFW, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

		bool m_value;

	public:

		MetricChangeGoonLookingForWork(bool value) : m_value(value)
		{;}

		virtual bool Write(RsonWriter* rw) const
		{
			return MetricPlayStat::Write(rw)
				&& rw->WriteBool("v", m_value);
		}
	};

	void CommandChangeGoonLookingForWork(bool value)
	{
		static bool s_previousValue = false;
		static u32 s_lastTimeMetricSent = sysTimer::GetSystemMsTime();
		static const unsigned MIN_SEND_PERIOD = 60000; // 1 minute

		if(value == s_previousValue)
			return;

		if(s_lastTimeMetricSent + MIN_SEND_PERIOD > sysTimer::GetSystemMsTime())
			return;

		const StatId statId = StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID");
		const u64 bossId = StatsInterface::GetUInt64Stat(statId);

		scriptAssertf(bossId == 0, "%s:CHANGE_GOON_LOOKING_FOR_WORK - BossId is set, you're not a Goon", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		if(bossId == 0)
		{
			MetricChangeGoonLookingForWork m(value);
			APPEND_METRIC(m);

			s_lastTimeMetricSent = sysTimer::GetSystemMsTime();
		}

		s_previousValue = value;

		scriptDebugf1("%s:CHANGE_GOON_LOOKING_FOR_WORK - Command called with value %s.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), (value) ? "true" : "false");
	}


	//   Metric sent when "ghosting to player" is activated
	class MetricGHOSTING_TO_PLAYER: public MetricPlayStat
	{
		RL_DECLARE_METRIC(GHOSTING, TELEMETRY_CHANNEL_AMBIENT, LOGLEVEL_MEDIUM_PRIORITY);

	public:

		MetricGHOSTING_TO_PLAYER(const rlGamerHandle& hGamer)
			: MetricPlayStat()
			, m_playerAccountId(0)
		{
			if (hGamer.IsValid())
			{
				hGamer.ToUserId(m_player);
				hGamer.ToString(m_account, COUNTOF(m_account));
				CNetGamePlayer* pPlayer = NetworkInterface::GetPlayerFromGamerHandle(hGamer);
				if (pPlayer)
				{
					m_playerAccountId = pPlayer->GetPlayerAccountId();
				}
				
			}
			else
			{
				m_player[0] = '\0';
				m_account[0] = '\0';
			}
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);
			result &= rw->WriteString("p", m_player);
			result &= rw->WriteString("a", m_account);
			result &= rw->WriteInt("i", m_playerAccountId);
			return result;
		}

		char m_player[RL_MAX_USERID_BUF_LENGTH];
		char m_account[RL_MAX_GAMER_HANDLE_CHARS];
		s32 m_playerAccountId;
	};

	void CommandNetworkSendGhostingToPlayer(int& hGamerHandleData)
	{
		rlGamerHandle gamerhandle;
		if(CTheScripts::ImportGamerHandle(&gamerhandle, hGamerHandleData, SCRIPT_GAMER_HANDLE_SIZE  ASSERT_ONLY(, "SEND_METRIC_GHOSTING_TO_PLAYER")))
		{
			MetricGHOSTING_TO_PLAYER m(gamerhandle);
			APPEND_METRIC(m);
			scriptDebugf1("%s:SEND_METRIC_GHOSTING_TO_PLAYER - Command called.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}

    int CommandStatGetLastSuccessfulStuntJump()
    {
        return SStuntJumpManager::GetInstance().GetLastSuccessfulStuntJump();
    }



    class MetricVIP_POACH: public MetricPlayStat
    {
        RL_DECLARE_METRIC(POACH, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

    public:
        MetricVIP_POACH(u64 bossId, u64 otherBossId, bool isVip) 
            : MetricPlayStat()
            , m_bossId(bossId)
            , m_otherBossId(otherBossId)
            , m_isVip(isVip)
        {	
        }

        virtual bool Write(RsonWriter* rw) const
        {
            return MetricPlayStat::Write(rw)
                && rw->WriteInt64("bid", m_bossId)
                && rw->WriteInt64("obid", m_otherBossId)
                && rw->WriteBool("vip", m_isVip);
        }

        u64 m_bossId;
        u64 m_otherBossId;
        bool m_isVip;
    };

    void CommandNetworkSendVipPoach(int bossId1, int bossId2, bool isVip)
    {
        const StatId bossIdStat = StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID");
        const u64 bossId = StatsInterface::GetUInt64Stat(bossIdStat);

        scriptAssertf(bossId, "%s: SEND_METRIC_VIP_POACH - invalid BossId.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
        if (!bossId)
            return;

        const u64 bid0 = ((u64)bossId1) << 32;
        const u64 bid1 = (0x00000000FFFFFFFF&(u64)bossId2);

        u64 otherBossId = bid0|bid1;

        scriptAssertf(otherBossId, "%s: SEND_METRIC_VIP_POACH - invalid other player BossId.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
        if (!otherBossId)
            return;

        MetricVIP_POACH m(bossId, otherBossId, isVip);
        APPEND_METRIC(m);
    }


    class MetricPUNISH_BODYGUARD: public MetricPlayStat
    {
        RL_DECLARE_METRIC(PUNISH, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

    public:
        MetricPUNISH_BODYGUARD(bool punished) 
            : MetricPlayStat()
            , m_punished(punished)
        {	
        }

        virtual bool Write(RsonWriter* rw) const
        {
            return MetricPlayStat::Write(rw)
                && rw->WriteBool("p", m_punished);
        }

        bool m_punished;
    };

    void CommandNetworkSendPunishBodyguard(int punished)
    {
        const StatId bossIdStat = StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID");
        const u64 bossId = StatsInterface::GetUInt64Stat(bossIdStat);

        scriptAssertf(bossId, "%s: SEND_METRIC_PUNISH_BODYGUARD - invalid BossId.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
        if (!bossId)
            return;

        MetricPUNISH_BODYGUARD m(punished != 0);
        APPEND_METRIC(m);
    }

    void CommandNetworkStartTrackingStunts()
    {
        scriptDebugf1("%s:PLAYSTATS_START_TRACKING_STUNTS - Command called.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
        CStatsMgr::StartTrackingStunts();
    }
    void CommandNetworkStopTrackingStunts()
    {
        scriptDebugf1("%s:PLAYSTATS_STOP_TRACKING_STUNTS - Command called.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
        CStatsMgr::StopTrackingStunts();
    }

	class MetricMISSION_ENDED: public MetricPlayStat
	{
		RL_DECLARE_METRIC(MISSION_ENDED, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		MetricMISSION_ENDED() 
			: MetricPlayStat()
			, m_a(-1)
			, m_b(-1)
			, m_c(-1)
			, m_d(-1)
			, m_e(-1)
			, m_f(-1)
			, m_g(-1)
			, m_h(-1)
			, m_i(-1)
			, m_j(-1)
			, m_k(-1)
			, m_l(-1)
			, m_m(-1)
			, m_n(-1)
			, m_o(-1)
			, m_p(-1)
			, m_q(-1)
			, m_r(-1)
			, m_s(-1)
			, m_t(-1)
			, m_u(-1)
			, m_v(-1)
			, m_w(-1)
			, m_x(-1)
			, m_y(-1)
			, m_z(-1)
			, m_ab(-1)
			, m_bb(-1)
			, m_cb(-1)
			, m_db(-1)
			, m_eb(-1)
			, m_fb(-1)
			, m_gb(-1)
			, m_hb(-1)
			, m_ib(-1)
			, m_jb(-1)
			, m_kb(-1)
			, m_lb(-1)
			, m_mb(-1)
			, m_nb(-1)
			, m_ob(-1)
			, m_pb(-1)
			, m_qb(-1)
			, m_rb(-1)
			, m_sb(-1)
		{	
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);

			result &= rw->WriteInt("mtid", m_missionTypeId);
			result &= rw->WriteInt("mid", m_missionId);
			result &= rw->WriteInt64("maid", m_matchId);
			result &= rw->WriteInt64("buuid", m_bossId);
			result &= rw->WriteInt64("sbuuid", m_starterBossId);
			result &= rw->WriteInt("isboss", m_isboss);

			if (m_a > -1) result &= rw->WriteInt("a", m_a);
			if (m_b > -1) result &= rw->WriteInt("b", m_b);
			if (m_c > -1) result &= rw->WriteInt("c", m_c);
			if (m_d > -1) result &= rw->WriteInt("d", m_d);
			if (m_e > -1) result &= rw->WriteInt("e", m_e);
			if (m_f > -1) result &= rw->WriteInt("f", m_f);
			if (m_g > -1) result &= rw->WriteInt("g", m_g);
			if (m_h > -1) result &= rw->WriteInt("h", m_h);
			if (m_i > -1) result &= rw->WriteInt("i", m_i);
			if (m_j > -1) result &= rw->WriteInt("j", m_j);
			if (m_k > -1) result &= rw->WriteInt("k", m_k);
			if (m_l > -1) result &= rw->WriteInt("l", m_l);
			if (m_m > -1) result &= rw->WriteInt("m", m_m);
			if (m_n > -1) result &= rw->WriteInt("n", m_n);
			if (m_o > -1) result &= rw->WriteInt("o", m_o);
			if (m_p > -1) result &= rw->WriteInt("p", m_p);
			if (m_q > -1) result &= rw->WriteInt("q", m_q);
			if (m_r > -1) result &= rw->WriteInt("r", m_r);
			if (m_s > -1) result &= rw->WriteInt("s", m_s);
			if (m_t > -1) result &= rw->WriteInt("t", m_t);
			if (m_u > -1) result &= rw->WriteInt("u", m_u);
			if (m_v > -1) result &= rw->WriteInt("v", m_v);
			if (m_w > -1) result &= rw->WriteInt("w", m_w);
			if (m_x > -1) result &= rw->WriteInt("x", m_x);
			if (m_y > -1) result &= rw->WriteInt("y", m_y);
			if (m_z > -1) result &= rw->WriteInt("z", m_z);
			if (m_ab > -1) result &= rw->WriteInt("ab", m_ab);
			if (m_bb > -1) result &= rw->WriteInt("bb", m_bb);
			if (m_cb > -1) result &= rw->WriteInt("cb", m_cb);
			if (m_db > -1) result &= rw->WriteInt("db", m_db);
			if (m_eb > -1) result &= rw->WriteInt("eb", m_eb);
			if (m_fb > -1) result &= rw->WriteInt("fb", m_fb);
			if (m_gb > -1) result &= rw->WriteInt("gb", m_gb);
			if (m_hb > -1) result &= rw->WriteInt("hb", m_hb);
			if (m_ib > -1) result &= rw->WriteInt("ib", m_ib);
			if (m_jb > -1) result &= rw->WriteInt("jb", m_jb);
			if (m_kb > -1) result &= rw->WriteInt("kb", m_kb);
			if (m_lb > -1) result &= rw->WriteInt("lb", m_lb);
			if (m_mb > -1) result &= rw->WriteInt("mb", m_mb);
			if (m_nb > -1) result &= rw->WriteInt("nb", m_nb);
			if (m_ob > -1) result &= rw->WriteInt("ob", m_ob);
			if (m_pb > -1) result &= rw->WriteInt("pb", m_pb);
			if (m_qb > -1) result &= rw->WriteInt("qb", m_qb);
			if (m_rb > -1) result &= rw->WriteInt("rb", m_rb);
			if (m_sb > -1) result &= rw->WriteInt("sb", m_sb);
			if (m_tb > -1) result &= rw->WriteBool("tb", (m_tb == 1));
			if (m_ub != 0) result &= rw->WriteInt("ub", m_ub);
			if (m_vb != 0) result &= rw->WriteInt("vb", m_vb);

			return result;
		}

	public:
		int m_missionTypeId;
		int m_missionId;
		u64 m_matchId;
		u64 m_bossId;
		u64 m_starterBossId; // The bossId of the president who started the mission
		int m_isboss;
		int m_a;
		int m_b;
		int m_c;
		int m_d;
		int m_e;
		int m_f;
		int m_g;
		int m_h;
		int m_i;
		int m_j;
		int m_k;
		int m_l;
		int m_m;
		int m_n;
		int m_o;
		int m_p;
		int m_q;
		int m_r;
		int m_s;
		int m_t;
		int m_u;
		int m_v;
		int m_w;
		int m_x;
		int m_y;
		int m_z;
		int m_ab;
		int m_bb;
		int m_cb;
		int m_db;
		int m_eb;
		int m_fb;
		int m_gb;
		int m_hb;
		int m_ib;
		int m_jb;
		int m_kb;
		int m_lb;
		int m_mb;
		int m_nb;
		int m_ob;
		int m_pb;
		int m_qb;
		int m_rb;
		int m_sb;
		int m_tb;
		int m_ub;
		int m_vb;
	};

	void CommandPlaystatsMissionEnded(sMissionEnded* data)
	{
		scriptAssertf(data, "%s: PLAYSTATS_MISSION_ENDED - data is NULL.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!data)
			return;

		const u64 bid0 = ((u64)data->m_bossId1.Int) << 32;
		const u64 bid1 = (0x00000000FFFFFFFF&(u64)data->m_bossId2.Int);
		const u64 localbossId = StatsInterface::GetUInt64Stat(StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID"));

		const u64 sbid0 = ((u64)data->m_starterBossId1.Int) << 32;
		const u64 sbid1 = (0x00000000FFFFFFFF&(u64)data->m_starterBossId2.Int);

		MetricMISSION_ENDED m;
		m.m_missionTypeId = data->m_missionTypeId.Int;
		scriptAssertf(data->m_missionId.Int != 0, "%s: PLAYSTATS_MISSION_ENDED - MissionId is 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		m.m_missionId = data->m_missionId.Int;
		const u64 mid0 = ((u64)data->m_matchId1.Int) << 32;
		const u64 mid1 = ((u64)data->m_matchId2.Int);
		m.m_matchId = mid0|mid1;
		m.m_bossId = bid0|bid1;
		m.m_starterBossId = sbid0|sbid1;
		m.m_isboss = (m.m_bossId != 0 && m.m_bossId == localbossId);
		m.m_a = data->m_PlayerParticipated.Int;
		m.m_b = data->m_TimeStart.Int;
		m.m_c = data->m_TimeEnd.Int;
		m.m_d = data->m_Won.Int;
		m.m_e = data->m_EndingReason.Int;
		m.m_f = data->m_CashEarned.Int;
		m.m_g = data->m_RpEarned.Int;
		m.m_h = data->m_TargetsKilled.Int;
		m.m_i = data->m_InnocentsKilled.Int;
		m.m_j = data->m_Deaths.Int;
		m.m_k = data->m_LauncherRank.Int;
		m.m_l = data->m_MCPointsEarnedFromWinning .Int;
		m.m_m = data->m_TimeTakenToComplete.Int;
		m.m_n = data->m_TimeTakenForObjective.Int;
		m.m_o = data->m_PropertyDamageValue.Int;
		m.m_p = data->m_LongestWheelieTime.Int;
		m.m_q = data->m_NumberOfCarsKicked.Int;
		m.m_r = data->m_PlayersLeftInProgress.Int;
		m.m_s = data->m_Location.Int;
		m.m_t = data->m_InvitesSent.Int;
		m.m_u = data->m_InvitesAccepted.Int;
		m.m_v = data->m_BossKilled.Int;
		m.m_w = data->m_GoonsKilled.Int;
		m.m_x = data->m_PackagesGathered.Int;
		m.m_y = data->m_FriendlyAiDeaths.Int;
		m.m_z = data->m_RaceToLocationLaunched.Int;
		m.m_ab = data->m_CratesDestroyed.Int;
		m.m_bb = data->m_AIKilled.Int;
		m.m_cb = data->m_AlliesSpawned.Int;
		m.m_db = data->m_AlliesDroppedOff.Int;
		m.m_eb = data->m_VansSpawned.Int;
		m.m_fb = data->m_VansDestroyed.Int;
		m.m_gb = data->m_TypeOfBusinesS.Int;
		m.m_hb = data->m_OtherBusinessOwned.Int;
		m.m_ib = data->m_StartVolume.Int;
		m.m_jb = data->m_EndVolume.Int;
		m.m_kb = data->m_StartSupplyCapacity.Int;
		m.m_lb = data->m_EndSupplyCapacity.Int;
		m.m_mb = data->m_SuppliesDestroyed.Int;
		m.m_nb = data->m_SuppliesDelivered.Int;
		m.m_ob = data->m_IsMissionAfterFailedDefend.Int;
		m.m_pb = data->m_CashSpentOnResupplies.Int;
		m.m_qb = data->m_ProductDestroyed.Int;
		m.m_rb = data->m_ProductDelivered.Int;
		m.m_sb = data->m_UpgradesOwned.Int;
		m.m_tb = data->m_FromHackerTruck.Int;
		m.m_ub = data->m_properties.Int;
		m.m_vb = data->m_properties2.Int;


		APPEND_METRIC(m);
	}




	class MetricIMPEXP_MISSION_ENDED: public MetricPlayStat
	{
		RL_DECLARE_METRIC(MISSION_IMPEXP_ENDED, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		MetricIMPEXP_MISSION_ENDED() 
			: MetricPlayStat()
			, m_a(-1)
			, m_b(-1)
			, m_c(-1)
			, m_d(-1)
			, m_e(-1)
			, m_f(-1)
			, m_g(-1)
			, m_h(-1)
			, m_i(-1)
			, m_j(-1)
			, m_k(-1)
			, m_l(-1)
			, m_m(-1)
			, m_n(-1)
			, m_o(-1)
			, m_p(-1)
			, m_q(-1)
			, m_r(-1)
			, m_s(-1)
			, m_t(-1)
			, m_u(-1)
			, m_v(-1)
			, m_w(-1)
			, m_x(-1)
			, m_y(-1)
			, m_z(-1)
			, m_aa(-1)
			, m_ab(-1)
			, m_ac(-1)
			, m_ad(-1)
			, m_ae(-1)
			, m_af(-1)
			, m_ag(-1)
			, m_ah(-1)
			, m_ai(-1)
			, m_aj(-1)
			, m_ak(-1)
			, m_al(-1)
			, m_am(-1)
			, m_an(-1)
			, m_ao(-1)
			, m_ap(-1)
			, m_aq(-1)
			, m_ar(-1)
			, m_as(-1)
			, m_at(-1)
			, m_au(-1)
			, m_av(-1)
			, m_aw(-1)
			, m_ax(-1)
			, m_ay(-1)
			, m_az(-1)
			, m_ba(-1)
			, m_bb(-1)
			, m_bc(-1)
		{	
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);

			result &= rw->WriteInt("mtid", m_missionTypeId);
			result &= rw->WriteInt("mid", m_missionId);
			result &= rw->WriteInt64("maid", m_matchId);
			result &= rw->WriteInt64("buuid", m_bossId);
			result &= rw->WriteInt64("sbuuid", m_starterBossId);
			result &= rw->WriteInt("isboss", m_isboss);

			if(m_a > -1)  result &= rw->WriteInt("a", m_a);
			if(m_b > -1)  result &= rw->WriteInt("b", m_b);
			if(m_c > -1)  result &= rw->WriteInt("c", m_c);
			if(m_d > -1)  result &= rw->WriteInt("d", m_d);
			if(m_e > -1)  result &= rw->WriteInt("e", m_e);
			if(m_f > -1)  result &= rw->WriteInt("f", m_f);
			if(m_g > -1)  result &= rw->WriteInt("g", m_g);
			if(m_h > -1)  result &= rw->WriteInt("h", m_h);
			if(m_i > -1)  result &= rw->WriteInt("i", m_i);
			if(m_j > -1)  result &= rw->WriteInt("j", m_j);
			if(m_k > -1)  result &= rw->WriteInt("k", m_k);
			if(m_l > -1)  result &= rw->WriteInt("l", m_l);
			if(m_m > -1)  result &= rw->WriteInt("m", m_m);
			if(m_n > -1)  result &= rw->WriteInt("n", m_n);
			if(m_o > -1)  result &= rw->WriteInt("o", m_o);
			if(m_p > -1)  result &= rw->WriteInt("p", m_p);
			if(m_q > -1)  result &= rw->WriteInt("q", m_q);
			if(m_r > -1)  result &= rw->WriteInt("r", m_r);
			if(m_s > -1)  result &= rw->WriteInt("s", m_s);
			result &= rw->WriteInt("t", m_t); // allow negative values, this is a vehicle hash
			if(m_u > -1)  result &= rw->WriteInt("u", m_u);
			if(m_v > -1)  result &= rw->WriteInt("v", m_v);
			if(m_w > -1)  result &= rw->WriteInt("w", m_w);
			if(m_x > -1)  result &= rw->WriteInt("x", m_x);
			result &= rw->WriteInt("y", m_y); // allow negative values, this is a vehicle hash
			if(m_z > -1)  result &= rw->WriteInt("z", m_z);
			if(m_aa > -1)  result &= rw->WriteInt("aa", m_aa);
			if(m_ab > -1)  result &= rw->WriteInt("ab", m_ab);
			if(m_ac > -1)  result &= rw->WriteInt("ac", m_ac);
			result &= rw->WriteInt("ad", m_ad); // allow negative values, this is a vehicle hash
			if(m_ae > -1)  result &= rw->WriteInt("ae", m_ae);
			if(m_af > -1)  result &= rw->WriteInt("af", m_af);
			if(m_ag > -1)  result &= rw->WriteInt("ag", m_ag);
			if(m_ah > -1)  result &= rw->WriteInt("ah", m_ah);
			result &= rw->WriteInt("ai", m_ai); // allow negative values, this is a vehicle hash
			if(m_aj > -1)  result &= rw->WriteInt("aj", m_aj);
			if(m_ak > -1)  result &= rw->WriteInt("ak", m_ak);
			if(m_al > -1)  result &= rw->WriteInt("al", m_al);
			if(m_am > -1)  result &= rw->WriteInt("am", m_am);
			if(m_an > -1)  result &= rw->WriteInt("an", m_an);
			if(m_ao > -1)  result &= rw->WriteInt("ao", m_ao);
			if(m_ap > -1)  result &= rw->WriteInt("ap", m_ap);
			if(m_aq > -1)  result &= rw->WriteInt("aq", m_aq);
			if(m_ar > -1)  result &= rw->WriteInt("ar", m_ar);
			if(m_as > -1)  result &= rw->WriteInt("as", m_as);
			if(m_at > -1)  result &= rw->WriteInt("at", m_at);
			if(m_au > -1)  result &= rw->WriteInt("au", m_au);
			if(m_av > -1)  result &= rw->WriteInt("av", m_av);
			if(m_aw > -1)  result &= rw->WriteInt("aw", m_aw);
			if(m_ax > -1)  result &= rw->WriteInt("ax", m_ax);
			if(m_ay > -1)  result &= rw->WriteInt("ay", m_ay);
			if(m_az > -1)  result &= rw->WriteInt("az", m_az);
			if (m_ba > -1)  result &= rw->WriteInt("ba", m_ba);
			if (m_bb > -1)  result &= rw->WriteInt("bb", m_bb);
			if (m_bc > -1)  result &= rw->WriteBool("bc", (m_bc == 1));

			return result;
		}

	public:
		int m_missionTypeId;
		int m_missionId;
		u64 m_matchId;
		u64 m_bossId;
		u64 m_starterBossId; // The bossId of the president who started the mission
		int m_isboss;
		int m_a;
		int m_b;
		int m_c;
		int m_d;
		int m_e;
		int m_f;
		int m_g;
		int m_h;
		int m_i;
		int m_j;
		int m_k;
		int m_l;
		int m_m;
		int m_n;
		int m_o;
		int m_p;
		int m_q;
		int m_r;
		int m_s;
		int m_t;
		int m_u;
		int m_v;
		int m_w;
		int m_x;
		int m_y;
		int m_z;
		int m_aa;
		int m_ab;
		int m_ac;
		int m_ad;
		int m_ae;
		int m_af;
		int m_ag;
		int m_ah;
		int m_ai;
		int m_aj;
		int m_ak;
		int m_al;
		int m_am;
		int m_an;
		int m_ao;
		int m_ap;
		int m_aq;
		int m_ar;
		int m_as;
		int m_at;
		int m_au;
		int m_av;
		int m_aw;
		int m_ax;
		int m_ay;
		int m_az;
		int m_ba;		
		int m_bb;
		int m_bc;
	};

	void CommandPlaystatsImpExpMissionEnded(sImpExpMissionEnded* data)
	{
		scriptAssertf(data, "%s: PLAYSTATS_MISSION_ENDED - data is NULL.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!data)
			return;

		const u64 bid0 = ((u64)data->m_bossId1.Int) << 32;
		const u64 bid1 = (0x00000000FFFFFFFF&(u64)data->m_bossId2.Int);
		const u64 localbossId = StatsInterface::GetUInt64Stat(StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID"));

		const u64 sbid0 = ((u64)data->m_starterBossId1.Int) << 32;
		const u64 sbid1 = (0x00000000FFFFFFFF&(u64)data->m_starterBossId2.Int);

		MetricIMPEXP_MISSION_ENDED m;
		m.m_missionTypeId = data->m_missionTypeId.Int;
		scriptAssertf(data->m_missionId.Int != 0, "%s: PLAYSTATS_MISSION_ENDED - MissionId is 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		m.m_missionId = data->m_missionId.Int;
		const u64 mid0 = ((u64)data->m_matchId1.Int) << 32;
		const u64 mid1 = ((u64)data->m_matchId2.Int);
		m.m_matchId = mid0|mid1;
		m.m_bossId = bid0|bid1;
		m.m_starterBossId = sbid0|sbid1;
		m.m_isboss = (m.m_bossId != 0 && m.m_bossId == localbossId);

		m.m_a = data->m_PlayerParticipated.Int;
		m.m_b = data->m_TimeStart.Int;
		m.m_c = data->m_TimeEnd.Int;
		m.m_d = data->m_Won.Int;
		m.m_e = data->m_EndingReason.Int;
		m.m_f = data->m_CashEarned.Int;
		m.m_g = data->m_RpEarned.Int;
		m.m_h = data->m_TargetsKilled.Int;
		m.m_i = data->m_InnocentsKilled.Int;
		m.m_j = data->m_Deaths.Int;
		m.m_k = data->m_LauncherRank.Int;
		m.m_l = data->m_TimeTakenToComplete.Int;
		m.m_m = data->m_PlayersLeftInProgress.Int;
		m.m_n = data->m_Location.Int;
		m.m_o = data->m_InvitesSent.Int;
		m.m_p = data->m_InvitesAccepted.Int;
		m.m_q = data->m_BossKilled.Int;
		m.m_r = data->m_GoonsKilled.Int;
		m.m_s = data->m_CrateAreasDestroyed.Int;
		m.m_t = data->m_Vehicle.Int;
		m.m_u = data->m_VehicleStyle.Int;
		m.m_v = data->m_VehicleHealth.Int;
		m.m_w = data->m_VehiclePoached.Int;
		m.m_x = data->m_VehiclePoachedMethod.Int;
		m.m_y = data->m_Vehicle2.Int;
		m.m_z = data->m_Vehicle2Style.Int;
		m.m_aa = data->m_Vehicle2Health.Int;
		m.m_ab = data->m_Vehicle2Poached.Int;
		m.m_ac = data->m_Vehicle2PoachedMethod.Int;
		m.m_ad = data->m_Vehicle3.Int;
		m.m_ae = data->m_Vehicle3Style.Int;
		m.m_af = data->m_Vehicle3Health.Int;
		m.m_ag = data->m_Vehicle3Poached.Int;
		m.m_ah = data->m_Vehicle3PoachedMethod.Int;
		m.m_ai = data->m_Vehicle4.Int;
		m.m_aj = data->m_Vehicle4Style.Int;
		m.m_ak = data->m_Vehicle4Health.Int;
		m.m_al = data->m_Vehicle4Poached.Int;
		m.m_am = data->m_Vehicle4PoachedMethod.Int;
		m.m_an = data->m_BuyerChosen.Int;
		m.m_ao = data->m_WasMissionCollection.Int;
		m.m_ap = data->m_Collection1.Int;
		m.m_aq = data->m_Collection2.Int;
		m.m_ar = data->m_Collection3.Int;
		m.m_as = data->m_Collection4.Int;
		m.m_at = data->m_Collection5.Int;
		m.m_au = data->m_Collection6.Int;
		m.m_av = data->m_Collection7.Int;
		m.m_aw = data->m_Collection8.Int;
		m.m_ax = data->m_Collection9.Int;
		m.m_ay = data->m_Collection10.Int;
		m.m_az = data->m_StartVolume.Int;
		m.m_ba = data->m_EndVolume.Int;
		m.m_bb = data->m_MissionType.Int;
		m.m_bc = data->m_FromHackerTruck.Int;

		APPEND_METRIC(m);
	}


	//////////////////////////////////////////////////////////////////////////
	// BIKERS TELEMETRY


	// Helper function, we need to pack these in every command below
	bool PackBossIdAndMatchId(int bossId1, int bossId2, int matchId1, int matchId2, u64& bossId, u64& matchId ASSERT_ONLY(, const char* commandName) )
	{
		const u64 bid0 = ((u64)bossId1) << 32;
		const u64 bid1 = (0x00000000FFFFFFFF&(u64)bossId2);

		bossId = bid0|bid1;

		scriptAssertf(bossId != 0, "%s: %s - Failed - Invalid BossId %" I64FMT "d.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName, bossId);
		if(!bossId)
			return false;

		const u64 mid0 = ((u64)matchId1) << 32;
		const u64 mid1 = ((u64)matchId2);

		matchId = mid0|mid1;
		scriptAssertf(matchId != 0, "%s: %s - MatchId should be different from 0", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
		if(!matchId)
			return false;

		return true;
	}

	class MetricChangeMcRole : public MetricPlayStat
	{
		RL_DECLARE_METRIC(MC_ROLE, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

	public:

		MetricChangeMcRole(u64 bossId, u64 matchId, int previousRole, int newRole, int mcPoint)
			: m_bossId(bossId)
			, m_localbossId(StatsInterface::GetUInt64Stat(StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID")))	
			, m_matchId(matchId)
			, m_previousRole(previousRole)
			, m_newRole(newRole)
			, m_mcPoints(mcPoint)
		{;}

		virtual bool Write(RsonWriter* rw) const
		{
			bool isBoss = m_bossId != 0 && m_bossId == m_localbossId;

			return (m_bossId != 0)
				&& MetricPlayStat::Write(rw)
				&& rw->WriteInt64("bid", m_bossId)
				&& rw->WriteInt64("mid", m_matchId)
				&& rw->WriteBool("isboss", isBoss)
				&& rw->WriteInt("a", m_previousRole)
				&& rw->WriteInt("b", m_newRole)
				&& rw->WriteInt("c", m_mcPoints);
		}

	private:

		u64 m_bossId;					//Unique BossID
		u64 m_localbossId;				//Local Unique BossID
		u64 m_matchId;					//Unique Match Id
		int m_previousRole;
		int m_newRole;
		int m_mcPoints;					//Current MC Points
	};

	void CommandPlaystatsChangeMcRole(int bossId1, int bossId2, int matchId1, int matchId2, int previousRole, int newRole, int mcPoint)
	{
		static const u32 ANTI_SPAM_FREQUENCY = 30000; // we dont sent this metric more than once every 30 seconds
		static u32 s_lastTimeMetricSent = 0;
		
		if(fwTimer::GetSystemTimeInMilliseconds() - s_lastTimeMetricSent < ANTI_SPAM_FREQUENCY)
		{
			scriptWarningf("%s: PLAYSTATS_CHANGE_MC_ROLE - Ignoring metric, already sent less than %d seconds ago.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), s_lastTimeMetricSent / 1000);
			return;
		}
		s_lastTimeMetricSent = fwTimer::GetSystemTimeInMilliseconds();

		u64 bossId = 0;
		u64 matchId = 0;

		if(!PackBossIdAndMatchId(bossId1, bossId2, matchId1, matchId2, bossId, matchId  ASSERT_ONLY(, "PLAYSTATS_CHANGE_MC_ROLE") ))
			return;

		scriptDebugf1("%s: PLAYSTATS_CHANGE_MC_ROLE - Sending MetricChangeMcRole metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		MetricChangeMcRole m(bossId, matchId, previousRole, newRole, mcPoint);

		APPEND_METRIC(m);
	}


	class MetricChangeMcOutfit : public MetricPlayStat
	{
		RL_DECLARE_METRIC(MC_OUTFIT, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

	public:

		MetricChangeMcOutfit(u64 bossId, u64 matchId, int outfit)
			: m_bossId(bossId)
			, m_localbossId(StatsInterface::GetUInt64Stat(StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID")))	
			, m_matchId(matchId)
			, m_outfit(outfit)
		{;}

		virtual bool Write(RsonWriter* rw) const
		{
			bool isBoss = m_bossId != 0 && m_bossId == m_localbossId;

			return (m_bossId != 0)
				&& MetricPlayStat::Write(rw)
				&& rw->WriteInt64("bid", m_bossId)
				&& rw->WriteInt64("mid", m_matchId)
				&& rw->WriteBool("isboss", isBoss)
				&& rw->WriteInt("a", m_outfit);
		}

	private:

		u64 m_bossId;					//Unique BossID
		u64 m_localbossId;				//Local Unique BossID
		u64 m_matchId;					//Unique Match Id
		int m_outfit;
	};

	void CommandPlaystatsChangeMcOutfit(int bossId1, int bossId2, int matchId1, int matchId2, int outfit)
	{
		static const u32 ANTI_SPAM_FREQUENCY = 30000; // we dont sent this metric more than once every 30 seconds
		static u32 s_lastTimeMetricSent = 0;

		if(fwTimer::GetSystemTimeInMilliseconds() - s_lastTimeMetricSent < ANTI_SPAM_FREQUENCY)
		{
			scriptWarningf("%s: PLAYSTATS_CHANGE_MC_OUTFIT - Ignoring metric, already sent less than %d seconds ago.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), s_lastTimeMetricSent / 1000);
			return;
		}
		s_lastTimeMetricSent = fwTimer::GetSystemTimeInMilliseconds();

		u64 bossId = 0;
		u64 matchId = 0;

		if(!PackBossIdAndMatchId(bossId1, bossId2, matchId1, matchId2, bossId, matchId ASSERT_ONLY( , "PLAYSTATS_CHANGE_MC_OUTFIT") ))
			return;

		scriptDebugf1("%s: PLAYSTATS_CHANGE_MC_OUTFIT - Sending MetricChangeMcOutfit metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		MetricChangeMcOutfit m(bossId, matchId, outfit);

		APPEND_METRIC(m);
	}


	class MetricChangeMcEmblem : public MetricPlayStat
	{
		RL_DECLARE_METRIC(MC_EMBLEM, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

	public:

		MetricChangeMcEmblem(u64 bossId, u64 matchId, bool emblem)
			: m_bossId(bossId)
			, m_localbossId(StatsInterface::GetUInt64Stat(StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID")))	
			, m_matchId(matchId)
			, m_emblem(emblem)
		{;}

		virtual bool Write(RsonWriter* rw) const
		{
			bool isBoss = m_bossId != 0 && m_bossId == m_localbossId;

			return (m_bossId != 0)
				&& MetricPlayStat::Write(rw)
				&& rw->WriteInt64("bid", m_bossId)
				&& rw->WriteInt64("mid", m_matchId)
				&& rw->WriteBool("isboss", isBoss)
				&& rw->WriteBool("a", m_emblem);
		}

	private:

		u64 m_bossId;					//Unique BossID
		u64 m_localbossId;				//Local Unique BossID
		u64 m_matchId;					//Unique Match Id
		bool m_emblem;
	};

	void CommandPlaystatsChangeMcEmblem(int bossId1, int bossId2, int matchId1, int matchId2, int emblem)
	{
		static const u32 ANTI_SPAM_FREQUENCY = 30000; // we dont sent this metric more than once every 30 seconds
		static u32 s_lastTimeMetricSent = 0;

		if(fwTimer::GetSystemTimeInMilliseconds() - s_lastTimeMetricSent < ANTI_SPAM_FREQUENCY)
		{
			scriptWarningf("%s: PLAYSTATS_SWITCH_MC_EMBLEM - Ignoring metric, already sent less than %d seconds ago.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), s_lastTimeMetricSent / 1000);
			return;
		}
		s_lastTimeMetricSent = fwTimer::GetSystemTimeInMilliseconds();

		u64 bossId = 0;
		u64 matchId = 0;

		if(!PackBossIdAndMatchId(bossId1, bossId2, matchId1, matchId2, bossId, matchId  ASSERT_ONLY(, "PLAYSTATS_SWITCH_MC_EMBLEM") ))
			return;

		scriptDebugf1("%s: PLAYSTATS_SWITCH_MC_EMBLEM - Sending MetricChangeMcOutfit metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		MetricChangeMcEmblem m(bossId, matchId, (emblem != 0) );

		APPEND_METRIC(m);
	}

	class MetricMcRequestBike : public MetricPlayStat
	{
		RL_DECLARE_METRIC(MC_REQ_BIKE, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

	public:

		MetricMcRequestBike(u64 bossId, u64 matchId, int vehicleHash)
			: m_bossId(bossId)
			, m_localbossId(StatsInterface::GetUInt64Stat(StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID")))	
			, m_matchId(matchId)
			, m_vehicleHash(vehicleHash)
		{;}

		virtual bool Write(RsonWriter* rw) const
		{
			bool isBoss = m_bossId != 0 && m_bossId == m_localbossId;

			return (m_bossId != 0)
				&& MetricPlayStat::Write(rw)
				&& rw->WriteInt64("bid", m_bossId)
				&& rw->WriteInt64("mid", m_matchId)
				&& rw->WriteBool("isboss", isBoss)
				&& rw->WriteInt("a", m_vehicleHash);
		}

	private:

		u64 m_bossId;					//Unique BossID
		u64 m_localbossId;				//Local Unique BossID
		u64 m_matchId;					//Unique Match Id
		int m_vehicleHash;
	};

	void CommandPlaystatsMcRequestBike(int bossId1, int bossId2, int matchId1, int matchId2, int vehicleHash)
	{
		static const u32 ANTI_SPAM_FREQUENCY = 30000; // we dont sent this metric more than once every 30 seconds
		static u32 s_lastTimeMetricSent = 0;

		if(fwTimer::GetSystemTimeInMilliseconds() - s_lastTimeMetricSent < ANTI_SPAM_FREQUENCY)
		{
			scriptWarningf("%s: PLAYSTATS_MC_REQUEST_BIKE - Ignoring metric, already sent less than %d seconds ago.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), s_lastTimeMetricSent / 1000);
			return;
		}
		s_lastTimeMetricSent = fwTimer::GetSystemTimeInMilliseconds();

		u64 bossId = 0;
		u64 matchId = 0;

		if(!PackBossIdAndMatchId(bossId1, bossId2, matchId1, matchId2, bossId, matchId ASSERT_ONLY( , "PLAYSTATS_MC_REQUEST_BIKE") ))
			return;

		scriptDebugf1("%s: PLAYSTATS_MC_REQUEST_BIKE - Sending MetricMcRequestBike metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		MetricMcRequestBike m(bossId, matchId, vehicleHash);

		APPEND_METRIC(m);
	}

	class MetricKilledRivalMcMember: public MetricPlayStat
	{
		RL_DECLARE_METRIC(KILLED_RIVAL_MC, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

	public:

		MetricKilledRivalMcMember(u64 bossId, u64 matchId, int rpEarned)
			: m_bossId(bossId)
			, m_localbossId(StatsInterface::GetUInt64Stat(StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID")))	
			, m_matchId(matchId)
			, m_rpEarned(rpEarned)
		{;}

		virtual bool Write(RsonWriter* rw) const
		{
			bool isBoss = m_bossId != 0 && m_bossId == m_localbossId;

			return (m_bossId != 0)
				&& MetricPlayStat::Write(rw)
				&& rw->WriteInt64("bid", m_bossId)
				&& rw->WriteInt64("mid", m_matchId)
				&& rw->WriteBool("isboss", isBoss)
				&& rw->WriteInt("a", m_rpEarned);
		}

	private:

		u64 m_bossId;					//Unique BossID
		u64 m_localbossId;				//Local Unique BossID
		u64 m_matchId;					//Unique Match Id
		int m_rpEarned;
	};

	void CommandPlaystatsKilledRivalMcMember(int bossId1, int bossId2, int matchId1, int matchId2, int rpEarned)
	{
		static const u32 ANTI_SPAM_FREQUENCY = 30000; // we dont sent this metric more than once every 30 seconds
		static u32 s_lastTimeMetricSent = 0;

		if(fwTimer::GetSystemTimeInMilliseconds() - s_lastTimeMetricSent < ANTI_SPAM_FREQUENCY)
		{
			scriptWarningf("%s: PLAYSTATS_MC_KILLED_RIVAL_MC_MEMBER - Ignoring metric, already sent less than %d seconds ago.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), s_lastTimeMetricSent / 1000);
			return;
		}
		s_lastTimeMetricSent = fwTimer::GetSystemTimeInMilliseconds();

		u64 bossId = 0;
		u64 matchId = 0;

		if(!PackBossIdAndMatchId(bossId1, bossId2, matchId1, matchId2, bossId, matchId ASSERT_ONLY( , "PLAYSTATS_MC_KILLED_RIVAL_MC_MEMBER") ))
			return;

		scriptDebugf1("%s: PLAYSTATS_MC_KILLED_RIVAL_MC_MEMBER - Sending MetricKilledRivalMcMember metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		MetricKilledRivalMcMember m(bossId, matchId, rpEarned);

		APPEND_METRIC(m);
	}

	class MetricAbandoningMc : public MetricPlayStat
	{
		RL_DECLARE_METRIC(ABANDON_MC, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

	public:

		MetricAbandoningMc(u64 bossId, u64 matchId, int abandonedMc)
			: m_bossId(bossId)
			, m_localbossId(StatsInterface::GetUInt64Stat(StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID")))	
			, m_matchId(matchId)
			, m_abandonedMc(abandonedMc)
		{;}

		virtual bool Write(RsonWriter* rw) const
		{
			bool isBoss = m_bossId != 0 && m_bossId == m_localbossId;

			return (m_bossId != 0)
				&& MetricPlayStat::Write(rw)
				&& rw->WriteInt64("bid", m_bossId)
				&& rw->WriteInt64("mid", m_matchId)
				&& rw->WriteBool("isboss", isBoss)
				&& rw->WriteInt("a", m_abandonedMc);
		}

	private:

		u64 m_bossId;					//Unique BossID
		u64 m_localbossId;				//Local Unique BossID
		u64 m_matchId;					//Unique Match Id
		int m_abandonedMc;
	};

	void CommandPlaystatsAbandonedMc(int bossId1, int bossId2, int matchId1, int matchId2, int abandonedMc)
	{
		static const u32 ANTI_SPAM_FREQUENCY = 30000; // we dont sent this metric more than once every 30 seconds
		static u32 s_lastTimeMetricSent = 0;

		if(fwTimer::GetSystemTimeInMilliseconds() - s_lastTimeMetricSent < ANTI_SPAM_FREQUENCY)
		{
			scriptWarningf("%s: PLAYSTATS_ABANDONED_MC - Ignoring metric, already sent less than %d seconds ago.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), s_lastTimeMetricSent / 1000);
			return;
		}
		s_lastTimeMetricSent = fwTimer::GetSystemTimeInMilliseconds();

		u64 bossId = 0;
		u64 matchId = 0;

		if(!PackBossIdAndMatchId(bossId1, bossId2, matchId1, matchId2, bossId, matchId  ASSERT_ONLY( , "PLAYSTATS_ABANDONED_MC") ))
			return;

		scriptDebugf1("%s: PLAYSTATS_ABANDONED_MC - Sending MetricAbandoningMc metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		MetricAbandoningMc m(bossId, matchId, abandonedMc);

		APPEND_METRIC(m);
	}

	class MetricEarnedMcPoints : public MetricPlayStat
	{
		RL_DECLARE_METRIC(EARNED_MC_POINTS, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

	public:

		MetricEarnedMcPoints(u64 bossId, u64 matchId, int method, int amount)
			: m_bossId(bossId)
			, m_localbossId(StatsInterface::GetUInt64Stat(StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID")))	
			, m_matchId(matchId)
			, m_method(method)
			, m_amount(amount)
		{;}

		virtual bool Write(RsonWriter* rw) const
		{
			bool isBoss = m_bossId != 0 && m_bossId == m_localbossId;

			return (m_bossId != 0)
				&& MetricPlayStat::Write(rw)
				&& rw->WriteInt64("bid", m_bossId)
				&& rw->WriteInt64("mid", m_matchId)
				&& rw->WriteBool("isboss", isBoss)
				&& rw->WriteInt("a", m_method)
				&& rw->WriteInt("b", m_amount);
		}

	private:

		u64 m_bossId;					//Unique BossID
		u64 m_localbossId;				//Local Unique BossID
		u64 m_matchId;					//Unique Match Id
		int m_method;
		int m_amount;
	};

	void CommandPlaystatsEarnedMcPoints(int bossId1, int bossId2, int matchId1, int matchId2, int method, int amount)
	{
		static const u32 ANTI_SPAM_FREQUENCY = 30000; // we dont sent this metric more than once every 30 seconds
		static u32 s_lastTimeMetricSent = 0;

		if(fwTimer::GetSystemTimeInMilliseconds() - s_lastTimeMetricSent < ANTI_SPAM_FREQUENCY)
		{
			scriptWarningf("%s: PLAYSTATS_EARNED_MC_POINTS - Ignoring metric, already sent less than %d seconds ago.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), s_lastTimeMetricSent / 1000);
			return;
		}
		s_lastTimeMetricSent = fwTimer::GetSystemTimeInMilliseconds();

		u64 bossId = 0;
		u64 matchId = 0;

		if(!PackBossIdAndMatchId(bossId1, bossId2, matchId1, matchId2, bossId, matchId  ASSERT_ONLY( , "PLAYSTATS_EARNED_MC_POINTS") ))
			return;

		scriptDebugf1("%s: PLAYSTATS_EARNED_MC_POINTS - Sending MetricEarnedMcPoints metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		MetricEarnedMcPoints m(bossId, matchId, method, amount);

		APPEND_METRIC(m);
	}

	class MetricMcFormationEnds : public MetricPlayStat
	{
		RL_DECLARE_METRIC(MC_FORMATION, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

	public:

		MetricMcFormationEnds(u64 bossId, u64 matchId, int formationStyle, int duration, int membersAmount)
			: m_bossId(bossId)
			, m_localbossId(StatsInterface::GetUInt64Stat(StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID")))	
			, m_matchId(matchId)
			, m_formationStyle(formationStyle)
			, m_duration(duration)
			, m_membersAmount(membersAmount)
		{;}

		virtual bool Write(RsonWriter* rw) const
		{
			bool isBoss = m_bossId != 0 && m_bossId == m_localbossId;

			return (m_bossId != 0)
				&& MetricPlayStat::Write(rw)
				&& rw->WriteInt64("bid", m_bossId)
				&& rw->WriteInt64("mid", m_matchId)
				&& rw->WriteBool("isboss", isBoss)
				&& rw->WriteInt("a", m_formationStyle)
				&& rw->WriteInt("b", m_duration)
				&& rw->WriteInt("c", m_membersAmount);
		}

	private:

		u64 m_bossId;					//Unique BossID
		u64 m_localbossId;				//Local Unique BossID
		u64 m_matchId;					//Unique Match Id
		int m_formationStyle;
		int m_duration;
		int m_membersAmount;
	};

	void CommandPlaystatsMcFormationEnds(int bossId1, int bossId2, int matchId1, int matchId2, int formationStyle, int duration, int membersAmount)
	{
		static const u32 ANTI_SPAM_FREQUENCY = 30000; // we dont sent this metric more than once every 30 seconds
		static u32 s_lastTimeMetricSent = 0;

		if(fwTimer::GetSystemTimeInMilliseconds() - s_lastTimeMetricSent < ANTI_SPAM_FREQUENCY)
		{
			scriptWarningf("%s: PLAYSTATS_MC_FORMATION_ENDS - Ignoring metric, already sent less than %d seconds ago.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), s_lastTimeMetricSent / 1000);
			return;
		}
		s_lastTimeMetricSent = fwTimer::GetSystemTimeInMilliseconds();

		u64 bossId = 0;
		u64 matchId = 0;

		if(!PackBossIdAndMatchId(bossId1, bossId2, matchId1, matchId2, bossId, matchId  ASSERT_ONLY(, "PLAYSTATS_MC_FORMATION_ENDS") ))
			return;

		scriptDebugf1("%s: PLAYSTATS_MC_FORMATION_ENDS - Sending MetricEarnedMcPoints metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		MetricMcFormationEnds m(bossId, matchId, formationStyle, duration, membersAmount);

		APPEND_METRIC(m);
	}

	class MetricMcClubhouseActivity : public MetricPlayStat
	{
		RL_DECLARE_METRIC(MC_CH_ACTIVITY, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

	public:

		MetricMcClubhouseActivity(u64 bossId, u64 matchId, int minigameType, int jukeboxStation, int mcPointsEarned)
			: m_bossId(bossId)
			, m_localbossId(StatsInterface::GetUInt64Stat(StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID")))	
			, m_matchId(matchId)
			, m_minigameType(minigameType)
			, m_jukeboxStation(jukeboxStation)
			, m_mcPointsEarned(mcPointsEarned)
		{;}

		virtual bool Write(RsonWriter* rw) const
		{
			bool isBoss = m_bossId != 0 && m_bossId == m_localbossId;

			return (m_bossId != 0)
				&& MetricPlayStat::Write(rw)
				&& rw->WriteInt64("bid", m_bossId)
				&& rw->WriteInt64("mid", m_matchId)
				&& rw->WriteBool("isboss", isBoss)
				&& rw->WriteInt("a", m_minigameType)
				&& rw->WriteInt("b", m_jukeboxStation)
				&& rw->WriteInt("c", m_mcPointsEarned);
		}

	private:

		u64 m_bossId;					//Unique BossID
		u64 m_localbossId;				//Local Unique BossID
		u64 m_matchId;					//Unique Match Id
		int m_minigameType;
		int m_jukeboxStation;
		int m_mcPointsEarned;
	};

	void CommandPlaystatsMcClubhouseActivity(int bossId1, int bossId2, int matchId1, int matchId2, int minigameType, int jukeboxStation, int mcPointsEarned)
	{
		static const u32 ANTI_SPAM_FREQUENCY = 30000; // we dont sent this metric more than once every 30 seconds
		static u32 s_lastTimeMetricSent = 0;

		if(fwTimer::GetSystemTimeInMilliseconds() - s_lastTimeMetricSent < ANTI_SPAM_FREQUENCY)
		{
			scriptWarningf("%s: PLAYSTATS_MC_CLUBHOUSE_ACTIVITY - Ignoring metric, already sent less than %d seconds ago.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), s_lastTimeMetricSent / 1000);
			return;
		}
		s_lastTimeMetricSent = fwTimer::GetSystemTimeInMilliseconds();

		u64 bossId = 0;
		u64 matchId = 0;

		if(!PackBossIdAndMatchId(bossId1, bossId2, matchId1, matchId2, bossId, matchId  ASSERT_ONLY(, "PLAYSTATS_MC_CLUBHOUSE_ACTIVITY") ))
			return;

		scriptDebugf1("%s: PLAYSTATS_MC_CLUBHOUSE_ACTIVITY- Sending MetricMcClubhouseActivity metric.", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		MetricMcClubhouseActivity m(bossId, matchId, minigameType, jukeboxStation, mcPointsEarned);

		APPEND_METRIC(m);
	}

	class MetricCopyRankIntoNewSlot : public MetricPlayStat
	{
		RL_DECLARE_METRIC(COPY_RANK, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

	public:

		MetricCopyRankIntoNewSlot(int slotUsed, int copyOption, int propertyAwarded, bool characterInSlot, int rank, int previousRank, int totalTime)
			: m_slotUsed(slotUsed)
			, m_copyOption(copyOption)
			, m_propertyAwarded(propertyAwarded)
			, m_characterInSlot(characterInSlot)
			, m_rank(rank)
			, m_previousRank(previousRank)
			, m_totalTime(totalTime)
		{;}

		virtual bool Write(RsonWriter* rw) const
		{
			return MetricPlayStat::Write(rw)
				&& rw->WriteInt("a", m_slotUsed)
				&& rw->WriteInt("b", m_copyOption)
				&& rw->WriteInt("c", m_propertyAwarded)
				&& rw->WriteBool("d", m_characterInSlot)
				&& rw->WriteInt("e", m_rank)
				&& rw->WriteInt("f", m_previousRank)
				&& rw->WriteInt("g", m_totalTime);
		}

	private:

		int m_slotUsed;
		int m_copyOption;
		int m_propertyAwarded;
		bool m_characterInSlot;
		int m_rank;
		int m_previousRank;
		int m_totalTime;
	};

	void CommandPlaystatsCopyRankIntoNewSlot(int slotUsed, int copyOption, int propertyAwarded, int characterInSlot, int rank, int previousRank, int totalTime)
	{
		MetricCopyRankIntoNewSlot m(slotUsed, copyOption, propertyAwarded, (characterInSlot != 0), rank, previousRank, totalTime);
		APPEND_METRIC(m);
	}

	// Import Export Metrics



	class MetricRivalBehaviour : public MetricPlayStat
	{
		RL_DECLARE_METRIC(RIVAL_BEHAVIOR, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

	public:

		MetricRivalBehaviour(u64 bossId, u64 matchId, int deliveryDestination, int cashEarned, int rpEarned, int vehicle, int vehicleStyle, bool importMission)
			: m_bossId(bossId)
			, m_localbossId(StatsInterface::GetUInt64Stat(StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID")))	
			, m_matchId(matchId)
			, m_deliveryDestination(deliveryDestination)
			, m_cashEarned(cashEarned)
			, m_rpEarned(rpEarned)
			, m_vehicle(vehicle)
			, m_vehicleStyle(vehicleStyle)
			, m_importMission(importMission)
		{;}

		virtual bool Write(RsonWriter* rw) const
		{
			bool isBoss = m_bossId != 0 && m_bossId == m_localbossId;

			return (m_bossId != 0)
				&& MetricPlayStat::Write(rw)
				&& rw->WriteInt64("bid", m_bossId)
				&& rw->WriteInt64("mid", m_matchId)
				&& rw->WriteBool("isboss", isBoss)
				&& rw->WriteInt("a", m_deliveryDestination)
				&& rw->WriteInt("b", m_cashEarned)
				&& rw->WriteInt("c", m_rpEarned)
				&& rw->WriteInt("d", m_vehicle)
				&& rw->WriteInt("e", m_vehicleStyle)
				&& rw->WriteBool("f", m_importMission);
		}

	private:

		u64 m_bossId;					//Unique BossID
		u64 m_localbossId;				//Local Unique BossID
		u64 m_matchId;					//Unique Match Id
		int m_deliveryDestination;
		int m_cashEarned;
		int m_rpEarned;
		int m_vehicle;
		int m_vehicleStyle;
		bool m_importMission;
	};

	void CommandPlaystatsRivalBehaviour(int bossId1, int bossId2, int matchId1, int matchId2, int deliveryDestination, int cashEarned, int rpEarned, int vehicle, int vehicleStyle, bool importMission)
	{
		u64 bossId = 0;
		u64 matchId = 0;

		if(!PackBossIdAndMatchId(bossId1, bossId2, matchId1, matchId2, bossId, matchId  ASSERT_ONLY(, "PLAYSTATS_RIVAL_BEHAVIOR") ))
			return;

		MetricRivalBehaviour m(bossId, matchId, deliveryDestination, cashEarned, rpEarned, vehicle, vehicleStyle, importMission);

		APPEND_METRIC(m);
	}

	//////////////////////////////////////////////////////////////////////////
	/// Dupe Detection

	class MetricDupeDetection : public MetricPlayStat
	{
		RL_DECLARE_METRIC(DUPE_DETECT, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

	public:

		MetricDupeDetection(DupeDetection* data)
			: m_locationBlocked(data->m_locationBlocked.Int)
			, m_reasonBlocked(data->m_reasonBlocked.Int)
			, m_vehicleModel(data->m_vehicleModel.Int)
			, m_dupesSold(data->m_dupesSold.Int)
			, m_blockedVehicles(data->m_blockedVehicles.Int)
			, m_ownedVehicles(data->m_ownedVehicles.Int)
			, m_garageSpace(data->m_garageSpace.Int)
			, m_exploitLevel(data->m_exploitLevel.Int)
			, m_levelIncrease(data->m_levelIncrease.Int)
			, m_iFruit(data->m_iFruit.Int != 0)
			, m_spareSlot1(data->m_spareSlot1.Int)
			, m_spareSlot2(data->m_spareSlot2.Int)
		{;}

		virtual bool Write(RsonWriter* rw) const
		{
			return MetricPlayStat::Write(rw)
				&& rw->WriteInt("a", m_locationBlocked)
				&& rw->WriteInt("b", m_reasonBlocked)
				&& rw->WriteUns("c", m_vehicleModel)
				&& rw->WriteInt("d", m_dupesSold)
				&& rw->WriteInt("e", m_blockedVehicles)
				&& rw->WriteInt("f", m_ownedVehicles)
				&& rw->WriteInt("g", m_garageSpace)
				&& rw->WriteInt("h", m_exploitLevel)
				&& rw->WriteInt("i", m_levelIncrease)
				&& rw->WriteBool("j", m_iFruit)
				&& rw->WriteInt("k", m_spareSlot1)
				&& rw->WriteInt("l", m_spareSlot2);
		}

	private:

		int  m_locationBlocked;   // Where they were blocked (vehicle entry, mod shop entry, selling option)
		int  m_reasonBlocked;     // Why they were blocked (which of the 3 lists. previous 5 veh sold, last 5 plates to be changed, plates of last 10 ‘known dupes’)
		u32  m_vehicleModel;      // Vehicle model
		int  m_dupesSold;         // If a limited number of allowed dupes had been sold, and if so, how many.
		int  m_blockedVehicles;   // Number of vehicles owned by player which match blocked vehicle’s license plate
		int  m_ownedVehicles;     // Total number of owned vehicles
		int  m_garageSpace;       // Total number of personal garage spaces
		int  m_exploitLevel;      // Exploit Level of player
		int  m_levelIncrease;     // Did we apply an exploit level increase? If so, how much
		bool m_iFruit;            // Does the player have the iFruit app?
		int  m_spareSlot1;        // spare slot
		int  m_spareSlot2;        // spare slot
	};

	void CommandPlaystatsDupeDetected(DupeDetection* data)
	{
		MetricDupeDetection m(data);

		APPEND_METRIC(m);
	}

	//////////////////////////////////////////////////////////////////////////
	/// ScriptHook Ban Alert 

	class MetricBanAlert : public MetricPlayStat
	{
		RL_DECLARE_METRIC(BAN_ALERT, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

	public:

		MetricBanAlert(int optionSelected)
			: m_optionSelected(optionSelected)
		{;}

		virtual bool Write(RsonWriter* rw) const
		{
			return MetricPlayStat::Write(rw)
				&& rw->WriteInt("o", m_optionSelected);
		}

	private:

		int  m_optionSelected;
	};

	void CommandPlaystatsBanAlert(int optionSelected)
	{
		MetricBanAlert m(optionSelected);

		APPEND_METRIC(m);
	}

	//////////////////////////////////////////////////////////////////////////
	/// Gun Running

	class MetricGUNRUN_MISSION_ENDED: public MetricPlayStat
	{
		RL_DECLARE_METRIC(GUNRUN_MISSION_ENDED, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		MetricGUNRUN_MISSION_ENDED() 
			: MetricPlayStat()
			, m_a(-1)
			, m_b(-1)
			, m_c(-1)
			, m_d(-1)
			, m_e(-1)
			, m_f(-1)
			, m_g(-1)
			, m_h(-1)
			, m_i(-1)
			, m_j(-1)
			, m_k(-1)
			, m_l(-1)
			, m_m(-1)
			, m_n(-1)
			, m_o(-1)
			, m_p(-1)
			, m_q(-1)
			, m_r(-1)
			, m_s(-1)
			, m_t(-1)
			, m_u(-1)
			, m_v(-1)
			, m_w(-1)
			, m_x(-1)
			, m_y(-1)
			, m_aa(-1)
			, m_ab(-1)
			, m_ac(-1)
			, m_ad(-1)
			, m_ae(-1)
			, m_af(-1)
			, m_ag(-1)
			, m_ah(-1)
			, m_ai(-1)
			, m_aj(-1)
			, m_ak(-1)
			, m_al(-1)
			, m_am(-1)
		{	
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);

			result &= rw->WriteInt("mtid", m_missionTypeId);
			result &= rw->WriteInt("mid", m_missionId);
			result &= rw->WriteInt64("maid", m_matchId);
			result &= rw->WriteInt64("buuid", m_bossId);
			result &= rw->WriteInt("isboss", m_isboss);

			if(m_a > -1)  result &= rw->WriteInt("a", m_a);
			if(m_b > -1)  result &= rw->WriteInt("b", m_b);
			if(m_c > -1)  result &= rw->WriteInt("c", m_c);
			if(m_d > -1)  result &= rw->WriteInt("d", m_d);
			if(m_e > -1)  result &= rw->WriteInt("e", m_e);
			if(m_f > -1)  result &= rw->WriteInt("f", m_f);
			if(m_g > -1)  result &= rw->WriteInt("g", m_g);
			if(m_h > -1)  result &= rw->WriteInt("h", m_h);
			if(m_i > -1)  result &= rw->WriteInt("i", m_i);
			if(m_j > -1)  result &= rw->WriteInt("j", m_j);
			if(m_k > -1)  result &= rw->WriteInt("k", m_k);
			if(m_l > -1)  result &= rw->WriteInt("l", m_l);
			if(m_m > -1)  result &= rw->WriteInt("m", m_m);
			if(m_n > -1)  result &= rw->WriteInt("n", m_n);
			if(m_o > -1)  result &= rw->WriteInt("o", m_o);
			if(m_p > -1)  result &= rw->WriteInt("p", m_p);
			if(m_q > -1)  result &= rw->WriteInt("q", m_q);
			if(m_r > -1)  result &= rw->WriteInt("r", m_r);
			if(m_s > -1)  result &= rw->WriteInt("s", m_s);
			if(m_t > -1)  result &= rw->WriteInt("t", m_t);
			if(m_u > -1)  result &= rw->WriteInt("u", m_u);
			if(m_v > -1)  result &= rw->WriteInt("v", m_v);
			if(m_w > -1)  result &= rw->WriteInt("w", m_w);
			if(m_x > -1)  result &= rw->WriteInt("x", m_x);
			if(m_y > -1)  result &= rw->WriteInt("y", m_y);
			if(m_aa > -1)  result &= rw->WriteInt("aa", m_aa);
			if(m_ab > -1)  result &= rw->WriteInt("ab", m_ab);
			if(m_ac > -1)  result &= rw->WriteInt("ac", m_ac);
			if(m_ad > -1)  result &= rw->WriteInt("ad", m_ad);
			if(m_ae > -1)  result &= rw->WriteInt("ae", m_ae);
			if(m_af > -1)  result &= rw->WriteInt("af", m_af);
			if(m_ag > -1)  result &= rw->WriteInt("ag", m_ag);
			if(m_ah > -1)  result &= rw->WriteInt("ah", m_ah);
			if(m_ai > -1)  result &= rw->WriteInt("ai", m_ai);
			if(m_aj > -1)  result &= rw->WriteInt("aj", m_aj);
			if(m_ak > -1)  result &= rw->WriteInt("ak", m_ak);
			if (m_al > -1)  result &= rw->WriteInt("al", m_al);
			if (m_am > -1)  result &= rw->WriteBool("am", (m_am == 1));
			if (m_an !=0)  result &= rw->WriteInt("an", m_an);
			if (m_ao != 0)  result &= rw->WriteInt("ao", m_ao);
			result &= rw->WriteInt("ap", m_ap);

			return result;
		}

	public:
		int m_missionTypeId;
		int m_missionId;
		u64 m_matchId;
		u64 m_bossId;
		int m_isboss;
		int m_a;
		int m_b;
		int m_c;
		int m_d;
		int m_e;
		int m_f;
		int m_g;
		int m_h;
		int m_i;
		int m_j;
		int m_k;
		int m_l;
		int m_m;
		int m_n;
		int m_o;
		int m_p;
		int m_q;
		int m_r;
		int m_s;
		int m_t;
		int m_u;
		int m_v;
		int m_w;
		int m_x;
		int m_y;
		int m_aa;
		int m_ab;
		int m_ac;
		int m_ad;
		int m_ae;
		int m_af;
		int m_ag;
		int m_ah;
		int m_ai;
		int m_aj;
		int m_ak;
		int m_al;
		int m_am;
		int m_an;
		int m_ao;
		int m_ap;
	};

	void CommandPlaystatsGunRunningMissionEnded(sGunRunningMissionEnded* data)
	{
		scriptAssertf(data, "%s: PLAYSTATS_GUNRUNNING_MISSION_ENDED - data is NULL.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!data)
			return;

		const u64 bid0 = ((u64)data->m_bossId1.Int) << 32;
		const u64 bid1 = (0x00000000FFFFFFFF&(u64)data->m_bossId2.Int);
		const u64 localbossId = StatsInterface::GetUInt64Stat(StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID"));

		MetricGUNRUN_MISSION_ENDED m;
		m.m_missionTypeId = data->m_missionTypeId.Int;
		scriptAssertf(data->m_missionId.Int != 0, "%s: PLAYSTATS_GUNRUNNING_MISSION_ENDED - MissionId is 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		m.m_missionId = data->m_missionId.Int;
		const u64 mid0 = ((u64)data->m_matchId1.Int) << 32;
		const u64 mid1 = ((u64)data->m_matchId2.Int);
		m.m_matchId = mid0|mid1;
		m.m_bossId = bid0|bid1;
		m.m_isboss = (m.m_bossId != 0 && m.m_bossId == localbossId);

		m.m_a = data->m_PlayerParticipated.Int;
		m.m_b = data->m_PlayerRole.Int;
		m.m_c = data->m_TimeStart.Int;
		m.m_d = data->m_TimeEnd.Int;
		m.m_e = data->m_Won.Int;
		m.m_f = data->m_EndingReason.Int;
		m.m_g = data->m_CashEarned.Int;
		m.m_h = data->m_RpEarned.Int;
		m.m_i = data->m_TargetsKilled.Int;
		m.m_j = data->m_InnocentsKilled.Int;
		m.m_k = data->m_Deaths.Int;
		m.m_l = data->m_LauncherRank.Int;
		m.m_m = data->m_TimeTakenToComplete.Int;
		m.m_n = data->m_PlayersLeftInProgress.Int;
		m.m_o = data->m_Location.Int;
		m.m_p = data->m_InvitesSent.Int;
		m.m_q = data->m_InvitesAccepted.Int;
		m.m_r = data->m_BossKilled.Int;
		m.m_s = data->m_GoonsKilled.Int;
		m.m_t = data->m_SuppliesStolen.Int;
		m.m_u = data->m_SuppliesOwned.Int;
		m.m_v = data->m_ProductsOwned.Int;
		m.m_w = data->m_OwnBunker.Int;
		m.m_x = data->m_OwnFiringRange.Int;
		m.m_y = data->m_OwnGunLocker.Int;
		m.m_aa = data->m_OwnCaddy.Int;
		m.m_ab = data->m_OwnTrailer.Int;
		m.m_ac = data->m_OwnLivingArea.Int;
		m.m_ad = data->m_OwnPersonalQuarter.Int;
		m.m_ae = data->m_OwnCommandCenter.Int;
		m.m_af = data->m_OwnArmory.Int;
		m.m_ag = data->m_OwnVehModArmory.Int;
		m.m_ah = data->m_OwnVehStorage.Int;
		m.m_ai = data->m_OwnEquipmentUpgrade.Int;
		m.m_aj = data->m_OwnSecurityUpgrade.Int;
		m.m_ak = data->m_OwnStaffUpgrade.Int;
		m.m_al = data->m_ResuppliesPassed.Int;
		m.m_am = data->m_FromHackerTruck.Int;
		m.m_an = data->m_properties.Int;
		m.m_ao = data->m_properties2.Int;
		m.m_ap = data->m_missionCategory.Int;

		APPEND_METRIC(m);
	}


	class MetricGUNRUN_RND: public MetricPlayStat
	{
		RL_DECLARE_METRIC(GUNRUN_RND, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		MetricGUNRUN_RND() 
			: MetricPlayStat()
			, m_StartorEnd(-1)
			, m_UpgradeType(-1)
			, m_staffPercentage(-1)
			, m_supplyAmount(-1)
			, m_productionAmount(-1)
		{	
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);

			result &= rw->WriteInt64("buuid", m_bossId);

			if(m_StartorEnd > -1)  result &= rw->WriteInt("a", m_StartorEnd);
			if(m_UpgradeType > -1)  result &= rw->WriteInt("b", m_UpgradeType);
			if(m_staffPercentage > -1)  result &= rw->WriteInt("c", m_staffPercentage);
			if(m_supplyAmount > -1)  result &= rw->WriteInt("d", m_supplyAmount);
			if(m_productionAmount > -1)  result &= rw->WriteInt("e", m_productionAmount);

			return result;
		}

	public:
		u64 m_bossId;
		int m_StartorEnd;
		int m_UpgradeType;
		int m_staffPercentage;
		int m_supplyAmount;
		int m_productionAmount;
	};

	void CommandPlaystatsGunRunningRnD(sGunRunningRnD* data)
	{
		scriptAssertf(data, "%s: PLAYSTATS_MISSION_ENDED - data is NULL.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!data)
			return;

		const u64 bid0 = ((u64)data->m_bossId1.Int) << 32;
		const u64 bid1 = (0x00000000FFFFFFFF&(u64)data->m_bossId2.Int);

		MetricGUNRUN_RND m;
		m.m_bossId = bid0|bid1;

		m.m_StartorEnd = data->m_StartorEnd.Int;
		m.m_UpgradeType = data->m_UpgradeType.Int;
		m.m_staffPercentage = data->m_staffPercentage.Int;
		m.m_supplyAmount = data->m_supplyAmount.Int;
		m.m_productionAmount = data->m_productionAmount.Int;

		APPEND_METRIC(m);
	}


	//////////////////////////////////////////////////////////////////////////
	/// Smuggler

	class MetricPEGASAIRCRFT: public MetricPlayStat
	{
		RL_DECLARE_METRIC(PEGASAIRCRFT, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		MetricPEGASAIRCRFT(int vehicleId) 
			: MetricPlayStat()
			, m_vehicleId(vehicleId)
		{	
		}

		virtual bool Write(RsonWriter* rw) const
		{
			return MetricPlayStat::Write(rw)
				&& rw->WriteInt("v", m_vehicleId);
		}

	public:
		int m_vehicleId;
	};
	void CommandAppendPegasusAsPersonalAircraft(int vehicleId)
	{
		MetricPEGASAIRCRFT m(vehicleId);
		APPEND_METRIC(m);
	}


	class MetricSMUG_MISSION_ENDED: public MetricPlayStat
	{
		RL_DECLARE_METRIC(SMUG_MISSION_ENDED, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		MetricSMUG_MISSION_ENDED() 
			: MetricPlayStat()
			, m_a(-1)
			, m_b(-1)
			, m_c(-1)
			, m_d(-1)
			, m_e(-1)
			, m_f(-1)
			, m_g(-1)
			, m_h(-1)
			, m_i(-1)
			, m_j(-1)
			, m_k(-1)
			, m_l(-1)
			, m_m(-1)
			, m_n(-1)
			, m_o(-1)
			, m_p(-1)
			, m_q(-1)
			, m_r(-1)
			, m_s(-1)
			, m_t(-1)
			, m_u(-1)
			, m_v(-1)
			, m_w(-1)
			, m_x(-1)
			, m_y(-1)
			, m_z(-1)
			, m_aa(-1)
			, m_ab(-1)
			, m_ac(-1)
			, m_ad(-1)
			, m_ae(-1)
			, m_af(-1)
			, m_ag(-1)
			, m_ah(-1)
			, m_ai(-1)
			, m_aj(-1)
			, m_ak(-1)
			, m_al(-1)
			, m_am(-1)
			, m_an(-1)
			, m_ao(-1)
			, m_ap(-1)
			, m_aq(-1)
			, m_ar(-1)
			, m_as(-1)
			, m_at(-1)
			, m_au(-1)
			, m_av(-1)
		{	
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);

			result &= rw->WriteInt("mtid", m_missionTypeId);
			result &= rw->WriteInt("mid", m_missionId);
			result &= rw->WriteInt64("maid", m_matchId);
			result &= rw->WriteInt64("buuid", m_bossId);
			result &= rw->WriteInt("isboss", m_isboss);

			if(m_a != -1)  result &= rw->WriteInt("a", m_a);
			if(m_b != -1)  result &= rw->WriteInt("b", m_b);
			if(m_c != -1)  result &= rw->WriteInt("c", m_c);
			if(m_d != -1)  result &= rw->WriteInt("d", m_d);
			if(m_e != -1)  result &= rw->WriteInt("e", m_e);
			if(m_f != -1)  result &= rw->WriteInt("f", m_f);
			if(m_g != -1)  result &= rw->WriteInt("g", m_g);
			if(m_h != -1)  result &= rw->WriteInt("h", m_h);
			if(m_i != -1)  result &= rw->WriteInt("i", m_i);
			if(m_j != -1)  result &= rw->WriteInt("j", m_j);
			if(m_k != -1)  result &= rw->WriteInt("k", m_k);
			if(m_l != -1)  result &= rw->WriteInt("l", m_l);
			if(m_m != -1)  result &= rw->WriteInt("m", m_m);
			if(m_n != -1)  result &= rw->WriteInt("n", m_n);
			if(m_o != -1)  result &= rw->WriteInt("o", m_o);
			if(m_p != -1)  result &= rw->WriteInt("p", m_p);
			if(m_q != -1)  result &= rw->WriteInt("q", m_q);
			if(m_r != -1)  result &= rw->WriteInt("r", m_r);
			if(m_s != -1)  result &= rw->WriteInt("s", m_s);
			if(m_t != -1)  result &= rw->WriteInt("t", m_t);
			if(m_u != -1)  result &= rw->WriteInt("u", m_u);
			if(m_v != -1)  result &= rw->WriteInt("v", m_v);
			if(m_w != -1)  result &= rw->WriteInt("w", m_w);
			if(m_x != -1)  result &= rw->WriteInt("x", m_x);
			if(m_y != -1)  result &= rw->WriteInt("y", m_y);
			if(m_z != -1)  result &= rw->WriteInt("z", m_z);
			if(m_aa != -1)  result &= rw->WriteInt("aa", m_aa);
			if(m_ab != -1)  result &= rw->WriteInt("ab", m_ab);
			if(m_ac != -1)  result &= rw->WriteInt("ac", m_ac);
			if(m_ad != -1)  result &= rw->WriteInt("ad", m_ad);
			if(m_ae != -1)  result &= rw->WriteInt("ae", m_ae);
			if(m_af != -1)  result &= rw->WriteInt("af", m_af);
			if(m_ag != -1)  result &= rw->WriteInt("ag", m_ag);
			if(m_ah != -1)  result &= rw->WriteInt("ah", m_ah);
			if(m_ai != -1)  result &= rw->WriteInt("ai", m_ai);
			if(m_aj != -1)  result &= rw->WriteInt("aj", m_aj);
			if(m_ak != -1)  result &= rw->WriteInt("ak", m_ak);
			if(m_al != -1)  result &= rw->WriteInt("al", m_al);
			if(m_am != -1)  result &= rw->WriteInt("am", m_am);
			if(m_an != -1)  result &= rw->WriteInt("an", m_an);
			if(m_ao != -1)  result &= rw->WriteInt("ao", m_ao);
			if(m_ap != -1)  result &= rw->WriteInt("ap", m_ap);
			if(m_aq != -1)  result &= rw->WriteInt("aq", m_aq);
			if(m_ar != -1)  result &= rw->WriteInt("ar", m_ar);
			if(m_as != -1)  result &= rw->WriteInt("as", m_as);
			if(m_at != -1)  result &= rw->WriteInt("at", m_at);
			if (m_au != -1)  result &= rw->WriteInt("au", m_au);
			if (m_av != -1)  result &= rw->WriteBool("av", (m_av == 1));

			return result;
		}

	public:
		int m_missionTypeId;
		int m_missionId;
		u64 m_matchId;
		u64 m_bossId;
		int m_isboss;
		int m_a;
		int m_b;
		int m_c;
		int m_d;
		int m_e;
		int m_f;
		int m_g;
		int m_h;
		int m_i;
		int m_j;
		int m_k;
		int m_l;
		int m_m;
		int m_n;
		int m_o;
		int m_p;
		int m_q;
		int m_r;
		int m_s;
		int m_t;
		int m_u;
		int m_v;
		int m_w;
		int m_x;
		int m_y;
		int m_z;
		int m_aa;
		int m_ab;
		int m_ac;
		int m_ad;
		int m_ae;
		int m_af;
		int m_ag;
		int m_ah;
		int m_ai;
		int m_aj;
		int m_ak;
		int m_al;
		int m_am;
		int m_an;
		int m_ao;
		int m_ap;
		int m_aq;
		int m_ar;
		int m_as;
		int m_at;
		int m_au;
		int m_av;
	};

	void CommandPlaystatsSmugglerMissionEnded(sSmugglerMissionEnded* data)
	{
		scriptAssertf(data, "%s: PLAYSTATS_MISSION_ENDED - data is NULL.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!data)
			return;

		const u64 bid0 = ((u64)data->m_bossId1.Int) << 32;
		const u64 bid1 = (0x00000000FFFFFFFF&(u64)data->m_bossId2.Int);
		const u64 localbossId = StatsInterface::GetUInt64Stat(StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID"));

		MetricSMUG_MISSION_ENDED m;
		m.m_missionTypeId = data->m_missionTypeId.Int;
		scriptAssertf(data->m_missionId.Int != 0, "%s: PLAYSTATS_MISSION_ENDED - MissionId is 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		m.m_missionId = data->m_missionId.Int;
		const u64 mid0 = ((u64)data->m_matchId1.Int) << 32;
		const u64 mid1 = ((u64)data->m_matchId2.Int);
		m.m_matchId = mid0|mid1;
		m.m_bossId = bid0|bid1;
		m.m_isboss = (m.m_bossId != 0 && m.m_bossId == localbossId);

		m.m_a = data->m_PlayerParticipated.Int;
		m.m_b = data->m_PlayerRole.Int;
		m.m_c = data->m_TimeStart.Int;
		m.m_d = data->m_TimeEnd.Int;
		m.m_e = data->m_Won.Int;
		m.m_f = data->m_EndingReason.Int;
		m.m_g = data->m_CashEarned.Int;
		m.m_h = data->m_RpEarned.Int;
		m.m_i = data->m_TargetsKilled.Int;
		m.m_j = data->m_InnocentsKilled.Int;
		m.m_k = data->m_Deaths.Int;
		m.m_l = data->m_LauncherRank.Int;
		m.m_m = data->m_TimeTakenToComplete.Int;
		m.m_n = data->m_PlayersLeftInProgress.Int;
		m.m_o = data->m_Location.Int;
		m.m_p = data->m_InvitesSent.Int;
		m.m_q = data->m_InvitesAccepted.Int;
		m.m_r = data->m_BossKilled.Int;
		m.m_s = data->m_GoonsKilled.Int;
		m.m_t = data->m_SuppliesStolen.Int;
		m.m_u = data->m_SuppliesOwned.Int;
		m.m_v = data->m_ResuppliesPassed.Int;
		m.m_w = data->m_OwnHangar.Int;
		m.m_x = data->m_OwnFurniture.Int;
		m.m_y = data->m_OwnFlooring.Int;
		m.m_z = data->m_OwnStyle.Int;
		m.m_aa = data->m_OwnLivingArea.Int;
		m.m_ab = data->m_OwnLighting.Int;
		m.m_ac = data->m_OwnWorkshop.Int;
		m.m_ad = data->m_AttackType.Int;
		m.m_ae = data->m_BossType.Int;
		m.m_af = data->m_MembersParticipated.Int;
		m.m_ag = data->m_HangarSlot1.Int;
		m.m_ah = data->m_HangarSlot2.Int;
		m.m_ai = data->m_HangarSlot3.Int;
		m.m_aj = data->m_HangarSlot4.Int;
		m.m_ak = data->m_HangarSlot5.Int;
		m.m_al = data->m_HangarSlot6.Int;
		m.m_am = data->m_HangarSlot7.Int;
		m.m_an = data->m_HangarSlot8.Int;
		m.m_ao = data->m_HangarSlot9.Int;
		m.m_ap = data->m_HangarSlot10.Int;
		m.m_aq = data->m_Vehicle1Health.Int;
		m.m_ar = data->m_Vehicle2Health.Int;
		m.m_as = data->m_Vehicle3Health.Int;
		m.m_at = data->m_SourceAnyType.Int;
		m.m_au = data->m_ContrabandType.Int;
		m.m_av = data->m_FromHackerTruck.Int;

		APPEND_METRIC(m);
	}

	//////////////////////////////////////////////////////////////////////////  CHRISTMAS 2017

	class MetricH2_FMPREP_END: public MetricPlayStat
	{
		RL_DECLARE_METRIC(H2_FMPREP_END, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		MetricH2_FMPREP_END() 
			: MetricPlayStat()
			, m_a(-1)
			, m_b(-1)
			, m_c(-1)
			, m_d(-1)
			, m_e(-1)
			, m_f(-1)
			, m_g(-1)
			, m_h(-1)
			, m_i(-1)
			, m_j(-1)
			, m_k(-1)
			, m_l(-1)
			, m_m(-1)
			, m_n(-1)
			, m_o(-1)
			, m_p(-1)
			, m_q(-1)
			, m_r(-1)
			, m_s(-1)
			, m_t(-1)
			, m_u(-1)
			, m_v(-1)
			, m_w(-1)
			, m_x(-1)
			, m_y(-1)
			, m_z(-1)
			, m_aa(-1)
			, m_ab(-1)
			, m_ac(-1)
			, m_ad(-1)
			, m_ae(-1)
			, m_af(-1)
			, m_ag(-1)
			, m_ah(-1)
			, m_ai(-1)
			, m_aj(-1)
			, m_ak(-1)
		{	
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);

			result &= rw->WriteInt("mtid", m_missionTypeId);
			result &= rw->WriteInt("mid", m_missionId);
			result &= rw->WriteInt64("maid", m_matchId);
			result &= rw->WriteInt64("buuid", m_bossId);
			result &= rw->WriteInt64("sbuuid", m_starterBossId);
			result &= rw->WriteInt("isboss", m_isboss);

			if(m_a != -1)  result &= rw->WriteInt("a", m_a);
			if(m_b != -1)  result &= rw->WriteInt("b", m_b);
			if(m_c != -1)  result &= rw->WriteInt("c", m_c);
			if(m_d != -1)  result &= rw->WriteInt("d", m_d);
			if(m_e != -1)  result &= rw->WriteInt("e", m_e);
			if(m_f != -1)  result &= rw->WriteInt("f", m_f);
			if(m_g != -1)  result &= rw->WriteInt("g", m_g);
			if(m_h != -1)  result &= rw->WriteInt("h", m_h);
			if(m_i != -1)  result &= rw->WriteInt("i", m_i);
			if(m_j != -1)  result &= rw->WriteInt("j", m_j);
			if(m_k != -1)  result &= rw->WriteInt("k", m_k);
			if(m_l != -1)  result &= rw->WriteInt("l", m_l);
			if(m_m != -1)  result &= rw->WriteInt("m", m_m);
			if(m_n != -1)  result &= rw->WriteInt("n", m_n);
			if(m_o != -1)  result &= rw->WriteInt("o", m_o);
			if(m_p != -1)  result &= rw->WriteInt("p", m_p);
			if(m_q != -1)  result &= rw->WriteInt("q", m_q);
			if(m_r != -1)  result &= rw->WriteInt("r", m_r);
			if(m_s != -1)  result &= rw->WriteInt("s", m_s);
			if(m_t != -1)  result &= rw->WriteInt("t", m_t);
			if(m_u != -1)  result &= rw->WriteInt("u", m_u);
			if(m_v != -1)  result &= rw->WriteInt("v", m_v);
			if(m_w != -1)  result &= rw->WriteInt("w", m_w);
			if(m_x != -1)  result &= rw->WriteInt("x", m_x);
			if(m_y != -1)  result &= rw->WriteInt("y", m_y);
			if(m_z != -1)  result &= rw->WriteInt("z", m_z);
			if(m_aa != -1)  result &= rw->WriteInt("aa", m_aa);
			if(m_ab != -1)  result &= rw->WriteInt("ab", m_ab);
			if(m_ac != -1)  result &= rw->WriteInt("ac", m_ac);
			if(m_ad != -1)  result &= rw->WriteInt("ad", m_ad);
			if(m_ae != -1)  result &= rw->WriteInt("ae", m_ae);
			if(m_af != -1)  result &= rw->WriteInt("af", m_af);
			if(m_ag != -1)  result &= rw->WriteInt("ag", m_ag);
			if(m_ah != -1)  result &= rw->WriteInt("ah", m_ah);
			if(m_ai != -1)  result &= rw->WriteInt("ai", m_ai);
			if(m_aj != -1)  result &= rw->WriteInt("aj", m_aj);
			if(m_ak != -1)  result &= rw->WriteInt("ak", m_ak);
			if(m_al != -1)  result &= rw->WriteInt("al", m_al);

			if (m_r != -1 && m_ae != -1)
			{
				u64 heistinfo = ((u64)m_r) << 32; heistinfo |= m_ae & 0xFFFFFFFF;
				result = result && rw->WriteInt64("hi", (s64)heistinfo);
			}

			return result;
		}

	public:
		int m_missionTypeId;
		int m_missionId;
		u64 m_matchId;
		u64 m_bossId;
		int m_isboss;
		int m_a;
		int m_b;
		int m_c;
		int m_d;
		int m_e;
		int m_f;
		int m_g;
		int m_h;
		int m_i;
		int m_j;
		int m_k;
		int m_l;
		int m_m;
		int m_n;
		int m_o;
		int m_p;
		int m_q;
		int m_r;
		int m_s;
		int m_t;
		int m_u;
		int m_v;
		int m_w;
		int m_x;
		int m_y;
		int m_z;
		int m_aa;
		int m_ab;
		int m_ac;
		int m_ad;
		int m_ae;
		int m_af;
		int m_ag;
		u64 m_starterBossId;
		int m_ah;
		int m_ai;
		int m_aj;
		int m_ak;
		int m_al;
	};

	void CommandPlaystatsFreemodePrepEnd(sFreemodePrepEnded* data)
	{
		scriptAssertf(data, "%s: PLAYSTATS_FM_HEIST_PREP_ENDED - data is NULL.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!data)
			return;

		u64 bid0 = ((u64)data->m_bossId1.Int) << 32;
		u64 bid1 = (0x00000000FFFFFFFF&(u64)data->m_bossId2.Int);
		const u64 localbossId = StatsInterface::GetUInt64Stat(StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID"));

		MetricH2_FMPREP_END m;
		m.m_missionTypeId = data->m_missionTypeId.Int;
		scriptAssertf(data->m_missionId.Int != 0, "%s: PLAYSTATS_FM_HEIST_PREP_ENDED - MissionId is 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		m.m_missionId = data->m_missionId.Int;
		const u64 mid0 = ((u64)data->m_matchId1.Int) << 32;
		const u64 mid1 = ((u64)data->m_matchId2.Int);
		m.m_matchId = mid0|mid1;
		m.m_bossId = bid0|bid1;
		m.m_isboss = (m.m_bossId != 0 && m.m_bossId == localbossId);

		m.m_a = data->m_PlayerParticipated.Int;
		m.m_b = data->m_PlayerRole.Int;
		m.m_c = data->m_TimeStart.Int;
		m.m_d = data->m_TimeEnd.Int;
		m.m_e = data->m_Won.Int;
		m.m_f = data->m_EndingReason.Int;
		m.m_g = data->m_CashEarned.Int;
		m.m_h = data->m_RpEarned.Int;
		m.m_i = data->m_TargetsKilled.Int;
		m.m_j = data->m_InnocentsKilled.Int;
		m.m_k = data->m_Deaths.Int;
		m.m_l = data->m_LauncherRank.Int;
		m.m_m = data->m_TimeTakenToComplete.Int;
		m.m_n = data->m_PlayersLeftInProgress.Int;
		m.m_o = data->m_Location.Int;
		m.m_p = data->m_InvitesSent.Int;
		m.m_q = data->m_InvitesAccepted.Int;
		m.m_r = data->m_HeistSessionID.Int;
		m.m_s = data->m_OwnBase.Int;
		m.m_t = data->m_OwnCannon.Int;
		m.m_u = data->m_OwnSecurityRoom.Int;
		m.m_v = data->m_OwnLounge.Int;
		m.m_w = data->m_OwnLivingQuarters.Int;
		m.m_x = data->m_OwnTiltRotor.Int;
		m.m_y = data->m_OrbitalCannonShots.Int;
		m.m_z = data->m_OrbitalCannonKills.Int;
		m.m_aa = data->m_AssasinationLevel1Calls.Int;
		m.m_ab = data->m_AssasinationLevel2Calls.Int;
		m.m_ac = data->m_AssasinationLevel3Calls.Int;
		m.m_ad = data->m_PrepCompletionType.Int;
		m.m_ae = data->m_TimeSpentHacking.Int;
		m.m_af = data->m_FailedStealth.Int;
		m.m_ag = data->m_QuickRestart.Int;
		bid0 = ((u64)data->m_StarterBossId1.Int) << 32;
		bid1 = (0x00000000FFFFFFFF&(u64)data->m_StarterBossId2.Int);
		m.m_starterBossId = bid0|bid1;
		m.m_ah = data->m_SessionID.Int;
		m.m_ai = data->m_BossType.Int;
		m.m_aj = data->m_AttackType.Int;
		m.m_ak = data->m_TimeTakenForObjective.Int;
		m.m_al = data->m_EntitiesStolen.Int;

		APPEND_METRIC_FLUSH(m, true);
	}


	class MetricH2_INSTANCE_END: public MetricJOB_ENDED
	{
		RL_DECLARE_METRIC(H2_INSTANCE_END, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		MetricH2_INSTANCE_END(const char* playlistid, const sJobInfo& jobInfo)
			: MetricJOB_ENDED(jobInfo, playlistid)
		{

		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricJOB_ENDED::Write(rw);

			result &= rw->WriteInt64("buuid", m_bossId);
			result &= rw->WriteInt("isboss", m_isboss);

			result &= rw->WriteInt("a", m_BossType);
			result &= rw->WriteInt("bb", m_AttackType);
			result &= rw->WriteInt("c", m_OwnBase);
			result &= rw->WriteInt("d", m_OwnCannon);
			result &= rw->WriteInt("e", m_OwnSecurityRoom);
			result &= rw->WriteInt("f", m_OwnLounge);
			result &= rw->WriteInt("g", m_OwnLivingQuarters);
			result &= rw->WriteInt("h", m_OwnTiltRotor);
			result &= rw->WriteInt("ii", m_OrbitalCannonShots);
			result &= rw->WriteInt("j", m_OrbitalCannonKills);
			result &= rw->WriteInt("k", m_AssasinationLevel1Calls);
			result &= rw->WriteInt("l", m_AssasinationLevel2Calls);
			result &= rw->WriteInt("m", m_AssasinationLevel3Calls);
			result &= rw->WriteInt("n", m_ObserveTargetCalls);
			result &= rw->WriteInt("o", m_PrepCompletionType);
			result &= rw->WriteInt("p", m_FailedStealth);
			result &= rw->WriteInt("q", m_QuickRestart);

			return result;
		}

	public:

		u64 m_bossId;
		JobBInfo m_jobInfos;
		int m_isboss;
		int m_BossType;
		int m_AttackType;	
		int m_OwnBase;
		int m_OwnCannon;
		int m_OwnSecurityRoom;
		int m_OwnLounge;
		int m_OwnLivingQuarters;
		int m_OwnTiltRotor;
		int m_OrbitalCannonShots;
		int m_OrbitalCannonKills;
		int m_AssasinationLevel1Calls;
		int m_AssasinationLevel2Calls;
		int m_AssasinationLevel3Calls;
		int m_ObserveTargetCalls;
		int m_PrepCompletionType;
		int m_FailedStealth;
		int m_QuickRestart;
	};

	void CommandPlaystatsInstancedHeistEnd(const char* creator, const char* matchId, const char* playlistid, sInstancedHeistEnded* data)
	{
		scriptAssertf(data, "%s: PLAYSTATS_INSTANCED_HEIST_ENDED - data is NULL.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!data)
			return;

		gnetAssertf(CNetworkTelemetry::GetMatchHistoryId(), "Invalid MatchHistoryId! Please add logs to bugstar:4266167.");

		u64 bid0 = ((u64)data->m_bossId1.Int) << 32;
		u64 bid1 = (0x00000000FFFFFFFF&(u64)data->m_bossId2.Int);
		const u64 localbossId = StatsInterface::GetUInt64Stat(StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID"));

		sJobInfo jobInfo;

		jobInfo.m_dataU64[jobInfo.m_u64used] = ((u64)data->m_JobInfos.m_heistSessionHashMac.Int) << 32;
		jobInfo.m_dataU64[jobInfo.m_u64used] |= data->m_JobInfos.m_heistSessionIdPosixTime.Int & 0xFFFFFFFF;
		++jobInfo.m_u64used; gnetAssert(jobInfo.m_u64used <= sJobInfo::NUM_U64);

		s64 cashStart = s_cashStart;
		s64 cashEnd = MoneyInterface::GetVCWalletBalance() + MoneyInterface::GetVCBankBalance();
		jobInfo.m_dataU64[jobInfo.m_u64used] = cashStart;
		++jobInfo.m_u64used; gnetAssert(jobInfo.m_u64used <= sJobInfo::NUM_U64);
		jobInfo.m_dataU64[jobInfo.m_u64used] = cashEnd;
		++jobInfo.m_u64used; gnetAssert(jobInfo.m_u64used <= sJobInfo::NUM_U64);

		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(data->m_JobInfos.m_role.Int >= 0) ? data->m_JobInfos.m_role.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(data->m_JobInfos.m_cashcutpercentage.Int >= 0) ? data->m_JobInfos.m_cashcutpercentage.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(data->m_JobInfos.m_kills.Int >= 0) ? data->m_JobInfos.m_kills.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(data->m_JobInfos.m_deaths.Int >= 0) ? data->m_JobInfos.m_deaths.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(data->m_JobInfos.m_clothes.Int >= 0) ? data->m_JobInfos.m_clothes.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(data->m_JobInfos.m_Cash.Int >= 0) ? data->m_JobInfos.m_Cash.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(data->m_JobInfos.m_CashStart.Int >= 0) ? data->m_JobInfos.m_CashStart.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(data->m_JobInfos.m_CashEnd.Int >= 0) ? data->m_JobInfos.m_CashEnd.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(data->m_JobInfos.m_MissionLaunch.Int >= 0) ? data->m_JobInfos.m_MissionLaunch.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(data->m_JobInfos.m_leaderscashcutpercentage.Int >= 0) ? data->m_JobInfos.m_leaderscashcutpercentage.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = data->m_JobInfos.m_heistRootConstentId.Int;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(data->m_JobInfos.m_TimePlanningBoard.Int >= 0) ? data->m_JobInfos.m_TimePlanningBoard.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(data->m_JobInfos.m_outfitChoiceBy.Int >= 0) ? data->m_JobInfos.m_outfitChoiceBy.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(data->m_JobInfos.m_outfitChoiceType.Int >= 0) ? data->m_JobInfos.m_outfitChoiceType.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(data->m_JobInfos.m_outfitStyle.Int >= -1) ? data->m_JobInfos.m_outfitStyle.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(data->m_JobInfos.m_difficulty.Int >= 0) ? data->m_JobInfos.m_difficulty.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(data->m_JobInfos.m_1stperson.Int >= 0) ? data->m_JobInfos.m_1stperson.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(data->m_JobInfos.m_medal.Int >= 0) ? data->m_JobInfos.m_medal.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(data->m_JobInfos.m_teamLivesUsed.Int >= 0) ? data->m_JobInfos.m_teamLivesUsed.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(data->m_JobInfos.m_failureReason.Int >= 0) ? data->m_JobInfos.m_failureReason.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(data->m_JobInfos.m_failureRole.Int >= 0) ? data->m_JobInfos.m_failureRole.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(data->m_JobInfos.m_usedQuickRestart.Int >= 0) ? data->m_JobInfos.m_usedQuickRestart.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(data->m_JobInfos.m_cashLost.Int >= 0) ? data->m_JobInfos.m_cashLost.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(data->m_JobInfos.m_cashPickedUp.Int >= 0) ? data->m_JobInfos.m_cashPickedUp.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(data->m_JobInfos.m_minigameTimeTaken0.Int >= 0) ? data->m_JobInfos.m_minigameTimeTaken0.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(data->m_JobInfos.m_minigameNumberOfTimes0.Int >= 0) ? data->m_JobInfos.m_minigameNumberOfTimes0.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(data->m_JobInfos.m_minigameTimeTaken1.Int >= 0) ? data->m_JobInfos.m_minigameTimeTaken1.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(data->m_JobInfos.m_minigameNumberOfTimes1.Int >= 0) ? data->m_JobInfos.m_minigameNumberOfTimes1.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);
		jobInfo.m_dataINTS[jobInfo.m_intsused] = scriptVerify(data->m_JobInfos.m_maskChoice.Int >= 0) ? data->m_JobInfos.m_maskChoice.Int : 0;
		++jobInfo.m_intsused; gnetAssert(jobInfo.m_intsused <= sJobInfo::NUM_INTS);

		jobInfo.m_dataU32[jobInfo.m_u32used] = fwTimer::GetSystemTimeInMilliseconds() - CNetworkTelemetry::GetMatchStartTime();
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = (u32)StatsInterface::GetUInt64Stat(STAT_TOTAL_PLAYING_TIME.GetHash());;
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = (u32)data->m_JobInfos.m_XP.Int;
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = (u32)data->m_JobInfos.m_Suicides.Int;
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = (u32)data->m_JobInfos.m_Rank.Int;
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = (u32)data->m_JobInfos.m_CrewId.Int;
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = (u32)data->m_JobInfos.m_TeamId.Int;
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = (u32)data->m_JobInfos.m_matchResult.Int;
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = (u32)data->m_JobInfos.m_jpEarned.Int;
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = (u32)data->m_JobInfos.m_celebrationanim.Int;
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);
		jobInfo.m_dataU32[jobInfo.m_u32used] = (u32)data->m_JobInfos.m_quickplayanim.Int;
		++jobInfo.m_u32used; gnetAssert(jobInfo.m_u32used <= sJobInfo::NUM_U32);

		jobInfo.m_dataBOOLEANS[jobInfo.m_boolused] = (data->m_JobInfos.m_ishost.Int != 0) ? true : false;
		++jobInfo.m_boolused; gnetAssert(jobInfo.m_boolused <= sJobInfo::NUM_BOOLS);
		jobInfo.m_dataBOOLEANS[jobInfo.m_boolused] = (data->m_JobInfos.m_sessionVisible.Int != 0) ? true : false;
		++jobInfo.m_boolused; gnetAssert(jobInfo.m_boolused <= sJobInfo::NUM_BOOLS);
		jobInfo.m_dataBOOLEANS[jobInfo.m_boolused] = (data->m_JobInfos.m_leftInProgress.Int != 0) ? true : false;
		++jobInfo.m_boolused; gnetAssert(jobInfo.m_boolused <= sJobInfo::NUM_BOOLS);
		jobInfo.m_dataBOOLEANS[jobInfo.m_boolused] = (data->m_JobInfos.m_lesterCalled.Int != 0) ? true : false;
		++jobInfo.m_boolused; gnetAssert(jobInfo.m_boolused <= sJobInfo::NUM_BOOLS);
		jobInfo.m_dataBOOLEANS[jobInfo.m_boolused] = (data->m_JobInfos.m_spookedCops.Int != 0) ? true : false;
		++jobInfo.m_boolused; gnetAssert(jobInfo.m_boolused <= sJobInfo::NUM_BOOLS);
		jobInfo.m_dataBOOLEANS[jobInfo.m_boolused] = (data->m_JobInfos.m_usedTripSkip.Int != 0) ? true : false;
		++jobInfo.m_boolused; gnetAssert(jobInfo.m_boolused <= sJobInfo::NUM_BOOLS);

		jobInfo.m_MatchCreator[0] = '\0';
		safecpy(jobInfo.m_MatchCreator, creator, COUNTOF(jobInfo.m_MatchCreator));

		jobInfo.m_UniqueMatchId[0] = '\0';
		safecpy(jobInfo.m_UniqueMatchId, matchId, COUNTOF(jobInfo.m_UniqueMatchId));

		MetricH2_INSTANCE_END m(playlistid, jobInfo);
		
		m.m_bossId = bid0|bid1;
		m.m_isboss = (m.m_bossId != 0 && m.m_bossId == localbossId);

		m.m_BossType = data->m_BossType.Int;
		m.m_AttackType = data->m_AttackType.Int;
		m.m_OwnBase = data->m_OwnBase.Int;
		m.m_OwnCannon = data->m_OwnCannon.Int;
		m.m_OwnSecurityRoom = data->m_OwnSecurityRoom.Int;
		m.m_OwnLounge = data->m_OwnLounge.Int;
		m.m_OwnLivingQuarters = data->m_OwnLivingQuarters.Int;
		m.m_OwnTiltRotor = data->m_OwnTiltRotor.Int;
		m.m_OrbitalCannonShots = data->m_OrbitalCannonShots.Int;
		m.m_OrbitalCannonKills = data->m_OrbitalCannonKills.Int;
		m.m_AssasinationLevel1Calls = data->m_AssasinationLevel1Calls.Int;
		m.m_AssasinationLevel2Calls = data->m_AssasinationLevel2Calls.Int;
		m.m_AssasinationLevel3Calls = data->m_AssasinationLevel3Calls.Int;
		m.m_ObserveTargetCalls = data->m_ObserveTargetCalls.Int;
		m.m_PrepCompletionType = data->m_PrepCompletionType.Int;
		m.m_FailedStealth = data->m_FailedStealth.Int;
		m.m_QuickRestart = data->m_QuickRestart.Int;

		APPEND_METRIC_FLUSH(m, true);
		CNetworkTelemetry::SessionMatchEnd();
	}


	class MetricDAR_MISSION_END: public MetricPlayStat
	{
		RL_DECLARE_METRIC(DAR_MISSION_END, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		MetricDAR_MISSION_END() 
			: MetricPlayStat()
		{	
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);

			result &= rw->WriteInt("a", m_StartLocation);
			result &= rw->WriteInt("b", m_Checkpoint1Complete);
			result &= rw->WriteInt("c", m_TimeTakenToCompleteCheckpoint1);
			result &= rw->WriteInt("d", m_Checkpoint2Complete);
			result &= rw->WriteInt("e", m_TimeTakenToCompleteCheckpoint2);
			result &= rw->WriteInt("f", m_Checkpoint3Complete);
			result &= rw->WriteInt("g", m_TimeTakenToCompleteCheckpoint3);
			result &= rw->WriteInt("h", m_Checkpoint4Complete);
			result &= rw->WriteInt("i", m_TimeTakenToCompleteCheckpoint4);
			result &= rw->WriteInt("j", m_EndLocation);
			result &= rw->WriteInt("k", m_DarAcquired);
			result &= rw->WriteInt("l", m_TotalTimeSpent);
			result &= rw->WriteInt("m", m_CashEarned);
			result &= rw->WriteInt("n", m_RPEarned);
			result &= rw->WriteInt("o", m_Replay);
			result &= rw->WriteInt("p", m_FailedReason);
			result &= rw->WriteInt("q", m_RockstarAccountIndicator);

			return result;
		}

	public:

		int m_StartLocation;
		int m_Checkpoint1Complete;
		int m_TimeTakenToCompleteCheckpoint1;
		int m_Checkpoint2Complete;
		int m_TimeTakenToCompleteCheckpoint2;
		int m_Checkpoint3Complete;
		int m_TimeTakenToCompleteCheckpoint3;
		int m_Checkpoint4Complete;
		int m_TimeTakenToCompleteCheckpoint4;
		int m_EndLocation;
		int m_DarAcquired;
		int m_TotalTimeSpent;
		int m_CashEarned;
		int m_RPEarned;
		int m_Replay;
		int m_FailedReason;
		int m_RockstarAccountIndicator;
	};

	void CommandPlaystatsDarCheckpoint(sDarCheckpoint* data)
	{
		scriptAssertf(data, "%s: PLAYSTATS_INSTANCED_HEIST_ENDED - data is NULL.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!data)
			return;

		MetricDAR_MISSION_END m;
		m.m_StartLocation = data->m_StartLocation.Int;
		m.m_Checkpoint1Complete = data->m_Checkpoint1Complete.Int;
		m.m_TimeTakenToCompleteCheckpoint1 = data->m_TimeTakenToCompleteCheckpoint1.Int;
		m.m_Checkpoint2Complete = data->m_Checkpoint2Complete.Int;
		m.m_TimeTakenToCompleteCheckpoint2 = data->m_TimeTakenToCompleteCheckpoint2.Int;
		m.m_Checkpoint3Complete = data->m_Checkpoint3Complete.Int;
		m.m_TimeTakenToCompleteCheckpoint3 = data->m_TimeTakenToCompleteCheckpoint3.Int;
		m.m_Checkpoint4Complete = data->m_Checkpoint4Complete.Int;
		m.m_TimeTakenToCompleteCheckpoint4 = data->m_TimeTakenToCompleteCheckpoint4.Int;
		m.m_EndLocation = data->m_EndLocation.Int;
		m.m_DarAcquired = data->m_DarAcquired.Int;
		m.m_TotalTimeSpent = data->m_TotalTimeSpent.Int;
		m.m_CashEarned = data->m_CashEarned.Int;
		m.m_RPEarned = data->m_RPEarned.Int;
		m.m_Replay = data->m_Replay.Int;
		m.m_FailedReason = data->m_FailedReason.Int;
		m.m_RockstarAccountIndicator = data->m_RockstarAccountIndicator.Int;

		APPEND_METRIC(m);
	}


	class MetricENTER_SESSION_PACK: public MetricPlayStat
	{
		RL_DECLARE_METRIC(ENTER_SESSION_PACK, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		MetricENTER_SESSION_PACK() 
			: MetricPlayStat()
		{
			m_CharSlot      = StatsInterface::GetCurrentMultiplayerCharaterSlot();
			m_CharCash      = MoneyInterface::GetVCWalletBalance();
			m_CharBank      = MoneyInterface::GetVCBankBalance();
			m_CharEVC       = MoneyInterface::GetEVCBalance();
			m_CharPVC       = MoneyInterface::GetPVCBalance();
			m_CharPXR       = MoneyInterface::GetPXRValue();
			m_starterPack     = CLiveManager::GetCommerceMgr().IsEntitledToStarterPack();
			m_premiumPack     = CLiveManager::GetCommerceMgr().IsEntitledToPremiumPack();
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);

			result &= rw->WriteInt("a", m_CharSlot);
			result &= rw->WriteInt64("b", m_CharCash);
			result &= rw->WriteInt64("c", m_CharBank);
			result &= rw->WriteInt64("d", m_CharEVC);
			result &= rw->WriteInt64("e", m_CharPVC);
			result &= rw->WriteFloat("f", m_CharPXR);
			result &= rw->WriteInt("g", m_starterPack);
			result &= rw->WriteInt("h", m_premiumPack);
			result &= rw->WriteInt("j", m_Enterp_Prop);
			result &= rw->WriteInt("k", m_Enterp_Veh);
			result &= rw->WriteInt("l", m_Enterp_Weapon);
			result &= rw->WriteInt("m", m_Enterp_Tattoo1);
			result &= rw->WriteInt("n", m_Enterp_Tattoo2);
			result &= rw->WriteInt("o", m_Enterp_Clothing1);
			result &= rw->WriteInt("p", m_Enterp_Clothing2);

			return result;
		}

	public:
		
		int m_CharSlot;
		s64 m_CharCash;
		s64 m_CharBank;
		s64 m_CharEVC;
		s64 m_CharPVC;
		float m_CharPXR;
		int m_starterPack;
		int m_premiumPack;
		int m_Enterp_Prop;
		int m_Enterp_Veh;
		int m_Enterp_Weapon;
		int m_Enterp_Tattoo1;
		int m_Enterp_Tattoo2;
		int m_Enterp_Clothing1;
		int m_Enterp_Clothing2;
	};

	void CommandPlaystatsEnterSessionPack(sEnterSessionPack* data)
	{
		scriptAssertf(data, "%s: PLAYSTATS_ENTER_SESSION_PACK - data is NULL.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!data)
			return;

		MetricENTER_SESSION_PACK m;
		m.m_Enterp_Prop = data->m_Enterp_Prop.Int;
		m.m_Enterp_Veh = data->m_Enterp_Veh.Int;
		m.m_Enterp_Weapon = data->m_Enterp_Weapon.Int;
		m.m_Enterp_Tattoo1 = data->m_Enterp_Tattoo1.Int;
		m.m_Enterp_Tattoo2 = data->m_Enterp_Tattoo2.Int;
		m.m_Enterp_Clothing1 = data->m_Enterp_Clothing1.Int;
		m.m_Enterp_Clothing2 = data->m_Enterp_Clothing2.Int;

		APPEND_METRIC(m);
	}

	class MetricDRONE_USAGE : public MetricPlayStat
	{
		RL_DECLARE_METRIC(DRONE_USAGE, TELEMETRY_CHANNEL_MISC, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		MetricDRONE_USAGE(int time, int endReason, int totalDroneTases)
			: MetricPlayStat()
			, m_time(time)
			, m_endReason(endReason)
			, m_totalDroneTases(totalDroneTases)
		{

		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);
			result &= rw->WriteInt("t", m_time);
			result &= rw->WriteInt("e", m_endReason);
			result &= rw->WriteInt("d", m_totalDroneTases);
			return result;
		}

	public:

		int m_time;
		int m_endReason;
		int m_totalDroneTases;
	};

	void CommandPlaystatsDroneUsage(int time, int endReason, int totalDroneTases)
	{
		MetricDRONE_USAGE m(time, endReason, totalDroneTases);
		APPEND_METRIC(m);
	}

	////////////////////////////////////////////////////////////////////////// MEGA BUSINESS


	class MetricBUSINESS_BATTLE_ENDED : public MetricPlayStat
	{
		RL_DECLARE_METRIC(BUSINESS_BATTLE_ENDED, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		MetricBUSINESS_BATTLE_ENDED()
			: MetricPlayStat()
			, m_missionTypeId(0)
			, m_missionId(0)
			, m_matchId(0)
			, m_bossId(0)
			, m_isboss(false)
			, m_playerParticipated(-1)      //Whether  or not player participated 
			, m_timeStart(-1)				//Time work started
			, m_timeEnd(-1)					//Time work ended
			, m_won(-1)						//won/lost
			, m_endingReason(-1)            //Reason for script ending - timer ran out, left session, etc
			, m_cashEarned(-1)				//Cash Earned.
			, m_rpEarned(-1)				//RP Earned.
			, m_bossKilled(-1)				//Boss's killed
			, m_goonsKilled(-1)             //Goons killed
			, m_targetLocation(-1)
			, m_launchedByBoss(false)          //Whether  or not work was launched by this Boss
			, m_starterbossuuid(0)
			, m_playerrole(-1)
			, m_targetskilled(-1)
			, m_timetakentocomplete(-1)
			, m_playersleftinprogress(-1)
			, m_sessionid(0)
			, m_bosstype(-1)
			, m_attacktype(-1)
			, m_packagesstolen(-1)
			, m_participationLevel(-1)
			, m_entitiesstolentype(-1)
			, m_ownnightclub(false)
			, m_objectivelocation(-1)
			, m_deliverylocation(-1)
			, m_collectiontype(-1)
			, m_enemytype(-1)
			, m_enemyvehicle(-1)
			, m_eventitem(-1)
			, m_iscop(false)
		{
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);

			result &= rw->WriteInt("mtid", m_missionTypeId);
			result &= rw->WriteInt("mid", m_missionId);
			result &= rw->WriteInt64("maid", m_matchId);
			result &= rw->WriteInt64("buuid", m_bossId);
			result &= rw->WriteBool("isboss", m_isboss);
			
			if (m_playerParticipated != -1)  result &= rw->WriteInt("a", m_playerParticipated);
			if (m_timeStart != -1)  result &= rw->WriteInt("b", m_timeStart);
			if (m_timeEnd != -1)  result &= rw->WriteInt("c", m_timeEnd);
			if (m_won != -1)  result &= rw->WriteInt("d", m_won);
			if (m_endingReason != -1)  result &= rw->WriteInt("e", m_endingReason);
			if (m_cashEarned != -1)  result &= rw->WriteInt("f", m_cashEarned);
			if (m_rpEarned != -1)  result &= rw->WriteInt("g", m_rpEarned);
			if (m_bossKilled != -1)  result &= rw->WriteInt("h", m_bossKilled);
			if (m_goonsKilled != -1)  result &= rw->WriteInt("i", m_goonsKilled);
			if (m_targetLocation != -1)  result &= rw->WriteInt("j", m_targetLocation);
			result &= rw->WriteBool("k", m_launchedByBoss);
			result &= rw->WriteInt64("l", m_starterbossuuid);
			if (m_playerrole != -1)  result &= rw->WriteInt("m", m_playerrole);
			if (m_targetskilled != -1)  result &= rw->WriteInt("n", m_targetskilled);
			if (m_timetakentocomplete != -1)  result &= rw->WriteInt("o", m_timetakentocomplete);
			if (m_playersleftinprogress != -1)  result &= rw->WriteInt("p", m_playersleftinprogress);
			result &= rw->WriteInt64("q", m_sessionid);
			if (m_bosstype != -1)  result &= rw->WriteInt("r", m_bosstype);
			if (m_attacktype != -1)  result &= rw->WriteInt("s", m_attacktype);
			if (m_packagesstolen != -1)  result &= rw->WriteInt("t", m_packagesstolen);
			if (m_participationLevel != -1)  result &= rw->WriteInt("u", m_participationLevel);
			if (m_entitiesstolentype != -1)  result &= rw->WriteInt("v", m_entitiesstolentype);
			result &= rw->WriteBool("w", m_ownnightclub);
			if (m_objectivelocation != -1)  result &= rw->WriteInt("x", m_objectivelocation);
			if (m_deliverylocation != -1)  result &= rw->WriteInt("y", m_deliverylocation);
			if (m_collectiontype != -1)  result &= rw->WriteInt("z", m_collectiontype);
			if (m_enemytype != -1)  result &= rw->WriteInt("aa", m_enemytype);
			if (m_enemyvehicle != -1)  result &= rw->WriteInt("ab", m_enemyvehicle);
			if (m_eventitem != -1)  result &= rw->WriteInt("ac", m_eventitem);
			result &= rw->WriteBool("ad", m_iscop);

			return result;
		}

	public:

		int m_missionTypeId;
		int m_missionId;
		u64 m_matchId;
		u64 m_bossId;
		bool m_isboss;
		int m_playerParticipated;      //Whether  or not player participated 
		int m_timeStart;				//Time work started
		int m_timeEnd;					//Time work ended
		int m_won;						//won/lost
		int m_endingReason;            //Reason for script ending - timer ran out, left session, etc
		int m_cashEarned;				//Cash Earned.
		int m_rpEarned;				//RP Earned.
		int m_bossKilled;				//Boss's killed
		int m_goonsKilled;             //Goons killed
		int m_targetLocation;
		bool m_launchedByBoss;          //Whether  or not work was launched by this Boss
		u64 m_starterbossuuid;
		int m_playerrole;
		int m_targetskilled;
		int m_timetakentocomplete;
		int m_playersleftinprogress;
		u64 m_sessionid;
		int m_bosstype;
		int m_attacktype;
		int m_packagesstolen;
		int m_participationLevel;
		int m_entitiesstolentype;
		bool m_ownnightclub;
		int m_objectivelocation;
		int m_deliverylocation;
		int m_collectiontype;
		int m_enemytype;
		int m_enemyvehicle;
		int m_eventitem;
		bool m_iscop;
	};

	void CommandPlaystatsBusinessBattleEnded(sBusinessBattleEnded* data)
	{
		scriptAssertf(data, "%s: PLAYSTATS_BUSINESS_BATTLE_ENDED - data is NULL.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!data)
			return;

		const u64 bid0 = ((u64)data->m_bossId1.Int) << 32;
		const u64 bid1 = (0x00000000FFFFFFFF & (u64)data->m_bossId2.Int);
		const u64 localbossId = StatsInterface::GetUInt64Stat(StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID"));

		const u64 sbid0 = ((u64)data->m_starterBossId1.Int) << 32;
		const u64 sbid1 = (0x00000000FFFFFFFF & (u64)data->m_starterBossId2.Int);

		MetricBUSINESS_BATTLE_ENDED m;
		m.m_missionTypeId = data->m_missionTypeId.Int;
		m.m_missionId = data->m_missionId.Int;
		const u64 mid0 = ((u64)data->m_matchId1.Int) << 32;
		const u64 mid1 = ((u64)data->m_matchId2.Int);
		m.m_matchId = mid0 | mid1;
		m.m_bossId = bid0 | bid1;
		m.m_isboss = (m.m_bossId != 0 && m.m_bossId == localbossId);

		m.m_playerParticipated = data->m_playerParticipated.Int;
		m.m_timeStart = data->m_timeStart.Int;
		m.m_timeEnd = data->m_timeEnd.Int;
		m.m_won = data->m_won.Int;
		m.m_endingReason = data->m_endingReason.Int;
		m.m_cashEarned = data->m_cashEarned.Int;
		m.m_rpEarned = data->m_rpEarned.Int;
		m.m_bossKilled = data->m_bossKilled.Int;
		m.m_goonsKilled = data->m_goonsKilled.Int;
		m.m_targetLocation = data->m_targetLocation.Int;
		m.m_launchedByBoss = data->m_launchedByBoss.Int != 0;
		m.m_starterbossuuid = sbid0 | sbid1;
		m.m_playerrole = data->m_playerrole.Int;
		m.m_targetskilled = data->m_targetskilled.Int;
		m.m_timetakentocomplete = data->m_timetakentocomplete.Int;
		m.m_playersleftinprogress = data->m_playersleftinprogress.Int;
		m.m_sessionid = CNetworkTelemetry::GetCurMpSessionId();
		m.m_bosstype = data->m_bosstype.Int;
		m.m_attacktype = data->m_attacktype.Int;
		m.m_packagesstolen = data->m_packagesstolen.Int;
		m.m_participationLevel = data->m_participationLevel.Int;
		m.m_entitiesstolentype = data->m_entitiesstolentype.Int;
		m.m_ownnightclub = data->m_ownnightclub.Int != 0;
		m.m_objectivelocation = data->m_objectivelocation.Int;
		m.m_deliverylocation = data->m_deliverylocation.Int;
		m.m_collectiontype = data->m_collectiontype.Int;
		m.m_enemytype = data->m_enemytype.Int;
		m.m_enemyvehicle = data->m_enemyvehicle.Int;
		m.m_eventitem = data->m_eventitem.Int;
		m.m_iscop = data->m_iscop.Int != 0;

		APPEND_METRIC(m);
	}

	//////////////////////////////////////////////////////////////////////////
	
	class MetricWAREHOUSE_MISSION_ENDED : public MetricPlayStat
	{
		RL_DECLARE_METRIC(WAREHOUSE_MISSION_ENDED, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		MetricWAREHOUSE_MISSION_ENDED()
			: MetricPlayStat()
			, m_a(-1)
			, m_b(-1)
			, m_c(-1)
			, m_d(-1)
			, m_e(-1)
			, m_f(-1)
			, m_g(-1)
			, m_h(-1)
			, m_i(-1)
			, m_j(-1)
			, m_k(-1)
			, m_l(-1)
			, m_m(-1)
			, m_n(-1)
			, m_o(-1)
			, m_p(-1)
			, m_q(-1)
			, m_r(-1)
			, m_s(-1)
			, m_t(-1)
			, m_u(-1)
			, m_v(-1)
			, m_w(false)
			, m_x(-1)
			, m_y(false)
			, m_z(false)
			, m_aa(-1)
			, m_ab(-1)
			, m_ac(-1)
			, m_ad(-1)
			, m_ae(false)
			, m_af(false)
			, m_ag(false)
			, m_ah(false)
			, m_ai(-1)
			, m_aj(0)
			, m_ak(-1)
			, m_al(-1)
			, m_am(-1)
			, m_an(-1)
			, m_ao(-1)
			, m_ap(-1)
			, m_aq(-1)
			, m_ar(-1)
			, m_as(-1)
			, m_at(-1)
			, m_au(-1)
			, m_av(-1)
			, m_aw(false)
		{
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);

			result &= rw->WriteInt("mtid", m_missionTypeId);
			result &= rw->WriteInt("mid", m_missionId);
			result &= rw->WriteInt64("maid", m_matchId);
			result &= rw->WriteInt64("buuid", m_bossId);
			result &= rw->WriteInt("isboss", m_isboss);

			if (m_a > -1)  result &= rw->WriteInt("a", m_a);
			if (m_b > -1)  result &= rw->WriteInt("b", m_b);
			if (m_c > -1)  result &= rw->WriteInt("c", m_c);
			if (m_d > -1)  result &= rw->WriteInt("d", m_d);
			if (m_e > -1)  result &= rw->WriteInt("e", m_e);
			if (m_f > -1)  result &= rw->WriteInt("f", m_f);
			if (m_g > -1)  result &= rw->WriteInt("g", m_g);
			if (m_h > -1)  result &= rw->WriteInt("h", m_h);
			if (m_i > -1)  result &= rw->WriteInt("i", m_i);
			if (m_j > -1)  result &= rw->WriteInt("j", m_j);
			if (m_k > -1)  result &= rw->WriteInt("k", m_k);
			if (m_l > -1)  result &= rw->WriteInt("l", m_l);
			if (m_m > -1)  result &= rw->WriteInt("m", m_m);
			if (m_n > -1)  result &= rw->WriteInt("n", m_n);
			if (m_o > -1)  result &= rw->WriteInt("o", m_o);
			if (m_p > -1)  result &= rw->WriteInt("p", m_p);
			if (m_q > -1)  result &= rw->WriteInt("q", m_q);
			if (m_r > -1)  result &= rw->WriteInt("r", m_r);
			if (m_s > -1)  result &= rw->WriteInt("s", m_s);
			if (m_t > -1)  result &= rw->WriteInt("t", m_t);
			if (m_u > -1)  result &= rw->WriteInt("u", m_u);
			if (m_v > -1)  result &= rw->WriteInt("v", m_v);
			result &= rw->WriteBool("w", m_w);
			if (m_x > -1)  result &= rw->WriteInt("x", m_x);
			result &= rw->WriteBool("y", m_y);
			result &= rw->WriteBool("z", m_z);
			if (m_aa > -1)  result &= rw->WriteInt("aa", m_aa);
			if (m_ab > -1)  result &= rw->WriteInt("ab", m_ab);
			if (m_ac > -1)  result &= rw->WriteInt("ac", m_ac);
			if (m_ad > -1)  result &= rw->WriteInt("ad", m_ad);
			result &= rw->WriteBool("ae", m_ae);
			result &= rw->WriteBool("af", m_af);
			result &= rw->WriteBool("ag", m_ag);
			result &= rw->WriteBool("ah", m_ah);
			if (m_ai > -1)  result &= rw->WriteInt("ai", m_ai);
			result &= rw->WriteInt64("aj", m_aj);
			if (m_ak > -1)  result &= rw->WriteInt("ak", m_ak);
			if (m_al > -1)  result &= rw->WriteInt("al", m_al);
			if (m_am > -1)  result &= rw->WriteInt("am", m_am);
			if (m_an > -1)  result &= rw->WriteInt("an", m_an);
			if (m_ao > -1)  result &= rw->WriteInt("ao", m_ao);
			if (m_ap > -1)  result &= rw->WriteInt("ap", m_ap);
			if (m_aq > -1)  result &= rw->WriteInt("aq", m_aq);
			if (m_ar > -1)  result &= rw->WriteInt("ar", m_ar);
			if (m_as > -1)  result &= rw->WriteInt("as", m_as);
			if (m_at > -1)  result &= rw->WriteInt("at", m_at);
			if (m_au > -1)  result &= rw->WriteInt("au", m_au);
			if (m_av > -1)  result &= rw->WriteInt("av", m_av);
			result &= rw->WriteBool("aw", m_aw);

			return result;
		}

	public:
		int m_missionTypeId;
		int m_missionId;
		u64 m_matchId;
		u64 m_bossId;
		int m_isboss;
		int m_a;
		int m_b;
		int m_c;
		int m_d;
		int m_e;
		int m_f;
		int m_g;
		int m_h;
		int m_i;
		int m_j;
		int m_k;
		int m_l;
		int m_m;
		int m_n;
		int m_o;
		int m_p;
		int m_q;
		int m_r;
		int m_s;
		int m_t;
		int m_u;
		int m_v;
		bool m_w;
		int m_x;
		bool m_y;
		bool m_z;
		int m_aa;
		int m_ab;
		int m_ac;
		int m_ad;
		bool m_ae;
		bool m_af;
		bool m_ag;
		bool m_ah;
		int m_ai;
		u64 m_aj;
		int m_ak;
		int m_al;
		int m_am;
		int m_an;
		int m_ao;
		int m_ap;
		int m_aq;
		int m_ar;
		int m_as;
		int m_at;
		int m_au;
		int m_av;
		bool m_aw;
	};

	void CommandPlaystatsWarehouseMissionEnded(sWarehouseMissionEnded* data)
	{
		scriptAssertf(data, "%s: PLAYSTATS_WAREHOUSE_MISSION_ENDED - data is NULL.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!data)
			return;

		const u64 bid0 = ((u64)data->m_bossId1.Int) << 32;
		const u64 bid1 = (0x00000000FFFFFFFF & (u64)data->m_bossId2.Int);
		const u64 localbossId = StatsInterface::GetUInt64Stat(StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID"));

		MetricWAREHOUSE_MISSION_ENDED m;
		m.m_missionTypeId = data->m_missionTypeId.Int;
		scriptAssertf(data->m_missionId.Int != 0, "%s: PLAYSTATS_MISSION_ENDED - MissionId is 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		m.m_missionId = data->m_missionId.Int;
		const u64 mid0 = ((u64)data->m_matchId1.Int) << 32;
		const u64 mid1 = ((u64)data->m_matchId2.Int);
		m.m_matchId = mid0 | mid1;
		m.m_bossId = bid0 | bid1;
		m.m_isboss = (m.m_bossId != 0 && m.m_bossId == localbossId);

		m.m_a = data->m_PlayerParticipated.Int;
		m.m_b = data->m_PlayerRole.Int;
		m.m_c = data->m_TimeStart.Int;
		m.m_d = data->m_TimeEnd.Int;
		m.m_e = data->m_Won.Int;
		m.m_f = data->m_EndingReason.Int;
		m.m_g = data->m_CashEarned.Int;
		m.m_h = data->m_RpEarned.Int;
		m.m_i = data->m_TargetsKilled.Int;
		m.m_j = data->m_InnocentsKilled.Int;
		m.m_k = data->m_Deaths.Int;
		m.m_l = data->m_LauncherRank.Int;
		m.m_m = data->m_TimeTakenToComplete.Int;
		m.m_n = data->m_PlayersLeftInProgress.Int;
		m.m_o = data->m_Location.Int;
		m.m_p = data->m_InvitesSent.Int;
		m.m_q = data->m_InvitesAccepted.Int;
		m.m_r = data->m_BossKilled.Int;
		m.m_s = data->m_GoonsKilled.Int;
		m.m_t = data->m_SuppliesStolen.Int;
		m.m_u = data->m_SuppliesOwned.Int;
		m.m_v = data->m_ProductsOwned.Int;
		m.m_w = data->m_ownnightclub.Int != 0;
		m.m_x = data->m_numberoftechnicians.Int;
		m.m_y = data->m_ownequipmentupgrade.Int != 0;
		m.m_z = data->m_ownhackertruck.Int != 0;
		m.m_aa = data->m_ownwarehouseb2.Int;
		m.m_ab = data->m_ownwarehouseb3.Int;
		m.m_ac = data->m_ownwarehouseb4.Int;
		m.m_ad = data->m_ownwarehouseb5.Int;
		m.m_ae = data->m_ownmissilelauncher.Int != 0;
		m.m_af = data->m_owndronestation.Int != 0;
		m.m_ag = data->m_ownhackerbike.Int != 0;
		m.m_ah = data->m_ownvehicleworkshop.Int != 0;
		m.m_ai = data->m_quickrestart.Int;
		m.m_aj = CNetworkTelemetry::GetCurMpSessionId();
		m.m_ak = data->m_bosstype.Int;
		m.m_al = data->m_attacktype.Int;
		m.m_am = data->m_deliveryproduct.Int;
		m.m_an = data->m_enemygroup.Int;
		m.m_ao = data->m_ambushvehicle.Int;
		m.m_ap = data->m_deliveryvehicle.Int;
		m.m_aq = data->m_productssellchoice.Int;
		m.m_ar = data->m_shipmentsize.Int;
		m.m_as = data->m_totalunitssold.Int;
		m.m_at = data->m_timeusingdrone.Int;
		m.m_au = data->m_dronedestroyed.Int;
		m.m_av = data->m_totaldronetases.Int;
		m.m_aw = data->m_iscop.Int != 0;

		APPEND_METRIC(m);
	}


	class MetricNIGHTCLUB_MISSION_ENDED : public MetricPlayStat
	{
		RL_DECLARE_METRIC(NIGHTCLUB_MISSION_ENDED, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		MetricNIGHTCLUB_MISSION_ENDED()
			: MetricPlayStat()
			, m_a(-1)
			, m_b(-1)
			, m_c(-1)
			, m_d(-1)
			, m_e(-1)
			, m_f(-1)
			, m_g(-1)
			, m_h(-1)
			, m_i(-1)
			, m_j(-1)
			, m_k(-1)
			, m_l(-1)
			, m_m(-1)
			, m_n(-1)
			, m_o(-1)
			, m_p(-1)
			, m_q(-1)
			, m_r(-1)
			, m_s(-1)
			, m_t(-1)
			, m_u(-1)
			, m_v(-1)
			, m_w(false)
			, m_x(-1)
			, m_y(-1)
			, m_z(false)
			, m_aa(-1)
			, m_ab(0)
			, m_ac(-1)
			, m_ad(-1)
			, m_ae(-1)
			, m_af(-1)
			, m_ag(-1)
			, m_ah(-1)
		{
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);

			result &= rw->WriteInt("mtid", m_missionTypeId);
			result &= rw->WriteInt("mid", m_missionId);
			result &= rw->WriteInt64("maid", m_matchId);
			result &= rw->WriteInt64("buuid", m_bossId);
			result &= rw->WriteInt("isboss", m_isboss);

			if (m_a > -1)  result &= rw->WriteInt("a", m_a);
			if (m_b > -1)  result &= rw->WriteInt("b", m_b);
			if (m_c > -1)  result &= rw->WriteInt("c", m_c);
			if (m_d > -1)  result &= rw->WriteInt("d", m_d);
			if (m_e > -1)  result &= rw->WriteInt("e", m_e);
			if (m_f > -1)  result &= rw->WriteInt("f", m_f);
			if (m_g > -1)  result &= rw->WriteInt("g", m_g);
			if (m_h > -1)  result &= rw->WriteInt("h", m_h);
			if (m_i > -1)  result &= rw->WriteInt("i", m_i);
			if (m_j > -1)  result &= rw->WriteInt("j", m_j);
			if (m_k > -1)  result &= rw->WriteInt("k", m_k);
			if (m_l > -1)  result &= rw->WriteInt("l", m_l);
			if (m_m > -1)  result &= rw->WriteInt("m", m_m);
			if (m_n > -1)  result &= rw->WriteInt("n", m_n);
			if (m_o > -1)  result &= rw->WriteInt("o", m_o);
			if (m_p > -1)  result &= rw->WriteInt("p", m_p);
			if (m_q > -1)  result &= rw->WriteInt("q", m_q);
			if (m_r > -1)  result &= rw->WriteInt("r", m_r);
			if (m_s > -1)  result &= rw->WriteInt("s", m_s);
			if (m_t > -1)  result &= rw->WriteInt("t", m_t);
			if (m_u > -1)  result &= rw->WriteInt("u", m_u);
			if (m_v > -1)  result &= rw->WriteInt("v", m_v);
			result &= rw->WriteBool("w", m_w);
			if (m_x > -1)  result &= rw->WriteInt("x", m_x);
			if (m_y > -1)  result &= rw->WriteInt("y", m_y);
			result &= rw->WriteBool("z", m_z);
			if (m_aa > -1)  result &= rw->WriteInt("aa", m_aa);
			result &= rw->WriteInt64("ab", m_ab);
			if (m_ac > -1)  result &= rw->WriteInt("ac", m_ac);
			if (m_ad > -1)  result &= rw->WriteInt("ad", m_ad);
			if (m_ae > -1)  result &= rw->WriteInt("ae", m_ae);
			if (m_af > -1)  result &= rw->WriteInt("af", m_af);
			if (m_ag > -1)  result &= rw->WriteInt("ag", m_ag);
			if (m_ah > -1)  result &= rw->WriteInt("ah", m_ah);
			if (m_ai != 0)  result &= rw->WriteInt("ai", m_ai);
			if (m_aj != 0)  result &= rw->WriteInt("aj", m_aj);
			if (m_ak != 0)  result &= rw->WriteInt("ak", m_ak);

			return result;
		}

	public:
		int m_missionTypeId;
		int m_missionId;
		u64 m_matchId;
		u64 m_bossId;
		int m_isboss;
		int m_a;
		int m_b;
		int m_c;
		int m_d;
		int m_e;
		int m_f;
		int m_g;
		int m_h;
		int m_i;
		int m_j;
		int m_k;
		int m_l;
		int m_m;
		int m_n;
		int m_o;
		int m_p;
		int m_q;
		int m_r;
		int m_s;
		int m_t;
		int m_u;
		int m_v;
		bool m_w;
		int m_x;
		int m_y;
		bool m_z;
		int m_aa;
		u64 m_ab;
		int m_ac;
		int m_ad;
		int m_ae;
		int m_af;
		int m_ag;
		int m_ah;
		int m_ai;
		int m_aj;
		int m_ak;
	};

	void CommandPlaystatsNightclubMissionEnded(sNightclubMissionEnded* data)
	{
		scriptAssertf(data, "%s: PLAYSTATS_NIGHTCLUB_MISSION_ENDED - data is NULL.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!data)
			return;

		const u64 bid0 = ((u64)data->m_bossId1.Int) << 32;
		const u64 bid1 = (0x00000000FFFFFFFF & (u64)data->m_bossId2.Int);
		const u64 localbossId = StatsInterface::GetUInt64Stat(StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID"));

		MetricNIGHTCLUB_MISSION_ENDED m;
		m.m_missionTypeId = data->m_missionTypeId.Int;
		scriptAssertf(data->m_missionId.Int != 0, "%s: PLAYSTATS_MISSION_ENDED - MissionId is 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		m.m_missionId = data->m_missionId.Int;
		const u64 mid0 = ((u64)data->m_matchId1.Int) << 32;
		const u64 mid1 = ((u64)data->m_matchId2.Int);
		m.m_matchId = mid0 | mid1;
		m.m_bossId = bid0 | bid1;
		m.m_isboss = (m.m_bossId != 0 && m.m_bossId == localbossId);

		m.m_a = data->m_PlayerRole.Int;
		m.m_b = data->m_PlayerParticipated.Int;
		m.m_c = data->m_TimeStart.Int;
		m.m_d = data->m_TimeEnd.Int;
		m.m_e = data->m_Won.Int;
		m.m_f = data->m_EndingReason.Int;
		m.m_g = data->m_CashEarned.Int;
		m.m_h = data->m_RpEarned.Int;
		m.m_i = data->m_TargetsKilled.Int;
		m.m_j = data->m_InnocentsKilled.Int;
		m.m_k = data->m_Deaths.Int;
		m.m_l = data->m_LauncherRank.Int;
		m.m_m = data->m_TimeTakenToComplete.Int;
		m.m_n = data->m_PlayersLeftInProgress.Int;
		m.m_o = data->m_Location.Int;
		m.m_p = data->m_InvitesSent.Int;
		m.m_q = data->m_InvitesAccepted.Int;
		m.m_r = data->m_BossKilled.Int;
		m.m_s = data->m_GoonsKilled.Int;
		m.m_t = data->m_SuppliesStolen.Int;
		m.m_u = data->m_SuppliesOwned.Int;
		m.m_v = data->m_ProductsOwned.Int;
		m.m_w = data->m_ownnightclub.Int != 0;
		m.m_x = data->m_ownadditionalstaff.Int;
		m.m_y = data->m_ownadditionalsecurity.Int;
		m.m_z = data->m_ownhackertruck.Int != 0;
		m.m_aa = data->m_quickrestart.Int;
		m.m_ab = CNetworkTelemetry::GetCurMpSessionId();
		m.m_ac = data->m_bosstype.Int;
		m.m_ad = data->m_attacktype.Int;
		m.m_ae = data->m_collectiontype.Int;
		m.m_af = data->m_enemygroup.Int;
		m.m_ag = data->m_ambushvehicle.Int;
		m.m_ah = data->m_deliverylocation.Int;
		m.m_ai = data->m_properties.Int;
		m.m_aj = data->m_properties2.Int;
		m.m_ak = data->m_launchMethod.Int;

		APPEND_METRIC(m);
	}

	class MetricDJ_USAGE : public MetricPlayStat
	{
		RL_DECLARE_METRIC(DJ_USAGE, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		MetricDJ_USAGE(int djActive, int location) : MetricPlayStat()
			, m_djActive(djActive)
			, m_location(location)
		{		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);
			result &= rw->WriteInt("dj", m_djActive);
			result &= rw->WriteInt("l", m_location);
			return result;
		}

		int m_djActive;
		int m_location;
	};

	void CommandPlaystatsDjUsage(int djActive, int location = 0)
	{
		MetricDJ_USAGE m(djActive, location);
		APPEND_METRIC(m);
	}

	class MetricMINIGAME_USAGE : public MetricPlayStat
	{
		RL_DECLARE_METRIC(MINIGAME_USAGE, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		MetricMINIGAME_USAGE(int usage, int location, int duration) : MetricPlayStat()
			, m_usage(usage)
			, m_location(location)
			, m_duration(duration)
		{		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);
			result &= rw->WriteInt("u", m_usage);
			result &= rw->WriteInt("l", m_location);
			result &= rw->WriteInt("d", m_duration);
			return result;
		}

		int m_usage;
		int m_location;
		int m_duration;
	};

	void CommandPlaystatsMinigameUsage(int usage, int location = 0, int duration = 0)
	{
		MetricMINIGAME_USAGE m(usage, location, duration);
		APPEND_METRIC(m);
	}

	class MetricSTONE_HATCHET_END : public MetricPlayStat
	{
		RL_DECLARE_METRIC(STONE_HATCHET_END, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		MetricSTONE_HATCHET_END() : MetricPlayStat()
			
		{		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);
			result &= rw->WriteInt("a", m_timespent);
			result &= rw->WriteInt("b", m_result);
			result &= rw->WriteInt("c", m_cashearned);
			result &= rw->WriteInt("d", m_rpearned);
			result &= rw->WriteInt("e", m_Location);
			result &= rw->WriteInt("f", m_Target);
			result &= rw->WriteFloat("g", m_TargetX);
			result &= rw->WriteFloat("h", m_TargetY);
			result &= rw->WriteInt("i", m_TargetEvasionChoice);
			result &= rw->WriteFloat("j", m_damagedealt);
			result &= rw->WriteFloat("k", m_damagerecieved);
			result &= rw->WriteBool("l", m_capturedorkilled);
			result &= rw->WriteBool("m", m_chestfound);
			return result;
		}

		int m_timespent;
		int m_result;
		int m_cashearned;
		int m_rpearned;
		int m_Location;
		int m_Target;
		float m_TargetX;
		float m_TargetY;
		int m_TargetEvasionChoice;
		float m_damagedealt;
		float m_damagerecieved;
		bool m_capturedorkilled;
		bool m_chestfound;

	};

	void CommandPlaystatsStoneHatchetEnd(sStoneHatchetEnd* data)
	{
		scriptAssertf(data, "%s: PLAYSTATS_STONE_HATCHET_ENDED - data is NULL.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!data)
			return;

		MetricSTONE_HATCHET_END m;
		m.m_timespent = data->m_timespent.Int;
		m.m_result = data->m_result.Int;
		m.m_cashearned = data->m_cashearned.Int;
		m.m_rpearned = data->m_rpearned.Int;
		m.m_Location = data->m_Location.Int;
		m.m_Target = data->m_Target.Int;
		m.m_TargetX = data->m_TargetX.Float;
		m.m_TargetY = data->m_TargetY.Float;
		m.m_TargetEvasionChoice = data->m_TargetEvasionChoice.Int;
		m.m_damagedealt = data->m_damagedealt.Float;
		m.m_damagerecieved = data->m_damagerecieved.Float;
		m.m_capturedorkilled = data->m_capturedorkilled.Int != 0;
		m.m_chestfound = data->m_chestfound.Int != 0;

		APPEND_METRIC(m);
	}

	//////////////////////////////////////////////////////////////////////////
	// ARENA WARS

	class MetricSPECTATOR_WHEEL_SPIN : public MetricPlayStat
	{
		RL_DECLARE_METRIC(SPECTATOR_WHEEL_SPIN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		MetricSPECTATOR_WHEEL_SPIN(int contentId, int spintType, int rewardType, int rewardItem) : MetricPlayStat()
			, m_sessionid(0)
			, m_contentId(contentId)
			, m_spinType(spintType)
			, m_rewardType(rewardType)
			, m_rewardItem(rewardItem)
		{
			m_sessionid = CNetworkTelemetry::GetCurMpSessionId();
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);
			result &= rw->WriteInt64("s", m_sessionid);
			result &= rw->WriteInt("c", m_contentId);
			result &= rw->WriteInt("t", m_spinType);
			result &= rw->WriteInt("r", m_rewardType);
			result &= rw->WriteInt("i", m_rewardItem);
			return result;
		}

		u64 m_sessionid;
		int m_contentId;
		int m_spinType;
		int m_rewardType;
		int m_rewardItem;
	};

	void CommandPlaystatsSpinWheel(int contentId, int spintType, int rewardType, int rewardItem)
	{
		MetricSPECTATOR_WHEEL_SPIN m(contentId, spintType, rewardType, rewardItem);
		APPEND_METRIC(m);
	}

	class MetricARENA_WAR_SPECTATOR : public MetricPlayStat
	{
		RL_DECLARE_METRIC(ARENA_WAR_SPECTATOR, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		MetricARENA_WAR_SPECTATOR(int contentId, int spectatorMode, int weaponizedMode, int duration, int kills) : MetricPlayStat()
			, m_sessionid(0)
			, m_contentId(contentId)
			, m_spectatorMode(spectatorMode)
			, m_weaponizedMode(weaponizedMode)
			, m_duration(duration)
			, m_kills(kills)
		{
			m_sessionid = CNetworkTelemetry::GetCurMpSessionId();
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);
			result &= rw->WriteInt64("s", m_sessionid);
			result &= rw->WriteInt("c", m_contentId);
			result &= rw->WriteInt("m", m_spectatorMode);
			result &= rw->WriteInt("w", m_weaponizedMode);
			result &= rw->WriteInt("d", m_duration);
			result &= rw->WriteInt("k", m_kills);
			return result;
		}

		u64 m_sessionid;
		int m_contentId;
		int m_spectatorMode;
		int m_weaponizedMode;
		int m_duration;
		int m_kills;
	};

	void CommandPlaystatsArenaWarsSpectator(int contentId, int spectatorMode, int weaponizedMode, int duration, int kills)
	{
		MetricARENA_WAR_SPECTATOR m(contentId, spectatorMode, weaponizedMode, duration, kills);
		APPEND_METRIC(m);
	}


	class MetricARENA_WARS_ENDED : public MetricPlayStat
	{
		RL_DECLARE_METRIC(ARENA_WARS_ENDED, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		MetricARENA_WARS_ENDED()
			: MetricPlayStat()
			, m_sessionid(0)
			, m_publiccontentid(-1)
			, m_cashearned(-1)
			, m_launchmethod(-1)
			, m_leftinprogress(-1)
			, m_xp(-1)
			, m_kills(-1)
			, m_deaths(-1)
			, m_suicides(-1)
			, m_rank(-1)
			, m_vehicleid(-1)
			, m_matchduration(-1)
			, m_playingtime(-1)
			, m_result(-1)
			, m_premiumEvent(false)
			, m_skillLevel(-1)
			, m_endreason(-1)
			, m_killer(-1)
			, m_killervehicle(-1)
			, m_matchtype(-1)
			, m_matchname(-1)
			, m_checkpointsCollected(-1)
			, m_bonusTime(-1)
			, m_finishPosition(-1)
			, m_teamType(-1)
			, m_teamId(-1)
			, m_personalVehicle(false)
			, m_flagsStolen(-1)
			, m_flagsDelivered(-1)
			, m_totalPoints(-1)
			, m_goalsScored(-1)
			, m_suddenDeath(false)
			, m_winConditions(-1)
			, m_damageDealt(-1)
			, m_damageReceived(-1)
			, m_gladiatorKills(-1)
			, m_bombTime(-1)
			, m_spectatorTime(-1)
			, m_totalTagIns(-1)
			, m_timeEliminated(-1)
			, m_lobbyModLivery(-1)
			, m_lobbyModArmor(-1)
			, m_lobbyModWeapon(-1)
			, m_lobbyModMine(-1)
			, m_controlTurret(false)
		{
			m_sessionid = CNetworkTelemetry::GetCurMpSessionId();
		}

	virtual bool Write(RsonWriter* rw) const
	{
		bool result = MetricPlayStat::Write(rw);

		result &= rw->WriteInt64("sid", m_sessionid);

		result &= rw->WriteInt("b", m_publiccontentid);
		if (m_cashearned > -1) result &= rw->WriteInt("c", m_cashearned);
		if (m_launchmethod > -1) result &= rw->WriteInt("d", m_launchmethod);
		if (m_leftinprogress > -1) result &= rw->WriteInt("e", m_leftinprogress);
		if (m_xp > -1) result &= rw->WriteInt("f", m_xp);
		if (m_kills > -1) result &= rw->WriteInt("g", m_kills);
		if (m_deaths > -1) result &= rw->WriteInt("h", m_deaths);
		if (m_suicides > -1) result &= rw->WriteInt("i", m_suicides);
		if (m_rank > -1) result &= rw->WriteInt("j", m_rank);
		result &= rw->WriteInt("k", m_vehicleid);
		if (m_matchduration > -1) result &= rw->WriteInt("l", m_matchduration);
		if (m_playingtime > -1) result &= rw->WriteInt("m", m_playingtime);
		if (m_result > -1) result &= rw->WriteInt("n", m_result);
		result &= rw->WriteBool("o", m_premiumEvent);
		if (m_skillLevel > -1) result &= rw->WriteInt("p", m_skillLevel);
		result &= rw->WriteInt("q", m_endreason);
		if (m_killer > -1) result &= rw->WriteInt("r", m_killer);
		if (m_killervehicle > -1) result &= rw->WriteInt("s", m_killervehicle);
		if (m_matchtype > -1) result &= rw->WriteInt("t", m_matchtype);
		result &= rw->WriteInt("u", m_matchname);
		if (m_checkpointsCollected > -1) result &= rw->WriteInt("v", m_checkpointsCollected);
		if (m_bonusTime > -1) result &= rw->WriteInt("w", m_bonusTime);
		if (m_finishPosition > -1) result &= rw->WriteInt("x", m_finishPosition);
		if (m_teamType > -1) result &= rw->WriteInt("y", m_teamType);
		if (m_teamId > -1) result &= rw->WriteInt("z", m_teamId);
		result &= rw->WriteBool("aa", m_personalVehicle);
		if (m_flagsStolen > -1) result &= rw->WriteInt("ab", m_flagsStolen);
		if (m_flagsDelivered > -1) result &= rw->WriteInt("ac", m_flagsDelivered);
		if (m_totalPoints > -1) result &= rw->WriteInt("ad", m_totalPoints);
		if (m_goalsScored > -1) result &= rw->WriteInt("ae", m_goalsScored);
		result &= rw->WriteBool("af", m_suddenDeath);
		if (m_winConditions > -1) result &= rw->WriteInt("ag", m_winConditions);
		if (m_damageDealt > -1) result &= rw->WriteInt("ah", m_damageDealt);
		if (m_damageReceived > -1) result &= rw->WriteInt("ai", m_damageReceived);
		if (m_gladiatorKills > -1) result &= rw->WriteInt("aj", m_gladiatorKills);
		if (m_bombTime > -1) result &= rw->WriteInt("ak", m_bombTime);
		if (m_spectatorTime > -1) result &= rw->WriteInt("al", m_spectatorTime);
		if (m_totalTagIns > -1) result &= rw->WriteInt("am", m_totalTagIns);
		if (m_timeEliminated > -1) result &= rw->WriteInt("an", m_timeEliminated);
		result &= rw->WriteInt("ao", m_lobbyModLivery);
		result &= rw->WriteInt("ap", m_lobbyModArmor);
		result &= rw->WriteInt("aq", m_lobbyModWeapon);
		result &= rw->WriteInt("ar", m_lobbyModMine);
		result &= rw->WriteBool("as", m_controlTurret);
		
		return result;
	}

	public:
		u64 m_sessionid;
		int m_publiccontentid;
		int m_cashearned;
		int m_launchmethod;
		int m_leftinprogress;
		int m_xp;
		int m_kills;
		int m_deaths;
		int m_suicides;
		int m_rank;
		int m_vehicleid;
		int m_matchduration;
		int m_playingtime;
		int m_result;
		bool m_premiumEvent;
		int m_skillLevel;
		int m_endreason;
		int m_killer;
		int m_killervehicle;
		int m_matchtype;
		int m_matchname;
		int m_checkpointsCollected;
		int m_bonusTime;
		int m_finishPosition;
		int m_teamType;
		int m_teamId;
		bool m_personalVehicle;
		int m_flagsStolen;
		int m_flagsDelivered;
		int m_totalPoints;
		int m_goalsScored;
		bool m_suddenDeath;
		int m_winConditions;
		int m_damageDealt;
		int m_damageReceived;
		int m_gladiatorKills;
		int m_bombTime;
		int m_spectatorTime;
		int m_totalTagIns;
		int m_timeEliminated;
		int m_lobbyModLivery;
		int m_lobbyModArmor;
		int m_lobbyModWeapon;
		int m_lobbyModMine;
		bool m_controlTurret;
	};

	void CommandPlaystatsArenaWarsEnd(scrArenaWarsEnd* data)
	{
		scriptAssertf(data, "%s: PLAYSTATS_ARENA_WARS_ENDED - data is NULL.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!data)
			return;

		MetricARENA_WARS_ENDED m;
		m.m_publiccontentid = data->m_publiccontentid.Int;
		m.m_cashearned = data->m_cashearned.Int;
		m.m_launchmethod = data->m_launchmethod.Int;
		m.m_leftinprogress = data->m_leftinprogress.Int;
		m.m_xp = data->m_xp.Int;
		m.m_kills = data->m_kills.Int;
		m.m_deaths = data->m_deaths.Int;
		m.m_suicides = data->m_suicides.Int;
		m.m_rank = data->m_rank.Int;
		m.m_vehicleid = data->m_vehicleid.Int;
		m.m_matchduration = data->m_matchduration.Int;
		m.m_playingtime = data->m_playingtime.Int;
		m.m_result = data->m_result.Int;
		m.m_premiumEvent = data->m_premiumEvent.Int == 1;
		m.m_skillLevel = data->m_skillLevel.Int;
		m.m_endreason = data->m_endreason.Int;
		m.m_killer = data->m_killer.Int;
		m.m_killervehicle = data->m_killervehicle.Int;
		m.m_matchtype = data->m_matchtype.Int;
		m.m_matchname = data->m_matchname.Int;
		m.m_checkpointsCollected = data->m_checkpointsCollected.Int;
		m.m_bonusTime = data->m_bonusTime.Int;
		m.m_finishPosition = data->m_finishPosition.Int;
		m.m_teamType = data->m_teamType.Int;
		m.m_teamId = data->m_teamId.Int;
		m.m_personalVehicle = data->m_personalVehicle.Int == 1;
		m.m_flagsStolen = data->m_flagsStolen.Int;
		m.m_flagsDelivered = data->m_flagsDelivered.Int;
		m.m_totalPoints = data->m_totalPoints.Int;
		m.m_goalsScored = data->m_goalsScored.Int;
		m.m_suddenDeath = data->m_suddenDeath.Int == 1;
		m.m_winConditions = data->m_winConditions.Int;
		m.m_damageDealt = data->m_damageDealt.Int;
		m.m_damageReceived = data->m_damageReceived.Int;
		m.m_gladiatorKills = data->m_gladiatorKills.Int;
		m.m_bombTime = data->m_bombTime.Int;
		m.m_spectatorTime = data->m_spectatorTime.Int;
		m.m_totalTagIns = data->m_totalTagIns.Int;
		m.m_timeEliminated = data->m_timeEliminated.Int;
		m.m_lobbyModLivery = data->m_lobbyModLivery.Int;
		m.m_lobbyModArmor = data->m_lobbyModArmor.Int;
		m.m_lobbyModWeapon = data->m_lobbyModWeapon.Int;
		m.m_controlTurret = data->m_controlTurret.Int == 1;

		APPEND_METRIC_FLUSH(m, true);

		CNetworkTelemetry::SessionMatchEnd();
	}

	class MetricPASSIVE_MODE : public MetricPlayerCoords
	{
		RL_DECLARE_METRIC(PASSIVE_MODE, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		MetricPASSIVE_MODE(bool value, int source, int ghostedTo, int endReason)
			: MetricPlayerCoords()
			, m_choice(value)
			, m_source(source)
			, m_ghostedTo(ghostedTo)
			, m_endReason(endReason)
		{

		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayerCoords::Write(rw);
			result &= rw->WriteBool("c", m_choice);
			result &= rw->WriteInt("s", m_source);
			result &= rw->WriteInt("gt", m_ghostedTo);
			result &= rw->WriteInt("r", m_endReason);
			return result;
		}

	public:

		bool m_choice;
		int m_source;
		int m_ghostedTo;
		int m_endReason;
	};

	void CommandPlaystatsSwitchPassiveMode(bool value, int source, int ghostedTo, int endReason)
	{
		MetricPASSIVE_MODE m(value, source, ghostedTo, endReason);
		APPEND_METRIC(m);
	}

	//////////////////////////////////////////////////////////////////////////

	class MetricCOLLECTIBLE : public MetricPlayerCoords
	{
		RL_DECLARE_METRIC(COLLECTIBLE, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		MetricCOLLECTIBLE(int collectionType, int pickedUp, int pickedSoFar, int total, int awardCash, int awardRP, int awardChips, int awardItems, int location, int animal, int awardMedal)
			: MetricPlayerCoords()
			, m_collectionType(collectionType)
			, m_pickedUp(pickedUp)
			, m_pickedSoFar(pickedSoFar)
			, m_total(total)
			, m_awardCash(awardCash)
			, m_awardRP(awardRP)
			, m_awardChips(awardChips)
			, m_awardItems(awardItems)
			, m_location(location)
			, m_animal(animal)
			, m_awardMedal(awardMedal)
		{
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayerCoords::Write(rw);
			result &= rw->WriteInt("a", m_collectionType);
			result &= rw->WriteInt("b", m_pickedUp);
			result &= rw->WriteInt("c", m_pickedSoFar);
			result &= rw->WriteInt("d", m_total);
			result &= rw->WriteInt("e", m_awardCash);
			result &= rw->WriteInt("f", m_awardRP);
			result &= rw->WriteInt("g", m_awardChips);
			result &= rw->WriteInt("h", m_awardItems);
			result &= rw->WriteInt("i", m_location);
			result &= rw->WriteInt("j", m_animal);
			result &= rw->WriteInt("k", m_awardMedal);
			return result;
		}

	public:

		int m_collectionType;
		int m_pickedUp;
		int m_pickedSoFar;
		int m_total;
		int m_awardCash;
		int m_awardRP;
		int m_awardChips;
		int m_awardItems;
		int m_location;
		int m_animal;
		int m_awardMedal;
	};

	void CommandPlaystatsCollectiblePickedUp(int collectionType, int pickedUp, int pickedSoFar, int total, int awardCash, int awardRP, int awardChips, int awardItems, int location, int animal, int awardMedal)
	{
		MetricCOLLECTIBLE m(collectionType, pickedUp, pickedSoFar, total, awardCash, awardRP, awardChips, awardItems, location, animal, awardMedal);
		APPEND_METRIC(m);
	}

	////////////////////////////////////////////////////////////////////////// CASINO HEIST TELEMETRY

	class MetricARCADE_LOVEMATCH : public MetricPlayerCoords
	{
		RL_DECLARE_METRIC(ARC_LM, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		MetricARCADE_LOVEMATCH(const rlGamerHandle& hGamer, scrArcadeLoveMatch* data)
			: MetricPlayerCoords()
			, m_matchId(0)
			, m_score(0)
			, m_flying(0)
			, m_stamina(0)
			, m_shooting(0)
			, m_driving(0)
			, m_stealth(0)
			, m_maxHealth(0)
			, m_trueLove(0)
			, m_nemesis(0)
		{
			if (hGamer.IsValid())
			{
				hGamer.ToUserId(m_player);
				hGamer.ToString(m_account, COUNTOF(m_account));
				CNetGamePlayer* pPlayer = NetworkInterface::GetPlayerFromGamerHandle(hGamer);
				if (pPlayer)
				{
					m_playerAccountId = pPlayer->GetPlayerAccountId();
				}

			}
			else
			{
				m_player[0] = '\0';
				m_account[0] = '\0';
			}

			const u64 mid0 = ((u64)data->matchId1.Int) << 32;
			const u64 mid1 = ((u64)data->matchId2.Int);
			m_matchId = mid0 | mid1;
			m_score = data->score.Int;
			m_flying = data->flying.Int;
			m_stamina = data->stamina.Int;
			m_shooting = data->shooting.Int;
			m_driving = data->driving.Int;
			m_stealth = data->stealth.Int;
			m_maxHealth = data->maxHealth.Int;
			m_trueLove = data->trueLove.Int;
			m_nemesis = data->nemesis.Int;
			m_sexActs = StatsInterface::GetIntStat(STAT_PROSTITUTES_FREQUENTED.GetHash());
			m_lapDances = StatsInterface::GetIntStat(STAT_LAP_DANCED_BOUGHT.GetHash());
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayerCoords::Write(rw);
			result &= rw->WriteInt64("matchId", m_matchId);
			result &= rw->WriteString("p", m_player);
			result &= rw->WriteString("a", m_account);
			result &= rw->WriteInt("i", m_playerAccountId);
			result &= rw->WriteInt("score", m_score);
			result &= rw->BeginArray("s", NULL);
			{
				result = result && rw->WriteInt(NULL, m_flying);
				result = result && rw->WriteInt(NULL, m_stamina);
				result = result && rw->WriteInt(NULL, m_shooting);
				result = result && rw->WriteInt(NULL, m_driving);
				result = result && rw->WriteInt(NULL, m_stealth);
				result = result && rw->WriteInt(NULL, m_maxHealth);
				result = result && rw->WriteInt(NULL, m_lapDances);
				result = result && rw->WriteInt(NULL, m_sexActs);
				result = result && rw->WriteInt(NULL, m_trueLove);
				result = result && rw->WriteInt(NULL, m_nemesis);
			}
			result &= rw->End();
			return result;
		}

	public:

		u64 m_matchId;
		int m_score;
		char m_player[RL_MAX_USERID_BUF_LENGTH];
		char m_account[RL_MAX_GAMER_HANDLE_CHARS];
		s32 m_playerAccountId;
		// stats
		int m_flying;
		int m_stamina;
		int m_shooting;
		int m_driving;
		int m_stealth;
		int m_maxHealth;
		int m_trueLove;
		int m_nemesis;
		int m_lapDances;
		int m_sexActs;
	};

	void CommandPlaystatsArcadeLoveMatch(int& hGamerHandleData, scrArcadeLoveMatch* data)
	{
		rlGamerHandle gamerhandle;
		if (CTheScripts::ImportGamerHandle(&gamerhandle, hGamerHandleData, SCRIPT_GAMER_HANDLE_SIZE  ASSERT_ONLY(, "PLAYSTATS_ARCADE_LOVE_MATCH")))
		{
			MetricARCADE_LOVEMATCH m(gamerhandle, data);
			APPEND_METRIC(m);
		}
	}

	////////////////////////////////////////////////////////////////////////// CASINO METRICS

    class MetricCASINO_STORY_MISSION_ENDED : public MetricPlayStat
    {
        RL_DECLARE_METRIC(CASINO_STORY_MISSION_ENDED, TELEMETRY_CHANNEL_MISSION, LOGLEVEL_VERYHIGH_PRIORITY);

    public:
        MetricCASINO_STORY_MISSION_ENDED(sCasinoStoryMissionEndedMetric* data, const char* contentId) : MetricPlayStat()
        {
            m_SessionId = CNetworkTelemetry::GetCurMpSessionId();
            m_GameMode = CNetworkTelemetry::GetCurMpGameMode();
            m_PublicSlots = CNetworkTelemetry::GetCurMpNumPubSlots();
            m_PrivateSlots = CNetworkTelemetry::GetCurMpNumPrivSlots();
            m_MatchCreator = data->m_MatchCreator.Int;
			safecpy(m_PublicContentId, contentId);
            m_SessionType = data->m_SessionType.Int;
            m_MatchmakingKey = NetworkBaseConfig::GetMatchmakingUser(false);
			m_ExecutableSize = (s64)NetworkInterface::GetExeSize();
			const CNetworkSession::sUniqueCrewData& crewData = CNetwork::GetNetworkSession().GetMainSessionCrewData();
			m_CrewLimit = crewData.m_nLimit;
            m_PlaylistId = data->m_PlaylistId.Int;
            m_Kills = data->m_Kills.Int;
            m_Deaths = data->m_Deaths.Int;
            m_Cash = data->m_Cash.Int;
            m_CashStart = data->m_CashStart.Int;
            m_CashEnd = data->m_CashEnd.Int;
            m_MissionLaunch = data->m_MissionLaunch.Int;
            m_Difficulty = data->m_Difficulty.Int;
            m_FirstPerson = data->m_FirstPerson.Int;
            m_Medal = data->m_Medal.Int;
            m_TeamLivesUsed = data->m_TeamLivesUsed.Int;
            m_FailureReason = data->m_FailureReason.Int;
            m_UsedQuickRestart = data->m_UsedQuickRestart.Int;
            m_IsHost = data->m_IsHost.Int != 0;
            m_IsSessionVisible = data->m_IsSessionVisible.Int != 0;
            m_LeftInProgress = data->m_LeftInProgress.Int != 0;
            m_SpookedCops = data->m_SpookedCops.Int != 0;
            m_PlayingTime = data->m_PlayingTime.Int;
            m_XP = data->m_XP.Int;
            m_Suicides = data->m_Suicides.Int;
            m_Rank = data->m_Rank.Int;
            m_CrewId = data->m_CrewId.Int;
            m_Result = data->m_Result.Int;
            m_JobPoints = data->m_JobPoints.Int;
            m_UsedVoiceChat = CNetworkTelemetry::HasUsedVoiceChatSinceLastTime();
            m_HeistSessionId = data->m_HeistSessionId.Int;
            m_ClosedJob = data->m_ClosedJob.Int != 0;
            m_PrivateJob = data->m_PrivateJob.Int != 0;
            m_FromClosedFreemode = data->m_FromClosedFreemode.Int != 0;
            m_FromPrivateFreemode = data->m_FromPrivateFreemode.Int != 0;
			const u64 bid0 = ((u64)data->m_BossUUID.Int) << 32;
			const u64 bid1 = (0x00000000FFFFFFFF & (u64)data->m_BossUUID2.Int);
			m_BossUUID = bid0 | bid1;
            m_BossType = data->m_BossType.Int;
            m_FailedStealth = data->m_FailedStealth.Int;
            m_MissionID = data->m_MissionID.Int;
            m_MissionVariation = data->m_MissionVariation.Int;
            m_OwnPenthouse = data->m_OwnPenthouse.Int != 0;
            m_OwnGarage = data->m_OwnGarage.Int != 0;
            m_OwnOffice = data->m_OwnOffice.Int != 0;
            m_HouseChipsEarned = data->m_HouseChipsEarned.Int;
			m_RestartPoint = data->m_RestartPoint.Int;
			m_LaunchedByPlayer = data->m_LaunchedByPlayer.Int != 0;
        }

        virtual bool Write(RsonWriter* rw) const
        {
            bool result = MetricPlayStat::Write(rw);
            if (result)
            {
                result &= rw->WriteInt64("a", m_SessionId);
                result &= rw->WriteInt("b", m_GameMode);
                result &= rw->WriteInt("c", m_PublicSlots);
                result &= rw->WriteInt("d", m_PrivateSlots);
                result &= rw->WriteInt("e", m_MatchCreator);
                result &= rw->WriteString("f", m_PublicContentId);
                result &= rw->WriteInt("g", m_SessionType);
                result &= rw->WriteUns("h", m_MatchmakingKey);
                result &= rw->WriteInt64("i", m_ExecutableSize);
                result &= rw->WriteInt("j", m_CrewLimit);
                result &= rw->WriteInt("k", m_PlaylistId);
                result &= rw->WriteInt("l", m_Kills);
                result &= rw->WriteInt("m", m_Deaths);
                result &= rw->WriteInt("n", m_Cash);
                result &= rw->WriteInt("o", m_CashStart);
                result &= rw->WriteInt("p", m_CashEnd);
                result &= rw->WriteInt("q", m_MissionLaunch);
                result &= rw->WriteInt("r", m_Difficulty);
                result &= rw->WriteInt("s", m_FirstPerson);
                result &= rw->WriteInt("t", m_Medal);
                result &= rw->WriteInt("u", m_TeamLivesUsed);
                result &= rw->WriteInt("v", m_FailureReason);
                result &= rw->WriteInt("w", m_UsedQuickRestart);
                result &= rw->WriteBool("x", m_IsHost);
                result &= rw->WriteBool("y", m_IsSessionVisible);
                result &= rw->WriteBool("z", m_LeftInProgress);
                result &= rw->WriteBool("aa", m_SpookedCops);
                result &= rw->WriteInt("ab", m_PlayingTime);
                result &= rw->WriteInt("ac", m_XP);
                result &= rw->WriteInt("ad", m_Suicides);
                result &= rw->WriteInt("ae", m_Rank);
                result &= rw->WriteInt("af", m_CrewId);
                result &= rw->WriteInt("ag", m_Result);
                result &= rw->WriteInt("ah", m_JobPoints);
                result &= rw->WriteBool("ai", m_UsedVoiceChat);
                result &= rw->WriteInt("aj", m_HeistSessionId);
                result &= rw->WriteBool("ak", m_ClosedJob);
                result &= rw->WriteBool("al", m_PrivateJob);
                result &= rw->WriteBool("am", m_FromClosedFreemode);
                result &= rw->WriteBool("an", m_FromPrivateFreemode);
                result &= rw->WriteInt64("ao", m_BossUUID);
                result &= rw->WriteInt("ap", m_BossType);
                result &= rw->WriteInt("aq", m_FailedStealth);
                result &= rw->WriteInt("ar", m_MissionID);
                result &= rw->WriteInt("as", m_MissionVariation);
                result &= rw->WriteBool("at", m_OwnPenthouse);
                result &= rw->WriteBool("au", m_OwnGarage);
                result &= rw->WriteBool("av", m_OwnOffice);
                result &= rw->WriteInt("aw", m_HouseChipsEarned);
				result &= rw->WriteInt("ax", m_RestartPoint);
				result &= rw->WriteBool("ay", m_LaunchedByPlayer);
            }
            return result;
        }

        u64 m_SessionId;
        int m_GameMode;
        int m_PublicSlots;
        int m_PrivateSlots;
        int m_MatchCreator;
        char m_PublicContentId[32];
        int m_SessionType;
        u32 m_MatchmakingKey;
        s64 m_ExecutableSize;
        int m_CrewLimit;
        int m_PlaylistId;
        int m_Kills;
        int m_Deaths;
        int m_Cash;
        int m_CashStart;
        int m_CashEnd;
        int m_MissionLaunch;
        int m_Difficulty;
        int m_FirstPerson;
        int m_Medal;
        int m_TeamLivesUsed;
        int m_FailureReason;
        int m_UsedQuickRestart;
        bool m_IsHost;
        bool m_IsSessionVisible;
        bool m_LeftInProgress;
        bool m_SpookedCops;
        int m_PlayingTime;
        int m_XP;
        int m_Suicides;
        int m_Rank;
        int m_CrewId;
        int m_Result;
        int m_JobPoints;
        bool m_UsedVoiceChat;
        int m_HeistSessionId;
        bool m_ClosedJob;
        bool m_PrivateJob;
        bool m_FromClosedFreemode;
        bool m_FromPrivateFreemode;
        u64 m_BossUUID;
        int m_BossType;
        int m_FailedStealth;
        int m_MissionID;
        int m_MissionVariation;
        bool m_OwnPenthouse;
        bool m_OwnGarage;
        bool m_OwnOffice;
        int m_HouseChipsEarned;
        int m_RestartPoint;
		bool m_LaunchedByPlayer;
    };

    void CommandPlaystatsCasinoStoryMissionEnded(sCasinoStoryMissionEndedMetric* data, const char* contentId)
    {
        MetricCASINO_STORY_MISSION_ENDED m(data, contentId);
		APPEND_METRIC(m);
		CNetworkTelemetry::SessionMatchEnd();
    }

	class MetricCASINO : public MetricPlayStat
	{
		RL_DECLARE_METRIC(CASINO, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		MetricCASINO(sCasinoMetric* data) : MetricPlayStat()
		{
			m_gameType = data->m_gameType.Int;
			m_matchType = data->m_matchType.Int;
			m_tableID = data->m_tableID.Int;
			m_handID = data->m_handID.Int;
			m_endReason = data->m_endReason.Int;
			m_allIn = data->m_allIn.Int != 0;
			m_chipsDelta = data->m_chipsDelta.Int;
			m_finalChipBalance = data->m_finalChipBalance.Int;
			m_totalPot = data->m_totalPot.Int;
			m_playersAtTable = data->m_playersAtTable.Int;
			m_viewedLegalScreen = data->m_viewedLegalScreen.Int != 0;
			m_ownPenthouse = data->m_ownPenthouse.Int != 0;
			m_betAmount1 = data->m_betAmount1.Int;
			m_betAmount2 = data->m_betAmount2.Int;
			m_winAmount = data->m_winAmount.Int;
			m_win = data->m_win.Int != 0;
			m_cheat = data->m_cheat.Int != 0;
			m_timePlayed = data->m_timePlayed.Int;
			m_isHost = data->m_isHost.Int != 0;
			m_hostID = data->m_hostID.Int;
			m_membership = data->m_membership.Int;
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);

			result &= rw->WriteInt("a", m_gameType);
			result &= rw->WriteInt("b", m_matchType);
			result &= rw->WriteInt("c", m_tableID);
			result &= rw->WriteInt("d", m_handID);
			result &= rw->WriteInt("e", m_endReason);
			result &= rw->WriteBool("f", m_allIn);
			result &= rw->WriteInt("g", m_chipsDelta);
			result &= rw->WriteInt("h", m_finalChipBalance);
			result &= rw->WriteInt("i", m_totalPot);
			result &= rw->WriteInt("j", m_playersAtTable);
			result &= rw->WriteBool("k", m_viewedLegalScreen);
			result &= rw->WriteBool("l", m_ownPenthouse);
			result &= rw->WriteInt("m", m_betAmount1);
			result &= rw->WriteInt("n", m_betAmount2);
			result &= rw->WriteInt("o", m_winAmount);
			result &= rw->WriteBool("p", m_win);
			result &= rw->WriteBool("q", m_cheat);
			result &= rw->WriteInt("r", m_timePlayed);
			result &= rw->WriteBool("s", m_isHost);
			result &= rw->WriteInt("t", m_hostID);
			result &= rw->WriteInt("mem", m_membership);
			return result;
		}

		int m_gameType;
		int m_matchType;
		int m_tableID;
		int m_handID;
		int m_endReason;
		bool m_allIn;
		int m_chipsDelta;
		int m_finalChipBalance;
		int m_totalPot;
		int m_playersAtTable;
		bool m_viewedLegalScreen;
		bool m_ownPenthouse;
		int m_betAmount1;
		int m_betAmount2;
		int m_winAmount;
		bool m_win;
		bool m_cheat;
		int m_timePlayed;
		bool m_isHost;
		int m_hostID;
		int m_membership;
	};

    class MetricCASINO_CHIP : public MetricPlayStat
    {
        RL_DECLARE_METRIC(CASINO_CHIP, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

    public:
        MetricCASINO_CHIP(sCasinoChipMetric* data) : MetricPlayStat()
        {
            m_action = data->m_action.Int;
            m_sessionID = CNetworkTelemetry::GetCurMpSessionId();
            m_transactionID = data->m_transactionID.Int;
            m_cashAmount = data->m_cashAmount.Int;
            m_chipsAmount = data->m_chipsAmount.Int;
            m_reason = data->m_reason.Int;
            m_cashBalance = data->m_cashBalance.Int;
            m_chipBalance = data->m_chipBalance.Int;
			m_item = data->m_item.Int;
        }

        virtual bool Write(RsonWriter* rw) const
        {
            bool result = MetricPlayStat::Write(rw);
            if (result)
            {
                result &= rw->WriteInt("a", m_action);
                result &= rw->WriteInt64("b", m_sessionID);
                result &= rw->WriteInt("c", m_transactionID);
                result &= rw->WriteInt("d", m_cashAmount);
                result &= rw->WriteInt("e", m_chipsAmount);
                result &= rw->WriteInt("f", m_reason);
                result &= rw->WriteInt("g", m_cashBalance);
                result &= rw->WriteInt("h", m_chipBalance);
				result &= rw->WriteInt("i", m_item);
            }
            return result;
        }

        int m_action;
        u64 m_sessionID;
        int m_transactionID;
        int m_cashAmount;
        int m_chipsAmount;
        int m_reason;
        int m_cashBalance;
        int m_chipBalance;
		int m_item;
    };

    void CommandPlaystatsCasinoChip(sCasinoChipMetric* data)
    {
        MetricCASINO_CHIP m(data);
        APPEND_METRIC(m);
    }

	class MetricROULETTE : public MetricCASINO
	{
		RL_DECLARE_METRIC(ROULETTE, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		MetricROULETTE(sRouletteMetric* data) : MetricCASINO(&data->m_casinoMetric)
		{
			m_iBetStraight = data->m_iBetStraight.Int == 1;
			m_iBetSplit = data->m_iBetSplit.Int == 1;
			m_iBetStreet = data->m_iBetStreet.Int == 1;
			m_iBetCorner = data->m_iBetCorner.Int == 1;
			m_iBetFiveNumber = data->m_iBetFiveNumber.Int == 1;
			m_iBetSixLine = data->m_iBetSixLine.Int == 1;
			m_oBetRed = data->m_oBetRed.Int == 1;
			m_oBetBlack = data->m_oBetBlack.Int == 1;
			m_oBetOddNumber = data->m_oBetOddNumber.Int == 1;
			m_oBetEvenNumber = data->m_oBetEvenNumber.Int == 1;
			m_oBetLowBracket = data->m_oBetLowBracket.Int == 1;
			m_oBetHighBracket = data->m_oBetHighBracket.Int == 1;
			m_oBetDozen = data->m_oBetDozen.Int == 1;
			m_oBetColumn = data->m_oBetColumn.Int == 1;
			m_winningNumber = data->m_winningNumber.Int;
			m_winningColour = data->m_winningColour.Int;
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricCASINO::Write(rw);

			result &= rw->WriteBool("u", m_iBetStraight);
			result &= rw->WriteBool("v", m_iBetSplit);
			result &= rw->WriteBool("w", m_iBetStreet);
			result &= rw->WriteBool("x", m_iBetCorner);
			result &= rw->WriteBool("y", m_iBetFiveNumber);
			result &= rw->WriteBool("z", m_iBetSixLine);
			result &= rw->WriteBool("aa", m_oBetRed);
			result &= rw->WriteBool("ab", m_oBetBlack);
			result &= rw->WriteBool("ac", m_oBetOddNumber);
			result &= rw->WriteBool("ad", m_oBetEvenNumber);
			result &= rw->WriteBool("ae", m_oBetLowBracket);
			result &= rw->WriteBool("af", m_oBetHighBracket);
			result &= rw->WriteBool("ag", m_oBetDozen);
			result &= rw->WriteBool("ah", m_oBetColumn);
			result &= rw->WriteInt("ai", m_winningNumber);
			result &= rw->WriteInt("aj", m_winningColour);
			return result;
		}

		bool m_iBetStraight;
		bool m_iBetSplit;
		bool m_iBetStreet;
		bool m_iBetCorner;
		bool m_iBetFiveNumber;
		bool m_iBetSixLine;
		bool m_oBetRed;
		bool m_oBetBlack;
		bool m_oBetOddNumber;
		bool m_oBetEvenNumber;
		bool m_oBetLowBracket;
		bool m_oBetHighBracket;
		bool m_oBetDozen;
		bool m_oBetColumn;
		int m_winningNumber;
		int m_winningColour;
	};

	void CommandPlaystatsCasinoRoulette(sRouletteMetric* data)
	{
		MetricROULETTE m(data);
		APPEND_METRIC(m);
	}

	class MetricBLACKJACK : public MetricCASINO
	{
		RL_DECLARE_METRIC(BLACKJACK, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		MetricBLACKJACK(sBlackjackMetric* data) : MetricCASINO(&data->m_casinoMetric)
		{
			m_stand = data->m_stand.Int == 1;
			m_hit = data->m_hit.Int == 1;
			m_double = data->m_double.Int == 1;
			m_split = data->m_split.Int == 1;
			m_outcome = data->m_outcome.Int;
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricCASINO::Write(rw);

			result &= rw->WriteBool("u", m_stand);
			result &= rw->WriteBool("v", m_hit);
			result &= rw->WriteBool("w", m_double);
			result &= rw->WriteBool("x", m_split);
			result &= rw->WriteInt("y", m_outcome);
			return result;
		}

		bool m_stand;
		bool m_hit;
		bool m_double;
		bool m_split;
		int m_outcome;
	};

	void CommandPlaystatsCasinoBlackjack(sBlackjackMetric* data)
	{
		MetricBLACKJACK m(data);
		APPEND_METRIC(m);
	}

	class MetricTHREECARDPOKER : public MetricCASINO
	{
		RL_DECLARE_METRIC(THREECARDPOKER, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		MetricTHREECARDPOKER(sThreeCardPokerMetric* data) : MetricCASINO(&data->m_casinoMetric)
		{
			m_pairPlus = data->m_pairPlus.Int == 1;
			m_outcome = data->m_outcome.Int;
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricCASINO::Write(rw);

			result &= rw->WriteBool("u", m_pairPlus);
			result &= rw->WriteInt("v", m_outcome);
			return result;
		}

		bool m_pairPlus;
		int m_outcome;
	};

	void CommandPlaystatsCasinoThreeCardPoker(sThreeCardPokerMetric* data)
	{
		MetricTHREECARDPOKER m(data);
		APPEND_METRIC(m);
	}

	class MetricSLOTMACHINE : public MetricCASINO
	{
		RL_DECLARE_METRIC(SLOTMACHINE, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		MetricSLOTMACHINE(sSlotMachineMetric* data) : MetricCASINO(&data->m_casinoMetric)
		{
			m_machineStyle = data->m_machineStyle.Int;
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricCASINO::Write(rw);
			result &= rw->WriteInt("u", m_machineStyle);
			return result;
		}

		int m_machineStyle;
	};

	void CommandPlaystatsCasinoSlotMachine(sSlotMachineMetric* data)
	{
		MetricSLOTMACHINE m(data);
		APPEND_METRIC(m);
	}

	class MetricINSIDETRACK : public MetricCASINO
	{
		RL_DECLARE_METRIC(INSIDETRACK, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		MetricINSIDETRACK(sInsideTrackMetric* data) : MetricCASINO(&data->m_casinoMetric)
		{
			m_gameChoice = data->m_gameChoice.Int;
			m_horseChoice = data->m_horseChoice.Int;
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricCASINO::Write(rw);
			result &= rw->WriteInt("u", m_gameChoice);
			result &= rw->WriteInt("v", m_horseChoice);
			return result;
		}

		int m_gameChoice;
		int m_horseChoice;
	};

	void CommandPlaystatsCasinoInsideTrack(sInsideTrackMetric* data)
	{
		MetricINSIDETRACK m(data);
		APPEND_METRIC(m);
	}

	class MetricLUCKYSEVEN : public MetricCASINO
	{
		RL_DECLARE_METRIC(LUCKYSEVEN, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		MetricLUCKYSEVEN(sLuckySevenMetric* data) : MetricCASINO(&data->m_casinoMetric)
		{
			m_rewardType = data->m_rewardType.Int;
			m_rewardItem = data->m_rewardItem.Int;
			m_rewardAmount = data->m_rewardAmount.Int;
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricCASINO::Write(rw);
			result &= rw->WriteInt("u", m_rewardType);
			result &= rw->WriteInt("v", m_rewardItem);
			result &= rw->WriteInt("w", m_rewardAmount);
			return result;
		}

		int m_rewardType;
		int m_rewardItem;
		int m_rewardAmount;
	};

	void CommandPlaystatsCasinoLuckySeven(sLuckySevenMetric* data)
	{
		MetricLUCKYSEVEN m(data);
		APPEND_METRIC(m);
	}

	class MetricCASINOLIGHT : public MetricPlayStat
	{
		RL_DECLARE_METRIC(CASINOLIGHT, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		MetricCASINOLIGHT(sCasinoMetricLight* data) : MetricPlayStat()
		{
			m_matchType = data->m_matchType.Int;
			m_tableID = data->m_tableID.Int;
			m_endReason = data->m_endReason.Int;
			m_chipsDelta = data->m_chipsDelta.Int;
			m_finalChipBalance = data->m_finalChipBalance.Int;
			m_duration = data->m_duration.Int;
			m_viewedLegalScreen = data->m_viewedLegalScreen.Int != 0;
			m_betAmount1 = data->m_betAmount1.Int;
			m_betAmount2 = data->m_betAmount2.Int;
			m_cheatCount = data->m_cheatCount.Int;
			m_isHost = data->m_isHost.Int != 0;
			m_hostID = data->m_hostID.Int;
			m_handsPlayed = data->m_handsPlayed.Int;
			m_winCount = data->m_winCount.Int;
			m_loseCount = data->m_loseCount.Int;
			m_membership = data->m_membership.Int;
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);
			result &= rw->WriteInt("a", m_matchType);
			result &= rw->WriteInt("b", m_tableID);
			result &= rw->WriteInt("c", m_endReason);
			result &= rw->WriteInt("d", m_chipsDelta);
			result &= rw->WriteInt("e", m_finalChipBalance);
			result &= rw->WriteInt("f", m_duration);
			result &= rw->WriteBool("g", m_viewedLegalScreen);
			result &= rw->WriteInt("h", m_betAmount1);
			result &= rw->WriteInt("i", m_betAmount2);
			result &= rw->WriteInt("j", m_cheatCount);
			result &= rw->WriteBool("k", m_isHost);
			result &= rw->WriteInt("l", m_hostID);
			result &= rw->WriteInt("m", m_handsPlayed);
			result &= rw->WriteInt("n", m_winCount);
			result &= rw->WriteInt("o", m_loseCount);
			result &= rw->WriteInt("mem", m_membership);
			return result;
		}

		int m_matchType;
		int m_tableID;
		int m_endReason;
		int m_chipsDelta;
		int m_finalChipBalance;
		int m_duration;
		bool m_viewedLegalScreen;
		int m_betAmount1;
		int m_betAmount2;
		int m_cheatCount;
		bool m_isHost;
		int m_hostID;
		int m_handsPlayed;
		int m_winCount;
		int m_loseCount;
		int m_membership;
	};

	class MetricROULETTE_L : public MetricCASINOLIGHT
	{
		RL_DECLARE_METRIC(ROULETTE_L, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		MetricROULETTE_L(sCasinoMetricLight* data) : MetricCASINOLIGHT(data)
		{

		}
	};

	void CommandPlaystatsCasinoRouletteLight(sCasinoMetricLight* data)
	{
		MetricROULETTE_L m(data);
		APPEND_METRIC(m);
	}

	class MetricBLACKJACK_L : public MetricCASINOLIGHT
	{
		RL_DECLARE_METRIC(BLACKJACK_L, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		MetricBLACKJACK_L(sBlackjackMetricLight* data) : MetricCASINOLIGHT(&data->m_casinoMetric)
		{
			m_dealerBlackjackCount = data->m_dealerBlackjackCount.Int;
			m_playerBlackjackCount = data->m_playerBlackjackCount.Int;
			m_surrenderCount = data->m_surrenderCount.Int;
			m_bustCount = data->m_bustCount.Int;
			m_7CardCharlieCount = data->m_7CardCharlieCount.Int;
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricCASINOLIGHT::Write(rw);
			result &= rw->WriteInt("p", m_dealerBlackjackCount);
			result &= rw->WriteInt("q", m_playerBlackjackCount);
			result &= rw->WriteInt("r", m_surrenderCount);
			result &= rw->WriteInt("s", m_bustCount);
			result &= rw->WriteInt("t", m_7CardCharlieCount);
			return result;
		}

		int m_dealerBlackjackCount;
		int m_playerBlackjackCount;
		int m_surrenderCount;
		int m_bustCount;
		int m_7CardCharlieCount;

	};

	void CommandPlaystatsCasinoBlackjackLight(sBlackjackMetricLight* data)
	{
		MetricBLACKJACK_L m(data);
		APPEND_METRIC(m);
	}

	class MetricTHREECARDPOKER_L : public MetricCASINOLIGHT
	{
		RL_DECLARE_METRIC(THREECARDPOKER_L, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		MetricTHREECARDPOKER_L(sCasinoMetricLight* data) : MetricCASINOLIGHT(data)
		{

		}
	};

	void CommandPlaystatsCasinoThreeCardPokerLight(sCasinoMetricLight* data)
	{
		MetricTHREECARDPOKER_L m(data);
		APPEND_METRIC(m);
	}

	class MetricSLOTMACHINE_L : public MetricCASINOLIGHT
	{
		RL_DECLARE_METRIC(SLOTMACHINE_L, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		MetricSLOTMACHINE_L(sSlotMachineMetricLight* data) : MetricCASINOLIGHT(&data->m_casinoMetric)
		{
			m_machineStyle = data->m_machineStyle.Int;
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricCASINOLIGHT::Write(rw);
			result &= rw->WriteInt("p", m_machineStyle);
			return result;
		}

		int m_machineStyle;
	};

	void CommandPlaystatsCasinoSlotMachineLight(sSlotMachineMetricLight* data)
	{
		MetricSLOTMACHINE_L m(data);
		APPEND_METRIC(m);
	}

	class MetricINSIDETRACK_L : public MetricCASINOLIGHT
	{
		RL_DECLARE_METRIC(INSIDETRACK_L, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		MetricINSIDETRACK_L(sInsideTrackMetricLight* data) : MetricCASINOLIGHT(&data->m_casinoMetric)
		{
			m_individualGameCount = data->m_individualGameCount.Int;
			m_mainEventCount = data->m_mainEventCount.Int;
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricCASINOLIGHT::Write(rw);
			result &= rw->WriteInt("p", m_individualGameCount);
			result &= rw->WriteInt("q", m_mainEventCount);
			return result;
		}

		int m_individualGameCount;
		int m_mainEventCount;
	};

	void CommandPlaystatsCasinoInsideTrackLight(sInsideTrackMetricLight* data)
	{
		MetricINSIDETRACK_L m(data);
		APPEND_METRIC(m);
	}

	//////////////////////////////////////////////////////////////////////////

	class MetricARCADEGAME : public MetricPlayerCoords
	{
		RL_DECLARE_METRIC(ARCADEGAME, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		MetricARCADEGAME(int gameType, int numPlayers, int powerUps, int kills, int timePlayed, int score, int location)
			: MetricPlayerCoords()
			, m_gameType(gameType)
			, m_numPlayers(numPlayers)
			, m_powerUps(powerUps)
			, m_kills(kills)
			, m_timePlayed(timePlayed)
			, m_score(score)
			, m_location(location)
		{

		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayerCoords::Write(rw);
			result &= rw->WriteInt("a", m_gameType);
			result &= rw->WriteInt("b", m_numPlayers);
			result &= rw->WriteInt("c", m_powerUps);
			result &= rw->WriteInt("d", m_kills);
			result &= rw->WriteInt("e", m_timePlayed);
			result &= rw->WriteInt("f", m_score);
			result &= rw->WriteInt("g", m_location);
			return result;
		}

	public:

		int m_gameType;
		int m_numPlayers;
		int m_powerUps;
		int m_kills;
		int m_timePlayed;
		int m_score;
		int m_location;
	};

	void CommandPlaystatsArcadeGame(int gameType, int numPlayers, int powerUps, int kills, int timePlayed, int score, int location)
	{
		MetricARCADEGAME m(gameType, numPlayers, powerUps, kills, timePlayed, score, location);
		APPEND_METRIC(m);
	}

	//////////////////////////////////////////////////////////////////////////

	class MetricCASINO_MISSION_ENDED : public MetricPlayStat
	{
		RL_DECLARE_METRIC(CASINO_MISSION_ENDED, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		MetricCASINO_MISSION_ENDED()
			: MetricPlayStat()
			, m_a(-1)
			, m_b(-1)
			, m_c(-1)
			, m_d(-1)
			, m_e(-1)
			, m_f(-1)
			, m_g(-1)
			, m_h(-1)
			, m_i(-1)
			, m_j(-1)
			, m_k(-1)
			, m_l(-1)
			, m_m(-1)
			, m_n(-1)
			, m_o(-1)
			, m_p(-1)
			, m_q(-1)
			, m_r(-1)
			, m_s(-1)
			, m_t(-1)
			, m_u(-1)
			, m_v(false)
			, m_w(false)
			, m_x(false)
			, m_y(-1)
		{
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);

			result &= rw->WriteInt("mtid", m_missionTypeId);
			result &= rw->WriteInt("mid", m_missionId);
			result &= rw->WriteInt64("maid", m_matchId);
			result &= rw->WriteInt64("buuid", m_bossId);
			result &= rw->WriteInt("isboss", m_isboss);
			result &= rw->WriteInt64("sid", m_sessionId);

			if (m_a > -1)  result &= rw->WriteInt("a", m_a);
			if (m_b > -1)  result &= rw->WriteInt("b", m_b);
			if (m_c > -1)  result &= rw->WriteInt("c", m_c);
			if (m_d > -1)  result &= rw->WriteInt("d", m_d);
			if (m_e > -1)  result &= rw->WriteInt("e", m_e);
			if (m_f > -1)  result &= rw->WriteInt("f", m_f);
			if (m_g > -1)  result &= rw->WriteInt("g", m_g);
			if (m_h > -1)  result &= rw->WriteInt("h", m_h);
			if (m_i > -1)  result &= rw->WriteInt("i", m_i);
			if (m_j > -1)  result &= rw->WriteInt("j", m_j);
			if (m_k > -1)  result &= rw->WriteInt("k", m_k);
			if (m_l > -1)  result &= rw->WriteInt("l", m_l);
			if (m_m > -1)  result &= rw->WriteInt("m", m_m);
			if (m_n > -1)  result &= rw->WriteInt("n", m_n);
			if (m_o > -1)  result &= rw->WriteInt("o", m_o);
			if (m_p > -1)  result &= rw->WriteInt("p", m_p);
			if (m_q > -1)  result &= rw->WriteInt("q", m_q);
			if (m_r > -1)  result &= rw->WriteInt("r", m_r);
			if (m_s > -1)  result &= rw->WriteInt("s", m_s);
			if (m_t > -1)  result &= rw->WriteInt("t", m_t);
			if (m_u > -1)  result &= rw->WriteInt("u", m_u);
			result &= rw->WriteBool("v", m_v);
			result &= rw->WriteBool("w", m_w);
			result &= rw->WriteBool("x", m_x);
			if (m_y > -1)  result &= rw->WriteInt("y", m_y);

			return result;
		}

	public:
		int m_missionTypeId;
		int m_missionId;
		u64 m_matchId;
		u64 m_bossId;
		int m_isboss;
		u64 m_sessionId;
		int m_a;
		int m_b;
		int m_c;
		int m_d;
		int m_e;
		int m_f;
		int m_g;
		int m_h;
		int m_i;
		int m_j;
		int m_k;
		int m_l;
		int m_m;
		int m_n;
		int m_o;
		int m_p;
		int m_q;
		int m_r;
		int m_s;
		int m_t;
		int m_u;
		bool m_v;
		bool m_w;
		bool m_x;
		int m_y;
	};

	void CommandPlaystatsFreemodeCasinoMissionEnd(sFreemodeCasinoMissionEnded* data)
	{
		scriptAssertf(data, "%s: PLAYSTATS_FREEMODE_CASINO_MISSION_ENDED - data is NULL.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!data)
			return;

		const u64 bid0 = ((u64)data->m_bossId1.Int) << 32;
		const u64 bid1 = (0x00000000FFFFFFFF & (u64)data->m_bossId2.Int);
		const u64 localbossId = StatsInterface::GetUInt64Stat(StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID"));

		MetricCASINO_MISSION_ENDED m;
		m.m_missionTypeId = data->m_missionTypeId.Int;
		scriptAssertf(data->m_missionId.Int != 0, "%s: PLAYSTATS_FREEMODE_CASINO_MISSION_ENDED - MissionId is 0.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		m.m_missionId = data->m_missionId.Int;
		const u64 mid0 = ((u64)data->m_matchId1.Int) << 32;
		const u64 mid1 = ((u64)data->m_matchId2.Int);
		m.m_matchId = mid0 | mid1;
		m.m_bossId = bid0 | bid1;
		m.m_isboss = (m.m_bossId != 0 && m.m_bossId == localbossId);
		m.m_sessionId = CNetworkTelemetry::GetCurMpSessionId();

		m.m_a = data->m_PlayerParticipated.Int;
		m.m_b = data->m_PlayerRole.Int;
		m.m_c = data->m_TimeStart.Int;
		m.m_d = data->m_TimeEnd.Int;
		m.m_e = data->m_Won.Int;
		m.m_f = data->m_EndingReason.Int;
		m.m_g = data->m_CashEarned.Int;
		m.m_h = data->m_RpEarned.Int;
		m.m_i = data->m_TargetsKilled.Int;
		m.m_j = data->m_InnocentsKilled.Int;
		m.m_k = data->m_Deaths.Int;
		m.m_l = data->m_LauncherRank.Int;
		m.m_m = data->m_TimeTakenToComplete.Int;
		m.m_n = data->m_PlayersLeftInProgress.Int;
		m.m_o = data->m_Location.Int;
		m.m_p = data->m_InvitesSent.Int;
		m.m_q = data->m_InvitesAccepted.Int;
		m.m_r = data->m_BossKilled.Int;
		m.m_s = data->m_GoonsKilled.Int;
		m.m_t = data->m_missionVariation.Int;
		m.m_u = data->m_launchMethod.Int;
		m.m_v = data->m_ownPenthouse.Int != 0;
		m.m_w = data->m_ownGarage.Int != 0;
		m.m_x = data->m_ownOffice.Int != 0;
		m.m_y = data->m_houseChipsEarned.Int;

		APPEND_METRIC(m);
	}

	////////////////////////////////////////////////////////////////////////// Heist 3

/////////////////////////////////////////////////////////////////////////////// HEIST3_DRONE
	class MetricHEIST3_DRONE : public MetricPlayStat
	{
		RL_DECLARE_METRIC(HEIST3_DRONE, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

	public:
		MetricHEIST3_DRONE(Heist3DroneInfo* data)
		{
			m_missionName = data->m_missionName.Int;
			m_playthroughId = data->m_playthroughId.Int;
			m_endReason = data->m_endReason.Int;
			m_time = data->m_time.Int;
			m_totalDroneTases = data->m_totalDroneTases.Int;
			m_totalDroneTranq = data->m_totalDroneTranq.Int;
			m_nano = data->m_nano.Int != 0;
			m_cpCollected = data->m_cpCollected.Int;
			m_targetsKilled = data->m_targetsKilled.Int;
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = true;
			result &= rw->WriteInt("a", m_missionName);
			result &= rw->WriteInt("b", m_playthroughId);
			result &= rw->WriteInt("c", m_endReason);
			result &= rw->WriteInt("d", m_time);
			result &= rw->WriteInt("e", m_totalDroneTases);
			result &= rw->WriteInt("f", m_totalDroneTranq);
			result &= rw->WriteBool("g", m_nano);
			result &= rw->WriteInt("h", m_cpCollected);
			result &= rw->WriteInt("i", m_targetsKilled);
			return result;
		}

		int m_missionName;             //Checkpoint Collection, Assassination, Finale, null if not applicable
		int m_playthroughId;           //if used during the finale, null if not applicable; playthrough ID, this should appear in all related content, like scopes, preps and finales, and would enable us to track an entire heist playthrough
		int m_endReason;               //reason for stopping
		int m_time;                    //length of time drone was used
		int m_totalDroneTases;         //number of cameras and enemies tased
		int m_totalDroneTranq;         //number of enemies tranquilised
		bool m_nano;                   //whether this is the nano drone
		int m_cpCollected;             //checkpoints collected before exiting, in Checkpoint Collection; null if not applicable
		int m_targetsKilled;           //targets killed in Assassination; null if not applicable
	};

	// triggers after using the Casino Heist drone
	void CommandPlaystatsHeist3Drone(Heist3DroneInfo* data)
	{
		MetricHEIST3_DRONE m(data);
		APPEND_METRIC(m);
	}

	/////////////////////////////////////////////////////////////////////////////// HEIST3_HACK
	class MetricHEIST3_HACK : public MetricPlayStat
	{
		RL_DECLARE_METRIC(HEIST3_HACK, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

	public:
		MetricHEIST3_HACK(int type, int playthroughId, int minigame, bool won, int step, int time)
		{
			m_type = type;
			m_playthroughId = playthroughId;
			m_minigame = minigame;
			m_won = won;
			m_step = step;
			m_time = time;
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = true;
			result &= rw->WriteInt("a", m_type);
			result &= rw->WriteInt("b", m_playthroughId);
			result &= rw->WriteInt("c", m_minigame);
			result &= rw->WriteBool("d", m_won);
			result &= rw->WriteInt("e", m_step);
			result &= rw->WriteInt("f", m_time);
			return result;
		}

		int m_type;                    //practice, heist
		int m_playthroughId;           //if used during the finale, null if not applicable; playthrough ID, this should appear in all related content, like scopes, preps and finales, and would enable us to track an entire heist playthrough
		int m_minigame;                //fingerprint clone, order unlock
		bool m_won;                    //whether the player won
		int m_step;                    //what step did the player fail on, null if not applicable
		int m_time;                    //time spent on the minigame
	};

	// triggers at the end of the hacking minigames added in Casino Heist
	void CommandPlaystatsHeist3Hack(int type, int playthroughId, int minigame, bool won, int step, int time)
	{
		MetricHEIST3_HACK m(type, playthroughId, minigame, won, step, time);
		APPEND_METRIC(m);
	}

	/////////////////////////////////////////////////////////////////////////////// NPC_PHONE
	class MetricNPC_PHONE : public MetricPlayStat
	{
		RL_DECLARE_METRIC(NPC_PHONE, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

	public:
		MetricNPC_PHONE(int caller, int type, int subject, int playerAction, bool request, int rank)
		{
			m_caller = caller;
			m_type = type;
			m_subject = subject;
			m_playerAction = playerAction;
			m_request = request;
			m_rank = rank;
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = true;
			result &= rw->WriteInt("a", m_caller);
			result &= rw->WriteInt("b", m_type);
			result &= rw->WriteInt("c", m_subject);
			result &= rw->WriteInt("d", m_playerAction);
			result &= rw->WriteBool("e", m_request);
			result &= rw->WriteInt("f", m_rank);
			return result;
		}

		int m_caller;                  //NPC who called/texted
		int m_type;                    //text, email, cal
		int m_subject;                 //what this call is about
		int m_playerAction;            //player's action: accept, reject, ignore
		bool m_request;                //whether the player requested this call on the pause menu map
		int m_rank;                    //player's rank
	};

	// triggers after being contacted by an NPC on the phone
	void CommandPlaystatsNpcPhone(int caller, int type, int subject, int playerAction, bool request, int rank)
	{
		MetricNPC_PHONE m(caller, type, subject, playerAction, request, rank);
		APPEND_METRIC(m);
	}

	/////////////////////////////////////////////////////////////////////////////// ARCADE_CABINET
	class MetricARCADE_CABINET : public MetricPlayStat
	{
		RL_DECLARE_METRIC(ARC_CAB, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

	public:
		MetricARCADE_CABINET(ArcadeCabinetInfo* data)
		{
			m_gameType = data->m_gameType.Int;
			m_matchId = data->m_matchId.Int;
			m_numPlayers = data->m_numPlayers.Int;
			m_level = data->m_level.Int;
			m_powerUps = data->m_powerUps.Int;
			m_kills = data->m_kills.Int;
			m_timePlayed = data->m_timePlayed.Int;
			m_score = data->m_score.Int;
			m_reward = data->m_reward.Int;
			m_challenges = data->m_challenges.Int;
			m_location = data->m_location.Int;
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = true;
			result &= rw->WriteInt("a", m_gameType);
			result &= rw->WriteInt("b", m_matchId);
			result &= rw->WriteInt("c", m_numPlayers);
			result &= rw->WriteInt("d", m_level);
			result &= rw->WriteInt("e", m_powerUps);
			result &= rw->WriteInt("f", m_kills);
			result &= rw->WriteInt("g", m_timePlayed);
			result &= rw->WriteInt("h", m_score);
			result &= rw->WriteInt("i", m_reward);
			result &= rw->WriteInt("j", m_challenges);
			result &= rw->WriteInt("k", m_location);
			return result;
		}

		int m_gameType;                //which arcade game: Badlands Revenge 2, Race & Chase, Axe of Fury, Madame Nazar etc.
		int m_matchId;                 //ID to connect the arcade match between players, where applicable
		int m_numPlayers;              //number of players
		int m_level;                   //which level the player stopped on
		int m_powerUps;                //number of power ups collected
		int m_kills;                   //number of entities destroyed/killed
		int m_timePlayed;              //time played
		int m_score;                   //score at the end of the game
		int m_reward;                  //fortune from Madam Nazar, prize from claw crane, trophy, null if not applicable
		int m_challenges;              //which challenges were completed
		int m_location;				   //property hash this arcade game is in
	};

	// triggers at the end of the Arcade minigames added in Casino Heist with the exception of Love Match, tracked separately
	void CommandPlaystatsArcadeCabinet(ArcadeCabinetInfo* data)
	{
		MetricARCADE_CABINET m(data);
		APPEND_METRIC(m);
	}

	/////////////////////////////////////////////////////////////////////////////// HEIST3_FINALE
	class MetricHEIST3_FINALE : public MetricPlayStat
	{
		RL_DECLARE_METRIC(HEIST3_FINALE, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

	public:
		MetricHEIST3_FINALE(Heist3FinaleInfo* data)
		{
			m_playthroughId = data->m_playthroughId.Int;
			m_missionId = data->m_missionId.Int;
			m_sessionId = CNetworkTelemetry::GetCurMpSessionId();
			safecpy(m_publicContentId, data->m_publicContentId);
			m_playthroughHashMac = data->m_playthroughHashMac.Int;
			m_bossId = ((u64)data->m_bossId1.Int) << 32 | (0x00000000FFFFFFFF & (u64)data->m_bossId2.Int);
			m_isBoss = (m_bossId != 0 && m_bossId == StatsInterface::GetUInt64Stat(StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID")));
			m_bosstype = data->m_bosstype.Int;
			m_launcherRank = data->m_launcherRank.Int;
			m_playerRole = data->m_playerRole.Int;
			m_endingReason = data->m_endingReason.Int;
			m_replay = data->m_replay.Int;
			m_rpEarned = data->m_rpEarned.Int;
			m_difficult = data->m_difficult.Int != 0;
			m_timeTakenToComplete = data->m_timeTakenToComplete.Int;
			m_checkpoint = data->m_checkpoint.Int;
			m_playCount = data->m_playCount.Int;
			m_approachBoard = data->m_approachBoard.Int;
			m_approachDirect = data->m_approachDirect.Int != 0;
			m_wCrew = data->m_wCrew.Int;
			m_wLoadout = data->m_wLoadout.Int;
			m_dCrew = data->m_dCrew.Int;
			m_vehicleGetaway = data->m_vehicleGetaway.Int;
			m_vehicleSwap = data->m_vehicleSwap.Int;
			m_hCrew = data->m_hCrew.Int;
			m_outfitIn = data->m_outfitIn.Int;
			m_outfitOut = data->m_outfitOut.Int;
			m_mask = data->m_mask.Int;
			m_vehicleSwapped = data->m_vehicleSwapped.Int;
			m_useEMP = data->m_useEMP.Int;
			m_useDrone = data->m_useDrone.Int;
			m_useThermite = data->m_useThermite.Int;
			m_useKeycard = data->m_useKeycard.Int;
			m_hack = data->m_hack.Int;
			m_cameras = data->m_cameras.Int;
			m_accessPoints = data->m_accessPoints.Int;
			m_vaultTarget = data->m_vaultTarget.Int;
			m_vaultAmt = data->m_vaultAmt.Int;
			m_dailyCashRoomAmt = data->m_dailyCashRoomAmt.Int;
			m_depositBoxAmt = data->m_depositBoxAmt.Int;
			m_percentage = data->m_percentage.Int;
			m_deaths = data->m_deaths.Int;
			m_targetsKilled = data->m_targetsKilled.Int;
			m_innocentsKilled = data->m_innocentsKilled.Int;
			m_buyerLocation = data->m_buyerLocation.Int;
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = true;
			result &= rw->WriteInt("a", m_playthroughId);
			result &= rw->WriteInt("b", m_missionId);
			result &= rw->WriteInt64("c", m_sessionId);
			result &= rw->WriteString("d", m_publicContentId);
			result &= rw->WriteInt("ee", m_playthroughHashMac);
			result &= rw->WriteInt64("f", m_bossId);
			result &= rw->WriteBool("g", m_isBoss);
			result &= rw->WriteInt("h", m_bosstype);
			result &= rw->WriteInt("i", m_launcherRank);
			result &= rw->WriteInt("j", m_playerRole);
			result &= rw->WriteInt("k", m_endingReason);
			result &= rw->WriteInt("l", m_replay);
			result &= rw->WriteInt("m", m_rpEarned);
			result &= rw->WriteBool("n", m_difficult);
			result &= rw->WriteInt("o", m_timeTakenToComplete);
			result &= rw->WriteInt("p", m_checkpoint);
			result &= rw->WriteInt("q", m_playCount);
			result &= rw->WriteInt("r", m_approachBoard);
			result &= rw->WriteBool("s", m_approachDirect);
			result &= rw->WriteInt("t", m_wCrew);
			result &= rw->WriteInt("u", m_wLoadout);
			result &= rw->WriteInt("v", m_dCrew);
			result &= rw->WriteInt("w", m_vehicleGetaway);
			result &= rw->WriteInt("x", m_vehicleSwap);
			result &= rw->WriteInt("y", m_hCrew);
			result &= rw->WriteInt("z", m_outfitIn);
			result &= rw->WriteInt("aa", m_outfitOut);
			result &= rw->WriteInt("ab", m_mask);
			result &= rw->WriteInt("ac", m_vehicleSwapped);
			result &= rw->WriteInt("ad", m_useEMP);
			result &= rw->WriteInt("ae", m_useDrone);
			result &= rw->WriteInt("af", m_useThermite);
			result &= rw->WriteInt("ag", m_useKeycard);
			result &= rw->WriteInt("ah", m_hack);
			result &= rw->WriteInt("ai", m_cameras);
			result &= rw->WriteInt("aj", m_accessPoints);
			result &= rw->WriteInt("ak", m_vaultTarget);
			result &= rw->WriteInt("al", m_vaultAmt);
			result &= rw->WriteInt("am", m_dailyCashRoomAmt);
			result &= rw->WriteInt("an", m_depositBoxAmt);
			result &= rw->WriteInt("ao", m_percentage);
			result &= rw->WriteInt("ap", m_deaths);
			result &= rw->WriteInt("aq", m_targetsKilled);
			result &= rw->WriteInt("ar", m_innocentsKilled);
			result &= rw->WriteInt("as", m_buyerLocation);
			return result;
		}

		int m_playthroughId;           //playthrough ID, this should appear in all related content, like scopes, preps and finales, and would enable us to track an entire heist playthrough
		int m_missionId;               //mission name
		u64 m_sessionId;               //session ID
		char m_publicContentId[MetricPlayStat::MAX_STRING_LENGTH]; //public content ID
		int m_playthroughHashMac;      //hash mac
		u64 m_bossId;                  //unique ID of the Boss instance
		bool m_isBoss;                 //whether this player is the leader
		int m_bosstype;                //type of org: Securoserv, MC, unaffiliated
		int m_launcherRank;            //rank of the player launching
		int m_playerRole;              //similar to previous metrics, the player's role: CEO, bodyguard, biker prospect etc.
		int m_endingReason;            //similar to previous metrics: won, lost, boss_left etc.
		int m_replay;                  //counts how many times this mission was failed, then continued from checkpoint/restarted; resets after the player exits the mission, null if not applicable
		int m_rpEarned;                //RP earned
		bool m_difficult;              //whether the mission has increased difficulty
		int m_timeTakenToComplete;     //time taken to complete
		int m_checkpoint;              //what checkpoint was the heist failed at; null if not applicable
		int m_playCount;               //how many times has the player completed this mission before; first time is 1, then increments of 1 after completing finale, only success matters
		int m_approachBoard;           //stealth, subterfuge, direct
		bool m_approachDirect;         //was the original approach failed, and default to direct; null if the original approach is Direct
		int m_wCrew;                   //weapon crew member
		int m_wLoadout;                //loadout 1 or 2
		int m_dCrew;                   //driver crew member
		int m_vehicleGetaway;          //getaway vehicle
		int m_vehicleSwap;             //vehicle picked for swapping
		int m_hCrew;                   //hacker crew member
		int m_outfitIn;                //outfit the player used when entering the casino
		int m_outfitOut;               //outfit the player used when leaving the casino, if applicable and different to outfitIn
		int m_mask;                    //mask used, null if not applicable
		int m_vehicleSwapped;          //whether the player used the chosen swap vehicle
		int m_useEMP;                  //whether the the EMP was used
		int m_useDrone;                //whether the player used the Drone
		int m_useThermite;             //whether the player used thermite to pass the security keypads
		int m_useKeycard;              //whether the player used the keycard to pass the security keypads
		int m_hack;                    //whether the player completed the hacking minigame to pass the security keypads
		int m_cameras;                 //does the player have the security cameras information
		int m_accessPoints;            //which access points were used
		int m_vaultTarget;             //diamonds, gold, cash, art
		int m_vaultAmt;                //amount stolen from vault
		int m_dailyCashRoomAmt;        //amount stolen from daily cash room
		int m_depositBoxAmt;           //amount stolen from safety deposit boxes
		int m_percentage;              //heist percentage of payout
		int m_deaths;                  //how many times the player died
		int m_targetsKilled;           //number of mission critical/aggressive peds killed
		int m_innocentsKilled;         //number of innocent peds killed
		int m_buyerLocation;           //buyer's location
	};

	// triggers at the end of finale missions
	void CommandPlaystatsHeist3Finale(Heist3FinaleInfo* data)
	{
		MetricHEIST3_FINALE m(data);
		APPEND_METRIC(m);
	}

	/////////////////////////////////////////////////////////////////////////////// HEIST3_PREP
	class MetricHEIST3_PREP : public MetricPlayStat
	{
		RL_DECLARE_METRIC(HEIST3_PREP, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

	public:
		MetricHEIST3_PREP(Heist3PrepInfo* data)
		{
			m_playthroughId = data->m_playthroughId.Int;
			m_missionName = data->m_missionName.Int;
			m_sessionId = CNetworkTelemetry::GetCurMpSessionId();
			m_playthroughHashMac = data->m_playthroughHashMac.Int;
			m_playCount = data->m_playCount.Int;
			m_approach = data->m_approach.Int;
			m_type = data->m_type.Int;
			m_bossId = ((u64)data->m_bossId1.Int) << 32 | (0x00000000FFFFFFFF & (u64)data->m_bossId2.Int);
			m_isBoss = (m_bossId != 0 && m_bossId == StatsInterface::GetUInt64Stat(StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID")));
			m_bosstype = data->m_bosstype.Int;
			m_launcherRank = data->m_launcherRank.Int;
			m_playerRole = data->m_playerRole.Int;
			m_playerParticipated = data->m_playerParticipated.Int != 0;
			m_difficult = data->m_difficult.Int != 0;
			m_timeTakenToComplete = data->m_timeTakenToComplete.Int;
			m_endingReason = data->m_endingReason.Int;
			m_isRivalParty = data->m_isRivalParty.Int != 0;
			m_cashEarned = data->m_cashEarned.Int;
			m_rpEarned = data->m_rpEarned.Int;
			m_location = data->m_location.Int;
			m_invitesSent = data->m_invitesSent.Int;
			m_invitesAccepted = data->m_invitesAccepted.Int;
			m_deaths = data->m_deaths.Int;
			m_targetsKilled = data->m_targetsKilled.Int;
			m_innocentsKilled = data->m_innocentsKilled.Int;
			m_crewMember = data->m_crewMember.Int;
			m_unlockYohan = data->m_unlockYohan.Int;
			m_unlockCharlie = data->m_unlockCharlie.Int;
			m_unlockChester1 = data->m_unlockChester1.Int;
			m_unlockChester2 = data->m_unlockChester2.Int;
			m_unlockZach = data->m_unlockZach.Int;
			m_unlockPatrick = data->m_unlockPatrick.Int;
			m_unlockAvi = data->m_unlockAvi.Int;
			m_unlockPaige = data->m_unlockPaige.Int;
			m_accessPoints = data->m_accessPoints.Int;
			m_shipmentsDestroyed = data->m_shipmentsDestroyed.Int;
			m_vaultTarget = data->m_vaultTarget.Int;
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = true;
			result &= rw->WriteInt("a", m_playthroughId);
			result &= rw->WriteInt("b", m_missionName);
			result &= rw->WriteInt64("c", m_sessionId);
			result &= rw->WriteInt("dd", m_playthroughHashMac);
			result &= rw->WriteInt("e", m_playCount);
			result &= rw->WriteInt("f", m_approach);
			result &= rw->WriteInt("g", m_type);
			result &= rw->WriteInt64("h", m_bossId);
			result &= rw->WriteBool("i", m_isBoss);
			result &= rw->WriteInt("j", m_bosstype);
			result &= rw->WriteInt("k", m_launcherRank);
			result &= rw->WriteInt("l", m_playerRole);
			result &= rw->WriteBool("m", m_playerParticipated);
			result &= rw->WriteBool("n", m_difficult);
			result &= rw->WriteInt("o", m_timeTakenToComplete);
			result &= rw->WriteInt("p", m_endingReason);
			result &= rw->WriteBool("q", m_isRivalParty);
			result &= rw->WriteInt("r", m_cashEarned);
			result &= rw->WriteInt("s", m_rpEarned);
			result &= rw->WriteInt("t", m_location);
			result &= rw->WriteInt("u", m_invitesSent);
			result &= rw->WriteInt("v", m_invitesAccepted);
			result &= rw->WriteInt("w", m_deaths);
			result &= rw->WriteInt("x", m_targetsKilled);
			result &= rw->WriteInt("y", m_innocentsKilled);
			result &= rw->WriteInt("z", m_crewMember);
			result &= rw->WriteInt("aa", m_unlockYohan);
			result &= rw->WriteInt("ab", m_unlockCharlie);
			result &= rw->WriteInt("ac", m_unlockChester1);
			result &= rw->WriteInt("ad", m_unlockChester2);
			result &= rw->WriteInt("ae", m_unlockZach);
			result &= rw->WriteInt("af", m_unlockPatrick);
			result &= rw->WriteInt("ag", m_unlockAvi);
			result &= rw->WriteInt("ah", m_unlockPaige);
			result &= rw->WriteInt("ai", m_accessPoints);
			result &= rw->WriteInt("aj", m_shipmentsDestroyed);
			result &= rw->WriteInt("ak", m_vaultTarget);
			return result;
		}

		int m_playthroughId;           //playthrough ID, this should appear in all related content, like scopes, preps and finales, and would enable us to track an entire heist playthrough
		int m_missionName;             //mission name
		u64 m_sessionId;               //session ID
		int m_playthroughHashMac;      //hash mac
		int m_playCount;               //how many times has the player completed this mission before, can be null for Arcade property setup, since it can only be played once; first time is 1, then increments of 1 after completing finale, only success matters
		int m_approach;                //stealth, subterfuge, direct, null if not applicable
		int m_type;                    //arcade setup, scope, weapon prep, vehicle prep, equipment prep
		u64 m_bossId;                  //unique ID of the Boss instance
		bool m_isBoss;                 //whether this player is the leader
		int m_bosstype;                //type of org: Securoserv, MC, unaffiliated
		int m_launcherRank;            //rank of the player launching
		int m_playerRole;              //similar to previous metrics, the player's role: CEO, bodyguard, biker prospect etc.
		bool m_playerParticipated;     //whether the player participated
		bool m_difficult;              //whether the mission has increased difficulty
		int m_timeTakenToComplete;     //time taken to complete
		int m_endingReason;            //similar to previous metrics: won, lost, boss_left etc.
		bool m_isRivalParty;           //is the party completing the mission the same as the party that launched it, null if not applicable
		int m_cashEarned;              //money earned
		int m_rpEarned;                //RP earned
		int m_location;                //mission subvariation or Arcade property location
		int m_invitesSent;             //invites sent
		int m_invitesAccepted;         //invites accepted
		int m_deaths;                  //how many times the player died
		int m_targetsKilled;           //number of mission critical/aggressive peds killed
		int m_innocentsKilled;         //number of innocent peds killed
		int m_crewMember;              //elected crew member for this type of prep, null if not applicable
		int m_unlockYohan;             //has this player completed the Nighclub setup, unlocking hacker Yohan?
		int m_unlockCharlie;           //has this player completed the Smuggler business setup, unlocking gunman Charlie?
		int m_unlockChester1;          //has this player completed the Gunrunning business setup, unlocking driver/gunman Chester McCoy?
		int m_unlockChester2;          //does this player own the Weapons Expert Arena Workshop upgrade, unlocking driver/gunman Chester McCoy?
		int m_unlockZach;              //does this player have a Biker Business set up, unlocking driver Zach?
		int m_unlockPatrick;           //has this player completed the secret Random Event in Freemode, unlocking gunman Packie McReary?
		int m_unlockAvi;               //has the player destroyed all 50 hidden signal jammers, unlocking hacker Avi Schwartzman?
		int m_unlockPaige;             //has the player completed a Client Job in their Terrorbyte, unlocking hacker Paige Harris?
		int m_accessPoints;            //which entry points were captured, for Scope Out Casino; null is not applicable
		int m_shipmentsDestroyed;      //number of shipments destroyed, for Disrupt Shipments, null if not applicable
		int m_vaultTarget;             //diamonds, gold, cash, art, for Vault Contents
	};

	// triggers at the end of arcade setup missions, casino scoping, general prep missions
	void CommandPlaystatsHeist3Prep(Heist3PrepInfo* data)
	{
		MetricHEIST3_PREP m(data);
		APPEND_METRIC(m);
	}

	/////////////////////////////////////////////////////////////////////////////// MASTER_CONTROL
	class MetricMASTER_CONTROL : public MetricPlayStat
	{
		RL_DECLARE_METRIC(MCT, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

	public:
		MetricMASTER_CONTROL(int accessed, int exit, int missionType, int missionName)
		{
			m_accessed = accessed;
			m_exit = exit;
			m_missionType = missionType;
			m_missionName = missionName;
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = true;
			result &= rw->WriteInt("a", m_accessed);
			result &= rw->WriteInt("b", m_exit);
			result &= rw->WriteInt("c", m_missionType);
			result &= rw->WriteInt("d", m_missionName);
			return result;
		}

		int m_accessed;                //what areas did the player navigate through. 
		int m_exit;                    //whether the player exited the MCT by leaving the menu or by starting a mission
		int m_missionType;             //type of mission: sell, restock etc.
		int m_missionName;             //mission launched, null if not applicable
	};

	// triggers when the player exits the MCT, tracking the actions taken
	void CommandPlaystatsMasterControl(int accessed, int exit, int missionType, int missionName)
	{
		MetricMASTER_CONTROL m(accessed, exit, missionType, missionName);
		APPEND_METRIC(m);
	}

	/////////////////////////////////////////////////////////////////////////////// QUIT_MODE
	class MetricQUIT_MODE : public MetricPlayStat
	{
		RL_DECLARE_METRIC(QUIT_MODE, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

	public:
		MetricQUIT_MODE(int missionType, const char* mid, int players, int duration, bool voteDiff)
		{
			m_missionType = missionType;
			m_sessionId = CNetworkTelemetry::GetCurMpSessionId();
			safecpy(m_mid, mid);
			m_players = players;
			m_duration = duration;
			m_voteDiff = voteDiff;
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = true;
			result &= rw->WriteInt("a", m_missionType);
			result &= rw->WriteInt64("b", m_sessionId);
			result &= rw->WriteString("c", m_mid);
			result &= rw->WriteInt("d", m_players);
			result &= rw->WriteInt("e", m_duration);
			result &= rw->WriteBool("f", m_voteDiff);
			return result;
		}

		int m_missionType;             //type of mission: sell, restock etc.
		u64 m_sessionId;               //Session ID
		char m_mid[MetricPlayStat::MAX_STRING_LENGTH]; //Public content ID
		int m_players;                 //Number of players in mode at the time of leaving
		int m_duration;                //Time spent in mode until leaving
		bool m_voteDiff;               //Did the vote on the NJVS/MISSION_VOTE differ from this player's vote; null if N/A
	};

	// triggers if you quit a job/mission before the end; should also cover scenario where the player quits to singleplayer
	void CommandPlaystatsQuitMode(int missionType, const char* mid, int players, int duration, bool voteDiff)
	{
		MetricQUIT_MODE m(missionType, mid, players, duration, voteDiff);
		APPEND_METRIC(m);
	}

	/////////////////////////////////////////////////////////////////////////////// MISSION_VOTE
	class MetricMISSION_VOTE : public MetricPlayStat
	{
		RL_DECLARE_METRIC(MISSION_VOTE, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

	public:
		MetricMISSION_VOTE(MissionVoteInfo* data)
		{
			m_sessionId = CNetworkTelemetry::GetCurMpSessionId();
			safecpy(m_mid, data->m_mid);
			m_numPlayers = data->m_numPlayers.Int;
			m_voteChoice = data->m_voteChoice.Int;
			m_result = data->m_result.Int;
			m_timeOnline = data->m_timeOnline.Int;
			m_voteDiff = data->m_voteDiff.Int != 0;
			m_endBoard = data->m_endBoard.Int != 0;
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = true;
			result &= rw->WriteInt64("a", m_sessionId);
			result &= rw->WriteString("b", m_mid);
			result &= rw->WriteInt("c", m_numPlayers);
			result &= rw->WriteInt("d", m_voteChoice);
			result &= rw->WriteInt("e", m_result);
			result &= rw->WriteInt("f", m_timeOnline);
			result &= rw->WriteBool("g", m_voteDiff);
			result &= rw->WriteBool("h", m_endBoard);
			return result;
		}

		u64 m_sessionId;               //Session ID
		char m_mid[MetricPlayStat::MAX_STRING_LENGTH]; //Public content ID of mission the player is in
		int m_numPlayers;              //Number of current players in the vote screen
		int m_voteChoice;              //Restart, continue from checkpoint, quit, abstain; null if equal to field result
		int m_result;                  //Outcome of the vote
		int m_timeOnline;              //Total time the player has been online
		bool m_voteDiff;               //Did the winning vote on the fail alert screen differ from this player's vote
		bool m_endBoard;			   //whether this is the leaderboard displayed at the end of the mode or not
	};

	// triggers after voting on the mission fail alert screen
	void CommandPlaystatsMissionVote(MissionVoteInfo* data)
	{
		MetricMISSION_VOTE m(data);
		APPEND_METRIC(m);
	}

	/////////////////////////////////////////////////////////////////////////////// NJVS_VOTE
	class MetricNJVS_VOTE : public MetricPlayStat
	{
		RL_DECLARE_METRIC(NJVS_VOTE, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

	public:
		MetricNJVS_VOTE(NjvsVoteInfo* data)
		{
			m_sessionId = CNetworkTelemetry::GetCurMpSessionId();
			safecpy(m_mid, data->m_mid);
			m_numPlayers = data->m_numPlayers.Int;
			safecpy(m_voteChoice, data->m_voteChoice);
			safecpy(m_result, data->m_result);
			m_timeOnline = data->m_timeOnline.Int;
			m_voteDiff = data->m_voteDiff.Int != 0;
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = true;
			result &= rw->WriteInt64("a", m_sessionId);
			result &= rw->WriteString("b", m_mid);
			result &= rw->WriteInt("c", m_numPlayers);
			result &= rw->WriteString("d", m_voteChoice);
			result &= rw->WriteString("e", m_result);
			result &= rw->WriteInt("f", m_timeOnline);
			result &= rw->WriteBool("g", m_voteDiff);
			return result;
		}

		u64 m_sessionId;               //Session ID
		char m_mid[MetricPlayStat::MAX_STRING_LENGTH]; //Public content ID of mission the player is in
		int m_numPlayers;              //Number of current players in the vote screen
		char m_voteChoice[MetricPlayStat::MAX_STRING_LENGTH]; //Which mode the player voted for while on the NJVS; presumably will be the public content ID, null if abstain or equal to field result
		char m_result[MetricPlayStat::MAX_STRING_LENGTH]; //Mode that won the vote; presumably public content ID
		int m_timeOnline;              //Total time the player has been online
		bool m_voteDiff;               //Did the winning vote on the NJVS differ from this player's vote; null if N/A
	};

	// triggers after voting is concluded on the NJVS
	void CommandPlaystatsNjvsVote(NjvsVoteInfo* data)
	{
		MetricNJVS_VOTE m(data);
		APPEND_METRIC(m);
	}

	void CommandPlaystatsKillYourself()
	{
		CPed* playerPed = FindPlayerPed();
		if(playerPed)
		{
			CNetworkTelemetry::PlayerDied(playerPed, playerPed, 0, 0, false);
		}
	}

	class MetricDISPATCH_ENDED : public MetricPlayStat
	{
		RL_DECLARE_METRIC(DISPATCH_ENDED, TELEMETRY_CHANNEL_MISSION, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		MetricDISPATCH_ENDED(sDispatchEndedMetric* data, const char* contentId) : MetricPlayStat()
		{
			m_SessionId = CNetworkTelemetry::GetCurMpSessionId();
			m_GameMode = CNetworkTelemetry::GetCurMpGameMode();
			m_PublicSlots = CNetworkTelemetry::GetCurMpNumPubSlots();
			m_PrivateSlots = CNetworkTelemetry::GetCurMpNumPrivSlots();
			m_MatchCreator = data->m_MatchCreator.Int;
			safecpy(m_PublicContentId, contentId);
			m_SessionType = data->m_SessionType.Int;
			m_MatchmakingKey = NetworkBaseConfig::GetMatchmakingUser(false);

			m_launchMethod = data->m_launchMethod.Int;
			m_endingReason = data->m_endingReason.Int;
			m_timeTakenToComplete = data->m_timeTakenToComplete.Int;
			m_targetsKilled = data->m_targetsKilled.Int;
			m_cashEarned = data->m_cashEarned.Int;
			m_rpEarned = data->m_rpEarned.Int;
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);

			if (result)
			{
				result &= rw->WriteInt64("a", m_SessionId);
				result &= rw->WriteInt("b", m_GameMode);
				result &= rw->WriteInt("c", m_PublicSlots);
				result &= rw->WriteInt("d", m_PrivateSlots);
				result &= rw->WriteInt("e", m_MatchCreator);
				result &= rw->WriteString("f", m_PublicContentId);
				result &= rw->WriteInt("g", m_SessionType);
				result &= rw->WriteUns("h", m_MatchmakingKey);
				////////////////////////////////////////////////////
				result &= rw->WriteInt("i", m_launchMethod);
				result &= rw->WriteInt("j", m_endingReason);
				result &= rw->WriteInt("k", m_timeTakenToComplete);
				result &= rw->WriteInt("l", m_targetsKilled);
				result &= rw->WriteInt("m", m_cashEarned);
				result &= rw->WriteInt("n", m_rpEarned);
			}

			return result;
		}

		u64 m_SessionId;
		int m_GameMode;
		int m_PublicSlots;
		int m_PrivateSlots;
		int m_MatchCreator;
		char m_PublicContentId[32];
		int m_SessionType;
		u32 m_MatchmakingKey;

		int m_launchMethod;		// how this mission was launched; accepted/requested dispatch call
		int m_endingReason;		// reason this mission ended; arrest, kill, die, leave
		int m_timeTakenToComplete;	// time taken to complete
		int m_targetsKilled;		// how many hostile mission peds were killed
		int m_cashEarned;			// amount of cash earned
		int m_rpEarned;			// amount of RP earned


	};
	void CommandPlaystatsDispatchEnded(sDispatchEndedMetric* data, const char* contentId)
	{
		MetricDISPATCH_ENDED m(data, contentId);
		APPEND_METRIC(m);
	}

	////////////////////////////////////////////////////////////////////////// YACHT_ENDED

	class MetricYACHT_ENDED : public MetricPlayStat
	{
		RL_DECLARE_METRIC(YACHT_ENDED, TELEMETRY_CHANNEL_MONEY, LOGLEVEL_VERYHIGH_PRIORITY);

	public:
		MetricYACHT_ENDED()
			: MetricPlayStat()
			, m_matchId(0)
			, m_bossId(0)
			, m_sessionid(0)
			, m_missionId(0)
			, m_missionTypeId(0)
			, m_isboss(false)
			, m_bosstype(-1)
			, m_playerrole(-1)
			, m_playerParticipated(-1)
			, m_won(-1)
			, m_launchMethod(-1)             // where this mission was launched from (from blip, phoning captain, pause menu)
			, m_endingReason(-1)
			, m_cashEarned(-1)
			, m_rpEarned(-1)
			, m_bossKilled(-1)
			, m_goonsKilled(-1)
			, m_yachtLocation(-1)
			, m_targetskilled(-1)
			, m_timetakentocomplete(-1)
			, m_playersleftinprogress(-1)
			, m_waterBombs(-1)              // number of water bombs dropped(mission Bluffs on Fire)
			, m_lockOn(-1)                  // number of times the player got locked - on(mission Kraken Skulls)
		{
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);

			result &= rw->WriteInt64("maid", m_matchId);
			result &= rw->WriteInt64("buuid", m_bossId);
			result &= rw->WriteInt64("sid", m_sessionid);
			result &= rw->WriteInt("mid", m_missionId);
			result &= rw->WriteInt("mtid", m_missionTypeId);
			result &= rw->WriteBool("isboss", m_isboss);

			if (m_bosstype != -1)              result &= rw->WriteInt("a", m_bosstype);
			if (m_playerrole != -1)            result &= rw->WriteInt("b", m_playerrole);
			if (m_playerParticipated != -1)    result &= rw->WriteInt("c", m_playerParticipated);
			if (m_won != -1)                   result &= rw->WriteInt("d", m_won);
			if (m_launchMethod != -1)          result &= rw->WriteInt("e", m_launchMethod);
			if (m_endingReason != -1)          result &= rw->WriteInt("f", m_endingReason);
			if (m_cashEarned != -1)            result &= rw->WriteInt("g", m_cashEarned);
			if (m_rpEarned != -1)              result &= rw->WriteInt("h", m_rpEarned);
			if (m_bossKilled != -1)            result &= rw->WriteInt("i", m_bossKilled);
			if (m_goonsKilled != -1)           result &= rw->WriteInt("j", m_goonsKilled);
			if (m_yachtLocation != -1)         result &= rw->WriteInt("l", m_yachtLocation);
			if (m_targetskilled != -1)         result &= rw->WriteInt("m", m_targetskilled);
			if (m_timetakentocomplete != -1)   result &= rw->WriteInt("n", m_timetakentocomplete);
			if (m_playersleftinprogress != -1) result &= rw->WriteInt("o", m_playersleftinprogress);
			if (m_waterBombs != -1)            result &= rw->WriteInt("p", m_waterBombs);
			if (m_lockOn != -1)                result &= rw->WriteInt("q", m_lockOn);

			return result;
		}

	public:
		u64 m_matchId;
		u64 m_bossId;
		u64 m_sessionid;
		int m_missionId;
		int m_missionTypeId;
		bool m_isboss;
		int  m_bosstype;
		int  m_playerrole;
		int  m_playerParticipated;
		int  m_won;
		int  m_launchMethod;             // where this mission was launched from (from blip, phoning captain, pause menu)
		int  m_endingReason;
		int  m_cashEarned;
		int  m_rpEarned;
		int  m_bossKilled;
		int  m_goonsKilled;
		int  m_yachtLocation;
		int  m_targetskilled;
		int  m_timetakentocomplete;
		int  m_playersleftinprogress;
		int  m_waterBombs;              // number of water bombs dropped(mission Bluffs on Fire)
		int  m_lockOn;                  // number of times the player got locked - on(mission Kraken Skulls)
	};

	void CommandPlaystatsYachtEnded(sYatchEnded* data)
	{
		scriptAssertf(data, "%s: PLAYSTATS_YACHT_ENDED - data is NULL.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		if (!data)
			return;

		const u64 bid0 = ((u64)data->m_bossId1.Int) << 32;
		const u64 bid1 = (0x00000000FFFFFFFF & (u64)data->m_bossId2.Int);
		const u64 localbossId = StatsInterface::GetUInt64Stat(StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID"));

		MetricYACHT_ENDED m;

		const u64 mid0 = ((u64)data->m_matchId1.Int) << 32;
		const u64 mid1 = ((u64)data->m_matchId2.Int);
		m.m_matchId = mid0 | mid1;
		m.m_bossId = bid0 | bid1;
		m.m_sessionid = CNetworkTelemetry::GetCurMpSessionId();
		m.m_missionId = data->m_missionId.Int;
		m.m_missionTypeId = data->m_missionTypeId.Int;
		m.m_isboss = (m.m_bossId != 0 && m.m_bossId == localbossId);
		m.m_bosstype = data->m_bosstype.Int;
		m.m_playerrole = data->m_playerrole.Int;
		m.m_playerParticipated = data->m_playerParticipated.Int;
		m.m_won = data->m_won.Int;
		m.m_launchMethod = data->m_launchMethod.Int;
		m.m_endingReason = data->m_endingReason.Int;
		m.m_cashEarned = data->m_cashEarned.Int;
		m.m_rpEarned = data->m_rpEarned.Int;
		m.m_bossKilled = data->m_bossKilled.Int;
		m.m_goonsKilled = data->m_goonsKilled.Int;
		m.m_yachtLocation = data->m_yachtLocation.Int;
		m.m_targetskilled = data->m_targetskilled.Int;
		m.m_timetakentocomplete = data->m_timetakentocomplete.Int;
		m.m_playersleftinprogress = data->m_playersleftinprogress.Int;
		m.m_waterBombs = data->m_waterBombs.Int;
		m.m_lockOn = data->m_lockOn.Int;

		APPEND_METRIC(m);
	}



	
#if ARCHIVED_SUMMER_CONTENT_ENABLED   
	/////////////////////////////////////////////////////////////////////////////// Cops & Crooks

/////////////////////////////////////////////////////////////////////////////// CNC_ROUND
	class MetricCNC_ROUND : public MetricPlayStat
	{
		RL_DECLARE_METRIC(CNC_ROUND, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

	public:

		MetricCNC_ROUND(CncRoundInfo* data)
		{
			const u64 mid0 = ((u64)data->m_matchId1.Int) << 32;
			const u64 mid1 = ((u64)data->m_matchId2.Int);

			m_matchId = mid0|mid1;
			m_roundNumber = data->m_roundNumber.Int;
			m_team = data->m_team.Int;
			m_role = data->m_role.Int;
			m_gameRank = data->m_gameRank.Int;
			m_progRank = data->m_progRank.Int;
			m_roleChange = data->m_roleChange.Int != 0;
			m_spawnLoc = data->m_spawnLoc.Int;
			m_escapeLoc = data->m_escapeLoc.Int;
			m_roundStartTime = data->m_roundStartTime.Int;
			m_endReasonStgOne = data->m_endReasonStgOne.Int;
			m_endReasonStgTwo = data->m_endReasonStgTwo.Int;
			m_escapeResult = data->m_escapeResult.Int;
			m_durationStgOne = data->m_durationStgOne.Int;
			m_durationStgTwo = data->m_durationStgTwo.Int;
			m_launchMethod = data->m_launchMethod.Int;
			m_jip = data->m_jip.Int != 0;
			m_win = data->m_win.Int != 0;
			m_xpEarned = data->m_xpEarned.Int;
			m_streetCrimeCount = data->m_streetCrimeCount.Int;
			m_jobCount = data->m_jobCount.Int;
			m_jobsRemaining = data->m_jobsRemaining.Int;
			m_copPoints = data->m_copPoints.Int;
			m_arrests = data->m_arrests.Int;
			m_moneyEarned = data->m_moneyEarned.Int;
			m_moneyStashed = data->m_moneyStashed.Int;
			m_moneyBanked = data->m_moneyBanked.Int;
			m_crooksKilled = data->m_crooksKilled.Int;
			m_copsKilled = data->m_copsKilled.Int;
			m_deaths = data->m_deaths.Int;
			m_copKiller = data->m_copKiller.Int != 0;
			m_incapacitations = data->m_incapacitations.Int;
			m_moneyMaxHeld = data->m_moneyMaxHeld.Int;
			m_endurance = data->m_endurance.Int;
			m_spawnGroup = data->m_spawnGroup.Int;
			m_escapeTime = data->m_escapeTime.Int;
			m_spectateTime = data->m_spectateTime.Int;
			m_durationVehicleStgOne = data->m_durationVehicleStgOne.Int;
			m_durationVehicleStgTwo = data->m_durationVehicleStgTwo.Int;
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);
			result &= rw->WriteInt64("a", m_matchId);
			result &= rw->WriteInt("b", m_roundNumber);
			result &= rw->WriteInt("c", m_team);
			result &= rw->WriteInt("d", m_role);
			result &= rw->WriteInt("e", m_gameRank);
			result &= rw->WriteInt("f", m_progRank);
			result &= rw->WriteBool("g", m_roleChange);
			result &= rw->WriteInt("h", m_spawnLoc);
			result &= rw->WriteInt("i", m_escapeLoc);
			result &= rw->WriteInt("j", m_roundStartTime);
			result &= rw->WriteInt("k", m_endReasonStgOne);
			result &= rw->WriteInt("l", m_endReasonStgTwo);
			result &= rw->WriteInt("m", m_escapeResult);
			result &= rw->WriteInt("n", m_durationStgOne);
			result &= rw->WriteInt("o", m_durationStgTwo);
			result &= rw->WriteInt("p", m_launchMethod);
			result &= rw->WriteBool("q", m_jip);
			result &= rw->WriteBool("r", m_win);
			result &= rw->WriteInt("s", m_xpEarned);
			result &= rw->WriteInt("t", m_streetCrimeCount);
			result &= rw->WriteInt("u", m_jobCount);
			result &= rw->WriteInt("v", m_jobsRemaining);
			result &= rw->WriteInt("w", m_copPoints);
			result &= rw->WriteInt("x", m_arrests);
			result &= rw->WriteInt("y", m_moneyEarned);
			result &= rw->WriteInt("z", m_moneyStashed);
			result &= rw->WriteInt("aa", m_moneyBanked);
			result &= rw->WriteInt("ab", m_crooksKilled);
			result &= rw->WriteInt("ac", m_copsKilled);
			result &= rw->WriteInt("ad", m_deaths);
			result &= rw->WriteBool("ae", m_copKiller);
			result &= rw->WriteInt("af", m_incapacitations);
			result &= rw->WriteInt("ag", m_moneyMaxHeld);
			result &= rw->WriteInt("ah", m_endurance);
			result &= rw->WriteInt("ai", m_spawnGroup);
			result &= rw->WriteInt("aj", m_escapeTime);
			result &= rw->WriteInt("ak", m_spectateTime);
			result &= rw->WriteInt("al", m_durationVehicleStgOne);
			result &= rw->WriteInt("am", m_durationVehicleStgTwo);
			return result;
		}

	private:

		u64 m_matchId;                 // unique ID for the match, used to connect the two rounds and other metrics
		int m_roundNumber;             // round number
		int m_team;                    // cops/crooks
		int m_role;                    // hash, player's role
		int m_gameRank;                // player's current rank in GTAO classic
		int m_progRank;                // player's current CnC progression rank
		bool m_roleChange;             // whether the player has changed role during this match
		int m_spawnLoc;                // hash, spawn location; potentially not necessary
		int m_escapeLoc;               // hash, escape variant
		int m_roundStartTime;          // timestamp, start of the round from when the player joined
		int m_endReasonStgOne;         // stage 1 end reason: ended due to time, enough cop points, player quit etc.
		int m_endReasonStgTwo;         // stage 2 end reason: player died, player arrested, banked money, quit, time out etc.
		int m_escapeResult;            // how the escape ended: Success (Escaped), CopKill (The Crook was killed by a Cop), Dead (the player died for any other reason), Arrested (The crook was arrested), TimeUp (Stage timer ran out) etc.
		int m_durationStgOne;          // stage 1 duration, in milliseconds; round specific, might be shorter if the player quits before the end
		int m_durationStgTwo;          // stage 2 duration, in milliseconds; round specific, might be shorter if the player quits before the end
		int m_launchMethod;            // how the player launched this, via pause menu, corona, join with friends, on boot etc.
		bool m_jip;                    // whether the player joined in progress
		bool m_win;                    // at the end of round 2, whether this player's team has won
		int m_xpEarned;                // amount of XP earned
		int m_streetCrimeCount;        // on crook's round, number of street crimes the player has completed at the end of the round
		int m_jobCount;                // on crook's round, number of jobs the player completed at the end of the round
		int m_jobsRemaining;           // jobs remaining unattempted in the round
		int m_copPoints;               // number of cop points at the end of the round
		int m_arrests;                 // depending on team, number of times crook was arrested/ number of times cop made arrests
		int m_moneyEarned;             // on crook's round, money earned from street crimes and jobs; null if not applicable
		int m_moneyStashed;            // on crook's round, money stashed; null if not applicable
		int m_moneyBanked;             // on crook's round, money banked; null if not applicable
		int m_crooksKilled;            // crooks killed during this round
		int m_copsKilled;              // cops killed during this round
		int m_deaths;                  // number of times the player died
		bool m_copKiller;              // whether the player was marked as a cop killer during the round
		int m_incapacitations;         // depending on team, number of times crook was incapacitated/ number of times cop incapacitated a crook
		int m_moneyMaxHeld;            // on crook's round, max amount of money held during round that has not been stashed; null if not applicable
		int m_endurance;               // depending on team, amount of endurance damage received by crook or damage drained by cop
		int m_spawnGroup;              // group number for the group of 4 players the player spawns with
		int m_escapeTime;              // escape duration, in milliseconds; individual to the player
		int m_spectateTime;            // spectate mode duration, in milliseconds; individual to the player
		int m_durationVehicleStgOne;   // stage 1 time spent in any vehicle, in milliseconds
		int m_durationVehicleStgTwo;   // stage 2 time spent in any vehicle, in milliseconds
	};

	// triggers at the end of a round 
	void CommandPlaystatsCncRound(CncRoundInfo* data)
	{
		MetricCNC_ROUND m(data);
		APPEND_METRIC(m);
	}


	/////////////////////////////////////////////////////////////////////////////// CNC_LOBBY
	class MetricCNC_LOBBY : public MetricPlayStat
	{
		RL_DECLARE_METRIC(CNC_LOBBY, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

	public:

		MetricCNC_LOBBY(CncLobbyInfo* data)
		{
			const u64 mid0 = ((u64)data->m_matchId1.Int) << 32;
			const u64 mid1 = ((u64)data->m_matchId2.Int);
		
			m_matchId = mid0|mid1;
			m_team = data->m_team.Int;
			m_gameRank = data->m_gameRank.Int;
			m_progRank = data->m_progRank.Int;
			m_copRole = data->m_copRole.Int;
			m_copWeapons = data->m_copWeapons.Int;
			m_copClothing = data->m_copClothing.Int;
			m_copTattoos = data->m_copTattoos.Int;
			m_copEmote = data->m_copEmote.Int;
			m_copVehicle = data->m_copVehicle.Int;
			m_copAbilities = data->m_copAbilities.Int;
			m_crookRole = data->m_crookRole.Int;
			m_crookWeapons = data->m_crookWeapons.Int;
			m_crookClothing = data->m_crookClothing.Int;
			m_crookTattoos = data->m_crookTattoos.Int;
			m_crookEmote = data->m_crookEmote.Int;
			m_crookVehicle = data->m_crookVehicle.Int;
			m_crookAbilities = data->m_crookAbilities.Int;
			m_endReason = data->m_endReason.Int;
			m_joinFrom = data->m_joinFrom.Int;
			m_copSlot = data->m_copSlot.Int;
			m_crookSlot = data->m_crookSlot.Int;
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);
			result &= rw->WriteInt64("a", m_matchId);
			result &= rw->WriteInt("b", m_team);
			result &= rw->WriteInt("c", m_gameRank);
			result &= rw->WriteInt("d", m_progRank);
			result &= rw->WriteInt("e", m_copRole);
			result &= rw->WriteInt("f", m_copWeapons);
			result &= rw->WriteInt("g", m_copClothing);
			result &= rw->WriteInt("h", m_copTattoos);
			result &= rw->WriteInt("i", m_copEmote);
			result &= rw->WriteInt("j", m_copVehicle);
			result &= rw->WriteInt("k", m_copAbilities);
			result &= rw->WriteInt("l", m_crookRole);
			result &= rw->WriteInt("m", m_crookWeapons);
			result &= rw->WriteInt("n", m_crookClothing);
			result &= rw->WriteInt("o", m_crookTattoos);
			result &= rw->WriteInt("p", m_crookEmote);
			result &= rw->WriteInt("q", m_crookVehicle);
			result &= rw->WriteInt("r", m_crookAbilities);
			result &= rw->WriteInt("s", m_endReason);
			result &= rw->WriteInt("t", m_joinFrom);
			result &= rw->WriteInt("u", m_copSlot);
			result &= rw->WriteInt("v", m_crookSlot);
			return result;
		}

	private:

		u64 m_matchId;                 // unique ID for the match, used to connect the two rounds and other metrics
		int m_team;                    // what team does the player fall in round one, cops/crooks
		int m_gameRank;                // player's current rank in GTAO classic
		int m_progRank;                // player's current CnC progression rank
		int m_copRole;                 // hash, cop's role
		int m_copWeapons;              // hash array, cop weapons: primary lethal weapon, secondary lethal weapon, throwable, melee, non lethal; will also need to take into account modkits
		int m_copClothing;             // hash array, cop clothing: outfit, mask
		int m_copTattoos;              // hash array, cop tattoos: Head, Left Arm, Right Arm, Torso - Chest, Torso - Stomach, Torso - Back, Right Leg, Left Leg, Torso - Side
		int m_copEmote;                // hash, cop emote
		int m_copVehicle;              // hash array: cop vehicle, livery, modkit
		int m_copAbilities;            // hash array, cop abilities: special ability 1, passive ability 1, vehicle ability 1
		int m_crookRole;               // hash, crook's role
		int m_crookWeapons;            // hash array, crook weapons: primary lethal weapon, secondary lethal weapon, throwable, melee, non lethal; will also need to take into account modkits
		int m_crookClothing;           // hash array, crook clothing: outfit, mask
		int m_crookTattoos;            // hash array, crook tattoos: Head, Left Arm, Right Arm, Torso - Chest, Torso - Stomach, Torso - Back, Right Leg, Left Leg, Torso - Side
		int m_crookEmote;              // hash, crook emote
		int m_crookVehicle;            // hash array: crook vehicle, livery, modkit
		int m_crookAbilities;          // hash array, crook abilities: special ability 1, passive ability 1, vehicle ability 1
		int m_endReason;               // why did the lobby end for the player (match start, exit, disconnect, lobby disbanded, lobby merge, etc.)
		int m_joinFrom;                // where did they join this lobby from? (match end, lobby merge, lobby switch, main menu, join with friends, on boot etc.)
		int m_copSlot;				   //  which slot was used for the cop loadout: either preset/custom, or slot number, depending on how this is set up
		int m_crookSlot;			   //  which slot was used for the crook loadout: either preset/custom, or slot number, depending on how this is set up
	};

	// triggers at the end of the lobby; the main purpose is to track the initial loadouts for both cop and crook default role
	void CommandPlaystatsCncLobby(CncLobbyInfo* data)
	{
		MetricCNC_LOBBY m(data);
		APPEND_METRIC(m);
	}

	/////////////////////////////////////////////////////////////////////////////// CNC_STASH

	class MetricCNC_STASH : public MetricPlayStat
	{
		RL_DECLARE_METRIC(CNC_STASH, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

	public:

		MetricCNC_STASH(const int matchId1, const int matchId2, const int roundNumber, const int stashedFrom, const int safeLoc, const int amount, const int wantedLvl)
		{
			const u64 mid0 = ((u64)matchId1) << 32;
			const u64 mid1 = ((u64)matchId2);

			m_matchId = mid0 | mid1;
			m_roundNumber = roundNumber;
			m_stashedFrom = stashedFrom;
			m_safeLoc = safeLoc;
			m_amount = amount;
			m_wantedLvl = wantedLvl;
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);
			result &= rw->WriteInt64("a", m_matchId);
			result &= rw->WriteInt("b", m_roundNumber);
			result &= rw->WriteInt("c", m_stashedFrom);
			result &= rw->WriteInt("d", m_safeLoc);
			result &= rw->WriteInt("e", m_amount);
			result &= rw->WriteInt("f", m_wantedLvl);
			return result;
		}

	private:

		u64 m_matchId;                 // unique ID for the match, used to connect the two rounds and other metrics
		int m_roundNumber;             // round number
		int m_stashedFrom;             // whether the money came from a penalty, meaning the crook was killed by a cop, or from a player deposit
		int m_safeLoc;                 // hash, location of the safehouse; null for money earned from penalties
		int m_amount;                  // amount of cash stashed on this occasion
		int m_wantedLvl;               // crook's wanted level
	};

	// triggers after crook stashed money at a safehouse
	void CommandPlaystatsCncStash(const int matchId1, const int matchId2, const int roundNumber, const int stashedFrom, const int safeLoc, const int amount, const int wantedLvl)
	{
		MetricCNC_STASH m(matchId1, matchId2, roundNumber, stashedFrom, safeLoc, amount, wantedLvl);
		APPEND_METRIC(m);
	}

	/////////////////////////////////////////////////////////////////////////////// CNC_SAFE
	class MetricCNC_SAFE : public MetricPlayStat
	{
		RL_DECLARE_METRIC(CNC_SAFE, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);
	public:
		static const int m_safehouseArraySize = 32;
		int m_safehouse[m_safehouseArraySize];               // array of this round's safehouses

		MetricCNC_SAFE(const int matchId1, const int matchId2, const int roundNumber)
		{
			const u64 mid0 = ((u64)matchId1) << 32;
			const u64 mid1 = ((u64)matchId2);

			m_matchId = mid0 | mid1;
			m_roundNumber = roundNumber;

			// Initialise array to -1, eSAFEHOUSE_INVALID
			for (int i = 0; i < m_safehouseArraySize; i++)
			{
				m_safehouse[i] = -1;
			}
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);
			result &= rw->WriteInt64("a", m_matchId);
			result &= rw->WriteInt("b", m_roundNumber);
			result = result && rw->BeginArray("c", NULL);
			for (int i = 0; i < m_safehouseArraySize; i++)
			{
				result = result && rw->WriteInt(NULL, m_safehouse[i]);
			}
			result = result && rw->End();
			return result;
		}

	private:

		u64 m_matchId;                 // unique ID for the match, used to connect the two rounds and other metrics
		int m_roundNumber;             // round number
	};

	// triggers at the beginning of each round
	void CommandPlaystatsCncSafe(const int matchId1, const int matchId2, const int roundNumber, scrArrayBaseAddr& safehouses)
	{
		MetricCNC_SAFE m(matchId1, matchId2, roundNumber);

		int numItems = scrArray::GetCount(safehouses);
		scrValue* pArray = scrArray::GetElements<scrValue>(safehouses);
		scriptAssertf(m.m_safehouseArraySize >= numItems, "%s - PLAYSTATS_CNC_SAFE: Too many safehouses (%d). The metric only supports %d safehouses, ask code to bump it.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), numItems, m.m_safehouseArraySize);

		for (int i = 0; i < m.m_safehouseArraySize && i < numItems ; i++)
		{
			m.m_safehouse[i] = pArray[i].Int;
		}
		APPEND_METRIC(m);
	}

	/////////////////////////////////////////////////////////////////////////////// CNC_UNLOCK
	class MetricCNC_UNLOCK : public MetricPlayStat
	{
		RL_DECLARE_METRIC(CNC_UNLOCK, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

	public:

		MetricCNC_UNLOCK(CncUnlockInfo* data)
		{
			m_item = data->m_item.Int;
			m_itemType = data->m_itemType.Int;
			m_itemRank = data->m_itemRank.Int;
			m_itemRole = data->m_itemRole.Int;
			m_progRank = data->m_progRank.Int;
			m_tknSpend = data->m_tknSpend.Int;
			m_tknBalance = data->m_tknBalance.Int;
			m_moneySpend = data->m_moneySpend.Int;
			m_moneyBalance = MoneyInterface::GetVCBankBalance();
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);
			result &= rw->WriteInt("a", m_item);
			result &= rw->WriteInt("b", m_itemType);
			result &= rw->WriteInt("c", m_itemRank);
			result &= rw->WriteInt("d", m_itemRole);
			result &= rw->WriteInt("e", m_progRank);
			result &= rw->WriteInt("f", m_tknSpend);
			result &= rw->WriteInt("g", m_tknBalance);
			result &= rw->WriteInt("h", m_moneySpend);
			result &= rw->WriteInt64("i", m_moneyBalance);
			return result;
		}

	private:

		int m_item;                    // hash, item bought/unlocked in the CnC shop
		int m_itemType;                // hash, item type: outfit, mask, tattoo, weapon mod kit etc.
		int m_itemRank;                // progression rank this item belongs to
		int m_itemRole;                // hash, role this item is relevant for
		int m_progRank;                // player's current CnC progression rank
		int m_tknSpend;                // amount of tokens spent to buy the item
		int m_tknBalance;              // token balance after the transaction is completed
		int m_moneySpend;              // money spent to buy this item
		s64 m_moneyBalance;            // money balance after the transaction is completed
	};

	// triggers after purchasing an item in the progression shop
	void CommandPlaystatsCncUnlock(CncUnlockInfo* data)
	{
		MetricCNC_UNLOCK m(data);
		APPEND_METRIC(m);
	}

	/////////////////////////////////////////////////////////////////////////////// CNC_ABILITY
	class MetricCNC_ABILITY : public MetricPlayStat
	{
		RL_DECLARE_METRIC(CNC_ABILITY, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

	public:

		MetricCNC_ABILITY(CncAbilityInfo* data)
		{
			const u64 mid0 = ((u64)data->m_matchId1.Int) << 32;
			const u64 mid1 = ((u64)data->m_matchId2.Int);

			m_matchId = mid0 | mid1;
			m_team = data->m_team.Int;
			m_role = data->m_role.Int;
			m_roundNumber = data->m_roundNumber.Int;
			m_stage = data->m_stage.Int;
			m_ability = data->m_ability.Int;
			m_vehicle = data->m_vehicle.Int;
			m_wantedLvl = data->m_wantedLvl.Int;
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);
			result &= rw->WriteInt64("a", m_matchId);
			result &= rw->WriteInt("b", m_team);
			result &= rw->WriteInt("c", m_role);
			result &= rw->WriteInt("d", m_roundNumber);
			result &= rw->WriteInt("e", m_stage);
			result &= rw->WriteInt("f", m_ability);
			result &= rw->WriteInt("g", m_vehicle);
			result &= rw->WriteInt("h", m_wantedLvl);
			return result;
		}

	private:

		u64 m_matchId;                 // unique ID for the match, used to connect the two rounds and other metrics
		int m_team;                    // cops/crooks
		int m_role;                    // hash, player's role
		int m_roundNumber;             // round number
		int m_stage;                   // potentially hash; "Stash the Cash" or "the Escape"
		int m_ability;                 // hash, name of the ability used
		int m_vehicle;                 // hash, vehicle the player is in; null if not applicable
		int m_wantedLvl;               // crook's wanted level
	};

	// triggers after using an active ability
	void CommandPlaystatsCncAbility(CncAbilityInfo* data)
	{
		MetricCNC_ABILITY m(data);
		APPEND_METRIC(m);
	}

	/////////////////////////////////////////////////////////////////////////////// CNC_INCAPACITATE
	class MetricCNC_INCAPACITATE : public MetricPlayStat
	{
		RL_DECLARE_METRIC(CNC_INCAPACITATE, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

	public:

		MetricCNC_INCAPACITATE(CncIncapacitateInfo* data)
		{
			const u64 mid0 = ((u64)data->m_matchId1.Int) << 32;
			const u64 mid1 = ((u64)data->m_matchId2.Int);

			m_matchId = mid0 | mid1;
			m_incType = data->m_incType.Int;
			m_crookRole = data->m_crookRole.Int;
			m_crookProgRank = data->m_crookProgRank.Int;
			m_crookVehicle = data->m_crookVehicle.Int;
			m_copId = data->m_copId.Int;
			m_copRole = data->m_copRole.Int;
			m_copProgRank = data->m_copProgRank.Int;
			m_copWeapon = data->m_copWeapon.Int;
			m_copVehicle = data->m_copVehicle.Int;
			m_copKiller = data->m_copKiller.Int != 0;
			m_wantedLvl = data->m_wantedLvl.Int;
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);
			result &= rw->WriteInt64("a", m_matchId);
			result &= rw->WriteInt("b", m_incType);
			result &= rw->WriteInt("c", m_crookRole);
			result &= rw->WriteInt("d", m_crookProgRank);
			result &= rw->WriteInt("e", m_crookVehicle);
			result &= rw->WriteInt("f", m_copId);
			result &= rw->WriteInt("g", m_copRole);
			result &= rw->WriteInt("h", m_copProgRank);
			result &= rw->WriteInt("i", m_copWeapon);
			result &= rw->WriteInt("j", m_copVehicle);
			result &= rw->WriteBool("k", m_copKiller);
			result &= rw->WriteInt("l", m_wantedLvl);
			return result;
		}

	private:

		u64 m_matchId;                 // unique ID for the match, used to connect the two rounds and other metrics
		int m_incType;                 // hash, type of incapacitation: damage from non-lethal weapons, melee weapons, vehicle ejection, vehicle collision, falling etc.
		int m_crookRole;               // hash, crook's role
		int m_crookProgRank;           // CnC progression rank for crook
		int m_crookVehicle;            // hash, the vehicle the crook was in at the time; null if not applicable
		int m_copId;                   // cop account ID, from SCAdmin; this is the last cop that damaged the player's endurance prior to becoming incapacitated
		int m_copRole;                 // hash, cop's role
		int m_copProgRank;             // CnC progression rank for cop
		int m_copWeapon;               // hash, weapon the cop player used to drain the last of the endurance, causing the incapacitation
		int m_copVehicle;              // hash, the vehicle the cop was in at the time; null if not applicable
		bool m_copKiller;              // whether the crook was branded a cop killer at the time of the incapacitation
		int m_wantedLvl;               // crook's wanted level
	};

	// triggers for a crook after their endurance has been completely drained and they became incapacitated
	void CommandPlaystatsCncIncapacitate(CncIncapacitateInfo* data)
	{
		MetricCNC_INCAPACITATE m(data);
		APPEND_METRIC(m);
	}

	/////////////////////////////////////////////////////////////////////////////// CNC_JUSTICE
	class MetricCNC_JUSTICE : public MetricPlayStat
	{
		RL_DECLARE_METRIC(CNC_JUSTICE, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

	public:

		MetricCNC_JUSTICE(CncJusticeInfo* data)
		{
			const u64 mid0 = ((u64)data->m_matchId1.Int) << 32;
			const u64 mid1 = ((u64)data->m_matchId2.Int);

			m_matchId = mid0 | mid1;
			m_roundNumber = data->m_roundNumber.Int;
			m_stage = data->m_stage.Int;
			m_crookId = data->m_crookId.Int;
			m_crookRole = data->m_crookRole.Int;
			m_crookProgRank = data->m_crookProgRank.Int;
			m_copRole = data->m_copRole.Int;
			m_copProgRank = data->m_copProgRank.Int;
			m_crookEndurance = data->m_crookEndurance.Int;
			m_copKiller = data->m_copKiller.Int != 0;
			m_cashLost = data->m_cashLost.Int;
			m_wantedLvl = data->m_wantedLvl.Int;
			m_cashPenalty = data->m_cashPenalty.Int;
			m_type = data->m_type.Int;
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);
			result &= rw->WriteInt64("a", m_matchId);
			result &= rw->WriteInt("b", m_roundNumber);
			result &= rw->WriteInt("c", m_stage);
			result &= rw->WriteInt("d", m_crookId);
			result &= rw->WriteInt("e", m_crookRole);
			result &= rw->WriteInt("f", m_crookProgRank);
			result &= rw->WriteInt("g", m_copRole);
			result &= rw->WriteInt("h", m_copProgRank);
			result &= rw->WriteInt("i", m_crookEndurance);
			result &= rw->WriteBool("j", m_copKiller);
			result &= rw->WriteInt("k", m_cashLost);
			result &= rw->WriteInt("l", m_wantedLvl);
			result &= rw->WriteInt("m", m_cashPenalty);
			result &= rw->WriteInt("n", m_type);
			return result;
		}

	private:

		u64 m_matchId;                 // unique ID for the match, used to connect the two rounds and other metrics
		int m_roundNumber;             // round number
		int m_stage;                   // potentially hash; "Stash the Cash" or "the Escape"
		int m_crookId;                 // crook account ID, from SCAdmin
		int m_crookRole;               // hash, crook's role
		int m_crookProgRank;           // CnC progression rank for crook
		int m_copRole;                 // hash, cop's role
		int m_copProgRank;             // CnC progression rank for cop
		int m_crookEndurance;          // crook's remaining endurance, at the time of the arrest or kill
		bool m_copKiller;              // whether the crook was branded a cop killer at the time of the arrest
		int m_cashLost;                // amount of cash the crook lost
		int m_wantedLvl;               // crook's wanted level
		int m_cashPenalty;             // amount of cash the cop was penalized
		int m_type;                    // potentially hash; "Arrest" or "Kill"
	};

	// triggers after a cop makes an arrest or kills a crook
	void CommandPlaystatsCncJustice(CncJusticeInfo* data)
	{
		MetricCNC_JUSTICE m(data);
		APPEND_METRIC(m);
	}

	/////////////////////////////////////////////////////////////////////////////// CNC_WORK
	class MetricCNC_WORK : public MetricPlayStat
	{
		RL_DECLARE_METRIC(CNC_WORK, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

	public:

		MetricCNC_WORK(CncWorkInfo* data)
		{
			const u64 mid0 = ((u64)data->m_matchId1.Int) << 32;
			const u64 mid1 = ((u64)data->m_matchId2.Int);

			m_matchId = mid0 | mid1;
			m_jobId = data->m_jobId.Int;
			m_roundNumber = data->m_roundNumber.Int;
			m_role = data->m_role.Int;
			m_workType = data->m_workType.Int;
			m_workName = data->m_workName.Int;
			m_endReason = data->m_endReason.Int;
			m_dropOff = data->m_dropOff.Int;
			m_amount = data->m_amount.Int;
			m_bonus = data->m_bonus.Int != 0;
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);
			result &= rw->WriteInt64("a", m_matchId);
			result &= rw->WriteInt("b", m_jobId);
			result &= rw->WriteInt("c", m_roundNumber);
			result &= rw->WriteInt("d", m_role);
			result &= rw->WriteInt("e", m_workType);
			result &= rw->WriteInt("f", m_workName);
			result &= rw->WriteInt("g", m_endReason);
			result &= rw->WriteInt("h", m_dropOff);
			result &= rw->WriteInt("i", m_amount);
			result &= rw->WriteBool("j", m_bonus);
			return result;
		}

	private:

		u64 m_matchId;                 // unique ID for the match, used to connect the two rounds and other metrics
		int m_jobId;                   // ID to match more than one player completing the same job; null if not applicable
		int m_roundNumber;             // round number
		int m_role;                    // hash, player's role
		int m_workType;                // hash, street crime or job
		int m_workName;                // hash, name of the street crime or job
		int m_endReason;               // end reason: complete, died, arrested, time ran out
		int m_dropOff;                 // drop off location
		int m_amount;                  // money earned
		bool m_bonus;                  // true if this was the last package dropped of a set, resulting in a cash bonus
	};

	// triggers after a criminal activity is completed; should also trigger on jobs after a package is dropped, since presumably you could stop after one package and walk away
	void CommandPlaystatsCncWork(CncWorkInfo* data)
	{
		MetricCNC_WORK m(data);
		APPEND_METRIC(m);
	}

	/////////////////////////////////////////////////////////////////////////////// CNC_SWITCH
	class MetricCNC_SWITCH : public MetricPlayStat
	{
		RL_DECLARE_METRIC(CNC_SWITCH, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

	public:
		static const int m_newWeaponsArraySize = 9;
		int m_newWeapons[m_newWeaponsArraySize]; // hash array, new weapons: primary lethal weapon, secondary lethal weapon, throwable, melee, non lethal; need to take into account modkits

		static const int m_newClothingArraySize = 2;
		int m_newClothing[m_newClothingArraySize]; // hash array, new clothing: outfit, mask

		static const int m_newTattoosArraySize = 9;
		int m_newTattoos[m_newTattoosArraySize]; // hash array, new tattoos: Head, Left Arm, Right Arm, Torso - Chest, Torso - Stomach, Torso - Back, Right Leg, Left Leg, Torso - Side

		static const int m_newVehicleArraySize = 3;
		int m_newVehicle[m_newVehicleArraySize]; // hash array: new vehicle, livery, modkit

		MetricCNC_SWITCH(CncSwitchInfo* data)
		{
			const u64 mid0 = ((u64)data->m_matchId1.Int) << 32;
			const u64 mid1 = ((u64)data->m_matchId2.Int);

			m_matchId = mid0 | mid1;
			m_roundNumber = data->m_roundNumber.Int;
			m_prevRole = data->m_prevRole.Int;
			m_newRole = data->m_newRole.Int;
			m_newEmote = data->m_newEmote.Int;
			m_slot = data->m_slot.Int;
			m_cashLost = data->m_cashLost.Int;
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);
			result &= rw->WriteInt64("a", m_matchId);
			result &= rw->WriteInt("b", m_roundNumber);
			result &= rw->WriteInt("c", m_prevRole);
			result &= rw->WriteInt("d", m_newRole);

			result = result && rw->BeginArray("e", NULL);
			for (int i = 0; i < m_newWeaponsArraySize; i++)
			{
				result = result && rw->WriteInt(NULL, m_newWeapons[i]);
			}
			result = result && rw->End();

			result = result && rw->BeginArray("f", NULL);
			for (int i = 0; i < m_newClothingArraySize; i++)
			{
				result = result && rw->WriteInt(NULL, m_newClothing[i]);
			}
			result = result && rw->End();

			result = result && rw->BeginArray("g", NULL);
			for (int i = 0; i < m_newTattoosArraySize; i++)
			{
				result = result && rw->WriteInt(NULL, m_newTattoos[i]);
			}
			result = result && rw->End();

			result &= rw->WriteInt("h", m_newEmote);

			result = result && rw->BeginArray("i", NULL);
			for (int i = 0; i < m_newVehicleArraySize; i++)
			{
				result = result && rw->WriteInt(NULL, m_newVehicle[i]);
			}
			result = result && rw->End();

			result &= rw->WriteInt("j", m_slot);
			result &= rw->WriteInt("k", m_cashLost);
			return result;
		}

	private:

		u64 m_matchId;                 // unique ID for the match, used to connect the two rounds and other metrics
		int m_roundNumber;             // round number
		int m_prevRole;                // hash, previous role
		int m_newRole;                 // hash, new role
		int m_newEmote;                // hash, emote
		int m_slot;                    // hash, which slot was the new loadout stored in: either preset/custom, or slot number, depending on how this is set up
		int m_cashLost;                // cash lost when switching
	};

	// triggers after switching roles or loadout, in the Stash the Cash stage
	void CommandPlaystatsCncSwitch(CncSwitchInfo* data, scrArrayBaseAddr& newWeapons, scrArrayBaseAddr& newClothing, scrArrayBaseAddr& newTattoos, scrArrayBaseAddr& newVehicle)
	{
		MetricCNC_SWITCH m(data);

		int numNewWeaponsItems = scrArray::GetCount(newWeapons);
		scrValue* pNewWeaponsArray = scrArray::GetElements<scrValue>(newWeapons);
		scriptAssertf(m.m_newWeaponsArraySize >= numNewWeaponsItems, "%s - PLAYSTATS_CNC_SWITCH: newWeapons: Too many entries (%d). The metric only supports %d entries, ask code to bump it.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), numNewWeaponsItems, m.m_newWeaponsArraySize);

		for (int i = 0; i < m.m_newWeaponsArraySize && i < numNewWeaponsItems; i++)
		{
			m.m_newWeapons[i] = pNewWeaponsArray[i].Int;
		}

		int numNewClothingItems = scrArray::GetCount(newClothing);
		scrValue* pNewClothingArray = scrArray::GetElements<scrValue>(newClothing);
		scriptAssertf(m.m_newClothingArraySize >= numNewClothingItems, "%s - PLAYSTATS_CNC_SWITCH: newClothing: Too many entries (%d). The metric only supports %d entries, ask code to bump it.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), numNewClothingItems, m.m_newClothingArraySize);

		for (int i = 0; i < m.m_newClothingArraySize && i < numNewClothingItems; i++)
		{
			m.m_newClothing[i] = pNewClothingArray[i].Int;
		}

		int numNewTattoosItems = scrArray::GetCount(newTattoos);
		scrValue* pNewTattoosArray = scrArray::GetElements<scrValue>(newTattoos);
		scriptAssertf(m.m_newTattoosArraySize >= numNewTattoosItems, "%s - PLAYSTATS_CNC_SWITCH: newTattoos: Too many entries (%d). The metric only supports %d entries, ask code to bump it.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), numNewTattoosItems, m.m_newTattoosArraySize);

		for (int i = 0; i < m.m_newTattoosArraySize && i < numNewTattoosItems; i++)
		{
			m.m_newTattoos[i] = pNewTattoosArray[i].Int;
		}

		int numNewVehicleItems = scrArray::GetCount(newVehicle);
		scrValue* pNewVehicleArray = scrArray::GetElements<scrValue>(newVehicle);
		scriptAssertf(m.m_newVehicleArraySize >= numNewVehicleItems, "%s - PLAYSTATS_CNC_SWITCH: newVehicle: Too many entries (%d). The metric only supports %d entries, ask code to bump it.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), numNewVehicleItems, m.m_newVehicleArraySize);

		for (int i = 0; i < m.m_newVehicleArraySize && i < numNewVehicleItems; i++)
		{
			m.m_newVehicle[i] = pNewVehicleArray[i].Int;
		}

		APPEND_METRIC(m);
	}

	/////////////////////////////////////////////////////////////////////////////// CNC_JOBS
	class MetricCNC_JOBS : public MetricPlayStat
	{
		RL_DECLARE_METRIC(CNC_JOBS, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

	public:
		static const int m_jobsArraySize = 16;
		int m_jobs[m_jobsArraySize];	// array of jobs that spawned during the round, in the order they spawned

		MetricCNC_JOBS(const int roundNumber, const int matchId1, const int matchId2)
		{
			m_roundNumber = roundNumber;

			const u64 mid0 = ((u64)matchId1) << 32;
			const u64 mid1 = ((u64)matchId2);

			m_matchId = mid0 | mid1;
			
			
			// Initialise array to -1, eCNC_JOB_POINT_INVALID
			for (int i = 0; i < m_jobsArraySize; i++)
			{
				m_jobs[i] = -1;
			}
			
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);
			result &= rw->WriteInt("a", m_roundNumber);
			result &= rw->WriteInt64("b", m_matchId);
			
			result = result && rw->BeginArray("c", NULL);
			for (int i = 0; i < m_jobsArraySize; i++)
			{
				result = result && rw->WriteInt(NULL, m_jobs[i]);
			}
			result = result && rw->End();
		
			return result;
		}

	private:

		int m_roundNumber;             // round number
		u64 m_matchId;                 // unique ID for the match, used to connect the two rounds and other metrics
	};

	// triggers at the start of CNC script
	void CommandPlaystatsCncJobs(const int roundNumber, const int matchId1, const int matchId2, scrArrayBaseAddr& jobs)
	{
		MetricCNC_JOBS m(roundNumber, matchId1, matchId2);
		
		int numJobsItems = scrArray::GetCount(jobs);
		scrValue* pJobsArray = scrArray::GetElements<scrValue>(jobs);
		scriptAssertf(m.m_jobsArraySize >= numJobsItems, "%s - PLAYSTATS_CNC_JOBS: jobs: Too many entries (%d). The metric only supports %d entries, ask code to bump it.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), numJobsItems, m.m_jobsArraySize);
		
		for (int i = 0; i < m.m_jobsArraySize && i < numJobsItems; i++)
		{
			m.m_jobs[i] = pJobsArray[i].Int;
		}
		APPEND_METRIC(m);
	}

	/////////////////////////////////////////////////////////////////////////////// 
#endif

	/////////////////////////////////////////////////////////////////////////////// ISLAND HEIST


/////////////////////////////////////////////////////////////////////////////// HEIST4_PREP
	class MetricHEIST4_PREP : public MetricPlayStat
	{
		RL_DECLARE_METRIC(HEIST4_PREP, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

	public:

		MetricHEIST4_PREP(Heist4PrepInfo* data)
		{
			const u64 bid0 = ((u64)data->m_bossId1.Int) << 32;
			const u64 bid1 = (0x00000000FFFFFFFF & (u64)data->m_bossId2.Int);
			const u64 localbossId = StatsInterface::GetUInt64Stat(StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID"));

			m_playthroughId = data->m_playthroughId.Int;
			m_missionName = data->m_missionName.Int;
			m_sessionId = data->m_sessionId.Int;
			m_hashmac = data->m_hashmac.Int;
			m_playCount = data->m_playCount.Int;
			m_type = data->m_type.Int;
			m_bossId = bid0 | bid1;
			m_isBoss = (m_bossId != 0 && m_bossId == localbossId);
			m_bosstype = data->m_bosstype.Int;
			m_launcherRank = data->m_launcherRank.Int;
			m_playerRole = data->m_playerRole.Int;
			m_playerParticipated = data->m_playerParticipated.Int != 0;
			m_difficult = data->m_difficult.Int != 0;
			m_timeTakenToComplete = data->m_timeTakenToComplete.Int;
			m_endingReason = data->m_endingReason.Int;
			m_isRivalParty = data->m_isRivalParty.Int != 0;
			m_cashEarned = data->m_cashEarned.Int;
			m_rpEarned = data->m_rpEarned.Int;
			m_location = data->m_location.Int;
			m_invitesSent = data->m_invitesSent.Int;
			m_invitesAccepted = data->m_invitesAccepted.Int;
			m_deaths = data->m_deaths.Int;
			m_targetsKilled = data->m_targetsKilled.Int;
			m_innocentsKilled = data->m_innocentsKilled.Int;
			m_mandatory = data->m_mandatory.Int != 0;
			m_primTar = data->m_primTar.Int;
			m_properties = data->m_properties.Int;
			m_entrances = data->m_entrances.Int;
			m_approaches = data->m_approaches.Int;
			m_grappleLoc = data->m_grappleLoc.Int;
			m_uniLoc = data->m_uniLoc.Int;
			m_cutterLoc = data->m_cutterLoc.Int;
			m_secLootLoc = data->m_secLootLoc.Int;
			m_miscActions = data->m_miscActions.Int;
			m_properties2 = data->m_properties2.Int;
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);
			result &= rw->WriteInt("a", m_playthroughId);
			result &= rw->WriteInt("b", m_missionName);
			result &= rw->WriteInt("c", m_sessionId);
			result &= rw->WriteInt("d", m_hashmac);
			result &= rw->WriteInt("e", m_playCount);
			result &= rw->WriteInt("f", m_type);
			result &= rw->WriteUns64("g", m_bossId);
			result &= rw->WriteBool("h", m_isBoss);
			result &= rw->WriteInt("i", m_bosstype);
			result &= rw->WriteInt("j", m_launcherRank);
			result &= rw->WriteInt("k", m_playerRole);
			result &= rw->WriteBool("l", m_playerParticipated);
			result &= rw->WriteBool("m", m_difficult);
			result &= rw->WriteInt("n", m_timeTakenToComplete);
			result &= rw->WriteInt("o", m_endingReason);
			result &= rw->WriteBool("p", m_isRivalParty);
			result &= rw->WriteInt("q", m_cashEarned);
			result &= rw->WriteInt("r", m_rpEarned);
			result &= rw->WriteInt("s", m_location);
			result &= rw->WriteInt("t", m_invitesSent);
			result &= rw->WriteInt("u", m_invitesAccepted);
			result &= rw->WriteInt("v", m_deaths);
			result &= rw->WriteInt("w", m_targetsKilled);
			result &= rw->WriteInt("x", m_innocentsKilled);
			result &= rw->WriteBool("y", m_mandatory);
			result &= rw->WriteInt("z", m_primTar);
			result &= rw->WriteInt("aa", m_properties);
			result &= rw->WriteInt("ab", m_entrances);
			result &= rw->WriteInt("ac", m_approaches);
			result &= rw->WriteInt("ad", m_grappleLoc);
			result &= rw->WriteInt("ae", m_uniLoc);
			result &= rw->WriteInt("af", m_cutterLoc);
			result &= rw->WriteInt("ag", m_secLootLoc);
			result &= rw->WriteInt("ah", m_miscActions);
			result &= rw->WriteInt("ai", m_properties2);
			return result;
		}

	private:

		int m_playthroughId;           // playthrough ID, this should appear in all related content, like scopes, preps, finales and minigames, and would enable us to track an entire heist playthrough
		int m_missionName;             // mission name
		int m_sessionId;               // session ID
		int m_hashmac;                 // hash mac
		int m_playCount;               // number of preps completed
		int m_type;                    // setup/scope, vehicle prep, equipment prep, weapon prep, disruption prep
		u64 m_bossId;                  // unique ID of the Boss instance
		bool m_isBoss;                 // whether this player is the leader
		int m_bosstype;                // type of org: Securoserv, MC, unaffiliated
		int m_launcherRank;            // rank of the player launching
		int m_playerRole;              // similar to previous metrics, the player's role: CEO, bodyguard, biker prospect etc.
		bool m_playerParticipated;     // whether the player participated
		bool m_difficult;              // whether the mission has increased difficulty
		int m_timeTakenToComplete;     // time taken to complete, in seconds (for consistency)
		int m_endingReason;            // similar to previous metrics: won, lost, boss_left etc.
		bool m_isRivalParty;           // is the party completing the mission the same as the party that launched it, null if not applicable
		int m_cashEarned;              // money earned
		int m_rpEarned;                // RP earned
		int m_location;                // mission subvariation
		int m_invitesSent;             // invites sent
		int m_invitesAccepted;         // invites accepted
		int m_deaths;                  // how many times the player died
		int m_targetsKilled;           // number of mission critical/aggressive peds killed
		int m_innocentsKilled;         // number of innocent peds killed
		bool m_mandatory;              // true if this is a mandatory mission, false if it's optional
		int m_primTar;                 // what is the primary target: files, bearer bonds, diamonds etc.; null if not applicable to this mission
		int m_properties;              // bitset of properties owned: casino penthouse, gunrunning bunker, smuggler hangar, terrorbyte, submarine etc.
		int m_entrances;               // bitset of entrances scoped: main gate, side gate left, side gate right etc.; null if not applicable to this mission
		int m_approaches;              // bitset of approaches unlocked: Stealth Chopper ? North Drop Zone, Gun Boat ? Main Docks etc.; null if not applicable to this mission
		int m_grappleLoc;              // bitset of grapple equipment locations scoped; null if not applicable to this mission
		int m_uniLoc;                  // bitset of uniform locations scoped; null if not applicable to this mission
		int m_cutterLoc;               // bitset of bolt cutter locations scoped; null if not applicable to this mission
		int m_secLootLoc;              // bitset of secondary loot locations scoped: cash, weed, cocaine, gold, paintings; null if not applicable to this mission
		int m_miscActions;             // bitset to account for miscellaneous actions: poison water supply, enemies armour disruption, enemies helicopter disruption, power station scoped, trojan vehicle scoped, control tower scoped, golden gun drawer key, double key cards, acetylene torch; null if not applicable to this mission
		int m_properties2;			   // bitset of type of properties owned
	};

	// triggers after scope/setup and prep missions end for the Island Heist
	void CommandPlaystatsHeist4Prep(Heist4PrepInfo* data)
	{
		MetricHEIST4_PREP m(data);
		APPEND_METRIC(m);
	}

	/////////////////////////////////////////////////////////////////////////////// HEIST4_FINALE
	class MetricHEIST4_FINALE : public MetricPlayStat
	{
		RL_DECLARE_METRIC(HEIST4_FINALE, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

	public:

		MetricHEIST4_FINALE(Heist4FinaleInfo* data)
		{
			const u64 bid0 = ((u64)data->m_bossId1.Int) << 32;
			const u64 bid1 = (0x00000000FFFFFFFF & (u64)data->m_bossId2.Int);
			const u64 localbossId = StatsInterface::GetUInt64Stat(StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID"));

			m_playthroughId = data->m_playthroughId.Int;
			m_missionId = data->m_missionId.Int;
			m_sessionId = data->m_sessionId.Int;
			safecpy(m_publicContentId, data->m_publicContentId);
			m_hashmac = data->m_hashmac.Int;
			m_bossId = bid0 | bid1;
			m_isBoss = (m_bossId != 0 && m_bossId == localbossId);
			m_bosstype = data->m_bosstype.Int;
			m_launcherRank = data->m_launcherRank.Int;
			m_playerRole = data->m_playerRole.Int;
			m_endingReason = data->m_endingReason.Int;
			m_replay = data->m_replay.Int;
			m_rpEarned = data->m_rpEarned.Int;
			m_difficult = data->m_difficult.Int != 0;
			m_timeTakenToComplete = data->m_timeTakenToComplete.Int;
			m_checkpoint = data->m_checkpoint.Int;
			m_playCount = data->m_playCount.Int;
			m_deaths = data->m_deaths.Int;
			m_targetsKilled = data->m_targetsKilled.Int;
			m_innocentsKilled = data->m_innocentsKilled.Int;
			m_properties = data->m_properties.Int;
			m_properties2 = data->m_properties2.Int;
			m_percentage = data->m_percentage.Int;
			m_tod = data->m_tod.Int;
			m_wLoadout = data->m_wLoadout.Int;
			m_outfits = data->m_outfits.Int;
			m_suppressors = data->m_suppressors.Int != 0;
			m_supCrewBoard = data->m_supCrewBoard.Int;
			m_islApproach = data->m_islApproach.Int;
			m_islEscBoard = data->m_islEscBoard.Int;
			m_primTar = data->m_primTar.Int;
			m_weather = data->m_weather.Int;
			m_weapDisrup = data->m_weapDisrup.Int;
			m_supCrewActual = data->m_supCrewActual.Int;
			m_miscActions = data->m_miscActions.Int;
			m_compEntrance = data->m_compEntrance.Int;
			m_primTarEntrance = data->m_primTarEntrance.Int;
			m_compExit = data->m_compExit.Int;
			m_interestItemUsed = data->m_interestItemUsed.Int;
			m_islEscActual = data->m_islEscActual.Int;
			m_failedStealth = data->m_failedStealth.Int != 0;
			m_specCam = data->m_specCam.Int != 0;
			m_moneyEarned = data->m_moneyEarned.Int;
			m_secLoot = data->m_secLoot.Int;
			m_secTarCash = data->m_secTarCash.Int;
			m_secTarCocaine = data->m_secTarCocaine.Int;
			m_secTarWeed = data->m_secTarWeed.Int;
			m_secTarGold = data->m_secTarGold.Int;
			m_secTarPaint = data->m_secTarPaint.Int;
			m_bagCapacity = data->m_bagCapacity.Int;
			m_CashBlocksStolen = data->m_CashBlocksStolen.Int;
			m_CashValueFinal = data->m_CashValueFinal.Int;
			m_CashBlocksFinal = data->m_CashBlocksFinal.Int;
			m_CashBlocksSpace = data->m_CashBlocksSpace.Int;
			m_CocaBlocksStolen = data->m_CocaBlocksStolen.Int;
			m_CocaValueFinal = data->m_CocaValueFinal.Int;
			m_CocaBlocksFinal = data->m_CocaBlocksFinal.Int;
			m_CocaBlocksSpace = data->m_CocaBlocksSpace.Int;
			m_WeedBlocksStolen = data->m_WeedBlocksStolen.Int;
			m_WeedValueFinal = data->m_WeedValueFinal.Int;
			m_WeedBlocksFinal = data->m_WeedBlocksFinal.Int;
			m_WeedBlocksSpace = data->m_WeedBlocksSpace.Int;
			m_GoldBlocksStolen = data->m_GoldBlocksStolen.Int;
			m_GoldValueFinal = data->m_GoldValueFinal.Int;
			m_GoldBlocksFinal = data->m_GoldBlocksFinal.Int;
			m_GoldBlocksSpace = data->m_GoldBlocksSpace.Int;
			m_PaintBlocksStolen = data->m_PaintBlocksStolen.Int;
			m_PaintValueFinal = data->m_PaintValueFinal.Int;
			m_PaintBlocksFinal = data->m_PaintBlocksFinal.Int;
			m_PaintBlocksSpace = data->m_PaintBlocksSpace.Int;
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);
			result &= rw->WriteInt("a", m_playthroughId);
			result &= rw->WriteInt("b", m_missionId);
			result &= rw->WriteInt("c", m_sessionId);
			result &= rw->WriteString("d", m_publicContentId);
			result &= rw->WriteInt("e", m_hashmac);
			result &= rw->WriteUns64("f", m_bossId);
			result &= rw->WriteBool("g", m_isBoss);
			result &= rw->WriteInt("h", m_bosstype);
			result &= rw->WriteInt("i", m_launcherRank);
			result &= rw->WriteInt("j", m_playerRole);
			result &= rw->WriteInt("k", m_endingReason);
			result &= rw->WriteInt("l", m_replay);
			result &= rw->WriteInt("m", m_rpEarned);
			result &= rw->WriteBool("n", m_difficult);
			result &= rw->WriteInt("o", m_timeTakenToComplete);
			result &= rw->WriteInt("p", m_checkpoint);
			result &= rw->WriteInt("q", m_playCount);
			result &= rw->WriteInt("r", m_deaths);
			result &= rw->WriteInt("s", m_targetsKilled);
			result &= rw->WriteInt("t", m_innocentsKilled);
			result &= rw->WriteInt("u", m_properties);
			result &= rw->WriteInt("v", m_properties2);
			result &= rw->WriteInt("w", m_percentage);
			result &= rw->WriteInt("x", m_tod);
			result &= rw->WriteInt("y", m_wLoadout);
			result &= rw->WriteInt("z", m_outfits);
			result &= rw->WriteBool("aa", m_suppressors);
			result &= rw->WriteInt("ab", m_supCrewBoard);
			result &= rw->WriteInt("ac", m_islApproach);
			result &= rw->WriteInt("ad", m_islEscBoard);
			result &= rw->WriteInt("ae", m_primTar);
			result &= rw->WriteInt("af", m_weather);
			result &= rw->WriteInt("ag", m_weapDisrup);
			result &= rw->WriteInt("ah", m_supCrewActual);
			result &= rw->WriteInt("ai", m_miscActions);
			result &= rw->WriteInt("aj", m_compEntrance);
			result &= rw->WriteInt("ak", m_primTarEntrance);
			result &= rw->WriteInt("al", m_compExit);
			result &= rw->WriteInt("am", m_interestItemUsed);
			result &= rw->WriteInt("an", m_islEscActual);
			result &= rw->WriteBool("ao", m_failedStealth);
			result &= rw->WriteBool("ap", m_specCam);
			result &= rw->WriteInt("aq", m_moneyEarned);
			result &= rw->WriteInt("ar", m_secLoot);
			result &= rw->WriteInt("as", m_secTarCash);
			result &= rw->WriteInt("at", m_secTarCocaine);
			result &= rw->WriteInt("au", m_secTarWeed);
			result &= rw->WriteInt("av", m_secTarGold);
			result &= rw->WriteInt("aw", m_secTarPaint);
			result &= rw->WriteInt("ax", m_bagCapacity);

			result &= rw->BeginArray("ay", nullptr);
			result &= rw->WriteInt(nullptr, m_CashBlocksStolen);
			result &= rw->WriteInt(nullptr, m_CashValueFinal);
			result &= rw->WriteInt(nullptr, m_CashBlocksFinal);
			result &= rw->WriteInt(nullptr, m_CashBlocksSpace);
			result &= rw->End();
			result &= rw->BeginArray("az", nullptr);
			result &= rw->WriteInt(nullptr, m_CocaBlocksStolen);
			result &= rw->WriteInt(nullptr, m_CocaValueFinal);
			result &= rw->WriteInt(nullptr, m_CocaBlocksFinal);
			result &= rw->WriteInt(nullptr, m_CocaBlocksSpace);
			result &= rw->End();
			result &= rw->BeginArray("ba", nullptr);
			result &= rw->WriteInt(nullptr, m_WeedBlocksStolen);
			result &= rw->WriteInt(nullptr, m_WeedValueFinal);
			result &= rw->WriteInt(nullptr, m_WeedBlocksFinal);
			result &= rw->WriteInt(nullptr, m_WeedBlocksSpace);
			result &= rw->End();
			result &= rw->BeginArray("bb", nullptr);
			result &= rw->WriteInt(nullptr, m_GoldBlocksStolen);
			result &= rw->WriteInt(nullptr, m_GoldValueFinal);
			result &= rw->WriteInt(nullptr, m_GoldBlocksFinal);
			result &= rw->WriteInt(nullptr, m_GoldBlocksSpace);
			result &= rw->End();
			result &= rw->BeginArray("bc", nullptr);
			result &= rw->WriteInt(nullptr, m_PaintBlocksStolen);
			result &= rw->WriteInt(nullptr, m_PaintValueFinal);
			result &= rw->WriteInt(nullptr, m_PaintBlocksFinal);
			result &= rw->WriteInt(nullptr, m_PaintBlocksSpace);
			result &= rw->End();
			return result;
		}

	private:

		int m_playthroughId;           // playthrough ID, this should appear in all related content, like scopes, preps, finales and minigames, and would enable us to track an entire heist playthrough
		int m_missionId;               // mission name
		int m_sessionId;               // session ID
		char m_publicContentId[MetricPlayStat::MAX_STRING_LENGTH]; // public content ID
		int m_hashmac;                 // hash mac
		u64 m_bossId;                  // unique ID of the Boss instance
		bool m_isBoss;                 // whether this player is the leader
		int m_bosstype;                // type of org: Securoserv, MC, unaffiliated
		int m_launcherRank;            // rank of the player launching
		int m_playerRole;              // similar to previous metrics, the player's role: CEO, bodyguard, biker prospect etc.
		int m_endingReason;            // similar to previous metrics: won, lost, boss_left etc.
		int m_replay;                  // counts how many times this mission was failed, then continued from checkpoint/restarted; resets after the player exits the mission, null if not applicable
		int m_rpEarned;                // RP earned
		bool m_difficult;              // whether the mission has increased difficulty
		int m_timeTakenToComplete;     // time taken to complete, in seconds (for consistency)
		int m_checkpoint;              // what checkpoint was the heist failed at; null if not applicable
		int m_playCount;               // how many times has the player completed this mission before; first time is 1, then increments of 1 after completing finale, only success matters
		int m_deaths;                  // how many times the player died
		int m_targetsKilled;           // number of mission critical/aggressive peds killed
		int m_innocentsKilled;         // number of innocent peds killed
		int m_properties;              // bitset of type of properties owned: casino penthouse, gunrunning bunker, smuggler hangar, terrorbyte, submarine etc.
		int m_properties2;             // bitset of type of properties owned; future proofing
		int m_percentage;              // heist percentage of payout
		int m_tod;                     // time of day chosen (day, night)
		int m_wLoadout;                // weapon loadout the player has picked on the board
		int m_outfits;                 // outfits chosen on the board
		bool m_suppressors;            // whether suppressors were selected on the board
		int m_supCrewBoard;            // bitset of support crews selected on the planning board: airstrike, heavy weapon drop, sniper, air support, spy drone, weapon stash
		int m_islApproach;             // which approach the players took to infiltrate the island: Stealth Chopper ? North Drop Zone, Gun Boat ? Main Docks etc.
		int m_islEscBoard;             // which approach the players chose on the board to escape the island: airstrip, main docks etc.
		int m_primTar;                 // what is the primary target: files, bearer bonds, diamonds etc.
		int m_weather;                 // type of weather during the heist
		int m_weapDisrup;              // level of enemy weapon disruption: none, low, medium, high
		int m_supCrewActual;           // bitset of support crews used during the mission: airstrike, heavy weapon drop, sniper, air support, spy drone, weapon stash
		int m_miscActions;             // bitset for miscellaneous actions in the finale: whether the player picked up the golden gun, whether the player picked up the SPAS 12, whether the player looted El Rubio's safe, whether the radar tower has been disabled
		int m_compEntrance;            // which compound entrance the players used: main gate explosion, side gate left, side gate right, rappel left etc.
		int m_primTarEntrance;         // which entrance the players chose to get to the primary target: elevator, underground entrance 1, underground entrance 1
		int m_compExit;                // which exit from the compound the players used: main gate, side gate left etc.
		int m_interestItemUsed;        // bitset of equipment used during the finale, on the island: explosives, bolt cutters, side gate codes, trojan horse vehicle, uniform, rappel equipment, jailor keys, glass cutter, double keycard
		int m_islEscActual;            // which approach the players actually used to escape the island: airstrip, main docks etc.
		bool m_failedStealth;          // whether the player failed stealth and alerted the enemies
		bool m_specCam;                // was this player in spectator cam when the mission ended
		int m_moneyEarned;             // total amount of money earned from the heist
		int m_secLoot;                 // bitset of secondary loot locations that were hit, minimum of one block stolen: cash, weed, cocaine, gold, paintings
		int m_secTarCash;              // array for secondary target cash: number of blocks stolen, money value at the end of the mission, number of blocks at the end of the mission, set block value in the bag
		int m_secTarCocaine;           // array for secondary target cocaine: number of blocks stolen, money value at the end of the mission, number of blocks at the end of the mission, set block value in the bag
		int m_secTarWeed;              // array for secondary target weed: number of blocks stolen, money value at the end of the mission, number of blocks at the end of the mission, set block value in the bag
		int m_secTarGold;              // array for secondary target gold: number of blocks stolen, money value at the end of the mission, number of blocks at the end of the mission, set block value in the bag
		int m_secTarPaint;             // array for secondary target paintings: number of blocks stolen, money value at the end of the mission, number of blocks at the end of the mission, set block value in the bag
		int m_bagCapacity;             // player's total bag capacity
		int m_CashBlocksStolen;        // number of blocks of cash stolen
		int m_CashValueFinal;          // money value of cash stolen at the end of the mission 
		int m_CashBlocksFinal;         // number of blocks of cash held at the end of the mission
		int m_CashBlocksSpace;         // set block value in the bag for cash
		int m_CocaBlocksStolen;        // number of blocks of cocaine stolen
		int m_CocaValueFinal;          // money value of cocaine stolen at the end of the mission 
		int m_CocaBlocksFinal;         // number of blocks of cocaine held at the end of the mission
		int m_CocaBlocksSpace;         // set block value in the bag for cocaine
		int m_WeedBlocksStolen;        // number of blocks of weed stolen
		int m_WeedValueFinal;          // money value of weed stolen at the end of the mission 
		int m_WeedBlocksFinal;         // number of blocks of weed held at the end of the mission
		int m_WeedBlocksSpace;         // set block value in the bag for weed
		int m_GoldBlocksStolen;        // number of blocks of gold stolen
		int m_GoldValueFinal;          // money value of gold stolen at the end of the mission 
		int m_GoldBlocksFinal;         // number of blocks of gold held at the end of the mission
		int m_GoldBlocksSpace;         // set block value in the bag for gold
		int m_PaintBlocksStolen;       // number of blocks of paintings stolen
		int m_PaintValueFinal;         // money value of paintings stolen at the end of the mission 
		int m_PaintBlocksFinal;        // number of blocks of paintings held at the end of the mission
		int m_PaintBlocksSpace;        // set block value in the bag for paintings
	};

	// triggers after the end of Island Heist finale missions
	void CommandPlaystatsHeist4Finale(Heist4FinaleInfo* data)
	{
		MetricHEIST4_FINALE m(data);
		APPEND_METRIC(m);
	}

	/////////////////////////////////////////////////////////////////////////////// HEIST4_HACK
	class MetricHEIST4_HACK : public MetricPlayStat
	{
		RL_DECLARE_METRIC(HEIST4_HACK, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

	public:

		MetricHEIST4_HACK(const int playthroughId, const int minigame, const bool won, const int step, const int time)
		{
			m_playthroughId = playthroughId;
			m_minigame = minigame;
			m_won = won;
			m_step = step;
			m_time = time;
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);
			result &= rw->WriteInt("a", m_playthroughId);
			result &= rw->WriteInt("b", m_minigame);
			result &= rw->WriteBool("c", m_won);
			result &= rw->WriteInt("d", m_step);
			result &= rw->WriteInt("e", m_time);
			return result;
		}

	private:

		int m_playthroughId;           // playthrough ID, this should appear in all related content, like scopes, preps, finales and minigames, and would enable us to track an entire heist playthrough
		int m_minigame;                // fingerprint clone, voltage minigame
		bool m_won;                    // whether the player won
		int m_step;                    // what step did the player fail on, null if not applicable
		int m_time;                    // time spent on the minigame
	};

	// triggers at the end of the hacking minigames in the Island Heist
	void CommandPlaystatsHeist4Hack(const int playthroughId, const int minigame, const bool won, const int step, const int time)
	{
		MetricHEIST4_HACK m(playthroughId, minigame, won, step, time);
		APPEND_METRIC(m);
	}

	/////////////////////////////////////////////////////////////////////////////// SUB_WEAP
	class MetricSUB_WEAP : public MetricPlayStat
	{
		RL_DECLARE_METRIC(SUB_WEAP, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

	public:

		MetricSUB_WEAP(const int weapon, const bool ownSub, const int projectileCount, const int kills)
		{
			m_weapon = weapon;
			m_ownSub = ownSub;
			m_projectileCount = projectileCount;
			m_kills = kills;
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);
			result &= rw->WriteInt("a", m_weapon);
			result &= rw->WriteBool("b", m_ownSub);
			result &= rw->WriteInt("c", m_projectileCount);
			result &= rw->WriteInt("d", m_kills);
			return result;
		}

	private:

		int m_weapon;                  // bitset of which submarine weapon the player used
		bool m_ownSub;                 // is this the player's submarine
		int m_projectileCount;         // number of projectiles fired
		int m_kills;                   // number of kills, players or peds
	};

	// triggers after the player has left the console for controlling the anti-aircraft missiles, remote-guided missiles and torpedoes
	void CommandPlaystatsSubWeap(const int weapon, const bool ownSub, const int projectileCount, const int kills)
	{
		MetricSUB_WEAP m(weapon, ownSub, projectileCount, kills);
		APPEND_METRIC(m);
	}

	/////////////////////////////////////////////////////////////////////////////// FAST_TRVL
	class MetricFAST_TRVL : public MetricPlayStat
	{
		RL_DECLARE_METRIC(FAST_TRVL, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

	public:

		MetricFAST_TRVL(const int startLoc, const float startX, const float startY, const float startZ, const int endLoc, const float endX, const float endY, const float endZ, const int type)
		{
			m_startLoc = startLoc;
			m_startX = startX;
			m_startY = startY;
			m_startZ = startZ;
			m_endLoc = endLoc;
			m_endX = endX;
			m_endY = endY;
			m_endZ = endZ;
			m_type = type;
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);
			result &= rw->WriteInt("a", m_startLoc);
			result &= rw->WriteFloat("b", m_startX);
			result &= rw->WriteFloat("c", m_startY);
			result &= rw->WriteFloat("d", m_startZ);
			result &= rw->WriteInt("e", m_endLoc);
			result &= rw->WriteFloat("f", m_endX);
			result &= rw->WriteFloat("g", m_endY);
			result &= rw->WriteFloat("h", m_endZ);
			result &= rw->WriteInt("i", m_type);
			return result;
		}

	private:

		int m_startLoc;                // location where the vehicle currently is (this might not be possible)
		float m_startX;                // start x position of vehicle
		float m_startY;                // start y position of vehicle
		float m_startZ;                // start z position of vehicle
		int m_endLoc;                  // destination picked to relocate the vehicle
		float m_endX;				   // end x position of vehicle
		float m_endY;				   // end y position of vehicle
		float m_endZ;				   // end z position of vehicle
		int m_type;					   // type of fast travel (submarine, casino penthouse limo, etc.)
	};

	// triggers after the player uses the vehicle menu to fast travel
	void CommandPlaystatsFastTrvl(const int startLoc, const float startX, const float startY, const float startZ, const int endLoc, const float endX, const float endY, const float endZ, const int type)
	{
		MetricFAST_TRVL m(startLoc, startX, startY, startZ, endLoc, endX, endY, endZ, type);
		APPEND_METRIC(m);
	}

	/////////////////////////////////////////////////////////////////////////////// HUB_ENTRY
	class MetricHUB_ENTRY : public MetricPlayStat
	{
		RL_DECLARE_METRIC(HUB_ENTRY, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

	public:

		MetricHUB_ENTRY(HubEntryInfo* data)
		{
			const u64 bid0 = ((u64)data->m_bossId1.Int) << 32;
			const u64 bid1 = (0x00000000FFFFFFFF & (u64)data->m_bossId2.Int);

			m_type = data->m_type.Int;
			m_properties = data->m_properties.Int;
			m_access = data->m_access.Int;
			m_cost = data->m_cost.Int;
			m_bossId = bid0 | bid1;
			m_playerRole = data->m_playerRole.Int;
			m_pulled = data->m_pulled.Int != 0;
			m_invited = data->m_invited.Int != 0;
			m_properties2 = data->m_properties2.Int;
			m_vehicle = data->m_vehicle.Int;
			m_crewId = data->m_crewId.Int;
			m_private = data->m_private.Int != 0;
			m_vehicleType = data->m_vehicleType.Int;
			m_hubId = data->m_hubId.Int;
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);
			result &= rw->WriteInt("a", m_type);
			result &= rw->WriteInt("b", m_properties);
			result &= rw->WriteInt("c", m_access);
			result &= rw->WriteInt("d", m_cost);
			result &= rw->WriteUns64("e", m_bossId);
			result &= rw->WriteInt("f", m_playerRole);
			result &= rw->WriteBool("g", m_pulled);
			result &= rw->WriteBool("h", m_invited);
			result &= rw->WriteInt("i", m_properties2);
			result &= rw->WriteInt("j", m_vehicle);
			result &= rw->WriteInt("k", m_crewId);
			result &= rw->WriteBool("l", m_private);
			result &= rw->WriteInt("m", m_vehicleType);
			result &= rw->WriteInt("n", m_hubId);
			return result;
		}

	private:

		int m_type;                    // which social hub is the player in: casino nightclub, beach party
		int m_properties;              // bitset of the properties owned
		int m_access;                  // which access route did the player use: main door, elevator etc.
		int m_cost;                    // price of entrance, should be 0 if free
		u64 m_bossId;                  // unique ID of the Boss instance
		int m_playerRole;              // similar to previous metrics, the player's role: CEO, bodyguard, biker prospect etc.
		bool m_pulled;                 // whether this player was pulled into the social hub (via IM, for example)
		bool m_invited;                // whether this player joined by accepting an invite
		int m_properties2;			   // bitset of type of properties owned
		int m_vehicle;				   // hash of the player-driven vehicle
		int m_crewId;				   // ID of the social club crew the player belongs to
		bool m_private;				   // instance of social hub is private
		int m_vehicleType;			   // hash of whether the vehicle driven is a personal vehicle, another player's personal vehicle, generic test drive vehicle or Hao's premium vehicle
		int m_hubId;				   // ID that applies to this instance of the social hub; would be the same for those players inside the social hub
	};

	// triggers after the player enters a social hub, meaning the Casino Nightclub and the beach party; does not include the After Hours Nightclub (we can look at adding this at a later date if needed)
	void CommandPlaystatsHubEntry(HubEntryInfo* data)
	{
		MetricHUB_ENTRY m(data);
		APPEND_METRIC(m);
	}
	
	/////////////////////////////////////////////////////////////////////////////// DJ_MISSION_ENDED
	class MetricDJ_MISSION_ENDED : public MetricPlayStat
	{
		RL_DECLARE_METRIC(DJ_MISSION_ENDED, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

	public:

		MetricDJ_MISSION_ENDED(DjMissionEndedInfo* data)
		{
			const u64 bid0 = ((u64)data->m_bossId1.Int) << 32;
			const u64 bid1 = (0x00000000FFFFFFFF & (u64)data->m_bossId2.Int);
			const u64 localbossId = StatsInterface::GetUInt64Stat(StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID"));


			m_missionTypeId = data->m_missionTypeId.Int;
			m_missionId = data->m_missionId.Int;
			m_matchId = data->m_matchId.Int;
			m_bossId = bid0 | bid1;
			m_isboss = (m_bossId != 0 && m_bossId == localbossId);
			m_bosstype = data->m_bosstype.Int;
			m_playerRole = data->m_playerRole.Int;
			m_launcherRank = data->m_launcherRank.Int;
			m_location = data->m_location.Int;
			m_playerParticipated = data->m_playerParticipated.Int;
			m_won = data->m_won.Int != 0;
			m_endingReason = data->m_endingReason.Int;
			m_timeTakenToComplete = data->m_timeTakenToComplete.Int;
			m_cashEarned = data->m_cashEarned.Int;
			m_rpEarned = data->m_rpEarned.Int;
			m_chipsEarned = data->m_chipsEarned.Int;
			m_itemEarned = data->m_itemEarned.Int;
			m_targetsKilled = data->m_targetsKilled.Int;
			m_innocentsKilled = data->m_innocentsKilled.Int;
			m_deaths = data->m_deaths.Int;
			m_bottlesDelivered = data->m_bottlesDelivered.Int;
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);
			result &= rw->WriteInt("a", m_missionTypeId);
			result &= rw->WriteInt("b", m_missionId);
			result &= rw->WriteInt("c", m_matchId);
			result &= rw->WriteUns64("d", m_bossId);
			result &= rw->WriteBool("e", m_isboss);
			result &= rw->WriteInt("f", m_bosstype);
			result &= rw->WriteInt("g", m_playerRole);
			result &= rw->WriteInt("h", m_launcherRank);
			result &= rw->WriteInt("i", m_location);
			result &= rw->WriteInt("j", m_playerParticipated);
			result &= rw->WriteBool("k", m_won);
			result &= rw->WriteInt("l", m_endingReason);
			result &= rw->WriteInt("m", m_timeTakenToComplete);
			result &= rw->WriteInt("n", m_cashEarned);
			result &= rw->WriteInt("o", m_rpEarned);
			result &= rw->WriteInt("p", m_chipsEarned);
			result &= rw->WriteInt("q", m_itemEarned);
			result &= rw->WriteInt("r", m_targetsKilled);
			result &= rw->WriteInt("s", m_innocentsKilled);
			result &= rw->WriteInt("t", m_deaths);
			result &= rw->WriteInt("u", m_bottlesDelivered);
			return result;
		}

	private:

		int m_missionTypeId;           // mission name
		int m_missionId;               // mission ID
		int m_matchId;                 // match ID
		u64 m_bossId;                  // unique ID of the Boss instance
		bool m_isboss;                 // whether this player is the leader
		int m_bosstype;                // type of org: Securoserv, MC, unaffiliated
		int m_playerRole;              // similar to previous metrics, the player's role: CEO, bodyguard, biker prospect etc.
		int m_launcherRank;            // rank of the player launching
		int m_location;                // subvariation
		int m_playerParticipated;      // whether the player participated
		bool m_won;                    // whether the mission was completed successfully
		int m_endingReason;            // similar to previous metrics: won, lost, boss_left etc.
		int m_timeTakenToComplete;     // time taken to complete, in seconds (for consistency)
		int m_cashEarned;              // money earned
		int m_rpEarned;                // RP earned
		int m_chipsEarned;             // amount of chips earned
		int m_itemEarned;              // hash of item earned (t-shirt, vehicle etc.)
		int m_targetsKilled;           // number of mission critical/aggressive peds killed
		int m_innocentsKilled;         // number of innocent peds killed
		int m_deaths;                  // how many times the player died
		int m_bottlesDelivered;        // amount of tequila/champagne delivered
	};

	// triggers after an Island Heist DJ mission has ended
	void CommandPlaystatsDjMissionEnded(DjMissionEndedInfo* data)
	{
		MetricDJ_MISSION_ENDED m(data);
		APPEND_METRIC(m);
	}

	/////////////////////////////////////////////////////////////////////////////// ROBBERY_PREP
	class MetricROBBERY_PREP : public MetricPlayStat
	{
		RL_DECLARE_METRIC(ROBBERY_PREP, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

	public:

		MetricROBBERY_PREP(RobberyPrepInfo* data)
		{
			u64 bid0 = ((u64)data->m_bossId1.Int) << 32;
			u64 bid1 = (0x00000000FFFFFFFF&(u64)data->m_bossId2.Int);
			const u64 localbossId = StatsInterface::GetUInt64Stat(StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID"));

			m_playthroughID = data->m_playthroughID.Int;
			m_missionName = data->m_missionName.Int;
			m_sessionID = data->m_sessionID.Int;
			m_strandID = data->m_strandID.Int;
			m_type = data->m_type.Int;
			m_bossId = bid0 | bid1;
			m_isBoss = (m_bossId != 0 && m_bossId == localbossId);
			m_bosstype = data->m_bosstype.Int;
			m_launcherRank = data->m_launcherRank.Int;
			m_playerRole = data->m_playerRole.Int;
			m_playerParticipated = data->m_playerParticipated.Int != 0;
			m_timeTakenToComplete = data->m_timeTakenToComplete.Int;
			m_timeLimit = data->m_timeLimit.Int;
			m_endingReason = data->m_endingReason.Int;
			m_isRivalParty = data->m_isRivalParty.Int != 0;
			m_cashEarned = data->m_cashEarned.Int;
			m_rpEarned = data->m_rpEarned.Int;
			m_location = data->m_location.Int;
			m_invitesSent = data->m_invitesSent.Int;
			m_invitesAccepted = data->m_invitesAccepted.Int;
			m_deaths = data->m_deaths.Int;
			m_targetsKilled = data->m_targetsKilled.Int;
			m_innocentsKilled = data->m_innocentsKilled.Int;
			m_properties = data->m_properties.Int;
			m_properties2 = data->m_properties2.Int;
			m_failedStealth = data->m_failedStealth.Int != 0;
			m_deliveryLocation = data->m_deliveryLocation.Int;
			m_hashmac = data->m_hashmac.Int;
			m_vehicleStolen = data->m_vehicleStolen.Int;
			m_vehicleCount = data->m_vehicleCount.Int;
			m_outfits = data->m_outfits.Int;
			m_entrance = data->m_entrance.Int;
			m_missionCategory = data->m_missionCategory.Int;
			m_won = data->m_won.Int != 0;
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);
			result &= rw->WriteInt("a", m_playthroughID);
			result &= rw->WriteInt("b", m_missionName);
			result &= rw->WriteInt("c", m_sessionID);
			result &= rw->WriteInt("d", m_strandID);
			result &= rw->WriteInt("e", m_type);
			result &= rw->WriteInt64("f", m_bossId);
			result &= rw->WriteBool("g", m_isBoss);
			result &= rw->WriteInt("h", m_bosstype);
			result &= rw->WriteInt("i", m_launcherRank);
			result &= rw->WriteInt("j", m_playerRole);
			result &= rw->WriteBool("k", m_playerParticipated);
			result &= rw->WriteInt("l", m_timeTakenToComplete);
			result &= rw->WriteInt("m", m_timeLimit);
			result &= rw->WriteInt("n", m_endingReason);
			result &= rw->WriteBool("o", m_isRivalParty);
			result &= rw->WriteInt("p", m_cashEarned);
			result &= rw->WriteInt("q", m_rpEarned);
			result &= rw->WriteInt("r", m_location);
			result &= rw->WriteInt("s", m_invitesSent);
			result &= rw->WriteInt("t", m_invitesAccepted);
			result &= rw->WriteInt("u", m_deaths);
			result &= rw->WriteInt("v", m_targetsKilled);
			result &= rw->WriteInt("w", m_innocentsKilled);
			result &= rw->WriteInt("x", m_properties);
			result &= rw->WriteInt("y", m_properties2);
			result &= rw->WriteBool("z", m_failedStealth);
			result &= rw->WriteInt("aa", m_deliveryLocation);
			result &= rw->WriteInt("ab", m_hashmac);
			result &= rw->WriteInt("ac", m_vehicleStolen);
			result &= rw->WriteInt("ad", m_vehicleCount);
			result &= rw->WriteInt("ae", m_outfits);
			result &= rw->WriteInt("af", m_entrance);
			result &= rw->WriteInt("ag", m_missionCategory);
			result &= rw->WriteBool("ah", m_won);
			return result;
		}

	private:

		int m_playthroughID;           // playthrough ID, this should appear in all related content for a strand, like preps and finale, and would enable us to track an entire strand playthrough
		int m_missionName;             // mission name
		int m_sessionID;               // session ID
		int m_strandID;                // active strand
		int m_type;                    // setup, prep
		u64 m_bossId;                  // unique ID of the Boss instance
		bool m_isBoss;                 // whether this player is the leader
		int m_bosstype;                // type of org: Securoserv, MC, unaffiliated
		int m_launcherRank;            // rank of the player launching
		int m_playerRole;              // similar to previous metrics, the player's role: CEO, bodyguard, biker prospect etc.
		bool m_playerParticipated;     // whether the player participated
		int m_timeTakenToComplete;     // time taken to complete, in seconds (for consistency)
		int m_timeLimit;               // time limit, in seconds (for consistency); null if not applicable
		int m_endingReason;            // similar to previous metrics: won, lost, boss_left etc.
		bool m_isRivalParty;           // is the party completing the mission the same as the party that launched it; null if not applicable
		int m_cashEarned;              // money earned
		int m_rpEarned;                // RP earned
		int m_location;                // mission subvariation
		int m_invitesSent;             // invites sent
		int m_invitesAccepted;         // invites accepted
		int m_deaths;                  // how many times the player died
		int m_targetsKilled;           // number of mission critical/aggressive peds killed
		int m_innocentsKilled;         // number of innocent peds killed
		int m_properties;              // existing bitset of type of properties owned: casino penthouse, gunrunning bunker, smuggler hangar, terrorbyte, submarine etc.
		int m_properties2;             // bitset of type of properties owned; future proofing
		bool m_failedStealth;          // whether the player failed stealth and alerted the enemies
		int m_deliveryLocation;        // delivery location
		int m_hashmac;                 // hash mac
		int m_vehicleStolen;           // hash, stolen vehicle in Sewer Schematics, Meth Tanker
		int m_vehicleCount;            // count of vehicles delivered in Warehouse Defenses
		int m_outfits;                 // outfits used, if any, relevant for Scope Transporter, Container Manifest; null otherwise
		int m_entrance;                // entrance, relevant for Locate Bunker; null otherwise
		int m_missionCategory;		   // which set of missions these are (tuner robberies, fixer story missions etc.)
		bool m_won;					   // whether the mission was completed successfully
	};

	// triggers after a Tuner Robbery setup/prep mission has ended
	void CommandPlaystatsRobberyPrep(RobberyPrepInfo* data)
	{
		MetricROBBERY_PREP m(data);
		APPEND_METRIC(m);
	}

	/////////////////////////////////////////////////////////////////////////////// ROBBERY_FINALE
	class MetricROBBERY_FINALE : public MetricPlayStat
	{
		RL_DECLARE_METRIC(ROBBERY_FINALE, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

	public:

		MetricROBBERY_FINALE(RobberyFinaleInfo* data)
		{
			u64 bid0 = ((u64)data->m_bossId1.Int) << 32;
			u64 bid1 = (0x00000000FFFFFFFF & (u64)data->m_bossId2.Int);
			const u64 localbossId = StatsInterface::GetUInt64Stat(StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID"));

			m_playthroughID = data->m_playthroughID.Int;
			m_missionID = data->m_missionID.Int;
			m_sessionID = data->m_sessionID.Int;
			m_strandID = data->m_strandID.Int;
			safecpy(m_publicContentID, data->m_publicContentID);
			m_bossId = bid0 | bid1;
			m_isBoss = (m_bossId != 0 && m_bossId == localbossId);
			m_bosstype = data->m_bosstype.Int;
			m_launcherRank = data->m_launcherRank.Int;
			m_playerRole = data->m_playerRole.Int;
			m_endingReason = data->m_endingReason.Int;
			m_replay = data->m_replay.Int;
			m_rpEarned = data->m_rpEarned.Int;
			m_timeTakenToComplete = data->m_timeTakenToComplete.Int;
			m_checkpoint = data->m_checkpoint.Int;
			m_deaths = data->m_deaths.Int;
			m_targetsKilled = data->m_targetsKilled.Int;
			m_innocentsKilled = data->m_innocentsKilled.Int;
			m_properties = data->m_properties.Int;
			m_properties2 = data->m_properties2.Int;
			m_failedStealth = data->m_failedStealth.Int != 0;
			m_wLoadout = data->m_wLoadout.Int;
			m_outfits = data->m_outfits.Int;
			m_moneyEarned = data->m_moneyEarned.Int;
			m_vehicleBoard = data->m_vehicleBoard.Int;
			m_hashmac = data->m_hashmac.Int;
			m_missionCategory = data->m_missionCategory.Int;
			m_won = data->m_won.Int != 0;
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);
			result &= rw->WriteInt("a", m_playthroughID);
			result &= rw->WriteInt("b", m_missionID);
			result &= rw->WriteInt("c", m_sessionID);
			result &= rw->WriteInt("d", m_strandID);
			result &= rw->WriteString("e", m_publicContentID);
			result &= rw->WriteInt64("f", m_bossId);
			result &= rw->WriteBool("g", m_isBoss);
			result &= rw->WriteInt("h", m_bosstype);
			result &= rw->WriteInt("i", m_launcherRank);
			result &= rw->WriteInt("j", m_playerRole);
			result &= rw->WriteInt("k", m_endingReason);
			result &= rw->WriteInt("l", m_replay);
			result &= rw->WriteInt("m", m_rpEarned);
			result &= rw->WriteInt("n", m_timeTakenToComplete);
			result &= rw->WriteInt("o", m_checkpoint);
			result &= rw->WriteInt("p", m_deaths);
			result &= rw->WriteInt("q", m_targetsKilled);
			result &= rw->WriteInt("r", m_innocentsKilled);
			result &= rw->WriteInt("s", m_properties);
			result &= rw->WriteInt("t", m_properties2);
			result &= rw->WriteBool("u", m_failedStealth);
			result &= rw->WriteInt("v", m_wLoadout);
			result &= rw->WriteInt("w", m_outfits);
			result &= rw->WriteInt("x", m_moneyEarned);
			result &= rw->WriteInt("y", m_vehicleBoard);
			result &= rw->WriteInt("z", m_hashmac);
			result &= rw->WriteInt("aa", m_missionCategory);
			result &= rw->WriteBool("ab", m_won);
			return result;
		}

	private:

		int m_playthroughID;           // playthrough ID, this should appear in all related content for a strand, like preps and finale, and would enable us to track an entire strand playthrough
		int m_missionID;               // mission name
		int m_sessionID;               // session ID
		int m_strandID;                // active strand
		char m_publicContentID[MetricPlayStat::MAX_STRING_LENGTH]; // public content ID
		u64 m_bossId;                  // unique ID of the Boss instance
		bool m_isBoss;                 // whether this player is the leader
		int m_bosstype;                // type of org: Securoserv, MC, unaffiliated
		int m_launcherRank;            // rank of the player launching
		int m_playerRole;              // similar to previous metrics, the player's role: CEO, bodyguard, biker prospect etc.
		int m_endingReason;            // ending reason
		int m_replay;                  // counts how many times this mission was failed, then continued from checkpoint/restarted; resets after the player exits the mission, null if not applicable
		int m_rpEarned;                // RP earned
		int m_timeTakenToComplete;     // time taken to complete, in seconds (for consistency)
		int m_checkpoint;              // what checkpoint was the mission failed at; null if not applicable
		int m_deaths;                  // how many times the player died
		int m_targetsKilled;           // number of mission critical/aggressive peds killed
		int m_innocentsKilled;         // number of innocent peds killed
		int m_properties;              // existing bitset of type of properties owned: casino penthouse, gunrunning bunker, smuggler hangar, terrorbyte, submarine etc.
		int m_properties2;             // bitset of type of properties owned; future proofing
		bool m_failedStealth;          // whether the player failed stealth and alerted the enemies
		int m_wLoadout;                // weapon loadout the player has picked on the board
		int m_outfits;                 // outfits chosen on the board
		int m_moneyEarned;             // total amount of money earned from the heist
		int m_vehicleBoard;            // hash, car chosen on the planning board
		int m_hashmac;                 // hash mac
		int m_missionCategory;		   // which set of missions these are (tuner robberies, fixer story missions etc.)
		bool m_won;					   // whether the mission was completed successfully
	};

	// triggers after a Tuner Robbery finale mission has ended
	void CommandPlaystatsRobberyFinale(RobberyFinaleInfo* data)
	{
		MetricROBBERY_FINALE m(data);
		APPEND_METRIC(m);
	}

	/////////////////////////////////////////////////////////////////////////////// EXTRA_EVENT
	class MetricEXTRA_EVENT : public MetricPlayStat
	{
		RL_DECLARE_METRIC(EXTRA_EVENT, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

	public:

		MetricEXTRA_EVENT(ExtraEventInfo* data)
		{
			m_missionName = data->m_missionName.Int;
			m_pursuer = data->m_pursuer.Int;
			m_sessionID = data->m_sessionID.Int;
			m_endingReason = data->m_endingReason.Int;
			m_onFoot = data->m_onFoot.Int != 0;
			m_time = data->m_time.Int;
			m_timeLimit = data->m_timeLimit.Int;
			m_pursuerHealth = data->m_pursuerHealth.Float;
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);
			result &= rw->WriteInt("a", m_missionName);
			result &= rw->WriteInt("b", m_pursuer);
			result &= rw->WriteInt("c", m_sessionID);
			result &= rw->WriteInt("d", m_endingReason);
			result &= rw->WriteBool("e", m_onFoot);
			result &= rw->WriteInt("f", m_time);
			result &= rw->WriteInt("g", m_timeLimit);
			result &= rw->WriteFloat("h", m_pursuerHealth);
			return result;
		}

	private:

		int m_missionName;             // mission name
		int m_pursuer;                 // type of pursuer: phantom car, psycho, clown etc.
		int m_sessionID;               // session ID
		int m_endingReason;            // reason for the event ending: player went inside interior, died, time ran out etc.
		bool m_onFoot;                 // whether the player was on foot
		int m_time;                    // time until the event ended, in milliseconds
		int m_timeLimit;               // max time the event can last, in milliseconds
		float m_pursuerHealth;         // or int, amount of health left for the pursuer
	};

	// triggers at the end of special events, in this case Halloween events phantom car and slashers, when the pursuer despawns
	void CommandPlaystatsExtraEvent(ExtraEventInfo* data)
	{
		MetricEXTRA_EVENT m(data);
		APPEND_METRIC(m);
	}

	/////////////////////////////////////////////////////////////////////////////// CARCLUB_POINTS
	class MetricCARCLUB_POINTS : public MetricPlayStat
	{
		RL_DECLARE_METRIC(CARCLUB_POINTS, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

	public:

		MetricCARCLUB_POINTS(CarclubPointsInfo* data)
		{
			u64 bid0 = ((u64)data->m_bossId1.Int) << 32;
			u64 bid1 = (0x00000000FFFFFFFF & (u64)data->m_bossId2.Int);
			const u64 localbossId = StatsInterface::GetUInt64Stat(StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID"));

			m_match_id = data->m_match_id.Int;
			m_bossId = bid0 | bid1;
			m_is_boss = (m_bossId != 0 && m_bossId == localbossId);
			m_method = data->m_method.Int;
			m_amount = data->m_amount.Int;
			m_itemBought = data->m_itemBought.Int;
			m_streak = data->m_streak.Int;
			m_memberLevel = data->m_memberLevel.Int;
			m_levelUp = data->m_levelUp.Int != 0;
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);
			result &= rw->WriteInt64("a", m_bossId);
			result &= rw->WriteInt("b", m_match_id);
			result &= rw->WriteBool("c", m_is_boss);
			result &= rw->WriteInt("d", m_method);
			result &= rw->WriteInt("e", m_amount);
			result &= rw->WriteInt("f", m_itemBought);
			result &= rw->WriteInt("g", m_streak);
			result &= rw->WriteInt("h", m_memberLevel);
			result &= rw->WriteBool("i", m_levelUp);
			return result;
		}

	private:

		u64 m_bossId;                 // unique ID of the Boss/organization
		int m_match_id;                // match ID
		bool m_is_boss;                // whether this player is the gang boss
		int m_method;                  // hash, reason for earning points: item bought, first time bonus, complete mission etc.
		int m_amount;                  // number of points earned
		int m_itemBought;              // hash, item bought that awarded Car Club reputation
		int m_streak;                  // streak count for visiting the the Car Club
		int m_memberLevel;             // membership level
		bool m_levelUp;                // whether the player has just leveled up their membership
	};

	// triggers after the player earns Car Club reputation to level up their membership
	void CommandPlaystatsCarclubPoints(CarclubPointsInfo* data)
	{
		MetricCARCLUB_POINTS m(data);
		APPEND_METRIC(m);
	}

#if RSG_GEN9
	/////////////////////////////////////////////////////////////////////////////// LP_STORY
	// triggers after the player leaves a tab, with a record of what they viewed on that tab, provided they spent X milliseconds on it, set by tunable, to prevent spamming (I'd say start with 5 seconds); triggers regardless of time spent when leaving the landing page altogether. Additional tunable for percentage of players sending the metric.
	void CommandPlaystatsLpNav(LpNavInfo* data)
	{
		CNetworkTelemetry::AppendLandingPageNavFromScript(data->m_clickedTile.Int, data->m_leftBy.Int, data->m_tab.Int, data->m_hoverTile.Int, data->m_exitLp.Int != 0);
	}

	// triggers after a UGC tool is launched
	void CommandPlaystatsUgcNav(const int feature, const int location)
	{
		MetricUGC_NAV m(feature, location);
		APPEND_METRIC(m);
	}

	//////////////////////////////////////////////////////////////////////////
#endif // RSG_GEN9

	/////////////////////////////////////////////////////////////////////////////// CARCLUB_CHALLENGE
	class MetricCARCLUB_CHALLENGE : public MetricPlayStat
	{
		RL_DECLARE_METRIC(CARCLUB_CHALLENGE, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

	public:

		MetricCARCLUB_CHALLENGE(const int missionName, const int numGoalsCompleted, const int numGoalsRequired, const int memberLevel)
		{
			m_missionName = missionName;
			m_numGoalsCompleted = numGoalsCompleted;
			m_numGoalsRequired = numGoalsRequired;
			m_memberLevel = memberLevel;
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);
			result &= rw->WriteInt("a", m_missionName);
			result &= rw->WriteInt("b", m_numGoalsCompleted);
			result &= rw->WriteInt("c", m_numGoalsRequired);
			result &= rw->WriteInt("d", m_memberLevel);
			return result;
		}

	private:

		int m_missionName;             // name of the challenge
		int m_numGoalsCompleted;       // number of challenge steps completed up to this point, during this event
		int m_numGoalsRequired;        // total number of challenge steps that need to be completed during this event
		int m_memberLevel;             // membership level
	};

	// triggers after a step of the weekly vehicle challenge has been completed
	void CommandPlaystatsCarclubChallenge(const int missionName, const int numGoalsCompleted, const int numGoalsRequired, const int memberLevel)
	{
		MetricCARCLUB_CHALLENGE m(missionName, numGoalsCompleted, numGoalsRequired, memberLevel);
		APPEND_METRIC(m);
	}

	/////////////////////////////////////////////////////////////////////////////// CARCLUB_PRIZE
	class MetricCARCLUB_PRIZE : public MetricPlayStat
	{
		RL_DECLARE_METRIC(CARCLUB_PRIZE, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

	public:

		MetricCARCLUB_PRIZE(const int missionName, const int vehicle)
		{
			m_missionName = missionName;
			m_vehicle = vehicle;
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);
			result &= rw->WriteInt("a", m_missionName);
			result &= rw->WriteInt("b", m_vehicle);
			return result;
		}

	private:

		int m_missionName;             // name of the challenge
		int m_vehicle;                 // hash, vehicle that was claimed
	};

	// triggers after the player claims the prize car earned for completing the weekly challenge
	void CommandPlaystatsCarclubPrize(const int missionName, const int vehicle)
	{
		MetricCARCLUB_PRIZE m(missionName, vehicle);
		APPEND_METRIC(m);
	}


#if GEN9_STANDALONE_ENABLED
	/////////////////////////////////////////////////////////////////////////////// PM_NAV
	class MetricPM_NAV : public MetricPlayStat
	{
		RL_DECLARE_METRIC(PM_NAV, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

	public:

		MetricPM_NAV(const int pageView, const int leftBy, const int duration)
			: m_pageView(pageView)
			, m_leftBy(leftBy)
			, m_duration(duration)
		{
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);
			result &= rw->WriteInt("a", m_pageView);
			result &= rw->WriteInt("b", m_leftBy);
			result &= rw->WriteInt("c", m_duration);
			return result;
		}

	private:

		int m_pageView;
		int m_leftBy;
		int m_duration;
	};
#endif

	void CommandPlaystatsPmNav(const int GEN9_STANDALONE_ONLY(pageView), const int GEN9_STANDALONE_ONLY(leftBy), const int GEN9_STANDALONE_ONLY(duration))
	{
#if GEN9_STANDALONE_ENABLED
		MetricPM_NAV m(pageView, leftBy, duration);
		APPEND_METRIC(m);
#endif
	}

        class MetricFreemodeMissionEnd : public MetricPlayStat
        {
                RL_DECLARE_METRIC(FM_MISSION_END, TELEMETRY_CHANNEL_MISSION, LOGLEVEL_HIGH_PRIORITY);

        public:
                MetricFreemodeMissionEnd(sFreemodeMissionEnd* data)
                : MetricPlayStat()
                , m_availMisCount(0)
                , m_availMisDifCount(0)
                {
                        m_groupID                 = data->m_groupID.Int;
                        m_missionCategory         = data->m_missionCategory.Int;
                        m_missionName             = data->m_missionName.Int;
                        m_sessionID               = data->m_sessionID.Int;
                        m_type                    = data->m_type.Int;
                        m_location                = data->m_location.Int;
                        m_launchMethod            = data->m_launchMethod.Int;
                        m_bossId                  = ((u64)data->m_bossId1.Int) << 32 | (0x00000000FFFFFFFF & (u64)data->m_bossId2.Int);
                        m_isBoss                  = (m_bossId != 0 && m_bossId == StatsInterface::GetUInt64Stat(StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID")));
                        m_bosstype                = data->m_bosstype.Int;
                        m_launcherRank            = data->m_launcherRank.Int;
                        m_playerRole              = data->m_playerRole.Int;
                        m_playerParticipated      = data->m_playerParticipated.Int != 0;
                        m_timeTakenToComplete     = data->m_timeTakenToComplete.Int;
                        m_timeLimit               = data->m_timeLimit.Int;
                        m_won                     = data->m_won.Int != 0;
                        m_endingReason            = data->m_endingReason.Int;
                        m_isRivalParty            = data->m_isRivalParty.Int != 0;
                        m_cashEarned              = data->m_cashEarned.Int;
                        m_rpEarned                = data->m_rpEarned.Int;
                        m_deaths                  = data->m_deaths.Int;
                        m_targetsKilled           = data->m_targetsKilled.Int;
                        m_innocentsKilled         = data->m_innocentsKilled.Int;
                        m_properties              = data->m_properties.Int;
                        m_properties2             = data->m_properties2.Int;
                        m_failedStealth           = data->m_failedStealth.Int != 0;
                        m_difficulty              = data->m_difficulty.Int;
                        m_bonusKillMethod         = data->m_bonusKillMethod.Int;
                        m_bonusKillComplete       = data->m_bonusKillComplete.Int;
                }

                virtual bool Write(RsonWriter* rw) const
                {
                        bool result = MetricPlayStat::Write(rw);
                        result &= rw->WriteInt("a", m_groupID);
                        result &= rw->WriteInt("b", m_missionCategory);
                        result &= rw->WriteInt("c", m_missionName);
                        result &= rw->WriteInt("d", m_sessionID);
                        result &= rw->WriteInt("e", m_type);
                        result &= rw->WriteInt("f", m_location);
                        result &= rw->WriteInt("g", m_launchMethod);
                        result &= rw->WriteInt64("h", m_bossId);
                        result &= rw->WriteBool("i", m_isBoss);
                        result &= rw->WriteInt("j", m_bosstype);
                        result &= rw->WriteInt("k", m_launcherRank);
                        result &= rw->WriteInt("l", m_playerRole);
                        result &= rw->WriteBool("m", m_playerParticipated);
                        result &= rw->WriteInt("n", m_timeTakenToComplete);
                        result &= rw->WriteInt("o", m_timeLimit);
                        result &= rw->WriteBool("p", m_won);
                        result &= rw->WriteInt("q", m_endingReason);
                        result &= rw->WriteBool("r", m_isRivalParty);
                        result &= rw->WriteInt("s", m_cashEarned);
                        result &= rw->WriteInt("t", m_rpEarned);
                        result &= rw->WriteInt("u", m_deaths);
                        result &= rw->WriteInt("v", m_targetsKilled);
                        result &= rw->WriteInt("w", m_innocentsKilled);
                        result &= rw->WriteInt("x", m_properties);
                        result &= rw->WriteInt("y", m_properties2);
                        result &= rw->WriteBool("z", m_failedStealth);
                        result &= rw->WriteInt("aa", m_difficulty);
                        result &= rw->WriteInt("ab", m_bonusKillMethod);
                        result &= rw->WriteInt("ac", m_bonusKillComplete);

                        result &= rw->BeginArray("ad", NULL);
                        for (int i = 0; i < m_availMisCount; i++)
                        {
                                result = result && rw->WriteInt(NULL, m_availMis[i]);
                        }
                        result &= rw->End();

                        result &= rw->BeginArray("ae", NULL);
                        for (int i = 0; i < m_availMisDifCount; i++)
                        {
                                result = result && rw->WriteInt(NULL, m_availMisDif[i]);
                        }
                        result &= rw->End();
                        return result;
                }

        private:
                int m_groupID;
                int m_missionCategory;
                int m_missionName;
                int m_sessionID;
                int m_type;
                int m_location;
                int m_launchMethod;
                u64 m_bossId;
                bool m_isBoss;
                int m_bosstype;
                int m_launcherRank;
                int m_playerRole;
                bool m_playerParticipated;
                int m_timeTakenToComplete;
                int m_timeLimit;
                bool m_won;
                int m_endingReason;
                bool m_isRivalParty;
                int m_cashEarned;
                int m_rpEarned;
                int m_deaths;
                int m_targetsKilled;
                int m_innocentsKilled;
                int m_properties;
                int m_properties2;
                bool m_failedStealth;
                int m_difficulty;
                int m_bonusKillMethod;
                int m_bonusKillComplete;

        public:
                static const int m_availMisSize = 32;
                int m_availMisCount;
                int m_availMis[m_availMisSize];

                static const int m_availMisDifSize = 32;
                int m_availMisDifCount;
                int m_availMisDif[m_availMisDifSize];

        };

        void CommandPlaystatsFreemodeMissionEnd(sFreemodeMissionEnd* data, scrArrayBaseAddr& availMis, scrArrayBaseAddr& availMisDif)
        {
                MetricFreemodeMissionEnd m(data);

                m.m_availMisCount = scrArray::GetCount(availMis);
                scrValue* pAvailMis = scrArray::GetElements<scrValue>(availMis);
                scriptAssertf(m.m_availMisSize >= m.m_availMisCount, "%s - PLAYSTATS_FM_MISSION_END: availMis: Too many entries (%d). The metric only supports %d entries, ask code to bump it.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), m.m_availMisCount, m.m_availMisSize);

                m.m_availMisCount = Min(m.m_availMisCount, m.m_availMisSize);
                for (int i = 0; i < m.m_availMisCount; i++)
                {
                        m.m_availMis[i] = pAvailMis[i].Int;
                }

                m.m_availMisDifCount = scrArray::GetCount(availMisDif);
				scrValue* pAvailMisDif = scrArray::GetElements<scrValue>(availMisDif);
                scriptAssertf(m.m_availMisDifSize >= m.m_availMisDifCount, "%s - PLAYSTATS_FM_MISSION_END: availMisDif: Too many entries (%d). The metric only supports %d entries, ask code to bump it.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), m.m_availMisDifCount, m.m_availMisDifSize); 

                m.m_availMisDifCount = Min(m.m_availMisDifCount, m.m_availMisDifSize);
                for (int i = 0; i < m.m_availMisDifCount; i++)
                {
                        m.m_availMisDif[i] = pAvailMisDif[i].Int;
                }

                APPEND_METRIC(m);
        }

	/////////////////////////////////////////////////////////////////////////////// AWARDS_NAV
	class MetricAWARDS_NAV : public MetricPlayStat
	{
		RL_DECLARE_METRIC(AWARDS_NAV, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

	public:

		MetricAWARDS_NAV(const int fixerView1, const int fixerDone1)
			: m_fixerView1(fixerView1)
			, m_fixerDone1(fixerDone1)
		{
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);
			result &= rw->WriteInt("a", m_fixerView1);
			result &= rw->WriteInt("b", m_fixerDone1);
			return result;
		}

	private:

		int m_fixerView1;
		int m_fixerDone1;
	};

	void CommandPlaystatsAwardNav(const int fixerView1, const int fixerDone1)
	{
		MetricAWARDS_NAV m(fixerView1, fixerDone1);
		APPEND_METRIC(m);
	}

	class MetricInstMissionEnd : public MetricPlayStat
	{
		RL_DECLARE_METRIC(INST_MISSION_END, TELEMETRY_CHANNEL_MISC, LOGLEVEL_MEDIUM_PRIORITY);
		
	public:
		MetricInstMissionEnd(sInstMissionEndInfo* data)
		: MetricPlayStat()
		{
			m_groupID = data->m_groupID.Int;
			m_missionCategory = data->m_missionCategory.Int;
			m_missionName = data->m_missionName.Int;
			m_sessionID = data->m_sessionID.Int;
			m_type = data->m_type.Int;
			m_location = data->m_location.Int;
			m_launchMethod = data->m_launchMethod.Int;
			safecpy(m_publicContentID, data->m_publicContentID);
			m_bossId = ((u64)data->m_bossId1.Int) << 32 | (0x00000000FFFFFFFF & (u64)data->m_bossId2.Int);
			m_isBoss = (m_bossId != 0 && m_bossId == StatsInterface::GetUInt64Stat(StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID")));
			m_bosstype = data->m_bosstype.Int;
			m_launcherRank = data->m_launcherRank.Int;
			m_playerRole = data->m_playerRole.Int;
			m_timeTakenToComplete = data->m_timeTakenToComplete.Int;
			m_timeLimit = data->m_timeLimit.Int;
			m_won = data->m_won.Int != 0;
			m_endingReason = data->m_endingReason.Int;
			m_cashEarned = data->m_cashEarned.Int;
			m_rpEarned = data->m_rpEarned.Int;
			m_deaths = data->m_deaths.Int;
			m_targetsKilled = data->m_targetsKilled.Int;
			m_innocentsKilled = data->m_innocentsKilled.Int;
			m_properties = data->m_properties.Int;
			m_properties2 = data->m_properties2.Int;
			m_failedStealth = data->m_failedStealth.Int != 0;
			m_difficulty = data->m_difficulty.Int;
			m_playerCount = data->m_playerCount.Int;
		}
		
		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);
			result &= rw->WriteInt("a", m_groupID);
			result &= rw->WriteInt("b", m_missionCategory);
			result &= rw->WriteInt("c", m_missionName);
			result &= rw->WriteInt("d", m_sessionID);
			result &= rw->WriteInt("e", m_type);
			result &= rw->WriteInt("f", m_location);
			result &= rw->WriteInt("g", m_launchMethod);
			result &= rw->WriteString("h", m_publicContentID);
			result &= rw->WriteInt64("i", m_bossId);
			result &= rw->WriteBool("j", m_isBoss);
			result &= rw->WriteInt("k", m_bosstype);
			result &= rw->WriteInt("l", m_launcherRank);
			result &= rw->WriteInt("m", m_playerRole);
			result &= rw->WriteInt("n", m_timeTakenToComplete);
			result &= rw->WriteInt("o", m_timeLimit);
			result &= rw->WriteBool("p", m_won);
			result &= rw->WriteInt("q", m_endingReason);
			result &= rw->WriteInt("r", m_cashEarned);
			result &= rw->WriteInt("s", m_rpEarned);
			result &= rw->WriteInt("t", m_deaths);
			result &= rw->WriteInt("u", m_targetsKilled);
			result &= rw->WriteInt("v", m_innocentsKilled);
			result &= rw->WriteInt("w", m_properties);
			result &= rw->WriteInt("x", m_properties2);
			result &= rw->WriteBool("y", m_failedStealth);
			result &= rw->WriteInt("z", m_difficulty);
			result &= rw->WriteInt("aa", m_playerCount);
			return result;
		}
		
	private:
		int m_groupID;              // group ID; this would enable us to track those players who play this mission together
		int m_missionCategory;      // which set of missions these are; for now, Franklin and Lamar short trips
		int m_missionName;          // mission name
		int m_sessionID;            // session ID
		int m_type;                 // type of mission
		int m_location;             // mission variation
		int m_launchMethod;         // launch method
		char m_publicContentID[32]; // public content ID
		u64 m_bossId;               // unique ID of the Boss instance
		bool m_isBoss;              // whether this player is the leader
		int m_bosstype;             // type of org: Securoserv, MC, unaffiliated
		int m_launcherRank;         // rank of the player launching
		int m_playerRole;           // similar to previous metrics, the player's role: CEO, bodyguard, biker prospect etc.
		int m_timeTakenToComplete;  // time taken to complete, in seconds (for consistency)
		int m_timeLimit;            // time limit, in seconds (for consistency); null if not applicable
		bool m_won;                 // whether the mission was completed successfully
		int m_endingReason;         // ending reason
		int m_cashEarned;           // money earned
		int m_rpEarned;             // RP earned
		int m_deaths;               // how many times the player died
		int m_targetsKilled;        // number of mission critical/aggressive peds killed
		int m_innocentsKilled;      // number of innocent peds killed
		int m_properties;           // existing bitset of type of properties owned: casino penthouse, gunrunning bunker, smuggler hangar, terrorbyte, submarine etc.
		int m_properties2;          // bitset of type of properties owned; future proofing
		bool m_failedStealth;       // whether the player failed stealth and alerted the enemies
		int m_difficulty;           // mission difficulty
		int m_playerCount;          // number of players currently on this mission
	};

	void CommandPlaystatsInstMissionEnd(sInstMissionEndInfo* data)
	{
		MetricInstMissionEnd m(data);
		APPEND_METRIC(m);
	}

	class MetricHubExit : public MetricPlayStat
	{
		RL_DECLARE_METRIC(HUB_EXIT, TELEMETRY_CHANNEL_MISC, LOGLEVEL_MEDIUM_PRIORITY);
		
	public:
		MetricHubExit(sHubExitInfo* data)
		: MetricPlayStat()
		{
			m_hubID = data->m_hubID.Int;
			m_type = data->m_type.Int;
			m_bossId = ((u64)data->m_bossId1.Int) << 32 | (0x00000000FFFFFFFF & (u64)data->m_bossId2.Int);
			m_playerRole = data->m_playerRole.Int;
			m_pulled = data->m_pulled.Int;
			m_crewId = data->m_crewId.Int;
			m_duration = data->m_duration.Int;
			m_dre = data->m_dre.Int != 0;
			m_playerCount = data->m_playerCount.Int;
		}
		
		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);
			result &= rw->WriteInt("a", m_hubID);
			result &= rw->WriteInt("b", m_type);
			result &= rw->WriteInt64("c", m_bossId);
			result &= rw->WriteInt("d", m_playerRole);
			result &= rw->WriteInt("e", m_pulled);
			result &= rw->WriteInt("f", m_crewId);
			result &= rw->WriteInt("g", m_duration);
			result &= rw->WriteBool("h", m_dre);
			result &= rw->WriteInt("i", m_playerCount);
			return result;
		}
		
	private:
		int m_hubID;       // ID that applies to this instance of the social hub; would be the same for those players inside the social hub
		int m_type;        // which social hub the player left: music studio, smoking room etc.
		u64 m_bossId;      // unique ID of the Boss instance
		int m_playerRole;  // similar to previous metrics, the player's role: CEO, bodyguard, biker prospect etc.
		int m_pulled;      // whether this player was pulled out of the social hub (by a mission starting, for example)
		int m_crewId;      // ID of the Social Club Crew the player belongs to
		int m_duration;    // time spent in the social hub
		bool m_dre;        // whether Dr Dre was present in the social hub
		int m_playerCount; // number of players in the social hub when exiting
	};

	void CommandPlaystatsHubExit(sHubExitInfo* data)
	{
		MetricHubExit m(data);
		APPEND_METRIC(m);
	}

	////////////////////////////////////////////////////////////////////////// Summer 2022

	/////////////////////////////////////////////////////////////////////////////// VEH_DEL
	class MetricVEH_DEL : public MetricPlayStat
	{
		RL_DECLARE_METRIC(VEH_DEL, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

	public:

		MetricVEH_DEL(int bossId1, int bossId2, int bossType, int vehicleID, int reason)
		{
			m_bossId = ((u64)bossId1) << 32 | (0x00000000FFFFFFFF & (u64)bossId2);
			const u64 bid0 = ((u64)bossId1) << 32;
			const u64 bid1 = (0x00000000FFFFFFFF & (u64)bossId2);
			const u64 localbossId = StatsInterface::GetUInt64Stat(StatsInterface::GetStatsModelHashId("BOSS_GOON_UUID"));
			m_isBoss = ((bid0 | bid1) != 0 && (bid0 | bid1) == localbossId);
			m_bossType = bossType;
			m_vehicleID = vehicleID;
			m_reason = reason;
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);
			result &= rw->WriteInt64("a", m_bossId);
			result &= rw->WriteBool("b", m_isBoss);
			result &= rw->WriteInt("c", m_bossType);
			result &= rw->WriteInt("d", m_vehicleID);
			result &= rw->WriteInt("e", m_reason);
			return result;
		}

	private:

		u64 m_bossId;                  // unique ID of the Boss instance
		bool m_isBoss;                 // whether this player is the leader
		int m_bossType;                // type of org: Securoserv, MC, unaffiliated
		int m_vehicleID;               // hash, vehicle that was removed
		int m_reason;                  // hash, reason for removal
	};

	// triggers after a vehicle is removed from the player's inventory
	void CommandPlaystatsVehDelInfo(int bossId1, int bossId2, int bossType, int vehicleID, int reason)
	{
		MetricVEH_DEL m(bossId1, bossId2, bossType, vehicleID, reason);
		APPEND_METRIC(m);
	}

	/////////////////////////////////////////////////////////////////////////////// INVENTORY
	class MetricINVENTORY : public MetricPlayStat
	{
		RL_DECLARE_METRIC(INVENTORY, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);

	public:

		MetricINVENTORY(InventoryInfo* data)
		{
			m_action = data->m_action.Int;
			m_reason = data->m_reason.Int;
			m_crewId = data->m_crewId.Int;
			m_location = data->m_location.Int;
			m_shopType = data->m_shopType.Int;
			m_itemCategory = data->m_itemCategory.Int;
			m_itemHash = data->m_itemHash.Int;
			m_itemDelta = data->m_itemDelta.Int;
			m_purchaseID = data->m_purchaseID.Int;
			m_takeAll = data->m_takeAll.Int != 0;
		}

		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);
			result &= rw->WriteInt("a", m_action);
			result &= rw->WriteInt("b", m_reason);
			result &= rw->WriteInt("c", m_crewId);
			result &= rw->WriteInt("d", m_location);
			result &= rw->WriteInt("e", m_shopType);
			result &= rw->WriteInt("f", m_itemCategory);
			result &= rw->WriteInt("g", m_itemHash);
			result &= rw->WriteInt("h", m_itemDelta);
			result &= rw->WriteInt("i", m_purchaseID);
			result &= rw->WriteBool("j", m_takeAll);
			return result;
		}

	private:

		int m_action;                  // was the item added to or removed from the inventory
		int m_reason;                  // hash, reason for adding/removing: purchase, pick-up, use, drop etc.
		int m_crewId;                  // ID of the Social Club Crew the player belongs to
		int m_location;                // hash, exact location where the item was purchased (which office, which arcade etc.); null if not applicable
		int m_shopType;                // hash, "shop" type the item was acquired from: vending machine, office desk, convenience store etc.; null if not applicable
		int m_itemCategory;            // hash, the item category: snack, armor
		int m_itemHash;                // hash, the item added/removed
		int m_itemDelta;               // number of items of this type added/removed
		int m_purchaseID;              // ID to link the purchases made in the same transaction
		bool m_takeAll;                // whether the player used the option to take all of the items of that type
	};

	// Triggers after a snack or armor item type are added to the inventory, removed from the inventory, or consumed outwith the inventory (vending machines, bars etc.). Would trigger each time the item type changes, or after X seconds from the change (set by tunable) that allows for the case where the player hits "buy", "drop" or "use" repeatedly.
	void CommandPlaystatsInventoryInfo(InventoryInfo* data)
	{
		MetricINVENTORY m(data);
		APPEND_METRIC(m);
	}

	////////////////////////////////////////////////////////////////////////// MAP_NAV
	class MetricMAP_NAV : public MetricPlayStat
	{
		RL_DECLARE_METRIC(MAP_NAV, TELEMETRY_CHANNEL_JOB, LOGLEVEL_HIGH_PRIORITY);
	public:
		MetricMAP_NAV(MapNavInfo* data)
		{
			m_TUBlips = data->m_TUBlips.Int;
			m_cloudMissBlips = data->m_cloudMissBlips.Int;
			m_seriesBlips = data->m_seriesBlips.Int;
			m_collectBlips = data->m_collectBlips.Int;
			m_rank = data->m_rank.Int;
			m_leftBy = data->m_leftBy.Int;
			m_waypoint = data->m_waypoint.Int;
		}
		virtual bool Write(RsonWriter* rw) const
		{
			bool result = MetricPlayStat::Write(rw);
			result &= rw->WriteInt("a", m_TUBlips);
			result &= rw->WriteInt("b", m_cloudMissBlips);
			result &= rw->WriteInt("c", m_seriesBlips);
			result &= rw->WriteInt("d", m_collectBlips);
			result &= rw->WriteInt("e", m_rank);
			result &= rw->WriteInt("f", m_leftBy);
			result &= rw->WriteInt("g", m_waypoint);
			return result;
		}
	private:
		int m_TUBlips;                 // bitset of TU blips rolled over
		int m_cloudMissBlips;          // bitset of cloud missions blips rolled over
		int m_seriesBlips;             // bitset of series blips rolled over
		int m_collectBlips;            // bitset of collectible blips rolled over
		int m_rank;                    // current rank
		int m_leftBy;                  // hash, means by which the player left the pause menu map: going back, selecting to join a lobby
		int m_waypoint;                // hash, blip that the player selected as a waypoint
	};
	// Triggers when the pause menu map is closed, provided the player spent X milliseconds on it (set by tunable).
	void CommandPlaystatsMapNav(MapNavInfo* data)
	{
		MetricMAP_NAV m(data);
		APPEND_METRIC(m);
	}

	//////////////////////////////////////////////////////////////////////////

	void SetupScriptCommands()
	{
	    // --- Change -------------------------------------------------------------------------

		SCR_REGISTER_UNUSED(STAT_GET_CODER_STAT,0x66eb2b1dfefb88f9, CommandStatGetCoderStat             );

		SCR_REGISTER_SECURE(STAT_CLEAR_SLOT_FOR_RELOAD,0xbeaed5d2d3ba3da8, CommandStatClearSlotForReload       );
		SCR_REGISTER_SECURE(STAT_LOAD,0xd7996852361ad7db, CommandStatLoad                     );
		SCR_REGISTER_SECURE(STAT_SAVE,0x6fea96827e2a3e4f, CommandStatSave                     );
		SCR_REGISTER_SECURE(STAT_SET_OPEN_SAVETYPE_IN_JOB,0x16b95be96a229823, CommandStatSetCanSaveDuringJob      );
		SCR_REGISTER_SECURE(STAT_LOAD_PENDING,0x0144e4060b91ec2e, CommandStatLoadPending              );
		SCR_REGISTER_SECURE(STAT_SAVE_PENDING,0x6fde94f8c5742d96, CommandStatSavePending              );
		SCR_REGISTER_SECURE(STAT_SAVE_PENDING_OR_REQUESTED,0xbbf12d94ba938483, CommandStatSavePendingOrRequested   );
		SCR_REGISTER_SECURE(STAT_DELETE_SLOT,0x312a27f00878aaae, CommandStatDeleteSlot               );
		SCR_REGISTER_SECURE(STAT_SLOT_IS_LOADED,0x587b7bf26e4105bf, CommandStatSlotIsLoaded             );
		SCR_REGISTER_SECURE(STAT_CLOUD_SLOT_LOAD_FAILED,0x7d9207534c16b2aa, CommandStatCloudFileLoadFailed      );
		SCR_REGISTER_SECURE(STAT_CLOUD_SLOT_LOAD_FAILED_CODE,0x27c2b98ca118874b, CommandStatCloudFileLoadFailedCode  );
		SCR_REGISTER_UNUSED(STAT_SET_CLOUD_SAVE_SERVICE,0xd4a6dfe772ebb5e1, CommandStatSetCloudSaveService      );
		SCR_REGISTER_UNUSED(STAT_SET_CLOUD_SLOT_LOAD_FAILED,0x455bb75024bdd2bd, CommandStatSetCloudLoadFailed       );
		SCR_REGISTER_SECURE(STAT_SET_BLOCK_SAVES,0xc669ac81a9ecd6cf, CommandStatSetBlockSaveRequests     );
		SCR_REGISTER_SECURE(STAT_GET_BLOCK_SAVES,0x08a029193b31ef73, CommandStatGetBlockSaveRequests);
		SCR_REGISTER_SECURE(STAT_CLOUD_SLOT_SAVE_FAILED,0x51f83a77b94083cb, CommandStatCloudFileSaveFailed      );
		SCR_REGISTER_UNUSED(STAT_CLOUD_SLOT_SAVE_FAILED_CODE,0x55af46fe4a2116a7, CommandStatCloudFileSaveFailedCode  );
		SCR_REGISTER_SECURE(STAT_CLEAR_PENDING_SAVES,0x0bf47acb994fc692, CommandStatClearPendingSaves        );
		SCR_REGISTER_SECURE(STAT_LOAD_DIRTY_READ_DETECTED,0xa1094269c42a1b05, CommandStatDirtyReadDetected        );
		SCR_REGISTER_SECURE(STAT_CLEAR_DIRTY_READ_DETECTED,0xf9816d1fa9b68e2f, CommandStatClearDirtyReadDetected   );
		SCR_REGISTER_UNUSED(STAT_GET_LOCAL_SAVE_TIME,0x0808051294c4c2a9, CommandStatGetLocalSaveTimestamp);
		SCR_REGISTER_UNUSED(STAT_GET_PROFILE_STATS_SERVER_SAVE_TIME,0xd9d2e8de35ce5cca, CommandStatProfileStatsLastServerTimestamp);
		SCR_REGISTER_SECURE(STAT_GET_LOAD_SAFE_TO_PROGRESS_TO_MP_FROM_SP,0xc0dce8359a3476d3, CommandStatGetIsSafeToProgressToMpFromSp);

		SCR_REGISTER_UNUSED(STAT_IS_GAME_RUNNING_WITH_NOMPSAVEGAME_PARAM,0x8e7220fca12bdd90, CommandIsGameRunningWithNoMpSaveGameParam);

		SCR_REGISTER_UNUSED(STAT_HASH_ID,0x7125cd61d5aeddec, CommandStatHashId                   );
		SCR_REGISTER_UNUSED(STAT_GET_NAME,0x6112c354da434263, CommandStatGetName                  );
		SCR_REGISTER_UNUSED(STAT_IS_KEY_VALID,0x8c40bf5bd821e5c6, CommandStatIsKeyValid               );

		SCR_REGISTER_SECURE(STAT_SET_INT,0x076a5661ef5abee4, CommandStatSetInt                   );
		SCR_REGISTER_SECURE(STAT_SET_FLOAT,0xd9b28f1445ffcee7, CommandStatSetFloat                 );
		SCR_REGISTER_SECURE(STAT_SET_BOOL,0x41483427efbc0047, CommandStatSetBool                  );
		SCR_REGISTER_SECURE(STAT_SET_GXT_LABEL,0xa83328d394deaa9f, CommandStatSetGxtValue             );
		SCR_REGISTER_SECURE(STAT_SET_DATE,0x8147c3d3000a3094, CommandStatSetDate                  );
		SCR_REGISTER_SECURE(STAT_SET_STRING,0x6e74a171535f0808, CommandStatSetString                );
		SCR_REGISTER_SECURE(STAT_SET_POS,0x14acf0cbd3e0dfb8, CommandStatSetPos                   );
		SCR_REGISTER_SECURE(STAT_SET_MASKED_INT,0xe2ea061e6f7b236f, CommandStatSetMaskedInt             );
		SCR_REGISTER_SECURE(STAT_SET_USER_ID,0xed186f54433a4358, CommandStatSetUserId                );
		SCR_REGISTER_SECURE(STAT_SET_CURRENT_POSIX_TIME,0x39ba1f3b9759059d, CommandStatSetCurrentPosixTime      );

		SCR_REGISTER_SECURE(STAT_GET_INT,0x61481f9fbb1c7edd, CommandStatGetInt                   );
		SCR_REGISTER_SECURE(STAT_GET_FLOAT,0x9a69a54179cea8e5, CommandStatGetFloat                 );
		SCR_REGISTER_SECURE(STAT_GET_BOOL,0x1bb27397611554ce, CommandStatGetBool                  );
		SCR_REGISTER_UNUSED(STAT_GET_GXT_LABEL,0x7354a4c703aaeb34, CommandStatGetGxtLabel              );
		SCR_REGISTER_SECURE(STAT_GET_DATE,0x585e6867d0d71962, CommandStatGetDate                  );
		SCR_REGISTER_SECURE(STAT_GET_STRING,0xafca9e302ef8c3e0, CommandStatGetString                );
		SCR_REGISTER_SECURE(STAT_GET_POS,0x335be6bc12888abe, CommandStatGetPos                   );
		SCR_REGISTER_SECURE(STAT_GET_MASKED_INT,0xf881d9ecfc43b6ea, CommandStatGetMaskedInt             );
		SCR_REGISTER_SECURE(STAT_GET_USER_ID,0x45a1e17ec0480db4, CommandStatGetUserId                );
		SCR_REGISTER_UNUSED(STAT_GET_POSIX_TIME_DIFFERENCE,0x485eee9aae9f082b, CommandStatGetPosixTimeDifference);

		SCR_REGISTER_SECURE(STAT_GET_LICENSE_PLATE,0x378cf7462fea8a28, CommandStatGetLicensePlate          );
		SCR_REGISTER_SECURE(STAT_SET_LICENSE_PLATE,0xce9b2b59b53a67c6, CommandStatSetLicensePlate          );

		SCR_REGISTER_SECURE(STAT_INCREMENT,0xfa38cf6ffd2bf050, CommandStatIncrement                );
		SCR_REGISTER_UNUSED(STAT_DECREMENT,0x155e99bf32775ddc, CommandStatDecrement				);

		SCR_REGISTER_UNUSED(STAT_SET_GREATER,0x53b0eb1ed6aea380, CommandStatSetGreater               );
		SCR_REGISTER_UNUSED(STAT_SET_LESSER,0x537b9584db71d36c, CommandStatSetLesser                );

		SCR_REGISTER_UNUSED(STAT_GET_SUPPORTS_LABELS,0x6cb367057d418b9e, CommandStatGetSupportsLabels        );
		SCR_REGISTER_UNUSED(STAT_GET_SUPPORTS_STRINGS,0x591ea0765690adce, CommandStatGetSupportsString        );

		SCR_REGISTER_SECURE(STAT_COMMUNITY_START_SYNCH,0xd17ba652ddceb594,	CommandStatCommunityStartSynch      );
		SCR_REGISTER_SECURE(STAT_COMMUNITY_SYNCH_IS_PENDING,0x20e53dd70de083d7,	CommandStatCommunitySynchPending    );
		SCR_REGISTER_UNUSED(STAT_COMMUNITY_SYNCH_FAILED,0x03647b08aef903b4,	CommandStatCommunitySynchFailed     );
		SCR_REGISTER_SECURE(STAT_COMMUNITY_GET_HISTORY,0xb4455b84f602b40d,	CommandStatCommunityGetHistory      );

		SCR_REGISTER_SECURE(STAT_RESET_ALL_ONLINE_CHARACTER_STATS,0xad7126cac02de4f6,	CommandStatClearAllOnlineCharacterStats);
		SCR_REGISTER_SECURE(STAT_LOCAL_RESET_ALL_ONLINE_CHARACTER_STATS,0x9ab4f7cabff7feb1, CommandStatLocalClearAllOnlineCharacterStats);
		SCR_REGISTER_UNUSED(STAT_RESET_ALL_SINGLE_PLAYER_STATS,0x7f5124fd1e387744,	CommandStatClearAllSinglePlayerStats);

		SCR_REGISTER_SECURE(STAT_GET_NUMBER_OF_DAYS,0xdb9dd213ed802a90,	CommandStatGetNumberOfDays            );
		SCR_REGISTER_SECURE(STAT_GET_NUMBER_OF_HOURS,0x0896ca1f4f0d8af1,	CommandStatGetNumberOfHours           );
		SCR_REGISTER_SECURE(STAT_GET_NUMBER_OF_MINUTES,0xbb9770585f7d56a2,	CommandStatGetNumberOfMinutes         );
		SCR_REGISTER_SECURE(STAT_GET_NUMBER_OF_SECONDS,0x687230a23f87a4cb,	CommandStatGetNumberOfSeconds         );

		SCR_REGISTER_UNUSED(STAT_CONVERT_MILLISECONDS_TO_DAYS,0x5d186411e9fa592e,	CommandStatMillisecondsConvertToDays    );
		SCR_REGISTER_UNUSED(STAT_CONVERT_MILLISECONDS_TO_HOURS,0x126361977a4e3c28,	CommandStatMillisecondsConvertToHours   );
		SCR_REGISTER_UNUSED(STAT_CONVERT_MILLISECONDS_TO_MINUTES,0x4641d1c177c9b3c6,	CommandStatMillisecondsConvertToMinutes );
		SCR_REGISTER_UNUSED(STAT_CONVERT_MILLISECONDS_TO_SECONDS,0x5579164d8ffbdcf8,	CommandStatMillisecondsConvertToSeconds );
		SCR_REGISTER_UNUSED(STAT_GET_MILLISECONDS_TO_NEAREST_SECOND,0x9faa30d4498b3a47, CommandStatGetMillisecondsToNearestSecond);

		SCR_REGISTER_UNUSED(STAT_GET_TIME_AS_DATE,0xb9287757fd89a7e2, CommandStatGetTimeAsDate);
		SCR_REGISTER_UNUSED(GET_NET_TIMER_AS_DATE,0x2d265cd5dc21337b, CommandStatGetTimeAsDate);

		SCR_REGISTER_SECURE(STAT_SET_PROFILE_SETTING_VALUE,0x0141515577c9beb6, CommandStatSetProfileSettingValue );

#if RSG_GEN9
		SCR_REGISTER_UNUSED(STATS_CHARACTER_CREATION_OUTFIT_SELECTED, 0xd18294dc119f435b, CommandCharacterCreationOutfitSelected);
#endif // RSG_GEN9
		SCR_REGISTER_SECURE(STATS_COMPLETED_CHARACTER_CREATION,0x129022c7ce97a9c6, CommandCompletedCharacterCreation);

		// --- Packed Stats Helper Functions --------------------------------------------------

		SCR_REGISTER_UNUSED(PACKED_STAT_GET_BOOL_STAT_INDEX,0x31f134b0b91d099c, CommandPackedStatGetBoolStatIndex);
		SCR_REGISTER_SECURE(PACKED_STAT_GET_INT_STAT_INDEX,0x4dd6546b33af95be, CommandPackedStatGetIntStatIndex);
		SCR_REGISTER_UNUSED(GET_PACKED_BOOL_STAT_KEY,0x27df3e7d400669bb, CommandGetPackedBoolStatKey);
		SCR_REGISTER_SECURE(GET_PACKED_INT_STAT_KEY,0xf66585648257e5b9, CommandGetPackedIntStatKey);
		SCR_REGISTER_UNUSED(GET_PACKED_TU_BOOL_STAT_KEY,0xe03cb2510772a1a6, CommandGetPackedTUBoolStatKey);
		SCR_REGISTER_SECURE(GET_PACKED_TU_INT_STAT_KEY,0xc3fad245f85182fa, CommandGetPackedTUIntStatKey);
		SCR_REGISTER_UNUSED(GET_PACKED_NG_BOOL_STAT_KEY,0x2da085b3addad1ac, CommandGetPackedNGBoolStatKey);
		SCR_REGISTER_SECURE(GET_PACKED_NG_INT_STAT_KEY,0x246883873095b181, CommandGetPackedNGIntStatKey);

		SCR_REGISTER_UNUSED(STAT_GET_BOOL_MASKED,0xb0546d69db2298a1, CommandGetBoolMasked);
		SCR_REGISTER_UNUSED(STAT_SET_BOOL_MASKED,0x77af9853b955948e, CommandSetBoolMasked);

		SCR_REGISTER_UNUSED(STATS_CHECK_PACKED_STATS_END_VALUE,0x06b559d06f5714fc, CommandCheckPackedStatsEndValue);

		SCR_REGISTER_SECURE(GET_PACKED_STAT_BOOL_CODE,0xda7ebfc49ae3f1b0, CommandPackedStatGetPackedBool);
		SCR_REGISTER_SECURE(GET_PACKED_STAT_INT_CODE,0x0bc900a6fe73770c, CommandPackedStatGetPackedInt);
		SCR_REGISTER_SECURE(SET_PACKED_STAT_BOOL_CODE,0xdb8a58aeaa67cd07, CommandPackedStatSetPackedBool);
		SCR_REGISTER_SECURE(SET_PACKED_STAT_INT_CODE,0x1581503ae529cd2e, CommandPackedStatSetPackedInt);

		// --- Meta ---------------------------------------------------------------------------
		
		SCR_REGISTER_UNUSED(STAT_GET_PERCENT_SHOTS_IN_COVER,0x91194ad83a654cef,	CommandGetPercentShotsInCover		);
		SCR_REGISTER_UNUSED(STAT_GET_PERCENT_SHOTS_IN_CROUCH,0x6f21ca40b1b4bebf,	CommandGetPercentShotsInCrouch		);

		// --- PlayStats ----------------------------------------------------------------------

		DLC_SCR_REGISTER_SECURE(PLAYSTATS_BACKGROUND_SCRIPT_ACTION,0x8f21ac03c0a56f7b,	CommandPlayStatsGenericMetric       );
		SCR_REGISTER_SECURE(PLAYSTATS_NPC_INVITE,0x3a74fc046fc85ede,	CommandPlayStatsNpcInvite           );
		SCR_REGISTER_SECURE(PLAYSTATS_AWARD_XP,0x5842ea5d6a4620e2,	CommandPlayStatsAwardXP             );
		SCR_REGISTER_SECURE(PLAYSTATS_RANK_UP,0xc22e65256a3e9882,	CommandPlayStatsRankUp              );
		SCR_REGISTER_SECURE(PLAYSTATS_STARTED_SESSION_IN_OFFLINEMODE,0xda10adb0fc8885a3, CommandPlayStatsOfflineMode       );
		SCR_REGISTER_SECURE(PLAYSTATS_ACTIVITY_DONE,0xde6c5252c20a2df4, CommandPlayStatsActivity          );
		SCR_REGISTER_SECURE(PLAYSTATS_LEAVE_JOB_CHAIN,0xd2384f6e9d32e19e, CommandPlayStatsLeaveJobChain     );
		SCR_REGISTER_SECURE(PLAYSTATS_MISSION_STARTED,0xb85c479d5c3ea069,	CommandPlayStatsMissionStarted		);
		SCR_REGISTER_SECURE(PLAYSTATS_MISSION_OVER,0xa77f683fa0e7b1f5,	CommandPlayStatsMissionOver         );
		SCR_REGISTER_SECURE(PLAYSTATS_MISSION_CHECKPOINT,0x0b33380e33b61778,	CommandPlayStatsMissionCheckpoint   );
		SCR_REGISTER_SECURE(PLAYSTATS_RANDOM_MISSION_DONE,0x151e30e68b218815,	CommandPlayStatsRandomMissionDone   );
		SCR_REGISTER_SECURE(PLAYSTATS_ROS_BET,0x35b04e6527be3f22,	CommandPlayStatsRosBet              );
		SCR_REGISTER_SECURE(PLAYSTATS_RACE_CHECKPOINT,0x610807472a433d37,	CommandPlayStatsRaceCheckpoint      );
		SCR_REGISTER_UNUSED(PLAYSTATS_POST_MATCH_BLOB,0x99bfa8c541ded993,	CommandPlayStatsPostMatchBlob       );
		SCR_REGISTER_UNUSED(PLAYSTATS_CREATE_MATCH_HISTORY_ID,0x61e9186ebd083d85,	CommandPlayStatsCreateMatchHistoryId);
		SCR_REGISTER_SECURE(PLAYSTATS_CREATE_MATCH_HISTORY_ID_2,0x034e89ed97681ac2,	CommandPlayStatsCreateMatchHistoryId2);
		SCR_REGISTER_SECURE(PLAYSTATS_MATCH_STARTED,0x47236238b4e9bc2d,	CommandPlayStatsMatchStarted        );
		SCR_REGISTER_UNUSED(PLAYSTATS_MATCH_ENDED,0x72dbf875c8c4dd8a,	CommandPlayStatsMatchEnded          );
		SCR_REGISTER_SECURE(PLAYSTATS_SHOP_ITEM,0xb18779ac440ad5c8,	CommandPlayStatsShopItem            );
		SCR_REGISTER_SECURE(PLAYSTATS_CRATE_DROP_MISSION_DONE,0x35dae173a02be828,	CommandPlayStatsCrateDropMissionDone    );
		SCR_REGISTER_SECURE(PLAYSTATS_CRATE_CREATED,0x25d1e85627077680,	CommandPlayStatsCrateCreated);
		SCR_REGISTER_SECURE(PLAYSTATS_HOLD_UP_MISSION_DONE,0x18d3ffc58bbbdfdc,	CommandPlayStatsHoldUpMissionDone       );
		SCR_REGISTER_SECURE(PLAYSTATS_IMPORT_EXPORT_MISSION_DONE,0xb69482ecd5b39c5d,	CommandPlayStatsImportExportMissionDone );
		SCR_REGISTER_UNUSED(PLAYSTATS_SECURITY_VAN_MISSION_DONE,0xb9e49b8648e5c6a0,	CommandPlayStatsSecurityVanMissionDone  );
		SCR_REGISTER_SECURE(PLAYSTATS_RACE_TO_POINT_MISSION_DONE,0x6eaeb9d07327cb9f,	CommandPlayStatsRaceToPointMissionDone  );
		SCR_REGISTER_UNUSED(PLAYSTATS_PROSTITUTE_MISSION_DONE,0xdb7ed300b8bf62bd,	CommandPlayStatsProstitutesMissionDone  );
		SCR_REGISTER_SECURE(PLAYSTATS_ACQUIRED_HIDDEN_PACKAGE,0x599a5bfb6e56ddfc,	CommandPlayStatsAcquiredHiddenPackage   );
		SCR_REGISTER_SECURE(PLAYSTATS_WEBSITE_VISITED,0x615e4a419d932af4,	CommandPlayStatsWebsiteVisited);
		SCR_REGISTER_SECURE(PLAYSTATS_FRIEND_ACTIVITY,0x1a2781cd7cd742e6,	CommandPlayStatsFriendsActivity);
		SCR_REGISTER_SECURE(PLAYSTATS_ODDJOB_DONE,0xa5c246361b61ed23,	CommandPlayStatsOddJobDone);
		SCR_REGISTER_SECURE(PLAYSTATS_PROP_CHANGE,0x182f52ff6d37bd4a,	CommandPlayStatsPropChange);
		SCR_REGISTER_SECURE(PLAYSTATS_CLOTH_CHANGE,0xb3a9c248df46950d,	CommandPlayStatsClothChange);
		SCR_REGISTER_SECURE(PLAYSTATS_WEAPON_MODE_CHANGE,0x79aed75673bc26f9,	CommandPlayStatsWeaponModChange);
		SCR_REGISTER_SECURE(PLAYSTATS_CHEAT_APPLIED,0x0a5ebd0f656b440c,	CommandPlayStatsCheatApplied);
		SCR_REGISTER_SECURE(PLAYSTATS_JOB_ACTIVITY_END,0x7c2f437d5dcee019,	CommandPlayStatsJobActivityEnd );
		SCR_REGISTER_SECURE(PLAYSTATS_JOB_BEND,0x354e2254fc9758f6,	CommandPlayStatsJobBEnd );
		SCR_REGISTER_SECURE(PLAYSTATS_JOB_LTS_END,0xf392afc86853f9a8,	CommandPlayStatsJobLtsEnd );
		SCR_REGISTER_SECURE(PLAYSTATS_JOB_LTS_ROUND_END,0xee86a0e0fa49d8ce,	CommandPlayStatsJobLtsRoundEnd );
		SCR_REGISTER_SECURE(PLAYSTATS_QUICKFIX_TOOL,0x31d208859887778e, CommandPlayStatsQuickFixTool);
		SCR_REGISTER_SECURE(PLAYSTATS_IDLE_KICK,0xa5741f10697506f4,	CommandPlayStatsIdleKick );
		SCR_REGISTER_SECURE(PLAYSTATS_SET_JOIN_TYPE,0xcdb137fb56218d0b, CommandPlayStatsSetOnlineJoinType);
		SCR_REGISTER_UNUSED(PLAYSTATS_CREATOR_USAGE,0xd18ba756f08547a6,	CommandAppendCreatorUsageMetric);
		SCR_REGISTER_SECURE(PLAYSTATS_HEIST_SAVE_CHEAT,0xf3e9ac7d5adcefff,	CommandPlayStatsHeistSaveCheat);
		SCR_REGISTER_UNUSED(PLAYSTATS_APPEND_STARTER_PACK_APPLIED,0x377457ac40fbc473,	CommandAppendStarterPackApplied);
		SCR_REGISTER_SECURE(PLAYSTATS_APPEND_DIRECTOR_METRIC,0x8f37c5ad44e3e390,	CommandAppendDirectorModeMetric);
		SCR_REGISTER_SECURE(PLAYSTATS_AWARD_BAD_SPORT,0xc7e3e54072256976,	CommandAppendAwardBadSport);
		SCR_REGISTER_SECURE(PLAYSTATS_PEGASUS_AS_PERSONAL_AIRCRAFT,0xa1574858139a3894,	CommandAppendPegasusAsPersonalAircraft);

		//////////////////////////////////////////////////////////////////////////

		SCR_REGISTER_SECURE(PLAYSTATS_FM_EVENT_CHALLENGES,0xb6aa2ab935f596ea, CommandPlayStatsFmEventAmbMission_Challenges);
		SCR_REGISTER_SECURE(PLAYSTATS_FM_EVENT_VEHICLETARGET,0xd36a8190bfc150eb, CommandPlayStatsFmEventAmbMission_VehicleTarget);
		SCR_REGISTER_SECURE(PLAYSTATS_FM_EVENT_URBANWARFARE,0x0f8b888069fabc4b, CommandPlayStatsFmEventAmbMission_UrbanWarfare);
		SCR_REGISTER_SECURE(PLAYSTATS_FM_EVENT_CHECKPOINTCOLLECTION,0xa0462f398aa286df, CommandPlayStatsFmEventAmbMission_CheckpointCollection);
		SCR_REGISTER_UNUSED(PLAYSTATS_FM_EVENT_PLANEDROP,0xc09e4a7895c35881, CommandPlayStatsFmEventAmbMission_PlaneDrop);
		SCR_REGISTER_SECURE(PLAYSTATS_FM_EVENT_ATOB,0xf8da4028cd22e160, CommandPlayStatsFmEventAmbMission_ATOB);
		SCR_REGISTER_SECURE(PLAYSTATS_FM_EVENT_PENNEDIN,0x5c348c9363cd3372,	CommandPlayStatsFmEventAmbMission_PennedIn);
		SCR_REGISTER_UNUSED(PLAYSTATS_FM_EVENT_TARGETASSASSINATION,0xf6b5d6c7108b01a9,	CommandPlayStatsFmEventAmbMission_TargetAssassination);
		SCR_REGISTER_SECURE(PLAYSTATS_FM_EVENT_PASSTHEPARCEL,0x7236b5a5338da076,	CommandPlayStatsFmEventAmbMission_PasstheParcel);
		SCR_REGISTER_SECURE(PLAYSTATS_FM_EVENT_HOTPROPERTY,0x036357a803423cff,	CommandPlayStatsFmEventAmbMission_HotProperty);
		SCR_REGISTER_SECURE(PLAYSTATS_FM_EVENT_DEADDROP,0xbaf4c2bad0b0ea1a,	CommandPlayStatsFmEventAmbMission_DeadDrop);
		SCR_REGISTER_SECURE(PLAYSTATS_FM_EVENT_KINGOFTHECASTLE,0xbfc873f8c77d376a,	CommandPlayStatsFmEventAmbMission_KingoftheCastle);
		SCR_REGISTER_SECURE(PLAYSTATS_FM_EVENT_CRIMINALDAMAGE,0xa4acc498bf04bbd2,	CommandPlayStatsFmEventAmbMission_CriminalDamage);
		SCR_REGISTER_UNUSED(PLAYSTATS_FM_EVENT_INFECTION,0x19b6ba588580a582,	CommandPlayStatsFmEventAmbMission_Infection);
		SCR_REGISTER_SECURE(PLAYSTATS_FM_EVENT_COMPETITIVEURBANWARFARE,0xa68ba8226372eb99,	CommandPlayStatsFmEventAmbMission_CompetitiveUrbanWarfare);
		SCR_REGISTER_SECURE(PLAYSTATS_FM_EVENT_HUNTBEAST,0x1a6091d59d217dd9,	CommandPlayStatsFmEventAmbMission_HuntBeast);

		SCR_REGISTER_SECURE(PLAYSTATS_PIMENU_HIDE_OPTIONS,0xd55d9d8a7aa7d37c,	CommandPlayStatsPIMenuHideSettings);


		// --- Leaderboards -------------------------------------------------------------------
		
		SCR_REGISTER_SECURE(LEADERBOARDS_GET_NUMBER_OF_COLUMNS,0x00c49126c192c957,	CommandLeaderboardsGetNumberOfColumns);
		SCR_REGISTER_SECURE(LEADERBOARDS_GET_COLUMN_ID,0x07a9b621946d6d59,	CommandLeaderboardsGetColumnId       );
		SCR_REGISTER_SECURE(LEADERBOARDS_GET_COLUMN_TYPE,0x8cee6633e5a509ad,	CommandLeaderboardsGetColumnType     );

		SCR_REGISTER_UNUSED(LEADERBOARDS_CAN_READ,0x6d57674f672c9076,	CommandLeaderboardsCanRead           );
		SCR_REGISTER_SECURE(LEADERBOARDS_READ_CLEAR_ALL,0x8a3025b5ff237b8e,	CommandLeaderboardsReadClearAll      );
		SCR_REGISTER_SECURE(LEADERBOARDS_READ_CLEAR,0x25cfa7d8d16b29d0,	CommandLeaderboardsReadClear         );
		SCR_REGISTER_SECURE(LEADERBOARDS_READ_PENDING,0x589034697689cc35,	CommandLeaderboardsReadPending       );
		SCR_REGISTER_SECURE(LEADERBOARDS_READ_ANY_PENDING,0xc15645989af376b8,	CommandLeaderboardsReadAnyPending    );
		SCR_REGISTER_UNUSED(LEADERBOARDS_READ_EXISTS,0x8e70e98efae1c9aa,	CommandLeaderboardsReadExists        );
		SCR_REGISTER_SECURE(LEADERBOARDS_READ_SUCCESSFUL,0x4a37499e7c2832d2,	CommandLeaderboardsReadSuccessful    );
		SCR_REGISTER_UNUSED(LEADERBOARDS_READ_SUCCESSFUL_BY_HANDLE,0xc62769774d2f3a98,	CommandLeaderboardsReadSuccessfulByHandle);

		SCR_REGISTER_UNUSED(LEADERBOARDS2_READ_SESSION_PLAYERS_BY_ROW,0xf55d542cdcb05fc4, CommandLeaderboards2ReadSessionPlayersByRow);
		SCR_REGISTER_SECURE(LEADERBOARDS2_READ_FRIENDS_BY_ROW,0x918b101666f9cb83, CommandLeaderboards2ReadFriendsByRow);
		SCR_REGISTER_SECURE(LEADERBOARDS2_READ_BY_HANDLE,0xb3fde7b4cab1c054, CommandLeaderboards2ReadByHandle      );
		SCR_REGISTER_SECURE(LEADERBOARDS2_READ_BY_ROW,0xa9cdb1e3f0a49883, CommandLeaderboards2ReadByRow         );
		SCR_REGISTER_SECURE(LEADERBOARDS2_READ_BY_RANK,0xda3444de6c78d480, CommandLeaderboards2ReadByRank        );
		SCR_REGISTER_SECURE(LEADERBOARDS2_READ_BY_RADIUS,0x0afcb24deebbd31a, CommandLeaderboards2ReadByRadius      );
		SCR_REGISTER_SECURE(LEADERBOARDS2_READ_BY_SCORE_INT,0xace795925470090f, CommandLeaderboards2ReadByScoreInt    );
		SCR_REGISTER_SECURE(LEADERBOARDS2_READ_BY_SCORE_FLOAT,0x493e419f7eeb0a5a, CommandLeaderboards2ReadByScoreFloat  );
		SCR_REGISTER_SECURE(LEADERBOARDS2_READ_RANK_PREDICTION,0x45b097c5e2959673, CommandLeaderboards2ReadRankPrediction);
		SCR_REGISTER_SECURE(LEADERBOARDS2_READ_BY_PLAFORM,0x94df71b0047c472b, CommandLeaderboards2ReadByPlatform  );

		SCR_REGISTER_SECURE(LEADERBOARDS2_READ_GET_ROW_DATA_START,0x581496cff543462f,	CommandLeaderboards2ReadGetRowDataStart );
		SCR_REGISTER_SECURE(LEADERBOARDS2_READ_GET_ROW_DATA_END,0x6ccde78d9c553a24,	CommandLeaderboards2ReadGetRowDataEnd   );
		SCR_REGISTER_SECURE(LEADERBOARDS2_READ_GET_ROW_DATA_INFO,0x1ea20d6fd50c44e7,	CommandLeaderboards2ReadGetRowDataInfo  );
		SCR_REGISTER_SECURE(LEADERBOARDS2_READ_GET_ROW_DATA_INT,0x76c271030ac81113,	CommandLeaderboards2ReadGetRowDataInt   );
		SCR_REGISTER_SECURE(LEADERBOARDS2_READ_GET_ROW_DATA_FLOAT,0x015a67958e8477f3,	CommandLeaderboards2ReadGetRowDataFloat );

		SCR_REGISTER_SECURE(LEADERBOARDS2_WRITE_DATA,0x4a42a2e45a8d2c8b,	CommandLeaderboards2WriteLeaderboardData );
		SCR_REGISTER_SECURE(LEADERBOARDS_WRITE_ADD_COLUMN,0x6d1a116f1b08fa08,	CommandLeaderboardsWriteAddColumn        );
		SCR_REGISTER_SECURE(LEADERBOARDS_WRITE_ADD_COLUMN_LONG,0x05ccdfa8c7547aed,	CommandLeaderboardsWriteAddColumnLong    );

		SCR_REGISTER_SECURE(LEADERBOARDS_CACHE_DATA_ROW,0x513637d173b51a91,	CommandLeaderboardsCacheDataRow          );
		SCR_REGISTER_SECURE(LEADERBOARDS_CLEAR_CACHE_DATA,0x9231b11e90084508,	CommandLeaderboardsClearCacheData        );
		SCR_REGISTER_SECURE(LEADERBOARDS_CLEAR_CACHE_DATA_ID,0x0c2cc1b63aa0b36c,	CommandLeaderboardsClearCachedDisplayDataById );
		SCR_REGISTER_SECURE(LEADERBOARDS_GET_CACHE_EXISTS,0x8b36c5e1084b91ec,	CommandLeaderboardsGetCacheExists        );
		SCR_REGISTER_SECURE(LEADERBOARDS_GET_CACHE_TIME,0xcd72499771f7a0da,	CommandLeaderboardsGetCacheTime          );
		SCR_REGISTER_SECURE(LEADERBOARDS_GET_CACHE_NUMBER_OF_ROWS,0xe02ee9af4474e243,	CommandLeaderboardsGetCacheNumberOfRows  );
		SCR_REGISTER_SECURE(LEADERBOARDS_GET_CACHE_DATA_ROW,0x8de5cbdb548619a2,	CommandLeaderboardsGetCacheDataRow       );

		// --- Presence Events (for stats)---------------------------------------------------------------------------

		SCR_REGISTER_SECURE(PRESENCE_EVENT_UPDATESTAT_INT,0x423a6008ceed5d6c, CommandPresenceEventUpdateStat_Int);
		SCR_REGISTER_SECURE(PRESENCE_EVENT_UPDATESTAT_FLOAT,0x4115628326810a31, CommandPresenceEventUpdateStat_Float);
		SCR_REGISTER_SECURE(PRESENCE_EVENT_UPDATESTAT_INT_WITH_STRING,0xcc0d9986102e9b4f, CommandPresenceEventUpdateStat_IntString);
		SCR_REGISTER_UNUSED(PRESENCE_EVENT_UPDATESTAT_FLOAT_WITH_STRING,0x5c17293d8f69a65d, CommandPresenceEventUpdateStat_FloatString);

		//////////////////////////////////////////////////////////////////////////
		SCR_REGISTER_SECURE(GET_PLAYER_HAS_DRIVEN_ALL_VEHICLES,0x820fa4066d63f3b3, CommandGetPlayerHasDrivenAllVehicles);
		SCR_REGISTER_SECURE(SET_HAS_POSTED_ALL_VEHICLES_DRIVEN,0x0786f84b30fe6f63, CommandSetProfileSettingHasPostedAllVehiclesDriven);
		SCR_REGISTER_SECURE(SET_PROFILE_SETTING_PROLOGUE_COMPLETE,0x01d104274a911a48, CommandSetProfileSettingPrologueComplete);
		SCR_REGISTER_SECURE(SET_PROFILE_SETTING_SP_CHOP_MISSION_COMPLETE,0x909f07ee95db559d, CommandSetProfileSettingSpChopMissionComplete);

		SCR_REGISTER_SECURE(SET_PROFILE_SETTING_CREATOR_RACES_DONE,0x81c6ca52ca44a933, CommandSetProfileSettingRacesDone);
		SCR_REGISTER_SECURE(SET_PROFILE_SETTING_CREATOR_DM_DONE,0x350624f9e8ab4d03, CommandSetProfileSettingDMDone);
		SCR_REGISTER_SECURE(SET_PROFILE_SETTING_CREATOR_CTF_DONE,0xaeb1481d52e85af9, CommandSetProfileSettingCTFDone);

		SCR_REGISTER_SECURE(SET_JOB_ACTIVITY_ID_STARTED,0x3b6c41b60543c47e, CommandSetJobActivityStarted);
		SCR_REGISTER_SECURE(SET_FREEMODE_PROLOGUE_DONE,0xb625f19076319e9c, CommandSetFreemodePrologueDone);
		SCR_REGISTER_SECURE(SET_FREEMODE_STRAND_PROGRESSION_STATUS, 0x79d310a861697cc9, CommandSetFreemodeStrandProgressionStatus);

		SCR_REGISTER_SECURE(STAT_NETWORK_INCREMENT_ON_SUICIDE,0x4ec5e38b70b5ac89, CommandStatNetworkIncrementOnSuicide);

		SCR_REGISTER_SECURE(STAT_SET_CHEAT_IS_ACTIVE,0xada020ed8ed3a5cd, CommandStatSetCheatIsActive);
		SCR_REGISTER_UNUSED(STAT_GET_OPEN_SAVETYPE_IN_JOB,0x5742bbdc30a3db33, CommandStatGetCanSaveDuringJob);
		SCR_REGISTER_SECURE(LEADERBOARDS2_WRITE_DATA_FOR_EVENT_TYPE,0x40ceb23e136d8ea3,	CommandLeaderboards2WriteLeaderboardDataForEventType);
		SCR_REGISTER_SECURE(FORCE_CLOUD_MP_STATS_DOWNLOAD_AND_OVERWRITE_LOCAL_SAVE,0x0de669ef2856bfc5, CommandStatForceCloudProfileDownloadAndOverwriteLocalStats);

		
		//////////////////////////////////////////////////////////////////////////

		SCR_REGISTER_SECURE(STAT_MIGRATE_CLEAR_FOR_RESTART,0x9f6c9aeb9171f802, CommandMigrateClearForRestart);

		SCR_REGISTER_SECURE(STAT_MIGRATE_SAVEGAME_START,0x23bf533940118020, CommandMigrateMultiplayerSavegameStart);
		SCR_REGISTER_SECURE(STAT_MIGRATE_SAVEGAME_GET_STATUS,0x0460a834bafd552a, CommandMigrateMultiplayerSavegameGetStatus);

		SCR_REGISTER_SECURE(STAT_MIGRATE_CHECK_ALREADY_DONE,0x07c58cf1382442d7, CommandMigrateMultiplayerSavegameCheckAlreadyDone);
		SCR_REGISTER_SECURE(STAT_MIGRATE_CHECK_START,0xa1088d3ed018d225, CommandMigrateMultiplayerSavegameCheckStart);
		SCR_REGISTER_SECURE(STAT_MIGRATE_CHECK_GET_IS_PLATFORM_AVAILABLE,0xe448cc16212f2c77, CommandMigrateMultiplayerSavegameGetPlatformIsAvailable);
		SCR_REGISTER_SECURE(STAT_MIGRATE_CHECK_GET_PLATFORM_STATUS,0xf2c5a32ea175f6f2, CommandMigrateMultiplayerSavegameGetPlatformStatus);

		SCR_REGISTER_UNUSED(STAT_GET_PLAYER_HAS_PLAYED_SP_LAST_GEN,0x7a21ae6231f791ce, CommandGetPlayerHasPlayedLastGen);

		SCR_REGISTER_UNUSED(STAT_SAVE_MIGRATION_STATUS_START,0x748942af1e12a63c, CommandStatSaveMigrationStatusStart);
		SCR_REGISTER_SECURE(STAT_GET_SAVE_MIGRATION_STATUS,0x08eae21f06c7a7b5, CommandStatGetSaveMigrationStatus);

		SCR_REGISTER_SECURE(STAT_SAVE_MIGRATION_CANCEL_PENDING_OPERATION,0xb9565218052f429f, CommandStatSaveMigrationCancelPendingOperation);
		SCR_REGISTER_SECURE(STAT_GET_CANCEL_SAVE_MIGRATION_STATUS,0xf8f7ccdaadfec4a1, CommandStatGetCancelSaveMigrationStatus);

		//////////////////////////////////////////////////////////////////////////

		SCR_REGISTER_SECURE(STAT_SAVE_MIGRATION_CONSUME_CONTENT,0xc8f2a29ebfc11d15, CommandStatSaveMigrationConsumeContent);
		SCR_REGISTER_SECURE(STAT_GET_SAVE_MIGRATION_CONSUME_CONTENT_STATUS,0x0b20b5d4d87c4382, CommandStatGetConsumeContentSaveMigrationStatus);

		//////////////////////////////////////////////////////////////////////////

		SCR_REGISTER_SECURE(STAT_ENABLE_STATS_TRACKING,0x59574790c15de236, CommandEnableStatsTracking);
		SCR_REGISTER_SECURE(STAT_DISABLE_STATS_TRACKING,0xbe6819d99223c1f8, CommandDisableStatsTracking);
		SCR_REGISTER_SECURE(STAT_IS_STATS_TRACKING_ENABLED,0x2cef7a2968af81aa, CommandIsStatsTrackingEnabled);

		SCR_REGISTER_SECURE(STAT_START_RECORD_STAT,0x1ccfe7df655ac498, CommandStartRecordStat);
		SCR_REGISTER_SECURE(STAT_STOP_RECORD_STAT,0x2ac5dd7077138d6d, CommandStopRecordStat);
		SCR_REGISTER_SECURE(STAT_GET_RECORDED_VALUE,0xd1b34bb9f529fc07, CommandGetRecordedStatValue);
		SCR_REGISTER_SECURE(STAT_IS_RECORDING_STAT,0xca66ea7984aab667, CommandIsRecordingStat);
	
		SCR_REGISTER_UNUSED(STAT_GET_CURRENT_NEAR_MISS_NOCRASH,0x29640d345d607667, CommandGetCurrentNearMissNoCrashValue);
		SCR_REGISTER_SECURE(STAT_GET_CURRENT_NEAR_MISS_NOCRASH_PRECISE,0xa6b44a0401bc7bdc, CommandGetCurrentNearMissNoCrashPreciseValue);
		SCR_REGISTER_UNUSED(STAT_RESET_CURRENT_NEAR_MISS_NOCRASH,0xed41ff02b5bb53b9, CommandResetCurrentNearMissNoCrash);
		SCR_REGISTER_SECURE(STAT_GET_CURRENT_REAR_WHEEL_DISTANCE,0xe073c52bda52fad1, CommandGetCurrentRearWheelDistance);
		SCR_REGISTER_SECURE(STAT_GET_CURRENT_FRONT_WHEEL_DISTANCE,0x23ccd14221a67576, CommandGetCurrentFrontWheelDistance);
		SCR_REGISTER_UNUSED(STAT_GET_LAST_JUMP_HEIGHT,0x0f5d7211074d2254, CommandGetLastJumpHeight);
		SCR_REGISTER_SECURE(STAT_GET_CURRENT_JUMP_DISTANCE,0x99172e9326fb9464, CommandGetCurrentJumpDistance);
		SCR_REGISTER_SECURE(STAT_GET_CURRENT_DRIVE_NOCRASH_DISTANCE,0xe15f1878b9b5545d, CommandGetCurrentDriveNoCrashDistance);
		SCR_REGISTER_SECURE(STAT_GET_CURRENT_SPEED,0xd6a1e19478669f13, CommandGetCurrentSpeed);
		SCR_REGISTER_UNUSED(STAT_GET_CURRENT_FLIPS,0xb121f715c649d300, CommandGetCurrentFlips);
		SCR_REGISTER_UNUSED(STAT_GET_CURRENT_SPINS,0xf41d98f7f772007d, CommandGetCurrentSpins);
		SCR_REGISTER_SECURE(STAT_GET_CURRENT_DRIVING_REVERSE_DISTANCE,0xed08fd0b61f59a38, CommandGetCurrentDrivingReverseDistance);
		SCR_REGISTER_SECURE(STAT_GET_CURRENT_SKYDIVING_DISTANCE,0x6a91ed71437a1f2c, CommandGetCurrentSkydivingDistance);
		SCR_REGISTER_UNUSED(STAT_GET_CURRENT_FOOT_ALTITUDE,0xbc861984fe512128, CommandGetCurrentFootAltitude);
		SCR_REGISTER_UNUSED(STAT_GET_CHALLENGE_FLYING_TIME,0x9b1a199d1cc1db47, CommandGetFlyingCounter);
		SCR_REGISTER_SECURE(STAT_GET_CHALLENGE_FLYING_DIST,0xb099946873170472, CommandGetFlyingDistance);
		SCR_REGISTER_SECURE(STAT_GET_FLYING_ALTITUDE,0x9c6a175f9509b94d, CommandGetFlyingAltitude);
		SCR_REGISTER_SECURE(STAT_IS_PLAYER_VEHICLE_ABOVE_OCEAN,0x79a47b0799590d3d, CommandIsPlayerVehicleAboveOcean);
		SCR_REGISTER_UNUSED(STAT_GET_PLANE_BARREL_ROLL_COUNT,0x1b44c51cab97394d, CommandGetPlaneBarrelRollCounter);
		SCR_REGISTER_SECURE(STAT_GET_VEHICLE_BAIL_DISTANCE,0x1c976e6e8b56d553, CommandGetVehicleBailDistance);
		SCR_REGISTER_UNUSED(STAT_GET_CURRENT_FREEFALL_ALTITUDE,0xe8cca30684d34472, CommandGetFreeFallAltitude);
        SCR_REGISTER_UNUSED(STAT_GET_LAST_PLAYERS_VEHICLES_DAMAGES,0x283eb1893abba49d, CommandGetLastPlayersVehiclesDamages);
        SCR_REGISTER_UNUSED(STAT_GET_CURRENT_DISTANCE_BEACHING_BOAT,0xfc4e94a9e1cd22db, CommandGetBoatDistanceOnGround);
        SCR_REGISTER_UNUSED(STAT_GET_CURRENT_WALLRIDE_DISTANCE,0xa340eed499d7a1a5, CommandGetWallridingDistance);

		SCR_REGISTER_SECURE(STAT_ROLLBACK_SAVE_MIGRATION,0x13f9b097d05090e4, CommandStatRollbackSaveMigration);

		SCR_REGISTER_SECURE(SET_HAS_SPECIALEDITION_CONTENT,0x212eddd868f364b5, CommandSetProfileSettingHasSpecialEditionContent);
		SCR_REGISTER_SECURE(SET_SAVE_MIGRATION_TRANSACTION_ID_WARNING,0x38c0996ec9115333, CommandSetSaveMigrationTransactionIdWarning);

		SCR_REGISTER_SECURE(GET_BOSS_GOON_UUID,0x77ba8a25e71029a6, CommandGetBossGoonUUID);
		SCR_REGISTER_UNUSED(CLEAR_BOSS_GOON_UUID,0x2d6c553aff8bdf96, CommandClearBossGoonUUID);


		SCR_REGISTER_SECURE(PLAYSTATS_BW_BOSSONBOSSDEATHMATCH,0xa2bf4ac9acf0021a, CommandPlayStatsBw_BossOnBossDeathMatch);
		SCR_REGISTER_SECURE(PLAYSTATS_BW_YATCHATTACK,0x2f1582c801d17816, CommandPlayStatsBw_YatchAttack);
		SCR_REGISTER_SECURE(PLAYSTATS_BW_HUNT_THE_BOSS,0xa3694635b2da5e02, CommandPlayStatsBw_HuntTheBoss);
		SCR_REGISTER_UNUSED(PLAYSTATS_BW_PUT_OUT_A_HIT,0xd0eaa9f9db082fac, CommandPlayStatsBw_PutOutAHit);
		SCR_REGISTER_SECURE(PLAYSTATS_BW_SIGHTSEER,0x7fc250a0d1c39223, CommandPlayStatsBw_Sightseer);
		SCR_REGISTER_SECURE(PLAYSTATS_BW_ASSAULT,0x4cbb7d97ae1390e4, CommandPlayStatsBw_Assault);
		SCR_REGISTER_SECURE(PLAYSTATS_BW_BELLY_OF_THE_BEAST,0x73b0858131673542, CommandPlayStatsBw_BellyOfTheBeast);
        SCR_REGISTER_SECURE(PLAYSTATS_BW_HEAD_HUNTER,0x03d59b76663576e2, CommandPlayStatsBw_Headhunter);
        SCR_REGISTER_SECURE(PLAYSTATS_BW_FRAGILE_GOODS,0x4612c1ed6611a9fc, CommandPlayStatsBw_FragileGoods);
        SCR_REGISTER_SECURE(PLAYSTATS_BW_AIR_FREIGHT,0x7eb13bd67b2c8aa0, CommandPlayStatsBw_AirFreight);

		SCR_REGISTER_SECURE(PLAYSTATS_BC_CAR_JACKING,0x1f7b9822fbcbc8b8, CommandPlayStatsBc_CarJacking);
		SCR_REGISTER_SECURE(PLAYSTATS_BC_SMASH_AND_GRAB,0x187d3bfc3d998a39, CommandPlayStatsBc_SmashAndGrab);
		SCR_REGISTER_SECURE(PLAYSTATS_BC_PROTECTION_RACKET,0x37234f685a03b73f, CommandPlayStatsBc_ProtectionRacket);
		SCR_REGISTER_SECURE(PLAYSTATS_BC_MOST_WANTED,0x95bb1bc7bdff19c2, CommandPlayStatsBc_MostWanted);
		SCR_REGISTER_SECURE(PLAYSTATS_BC_FINDERS_KEEPERS,0x65eb7510f961294b, CommandPlayStatsBc_FindersKeepers);
		SCR_REGISTER_UNUSED(PLAYSTATS_BC_RUNNING_RIOT,0x64b82f7a437e51e6, CommandPlayStatsBc_RunningRiot);
        SCR_REGISTER_SECURE(PLAYSTATS_BC_POINT_TO_POINT,0x219041062b6224d5, CommandPlayStatsBc_PointToPoint);
        SCR_REGISTER_SECURE(PLAYSTATS_BC_CASHING,0xa39339c7305adb9c, CommandPlayStatsBc_Cashing);
        SCR_REGISTER_SECURE(PLAYSTATS_BC_SALVAGE,0x1e7c34e08a322056, CommandPlayStatsBc_Salvage);

        SCR_REGISTER_SECURE(PLAYSTATS_SPENT_PI_CUSTOM_LOADOUT,0x1ca7d9ffee780abd, CommandPlayStatsPiCustomLoadout);
        SCR_REGISTER_SECURE(PLAYSTATS_BUY_CONTRABAND_MISSION,0x3896bbdea3bcafad, CommandPlayStatsBuyContrabandMission);
        SCR_REGISTER_SECURE(PLAYSTATS_SELL_CONTRABAND_MISSION,0x97bb06ae7f3419c6, CommandPlayStatsSellContrabandMission);
        SCR_REGISTER_SECURE(PLAYSTATS_DEFEND_CONTRABAND_MISSION,0xb17bfb605ef6b707, CommandPlayStatsDefendContrabandMission);
        SCR_REGISTER_SECURE(PLAYSTATS_RECOVER_CONTRABAND_MISSION,0xd212223613e42328, CommandPlayStatsRecoverContrabandMission);
        SCR_REGISTER_SECURE(PLAYSTATS_HIT_CONTRABAND_DESTROY_LIMIT,0x7bc2b65f979b471a, CommandPlayStatsHitContrabandDestroyLimit);

		SCR_REGISTER_SECURE(START_BEING_BOSS,0xcc2c5d5d493f83c6, CommandStartBeingBoss);
		SCR_REGISTER_SECURE(START_BEING_GOON,0xbdcc3d18a955cf5e, CommandStartBeingGoon);
		SCR_REGISTER_SECURE(END_BEING_BOSS,0xb9ec19c8ab4c6a90, CommandEndBeingBoss);
		SCR_REGISTER_SECURE(END_BEING_GOON,0x53c6816dab686bad, CommandEndBeingGoon);
		SCR_REGISTER_SECURE(HIRED_LIMO,0xa3fef02369d04d74, CommandHiredLimo);
		SCR_REGISTER_SECURE(ORDER_BOSS_VEHICLE,0xfc0f1f89410e78ca, CommandOrderBossVehicle);
		SCR_REGISTER_SECURE(CHANGE_UNIFORM,0x920f8e60c962fbfd, CommandChangeUniform);
		SCR_REGISTER_SECURE(CHANGE_GOON_LOOKING_FOR_WORK,0x2e47f7ca5ba96254, CommandChangeGoonLookingForWork);

        SCR_REGISTER_SECURE(SEND_METRIC_GHOSTING_TO_PLAYER,0x8717e4a42c91b477, CommandNetworkSendGhostingToPlayer);

        SCR_REGISTER_UNUSED(GET_LAST_STUNTJUMP_ID,0x8e16a719fdd45bd5, CommandStatGetLastSuccessfulStuntJump);

        SCR_REGISTER_SECURE(SEND_METRIC_VIP_POACH,0x129634c27b11e46a, CommandNetworkSendVipPoach);
        SCR_REGISTER_SECURE(SEND_METRIC_PUNISH_BODYGUARD,0xb749f9c25cc70c69, CommandNetworkSendPunishBodyguard);

        SCR_REGISTER_SECURE(PLAYSTATS_START_TRACKING_STUNTS,0x18345c8b764a8920, CommandNetworkStartTrackingStunts);
        SCR_REGISTER_SECURE(PLAYSTATS_STOP_TRACKING_STUNTS,0xa3662e8735ccb6bf, CommandNetworkStopTrackingStunts);

		SCR_REGISTER_UNUSED(STAT_TRACK_AVERAGE_TIME_PER_SESSION,0x13d60a1dcdcc4a23, CommandsStatTrackAverageTimePerSession);

		SCR_REGISTER_SECURE(PLAYSTATS_MISSION_ENDED,0x13832e7d597ddad5, CommandPlaystatsMissionEnded);
		SCR_REGISTER_SECURE(PLAYSTATS_IMPEXP_MISSION_ENDED,0x58e508b2cce30269, CommandPlaystatsImpExpMissionEnded);

		SCR_REGISTER_SECURE(PLAYSTATS_CHANGE_MC_ROLE,0x61778c42b38c1c54, CommandPlaystatsChangeMcRole);
		SCR_REGISTER_SECURE(PLAYSTATS_CHANGE_MC_OUTFIT,0x29e604852f212ac6, CommandPlaystatsChangeMcOutfit);
		SCR_REGISTER_SECURE(PLAYSTATS_SWITCH_MC_EMBLEM,0xd3e8a783e68f3e22, CommandPlaystatsChangeMcEmblem);
		SCR_REGISTER_SECURE(PLAYSTATS_MC_REQUEST_BIKE,0x5828a425c266fc6f, CommandPlaystatsMcRequestBike);
		SCR_REGISTER_SECURE(PLAYSTATS_MC_KILLED_RIVAL_MC_MEMBER,0x8695ebbd3140c524, CommandPlaystatsKilledRivalMcMember);
		SCR_REGISTER_SECURE(PLAYSTATS_ABANDONED_MC,0x82cba4a4eba2bc27, CommandPlaystatsAbandonedMc);
		SCR_REGISTER_SECURE(PLAYSTATS_EARNED_MC_POINTS,0x32f2a84df86ecd0c, CommandPlaystatsEarnedMcPoints);
		SCR_REGISTER_SECURE(PLAYSTATS_MC_FORMATION_ENDS,0x80360c1d11b06af0, CommandPlaystatsMcFormationEnds);
		SCR_REGISTER_SECURE(PLAYSTATS_MC_CLUBHOUSE_ACTIVITY,0xb3f4e4027712226f, CommandPlaystatsMcClubhouseActivity);

		SCR_REGISTER_SECURE(PLAYSTATS_RIVAL_BEHAVIOR,0x05e10ef660f1eaa5, CommandPlaystatsRivalBehaviour);

		SCR_REGISTER_SECURE(PLAYSTATS_COPY_RANK_INTO_NEW_SLOT,0x87f64e95e7d20e35, CommandPlaystatsCopyRankIntoNewSlot);
		SCR_REGISTER_SECURE(PLAYSTATS_DUPE_DETECTED,0xb1f7a9523f08ff13, CommandPlaystatsDupeDetected);
		SCR_REGISTER_SECURE(PLAYSTATS_BAN_ALERT,0x00409823e90f7907, CommandPlaystatsBanAlert);

		SCR_REGISTER_SECURE(PLAYSTATS_GUNRUNNING_MISSION_ENDED,0x806fe7b7db6b4483, CommandPlaystatsGunRunningMissionEnded);
		SCR_REGISTER_SECURE(PLAYSTATS_GUNRUNNING_RND,0x898d135426febf95, CommandPlaystatsGunRunningRnD);

		SCR_REGISTER_SECURE(PLAYSTATS_BUSINESS_BATTLE_ENDED,0x408363ec3f842fc2, CommandPlaystatsBusinessBattleEnded);
		SCR_REGISTER_SECURE(PLAYSTATS_WAREHOUSE_MISSION_ENDED,0x088366fa1148eb2f, CommandPlaystatsWarehouseMissionEnded);
		SCR_REGISTER_SECURE(PLAYSTATS_NIGHTCLUB_MISSION_ENDED,0xd64d32d200423b06, CommandPlaystatsNightclubMissionEnded);
		SCR_REGISTER_SECURE(PLAYSTATS_DJ_USAGE,0x695c81d0332d9916, CommandPlaystatsDjUsage);
		SCR_REGISTER_SECURE(PLAYSTATS_MINIGAME_USAGE,0x6993f138c829b26a, CommandPlaystatsMinigameUsage);
		SCR_REGISTER_SECURE(PLAYSTATS_STONE_HATCHET_ENDED,0xbeb7291ef6b778e3, CommandPlaystatsStoneHatchetEnd);
		

		SCR_REGISTER_SECURE(PLAYSTATS_SMUGGLER_MISSION_ENDED,0xe054822c491e697a, CommandPlaystatsSmugglerMissionEnded);
		SCR_REGISTER_SECURE(PLAYSTATS_FM_HEIST_PREP_ENDED,0x394a4ac6fa3905c0, CommandPlaystatsFreemodePrepEnd);
		SCR_REGISTER_SECURE(PLAYSTATS_INSTANCED_HEIST_ENDED,0xb48ebffa2167e483, CommandPlaystatsInstancedHeistEnd);
		SCR_REGISTER_SECURE(PLAYSTATS_DAR_CHECKPOINT,0x17da863603f94ac8, CommandPlaystatsDarCheckpoint);
		SCR_REGISTER_SECURE(PLAYSTATS_ENTER_SESSION_PACK,0xeb0ddef618b05dc8, CommandPlaystatsEnterSessionPack);

		SCR_REGISTER_UNUSED(APPEND_LAST_VEH_METRIC,0x9b5d2e1b89f58544, CommandAppendLastVehMetric);
		SCR_REGISTER_SECURE(PLAYSTATS_DRONE_USAGE,0x12cd528ec74914fe, CommandPlaystatsDroneUsage);
		
		SCR_REGISTER_SECURE(PLAYSTATS_SPIN_WHEEL,0x0cc7ae3d2fd7dcf2, CommandPlaystatsSpinWheel);
		SCR_REGISTER_SECURE(PLAYSTATS_ARENA_WARS_SPECTATOR,0xaed646ac791f774a, CommandPlaystatsArenaWarsSpectator);
		SCR_REGISTER_SECURE(PLAYSTATS_ARENA_WARS_ENDED,0xfad8de13daf839cc, CommandPlaystatsArenaWarsEnd);

		SCR_REGISTER_SECURE(PLAYSTATS_SWITCH_PASSIVE_MODE,0x5106caaf87c49a90, CommandPlaystatsSwitchPassiveMode);
		SCR_REGISTER_SECURE(PLAYSTATS_COLLECTIBLE_PICKED_UP,0x105d0aa48fb961ae, CommandPlaystatsCollectiblePickedUp);
        
        SCR_REGISTER_SECURE(PLAYSTATS_CASINO_STORY_MISSION_ENDED,0x4c517c8e9091cf2f, CommandPlaystatsCasinoStoryMissionEnded);
        SCR_REGISTER_SECURE(PLAYSTATS_CASINO_CHIP,0x40b7bafae68b3b4b, CommandPlaystatsCasinoChip);
		SCR_REGISTER_SECURE(PLAYSTATS_CASINO_ROULETTE,0xce2b99bd74235922, CommandPlaystatsCasinoRoulette);
		SCR_REGISTER_SECURE(PLAYSTATS_CASINO_BLACKJACK,0xcf03dfe4022cd1aa, CommandPlaystatsCasinoBlackjack);
		SCR_REGISTER_SECURE(PLAYSTATS_CASINO_THREE_CARD_POKER,0x7f6e93cfee0575cc, CommandPlaystatsCasinoThreeCardPoker);
		SCR_REGISTER_SECURE(PLAYSTATS_CASINO_SLOT_MACHINE,0xc4f42fb6ef3d3ad8, CommandPlaystatsCasinoSlotMachine);
		SCR_REGISTER_SECURE(PLAYSTATS_CASINO_INSIDE_TRACK,0x0d72f741f82c7fdb, CommandPlaystatsCasinoInsideTrack);
		SCR_REGISTER_SECURE(PLAYSTATS_CASINO_LUCKY_SEVEN,0x1957acef6a2a721c, CommandPlaystatsCasinoLuckySeven);
		SCR_REGISTER_SECURE(PLAYSTATS_CASINO_ROULETTE_LIGHT,0xb2b72c2a5c575c31, CommandPlaystatsCasinoRouletteLight);
		SCR_REGISTER_SECURE(PLAYSTATS_CASINO_BLACKJACK_LIGHT,0xbde5abc4347750ff, CommandPlaystatsCasinoBlackjackLight);
		SCR_REGISTER_SECURE(PLAYSTATS_CASINO_THREE_CARD_POKER_LIGHT,0x2f56bfe7cf953d90, CommandPlaystatsCasinoThreeCardPokerLight);
		SCR_REGISTER_SECURE(PLAYSTATS_CASINO_SLOT_MACHINE_LIGHT,0x1e63d77aa576606a, CommandPlaystatsCasinoSlotMachineLight);
		SCR_REGISTER_SECURE(PLAYSTATS_CASINO_INSIDE_TRACK_LIGHT,0xddef35d1614e2f2c, CommandPlaystatsCasinoInsideTrackLight);

		SCR_REGISTER_SECURE(PLAYSTATS_ARCADE_GAME,0xc9e0752c5af51dc0, CommandPlaystatsArcadeGame);
		SCR_REGISTER_SECURE(PLAYSTATS_ARCADE_LOVE_MATCH,0x508b99f137224f1d, CommandPlaystatsArcadeLoveMatch);
		SCR_REGISTER_SECURE(PLAYSTATS_FREEMODE_CASINO_MISSION_ENDED,0xf736e9ffb158c31e, CommandPlaystatsFreemodeCasinoMissionEnd);

		// Heist3
		SCR_REGISTER_SECURE(PLAYSTATS_HEIST3_DRONE,0x3410b63b8434966b, CommandPlaystatsHeist3Drone);
		SCR_REGISTER_SECURE(PLAYSTATS_HEIST3_HACK,0x74f42e3fabc4f024, CommandPlaystatsHeist3Hack);
		SCR_REGISTER_SECURE(PLAYSTATS_NPC_PHONE,0xd6cb1a1c9d35d3ef, CommandPlaystatsNpcPhone);
		SCR_REGISTER_SECURE(PLAYSTATS_ARCADE_CABINET,0xe302e901503a46d8, CommandPlaystatsArcadeCabinet);
		SCR_REGISTER_SECURE(PLAYSTATS_HEIST3_FINALE,0xa05816757b8077e8, CommandPlaystatsHeist3Finale);
		SCR_REGISTER_SECURE(PLAYSTATS_HEIST3_PREP,0x71e002586231cecc, CommandPlaystatsHeist3Prep);
		SCR_REGISTER_SECURE(PLAYSTATS_MASTER_CONTROL,0xf4325aa893d9a6c1, CommandPlaystatsMasterControl);
		SCR_REGISTER_SECURE(PLAYSTATS_QUIT_MODE,0x5c1faee011fb9409, CommandPlaystatsQuitMode);
		SCR_REGISTER_SECURE(PLAYSTATS_MISSION_VOTE,0x73fc05558dd31e81, CommandPlaystatsMissionVote);
		SCR_REGISTER_SECURE(PLAYSTATS_NJVS_VOTE,0xf9b99a9cf272cad8, CommandPlaystatsNjvsVote);

		SCR_REGISTER_SECURE(PLAYSTATS_KILL_YOURSELF,0x4e6031b668a3f214, CommandPlaystatsKillYourself);

		SCR_REGISTER_UNUSED(PLAYSTATS_DISPATCH_ENDED,0xb1a79ef7f74902ac, CommandPlaystatsDispatchEnded);

		SCR_REGISTER_UNUSED(PLAYSTATS_YACHT_ENDED,0x114b6941bf75808d, CommandPlaystatsYachtEnded);

		SCR_REGISTER_SECURE(PLAYSTATS_FM_MISSION_END,0x46a70777be6ceab9, CommandPlaystatsFreemodeMissionEnd);

#if ARCHIVED_SUMMER_CONTENT_ENABLED   
		// Cops & Crooks
		SCR_REGISTER_UNUSED(PLAYSTATS_CNC_ROUND,0x4f7fd2564eaed5a2, CommandPlaystatsCncRound);
		SCR_REGISTER_UNUSED(PLAYSTATS_CNC_LOBBY,0xc5f96cbd06e73bbe, CommandPlaystatsCncLobby);
		SCR_REGISTER_UNUSED(PLAYSTATS_CNC_STASH,0x1bb84b52fb2d718b, CommandPlaystatsCncStash);
		SCR_REGISTER_UNUSED(PLAYSTATS_CNC_SAFE,0xa0f24832987635fb, CommandPlaystatsCncSafe);
		SCR_REGISTER_UNUSED(PLAYSTATS_CNC_UNLOCK,0xa6f2e439090f5d6a, CommandPlaystatsCncUnlock);
		SCR_REGISTER_UNUSED(PLAYSTATS_CNC_ABILITY,0xbc247efa1bd9dbdb, CommandPlaystatsCncAbility);
		SCR_REGISTER_UNUSED(PLAYSTATS_CNC_INCAPACITATE,0x6aeba036b8703fda, CommandPlaystatsCncIncapacitate);
		SCR_REGISTER_UNUSED(PLAYSTATS_CNC_JUSTICE,0xe06b603b521a57b5, CommandPlaystatsCncJustice);
		SCR_REGISTER_UNUSED(PLAYSTATS_CNC_WORK,0x4897a470c8c80f4e, CommandPlaystatsCncWork);
		SCR_REGISTER_UNUSED(PLAYSTATS_CNC_SWITCH,0x891feb9d8b1a8fb8, CommandPlaystatsCncSwitch);
		SCR_REGISTER_UNUSED(PLAYSTATS_CNC_JOBS,0x747378dcd7ccf09f, CommandPlaystatsCncJobs);
#endif

		// Heist 4
		SCR_REGISTER_SECURE(PLAYSTATS_HEIST4_PREP,0x95663ae09f86ee73, CommandPlaystatsHeist4Prep);
		SCR_REGISTER_SECURE(PLAYSTATS_HEIST4_FINALE,0xa2f2a0d01db6acaf, CommandPlaystatsHeist4Finale);
		SCR_REGISTER_SECURE(PLAYSTATS_HEIST4_HACK,0x4f4c481abe5088bc, CommandPlaystatsHeist4Hack);
		SCR_REGISTER_SECURE(PLAYSTATS_SUB_WEAP,0x1000df2eb45e521c, CommandPlaystatsSubWeap);
		SCR_REGISTER_SECURE(PLAYSTATS_FAST_TRVL,0xbe608f8c2140bdfd, CommandPlaystatsFastTrvl);
		SCR_REGISTER_SECURE(PLAYSTATS_HUB_ENTRY,0x61bb55e2dd91d18b, CommandPlaystatsHubEntry);
		SCR_REGISTER_SECURE(PLAYSTATS_DJ_MISSION_ENDED,0xa0ee58df5f161a36, CommandPlaystatsDjMissionEnded);

		// Tuner
		SCR_REGISTER_SECURE(PLAYSTATS_ROBBERY_PREP,0xdd14b13c611c2a72, CommandPlaystatsRobberyPrep);
		SCR_REGISTER_SECURE(PLAYSTATS_ROBBERY_FINALE,0xe0ee314a99919d15, CommandPlaystatsRobberyFinale);
		SCR_REGISTER_SECURE(PLAYSTATS_EXTRA_EVENT,0xbf65cdc22b435e64, CommandPlaystatsExtraEvent);
		SCR_REGISTER_SECURE(PLAYSTATS_CARCLUB_POINTS,0x533e3fa298cc9b99, CommandPlaystatsCarclubPoints);
		SCR_REGISTER_SECURE(PLAYSTATS_CARCLUB_CHALLENGE,0x3ad2a952db41e956, CommandPlaystatsCarclubChallenge);
		SCR_REGISTER_SECURE(PLAYSTATS_CARCLUB_PRIZE,0x2fc88162813e5509, CommandPlaystatsCarclubPrize);

		SCR_REGISTER_SECURE(PLAYSTATS_AWARD_NAV,0x70f52471e758ebae, CommandPlaystatsAwardNav);
		SCR_REGISTER_UNUSED(PLAYSTATS_PM_NAV,0xe7cbbe6c2462c51c, CommandPlaystatsPmNav);
		
		SCR_REGISTER_SECURE(PLAYSTATS_INST_MISSION_END,0xfea3f7e83c0610fa, CommandPlaystatsInstMissionEnd);
		SCR_REGISTER_SECURE(PLAYSTATS_HUB_EXIT,0x5a46ace5c4661132, CommandPlaystatsHubExit);

#if RSG_GEN9
		SCR_REGISTER_UNUSED(PLAYSTATS_LP_NAV, 0x88e31379119430b6, CommandPlaystatsLpNav);
		SCR_REGISTER_UNUSED(PLAYSTATS_UGC_NAV, 0x9a387654cd8f86fc, CommandPlaystatsUgcNav);
#endif // RSG_GEN9

		SCR_REGISTER_UNUSED(PLAYSTATS_MAP_NAV, 0x3dba6d27e8822127, CommandPlaystatsMapNav);

		// Summer 2022
		SCR_REGISTER_UNUSED(PLAYSTATS_VEH_DEL, 0xee1db1f96b06dda7, CommandPlaystatsVehDelInfo);
		SCR_REGISTER_SECURE(PLAYSTATS_INVENTORY, 0x887dad63cf5b7908, CommandPlaystatsInventoryInfo);

	}

}  // end of namespace stats_commands


#undef __script_channel
#define __script_channel script

//eof 

