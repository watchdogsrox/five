#ifndef PED_TARGET_EVALUATOR_H
#define PED_TARGET_EVALUATOR_H

// Rage headers
#include "system/bit.h"
#include "vectormath/vec3v.h"

// Game headers
#include "Peds/PedWeapons/PlayerPedTargeting.h"
#include "Task/System/Tuning.h"

// Forward declarations
namespace rage { class Vector3; }
class CEntity;
class CPed;

//////////////////////////////////////////////////////////////////////////
// Target entity.
//////////////////////////////////////////////////////////////////////////

struct sTargetEntity
{
	CEntity* pEntity;
	float fHeadingScore;
	float fDistanceScore;
	float fMeleeScore;
	float fTaskModifier;
	float fAimModifier;
	float fThreatScore;
	bool bRejectedInVehicle;
	bool bRejectLockOnForDistance;
	bool bTargetBehindLowVehicle;
	bool bOutsideMinHeading;
	bool bCantSeeFromCover;

	float GetTotalScore() const { return (fHeadingScore * fDistanceScore * fMeleeScore * fTaskModifier * fAimModifier) + fThreatScore; }
	float GetTotalScoreNonNormalized() const { return ((fHeadingScore + fDistanceScore + fMeleeScore) * fTaskModifier * fAimModifier) + fThreatScore; }

	static int Sort(const sTargetEntity *a, const sTargetEntity *b) 
	{ 
		int iCmp = (a->GetTotalScore() > b->GetTotalScore()) ? -1 : 1;
		return iCmp;
	}

	static int SortNonNormalized(const sTargetEntity *a, const sTargetEntity *b) 
	{ 
		int iCmp = (a->GetTotalScoreNonNormalized() > b->GetTotalScoreNonNormalized()) ? -1 : 1;
		return iCmp;
	}
};

//////////////////////////////////////////////////////////////////////////
// CPedTargetEvaluator
//////////////////////////////////////////////////////////////////////////

class CPedTargetEvaluator
{
public:

	// PURPOSE:	Tuning parameter struct.
	struct Tunables : public CTuning
	{
		Tunables();

		float m_DefaultTargetAngularLimitMelee;
		float m_DefaultTargetAngularLimitMeleeLockOnNoStick;
		float m_DefaultTargetDistanceWeightMelee;
		float m_DefaultTargetDistanceWeightMeleeFPS;
		float m_DefaultTargetDistanceWeightMeleeRunning;
		float m_DefaultTargetDistanceWeightMeleeRunningFPS;
		float m_DefaultTargetHeadingWeightMelee;
		float m_DefaultTargetHeadingWeightMeleeFPS;
		float m_DefaultTargetHeadingWeightMeleeRunning;
		float m_DefaultTargetHeadingWeightMeleeRunningFPS;
		float m_MeleeLockOnStickWeighting;
		float m_MeleeLockOnCameraWeighting;
		float m_MeleeLockOnCameraWeightingNoStick;
		float m_MeleeLockOnPedWeightingNoStick;
		float m_DefaultTargetAngularLimitVehicleWeapon;
		float m_DefaultTargetCycleAngularLimit;
		float m_PrioHarmless;
		float m_PrioNeutral;
		float m_PrioNeutralInjured;
		float m_PrioIngangOrFriend;
		float m_PrioPotentialThreat;
		float m_PrioMissionThreat;
		float m_PrioMissionThreatCuffed;
		float m_PrioPlayer2PlayerEveryone;
		float m_PrioPlayer2PlayerStrangers;
		float m_PrioPlayer2PlayerAttackers;
		float m_PrioPlayer2Player;
		float m_PrioPlayer2PlayerCuffed;
		float m_PrioScriptedHighPriority;
		float m_PrioMeleeDead;
		float m_PrioMeleeCombatThreat;
		float m_PrioMeleeDownedCombatThreat;
		float m_PrioMeleeInjured;
		float m_PrioMeleePotentialThreat;

		float m_DownedThreatModifier;
		float m_InCoverScoreModifier;
		float m_ClosestPointToLineDist;
		float m_ClosestPointToLineBonusModifier;
		float m_MeleeHeadingOverride;
		float m_MeleeHeadingOverrideRunning;
		float m_MeleeHeadingFalloffPowerRunning;
		float m_DefaultMeleeRange;
		bool m_UseMeleeHeadingOverride;
		bool m_LimitMeleeRangeToDefault;
		bool m_DebugTargetting;

		float m_TargetDistanceWeightingMin;
		float m_TargetDistanceWeightingMax;
		float m_TargetHeadingWeighting;
		u32 m_TargetDistanceMaxWeightingAimTime;

		float m_TargetDistanceFallOffMin;
		float m_TargetDistanceFallOffMax;

		bool m_UseNonNormalisedScoringForPlayer;
		bool m_RejectLockIfBestTargetIsInCover;

		float m_RejectLockonHeadingTheshold;
		float m_HeadingScoreForCoverLockOnRejection;

		float m_AircraftToAircraftRejectionModifier;

		u32 m_TimeForTakedownTargetAcquiry;

		u32 m_TimeToIncreaseFriendlyFirePlayer2PlayerPriority;

		PAR_PARSABLE;
	};

	// PURPOSE: Instance of tunable params
	static Tunables	ms_Tunables;	

	//! DMKH. Note: This should match targeting mode enum in commands_player.sch.
	enum
	{
		TARGETING_OPTION_GTA_TRADITIONAL = 0,
		TARGETING_OPTION_ASSISTED_AIM,
		TARGETING_OPTION_ASSISTED_FREEAIM,
		TARGETING_OPTION_FREEAIM,
		MAX_TARGETING_OPTIONS,
	};

	static bool IsAssistedAim(int eOption)
	{
		switch(eOption)
		{
		case(TARGETING_OPTION_GTA_TRADITIONAL):
		case(TARGETING_OPTION_ASSISTED_AIM):
			return true;
		case(TARGETING_OPTION_ASSISTED_FREEAIM):
		case(TARGETING_OPTION_FREEAIM):
			return false;
		default:
			Assertf(0, "Invalid targeting option!");
		}

		return false;
	}

	static bool IsFreeAim(int eOption)
	{
		switch(eOption)
		{
		case(TARGETING_OPTION_GTA_TRADITIONAL):
		case(TARGETING_OPTION_ASSISTED_AIM):
			return false;
		case(TARGETING_OPTION_ASSISTED_FREEAIM):
		case(TARGETING_OPTION_FREEAIM):
			return true;
		default:
			Assertf(0, "Invalid targeting option!");
		}

		return false;
	}

	// Settings that control the score for each possible target, with the highest scoring one chosen
	enum TargetScoringFlags
	{
		TS_PED_HEADING										= BIT0,		// Heading relative to player heading
		TS_CAMERA_HEADING									= BIT1,		// Heading relative to camera heading
		TS_DESIRED_DIRECTION_HEADING						= BIT2,		// Heading relative to ped desired direction
		TS_CYCLE_LEFT										= BIT3,		// Heading relative to cycling through the current target
		TS_CYCLE_RIGHT										= BIT4,		// Heading relative to cycling through the current target
		TS_ACCEPT_HEADING_FAILURES							= BIT5,		// Will ignore targets who fail any heading scoring
		TS_CALCULATE_MELEE_ACTION							= BIT6,		// Ensures that the target can have a melee action performed against them.
		TS_CHECK_PEDS										= BIT7,		// Include peds in target search
		TS_CHECK_OBJECTS									= BIT8,		// Include objects in target search
		TS_CHECK_VEHICLES									= BIT9,		// Include vehicles in target search 
		TS_USE_THREAT_SCORING								= BIT10,	// Evaluates the threat of the target and includes this in the target scoring
		TS_USE_DISTANCE_SCORING								= BIT11,	// Evaluates the threat of the target and includes this in the target scoring
		TS_USE_MELEE_SCORING								= BIT12,	// Calculate score based on melee conditions
		TS_ACCEPT_INJURED_AND_DEAD_PEDS						= BIT13,	// Allows peds that are injured (onGround) or dead to be targeted
		TS_ALLOW_LOCKON_EVEN_IF_DISABLED					= BIT14,	// Allows lock-on to be available even if its been disabled (for melee weapons)
		TS_IGNORE_LOW_OBJECTS								= BIT15,	// Ignore low objects in collision checks
		TS_ACCEPT_ALL_VEHICLES								= BIT16,	// Allows all vehicles to be targeted (irrespective of their bVehicleCanBeTargetted flag)
		TS_ACCEPT_ALL_OCCUPIED_VEHICLES						= BIT17,	// Allows all occupied vehicles to be targeted (irrespective of their bVehicleCanBeTargetted flag)
		TS_USE_MODIFIED_LOS_DISTANCE_IF_SET					= BIT18,	// When doing los checks it will take the entity's targetable distance into account
		TS_PREFER_INTACT_VEHICLES							= BIT19,	// Scores WRECKED vehicles lower than intact vehicles (requires TS_USE_THREAT_SCORING)
		TS_LOCKON_TO_PLAYERS_IN_VEHICLES					= BIT20,	// Allows players to lockon to other players in vehicles
		TS_IGNORE_VEHICLES_IN_LOS							= BIT21,	// Ignore vehicles in LOS tests
		TS_HOMING_MISSLE_CHECK								= BIT22,	// Allows us to check the homing flag and eliminate designated script defined entities
		TS_ALLOW_LOCK_ON_IF_TARGET_IS_FRIENDLY				= BIT23,	// Allows player ped to target peds marked as "CPED_CONFIG_FLAG_AllowPlayerLockOnIfFriendly"
		TS_DO_AIMVECTOR_BONUS_AND_TASK_MODIFIER				= BIT24,	// Give bonus if target is close to aim vector & do task modifier.
		TS_USE_MELEE_LOS_POSITION							= BIT25,	// Use Melee lock on position.
		TS_USE_COVER_VANTAGE_POINT_FOR_LOS					= BIT26,	// Use Cover vantage point to allow lock on of peds in cover.
		TS_ALLOW_DRIVER_LOCKON_TO_AMBIENT_PEDS				= BIT27,	// Allow lock on to ambient peds in a vehicle.
		TS_ALWAYS_KEEP_BEST_TARGET							= BIT28,	// This is to bypass MP logic that will throw out a best target if they are not in combat
		TS_USE_NON_NORMALIZED_SCORE							= BIT29,	// Flag to use non normalized scoring (normalized score requires you to use HEADING)
		TS_DO_COVER_LOCKON_REJECTION						= BIT30,	// Do Lock on rejection to prioritise peds in cover.
		TS_VEH_WEP_BONE_HEADING								= BIT31,	// Heading relative to vehicle weapon bone

		// Bit sets
		DEFAULT_TARGET_FLAGS				= TS_CAMERA_HEADING|TS_CHECK_PEDS|TS_CHECK_OBJECTS|TS_USE_THREAT_SCORING|TS_USE_DISTANCE_SCORING|TS_CHECK_VEHICLES|TS_USE_MODIFIED_LOS_DISTANCE_IF_SET,
		MELEE_TARGET_FLAGS					= TS_CHECK_PEDS|TS_USE_MELEE_SCORING|TS_USE_DISTANCE_SCORING|TS_USE_MODIFIED_LOS_DISTANCE_IF_SET|TS_ALLOW_LOCKON_EVEN_IF_DISABLED|TS_ALWAYS_KEEP_BEST_TARGET|TS_USE_NON_NORMALIZED_SCORE,
		CYCLE_LEFT_TARGET_FLAGS				= TS_CAMERA_HEADING|TS_CYCLE_LEFT|TS_CHECK_PEDS|TS_CHECK_OBJECTS|TS_USE_THREAT_SCORING|TS_CHECK_VEHICLES|TS_USE_MODIFIED_LOS_DISTANCE_IF_SET,
		CYCLE_RIGHT_TARGET_FLAGS			= TS_CAMERA_HEADING|TS_CYCLE_RIGHT|TS_CHECK_PEDS|TS_CHECK_OBJECTS|TS_USE_THREAT_SCORING|TS_CHECK_VEHICLES|TS_USE_MODIFIED_LOS_DISTANCE_IF_SET,
		AIRCRAFT_WEAPON_TARGET_FLAGS		= TS_PED_HEADING|TS_CHECK_VEHICLES|TS_ACCEPT_ALL_VEHICLES|TS_ACCEPT_ALL_OCCUPIED_VEHICLES|TS_CHECK_PEDS|TS_USE_DISTANCE_SCORING|TS_USE_THREAT_SCORING|TS_PREFER_INTACT_VEHICLES|TS_ALLOW_LOCKON_EVEN_IF_DISABLED|TS_LOCKON_TO_PLAYERS_IN_VEHICLES|TS_IGNORE_VEHICLES_IN_LOS,
		VEHICLE_WEAPON_TARGET_FLAGS			= TS_PED_HEADING|TS_CHECK_VEHICLES|TS_ACCEPT_ALL_VEHICLES|TS_CHECK_PEDS|TS_USE_DISTANCE_SCORING|TS_USE_THREAT_SCORING|TS_PREFER_INTACT_VEHICLES|TS_ALLOW_LOCKON_EVEN_IF_DISABLED|TS_LOCKON_TO_PLAYERS_IN_VEHICLES|TS_IGNORE_VEHICLES_IN_LOS,
	};

	struct tHeadingLimitData 
	{
		tHeadingLimitData()
			: fHeadingAngularLimit(CPlayerPedTargeting::ms_Tunables.GetDefaultTargetAngularLimit())
			, fWideHeadingAngulerLimit(CPlayerPedTargeting::ms_Tunables.GetDefaultTargetAngularLimit())
			, fHeadingAngularLimitClose(-1.0f)
			, fHeadingAngularLimitCloseDistMin(-1.0f)
			, fHeadingAngularLimitCloseDistMax(-1.0f)
			, fAimPitchLimitMin(360.0f)
			, fAimPitchLimitMax(360.0f)
			, fHeadingFalloffPower(2.0f) //sq by default.
			, fPedHeadingWeighting(1.0f)
			, fCameraHeadingWeighting(1.0f)
			, fStickHeadingWeighting(1.0f)
			, bPedHeadingNeedsTargetOnScreen(false)
			, bCamHeadingNeedsTargetOnScreen(false)
			, bStickHeadingNeedsTargetOnScreen(false)
		{}

		tHeadingLimitData(float fLimit, float fLimitClose = -1.0f, float fLimitCloseDistMin = -1.0f, float fLimitCloseDistMax = -1.0f)
			: fHeadingAngularLimit(fLimit)
			, fWideHeadingAngulerLimit(fLimit)
			, fHeadingAngularLimitClose(fLimitClose)
			, fHeadingAngularLimitCloseDistMin(fLimitCloseDistMin)
			, fHeadingAngularLimitCloseDistMax(fLimitCloseDistMax)
			, fAimPitchLimitMin(360.0f)
			, fAimPitchLimitMax(360.0f)
			, fHeadingFalloffPower(2.0f) //sq by default.
			, fPedHeadingWeighting(1.0f)
			, fCameraHeadingWeighting(1.0f)
			, fStickHeadingWeighting(1.0f)
			, bPedHeadingNeedsTargetOnScreen(false)
			, bCamHeadingNeedsTargetOnScreen(false)
			, bStickHeadingNeedsTargetOnScreen(false)
		{}	

		float GetAngularLimit() const { return fHeadingAngularLimit; }
		float GetAngularLimitForDistance(float fDistance) const ;

		float fHeadingAngularLimit;
		float fHeadingAngularLimitClose;
		float fHeadingAngularLimitCloseDistMin;
		float fHeadingAngularLimitCloseDistMax;
		float fWideHeadingAngulerLimit;
		float fAimPitchLimitMin;
		float fAimPitchLimitMax;
		float fHeadingFalloffPower;
		float fPedHeadingWeighting;
		float fCameraHeadingWeighting;
		float fStickHeadingWeighting;
		bool bPedHeadingNeedsTargetOnScreen;
		bool bCamHeadingNeedsTargetOnScreen;
		bool bStickHeadingNeedsTargetOnScreen;
	};

	// Determines the current targeting mode based on current special abilities 
	static u32 GetTargetMode( const CPed* pTargetingPed );

	// Find the best target that matches the specified conditions
	static dev_float TARGET_DISTANCE_WEIGHTING_DEFAULT;
	static dev_float TARGET_HEADING_WEIGHTING_DEFAULT;
	static dev_float TARGET_DISANCE_FALLOFF_DEFAULT;

	struct tFindTargetParams
	{
		tFindTargetParams() : pTargetingPed(NULL)
			, iFlags(0)
			, fMaxDistance(0.0f)
			, fMaxRejectionDistance(0.0f)
			, pCurrentTarget(NULL)
			, pCurrentMeleeTarget(NULL)
			, pfThreatScore(NULL) 
			, fPitchLimitMin(0.0f) 
			, fPitchLimitMax(0.0f) 
			, fDistanceWeight(TARGET_DISTANCE_WEIGHTING_DEFAULT)
			, fHeadingWeight(TARGET_HEADING_WEIGHTING_DEFAULT)
			, fDistanceFallOff(TARGET_DISANCE_FALLOFF_DEFAULT)
			, pvDesiredStickOverride(NULL)
		{}

		const CPed* pTargetingPed;
		s32 iFlags;
		float fMaxDistance; 
		float fMaxRejectionDistance;
		const CEntity* pCurrentTarget;
		const CEntity* pCurrentMeleeTarget;
		float* pfThreatScore;
		tHeadingLimitData headingLimitData;
		float fPitchLimitMin;
		float fPitchLimitMax; 
		float fDistanceWeight;
		float fHeadingWeight;
		float fDistanceFallOff;
		Vector2 *pvDesiredStickOverride;
	};

	static CEntity* FindTarget(const tFindTargetParams &params);

	//! For legacy reasons, keep this about. TO DO - get rid of and use tFindTargetParams version.
	static CEntity* FindTarget(const CPed* pTargetingPed, 
		s32 iFlags, 
		float fMaxDistance, 
		float fMaxRejectionDistance,
		const CEntity* pCurrentTarget = NULL, 
		float* pfThreatScore = NULL, 
		const tHeadingLimitData &headingLimitData = tHeadingLimitData(), 
		float fPitchLimitMin = 0.0f, 
		float fPitchLimitMax = 0.0f, 
		float fDistanceWeight = TARGET_DISTANCE_WEIGHTING_DEFAULT, 
		float fHeadingWeight = TARGET_HEADING_WEIGHTING_DEFAULT,
		float fDistanceFallOff = TARGET_DISANCE_FALLOFF_DEFAULT);

	static void FindTargetInternal(const tFindTargetParams &testParams, CEntity* pEntity, const CPed *pTargetingPed, u32& iFlags, const Vector3 vTargetingPedPosition, const CWeaponInfo *pWeaponInfo, const bool bAcceptHeadingFailures, const bool bDoClosestPointToLineTest, const Vector3 vClosestStart, const Vector3 vClosestEnd, const bool bIsOnFootHoming ASSERT_ONLY(, int& nNumUncheckedEntities));

	// Check an individual target to determine if it is withing constraints
	static bool IsTargetValid( Vec3V_In vPosition, 
		Vec3V_In vTargetPosition, 
		s32 iFlags, 
		const Vector3& vComparisonHeading, 
		float fMaxDistance, 
		float& fHeadingDifferenceOut,
		bool& bOutsideMinHeading,
		bool& bInInclusionRangeFailedHeading,
		bool& bOnFootHomingTargetInDeadzone,
		const tHeadingLimitData &headingLimitData = tHeadingLimitData(), 
		float fPitchLimitMinInDegrees = 0.0f, 
		float fPitchLimitMaxInDegrees = 0.0f,
		bool  bIsOnFootHoming = false,
		bool  bIsFiring = false,
		bool  bAlreadyLockedOnToThisTarget = false,
		bool  bDoHomingBoxCheck = true);

	// Query if the targeting is disabled
	static bool GetIsTargetingDisabled(const CPed* pTargetingPed, const CEntity* pEntity, s32 iFlags);

	// Query whether the specified ped can be targeted if they are in a vehicle
	static bool GetCanPedBeTargetedInAVehicle(const CPed* pTargetingPed, const CPed* pTargetPed, s32 iFlags);

	// Is the line of sight blocked between the two peds?
	static bool GetIsLineOfSightBlockedBetweenPeds(const CPed* pPed1, 
		const CPed* pPed2, 
		const bool bIgnoreTargetsCover, 
		const bool bAlsoIgnoreLowObjects, 
		const bool bUseTargetableDistance, 
		const bool bIgnoreVehiclesForLOS, 
		const bool bUseCoverVantagePointForLOS,
		const bool bUseMeleePosForLOS = false,
		const bool bTestPotentialPedTargets = false);

	// Do we need a line of sight test?
	static bool GetNeedsLineOfSightToTarget(const CEntity* pEntity);

	// Is the line of sight blocked?
	static bool GetIsLineOfSightBlocked(const CPed* pTargetingPed, 
		const CEntity* pEntity, 
		const bool bIgnoreTargetsCover, 
		const bool bAlsoIgnoreLowObjects, 
		const bool bUseTargetableDistance, 
		const bool bIgnoreVehiclesForLOS, 
		const bool bUseCoverVantagePointForLOS,
		const bool bUseMeleePosForLOS,
		const bool bTestPotentialPedTargets,
		s32 iFlags);

	// Is other ped a threat?
	static float GetPedThreatAssessment( const CPed* pTargetingPed, const CEntity* pEntity );

	// Is other ped downed?
	static bool IsDownedPedThreat( const CPed* pPed, bool bConsiderFlinch = false );

	// Get range modifier for this entity.
	static bool CanBeTargetedInRejectionDistance(const CPed* pTargetingPed, const CWeaponInfo* pWeaponInfo, const CEntity *pEntity);

	static bool IsPlayerAThreat(const CPed* pTargetingPed, const CPed *pOtherPLayer);

	// Static constants - exposed to RAG
	static bank_bool  CHECK_VEHICLES;
	static bank_bool  USE_FLAT_HEADING;
	static bank_u32 DEBUG_TARGETSCORING_MODE;
	static bool DEBUG_TARGETSCORING_ENABLED;
	static bank_bool DEBUG_LOCKON_TARGET_SCRORING_ENABLED;
	static bank_bool DEBUG_VEHICLE_LOCKON_TARGET_SCRORING_ENABLED;
	static bank_bool DEBUG_MELEE_TARGET_SCRORING_ENABLED;
	static bank_bool DEBUG_TARGET_SWITCHING_SCORING_ENABLED;
	static bank_bool DEBUG_FOCUS_ENTITY_LOGGING;
	static bank_bool PROCESS_REJECTION_STRING;
	static bank_bool DISPLAY_REJECTION_STRING_PEDS;
	static bank_bool DISPLAY_REJECTION_STRING_OBJECTS;
	static bank_bool DISPLAY_REJECTION_STRING_VEHICLES;
	static bank_u32 DEBUG_TARGETSCORING_DIRECTION;
	static void EnableDebugScoring(bool bEnable) { DEBUG_TARGETSCORING_ENABLED = bEnable; }

	static s32 GetNumPotentialTargets() { return ms_aEntityTargets.GetCount(); }
	static sTargetEntity &GetPotentialTarget(s32 index) 
	{
		Assert(index < ms_aEntityTargets.GetCount());
		return ms_aEntityTargets[index];
	}

	static s32 FindPotentialTarget(const CEntity *pEntityToFind) 
	{
		for(s32 i = 0; i < ms_aEntityTargets.GetCount(); ++i)
		{
			if(ms_aEntityTargets[i].pEntity == pEntityToFind)
				return i;
		}
		return -1;
	}

	// Early out checks
	static bool CheckBasicTargetExclusions(const CPed* pTargetingPed, 
		const CEntity* pEntity, 
		s32 iFlags, 
		float fMaxLockOnDistance, 
		float fMaxDistance,
		bool* pbRejectedDueToVehicle = NULL,
		bool* pbOutsideLockOnRange = NULL);

#if __BANK
	static void AddWidgets();
#endif // __BANK

#if __BANK
	enum eDebugScoringMode {
		eDSM_None = 0,
		eDSM_Full,
		eDSM_Concise,
		eDSM_Max,
	};

	enum eDebugScoringDir {
		eDSD_None = 0,
		eDSD_Left,
		eDSD_Right,
		eDSD_Max,
	};
#endif

private:

	// Helper to get lock on pos (and to allow easy overrides of lock-on position for scoring purposes).
	static void GetLockOnTargetAimAtPos(const CEntity* pEntity, const CPed* pTargetingPed, const s32 iFlags, Vector3& aimAtPos);

	static const int MAX_ENTITIES = 96;
	static atFixedArray<sTargetEntity, MAX_ENTITIES>  ms_aEntityTargets;

	enum HeadingDirection
	{ 
		HD_BOTH = 0, 
		HD_JUST_LEFT,
		HD_JUST_RIGHT,
	};

	// Scoring functions
	static float GetScoreBasedOnHeading(const CEntity* pEntity, 
		const Vector3& vStartPos, 
		s32 iFlags, 
		const Vector3& vComparisonHeading, 
		float fOrientationDiff, 
		HeadingDirection eDirection,
		bool bInInclusionRangeFailedHeading,
		float fHeadingFalloff,
		const CPed* pTargetingPed);

	static float GetScoreBasedOnDistance(const CPed* pTargetingPed, const CEntity* pEntity, float fDistanceFallOff, s32 iFlags);
	static float GetScoreBasedOnThreatAssesment(const CPed* pTargetingPed, const CEntity* pEntity);
	static float GetScoreBasedOnMeleeAssesment(const CPed* pTargetingPed, const CEntity* pEntity, bool bIgnoreLowThreats = false);
	static float GetScoreBasedOnTask(const CPed* pTargetingPed, const CEntity* pEntity, s32 iFlags, bool &bDisallowAimBonus);

public:

	static void SetPedToIgnoreForDownedCheck(CPed *pPed) { ms_pIgnoreDownedPed = pPed; }

private:

	static RegdPed ms_pIgnoreDownedPed;
};

#endif // PED_TARGET_EVALUATOR_H
