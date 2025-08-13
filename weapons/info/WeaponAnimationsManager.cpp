// File header
#include "Weapons/Info/WeaponAnimationsManager.h"

// Rage headers
#include "fwanimation/clipsets.h"
#include "parser/manager.h"

// Game headers
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "ModelInfo/PedModelInfo.h"
#include "Task/Motion/Locomotion/TaskMotionAiming.h"
#include "Weapons/Components/WeaponComponentManager.h"

// Parser files
#include "Weapons/Info/WeaponAnimationsManager_parser.h"
#include "Weapons/WeaponDebug.h"

WEAPON_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////
// Hard-Coded Stuff
////////////////////////////////////////////////////////////////////////////////

namespace CWeaponAnimationsGlobals
{
	const atHashWithStringNotFinal DEFAULT_WEAPON_ANIMATIONS = atHashWithStringNotFinal("Default",0xE4DF46D5);
#if FPS_MODE_SUPPORTED
	const atHashWithStringNotFinal DEFAULT_WEAPON_FIRSTPERSON_ANIMATIONS = atHashWithStringNotFinal("FirstPerson",0xee38e8e0);
	const atHashWithStringNotFinal DEFAULT_WEAPON_FIRSTPERSON_RNG_ANIMATIONS = atHashWithStringNotFinal("FirstPersonRNG",0xa4fdd608);
	const atHashWithStringNotFinal DEFAULT_WEAPON_FIRSTPERSON_AIMING_ANIMATIONS = atHashWithStringNotFinal("FirstPersonAiming",0xc76297a3);
	const atHashWithStringNotFinal DEFAULT_WEAPON_FIRSTPERSON_SCOPE_ANIMATIONS = atHashWithStringNotFinal("FirstPersonScope",0x28117c22);
#endif // FPS_MODE_SUPPORTED
}

////////////////////////////////////////////////////////////////////////////////
// Nasty Macros
////////////////////////////////////////////////////////////////////////////////

#define ReturnFloatForWeapon(hWeapon, hMember) \
	const CWeaponAnimationsSet* pWeaponAnimationsSet = GetWeaponAnimationsSetForHash(CWeaponAnimationsGlobals::DEFAULT_WEAPON_ANIMATIONS); \
	if(pWeaponAnimationsSet) \
	{ \
		const CWeaponAnimations* pWeaponAnimations = pWeaponAnimationsSet->m_WeaponAnimations.SafeGet(hWeapon); \
		if(pWeaponAnimations) \
		{ \
			return pWeaponAnimations->hMember; \
		} \
	} \
	return 0.0f;

#define ReturnHashForPedAndWeapon(rPed, hWeapon, hMember, fpsStreamingFlags) \
	for(const CWeaponAnimationsSet* pWeaponAnimationsSet = GetWeaponAnimationsSetForPed(rPed, fpsStreamingFlags); pWeaponAnimationsSet; pWeaponAnimationsSet = GetWeaponAnimationsSetForHash(pWeaponAnimationsSet->m_Fallback)) \
	{ \
		const CWeaponAnimations* pWeaponAnimations = pWeaponAnimationsSet->m_WeaponAnimations.SafeGet(hWeapon); \
		if(pWeaponAnimations) \
		{ \
			atHashWithStringNotFinal hValue = pWeaponAnimations->hMember; \
			if(hValue.IsNotNull()) \
			{ \
				return hValue; \
			} \
		} \
	} \
	return atHashWithStringNotFinal::Null();

#define ReturnFidgetHashForPedAndWeaponAndIndex(rPed, hWeapon, fpsStreamingFlags, iIndex) \
	for(const CWeaponAnimationsSet* pWeaponAnimationsSet = GetWeaponAnimationsSetForPed(rPed, fpsStreamingFlags); pWeaponAnimationsSet; pWeaponAnimationsSet = GetWeaponAnimationsSetForHash(pWeaponAnimationsSet->m_Fallback)) \
	{ \
		if (pWeaponAnimationsSet) \
		{ \
			const CWeaponAnimations* pWeaponAnimations = pWeaponAnimationsSet->m_WeaponAnimations.SafeGet(hWeapon); \
			if(pWeaponAnimations) \
			{ \
				if(weaponVerifyf(iIndex >= 0 && iIndex < pWeaponAnimations->FPSFidgetClipsetHashes.GetCount(), "iIndex [%d] out of bounds [0..%d]", iIndex, pWeaponAnimations->FPSFidgetClipsetHashes.GetCount())) \
				{ \
					atHashWithStringNotFinal hValue = pWeaponAnimations->FPSFidgetClipsetHashes[iIndex]; \
					if(hValue.IsNotNull()) \
					{ \
						return hValue; \
					} \
				} \
			} \
		} \
	} \
	return atHashWithStringNotFinal::Null();

#define ReturnHashForWeapon(hWeapon, hMember) \
	const CWeaponAnimationsSet* pWeaponAnimationsSet = GetWeaponAnimationsSetForHash(CWeaponAnimationsGlobals::DEFAULT_WEAPON_ANIMATIONS); \
	if(pWeaponAnimationsSet) \
	{ \
		const CWeaponAnimations* pWeaponAnimations = pWeaponAnimationsSet->m_WeaponAnimations.SafeGet(hWeapon); \
		if(pWeaponAnimations) \
		{ \
			return pWeaponAnimations->hMember; \
		} \
	} \
	return atHashWithStringNotFinal::Null();

#define ReturnBoolForWeapon(hWeapon, hMember) \
	const CWeaponAnimationsSet* pWeaponAnimationsSet = GetWeaponAnimationsSetForHash(CWeaponAnimationsGlobals::DEFAULT_WEAPON_ANIMATIONS); \
	if(pWeaponAnimationsSet) \
	{ \
		const CWeaponAnimations* pWeaponAnimations = pWeaponAnimationsSet->m_WeaponAnimations.SafeGet(hWeapon); \
		if(pWeaponAnimations) \
		{ \
			return pWeaponAnimations->hMember; \
		} \
	} \
	return false;

////////////////////////////////////////////////////////////////////////////////
// Data file loader interface
////////////////////////////////////////////////////////////////////////////////

class CWeaponAnimationsDataFileMounter : public CDataFileMountInterface
{
	virtual bool LoadDataFile(const CDataFileMgr::DataFile & file)
	{
		CWeaponAnimationsManager::GetInstance().AddWeaponAnimationSet(file.m_filename);
		return true;
	}

	virtual void UnloadDataFile(const CDataFileMgr::DataFile & ) {}

} g_WeaponAnimationsDataFileMounter;


////////////////////////////////////////////////////////////////////////////////
// CWeaponAnimations
////////////////////////////////////////////////////////////////////////////////

void CWeaponAnimations::OnPostLoad()
{
#if __ASSERT
	if(CoverMovementClipSetHash.GetHash() != 0)
	{
		fwMvClipSetId CoverMovementClipSet = fwMvClipSetId(CoverMovementClipSetHash.GetHash());
		Assertf(fwClipSetManager::GetClipSet(CoverMovementClipSet), "Could not find clip set %s!", CoverMovementClipSetHash.GetCStr());
	}

	if(CoverWeaponClipSetHash.GetHash() != 0)
	{
		fwMvClipSetId CoverWeaponClipSet = fwMvClipSetId(CoverWeaponClipSetHash.GetHash());
		Assertf(fwClipSetManager::GetClipSet(CoverWeaponClipSet), "Could not find clip set %s!", CoverWeaponClipSetHash.GetCStr());
	}

	if(MotionClipSetHash.GetHash() != 0)
	{
		fwMvClipSetId MotionClipSet = fwMvClipSetId(MotionClipSetHash.GetHash());
		Assertf(fwClipSetManager::GetClipSet(MotionClipSet), "Could not find clip set %s!", MotionClipSetHash.GetCStr());
	}

	if(MotionCrouchClipSetHash.GetHash() != 0)
	{
		fwMvClipSetId MotionClipSet = fwMvClipSetId(MotionCrouchClipSetHash.GetHash());
		Assertf(fwClipSetManager::GetClipSet(MotionClipSet), "Could not find clip set %s!", MotionCrouchClipSetHash.GetCStr());
	}

	if(WeaponClipSetHash.GetHash() != 0)
	{
		fwMvClipSetId WeaponClipSet = fwMvClipSetId(WeaponClipSetHash.GetHash());
		Assertf(fwClipSetManager::GetClipSet(WeaponClipSet), "Could not find clip set %s!", WeaponClipSetHash.GetCStr());
	}

	if(WeaponClipSetHashInjured.GetHash() != 0)
	{
		fwMvClipSetId WeaponClipSet = fwMvClipSetId(WeaponClipSetHashInjured.GetHash());
		Assertf(fwClipSetManager::GetClipSet(WeaponClipSet), "Could not find clip set %s!", WeaponClipSetHashInjured.GetCStr());
	}

	if(WeaponClipSetHashStealth.GetHash() != 0)
	{
		fwMvClipSetId WeaponClipSet = fwMvClipSetId(WeaponClipSetHashStealth.GetHash());
		Assertf(fwClipSetManager::GetClipSet(WeaponClipSet), "Could not find clip set %s!", WeaponClipSetHashStealth.GetCStr());
	}

	if(WeaponClipSetHashHiCover.GetHash() != 0)
	{
		fwMvClipSetId WeaponClipSet = fwMvClipSetId(WeaponClipSetHashHiCover.GetHash());
		Assertf(fwClipSetManager::GetClipSet(WeaponClipSet), "Could not find clip set %s!", WeaponClipSetHashHiCover.GetCStr());
	}

	if(AlternativeClipSetWhenBlocked.GetHash() != 0)
	{
		fwMvClipSetId WeaponClipSet = fwMvClipSetId(AlternativeClipSetWhenBlocked.GetHash());
		Assertf(fwClipSetManager::GetClipSet(WeaponClipSet), "Could not find clip set %s!", AlternativeClipSetWhenBlocked.GetCStr());
	}

	if(ScopeWeaponClipSet.GetHash() != 0)
	{
		fwMvClipSetId WeaponClipSet = fwMvClipSetId(ScopeWeaponClipSet.GetHash());
		Assertf(fwClipSetManager::GetClipSet(WeaponClipSet), "Could not find clip set %s!", ScopeWeaponClipSet.GetCStr());
	}

	if(AlternateAimingStandingClipSetHash.GetHash() != 0)
	{
		fwMvClipSetId AlternateAimingStandingClipSet = fwMvClipSetId(AlternateAimingStandingClipSetHash.GetHash());
		Assertf(fwClipSetManager::GetClipSet(AlternateAimingStandingClipSet), "Could not find clip set %s!", AlternateAimingStandingClipSetHash.GetCStr());
	}

	if(AlternateAimingCrouchingClipSetHash.GetHash() != 0)
	{
		fwMvClipSetId AlternateAimingCrouchingClipSet = fwMvClipSetId(AlternateAimingCrouchingClipSetHash.GetHash());
		Assertf(fwClipSetManager::GetClipSet(AlternateAimingCrouchingClipSet), "Could not find clip set %s!", AlternateAimingCrouchingClipSetHash.GetCStr());
	}

	if(MeleeBaseClipSetHash.GetHash() != 0)
	{
		fwMvClipSetId MeleeBaseClipSet = fwMvClipSetId(MeleeBaseClipSetHash.GetHash());
		Assertf(fwClipSetManager::GetClipSet(MeleeBaseClipSet), "Could not find clip set %s!", MeleeBaseClipSet.GetCStr());
	}

	if(MeleeClipSetHash.GetHash() != 0)
	{
		fwMvClipSetId MeleeClipSet = fwMvClipSetId(MeleeClipSetHash.GetHash());
		Assertf(fwClipSetManager::GetClipSet(MeleeClipSet), "Could not find clip set %s!", MeleeClipSetHash.GetCStr());
	}

	if(MeleeVariationClipSetHash.GetHash() != 0)
	{
		fwMvClipSetId MeleeVariationClipSet = fwMvClipSetId(MeleeVariationClipSetHash.GetHash());
		Assertf(fwClipSetManager::GetClipSet(MeleeVariationClipSet), "Could not find clip set %s!", MeleeVariationClipSetHash.GetCStr());
	}

	if(MeleeTauntClipSetHash.GetHash() != 0)
	{
		fwMvClipSetId MeleeTauntClipSet = fwMvClipSetId(MeleeTauntClipSetHash.GetHash());
		Assertf(fwClipSetManager::GetClipSet(MeleeTauntClipSet), "Could not find clip set %s!", MeleeTauntClipSetHash.GetCStr());
	}

	if(MeleeSupportTauntClipSetHash.GetHash() != 0)
	{
		fwMvClipSetId MeleeSupportTauntClipSet = fwMvClipSetId(MeleeSupportTauntClipSetHash.GetHash());
		Assertf(fwClipSetManager::GetClipSet(MeleeSupportTauntClipSet), "Could not find clip set %s!", MeleeSupportTauntClipSetHash.GetCStr());
	}

	if(MeleeStealthClipSetHash.GetHash() != 0)
	{
		fwMvClipSetId MeleeStealthClipSet = fwMvClipSetId(MeleeStealthClipSetHash.GetHash());
		Assertf(fwClipSetManager::GetClipSet(MeleeStealthClipSet), "Could not find clip set %s!", MeleeStealthClipSetHash.GetCStr());
	}

	if(ShellShockedClipSetHash.GetHash() != 0)
	{
		fwMvClipSetId ShellShockedClipSet = fwMvClipSetId(ShellShockedClipSetHash.GetHash());
		Assertf(fwClipSetManager::GetClipSet(ShellShockedClipSet), "Could not find clip set %s!", ShellShockedClipSetHash.GetCStr());
	}

	if(JumpUpperbodyClipSetHash.GetHash() != 0)
	{
		fwMvClipSetId JumpUpperbodyClipSet = fwMvClipSetId(JumpUpperbodyClipSetHash.GetHash());
		Assertf(fwClipSetManager::GetClipSet(JumpUpperbodyClipSet), "Could not find clip set %s!", JumpUpperbodyClipSetHash.GetCStr());
	}

	if(FallUpperbodyClipSetHash.GetHash() != 0)
	{
		fwMvClipSetId FallUpperbodyClipSet = fwMvClipSetId(FallUpperbodyClipSetHash.GetHash());
		Assertf(fwClipSetManager::GetClipSet(FallUpperbodyClipSet), "Could not find clip set %s!", FallUpperbodyClipSetHash.GetCStr());
	}

	if(FromStrafeTransitionUpperBodyClipSetHash.GetHash() != 0)
	{
		fwMvClipSetId FromStrafeTransitionUpperBodyClipSet = fwMvClipSetId(FromStrafeTransitionUpperBodyClipSetHash.GetHash());
		Assertf(fwClipSetManager::GetClipSet(FromStrafeTransitionUpperBodyClipSet), "Could not find clip set %s!", FromStrafeTransitionUpperBodyClipSetHash.GetCStr());
	}

	if(AimGrenadeThrowNormalClipsetHash.GetHash() != 0)
	{
		fwMvClipSetId AimGrenadeThrowClipset = fwMvClipSetId(AimGrenadeThrowNormalClipsetHash.GetHash());
		Assertf(fwClipSetManager::GetClipSet(AimGrenadeThrowClipset), "Could not find clip set %s!", AimGrenadeThrowNormalClipsetHash.GetCStr());
	}

	if(AimGrenadeThrowAlternateClipsetHash.GetHash() != 0)
	{
		fwMvClipSetId AimGrenadeThrowAlternateClipset = fwMvClipSetId(AimGrenadeThrowAlternateClipsetHash.GetHash());
		Assertf(fwClipSetManager::GetClipSet(AimGrenadeThrowAlternateClipset), "Could not find clip set %s!", AimGrenadeThrowAlternateClipsetHash.GetCStr());
	}

	if(GestureBeckonOverrideClipSetHash.GetHash() != 0)
	{
		fwMvClipSetId GestureBeckonOverrideClipSet = fwMvClipSetId(GestureBeckonOverrideClipSetHash.GetHash());
		Assertf(fwClipSetManager::GetClipSet(GestureBeckonOverrideClipSet), "Could not find clip set %s!", GestureBeckonOverrideClipSetHash.GetCStr());
	}

	if(GestureOverThereOverrideClipSetHash.GetHash() != 0)
	{
		fwMvClipSetId GestureOverThereOverrideClipSet = fwMvClipSetId(GestureOverThereOverrideClipSetHash.GetHash());
		Assertf(fwClipSetManager::GetClipSet(GestureOverThereOverrideClipSet), "Could not find clip set %s!", GestureOverThereOverrideClipSetHash.GetCStr());
	}

	if(GestureHaltOverrideClipSetHash.GetHash() != 0)
	{
		fwMvClipSetId GestureHaltOverrideClipSet = fwMvClipSetId(GestureHaltOverrideClipSetHash.GetHash());
		Assertf(fwClipSetManager::GetClipSet(GestureHaltOverrideClipSet), "Could not find clip set %s!", GestureHaltOverrideClipSetHash.GetCStr());
	}

	if(GestureGlancesOverrideClipSetHash.GetHash() != 0)
	{
		fwMvClipSetId GestureGlancesOverrideClipSet = fwMvClipSetId(GestureGlancesOverrideClipSetHash.GetHash());
		Assertf(fwClipSetManager::GetClipSet(GestureGlancesOverrideClipSet), "Could not find clip set %s!", GestureGlancesOverrideClipSetHash.GetCStr());
	}

	if(CombatReactionOverrideClipSetHash.GetHash() != 0)
	{
		fwMvClipSetId CombatReactionOverrideClipSet = fwMvClipSetId(CombatReactionOverrideClipSetHash.GetHash());
		Assertf(fwClipSetManager::GetClipSet(CombatReactionOverrideClipSet), "Could not find clip set %s!", CombatReactionOverrideClipSetHash.GetCStr());
	}

	if(FPSTransitionFromIdleHash.GetHash() != 0)
	{
		fwMvClipSetId FPSTransitionFromIdleClipSet = fwMvClipSetId(FPSTransitionFromIdleHash.GetHash());
		Assertf(fwClipSetManager::GetClipSet(FPSTransitionFromIdleClipSet), "Could not find clip set %s!", FPSTransitionFromIdleHash.GetCStr());
	}

	if(FPSTransitionFromRNGHash.GetHash() != 0)
	{
		fwMvClipSetId FPSTransitionFromRNGClipSet = fwMvClipSetId(FPSTransitionFromRNGHash.GetHash());
		Assertf(fwClipSetManager::GetClipSet(FPSTransitionFromRNGClipSet), "Could not find clip set %s!", FPSTransitionFromRNGHash.GetCStr());
	}

	if(FPSTransitionFromLTHash.GetHash() != 0)
	{
		fwMvClipSetId FPSTransitionFromLTClipSet = fwMvClipSetId(FPSTransitionFromLTHash.GetHash());
		Assertf(fwClipSetManager::GetClipSet(FPSTransitionFromLTClipSet), "Could not find clip set %s!", FPSTransitionFromLTHash.GetCStr());
	}

	if(FPSTransitionFromScopeHash.GetHash() != 0)
	{
		fwMvClipSetId FPSTransitionFromScopeClipSet = fwMvClipSetId(FPSTransitionFromScopeHash.GetHash());
		Assertf(fwClipSetManager::GetClipSet(FPSTransitionFromScopeClipSet), "Could not find clip set %s!", FPSTransitionFromScopeHash.GetCStr());
	}

	if(FPSTransitionFromUnholsterHash.GetHash() != 0)
	{
		fwMvClipSetId FPSTransitionFromUnholsterClipSet = fwMvClipSetId(FPSTransitionFromUnholsterHash.GetHash());
		Assertf(fwClipSetManager::GetClipSet(FPSTransitionFromUnholsterClipSet), "Could not find clip set %s!", FPSTransitionFromUnholsterHash.GetCStr());
	}

	if(FPSTransitionFromStealthHash.GetHash() != 0)
	{
		fwMvClipSetId FPSTransitionFromStealthClipSet = fwMvClipSetId(FPSTransitionFromStealthHash.GetHash());
		Assertf(fwClipSetManager::GetClipSet(FPSTransitionFromStealthClipSet), "Could not find clip set %s!", FPSTransitionFromStealthHash.GetCStr());
	}

	if(FPSTransitionToStealthHash.GetHash() != 0)
	{
		fwMvClipSetId FPSTransitionToStealthClipSet = fwMvClipSetId(FPSTransitionToStealthHash.GetHash());
		Assertf(fwClipSetManager::GetClipSet(FPSTransitionToStealthClipSet), "Could not find clip set %s!", FPSTransitionToStealthHash.GetCStr());
	}

	if(FPSTransitionToStealthFromUnholsterHash.GetHash() != 0)
	{
		fwMvClipSetId FPSTransitionToStealthFromUnholsterClipSet = fwMvClipSetId(FPSTransitionToStealthFromUnholsterHash.GetHash());
		Assertf(fwClipSetManager::GetClipSet(FPSTransitionToStealthFromUnholsterClipSet), "Could not find clip set %s!", FPSTransitionToStealthFromUnholsterHash.GetCStr());
	}

	if (FPSFidgetClipsetHashes.GetCount() > 0)
	{
		for (int i = 0; i < FPSFidgetClipsetHashes.GetCount(); i++)
		{
			fwMvClipSetId FPSFidgetClipSet = fwMvClipSetId(FPSFidgetClipsetHashes[i].GetHash());
			Assertf(fwClipSetManager::GetClipSet(FPSFidgetClipSet), "Could not find clip set %s!", FPSFidgetClipsetHashes[i].GetCStr());
		}
	}

	if(MovementOverrideClipSetHash.GetHash() != 0)
	{
		fwMvClipSetId MovementOverrideClipSet = fwMvClipSetId(MovementOverrideClipSetHash.GetHash());
		Assertf(fwClipSetManager::GetClipSet(MovementOverrideClipSet), "Could not find clip set %s!", MovementOverrideClipSetHash.GetCStr());
	}

	if(WeaponSwapClipSetHash.GetHash() != 0)
	{
		fwMvClipSetId WeaponSwapClipSet = fwMvClipSetId(WeaponSwapClipSetHash.GetHash());
		Assertf(fwClipSetManager::GetClipSet(WeaponSwapClipSet), "Could not find clip set %s!", WeaponSwapClipSetHash.GetCStr());
	}

#endif // __ASSERT
}

////////////////////////////////////////////////////////////////////////////////
// CWeaponAnimationsManager
////////////////////////////////////////////////////////////////////////////////

CWeaponAnimationsManager *CWeaponAnimationsManager::sm_Instance = NULL;

CWeaponAnimationsManager::CWeaponAnimationsManager()
{
	CDataFileMount::RegisterMountInterface(CDataFileMgr::WEAPON_ANIMATIONS_FILE, &g_WeaponAnimationsDataFileMounter);
}

void CWeaponAnimationsManager::LoadAllMetaFiles()
{
	m_WeaponAnimationsSets.m_WeaponAnimationsSets.Reset();

	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::WEAPON_ANIMATIONS_FILE);
	while(DATAFILEMGR.IsValid(pData))
	{
		if (!pData->m_disabled)
		{
			AddWeaponAnimationSet(pData->m_filename);
		}

		// Get next file
		pData = DATAFILEMGR.GetNextFile(pData);
	}

#if __BANK
	// Init the widgets
	CWeaponDebug::InitBank();
#endif // __BANK
}

void CWeaponAnimationsManager::AddWeaponAnimationSet(const char *filename)
{
	DIAG_CONTEXT_MESSAGE("WeaponAnimationSet(%s)", filename);

	//Parse the weapon animations - let's use a temporary object first.
	CWeaponAnimationsSets tempSet;
	PARSER.LoadObject(filename, "", tempSet);

	// Now merge it into the master set.
	CWeaponAnimationsSets::WeaponAnimationSetMap::Iterator setIt = tempSet.m_WeaponAnimationsSets.Begin();
	CWeaponAnimationsSets::WeaponAnimationSetMap::Iterator setEnd = tempSet.m_WeaponAnimationsSets.End();

	while (setIt != setEnd)
	{
		atHashWithStringNotFinal key = setIt.GetKey();

		CWeaponAnimationsSet *set = m_WeaponAnimationsSets.m_WeaponAnimationsSets.SafeGet(key);

		if (!set)
		{
			set = m_WeaponAnimationsSets.m_WeaponAnimationsSets.FastInsert(key);

			// TODO: Would be nice if there was a SafeGet that works on unsorted binmaps so we
			// don't have to resort ALL the time.
			m_WeaponAnimationsSets.m_WeaponAnimationsSets.FinishInsertion();
		}

		CWeaponAnimationsSet &srcSet = *setIt;

		// Now merge all the individual entries.
		CWeaponAnimationsSet::WeaponAnimationMap::Iterator animIt = srcSet.m_WeaponAnimations.Begin();
		CWeaponAnimationsSet::WeaponAnimationMap::Iterator animEnd = srcSet.m_WeaponAnimations.End();

		while (animIt != animEnd)
		{
			atHashWithStringNotFinal animKey = animIt.GetKey();

			Assertf(set->m_WeaponAnimations.SafeGet(animKey) == NULL, "A secondary weapon anim file (presumably DLC) is overriding the already existing animation %s", animKey.GetCStr());

			set->m_WeaponAnimations.Insert(animKey, *animIt);

			// TODO: Would be nice if there was a SafeGet that works on unsorted binmaps so we
			// don't have to resort ALL the time.
			set->m_WeaponAnimations.FinishInsertion();

			++animIt;
		}

		set->FPSStrafeClipSetHash = srcSet.FPSStrafeClipSetHash;

		// Finally, set the fallback. TODO: What do we do about DLC? Does it just overwrite it?
		set->m_Fallback = srcSet.m_Fallback;

		// Re-sort the list.
//		set->m_WeaponAnimations.FinishInsertion();

		++setIt;
	}

	// Re-sort the master list.
//	m_WeaponAnimationsSets.m_WeaponAnimationsSets.FinishInsertion();
}

CWeaponAnimationsManager::~CWeaponAnimationsManager()
{
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetAimTurnCrouchingClipSetHash(atHashWithStringNotFinal hWeapon) const
{
	ReturnHashForWeapon(hWeapon, AimTurnCrouchingClipSetHash)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetAimTurnCrouchingClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, AimTurnCrouchingClipSetHash, fpsStreamingFlags)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetAimTurnStandingClipSetHash(atHashWithStringNotFinal hWeapon) const
{
	ReturnHashForWeapon(hWeapon, AimTurnStandingClipSetHash)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetAimTurnStandingClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, AimTurnStandingClipSetHash, fpsStreamingFlags)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetAlternateAimingCrouchingClipSetHash(atHashWithStringNotFinal hWeapon) const
{
	ReturnHashForWeapon(hWeapon, AlternateAimingCrouchingClipSetHash)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetAlternateAimingCrouchingClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, AlternateAimingCrouchingClipSetHash, fpsStreamingFlags)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetAlternateAimingStandingClipSetHash(atHashWithStringNotFinal hWeapon) const
{
	ReturnHashForWeapon(hWeapon, AlternateAimingStandingClipSetHash)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetAlternateAimingStandingClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, AlternateAimingStandingClipSetHash, fpsStreamingFlags)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetCoverAlternateMovementClipSetHash(atHashWithStringNotFinal hWeapon) const
{
	ReturnHashForWeapon(hWeapon, CoverAlternateMovementClipSetHash)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetCoverAlternateMovementClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, CoverAlternateMovementClipSetHash, fpsStreamingFlags)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetCoverMovementClipSetHash(atHashWithStringNotFinal hWeapon) const
{
	ReturnHashForWeapon(hWeapon, CoverMovementClipSetHash)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetCoverMovementClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, CoverMovementClipSetHash, fpsStreamingFlags)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetCoverMovementExtraClipSetHash(atHashWithStringNotFinal hWeapon) const
{
	ReturnHashForWeapon(hWeapon, CoverMovementExtraClipSetHash)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetCoverMovementExtraClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, CoverMovementExtraClipSetHash, fpsStreamingFlags)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetCoverWeaponClipSetHash(atHashWithStringNotFinal hWeapon) const
{
	ReturnHashForWeapon(hWeapon, CoverWeaponClipSetHash)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetCoverWeaponClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, CoverWeaponClipSetHash, fpsStreamingFlags)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetFiringVariationsCrouchingClipSetHash(atHashWithStringNotFinal hWeapon) const
{
	ReturnHashForWeapon(hWeapon, FiringVariationsCrouchingClipSetHash)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetFiringVariationsCrouchingClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, FiringVariationsCrouchingClipSetHash, fpsStreamingFlags)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetFiringVariationsStandingClipSetHash(atHashWithStringNotFinal hWeapon) const
{
	ReturnHashForWeapon(hWeapon, FiringVariationsStandingClipSetHash)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetFiringVariationsStandingClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, FiringVariationsStandingClipSetHash, fpsStreamingFlags)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetMeleeBaseClipSetHash(atHashWithStringNotFinal hWeapon) const
{
	ReturnHashForWeapon(hWeapon, MeleeBaseClipSetHash)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetMeleeBaseClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, MeleeBaseClipSetHash, fpsStreamingFlags)
}


atHashWithStringNotFinal CWeaponAnimationsManager::GetMeleeClipSetHash(atHashWithStringNotFinal hWeapon) const
{
	ReturnHashForWeapon(hWeapon, MeleeClipSetHash)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetMeleeClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, MeleeClipSetHash, fpsStreamingFlags)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetMeleeSupportTauntClipSetHash(atHashWithStringNotFinal hWeapon) const
{
	ReturnHashForWeapon(hWeapon, MeleeSupportTauntClipSetHash)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetMeleeSupportTauntClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, MeleeSupportTauntClipSetHash, fpsStreamingFlags)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetMeleeStealthClipSetHash(atHashWithStringNotFinal hWeapon) const
{
	ReturnHashForWeapon(hWeapon, MeleeStealthClipSetHash)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetMeleeStealthClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, MeleeStealthClipSetHash, fpsStreamingFlags)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetMeleeTauntClipSetHash(atHashWithStringNotFinal hWeapon) const
{
	ReturnHashForWeapon(hWeapon, MeleeTauntClipSetHash)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetMeleeTauntClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, MeleeTauntClipSetHash, fpsStreamingFlags)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetMeleeVariationClipSetHash(atHashWithStringNotFinal hWeapon) const
{
	ReturnHashForWeapon(hWeapon, MeleeVariationClipSetHash)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetMeleeVariationClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, MeleeVariationClipSetHash, fpsStreamingFlags)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetJumpUpperbodyClipSetHash(atHashWithStringNotFinal hWeapon) const
{
	ReturnHashForWeapon(hWeapon, JumpUpperbodyClipSetHash)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetJumpUpperbodyClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, JumpUpperbodyClipSetHash, fpsStreamingFlags)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetFallUpperbodyClipSetHash(atHashWithStringNotFinal hWeapon) const
{
	ReturnHashForWeapon(hWeapon, FallUpperbodyClipSetHash)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetFallUpperbodyClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, FallUpperbodyClipSetHash, fpsStreamingFlags)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetFromStrafeTransitionUpperBodyClipSetHash(atHashWithStringNotFinal hWeapon) const
{
	ReturnHashForWeapon(hWeapon, FromStrafeTransitionUpperBodyClipSetHash)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetFromStrafeTransitionUpperBodyClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, FromStrafeTransitionUpperBodyClipSetHash, fpsStreamingFlags)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetMotionClipSetHash(atHashWithStringNotFinal hWeapon) const
{
	ReturnHashForWeapon(hWeapon, MotionClipSetHash)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetMotionClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, MotionClipSetHash, fpsStreamingFlags)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetMotionCrouchClipSetHash(atHashWithStringNotFinal hWeapon) const
{
	ReturnHashForWeapon(hWeapon, MotionCrouchClipSetHash)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetMotionCrouchClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, MotionCrouchClipSetHash, fpsStreamingFlags)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetMotionFilterHash(atHashWithStringNotFinal hWeapon) const
{
	ReturnHashForWeapon(hWeapon, MotionFilterHash)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetMotionFilterHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, MotionFilterHash, fpsStreamingFlags)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetMotionStrafingClipSetHash(atHashWithStringNotFinal hWeapon) const
{
	ReturnHashForWeapon(hWeapon, MotionStrafingClipSetHash)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetMotionStrafingClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, MotionStrafingClipSetHash, fpsStreamingFlags)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetMotionStrafingStealthClipSetHash(atHashWithStringNotFinal hWeapon) const
{
	ReturnHashForWeapon(hWeapon, MotionStrafingStealthClipSetHash)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetMotionStrafingStealthClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, MotionStrafingStealthClipSetHash, fpsStreamingFlags)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetMotionStrafingUpperBodyClipSetHash(atHashWithStringNotFinal hWeapon) const
{
	ReturnHashForWeapon(hWeapon, MotionStrafingUpperBodyClipSetHash)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetMotionStrafingUpperBodyClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, MotionStrafingUpperBodyClipSetHash, fpsStreamingFlags)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetShellShockedClipSetHash(atHashWithStringNotFinal hWeapon) const
{
	ReturnHashForWeapon(hWeapon, ShellShockedClipSetHash)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetShellShockedClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, ShellShockedClipSetHash, fpsStreamingFlags)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetWeaponClipSetHash(atHashWithStringNotFinal hWeapon) const
{
	ReturnHashForWeapon(hWeapon, WeaponClipSetHash)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetWeaponClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, WeaponClipSetHash, fpsStreamingFlags)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetWeaponClipSetHashInjured(atHashWithStringNotFinal hWeapon) const
{
	ReturnHashForWeapon(hWeapon, WeaponClipSetHashInjured)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetWeaponClipSetHashInjured(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, WeaponClipSetHashInjured, fpsStreamingFlags)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetWeaponClipSetHashStealth(atHashWithStringNotFinal hWeapon) const
{
	ReturnHashForWeapon(hWeapon, WeaponClipSetHashStealth)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetWeaponClipSetHashStealth(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, WeaponClipSetHashStealth, fpsStreamingFlags)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetWeaponClipSetHashHiCover(atHashWithStringNotFinal hWeapon) const
{
	ReturnHashForWeapon(hWeapon, WeaponClipSetHashHiCover)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetWeaponClipSetHashHiCover(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, WeaponClipSetHashHiCover, fpsStreamingFlags)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetAlternativeClipSetWhenBlocked(atHashWithStringNotFinal hWeapon) const
{
	ReturnHashForWeapon(hWeapon, AlternativeClipSetWhenBlocked)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetAlternativeClipSetWhenBlocked(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, AlternativeClipSetWhenBlocked, fpsStreamingFlags)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetScopeWeaponClipSet(atHashWithStringNotFinal hWeapon) const
{
	ReturnHashForWeapon(hWeapon, ScopeWeaponClipSet)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetScopeWeaponClipSet(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, ScopeWeaponClipSet, fpsStreamingFlags)
}	

atHashWithStringNotFinal CWeaponAnimationsManager::GetWeaponClipSetStreamedHash(atHashWithStringNotFinal hWeapon) const
{
	ReturnHashForWeapon(hWeapon, WeaponClipSetStreamedHash)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetWeaponClipSetStreamedHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, WeaponClipSetStreamedHash, fpsStreamingFlags)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetSwapWeaponFilterHash(atHashWithStringNotFinal hWeapon) const
{
	ReturnHashForWeapon(hWeapon, SwapWeaponFilterHash)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetSwapWeaponFilterHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, SwapWeaponFilterHash, fpsStreamingFlags)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetSwapWeaponInLowCoverFilterHash(atHashWithStringNotFinal hWeapon) const
{
	ReturnHashForWeapon(hWeapon, SwapWeaponInLowCoverFilterHash)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetSwapWeaponInLowCoverFilterHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, SwapWeaponInLowCoverFilterHash, fpsStreamingFlags)
}

const CWeaponSwapData* CWeaponAnimationsManager::GetSwapWeaponData(atHashWithStringNotFinal hWeapon) const
{
	const CWeaponAnimationsSet* pWeaponAnimationsSet = GetWeaponAnimationsSetForHash(CWeaponAnimationsGlobals::DEFAULT_WEAPON_ANIMATIONS);
	if (pWeaponAnimationsSet)
	{ 
		const CWeaponAnimations* pWeaponAnimations = pWeaponAnimationsSet->m_WeaponAnimations.SafeGet(hWeapon);
		if(pWeaponAnimations)
		{
			return static_cast<const CWeaponSwapData*>(pWeaponAnimations->WeaponSwapData);
		}
	}
	return NULL;
}

const CWeaponSwapData* CWeaponAnimationsManager::GetSwapWeaponData(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	for(const CWeaponAnimationsSet* pWeaponAnimationsSet = GetWeaponAnimationsSetForPed(rPed, fpsStreamingFlags); pWeaponAnimationsSet; pWeaponAnimationsSet = GetWeaponAnimationsSetForHash(pWeaponAnimationsSet->m_Fallback))
	{ 
		const CWeaponAnimations* pWeaponAnimations = pWeaponAnimationsSet->m_WeaponAnimations.SafeGet(hWeapon);
		if(pWeaponAnimations)
		{
			return static_cast<const CWeaponSwapData*>(pWeaponAnimations->WeaponSwapData);
		}
	}
	return NULL;
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetSwapWeaponClipSetHash(atHashWithStringNotFinal hWeapon) const
{
	ReturnHashForWeapon(hWeapon, WeaponSwapClipSetHash)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetSwapWeaponClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, WeaponSwapClipSetHash, fpsStreamingFlags)
}


atHashWithStringNotFinal CWeaponAnimationsManager::GetAimGrenadeThrowClipsetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, AimGrenadeThrowNormalClipsetHash, fpsStreamingFlags)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetAimGrenadeThrowAlternateClipsetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, AimGrenadeThrowAlternateClipsetHash, fpsStreamingFlags)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetGestureBeckonOverrideClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, GestureBeckonOverrideClipSetHash, fpsStreamingFlags)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetGestureOverThereOverrideClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, GestureOverThereOverrideClipSetHash, fpsStreamingFlags)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetGestureHaltOverrideClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, GestureHaltOverrideClipSetHash, fpsStreamingFlags)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetGestureGlancesOverrideClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, GestureGlancesOverrideClipSetHash, fpsStreamingFlags)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetCombatReactionOverrideClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, CombatReactionOverrideClipSetHash, fpsStreamingFlags)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetFPSTransitionFromIdleClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, FPSTransitionFromIdleHash, fpsStreamingFlags)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetFPSTransitionFromRNGClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, FPSTransitionFromRNGHash, fpsStreamingFlags)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetFPSTransitionFromLTClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, FPSTransitionFromLTHash, fpsStreamingFlags)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetFPSTransitionFromScopeClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, FPSTransitionFromScopeHash, fpsStreamingFlags)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetFPSTransitionFromUnholsterClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, FPSTransitionFromUnholsterHash, fpsStreamingFlags);
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetFPSTransitionFromStealthClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, FPSTransitionFromStealthHash, fpsStreamingFlags);
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetFPSTransitionToStealthClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, FPSTransitionToStealthHash, fpsStreamingFlags);
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetFPSTransitionToStealthFromUnholsterClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, FPSTransitionToStealthFromUnholsterHash, fpsStreamingFlags);
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetFPSFidgetClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags, int iIndex) const
{
	ReturnFidgetHashForPedAndWeaponAndIndex(rPed, hWeapon, fpsStreamingFlags, iIndex)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetMovementOverrideClipSetHash(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, MovementOverrideClipSetHash, fpsStreamingFlags)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetWeaponClipSetHashForClone(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, WeaponClipSetHashForClone, fpsStreamingFlags)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetMotionClipSetHashForClone(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	ReturnHashForPedAndWeapon(rPed, hWeapon, MotionClipSetHashForClone, fpsStreamingFlags)
}

atHashWithStringNotFinal CWeaponAnimationsManager::GetFPSStrafeClipSetHash(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	for(const CWeaponAnimationsSet* pWeaponAnimationsSet = GetWeaponAnimationsSetForPed(rPed, fpsStreamingFlags); pWeaponAnimationsSet; pWeaponAnimationsSet = GetWeaponAnimationsSetForHash(pWeaponAnimationsSet->m_Fallback))
	{
		if(pWeaponAnimationsSet->FPSStrafeClipSetHash.IsNotNull())
		{
			return pWeaponAnimationsSet->FPSStrafeClipSetHash;
		}
	}

	return atHashWithStringNotFinal::Null();
}

int CWeaponAnimationsManager::GetNumFPSFidgets(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	for(const CWeaponAnimationsSet* pWeaponAnimationsSet = GetWeaponAnimationsSetForPed(rPed, fpsStreamingFlags); pWeaponAnimationsSet; pWeaponAnimationsSet = GetWeaponAnimationsSetForHash(pWeaponAnimationsSet->m_Fallback))
	{ 
		const CWeaponAnimations* pWeaponAnimations = pWeaponAnimationsSet->m_WeaponAnimations.SafeGet(hWeapon);
		if(pWeaponAnimations)
		{ 
			int hValue = pWeaponAnimations->FPSFidgetClipsetHashes.GetCount(); 
			return hValue;
		} 
	}
	return 0;
}

float CWeaponAnimationsManager::GetAnimBlindFireRateModifier(atHashWithStringNotFinal hWeapon) const
{
	ReturnFloatForWeapon(hWeapon, AnimBlindFireRateModifier)
}

float CWeaponAnimationsManager::GetAnimFireRateModifier(atHashWithStringNotFinal hWeapon) const
{
	ReturnFloatForWeapon(hWeapon, AnimFireRateModifier)
}

float CWeaponAnimationsManager::GetAnimWantingToShootFireRateModifier(atHashWithStringNotFinal hWeapon) const
{
	ReturnFloatForWeapon(hWeapon, AnimWantingToShootFireRateModifier)
}

bool CWeaponAnimationsManager::GetUseFromStrafeUpperBodyAimNetwork(atHashWithStringNotFinal hWeapon) const
{
	ReturnBoolForWeapon(hWeapon, UseFromStrafeUpperBodyAimNetwork)
}

bool CWeaponAnimationsManager::GetAimingDownTheBarrel(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	for(const CWeaponAnimationsSet* pWeaponAnimationsSet = GetWeaponAnimationsSetForPed(rPed, fpsStreamingFlags); pWeaponAnimationsSet; pWeaponAnimationsSet = GetWeaponAnimationsSetForHash(pWeaponAnimationsSet->m_Fallback))
	{ 
		const CWeaponAnimations* pWeaponAnimations = pWeaponAnimationsSet->m_WeaponAnimations.SafeGet(hWeapon);
		if(pWeaponAnimations)
		{
			return pWeaponAnimations->AimingDownTheBarrel;
		}
	}

	ReturnBoolForWeapon(hWeapon, AimingDownTheBarrel)
}

bool CWeaponAnimationsManager::GetUseLeftHandIKAllowTags(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	for(const CWeaponAnimationsSet* pWeaponAnimationsSet = GetWeaponAnimationsSetForPed(rPed, fpsStreamingFlags); pWeaponAnimationsSet; pWeaponAnimationsSet = GetWeaponAnimationsSetForHash(pWeaponAnimationsSet->m_Fallback))
	{ 
		const CWeaponAnimations* pWeaponAnimations = pWeaponAnimationsSet->m_WeaponAnimations.SafeGet(hWeapon);
		if(pWeaponAnimations)
		{
			return pWeaponAnimations->UseLeftHandIKAllowTags;
		}
	}

	ReturnBoolForWeapon(hWeapon, UseLeftHandIKAllowTags)
}

bool CWeaponAnimationsManager::GetBlockLeftHandIKWhileAiming(const CPed& rPed, atHashWithStringNotFinal hWeapon, const FPSStreamingFlags fpsStreamingFlags) const
{
	for(const CWeaponAnimationsSet* pWeaponAnimationsSet = GetWeaponAnimationsSetForPed(rPed, fpsStreamingFlags); pWeaponAnimationsSet; pWeaponAnimationsSet = GetWeaponAnimationsSetForHash(pWeaponAnimationsSet->m_Fallback))
	{ 
		const CWeaponAnimations* pWeaponAnimations = pWeaponAnimationsSet->m_WeaponAnimations.SafeGet(hWeapon);
		if(pWeaponAnimations)
		{
			return pWeaponAnimations->BlockLeftHandIKWhileAiming;
		}
	}

	ReturnBoolForWeapon(hWeapon, BlockLeftHandIKWhileAiming)
}

void CWeaponAnimationsManager::Init()
{
	Assert(!sm_Instance);
	sm_Instance = rage_new CWeaponAnimationsManager;
	Load();
}

void CWeaponAnimationsManager::Shutdown()
{
	Assert(sm_Instance);
	delete sm_Instance;
	sm_Instance = NULL;
}

#if __BANK
atString CWeaponAnimationsManager::GetWidgetGroupName()
{
	return atString("Weapon Animations");
}

void CWeaponAnimationsManager::AddWidgetButton(bkBank& bank)
{
	bank.PushGroup(GetWidgetGroupName().c_str());

	atString metadataWidgetCreateLabel("");
	metadataWidgetCreateLabel += "Create ";
	metadataWidgetCreateLabel += GetWidgetGroupName();
	metadataWidgetCreateLabel += " widgets";

	bank.AddButton(metadataWidgetCreateLabel.c_str(), datCallback(CFA1(CWeaponAnimationsManager::AddWidgets), &bank));

	bank.PopGroup();
}

void CWeaponAnimationsManager::AddWidgets(bkBank& bank)
{
	// Destroy stub group before creating new one
	if (bkWidget* pWidgetGroup = bank.FindChild(GetWidgetGroupName().c_str()))
	{
		pWidgetGroup->Destroy();
	}

	bank.PushGroup(GetWidgetGroupName().c_str());

	bank.AddButton("Load", Load);
	bank.AddButton("Save", Save);

	bank.PushGroup("Metadata");
	PARSER.AddWidgets(bank, &sm_Instance->m_WeaponAnimationsSets);
	bank.PopGroup();

	bank.PopGroup();
}
#endif // __BANK

void CWeaponAnimationsManager::Load()
{
	sm_Instance->LoadAllMetaFiles();
}

#if __BANK
void CWeaponAnimationsManager::Save()
{
	weaponVerifyf(PARSER.SaveObject("common:/data/ai/weaponanimations", "meta", &sm_Instance->m_WeaponAnimationsSets, parManager::XML), "Failed to save weapon animations");
}
#endif // __BANK

bool CWeaponAnimationsManager::DoesWeaponAnimationSetForHashExist(atHashWithStringNotFinal hName) const
{
	if(GetWeaponAnimationsSetForHash(hName))
	{
		return true;
	}
	return false;
}

const CWeaponAnimationsSet* CWeaponAnimationsManager::GetWeaponAnimationsSetForHash(atHashWithStringNotFinal hName) const
{
	return m_WeaponAnimationsSets.m_WeaponAnimationsSets.SafeGet(hName);
}

const CWeaponAnimationsSet* CWeaponAnimationsManager::GetWeaponAnimationsSetForPed(const CPed& rPed, const FPSStreamingFlags FPS_MODE_SUPPORTED_ONLY(fpsStreamingFlags)) const
{
	//Keep track of the weapon animations.
	atHashWithStringNotFinal hWeaponAnimations = CWeaponAnimationsGlobals::DEFAULT_WEAPON_ANIMATIONS;
	const CPedModelInfo* pPedModelInfo = rPed.GetPedModelInfo();

#if FPS_MODE_SUPPORTED
	if(fpsStreamingFlags != FPS_StreamThirdPerson)
	{
		bool bUseFirstPersonAnimsForClone = rPed.IsNetworkClone() && rPed.IsAPlayerPed() && rPed.UseFirstPersonUpperBodyAnims(false);
	
		if(bUseFirstPersonAnimsForClone || rPed.IsFirstPersonShooterModeEnabledForPlayer(false) || rPed.IsInFirstPersonVehicleCamera() || (fpsStreamingFlags != FPS_StreamDefault))
		{
			const bool bFirstPersonDriveby = rPed.GetIsInVehicle() || rPed.GetIsParachuting();
			const bool bFirstPersonBlindFire = rPed.GetPedResetFlag(CPED_RESET_FLAG_IsBlindFiring);

			if (pPedModelInfo && pPedModelInfo->GetPersonalitySettings().GetWeaponAnimsFPSIdle() != 0)
			{
				hWeaponAnimations = pPedModelInfo->GetPersonalitySettings().GetWeaponAnimsFPSIdle();
			}
			else
			{
				hWeaponAnimations = CWeaponAnimationsGlobals::DEFAULT_WEAPON_FIRSTPERSON_ANIMATIONS;
			}
		
			if (fpsStreamingFlags != FPS_StreamIdle)
			{
				if(((bFirstPersonDriveby || bFirstPersonBlindFire || rPed.GetMotionData()->GetIsFPSRNG()) && fpsStreamingFlags == FPS_StreamDefault)|| fpsStreamingFlags == FPS_StreamRNG)
				{
					if (pPedModelInfo && pPedModelInfo->GetPersonalitySettings().GetWeaponAnimsFPSRNG() != 0)
					{
						hWeaponAnimations = pPedModelInfo->GetPersonalitySettings().GetWeaponAnimsFPSRNG();
					}
					else
					{
						hWeaponAnimations = CWeaponAnimationsGlobals::DEFAULT_WEAPON_FIRSTPERSON_RNG_ANIMATIONS;
					}
				}
				else if((rPed.GetMotionData()->GetIsFPSLT() && fpsStreamingFlags == FPS_StreamDefault) || fpsStreamingFlags == FPS_StreamLT)
				{
					if (pPedModelInfo && pPedModelInfo->GetPersonalitySettings().GetWeaponAnimsFPSLT() != 0)
					{
						hWeaponAnimations = pPedModelInfo->GetPersonalitySettings().GetWeaponAnimsFPSLT();
					}
					else
					{
						hWeaponAnimations = CWeaponAnimationsGlobals::DEFAULT_WEAPON_FIRSTPERSON_AIMING_ANIMATIONS;
					}
				}
				else if ((rPed.GetMotionData()->GetIsFPSScope() && fpsStreamingFlags == FPS_StreamDefault) || fpsStreamingFlags == FPS_StreamScope)
				{
					if (pPedModelInfo && pPedModelInfo->GetPersonalitySettings().GetWeaponAnimsFPSScope() != 0)
					{
						hWeaponAnimations = pPedModelInfo->GetPersonalitySettings().GetWeaponAnimsFPSScope();
					}
					else
					{
						hWeaponAnimations = CWeaponAnimationsGlobals::DEFAULT_WEAPON_FIRSTPERSON_SCOPE_ANIMATIONS;
					}
				}
			}

			const CWeaponAnimationsSet* pAnimationSet = GetWeaponAnimationsSetForHash(hWeaponAnimations);
			if(pAnimationSet)
			{
				return pAnimationSet;
			}
		}
	}
#endif // FPS_MODE_SUPPORTED

	//Check if the model info is valid.
	if(pPedModelInfo)
	{
		//If the override has been set, use it.
		if(rPed.GetOverrideWeaponAnimations() != 0)
		{
			hWeaponAnimations = rPed.GetOverrideWeaponAnimations();
		}
		else
		{
			TUNE_GROUP_BOOL(ALTERNATE_WEAPONS_ANIMS, bUseAlternateAnims, true);
			if(pPedModelInfo->GetPersonalitySettings().GetNumWeaponAnimations()==0)
			{
				hWeaponAnimations = atHashWithStringNotFinal("default",0xe4df46d5);
			}
			else
			{
				u32 uIndex = bUseAlternateAnims ? rPed.GetWeaponAnimationsIndex() : 0;

				//! players don't use variations. 
				if(rPed.IsPlayer())
					uIndex = 0;

#if __BANK
				TUNE_GROUP_INT(ALTERNATE_WEAPONS_ANIMS, iForceWeaponAnimIndex, -1, -1, 99, 1);
				if(iForceWeaponAnimIndex >= 0 && iForceWeaponAnimIndex < pPedModelInfo->GetPersonalitySettings().GetNumWeaponAnimations())
				{
					uIndex = (u32)iForceWeaponAnimIndex;
				}
#endif

				hWeaponAnimations = pPedModelInfo->GetPersonalitySettings().GetWeaponAnimations(uIndex);
			}
		}
	}

	//Get the weapon animations.
	return GetWeaponAnimationsSetForHash(hWeaponAnimations);
}
