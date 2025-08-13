#ifndef PED_ACCURACY_H
#define PED_ACCURACY_H

// Rage headers
#include "System/Bit.h"

// Framework headers
#include "fwutil/Flags.h"

// Game headers
#include "Debug/DebugDrawStore.h"
#include "Weapons/Info/WeaponInfo.h"

// Forward declarations
class  CPed;
class  CWeaponInfo;
struct sWeaponAccuracy;

//////////////////////////////////////////////////////////////////////////
// CPedAccuracyResetVariables
//////////////////////////////////////////////////////////////////////////

class CPedAccuracyResetVariables
{
public:

	// Flags are used to specify what accuracy modifiers are applied
	// This way, all accuracy modifiers are dealt with in one place, 
	// rather than littered throughout the code
	enum AccuracyFlags
	{
		Flag_TargetComingOutOfCover = BIT0,
		Flag_TargetInCover          = BIT1,
		Flag_TargetWalking          = BIT2,
		Flag_TargetRunning          = BIT3,
		Flag_TargetInAir            = BIT4,
		Flag_BlindFiring            = BIT5,
		Flag_TargetInCombatRoll		= BIT6,
		Flag_TargetDrivingAtSpeed	= BIT7,
	};

	CPedAccuracyResetVariables();

	// Reset all the variables
	void Reset();

	// Get the accuracy modifier
	float GetAccuracyModifier() const;

	// Set an accuracy flag
	void SetFlag(AccuracyFlags flag) { m_accuracyFlags.SetFlag(static_cast<s8>(flag)); }

	// Get an accuracy flag
	bool IsFlagSet(AccuracyFlags flag) const { return m_accuracyFlags.IsFlagSet(static_cast<s8>(flag)); }

private:

	//
	// Members
	//

	// Accuracy modifier - resets to 1.0f each frame, 
	// overridden by tasks to change the weapon accuracy in different situations
	// The smaller the number the less accurate the ped is
	mutable float m_fAccuracyModifier;

	//
	// Flags
	//

	enum InternalFlags
	{
		Flag_AccuracyCalculated = BIT0,
	};

	fwFlags8 m_accuracyFlags;
	mutable fwFlags8 m_internalFlags;
};

//////////////////////////////////////////////////////////////////////////
// CPedAccuracy
//////////////////////////////////////////////////////////////////////////

class CPedAccuracy
{
public:

#if DEBUG_DRAW
	static CDebugDrawStore ms_debugDraw;
#endif // DEBUG_DRAW

	CPedAccuracy();

	// Main Processing function
	void ProcessPlayerAccuracy(const CPed& ped);

	// Calculate the accuracy of the ped -
	// Modify the provided weapon accuracy based on the current state of the ped
	void CalculateAccuracyRange(const CPed& ped, const CWeaponInfo& weaponInfo, float fDistanceToTarget, sWeaponAccuracy& resultantAccuracy) const;

	//
	// Accessors/Settors
	//

	// Get the reset variables
	const CPedAccuracyResetVariables& GetResetVars() const { return m_resetVariables; }
	CPedAccuracyResetVariables& GetResetVars() { return m_resetVariables; }

	// Get the current recoil accuracy
	float GetRecoilAccuracy(const CWeaponInfo& weaponInfo) const { return weaponInfo.GetRecoilAccuracyMax() * m_fNormalisedTimeFiring; }

	static float fMPAmbientLawPedAccuracyModifier;

private:

	//
	void ProcessPlayerRecoilAccuracy(const CPed& ped);

	// Calculate player accuracy modifiers
	void CalculatePlayerAccuracyRange(const CPed& ped, const CWeaponInfo& weaponInfo, float fDistanceToTarget, sWeaponAccuracy& resultantAccuracy) const;

	// Calculate AI accuracy modifiers
	void CalculateAIAccuracyRange(const CPed& ped, const CWeaponInfo& weaponInfo, float fDistanceToTarget, sWeaponAccuracy& resultantAccuracy) const;

	// Calculate the recoil accuracy - this is based on how long we have been firing our weapon
	float CalculatePlayerRecoilAccuracy(const CPed& ped, const CWeaponInfo& weaponInfo) const;

	// Calculate the run & gun/assisted aiming R2 only mode accuracy
	float CalculatePlayerRunAndGunAccuracyModifier(const CPed& ped, const CWeaponInfo& weaponInfo) const;

	// Apply override in certain situations (such as sniper rifles which have perfect aim, so mods wouldn't apply)
	void ApplyPlayerRunAndGunAccuracyOverride(const CPed& ped, const CWeaponInfo& weaponInfo, sWeaponAccuracy& resultantAccuracy) const;

	// Calculate the camera accuracy modifier -
	// Based on the camera zoom level
	float CalculatePlayerCameraAccuracyModifier(const CWeaponInfo& weaponInfo) const;

	// Calculate the accuracy modifier when being recently hit in MP
	float CalculatePlayerRecentlyHitInMPAccuracyModifier(const CPed& ped) const;

	// Calculate the blind firing accuracy modifier
	float CalculateBlindFiringAccuracyModifier(const CPed& ped) const;

	// Calculate the hurt accuracy modifier
	float CalculateHurtAccuracyModifier(const CPed& ped) const;

	// Modify the accuracy based on the weapon.
	float ModifyAccuracyBasedOnWeapon(const CPed& ped, float fAccuracy, const CWeaponInfo& weaponInfo) const;

	// Modify the accuracy based on distance.
	float ModifyAccuracyBasedOnDistance(const CPed& ped, float fAccuracy, float fDistanceToTarget) const;

	// Modify the accuracy based on the number of enemies.
	float ModifyAccuracyBasedOnNumberOfEnemies(const CPed& ped, float fAccuracy) const;

	// Modify the accuracy based on ped type
	float ModifyAccuracyBasedOnPedType(const CPed& ped, float fAccuracy) const;

	//
	// Members
	//

	// Reset variables
	CPedAccuracyResetVariables m_resetVariables;

	// Recoil firing time
	float m_fNormalisedTimeFiring;
};

//////////////////////////////////////////////////////////////////////////
// sPedAccuracyModifiers
//////////////////////////////////////////////////////////////////////////

struct sPedAccuracyModifiers
{
    // Initialise
    static void Init();

#if __BANK
    static void InitWidgets();
#endif // __BANK
	
	// Load the data
	static void Load();

#if __BANK
	// Save the data
	static void Save();
#endif // __BANK


    // Static object
    static sPedAccuracyModifiers ms_Instance;

	// Player modifiers
	float PLAYER_RECOIL_MODIFIER_MIN;
	float PLAYER_RECOIL_MODIFIER_MAX;
	float PLAYER_RECOIL_CROUCHED_MODIFIER;
	float PLAYER_BLIND_FIRE_MODIFIER_MIN;
	float PLAYER_BLIND_FIRE_MODIFIER_MAX;
	float PLAYER_RECENTLY_DAMAGED_MODIFIER;

	// AI modifiers
	float AI_GLOBAL_MODIFIER;
	float AI_TARGET_IN_COMBAT_ROLL_MODIFIER;
    float AI_TARGET_COMING_OUT_OF_COVER_MODIFIER;
    float AI_TARGET_IN_COVER_MODIFIER;
    float AI_TARGET_WALKING_MODIFIER;
    float AI_TARGET_RUNNING_MODIFIER;
    float AI_TARGET_IN_AIR_MODIFIER;
	float AI_TARGET_DRIVING_AT_SPEED_MODIFIER;
	float AI_BLIND_FIRE_MODIFIER;
	float AI_DISTANCE_FOR_MAX_ACCURACY_BOOST;
    float AI_DISTANCE_FOR_MIN_ACCURACY_BOOST;
	float AI_HURT_ON_GROUND_MODIFIER;
	float AI_HURT_MODIFIER;
	float AI_PROFESSIONAL_PISTOL_VS_AI_MODIFIER;
    PAR_SIMPLE_PARSABLE
};

#endif // PED_ACCURACY_H
