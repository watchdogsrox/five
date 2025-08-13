//
// StatsMgr.h
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

#ifndef STATSMGR_H
#define STATSMGR_H


// --- Include Files ------------------------------------------------------------

// rage headers
#include "atl/array.h"
#include "vector/vector3.h"
#include "vector/vector4.h"
#include "fwutil/Flags.h"
#include "atl/hashstring.h"
#include "atl/string.h"
#include "atl/singleton.h"

// Game headers
#include "Stats/StatsTypes.h"
#include "peds/pedtype.h"
#include "text/TextFile.h"
#include "weapons/WeaponEnums.h"
#include "task/System/Tuning.h"
#include "network/cloud/CloudManager.h"

// --- Forward Definitions ------------------------------------------------------
class CVehicle;
class CBike;
class CBmx;
class CBoat;
class CTrain;
class CStatsDataMgr;
class CPed;
class CWeapon;
class CEntity;
class CAsyncProbeHelper;

// --- Structure/Class Definitions ----------------------------------------------

//Our product ids information.


//PURPOSE
//   Records for vehicle usage analytics.
class CStatsVehicleUsage
{
	friend class CStatsMgr;

public:
	static const u32   VEHICLE_MIN_DRIVE_TIME = 500;
	static const float VEHICLE_MIN_DRIVE_DIST;

public:
	u32     m_VehicleId;
	u32     m_CharacterId;
	u32     m_TimeDriven;
	float   m_DistDriven;
	float   m_LastDistDriven;
	u32     m_NumDriven;
	bool    m_HasSetNumDriven;
	u32     m_NumStolen;
	u32     m_NumSpins;
	u32     m_NumFlips;
	u32     m_NumPlaneLandings;
	u32     m_NumWheelies;
	u32     m_NumAirLaunches;
	u32     m_NumAirLaunchesOver5s;
	u32     m_NumAirLaunchesOver5m;
	u32     m_NumAirLaunchesOver40m;
	u32     m_NumLargeAccidents;
	u32     m_TimeSpentInVehicle;
	float   m_HighestSpeed;
	u32     m_LongestWheelieTime;
	float   m_LongestWheelieDist;
	float   m_HighestJumpDistance;
	u32     m_LastTimeSpentInVehicle;
	u32     m_NumPedsRundown;
	u32		m_Location;
	bool    m_Online;
	bool	m_Owned;
	PAR_SIMPLE_PARSABLE

public:
	CStatsVehicleUsage() 
		: m_VehicleId(0)
		, m_CharacterId(CHAR_INVALID)
		, m_TimeDriven(0)
		, m_DistDriven(0.0f)
		, m_LastDistDriven(0.0f)
		, m_NumDriven(0) 
		, m_HasSetNumDriven(false)
		, m_NumStolen(0) 
		, m_NumSpins(0)
		, m_NumFlips(0)
		, m_NumPlaneLandings(0)
		, m_NumWheelies(0)
		, m_NumAirLaunches(0)
		, m_NumAirLaunchesOver5s(0)
		, m_NumAirLaunchesOver5m(0)
		, m_NumAirLaunchesOver40m(0)
		, m_NumLargeAccidents(0)
		, m_TimeSpentInVehicle(0)
		, m_HighestSpeed(0.0f)
		, m_LongestWheelieTime(0)
		, m_LongestWheelieDist(0.0f)
		, m_HighestJumpDistance(0.0f)
		, m_LastTimeSpentInVehicle(0)
		, m_NumPedsRundown(0)
		, m_Location(0)
		, m_Online(false)
		, m_Owned(false)
	{
		Clear();
	}
	~CStatsVehicleUsage() {;}

	void Clear(const bool clearlastrecords = true);

	bool       IsValid() const {return (m_VehicleId!=0 && m_CharacterId!=CHAR_INVALID);}
	bool      IsDriven() const {return (IsValid() && m_TimeDriven>=VEHICLE_MIN_DRIVE_TIME && m_DistDriven>=VEHICLE_MIN_DRIVE_DIST && m_NumDriven>0);}
	bool      IsStolen() const {return (IsValid() && m_NumStolen>0);}
	bool  CanBeCleared() const {return (!IsDriven());}

	//Called when we need to close current analytics tracking
	void  EndVehicleStats();

	CStatsVehicleUsage& operator=(const CStatsVehicleUsage& other)
	{
		m_VehicleId              = other.m_VehicleId;
		m_CharacterId            = other.m_CharacterId;
		m_TimeDriven             = other.m_TimeDriven;
		m_DistDriven             = other.m_DistDriven;
		m_LastDistDriven         = other.m_LastDistDriven;
		m_NumDriven              = other.m_NumDriven;
		m_HasSetNumDriven        = other.m_HasSetNumDriven;
		m_NumStolen              = other.m_NumStolen;
		m_NumSpins               = other.m_NumSpins;
		m_NumFlips               = other.m_NumFlips;
		m_NumPlaneLandings       = other.m_NumPlaneLandings;
		m_NumWheelies            = other.m_NumWheelies;
		m_NumAirLaunches         = other.m_NumAirLaunches;
		m_NumAirLaunchesOver5s   = other.m_NumAirLaunchesOver5s; 
		m_NumAirLaunchesOver5m   = other.m_NumAirLaunchesOver5m;
		m_NumAirLaunchesOver40m  = other.m_NumAirLaunchesOver40m;
		m_NumLargeAccidents      = other.m_NumLargeAccidents;
		m_TimeSpentInVehicle     = other.m_TimeSpentInVehicle;
		m_HighestSpeed           = other.m_HighestSpeed;
		m_LongestWheelieTime     = other.m_LongestWheelieTime;
		m_LongestWheelieDist     = other.m_LongestWheelieDist;
		m_HighestJumpDistance    = other.m_HighestJumpDistance;
		m_Online                 = other.m_Online;
		m_NumPedsRundown         = other.m_NumPedsRundown;
		m_LastTimeSpentInVehicle = other.m_LastTimeSpentInVehicle;

		return *this;
	}
};

class NonFlyableArea
{
public:
	virtual ~NonFlyableArea(){}        // Virtual destructor

	::rage::Vector4        m_rectXYWH;		// X,Y is the bottom left corner, H and W are height and width

	PAR_PARSABLE;
};

class NonFlyableAreaArray
{
public:

	virtual ~NonFlyableAreaArray(){}        // Virtual destructor
	::rage::atArray< NonFlyableArea, 0, ::rage::u16>         m_areas;

	PAR_PARSABLE;
};


// PURPOSE:	Tuning parameter struct.
struct sStatsMetadataTuning
{
	sStatsMetadataTuning() 
		: m_FreefallTimeSinceLastDamage(1000)
		, m_AwardVehicleJumpTime(5000)
		, m_AwardParachuteJumpTime(180000)
		, m_SPLargeAccidenThresold(300.0f)
		, m_MPLargeAccidenThresold(300.0f)
		, m_FreefallThresold(2.2f)
		, m_AwardVehicleJumpDistanceA(5.0f)
		, m_AwardVehicleJumpDistanceB(100.0f)
		, m_AwardParachuteJumpDistanceA(40.0f)
		, m_AwardParachuteJumpDistanceB(50.0f)
	{;}
	virtual ~sStatsMetadataTuning() { }

	u32    m_FreefallTimeSinceLastDamage;  //maximum time to consider freefall caused by last damage. 
	u32    m_AwardVehicleJumpTime;         //Award vehicle jump over 5 seconds
	u32    m_AwardParachuteJumpTime;       //Award Parachute jump over 60 seconds
	float  m_SPLargeAccidenThresold;       //Threshold for registering a large accident the player has had.
	float  m_MPLargeAccidenThresold;       //Threshold for registering a large accident the player has had.
	float  m_FreefallThresold;             //Minimum distance to consider freefall.
	float  m_AwardVehicleJumpDistanceA;    //Award vehicle jump over 5 meters
	float  m_AwardVehicleJumpDistanceB;    //Award vehicle jump over 40 meters
	float  m_AwardParachuteJumpDistanceA;  //Award Parachute jump over 50 meters
	float  m_AwardParachuteJumpDistanceB;  //Award Parachute jump over 40 meters
	NonFlyableAreaArray m_nonFlyableAreas;
	PAR_PARSABLE;
};

//PURPOSE
//   Records and manages all the statistics in the game.
class CStatsMgr
{
	// CStatsUtils can access private part
	friend class CStatsUtils;
	friend class CStatsSaveStructure;
	friend class CMultiplayerStatsSaveStructure;

public:
	// Invert flight dot product value check
	static float minInvertFlightDot;

	//Have a record of the last x Peds that the player has runned down
	static const u32 NUM_PEDS_TO_TRACK = 10;

	//Have a record of the last x vehicles analytics to be flushed to the 
	static const u32  NUM_VEHICLE_RECORDS  = 20;
	static const u32  INVALID_RECORD       = NUM_VEHICLE_RECORDS;


	// Please keep in sync with the enum in commands_stats.sch
	enum RecordStat
	{
		REC_STAT_NONE,
		REC_STAT_LONGEST_WHEELIE_DIST,
		REC_STAT_LONGEST_STOPPIE_DIST,
		REC_STAT_LONGEST_DRIVE_NOCRASH,
		REC_STAT_TOP_SPEED_CAR,
		REC_STAT_MOST_FLIPS_IN_ONE_JUMP,
		REC_STAT_MOST_SPINS_IN_ONE_JUMP,
		REC_STAT_HIGHEST_JUMP_REACHED,
		REC_STAT_FARTHEST_JUMP_DIST,
		REC_STAT_NUMBER_NEAR_MISS_NOCRASH,
		REC_STAT_LONGEST_SURVIVED_FREEFALL,
		REC_STAT_LOWEST_PARACHUTE_OPEN,
		REC_STAT_DIST_DRIVING_CAR_REVERSE,
		REC_STAT_LONGEST_SURVIVED_SKYDIVE,
		REC_STAT_NUMBER_STOLEN_VEHICLE,
		REC_STAT_PLAYERS_SET_ON_FIRE,
		REC_STAT_ON_FOOT_ALTITUDE,
		REC_STAT_FLIGHT_TIME_BELOW_20,
		REC_STAT_FLIGHT_DIST_BELOW_20,
		REC_STAT_FLIGHT_TIME_BELOW_100,
		REC_STAT_FLIGHT_DIST_BELOW_100,
		REC_STAT_PLANE_BARREL_ROLLS,
		REC_STAT_MELEE_KILLED_PLAYERS,
		REC_STAT_SNIPER_KILL_DISTANCE,
		REC_STAT_SNIPER_KILL,
		REC_STAT_DB_NON_TURRET_PLAYER_KILLS,
		REC_STAT_PLAYER_HEADSHOTS,
		REC_STAT_DIST_BAILING_FROM_VEHICLE,
		REC_STAT_PLAYER_VEHICLE_DAMAGES,
        REC_STAT_NEAR_MISS_PRECISE,
        REC_STAT_DIST_BEACHING_BOAT,
        REC_STAT_DIST_WALLRIDE
	};

	enum RecordStatPolicy
	{
		RSP_NONE,
		RSP_SUM,
		RSP_GREATEST,
		RSP_LOWEST,
	};

private:

	//Stats tunables
	static sStatsMetadataTuning sm_TuneMetadata;

	// Manages all the data of the stats.
	static CStatsDataMgr sm_StatsData;

	// --- Some stats are arranged in arrays by themselves for convenience ---------------------

	//Track vehicle analytics that is flushed every LB_FLUSH_INTERVAL or when its full.
	static atFixedArray <CStatsVehicleUsage, NUM_VEHICLE_RECORDS> sm_vehicleRecords;
	static u32  sm_curVehIdx; //Current vehicle record.

	//Track Peds killed by the player.
	static atFixedArray<int, PEDTYPE_LAST_PEDTYPE> sm_PedsKilledOfThisType;

	//Track Peds runned down by the player.
	static atFixedArray <u16, NUM_PEDS_TO_TRACK> sm_PedsRunDownRecords;

	//Track Number of tyres Popped by Gunshot.
	static atArray< bool > sm_WheelStatus;

	//Track session playing time.
	static float sm_SessionRunningTime;
	static float sm_PlayingTime;
	static float sm_StealthTime;
	static float sm_FirstPersonTime;
	static float sm_3rdPersonTime;

	//Track stats that depend on checking if the player is dead.
	static bool  sm_VehicleDeadCheck;
	static float sm_VehicleDeadCheckCounter;

	//Track Longest Drive Without a crash
	static float sm_DriveNoCrashTime;
	static float sm_DriveNoCrashDist;

	// Track most near misses without crashing
	static u32 sm_NearMissNoCrash;

	static float sm_JumpDistance;
	static float sm_LastJumpHeight;
	static float sm_CurrentSpeed;

	//Track Longest time spent driving in cinematic camera
	static float sm_DriveInCinematic;

	// Tracks time player was close to other vehicle
	static const int MAXIMUM_NEAR_MISS_COUNTER = 5;
	static u32 sm_NearMissNoCrashPrecise;
	static u32 sm_NearMissCounterArray[MAXIMUM_NEAR_MISS_COUNTER];

	//Track of weapon firing within states.
	static bool sm_bWasInCover;
	static bool sm_bShotFiredInCover;
	static bool sm_bWasCrouching;
	static bool sm_bShotFiredInCrouch;

	//Track player in Vehicle
	static bool  sm_WasInVehicle;

	//Track tires popped by gun shot.
	static u16 sm_VehicleRandomSeed;

	//Track Last vehicle stolen index
	static u16 sm_LastVehicleStolenSeed;
	static u16 sm_LastVehicleStolenForChallenge;

	//Track killing spree.
	static u32 sm_KillingSpreeTime;
	static u32 sm_KillingSpreeTotalTime;
	static u32 sm_KillingSpreeCounter;

	//Track vehicles destroyed in spree.
	static u32 sm_VehicleDestroyedSpreeTime;
	static u32 sm_VehicleDestroyedSpreeTotalTime;
	static u32 sm_VehicleDestroyedSpreeCounter;

	//Track vehicles damage.
	static u32   sm_CrashedVehicleKeyHash;
	static u32   sm_CrashedVehicleTimer;
	static float sm_CrashedVehicleDamageAccumulator;


	//Track different models driven - MaxVehicleModelInfos
	static atArray < u32 > sm_SpVehicleDriven;
	static atArray < u32 > sm_MpVehicleDriven;
	static u32   sm_currCountAsFacebookDriven;
	static u32   sm_maxCountAsFacebookDriven;

	//Work out player vehicle stats
	static Vector3 sm_VehiclePreviousPosition;
	static u32     sm_CarTwoWheelCounter;        //How long has player's car been on two wheels
	static float   sm_CarTwoWheelDist;           // 
	static float   sm_CarLess3WheelCounter;      //How long has player's car been on less than 3 wheels
	static u32     sm_BikeRearWheelCounter;      //How long has player's bike been on rear wheel only
	static float   sm_BikeRearWheelDist;         // 
	static bool    sm_IsWheelingOnPushBike;		 //Is the player currently doing a wheelie on a BMX (push bike)
	static bool    sm_IsDoingAStoppieOnPushBike; //Is the player currently doing a stoppie on a BMX (push bike)
	static u32     sm_BikeFrontWheelCounter;     //How long has player's bike been on front wheel only
	static float   sm_BikeFrontWheelDist;        //
	static u32     sm_TempBufferCounter;         //So wheels can leave the ground for a few frames without stopping above counters
	//Best values for the script to check - will be zero most of the time, only value
	// when finished trick - script should retrieve value then reset to zero
	static u32     sm_BestCarTwoWheelsTimeMs;
	static float   sm_BestCarTwoWheelsDistM;
	static u32     sm_BestBikeWheelieTimeMs;
	static float   sm_BestBikeWheelieDistM;
	static u32     sm_BestBikeStoppieTimeMs;
	static float   sm_BestBikeStoppieDistM;

	//Track vehicle air jumps
	static u32     sm_ZeroWheelCounter;
	static Vector3 sm_ZeroWheelPosStart;
	static Vector3 sm_ZeroWheelPosEnd;
	static Vector3 sm_ZeroWheelPosHeight;
	static u32     sm_ZeroWheelBufferCounter;  //So wheels can leave the ground for a few frames without stopping above counters

	//Track the parachute jump distance.
	// Parachute start pos is the position when you started falling with a parachute in the back, in the parachute falling pose
	static Vector3 sm_ParachuteStartPos;
	static u32     sm_ParachuteStartTime;
	// Parachute start pos is the position when you actually opened the parachute
	static Vector3 sm_ParachuteDeployPos;
	static bool    sm_HasStartedParachuteDeploy;
	// As you can bail from parachuting, this will keep track of it
	static bool    sm_hasBailedFromParachuting;

	//Track player in air stats
	static bool    sm_WasInAir;
	static Vector3 sm_FreeFallStartPos;
	static bool    sm_FreeFallDueToPlayerDamage;
	static u32     sm_FreeFallStartTimer;
	static Vector3 sm_ChallengeSkydiveStartPos;

	//Use an asynchronous probe helper to manage the probes used to determine the altitude of stats.
	static CAsyncProbeHelper&  sm_GroundProbeHelper;
	static u32                 sm_TimeSinceLastGroundProbe;

	//Used to track number of vehicle flips.
	static bool  sm_VehicleFlipsCounter;
    static u32   sm_VehicleFlipsAccumulator;
    static float  sm_VehicleFlipsHeadingLast;

    //Used to track number of vehicle spins.
    static float  sm_VehicleSpinsAccumulator;
    static float  sm_VehicleSpinsHeadingLast;

    //Used to track number of vehicle rotations.
    static float  sm_VehicleRollsAccumulator;
    static float  sm_VehicleRollsHeadingLast;


	//Used to track good plane landings
	static u32 sm_InAirTimer;
	static int sm_LandingTimer;

	//TRUE when the player has cheated recently
	static bool sm_cheatIsActive;
	static u32  sm_cheatIsActiveTimer;

	//Holds the vehicle altitude when he has all 4 wheels on the ground
	static float sm_hydraulicJumpVehicleAltitude;


	// Recorded stat
	static RecordStat m_recordedStat;
	static RecordStatPolicy m_statRecordingPolicy;
	static float m_recordedStatValue;
   
	// Track jump distance separately for challenges
	static Vector3 sm_ChallengeZeroWheelPosStart;
	static bool    sm_invalidJump;

	static Vector3 sm_ReverseDrivingPosCurrent;
	static float sm_CurrentDrivingReverseDistance;
	static float sm_SkydiveDistance;
	static float sm_FreeFallAltitude;

	static Vector3 sm_OnFootPosStart;
	static float sm_FootAltitude;

	static bool sm_drivingFlyingVehicle;
	static bool sm_drivingFlyingVehicleForProbe;
	static Vector3 sm_FlyingPlanePosHeight;
	static Vector3 sm_LastFlyingPlanePosition;
	static float sm_FlyingAltitude;
	static bool sm_ValidFlyingAltitude;
	static float sm_FlyingCounter;
	static float sm_FlyingDistance;
	static float sm_PlaneBarrelRollCounter;
	static float sm_PlaneRollAccumulator;
	static float sm_PlaneRollLast;

	static Vector3 sm_StartVehicleBail;
	static Vector3 sm_LastVehicleBailPosition;
	static float sm_VehicleBailDistance;

	static float sm_PlayersVehiclesDamages;

	// Boat beaching tracking
	static Vector3 sm_BoatPositionOnGround;
	static Vector3 sm_LastBoatPositionInWater;
	static float sm_BoatDistanceOnGround;
	static bool sm_wasInWaterPreviously;
	static bool sm_outOfWater;
	static float sm_inAirAfterGroundTimer;

	static bool sm_HadAttachedParentVehicle;
	static float sm_FootOnVehicleTimer;

	static const u32 TIME_CONSIDERED_ON_VEHICLE_AFTER_CONTACT = 800;
	static u32 sm_TimerAfterOnAnotherVehicle;
	static bool sm_IsOnAnotherVehicle;
	static bool sm_WasOnAnotherVehicleWhenJumped;

	static u32 sm_MostRecentCollisionTimeWithPlaneInAir;

    static Vector3 sm_LastWallridePosition;
    static float sm_WallrideDistance;
    static float sm_WallrideBufferTimer;
    static const float WALLRIDE_BUFFERTIMER_VALUE;

public:

	// PURPOSE:  Access the data manager.
	static CStatsDataMgr& GetStatsDataMgr() { return sm_StatsData; }

	static void  Init(unsigned initMode);
	static void  Shutdown(unsigned shutdownMode);
	static void  OpenNetwork();
	static void  CloseNetwork();
	static void  WriteAfterGameLoad();

	// PURPOSE: Called when the player signs out locally.
	static void  SignedOut();
	static void  SignedOffline();

	// PURPOSE: Called when the player signs in locally.
	static void  SignedIn();

	// PURPOSE:  Displays a help message to say which stat has been altered and by how much
	static void  DisplayScriptStatUpdateMessage(bool IncOrDec, const StatId& keyHash, float fValue);

	static const int NEAR_MISS_TIMER = 500;
#if __BANK
	static void	 InitWidgets();

	static void drawNonFlyableAreas();
#endif

	// --- Modify Stats ---------------------------------------------

	// PURPOSE:  Increments the stat by amount.
	// NOTES:    Can not be used with boolean values and strings ... 
	static void  IncrementStat(const StatId& keyHash, const float fAmount, const u32 flags = STATUPDATEFLAG_DEFAULT);

	// PURPOSE:  Decrements the stat by amount.
	// NOTES:    Can not be used with boolean values and strings ... 
	static void  DecrementStat(const StatId& keyHash, const float fAmount, const u32 flags = STATUPDATEFLAG_DEFAULT);

	// PURPOSE: Set a new value to the stat if the new value is greater than the current value.
	// NOTES:    Can not be used with boolean values and strings ... 
	static void  SetGreater(const StatId& keyHash, const float fNewAmount, const u32 flags = STATUPDATEFLAG_DEFAULT);

	// PURPOSE: Set a new value to the stat if the new value is lesser than the current value.
	// NOTES:    Can not be used with boolean values and strings ... 
	static void  SetLesser(const StatId& keyHash, const float fNewAmount, const u32 flags = STATUPDATEFLAG_DEFAULT);

	// PURPOSE:  Sets a new MIN value.
	// NOTES:    Can only be used with int and floats
	static void  RegisterMinBetterValue(const StatId& keyHash, int iNewTime, const u32 flags = STATUPDATEFLAG_DEFAULT);

	// PURPOSE:  Update generic stats. Should be called every frame
	static void  Update();

	// PURPOSE:  Update stats when player leaves and enters a vehicle.
	static void  PlayerEnteredVehicle(const CVehicle* vehicle);
	static void  PlayerLeftVehicle(const CVehicle* vehicle);

	// PURPOSE:  Update Stats when a vehicle is stolen
	static bool  UpdateVehiclesStolen(const CVehicle* vehicle);
	// PURPOSE:  Update Stats when player respaw.
	static void  UpdateStatsOnRespawn();

	// PURPOSE:  Register vehicles near misses.
	static void  RegisterVehicleNearMiss(const CPed* driver, const u32 time);

	// PURPOSE:  Register player close to another vehicle
	static void  RegisterVehicleNearMissPrecise(const CPed* playerDriver, const u32 time);
	static void  RegisterVehicleNearMissAccepted();

	// PURPOSE:  Register stats hits on peds and vehicles done by the local player with explosions.
	static void  RegisterExplosionHit(const CEntity* inflictor, const CEntity* victim, const u32 weaponHash, const float damage);

	// PURPOSE:  Register stats for explosion hits on any object
	static void  RegisterExplosionHitAnything(const CEntity* inflictor, const u32 weaponHash);

	// PURPOSE:  Register that a tire has been popped by bullet.
	static void  RegisterTyresPoppedByGunshot(u16 iRandomSeed, const u8 iWheel, const int iNumWheels);

	// PURPOSE:  Register a kill in the game.
	static void  RegisterPedKill(const CEntity* inflictor, const CPed* victim, const u32 weaponHash, bool headShot, const int weaponDamageComponent, const bool bWithMeleeWeapon, const fwFlags32& flags);

	// PURPOSE:  Register vehicles destroyed by the player.
	static void  RegisterVehicleBlownUpByPlayer(CVehicle* vehicle, const float damage = 0.0f);

	// PURPOSE:  Register Vehicle Damage.
	static void  RegisterVehicleDamage(CVehicle* vehicle, CEntity* inflictor, eDamageType damageType, float damage, const bool wasWrecked);

	// PURPOSE:  Register Player set on fire
	static void  RegisterPlayerSetOnFire();

	// PURPOSE: Called when the parachute is deployed
	static void  RegisterParachuteDeployed(CPed* ped); 

	// PURPOSE: Called when player presses button to open parachute
	static void RegisterPreOpenParachute(CPed* ped);

	// PURPOSE: Called when the parachut is destroyed
	static void RegisterDestroyParachute(const CPed* pPed);

	// PURPOSE: Called when the player start skydiving for challenges
	static void RegisterStartSkydiving(CPed* pPed);

	// PURPOSE: Called when a player is about to be teleported back on the map after falling through it
	static void FallenThroughTheMap(CPed* pPed);

	//PURPOSE:  It is maintained a record of the last 10 peds runned down by the player so that there is no repetion
	static void  UpdateStatsWhenPedRunDown(u16 iRandomSeed, CVehicle* vehicle);

	// PURPOSE:  Set stats on certain player events
	static void  PlayerFireWeapon(const CWeapon* weapon);
	static void  PlayerFiredWeaponToInvenciblePed(const CWeapon* weapon);
	static void  PlayerArrested();
	static void  PreSaveBaseStats(const bool bMultiplayerSave);
	static void  PlayerCompleteMission();
	static void  PlayerFailedMission();

	// PURPOSE:  Check if a vehicle has been driven
	static bool  VehicleHasBeenDriven(const u32 vehicleHash, const bool inMultiplayer);
	static void  SetMaxCountAsFacebookDriven(const u32 count);

	// ---- Access Methods ----------------------------------------------
	static float  GetPercentageProgress(void);
	static float  GetPercentageKillsMadeinFreeAim(void);
	static float  GetFatAndMuscleModifier(eStatModAbilities nAbility);

	static bool  GetFreeFallDueToPlayerDamage() {return sm_FreeFallDueToPlayerDamage;}
	static u32   GetFreeFallStartTimer() {return sm_FreeFallStartTimer;}

	static u32   GetCurrentNearMissNoCrash() {return sm_NearMissNoCrash;}
	static void  ResetCurrentNearMissNoCrash() {sm_NearMissNoCrash=0;}

	static u32   GetCurrentNearMissNoCrashPrecise() {return sm_NearMissNoCrashPrecise;}

	static float GetCurrentRearWheelDistance() 
	{ 
		// Don't want to display wheelie distance during the wheelie challenge if the player is on a BMX/push bike
		if (sm_IsWheelingOnPushBike)
		{
			return 0.f;			
		}
		
		return sm_BikeRearWheelDist; 		
	}
	static float GetCurrentFrontWheelDistance() 
	{ 
		// Don't want to display stoppie distance during the stoppie challenge if the player is on a BMX/push bike
		if (sm_IsDoingAStoppieOnPushBike)
		{
			return 0.f;			
		}

		return sm_BikeFrontWheelDist; 
	}

	static float GetCurrentJumpDistance() { return sm_JumpDistance; }
	static float GetLastJumpHeight() { return sm_LastJumpHeight; }
	static float GetDriveNoCrashDist() { return sm_DriveNoCrashDist; }
	static float GetCurrentSpeed() { return sm_CurrentSpeed; }
	static u32 GetVehicleFlipsAccumulator() { return sm_VehicleFlipsAccumulator; }
	static float GetVehicleSpinsAccumulator() { return sm_VehicleSpinsAccumulator; }
	static float GetCurrentDrivingReverseDistance() { return sm_CurrentDrivingReverseDistance; }
	static float GetCurrentSkydivingDistance() { return sm_SkydiveDistance; }
	static float GetCurrentFootAltitude() { return sm_FootAltitude; }
	static float GetFlyingCounter() { return sm_FlyingCounter; }
	static float GetFlyingDistance() { return sm_FlyingDistance; }
	static bool GetFlyingAltitude(float& value);
	static float GetPlaneBarrelRollCounter() { return sm_PlaneBarrelRollCounter; }
	static float GetVehicleBailDistance() { return sm_VehicleBailDistance; }
	static float GetFreeFallAltitude() { return sm_FreeFallAltitude; }
	static float GetLastPlayersVehiclesDamages() { return sm_PlayersVehiclesDamages; }
	static float GetBoatDistanceOnGround() { return sm_BoatDistanceOnGround; }
    static float GetWallridingDistance() { return sm_WallrideDistance; }
	// Used for the command exposed to script, will also check the forbidden areas
	static bool  IsPlayerVehicleAboveOcean();

	static bool  IsAboveForbiddenArea(const Vec2V& position);

	//Returns TRUE when the player has driven all vehicles 
	static bool  GetHasDrivenAllVehicles() {return (sm_maxCountAsFacebookDriven > 0 && sm_currCountAsFacebookDriven >= sm_maxCountAsFacebookDriven);}

	//Called by level design to set that a CHEAT is active
	static void  SetCheatIsActive(const bool active);

	// PURPOSE:  Check if the player is active this frame.
	static bool  IsPlayerActive();

	//Deal with vehicle leaderboard writes.
	static void  CheckWriteVehicleRecords(const bool bforce=false);

	static void	 IncrementCRCNumChecks(bool bCheckIssuedFromLocal);
	static void	 ResetCRCStats();
	static void	 UpdateCRCStats(bool bWasAMismatch);

	static bool PlayerCharIsValidAndPlayingFreemode();



	static void StartRecordingStat(RecordStat stat, RecordStatPolicy policy);
	static void StopRecordingStat();
	static bool IsRecordingStat();
	static bool GetRecordedStatValue(float& value);
	static void ResetRecordingStats();

    static void StartTrackingStunts();
    static void StopTrackingStunts();

	static void StartBailVehicle(CPed* playerPed);

	static bool ShouldCountKills();
	static bool CountKillsOnlyInInstancedContent();

private:

	//Load stats metadata file.
	static bool LoadMetadataFile();

	//Deal with vehicle records.
	static void  StartNewVehicleRecords(const u32 hashKey);
	static bool  CanUpdateVehicleRecord(const u32 hashKey);
	static u32   FindVehicleRecord(const u32 hashKey);

	//Update peds types killed.
	inline static void  UpdateStatPedsKilledOfThisType(ePedType pedType) { if(pedType<sm_PedsKilledOfThisType.GetCount()) sm_PedsKilledOfThisType[pedType]++; }

	//Parachute altitude.
	static void  CheckParachuteResults(CPed* playerPed);
	static void  CheckFreeFallResults(CPed* playerPed);

	//Parachute vehicle altitude.
	static void  StartVehicleGroundProbe();
	static void  CheckVehicleGroundProbeResults();

	//Make Ground Probes.
	static bool  CheckGroundProbeResults(float& altitude, Vector3& groundProbePosStart);
	static bool  StartGroundProbe(Vector3& groundProbePosStart);
	//Helper for the flying challenges
	static bool  IsAboveOcean(const Vector3& position);

	// PURPOSE:  Register Ped kill by the local Player.
	static void  RegisterKillByLocalPlayer(const CPed* killer, const CPed* victim, const u32 weaponHash, const bool headShot, const fwFlags32& flags);
	// PURPOSE:  Update stats when player is killed.
	static void  RegisterPlayerKilled(const CPed* killer, const CPed* killed, const u32 weaponHash, const int weaponDamageComponent, const bool withMeleeWeapon);

	// PURPOSE:  Track Vehicle models driven.
	static void  SetVehicleHasBeenDriven(const u32 vehicleHash, const bool countAsFacebookDriven);

	//PURPOSE:  Update stats - TIME_IN_COVER, ENTERED_COVER_AND_SHOT, ENTERED_COVER, CROUCHED_AND_SHOT and CROUCHED.
	static void  UpdatePlayerInCoverCrouchStats();
	//PURPOSE:  Update stats when player kills someone.
	static void  UpdateWeaponKill(const CPed* killer, const u32 weaponHash, const bool bHeadShot, const bool killedPlayer, const fwFlags32& flags, const CPed* victim);

	// PURPOSE:  Update stats when player is on a vehicle.
	static void  WorkOutPlayerVehicleStats(CPed* playerPed, CVehicle* playerVehicle);
	static void  UpdateVehicleStats(CVehicle* playerVehicle);
	static void  UpdateVehicleWheelie(CVehicle* playerVehicle);
    // Flips are on the pitch axis
    static void  UpdateVehicleFlips(CVehicle* playerVehicle);
    static void  UpdateVehicleFlipsStunt(CVehicle* playerVehicle);
    // Spins are on the yaw axis
    static void  UpdateVehicleSpins(CVehicle* playerVehicle);
    // Rotations are on the roll axis
    static void  UpdateVehicleRolls(CVehicle* playerVehicle);
	static void  UpdateVehicleZeroWheel(CVehicle* playerVehicle);
	static void  UpdateVehicleReverseDriving(CVehicle* playerVehicle);
    static void  UpdateVehicleHydraulics(CVehicle* playerVehicle);
    static void  UpdateVehicleWallride(CVehicle* playerVehicle);
	static void  UpdateAverageSpeed();
	static void  EndVehicleStats();
	static void  ClearPlayerVehicleStats();
	static void  UpdateFlyingVehicle(CVehicle* playerVehicle);
	static void  UpdatePlane(CVehicle* playerVehicle);
	static void   UpdateHeli(CVehicle* playerVehicle);
	static void  UpdateBoat(CVehicle* playerVehicle);
	static void  CheckCrashDamageAccumulator( const bool force = false );

	static void ClearPlayingTimeStats( );

	//Return TRUE if the player is inside a TRAIN.
	static bool  IsInsideTrain();
	static bool  IsOnTrain();
	static bool  IsOnBoat();
	static bool  IsOnCar();
	static void  ProcessOnFootVehicleChecks(CPed* playerPed);

	static void UpdateTimersInStartMenu();

	static bool IsFirstPersonShooterCamera(const bool vehicleCheck = true);

	static void UpdateRecordedStat(RecordStat stat, float value);

	static void ClearNearMissArray();
	static void AddNearMissTimeToArray(u32 nearMissTime);
#if __ASSERT
public:
	static void  MissingWeaponStatName(const u32 hashType, StatId& stat, const u32 weaponHash, const u32 damageType, const char* weaponname);
#endif // __ASSER
};

#endif

// eof

