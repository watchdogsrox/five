
/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    playerinfo.h
// PURPOSE : Specific information for each player is stored here.
// AUTHOR :  Obbe
// CREATED : 3-11-99
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef INC_PLAYERINFO_H_
#define INC_PLAYERINFO_H_

// Rage headers
#include "rline/rlgamerinfo.h"
#include "output/constantlighteffect.h"
#include "output/flashinglighteffect.h"

// Framework headers
#include "cranimation/frame.h"
#include "entity/extensiblebase.h"
#include "fwlight/wantedlighteffect.h"
#include "fwsys/timer.h"
#include "fwtl/pool.h"
#include "spatialdata/aabb.h"

// Game headers
#include "game/wanted.h"
#include "peds/PedWeapons/PlayerPedTargeting.h"
#include "scene/RegdRefTypes.h"
#include "system/control.h"
#include "task/Combat/Cover/Cover.h"
#include "task/Combat/Cover/TaskCover.h"
#include "Task/System/Tuning.h"
#include "task/System/TaskHelpers.h"
#include "debug/Debug.h"
#include "Task/Vehicle/PlayerHorseSpeedMatch.h"
#include "Task/Movement/JetpackObject.h"
#include "Peds/Horse/horseDefines.h"

#include "Peds/ArcadeAbilityEffects.h"
#include "Peds/PlayerArcadeInformation.h"

class CCoverPoint;
class CEntity;
class CPed;
class CPlayerInfo;
class CPopZone;
class CVehicle;
class CPad;
class CAutomobile;
class CObject;
class CPedClothesDesc;

namespace rage
{
	class fwPtrList;
    class netPlayer;
	class phInst;
	class phCollider;
}

#define MAX_PLAYER_NAME_LEN					40
#define NUM_MELEE_ATTACK_POINTS				(6)
#define PLAYER_HS_MISSILE_LOCKON_TIME		1500
#define PLAYER_HS_MISSILE_LOS_TEST_TIME		1000

#define LIGHT_EFFECT_FLASH_FOR_LOW_HEALTH	(0 && LIGHT_EFFECTS_SUPPORT)

static const u8 MAX_NEAR_VEHICLES = 8;	// needs to be pow
static const u8 NEAR_VEHICLE_MASK = MAX_NEAR_VEHICLES - 1;

// how many instances of enemies charging the player to remember
static const u8	CHARGER_HISTORY_SIZE = 4;

// how many instances of enemies throwing smoke at the player to remember
static const u8	SMOKE_THROWER_HISTORY_SIZE = 4;

//Match with WRECKLESS_TYPE in commands_player.sch
enum eWrecklessType
{
	ON_PAVEMENT = 0,
	RAN_RED_LIGHT,
	DROVE_AGAINST_TRAFFIC
};

enum ePlayerSpecialAbilitySlot
{
	PSAS_INVALID = -1,
	PSAS_PRIMARY = 0,
	PSAS_SECONDARY = 1,
	PSAS_MAX = 2
};


// This class just holds the last scanned time and visible or not flag
class CPlayerLOSInfo
{
public:
	CPlayerLOSInfo()
		: m_uTimeLastScanned(0)
		, m_bIsVisible(false)
	{}

	const u32 GetTimeLastScanned() const { return m_uTimeLastScanned; }
	const bool GetIsVisible() const { return m_bIsVisible; }

	void SetTimeLastScanned(u32 uTimeLastScanned) { m_uTimeLastScanned = uTimeLastScanned; }
	void SetIsVisible(bool bIsVisible) { m_bIsVisible = bIsVisible; }

private:
	u32			 m_uTimeLastScanned;
	bool		 m_bIsVisible;
};
extern void ReportPlayerResetMismatch(u32, u32);

class CPlayerResetFlags
{
public:
	enum PlayerResetFlags
	{
		PRF_NOT_ALLOWED_TO_ENTER_ANY_CAR	= (1<<0),
		PRF_ASSISTED_AIMING_ON				= (1<<1),
		PRF_FORCED_ZOOM						= (1<<2),
		PRF_FORCE_PLAYER_INTO_COVER			= (1<<3),
		PRF_FORCED_AIMING					= (1<<4),
		PRF_DISABLE_HEALTH_RECHARGE			= (1<<5),
		PRF_FORCE_SKIP_AIM_INTRO			= (1<<6),
		PRF_DISABLE_AIM_CAMERA				= (1<<7),
		PRF_RUN_AND_GUN						= (1<<8),
		PRF_SKIP_COVER_ENTRY_ANIM			= (1<<9),
		PRF_NO_RETICULE_AIM_ASSIST_ON		= (1<<10),
		PRF_EXPLOSIVE_AMMO_ON				= (1<<11),
		PRF_FIRE_AMMO_ON					= (1<<12),
		PRF_EXPLOSIVE_MELEE_ON				= (1<<13),
		PRF_SUPER_JUMP_ON					= (1<<14),
		PRF_INCREASE_JUMP_SUPPRESSION_RANGE = (1<<15),
		PRF_SPECIFY_INITIAL_COVER_HEADING	= (1<<16),
		PRF_FACING_LEFT_IN_COVER			= (1<<17),
		PRF_USE_COVER_THREAT_WEIGHTING		= (1<<18),
		PRF_DISABLE_DISPATCHED_HELI_REFUEL	= (1<<19),
		PRF_DISABLE_DISPATCHED_HELI_DESTROYED_SPAWN_DELAY = (1<<20),
		PRF_DISABLE_VEHICLE_REWARDS			= (1<<21),
		PRF_PREFER_REAR_SEATS				= (1<<22),
		PRF_PREFER_FRONT_PASSENGER_SEAT		= (1<<23),
		PRF_DISABLE_CAMERA_VIEW_MODE_CYCLE  = (1<<24),
		PRF_CAMERA_VIEW_MODE_SWITCHED_TO_OR_FROM_FIRST_PERSON = (1<<25),
		PRF_CAMERA_VIEW_MODE_SWITCHED_TO_FIRST_PERSON = (1<<26),
		PRF_DISABLE_CAN_USE_COVER 			= (1<<27),
		PRF_BEAST_JUMP_ON					= (1<<28),
		PRF_FORCED_JUMP						= (1<<29),
		PRF_DOT_SHOCKED						= (1<<30),
		PRF_DOT_CHOKING						= (1<<31)
	};

	CPlayerResetFlags(); 
	~CPlayerResetFlags(); 
	
	inline void ResetAllFlags() { m_iPlayerResetFlags = 0; };
	
	void ResetPreAI(); //Reset flags Pre AI update
	void ResetPostAI();	//Reset flag Post AI update		
	void ResetPostPhysics();
#if RSG_PC
	inline void SetFlag(const int iFlag) {m_iPlayerResetFlags.Set(m_iPlayerResetFlags | iFlag);}
	inline void UnsetFlag(const int iFlag) {m_iPlayerResetFlags.Set(m_iPlayerResetFlags & ~iFlag);}
#else
	inline void SetFlag(const int iFlag) {m_iPlayerResetFlags |= iFlag;}
	inline void UnsetFlag(const int iFlag) {m_iPlayerResetFlags &= ~iFlag;}
#endif
	inline bool IsFlagSet(const int iFlag) const {return ((m_iPlayerResetFlags & iFlag)) != 0;}
private:

#if RSG_PC
	sysLinkedData<u32, ReportPlayerResetMismatch> m_iPlayerResetFlags; 
#else
	u32 m_iPlayerResetFlags;
#endif
};

// Class managing player health recharge
class CPlayerHealthRecharge
{
public:

	CPlayerHealthRecharge()
		: m_uLastTimeHealthDamageTaken(0U)
		, m_fRechargeScriptMultiplier (1.0f)
		, m_fMaxHealthToRechargeAsPercentage (0.5f)
	{

	}
	void Process(CPed* pPed);

	void NotifyHealthDamageTaken();

	float GetRechargeScriptMultiplier() const { return m_fRechargeScriptMultiplier; }
	void SetRechargeScriptMultiplier(float val) { m_fRechargeScriptMultiplier = val; }

	float GetRechargeMaxPercent() const { return m_fMaxHealthToRechargeAsPercentage; }
	void SetRechargeMaxPercent(float val) { m_fMaxHealthToRechargeAsPercentage = val; }

private:
	float m_fRechargeScriptMultiplier;
	float m_fMaxHealthToRechargeAsPercentage;
	u32 m_uLastTimeHealthDamageTaken;

public:
	static bool		ms_bActivated;
	static float	ms_fRechargeSpeed;
	static float	ms_fRechargeSpeedWhileCrouchedOrCover;
	static float	ms_fTimeSinceDamageToStartRecharding;
	static float	ms_fTimeSinceDamageToStartRechardingCrouchedOrCover;
};

// Class managing player endurance
class CPlayerEnduranceManager
{
public:

	CPlayerEnduranceManager() :
		m_uLastTimeEnduranceDamageTaken(0U),
		m_uLastTimeEnduranceRecovered(0U)
	{
	}

	void Process(CPed* pPed);
	void NotifyEnduranceDamageTaken();
	void NotifyEnduranceRecovered();
	bool ShouldIgnoreEnduranceDamage() const;

private:
	void ProcessRegen(CPed* pPed);
	void ProcessEnduranceOverlayFX(CPed* pPed);
	u32 m_uLastTimeEnduranceDamageTaken;
	u32 m_uLastTimeEnduranceRecovered;
};

// Class managing player damage over time effects
class CPlayerDamageOverTime
{
	struct DOTEffect
	{
		u32 m_uWeaponHash;
		u32 m_uLastDamageTime;
		u32 m_uExpiryTime;
		RegdPed m_OriginalInflicter;
		RegdPed m_LatestInflicter;
	};

public:

	CPlayerDamageOverTime(){}

	void Process(CPed* pPed);
	void Start(u32 uWeaponHash, CPed* pVictimPed, CEntity* pInflicterEntity);
	void Stop(u32 uWeaponHash);
	void StopAll();
	bool IsActive(u32 uWeaponHash);

#ifdef AI_DEBUG_OUTPUT_ENABLED
	u32 GetCount() { return m_DOTEffects.GetCount(); }
	atString GetDebugString(s32 iEffectIndex);
#endif //AI_DEBUG_OUTPUT_ENABLED

private:
	void StartAdditionalEffects(CPed* pPed, const CWeaponInfo* pWeaponInfo);
	void ProcessAdditionalEffects(CPed* pPed, const CWeaponInfo* pWeaponInfo);

	static const int MAX_ENTRIES = 6;
	atFixedArray<DOTEffect, MAX_ENTRIES> m_DOTEffects;

	static const int MAX_STACK = 3;
};

//******************************************************************************************
// eSetPlayerControlFlags - A set of flags to determine the effects of SetPlayerControls()
// These must match the SET_PLAYER_CONTROL_FLAGS script enum in "commands_player.sch" even
// although that enumeration is a subset of the one below (not all are exposed to script).

enum eSetPlayerControlFlags
{
	// disables/enables the player's controls
	SPC_DisableControls			= 1,
	// undocumented - presumably whether this was called by an ambient script
	SPC_AmbientScript			= 2,
	// also clears the player's task
	SPC_ClearTasks				= 4,
	// removes fires in the vicinity of the player
	SPC_RemoveFires				= 8,
	// removes explosions in the vicinity of the player
	SPC_RemoveExplosions		= 16,
	// removes all projectiles
	SPC_RemoveProjectiles		= 32,
	// deactivates all the player's gadgets
	SPC_DeactivateGadgets		= 64,
	// reenables the player's controls if they die whilst not under control
	SPC_ReEnableControlOnDeath	= 128,
	// leaves camera controls on
	SPC_LeaveCameraControlsOn	= 256,
	//Allows the player to be damaged even if control is off. 
	SPC_AllowPlayerToBeDamaged	= 512,
	//Tells nearby vehicles to stop moving or get out of the way
	SPC_DontStopOtherCarsAroundPlayer = 1024,
	// Prevent everybody from backing off when player controls are disabled
	SPC_PreventEverybodyBackoff = 2048,
	// Disables pad shake
	SPC_AllowPadShake			= 4096,
};

// ------------------------------------------------------------------------------

// ------------------------------------------------------------------------------

class CPlayerInfo : public fwExtensibleBase
{
public:

	FW_REGISTER_CLASS_POOL(CPlayerInfo);

	static CDynamicCoverHelper ms_DynamicCoverHelper;

	enum State
	{
        PLAYERSTATE_INVALID     = -1,
		PLAYERSTATE_PLAYING,
		PLAYERSTATE_HASDIED,
		PLAYERSTATE_HASBEENARRESTED,
		PLAYERSTATE_FAILEDMISSION,
		PLAYERSTATE_LEFTGAME,
		PLAYERSTATE_RESPAWN,
		PLAYERSTATE_IN_MP_CUTSCENE,
		PLAYERSTATE_MAX_STATES
	};
	enum { PLAYER_DEFAULT_MAX_HEALTH = 200, PLAYER_DEFAULT_MAX_ARMOUR = 100, NETWORK_PLAYER_DEFAULT_JACK_SPEED = 90 };

	enum eMultiplayerTeam
	{
		MP0_CHAR_TEAM = 0,
		MP1_CHAR_TEAM,
		MP2_CHAR_TEAM,
		MP3_CHAR_TEAM,
		MP4_CHAR_TEAM
	};

	struct sPortablePickupInfo
	{
		int			modelIndex;
		u16			maxPermitted;
		u16			numCarried;
	};

	// PURPOSE: Stores player stat min/max values
	struct sPlayerStatInfo
	{
		virtual ~sPlayerStatInfo() {}

		atHashWithStringNotFinal m_Name;
		float m_MinStaminaDuration;
		float m_MaxStaminaDuration;
		float m_MinHoldBreathDuration;
		float m_MaxHoldBreathDuration;
		float m_MinWheelieAbility;
		float m_MaxWheelieAbility;
		float m_MinPlaneControlAbility;
		float m_MaxPlaneControlAbility;
		float m_MinPlaneDamping;
		float m_MaxPlaneDamping;
		float m_MinHeliDamping;
		float m_MaxHeliDamping;
		float m_MinFallHeight;
		float m_MaxFallHeight;
		float m_MinDiveHeight;
		float m_MaxDiveHeight;
		float m_DiveRampPow;

#if __BANK
		void PreAddWidgets(bkBank& bank);
		void PostAddWidgets(bkBank& bank);
#endif // __BANK

		PAR_PARSABLE;
	};

	enum ePlayerStatTypes
	{
		PT_SP0,
		PT_SP1,
		PT_SP2,
		PT_MP0_CHAR_TEAM,
		PT_MP1_CHAR_TEAM,
		PT_MP2_CHAR_TEAM,
		PT_MP3_CHAR_TEAM,
		PT_MP4_CHAR_TEAM,
		PT_MAX
	};

	enum eSprintType
	{
		SPRINT_ON_FOOT = 0,
		SPRINT_ON_BICYCLE,
		SPRINT_ON_WATER,
		SPRINT_UNDER_WATER,
		SPRINT_TYPE_MAX
	};

	struct sSprintControlData
	{
		virtual ~sSprintControlData() {};

		float m_TapAdd;
		float m_HoldSub;
		float m_ReleaseSub;
		float m_Threshhold;
		float m_MaxLimit;
		float m_ResultMult;

		PAR_PARSABLE;
	};

	// PURPOSE:	Tuning parameter struct.
	struct Tunables : public CTuning
	{
		struct EnemyCharging
		{
			EnemyCharging()
			{}

			float m_fChargeGoalBehindCoverCentralOffset;
			float m_fChargeGoalLateralOffset;
			float m_fChargeGoalRearOffset;
			float m_fChargeGoalMaxAdjustRadius;
			float m_fPlayerMoveDistToResetChargeGoals;

			PAR_SIMPLE_PARSABLE;
		};

		struct CombatLoitering
		{
			CombatLoitering()
			{}

			float m_fPlayerMoveDistToResetLoiterPosition;
			u32 m_uDistanceCheckPeriodMS;

			PAR_SIMPLE_PARSABLE;
		};

		struct EnduranceManagerSettings
		{
			bool m_CanEnduranceIncapacitate;
			u32 m_MaxRegenTime;
			u32 m_RegenDamageCooldown;

			PAR_SIMPLE_PARSABLE;
		};

		Tunables();

		EnemyCharging m_EnemyCharging;
		CombatLoitering m_CombatLoitering;
		EnduranceManagerSettings m_EnduranceManagerSettings;
		float												m_MinVehicleCollisionDamageScale;
		float												m_MaxVehicleCollisionDamageScale;
		float												m_MaxAngleConsidered;
		float												m_MinDotToConsiderVehicleValid;
		float												m_MaxDistToConsiderVehicleValid;
		float 												m_SprintReplenishFinishedPercentage;
		float 												m_SprintReplenishFinishedPercentageBicycle;
		float 												m_SprintReplenishRateMultiplier;
		float 												m_SprintReplenishRateMultiplierBike;
		float												m_MaxWorldLimitsPlayerX;
		float												m_MaxWorldLimitsPlayerY;
		float												m_MinWorldLimitsPlayerX;
		float												m_MinWorldLimitsPlayerY;
		float												m_MaxTimeToTrespassWhileSwimmingBeforeDeath;
		float												m_MovementAwayWeighting;
		float												m_DistanceWeighting;
		float												m_HeadingWeighting;
		float												m_CameraWeighting;
		float												m_DistanceWeightingNoStick;
		float												m_HeadingWeightingNoStick;
		float												m_OnFireWeightingMult;
		float												m_BikeMaxRestoreDuration;
		float												m_BikeMinRestoreDuration;
		float												m_BicycleDepletionMinMult;
		float												m_BicycleDepletionMidMult;
		float												m_BicycleDepletionMaxMult;
		float												m_BicycleMinDepletionLimit;
		float												m_BicycleMidDepletionLimit;
		float												m_BicycleMaxDepletionLimit;
		u32													m_TimeBetweenSwitchToClearTasks;
		u32													m_TimeBetweenShoutTargetPosition;
		atHashString										m_TrespassGuardModelName;
		bool												m_GuardWorldExtents;
		atFixedArray<sSprintControlData, SPRINT_TYPE_MAX>	m_SprintControlData;
		atFixedArray<sPlayerStatInfo, PT_MAX>				m_PlayerStatInfos;
		float												m_ScanNearbyMountsDistance;
		float												m_MountPromptDistance;
		bool												m_bUseAmmoCaching;

		PAR_PARSABLE;
	};

	// PURPOSE: Instance of tunable params
	static Tunables	ms_Tunables;

	// Cloud tunables
	static s32 ms_EnduranceMaxRegenTime;
	static s32 ms_EnduranceRegenDamageCooldown;
	static float ms_fStaminaDepletionRateModifierCNC;
	static float ms_fStaminaReplenishRateModifierCNC;

	static void InitTunables();
	static float GetMaxTimeUnderWaterForPed(const CPed& ped);
	static float GetFallHeightForPed(const CPed& ped);
	static float GetDiveHeightForPed(const CPed& ped);
	static float ComputeDurationFromStat(float fCurrentStatPercentage, float fMinDuration, float fMaxDuration);
	static const sPlayerStatInfo& GetPlayerStatInfoForPed(const CPed& ped);
	static bool GetStatValuesForSprintType(const CPed& ped, float& fSprintStatValue, float& fMinSprintDuration, float& fMaxSprintDuration, eSprintType nSprintType);
	static s32 GetPlayerIndex(const CPed& ped);

	static u32 GetEnduranceMaxRegenTime();
	static u32 GetEnduranceRegenDamageCooldown();

	static const unsigned MAX_PORTABLE_PICKUP_INFOS = 20;

	rlGamerInfo			m_GamerInfo;        // all relevant gamer info (presence, peer, id, name, etc.)

	// Polling friends is too costly on large friends list, cache in the player info
	enum
	{
		FRIEND_UNKNOWN = 0,
		FRIEND_NOTFRIEND,
		FRIEND_ISFRIEND,
	};

	int	m_FriendStatus;

	s32		Team;						// The player's team (in network game)
	s32		CollectablesPickedUp;		// How many bags of sugar do we have
	s32		TotalNumCollectables;		// How many bags of sugar are there to be had in the game

	sPortablePickupInfo	PortablePickupsInfo[MAX_PORTABLE_PICKUP_INFOS];	// used to limit the maxium number of a portable pickup type carried

	u32		m_LastTimeHeliWasDestroyed;

	float		m_fForceAirDragMult;	//	Affects the air drag of the player's current car/bike
	float		m_fSwimSpeedMultiplier;
	
//	u32		TimeOfRemoteVehicleExplosion;

	u32		LastTimeEnergyLost;	// To make numbers flash on the HUD
	u32		LastTimeArmourLost;
	u32		TimeSinceSwitch;

//	u32		TimesUpsideDownInARow;		// Make car blow up if car upside down
//	u32		TimesStuckInARow;			// Make car blow up if player cannot get out

	s32		HavocCaused;			// A counter going up when the player does bad stuff.

	float  m_fLastPlayerNearbyTimer; // What the "nearby player timer" was set at most recently in CTaskAmbientClips

	float	m_fPutPedDirectlyIntoCoverNetworkBlendInDuration;

	float	StealthRate;

	u16		CarDensityForCurrentZone;

	u16		m_nLastBustMessageNumber;

	static Vector3	ms_cachedMainPlayerPos;			// cached position which can be safely accessed from other threads

	// 'Special' abilities that gets awarded during the game
	u16		MaxHealth;  // Kev Wong wants to set these to over 255 so changed from UINT8 to UINT16
	u16		MaxArmour;	

	u32		DetonateDownTime; // Timer to disable conflicting inputs whilst detonating.
	static dev_u32 DETONATE_TIMER;
	bool	bExplodedTriggeredProjectilesWithThisButtonPress;
	
	s32			m_sFireProofTimer; // Time for fire proof after recovering from being set on fire 

	u16		JackSpeed;
	s8     m_maxPortablePickupsCarried; // the maximum number of portable pickups of any type the player can carry
	u8	   m_numPortablePickupsCarried; // the current number of portable pickups being carried

	u32 m_uLastTimeStopAiming;
	
	static const int NUMBER_OF_DRIVERS_NEAR_MISS = 5;
	u32 m_nearMissedDriverQueueIndex;
	RegdConstVeh m_lastNearMissedVehicles[NUMBER_OF_DRIVERS_NEAR_MISS];

	////////////////////////////////////////////////// methods
	// constructor
	CPlayerInfo(CPed* ped, const rlGamerInfo* gamerInfo);
	~CPlayerInfo();

	void	MultiplayerReset(); // called when transitioning between MP & SP

	void	SetInitialState(); 

	bool	ProcessControl();
	bool	ProcessPreCamera();
	void	ProcessLightEffects();
#if FPS_MODE_SUPPORTED
	void	ProcessFPSState(bool bIgnoreWeaponWheel = false);
	void	SetCoverHeadingCorrectionAimHeading(float fCoverHeadingCorrectionAimHeading) { m_fCoverHeadingCorrectionAimHeading = fCoverHeadingCorrectionAimHeading; }
	float	GetCoverHeadingCorrectionAimHeading() const { return m_fCoverHeadingCorrectionAimHeading; }
	void	ResetFPSRNGTimer() { m_fRNGTimerFPS = 0.0f; }
	void	ForceFPSRNGState(bool bForce) { m_bForceFPSRNGState = bForce; }
	static bool IsFirstPersonModeSupportedForPed(const CPed& ped);
#endif

#if FPS_MODE_SUPPORTED && KEYBOARD_MOUSE_SUPPORT
	static void SetShouldUpdateScopeStateFromMenu(bool bVal) { sm_bShouldUpdateScopeStateFromMenu = bVal; }
#endif

	void	ResetPreScript();
	
	bool	IsPlayerInRemoteMode(void);

	static CVehicle * ScanForVehicleToEnter(CPed * pPlayerPed, const Vector3& vCarSearchDirection, const bool bUsingStickInput);
	static CPed *	  ScanForAnimalToRide(CPed * pPlayerPed, const Vector3& vAnimalSearchDirection, const bool bUsingStickInput, const bool bDoDepthTest = false);
	static CVehicle * ScanForTrainToEnter(CPed * pPlayerPed, const Vector3& vCarSearchDirection, const bool bUsingStickInput);

	Vector3 GetSpeed(void);
	Vector3 GetPos(void);
	
	bool 	IsRestartingAfterDeath(void);
	bool 	IsRestartingAfterArrest(void);

	void 	KillPlayer( void );
	void 	ArrestPlayer( void );
	void 	ResurrectPlayer( void );
	void 	SetPlayerControl(bool bAmbientScript, bool bDisableControls, float ExtinguishRange = 10000.0f, bool bClearTasks = true, bool bRemoveFiresAndProjectiles = true, bool bAllowDamage = false);
	void	SetPlayerControl(const u32 iFlags, const float fRemovalRange);
	void	AwardMoneyForExplosion(CVehicle *pVictim);
	void 	SetPlayerSkin(const char* pSkinName);
	void 	LoadPlayerSkin();
	void 	DeletePlayerSkin();

	void	ResetHavocCaused(void) { HavocCaused = 0;};
	s32		ReturnHavocCaused(void) { return HavocCaused; };
	void	AddHealth(s32 Amount);

	void    SetLastPlayerNearbyTimer(float fTime)	{ m_fLastPlayerNearbyTimer = fTime; }
	float	GetLastPlayerNearbyTimer() const		{ return m_fLastPlayerNearbyTimer;  }

	void 	SetPlayerPed(CPed	*pPed, bool bClearPedDamage = true);
	CPed*	GetPlayerPed() {return m_pPlayerPed.Get();}
	const CPed* GetPlayerPed() const {return m_pPlayerPed.Get();}

	PhysicalPlayerIndex GetPhysicalPlayerIndex();

	void 	SetLastTargetVehicle(CVehicle* pTargetVehicle);
	const CVehicle* GetLastTargetVehicle() const {return m_pLastTargetVehicle;}
	
	const CVehicleClipRequestHelper& GetVehicleClipRequestHelper() const { return m_VehicleClipRequestHelper; }
	void	ProcessPostMovement();

	void	ProcessTestForShelter();

	// Check if the player has started to wander away from the map and respond appropriately (killing him).
	void	ProcessWorldExtentsCheck();

	// Track the peds who are in combat with this player
	void	ProcessNumEnemiesInCombat();
	
	// Track time spent in cover
	void	ProcessCoverStateTracking();

	// Manage timer and position for loitering in combat
	void	ProcessCombatLoitering();

	// Manage requests by peds to elect best charger for this player
	void	ProcessEnemyElections();

	// Manage potential charge goal positions against this player
	void	ProcessChargingEnemyGoalPositions();

	// Returns whether we have a validated charge goal position.
	bool	HasValidatedChargeGoalPosition() const;

	// Returns whether we have a validated charge goal position. If so the out parameter is set.
	bool	GetValidatedChargeGoalPosition(Vec3V_In in_queryPos, Vec3V_InOut out_ChargeGoalPos) const;

	// Clear charge goal positions
	void	ResetChargingEnemyGoalPositions();

	// Inform the owner ped's group members of our target
	void	ShoutTargetPosition();

	// Gets the time since the player last aimed
	s32	GetTimeSinceLastAimedMS();

	void	SetMaxPortablePickupsOfTypePermitted(int modelIndex, int maxPermitted);
	bool	CanCarryPortablePickupOfType(int modelIndex, bool bAlreadyCollected);
	void	PortablePickupCollected(int modelIndex);
	void	PortablePickupDropped(int modelIndex);

	static bool CanPlayerPedCollectAnyPickups(const CPed& playerPed);

	static bool	FindClosestCarCB(CEntity* pEntity, void* data);
	static bool IsVehicleStuckInWater(const CVehicle* pVehicle);
	static bool CanBoardVehicleWhenStoodOnTop(const CVehicle* pVehicle);
	static bool ShouldDisableEntryForTrain(const CVehicle* pVehicle, const CPed* pPed);
	static bool ShouldDisableEntryForPlane(const CVehicle* pVehicle, const CPed* pPed);
	static bool ShouldDisableEntryForSyncedScene(const CVehicle* pVehicle);
	static bool ShouldDisableEntryBecauseUpsideDown(const CVehicle* pVehicle);
	static bool ShouldDisableEntryDueToHeightCondition(bool& bPassedOriginalHeightCondition, bool bPedIsStandingOnThisVehicle, bool bCanBoardVehicleWhenStoodOnTop, const Vector3& vPedPos, const CPed* pPed, const CVehicle* pVehicle);
	static bool ShouldDisableEntryBecauseVehicleMovingAway(Vector3 vPedVehDiff, const CPed* pPed, const CVehicle* pVehicle);
	static bool FindClosestRidableAnimalCB(CEntity* pEntity, void* data);
	static bool	FindClosestTrainCB(CEntity* pEntity, void* data);
	static void	EvaluateCarPosition(CEntity* pEntity, CPed *pPed, Vector3& vSeatPos, float Distance, float *pCloseness, CVehicle **ppClosestVehicle, Vector3& vCarSearchDirection, const bool bUsingStickInput);
	static void EvaluateAnimalPosition(CEntity* pEntity, CPed *m_pPed, float Distance, float *pCloseness, CPed **ppClosestAnimal, Vector3& vAnimalSearchDirection, const bool bUsingStickInput);

	bool	PlayerIsPissedOff() { return (false); } //to be replaced TF

	bool	GetIsPlayerShelteredFromRain() const { return m_bIsShelteredAbove; }
	void	SetMayOnlyEnterThisVehicle(CVehicle * pVehicle);

			// Functions to detect the player driving recklessly and to retrieve that information
	void	UpdateRecklessDriving();
	void	NearVehicleMiss(const CVehicle* playerVehicle, const CVehicle* otherVehicle, bool trackPreciseNearMiss = false);
  const CVehicle* GetLastNearMissVehicle() const;
  u32     GetTimeSinceLastNearMiss() const;
	u32		GetTimeSincePlayerHitCar();
	u32		GetTimeSincePlayerHitPed();
	u32		GetTimeSincePlayerHitBuilding();
	u32		GetTimeSincePlayerHitObject();
	u32		GetTimeSincePlayerDroveOnPavement();
	u32		GetTimeSincePlayerRanLight();
	u32		GetTimeSincePlayerDroveAgainstTraffic();
	u32		GetTimeLastWeaponPickedUp() const { return m_TimeLastWeaponPickedUp; }
	u32		GetLastWeaponHashPickedUp() const { return m_LastWeaponHashPickedUp; }
	void	SetLastWeaponHashPickedUp(u32 uWeaponHash);

	void	DecayStealthNoise(float timeStep);		// Normally called by CPlayerInfo::Process().
	float	GetStealthNoise() const					{ return m_StealthNoise; }
	void	SetStealthNoise(float noiseRange)		{ m_StealthNoise = noiseRange; }
	void	ReportStealthNoise(float noiseRange);

	float	GetJackRate() const { return static_cast<float>(JackSpeed) / 100.0f;}
	float	GetStealthRate() const { return StealthRate; }

	// Ways to scale the noise reported from a player's actions.

	// Accessor function for the stealth multiplier, which is different depending on the state of the player.
	float	GetStealthMultiplier() const;

	// Modifier used when the player is walking around normally.
	float	GetNormalStealthMultiplier() const			{ return m_fNormalStealthMultiplier; }
	void	SetNormalStealthMultiplier(float mult)		{ m_fNormalStealthMultiplier = mult; }

	// Modifier used when the player is actively using stealth.
	float	GetSneakingStealthMultiplier() const		{ return m_fSneakingStealthMultiplier; }
	void	SetSneakingStealthMultiplier(float mult)	{ m_fSneakingStealthMultiplier = mult; }

	// Set fire proof timer
	void SetFireProofTimer(u32 proofTimeInMS) {m_sFireProofTimer = (s32)proofTimeInMS;}
	void ProcessFireProofTimer();

	// The different things that can disable the controls
	#define	DISABLE_CAMERA			(1<<0)
	#define	DISABLE_DEBUG 			(1<<1)
	#define	DISABLE_GARAGES 		(1<<2)
//	#define	DISABLE_PEDCODE 		(1<<3)
//	#define	DISABLE_FADEOUT		 	(1<<4)
	#define	DISABLE_SCRIPT		 	(1<<5)
//	#define	DISABLE_PHONES		 	(1<<6)
	#define	DISABLE_CUTSCENES	 	(1<<7)
//	#define	DISABLE_SHORTCUT	 	(1<<8)
	#define	DISABLE_LOADINGSCREEN 	(1<<9)
	#define	DISABLE_FRONTEND	 	(1<<10)
	#define DISABLE_SCRIPT_AMBIENT	(1<<11)

	static float SCANNEARBYVEHICLES;

	// Disable/Enable controls
	bool	AreControlsDisabled() const {return DisableControls != 0;}
	bool	AreAnyControlsOtherThanFrontendDisabled();
	void	DisableControlsCamera() { BANK_ONLY(if (!IsControlsCameraDisabled()) { controlDebugf3("DisableControlsCamera. Callstack:"); sysStack::PrintStackTrace(); }); DisableControls |= DISABLE_CAMERA; };
	void	EnableControlsCamera() { BANK_ONLY(if (IsControlsCameraDisabled()) { controlDebugf3("EnableControlsCamera. Callstack:"); sysStack::PrintStackTrace(); }); DisableControls &= ~DISABLE_CAMERA; };
	bool	IsControlsCameraDisabled() const { return(0!=(DisableControls & DISABLE_CAMERA)); };
	void	DisableControlsDebug() { BANK_ONLY(if (!IsControlsDebugDisabled()) { controlDebugf3("DisableControlsDebug. Callstack:"); sysStack::PrintStackTrace(); }); DisableControls |= DISABLE_DEBUG; };
	void	EnableControlsDebug() { BANK_ONLY(if (IsControlsDebugDisabled()) { controlDebugf3("EnableControlsDebug. Callstack:"); sysStack::PrintStackTrace(); }); DisableControls &= ~DISABLE_DEBUG; };
	bool	IsControlsDebugDisabled() const { return(0!=(DisableControls & DISABLE_DEBUG)); };
	void	DisableControlsGarages() { BANK_ONLY(if (!IsControlsGaragesDisabled()) { controlDebugf3("DisableControlsGarages. Callstack:"); sysStack::PrintStackTrace(); }); DisableControls |= DISABLE_GARAGES; };
	void	EnableControlsGarages() { BANK_ONLY(if (IsControlsGaragesDisabled()) { controlDebugf3("EnableControlsGarages. Callstack:"); sysStack::PrintStackTrace(); }); DisableControls &= ~DISABLE_GARAGES; };
	bool	IsControlsGaragesDisabled() const { return(0!=(DisableControls & DISABLE_GARAGES)); };
//	void	DisableControlsPedCode() { DisableControls |= DISABLE_PEDCODE; };
//	void	EnableControlsPedCode() { DisableControls &= ~DISABLE_PEDCODE; };
//	bool	IsControlsPedCodeDisabled() { return((bool)(DisableControls & DISABLE_PEDCODE)); };
//	void	DisableControlsFadeOut() { DisableControls |= DISABLE_FADEOUT; };
//	void	EnableControlsFadeOut() { DisableControls &= ~DISABLE_FADEOUT; };
//	bool	IsControlsFadeOutDisabled() { return((bool)(DisableControls & DISABLE_FADEOUT)); };
	void	DisableControlsScript() { BANK_ONLY(if (!IsControlsScriptDisabled()) { controlDebugf3("DisableControlsScript. Callstack:"); sysStack::PrintStackTrace(); }); DisableControls |= DISABLE_SCRIPT; };
	void	EnableControlsScript() { BANK_ONLY(if (IsControlsScriptDisabled()) { controlDebugf3("EnableControlsScript. Callstack:"); sysStack::PrintStackTrace(); }); DisableControls &= ~DISABLE_SCRIPT; };
	bool	IsControlsScriptDisabled() const { return(0!=(DisableControls & DISABLE_SCRIPT)); };
	void	DisableControlsScriptAmbient() { BANK_ONLY(if (!IsControlsScriptAmbientDisabled()) { controlDebugf3("DisableControlsScriptAmbient. Callstack:"); sysStack::PrintStackTrace(); }); DisableControls |= DISABLE_SCRIPT_AMBIENT; };
	void	EnableControlsScriptAmbient() { BANK_ONLY(if (IsControlsScriptAmbientDisabled()) { controlDebugf3("EnableControlsScriptAmbient. Callstack:"); sysStack::PrintStackTrace(); }); DisableControls &= ~DISABLE_SCRIPT_AMBIENT; };
	bool	IsControlsScriptAmbientDisabled() const { return(0!=(DisableControls & DISABLE_SCRIPT_AMBIENT)); };
//	void	DisableControlsPhones() { DisableControls |= DISABLE_PHONES; };
//	void	EnableControlsPhones() { DisableControls &= ~DISABLE_PHONES; };
//	bool	IsControlsPhonesDisabled() { return((bool)(DisableControls & DISABLE_PHONES)); };
	void	DisableControlsCutscenes() { BANK_ONLY(if (!IsControlsCutscenesDisabled()) { controlDebugf3("DisableControlsCutscenes. Callstack:"); sysStack::PrintStackTrace(); }); DisableControls |= DISABLE_CUTSCENES; };
	void	EnableControlsCutscenes() { BANK_ONLY(if (IsControlsCutscenesDisabled()) { controlDebugf3("EnableControlsCutscenes. Callstack:"); sysStack::PrintStackTrace(); }); DisableControls &= ~DISABLE_CUTSCENES; };
	bool	IsControlsCutscenesDisabled() const { return(0!=(DisableControls & DISABLE_CUTSCENES)); };
//	void	DisableControlsShortCut() { DisableControls |= DISABLE_SHORTCUT; };
//	void	EnableControlsShortCut() { DisableControls &= ~DISABLE_SHORTCUT; };
//	bool	IsControlsShortCutDisabled() { return((bool)(DisableControls & DISABLE_SHORTCUT)); };
	void	DisableControlsLoadingScreen() { BANK_ONLY(if (!IsControlsLoadingScreenDisabled()) { controlDebugf3("DisableControlsLoadingScreen. Callstack:"); sysStack::PrintStackTrace(); }); DisableControls |= DISABLE_LOADINGSCREEN; };
	void	EnableControlsLoadingScreen() { BANK_ONLY(if (IsControlsLoadingScreenDisabled()) { controlDebugf3("EnableControlsLoadingScreen. Callstack:"); sysStack::PrintStackTrace(); }); DisableControls &= ~DISABLE_LOADINGSCREEN; };
	bool	IsControlsLoadingScreenDisabled() const { return(0!=(DisableControls & DISABLE_LOADINGSCREEN)); };
	void	DisableControlsFrontend() { BANK_ONLY(if (!IsControlsFrontendDisabled()) { controlDebugf3("DisableControlsFrontend. Callstack:"); sysStack::PrintStackTrace(); }); DisableControls |= DISABLE_FRONTEND; };
	void	EnableControlsFrontend() { BANK_ONLY(if (IsControlsFrontendDisabled()) { controlDebugf3("EnableControlsFrontend. Callstack:"); sysStack::PrintStackTrace(); }); DisableControls &= ~DISABLE_FRONTEND; };
	bool	IsControlsFrontendDisabled() const { return(0!=(DisableControls & DISABLE_FRONTEND)); };
	void	EnableAllControls() { BANK_ONLY(if (DisableControls != 0) { controlDebugf3("EnableAllControls. Callstack:"); sysStack::PrintStackTrace(); }); DisableControls = 0; };

	State   GetPlayerState() const { return m_PlayerState; }
	void    SetPlayerState(const State newState);
	const char* PlayerStateToString() const;
	u32		GetTimeOfPlayerStateChange() const { return m_TimeOfPlayerStateChange; }
	bool	GetHasRespawnedWithinTime(u32 uTimeLastRespawnedCheck) const;

	const CPlayerResetFlags& GetPlayerResetFlags() const { return m_PlayerResetFlags; } 
	CPlayerResetFlags& GetPlayerResetFlags() { return m_PlayerResetFlags; } 
	CVehicle* GetVehiclePlayerIsRestrictedToEnter() { return m_pOnlyEnterThisVehicle.Get(); } 
	void SetVehiclePlayerPreferRearSeat(const CVehicle* pVeh) { m_pPreferRearSeatsVehicle = pVeh; }
	const CVehicle* GetVehiclePlayerPreferRearSeat() const { return m_pPreferRearSeatsVehicle.Get(); }
	void SetVehiclePlayerPreferFrontPassengerSeat(const CVehicle* pVeh) { m_pPreferFrontPassengerSeatVehicle = pVeh; }
	const CVehicle* GetVehiclePlayerPreferFrontPassengerSeat() const { return m_pPreferFrontPassengerSeatVehicle.Get(); }


	// PURPOSE:  Used to dampen the vertical motion on the local root (to stop bobbing when the camera is in close)
	void ProcessDampenRoot();
	void SetDampenRootWeight(float fDampenRootWeight) { m_fDampenRootWeight = fDampenRootWeight; }
	void SetDampenRootTargetWeight(float fDampenRootTargetWeight) { m_fDampenRootTargetWeight = fDampenRootTargetWeight; }
	void SetDampenRootTargetHeight(float fDampenRootTargetHeight) { m_fDampenRootTargetHeight = fDampenRootTargetHeight; }

	////////////////////////////////////////////////////////////////////////
	// Moved from CPedPlayer
	////////////////////////////////////////////////////////////////////////

	// Targeting accessors
	const CPlayerPedTargeting& GetTargeting() const { return m_targeting; }
	CPlayerPedTargeting& GetTargeting() { return m_targeting; }

	// Stamina/lung/sprint functions
	void ResetSprintEnergy();// { m_pPlayerData->m_fSprintEnergy = m_pPlayerData->m_fMaxSprintEnergy; }
	void RestoreSprintEnergy(float fPercent);
	bool HandleSprintEnergy(bool bSprint, float fRate=1.0f);

	float GetDefaultHoldBreathDurationForPed(const CPed& ped) const;
	float ComputeSprintDepletionRateForPed(const CPed& ped, float fMaxSprintEnergy, eSprintType nSprintType) const;
	// Probably shouldn't be called more than one per frame
	float ControlButtonSprint(eSprintType nSprintType, bool bUseButtonBashing=true);
	float GetButtonSprintResults(eSprintType nSprintType);

	// Returns whether we are allowed to sprint
	bool ProcessCanSprint(CPed& ped, const bool bCheckDisabled = true);

	// Returns a value representing whether we want to sprint > 1 if wanting to sprint
	float ProcessSprintControl(const CControl& control, const CPed& ped, eSprintType nSprintType, bool bUseButtonBashing);

	void ControlButtonDuck();

	bool IsPlayerPressingHorn() const;

	// water cannon
	void RegisterHitByWaterCannon();
	bool AffectedByWaterCannon();

	bool 	IsHidden() const;

	ePedVarComp	 GetPreviousVariationComponent()	{ return m_PreviousVariationComponent; }
	void		 GetPreviousVariationInfo(u32& rPreviousVariationDrawableId, u32& rPreviousVariationDrawableAltId, u32& rPreviousVariationTexId, u32& rPreviousVariationPaletteId);

	void		 ClearPreviousVariationData();
	void		 SetPreviousVariationData(ePedVarComp ePreviousVariationComponent, u32 uPreviousVariationDrawableId, u32 uPreviousVariationDrawableAltId,
										  u32 uPreviousVariationTexId, u32 uPreviousVariationPaletteId);

	static bool GetHasDisplayedPlayerQuitEnterCarHelpText()			{ return ms_bHasDisplayedPlayerQuitEnterCarHelpText; }
	static void SetHasDisplayedPlayerQuitEnterCarHelpText(bool bHasDisplayedPlayerQuitEnterCarHelpText) { ms_bHasDisplayedPlayerQuitEnterCarHelpText = bHasDisplayedPlayerQuitEnterCarHelpText; }
	static float GetSoftAimBoundary() 								{ return ms_PlayerSoftAimBoundary; }


	//TODO: refactor these.  static hell (assumes single player per machine)
	static bool IsNotDrivingVehicle();
	static bool IsFiring_s();
	float GetFiringAttackValue();
	float GetFiringBoundaryValue();
	bool IsFiring();
	static bool IsDriverFiring();
	static bool IsFlyingAircraft();
	static bool IsSimulatingSoftFiring(const CPed * pPlayer);
	static bool IsSoftFiring();
	static bool IsJustFiring();
	static bool CanUseAssistedAiming();
	static bool IsAssistedAiming();
	static bool IsRunAndGunning(bool bCanRunAndGunWhileAiming = false, bool bIgnoreSoftFiring = false);
	static bool IsAiming(const bool bConsiderAttackTriggerAiming = true);
	static bool IsSoftAiming(const bool bConsiderAttackTriggerAiming = true);
	static bool IsHardAiming();
	static bool IsForcedAiming();
	static bool IsJustHardAiming();
	static bool IsSimulatingAiming();
	static bool IsReloading();
	static bool IsCombatRolling();
	static bool IsSelectingLeftTarget(bool bCheckAimButton = true);
	static bool IsSelectingRightTarget(bool bCheckAimButton = true);
	static bool CanPlayerWarpIntoVehicle(const CPed& rPed, const CVehicle& rVeh, s32 iTargetSeat = -1);
	static void GetCachedMeleeInputs( bool& bWasLightAttackPressed, bool& bWasAlternateAttackPressed );
	bool WasLightAttackPressed() { return m_bWasLightAttackPressed; }
	bool WasAlternateAttackPressed() { return m_bWasAlternateAttackPressed; }

	void SetModelId(fwModelId modelId);
	void SetupPlayerGroup();
	bool CanPlayerStartMission();

	void KeepAreaAroundPlayerClear();

	void SetPlayerCanBeDamaged(bool DmgeFlag);
	void SetDisablePlayerSprint(bool bSprintFlag)		{ m_bPlayerSprintDisabled = bSprintFlag;}

	CWanted &		GetWanted()							{ return m_Wanted; }
	const CWanted &	GetWanted() const					{ return m_Wanted; }

	u32				GetPlayerDataSprintReplenishing() const	{ return m_bSprintReplenishing; }
	float			GetPlayerDataSprintEnergy() const	{ return m_fSprintEnergy; }
	float			GetPlayerDataSprintControlCounter() const	{ return m_fSprintControlCounter; }
	void			SetPlayerDataSprintControlCounter(float v)	{ m_fSprintControlCounter = v; }

	void			SetPlayerDataMaxSprintEnergy(float fNewMax)	{ m_fMaxSprintEnergy = fNewMax; }
	float			GetPlayerDataMaxSprintEnergy() const { return m_fMaxSprintEnergy; }
	float			GetSprintEnergyRatio() const	{ return m_fSprintEnergy/m_fMaxSprintEnergy; }

	void			SetRunSprintSpeedMultiplier(float fNewMultiplier) { m_fRunSprintSpeedMultiplier = fNewMultiplier; }
	const float		GetRunSprintSpeedMultiplier() const;

	float			GetTimeBeenSprinting() const { return m_fTimeBeenSprinting; }

	static bool		AreCNCResponsivenessChangesEnabled(const CPed* pPed);

	// Weapon/damage info
	u32				GetPlayerDataLastChangeWeaponFrame() const	{ return m_nLastChangeWeaponFrame; }
	void			SetPlayerDataLastChangeWeaponFrame(u32 v)	{ m_nLastChangeWeaponFrame = v; }
	f32				GetPlayerWeaponDamageModifier() const	{ return m_fWeaponDamageModifier; }
	void			SetPlayerWeaponDamageModifier(f32 f)	{ static const float MIN_DAMAGE_MODIFIER = 0.1f; Assertf(f > MIN_DAMAGE_MODIFIER, "Damage modifier [%f] must be greater than [%f]", f, MIN_DAMAGE_MODIFIER); m_fWeaponDamageModifier = Max(f, MIN_DAMAGE_MODIFIER); }
	f32				GetPlayerWeaponDefenseModifier() const	{ return m_fWeaponDefenseModifier; }
	void			SetPlayerWeaponDefenseModifier(f32 f)	{ Assertf(f >= 0.1f, "Defense modifier [%f] must be greater than [%f]", f, 0.1f); m_fWeaponDefenseModifier = Max(f, 0.1f); }
	f32				GetPlayerWeaponMinigunDefenseModifier() const	{ return m_fWeaponMinigunDefenseModifier; }
	void			SetPlayerWeaponMinigunDefenseModifier(f32 f)	{ Assertf(f >= 0.1f, "Minigun defense modifier [%f] must be greater than [%f]", f, 0.1f); m_fWeaponMinigunDefenseModifier = Max(f, 0.1f); }
	f32				GetPlayerMeleeWeaponDamageModifier() const	{ return m_fMeleeWeaponDamageModifier; }
	void			SetPlayerMeleeWeaponDamageModifier(f32 f)	{ static const float MIN_DAMAGE_MODIFIER = 0.1f; Assertf(f > MIN_DAMAGE_MODIFIER, "Melee Damage modifier [%f] must be greater than [%f]", f, MIN_DAMAGE_MODIFIER); m_fMeleeWeaponDamageModifier = Max(f, MIN_DAMAGE_MODIFIER); }
	f32				GetPlayerMeleeUnarmedDamageModifier() const	{ return m_fMeleeUnarmedDamageModifier; }
	void			SetPlayerMeleeUnarmedDamageModifier(f32 f)	{ static const float MIN_DAMAGE_MODIFIER = 0.1f; Assertf(f > MIN_DAMAGE_MODIFIER, "Melee Unarmed Damage modifier [%f] must be greater than [%f]", f, MIN_DAMAGE_MODIFIER); m_fMeleeUnarmedDamageModifier = Max(f, MIN_DAMAGE_MODIFIER); }
	f32				GetPlayerMeleeWeaponDefenseModifier() const	{ return m_fMeleeWeaponDefenseModifier; }
	void			SetPlayerMeleeWeaponDefenseModifier(f32 f)	{ Assertf(f >= 0.1f, "Melee Defense modifier [%f] must be greater than [%f]", f, 0.1f); m_fMeleeWeaponDefenseModifier = Max(f, 0.1f); }
	f32				GetPlayerMeleeWeaponForceModifier() const { return m_fMeleeWeaponForceModifier; }
	void			SetPlayerMeleeWeaponForceModifier(f32 f) { Assertf(f >= 1.0f, "Melee force modifier [%f] must be greater than [%f]", f, 1.0f); m_fMeleeWeaponForceModifier = Max(f, 1.0f); }
	f32				GetPlayerVehicleDamageModifier() const	{ return m_fVehicleDamageModifier; }
	void			SetPlayerVehicleDamageModifier(f32 f)	{ Assertf(f >= 0.1f, "Vehicle Damage modifier [%f] must be greater than [%f]", f, 0.1f); m_fVehicleDamageModifier = Max(f, 0.1f); }
	f32				GetPlayerVehicleDefenseModifier() const	{ return m_fVehicleDefenseModifier; }
	void			SetPlayerVehicleDefenseModifier(f32 f)	{ Assertf(f >= 0.1f, "Vehicle Defense modifier [%f] must be greater than [%f]", f, 0.1f); m_fVehicleDefenseModifier = Max(f, 0.1f); }
	f32				GetPlayerMaxExplosiveDamage() { return m_fMaxExplosiveDamage; }
	void			SetPlayerMaxExplosiveDamage(f32 f) { m_fMaxExplosiveDamage = f; }
	f32				GetPlayerExplosiveDamageModifier() { return m_fExplosiveDamageModifier; }
	void			SetPlayerExplosiveDamageModifier(f32 f) { m_fExplosiveDamageModifier = f; }
	f32				GetPlayerWeaponTakedownDefenseModifier() const	{ return m_fWeaponTakedownDefenseModifier; }
	void			SetPlayerWeaponTakedownDefenseModifier(f32 f)	{ Assertf(f >= 0.1f, "Weapon takedown defense modifier [%f] must be greater than [%f]", f, 0.1f); m_fWeaponTakedownDefenseModifier = Max(f, 0.1f); }

	f32				GetPlayerMeleeDamageModifier(bool bUnarmedMelee) const;

#if __BANK
	void			AddWeaponTuningWidgets(bkBank& bank);
#endif // __BANK
	u32				GetPlayerDataFreeAiming() const	{ return m_bFreeAiming; }
	void			SetPlayerDataFreeAiming(bool v)	{ m_bFreeAiming = v; }
	bool			GetPlayerDataCanBeDamaged() const { return m_bCanBeDamaged; }
	bool			IsPlayerDataDamageAllowedFromSetPlayerControl() const { return m_DamageAllowedFromSetPlayerControl; }
	u32				GetPlayerDataHasControlOfRagdoll() const { return m_bHasControlOfRagdoll; }
	void			SetPlayerDataHasControlOfRagdoll(bool v) { m_bHasControlOfRagdoll = v; }

	u32				GetPlayerDataAllowedToPickUpNonMissionObjects() const	{ return m_bAllowedToPickUpNonMissionObjects; }
	void			SetPlayerDataAllowedToPickUpNonMissionObjects(bool v)	{ m_bAllowedToPickUpNonMissionObjects = v; }
	u32				GetPlayerDataCanCollectMoneyPickups() const	{ return m_bCanCollectMoneyPickups; }
	void			SetPlayerDataCanCollectMoneyPickups(bool v)	{ m_bCanCollectMoneyPickups = v; }

	u32				GetPlayerDataAmbientMeleeMoveDisabled() const	{ return m_bAmbientMeleeMoveDisabled; }
	void			SetPlayerDataAmbientMeleeMoveDisabled(bool v) 	{ m_bAmbientMeleeMoveDisabled = v; }


	// Vehicle node information
	u32				GetPlayerDataPlayerOnHighway() const	{ return m_bPlayerOnHighway; }
	u32				GetPlayerDataPlayerOnWideRoad() const	{ return m_bPlayerOnWideRoad; }
	u32				GetPlayerDataNodesNearbyInZ() const	{ return m_bNodesNearbyInZ; }
	CNodeAddress	GetPlayerDataNodeAddress() const { return m_PlayerRoadNode;}
	s16				GetPlayerDataLocalLinkIndex() const {return m_iPlayerRoadLinkLocalIndex;}

	void			SetCanUseSearchLight(bool v) { m_bCanUseSearchLight = v; }
	bool			GetCanUseSearchLight() const {return m_bCanUseSearchLight;}

	// The number of enemy peds in combat targetting this player.
	u32				GetNumEnemiesInCombat() const { return m_iNumEnemiesInCombat; }
	u32				GetNumEnemiesShootingInCombat() const { return m_iNumEnemiesShootingInCombat; }

	s32				GetPlayerDataPlayerGroup() const	{ return m_PlayerGroup; }

	CPhysical*		GetFollowTarget() const { return m_pFollowTarget; }
	void			SetFollowTarget(CPhysical* pFollowTarget, float fMaxFollowDistance = -1.f) { m_pFollowTarget = pFollowTarget; m_fMaxFollowDistance = fMaxFollowDistance; }
	float			GetMaxFollowDistance() const { return m_fMaxFollowDistance; }
	float			GetFollowTargetRadius() const { return m_fFollowTargetRadius; }
	void			SetFollowTargetRadius(float fFollowTargetRadius) { m_fFollowTargetRadius = fFollowTargetRadius; }
	Vec3V_ConstRef	GetFollowTargetOffset() const { return m_vFollowTargetOffset; }
	void			SetFollowTargetOffset(Vec3V_ConstRef vFollowTargetOffset) { m_vFollowTargetOffset = vFollowTargetOffset;  }
	bool			GetMayFollowTargetGroupMembers() const { return m_bMayFollowTargetGroupMembers; }
	void			SetMayFollowTargetGroupMembers(bool b) { m_bMayFollowTargetGroupMembers = b; }

#if ENABLE_HORSE
	CPlayerHorseSpeedMatch*	GetInputHorse()	{ return &m_InputHorse; }
#endif

	bool 			GetPlayerDataPlayerSprintDisabled() const	{ return m_bPlayerSprintDisabled; }
	void 			SetPlayerDataPlayerSprintDisabled(bool v)	{ m_bPlayerSprintDisabled = v; }
	bool			GetPlayerDataDisplayMobilePhone() const		{ return m_bDisplayMobilePhone; }
	void			SetPlayerDataDisplayMobilePhone(bool v)		{ m_bDisplayMobilePhone = v; }

	u32				GetPlayerDataLastTimePlayerHitCar() const	{ return m_LastTimePlayerHitCar; }
	u32				GetPlayerDataLastTimePlayerHitPed() const	{ return m_LastTimePlayerHitPed; }
	u32				GetPlayerDataLastTimePlayerHitBuilding() const	{ return m_LastTimePlayerHitBuilding; }
	u32				GetPlayerDataLastTimePlayerHitObject() const	{ return m_LastTimePlayerHitObject; }
	u32				GetPlayerDataLastTimePlayerDroveOnPavement() const	{ return m_LastTimePlayerDroveOnPavement; }
	u32				GetPlayerDataLastTimePlayerRanLight() const	{ return m_LastTimePlayerRanLight; }
	u32				GetPlayerDataLastTimePlayerDroveAgainstTraffic() const	{ return m_LastTimePlayerDroveAgainstTraffic; }


	// Dynamic cover point, moved around as the player shifts dynamically around cover.
	CCoverPoint* GetDynamicCoverPoint()					{ return &m_dynamicCoverPoint; }
	const CCoverPoint* GetDynamicCoverPoint() const		{ return &m_dynamicCoverPoint; }
	Vector3 GetDynamicCoverPointLastKnownPosition() const { return m_vDynamicCoverPointLastKnownPosition; }
	void SetDynamicCoverPointLastKnownPosition(const Vector3& vDynamicCoverPointLastKnownPosition) { m_vDynamicCoverPointLastKnownPosition = vDynamicCoverPointLastKnownPosition; }
	bool DynamicCoverCanMoveRight() const				{ return m_bCanMoveRight; }
	void SetDynamicCoverCanMoveRight(bool canMove)		{ m_bCanMoveRight = canMove; }
	bool DynamicCoverCanMoveLeft() const				{ return m_bCanMoveLeft; }
	void SetDynamicCoverCanMoveLeft(bool canMove)		{ m_bCanMoveLeft = canMove; }
	bool DynamicCoverHitPed() const						{ return m_bHitPed; }
	void SetDynamicCoverHitPed(bool hitPed) 			{ m_bHitPed = hitPed; }
	bool DynamicCoverInsideCorner() const				{ return m_bInsideCorner; }
	void SetDynamicCoverEdgeDistance(float fDistance) 	{ m_fCoverEdgeDistance = fDistance;}
	float GetDynamicCoverEdgeDistance() 				{ return m_fCoverEdgeDistance;}
	float GetInsideCornerDistance() const 				{ return m_fInsideCornerDistance; }
	void SetDynamicCoverInsideCorner(bool insideCorner, float dist = -1.0f) { m_bInsideCorner = insideCorner; m_fInsideCornerDistance = dist; }
	bool IsRoundCover() const							{ return m_bIsRoundCover; }
	Vector3 GetRoundCoverCenter() const					{ return m_vRoundCoverCenter;}
	float GetRoundCoverRadius() const					{ return m_fRoundCoverRadius;}	
	void SetRoundCover(const Vector3& center, float fRadius) { m_bIsRoundCover = true; m_vRoundCoverCenter = center; m_fRoundCoverRadius = fRadius;}	
	void ClearRoundCover()								{ m_bIsRoundCover = false;}	
	bool IsCoverGeneratedByDynamicEntity() const		{ return m_bCoverGeneratedByDynamicEntity; }
	void SetCoverGeneratedByDynamicEntity(bool bCoverGeneratedByDynamicEntity)	{ m_bCoverGeneratedByDynamicEntity = bCoverGeneratedByDynamicEntity; }

	// Player using cover?
	float GetTimeElapsedUsingCoverSeconds() const { return m_fTimeElapsedUsingCoverSeconds; }

	// Player hiding in cover?
	float GetTimeElapsedHidingInCoverSeconds() const { return m_fTimeElapsedHidingInCoverSeconds; }

	// Player loitering in combat?
	float GetTimeElapsedLoiteringInCombatSeconds() const { return m_fTimeElapsedLoiteringSeconds; }

	// Charging peds election methods:
	// -
	// Check if at least queryCount charges have occurred within the given query period.
	bool HaveNumEnemiesChargedWithinQueryPeriod(u32 queryPeriodMS, int queryCount) const;
	// Check if the given ped is the best charger candidate in the most recent registration period.
	bool IsBestChargerCandidate(const CPed* pCandidatePed) const;
	// The best charger candidate has started his charge.
	// Adds current time to history and closes the acceptance period.
	void ReportEnemyStartedCharge();
	// Consider the given ped as a candidate.
	// Opens a new registration period if appropriate.
	// Does nothing if the acceptance period is open.
	void RegisterCandidateCharger(const CPed* pCandidatePed);
	// -

	// Smoke grenade thrower election methods:
	// -
	// Check if at least queryCount throws have occurred within the given query period.
	bool HaveNumEnemiesThrownSmokeWithinQueryPeriod(u32 queryPeriodMS, int queryCount) const;
	// Check if the given ped is the best thrower candidate in the most recent registration period.
	bool IsBestSmokeThrowerCandidate(const CPed* pCandidatePed) const;
	// The best thrower candidate has started his throw.
	// Adds current time to history and closes the acceptance period.
	void ReportEnemyStartedSmokeThrow();
	// Consider the given ped as a candidate.
	// Opens a new registration period if appropriate.
	// Does nothing if the acceptance period is open.
	void RegisterCandidateSmokeThrower(const CPed* pCandidatePed);
	// -

	bool GetCanLeaveParachuteSmokeTrail() const { return m_bCanLeaveParachuteSmokeTrail; }
	void SetCanLeaveParachuteSmokeTrail(bool bValue) { m_bCanLeaveParachuteSmokeTrail = bValue; }

	bool IsInsideMovingTrain() const { return m_bIsInsideMovingTrain; }
	bool AllowControlInsideMovingTrain() const { return m_bAllowControlInsideMovingTrain; }
	CVehicle * GetTrainPedIsInside() const { return m_pTrainPedIsInside; }

	bool GetDisableAgitation() const { return m_bDisableAgitation; }
	
	Color32 GetParachuteSmokeTrailColor() const { return m_ParachuteSmokeTrailColor; }
	void SetParachuteSmokeTrailColor(const Color32 cColor) { m_ParachuteSmokeTrailColor = cColor; }

	// Simulate player gait. Used for blendouts.
	bool  GetSimulateGaitInputOn() const { return m_bSimulateGaitInput; }
	float GetSimulateGaitDuration() const { return m_fSimulateGaitDuration; }
	float GetSimulateGaitTimerCount() const { return m_fSimulateGaitTimerCount; }
	float GetSimulateGaitMoveBlendRatio() const { return m_fSimulateGaitMoveBlendRatio; }
	float GetSimulateGaitHeading() const { return m_fSimulateGaitHeading; }
	float GetSimulateGaitStartHeading() const { return m_fSimulateGaitStartHeading; }
	float GetSimulateGaitUseRelativeHeading() const { return m_bSimulateGaitUseRelativeHeading; }
	float GetSimulateGaitNoInputInterruption() const { return m_bSimulateGaitNoInputInterruption; }
	void SetSimulateGaitTimerCount(float fTime) { m_fSimulateGaitTimerCount = fTime; }
	void SetSimulateGaitInput(float fMoveBlendRatio, float fTime = -1.0f, float fHeading = 0.0f, bool bRelativeHeading = false, bool bNoInputInterruption = false);
	void ResetSimulateGaitInput() { m_bSimulateGaitInput = false; m_fSimulateGaitTimerCount = 0.0f; }

	float GetStealthPerceptionModifier() const { return m_fStealthPerceptionModifier; }
	void SetStealthPerceptionModifier(float fMod) { m_fStealthPerceptionModifier = Clamp(fMod, 0.0f, 1.0f); }

	u32 GetTimeExitVehicleTaskFinished() const { return m_uTimeExitVehicleTaskFinished; }
	void SetExitVehicleTaskFinished() { m_uTimeExitVehicleTaskFinished = fwTimer::GetTimeInMilliseconds(); }

	bool GetAutoGiveParachuteWhenEnterPlane() const {return m_bAutoGiveParachuteWhenEnterPlane;}
	void SetAutoGiveParachuteWhenEnterPlane(bool bAutoGiveParachute) {m_bAutoGiveParachuteWhenEnterPlane = bAutoGiveParachute;}

	bool GetAutoGiveScubaGearWhenExitVehicle() const {return m_bAutoGiveScubaGearWhenExitVehicle;}
	void SetAutoGiveScubaGearWhenExitVehicle(bool bAutoGiveScubaGear) {m_bAutoGiveScubaGearWhenExitVehicle = bAutoGiveScubaGear;}

	bool GetPlayerLeavePedBehind() const { return m_bPlayerLeavePedBehind; }
	void SetPlayerLeavePedBehind(bool bLeavePedBehind) { m_bPlayerLeavePedBehind = bLeavePedBehind; }
	
	float GetTimeSinceLastDrivingAtSpeed() const { return m_fTimeSinceLastDrivingAtSpeed; }

	//////////////////////////////////////////////////////////////////////////
	// Player LOS info functions

	void UpdatePlayerTimeLastScanned(u8 playerIndex, u32 timeScanned)
	{
		if(Verifyf(playerIndex < MAX_NUM_PHYSICAL_PLAYERS, "playerIndex out of range"))
		{
			m_aPlayerLOSInfo[playerIndex].SetTimeLastScanned(timeScanned);
		}
	}

	void UpdatePlayerLOS(u8 playerIndex, bool bIsVisible)
	{
		if(Verifyf(playerIndex < MAX_NUM_PHYSICAL_PLAYERS, "playerIndex out of range"))
		{
			m_aPlayerLOSInfo[playerIndex].SetIsVisible(bIsVisible);
		}
	}

	void ResetPlayerLOS(u8 playerIndex)
	{
		if(Verifyf(playerIndex < MAX_NUM_PHYSICAL_PLAYERS, "playerIndex out of range"))
		{
			m_aPlayerLOSInfo[playerIndex].SetIsVisible(false);
		}
	}

	void ResetAllPlayerLOS()
	{
		for(int i = 0; i < m_aPlayerLOSInfo.GetCount(); i++)
		{
			m_aPlayerLOSInfo[i].SetTimeLastScanned(0);
			m_aPlayerLOSInfo[i].SetIsVisible(false);
		}
	}

	u32 GetTimeLastScannedPlayer(u8 playerIndex)
	{
		if(Verifyf(playerIndex < MAX_NUM_PHYSICAL_PLAYERS, "playerIndex out of range"))
		{
			return m_aPlayerLOSInfo[playerIndex].GetTimeLastScanned();
		}

		return 0;
	}

	bool GetIsPlayerVisible(u8 playerIndex)
	{
		if(Verifyf(playerIndex < MAX_NUM_PHYSICAL_PLAYERS, "playerIndex out of range"))
		{
			return m_aPlayerLOSInfo[playerIndex].GetIsVisible();
		}

		return false;
	}
	
	// Talking API
	void	SetLastTimePedCommentedOnWeirdPlayer() { m_uLastTimePedMadeMobileCommentOnWeirdPlayer = fwTimer::GetTimeInMilliseconds(); }
	u32     GetLastTimePedCommentedOnWeirdPlayer() const { return m_uLastTimePedMadeMobileCommentOnWeirdPlayer; }

	void	SetLastTimeDamagedLocalPlayer() { m_uLastTimeDamagedLocalPlayer = fwTimer::GetTimeInMilliseconds(); }
	u32     GetLastTimeDamagedLocalPlayer() const { return m_uLastTimeDamagedLocalPlayer; }

	CPlayerHealthRecharge& GetHealthRecharge() { return m_healthRecharge; }
	CPlayerEnduranceManager& GetEnduranceManager() { return m_enduranceManager; }
	CPlayerDamageOverTime& GetDamageOverTime() { return m_damageOverTime; }

	const CPlayerHealthRecharge& GetHealthRecharge() const { return m_healthRecharge; }
	const CPlayerEnduranceManager& GetEnduranceManager() const { return m_enduranceManager; }
	const CPlayerDamageOverTime& GetDamageOverTime() const { return m_damageOverTime; }

	void	SetLastTimeBustInVehicleFailed() { m_LastTimeBustInVehicleFailed = fwTimer::GetTimeInMilliseconds(); }
	u32		GetLastTimeBustInVehicleFailed() const { return m_LastTimeBustInVehicleFailed; }

	void	SetLastTimeArrestAttempted() { m_LastTimeArrestAttempted = fwTimer::GetTimeInMilliseconds(); }
	u32		GetLastTimeArrestAttempted() const { return m_LastTimeArrestAttempted; }

	void	SetLastTimeWarpedIntoVehicle() { m_LastTimeWarpedIntoVehicle = fwTimer::GetTimeInMilliseconds(); }
	u32		GetLastTimeWarpedIntoVehicle() const { return m_LastTimeWarpedIntoVehicle; }

	void	SetLastTimeWarpedOutOfVehicle() { m_LastTimeWarpedOutOfVehicle = fwTimer::GetTimeInMilliseconds(); }
	u32		GetLastTimeWarpedOutOfVehicle() const { return m_LastTimeWarpedOutOfVehicle; }

	u8		GetNumAiAttempingArrestOnPlayer() const { return m_uNumAiAttemptingArrestOnPlayer; }
	void	IncrementAiAttemptingArrestOnPlayer();
	void	DecrementAiAttemptingArrestOnPlayer();

	// Functions to handle the player attempting to leave the map.
	bool HasPlayerLeftTheWorld() const;
	void HandlePlayerWorldBreach();
	void HandlePlayerWorldBreachParachuting();
	void HandlePlayerWorldBreachVehicle(CVehicle& rVehicle);
	void HandlePlayerWorldBreachSwimming();

	float GetExitCombatRollToScopeTimerInFPS() {return m_fExitCombatRollToScopeTimerInFPS;}


	//////////////////////////////////////////////////////////////////////////
	// Special abilities
	SpecialAbilityType GetMPSpecialAbility(ePlayerSpecialAbilitySlot eAbilitySlot = PSAS_PRIMARY) const;
	void SetMPSpecialAbility(SpecialAbilityType eAbility, ePlayerSpecialAbilitySlot eAbilitySlot = PSAS_PRIMARY);

	inline const CArcadePassiveAbilityEffects& GetArcadeAbilityEffects() const { return m_arcadeAbilityEffects; }
	inline CArcadePassiveAbilityEffects& AccessArcadeAbilityEffects() { return m_arcadeAbilityEffects; }

	inline const CPlayerArcadeInformation& GetArcadeInformation() const { return m_arcadeInformation; }
	inline CPlayerArcadeInformation& AccessArcadeInformation() { return m_arcadeInformation; }

#if __BANK
	// Tuning
	static void AddAimWidgets(bkBank & bank);
	static bool & GetDisplayRecklessDriving()					{ return ms_bDisplayRecklessDriving; }
	static bool & GetDisplayNumEnemiesInCombat()				{ return ms_bDisplayNumEnemiesInCombat; }
	static bool & GetDisplayCoverTracking()						{ return ms_bDisplayCoverTracking; }
	static bool & GetDisplayCombatLoitering()					{ return ms_bDisplayCombatLoitering; }
	static bool & GetDebugCombatLoitering()						{ return ms_bDebugCombatLoitering; }
	static bool & GetDisplayNumEnemiesShootingInCombat()		{ return ms_bDisplayNumEnemiesShootingInCombat; }
	static bool & GetDebugCandidateChargeGoalPositions()		{ return ms_bDebugCandidateChargeGoalPositions; }
	static bool & GetDebugDrawCandidateChargePositions()		{ return ms_bDebugDrawCandidateChargePositions; }
#endif

	// Debug functionality
#if __ASSERT
	static void SetChangingPlayerModel(bool changingPlayerModel) { ms_ChangingPlayerModel = changingPlayerModel; };
	static bool IsChangingPlayerModel()                          { return ms_ChangingPlayerModel; };
#endif // __ASSERT
#if !__FINAL
	static bool		IsPlayerInvincible() { return ms_bDebugPlayerInvincible BANK_ONLY(|| CDebug::GetInvincibleBugger()); }
	static bool		IsPlayerInvincibleRestoreHealth() { return ms_bDebugPlayerInvincibleRestoreHealth; }
	static bool		IsPlayerInvincibleRestoreArmorWithHealth() { return ms_bDebugPlayerInvincibleRestoreArmorWithHealth; }

	static bool		ms_bDebugPlayerInvincible;
	static bool		ms_bDebugPlayerInvincibleRestoreHealth;
	static bool	    ms_bDebugPlayerInvincibleRestoreArmorWithHealth;
	static bool		ms_bDebugPlayerInvisible;
	static bool		ms_bDebugPlayerInfiniteStamina;
#endif // !__FINAL

	static void ExtendWorldBoundary(const Vector3 &vCoordExtendsTo);
	static void ResetWorldBoundary() {		
		ms_WorldLimitsPlayerAABB = spdAABB(Vec3V(ms_Tunables.m_MinWorldLimitsPlayerX, ms_Tunables.m_MinWorldLimitsPlayerY, WORLDLIMITS_ZMIN), 
		Vec3V(ms_Tunables.m_MaxWorldLimitsPlayerX, ms_Tunables.m_MaxWorldLimitsPlayerY, WORLDLIMITS_ZMAX));
		ms_bPlayerBoundaryInitialized = true;
	}

	// Player vehicle control variables
	float GetLaggedYawControl() const { return m_fLaggedYawControl; }
	void SetLaggedYawControl(float val) { m_fLaggedYawControl = val; }
	float GetLaggedPitchControl() const { return m_fLaggedPitchControl; }
	void SetLaggedPitchControl(float val) { m_fLaggedPitchControl = val; }
	float GetLaggedRollControl() const { return m_fLaggedRollControl; }
	void SetLaggedRollControl(float val) { m_fLaggedRollControl = val; }
	float GetLaggedSteerControl() const { return m_fLaggedSteerControl; }
	void SetLaggedSteerControl(float val) { m_fLaggedSteerControl = val; }

	float GetTimeBetweenRandomControlInputs() const { return m_fTimeBetweenRandomControlInputs; }
	void SetTimeBetweenRandomControlInputs(float val) { m_fTimeBetweenRandomControlInputs = val; }
	float GetRandomControlYaw() const { return m_fRandomControlYaw; }
	void SetRandomControlYaw(float val) { m_fRandomControlYaw = val; }
	float GetRandomControlPitch() const { return m_fRandomControlPitch; }
	void SetRandomControlPitch(float val) { m_fRandomControlPitch = val; }
	float GetRandomControlRoll() const { return m_fRandomControlRoll; }
	void SetRandomControlRoll(float val) { m_fRandomControlRoll = val; }
	float GetRandomControlThrottle() const { return m_fRandomControlThrottle; }
	void SetRandomControlThrottle(float val) { m_fRandomControlThrottle = val; }
	void SetRandomControlSteerModifier(float val) { m_fRandomControlSteerModifier = val; } 
	float GetRandomControlSteerModifier() const { return m_fRandomControlSteerModifier; }
	void SetControlSettledTime(float val) { m_fControlSettledTime = val; }
	float GetControlSettledTime() const { return m_fControlSettledTime; }
	void SetTimeSinceApplyingRandomControl(float val) { m_fTimeSinceApplyingRandomControl = val; }
	float GetTimeSinceApplyingRandomControl() const { return m_fTimeSinceApplyingRandomControl; }
	void SetPreviousSteerValue(float val) { m_fPreviousSteerValue = val; }
	float GetPreviousSteerValue() const { return m_fPreviousSteerValue; }
	
	void ResetVehicleControls()
	{
		m_fLaggedYawControl = 0.0f;
		m_fLaggedPitchControl = 0.0f;
		m_fLaggedRollControl = 0.0f;
		m_fLaggedSteerControl = 0.0f;
		m_fTimeBetweenRandomControlInputs = 0.0f;
		m_fRandomControlYaw = 0.0f;
		m_fRandomControlPitch = 0.0f;
		m_fRandomControlRoll = 0.0f;
		m_fRandomControlThrottle = 0.0f;
		m_fRandomControlSteerModifier = 0.0f;
		m_fControlSettledTime = 0.0f;
		m_fPreviousSteerValue = 0.0f;
		m_fTimeSinceApplyingRandomControl = 0.0f;
	}

	CPed* GetSpotterOfStolenVehicle() const { return m_pSpotterOfStolenVehicle; }

	bool HasBeenSpottedInStolenVehicle() const
	{
		return (m_pSpotterOfStolenVehicle != NULL);
	}

	void BumpedInVehicle(const CPed& rPed);
	u32 GetLastTimeBumpedPedInVehicle() const { return m_uLastTimeBumpedPedInVehicle; }

	void DisableAgitation();

	void SprintAimBreakOut() { m_uSprintAimBreakOutTime = fwTimer::GetTimeInMilliseconds(); }
	bool IsSprintAimBreakOutOver() const;

	// Parachute variation override.

	ePedVarComp GetPedParachuteVariationComponentOverride() const { return m_ParachuteVariationComponent; }
	u32 GetPedParachuteVariationDrawableOverride() const  { return m_ParachuteVariationDrawable; }
	u32 GetPedParachuteVariationAltDrawableOverride() const  { return m_ParachuteVariationAltDrawable; }
	u32 GetPedParachuteVariationTexIdOverride() const  { return m_ParachuteVariationTexId; }
	u32 GetPedParachuteModelOverride() const { return m_ParachuteModelHash; }
	u32 GetPedReserveParachuteModelOverride() const { return m_ReserveParachuteModelHash; }
	u32 GetPedParachutePackModelOverride() const { return m_ParachutePackModelHash; }
	bool CanRetainParachuteAfterTeleport() const { return m_bRetainParachuteAfterTeleport; }

	void SetPedParachuteVariationOverride(ePedVarComp slotId, u32 drawblId, u32 texId = 0, u32 altDrawblId = 0);
	void ClearPedParachuteVariationOverride();
	void SetPedParachuteModelOverride(u32 uModelHash) { m_ParachuteModelHash = uModelHash; }
	void SetPedReserveParachuteModelOverride(u32 uModelHash) { m_ReserveParachuteModelHash = uModelHash; }
	void SetPedParachutePackModelOverride(u32 uModelHash)  { m_ParachutePackModelHash = uModelHash; }

	bool GetEnableBlueToothHeadset() const { return m_bEnableBlueToothHeadset; }
	void SetEnableBlueToothHeadset(bool b) { m_bEnableBlueToothHeadset = b; }
	// cover.

	bool CanUseCover() const;

	bool GetInFPSScopeState() const { return m_bInFPSScopeState; }

	void SetSwitchedToOrFromFirstPersonThisFrame(const bool bSwitchedToFirstPerson) 
	{ 
		m_SwitchedToOrFromFirstPersonCount = 1; 
		m_SwitchedToFirstPersonCount = (bSwitchedToFirstPerson ? 1 : 0); 
		m_PlayerResetFlags.SetFlag(CPlayerResetFlags::PRF_CAMERA_VIEW_MODE_SWITCHED_TO_OR_FROM_FIRST_PERSON);
		if (bSwitchedToFirstPerson)
		{
			m_PlayerResetFlags.SetFlag(CPlayerResetFlags::PRF_CAMERA_VIEW_MODE_SWITCHED_TO_FIRST_PERSON);
		}
	}
	void SetSwitchedToOrFromFirstPerson(const bool bSwitchedToFirstPerson) { m_SwitchedToOrFromFirstPersonCount = 2; m_SwitchedToFirstPersonCount = (bSwitchedToFirstPerson ? 2 : 0); }
	void SetSwitchedFirstPersonFromThirdPersonCoverCount() { m_SwitchedToFirstPersonFromThirdPersonCoverCount = 2; }
	u32 GetSwitchedToOrFromFirstPersonCount() const { return m_SwitchedToOrFromFirstPersonCount; }
	u32 GetSwitchedToFirstPersonCount() const { return m_SwitchedToFirstPersonCount; }
	u32 GetSwitchedToFirstPersonFromThirdPersonCoverCount() const { return m_SwitchedToFirstPersonFromThirdPersonCoverCount; }

	float GetFallHeightForPedOverride() const { return m_fFallHeightOverride; }
	void SetFallHeightForPedOverride(float dist) { m_fFallHeightOverride = dist; }

	bool IsPlayerStateValidForFPSManipulation() const;
	void			ProcessPlayerVehicleSpeedTimer();

	bool CanPlayerPerformVehicleMelee() const;
	bool GetVehMeleePerformingRightHit() const { return m_bVehMeleePerformingRightHit; }
	void SetVehMeleePerformingRightHit(bool bRight) { m_bVehMeleePerformingRightHit = bRight; }

#if FPS_MODE_SUPPORTED
	// B*2248699: Used to store left hand position before we update the peds heading in CTaskHumanLocomotion::ProcessPostCamera.
	void SetLeftHandPositionFPS(Vec3V &vOffset) { m_vLeftHandPositionFPS = vOffset; }
	Vec3V GetLeftHandGripPositionFPS() const { return m_vLeftHandPositionFPS; }
#endif	// FPS_MODE_SUPPORTED

	bool GetHasScriptedWeaponFirePos() const { return m_bHasScriptedWeaponFirePos; }
	void SetScriptedWeaponFiringPos(Vec3V vPos) { m_bHasScriptedWeaponFirePos = true; m_vScriptedWeaponFiringPos = vPos; }
	void ClearScriptedWeaponFiringPos() { m_bHasScriptedWeaponFirePos = false; m_vScriptedWeaponFiringPos = Vec3V(V_ZERO); }
	Vec3V GetScriptedWeaponFiringPos() const { return m_vScriptedWeaponFiringPos; }

	bool GetAllowStuntJumpCamera() const { return m_bAllowStuntJumpCamera; }
	void SetAllowStuntJumpCamera(bool bEnable) { m_bAllowStuntJumpCamera = bEnable; }

private:

	void			CollidedWithPed(CPed& rPed);
	void			Process();
	void			ProcessCollisions();
	void			ProcessDisableAgitation();
	void			ProcessIsInsideMovingTrain();
	void			ProcessStolenVehicleSpotted();

	// Potentially add rumble for this collision.
	void			HandleCollisionWithRumblePed(CPed& rPed);

private:
	State			m_PlayerState; // Player State
	u32				m_TimeOfPlayerStateChange; // Time of the player state change

	// Time this player last went from respawn to a new state NOTE - uses a non standard timer to be careful if you plan on using this to check against the proper time
	u32				m_TimeOfPlayerLastRespawn;

	RegdPed			m_pPlayerPed;				// Pointer to the player ped (should always be set)
//	RegdVeh			m_pRemoteVehicle;			// Pointer to vehicle player is driving remotely at the moment.(NULL if on foot)
	RegdVeh			m_pLastTargetVehicle;		// Last vehicle player tried to enter.

	RegdVeh			m_pOnlyEnterThisVehicle;	// Restrict the player to only being able to enter this vehicle (script-controlled)	
	RegdConstVeh	m_pPreferRearSeatsVehicle;	// Script can prefer the player to enter the rear seats for this vehicle
	RegdConstVeh	m_pPreferFrontPassengerSeatVehicle;	// Script can prefer the player to enter the frotn passenger seat for this vehicle

	RegdPed			m_pSpotterOfStolenVehicle;

	CPlayerResetFlags	m_PlayerResetFlags;		//flags on the player that get reset every frame

	// We can disable control for a player here. It's better to do this here as opposed to in the ControlMgr as
	// we can have more players on one machine now and it is also easier to update the 'disable' information in a
	// network game if it is stored with the player.
	u32				DisableControls;

	// These variables are to tell whether the player is sheltered above from the rain.
	// It is only done when NOT in an interior and when it IS raining..	
	s32				m_iTimeTillNextShelterLineTest;		// Time till we next do a linetest
	WorldProbe::CShapeTestSingleResult	m_hShelterLineTestHandle;

	CNodeAddress	m_PlayerRoadNode;	

	// A count of the number of enemy peds in combat targetting this player.
	u32				m_iNumEnemiesInCombat;

	// A count of the number of enemy peds shooting at this player.
	u32				m_iNumEnemiesShootingInCombat;

#if LIGHT_EFFECTS_SUPPORT
	ioConstantLightEffect m_PlayerLightEffect;
	fwWantedLightEffect   m_WantedLightEffect;

#if LIGHT_EFFECT_FLASH_FOR_LOW_HEALTH
	ioFlashingLightEffect m_FlashingPlayerLightEffect;
#endif // LIGHT_EFFECT_FLASH_FOR_LOW_HEALTH
#endif // LIGHT_EFFECTS_SUPPORT

	////////////////////////////////////////////////////////////////////////
	// Moved from CPedPlayer
	////////////////////////////////////////////////////////////////////////

	// Timers to avoid player getting stuck permanently hit by water cannon
	float			m_fHitByWaterCannonTimer;			// Total time hit by water cannon (constantly decreases by a cooldown rate)
	float			m_fHitByWaterCannonImmuneTimer;	// Total time of water cannon immunity left
	float			m_fRoundCoverRadius;

	// Targeting object
	CPlayerPedTargeting m_targeting;

	// Vehicle clipset streaming
	CVehicleClipRequestHelper m_VehicleClipRequestHelper;

	float			m_fTimeToNextCreateInteriorShockingEvents;

	// Dynamic cover point
	CCoverPoint		m_dynamicCoverPoint;
		
	Vector3			m_vRoundCoverCenter;
	Vector3			m_vDynamicCoverPointLastKnownPosition;

	// How long has the player been in cover?
	float			m_fTimeElapsedUsingCoverSeconds;
	float			m_fTimeElapsedHidingInCoverSeconds;

	float			m_fInsideCornerDistance;
	float			m_fCoverEdgeDistance;

	// Ped election period
	// NOTE: Used to elect charging enemy and smoke grenade thrower.
	//       Only one election is active at any given time.
	u32				m_CandidateRegistrationEndTimeMS;
	u32				m_BestCandidateAcceptanceEndTimeMS;

	// Charging enemies
	u32				m_ChargedByEnemyHistoryMS[CHARGER_HISTORY_SIZE];
	RegdConstPed	m_BestEnemyChargerCandidatePed;

	// Smoke grenade thrower enemies
	u32				m_SmokeThrownByEnemyHistoryMS[SMOKE_THROWER_HISTORY_SIZE];
	RegdConstPed	m_BestEnemySmokeThrowerCandidatePed;

	fwClipSetRequestHelper	m_meleeTauntClipSetHelper;
	fwClipSetRequestHelper	m_meleeVariationClipSetHelper;
	
	// Charging enemies goal positions
	// -
	enum EChargeGoalPosition 
	{
		CGP_Left,
		CGP_Right,
		CGP_Rear,
		
		CGP_MAX_NUM
	};
	enum EChargeGoalPositionStatus 
	{
		CGPS_Unknown,
		CGPS_PendingNavmeshCheck,
		CGPS_PendingProbeCheck,
		CGPS_Invalid,
		CGPS_Valid
	};
	class CCandidateChargeGoalPosition
	{
	public:
		
		CCandidateChargeGoalPosition() : BANK_ONLY(m_InitialPosition(V_ZERO),) m_Position(V_ZERO), m_Status(CGPS_Unknown) {}

		inline void Reset() { BANK_ONLY(m_InitialPosition.ZeroComponents()); m_Position.ZeroComponents(); m_Status = CGPS_Unknown; }

#if __BANK
		Vec3V m_InitialPosition;
#endif // __BANK
		Vec3V m_Position;
		u8 m_Status;
	};
	// Position of the player for current charge goals processing
	// If this position is zeroed out, it indicates the charge goal position processing is dormant
	Vec3V			m_vChargeGoalPlayerPosition;
	// List of candidate charge goal positions
	CCandidateChargeGoalPosition m_CandidateChargeGoals[CGP_MAX_NUM];
	CNavmeshClosestPositionHelper m_CandidateChargeGoalNavmeshHelpers[CGP_MAX_NUM];
	CAsyncProbeHelper m_CandidateChargeGoalProbeHelpers[CGP_MAX_NUM];
	// -

	u32 m_nMeleeEquippedWeaponHash;
	u32 m_nMeleeClipSetReleaseTimer;	

	// Combat Loitering
	Vec3V			m_vCombatLoiteringPosition;
	float			m_fTimeElapsedLoiteringSeconds;
	u32				m_uNextLoiterDistCheckTimeMS;

	float			m_fTimeBeenSprinting;
	float			m_fSprintStaminaUpdatePeriod;
	float			m_fSprintStaminaDurationMultiplier;	
	float			m_fSprintApplyDamageTimer;

	RegdPed			m_pCollidedWithPed;
	u32				m_uCollidedWithPedTimer;

	u32				m_uLastTimeBumpedPedInVehicle;

	// An array of CPlayerLOSInfos, one for each possible network player
	atFixedArray<CPlayerLOSInfo, MAX_NUM_PHYSICAL_PLAYERS> m_aPlayerLOSInfo;
	
	// Keep track of the last weapon picked up
	u32		m_TimeLastWeaponPickedUp;
	u32		m_LastWeaponHashPickedUp;

	// Keep track of the last time an arrest failed on this player when this player was in a vehicle
	u32		m_LastTimeBustInVehicleFailed;
	u32		m_LastTimeArrestAttempted;
	
	// Keep track of the last time we warped into / out of a vehicle (to prevent insta-warp if button mashing).
	u32		m_LastTimeWarpedIntoVehicle;
	u32		m_LastTimeWarpedOutOfVehicle;

	//mjc- Data moved from CPlayerPedData
	//
	CWanted			m_Wanted;

	RegdPhy			m_pFollowTarget;
	float			m_fMaxFollowDistance;
	float			m_fFollowTargetRadius;
	// When is the last time we shouted our target's position?
	u32		m_LastShoutTargetPositionTime;

	Vec3V			m_vFollowTargetOffset;

#if ENABLE_HORSE
	CPlayerHorseSpeedMatch m_InputHorse;
#endif

	// Sprint variables
	float			m_fRunSprintSpeedMultiplier;
	float			m_fSprintEnergy;
	float			m_fMaxSprintEnergy;
	float			m_fSprintControlCounter;
	float			m_fCachedSprintMultThisFrame;
#if KEYBOARD_MOUSE_SUPPORT
	u32				m_uTimeBikeSprintPressed;
#endif

	// Weapon/damage info
	u32				m_nLastChangeWeaponFrame;
	f32				m_fWeaponDamageModifier;
	f32				m_fWeaponDefenseModifier;
	f32				m_fWeaponMinigunDefenseModifier;
	f32				m_fMeleeWeaponDamageModifier;
	f32				m_fMeleeUnarmedDamageModifier;
	f32				m_fMeleeWeaponDefenseModifier;
	f32				m_fMeleeWeaponForceModifier;
	f32				m_fVehicleDamageModifier;
	f32				m_fVehicleDefenseModifier;
	s32				m_PlayerGroup;
	f32				m_fMaxExplosiveDamage;
	f32				m_fExplosiveDamageModifier;
	f32				m_fWeaponTakedownDefenseModifier;

	Color32			m_ParachuteSmokeTrailColor;

	// These variables keep track of reckless driving. The script can work out whether the player should be pulled over.
	u32				m_LastTimePlayerHitCar;
	u32				m_LastTimePlayerHitPed;
	u32				m_LastTimePlayerHitBuilding;
	u32				m_LastTimePlayerHitObject;
	u32				m_LastTimePlayerDroveOnPavement;
	u32				m_LastTimePlayerRanLight;
	u32				m_LastTimePlayerDroveAgainstTraffic;

	u32				m_uTimeToEnableAgitation;

	// Class managing player health recharge
	CPlayerHealthRecharge	m_healthRecharge;

	// Class managing player endurance recharge
	CPlayerEnduranceManager m_enduranceManager;

	// Class managing player damage over time effects
	CPlayerDamageOverTime m_damageOverTime;

	// PURPOSE:	Keeps track of noise indications in MP. This is basically the
	//			size of a sonar blip on the stealth radar, decaying over time.
	float	m_StealthNoise;

	// PURPOSE:  allows scripts to tweak how noisy a player is while walking normally.
	float	m_fNormalStealthMultiplier;

	// PURPOSE:  allows scripts to tweak how noisy a player is while sneaking.
	float	m_fSneakingStealthMultiplier;

	// PURPOSE:  allows code to dampen the vertical motion on the root
	// Interpolates the local root z value between its current animated value and m_fDampenRootTargetHeight
	// weight 0 is not dampened (local root z = current animated value ) 
	// weight 1 is fully dampened (local root z = m_fDampenRootTargetHeight
	float	m_fDampenRootWeight;	   
	float	m_fDampenRootTargetWeight;
	float	m_fDampenRootTargetHeight;

	//PURPOSE: Stores information for the talking system.
	u32     m_uLastTimePedMadeMobileCommentOnWeirdPlayer;

	//PURPOSE: Stores damage information for local player. E.g. used to bump target threat if a friendly ped fires on local player.
	u32		m_uLastTimeDamagedLocalPlayer;

	//PURPOSE: Simulate player gait input variables	
	float m_fSimulateGaitMoveBlendRatio;	// the simulated move blend ratio: stand, walk, run, sprint...
	float m_fSimulateGaitHeading;	// the simulated heading. Range from -PI to PI.
	float m_fSimulateGaitDuration;		// how long the simulated gait is going to last, in seconds.
	float m_fSimulateGaitTimerCount;	
	float m_fSimulateGaitStartHeading;

	// PURPOSE: Vehicle control variables
	float m_fLaggedYawControl;
	float m_fLaggedPitchControl;
	float m_fLaggedRollControl;
	float m_fLaggedSteerControl;

	float m_fTimeBetweenRandomControlInputs;
	float m_fRandomControlYaw;
	float m_fRandomControlPitch;
	float m_fRandomControlRoll;
	float m_fRandomControlThrottle;
	float m_fRandomControlSteerModifier;
	float m_fControlSettledTime;
	float m_fPreviousSteerValue;
	float m_fTimeSinceApplyingRandomControl;
	float m_fTimeSinceLastDrivingAtSpeed;

#if FPS_MODE_SUPPORTED
	// PURPOSE: Stores left hand position, set from CTaskHumanLocomotion::ProcessPostCamera and used in CTaskPlayerOnFoot::IsStateValidForIK (see url:bugstar:2248699).
	Vec3V m_vLeftHandPositionFPS;
#endif	// FPS_MODE_SUPPORTED

#if RSG_PC && FPS_MODE_SUPPORTED
	// PURPOSE: Used to initialize m_bInFPSScopeState based on menu preference (B*2318618).
	// Keep in sync with "MENU_OPTION_FPS_DEFAULT_AIM_TYPE" in pausemenu.xml
	enum eDefaultAimTypeFPS
	{
		FPS_AIM_NORMAL = 0,
		FPS_AIM_IRON_SIGHTS = 1,
	};
#endif	// RSG_PC && FPS_MODE_SUPPORTED

	// PURPOSE: Modify the ability for peds to see you when in stealth
	float m_fStealthPerceptionModifier;

	u32 m_uTimeExitVehicleTaskFinished;

	//
	//mjc- END moved from CPlayerPedData
	//

	struct sNearVehicle
	{
		const CVehicle* veh;
		u32 time;
	};
	sNearVehicle m_nearVehicles[MAX_NEAR_VEHICLES];	

	// PURPOSE:	Used by UpdateRecklessDriving() to keep track of the previous node pair to speed up
	//			the search for a new node pair.
	CNodeAddress	m_RecklessDrivingHintFrom;
	CNodeAddress	m_RecklessDrivingHintTo;

	// End of the world variables.
	float	m_fSwimBoundsEscapeTimer;	

	u32 m_uSprintAimBreakOutTime;

	// Previous variation information
	ePedVarComp		m_PreviousVariationComponent;
	u32				m_PreviousVariationDrawableId;
	u32				m_PreviousVariationDrawableAltId;
	u32				m_PreviousVariationTexId;
	u32				m_PreviousVariationPaletteId;

	ePedVarComp m_ParachuteVariationComponent;
	u32 m_ParachuteVariationDrawable;
	u32 m_ParachuteVariationAltDrawable;
	u32 m_ParachuteVariationTexId;
	u32 m_ParachuteModelHash;
	u32 m_ReserveParachuteModelHash;
	u32 m_ParachutePackModelHash;
	bool m_bRetainParachuteAfterTeleport;

	u32 m_SwitchedToOrFromFirstPersonCount;
	u32 m_SwitchedToFirstPersonCount;
	u32 m_SwitchedToFirstPersonFromThirdPersonCoverCount;

	float m_fFallHeightOverride;

	//////////////////////////////////////////////////////////////////////////
	// Special ability
	SpecialAbilityType	m_eSpecialAbilitiesMP[PSAS_MAX];

	//////////////////////////////////////////////////////////////////////////
	// Arcade
	CArcadePassiveAbilityEffects m_arcadeAbilityEffects;
	CPlayerArcadeInformation m_arcadeInformation;

	//////////////////////////////////////////////////////////////////////////
	// Veh melee
	bool m_bVehMeleePerformingRightHit;	

	bool m_bHasScriptedWeaponFirePos;
	Vec3V m_vScriptedWeaponFiringPos;

	// -----------------------------------------------------------------------
	//
	// WARNING - START
	// THE FOLLOW CODE IS OPTIMIZED!  DON'T FUCK WITH IT!
	//
	s16	m_iPlayerRoadLinkLocalIndex;
	u8 m_nearVehiclePut;
	u8 m_uNumAiAttemptingArrestOnPlayer;

	bool		m_bMayFollowTargetGroupMembers : 1;
	bool		m_bEnableMeleeClipSetReleaseTimer : 1;
	bool		m_bEnableBlueToothHeadset : 1;
	bool		m_bSimulateGaitNoInputInterruption : 1;	// don't break out on input changes, it just times out.

public:
	bool		PortablePickupPending : 1;		// Set when we are trying to carry a portable pickup
	bool 		bDoesNotGetTired : 1;
	bool		bFastReload : 1;
	bool		bFireProof : 1;
	bool		bCanDoDriveBy : 1;
	bool		bCanBeHassledByGangs : 1;
	bool		bCanUseCover : 1;
	bool		bEnableControlOnDeath : 1;
	bool		bHasDamagedAtLeastOnePed : 1;				// including the animal type
	bool		bHasDamagedAtLeastOneNonAnimalPed : 1;		// Excluding the animal type
	bool		bHasDamagedAtLeastOneVehicle : 1;
	//	bool		bAfterRemoteVehicleExplosion : 1;
	//	bool		bCreateRemoteVehicleExplosion : 1;
	//	bool		bFadeAfterRemoteVehicleExplosion : 1;
	bool		bSpecifyInitialCoverDirection : 1;
	bool		bInitialCoverDirectionFaceLeft : 1;

private:
	bool			m_bIsShelteredAbove : 1;				// Whether the player is sheltered above (occasional linetest)
	bool			m_bCanMoveLeft : 1;
	bool			m_bCanMoveRight : 1;
	bool			m_bInsideCorner : 1;
	bool			m_bCoverGeneratedByDynamicEntity : 1;
	bool			m_bIsRoundCover : 1;
	bool			m_bHitPed : 1;

	bool 			m_bPlayerSprintDisabled : 1;
	bool			m_bDisplayMobilePhone : 1;
	bool			m_bStopNearbyVehicles : 1;
	bool			m_bCanLeaveParachuteSmokeTrail : 1;
	bool			m_bIsInsideMovingTrain : 1;
	bool			m_bAllowControlInsideMovingTrain : 1;

	RegdVeh			m_pTrainPedIsInside;

	bool m_bSimulateGaitInput : 1;		// if the simulated gait is on.
	bool m_bSimulateGaitUseRelativeHeading : 1;		// the heading is local to the ped.

	bool m_bAutoGiveParachuteWhenEnterPlane : 1;	// if give player a parachute automatically when he gets into a plane or helicopter.
	bool m_bAutoGiveScubaGearWhenExitVehicle : 1;
	
	bool	m_bHasWorldExtentsSpawnedAShark : 1;

	bool				m_bSprintReplenishing : 1;		// sprint energy is replenishing, needs to be above zero to re-start sprinting
	bool				m_bFreeAiming : 1;
	bool				m_bCanBeDamaged : 1;
	bool				m_DamageAllowedFromSetPlayerControl : 1;
	bool				m_bHasControlOfRagdoll : 1;	// player can apply forces to the ragdoll, to move around

	bool				m_bAllowedToPickUpNonMissionObjects : 1;	// script will set this to false when it needs to use LB button for something else like launching the internet script
	bool				m_bCanCollectMoneyPickups : 1;	// if false, the player is not allowed to collect dropped money pickups (used for cops in cops 'n' crooks)
	bool				m_bAmbientMeleeMoveDisabled : 1; // if true, the player cannot perform the ambient melee move

	// Vehicle node information
	bool				m_bPlayerOnHighway : 1;		// Player is on highway. Ped population can be reduced.
	bool				m_bPlayerOnWideRoad : 1;	// If player is on a wide road and goes fast. Ped population can be reduced.
	bool				m_bNodesNearbyInZ : 1;		// Are there nodes present nearby the player at a similar height (+/- 10 meters). If not height requirement should be dropped when creating cars.

	bool				m_bCanUseSearchLight : 1;	// Player is allow to use vehicle searchlights

	bool				m_bDisableAgitation : 1;

	bool				m_bWasAiming : 1;

	bool				m_bWasLightAttackPressed : 1;
	bool				m_bWasAlternateAttackPressed : 1;

	bool				m_bUseHighFiringAttackBoundary : 1;

#if FPS_MODE_SUPPORTED
	// Variables used for FPS state changes
	float				m_fRNGTimerFPS;
	bool				m_bWasRunAndGunning : 1;
	bool				m_bInFPSScopeState : 1;
	bool				m_bFiredInFPSFullAimState : 1;
	bool				m_bFiredGunInFPS : 1;
	bool				m_bFiredDuringFPSSprint : 1;
	float				m_fExitCombatRollToScopeTimerInFPS;
	float				m_fCoverHeadingCorrectionAimHeading;
	bool				m_bForceFPSRNGState : 1;
#endif	// FPS_MODE_SUPPORTED

#if FPS_MODE_SUPPORTED && KEYBOARD_MOUSE_SUPPORT
	static bool			sm_bShouldUpdateScopeStateFromMenu;
#endif	// FPS_MODE_SUPPORTED && KEYBOARD_MOUSE_SUPPORT

	// 
	// WARNING - END
	//
	// -----------------------------------------------------------------------

	bool				m_bPlayerLeavePedBehind : 1;

	bool m_bAllowStuntJumpCamera;

	// Statics
	static const float ms_fInteriorShockingEventFreq;
	static bool		ms_bHasDisplayedPlayerQuitEnterCarHelpText;

	// Melee 
	static dev_u32 ms_nMeleeClipSetReleaseDuration;

	// Aim tuning data
	static bank_float ms_PlayerSoftAimBoundary;
#if FPS_MODE_SUPPORTED
	static bank_float ms_PlayerFPSFireBoundaryLow;
	static bank_float ms_PlayerFPSFireBoundaryHigh;
#endif
	static bank_float ms_PlayerLowAimBoundry;
	static bank_bool  ms_ReverseAimBoundarys;
	static bank_float ms_PlayerSoftTargetSwitchBoundary;
	// PURPOSE:	The time it takes for m_StealthNoise to decay to 1/e of its value, in seconds.
	static bank_float ms_StealthNoiseExponentialDecayTimeConstant;

	// PURPOSE:	The value at which m_StealthNoise will be considered too small to update, forcing it to zero.
	static bank_float ms_StealthNoiseMinValToCareAbout;

	// The rate at which the root damping blends in or out
	static bank_float ms_fDampenRootRate;

	// Guard range for player leaving the map.
	static bool ms_bPlayerBoundaryInitialized;
	static spdAABB ms_WorldLimitsPlayerAABB;

#if __ASSERT
	static bool		ms_ChangingPlayerModel;
#endif // __ASSERT
#if __BANK
	static	bool	ms_bDisplayRecklessDriving;
	static	bool	ms_bDisplayNumEnemiesInCombat;
	static	bool	ms_bDisplayCoverTracking;
	static	bool	ms_bDisplayCombatLoitering;
	static	bool	ms_bDebugCombatLoitering;
	static	bool	ms_bDisplayNumEnemiesShootingInCombat;
	static	bool	ms_bDebugCandidateChargeGoalPositions;
	static	bool	ms_bDebugDrawCandidateChargePositions;
#endif

public:

	// Debug static bools.
#if !__FINAL
		static bool		ms_bFakeWorldExtentsTresspassing;
#endif
};

// ------------------------------------------------------------------------------

class CAnimSpeedUps
{
public:

	// PURPOSE: Wraps the logic to decide if we should use MP anim rates
	static bool ShouldUseMPAnimRates();

	// PURPOSE:	Tuning parameter struct.
	struct Tunables : public CTuning
	{
		Tunables();

		float m_MultiplayerClimbStandRateModifier;
		float m_MultiplayerClimbRunningRateModifier;
		float m_MultiplayerClimbClamberRateModifier;
		float m_MultiplayerEnterExitJackVehicleRateModifier;
		float m_MultiplayerLadderRateModifier;
		float m_MultiplayerReloadRateModifier;
		float m_MultiplayerCoverIntroRateModifier;
		float m_MultiplayerIdleTurnRateModifier;
		bool  m_ForceMPAnimRatesInSP;

		PAR_PARSABLE;
	};

	// PURPOSE: Instance of tunable task params
	static Tunables	ms_Tunables;	
};


// ------------------------------------------------------------------------------

// PURPOSE: Returns a pointer to the player car (or NULL if there isn't one)
CVehicle*		FindPlayerVehicle	(CPlayerInfo* pPlayer = NULL, bool bReturnRemoteVehicle = false);
// PURPOSE: Returns a pointer to the player info player ped (or NULL if there isn't one)
CPed*			FindPlayerPed		(CPlayerInfo* pPlayer);
// PURPOSE: Returns a pointer to the main player ped (or NULL if there isn't one)
CPed*			FindPlayerPed		();
// PURPOSE: Returns a pointer to the spectator car (or NULL if there isn't one)
CVehicle*		FindFollowVehicle	(CPlayerInfo* pPlayer = NULL);
// PURPOSE: Returns a pointer to the spectator ped (or NULL if there isn't one)
CPed*			FindFollowPed		(CPlayerInfo* pPlayer = NULL);

// Havoc points being awarded for bad things done
#define HAVOC_UPROOTOBJECT		(2)
#define HAVOC_KILLPED			(10)
#define HAVOC_DENTCAR			(2)	
#define HAVOC_BLOWUPCAR			(20)
#define HAVOC_CAUSEEXPLOSION	(5)
#define HAVOC_PEDDAMAGED		(1)

#define CHASE_KILLPED			(5)
#define CHASE_DENTCAR			(1)
#define CHASE_BLOWUPCAR			(10)
#define CHASE_CAUSEEXPLOSION	(7)

#endif
