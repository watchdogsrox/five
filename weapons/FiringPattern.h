//
// weapons/firingpattern.h
//
// Copyright (C) 1999-2010 Rockstar North.  All Rights Reserved. 
//

#ifndef FIRING_PATTERN_H
#define FIRING_PATTERN_H

// Rage headers
#include "atl/array.h"
#include "atl/hashstring.h"
#include "parser/macros.h"

// Framework headers
#include "fwmaths/random.h"
#include "fwtl/pool.h"
#include "fwtl/regdrefs.h"

// Game headers
#include "Peds/Ped.h"
#include "Weapons/WeaponChannel.h"

class CPed;
class CPedDebugVisualiser;

////////////////////////////////////////////////////////////////////////////////
// CFiringPatternInfo
////////////////////////////////////////////////////////////////////////////////

class CFiringPatternInfo : public fwRefAwareBase
{
public:

#if __BANK
	friend class CPedDebugVisualiser;
	friend class CCombatTextDebugInfo;
#endif

	// Construction
	CFiringPatternInfo();

	// Get the hash
	u32 GetHash() const
	{
		return m_Name.GetHash();
	}

	s32 GetNumberOfBursts() const
	{
		weaponAssert(m_NumberOfBurstsMin <= m_NumberOfBurstsMax);
		return fwRandom::GetRandomNumberInRange(m_NumberOfBurstsMin, m_NumberOfBurstsMax);
	}

	s32 GetNumberOfShotsPerBurst() const
	{
		weaponAssert(m_NumberOfShotsPerBurstMin <= m_NumberOfShotsPerBurstMax);
		return fwRandom::GetRandomNumberInRange(m_NumberOfShotsPerBurstMin, m_NumberOfShotsPerBurstMax);
	}

	f32 GetTimeBetweenShots(f32 fMod) const
	{
		weaponAssert(m_TimeBetweenShotsMin <= m_TimeBetweenShotsMax);
		return Max(fwRandom::GetRandomNumberInRange(m_TimeBetweenShotsMin, m_TimeBetweenShotsMax) * fMod, m_TimeBetweenShotsAbsoluteMin);
	}

	f32 GetTimeBetweenBursts(f32 fMod) const
	{
		weaponAssert(m_TimeBetweenBurstsMin <= m_TimeBetweenBurstsMax);
		return Max(fwRandom::GetRandomNumberInRange(m_TimeBetweenBurstsMin, m_TimeBetweenBurstsMax) * fMod, m_TimeBetweenBurstsAbsoluteMin);
	}

	f32 GetTimeBeforeFiring() const
	{
		weaponAssert(m_TimeBeforeFiringMin <= m_TimeBeforeFiringMax);
		return fwRandom::GetRandomNumberInRange(m_TimeBeforeFiringMin, m_TimeBeforeFiringMax);
	}

	bool GetBurstCooldownOnUnset() const { return m_BurstCooldownOnUnset; }

	// Function to sort the array so we can use a binary search
	static s32 PairSort(CFiringPatternInfo* const* a, CFiringPatternInfo* const* b) { return ((*a)->m_Name.GetHash() < (*b)->m_Name.GetHash() ? -1 : 1); }

#if !__FINAL
	// Get the name from the metadata manager
	const char* GetName() const { return m_Name.GetCStr(); }
#endif // !__FINAL

private:

	//
	// Members
	//

	atHashWithStringNotFinal m_Name;
	s16 m_NumberOfBurstsMin;
	s16 m_NumberOfBurstsMax;
	s16 m_NumberOfShotsPerBurstMin;
	s16 m_NumberOfShotsPerBurstMax;
	f32 m_TimeBetweenShotsMin;
	f32 m_TimeBetweenShotsMax;
	f32 m_TimeBetweenShotsAbsoluteMin;
	f32 m_TimeBetweenBurstsMin;
	f32 m_TimeBetweenBurstsMax;
	f32 m_TimeBetweenBurstsAbsoluteMin;
	f32 m_TimeBeforeFiringMin;
	f32 m_TimeBeforeFiringMax;
	bool m_BurstCooldownOnUnset;

private:
	// Not allowed to copy construct or assign
	CFiringPatternInfo(const CFiringPatternInfo& other);
	CFiringPatternInfo& operator=(const CFiringPatternInfo& other);

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CFiringPatternInfoManager
////////////////////////////////////////////////////////////////////////////////

class CFiringPatternInfoManager
{
public:

	// Initialise
	static void Init();

	// Shutdown
	static void Shutdown();

	// Access to the firing pattern data
	static const CFiringPatternInfo* GetInfo(u32 uNameHash)
	{
		if(uNameHash != 0)
		{
			s32 iLow = 0;
			s32 iHigh = ms_Instance.m_InfoPtrs.GetCount()-1;
			while(iLow <= iHigh)
			{
				s32 iMid = (iLow + iHigh) >> 1;
				if(uNameHash == ms_Instance.m_InfoPtrs[iMid]->GetHash())
				{
					return ms_Instance.m_InfoPtrs[iMid];
				}
				else if (uNameHash < ms_Instance.m_InfoPtrs[iMid]->GetHash())
				{
					iHigh = iMid-1;
				}
				else
				{
					iLow = iMid+1;
				}
			}
			weaponAssertf(0, "Firing pattern [%d] not found in data", uNameHash);
		}
		return NULL;
	}

#if __BANK
	// Add widgets
	static void AddWidgets(bkBank& bank);
#endif // __BANK

private:

	// Delete the data
	static void Reset();

	// Load the data
	static void Load();

#if __BANK
	// Save the data
	static void Save();
#endif // __BANK

	//
	// Members
	//

	// The pattern storage
	atArray<CFiringPatternInfo*> m_Infos;

	// Pointers to infos, sorted
	atArray<CFiringPatternInfo*> m_InfoPtrs;

	// Static manager object
	static CFiringPatternInfoManager ms_Instance;

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CFiringPattern
////////////////////////////////////////////////////////////////////////////////

class CFiringPattern
{
public:

#if __BANK
	friend class CPedDebugVisualiser;
	friend class CCombatTextDebugInfo;
#endif

	CFiringPattern();
	CFiringPattern(const CFiringPattern& other);

	// Set the active firing pattern - if the firing pattern is new, the data will be updated
	void SetFiringPattern(u32 uFiringPatternHash, bool bForceReset = false);
	u32  GetFiringPatternHash() { return m_uFiringPatternHash; }
	//
	// Processing
	//

	// Should we process the firing pattern?
	bool ShouldProcess(const CPed& ped) const;

	// Should be called every frame and before any other member functions are called
	void PreProcess(const CPed& ped);

	// Should be called every frame and after all member functions are called
	void PostProcess(float fTimeStep);

	// Called when the weapon has been fired
	void ProcessFired();

	// Called when the weapon has finished its burst
	void ProcessBurstFinished();

	//
	// Querying
	//

#if __BANK
	const CFiringPatternInfo* GetFiringPatternInfo() const { return m_pFiringPatternInfo; }
#endif // __BANK

	// Check if we can fire at all - do we have any shots/bursts remaining?
	bool WantsToFire() const
	{
		if((m_iNumberOfBursts == -1 || m_iNumberOfBursts > 0) && (m_iNumberOfShotsPerBurst == -1 || m_iNumberOfShotsPerBurst > 0))
		{
			return true;
		}

		return false;
	}

	// Check if we are ready to fire - have the timers ticked down?
	bool ReadyToFire() const
	{
		if(m_fTimeToDelayFiring <= 0.0f && m_fTimeTillNextShot <= 0.0f && m_fTimeTillNextBurst <= 0.0f && (!m_pFiringPatternInfo || m_uBurstCooldownOnUnsetPatternHash != m_pFiringPatternInfo->GetHash() || m_fBurstCooldownOnUnsetTimer <= 0.0f))
		{
			return true;
		}

		return false;
	}

	// Check if the burst is finished
	bool IsBurstFinished() const { return m_iNumberOfShotsPerBurst == 0; }

	bool HasBurstFinishedThisFrame() const { return m_bBurstFinishedThisFrame; }

	bool HasBurstCooldownOnUnset() const { return m_pFiringPatternInfo ? m_pFiringPatternInfo->GetBurstCooldownOnUnset() : false; }

	//
	// Overriding
	//

	// Change m_fTimeTillNextShotModifier
	void SetTimeTillNextShotModifier(f32 fTimeTillNextShotModifier) { m_fTimeTillNextShotModifier = fTimeTillNextShotModifier; }

	// Get m_fTimeTillNextShotModifier
	f32 GetTimeTillNextShotModifier() const { return m_fTimeTillNextShotModifier; }

	f32 GetTimeTillNextBurst() const { return m_fTimeTillNextBurst; }

	// Change m_fTimeTillNextBurstModifier
	void SetTimeTillNextBurstModifier(f32 fTimeTillNextBurstModifier) { m_fTimeTillNextBurstModifier = fTimeTillNextBurstModifier; }

	// Get m_fTimeTillNextBurstModifier
	f32 GetTimeTillNextBurstModifier() const { return m_fTimeTillNextBurstModifier; }

	// Force the time until next burst to be expired
	void ResetTimeUntilNextBurst() { m_fTimeTillNextBurst = 0.0f; }

private:

	// Reset the members
	void Reset();

	//
	// Members
	//

	// Our set firing pattern
	u32 m_uFiringPatternHash;

	// The weapon our firing pattern is for
	atHashWithStringNotFinal m_EquippedWeaponHash;

	// Our current firing pattern data
	fwRegdRef<const CFiringPatternInfo> m_pFiringPatternInfo;

	// Number of bursts to perform
	s32 m_iNumberOfBursts;

	// Number of shots per burst
	s32 m_iNumberOfShotsPerBurst;

	// The time before allowing firing
	f32 m_fTimeToDelayFiring;

	// The time between shots
	f32 m_fTimeTillNextShot;

	// A modifier that can scale the time between shots
	f32 m_fTimeTillNextShotModifier;

	// The time between bursts
	f32 m_fTimeTillNextBurst;

	// A modifier that can scale the time between bursts
	f32 m_fTimeTillNextBurstModifier;

	// The separate timer used for the firing pattern we've designated as forced
	f32 m_fBurstCooldownOnUnsetTimer;

	// The hash of the firing pattern we're forcing a burst cooldown on
	u32 m_uBurstCooldownOnUnsetPatternHash;

	// Flags
	bool m_bForceReset : 1;
	bool m_bBurstFinishedThisFrame : 1;

	//
	// Statics
	//

	static const s32 INFINITE_NUMBER_OF_BURSTS;
	static const s32 INFINITE_NUMBER_OF_SHOTS_PER_BURST;
	static const s32 DEFAULT_NUMBER_OF_BURSTS;
	static const s32 DEFAULT_NUMBER_OF_SHOTS_PER_BURST;
	static const f32 DEFAULT_TIME_TO_DELAY_FIRING;
	static const f32 DEFAULT_TIME_TILL_NEXT_SHOT;
	static const f32 DEFAULT_TIME_TILL_NEXT_SHOT_MOD;
	static const f32 DEFAULT_TIME_TILL_NEXT_BURST;
	static const f32 DEFAULT_TIME_TILL_NEXT_BURST_MOD;
};

////////////////////////////////////////////////////////////////////////////////

inline bool CFiringPattern::ShouldProcess(const CPed& ped) const
{
	if(ped.GetWeaponManager())
	{
		const CWeaponInfo* pEquippedWeaponInfo = ped.GetWeaponManager()->GetEquippedWeaponInfo();
		if(pEquippedWeaponInfo && (pEquippedWeaponInfo->GetIsGunOrCanBeFiredLikeGun() || pEquippedWeaponInfo->GetIsVehicleWeapon()))
		{
			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

inline void CFiringPattern::PostProcess(float fTimeStep)
{
	// Decrement timers
	m_fTimeToDelayFiring		   -= fTimeStep;
	m_fTimeTillNextShot			   -= fTimeStep;
	m_fTimeTillNextBurst		   -= fTimeStep;
	m_fBurstCooldownOnUnsetTimer -= fTimeStep;

	// Clear our burst fire finished flag
	m_bBurstFinishedThisFrame = false;

	// Reset the forced cooldown hash if our timer has expired
	if (m_fBurstCooldownOnUnsetTimer < 0.0f)
	{
		m_uBurstCooldownOnUnsetPatternHash = 0;
	}
}

////////////////////////////////////////////////////////////////////////////////

inline void CFiringPattern::Reset()
{
	m_iNumberOfBursts            = DEFAULT_NUMBER_OF_BURSTS;
	m_iNumberOfShotsPerBurst     = DEFAULT_NUMBER_OF_SHOTS_PER_BURST;
	m_fTimeToDelayFiring         = DEFAULT_TIME_TO_DELAY_FIRING;
	m_fTimeTillNextShot          = DEFAULT_TIME_TILL_NEXT_SHOT;
	m_fTimeTillNextBurst         = DEFAULT_TIME_TILL_NEXT_BURST;
	m_bBurstFinishedThisFrame	 = false;
}

////////////////////////////////////////////////////////////////////////////////

#endif // FIRING_PATTERN_H
