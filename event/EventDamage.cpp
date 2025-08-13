//
// filename:	EventDamage.cpp
// description:	
//
#include "event/eventDamage.h"

#include "fragmentnm/nm_channel.h"

// Framework Headers
#include "grcore/debugdraw.h"
#include "fwmaths/angle.h"
#include "audioengine/engineutil.h"

// Game headers
#include "audio/policescanner.h"
#include "audio/speechaudioentity.h"
#include "camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/gameplay/aim/FirstPersonShooterCamera.h"
#include "control/replay/ReplayBufferMarker.h"
#include "event/EventResponseFactory.h"
#include "event/EventShocking.h"
#include "event/ShockingEvents.h"
#include "Frontend/NewHud.h"
#include "game/cheat.h"
#include "ik/IkConfig.h"
#include "Network/Live/NetworkTelemetry.h"
#include "network/general/NetworkUtil.h"
#include "network/objects/entities/NetObjPlayer.h"
#include "network/players/NetGamePlayer.h"
#include "PedGroup/PedGroup.h"
#include "Peds/Ped.h"
#include "peds/pedGeometryAnalyser.h"
#include "peds/PedHelmetComponent.h"
#include "peds/pedIntelligence.h"
#include "Peds/pedType.h"
#include "Peds/HealthConfig.h"
#include "renderer/PostProcessFX.h"
#include "scene/world/gameWorld.h"
#include "Stats/StatsMgr.h"
#include "task/combat/TaskCombat.h"
#include "task/Combat/TaskNewCombat.h"
#include "task/Combat/TaskCombatMelee.h"
#include "Task/Combat/TaskDamageDeath.h"
#include "Task/Combat/TaskThreatResponse.h"
#include "Task/Combat/TaskToHurtTransit.h"
#include "task/combat/TaskWrithe.h"
#include "task/Default/TaskIncapacitated.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/General/TaskBasic.h"
#include "Task/General/TaskSecondary.h"
#include "Task/Motion/TaskMotionBase.h"
#include "Task/Movement/Jumping/TaskFallGetUp.h"
#include "Task/Movement/TaskGetUp.h"
#include "Task/Physics/TaskNM.h"
#include "Task/Physics/TaskNMShot.h"
#include "Task/Physics/TaskNMExplosion.h"
#include "task/Response/TaskAgitated.h"
#include "Task/Response/TaskFlee.h"
#include "Task/Response/TaskGangs.h"
#include "Task/Vehicle/TaskVehicleWeapon.h"
#include "Task/Vehicle/TaskExitVehicle.h"
#include "task/Weapons/Gun/TaskGun.h"
#include "vehicleAi/task/TaskVehicleThreePointTurn.h"
#include "vehicles/vehicle.h"
#include "vehicles/Metadata/VehicleSeatInfo.h"
#include "weapons/Info/WeaponInfo.h"
#include "Weapons/Info/WeaponInfoManager.h"
#include "Weapons/WeaponNetworkModifiers.h"
#include "Weapons/Explosion.h"
#include "network/General/NetworkUtil.h"
#include "game/ModelIndices.h"

#include "event/EventDamage_parser.h"
#include "Network/General/NetworkGhostCollisions.h"

AI_OPTIMISATIONS()
NETWORK_OPTIMISATIONS()
WEAPON_OPTIMISATIONS()

RAGE_DEFINE_SUBCHANNEL(net, ped_damage, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_ped_damage

XPARAM(invincibleMigratingPeds);

///////////////////////////////////
//PED DAMAGE RESPONSE CALCULATOR
///////////////////////////////////

float CPedDamageCalculator::sm_fAiMeleeWeaponDamageModifierDefault	= 1.0f;
float CPedDamageCalculator::sm_fAiMeleeWeaponDamageModifier			= CPedDamageCalculator::sm_fAiMeleeWeaponDamageModifierDefault;
float CPedDamageCalculator::sm_fAiWeaponDamageModifierDefault		= 0.33f;
float CPedDamageCalculator::sm_fAiWeaponDamageModifier				= CPedDamageCalculator::sm_fAiWeaponDamageModifierDefault;

#if !__FINAL

u32 CPedDamageCalculator::m_uRejectionReason = CPedDamageCalculator::DRR_None;
#define damageRejectionDebug1f(reason) CPedDamageCalculator::m_uRejectionReason = reason;
#define damageRejectionDebug2f(pVictim, pInflictor, nWeaponHash, damageResponse, nHitComponent, reason) CPedDamageCalculator::m_uRejectionReason = reason; pVictim->AddDamageRecord(CPed::sDamageRecord(pInflictor, fwTimer::GetTimeInMilliseconds(), nWeaponHash, damageResponse.m_fHealthLost, damageResponse.m_fArmourLost, damageResponse.m_fEnduranceLost, pVictim->GetHealth(), nHitComponent, damageResponse.m_bForceInstantKill, damageResponse.m_bInvincible, CPedDamageCalculator::m_uRejectionReason));

#else
#define damageRejectionDebug1f(reason)
#define damageRejectionDebug2f(pVictim, pInflictor, nWeaponHash, damageResponse, nHitComponent, reason)
#endif //__FINAL

CPedDamageCalculator::CPedDamageCalculator
(const CEntity* pInflictor, const float fRawDamage, const u32 nWeaponUsedHash, const int nComponent, const bool bJumpedOutOfMovingCar, const u8 nDamageAggregationCount)
: m_pInflictor(const_cast<CEntity*>(pInflictor)),
m_fRawDamage(fRawDamage),
m_nHitComponent(nComponent),
m_nWeaponUsedHash(nWeaponUsedHash),
m_bJumpedOutOfMovingCar(bJumpedOutOfMovingCar),
m_nDamageAggregationCount(nDamageAggregationCount)
{
}

CPedDamageCalculator::~CPedDamageCalculator()
{
}

////////////////////////////////////////////////////////////////////////////////

// Special Ammo tunables

// Gunrunning 
float CPedDamageCalculator::sm_fSpecialAmmoArmorPiercingDamageBonusPercentToApplyToArmor = 0.2f; // Was 0.1f
bool  CPedDamageCalculator::sm_bSpecialAmmoArmorPiercingIgnoresArmoredHelmets = true;
float CPedDamageCalculator::sm_fSpecialAmmoIncendiaryPedDamageMultiplier = 0.9f;
float CPedDamageCalculator::sm_fSpecialAmmoIncendiaryPlayerDamageMultiplier = 0.9f;
float CPedDamageCalculator::sm_fSpecialAmmoIncendiaryPedFireChanceOnHit = 0.2f;
float CPedDamageCalculator::sm_fSpecialAmmoIncendiaryPlayerFireChanceOnHit = 0.2f;
float CPedDamageCalculator::sm_fSpecialAmmoIncendiaryPedFireChanceOnKill = 0.4f;
float CPedDamageCalculator::sm_fSpecialAmmoIncendiaryPlayerFireChanceOnKill = 0.4f;
float CPedDamageCalculator::sm_fSpecialAmmoFMJPedDamageMultiplier = 0.9f;
float CPedDamageCalculator::sm_fSpecialAmmoFMJPlayerDamageMultiplier = 0.9f;
float CPedDamageCalculator::sm_fSpecialAmmoHollowPointPedDamageMultiplierForNoArmor = 1.75f; // Was 1.5f
float CPedDamageCalculator::sm_fSpecialAmmoHollowPointPlayerDamageMultiplierForNoArmor = 1.75f; // Was 1.5f
float CPedDamageCalculator::sm_fSpecialAmmoHollowPointPedDamageMultiplierForHavingArmor = 0.9f;
float CPedDamageCalculator::sm_fSpecialAmmoHollowPointPlayerDamageMultiplierForHavingArmor = 0.9f;

// Xmas17
bool CPedDamageCalculator::sm_bSpecialAmmoIncendiaryForceForWeaponTypes = true;
float CPedDamageCalculator::sm_fSpecialAmmoIncendiaryShotgunPedFireChanceOnHit = 0.083f; // 50% chance if all hit
float CPedDamageCalculator::sm_fSpecialAmmoIncendiaryShotgunPlayerFireChanceOnHit = 0.083f; // 50% chance if all hit
float CPedDamageCalculator::sm_fSpecialAmmoIncendiaryShotgunPedFireChanceOnKill = 0.083f; // 50% chance if all hit
float CPedDamageCalculator::sm_fSpecialAmmoIncendiaryShotgunPlayerFireChanceOnKill = 0.083f; // 50% chance if all hit

// Assault Course
float CPedDamageCalculator::sm_fSpecialAmmoHollowPointRevolverPedDamageMultiplierForNoArmor = 1.25f; // 200 Damage + 75% = Broken One-Shot Weapon...
float CPedDamageCalculator::sm_fSpecialAmmoHollowPointRevolverPlayerDamageMultiplierForNoArmor = 1.25f; // 200 Damage + 75% = Broken One-Shot Weapon...

//Endurance
float CPedDamageCalculator::sm_fFallEnduranceDamageMultiplier = 1.0f;
float CPedDamageCalculator::sm_fRammedByVehicleEnduranceDamageMultiplier = 1.0f;
float CPedDamageCalculator::sm_fRunOverByVehicleEnduranceDamageMultiplier = 1.0f;
float CPedDamageCalculator::sm_fVehicleCollidedWithEnvironmentEnduranceDamageMultiplier = 1.0f;

// Binomial cumulative distribution table for incendiary shotgun chances
//	1 Hit | 2 Hit | 3 Hit | 4 Hit | 5 Hit | 6 Hit | 7 Hit | 8 Hit 
//	1.3%													10.0%
//	2.8%													20.0%	
//	4.4%													30.0%	
//	6.2%													40.0%	
//	8.3%	15.9%	22.9%	29.2%	35.2%	40.5%	45.4%	50.0%	
//	10.8%	20.4%	29.0%	36.7%	43.5%	49.6%	55.7%	60.0%
//	14.0%	26.0%	36.4%	45.3%	53.0%	59.5%	65.2%	70.0%
//	18.2%													80.0%
//	25.0%													90.0%
//	43.8%													99.0%

void CPedDamageCalculator::InitTunables()
{
	sm_fSpecialAmmoArmorPiercingDamageBonusPercentToApplyToArmor	= ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("GUNRUN_AMMO_AP_DAMAGETOARMOR", 0xEF248153), sm_fSpecialAmmoArmorPiercingDamageBonusPercentToApplyToArmor);
	sm_bSpecialAmmoArmorPiercingIgnoresArmoredHelmets				= ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("GUNRUN_AMMO_AP_IGNOREHELMETS", 0xF26A165E), sm_bSpecialAmmoArmorPiercingIgnoresArmoredHelmets);				
	sm_fSpecialAmmoIncendiaryPedDamageMultiplier					= ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("GUNRUN_AMMO_INC_PED_DAMAGEMULT", 0xA269E8BF), sm_fSpecialAmmoIncendiaryPedDamageMultiplier);					
	sm_fSpecialAmmoIncendiaryPlayerDamageMultiplier					= ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("GUNRUN_AMMO_INC_PLYR_DAMAGEMULT", 0x98D4F32C), sm_fSpecialAmmoIncendiaryPlayerDamageMultiplier);			
	sm_fSpecialAmmoIncendiaryPedFireChanceOnHit						= ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("GUNRUN_AMMO_INC_PED_CHANCEONHIT", 0x7301CF8), sm_fSpecialAmmoIncendiaryPedFireChanceOnHit);					
	sm_fSpecialAmmoIncendiaryPlayerFireChanceOnHit					= ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("GUNRUN_AMMO_INC_PLYR_CHANCEONHIT", 0x7092EED5), sm_fSpecialAmmoIncendiaryPlayerFireChanceOnHit);					
	sm_fSpecialAmmoIncendiaryPedFireChanceOnKill					= ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("GUNRUN_AMMO_INC_PED_CHANCEONKILL", 0x388D63E4), sm_fSpecialAmmoIncendiaryPedFireChanceOnKill);					
	sm_fSpecialAmmoIncendiaryPlayerFireChanceOnKill					= ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("GUNRUN_AMMO_INC_PLAYER_CHANCEONKILL", 0x53ABB8B3), sm_fSpecialAmmoIncendiaryPlayerFireChanceOnKill);				
	sm_fSpecialAmmoFMJPedDamageMultiplier							= ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("GUNRUN_AMMO_FMJ_PED_DAMAGEMULT", 0x3162D25B), sm_fSpecialAmmoFMJPedDamageMultiplier);							
	sm_fSpecialAmmoFMJPlayerDamageMultiplier						= ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("GUNRUN_AMMO_FMJ_PLYR_DAMAGEMULT", 0xF5A3D505), sm_fSpecialAmmoFMJPlayerDamageMultiplier);						
	sm_fSpecialAmmoHollowPointPedDamageMultiplierForNoArmor			= ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("GUNRUN_AMMO_HP_PED_DAMAGEMULTNOARMOR", 0xFF9E2E5E), sm_fSpecialAmmoHollowPointPedDamageMultiplierForNoArmor);		
	sm_fSpecialAmmoHollowPointPlayerDamageMultiplierForNoArmor		= ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("GUNRUN_AMMO_HP_PLYR_DAMAGEMULTNOARMOR", 0x2D89F6D), sm_fSpecialAmmoHollowPointPlayerDamageMultiplierForNoArmor);		
	sm_fSpecialAmmoHollowPointPedDamageMultiplierForHavingArmor		= ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("GUNRUN_AMMO_HP_PED_DAMAGEMULTWITHARMOR", 0xBBC5BA21), sm_fSpecialAmmoHollowPointPedDamageMultiplierForHavingArmor);	
	sm_fSpecialAmmoHollowPointPlayerDamageMultiplierForHavingArmor	= ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("GUNRUN_AMMO_HP_PLYR_DAMAGEMULTWITHARMOR", 0xC4BC573B), sm_fSpecialAmmoHollowPointPlayerDamageMultiplierForHavingArmor);

	sm_bSpecialAmmoIncendiaryForceForWeaponTypes					= ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("XMAS17_AMMO_INC_FORCEFORWEAPONTYPES", 0x5490F372), sm_bSpecialAmmoIncendiaryForceForWeaponTypes);	
	sm_fSpecialAmmoIncendiaryShotgunPedFireChanceOnHit				= ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("XMAS17_AMMO_INC_SHGN_PED_CHANCEONHIT", 0xA21880EC), sm_fSpecialAmmoIncendiaryShotgunPedFireChanceOnHit);					
	sm_fSpecialAmmoIncendiaryShotgunPlayerFireChanceOnHit			= ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("XMAS17_AMMO_INC_SHGN_PLYR_CHANCEONHIT", 0x7BC3CBDF), sm_fSpecialAmmoIncendiaryShotgunPlayerFireChanceOnHit);					
	sm_fSpecialAmmoIncendiaryShotgunPedFireChanceOnKill				= ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("XMAS17_AMMO_INC_SHGN_PED_CHANCEONKILL", 0x7A7938C8), sm_fSpecialAmmoIncendiaryShotgunPedFireChanceOnKill);					
	sm_fSpecialAmmoIncendiaryShotgunPlayerFireChanceOnKill			= ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("XMAS17_AMMO_INC_SHGN_PLAYER_CHANCEONKILL", 0xF88DEC82), sm_fSpecialAmmoIncendiaryShotgunPlayerFireChanceOnKill);	

	sm_fSpecialAmmoHollowPointRevolverPedDamageMultiplierForNoArmor		= ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("ASSAULT_AMMO_HP_REV_PED_DAMAGEMULTNOARMOR", 0xA4A63FE8), sm_fSpecialAmmoHollowPointRevolverPedDamageMultiplierForNoArmor);
	sm_fSpecialAmmoHollowPointRevolverPlayerDamageMultiplierForNoArmor	= ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("ASSAULT_AMMO_HP_REV_PLYR_DAMAGEMULTNOARMOR", 0x79CD2731), sm_fSpecialAmmoHollowPointRevolverPlayerDamageMultiplierForNoArmor);

	sm_fFallEnduranceDamageMultiplier								= ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("CNC_ENDURANCE_DMG_MULTIPLIER_FALL", 0x6566BABA), sm_fFallEnduranceDamageMultiplier);
	sm_fRammedByVehicleEnduranceDamageMultiplier					= ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("CNC_ENDURANCE_DMG_MULTIPLIER_RAMMED", 0x1A57F442), sm_fRammedByVehicleEnduranceDamageMultiplier);
	sm_fRunOverByVehicleEnduranceDamageMultiplier					= ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("CNC_ENDURANCE_DMG_MULTIPLIER_RUN_OVER", 0x6E3253ED), sm_fRunOverByVehicleEnduranceDamageMultiplier);
	sm_fVehicleCollidedWithEnvironmentEnduranceDamageMultiplier		= ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("CNC_ENDURANCE_DMG_MULTIPLIER_ENVIRONMENT", 0x46B58492), sm_fVehicleCollidedWithEnvironmentEnduranceDamageMultiplier);
}

////////////////////////////////////////////////////////////////////////////////

void CPedDamageCalculator::SetComponentFromPedBoneTag(CPed* pVictim, eAnimBoneTag nBoneTag)
{
	m_nHitComponent = pVictim->GetRagdollComponentFromBoneTag(nBoneTag);
}

void CPedDamageCalculator::ApplyDamageAndComputeResponse(CPed* pVictim, CPedDamageResponse& damageResponse, const fwFlags32& flags, const u32 uParentMeleeActionResultID, const float fDist)
{
	CNetObjGame* pVictimNetworkObject = pVictim->GetNetworkObject();
	weaponDebugf3("CPedDamageCalculator::ApplyDamageAndComputeResponse pVictim[%p][%s] pVictim->GetHealth[%f] flags[%d] uParentMeleeActionResultID[%u] fDist[%f]", pVictim, pVictimNetworkObject ? pVictimNetworkObject->GetLogName() : "", pVictim->GetHealth(), (s32)flags, uParentMeleeActionResultID, fDist);

#if !__FINAL
	m_uRejectionReason = DRR_None;
#endif //__FINAL

	// don't want to do this twice
	if(damageResponse.m_bCalculated)
	{
		weaponDebugf3("(damageResponse.m_bCalculated)-->return");
		return;
	}

	//Initialise the damage response.
	damageResponse.ResetDamage();
	damageResponse.m_bCalculated = true;

	if((NetworkUtils::IsNetworkCloneOrMigrating(pVictim) && !damageResponse.m_bClonePedKillCheck) || PARAM_invincibleMigratingPeds.Get())
	{
		weaponDebugf3("(NetworkUtils::IsNetworkCloneOrMigrating(pVictim) && !damageResponse.m_bClonePedKillCheck)-->return");
		damageRejectionDebug2f(pVictim, m_pInflictor, m_nWeaponUsedHash, damageResponse, m_nHitComponent, DRR_MigratingClone);
		// allow the damage event to affect clone peds but not apply any damage
		damageResponse.m_bAffectsPed = true;
		return;
	}

	const bool bPedWasDeadOrDying = pVictim->IsFatallyInjured();

	if(m_nWeaponUsedHash == 0)
	{
		weaponDebugf3("(m_nWeaponUsedHash == 0)-->return");

		const CNetObjPhysical* netPhysical = SafeCast(CNetObjPhysical, pVictimNetworkObject);
		if (netPhysical && netPhysical->GetTriggerDamageEventForZeroWeaponHash())
		{
			UpdateDamageTrackers(pVictim, damageResponse, bPedWasDeadOrDying, flags);
		}

		return;
	}

	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_nWeaponUsedHash);
	if(!pWeaponInfo)
	{
		weaponDebugf3("(!pWeaponInfo)-->return");
		return;
	}

	// if special ability rage is active don't apply any damage
	if(pVictim->GetSpecialAbility() && pVictim->GetSpecialAbility()->IsActive() && pVictim->GetSpecialAbility()->GetType() == SAT_RAGE)
	{
		weaponDebugf3("(pVictim->GetSpecialAbility() && pVictim->GetSpecialAbility()->IsActive() && pVictim->GetSpecialAbility()->GetType() == SAT_RAGE)-->return");
		damageRejectionDebug2f(pVictim, m_pInflictor, m_nWeaponUsedHash, damageResponse, m_nHitComponent, DRR_RageAbility);
		return;
	}

	// if returns false then we're not accepting the event
	// i.e. this defines if the ped accepts the event to a certain extent
	// Ignore these flags if the ped is already injured

	bool bIgnoreFlags = pVictim->IsInjured();
	if (flags & CPedDamageCalculator::DF_IgnorePedFlags)
	{
		bIgnoreFlags = true;
	}

	if(!bIgnoreFlags && !AccountForPedFlags(pVictim, damageResponse, flags))
	{
		damageResponse.m_bAffectsPed = false;
		weaponDebugf3("(!bIgnoreFlags && !AccountForPedFlags(pVictim, damageResponse, flags))--damageResponse.m_bAffectsPed = false-->return");
		damageRejectionDebug2f(pVictim, m_pInflictor, m_nWeaponUsedHash, damageResponse, m_nHitComponent, m_uRejectionReason);
		
		if(NetworkInterface::ShouldTriggerDamageEventForZeroDamage(pVictim))
		{
			UpdateDamageTrackers(pVictim, damageResponse, bPedWasDeadOrDying, flags);
		}

		return;
	}
	else
	{
		damageResponse.m_bAffectsPed = true;
	}

	CPed* pAttacker = m_pInflictor && m_pInflictor->GetIsTypePed() ? static_cast<CPed*>(m_pInflictor.Get()) : NULL;

	m_eSpecialAmmoType = CAmmoInfo::None;
	if(!flags.IsFlagSet(DF_MeleeDamage) && pAttacker)
	{
		const CAmmoInfo* pAmmoInfo = pWeaponInfo->GetAmmoInfo(pAttacker);
		if (pAmmoInfo)
		{
			m_eSpecialAmmoType = pAmmoInfo->GetAmmoSpecialType();
		}
	}

	AccountForPedDamageStats(pVictim, damageResponse, flags, fDist);
	AccountForPedAmmoTypes(pVictim);
	AccountForPedArmourAndBlocking(pVictim, pWeaponInfo, damageResponse, flags);

	// if this damage has been received over the network ignore any modification of the damage and use the damage received in the network event (unless the
	// local player is invincible). The damage calculated remotely has already had all of these modifications applied.
	if ((flags & DF_UsePlayerPendingDamage) && m_fRawDamage != 0.0f && AssertVerify(pVictim->IsAPlayerPed()))
	{
		CNetObjPlayer* pPlayerObj = SafeCast(CNetObjPlayer, pVictimNetworkObject);

		if (pPlayerObj)
		{
			m_fRawDamage = (float)pPlayerObj->GetPendingDamage();
			pPlayerObj->SetPendingDamage(0);
			weaponDebugf3("DF_UsePlayerPendingDamage - m_fRawDamage[%f]", m_fRawDamage);
		}
	}

	ComputeWillKillPed(pVictim, pWeaponInfo, damageResponse, flags, uParentMeleeActionResultID);

#if !__FINAL
	pVictim->AddDamageRecord(CPed::sDamageRecord(m_pInflictor, fwTimer::GetTimeInMilliseconds(), m_nWeaponUsedHash, damageResponse.m_fHealthLost, damageResponse.m_fArmourLost, damageResponse.m_fEnduranceLost, pVictim->GetHealth(), m_nHitComponent, damageResponse.m_bForceInstantKill, damageResponse.m_bInvincible, m_uRejectionReason));
#endif // !__FINAL

	if (pVictim && pAttacker && (damageResponse.m_fHealthLost > 0.0f))
	{
		pAttacker->SetPedResetFlag(CPED_RESET_FLAG_InflictedDamageThisFrame, true);
	}

#if !__FINAL
	if(pVictim && pVictim->IsLocalPlayer() && damageResponse.m_bKilled && CPlayerInfo::IsPlayerInvincibleRestoreHealth())
	{
		pVictim->SetHealth(pVictim->GetMaxHealth());

		if (CPlayerInfo::IsPlayerInvincibleRestoreArmorWithHealth())
		{
			pVictim->SetArmour(pVictim->GetPlayerInfo()->MaxArmour);
		}
		// Clear all melee flags
		pVictim->SetPedConfigFlag( CPED_CONFIG_FLAG_KilledByStealth, false );
		pVictim->SetPedConfigFlag( CPED_CONFIG_FLAG_KilledByTakedown, false );
		pVictim->SetPedConfigFlag( CPED_CONFIG_FLAG_Knockedout, false );
		pVictim->SetPedConfigFlag( CPED_CONFIG_FLAG_KilledByStandardMelee, false );
		// Unflag as dead
		damageResponse.m_bKilled = false;
		damageResponse.m_bInjured = false;
	}
#endif // !__FINAL

	//If the ped is the player and he drops below 10 health (110 really) he get pissed off (Grr).
	//if(pVictim==FindPlayerPed())
	//{
//#define HEALTH_BELOW_WHICH_PISSED_OFF	(110.0f)
//		if (pVictim->GetHealth() < HEALTH_BELOW_WHICH_PISSED_OFF && (pVictim->GetHealth()+damageResponse.m_fHealthLost) > HEALTH_BELOW_WHICH_PISSED_OFF )
//		{
//			CGameWorld::GetMainPlayerInfo()->SetPlayerMoodPissedOff(150);		//2.5 minutes of being pissed off.
//		}
//	}

	// start synchronising this ped so that its new health state is broadcast
	if (damageResponse.m_bAffectsPed && !pVictim->IsNetworkClone() && pVictim->GetNetworkObject() && !pVictim->GetNetworkObject()->GetSyncData() && pVictim->GetNetworkObject()->CanSynchronise(false))
	{
		pVictim->GetNetworkObject()->StartSynchronising();
	}

	//if I am mounted and killed eject my rider (for now)
	if (damageResponse.m_bKilled && pVictim->GetSeatManager() && pVictim->GetSeatManager()->GetDriver())
	{
		CPed* pDriver = pVictim->GetSeatManager()->GetDriver();
		pDriver->SetPedOffMount();

		// Run an appropriate NM behavior
		if (CTaskNMBehaviour::CanUseRagdoll(pDriver, RAGDOLL_TRIGGER_EXPLOSION, NULL, 100.0f))
		{
			pDriver->GetRagdollInst()->SetDontZeroMatricesOnActivation();

			nmEntityDebugf(pDriver, "CPedDamageCalculator::ApplyDamageAndComputeResponse - Adding CTaskNMExplosion");
			aiTask* pTaskNM = rage_new CTaskNMExplosion(3000, 6000, VEC3V_TO_VECTOR3(pVictim->GetTransform().GetPosition()));
			CEventSwitch2NM event(4000, pTaskNM);
			pDriver->SwitchToRagdoll(event);
		}
	}

	// Incendiary ammo fire chance (was previously on impact like flaming bullet cheat, but we need to do this after damage calculations to check for kill shot)
	if (pAttacker && m_eSpecialAmmoType == CAmmoInfo::Incendiary && CFireManager::CanSetPedOnFire(pVictim) && !g_fireMan.IsEntityOnFire(pVictim))
	{
		float fChanceToSetAlight = 0.0f;
		bool bIsKillShot = damageResponse.m_bKilled && !bPedWasDeadOrDying;

		if (sm_bSpecialAmmoIncendiaryForceForWeaponTypes && pWeaponInfo->GetIncendiaryGuaranteedChance())
		{
			fChanceToSetAlight = 1.0f;
		}
		else if (pWeaponInfo->GetGroup() == WEAPONGROUP_SHOTGUN) // Reduced chances, as this gets rolled on each pellet
		{
			if (pVictim->IsPlayer())
			{
				fChanceToSetAlight = bIsKillShot ? sm_fSpecialAmmoIncendiaryShotgunPlayerFireChanceOnKill : sm_fSpecialAmmoIncendiaryShotgunPlayerFireChanceOnHit;
			}
			else
			{
				fChanceToSetAlight = bIsKillShot ? sm_fSpecialAmmoIncendiaryShotgunPedFireChanceOnKill : sm_fSpecialAmmoIncendiaryShotgunPedFireChanceOnHit;
			}	
		}
		else
		{
			if (pVictim->IsPlayer())
			{
				fChanceToSetAlight = bIsKillShot ? sm_fSpecialAmmoIncendiaryPlayerFireChanceOnKill : sm_fSpecialAmmoIncendiaryPlayerFireChanceOnHit;
			}
			else
			{
				fChanceToSetAlight = bIsKillShot ? sm_fSpecialAmmoIncendiaryPedFireChanceOnKill : sm_fSpecialAmmoIncendiaryPedFireChanceOnHit;
			}	
		}

		// Network games batch up fire messages from the same weapon within a 100ms or 200ms period, and send a single WeaponDamageEvent sent over the network with an override damage value. 
		// We don't know exactly how many pellets hit with shotgun weapons, so the best we can do is a rough estimation...
		u32 iNumIncendiaryRolls = 1;

		const u32 uPelletsInShot = pWeaponInfo->GetBulletsInBatch();
		if (pAttacker && pAttacker->IsNetworkClone() && uPelletsInShot > 1)
		{
			// Work out a rough number of what damage each shot *should* be after several basic multipliers
			float fEstimatedDamagePerShot = pWeaponInfo->GetDamage();
			fEstimatedDamagePerShot *= pAttacker->IsPlayer() ? pAttacker->GetPlayerInfo()->GetPlayerWeaponDamageModifier() : 1.0f; // x0.72
			fEstimatedDamagePerShot *= pVictim->IsPlayer() ? sm_fSpecialAmmoIncendiaryPlayerDamageMultiplier : sm_fSpecialAmmoIncendiaryPedDamageMultiplier; // x0.90
			fEstimatedDamagePerShot = Floorf(fEstimatedDamagePerShot); // Network damage uses int, not float

			// Work out how many rolls we should do and clamp to reasonable limits
			iNumIncendiaryRolls = (u32)(ceil(m_fRawDamage / fEstimatedDamagePerShot));
			iNumIncendiaryRolls = Clamp(iNumIncendiaryRolls, (u32)1, uPelletsInShot);

			weaponDebugf1("Incendiary Ammo - Frame[%u] Victim[%p][%s] - Multi-shot batching, doing %u rolls (m_fRawDamage[%.2f] fEstimatedDamagePerShot[%.2f])", fwTimer::GetFrameCount(), pVictim, pVictim && pVictim->GetNetworkObject() ? pVictim->GetNetworkObject()->GetLogName() : "", iNumIncendiaryRolls, m_fRawDamage, fEstimatedDamagePerShot);
		}

		for (u32 i = 0; i < iNumIncendiaryRolls; i++)
		{
			float fRandom = fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
			if (m_pInflictor && m_pInflictor->GetIsTypePed() && fRandom <= fChanceToSetAlight)
			{			
				CPed* pAttacker = static_cast<CPed*>(m_pInflictor.Get());
				if(g_fireMan.StartPedFire(pVictim, pAttacker, FIRE_DEFAULT_NUM_GENERATIONS, Vec3V(V_ZERO), BANK_ONLY(Vec3V(V_ZERO),) false, false, m_nWeaponUsedHash))
				{
					weaponDebugf1("Incendiary Ammo - Frame[%u] pVictim[%p][%s] bIsKillShot[%s] (fRandom[%.3f] <= fChanceToSetAlight[%.3f]) - Roll Success!", fwTimer::GetFrameCount(), pVictim, pVictim && pVictim->GetNetworkObject() ? pVictim->GetNetworkObject()->GetLogName() : "", bIsKillShot ? "TRUE" : "FALSE", fRandom, fChanceToSetAlight);
					pVictim->GetPedAudioEntity()->OnIncendiaryAmmoFireStarted();
				}
				else
				{
					weaponDebugf1("Incendiary Ammo - Frame[%u] pVictim[%p][%s] bIsKillShot[%s] (fRandom[%.3f] <= fChanceToSetAlight[%.3f]) - Roll Success! (but was unable to start ped fire)", fwTimer::GetFrameCount(), pVictim, pVictim && pVictim->GetNetworkObject() ? pVictim->GetNetworkObject()->GetLogName() : "", bIsKillShot ? "TRUE" : "FALSE", fRandom, fChanceToSetAlight);
				}
			}
			else
			{
				weaponDebugf1("Incendiary Ammo - Frame[%u] pVictim[%p][%s] bIsKillShot[%s] (fRandom[%.3f] <= fChanceToSetAlight[%.3f]) - Roll Failed!", fwTimer::GetFrameCount(), pVictim, pVictim && pVictim->GetNetworkObject() ? pVictim->GetNetworkObject()->GetLogName() : "", bIsKillShot ? "TRUE" : "FALSE", fRandom, fChanceToSetAlight);
			}
		}
	}

	bool bIsPlayer = pVictim->IsPlayer();
	const u32 iCurrentTime = fwTimer::GetTimeInMilliseconds();
	// Apply shake
	const float fRumbleDamage = flags.IsFlagSet(CPedDamageCalculator::DF_EnduranceDamageOnly) ? damageResponse.m_fEnduranceLost : damageResponse.m_fHealthLost;
	ApplyPadShake(pVictim, pVictim->IsLocalPlayer(), fRumbleDamage*pWeaponInfo->GetRumbleDamageIntensity() + damageResponse.m_fArmourLost);

#if FPS_MODE_SUPPORTED
	if( camInterface::GetGameplayDirector().GetFirstPersonShooterCamera() )
	{
		if( pVictim && (pWeaponInfo->GetDamageType() == DAMAGE_TYPE_BULLET || pWeaponInfo->GetDamageType() == DAMAGE_TYPE_BULLET_RUBBER))
		{
			// Trigger camera shake when player is hit by melee attack in first person camera.
			// TODO: set using metadata?  Directional shakes?
			CPed *pAttacker = m_pInflictor && m_pInflictor->GetIsTypePed() ? static_cast<CPed*>(m_pInflictor.Get()) : NULL;
			camInterface::GetGameplayDirector().GetFirstPersonShooterCamera()->OnBulletImpact(pAttacker, pVictim);
		}

		if (m_pInflictor && pVictim && pVictim->IsLocalPlayer() && pWeaponInfo && pWeaponInfo->GetDamageType() == DAMAGE_TYPE_EXPLOSIVE)
		{			
			Vector3 damagePos = VEC3V_TO_VECTOR3(m_pInflictor->GetTransform().GetPosition());
			PostFX::RegisterBulletImpact(damagePos, 1.0f);
		}
	}
#endif // FPS_MODE_SUPPORTED

	bool giveAbilityChargeForKill = false;
	if(bIsPlayer)
	{
		pVictim->GetPlayerInfo()->LastTimeEnergyLost = iCurrentTime;

		// don't let peds take credit for damage dealt to a player by the player landing on them from a height
		if(pWeaponInfo && pWeaponInfo->GetDamageType() == DAMAGE_TYPE_FALL && m_pInflictor && m_pInflictor->GetIsTypePed())
		{
			m_pInflictor = NULL;
		}

		// Audio - Cop has killed a suspect he was pursuing (don't need to check if its a cop)
		if( m_pInflictor && m_pInflictor->GetIsTypePed() && !bPedWasDeadOrDying && pVictim->IsInjured() )
		{
			CPed* pPedInflictor = static_cast<CPed*>(m_pInflictor.Get());
			if( pPedInflictor->GetPedType() == PEDTYPE_COP  && pPedInflictor->GetSpeechAudioEntity())
				pPedInflictor->GetSpeechAudioEntity()->SayWhenSafe("KILLED_SUSPECT", "SPEECH_PARAMS_FORCE");
		}

		if (pWeaponInfo->GetDamageType() == DAMAGE_TYPE_BULLET || pWeaponInfo->GetDamageType() == DAMAGE_TYPE_BULLET_RUBBER)
			CPlayerSpecialAbilityManager::ChargeEvent(ACET_BULLET_DAMAGE, pVictim);
	}
	else
	{
		giveAbilityChargeForKill = damageResponse.m_bKilled && m_pInflictor && m_pInflictor->GetIsTypePed();
	}

	//Update damage tracking for the victim.
	UpdateDamageTrackers(pVictim, damageResponse, bPedWasDeadOrDying, flags);

	if (giveAbilityChargeForKill && !bPedWasDeadOrDying)
	{
		CPed* inflictor = static_cast<CPed*>(m_pInflictor.Get());

		if (pWeaponInfo->GetDamageType() == DAMAGE_TYPE_EXPLOSIVE)
			CPlayerSpecialAbilityManager::ChargeEvent(ACET_EXPLOSION_KILL, inflictor);
		else if (pVictim->GetPedConfigFlag(CPED_CONFIG_FLAG_KilledByStealth))
			CPlayerSpecialAbilityManager::ChargeEvent(ACET_STEALTH_KILL, inflictor);
		else if (pVictim->GetPedConfigFlag(CPED_CONFIG_FLAG_Knockedout))
			CPlayerSpecialAbilityManager::ChargeEvent(ACET_KNOCKOUT, inflictor);

		CPlayerSpecialAbilityManager::ChargeEvent(ACET_KILL, inflictor);
	}

	// Determine if we should call for help
	bool bDoCryForHelp = false;
	CPed* pAttackingPed = NULL;

	static float DAMAGE_THRESHOLD = 3.0f;
	// There are three main things that cause injured ped events:  falling (but not being bumped), being hit by vehicles, and being hit by stunguns/melee.
	// Stunguns (and melee) don't generate the same shocking events as most other weapons, but we still want peds to react to them being fired in some way.
	// This is handled by the CEventShockingInjuredPed event.
	if ((((m_nWeaponUsedHash==WEAPONTYPE_FALL)||m_nWeaponUsedHash==WEAPONTYPE_RAMMEDBYVEHICLE||m_nWeaponUsedHash==WEAPONTYPE_RUNOVERBYVEHICLE) && m_fRawDamage>DAMAGE_THRESHOLD) || 
		(pWeaponInfo->GetIsGun() && pWeaponInfo->GetIsNonLethal()) || (pWeaponInfo->GetIsMelee() && m_fRawDamage>DAMAGE_THRESHOLD))
	{
		// Check the source of the damage event.  CBuilding objects can have a transform matrix centered at the origin, and as such
		// we will set the source of the event to NULL below.  This way, anyone who responds to the damage will react to the position of the event
		// instead of the origin.
		bool bUseInflictorAsSource = true;
		if (m_pInflictor && m_pInflictor->GetIsTypeBuilding())
		{
			bUseInflictorAsSource = false;
		}

		//  Add in a shocking event for the ped being injured.
		wantedDebugf3("CPedDamageCalculator::ApplyDamageAndComputeResponse--pWeaponInfo[%p][%s] bUseInflictorAsSource[%d] m_pInflictor[%p] --> CEventShockingInjuredPed",pWeaponInfo,pWeaponInfo ? pWeaponInfo->GetModelName() : "",bUseInflictorAsSource,m_pInflictor.Get());
		CEventShockingInjuredPed ev(*pVictim, bUseInflictorAsSource ? m_pInflictor.Get() : NULL);

		// Make this event less noticeable based on the pedtype.
		float fKilledPerceptionModifier = pVictim->GetPedModelInfo()->GetKilledPerceptionRangeModifer();
		if (fKilledPerceptionModifier >= 0.0f)
		{
			ev.SetVisualReactionRangeOverride(fKilledPerceptionModifier);
			ev.SetAudioReactionRangeOverride(fKilledPerceptionModifier);
		}

		CShockingEventsManager::Add(ev);

		if (m_nWeaponUsedHash == WEAPONTYPE_RAMMEDBYVEHICLE || m_nWeaponUsedHash == WEAPONTYPE_RUNOVERBYVEHICLE) {
			if (m_pInflictor && pVictim && m_pInflictor.Get()->GetIsTypeVehicle()) {
				CVehicle* pVehicle = static_cast<CVehicle*>(m_pInflictor.Get());
				pAttackingPed = pVehicle->GetDriver();
				if (pAttackingPed) {
					bDoCryForHelp = true;
				}
			}
		}
		// When hit by a stungun, the ped should still call out for help from friends
		else if (pWeaponInfo->GetIsGun() && pWeaponInfo->GetIsNonLethal() && pWeaponInfo->GetDamageType() != DAMAGE_TYPE_TRANQUILIZER)
		{
			if (m_pInflictor && m_pInflictor.Get()->GetIsTypePed())
			{
				pAttackingPed = static_cast<CPed*>(m_pInflictor.Get());
				bDoCryForHelp = true;
			}
		}
		else if(flags.IsFlagSet(DF_MeleeDamage) && m_fRawDamage > DAMAGE_THRESHOLD)
		{
			// add in a shocking event for melee damage attacks
			if (m_pInflictor && pVictim && m_pInflictor.Get()->GetIsTypePed())
			{
				pAttackingPed = static_cast<CPed*>(m_pInflictor.Get());
				if (pAttackingPed != pVictim && !pVictim->IsAPlayerPed())
				{
					bDoCryForHelp = true;
				}
			}
		}
	}

	// apply ped damage packs for big damage
	if( pVictim->IsPlayer() )
	{
		const float FALL_VELOCITY_THRESHOLD = 12.5f; // 8m fall
		const float JUMPED_OUT_OF_CAR_VELOCITY_THRESHOLD = 30.0f; // crazy velocity
		const float HIT_BY_VEHICLE_DAMAGE_THRESHOLD = 250.0f; // high velocity, med mass
		const float BIG_HIT_BY_VEHICLE_DAMAGE_THRESHOLD = 500.0f; // crazy velocity, med mass
		const float RUN_OVER_BY_VEHICLE_DAMAGE_THRESHOLD = 250.0f; 
		const float BIG_RUN_OVER_BY_VEHICLE_DAMAGE_THRESHOLD = 500.0f;
		const float EXPLOSION_DAMAGE_THRESHOLD = 100.0f;
		const float EXPLOSION_DISTANCE_UPGRADE_PCT = 0.1f;

		atHashString hashForDamagePack;
		bool addCustomEnv = false;
		if(m_nWeaponUsedHash==WEAPONTYPE_FALL)
		{ 
			// for these, damage isn't a great indicator of...um...damage. Using velocity instead
			float velsq = pVictim->GetVelocity().Mag2();
			if ( velsq > JUMPED_OUT_OF_CAR_VELOCITY_THRESHOLD * JUMPED_OUT_OF_CAR_VELOCITY_THRESHOLD && 
				 (pVictim->GetIsDrivingVehicle() || pVictim->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_NM_JUMP_ROLL_FROM_ROAD_VEHICLE)) )
			{
				if( damageResponse.m_bKilled )
				{
					hashForDamagePack = ATSTRINGHASH("Fall",0xaa4896aa);
					addCustomEnv = true;
				}
				else
				{
					hashForDamagePack = ATSTRINGHASH("Fall_Low",0x93f2fd33);
				}
			}
			else if( velsq > FALL_VELOCITY_THRESHOLD * FALL_VELOCITY_THRESHOLD && m_fRawDamage > 5.0f && !pVictim->GetIsInWater() )
			{
				if( damageResponse.m_bKilled )
				{
					hashForDamagePack = ATSTRINGHASH("Fall",0xaa4896aa);
					addCustomEnv = true;
				}
				else
				{
					hashForDamagePack = ATSTRINGHASH("Fall_Low",0x93f2fd33);
				}
			}
		}
		else if(m_nWeaponUsedHash == WEAPONTYPE_RAMMEDBYVEHICLE)
		{
			if(pVictim->GetIsDrivingVehicle())
			{
				if( damageResponse.m_bKilled )
				{
					hashForDamagePack = ATSTRINGHASH("Car_Crash_Heavy",0xb8c306b0);
					addCustomEnv = true;
				}
				else
				{
					hashForDamagePack = ATSTRINGHASH("Car_Crash_Light",0xe69f283c);
				}
			}
			else if(m_fRawDamage > BIG_HIT_BY_VEHICLE_DAMAGE_THRESHOLD)
			{
				hashForDamagePack = ATSTRINGHASH("BigHitByVehicle",0x8f6f9549);
				addCustomEnv = damageResponse.m_bKilled;
			}
			else if( m_fRawDamage > HIT_BY_VEHICLE_DAMAGE_THRESHOLD)
			{
				hashForDamagePack = ATSTRINGHASH("HitByVehicle",0x86b0a573);
			}
		}
		else if(m_nWeaponUsedHash == WEAPONTYPE_RUNOVERBYVEHICLE)
		{
			if(m_fRawDamage > BIG_RUN_OVER_BY_VEHICLE_DAMAGE_THRESHOLD)
			{
				hashForDamagePack = ATSTRINGHASH("BigRunOverByVehicle",0x72abe68c);
				addCustomEnv = damageResponse.m_bKilled;
			}
			else if( m_fRawDamage > RUN_OVER_BY_VEHICLE_DAMAGE_THRESHOLD)
			{
				hashForDamagePack = ATSTRINGHASH("RunOverByVehicle",0x69fad7b);
			}
		}
		else if(pWeaponInfo->GetDamageType() == DAMAGE_TYPE_EXPLOSIVE)
		{
			if(m_fRawDamage > EXPLOSION_DAMAGE_THRESHOLD)
			{
				Vector3 vpos = VEC3V_TO_VECTOR3(pVictim->GetTransform().GetPosition());
				float radius = pWeaponInfo->GetDamageFallOffRangeMax()*EXPLOSION_DISTANCE_UPGRADE_PCT;
				// test if the explosion is in our threshold range between med/large
				if( CExplosionManager::TestForExplosionsInSphere(pWeaponInfo->GetExplosionTag(), vpos, radius) )
				{
					hashForDamagePack = ATSTRINGHASH("Explosion_Large",0x94457098);
				}
				else
				{
					hashForDamagePack = ATSTRINGHASH("Explosion_Med",0x8c3265be);
				}
				addCustomEnv = damageResponse.m_bKilled;
			}
		}
		else if(pAttackingPed && pAttackingPed->GetPedType() == PEDTYPE_ANIMAL)
		{
			if( damageResponse.m_bKilled )
			{
				hashForDamagePack = ATSTRINGHASH("SCR_Cougar",0x3f0d5853);
			}
		}
		// now apply the pack, if we met the damage/type criteria
		if( hashForDamagePack.IsNotNull() )
		{
			PEDDAMAGEMANAGER.AddPedDamagePack(pVictim, hashForDamagePack, 0.0f, 1.0f);

			// Check player ped is in first person view
			if (hashForDamagePack == ATSTRINGHASH("Fall",0xaa4896aa) && camInterface::IsRenderingFirstPersonShooterCamera())
			{
				Vector3 pedPos = VEC3V_TO_VECTOR3(pVictim->GetTransform().GetPosition());

				Vector3 damagePos = pedPos + Vector3(0.0f, 0.0f, -1.0f);
				PostFX::RegisterBulletImpact(damagePos, 1.0f);

				damagePos = pedPos + Vector3(-1.0f, 0.0f, -1.0f);
				PostFX::RegisterBulletImpact(damagePos, 1.0f);

				damagePos = pedPos + Vector3(1.0f, 0.0f, -1.0f);
				PostFX::RegisterBulletImpact(damagePos, 1.0f);
			}
		}
		if( addCustomEnv )
		{
			pVictim->SetCustomDirtScale(1.0f);
		}
	} 
	const bool bShouldBlockFroStunGun = pVictim->GetPedConfigFlag(CPED_CONFIG_FLAG_DontCryForHelpOnStun) && m_nWeaponUsedHash == WEAPONTYPE_STUNGUN;
	if (bDoCryForHelp && pAttackingPed != NULL && !pVictim->GetPedConfigFlag(CPED_CONFIG_FLAG_DisableInjuredCryForHelpEvents) && !bShouldBlockFroStunGun)
	{
		CEventInjuredCryForHelp ev(pVictim, pAttackingPed);
		ev.CEvent::CommunicateEvent(pVictim, false);
	}

	bool isPlayer = pVictim && pVictim->IsPlayer();
	// Do appropriate audio things - stop speech, play pain dialogue, do body crunching sfx
	if (isPlayer && m_nWeaponUsedHash==WEAPONTYPE_FALL && damageResponse.m_bKilled)
	{
		//We handle falling pain sound triggers elsewhere, but if this is the player and they are dying of falling, we need to kick off
		//	some police scanner "suicide" speech
		if(pVictim->GetPlayerWanted() && pVictim->GetPlayerWanted()->GetWantedLevel() != WANTED_CLEAN && pVictim->GetSpeechAudioEntity() && 
			g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0) - pVictim->GetSpeechAudioEntity()->GetLastTimeFalling() < 100)
				g_PoliceScanner.ReportPlayerCrime(VEC3V_TO_VECTOR3(pVictim->GetMatrix().d()), CRIME_SUICIDE);
	}
	else if (m_fRawDamage > 0.0f && m_nWeaponUsedHash!=WEAPONTYPE_BLEEDING)
	{
		eAnimBoneTag nHitBoneTag = BONETAG_ROOT;
		if(m_nHitComponent > -1)
			nHitBoneTag = pVictim->GetBoneTagFromRagdollComponent(m_nHitComponent);

		audDamageStats damageStats;
		damageStats.RawDamage = m_fRawDamage;
		damageStats.Fatal = damageResponse.m_bKilled;
		damageStats.PedWasAlreadyDead = pVictim->ShouldBeDead() && pVictim->GetDeathTime() != 0 && fwTimer::GetTimeInMilliseconds() > pVictim->GetDeathTime();
		damageStats.Inflictor = m_pInflictor ? m_pInflictor.Get() : NULL;
		damageStats.IsHeadshot = CPed::IsHeadShot(nHitBoneTag) && pWeaponInfo->GetWeaponWheelSlot() != WEAPON_WHEEL_SLOT_SHOTGUN;
		damageStats.IsSilenced = pWeaponInfo->GetIsSilenced();
		damageStats.IsSniper = pWeaponInfo->GetWeaponWheelSlot() == WEAPON_WHEEL_SLOT_SNIPER;
		
		//Play gargle if it's a neck stab
		damageStats.PlayGargle = nHitBoneTag==BONETAG_NECK && pWeaponInfo &&
						(pWeaponInfo->GetEffectGroup() == WEAPON_EFFECT_GROUP_MELEE_METAL || pWeaponInfo->GetEffectGroup() == WEAPON_EFFECT_GROUP_MELEE_SHARP);

		if (m_nWeaponUsedHash == WEAPONTYPE_DROWNING || m_nWeaponUsedHash == WEAPONTYPE_DROWNINGINVEHICLE)
		{
			if(pVictim->GetVehiclePedInside())
			{
				f32 waterZ;
				const Vector3 pedHeadPos = pVictim->GetBonePositionCached(BONETAG_HEAD);
				Water::GetWaterLevel(pedHeadPos, &waterZ, false, POOL_DEPTH, REJECTIONABOVEWATER, NULL);
				if(pedHeadPos.z < waterZ)
				{
					damageStats.DamageReason = AUD_DAMAGE_REASON_DROWNING;
				}
			}
			else if (pVictim->m_Buoyancy.GetStatus() == FULLY_IN_WATER)
			{
				damageStats.DamageReason = AUD_DAMAGE_REASON_DROWNING;
			}
			else
			{
				damageStats.DamageReason = AUD_DAMAGE_REASON_SURFACE_DROWNING;
			}
		}
		else if(m_nWeaponUsedHash == WEAPONTYPE_EXHAUSTION)
			damageStats.DamageReason = AUD_DAMAGE_REASON_EXHAUSTION;
		else if(pWeaponInfo)
		{
			if(pWeaponInfo->GetDamageType() == DAMAGE_TYPE_EXPLOSIVE)
				damageStats.DamageReason = AUD_DAMAGE_REASON_EXPLOSION;
			else if(pWeaponInfo->GetDamageType() == DAMAGE_TYPE_MELEE)
				damageStats.DamageReason = AUD_DAMAGE_REASON_MELEE;
			else if(pWeaponInfo->GetDamageType() == DAMAGE_TYPE_ELECTRIC)
				damageStats.DamageReason = isPlayer ? AUD_DAMAGE_REASON_DEFAULT : AUD_DAMAGE_REASON_TAZER;
			else if(pWeaponInfo->GetDamageType() == DAMAGE_TYPE_SMOKE)
			{
				damageStats.DamageReason = AUD_DAMAGE_REASON_COUGH;

				// We want to know when we first inflicted lung damage, so we know how to ramp down the coughing as time passes.
				// Otherwise the CPedHealthScanner will keep kicking off cough events forever since the lungs are permanently damaged.
				if(pVictim->GetSpeechAudioEntity())
				{
					pVictim->GetSpeechAudioEntity()->RegisterLungDamage();
				}

				// Play cough facial
				pVictim->SetPedResetFlag(CPED_RESET_FLAG_DoDamageCoughFacial, true);
			}
		}

		//Yanking this for now, as we don't have assets that match this.  
		//Possible TODO - Replace with similar V-specific system

		// Do SHOT_IN_LEG first - otherwise it'll never play, because pain will instead
		/*bool playedShotInLeg = false;
		if (pWeaponInfo->GetDamageType() == DAMAGE_TYPE_BULLET ||
            (
             (pWeaponInfo->GetUseLegDamageVoice()) && 
             pVictim->GetPedType() == PEDTYPE_COP && 
             (fwRandom::GetRandomNumberInRange(0,15) == 15)
            )
           )
		{
			switch(nHitBoneTag)
			{
			case BONETAG_R_THIGH:
			case BONETAG_R_CALF:
			case BONETAG_L_THIGH:
			case BONETAG_L_CALF:
				playedShotInLeg = pVictim->NewSay("SHOT_IN_LEG", 0, false, false);
				break;

			default:
				break;
			}
		}*/

		// If we've not played a scream in a while, force one - it means the first time you start causing carnage, you're guaranteed to 
		// get a dramatic result. Only do it for the player shooting.
		/*if (!playedShotInLeg && pVictim->GetSpeechAudioEntity())
		{
			if (audSpeechAudioEntity::sm_TimeToNextPlayShotScream<fwTimer::GetTimeInMilliseconds() && m_pInflictor &&
				m_pInflictor->GetIsTypePed() && ((CPed*)m_pInflictor.Get())->IsLocalPlayer() && !pVictim->IsFatallyInjured() &&
				pWeaponInfo->GetDamageType() == DAMAGE_TYPE_BULLET)
			{
				audSpeechInitParams speechParams;
				speechParams.forcePlay = true;
				speechParams.allowRecentRepeat = true;
				pVictim->GetSpeechAudioEntity()->Say("PANIC_SHORT", speechParams, ATSTRINGHASH("PAIN_VOICE", 0x48571D8D));
				audSpeechAudioEntity::sm_TimeToNextPlayShotScream = fwTimer::GetTimeInMilliseconds() + 7500;
			}
			else
			{
				pVictim->GetSpeechAudioEntity()->InflictPain(damageStats);
			}
		}*/

		if(m_pInflictor && pVictim && pVictim->IsLocalPlayer() && damageResponse.m_bKilled && 
			(pWeaponInfo->GetDamageType() == DAMAGE_TYPE_BULLET || pWeaponInfo->GetDamageType() == DAMAGE_TYPE_BULLET_RUBBER) && 
			m_pInflictor->GetIsTypePed() && !((CPed*)m_pInflictor.Get())->IsLocalPlayer() && 
			 !(flags.IsFlagSet(DF_MeleeDamage)) &&
			 !( m_nWeaponUsedHash == WEAPONTYPE_FALL || 
				m_nWeaponUsedHash == WEAPONTYPE_FIRE || 
				m_nWeaponUsedHash == WEAPONTYPE_EXPLOSION || 
				m_nWeaponUsedHash == WEAPONTYPE_DROWNING || 
				m_nWeaponUsedHash == WEAPONTYPE_DROWNINGINVEHICLE || 
				m_nWeaponUsedHash == WEAPONTYPE_EXHAUSTION || 
				m_nWeaponUsedHash == WEAPONTYPE_RAMMEDBYVEHICLE || 
				m_nWeaponUsedHash == WEAPONTYPE_RUNOVERBYVEHICLE || 
				m_nWeaponUsedHash == WEAPONTYPE_BLEEDING || 
				m_nWeaponUsedHash == WEAPONTYPE_ROTORS || 
				m_nWeaponUsedHash == WEAPONTYPE_EXHAUSTION || 
				m_nWeaponUsedHash == WEAPONTYPE_ELECTRICFENCE || 
				m_nWeaponUsedHash == ATSTRINGHASH("WEAPON_MOLOTOV", 0x24B17070) || 
				m_nWeaponUsedHash == ATSTRINGHASH("WEAPON_FLARE", 0x497facc3)))
		{		
			CWeapon* pWeapon = ((CPed*)m_pInflictor.Get())->GetWeaponManager()->GetEquippedWeapon();
			if(pWeapon && pVictim->GetPedAudioEntity() && !pVictim->GetPedAudioEntity()->GetHasPlayedKillShotAudio())
			{
				pVictim->GetPedAudioEntity()->SetHasPlayedKillShotAudio(true);
				pWeapon->DoWeaponFireKillShotAudio(m_pInflictor, damageResponse.m_bKilled);
			}
		}
		if (pVictim->GetSpeechAudioEntity())
			pVictim->GetSpeechAudioEntity()->InflictPain(damageStats);
	}

	if(bIsPlayer && pWeaponInfo->GetDamageType() == DAMAGE_TYPE_WATER_CANNON && m_pInflictor && !CNetworkGhostCollisions::IsLocalPlayerInGhostCollision(*(CPhysical*)m_pInflictor.Get()))
	{
		pVictim->GetPlayerInfo()->RegisterHitByWaterCannon();
	}

	if (pVictim && pVictim->ShouldUseInjuredMovement())
	{
		pVictim->SetPedConfigFlag(CPED_CONFIG_FLAG_IsInjured, true);
	}

#if __BANK
	if( bIsPlayer && damageResponse.m_bKilled && !bPedWasDeadOrDying )
	{
		CTaskCombat::ms_EnemyAccuracyLog.RecordLocalPlayerDeath();
	}
#endif // __BANK

#if FPS_MODE_SUPPORTED
	camFirstPersonShooterCamera* pFpsCamera = camInterface::GetGameplayDirector().GetFirstPersonShooterCamera( pVictim );
	if(pFpsCamera)
	{
		if( damageResponse.m_bKilled && !bPedWasDeadOrDying )
		{
			pFpsCamera->OnDeath( pVictim );
		}
	}
#endif

	// Check for network game and inflictor ped
	if( NetworkInterface::IsGameInProgress() && m_pInflictor && m_pInflictor->GetIsTypePed() )
	{
		CPed* pInflictorPed = static_cast<CPed*>(m_pInflictor.Get());

		// If inflictor is player controlled and used an airstrike rocket
		if( pInflictorPed && pInflictorPed->IsControlledByLocalOrNetworkPlayer() && m_nWeaponUsedHash==WEAPONTYPE_AIRSTRIKE_ROCKET )
		{
			// If victim is part of a group
			CPedGroup* pVictimGroup = pVictim->GetPedsGroup();
			if( pVictimGroup )
			{
				// and the group leader is player controlled
				CPedGroupMembership* pVictimGroupMembership = pVictimGroup->GetGroupMembership();
				if(pVictimGroupMembership && pVictimGroupMembership->GetLeader() && pVictimGroupMembership->GetLeader()->IsPlayer())
				{
					// tell the group about the inflictor, attack!
					// NOTE: peds won't attack friendlies from this event, nor peds they are marked to not attack, etc.
					CEventShoutTargetPosition shoutTargetPositionEvent(pVictim, pInflictorPed);
					pVictimGroup->GiveEventToAllMembers(shoutTargetPositionEvent, pVictim);
				}
			}
		}	
	}

	weaponDebugf3("complete CPedDamageCalculator::ApplyDamageAndComputeResponse");
}

bool CPedDamageCalculator::ComputeWillUpdateDamageTrackers(CPed* pVictim,  CEntity* damager, u32& weaponUsedHash, const float totalDamage, const fwFlags32& flags)
{
	bool result = true;

	static const float FALL_DAMAGE_TRESHOLD = 0.1f;

	const u32 timeSinceLastDamage = fwTimer::GetTimeInMilliseconds() - pVictim->GetWeaponDamagedTime();
	bool isInflictorPed     = damager && damager->GetIsTypePed();
	bool isInflictorVehicle = damager && damager->GetIsTypeVehicle();

	if(pVictim && pVictim->IsFatallyInjured() && pVictim->IsLocalPlayer())
	{
		if (NetworkInterface::IsGameInProgress())
		{
			CVehicle* myvehicle = pVictim->GetMyVehicle();

#if !__FINAL
			weaponDebugf1("ComputeWillUpdateDamageTrackers()");

			if (gnetVerify(pVictim->GetNetworkObject()))
			{
				weaponDebugf1(" ........... Victim: '%s'.", pVictim->GetNetworkObject()->GetLogName());
			}

			if (damager)
			{
				const CPhysical* pPhysical = damager->GetIsPhysical() ? static_cast<const CPhysical*>(damager) : NULL;
				if (pPhysical)
				{
					if (pPhysical->GetNetworkObject())
					{
						weaponDebugf1(" ........... Damager: '%s'.", pPhysical->GetNetworkObject()->GetLogName());
					}
					else
					{
						weaponDebugf1(" ........... Damager: 'NO network object' logname'%s'.",pPhysical->GetLogName());
					}
				}
				else
				{
					weaponDebugf1(" ........... Damager: 'NOT Physical'.");
				}
			}
			else
			{
				weaponDebugf1(" ........... Damager: 'NULL'.");
			}

			if (myvehicle)
			{
				CEntity* damagerOfVehicle = myvehicle->GetWeaponDamageEntity();
				if (damagerOfVehicle)
				{
					if (damagerOfVehicle->GetIsPhysical())
					{
						if (((CPhysical*)damagerOfVehicle)->GetNetworkObject())
						{
							weaponDebugf1(" ........... my vehicle damager: '%s'.", ((CPhysical*)damagerOfVehicle)->GetNetworkObject()->GetLogName());
							weaponDebugf1(" ........... my vehicle damager time: '%d'.", fwTimer::GetTimeInMilliseconds() - myvehicle->GetWeaponDamagedTime());
						}
						else
						{
							weaponDebugf1(" ........... my vehicle damager: 'NO network object' logname'%s'.", ((CPhysical*)damagerOfVehicle)->GetLogName());
						}
					}
					else
					{
						weaponDebugf1(" ........... my vehicle damager: 'NOT Physical'.");
					}
				}
				else
				{
					weaponDebugf1(" ........... No Damager of Vehicle.");
				}
			}
			else
			{
				weaponDebugf1(" ........... Cant find a vehicle for the Victim.");
			}
#endif // !__FINAL

			if (isInflictorVehicle || !damager)
			{
				//We are attributing the damager to be our vehicle - a suicide.
				if (myvehicle && (myvehicle->InheritsFromBike() || myvehicle->InheritsFromQuadBike() || myvehicle->InheritsFromAmphibiousQuadBike()) && (!damager || damager == myvehicle))
				{
					const u32 timeSinceLastVehicleDamage = fwTimer::GetTimeInMilliseconds() - myvehicle->GetWeaponDamagedTime();
					const u32 FALL_USE_PREVIOUS_DAMAGE = 3000;

					//Change damager to be who knocked us out from the Bike/QuadBike
					if (timeSinceLastVehicleDamage <= FALL_USE_PREVIOUS_DAMAGE)
					{
						//Check that the damager of our vehicle (which killed us) was actually a ped or vehicle.
						//This could be a building if you smash into a wall and your bike kills you after being damaged by the building
						CEntity* damagerOfMyVehicle = myvehicle->GetWeaponDamageEntity();
						if (damagerOfMyVehicle && damagerOfMyVehicle->GetIsPhysical())
						{
							damager = myvehicle->GetWeaponDamageEntity();
							weaponUsedHash = myvehicle->GetWeaponDamageHash();
						}
					}
				}
			}
		}
	}

	//Damager pointer could have updated since the start of the function, update the type variables
	isInflictorPed     = damager && damager->GetIsTypePed();
	isInflictorVehicle = damager && damager->GetIsTypeVehicle();

	if(totalDamage == 0.0f && (isInflictorPed || isInflictorVehicle) && (WEAPONTYPE_FALL == m_nWeaponUsedHash || WEAPONTYPE_FIRE == m_nWeaponUsedHash || WEAPONTYPE_EXPLOSION == m_nWeaponUsedHash))
	{
#if !__FINAL
		if(pVictim && pVictim->IsFatallyInjured() && pVictim->IsLocalPlayer())
		{
			weaponDebugf1("Don't set weapon damage when a ped is bumped by another ped, and no damage is caused.");
		}
#endif //!__FINAL

		//Don't set weapon damage when a ped is bumped by another ped, and no damage is caused.
		damager = 0;
		result  = false;
	}
	else if (!damager || (totalDamage == 0.0f && !NetworkInterface::ShouldTriggerDamageEventForZeroDamage(pVictim) && m_nWeaponUsedHash != WEAPONTYPE_DLC_TRANQUILIZER))
	{
#if !__FINAL
		if(pVictim && pVictim->IsFatallyInjured() && pVictim->IsLocalPlayer())
		{
			weaponDebugf1("Don't set weapon damage when there is no inflictor or the total damage is 0.0f.");
		}
#endif //!__FINAL

		//Don't set weapon damage when there is no inflictor or the total damage is 0.0f
		damager = 0;
		result  = false;

		//If the ped is killed then update the damage trackers
		if (totalDamage > 0.0f && pVictim->IsFatallyInjured())
		{
			//Update physical damage trackers.
			result = true;

			static const u32 FALL_USE_PREVIOUS_DAMAGE = 1000;

			//Set to previous damager.
			if (timeSinceLastDamage <= FALL_USE_PREVIOUS_DAMAGE || (pVictim->IsLocalPlayer() && CStatsMgr::GetFreeFallDueToPlayerDamage()))
			{
				damager = pVictim->GetWeaponDamageEntity();
				weaponUsedHash = pVictim->GetWeaponDamageHash();

#if !__FINAL
				if(pVictim && pVictim->IsFatallyInjured() && pVictim->IsLocalPlayer())
				{
					weaponDebugf1("Set to previous damager.");
				}
#endif //!__FINAL
			}
		}
	}
	else if(WEAPONTYPE_FALL == m_nWeaponUsedHash && totalDamage <= FALL_DAMAGE_TRESHOLD)
	{
#if !__FINAL
		if(pVictim && pVictim->IsFatallyInjured() && pVictim->IsLocalPlayer())
		{
			weaponDebugf1("Don't set weapon damage when we receive damage from thw world and is not above a certain treshold.");
		}
#endif //!__FINAL

		//Don't set weapon damage when we receive damage from thw world and is not above a certain treshold.
		damager = 0;
		result  = false;
	}
	else
	{
		static const u32 FALL_USE_PREVIOUS_DAMAGE = 5000;

		if (isInflictorVehicle)
		{
			CVehicle* damagerVehicle = static_cast<CVehicle*>(damager);

			// for stat recording, we need death by explosion to be attributed to the ped / vehicle that caused the explosion....
			if( m_nWeaponUsedHash == WEAPONTYPE_EXPLOSION)
			{
				if(pVictim->GetIsInVehicle() && pVictim->GetMyVehicle())
				{
					if(damagerVehicle && damagerVehicle->GetDriver() && damagerVehicle->GetDriver()->IsPlayer())
					{
						if(pVictim->GetMyVehicle() != damagerVehicle)
						{
							CPed* damagerPed = damagerVehicle->GetDriver();

							// whichever weapon the damager ped is armed with at the moment, attribute the kill stat to that weapon (bullets / missiles)
							int iSelectedWeaponIndex = damagerPed->GetWeaponManager()->GetEquippedVehicleWeaponIndex();
							if(iSelectedWeaponIndex != -1)
							{
								if(damagerVehicle->GetVehicleWeaponMgr())
								{
									atArray<CVehicleWeapon*> weaponArray;
									atArray<CTurret*> turretArray;
									damagerVehicle->GetVehicleWeaponMgr()->GetWeaponsAndTurretsForSeat(damagerVehicle->GetSeatManager()->GetPedsSeatIndex(damagerPed),weaponArray,turretArray);

									if(weaponArray.GetCount()>iSelectedWeaponIndex)
									{
										const CWeaponInfo* pWeaponInfo = weaponArray[iSelectedWeaponIndex]->GetWeaponInfo();
										if(pWeaponInfo)
										{
											weaponUsedHash  = pWeaponInfo->GetHash();
											damager			= damagerVehicle;
										}
									}
								}
							}
						}	
					}
				}
			}

			//Running over someone - Attribute to the 1st vehicle causing damage in the 1st place.
			if (m_nWeaponUsedHash == WEAPONTYPE_RUNOVERBYVEHICLE && timeSinceLastDamage <= 1000 && m_nWeaponUsedHash == pVictim->GetWeaponDamageHash())
			{
				CEntity* previousDamager = pVictim->GetWeaponDamageEntity();

				//Previous damage entity was a vehicle.
				if (previousDamager)
				{
					if(previousDamager->GetIsTypeVehicle() 
						|| (previousDamager->GetIsTypePed() && static_cast<CPed*>(previousDamager)->GetIsInVehicle()))
					{
						damager = previousDamager;
					}
				}
			}

			//Player is in beast mode url:bugstar:2779019
			if (m_nWeaponUsedHash == WEAPONTYPE_RUNOVERBYVEHICLE && (fwTimer::GetTimeInMilliseconds() - damagerVehicle->GetWeaponDamagedTime() <= 1000))
			{
				CEntity* previousDamager = damagerVehicle->GetWeaponDamageEntity();
				if (previousDamager && previousDamager->GetIsTypePed() && WEAPONTYPE_UNARMED == damagerVehicle->GetWeaponDamageHash())
				{
					CPed* ped = static_cast<CPed*>(previousDamager);
					if (ped->IsPlayer())
					{
						damager = previousDamager;
					}
				}
			}

			//Fixes B*726631 - If a player is driving a car and kills a ped (WEAPONTYPE_RUNOVERBYVEHICLE) the car might not exist 
			//  in the other players machines but the driver will exist - The player.
			CPed* pedDriver = damagerVehicle->GetDriver();
			if (!pedDriver && damagerVehicle->GetSeatManager())
			{
				pedDriver = damagerVehicle->GetSeatManager()->GetLastPedInSeat(0);

				//Adding a time and scope check due to B*2447002.
				if (pedDriver)
				{
					//time
					static const u32 MAX_DRIVER_TIME = 5000;
					const u32 timeSinceDriver = damagerVehicle->m_LastTimeWeHadADriver > 0 ? fwTimer::GetTimeInMilliseconds() - damagerVehicle->m_LastTimeWeHadADriver : 0;

					//scope
					static const float MAX_SCOPE_DISTANCE = 5000.0f;
					float distanceToDriver = Dist(pVictim->GetTransform().GetPosition(), pedDriver->GetTransform().GetPosition()).Getf();

					if (timeSinceDriver > MAX_DRIVER_TIME) //Check for 5s
					{
						weaponDebugf3("CPedDamageCalculator::ComputeWillUpdateDamageTrackers - Clear pedDriver because timeSinceDriver='%d'.", timeSinceDriver);
						pedDriver = NULL;
					}
					else if (distanceToDriver > MAX_SCOPE_DISTANCE)
					{
						weaponDebugf3("CPedDamageCalculator::ComputeWillUpdateDamageTrackers - Clear pedDriver because distanceToDriver='%f'.", distanceToDriver);
						pedDriver = NULL;
					}
				}
			}

			if (pedDriver)
			{
				damager = pedDriver;
			}
		}
		else if (isInflictorPed)
		{
			//Collisions with peds on foot should be attributed to the victim since it is driving a vehicle.
			if (m_nWeaponUsedHash == WEAPONTYPE_FALL)
			{
				CPed* damagerPed = static_cast< CPed* >(damager);

				const bool victimIsInVehicle = (pVictim->GetMyVehicle() && pVictim->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ));
				const bool damagerIsOnFoot   = (!damagerPed->GetMyVehicle() || !damagerPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ));

				if (victimIsInVehicle && damagerIsOnFoot)
				{
					CVehicle* victimVehicle = pVictim->GetMyVehicle();

					//If this is not a stationary vehicle.
					if (victimVehicle->m_nVehicleFlags.bIsThisADriveableCar 
						&& victimVehicle->m_nVehicleFlags.bEngineOn 
						&& !victimVehicle->m_nVehicleFlags.bIsThisAStationaryCar)
					{
						//Set as suicide.
						damager = pVictim;
					}
				}

				//Ped is Falling and has a previous valid damager 
				if (timeSinceLastDamage <= FALL_USE_PREVIOUS_DAMAGE && pVictim->GetWeaponDamageEntity())
				{
					//Set to previous damager.
					damager = pVictim->GetWeaponDamageEntity();
					weaponUsedHash = pVictim->GetWeaponDamageHash();
				}
			}
		}
		else
		{
			//Set as suicide.
			damager = pVictim;

			//Collisions with ground or buildings or objects
			if (timeSinceLastDamage <= FALL_USE_PREVIOUS_DAMAGE || (pVictim->IsLocalPlayer() && CStatsMgr::GetFreeFallDueToPlayerDamage()))
			{
				//Attribute kill to previous damager if we have one otherwise to the victim.
				if (pVictim->GetWeaponDamageEntity())
				{
					damager = pVictim->GetWeaponDamageEntity();
					weaponUsedHash = pVictim->GetWeaponDamageHash();
				}
			}
		}
	}

	if(damager && damager->GetIsTypePed() && pVictim->GetPedResetFlag(CPED_RESET_FLAG_NoCollisionDamageFromOtherPeds) && !pVictim->IsFatallyInjured())
	{
		result = false;
	}
	
	//Register local player got damaged.
	if (result)
	{
		if (pVictim->IsLocalPlayer())
		{
			CNetworkTelemetry::PlayerInjured();
		}

		if(weaponUsedHash != WEAPONTYPE_BLEEDING || pVictim->GetWeaponDamageHash() == 0 || flags.IsFlagSet(CPedDamageCalculator::DF_MeleeDamage))
		{
			bool bMeleeHit = flags.IsFlagSet(CPedDamageCalculator::DF_MeleeDamage) || flags.IsFlagSet(CPedDamageCalculator::DF_VehicleMeleeHit);
			pVictim->SetWeaponDamageInfo(damager, weaponUsedHash, fwTimer::GetTimeInMilliseconds(), bMeleeHit);
			pVictim->SetWeaponDamageComponent(m_nHitComponent);

#if HAS_TRIGGER_RUMBLE
			if (damager && damager->GetIsTypePed())
			{
				CPed* damagerPed = static_cast<CPed*>(damager);
				bool bHitThisFrame = pVictim->GetWeaponDamagedTimeByHash(m_nWeaponUsedHash) == fwTimer::GetTimeInMilliseconds();
				if (damagerPed && damagerPed->IsLocalPlayer() && bHitThisFrame && !pVictim->IsFatallyInjured() && !damagerPed->GetIsDrivingVehicle())
					ApplyTriggerShake(m_nWeaponUsedHash, false, &CControlMgr::GetMainPlayerControl());
			}
#endif //HAS_TRIGGER_RUMBLE
		}
	}

	return result;
}

void  CPedDamageCalculator::UpdateDamageTrackers(CPed* pVictim, CPedDamageResponse& damageResponse, const bool bPedWasDeadOrDying, const fwFlags32& flags)
{
	if (!gnetVerify(pVictim))
		return;

	//Ped is already dead no point in setting physical damage trackers or anything else...
	if (bPedWasDeadOrDying)
		return;

	CEntity* damager = m_pInflictor ? m_pInflictor.Get() : 0;

	u32 weaponUsedHash = m_nWeaponUsedHash;

	//Total damage: Health/Armour or Endurance, which ever is bigger, as they are disparate pools
	const float totalDamage = MAX(damageResponse.m_fHealthLost + damageResponse.m_fArmourLost, damageResponse.m_fEnduranceLost);

	//Register Physical Damage Trackers
	if ((totalDamage > 0.0f || damageResponse.m_bKilled || weaponUsedHash == WEAPONTYPE_DLC_TRANQUILIZER || NetworkInterface::ShouldTriggerDamageEventForZeroDamage(pVictim)) && ComputeWillUpdateDamageTrackers(pVictim, damager, weaponUsedHash, totalDamage, flags))
	{
		damager = pVictim->GetWeaponDamageEntity();
		weaponUsedHash = pVictim->GetWeaponDamageHash();
	}

	//Register Hits on Peds for explosion weapons - projectiles and such
	if (totalDamage > 0.0f && damager && !pVictim->IsLocalPlayer())
	{
		CStatsMgr::RegisterExplosionHit(damager, pVictim, weaponUsedHash, totalDamage);
	}

	//Register incapacitation
	if (damageResponse.m_fEnduranceLost > 0.0f && damageResponse.m_bIncapacitated)
	{
		RegisterIncapacitated(pVictim, damager, pVictim->GetWeaponDamageHash(), damageResponse.m_bHeadShot, pVictim->GetWeaponDamageComponent(), flags.IsFlagSet(CPedDamageCalculator::DF_MeleeDamage), flags);
	}

	//Register the kill.
	if(pVictim->IsFatallyInjured())
	{
		RegisterKill(pVictim, damager, pVictim->GetWeaponDamageHash(), damageResponse.m_bHeadShot, pVictim->GetWeaponDamageComponent(), flags.IsFlagSet(CPedDamageCalculator::DF_MeleeDamage), flags);
	}

	//Update Network Damage Trackers
	if (NetworkInterface::IsGameInProgress() && !NetworkUtils::IsNetworkCloneOrMigrating(pVictim))
	{
		CNetObjPhysical* netVictim = static_cast<CNetObjPhysical *>(pVictim->GetNetworkObject());
		if(netVictim)
		{
			CNetObjPhysical::NetworkDamageInfo damageInfo(pVictim->GetWeaponDamageEntity()
															,damageResponse.m_fHealthLost
															,damageResponse.m_fArmourLost
															,damageResponse.m_fEnduranceLost
															,damageResponse.m_bIncapacitated
															,pVictim->GetWeaponDamageHash()
															,pVictim->GetWeaponDamageComponent()
															,damageResponse.m_bHeadShot
															,pVictim->IsFatallyInjured()
															,flags.IsFlagSet(CPedDamageCalculator::DF_MeleeDamage));
			netVictim->UpdateDamageTracker(damageInfo);
		}
	}
}


#if __ASSERT

void CPedDamageCalculator::CheckForAssertsWhenVictimIsKilled(CPed* pVictim, CPedDamageResponse& damageResponse, bool bDontAssertOnNullInflictor)
{
	if (!pVictim->IsFatallyInjured())
	{
		if (damageResponse.m_fHealthLost+damageResponse.m_fArmourLost <= 0.0f)
		{
			Assertf(0, "[PedDamageCalculator] Victim %s is killed without Damage=%f.", pVictim->GetNetworkObject() ? pVictim->GetNetworkObject()->GetLogName() : pVictim->GetModelName(), damageResponse.m_fHealthLost+damageResponse.m_fArmourLost);
		}
		else if (!m_pInflictor && NetworkInterface::IsGameInProgress())
		{
			if (!pVictim->GetNetworkObject())
			{
				gnetWarning("[PedDamageCalculator] - Victim %s is killed without a inflictor.", pVictim->GetModelName());
			}
			else if (m_nWeaponUsedHash == WEAPONTYPE_FALL
				|| m_nWeaponUsedHash == WEAPONTYPE_FIRE
				|| m_nWeaponUsedHash == WEAPONTYPE_EXPLOSION
				|| m_nWeaponUsedHash == WEAPONTYPE_DROWNING
				|| m_nWeaponUsedHash == WEAPONTYPE_DROWNINGINVEHICLE
				|| m_nWeaponUsedHash == WEAPONTYPE_EXHAUSTION)
			{
				//Valid exceptions
				gnetWarning("[PedDamageCalculator] - Victim %s is killed without a inflictor.", pVictim->GetNetworkObject() ? pVictim->GetNetworkObject()->GetLogName() : pVictim->GetModelName());
			}
			else if((fwTimer::GetTimeInMilliseconds() - pVictim->GetWeaponDamagedTime()) > 2000 || (!m_pInflictor && !pVictim->GetWeaponDamageEntity()))
			{
				const CWeaponInfo* cwi = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_nWeaponUsedHash);
				gnetError("[PedDamageCalculator] - Weapon Used Type: %s (%d)", cwi ? cwi->GetName() : "unknown", m_nWeaponUsedHash);
				pVictim->SpewDamageHistory();
				if (!bDontAssertOnNullInflictor)
				{
					Assertf(m_pInflictor || pVictim->GetWeaponDamageEntity(), "[PedDamageCalculator] Victim %s is killed without a inflictor.", pVictim->GetDebugName());
				}
			}
		}
	}
}

#endif // __ASSERT


dev_float PADSHAKE_MAX_DURATION = 170.0f;  // Minimum duration of the pad shake when player loses health
//dev_float PADSHAKE_MAX_FREQUENCY = 90.0f; // Minimum frequency of the pad shake when player loses health
dev_float PADSHAKE_MAX_INTENSITY = 0.8f;
dev_float PADSHAKE_INTENSITY_MULT = 0.01f;
//dev_float DIST_TO_CONSIDER_MOVEMENT = 0.5f;
//dev_float SPEED_TO_CONSIDER_MOVEMENT = 0.3f;

void CPedDamageCalculator::ApplyPadShake(const CPed* UNUSED_PARAM(pVictim), const bool bIsPlayer, const float fDamage)
{
	if(bIsPlayer)
	{
		//CControlMgr::StartPlayerPadShake((u32)(rage::Min(PADSHAKE_MAX_DURATION, 65.0f + fDamage)), (s32)(rage::Min(PADSHAKE_MAX_FREQUENCY, 45.0f + fDamage*5)));

		//B*1752710: fix for rumble assert. Damage was ~ -800, causing Min function to mess up and set a ridiculously high damage.
		//If damage is less than 0, use the absolute value
		float fDamageAbs = fDamage;
		if (rage::Max(fDamageAbs, 0.0f) == 0.0f)
		{
			fDamageAbs = Abs(fDamage);
		}


		CControlMgr::StartPlayerPadShakeByIntensity((u32)(rage::Min(PADSHAKE_MAX_DURATION, 70.0f + fDamageAbs)), rage::Min(PADSHAKE_MAX_INTENSITY, 0.2f + fDamageAbs*PADSHAKE_INTENSITY_MULT));
	}
}

void CPedDamageCalculator::ApplyTriggerShake(const u32 weaponUsedHash, const bool bIsKilled, CControl *pControl)
{
	const CWeaponInfo* weaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(weaponUsedHash);
	if (weaponInfo && weaponInfo->GetIsGunOrCanBeFiredLikeGun() && !weaponInfo->GetIsNonLethal() && (!weaponInfo->GetIsVehicleWeapon() || weaponInfo->GetIsTurret()))
	{
		f32 triggerIntensity = (bIsKilled) ? 0.95f : weaponInfo->GetRumbleIntensityTrigger();
		pControl->ApplyRecoilEffect(weaponInfo->GetRumbleDuration(), weaponInfo->GetRumbleIntensity(), triggerIntensity);
	}
}

bool CPedDamageCalculator::AccountForPedFlags(CPed* pVictim, CPedDamageResponse& damageResponse, const fwFlags32& damageFlags)
{
	//	if (pVictim->IsPlayer() && CCheat::IsCheatActive(CCheat::BULLETPROOF_CHEAT) && m_nWeaponUsedHash < WEAPONTYPE_LAST_WEAPONTYPE)
	//		return false;

	if(m_nWeaponUsedHash==WEAPONTYPE_DROWNING && !pVictim->GetPedConfigFlag( CPED_CONFIG_FLAG_DrownsInWater ))
	{
		damageRejectionDebug1f(DRR_ImmuneToDrown);
		return false;
	}

	if(m_nWeaponUsedHash==WEAPONTYPE_DROWNINGINVEHICLE && !pVictim->GetPedConfigFlag( CPED_CONFIG_FLAG_DrownsInSinkingVehicle ))
	{
		damageRejectionDebug1f(DRR_ImmuneToDrownInVehicle);
		return false;
	}

	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_nWeaponUsedHash);
	if(!pWeaponInfo)
		return false;

	// return if ped is shot in chest area whilst wearing a bullet proof vest
	if(pVictim->GetPedConfigFlag( CPED_CONFIG_FLAG_HasBulletProofVest ) && pWeaponInfo->GetIsGun())
	{
		eAnimBoneTag nHitBoneTag = pVictim->GetBoneTagFromRagdollComponent(m_nHitComponent);
		if(nHitBoneTag==BONETAG_SPINE0 || nHitBoneTag==BONETAG_SPINE1 || nHitBoneTag==BONETAG_SPINE2 || nHitBoneTag==BONETAG_SPINE3)
		{
			damageRejectionDebug1f(DRR_BulletProofVest);
			return false;
		}
	}

	//If the ped is the player and the player can't be damaged then return false.
	if(pVictim->IsPlayer())
	{
		if(!pVictim->GetPlayerInfo()->GetPlayerDataCanBeDamaged())
		{
			damageRejectionDebug1f(DRR_PlayerCantBeDamaged);
			return false;
		}

		if( pVictim->GetPlayerInfo()->bFireProof && pWeaponInfo->GetDamageType()==DAMAGE_TYPE_FIRE )
		{
			damageRejectionDebug1f(DRR_FireProof);
			return false;
		}

		if( pWeaponInfo->GetDamageType() == DAMAGE_TYPE_WATER_CANNON && !pVictim->GetPlayerInfo()->AffectedByWaterCannon())
		{
			damageRejectionDebug1f(DRR_ImmuneToWaterCannon);
			return false;
		}
	}

	// If inflictor is a ped and both pPed and pInflictor are in same group then don't accept the damage event.
	// unless it is an explosion, cars blowing up should damage everyone.
	if(m_pInflictor && m_pInflictor->GetIsTypePed() && !((CPed *)m_pInflictor.Get())->IsPlayer())
	{
		if(CPedGroups::AreInSameGroup(pVictim,(CPed*)m_pInflictor.Get()) && m_nWeaponUsedHash != WEAPONTYPE_EXPLOSION)
		{
			damageRejectionDebug1f(DRR_InflictorSameGroup);
			return false;
		}
	}

	// Don't take damage if the ped is set up to be non-shootable in a car
	if( pWeaponInfo->GetDamageType() == DAMAGE_TYPE_BULLET && pVictim->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && !pVictim->GetPedConfigFlag( CPED_CONFIG_FLAG_CanBeShotInVehicle ))
	{
		damageRejectionDebug1f(DRR_CantBeShotInVehicle);
		return false;
	}

	// Don't take damage if the player is ghosted to the inflictor
	if(pWeaponInfo->GetDamageType() == DAMAGE_TYPE_WATER_CANNON && ((CPed *)m_pInflictor.Get())->IsPlayer() && m_pInflictor && CNetworkGhostCollisions::IsLocalPlayerInGhostCollision(*(CPhysical*)m_pInflictor.Get()))
	{
		damageRejectionDebug1f(DRR_WaterCannonGhosted);
		return false;
	}

	//! Temporarily turn off "damage off" bool as we want to allow this for melee. Note: We reject damage later, but we want the 
	//! reaction.
	bool bOldNotDamagedByAnything = pVictim->m_nPhysicalFlags.bNotDamagedByAnything;
	if(damageFlags.IsFlagSet(DF_MeleeDamage))
	{
		pVictim->m_nPhysicalFlags.bNotDamagedByAnything = false;
	}

	NOTFINAL_ONLY(u32 uPhysicalRejectionReason = 0;)
	bool bRespondsToWeapon = pVictim->CanPhysicalBeDamaged(m_pInflictor.Get(), m_nWeaponUsedHash, !damageResponse.m_bClonePedKillCheck, false NOTFINAL_ONLY(, &uPhysicalRejectionReason));

#if !__FINAL
	if (!bRespondsToWeapon)
	{
		damageRejectionDebug1f(uPhysicalRejectionReason);
	}
#endif //__FINAL

	pVictim->m_nPhysicalFlags.bNotDamagedByAnything = bOldNotDamagedByAnything;

	if(m_nWeaponUsedHash == WEAPONTYPE_FALL)
	{
		if(pVictim->GetIsAttached() && !pVictim->GetPedConfigFlag(CPED_CONFIG_FLAG_AttachedToVehicle))
		{
			damageRejectionDebug1f(DRR_FallingAttachedToNonVeh);
			bRespondsToWeapon=false;
		}

		if(pVictim->GetIsSkiing() && pVictim->GetPedConfigFlag( CPED_CONFIG_FLAG_NeverFallOffSkis ))
		{
			damageRejectionDebug1f(DRR_NeverFallOffSkis);
			bRespondsToWeapon= false;
		}
	}
	// Falling out upside down vehicle
	else if (m_nWeaponUsedHash == WEAPONTYPE_RAMMEDBYVEHICLE && pVictim->GetIsInVehicle() && pVictim->GetPedConfigFlag(CPED_CONFIG_FLAG_CanActivateRagdollWhenVehicleUpsideDown))
	{
		damageRejectionDebug1f(DRR_None);
		bRespondsToWeapon = true;
	}
	return bRespondsToWeapon;
}

void CPedDamageCalculator::AccountForPedDamageStats(CPed* pVictim, CPedDamageResponse& damageResponse, const fwFlags32& flags, const float fDist)
{
	weaponDebugf3("CPedDamageCalculator::AccountForPedDamageStats pVictim[%p][%s] m_nHitComponent[%d] m_fRawDamage[%f]",pVictim,pVictim && pVictim->GetNetworkObject() ? pVictim->GetNetworkObject()->GetLogName() : "",m_nHitComponent,m_fRawDamage);

#if !__FINAL
	if (pVictim->IsDebugInvincible())
	{
		damageRejectionDebug1f(DRR_DebugInvincible);
		damageResponse.m_bInvincible = true;
	}
#endif //__FINAL

	// invincible player
	if( (flags.IsFlagSet( DF_MeleeDamage ) && pVictim->m_nPhysicalFlags.bNotDamagedByMelee) )
	{
		damageRejectionDebug1f(DRR_NotDamagedByMelee);
		damageResponse.m_bInvincible = true;
	}

	if (pVictim->m_nPhysicalFlags.bNotDamagedByAnythingButHasReactions)
	{
		damageRejectionDebug1f(DRR_NotDamagedByAnythingButHasReactions);
		damageResponse.m_bInvincible = true;
	}

#if __BANK
	if( !CPedDamageManager::GetApplyDamage() )
	{
		damageRejectionDebug1f(DRR_DontApplyDamage);
		damageResponse.m_bInvincible = true;
	}
#endif

	if( pVictim->m_nPhysicalFlags.bNotDamagedByAnything )
	{
		damageRejectionDebug1f(DRR_NotDamagedByAnything);
		damageResponse.m_bInvincible = true;
	}

	if(m_nWeaponUsedHash == 0)
	{
		weaponDebugf3("(m_nWeaponUsedHash == 0)-->return");
		return;
	}

	//
	// Work out in what way damage should be reduced, if at all
	//

	// Cache the weapon info
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_nWeaponUsedHash);
	if(!pWeaponInfo)
	{
		weaponDebugf3("(!pWeaponInfo)-->return");
		return;
	}

	// Get the inflictor ped
	const CPed* pInflictorPed = NULL;
	bool bInflictorIsDriver = false;
	if(m_pInflictor)
	{
		if(m_pInflictor->GetIsTypePed())
		{
			pInflictorPed = static_cast<const CPed *>(m_pInflictor.Get());
			bInflictorIsDriver = pInflictorPed->GetIsDrivingVehicle();
		}
		else if(m_pInflictor->GetIsTypeVehicle())
		{
			pInflictorPed = static_cast<const CVehicle *>(m_pInflictor.Get())->GetSeatManager()->GetDriver();
			bInflictorIsDriver = true;
		}
	}



	// There are some special cases that we do not want to alter the raw damage via stats (i.e. melee knockouts, takedowns and stealth kills)
	if( flags.IsFlagSet( DF_IgnoreStatModifiers ) )
	{
		// Script can modify takedown defense to be non-lethal, apply only this modifier and no others
		if (pVictim->IsPlayer() && (pVictim->GetPedConfigFlag(CPED_CONFIG_FLAG_KilledByTakedown) || pVictim->GetPedConfigFlag(CPED_CONFIG_FLAG_KilledByStealth)))
		{
			Assert(pVictim->GetPlayerInfo());
			m_fRawDamage *= pVictim->GetPlayerInfo()->GetPlayerWeaponTakedownDefenseModifier();
#if !__FINAL
			if (pVictim->GetPlayerInfo()->GetPlayerWeaponTakedownDefenseModifier() <= SMALL_FLOAT)
			{
				damageRejectionDebug1f(DRR_ZeroTakedownDefenseMod);
			}
#endif //__FINAL
			weaponDebugf3("m_fRawDamage[%f] after pVictim->GetPlayerInfo()->GetPlayerWeaponTakedownDefenseModifier()[%f]", m_fRawDamage, pVictim->GetPlayerInfo()->GetPlayerWeaponTakedownDefenseModifier());
			weaponDebugf3("(pVictim->IsPlayer() && pVictim->GetPedConfigFlag(CPED_CONFIG_FLAG_KilledByTakedown))-->return");
			return;
		}

		// In all other cases of this flag, skip stat modifers
		weaponDebugf3("(flags.IsFlagSet(DF_IgnoreStatModifiers))-->return");
		return;
	}

	// if we're in a network game apply the network damage modifiers based on the victim
	if(NetworkInterface::IsGameInProgress())
	{
		if (pInflictorPed && pInflictorPed != pVictim && pVictim->IsPlayer() && pInflictorPed->GetPlayerInfo())
		{
			CNetObjPed* pTargetPedObj = SafeCast(CNetObjPed, pVictim->GetNetworkObject());
			if (pTargetPedObj && !pTargetPedObj->GetIsDamagableByPlayer(pInflictorPed->GetPlayerInfo()->GetPhysicalPlayerIndex()))
			{
				damageRejectionDebug1f(DRR_NotDamagableByThisPlayer);
				damageResponse.m_bInvincible = true;
			}
		}

		if(pVictim->IsPlayer())
		{
			// Don't allow damage if we are temporarily invincible after being re-spawned
			CNetObjPlayer* netObjPlayer = static_cast<CNetObjPlayer *>(pVictim->GetNetworkObject());
			if(netObjPlayer && netObjPlayer->GetRespawnInvincibilityTimer() > 0)
			{
				weaponDebugf3("(netObjPlayer && netObjPlayer->GetRespawnInvincibilityTimer() > 0)-->m_fRawDamage= 0.0f; damageResponse.m_bInvincible = true");
				damageRejectionDebug1f(DRR_Respawning);
				m_fRawDamage = 0.0f;
				damageResponse.m_bInvincible = true;
			}
			else
			{
				m_fRawDamage *= pWeaponInfo->GetNetworkPlayerDamageModifier();
#if !__FINAL
				if (pWeaponInfo->GetNetworkPlayerDamageModifier() <= SMALL_FLOAT)
				{
					damageRejectionDebug1f(DRR_ZeroPlayerMod);
				}
#endif //__FINAL
				weaponDebugf3("m_fRawDamage[%f] after GetNetworkPlayerDamageModifier[%f]",m_fRawDamage,pWeaponInfo->GetNetworkPlayerDamageModifier());
			}
		}
		else
		{
			m_fRawDamage *= pWeaponInfo->GetNetworkPedDamageModifier();
#if !__FINAL
			if (pWeaponInfo->GetNetworkPedDamageModifier() <= SMALL_FLOAT)
			{
				damageRejectionDebug1f(DRR_ZeroPedMod);
			}
#endif //__FINAL
			weaponDebugf3("m_fRawDamage[%f] after GetNetworkPedDamageModifier[%f]",m_fRawDamage,pWeaponInfo->GetNetworkPedDamageModifier());
		}

		// Check whether we should accept this damage event based on the friendly fire settings
		if(!NetworkInterface::FriendlyFireAllowed() && !pWeaponInfo->GetNoFriendlyFireDamage())
		{
			if (pInflictorPed && pInflictorPed->IsPlayer() && pVictim && pVictim->IsPlayer())
			{
				// the inflictor is allowed to kill this ped if driving under certain circumstances (eg the car is being blown up due to bad driving)
				if (!(bInflictorIsDriver && (flags & DF_AllowDriverKill) && pVictim->GetIsInVehicle() && pVictim->GetMyVehicle() == pInflictorPed->GetMyVehicle()))
				{
					if (!NetworkInterface::IsPedAllowedToDamagePed(*pInflictorPed, *pVictim))
					{
						weaponDebugf3("!NetworkInterface::IsPedAllowedToDamagePed(*pInflictorPed, *pVictim)-->m_fRawDamage = 0.0f; damageResponse.m_bInvincible = true");
						damageRejectionDebug1f(DRR_NotDamagedByPed);
						m_fRawDamage = 0.0f;
						damageResponse.m_bInvincible = true;
					}
				}
			}
		}
	}

	// Check for player being in the rage special ability and include the damage multiplier
	if(pInflictorPed)
	{
		if(pInflictorPed->IsLocalPlayer() && !pVictim->IsLocalPlayer())
		{
			weaponDebugf3("(pInflictorPed->IsLocalPlayer() && !pVictim->IsLocalPlayer())");

			if(pInflictorPed->GetSpecialAbility())
			{
				if(pInflictorPed->GetSpecialAbility()->IsActive())
				{
					m_fRawDamage *= pInflictorPed->GetSpecialAbility()->GetDamageMultiplier();
#if !__FINAL
					if (pInflictorPed->GetSpecialAbility()->GetDamageMultiplier() <= SMALL_FLOAT)
					{
						damageRejectionDebug1f(DRR_ZeroSpecialMod);
					}
#endif //__FINAL
					weaponDebugf3("(pInflictorPed->GetSpecialAbility()->IsActive())-->m_fRawDamage[%f] after pInflictorPed->GetSpecialAbility()->GetDamageMultiplier()[%f]",m_fRawDamage,pInflictorPed->GetSpecialAbility()->GetDamageMultiplier());
				}
			}

			// Include the inflictor damage modifier as defined by script
			// If the inflictor was a clone, or we are doing a clone kill check, skip this as we have already applied it as part of CWeapon::SendFireMessage
			if(!pInflictorPed->IsNetworkClone() && !damageResponse.m_bClonePedKillCheck)
			{
				Assert(pInflictorPed->GetPlayerInfo());
				if( flags & CPedDamageCalculator::DF_MeleeDamage  )
				{
					m_fRawDamage *= pInflictorPed->GetPlayerInfo()->GetPlayerMeleeDamageModifier(pWeaponInfo->GetIsUnarmed());
#if !__FINAL
					if (pInflictorPed->GetPlayerInfo()->GetPlayerMeleeDamageModifier(pWeaponInfo->GetIsUnarmed()) <= SMALL_FLOAT)
					{
						damageRejectionDebug1f(DRR_ZeroMeleeMod);
					}
#endif //__FINAL
					weaponDebugf3("( flags & CPedDamageCalculator::DF_MeleeDamage  )-->m_fRawDamage[%f] after pInflictorPed->GetPlayerInfo()->GetPlayerMeleeWeaponDamageModifier()[%f]",m_fRawDamage,pInflictorPed->GetPlayerInfo()->GetPlayerMeleeWeaponDamageModifier());
				}
				else
				{
					m_fRawDamage *= pInflictorPed->GetPlayerInfo()->GetPlayerWeaponDamageModifier();
#if !__FINAL
					if (pInflictorPed->GetPlayerInfo()->GetPlayerWeaponDamageModifier() <= SMALL_FLOAT)
					{
						damageRejectionDebug1f(DRR_ZeroPlayerWeaponMod);
					}
#endif //__FINAL
					weaponDebugf3("m_fRawDamage[%f] after pInflictorPed->GetPlayerInfo()->GetPlayerWeaponDamageModifier()[%f]",m_fRawDamage,pInflictorPed->GetPlayerInfo()->GetPlayerWeaponDamageModifier());
				}
			}
		}
		else if(!pInflictorPed->IsPlayer())
		{
			weaponDebugf3("(!pInflictorPed->IsPlayer())");

			// Include the inflictor damage modifier
			// If the inflictor was a clone, or we are doing a clone kill check, skip this as we have already applied it as part of CWeapon::SendFireMessage
			if(!pInflictorPed->IsNetworkClone() && !damageResponse.m_bClonePedKillCheck)
			{
				if( flags & CPedDamageCalculator::DF_MeleeDamage )
				{
					m_fRawDamage *= CPedDamageCalculator::GetAiMeleeWeaponDamageModifier();

#if !__FINAL
					if (CPedDamageCalculator::GetAiMeleeWeaponDamageModifier() <= SMALL_FLOAT)
					{
						damageRejectionDebug1f(DRR_ZeroAiWeaponMod);
					}
#endif //__FINAL
					weaponDebugf3("m_fRawDamage[%f] after CPedDamageCalculator::GetAiMeleeWeaponDamageModifier()[%f]",m_fRawDamage,CPedDamageCalculator::GetAiMeleeWeaponDamageModifier());
				}
				else
				{
					m_fRawDamage *= CPedDamageCalculator::GetAiWeaponDamageModifier();

#if !__FINAL
					if (CPedDamageCalculator::GetAiWeaponDamageModifier() <= SMALL_FLOAT)
					{
						damageRejectionDebug1f(DRR_ZeroAiWeaponMod);
					}
#endif //__FINAL
					weaponDebugf3("m_fRawDamage[%f] after CPedDamageCalculator::GetAiWeaponDamageModifier()[%f]",m_fRawDamage, CPedDamageCalculator::GetAiWeaponDamageModifier());
				}

				if (pWeaponInfo->GetIsArmed())
				{
					m_fRawDamage *= pInflictorPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatWeaponDamageModifier);
#if !__FINAL
					if (pInflictorPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatWeaponDamageModifier) <= SMALL_FLOAT)
					{
						damageRejectionDebug1f(DRR_ZeroAiWeaponMod);
					}
#endif //__FINAL
					weaponDebugf3("m_fRawDamage[%f] after pInflictorPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatWeaponDamageModifier)[%f]",
						m_fRawDamage, pInflictorPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatWeaponDamageModifier));
				}
			}
		}
	}

	// Include the victim defense modifier as defined by script
	if( pVictim && pVictim->IsPlayer() )
	{
		weaponDebugf3("pVictim->IsPlayer");

		if(pVictim->GetSpecialAbility())
		{
			if(pVictim->GetSpecialAbility()->IsActive())
			{
				m_fRawDamage *= pVictim->GetSpecialAbility()->GetDefenseMultiplier();

#if !__FINAL
				if (pVictim->GetSpecialAbility()->GetDefenseMultiplier() <= SMALL_FLOAT)
				{
					damageRejectionDebug1f(DRR_ZeroDefenseMod);
				}
#endif //__FINAL
				weaponDebugf3("m_fRawDamage[%f] after pVictim->GetSpecialAbility()->GetDefenseMultiplier()[%f]",m_fRawDamage,pVictim->GetSpecialAbility()->GetDefenseMultiplier());
			}
		}

		Assert(pVictim->GetPlayerInfo());
		if( flags & CPedDamageCalculator::DF_MeleeDamage  )
		{
			m_fRawDamage *= pVictim->GetPlayerInfo()->GetPlayerMeleeWeaponDefenseModifier();

#if !__FINAL
			if (pVictim->GetPlayerInfo()->GetPlayerMeleeWeaponDefenseModifier() <= SMALL_FLOAT)
			{
				damageRejectionDebug1f(DRR_ZeroPlayerMeleeDefMod);
			}
#endif //__FINAL
			weaponDebugf3("m_fRawDamage[%f] after pVictim->GetPlayerInfo()->GetPlayerMeleeWeaponDefenseModifier()[%f]",m_fRawDamage,pVictim->GetPlayerInfo()->GetPlayerMeleeWeaponDefenseModifier());
		}
		else
		{
			// B*3191207: If minigun defense mod is set to something, use it instead of generic one.
			if (m_nWeaponUsedHash == atHashString("WEAPON_MINIGUN",0x42BF8A85) && pVictim->GetPlayerInfo()->GetPlayerWeaponMinigunDefenseModifier() != 1.0f)
			{
				m_fRawDamage *= pVictim->GetPlayerInfo()->GetPlayerWeaponMinigunDefenseModifier();
#if !__FINAL
				if (pVictim->GetPlayerInfo()->GetPlayerWeaponMinigunDefenseModifier() <= SMALL_FLOAT)
				{
					damageRejectionDebug1f(DRR_ZeroPlayerWeaponMinigunDefMod);
				}
#endif //__FINAL
				weaponDebugf3("m_fRawDamage[%f] after pVictim->GetPlayerInfo()->GetPlayerWeaponDefenseModifier()[%f]",m_fRawDamage,pVictim->GetPlayerInfo()->GetPlayerWeaponMinigunDefenseModifier());
			}
			else
			{
				m_fRawDamage *= pVictim->GetPlayerInfo()->GetPlayerWeaponDefenseModifier();
#if !__FINAL
				if (pVictim->GetPlayerInfo()->GetPlayerWeaponDefenseModifier() <= SMALL_FLOAT)
				{
					damageRejectionDebug1f(DRR_ZeroPlayerWeaponDefMod);
				}
#endif //__FINAL
				weaponDebugf3("m_fRawDamage[%f] after pVictim->GetPlayerInfo()->GetPlayerWeaponDefenseModifier()[%f]",m_fRawDamage,pVictim->GetPlayerInfo()->GetPlayerWeaponDefenseModifier());
			}
		}

		if(m_nWeaponUsedHash==WEAPONTYPE_RAMMEDBYVEHICLE || m_nWeaponUsedHash == WEAPONTYPE_RUNOVERBYVEHICLE)
		{
			m_fRawDamage *= pVictim->GetPlayerInfo()->GetPlayerVehicleDefenseModifier();
#if !__FINAL
			if (pVictim->GetPlayerInfo()->GetPlayerVehicleDefenseModifier() <= SMALL_FLOAT)
			{
				damageRejectionDebug1f(DRR_ZeroPlayerVehDefMod);
			}
#endif //__FINAL
			weaponDebugf3("m_fRawDamage[%f] after pVictim->GetPlayerInfo()->GetPlayerVehicleDefenseModifier()[%f]",m_fRawDamage,pVictim->GetPlayerInfo()->GetPlayerVehicleDefenseModifier());
		}
	}

	//
	// Based on the bone hit, scale the damage accordingly
	//

	// Get the hit bone tag
	eAnimBoneTag nHitBoneTag = BONETAG_ROOT;
	if(m_nHitComponent > -1)
	{
		nHitBoneTag = pVictim->GetBoneTagFromRagdollComponent(m_nHitComponent);
	}

	weaponDebugf3("nHitBoneTag[%d]",nHitBoneTag);

	if(CPed::IsHeadShot(nHitBoneTag))
	{
		weaponDebugf3("CPed::IsHeadShot(nHitBoneTag)");

		if(pInflictorPed && pInflictorPed->IsPlayer())
		{
			const bool bDamageFromBentBullet = flags.IsFlagSet(DF_DamageFromBentBullet) && CPlayerPedTargeting::GetAreCNCFreeAimImprovementsEnabled(pInflictorPed);	// Only block bullet bending headshot damage in CNC mode for now.
			weaponDebugf3("(pInflictorPed && pInflictorPed->IsPlayer()) DF_AllowHeadShot[%d] DF_MeleeDamage[%d] DAMAGE_TYPE_BULLET[%d] NoCriticalHits[%d] bDamageFromBentBullet[%d]",flags.IsFlagSet(DF_AllowHeadShot),flags.IsFlagSet(DF_MeleeDamage),(pWeaponInfo->GetDamageType() == DAMAGE_TYPE_BULLET),pVictim->GetPedConfigFlag(CPED_CONFIG_FLAG_NoCriticalHits), bDamageFromBentBullet);

			if(flags.IsFlagSet(DF_AllowHeadShot) && !flags.IsFlagSet(DF_MeleeDamage) && pWeaponInfo->GetDamageType() == DAMAGE_TYPE_BULLET && !pVictim->GetPedConfigFlag(CPED_CONFIG_FLAG_NoCriticalHits) && !bDamageFromBentBullet)
			{
				weaponDebugf3("(flags.IsFlagSet(DF_AllowHeadShot) && !flags.IsFlagSet(DF_MeleeDamage) && pWeaponInfo->GetDamageType() == DAMAGE_TYPE_BULLET && !pVictim->GetPedConfigFlag(CPED_CONFIG_FLAG_NoCriticalHits))");

				float fMinHeadShotDistance, fMaxHeadShotDistance, fHeadShotDamageModifier;
				if(pVictim->IsPlayer())
				{
					fMinHeadShotDistance = pWeaponInfo->GetMinHeadShotDistancePlayer();
					fMaxHeadShotDistance = pWeaponInfo->GetMaxHeadShotDistancePlayer();
					fHeadShotDamageModifier = pWeaponInfo->GetHeadShotDamageModifierPlayer();
				}
				else
				{
					fMinHeadShotDistance = pWeaponInfo->GetMinHeadShotDistanceAI();
					fMaxHeadShotDistance = pWeaponInfo->GetMaxHeadShotDistanceAI();
					fHeadShotDamageModifier = pWeaponInfo->GetHeadShotDamageModifierAI();
				}

				if(fDist > fMaxHeadShotDistance)
				{
					// No head shot damage
					weaponDebugf3("fDist > fMaxHeadShotDistance)-->No head shot damage");
				}
				else
				{
					bool bHasHelmetProp = CPedPropsMgr::CheckPropFlags(pVictim, ANCHOR_HEAD, PV_FLAG_DEFAULT_HELMET|PV_FLAG_RANDOM_HELMET|PV_FLAG_SCRIPT_HELMET|PV_FLAG_ARMOURED);

					// B*2004318/1949041: Armoured Helmet - check BERD component for armoured flag (no longer a ped prop as it now uses palette shader which isn't supported on props).
					bool bHasArmouredHelmet = CPedPropsMgr::CheckPropFlags(pVictim, ANCHOR_HEAD, PV_FLAG_ARMOURED);
					if (!bHasArmouredHelmet)
					{
						CPedModelInfo* pModelInfo = pVictim->GetPedModelInfo();
						if (pModelInfo)
						{
							CPedVariationInfoCollection* pVarInfoCollection = pModelInfo->GetVarInfo();
							if(pVarInfoCollection)
							{
								const CPedVariationData& varData = pVictim->GetPedDrawHandler().GetVarData();
								const u32 drawblId = varData.GetPedCompIdx(PV_COMP_BERD);
								bHasArmouredHelmet = pVarInfoCollection->HasComponentFlagsSet(PV_COMP_BERD, drawblId, PV_FLAG_ARMOURED);
							}
						}
					}
					
					// If we are treated as wearing a helmet, disregard the first head shot (unless using Armor Piercing ammunition)
					if(!pVictim->GetPedConfigFlag(CPED_CONFIG_FLAG_DisableHelmetArmor) && pVictim->IsPlayer() && !pVictim->GetPedConfigFlag(CPED_CONFIG_FLAG_HelmetHasBeenShot) && !(m_eSpecialAmmoType == CAmmoInfo::ArmorPiercing && sm_bSpecialAmmoArmorPiercingIgnoresArmoredHelmets)  && (bHasHelmetProp || bHasArmouredHelmet))
					{
						// B*1949041: Armoured Helmet - Takes 3 shots until stops protection (or 1 shot if using heavy sniper - flagged to IgnoreHelmets)
						TUNE_GROUP_FLOAT(ARMOURED_HELMET, NUM_HITS_ALLOWED, 2.0f, 1.0f, 10.0f, 1.0f);
						int iNumHitsAllowed = (int)NUM_HITS_ALLOWED; 
						if (bHasArmouredHelmet && !(pVictim->GetNumArmouredHelmetHits() > iNumHitsAllowed))
						{
							int iNumHits = 1;
							if (pWeaponInfo->GetFlag(CWeaponInfoFlags::IgnoreHelmets))
							{
								iNumHits = iNumHitsAllowed;
							}
							pVictim->IncrementNumArmouredHelmetHits(iNumHits);
							if (pVictim->GetNumArmouredHelmetHits() >= iNumHitsAllowed)
							{
								pVictim->SetPedConfigFlag(CPED_CONFIG_FLAG_HelmetHasBeenShot, true);
								weaponDebugf3("CPED_CONFIG_FLAG_HelmetHasBeenShot true");
							}

						}
						else if (!pWeaponInfo->GetFlag(CWeaponInfoFlags::IgnoreHelmets))
						{
							pVictim->SetPedConfigFlag(CPED_CONFIG_FLAG_HelmetHasBeenShot, true);
							weaponDebugf3("CPED_CONFIG_FLAG_HelmetHasBeenShot true");
						}
					}
					else
					{
						damageResponse.m_bHeadShot = true;
						weaponDebugf3("damageResponse.m_bHeadShot = true");

						if(fDist < fMinHeadShotDistance)
						{
							// Full head shot damage
							m_fRawDamage *= 1.f + fHeadShotDamageModifier;
							weaponDebugf3("m_fRawDamage[%f] after 1 + fHeadShotDamageModifier[%f]",m_fRawDamage,fHeadShotDamageModifier);
						}
						else if(fDist > fMinHeadShotDistance && fDist < fMaxHeadShotDistance)
						{
							float fAdjustedDistanceProjectileTraveled = fDist - fMinHeadShotDistance;
							float fWeaponFalloffRange = fMaxHeadShotDistance - fMinHeadShotDistance;
							Assert(Abs(fWeaponFalloffRange) > 0.001f);
							float fAdjust = Clamp(1.0f - (fAdjustedDistanceProjectileTraveled / fWeaponFalloffRange), 0.0f, 1.0f);

							// Damage fall off based on range
							float fModifier = (fHeadShotDamageModifier * fAdjust);
							m_fRawDamage *= 1.f + fModifier;
							weaponDebugf3("m_fRawDamage[%f] after 1 + (fHeadShotDamageModifier[%f] * fAdjust[%f])[%f]",m_fRawDamage,fHeadShotDamageModifier,fAdjust,fModifier);
						}
					}
				}
			}
		}
		else
		{
			damageResponse.m_bHeadShot = true;
			weaponDebugf3("else damageResponse.m_bHeadShot = true");
		}
	}
	else if(CPed::IsTorsoShot(nHitBoneTag))
	{
		damageResponse.m_bHitTorso = true;
		weaponDebugf3("damageResponse.m_bHitTorso = true");
	}
	// Only apply limb damage for non melee or Endurance-only attacks
	else if(!flags.IsFlagSet(CPedDamageCalculator::DF_MeleeDamage) && !flags.IsFlagSet(CPedDamageCalculator::DF_EnduranceDamageOnly))
	{
		weaponDebugf3("(!flags.IsFlagSet(CPedDamageCalculator::DF_MeleeDamage) && !flags.IsFlagSet(CPedDamageCalculator::DF_EnduranceDamageOnly))");

		switch(nHitBoneTag)
		{
		case BONETAG_R_HAND:
		case BONETAG_L_HAND:
		case BONETAG_R_FOOT:
		case BONETAG_L_FOOT:
		case BONETAG_R_UPPERARM:
		case BONETAG_R_FOREARM:
		case BONETAG_R_THIGH:
		case BONETAG_R_CALF:
		case BONETAG_L_UPPERARM:
		case BONETAG_L_FOREARM:
		case BONETAG_L_THIGH:
		case BONETAG_L_CALF:
			if(NetworkInterface::IsGameInProgress())
			{
				m_fRawDamage *= pWeaponInfo->GetNetworkHitLimbsDamageModifier();
#if !__FINAL
				if (pWeaponInfo->GetNetworkHitLimbsDamageModifier() <= SMALL_FLOAT)
				{
					damageRejectionDebug1f(DRR_ZeroNetLimbMod);
				}
#endif //__FINAL
				weaponDebugf3("m_fRawDamage[%f] after pWeaponInfo->GetNetworkHitLimbsDamageModifier()[%f]",m_fRawDamage,pWeaponInfo->GetNetworkHitLimbsDamageModifier());
			}
			else
			{
				m_fRawDamage *= pWeaponInfo->GetHitLimbsDamageModifier();
#if !__FINAL
				if (pWeaponInfo->GetHitLimbsDamageModifier() <= SMALL_FLOAT)
				{
					damageRejectionDebug1f(DRR_ZeroLimbMod);
				}
#endif //__FINAL
				weaponDebugf3("m_fRawDamage[%f] after pWeaponInfo->GetHitLimbsDamageModifier()[%f]",m_fRawDamage,pWeaponInfo->GetHitLimbsDamageModifier());
			}
			break;
		default:
			break;
		}
	}

	// Bone toughness
	if(pVictim->IsBoneLightlyArmoured(nHitBoneTag))
	{
		m_fRawDamage *= pWeaponInfo->GetLightlyArmouredDamageModifier();
#if !__FINAL
		if (pWeaponInfo->GetLightlyArmouredDamageModifier() <= SMALL_FLOAT)
		{
			damageRejectionDebug1f(DRR_ZeroLightArmorMod);
		}
#endif //__FINAL
		weaponDebugf3("m_fRawDamage[%f] after pWeaponInfo->GetLightlyArmouredDamageModifier()[%f]",m_fRawDamage,pWeaponInfo->GetLightlyArmouredDamageModifier());
	}

	// Awesome hack to force an offline stunned ped to be in the injured state
	if( !NetworkInterface::IsGameInProgress() && pVictim->GetPedResetFlag( CPED_RESET_FLAG_ForceInjuryAfterStunned ) )
	{
		const eDamageType weaponDamageType = pWeaponInfo->GetDamageType();
		if( weaponDamageType == DAMAGE_TYPE_ELECTRIC )
		{
			weaponDebugf3("weaponDamageType == DAMAGE_TYPE_ELECTRIC");

			// Check to see if the victim is already hurt?
			float fCurrentHealth = pVictim->GetHealth();
			float fInjuredThreshold = pVictim->GetInjuredHealthThreshold();
			if( fCurrentHealth > fInjuredThreshold )
			{
				// Need to take off an addition hit point since the hurt animation logic tests
				// GetHealth() < GetHurtHealthThreshold()
				m_fRawDamage = ( fCurrentHealth - fInjuredThreshold ) + 1.0f;
				weaponDebugf3("m_fRawDamage[%f] after = ( fCurrentHealth[%f] - fInjuredThreshold[%f] ) + 1.0f",m_fRawDamage,fCurrentHealth,fInjuredThreshold);
			}
		}
	}

	// invincible player
	if( damageResponse.m_bInvincible )
	{
		m_fRawDamage = 0.0f;
#if !__FINAL
		if (m_uRejectionReason == DRR_None)
		{
			damageRejectionDebug1f(DRR_Invincible);
		}
#endif //__FINAL
		weaponDebugf3("damageResponse.m_bInvincible-->m_fRawDamage = 0.0f");
	}
	else
	{
		if (m_nWeaponUsedHash == atHashString("WEAPON_TRANQUILIZER", 0x32A888BD))
		{
			damageRejectionDebug1f(DRR_Tranquilizer);
			weaponDebugf3("(Hit by WEAPON_TRANQUILIZER)-->return");

			pVictim->SetPedConfigFlag(CPED_CONFIG_FLAG_HitByTranqWeapon, true);
			pVictim->SetBlockingOfNonTemporaryEvents(true);
			m_fRawDamage = 0.f;
			damageResponse.m_bAffectsPed = false;
		}
	}

	weaponDebugf3("complete CPedDamageCalculator::AccountForPedDamageStats m_bInvincible[%d] m_bHeadShot[%d] m_bHitTorso[%d] m_fRawDamage[%f]",damageResponse.m_bInvincible,damageResponse.m_bHeadShot,damageResponse.m_bHitTorso,m_fRawDamage);
}

void CPedDamageCalculator::AccountForPedAmmoTypes(CPed* pVictim)
{
	weaponDebugf3("CPedDamageCalculator::AccountForPedAmmoTypes m_fRawDamage[%f] specialAmmoType[%i]", m_fRawDamage, (int)m_eSpecialAmmoType);

	// Apply a small penalty multiplier when hitting peds with FMJ or Incendiary ammunition
	float fMult = 1.0f;
	switch (m_eSpecialAmmoType)
	{
	case CAmmoInfo::FMJ:
		fMult = pVictim->IsPlayer() ? sm_fSpecialAmmoFMJPlayerDamageMultiplier : sm_fSpecialAmmoFMJPedDamageMultiplier;
		m_fRawDamage *= fMult;
		weaponDebugf3("m_fRawDamage[%f] after *= %s[%f]", m_fRawDamage, pVictim->IsPlayer() ? "sm_fSpecialAmmoFMJPlayerDamageMultiplier" : "sm_fSpecialAmmoFMJPedDamageMultiplier", fMult);
		break;
	case CAmmoInfo::Incendiary:
		fMult = pVictim->IsPlayer() ? sm_fSpecialAmmoIncendiaryPlayerDamageMultiplier : sm_fSpecialAmmoIncendiaryPedDamageMultiplier;
		m_fRawDamage *= fMult;
		weaponDebugf3("m_fRawDamage[%f] after *= %s[%f]", m_fRawDamage, pVictim->IsPlayer() ? "sm_fSpecialAmmoIncendiaryPlayerDamageMultiplier" : "sm_fSpecialAmmoIncendiaryPedDamageMultiplier", fMult);
	default:
		break;
	}

	weaponDebugf3("complete CPedDamageCalculator::AccountForPedAmmoTypes m_fRawDamage[%f]", m_fRawDamage);
}

void CPedDamageCalculator::AccountForPedArmourAndBlocking(CPed* pVictim, const CWeaponInfo* pWeaponInfo, CPedDamageResponse& damageResponse, const fwFlags32& flags)
{
	weaponDebugf3("CPedDamageCalculator::AccountForPedArmourAndBlocking pVictim[%p][%s] m_fRawDamage[%f]",pVictim,pVictim && pVictim->GetNetworkObject() ? pVictim->GetNetworkObject()->GetLogName() : "",m_fRawDamage);

	if( flags.IsFlagSet( DF_IgnoreArmor ) )
	{
		weaponDebugf3("(flags.IsFlagSet(DF_IgnoreArmor))-->return");
		return;
	}

	// Apply bonus for shooting an unarmored ped with hollow point ammo
	if (pVictim->GetArmour() <= 0.0f && m_eSpecialAmmoType == CAmmoInfo::HollowPoint)
	{
		if (m_nWeaponUsedHash == ATSTRINGHASH("WEAPON_REVOLVER_MK2", 0xCB96392F))
		{
			// Reduced multiplier for Heavy Revolver Mk II to prevent overpowered damage
			float fMult = pVictim->IsPlayer() ? sm_fSpecialAmmoHollowPointRevolverPlayerDamageMultiplierForNoArmor : sm_fSpecialAmmoHollowPointRevolverPedDamageMultiplierForNoArmor;
			m_fRawDamage *= fMult;
			weaponDebugf3("m_fRawDamage[%f] after *= %s[%f]", m_fRawDamage, pVictim->IsPlayer() ? "sm_fSpecialAmmoHollowPointRevolverPlayerDamageMultiplierForNoArmor" : "sm_fSpecialAmmoHollowPointRevolverPedDamageMultiplierForNoArmor", fMult);
		}
		else
		{
			float fMult = pVictim->IsPlayer() ? sm_fSpecialAmmoHollowPointPlayerDamageMultiplierForNoArmor : sm_fSpecialAmmoHollowPointPedDamageMultiplierForNoArmor;
			m_fRawDamage *= fMult;
			weaponDebugf3("m_fRawDamage[%f] after *= %s[%f]", m_fRawDamage, pVictim->IsPlayer() ? "sm_fSpecialAmmoHollowPointPlayerDamageMultiplierForNoArmor" : "sm_fSpecialAmmoHollowPointPedDamageMultiplierForNoArmor", fMult);
		}
	}
	//If the ped has armour and the weapon isn't armour penetrating
	//then reduce the damage that will be inflicted on the ped.
	else if(pVictim->GetArmour() > 0.0f && !pWeaponInfo->GetIsArmourPenetrating())
	{
		//If the ped is the player then record the last time that armour was reduced.
		if(FindPlayerPed()==pVictim)
			FindPlayerPed()->GetPlayerInfo()->LastTimeArmourLost = fwTimer::GetTimeInMilliseconds();

		// Apply penalty if using hollow point ammo against armored peds
		if (m_eSpecialAmmoType == CAmmoInfo::HollowPoint)
		{
			float fMult = pVictim->IsPlayer() ? sm_fSpecialAmmoHollowPointPlayerDamageMultiplierForHavingArmor : sm_fSpecialAmmoHollowPointPedDamageMultiplierForHavingArmor;
			m_fRawDamage *= fMult;
			weaponDebugf3("m_fRawDamage[%f] after *= %s[%f]", m_fRawDamage, pVictim->IsPlayer() ? "sm_fSpecialAmmoHollowPointPlayerDamageMultiplierForHavingArmor" : "sm_fSpecialAmmoHollowPointPedDamageMultiplierForHavingArmor", fMult);
		}

		// Character traits, armour efficiency
		const CPedModelInfo* mi = pVictim->GetPedModelInfo();
		Assert(mi);
		const CPedModelInfo::PersonalityData& pd = mi->GetPersonalitySettings();

		// apply damage to armour and reduce raw damage
		float fArmourEvaluation = pVictim->GetArmour() * pd.GetArmourEfficiency();
		weaponDebugf3("m_fRawDamage[%f] fArmourEvaluation(pVictim->GetArmour()[%f] * pd.GetArmourEfficiency()[%f])[%f]",m_fRawDamage,pVictim->GetArmour(),pd.GetArmourEfficiency(),fArmourEvaluation);

		if (m_eSpecialAmmoType == CAmmoInfo::ArmorPiercing && (!damageResponse.m_bHeadShot || !damageResponse.m_bForceInstantKill))
		{
			// For armor piercing ammo, we want the full amount of m_fRawDamage to remain and apply to the body, but take off a small additional percentage of armor
			float fArmourLostFromAPAmmo = m_fRawDamage * sm_fSpecialAmmoArmorPiercingDamageBonusPercentToApplyToArmor;
			damageResponse.m_fArmourLost = Min(pVictim->GetArmour(), fArmourLostFromAPAmmo);

			// We subtract m_fArmourLost from m_fRawDamage later on in ComputeWillKillPed calculations, so add it on here to balance things out
			m_fRawDamage += damageResponse.m_fArmourLost;
			
			// Don't set this for clone peds - This is set through ped sync nodes
			if (!NetworkUtils::IsNetworkCloneOrMigrating(pVictim))
			{			
				float fNewArmour = pVictim->GetArmour() - damageResponse.m_fArmourLost;
				weaponDebugf3("-->invoke SetArmour( (pVictim->GetArmour()[%f] - damageResponse.m_fArmourLost[%f])[%f] )",pVictim->GetArmour(), damageResponse.m_fArmourLost, fNewArmour);
				pVictim->SetArmour(fNewArmour);
			}
		}
		else if(m_fRawDamage <= fArmourEvaluation)
		{
			m_fRawDamage /= pd.GetArmourEfficiency();
			damageResponse.m_fArmourLost = m_fRawDamage;
			weaponDebugf3("m_fRawDamage[%f] after /= pd.GetArmourEfficienty()[%f]; m_fAmourLost[%f]",m_fRawDamage,pd.GetArmourEfficiency(),damageResponse.m_fArmourLost);

			// Don't set this for clone peds - This is set through ped sync nodes
			if (!NetworkUtils::IsNetworkCloneOrMigrating(pVictim))
			{
				float fNewArmour = pVictim->GetArmour() - m_fRawDamage;
				weaponDebugf3("-->invoke SetArmour( (pVictim->GetArmour()[%f] m_fRawDamage[%f])[%f] )",pVictim->GetArmour(),m_fRawDamage,fNewArmour);
				pVictim->SetArmour( fNewArmour );
			}
		}
		else
		{
			float fMaxDamageDeduction = Max(pVictim->GetArmour() * pd.GetArmourEfficiency() - pVictim->GetArmour(), 0.f);
			m_fRawDamage -= fMaxDamageDeduction;
			damageResponse.m_fArmourLost = pVictim->GetArmour();
			weaponDebugf3("m_fRawDamage[%f] after -= Max(pVictim->GetArmour()[%f] * pd.GetArmourEfficiency()[%f] - pVictim->GetArmour()[%f], 0.f)[%f]; m_fArmourLost[%f]",m_fRawDamage,pVictim->GetArmour(),pd.GetArmourEfficiency(),pVictim->GetArmour(),fMaxDamageDeduction,damageResponse.m_fArmourLost);

			// Don't set this for clone peds - This is set through ped sync nodes
			if (!NetworkUtils::IsNetworkCloneOrMigrating(pVictim))
			{
				weaponDebugf3("-->invoke SetArmour( 0.0f )");
				pVictim->SetArmour(0.0f);
			}
		}
	}

	weaponDebugf3("complete CPedDamageCalculator::AccountForPedArmourAndBlocking m_fRawDamage[%f]",m_fRawDamage);
}

void CPedDamageCalculator::ComputeWillKillPed(CPed* pVictim, const CWeaponInfo* pWeaponInfo, CPedDamageResponse& damageResponse, const fwFlags32& flags, const u32 uParentMeleeActionResultID)
{
	weaponDebugf3("CPedDamageCalculator::ComputeWillKillPed pVictim[%p][%s] pVictim->GetHealth[%f]", pVictim, pVictim->GetNetworkObject() ? pVictim->GetNetworkObject()->GetLogName() : "", pVictim->GetHealth());

	// Ignore all state changes if invincible
	if( damageResponse.m_bInvincible )
	{
		weaponDebugf3("(damageResponse.m_bInvincible)-->return");

		if (pVictim->IsAPlayerPed(), pVictim->IsNetworkClone())
		{
			weaponDebugf3("INVINCIBLE - reset pending damage and acknowledged damage");

			static_cast<CNetObjPlayer*>(pVictim->GetNetworkObject())->AbortNMShotReaction();
		}

		return;
	}

	float victimHealth = pVictim->GetHealth();

	weaponDebugf3("victim health = %f", victimHealth);

	if (damageResponse.m_bClonePedKillCheck && pVictim->IsAPlayerPed() && !pVictim->IsLocalPlayer() && pVictim->GetNetworkObject())
	{
		CNetObjPlayer* pPlayerObj = SafeCast(CNetObjPlayer, pVictim->GetNetworkObject());

		if ((float)pPlayerObj->GetLastReceivedHealth() != victimHealth)
		{
			victimHealth = (float)pPlayerObj->GetLastReceivedHealth();

			weaponDebugf3("use last received victim health (%f)", victimHealth);
		}
	}

	// Work out how much health is to be lost after the armor (that has already been taken off) has taken a bit out
	float fHealthLostAfterArmour = MAX(0.0f, m_fRawDamage - damageResponse.m_fArmourLost );
	damageResponse.m_bForceInstantKill = ComputeWillForceDeath(pVictim, damageResponse, flags);

	weaponDebugf3("damage = %f \r\n", fHealthLostAfterArmour);

	if (pVictim->IsAPlayerPed() && (flags & DF_ExpectedPlayerKill))
	{
		// if this is an expected kill and the player is nearly dead, force a death
		const float LOW_PLAYER_HEALTH_DEATH_THRESHOLD = 0.1f; // 10%

		float deathThreshold = pVictim->GetMaxHealth()*LOW_PLAYER_HEALTH_DEATH_THRESHOLD;
	
		if (pVictim->GetHealth() - fHealthLostAfterArmour <= pVictim->GetDyingHealthThreshold() + deathThreshold)
		{
			damageResponse.m_bForceInstantKill = true;
			weaponDebugf3("DF_ExpectedPlayerKill - force death");
		}
	}

	bool bGiveAbilityCharge = false;

	bool bWillDieRatherThanBecomeInjured = pVictim->GetPedConfigFlag( CPED_CONFIG_FLAG_ForceDieIfInjured ) || pVictim->IsAPlayerPed() || pVictim->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) || pVictim->GetPedResetFlag( CPED_RESET_FLAG_IsDrowning );
	if( NetworkInterface::IsGameInProgress() )
	{
		if( pVictim->PopTypeIsRandom() )
		{
			weaponDebugf3("PopTypeIsRandom-->bWillDieRatherThanBecomeInjured = true");
			bWillDieRatherThanBecomeInjured = true;
		}
	}

	bool bBlockHealthDamage = false;
	ApplyEnduranceDamage(pVictim, pWeaponInfo, damageResponse, flags, bBlockHealthDamage);

	if (bBlockHealthDamage)
	{
		weaponDebugf3("CPedDamageCalculator::ComputeWillKillPed--( bBlockHealthDamage ), Endurance has blocked health damage");
	}
	else if( damageResponse.m_bForceInstantKill || flags.IsFlagSet( DF_FatalMeleeDamage ) )
	{
		weaponDebugf3("CPedDamageCalculator::ComputeWillKillPed--( damageResponse.m_bForceInstantKill || flags.IsFlagSet( DF_FatalMeleeDamage ) )--m_bKilled = true");
		damageResponse.m_bKilled = true;
		damageResponse.m_fHealthLost = pVictim->GetHealth();
		damageResponse.m_fArmourLost = pVictim->GetArmour();

		if (!damageResponse.m_bClonePedKillCheck)
		{
			ASSERT_ONLY( CheckForAssertsWhenVictimIsKilled(pVictim, damageResponse, flags.IsFlagSet(DF_DontAssertOnNullInflictor)); )
			pVictim->SetHealth(0.0f, false, false);
			pVictim->SetArmour(0.0f);
		}
	}
	else if((WEAPONTYPE_FALL==m_nWeaponUsedHash && m_bJumpedOutOfMovingCar && victimHealth > pVictim->GetInjuredHealthThreshold() )
		||		 pVictim->GetPedResetFlag( CPED_RESET_FLAG_ForceScriptControlledRagdoll ) )
	{
		weaponDebugf3("WEAPONTYPE_FALL");

		//The ped has fallen out of a moving vehicle don't kill, only reduce health down to 5
		if (!damageResponse.m_bClonePedKillCheck)
		{
			pVictim->SetHealth(MAX(pVictim->GetInjuredHealthThreshold() +5.0f, victimHealth - fHealthLostAfterArmour), false, false);
		}

		damageResponse.m_fHealthLost = victimHealth - MAX(pVictim->GetInjuredHealthThreshold() +5.0f, victimHealth - fHealthLostAfterArmour);
	}
	else if(victimHealth - fHealthLostAfterArmour >= pVictim->GetInjuredHealthThreshold() )
	{
		weaponDebugf3("(pVictim->GetHealth() - fHealthLostAfterArmour >= pVictim->GetInjuredHealthThreshold() )");

		//The ped will have finite health damage and isn't being forced to die.  
		damageResponse.m_fHealthLost = fHealthLostAfterArmour;

		if (!damageResponse.m_bClonePedKillCheck)
		{
			pVictim->ChangeHealth(-fHealthLostAfterArmour);	
		}

		bGiveAbilityCharge = true;
	}
	else if( flags.IsFlagSet( DF_MeleeDamage ) && m_pInflictor && m_pInflictor->GetIsTypePed() )
	{
		weaponDebugf3("( flags.IsFlagSet( DF_MeleeDamage ) && m_pInflictor && m_pInflictor->GetIsTypePed() )");

		// Continue to decrease health as long as we are within the knockout threshold
		const CHealthConfigInfo* pHealthInfo = pVictim->GetPedHealthInfo();

		// Check to see if this action is a knockout and if so mark the victim ped
		if( !pVictim->GetPedConfigFlag( CPED_CONFIG_FLAG_Knockedout ) )
		{	
			u32 indexFound = 0;
			const CActionResult* pActionResult = ACTIONMGR.FindActionResult( indexFound, uParentMeleeActionResultID );
			if( pActionResult && pActionResult->GetIsAKnockout() )
			{
				pVictim->SetRagdollOnCollisionIgnorePhysical( (CPed*)m_pInflictor.Get() );
				pVictim->SetPedConfigFlag( CPED_CONFIG_FLAG_Knockedout, true );
			}
		}

		if( flags.IsFlagSet( DF_SelfDamage ) &&
			( pVictim->GetPedConfigFlag( CPED_CONFIG_FLAG_KilledByStealth ) ||
			  pVictim->GetPedConfigFlag( CPED_CONFIG_FLAG_KilledByTakedown ) ||
			  pVictim->GetPedConfigFlag( CPED_CONFIG_FLAG_Knockedout ) ) )
		{
			weaponDebugf3("CPedDamageCalculator::ComputeWillKillPed--DF_SelfDamage...--m_bKilled = true");
			damageResponse.m_bKilled = true;
			damageResponse.m_fHealthLost = victimHealth;

			if( !damageResponse.m_bClonePedKillCheck )
			{
				ASSERT_ONLY( CheckForAssertsWhenVictimIsKilled(pVictim, damageResponse, flags.IsFlagSet(DF_DontAssertOnNullInflictor)); )
				pVictim->SetHealth( 0.0f, false, false );
			}
		}
		else if( !pVictim->IsDead() )
		{
			// Do not allow health loss to go below knockout damage threshold (prevent non-knockout melee kills)
			if( !flags.IsFlagSet( DF_ForceMeleeDamage ) )
				fHealthLostAfterArmour = Min( victimHealth - ( pHealthInfo->GetInjuredHealthThreshold() + 1.0f ), fHealthLostAfterArmour );

			// just been injured if ped was healthy before this event
			damageResponse.m_bInjured = true;
			damageResponse.m_fHealthLost = fHealthLostAfterArmour;

			if( !damageResponse.m_bClonePedKillCheck )
			{
				pVictim->SetHealth( victimHealth - fHealthLostAfterArmour, false, false );
				if(pVictim->GetHealth() < pVictim->GetDyingHealthThreshold())
				{
					weaponDebugf3("CPedDamageCalculator::ComputeWillKillPed--(pVictim->GetHealth() < pVictim->GetDyingHealthThreshold())--m_bKilled = true");
					damageResponse.m_bKilled = true;
					pVictim->SetPedConfigFlag( CPED_CONFIG_FLAG_KilledByStandardMelee, true );
				}
			}

			bGiveAbilityCharge = true;
		}
	}
	else if(victimHealth - fHealthLostAfterArmour >= pVictim->GetDyingHealthThreshold() && !bWillDieRatherThanBecomeInjured )
	{
		// just been injured if ped was healthy before this event
		weaponDebugf3("(pVictim->GetHealth() - fHealthLostAfterArmour >= pVictim->GetDyingHealthThreshold() && !bWillDieRatherThanBecomeInjured )");
		damageResponse.m_bInjured = true;
		damageResponse.m_fHealthLost = fHealthLostAfterArmour;

		if (!damageResponse.m_bClonePedKillCheck)
		{
			pVictim->ChangeHealth(-fHealthLostAfterArmour);	 
		}

		bGiveAbilityCharge = true;
	}
	else
	{
		// ped is killed, dead.
		weaponDebugf3("else--m_bKilled = true");
		damageResponse.m_bKilled = true;
		damageResponse.m_fHealthLost = victimHealth;

		if (!damageResponse.m_bClonePedKillCheck)
		{
			ASSERT_ONLY( CheckForAssertsWhenVictimIsKilled(pVictim, damageResponse, flags.IsFlagSet(DF_DontAssertOnNullInflictor)); )
			pVictim->SetHealth(0.0f, false, false);
		}
	}

	if (pVictim->IsPlayer())
	{
		if (bGiveAbilityCharge)
		{
			CPlayerSpecialAbilityManager::ChargeEvent(ACET_DAMAGE, pVictim, fHealthLostAfterArmour);
		}
	}

	if (damageResponse.m_fHealthLost > 0.0f && !damageResponse.m_bClonePedKillCheck && pVictim->GetPlayerInfo())
	{
		// Notify HealthRecharge that the player took damage
		CPlayerHealthRecharge& healthRecharge = pVictim->GetPlayerInfo()->GetHealthRecharge();
		healthRecharge.NotifyHealthDamageTaken();
	}

	weaponDebugf3("complete CPedDamageCalculator::ComputeWillKillPed pVictim->GetHealth[%f] m_bClonePedKillCheck[%d] m_bKilled[%d] m_bInjured[%d] m_fHealthLost[%f]",pVictim->GetHealth(),damageResponse.m_bClonePedKillCheck,damageResponse.m_bKilled,damageResponse.m_bInjured,damageResponse.m_fHealthLost);
}

float CPedDamageCalculator::GetEnduranceDamage(const CPed* UNUSED_PARAM(pPed), const CWeaponInfo* pWeaponInfo, const fwFlags32& flags) const
{
	float fEnduranceDamage = 0.0f;

	if (flags.IsFlagSet(DF_HealthDamageOnly))
	{
		return fEnduranceDamage; //0.0f
	}
	else if (flags.IsFlagSet(DF_EnduranceDamageOnly))
	{
		fEnduranceDamage = m_fRawDamage;
	}
	else if (m_pInflictor && m_pInflictor->GetIsTypeObject())
	{
		fEnduranceDamage = m_fRawDamage * sm_fVehicleCollidedWithEnvironmentEnduranceDamageMultiplier;
	}
	else if (m_nWeaponUsedHash == WEAPONTYPE_FALL)
	{
		fEnduranceDamage = m_fRawDamage * sm_fFallEnduranceDamageMultiplier;
	}
	else if (m_nWeaponUsedHash == WEAPONTYPE_RAMMEDBYVEHICLE)
	{
		fEnduranceDamage = m_fRawDamage * sm_fRammedByVehicleEnduranceDamageMultiplier;
	}
	else if (m_nWeaponUsedHash == WEAPONTYPE_RUNOVERBYVEHICLE)
	{
		fEnduranceDamage = m_fRawDamage * sm_fRunOverByVehicleEnduranceDamageMultiplier;
	}
	// Some weapons explicitly deal Endurance damage. Use this value if no other damage is found
	else if (pWeaponInfo->GetEnduranceDamage() > 0.0f)
	{
		fEnduranceDamage = pWeaponInfo->GetEnduranceDamage() * (m_nDamageAggregationCount + 1);
	}

	// Apply modifiers (skip if damage is zero)
	if (fEnduranceDamage > 0.0f)
	{
		if (flags.IsFlagSet(DF_MeleeDamage) && m_pInflictor && m_pInflictor->GetIsTypePed())
		{
			const CPed* pAttacker = static_cast<const CPed*>(m_pInflictor.Get());
			const CPlayerInfo* pAttackerPlayerInfo = pAttacker->GetPlayerInfo();
			if (pAttackerPlayerInfo)
			{
				const CArcadePassiveAbilityEffects& attackerAbilities = pAttackerPlayerInfo->GetArcadeAbilityEffects();

				if (attackerAbilities.GetIsActive(AAE_FREIGHT_TRAIN))
				{
					fEnduranceDamage *= attackerAbilities.GetModifier(AAE_FREIGHT_TRAIN);
				}
			}
		}
	}

	return fEnduranceDamage;
}

void CPedDamageCalculator::ApplyEnduranceDamage(CPed* pPed, const CWeaponInfo* pWeaponInfo, CPedDamageResponse& damageResponse, const fwFlags32& flags, bool& out_bBlockHealthDamage)
{
	//Calculate damage to deal
	float fEnduranceDamage = GetEnduranceDamage(pPed, pWeaponInfo, flags);

	if (flags.IsFlagSet(DF_EnduranceDamageOnly))
	{
		damageResponse.m_bForceInstantKill = false;
		out_bBlockHealthDamage = true;
	}
	
	//Apply damage (if any)
	if (fEnduranceDamage > 0.0f)
	{
		if (pPed->GetPlayerInfo())
		{
			CPlayerEnduranceManager& enduranceManager = pPed->GetPlayerInfo()->GetEnduranceManager();
			if (enduranceManager.ShouldIgnoreEnduranceDamage())
			{
				weaponDebugf3("(enduranceManager.ShouldIgnoreEnduranceDamage())-->return");
				return;
			}
		}

		// check for damage reduction due to special ability
		if (pPed->IsPlayer())
		{			
			if (pPed->GetSpecialAbility())
			{
				if (pPed->GetSpecialAbility()->IsActive())
				{
					fEnduranceDamage *= pPed->GetSpecialAbility()->GetDefenseMultiplier();

#if !__FINAL
					if (pPed->GetSpecialAbility()->GetDefenseMultiplier() <= SMALL_FLOAT)
					{
						damageRejectionDebug1f(DRR_ZeroDefenseMod);
					}
#endif //__FINAL
					weaponDebugf3("fEnduranceDamage[%f] after pPed->GetSpecialAbility()->GetDefenseMultiplier()[%f]", fEnduranceDamage, pPed->GetSpecialAbility()->GetDefenseMultiplier());

				}
			}
		}

		if(pPed->IsLocalPlayer())
		{
			//if the affected ped is a local player, notify the camera system of the received endurance damage.
			camInterface::GetGameplayDirector().RegisterEnduranceDamage(fEnduranceDamage);
		}

		damageResponse.m_fEnduranceLost = MIN(fEnduranceDamage, pPed->GetEndurance());

		//Set m_bIncapacitated flag if this damage would cause incapacitation
		damageResponse.m_bIncapacitated =	pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_CanBeIncapacitated) &&
											CPlayerInfo::ms_Tunables.m_EnduranceManagerSettings.m_CanEnduranceIncapacitate &&
											damageResponse.m_fEnduranceLost > 0.0f &&
											damageResponse.m_fEnduranceLost >= pPed->GetEndurance();

		if (!damageResponse.m_bClonePedKillCheck && damageResponse.m_fEnduranceLost > 0.0f)
		{
			pPed->SetEndurance(pPed->GetEndurance() - damageResponse.m_fEnduranceLost);	

			if (CPlayerInfo* pPlayerInfo = pPed->GetPlayerInfo())
			{
				pPlayerInfo->GetEnduranceManager().NotifyEnduranceDamageTaken();
			}
		}
	}

	weaponDebugf3("complete CPedDamageCalculator::ApplyEnduranceDamage pVictim->GetEndurance[%f] m_bIncapacitated[%d] m_fEnduranceLost[%f]", pPed->GetEndurance(), damageResponse.m_bIncapacitated, damageResponse.m_fEnduranceLost);
}

bool CPedDamageCalculator::ComputeWillForceDeath(CPed* pVictim, CPedDamageResponse& damageResponse, const fwFlags32& flags )
{
	if (pVictim->GetPedConfigFlag( CPED_CONFIG_FLAG_NoCriticalHits ) || pVictim->IsPlayer() || damageResponse.m_bInvincible)
	{
		return false;
	}

	// Instantly kill any ped that is marked for cowering when they take damage.
	// This is done because peds in that state generally aren't able to get up cleanly.
	if (pVictim->GetPedConfigFlag( CPED_CONFIG_FLAG_CowerInsteadOfFlee ))
	{
#if __ASSERT
		if (!pVictim->IsFatallyInjured()) gnetDebug1("[PedDamageCalculator] Victim %s is being Forced Killed, reason=CPED_CONFIG_FLAG_CowerInsteadOfFlee", pVictim->GetNetworkObject() ? pVictim->GetNetworkObject()->GetLogName() : pVictim->GetModelName());
#endif

		return true;
	}

	// Instantly kill any ped that is seated.
	// This is also done to avoid peds playing getup animations on top of furniture.
	if (pVictim->GetIsSitting())
	{
#if __ASSERT
		if (!pVictim->IsFatallyInjured()) gnetDebug1("[PedDamageCalculator] Victim %s is being Forced Killed, reason=CPED_CONFIG_FLAG_IsSitting", pVictim->GetNetworkObject() ? pVictim->GetNetworkObject()->GetLogName() : pVictim->GetModelName());
#endif

		return true;
	}

	// Instantly kill any ped when the flag DF_ForceInstantKill is present.
	if (!NetworkUtils::IsNetworkCloneOrMigrating(pVictim) && flags.IsFlagSet( DF_ForceInstantKill ))
	{
#if __ASSERT
		if (!pVictim->IsFatallyInjured()) gnetDebug1("[PedDamageCalculator] Victim %s is being Forced Killed, reason=DF_ForceInstantKill", pVictim->GetNetworkObject() ? pVictim->GetNetworkObject()->GetLogName() : pVictim->GetModelName());
#endif //__ASSERT

		return true;
	}

	// We don't have quite good reactions to deal with hits during writhe task so we just die when hit
	if (m_fRawDamage > 0.0f && pVictim->GetPedIntelligence() && pVictim->GetPedIntelligence()->GetTaskWrithe())
	{
#if __ASSERT
		if (!pVictim->IsFatallyInjured()) gnetDebug1("[PedDamageCalculator] Victim %s is being Forced Killed", pVictim->GetNetworkObject() ? pVictim->GetNetworkObject()->GetLogName() : pVictim->GetModelName());
#endif

		return true;
	}

	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_nWeaponUsedHash);
	Assert(pWeaponInfo);
	eDamageType damageType = pWeaponInfo->GetDamageType();
	if (damageType == DAMAGE_TYPE_FIRE)
	{
		if (pVictim->GetTaskData().GetIsFlagSet(CTaskFlags::DiesInstantlyToFire))
		{
#if __ASSERT
			if (!pVictim->IsFatallyInjured()) gnetDebug1("[PedDamageCalculator] Victim %s is being Forced Killed, reason=DiesInstantlyToFire", pVictim->GetNetworkObject() ? pVictim->GetNetworkObject()->GetLogName() : pVictim->GetModelName());
#endif

			return true;
		}
	}

	if (damageType == DAMAGE_TYPE_MELEE)
	{
		if (pVictim->GetTaskData().GetIsFlagSet(CTaskFlags::DiesInstantlyToMelee))
		{
#if __ASSERT
			if (!pVictim->IsFatallyInjured()) gnetDebug1("[PedDamageCalculator] Victim %s is being Forced Killed, reason=DiesInstantlyToMelee", pVictim->GetNetworkObject() ? pVictim->GetNetworkObject()->GetLogName() : pVictim->GetModelName());
#endif

			return true;
		}
	}

	return false;
}

void CPedDamageCalculator::RegisterKill(const CPed* pPed, const CEntity* pInflictor, const u32 nWeaponUsedHash, const bool bHeadShot, const int weaponDamageComponent, const bool bWithMeleeWeapon, const fwFlags32& flags)
{
	weaponDebugf3("CPedDamageCalculator::RegisterKill pPed[%p] pInflictor[%p] nWeaponUsedHash[%08x] bHeadShot[%d] weaponDamageComponent[%d]",pPed,pInflictor,nWeaponUsedHash,bHeadShot,weaponDamageComponent);
	wantedDebugf3("CPedDamageCalculator::RegisterKill pPed[%p] pInflictor[%p] nWeaponUsedHash[%08x] bHeadShot[%d] weaponDamageComponent[%d]",pPed,pInflictor,nWeaponUsedHash,bHeadShot,weaponDamageComponent);
	Assertf(pPed,"RegisterKill - pPed is null ptr");
	Assertf(FindPlayerPed(),"Local player is NULL ptr");

	if(pPed)
	{
		//Register kill with stats manager
		if (pPed)
		{
			CStatsMgr::RegisterPedKill(pInflictor, pPed, nWeaponUsedHash, bHeadShot, weaponDamageComponent, bWithMeleeWeapon, flags);
		}

#if __ASSERT
		CPedModelInfo* mi = FindPlayerPed() ? FindPlayerPed()->GetPedModelInfo() : NULL;
		Assertf(mi, "Player doesnt have a model info");
#endif

		CPed* killerPed =
			(pInflictor && pInflictor->GetIsTypePed())
			? (CPed*)pInflictor
			: NULL;

		//Killer is a ped
		if(killerPed && killerPed->IsPlayer() && pInflictor != pPed)
		{
			if(pPed->IsLawEnforcementPed() || bHeadShot)
			{
				REPLAY_ONLY(ReplayBufferMarkerMgr::AddMarker(3000,2000,IMPORTANCE_LOW);)
			}

			// kill by player:
			FindPlayerPed()->GetPlayerInfo()->HavocCaused += HAVOC_KILLPED;
			CPedGroups::RegisterKillByPlayer();

			//IsLocalPlayer
			if (killerPed->IsLocalPlayer())
			{
				const CWeaponInfo* weaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(nWeaponUsedHash);

				// check for crime reporting
				switch (CCrime::sm_eReportCrimeMethod)
				{
				case CCrime::REPORT_CRIME_DEFAULT:
					if(!pPed->IsPlayer() && !flags.IsFlagSet(DF_KillPriorToClearedWantedLevel) && !flags.IsFlagSet(DF_DontReportCrimes))
					{
						if(pPed->IsLawEnforcementPed())
						{
							wantedDebugf3("CPedDamageCalculator::RegisterKill killerped is localplayer -- IsLawEnforcementPed --> CCrime::ReportCrime");

							eCrimeType crimeToReport = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_KilledByStealth) ? CRIME_STEALTH_KILL_COP : CRIME_KILL_COP;
							CCrime::ReportCrime(crimeToReport, const_cast<CPed*>(pPed), killerPed);
						}
						else if(weaponInfo && (weaponInfo->GetFireType() == FIRE_TYPE_MELEE || weaponInfo->GetDamageType() == DAMAGE_TYPE_WATER_CANNON))
						{
							wantedDebugf3("CPedDamageCalculator::RegisterKill killerped is localplayer -- MELEE/WATER_CANNON --> CCrime::ReportCrime");

							eCrimeType crimeToReport = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_KilledByStealth) ? CRIME_STEALTH_KILL_PED : CRIME_KILL_PED;
							CCrime::ReportCrime(crimeToReport, const_cast<CPed*>(pPed), killerPed);
						}
					}
					break;
				case CCrime::REPORT_CRIME_ARCADE_CNC:
					// No action - ped deaths are caught in script for CnC
					break;
				}
				
				if (bHeadShot)
					CPlayerSpecialAbilityManager::ChargeEvent(ACET_HEADSHOT, const_cast<CPed*>(killerPed));

				camInterface::GetGameplayDirector().RegisterPedKill(*killerPed, *pPed, nWeaponUsedHash, bHeadShot);
				CNewHud::GetReticule().RegisterPedKill(*killerPed, *pPed, nWeaponUsedHash);

				//NOTE: we only want to trigger the ped kill overlay if the ped was killed with a lethal weapon that fires like a gun.
				if(weaponInfo && weaponInfo->GetIsGunOrCanBeFiredLikeGun() && !weaponInfo->GetIsNonLethal())
				{
					PostFX::TriggerPedKillOverlay();

#if HAS_TRIGGER_RUMBLE
					bool bKilledThisFrame = pPed->GetWeaponDamagedTimeByHash(weaponInfo->GetHash()) == fwTimer::GetTimeInMilliseconds();
					if (killerPed->IsFiring() && bKilledThisFrame && !killerPed->GetIsDrivingVehicle())
						ApplyTriggerShake(nWeaponUsedHash, true, &CControlMgr::GetMainPlayerControl());
#endif //HAS_TRIGGER_RUMBLE
				}

				//Trigger friendly audio.
				TriggerFriendlyAudioForPlayerKill(*killerPed, *pPed);
			}
			}
		else if(killerPed && killerPed->IsLawEnforcementPed() && pPed->IsAPlayerPed())
		{
			TriggerSuspectKilledAudio(*killerPed, *pPed);
		}
		else if(!((pInflictor) && (pInflictor->GetIsTypeVehicle() && (CVehicle*)pInflictor == FindPlayerVehicle())))
		{
			// Kill not directly by player, could have been hurt badly by player recently
			CEntity* pWeaponDamageEntity = pPed->GetWeaponDamageEntity();
			if (pWeaponDamageEntity
				&& ((fwTimer::GetTimeInMilliseconds() - pPed->GetWeaponDamagedTime()) < PLAYER_WEAPON_DAMAGE_TIMEOUT)
				&& pWeaponDamageEntity->GetIsTypePed()
				&& ((CPed*)pWeaponDamageEntity)->IsPlayer()) 
			{
				// kill by player:
				FindPlayerPed()->GetPlayerInfo()->HavocCaused += HAVOC_KILLPED;
				CPedGroups::RegisterKillByPlayer();
			}
		}

		// if we've killed / headshotted a local ped and we're in a network game, increase the counter....
		if (pPed->GetNetworkObject())
		{
			//Clone head shot registering is done elsewhere in CNetObjPed::SetPedHealthData / CNetObjPhysical::SetPhysicalHealthData....
			if( !NetworkUtils::IsNetworkCloneOrMigrating(pPed) ) 
			{
				NetworkInterface::RegisterKillWithNetworkTracker(pPed);

				//If it's a headshot, register the head shot....
				if(bHeadShot)
				{
					NetworkInterface::RegisterHeadShotWithNetworkTracker(pPed);
				}				
			}
		}
	}
}

void CPedDamageCalculator::RegisterIncapacitated(const CPed* pPed, const CEntity* pInflictor, const u32 nWeaponUsedHash, const bool UNUSED_PARAM(bHeadShot), const int UNUSED_PARAM(weaponDamageComponent), const bool UNUSED_PARAM(bWithMeleeWeapon), const fwFlags32& UNUSED_PARAM(flags))
{
	if (pPed)
	{
		CPed* pInflictorPed = (pInflictor && pInflictor->GetIsTypePed()) ? (CPed*)pInflictor : NULL;
		if (pInflictorPed && pInflictorPed->IsPlayer() && pInflictor != pPed)
		{
			CNewHud::GetReticule().RegisterPedIncapacitated(*pInflictorPed, *pPed, nWeaponUsedHash);
		}
	}
}

void CPedDamageCalculator::TriggerFriendlyAudioForPlayerKill(const CPed& rPlayer, const CPed& rTarget)
{
	//Iterate over the nearby peds.
	ScalarV scMaxDistSq = ScalarVFromF32(square(30.0f));
	CEntityScannerIterator nearbyPeds = rPlayer.GetPedIntelligence()->GetNearbyPeds();
	for(CEntity* pEntity = nearbyPeds.GetFirst(); pEntity; pEntity = nearbyPeds.GetNext())
	{
		//Ensure the nearby ped is alive.
		CPed* pNearbyPed = static_cast<CPed *>(pEntity);
		if(pNearbyPed->IsInjured())
		{
			continue;
		}

		//Ensure the nearby ped is friendly with the player.
		if(!pNearbyPed->GetPedIntelligence()->IsFriendlyWith(rPlayer))
		{
			continue;
		}

		//Ensure the max distance has not been exceeded.
		ScalarV scDistSq = DistSquared(rPlayer.GetTransform().GetPosition(), pNearbyPed->GetTransform().GetPosition());
		if(IsGreaterThanAll(scDistSq, scMaxDistSq))
		{
			break;
		}

		//Check if the nearby ped was targeting the ped that was killed,
		//or the ped that was killed was targeting the nearby ped.
		CPedTargetting* pNearbyPedTargeting = pNearbyPed->GetPedIntelligence()->GetTargetting(false);
		CPedTargetting* pKilledPedTargeting = rTarget.GetPedIntelligence()->GetTargetting(false);
		if((pNearbyPedTargeting && (pNearbyPedTargeting->GetCurrentTarget() == &rTarget)) ||
			(pKilledPedTargeting && (pKilledPedTargeting->GetCurrentTarget() == pNearbyPed)))
		{
			if(pNearbyPed->GetSpeechAudioEntity())
				pNearbyPed->GetSpeechAudioEntity()->SayWhenSafe("NICE_SHOT");
		}
	}
}

void CPedDamageCalculator::TriggerSuspectKilledAudio(CPed& rLawPed, const CPed& rPlayer)
{
	ScalarV scMaxDistSq = ScalarVFromF32(30.0f);
	scMaxDistSq = (scMaxDistSq * scMaxDistSq);
	if(!rLawPed.GetPedConfigFlag(CPED_CONFIG_FLAG_VisibleOnScreen))
	{
		//Ensure the max distance has not been exceeded.
		ScalarV scDistSq = DistSquared(rLawPed.GetTransform().GetPosition(), rPlayer.GetTransform().GetPosition());
		if(IsLessThanAll(scDistSq, scMaxDistSq) && rLawPed.GetSpeechAudioEntity())
		{
			rLawPed.GetSpeechAudioEntity()->SayWhenSafe("SUSPECT_KILLED_REPORT");
			return;
		}
	}

	CEntityScannerIterator nearbyPeds = rPlayer.GetPedIntelligence()->GetNearbyPeds();
	for(CEntity* pEntity = nearbyPeds.GetFirst(); pEntity; pEntity = nearbyPeds.GetNext())
	{
		CPed* pNearbyPed = static_cast<CPed *>(pEntity);

		//Ensure the max distance has not been exceeded.
		ScalarV scDistSq = DistSquared(rPlayer.GetTransform().GetPosition(), pNearbyPed->GetTransform().GetPosition());
		if(IsGreaterThanAll(scDistSq, scMaxDistSq))
		{
			break;
		}

		//Ensure the nearby ped is alive, a cop and not on screen
		if(pNearbyPed->IsInjured() || !pNearbyPed->IsLawEnforcementPed() || pNearbyPed->GetPedConfigFlag(CPED_CONFIG_FLAG_VisibleOnScreen))
		{
			continue;
		}

		//Ensure the nearby ped is NOT friendly with the player.
		if(pNearbyPed->GetPedIntelligence()->IsFriendlyWith(rPlayer))
		{
			continue;
		}

		if(pNearbyPed->GetSpeechAudioEntity())
			pNearbyPed->GetSpeechAudioEntity()->SayWhenSafe("SUSPECT_KILLED_REPORT");
		return;
	}
}

bool CPedDamageCalculator::WasDamageFromEntityBeforeWantedLevelCleared(CEntity* pInflictor, CPed* pVictim)
{
	if(pVictim && pInflictor && pInflictor->GetIsTypePed())
	{
		u32 iDamageTime = pVictim->GetWeaponDamagedTimeByEntity(pInflictor);
		if(iDamageTime == 0)
		{
			return false;
		}

		CWanted* pInflictorWanted = static_cast<CPed*>(pInflictor)->GetPlayerWanted();
		if(pInflictorWanted && pInflictorWanted->GetTimeWantedLevelCleared() > iDamageTime)
		{
			return true;
		}
	}

	return false;
}

//////////////////////
//DAMAGE EVENT
//////////////////////
dev_float PLAYERPED_HITBREAK_SHORT = 500;	// artifical delay from playing hit anim before
dev_float CEventDamage::sm_fHitByVehicleAgitationThreshold = 30.0f;
//
CEventDamage::CEventDamage(CEntity* pInflictor,
						   const u32 nTime,
						   const u32 nWeaponUsedHash)
						   : CEventEditableResponse(),
						   m_pInflictor(pInflictor),
						   m_nDamageTime(nTime),
						   m_nWeaponUsedHash(nWeaponUsedHash),
						   m_pPhysicalResponseTask(NULL),
						   m_bStealthMode(false),
						   m_bRagdollResponse(false),
						   m_bMeleeResponse(false),
						   m_bMeleeDamage(false),
						   m_bCanReportCrimes(true)
{
#if !__FINAL
	m_EventType = EVENT_DAMAGE;
#endif // !__FINAL
}

CEventDamage::~CEventDamage()
{
	if(m_pPhysicalResponseTask)
		delete m_pPhysicalResponseTask;
}

CEventDamage::CEventDamage(const CEventDamage& src)
: CEventEditableResponse()
{
#if !__FINAL
	m_EventType = EVENT_DAMAGE;
#endif // !__FINAL
	From(src);
}


CEventEditableResponse* CEventDamage::CloneEditable() const
{
	CEventDamage* pEvent = rage_new CEventDamage(*this);
	return pEvent;
}


CEventDamage& CEventDamage::operator=(const CEventDamage& src)
{
	From(src);
	return *this;
}


void CEventDamage::From(const CEventDamage& src)
{
	m_pInflictor=src.m_pInflictor;

	m_nDamageTime = src.m_nDamageTime;
	m_nWeaponUsedHash=src.m_nWeaponUsedHash;
	m_pedDamageResponse=src.m_pedDamageResponse;

	//	Assert(src.m_pPhysicalResponseTask==NULL);
	//	m_pPhysicalResponseTask = NULL;
	m_pPhysicalResponseTask = src.ClonePhysicalResponseTask();
	m_bRagdollResponse = src.m_bRagdollResponse;
	m_bMeleeResponse = src.m_bMeleeResponse;
	m_bMeleeDamage = src.m_bMeleeDamage;
	m_bCanReportCrimes = src.m_bCanReportCrimes;

	m_bStealthMode = src.m_bStealthMode;
}

bool CEventDamage::CanDamageEventInterruptNM(const CPed* pPed) const
{
	CTask* pTask = pPed->GetPedIntelligence()->GetTaskActive();
	if (pTask && pTask->GetTaskType() == CTaskTypes::TASK_NM_CONTROL)
	{
		// RA - Commenting this test out because it was having the knock-on effect of stopping
		// events being passed to the ragdoll while blending from NM to animation.
		//if (smart_cast<CTaskNMControl*>(pTask)->IsFeedbackFlagSet(CTaskNMControl::BALANCE_FAILURE))
		//{
		//	return false;
		//}
	}
	return true;
}

bool CEventDamage::RequiresAbortForMelee(const CPed* pPed) const
{
	aiFatalAssertf(pPed,"pPed was NULL in CEventDamage::RequiresAbortForMelee");
	if (pPed->GetIsDeadOrDying())
	{
		return false;
	}


	return m_bMeleeResponse;
}

int CEventDamage::GetEventPriority() const 
{
	if (m_pedDamageResponse.m_bKilled)
	{
		if (m_bRagdollResponse)
		{
			return E_PRIORITY_DEATH_WITH_RAGDOLL;
		}
		else
		{
		return E_PRIORITY_DEATH;
	}
	}
	else if (m_bRagdollResponse)
	{
		// Check for injured// dying ped
		return E_PRIORITY_DAMAGE_WITH_RAGDOLL;
	}
	else
	{
		const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>( m_nWeaponUsedHash );
		return (pWeaponInfo && pWeaponInfo->GetIsMelee()) ? E_PRIORITY_DAMAGE_BY_MELEE : E_PRIORITY_DAMAGE;
	}
}

#if !__FINAL
void CEventDamage::Debug(const CPed& DEBUG_DRAW_ONLY(ped)) const
{
#if DEBUG_DRAW
	//Draw a line from the ped's pos to the inflictor's pos.
	{
		if(m_pInflictor)
		{
			const Vector3 v1=VEC3V_TO_VECTOR3(m_pInflictor->GetTransform().GetPosition());
			const Vector3 v2=VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition());
			grcDebugDraw::Line(v1,v2,Color32(0x00, 0xff, 0x00, 0xff));
		}
	}
#endif
}
#endif

CEntity* CEventDamage::GetSourceEntity() const
{
	if( m_pInflictor && m_pInflictor->GetType()==ENTITY_TYPE_VEHICLE )
	{
		CVehicle* pInflictorVehicle = static_cast<CVehicle*>(m_pInflictor.Get());
		
		// Use the driving parent vehicle as inflictor vehicle, if any
		// NOTE: If we ever allow chains of trailers this will need to change accordingly.
		CVehicle* pAttachedParentVehicle = pInflictorVehicle->GetAttachParentVehicleDummyOrPhysical();
		if( pAttachedParentVehicle )
		{
			pInflictorVehicle = pAttachedParentVehicle;
		}

		// get the driver of the inflictor vehicle, if any
		CPed* pDriverPed = pInflictorVehicle->GetSeatManager()->GetDriver();
		if(pDriverPed)
		{
			return pDriverPed;
		}
	}

	return m_pInflictor;
}

CPed* CEventDamage::GetPlayerCriminal() const 
{	
	CPed* pPedInflictor = NULL;

	if(m_pInflictor)
	{
		if( m_pInflictor->GetIsTypePed() )
		{
			pPedInflictor = (CPed*)m_pInflictor.Get();
		}
		else if( m_pInflictor->GetIsTypeVehicle() )
		{
			CVehicle* pInflictorVehicle = static_cast<CVehicle*>(m_pInflictor.Get());

			// Use the driving parent vehicle as inflictor vehicle, if any
			// NOTE: If we ever allow chains of trailers this will need to change accordingly.
			CVehicle* pAttachedParentVehicle = pInflictorVehicle->GetAttachParentVehicleDummyOrPhysical();
			if( pAttachedParentVehicle )
			{
				pInflictorVehicle = pAttachedParentVehicle;
		}

			pPedInflictor = pInflictorVehicle->GetSeatManager()->GetDriver();
		}

		if( pPedInflictor && pPedInflictor->IsPlayer() )
		{
			return pPedInflictor;
		}
	}
	return NULL;
}
void CEventDamage::OnEventAdded(CPed *pPed) const 
{
	CEvent::OnEventAdded(pPed);

	// Ignore events that caused no damage
	if( GetDamageApplied() <= 0.0f )
		return;

	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_nWeaponUsedHash);

	Assert(pPed);
	CPed* pPlayerCriminal = GetPlayerCriminal();
	if(pPlayerCriminal && pPlayerCriminal != pPed && m_bCanReportCrimes)
	{
		// report the proper crime
		if(pWeaponInfo && pWeaponInfo->GetFireType() == FIRE_TYPE_MELEE)
		{
			bool bKilledByStealth = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_KilledByStealth);
			if(!pPed->IsLawEnforcementPed())
			{
				eCrimeType crimeToReport = bKilledByStealth ? CRIME_STEALTH_KILL_PED : (pWeaponInfo->GetIsBlade() ? CRIME_STAB_PED : CRIME_HIT_PED);
				CCrime::ReportCrime(crimeToReport, pPed, pPlayerCriminal, m_nDamageTime);
			}
			else if(!bKilledByStealth)
			{
				CCrime::ReportCrime(pWeaponInfo->GetIsBlade() ? CRIME_STAB_COP : CRIME_HIT_COP, pPed, pPlayerCriminal, m_nDamageTime);
			}
		}
		else if(m_nWeaponUsedHash == WEAPONTYPE_RAMMEDBYVEHICLE || m_nWeaponUsedHash == WEAPONTYPE_RUNOVERBYVEHICLE)
		{
			CCrime::ReportCrime(pPed->IsLawEnforcementPed() ? CRIME_RUNOVER_COP : CRIME_RUNOVER_PED, pPed, pPlayerCriminal, m_nDamageTime);
		}

		// Need to report if we killed a cop so our WL will be adjusted properly
		if(m_pedDamageResponse.m_bKilled && pPed->IsLawEnforcementPed())
		{
			CCrime::ReportCrime(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_KilledByStealth) ? CRIME_STEALTH_KILL_COP : CRIME_KILL_COP, pPed, pPlayerCriminal, m_nDamageTime);
		}
	}

	// Report as shocking event if ped is shot.
	// Except, we don't want to fire this event for non-lethal weapons like stunguns.
	if( pWeaponInfo && pWeaponInfo->GetIsGun() && !pWeaponInfo->GetIsNonLethal()
		&& m_pInflictor && m_pInflictor->GetIsTypePed() )
	{
		CEventShockingPedShot ev(*m_pInflictor, pPed);

		// Make this event less noticeable based on the pedtype.
		float fKilledPerceptionModifier = pPed->GetPedModelInfo()->GetKilledPerceptionRangeModifer();
		if (fKilledPerceptionModifier >= 0.0f)
		{
			ev.SetVisualReactionRangeOverride(fKilledPerceptionModifier);
			ev.SetAudioReactionRangeOverride(fKilledPerceptionModifier);
		}

		CShockingEventsManager::Add(ev);
	}
}

bool CEventDamage::AffectsPedCore(CPed* pPed) const
{
	// all of the damage flag type stuff is now done in
	// CPedDamageCalculator::AccountForPedFlags()
	//
	// CPedDamageCalculator::ApplyDamageAndComputeResponse()
	// MUST have been called by now otherwise we'll just return false
	//
	if(!m_pedDamageResponse.m_bCalculated || !m_pedDamageResponse.m_bAffectsPed)
		return false;

	//If the ped is already dead then return false.
	if(pPed->IsDead())
	{
		if(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_DYING_DEAD))
		{
			return false;
		}
	}
	
	const CPed* pInflictorPed = NULL;
	if(m_pInflictor && m_pInflictor->GetIsTypePed())
	{
		pInflictorPed = static_cast<CPed *>(m_pInflictor.Get());
	}

	if( GetWitnessedFirstHand() && pInflictorPed)
	{
		if(!pPed->GetPedIntelligence()->IsFriendlyWith(*pInflictorPed))
		{
			if(!pPed->IsPlayer() && pInflictorPed->GetPedIntelligence()->CanPinDownOthers())
			{
				pPed->GetPedIntelligence()->IncreaseAmountPinnedDown(CTaskInCover::ms_Tunables.m_AmountPinnedDownByDamage);
			}

			if(GetDamageApplied() > 0.0f && pPed->IsLocalPlayer())
			{
				if(GetIsMeleeDamage())
				{
					pPed->SetUsingActionMode(true, CPed::AME_Melee, CTaskMeleeActionResult::sm_Tunables.m_ActionModeTime, false);
					pPed->SetForcedRunDelay((u32)(CTaskMeleeActionResult::sm_Tunables.m_ActionModeTime * 1000.0f));
				}
				else
				{
					pPed->GetPedIntelligence()->IncreaseBattleAwareness(CPedIntelligence::BATTLE_AWARENESS_DAMAGE);
				}
			}
		}

		if(pInflictorPed->GetPlayerInfo())
		{
			pInflictorPed->GetPlayerInfo()->SetLastTimeDamagedLocalPlayer();
		}
	}

	if(!m_bCanReportCrimes && pPed->ShouldBehaveLikeLaw())
	{
		return false;
	}

	return true;	
}


bool CEventDamage::TakesPriorityOver(const CEvent& otherEvent) const
{
	if((otherEvent.GetEventType()==EVENT_IN_WATER) && HasKilledPed())
	{
		return true;
	}
	else if((otherEvent.GetEventType()==EVENT_ON_FIRE) && HasKilledPed())
	{
		return true;
	}
	else if(m_pInflictor && m_pInflictor->GetIsTypePed() && ((CPed*)m_pInflictor.Get())->IsPlayer() && otherEvent.GetEventType()==EVENT_DAMAGE)
	{
		//Player has caused damage and other event is a damage event as well.
		//If other event wasn't caused by player and this event was this event takes priority.
		const CEntity* pOtherEventSource=otherEvent.GetSourceEntity();
		if(pOtherEventSource==m_pInflictor)
		{
			return true;			
		}
		else
		{
			//Not already responding to player.
			return true;			
		}
	}
	else if(otherEvent.GetEventType()==EVENT_DAMAGE && (GetSourceEntity()!=otherEvent.GetSourceEntity() || HasKilledPed()))
	{
		//Been damaged by someone new.
		//Responsd to the latest event.
		return true;		
	}
	else if(otherEvent.GetEventType()==EVENT_SWITCH_2_NM_TASK && m_pPhysicalResponseTask
		&& ((CTask *) m_pPhysicalResponseTask.Get())->IsNMBehaviourTask())
	{
		return true;
	}
	else if(otherEvent.GetEventType()==EVENT_DAMAGE && GetTargetPed() && GetTargetPed()->GetUsingRagdoll())
	{
		if (otherEvent.RequiresAbortForRagdoll() && !RequiresAbortForRagdoll())
		{
			// Don't want non-ragdoll damage events to take priority over existing ragdoll damage events
			return false;
		}
		else if (RequiresAbortForRagdoll() && !otherEvent.RequiresAbortForRagdoll())
		{
			// Want new ragdoll damage events to take priority over existingnon-ragdoll damage events
			return true;
		}
	}

	return CEvent::TakesPriorityOver(otherEvent);
}



bool CEventDamage::IsSameEventForAI(CEventDamage *pEvent) const
{
	if(m_pInflictor == pEvent->GetInflictor()
		&& m_nWeaponUsedHash == pEvent->GetWeaponUsedHash())
		return true;

	return false;
}


void CEventDamage::ComputeDamageAnim(CPed *pPed, CEntity* pInflictor, float fHitDirn, int nHitBoneTag, fwMvClipSetId &clipSetId, fwMvClipId& clipId, bool& bNeedsTask, bool& bNeedsFallTask)
{
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_nWeaponUsedHash);
	// check hit anim delay timer
	if(fwTimer::GetTimeInMilliseconds() < pPed->GetDamageDelayTimer())
	{
		// all peds use timer for gun impacts
		if(pWeaponInfo->GetIsGun())
			return;
		// only the player uses timer for other weapon hits
		else if(pPed->IsPlayer() && m_nWeaponUsedHash != 0)
			return;
	}

	// transform float heading into an integer offset
	float fTemp = fHitDirn + QUARTER_PI;
	if(fTemp < 0.0f) fTemp += TWO_PI;
	int nHitDirn = int(fTemp / HALF_PI);

	bool isQuadruped = pPed->GetCapsuleInfo()->IsQuadruped();
#if IK_QUAD_REACT_SOLVER_ENABLE
	CBaseIkManager::eIkSolverType solverType = !isQuadruped ? IKManagerSolverTypes::ikSolverTypeTorsoReact : IKManagerSolverTypes::ikSolverTypeQuadrupedReact;
#else
	CBaseIkManager::eIkSolverType solverType = IKManagerSolverTypes::ikSolverTypeTorsoReact;
#endif // IK_QUAD_REACT_SOLVER_ENABLE
	bool useSimpleReactions = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) || isQuadruped;
	if (useSimpleReactions)
	{
		if (!pPed->GetIkManager().IsSupported(solverType))
		{
			// HACK - Force male drivers in cars to always use the front additive damage anim (so hands don't penetrate the wheel so badly)
			// Females use ik anyway so we don't have to worry about this.
			bool forceFrontAnim = pPed->IsMale() && pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pPed->GetIsDrivingVehicle();
			
			if (forceFrontAnim)
			{
				clipId = CLIP_DAM_FRONT;
			}
			else
			{
				switch(nHitDirn)
				{
				case CEntityBoundAI::FRONT:
					clipId = CLIP_DAM_FRONT;
					break;
				case CEntityBoundAI::LEFT:
					clipId = CLIP_DAM_LEFT;
					break;
				case CEntityBoundAI::REAR:
					clipId = CLIP_DAM_BACK;
					break;
				case CEntityBoundAI::RIGHT:
					clipId = CLIP_DAM_RIGHT;
					break;
				default:
					clipId = CLIP_DAM_FRONT;
				}
			}		

			clipSetId = pPed->GetPedModelInfo()->GetAdditiveDamageClipSet();

			pPed->SetDamageDelayTimer(fwTimer::GetTimeInMilliseconds() + (u32)PLAYERPED_HITBREAK_SHORT);
		}

		// additive, just blend in anim, don't use task
		bNeedsTask = false;

		return;
	}

	Vector3 vecHeadPos(0.0f,0.0f,0.0f);
	pPed->GetBonePosition(vecHeadPos, BONETAG_HEAD);

	CTask *pTaskSimplest = pPed->GetPedIntelligence()->GetTaskActiveSimplest();

	const bool bGasWeapon = m_nWeaponUsedHash == WEAPONTYPE_SMOKEGRENADE || m_nWeaponUsedHash == WEAPONTYPE_DLC_BOMB_GAS || m_nWeaponUsedHash == WEAPONTYPE_DLC_BZGAS_MK2;
	if( vecHeadPos.z < pPed->GetTransform().GetPosition().GetZf() && !pPed->IsPlayer() && !bGasWeapon
		&& pTaskSimplest && (pTaskSimplest->GetTaskType()==CTaskTypes::TASK_FALL_OVER || pTaskSimplest->GetTaskType()==CTaskTypes::TASK_ANIMATED_HIT_BY_EXPLOSION ||
		// The getup task handles getting the ped back to a standing pose so no need to use the on-ground animations if we're not going to allow 
		// this event to be handled until we're standing
		(pTaskSimplest->GetTaskType()==CTaskTypes::TASK_GET_UP && static_cast<CTaskGetUp*>(pTaskSimplest)->ShouldAbortExternal(this))) )
	{
		const EstimatedPose pose = pPed->EstimatePose();
		if( pose == POSE_ON_BACK )
			clipId = CLIP_DAM_FLOOR_BACK;
		else
			clipId = CLIP_DAM_FLOOR_FRONT;

		Matrix34 rootMatrix;
		if(pPed->GetBoneMatrix(rootMatrix, BONETAG_ROOT))
		{
			float fAngle = AngleZ(pPed->GetTransform().GetForward(), RCC_VEC3V(rootMatrix.c)).Getf();
			if(fAngle < -QUARTER_PI)
			{
				clipId = CLIP_DAM_FLOOR_LEFT;
			}
			else if(fAngle > QUARTER_PI)
			{
				clipId = CLIP_DAM_FLOOR_RIGHT;
			}
		}

		clipSetId = pPed->GetPedModelInfo()->GetFullBodyDamageClipSet();
		bNeedsFallTask = true;
	}
	else
	{
		if(pWeaponInfo->GetGroup()==WEAPONGROUP_SHOTGUN && !pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_NoCriticalHits ))
		{
			if(nHitBoneTag==BONETAG_PELVIS || nHitBoneTag==BONETAG_SPINE0 || nHitBoneTag==BONETAG_SPINE1 || nHitBoneTag==BONETAG_SPINE2 || nHitBoneTag==BONETAG_SPINE3
				|| nHitBoneTag==BONETAG_NECK || nHitBoneTag==BONETAG_HEAD || nHitBoneTag==BONETAG_L_CLAVICLE || nHitBoneTag==BONETAG_R_CLAVICLE)
			{
				bNeedsFallTask = true;
			}
		}
		// Explosions should always cause peds to fall over
		else if( pWeaponInfo->GetDamageType() == DAMAGE_TYPE_EXPLOSIVE && !pPed->GetPedResetFlag(CPED_RESET_FLAG_BlockFallTaskFromExplosionDamage))
		{
			bNeedsFallTask = true;
		}

		if(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_UpperBodyDamageAnimsOnly ) || pPed->GetIsCrouching() || pPed->GetIsAttached())
		{
			bNeedsFallTask = false;
		}

		if(m_nWeaponUsedHash==WEAPONTYPE_FALL && m_pInflictor && m_pInflictor->GetIsTypeObject())
		{
			bNeedsFallTask = true;
		}

		if( pInflictor &&
			!pPed->IsAPlayerPed() && pPed->GetNetworkObject() && pInflictor->GetIsTypePed())
		{
			CPed* pInflictorPed = (CPed*)pInflictor;

			if (pInflictorPed->IsNetworkClone() && pInflictorPed->IsAPlayerPed())
			{
				// Fix for B* 1769037 - peds being shot by a remote player while running in MP need to fall, as they will fall for the player shooting them
				float moveBlendRatioX = 0.0f;
				float moveBlendRatioY = 0.0f;

				pPed->GetMotionData()->GetGaitReducedDesiredMoveBlendRatio(moveBlendRatioX, moveBlendRatioY);

				if (moveBlendRatioY >= 2.0f)
				{
					bNeedsFallTask = true;
				}
			}
		}
		
		// FALL DOWN
		if(bNeedsFallTask)
		{
			CTaskFallOver::eFallDirection dir = CTaskFallOver::kDirFront;
			switch(nHitDirn)
			{
			case 0:
				dir = CTaskFallOver::kDirFront;
				break;
			case 1:
				dir = CTaskFallOver::kDirLeft;
				break;
			case 2:
				dir = CTaskFallOver::kDirBack;
				break;
			case 3:
				dir = CTaskFallOver::kDirRight;
				break;
			}

			CTaskFallOver::PickFallAnimation(pPed, CTaskFallOver::ComputeFallContext(m_nWeaponUsedHash, nHitBoneTag), dir, clipSetId, clipId);
		}
		else
		{
			CTaskSynchronizedScene *pTaskSynchronizedScene = static_cast< CTaskSynchronizedScene * >(pPed->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_SYNCHRONIZED_SCENE));
			if(pTaskSynchronizedScene)
			{
				if(pTaskSynchronizedScene->IsSceneFlagSet(SYNCED_SCENE_ABORT_ON_WEAPON_DAMAGE))
				{
					pTaskSynchronizedScene->DoAbort(aiTask::ABORT_PRIORITY_URGENT, this);
				}
			}

			// Prevent player flinching when being shot
			TUNE_BOOL(bDisablePlayerDamageReactions, false);
			if(bDisablePlayerDamageReactions && pPed->IsPlayer())
				return;

			if (!pPed->GetIkManager().IsSupported(solverType))
			{
				switch(nHitDirn)
				{
				case CEntityBoundAI::FRONT:
					clipId = CLIP_DAM_FRONT;
					break;
				case CEntityBoundAI::LEFT:
					clipId = CLIP_DAM_LEFT;
					break;
				case CEntityBoundAI::REAR:
					clipId = CLIP_DAM_BACK;
					break;
				case CEntityBoundAI::RIGHT:
					clipId = CLIP_DAM_RIGHT;
					break;
				default:
					clipId = CLIP_DAM_FRONT;
				}

				clipSetId = pPed->GetPedModelInfo()->GetAdditiveDamageClipSet();
				
				pPed->SetDamageDelayTimer(fwTimer::GetTimeInMilliseconds() + (u32)PLAYERPED_HITBREAK_SHORT);
			}

			// additive, just blend in anim, don't use task
			bNeedsTask = false;
		}
	}
}

bool CEventDamage::CreateResponseTask(CPed* pPed, CEventResponse& response) const
{
	if(pPed->IsDead())
	{
		bool bValidDeadTask = false;
		if(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_DYING_DEAD))
		{
			bValidDeadTask = true;
		}

		// if we require a ragdoll response, and don't have a valid dead task running, then we can't jump out here
		if(!RequiresAbortForRagdoll() || bValidDeadTask)
			return false;
	}

	//Get some damage event details.
	CEntity* pInflictor = GetInflictor();
	u32 weaponUsedHash = GetWeaponUsedHash();

	// handle events sent second hand from friends
	if (!GetWitnessedFirstHand())
	{
		bool bAlreadyReacting = false;
		CEvent* pCurrentEvent = pPed->GetPedIntelligence()->GetCurrentEvent();
		if(pCurrentEvent && pCurrentEvent->GetEventType()==EVENT_DAMAGE
			&& IsSameEventForAI((CEventDamage*)pCurrentEvent) )
		{
			bAlreadyReacting = true;
		}

		if(!pPed->IsPlayer() && !bAlreadyReacting)
		{
			if(pInflictor && pInflictor->GetIsTypePed())
			{
				ComputePersonalityResponseToDamage(pPed, response);
			}
		}

		return response.HasEventResponse();
	}

	//Ignore damage caused by our own vehicle colliding with things.
	if(pInflictor && pInflictor->GetIsTypeVehicle() && (pPed->GetVehiclePedInside() == pInflictor))
	{
		return false;
	}

	//Register this as an 'interesting event' and report event to driver of car
	if((weaponUsedHash==WEAPONTYPE_RAMMEDBYVEHICLE || weaponUsedHash==WEAPONTYPE_RUNOVERBYVEHICLE) && pInflictor && !pInflictor->GetIsTypeBuilding())
	{
		g_InterestingEvents.Add(CInterestingEvents::EPedRunOver, pPed);
		CEventShockingPedRunOver ev(*pPed, pInflictor);
		CShockingEventsManager::Add(ev);

		if(pInflictor->GetType() == ENTITY_TYPE_VEHICLE)
		{
			CVehicle* pVehicle = (CVehicle*) pInflictor;

			if(pVehicle)
			{
				CPed* pDriver = pVehicle->GetSeatManager()->GetDriver();

				if(pDriver && CEvent::CanCreateEvent())
				{
					//create new event
					CEventRanOverPed eventRanOverPed(pVehicle,pPed,GetDamageApplied());

					// Add a copy to the ped's event queue.
					pDriver->GetPedIntelligence()->AddEvent(eventRanOverPed);
				}

				CPed* pPlayer = FindPlayerPed();
				if(pPlayer && pVehicle == pPlayer->GetMyVehicle() && audSpeechAudioEntity::CanPlayPlayerCarHitPedSpeech())
				{
					audSpeechAudioEntity::SetPlayerCarHitPedSpeechPlayed();
					u32 contextHash = ATSTRINGHASH("CAR_HIT_PED", 0x80EE7B29);

					if (pPlayer->GetPedIntelligence()->IsFriendlyWith(*pPed))
					{
						contextHash = ATSTRINGHASH("PLAYER_KILLED_FRIEND", 0xBB2D0546);
					}

					bool victimIsAnimal = pPed->GetSpeechAudioEntity() && pPed->GetSpeechAudioEntity()->IsAnimal();
					if(victimIsAnimal)
					{
						if(audEngineUtil::ResolveProbability(0.5f))
						{
							if(CTaskVehicleThreePointTurn::IsPointOnRoad(VEC3V_TO_VECTOR3(pVehicle->GetMatrix().d()), pVehicle))
								contextHash = ATSTRINGHASH("VEHICLE_COLLIDE_ONROAD_ANIMAL_GENERIC", 0x4675052A);
							else
								contextHash = ATSTRINGHASH("VEHICLE_COLLIDE_OFFROAD_ANIMAL_GENERIC", 0x80C8017E);
						}
						else
						{
							switch(pPed->GetSpeechAudioEntity()->GetAnimalType())
							{
							case kAnimalEagle:
							case kAnimalPigeon:
							case kAnimalSeagull:
							case kAnimalCrow:
							case kAnimalChickenhawk:
							case kAnimalCormorant:
								contextHash = ATSTRINGHASH("VEHICLE_COLLIDE_BIRD_GENERIC", 0xCA5067C9);
								break;
							case kAnimalSmallDog:
								contextHash = ATSTRINGHASH("VEHICLE_COLLIDE_DOG", 0xBA79A28B);
								break;
							case kAnimalCat:
								contextHash = ATSTRINGHASH("VEHICLE_COLLIDE_CAT", 0xE5C317D7);
								break;
							case kAnimalDog:
							case kAnimalRottweiler:
							case kAnimalRetriever:
								contextHash = ATSTRINGHASH("VEHICLE_COLLIDE_DOG", 0xBA79A28B);
								break;
							case kAnimalHorse:
								contextHash = ATSTRINGHASH("VEHICLE_COLLIDE_HORSE", 0x4BE74DAE);
								break;
							case kAnimalDeer:
								contextHash = ATSTRINGHASH("VEHICLE_COLLIDE_DEER", 0x3FF5E645);
								break;
							case kAnimalCoyote:
								contextHash = ATSTRINGHASH("VEHICLE_COLLIDE_COYOTE", 0x30B804B7);
								break;
							case kAnimalMountainLion:
								contextHash = ATSTRINGHASH("VEHICLE_COLLIDE_MOUNTAIN_LION", 0xBAD83E47);
								break;
							case kAnimalChicken:
							case kAnimalHen:
								contextHash = ATSTRINGHASH("VEHICLE_COLLIDE_CHICKEN", 0x2204B187);
								break;
							default:
								if(CTaskVehicleThreePointTurn::IsPointOnRoad(VEC3V_TO_VECTOR3(pVehicle->GetMatrix().d()), pVehicle))
									contextHash = ATSTRINGHASH("VEHICLE_COLLIDE_ONROAD_ANIMAL_GENERIC", 0x4675052A);
								else
									contextHash = ATSTRINGHASH("VEHICLE_COLLIDE_OFFROAD_ANIMAL_GENERIC", 0x80C8017E);
								break;
							}
						}
					}

					u32 numPassengers = 0;
					CPed** vehiclePassengers = Alloca(CPed*, pVehicle->GetSeatManager()->GetMaxSeats());

					for (s32 i = 0; i < pVehicle->GetSeatManager()->GetMaxSeats(); i++)
					{
						CPed* pPassenger = pVehicle->GetSeatManager()->GetPedInSeat(i);
						if (pPassenger && !pVehicle->IsDriver(pPassenger))
						{
							vehiclePassengers[numPassengers] = pPassenger;
							numPassengers++;
						}
					}

					CPed* pPassengerToSayAudio = NULL;

					if (numPassengers > 0)
					{
						u32 randomPassengerIndex = fwRandom::GetRandomNumberInRange(0, numPassengers);
						pPassengerToSayAudio = vehiclePassengers[randomPassengerIndex];
					}

					// make sure it's a car or bike we're in
					if (pVehicle->GetVehicleType()==VEHICLE_TYPE_CAR || pVehicle->GetVehicleType()==VEHICLE_TYPE_BIKE || pVehicle->GetVehicleType()==VEHICLE_TYPE_QUADBIKE || pVehicle->GetVehicleType()==VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE)
					{
						if( pDriver && pDriver->IsLocalPlayer() )
						{
							const bool bPassengerSaidAudio = !victimIsAnimal && fwRandom::GetRandomTrueFalse() && pPassengerToSayAudio && 
								!victimIsAnimal && pPassengerToSayAudio->GetSpeechAudioEntity() &&
								pPassengerToSayAudio->GetSpeechAudioEntity()->Say("CAR_HIT_PED_DRIVEN", "SPEECH_PARAMS_STANDARD_SHORT_LOAD") != AUD_SPEECH_SAY_FAILED;
							if( !bPassengerSaidAudio && pDriver->GetSpeechAudioEntity())
									pDriver->GetSpeechAudioEntity()->Say(contextHash, "SPEECH_PARAMS_STANDARD_SHORT_LOAD");
						}
						else if( pPassengerToSayAudio && pPassengerToSayAudio->GetSpeechAudioEntity() && !victimIsAnimal )
						{
							pPassengerToSayAudio->GetSpeechAudioEntity()->Say("CAR_HIT_PED_DRIVEN", "SPEECH_PARAMS_STANDARD_SHORT_LOAD");
						}
					}
				}
			#if FPS_MODE_SUPPORTED
				else if(pPlayer && pPed == pPlayer)
				{
					camFirstPersonShooterCamera* pFpsCamera = camInterface::GetGameplayDirector().GetFirstPersonShooterCamera();
					if (pFpsCamera)
					{
						pFpsCamera->OnVehicleImpact(pDriver, pPed);
					}
				}
			#endif // FPS_MODE_SUPPORTED
			}
		}
	}

	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(weaponUsedHash);
	if(pWeaponInfo && pWeaponInfo->GetIsMelee() && pInflictor)
	{
		CPlayerSpecialAbilityManager::ChargeEvent(ACET_GOT_PUNCHED, pPed);
		
		if (pInflictor->GetIsTypePed())
		{
			CPlayerSpecialAbilityManager::ChargeEvent(ACET_PUNCHED, (CPed*)pInflictor);
		}
	}

	if(HasKilledPed() || HasInjuredPed() || pPed->GetHealth() < 1.0f)
	{
		weaponDebugf3("HasKilledPed[%d] HasInjuredPed[%d] GetHealth[%f]",HasKilledPed(),HasInjuredPed(),pPed->GetHealth());

		aiTask* pResponseTaskFromEvent = NULL;
		if(HasPhysicalResponseTask())
			pResponseTaskFromEvent = ClonePhysicalResponseTask();

		if (!pResponseTaskFromEvent)
		{
			// If running an nm task that wants to continue on death, keep using it.
			CTask* pTaskSimplest = pPed->GetPedIntelligence()->GetTaskActiveSimplest();
			if(pTaskSimplest->IsNMBehaviourTask())
			{
				CTaskNMBehaviour *pTaskNM = smart_cast<CTaskNMBehaviour*>(pTaskSimplest);
				if (pTaskNM->ShouldContinueAfterDeath())
				{
					nmEntityDebugf(pPed, "EventDamage::CreateResponseTask - Continuing nm task %s after death", pTaskNM->GetName().c_str());
					pResponseTaskFromEvent = rage_new CTaskNMControl(pTaskNM->GetMinTime(), pTaskNM->GetMaxTime(), pTaskNM->Copy(), CTaskNMControl::ALREADY_RUNNING);
				}
			}	
		}

		//fwMvClipSetId clipSetId = CLIP_SET_ID_INVALID;
		//fwMvClipId clipId = CLIP_ID_INVALID;

		CDeathSourceInfo info(GetInflictor(), weaponUsedHash);

		if(pResponseTaskFromEvent==NULL)
		{
			weaponDebugf3("pResponseTaskFromEvent==NULL --> invoke CTaskDyingDead");
			CDeathSourceInfo info(GetInflictor(), weaponUsedHash);
			CTaskDyingDead* pDyingTask = rage_new CTaskDyingDead(&info);
			response.SetEventResponse(CEventResponse::EventResponse, pDyingTask);	
		}
		else if(((CTask*)pResponseTaskFromEvent)->HandlesDeadPed())
		{
			response.SetEventResponse(CEventResponse::EventResponse, pResponseTaskFromEvent);
		}
		else
		{
			weaponDebugf3("else --> SetEventResponse CTaskDyingDead");
			response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskDyingDead(&info));
			if(pResponseTaskFromEvent && pResponseTaskFromEvent->GetTaskType() == CTaskTypes::TASK_NM_CONTROL )
			{
				static_cast<CTaskDyingDead*>(response.GetEventResponse(CEventResponse::EventResponse))->SetForcedNaturalMotionTask(pResponseTaskFromEvent);
			}
			else if( pResponseTaskFromEvent )
			{
				delete pResponseTaskFromEvent;
			}
		}

		//Register this as an 'interesting event'
		g_InterestingEvents.Add(CInterestingEvents::EPedGotKilled, pPed);

		if (!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_BlockDeadBodyShockingEventsWhenDead))
		{
			CEventShockingDeadBody ev(*pPed);

			// Make this event less noticeable based on the pedtype.
			float fKilledPerceptionModifier = pPed->GetPedModelInfo()->GetKilledPerceptionRangeModifer();
			if (fKilledPerceptionModifier >= 0.0f)
			{
				ev.SetVisualReactionRangeOverride(fKilledPerceptionModifier);
				ev.SetAudioReactionRangeOverride(fKilledPerceptionModifier);
			}

			CShockingEventsManager::Add(ev);
		}

		// get rid of previous non-dying response
		if(pPed->GetPedIntelligence()->GetTaskAtPriority(PED_TASK_PRIORITY_PHYSICAL_RESPONSE))
		{
			pPed->GetPedIntelligence()->GetTaskAtPriority(PED_TASK_PRIORITY_PHYSICAL_RESPONSE)->MakeAbortable( CTask::ABORT_PRIORITY_URGENT, NULL);
		}
	}
	else
	{
		if(HasPhysicalResponseTask())
		{
			response.SetEventResponse(CEventResponse::PhysicalResponse, ClonePhysicalResponseTask());
		}

		// check if ped has already made a decision on how to react from previous damage event
		bool bAlreadyReacting = false;
		CEvent* pCurrentEvent = pPed->GetPedIntelligence()->GetCurrentEvent();
		if(pCurrentEvent && pCurrentEvent->GetEventType()==EVENT_DAMAGE
			&& IsSameEventForAI((CEventDamage*)pCurrentEvent) )
		{
			bAlreadyReacting = true;
		}

		if(GetWitnessedFirstHand() && GetDamageApplied() <= 0.0f && 
			pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_SAY_AUDIO))
		{
			// Restart the reaction if we have been "bumped" and are on the "say audio" part of the reaction
			bAlreadyReacting = false;
		}

		// compute AI response to damage event
		if(!pPed->IsPlayer() && !bAlreadyReacting)
		{
			if(GetSourceEntity())
			{
				ComputePersonalityResponseToDamage(pPed, response);
			}
		}
	}

#if !__NO_OUTPUT
	if (rage::DIAG_SEVERITY_DEBUG2 <= RAGE_CAT2(Channel_,nm).MaxLevel && HasPhysicalResponseTask())
	{
		aiTask* pTask = response.GetEventResponse(CEventResponse::PhysicalResponse);
		if (pTask == NULL)
		{
			pTask = response.GetEventResponse(CEventResponse::EventResponse);
		}
		while (pTask != NULL)
		{
			nmDebugf2("[%u] CreateResponseTask:%s(%p) CEventDamage:%s", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, pTask->GetName().c_str());
			pTask = pTask->GetSubTask();
		}
	}
#endif

	return response.HasEventResponse();
}

extern f32 g_HealthLostForLowPain;
extern const u32 g_SpeechContextBumpPHash;
extern const u32 g_SpeechContextGangBumpPHash;

void CEventDamage::ComputePersonalityResponseToDamage(CPed* pPed, CEventResponse& response) const
{
	Assert(pPed);

	CEntity* pInflictor = GetInflictor();

	if (pInflictor == NULL)
	{
		return;
	}
	
	if( pInflictor->GetIsTypeVehicle())
	{
		CVehicle* pVehicle = static_cast<CVehicle*>(GetInflictor());
		if(pVehicle->GetDriver())
		{
			pInflictor = pVehicle->GetDriver();
		}
	}

	// Ignore personality event responses if blocking non-temp events
	if( pPed->GetBlockingOfNonTemporaryEvents() )
	{
		return;
	}

	// Don't do anything if handcuffed, for now
	if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsHandCuffed))
	{
		return;
	}

	// If we've hurt ourselves we don't do anything
	if (!pInflictor->GetIsTypePed() || pPed == (CPed*)pInflictor)
	{
		return;
	}

	CPed* pPedInflictor = (CPed*)pInflictor;

	//Check if the ped was bumped.
	if(pPedInflictor && (GetWeaponUsedHash() == WEAPONTYPE_FALL))
	{
		if(pPed->GetSpeechAudioEntity())
		{
				bool playPain = true;
				if (pPed->GetSpeechAudioEntity()->IsAmbientSpeechPlaying())
				{
					u32 currentlyPlayingSpeechHash = pPed->GetSpeechAudioEntity()->GetCurrentlyPlayingAmbientSpeechContextHash();
					if (currentlyPlayingSpeechHash==g_SpeechContextBumpPHash || currentlyPlayingSpeechHash==g_SpeechContextGangBumpPHash)
					{
						playPain = false;
					}
				}
				if (playPain)
				{
					// play some low pain
					audDamageStats damageStats;
					damageStats.Fatal = false;
					damageStats.RawDamage = g_HealthLostForLowPain + 1.0f;
					damageStats.DamageReason = AUD_DAMAGE_REASON_DEFAULT;
					pPed->GetSpeechAudioEntity()->InflictPain(damageStats);
				}
			}

			pPed->GetPedAudioEntity()->GetFootStepAudio().PlayPedOnPedBumpSounds(pPedInflictor, pPedInflictor->GetMotionData()->GetCurrentMbrY());

		//Random peds should fall back to agitation, and not threat response.
		if(pPed->PopTypeIsRandom())
		{
			//Clear the response.
			response.ClearEventResponse(CEventResponse::EventResponse);
			return;
		}
	}

	//Ensure we have not recently tried to say damage audio.
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(GetWeaponUsedHash());
	static dev_u32 s_uMinTimeSinceLastTriedToSayAudioForDamage = 2000;
	if((pPed->GetPedIntelligence()->GetLastTimeTriedToSayAudioForDamage() + s_uMinTimeSinceLastTriedToSayAudioForDamage) < fwTimer::GetTimeInMilliseconds())
	{
		Assert(pWeaponInfo);
		eDamageType damageType = pWeaponInfo->GetDamageType();
		if( damageType == DAMAGE_TYPE_MELEE )
		{
			if( GetInflictor() && GetInflictor()->GetIsTypePed() && pPed->GetPedIntelligence()->IsFriendlyWith( static_cast<CPed&>(*pInflictor) ) )
			{
				pPed->NewSay("HIT_BY_PLAYER");
			}
		}
		else
		{
			if( GetInflictor() && GetInflictor()->GetIsTypePed() && pPed->GetPedIntelligence()->IsFriendlyWith( static_cast<CPed&>(*pInflictor) ) )
			{
				pPed->NewSay("SHOT_BY_PLAYER");
			}
			else
			{
				pPed->NewSay("SHOT_BY_PLAYER_DISLIKE");
			}
		}

		pPed->GetPedIntelligence()->TriedToSayAudioForDamage();
	}

	// B* 508975 - Peds damaged by the player should never put their hands up in response to him.
	if (GetDamageApplied() > 0.0f && pPedInflictor->IsPlayer())
	{
		pPed->GetPedIntelligence()->GetFleeBehaviour().GetFleeFlags().SetFlag(CFleeBehaviour::BF_DisableHandsUp);
	}

	bool bBumpedOnFootByPlayerInVehicle = pPed->GetIsOnFoot() && pPedInflictor && pPedInflictor->IsPlayer() && pPedInflictor->GetIsDrivingVehicle() &&
		((GetWeaponUsedHash() == WEAPONTYPE_RAMMEDBYVEHICLE) || (GetWeaponUsedHash() == WEAPONTYPE_RUNOVERBYVEHICLE));
	
	// If the ped is going to ragdoll, then never let agitation skip this event in certain cases.
	bool bCanIgnoreDamageForAgitation = true;
	if (bBumpedOnFootByPlayerInVehicle && HasPhysicalResponseTask())
	{
		switch(GetPhysicalResponseTaskType())
		{
			case CTaskTypes::TASK_NM_CONTROL:
			{
				bCanIgnoreDamageForAgitation = false;
				break;
			}
			default:
			{
				break;
			}
		}
	}


	//Check if the ped is on foot, and was bumped by a player in a vehicle.
	if(bBumpedOnFootByPlayerInVehicle && bCanIgnoreDamageForAgitation)
	{
		//Random peds should fall back to agitation, and not threat response.
		float fDamageApplied = GetDamageApplied();
		if(pPed->PopTypeIsRandom() && fDamageApplied < sm_fHitByVehicleAgitationThreshold)
		{
			//Clear the response.
			response.ClearEventResponse(CEventResponse::EventResponse);
			return;
		}
	}

	//Check if the ped is random, and was bumped by a random ped in a vehicle.
	if(pPed->PopTypeIsRandom() &&
		((GetWeaponUsedHash() == WEAPONTYPE_RAMMEDBYVEHICLE) || (GetWeaponUsedHash() == WEAPONTYPE_RUNOVERBYVEHICLE)))
	{
		//Check if the ped is in combat.
		if(pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT))
		{
			//Check if the target is a player.
			const CEntity* pTarget = pPed->GetPedIntelligence()->GetQueriableInterface()->GetHostileTarget();
			if(pTarget && pTarget->GetIsTypePed() && static_cast<const CPed *>(pTarget)->IsPlayer())
			{
				//Check if the ped is not armed.
				const CPedWeaponManager* pWeaponManager = pPed->GetWeaponManager();
				if(!pWeaponManager || !pWeaponManager->GetIsArmed())
				{
					return;
				}
			}
		}
	}

	//If ped is configured not to respond to random peds and the inflictor is one of them, we do nothing
	if ( pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DontRespondToRandomPedsDamage) && pPedInflictor && pPedInflictor->PopTypeIsRandom() )
	{
		return;
	}

	//Compute a personality response to the damage.
	const int iTaskType=GetResponseTaskType();
	Assert(pInflictor->GetType()==ENTITY_TYPE_PED);  

	// If it's a combat task then make sure we can attack the target ped
	if( CTaskTypes::IsCombatTask(iTaskType) && 
		!pPed->GetPedIntelligence()->CanAttackPed(static_cast<const CPed*>(pInflictor)) )
	{
		return;
	}

	// Check if the weapon triggers disputes instead of combat.
	if (pPedInflictor && pPedInflictor->IsAPlayerPed() && pWeaponInfo && pWeaponInfo->GetDamageCausesDisputes())
	{
		// Give the ped an agitated event instead.
		CEventAgitated event(pPedInflictor, AT_Griefing);
		pPed->GetPedIntelligence()->AddEvent(event);
		response.ClearEventResponse(CEventResponse::EventResponse);
		return;
	}

	//Ensure we are not already agitated.
	if(CTaskAgitated::ShouldIgnoreEvent(*pPed, *this))
	{
		return;
	}

	switch(iTaskType)
	{
	case CTaskTypes::TASK_COMBAT:
		{
			if(!pPed->GetPedIntelligence()->IsFriendlyWith(static_cast<CPed&>(*pInflictor)))
			{
				Assert( pPed->GetPedsGroup() == NULL || !pPed->GetPedsGroup()->GetGroupMembership()->IsMember((CPed*)pInflictor) );

				aiTask* pCombatTask = CTaskThreatResponse::CreateForTargetDueToEvent(pPed, (CPed*)pInflictor, *this);

				if (Verifyf(pCombatTask, "NULL threat response task returned from CreateForTargetDueToEvent, ped won't have an event response."))
				{
					response.SetEventResponse(CEventResponse::EventResponse, pCombatTask);
					pPed->GetPedIntelligence()->RegisterThreat((CPed*)pInflictor, true);

					if (GetWeaponUsedHash() == WEAPONTYPE_UNARMED)
					{
						// Add a melee action event corresponding to this ped getting barged into.
						CEventShockingPedKnockedIntoByPlayer shockingEvent(*pInflictor, pPed);
						CShockingEventsManager::Add(shockingEvent);
					}
				}
			}
			else
			{
				response.ClearEventResponse(CEventResponse::EventResponse);
			}
		}
		break;
	case CTaskTypes::TASK_SMART_FLEE:
		//m_pTaskEventResponse=new CTaskComplexSmartFleeEntity((CPed*)pInflictor);
		// REPLACE FLEE WITH FORCED COMBAT FLEE
		{
			if(!pPed->GetPedIntelligence()->IsFriendlyWith(static_cast<CPed&>(*pInflictor)))
			{
				aiTask* pCombatTask = rage_new CTaskThreatResponse((CPed*)pInflictor);
				response.SetEventResponse(CEventResponse::EventResponse, pCombatTask);
				pPed->GetPedIntelligence()->RegisterThreat((CPed*)pInflictor, true);
				
				if (GetWeaponUsedHash() == WEAPONTYPE_UNARMED)
				{
					// Add a melee action event corresponding to this ped getting barged into.
					CEventShockingPedKnockedIntoByPlayer shockingEvent(*pInflictor, pPed);
					CShockingEventsManager::Add(shockingEvent);
				}

			}
			else
			{
				response.ClearEventResponse(CEventResponse::EventResponse);
			}
		}
		break;	    			
	case CTaskTypes::TASK_NONE:
		response.ClearEventResponse(CEventResponse::EventResponse);
		break;
	case CTaskTypes::TASK_CROUCH:
		if(!pPed->GetIsCrouching())
			response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskCrouch(fwRandom::GetRandomNumberInRange(2000, 5000)));
		break;

// CS - Disabled due to TASK_COMPLEX_CAR_DRIVE_MISSION_FLEE_SCENE no longer existing
// 	case CTaskTypes::TASK_COMPLEX_CAR_DRIVE_MISSION_FLEE_SCENE:
// 		if(pPed->GetMyVehicle() && pPed->GetMyVehicle()->IsDriver(pPed))
// 		{
// 			CTaskSmartFlee* pFleeTask=rage_new CTaskSmartFlee(CAITarget(pPed->GetPosition()));
// 			response.SetEventResponse(CEventResponse::EventResponse, pFleeTask);
// 		}
// 		else
// 		{
// 			//m_pTaskEventResponse=new CTaskComplexSmartFleeEntity((CPed*)pInflictor);						
// 			// REPLACE FLEE WITH FORCED COMBAT FLEE
// 			aiTask* pCombatTask = CTaskThreatResponse::CreateCombatTask((CPed*)pInflictor, true, false);
// 			response.SetEventResponse(CEventResponse::EventResponse, pCombatTask);
// 			pPed->GetPedIntelligence()->RegisterThreat((CPed*)pInflictor, true);
// 		}
// 		break;	

	case CTaskTypes::TASK_COMPLEX_CAR_DRIVE_MISSION_KILL_PED:
		{
			aiTask* pCombatTask = rage_new CTaskThreatResponse((CPed*)pInflictor);
			response.SetEventResponse(CEventResponse::EventResponse, pCombatTask);
		}
		break;						
	case CTaskTypes::TASK_VEHICLE_GUN:
		if(pInflictor->GetType()==ENTITY_TYPE_PED && pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pPed->GetMyVehicle())
		{
			CPed* pFiringPed = (CPed*)pInflictor;
			const CPedWeaponManager* pPedWeaponMgr = pFiringPed->GetWeaponManager();
			if(pPedWeaponMgr && pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pPed->GetMyVehicle() && pPed != pPed->GetMyVehicle()->GetSeatManager()->GetDriver()
				&& pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_IN_VEHICLE_BASIC)
				&& CWeaponInfoManager::GetInfo<CWeaponInfo>(pPedWeaponMgr->GetEquippedWeaponHash())->GetFireType() == FIRE_TYPE_INSTANT_HIT
				&& !pPed->GetPedIntelligence()->IsFriendlyWith(*pFiringPed) )
			{
				//Compute the abort range of the driveby.
				const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(pPedWeaponMgr->GetEquippedWeaponHash());
				const float fWeaponRange = pWeaponInfo->GetLockOnRange();
				const float fSenseRange = pPed->GetPedIntelligence()->GetPedPerception().GetSeeingRange()+5.0f;
				const float fAbortRange = (fWeaponRange > fSenseRange) ? fWeaponRange : fSenseRange;

				//Compute the distance to the target.
				const Vector3 vecDelta = VEC3V_TO_VECTOR3(pFiringPed->GetTransform().GetPosition() - pPed->GetTransform().GetPosition());
				float fRangeToTarget = vecDelta.Mag();

				//Only start a driveby if distance to target is less than abort range.
				if(fRangeToTarget < fAbortRange)
				{
					const float fShootRateModifier = pFiringPed->GetPedIntelligence()->GetCombatBehaviour().GetShootRateModifier();
					CAITarget target = CAITarget(pFiringPed);
					response.SetEventResponse(CEventResponse::EventResponse, rage_new CTaskVehicleGun(CTaskVehicleGun::Mode_Fire, ATSTRINGHASH("FIRING_PATTERN_BURST_FIRE_DRIVEBY", 0x0d31265f2),&target,fShootRateModifier));
				}
				else
				{
					response.ClearEventResponse(CEventResponse::EventResponse);
				}
			}
		}
		break;
	default:
		CEventResponseFactory::CreateEventResponse(pPed, this, response);
		break;
	}
}

bool CEventDamage::HasFallenDown() const
{
	return (m_pPhysicalResponseTask && m_pPhysicalResponseTask->GetTaskType()==CTaskTypes::TASK_FALL_AND_GET_UP);
}

int CEventDamage::GetPhysicalResponseTaskType() const
{
	return (m_pPhysicalResponseTask ? m_pPhysicalResponseTask->GetTaskType() : -1);
}

aiTask* CEventDamage::ClonePhysicalResponseTask() const
{
	return (m_pPhysicalResponseTask ? m_pPhysicalResponseTask->Copy() : NULL);
}
//////////////////////
//DEATH EVENT
//////////////////////

CEventDeath::CEventDeath(const bool bHasDrowned, const bool bUseRagdoll)
: CEvent()
, m_bHasDrowned(bHasDrowned)
, m_bUseRagdoll(bUseRagdoll)
, m_bUseRagdollFrame(false)
, m_clipSetId(CLIP_SET_ID_INVALID)
, m_clipId(CLIP_ID_INVALID)
, m_startPhase(0.0f)
, m_bDisableVehicleImpactsWhilstDying(false)
{
#if !__FINAL
	m_EventType = EVENT_DEATH;
#endif

	m_iTimeOfDeath=fwTimer::GetTimeInMilliseconds();
}

CEventDeath::CEventDeath(const bool bHasDrowned, const int iTimeOfDeath, const bool bUseRagdoll)
: CEvent()
, m_bHasDrowned(bHasDrowned)
, m_bUseRagdoll(bUseRagdoll)
, m_bUseRagdollFrame(false)
, m_iTimeOfDeath(iTimeOfDeath)
, m_clipSetId(CLIP_SET_ID_INVALID)
, m_clipId(CLIP_ID_INVALID)
, m_startPhase(0.0f)
, m_bDisableVehicleImpactsWhilstDying(false)
, m_pNaturalMotionTask(NULL)
{
}

bool CEventDeath::AffectsPedCore(CPed* UNUSED_PARAM(pPed)) const
{
	//Assert(!pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ));
	// Death events will always be on local ped event lists so must affect this ped
	return true;
}

bool CEventDeath::CreateResponseTask(CPed* pPed, CEventResponse& response) const
{
	//Assert(!pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ));

	bool bClipOverride = false;
	s32 iFlags = 0;
	if( m_clipSetId != CLIP_SET_ID_INVALID && m_clipId != CLIP_ID_INVALID)
	{
		bClipOverride = true;
	}
	else if (!m_pNaturalMotionTask && !pPed->GetIsInVehicle())
	{
		iFlags |= CTaskDyingDead::Flag_startDead;
	}

	CTaskDyingDead* pTask = NULL;

	CEntity* inflictor = pPed->GetWeaponDamageEntity();
	if (NetworkInterface::IsGameInProgress())
	{
		inflictor = NetworkUtils::GetPedFromDamageEntity(pPed, pPed->GetWeaponDamageEntity());
	}

	CDeathSourceInfo info(inflictor, pPed->GetWeaponDamageHash());
	pTask = rage_new CTaskDyingDead(&info, iFlags, GetTimeOfDeath());

	if (m_pNaturalMotionTask)
	{
		pTask->SetForcedNaturalMotionTask(m_pNaturalMotionTask->Copy());
	}
	if (m_bUseRagdollFrame)
	{
		pTask->GoStraightToRagdollFrame();
	}
	if (m_bDisableVehicleImpactsWhilstDying)
	{
		pTask->DisableVehicleImpactsWhilstDying();
	}

	//! Set Clip set ID if we have provided one.
	if( bClipOverride )
	{
		pTask->SetDeathAnimationBySet(fwMvClipSetId(m_clipSetId), fwMvClipId(m_clipId), NORMAL_BLEND_DURATION, m_startPhase);
	}

	response.SetEventResponse(CEventResponse::EventResponse, pTask);
	return true;
}

CEventDeath::~CEventDeath()
{
	if (m_pNaturalMotionTask)
		delete m_pNaturalMotionTask;

	m_pNaturalMotionTask = NULL;
}

CEvent* CEventDeath::Clone() const {
	CEventDeath* pEvent = rage_new CEventDeath(m_bHasDrowned,m_iTimeOfDeath, m_bUseRagdoll);
	pEvent->SetUseRagdollFrame(m_bUseRagdollFrame);
	pEvent->SetClipSet(m_clipSetId, m_clipId, m_startPhase);
	pEvent->SetDisableVehicleImpactsWhilstDying(m_bDisableVehicleImpactsWhilstDying);
	if (m_pNaturalMotionTask)
		pEvent->SetNaturalMotionTask(static_cast<CTaskNMControl*>(m_pNaturalMotionTask->Copy()));
	return pEvent;
}


CEventWrithe::CEventWrithe(const CWeaponTarget& Inflictor, bool bFromGetUp)
	: m_Inflictor(Inflictor)
	, m_bFromGetUp(bFromGetUp)
{
	
}

CEventWrithe::~CEventWrithe()
{

}

bool CEventWrithe::AffectsPedCore(CPed* /*pPed*/) const
{
	return true;
}

bool CEventWrithe::CreateResponseTask(CPed* pPed, CEventResponse& response) const
{
	if (pPed && !pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_WRITHE))
	{
		CTaskWrithe* pTask = rage_new CTaskWrithe(m_Inflictor, m_bFromGetUp);
		response.SetEventResponse(CEventResponse::EventResponse, pTask);
	}

	return response.HasEventResponse();
}

CEventHurtTransition::CEventHurtTransition(CEntity* pTarget)
	: m_pTarget(pTarget)
{

}

CEventHurtTransition::~CEventHurtTransition()
{

}

bool CEventHurtTransition::AffectsPedCore(CPed* UNUSED_PARAM(pPed)) const
{
	return true;
}

bool CEventHurtTransition::CreateResponseTask(CPed* pPed, CEventResponse& out_response) const
{
	if( pPed && !pPed->HasHurtStarted() && !pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_TO_HURT_TRANSIT) )
	{
		CTaskToHurtTransit* pTask = rage_new CTaskToHurtTransit(m_pTarget);
		out_response.SetEventResponse(CEventResponse::PhysicalResponse, pTask);
	}

	return out_response.HasEventResponse();
}

#if CNC_MODE_ENABLED
int CEventIncapacitated::GetEventPriority() const
{
	return E_PRIORITY_INCAPACITATED;
}

bool CEventIncapacitated::CreateResponseTask(CPed* pPed, CEventResponse& response) const
{
	CTaskIncapacitated* pTask = rage_new CTaskIncapacitated();

	// If running an nm task that wants to continue on 'death' (incapacitation), keep using it.
	CTask* pTaskSimplest = pPed->GetPedIntelligence()->GetTaskActiveSimplest();
	if (pTaskSimplest->IsNMBehaviourTask())
	{
		CTaskNMBehaviour *pTaskNM = smart_cast<CTaskNMBehaviour*>(pTaskSimplest);
		if (pTaskNM->ShouldContinueAfterDeath())
		{
			nmEntityDebugf(pPed, "EventDamage::CreateResponseTask - Continuing nm task %s for incapacitation", pTaskNM->GetName().c_str());
			pTask->SetForcedNaturalMotionTask(rage_new CTaskNMControl(pTaskNM->GetMinTime(), pTaskNM->GetMaxTime(), pTaskNM->Copy(), CTaskNMControl::ALREADY_RUNNING));
		}
	}

	response.SetEventResponse(CEventResponse::EventResponse, pTask);

	return true;
}

CEvent* CEventIncapacitated::Clone() const
{
	CEventIncapacitated* pEvent = rage_new CEventIncapacitated();

	if (!pEvent)
	{
		return nullptr;
	}

	return pEvent;
}
#endif //CNC_MODE_ENABLED
//revert channel to default name
#undef __net_channel
#define __net_channel net
