// FILE		: PlayerSpecialAbility.h
// PURPOSE	: Load in special ability information and manage the abilities
// CREATED	: 23-03-2011

#ifndef PLAYER_SPECIAL_ABILITY_H
#define PLAYER_SPECIAL_ABILITY_H

// Rage headers
#include "atl/array.h"
#include "atl/hashstring.h"
#include "parser/macros.h"
#include "vector/vector3.h"
#include "fwanimation/clipsets.h"

// Framework headers

// Game headers
#include "scene/RegdRefTypes.h"
#include "stats/StatsTypes.h"
#include "control/replay/ReplaySettings.h"

class CPlayerSpecialAbility;
class CPed;
class CPedModelInfo;
class CVehicle;

enum AbilityChargeEventType
{
	ACET_MISSION_FAILED = 0,
	ACET_FALL_OVER,
	ACET_ANGRY,
	ACET_DAMAGE,
	ACET_CHANGED_HEALTH,
	ACET_GOT_PUNCHED,
	ACET_RAMMED_BY_CAR,
	ACET_RAMMED_INTO_CAR,
	ACET_RAN_OVER_PED,
	ACET_PICKUP,

	ACET_RESET_LOW_HEALTH,
	ACET_NEAR_BULLET_MISS,
	ACET_PUNCHED,
	ACET_WEAPON_TAKEDOWN,

	ACET_HEADSHOT,

	ACET_NEAR_VEHICLE_MISS,
	ACET_DRIFTING,
	ACET_DRIVING_TOWARDS_ONCOMING,
	ACET_CRASHED_VEHICLE,
	ACET_DRIVING_IN_CINEMATIC,
	ACET_TOP_SPEED,
	ACET_VEHICLE_JUMP,

	ACET_BUMPED_INTO_PED,
    ACET_EXPLOSION_KILL,
	ACET_STEALTH_KILL,
	ACET_KNOCKOUT,
	ACET_BULLET_DAMAGE,
	ACET_KILL,

	ACET_MAX
};

/*
RUFFIAN RM: If you are adding new special ability types you will also need to update
abilityNames in PlayerSpecialAbility.cpp
*/
enum SpecialAbilityType
{
	SAT_NONE = -1,
	SAT_CAR_SLOWDOWN = 0,
	SAT_RAGE,
	SAT_BULLET_TIME,
	SAT_SNAPSHOT,
	SAT_INSULT,

	SAT_ENRAGED,
	SAT_GHOST,
	SAT_SPRINT_SPEED_BOOST,
	SAT_NITRO_COP,
	SAT_NITRO_CROOK,
	SAT_EMP,
	SAT_SPIKE_MINE,
	SAT_FULL_THROTTLE,
	SAT_PUMPED_UP,
	SAT_BACKUP,
	SAT_OIL_SLICK,
	SAT_KINETIC_RAM,

	SAT_MAX
};

enum eFadeCurveType
{
    FCT_NONE = 0,
    FCT_LINEAR,
    FCT_HALF_SIGMOID,
    FCT_SIGMOID,
    FCT_MAX
};

class CSpecialAbilityData
{
public:
	CSpecialAbilityData()
		: duration(30)
        , initialUnlockedCap(12)
		, minimumChargeForActivation(0)
		, allowsDeactivationByInput(true)
		, allowsActivationOnFoot(true)
		, allowsActivationInVehicle(true)
		, timeWarpScale(1.0f)
		, damageMultiplier(1.0f)
		, defenseMultiplier(1.0f)
		, staminaMultiplier(1.0f)
		, sprintMultiplier(1.0f)
		, steeringMultiplier(1.0f)
		, depletionMultiplier(1.0f)
		, chargeMultiplier(1.0f)
		, fxName()
		, outFxName()
		, activeAnimSet()
	{
	}
	s32 duration;
	s32 initialUnlockedCap;
	s32 minimumChargeForActivation;	// ability cannot be activated until charge has reached this minimum level
	bool allowsDeactivationByInput;		// true if this ability can be deactivated by player input
	bool allowsActivationOnFoot;
	bool allowsActivationInVehicle;
	float timeWarpScale;
	float damageMultiplier;
	float defenseMultiplier;
	float staminaMultiplier;
	float sprintMultiplier;
	float steeringMultiplier;
	float depletionMultiplier;
    float chargeMultiplier;
	atHashString fxName;
	atHashString outFxName;
	atHashString activeAnimSet;

	SpecialAbilityType type;

	PAR_SIMPLE_PARSABLE;
};

class CPlayerSpecialAbilityManager
{
public:
	
	// cloud tunable data
	static s32 ms_CNCAbilityEnragedDuration;
	static float ms_CNCAbilityEnragedHealthReduction;
	static float ms_CNCAbilityEnragedStaminaReduction;
	static s32 ms_CNCAbilityGhostDuration;
	static s32 ms_CNCAbilitySprintBoostDuration;
	static float ms_CNCAbilitySprintBoostIncrease;
	static s32 ms_CNCAbilityCallDuration;
	static s32 ms_CNCAbilityNitroDuration;
	static s32 ms_CNCAbilityOilSlickDuration;
	
	static s32 ms_CNCAbilityEMPVehicleDuration;
	static s32 ms_CNCAbilityOilSlickVehicleDuration;
	static float ms_CNCAbilityOilSlickFrictionReduction;
	static float ms_CNCAbilityBullbarsAttackRadius;
	static float ms_CNCAbilityBullbarsImpulseAmount;
	static float ms_CNCAbilitySpikeMineRadius;
	static s32 ms_CNCAbilitySpikeMineDurationArming;
	static s32 ms_CNCAbilitySpikeMineDurationLifeTime;

	static void			InitTunables();

public:
	static void			Init(const char* fileName);
	static void			Shutdown();

	static CPlayerSpecialAbility* Create(SpecialAbilityType type);
	static void			Destroy(CPlayerSpecialAbility* ability);

	static s32			GetSmallCharge();
	static s32			GetMediumCharge();
	static s32			GetLargeCharge();
	static s32			GetContinuousCharge();

	static void			ChargeEvent(AbilityChargeEventType eventType, CPed* eventPed, float data = 0.f, u32 iData = 0, void* pData = NULL);

	static bool			IsAbilityUnlocked(u32 modelNameHash);
	static void			SetAbilityStatus(u32 modelNameHash, bool unlocked, bool isArcadePlayer = false);

	static void			ProcessControl(CPed* ped);
	static bool			UpdateSpecialAbilityTrigger(CPed *ped);
	static bool			CanTriggerSpecialPedAbility();
	static bool			CanUnTriggerSpecialPedAbility();
	static bool			IsSpecialAbilityBlockingInputs();
	static bool			HasSpecialAbilityDisabledSprint();
	static bool			CanActivateSprintToggle();
	static void			ResetActivateSprintToggle() { activateSprintToggle = false; }

    static eFadeCurveType GetFadeCurveType();
    static float        GetHalfSigmoidConstant();
    static float        GetSigmoidConstant();
    static float        GetFadeInTime();
    static float        GetFadeOutTime();

    static float        GetChargeMultiplier() { return instance.chargeMultiplier * specialChargeMultiplier; }
    static void         SetChargeMultiplier(float val) { instance.chargeMultiplier = val; }

	static void			UpdateDlcMultipliers();
	static void			SetSpecialChargeMultiplier(float mult) { specialChargeMultiplier = mult; }

#if __BANK
	static void			AddWidgets(bkBank& bank);
	static void			LoadCallback();
	static void			SaveCallback();

    static void         LinearCurveSelect();
    static void         SigmoidCurveSelect();
    static void         HalfSigmoidCurveSelect();
    static void         UnlockedCapacityCallback();

    static u32          unlockedCapacityPercentage;
#endif

	static const CSpecialAbilityData* GetSpecialAbilityData(SpecialAbilityType type);

private:

	static void			Reset();
	static void			Load(const char* fileName);
	void				PostLoad();
		
	atArray<CSpecialAbilityData> specialAbilities;
	s32					smallCharge;
	s32					mediumCharge;
	s32					largeCharge;
	s32					continuousCharge;

    eFadeCurveType      fadeCurveType;
    float               halfSigmoidConstant;        // [0.1, 10.f] where 0.1f is steep and 10.f is close to linear
    float               sigmoidConstant;            // [0.1, 1.f] where 0.1f is steep and 1.f more relaxed
    float               fadeInTime;
    float               fadeOutTime;

	float				chargeMultiplier;

	static float		specialChargeMultiplier;

	static u32			pendingActivationTime1;
	static u32			pendingActivationTime2;
	static u32			timeForActivation;
	static u32			activationTime;
	static u32			activationTime_CNC;
	static u32			audioActivationTime;
	static u32			canTriggerAbilityTime;
	static bool			pendingActivation;
	static bool			needsResetForActivation;
	static bool			canTriggerAbility;
	static bool			canUnTriggerAbility;
	static bool			blockingInputs;
	static bool			specialAbilityDisabledSprint;
	static bool			activateSprintToggle;

	static bool			abilityUnlocked[5];

	

	static CPlayerSpecialAbilityManager instance;

	PAR_SIMPLE_PARSABLE;
};

class CPlayerSpecialAbility
{
#if __BANK
	friend class CPlayerSpecialAbilityManager;
#endif

public:
						CPlayerSpecialAbility(const CSpecialAbilityData* data);
						~CPlayerSpecialAbility();

	void				UpdateFromStat();

	void				Process(CPed *pPed);

	bool				CanBeActivated() const;
	bool				CanBeSelectedInArcadeMode() const;


	bool				Activate();
	void				Deactivate();
	void				DeactivateNoFadeOut();
	void				Toggle(CPed* ped);
	void				UnlockToggle() { canToggle = true; }
	bool				CanToggle() const { return canToggle; }

	void				Reset();

	bool				IsActive() const { return isActive; }
    bool                IsMeterFull() const;

#if GTA_REPLAY
	void				OverrideActiveStateForReplay(bool active) { isActive = active;}
	void				OverrideFadingOutForReplay(bool fadingOut) { isFadingOut = fadingOut; }
	void				OverrideRemainingNormalizedForReplay(f32 remainingNormalized) { replayRemainingNormalized = remainingNormalized; }
#endif //GTA_REPLAY

	void				FillMeter(bool ignoreActive);
	void				DepleteMeter(bool ignoreActive);
	void				AddToMeter(s32 value, bool ignoreActive);
	void				AddToMeterNormalized(float value, bool ignoreActive, bool applyLocalMultiplier = true);
	void				AddToMeterContinuous(bool ignoreActive, float multiplier);

	void				AddSmallCharge(bool ignoreActive) { AddToMeter(CPlayerSpecialAbilityManager::GetSmallCharge(), ignoreActive); }
	void				AddMediumCharge(bool ignoreActive) { AddToMeter(CPlayerSpecialAbilityManager::GetMediumCharge(), ignoreActive); }
	void				AddLargeCharge(bool ignoreActive) { AddToMeter(CPlayerSpecialAbilityManager::GetLargeCharge(), ignoreActive); }

	void				RemoveSmallCharge(bool ignoreActive) { AddToMeter(-CPlayerSpecialAbilityManager::GetSmallCharge(), ignoreActive); }
	void				RemoveMediumCharge(bool ignoreActive) { AddToMeter(-CPlayerSpecialAbilityManager::GetMediumCharge(), ignoreActive); }
	void				RemoveLargeCharge(bool ignoreActive) { AddToMeter(-CPlayerSpecialAbilityManager::GetLargeCharge(), ignoreActive); }

	s32					GetDuration() const;
	float				GetDamageMultiplier() const { return data->damageMultiplier; }
	float				GetDefenseMultiplier() const;
	float				GetStaminaMultiplier() const;
	float				GetSprintMultiplier() const;
	float				GetSteeringMultiplier() const { return  data->steeringMultiplier; }

	// returns current remaining, rounded to a 32 bit int
	// if roundDown is true, value will be rounded down to nearest whole number
	// otherwise value will be rounded to nearest whole number
	s32					GetRemaining(bool roundDown = true) const;
	float				GetRemainingNormalized() const;
	float				GetRemainingPercentageForDisplay() const;

	s32					GetMinimumChargeForActivation() const { return data->minimumChargeForActivation; }
	bool				GetAllowsDeactivationByInput() const { return data->allowsDeactivationByInput; }

    float               GetCapacity() const;
	float				GetCapacityPercentageForDisplay() const;

	SpecialAbilityType	GetType() const { return data->type; }
	void 				GetSpecialAbilityFx(atHashString &fxmod) const;

	atHashString		GetActiveAnimSetHashString		(void) const	{ return data->activeAnimSet; }
#if !__FINAL
	const char*			GetActiveAnimSetName			(void) const	{ return data->activeAnimSet.GetCStr(); }
#endif	//	!__FINAL
	u32					GetActiveAnimSetID				(void) const	{ return data->activeAnimSet.GetHash(); }

	float				GetFxStrength() const;
	bool				ShouldApplyFx() const;
	bool				IsPlayingFx() const;

	void				ApplyFx() const;
	void				StartFx() const;
	void				StopFx() const;

	void				TriggerNearVehicleMiss();
	void				AbortNearVehicleMiss();
	void				TriggerLowHealth();
	void				AbortLowHealth();
	void				TriggerExplosionKill();
	void				TriggerBumpedIntoPed(void* pedEa);
	void				TriggerRanOverPed(float damage);
	void				TriggerVehicleJump(CVehicle* pVeh);
    void                HandleVehicleJump();

	bool				IsAbilityUnlocked() const;
	bool				IsFadingOut() const { return isFadingOut; }

	void				ProcessVfx();

	void				EnableAbility() { isEnabled = true; }
	void				DisableAbility() { isEnabled = false; }
	bool				IsAbilityEnabled() const { return isEnabled; }

	u32					GetToggleTime() const { return toggleTime; }

	float				GetUnlockedCapacity() const { return unlockedCapacity; }
	
	void				GiveKillCharge() { gaveKillCharge = true; }
	bool				HasGivenKillCharge() const { return gaveKillCharge; }

	void				SetInternalTimeWarp(float fTimeWarp);
	float				GetInternalTimeWarp() const;

	void                StartKineticRam();
	void                StopKineticRam();
	void                HandleKineticRam();

#if __BANK
	void				ResetStat();
#endif
private:
	const CSpecialAbilityData* data;	
	StatId				m_statAbility;
    StatId              m_statUnlockedAbility;

	fwClipSetRequestHelper m_ClipsetRequestHelper;

	RegdVeh				jumpVeh;
	u32					vehJumpStartTime;
	u32					vehJumpTouchTime;

	u32					toggleTime;
	u32					lastTimeWarpFrame;

	float				chargeToAdd;
	float				currentRemaining;
	float				sliceToAdd;

    float               unlockedCapacity;       // this is stored as absolute vlaue, like duration, but when saved in the STAT variable it needs to be a percentage [0, 100]
    float               accumulatedCharge;      // keeps track of how much charge we accumulate (not subtracting when used), used to unlock greater capacity

	float				addContinuousMultiplier;

	u32					lastExplosionKillTime;
	void*				lastPedBumpedInto;		// keep this around as void so nobody gets the idea to dereference the pointer
	u32					lastPedBumpTime;
	u32					lastPedRanOverTime;

	u32					lastNearVehicleMissTime;
    float               nearMissChargeAmount;
	bool				appliedLowHealthCharge;

	bool				isVehJumpInProgress;

	bool				isEnabled;				// when disabled the meter can be filled but the ability can't be activated
	bool				isActive;
	bool				addingContinuous;

	bool				canToggle;

	bool				gaveKillCharge;			// don't give more than one kill charge per frame (e.g. kill + headshot)

	bool				isFadingOut;

#if GTA_REPLAY
	float				replayRemainingNormalized;
#endif
	bool				CanBeActivatedInArcadeMode(CPed* pPed) const;
	s32					GetTunedOrDefaultDuration(s32 abilityDuration) const;

	static float		timeToAddCharge;		// how long it takes to add the current charge, used to animate rather than give whole chunks	

	u32					kineticRamExplosionCount; 
	u32					kineticRamActivationTime; 
};


#endif // PLAYER_SPECIAL_ABILITY_H
