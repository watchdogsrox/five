// Title	:	Wanted.h
// Author	:	Gordon Speirs
// Started	:	17/04/00

#ifndef INC_WANTED_H_
#define INC_WANTED_H_

// Rage headers
#include "grcore/debugdraw.h"		// Only for DEBUG_DRAW.
#include "Vector/Vector3.h"

// Game headers
#include "Crime.h"
#include "witness.h"
#include "debug/debug.h"
#include "parser/macros.h"
#include "scene/RegdRefTypes.h"
#include "task/Combat/TacticalAnalysis.h"
#include "Task/System/Tuning.h"


class CEntity;
class CPed;
class CVehicle;

enum eWantedLevel
{
	WANTED_CLEAN = 0,
	WANTED_LEVEL1,
	WANTED_LEVEL2,
	WANTED_LEVEL3,
	WANTED_LEVEL4,
	WANTED_LEVEL5,

	WANTED_LEVEL_LAST
};

enum SetWantedLevelFrom
{
	WL_FROM_INTERNAL = 0,
	WL_REMOVING,
	WL_FROM_TEST,
	WL_FROM_NETWORK,
	WL_FROM_CHEAT,
	WL_FROM_SCRIPT,
	WL_FROM_LAW_RESPONSE_EVENT,
	WL_FROM_TASK_NM_BALANCE,
	WL_FROM_TASK_NM_BRACE,
	WL_FROM_TASK_ARREST_PED,
	WL_FROM_TASK_BUSTED,
	WL_FROM_TASK_THREAT_RESPONSE_DUE_TO_EVENT,
	WL_FROM_TASK_THREAT_RESPONSE_INITIAL_DECISION,
	WL_FROM_CALL_POLICE_TASK,
	WL_FROM_REPORTED_CRIME,
	WL_FROM_BROKE_PAROLE,
	WL_FROM_INTERNAL_EVADING,

	WL_FROM_MAX_LOCATIONS
};

#if __BANK
	extern const char* sm_WantedLevelSetFromLocations[WL_FROM_MAX_LOCATIONS];
#endif

#define CRIMEFLAG_POSSESSION_GUN	(1<<0)
#define CRIMEFLAG_HIT_PED			(1<<1)
#define CRIMEFLAG_HIT_COP			(1<<2)
#define CRIMEFLAG_SHOOT_PED			(1<<3)
#define CRIMEFLAG_SHOOT_COP			(1<<4)
#define CRIMEFLAG_STEAL_CAR			(1<<5)
#define CRIMEFLAG_RUN_REDLIGHT		(1<<6)
#define CRIMEFLAG_RECKLESS_DRIVING	(1<<7)
#define CRIMEFLAG_SPEEDING			(1<<8)
#define CRIMEFLAG_RUNOVER_PED		(1<<9)

#define CRIMES_STAY_QD_TIME				15000
#define CRIMES_GET_REPORTED_TIME		10000
// Maximum of an hour
#define MAX_TIME_LAST_SEEN_COUNTER		3600000 

#define WANTED_CHANGE_DELAY_DEFAULT	(10000)		// Default: it takes this many milliseconds for the wanted level to go up.
#define WANTED_CHANGE_DELAY_QUICK	(1000)		// This long if the player commits crime spotted by police.
#define WANTED_CHANGE_DELAY_INSTANT	(0)			// Immediate wanted level

#define WANTED_LEVEL_HELI_1_DISPATCHED (WANTED_LEVEL3)
#define WANTED_LEVEL_HELI_2_DISPATCHED (WANTED_LEVEL5)
#define WANTED_LEVEL_2_HELIS_DISPATCHED (WANTED_LEVEL5)
#define WANTED_LEVEL_HELI_DISPATCHED_NETWORK (WANTED_LEVEL4)

#define WANTED_CIRCLE_POLICE_CORDON	(100.0f)	// At this distance the police will try to block the player.
//#define WANTED_CIRCLE_PLAYER_ESCAPE (140.0f)	// At this distance the player has escaped and his wanted level will drop to zero.

class CCrimeBeingQd
{
public:

	void AddWitness(CEntity& rEntity);
	bool HasLifetimeExpired() const;
	bool HasBeenWitnessedBy(const CEntity& rEntity) const;
	bool HasOnlyBeenWitnessedBy(const CEntity& rEntity) const;
	bool HasMaxWitnesses() const;
	bool HasWitnesses() const;
	bool IsInProgress() const;
	bool IsSerious() const;
	void ResetWitnesses();

	CEntity* GetVictim() const			{	return m_Victim;			}
	eCrimeType GetCrimeType() const		{	return CrimeType;			}
	Vec3V_ConstRef GetPosition() const	{	return RCC_VEC3V(Coors);	}
	eWitnessType GetWitnessType() const	{	return m_WitnessType;		}
	bool IsValid() const				{	return CrimeType != CRIME_NONE;	}
	void SetWitnessType(eWitnessType t)	{	m_WitnessType = t;			}
	bool WasAlreadyReported() const		{	return bAlreadyReported;	}
	u32 GetTimeOfQing() const			{ return TimeOfQing; }

	atFixedArray<RegdEnt, CCrimeWitnesses::sMaxPerCrime>	m_Witnesses;
	Vector3													Coors;
	RegdEnt													m_Victim;
	eCrimeType												CrimeType;
	eWitnessType											m_WitnessType;
	const CEntity*											m_CrimeVictimId; // Not safe to dereference, may have been deleted.
	u32														TimeOfQing;
	u32														Lifetime;
	u32														ReportDelay;
	u32														WantedScoreIncrement;
	bool													bAlreadyReported;
	bool													bPoliceDontReallyCare;
	bool													bMustBeWitnessed;
	bool													bMustNotifyLawEnforcement;
	bool													bOnlyVictimCanNotifyLawEnforcement;
};

#define USE_COP_COMMUNCIATION 1
//#define NUM_CIRCLE_RADAR_BLIPS (1)  // can remove this array once i know that the radius is ok

class CWantedBlips
{
public:
	CWantedBlips();

	void Init();
	void UpdateCopBlips(class CWanted* pWanted);
	void DisableCones();
	void CreateAndUpdateRadarBlips(class CWanted* pWanted);
	void EnablePoliceRadarBlips(bool bEnable) {m_PoliceRadarBlips = bEnable;}
	int GetBlipForPed(class CPed* pPed);
	bool GetIsBeingAttacked() {return m_beingAttacked;}
	void ResetBeingAttackedTimer() { m_beingAttackedTimer = 0;}

	void SetBlipSpriteToUseForCopPeds(s32 CopSprite, float fScale) { m_BlipSpriteToUseForCopPeds = CopSprite; m_fBlipScaleToUseForCopPeds = fScale; }
	void ResetBlipSpriteToUseForCopPeds() { m_BlipSpriteToUseForCopPeds = BLIP_POLICE_PED; m_fBlipScaleToUseForCopPeds = 0.4f; }
private:

	bool IsPedAttackingPlayer(CPed* pPed, CPed* pWantedPed);

	enum CopBlipResetFlags {
		BLIP_VALID=(1<<0),
		CONE_VALID=(1<<1)
	};
	struct CopBlipListStruct
	{
		int iBlipId;
		CEntityPoolIndexForBlip iPedPoolIndex;
		u32 m_lastFrameValid;
	};

	s32 m_BlipSpriteToUseForCopPeds;
	float m_fBlipScaleToUseForCopPeds;
	s32 m_aCircleRadarBlip;//[NUM_CIRCLE_RADAR_BLIPS];		// These blips are used to display the area the player needs to escape for the wanted level to drop.
	u32 m_beingAttackedTimer;
	bool m_PoliceRadarBlips;
	bool m_beingAttacked;
	atArray<CopBlipListStruct> m_CopBlips;
};



class CWanted
{

public:

	class Overrides
	{
	
	public:
	
		Overrides();
		~Overrides();
		
	public:
		
		float	GetDifficulty() const;
		u32		GetMultiplayerHiddenEvasionTimesOverride(s32 iWantedLevel) const;
		bool	HasDifficulty() const;
		void	Reset();
		void	ResetDifficulty();
		void	ResetEvasionTimeArray();
		void	SetDifficulty(float fDifficulty);
		void	SetMultiplayerHiddenEvasionTimesOverride(const s32 index, const u32 value);
		
	private:
		
		float m_fDifficulty;

		//Override for the hidden evasion timer
		atFixedArray<u32, WANTED_LEVEL_LAST> m_MultiplayerHiddenEvasionTimesOverride;
	};
	
public:

	struct Tunables : CTuning
	{
		struct WantedLevel
		{
			struct Difficulty
			{
				struct Calculation
				{
					struct Weights
					{
						Weights()
						{}

						float	m_WantedLevel;
						float	m_LastSpottedDistance;
						float	m_Randomness;

						PAR_SIMPLE_PARSABLE;
					};

					struct Decay
					{
						Decay()
						{}

						float	m_TimeEvadingForMaxValue;
						float	m_MaxValue;
						bool	m_DisableWhenOffMission;

						PAR_SIMPLE_PARSABLE;
					};
					
					Calculation()
					{}
					
					float	m_FromWantedLevel;
					Weights	m_Weights;
					Decay	m_Decay;
					
					PAR_SIMPLE_PARSABLE;
				};

				struct Helis
				{
					struct Refuel
					{
						Refuel()
						{}

						bool	m_Enabled;
						float	m_TimeBefore;
						float	m_Delay;

						PAR_SIMPLE_PARSABLE;
					};

					Helis()
					{}

					Refuel m_Refuel;

					PAR_SIMPLE_PARSABLE;
				};

				Difficulty()
				{}

				Calculation	m_Calculation;
				Helis		m_Helis;

				PAR_SIMPLE_PARSABLE;
			};
			
			WantedLevel()
			{}
			
			Difficulty	m_Difficulty;
			
			PAR_SIMPLE_PARSABLE;
		};
		
		struct Difficulty
		{
			struct Range
			{
				Range()
				{}
				
				float CalculateValue(float fDifficulty) const
				{
					float fValue = Clamp(fDifficulty, m_ValueForMin, m_ValueForMax);
					float fLerp = (fValue - m_ValueForMin) / (m_ValueForMax - m_ValueForMin);
					return Lerp(fLerp, m_Min, m_Max);
				}
				
				float m_Min;
				float m_Max;
				float m_ValueForMin;
				float m_ValueForMax;
				
				PAR_SIMPLE_PARSABLE;
			};
			
			struct Spawning
			{
				struct Scoring
				{
					struct Weights
					{
						Weights()
						{}

						Range	m_Distance;
						Range	m_Direction;
						Range	m_Randomness;

						PAR_SIMPLE_PARSABLE;
					};

					Scoring()
					{}

					Weights	m_Weights;

					PAR_SIMPLE_PARSABLE;
				};

				Spawning()
				{}

				Scoring	m_Scoring;
				Range	m_IdealDistance;
				Range	m_ChancesToForceWaitInFront;

				PAR_SIMPLE_PARSABLE;
			};

			struct Despawning
			{
				Despawning()
				{}

				Range	m_MaxFacingThreshold;
				Range	m_MaxMovingThreshold;
				Range	m_MinDistanceToBeConsideredLaggingBehind;
				Range	m_MinDistanceToCheckClumped;
				Range	m_MaxDistanceToBeConsideredClumped;

				PAR_SIMPLE_PARSABLE;
			};
			
			struct Peds
			{
				struct Attributes
				{
					struct Situations
					{
						struct Situation
						{
							Situation()
							{}

							Range	m_SensesRange;
							Range	m_IdentificationRange;
							Range	m_ShootRateModifier;
							Range	m_WeaponAccuracy;
							float	m_WeaponAccuracyModifierForEvasiveMovement;
							float	m_WeaponAccuracyModifierForOffScreen;
							float	m_WeaponAccuracyModifierForAimedAt;
							float	m_MinForDrivebys;

							PAR_SIMPLE_PARSABLE;
						};
						
						Situations()
						{}
						
						Situation	m_Default;
						Situation	m_InVehicle;
						Situation	m_InHeli;
						Situation	m_InBoat;
						
						PAR_SIMPLE_PARSABLE;
					};

					Attributes()
					{}

					Situations	m_Situations;
					Range		m_AutomobileSpeedModifier;
					Range		m_HeliSpeedModifier;

					PAR_SIMPLE_PARSABLE;
				};
				
				Peds()
				{}
				
				Attributes	m_Cops;
				Attributes	m_Swat;
				Attributes	m_Army;
				
				PAR_SIMPLE_PARSABLE;
			};
			
			struct Dispatch
			{
				Dispatch()
				{}
				
				Range m_TimeBetweenSpawnAttemptsModifier;
				
				PAR_SIMPLE_PARSABLE;
			};
				
			Difficulty()
			{}
			
			Spawning	m_Spawning;
			Despawning	m_Despawning;
			Peds		m_Peds;
			Dispatch	m_Dispatch;
			
			PAR_SIMPLE_PARSABLE;
		};

		struct Rendering
		{
			Rendering()
			{}

			bool m_Enabled;
			bool m_Witnesses;
			bool m_Crimes;

			PAR_SIMPLE_PARSABLE;
		};
		
		struct Timers
		{
			Timers()
			{}
			
			float m_TimeBetweenDifficultyUpdates;
			
			PAR_SIMPLE_PARSABLE;
		};
		
		// information on how crimes should interpreted by the wanted system
		struct CrimeInfo
		{
			virtual ~CrimeInfo() {};
			// penalty - the number of wanted points or wanted stars that this crime will incur on the player
			int m_WantedScorePenalty;
			// penalty - the number of wanted points or wanted stars that this crime will incur on the player
			int m_WantedStarPenalty;
			// lifetime - the time (ms) for which this crime will remain in the queue of crimes to process
			int m_Lifetime;
			// report delay - the time (ms) for which this crime will remain in the crime queue before it is reported
			int m_ReportDelay;
			// report duplicates - true if duplicates of this crime can be stored in the crime queue (allow repeat offences)
			bool m_ReportDuplicates;

			PAR_PARSABLE;
		};

		Tunables();
			
		WantedLevel	m_WantedClean;
		WantedLevel	m_WantedLevel1;
		WantedLevel	m_WantedLevel2;
		WantedLevel	m_WantedLevel3;
		WantedLevel	m_WantedLevel4;
		WantedLevel	m_WantedLevel5;
		Difficulty	m_Difficulty;
		Rendering	m_Rendering;
		Timers		m_Timers;
		u32			m_MaxTimeTargetVehicleMoving;
		u32			m_DefaultAmnestyTime;
		u32			m_DefaultHiddenEvasionTimeReduction;
		u32			m_InitialAreaTimeoutWhenSeen;
		u32			m_InitialAreaTimeoutWhenCrimeReported;
		float		m_ExtraStartHiddenEvasionDelay;
		
		atBinaryMap<CrimeInfo, atHashWithStringNotFinal> m_CrimeInfosDefault;
		atBinaryMap<CrimeInfo, atHashWithStringNotFinal> m_CrimeInfosArcadeCNC;

		PAR_PARSABLE;
	};

public:
	CWanted();
	~CWanted();

	void SetPed (CPed* pOwnerPed);
	CPed* GetPed() const				{	return m_pPed;	}
	
	Overrides m_Overrides;

	s32 	m_nWantedLevel;
//	s32 	m_nWantedLevelDelayed;
	s32 	m_nWantedLevelBeforeParole;
	u32 	m_LastTimeWantedLevelChanged;
	u32	m_TimeWhenNewWantedLevelTakesEffect;
	s32	m_TimeOutsideCircle;
	s32	m_TimeOfParole;
	float	m_fMultiplier;							        // New crimes have their wanted level contribution multiplied by this
	float	m_fDifficulty;
	float	m_fTimeSinceLastDifficultyUpdate;
	u8	m_nMaxCopCarsInPursuit;
	PhysicalPlayerIndex m_CausedByPlayerPhysicalIndex;      // Physical index of player that caused the local player to gain a wanted level. 

	Vec3V m_vInitialSearchPosition;							// This is set when first given a WL
	Vector3	m_LastSpottedByPolice;							// The coordinates where this player was last spotted. This defines the radius the player has to escape.
	Vector3 m_CurrentSearchAreaCentre;
	u32 m_iTimeSearchLastRefocused;				// The time the search was refocused, either when being spotted or crime being reported or updating WL
	u32	m_iTimeLastSpotted;						// The time the player was last spotted in milliseconds
	u32 m_iTimeFirstSpotted;					// The time the player was first spotted in milliseconds
	u32 m_iTimeHiddenEvasionStarted;			// This is the time that we started hidden evasion (usually the last spotted time or the time of police dispatch)
	u32 m_iTimeHiddenWhenEvasionReset;			// We will use when when restarting the hidden evasion if we weren't seen the last time it was reset
	u32 m_iTimeInitialSearchAreaWillTimeOut;	// The time that the initial search area will be considered left even if we are still in it (based on crime, spotted, etc)
	u32 m_uLastEvasionTime;
	float m_fTimeEvading;

	u16	m_nChanceOnRoadBlock;					// /127 for each rage_new roadsegment
	u8	m_PoliceBackOff : 1;					// If this is set the police will leave player alone (for cut-scenes)
	u8	m_PoliceBackOffGarage : 1;				// If this is set the police will leave player alone (for garages)
	u8	m_EverybodyBackOff : 1;					// If this is set then everybody (including police) will leave the player alone (for cut-scenes)
	u8	m_AllRandomsFlee : 1;					// If this is set then random peds (excluding police) will flee the player. Reset every frame.
	u8	m_AllNeutralRandomsFlee : 1;			// If this is set then all neutral random peds (excluding police) will flee the player. Reset every frame.
	u8	m_AllRandomsOnlyAttackWithGuns : 1;		// If this is set then randoms will only attack with guns and will flee if not armed with a gun. Reset every frame.
	u8	m_IgnoreLowPriorityShockingEvents : 1;	// If this is set then everybody (including police) will ignore lower priority shocking events (dead peds, melee)
	u8	m_DontDispatchCopsForThisPlayer : 1;		// The script says not to dispatch cops to apprehend this player.
	u8	m_MaintainFlashingStarAfterOffence : 1;		// If this is set the script doesn't want us to drop down from 1 * flashing. (cop pulling us over)
	u8	m_CenterWantedPosition : 1;				// If this is set then we will center the radius position to the players position
	u8	m_SuppressLosingWantedLevelIfHiddenThisFrame : 1;	// Disables the ability to evade the WL by hidden evasion for this frame
	u8	m_AllowEvasionHUDIfDisablingHiddenEvasionThisFrame : 1;	// Allow us to show hidden evasion HUD this frame even if we are preventing hidden evasion from ending via m_SuppressLosingWantedLevelIfHiddenThisFrame.
	u8	m_HasLeftInitialSearchArea : 1;			// Will be true if the player has left the initial search area when a WL is given, will stay true until WL is reset
	u8	m_DisableCallPoliceThisFrame : 1;		// This will prevent witnesses to crimes and shocking events from calling the police for this frame
	u8	m_LawPedCanAttackNonWantedPlayer : 1;	// If this is set then the player won't be removed from the target list in CTaskCombat::ProcessPreFSM if their wanted level is clean when being analysed by law peds. Essentially acts as a global "PED_CONFIG_FLAG_CanAttackNonWantedPlayerAsLaw" flag on law peds.
	float   m_fCurrentChaseTimeCounter;
	float   m_fCurrentChaseTimeCounterMaxStar;
	s32		m_iTimeBeforeAllowReporting;		// B*2343618: 
	bool   m_bTimeCounting;
	bool   m_bTimeCountingMaxStar;
	bool   m_bIsOutsideCircle;

	eWantedLevel m_WantedLevelOld;
	eWantedLevel m_WantedLevel;
	eWantedLevel m_WantedLevelBeforeParole;
	s32		 m_nNewWantedLevel;					// The rage_new wanted level takes effect when time is greater than m_TimeWhenNewWantedLevelTakesEffect

	u32		 m_WantedLevelCharId; // Used for stats to identify who this wanted level is for.

	RegdIncident m_wantedLevelIncident;

	CCrimeBeingQd*	CrimesBeingQd;				// Pointer to array of m_MaxCrimesBeingQd elements.
	int				m_MaxCrimesBeingQd;			// Number of elements allocated in CrimesBeingQd[].

	u32	m_uTimeHeliRefuelComplete;
	u32 m_LastTimeOfScannerRefuelReport;

	u32 m_DisableRelationshipGroupHashResetThisFrame;

	#define MAX_REPORTED_CRIMES_DEBUG 10
	#define MAX_REPORTED_CRIMES 32 
	
	struct sReportedCrime
	{
		sReportedCrime()
			: m_eCrimeType(CRIME_NONE)
			, m_uTimeReported(0)
			, m_uIncrement(0)
			, m_pCrimeVictim(NULL)
		{
		}

		sReportedCrime(eCrimeType crimeType, u32 timeReported, u32 incrementVal, const CEntity* pVictim)
			: m_eCrimeType(crimeType)
			, m_uTimeReported(timeReported)
			, m_uIncrement(incrementVal)
			, m_pCrimeVictim(pVictim)
		{
		}

		const CEntity*	m_pCrimeVictim; // Not safe to dereference, may have been deleted.
		eCrimeType	m_eCrimeType;
		u32			m_uTimeReported;
		u32			m_uIncrement;
	};

	atQueue<sReportedCrime, MAX_REPORTED_CRIMES> m_ReportedCrimes;

	RegdTacticalAnalysis m_pTacticalAnalysis;

	float m_fTimeSinceLastHostileAction;

	static eWantedLevel MaximumWantedLevel;
	static s32 		nMaximumWantedLevel;
	static s32		sm_iNumCopsOnFoot;
	static s32		sm_iMPVisibilityScanResetTime;
	static bool		bUseNewsHeliInAdditionToPolice;
	static bool		sm_bEnableCrimeDebugOutput;
	static bool		sm_bEnableRadiusEvasion;
	static bool		sm_bEnableHiddenEvasion;
	static CWantedBlips sm_WantedBlips;

#if __BANK
	static bool		sm_bRenderWantedLevelOfVehiclePasengers;
#endif

public:
	void Initialise();
	static void InitialiseStaticVariables(); // called by CDispatchData::Init

#if __BANK
	static void InitWidgets();
#endif
#if DEBUG_DRAW
	void DebugRender() const;
#endif

	void Reset();
	void Update(const Vector3 &PlayerCoors, const CVehicle *pVeh, const bool bLocal = true, const bool bUpdateStats = false);
	void UpdateWantedLevel(const Vector3 &Coors, const bool bCarKnown = true, const bool bRefocusSeach = true);
	void RegisterCrime(eCrimeType CrimeType, const Vector3 &Pos, const CEntity* CrimeVictimId, bool bPoliceDontReallyCare, CEntity* pVictim);
	void RegisterCrime_Immediately(eCrimeType CrimeType, const Vector3 &Pos, const CEntity* CrimeVictimId, bool bPoliceDontReallyCare, CEntity* pVictim);
	void UpdateStats( );

	void SetWantedLevel(const Vector3 &Coors, s32 NewLev, s32 delay, SetWantedLevelFrom eSetWantedLevelFrom, bool bIncludeNetworkPlayers = true, PhysicalPlayerIndex causedByPlayerPhysicalIndex = INVALID_PLAYER_INDEX);
	void CheatWantedLevel(s32 NewLev);
	void SetWantedLevelNoDrop(const Vector3 &Coors, s32 NewLev, s32 delay, SetWantedLevelFrom eSetWantedLevelFrom, bool bIncludeNetworkPlayers = true, PhysicalPlayerIndex causedByPlayerPhysicalIndex = INVALID_PLAYER_INDEX);
	void SetOneStarParoleNoDrop();
	void ClearParole();
	void PlayerBrokeParole(const Vector3 &Coors);
//	void ClearWantedLevelAndGoOnParole();

	eWantedLevel GetWantedLevel(void) const					{ return m_WantedLevel; }

	bool PoliceBackOff() const {return (m_PoliceBackOff || m_PoliceBackOffGarage || m_EverybodyBackOff);}
	
	// Get the time since the parameter last happened
	u32	GetTimeSince(u32 iTimeLastHappenedInMs) const;

	// Get the time since the player was last seen / the search area was last refocused / the hidden evasion started
	u32 GetTimeSinceLastSpotted() const;
	u32 GetTimeSinceSearchLastRefocused() const;
	u32 GetTimeSinceHiddenEvasionStarted() const;

	// Get/Set for disabling crimes on a per frame basis
	void SuppressCrimeThisFrame(eCrimeType CrimeType) { m_CrimesSuppressedThisFrame.Set(CrimeType, true); }
	bool IsCrimeSuppressedThisFrame(eCrimeType CrimeType) { return m_CrimesSuppressedThisFrame.IsSet(CrimeType); }
	void ResetSuppressedCrimes() { m_CrimesSuppressedThisFrame.Reset(); }

	void ClearQdCrimes();
	bool AddCrimeToQ(eCrimeType ArgCrimeType, const CEntity* CrimeVictimId, const Vector3 &Coors, bool bArgAlreadyReported, bool bArgPoliceDontReallyCare, u32 Increment, CEntity* pVictim);
	void UpdateCrimesQ();
	void ReportCrimeNow(eCrimeType CrimeType, const Vector3 &Coors, bool bPoliceDontReallyCare, bool bIncludeNetworkPlayers = true, u32 Increment = 0, const CEntity* pVictim = NULL, PhysicalPlayerIndex causedByPlayerPhysicalIndex = INVALID_PLAYER_INDEX);
	void ReportPoliceSpottingPlayer(CPed* pSpotter, const Vector3 &Coors, s32 oldWantedLevel, const bool bCarKnown = true, const bool bPlayerSeen = false);

	void ReportWitnessSuccessful(CEntity* pEntity);
	void ReportWitnessUnsuccessful(CEntity* pEntity);
	bool HasPendingCrimesQueued() const;
	bool HasPendingCrimesNotWitnessedByLaw() const;
	void SetAllPendingCrimesWitnessedByLaw();
	void SetAllPendingCrimesWitnessedByPed(CPed& rPed);

	void PassengersCommentOnPolice();
	void CopsCommentOnSwat();
//	static void UpdateEachFrame();

	// Called each frame to update any cops in the world
	void	UpdateCopPeds( void );
	void	UpdateRelationshipStatus( CRelationshipGroup* pRelGroup );

	bool	CopsAreSearching() const;
	void	SetCopsAreSearching(const bool bValue);

	void	PlayerEnteredVehicle( CVehicle* pVehicle );
	bool	GetWithinAmnestyTime() const;
	void	SetTimeAmnestyEnds(u32 TimeAmnestyEnds) { m_TimeCurrentAmnestyEnds = TimeAmnestyEnds; }
	u32		GetTimeWantedLevelCleared() { return m_TimeWantedLevelCleared; }
	void	SetShouldDelayLawResponse(bool bShouldDelayResponse) { m_bShouldDelayResponse = bShouldDelayResponse; }
	void	SetLastTimeWantedSetByScript(u32 currentTime) { m_uLastTimeWantedSetByScript = currentTime; }
	float	GetTimeUntilHiddenEvasionStarts() const { return m_fTimeUntilEvasionStarts; }

	PhysicalPlayerIndex     GetPhysicalIndexOfPlayerThatCausedCurrentWL() const { return m_CausedByPlayerPhysicalIndex; }

	void	SetHiddenEvasionTime(u32 currentTime);
	void	ForceStartHiddenEvasion();
	bool	ShouldRefocusSearchForCrime(eCrimeType CrimeType, eWantedLevel oldWantedLevel);

	SetWantedLevelFrom GetWantedLevelLastSetFrom() const { return m_WantedLevelLastSetFrom; }

	u32 GetLastTimeWantedLevelClearedByScript() const { return m_uLastTimeWantedLevelClearedByScript; }

	void	TriggerCopArrivalAudioFromPed(CPed* pCopPed);
	bool	TriggerCopSeesWeaponAudio(CPed* pCopPed, bool bIsWeaponSwitch);
	bool	ShouldTriggerCopSeesWeaponAudioForWeaponSwitch(const CWeaponInfo* pDrawingWeaponInfo);
	
	void ResetFlags();

	float CalculateChancesToForceWaitInFront() const;

	struct DespawnLimits
	{
		DespawnLimits()
		: m_fMaxFacingThreshold(-1.0f)
		, m_fMaxMovingThreshold(-1.0f)
		, m_fMinDistanceToBeConsideredLaggingBehind(250.0f)
		, m_fMinDistanceToCheckClumped(250.0f)
		, m_fMaxDistanceToBeConsideredClumped(10.0f)
		{}
		
		float m_fMaxFacingThreshold;
		float m_fMaxMovingThreshold;
		float m_fMinDistanceToBeConsideredLaggingBehind;
		float m_fMinDistanceToCheckClumped;
		float m_fMaxDistanceToBeConsideredClumped;
	};
	void CalculateDespawnLimits(DespawnLimits& rLimits) const;

	bool CalculateHeliRefuel(float& fTimeBefore, float& fDelay) const;

	float CalculateIdealSpawnDistance() const;
	
	struct PedAttributes
	{
		PedAttributes()
		: m_fShootRateModifier(1.0f)
		, m_fWeaponAccuracy(1.0f)
		, m_fAutomobileSpeedModifier(1.0f)
		, m_fHeliSpeedModifier(1.0f)
		, m_fSensesRange(60.0f)
		, m_fIdentificationRange(60.0f)
		, m_bCanDoDrivebys(false)
		{}
		
		float	m_fShootRateModifier;
		float	m_fWeaponAccuracy;
		float	m_fAutomobileSpeedModifier;
		float	m_fHeliSpeedModifier;
		float	m_fSensesRange;
		float	m_fIdentificationRange;
		bool	m_bCanDoDrivebys;
	};
	void CalculatePedAttributes(const CPed& rPed, PedAttributes& rAttributes) const;
	
	struct SpawnScoreWeights
	{
		SpawnScoreWeights()
		: m_fDistance(0.0f)
		, m_fDirection(0.0f)
		, m_fRandomness(1.0f)
		{}
		
		float m_fDistance;
		float m_fDirection;
		float m_fRandomness;
	};
	void CalculateSpawnScoreWeights(SpawnScoreWeights& rWeights) const;
	
	float CalculateTimeBetweenSpawnAttemptsModifier() const;
	
	bool	AreHelisRefueling() const;
	float	CalculateDifficulty() const;
	float	GetDifficulty() const;
	bool	HasBeenSpotted() const;
	void	OnHeliRefuel(float fDelay);
	bool	ShouldUpdateDifficulty() const;
	void	UpdateDifficulty();
	void	UpdateEvasion();
	void	UpdateTacticalAnalysis();
	void	UpdateTimers();

	s32	 GetValueForWantedLevel(s32 desiredWantedLevel);
	void SetResistingArrest(eCrimeType crimeType);

	void DisableRelationshipGroupResetThisFrame(u32 relGroup) { m_DisableRelationshipGroupHashResetThisFrame = relGroup; }  

	static void InitHiddenEvasionForPed(const CPed* pPed);
	static void RegisterHostileAction(const CPed* pPed);

	static eWantedLevel GetWantedLevelIncludingVehicleOccupants(Vector3 *lastSpottedByPolice);
	static s32 WorkOutPolicePresence(const Vec3V& Coors, float Radius);	
	static void AlertNearbyPolice(const Vec3V& Coors, float Radius, CPed* pCriminal, bool bAttemptArrest);	

	static void SetMaximumWantedLevel(s32 NewMaxLev);
	static void SetMaximumWantedValue(s32 nNewMaxWantedLevel) { nMaximumWantedLevel = nNewMaxWantedLevel; }

	static CWantedBlips& GetWantedBlips() {return sm_WantedBlips;}

	static bool ShouldCompensateForSlowMovingVehicle(const CEntity* pEntity, const CEntity* pTargetEntity);

	u8	m_nMaxCopsInPursuit;

#if __BANK
	char m_WantedLevelLastSetFromScriptName[128];
#endif


private:

	///////////////////
	// Member functions
	///////////////////

	bool EvaluateCrime(eCrimeType CrimeType, bool bPoliceDontReallyCare, u32 &Increment);	
	void ResetHiddenEvasion(const Vector3 &PlayerCoors, u32 currentTime, bool bPlayerSeen);
	void ResetTimeInitialSearchAreaWillTimeOut(u32 currentTime, bool bPlayerSeen);
	void RestartPoliceSearch(const Vector3 &PlayerCoors, u32 currentTime, bool bPlayerSeen);
	u32  GetCharacterIndex() const;
	void SetWantedLevelForVehiclePassengers(const CVehicle* pVehicle, const Vector3 &Coors, s32 NewLev, s32 delay, SetWantedLevelFrom eSetWantedLevelFrom, PhysicalPlayerIndex causedByPlayerPhysicalIndex);
	void ReportCrimeNowForVehiclePassengers(const CVehicle* pVehicle, const Vector3 &Coors, eCrimeType CrimeType, bool bPoliceDontReallyCare, PhysicalPlayerIndex causedByPlayerPhysicalIndex);

	bool GetCrimeInfo(eCrimeType crimeType, Tunables::CrimeInfo& crimeInfo);
	int GetCrimeReportDelay(eCrimeType crimeType);
	int GetCrimeLifetime(eCrimeType crimeType);
	int GetCrimePenalty(eCrimeType crimeType);

private:

	static const Tunables::WantedLevel* GetTunablesForWantedLevel(eWantedLevel nWantedLevel);

	///////////////////////////
	// Private member variables
	///////////////////////////

	CCrimeWitnesses		m_Witnesses;

	bool m_bStoredPoliceBackOff;
	bool m_bShouldDelayResponse;
	bool m_bHasCopArrivalAudioPlayed;
	bool m_bHasCopSeesWeaponAudioPlayed;
	u32	m_TimeCurrentAmnestyEnds;						// Time in milliseconds when our amnesty ends
	u32	m_TimeWantedLevelCleared;
	float m_fTimeUntilEvasionStarts;
	u32 m_uLastTimeWantedSetByScript;
	SetWantedLevelFrom m_WantedLevelLastSetFrom;	
	u32 m_uLastTimeWantedLevelClearedByScript;

	CPed* m_pPed; // The ped whose wanted level this is

	// Keeps track of crimes that are disabled this frame
	atFixedBitSet<MAX_CRIMES>	m_CrimesSuppressedThisFrame;

private:

	static Tunables sm_Tunables;
};


#endif
