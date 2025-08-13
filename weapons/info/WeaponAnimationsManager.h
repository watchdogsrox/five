#ifndef INC_WEAPON_ANIMATIONS_MANAGER_H
#define INC_WEAPON_ANIMATIONS_MANAGER_H

// Rage headers
#include "atl/binmap.h"
#include "atl/hashstring.h"
#include "parser/macros.h"

// Game headers
#include "Weapons/Info/WeaponInfo.h"

// Game forward declarations
class CPed;
class CWeaponComponentData;
class CWeaponSwapData;

////////////////////////////////////////////////////////////////////////////////
// CWeaponAnimations
////////////////////////////////////////////////////////////////////////////////

struct CWeaponAnimations
{

public:

	CWeaponAnimations()
	{}

	void AddWeaponAnimationSet(const char *filename);

private:

	void OnPostLoad();

public:

	atHashWithStringNotFinal CoverMovementClipSetHash;
	atHashWithStringNotFinal CoverMovementExtraClipSetHash;
	atHashWithStringNotFinal CoverAlternateMovementClipSetHash;
	atHashWithStringNotFinal CoverWeaponClipSetHash;
	atHashWithStringNotFinal MotionClipSetHash; 
	atHashWithStringNotFinal MotionFilterHash; 
	atHashWithStringNotFinal MotionCrouchClipSetHash; 
	atHashWithStringNotFinal MotionStrafingClipSetHash;
	atHashWithStringNotFinal MotionStrafingStealthClipSetHash;
	atHashWithStringNotFinal MotionStrafingUpperBodyClipSetHash;
	atHashWithStringNotFinal WeaponClipSetHash;
	atHashWithStringNotFinal WeaponClipSetStreamedHash;
	atHashWithStringNotFinal WeaponClipSetHashInjured;
	atHashWithStringNotFinal WeaponClipSetHashStealth;
	atHashWithStringNotFinal WeaponClipSetHashHiCover;
	atHashWithStringNotFinal AlternativeClipSetWhenBlocked;
	atHashWithStringNotFinal ScopeWeaponClipSet;
	atHashWithStringNotFinal AlternateAimingStandingClipSetHash;
	atHashWithStringNotFinal AlternateAimingCrouchingClipSetHash;
	atHashWithStringNotFinal FiringVariationsStandingClipSetHash;
	atHashWithStringNotFinal FiringVariationsCrouchingClipSetHash;
	atHashWithStringNotFinal AimTurnStandingClipSetHash;
	atHashWithStringNotFinal AimTurnCrouchingClipSetHash;
	atHashWithStringNotFinal MeleeBaseClipSetHash;
	atHashWithStringNotFinal MeleeClipSetHash;
	atHashWithStringNotFinal MeleeVariationClipSetHash;
	atHashWithStringNotFinal MeleeTauntClipSetHash;
	atHashWithStringNotFinal MeleeSupportTauntClipSetHash;
	atHashWithStringNotFinal MeleeStealthClipSetHash;
	atHashWithStringNotFinal ShellShockedClipSetHash;
	atHashWithStringNotFinal JumpUpperbodyClipSetHash; 
	atHashWithStringNotFinal FallUpperbodyClipSetHash; 
	atHashWithStringNotFinal FromStrafeTransitionUpperBodyClipSetHash;
	atHashWithStringNotFinal SwapWeaponFilterHash;
	atHashWithStringNotFinal SwapWeaponInLowCoverFilterHash;
	const CWeaponSwapData* WeaponSwapData;
	atHashWithStringNotFinal WeaponSwapClipSetHash;
	atHashWithStringNotFinal AimGrenadeThrowNormalClipsetHash;
	atHashWithStringNotFinal AimGrenadeThrowAlternateClipsetHash;
	atHashWithStringNotFinal GestureBeckonOverrideClipSetHash;
	atHashWithStringNotFinal GestureOverThereOverrideClipSetHash;
	atHashWithStringNotFinal GestureHaltOverrideClipSetHash;
	atHashWithStringNotFinal GestureGlancesOverrideClipSetHash;
	atHashWithStringNotFinal CombatReactionOverrideClipSetHash;
	atHashWithStringNotFinal FPSTransitionFromIdleHash;
	atHashWithStringNotFinal FPSTransitionFromRNGHash;
	atHashWithStringNotFinal FPSTransitionFromLTHash;
	atHashWithStringNotFinal FPSTransitionFromScopeHash;
	atHashWithStringNotFinal FPSTransitionFromUnholsterHash;
	atHashWithStringNotFinal FPSTransitionFromStealthHash;
	atHashWithStringNotFinal FPSTransitionToStealthHash;
	atHashWithStringNotFinal FPSTransitionToStealthFromUnholsterHash;
	atArray<atHashWithStringNotFinal> FPSFidgetClipsetHashes;
	atHashWithStringNotFinal MovementOverrideClipSetHash;
	atHashWithStringNotFinal WeaponClipSetHashForClone;
	atHashWithStringNotFinal MotionClipSetHashForClone;

	float AnimFireRateModifier;
	float AnimBlindFireRateModifier;
	float AnimWantingToShootFireRateModifier;
	bool UseFromStrafeUpperBodyAimNetwork;
	bool AimingDownTheBarrel;
	bool UseLeftHandIKAllowTags;
	bool BlockLeftHandIKWhileAiming;

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CWeaponAnimationsSet
////////////////////////////////////////////////////////////////////////////////

struct CWeaponAnimationsSet
{
	CWeaponAnimationsSet()
	{}

	atHashWithStringNotFinal m_Fallback;

	atHashWithStringNotFinal FPSStrafeClipSetHash;

	typedef atBinaryMap<CWeaponAnimations, atHashWithStringNotFinal> WeaponAnimationMap;
	WeaponAnimationMap m_WeaponAnimations;

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CWeaponAnimationsSets
////////////////////////////////////////////////////////////////////////////////

struct CWeaponAnimationsSets
{
	CWeaponAnimationsSets()
	{}

	typedef atBinaryMap<CWeaponAnimationsSet, atHashWithStringNotFinal> WeaponAnimationSetMap;
	WeaponAnimationSetMap m_WeaponAnimationsSets;

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CWeaponAnimationsManager
////////////////////////////////////////////////////////////////////////////////

class CWeaponAnimationsManager
{

public:

	static CWeaponAnimationsManager& GetInstance()
	{
		FastAssert(sm_Instance); return *sm_Instance;
	}

private:

	explicit CWeaponAnimationsManager();
	~CWeaponAnimationsManager();

public:
	void						AddWeaponAnimationSet(const char *filename);

	atHashWithStringNotFinal	GetAimTurnCrouchingClipSetHash(atHashWithStringNotFinal hWeapon) const;
	atHashWithStringNotFinal	GetAimTurnCrouchingClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;
	atHashWithStringNotFinal	GetAimTurnStandingClipSetHash(atHashWithStringNotFinal hWeapon) const;
	atHashWithStringNotFinal	GetAimTurnStandingClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;
	atHashWithStringNotFinal	GetAlternateAimingCrouchingClipSetHash(atHashWithStringNotFinal hWeapon) const;
	atHashWithStringNotFinal	GetAlternateAimingCrouchingClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;
	atHashWithStringNotFinal	GetAlternateAimingStandingClipSetHash(atHashWithStringNotFinal hWeapon) const;
	atHashWithStringNotFinal	GetAlternateAimingStandingClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;
	atHashWithStringNotFinal	GetCoverAlternateMovementClipSetHash(atHashWithStringNotFinal hWeapon) const;
	atHashWithStringNotFinal	GetCoverAlternateMovementClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;
	atHashWithStringNotFinal	GetCoverMovementClipSetHash(atHashWithStringNotFinal hWeapon) const;
	atHashWithStringNotFinal	GetCoverMovementClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;
	atHashWithStringNotFinal	GetCoverMovementExtraClipSetHash(atHashWithStringNotFinal hWeapon) const;
	atHashWithStringNotFinal	GetCoverMovementExtraClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;
	atHashWithStringNotFinal	GetCoverWeaponClipSetHash(atHashWithStringNotFinal hWeapon) const;
	atHashWithStringNotFinal	GetCoverWeaponClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;
	atHashWithStringNotFinal	GetFiringVariationsCrouchingClipSetHash(atHashWithStringNotFinal hWeapon) const;
	atHashWithStringNotFinal	GetFiringVariationsCrouchingClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;
	atHashWithStringNotFinal	GetFiringVariationsStandingClipSetHash(atHashWithStringNotFinal hWeapon) const;
	atHashWithStringNotFinal	GetFiringVariationsStandingClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;
	atHashWithStringNotFinal	GetMeleeBaseClipSetHash(atHashWithStringNotFinal hWeapon) const;
	atHashWithStringNotFinal	GetMeleeBaseClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;
	atHashWithStringNotFinal	GetMeleeClipSetHash(atHashWithStringNotFinal hWeapon) const;
	atHashWithStringNotFinal	GetMeleeClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;
	atHashWithStringNotFinal	GetMeleeSupportTauntClipSetHash(atHashWithStringNotFinal hWeapon) const;
	atHashWithStringNotFinal	GetMeleeSupportTauntClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;
	atHashWithStringNotFinal	GetMeleeStealthClipSetHash(atHashWithStringNotFinal hWeapon) const;
	atHashWithStringNotFinal	GetMeleeStealthClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;
	atHashWithStringNotFinal	GetMeleeTauntClipSetHash(atHashWithStringNotFinal hWeapon) const;
	atHashWithStringNotFinal	GetMeleeTauntClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;
	atHashWithStringNotFinal	GetMeleeVariationClipSetHash(atHashWithStringNotFinal hWeapon) const;
	atHashWithStringNotFinal	GetMeleeVariationClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;
	atHashWithStringNotFinal	GetJumpUpperbodyClipSetHash(atHashWithStringNotFinal hWeapon) const;
	atHashWithStringNotFinal	GetJumpUpperbodyClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;
	atHashWithStringNotFinal	GetFallUpperbodyClipSetHash(atHashWithStringNotFinal hWeapon) const;
	atHashWithStringNotFinal	GetFallUpperbodyClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;
	atHashWithStringNotFinal	GetFromStrafeTransitionUpperBodyClipSetHash(atHashWithStringNotFinal hWeapon) const;
	atHashWithStringNotFinal	GetFromStrafeTransitionUpperBodyClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;
	atHashWithStringNotFinal	GetMotionClipSetHash(atHashWithStringNotFinal hWeapon) const;
	atHashWithStringNotFinal	GetMotionClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;
	atHashWithStringNotFinal	GetMotionCrouchClipSetHash(atHashWithStringNotFinal hWeapon) const;
	atHashWithStringNotFinal	GetMotionCrouchClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;
	atHashWithStringNotFinal	GetMotionFilterHash(atHashWithStringNotFinal hWeapon) const;
	atHashWithStringNotFinal	GetMotionFilterHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;
	atHashWithStringNotFinal	GetMotionStrafingClipSetHash(atHashWithStringNotFinal hWeapon) const;
	atHashWithStringNotFinal	GetMotionStrafingClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;
	atHashWithStringNotFinal	GetMotionStrafingStealthClipSetHash(atHashWithStringNotFinal hWeapon) const;
	atHashWithStringNotFinal	GetMotionStrafingStealthClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;
	atHashWithStringNotFinal	GetMotionStrafingUpperBodyClipSetHash(atHashWithStringNotFinal hWeapon) const;
	atHashWithStringNotFinal	GetMotionStrafingUpperBodyClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;
	atHashWithStringNotFinal	GetShellShockedClipSetHash(atHashWithStringNotFinal hWeapon) const;
	atHashWithStringNotFinal	GetShellShockedClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;
	atHashWithStringNotFinal	GetWeaponClipSetHash(atHashWithStringNotFinal hWeapon) const;
	atHashWithStringNotFinal	GetWeaponClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;
	atHashWithStringNotFinal	GetWeaponClipSetHashInjured(atHashWithStringNotFinal hWeapon) const;
	atHashWithStringNotFinal	GetWeaponClipSetHashInjured(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;
	atHashWithStringNotFinal	GetWeaponClipSetHashStealth(atHashWithStringNotFinal hWeapon) const;
	atHashWithStringNotFinal	GetWeaponClipSetHashStealth(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;
	atHashWithStringNotFinal	GetWeaponClipSetHashHiCover(atHashWithStringNotFinal hWeapon) const;
	atHashWithStringNotFinal	GetWeaponClipSetHashHiCover(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;
	atHashWithStringNotFinal	GetAlternativeClipSetWhenBlocked(atHashWithStringNotFinal hWeapon) const;
	atHashWithStringNotFinal	GetAlternativeClipSetWhenBlocked(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;
	atHashWithStringNotFinal	GetScopeWeaponClipSet(atHashWithStringNotFinal hWeapon) const;
	atHashWithStringNotFinal	GetScopeWeaponClipSet(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;
	atHashWithStringNotFinal	GetWeaponClipSetStreamedHash(atHashWithStringNotFinal hWeapon) const;
	atHashWithStringNotFinal	GetWeaponClipSetStreamedHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;
	atHashWithStringNotFinal	GetSwapWeaponFilterHash(atHashWithStringNotFinal hWeapon) const;
	atHashWithStringNotFinal	GetSwapWeaponFilterHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;
	atHashWithStringNotFinal	GetSwapWeaponInLowCoverFilterHash(atHashWithStringNotFinal hWeapon) const;
	atHashWithStringNotFinal	GetSwapWeaponInLowCoverFilterHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;
	const CWeaponSwapData*		GetSwapWeaponData( atHashWithStringNotFinal hWeapon) const;
	const CWeaponSwapData*		GetSwapWeaponData(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;
	atHashWithStringNotFinal	GetSwapWeaponClipSetHash(atHashWithStringNotFinal hWeapon) const;
	atHashWithStringNotFinal	GetSwapWeaponClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;
	atHashWithStringNotFinal	GetAimGrenadeThrowClipsetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;
	atHashWithStringNotFinal	GetAimGrenadeThrowAlternateClipsetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;

	atHashWithStringNotFinal	GetGestureBeckonOverrideClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;
	atHashWithStringNotFinal	GetGestureOverThereOverrideClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;
	atHashWithStringNotFinal	GetGestureHaltOverrideClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;
	atHashWithStringNotFinal	GetGestureGlancesOverrideClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;
	atHashWithStringNotFinal	GetCombatReactionOverrideClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;

	atHashWithStringNotFinal	GetFPSTransitionFromIdleClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;
	atHashWithStringNotFinal	GetFPSTransitionFromRNGClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;
	atHashWithStringNotFinal	GetFPSTransitionFromLTClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;
	atHashWithStringNotFinal	GetFPSTransitionFromScopeClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;
	atHashWithStringNotFinal	GetFPSTransitionFromUnholsterClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;
	atHashWithStringNotFinal	GetFPSTransitionFromStealthClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;
	atHashWithStringNotFinal	GetFPSTransitionToStealthClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;
	atHashWithStringNotFinal	GetFPSTransitionToStealthFromUnholsterClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;


	atHashWithStringNotFinal	GetFPSFidgetClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags, int iIndex) const;
	int							GetNumFPSFidgets(const CPed &rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;

	atHashWithStringNotFinal	GetMovementOverrideClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;

	atHashWithStringNotFinal	GetWeaponClipSetHashForClone(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;
	atHashWithStringNotFinal	GetMotionClipSetHashForClone(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;

	atHashWithStringNotFinal	GetFPSStrafeClipSetHash(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault) const;

	float						GetAnimBlindFireRateModifier(atHashWithStringNotFinal hWeapon) const;
	float						GetAnimFireRateModifier(atHashWithStringNotFinal hWeapon) const;
	float						GetAnimWantingToShootFireRateModifier(atHashWithStringNotFinal hWeapon) const;
	bool						GetUseFromStrafeUpperBodyAimNetwork(atHashWithStringNotFinal hWeapon) const;

	bool						DoesWeaponAnimationSetForHashExist(atHashWithStringNotFinal hName) const;
	bool						GetAimingDownTheBarrel(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;
	bool						GetUseLeftHandIKAllowTags(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;
	bool						GetBlockLeftHandIKWhileAiming(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const;

	CWeaponAnimationsSets& GetAnimationWeaponSets() { return m_WeaponAnimationsSets; }
public:

	static void	Init();
	static void	Shutdown();

#if __BANK
	// Widgets
	static atString GetWidgetGroupName();
	static void AddWidgetButton(bkBank& bank);
	static void AddWidgets(bkBank& bank);
#endif // __BANK

	// Load the data
	static void Load();

#if __BANK
	// Save the data
	static void Save();
#endif // __BANK

private:

	const CWeaponAnimationsSet*	GetWeaponAnimationsSetForHash(atHashWithStringNotFinal hName) const;
	const CWeaponAnimationsSet* GetWeaponAnimationsSetForPed(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const;
	void						LoadAllMetaFiles();

private:

	CWeaponAnimationsSets m_WeaponAnimationsSets;


private:

	static CWeaponAnimationsManager* sm_Instance;

};

#endif // INC_WEAPON_ANIMATIONS_MANAGER_H
