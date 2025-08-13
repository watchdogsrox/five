// File header
#include "Peds/PedWeapons/PedAccuracy.h"
#include "Peds/PedWeapons/PedAccuracy_parser.h"

// Game headers
#include "Camera/CamInterface.h"
#include "Camera/Gameplay/GameplayDirector.h"
#include "Camera/Gameplay/Aim/ThirdPersonAimCamera.h"
#include "Debug/DebugScene.h"
#include "Network/Network.h"
#include "Peds/Ped_Channel.h"
#include "Peds/PedIntelligence.h"
#include "Peds/Ped.h"
#include "Stats/StatsInterface.h"
#include "Task/Combat/Cover/TaskCover.h"
#include "Task/Combat/TaskNewCombat.h"
#include "Task/Default/TaskPlayer.h"
#include "Weapons/Weapon.h"
#include "Weapons/WeaponDebug.h"
#include "Task/Vehicle/TaskVehicleWeapon.h"

// Rage headers
#include "parser/manager.h"

AI_OPTIMISATIONS()
WEAPON_OPTIMISATIONS()

// Static initialization
sPedAccuracyModifiers sPedAccuracyModifiers::ms_Instance;
#define PEDACCMODS sPedAccuracyModifiers::ms_Instance

float CPedAccuracy::fMPAmbientLawPedAccuracyModifier = 1.0f;

//////////////////////////////////////////////////////////////////////////
// CPedAccuracyResetVariables
//////////////////////////////////////////////////////////////////////////

CPedAccuracyResetVariables::CPedAccuracyResetVariables()
{
	Reset();
}

void CPedAccuracyResetVariables::Reset()
{
	m_accuracyFlags.ClearAllFlags();
	m_internalFlags.ClearAllFlags();
}

float CPedAccuracyResetVariables::GetAccuracyModifier() const
{
	if(!m_internalFlags.IsFlagSet(Flag_AccuracyCalculated))
	{
		m_fAccuracyModifier = 1.0f;

		if(m_accuracyFlags.IsFlagSet(Flag_TargetComingOutOfCover))
		{
			m_fAccuracyModifier *= PEDACCMODS.AI_TARGET_COMING_OUT_OF_COVER_MODIFIER;
		}

		if(m_accuracyFlags.IsFlagSet(Flag_TargetInCombatRoll))
		{
			m_fAccuracyModifier *= PEDACCMODS.AI_TARGET_IN_COMBAT_ROLL_MODIFIER;
		}

		if(m_accuracyFlags.IsFlagSet(Flag_TargetInCover))
		{
			m_fAccuracyModifier *= PEDACCMODS.AI_TARGET_IN_COVER_MODIFIER;
		}

		if(m_accuracyFlags.IsFlagSet(Flag_TargetWalking))
		{
			m_fAccuracyModifier *= PEDACCMODS.AI_TARGET_WALKING_MODIFIER;
		}

		if(m_accuracyFlags.IsFlagSet(Flag_TargetRunning))
		{
			m_fAccuracyModifier *= PEDACCMODS.AI_TARGET_RUNNING_MODIFIER;
		}

		if(m_accuracyFlags.IsFlagSet(Flag_TargetInAir))
		{
			m_fAccuracyModifier *= PEDACCMODS.AI_TARGET_IN_AIR_MODIFIER;
		}

		if(m_accuracyFlags.IsFlagSet(Flag_TargetDrivingAtSpeed))
		{
			m_fAccuracyModifier *= PEDACCMODS.AI_TARGET_DRIVING_AT_SPEED_MODIFIER;
		}

		// Mark ourselves as having calculated our accuracy
		m_internalFlags.SetFlag(Flag_AccuracyCalculated);
	}

	return m_fAccuracyModifier;
}

//////////////////////////////////////////////////////////////////////////
// CPedAccuracy
//////////////////////////////////////////////////////////////////////////

// Debug rendering
#if DEBUG_DRAW
#define MAX_ACCURACY_DRAWABLES 100
CDebugDrawStore CPedAccuracy::ms_debugDraw(MAX_ACCURACY_DRAWABLES);
#endif // DEBUG_DRAW

CPedAccuracy::CPedAccuracy()
: m_fNormalisedTimeFiring(0.0f)
{
}

void CPedAccuracy::ProcessPlayerAccuracy(const CPed& ped)
{
	pedAssertf(ped.IsPlayer(), "Not a player");
	ProcessPlayerRecoilAccuracy(ped);
}

void CPedAccuracy::CalculateAccuracyRange(const CPed& ped, const CWeaponInfo& weaponInfo, float fDistanceToTarget, sWeaponAccuracy& resultantAccuracy) const
{
	if(ped.IsPlayer())
	{
		CalculatePlayerAccuracyRange(ped, weaponInfo, fDistanceToTarget, resultantAccuracy);
	}
	else
	{
		CalculateAIAccuracyRange(ped, weaponInfo, fDistanceToTarget, resultantAccuracy);
	}

	// Clamp min/max within reasonable values
	resultantAccuracy.fAccuracyMax = Clamp(resultantAccuracy.fAccuracyMax, 0.f, 1.f);
	resultantAccuracy.fAccuracyMin = Min(resultantAccuracy.fAccuracyMin, resultantAccuracy.fAccuracyMax);
}

void CPedAccuracy::ProcessPlayerRecoilAccuracy(const CPed& ped)
{
	// Store the current recoil accuracy value as a normalised value, so it maps onto weapons with different error times effectively
	const CWeapon* pWeapon = NULL;
	const CWeaponInfo* pWeaponInfo = NULL;

	//B*1814982: Increase spread/accuracy of spycar machinegun and drone laser. Not doing this for all vehicles as we shipped without this.
	bool bUseMountedVehicleWeapon = false;
	if(ped.GetWeaponManager()->GetEquippedVehicleWeaponIndex() != -1) 
	{
		const CVehicleWeapon* pVehWeapon = ped.GetWeaponManager()->GetEquippedVehicleWeapon();
		pWeaponInfo = pVehWeapon ? pVehWeapon->GetWeaponInfo() : NULL;
		static const atHashWithStringNotFinal SPYCAR_MG_HASH("VEHICLE_WEAPON_SPYCARGUN", 0xa9ee94f7);
		static const atHashWithStringNotFinal DRONE_LASER_HASH("VEHICLE_WEAPON_DRONE_LASER", 0x9f308d7f);
		if (pWeaponInfo && (pWeaponInfo->GetHash() == SPYCAR_MG_HASH || pWeaponInfo->GetHash() == DRONE_LASER_HASH))
		{
			bUseMountedVehicleWeapon = true;
		}
	}
	if (!bUseMountedVehicleWeapon)
	{
		pWeapon = ped.GetWeaponManager()->GetEquippedWeapon();
		pWeaponInfo = pWeapon ? pWeapon->GetWeaponInfo() : NULL;
	}

	float fRecoilErrorTime;
	float fRecoilRecoveryRate;
	if(pWeaponInfo)
	{
		fRecoilErrorTime = pWeaponInfo->GetRecoilErrorTime();
		fRecoilRecoveryRate = pWeaponInfo->GetRecoilRecoveryRate();
	}
	else
	{
		fRecoilErrorTime = 0.f;
		fRecoilRecoveryRate = 1.f;
	}

	if(fRecoilErrorTime > 0.f)
	{
		float fTimeSpentFiring = m_fNormalisedTimeFiring * fRecoilErrorTime;

		bool bIsFiring = false;
		if (!bUseMountedVehicleWeapon)
		{
			CTaskGun* pTaskGun = static_cast<CTaskGun*>(ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_GUN));
			bIsFiring = pTaskGun ? pTaskGun->GetIsFiring() : false;
		}
		else
		{
			CTaskVehicleMountedWeapon *pTaskMountedWeapon = static_cast<CTaskVehicleMountedWeapon*>(ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_VEHICLE_MOUNTED_WEAPON));
			bIsFiring = pTaskMountedWeapon ? pTaskMountedWeapon->IsFiring() : false;
		}

		// Allow more responsive recovery calculations for some weapons, based on weapon state rather than ped task state
		if (pWeapon && pWeaponInfo && pWeaponInfo->GetResponsiveRecoilRecovery())
		{
			switch(pWeapon->GetState())
			{
			case CWeapon::STATE_WAITING_TO_FIRE:
				bIsFiring = true;
				break;
			case CWeapon::STATE_RELOADING:
				// Do nothing, use the existing value from ped task
				break;
			default:
				bIsFiring = false;
			}
		}

		if(bIsFiring)
		{
			fTimeSpentFiring += fwTimer::GetTimeStep();
		}
		else
		{
			Approach(fTimeSpentFiring, 0.f, fRecoilRecoveryRate, fwTimer::GetTimeStep());
		}

		fTimeSpentFiring = Min(fTimeSpentFiring, fRecoilErrorTime);
		m_fNormalisedTimeFiring = fTimeSpentFiring / fRecoilErrorTime;
	}
	else
	{
		// If no error time, reduce the normalised value (essentially treating it as 1 second)
		Approach(m_fNormalisedTimeFiring, 0.f, fRecoilRecoveryRate, fwTimer::GetTimeStep());
	}
}

void CPedAccuracy::CalculatePlayerAccuracyRange(const CPed& ped, const CWeaponInfo& weaponInfo, float /*fDistanceToTarget*/, sWeaponAccuracy& resultantAccuracy) const
{
	//
	// Modify MIN accuracy
	//

	resultantAccuracy.fAccuracyMin  = CalculatePlayerRecoilAccuracy(ped, weaponInfo);
	resultantAccuracy.fAccuracyMin *= CalculatePlayerRunAndGunAccuracyModifier(ped, weaponInfo);
	resultantAccuracy.fAccuracyMin *= CalculateBlindFiringAccuracyModifier(ped);
	resultantAccuracy.fAccuracyMin *= CalculatePlayerCameraAccuracyModifier(weaponInfo);

	// Apply override in certain situations (such as sniper rifles which have perfect aim, so mods wouldn't apply)
	ApplyPlayerRunAndGunAccuracyOverride(ped, weaponInfo, resultantAccuracy);

	// Invert the value, 1.f is accurate, 0.f inaccurate
	resultantAccuracy.fAccuracyMin  = Clamp(1.f - resultantAccuracy.fAccuracyMin, 0.f, 1.f);

	//
	// Modify MAX accuracy
	//

	resultantAccuracy.fAccuracyMax  = 1.f;
	resultantAccuracy.fAccuracyMax *= CalculatePlayerRecentlyHitInMPAccuracyModifier(ped);
	resultantAccuracy.fAccuracyMax  = Clamp(resultantAccuracy.fAccuracyMax, 0.f, 1.f);
}

void CPedAccuracy::CalculateAIAccuracyRange(const CPed& ped, const CWeaponInfo& weaponInfo, float fDistanceToTarget, sWeaponAccuracy& resultantAccuracy) const
{
	//
	// Work out the max ped accuracy
	//

	float fPedAccuracy = ped.GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatWeaponAccuracy);
	fPedAccuracy *= PEDACCMODS.AI_GLOBAL_MODIFIER;
	fPedAccuracy  = Clamp(fPedAccuracy, 0.f, 1.f);

	// Modify the accuracy based on the weapon.
	fPedAccuracy = ModifyAccuracyBasedOnWeapon(ped, fPedAccuracy, weaponInfo);

	// Modify the accuracy based on distance.
	fPedAccuracy = ModifyAccuracyBasedOnDistance(ped, fPedAccuracy, fDistanceToTarget);

	// Modify the accuracy based on the number of enemies.
	fPedAccuracy = ModifyAccuracyBasedOnNumberOfEnemies(ped, fPedAccuracy);

	// Modify the accuracy based on ped type.
	fPedAccuracy = ModifyAccuracyBasedOnPedType(ped, fPedAccuracy);

	// Init the min/max to this value
	fPedAccuracy = Clamp(fPedAccuracy, 0.f, 1.f);
	resultantAccuracy.fAccuracyMin = fPedAccuracy;
	resultantAccuracy.fAccuracyMax = fPedAccuracy;

	//
	// Modify MIN accuracy
	//

	resultantAccuracy.fAccuracyMin *= m_resetVariables.GetAccuracyModifier();
	resultantAccuracy.fAccuracyMin *= CalculateBlindFiringAccuracyModifier(ped);
	resultantAccuracy.fAccuracyMin *= CalculateHurtAccuracyModifier(ped);

	//
	// Modify MAX accuracy
	//
}

float CPedAccuracy::CalculatePlayerRecoilAccuracy(const CPed& ped, const CWeaponInfo& weaponInfo) const
{
	if(m_fNormalisedTimeFiring > 0.f)
	{
		float fRecoilAccuracyMax = weaponInfo.GetRecoilAccuracyMax(); 

		if(ped.GetIsCrouching())
		{
			fRecoilAccuracyMax *= PEDACCMODS.PLAYER_RECOIL_CROUCHED_MODIFIER;
		}

		// Modify by skill - the better the skill, we tend to the MIN value
		static const float ONE_OVER_100 = 1.f / 100.f;
		float fShootingAbility = StatsInterface::GetFloatStat(StatsInterface::GetStatsModelHashId("SHOOTING_ABILITY")) * ONE_OVER_100;
		float fMod = PEDACCMODS.PLAYER_RECOIL_MODIFIER_MIN + ((1.f - fShootingAbility) * (PEDACCMODS.PLAYER_RECOIL_MODIFIER_MAX - PEDACCMODS.PLAYER_RECOIL_MODIFIER_MIN));
		fRecoilAccuracyMax *= fMod;

		float fAccuracyModifier = Lerp(m_fNormalisedTimeFiring, 0.f, fRecoilAccuracyMax);
		return fAccuracyModifier;
	}

	return 0.f;
}

float CPedAccuracy::CalculatePlayerRunAndGunAccuracyModifier(const CPed& ped, const CWeaponInfo& weaponInfo) const
{
	if(ped.GetPlayerResetFlag(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON) || ped.GetPlayerResetFlag(CPlayerResetFlags::PRF_RUN_AND_GUN))
	{
		float fAccuracyModifier = weaponInfo.GetRunAndGunAccuracyModifier();
		return fAccuracyModifier;
	}

	return 1.f;
}

void CPedAccuracy::ApplyPlayerRunAndGunAccuracyOverride(const CPed& ped, const CWeaponInfo& weaponInfo, sWeaponAccuracy& resultantAccuracy) const
{
	if(ped.GetPlayerResetFlag(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON) || ped.GetPlayerResetFlag(CPlayerResetFlags::PRF_RUN_AND_GUN))
	{
		float fAccuracyOverride = weaponInfo.GetRunAndGunAccuracyMinOverride();
		if (fAccuracyOverride >= 0.f && fAccuracyOverride <= 1.f)
		{
			resultantAccuracy.fAccuracyMin = fAccuracyOverride;
		}
	}
}

float CPedAccuracy::CalculatePlayerCameraAccuracyModifier(const CWeaponInfo& weaponInfo) const
{
	bool bIsAccuratelyAiming = CPlayerPedTargeting::IsCameraInAccurateMode();
	if(bIsAccuratelyAiming)
	{
		float fExtraZoomFactor = Max(CPlayerPedTargeting::ComputeExtraZoomFactorForAccurateMode(weaponInfo), 1.f); 
		// Invert the zoom factor, so this applies to the inaccuracy, shouldn't make it less accurate, so clamp to minimum of 1.f
		fExtraZoomFactor = 1.f / fExtraZoomFactor;
		float fAccuracyModifier = weaponInfo.GetAccurateModeAccuracyModifier();
		fAccuracyModifier *= fExtraZoomFactor;
		return fAccuracyModifier;
	}

	return 1.f;
}

float CPedAccuracy::CalculatePlayerRecentlyHitInMPAccuracyModifier(const CPed& ped) const
{
	const bool bPlayerWasRecentlyHit = CNetwork::IsGameInProgress() && fwTimer::GetTimeInMilliseconds() < ped.GetDamageDelayTimer();
	if(bPlayerWasRecentlyHit)
	{
		float fAccuracyModifier = PEDACCMODS.PLAYER_RECENTLY_DAMAGED_MODIFIER;
		return fAccuracyModifier;
	}

	return 1.f;
}

float CPedAccuracy::CalculateBlindFiringAccuracyModifier(const CPed& ped) const
{
	if(m_resetVariables.IsFlagSet(CPedAccuracyResetVariables::Flag_BlindFiring))
	{
		if(ped.IsPlayer())
		{
			// Modify by skill - the better the skill, we tend to the MIN value
			static const float ONE_OVER_100 = 1.f / 100.f;
			float fShootingAbility = StatsInterface::GetFloatStat(StatsInterface::GetStatsModelHashId("SHOOTING_ABILITY")) * ONE_OVER_100;
			float fAccuracyModifier = PEDACCMODS.PLAYER_BLIND_FIRE_MODIFIER_MIN + ((1.f - fShootingAbility) * (PEDACCMODS.PLAYER_BLIND_FIRE_MODIFIER_MAX - PEDACCMODS.PLAYER_BLIND_FIRE_MODIFIER_MIN));
			return fAccuracyModifier;
		}
		else
		{
			float fAccuracyModifier = PEDACCMODS.AI_BLIND_FIRE_MODIFIER;
			return fAccuracyModifier;
		}
	}

	return 1.f;
}

float CPedAccuracy::CalculateHurtAccuracyModifier(const CPed& ped) const
{
	if(ped.HasHurtStarted())
	{
		if(ped.GetPedResetFlag(CPED_RESET_FLAG_ShootFromGround))
		{
			return PEDACCMODS.AI_HURT_ON_GROUND_MODIFIER;
		}
		else
		{
			return PEDACCMODS.AI_HURT_MODIFIER;
		}
	}

	return 1.f;
}

float CPedAccuracy::ModifyAccuracyBasedOnWeapon(const CPed& ped, float fAccuracy, const CWeaponInfo& weaponInfo) const
{
	//Assert that the ped is not a player.
	pedAssert(!ped.IsPlayer());

	//Check if the ped is ambient.
	if(ped.PopTypeIsRandom())
	{
		//Check if the ped is a professional.
		if(ped.GetPedIntelligence()->GetCombatBehaviour().GetCombatAbility() == CCombatData::CA_Professional)
		{
			//Check if the weapon is a pistol.
			if(weaponInfo.GetGroup() == WEAPONGROUP_PISTOL)
			{
				//Check if the ped is running a gun task.
				if(CTaskGun* pTaskGun = static_cast<CTaskGun *>(ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_GUN)))
				{
					//Check if the target is not a player.
					if(pTaskGun->GetTarget().GetEntity() && pTaskGun->GetTarget().GetEntity()->GetIsTypePed() && !static_cast<const CPed *>(pTaskGun->GetTarget().GetEntity())->IsPlayer())
					{
						fAccuracy *= PEDACCMODS.AI_PROFESSIONAL_PISTOL_VS_AI_MODIFIER;
					}
				}
			}
		}
	}

	return fAccuracy;
}

float CPedAccuracy::ModifyAccuracyBasedOnDistance(const CPed& ped, float fAccuracy, float fDistanceToTarget) const
{
	// Note:
	//		At the moment, accuracy is only increased due to proximity to the target.
	//		It is not decreased when the target is far away.
	//		If this is added, some of the early-out checks will need to change.

	// Assert that the ped is not a player.
	pedAssertf(!ped.IsPlayer(), "The ped is a player.");

	// Ensure the ped has the flag set.
	if(!ped.GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_UseProximityAccuracy))
	{
		return fAccuracy;
	}

	// Ensure the accuracy has room to improve.
	if(fAccuracy >= 1.0f)
	{
		return fAccuracy;
	}

	// Calculate a lerp value based on the distance.
	float fDistanceForMaxAccuracyBoost = PEDACCMODS.AI_DISTANCE_FOR_MAX_ACCURACY_BOOST;
	float fDistanceForMinAccuracyBoost = PEDACCMODS.AI_DISTANCE_FOR_MIN_ACCURACY_BOOST;
	pedAssertf(fDistanceForMaxAccuracyBoost <= fDistanceForMinAccuracyBoost, "The distance for the min accuracy boost should not be less than the distance for the max accuracy boost.");
	fDistanceToTarget = Clamp(fDistanceToTarget, fDistanceForMaxAccuracyBoost, fDistanceForMinAccuracyBoost);
	float fLerp = (fDistanceToTarget - fDistanceForMaxAccuracyBoost) / (fDistanceForMinAccuracyBoost - fDistanceForMaxAccuracyBoost);

	// Calculate the accuracy.
	fAccuracy = Lerp(fLerp, 1.0f, fAccuracy);
	return fAccuracy;
}

float CPedAccuracy::ModifyAccuracyBasedOnNumberOfEnemies(const CPed& ped, float fAccuracy) const
{
	//Assert that the ped is not a player.
	pedAssertf(!ped.IsPlayer(), "The ped is a player.");

	// Check if this feature is not enabled on this ped
	if(!ped.GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_UseEnemyAccuracyScaling))
	{
		return fAccuracy;
	}

	// Check if the accuracy is already at or below the floor for this mechanism
	if(fAccuracy <= CTaskCombat::ms_Tunables.m_EnemyAccuracyScaling.m_fAccuracyReductionFloor)
	{
		return fAccuracy;
	}

	// Get a handle to the local player
	const CPed* pLocalPlayerPed = CGameWorld::FindLocalPlayer();
	if(pLocalPlayerPed && pLocalPlayerPed->GetPlayerInfo())
	{
		// Check if ped is in combat and has the local player as it's hostile target
		if(ped.GetPedIntelligence()->GetQueriableInterface()->GetHostileTarget() == pLocalPlayerPed)
		{
			// Get the count the number of enemies shooting at the local player
			int numEnemiesShootingCount = pLocalPlayerPed->GetPlayerInfo()->GetNumEnemiesShootingInCombat();

			// Check if the count of enemies is high enough
			if(numEnemiesShootingCount > CTaskCombat::ms_Tunables.m_EnemyAccuracyScaling.m_iMinNumEnemiesForScaling)
			{
#if __BANK
				const float fOriginalAccuracy = fAccuracy;
#endif // __BANK

				// How many times do we want to apply the scaling reduction?  Default to number of enemies shooting.
				int scalingReductionCount = numEnemiesShootingCount;

				// If we have a valid (non-negative) minimum, subtract the minimum from the number shooting so we can scale
				// this without a big step at (minimum+1).  Mad Doctor I, April 22, 2013.
				if (CTaskCombat::ms_Tunables.m_EnemyAccuracyScaling.m_iMinNumEnemiesForScaling > 0)
				{
					// Take away the minimum so our first scaling amount isn't a big step
					scalingReductionCount -= CTaskCombat::ms_Tunables.m_EnemyAccuracyScaling.m_iMinNumEnemiesForScaling;

				} // if (CTaskCombat::ms_Tunables.m_EnemyAccuracyScaling.m_iMinNumEnemiesForScaling > 0)...

				// Compute new reduced accuracy: number of enemies shooting (beyond minimum to reduce) times a tweakable percent.
				const float fAccuracyReduction = scalingReductionCount * CTaskCombat::ms_Tunables.m_EnemyAccuracyScaling.m_fAccuracyReductionPerEnemy;

				// Don't let it ever go below a floor (tweakable in tasktunables).
				fAccuracy = Max(CTaskCombat::ms_Tunables.m_EnemyAccuracyScaling.m_fAccuracyReductionFloor, fAccuracy - fAccuracyReduction);
#if __BANK
				if( CTaskCombat::ms_EnemyAccuracyLog.IsEnabled() )
				{
					int currentWantedLevel = -1;
					if( pLocalPlayerPed->GetPlayerWanted() )
					{
						currentWantedLevel = (int)pLocalPlayerPed->GetPlayerWanted()->GetWantedLevel();
					}
					const float fEffectiveAccuracyReduction = fOriginalAccuracy - fAccuracy;
					CTaskCombat::ms_EnemyAccuracyLog.RecordEnemyShotFired(currentWantedLevel, numEnemiesShootingCount, fEffectiveAccuracyReduction, fAccuracy);
				}
#endif // __BANK
			}
		}
	}

	return fAccuracy;
}

float CPedAccuracy::ModifyAccuracyBasedOnPedType(const CPed& ped, float fAccuracy) const
{
	if (NetworkInterface::IsGameInProgress() && !ped.IsAPlayerPed() && ped.PopTypeIsRandom() && ped.IsLawEnforcementPed())
	{
		fAccuracy *= fMPAmbientLawPedAccuracyModifier;
	}

	return fAccuracy;
}

//////////////////////////////////////////////////////////////////////////
// sPedAccuracyModifiers
//////////////////////////////////////////////////////////////////////////

void sPedAccuracyModifiers::Init()
{
	Load();
}

#if __BANK
void sPedAccuracyModifiers::InitWidgets()
{
    bkBank* pBank = BANKMGR.FindBank("Weapons");
    if(pBank)
    {
        pBank->PushGroup("Ped Accuracy Modifiers", false);
		pBank->AddButton("Load", Load);
		pBank->AddButton("Save", Save);
		PARSER.AddWidgets((*pBank), &ms_Instance);
        pBank->PopGroup();
    }
}
#endif // __BANK

void sPedAccuracyModifiers::Load()
{
	PARSER.LoadObject("commoncrc:/data/ai/pedaccuracy", "meta", ms_Instance);

#if __BANK
	// Init the widgets
	CWeaponDebug::InitBank();
#endif // __BANK
}

#if __BANK
void sPedAccuracyModifiers::Save()
{
	weaponVerifyf(PARSER.SaveObject("commoncrc:/data/ai/pedaccuracy", "meta", &ms_Instance, parManager::XML), "Failed to save accuracy modifiers");
}
#endif // __BANK
