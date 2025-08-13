//
// StatsMgr.cpp
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

// --- Include Files ------------------------------------------------------------

// C headers

// Rage headers
#include "system/param.h"
#include "file/asset.h"
#include "net/nethardware.h"
#include "grcore/device.h"

// Framework headers
#include "fwnet/netleaderboardmgr.h"
#include "fwnet/netscgameconfigparser.h"
#include "fwnet/netleaderboardwrite.h"
#include "fwnet/netleaderboardcommon.h"

// Stats Headers
#include "StatsMgr.h"
#include "Stats/StatsUtils.h"
#include "Stats/StatsDataMgr.h"
#include "Stats/StatsInterface.h"
#include "Stats/stats_channel.h"
#include "Stats/MoneyInterface.h"
//Parser headers
#include "Stats/StatsMgr_parser.h"

// Game headers
#include "Core/Game.h"
#include "game/user.h"
#include "ModelInfo/PedModelInfo.h"
#include "Modelinfo/VehicleModelInfo.h"
#include "PedGroup/PedGroup.h"
#include "Peds/ped.h"
#include "Peds/PlayerInfo.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PopZones.h"
#include "Vehicles/Vehicle.h"
#include "Vehicles/Bike.h"
#include "Vehicles/Boat.h"
#include "Vehicles/Train.h"
#include "Vehicles/Planes.h"
#include "Vehicles/Submarine.h"
#include "Vehicles/Metadata/VehicleLayoutInfo.h"
#include "Weapons/Weapon.h"
#include "Scene/Entity.h"
#include "Scene/World/GameWorld.h"
#include "scene/world/GameWorldHeightMap.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "Camera/CamInterface.h"
#include "Camera/cinematic/CinematicDirector.h"
#include "camera/base/BaseCamera.h"
#include "camera/cinematic/camera/mounted/CinematicMountedCamera.h"
#include "System/control.h"
#include "Event/EventDamage.h"
#include "Control/Gamelogic.h"
#include "task/System/AsyncProbeHelper.h"
#include "task/Default/TaskPlayer.h"
#include "physics/WorldProbe/shapetestresults.h"
#include "script/script_cars_and_peds.h"
#include "script/script.h"
#include "Network/Objects/Entities/NetObjVehicle.h"
#include "network/live/NetworkTelemetry.h"
#include "Network/Sessions/NetworkSession.h"
#include "network/Stats/NetworkLeaderboardSessionMgr.h"
#include "event/EventDamage.h"
#include "SaveLoad/GenericGameStorage.h"

#include "fwsys/fileExts.h"
#include "audio/ambience/ambientaudioentity.h"
#include "audio/ambience/water/audshorelineOcean.h"

#include "script/script_hud.h"

FRONTEND_STATS_OPTIMISATIONS()
SAVEGAME_OPTIMISATIONS()

// --- Defines ------------------------------------------------------------------

#undef OpenFile

RAGE_DEFINE_CHANNEL(stat, DIAG_SEVERITY_DEBUG3, DIAG_SEVERITY_DEBUG1, DIAG_SEVERITY_ASSERT)

// --- Constants/Static arrays --------------------------------------------------

const float CStatsVehicleUsage::VEHICLE_MIN_DRIVE_DIST = 0.5f;

//Minimum speed to consider movement
static const float MIN_SPEED_TO_CONSIDER_MOVEMENT = 0.1f;
static const float MIN_PLANE_SPEED_TO_CONSIDER_MOVEMENT = 1.0f;
static const float MIN_SPEED_TO_CONSIDER_JUMP = 2.0f;

// Flip up threshold
static const float VEHICLE_FLIP_UP_THRESHOLD = 0.60f;
//Up side down threshold
static const float VEHICLE_UPSIDEDOWN_UPZ_THRESHOLD = -0.90f;

//Vertical threshold
static const float VEHICLE_FRONT_THRESHOLD = 0.9f;

//How long to let less than 2 wheels count as 2.
static const u32 CAR_TWOWHEEL_BUFFERLIMIT  = 500;
static const u32 CAR_TWOWHEEL_MINBEST      = 1000; //Need to be at least this long to bother recording it.
//How long to let NO wheels count as 1
static const u32 BIKE_ONEWHEEL_BUFFERLIMIT = 500;
static const u32 BIKE_REARWHEEL_MINBEST	   = 3000; //Need to be at least this long to bother recording it
static const u32 BIKE_FRONTWHEEL_MINBEST   = 1000; //Need to be at least this long to bother recording it
//How long to let NO wheels count as 1
static const u32   ZEROWHEEL_MINBEST       = 500;  //Need to be at least this long to bother recording it
static const u32   ZEROWHEEL_BUFFERLIMIT   = 1000;
static const float ZEROWHEEL_MIN_DIST      = 0.5f; //Need to be at least this long to bother recording it
static const u32   ZEROWHEEL_MIN_WHEELS    = 2;    //Need to be at least this Minimum number of contact wheels when landing

//Time between ground probes.
static const u32  TIME_BETWEEN_GROUND_PROBES = 3000;

//update rate for playing time
const float  UPDATE_RATE_PLAYING_TIME = 10000.0f;

static const u32 STAT_NUM_CHECKS_DONE = ATSTRINGHASH("NUM_CHECKS_DONE", 0xFBDF6351);
static const u32 STAT_NUM_CHECKS_MISM_CURR = ATSTRINGHASH("NUM_CHECKS_MISM_CURR", 0x8D72E8CA);
static const u32 STAT_NUM_CHECKS_MISM_PERC = ATSTRINGHASH("NUM_CHECKS_MISM_PERC", 0x20D62AC7);
static const u32 STAT_NUM_CHECKS_ISSUED_CURR = ATSTRINGHASH("NUM_CHECKS_ISSUED_CURR", 0x8F1AAA90);
static const u32 STAT_NUM_CHECKS_RCVD_CURR = ATSTRINGHASH("NUM_CHECKS_RCVD_CURR", 0xF134773E);
static const u32 STAT_NUM_CHECKS_ISSUED = ATSTRINGHASH("NUM_CHECKS_ISSUED", 0x9E0CFC5);
static const u32 STAT_NUM_CHECKS_RCVD = ATSTRINGHASH("NUM_CHECKS_RCVD", 0x14BA8EBF);
// --- Static Class Members -----------------------------------------------------

PARAM(facebookvehiclesdriven, "[stat_savemgr] Setup the number of vehicles driven.");

sStatsMetadataTuning CStatsMgr::sm_TuneMetadata;

//Stats metadata manager
CStatsDataMgr   CStatsMgr::sm_StatsData;

//Used to transitions of valid multiplayer characters
static bool s_isInSP = true;

//Track vehicle analytics that is flushed every sm_lbFlushDefaultInterval or when its full.
atFixedArray <CStatsVehicleUsage, CStatsMgr::NUM_VEHICLE_RECORDS> CStatsMgr::sm_vehicleRecords;
u32  CStatsMgr::sm_curVehIdx = CStatsMgr::INVALID_RECORD;

//Track Peds run down
atFixedArray < u16, CStatsMgr::NUM_PEDS_TO_TRACK >   CStatsMgr::sm_PedsRunDownRecords;

//Track diferent Ped types killed 
atFixedArray < int, PEDTYPE_LAST_PEDTYPE >    CStatsMgr::sm_PedsKilledOfThisType;

//Track tyres popped by Gun shot
atArray< bool >   CStatsMgr::sm_WheelStatus;
u16 	          CStatsMgr::sm_VehicleRandomSeed = 0;

float CStatsMgr::minInvertFlightDot = -0.3f;

//Track Session run time
float   CStatsMgr::sm_SessionRunningTime = 0.0f;
float   CStatsMgr::sm_PlayingTime = 0.0f;
float   CStatsMgr::sm_StealthTime = 0.0f;
float   CStatsMgr::sm_FirstPersonTime = 0.0f;
float   CStatsMgr::sm_3rdPersonTime = 0.0f;

//interval between accepting a stats that depend on dead check counters
static const unsigned DEAD_CHECK_INTERVAL = 3*1000;
bool  CStatsMgr::sm_VehicleDeadCheck        = false;
float CStatsMgr::sm_VehicleDeadCheckCounter = 0.0f;

//Track Longest Drive Without a crash
float CStatsMgr::sm_DriveNoCrashTime     = 0.0f;
float CStatsMgr::sm_DriveNoCrashDist     = 0.0f;

//Track most near misses without crashing
u32 CStatsMgr::sm_NearMissNoCrash     = 0;
float CStatsMgr::sm_JumpDistance      = 0.0f;
float CStatsMgr::sm_LastJumpHeight    = 0.0f;
float CStatsMgr::sm_CurrentSpeed	  = 0.0f;

// Track close encounters with vehicles(no crash)
u32 CStatsMgr::sm_NearMissNoCrashPrecise	= 0;
u32 CStatsMgr::sm_NearMissCounterArray[MAXIMUM_NEAR_MISS_COUNTER];

//Track cover and cower stats
bool   CStatsMgr::sm_bWasInCover        = false;
bool   CStatsMgr::sm_bShotFiredInCover  = false;
bool   CStatsMgr::sm_bWasCrouching      = false;
bool   CStatsMgr::sm_bShotFiredInCrouch = false;

//Track player in Vehicle
bool    CStatsMgr::sm_WasInVehicle      = false;

//Track ped in air stats
bool    CStatsMgr::sm_WasInAir          = false;
Vector3 CStatsMgr::sm_FreeFallStartPos  = Vector3(VEC3_ZERO);
bool    CStatsMgr::sm_FreeFallDueToPlayerDamage = false;
u32     CStatsMgr::sm_FreeFallStartTimer = false;
Vector3 CStatsMgr::sm_ChallengeSkydiveStartPos = Vector3(VEC3_ZERO);

//Track Last vehicle stolen index
u16  CStatsMgr::sm_LastVehicleStolenSeed = 0;
u16  CStatsMgr::sm_LastVehicleStolenForChallenge = 0;

//Track killing spree
static const unsigned KILLSPREE_INTERVAL    = 10*1000;   //interval between accepting a kill as spree
u32    CStatsMgr::sm_KillingSpreeTime       = 0;
u32    CStatsMgr::sm_KillingSpreeTotalTime  = 0;
u32    CStatsMgr::sm_KillingSpreeCounter    = 0;

//Track vehicles destroyed in spree
static const unsigned DESTROYED_VEHICLES_SPREE_INTERVAL = 10*1000;   //interval between accepting a destroyed vehicle as spree
u32    CStatsMgr::sm_VehicleDestroyedSpreeTime          = 0;
u32    CStatsMgr::sm_VehicleDestroyedSpreeTotalTime     = 0;
u32    CStatsMgr::sm_VehicleDestroyedSpreeCounter       = 0;

//Track vehicles damage
static const unsigned CRASHED_VEHICLE_INTERVAL      = 3000;   //interval between accepting a crash of the same vehicle as a new crash
u32   CStatsMgr::sm_CrashedVehicleKeyHash           = 0;
u32   CStatsMgr::sm_CrashedVehicleTimer             = 0;
float CStatsMgr::sm_CrashedVehicleDamageAccumulator = 0.0f;

//Track vehicle models driven
atArray < u32 > CStatsMgr::sm_SpVehicleDriven;
atArray < u32 > CStatsMgr::sm_MpVehicleDriven;
u32  CStatsMgr::sm_currCountAsFacebookDriven = 0;
u32  CStatsMgr::sm_maxCountAsFacebookDriven  = 0;

//Work out player vehicle stats
Vector3   CStatsMgr::sm_VehiclePreviousPosition;
u32       CStatsMgr::sm_CarTwoWheelCounter      = 0;
float     CStatsMgr::sm_CarTwoWheelDist         = 0.0f;
float     CStatsMgr::sm_CarLess3WheelCounter    = 0.0f;
u32       CStatsMgr::sm_BikeRearWheelCounter    = 0;
float     CStatsMgr::sm_BikeRearWheelDist       = 0.0f;
bool      CStatsMgr::sm_IsWheelingOnPushBike    = false;
bool      CStatsMgr::sm_IsDoingAStoppieOnPushBike    = false;
u32       CStatsMgr::sm_BikeFrontWheelCounter   = 0;
float     CStatsMgr::sm_BikeFrontWheelDist      = 0.0f;
u32       CStatsMgr::sm_TempBufferCounter       = 0;
u32       CStatsMgr::sm_BestCarTwoWheelsTimeMs  = 0;
float     CStatsMgr::sm_BestCarTwoWheelsDistM   = 0.0f;
u32       CStatsMgr::sm_BestBikeWheelieTimeMs   = 0;
float     CStatsMgr::sm_BestBikeWheelieDistM    = 0.0f;
u32       CStatsMgr::sm_BestBikeStoppieTimeMs   = 0;
float     CStatsMgr::sm_BestBikeStoppieDistM    = 0.0f;
//Track vehicle air jumps
u32       CStatsMgr::sm_ZeroWheelCounter       = 0;
Vector3   CStatsMgr::sm_ZeroWheelPosStart      = Vector3(VEC3_ZERO);
Vector3   CStatsMgr::sm_ZeroWheelPosEnd        = Vector3(VEC3_ZERO);
Vector3   CStatsMgr::sm_ZeroWheelPosHeight     = Vector3(VEC3_ZERO);
u32       CStatsMgr::sm_ZeroWheelBufferCounter = 0;

//Track the parachute jump distance.
Vector3  CStatsMgr::sm_ParachuteStartPos  = Vector3(VEC3_ZERO);
Vector3  CStatsMgr::sm_ParachuteDeployPos = Vector3(VEC3_ZERO);
u32      CStatsMgr::sm_ParachuteStartTime = 0;
bool	 CStatsMgr::sm_HasStartedParachuteDeploy = false;
bool	 CStatsMgr::sm_hasBailedFromParachuting = false;

//Track Longest time spent driving in cinematic camera
float   CStatsMgr::sm_DriveInCinematic = 0.0f;

//Use an asynchronous probe helper to manage the probes used to determine the altitude of stats.
static CAsyncProbeHelper s_GroundProbeHelper;
CAsyncProbeHelper&  CStatsMgr::sm_GroundProbeHelper(s_GroundProbeHelper);
u32                 CStatsMgr::sm_TimeSinceLastGroundProbe = 0;

//Used to track number of vehicle flips.
bool  CStatsMgr::sm_VehicleFlipsCounter = false;
u32   CStatsMgr::sm_VehicleFlipsAccumulator = 0;
float CStatsMgr::sm_VehicleFlipsHeadingLast = 0.0f;

//Used to track number of vehicle spins.
float  CStatsMgr::sm_VehicleSpinsAccumulator = 0.0f;
float  CStatsMgr::sm_VehicleSpinsHeadingLast = 0.0f;

//Used to track number of vehicle rotations.
float  CStatsMgr::sm_VehicleRollsAccumulator = 0.0f;
float  CStatsMgr::sm_VehicleRollsHeadingLast = 0.0f;

//Used to track good plane landings
u32  CStatsMgr::sm_InAirTimer = 0;
int  CStatsMgr::sm_LandingTimer = 0;

//TRUE when the player has cheated recently
static const u32 CHEAT_REST_TIME      = 2*1000; //After 2 seconds the cheat is reset
bool CStatsMgr::sm_cheatIsActive      = false;
u32  CStatsMgr::sm_cheatIsActiveTimer = 0;

float CStatsMgr::sm_hydraulicJumpVehicleAltitude = 0.0f;

//TRUE when we want to check for leaderboard flush after game load
static bool s_writeLeaderboardOnboot = false;

// Stat recording for the challenges
CStatsMgr::RecordStat CStatsMgr::m_recordedStat = CStatsMgr::REC_STAT_NONE;
float CStatsMgr::m_recordedStatValue = (float)(0x7F800001);   // Nan
CStatsMgr::RecordStatPolicy CStatsMgr::m_statRecordingPolicy = CStatsMgr::RSP_NONE;

Vector3 CStatsMgr::sm_ChallengeZeroWheelPosStart = Vector3(VEC3_ZERO);
Vector3 CStatsMgr::sm_ReverseDrivingPosCurrent = Vector3(VEC3_ZERO);
float   CStatsMgr::sm_CurrentDrivingReverseDistance = 0.0f;
bool    CStatsMgr::sm_invalidJump = false;

float CStatsMgr::sm_SkydiveDistance = 0.0f;
float CStatsMgr::sm_FreeFallAltitude = 0.0f;

// On foot altitude
Vector3 CStatsMgr::sm_OnFootPosStart = Vector3(VEC3_ZERO);
float CStatsMgr::sm_FootAltitude = 0.0f;

// Plane challenges
float CStatsMgr::sm_FlyingAltitude = 0.0f; 
bool CStatsMgr::sm_ValidFlyingAltitude = false;
float CStatsMgr::sm_FlyingCounter = 0.0f;
float CStatsMgr::sm_FlyingDistance = 0.0f;
float CStatsMgr::sm_PlaneBarrelRollCounter = 0.0f;
float CStatsMgr::sm_PlaneRollAccumulator = 0.0f;
float CStatsMgr::sm_PlaneRollLast = 0.0f;

bool CStatsMgr::sm_drivingFlyingVehicle = false;
bool CStatsMgr::sm_drivingFlyingVehicleForProbe = false;

Vector3 CStatsMgr::sm_FlyingPlanePosHeight = Vector3(VEC3_ZERO);
Vector3 CStatsMgr::sm_LastFlyingPlanePosition = Vector3(VEC3_ZERO);


// Vehicle bail
Vector3 CStatsMgr::sm_StartVehicleBail = Vector3(VEC3_ZERO);
Vector3 CStatsMgr::sm_LastVehicleBailPosition = Vector3(VEC3_ZERO);
float CStatsMgr::sm_VehicleBailDistance = 0.0f;

float CStatsMgr::sm_PlayersVehiclesDamages = 0.0f;

// Boat on ground
Vector3 CStatsMgr::sm_LastBoatPositionInWater = Vector3(VEC3_ZERO);
Vector3 CStatsMgr::sm_BoatPositionOnGround = Vector3(VEC3_ZERO);
float CStatsMgr::sm_BoatDistanceOnGround = 0.0f;

bool CStatsMgr::sm_wasInWaterPreviously = false;
bool CStatsMgr::sm_outOfWater = false;
float CStatsMgr::sm_inAirAfterGroundTimer = 0.0f;

bool CStatsMgr::sm_HadAttachedParentVehicle = false;

float CStatsMgr::sm_FootOnVehicleTimer=0.0f;

u32 CStatsMgr::sm_TimerAfterOnAnotherVehicle = 0;
bool CStatsMgr::sm_IsOnAnotherVehicle = false;
bool CStatsMgr::sm_WasOnAnotherVehicleWhenJumped = false;

u32 CStatsMgr::sm_MostRecentCollisionTimeWithPlaneInAir = 0;

Vector3 CStatsMgr::sm_LastWallridePosition = Vector3(VEC3_ZERO);
float CStatsMgr::sm_WallrideDistance = 0.0f;
float CStatsMgr::sm_WallrideBufferTimer = 0.0f;
const float CStatsMgr::WALLRIDE_BUFFERTIMER_VALUE = 0.2f; // Time buffer when we consider that you're wallriding even if there's smalls bumps


static bool s_isTrackingStunts = false;

// Timer started after landing a flip. We want to wait a few ms before sending the event in case we crash right after landing.
static float s_delayedFlipCount = 0.0f;
static bool s_landedFlip = false;

// B*2850786 - Types of stunts tracked for Stunt DLC
enum StuntType
{
    ST_FRONTFLIP,
    ST_BACKFLIP,
    ST_SPIN,
    ST_WHEELIE,
    ST_STOPPIE,
    ST_BOWLING_PIN,
    ST_FOOTBALL,
    ST_ROLL
};

void SendStuntEvent(StuntType type, float stuntValue)
{
    if(s_isTrackingStunts)
    {
        statDebugf3("Send stunt event : type = %d, value = %f", (int) type, stuntValue);
        GetEventScriptNetworkGroup()->Add(CEventNetworkStuntPerformed((int)type, stuntValue));
    }
}

// --- CStatsMgr ----------------------------------------------------------------

bool  IS_LOCAL_PLAYER_DRIVER_OR_LAST_DRIVER(CVehicle* vehicle)
{
	bool driverIsLocalPlayer = false;

	if (vehicle)
	{
		if (!vehicle->GetDriver())
		{
			if (vehicle->GetSeatManager())
			{
				CPed* lastDriver = vehicle->GetSeatManager()->GetLastPedInSeat(0);
				driverIsLocalPlayer = lastDriver && lastDriver->IsLocalPlayer();

				//Vehicle is being toad
				if (driverIsLocalPlayer)
				{
					CVehicle* pAttachParentVehicle = vehicle->GetAttachParentVehicle();
					if (pAttachParentVehicle && pAttachParentVehicle->GetDriver() && pAttachParentVehicle->GetDriver()->IsLocalPlayer())
					{
						driverIsLocalPlayer = false;
					}
				}
			}
		}
		else
		{
			driverIsLocalPlayer = vehicle->GetDriver()->IsLocalPlayer();
		}
	}

	return driverIsLocalPlayer;
}

void
CStatsMgr::WriteAfterGameLoad()
{
	s_writeLeaderboardOnboot = true;
}

void 
CStatsMgr::Init(unsigned initMode)
{
	USE_MEMBUCKET(MEMBUCKET_SCRIPT);

	if (initMode == INIT_CORE)
	{
		if (!CashPackInfo::IsInstantiated())
		{
			CashPackInfo::Instantiate();
		}
	}
    else if (initMode == INIT_SESSION)
    {
		LoadMetadataFile();

		sm_VehiclePreviousPosition.Zero();

		sm_vehicleRecords.Reset();
		sm_curVehIdx = CStatsMgr::INVALID_RECORD;

	    sm_PedsKilledOfThisType.clear();
	    sm_PedsKilledOfThisType.Reset();

	    sm_PedsKilledOfThisType.Resize(PEDTYPE_LAST_PEDTYPE);
	    for (s32 i=0; i<sm_PedsKilledOfThisType.GetCount(); i++) 
	    {
		    sm_PedsKilledOfThisType[i] = 0;
	    }

		sm_PedsRunDownRecords.clear();
		sm_PedsRunDownRecords.Reset();
		sm_PedsRunDownRecords.Resize(NUM_PEDS_TO_TRACK);
		for (s32 i=0; i<sm_PedsRunDownRecords.GetCount(); i++)
		{
			sm_PedsRunDownRecords[i] = 0;
		}

		sm_SpVehicleDriven.clear();
		sm_SpVehicleDriven.Reset();
		sm_MpVehicleDriven.clear();
		sm_MpVehicleDriven.Reset();
		sm_currCountAsFacebookDriven = 0;

	    sm_WheelStatus.clear();
	    sm_WheelStatus.Reset();

		sm_SessionRunningTime = 0.0f;
		sm_PlayingTime = 0.0f;
		sm_StealthTime = 0.0f;
		sm_FirstPersonTime = 0.0f;
		sm_3rdPersonTime = 0.0f;

		sm_WasInVehicle = false;

		sm_ParachuteStartTime = 0;
		sm_ParachuteStartPos.Zero();
		sm_ParachuteDeployPos.Zero();
		sm_HasStartedParachuteDeploy = false;

		sm_WasInAir = false;
		sm_FreeFallStartPos.Zero();
		sm_ChallengeSkydiveStartPos.Zero();
		sm_FreeFallDueToPlayerDamage = false;
		sm_FreeFallStartTimer = 0;
		sm_OnFootPosStart.Zero();
		sm_FootAltitude = 0.0f;
		sm_FlyingCounter = 0.0f;
		sm_FlyingDistance = 0.0f;

		sm_ZeroWheelPosHeight.Zero();
		sm_GroundProbeHelper.ResetProbe();
		ClearPlayerVehicleStats();

		sm_VehicleDeadCheckCounter = 0.0f;
		sm_VehicleDeadCheck        = false;

		SetCheatIsActive( false );

		sm_TimeSinceLastGroundProbe = 0;

		CStatsUtils::ResetStatsTracking();
		m_recordedStat = REC_STAT_NONE;
		m_statRecordingPolicy = RSP_NONE;
		ResetRecordingStats();
		sm_MostRecentCollisionTimeWithPlaneInAir = 0;

        sm_LastWallridePosition.Zero();
        sm_WallrideDistance = 0.0f;
	}

    sm_StatsData.Init(initMode);
}

bool 
CStatsMgr::LoadMetadataFile()
{
	bool done = false;

	char metadataFilename[RAGE_MAX_PATH];
	metadataFilename[0] = '\0';

	//Try to load "statsmgr.pso" file.
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::STATS_METADATA_PSO_FILE);
	if(pData && DATAFILEMGR.IsValid(pData))
	{
		formatf(metadataFilename, RAGE_MAX_PATH, "%s.%s", pData->m_filename, META_FILE_EXT);

		psoFile * pPsoFile = psoLoadFile(metadataFilename, PSOLOAD_PREP_FOR_PARSER_LOADING, atFixedBitSet32().Set(psoFile::REQUIRE_CHECKSUM));
		if(pPsoFile)
		{
			done = psoLoadObject(*pPsoFile, sm_TuneMetadata);
			delete pPsoFile;
		}
	}

	statAssertf(done, "Error loading %s", metadataFilename);

	return done;
}

void  
CStatsMgr::OpenNetwork()
{
	for (int i=0; i<sm_vehicleRecords.GetCount(); i++)
		sm_vehicleRecords[i].Clear(true);
	sm_vehicleRecords.Resize(0);
	sm_curVehIdx = CStatsMgr::INVALID_RECORD;
}

void  
CStatsMgr::CloseNetwork()
{
	for (int i=0; i<sm_vehicleRecords.GetCount(); i++)
		sm_vehicleRecords[i].Clear(true);
	sm_vehicleRecords.Resize(0);
	sm_curVehIdx = CStatsMgr::INVALID_RECORD;

	// Stop the recording
	m_recordedStat = REC_STAT_NONE;
	m_statRecordingPolicy = RSP_NONE;
}

void 
CStatsMgr::Shutdown(unsigned shutdownMode)
{
	if (shutdownMode == SHUTDOWN_CORE)
	{
		CashPackInfo::GetInstance().Shutdown();
	}
    else if (shutdownMode == SHUTDOWN_SESSION)
    {
		sm_vehicleRecords.Reset();
		sm_curVehIdx = CStatsMgr::INVALID_RECORD;

	    sm_PedsKilledOfThisType.clear();
	    sm_PedsKilledOfThisType.Reset();

		sm_WheelStatus.clear();
	    sm_WheelStatus.Reset();

		sm_PedsRunDownRecords.clear();
		sm_PedsRunDownRecords.Reset();

		sm_SpVehicleDriven.clear();
		sm_SpVehicleDriven.Reset();
		sm_MpVehicleDriven.clear();
		sm_MpVehicleDriven.Reset();
		sm_currCountAsFacebookDriven = 0;

		sm_PlayingTime = 0.0f;
		sm_StealthTime = 0.0f;
		sm_FirstPersonTime = 0.0f;
		sm_3rdPersonTime = 0.0f;
		sm_SessionRunningTime = 0.0f;
		sm_NearMissNoCrash    = 0;
		sm_DriveInCinematic   = 0.0f;
		sm_JumpDistance=0.0f;
		sm_OnFootPosStart.Zero();
		sm_FootAltitude = 0.0f;
		sm_FlyingCounter=0.0f;
		sm_FlyingDistance = 0.0f;
		sm_HadAttachedParentVehicle=false;
		sm_NearMissNoCrashPrecise = 0;
		ClearNearMissArray();

		sm_ZeroWheelPosHeight.Zero();
		sm_GroundProbeHelper.ResetProbe();
		ClearPlayerVehicleStats();

		sm_VehicleDeadCheckCounter = 0.0f;
		sm_VehicleDeadCheck        = false;

		sm_CrashedVehicleTimer             = 0;
		sm_CrashedVehicleKeyHash           = 0;
		sm_CrashedVehicleDamageAccumulator = 0.0f;

		SetCheatIsActive( false );

		CStatsUtils::ResetStatsTracking();
		sm_CurrentSpeed = 0.0f;

        sm_LastWallridePosition.Zero();
        sm_WallrideDistance = 0.0f;
	}

	sm_StatsData.Shutdown(shutdownMode);
}

u32 
CStatsMgr::FindVehicleRecord( const u32 hashKey )
{
	u32 idx = INVALID_RECORD;

	const bool isOnline = NetworkInterface::IsGameInProgress();
	u32          charId = StatsInterface::GetStatsModelPrefix();
	static u32 s_charId = StatsInterface::GetStatsModelPrefix();

	for (int i=0; i<sm_vehicleRecords.GetCount() && idx==CStatsMgr::INVALID_RECORD; i++)
	{
		if (sm_vehicleRecords[i].m_VehicleId == hashKey)
		{
			if (sm_vehicleRecords[i].m_CharacterId == charId)
			{
				if (sm_vehicleRecords[i].m_Online == isOnline)
				{
					idx = (u32)i;
					break;
				}
				else
				{
					statDebugf3("FindVehicleRecord - Vehicle id='%d' found but online flag='%s' doesnt match.", hashKey, sm_vehicleRecords[i].m_Online ? "true":"false");
				}
			}
			else
			{
				statDebugf3("FindVehicleRecord - Vehicle id='%d' found but character id='%d : %d' doesnt match.", hashKey, sm_vehicleRecords[i].m_CharacterId, charId);
			}
		}
	}

	//Lookup using previous character ID.
	if (idx == INVALID_RECORD && s_charId != charId)
	{
		charId   = s_charId;
		s_charId = StatsInterface::GetStatsModelPrefix();

		for (int i=0; i<sm_vehicleRecords.GetCount() && idx==CStatsMgr::INVALID_RECORD; i++)
		{
			if (sm_vehicleRecords[i].m_VehicleId == hashKey)
			{
				if (sm_vehicleRecords[i].m_CharacterId == charId)
				{
					if (sm_vehicleRecords[i].m_Online == isOnline)
					{
						idx = (u32)i;
						break;
					}
					else
					{
						statDebugf3("FindVehicleRecord - Vehicle id='%d' found but online flag='%s' doesnt match.", hashKey, sm_vehicleRecords[i].m_Online ? "true":"false");
					}
				}
				else
				{
					statDebugf3("FindVehicleRecord - Vehicle id='%d' found but character id='%d : %d' doesnt match.", hashKey, sm_vehicleRecords[i].m_CharacterId, charId);
				}
			}
		}
	}

	return idx;
}

void CStatsVehicleUsage::Clear(const bool clearlastrecords /*= true*/)
{
	if ( clearlastrecords )
	{
		EndVehicleStats( );
		m_Online                 = false;
		m_LastTimeSpentInVehicle = 0;
		m_LastDistDriven         = 0.0f;
		m_HasSetNumDriven        = false;
	}

	statDebugf3("Clear VehicleRecord %u", m_VehicleId);

	m_VehicleId              = 0;
	m_CharacterId            = CHAR_INVALID;
	m_TimeDriven             = 0;
	m_DistDriven             = 0.0f;
	m_NumDriven              = 0; 
	m_NumStolen              = 0; 
	m_NumSpins               = 0;
	m_NumFlips               = 0;
	m_NumPlaneLandings       = 0;
	m_NumWheelies            = 0;
	m_NumAirLaunches         = 0;
	m_NumAirLaunchesOver5s   = 0;
	m_NumAirLaunchesOver5m   = 0;
	m_NumAirLaunchesOver40m  = 0;
	m_NumLargeAccidents      = 0;
	m_TimeSpentInVehicle     = 0;
	m_HighestSpeed           = 0.0f;
	m_LongestWheelieTime     = 0;
	m_LongestWheelieDist     = 0.0f;
	m_HighestJumpDistance    = 0.0f;
	m_NumPedsRundown         = 0;
	m_Location				 = 0;
	m_Owned					 = false;
}

void  
CStatsVehicleUsage::EndVehicleStats( )
{
	const u32 timespentinvehicle = (m_LastTimeSpentInVehicle>0) ? sysTimer::GetSystemMsTime() - m_LastTimeSpentInVehicle : 0;


	//Set metric with distance driven.
	if (IsValid() && m_LastDistDriven>=VEHICLE_MIN_DRIVE_DIST)
	{
		CNetworkTelemetry::PlayerVehicleDistance(m_VehicleId, m_LastDistDriven, timespentinvehicle, CUserDisplay::AreaName.GetName(), CUserDisplay::DistrictName.GetName(), CUserDisplay::StreetName.GetName(), m_Owned);
	}

	if (m_LastTimeSpentInVehicle>0)
	{
		m_TimeSpentInVehicle += timespentinvehicle;
		m_LastTimeSpentInVehicle = 0;
	}

	m_LastDistDriven  = 0.0f;
	m_HasSetNumDriven = false;
}

bool 
CStatsMgr::CanUpdateVehicleRecord(const u32 hashKey)
{
	if (sm_curVehIdx >= INVALID_RECORD)
		return false;

	if (hashKey != sm_vehicleRecords[sm_curVehIdx].m_VehicleId)
		return false;

	if (StatsInterface::GetStatsModelPrefix() != sm_vehicleRecords[sm_curVehIdx].m_CharacterId)
		return false;

	if (!statVerify(sm_vehicleRecords[sm_curVehIdx].IsValid()))
		return false;

	return true;
}

void  
CStatsMgr::StartNewVehicleRecords(const u32 hashKey)
{
	if (sm_vehicleRecords.GetCount() >= CStatsMgr::NUM_VEHICLE_RECORDS)
	{
		statWarningf("Force Flush Vehicle leaderboard data due to array being full.");
		CheckWriteVehicleRecords( true );
	}

	//alredy tracking this vehicle hash key
	if (CanUpdateVehicleRecord( hashKey ))
		return;

	sm_curVehIdx = CStatsMgr::INVALID_RECORD;

	const bool isOnline = NetworkInterface::IsGameInProgress();
	const u32    charId = StatsInterface::GetStatsModelPrefix();

	for (int i=0; i<sm_vehicleRecords.GetCount() && sm_curVehIdx==CStatsMgr::INVALID_RECORD; i++)
	{
		if (sm_vehicleRecords[i].m_VehicleId == hashKey && sm_vehicleRecords[i].m_CharacterId == charId && sm_vehicleRecords[i].m_Online == isOnline)
		{
			sm_curVehIdx = (u32)i;
			break;
		}
	}

	if (sm_curVehIdx == CStatsMgr::INVALID_RECORD && statVerify(sm_vehicleRecords.GetCount()<CStatsMgr::NUM_VEHICLE_RECORDS))
	{
		const int index = sm_vehicleRecords.GetCount();
		sm_vehicleRecords.Insert(index);
		sm_vehicleRecords[index].Clear( true );
		sm_vehicleRecords[index].m_VehicleId   = hashKey;
		sm_vehicleRecords[index].m_Online      = NetworkInterface::IsGameInProgress() ? true : false;
		sm_vehicleRecords[index].m_CharacterId = StatsInterface::GetStatsModelPrefix();
		sm_curVehIdx = (u32)index;
		statDebugf3("StartNewVehicleRecords - %u", hashKey);
	}

	statAssert(CanUpdateVehicleRecord(hashKey) );
}

void
CStatsMgr::CheckWriteVehicleRecords(const bool bforce)
{
	bool shouldStartFlush = (bforce || sm_vehicleRecords.GetCount() >= CStatsMgr::NUM_VEHICLE_RECORDS);

	static StatId STAT_MP_VEHICLE_RECORD_FLUSH = StatId("MP_VEHICLE_RECORD_FLUSH");
	static StatId STAT_SP_VEHICLE_RECORD_FLUSH = StatId("SP_VEHICLE_RECORD_FLUSH");
	u64 timepassed = 0;
	u64 lastFlushTime = 0;

	if (NetworkInterface::IsGameInProgress())
	{
		timepassed    = StatsInterface::GetUInt64Stat(STAT_MP_PLAYING_TIME);
		lastFlushTime = StatsInterface::GetUInt64Stat(STAT_MP_VEHICLE_RECORD_FLUSH);
	}
	else
	{
		timepassed    = StatsInterface::GetUInt64Stat(STAT_PLAYING_TIME);
		lastFlushTime = StatsInterface::GetUInt64Stat(STAT_SP_VEHICLE_RECORD_FLUSH);
	}

	if (!shouldStartFlush)
	{
		const u64 defaultInterval = (u64)StatsInterface::GetVehicleLeaderboardWriteInterval();

		if ((lastFlushTime+defaultInterval) <= timepassed)
		{
			shouldStartFlush = true;

			//Is a UGC activity in progress
			if (CNetworkTelemetry::IsActivityInProgress())
				shouldStartFlush = false;
		}
	}

	if(shouldStartFlush)
	{
		statDebugf3("CheckWriteVehicleRecords - Flush vehicle records");

		if (NetworkInterface::IsGameInProgress())
			StatsInterface::SetStatData(STAT_MP_VEHICLE_RECORD_FLUSH, timepassed, STATUPDATEFLAG_ASSERTONLINESTATS);
		else
			StatsInterface::SetStatData(STAT_SP_VEHICLE_RECORD_FLUSH, timepassed, STATUPDATEFLAG_ASSERTONLINESTATS);

		if (sm_curVehIdx<sm_vehicleRecords.GetCount() && sm_vehicleRecords[sm_curVehIdx].IsValid())
		{
			CStatsVehicleUsage currRecord;
			currRecord = sm_vehicleRecords[sm_curVehIdx];

			for (int i=0; i<sm_vehicleRecords.GetCount(); i++)
				sm_vehicleRecords[i].Clear( true );

			sm_vehicleRecords.Resize(1);
			sm_curVehIdx = 0;
			sm_vehicleRecords[sm_curVehIdx].m_VehicleId   = currRecord.m_VehicleId;
			sm_vehicleRecords[sm_curVehIdx].m_CharacterId = currRecord.m_CharacterId;
			sm_vehicleRecords[sm_curVehIdx].m_Online      = currRecord.m_Online;
			sm_vehicleRecords[sm_curVehIdx].m_Owned		  = currRecord.m_Owned;
			sm_vehicleRecords[sm_curVehIdx].m_Location	  = currRecord.m_Location;
		}
		else
		{
			for (int i=0; i<sm_vehicleRecords.GetCount(); i++)
				sm_vehicleRecords[i].Clear( true );

			sm_curVehIdx = CStatsMgr::INVALID_RECORD;
			sm_vehicleRecords.Resize(0);
		}
	}
}

void CStatsMgr::IncrementCRCNumChecks(bool bCheckIssuedFromLocal)
{
	const u32 STAT_NUM_CHECKS_ISSUED_CURR = ATSTRINGHASH("NUM_CHECKS_ISSUED_CURR", 0x8F1AAA90);
	const u32 STAT_NUM_CHECKS_RCVD_CURR = ATSTRINGHASH("NUM_CHECKS_RCVD_CURR", 0xF134773E);

	if(bCheckIssuedFromLocal)
	{
		StatsInterface::IncrementStat(STAT_NUM_CHECKS_ISSUED_CURR, 1);
	}
	else
	{
		StatsInterface::IncrementStat(STAT_NUM_CHECKS_RCVD_CURR, 1);
	}
}

void CStatsMgr::ResetCRCStats()
{
	// Reset all last window stats
	StatsInterface::SetStatData(STAT_NUM_CHECKS_MISM_PERC, 0.0f);
	StatsInterface::SetStatData(STAT_NUM_CHECKS_ISSUED, 0);
	StatsInterface::SetStatData(STAT_NUM_CHECKS_RCVD, 0);

	// Set current window stats to either 0, or either some magic numbers so this guy can get detected earlier when back from the cheater pool
	int numChecksDoneReset, numChecksMismReset, numChecksIssuedReset, numChecksRcvdReset = 0;

	// By default we reset to get out of the pool as a clean player, but we can&should change that from the cloud
	::Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("NUM_CHECKS_DONE_RESET", 0xb329b3c8), numChecksDoneReset);
	::Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("NUM_CHECKS_MISM_CURR_RESET", 0x748ac144), numChecksMismReset);
	::Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("NUM_CHECKS_ISSUED_CURR_RESET", 0xeea84f34), numChecksIssuedReset);
	::Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("NUM_CHECKS_RCVD_CURR_RESET", 0x458318a2), numChecksRcvdReset);

	StatsInterface::SetStatData(STAT_NUM_CHECKS_DONE, (u32)numChecksDoneReset);
	StatsInterface::SetStatData(STAT_NUM_CHECKS_MISM_CURR, (u32)numChecksMismReset);
	StatsInterface::SetStatData(STAT_NUM_CHECKS_ISSUED_CURR, (u32)numChecksIssuedReset);
	StatsInterface::SetStatData(STAT_NUM_CHECKS_RCVD_CURR, (u32)numChecksRcvdReset);
}

void CStatsMgr::UpdateCRCStats(bool bWasAMismatch)
{
	if( CNetwork::IsNetworkSessionValid() )
	{
		static u64 lastCheckedSessionId = rlSession::INVALID_SESSION_ID;
		static u64 lastReportedSessionId = rlSession::INVALID_SESSION_ID;

		const u64 currentSessionId = CNetwork::GetNetworkSession().GetSnSession().GetSessionId();

		if( bWasAMismatch && lastReportedSessionId != currentSessionId )
		{
			StatsInterface::IncrementStat(STAT_NUM_CHECKS_MISM_CURR, 1);
			lastReportedSessionId = currentSessionId;
		}

		if(lastCheckedSessionId != currentSessionId)
		{
			StatsInterface::IncrementStat(STAT_NUM_CHECKS_DONE, 1);
			lastCheckedSessionId = currentSessionId;

			// Do rolling if we already tracked more than X sessions, so we can detect a player who only switched to hacker mode lately or for short periods
			int NUM_CHECKS_DONE_FOR_ROLLING_STAT = 100;

			::Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("NUM_CHECKS_DONE_FOR_ROLLING_STAT", 0xc1184646), NUM_CHECKS_DONE_FOR_ROLLING_STAT);

			const u32 uNumChecksDone = StatsInterface::GetUInt32Stat(STAT_NUM_CHECKS_DONE);

			if( uNumChecksDone >= NUM_CHECKS_DONE_FOR_ROLLING_STAT )
			{
				// If the tunable decreased drastically since last time we played (checks > 1.25x), forget about this tracking; otherwise we'll receive lots of false positives
				if( uNumChecksDone <= ( NUM_CHECKS_DONE_FOR_ROLLING_STAT + (NUM_CHECKS_DONE_FOR_ROLLING_STAT>>2) ) )
				{
					const float fMismatchPerc = (((float)StatsInterface::GetUInt32Stat(STAT_NUM_CHECKS_MISM_CURR))/(float)NUM_CHECKS_DONE_FOR_ROLLING_STAT)*100.0f;
					StatsInterface::SetStatData(STAT_NUM_CHECKS_MISM_PERC, fMismatchPerc);
				}

				StatsInterface::SetStatData(STAT_NUM_CHECKS_DONE, 0);
				StatsInterface::SetStatData(STAT_NUM_CHECKS_MISM_CURR, 0);

				const u32 uNumChecksIssuedInThisTimeWindow = StatsInterface::GetUInt32Stat(STAT_NUM_CHECKS_ISSUED_CURR);
				StatsInterface::SetStatData(STAT_NUM_CHECKS_ISSUED, uNumChecksIssuedInThisTimeWindow);
				StatsInterface::SetStatData(STAT_NUM_CHECKS_ISSUED_CURR, 0);

				const u32 uNumChecksReceivedInThisTimeWindow = StatsInterface::GetUInt32Stat(STAT_NUM_CHECKS_RCVD_CURR);
				StatsInterface::SetStatData(STAT_NUM_CHECKS_RCVD, uNumChecksReceivedInThisTimeWindow);
				StatsInterface::SetStatData(STAT_NUM_CHECKS_RCVD_CURR, 0);
			}
		}
	}
}

void  
CStatsMgr::SignedOut()
{
	sm_StatsData.SignedOut();
}

void  
CStatsMgr::SignedOffline()
{
	sm_StatsData.SignedOffline();
}

void  
CStatsMgr::SignedIn()
{
	sm_StatsData.SignedIn();
}

#if __BANK
void 
CStatsMgr::InitWidgets()
{
	sm_StatsData.Bank_InitWidgets();
}
#endif

void 
CStatsMgr::DisplayScriptStatUpdateMessage(bool UNUSED_PARAM(IncOrDec), const StatId& keyHash, float UNUSED_PARAM(fValue))
{
	// we dont want to overright a stat message for the standard script stat message....
	if (StatsInterface::GetBooleanStat(STAT_MSG_BEING_DISPLAYED.GetHash()))
	{
		StatsInterface::SetStatData(STAT_MSG_BEING_DISPLAYED.GetHash(), false, STATUPDATEFLAG_ASSERTONLINESTATS);
		return;
	}

	if (CStatsUtils::IsStatCapped(keyHash))
	{
		if (StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_FLOAT) && StatsInterface::GetFloatStat(keyHash) >= STAT_LIMIT)
		{
			return;
		}
		else if (StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_INT) && StatsInterface::GetIntStat(keyHash) >= STAT_LIMIT)
		{
			return;
		}
		else if (StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_UINT8) && StatsInterface::GetUInt8Stat(keyHash) >= STAT_LIMIT)
		{
			return;
		}
		else if (StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_UINT16) && StatsInterface::GetUInt16Stat(keyHash) >= STAT_LIMIT)
		{
			return;
		}
		else if (StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_UINT32) && StatsInterface::GetUInt32Stat(keyHash) >= STAT_LIMIT)
		{
			return;
		}
		else if (StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_UINT64) && StatsInterface::GetUInt64Stat(keyHash) >= STAT_LIMIT)
		{
			return;
		}
		else if (StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_INT64) && StatsInterface::GetInt64Stat(keyHash) >= STAT_LIMIT)
		{
			return;
		}
	}
}


// --- Modify Stats ---------------------------------------------

void  
CStatsMgr::IncrementStat(const StatId& keyHash, const float fAmount, const u32 flags)
{
	if (!CStatsUtils::CanUpdateStats() || !StatsInterface::IsKeyValid(keyHash) || 0.0f >= fAmount)
	{
		return;
	}

	switch (StatsInterface::GetStatType(keyHash))
	{
	case STAT_TYPE_INT:
		{
			int iNewVal = static_cast<int>(fAmount);
			iNewVal = iNewVal + StatsInterface::GetIntStat(keyHash);
			if (CStatsUtils::IsStatCapped(keyHash))
			{
				iNewVal = (iNewVal > STAT_LIMIT) ? (int) STAT_LIMIT : iNewVal;
			}
			StatsInterface::SetStatData(keyHash, &iNewVal, sizeof(int), flags);
		}
		break;
	case STAT_TYPE_INT64:
		{
			s64 iNewVal = static_cast<s64>(fAmount);
			iNewVal = iNewVal + StatsInterface::GetInt64Stat(keyHash);
			if (CStatsUtils::IsStatCapped(keyHash))
			{
				iNewVal = (iNewVal > STAT_LIMIT) ? (int) STAT_LIMIT : iNewVal;
			}
			StatsInterface::SetStatData(keyHash, &iNewVal, sizeof(s64), flags);
		}
		break;
	case STAT_TYPE_FLOAT:
		{
			float fNewVal = fAmount + StatsInterface::GetFloatStat(keyHash);
			if (CStatsUtils::IsStatCapped(keyHash))
			{
				fNewVal = rage::Min(fNewVal, STAT_LIMIT);
			}
			StatsInterface::SetStatData(keyHash, &fNewVal, sizeof(float), flags);
		}
		break;
	case STAT_TYPE_UINT8:
		{
			statAssertf(fAmount >= 0.0f, "Setting a negative value in a unsigned stat - type \"%d\" for stat \"%s\"", StatsInterface::GetStatType(keyHash), StatsInterface::GetKeyName(keyHash));
			u8 iNewVal = static_cast<u8>(fAmount);
			iNewVal = iNewVal + StatsInterface::GetUInt8Stat(keyHash);
			StatsInterface::SetStatData(keyHash, &iNewVal, sizeof(u8), flags);
		}
		break;
	case STAT_TYPE_UINT16:
		{
			statAssertf(fAmount >= 0.0f, "Setting a negative value in a unsigned stat - type \"%d\" for stat \"%s\"", StatsInterface::GetStatType(keyHash), StatsInterface::GetKeyName(keyHash));
			u16 iNewVal = static_cast<u16>(fAmount);
			iNewVal = iNewVal + StatsInterface::GetUInt16Stat(keyHash);
			StatsInterface::SetStatData(keyHash, &iNewVal, sizeof(u16), flags);
		}
		break;
	case STAT_TYPE_UINT32:
		{
			statAssertf(fAmount >= 0.0f, "Setting a negative value in a unsigned stat - type \"%d\" for stat \"%s\"", StatsInterface::GetStatType(keyHash), StatsInterface::GetKeyName(keyHash));
			u32 iNewVal = static_cast<u32>(fAmount);
			iNewVal = iNewVal + StatsInterface::GetUInt32Stat(keyHash);
			StatsInterface::SetStatData(keyHash, &iNewVal, sizeof(u32), flags);
		}
		break;
	case STAT_TYPE_TIME:
	case STAT_TYPE_UINT64:
		{
			statAssertf(fAmount >= 0.0f, "Setting a negative value in a unsigned stat - type \"%d\" for stat \"%s\"", StatsInterface::GetStatType(keyHash), StatsInterface::GetKeyName(keyHash));
			u64 iNewVal = static_cast<u64>(fAmount);
			iNewVal = iNewVal + StatsInterface::GetUInt64Stat(keyHash);
			StatsInterface::SetStatData(keyHash, &iNewVal, sizeof(u64), flags);
		}
		break;

	default:
		statErrorf("Invalid tag type \"%d\" for stat \"%s\"", StatsInterface::GetStatType(keyHash), StatsInterface::GetKeyName(keyHash));
		break;
	}

	//Update accuracy.
	if (keyHash == STAT_SHOTS || keyHash == STAT_HITS_PEDS_VEHICLES)
	{
		if (StatsInterface::IsKeyValid(STAT_WEAPON_ACCURACY))
		{
			float hits  = (float)StatsInterface::GetIntStat(STAT_HITS_PEDS_VEHICLES);
			if(hits>0.0f)
			{
				float shots = (float)StatsInterface::GetIntStat(STAT_SHOTS);
				if(shots==0.0f)
					shots=1.0f;
				float accuracy = (hits*100.0f)/shots;
				StatsInterface::SetStatData(STAT_WEAPON_ACCURACY.GetStatId(), accuracy, STATUPDATEFLAG_ASSERTONLINESTATS);
			}
		}
	}
}


void  
CStatsMgr::DecrementStat(const StatId& keyHash, const float fAmount, const u32 flags)
{
	if (!CStatsUtils::CanUpdateStats() || !statVerifyf(StatsInterface::IsKeyValid(keyHash), "Invalid key=\"%s\" hash=\"%d\"", keyHash.GetName(), keyHash.GetHash()))
	{
		return;
	}

	switch (StatsInterface::GetStatType(keyHash))
	{
	case STAT_TYPE_INT:
		{
			int iVal = static_cast<int>(fAmount);
			iVal = Max(StatsInterface::GetIntStat(keyHash) - iVal, 0);
			StatsInterface::SetStatData(keyHash, &iVal, sizeof(int), flags);
		}
		break;
	case STAT_TYPE_INT64:
		{
			s64 iOldVal = StatsInterface::GetInt64Stat(keyHash);
			s64 iNewVal = static_cast<s64>(fAmount);
			iNewVal = (iOldVal < iNewVal) ? 0 : iOldVal - iNewVal;
			StatsInterface::SetStatData(keyHash, &iNewVal, sizeof(s64), flags);
		}
		break;
	case STAT_TYPE_FLOAT:
		{
			float fVal = Max(StatsInterface::GetFloatStat(keyHash) - fAmount, 0.0f);
			StatsInterface::SetStatData(keyHash, &fVal, sizeof(float), flags);
		}
		break;
	case STAT_TYPE_UINT8:
		{
			u8 iOldVal = StatsInterface::GetUInt8Stat(keyHash);
			u8 iNewVal = static_cast<u8>(fAmount);
			iNewVal = (iOldVal < iNewVal) ? 0 : iOldVal - iNewVal;
			StatsInterface::SetStatData(keyHash, &iNewVal, sizeof(u8), flags);
		}
		break;
	case STAT_TYPE_UINT16:
		{
			u16 iOldVal = StatsInterface::GetUInt16Stat(keyHash);
			u16 iNewVal = static_cast<u16>(fAmount);
			iNewVal = (iOldVal < iNewVal) ? 0 : iOldVal - iNewVal;
			StatsInterface::SetStatData(keyHash, &iNewVal, sizeof(u16), flags);
		}
		break;
	case STAT_TYPE_UINT32:
		{
			u32 iOldVal = StatsInterface::GetUInt32Stat(keyHash);
			u32 iNewVal = static_cast<u32>(fAmount);
			iNewVal = (iOldVal < iNewVal) ? 0 : iOldVal - iNewVal;
			StatsInterface::SetStatData(keyHash, &iNewVal, sizeof(u32), flags);
		}
		break;
	case STAT_TYPE_TIME:
	case STAT_TYPE_UINT64:
		{
			u64 iOldVal = StatsInterface::GetUInt64Stat(keyHash);
			u64 iNewVal = static_cast<u64>(fAmount);
			iNewVal = (iOldVal < iNewVal) ? 0 : iOldVal - iNewVal;
			StatsInterface::SetStatData(keyHash, &iNewVal, sizeof(u64), flags);
		}
		break;

	default:
		statErrorf("Invalid tag type \"%d\" for stat \"%s\"", StatsInterface::GetStatType(keyHash), StatsInterface::GetKeyName(keyHash));
		break;
	}

	//Update accuracy.
	if (keyHash == STAT_SHOTS || keyHash == STAT_HITS_PEDS_VEHICLES)
	{
		if (StatsInterface::IsKeyValid(STAT_WEAPON_ACCURACY))
		{
			float hits  = (float)StatsInterface::GetIntStat(STAT_HITS_PEDS_VEHICLES);
			if(hits>0.0f)
			{
				float shots = (float)StatsInterface::GetIntStat(STAT_SHOTS);
				if(shots==0.0f)
					shots=1.0f;
				float accuracy = (hits*100.0f)/shots;
				StatsInterface::SetStatData(STAT_WEAPON_ACCURACY.GetStatId(), accuracy, STATUPDATEFLAG_ASSERTONLINESTATS);
			}
		}
	}
}

void 
CStatsMgr::SetGreater(const StatId& keyHash, const float fNewAmount, const u32 flags)
{
	if (!CStatsUtils::CanUpdateStats() 
		|| !statVerifyf(StatsInterface::IsKeyValid(keyHash), "Invalid Stat key=\"%s\" hash=\"%d\"", keyHash.GetName(), keyHash.GetHash())
		|| 0.0f >= fNewAmount)
		return;

	const StatType iType = StatsInterface::GetStatType(keyHash);
	switch (iType)
	{
	case STAT_TYPE_INT:
		{
			const int iNewVal = static_cast<int>(fNewAmount);
			if(iNewVal > StatsInterface::GetIntStat(keyHash))
			{
				StatsInterface::SetStatData(keyHash, iNewVal, flags);
			}
		}
		break;
	case STAT_TYPE_INT64:
		{
			const s64 iNewVal = static_cast<s64>(fNewAmount);
			if(iNewVal > StatsInterface::GetInt64Stat(keyHash))
			{
				StatsInterface::SetStatData(keyHash, iNewVal, flags);
			}
		}
		break;

	case STAT_TYPE_FLOAT: 
		if(fNewAmount > StatsInterface::GetFloatStat(keyHash))
		{
			StatsInterface::SetStatData(keyHash, fNewAmount, flags);
		}
		break;

	case STAT_TYPE_UINT8:
		{
			const u8 iNewVal = static_cast<u8>(fNewAmount);
			if(iNewVal > StatsInterface::GetUInt8Stat(keyHash)) 
			{
				StatsInterface::SetStatData(keyHash, iNewVal, flags);
			}
		}
		break;

	case STAT_TYPE_UINT16:
		{
			const u16 iNewVal = static_cast<u16>(fNewAmount);
			if (iNewVal > StatsInterface::GetUInt16Stat(keyHash))
			{
				StatsInterface::SetStatData(keyHash, iNewVal, flags);
			}
		}
		break;

	case STAT_TYPE_UINT32:
		{
			const u32 iNewVal = static_cast<u32>(fNewAmount);
			if (iNewVal > StatsInterface::GetUInt32Stat(keyHash))
			{
				StatsInterface::SetStatData(keyHash, iNewVal, flags);
			}
		}
		break;
	case STAT_TYPE_TIME:
	case STAT_TYPE_UINT64:
		{
			const u64 iNewVal = static_cast<u64>(fNewAmount);
			if (iNewVal > StatsInterface::GetUInt64Stat(keyHash))
			{
				StatsInterface::SetStatData(keyHash, iNewVal, flags);
			}
		}
		break;

	default:
		statErrorf("Invalid tag type \"%d\" for stat \"%s\"", iType, StatsInterface::GetKeyName(keyHash));
		break;
	}
}

void 
CStatsMgr::SetLesser(const StatId& keyHash, const float fNewAmount, const u32 flags)
{
	if (!CStatsUtils::CanUpdateStats() || !statVerify(StatsInterface::IsKeyValid(keyHash)))
		return;

	const StatType iType = StatsInterface::GetStatType(keyHash);
	switch (iType)
	{
	case STAT_TYPE_INT:
		{
			const int iNewVal = static_cast<int>(fNewAmount);
			if(iNewVal < StatsInterface::GetIntStat(keyHash))
			{
				StatsInterface::SetStatData(keyHash, iNewVal, flags) ;
			}
		}
		break;

	case STAT_TYPE_INT64:
		{
			const s64 iNewVal = static_cast<s64>(fNewAmount);
			if(iNewVal < StatsInterface::GetInt64Stat(keyHash))
			{
				StatsInterface::SetStatData(keyHash, iNewVal, flags) ;
			}
		}
		break;

	case STAT_TYPE_FLOAT: 
		if(fNewAmount < StatsInterface::GetFloatStat(keyHash))
		{
			StatsInterface::SetStatData(keyHash, fNewAmount, flags);
		}
		break;

	case STAT_TYPE_UINT8:
		{
			const u8 iNewVal = static_cast<u8>(fNewAmount);
			if(iNewVal < StatsInterface::GetUInt8Stat(keyHash)) 
			{
				StatsInterface::SetStatData(keyHash, iNewVal, flags);
			}
		}
		break;

	case STAT_TYPE_UINT16:
		{
			const u16 iNewVal = static_cast<u16>(fNewAmount);
			if (iNewVal < StatsInterface::GetUInt16Stat(keyHash))
			{
				StatsInterface::SetStatData(keyHash, iNewVal, flags);
			}
		}
		break;

	case STAT_TYPE_UINT32:
		{
			const u32 iNewVal = static_cast<u32>(fNewAmount);
			if (iNewVal < StatsInterface::GetUInt32Stat(keyHash))
			{
				StatsInterface::SetStatData(keyHash, iNewVal, flags);
			}
		}
		break;
	case STAT_TYPE_TIME:
	case STAT_TYPE_UINT64:
		{
			const u64 iNewVal = static_cast<u64>(fNewAmount);
			if (iNewVal < StatsInterface::GetUInt64Stat(keyHash))
			{
				StatsInterface::SetStatData(keyHash, iNewVal);
			}
		}
		break;

	default:
		statErrorf("Invalid tag type \"%d\" for stat \"%s\"", iType, StatsInterface::GetKeyName(keyHash));
		break;
	}
}

bool 
CStatsMgr::IsFirstPersonShooterCamera(const bool vehicleCheck)
{
	if (camInterface::IsRenderingFirstPersonShooterCamera())
		return true;

	if (vehicleCheck)
	{
		const camBaseCamera* dominantCam = camInterface::GetDominantRenderedCamera();
		if(dominantCam && dominantCam->GetIsClassId(camCinematicMountedCamera::GetStaticClassId()))
		{
			// All bonnet and pov vehicle cameras are valid.
			return (true);
		}
	}

	return false;
}

void
CStatsMgr::UpdateTimersInStartMenu()
{
	static float  s_timeInStartMenu = 0;
	static bool s_startMenuOpen = false;
	static bool s_isInMP = false;

	if (NetworkInterface::IsGameInProgress() 
		&& NetworkInterface::IsInFreeMode() 
		&& CPauseMenu::IsActive( PM_WaitUntilFullyFadedInOnly ) 
		&& CPauseMenu::GetCurrentMenuVersion() == FE_MENU_VERSION_MP_PAUSE)
	{
		s_startMenuOpen = true;
		s_timeInStartMenu += fwTimer::GetSystemTimeStep() * 1000.0f;
	}
	else if (s_startMenuOpen)
	{
		s_startMenuOpen = false;

		if (!s_isInMP && NetworkInterface::IsGameInProgress() && StatsInterface::GetStatsPlayerModelIsMultiplayer())
		{
			s_isInMP = true;
			s_timeInStartMenu = 0.0f;
		}
		else if (s_isInMP && !NetworkInterface::IsNetworkOpen() && StatsInterface::GetStatsPlayerModelIsSingleplayer())
		{
			s_isInMP = false;
			s_timeInStartMenu = 0.0f;
		}

		if (s_timeInStartMenu > 0.0f)
		{
			if ((StatsInterface::GetStatsPlayerModelIsMultiplayer() && StatsInterface::CloudFileLoadPending(STAT_MP_CATEGORY_DEFAULT)) 
				|| StatsInterface::GetStatsPlayerModelIsSingleplayer())
			{
				static StatId s_stattime("TOTAL_STARTMENU_TIME");
				StatsInterface::IncrementStat(s_stattime, s_timeInStartMenu, STATUPDATEFLAG_ASSERTONLINESTATS);
			}
		}

		s_timeInStartMenu = 0.0f;
	}
}

void
CStatsMgr::Update()
{
	PF_START_TIMEBAR("stats mgr");

	if (sm_StatsData.GetCount() == 0)
		return;

	CPed* playerPed = FindPlayerPed();
	if (!playerPed)
		return;

	CPlayerInfo* pi = playerPed->GetPlayerInfo();
	if (!pi)
		return;

	//Alternative update for Director mode and other situations where stats tracking is not enabled.
	if (!CStatsUtils::IsStatsTrackingEnabled())
	{
		//Stats Data Manager update
		sm_StatsData.Update();

		CashPackInfo::GetInstance().Update();

		return;
	}

	UpdateTimersInStartMenu( );

	static bool s_updatingStats = true;

	const bool gameIsPaused       = fwTimer::IsGamePaused();
	const bool playerStatePlaying = (pi->GetPlayerState() == CPlayerInfo::PLAYERSTATE_PLAYING);

	if (!gameIsPaused && playerStatePlaying)
	{
		s_updatingStats = true;

		//If the network is open then make sure the match has started.
		const bool setPlayingTime = ( (NetworkInterface::IsGameInProgress() && NetworkInterface::IsLocalPlayerOnline()) || !NetworkInterface::IsNetworkOpen() );

		if (setPlayingTime && StatsInterface::GetStatsPlayerModelValid())
		{
			if (s_isInSP && NetworkInterface::IsGameInProgress() && StatsInterface::GetStatsPlayerModelIsMultiplayer())
			{
				statDebugf1("Detected transition to multiplayer, clearing timers.");
				s_isInSP = false;
				sm_FirstPersonTime = 0.0f;
				sm_3rdPersonTime = 0.0f;
				sm_PlayingTime = 0.0f;
				sm_SessionRunningTime = 0.0f;
				sm_StealthTime = 0.0f;
			}
			else if (!s_isInSP && !NetworkInterface::IsNetworkOpen() && StatsInterface::GetStatsPlayerModelIsSingleplayer())
			{
				statDebugf1("Detected transition to singleplayer, clearing timers.");
				s_isInSP = true;
				sm_FirstPersonTime = 0.0f;
				sm_3rdPersonTime = 0.0f;
				sm_PlayingTime = 0.0f;
				sm_SessionRunningTime = 0.0f;
				sm_StealthTime = 0.0f;
			}

			const float fTimeStepInMs = fwTimer::GetSystemTimeStep() * 1000.0f;
			statAssertf(fTimeStepInMs >= 0.0f, "fTimeStepInMs is below zero");

			if (fTimeStepInMs > 0.0f)
			{
				if (IsFirstPersonShooterCamera())
					sm_FirstPersonTime += fTimeStepInMs;
				else
					sm_3rdPersonTime += fTimeStepInMs;

				sm_PlayingTime += fTimeStepInMs;
				sm_SessionRunningTime += fTimeStepInMs;

				const CPedMotionData* md = playerPed->GetMotionData();
				if (md && md->GetUsingStealth())
				{
					sm_StealthTime += fTimeStepInMs;
				}
			}
		}

		if (s_writeLeaderboardOnboot)
			s_writeLeaderboardOnboot = !StatsInterface::TryVehicleLeaderboardWriteOnBoot( );
	}

	if(s_updatingStats)
	{
		if (gameIsPaused || sm_PlayingTime >= UPDATE_RATE_PLAYING_TIME || !playerStatePlaying)
		{
			ClearPlayingTimeStats( );
			s_updatingStats = false;
		}
	}

	//time in milliseconds
	const unsigned curTime = fwTimer::GetTimeInMilliseconds_NonScaledClipped();

	//Reset Killing spree time now and start counting
	if(0 < sm_KillingSpreeTotalTime && (curTime - sm_KillingSpreeTime) > KILLSPREE_INTERVAL)
	{
		sm_KillingSpreeTime      = 0;
		sm_KillingSpreeTotalTime = 0;
		sm_KillingSpreeCounter   = 0;
	}

	//Reset Destroyed vehicles spree time now and start counting
	if(0 < sm_VehicleDestroyedSpreeTotalTime && (curTime - sm_VehicleDestroyedSpreeTime) > DESTROYED_VEHICLES_SPREE_INTERVAL)
	{
		sm_VehicleDestroyedSpreeTime      = 0;
		sm_VehicleDestroyedSpreeTotalTime = 0;
		sm_VehicleDestroyedSpreeCounter   = 0;
	}

	CVehicle* playerVehicle = playerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle) ? playerPed->GetMyVehicle() : NULL;

	//Using a vehicle.
	if (playerVehicle)
	{
		if(sm_WasInVehicle)
		{
			WorkOutPlayerVehicleStats(playerPed, playerVehicle);
		}
		else
		{
			sm_WasInVehicle = true;
			sm_OnFootPosStart.Zero();
		}
	}
	//Not using a vehicle.
	else
	{
		EndVehicleStats();

		if (IsPlayerActive())
		{
			ProcessOnFootVehicleChecks(playerPed);

			if (!sm_cheatIsActive && !IsOnTrain() && !IsInsideTrain() && !IsOnBoat() && !IsOnCar() && MIN_SPEED_TO_CONSIDER_MOVEMENT < playerPed->GetVelocity().Mag2())
			{
				if (!sm_VehiclePreviousPosition.IsZero())
				{
					sm_VehiclePreviousPosition.Zero();
				}
					
				//-----------------------------
				//----- SWIMMING
				//-----------------------------
				if (playerPed->GetIsSwimming())
				{
					//Checks if there is movement on the pad as if bobbing about on water, it increments this stat...
					const CControl* control = playerPed->GetControlFromPlayer();
					if (control && (control->GetPedWalkLeftRight().GetNorm() || control->GetPedWalkUpDown().GetNorm() || control->GetPedSprintIsDown()))
					{
						StatsInterface::IncrementStat(STAT_DIST_SWIMMING.GetStatId(), playerPed->GetDistanceTravelled(), STATUPDATEFLAG_ASSERTONLINESTATS);
						StatsInterface::IncrementStat(STAT_TIME_SWIMMING.GetStatId(), fwTimer::GetSystemTimeStep()*1000.0f, STATUPDATEFLAG_ASSERTONLINESTATS);
					}

					// Stop bail out of vehicle when in water
					if(!sm_StartVehicleBail.IsZero())
					{
						statDebugf3("Stop Bailing out, player is swimming");
						sm_StartVehicleBail.Zero();
						sm_LastVehicleBailPosition.Zero();
						sm_VehicleBailDistance=0.0f;
					}
				}

				//-----------------------------
				//----- FREE FALLING
				//-----------------------------
				else if (playerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsInTheAir))
				{
					if (0 == sm_ParachuteStartTime && playerPed->GetPedResetFlag(CPED_RESET_FLAG_IsParachuting))
					{
						sm_ParachuteStartTime = fwTimer::GetSystemTimeInMilliseconds();
						sm_ParachuteStartPos  = VEC3V_TO_VECTOR3(playerPed->GetTransform().GetPosition());
						statDebugf3("[Parachute] Start parachuting ParachuteStartPos <x=%f,y=%f,z=%f> ", sm_ParachuteStartPos.x, sm_ParachuteStartPos.y, sm_ParachuteStartPos.z);
					}
					
					if (!sm_WasInAir && playerPed->GetPedResetFlag(CPED_RESET_FLAG_IsFalling))
					{
						Vec3V coords = playerPed->GetTransform().GetPosition();

						if (coords.GetZf() > sm_FreeFallStartPos.z)
						{
							const float groundZ  = CGameWorldHeightMap::GetMinHeightFromWorldHeightMap(coords.GetXf(), coords.GetYf());
							const float altitude = coords.GetZf() - groundZ;

							if (altitude > sm_TuneMetadata.m_FreefallThresold)
							{
								sm_WasInAir = true;
								sm_FreeFallStartPos = VEC3V_TO_VECTOR3(coords);
								statDebugf3("Start Freefall Position <x=%f,y=%f,z=%f> altitude=<%f>", sm_FreeFallStartPos.x, sm_FreeFallStartPos.y, sm_FreeFallStartPos.z, altitude);

								sm_FreeFallStartTimer = fwTimer::GetTimeInMilliseconds();
								sm_FreeFallDueToPlayerDamage = false;

								if (playerPed->GetWeaponDamageEntity())
								{
									const u32 timeSinceLastDamage = fwTimer::GetTimeInMilliseconds() - playerPed->GetWeaponDamagedTime();
									if (timeSinceLastDamage <= sm_TuneMetadata.m_FreefallTimeSinceLastDamage)
									{
										sm_FreeFallDueToPlayerDamage = true;
									}
								}
							}
						}
					}

					if(!sm_FreeFallStartPos.IsZero())
					{
						Vec3V coords = playerPed->GetTransform().GetPosition();
						//Altitude from freefall start
						if(coords.GetZf() > CGameWorldHeightMap::GetMinHeightFromWorldHeightMap(coords.GetXf(), coords.GetYf()))
						{
							sm_FreeFallAltitude = sm_FreeFallStartPos.z - coords.GetZf();
						}
					}

					if(!sm_ChallengeSkydiveStartPos.IsZero() && sm_ParachuteDeployPos.IsZero())
					{
						Vec3V coords = playerPed->GetTransform().GetPosition();
						
						// If player is higher than they were when jump started, update the jump start position.
						// This can happen when player is jumping from heli that moves upward
						if(VEC3V_TO_VECTOR3(coords).GetZ() > sm_ChallengeSkydiveStartPos.GetZ())
						{
							sm_ChallengeSkydiveStartPos = VEC3V_TO_VECTOR3(coords);
							sm_SkydiveDistance = 0.0f;
						}

						//Altitude from skydive start
						if(coords.GetZf() > CGameWorldHeightMap::GetMinHeightFromWorldHeightMap(coords.GetXf(), coords.GetYf()))
						{
							sm_SkydiveDistance = sm_ChallengeSkydiveStartPos.z - coords.GetZf();
						}
					}
				}

				//-----------------------------
				//----- ON FOOT
				//-----------------------------
				else if (!sm_WasInAir && playerPed->GetIsOnFoot() && playerPed->GetIsStanding() && playerPed->GetWasStanding())
				{
					// Update the altitude 
					if (sm_OnFootPosStart.IsZero())
					{
						sm_OnFootPosStart = VEC3V_TO_VECTOR3(playerPed->GetTransform().GetPosition());
					}

					if(!sm_OnFootPosStart.IsZero())
					{
						float currentZf = playerPed->GetTransform().GetPosition().GetZf();

						if (currentZf >= sm_OnFootPosStart.GetZ())
						{
							sm_FootAltitude = currentZf - sm_OnFootPosStart.GetZ();
							UpdateRecordedStat(REC_STAT_ON_FOOT_ALTITUDE, sm_FootAltitude);
						}
						//else if((sm_OnFootPosStart.GetZ()-currentZf) > 1.0f)
						//{
						//	sm_OnFootPosStart.Zero();
						//}
					}

					if(!sm_StartVehicleBail.IsZero())
					{
						statDebugf3("Stop Bailing out, player is standing");
						UpdateRecordedStat(REC_STAT_DIST_BAILING_FROM_VEHICLE, sm_VehicleBailDistance);
						sm_StartVehicleBail.Zero();
						sm_LastVehicleBailPosition.Zero();
						sm_VehicleBailDistance = 0.0f;
					}

					if (!CScriptPeds::IsPedStopped(playerPed))
					{
						const CPedMotionData* md = playerPed->GetMotionData();
						if(md && !md->GetIsStill())
						{
							//Total Distance travelled
							if (NetworkInterface::IsGameInProgress() && StatsInterface::IsKeyValid(STAT_MP_CHAR_DIST_TRAVELLED))
							{
								StatsInterface::IncrementStat(STAT_MP_CHAR_DIST_TRAVELLED, playerPed->GetDistanceTravelled(), STATUPDATEFLAG_ASSERTONLINESTATS);
							}

							//distance running
							if(StatsInterface::IsKeyValid(STAT_DIST_RUNNING.GetHash()) && (md->GetIsRunning() || md->GetIsSprinting()))
							{
								StatsInterface::IncrementStat(STAT_DIST_RUNNING.GetStatId(), playerPed->GetDistanceTravelled(), STATUPDATEFLAG_ASSERTONLINESTATS);
							}
							else
							{
								//distance walked
								StatsInterface::IncrementStat(STAT_DIST_WALKING.GetStatId(), playerPed->GetDistanceTravelled(), STATUPDATEFLAG_ASSERTONLINESTATS);
								StatsInterface::IncrementStat(STAT_TIME_WALKING.GetStatId(), fwTimer::GetSystemTimeStep()*1000.0f, STATUPDATEFLAG_ASSERTONLINESTATS);

								//distance walked in stealth
								if(md->GetUsingStealth() && StatsInterface::IsKeyValid(STAT_DIST_WALKING_STEALTH.GetHash()))
								{
									StatsInterface::IncrementStat(STAT_DIST_WALKING_STEALTH.GetStatId(), playerPed->GetDistanceTravelled(), STATUPDATEFLAG_ASSERTONLINESTATS);
								}
							}
						}
					}
				}
				// Bailing out of the vehicle
				if(!sm_StartVehicleBail.IsZero())
				{
					Vec3V currentPosition = playerPed->GetTransform().GetPosition();
					sm_VehicleBailDistance += Mag(currentPosition - VECTOR3_TO_VEC3V(sm_LastVehicleBailPosition)).Getf();
					sm_LastVehicleBailPosition = VEC3V_TO_VECTOR3(currentPosition);
				}
				
			}
		}
		

		const CCollisionRecord *pCollRecord = playerPed->GetFrameCollisionHistory()->GetMostSignificantCollisionRecordOfType(ENTITY_TYPE_VEHICLE);
		if (pCollRecord && pCollRecord->m_pRegdCollisionEntity.Get())
		{
			CEntity* entity = pCollRecord->m_pRegdCollisionEntity.Get();
			if(entity && entity->GetIsTypeVehicle())
			{
				CVehicle* vehicle = static_cast<CVehicle*>(entity);
				if(vehicle->IsInAir())
				{
					sm_MostRecentCollisionTimeWithPlaneInAir = playerPed->GetFrameCollisionHistory()->GetMostRecentCollisionTime();
				}
			}
		}

		if (!playerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsInTheAir)
			&& !playerPed->GetPedResetFlag(CPED_RESET_FLAG_IsParachuting))
		{
			static const u32 TIME_AFTER_COLLISION_WITH_PLANE = 1000;
			if(fwTimer::GetTimeInMilliseconds() > sm_MostRecentCollisionTimeWithPlaneInAir + TIME_AFTER_COLLISION_WITH_PLANE)
			{
				CheckFreeFallResults(playerPed);
			}

			if(!playerPed->GetPedResetFlag(CPED_RESET_FLAG_IsFalling))
				CheckParachuteResults(playerPed);
		}
	}

	//Updates player in cover and crouch stats
	UpdatePlayerInCoverCrouchStats();

	//Stats Data Manager update
	sm_StatsData.Update();

	CashPackInfo::GetInstance().Update();

	//Reset cheat status flag
	if (sm_cheatIsActive)
	{
		if (0==sm_cheatIsActiveTimer || sm_cheatIsActiveTimer+CHEAT_REST_TIME <= fwTimer::GetTimeInMilliseconds())
		{
			SetCheatIsActive( false );
		}
	}

	u32 playerNearMissCounter = fwTimer::GetTimeInMilliseconds();

	for(unsigned i = 0; i < MAXIMUM_NEAR_MISS_COUNTER; i++)
	{
		if(sm_NearMissCounterArray[i] != 0 && playerNearMissCounter > sm_NearMissCounterArray[i] + NEAR_MISS_TIMER)
		{
			sm_NearMissCounterArray[i] = 0;

			if(!playerVehicle)
			{
				return;
			}

			u32 mostRecentCollisionsTime = playerVehicle->GetFrameCollisionHistory()->GetMostRecentCollisionTime();
			if ( mostRecentCollisionsTime == COLLISION_HISTORY_TIMER_NEVER_HIT 
				|| fwTimer::GetTimeInMilliseconds() > mostRecentCollisionsTime + NEAR_MISS_TIMER)
			{
				RegisterVehicleNearMissAccepted();
			}
		}
	}
}

bool
CStatsMgr::VehicleHasBeenDriven(const u32 vehicleHash, const bool inMultiplayer)
{
	bool result = false;

	if (inMultiplayer)
	{
		for (int i=0; i<sm_MpVehicleDriven.GetCount() && !result; i++)
		{
			result = (vehicleHash == sm_MpVehicleDriven[i]);
		}
	}
	else
	{
		for (int i=0; i<sm_SpVehicleDriven.GetCount() && !result; i++)
		{
			result = (vehicleHash == sm_SpVehicleDriven[i]);
		}
	}

	return result;
}

void  
CStatsMgr::SetMaxCountAsFacebookDriven(const u32 count) 
{
	if(0==sm_maxCountAsFacebookDriven)
	{
		sm_maxCountAsFacebookDriven = count;

		int overridecount = -1;
		if (PARAM_facebookvehiclesdriven.Get(overridecount))
		{
			if (overridecount > 0)
			{
				sm_maxCountAsFacebookDriven = (u32)overridecount;
				statDebugf3("SetMaxCountAsFacebookDriven (overridecount) - %d", sm_maxCountAsFacebookDriven);
			}
		}
		else
		{
			statDebugf3("SetMaxCountAsFacebookDriven - %d", sm_maxCountAsFacebookDriven);
		}
	}
}



class MetricHourlyXP : public rlMetric
{
	RL_DECLARE_METRIC(HOUR_XP, TELEMETRY_CHANNEL_AMBIENT, LOGLEVEL_LOW_PRIORITY);

public:
	MetricHourlyXP(u32 xp, u32 playtime) : m_xp(xp), m_playtime(playtime) {};

	virtual bool Write(RsonWriter* rw) const
	{
		bool result = true;
		result = result && rw->WriteUns("xp", m_xp);
		result = result && rw->WriteUns("pt", m_playtime);
		return result;
	}

private:
	u32 m_xp;
	u32 m_playtime;
};

bool
CStatsMgr::PlayerCharIsValidAndPlayingFreemode()
{
	return (!s_isInSP && NetworkInterface::IsGameInProgress() && StatsInterface::GetStatsPlayerModelIsMultiplayer());
}

void 
CStatsMgr::ClearPlayingTimeStats()
{
	if (sm_PlayingTime > 0.0f)
	{
		bool bTryToWriteVehicleStats = false;

		if (!s_isInSP && NetworkInterface::IsGameInProgress() && StatsInterface::GetStatsPlayerModelIsMultiplayer())
		{
			if (!NetworkInterface::IsInSpectatorMode())
			{
				StatsInterface::IncrementStat(STAT_MP_FIRST_PERSON_CAM_TIME.GetHash(), sm_FirstPersonTime, STATUPDATEFLAG_ASSERTONLINESTATS);
				StatsInterface::IncrementStat(STAT_MP_THIRD_PERSON_CAM_TIME.GetHash(), sm_3rdPersonTime, STATUPDATEFLAG_ASSERTONLINESTATS);
				StatsInterface::IncrementStat(STAT_TOTAL_PLAYING_TIME.GetStatId(), sm_PlayingTime, STATUPDATEFLAG_ASSERTONLINESTATS);
				StatsInterface::SetGreater(STAT_LONGEST_PLAYING_TIME.GetStatId(), sm_SessionRunningTime, STATUPDATEFLAG_ASSERTONLINESTATS);
				StatsInterface::IncrementStat(STAT_MP_PLAYING_TIME.GetHash(), sm_PlayingTime, STATUPDATEFLAG_ASSERTONLINESTATS);

				u64 average = (u64)StatsInterface::GetUInt64Stat(STAT_TOTAL_PLAYING_TIME);

				if (StatsInterface::GetIntStat(STAT_NUMBER_OF_SESSIONS_FM) > 0 && average > 0)
				{
					average /= (u64)StatsInterface::GetIntStat(STAT_NUMBER_OF_SESSIONS_FM);
				}

				StatsInterface::SetStatData(STAT_AVERAGE_TIME_PER_SESSON, average);

				if (StatsInterface::GetUInt64Stat(STAT_MP_PLAYING_TIME_NEW.GetHash()) == 0)
				{
					StatsInterface::SetStatData(STAT_MP_PLAYING_TIME_NEW.GetHash(), StatsInterface::GetUInt64Stat(STAT_MP_PLAYING_TIME.GetHash()), STATUPDATEFLAG_ASSERTONLINESTATS);
				}
				else
				{
					StatsInterface::IncrementStat(STAT_MP_PLAYING_TIME_NEW.GetHash(), sm_PlayingTime, STATUPDATEFLAG_ASSERTONLINESTATS);
				}

				StatsInterface::IncrementStat(StatId("LEADERBOARD_PLAYING_TIME"), sm_PlayingTime, STATUPDATEFLAG_ASSERTONLINESTATS);

				const u32 timepassed    = StatsInterface::GetUInt32Stat(STAT_TOTAL_PLAYING_TIME.GetHash());
				const s32 currHour      = CStatsUtils::ConvertToHours(timepassed);
				static s32 previousHour = currHour;
				if (previousHour != currHour)
				{
					CNetworkTelemetry::AppendMetric(MetricHourlyXP((u32)StatsInterface::GetIntStat(STAT_CHAR_XP_FM.GetStatId()), timepassed));
					previousHour = currHour;
				}

				::Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("MP_VEHICLE_LB_TRY_WRITE_ON_UPDATES", 0xd5a17534), bTryToWriteVehicleStats);
			}
		}
		else if (s_isInSP && !NetworkInterface::IsNetworkOpen() && StatsInterface::GetStatsPlayerModelIsSingleplayer())
		{
			StatsInterface::IncrementStat(STAT_TOTAL_PLAYING_TIME.GetStatId(), sm_PlayingTime, STATUPDATEFLAG_ASSERTONLINESTATS);
			StatsInterface::SetGreater(STAT_LONGEST_PLAYING_TIME.GetStatId(), sm_SessionRunningTime, STATUPDATEFLAG_ASSERTONLINESTATS);
			StatsInterface::IncrementStat(STAT_PLAYING_TIME.GetHash(), sm_PlayingTime, STATUPDATEFLAG_ASSERTONLINESTATS);
			StatsInterface::IncrementStat(STAT_FIRST_PERSON_CAM_TIME.GetHash(), sm_FirstPersonTime, STATUPDATEFLAG_ASSERTONLINESTATS);
			StatsInterface::IncrementStat(STAT_THIRD_PERSON_CAM_TIME.GetHash(), sm_3rdPersonTime, STATUPDATEFLAG_ASSERTONLINESTATS);

			::Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("SP_VEHICLE_LB_TRY_WRITE_ON_UPDATES", 0x662bc66c), bTryToWriteVehicleStats);
		}

		sm_3rdPersonTime = 0.0f;
		sm_FirstPersonTime = 0.0f;
		sm_PlayingTime = 0.0f;

		// If this is set to true, system will write vehicle data every GetVehicleLeaderboardWriteInterval
		if (bTryToWriteVehicleStats)
		{
			CheckWriteVehicleRecords( false );
		}
	}

	if (sm_StealthTime > 0.0f)
	{
		StatsInterface::IncrementStat(STAT_STEALTH_TIME.GetHash(), sm_StealthTime, STATUPDATEFLAG_ASSERTONLINESTATS);
		sm_StealthTime = 0.0f;
	}
}

void
CStatsMgr::SetVehicleHasBeenDriven(const u32 vehicleHash, const bool countAsFacebookDriven)
{
	gnetAssert(!VehicleHasBeenDriven(vehicleHash, NetworkInterface::IsGameInProgress()));

	if (!VehicleHasBeenDriven(vehicleHash, NetworkInterface::IsGameInProgress()))
	{
		if (NetworkInterface::IsGameInProgress())
		{
			sm_MpVehicleDriven.PushAndGrow(vehicleHash);
		}
		else
		{
			sm_SpVehicleDriven.PushAndGrow( vehicleHash );

			if (countAsFacebookDriven)
			{
				++sm_currCountAsFacebookDriven;

#if !__NO_OUTPUT
				fwModelId modelId;
				CBaseModelInfo* mi = CModelInfo::GetBaseModelInfoFromHashKey(vehicleHash, &modelId);
				if (mi)
				{
					statDebugf3("New Vehicle driven < %s >, current facebook count is < %d >.", mi->GetModelName(), sm_currCountAsFacebookDriven);
				}
#endif // !__NO_OUTPUT

			}
		}
	}
}

void
CStatsMgr::UpdatePlayerInCoverCrouchStats( )
{
	CPed* playerPed = FindPlayerPed();
	if (!playerPed)
		return;

	if (!StatsInterface::IsKeyValid(STAT_TIME_IN_COVER))
		return;

	// assume not in cover
	bool bIsInCover = false; 

	// Stat: TIME_IN_COVER
	CPedIntelligence* pPI = playerPed->GetPedIntelligence();
	if (pPI)
	{
		CQueriableInterface* pQI = pPI->GetQueriableInterface();
		if (pQI && pQI->IsTaskCurrentlyRunning(CTaskTypes::TASK_IN_COVER))
		{
			if (StatsInterface::IsKeyValid(STAT_TIME_IN_COVER))
			{
				StatsInterface::IncrementStat(STAT_TIME_IN_COVER.GetStatId(), fwTimer::GetSystemTimeStep()*1000.0f, STATUPDATEFLAG_ASSERTONLINESTATS);
			}

			bIsInCover = true;
		}
	}

	// Stat: ENTERED_COVER_AND_SHOT
	if (sm_bWasInCover && !bIsInCover)
	{
		if (sm_bShotFiredInCover && StatsInterface::IsKeyValid(STAT_ENTERED_COVER_AND_SHOT))
		{
			StatsInterface::IncrementStat(STAT_ENTERED_COVER_AND_SHOT.GetStatId(), 1.0f, STATUPDATEFLAG_ASSERTONLINESTATS);
		}

		// reset tracking variable
		sm_bShotFiredInCover = false;
	}

	// Stat: ENTERED_COVER
	if (bIsInCover && !sm_bWasInCover && StatsInterface::IsKeyValid(STAT_ENTERED_COVER))
	{
		StatsInterface::IncrementStat(STAT_ENTERED_COVER.GetStatId(), 1.0f, STATUPDATEFLAG_ASSERTONLINESTATS);
	}

	// track player cover state
	sm_bWasInCover = bIsInCover;

	// crouch states
	bool bIsCrouching = playerPed->GetIsCrouching();

	// Stat: CROUCHED_AND_SHOT
	if (sm_bWasCrouching && !bIsCrouching)
	{
		if (sm_bShotFiredInCrouch && StatsInterface::IsKeyValid(STAT_CROUCHED_AND_SHOT))
		{
			StatsInterface::IncrementStat(STAT_CROUCHED_AND_SHOT.GetStatId(), 1.0f, STATUPDATEFLAG_ASSERTONLINESTATS);
		}

		// reset tracking variable
		sm_bShotFiredInCrouch = false;
	}

	// Stat: CROUCHED
	if (bIsCrouching && !sm_bWasCrouching && StatsInterface::IsKeyValid(STAT_CROUCHED))
	{
		StatsInterface::IncrementStat(STAT_CROUCHED.GetStatId(), 1.0f, STATUPDATEFLAG_ASSERTONLINESTATS);
	}

	// track player crouch state
	sm_bWasCrouching = bIsCrouching;
}

void 
CStatsMgr::RegisterMinBetterValue(const StatId& keyHash, int iNewTime, const u32 flags)
{
	Assert(StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_INT) || StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_FLOAT));

	if (StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_INT) && StatsInterface::GetIntStat(keyHash) <= 0)
	{
		StatsInterface::SetStatData(keyHash, iNewTime, flags);
	}
	else if (StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_FLOAT) && StatsInterface::GetFloatStat(keyHash) <= 0.0f)
	{
		float fVal = static_cast<float>(iNewTime);
		StatsInterface::SetStatData(keyHash, fVal, flags);
	}
	else
	{
		if (StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_INT) && StatsInterface::GetIntStat(keyHash) <= 0)
		{
			iNewTime = static_cast<int>(rage::Min((float)StatsInterface::GetIntStat(keyHash), (float)iNewTime));
			StatsInterface::SetStatData(keyHash, iNewTime, flags);
		}
		else if (StatsInterface::GetIsBaseType(keyHash, STAT_TYPE_FLOAT) && StatsInterface::GetFloatStat(keyHash) <= 0.0f)
		{
			float fVal = rage::Min(StatsInterface::GetFloatStat(keyHash), (float)iNewTime);
			StatsInterface::SetStatData(keyHash, fVal, flags);
		}
	}
}

void  
CStatsMgr::RegisterTyresPoppedByGunshot(u16 iRandomSeed, const u8 iWheel, const int iNumWheels)
{
	if (IsPlayerActive() && sm_WheelStatus.GetCount()>0 && iRandomSeed == sm_VehicleRandomSeed)
	{
		if(iWheel<sm_WheelStatus.GetCount() && !sm_WheelStatus[iWheel])
		{
			sm_WheelStatus[iWheel] = true;
			CStatsMgr::IncrementStat(STAT_TIRES_POPPED_BY_GUNSHOT.GetStatId(), 1.0f);
		}
	}
	else
	{
		sm_WheelStatus.Reset();
		sm_WheelStatus.Resize(iNumWheels);

		for (int i=0; i<sm_WheelStatus.GetCount(); i++)
			sm_WheelStatus[i] = false;

		sm_VehicleRandomSeed = iRandomSeed;

		sm_WheelStatus[iWheel] = true;
		CStatsMgr::IncrementStat(STAT_TIRES_POPPED_BY_GUNSHOT.GetStatId(), 1.0f);
	}
}

bool
CStatsMgr::IsPlayerActive()
{
	static bool s_IsActive = false;
	static u32 s_PreviousFrame = 0;

	if (s_PreviousFrame != fwTimer::GetFrameCount())
	{
		CPlayerInfo* pi = CGameWorld::GetMainPlayerInfo();

		if (!pi)
			s_IsActive = false;
		else if (NetworkInterface::IsInSinglePlayerPrivateSession())
			s_IsActive = false;
		else if (g_PlayerSwitch.IsActive())
			s_IsActive = false;
		else if (fwTimer::IsGamePaused())
			s_IsActive = false;
		else if (pi->GetPlayerState() != CPlayerInfo::PLAYERSTATE_PLAYING)
			s_IsActive = false;
		else if (!CGameLogic::IsGameStateInPlay())
			s_IsActive = false;
		else if (!camInterface::IsFadedIn())
			s_IsActive = false;
		else if (!NetworkInterface::IsGameInProgress() && (pi->IsControlsFrontendDisabled() || pi->IsControlsLoadingScreenDisabled()))
			s_IsActive = false;
		else
			s_IsActive = true;

		s_PreviousFrame = fwTimer::GetFrameCount();
	}

	return s_IsActive;
}

void 
CStatsMgr::WorkOutPlayerVehicleStats(CPed* playerPed, CVehicle* playerVehicle)
{
	bool workOutPlayerStats = true;

	if(!sm_HadAttachedParentVehicle)
	{
		sm_HadAttachedParentVehicle = playerVehicle->GetAttachParentVehicle()!=NULL;
	}
	else if (playerVehicle->HasContactWheels() && playerVehicle->GetAttachParentVehicle() == NULL)
	{
		sm_HadAttachedParentVehicle = false;
	}

	if (!statVerify(playerPed)
			 || !statVerify(playerVehicle)
			 || !CStatsMgr::IsPlayerActive()
			 || !playerVehicle->m_nVehicleFlags.bIsThisADriveableCar
			 || (!playerVehicle->m_nVehicleFlags.bEngineOn && !playerVehicle->InheritsFromPlane())
			 || playerVehicle->GetAttachParentVehicle())
	{
		workOutPlayerStats = false;

		if (!sm_ZeroWheelPosHeight.IsZero())
		{
			sm_ZeroWheelPosHeight.Zero();
			sm_GroundProbeHelper.ResetProbe();
		}
	}
	else if (playerVehicle->GetVelocity().Mag2() <= MIN_SPEED_TO_CONSIDER_MOVEMENT)
	{
		workOutPlayerStats = false;
	}
	else if (!(playerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle)) && CScriptPeds::IsPedStopped(playerPed))
	{
		workOutPlayerStats = false;
	}
	else if(playerVehicle->InheritsFromAutomobile())
	{
		// Parachuting with a vehicle makes it too easy to increase some stats, dont track stats at all when parachuting.
		CAutomobile* automobile = static_cast<CAutomobile*>(playerVehicle);
		if(automobile->IsParachuting())
		{
			workOutPlayerStats = CPhysics::ms_bInStuntMode;
		}
	}

	CStatsMgr::CheckVehicleGroundProbeResults();
	CStatsMgr::StartVehicleGroundProbe();
	CStatsMgr::CheckCrashDamageAccumulator();

	sm_CurrentSpeed = 0.0f;

    // We want to update the hydraulics stats whether we're speeding or not
    CStatsMgr::UpdateVehicleHydraulics(playerVehicle);

	if (workOutPlayerStats)
	{
        CStatsMgr::UpdateVehicleStats(playerVehicle);
	}
	else
    {
		CStatsMgr::ClearPlayerVehicleStats();
	}
}

void
CStatsMgr::UpdateVehicleWheelie(CVehicle* playerVehicle)
{
	statAssert(playerVehicle);
	if (!playerVehicle)
		return;
	else if (!playerVehicle->GetDriver())
		return;
	else if (!playerVehicle->GetDriver()->IsLocalPlayer())
		return;

	//CAR || BOAT
	if(playerVehicle->InheritsFromAutomobile() && !playerVehicle->IsTrike() && !playerVehicle->InheritsFromQuadBike() )
	{
		CAutomobile* pCar = (CAutomobile *)playerVehicle;

		if(pCar->GetNumContactWheels() < 3)
			sm_CarLess3WheelCounter += fwTimer::GetSystemTimeStep() * 1000.0f;
		else
			sm_CarLess3WheelCounter = 0.0f;

		// if both left wheels are OFF the ground
		// remember to check wheel pointers exist!
		// this stat is invalid for cars missing the corner wheels
		const bool bHasAllWheels = pCar->GetWheelFromId(VEH_WHEEL_LF) 
									&& pCar->GetWheelFromId(VEH_WHEEL_RF) 
									&& pCar->GetWheelFromId(VEH_WHEEL_LR) 
									&& pCar->GetWheelFromId(VEH_WHEEL_RR);

		if(bHasAllWheels && !pCar->GetWheelFromId(VEH_WHEEL_LF)->GetIsTouching() && !pCar->GetWheelFromId(VEH_WHEEL_LR)->GetIsTouching())
		{
			// if both right wheels are ON the ground
			if(pCar->GetWheelFromId(VEH_WHEEL_RF)->GetIsTouching() 
				&& pCar->GetWheelFromId(VEH_WHEEL_RR)->GetIsTouching() 
				&& pCar->GetFrameCollisionHistory()->GetMaxCollisionImpulseMagLastFrame()==0.0f)
			{
				sm_CarTwoWheelCounter += (s32)fwTimer::GetSystemTimeStepInMilliseconds();
				sm_CarTwoWheelDist    += pCar->GetDistanceTravelled();

				if(sm_TempBufferCounter > fwTimer::GetSystemTimeStepInMilliseconds() / 2)
					sm_TempBufferCounter -= fwTimer::GetSystemTimeStepInMilliseconds() / 2;
				else
					sm_TempBufferCounter = 0;
			}
			else if(sm_CarTwoWheelCounter > 0 && sm_TempBufferCounter < CAR_TWOWHEEL_BUFFERLIMIT)
				sm_TempBufferCounter += fwTimer::GetSystemTimeStepInMilliseconds();
			else
			{
				if(sm_CarTwoWheelCounter >= CAR_TWOWHEEL_MINBEST)
				{
					sm_BestCarTwoWheelsTimeMs = sm_CarTwoWheelCounter;
					sm_BestCarTwoWheelsDistM  = sm_CarTwoWheelDist;

					//Increment the stats:
					StatsInterface::SetGreater(STAT_LONGEST_2WHEEL_TIME.GetStatId(), (float)sm_CarTwoWheelCounter, STATUPDATEFLAG_ASSERTONLINESTATS);
					StatsInterface::SetGreater(STAT_LONGEST_2WHEEL_DIST.GetStatId(), sm_CarTwoWheelDist, STATUPDATEFLAG_ASSERTONLINESTATS);
					statDebugf3("Player 2-Wheels for %d ms and %f m\n", sm_BestCarTwoWheelsTimeMs, sm_BestCarTwoWheelsDistM);

					const CVehicleModelInfo* vi = playerVehicle->GetVehicleModelInfo();
					if (statVerify( vi ) && statVerify( CanUpdateVehicleRecord(vi->GetHashKey()) ))
					{
						++sm_vehicleRecords[sm_curVehIdx].m_NumWheelies;

						if (sm_CarTwoWheelCounter > sm_vehicleRecords[sm_curVehIdx].m_LongestWheelieTime)
							sm_vehicleRecords[sm_curVehIdx].m_LongestWheelieTime = sm_CarTwoWheelCounter;

						if (sm_CarTwoWheelDist > sm_vehicleRecords[sm_curVehIdx].m_LongestWheelieDist)
							sm_vehicleRecords[sm_curVehIdx].m_LongestWheelieDist = sm_CarTwoWheelDist;
					}
				}

				// reset values
				sm_CarTwoWheelCounter = 0;
				sm_CarTwoWheelDist    = 0.0f;
				sm_TempBufferCounter  = 0;
			}
		}
		// if both right wheels are OFF the ground
		else if(bHasAllWheels && !pCar->GetWheelFromId(VEH_WHEEL_RF)->GetIsTouching() && !pCar->GetWheelFromId(VEH_WHEEL_RR)->GetIsTouching())
		{
			// if both left wheels are ON the ground
			if(pCar->GetWheelFromId(VEH_WHEEL_LF)->GetIsTouching() 
				&& pCar->GetWheelFromId(VEH_WHEEL_LR)->GetIsTouching() 
				&& pCar->GetFrameCollisionHistory()->GetMaxCollisionImpulseMagLastFrame()==0.0f)
			{
				sm_CarTwoWheelCounter += (s32)fwTimer::GetSystemTimeStepInMilliseconds();
				sm_CarTwoWheelDist    += pCar->GetDistanceTravelled();

				if(sm_TempBufferCounter > fwTimer::GetSystemTimeStepInMilliseconds() / 2)
					sm_TempBufferCounter -= fwTimer::GetSystemTimeStepInMilliseconds() / 2;
				else
					sm_TempBufferCounter = 0;
			}
			else if(sm_CarTwoWheelCounter > 0 && sm_TempBufferCounter < CAR_TWOWHEEL_BUFFERLIMIT)
				sm_TempBufferCounter += fwTimer::GetSystemTimeStepInMilliseconds();
			else
			{
				if(sm_CarTwoWheelCounter >= CAR_TWOWHEEL_MINBEST)
				{
					sm_BestCarTwoWheelsTimeMs = sm_CarTwoWheelCounter;
					sm_BestCarTwoWheelsDistM  = sm_CarTwoWheelDist;

					//Increment the stats:
					StatsInterface::SetGreater(STAT_LONGEST_2WHEEL_TIME.GetStatId(), (float)sm_CarTwoWheelCounter, STATUPDATEFLAG_ASSERTONLINESTATS);
					StatsInterface::SetGreater(STAT_LONGEST_2WHEEL_DIST.GetStatId(), sm_CarTwoWheelDist, STATUPDATEFLAG_ASSERTONLINESTATS);
					statDebugf3("Player 2-Wheels for %d ms and %f m\n", sm_BestCarTwoWheelsTimeMs, sm_BestCarTwoWheelsDistM);

					const CVehicleModelInfo* vi = playerVehicle->GetVehicleModelInfo();
					if (statVerify( vi ) && statVerify( CanUpdateVehicleRecord(vi->GetHashKey()) ))
					{
						++sm_vehicleRecords[sm_curVehIdx].m_NumWheelies;

						if (sm_CarTwoWheelCounter > sm_vehicleRecords[sm_curVehIdx].m_LongestWheelieTime)
							sm_vehicleRecords[sm_curVehIdx].m_LongestWheelieTime = sm_CarTwoWheelCounter;

						if (sm_CarTwoWheelDist > sm_vehicleRecords[sm_curVehIdx].m_LongestWheelieDist)
							sm_vehicleRecords[sm_curVehIdx].m_LongestWheelieDist = sm_CarTwoWheelDist;
					}
				}

				// reset values
				sm_CarTwoWheelCounter = 0;
				sm_CarTwoWheelDist    = 0.0f;
				sm_TempBufferCounter  = 0;
			}
		}
		else if(sm_CarTwoWheelCounter > 0)
		{
			if(sm_CarTwoWheelCounter >= CAR_TWOWHEEL_MINBEST)
			{
				sm_BestCarTwoWheelsTimeMs = sm_CarTwoWheelCounter;
				sm_BestCarTwoWheelsDistM  = sm_CarTwoWheelDist;

				//Increment the stats:
				StatsInterface::SetGreater(STAT_LONGEST_2WHEEL_TIME.GetStatId(), (float)sm_CarTwoWheelCounter, STATUPDATEFLAG_ASSERTONLINESTATS);
				StatsInterface::SetGreater(STAT_LONGEST_2WHEEL_DIST.GetStatId(), sm_CarTwoWheelDist, STATUPDATEFLAG_ASSERTONLINESTATS);
				statDebugf3("Player 2-Wheels for %d ms and %f m\n", sm_BestCarTwoWheelsTimeMs, sm_BestCarTwoWheelsDistM);

				const CVehicleModelInfo* vi = playerVehicle->GetVehicleModelInfo();
				if (statVerify( vi ) && statVerify( CanUpdateVehicleRecord(vi->GetHashKey()) ))
				{
					++sm_vehicleRecords[sm_curVehIdx].m_NumWheelies;

					if (sm_CarTwoWheelCounter > sm_vehicleRecords[sm_curVehIdx].m_LongestWheelieTime)
						sm_vehicleRecords[sm_curVehIdx].m_LongestWheelieTime = sm_CarTwoWheelCounter;

					if (sm_CarTwoWheelDist > sm_vehicleRecords[sm_curVehIdx].m_LongestWheelieDist)
						sm_vehicleRecords[sm_curVehIdx].m_LongestWheelieDist = sm_CarTwoWheelDist;
				}
			}

			// reset values
			sm_CarTwoWheelCounter = 0;
			sm_CarTwoWheelDist    = 0.0f;
			sm_TempBufferCounter  = 0;
		}

		// not on a bike
		sm_BikeRearWheelCounter  = 0;
		sm_BikeRearWheelDist     = 0.0f;
		sm_IsWheelingOnPushBike  = false;
		sm_IsDoingAStoppieOnPushBike = false;
		sm_BikeFrontWheelCounter = 0;		
		sm_BikeFrontWheelDist    = 0.0f;
		sm_BestBikeWheelieTimeMs = 0;
		sm_BestBikeWheelieDistM  = 0.0f;
		sm_BestBikeStoppieTimeMs = 0;
		sm_BestBikeStoppieDistM  = 0.0f;
	}
	else if(playerVehicle->InheritsFromBike() || playerVehicle->InheritsFromQuadBike() || playerVehicle->IsTrike())
	{
		bool rearWheelsTouching = false;
		bool frontWheelsTouching = false;

		if(playerVehicle->InheritsFromBike())
		{
			rearWheelsTouching = playerVehicle->GetWheelFromId(BIKE_WHEEL_R)->GetIsTouching();
			frontWheelsTouching = playerVehicle->GetWheelFromId(BIKE_WHEEL_F)->GetIsTouching();
		}
		else
		{
			rearWheelsTouching = playerVehicle->GetWheelFromId(VEH_WHEEL_LR)->GetIsTouching() && ( playerVehicle->IsReverseTrike() || playerVehicle->GetWheelFromId(VEH_WHEEL_RR)->GetIsTouching() );
			frontWheelsTouching = playerVehicle->GetWheelFromId(VEH_WHEEL_LF)->GetIsTouching();
		}

		// if front wheel off ground - do wheelie checks
		if(!frontWheelsTouching && sm_BikeFrontWheelCounter==0 && !sm_IsOnAnotherVehicle)
		{
			if(rearWheelsTouching 
				&& (playerVehicle->GetFrameCollisionHistory()->GetMaxCollisionImpulseMagLastFrame() < 5.0f 
				|| playerVehicle->GetVelocity().Mag2() > MIN_SPEED_TO_CONSIDER_MOVEMENT))
			{
				sm_BikeRearWheelCounter += (s32)fwTimer::GetSystemTimeStepInMilliseconds();
				sm_BikeRearWheelDist    += playerVehicle->GetDistanceTravelled();

				if(sm_TempBufferCounter > fwTimer::GetSystemTimeStepInMilliseconds() / 5)
					sm_TempBufferCounter -= fwTimer::GetSystemTimeStepInMilliseconds() / 5;
				else
					sm_TempBufferCounter = 0;
			}
			else if(sm_BikeRearWheelCounter > 0 && sm_TempBufferCounter < BIKE_ONEWHEEL_BUFFERLIMIT)
				sm_TempBufferCounter += fwTimer::GetSystemTimeStepInMilliseconds();
			else
			{
				if(sm_BikeRearWheelCounter >= BIKE_REARWHEEL_MINBEST)
				{
					sm_BestBikeWheelieTimeMs = sm_BikeRearWheelCounter;
					sm_BestBikeWheelieDistM  = sm_BikeRearWheelDist;

					//Increment the stats:
					StatsInterface::SetGreater(STAT_LONGEST_WHEELIE_TIME.GetStatId(), (float)sm_BikeRearWheelCounter, STATUPDATEFLAG_ASSERTONLINESTATS);
					StatsInterface::SetGreater(STAT_LONGEST_WHEELIE_DIST.GetStatId(), sm_BikeRearWheelDist, STATUPDATEFLAG_ASSERTONLINESTATS);
					statDebugf3("Player Wheelie for %d ms and %f m\n", sm_BestBikeWheelieTimeMs, sm_BestBikeWheelieDistM);

					const CVehicleModelInfo* vi = playerVehicle->GetVehicleModelInfo();
					if (statVerify( vi ) && statVerify( CanUpdateVehicleRecord(vi->GetHashKey()) ))
					{
						++sm_vehicleRecords[sm_curVehIdx].m_NumWheelies;

						if (sm_BikeRearWheelCounter > sm_vehicleRecords[sm_curVehIdx].m_LongestWheelieTime)
							sm_vehicleRecords[sm_curVehIdx].m_LongestWheelieTime = sm_BikeRearWheelCounter;

						if (sm_BikeRearWheelDist > sm_vehicleRecords[sm_curVehIdx].m_LongestWheelieDist)
							sm_vehicleRecords[sm_curVehIdx].m_LongestWheelieDist = sm_BikeRearWheelDist;
					}
				}

				StatId stat = STAT_TOTAL_WHEELIE_TIME.GetStatId();
				u64 uTotalWheelieTime = StatsInterface::GetUInt64Stat(stat);
				uTotalWheelieTime += (u64)sm_BikeRearWheelCounter;
				StatsInterface::SetStatData(stat, uTotalWheelieTime, STATUPDATEFLAG_ASSERTONLINESTATS);

				// reset values
				sm_BikeRearWheelCounter = 0;
				sm_BikeRearWheelDist    = 0.0f;
				sm_TempBufferCounter    = 0;
			}
			// Wheelies are too easy with push bikes, so don't count them towards challenge stats
			if (playerVehicle->InheritsFromBicycle() == false) 
			{
				UpdateRecordedStat(REC_STAT_LONGEST_WHEELIE_DIST, sm_BikeRearWheelDist);
			}
			else
			{
				sm_IsWheelingOnPushBike = true;
			}
		}
		//Wheelie just ended
		else if(sm_BikeRearWheelCounter > 0)
		{
			if(sm_BikeRearWheelCounter >= BIKE_REARWHEEL_MINBEST)
			{
				sm_BestBikeWheelieTimeMs = sm_BikeRearWheelCounter;
				sm_BestBikeWheelieDistM  = sm_BikeRearWheelDist;

				//Increment the stats:
				StatsInterface::SetGreater(STAT_LONGEST_WHEELIE_TIME.GetStatId(), (float)sm_BikeRearWheelCounter, STATUPDATEFLAG_ASSERTONLINESTATS);
				StatsInterface::SetGreater(STAT_LONGEST_WHEELIE_DIST.GetStatId(), sm_BikeRearWheelDist, STATUPDATEFLAG_ASSERTONLINESTATS);

				// Wheelies are too easy with push bikes, so don't count them towards challenge stats
				if (playerVehicle->InheritsFromBicycle() == false) 
				{
					UpdateRecordedStat(REC_STAT_LONGEST_WHEELIE_DIST, sm_BikeRearWheelDist);
				}
				statDebugf3("Player Wheelie for %d ms and %f m\n", sm_BestBikeWheelieTimeMs, sm_BestBikeWheelieDistM);

				const CVehicleModelInfo* vi = playerVehicle->GetVehicleModelInfo();
				if (statVerify( vi ) && statVerify( CanUpdateVehicleRecord(vi->GetHashKey()) ))
				{
					++sm_vehicleRecords[sm_curVehIdx].m_NumWheelies;

					if (sm_BikeRearWheelCounter > sm_vehicleRecords[sm_curVehIdx].m_LongestWheelieTime)
						sm_vehicleRecords[sm_curVehIdx].m_LongestWheelieTime = sm_BikeRearWheelCounter;

					if (sm_BikeRearWheelDist > sm_vehicleRecords[sm_curVehIdx].m_LongestWheelieDist)
						sm_vehicleRecords[sm_curVehIdx].m_LongestWheelieDist = sm_BikeRearWheelDist;
				}
			}

            SendStuntEvent(ST_WHEELIE, (float)sm_BikeRearWheelCounter/1000.0f);

			StatId stat = STAT_TOTAL_WHEELIE_TIME.GetStatId();
			u64 uTotalWheelieTime = StatsInterface::GetUInt64Stat(stat);
			uTotalWheelieTime += (u64)sm_BikeRearWheelCounter;
			StatsInterface::SetStatData(stat, uTotalWheelieTime, STATUPDATEFLAG_ASSERTONLINESTATS);

			// reset values
			sm_BikeRearWheelCounter = 0;
			sm_BikeRearWheelDist    = 0.0f;
			sm_IsWheelingOnPushBike = false;			
			sm_TempBufferCounter    = 0;
		}
		//Front wheel on ground, so check for stoppie
		else
		{
			if(!rearWheelsTouching && !sm_IsOnAnotherVehicle)
			{
				if(frontWheelsTouching && (playerVehicle->GetFrameCollisionHistory()->GetMaxCollisionImpulseMagLastFrame() < 5.0f || playerVehicle->GetVelocity().Mag2() > 0.1f))
				{	
					sm_BikeFrontWheelCounter += (s32)fwTimer::GetSystemTimeStepInMilliseconds();
					sm_BikeFrontWheelDist += playerVehicle->GetDistanceTravelled();
					if(sm_TempBufferCounter > fwTimer::GetSystemTimeStepInMilliseconds() / 5)
						sm_TempBufferCounter -= fwTimer::GetSystemTimeStepInMilliseconds() / 5;
					else
						sm_TempBufferCounter = 0;
				}		
				else if(sm_BikeFrontWheelCounter > 0 && sm_TempBufferCounter < BIKE_ONEWHEEL_BUFFERLIMIT)
					sm_TempBufferCounter += fwTimer::GetSystemTimeStepInMilliseconds();
				else
				{
					if(sm_BikeFrontWheelCounter >= BIKE_FRONTWHEEL_MINBEST)
					{
						sm_BestBikeStoppieTimeMs = sm_BikeFrontWheelCounter;
						sm_BestBikeStoppieDistM  = sm_BikeFrontWheelDist;

						//Increment the stats:
						StatsInterface::SetGreater(STAT_LONGEST_STOPPIE_TIME.GetStatId(), (float)sm_BikeFrontWheelCounter);
						StatsInterface::SetGreater(STAT_LONGEST_STOPPIE_DIST.GetStatId(), sm_BikeFrontWheelDist);		
						statDebugf3("Player Stoppie for %d ms and %f m\n", sm_BestBikeStoppieTimeMs, sm_BestBikeStoppieDistM);
					}
					
					// Stoppies are too easy with push bikes, so don't count them towards challenge stats
					if (playerVehicle->InheritsFromBicycle() == false) 
					{
						UpdateRecordedStat(REC_STAT_LONGEST_STOPPIE_DIST, sm_BikeFrontWheelDist);
					}
					// reset values
					sm_BikeFrontWheelCounter = 0;
					sm_BikeFrontWheelDist    = 0.0f;
					sm_TempBufferCounter     = 0;
				}
			}
			else
			{
				if(sm_BikeFrontWheelCounter >= BIKE_FRONTWHEEL_MINBEST)
				{
					sm_BestBikeStoppieTimeMs = sm_BikeFrontWheelCounter;
					sm_BestBikeStoppieDistM  = sm_BikeFrontWheelDist;

					//Increment the stats:
					StatsInterface::SetGreater(STAT_LONGEST_STOPPIE_TIME.GetStatId(), (float)sm_BikeFrontWheelCounter);
					StatsInterface::SetGreater(STAT_LONGEST_STOPPIE_DIST.GetStatId(), sm_BikeFrontWheelDist);
					statDebugf3("Player Stoppie for %d ms and %f m\n", sm_BestBikeStoppieTimeMs, sm_BestBikeStoppieDistM);
				}

				// Stoppies are too easy with push bikes, so don't count them towards challenge stats
				if (playerVehicle->InheritsFromBicycle() == false) 
				{
					UpdateRecordedStat(REC_STAT_LONGEST_STOPPIE_DIST, sm_BikeFrontWheelDist);
				}				
				else
				{
					sm_IsDoingAStoppieOnPushBike = true;
                }

                if(sm_BikeFrontWheelCounter != 0.0f)
                {
                    SendStuntEvent(ST_STOPPIE, (float)sm_BikeFrontWheelCounter/1000.0f);
                }

				// reset values
				sm_BikeFrontWheelCounter = 0;
				sm_BikeFrontWheelDist    = 0.0f;
				sm_TempBufferCounter     = 0;
			}
		}

		// not in a car
		sm_CarTwoWheelCounter   = 0;
		sm_CarLess3WheelCounter = 0.0f;
	}
	else
	{
		sm_CarTwoWheelCounter    = 0;
		sm_CarLess3WheelCounter  = 0.0f;
		sm_BikeRearWheelCounter  = 0;
		sm_BikeFrontWheelCounter = 0;
		sm_TempBufferCounter     = 0;
	}
}

void  
CStatsMgr::UpdateVehicleStats(CVehicle* playerVehicle)
{
	statAssert(playerVehicle);
	if (!playerVehicle)
		return;

	if(sm_VehiclePreviousPosition.IsZero())
		sm_VehiclePreviousPosition = playerVehicle->GetPreviousPosition();

	Vec3V currentPosition = playerVehicle->GetTransform().GetPosition();

	const float distanceTravelled = Mag(currentPosition - VECTOR3_TO_VEC3V(sm_VehiclePreviousPosition)).Getf();

	const bool driverIsPlayer = (playerVehicle->GetDriver() && playerVehicle->GetDriver()->IsLocalPlayer());

	const float timeStepInMilliseconds = fwTimer::GetSystemTimeStep()*1000.0f;

	StatId statDist;
	StatId statDistDriving;
	StatId statTimeDriving;
	StatId statFastestSpeed;
	StatId statPassengerDistDriving;

	s32 vehType = playerVehicle->GetVehicleType();

	if (VEHICLE_TYPE_BLIMP == vehType)
		return;

	// For bike-style trikes, we want to track the stats as BIKE despite the actual type being QUADBIKE (for physics purposes)
	if (playerVehicle->IsTrike())
		vehType = VEHICLE_TYPE_BIKE;

	switch (vehType)
	{
	case VEHICLE_TYPE_CAR:
	case VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE:

        // CAREFUL! --> check case VEHICLE_TYPE_SUBMARINECAR when updating this case

		CStatsMgr::UpdateVehicleSpins(playerVehicle);
        CStatsMgr::UpdateVehicleFlips(playerVehicle);
        CStatsMgr::UpdateVehicleFlipsStunt(playerVehicle);
        CStatsMgr::UpdateVehicleRolls(playerVehicle);
		CStatsMgr::UpdateVehicleZeroWheel(playerVehicle);
        CStatsMgr::UpdateVehicleWheelie(playerVehicle);
        CStatsMgr::UpdateVehicleReverseDriving(playerVehicle);
        CStatsMgr::UpdateVehicleWallride(playerVehicle);
		statDist         = STAT_DIST_CAR;
		statFastestSpeed = STAT_FASTEST_SPEED;
		if(driverIsPlayer)
		{
			if (!playerVehicle->IsInAir())
			{
				sm_DriveNoCrashDist += distanceTravelled;
				sm_DriveNoCrashTime += timeStepInMilliseconds;
			}

			statDistDriving  = STAT_DIST_DRIVING_CAR;
			statTimeDriving  = STAT_TIME_DRIVING_CAR;

			CStatsMgr::UpdateAverageSpeed();
			UpdateRecordedStat(REC_STAT_LONGEST_DRIVE_NOCRASH, sm_DriveNoCrashDist);

			if(!sm_ChallengeZeroWheelPosStart.IsZero() && !sm_HadAttachedParentVehicle && !sm_WasOnAnotherVehicleWhenJumped)
			{
				Vector3 vehiclePosition = VEC3V_TO_VECTOR3(playerVehicle->GetTransform().GetPosition());
				vehiclePosition.z = 0.0f;
				Vector3 startPos = sm_ChallengeZeroWheelPosStart;
				startPos.z = 0;
				if(playerVehicle->GetVehicleModelInfo() && !playerVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_PARACHUTE) 
					&& playerVehicle->GetModelIndex() != MI_CAR_DELUXO && playerVehicle->GetModelIndex() != MI_BIKE_OPPRESSOR2) // hover vehicles
				{
					sm_JumpDistance = Vector3(vehiclePosition - startPos).Mag();
				}
			}
		}
		else if(!NetworkInterface::IsGameInProgress())
		{
			if (playerVehicle->IsTaxi())
			{
				statPassengerDistDriving = STAT_DIST_AS_PASSENGER_TAXI;
			}
		}
		break;

	case VEHICLE_TYPE_PLANE:
		CStatsMgr::IncrementStat(STAT_FLIGHT_TIME.GetStatId(), timeStepInMilliseconds, STATUPDATEFLAG_ASSERTONLINESTATS);
		statDist = STAT_DIST_PLANE;
		if(driverIsPlayer) 
		{
			statDistDriving = STAT_DIST_DRIVING_PLANE;
			statTimeDriving = STAT_TIME_DRIVING_PLANE;
			CStatsMgr::UpdatePlane(playerVehicle);
		}
		break;

	case VEHICLE_TYPE_QUADBIKE:
	case VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE:
		CStatsMgr::UpdateVehicleSpins(playerVehicle);
        CStatsMgr::UpdateVehicleFlips(playerVehicle);
        CStatsMgr::UpdateVehicleFlipsStunt(playerVehicle);
        CStatsMgr::UpdateVehicleRolls(playerVehicle);
		CStatsMgr::UpdateVehicleZeroWheel(playerVehicle);
		CStatsMgr::UpdateVehicleWheelie(playerVehicle);
        CStatsMgr::UpdateVehicleReverseDriving(playerVehicle);
        CStatsMgr::UpdateVehicleWallride(playerVehicle);
		statDist = STAT_DIST_QUADBIKE;
		statFastestSpeed = STAT_FASTEST_SPEED;

		if (driverIsPlayer)
		{
			if (!playerVehicle->IsInAir())
			{
				sm_DriveNoCrashDist += distanceTravelled;
				sm_DriveNoCrashTime += timeStepInMilliseconds;
			}
			UpdateRecordedStat(REC_STAT_LONGEST_DRIVE_NOCRASH, sm_DriveNoCrashDist);

			statDistDriving = STAT_DIST_DRIVING_QUADBIKE;
			statTimeDriving = STAT_TIME_DRIVING_QUADBIKE;
			CStatsMgr::UpdateAverageSpeed();
		}

		if(!sm_ChallengeZeroWheelPosStart.IsZero() && !sm_HadAttachedParentVehicle && !sm_WasOnAnotherVehicleWhenJumped)
		{
			Vector3 vehiclePosition = VEC3V_TO_VECTOR3(playerVehicle->GetTransform().GetPosition());
			vehiclePosition.z = 0.0f;
			Vector3 startPos = sm_ChallengeZeroWheelPosStart;
			startPos.z = 0;
			if (playerVehicle->GetVehicleModelInfo() && !playerVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_PARACHUTE) && playerVehicle->GetModelIndex() != MI_CAR_DELUXO && playerVehicle->GetModelIndex() != MI_BIKE_OPPRESSOR2)
			{
				sm_JumpDistance = Vector3(vehiclePosition - startPos).Mag();
			}
		}
		break;

	case VEHICLE_TYPE_HELI:
		CStatsMgr::IncrementStat(STAT_FLIGHT_TIME.GetStatId(), timeStepInMilliseconds, STATUPDATEFLAG_ASSERTONLINESTATS);
		statDist = STAT_DIST_HELI;
		if(driverIsPlayer) 
		{
			statDistDriving = STAT_DIST_DRIVING_HELI;
			statTimeDriving = STAT_TIME_DRIVING_HELI;
			CStatsMgr::UpdateHeli(playerVehicle);
		}
		break;
	
    case VEHICLE_TYPE_BIKE:
        CStatsMgr::UpdateVehicleSpins(playerVehicle);
        CStatsMgr::UpdateVehicleFlips(playerVehicle);
        CStatsMgr::UpdateVehicleFlipsStunt(playerVehicle);
        CStatsMgr::UpdateVehicleRolls(playerVehicle);
		CStatsMgr::UpdateVehicleZeroWheel(playerVehicle);
        CStatsMgr::UpdateVehicleWheelie(playerVehicle);
        CStatsMgr::UpdateVehicleWallride(playerVehicle);
		statDist = STAT_DIST_BIKE;
		statFastestSpeed = STAT_FASTEST_SPEED;

		if(driverIsPlayer)
		{
			if (!playerVehicle->IsInAir())
			{
				sm_DriveNoCrashDist += distanceTravelled;
				sm_DriveNoCrashTime += timeStepInMilliseconds;
			}
			statDistDriving = STAT_DIST_DRIVING_BIKE;
			statTimeDriving = STAT_TIME_DRIVING_BIKE;
			CStatsMgr::UpdateAverageSpeed();
			UpdateRecordedStat(REC_STAT_LONGEST_DRIVE_NOCRASH, sm_DriveNoCrashDist);
		}

		if(!sm_ChallengeZeroWheelPosStart.IsZero() && !sm_HadAttachedParentVehicle && !sm_WasOnAnotherVehicleWhenJumped)
		{
			Vector3 vehiclePosition = VEC3V_TO_VECTOR3(playerVehicle->GetTransform().GetPosition());
			vehiclePosition.z = 0.0f;
			Vector3 startPos = sm_ChallengeZeroWheelPosStart;
			startPos.z = 0;
			if(playerVehicle->GetVehicleModelInfo() && !playerVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_PARACHUTE) && playerVehicle->GetModelIndex() != MI_CAR_DELUXO && playerVehicle->GetModelIndex() != MI_BIKE_OPPRESSOR2)
			{
				sm_JumpDistance = Vector3(vehiclePosition - startPos).Mag();
			}
		}
		break;

	case VEHICLE_TYPE_BICYCLE:
		CStatsMgr::UpdateVehicleWheelie(playerVehicle);
		statDist = STAT_DIST_BICYCLE;
		if(driverIsPlayer)
		{
			statDistDriving = STAT_DIST_DRIVING_BICYCLE;
			statTimeDriving = STAT_TIME_DRIVING_BICYCLE;
		}
		break;
	
	case VEHICLE_TYPE_BOAT:
		CStatsMgr::UpdateVehicleZeroWheel(playerVehicle);
		statDist = STAT_DIST_BOAT;
		if(driverIsPlayer)
		{
			CStatsMgr::UpdateBoat(playerVehicle);
			statDistDriving = STAT_DIST_DRIVING_BOAT;
			statTimeDriving = STAT_TIME_DRIVING_BOAT;
		}
		break;

	case VEHICLE_TYPE_TRAIN:
		statAssert(!driverIsPlayer);
		break;

	case VEHICLE_TYPE_SUBMARINE:
		statDist = STAT_DIST_SUBMARINE;
		if(driverIsPlayer)
		{
			statDistDriving = STAT_DIST_DRIVING_SUBMARINE;
			statTimeDriving = STAT_TIME_DRIVING_SUBMARINE;
		}
		break;

	case VEHICLE_TYPE_SUBMARINECAR: 
		{
			CSubmarineCar* submarineCar = static_cast< CSubmarineCar* >( playerVehicle );
			
			if( submarineCar->IsInSubmarineMode() )
			{
				statDist = STAT_DIST_SUBMARINE;
				if(driverIsPlayer)
				{
					statDistDriving = STAT_DIST_DRIVING_SUBMARINE;
					statTimeDriving = STAT_TIME_DRIVING_SUBMARINE;
				}
			}
			else
			{
                CStatsMgr::UpdateVehicleSpins(playerVehicle);
                CStatsMgr::UpdateVehicleFlips(playerVehicle);
                CStatsMgr::UpdateVehicleFlipsStunt(playerVehicle);
                CStatsMgr::UpdateVehicleRolls(playerVehicle);
                CStatsMgr::UpdateVehicleZeroWheel(playerVehicle);
                CStatsMgr::UpdateVehicleWheelie(playerVehicle);
                CStatsMgr::UpdateVehicleReverseDriving(playerVehicle);
                CStatsMgr::UpdateVehicleWallride(playerVehicle);
				statDist         = STAT_DIST_CAR;
				statFastestSpeed = STAT_FASTEST_SPEED;
				if(driverIsPlayer)
				{
					if (!playerVehicle->IsInAir())
					{
						sm_DriveNoCrashDist += distanceTravelled;
						sm_DriveNoCrashTime += timeStepInMilliseconds;
					}
					statDistDriving  = STAT_DIST_DRIVING_CAR;
					statTimeDriving  = STAT_TIME_DRIVING_CAR;
					CStatsMgr::UpdateAverageSpeed();
					UpdateRecordedStat(REC_STAT_LONGEST_DRIVE_NOCRASH, sm_DriveNoCrashDist);
				}
				else if(!NetworkInterface::IsGameInProgress())
				{
					if (playerVehicle->IsTaxi())
					{
						statPassengerDistDriving = STAT_DIST_AS_PASSENGER_TAXI;
					}
				}
			}
		}
		break;

	case VEHICLE_TYPE_TRAILER:
	case VEHICLE_TYPE_AUTOGYRO:
	default:
		statAssert(0);
	}

	const CVehicleModelInfo* vi = playerVehicle->GetVehicleModelInfo();
	if (vi && driverIsPlayer)
	{
		if (!CanUpdateVehicleRecord(vi->GetHashKey()))
		{
			StartNewVehicleRecords(vi->GetHashKey());
		}

		if (statVerify(CanUpdateVehicleRecord(vi->GetHashKey())))
		{
			sm_vehicleRecords[sm_curVehIdx].m_LastDistDriven += distanceTravelled;
			sm_vehicleRecords[sm_curVehIdx].m_DistDriven     += distanceTravelled;
			sm_vehicleRecords[sm_curVehIdx].m_TimeDriven     += (u32)timeStepInMilliseconds;	//< loss of precision !!!1
			sm_vehicleRecords[sm_curVehIdx].m_Owned			  = playerVehicle->IsPersonalVehicle();

			if (!sm_vehicleRecords[sm_curVehIdx].m_HasSetNumDriven && sm_vehicleRecords[sm_curVehIdx].m_LastDistDriven>=CStatsVehicleUsage::VEHICLE_MIN_DRIVE_DIST)
			{
				sm_vehicleRecords[sm_curVehIdx].m_HasSetNumDriven = true; //Lock further.
				sm_vehicleRecords[sm_curVehIdx].m_NumDriven += 1;
			}
		}

		//Total Distance travelled
		if (NetworkInterface::IsGameInProgress() && StatsInterface::IsKeyValid(STAT_MP_CHAR_DIST_TRAVELLED))
		{
			StatsInterface::IncrementStat(STAT_MP_CHAR_DIST_TRAVELLED, distanceTravelled, STATUPDATEFLAG_ASSERTONLINESTATS);
		}

		//If we are driving a car then make sure we set the model has been driven.
		if (playerVehicle->InheritsFromAutomobile())
		{
			if (!CStatsMgr::VehicleHasBeenDriven(vi->GetHashKey(), NetworkInterface::IsGameInProgress()))
			{
				CStatsMgr::SetVehicleHasBeenDriven(vi->GetHashKey(), vi->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_COUNT_AS_FACEBOOK_DRIVEN));
				CStatsMgr::IncrementStat( NetworkInterface::IsGameInProgress() ? STAT_MP_VEHICLE_MODELS_DRIVEN : STAT_SP_VEHICLE_MODELS_DRIVEN, 1.0f, STATUPDATEFLAG_ASSERTONLINESTATS);
			}
		}
	}

	//If the player is Driving in cinematic camera
	const camCinematicDirector& cinematicDirector = camInterface::GetCinematicDirector();
	if (cinematicDirector.IsRenderingAnyInVehicleCinematicCamera())
	{
		sm_DriveInCinematic += timeStepInMilliseconds;
		StatsInterface::SetGreater(STAT_LONGEST_CAM_TIME_DRIVING.GetStatId(), sm_DriveInCinematic, STATUPDATEFLAG_ASSERTONLINESTATS);

		CPlayerSpecialAbilityManager::ChargeEvent(ACET_DRIVING_IN_CINEMATIC, FindPlayerPed());
	}
	else
	{
		StatsInterface::SetGreater(STAT_LONGEST_CAM_TIME_DRIVING.GetStatId(), sm_DriveInCinematic, STATUPDATEFLAG_ASSERTONLINESTATS);
		sm_DriveInCinematic = 0.0f;
	}

	if(statDist.IsValid())
	{
		StatsInterface::IncrementStat(statDist, distanceTravelled, STATUPDATEFLAG_ASSERTONLINESTATS);
	}

	if (statPassengerDistDriving.IsValid())
	{
		StatsInterface::IncrementStat(statPassengerDistDriving, distanceTravelled, STATUPDATEFLAG_ASSERTONLINESTATS);
	}

	if(driverIsPlayer && statDistDriving.IsValid() && statVerify(statTimeDriving.IsValid()))
	{
		StatsInterface::IncrementStat(statDistDriving, distanceTravelled, STATUPDATEFLAG_ASSERTONLINESTATS);
		StatsInterface::IncrementStat(statTimeDriving, timeStepInMilliseconds, STATUPDATEFLAG_ASSERTONLINESTATS);
	}

	//Set fastest speed
	const float NO_CONTACT_WHEELS_REST_TIME = 2000.0f;
	static float s_noContactWheelsTimer = 0.0f;

	if (!playerVehicle->HasContactWheels())
	{
		s_noContactWheelsTimer = 0.0f;
	}
	else if(s_noContactWheelsTimer < NO_CONTACT_WHEELS_REST_TIME)
	{
		s_noContactWheelsTimer += timeStepInMilliseconds;
	}

	if (driverIsPlayer)
	{
		const float speed = DotProduct(playerVehicle->GetVelocity(), VEC3V_TO_VECTOR3(playerVehicle->GetTransform().GetB())) * 3.6f; // km/h forward speed

		//statFastestSpeed is only updated for land vehicles... statAssert( playerVehicle->GetIsLandVehicle() );
		if (statFastestSpeed.IsValid() && playerVehicle->HasContactWheels() && s_noContactWheelsTimer >= NO_CONTACT_WHEELS_REST_TIME)
		{
			if (vi && CanUpdateVehicleRecord(vi->GetHashKey()))
			{
				if (speed > sm_vehicleRecords[sm_curVehIdx].m_HighestSpeed)
				{
					sm_vehicleRecords[sm_curVehIdx].m_HighestSpeed = speed;
				}
			}

			if (speed > StatsInterface::GetFloatStat(statFastestSpeed))
			{
				StatsInterface::SetStatData(STAT_TOP_SPEED_CAR.GetStatId(), vi->GetHashKey(), STATUPDATEFLAG_ASSERTONLINESTATS);
				StatsInterface::SetStatData(statFastestSpeed, speed, STATUPDATEFLAG_ASSERTONLINESTATS);
			}
		}

		if(!sm_HadAttachedParentVehicle && statFastestSpeed.IsValid() && playerVehicle->HasContactWheels())
		{
			sm_CurrentSpeed = speed;
			UpdateRecordedStat(REC_STAT_TOP_SPEED_CAR, sm_CurrentSpeed);
		}
	}

	sm_VehiclePreviousPosition = VEC3V_TO_VECTOR3( currentPosition );
}

void 
CStatsMgr::UpdateWeaponKill(const CPed* killer, const u32 weaponHash, const bool bHeadShot, const bool killedPlayer, const fwFlags32& flags, const CPed* victim)
{
	statAssertf(killer, "Killer is NULL");
	if (!killer || !killer->IsLocalPlayer())
		return;

	const CWeaponInfo* wi = CWeaponInfoManager::GetInfo<CWeaponInfo>(weaponHash);
	statAssertf(wi, "NULL weapon info for weapon hash=[0x%X]", weaponHash);
	if(!wi)
		return;

	//Update weapon kills
	if (killedPlayer)
	{
		StatId stat = StatsInterface::GetLocalPlayerWeaponInfoStatId(StatsInterface::WEAPON_STAT_ENEMY_KILLS, wi, killer);
		if (StatsInterface::IsKeyValid(stat))
		{
			StatsInterface::IncrementStat(stat, 1.0f);
		}
	}

	//Update kills done with that weapon
	StatId statWeaponKill = StatsInterface::GetLocalPlayerWeaponInfoStatId(StatsInterface::WEAPON_STAT_KILLS, wi, killer);
	if (StatsInterface::IsKeyValid(statWeaponKill))
	{
		StatsInterface::IncrementStat(statWeaponKill, 1.0f);
	}
#if __ASSERT
	else if(StatsInterface::GetStatsPlayerModelValid() && killer->IsArchetypeSet())
	{
		CStatsMgr::MissingWeaponStatName(STATTYPE_WEAPON_KILLS, statWeaponKill, wi->GetHash(), wi->GetDamageType(), wi->GetName());
	}
#endif // __ASSERT

	//Update Total ped kills
	StatsInterface::IncrementStat(STAT_KILLS.GetStatId(), 1.0f);

	StatId statKillsSinceLastCheckpoint = STAT_KILLS_SINCE_LAST_CHECKPOINT.GetStatId();
	if(StatsInterface::IsKeyValid(statKillsSinceLastCheckpoint))
	{
		StatsInterface::IncrementStat(statKillsSinceLastCheckpoint, 1);
		StatsInterface::IncrementStat(STAT_KILLS_SINCE_SAFEHOUSE_VISIT.GetStatId(), 1);
	}

	//Melee kills dont update more stats
	if (wi->GetIsMelee() || flags.IsFlagSet(CPedDamageCalculator::DF_MeleeDamage))
	{
		//Armed Kill
		if (weaponHash != WEAPONTYPE_UNARMED && !wi->GetIsUnarmed() && weaponHash != WEAPONTYPE_RAMMEDBYVEHICLE && weaponHash != WEAPONTYPE_RUNOVERBYVEHICLE)
		{
			StatsInterface::IncrementStat(STAT_KILLS_ARMED.GetStatId(), 1.0f);			
		}

		if(killedPlayer)
			UpdateRecordedStat(REC_STAT_MELEE_KILLED_PLAYERS, 1.0f);

		return;
	}

	//Update armed kills
	if (wi->GetIsGun())
	{
		StatsInterface::IncrementStat(STAT_KILLS_ARMED.GetStatId(), 1.0f);
		
		if(killedPlayer && wi->GetGroup() == WEAPONGROUP_SNIPER && wi->GetWeaponWheelSlot() == WEAPON_WHEEL_SLOT_SNIPER)
		{
			float distance = Mag(killer->GetTransform().GetPosition() - victim->GetTransform().GetPosition()).Getf();
			UpdateRecordedStat(REC_STAT_SNIPER_KILL_DISTANCE, distance);
			UpdateRecordedStat(REC_STAT_SNIPER_KILL, 1.0f);
		}

		// url:bugstar:2398298 - If we are in the melee VS challenge, weapon kills count as -1
		if(m_recordedStat==REC_STAT_MELEE_KILLED_PLAYERS && killedPlayer && m_recordedStatValue>0.0f)
			UpdateRecordedStat(REC_STAT_MELEE_KILLED_PLAYERS, -1.0f);
	}

	//Is player free aiming
	CPlayerInfo* pi = killer->GetPlayerInfo();
	if ((pi && pi->GetPlayerDataFreeAiming()) || killer->GetPlayerResetFlag(CPlayerResetFlags::PRF_RUN_AND_GUN))
	{
		StatsInterface::IncrementStat(STAT_KILLS_IN_FREE_AIM.GetStatId(), 1.0f);
	}

	CVehicle* pVehicle = killer->GetVehiclePedInside();
	const bool isTheDriver = pVehicle ? (killer == pVehicle->GetDriver()) : false;

	//Drive by kills
	if(pVehicle && wi->GetIsGun() && pVehicle->GetIsLandVehicle())
	{
		if (isTheDriver)
		{
			StatsInterface::IncrementStat(STAT_DB_KILLS.GetStatId(), 1.0f);
			if (killedPlayer)
			{
				StatsInterface::IncrementStat(STAT_DB_PLAYER_KILLS.GetStatId(), 1.0f);
			}
		}
		else
		{
			StatsInterface::IncrementStat(STAT_PASS_DB_KILLS.GetStatId(), 1.0f);
			if (killedPlayer)
			{
				StatsInterface::IncrementStat(STAT_PASS_DB_PLAYER_KILLS.GetStatId(), 1.0f);
			}
		}
		if(killedPlayer && !wi->GetIsVehicleWeapon())
		{
			UpdateRecordedStat(REC_STAT_DB_NON_TURRET_PLAYER_KILLS, 1.0f);
		}
	}

	//HeadShots with a gun
	if(bHeadShot && (wi->GetDamageType() == DAMAGE_TYPE_BULLET || wi->GetDamageType() == DAMAGE_TYPE_BULLET_RUBBER))
	{
		//Total number of weapon HeadShots
		StatId statWeapon = StatsInterface::GetLocalPlayerWeaponInfoStatId(StatsInterface::WEAPON_STAT_HEADSHOTS, wi, killer);
		if (StatsInterface::IsKeyValid(statWeapon))
		{
			StatsInterface::IncrementStat(statWeapon, 1.0f);
		}
#if __ASSERT
		else if(StatsInterface::GetStatsPlayerModelValid() && killer->IsArchetypeSet())
		{
			CStatsMgr::MissingWeaponStatName(STATTYPE_WEAPON_HEADSHOTS, statWeapon, wi->GetHash(), wi->GetDamageType(), wi->GetName());
		}
#endif // __ASSERT

		//Total number of HeadShots
		StatsInterface::IncrementStat(StatsInterface::GetStatsModelHashId("HEADSHOTS"), 1.0f);

		if (killedPlayer)
		{
			StatsInterface::IncrementStat(STAT_PLAYER_HEADSHOTS.GetStatId(), 1.0f);
			UpdateRecordedStat(REC_STAT_PLAYER_HEADSHOTS, 1.0f);
		}

		if(pVehicle)
		{
			if(isTheDriver)
			{
				StatsInterface::IncrementStat(STAT_DB_HEADSHOTS.GetStatId(), 1.0f);
			}
			else
			{
				StatsInterface::IncrementStat(STAT_PASS_DB_HEADSHOTS.GetStatId(), 1.0f);
			}
		}
	}
}

void 
CStatsMgr::RegisterPlayerKilled(const CPed* killer, const CPed* killed, const u32 weaponHash, const int weaponDamageComponent, const bool withMeleeWeapon)
{
	//A Player has been killed
	CNetworkTelemetry::PlayerDied(killed, killer, weaponHash, weaponDamageComponent, withMeleeWeapon);

	statAssertf(killed, "Null victim.");
	if (!killed)
		return;

	//We only want to update the bellow stats for the local player
	if (!killed->IsLocalPlayer())
		return;

	const CWeaponInfo* wi = CWeaponInfoManager::GetInfo<CWeaponInfo>(weaponHash);
	statAssertf(wi, "NULL weapon info for weapon hash=[0x%X]", weaponHash);
	if(!wi)
		return;

	//Make sure we clear cheat tracking.
	SetCheatIsActive( false );

	//Total number of deaths
	StatsInterface::IncrementStat(STAT_DEATHS.GetStatId(), 1.0f);

	if (!NetworkInterface::IsGameInProgress() && CTheScripts::GetPlayerIsOnAMission())
	{
		StatsInterface::IncrementStat(STAT_DIED_IN_MISSION.GetStatId(), 1.0f);
	}

	//Total number of deaths done by other players
	if (killer && killer != killed && killer->IsPlayer() && !killer->IsLocalPlayer())
	{
		if (ShouldCountKills())
		{
			StatsInterface::IncrementStat(STAT_MP_DEATHS_PLAYER, 1.0f);
			StatsInterface::IncrementStat(STAT_MPPLY_DEATHS_PLAYERS_CHEATER, 1.0f);

			if (StatsInterface::IsKeyValid(STAT_DEATHS_PLAYER.GetHash()))
			{
				StatsInterface::IncrementStat(STAT_DEATHS_PLAYER.GetStatId(), 1.0f);
			}
		}
	}

	sm_ZeroWheelPosHeight.Zero();
	sm_GroundProbeHelper.ResetProbe();
	ClearPlayerVehicleStats();

	EndVehicleStats();

	sm_CrashedVehicleTimer             = 0;
	sm_CrashedVehicleKeyHash           = 0;
	sm_CrashedVehicleDamageAccumulator = 0.0f;	
	sm_NearMissNoCrash				   = 0;
	sm_NearMissNoCrashPrecise		   = 0;
	sm_JumpDistance					   = 0.0f;	
	sm_VehicleBailDistance			   = 0.0f;
	sm_VehicleDeadCheckCounter		   = 0;
	sm_VehicleDeadCheck				   = false;

	sm_StartVehicleBail.Zero();
	sm_LastVehicleBailPosition.Zero();	
	sm_HadAttachedParentVehicle = false;	
	sm_HasStartedParachuteDeploy = false;
	sm_hasBailedFromParachuting = false;
	sm_ParachuteStartTime = 0;
	sm_FlyingAltitude = 0.0f;
	sm_ValidFlyingAltitude = false;
	sm_CurrentSpeed = 0.0f;

	ClearNearMissArray();
	if (killer)
	{
		StatId stat = StatsInterface::GetWeaponInfoHashId("DEATHS", wi, killer);
		if (StatsInterface::IsKeyValid(stat))
		{
			StatsInterface::IncrementStat(stat, 1.0f);
		}
#if __ASSERT
		else if (killed->IsArchetypeSet() && StatsInterface::GetStatsPlayerModelValid())
		{
			statWarningf("Specific Weapon %s doesnt have a valid stat name for DEATHS.", wi->GetName());
		}
#endif // __ASSERT
	}

	eDamageType damageType = wi->GetDamageType();

	if (WEAPONTYPE_DROWNING.GetHash() == weaponHash)
	{
		StatsInterface::IncrementStat(StatsInterface::GetStatsModelHashId("DIED_IN_DROWNING"), 1);
	}
	else if (WEAPONTYPE_DROWNINGINVEHICLE.GetHash() == weaponHash)
	{
		StatsInterface::IncrementStat(StatsInterface::GetStatsModelHashId("DIED_IN_DROWNING"), 1);
		StatsInterface::IncrementStat(StatsInterface::GetStatsModelHashId("DIED_IN_DROWNINGINVEHICLE"), 1);
	}
	else if (damageType == DAMAGE_TYPE_EXPLOSIVE || WEAPONTYPE_EXPLOSION.GetHash() == weaponHash)
	{
		StatsInterface::IncrementStat(StatsInterface::GetStatsModelHashId("DIED_IN_EXPLOSION"), 1);
	}
	else if (damageType == DAMAGE_TYPE_FALL || WEAPONTYPE_FALL.GetHash() == weaponHash)
	{
		StatsInterface::IncrementStat(StatsInterface::GetStatsModelHashId("DIED_IN_FALL"), 1);
	}
	else if (damageType == DAMAGE_TYPE_FIRE || WEAPONTYPE_FIRE.GetHash() == weaponHash)
	{
		StatsInterface::IncrementStat(StatsInterface::GetStatsModelHashId("DIED_IN_FIRE"), 1);
	}
	else if (WEAPONTYPE_RAMMEDBYVEHICLE.GetHash() == weaponHash || WEAPONTYPE_RUNOVERBYVEHICLE.GetHash() == weaponHash)
	{
		StatsInterface::IncrementStat(StatsInterface::GetStatsModelHashId("DIED_IN_ROAD"), 1);
	}
#if !__FINAL
	else if (WEAPONTYPE_BUZZARD.GetHash() == weaponHash)
	{
		//Do nothing
	}
	else if (WEAPONTYPE_TANK.GetHash() == weaponHash)
	{
		//Do nothing
	}
	else if (WEAPONTYPE_ELECTRICFENCE.GetHash() == weaponHash)
	{
		//Do nothing
	}
	else
	{
		statWarningf("Specific death by Weapon %s DEATH is not being recorded.", wi->GetName());
	}
#endif // !__FINAL
}

void 
CStatsMgr::UpdateVehicleFlips(CVehicle* playerVehicle)
{
    statAssert(playerVehicle);
    if (!playerVehicle)
        return;
    else if (!playerVehicle->GetDriver())
        return;
    else if (!playerVehicle->GetDriver()->IsLocalPlayer())
        return;

    const u32 CRASHED_VEHICLE_FLIPS_INTERVAL = 3*1000;

    // Is it valid to update the spinning stats?
    if(playerVehicle->HasContactWheels() || (fwTimer::GetTimeInMilliseconds_NonScaledClipped() - sm_CrashedVehicleTimer) < CRASHED_VEHICLE_FLIPS_INTERVAL)
    {
        sm_VehicleFlipsAccumulator = 0;
        return;
    }

    Vector3 vCarUp = VEC3V_TO_VECTOR3(playerVehicle->GetTransform().GetC());

    if (!sm_VehicleFlipsCounter)
    {
        if (vCarUp.z >= -VEHICLE_UPSIDEDOWN_UPZ_THRESHOLD)
        {
            sm_VehicleFlipsCounter = true;
        }

        return;
    }

    if (vCarUp.z > VEHICLE_UPSIDEDOWN_UPZ_THRESHOLD)
    {
        return;
    }

    const CVehicleModelInfo* vi = playerVehicle->GetVehicleModelInfo();
    if (statVerify( vi ) && statVerify( CanUpdateVehicleRecord(vi->GetHashKey()) ))
    {
        ++sm_vehicleRecords[sm_curVehIdx].m_NumFlips;
    }

    sm_VehicleFlipsAccumulator += 1;
    CStatsMgr::SetGreater(STAT_MOST_FLIPS_IN_ONE_JUMP.GetStatId(), (float)sm_VehicleFlipsAccumulator);

    if(m_statRecordingPolicy==RSP_GREATEST || m_statRecordingPolicy==RSP_LOWEST)
        UpdateRecordedStat(REC_STAT_MOST_FLIPS_IN_ONE_JUMP, (float) sm_VehicleFlipsAccumulator);
    if(m_statRecordingPolicy==RSP_SUM)
        UpdateRecordedStat(REC_STAT_MOST_FLIPS_IN_ONE_JUMP, 1.0f);
    statDebugf3("Flips accumulator %d", sm_VehicleFlipsAccumulator);

    sm_VehicleFlipsCounter = false;
}


void 
CStatsMgr::UpdateVehicleFlipsStunt(CVehicle* playerVehicle)
{
    statAssert(playerVehicle);
    if (!playerVehicle)
        return;
    else if (!playerVehicle->GetDriver())
        return;
    else if (!playerVehicle->GetDriver()->IsLocalPlayer())
        return;

    const u32 CRASHED_VEHICLE_FLIPS_INTERVAL = 3*1000;

    const u32 FLIP_DELAY_BUFFER = 500; // We wait 500ms before sending the event for a flip, in case we crash right after landing
    static u32 s_flipDelayTimer = 0;
    
    if(s_flipDelayTimer && s_flipDelayTimer + FLIP_DELAY_BUFFER <= fwTimer::GetSystemTimeInMilliseconds())
    {
        if(s_landedFlip)
        {
            int flips = (int) rage::round(Abs<float>(s_delayedFlipCount/TWO_PI));
            if( flips > 0)
            {
                statDebugf3("Landed flip and didnt crash after delay, sending event : value = %f (%d)", s_delayedFlipCount, flips);
                SendStuntEvent( (s_delayedFlipCount <= 0.0f) ? ST_FRONTFLIP : ST_BACKFLIP , (float) flips);
            }
        }
        statDebugf3("Reset delay timer");
        s_flipDelayTimer = 0;
        s_delayedFlipCount = 0.0f;
        sm_VehicleFlipsHeadingLast = 0.0f;
    }

    // Is it valid to update the spinning stats?
    if(playerVehicle->HasContactWheels() || (fwTimer::GetTimeInMilliseconds_NonScaledClipped() - sm_CrashedVehicleTimer) < CRASHED_VEHICLE_FLIPS_INTERVAL)
    {
        if(!s_landedFlip)
        {
            s_landedFlip = true;
        }
        if(s_delayedFlipCount != 0.0f && s_flipDelayTimer == 0)
        {
            s_flipDelayTimer = fwTimer::GetSystemTimeInMilliseconds();
            statDebugf3("Landed, FlipCount = %f, starting flip delay", s_delayedFlipCount);
        }
        return;
    }

    if(!s_flipDelayTimer)
    {
        s_landedFlip = false;
    }

    // Flips
    float fFlip           = playerVehicle->GetTransform().GetPitch();
    float fFlipDifference = fFlip - sm_VehicleFlipsHeadingLast;


    // Assume wrap around if spun more than 1 rad in a single frame
    if(Abs<float>(fFlipDifference) > 1.0f)
    {
        fFlipDifference = 0.0f;
    }

    s_delayedFlipCount += fFlipDifference;
    sm_VehicleFlipsHeadingLast = fFlip;
    statDebugf3("Flip accum %.2f = %i flips", s_delayedFlipCount, (int)(s_delayedFlipCount/TWO_PI));
}

void 
    CStatsMgr::UpdateVehicleSpins(CVehicle* playerVehicle)
{
    statAssert(playerVehicle);
    if (!playerVehicle)
        return;
    else if (!playerVehicle->GetDriver())
        return;
    else if (!playerVehicle->GetDriver()->IsLocalPlayer())
        return;

    const u32 CRASHED_VEHICLE_SPIN_INTERVAL = 3*1000;

    // Is it valid to update the spinning stats?
    if(playerVehicle->HasContactWheels() || (fwTimer::GetTimeInMilliseconds_NonScaledClipped() - sm_CrashedVehicleTimer) < CRASHED_VEHICLE_SPIN_INTERVAL)
    {
        sm_VehicleSpinsAccumulator = rage::Abs(sm_VehicleSpinsAccumulator);

        if (sm_VehicleSpinsAccumulator > 0.0f)
        {
            const CVehicleModelInfo* vi = playerVehicle->GetVehicleModelInfo();
            if (statVerify( vi ) && statVerify( CanUpdateVehicleRecord(vi->GetHashKey()) ))
            {
                sm_vehicleRecords[sm_curVehIdx].m_NumSpins += (u32)(sm_VehicleSpinsAccumulator/TWO_PI);
            }
        }
        
        if(sm_VehicleSpinsAccumulator != 0.0f)
        {
            // We are on the ground
            if(sm_VehicleSpinsAccumulator >=  PI)
            {
                SendStuntEvent(ST_SPIN, (float)(sm_VehicleSpinsAccumulator / TWO_PI));
            }
            UpdateRecordedStat(REC_STAT_MOST_SPINS_IN_ONE_JUMP, sm_VehicleSpinsAccumulator / TWO_PI);
            sm_VehicleSpinsAccumulator = 0.0f;
            sm_VehicleSpinsHeadingLast = 0.0f;
        }
        return;
    }
    else if(Abs(playerVehicle->GetTransform().GetB().GetZf()) > VEHICLE_FRONT_THRESHOLD)
    {
        // We are vertical: don't reset the accumulator but don't add to it either
        return;
    }

    statDebugf3("Sping heading accum %.2f = %i spins", sm_VehicleSpinsAccumulator, (int)(sm_VehicleSpinsAccumulator/TWO_PI));

    float fHeading           = playerVehicle->GetTransform().GetHeading();
    float fHeadingDifference = 0.0f;
    if(sm_VehicleSpinsHeadingLast != 0.0f)
    {
        fHeadingDifference = fHeading - sm_VehicleSpinsHeadingLast;
    }
    sm_VehicleSpinsHeadingLast = fHeading;

    // Assume wrap around if spun more than 180 deg
    if(fHeadingDifference > PI)
    {
        fHeadingDifference = TWO_PI - fHeadingDifference;
    }
    if(fHeadingDifference < -PI)
    {
        fHeadingDifference = TWO_PI + fHeadingDifference;
    }
    sm_VehicleSpinsAccumulator += fHeadingDifference;

	// url:bugstar:3159843 - Wheels dont touch the ground, but we're in water with an amphibious vehicle
	if(playerVehicle->GetIsInWater() && (VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE == playerVehicle->GetVehicleType() || VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE == playerVehicle->GetVehicleType()) )
    {
		statWarningf("Ignore STAT_MOST_SPINS_IN_ONE_JUMP as vehicle is in the water");
		sm_VehicleSpinsAccumulator = 0.0f; // Reset the accumulator so it doesnt update the stat if we jump on a wave
		return;
	}

	//url:bugstar:4033108 - Dont allow this with the flying car
	if(playerVehicle->GetModelIndex() == MI_CAR_DELUXO || playerVehicle->GetModelIndex() == MI_BIKE_OPPRESSOR2)
	{
		statWarningf("Ignore MI_CAR_DELUXO or MI_BIKE_OPPRESSOR2 as vehicle for spins");
		sm_VehicleSpinsAccumulator = 0.0f;
		return;
	}

	CStatsMgr::SetGreater(STAT_MOST_SPINS_IN_ONE_JUMP.GetStatId(), (sm_VehicleSpinsAccumulator / TWO_PI));

}

void 
CStatsMgr::UpdateVehicleRolls(CVehicle* playerVehicle)
{
    statAssert(playerVehicle);
    if (!playerVehicle)
        return;
    else if (!playerVehicle->GetDriver())
        return;
    else if (!playerVehicle->GetDriver()->IsLocalPlayer())
        return;

    const u32 CRASHED_VEHICLE_SPIN_INTERVAL = 3*1000;

    if(playerVehicle->HasContactWheels() || (fwTimer::GetTimeInMilliseconds_NonScaledClipped() - sm_CrashedVehicleTimer) < CRASHED_VEHICLE_SPIN_INTERVAL)
    {
        // We are on the ground
        if(sm_VehicleRollsAccumulator != 0.0f)
        {
            int rolls = (int) rage::round(Abs<float>(sm_VehicleRollsAccumulator/TWO_PI));
            if( rolls > 0)
            {
                SendStuntEvent(ST_ROLL, (float) rolls);
            }
            sm_VehicleRollsAccumulator = 0.0f;
            sm_VehicleRollsHeadingLast = 0.0f;
        }
        return;
    }

    statDebugf3("Roll heading accum %.2f = %i rolls", sm_VehicleRollsAccumulator, (int)(sm_VehicleRollsAccumulator/TWO_PI));

    // Barrel rolls
    float fRoll           = playerVehicle->GetTransform().GetRoll();
    float fRollDifference = fRoll - sm_VehicleRollsHeadingLast;
    sm_VehicleRollsHeadingLast = fRoll;

    // Assume wrap around if spun more than 180 deg
    if(fRollDifference > PI)
    {
        fRollDifference = TWO_PI - fRollDifference;
    }
    if(fRollDifference < -PI)
    {
        fRollDifference = TWO_PI + fRollDifference;
    }

    sm_VehicleRollsAccumulator += fRollDifference;
}

bool
CStatsMgr::IsAboveOcean(const Vector3& position)
{
	bool isAboveOcean = false;

	float waterZ = -1.0f;
	if (Water::GetWaterLevelNoWaves(position, &waterZ, POOL_DEPTH, 999999.9f, NULL))
	{
		phSegment		seg;
		phIntersection	intersection;
		seg.Set(position, Vector3(position.x, position.y, -999999.9f));
		intersection.Reset();

		if (CPhysics::GetLevel()->TestProbe(seg, &intersection, NULL, ArchetypeFlags::GTA_MAP_TYPE_MOVER))
		{
			if (waterZ > intersection.GetPosition().GetZf())
			{
				const float minZ  = CGameWorldHeightMap::GetMinHeightFromWorldHeightMap(position.x, position.y);
				const float maxZ  = CGameWorldHeightMap::GetMaxHeightFromWorldHeightMap(position.x, position.y);

				if(waterZ == 0.0f && minZ == 0.0f)
				{
					//We are for sure in a Ocean
					if (IsClose(minZ, maxZ, 0.1f))
					{
						isAboveOcean = true;
					}
					else if (audShoreLineOcean::GetClosestShore())
					{
						isAboveOcean = (audShoreLineOcean::IsListenerOverOcean() && audShoreLineOcean::GetSqdDistanceIntoWater() < 1000.0f);
					}
				}
			}
		}
		else
		{
			isAboveOcean = true;  // no ground here, we're far off shore
		}
	}

	return isAboveOcean;
}

bool CStatsMgr::IsPlayerVehicleAboveOcean()
{
	CPed* player = FindPlayerPed();
	if(statVerify(player))
	{
		CVehicle* playerVehicle = player->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle) ? player->GetMyVehicle() : NULL;
		if(statVerify(playerVehicle))
		{
			Vec3V coords = playerVehicle->GetTransform().GetPosition();

			const Vec2V position = Vec2V(VEC3V_TO_VECTOR3(coords).x, VEC3V_TO_VECTOR3(coords).y);
			return IsAboveForbiddenArea(position) ||  IsAboveOcean(VEC3V_TO_VECTOR3(coords));
		}
	}
	return false;
}

bool CStatsMgr::IsAboveForbiddenArea(const Vec2V& position)
{
	for(int i = 0; i < sm_TuneMetadata.m_nonFlyableAreas.m_areas.size(); i++)
	{
		const NonFlyableArea& area = sm_TuneMetadata.m_nonFlyableAreas.m_areas[i];
		// simple inside rectangle check
		bool contains = area.m_rectXYWH.x < position.GetXf() 
			&& area.m_rectXYWH.x + area.m_rectXYWH.w > position.GetXf()
			&& area.m_rectXYWH.y < position.GetYf() 
			&& area.m_rectXYWH.y + area.m_rectXYWH.z > position.GetYf();
		if(contains)
		{
			// we dont want to update the challenges
			statDebugf3("Above an area excluded from the flying challenges");
			sm_FlyingCounter = 0.0f;
			sm_FlyingDistance = 0.0f;
			return true;
		}
	}
	return false;
}

void 
CStatsMgr::UpdateFlyingVehicle(CVehicle* playerVehicle)
{
	statAssert(playerVehicle);
	if (!playerVehicle)
		return;
	else if (!playerVehicle->GetDriver())
		return;
	else if (!playerVehicle->GetDriver()->IsLocalPlayer())
		return;

	bool isInAir = playerVehicle->IsInAir();
	if(playerVehicle->GetFrameCollisionHistory()->GetMaxCollisionImpulseMagLastFrame() > 0.0f)
	{
		// we collided with something and it's not an object (then has to be the ground or a building)
		const CCollisionRecord *pCollRecord = playerVehicle->GetFrameCollisionHistory()->GetMostSignificantCollisionRecordOfType(ENTITY_TYPE_OBJECT);
		if(!pCollRecord)
		{
			isInAir = false;
		}
	}

	Vec3V coords = playerVehicle->GetTransform().GetPosition();
	const Vec2V position = Vec2V(VEC3V_TO_VECTOR3(coords).x, VEC3V_TO_VECTOR3(coords).y);
	
	if(IsAboveForbiddenArea(position))
		return;

	if(isInAir)
	{
		sm_drivingFlyingVehicle = true;
		sm_InAirTimer += fwTimer::GetTimeStepInMilliseconds();

		if (!sm_GroundProbeHelper.IsProbeActive())
		{
			if (sm_FlyingPlanePosHeight.IsZero())
			{
				sm_FlyingPlanePosHeight = VEC3V_TO_VECTOR3(coords);
			}
		}

		sm_InAirTimer += fwTimer::GetTimeStepInMilliseconds();

		if(m_recordedStat == REC_STAT_FLIGHT_TIME_BELOW_20 || m_recordedStat == REC_STAT_FLIGHT_DIST_BELOW_20)
		{
			if(sm_ValidFlyingAltitude && sm_FlyingAltitude < 20.0f && !IsAboveOcean(VEC3V_TO_VECTOR3(coords)))  // Under 20m and not above ocean
			{
				if(playerVehicle->GetVelocity().Mag2() > MIN_PLANE_SPEED_TO_CONSIDER_MOVEMENT)
				{
					sm_FlyingCounter += fwTimer::GetSystemTimeStepInMilliseconds();
					UpdateRecordedStat(REC_STAT_FLIGHT_TIME_BELOW_20, sm_FlyingCounter);
				}

				if(!sm_LastFlyingPlanePosition.IsZero())
				{
					Vec3V position = coords;
					position.SetZf(0.0f);
					Vec3V lastPosition = VECTOR3_TO_VEC3V(sm_LastFlyingPlanePosition);
					lastPosition.SetZf(0.0f);
					sm_FlyingDistance += Mag(position - lastPosition).Getf();
				}

				UpdateRecordedStat(REC_STAT_FLIGHT_DIST_BELOW_20, sm_FlyingDistance);
			}
			else
			{
				sm_FlyingCounter = 0.0f;
				sm_FlyingDistance = 0.0f;
			}
		}
		if(m_recordedStat == REC_STAT_FLIGHT_TIME_BELOW_100 || m_recordedStat == REC_STAT_FLIGHT_DIST_BELOW_100)
		{
			if(sm_ValidFlyingAltitude && sm_FlyingAltitude < 100.0f && !IsAboveOcean(VEC3V_TO_VECTOR3(coords)))
			{
				Matrix34 planeMtx = MAT34V_TO_MATRIX34(playerVehicle->GetTransformPtr()->GetMatrix());
				Vector3 planeUp = planeMtx.c;

				if (planeUp.Dot(Vector3(0,0,1)) < CStatsMgr::minInvertFlightDot)
				{
					// Plane is on the back
					if(playerVehicle->GetVelocity().Mag2() > MIN_SPEED_TO_CONSIDER_MOVEMENT)
					{
						sm_FlyingCounter += fwTimer::GetSystemTimeStepInMilliseconds();
						UpdateRecordedStat(REC_STAT_FLIGHT_TIME_BELOW_100, sm_FlyingCounter);
					}

					float distanceThisFrame = 0.0f;
					if(!sm_LastFlyingPlanePosition.IsZero())
					{
						Vec3V position = coords;
						position.SetZf(0.0f);
						Vec3V lastPosition = VECTOR3_TO_VEC3V(sm_LastFlyingPlanePosition);
						lastPosition.SetZf(0.0f);
						distanceThisFrame = Mag(position - lastPosition).Getf();
						sm_FlyingDistance += distanceThisFrame;
					}
					if(m_statRecordingPolicy == RSP_SUM)
					{
						UpdateRecordedStat(REC_STAT_FLIGHT_DIST_BELOW_100, distanceThisFrame);
					}
					else
					{
						UpdateRecordedStat(REC_STAT_FLIGHT_DIST_BELOW_100, sm_FlyingDistance);
					}
				}
				else
				{
					sm_FlyingCounter = 0.0f;
					sm_FlyingDistance = 0.0f;
				}
			}
			else
			{
				sm_FlyingCounter = 0.0f;
				sm_FlyingDistance = 0.0f;
			}
		}

		sm_LastFlyingPlanePosition = VEC3V_TO_VECTOR3(playerVehicle->GetTransform().GetPosition());
	}
	else
	{
		sm_PlaneRollAccumulator = 0.0f;
		sm_PlaneRollLast = 0.0f;
		sm_FlyingCounter = 0.0f;
		sm_FlyingDistance = 0.0f;
		sm_FlyingAltitude = 0.0f;
		sm_ValidFlyingAltitude = false;
		sm_drivingFlyingVehicle = false;
		sm_LastFlyingPlanePosition.Zero();
	}
}



void 
CStatsMgr::UpdatePlane(CVehicle* playerVehicle)
{
	statAssert(playerVehicle);
	if (!playerVehicle)
		return;
	else if (!playerVehicle->GetDriver())
		return;
	else if (!playerVehicle->GetDriver()->IsLocalPlayer())
		return;

	// Landing
	static dev_u32 snMinLandingTime = 30000;
	static dev_u32 snMinCollTimePassed = 2000;
	if(!playerVehicle->IsInAir() || playerVehicle->GetFrameCollisionHistory()->GetMaxCollisionImpulseMagLastFrame() > 0.0f)
	{
		if(sm_InAirTimer > snMinLandingTime)
		{
			sm_LandingTimer = sm_InAirTimer = snMinLandingTime;
		}

		u32 nMinTimeSinceLastCollision = playerVehicle->GetFrameCollisionHistory()->GetMostRecentCollisionTime() + snMinCollTimePassed;
		if(sm_LandingTimer > 0 && playerVehicle->GetNumContactWheels() == playerVehicle->GetNumWheels() && 
		   fwTimer::GetTimeInMilliseconds() > nMinTimeSinceLastCollision)
		{
			sm_LandingTimer = 0;
			sm_InAirTimer = 0;
			CStatsMgr::IncrementStat(StatsInterface::GetStatsModelHashId("PLANE_LANDINGS"), 1.0f);

			const CVehicleModelInfo* vi = playerVehicle->GetVehicleModelInfo();
			if (statVerify( vi ) && statVerify( CanUpdateVehicleRecord(vi->GetHashKey()) ))
			{
				++sm_vehicleRecords[sm_curVehIdx].m_NumPlaneLandings;
			}
		}
		else
		{
			sm_LandingTimer -= fwTimer::GetTimeStepInMilliseconds();
			if(sm_LandingTimer < 0)
			{
				sm_LandingTimer = 0;
				sm_InAirTimer = 0;
			}
		}
	}

	UpdateFlyingVehicle(playerVehicle);
	
	if(playerVehicle->IsInAir())
	{
		// Barrel rolls
		float fRoll           = playerVehicle->GetTransform().GetRoll();
		float fRollDifference = fRoll - sm_PlaneRollLast;
		sm_PlaneRollLast = fRoll;

		// Assume wrap around if spun more than 180 deg
		if(fRollDifference > PI)
		{
			fRollDifference = TWO_PI - fRollDifference;
		}
		if(fRollDifference < -PI)
		{
			fRollDifference = TWO_PI + fRollDifference;
		}

		sm_PlaneRollAccumulator += fRollDifference;
		if(rage::Abs(sm_PlaneRollAccumulator) >= TWO_PI)
		{
			sm_PlaneBarrelRollCounter+=1.0f;
			sm_PlaneRollAccumulator=0.0f;
		}
		UpdateRecordedStat(REC_STAT_PLANE_BARREL_ROLLS, sm_PlaneBarrelRollCounter);
	}
}

void 
CStatsMgr::UpdateHeli(CVehicle* playerVehicle)
{
	statAssert(playerVehicle);
	if (!playerVehicle)
		return;
	else if (!playerVehicle->GetDriver())
		return;
	else if (!playerVehicle->GetDriver()->IsLocalPlayer())
		return;

	UpdateFlyingVehicle(playerVehicle);
}

void 
CStatsMgr::PlayerEnteredVehicle(const CVehicle* vehicle)
{
	if (vehicle)
	{
		const u16 seed = vehicle->GetRandomSeed();
		if (sm_LastVehicleStolenForChallenge == seed)
		{
			UpdateRecordedStat(REC_STAT_NUMBER_STOLEN_VEHICLE, 1.0f);
			sm_LastVehicleStolenForChallenge=0;
		}
		sm_curVehIdx = INVALID_RECORD;

		const CVehicleModelInfo* vi = vehicle->GetVehicleModelInfo();
		if (statVerify( vi ))
		{
			StartNewVehicleRecords( vi->GetHashKey() );
			if (statVerify( CanUpdateVehicleRecord( vi->GetHashKey() ) ))
			{
				sm_vehicleRecords[sm_curVehIdx].m_LastDistDriven = 0.0f;
				sm_vehicleRecords[sm_curVehIdx].m_LastTimeSpentInVehicle = sysTimer::GetSystemMsTime();
			}
		}
	}
}

void 
CStatsMgr::PlayerLeftVehicle(const CVehicle* vehicle)
{
	if (vehicle)
	{
		const CVehicleModelInfo* vi = vehicle->GetVehicleModelInfo();
		if (statVerify( vi ))
		{
			const u32 idx = FindVehicleRecord( vi->GetHashKey() );
			if (idx<CStatsMgr::INVALID_RECORD)
				sm_vehicleRecords[idx].EndVehicleStats();
		}
	}

	sm_CurrentSpeed = 0.0f;
}

bool
CStatsMgr::UpdateVehiclesStolen(const CVehicle* vehicle)
{
	if (!IsPlayerActive())
		return false;

	if (!statVerify(vehicle))
		return false;

	const u16 seed = vehicle->GetRandomSeed();

	if (sm_LastVehicleStolenSeed == seed)
		return false;

	sm_LastVehicleStolenSeed = seed;

	// Don't increment NUMBER_STOLEN_COP_VEHICLE if the cop vehicle is a player's personal vehicle (unless in the CNC mode).
	if (vehicle->IsLawEnforcementVehicle() && (!const_cast<CVehicle*>(vehicle)->IsPersonalVehicle() || NetworkInterface::IsInCopsAndCrooks()))
	{
		StatsInterface::IncrementStat(StatsInterface::GetStatsModelHashId("NUMBER_STOLEN_COP_VEHICLE"), 1.0f);
	}

	const s32 type = vehicle->GetVehicleType();
	bool countsAsVehicleStolen=false;
	switch(type)
	{
	case VEHICLE_TYPE_CAR:        
	case VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE:        
		StatsInterface::IncrementStat(StatsInterface::GetStatsModelHashId("NUMBER_STOLEN_CARS"), 1.0f);      countsAsVehicleStolen=true; break;
	case VEHICLE_TYPE_PLANE:      StatsInterface::IncrementStat(StatsInterface::GetStatsModelHashId("NUMBER_STOLEN_PLANES"), 1.0f);    countsAsVehicleStolen=true; break;
	case VEHICLE_TYPE_TRAILER:    StatsInterface::IncrementStat(StatsInterface::GetStatsModelHashId("NUMBER_STOLEN_TRAILERS"), 1.0f);  countsAsVehicleStolen=true; break;
	case VEHICLE_TYPE_QUADBIKE:   
	case VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE:   
			StatsInterface::IncrementStat(StatsInterface::GetStatsModelHashId("NUMBER_STOLEN_QUADBIKES"), 1.0f); countsAsVehicleStolen=true; break;
	case VEHICLE_TYPE_HELI:       StatsInterface::IncrementStat(StatsInterface::GetStatsModelHashId("NUMBER_STOLEN_HELIS"), 1.0f);     countsAsVehicleStolen=true; break;
	case VEHICLE_TYPE_BIKE:       StatsInterface::IncrementStat(StatsInterface::GetStatsModelHashId("NUMBER_STOLEN_BIKES"), 1.0f);     countsAsVehicleStolen=true; break;
	case VEHICLE_TYPE_BICYCLE:    StatsInterface::IncrementStat(StatsInterface::GetStatsModelHashId("NUMBER_STOLEN_BICYCLES"), 1.0f);  countsAsVehicleStolen=true; break;
	case VEHICLE_TYPE_BOAT:       StatsInterface::IncrementStat(StatsInterface::GetStatsModelHashId("NUMBER_STOLEN_BOATS"), 1.0f);     countsAsVehicleStolen=true; break;

	case VEHICLE_TYPE_AUTOGYRO:
	case VEHICLE_TYPE_SUBMARINE:
	case VEHICLE_TYPE_TRAIN:
		statWarningf("Vehicle %d has been stolen but hasn't been incremented", type);
		break;

	default:
		statErrorf("Unknown vehicle type \"id=%d : type=%d\" - CStatsMgr::UpdateVehiclesStolen() ", seed, type);
		break;
	}

	const CVehicleModelInfo* vi = vehicle->GetVehicleModelInfo();
	if (statVerify( vi ))
	{
		StartNewVehicleRecords( vi->GetHashKey() );
		if (statVerify( CanUpdateVehicleRecord( vi->GetHashKey() ) ))
		{
			++sm_vehicleRecords[sm_curVehIdx].m_NumStolen;
		}
	}
	if(countsAsVehicleStolen)
		sm_LastVehicleStolenForChallenge = seed;

	return true;
}

void 
CStatsMgr::EndVehicleStats( )
{
	if (sm_WasInVehicle)
	{
		sm_VehicleDeadCheckCounter = 0;
		sm_VehicleDeadCheck = true;

		sm_WasInVehicle = false;

		//If the player is Driving in cinematic camera
		if (sm_DriveInCinematic > 0.0f)
		{
			StatsInterface::SetGreater(STAT_LONGEST_CAM_TIME_DRIVING.GetStatId(), sm_DriveInCinematic);
		}

		sm_ZeroWheelPosHeight.Zero();
		sm_GroundProbeHelper.ResetProbe();

		CheckCrashDamageAccumulator( true );

		ClearPlayerVehicleStats();

		//End current checks.
		sm_curVehIdx = CStatsMgr::INVALID_RECORD;
		for (int i=0; i<sm_vehicleRecords.GetCount(); i++)
			sm_vehicleRecords[i].EndVehicleStats();

		//Check for stale records.
		for (int i=0; i<sm_vehicleRecords.GetCount(); i++)
		{
			if (sm_vehicleRecords[i].CanBeCleared())
			{
				sm_vehicleRecords[i].Clear(true);

				for (int j=i; j<sm_vehicleRecords.GetCount()-1; j++)
				{
					sm_vehicleRecords[j] = sm_vehicleRecords[j+1];
					sm_vehicleRecords[j+1].Clear(true);
				}

				sm_vehicleRecords.Resize(sm_vehicleRecords.GetCount()-1);
			}
		}

		sm_VehiclePreviousPosition.Zero();
		sm_ReverseDrivingPosCurrent.Zero();
		sm_CurrentDrivingReverseDistance=0.0f;
	}

	//Check for player dying and invalidate some stats.
	if (sm_VehicleDeadCheck)
	{
		sm_VehicleDeadCheckCounter += fwTimer::GetSystemTimeStep() * 1000.0f;
		if (sm_VehicleDeadCheckCounter > DEAD_CHECK_INTERVAL)
		{
			sm_VehicleDeadCheck = false;
			sm_VehicleDeadCheckCounter = 0;

			CPed* player = FindPlayerPed();

			//Player is not dead
			if (player && !player->IsDead())
			{
				//We are Tracking the longest drive without a crash
				//Check if we are driving or not...
				if (sm_DriveNoCrashTime > 0)
				{
					StatsInterface::SetGreater(STAT_LONGEST_DRIVE_NOCRASH.GetStatId(), sm_DriveNoCrashDist);
				}
			}

			sm_DriveNoCrashTime = 0.0f;
			sm_DriveNoCrashDist = 0.0f;
			sm_NearMissNoCrash = 0;
			sm_JumpDistance=0.0f;
			sm_HadAttachedParentVehicle=false;
			sm_NearMissNoCrashPrecise = 0;
			ClearNearMissArray();
		}
	}
}

static const int FOOTBALL_HIT_HISTORY_SIZE = 15;
static fwEntity* s_LastHitFootballs[FOOTBALL_HIT_HISTORY_SIZE] = {0};
static int s_lastHitFoolballIndex = 0;

void
CStatsMgr::CheckCrashDamageAccumulator( const bool force )
{

    CPed* playerPed = FindPlayerPed();
    if (playerPed)
    {
        CVehicle* playerVehicle = playerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle) ? playerPed->GetMyVehicle() : NULL;
        if(playerVehicle)
        {
            const CCollisionRecord* colRecord = (playerVehicle->GetFrameCollisionHistory()) ? playerVehicle->GetFrameCollisionHistory()->GetFirstObjectCollisionRecord() : nullptr;
            if(colRecord)
            {
				static const atHashString PIN_MODEL_HASH = ATSTRINGHASH("stt_prop_stunt_bowling_pin", 0x5779131A);
				static const atHashString FOOTBALL_MODEL_HASH = ATSTRINGHASH("stt_prop_stunt_soccer_lball", 0xDB2C3E38);
				static const atHashString FOOTBALL_MODEL_HASH_2 = ATSTRINGHASH("stt_prop_stunt_soccer_ball", 0xC00C3530);
				static const atHashString FOOTBALL_MODEL_HASH_3 = ATSTRINGHASH("stt_prop_stunt_soccer_sball", 0x286F5F0);

                u32 hash = (colRecord->m_pRegdCollisionEntity.Get() && colRecord->m_pRegdCollisionEntity.Get()->GetArchetype()) ? colRecord->m_pRegdCollisionEntity.Get()->GetArchetype()->GetModelNameHash() : 0;
                if(hash == PIN_MODEL_HASH.GetHash())
                {
                    static fwEntity* s_lastKnockedDownPin = nullptr;
                    fwEntity* bowlingPin = colRecord->m_pRegdCollisionEntity.Get();
                    if(s_lastKnockedDownPin != bowlingPin)
                    {
                        Vector3 v = VEC3V_TO_VECTOR3(bowlingPin->GetTransform().GetUp());
                        Vector3 up(0,1,0);
                        static const float STANDING_PIN_THRESHOLD = 0.1f;
                        float dotProduct = v.Dot(up);
                        if(v.z > 0.0f && Abs(dotProduct) <= STANDING_PIN_THRESHOLD) // the pin is standing
                        {
                            statDebugf3("Crash into a bowling pin : up=(%f, %f, %f) - Dot = %f", v.x, v.y, v.z, dotProduct);
                            SendStuntEvent(ST_BOWLING_PIN, 1.0f);
                            s_lastKnockedDownPin = bowlingPin;
                        }
                        else
                        {
                            statDebugf3("Crash into a bowling pin but it's not standing : up=(%f, %f, %f) - Dot = %f", v.x, v.y, v.z, dotProduct);
                        }
                    }
                }
				else if(hash == FOOTBALL_MODEL_HASH.GetHash() || hash == FOOTBALL_MODEL_HASH_2.GetHash() || hash == FOOTBALL_MODEL_HASH_3.GetHash())
                {
                    fwEntity* football = colRecord->m_pRegdCollisionEntity.Get();
                    bool alreadyHit = false;
                    for(int i=0; i<FOOTBALL_HIT_HISTORY_SIZE && !alreadyHit; i++)
                    {
                        if(football == s_LastHitFootballs[i])
                        {
                            alreadyHit = true;
                            statDebugf3("Football already hit");
                            break;
                        }
                    }
                    if(!alreadyHit)
                    {
                        statDebugf3("Crash into a football");
                        SendStuntEvent(ST_FOOTBALL, 1.0f);
                        s_LastHitFootballs[s_lastHitFoolballIndex] = football;
                        s_lastHitFoolballIndex++;
                        if(s_lastHitFoolballIndex >= FOOTBALL_HIT_HISTORY_SIZE)
                        {
                            s_lastHitFoolballIndex = 0;
                        }
                    }
                }
				else
				{
					if (s_isTrackingStunts)
					{
						statDebugf3("Collided with '%s', ignored.", atHashString(hash).TryGetCStr());
					}
				}
            }
        }
    }

	if (sm_CrashedVehicleTimer > 0 && (sm_CrashedVehicleTimer+CRASHED_VEHICLE_INTERVAL <= sysTimer::GetSystemMsTime() || force))
	{
		if (sm_CrashedVehicleKeyHash > 0 && sm_CrashedVehicleDamageAccumulator > 0.0f)
		{
            float largeAccidenThresold = sm_TuneMetadata.m_SPLargeAccidenThresold;
            if (NetworkInterface::IsGameInProgress())
            {
                largeAccidenThresold = sm_TuneMetadata.m_MPLargeAccidenThresold;
            }

            // We crashed, reset flips count
            if(s_delayedFlipCount > 0.0f && largeAccidenThresold <= sm_CrashedVehicleDamageAccumulator)
            {
                s_delayedFlipCount = 0.0f;
                statDebugf3("Crashed above large accident threshold, resetting flip count");
            }
            s_landedFlip = true;


            CPed* playerPed = FindPlayerPed();
            if (playerPed)
            {
                CVehicle* playerVehicle = playerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle) ? playerPed->GetMyVehicle() : NULL;
                if(playerVehicle)
                {
                    // Check if we are landing on our wheels (more or less) or not
                    Vector3 v = VEC3V_TO_VECTOR3(playerVehicle->GetTransform().GetUp());
                    Vector3 up(0,1,0);
                    static const float FLIP_LANDING_THRESHOLD = 0.5f;
                    float dotProduct = v.Dot(up);
                    if(v.z > 0.0f && Abs(dotProduct) <= FLIP_LANDING_THRESHOLD) 
                    {
                        statDebugf3("Crashed correctly - dotProduct = %f     - v=(%f,  %f, %f)", dotProduct, v.x, v.y, v.z);
                    }
                    else
                    {
                        statDebugf3("Crashed not flat - dotProduct = %f     - v=(%f,  %f, %f)", dotProduct, v.x, v.y, v.z);
                        s_delayedFlipCount = 0.0f;
                    }
                }
            }

			fwModelId modelId;
			CBaseModelInfo* mi = CModelInfo::GetBaseModelInfoFromHashKey(sm_CrashedVehicleKeyHash, &modelId);
			if (mi)
			{
				statDebugf3("CheckCrashDamageAccumulator Vehicle crash - \"%s : %u\" , Accumulator=%f", mi->GetModelName(), sm_CrashedVehicleKeyHash, sm_CrashedVehicleDamageAccumulator);

				CVehicleModelInfo* vmi = static_cast< CVehicleModelInfo* >( mi );

				switch(vmi->GetVehicleType())
				{
				case VEHICLE_TYPE_CAR:
				case VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE:
					CStatsMgr::IncrementStat(STAT_TOTAL_DAMAGE_CARS.GetStatId(), sm_CrashedVehicleDamageAccumulator);
					StatsInterface::IncrementStat(STAT_NUMBER_CRASHES_CARS.GetStatId(), 1.0f);
					if (largeAccidenThresold <= sm_CrashedVehicleDamageAccumulator)
					{
						StatsInterface::IncrementStat(STAT_LARGE_ACCIDENTS.GetStatId(), 1.0f);

						u32 vehIdx = FindVehicleRecord( sm_CrashedVehicleKeyHash );
						if (statVerifyf( vehIdx != INVALID_RECORD, "No record found for vehicle %u", sm_CrashedVehicleKeyHash) && statVerify(sm_vehicleRecords[vehIdx].IsValid()))
						{
							if (StatsInterface::GetStatsModelPrefix() == sm_vehicleRecords[vehIdx].m_CharacterId)
							{
								sm_vehicleRecords[vehIdx].m_NumLargeAccidents += 1;
							}
						}
					}
					break;
				case VEHICLE_TYPE_QUADBIKE:
				case VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE:
					CStatsMgr::IncrementStat(STAT_TOTAL_DAMAGE_QUADBIKES.GetStatId(), sm_CrashedVehicleDamageAccumulator);
					StatsInterface::IncrementStat(STAT_NUMBER_CRASHES_QUADBIKES.GetStatId(), 1.0f);
					break;
				case VEHICLE_TYPE_BIKE:
					CStatsMgr::IncrementStat(STAT_TOTAL_DAMAGE_BIKES.GetStatId(), sm_CrashedVehicleDamageAccumulator);
					StatsInterface::IncrementStat(STAT_NUMBER_CRASHES_BIKES.GetStatId(), 1.0f);
					break;

				case VEHICLE_TYPE_BICYCLE:
				case VEHICLE_TYPE_AUTOGYRO:
				case VEHICLE_TYPE_TRAILER:
				case VEHICLE_TYPE_HELI:
				case VEHICLE_TYPE_PLANE:
				case VEHICLE_TYPE_BOAT:
				case VEHICLE_TYPE_SUBMARINE:
				case VEHICLE_TYPE_TRAIN:
					break;

				default:
					statErrorf("Unknown vehicle type \"id=%s : type=%d\" - CStatsMgr::UpdateVehiclesStolen() ", vmi->GetGameName(), vmi->GetVehicleType());
					break;
				}
			}
		}

		sm_CrashedVehicleTimer             = 0;
		sm_CrashedVehicleKeyHash           = 0;
		sm_CrashedVehicleDamageAccumulator = 0.0f;
	}
}

void 
CStatsMgr::ClearPlayerVehicleStats()
{	
	sm_BestCarTwoWheelsTimeMs    = 0;
	sm_BestCarTwoWheelsDistM     = 0.0f;
	sm_BestBikeWheelieTimeMs     = 0;
	sm_BestBikeWheelieDistM      = 0.0f;
	sm_BestBikeStoppieTimeMs     = 0;
	sm_BestBikeStoppieDistM      = 0.0f;
	sm_BikeRearWheelCounter      = 0;
	sm_BikeRearWheelDist         = 0.0f;
	sm_BikeFrontWheelCounter     = 0;
	sm_BikeFrontWheelDist        = 0.0f;
	sm_CarTwoWheelCounter        = 0;
	sm_CarTwoWheelDist           = 0.0f;
	sm_CarLess3WheelCounter      = 0.0f;
	sm_DriveNoCrashDist          = 0.0f;
	sm_DriveNoCrashTime          = 0.0f;
	sm_DriveInCinematic          = 0.0f;	
	sm_FlyingAltitude	         = 0.0f;
	sm_IsWheelingOnPushBike      = false;
	sm_IsDoingAStoppieOnPushBike = false;
	sm_InAirTimer			     = 0;
	sm_invalidJump			     = false;		
	sm_TempBufferCounter         = 0;
	sm_LandingTimer			     = 0;
	sm_ZeroWheelPosStart.Zero();
	sm_ZeroWheelPosEnd.Zero();
	sm_ChallengeZeroWheelPosStart.Zero();
	sm_VehiclePreviousPosition.Zero();
	sm_ValidFlyingAltitude	     = false;
	sm_VehicleFlipsCounter       = false;
    sm_VehicleFlipsAccumulator   = 0;
    sm_VehicleSpinsAccumulator   = 0.0f;
    sm_VehicleSpinsHeadingLast   = 0.0f;
    sm_VehicleRollsAccumulator   = 0.0f;
    sm_VehicleRollsHeadingLast   = 0.0f;
	sm_ZeroWheelCounter          = 0;
	sm_ZeroWheelBufferCounter    = 0;
    s_delayedFlipCount           = 0.0f;
    sm_VehicleFlipsHeadingLast = 0.0f;
}

void 
CStatsMgr::StartVehicleGroundProbe()
{
	//A probe is already active
	if (sm_GroundProbeHelper.IsProbeActive())
		return;

	sm_GroundProbeHelper.ResetProbe();

	sm_drivingFlyingVehicleForProbe = sm_drivingFlyingVehicle;

	// Dont probe if the challenge is not running
	if(sm_drivingFlyingVehicleForProbe && 
		( m_recordedStat != REC_STAT_FLIGHT_TIME_BELOW_20 &&
		m_recordedStat != REC_STAT_FLIGHT_TIME_BELOW_100 &&
		m_recordedStat != REC_STAT_FLIGHT_DIST_BELOW_20 &&
		m_recordedStat != REC_STAT_FLIGHT_DIST_BELOW_100 )
	  )
	{
		return;
	}
	
	//Too soon to start another probe
	if (!sm_drivingFlyingVehicleForProbe && sm_TimeSinceLastGroundProbe + TIME_BETWEEN_GROUND_PROBES >= fwTimer::GetTimeInMilliseconds())
		return;

	//Start the probe.
	if (sm_ZeroWheelCounter == 0 && !sm_ZeroWheelPosHeight.IsZero())
	{
		statVerify( CStatsMgr::StartGroundProbe(sm_ZeroWheelPosHeight) );
	}
	//Start the probe for planes
	if (sm_drivingFlyingVehicleForProbe)
	{
		statDebugf3("StartVehicleGroundProbe : sm_FlyingPlanePosHeight <x=%f,y=%f,z=%f> ", sm_FlyingPlanePosHeight.x, sm_FlyingPlanePosHeight.y, sm_FlyingPlanePosHeight.z);
		if (!sm_FlyingPlanePosHeight.IsZero())
		{
			Vector3 startPos = sm_FlyingPlanePosHeight;
			startPos.z = CGameWorldHeightMap::GetMaxHeightFromWorldHeightMap(sm_FlyingPlanePosHeight.x, sm_FlyingPlanePosHeight.y);
			statVerify( CStatsMgr::StartGroundProbe(startPos) );
		}
	}
}

void 
CStatsMgr::CheckVehicleGroundProbeResults()
{
	//No probe is in progress
	if (!sm_GroundProbeHelper.IsProbeActive())
		return;

	//No valid probe position 
	if(sm_ZeroWheelPosHeight.IsZero() && sm_FlyingPlanePosHeight.IsZero())
	{
		//Something is wrong and the Ground Probe must be Reset.
		sm_GroundProbeHelper.ResetProbe();
		return;
	}

	//Check the probe
	Vector3 probeStartPos = (sm_drivingFlyingVehicleForProbe) ? sm_FlyingPlanePosHeight : sm_ZeroWheelPosHeight;
	if(sm_drivingFlyingVehicleForProbe)
	{
		// We start the probe from the position of the plane, or the max heightmap altitude if closer to the ground
		float maxHeightMap = CGameWorldHeightMap::GetMaxHeightFromWorldHeightMap(sm_FlyingPlanePosHeight.x, sm_FlyingPlanePosHeight.y);
		statDebugf3("Plane Position when starting probe Coords=<x=%f,y=%f,z=%f>   maxHeightmap=%f", sm_FlyingPlanePosHeight.x, sm_FlyingPlanePosHeight.y, sm_FlyingPlanePosHeight.z, maxHeightMap);
		probeStartPos.z = (maxHeightMap>sm_FlyingPlanePosHeight.z) ? sm_FlyingPlanePosHeight.z : maxHeightMap;
	}

	float altitude = 0.0f;
	if (CheckGroundProbeResults(altitude, probeStartPos))
	{
		if(sm_drivingFlyingVehicleForProbe)
		{
			// Probe was started from max heightmap or plane position, compute the real altitude now
			float heightmapz = CGameWorldHeightMap::GetMaxHeightFromWorldHeightMap(sm_FlyingPlanePosHeight.x, sm_FlyingPlanePosHeight.y);

			if(heightmapz<sm_FlyingPlanePosHeight.z)
				sm_FlyingAltitude = altitude+sm_FlyingPlanePosHeight.z-heightmapz;
			else
				sm_FlyingAltitude = altitude;

			if(sm_FlyingAltitude < 0.0f)  // We crashed too hard in the ground
				sm_FlyingAltitude = 0.0f;

			sm_ValidFlyingAltitude = true;
			statDebugf3("Plane Ground Probe Results: Start Coords=<x=%f,y=%f,z=%f> altitude=<%f>", probeStartPos.x, probeStartPos.y, probeStartPos.z, sm_FlyingAltitude);
		}
		else
		{
			if(!sm_invalidJump)
			{
				statDebugf3("Vehicle Ground Probe Results: Start Coords=<x=%f,y=%f,z=%f> altitude=<%f>", probeStartPos.x, probeStartPos.y, probeStartPos.z, altitude);

				CStatsMgr::SetGreater(STAT_HIGHEST_JUMP_REACHED.GetStatId(), altitude);
				UpdateRecordedStat(REC_STAT_HIGHEST_JUMP_REACHED, altitude);
			}
		}
	}

	//Probe has ended reset shyte

	sm_GroundProbeHelper.ResetProbe();
	sm_FlyingPlanePosHeight.Zero();
	sm_ZeroWheelPosHeight.Zero();
	sm_ZeroWheelPosStart.Zero();
	sm_ZeroWheelPosEnd.Zero();
	sm_ChallengeZeroWheelPosStart.Zero();

	sm_invalidJump = false;
}
void 
CStatsMgr::CheckParachuteResults(CPed* playerPed)
{
	//Not doing parachute probing
	if (0 == sm_ParachuteStartTime)
		return;

	//Check always freefall 1st
	if (sm_WasInAir)
		return;

	if (statVerify(playerPed) && !sm_ParachuteStartPos.IsZero() && !sm_ParachuteDeployPos.IsZero())
	{
		//Disable tracking for landing on water and if the player is dead.
		if (!playerPed->GetIsSwimming() && !playerPed->IsInjured())
		{
			Vec3V coords = playerPed->GetTransform().GetPosition();
			statDebugf3("[Parachute] CheckParachuteResults : player position: X:%f, Y:%f, Z:%f", coords.GetXf(), coords.GetYf(), coords.GetZf());

			u32 parachutetotalTime = fwTimer::GetSystemTimeInMilliseconds() - sm_ParachuteStartTime;

			//Altitude from parachuting start
			float altitude = sm_ParachuteStartPos.z - coords.GetZf();

			if (NetworkInterface::IsGameInProgress())
			{
				if (altitude >= sm_TuneMetadata.m_AwardParachuteJumpDistanceB && parachutetotalTime >= sm_TuneMetadata.m_AwardParachuteJumpTime)
				{
					StatsInterface::IncrementStat(STAT_AWD_PARACHUTE_JUMPS_50M.GetStatId(), 1.0f);
				}
			}

			if(StatsInterface::IsKeyValid(STAT_LONGEST_SKYDIVE.GetStatId()))
			{
				altitude = sm_ParachuteStartPos.z - sm_ParachuteDeployPos.z;
				CStatsMgr::SetGreater(STAT_LONGEST_SKYDIVE.GetStatId(), altitude);
			}

			//Altitude from parachute deployment
			if (NetworkInterface::IsGameInProgress())
			{
				altitude = sm_ParachuteDeployPos.z - coords.GetZf();
				if(altitude > 0.0f)
				{
					// if deploy position was over the ground
					if(CGameWorldHeightMap::GetMinHeightFromWorldHeightMap(sm_ParachuteDeployPos.x, sm_ParachuteDeployPos.y) < sm_ParachuteDeployPos.z)
					{
						if (altitude < sm_TuneMetadata.m_AwardParachuteJumpDistanceA)
						{
							StatsInterface::IncrementStat(STAT_AWD_PARACHUTE_JUMPS_10M.GetStatId(), 1.0f);
						}
						statDebugf3("[Parachute] CheckParachuteResults : parachute opened %f meters above the ground", altitude);
						UpdateRecordedStat(REC_STAT_LOWEST_PARACHUTE_OPEN, altitude);
					}
				}
			}
		}
	}

	sm_ParachuteStartTime = 0;
	sm_ParachuteStartPos.Zero();
	sm_ParachuteDeployPos.Zero();
	sm_HasStartedParachuteDeploy = false;
}

void 
CStatsMgr::CheckFreeFallResults(CPed* playerPed)
{
	if (!sm_WasInAir)
		return;

	statDebugf3("CheckFreeFallResults player position: X:%f, Y:%f, Z:%f", playerPed->GetTransform().GetPosition().GetXf(), playerPed->GetTransform().GetPosition().GetYf(), playerPed->GetTransform().GetPosition().GetZf());

	if (statVerify(playerPed) && !playerPed->IsInjured() && !sm_FreeFallStartPos.IsZero())
	{
		if ( (sm_ParachuteDeployPos.IsZero() && !sm_HasStartedParachuteDeploy) || sm_hasBailedFromParachuting )
		{
			statDebugf3("CheckFreeFallResults Altitude = %f", sm_FreeFallAltitude);

			StatsInterface::SetGreater(StatsInterface::GetStatsModelHashId("LONGEST_SURVIVED_FREEFALL"), sm_FreeFallAltitude);
			if(!playerPed->GetIsInWater())
			{
				UpdateRecordedStat(REC_STAT_LONGEST_SURVIVED_FREEFALL, sm_FreeFallAltitude);
			}
			sm_FreeFallAltitude = 0.0f;
		}
	}

	if(m_recordedStat == REC_STAT_LONGEST_SURVIVED_SKYDIVE && !playerPed->IsInjured() && !playerPed->GetIsInWater())
	{
		UpdateRecordedStat(REC_STAT_LONGEST_SURVIVED_SKYDIVE, sm_SkydiveDistance);
	}
	sm_SkydiveDistance = 0.0f;

	sm_FreeFallStartPos.Zero();
	sm_ChallengeSkydiveStartPos.Zero();
	sm_WasInAir = false;
	sm_FreeFallStartTimer = 0;
	sm_FreeFallDueToPlayerDamage = false;
	sm_MostRecentCollisionTimeWithPlaneInAir = 0;
	sm_hasBailedFromParachuting = false;
}

bool  
CStatsMgr::StartGroundProbe(Vector3& groundProbePosStart)
{
	statAssert(!sm_GroundProbeHelper.IsProbeActive());
	if(!sm_drivingFlyingVehicle)
	{
		statAssert(sm_TimeSinceLastGroundProbe + TIME_BETWEEN_GROUND_PROBES < fwTimer::GetTimeInMilliseconds());
	}

	Vector3 vEnd = groundProbePosStart;
	vEnd.z = CGameWorldHeightMap::GetMinHeightFromWorldHeightMap(vEnd.x, vEnd.y);

	float waterZ = 0.0f;
	if(CVfxHelper::GetWaterZ(VECTOR3_TO_VEC3V(sm_FlyingPlanePosHeight), waterZ) != WATERTEST_TYPE_NONE)
	{
		if(waterZ > vEnd.z)
		{
			vEnd.z = waterZ;
		}
	}

	if(groundProbePosStart.z < vEnd.z)
	{
		statWarningf("Ground Probe: Cannot probe from under the ground/water");
		groundProbePosStart.z = vEnd.z;
	}

	//Create and fire the probe
	WorldProbe::CShapeTestProbeDesc probeData;
	probeData.SetStartAndEnd(groundProbePosStart, vEnd);
	probeData.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);

	bool result = false;

	if (sm_GroundProbeHelper.StartTestLineOfSight(probeData))
	{
		statDebugf3("Ground Probe: Start at coords=<x=%f,y=%f,z=%f>", groundProbePosStart.x, groundProbePosStart.y, groundProbePosStart.z);
		statDebugf3("Ground Probe: End at coords=<x=%f,y=%f,z=%f>", vEnd.x, vEnd.y, vEnd.z);

		sm_TimeSinceLastGroundProbe = fwTimer::GetTimeInMilliseconds();
		result = true;
	}
	else
	{
		statErrorf("Ground Probe: Failed to Start at coords=<x=%f,y=%f,z=%f>", groundProbePosStart.x, groundProbePosStart.y, groundProbePosStart.z);
		statDebugf3("Ground Probe: Failed End at coords=<x=%f,y=%f,z=%f>", vEnd.x, vEnd.y, vEnd.z);
	}

	return result;
}

bool 
CStatsMgr::CheckGroundProbeResults(float& altitude, Vector3& groundProbePosStart)
{
	statAssert(sm_GroundProbeHelper.IsProbeActive());

	//Check the probe
	ProbeStatus probeStatus;
	Vector3 intersectionPos;
	if (sm_GroundProbeHelper.GetProbeResultsWithIntersection(probeStatus, &intersectionPos))
	{
		//Probe is done calculating
		if (probeStatus == PS_Blocked)
		{
			altitude = groundProbePosStart.z - intersectionPos.z;
		}
		else
		{
			statWarningf("Ground Probe: Failed End Probe. probeStatus= '%d'.", probeStatus);

			float fWaterHeight = 0.0f;
			if(CWaterTestHelper::GetWaterHeightAtPositionIncludingRivers(groundProbePosStart, &fWaterHeight))
			{
				altitude = groundProbePosStart.z - fWaterHeight;
				statWarningf("Ground Probe: Failed End Probe. Falling back to water level: %.2f", fWaterHeight);
			}
			else
			{
				statWarningf("Ground Probe: Failed End Probe. Not above water, return false");
				return false;
			}
		}

        sm_LastJumpHeight = altitude;
		//Another check can be done in 3 seconds.
		sm_TimeSinceLastGroundProbe = fwTimer::GetTimeInMilliseconds();

		return true;
	}

	return false;
}

void 
CStatsMgr::UpdateVehicleZeroWheel(CVehicle* pVehicle)
{
	statAssert(pVehicle);
	if (!pVehicle)
		return;
	else if (!pVehicle->GetDriver())
		return;
	else if (!pVehicle->GetDriver()->IsLocalPlayer())
		return;

	const s32  vehicleType      = pVehicle->GetVehicleType();
	const bool validVehicleType = (VEHICLE_TYPE_CAR==vehicleType || VEHICLE_TYPE_BIKE==vehicleType || VEHICLE_TYPE_QUADBIKE==vehicleType || VEHICLE_TYPE_BICYCLE==vehicleType);

	// for the challenges, consider landing into water as landing.
	if(pVehicle->GetWasInWater() && !sm_ChallengeZeroWheelPosStart.IsZero())
	{
		if(!sm_invalidJump)
		{
			// B*3150624 - Dont track this for vehicles that have a parachute
			if(!pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_PARACHUTE) && pVehicle->GetModelIndex() != MI_CAR_DELUXO && pVehicle->GetModelIndex() != MI_BIKE_OPPRESSOR2)
			{
				UpdateRecordedStat(REC_STAT_FARTHEST_JUMP_DIST, sm_JumpDistance);
			}
		}

		sm_JumpDistance = 0.0f;
		sm_ChallengeZeroWheelPosStart.Zero();
		sm_HadAttachedParentVehicle = false;
		sm_invalidJump = false;
	}

	bool collidingFragment = false;
	if(statVerify(pVehicle->GetFrameCollisionHistory()))
	{
		const CCollisionRecord *pCollRecord = pVehicle->GetFrameCollisionHistory()->GetMostSignificantCollisionRecordOfType(ENTITY_TYPE_OBJECT);
		if (pCollRecord && pCollRecord->m_pRegdCollisionEntity.Get())
		{
			CEntity* entity = pCollRecord->m_pRegdCollisionEntity.Get();
			if(entity && entity->GetCurrentPhysicsInst() && entity->GetCurrentPhysicsInst()->GetClassType()==PH_INST_FRAG_CACHE_OBJECT)
			{
				collidingFragment = true;
			}
		}
	}

	if (!sm_GroundProbeHelper.IsProbeActive() 
		&& validVehicleType 
		&& !pVehicle->HasContactWheels() 
		&& ( pVehicle->GetFrameCollisionHistory()->GetMaxCollisionImpulseMagLastFrame()==0.0f || collidingFragment) )
	{
		const float speed = pVehicle->GetVelocity().Mag() * 3.6f; // km/h vehicle velocity

		//Check if this is a glitch
		if (speed < MIN_SPEED_TO_CONSIDER_JUMP)
		{
			statDebugf1("Invalid speed to consider Zero Wheel '%f'.", speed);
			sm_invalidJump = true;
		}

		if(collidingFragment)
			statDebugf3("Was only colliding with a fragment when jumping");

		sm_ZeroWheelBufferCounter = 0;
		sm_ZeroWheelCounter += fwTimer::GetSystemTimeStepInMilliseconds();

		Vec3V coords = pVehicle->GetTransform().GetPosition();
		if (coords.GetZf() > sm_ZeroWheelPosHeight.z && sm_ZeroWheelCounter >= ZEROWHEEL_MINBEST)
		{
			sm_ZeroWheelPosHeight = VEC3V_TO_VECTOR3(coords);
		}

		if (sm_ZeroWheelPosStart.IsZero())
		{
			sm_ZeroWheelPosStart = VEC3V_TO_VECTOR3(coords);
			sm_ZeroWheelPosEnd.Zero();
		}

		if (sm_ChallengeZeroWheelPosStart.IsZero())
		{
			sm_ChallengeZeroWheelPosStart = VEC3V_TO_VECTOR3(coords);
			sm_JumpDistance = 0.0f;
			sm_WasOnAnotherVehicleWhenJumped = sm_IsOnAnotherVehicle;

			if(sm_WasOnAnotherVehicleWhenJumped)
			{
				statDebugf1("Jumped from a vehicle");
			}
		}
	}
	else if(!sm_ChallengeZeroWheelPosStart.IsZero())
	{
		if (!sm_invalidJump)
		{
			// B*3150624 - Dont track this for vehicles that have a parachute
			if (!pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_PARACHUTE) && pVehicle->GetModelIndex() != MI_CAR_DELUXO && pVehicle->GetModelIndex() != MI_BIKE_OPPRESSOR2)
			{
				UpdateRecordedStat(REC_STAT_FARTHEST_JUMP_DIST, sm_JumpDistance);
			}
		}

		sm_JumpDistance = 0.0f;
		sm_ChallengeZeroWheelPosStart.Zero();
		sm_invalidJump = false;
	}
	else if(sm_ZeroWheelCounter > 0 && sm_ZeroWheelBufferCounter < ZEROWHEEL_BUFFERLIMIT)
	{
		sm_ZeroWheelBufferCounter += fwTimer::GetSystemTimeStepInMilliseconds();

		if (sm_ZeroWheelPosEnd.IsZero() && sm_ZeroWheelBufferCounter >= ZEROWHEEL_MINBEST)
		{
			if (pVehicle->GetNumContactWheels() >= ZEROWHEEL_MIN_WHEELS)
			{
				sm_ZeroWheelPosEnd = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
			}
			else
			{
				sm_ZeroWheelCounter = 0;
				sm_ZeroWheelBufferCounter = 0;
				sm_ZeroWheelPosStart.Zero();
				sm_ZeroWheelPosEnd.Zero();
				sm_ZeroWheelPosHeight.Zero();
				sm_JumpDistance = 0.0f;
				sm_HadAttachedParentVehicle=false;
			}
		}
	}
	else
	{
		float distance = 0.0f;
		if (sm_ZeroWheelCounter >= ZEROWHEEL_MINBEST && !sm_ZeroWheelPosStart.IsZero() && pVehicle->GetModelIndex() != MI_CAR_DELUXO && pVehicle->GetModelIndex() != MI_BIKE_OPPRESSOR2)
		{
			distance = Mag(VECTOR3_TO_VEC3V(sm_ZeroWheelPosEnd) - VECTOR3_TO_VEC3V(sm_ZeroWheelPosStart)).Getf();
		}

		if (sm_ZeroWheelCounter >= ZEROWHEEL_MINBEST && distance >= ZEROWHEEL_MIN_DIST)
		{
			statDebugf3("Zero Wheel: Start Coords=<x=%f,y=%f,z=%f>, End Coords=<x=%f,y=%f,z=%f>, time=<%u>, dist=<%f>, altitude=<%f>, wheels=<%d>"
							,sm_ZeroWheelPosStart.x, sm_ZeroWheelPosStart.y, sm_ZeroWheelPosStart.z
							,sm_ZeroWheelPosEnd.x, sm_ZeroWheelPosEnd.y, sm_ZeroWheelPosEnd.z
							,sm_ZeroWheelCounter, distance, sm_ZeroWheelPosHeight.z - sm_ZeroWheelPosEnd.z
							,pVehicle->GetNumContactWheels());

			const CVehicleModelInfo* vi = pVehicle->GetVehicleModelInfo();
			const bool updateLbRecords = (statVerify( vi ) && statVerify( CanUpdateVehicleRecord(vi->GetHashKey()) ));

			if (sm_ZeroWheelCounter > sm_TuneMetadata.m_AwardVehicleJumpTime)
			{
				statDebugf3("Zero Wheel: Increment Stat AIR_LAUNCHES_OVER_5S");
				CStatsMgr::IncrementStat(STAT_AIR_LAUNCHES_OVER_5S.GetStatId(), 1.0f);

				if (updateLbRecords)
				{
					sm_vehicleRecords[sm_curVehIdx].m_NumAirLaunchesOver5s += 1;
				}
			}

			if (distance > sm_TuneMetadata.m_AwardVehicleJumpDistanceA)
			{
				statDebugf3("Zero Wheel: Increment Stat AIR_LAUNCHES_OVER_5M");
				CStatsMgr::IncrementStat(STAT_AIR_LAUNCHES_OVER_5M.GetStatId(), 1.0f);

				if (updateLbRecords)
				{
					sm_vehicleRecords[sm_curVehIdx].m_NumAirLaunchesOver5m += 1;
				}
			}

			if (distance > sm_TuneMetadata.m_AwardVehicleJumpDistanceB)
			{
				statDebugf3("Zero Wheel: Increment Stat AIR_LAUNCHES_OVER_40M");
				CStatsMgr::IncrementStat(STAT_AIR_LAUNCHES_OVER_40M.GetStatId(), 1.0f);
				if (updateLbRecords)
				{
					sm_vehicleRecords[sm_curVehIdx].m_NumAirLaunchesOver40m += 1;
				}
			}

			CStatsMgr::SetGreater(STAT_FARTHEST_JUMP_DIST.GetStatId(), distance);
			CStatsMgr::IncrementStat(STAT_NUMBER_OF_AIR_LAUNCHES.GetStatId(), 1.0f);

			if (updateLbRecords)
			{
				sm_vehicleRecords[sm_curVehIdx].m_NumAirLaunches += 1;

				if (distance > sm_vehicleRecords[sm_curVehIdx].m_HighestJumpDistance)
				{
					sm_vehicleRecords[sm_curVehIdx].m_HighestJumpDistance = distance;
				}
			}

		}
		else if(!sm_GroundProbeHelper.IsProbeActive())
		{
			sm_ZeroWheelPosHeight.Zero();
		}

		sm_ZeroWheelCounter       = 0;
		sm_ZeroWheelBufferCounter = 0;
		sm_ZeroWheelPosStart.Zero();
		sm_ZeroWheelPosEnd.Zero();
	}

	// Determine if the vehicle is on top of another one, with a small time buffer
	bool isOnAnotherVehicle = false;
	for(int i=0; i<pVehicle->GetNumWheels(); i++)
	{
		CWheel* wheel = pVehicle->GetWheel(i);
		CPhysical* physical = wheel->GetHitPhysical();
		if(physical)
		{
			isOnAnotherVehicle = isOnAnotherVehicle || physical->GetIsTypeVehicle();
		}
	}

	if(isOnAnotherVehicle)
	{
		sm_IsOnAnotherVehicle = true;
		sm_TimerAfterOnAnotherVehicle = 0;
	}

	if(sm_IsOnAnotherVehicle && !isOnAnotherVehicle)
	{
		sm_TimerAfterOnAnotherVehicle += fwTimer::GetTimeStepInMilliseconds();
		if(sm_TimerAfterOnAnotherVehicle > TIME_CONSIDERED_ON_VEHICLE_AFTER_CONTACT)
		{
			sm_IsOnAnotherVehicle = false;
		}
	}
}

void CStatsMgr::UpdateVehicleReverseDriving(CVehicle* playerVehicle)
{
	if(!statVerify(playerVehicle))
		return;

	const bool driverIsPlayer = (playerVehicle->GetDriver() && playerVehicle->GetDriver()->IsLocalPlayer());
	Vector3 forward = VEC3V_TO_VECTOR3(playerVehicle->GetTransform().GetForward());
	const Vector3& velocity = playerVehicle->GetVelocity();
	const bool reversing = DotProduct(velocity, forward) < 0;

	if(reversing && !sm_IsOnAnotherVehicle && playerVehicle->GetNumContactWheels()>0 && driverIsPlayer)
	{
		if(sm_VehiclePreviousPosition.IsZero())
			sm_VehiclePreviousPosition = playerVehicle->GetPreviousPosition();
		
		sm_ReverseDrivingPosCurrent = VEC3V_TO_VECTOR3(playerVehicle->GetTransform().GetPosition());
		const float distanceTravelled = Mag(VECTOR3_TO_VEC3V(sm_ReverseDrivingPosCurrent) - VECTOR3_TO_VEC3V(sm_VehiclePreviousPosition)).Getf();
		sm_CurrentDrivingReverseDistance += distanceTravelled;
		
		UpdateRecordedStat(REC_STAT_DIST_DRIVING_CAR_REVERSE, sm_CurrentDrivingReverseDistance);
	}
	if( !reversing && !sm_ReverseDrivingPosCurrent.IsZero() )
	{
		sm_ReverseDrivingPosCurrent.Zero();
		sm_VehiclePreviousPosition.Zero();
		sm_CurrentDrivingReverseDistance=0.0f;
	}
}

void CStatsMgr::UpdateVehicleHydraulics(CVehicle* playerVehicle)
{
	if(!statVerify(playerVehicle))
		return;
	if(playerVehicle->InheritsFromAutomobile())
	{
		CAutomobile* pCar = static_cast<CAutomobile*>(playerVehicle);

		const bool bHasAllWheels = pCar->GetWheelFromId(VEH_WHEEL_LF) && pCar->GetWheelFromId(VEH_WHEEL_RF) && pCar->GetWheelFromId(VEH_WHEEL_LR) && pCar->GetWheelFromId(VEH_WHEEL_RR);

		if(bHasAllWheels)
		{
			if(pCar->m_nAutomobileFlags.bHydraulicsBounceLanding )
			{
				if(!pCar->GetWheelFromId(VEH_WHEEL_LF)->GetIsTouching() && !pCar->GetWheelFromId(VEH_WHEEL_RF)->GetIsTouching() && 
				   !pCar->GetWheelFromId(VEH_WHEEL_LR)->GetIsTouching() && !pCar->GetWheelFromId(VEH_WHEEL_RR)->GetIsTouching() )
				{
					float currentAlt = pCar->GetTransform().GetPosition().GetZf();
					statDebugf3("Hydraulic jump : altitudeOnGround = %f       altitude = %f", sm_hydraulicJumpVehicleAltitude, currentAlt);
					float altitude = currentAlt - sm_hydraulicJumpVehicleAltitude;
					if(altitude > 0.0f)
					{
						CStatsMgr::SetGreater(STAT_HYDRAULIC_JUMP.GetStatId(), altitude);
						statDebugf3("Hydraulic jump : altitude %f", altitude);
					}
				}
			}
			else
			{
				if(pCar->GetWheelFromId(VEH_WHEEL_LF)->GetIsTouching() && pCar->GetWheelFromId(VEH_WHEEL_RF)->GetIsTouching() && 
					pCar->GetWheelFromId(VEH_WHEEL_LR)->GetIsTouching() && pCar->GetWheelFromId(VEH_WHEEL_RR)->GetIsTouching() )
				{
					// position of the car on ground
					sm_hydraulicJumpVehicleAltitude = pCar->GetTransform().GetPosition().GetZf();
				}
			}
		}
	}
}

void CStatsMgr::UpdateVehicleWallride(CVehicle* playerVehicle)
{
    if(!statVerify(playerVehicle))
        return;

    Matrix34 vehicleMtx = MAT34V_TO_MATRIX34(playerVehicle->GetTransformPtr()->GetMatrix());
    Vector3 vehicleUp = vehicleMtx.c;

    static float minWallride = 0.5f;

    float dot = vehicleUp.Dot(Vector3(0,0,1));

    if( dot < minWallride && dot > 0.0f)
    {
        // We're sideways, are we on the ground?
        bool isOnTheGround = true;
        for(int i=0; i<playerVehicle->GetNumWheels() && isOnTheGround; i++)
        {
            CWheel* wheel = playerVehicle->GetWheel(i);
            if(!wheel->GetIsTouching())
            {
                isOnTheGround = false;
            }
        }
        if(isOnTheGround)
        {
            //Yes, we're wallriding!
            Vec3V coords = playerVehicle->GetTransform().GetPosition();
            if(!sm_LastWallridePosition.IsZero())
            {
                Vec3V lastPosition = VECTOR3_TO_VEC3V(sm_LastWallridePosition);
                sm_WallrideDistance += Mag(coords - lastPosition).Getf();
            }
            sm_LastWallridePosition = VEC3V_TO_VECTOR3(coords);
            sm_WallrideBufferTimer = WALLRIDE_BUFFERTIMER_VALUE;
            return;
        }
    }

    // We were wallriding not that long ago
    if(sm_WallrideBufferTimer > 0.0f)
    {
        sm_WallrideBufferTimer -= fwTimer::GetSystemTimeStep();
        statDebugf3("UpdateVehicleWallride - not wallriding, but within the time buffer (%f)", sm_WallrideBufferTimer);
    }

    // We havent been wallriding for long enough, reset the values
    if(sm_WallrideBufferTimer < 0.0f)
    {
        if(sm_WallrideDistance > 0.0f)
        {
            UpdateRecordedStat(REC_STAT_DIST_WALLRIDE, sm_WallrideDistance);
            sm_WallrideDistance = 0.0f;
        }
        if(!sm_LastWallridePosition.IsZero())
        {
            sm_LastWallridePosition.Zero();
            sm_WallrideBufferTimer = 0.0f;
            statDebugf3("UpdateVehicleWallride - Stop wallriding");
        }
    }
}

void
CStatsMgr::UpdateAverageSpeed()
{
	const float totalDist = StatsInterface::GetFloatStat(STAT_DIST_DRIVING_CAR.GetStatId()) 
							+ StatsInterface::GetFloatStat(STAT_DIST_DRIVING_BIKE.GetStatId()); // meters

	const u64 totalTime = StatsInterface::GetUInt64Stat(STAT_TIME_DRIVING_CAR.GetStatId())
							+ StatsInterface::GetUInt64Stat(STAT_TIME_DRIVING_BIKE.GetStatId()); // milliseconds

	if ( totalTime == 0 || totalDist <= 0.0f )
		return;

	StatsInterface::SetStatData(STAT_AVERAGE_SPEED.GetStatId(), (totalDist/totalTime) * 3600.0f, STATUPDATEFLAG_ASSERTONLINESTATS); // km/h
}

void  
CStatsMgr::UpdateStatsWhenPedRunDown(u16 iRandomSeed, CVehicle* vehicle)
{
	bool bNewSeed = true;
	for (int i=0; i<sm_PedsRunDownRecords.GetCount();i++)
	{
		if (sm_PedsRunDownRecords[i] == iRandomSeed)
		{
			bNewSeed = false;
			break;
		}
	}

	if (!bNewSeed)
		return;
	else
	{
		bool bStored = false;
		for (int i=0; i<sm_PedsRunDownRecords.GetCount();i++)
		{
			if (sm_PedsRunDownRecords[i] == 0)
			{
				sm_PedsRunDownRecords[i] = iRandomSeed;
				bStored = true;
				break;
			}
		}

		if (!bStored)
		{
			for (int i=0; i<sm_PedsRunDownRecords.GetCount();i++)
				sm_PedsRunDownRecords[i] = 0;
			sm_PedsRunDownRecords[0] = iRandomSeed;
		}
	}

	CStatsMgr::IncrementStat(STAT_HIGHEST_SKITTLES.GetStatId(), 1.0f);

	if (vehicle)
	{
		const CVehicleModelInfo* vi = vehicle->GetVehicleModelInfo();
		if (statVerify( vi ) && CanUpdateVehicleRecord( vi->GetHashKey() ))
		{
			++sm_vehicleRecords[sm_curVehIdx].m_NumPedsRundown;
		}
	}
}

void 
CStatsMgr::UpdateStatsOnRespawn()
{
	sm_VehicleDeadCheckCounter = 0.0f;
	sm_ZeroWheelPosHeight.Zero();
	sm_GroundProbeHelper.ResetProbe();
	ClearPlayerVehicleStats();
	EndVehicleStats();
	sm_VehicleDeadCheckCounter = 0;
	sm_VehicleDeadCheck = false;
	sm_NearMissNoCrash = 0;
	sm_JumpDistance=0.0f;
	sm_OnFootPosStart.Zero();
	sm_FootAltitude = 0.0f;
	sm_FlyingCounter=0.0f;
	sm_FlyingDistance=0.0f;
	sm_wasInWaterPreviously = false;
	sm_outOfWater = false;
	sm_inAirAfterGroundTimer = 0.0f;
	sm_HadAttachedParentVehicle=false;
	sm_StartVehicleBail.Zero();
	sm_LastVehicleBailPosition.Zero();
	sm_VehicleBailDistance = 0.0f;
	sm_NearMissNoCrashPrecise = 0;
	ClearNearMissArray();
	sm_FlyingAltitude = 0.0f;
	sm_ValidFlyingAltitude = false;
    sm_hasBailedFromParachuting = false;
    sm_LastWallridePosition.Zero();
    sm_WallrideDistance = 0.0f;
}

void
CStatsMgr::PlayerFireWeapon(const CWeapon* weapon)
{
	if(!IsPlayerActive())
		return;

	if (!CStatsUtils::CanUpdateStats())
	{
		return;
	}

	statAssert(weapon);
	if (!weapon)
		return;

	CPed* playerPed = FindPlayerPed();
	if (!playerPed)
		return;

	const CWeaponInfo* wi = weapon->GetWeaponInfo();
	statAssert(wi);
	if (!wi)
		return;

	const u32 weaponHash = wi->GetHash();

	//Add exception for Weapon fire extinguisher because it is treated as a gun right now.
	if (weaponHash == WEAPONTYPE_FIREEXTINGUISHER)
		return;

	StatId statWeaponFired = StatsInterface::GetLocalPlayerWeaponInfoStatId(StatsInterface::WEAPON_STAT_SHOTS, wi, playerPed);
	if(StatsInterface::IsKeyValid(statWeaponFired))
	{
		StatsInterface::IncrementStat(statWeaponFired, 1.0f);
	}
#if __ASSERT
	else if(playerPed->IsArchetypeSet() && StatsInterface::GetStatsPlayerModelValid())
	{
		CStatsMgr::MissingWeaponStatName(STATTYPE_WEAPON_SHOTS
			,statWeaponFired
			,weaponHash
			,wi->GetDamageType()
			,wi->GetName());
	}
#endif // __ASSERT

	if (wi->GetDamageType() == DAMAGE_TYPE_EXPLOSIVE || /*wi->GetIsProjectile() ||*/ wi->GetIsVehicleWeapon())
	{
		if (wi->GetDamageType() == DAMAGE_TYPE_EXPLOSIVE)
		{
			if (StatsInterface::IsKeyValid(STAT_EXPLOSIVE_DAMAGE_SHOTS.GetHash()))
			{
				StatsInterface::IncrementStat(STAT_EXPLOSIVE_DAMAGE_SHOTS.GetStatId(), 1.0f, STATUPDATEFLAG_ASSERTONLINESTATS);
			}
		}

		if (weaponHash == WEAPONTYPE_STICKYBOMB || weaponHash == WEAPONTYPE_GRENADE || weaponHash == WEAPONTYPE_DLC_PROXMINE || weaponHash == WEAPONTYPE_DLC_PIPEBOMB)
		{
			StatsInterface::IncrementStat(STAT_EXPLOSIVES_USED.GetStatId(), 1.0f);
		}
	}
	else if (wi->GetIsGun() && (wi->GetDamageType() == DAMAGE_TYPE_BULLET || wi->GetDamageType() == DAMAGE_TYPE_BULLET_RUBBER))
	{
		StatsInterface::IncrementStat(STAT_SHOTS.GetStatId(), 1.0f);

		CVehicle* pVehicle = playerPed->GetVehiclePedInside();
		if(pVehicle)
		{
			const bool playerIsDriver = (playerPed == pVehicle->GetDriver());
			if(playerIsDriver)
			{
				StatsInterface::IncrementStat(STAT_DB_SHOTS.GetStatId(), 1.0f);
				StatsInterface::IncrementStat(STAT_DB_SHOTTIME.GetStatId(), fwTimer::GetSystemTimeStep()*1000.0f);
			}
			else
			{
				StatsInterface::IncrementStat(STAT_PASS_DB_SHOTS.GetStatId(), 1.0f);
				StatsInterface::IncrementStat(STAT_PASS_DB_SHOTTIME.GetStatId(), fwTimer::GetSystemTimeStep()*1000.0f);
			}
		}
	}

	// track if player has fired a shot while crouching
	if (playerPed->GetIsCrouching())
	{
		sm_bShotFiredInCrouch = true;
	}

	// track if player has fired a shot whilst in cover
	CPedIntelligence* pPI = playerPed->GetPedIntelligence();
	if (pPI)
	{
		CQueriableInterface* pQI = pPI->GetQueriableInterface();
		if (pQI && pQI->IsTaskCurrentlyRunning(CTaskTypes::TASK_IN_COVER))
		{
			sm_bShotFiredInCover = true;
		}
	}
};

void
CStatsMgr::PlayerFiredWeaponToInvenciblePed(const CWeapon* weapon)
{
	if(!IsPlayerActive())
		return;

	statAssert(weapon);
	if (!weapon)
		return;

	CPed* playerPed = FindPlayerPed();
	if (!playerPed)
		return;

	const CWeaponInfo* wi = weapon->GetWeaponInfo();
	statAssert(wi);
	if (!wi)
		return;

	const u32 weaponHash = wi->GetHash();

	//Add exception for Weapon fire extinguisher because it is treated as a gun right now.
	if (weaponHash == WEAPONTYPE_FIREEXTINGUISHER)
		return;
	// Another exception if the damage type is melee weapon
	if(weapon->GetDamageType()==DAMAGE_TYPE_MELEE)
		return;

	StatId statWeaponFired = StatsInterface::GetLocalPlayerWeaponInfoStatId(StatsInterface::WEAPON_STAT_SHOTS, wi, playerPed);
	if(StatsInterface::IsKeyValid(statWeaponFired))
	{
		StatsInterface::DecrementStat(statWeaponFired, 1.0f);
	}
#if __ASSERT
	else if(playerPed->IsArchetypeSet() && StatsInterface::GetStatsPlayerModelValid())
	{
		CStatsMgr::MissingWeaponStatName(STATTYPE_WEAPON_SHOTS
			,statWeaponFired
			,weaponHash
			,wi->GetDamageType()
			,wi->GetName());
	}
#endif // __ASSERT


	if (wi->GetDamageType() == DAMAGE_TYPE_EXPLOSIVE || /*wi->GetIsProjectile() ||*/ wi->GetIsVehicleWeapon())
	{
		if (wi->GetDamageType() == DAMAGE_TYPE_EXPLOSIVE)
		{
			if (StatsInterface::IsKeyValid(STAT_EXPLOSIVE_DAMAGE_SHOTS.GetHash()))
			{
				StatsInterface::DecrementStat(STAT_EXPLOSIVE_DAMAGE_SHOTS.GetStatId(), 1.0f, STATUPDATEFLAG_ASSERTONLINESTATS);
			}
		}

		if (weaponHash == WEAPONTYPE_STICKYBOMB || weaponHash == WEAPONTYPE_GRENADE || weaponHash == WEAPONTYPE_DLC_PROXMINE || weaponHash == WEAPONTYPE_DLC_PIPEBOMB)
		{
			StatsInterface::DecrementStat(STAT_EXPLOSIVES_USED.GetStatId(), 1.0f);
		}
	}
	else if (wi->GetIsGun() && (wi->GetDamageType() == DAMAGE_TYPE_BULLET || wi->GetDamageType() == DAMAGE_TYPE_BULLET_RUBBER))
	{
		StatsInterface::DecrementStat(STAT_SHOTS.GetStatId(), 1.0f);

		CVehicle* pVehicle = playerPed->GetVehiclePedInside();
		if(pVehicle)
		{
			const bool playerIsDriver = (playerPed == pVehicle->GetDriver());
			if(playerIsDriver)
			{
				StatsInterface::DecrementStat(STAT_DB_SHOTS.GetStatId(), 1.0f);
				StatsInterface::DecrementStat(STAT_DB_SHOTTIME.GetStatId(), fwTimer::GetSystemTimeStep()*1000.0f);
			}
			else
			{
				StatsInterface::DecrementStat(STAT_PASS_DB_SHOTS.GetStatId(), 1.0f);
				StatsInterface::DecrementStat(STAT_PASS_DB_SHOTTIME.GetStatId(), fwTimer::GetSystemTimeStep()*1000.0f);
			}
		}
	}

	// track if player has fired a shot while crouching
	if (playerPed->GetIsCrouching())
	{
		sm_bShotFiredInCrouch = false;
	}

	// track if player has fired a shot whilst in cover
	CPedIntelligence* pPI = playerPed->GetPedIntelligence();
	if (pPI)
	{
		CQueriableInterface* pQI = pPI->GetQueriableInterface();
		if (pQI && pQI->IsTaskCurrentlyRunning(CTaskTypes::TASK_IN_COVER))
		{
			sm_bShotFiredInCover = false;
		}
	}
};

void 
CStatsMgr::PlayerArrested()
{
	ClearPlayingTimeStats();

	CNetworkTelemetry::PlayerArrested();

	StatsInterface::IncrementStat(StatsInterface::GetStatsModelHashId("BUSTED"), 1);

	sm_ZeroWheelPosHeight.Zero();
	sm_LastVehicleBailPosition.Zero();
	sm_GroundProbeHelper.ResetProbe();
	ClearPlayerVehicleStats();

	EndVehicleStats();
	sm_VehicleDeadCheckCounter = 0;
	sm_VehicleDeadCheck = false;
	sm_DriveNoCrashTime = 0.0f;
	sm_DriveNoCrashDist = 0.0f;
	sm_NearMissNoCrash = 0;
	sm_JumpDistance=0.0f;
	sm_VehicleBailDistance = 0.0f;
	sm_wasInWaterPreviously = false;
	sm_outOfWater = false;
	sm_inAirAfterGroundTimer = 0.0f;
	sm_HadAttachedParentVehicle=false;
	sm_NearMissNoCrashPrecise = 0;
	ClearNearMissArray();

	SetCheatIsActive( false );
}

void 
CStatsMgr::PreSaveBaseStats(const bool bMultiplayerSave)
{
	CStatsMgr::CheckWriteVehicleRecords( StatsInterface::GetVehicleLeaderboardWriteOnSavegame( ) );

	sm_StatsData.PreSaveBaseStats( bMultiplayerSave );

	if (CGenericGameStorage::GetSaveOperation() == OPERATION_AUTOSAVING)
		StatsInterface::IncrementStat(StatsInterface::GetStatsModelHashId("AUTO_SAVED"), 1);
	else
		StatsInterface::IncrementStat(StatsInterface::GetStatsModelHashId("MANUAL_SAVED"), 1);

	//Set mac address profile stat
	if (!bMultiplayerSave)
	{
		static StatId s_screenWidth("_ScreenWidth");
		static StatId s_screenHeight("_ScreenHeight");
		if (StatsInterface::IsKeyValid(s_screenWidth))
		{
			StatsInterface::SetStatData(s_screenWidth, GRCDEVICE.GetWidth(), STATUPDATEFLAG_DIRTY_PROFILE);
			StatsInterface::SetStatData(s_screenHeight, GRCDEVICE.GetHeight(), STATUPDATEFLAG_DIRTY_PROFILE);
		}
	}

	ClearPlayingTimeStats();

	CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();
	if (pLocalPlayer && !pLocalPlayer->GetIsDrivingVehicle())
	{
		ClearPlayerVehicleStats();
		sm_ZeroWheelPosHeight.Zero();
		sm_GroundProbeHelper.ResetProbe();
	}
}

void 
CStatsMgr::PlayerCompleteMission()
{
	ClearPlayingTimeStats();
	ClearPlayerVehicleStats();
}

void 
CStatsMgr::PlayerFailedMission()
{
	ClearPlayingTimeStats();
	ClearPlayerVehicleStats();
}


// ---- Access Methods ----------------------------------------------

float 
CStatsMgr::GetPercentageProgress(void)
{
	statAssertf(!NetworkInterface::IsGameInProgress(), "Not tracked in MP - ask Brenda why not...");

	/*
	if (NetworkInterface::IsGameInProgress())
	{
		return StatsInterface::GetFloatStat(STAT_PROGRESS_MADE.GetStatId());
	}
	*/

	return StatsInterface::GetFloatStat(STAT_TOTAL_PROGRESS_MADE.GetHash());
}

float 
CStatsMgr::GetPercentageKillsMadeinFreeAim(void)
{
	float iTotalArmedKills     = static_cast<float>(StatsInterface::GetIntStat(STAT_KILLS_ARMED.GetHash()));
	float iTotalKillsInFreeAim = StatsInterface::GetFloatStat(STAT_KILLS_IN_FREE_AIM.GetStatId());

	return ((0.0f==iTotalArmedKills || 0.0f==iTotalKillsInFreeAim) ? 0.0f :rage::Floorf(iTotalKillsInFreeAim*100/iTotalArmedKills));
}

float 
CStatsMgr::GetFatAndMuscleModifier(eStatModAbilities nAbility)
{
	float fModifier = 1.0f;
	switch(nAbility)
	{
	case STAT_MODIFIER_CLIMB_HEIGHT:
		fModifier =  0.0;
		break;

	case STAT_MODIFIER_JUMP_SPEED:
		fModifier =  1.0;
		break;

	case STAT_MODIFIER_SPRINT_ENERGY:
		// if you change this, you need to update the ambient trigger for the player being tired as well (taskambient.cpp)
		fModifier = 100.0f;
		break;

	case STAT_MODIFIER_BREATH_UNDERWATER:
		fModifier = 1.0;
		break;

	default:
		Assert(0);
		break;
	}
	return fModifier;
}

bool CStatsMgr::GetFlyingAltitude(float& value)
{
	if(sm_ValidFlyingAltitude)
	{
		value = sm_FlyingAltitude;  
		return true;
	}
	return false;
}

void
CStatsMgr::SetCheatIsActive(const bool active)
{
	if (active)
	{
		statDebugf1("ACTIVATE CHEAT");
		sm_cheatIsActive      = true;
		sm_cheatIsActiveTimer = fwTimer::GetTimeInMilliseconds();
	}
	else
	{
		statDebugf1("DE-ACTIVATE CHEAT");
		sm_cheatIsActive      = false;
		sm_cheatIsActiveTimer = 0;
	}
}

void 
CStatsMgr::RegisterPedKill(const CEntity* inflictor, const CPed* victim, const u32 weaponHash, bool headShot, const int weaponDamageComponent, const bool withMeleeWeapon, const fwFlags32& flags)
{
	if (!CStatsUtils::IsStatsTrackingEnabled())
		return;

	AssertEntityPointerValid_NotInWorld((CPed *)victim);

	statAssertf(victim, "Null victim.");
	if (!victim)
		return;

	if(!IsPlayerActive())
		return;

	bool killedByVehicle = false;

	CVehicle* vehicleInflictor = 0;

	CPed* pedInflictor = 0;
	if (inflictor)
	{
		if (inflictor->GetIsTypePed())
		{
			pedInflictor = (CPed*)inflictor;
		}
		else if (inflictor->GetIsTypeVehicle())
		{
			vehicleInflictor = (CVehicle*)(inflictor);
			pedInflictor = vehicleInflictor->GetDriver();
			killedByVehicle = true;
		}
		else
		{
			//Search for the damage entity
			CEntity* damageEntity = victim->GetWeaponDamageEntity();
			if (damageEntity && ((fwTimer::GetTimeInMilliseconds() - victim->GetWeaponDamagedTime()) < PLAYER_WEAPON_DAMAGE_TIMEOUT))
			{
				if (damageEntity->GetIsTypePed())
				{
					pedInflictor = (CPed*)damageEntity;
				}
				else if (damageEntity->GetIsTypeVehicle())
				{
					vehicleInflictor = (CVehicle*)damageEntity;
					pedInflictor = vehicleInflictor->GetDriver();
					killedByVehicle = true;
				}
			}
		}
	}

	//We only care about peds and the peds driving the vehicle
	if (pedInflictor)
	{
		//Victim is not the local player.
		if (!victim->IsLocalPlayer())
		{
			if (pedInflictor->IsLocalPlayer())
			{
				if (weaponDamageComponent == RagdollComponent::RAGDOLL_HEAD || weaponDamageComponent == RagdollComponent::RAGDOLL_NECK)
				{
					headShot = true;
				}
				// We don't want to register headshots if melee weapons/pistol whips/melee attacks with firearms were registered (B* 2450011)
				if (withMeleeWeapon)
				{
					headShot = false;
				}
				RegisterKillByLocalPlayer(pedInflictor, victim, weaponHash, headShot, flags);

				//Count UNARMED_PED_HITS in multiplayer.
				if (weaponHash == WEAPONTYPE_UNARMED)
				{
					if(StatsInterface::IsKeyValid(STAT_UNARMED_PED_HITS.GetStatId()))
					{
						StatsInterface::IncrementStat(STAT_UNARMED_PED_HITS.GetStatId(), 1.0f, STATUPDATEFLAG_ASSERTONLINESTATS);
					}
				}

				//Update peds run down
				if (WEAPONTYPE_RUNOVERBYVEHICLE == weaponHash || WEAPONTYPE_RAMMEDBYVEHICLE == weaponHash)
				{
					if (WEAPONTYPE_HIT_BY_WATER_CANNON != weaponHash && (killedByVehicle || pedInflictor->GetIsDrivingVehicle()))
					{
						if (!vehicleInflictor && !killedByVehicle && pedInflictor->GetIsDrivingVehicle())
						{
							vehicleInflictor = pedInflictor->GetMyVehicle();
						}

						if (statVerify(vehicleInflictor))
						{
							CStatsMgr::UpdateStatsWhenPedRunDown(victim->GetRandomSeed(), vehicleInflictor);
						}
					}
				}
			}
			else
			{
				CStatsMgr::IncrementStat(STAT_KILLS_BY_OTHERS.GetStatId(), 1);
			}
		}
	}

	//Register that a player was killed.
	if(victim->IsPlayer())
	{
		CStatsMgr::RegisterPlayerKilled(pedInflictor, victim, weaponHash, weaponDamageComponent, withMeleeWeapon);
	}
}

void CStatsMgr::RegisterVehicleNearMiss(const CPed* driver, const u32 time)
{
	if (!CStatsUtils::IsStatsTrackingEnabled())
		return;

	if (IsPlayerActive() && driver && driver->IsLocalPlayer())
	{
		//Don't trigger near miss if car had just collided
		if (time + 2000 < fwTimer::GetTimeInMilliseconds())
		{
			CStatsMgr::IncrementStat(STAT_NUMBER_NEAR_MISS.GetStatId(), 1);
			sm_NearMissNoCrash++;
			CStatsMgr::SetGreater(STAT_NUMBER_NEAR_MISS_NOCRASH.GetStatId(), (float) sm_NearMissNoCrash);
			UpdateRecordedStat(REC_STAT_NUMBER_NEAR_MISS_NOCRASH, (float)sm_NearMissNoCrash);
		}
	}
}

void CStatsMgr::RegisterVehicleNearMissAccepted()
{
	sm_NearMissNoCrashPrecise++;
	CStatsMgr::SetGreater(STAT_NEAR_MISS_PRECISE.GetStatId(), (float) sm_NearMissNoCrashPrecise);
	UpdateRecordedStat(REC_STAT_NEAR_MISS_PRECISE, (float)sm_NearMissNoCrashPrecise);
}

void CStatsMgr::RegisterVehicleNearMissPrecise(const CPed* playerDriver, const u32 time)
{
	if (!CStatsUtils::IsStatsTrackingEnabled())
		return;

	if (IsPlayerActive() && playerDriver && playerDriver->IsLocalPlayer())
	{
		AddNearMissTimeToArray(time);
	}
}

void CStatsMgr::RegisterExplosionHit(const CEntity* inflictor, const CEntity* victim, const u32 weaponHash, const float damage)
{
	if (!CStatsUtils::IsStatsTrackingEnabled())
		return;

	if (!inflictor || !victim)
		return;

	const float MIN_DAMAGE_TO_CONSIDER_HIT = 1.0f;
	if (damage < MIN_DAMAGE_TO_CONSIDER_HIT)
		return;

	if (!inflictor->GetIsTypePed())
		return;

	if (!static_cast< const CPed* >( inflictor )->IsLocalPlayer())
		return;

	bool incrementStat = false;

	if (victim->GetIsTypePed())
	{
		incrementStat = !(static_cast< const CPed* >( victim )->IsLocalPlayer());
	}
	else if (victim->GetIsTypeVehicle())
	{
		const CVehicle* vehicle = static_cast< const CVehicle* >( victim );

		if (vehicle->GetLayoutInfo())
		{
			if (vehicle->GetDriver() || vehicle->GetNumberOfPassenger() > 0)
			{
				int numSeats = vehicle->GetLayoutInfo()->GetNumSeats();

				for(int i=0; i<numSeats && !incrementStat; i++)
				{
					CPed* pPed = vehicle->GetPedInSeat(i);

					if(pPed && !pPed->IsDead() && !pPed->IsLocalPlayer())
					{
						incrementStat = true;
					}
				}
			}
		}
	}

	if (!incrementStat)
		return;

	const CWeaponInfo* wi = CWeaponInfoManager::GetInfo< CWeaponInfo >( weaponHash );
	statAssertf(wi, "NULL weapon info for weapon hash=[0x%X]", weaponHash);
	if(!wi)
		return;

	if (wi->GetDamageType() == DAMAGE_TYPE_EXPLOSIVE)
	{
		if (StatsInterface::IsKeyValid(STAT_EXPLOSIVE_DAMAGE_HITS.GetHash()))
		{
			StatsInterface::IncrementStat(STAT_EXPLOSIVE_DAMAGE_HITS.GetStatId(), 1.0f, STATUPDATEFLAG_ASSERTONLINESTATS);
		}
	}
}

void CStatsMgr::RegisterExplosionHitAnything(const CEntity* inflictor, const u32 weaponHash)
{
	if (!CStatsUtils::IsStatsTrackingEnabled())
		return;

	if (!inflictor)
		return;

	if (!inflictor->GetIsTypePed())
		return;

	if (!static_cast< const CPed* >( inflictor )->IsLocalPlayer())
		return;

	const CWeaponInfo* wi = CWeaponInfoManager::GetInfo< CWeaponInfo >( weaponHash );
	if(!wi)
		return;

	if (wi->GetDamageType() != DAMAGE_TYPE_EXPLOSIVE)
		return;
	
	if (StatsInterface::IsKeyValid(STAT_EXPLOSIVE_DAMAGE_HITS_ANYTHING.GetHash()))
	{
		StatsInterface::IncrementStat(STAT_EXPLOSIVE_DAMAGE_HITS_ANYTHING.GetStatId(), 1.0f, STATUPDATEFLAG_ASSERTONLINESTATS);
	}
}

void CStatsMgr::RegisterKillByLocalPlayer(const CPed* killer, const CPed* victim, const u32 weaponHash, const bool headShot, const fwFlags32& flags)
{
	AssertEntityPointerValid_NotInWorld((CPed *)victim);
	statAssertf(killer, "Null Killer.");
	statAssertf(victim, "Null victim.");

	if (!killer || !victim)
		return;

	statAssertf(killer->IsLocalPlayer(), "Killer is not the local player.");
	if (!killer->IsLocalPlayer())
		return;

	//If this guy was in the players group the player will loose some respect.
	if (NetworkInterface::IsGameInProgress())
	{
		if (killer->GetNetworkObject() && killer->GetNetworkObject()->GetPlayerOwner())
		{
			const bool isPlayerVagosMember = (victim->IsPlayer() && StatsInterface::IsPlayerVagos(victim));
			const bool isPlayerLostMember  = (victim->IsPlayer() && StatsInterface::IsPlayerLost(victim));
			const bool isPedVagosMember    = (!victim->IsPlayer() && StatsInterface::GetPedOfType(victim, StatsInterface::MP_GROUP_GANG_VAGOS));
			const bool isPedLostMember     = (!victim->IsPlayer() && StatsInterface::GetPedOfType(victim, StatsInterface::MP_GROUP_GANG_LOST));

			if (isPlayerVagosMember || isPlayerLostMember || isPedLostMember || isPedVagosMember)
			{
				if (StatsInterface::IsSameTeam(victim, killer))
				{
					CStatsMgr::IncrementStat(STAT_KILLS_FRIENDLY_GANG_MEMBERS.GetStatId(), 1);
				}
				else if (isPlayerVagosMember && StatsInterface::IsPlayerLost(killer))
				{
					statAssertf(!isPlayerLostMember && !isPedLostMember && !isPedVagosMember, "isPlayerVagosMember=%s, isPlayerLostMember=%s, isPedLostMember=%s,isPedVagosMember=%s", isPlayerVagosMember?"True":"False", isPlayerLostMember?"True":"False", isPedLostMember?"True":"False", isPedVagosMember?"True":"False");
					CStatsMgr::IncrementStat(STAT_KILLS_ENEMY_GANG_MEMBERS.GetStatId(), 1);
				}
				else if (isPlayerLostMember && StatsInterface::IsPlayerVagos(killer))
				{
					statAssertf(!isPlayerVagosMember && !isPedLostMember && !isPedVagosMember, "isPlayerVagosMember=%s, isPlayerLostMember=%s, isPedLostMember=%s,isPedVagosMember=%s", isPlayerVagosMember?"True":"False", isPlayerLostMember?"True":"False", isPedLostMember?"True":"False", isPedVagosMember?"True":"False");
					CStatsMgr::IncrementStat(STAT_KILLS_ENEMY_GANG_MEMBERS.GetStatId(), 1);
				}
				else if (isPedVagosMember && StatsInterface::IsPlayerLost(killer))
				{
					statAssertf(!isPlayerVagosMember && !isPlayerLostMember && !isPedLostMember, "isPlayerVagosMember=%s, isPlayerLostMember=%s, isPedLostMember=%s,isPedVagosMember=%s", isPlayerVagosMember?"True":"False", isPlayerLostMember?"True":"False", isPedLostMember?"True":"False", isPedVagosMember?"True":"False");
					CStatsMgr::IncrementStat(STAT_KILLS_ENEMY_GANG_MEMBERS.GetStatId(), 1);
				}
				else if (isPedLostMember && StatsInterface::IsPlayerVagos(killer))
				{
					statAssertf(!isPlayerVagosMember && !isPlayerLostMember && !isPedVagosMember, "isPlayerVagosMember=%s, isPlayerLostMember=%s, isPedLostMember=%s,isPedVagosMember=%s", isPlayerVagosMember?"True":"False", isPlayerLostMember?"True":"False", isPedLostMember?"True":"False", isPedVagosMember?"True":"False");
					CStatsMgr::IncrementStat(STAT_KILLS_ENEMY_GANG_MEMBERS.GetStatId(), 1);
				}
			}
		}
	}

	CStatsMgr::UpdateStatPedsKilledOfThisType(victim->GetPedType());

	const ePedType pedType = victim->GetPedType();

	const bool killedPlayer = victim->IsPlayer() && statVerify(!victim->IsLocalPlayer());
	if (!killedPlayer)
	{
		if (PEDTYPE_SWAT == pedType)
		{
			StatsInterface::IncrementStat(STAT_KILLS_SWAT.GetStatId(), 1.0f);
		}
		else if (PEDTYPE_COP == pedType)
		{
			StatsInterface::IncrementStat(STAT_KILLS_COP.GetStatId(), 1.0f);
		}
		else if (!victim->IsLawEnforcementPed() && !victim->IsGangPed())
		{
			StatsInterface::IncrementStat(STAT_KILLS_INNOCENTS.GetStatId(), 1.0f); 
		}
	}

	if (victim->GetPedConfigFlag(CPED_CONFIG_FLAG_KilledByStealth))
	{
		StatsInterface::IncrementStat(STAT_KILLS_STEALTH.GetStatId(), 1.0f); 
	}

	const unsigned curTime = fwTimer::GetTimeInMilliseconds_NonScaledClipped();
	if((curTime - sm_KillingSpreeTime) < KILLSPREE_INTERVAL && 0 < (curTime - sm_KillingSpreeTime) && 0 < sm_KillingSpreeTime)
	{
		float killcounter = (sm_KillingSpreeCounter==0.0f) ? 2.0f : 1.0f;

		sm_KillingSpreeCounter   += (u32)killcounter;
		sm_KillingSpreeTotalTime += (curTime - sm_KillingSpreeTime);

		if(killedPlayer)
		{
			CStatsMgr::IncrementStat(STAT_PLAYER_KILLS_ON_SPREE.GetStatId(), killcounter);
		}
		else if(CPedType::IsCopType(pedType))
		{
			CStatsMgr::IncrementStat(STAT_COPS_KILLS_ON_SPREE.GetStatId(), killcounter);
		}
		else
		{
			CStatsMgr::IncrementStat(STAT_PEDS_KILLS_ON_SPREE.GetStatId(), killcounter);
		}

		CStatsMgr::SetGreater(STAT_LONGEST_KILLING_SPREE.GetStatId(), (float)sm_KillingSpreeCounter);
		CStatsMgr::SetGreater(STAT_LONGEST_KILLING_SPREE_TIME.GetStatId(), (float)sm_KillingSpreeTotalTime);
	}
	else
	{
		//Reset Killing spree time now and start counting
		sm_KillingSpreeTotalTime = 0;
		sm_KillingSpreeCounter   = 0;
	}

	sm_KillingSpreeTime = curTime;

	if (killedPlayer)
	{
		if(ShouldCountKills())
		{
			CStatsMgr::IncrementStat(STAT_KILLS_PLAYERS.GetStatId(), 1.0f);
			CStatsMgr::IncrementStat(STAT_MP_KILLS_PLAYERS, 1.0f);
			StatsInterface::IncrementStat(STAT_MPPLY_KILLS_PLAYERS_CHEATER, 1.0f);
		}
	}

	CStatsMgr::UpdateWeaponKill(killer, weaponHash, headShot, killedPlayer, flags, victim);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : RegisterCarBlownUpByPlayer
// PURPOSE :  This is called by the game code every time a car is killed.
/////////////////////////////////////////////////////////////////////////////////

void CStatsMgr::RegisterVehicleBlownUpByPlayer(CVehicle* vehicle, const float damage)
{
	if (!CStatsUtils::IsStatsTrackingEnabled())
		return;

	AssertEntityPointerValid_NotInWorld(vehicle);

	if(!IsPlayerActive())
		return;

	bool playerWasInsideTheVehicle = false;
	bool updateChainReaction       = true;

	CPed* playerPed = FindPlayerPed();
	const CSeatManager* sm = vehicle->GetSeatManager();
	if (statVerify(sm) && statVerify(playerPed))
	{
		playerWasInsideTheVehicle = (-1 != sm->GetPedsSeatIndex(playerPed));
	}

	if (!playerWasInsideTheVehicle)
	{
		const bool isLawEnforcement = vehicle->IsLawEnforcementVehicleModelId(vehicle->GetModelId());

		// increment the stats depending on what type of vehicle this is:
		if (VEHICLE_TYPE_CAR == vehicle->GetVehicleType() || VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE == vehicle->GetVehicleType())
		{
			CStatsMgr::IncrementStat(STAT_CARS_EXPLODED.GetStatId(), 1.0f);

			//The car belongs to the cops
			if(isLawEnforcement && !vehicle->IsPersonalVehicle())
			{
				CStatsMgr::IncrementStat(STAT_CARS_COPS_EXPLODED.GetStatId(), 1.0f);
			}
		}
		else if (VEHICLE_TYPE_BIKE == vehicle->GetVehicleType())
			CStatsMgr::IncrementStat(STAT_BIKES_EXPLODED.GetStatId(), 1.0f);
		else if (VEHICLE_TYPE_BOAT==vehicle->GetVehicleType())
			CStatsMgr::IncrementStat(STAT_BOATS_EXPLODED.GetStatId(), 1.0f);
		else if (VEHICLE_TYPE_HELI==vehicle->GetVehicleType())
			CStatsMgr::IncrementStat(STAT_HELIS_EXPLODED.GetStatId(), 1.0f);
		else if (VEHICLE_TYPE_PLANE==vehicle->GetVehicleType())
			CStatsMgr::IncrementStat(STAT_PLANES_EXPLODED.GetStatId(), 1.0f);
		else if (VEHICLE_TYPE_QUADBIKE==vehicle->GetVehicleType() || VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE==vehicle->GetVehicleType())
			CStatsMgr::IncrementStat(STAT_QUADBIKE_EXPLODED.GetStatId(), 1.0f);
		else if (VEHICLE_TYPE_BICYCLE==vehicle->GetVehicleType())
			CStatsMgr::IncrementStat(STAT_BICYCLE_EXPLODED.GetStatId(), 1.0f);
		else if (VEHICLE_TYPE_SUBMARINE==vehicle->GetVehicleType())
			CStatsMgr::IncrementStat(STAT_SUBMARINE_EXPLODED.GetStatId(), 1.0f);
		else if (VEHICLE_TYPE_TRAIN==vehicle->GetVehicleType())
			CStatsMgr::IncrementStat(STAT_TRAIN_EXPLODED.GetStatId(), 1.0f);
		else
		{
			updateChainReaction = false;
		}

		if (updateChainReaction)
		{
			const unsigned curTime = fwTimer::GetTimeInMilliseconds_NonScaledClipped();
			const unsigned timeSinceLastVehicleDestroyed = curTime - sm_VehicleDestroyedSpreeTime;
			if(timeSinceLastVehicleDestroyed < DESTROYED_VEHICLES_SPREE_INTERVAL && 0 < timeSinceLastVehicleDestroyed && 0 < sm_VehicleDestroyedSpreeTime)
			{
				const bool isTankModel = vehicle->IsTank();

				const float numVehiclesDestroyed = sm_VehicleDestroyedSpreeCounter == 0 ? 2.0f : 1.0f;

				sm_VehicleDestroyedSpreeTotalTime += (curTime - sm_VehicleDestroyedSpreeTime);
				sm_VehicleDestroyedSpreeCounter   += (sm_VehicleDestroyedSpreeCounter == 0) ? 2 : 1;

				CStatsMgr::IncrementStat(STAT_VEHICLES_DESTROYED_ON_SPREE.GetStatId(), numVehiclesDestroyed);

				if(isLawEnforcement)
				{
					CStatsMgr::IncrementStat(STAT_COP_VEHI_DESTROYED_ON_SPREE.GetStatId(), numVehiclesDestroyed);
				}
				else if(isTankModel)
				{
					CStatsMgr::IncrementStat(STAT_TANKS_DESTROYED_ON_SPREE.GetStatId(), numVehiclesDestroyed);
				}
			}
			else
			{
				sm_VehicleDestroyedSpreeTotalTime = 0;
				sm_VehicleDestroyedSpreeCounter   = 0;
			}

			sm_VehicleDestroyedSpreeTime = curTime;
		}
	}
	else if(vehicle->GetDriver() == playerPed)
	{
		// increment the stats depending on what type of vehicle this is:
		if (VEHICLE_TYPE_CAR == vehicle->GetVehicleType() && damage > 0.0f && vehicle->GetVehicleModelInfo() && vehicle->IsInAir())
		{
			const u32 keyHash = vehicle->GetVehicleModelInfo()->GetHashKey();
			StartNewVehicleRecords(keyHash);
			sm_CrashedVehicleKeyHash            = keyHash;
			sm_CrashedVehicleTimer              = fwTimer::GetSystemTimeInMilliseconds();
			sm_CrashedVehicleDamageAccumulator += damage;
			CheckCrashDamageAccumulator( true );
		}
	}
}

void 
CStatsMgr::RegisterVehicleDamage(CVehicle* vehicle, CEntity* inflictor, eDamageType damageType, float damage, const bool wasWrecked)
{
	if (!CStatsUtils::IsStatsTrackingEnabled())
		return;

	if(!IsPlayerActive())
		return;

	if (!vehicle || !inflictor)
		return;

	// we crashed with an armored vehicle, damage is zero but we still want to reset the no crash variables
	if (0.0f >= damage && damageType==DAMAGE_TYPE_COLLISION && IS_LOCAL_PLAYER_DRIVER_OR_LAST_DRIVER(vehicle))
	{
		sm_DriveNoCrashTime = 0.0f;
		sm_DriveNoCrashDist = 0.0f;
		sm_NearMissNoCrashPrecise = 0;
		ClearNearMissArray();
	}

	if (0.0f >= damage)
		return;

	const bool isWrecked = (vehicle->GetStatus() == STATUS_WRECKED);

	//Trashed vehicles wont count.
	if (isWrecked && wasWrecked)
		return;

	bool inflictorIsLocalPlayer = false;
	if (inflictor->GetIsTypePed())
	{
		inflictorIsLocalPlayer = static_cast<CPed*>(inflictor)->IsLocalPlayer();
	}
	else if (inflictor->GetIsTypeVehicle())
	{
		CPed* driver = static_cast<CVehicle*>(inflictor)->GetDriver();
		if (driver)
		{
			inflictorIsLocalPlayer = driver->IsLocalPlayer();
		}
	}

	if (inflictorIsLocalPlayer && isWrecked)
	{
		CStatsMgr::IncrementStat(STAT_CARS_WRECKED.GetStatId(), 1.0f);

		if (vehicle->IsLawEnforcementVehicleModelId(vehicle->GetModelId()))
		{
			CStatsMgr::IncrementStat(STAT_CARS_COPS_WRECKED.GetStatId(), 1.0f);
		}
	}

	if(damageType == DAMAGE_TYPE_COLLISION)
	{
		CVehicleModelInfo* mi = vehicle->GetVehicleModelInfo();
		if (statVerify(mi) && IS_LOCAL_PLAYER_DRIVER_OR_LAST_DRIVER(vehicle))
		{
			StatsInterface::SetGreater(STAT_LONGEST_DRIVE_NOCRASH.GetStatId(), sm_DriveNoCrashDist);
			UpdateRecordedStat(REC_STAT_LONGEST_DRIVE_NOCRASH, sm_DriveNoCrashDist);
			sm_DriveNoCrashTime = 0.0f;
			sm_DriveNoCrashDist = 0.0f;
			sm_NearMissNoCrash = 0;
			sm_ReverseDrivingPosCurrent.Zero(); // Stop when we crash
			sm_CurrentDrivingReverseDistance=0.0f;
			CheckCrashDamageAccumulator( );

			sm_CrashedVehicleKeyHash            = mi->GetHashKey();
			sm_CrashedVehicleTimer              = fwTimer::GetSystemTimeInMilliseconds();
			sm_CrashedVehicleDamageAccumulator += damage;
			sm_NearMissNoCrashPrecise = 0;
			ClearNearMissArray();

			sm_JumpDistance=0.0f;

			statAssertf(sm_CrashedVehicleKeyHash > 0, "Vehicle %s with invalid hash key = %u", mi->GetGameName(), sm_CrashedVehicleKeyHash);
			statDebugf3("Vehicle crash - \"%s : %u\" , damage=%f, Accumulator=%f", mi->GetGameName(), sm_CrashedVehicleKeyHash, damage, sm_CrashedVehicleDamageAccumulator);
		}

		if (inflictor->GetIsTypeVehicle())
		{
			CPed* inflictorDriver = static_cast<CVehicle*>(inflictor)->GetDriver();
			CPed* driver = vehicle->GetDriver();
			if (inflictorDriver && driver)
			{
				if(inflictorDriver->IsNetworkPlayer() && driver->IsLocalPlayer())
				{
					bool cloneIsResponsibleForCollision = CEventNetworkEntityDamage::IsVehicleResponsibleForCollision(vehicle, static_cast<CVehicle*>(inflictor), WEAPONTYPE_RAMMEDBYVEHICLE);
					if(!cloneIsResponsibleForCollision)
					{
						sm_PlayersVehiclesDamages = damage;
						UpdateRecordedStat(REC_STAT_PLAYER_VEHICLE_DAMAGES, sm_PlayersVehiclesDamages);
					}
				}
			}
		}
	}
}

void CStatsMgr::RegisterPlayerSetOnFire()
{
	UpdateRecordedStat(REC_STAT_PLAYERS_SET_ON_FIRE, 1.0f);
}

void 
CStatsMgr::RegisterParachuteDeployed(CPed* ped)
{
	if (!CStatsUtils::IsStatsTrackingEnabled())
		return;

	if(!IsPlayerActive())
		return;

	statAssert(ped);
	if (!ped)
		return;

	if (!ped->IsLocalPlayer())
		return;

	if (!ped->GetIsParachuting())
		return;

	if(sm_ParachuteDeployPos.IsZero() || sm_hasBailedFromParachuting)
	{
		sm_ParachuteDeployPos = VEC3V_TO_VECTOR3(ped->GetTransform().GetPosition());
		statDebugf3("[Parachute] RegisterParachuteDeployed : sm_ParachuteDeployPos <x=%f,y=%f,z=%f> ", sm_ParachuteDeployPos.GetX(), sm_ParachuteDeployPos.GetY(), sm_ParachuteDeployPos.GetZ());
		sm_hasBailedFromParachuting = false;

		sm_FreeFallAltitude = 0.0f;
		sm_FreeFallStartPos.Zero();
	}

	// Stop bail out of vehicle when a parachute is open
	if(!sm_StartVehicleBail.IsZero())
	{
		sm_StartVehicleBail.Zero();
		sm_LastVehicleBailPosition.Zero();
		sm_VehicleBailDistance=0.0f;
	}
}

void CStatsMgr::RegisterPreOpenParachute(CPed* ped)
{
	if (!CStatsUtils::IsStatsTrackingEnabled())
		return;

	if(!IsPlayerActive())
		return;

	statAssert(ped);
	if (!ped)
		return;

	if (!ped->IsLocalPlayer())
		return;

	statDebugf3("[Parachute] RegisterPreOpenParachute");
	sm_HasStartedParachuteDeploy = true;
}


void CStatsMgr::RegisterDestroyParachute(const CPed* pPed)
{
	statDebugf3("[Parachute] RegisterDestroyParachute");

	sm_hasBailedFromParachuting = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsInTheAir);

	if(sm_hasBailedFromParachuting)
	{
		statDebugf3("[Parachute] RegisterDestroyParachute : Bailing from parachute");

		Vec3V coords = pPed->GetTransform().GetPosition();

		const float groundZ  = CGameWorldHeightMap::GetMinHeightFromWorldHeightMap(coords.GetXf(), coords.GetYf());
		const float altitude = coords.GetZf() - groundZ;

		sm_ParachuteStartTime = 0;
		sm_HasStartedParachuteDeploy = false;

		if (altitude > sm_TuneMetadata.m_FreefallThresold)
		{
			sm_WasInAir = true;
			sm_FreeFallStartPos = VEC3V_TO_VECTOR3(coords);
			statDebugf3("[Parachute] RegisterDestroyParachute : Start Freefall Position <x=%f,y=%f,z=%f> altitude=<%f>", sm_FreeFallStartPos.x, sm_FreeFallStartPos.y, sm_FreeFallStartPos.z, altitude);

			sm_FreeFallStartTimer = fwTimer::GetTimeInMilliseconds();
			sm_FreeFallDueToPlayerDamage = false;

			if (pPed->GetWeaponDamageEntity())
			{
				const u32 timeSinceLastDamage = fwTimer::GetTimeInMilliseconds() - pPed->GetWeaponDamagedTime();
				if (timeSinceLastDamage <= sm_TuneMetadata.m_FreefallTimeSinceLastDamage)
				{
					sm_FreeFallDueToPlayerDamage = true;
				}
			}
		}
	}
}

void CStatsMgr::RegisterStartSkydiving(CPed* pPed)
{
	if (!CStatsUtils::IsStatsTrackingEnabled())
		return;

	if(!IsPlayerActive())
		return;

	statAssert(pPed);
	if (!pPed)
		return;

	if (!pPed->IsLocalPlayer())
		return;

	if (!pPed->GetIsParachuting())
		return;

	if(sm_ChallengeSkydiveStartPos.IsZero())
	{
		sm_ChallengeSkydiveStartPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	}
	statDebugf3("Start Skydive Position <x=%f,y=%f,z=%f> ", sm_ChallengeSkydiveStartPos.x, sm_ChallengeSkydiveStartPos.y, sm_ChallengeSkydiveStartPos.z);
}


void CStatsMgr::FallenThroughTheMap(CPed* pPed)
{
	statAssert(pPed);
	if (!pPed)
		return;
	if (!pPed->IsLocalPlayer())
		return;

	ResetRecordingStats();
}

//True when the player is inside a Train
static bool s_IsInsideTrain = false;
//True when the player on a Train
static bool s_IsOnTrain = false;
//True when the player on a Boat
static bool s_IsOnBoat = false;
//True when the player on a Car
static bool s_IsOnCar = false;

bool 
CStatsMgr::IsInsideTrain( )
{
	return s_IsInsideTrain;
}

bool
CStatsMgr::IsOnTrain()
{
	return s_IsOnTrain;
}

bool
CStatsMgr::IsOnCar()
{
	return s_IsOnCar;
}

bool
CStatsMgr::IsOnBoat()
{
	return s_IsOnBoat;
}

void 
CStatsMgr::ProcessOnFootVehicleChecks(CPed* playerPed)
{
	if (!CStatsUtils::IsStatsTrackingEnabled())
		return;

	static u32 s_PreviousFrame = 0;

	if (playerPed && s_PreviousFrame != fwTimer::GetFrameCount())
	{
		s_IsInsideTrain = false;

		CPedIntelligence* pPI = playerPed->GetPedIntelligence();
		if (pPI)
		{
			CQueriableInterface* pQI = pPI->GetQueriableInterface();
			if(pQI && pQI->IsTaskCurrentlyRunning(CTaskTypes::TASK_PLAYER_ON_FOOT)) 
			{
				const CTaskPlayerOnFoot* playerOnFootTask = static_cast<const CTaskPlayerOnFoot*>(pPI->FindTaskActiveByType(CTaskTypes::TASK_PLAYER_ON_FOOT)); 
				if(playerOnFootTask && playerOnFootTask->GetState() == CTaskPlayerOnFoot::STATE_RIDE_TRAIN)
				{
					s_IsInsideTrain = true;
				}
			}
		}

		s_IsOnTrain = false;
		s_IsOnBoat  = false;
		s_IsOnCar   = false;
		const CEntity* entity = playerPed->GetGroundPhysical();


		if(!entity)
		{
			entity = (const CEntity *)playerPed->GetAttachParent();
		}

		if (entity && entity->GetIsPhysical())
		{
			const CPhysical* physical = static_cast<const CPhysical *>(entity);
			if(physical->GetIsTypeVehicle())
			{
				const CVehicle* vehicle = static_cast<const CVehicle *>(physical);

				// If we're on a moving vehicle for more than 1 second, reset the start position for the challenge "highest altitude on foot"
				static const float MIN_VEHICLE_SPEED2 = 2.0f;
				static const float SECONDS_BEFORE_POSITION_RESET = 1000.0f;
				if(vehicle->GetVelocity().Mag2()>MIN_VEHICLE_SPEED2)
				{
					sm_FootOnVehicleTimer+=fwTimer::GetTimeStepInMilliseconds();
					if(sm_FootOnVehicleTimer>SECONDS_BEFORE_POSITION_RESET)
					{
						statDebugf3("Spent too much time on a moving vehicle, reset onFootStartPosition");
						sm_OnFootPosStart.Zero();
					}
				}

				Vector3 currentPosition = VEC3V_TO_VECTOR3(vehicle->GetTransform().GetPosition());
				if (sm_VehiclePreviousPosition.IsZero())
				{
					sm_VehiclePreviousPosition = currentPosition;
				}

				const float distanceTravelled = Mag(vehicle->GetTransform().GetPosition() - VECTOR3_TO_VEC3V(sm_VehiclePreviousPosition)).Getf();

				if(vehicle->InheritsFromTrain())
				{
					s_IsOnTrain = true;
					if (vehicle->GetVelocity().Mag2() >= MIN_SPEED_TO_CONSIDER_MOVEMENT)
					{
						CStatsMgr::IncrementStat(STAT_DIST_AS_PASSENGER_TRAIN.GetStatId(), distanceTravelled, STATUPDATEFLAG_ASSERTONLINESTATS);
					}
				}
				else if (vehicle->InheritsFromBoat())
				{
					s_IsOnBoat = true;
				}
				else if (vehicle->InheritsFromAutomobile())
				{
					s_IsOnCar = true;
				}

				sm_VehiclePreviousPosition = currentPosition;
			}
			else
			{
				sm_FootOnVehicleTimer=0.0f;
			}
		}

		s_PreviousFrame = fwTimer::GetFrameCount();
	}
}

#if __ASSERT

const atHashWithStringNotFinal WEAPONTYPE_DIGISCANNER("WEAPON_DIGISCANNER",0xFDBADCED);
const atHashWithStringNotFinal WEAPONTYPE_REMOTESNIPER("WEAPON_REMOTESNIPER",0x33058E22);
const atHashWithStringNotFinal WEAPONTYPE_BZGAS("WEAPON_BZGAS",0xa0973d5e);
const atHashWithStringNotFinal WEAPONTYPE_ELECTRIC_FENCE("WEAPON_ELECTRIC_FENCE",0x92BD4EBB);

const StatId_char STAT_ANNIHL_BULLET_SHOTS("ANNIHL_BULLET_SHOTS", false);
const StatId_char STAT_WEAPON_HIT_BY_WATER_CANNON("WEAPON_HIT_BY_WATER_CANNON", false);

void 
CStatsMgr::MissingWeaponStatName(const u32 hashType, StatId& stat, const u32 weaponHash, const u32 damageType, const char* weaponname)
{
	if (CScriptHud::bUsingMissionCreator)
	{
		return;
	}

	if (STATTYPE_WEAPON_HELDTIME == hashType && weaponHash == WEAPONTYPE_UNARMED)
	{
		//Exception
	}
	else if (STATTYPE_WEAPON_SHOTS == hashType && weaponHash == WEAPONTYPE_UNARMED)
	{
		//Exception
	}
	else if (STATTYPE_WEAPON_HITS == hashType && damageType == DAMAGE_TYPE_EXPLOSIVE)
	{
		//Exception for punching people while holding a grenade or some sort of explosive
	}
	else if (stat.GetHash() == STAT_ANNIHL_BULLET_SHOTS.GetHash())
	{
		//Exception
	}
	else if (stat.GetHash() == STAT_WEAPON_HIT_BY_WATER_CANNON.GetHash())
	{
		//Exception
	}
	else if(stat.GetHash() == atHashWithStringNotFinal("MP0_ROCKET_KILLS", 0x9022A747))
	{
		//Exception
	}
	else if (statVerify(weaponname)
				&& weaponHash != WEAPONTYPE_DROWNING.GetHash()
				&& weaponHash != WEAPONTYPE_DROWNINGINVEHICLE.GetHash()
				&& weaponHash != WEAPONTYPE_EXPLOSION.GetHash()
				&& weaponHash != WEAPONTYPE_FALL.GetHash()
				&& weaponHash != WEAPONTYPE_FIRE.GetHash()
				&& weaponHash != WEAPONTYPE_RAMMEDBYVEHICLE.GetHash()
				&& weaponHash != WEAPONTYPE_RUNOVERBYVEHICLE.GetHash()
				&& weaponHash != WEAPONTYPE_DIGISCANNER.GetHash()
				&& weaponHash != WEAPONTYPE_GOLFCLUB.GetHash()
				&& weaponHash != WEAPONTYPE_REMOTESNIPER.GetHash()
				&& weaponHash != WEAPONTYPE_BZGAS.GetHash()
				&& weaponHash != WEAPONTYPE_ELECTRIC_FENCE.GetHash())
	{
		if (hashType == STATTYPE_WEAPON_HEADSHOTS.GetHash())
		{
			if (!(weaponHash == WEAPONTYPE_GRENADELAUNCHER.GetHash() || weaponHash == WEAPONTYPE_DLC_RAILGUN.GetHash() || weaponHash == WEAPONTYPE_VEHICLE_PLAYER_BULLET.GetHash()))
			{
				statAssertf(damageType <= DAMAGE_TYPE_NONE || damageType >= NUM_EDAMAGETYPE, "Missing Weapon \"%s\" with stat named \"%s\".", weaponname, stat.GetName());
			}
		}
		else
		{
			statAssertf(damageType <= DAMAGE_TYPE_NONE || damageType >= NUM_EDAMAGETYPE, "Missing Weapon \"%s\" with stat named \"%s\" and damage type \"%u\".", weaponname, stat.GetName(), damageType);
		}
	}
}


#endif // __ASSERT


void CStatsMgr::StartBailVehicle(CPed* playerPed)
{	
	// Only start/reset bail stats variables if the local player bails, not any other peds/players
	if (playerPed && playerPed->IsLocalPlayer())
	{
		CVehicle * playerVehicle = playerPed->GetMyVehicle();
		if (playerVehicle != NULL)
		{
			if (!playerVehicle->GetIsLandVehicle())
			{
				return;
			}
		}

		sm_CurrentSpeed = 0.0f;

		if (g_PlayerSwitch.IsActive()) // We don't want to register bails from a vehicle if we are in sky cam
			return;

		if(sm_StartVehicleBail.IsZero())
        {
		    sm_VehicleBailDistance = 0.0f;
		    sm_StartVehicleBail = VEC3V_TO_VECTOR3(playerPed->GetTransform().GetPosition());
		    sm_LastVehicleBailPosition = sm_StartVehicleBail;
		    statDebugf3("Bailing out of the vehicle at position <x=%f,y=%f,z=%f>", sm_StartVehicleBail.x, sm_StartVehicleBail.y, sm_StartVehicleBail.z);
        }
	}
}

bool CStatsMgr::ShouldCountKills()
{
	// Since url:bugstar:7546782, we only count kills in instanced content
	if (CountKillsOnlyInInstancedContent())
	{
		const bool isInstancedContent = CNetwork::GetNetworkSession().IsActivitySession();
		return isInstancedContent;
	}
	return true;
}

bool CStatsMgr::CountKillsOnlyInInstancedContent()
{
	bool onlyInInstancedContent = true;
	::Tunables::GetInstance().Access(CD_GLOBAL_HASH, ATSTRINGHASH("COUNT_KILLS_ONLY_IN_INSTANCED_CONTENT", 0x57F41BE0), onlyInInstancedContent);
	return onlyInInstancedContent;
}

void CStatsMgr::UpdateBoat(CVehicle* playerVehicle)
{
	if(!statVerify(playerVehicle))
		return;
	if(!statVerifyf(playerVehicle->InheritsFromBoat(), "Vehicle is supposed to inherit from Boat"))
		return;

	CBoat* boat = static_cast<CBoat*>(playerVehicle);

	bool isInAir = playerVehicle->GetFrameCollisionHistory()->GetNumCollidedEntities()==0;

	// If we're dropped from a helicopter or something similar, doesnt count as beaching
	if(sm_HadAttachedParentVehicle)
	{
		sm_wasInWaterPreviously=false;
		sm_HadAttachedParentVehicle=false;
	}

	if(sm_wasInWaterPreviously && !boat->GetWasInWater())
	{
		sm_outOfWater=true;
		sm_LastBoatPositionInWater = VEC3V_TO_VECTOR3(playerVehicle->GetTransform().GetPosition());
	}
	// If we're not in water anymore, lets wait until we hit the ground to start counting
	if(sm_outOfWater && sm_BoatPositionOnGround.IsZero() && !isInAir)
	{
		sm_BoatPositionOnGround = VEC3V_TO_VECTOR3(playerVehicle->GetTransform().GetPosition());
		sm_inAirAfterGroundTimer=0.0f;
	}

	// If we're not in water anymore, we hit the ground before but we're in the air right after (we might just have bounced on the shore, doesnt count as beaching the boat)
	if(sm_outOfWater && !sm_BoatPositionOnGround.IsZero())
	{
		if(isInAir)
		{
			sm_inAirAfterGroundTimer+=fwTimer::GetTimeStepInMilliseconds();
		}
		else
		{
			sm_inAirAfterGroundTimer=0.0f;
		}
	}

	// Back in the water
	if(sm_outOfWater && playerVehicle->GetWasInWater())
	{
		sm_BoatPositionOnGround.Zero();
		sm_BoatDistanceOnGround = 0.0f;
		sm_outOfWater=false;
		sm_inAirAfterGroundTimer=0.0f;
	}
	// Spent too much time in the air, doesnt count as beaching
	static const float TIME_IN_AIR_THRESHOLD = 1000.0f;
	if(sm_inAirAfterGroundTimer>TIME_IN_AIR_THRESHOLD)
	{
		statDebugf3("Spent more than %.2f ms in the air, beaching doesnt count", TIME_IN_AIR_THRESHOLD);
		sm_BoatPositionOnGround.Zero();
		sm_BoatDistanceOnGround = 0.0f;
		sm_outOfWater=false;
		sm_inAirAfterGroundTimer=0.0f;
	}
	else if(!sm_BoatPositionOnGround.IsZero())
	{
		// we're still on the ground
		sm_BoatDistanceOnGround = Mag(playerVehicle->GetTransform().GetPosition()-VECTOR3_TO_VEC3V(sm_LastBoatPositionInWater)).Getf();
		UpdateRecordedStat(REC_STAT_DIST_BEACHING_BOAT, sm_BoatDistanceOnGround);
	}
	sm_wasInWaterPreviously = boat->GetWasInWater();
}

void CStatsMgr::UpdateRecordedStat(CStatsMgr::RecordStat stat, float value)
{
	if(m_recordedStat != stat)
		return;
	Assertf(m_statRecordingPolicy != RSP_NONE, "Policy should be set");

	if(!FPIsFinite(m_recordedStatValue))
	{
		m_recordedStatValue = value;
		statDebugf3("Recorder : UpdateRecordedStat, new value : %f", m_recordedStatValue);
		return;
	}
	Assertf(FPIsFinite(m_recordedStatValue), "The value should already have been set");

	if(m_statRecordingPolicy == RSP_SUM)
	{
		m_recordedStatValue += value;
		statDebugf3("Recorder : UpdateRecordedStat, new value : %f", m_recordedStatValue);
	}
	else if(m_statRecordingPolicy == RSP_GREATEST)
	{
		if(m_recordedStatValue < value)
		{
			m_recordedStatValue = value;
			statDebugf3("Recorder : UpdateRecordedStat, new value : %f", m_recordedStatValue);
		}
	}
	else if(m_statRecordingPolicy == RSP_LOWEST)
	{
		if(m_recordedStatValue > value)
		{
			m_recordedStatValue = value;
			statDebugf3("Recorder : UpdateRecordedStat, new value : %f", m_recordedStatValue);
		}
	}
	ASSERT_ONLY(else Assertf(0, "Cannot process a new value when no timeframe is started");)
}

bool CStatsMgr::GetRecordedStatValue(float& value)
{
	if(FPIsFinite(m_recordedStatValue))
	{
		value = m_recordedStatValue;
		return true;
	}
	return false;
}



void CStatsMgr::StartRecordingStat(RecordStat stat, RecordStatPolicy policy)
{
	statAssertf(policy != RSP_NONE, "Cannot record with a policy set to NONE");
	m_recordedStat = stat;
	m_statRecordingPolicy = policy;
	MakeNan(m_recordedStatValue);
	statDebugf3("Recorder : StartRecordingStat stat %d", (int)stat);

	ResetRecordingStats();
}

void CStatsMgr::ResetRecordingStats()
{
	statDebugf3("Recorder : ResetRecordingStats");
	sm_JumpDistance            = 0.0f;
	sm_LastJumpHeight          = 0.0f;
	sm_CurrentSpeed            = 0.0f;
	sm_BikeRearWheelDist       = 0.0f;
	sm_IsWheelingOnPushBike    = false;
	sm_IsDoingAStoppieOnPushBike = false;
	sm_BikeFrontWheelDist      = 0.0f;
	sm_DriveNoCrashTime		   = 0.0f;
	sm_DriveNoCrashDist        = 0.0f;
	sm_VehicleFlipsAccumulator = 0;
	sm_VehicleSpinsAccumulator = 0.0f;
    sm_VehicleSpinsAccumulator = 0.0f;
    sm_VehicleRollsAccumulator   = 0.0f;
    sm_VehicleRollsHeadingLast   = 0.0f;
	sm_NearMissNoCrash         = 0;
	sm_PlaneBarrelRollCounter  = 0.0f;
	sm_FlyingCounter           = 0.0f;
	sm_FlyingDistance		   = 0.0f;
	sm_OnFootPosStart.Zero();
	sm_FootAltitude			  = 0.0f;
	sm_PlayersVehiclesDamages = 0.0f;
	sm_VehicleBailDistance    = 0.0f;
	sm_CurrentDrivingReverseDistance = 0.0f;
	sm_NearMissNoCrashPrecise = 0;
	sm_StartVehicleBail.Zero();
	sm_FlyingAltitude         = 0.0f;
	sm_ValidFlyingAltitude    = false;
	sm_HasStartedParachuteDeploy = false;
    sm_FreeFallAltitude = 0.0f;
    sm_LastWallridePosition.Zero();
    sm_WallrideDistance = 0.0f;
	ClearNearMissArray();
}

void CStatsMgr::StopRecordingStat()
{
	float val = 0.0f;
	if(GetRecordedStatValue(val))
	{
		statDebugf3("Recorder : StopRecordingStat, value = %f", val);
	}
	else
	{
		statDebugf3("Recorder : StopRecordingStat, nothing has been recorded");
	}
	m_statRecordingPolicy = RSP_NONE;
	m_recordedStat = REC_STAT_NONE;
	ResetRecordingStats();
}

bool CStatsMgr::IsRecordingStat()
{
	return m_statRecordingPolicy != RSP_NONE;
}

void CStatsMgr::StartTrackingStunts()
{
    statDebugf3("Start tracking stunts");
    s_isTrackingStunts = true;
}

void CStatsMgr::StopTrackingStunts()
{
    statDebugf3("Stop tracking stunts");
    s_isTrackingStunts = false;
}

void CStatsMgr::ClearNearMissArray()
{
	for(unsigned i = 0; i < MAXIMUM_NEAR_MISS_COUNTER; i++)
	{
		sm_NearMissCounterArray[i] = 0;
	}
}

void CStatsMgr::AddNearMissTimeToArray(u32 nearMissTime)
{
	for(unsigned i = 0; i < MAXIMUM_NEAR_MISS_COUNTER; i++)
	{
		if(sm_NearMissCounterArray[i] == 0)
		{
			sm_NearMissCounterArray[i] = nearMissTime;
			return;
		}
	}
}

#if __BANK

#define MAX_DISPATCH_DRAWABLES 100
static CDebugDrawStore ms_debugDraw(MAX_DISPATCH_DRAWABLES);

void CStatsMgr::drawNonFlyableAreas()
{
	for(int i = 0; i < sm_TuneMetadata.m_nonFlyableAreas.m_areas.size(); i++)
	{
		const NonFlyableArea& area = sm_TuneMetadata.m_nonFlyableAreas.m_areas[i];
		const ::rage::Vector4& rect = area.m_rectXYWH;
		ms_debugDraw.AddVectorMapLine( Vec3V(rect.x, rect.y, 0.0f), Vec3V(rect.x+rect.w, rect.y, 0.0f), Color_red, 100);
		ms_debugDraw.AddVectorMapLine( Vec3V(rect.x+rect.w, rect.y, 0.0f), Vec3V(rect.x+rect.w, rect.y+rect.z, 0.0f), Color_red, 100);
		ms_debugDraw.AddVectorMapLine( Vec3V(rect.x+rect.w, rect.y+rect.z, 0.0f), Vec3V(rect.x, rect.y+rect.z, 0.0f), Color_red, 100);
		ms_debugDraw.AddVectorMapLine( Vec3V(rect.x, rect.y+rect.z, 0.0f), Vec3V(rect.x, rect.y, 0.0f), Color_red, 100);

	}
	ms_debugDraw.Render();
}

#endif // __BANK

// eof
