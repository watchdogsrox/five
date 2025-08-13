
/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    player.cpp
// PURPOSE : Specific information for each player is stored here.
// AUTHOR :  Obbe
// CREATED : 3-11-99
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _PLAYER_H_
	#include "peds/PlayerInfo.h"
#endif

// Framework headers
#include "grcore/debugdraw.h"
#include "fwmaths/angle.h"
#include "fwmaths/Vector.h"
#include "fwscene/world/WorldLimits.h"

// Game headers
#include "ai/debug/system/AIDebugLogManager.h"
#include "ai/debug/types/VehicleDebugInfo.h"
#include "audio/emitteraudioentity.h"
#include "audio/frontendaudioentity.h"
#include "audio/vehicleaudioentity.h"
#include "camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/gameplay/aim/FirstPersonShooterCamera.h"
#include "camera/helpers/ControlHelper.h"
#include "camera/switch/SwitchDirector.h"
#include "control/gamelogic.h"
#include "control/remote.h"
#include "control/replay/replay.h"
#include "debug/debugscene.h"
#include "event/eventdamage.h"
#include "event/eventnetwork.h"
#include "event/events.h"
#include "event/eventshocking.h"
#include "event/shockingevents.h"
#include "frontend/loadingscreens.h"
#include "frontend/MobilePhone.h"
#include "frontend/NewHud.h"
#include "fwscene/search/searchvolumes.h"
#include "game/cheat.h"
#include "game/clock.h"
#include "game/crimeinformation.h"
#include "game/modelindices.h"
#include "game/weather.h"
#include "modelinfo/vehiclemodelinfo.h"
#include "network/objects/entities/netobjplayer.h"
#include "objects/object.h"
#include "peds/ped.h"
#include "peds/ped_channel.h"
#include "peds/pedcapsule.h"
#include "peds/peddebugvisualiser.h"
#include "peds/pedfactory.h"
#include "peds/pedgeometryanalyser.h"
#include "peds/pedintelligence.h"
#include "peds/pedmoveblend/pedmoveblendinwater.h"
#include "peds/pedtype.h"
#include "peds/queriableinterface.h"
#include "peds/rendering/pedvariationstream.h"
#include "peds/wildlifemanager.h"
#include "physics/physics.h"
#include "pickups/pickupmanager.h"
#include "renderer/zonecull.h"
#include "scene/EntityIterator.h"
#include "scene/world/gameworld.h"
#include "script/script.h"
#include "script/script_hud.h"
#include "stats/stats_channel.h"
#include "stats/statsinterface.h"
#include "stats/statsmgr.h"
#include "streaming/streaming.h"
#include "system/appcontent.h"
#include "system/control.h"
#include "system/control_channel.h"
#include "system/controlMgr.h"
#include "system/pad.h"
#include "task/combat/cover/taskcover.h"
#include "task/combat/combatmanager.h"
#include "task/combat/taskcombat.h"
#include "task/combat/taskdamagedeath.h"
#include "task/combat/tasksharkattack.h"
#include "task/default/taskplayer.h"
#include "task/general/tasksecondary.h"
#include "Task/General/Phone/TaskMobilePhone.h"
#include "task/motion/locomotion/taskmotionped.h"
#include "task/Movement/TaskParachute.h"
#include "task/Movement/TaskJetpack.h"
#include "task/Physics/TaskNMHighFall.h"
#include "task/Physics/TaskNMJumpRollFromRoadVehicle.h"
#include "task/vehicle/taskcar.h"
#include "task/vehicle/taskcarutils.h"
#include "task/vehicle/taskentervehicle.h"
#include "task/vehicle/taskinvehicle.h"
#include "task/vehicle/TaskMountAnimalWeapon.h"
#include "task/weapons/gun/TaskAimGunOnFoot.h"
#include "text/messages.h"
#include "text/text.h"
#include "vehicleai/pathfind.h"
#include "vehicleai/task/taskvehicletempaction.h"
#include "vehicleai/vehicleintelligence.h"
#include "vehicles/automobile.h"
#include "vehicles/bike.h"
#include "vehicles/boat.h"
#include "vehicles/heli.h"
#include "vehicles/metadata/vehiclelayoutinfo.h"
#include "vehicles/metadata/vehiclemetadatamanager.h"
#include "vehicles/metadata/vehicleseatinfo.h"
#include "vehicles/Planes.h"
#include "vehicles/submarine.h"
#include "vehicles/train.h"
#include "vehicles/vehicle.h"
#include "vehicles/vehiclegadgets.h"
#include "vfx/misc/fire.h"
#include "weapons/explosion.h"
#include "weapons/projectiles/projectilemanager.h"

// Parser headers
#include "Peds/PlayerInfo_parser.h"

//#include "zones.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

// we need one additional slot to account for GTAO on Xbox One where we have to add
// players to a temporary slot whilst we retrieve their display name 
// when we fully add the player, they are added fully before the temporary slot is removed
// so we have a maximum of our full complement of MAX_NUM_PHYSICAL_PLAYERS plus one slot to 
// account for the brief (same frame) period where one player is in the player pool twice
static const unsigned MAX_NUM_PLAYER_INFOS = MAX_NUM_PHYSICAL_PLAYERS + 1;

FW_INSTANTIATE_CLASS_POOL(CPlayerInfo, MAX_NUM_PLAYER_INFOS, atHashString("CPlayerInfo",0x3ef71716));

PARAM(invincible, "Make player invincible");

#define MIN_SPRINT_ENERGY	(-150.0f)

//
// Statics
CDynamicCoverHelper CPlayerInfo::ms_DynamicCoverHelper;
Vector3 CPlayerInfo::ms_cachedMainPlayerPos(0.0f,0.0f,0.0f);
bool CPlayerInfo::ms_bHasDisplayedPlayerQuitEnterCarHelpText=false;

#if !__FINAL
	bool CPlayerInfo::ms_bFakeWorldExtentsTresspassing = false;
#endif

//
// Constants
const float CPlayerInfo::ms_fInteriorShockingEventFreq = 2.0f;

// Melee
dev_u32 CPlayerInfo::ms_nMeleeClipSetReleaseDuration = 30000;

//
// Tune data
bank_float CPlayerInfo::ms_PlayerSoftAimBoundary = 0.78f;
#if FPS_MODE_SUPPORTED
bank_float CPlayerInfo::ms_PlayerFPSFireBoundaryLow = 0.1f;
bank_float CPlayerInfo::ms_PlayerFPSFireBoundaryHigh = 0.78f;
#endif

#if FPS_MODE_SUPPORTED && KEYBOARD_MOUSE_SUPPORT
bool CPlayerInfo::sm_bShouldUpdateScopeStateFromMenu = false;
#endif

bank_float CPlayerInfo::ms_PlayerLowAimBoundry = ioValue::ANALOG_BUTTON_DOWN_THRESHOLD; // roughly 10 in a 0-255 scale.
bank_bool CPlayerInfo::ms_ReverseAimBoundarys = false;

bank_float CPlayerInfo::ms_PlayerSoftTargetSwitchBoundary = 0.98f;

bank_float CPlayerInfo::ms_StealthNoiseExponentialDecayTimeConstant = 1.5f;
bank_float CPlayerInfo::ms_StealthNoiseMinValToCareAbout = 1.0f;

bank_float CPlayerInfo::ms_fDampenRootRate = 5.0f;
spdAABB CPlayerInfo::ms_WorldLimitsPlayerAABB = spdAABB();
bool CPlayerInfo::ms_bPlayerBoundaryInitialized = false;

// Make timers different when we are really far out (only do this for player)
// This means if we crash a plane / heli far out then we'll die before we have to swim all the way back.
dev_float PLAYER_SWIMMING_RANGE = 10000.0f;
dev_float PLAYER_MAX_TIME_IN_WATER_LONG_RANGE = 60.0f;

// Timer to avoid player getting stuck permanently hit by water cannon
dev_float PLAYER_MAX_TIME_HIT_BY_WATER_CANNON	= 20.0f;
dev_float PLAYER_WATER_CANNON_COOLDOWN_RATE	= 0.1f;		// Hit by timer decreases by rate * t every frame if > 0.0f
dev_float PLAYER_WATER_CANNON_IMMUNE_TIME		= 10.0f;

//
// Debug
#if !__FINAL
bool CPlayerInfo::ms_bDebugPlayerInvincible = false;
bool CPlayerInfo::ms_bDebugPlayerInvincibleRestoreHealth = false;
bool CPlayerInfo::ms_bDebugPlayerInvincibleRestoreArmorWithHealth = false;
bool CPlayerInfo::ms_bDebugPlayerInvisible = false;
bool CPlayerInfo::ms_bDebugPlayerInfiniteStamina = false;
#endif // !__FINAL
#if __ASSERT
bool CPlayerInfo::ms_ChangingPlayerModel = false;
#endif // __ASSERT
#if __BANK
bool CPlayerInfo::ms_bDisplayRecklessDriving = false;
bool CPlayerInfo::ms_bDisplayNumEnemiesInCombat = false;
bool CPlayerInfo::ms_bDisplayCoverTracking = false;
bool CPlayerInfo::ms_bDisplayCombatLoitering = false;
bool CPlayerInfo::ms_bDebugCombatLoitering = false;
bool CPlayerInfo::ms_bDisplayNumEnemiesShootingInCombat = false;
bool CPlayerInfo::ms_bDebugCandidateChargeGoalPositions = false;
bool CPlayerInfo::ms_bDebugDrawCandidateChargePositions = false;
#endif // __BANK


// Structure used in searches for closest cars/trains
struct FindClosestCarCBData{
	CPed*			pPed;
	CVehicle*		pVehicle;
	float			Closeness;
	Vector3			vSearchDir;
	bool			bStickInput;
};

// Structure used in searches for closest mounts
struct FindClosestRidableAnimalCBData{
	CPed*			pPed;
	CPed*			pAnimal;
	float			Closeness;
	Vector3			vSearchDir;
	bool			bStickInput;
	bool			bDepthCheck;
};

// Player health recharging
bool  CPlayerHealthRecharge::ms_bActivated = true;
float CPlayerHealthRecharge::ms_fRechargeSpeed = 4.0f;
float CPlayerHealthRecharge::ms_fRechargeSpeedWhileCrouchedOrCover = 8.0f;
float CPlayerHealthRecharge::ms_fTimeSinceDamageToStartRecharding = 5.0f;
float CPlayerHealthRecharge::ms_fTimeSinceDamageToStartRechardingCrouchedOrCover = 5.0f;

void CPlayerHealthRecharge::Process(CPed* pPed)
{
	// Dont process for network clones or if deactivated
	if( pPed->IsNetworkClone() || !CPlayerHealthRecharge::ms_bActivated)
	{
		return;
	}

	// If we're injured we can't recharge
	if( pPed->IsInjured() )
	{
		return;
	}

	// if getting taken down via melee, no recharge.
	if( pPed->IsDeadByMelee() )
	{
		return;
	}

	// If no health is needed, early out
	const float fMaxHealthToRestore = pPed->GetInjuredHealthThreshold() + ((pPed->GetMaxHealth() - pPed->GetInjuredHealthThreshold()) * m_fMaxHealthToRechargeAsPercentage);
	const float fHealthNeeded = fMaxHealthToRestore - pPed->GetHealth();
	if( fHealthNeeded <= 0.0f )
	{
		return;
	}

	// Dont recharge if underwater
	if( pPed->GetIsInWater() && pPed->GetCurrentMotionTask()->IsUnderWater() )
	{
		return;
	}

	// Dont recharge if on fire
	if( g_fireMan.IsEntityOnFire(pPed) )
	{
		return;
	}

	// Dont recharge if underwater in a vehicle
	if( pPed->GetPedIntelligence()->GetEventScanner()->GetInWaterEventScanner().IsDrowningInVehicle(*pPed) )
	{
		return;
	}

	// Dont recharge if we've been in the water for a long time and are slowly losing health
	if( pPed->GetPedIntelligence()->GetEventScanner()->GetInWaterEventScanner().IsTakingDamageDueToFatigue(*pPed) )
	{
		return;
	}

	// Dont recharge if we've disabled health regeneration while we are being electrocuted by WEAPON_STUNGUN_MP
	if( pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DisableHealthRegenerationWhenStunned) && pPed->GetPedResetFlag(CPED_RESET_FLAG_BeingElectrocuted) && NetworkInterface::IsGameInProgress())
	{
		return;
	}

	// Work out if we are crouched and stationary.
	bool bStill = false;
	bool bCrouched = false;
	if( pPed->GetIsOnFoot() && pPed->GetPrimaryMotionTask() )
	{
		Vector2 vMBR;
		pPed->GetMotionData()->GetCurrentMoveBlendRatio(vMBR);
		bStill = vMBR.Mag2() < rage::square(MBR_WALK_BOUNDARY);
		bCrouched = pPed->GetIsCrouching();
	}

	// Work out if we are idle in cover
	bool bInCoverIdle = (pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_IN_COVER) &&
						pPed->GetPedIntelligence()->GetQueriableInterface()->GetStateForTaskType(CTaskTypes::TASK_IN_COVER) == CTaskInCover::State_Idle );

	// Work out if we are idle in cover
	const bool bStandingStillOrInCover = bStill || bInCoverIdle;
	const bool bInVehicleOrOnMount = pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) || pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_OnMount );
	const bool bCrouchedOrInCover = bCrouched || bInCoverIdle;

	// Cant recharge if moving
	if( !bStandingStillOrInCover && !bInVehicleOrOnMount )
	{
		return;
	}

	// Work out the recharge variables based on our current state
	const float fTimeSinceDamageToRecharge = bCrouchedOrInCover ? CPlayerHealthRecharge::ms_fTimeSinceDamageToStartRechardingCrouchedOrCover : CPlayerHealthRecharge::ms_fTimeSinceDamageToStartRecharding;
	float fRechargeSpeed = bCrouchedOrInCover ? CPlayerHealthRecharge::ms_fRechargeSpeedWhileCrouchedOrCover : CPlayerHealthRecharge::ms_fRechargeSpeed;

	fRechargeSpeed *= GetRechargeScriptMultiplier();

	const CPedModelInfo* mi = pPed->GetPedModelInfo();
	Assert(mi);
	const CPedModelInfo::PersonalityData& pd = mi->GetPersonalitySettings();

	fRechargeSpeed *= pd.GetHealthRegenEfficiency();

	u32 uLastDamageTime = pPed->GetWeaponDamagedTime();
	if (NetworkInterface::IsInCopsAndCrooks() && m_uLastTimeHealthDamageTaken > 0)
	{
		uLastDamageTime = m_uLastTimeHealthDamageTaken;
	}

	float fTimeSinceLastDamage = ((float)fwTimer::GetTimeInMilliseconds() - uLastDamageTime)/1000.0f;

	// Only start recharging a certain amount of time after being damaged
	if( fTimeSinceLastDamage > fTimeSinceDamageToRecharge && !pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_DISABLE_HEALTH_RECHARGE))
	{
		// Clamp the health increase so we dont recharge over the maximum amount
		float fHealthIncrease = fRechargeSpeed*fwTimer::GetTimeStep();
		fHealthIncrease = Clamp(fHealthIncrease, 0.0f, fHealthNeeded);

		// Apply the health increase
		pPed->ChangeHealth(fHealthIncrease);
	}
}

void CPlayerHealthRecharge::NotifyHealthDamageTaken()
{
	m_uLastTimeHealthDamageTaken = fwTimer::GetTimeInMilliseconds();
}

void CPlayerEnduranceManager::Process(CPed* CNC_MODE_ENABLED_ONLY(pPed))
{
#if CNC_MODE_ENABLED
	if(pPed->GetEndurance() > 0.0f)
	{
		ProcessRegen(pPed);
	}
	else if(CPlayerInfo::ms_Tunables.m_EnduranceManagerSettings.m_CanEnduranceIncapacitate && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_CanBeIncapacitated))
	{
		//Kick off incapacitation event on owner if we're not running it already
		const CTask* incapacitationTask = pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_INCAPACITATED);
		if(!pPed->IsNetworkClone() && incapacitationTask == nullptr)
		{
			if(!pPed->GetPedIntelligence()->HasEventOfType(EVENT_INCAPACITATED))
			{
				const CEventIncapacitated eventIncapacitated;
				pPed->GetPedIntelligence()->AddEvent(eventIncapacitated);
			}
		}
	}
	ProcessEnduranceOverlayFX(pPed);
#endif //#if CNC_MODE_ENABLED
}

void CPlayerEnduranceManager::NotifyEnduranceDamageTaken()
{
	m_uLastTimeEnduranceDamageTaken = fwTimer::GetTimeInMilliseconds();
}

void CPlayerEnduranceManager::NotifyEnduranceRecovered()
{
	m_uLastTimeEnduranceRecovered = fwTimer::GetTimeInMilliseconds();
}

bool CPlayerEnduranceManager::ShouldIgnoreEnduranceDamage() const
{
#if CNC_MODE_ENABLED
	const int fTimeSinceRecovered = fwTimer::GetTimeInMilliseconds() - m_uLastTimeEnduranceRecovered;
	return fTimeSinceRecovered < CTaskIncapacitated::GetTimeSinceRecoveryToIgnoreEnduranceDamage();
#endif

	return true;
}

void CPlayerEnduranceManager::ProcessRegen(CPed* pPed)
{
	//Early out if at max
	if (pPed->GetEndurance() >= pPed->GetMaxEndurance())
	{
		return;
	}

	//Do not regen if hit recently
	const int timeSinceLastDamage = fwTimer::GetTimeInMilliseconds() - m_uLastTimeEnduranceDamageTaken;
	if (timeSinceLastDamage < CPlayerInfo::GetEnduranceRegenDamageCooldown())
	{
		return;
	}

	const float fDeltaTime = fwTimer::GetTimeStep();
	const float fMaxRegenTime = CPlayerInfo::GetEnduranceMaxRegenTime() / 1000.0f;
	const float fRegenRate = fMaxRegenTime > SMALL_FLOAT ? pPed->GetMaxEndurance() / fMaxRegenTime : 0.0f;
	
	float fRegenMod = 1.0f;
	// check for arcade ability effect
	if ((pPed->GetPlayerInfo() != nullptr) && (pPed->GetPlayerInfo()->GetArcadeAbilityEffects().GetIsActive(AAE_SECOND_WIND)))
	{
		fRegenMod = pPed->GetPlayerInfo()->GetArcadeAbilityEffects().GetModifier(AAE_SECOND_WIND);
	}

	pPed->SetEndurance(pPed->GetEndurance() + fRegenRate * fRegenMod * fDeltaTime);
}


void CPlayerEnduranceManager::ProcessEnduranceOverlayFX(CPed* pPed)
{
	//Early out if the ped cannot be incapacitated (e.g. is a player on the C&C cop team or a simple ped)
	if (!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_CanBeIncapacitated) || !CPlayerInfo::ms_Tunables.m_EnduranceManagerSettings.m_CanEnduranceIncapacitate)
	{
		return;
	}
	
	if (scriptVerifyf(pPed->GetPlayerInfo(), "Player with no player info!"))
	{
		bool bAreSpecialAbilityFXRunning = false;
		for (int i = 0; i < PSAS_MAX; ++i)
		{
			if (CPlayerSpecialAbility* pSpecialAbility = pPed->GetSpecialAbility((ePlayerSpecialAbilitySlot)i))
			{
				if (pSpecialAbility->IsPlayingFx())
				{
					bAreSpecialAbilityFXRunning = true;
					break;
				}
			}
		}
	
		float EndurancePercentage = pPed->GetEndurance() / pPed->GetMaxEndurance();

		// If a special ability FX are running, stop all endurance related FX
		if (bAreSpecialAbilityFXRunning || pPed->GetArrestState() != ArrestState_None || pPed->IsDead())
		{
			if (ANIMPOSTFXMGR.IsRunning(ATSTRINGHASH("CnC_LowEndurance", 0x3A8D7228)))
			{
				ANIMPOSTFXMGR.Stop(ATSTRINGHASH("CnC_LowEndurance", 0x3A8D7228), AnimPostFXManager::kDefault);
			}

			if (ANIMPOSTFXMGR.IsRunning(ATSTRINGHASH("CnC_LowEnduranceOut", 0xFDEAAEE6)))
			{
				ANIMPOSTFXMGR.Stop(ATSTRINGHASH("CnC_LowEnduranceOut", 0xFDEAAEE6), AnimPostFXManager::kDefault);
			}
		}
		else if (EndurancePercentage >= 0.5)
		{
			if (ANIMPOSTFXMGR.IsRunning(ATSTRINGHASH("CnC_LowEndurance", 0x3A8D7228)) && !ANIMPOSTFXMGR.IsRunning(ATSTRINGHASH("CnC_LowEnduranceOut", 0xFDEAAEE6)))
			{
				// DStop Endurance effect and start it's outro
				ANIMPOSTFXMGR.Stop(ATSTRINGHASH("CnC_LowEndurance", 0x3A8D7228), AnimPostFXManager::kDefault);
				ANIMPOSTFXMGR.Start(ATSTRINGHASH("CnC_LowEnduranceOut", 0xFDEAAEE6), 0U, false, false, false, 0U, AnimPostFXManager::kDefault);
			}
		}
		else
		{
			if (!ANIMPOSTFXMGR.IsRunning(ATSTRINGHASH("CnC_LowEndurance", 0x3A8D7228)))
			{
				ANIMPOSTFXMGR.Start(ATSTRINGHASH("CnC_LowEndurance", 0x3A8D7228), 0U, false, false, false, 0U, AnimPostFXManager::kDefault);
			}
		}
	}		
}

////////////////////////////////////////////////////////////////////////////////

void CPlayerDamageOverTime::Process(CPed* pPed)
{	
	if (!pPed || !pPed->IsLocalPlayer() || m_DOTEffects.IsEmpty())
	{
		return;
	}

	u32 uCurrentTime = fwTimer::GetTimeInMilliseconds();

	int iCount = m_DOTEffects.GetCount();
	for(int i = 0; i < iCount; i++)
	{
		const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_DOTEffects[i].m_uWeaponHash);
		if (pWeaponInfo)
		{
			bool bApplyDamage = false;
			bool bExpired = false;

			u32 uTickRate = (u32)(pWeaponInfo->GetTimeBetweenShots() * 1000.0f);
			u32 uTimeSinceLastDamage = uCurrentTime - m_DOTEffects[i].m_uLastDamageTime;

			if (uCurrentTime >= m_DOTEffects[i].m_uExpiryTime)
			{
				bApplyDamage = true;
				bExpired = true;
			}
			else if (uTimeSinceLastDamage >= uTickRate)
			{
				bApplyDamage = true;
			}

			if (bApplyDamage)
			{
				float fHealthDamagePerSecond = pWeaponInfo->GetDamage();
				float fEnduranceDamagePerSecond = pWeaponInfo->GetEnduranceDamage();
				float fSecondsSinceLastDamage = (float)uTimeSinceLastDamage / 1000.0f;

				// Send as two individual events (only way to apply raw damage values on both health and endurance)
				if (fHealthDamagePerSecond > 0.0f)
				{
					float fScaledHealthDamage = fHealthDamagePerSecond * fSecondsSinceLastDamage;
					CWeaponDamage::GeneratePedDamageEvent(m_DOTEffects[i].m_LatestInflicter, pPed, m_DOTEffects[i].m_uWeaponHash, fScaledHealthDamage, VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), NULL, CPedDamageCalculator::DF_HealthDamageOnly);
				}
				if (fEnduranceDamagePerSecond > 0.0f)
				{
					float fScaledEnduranceDamage = fEnduranceDamagePerSecond * fSecondsSinceLastDamage;
					CWeaponDamage::GeneratePedDamageEvent(m_DOTEffects[i].m_LatestInflicter, pPed, m_DOTEffects[i].m_uWeaponHash, fScaledEnduranceDamage, VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), NULL, CPedDamageCalculator::DF_EnduranceDamageOnly);
				}

				m_DOTEffects[i].m_uLastDamageTime = uCurrentTime;
			}

			if (bExpired)
			{
				m_DOTEffects.Delete(i);
				iCount--;
				i--;
			}
			else
			{
				ProcessAdditionalEffects(pPed, pWeaponInfo);
			}
		}
	}
}

void CPlayerDamageOverTime::Start(u32 uWeaponHash, CPed* pVictimPed, CEntity* pInflicterEntity)
{
	// Check params
	if(!pedVerifyf(uWeaponHash != 0, "CPlayerDamageOverTime::StartEffect - No weapon hash set for damage over time, not applying"))
	{
		return;
	}
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(uWeaponHash);
	if(pWeaponInfo == NULL)
	{
		return;
	}
	if(!pedVerifyf(pVictimPed != NULL, "CPlayerDamageOverTime::StartEffect - Invalid victim pointer, not applying"))
	{
		return;
	}
	if(!pedVerifyf(pInflicterEntity != NULL, "CPlayerDamageOverTime::StartEffect - Invalid inflictor pointer, not applying"))
	{
		return;
	}

	CPed* pInflicterPed = NULL;
	if (pInflicterEntity->GetIsTypePed())
	{
		pInflicterPed = static_cast<CPed*>(pInflicterEntity);
	}
	else
	{
		// Ignoring anything that isn't a ped inflicter (for now)
		return;
	}

	// Validate data
	if(!pedVerifyf(pWeaponInfo->GetDamageTime() > 0.0f, "CPlayerDamageOverTime::StartEffect - DamageTime invalid for %s, not applying", pWeaponInfo->GetName()))
	{
		return;
	}
	if(!pedVerifyf(pWeaponInfo->GetTimeBetweenShots() > 0.0f, "CPlayerDamageOverTime::StartEffect - TimeBetweenShots invalid for %s, not applying", pWeaponInfo->GetName()))
	{
		return;
	}
	if(!pedVerifyf(pWeaponInfo->GetDamage() > 0.0f || pWeaponInfo->GetEnduranceDamage() > 0.0f, "CPlayerDamageOverTime::StartEffect - %s has no damage value set, not applying", pWeaponInfo->GetName()))
	{
		return;
	}

	CPlayerDamageOverTime::DOTEffect effect;
	effect.m_uWeaponHash = uWeaponHash;
	effect.m_uLastDamageTime = fwTimer::GetTimeInMilliseconds();
	effect.m_uExpiryTime = effect.m_uLastDamageTime + (u32)(pWeaponInfo->GetDamageTime() * 1000.0f);
	effect.m_OriginalInflicter = pInflicterPed;
	effect.m_LatestInflicter = pInflicterPed;

	bool bAddNewEntry = true;

	if (pWeaponInfo->GetDamageOverTimeAllowStacking())
	{
		int iExistingCount = 0;
	
		for(int i = 0; i < m_DOTEffects.GetCount(); i++)
		{
			if (m_DOTEffects[i].m_uWeaponHash == uWeaponHash)
			{
				// If an entry already exists for weapon/inflicter, extend duration instead of adding new one
				if(m_DOTEffects[i].m_OriginalInflicter == effect.m_OriginalInflicter)
				{
					m_DOTEffects[i].m_uExpiryTime = effect.m_uExpiryTime;
					bAddNewEntry = false;
				}

				// Set new inflicter as 'latest' on all existing of this weapon type, so we attribute death/incapacitation to last person who applied effect
				m_DOTEffects[i].m_LatestInflicter = effect.m_OriginalInflicter;

				iExistingCount++;
			}
		}

		// Enforce a stacking limit for the number of different inflicters
		if (iExistingCount >= MAX_STACK)
		{
			bAddNewEntry = false;
		}
	}
	else
	{
		// Find and extend the duration of an existing effect instead of adding new one
		for(int i = 0; i < m_DOTEffects.GetCount(); i++)
		{
			if (m_DOTEffects[i].m_uWeaponHash == uWeaponHash)
			{
				m_DOTEffects[i].m_uExpiryTime = effect.m_uExpiryTime;
				bAddNewEntry = false;

				// Can early out, there's only ever going to be one
				break;
			}
		}
	}

	if(bAddNewEntry && pedVerifyf(!m_DOTEffects.IsFull(), "CPlayerDamageOverTime::StartEffect - Full! Increase array size MAX_ENTRIES to match potential (currently %i)", m_DOTEffects.GetMaxCount()))
	{
		m_DOTEffects.Push(effect);
	}

	// Do anything we need to do when an effect is applied/extended
	StartAdditionalEffects(pVictimPed, pWeaponInfo);
}

void CPlayerDamageOverTime::Stop(u32 uWeaponHash)
{
	// Remove all entries matching this weapon hash
	int iCount = m_DOTEffects.GetCount();
	for(int i = 0; i < iCount; i++)
	{
		if (m_DOTEffects[i].m_uWeaponHash == uWeaponHash)
		{
			m_DOTEffects.Delete(i);
			iCount--;
			i--;
		}
	}
}

void CPlayerDamageOverTime::StopAll()
{
	// Remove all entries
	for(int i = 0; i < m_DOTEffects.GetCount(); i++)
	{
		m_DOTEffects.Delete(i);
	}
}

// Note - It's preferable to check active Player Reset Flags instead of using this function!
bool CPlayerDamageOverTime::IsActive(u32 uWeaponHash)
{
	for(int i = 0; i < m_DOTEffects.GetCount(); i++)
	{
		if (m_DOTEffects[i].m_uWeaponHash == uWeaponHash)
		{
			return m_DOTEffects[i].m_uExpiryTime < fwTimer::GetTimeInMilliseconds();
		}
	}
	return false;
}

#if AI_DEBUG_OUTPUT_ENABLED
atString CPlayerDamageOverTime::GetDebugString(s32 iEffectIndex)
{
	if(pedVerifyf(iEffectIndex >= 0 && iEffectIndex < m_DOTEffects.GetCount(), "CPlayerDamageOverTime::GetDebugStringForEffect - Bad index!"))
	{
		static const u32 uBUFFER_SIZE = 256;
		static char strBuffer[uBUFFER_SIZE];

		const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_DOTEffects[iEffectIndex].m_uWeaponHash);
		if (pWeaponInfo)
		{
			const float fTimeLeft = (float)(m_DOTEffects[iEffectIndex].m_uExpiryTime - fwTimer::GetTimeInMilliseconds()) / 1000.0f;
			const float fTimeSinceLastTick = (float)(fwTimer::GetTimeInMilliseconds() - m_DOTEffects[iEffectIndex].m_uLastDamageTime) / 1000.0f;
			const float fTimeUntilNextTick = pWeaponInfo->GetTimeBetweenShots() - fTimeSinceLastTick;
			const CPed* pInflictor = m_DOTEffects[iEffectIndex].m_OriginalInflicter;
			const char* inflictorName = pInflictor ? (pInflictor->GetNetworkObject() ? pInflictor->GetNetworkObject()->GetLogName() : pInflictor->GetModelName()) : "NULL";

			snprintf(strBuffer, uBUFFER_SIZE, "  %s, %s, TimeLeft:%.2f, NextTick:%.2f, DPS:%.2fH/%.2fE", pWeaponInfo->GetName(), inflictorName, fTimeLeft, fTimeUntilNextTick, pWeaponInfo->GetDamage(), pWeaponInfo->GetEnduranceDamage());
			return atString(strBuffer);
		}
	}
	
	return atString("Invalid");
}
#endif //AI_DEBUG_OUTPUT_ENABLED

void CPlayerDamageOverTime::StartAdditionalEffects(CPed* pPed, const CWeaponInfo* pWeaponInfo)
{
	if (!pPed || !pWeaponInfo)
	{
		return;
	}

	// Do one-shot things when effect is applied / refreshed
	if (pWeaponInfo->GetDamageOverTimeShockedEffect())
	{
		//TODO
	}

	if (pWeaponInfo->GetDamageOverTimeChokingEffect())
	{
		//TODO
	}
}

void CPlayerDamageOverTime::ProcessAdditionalEffects(CPed* pPed, const CWeaponInfo* pWeaponInfo)
{
	if (!pPed || !pWeaponInfo)
	{
		return;
	}

	// Do things every frame while effect is active
	if (pWeaponInfo->GetDamageOverTimeShockedEffect())
	{
		pPed->GetPlayerInfo()->GetPlayerResetFlags().SetFlag(CPlayerResetFlags::PRF_DOT_SHOCKED);
	}

	if (pWeaponInfo->GetDamageOverTimeChokingEffect())
	{
		pPed->GetPlayerInfo()->GetPlayerResetFlags().SetFlag(CPlayerResetFlags::PRF_DOT_SHOCKED);

	}
}

////////////////////////////////////////////////////////////////////////////////

#if __BANK
void CPlayerInfo::sPlayerStatInfo::PreAddWidgets(bkBank& bank)
{
	bank.PushGroup(m_Name.GetCStr(), false);
}

void CPlayerInfo::sPlayerStatInfo::PostAddWidgets(bkBank& bank)
{
	bank.PopGroup();
}
#endif // __BANK

////////////////////////////////////////////////////////////////////////////////

const CPlayerInfo::sPlayerStatInfo& CPlayerInfo::GetPlayerStatInfoForPed(const CPed& ped)
{
	if (NetworkInterface::IsGameInProgress())
	{
		aiDebugf2("MP SLOT : %i", StatsInterface::GetCurrentMultiplayerCharaterSlot());

		//Team is stored in the first 8 bits of this stat.
		s32 iTeam = 0;
		if (aiVerify(StatsInterface::GetMaskedInt(STAT_PACKED_STAT_INT0.GetStatId(), iTeam, 0, 8, -1)))
		{
			switch (iTeam)
			{
			case MP0_CHAR_TEAM: return ms_Tunables.m_PlayerStatInfos[PT_MP0_CHAR_TEAM];
			case MP1_CHAR_TEAM: return ms_Tunables.m_PlayerStatInfos[PT_MP1_CHAR_TEAM];
			case MP2_CHAR_TEAM: return ms_Tunables.m_PlayerStatInfos[PT_MP2_CHAR_TEAM];
			case MP3_CHAR_TEAM: return ms_Tunables.m_PlayerStatInfos[PT_MP3_CHAR_TEAM];
			case MP4_CHAR_TEAM: return ms_Tunables.m_PlayerStatInfos[PT_MP4_CHAR_TEAM];
			default: break;
			}
		}

		return ms_Tunables.m_PlayerStatInfos[PT_MP0_CHAR_TEAM];
	}
	else
	{
		switch (ped.GetPedType())
		{
			case PEDTYPE_PLAYER_0: return ms_Tunables.m_PlayerStatInfos[PT_SP0];
			case PEDTYPE_PLAYER_1: return ms_Tunables.m_PlayerStatInfos[PT_SP1];
			case PEDTYPE_PLAYER_2: return ms_Tunables.m_PlayerStatInfos[PT_SP2];
			default: break;
		}

		return ms_Tunables.m_PlayerStatInfos[PT_SP0]; 
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CPlayerInfo::GetStatValuesForSprintType(const CPed& ped, float& fSprintStatValue, float& fMinSprintDuration, float& fMaxSprintDuration, eSprintType nSprintType)
{
	switch (nSprintType)
	{
		// Intentional fall-thru
		case SPRINT_ON_FOOT:	
		case SPRINT_ON_BICYCLE:	
		case SPRINT_ON_WATER:		
		case SPRINT_UNDER_WATER:	
			{
				fSprintStatValue   = rage::Clamp(static_cast<float>(StatsInterface::GetIntStat(STAT_STAMINA.GetStatId())) / 100.0f, 0.0f, 1.0f);
				fMinSprintDuration = GetPlayerStatInfoForPed(ped).m_MinStaminaDuration;
				fMaxSprintDuration = GetPlayerStatInfoForPed(ped).m_MaxStaminaDuration;
				return true;
			}
		default: break;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

const float CPlayerInfo::GetRunSprintSpeedMultiplier() const
{
	const CPlayerSpecialAbility* pAbility = m_pPlayerPed ? m_pPlayerPed->GetSpecialAbility() : NULL;

	if (pAbility != NULL)
	{
		// We only want to apply this when we're actually sprinting
		if (pAbility->IsActive() && m_pPlayerPed->GetMotionData()->GetIsSprinting())
		{
			float fAbilityMultiplier = pAbility->GetSprintMultiplier();
			if (fAbilityMultiplier > 0.0f)
			{
				return fAbilityMultiplier;
			}
		}
	}

	return m_fRunSprintSpeedMultiplier;
}

////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
PARAM(enableCNCResponsivenessChanges, "[CNC] Enable CNC responsiveness changes for player.");
PARAM(forceEnableCNCResponsivenessChanges, "[CNC] Force enable CNC responsiveness changes for player, even if not in CNC mode.");
#endif	// !__FINAL

bool CPlayerInfo::AreCNCResponsivenessChangesEnabled(const CPed* CNC_MODE_ENABLED_ONLY(pPed))
{
#if CNC_MODE_ENABLED

	if (!pPed || !pPed->IsPlayer())
	{
		return false;
	}

#if !__FINAL
	if (PARAM_forceEnableCNCResponsivenessChanges.Get())
	{
		return true;
	}
#endif // !__FINAL

	if (NetworkInterface::IsInCopsAndCrooks())
	{
		TUNE_GROUP_BOOL(CNC_RESPONSIVENESS, bEnableCNCResponsivenessChanges, true);
		if (bEnableCNCResponsivenessChanges)
		{
			return true;
		}

#if !__FINAL
		if (PARAM_enableCNCResponsivenessChanges.Get())
		{
			return true;
		}
#endif	// !__FINAL
	}

#endif	// CNC_MODE_ENABLED

	return false;
}

////////////////////////////////////////////////////////////////////////////////

s32 CPlayerInfo::GetPlayerIndex(const CPed& ped)
{
	switch (ped.GetPedType())
	{
		case PEDTYPE_PLAYER_0 : return 0;
		case PEDTYPE_PLAYER_1 : return 1;
		case PEDTYPE_PLAYER_2 : return 2;
		default: break;
	}
	return -1;
}

////////////////////////////////////////////////////////////////////////////////

s32 CPlayerInfo::ms_EnduranceMaxRegenTime = -1;
s32 CPlayerInfo::ms_EnduranceRegenDamageCooldown = -1;
float CPlayerInfo::ms_fStaminaDepletionRateModifierCNC = 1.25f;
float CPlayerInfo::ms_fStaminaReplenishRateModifierCNC = 0.75f;

void CPlayerInfo::InitTunables()
{
	ms_EnduranceMaxRegenTime = ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("CNC_ENDURANCE_MAX_REGEN_TIME", 0xBDA9083B), -1);
	ms_EnduranceRegenDamageCooldown = ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("CNC_ENDURANCE_REGEN_DAMAGE_COOLDOWN", 0xEC265896), -1);
	ms_fStaminaDepletionRateModifierCNC = ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("CNC_STAMINA_DEPLETION_RATE_MODIFIER", 0xa7ba62ee), ms_fStaminaDepletionRateModifierCNC);
	ms_fStaminaReplenishRateModifierCNC = ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("CNC_STAMINA_REPLENISH_RATE_MODIFIER", 0x3cec399e), ms_fStaminaReplenishRateModifierCNC);
}

u32 CPlayerInfo::GetEnduranceMaxRegenTime()
{
	return ms_EnduranceMaxRegenTime != -1 ? (u32)ms_EnduranceMaxRegenTime : ms_Tunables.m_EnduranceManagerSettings.m_MaxRegenTime;
}

u32 CPlayerInfo::GetEnduranceRegenDamageCooldown()
{
	return ms_EnduranceRegenDamageCooldown != -1 ? (u32)ms_EnduranceRegenDamageCooldown : ms_Tunables.m_EnduranceManagerSettings.m_RegenDamageCooldown;
}

////////////////////////////////////////////////////////////////////////////////

float CPlayerInfo::GetMaxTimeUnderWaterForPed(const CPed& ped)
{
	float fLungStatValue = 0.0f;

	TUNE_GROUP_BOOL(SPRINT_DEBUG, USE_SIMULATED_LUNG_STAT, false);
	if (USE_SIMULATED_LUNG_STAT)
	{
		TUNE_GROUP_FLOAT(SPRINT_DEBUG, LUNG_STAT_SIMULATED, 0.0f, 0.0f, 1.0f, 0.01f);
		fLungStatValue = LUNG_STAT_SIMULATED;
	}
	else if (ped.GetPedModelInfo())
	{
		fLungStatValue = rage::Clamp(static_cast<float>(StatsInterface::GetIntStat(STAT_LUNG_CAPACITY.GetStatId())) / 100.0f, 0.0f, 1.0f);
	}
	return ComputeDurationFromStat(fLungStatValue, GetPlayerStatInfoForPed(ped).m_MinHoldBreathDuration, GetPlayerStatInfoForPed(ped).m_MaxHoldBreathDuration); 
}

////////////////////////////////////////////////////////////////////////////////

float CPlayerInfo::GetFallHeightForPed(const CPed& ped)
{
	// Script overridden fall height
	CPlayerInfo* pPlayerInfo = ped.GetPlayerInfo();
	if (pPlayerInfo && pPlayerInfo->m_fFallHeightOverride >= 0.0f)
	{
		return pPlayerInfo->m_fFallHeightOverride;
	}

	float fStrengthValue = 0.0f;

	if (ped.GetPedModelInfo())
	{
		fStrengthValue = rage::Clamp(static_cast<float>(StatsInterface::GetIntStat(STAT_STRENGTH.GetStatId())) / 100.0f, 0.0f, 1.0f);
	}

	float fMin = GetPlayerStatInfoForPed(ped).m_MinFallHeight;
	float fMax = GetPlayerStatInfoForPed(ped).m_MaxFallHeight;
	return fMin + ((fMax - fMin)*fStrengthValue);
}

////////////////////////////////////////////////////////////////////////////////

float CPlayerInfo::GetDiveHeightForPed(const CPed& ped)
{
	float fStrengthValue = 0.0f;

	if (ped.GetPedModelInfo())
	{
		fStrengthValue = rage::Clamp(static_cast<float>(StatsInterface::GetIntStat(STAT_LUNG_CAPACITY.GetStatId())) / 100.0f, 0.0f, 1.0f);
	}

	//! Infinite if maxed out stat.
	if(fStrengthValue >= 1.0f)
	{
		return FLT_MAX;
	}

	//! ramp stat
	fStrengthValue = powf(fStrengthValue, GetPlayerStatInfoForPed(ped).m_DiveRampPow);
	
	float fMin = GetPlayerStatInfoForPed(ped).m_MinDiveHeight;
	float fMax = GetPlayerStatInfoForPed(ped).m_MaxDiveHeight;
	return fMin + ((fMax - fMin)*fStrengthValue);
}

////////////////////////////////////////////////////////////////////////////////

float CPlayerInfo::ComputeDurationFromStat(float fCurrentStatPercentage, float fMinDuration, float fMaxDuration)
{
	TUNE_GROUP_BOOL(SPRINT_DEBUG, USE_STAMINA_STAT_SIMULATED, false);
	if (USE_STAMINA_STAT_SIMULATED)
	{
		TUNE_GROUP_FLOAT(SPRINT_DEBUG, STAMINA_STAT_SIMULATED, 0.0f, 0.0f, 1.0f, 0.01f);
		fCurrentStatPercentage = STAMINA_STAT_SIMULATED;
	}
	aiAssertf(fCurrentStatPercentage >= 0.0f && fCurrentStatPercentage <= 1.0f, "Expected fCurrentStatPercentage to be between 0 and 1");
	return (1.0f - fCurrentStatPercentage) * fMinDuration + fCurrentStatPercentage * fMaxDuration;
}

////////////////////////////////////////////////////////////////////////////////

CPlayerInfo::CPlayerInfo(CPed* ped, const rlGamerInfo* gamerInfo) :
m_pPlayerPed(ped),
m_pOnlyEnterThisVehicle(NULL),
m_pPreferRearSeatsVehicle(NULL),
m_pPreferFrontPassengerSeatVehicle(NULL),
m_pSpotterOfStolenVehicle(NULL),
Team(-1),
m_PlayerState(PLAYERSTATE_PLAYING),
CarDensityForCurrentZone(0),
//bAfterRemoteVehicleExplosion(false),
//bCreateRemoteVehicleExplosion(false),
//bFadeAfterRemoteVehicleExplosion(false),
m_fForceAirDragMult(0.0f),
m_fSwimSpeedMultiplier(1.f),
m_fRunSprintSpeedMultiplier(1.f),
CollectablesPickedUp(0),
TotalNumCollectables(3),
PortablePickupPending(false),
m_LastTimeHeliWasDestroyed(0),
LastTimeEnergyLost(0),
LastTimeArmourLost(0),
//TimesUpsideDownInARow(0),
//TimesStuckInARow(0),
bDoesNotGetTired(false),
bFastReload(false),
bFireProof(false),
MaxHealth(PLAYER_DEFAULT_MAX_HEALTH),
MaxArmour(PLAYER_DEFAULT_MAX_ARMOUR),
JackSpeed(NETWORK_PLAYER_DEFAULT_JACK_SPEED),
DetonateDownTime(0),
bExplodedTriggeredProjectilesWithThisButtonPress(false),
m_bIsShelteredAbove(false),
m_iTimeTillNextShelterLineTest(0),
m_iNumEnemiesInCombat(0),
m_iNumEnemiesShootingInCombat(0),
#if LIGHT_EFFECTS_SUPPORT
m_PlayerLightEffect(),
m_WantedLightEffect(),
#if LIGHT_EFFECT_FLASH_FOR_LOW_HEALTH
m_FlashingPlayerLightEffect(),
#endif // LIGHT_EFFECT_FLASH_FOR_LOW_HEALTH
#endif // LIGHT_EFFECT_SUPPORT
bCanDoDriveBy(true),
bCanBeHassledByGangs(false),
bCanUseCover(true),
bEnableControlOnDeath(true),
bHasDamagedAtLeastOnePed(false),
bHasDamagedAtLeastOneNonAnimalPed(false),
bHasDamagedAtLeastOneVehicle(false),
bSpecifyInitialCoverDirection(false),
bInitialCoverDirectionFaceLeft(false),
HavocCaused(0),
m_fLastPlayerNearbyTimer(0.0f),
StealthRate(1.f),
m_nLastBustMessageNumber(1),		
m_bCoverGeneratedByDynamicEntity(false),
m_bCanMoveLeft(false),
m_bCanMoveRight(false),
m_bInsideCorner(false),
m_bIsRoundCover(false),
m_fInsideCornerDistance(-1.0f),
m_vRoundCoverCenter(Vector3::ZeroType),
m_fRoundCoverRadius(0),
m_fCoverEdgeDistance(-1.0f),
m_vDynamicCoverPointLastKnownPosition(Vector3::ZeroType),
m_fTimeElapsedUsingCoverSeconds(0.0f),
m_fTimeElapsedHidingInCoverSeconds(0.0f),
m_CandidateRegistrationEndTimeMS(0),
m_BestCandidateAcceptanceEndTimeMS(0),
m_BestEnemyChargerCandidatePed(NULL),
m_BestEnemySmokeThrowerCandidatePed(NULL),
m_vChargeGoalPlayerPosition(V_ZERO),
m_vCombatLoiteringPosition(V_ZERO),
m_fTimeElapsedLoiteringSeconds(-1.0f),
m_uNextLoiterDistCheckTimeMS(0),
m_aPlayerLOSInfo(MAX_NUM_PHYSICAL_PLAYERS),
m_nearVehiclePut(0),
m_bCanLeaveParachuteSmokeTrail(false),
m_bIsInsideMovingTrain(false),
m_bAllowControlInsideMovingTrain(true),
m_pTrainPedIsInside(NULL),
m_StealthNoise(0.0f),
m_fNormalStealthMultiplier(1.0f),
m_fSneakingStealthMultiplier(1.0f),
m_fDampenRootWeight(0.0f),
m_fDampenRootTargetWeight(0.0f),
m_fDampenRootTargetHeight(0.0f),
m_uLastTimePedMadeMobileCommentOnWeirdPlayer(0),
m_uLastTimeDamagedLocalPlayer(0),
m_bSimulateGaitInput(false),
m_fSimulateGaitMoveBlendRatio(0.0f),
m_fSimulateGaitHeading(0.0f),
m_fSimulateGaitDuration(-1.0f),
m_fSimulateGaitTimerCount(0.0f),
m_fSimulateGaitStartHeading(0.0f),
m_bSimulateGaitNoInputInterruption(false),
m_fStealthPerceptionModifier(1.0f),
m_uTimeExitVehicleTaskFinished(0),
m_fCachedSprintMultThisFrame(-1.0f),
#if KEYBOARD_MOUSE_SUPPORT
m_uTimeBikeSprintPressed(0),
#endif
m_bAutoGiveParachuteWhenEnterPlane(false),
m_bAutoGiveScubaGearWhenExitVehicle(false),
m_uNumAiAttemptingArrestOnPlayer(0),
m_LastTimeBustInVehicleFailed(0),
m_LastTimeArrestAttempted(0),
m_LastTimeWarpedIntoVehicle(0),
m_LastTimeWarpedOutOfVehicle(0),
m_LastShoutTargetPositionTime(0),
m_fLaggedYawControl(0.0f),
m_fLaggedPitchControl(0.0f),
m_fLaggedRollControl(0.0f),
m_fLaggedSteerControl(0.0f),
m_fTimeBetweenRandomControlInputs(0.0f),
m_fRandomControlYaw(0.0f),
m_fRandomControlPitch(0.0f),
m_fRandomControlRoll(0.0f),
m_fRandomControlThrottle(0.0f),
m_fRandomControlSteerModifier(0.0f),
m_fControlSettledTime(0.0f),
m_fPreviousSteerValue(0.0f),
m_fTimeSinceApplyingRandomControl(0.0f),
m_fTimeSinceLastDrivingAtSpeed(0.0f),
m_fSwimBoundsEscapeTimer(ms_Tunables.m_MaxTimeToTrespassWhileSwimmingBeforeDeath),
m_uSprintAimBreakOutTime(0),
m_bHasWorldExtentsSpawnedAShark(false),
m_maxPortablePickupsCarried(-1),
m_numPortablePickupsCarried(0),
m_sFireProofTimer(0),
m_uCollidedWithPedTimer(0),
m_uLastTimeBumpedPedInVehicle(0),
m_bDisableAgitation(true),
m_uTimeToEnableAgitation(0),
m_uLastTimeStopAiming(0),
m_bWasAiming(false),
m_bWasLightAttackPressed(false),
m_bWasAlternateAttackPressed(false),
#if FPS_MODE_SUPPORTED
m_fRNGTimerFPS(0.0f),
m_bFiredInFPSFullAimState(false),
m_bFiredGunInFPS(false),
m_bFiredDuringFPSSprint(false),
m_bWasRunAndGunning(false),
m_bInFPSScopeState(false),
#endif
m_nMeleeEquippedWeaponHash(0),
m_nMeleeClipSetReleaseTimer(0),
m_bEnableMeleeClipSetReleaseTimer(false),
m_bEnableBlueToothHeadset(false),
m_TimeOfPlayerLastRespawn(0),
m_PreviousVariationComponent(PV_COMP_INVALID),
m_PreviousVariationDrawableId(0),
m_PreviousVariationDrawableAltId(0),
m_PreviousVariationTexId(0),
m_PreviousVariationPaletteId(0),
m_bPlayerLeavePedBehind(true),
m_ParachuteVariationComponent(PV_COMP_INVALID),
m_ParachuteVariationDrawable(0),
m_ParachuteVariationAltDrawable(0),
m_ParachuteVariationTexId(0),
m_ParachuteModelHash(0),
m_ReserveParachuteModelHash(0),
m_ParachutePackModelHash(0),
m_bRetainParachuteAfterTeleport(false),
m_SwitchedToOrFromFirstPersonCount(0),
m_SwitchedToFirstPersonCount(0),
m_SwitchedToFirstPersonFromThirdPersonCoverCount(0),
m_fFallHeightOverride(-1.0f),
#if FPS_MODE_SUPPORTED
m_fExitCombatRollToScopeTimerInFPS(0.0f),
m_fCoverHeadingCorrectionAimHeading(FLT_MAX),
m_bForceFPSRNGState(false),
#endif
#if ENABLE_HORSE
m_InputHorse(ped),
#endif
m_vFollowTargetOffset(V_ZERO),
m_fFollowTargetRadius(1.f),
m_fMaxFollowDistance(-1.f),
m_bMayFollowTargetGroupMembers(false),
m_pFollowTarget(NULL),
m_FriendStatus(0),
m_nearMissedDriverQueueIndex(0),
m_bVehMeleePerformingRightHit(true)
#if FPS_MODE_SUPPORTED
, m_vLeftHandPositionFPS(V_ZERO)
#endif // FPS_MODE_SUPPORTED
{
	if (gamerInfo)
		m_GamerInfo = *gamerInfo;

	EnableAllControls();

	for (int i = 0; i < PSAS_MAX; ++i)
	{
		m_eSpecialAbilitiesMP[i] = SAT_NONE;
	}

	// these used to be called in CPlayerPed constructor:
	m_Wanted.SetPed (ped);
	m_Wanted.Initialise();

	m_PlayerGroup = -1;
	m_bDisplayMobilePhone	= false;
	m_bStopNearbyVehicles = true;

	m_bUseHighFiringAttackBoundary = false;

	// all below moved from (defunct) CPlayerPed
	m_fHitByWaterCannonTimer = 0.0f;
	m_fHitByWaterCannonImmuneTimer = 0.0f;

	m_fTimeBeenSprinting = 0.0f;
	m_fSprintStaminaUpdatePeriod = 0.0f;
	m_fSprintStaminaDurationMultiplier = 1.0f;	
	m_fSprintApplyDamageTimer = 0;

	m_fTimeToNextCreateInteriorShockingEvents = ms_fInteriorShockingEventFreq;

	MultiplayerReset();

#if !__FINAL
	if(PARAM_invincible.Get())
		ms_bDebugPlayerInvincible = true;
#endif

    for (s32 i = 0; i < MAX_NEAR_VEHICLES; ++i)
    {
        m_nearVehicles[i].veh = NULL;
    }

	for( s32 i = 0; i < CHARGER_HISTORY_SIZE; ++i )
	{
		m_ChargedByEnemyHistoryMS[i] = 0;
	}

	for( s32 i = 0; i < SMOKE_THROWER_HISTORY_SIZE; ++i )
	{
		m_SmokeThrownByEnemyHistoryMS[i] = 0;
	}

	if (!ms_bPlayerBoundaryInitialized)
	{
		// Initialize the player restriction range - but only once.
		ms_WorldLimitsPlayerAABB = spdAABB(Vec3V(ms_Tunables.m_MinWorldLimitsPlayerX, ms_Tunables.m_MinWorldLimitsPlayerY, WORLDLIMITS_ZMIN), 
			Vec3V(ms_Tunables.m_MaxWorldLimitsPlayerX, ms_Tunables.m_MaxWorldLimitsPlayerY, WORLDLIMITS_ZMAX));
		ms_bPlayerBoundaryInitialized = true;
	}
}

void CPlayerInfo::MultiplayerReset()
{
	SetInitialState();

	m_PlayerState = PLAYERSTATE_PLAYING;

	PortablePickupPending			= false;

	for (u32 i=0; i<MAX_PORTABLE_PICKUP_INFOS; i++)
	{
		PortablePickupsInfo[i].modelIndex = -1;
		PortablePickupsInfo[i].maxPermitted = 0;
		PortablePickupsInfo[i].numCarried = 0;
	}

	m_maxPortablePickupsCarried = -1;
	m_numPortablePickupsCarried = 0;

	m_bPlayerLeavePedBehind = true;
}

void CPlayerInfo::SetInitialState()
{
	m_nLastChangeWeaponFrame = 0;
	m_fWeaponDamageModifier = 1.f;
	m_fWeaponDefenseModifier = 1.f;
	m_fWeaponMinigunDefenseModifier = 1.f;
	m_fMeleeWeaponDamageModifier = 1.f;
	m_fMeleeUnarmedDamageModifier = 1.f;
	m_fMeleeWeaponDefenseModifier = 1.f;
	m_fMeleeWeaponForceModifier = 1.f;
	m_fVehicleDamageModifier = 1.f;
	m_fVehicleDefenseModifier = 1.f;
	m_fMaxExplosiveDamage = -1.0f;
	m_fExplosiveDamageModifier = 1.0f;
	m_fWeaponTakedownDefenseModifier = 1.f;

	m_bFreeAiming = false;

	m_bCanBeDamaged = true;
	m_DamageAllowedFromSetPlayerControl = false;

	m_bPlayerSprintDisabled = false;
	m_bPlayerOnHighway = false;
	m_bPlayerOnWideRoad = false;
	m_bNodesNearbyInZ = true;
	m_bCanUseSearchLight = false;

	SetPlayerDataMaxSprintEnergy(CStatsMgr::GetFatAndMuscleModifier(STAT_MODIFIER_SPRINT_ENERGY));
	m_fSprintEnergy = GetPlayerDataMaxSprintEnergy();

	m_fSprintControlCounter = 0.0f;
	m_bSprintReplenishing = false;
	m_bAllowedToPickUpNonMissionObjects = true;
	m_bCanCollectMoneyPickups = true;
	m_bHasControlOfRagdoll = false;
	m_bAmbientMeleeMoveDisabled = false;

	m_iNumEnemiesInCombat = 0;
	m_iNumEnemiesShootingInCombat = 0;

	m_Wanted.Initialise();

	m_LastTimePlayerHitCar = 0;
	m_LastTimePlayerHitPed = 0;
	m_LastTimePlayerHitBuilding = 0;
	m_LastTimePlayerHitObject = 0;
	m_LastTimePlayerDroveOnPavement = 0;
	m_LastTimePlayerRanLight = 0;
	m_LastTimePlayerDroveAgainstTraffic = 0;
	
	m_uLastTimeDamagedLocalPlayer = 0;

	m_LastWeaponHashPickedUp = 0;
	m_TimeLastWeaponPickedUp = 0;

	m_StealthNoise = 0.0f;

	m_fPutPedDirectlyIntoCoverNetworkBlendInDuration = INSTANT_BLEND_DURATION;

	m_fLastPlayerNearbyTimer = 0.0f;

	ms_DynamicCoverHelper.Reset();

	m_LastTimeBustInVehicleFailed = 0;
	m_LastTimeArrestAttempted = 0;
	m_LastTimeWarpedIntoVehicle = 0;
	m_LastTimeWarpedOutOfVehicle = 0;
	m_uNumAiAttemptingArrestOnPlayer = 0;

	m_LastShoutTargetPositionTime = 0;

	// Charging enemies
	m_CandidateRegistrationEndTimeMS = 0;
	m_BestCandidateAcceptanceEndTimeMS = 0;
	m_BestEnemyChargerCandidatePed = NULL;
	for(int i=0; i < CHARGER_HISTORY_SIZE; ++i)
	{
		m_ChargedByEnemyHistoryMS[i] = 0;
	}
	ResetChargingEnemyGoalPositions();

	// Smoke thrower enemies
	m_BestEnemySmokeThrowerCandidatePed = NULL;
	for(int i=0; i < SMOKE_THROWER_HISTORY_SIZE; ++i)
	{
		m_SmokeThrownByEnemyHistoryMS[i] = 0;
	}

	m_PreviousVariationComponent = PV_COMP_INVALID;
	m_PreviousVariationDrawableId = 0;
	m_PreviousVariationDrawableAltId = 0;
	m_PreviousVariationTexId = 0;
	m_PreviousVariationPaletteId = 0;

	m_bPlayerLeavePedBehind = true;

	m_vScriptedWeaponFiringPos.ZeroComponents();
	m_bHasScriptedWeaponFirePos = false;

	m_bAllowStuntJumpCamera = false;
}


CPlayerInfo::~CPlayerInfo()
{
	CPed * pPed = GetPlayerPed();

	DEV_BREAK_IF_FOCUS( CDebugScene::ShouldDebugBreakOnDestroyOfFocusEntity(), pPed );
	DEV_BREAK_ON_PROXIMITY( CDebugScene::ShouldDebugBreakOnProximityOfDestroyCallingEntity(), VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) );

	if (pPed)
	{
#if __DEV
	    Assertf(CPedFactory::IsCurrentlyInDestroyPlayerPedFunction(), "Deleting a player ped from outside of the factory destroy method, this will cause problems for networked player peds");
#endif
		// moved from CGameWorld::DeletePlayer(): If this player was driving a car we have to tidy up
		CVehicle * pMyVehicle = pPed->GetMyVehicle();
		if (pMyVehicle && pMyVehicle->GetDriver() == pPed)
		{
			if (pMyVehicle->GetStatus() != STATUS_WRECKED)
			{
				pMyVehicle->SetStatus(STATUS_PHYSICS);
			}

			if (!pMyVehicle->IsNetworkClone())
			{
				pMyVehicle->SetThrottle(0.0f);
				pMyVehicle->SetBrake(HIGHEST_BRAKING_WITHOUT_LIGHT);
			}
		}

		GetWanted().SetPed (NULL);

		if (GetPlayerDataPlayerGroup() >= 0)
		{
			CPedGroups::ms_groups[GetPlayerDataPlayerGroup()].Flush(false, false);
		}
	}

#if LIGHT_EFFECTS_SUPPORT
	// Even if control is disabled, still want to update light-effect color.
	CControl& control = CControlMgr::GetPlayerMappingControl();
	control.ClearLightEffect(&m_WantedLightEffect, CControl::CODE_LIGHT_EFFECT);
	control.ClearLightEffect(&m_PlayerLightEffect, CControl::PLAYER_LIGHT_EFFECT);

#if LIGHT_EFFECT_FLASH_FOR_LOW_HEALTH
	control.ClearLightEffect(&m_FlashingPlayerLightEffect, CControl::PLAYER_LIGHT_EFFECT);
#endif // LIGHT_EFFECT_FLASH_FOR_LOW_HEALTH

#endif // LIGHT_EFFECT_SUPPORT

#if __BANK
	// ensure no other peds are using this player info
	CPed::Pool *pedPool = CPed::GetPool();

	for(int index = 0; index < pedPool->GetSize(); index++)
	{
		CPed *pPedTemp = pedPool->GetSlot(index);

		if(pPedTemp && pPedTemp->GetPlayerInfo() == this)
		{
			Assertf(0, "CPlayerInfo::~CPlayerInfo() - Deleting a player info pointer (%p) when it is still referenced by %s", this, pPedTemp->GetDebugName());
		}
	}
#endif // BANK
}

void CPlayerInfo::SetPlayerPed(CPed* pPlayerPed, bool bClearPedDamage)
{
	Assert(m_pPlayerPed==NULL || m_pPlayerPed->GetPlayerInfo()==NULL);	// make sure the last user of this CPlayerInfo is no longer pointing to it

	m_pPlayerPed=pPlayerPed;
	m_Wanted.SetPed (pPlayerPed);	
	m_targeting.SetOwner(pPlayerPed);
	ResetAllPlayerLOS();
	m_healthRecharge.SetRechargeScriptMultiplier(1.0f);

	//
	// init all the contained components now we are attaching to a ped
	//
    Assert(CPedFactory::IsCurrentlyWithinFactory() || CPlayerInfo::IsChangingPlayerModel() || !pPlayerPed);

	m_targeting.SetOwner(pPlayerPed);

	if (pPlayerPed)
	{
		Assert(pPlayerPed->GetPortalTracker());
		pPlayerPed->GetPortalTracker()->SetLoadsCollisions(true);	//force loading of interior collisions
		CPortalTracker::AddToActivatingTrackerList(pPlayerPed->GetPortalTracker());		// causes interiors to activate & stay active

 		// the player can have a cop model in the multiplayer game, which gives him a PEDTYPE_COP - this confuses the dispatch stuff
		if (NetworkInterface::IsInSession() && !CPedType::IsAnimalType(pPlayerPed->GetPedType()))
		{
			pPlayerPed->SetPedType(PEDTYPE_NETWORK_PLAYER);
		}

		pPlayerPed->SetPedConfigFlag( CPED_CONFIG_FLAG_UpperBodyDamageAnimsOnly, true );
		pPlayerPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DisableLockonToRandomPeds, false );
		pPlayerPed->SetPedConfigFlag( CPED_CONFIG_FLAG_GeneratesSoundEvents, true );

		// Have to update the max health here so that it is correct for the 2nd player in 2-player games.
		pPlayerPed->SetMaxHealth(CPlayerInfo::PLAYER_DEFAULT_MAX_HEALTH); //CStatsMgr::GetFatAndMuscleModifier(STAT_MODIFIER_MAX_HEALTH);
		pPlayerPed->SetHealth(pPlayerPed->GetMaxHealth(), NetworkInterface::IsNetworkOpen());

		if (bClearPedDamage)
		{
			pPlayerPed->ClearDamage();
		}
		// pPlayerPed->ClearDecorations();  // should we also clear all the scars and tattoos?

		// these used to be set in CGameWorld just after the ped was created:
		//mjc-moved m_targeting.SetOwner(this);
		//pPlayerPed->SetHeading(0.0f);
		
		//Its legit for some characters to not be 
		if (pPlayerPed->GetWeaponManager())
		{
			pPlayerPed->GetPedIntelligence()->GetCombatBehaviour().SetCombatFloat(kAttribFloatWeaponAccuracy, 1.0f);
		}

		pPlayerPed->GetPedIntelligence()->SetInformRespectedFriends(30.0f, 2);

		pPlayerPed->m_PedConfigFlags.SetPedLegIkMode( CIkManager::GetDefaultLegIkMode(pPlayerPed) );

		// The player ped is always treated as suspicious
		pPlayerPed->GetPedIntelligence()->GetPedStealth().GetFlags().SetFlag(CPedStealth::SF_TreatedAsActingSuspiciously);
		// Player's footsteps always generate events
		pPlayerPed->GetPedIntelligence()->GetPedStealth().GetFlags().SetFlag(CPedStealth::SF_PedGeneratesFootStepEvents);

        // Re setup the players group. We can't do this for players in a network game that have just been created and have no network object (this
		// will be done later)
		if (!NetworkInterface::IsNetworkOpen() || pPlayerPed->GetNetworkObject())
		{
			SetupPlayerGroup();
		}

		// Make sure the vehicle knows its under player control
		if (pPlayerPed->GetMyVehicle() && m_pPlayerPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pPlayerPed->GetMyVehicle()->GetStatus()!=STATUS_WRECKED)
		{
			pPlayerPed->GetMyVehicle()->SetStatus(STATUS_PLAYER);
		}

		// prevent dead players being revived 
		pPlayerPed->SetPedConfigFlag( CPED_CONFIG_FLAG_AllowMedicsToReviveMe, FALSE );

		if( pPlayerPed->IsLocalPlayer() )
		{
			pPlayerPed->m_nFlags.bAddtoMotionBlurMask = true;
		}
		else
		{
			pPlayerPed->m_nFlags.bAddtoMotionBlurMask = false;
		}
	}

#if ENABLE_HORSE
	m_InputHorse.SetPlayerPed(m_pPlayerPed);
#endif
}

// PURPOSE: To get the PhysicalPlayerIndex, if there is one
PhysicalPlayerIndex CPlayerInfo::GetPhysicalPlayerIndex()
{
	if(m_pPlayerPed)
	{
		if(CNetObjGame* pNetworkObject = m_pPlayerPed->GetNetworkObject())
		{
			if(CNetGamePlayer* pNetGamePlayer = pNetworkObject->GetPlayerOwner())
			{
				return pNetGamePlayer->GetPhysicalPlayerIndex();
			}
		}
	}

	return INVALID_PLAYER_INDEX;
}

void CPlayerInfo::SetLastTargetVehicle(CVehicle* pTargetVehicle)
{
	m_pLastTargetVehicle=pTargetVehicle;	
}


/*
void CPlayerInfo::SetRemoteVehicle(CVehicle* pVehicle)
{
	m_pRemoteVehicle=pVehicle;		
}
*/

void CPlayerInfo::SetMayOnlyEnterThisVehicle(CVehicle * pVehicle)
{
	m_pOnlyEnterThisVehicle = pVehicle;
}

void CPlayerInfo::BumpedInVehicle(const CPed& UNUSED_PARAM(rPed))
{
	m_uLastTimeBumpedPedInVehicle = fwTimer::GetTimeInMilliseconds();
}

void CPlayerInfo::DisableAgitation()
{
	//Set the flag.
	m_bDisableAgitation = true;

	//Set the timer.
	m_uTimeToEnableAgitation = 0;
}

bool CPlayerInfo::IsSprintAimBreakOutOver() const
{
	TUNE_GROUP_INT(SPRINT_DEBUG, MIN_SPRINT_BREAKOUT_TIME, 600, 0, 5000, 1);
	return (m_uSprintAimBreakOutTime + MIN_SPRINT_BREAKOUT_TIME) < fwTimer::GetTimeInMilliseconds();
}

void CPlayerInfo::CollidedWithPed(CPed& rPed)
{
	//Ensure the ped is not a player.
	if(rPed.IsPlayer())
	{
		return;
	}

	//Ensure the ped is not already agitated.
	if(rPed.GetPedConfigFlag(CPED_CONFIG_FLAG_IsAgitated))
	{
		return;
	}

	//Check if the peds do not match.
	if(&rPed != m_pCollidedWithPed)
	{
		//Set the ped and timer.
		m_pCollidedWithPed		= &rPed;
		m_uCollidedWithPedTimer = fwTimer::GetTimeInMilliseconds();
	}
	else
	{
		//Check if we are in the ignore window.
		static dev_u32 s_uIgnoreWindow = 1000;
		if((m_uCollidedWithPedTimer + s_uIgnoreWindow) > fwTimer::GetTimeInMilliseconds())
		{
			//Ignore.
		}
		else
		{
			// Potentially add rumble for this collision.
			HandleCollisionWithRumblePed(rPed);
			
			//Check if we are in the agitated window.
			static dev_u32 s_uAgitatedWindow = 6000;
			if((m_uCollidedWithPedTimer + s_uIgnoreWindow + s_uAgitatedWindow) > fwTimer::GetTimeInMilliseconds())
			{
				//Give the ped an agitated event.
				CEventAgitated event(m_pPlayerPed, AT_Bumped);
				rPed.GetPedIntelligence()->AddEvent(event);

				//Clear the ped and timer.
				m_pCollidedWithPed		= NULL;
				m_uCollidedWithPedTimer = 0;
			}
			else
			{
				//Set the timer.
				m_uCollidedWithPedTimer = fwTimer::GetTimeInMilliseconds();
			}
		}
	}
}

void CPlayerInfo::HandleCollisionWithRumblePed(CPed& rPed)
{
	CPedModelInfo* pModelInfo = rPed.GetPedModelInfo();
	if (!rPed.GetMotionData()->GetIsStill() && pModelInfo && pModelInfo->GetPersonalitySettings().GetCausesRumbleWhenCollidesWithPlayer())
	{
		CControlMgr::StartPlayerPadShakeByIntensity(1000, 0.5f);
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : Process
// PURPOSE :  Processes the player stuff for one player. The player might jump
//			  into a car or leave a car etc.
/////////////////////////////////////////////////////////////////////////////////

float CPlayerInfo::SCANNEARBYVEHICLES = 10.0f;
dev_u32 CPlayerInfo::DETONATE_TIMER = 800; // milliseconds.

#define HEALTH_TAKEN_FOR_JUMPING_OUT_AP_HIGH_SPEED (20.0f)	// Health taken for jumping out of car at high speed

void CPlayerInfo::Process()
{
#if RSG_ASSERT
	const CPed* pPlayer = CGameWorld::FindLocalPlayer();

	// If the ped does not have player control
	if(pPlayer == NULL || pPlayer->GetControlFromPlayer() == NULL)
	{
		CPed* pControlledPed = NULL;
		
		// Find the ped that does have control.
		CEntityIterator entityIterator(IteratePeds);
		CEntity* pEntity = entityIterator.GetNext();
		while(pControlledPed == NULL && pEntity != NULL)
		{
			if(pEntity->GetIsTypePed())
			{
				CPed* pPed = static_cast<CPed*>(pEntity);
				if(pPed->GetControlFromPlayer())
				{
					pControlledPed = pPed;
				}
			}

			pEntity = entityIterator.GetNext();
		}

		Assertf(false, 
				"Player Ped: '%s' does not have control, '%s' does!",
				(pPlayer ? pPlayer->GetModelName() : "NO PED"),
				(pControlledPed ? pControlledPed->GetModelName() : "NO PED") );
	}
#endif // RSG_ASSERT

    // don't process the player info for network bot controlled objects
    if(m_pPlayerPed->IsNetworkBot())
    {
		return;
    }

	if (m_pPlayerPed && (CGameWorld::GetMainPlayerInfo() == this)){
		ms_cachedMainPlayerPos = VEC3V_TO_VECTOR3(m_pPlayerPed->GetTransform().GetPosition());
	}

	DecayStealthNoise(fwTimer::GetTimeStep());

	Vector3		DropCoors, Delta;

#ifdef DEBUG
//	Last updated 24.11.00

	AssertEntityPointerValid(m_pPlayerPed);
	
	if (GetRemoteVehicle())
	{
		AssertEntityPointerValid_NotInWorld(GetRemoteVehicle());
	}

//	End last updated 24.11.00
#endif

#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive())
		return;
#endif

	Assert(m_pPlayerPed);

	// Update Targeting
	GetTargeting().Process();

	CPlayerInfo::UpdateRecklessDriving();

	ProcessTestForShelter();

	// player health doesn't recharge when out of
	// sprint energy
	TUNE_GROUP_FLOAT(SPRINT_DEBUG, s_minSprintEnergyForHealthRecharge, 0.0f, 0.0f, 100.0f, 0.01f);
	if (m_fSprintEnergy>s_minSprintEnergyForHealthRecharge)
	{
		// process player health recharging
		m_healthRecharge.Process(m_pPlayerPed);
	}

	// process player endurance recharging
	m_enduranceManager.Process(m_pPlayerPed);

	// process player damage over time effects
	m_damageOverTime.Process(m_pPlayerPed);

	m_fDampenRootTargetWeight = 0.0f;

	ProcessIsInsideMovingTrain();

	ProcessWorldExtentsCheck();

	ProcessNumEnemiesInCombat();

	ProcessCoverStateTracking();

	ProcessCombatLoitering();
	
	ProcessChargingEnemyGoalPositions();

	ProcessEnemyElections();

	ProcessStolenVehicleSpotted();

	ProcessCollisions();

	ProcessDisableAgitation();

	ProcessPlayerVehicleSpeedTimer();

	m_bRetainParachuteAfterTeleport = m_pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_DisableTakeOffParachutePack) && (GetPedParachuteVariationComponentOverride() != PV_COMP_INVALID);

	if (CPlayerInfo::AreCNCResponsivenessChangesEnabled(m_pPlayerPed))
	{
		m_pPlayerPed->SetPedResetFlag(CPED_RESET_FLAG_UseFastEnterExitVehicleRates, true);
	}
}

void CPlayerInfo::ProcessCollisions()
{
	//Ensure we collided with a ped.
	const CCollisionRecord* pRecord = m_pPlayerPed->GetFrameCollisionHistory()->GetFirstPedCollisionRecord();
	if(!pRecord)
	{
		return;
	}

	//Ensure the entity is valid.
	if(!pRecord->m_pRegdCollisionEntity)
	{
		return;
	}

	//Note that we collided with a ped.
	aiAssert(pRecord->m_pRegdCollisionEntity->GetIsTypePed());
	CollidedWithPed(*static_cast<CPed *>(pRecord->m_pRegdCollisionEntity.Get()));
}

void CPlayerInfo::ProcessDisableAgitation()
{
	//Ensure agitation is disabled.
	if(!m_bDisableAgitation)
	{
		return;
	}

	//Check if the timer has not been set.
	if(m_uTimeToEnableAgitation == 0)
	{
		//Ensure controls are not disabled.
		if(AreControlsDisabled())
		{
			return;
		}

		//Check if the player is in a vehicle.
		if(m_pPlayerPed->GetIsInVehicle())
		{
			//Check if the vehicle is still.
			float fSpeedSq = m_pPlayerPed->GetVehiclePedInside()->GetVelocity().XYMag2();
			static dev_float s_fMaxSpeed = 0.1f;
			float fMaxSpeedSq = square(s_fMaxSpeed);
			if(fSpeedSq < fMaxSpeedSq)
			{
				return;
			}
		}
		//Check if the player is on a horse.
		else if(m_pPlayerPed->GetIsOnMount())
		{
			//Check if the mount is still.
			if(m_pPlayerPed->GetMountPedOn()->GetMotionData()->GetIsStill())
			{
				return;
			}
		}
		else
		{
			//Check if the player is still.
			if(m_pPlayerPed->GetMotionData()->GetIsStill())
			{
				return;
			}
		}

		//Set the time.
		m_uTimeToEnableAgitation = fwTimer::GetTimeInMilliseconds() + 5000;
	}
	//Check if the timer has expired.
	else if(fwTimer::GetTimeInMilliseconds() > m_uTimeToEnableAgitation)
	{
		//Set the flag.
		m_bDisableAgitation = false;

		//Clear the time.
		m_uTimeToEnableAgitation = 0;
	}
}

void CPlayerInfo::ProcessPlayerVehicleSpeedTimer()
{
	if( CNetwork::IsGameInProgress() )
	{
		if( m_pPlayerPed && m_pPlayerPed->GetIsInVehicle() && m_pPlayerPed->GetMyVehicle() )
		{
			//Ensure the vehicle is moving slow.
			float fSpeedSq = m_pPlayerPed->GetMyVehicle()->GetVelocity().Mag2();
			static dev_float s_fMaxSpeed = 5.0f;
			float fMaxSpeedSq = square(s_fMaxSpeed);
			if(fSpeedSq < fMaxSpeedSq)
			{
				m_fTimeSinceLastDrivingAtSpeed += fwTimer::GetTimeStep();
				return;
			}
		}
	}

	m_fTimeSinceLastDrivingAtSpeed = 0.0f;
}

void CPlayerInfo::ProcessIsInsideMovingTrain()
{
	//Clear the flags.
	m_bIsInsideMovingTrain				= false;
	m_bAllowControlInsideMovingTrain	= true;
	m_pTrainPedIsInside					= NULL;

	//Ensure the entity is valid.
	const CEntity* pEntity = m_pPlayerPed->GetGroundPhysical();
	if(!pEntity)
	{
		pEntity = (const CEntity *)m_pPlayerPed->GetAttachParent();
		if(!pEntity)
		{
			return;
		}
	}

	//Ensure the entity is physical.
	if(!pEntity->GetIsPhysical())
	{
		return;
	}

	//Ensure the physical is a vehicle.
	const CPhysical* pPhysical = static_cast<const CPhysical *>(pEntity);
	if(!pPhysical->GetIsTypeVehicle())
	{
		return;
	}

	//Ensure the vehicle is a train.
	const CVehicle* pVehicle = static_cast<const CVehicle *>(pPhysical);
	if(!pVehicle->InheritsFromTrain())
	{
		return;
	}

	const CTrain* pTrain = static_cast<const CTrain *>(pVehicle);

	//Ensure the player is inside the train.
	spdAABB boundBox;
	if(!pTrain->GetBoundBox(boundBox).ContainsPoint(m_pPlayerPed->GetTransform().GetPosition()))
	{
		return;
	}

	//TODO: HACK:	The metro train bounding box extends well over the top of the car, and we want to 
	//				be able to stand on top of it and move around.  Inside, though, we want to disable control.
	static atHashWithStringNotFinal s_hMetroTrain("metrotrain",0x33C9E158);
	if(pTrain->GetArchetype()->GetModelNameHash() == s_hMetroTrain.GetHash())
	{
		if(m_pPlayerPed->GetTransform().GetPosition().GetZf() > (pTrain->GetTransform().GetPosition().GetZf() + 2.0f))
		{
			return;
		}

		//Don't allow control - in SP, in MP always keep the player in control
		if (!NetworkInterface::IsGameInProgress())
			m_bAllowControlInsideMovingTrain = false;

		m_pTrainPedIsInside = (CVehicle*)pTrain;
	}

	//Ensure the train is moving.
	if(!pTrain->IsMoving() && pTrain->GetTrainState() != CTrain::TS_ArrivingAtStation)
	{
		return;
	}

	//Set the flag.
	m_bIsInsideMovingTrain = true;
}

void CPlayerInfo::ProcessStolenVehicleSpotted()
{
	//Check if the player is in a stolen vehicle, and is not wanted.
	const CVehicle* pVehicle = m_pPlayerPed->GetVehiclePedInside();
	if(pVehicle && pVehicle->m_nVehicleFlags.bIsStolen &&
		(m_pPlayerPed->GetPlayerWanted()->GetWantedLevel() == WANTED_CLEAN))
	{
		//Create the flags.
		s32 iFlags = TargetOption_IgnoreVehicles | TargetOption_TargetInVehicle | TargetOption_UseFOVPerception;

		//Check if someone spotted the vehicle last frame.
		if(m_pSpotterOfStolenVehicle && !m_pSpotterOfStolenVehicle->IsInjured())
		{
			//Check if the last spotter can target the vehicle.
			ECanTargetResult nCanTargetResult = CPedGeometryAnalyser::CanPedTargetPedAsync(*m_pSpotterOfStolenVehicle, *m_pPlayerPed, iFlags);
			switch(nCanTargetResult)
			{
				case ECanNotTarget:
				{
					break;
				}
				default:
				{
					return;
				}
			}
		}

		//Iterate over the nearby peds.
		CEntityScannerIterator it = m_pPlayerPed->GetPedIntelligence()->GetNearbyPeds();
		for(CEntity* pEntity = it.GetFirst(); pEntity != NULL; pEntity = it.GetNext())
		{
			//Ensure the other ped is alive.
			CPed* pOtherPed = static_cast<CPed *>(pEntity);
			if(pOtherPed->IsInjured())
			{
				continue;
			}

			//Ensure the other ped is a cop.
			if(!pOtherPed->IsRegularCop())
			{
				continue;
			}

			//Ensure the other ped is random.
			if(!pOtherPed->PopTypeIsRandom())
			{
				continue;
			}

			//Check if the other ped can see the player.
			ECanTargetResult nCanTargetResult = CPedGeometryAnalyser::CanPedTargetPedAsync(*pOtherPed, *m_pPlayerPed, iFlags);
			if(nCanTargetResult == ECanTarget)
			{
				//Set the spotter.
				m_pSpotterOfStolenVehicle = pOtherPed;
				return;
			}
		}
	}

	//Clear the spotter.
	m_pSpotterOfStolenVehicle = NULL;
}

// Name			:	ProcessControl
// Purpose		:	Processes control for player ped object
// Parameters	:	None
// Returns		:	Nothing
bool CPlayerInfo::ProcessControl()
{
	if(DisableControls != 0)
	{
		controlDebugf2("Some player controls are disabled, disabled control flags: 0x%8X", DisableControls);
	}

	CPed* pPlayerPed = GetPlayerPed();
	pedFatalAssertf( pPlayerPed, "No player ped!" );

	GetPlayerResetFlags().ResetPreAI();
	if(m_SwitchedToFirstPersonFromThirdPersonCoverCount > 0)
		--m_SwitchedToFirstPersonFromThirdPersonCoverCount;

	if(m_SwitchedToOrFromFirstPersonCount > 0)
	{
		--m_SwitchedToOrFromFirstPersonCount;

		if(m_SwitchedToFirstPersonCount > 0)
			--m_SwitchedToFirstPersonCount;

		if(m_SwitchedToOrFromFirstPersonCount == 1)
			GetPlayerResetFlags().SetFlag(CPlayerResetFlags::PRF_CAMERA_VIEW_MODE_SWITCHED_TO_OR_FROM_FIRST_PERSON);
		if(m_SwitchedToFirstPersonCount == 1)
			GetPlayerResetFlags().SetFlag(CPlayerResetFlags::PRF_CAMERA_VIEW_MODE_SWITCHED_TO_FIRST_PERSON);

		// B*2038645 - Reset simulating aiming flag if we've changed camera mode (in case it doesn't get reset in CPlayerInfo::ProcessFPSState).
		if (pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_SimulatingAiming) && (GetPlayerResetFlags().IsFlagSet(CPlayerResetFlags::PRF_CAMERA_VIEW_MODE_SWITCHED_TO_OR_FROM_FIRST_PERSON) || GetPlayerResetFlags().IsFlagSet(CPlayerResetFlags::PRF_CAMERA_VIEW_MODE_SWITCHED_TO_FIRST_PERSON)))
		{
			pPlayerPed->SetPedConfigFlag(CPED_CONFIG_FLAG_SimulatingAiming, false);
		}
	}

	m_fCachedSprintMultThisFrame = -1.0f;

	Vector3 vPlayerPos = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition());
	DEV_BREAK_IF_FOCUS( CDebugScene::ShouldDebugBreakOnProcessControlOfFocusEntity(), pPlayerPed );
	DEV_BREAK_ON_PROXIMITY( (CDebugScene::ShouldDebugBreakOnProximityOfProcessControlCallingEntity() || CDebugScene::ShouldDebugBreakOnProximityOfProcessControlCallingPed()), vPlayerPos );

	if (pPlayerPed->IsControlledByNetworkPlayer())
	{
		// we have control of another network player, just do a simple ped process (and do none of the player processing in here)
		return true;
	}

	Assertf((pPlayerPed->PopTypeGet() == POPTYPE_PERMANENT), "CPlayerPed::ProcessControl - Player Ped should always be a permanent character");

	Assertf(pPlayerPed->GetRagdollState() != RAGDOLL_STATE_ANIM_LOCKED || AreControlsDisabled(),"CPlayerPed: Player is in control of locked ragdoll. Has script called UNLOCK_RAGDOLL on playerped?");

	// Decrement hit by water cannon timer. Timer gets increased whenever player gets hit by water cannon
	// This ensures player can't get trapped permanently by water cannon
	if(m_fHitByWaterCannonImmuneTimer > 0.0f)
		m_fHitByWaterCannonImmuneTimer -= fwTimer::GetTimeStep();

	if(m_fHitByWaterCannonTimer > 0.0f)
		m_fHitByWaterCannonTimer -= fwTimer::GetTimeStep()*PLAYER_WATER_CANNON_COOLDOWN_RATE;

	Process(); //potentially we want to pull all the code in Process into here (or split the two functions into more logical blocks).  //TODO

	CControl* pControl = pPlayerPed->GetControlFromPlayer();


	// If a player ped stands on a police car he will get a wanted level
	CPhysical * pGroundPhysical = pPlayerPed->GetGroundPhysical();
	if (pGroundPhysical && pGroundPhysical->GetIsTypeVehicle())
	{
		CVehicle *pVeh = static_cast<CVehicle*>(pGroundPhysical);

		// Make sure the vehicle is a cop car and the player isn't a cop.
		if ( !(fwTimer::GetSystemFrameCount()&63) &&
			 pVeh->ReportCrimeIfStandingOn() && !pPlayerPed->IsLawEnforcementPed() && !pVeh->IsPersonalVehicle())
		{

			// The vehicle must have alive cops in it or have police within the detection range
			if ( pVeh->HasAliveLawPedsInIt() ||
				 CWanted::WorkOutPolicePresence(pPlayerPed->GetTransform().GetPosition(),
				 CCrimeInformationManager::GetInstance().GetImmediateDetectionRange(CRIME_STAND_ON_POLICE_CAR)) )
			{
				CCrime::ReportCrime(CRIME_STAND_ON_POLICE_CAR, pVeh, pPlayerPed);
			}
		}
	}

	//	Get PlayerPed Position again just in case it could have changed inside Process()
	vPlayerPos = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition());

	m_Wanted.Update( vPlayerPos, pPlayerPed->GetVehiclePedInside(), true, true );

	// let the player fire his gun when ragdolling
	CPedWeaponManager * pWeaponManager = pPlayerPed->GetWeaponManager();
	if( pWeaponManager )
	{
		//Check on weapon cook timers
		const CWeaponInfo* pEquippedWeaponInfo = pWeaponManager->GetEquippedWeaponInfo();
		if( pEquippedWeaponInfo )
		{
			CWeapon* pEquippedWeapon = pWeaponManager->GetEquippedWeapon();
			if( pEquippedWeapon )
			{
				if( pControl && pControl->GetPedAttack().IsDown() && pPlayerPed->GetUsingRagdoll() )
				{
					CObject* pWeaponObject = pWeaponManager->GetEquippedWeaponObject();
					if( pWeaponObject && pWeaponObject->GetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY) )
					{
						// We're pretty sure this is the actual player weapon, so we can force that one to true
						pWeaponObject->m_nFlags.bAddtoMotionBlurMask = true;

						if (pEquippedWeaponInfo->GetIsGunOrCanBeFiredLikeGun() && !pPlayerPed->IsInjured() )
						{
							bool bCanFire = true;
							if(m_pPlayerPed->m_Buoyancy.GetStatus() == FULLY_IN_WATER)
							{
								bCanFire = false;
							}
							else
							{
								CTask * pTaskDrive = pPlayerPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PLAYER_DRIVE);
								CTask * pTaskJumpRollFromVehicle = pPlayerPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_NM_JUMP_ROLL_FROM_ROAD_VEHICLE);
								if(pTaskDrive || pTaskJumpRollFromVehicle)
									bCanFire = false;
							}

							if(bCanFire)
							{
								Matrix34 m = MAT34V_TO_MATRIX34( pWeaponObject->GetMatrix() );
								pEquippedWeapon->Fire( CWeapon::sFireParams( pPlayerPed, m ) );
							}
						}
						else if (pEquippedWeaponInfo->GetCookWhileAiming() && pEquippedWeapon && pEquippedWeapon->GetCookTime()==0 && pPlayerPed->IsInjured())
						{
							pEquippedWeapon->StartCookTimer(fwTimer::GetTimeInMilliseconds());
						}
					}
				}

// 				if( pEquippedWeapon->GetIsCooking() ) //Removing grenade disarms per B* 983722
// 				{
// 					if (!pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_IsAiming) && !pPlayerPed->GetUsingRagdoll() && !IsFiring())
// 						pEquippedWeapon->CancelCookTimer();
// 				}
			}
		}

		// Take care of all hand placed bombs
		// Unfortunately we can never eliminate this hard coded value to STICKYBOMB since
		// we cannot always have sticky bombs equipped when attempting to detonate
		if( pPlayerPed && pControl )
		{
			CVehicle* pPlayerVehicle = pPlayerPed->GetMyVehicle();
			// If we are not in a vehicle the use is down to detonate. Otherwise detonate the button was released within a time frame.
			// i.e. button tapped (to stop conflicts with other vehicle controls).
			if(    (pControl->GetPedDetonate().IsDown() && (!pPlayerVehicle || !pPlayerVehicle->GetVehicleAudioEntity()->GetHasNormalRadio() || !pPlayerVehicle->GetVehicleAudioEntity()->IsRadioEnabled() ))
				|| (pControl->GetPedDetonate().IsReleased() && DetonateDownTime + DETONATE_TIMER > fwTimer::GetTimeInMilliseconds()) )
			{
				DetonateDownTime = 0;

				const u32 uTimeAllowToDetonateAfterDeath = 1000;
				if( (!pPlayerPed->GetIsDeadOrDying() || fwTimer::GetTimeInMilliseconds() - pPlayerPed->GetDeathTime() <= uTimeAllowToDetonateAfterDeath) 
					&& !pPlayerPed->GetCustodian()
					&& !pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsHandCuffed)
					&& CProjectileManager::CanExplodeTriggeredProjectiles(m_pPlayerPed) )
				{
					// This is so conflicting inputs remains disabled until detonate is released.
					pControl->SetInputExclusive(INPUT_DETONATE);
					CProjectileManager::ExplodeTriggeredProjectiles( m_pPlayerPed );

					bExplodedTriggeredProjectilesWithThisButtonPress = true;
				}
			}
			// if we are here then we are in a car or the button is not down. if the button has just been pressed start the timer to see
			// if it is released in time (i.e. to detect tapped or held).
			else if( pControl->GetPedDetonate().IsPressed() && CProjectileManager::CanExplodeTriggeredProjectiles(m_pPlayerPed) )
			{
				DetonateDownTime = fwTimer::GetTimeInMilliseconds();
				pControl->SetInputExclusive(INPUT_DETONATE, ioValue::DisableOptions(ioValue::DisableOptions::F_DISABLE_UPDATE_WHEN_DISABLED));
			}
			// whilst the button is down and we are in a vehicle, continue suppressing conflicting inputs unless the timer has expired
			// (i.e. button is held down so do not detonate).
			else if( pControl->GetPedDetonate().IsDown() && DetonateDownTime + DETONATE_TIMER > fwTimer::GetTimeInMilliseconds() )
			{
				// This is so conflicting inputs remains disabled until detonate is released.
				pControl->SetInputExclusive(INPUT_DETONATE, ioValue::DisableOptions(ioValue::DisableOptions::F_DISABLE_UPDATE_WHEN_DISABLED));
			}
			else if( ( ( pControl->GetPedDetonate().IsDown() || pControl->GetPedDetonate().IsReleased() ) && bExplodedTriggeredProjectilesWithThisButtonPress ) )
			{
				// This conflicts with grenade toss
				pControl->SetInputExclusive(INPUT_DETONATE, ioValue::DisableOptions(ioValue::DisableOptions::F_DISABLE_UPDATE_WHEN_DISABLED));

				if( pControl->GetPedDetonate().IsReleased() )
				{
					bExplodedTriggeredProjectilesWithThisButtonPress = false;
				}

				DetonateDownTime = 0;
			}
			else if( bExplodedTriggeredProjectilesWithThisButtonPress )
			{
				// Fallback to fix up this bool if it doesn't get fixed up above
				bExplodedTriggeredProjectilesWithThisButtonPress = false;

				DetonateDownTime = 0;
			}
			
			else
			{
				DetonateDownTime = 0;
			}
		}
		else
		{
			DetonateDownTime = 0;
		}
	}

	// RECHARGE SPRINT ENERGY - Done here before tasks are processed
	TUNE_GROUP_BOOL(SPRINT_DEBUG, DISABLE_RECHARGE, false);
	if (!DISABLE_RECHARGE)
	{
		// Full restoration durations are longer the higher the stat, maybe should scale with the stat so the duration is fixed?
		TUNE_GROUP_FLOAT(SPRINT_DEBUG, RUNNING_RESTORE_DURATION, 50.0f, 1.0f, 100.0f, 1.0f);
		TUNE_GROUP_FLOAT(SPRINT_DEBUG, NOT_RUNNING_RESTORE_DURATION, 15.0f, 1.0f, 100.0f, 1.0f);
		CTaskMotionBase* pBaseTask = pPlayerPed->GetCurrentMotionTask();
		const bool bUnderwater = pBaseTask->IsUnderWater();

		if (pPlayerPed->GetIsInVehicle() && pPlayerPed->GetMyVehicle()->InheritsFromBicycle())
		{
			const bool bIsStillOnBicycle = pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_IsStillOnBicycle);
			const float fSprintStatValue  = Clamp(static_cast<float>(StatsInterface::GetIntStat(STAT_STAMINA.GetStatId())) / 100.0f, 0.0f, 1.0f);
			float fRestoreDuration = bIsStillOnBicycle ? ms_Tunables.m_BikeMinRestoreDuration : ms_Tunables.m_BikeMaxRestoreDuration;
			if (!bIsStillOnBicycle)
			{
				fRestoreDuration = (1.0f - fSprintStatValue) * ms_Tunables.m_BikeMinRestoreDuration + fSprintStatValue * ms_Tunables.m_BikeMaxRestoreDuration;
			}
			//aiDisplayf("fRestoreDuration = %.2f, fSprintStatValue = %.2f, rate = %.2f", fRestoreDuration, fSprintStatValue, GetPlayerDataMaxSprintEnergy() / (fRestoreDuration));
			TUNE_GROUP_BOOL(SPRINT_DEBUG, ALLOW_RESTORE_WHEN_HELD_ON_ZERO_ENERGY, true);
			TUNE_GROUP_INT(SPRINT_DEBUG, MIN_HELD_DURATION_TO_HANDLE_SPRINT_ENERGY, 100, 0, 1000, 10);
#if KEYBOARD_MOUSE_SUPPORT
			const bool bBlockHandleSprintWhenBikeSprintToggleEnabled = pControl && pControl->GetToggleBikeSprintOn();
#else // KEYBOARD_MOUSE_SUPPORT
			const bool bBlockHandleSprintWhenBikeSprintToggleEnabled = false;
#endif // KEYBOARD_MOUSE_SUPPORT
			const bool bSprintHeldDown = ALLOW_RESTORE_WHEN_HELD_ON_ZERO_ENERGY && pControl && pControl->GetVehiclePushbikePedal().HistoryHeldDown(MIN_HELD_DURATION_TO_HANDLE_SPRINT_ENERGY);
			if (((!bBlockHandleSprintWhenBikeSprintToggleEnabled && bSprintHeldDown) || m_fSprintEnergy > 0.0f) || m_fSprintControlCounter == 0.0f) //don't recharge when 0 and sprinting
				HandleSprintEnergy(false, GetPlayerDataMaxSprintEnergy() / (fRestoreDuration));
		}
		else
		{
			CPedMotionData& motionData = *pPlayerPed->GetMotionData();
			if (bUnderwater)
			{
				// Restore sprint energy at a lower rate if running but not sprinting 
				// or walking, for some reason the mbr underwater is capped
				if(motionData.GetIsWalking() || motionData.GetIsRunning())
					HandleSprintEnergy(false, GetPlayerDataMaxSprintEnergy() / RUNNING_RESTORE_DURATION);
				// Restore sprint energy if not running or sprinting
				else if(!motionData.GetIsSprinting())
					HandleSprintEnergy(false, GetPlayerDataMaxSprintEnergy() / NOT_RUNNING_RESTORE_DURATION);
			}
			else
			{
				// Restore sprint energy at a lower rate if running but not sprinting
				if(motionData.GetIsRunning())
					HandleSprintEnergy(false, GetPlayerDataMaxSprintEnergy() / RUNNING_RESTORE_DURATION);
				// Restore sprint energy if not running or sprinting
				else if(!motionData.GetIsSprinting())
					HandleSprintEnergy(false, GetPlayerDataMaxSprintEnergy() / NOT_RUNNING_RESTORE_DURATION);
			}
		} 
	}

	if(pPlayerPed->GetIsDeadOrDying())
	{
		GetTargeting().ClearLockOnTarget();
		return true;
	}

	// Cache off the button inputs since we cannot check for inputs in ProcessPostRender as they have already been reset by that time
	if( pControl && !CControlMgr::IsDisabledControl( const_cast<CControl*>(pControl) ) )
	{
		m_bWasLightAttackPressed = pControl->GetMeleeAttackLight().IsPressed();
		m_bWasAlternateAttackPressed = pControl->GetMeleeAttackAlternate().IsPressed();
	}
	else
	{
		m_bWasLightAttackPressed = false;
		m_bWasAlternateAttackPressed = false;
	}

	// Every so often if we are inside an interior & standing on an object, we create a shocking event.
	// This is intended to make peds look if we're standing on tables, etc. (however it might not work
	// quite as planned as many of the interior furniture is placed as part of the building)
	if(pPlayerPed->GetPortalTracker()->IsInsideInterior() && pGroundPhysical && pGroundPhysical->GetIsTypeObject())
	{
		m_fTimeToNextCreateInteriorShockingEvents -= fwTimer::GetTimeStep();
		if(m_fTimeToNextCreateInteriorShockingEvents <= 0.0f)
		{
			CEventShockingRunningPed ev(*pPlayerPed);
			CShockingEventsManager::Add(ev);
			m_fTimeToNextCreateInteriorShockingEvents = ms_fInteriorShockingEventFreq;
		}
	}

	// Store whether we were firing this frame
	pPlayerPed->SetPedConfigFlag( CPED_CONFIG_FLAG_WasFiring, pPlayerPed->GetPedConfigFlag( CPED_CONFIG_FLAG_IsFiring ) );
	pPlayerPed->SetPedConfigFlag( CPED_CONFIG_FLAG_IsFiring, pPlayerPed->GetPlayerInfo()->IsFiring() );

	// Store whether we are aiming
	bool bIsAiming = pPlayerPed->GetPlayerInfo()->IsAiming();

	if(!IsSprintAimBreakOutOver())
	{
		if((pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsFiring) && !pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_WasFiring)) || (bIsAiming && !m_bWasAiming))
		{
			// Clear timer
			m_uSprintAimBreakOutTime = 0;
		}
	}

#if FPS_MODE_SUPPORTED
	if(pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ExitingFPSCombatRoll) && !pPlayerPed->GetMotionData()->GetCombatRoll())
	{
		pPlayerPed->SetPedConfigFlag(CPED_CONFIG_FLAG_ExitingFPSCombatRoll, false);
		m_fExitCombatRollToScopeTimerInFPS = 0.0f;
	}

	bool bFPSMode = pPlayerPed->IsFirstPersonShooterModeEnabledForPlayer(false);
	if (bFPSMode)
	{
		if(pPlayerPed->GetMotionData()->GetCombatRoll() && pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InFPSUnholsterTransition))
		{
			pPlayerPed->SetPedConfigFlag(CPED_CONFIG_FLAG_ExitingFPSCombatRoll, true);
		}

		if(pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ExitingFPSCombatRoll))
		{
			m_fExitCombatRollToScopeTimerInFPS += fwTimer::GetTimeStep();
		}
	}
	else
	{
		m_fCoverHeadingCorrectionAimHeading = FLT_MAX;
	}
#endif

	m_bWasAiming = bIsAiming;

	ProcessFireProofTimer();

#if FPS_MODE_SUPPORTED
	TUNE_GROUP_BOOL(FIRST_PERSON_TUNE, USE_FPS_FIRING_BOUNDARIES, false);
	if(USE_FPS_FIRING_BOUNDARIES && pPlayerPed->IsLocalPlayer() && pPlayerPed->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		float fFiringAttackValue = GetFiringAttackValue();
		if(fFiringAttackValue > ms_PlayerFPSFireBoundaryHigh)
		{
			m_bUseHighFiringAttackBoundary = true;
		}

		if(fFiringAttackValue < ms_PlayerFPSFireBoundaryLow)
		{
			m_bUseHighFiringAttackBoundary = false;
		}
	}
	else
	{
		m_bUseHighFiringAttackBoundary = true;
	}
#endif

	return true;
} // end - CPlayerInfo::ProcessControl

#if FPS_MODE_SUPPORTED
void CPlayerInfo::ProcessFPSState(bool bIgnoreWeaponWheel)
{
	CPed* pPlayerPed = GetPlayerPed();
	pedFatalAssertf( pPlayerPed, "No player ped!" );


#if FPS_MODE_SUPPORTED && KEYBOARD_MOUSE_SUPPORT
	// B*2318618: Set scope state based on menu preference if it's been modified. 
	if (sm_bShouldUpdateScopeStateFromMenu)
	{
		sm_bShouldUpdateScopeStateFromMenu = false;
		eDefaultAimTypeFPS menuPref = static_cast<eDefaultAimTypeFPS>(CPauseMenu::GetMenuPreference(PREF_FPS_DEFAULT_AIM_TYPE));
		switch(menuPref)
		{
		case FPS_AIM_NORMAL:
			m_bInFPSScopeState = false;
			break;
		case FPS_AIM_IRON_SIGHTS:
			m_bInFPSScopeState = true;
			break;
		default:
			m_bInFPSScopeState = false;
			break;
		}
	}
#endif	// FPS_MODE_SUPPORTED && KEYBOARD_MOUSE_SUPPORT

	if(!bIgnoreWeaponWheel && CNewHud::IsShowingHUDMenu())
	{
		// Update the previous fps state to current state to avoid looping in transitions.
		if(pPlayerPed->GetMotionData() && pPlayerPed->GetMotionData()->GetPreviousFPSState() == pPlayerPed->GetMotionData()->GetFPSState())
		{
			return;
		}
	}

	// Do not update FPS state when controls are disabled
	TUNE_GROUP_BOOL(FIRST_PERSON_TUNE, UPDATE_FPS_STATE_WHEN_CONTROLS_DISABLED, true);
	if (!UPDATE_FPS_STATE_WHEN_CONTROLS_DISABLED && pPlayerPed->GetPlayerInfo() && pPlayerPed->GetPlayerInfo()->AreControlsDisabled())
	{
		return;
	}

	const int viewMode = camControlHelper::GetViewModeForContext(camControlHelperMetadataViewMode::ON_FOOT);
	bool bFPSMode = pPlayerPed->IsFirstPersonShooterModeEnabledForPlayer(false) || (m_bForceFPSRNGState && viewMode == camControlHelperMetadataViewMode::FIRST_PERSON);

	if(pPlayerPed->GetMotionData())
	{
		pPlayerPed->GetMotionData()->SetUsingFPSMode(pPlayerPed->IsFirstPersonShooterModeEnabledForPlayer(false));
	}

	// Set current FPS state based on player input

	CPedWeaponManager* pWeapMgr = pPlayerPed->GetWeaponManager();
	const CWeaponInfo* pEquippedWeaponInfo = pWeapMgr && pWeapMgr->GetEquippedWeapon() ? pWeapMgr->GetEquippedWeapon()->GetWeaponInfo() : NULL;

	if (bFPSMode && pEquippedWeaponInfo)
	{
		int iState = CPedMotionData::FPS_IDLE;	//Relaxed/idle pose 
		bool bSprint = ControlButtonSprint(CPlayerInfo::SPRINT_ON_FOOT) > 1.0f;
		bool bPlayerIsInCutscene = (pPlayerPed->GetPedIntelligence() && pPlayerPed->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_CUTSCENE));
		bool bIsUnarmed = pEquippedWeaponInfo->GetIsUnarmed() || pEquippedWeaponInfo->GetIsMeleeFist();
		bool bUseIdleStateOnly = (pEquippedWeaponInfo->GetIsMelee() && !pEquippedWeaponInfo->GetCanBeAimedLikeGunWithoutFiring() && !bIsUnarmed) || pEquippedWeaponInfo->GetEnableFPSIdleOnly() || bPlayerIsInCutscene;
		const bool bFirstPersonLookBehind = camInterface::GetGameplayDirector().GetFirstPersonShooterCamera() ? camInterface::GetGameplayDirector().GetFirstPersonShooterCamera()->IsLookingBehind() : false;
		TUNE_GROUP_FLOAT(AIMING_DEBUG, fTimeToStayInRunAndGunFPS, 5.0f, 0.0f, 20.0f, 0.01f);
		bool bStayInRNG = false;
		bool bFiring = IsFiring();

		//If locking on to friendly targets while unarmed, disable FPS LT state and allow idle only
		if(bIsUnarmed)
		{
			CEntity* pTargetEntity = GetTargeting().GetLockOnTarget();		
			CPed* pTargetPed = (pTargetEntity && pTargetEntity->GetIsTypePed()) ? static_cast<CPed*>(pTargetEntity) : NULL;
			if( IsSoftAiming(false) ||  (pTargetPed && pTargetPed->GetPedConfigFlag(CPED_CONFIG_FLAG_AllowPlayerLockOnIfFriendly) && pTargetPed->GetPedType() == PEDTYPE_ANIMAL))
			{
				bUseIdleStateOnly = true;
			}
		}

		if (pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_FiringWeapon) || pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_FiringWeaponWhenReady))
		{
			bFiring = true;
			m_bFiredGunInFPS = true;
			m_fRNGTimerFPS = 0.0f; //Reset run and gun timer when firing
			if(bSprint)
			{
				m_bFiredDuringFPSSprint = true;
			}
		}

		// B*2038645 - Stay in RNG mode if we swap between characters if the character we're switching to is aiming. Force flag/timer reset in CGameWorld::ChangePlayerPed.
		if (m_bForceFPSRNGState)
		{	
			m_bFiredGunInFPS = true;
			bStayInRNG = true;

			// Clear the force aiming/rng flags once we're fully in FPS camera mode again
			if (pPlayerPed->IsFirstPersonShooterModeEnabledForPlayer(false))
			{
				m_bForceFPSRNGState = false;
				pPlayerPed->SetPedConfigFlag(CPED_CONFIG_FLAG_SimulatingAiming, false);
			}
		}

		bool bDisableRNG = bIsUnarmed;
		bool bThrownWeapon =  pEquippedWeaponInfo->GetIsThrownWeapon() && pEquippedWeaponInfo->GetUseFPSAimIK() && !pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ThrowingGrenadeWhileAiming);
		bool bJerryCan = pEquippedWeaponInfo->GetGroup() == WEAPONGROUP_PETROLCAN;
		// Stay in RNG if timer is still going
		if ((m_bWasRunAndGunning || m_bFiredInFPSFullAimState) && !bDisableRNG && !bThrownWeapon && m_bFiredGunInFPS && (m_fRNGTimerFPS <= fTimeToStayInRunAndGunFPS))
		{
			if(pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_IsReloading))
			{
				m_fRNGTimerFPS = 0.0f; //Reset run and gun timer when reloading
				bStayInRNG = true;
			}
			else if (!bFiring && bSprint)
			{
				// Leave RNG mode immediately if sprinting
				bStayInRNG = false;
			}
			else
			{
				bStayInRNG = true;
			}
		}

		TUNE_GROUP_BOOL(FIRST_PERSON_COVER_TUNE, DISABLE_STAY_IN_RNG_WHEN_NOT_AIMING_DIRECT_FROM_COVER, true);
		bool bShouldForceAimingStateDueToAimingOutFromCover = false;
		bool bAimingDirectlyInCover = false;
		if (DISABLE_STAY_IN_RNG_WHEN_NOT_AIMING_DIRECT_FROM_COVER && pPlayerPed->GetIsInCover())
		{
			bAimingDirectlyInCover = CTaskCover::IsPlayerAimingDirectlyInFirstPerson(*pPlayerPed);
			if (!bAimingDirectlyInCover)
			{
				if (pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_IsAiming))
				{
					bShouldForceAimingStateDueToAimingOutFromCover = true;
				}
				bStayInRNG = false;
			}
		}

		if(bJerryCan && !IsFiring())
		{
			// Quit RNG mode immediately if player stops dumping fuel from the jerry can
			bStayInRNG = false;
			m_bFiredGunInFPS = false;
			m_bWasRunAndGunning = false;
			m_bFiredDuringFPSSprint = false;
			m_fRNGTimerFPS = 0.0f;
		}

		if (!CTaskMobilePhone::IsRunningMobilePhoneTask(*pPlayerPed) && !bUseIdleStateOnly)
		{

			// B*2555616: Stay in same FPS state while firing and not at an interruptible point (was causing a pop when restarting aiming tasks, v noticable with long fire anims ie the revolver). 
			//			  Also don't allow toggling between scope/iron sights while firing.
			//			  Only do this for certain weapons at this point. Most weapon fire anims (particularly automatic weapons) are short and don't tend to move the gun around as much.
			bool bFiredWhileAiming = false;
			bool bFiredWhileRNG = false;
			if (pEquippedWeaponInfo->GetBlockFirstPersonStateTransitionWhileFiring())
			{
				CTaskAimGunOnFoot* pAimGunOnFootTask = pPlayerPed->GetPedIntelligence() ? static_cast<CTaskAimGunOnFoot*>(pPlayerPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_ON_FOOT)) : NULL;
				if (pAimGunOnFootTask && pAimGunOnFootTask->GetIsFiring() && !pAimGunOnFootTask->GetIsFireInterruptible())
				{
					bFiredWhileRNG = (pPlayerPed->GetMotionData()->GetWasFPSRNG() || pPlayerPed->GetMotionData()->GetWasFPSIdle());
					bFiredWhileAiming = (pPlayerPed->GetMotionData()->GetWasFPSLT() || pPlayerPed->GetMotionData()->GetWasFPSScope());
				}
			}

			const bool bAimingInCoverButNotDirectly = pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_IsAimingFromCover) && !bAimingDirectlyInCover;
			if ((bAimingInCoverButNotDirectly || IsAiming(!bThrownWeapon) || bFiredWhileAiming) 
				 && !IsRunAndGunning(bThrownWeapon || bJerryCan, bThrownWeapon)
				 && !pEquippedWeaponInfo->GetEnableFPSRNGOnly() && 
				 (!pEquippedWeaponInfo->GetOnlyAllowFiring() || IsFiring()) 
				 && !pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_IsEnteringCover)
				 && !bFiredWhileRNG)
			{
				iState = CPedMotionData::FPS_LT;	//Left trigger full aim

				// Toggle scope if R3 pressed while aiming
				CControl *pControl = pPlayerPed->GetControlFromPlayer();

				// B*2204503: Don't process scope state while using a sniper rifle.
				const bool bWeaponHasFirstPersonScope = pPlayerPed->GetWeaponManager() && pPlayerPed->GetWeaponManager()->GetEquippedWeaponHasFirstPersonScope();
				if(!pEquippedWeaponInfo->GetDisableFPSScope() && pControl && !bWeaponHasFirstPersonScope && !bFiredWhileAiming)
				{
#if KEYBOARD_MOUSE_SUPPORT
					// B* 2032581: The way the mouse wheel works we have to always toggle regardless of meta data settings. The reason
					// for this is that the wheel cannot be held forwards or backwards. To make this work more like the sniper rifle on
					// the mouse, we will use wheel forward for enter accurate aim and wheel backwards for exit accurate aim. On the
					// keyboard, this will be two different buttons.
					if(pControl->GetPedAccurateAim().GetSource().m_DeviceIndex == ioSource::IOMD_KEYBOARD_MOUSE)
					{
						const float aimValue = pControl->GetPedAccurateAim().GetUnboundNorm();

						// Positive is wheel up/forwards.
						if(aimValue > ioValue::BUTTON_DOWN_THRESHOLD)
						{
							m_bInFPSScopeState = false;
						}
						else if(aimValue < -ioValue::BUTTON_DOWN_THRESHOLD)
						{
							m_bInFPSScopeState = true;
						}
					}
					else
#endif // KEYBOARD_MOUSE_SUPPORT
					if(pControl && pControl->GetPedAccurateAim().IsPressed())
					{
						m_bInFPSScopeState = !m_bInFPSScopeState;
					}
				}
				TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, fCombatRollExitSwitchToScopeZoomTime, 0.15f, 0.0f, 5.0f, 0.01f);
				bool bCombatRollIgnoreScope = pPlayerPed->GetMotionData()->GetCombatRoll() && m_fExitCombatRollToScopeTimerInFPS < fCombatRollExitSwitchToScopeZoomTime;
				bool bParachuteIgnoresScope = pPlayerPed->GetIsParachuting();
				const bool bReloadingInCover = pPlayerPed->GetIsInCover() && pPlayerPed->GetPedIntelligence()->FindTaskSecondaryByType(CTaskTypes::TASK_RELOAD_GUN);
				const bool bShouldDisableFPSAimForScope = bWeaponHasFirstPersonScope && pEquippedWeaponInfo->GetDisableFPSAimForScope();

				// Go to scope state directly if we are aiming a sniper rifle.
				/*if(pEquippedWeaponInfo->GetHasFirstPersonScope() && IsAiming())
				{
					bCombatRollIgnoreScope  = false;
				}*/

				if (!pEquippedWeaponInfo->GetDisableFPSScope() && (m_bInFPSScopeState || bShouldDisableFPSAimForScope) && !bCombatRollIgnoreScope && !bReloadingInCover && !bParachuteIgnoresScope)
				{
					iState = CPedMotionData::FPS_SCOPE; //Scoped/iron sights aim
				}

				// Start a timer if we're firing so we go to RNG if we let go of LT
				if (IsFiring())
				{
					m_bFiredInFPSFullAimState = true;
				}
				if (m_bFiredInFPSFullAimState)
				{
					m_fRNGTimerFPS += fwTimer::GetTimeStep();
					// Don't go back to RNG if we've surpassed the max time. Go straight back to idle instead.
					if (m_fRNGTimerFPS > fTimeToStayInRunAndGunFPS)
					{
						m_bFiredInFPSFullAimState = false;
						m_fRNGTimerFPS = 0.0f;
					}
				}
				//B*2014264: Reset the timer if we come back into an aim state whilst the RNG timer is ticking
				if (!m_bFiredInFPSFullAimState && m_fRNGTimerFPS > 0.0f)
				{
					m_fRNGTimerFPS = 0.0f;
				}
			}
			else if (!bFirstPersonLookBehind && !bDisableRNG && (IsRunAndGunning(bThrownWeapon || bJerryCan, bThrownWeapon) || bStayInRNG || m_bFiredInFPSFullAimState))
			{
				iState = CPedMotionData::FPS_RNG;	//Run and gun
				m_bWasRunAndGunning = true;

				// If the sprint button is held, we defer to decision of whether to add more time for later (still keeping the timer running)
				// This is also applies to LT firing as we could do aim->fire->sprint_held->rng (and other cases), in which case we want to add time back on the timer (if needed)
				// during the sprint_held->rng transition and not earlier. 
				bool bFiredBeforeSwitchingToRNG = (m_bFiredInFPSFullAimState || m_bFiredDuringFPSSprint) && !bSprint;

				// Put 2 seconds back on the timer if we've swapped from full aiming/sprinting to ensure we don't just flick into the RNG state for a v short amount of time
				if (bFiredBeforeSwitchingToRNG && (fTimeToStayInRunAndGunFPS - m_fRNGTimerFPS < 3.0f) )
				{
					m_fRNGTimerFPS = fTimeToStayInRunAndGunFPS - 3.0f;
					m_bFiredInFPSFullAimState = false;
					m_bFiredDuringFPSSprint = false;
				}
				else if (bFiredBeforeSwitchingToRNG)
				{
					// Else un-set the fired from full aim flag
					m_bFiredInFPSFullAimState = false;
					m_bFiredDuringFPSSprint = false;
				}
				// Only start timer once we've stopped RNG'ing
				if (bStayInRNG || m_bFiredInFPSFullAimState)
				{
					m_fRNGTimerFPS += fwTimer::GetTimeStep();
					if(m_fRNGTimerFPS > fTimeToStayInRunAndGunFPS)
					{
						m_bFiredInFPSFullAimState = false;
						m_bFiredDuringFPSSprint = false;
					}
				}
			}
			else if (bShouldForceAimingStateDueToAimingOutFromCover && !bThrownWeapon)
			{
				// Stay in our current state when aiming out from cover (cover task handles outros)
				// Don't do this for thrown weapons, we don't need to and it causes B*2028622
				iState = pPlayerPed->GetMotionData()->GetFPSState();
			}
		}

		pPlayerPed->GetMotionData()->SetCurrentFPSState(iState);
		pPlayerPed->GetMotionData()->SetIsUsingStealthInFPS(pPlayerPed->GetMotionData()->GetUsingStealth());

		if (pPlayerPed->GetMotionData()->GetPreviousFPSState() != iState)
		{
			// Reset flags when idle
			if (pPlayerPed->GetMotionData()->GetIsFPSIdle())
			{
				// If we are changing to FPS_Idle, reset RNG state, unless we were forced to FPS_Idle by look behind.
				if(!bFirstPersonLookBehind || !bStayInRNG)
				{
					m_bFiredGunInFPS = false;
					m_bWasRunAndGunning = false;
					m_bFiredDuringFPSSprint = false;
					m_fRNGTimerFPS = 0.0f;
				}
				else
				{
					m_fRNGTimerFPS = Min(m_fRNGTimerFPS, fTimeToStayInRunAndGunFPS*0.50f);
				}
			}
		}
	}
	else
	{
		// Reset timers if switching third/first person
		m_bFiredGunInFPS = false;
		m_bWasRunAndGunning = false;
		m_bFiredDuringFPSSprint = false;
		m_bFiredInFPSFullAimState = false;
		m_fRNGTimerFPS = 0.0f;
	}
}

bool CPlayerInfo::IsFirstPersonModeSupportedForPed(const CPed& ped)
{
	// Not for animals.
	if (ped.GetPedType() == PEDTYPE_ANIMAL)
	{
		return false;
	}

	// Not for the sasquatch.
	const CPedModelInfo* pModelInfo = ped.GetPedModelInfo();
	if (pModelInfo && pModelInfo->GetModelNameHash() == MI_PED_ORLEANS.GetName().GetHash())
	{
		return false;
	}

	return true;
}
#endif // FPS_MODE_SUPPORTED

bool CPlayerInfo::CanUseCover() const
{
	return bCanUseCover && !m_pPlayerPed->GetHasJetpackEquipped() && !m_pPlayerPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_DISABLE_CAN_USE_COVER) && !CPedType::IsAnimalType(m_pPlayerPed->GetPedType());
}


// Name			:	ProcessPreCamera
// Purpose		:	Processes before camera update.
// Parameters	:	None
// Returns		:	Nothing
bool CPlayerInfo::ProcessPreCamera()
{
	if(m_pPlayerPed)
	{
		GetTargeting().ProcessPreCamera();
	}
	return true;
}

// PURPOSE:	To reset some variable prior to script running
void CPlayerInfo::ResetPreScript()
{
	// Reset the suppressed crimes for this player
	m_Wanted.ResetSuppressedCrimes();
	m_Wanted.ResetFlags();
	m_Wanted.m_DisableCallPoliceThisFrame = false;
}

void CPlayerInfo::ProcessLightEffects()
{
#if LIGHT_EFFECTS_SUPPORT
	//Only adjust the control light effects for the Local Player
	if (m_pPlayerPed && m_pPlayerPed->IsLocalPlayer())
	{
		const u32 c_MaxFlashDuration	= 1000;
		const u32 c_MinFlashDuration	= 300;
		const u32 c_LevelFlashScale		= 165;

		// Even if control is disabled, still want to update light-effect color.
		CControl& control = CControlMgr::GetPlayerMappingControl();
		const bool gameWorldHidden = camInterface::IsFadedOut() || CLoadingScreens::AreActive();
		if (gameWorldHidden &&
	#if LIGHT_EFFECT_FLASH_FOR_LOW_HEALTH
			control.IsCurrentLightEffect(&m_FlashingPlayerLightEffect, CControl::PLAYER_LIGHT_EFFECT) == false &&
	#endif
			control.IsCurrentLightEffect(&m_WantedLightEffect, CControl::CODE_LIGHT_EFFECT) == false)
		{
			// Do not change light effect color when world is not rendered, unless we are flashing for wanted level.
			return;
		}

		{
		#if LIGHT_EFFECT_FLASH_FOR_LOW_HEALTH
			const float c_fBadlyHurtThreshold = m_pPlayerPed->GetInjuredHealthThreshold() + ((m_pPlayerPed->GetHurtHealthThreshold() - m_pPlayerPed->GetInjuredHealthThreshold()) * 0.50f);
			if ( m_pPlayerPed->GetHealth() > c_fBadlyHurtThreshold &&
				 (m_pPlayerPed->GetPlayerWanted()->GetWantedLevel() != WANTED_CLEAN || CScriptHud::iFakeWantedLevel > 0) &&
				!CPauseMenu::IsActive() &&
				!camInterface::GetSwitchDirector().IsRendering() &&
				!m_pPlayerPed->GetIsArrested() &&
				!m_pPlayerPed->IsDead() )
		#else
			if ( (m_pPlayerPed->GetPlayerWanted()->GetWantedLevel() != WANTED_CLEAN || CScriptHud::iFakeWantedLevel > 0) &&
				!CPauseMenu::IsActive() &&
				!camInterface::GetSwitchDirector().IsRendering() &&
				!m_pPlayerPed->GetIsArrested() &&
				!m_pPlayerPed->IsDead() )
		#endif
			{
				// TODO: un-hardcode values?
				u32 uWantedLevel;
				
				if(m_pPlayerPed->GetPlayerWanted()->GetWantedLevel() != WANTED_CLEAN)
				{
					uWantedLevel = (u32)m_pPlayerPed->GetPlayerWanted()->GetWantedLevel() - 1;	// 0 to 4 inclusive
				}
				else
				{
					uWantedLevel = (u32)CScriptHud::iFakeWantedLevel - 1;	// 0 to 4 inclusive
				}

				u32 uToggleTime = c_MaxFlashDuration - (uWantedLevel) * c_LevelFlashScale;
				if (uToggleTime < c_MinFlashDuration)
				{
					uToggleTime = c_MinFlashDuration;	// Avoid triggering seizure.  (5Hz-70Hz, which is 14ms to 200ms, is bad, http://www.birket.com/technical-library/144)
				}

				if(control.IsCurrentLightEffect(&m_WantedLightEffect, CControl::CODE_LIGHT_EFFECT) == false || uToggleTime != m_WantedLightEffect.GetDuration())
				{
					m_WantedLightEffect = fwWantedLightEffect(uToggleTime);
					control.SetLightEffect(&m_WantedLightEffect, CControl::CODE_LIGHT_EFFECT);
				}
			}
			else
			{
				control.ClearLightEffect(&m_WantedLightEffect, CControl::CODE_LIGHT_EFFECT);

				// Note: cannot turn off the light bar.
				Color32 oPlayerColor(160, 160, 160, 255);		// arbitrary default
				bool bColorSet = false;
				if (NetworkInterface::IsGameInProgress())
				{
					if (NetworkInterface::GetLocalPlayer())
					{
						int sPlayerTeam = NetworkInterface::GetLocalPlayer()->GetTeam();
						if (sPlayerTeam != INVALID_TEAM)
						{
							NetworkColours::NetworkColour eTeamColour = NetworkColours::GetTeamColour(sPlayerTeam);
							if (eTeamColour != NetworkColours::INVALID_COLOUR)
							{
								oPlayerColor = (eTeamColour == NetworkColours::NETWORK_COLOUR_CUSTOM) ?
									NetworkColours::GetCustomTeamColour(sPlayerTeam) :
									NetworkColours::GetNetworkColour(eTeamColour);
								bColorSet = true;
							}
						}
						else
						{
							const rlClanDesc& clan = NetworkInterface::GetLocalPlayer()->GetClanDesc();
							if(clan.IsValid())
							{
								oPlayerColor.Set(clan.m_clanColor);
								bColorSet = true;
							}
						}
					}
				}
				else
				{
					switch (m_pPlayerPed->GetPedType())
					{
						// TODO: for now hardcode, need to read from timecycle modifier eventually.
						case PEDTYPE_PLAYER_0: oPlayerColor = CHudColour::GetRGBA(HUD_COLOUR_CONTROLLER_MICHAEL);	bColorSet = true;	break;		// Michael	light cyan
						case PEDTYPE_PLAYER_1: oPlayerColor = CHudColour::GetRGBA(HUD_COLOUR_CONTROLLER_FRANKLIN);	bColorSet = true;	break;		// Franklin	light green
						case PEDTYPE_PLAYER_2: oPlayerColor = CHudColour::GetRGBA(HUD_COLOUR_CONTROLLER_TREVOR);	bColorSet = true;	break;		// Trevor	light orange
						default: break;
					}
				}

			#if LIGHT_EFFECT_FLASH_FOR_LOW_HEALTH
				const float c_fHurtThreshold = m_pPlayerPed->GetHurtHealthThreshold();
				if (bColorSet && m_pPlayerPed->GetHealth() < c_fHurtThreshold && !m_pPlayerPed->IsDead())
				{
					u32 uToggleTime = c_MinFlashDuration;
					if (m_pPlayerPed->GetPlayerWanted()->GetWantedLevel() == WANTED_CLEAN)
					{
						// TODO: un-hardcode values?
						float fHealthRatio = Min(5.0f * m_pPlayerPed->GetHealth() / c_fHurtThreshold, 4.99f);
						u32 uHealthLevel = (u32)fHealthRatio;	// 0 to 4 inclusive
						u32 uToggleTime = c_MaxFlashDuration - (uHealthLevel) * c_LevelFlashScale;
						if (uToggleTime < c_MinFlashDuration)
						{
							uToggleTime = c_MinFlashDuration;	// Avoid triggering seizure.  (5Hz-70Hz, which is 14ms to 200ms, is bad, http://www.birket.com/technical-library/144)
						}
					}

					if( control.IsCurrentLightEffect(&m_FlashingPlayerLightEffect, CControl::PLAYER_LIGHT_EFFECT) == false ||
						uToggleTime != m_FlashingPlayerLightEffect.GetDuration() ||
						oPlayerColor.GetRed() != m_FlashingPlayerLightEffect.GetRed() ||
						oPlayerColor.GetGreen() != m_FlashingPlayerLightEffect.GetGreen() ||
						oPlayerColor.GetBlue() != m_FlashingPlayerLightEffect.GetBlue() )
					{
						m_FlashingPlayerLightEffect = ioFlashingLightEffect(uToggleTime, 0.40f, oPlayerColor);
						control.SetLightEffect(&m_FlashingPlayerLightEffect, CControl::PLAYER_LIGHT_EFFECT);
					}
				}
				else
			#endif // LIGHT_EFFECT_FLASH_FOR_LOW_HEALTH
				{
					if (bColorSet)
					{
						if( control.IsCurrentLightEffect(&m_PlayerLightEffect, CControl::PLAYER_LIGHT_EFFECT) == false ||
							oPlayerColor.GetRed() != m_PlayerLightEffect.GetRed() ||
							oPlayerColor.GetGreen() != m_PlayerLightEffect.GetGreen() ||
							oPlayerColor.GetBlue() != m_PlayerLightEffect.GetBlue() )
						{
							m_PlayerLightEffect = ioConstantLightEffect(oPlayerColor);
							control.SetLightEffect(&m_PlayerLightEffect, CControl::PLAYER_LIGHT_EFFECT);
						}
					}
					//Dont clear previous colour: B*1763685 - during transition's if the player is NULL ( m_pPlayerPed )
					// it will flash the controller. So not clearing the color will maintain it to the Crew/Team colour.
					//else
					//{
					//	control.ResetLightDeviceColor();
					//}
 				}
			}
		}
	}
#endif // LIGHT_EFFECTS_SUPPORT
}

// PURPOSE:  Used to dampen the vertical motion on the local root (to stop bobbing when the camera is in close)
void CPlayerInfo::ProcessDampenRoot()
{
	TUNE_GROUP_BOOL(DAMPEN_ROOT, bOverrideDampenRootTargetWeight, false);
	TUNE_GROUP_FLOAT ( DAMPEN_ROOT, fOverridenDampenRootTargetWeight, 1.0f, 0.0f, 1.0f, 0.001f);
	if(bOverrideDampenRootTargetWeight)
	{
		m_fDampenRootTargetWeight = fOverridenDampenRootTargetWeight;
	}

	TUNE_GROUP_BOOL(DAMPEN_ROOT, bOverrideDampenRootTargetHeight, false);
	TUNE_GROUP_FLOAT ( DAMPEN_ROOT, fOverridenDampenRootTargetHeight, 0.0f, -1.0f, 1.0f, 0.001f);
	if(bOverrideDampenRootTargetHeight)
	{
		m_fDampenRootTargetHeight = fOverridenDampenRootTargetHeight;
	}

	if(GetPlayerResetFlags().IsFlagSet(CPlayerResetFlags::PRF_CAMERA_VIEW_MODE_SWITCHED_TO_OR_FROM_FIRST_PERSON))
	{
		// Snap
		m_fDampenRootWeight = m_fDampenRootTargetWeight;
	}
	else
	{
		if (m_fDampenRootWeight <= m_fDampenRootTargetWeight)
		{
			m_fDampenRootWeight += ms_fDampenRootRate*fwTimer::GetTimeStep();
			m_fDampenRootWeight = MIN(m_fDampenRootWeight, m_fDampenRootTargetWeight);
		}
		else
		{
			m_fDampenRootWeight -= ms_fDampenRootRate*fwTimer::GetTimeStep();
			m_fDampenRootWeight = MAX(m_fDampenRootWeight, m_fDampenRootTargetWeight);
		}
	}

	if (m_fDampenRootWeight > 0.0f)
	{
		CPed* pPlayerPed = GetPlayerPed();
		if (pPlayerPed)
		{
			float fDampenedRootZ = Lerp(m_fDampenRootWeight, RC_MATRIX34(pPlayerPed->GetSkeleton()->GetLocalMtx(0)).d.z, m_fDampenRootTargetHeight);
			RC_MATRIX34(pPlayerPed->GetSkeleton()->GetLocalMtx(0)).d.z = fDampenedRootZ;
			RC_MATRIX34(pPlayerPed->GetSkeleton()->GetObjectMtx(0)).d.z = fDampenedRootZ;
			pPlayerPed->GetSkeleton()->Update();
		}
	}
}

bool CPlayerInfo::FindClosestCarCB(CEntity* pEntity, void* data)
{
	// NB : Return 'true' to continue enumerating vehicles from CGameWorld::ForAllEntitiesIntersecting()..
	if (!Verifyf(pEntity->GetIsTypeVehicle(), "Entity %s is not of type vehicle", pEntity->GetModelName()))
	{
		return true;
	}

#if __ASSERT
	if (!Verifyf(static_cast<CVehicle*>(pEntity)->GetLayoutInfo(), "Vehicle %s has no layout info, are your vehicles.meta and vehiclelayouts.meta out of sync?", static_cast<CVehicle*>(pEntity)->GetDebugName()))
	{
		return true;
	}
#endif // __ASSERT

	CVehicle* pVehicle = static_cast<CVehicle*>(pEntity);
	if (!pVehicle->IsCollisionEnabled())
	{
		AI_LOG_REJECTION_WITH_IDENTIFIER_AND_REASON("[VehicleEntryExit][CPlayerInfo::ScanForVehicleToEnter]", pVehicle, "disabled collision");
		return true;
	}

	if (!pVehicle->m_nVehicleFlags.bConsideredByPlayer)
	{
		AI_LOG_REJECTION_WITH_IDENTIFIER_AND_REASON("[VehicleEntryExit][CPlayerInfo::ScanForVehicleToEnter]", pVehicle, "vehicle isn't considered by player");
		return true;
	}

	FindClosestCarCBData* pCBData = static_cast<FindClosestCarCBData*>(data);
	const bool bStuckInWater = IsVehicleStuckInWater(pVehicle);
	if (bStuckInWater)
	{
		AI_LOG_REJECTION_WITH_IDENTIFIER_AND_REASON("[VehicleEntryExit][CPlayerInfo::ScanForVehicleToEnter]", pVehicle, "stuck in water");
		return true;
	}

	if (!pVehicle->GetVehicleDamage()->GetIsDriveable(true, true, true))
	{
		AI_LOG_REJECTION_WITH_IDENTIFIER_AND_REASON("[VehicleEntryExit][CPlayerInfo::ScanForVehicleToEnter]", pVehicle, "vehicle undriveable");
		return true;
	}

	const CPed* pPed = pCBData->pPed;
	const bool bCanBoardVehicleWhenStoodOnTop = CanBoardVehicleWhenStoodOnTop(pVehicle);
	const bool bDisableVehicleEntryDueToLocks = (bCanBoardVehicleWhenStoodOnTop && !pVehicle->CanPedOpenLocks(pPed));	// Condition seems a bit odd, originally this was for boats only

	if (bDisableVehicleEntryDueToLocks)
	{
		AI_LOG_REJECTION_WITH_IDENTIFIER_AND_REASON("[VehicleEntryExit][CPlayerInfo::ScanForVehicleToEnter]", pVehicle, "ped can't open locks");
		return true;
	}

	if (ShouldDisableEntryForTrain(pVehicle, pPed))
	{
		return true;
	}

	if (ShouldDisableEntryForPlane(pVehicle, pPed))
	{
		return true;
	}

	if (ShouldDisableEntryForSyncedScene(pVehicle))
	{
		return true;
	}

	if (ShouldDisableEntryBecauseUpsideDown(pVehicle))
	{
		return true;
	}
	
	bool bPassedOriginalHeightCondition = false;
	const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	const bool bPedIsStandingOnThisVehicle = pPed->GetGroundPhysical() == pVehicle;
	if (ShouldDisableEntryDueToHeightCondition(bPassedOriginalHeightCondition, bPedIsStandingOnThisVehicle, bCanBoardVehicleWhenStoodOnTop, vPedPos, pPed, pVehicle))
	{
		AI_LOG_REJECTION_WITH_IDENTIFIER_AND_REASON("[VehicleEntryExit][CPlayerInfo::ScanForVehicleToEnter]", pVehicle, "height condition failed");
		return true;
	}

	// B*1649000
	if (pPed->IsLocalPlayer() && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PreventUsingLowerPrioritySeats))
	{
		aiAssertf(0, "Player shouldn't have this flag set ever");
		pCBData->pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_PreventUsingLowerPrioritySeats, false);
	}

	int iSeatIndex = -1;
	Vector3 vSeatPos;
	if (!CCarEnterExit::GetNearestCarSeat(*pPed,*pVehicle,vSeatPos,iSeatIndex))
	{
		AI_LOG_REJECTION_WITH_IDENTIFIER_AND_REASON("[VehicleEntryExit][CPlayerInfo::ScanForVehicleToEnter]", pVehicle, "couldn't find nearest seat");
		return true;
	}

	// If we didn't pass the original height condition, we must check that the seat we're potentially entering
	// should actually have used the altered height condition and reject it if it shouldn't have
	if (!bPassedOriginalHeightCondition && pVehicle->IsSeatIndexValid(iSeatIndex))
	{
		const s32 iEntryIndex = pVehicle->GetDirectEntryPointIndexForSeat(iSeatIndex);
		if (!CTaskVehicleFSM::CanPedWarpIntoHoveringVehicle(pCBData->pPed, *pVehicle, iEntryIndex))
		{
			AI_LOG_REJECTION_WITH_IDENTIFIER_AND_REASON("[VehicleEntryExit][CPlayerInfo::ScanForVehicleToEnter]", pVehicle, "couldn't warp into hovering vehicle");
			return true;
		}
	}

	Vector3 vDiff = vPedPos - vSeatPos;
	vDiff.z = 0;

	// Don't consider vehicles going too fast away from us unless we are standing on top of a vehicle we can board
	if (!bPedIsStandingOnThisVehicle || !bCanBoardVehicleWhenStoodOnTop)
	{
		if (ShouldDisableEntryBecauseVehicleMovingAway(vDiff, pPed, pVehicle))
		{
			AI_LOG_REJECTION_WITH_IDENTIFIER_AND_REASON("[VehicleEntryExit][CPlayerInfo::ScanForVehicleToEnter]", pVehicle, "vehicle moving away");
			return true;
		}
	}

	// Test the distance to this vehicle
	TUNE_GROUP_FLOAT(VEHICLE_ENTRY_TUNE, MAX_DISTANCE, 15.0f, 0.0f, 25.0f, 0.01f);
	float Distance = vDiff.Mag();
	float fDistanceLimit = MAX_DISTANCE;
	
	// if this is a vehicle we can enter from standing on, reduce the distance by the bounding radius (cos some of them are massive)
	if (bCanBoardVehicleWhenStoodOnTop)
	{
		fDistanceLimit += pEntity->GetBoundRadius();
	}

	if (Distance > fDistanceLimit)
	{
		CEntityBoundAI bound(*pEntity,vPedPos.z,pCBData->pPed->GetCapsuleInfo()->GetHalfWidth());
		if (bound.LiesInside(vPedPos))
		{
			Distance = fDistanceLimit;
		}
	}

	if (Distance > fDistanceLimit)
	{
		AI_LOG_REJECTION_WITH_IDENTIFIER_AND_REASON("[VehicleEntryExit][CPlayerInfo::ScanForVehicleToEnter]", pVehicle, "vehicle too far away");
		return true;
	}

	// Now actually evaluate the position of this car
	EvaluateCarPosition(pEntity, pCBData->pPed, vSeatPos, Distance, &pCBData->Closeness, &pCBData->pVehicle, pCBData->vSearchDir, pCBData->bStickInput);
	return true;
}

bool CPlayerInfo::CanBoardVehicleWhenStoodOnTop(const CVehicle* pVehicle)
{
	return pVehicle->InheritsFromBoat() || pVehicle->InheritsFromSubmarine() || pVehicle->GetLayoutInfo()->GetWarpInWhenStoodOnTop();
}

bool CPlayerInfo::ShouldDisableEntryForTrain(const CVehicle* pVehicle, const CPed* pPed)
{
	if (pVehicle->GetVehicleType() != VEHICLE_TYPE_TRAIN)
		return false;

	// The script deals with entering cable cars
	if (pVehicle->GetModelIndex() == MI_CAR_CABLECAR)
	{
		AI_LOG_REJECTION_WITH_REASON(pVehicle, "is a cablecar");
		return true;
	}

	// Stop the player entering trains from the wrong side
	const CTrain* pTrain = static_cast<const CTrain*>(pVehicle);
	const Vector3 vTrainToPlayer = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition() - pTrain->GetTransform().GetPosition());
	if (pTrain->m_nTrainFlags.iStationPlatformSides & CTrainTrack::Right)
	{
		if (DotProduct(vTrainToPlayer, VEC3V_TO_VECTOR3(pTrain->GetTransform().GetA())) < 0.0f)
		{
			AI_LOG_REJECTION_WITH_IDENTIFIER_AND_REASON("[VehicleEntryExit][CPlayerInfo::ShouldDisableEntryForTrain]",pVehicle, "on right side platform, train wrong way");
			return true;
		}
	}
	else
	{
		if (DotProduct(vTrainToPlayer, VEC3V_TO_VECTOR3(pTrain->GetTransform().GetA())) > 0.0f)
		{
			AI_LOG_REJECTION_WITH_IDENTIFIER_AND_REASON("[VehicleEntryExit][CPlayerInfo::ShouldDisableEntryForTrain]",pVehicle, "on left side platform, train wrong way");
			return true;
		}
	}

	return false;
}

bool CPlayerInfo::ShouldDisableEntryForPlane(const CVehicle* pVehicle, const CPed* pPed)
{
	if (pVehicle->GetVehicleType() != VEHICLE_TYPE_PLANE)
		return false;

	// Don't try and enter planes with door above wing when landing gears are up (climb animation forces them through ground). Player can still walk on the wing and enter from there.
	const CPlane* pPlane = static_cast<const CPlane*>(pVehicle);
	bool bLandingGearUp = pPlane->GetLandingGear().GetPublicState() != CLandingGear::STATE_LOCKED_DOWN;
	bool bStandingOnPlane = pPed->GetGroundPhysical() == pPlane;
	bool bDoorAboveWing = pPlane->GetModelIndex() == MI_PLANE_CUBAN.GetModelIndex() ||
		pPlane->GetModelIndex() == MI_PLANE_VELUM.GetModelIndex();

	if (bLandingGearUp && bDoorAboveWing && !bStandingOnPlane)
	{
		AI_LOG_REJECTION_WITH_IDENTIFIER_AND_REASON("[VehicleEntryExit][CPlayerInfo::ShouldDisableEntryForPlane]", pVehicle, "landing gear up, but ped on ground");
		return true;
	}

	// Plane vehicles can't be boarded when hovering, unless we are already on top
	// of the vehicle
	if (pVehicle->IsInAir() && pPed->GetGroundPhysical() != pVehicle)
	{
		// If there is a height difference bigger than half a meter from our position 
		// to the entry point of the driver's seat, discard the vehicle
		s32 iDriverEntryPoint = pVehicle->GetDirectEntryPointIndexForSeat(0);
		if (pVehicle->IsEntryIndexValid(iDriverEntryPoint))
		{
			// Get the drivers entry point position
			Vector3 vEntryPosition; Quaternion qEntryOrientation;
			CModelSeatInfo::CalculateEntrySituation(pVehicle, pPed, vEntryPosition, qEntryOrientation, iDriverEntryPoint);

			// Set the max tolerable height to enter the vehicle
			const float fMaxTolerableEntryHeight = 0.5f;
			const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			// If the vehicle is too high in the air discard it
			if (abs(vPedPosition.z - vEntryPosition.z) > fMaxTolerableEntryHeight)
			{
				AI_LOG_REJECTION_WITH_IDENTIFIER_AND_REASON("[VehicleEntryExit][CPlayerInfo::ShouldDisableEntryForPlane]",pVehicle, "plane hovering and ped is on ground");
				return true;
			}
		}
	}
	
	return false;
}

bool CPlayerInfo::ShouldDisableEntryForSyncedScene(const CVehicle* pVehicle)
{
	// if the vehicle is running the synchronized scene task we shouldn't be trying to get into it.
	if (pVehicle->GetIntelligence() && pVehicle->GetIntelligence()->GetTaskManager())
	{
		const CTaskSynchronizedScene *pTask = static_cast<const CTaskSynchronizedScene*>(pVehicle->GetIntelligence()->GetTaskManager()->FindTaskByTypeActive(VEHICLE_TASK_TREE_SECONDARY, CTaskTypes::TASK_SYNCHRONIZED_SCENE));
		if (pTask)
		{
			// unless we really should be trying to get into it.
			if (!pTask->IsSceneFlagSet(SYNCED_SCENE_VEHICLE_ALLOW_PLAYER_ENTRY))
			{
				AI_LOG_REJECTION_WITH_IDENTIFIER_AND_REASON("[VehicleEntryExit][CPlayerInfo::ShouldDisableEntryForSyncedScene]",pVehicle, "playing synced scene, entry disallowed");
				return true;
			}
		}
	}
	return false;
}

bool CPlayerInfo::ShouldDisableEntryBecauseUpsideDown(const CVehicle* pVehicle)
{
	// Make sure the vehicle is not upside down
	if (pVehicle->InheritsFromBike())
	{
		// We don't want to do all these checks for bike vehicles meant for missions
		// since we always want to allow to get in those
		if (!pVehicle->PopTypeIsMission())
		{
			// How much can we incline the front of the motorbike? 90 would mean that the bike
			// is in vertical position
			TUNE_GROUP_FLOAT( VEHICLE_ENTRY_TUNE, fBikeFrontDegreesAllowance, 55.0f, 0.0f, 90.0f, 0.1f );
			// How much can the bike be turned on the side? a value of 0.0f indicates that the bike
			// will only be allowed to go as much as flat. -90 would allow the bike completely upside down
			// (imagine the handles touching the ground and the wheels in the air)
			TUNE_GROUP_FLOAT( VEHICLE_ENTRY_TUNE, fBikeUpDegreesAllowance, -45.0f, -90.0f, 0.0f, 0.1f );
			const float fDotBetweenBikeFrontAndWorldUp = pVehicle->GetTransform().GetB().GetZf();
			const float fDotBetweenBikeUpAndWorldUp = pVehicle->GetTransform().GetC().GetZf();
			bool bVehicleIsUpsideDown = fDotBetweenBikeFrontAndWorldUp > cos( abs(HALF_PI - (fBikeFrontDegreesAllowance * DtoR)) ) ||
				fDotBetweenBikeUpAndWorldUp < cos( (fBikeUpDegreesAllowance - 90.0f) * DtoR );

			if (bVehicleIsUpsideDown)
			{
				AI_LOG_REJECTION_WITH_IDENTIFIER_AND_REASON("[VehicleEntryExit][CPlayerInfo::ShouldDisableEntryBecauseUpsideDown]",pVehicle, "bike upside down, entry disallowed");
				return true;
			}
		}
	}
	else
	{
		// For cars we only care about the up vector
		const float fDotBetweenVehicleUpAndWorldUp = pVehicle->GetTransform().GetC().GetZf();
		bool bVehicleIsUpsideDown = fDotBetweenVehicleUpAndWorldUp < 0.3f;
		if (bVehicleIsUpsideDown)
		{
			AI_LOG_REJECTION_WITH_IDENTIFIER_AND_REASON("[VehicleEntryExit][CPlayerInfo::ShouldDisableEntryBecauseUpsideDown]",pVehicle, "vehicle upside down, entry disallowed");
			return true;
		}
	}
	return false;
}

bool CPlayerInfo::ShouldDisableEntryDueToHeightCondition(bool& bPassedOriginalHeightCondition, bool bPedIsStandingOnThisVehicle, bool bCanBoardVehicleWhenStoodOnTop, const Vector3& vPedPos, const CPed* pPed, const CVehicle* pVehicle)
{
	// Make sure we're in a certain z-range (don't want cars on bridges etc)
	Vector3 vecBoundCentre = pVehicle->GetBoundCentre();
	vecBoundCentre = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
	vecBoundCentre.z = vecBoundCentre.z - pVehicle->GetHeightAboveRoad() + 1.0f;

	Vector3 vDiff = vPedPos - vecBoundCentre;

	float fHeightDiff=DotProduct(vDiff, Vector3(0,0,1.0f));

	TUNE_GROUP_FLOAT(VEHICLE_ENTRY_TUNE, MAX_HEIGHT_DIFF, 3.0f, 0.0f, 5.0f, 0.01f);
	bPassedOriginalHeightCondition = rage::Abs(fHeightDiff)<MAX_HEIGHT_DIFF || (bPedIsStandingOnThisVehicle && bCanBoardVehicleWhenStoodOnTop);

	// Increase the z tolerance for heli's as we may potentially enter the rear seats via a warp
	bool bPassedAlteredHeightCondition = false;
	if (CTaskVehicleFSM::CanPedWarpIntoHoveringVehicle(pPed, *pVehicle))
	{
		bPassedAlteredHeightCondition = rage::Abs(fHeightDiff)<CTaskVehicleFSM::ms_Tunables.m_MaxHoverHeightDistToWarpIntoHeli;
	}

	static bool bAllowBoatBoardingFromLand = true; // JB: enabled this for url:bugstar:203053
	const bool bPassedHeightCondition = ( (bPassedOriginalHeightCondition || bPassedAlteredHeightCondition) && !bCanBoardVehicleWhenStoodOnTop ) ||
		( bCanBoardVehicleWhenStoodOnTop && ( bPedIsStandingOnThisVehicle || pPed->GetIsInWater() || bAllowBoatBoardingFromLand ) );
	return !bPassedHeightCondition;
}

bool CPlayerInfo::ShouldDisableEntryBecauseVehicleMovingAway(Vector3 vPedVehDiff, const CPed* pPed, const CVehicle* pVehicle)
{
	static const float COS15 = 0.96593f;
	static const float VehicleVelLimit = rage::square(7.0f);	// Roughly max sprint speed
	Vector3 VehicleVel = pVehicle->GetRelativeVelocity(*pPed);
	if (VehicleVel.Mag2() > VehicleVelLimit)
	{
		VehicleVel.z = 0.f;
		VehicleVel.Normalize();
		vPedVehDiff.Normalize();
		if (vPedVehDiff.Dot(VehicleVel) < COS15)
			return true;
	}
	return false;
}

bool CPlayerInfo::IsVehicleStuckInWater(const CVehicle* pVehicle)
{
	bool isSeaPlane = pVehicle->pHandling && pVehicle->pHandling->GetSeaPlaneHandlingData();

	return pVehicle->m_nVehicleFlags.bIsDrowning
		|| (pVehicle->InheritsFromAutomobile() && !pVehicle->InheritsFromAmphibiousAutomobile() && !pVehicle->InheritsFromSubmarineCar() && !isSeaPlane && pVehicle->HasContactWheels()==false && pVehicle->GetIsInWater())
		|| (pVehicle->InheritsFromBike() && pVehicle->GetTransform().GetC().GetZf() > 0.707f && !pVehicle->HasContactWheels() && pVehicle->GetIsInWater());
}

bool CPlayerInfo::FindClosestRidableAnimalCB(CEntity* pEntity, void* data)
{
	Assert(pEntity);
	Assert(data);
	Assert(pEntity->GetIsTypePed());

	float	Distance;
	FindClosestRidableAnimalCBData* pCBData = static_cast<FindClosestRidableAnimalCBData*>(data);

	if (pEntity->GetIsTypePed())
	{
		// Make sure I am ridable (for now just using IsQuadruped and has a vehicle layout until we have species checks)
		CPed* pPed = static_cast<CPed*>(pEntity);
		if ((pPed->GetHealth() > 0.f) && pPed->GetCapsuleInfo()->IsQuadruped() && pPed->GetPedModelInfo()->GetLayoutInfo()) { 

			//Ignore mounted animals mounted by friendlies
			if (pPed->GetSeatManager()->GetDriver())
			{
				if (pCBData->pPed->GetPedIntelligence()->IsFriendlyWith(*pPed->GetSeatManager()->GetDriver()))
					return true;
			}

			//no ragdolling or recovering animals
			if (pPed->GetRagdollState() > RAGDOLL_STATE_ANIM || pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_NM_CONTROL) != NULL)
				return true;

			// Make sure we're in a certain z-range (don't want animals on bridges etc)
			Vector3 vecBoundCentre = pEntity->GetBoundCentre();
			const Vector3 vCBPedPos = VEC3V_TO_VECTOR3(pCBData->pPed->GetTransform().GetPosition());
			Vector3 vDiff = vCBPedPos - vecBoundCentre;

			float fHeightDiff=DotProduct(vDiff, Vector3(0,0,1.0f));

			if( pCBData->bDepthCheck || rage::Abs(fHeightDiff)<3.0f )
			{
				// Test the distance to this animal
				vDiff.z=0;
				Distance=vDiff.Mag();
				float fDistanceLimit = ms_Tunables.m_ScanNearbyMountsDistance;
				if(Distance <= fDistanceLimit)
				{
					// Now actually evaluate the position of this mount
					EvaluateAnimalPosition(pEntity, pCBData->pPed, Distance, &pCBData->Closeness, &pCBData->pAnimal, pCBData->vSearchDir, pCBData->bStickInput);
				}		
			}
		}
	}

	// NB : Return 'true' to continue enumerating vehicles from CGameWorld::ForAllEntitiesIntersecting()..
	return true;
}

// FindClosestTrainCB is only there so that we can quickly find out whether we are near a train.
// using FindClosestCarCB takes up to 20 msecs.
// This is only used by a script that prints 'Press 'Y' to enter train.
// I guess it should be removed at some point (Obbe, March 2008)
bool CPlayerInfo::FindClosestTrainCB(CEntity* pEntity, void* data)
{
	Assert(pEntity);
	Assert(data);
	Assert(pEntity->GetIsTypeVehicle());

	FindClosestCarCBData* pCBData = static_cast<FindClosestCarCBData*>(data);

	if (pEntity->IsCollisionEnabled() && pEntity->GetIsTypeVehicle() )
	{
		CVehicle* pVehicle = static_cast<CVehicle*>(pEntity);
		bool	bDisableCarEntry = false;

		// Stop the player entering trains from the wrong side
		if( pVehicle->GetVehicleType() == VEHICLE_TYPE_TRAIN )
		{
			CTrain* pTrain = static_cast<CTrain*>(pEntity);
			Vector3 vTrainToPlayer = VEC3V_TO_VECTOR3(pCBData->pPed->GetTransform().GetPosition() - pTrain->GetTransform().GetPosition());
			if( pTrain->m_nTrainFlags.iStationPlatformSides & CTrainTrack::Right )
			{
				if( DotProduct(vTrainToPlayer, VEC3V_TO_VECTOR3(pTrain->GetTransform().GetA())) < 0.0f )
				{
					bDisableCarEntry = true;
				}
			}
			else
			{
				if( DotProduct(vTrainToPlayer, VEC3V_TO_VECTOR3(pTrain->GetTransform().GetA())) > 0.0f )
				{
					bDisableCarEntry = true;
				}
			}
		}


		if (!bDisableCarEntry) // && pEntity->GetStatus() != STATUS_TRAIN_MOVING)	// Wrecked cars are of no interest to us
		{
			// Make sure this car isn't upside down
//			if(pEntity->GetC().z > 0.3f || (pVehicle->GetBaseVehicleType()==VEHICLE_TYPE_BIKE && pEntity->GetC().z > -0.5f) )
			{
				const Vector3 vEntityPos = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());

				// Make sure we're in a certain z-range (don't want cars on bridges etc)
				Vector3 vecBoundCentre = pEntity->GetBoundCentre();

				vecBoundCentre = vEntityPos;
				vecBoundCentre.z = vecBoundCentre.z - pVehicle->GetHeightAboveRoad() + 1.0f;

				const Vector3 vCBPedPos = VEC3V_TO_VECTOR3(pCBData->pPed->GetTransform().GetPosition());
				Vector3 vDiff;
				vDiff = vCBPedPos - vecBoundCentre;

				float fHeightDiff=DotProduct(vDiff, Vector3(0,0,1.0f));


				if(rage::Abs(fHeightDiff)<2.0f)
				{
					if (((CVehicle *)pEntity)->m_nVehicleFlags.bConsideredByPlayer)
					{
						CBaseModelInfo *pModelInfo = pEntity->GetBaseModelInfo();
						float dist = 0.0f;
						float distX = rage::Abs(DotProduct( (vCBPedPos - vEntityPos), VEC3V_TO_VECTOR3(pEntity->GetTransform().GetA())));
						dist += rage::Max(0.0f, distX - pModelInfo->GetBoundingBoxMax().x);
						float distY = rage::Abs(DotProduct( (vCBPedPos - vEntityPos), VEC3V_TO_VECTOR3(pEntity->GetTransform().GetB())));
						dist += rage::Max(0.0f, distY - pModelInfo->GetBoundingBoxMax().y);
						float distZ = rage::Abs(DotProduct( (vCBPedPos - vEntityPos), VEC3V_TO_VECTOR3(pEntity->GetTransform().GetC())));
						dist += rage::Max(0.0f, distZ - pModelInfo->GetBoundingBoxMax().z);

						if(dist <= SCANNEARBYVEHICLES)
						{
							// Now actually evaluate the position of this car
							Vector3 vSeatPos;
//							eHierarchyId iSeat=(eHierarchyId)0;

//							if(CCarEnterExit::GetNearestCarSeat(*(pCBData->pPed),*pVehicle,vSeatPos,iSeat))
							{
								vSeatPos = vEntityPos;
								EvaluateCarPosition(pEntity, pCBData->pPed, vSeatPos, dist, &pCBData->Closeness, &pCBData->pVehicle, pCBData->vSearchDir, pCBData->bStickInput);
							}
						}
					}
				}
			}
		}
	}

	// NB : Return 'true' to continue enumerating vehicles from CGameWorld::ForAllEntitiesIntersecting()..
	return true;
}


CVehicle *
CPlayerInfo::ScanForVehicleToEnter(CPed * pPlayerPed, const Vector3& vCarSearchDirection, const bool bUsingStickInput )
{
	// Scripts can now specify a car which overrides the vehicle selection.
	CPlayerInfo * pInfo = pPlayerPed->GetPlayerInfo();
	
	if(pInfo->GetPlayerResetFlags().IsFlagSet(CPlayerResetFlags::PRF_NOT_ALLOWED_TO_ENTER_ANY_CAR))
	{
		AI_FUNCTION_LOG("not allowed to enter any car");
		return NULL; 
	}
	
	if(pInfo->m_pOnlyEnterThisVehicle)
	{
		Vec3V vVehTestPos = pInfo->m_pOnlyEnterThisVehicle->GetVehiclePosition();

		Vector3 vAwayFromVehicle = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition() - vVehTestPos);
		float fDist = vAwayFromVehicle.Mag();

		if (fDist > CPlayerInfo::SCANNEARBYVEHICLES*1.5f)
		{
			AI_LOG_REJECTION_WITH_IDENTIFIER_AND_REASON("[VehicleEntryExit][CPlayerInfo::ScanForVehicleToEnter]",pInfo->m_pOnlyEnterThisVehicle, "ped can only enter this vehicle which is too far away");
			return NULL;
		} 
		else if(pInfo->m_pOnlyEnterThisVehicle->CanBeDriven())
		{
			AI_LOG_ACCEPTANCE_WITH_IDENTIFIER_AND_REASON("[VehicleEntryExit][CPlayerInfo::ScanForVehicleToEnter]", pInfo->m_pOnlyEnterThisVehicle, "ped can only enter this vehicle which can be driven");
			return pInfo->m_pOnlyEnterThisVehicle;
		}
		else 
		{
			AI_LOG_REJECTION_WITH_IDENTIFIER_AND_REASON("[VehicleEntryExit][CPlayerInfo::ScanForVehicleToEnter]",pInfo->m_pOnlyEnterThisVehicle, "ped can only enter this vehicle which can't be driven");
			return NULL;
		}
	}

	// If the player is already standing on a boat or plane, then get into this
	if (pPlayerPed->GetGroundPhysical() && pPlayerPed->GetGroundPhysical()->GetIsTypeVehicle())
	{
		CVehicle* pVehicleStoodOn = static_cast<CVehicle*>(pPlayerPed->GetGroundPhysical());
		if (CVehicleModelInfo::AllowsVehicleEntryWhenStoodOn(*pVehicleStoodOn) && pVehicleStoodOn->CanPedOpenLocks(pPlayerPed))
		{
			//! If vehicle is wrecked, don't consider this vehicle or anything else as we may struggle to get off it (e.g. when inside a boat).
			if(pVehicleStoodOn->GetStatus() == STATUS_WRECKED)
			{
				AI_LOG_REJECTION_WITH_IDENTIFIER_AND_REASON("[VehicleEntryExit][CPlayerInfo::ScanForVehicleToEnter]", pVehicleStoodOn, "stood on wrecked vehicle");
				return NULL;
			}

			if (pVehicleStoodOn->m_nVehicleFlags.bConsideredByPlayer)
			{
				TUNE_GROUP_FLOAT(TITAN_TUNE, MAX_DIST_FROM_OTHER_VEHICLE_TO_IGNORE_ENTRY, 3.0f, 0.0f, 10.0f, 0.01f);
				// Is there a vehicle within MAX_DIST_FROM_OTHER_VEHICLE_TO_IGNORE_ENTRY meters of the player, prefer to get on that instead
				bool bValidForEntry = true;
				if (pVehicleStoodOn->GetModelIndex() == MI_PLANE_TITAN.GetModelIndex())
				{
					const Vec3V vPlayerPos = pPlayerPed->GetTransform().GetPosition();
					CVehicleScanner& vehScanner = *pPlayerPed->GetPedIntelligence()->GetVehicleScanner();
					CEntityScannerIterator entityList = vehScanner.GetIterator();
					for (CEntity* pEnt = static_cast<CVehicle*>(entityList.GetFirst()); pEnt; pEnt = entityList.GetNext())
					{
						CVehicle* pNearbyVeh = static_cast<CVehicle*>(pEnt);
						const float fDistSqd = MagSquared(pNearbyVeh->GetTransform().GetPosition() - vPlayerPos).Getf();
						if (fDistSqd < square(MAX_DIST_FROM_OTHER_VEHICLE_TO_IGNORE_ENTRY))
						{
							bValidForEntry = false;
							break;
						}
					}
				}

				if (bValidForEntry)
				{
					AI_LOG_ACCEPTANCE_WITH_IDENTIFIER_AND_REASON("[VehicleEntryExit][CPlayerInfo::ScanForVehicleToEnter]", pVehicleStoodOn, "stood on vehicle");
					return pVehicleStoodOn;
				}
			}
		}
	}

	CVehicle	*pTargetVehicle = NULL;
	float		CarCloseness = 0.0f;

	spdAABB testBox;
	Vector3 corner1(VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition()));
	Vector3 corner2 = corner1;

	corner1.x += SCANNEARBYVEHICLES;
	corner1.y += SCANNEARBYVEHICLES;
	corner2.x -= SCANNEARBYVEHICLES;
	corner2.y -= SCANNEARBYVEHICLES;

	testBox.Invalidate();
	testBox.GrowPoint(RCC_VEC3V(corner1));
	testBox.GrowPoint(RCC_VEC3V(corner2));

	fwIsBoxIntersectingApprox searchBox(testBox);

	FindClosestCarCBData callBackData = {pPlayerPed, pTargetVehicle, CarCloseness, vCarSearchDirection, bUsingStickInput};
	CGameWorld::ForAllEntitiesIntersecting(&searchBox, FindClosestCarCB, static_cast<void*>(&callBackData), 
		ENTITY_TYPE_MASK_VEHICLE, (SEARCH_LOCATION_EXTERIORS|SEARCH_LOCATION_INTERIORS),
		SEARCH_LODTYPE_HIGHDETAIL, SEARCH_OPTION_NONE, WORLDREP_SEARCHMODULE_PEDS);
	/*false, true, false, false, false, false, false, true);*/		// intersecting cars only

	pTargetVehicle = callBackData.pVehicle;
	CarCloseness = callBackData.Closeness;


	if (pTargetVehicle)
	{
		if(!pTargetVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_CANNOT_BE_DRIVEN_BY_PLAYER) && pTargetVehicle->CanBeDriven())
		{
			// DMKH. Do not allow player to get onto a bike that is overturned whilst handcuffed. -> This should probably be rejected in FindClosestCarCB?
			if (pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsHandCuffed) && pTargetVehicle->InheritsFromBike() )
			{
				CBike *pBike = static_cast<CBike*>(pTargetVehicle);
				if(!pBike->m_nBikeFlags.bOnSideStand)
				{
					AI_LOG_REJECTION_WITH_IDENTIFIER_AND_REASON("[VehicleEntryExit][CPlayerInfo::ScanForVehicleToEnter]", pBike, "bike overturned and player is handcuffed");
					return NULL;
				}
			}

			CVehicleModelInfo* pModelInfo = static_cast<CVehicleModelInfo*>(pTargetVehicle->GetBaseModelInfo());
			const bool bIgnoreOnSideTest = pModelInfo && pModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_IGNORE_ON_SIDE_CHECK); 
			if (pTargetVehicle->IsInEnterableState(bIgnoreOnSideTest)) // -> This should probably be rejected in FindClosestCarCB?
			{
				AI_LOG_ACCEPTANCE_WITH_IDENTIFIER_AND_REASON("[VehicleEntryExit][CPlayerInfo::ScanForVehicleToEnter]", pTargetVehicle, "vehicle is enterable");
				return pTargetVehicle;
			}
		}
	}

	return NULL;
}

CPed *
CPlayerInfo::ScanForAnimalToRide(CPed * pPlayerPed, const Vector3& vAnimalSearchDirection, const bool bUsingStickInput, const bool bDoDepthTest )
{
	// Scripts can now specify an animal which overrides the selection.
	CPlayerInfo * pInfo = pPlayerPed->GetPlayerInfo();

	if(pInfo->GetPlayerResetFlags().IsFlagSet(CPlayerResetFlags::PRF_NOT_ALLOWED_TO_ENTER_ANY_CAR)) //should apply for animals too?
	{
		return NULL; 
	}	

	CPed	*pTargetAnimal = NULL;
	float	AnimalCloseness = 0.0f;

	spdAABB testBox;
	Vector3 corner1(VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition()));
	Vector3 corner2 = corner1;
	static dev_float sf_MountAnimDepthTestDistance = 5.0f;
	if (bDoDepthTest)
		corner2.z -= sf_MountAnimDepthTestDistance;

	corner1.x += ms_Tunables.m_ScanNearbyMountsDistance;
	corner1.y += ms_Tunables.m_ScanNearbyMountsDistance;
	corner2.x -= ms_Tunables.m_ScanNearbyMountsDistance;
	corner2.y -= ms_Tunables.m_ScanNearbyMountsDistance;

	testBox.Invalidate();
	testBox.GrowPoint(RCC_VEC3V(corner1));
	testBox.GrowPoint(RCC_VEC3V(corner2));

	fwIsBoxIntersectingApprox searchBox(testBox);

	FindClosestRidableAnimalCBData callBackData = {pPlayerPed, pTargetAnimal, AnimalCloseness, vAnimalSearchDirection, bUsingStickInput, bDoDepthTest};
	CGameWorld::ForAllEntitiesIntersecting(&searchBox, FindClosestRidableAnimalCB, static_cast<void*>(&callBackData), 
		ENTITY_TYPE_MASK_PED, (SEARCH_LOCATION_EXTERIORS|SEARCH_LOCATION_INTERIORS),
		SEARCH_LODTYPE_HIGHDETAIL, SEARCH_OPTION_NONE, WORLDREP_SEARCHMODULE_PEDS);	

	pTargetAnimal = callBackData.pAnimal;
	AnimalCloseness = callBackData.Closeness;
	return pTargetAnimal;
}

CVehicle *
CPlayerInfo::ScanForTrainToEnter(CPed * pPlayerPed, const Vector3& vCarSearchDirection, const bool bUsingStickInput )
{

	CVehicle	*pTargetVehicle = NULL;
	float		CarCloseness = 0.0f;

	CVehicle::Pool *VehiclePool = CVehicle::GetPool();
	s32 i = (s32) VehiclePool->GetSize();
	FindClosestCarCBData callBackData = {pPlayerPed, pTargetVehicle, CarCloseness, vCarSearchDirection, bUsingStickInput};
	while(i--)
	{
		CVehicle *pVeh = VehiclePool->GetSlot(i);
		if (pVeh && pVeh->InheritsFromTrain())
		{
			FindClosestTrainCB(pVeh, &callBackData);
		}
	}
	pTargetVehicle = callBackData.pVehicle;
	CarCloseness = callBackData.Closeness;

	if (pTargetVehicle && pTargetVehicle->CanBeDriven())
	{
		return pTargetVehicle;
	}

	return NULL;
}


/////////////////////////////////////////////////////////////////////////////////
//
// FUNCTION : IsPlayerInRemoteMode
// PURPOSE :  Returns true or false depending on whether teh player is controlling
//			  a remotely controlled car
//
/////////////////////////////////////////////////////////////////////////////////

bool CPlayerInfo::IsPlayerInRemoteMode(void)
{
//	if (GetRemoteVehicle()) return true;
//	if (bAfterRemoteVehicleExplosion) return true;
	return (false);
}

////////////////////////////////////////////////////////////////////////////////

void CPlayerInfo::ProcessPostMovement()
{
	// Update the vehicle clip streaming request helper
	// this searches nearby vehicles and tries to stream the clips
	m_VehicleClipRequestHelper.SetPed(m_pPlayerPed);
	m_VehicleClipRequestHelper.Update(fwTimer::GetTimeStep());

	const CWeaponInfo* pEquippedWeaponInfo = m_pPlayerPed->GetWeaponManager() ? m_pPlayerPed->GetWeaponManager()->GetEquippedWeaponInfo() : NULL;
	if( pEquippedWeaponInfo )
	{
		bool bChangingWeapons = false;

		// Switch the cached weapon hash and force clip set functionality
		if( pEquippedWeaponInfo->GetHash() != m_nMeleeEquippedWeaponHash )
		{
			m_nMeleeEquippedWeaponHash = pEquippedWeaponInfo->GetHash();
			bChangingWeapons = true;
		}

		bool bRunningMelee = m_pPlayerPed->GetPedIntelligence() ? m_pPlayerPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning( CTaskTypes::TASK_MELEE ) : false;
		if( bRunningMelee )
		{
			// Turn off the melee clip set release timer
			m_bEnableMeleeClipSetReleaseTimer = false;
		
			// Melee variation streaming request		
			if( bChangingWeapons || !m_meleeVariationClipSetHelper.IsLoaded() )
			{
				m_meleeVariationClipSetHelper.Request( pEquippedWeaponInfo->GetMeleeVariationClipSetId( *m_pPlayerPed ) );
			}			

			// Melee taunt streaming request
			if( bChangingWeapons || !m_meleeTauntClipSetHelper.IsLoaded() )
			{
				m_meleeTauntClipSetHelper.Request( pEquippedWeaponInfo->GetMeleeTauntClipSetId( *m_pPlayerPed ) );
			}
		}
		else
		{
			// Check to see if we should reset the clip set release timer?
			if( !m_bEnableMeleeClipSetReleaseTimer )
			{
				m_bEnableMeleeClipSetReleaseTimer = true;
				m_nMeleeClipSetReleaseTimer = fwTimer::GetTimeInMilliseconds() + ms_nMeleeClipSetReleaseDuration;
			}

			// Release the variation clip set if necessary
			if( m_meleeVariationClipSetHelper.IsLoaded() && ( bChangingWeapons || m_nMeleeClipSetReleaseTimer < fwTimer::GetTimeInMilliseconds() ) )
			{
				m_meleeVariationClipSetHelper.Release();
			}

			// Release the taunt clip set if necessary
			if( m_meleeTauntClipSetHelper.IsLoaded() && ( bChangingWeapons || m_nMeleeClipSetReleaseTimer < fwTimer::GetTimeInMilliseconds() ) )
			{
				m_meleeTauntClipSetHelper.Release();
			}
		}
	}
	// Do not hesitate to release the melee clip sets
	else
	{
		m_meleeVariationClipSetHelper.Release();
		m_meleeTauntClipSetHelper.Release();
	}
}

void CPlayerInfo::ProcessTestForShelter()
{
	static const int iLineTestFreqMs = 10000;
	static const float fDistAbovePlayer = 30.0f;

	// Process retrieving the async probe result
	if(m_hShelterLineTestHandle.GetWaitingOnResults())
	{
		return;
	}

	if(m_hShelterLineTestHandle.GetResultsReady())
	{
		m_bIsShelteredAbove = m_hShelterLineTestHandle.GetNumHits() > 0u;
		m_hShelterLineTestHandle.Reset();
		return;
	}

	// If we're in an interior then we can probably assume we're in shelter (can't we??)
	if(m_pPlayerPed->m_nFlags.bInMloRoom)
	{
		m_iTimeTillNextShelterLineTest = iLineTestFreqMs;
		m_bIsShelteredAbove = true;
		return;
	}
	// If its not raining then don't bother with any of these linetests
	if(!g_weather.IsRaining())
		return;

	m_iTimeTillNextShelterLineTest -= fwTimer::GetTimeStepInMilliseconds();
	if(m_iTimeTillNextShelterLineTest <= 0)
	{
		m_iTimeTillNextShelterLineTest = iLineTestFreqMs;
		Assert(m_hShelterLineTestHandle.GetResultsStatus() == WorldProbe::TEST_NOT_SUBMITTED);

		const Vector3 vPlayerPos = VEC3V_TO_VECTOR3(m_pPlayerPed->GetTransform().GetPosition());
		WorldProbe::CShapeTestProbeDesc probeData;
		probeData.SetResultsStructure(&m_hShelterLineTestHandle);
		probeData.SetStartAndEnd(vPlayerPos,vPlayerPos + Vector3(0.0f,0.0f,fDistAbovePlayer));
		probeData.SetContext(WorldProbe::ENotSpecified);
		probeData.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
		probeData.SetIsDirected(true);
		WorldProbe::GetShapeTestManager()->SubmitTest(probeData, WorldProbe::PERFORM_ASYNCHRONOUS_TEST);
	}
}

void CPlayerInfo::ProcessWorldExtentsCheck()
{
	if (HasPlayerLeftTheWorld())
	{
		HandlePlayerWorldBreach();
	}
	else
	{
		// Reset the escape timer as the player is back inside the gameplay zone.
		m_fSwimBoundsEscapeTimer = ms_Tunables.m_MaxTimeToTrespassWhileSwimmingBeforeDeath;
	}
}

void CPlayerInfo::ProcessNumEnemiesInCombat()
{
	m_iNumEnemiesInCombat = 0;
	m_iNumEnemiesShootingInCombat = 0;

	CCombatTaskManager* pCombatTaskManager = CCombatManager::GetCombatTaskManager();
	if( pCombatTaskManager && m_pPlayerPed )
	{
		m_iNumEnemiesInCombat = pCombatTaskManager->CountPedsInCombatWithTarget(*m_pPlayerPed, BANK_ONLY(ms_bDisplayNumEnemiesInCombat ? CCombatTaskManager::OF_DebugRender :) 0);
		fwFlags8 combatManagerFlags(CCombatTaskManager::OF_MustHaveClearLOS|CCombatTaskManager::OF_MustHaveGunWeaponEquipped);

#if __BANK
		if(ms_bDisplayNumEnemiesShootingInCombat)
		{
			combatManagerFlags.SetFlag(CCombatTaskManager::OF_DebugRender);
		}
#endif

		m_iNumEnemiesShootingInCombat = pCombatTaskManager->CountPedsInCombatWithTarget(*m_pPlayerPed, combatManagerFlags);
	}
#if __BANK
	bool bDisplayNumEnemiesInCombat = (ms_bDisplayNumEnemiesInCombat && m_iNumEnemiesInCombat > 0);
	bool bDisplayNumEnemiesShootingInCombat = (ms_bDisplayNumEnemiesShootingInCombat && m_iNumEnemiesShootingInCombat > 0);
	if( bDisplayNumEnemiesInCombat || bDisplayNumEnemiesShootingInCombat )
	{
		int lineNum = 0;
		const int DEBUG_STRING_SIZE = 128;
		char debugString[DEBUG_STRING_SIZE];
		const Vector3 vDebugPos = VEC3V_TO_VECTOR3(m_pPlayerPed->GetTransform().GetPosition());
		if( bDisplayNumEnemiesInCombat )
		{
			formatf(debugString, DEBUG_STRING_SIZE, "NumEnemiesInCombat: %d", m_iNumEnemiesInCombat);
			grcDebugDraw::Text(vDebugPos, Color_red, 0, lineNum * grcDebugDraw::GetScreenSpaceTextHeight(), debugString );
			lineNum++;
		}
		if( bDisplayNumEnemiesShootingInCombat )
		{
			formatf(debugString, DEBUG_STRING_SIZE, "NumEnemiesShootingInCombat: %d", m_iNumEnemiesShootingInCombat);
			grcDebugDraw::Text(vDebugPos, Color_red, 0, lineNum * grcDebugDraw::GetScreenSpaceTextHeight(), debugString );
			lineNum++;
		}
	}
#endif // __BANK

}

void CPlayerInfo::ProcessCoverStateTracking()
{
	if( m_pPlayerPed )
	{
		// Get the player's running cover task, if any
		CTaskInfo* pCoverTaskInfo = m_pPlayerPed->GetPedIntelligence()->GetQueriableInterface()->FindTaskInfoIfCurrentlyRunning(CTaskTypes::TASK_IN_COVER);
		if( pCoverTaskInfo )
		{
			// in cover, so increment the using cover counter
			m_fTimeElapsedUsingCoverSeconds += fwTimer::GetTimeStep();

			// Check if the current state is any that qualifies as NOT hiding (active in combat)
			s32 currentCoverState = pCoverTaskInfo->GetState();
			switch(currentCoverState)
			{
			case CTaskInCover::State_Aim:
			case CTaskInCover::State_AimIntro:
			case CTaskInCover::State_AimOutro:
			case CTaskInCover::State_BlindFiring:
			case CTaskInCover::State_ThrowingProjectile:
				// active in cover, reset hiding time
				m_fTimeElapsedHidingInCoverSeconds = 0.0f;
				break;
			default:
				// otherwise the player is considered as hiding in cover
				m_fTimeElapsedHidingInCoverSeconds += fwTimer::GetTimeStep();
				break;
			}
		}
		else
		{
			// not in cover, so reset the elapsed times
			m_fTimeElapsedUsingCoverSeconds = 0.0f;
			m_fTimeElapsedHidingInCoverSeconds = 0.0f;
		}

#if __DEV
		bool bDebugCoverTracking = ( ms_bDisplayCoverTracking && m_fTimeElapsedUsingCoverSeconds > 0.0f );
		if( bDebugCoverTracking )
		{
			int lineNum = 0;
			const int DEBUG_STRING_SIZE = 256;
			char debugString[DEBUG_STRING_SIZE];
			const Vector3 vDebugPos = VEC3V_TO_VECTOR3(m_pPlayerPed->GetTransform().GetPosition());
			formatf(debugString, DEBUG_STRING_SIZE, "UsingCover: %.1f s", m_fTimeElapsedUsingCoverSeconds);
			grcDebugDraw::Text(vDebugPos, Color_green, 0, lineNum * grcDebugDraw::GetScreenSpaceTextHeight(), debugString );
			lineNum++;
			formatf(debugString, DEBUG_STRING_SIZE, "HidingInCover: %.1f s", m_fTimeElapsedHidingInCoverSeconds);
			grcDebugDraw::Text(vDebugPos, m_fTimeElapsedHidingInCoverSeconds > 0.0f ? Color_green : Color_grey, 0, lineNum * grcDebugDraw::GetScreenSpaceTextHeight(), debugString );
			lineNum++;
		}
#endif // __DEV

	}
}

void CPlayerInfo::ProcessCombatLoitering()
{
	if( m_pPlayerPed )
	{
		// If player has enemies in combat
		if( BANK_ONLY(ms_bDebugCombatLoitering ||) m_iNumEnemiesInCombat > 0 )
		{
			bool bResetLoitering = false;

			// if loitering has not been set
			if( m_fTimeElapsedLoiteringSeconds < 0.0f )
			{
				bResetLoitering = true;
			}
			else
			{
				const u32 currentTimeMS = fwTimer::GetTimeInMilliseconds();

				// if enough time has elapsed to check distance
				if( currentTimeMS >= m_uNextLoiterDistCheckTimeMS )
				{
					// set next time to check distance
					m_uNextLoiterDistCheckTimeMS = currentTimeMS + ms_Tunables.m_CombatLoitering.m_uDistanceCheckPeriodMS;

					// compute distance between player and stored loiter position
					const ScalarV movementDeltaSq = DistSquared(m_pPlayerPed->GetTransform().GetPosition(), m_vCombatLoiteringPosition);
					const ScalarV movementDeltaSq_MAX = LoadScalar32IntoScalarV(rage::square(ms_Tunables.m_CombatLoitering.m_fPlayerMoveDistToResetLoiterPosition));
					if( IsGreaterThanAll(movementDeltaSq, movementDeltaSq_MAX) )
					{
						bResetLoitering = true;
					}
				}
			}

			if( bResetLoitering)
			{
				// update loitering position
				m_vCombatLoiteringPosition = m_pPlayerPed->GetTransform().GetPosition();

				// reset loitering time to none
				m_fTimeElapsedLoiteringSeconds = 0.0f;
			}
			else
			{
				// increment the time loitering
				m_fTimeElapsedLoiteringSeconds += fwTimer::GetTimeStep();
			}

#if __DEV
			bool bDebugCombatLoitering = (ms_bDisplayCombatLoitering && m_fTimeElapsedLoiteringSeconds > 0.0f);
			if( bDebugCombatLoitering )
			{
				int lineNum = 0;
				const int DEBUG_STRING_SIZE = 256;
				char debugString[DEBUG_STRING_SIZE];
				const Vector3 vDebugPos = VEC3V_TO_VECTOR3(m_pPlayerPed->GetTransform().GetPosition());
				formatf(debugString, DEBUG_STRING_SIZE, "Loitering: %.1f s", m_fTimeElapsedLoiteringSeconds);
				grcDebugDraw::Text(vDebugPos, Color_grey, 0, lineNum * grcDebugDraw::GetScreenSpaceTextHeight(), debugString );
				
				// indicate tracked loitering position
				const float fSphereRadius = 0.3f;
				grcDebugDraw::Sphere(m_vCombatLoiteringPosition, fSphereRadius, Color_grey);
				
				// indicate loitering radius
				const bool bSolid = false;
				grcDebugDraw::Sphere(m_vCombatLoiteringPosition, ms_Tunables.m_CombatLoitering.m_fPlayerMoveDistToResetLoiterPosition, Color_grey, bSolid);
			}
#endif // __DEV
			
			// exit this method
			return;
		}
	}

	// by default reset loitering time to none
	m_fTimeElapsedLoiteringSeconds = -1.0f;
}

void CPlayerInfo::ProcessEnemyElections()
{
	// Check if there is an open registration or acceptance period
	const bool bOpenRegistration = (m_CandidateRegistrationEndTimeMS > 0);
	const bool bOpenAcceptance = (m_BestCandidateAcceptanceEndTimeMS > 0);
	if( bOpenRegistration || bOpenAcceptance )
	{
		// Get current time
		const u32 currentTimeMS = fwTimer::GetTimeInMilliseconds();

		// Check if the registration period has expired
		if( bOpenRegistration && currentTimeMS > m_CandidateRegistrationEndTimeMS )
		{
			Assert(bOpenAcceptance == false);

			// Clear the registration period
			m_CandidateRegistrationEndTimeMS = 0;

			// Start the best candidate acceptance period
			const u32 uCandidateAcceptancePeriodMS = 2000;
			m_BestCandidateAcceptanceEndTimeMS = currentTimeMS + uCandidateAcceptancePeriodMS;

			aiDebugf3("ELECTION: CPlayerInfo::ProcessEnemyElections registration ended, acceptance scheduled [%d]", m_BestCandidateAcceptanceEndTimeMS);
		}
		// Check if the acceptance period has expired
		// NOTE: We don't expect this to happen often, it's a safety in case the winning ped is
		// killed off or otherwise vanished before they can report starting
		else if( bOpenAcceptance && currentTimeMS > m_BestCandidateAcceptanceEndTimeMS )
		{
			Assert(bOpenRegistration == false);

			// Clear the acceptance period
			m_BestCandidateAcceptanceEndTimeMS = 0;

			// Clear the winning ped, they didn't report start in time
			m_BestEnemyChargerCandidatePed = NULL;
			m_BestEnemySmokeThrowerCandidatePed = NULL;

			aiDebugf3("ELECTION: CPlayerInfo::ProcessEnemyElections acceptance period timed out, clearing!");
		}
	}
}

void CPlayerInfo::ProcessChargingEnemyGoalPositions()
{
	// By default consider resetting charge goal position variables
	bool bResetChargeGoalPositions = true;

	// Check if the player has hostiles targeting him
	if( BANK_ONLY(ms_bDebugCandidateChargeGoalPositions ||) m_iNumEnemiesInCombat > 0 )
	{
		// Check if the player is using cover according to the member time
		if( m_fTimeElapsedUsingCoverSeconds > 0.0f )
		{
			// Player is in combat and using cover, do not reset the variables
			bResetChargeGoalPositions = false;

			// Check if the player charge position has not been set
			if( IsZeroAll(m_vChargeGoalPlayerPosition) )
			{
				// Calculate and adjust charge goal candidate positions according to player cover
				CCoverPoint* pPlayerCoverPoint = NULL;
				if( m_pPlayerPed ) 
				{ 
					pPlayerCoverPoint = m_pPlayerPed->GetCoverPoint(); 
				}
				if( pPlayerCoverPoint )
				{
					// Calculate candidate charge goal positions
					Vec3V vChargeGoalPosBase;

					// First adjust the goal along the cover direction axis, away from the cover
					const Vec3V vCoverDirection = pPlayerCoverPoint->GetCoverDirectionVector();
					Vector3 vCoverPosition;
					if( pPlayerCoverPoint->GetCoverPointPosition(vCoverPosition) )
					{
						vChargeGoalPosBase = Vec3V(VECTOR3_TO_VEC3V(vCoverPosition)) + ( -ScalarV(ms_Tunables.m_EnemyCharging.m_fChargeGoalBehindCoverCentralOffset)  * vCoverDirection );
					}
					else // we don't have a valid cover position, bail out
					{
						return;
					}

					// Set the position of the player for charging
					// NOTE: This also serves as indicator that this process is active
					m_vChargeGoalPlayerPosition = m_pPlayerPed->GetTransform().GetPosition();

					// Adjust a bit further back from this for the rear position
					m_CandidateChargeGoals[CGP_Rear].m_Position = vChargeGoalPosBase - (ScalarV(ms_Tunables.m_EnemyCharging.m_fChargeGoalRearOffset) * vCoverDirection);

					// Adjust using lateral offsets to either side
					const ScalarV radiansOrthogonalRight = ScalarV(-90.0f * DtoR);
					Vec3V vCoverRightDirection = RotateAboutZAxis(vCoverDirection, radiansOrthogonalRight);
					m_CandidateChargeGoals[CGP_Left].m_Position =  vChargeGoalPosBase - (ScalarV(ms_Tunables.m_EnemyCharging.m_fChargeGoalLateralOffset) * vCoverRightDirection);
					m_CandidateChargeGoals[CGP_Right].m_Position = vChargeGoalPosBase + (ScalarV(ms_Tunables.m_EnemyCharging.m_fChargeGoalLateralOffset) * vCoverRightDirection);
#if __BANK
					// For debugging stash the original offset positions
					m_CandidateChargeGoals[CGP_Left].m_InitialPosition = m_CandidateChargeGoals[CGP_Left].m_Position;
					m_CandidateChargeGoals[CGP_Right].m_InitialPosition = m_CandidateChargeGoals[CGP_Right].m_Position;
					m_CandidateChargeGoals[CGP_Rear].m_InitialPosition = m_CandidateChargeGoals[CGP_Rear].m_Position;
#endif // __BANK
					// Mark all of the goals as pending navmesh check
					for(int i=0; i < CGP_MAX_NUM; i++)
					{
						m_CandidateChargeGoals[i].m_Status = CGPS_PendingNavmeshCheck;
					}
				}
			}
			else // the player charge position has been set, process is active
			{
				// Check to see if the player has moved enough to reset the charge goal position process
				const ScalarV DistPlayerActualToStoredPositionSq_MAX = ScalarVFromF32(rage::square(ms_Tunables.m_EnemyCharging.m_fPlayerMoveDistToResetChargeGoals));
				const ScalarV DistPlayerActualToStoredPositionSq = DistSquared(m_pPlayerPed->GetTransform().GetPosition(), m_vChargeGoalPlayerPosition);
				if( IsGreaterThanAll(DistPlayerActualToStoredPositionSq, DistPlayerActualToStoredPositionSq_MAX) )
				{
					// make sure we reset below
					bResetChargeGoalPositions = true;
				}
				else // ongoing process of charge goal positions for current player position
				{
					// Traverse the candidates
					for(int i=0; i < CGP_MAX_NUM; i++)
					{
						// If the candidate is waiting on navmesh check and adjustment
						if( m_CandidateChargeGoals[i].m_Status == CGPS_PendingNavmeshCheck )
						{
							// If the search is already active
							if( m_CandidateChargeGoalNavmeshHelpers[i].IsSearchActive() )
							{
								// Check for search results
								SearchStatus searchResult;
								int numPositionsFound;
								const int maxPositionsFound = 1;
								Vector3 foundPositions[maxPositionsFound];
								if( m_CandidateChargeGoalNavmeshHelpers[i].GetSearchResults(searchResult, numPositionsFound, foundPositions, maxPositionsFound) )
								{
									if( SS_SearchFailed == searchResult || (SS_SearchSuccessful == searchResult && numPositionsFound <= 0))
									{
										m_CandidateChargeGoals[i].m_Status = CGPS_Invalid;
									}
									else // search successful and position found
									{
										aiAssert( SS_SearchSuccessful == searchResult );// if still searching, GetSearchResults should return false

										// Use the closest found position as the new goal position
										m_CandidateChargeGoals[i].m_Position = VECTOR3_TO_VEC3V(foundPositions[0]);

										// Mark the candidate for line of sight check
										m_CandidateChargeGoals[i].m_Status = CGPS_PendingProbeCheck;
									}
								}
							}
							else // need to kick off the search
							{
								// Try to kick off the process of adjusting on navmesh
								const float fMaxAdjustRadius = ms_Tunables.m_EnemyCharging.m_fChargeGoalMaxAdjustRadius;
								u32 iFlags = CNavmeshClosestPositionHelper::Flag_ConsiderDynamicObjects|CNavmeshClosestPositionHelper::Flag_ConsiderInterior|CNavmeshClosestPositionHelper::Flag_ConsiderExterior;
								// NOTE: the request to start search may fail, so keep trying if necessary
								m_CandidateChargeGoalNavmeshHelpers[i].StartClosestPositionSearch(VEC3V_TO_VECTOR3(m_CandidateChargeGoals[i].m_Position), fMaxAdjustRadius, iFlags);
							}
						}
						// If the candidate is waiting on line of sight check
						else if( m_CandidateChargeGoals[i].m_Status == CGPS_PendingProbeCheck )
						{
							// If the probe is already active
							if( m_CandidateChargeGoalProbeHelpers[i].IsProbeActive() )
							{
								// Check to see if pending results are now ready
								ProbeStatus probeResultStatus;
								if( m_CandidateChargeGoalProbeHelpers[i].GetProbeResults(probeResultStatus) )
								{
									// Mark the status as valid or invalid according to results
									if( probeResultStatus == PS_Clear )
									{
										m_CandidateChargeGoals[i].m_Status = CGPS_Valid;
									}
									else
									{
										m_CandidateChargeGoals[i].m_Status = CGPS_Invalid;
									}
								}
							}
							else // need to start the probe
							{
								// Kick off the process of validating LOS from candidate charge goal position to player position
								WorldProbe::CShapeTestProbeDesc probeDescriptor;
								probeDescriptor.SetStartAndEnd(VEC3V_TO_VECTOR3(m_CandidateChargeGoals[i].m_Position), VEC3V_TO_VECTOR3(m_vChargeGoalPlayerPosition));
								probeDescriptor.SetContext(WorldProbe::ELosCombatAI);
								probeDescriptor.SetExcludeEntity(m_pPlayerPed);
								probeDescriptor.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_OBJECT_TYPE|ArchetypeFlags::GTA_VEHICLE_TYPE);
								probeDescriptor.SetOptions(WorldProbe::LOS_IGNORE_SEE_THRU);
								// NOTE: the request to start the probe may be denied, keep trying if necessary
								m_CandidateChargeGoalProbeHelpers[i].StartTestLineOfSight(probeDescriptor);
							}
						}
					}// END for candidate points list
				}// END if processing candidate points
			}// END if process active
		}// END if using cover
	}// END if in combat

	// If we should reset the charge goal positions, do so
	if( bResetChargeGoalPositions && !IsZeroAll(m_vChargeGoalPlayerPosition) )
	{
		ResetChargingEnemyGoalPositions();
	}

#if __BANK
	if( ms_bDebugDrawCandidateChargePositions && !IsZeroAll(m_vChargeGoalPlayerPosition) )
	{
		// Draw blue sphere at player position
		Color32 debugColor = Color_blue;
		float fDebugRadiusA = 0.5f;
		float fDebugRadiusB = 0.6f;
		bool bSolid = false;
		grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(m_vChargeGoalPlayerPosition), fDebugRadiusB, debugColor, bSolid);

		for(int i=0; i < CGP_MAX_NUM; i++)
		{
			debugColor = Color_white; // initial position
			grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(m_CandidateChargeGoals[i].m_InitialPosition), fDebugRadiusA, debugColor, bSolid);
			grcDebugDraw::Line(VEC3V_TO_VECTOR3(m_CandidateChargeGoals[i].m_InitialPosition), VEC3V_TO_VECTOR3(m_CandidateChargeGoals[i].m_Position), debugColor, debugColor);

			debugColor = Color_yellow;// CGPS_Unknown
			if( m_CandidateChargeGoals[i].m_Status == CGPS_Valid) { debugColor = Color_green; }
			else if( m_CandidateChargeGoals[i].m_Status == CGPS_Invalid) { debugColor = Color_red; }
			else if( m_CandidateChargeGoals[i].m_Status == CGPS_PendingNavmeshCheck) { debugColor = Color_blue;}
			else if( m_CandidateChargeGoals[i].m_Status == CGPS_PendingProbeCheck) { debugColor = Color_orange;}
			grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(m_CandidateChargeGoals[i].m_Position), fDebugRadiusB, debugColor, bSolid);
		}
	}
#endif // __BANK
}

bool CPlayerInfo::HasValidatedChargeGoalPosition() const
{
	for(int i=0; i < CGP_MAX_NUM; i++)
	{
		if( m_CandidateChargeGoals[i].m_Status == CGPS_Valid )
		{
			return true;
		}
	}
	return false;
}

bool CPlayerInfo::GetValidatedChargeGoalPosition(Vec3V_In inQueryPosition, Vec3V_InOut outChargeGoalPosition) const
{
	// First check if any positions are validated
	if( !HasValidatedChargeGoalPosition() )
	{
		// none valid
		return false;
	}
	
	// Check player's cover to see if left or right would be best
	if( m_pPlayerPed && (m_CandidateChargeGoals[CGP_Left].m_Status == CGPS_Valid || m_CandidateChargeGoals[CGP_Right].m_Status == CGPS_Valid))
	{
		CCoverPoint* pPlayerCoverPoint = m_pPlayerPed->GetCoverPoint();
		if( pPlayerCoverPoint )
		{
			const CCoverPoint::eCoverUsage ePlayerCoverUsage = pPlayerCoverPoint->GetUsage();
			if( ePlayerCoverUsage == CCoverPoint::COVUSE_WALLTOLEFT )
			{
				if( m_CandidateChargeGoals[CGP_Left].m_Status == CGPS_Valid )
				{
					outChargeGoalPosition = m_CandidateChargeGoals[CGP_Left].m_Position;
					return true;
				}
			}
			else if( ePlayerCoverUsage == CCoverPoint::COVUSE_WALLTORIGHT )
			{
				if( m_CandidateChargeGoals[CGP_Right].m_Status == CGPS_Valid )
				{
					outChargeGoalPosition = m_CandidateChargeGoals[CGP_Right].m_Position;
					return true;
				}
			}
		}
	}

	// Use the closest valid position to query position
	ScalarV closestDistSq = LoadScalar32IntoScalarV(FLT_MAX);
	int iClosestIndex = -1;
	for(int i=0; i < CGP_MAX_NUM; i++)
	{
		if( m_CandidateChargeGoals[i].m_Status == CGPS_Valid )
		{
			ScalarV distSq = DistSquared(m_CandidateChargeGoals[i].m_Position, inQueryPosition);
			if( IsLessThanAll(distSq, closestDistSq) )
			{
				iClosestIndex = i;
				closestDistSq = distSq;
			}
		}
	}
	if( iClosestIndex >= 0 )
	{
		outChargeGoalPosition = m_CandidateChargeGoals[iClosestIndex].m_Position;
		return true;
	}

	// no valid charge goal by default
	return false;
}

void CPlayerInfo::ResetChargingEnemyGoalPositions()
{
	m_vChargeGoalPlayerPosition.ZeroComponents();
	for(int i=0; i < CGP_MAX_NUM; i++)
	{
		m_CandidateChargeGoals[i].Reset();
		m_CandidateChargeGoalNavmeshHelpers[i].ResetSearch();
		m_CandidateChargeGoalProbeHelpers[i].ResetProbe();
	}
}

bool CPlayerInfo::HasPlayerLeftTheWorld() const
{
	return ms_Tunables.m_GuardWorldExtents && !ms_WorldLimitsPlayerAABB.ContainsPointFlat(m_pPlayerPed->GetTransform().GetPosition()) NOTFINAL_ONLY( || ms_bFakeWorldExtentsTresspassing);
}

// Look at the player and decide how to deal with them crossing the world boundary.
void CPlayerInfo::HandlePlayerWorldBreach()
{
	CVehicle* pVehicle = m_pPlayerPed->GetVehiclePedInside();

	// If the player was not inside a vehicle, they could still be standing on one.  Treat these vehicles in the same way to
	// prevent the player riding an AI vehicle of some kind to freedom.
	if (!pVehicle)
	{
		CPhysical* pGroundPhysical = m_pPlayerPed->GetGroundPhysical();
		if (pGroundPhysical && pGroundPhysical->GetIsTypeVehicle())
		{
			pVehicle = static_cast<CVehicle*>(pGroundPhysical);
		}
	}

	if (pVehicle)
	{
		// Disable the vehicle.
		HandlePlayerWorldBreachVehicle(*pVehicle);
	}
	else if (m_pPlayerPed->GetIsParachuting())
	{
		// Disable the parachute.
		HandlePlayerWorldBreachParachuting();
	}
	else if (m_pPlayerPed->GetIsSwimming() || m_pPlayerPed->GetPedType() == PEDTYPE_ANIMAL)
	{
		// Spawn a shark if possible, otherwise just kill them.
		HandlePlayerWorldBreachSwimming();
	}
}

// Make the parachute drop.
void CPlayerInfo::HandlePlayerWorldBreachParachuting()
{
	CTaskParachute* pTaskParachute = static_cast<CTaskParachute*>(m_pPlayerPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PARACHUTE));
	if (pTaskParachute)
	{
		pTaskParachute->RemoveParachuteForScript();
	}
}

// Damage plane/heli/blimp engines and sink boats/subs.
void CPlayerInfo::HandlePlayerWorldBreachVehicle(CVehicle& rVehicle)
{
	CVehicleDamage* pDamage = rVehicle.GetVehicleDamage();
	CSubmarineHandling* subHandling = rVehicle.GetSubHandling();
	CBoatHandling* boatHandling = rVehicle.GetBoatHandling();

	if( subHandling )
	{
		if (rVehicle.GetStatus() != STATUS_WRECKED)
		{
			// Tell the submarine to sink when wrecked.
			subHandling->SetSinksWhenWrecked(true);

			// Wreck the submarine.
			rVehicle.SetIsWrecked();
		}
	}
	else if( boatHandling )
	{
		if (rVehicle.GetStatus() != STATUS_WRECKED)
		{
			// Tell the boat to sink when wrecked.
			boatHandling->SetWreckedAction( BWA_SINK );

			// Wreck the boat.
			rVehicle.SetIsWrecked();

			if(!rVehicle.IsNetworkClone())
			{
				// Apparently jetskis will still go forward if you wreck them.
				rVehicle.SwitchEngineOff(true);
			}
		}
	}
	else if (rVehicle.InheritsFromPlane())
	{
		// [HACK_GTAV]
		// B* 1673245 - it is possible to glide to other code crashing (CNetworkWorldGridManager, etc) if the player gets too far away from 0,0,0.
		// So, if they are in a plane and attempting this we perform more destructive code on them.
		// Do this once the engine is destroyed from the code below.
		CPlane& rPlane = static_cast<CPlane&>(rVehicle);
		if (!pDamage || pDamage->GetEngineHealth() <= SMALL_FLOAT)
		{
			rPlane.DamageForWorldExtents();

			if( rVehicle.GetModelIndex() == MI_PLANE_MICROLIGHT )
			{
				rVehicle.BlowUpCar( NULL );
			}
		}
	}
	else if(rVehicle.InheritsFromAutomobile() && rVehicle.HasParachute())
	{
		CAutomobile* pCar = static_cast<CAutomobile*>(&rVehicle);
		
		if(pCar->IsParachuting() && !pCar->IsFinishParachutingRequested())
		{
			pCar->RequestFinishParachuting();
		}

		rVehicle.SetIsWrecked();
	}
	else if( rVehicle.HasGlider() )
	{
		rVehicle.FinishGliding();
		if(!rVehicle.IsNetworkClone())
		{
			rVehicle.SwitchEngineOff();
		}
		rVehicle.BlowUpCar( NULL );
	}
	else if( rVehicle.pHandling->GetSpecialFlightHandlingData() )
	{
		if(!rVehicle.IsNetworkClone())
		{
			rVehicle.SwitchEngineOff();
		}
		rVehicle.SetIsWrecked();

		if( rVehicle.InheritsFromBike() )
		{
			rVehicle.BlowUpCar( NULL );
		}
	}
	else if (!(rVehicle.InheritsFromHeli() || rVehicle.InheritsFromBlimp()))
	{
		Assertf(false, "Unexpected vehicle type escaping the world extents!");
		rVehicle.SetIsWrecked();
	}

	// Continuously damage the engine.
	if (Verifyf(pDamage, "No vehicle damage pointer to destroy vehicle escaping world bounds!"))
	{
		pDamage->SetEngineHealth(rage::Max(0.0f, pDamage->GetEngineHealth() - 10.0f)); //magic number - doesn't really matter how much as long as it happens continuously
	}
}

// Try to spawn a shark to kill the player, or in the worst case just force kill them with a death event.
void CPlayerInfo::HandlePlayerWorldBreachSwimming()
{
	bool bJustKillThePlayer = false;

	if (m_bHasWorldExtentsSpawnedAShark)
	{
		if (!CTaskSharkAttack::SharksArePresent())
		{
			// Here, we've already spawned a shark.
			// But somehow it either died or gave up.  So count down until the max trespassing timer expires.
			m_fSwimBoundsEscapeTimer -= fwTimer::GetTimeStep();
		}

		if (m_fSwimBoundsEscapeTimer <= 0.0f)
		{
			// Reset this so we don't keep spamming death events if death takes a while to achieve.
			m_fSwimBoundsEscapeTimer = ms_Tunables.m_MaxTimeToTrespassWhileSwimmingBeforeDeath;

			// Something went wrong, the shark was unable to kill the player.  Default to just autokill.
			bJustKillThePlayer = true;
		}
		else
		{
			return;
		}
	}

	if (NetworkInterface::IsGameInProgress())
	{
		// No sharks in MP, so just kill the player.
		bJustKillThePlayer = true;
	}
	else if (m_pPlayerPed->GetPedType() == PEDTYPE_ANIMAL)
	{
		// Force death if the player is an animal.
		bJustKillThePlayer = true;
	}
	else if (!bJustKillThePlayer)
	{
		// Create a shark.

		CWildlifeManager& rManager = CWildlifeManager::GetInstance();

		bool bSuccess = rManager.DispatchASharkNearPlayer(true);

		if (bSuccess)
		{
			m_bHasWorldExtentsSpawnedAShark = true;
		}
		else
		{
			if (!rManager.CanSpawnSharkModels())
			{
				// The wildlife manager failed to spawn in some unrecoverable way (not just a streaming failure) - don't bother with trying to spawn one again.
				bJustKillThePlayer = true;
			}
		}
	}

	if (bJustKillThePlayer)
	{
		// Just give the player the death task.
		CDeathSourceInfo info(m_pPlayerPed);
		CTask* pTaskDie = rage_new CTaskDyingDead(&info, CTaskDyingDead::Flag_ragdollDeath);
		CEventGivePedTask deathEvent(PED_TASK_PRIORITY_PRIMARY, pTaskDie);
		m_pPlayerPed->GetPedIntelligence()->AddEvent(deathEvent);
	}
}

void CPlayerInfo::ExtendWorldBoundary(const Vector3 &vCoordExtendsTo) {ms_WorldLimitsPlayerAABB.GrowPoint(VECTOR3_TO_VEC3V(vCoordExtendsTo));}

void CPlayerInfo::SetMaxPortablePickupsOfTypePermitted(int modelIndex, int maxPermitted)
{
	Assert(modelIndex != -1);

	int freeSlot = -1;

	for (u32 i=0; i<MAX_PORTABLE_PICKUP_INFOS; i++)
	{
		if (PortablePickupsInfo[i].modelIndex == modelIndex)
		{
			if (maxPermitted == -1)
			{
				// clear this slot
				PortablePickupsInfo[i].modelIndex = -1;
				PortablePickupsInfo[i].maxPermitted = 0;
				PortablePickupsInfo[i].numCarried = 0;
			}
			else
			{
				PortablePickupsInfo[i].maxPermitted = (u16)maxPermitted;
			}
			return;
		}
		else if (PortablePickupsInfo[i].modelIndex == -1 && freeSlot == -1)
		{
			freeSlot = i;
		}
	}

	if (maxPermitted != -1)
	{
		if (freeSlot == -1)
		{
			Assertf(0, "CPlayerInfo::SetMaxPortablePickupsOfTypePermitted - ran out of portable pickup infos. Increase size of MAX_PORTABLE_PICKUP_INFOS");
		}
		else 
		{
			PortablePickupsInfo[freeSlot].modelIndex = modelIndex;
			PortablePickupsInfo[freeSlot].maxPermitted = (u16)maxPermitted;
			PortablePickupsInfo[freeSlot].numCarried = 0;
		}
	}
}

bool CPlayerInfo::CanCarryPortablePickupOfType(int modelIndex, bool bAlreadyCollected)
{
	if (m_maxPortablePickupsCarried != -1)
	{
		if (bAlreadyCollected)
		{
			if (m_numPortablePickupsCarried > m_maxPortablePickupsCarried)
			{
				return false;
			}
		}
		else
		{
			if (m_numPortablePickupsCarried >= m_maxPortablePickupsCarried)
			{
				return false;
			}
		}
	}

	for (u32 i=0; i<MAX_PORTABLE_PICKUP_INFOS; i++)
	{
		if (PortablePickupsInfo[i].modelIndex == modelIndex)
		{
			if (bAlreadyCollected)
			{
				if (PortablePickupsInfo[i].numCarried <= PortablePickupsInfo[i].maxPermitted)
				{
					return true;
				}
				else
				{
					return false;
				}
			}
			else 
			{
				if (PortablePickupsInfo[i].numCarried < PortablePickupsInfo[i].maxPermitted)
				{
					return true;
				}
				else
				{
					return false;
				}
			}
		}
	}

	return true;
}

void CPlayerInfo::PortablePickupCollected(int modelIndex)
{
	PortablePickupPending = false;

	if(NetworkInterface::IsNetworkOpen())
	{
		NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Portable pickup type collected", "%d", modelIndex);
	}

	for (u32 i=0; i<MAX_PORTABLE_PICKUP_INFOS; i++)
	{
		if (PortablePickupsInfo[i].modelIndex == modelIndex)
		{
			Assert (PortablePickupsInfo[i].numCarried < PortablePickupsInfo[i].maxPermitted);

			PortablePickupsInfo[i].numCarried++;

			if(NetworkInterface::IsNetworkOpen())
			{
				NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Number of this type collected", "%d", PortablePickupsInfo[i].numCarried);
				NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Maximum permitted", "%d", PortablePickupsInfo[i].maxPermitted);
			}	

			break;
		}
	}

	m_numPortablePickupsCarried++;
}

void CPlayerInfo::PortablePickupDropped(int modelIndex)
{
	if(NetworkInterface::IsNetworkOpen())
	{
		NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Portable pickup type dropped", "%d", modelIndex);
	}

	for (u32 i=0; i<MAX_PORTABLE_PICKUP_INFOS; i++)
	{
		if (PortablePickupsInfo[i].modelIndex == modelIndex)
		{
			if (AssertVerify(PortablePickupsInfo[i].numCarried > 0))
			{
				PortablePickupsInfo[i].numCarried--;

				if(NetworkInterface::IsNetworkOpen())
				{
					NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Remaining pickups of this type collected", "%d", PortablePickupsInfo[i].numCarried);
					NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Maximum permitted", "%d", PortablePickupsInfo[i].maxPermitted);
				}

				break;
			}
		}
	}

	if (m_numPortablePickupsCarried > 0)
	{
		m_numPortablePickupsCarried--;
	}
}

bool CPlayerInfo::CanPlayerPedCollectAnyPickups(const CPed& playerPed)
{
	if (playerPed.GetPedType() == PEDTYPE_ANIMAL)
	{
		return false;
	}

	return true;
}


/////////////////////////////////////////////////////////////////////////////////
//
// FUNCTION : EvaluateCarPosition
// PURPOSE :  How much should this car slow down taking into account that other car
//
/////////////////////////////////////////////////////////////////////////////////

void CPlayerInfo::EvaluateCarPosition(CEntity* pEntity, CPed *pPed, Vector3& vSeatPos, float Distance, float *pCloseness, CVehicle **ppClosestVehicle, Vector3& vCarSearchDirection, const bool bUsingStickInput)
{
	float	PedOrientation, CarOrientation, AngleDiff, Closeness;
	Vector3 vTargetPos = vSeatPos;	//	pEntity->GetPosition();


	CVehicle* pVehicle = static_cast<CVehicle*>(pEntity);
	float fDistanceLimit = SCANNEARBYVEHICLES;
	// if this is a boat, reduce the distance by the bounding radius (cos some of them are massive)
	if ( pVehicle->GetVehicleType() == VEHICLE_TYPE_BOAT )
	{
		fDistanceLimit += pEntity->GetBoundRadius();;
	}

	const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	// Calculate the angle difference between the pedestrian and the car.
	PedOrientation = fwAngle::GetATanOfXY(vCarSearchDirection.x, vCarSearchDirection.y);
	CarOrientation = fwAngle::GetATanOfXY( (vTargetPos.x - vPedPos.x), (vTargetPos.y - vPedPos.y));
	AngleDiff = PedOrientation - CarOrientation;
	AngleDiff = fwAngle::LimitRadianAngle(AngleDiff);
	AngleDiff = ABS(AngleDiff);

	float fCameraScore = 0.0f;
	float fMovementAwayScore = 1.0f;	// Decreases if we're moving away from a close vehicle

	if (bUsingStickInput)
	{
		bool bConsiderVehicleValidAnyway = false;
		// Unless its close to the player and we're pointing the cam at it, B*984581
		Vector3 vVehPos = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());
		const float fDistToVehSqd = (vVehPos - vPedPos).Mag2();
		float fDistTolerance = ms_Tunables.m_MaxDistToConsiderVehicleValid;
		if (pEntity->GetIsTypeVehicle())
		{
			fDistTolerance += static_cast<CVehicle*>(pEntity)->GetVehicleModelInfo()->GetBoundingSphereRadius();
		}

		Vector3 vCamFront = camInterface::GetFront();
		Vector3 vCamPos = camInterface::GetPos();
		Vector3 vCamToVeh = vTargetPos - vCamPos;
		vCamToVeh.Normalize();
		fCameraScore = vCamFront.Dot(vCamToVeh);

		if (fDistToVehSqd < rage::square(fDistTolerance))
		{
			if (fCameraScore >= ms_Tunables.m_MinDotToConsiderVehicleValid)
			{
				bConsiderVehicleValidAnyway = true;
			}
		}

		TUNE_GROUP_BOOL(CHOOSE_VEHICLE_TUNE, ENABLE_MOVEMENT_AWAY_SCORE, true);
		if (ENABLE_MOVEMENT_AWAY_SCORE)
		{
			// If the ped would need to double back to get in the vehicle, penalize them for it
			const float fDesiredHeading = fwAngle::LimitRadianAngleSafe(pPed->GetDesiredHeading());
			Vector3 vPedDesiredDirection = Vector3(0.0f, 1.0f, 0.0f);
			vPedDesiredDirection.RotateZ(fDesiredHeading);

			TUNE_GROUP_FLOAT(CHOOSE_VEHICLE_TUNE, MOVEMENT_AWAY_REJECT_COS_ANGLE, 0.0f, -1.0f, 1.0f, 0.01f);
			Vector3 vPedToTarget = (vTargetPos - vPedPos);
			vPedToTarget.Normalize();
			const float fCosAngle = vPedToTarget.Dot(vPedDesiredDirection);
			if (fCosAngle < MOVEMENT_AWAY_REJECT_COS_ANGLE)
			{
				const float fMbrNorm = pPed->GetMotionData()->GetCurrentMbrY() * ONE_OVER_MOVEBLENDRATIO_SPRINT;
				fMovementAwayScore -= fMbrNorm;
			}
		}

		// Ignore vehicles outside 90 degrees when using the stick
		if (AngleDiff > HALF_PI)
		{
			if (!bConsiderVehicleValidAnyway)
			{
				// Stick bounce back detection B*1617109
				if (pPed->IsLocalPlayer() && bUsingStickInput)
				{
					const CControl* pControl = pPed->GetControlFromPlayer();
					if (pControl)
					{
						Vector2 vecStickInput(pControl->GetPedWalkLeftRight().GetNorm(), pControl->GetPedWalkUpDown().GetNorm());
						TUNE_GROUP_INT(CHOOSE_VEHICLE_TUNE, MAX_STICK_MAG_TO_TRY_ACCEPT, 80, 0, 255, 1);
						if (Abs(vecStickInput.x) < MAX_STICK_MAG_TO_TRY_ACCEPT && Abs(vecStickInput.y) < MAX_STICK_MAG_TO_TRY_ACCEPT)
						{
							// Calculate the angle difference between the pedestrian and the car.
							Vector3 vCarSearchDirection = VEC3V_TO_VECTOR3(pPed->GetTransform().GetB());
							float PedOrientation = fwAngle::GetATanOfXY(vCarSearchDirection.x, vCarSearchDirection.y);
							float CarOrientation = fwAngle::GetATanOfXY( (vTargetPos.x - vPedPos.x), (vTargetPos.y - vPedPos.y));
							float AngleDiff = PedOrientation - CarOrientation;
							AngleDiff = fwAngle::LimitRadianAngle(AngleDiff);
							AngleDiff = ABS(AngleDiff);

							if (AngleDiff < HALF_PI)
							{
								bConsiderVehicleValidAnyway = true;
							}

							AI_FUNCTION_LOG_WITH_ARGS("%s vehicle %s because AngleDiff = %.2f, fDistToVehSqd = %.2f, fDistTolerance = %.2f", bConsiderVehicleValidAnyway ? "Considering" : "Rejecting", AILogging::GetDynamicEntityNameSafe(pVehicle), AngleDiff, fDistToVehSqd, fDistTolerance);
						}
					}
				}

				if (!bConsiderVehicleValidAnyway)
				{
					AI_LOG_REJECTION_WITH_IDENTIFIER_AND_REASON("[VehicleEntryExit][CPlayerInfo::ScanForVehicleToEnter]",pVehicle, "Not considered valid");
					return;
				}
			}
		}
	}

	const float fDistanceWeighting = bUsingStickInput ? ms_Tunables.m_DistanceWeighting : ms_Tunables.m_DistanceWeightingNoStick;
	const float fHeadingWeighting = bUsingStickInput ? ms_Tunables.m_HeadingWeighting : ms_Tunables.m_HeadingWeightingNoStick;
	const float fCameraWeighting = bUsingStickInput ? ms_Tunables.m_CameraWeighting : 0.0f;
	
	const float fAngleScore = fHeadingWeighting * (1.0f - (AngleDiff / ms_Tunables.m_MaxAngleConsidered));
	const float fDistanceScore = fDistanceWeighting * ((fDistanceLimit - Distance) / fDistanceLimit);
	fCameraScore *= fCameraWeighting;
	fMovementAwayScore *= ms_Tunables.m_MovementAwayWeighting;

	Closeness = fDistanceScore + fAngleScore + fCameraScore + fMovementAwayScore;

	// Make sure the score is non negative as the distance score can go heavily negative as it doesn't get clamped
	if (Closeness < 0.0f && pPed->GetGroundPhysical() == pVehicle)
	{
		Closeness = 0.0f;
	}

	// if car is on fire we might still want to get in to drive it and jump out again, so just lower the weighting for this vehicle
	if(pVehicle->GetVehicleDamage()->GetEngineHealth() <= 0.0f || pVehicle->GetVehicleDamage()->GetPetrolTankHealth() <= 0.0f)
		Closeness *= ms_Tunables.m_OnFireWeightingMult;

	// Make sure there is a clear los to the target seat (maybe too restrictive?)
	WorldProbe::CShapeTestProbeDesc probeDesc;
	WorldProbe::CShapeTestFixedResults<> probeResults;
	probeDesc.SetResultsStructure(&probeResults);
	TUNE_GROUP_BOOL(CHOOSE_VEHICLE_TUNE, TEST_VEHICLES_AND_USE_PED_POS, true);
	Vector3 vTestPos = TEST_VEHICLES_AND_USE_PED_POS ? vPedPos : pPed->GetBonePositionCached(BONETAG_HEAD);
	if (TEST_VEHICLES_AND_USE_PED_POS)
	{
		TUNE_GROUP_FLOAT(CHOOSE_VEHICLE_TUNE, EXTRA_Z_OFFSET, 0.0f, -1.0f, 1.0f, 0.01f);
		vTestPos.z += EXTRA_Z_OFFSET;
	}

	if (pVehicle->InheritsFromBike() && pVehicle->GetWheel(0))
	{
		Vector3 vWheelPos;
		pVehicle->GetWheel(0)->GetWheelPosAndRadius(vWheelPos);
		vSeatPos.x = vWheelPos.x;
		vSeatPos.y = vWheelPos.y;
	}

	probeDesc.SetStartAndEnd(vTestPos, vSeatPos);
	s32 iFlags = ArchetypeFlags::GTA_MAP_TYPE_MOVER | ArchetypeFlags::GTA_OBJECT_TYPE;
	if (TEST_VEHICLES_AND_USE_PED_POS)
	{
		iFlags |= ArchetypeFlags::GTA_VEHICLE_TYPE;
	}
	probeDesc.SetIncludeFlags(iFlags);
	probeDesc.SetExcludeEntity(NULL);
	
	if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
	{
		const CEntity* pHitEntity = probeResults[0].GetHitEntity();
		if (pHitEntity && (!pHitEntity->GetIsTypeVehicle() || pHitEntity != pEntity))
		{
			bool bShouldEarlyOut = true;

			// Check if the other vehicle we hit is drivable, if not, then ignore it
			if (pHitEntity->GetIsTypeVehicle())
			{
				if (static_cast<const CVehicle*>(pHitEntity)->GetStatus() == STATUS_WRECKED)
				{
					bShouldEarlyOut = false;
				}
			}	

			// Don't count things that are attached to us
			if (pHitEntity->GetAttachParent() == pPed)
			{
				bShouldEarlyOut = false;
			}

			// The below will only return false if we're hit a pickup exclusively, in this case we don't want to prevent entry
			if (!CPedGeometryAnalyser::TestIfCollisionTypeIsValid(pPed, iFlags, probeResults))
			{
				bShouldEarlyOut = false;
			}

			if (pHitEntity->GetIsTypeVehicle() && !pHitEntity->IsCollisionEnabled())
			{
				bShouldEarlyOut = false;
#if __BANK
				CVehicleLockDebugInfo debugInfo(pPed, static_cast<const CVehicle*>(pHitEntity));
				debugInfo.Print();
				aiWarningf("Ignoring vehicle %s because it's collision isn't enabled, is visible %s, please add a bug with full logs for default ai code", AILogging::GetDynamicEntityNameSafe(static_cast<const CVehicle*>(pHitEntity)), AILogging::GetBooleanAsString(pHitEntity->GetIsVisible()));
#endif // __BANK
			}
			
			if (pHitEntity->GetIsTypeObject() && pHitEntity->GetAttachParent() == pVehicle && static_cast<const CObject*>(pHitEntity)->PopTypeIsMission())
			{
				bShouldEarlyOut = false;
			}

			if (bShouldEarlyOut)
			{
#if __BANK
				char szDebugText[256];
				formatf(szDebugText, "Probe hit invalid collision, hit entity : %s", pHitEntity->GetIsDynamic() ? AILogging::GetDynamicEntityNameSafe(static_cast<const CDynamicEntity*>(pHitEntity)) : pHitEntity->GetModelName());
				AI_LOG_REJECTION_WITH_IDENTIFIER_AND_REASON("[VehicleEntryExit][CPlayerInfo::EvaluateCarPosition]", pVehicle, szDebugText);
#endif // __BANK
#if DEBUG_DRAW
				if( CTaskPlayerOnFoot::ms_bRenderPlayerVehicleSearch || CPedDebugVisualiser::GetDebugDisplay()==CPedDebugVisualiser::eVehicleEntryDebug)
				{
					CTask::ms_debugDraw.AddLine(RCC_VEC3V(vTestPos), RCC_VEC3V(vTargetPos), Color_red, 5000);
				}
#endif // DEBUG_DRAW
				return;
			}
		}
	}

#if DEBUG_DRAW
	if( CTaskPlayerOnFoot::ms_bRenderPlayerVehicleSearch || CPedDebugVisualiser::GetDebugDisplay()==CPedDebugVisualiser::eVehicleEntryDebug)
	{
		char szString[512];
		CTask::ms_debugDraw.AddLine( RCC_VEC3V(vTestPos), RCC_VEC3V(vTargetPos), Color_green, 5000);
		sprintf(szString, "%s, T(%.2f) = H(%.2f) + D(%.2f) + C(%.2f) + M(%.2f)", bUsingStickInput ? "S" : "N", Closeness, fAngleScore, fDistanceScore, fCameraScore, fMovementAwayScore );
		CTask::ms_debugDraw.AddText(RCC_VEC3V(vTargetPos), 0, 0, szString, Color_blue, 5000);
	}
#endif // DEBUG_DRAW

	AI_LOG_WITH_ARGS("[VehicleEntryExit][CPlayerInfo::EvaluateCarPosition] - Vehicle %s, Scores (distance = %.2f | angle = %.2f | camera = %.2f | movement = %.2f), Total = %.2f\n", AILogging::GetDynamicEntityNameSafe(pVehicle), fDistanceScore, fAngleScore, fCameraScore, fMovementAwayScore, Closeness);

	if (Closeness >= (*pCloseness) )
	{
		*pCloseness = Closeness;
		*ppClosestVehicle = (CVehicle *)pEntity;
	}
}

void CPlayerInfo::EvaluateAnimalPosition(CEntity* pEntity, CPed *m_pPed, float Distance, float *pCloseness, CPed **ppClosestAnimal, Vector3& vAnimalSearchDirection, const bool bUsingStickInput)
{
	float	PedOrientation, AnimalOrientation, AngleDiff, Closeness;
	Vector3 vTargetPos = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());

	float fDistanceLimit = ms_Tunables.m_ScanNearbyMountsDistance;	

	const Vector3 vPedPos = VEC3V_TO_VECTOR3(m_pPed->GetTransform().GetPosition());
	// Calculate the angle difference between the pedestrian and the car.
	PedOrientation = fwAngle::GetATanOfXY(vAnimalSearchDirection.x, vAnimalSearchDirection.y);
	AnimalOrientation = fwAngle::GetATanOfXY( (vTargetPos.x - vPedPos.x), (vTargetPos.y - vPedPos.y));
	AngleDiff = PedOrientation - AnimalOrientation;
	AngleDiff = fwAngle::LimitRadianAngle(AngleDiff);
	AngleDiff = ABS(AngleDiff);

	// Ignore vehicles outside 90 degrees when using the stick
	if( bUsingStickInput && AngleDiff > HALF_PI )
	{
		return;
	}

#define MAXANGLECONSIDERED (2.0f*PI)

	const float fAngleScore = 1.0f - (AngleDiff / MAXANGLECONSIDERED);
	const float fDistanceScore =  (fDistanceLimit - Distance) / fDistanceLimit;
	if( bUsingStickInput )
		Closeness = (fDistanceScore * ms_Tunables.m_DistanceWeighting) + (fAngleScore * ms_Tunables.m_HeadingWeighting);
	else
		Closeness = (fDistanceScore * ms_Tunables.m_DistanceWeightingNoStick) + (fAngleScore * ms_Tunables.m_HeadingWeightingNoStick);	

	if (Closeness >= (*pCloseness) )
	{
		*pCloseness = Closeness;
		*ppClosestAnimal = (CPed *)pEntity;
	}
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : GetSpeed
// PURPOSE :  Returns the speed of this player
//
/////////////////////////////////////////////////////////////////////////////////
Vector3 CPlayerInfo::GetSpeed(void)
{
	if (m_pPlayerPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && m_pPlayerPed->GetMyVehicle())
	{
		Assertf(m_pPlayerPed->GetMyVehicle(), "CPlayerInfo::GetSpeed - Player is not in a vehicle");
		
		return (m_pPlayerPed->GetMyVehicle()->GetVelocity());
	}
	else
	{
		return (m_pPlayerPed->GetVelocity());
	}
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : GetPos
// PURPOSE :  Returns the coordinates of this player
//
/////////////////////////////////////////////////////////////////////////////////
Vector3 CPlayerInfo::GetPos(void)
{
	if (m_pPlayerPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && m_pPlayerPed->GetMyVehicle())
	{
		Assertf(m_pPlayerPed->GetMyVehicle(), "CPlayerInfo::GetPos - Player is not in a vehicle");
	
		return (VEC3V_TO_VECTOR3(m_pPlayerPed->GetMyVehicle()->GetTransform().GetPosition()));
	}
	else
	{
		return (VEC3V_TO_VECTOR3(m_pPlayerPed->GetTransform().GetPosition()));
	}
}

/////////////////////////////////////////////////////////////////////////////////

bool CAnimSpeedUps::ShouldUseMPAnimRates()
{
#if !__FINAL
	return NetworkInterface::IsGameInProgress() || ms_Tunables.m_ForceMPAnimRatesInSP;
#else // !__FINAL
	return NetworkInterface::IsGameInProgress();
#endif // !__FINAL
}

/////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindPlayerVehicle
// PURPOSE :  Returns a pointer to the player car (or NULL if there isn't one)
/////////////////////////////////////////////////////////////////////////////////

CVehicle *FindPlayerVehicle(CPlayerInfo* pPlayer, bool UNUSED_PARAM(bReturnRemoteVehicle))
{
	// you can't use this in a non-update thread - you are dereffing an entity ptr!
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE)||CSystem::IsThisThreadId(SYS_THREAD_AUDIO));

	if (!pPlayer)
		pPlayer = CGameWorld::GetMainPlayerInfo();

	Assert(pPlayer);

	CPed* pPlayerPed = pPlayer ? pPlayer->GetPlayerPed() : NULL;

	if (pPlayerPed)
	{
		if (pPlayerPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
		{
			//RAGE			if (!CReplay::ReplayGoingOn()) Assertf(CGameWorld::Players[Player].m_pPlayerPed->m_pMyVehicle, "FindPlayerVehicle - Player not in vehicle");
			//			if(bReturnRemoteVehicle && pPlayerPed->GetPlayerInfo()->GetRemoteVehicle())
			//				return pPlayerPed->GetPlayerInfo()->GetRemoteVehicle();
			//			else
			return (pPlayerPed->GetMyVehicle());
		}
	}

	return NULL;
}




/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindPlayerPed
// PURPOSE :  Returns a pointer to the player ped
/////////////////////////////////////////////////////////////////////////////////

CPed *FindPlayerPed(CPlayerInfo* pPlayer)
{
	// you can't use this in a non-update thread - you are dereffing an entity ptr!
	//	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));
	if (pPlayer)
	{
		return pPlayer->GetPlayerPed();
	}

	return FindPlayerPed();
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindPlayerPed
// PURPOSE :  Returns a pointer to the main player ped
/////////////////////////////////////////////////////////////////////////////////

CPed *FindPlayerPed()
{
	// you can't use this in a non-update thread - you are dereffing an entity ptr!
	//	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));
	return CGameWorld::FindLocalPlayer();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindSpectatorVehicle
// PURPOSE :  Returns a pointer to the spectator car (or NULL if there isn't one)
/////////////////////////////////////////////////////////////////////////////////
CVehicle* 
FindFollowVehicle(CPlayerInfo* pPlayer)
{
	// you can't use this in a non-update thread - you are dereffing an entity ptr!
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE)||CSystem::IsThisThreadId(SYS_THREAD_AUDIO));

	CPed* pPlayerPed = NULL;

	if (!pPlayer)
	{
		CPed * player = CGameWorld::FindFollowPlayer();
		pPlayer = player ? player->GetPlayerInfo() : NULL;

		//We are spectating a Ped not a PlayerPed
		if (player && !player->IsAPlayerPed() && NetworkInterface::IsInSpectatorMode())
		{
			pPlayerPed = player;
		}
	}
	else
	{
		pPlayerPed = pPlayer->GetPlayerPed();
	}

	Assert(pPlayerPed);

	if (pPlayerPed && pPlayerPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
	{
		return (pPlayerPed->GetMyVehicle());
	}

	return NULL;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindSpectatorPed
// PURPOSE :  Returns a pointer to the spectator ped
/////////////////////////////////////////////////////////////////////////////////
CPed*
FindFollowPed(CPlayerInfo* pPlayer)
{
	// you can't use this in a non-update thread - you are dereffing an entity ptr!
	//	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));

	CPed* pPlayerPed = NULL;

	if (!pPlayer)
	{
		CPed* player = CGameWorld::FindFollowPlayer();
		pPlayer = player ? player->GetPlayerInfo() : NULL;

		//We are spectating a Ped not a PlayerPed
		if (player && !player->IsAPlayerPed() && NetworkInterface::IsInSpectatorMode())
		{
			pPlayerPed = player;
		}
		else if (pPlayer)
		{
			pPlayerPed = pPlayer->GetPlayerPed();
		}
	}
	else
	{
		pPlayerPed = pPlayer->GetPlayerPed();
	}

	return pPlayerPed;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : IsRestartingAfterDeath
// PURPOSE :  Should return TRUE if the player is restarting due to being killed.
/////////////////////////////////////////////////////////////////////////////////
bool CPlayerInfo::IsRestartingAfterDeath(void)
{
	if (this->m_PlayerState == PLAYERSTATE_HASDIED)
	{
		return (TRUE);
	}
	return (FALSE);	
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : IsRestartingAfterArrest
// PURPOSE :  Not implemented yet. Should return TRUE if the player is restarting
//										due to being arrested.
/////////////////////////////////////////////////////////////////////////////////
bool CPlayerInfo::IsRestartingAfterArrest(void)
{
	if (this->m_PlayerState == PLAYERSTATE_HASBEENARRESTED)
	{
		return (TRUE);
	}
	return (FALSE);	
}

void CPlayerInfo::KillPlayer( void )
{
	m_PlayerState = CPlayerInfo::PLAYERSTATE_HASDIED;		
}

void CPlayerInfo::ArrestPlayer( void )
{
	m_PlayerState = CPlayerInfo::PLAYERSTATE_HASBEENARRESTED;
}

void CPlayerInfo::ResurrectPlayer( void )
{
	//
	// RESET PED STATE
	//

	//A clone always has its state updated via sync messages
	if (!GetPlayerPed()->IsNetworkClone()) 
	{
		ResetSprintEnergy();
		GetTargeting().ClearLockOnTarget();
		GetPlayerPed()->SetInitialState();

		if (m_PlayerState == CPlayerInfo::PLAYERSTATE_IN_MP_CUTSCENE)
		{
			NetworkInterface::SetInMPCutscene(false, false);
		}
	
		SetPlayerState(CPlayerInfo::PLAYERSTATE_PLAYING);
	}

	m_TimeOfPlayerStateChange = fwTimer::GetTimeInMilliseconds_NonScaledClipped();

	m_fTimeBeenSprinting = 0.0f;
	m_fSprintStaminaUpdatePeriod = 0.0f;
	m_fSprintStaminaDurationMultiplier = 1.0f;	
	m_fSprintApplyDamageTimer = 0;

	EnableAllControls();
	SetPlayerControl(false, false);

	// If the script has requested that player controls are not re-enabled on death
	if(IsControlsScriptDisabled() && !bEnableControlOnDeath)
	{
		DisableControlsScript();
	}

	// Reset these as after the player has spawned they have left the world extents.
	m_bHasWorldExtentsSpawnedAShark = false;
	m_fSwimBoundsEscapeTimer = ms_Tunables.m_MaxTimeToTrespassWhileSwimmingBeforeDeath;
	NOTFINAL_ONLY(ms_bFakeWorldExtentsTresspassing = false);
}

dev_float sfPlayerMaxRange = 40000.0f*40000.0f;
dev_float sfPlayerMaxRangeHealthLoss = 1.0f;

void CPlayerInfo::SetPlayerControl(bool bAmbientScript, bool bDisableControls, float ExtinguishRange, bool bClearTasks, bool bRemoveFiresAndProjectiles, bool bAllowDamage)
{
	u32 iFlags = 0;
	if(bAmbientScript) iFlags |= SPC_AmbientScript;
	if(bDisableControls) iFlags |= SPC_DisableControls;
	if(bClearTasks) iFlags |= SPC_ClearTasks;
	if(bRemoveFiresAndProjectiles) iFlags |= (SPC_RemoveFires | SPC_RemoveExplosions | SPC_RemoveProjectiles);
	if(bAllowDamage) iFlags |= SPC_AllowPlayerToBeDamaged;

	SetPlayerControl(iFlags, ExtinguishRange);
}

void CPlayerInfo::SetPlayerControl(const u32 iFlags, const float fRemovalRange)
{
	const bool bAmbientScript			= ((iFlags&SPC_AmbientScript)!=0);
	const bool bDisableControls			= ((iFlags&SPC_DisableControls)!=0);
	const bool bClearTasks				= ((iFlags&SPC_ClearTasks)!=0);
	const bool bRemoveFires				= ((iFlags&SPC_RemoveFires)!=0);
	const bool bRemoveExplosions		= ((iFlags&SPC_RemoveExplosions)!=0);
	const bool bRemoveProjectiles		= ((iFlags&SPC_RemoveProjectiles)!=0);
	const bool bDeactivateGadgets		= ((iFlags&SPC_DeactivateGadgets)!=0);
	const bool bLeaveCameraControlsOn	= ((iFlags&SPC_LeaveCameraControlsOn)!=0);
	const bool bAllowDamage				= ((iFlags&SPC_AllowPlayerToBeDamaged)!=0);
	const bool bDontStopOtherCarsAroundPlayer = ((iFlags&SPC_DontStopOtherCarsAroundPlayer)!=0);
	const bool bAllowPadShake			= ((iFlags&SPC_AllowPadShake)!=0);
	if (bAmbientScript)
	{
		if (bDisableControls)
		{
			DisableControlsScriptAmbient();
		}
		else
		{
			EnableControlsScriptAmbient();
		}
	}
	else
	{
		if (bDisableControls)
		{
			DisableControlsScript();
		}
		else
		{
			EnableControlsScript();
		}
	}

	if ( IsControlsScriptDisabled() || IsControlsScriptAmbientDisabled() )
	{
		// If we haven't been told to prevent everybody from backing off, then set it to TRUE
		if((iFlags & SPC_PreventEverybodyBackoff) == 0)
		{
			m_Wanted.m_EverybodyBackOff = TRUE;
		}

		// Stop cars so that their momentum doesn't get us in trouble.
		CGameWorld::StopAllLawEnforcersInTheirTracks();
		if (!bAllowPadShake)
		{
			CControlMgr::StopPadsShaking();
		}
		//m_pPlayerPed->m_nPhysicalFlags.bNotDamagedByAnything = !bAllowDamage;

		m_pPlayerPed->GetPlayerInfo()->SetPlayerCanBeDamaged(bAllowDamage);
		if(bAllowDamage)
		{
			m_DamageAllowedFromSetPlayerControl = true;
		}

		if(m_fSprintEnergy < 0.0f)
			m_fSprintEnergy = 0.0f;
	
		// Get the player to quit all his tasks
		// (a networked player controlled by another machine will not be running any tasks)
		if( bClearTasks && !m_pPlayerPed->IsNetworkClone())
		{
			m_pPlayerPed->GetPedIntelligence()->ClearTasks(true,false);
		}

		// Network players are sometimes remaining on fire when they are respawned - so extinguish the fire directly
        g_fireMan.ExtinguishEntityFires(m_pPlayerPed, true); 

		if(bRemoveFires)
		{
			// Extinguish all fires in area
			// Only remove fires within 10m from player.
			// It looks bad when large fires just extinguish. (If it is dangerous change back to 4000, Obbe)
			Vector3 pos = GetPos();
			g_fireMan.ExtinguishArea(RCC_VEC3V(pos), fRemovalRange, true);	
			CGameWorld::ExtinguishAllCarFiresInArea(GetPos(), fRemovalRange, true);
		}
		if(bRemoveExplosions)
		{
		    // Remove all explosions in area
		    CExplosionManager::RemoveExplosionsInArea(EXP_TAG_DONTCARE, GetPos(), 4000.0f);
		}
		if(bRemoveProjectiles)
		{
		    // Remove all projectiles in the world
 		    CProjectileManager::RemoveAllProjectiles();
        }
		if (bDontStopOtherCarsAroundPlayer)
		{
			m_bStopNearbyVehicles = false;
		}
		
		m_pPlayerPed->GetPlayerInfo()->GetTargeting().ClearLockOnTarget();

		// Disable any pending bullets
		CTaskAimGunOnFoot* pAimGunOnFootTask = static_cast<CTaskAimGunOnFoot*>(m_pPlayerPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_ON_FOOT));
		if(pAimGunOnFootTask)
		{
			pAimGunOnFootTask->ResetWillFire();
		}
	}
	else if ( !IsControlsScriptDisabled() && !IsControlsScriptAmbientDisabled() )
	{
		m_Wanted.m_EverybodyBackOff = FALSE;
		m_pPlayerPed->m_nPhysicalFlags.bNotDamagedByAnything = FALSE;
		m_pPlayerPed->GetPlayerInfo()->SetPlayerCanBeDamaged(TRUE);
		m_bStopNearbyVehicles = true;
	}

	// Handle deactivating all the player's equipped gadgets
	if(bDeactivateGadgets)
	{
		weaponAssert(m_pPlayerPed->GetWeaponManager());
		m_pPlayerPed->GetWeaponManager()->DestroyEquippedGadgets();
	}

	// If reenabling controls, always reenable camera controls
	if (!bDisableControls)
	{
		EnableControlsCamera();
	}
	else 
	{
		if(!bLeaveCameraControlsOn)
			DisableControlsCamera();
		else
			EnableControlsCamera();
	}
}

bool CPlayerInfo::AreAnyControlsOtherThanFrontendDisabled()
{
#if !GTA_REPLAY
	Assertf(NetworkInterface::IsGameInProgress(), "CPlayerInfo::AreAnyControlsOtherThanFrontendDisabled - only expected this to be called during network games");
#endif

	u32 TempControlsFlag = (DisableControls & ~DISABLE_FRONTEND);
	return (TempControlsFlag != 0);
}

//
// name:		AddHealth
// description:	Adds a certain amount of health to this player taking into account the maximum health
//
void CPlayerInfo::AddHealth(s32 Amount)
{
	Assert(m_pPlayerPed);

	m_pPlayerPed->SetHealth(Clamp(m_pPlayerPed->GetHealth() + Amount, m_pPlayerPed->GetHealth(), (float)MaxHealth));
}

//
// name:		SetPlayerState
// description:	Sets the new player state and also executes a bit of logic related to player state changes.
//
void    
CPlayerInfo::SetPlayerState(const State newState)
{
	if (newState != m_PlayerState)
	{
		if (m_pPlayerPed)
		{
			if(m_pPlayerPed->GetNetworkObject())
			{
				NetworkLogUtils::WriteLogEvent(NetworkInterface::GetPlayerMgr().GetLog(), "NEW_PLAYER_STATE", "%s", m_pPlayerPed->GetNetworkObject()->GetLogName());

				if (m_pPlayerPed->IsLocalPlayer() && m_PlayerState == PLAYERSTATE_IN_MP_CUTSCENE && newState != PLAYERSTATE_PLAYING)
				{
					Assertf(0, "Trying to alter the state of the local player during an MP cutscene");
					return;
				}

				switch (newState)
				{
				case PLAYERSTATE_INVALID:
					NetworkInterface::GetPlayerMgr().GetLog().WriteDataValue("State", "Invalid");
					break;
				case PLAYERSTATE_PLAYING:
					NetworkInterface::GetPlayerMgr().GetLog().WriteDataValue("State", "Playing");
					break;
				case PLAYERSTATE_HASDIED:
					NetworkInterface::GetPlayerMgr().GetLog().WriteDataValue("State", "Died");
					break;
				case PLAYERSTATE_HASBEENARRESTED:
					NetworkInterface::GetPlayerMgr().GetLog().WriteDataValue("State", "Arrested");
					break;
				case PLAYERSTATE_FAILEDMISSION:
					NetworkInterface::GetPlayerMgr().GetLog().WriteDataValue("State", "Failed Mission");
					break;
				case PLAYERSTATE_LEFTGAME:
					NetworkInterface::GetPlayerMgr().GetLog().WriteDataValue("State", "Left Game");
					break;
				case PLAYERSTATE_RESPAWN:
					NetworkInterface::GetPlayerMgr().GetLog().WriteDataValue("State", "Respawn");
					break;
				case PLAYERSTATE_IN_MP_CUTSCENE:
					NetworkInterface::GetPlayerMgr().GetLog().WriteDataValue("State", "In MP Cutscene");
					break;
				default:
					Assertf(0, "Unrecognised player state");
				}
			}
		}

		if( m_PlayerState == PLAYERSTATE_RESPAWN ||
			(m_pPlayerPed && m_pPlayerPed->IsNetworkClone() && m_PlayerState == PLAYERSTATE_HASDIED) )
		{
			m_TimeOfPlayerLastRespawn = fwTimer::GetTimeInMilliseconds_NonScaledClipped();
		}

		m_PlayerState = newState;
		m_TimeOfPlayerStateChange = fwTimer::GetTimeInMilliseconds_NonScaledClipped();
	}
}

const char* 
CPlayerInfo::PlayerStateToString() const
{
	const char* state = NULL;

	switch (m_PlayerState)
	{
	case PLAYERSTATE_INVALID:         state = "Invalid"; break;
	case PLAYERSTATE_PLAYING:         state = "Playing"; break;
	case PLAYERSTATE_HASDIED:         state = "Died"; break;
	case PLAYERSTATE_HASBEENARRESTED: state = "Arrested"; break;
	case PLAYERSTATE_FAILEDMISSION:   state = "Failed Mission"; break;
	case PLAYERSTATE_LEFTGAME:        state = "Left Game"; break;
	case PLAYERSTATE_RESPAWN:         state = "Respawn"; break;
	case PLAYERSTATE_IN_MP_CUTSCENE:  state = "In MP Cutscene"; break;
	default:
		state = "Unknown";
		break;
	}

	return state;
}

bool CPlayerInfo::GetHasRespawnedWithinTime(u32 uTimeLastRespawnedCheck) const
{
	if(m_TimeOfPlayerLastRespawn != 0)
	{
		u32 timeSinceLastHappened;
		u32 currTime = fwTimer::GetTimeInMilliseconds_NonScaledClipped();

		// handle the time wrapping
		if (currTime < m_TimeOfPlayerLastRespawn)
		{
			timeSinceLastHappened = (MAX_UINT32 - m_TimeOfPlayerLastRespawn) + currTime;
		}
		else
		{
			timeSinceLastHappened = currTime - m_TimeOfPlayerLastRespawn;
		}

		return timeSinceLastHappened < uTimeLastRespawnedCheck;
	}

	return false;
}


/////////////////////////////////////////////////////////////////
//Player Reset Flags 
/////////////////////////////////////////////////////////////////
CPlayerResetFlags::CPlayerResetFlags()
{
	m_iPlayerResetFlags = 0;

#if RSG_PC
	for(int i = 0; i < fwRandom::GetRandomNumberInRange(MIN_NUM_SYSOBF_LINKS, MAX_NUM_SYSOBF_LINKS); i++)
	{
		m_iPlayerResetFlags.AddLink(rage_new sysLinkedData<u32, ReportPlayerResetMismatch>(m_iPlayerResetFlags));
	}
#endif

}

CPlayerResetFlags::~CPlayerResetFlags()
{ 
}

void CPlayerResetFlags::ResetPreAI()
{
	UnsetFlag(PRF_ASSISTED_AIMING_ON); 
	UnsetFlag(PRF_RUN_AND_GUN);
	UnsetFlag(PRF_NO_RETICULE_AIM_ASSIST_ON);
	UnsetFlag(PRF_DISABLE_AIM_CAMERA);
	UnsetFlag(PRF_DISABLE_CAMERA_VIEW_MODE_CYCLE);
	UnsetFlag(PRF_CAMERA_VIEW_MODE_SWITCHED_TO_OR_FROM_FIRST_PERSON);
	UnsetFlag(PRF_CAMERA_VIEW_MODE_SWITCHED_TO_FIRST_PERSON);
	UnsetFlag(PRF_DISABLE_CAN_USE_COVER);
	UnsetFlag(PRF_DOT_SHOCKED);
	UnsetFlag(PRF_DOT_CHOKING);
}

void CPlayerResetFlags::ResetPostAI()
{
	UnsetFlag(PRF_NOT_ALLOWED_TO_ENTER_ANY_CAR);	//A script controlled flag. 
	UnsetFlag(PRF_FORCED_ZOOM);						//A script controlled flag. 
	UnsetFlag(PRF_FORCED_AIMING);
	UnsetFlag(PRF_DISABLE_HEALTH_RECHARGE);			//A script controlled flag. 
	UnsetFlag(PRF_EXPLOSIVE_AMMO_ON);				//A script controlled flag. 
	UnsetFlag(PRF_FIRE_AMMO_ON);					//A script controlled flag. 
	UnsetFlag(PRF_EXPLOSIVE_MELEE_ON);				//A script controlled flag. 
	UnsetFlag(PRF_SUPER_JUMP_ON);					//A script controlled flag. 
	UnsetFlag(PRF_INCREASE_JUMP_SUPPRESSION_RANGE);	//A script controlled flag. 
	UnsetFlag(PRF_DISABLE_VEHICLE_REWARDS);			//A script controlled flag. 
	UnsetFlag(PRF_BEAST_JUMP_ON);					//A script controlled flag. 
	UnsetFlag(PRF_FORCED_JUMP);						//A script controlled flag. 
}

void CPlayerResetFlags::ResetPostPhysics()
{
	// Script controlled flags 
	UnsetFlag(PRF_USE_COVER_THREAT_WEIGHTING);
	UnsetFlag(PRF_DISABLE_DISPATCHED_HELI_REFUEL);
	UnsetFlag(PRF_DISABLE_DISPATCHED_HELI_DESTROYED_SPAWN_DELAY);
	UnsetFlag(PRF_PREFER_REAR_SEATS);
	UnsetFlag(PRF_PREFER_FRONT_PASSENGER_SEAT);
}

/*
void CPlayerPedData::AllocateData()
{
	if(m_Wanted == NULL)
		m_Wanted = rage_new CWanted();
	m_Wanted->Initialise();

//RAGE	if(m_pClothes == NULL)
//		m_pClothes = new CPedClothesDesc;
//	m_pClothes->Initialise();
}

void CPlayerPedData::DeAllocateData()
{
	if (m_Wanted)
	{
		delete m_Wanted;
	}
	m_Wanted = NULL;

//RAGE	if (m_pClothes)
//	{
//		delete m_pClothes;
//	}
//	m_pClothes = NULL;
}
*/

// Static member initialisation
CPlayerInfo::Tunables CPlayerInfo::ms_Tunables;	
IMPLEMENT_PLAYER_INFO_TUNABLES(CPlayerInfo, 0x3ef71716);

void CPlayerInfo::ResetSprintEnergy()
{
	m_fSprintEnergy = GetPlayerDataMaxSprintEnergy();	//CStatsMgr::GetFatAndMuscleModifier(STAT_MODIFIER_SPRINT_ENERGY);
}

void CPlayerInfo::RestoreSprintEnergy(float fPercent)
{
	if ( aiVerify(fPercent >= 0.f && fPercent <= 100.f) )
	{
		const float fMaxSprintEnergy = GetPlayerDataMaxSprintEnergy();
		m_fSprintEnergy += fMaxSprintEnergy * fPercent;
		if (m_fSprintEnergy >= fMaxSprintEnergy)
		{
			m_fSprintEnergy = fMaxSprintEnergy;
		}
	}
}


bool CPlayerInfo::HandleSprintEnergy(bool bSprint, float fRate)
{
	if (m_pPlayerPed && m_pPlayerPed->GetUsingRagdoll())
		return true;

	const float fTimeStep = fwTimer::GetTimeStep();
	if(bSprint)
	{
		const CPlayerSpecialAbility* pAbility = m_pPlayerPed ? m_pPlayerPed->GetSpecialAbility() : NULL;
		bool bHasRageAbilityEquipped = pAbility && pAbility->GetType() == SAT_RAGE && pAbility->IsActive();
		bool bHasSprintSpeedBoostAbilityEquipped = pAbility && pAbility->GetType() == SAT_SPRINT_SPEED_BOOST && pAbility->IsActive();
		const bool bDoesNotGetTiredResetFlag = m_pPlayerPed ? m_pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_DontUseSprintEnergy) : false;

		// Sprint indefinitely if full stamina stat in B*983326
		if(bDoesNotGetTiredResetFlag || bDoesNotGetTired || fRate==0.0f || bHasRageAbilityEquipped || bHasSprintSpeedBoostAbilityEquipped
			|| StatsInterface::GetIntStat(STAT_STAMINA.GetStatId()) == 100 || GetArcadeAbilityEffects().GetIsActive(AAE_MARATHON_MAN)
#if !__FINAL
		   || CPlayerInfo::ms_bDebugPlayerInfiniteStamina
#endif // !__FINAL
		   )
		{
			// CNC speed boost ability: instantly replenish sprint energy.
			if (bHasSprintSpeedBoostAbilityEquipped)
			{
				m_fSprintEnergy = GetPlayerDataMaxSprintEnergy();
			}

			// can sprint but doesn't use any energy
			return true;
		}
		else if (m_fSprintEnergy > 0.0f)
		{
			m_fTimeBeenSprinting += fTimeStep;
			m_fSprintEnergy = rage::Max(0.0f, m_fSprintEnergy - fRate * fTimeStep);
			return true;
		}
	}
	else 
	{
		// Increase sprint energy
		const float fMaxSprintEnergy = GetPlayerDataMaxSprintEnergy();
		if (m_fSprintEnergy < fMaxSprintEnergy)
		{
			m_fTimeBeenSprinting = 0.0f;

			const bool bOnBicyle = m_pPlayerPed ? (m_pPlayerPed->GetMyVehicle() && m_pPlayerPed->GetMyVehicle()->InheritsFromBicycle()) : false;
			float fReplenishRateMultiplier = bOnBicyle ? ms_Tunables.m_SprintReplenishRateMultiplierBike : ms_Tunables.m_SprintReplenishRateMultiplier;

			// CNC: Increase replenish rate slightly.
			if (CPlayerInfo::AreCNCResponsivenessChangesEnabled(m_pPlayerPed))
			{
				fReplenishRateMultiplier *= ms_fStaminaDepletionRateModifierCNC;
			}

			m_fSprintEnergy += fRate * fTimeStep * fReplenishRateMultiplier;
			m_fSprintEnergy = rage::Clamp(m_fSprintEnergy, 0.0f, fMaxSprintEnergy);
		}
	}

	return false;
}

// Accessor function for the stealth multiplier, which is different depending on the state of the player.
float CPlayerInfo::GetStealthMultiplier() const
{
	const CPed* pPed = GetPlayerPed();

	if (Verifyf(pPed, "No player ped in the player info!"))
	{
		return pPed->GetMotionData()->GetUsingStealth() ? GetSneakingStealthMultiplier() : GetNormalStealthMultiplier();
	}
	else
	{
		return GetNormalStealthMultiplier();
	}
}

float PLAYER_SPRINT_THRESHOLD = 5.0f;

////////////////////////////////////////////////////////////////////////////////

float CPlayerInfo::GetDefaultHoldBreathDurationForPed(const CPed& UNUSED_PARAM(ped)) const
{
	return 0.0f;
}

////////////////////////////////////////////////////////////////////////////////

float CPlayerInfo::ComputeSprintDepletionRateForPed(const CPed& ped, float fMaxSprintEnergy, eSprintType nSprintType) const
{
	float fSprintDuration = 10.0f;
	float fMinSprintDuration = 10.0f;
	float fMaxSprintDuration = 10.0f;
	float fSprintStat = 0.0f;

	if (GetStatValuesForSprintType(ped, fSprintStat, fMinSprintDuration, fMaxSprintDuration, nSprintType))
	{
		fSprintDuration = ComputeDurationFromStat(fSprintStat, fMinSprintDuration, fMaxSprintDuration);
	}
	
	float fDepletionRate = fMaxSprintEnergy / fSprintDuration;
	const CPlayerSpecialAbility* pSpecialAbility = ped.GetSpecialAbility();

	if (pSpecialAbility && pSpecialAbility->IsActive())
	{
		if (pSpecialAbility->GetStaminaMultiplier() > 0.0f)
		{
			controlDebugf1("Adjusting sprint energy depletion rate for ped: fDepletionRate (%f) * fStaminaMultiplier (%f)", fDepletionRate, pSpecialAbility->GetStaminaMultiplier());

			fDepletionRate *= pSpecialAbility->GetStaminaMultiplier();
		}
	}

	if (CPlayerInfo::AreCNCResponsivenessChangesEnabled(m_pPlayerPed))
	{
		fDepletionRate *= ms_fStaminaReplenishRateModifierCNC;
		controlDebugf3("Adjusting sprint energy depletion rate for ped (CNC Multiplier): fDepletionRate (%f) * ms_fStaminaReplenishRateModifierCNC (%f)", fDepletionRate, ms_fStaminaReplenishRateModifierCNC);
	}

	return fDepletionRate;
}

////////////////////////////////////////////////////////////////////////////////

bool CPlayerInfo::ProcessCanSprint(CPed& ped, const bool bCheckDisabled)
{
	CVehicle* pVeh = ped.GetVehiclePedInside();
	const bool bOnBicycle = pVeh && pVeh->GetVehicleType()==VEHICLE_TYPE_BICYCLE;
	const float fReplenishFinishedPercentage = bOnBicycle ? ms_Tunables.m_SprintReplenishFinishedPercentageBicycle : ms_Tunables.m_SprintReplenishFinishedPercentage;
	// If we're currently replenishing our sprint energy and we've recovered more then the required percentage to stop replenishing
	// then do so (and allow sprinting again)
	if (m_bSprintReplenishing && m_fSprintEnergy > m_fMaxSprintEnergy * fReplenishFinishedPercentage)
	{
		m_bSprintReplenishing = false;
	}

	bool bInFpsMode = false;
#if FPS_MODE_SUPPORTED
	bInFpsMode = ped.IsFirstPersonShooterModeEnabledForPlayer(false);
#endif // FPS_MODE_SUPPORTED

	// Sprinting can be disabled externally (via script for instance)
	if (m_bPlayerSprintDisabled && bCheckDisabled)
	{
		return false;
	}
	// Can disable sprinting if in an interior, don't do this if on a bicycle B*2014189
	else if (!ped.GetPedConfigFlag(CPED_CONFIG_FLAG_IgnoreInteriorCheckForSprinting) && !bOnBicycle && CTaskMovePlayer::ms_bDefaultNoSprintingInInteriors && ped.m_nFlags.bInMloRoom && !ped.GetPortalTracker()->IsAllowedToRunInInterior() 	// not generally allowed to sprint in interiors
#if FPS_MODE_SUPPORTED
		&& (!bInFpsMode || !CTaskPlayerOnFoot::sm_Tunables.m_AllowFPSAnalogStickRunInInteriors)
#endif // FPS_MODE_SUPPORTED
		)
	{
		return false;
	}

#if FPS_MODE_SUPPORTED
	//! Disable sprinting if aiming in 1st person.
	if(bInFpsMode)
	{
		if(IsAiming() || IsFiring())
		{
			return false;
		}

		if(ped.GetPedResetFlag(CPED_RESET_FLAG_IsReloading))
		{
			return false;
		}
	}
#endif

	// CNC speed boost ability: skip energy checks if ability is active.
	const CPlayerSpecialAbility* pAbility = m_pPlayerPed ? m_pPlayerPed->GetSpecialAbility() : NULL;
	bool bHasSprintSpeedBoostAbilityEquipped = pAbility && pAbility->GetType() == SAT_SPRINT_SPEED_BOOST && pAbility->IsActive();

	if (!bDoesNotGetTired && !ped.GetPedResetFlag(CPED_RESET_FLAG_DisableSprintDamage) && !bHasSprintSpeedBoostAbilityEquipped) 
	{
		TUNE_GROUP_BOOL(SPRINT_TUNE, NO_DAMAGE_AND_NO_SPRINT_IN_MP, true);
		if (NO_DAMAGE_AND_NO_SPRINT_IN_MP && NetworkInterface::IsGameInProgress())
		{
			return !m_bSprintReplenishing;
		}

		if (m_fSprintApplyDamageTimer > 0.0f)
		{
			m_fSprintApplyDamageTimer -= fwTimer::GetTimeStep();
		}
		else if (m_fSprintEnergy <=0.0f && !ped.GetUsingRagdoll())
		{
			static dev_float s_fOverSprintDamage = 2.5f;
			static dev_float s_fOverSprintDamageInterval = 0.75f;
			TUNE_GROUP_FLOAT(SPRINT_DEBUG, fExhaustionNmHealthThreshold, 8.0f, 0.0f, 100.0f, 0.001f);
			if ((ped.GetHealth()-s_fOverSprintDamage)>(ped.GetInjuredHealthThreshold()+fExhaustionNmHealthThreshold))
			{
				//Apply health damage B* 988313
				u32 nWeaponUsedHash = WEAPONTYPE_EXHAUSTION; 
				m_fSprintApplyDamageTimer = s_fOverSprintDamageInterval;
				CPedDamageCalculator damageCalculator(NULL,s_fOverSprintDamage,nWeaponUsedHash, 0, false);
				CEventDamage event(NULL, fwTimer::GetTimeInMilliseconds(), nWeaponUsedHash);
				damageCalculator.ApplyDamageAndComputeResponse(&ped, event.GetDamageResponseData(), CPedDamageCalculator::DF_None);
				bool bWasKilledOrInjured = (event.GetDamageResponseData().m_bKilled || event.GetDamageResponseData().m_bInjured);
				if( bWasKilledOrInjured )
				{
					ped.GetPedIntelligence()->AddEvent(event);
				}		
			}
			else if (CTaskNMBehaviour::CanUseRagdoll(&ped, RAGDOLL_TRIGGER_FALL))
			{
				TUNE_GROUP_INT(SPRINT_DEBUG, iMinExhaustionNmTime, 3000, 0, 10000, 1);
				if (bOnBicycle && pVeh->GetDriver()==&ped)
				{
					// do the jump roll from road vehicle behaviour
					CTaskNMJumpRollFromRoadVehicle* pRollTask = rage_new CTaskNMJumpRollFromRoadVehicle(iMinExhaustionNmTime, 10000, false, false, atHashString::Null(), pVeh);
					CEventSwitch2NM event(10000, pRollTask,false, iMinExhaustionNmTime);
					ped.GetPedIntelligence()->AddEvent(event);

					// Let the ragdoll damage system know that it should go easy on the damage for a bit.
					ped.SetPedConfigFlag(CPED_CONFIG_FLAG_ThrownFromVehicleDueToExhaustion, true);
					ped.SetExhaustionDamageTimer(fwTimer::GetTimeInMilliseconds());
				}
				else
				{
					// do the ragdoll exhaustion nm behaviour
					CTaskNMHighFall* pFallTask = rage_new CTaskNMHighFall(iMinExhaustionNmTime, NULL, CTaskNMHighFall::HIGHFALL_SPRINT_EXHAUSTED, NULL, true);			
					CEventSwitch2NM event(10000, pFallTask,false, iMinExhaustionNmTime);
					ped.GetPedIntelligence()->AddEvent(event);
				}

				TUNE_GROUP_FLOAT(SPRINT_DEBUG, fExhaustionNmEnergyRecoverMult, 0.01f, 0.0f, 1.0f, 0.001f);
				// restore a little sprint energy so the nm behaviour doesn't get immediately kicked off again
				m_fSprintEnergy = GetPlayerDataMaxSprintEnergy()*fExhaustionNmEnergyRecoverMult;
			}
				
		}
		return true;
	}

	// Don't yet have the minimum energy to sprint
// 	else if (m_bSprintReplenishing)
// 	{
// 		return false;
// 	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////


float CPlayerInfo::ProcessSprintControl(const CControl& control, const CPed& FPS_MODE_SUPPORTED_ONLY(ped), eSprintType nSprintType, bool bUseButtonBashing)
{
	bool bIsSprintButtonPressed;
	bool bIsSprintButtonDown;

#if FPS_MODE_SUPPORTED
	bool bFirstPersonRunToggleInsideStickLimits = true;
#endif

	bool bUsingKeyboardAndMouse = false;
#if KEYBOARD_MOUSE_SUPPORT
	bUsingKeyboardAndMouse = control.WasKeyboardMouseLastKnownSource();
#endif	// KEYBOARD_MOUSE_SUPPORT

	// B*2259601: Allow sprint breakout to strafe if player is using an FPS control scheme (not just if scheme uses ToggleRun).
	bool bUsingFPSControlLayoutInFirstPerson = ped.IsFirstPersonShooterModeEnabledForPlayer(false) && camFirstPersonShooterCamera::WhenSprintingUseRightStick(&ped, control);

	if (nSprintType == SPRINT_ON_BICYCLE)
	{
#if KEYBOARD_MOUSE_SUPPORT
		if(bUsingKeyboardAndMouse)
		{
			bIsSprintButtonDown = control.GetVehiclePushbikeSprintIsDown();
			bIsSprintButtonPressed = false;

			// B*2254606: Simulate a sprint press every 150ms so sprint/energy behaves like button bashing.
			static dev_u32 uTimeBetweenPresses = 150;
			if (bIsSprintButtonDown && (fwTimer::GetTimeInMilliseconds() > m_uTimeBikeSprintPressed + uTimeBetweenPresses))
			{
				m_uTimeBikeSprintPressed = fwTimer::GetTimeInMilliseconds();
				bIsSprintButtonPressed = true;
			}
		}
		else
#endif // KEYBOARD_MOUSE_SUPPORT
		{
			bIsSprintButtonPressed = control.GetVehiclePushbikePedal().IsPressed();
			bIsSprintButtonDown = control.GetVehiclePushbikePedal().IsDown();
		}
	}
	else if(control.CanUseToggleRun() || bUsingFPSControlLayoutInFirstPerson)
	{
		// The keyboard controls works differently to the controller for sprinting. Tapping the button toggles run/walk whilst holding sprints.
		// use use CControl::KEYBOARD_START_SPRINT_HELD_TIME to distinguish when we should sprint.
		bIsSprintButtonPressed = control.GetValue(INPUT_SPRINT).IsPressed();

#if FPS_MODE_SUPPORTED
		if(ped.IsFirstPersonShooterModeEnabledForPlayer(false) && !ped.GetMotionData()->GetUsingStealth())
		{
			static dev_float s_fSprintThreshold = 0.75f;
			float fStickY = -control.GetPedWalkUpDown().GetNorm();
			bFirstPersonRunToggleInsideStickLimits = fStickY > s_fSprintThreshold;

#if KEYBOARD_MOUSE_SUPPORT
			// B*2259601: PC: Clear sprint if player is trying to strafe.
			if (bUsingKeyboardAndMouse)
			{
				static dev_u32 uDurationReleased = 100;
				bool bPressingStrafeLeft = control.GetValue(INPUT_MOVE_LEFT_ONLY).IsPressed() || control.GetValue(INPUT_MOVE_LEFT_ONLY).IsDown() || control.GetValue(INPUT_MOVE_LEFT_ONLY).HistoryReleased(uDurationReleased);
				bool bPressingStrafeRight = control.GetValue(INPUT_MOVE_RIGHT_ONLY).IsPressed() || control.GetValue(INPUT_MOVE_RIGHT_ONLY).IsDown() || control.GetValue(INPUT_MOVE_RIGHT_ONLY).HistoryReleased(uDurationReleased);
				bFirstPersonRunToggleInsideStickLimits = !bPressingStrafeLeft && !bPressingStrafeRight;
			}
#endif	//KEYBOARD_MOUSE_SUPPORT

			// B*2312353: PC FPS: Check input is actually down (and has been down for 0.1ms) for sprinting with mouse/keyboard (ignore the "m_ToggleRunOn" value). Allows us to toggle between run/walk without flickering to sprint for a few frames.
			static dev_u32 fSprintWasDownTime = 100;
			bool bSprintInputDown = bUsingKeyboardAndMouse ? (control.GetPedSprint().IsDown() && control.GetPedSprintHistoryHeldDown(fSprintWasDownTime)) : control.GetPedSprintIsDown();
			bIsSprintButtonDown = bSprintInputDown && bFirstPersonRunToggleInsideStickLimits;
		}
		else
#endif
		{
			bIsSprintButtonDown = control.GetValue(INPUT_SPRINT).HistoryHeldDown(CControl::KEYBOARD_START_SPRINT_HELD_TIME);
		}
	}
	else
	{
		// Use the inputs directly here as we are interested in the state of the actual buttons not the toggle state!
		bIsSprintButtonPressed = control.GetValue(INPUT_SPRINT).IsPressed();
		bIsSprintButtonDown = control.GetValue(INPUT_SPRINT).IsDown();
	}

	const sSprintControlData& rSprintControlData = ms_Tunables.m_SprintControlData[nSprintType];

	// If we allow button bashing to control sprint behavior, fill or reduce the sprint meter depending on what the player is doing
	// On PC we do not use button mashing for the ped due to the placement of keys on the keyboard.
	if ( (bUseButtonBashing && !control.CanUseToggleRun() && bFirstPersonRunToggleInsideStickLimits && !bUsingKeyboardAndMouse) || (nSprintType == SPRINT_ON_BICYCLE))
	{
		// Just tapped sprint button, add to meter
		if (bIsSprintButtonPressed)
		{
			m_fSprintControlCounter = rage::Min(rSprintControlData.m_MaxLimit, m_fSprintControlCounter + rSprintControlData.m_TapAdd);
		}
		// Holding sprint button down, reduce slightly
		else if (bIsSprintButtonDown)
		{
			m_fSprintControlCounter = rage::Max(1.0f, m_fSprintControlCounter - rSprintControlData.m_HoldSub * fwTimer::GetTimeStep());
		}
		// Released sprint button, reduce if not empty
		else if (m_fSprintControlCounter > 0.0f)
		{
			m_fSprintControlCounter = rage::Max(0.0f, m_fSprintControlCounter - rSprintControlData.m_ReleaseSub * fwTimer::GetTimeStep());
		}
	}
	else
	{
		// Not using bashing, just set to max, no finger pain
		m_fSprintControlCounter = bIsSprintButtonDown ? rSprintControlData.m_MaxLimit : 0.0f;
	}

#if FPS_MODE_SUPPORTED
	const bool bWeaponWheelIsActive = CNewHud::IsWeaponWheelActive();
	if( ped.IsFirstPersonShooterModeEnabledForPlayer(false)	&&
		nSprintType != SPRINT_ON_BICYCLE					&&
		!ped.GetMotionData()->GetUsingStealth()				&&
		((!control.CanUseToggleRun() && !bUsingFPSControlLayoutInFirstPerson) || bFirstPersonRunToggleInsideStickLimits) )
	{
		TUNE_GROUP_INT(FIRST_PERSON_TUNE, SPRINT_AIM_DELAY, 100, 0, 1000, 100);
		bool bForceMaxLimit = control.GetValue(INPUT_SPRINT).HistoryHeldDown( SPRINT_AIM_DELAY )	|| 
									(control.GetValue(INPUT_SPRINT).HistoryPressed( SPRINT_AIM_DELAY ) && !bUsingKeyboardAndMouse)	|| 
									( bWeaponWheelIsActive && CPedMotionData::GetIsSprinting(ped.GetMotionData()->GetCurrentMbrY()) );

		// B*2083102: Don't force max sprint limits if weapon wheel is open and sprint button isn't down/pressed (when not using Standard FPS controls, ie toggle run).
		if (bForceMaxLimit && bWeaponWheelIsActive && !control.CanUseToggleRun() && !bIsSprintButtonDown && !bIsSprintButtonPressed)
		{
			bForceMaxLimit = false;
		}

		if(bForceMaxLimit)
		{
			// Even though button bashing is enabled, allow holding sprint button to sprint also.
			// TODO: should this be done for bicycles as well?
			m_fSprintControlCounter = rSprintControlData.m_MaxLimit;
		}
	}
#endif // FPS_MODE_SUPPORTED

	// Determine if we want to sprint based on the threshold for the sprint meter
	float fSprintMult = 0.0f;
	if (m_fSprintControlCounter > 0.0f)
	{
		fSprintMult = m_fSprintControlCounter / rSprintControlData.m_Threshhold;
	}
	return fSprintMult;
}

////////////////////////////////////////////////////////////////////////////////

float CPlayerInfo::ControlButtonSprint(eSprintType nSprintType, bool bUseButtonBashing)
{
	CPed& playerPed = *GetPlayerPed();
	CControl* pControl = playerPed.GetControlFromPlayer();

	// No control, no sprinting
	if (!pControl)
		return 0.0f;

	const bool bOnBicycle = nSprintType == SPRINT_ON_BICYCLE;

	if (bOnBicycle && playerPed.GetMyVehicle() && playerPed.GetPedResetFlag(CPED_RESET_FLAG_IsDoingDriveby))
	{
		const CVehicleDriveByInfo* pVehicleDriveByInfo = CVehicleMetadataMgr::GetVehicleDriveByInfoFromPed(&playerPed);
		if (!pVehicleDriveByInfo || !pVehicleDriveByInfo->GetUseSpineAdditive())
		{
			return 0.0f;
		}
	}

	// Called this function already this frame, just return previously computed value
	if (m_fCachedSprintMultThisFrame > -1.0f)
	{
		return m_fCachedSprintMultThisFrame;
	}

	// Determine if we can sprint
	const bool bCanSprint = ProcessCanSprint(playerPed);

	static dev_float s_fClearToggleRunThreshold = 0.75f;
	Vector3 vStickInput(pControl->GetPedWalkLeftRight().GetNorm(), -pControl->GetPedWalkUpDown().GetNorm(), 0.0f);

#if KEYBOARD_MOUSE_SUPPORT
	if (pControl->WasKeyboardMouseLastKnownSource() && pControl->GetPedWalkLeftRight().GetNorm() == 0.0f)
	{
		float fLeftNorm = Abs(pControl->GetValue(INPUT_MOVE_LEFT_ONLY).GetNorm());
		float fRightNorm = Abs(pControl->GetValue(INPUT_MOVE_RIGHT_ONLY).GetNorm());
		vStickInput.x = Max(fLeftNorm, fRightNorm);
	}
#endif	// KEYBOARD_MOUSE_SUPPORT

	if(vStickInput.Mag2() < s_fClearToggleRunThreshold)
	{
		pControl->ClearToggleRun();
	}

	// Determine if we want to sprint (return a value greater than one in this case)
	float fSprintMult = ProcessSprintControl(*pControl, playerPed, nSprintType, bUseButtonBashing);

	const sSprintControlData& rSprintControlData = ms_Tunables.m_SprintControlData[nSprintType];
	
	bool bIsSprintButtonDown;
 	if (bOnBicycle)
	{
		bIsSprintButtonDown = pControl->GetVehiclePushbikePedal().IsDown();
	}
#if KEYBOARD_MOUSE_SUPPORT
	else if(playerPed.IsUsingStealthMode() && pControl->WasKeyboardMouseLastKnownSource())
	{
		// On the keyboard and mouse we need to take into account the sprint toggle state.
		bIsSprintButtonDown = pControl->GetPedSprintIsDown();

		// On the keyboard, INPUT_SPRINT is held to sprint but pressed to toggle run/walk. If the button
		// is down for less than CControl::KEYBOARD_START_SPRINT_HELD_TIME then reset fSprintMult otherwise it can kick us out of stealth mode.
		// url:bugstar:1818710.
		if(pControl->GetValue(INPUT_SPRINT).HistoryHeldDown(CControl::KEYBOARD_START_SPRINT_HELD_TIME) == false)
		{
			fSprintMult = 0.0f;
		}
	}
#endif // KEYBOARD_MOUSE_SUPPORT
 	else
	{
		bIsSprintButtonDown = pControl->GetPedSprintIsDown();
	}

	const float fUseUpSprintEnergyThreshold = 1.0f;

	// We want to sprint and are allowed to
	if (fSprintMult > fUseUpSprintEnergyThreshold && bCanSprint)
	{
		const bool bShouldUseUpSprintEnergy = !bOnBicycle || !playerPed.GetPedResetFlag(CPED_RESET_FLAG_IsExitingVehicle);

		// Reduce energy when on bicycle quicker if mashing
		float fDepletionMultiplier = 1.0f;
		if (bOnBicycle)
		{
			fDepletionMultiplier = ms_Tunables.m_BicycleDepletionMinMult;

			if (fSprintMult >= ms_Tunables.m_BicycleMaxDepletionLimit)
			{
				fDepletionMultiplier = ms_Tunables.m_BicycleDepletionMaxMult;
			}
			else if (fSprintMult >= ms_Tunables.m_BicycleMidDepletionLimit && fSprintMult > ms_Tunables.m_BicycleMinDepletionLimit)
			{
				TUNE_GROUP_FLOAT(BICYCLE_TUNE, POW_EXP, 4.0f, 0.0f, 10.0f, 0.01f);
				const float fRangedNorm = (fSprintMult - ms_Tunables.m_BicycleMidDepletionLimit) / (ms_Tunables.m_BicycleMaxDepletionLimit - ms_Tunables.m_BicycleMidDepletionLimit);
				const float fMult = rage::Powf(fRangedNorm, POW_EXP);
				fDepletionMultiplier = ms_Tunables.m_BicycleDepletionMidMult + fMult * (ms_Tunables.m_BicycleDepletionMaxMult - ms_Tunables.m_BicycleDepletionMidMult);
			}
		}

		// Trying to sprint so use energy up
		if (bShouldUseUpSprintEnergy && HandleSprintEnergy(true, fDepletionMultiplier * ComputeSprintDepletionRateForPed(*m_pPlayerPed, GetPlayerDataMaxSprintEnergy(), nSprintType)))
        {
			fSprintMult = 1.0f + rSprintControlData.m_ResultMult*rage::Max(0.0f, fSprintMult-1.0f);
        }
		// Ran out of energy so we need to replenish a bit before we can sprint again
		else if (bShouldUseUpSprintEnergy)
		{
			m_bSprintReplenishing = true;
		}
	}
	// Don't want to or unable to sprint but holding sprint button, keep the sprint meter just below the threshold if we're not replenishing
	else if (bIsSprintButtonDown)
	{
		fSprintMult = 1.0f;
		TUNE_GROUP_BOOL(SPRINT_TUNE, ENABLE_SPRINT_IF_ANY_ENERGY, true);
		if ((ENABLE_SPRINT_IF_ANY_ENERGY && m_fSprintEnergy > 0.0f) || (m_fSprintEnergy > m_fMaxSprintEnergy * ms_Tunables.m_SprintReplenishFinishedPercentage))
		{
			m_fSprintControlCounter = rSprintControlData.m_Threshhold - 0.01f;
		}
	}
	// If not pressing sprint and we're currently sprinting, stop sprinting
	else if (fSprintMult > 1.0f) 
	{
		fSprintMult = 1.0f;
	}

	// Cache the sprint multiplier in case we call this again this frame
	m_fCachedSprintMultThisFrame = fSprintMult;
	return m_fCachedSprintMultThisFrame;
}

////////////////////////////////////////////////////////////////////////////////

float CPlayerInfo::GetButtonSprintResults(eSprintType nSprintType)
{
	if(m_fSprintControlCounter > PLAYER_SPRINT_THRESHOLD)
		return 1.0f + ms_Tunables.m_SprintControlData[nSprintType].m_ResultMult*rage::Max(0.0f, (m_fSprintControlCounter / PLAYER_SPRINT_THRESHOLD) - 1.0f);
	else if(m_fSprintControlCounter > 0.0f)
		return 1.0f;

	return 0.0f;		
}

////////////////////////////////////////////////////////////////////////////////

void CPlayerInfo::ControlButtonDuck()
{	
	CPed& playerPed = *GetPlayerPed();
	const CControl* pControl = playerPed.GetControlFromPlayer();

	if(pControl && pControl->GetPedDuck().IsPressed())
	{
		if( CGameConfig::Get().AllowStealthMovement())
		{
			if( playerPed.GetMotionData()->GetUsingStealth() )
			{
				playerPed.GetMotionData()->SetUsingStealth( false );
			}
			else if( playerPed.CanPedStealth() )
			{
				playerPed.GetMotionData()->SetUsingStealth( true );
			}
		}
		else if(CGameConfig::Get().AllowCrouchedMovement())
		{
			// Allow the ped to uncrouch when in stealth..
			if(playerPed.GetIsCrouching())
			{
				playerPed.SetIsCrouching(false);
			}
			// ..but not to crouch
			else if (playerPed.CanPedCrouch() && !playerPed.GetMotionData()->GetUsingStealth())
			{
				playerPed.SetIsCrouching(true);
			}
		}
	}
}

//mjc - potentially push down to CPed ?
bool CPlayerInfo::IsPlayerPressingHorn() const
{
	const CPed* pPlayerPed = GetPlayerPed();
	if(!pPlayerPed)
		return false;

	if(pPlayerPed->IsNetworkClone())
	{
		if(pPlayerPed->GetIsDrivingVehicle())
		{
			CNetObjVehicle* pNetObjVehicle = static_cast<CNetObjVehicle*>(pPlayerPed->GetMyVehicle()->GetNetworkObject());
			return (pNetObjVehicle && pNetObjVehicle->IsHornOnFromNetwork());
		}
		return false;
	}
	else
	{
		return pPlayerPed->GetIsDrivingVehicle() && CControlMgr::GetMainPlayerControl().GetVehicleHorn().IsDown();
	}
}

bool CPlayerInfo::AffectedByWaterCannon()
{
	if(m_fHitByWaterCannonImmuneTimer > 0.0f)
		return false;
	else
		return true;
}

void CPlayerInfo::RegisterHitByWaterCannon()
{
	// Add on cool down rate to cancel out constant decrement in ProcessControl
	m_fHitByWaterCannonTimer += fwTimer::GetTimeStep()*(1.0f + PLAYER_WATER_CANNON_COOLDOWN_RATE);

	if(m_fHitByWaterCannonTimer > PLAYER_MAX_TIME_HIT_BY_WATER_CANNON)
	{
		m_fHitByWaterCannonImmuneTimer = PLAYER_WATER_CANNON_IMMUNE_TIME;
		m_fHitByWaterCannonTimer = 0.0f;
	}
}


//mjc - into CPed ?
bool CPlayerInfo::IsHidden() const
{
#define HIDDEN_THRESHOLD (0.05f)
	return false;//(!GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && GetLightingTotal() <= HIDDEN_THRESHOLD);
}

float CPlayerInfo::GetFiringAttackValue()
{
	CPed * pPlayer = GetPlayerPed();

	if(pPlayer->GetPedConfigFlag(CPED_CONFIG_FLAG_PedIsArresting))
	{
		return false;
	}

	CControl* pControl = pPlayer->GetControlFromPlayer();

	// For network synced players.
	if( !pControl )
	{
		return false;
	}

	float fAttackValue = 0.0f;

	bool bInVehicle = false;

	// Held turrets use on foot aim / fire controls
	if(pPlayer->GetIsInVehicle() && !pPlayer->IsExitingVehicle() && !pPlayer->GetMyVehicle()->IsTurretSeat(pPlayer->GetMyVehicle()->GetSeatManager()->GetPedsSeatIndex(pPlayer)))
	{
		taskAssert(pPlayer->GetMyVehicle()->GetLayoutInfo());
		const bool bVehicleHasDriver = pPlayer->GetMyVehicle()->GetLayoutInfo()->GetHasDriver();

		if (bVehicleHasDriver)
		{
			bInVehicle = true;

			//Drive-bys have some really weird firing logic.
			const bool bIsDoingDriveBy = pPlayer->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_AIM_GUN_VEHICLE_DRIVE_BY, true);
			if(bIsDoingDriveBy)
			{
				bool bVehicleAim = pControl->IsVehicleAttackInputDown();

				if(bVehicleAim)
				{
					return true;
				}
				else
				{
					if (pPlayer->GetIsDrivingVehicle())
						return false;
				}
			}

			if( pPlayer->GetIsDrivingVehicle() && !pPlayer->GetMyVehicle()->InheritsFromTrailer())
			{
				const bool bSecondaryAttackButton = pPlayer->GetMyVehicle()->GetIsAircraft() || pPlayer->GetMyVehicle()->InheritsFromSubmarine() || (pPlayer->GetMyVehicle()->InheritsFromSubmarineCar() && pPlayer->GetMyVehicle()->IsInSubmarineMode());

				//Disable attach button turning the search light on and off, this is now on the headlight button
				bool bDisableAttackButton = false;
				const CVehicleWeaponMgr* pWeaponMgr = pPlayer->GetMyVehicle()->GetVehicleWeaponMgr();	
				if(pWeaponMgr)
				{
					if(pWeaponMgr->GetNumVehicleWeapons() == 1)
					{
						CVehicleWeapon* pWeapon = pWeaponMgr->GetVehicleWeapon(0);
						if(pWeapon)
						{
							if(pWeapon->GetType() == VGT_SEARCHLIGHT)
							{
								bDisableAttackButton = true;
							}
						}
					}
				}

				bool bUseVehicleAttackForFiring = false;
#if KEYBOARD_MOUSE_SUPPORT
				if(pControl->WasKeyboardMouseLastKnownSource() && (pPlayer->GetMyVehicle()->GetModelIndex() == MI_TANK_RHINO || (MI_TANK_KHANJALI.IsValid() && pPlayer->GetMyVehicle()->GetModelIndex() == MI_TANK_KHANJALI)))
				{
					bUseVehicleAttackForFiring = true;
				}
#endif // KEYBOARD_MOUSE_SUPPORT

				// Hack to use the cinematic camera input to fire torpedoes on the Kostka
				bool bIsKeyboardMouse = false;
#if KEYBOARD_MOUSE_SUPPORT
				bIsKeyboardMouse = pControl->WasKeyboardMouseLastKnownSource();
#endif // KEYBOARD_MOUSE_SUPPORT
				bool bIsKostaka = MI_SUB_KOSATKA.IsValid() && pPlayer->GetMyVehicle()->GetModelIndex() == MI_SUB_KOSATKA;
				bool bUseOtherInput = bIsKostaka && !bDisableAttackButton && !bIsKeyboardMouse;
				ioValue::ReadOptions readOptions;
				readOptions.SetFlags(ioValue::ReadOptions::F_READ_DISABLED, true);
				bool bIsOrbisJapaneseBuild = false;
#if RSG_ORBIS
				if(sysAppContent::IsJapaneseBuild())
				{
					bIsOrbisJapaneseBuild = true;
					if(sysAppContent::IsJapaneseBuild() && !CPhoneMgr::IsDisplayed() && bUseOtherInput && pControl->GetCellphoneCancel().IsDown(ioValue::BUTTON_DOWN_THRESHOLD, readOptions))
					{
						fAttackValue = pControl->GetCellphoneCancel().GetNorm(readOptions);
					}
				}
#endif // RSG_ORBIS

				if(!bIsOrbisJapaneseBuild && bUseOtherInput && pControl->GetVehicleCinematicCam().IsDown(ioValue::BUTTON_DOWN_THRESHOLD, readOptions))
				{
					fAttackValue = pControl->GetVehicleCinematicCam().GetNorm(readOptions);
				}

				//Using FlyAttack input for sub/subcar as we would need to patch the various commands meta files (ie settings.meta, standard.meta etc)
				bool bVehicleAttack = bSecondaryAttackButton ? pControl->GetVehicleFlyAttack().IsDown() : (bUseVehicleAttackForFiring ? pControl->GetVehicleAttack().IsDown() : pControl->GetVehicleAim().IsDown());
				if (bVehicleAttack && !bDisableAttackButton && !bUseOtherInput) 
				{
					float fAttackNorm = bUseVehicleAttackForFiring ? pControl->GetVehicleAttack().GetNorm() : pControl->GetVehicleAim().GetNorm();
					fAttackValue = MAX(fAttackNorm, pControl->GetVehicleFlyAttack().GetNorm());
				}
			}
			else if(pPlayer->GetMyVehicle()->GetModelIndex() == MI_VAN_RIOT_2 && pPlayer->GetWeaponManager() && pPlayer->GetWeaponManager()->GetEquippedVehicleWeaponIndex() != -1 && pControl->GetVehicleAim().IsDown()) 			
			{
				//B*4048108: Passengers using the water cannon of the RIOT2 should also allow L1 (INPUT_VEHICLE_AIM)
				fAttackValue = pControl->GetVehicleAim().GetNorm();
			}
			else if(pControl->IsVehicleAttackInputDown())
			{
				fAttackValue = pControl->GetVehicleAttackInputNorm();
			}
			else
			{				
				fAttackValue = pControl->GetVehiclePassengerAttack().GetNorm(); 
			}
		}
	}

	if (!bInVehicle)
	{
		fAttackValue = MAX(pControl->GetPedAttack2().GetNorm(), pControl->GetPedAttack().GetNorm());
	}

	return fAttackValue;
}

float CPlayerInfo::GetFiringBoundaryValue()
{
#if FPS_MODE_SUPPORTED
	CPed *pPed = GetPlayerPed();
	if(pPed && pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		if(m_bUseHighFiringAttackBoundary)
		{
			return ms_PlayerFPSFireBoundaryHigh;
		}
		else
		{
			return ms_PlayerFPSFireBoundaryLow;
		}
	}
#endif

	return ms_PlayerSoftAimBoundary;
}

//-------------------------------------------------------------------------
// Returns true if the player is not holding the aim button fully
//-------------------------------------------------------------------------
bool CPlayerInfo::IsFiring()
{
	float fAttackValue = GetFiringAttackValue();
	float fBoundaryValue = GetFiringBoundaryValue();

	return fAttackValue >= fBoundaryValue;
}


bool CPlayerInfo::IsFiring_s()
{
	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if( !pPlayer ) 
	{ 
		return false; 
	}
	return pPlayer->GetPlayerInfo()->IsFiring();
}

bool CPlayerInfo::IsDriverFiring()
{
	const CPed * pPlayer = CGameWorld::FindLocalPlayer();
	if( !pPlayer ) { return false; }

	if (pPlayer->GetIsDrivingVehicle())
	{
		const CControl* pControl = pPlayer->GetControlFromPlayer();
		return (pControl->GetVehicleAim().IsDown() && pControl->IsVehicleAttackInputDown());
	}
	return false;
}

bool CPlayerInfo::IsFlyingAircraft()
{
	const CPed * pPlayer = CGameWorld::FindLocalPlayer();
	if( !pPlayer ) { return false; }

	if (pPlayer->GetIsDrivingVehicle())
	{
		return pPlayer->GetMyVehicle()->GetIsAircraft();
	}
	return false;
}

bool CPlayerInfo::IsSimulatingSoftFiring(const CPed * pPlayer)
{
	FatalAssert(pPlayer);
	bool bSpecialAbilityForcingSoftAim = !NetworkInterface::IsGameInProgress() && pPlayer->GetSpecialAbility() && pPlayer->GetSpecialAbility()->GetType() == SAT_BULLET_TIME && pPlayer->GetSpecialAbility()->IsActive();
	if(bSpecialAbilityForcingSoftAim)
	{
		return true;
	}

	return false;
}

bool CPlayerInfo::IsSoftFiring()
{
	const CPed * pPlayer = CGameWorld::FindLocalPlayer();
	if( !pPlayer ) { return false; }

	if(IsSimulatingSoftFiring(pPlayer))
	{
		return true;
	}
	
#if FPS_MODE_SUPPORTED
	TUNE_GROUP_BOOL(SOFT_FIRE_TUNE, DONT_SOFT_FIRE_IN_COVER, true);
	if(DONT_SOFT_FIRE_IN_COVER && pPlayer->GetIsInCover() && pPlayer->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		return false;
	}
#endif // FPS_MODE_SUPPORTED

	const CControl* pControl = pPlayer->GetControlFromPlayer();
	if( !pControl ) { return false; }

	const float fFireTestHighValue	= ms_PlayerSoftAimBoundary - 0.001f;
	const float fFireTestLowValue	= ms_PlayerLowAimBoundry;

	const float fAttackValue = MAX(pControl->GetPedAttack2().GetNorm(), pControl->GetPedAttack().GetNorm());

	return (fAttackValue > fFireTestLowValue && fAttackValue <= fFireTestHighValue);
}

bool CPlayerInfo::IsJustFiring()
{
	const CPed * pPlayer = CGameWorld::FindLocalPlayer();
	if( !pPlayer ) { return false; }

	return pPlayer->GetPlayerInfo()->IsFiring() && !pPlayer->GetPedConfigFlag(CPED_CONFIG_FLAG_WasFiring);
}

bool CPlayerInfo::CanPlayerWarpIntoVehicle(const CPed& rPed, const CVehicle& rVeh, s32 iTargetSeat)
{
	if (rPed.IsLocalPlayer())
	{
#if __BANK
		TUNE_GROUP_BOOL(PERSONAL_VEHICLE_DEBUG, FORCE_CAN_WARP_INTO_VEHICLE, false);
		if (FORCE_CAN_WARP_INTO_VEHICLE)
		{
			return true;
		}
#endif

		CPed* pPedOccupyingSeat = iTargetSeat >= 0 ? rVeh.GetSeatManager()->GetPedInSeat(iTargetSeat) : nullptr;
		const bool bTargetIsPlayer = pPedOccupyingSeat && pPedOccupyingSeat->IsPlayer();
		if (bTargetIsPlayer)
		{
			return false;
		}

		TUNE_GROUP_BOOL(PERSONAL_VEHICLE_DEBUG, PRETEND_PERSONAL, false);
		if (rVeh.InheritsFromBike())
		{
			return true;
		}
		else if (rVeh.PopTypeIsMission())
		{
			return true;
		}
		else if (CTaskVehicleFSM::VehicleContainsFriendlyPassenger(&rPed, &rVeh))
		{
			return true;
		}
		// Some of the decorator functions could do with some const-correctness
		else if (const_cast<CVehicle&>(rVeh).IsPersonalVehicle() || PRETEND_PERSONAL)
		{
			// B*512607 - Allow warping into personal vehicles
			return true;
		}
	}

	return false;
}

void CPlayerInfo::GetCachedMeleeInputs( bool& bWasLightAttackPressed, bool& bWasAlternateAttackPressed )
{
	CPed * pPlayer = CGameWorld::FindLocalPlayer();
	if( !pPlayer ) 
		return; 

	bWasLightAttackPressed = pPlayer->GetPlayerInfo()->WasLightAttackPressed();
	bWasAlternateAttackPressed = pPlayer->GetPlayerInfo()->WasAlternateAttackPressed();
}

//-------------------------------------------------------------------------
// Returns true if the player is not in a vehicle or isn't driving a vehicle
//-------------------------------------------------------------------------
bool CPlayerInfo::IsNotDrivingVehicle()
{
	CPed * pPlayer = CGameWorld::FindLocalPlayer();
	if( !pPlayer ) 
	{ 
		return false; 
	}

	if( pPlayer->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pPlayer->GetMyVehicle())
	{
		Assert(pPlayer->GetMyVehicle()->GetLayoutInfo());
		const bool bVehicleHasDriver = pPlayer->GetMyVehicle()->GetLayoutInfo()->GetHasDriver();

		if (bVehicleHasDriver && pPlayer->GetIsDrivingVehicle())
		{
			return false;
		}
	}

	return true;
}

//-------------------------------------------------------------------------
// Returns true if the player can use assisted aim.
//-------------------------------------------------------------------------
bool CPlayerInfo::CanUseAssistedAiming()
{
	if(!CPauseMenu::GetMenuPreference(PREF_CINEMATIC_SHOOTING))
	{
		return false;
	}

	if(NetworkInterface::IsGameInProgress())
	{
		return false;
	}

#if FPS_MODE_SUPPORTED
	const CPed* pPed = CGameWorld::FindLocalPlayer();
	if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false) || pPed->IsInFirstPersonVehicleCamera())
	{
		return false;
	}
#endif

	TUNE_GROUP_BOOL(PLAYER_TARGETING, bDisableCinematicAim, true)
	if(bDisableCinematicAim)
	{
		return false;
	}

	return true;

}

//-------------------------------------------------------------------------
// Returns true if the player is just aiming using the fire button and using cinematic mode
//-------------------------------------------------------------------------
bool CPlayerInfo::IsAssistedAiming()
{
	const CPed* pPed = CGameWorld::FindLocalPlayer();
	return CanUseAssistedAiming() && !pPed->GetPlayerInfo()->IsAiming(false) && (pPed->GetPlayerInfo()->IsSoftFiring() ||  pPed->GetPlayerInfo()->IsFiring());
}

//-------------------------------------------------------------------------
// Returns true if the player is just aiming using the fire button
//-------------------------------------------------------------------------
bool CPlayerInfo::IsRunAndGunning(bool bCanRunAndGunWhileAiming, bool bIgnoreSoftFiring)
{
	const CPed* pPed = CGameWorld::FindLocalPlayer();
	bool bAllowRunAndGunWhileAiming = bCanRunAndGunWhileAiming || !pPed->GetPlayerInfo()->IsAiming(false);
	return !CanUseAssistedAiming() && bAllowRunAndGunWhileAiming && ((pPed->GetPlayerInfo()->IsSoftFiring()&&!bIgnoreSoftFiring) ||  pPed->GetPlayerInfo()->IsFiring());
}

//-------------------------------------------------------------------------
// Returns true if the player is aiming at all
//-------------------------------------------------------------------------
bool CPlayerInfo::IsAiming(const bool bConsiderAttackTriggerAiming)
{
	return IsSoftAiming(bConsiderAttackTriggerAiming) || IsHardAiming() || IsForcedAiming() || IsSimulatingAiming();
}

//-------------------------------------------------------------------------
// Returns true if the player is not holding the aim button fully
//-------------------------------------------------------------------------
bool CPlayerInfo::IsSoftAiming(const bool bConsiderAttackTriggerAiming)
{
	const CPed * pPlayer = CGameWorld::FindLocalPlayer();
	if( !pPlayer ) { return false; }

	const CControl* pControl = pPlayer->GetControlFromPlayer();

	if (pPlayer->GetMyVehicle())
	{
		Assert(pPlayer->GetMyVehicle()->GetLayoutInfo());
		bool bVehicleHasDriver = pPlayer->GetMyVehicle()->GetLayoutInfo()->GetHasDriver();

		if (bVehicleHasDriver && pPlayer->GetIsDrivingVehicle() && !pPlayer->IsExitingVehicle())
		{			
			return false; //drivers cannot soft aim (for now)
		}
	}

	float fAimTestHighValue	= ms_PlayerSoftAimBoundary - 0.001f;
	float fAimTestLowValue	= ms_PlayerLowAimBoundry;

	if( ms_ReverseAimBoundarys )
	{
		fAimTestLowValue	= fAimTestHighValue;
		fAimTestHighValue	= 1.0f;
	}

	const float fTargetValue = pControl ? pControl->GetPedTargetNorm() : 0.f;

	return (fTargetValue > fAimTestLowValue && fTargetValue <= fAimTestHighValue) || (bConsiderAttackTriggerAiming && IsSoftFiring());
}

//-------------------------------------------------------------------------
// Returns true if the player is  holding the aim button fully
//-------------------------------------------------------------------------
bool CPlayerInfo::IsHardAiming()
{
	CPed * pPlayer = CGameWorld::FindLocalPlayer();
	if( !pPlayer ) { return false; }

	CControl* pControl = pPlayer->GetControlFromPlayer();
	if( !pControl ) { return false; }

	if (pPlayer->GetMyVehicle())
	{
		Assert(pPlayer->GetMyVehicle()->GetLayoutInfo());
		bool bVehicleHasDriver = pPlayer->GetMyVehicle()->GetLayoutInfo()->GetHasDriver();

		if(!pPlayer->IsExitingVehicle())
		{
			if (bVehicleHasDriver && pPlayer->GetIsDrivingVehicle() && !pPlayer->GetMyVehicle()->IsTurretSeat(pPlayer->GetMyVehicle()->GetSeatManager()->GetPedsSeatIndex(pPlayer)))
			{
				if(pControl->GetVehicleAim().IsDown() && !pPlayer->GetVehiclePedInside()->GetIsAircraft())
				{
					return true;
				}

	#if RSG_PC
				//B*1823615: Enable driver aiming in PC builds and if we're using mouse/keyboard controls
				if (pControl->WasKeyboardMouseLastKnownSource() && pControl->GetVehicleTargetDown())
				{
					return true;
				}
	#endif

				return false;
			}
			else if(pPlayer->GetVehiclePedInside())
			{
#if RSG_PC
				// PREF_ALTERNATE_DRIVEBY shouldn't change mouse.
				if (pControl->WasKeyboardMouseLastKnownSource() && pControl->GetVehicleAim().IsDown())
				{
					return true;
				}
#endif
				if(CPauseMenu::GetMenuPreference(PREF_ALTERNATE_DRIVEBY) && pControl->GetVehicleAim().IsDown())
				{
					return true;
				}
			}
		}
	}

	float fAimTestHighValue	= 1.0f;
	float fAimTestLowValue	= ms_PlayerSoftAimBoundary;
	if( ms_ReverseAimBoundarys )
	{
		fAimTestHighValue	= fAimTestLowValue;
		fAimTestLowValue	= 0.004f;
	}

	bool bAimEnabled = pControl->GetVehicleTarget().IsEnabled();

	return bAimEnabled
		&& pControl->GetPedTargetNorm() >= fAimTestLowValue 
		&& pControl->GetPedTargetNorm() <= fAimTestHighValue;
}

bool CPlayerInfo::IsForcedAiming()
{
	CPed * pPlayer = CGameWorld::FindLocalPlayer();
	if( !pPlayer ) { return false; }
	
	return pPlayer->GetPedConfigFlag( CPED_CONFIG_FLAG_ForcedAim ) || pPlayer->GetPedConfigFlag( CPED_CONFIG_FLAG_ForcedAimFromArrest );
}

bool CPlayerInfo::IsSimulatingAiming()
{
	CPed * pPlayer = CGameWorld::FindLocalPlayer();
	if( !pPlayer ) { return false; }

	return pPlayer->GetPedConfigFlag( CPED_CONFIG_FLAG_SimulatingAiming );
}
//-------------------------------------------------------------------------
// Returns true if the player is  holding the aim button fully
//-------------------------------------------------------------------------
bool CPlayerInfo::IsJustHardAiming()
{
	if( !IsHardAiming() )
	{
		return false;
	}

	// IF the player is hard aiming now, return true if they weren't last frame
	CPed * pPlayer = CGameWorld::FindLocalPlayer();
	if( !pPlayer ) { return false; }
	CControl* pControl = pPlayer->GetControlFromPlayer();
	if( !pControl ) { return false; }

	float fAimTestHighValue	= 1.0f;
	float fAimTestLowValue	= ms_PlayerSoftAimBoundary;
	if( ms_ReverseAimBoundarys )
	{
		fAimTestHighValue	= fAimTestLowValue;
		fAimTestLowValue	= 0.004f;
	}

	return pControl->GetPedTargetLastNorm() < fAimTestLowValue ||
		pControl->GetPedTargetLastNorm() > fAimTestHighValue;
}

bool CPlayerInfo::IsReloading()
{
	CPed * pPlayer = CGameWorld::FindLocalPlayer();
	if( !pPlayer ) { return false; }
	CControl* pControl = pPlayer->GetControlFromPlayer();

	if(pPlayer->GetPedConfigFlag(CPED_CONFIG_FLAG_ForceReload))
	{
		// Should reload due to MAKE_PED_RELOAD.
		return true;
	}

#if USE_SIXAXIS_GESTURES
	if(CControlMgr::GetPlayerPad() && CPadGestureMgr::GetMotionControlEnabled(CPadGestureMgr::MC_TYPE_RELOAD))
	{
		CPadGesture* gesture = CControlMgr::GetPlayerPad()->GetPadGesture();
		if(gesture && gesture->GetHasReloaded())
		{
			return true;
		}
	}
#endif // USE_SIXAXIS_GESTURES

	return pControl && pControl->GetPedReload().IsPressed();
}

bool CPlayerInfo::IsCombatRolling()
{
	CPed * pPlayer = CGameWorld::FindLocalPlayer();
	if( !pPlayer ) { return false; }

#if FPS_MODE_SUPPORTED
	// HACK: disable combat roll when in first person camera.
	TUNE_GROUP_BOOL(FIRST_PERSON_TUNE, COMBAT_ROLL_ENABLE, true);
	if(pPlayer->IsFirstPersonShooterModeEnabledForPlayer(false) && !COMBAT_ROLL_ENABLE)
	{
		return false;
	}
#endif	// FPS_MODE_SUPPORTED

	CControl* pControl = pPlayer->GetControlFromPlayer();

	// Get the weapon
	weaponAssert(pPlayer->GetWeaponManager());
	const CWeapon* pWeapon = pPlayer->GetWeaponManager()->GetEquippedWeapon();
#if FPS_MODE_SUPPORTED
	if(pPlayer->IsFirstPersonShooterModeEnabledForPlayer(false) && (pPlayer->GetMotionData()->GetIsFPSIdle() ||
		(pWeapon && pWeapon->GetWeaponInfo() && (pWeapon->GetWeaponInfo()->GetIsUnarmed() || pWeapon->GetWeaponInfo()->GetIsMelee()))))
	{
		return false;
	}
#endif

	if(pControl && pControl->GetPedJump().IsDown() && (!pWeapon || !pWeapon->GetWeaponInfo()->GetDisableCombatRoll()) && pPlayer->GetPedIntelligence()->GetCanCombatRoll() && !pPlayer->GetIsInVehicle() && !pPlayer->GetIsParachuting())
	{
		return true;
	}

	return false;
}

static dev_u32 s_fHeldAndPressedMod = 2;

//-------------------------------------------------------------------------
// Returns true if the player is  trying to select the next target to the left
//-------------------------------------------------------------------------
bool CPlayerInfo::IsSelectingLeftTarget(bool bCheckAimButton)
{
	// IF the player is hard aiming now, return true if they weren't last frame
	CPed * pPlayer = CGameWorld::FindLocalPlayer(); 
	if( !pPlayer ) { return false; }
	CControl* pControl = pPlayer->GetControlFromPlayer();
	if( !pControl ) { return false; }

	bool bAssistedAiming = pPlayer->GetPlayerInfo()->GetPlayerResetFlags().IsFlagSet(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON);

	if( (!bCheckAimButton || IsHardAiming() || IsDriverFiring() || bAssistedAiming) &&
		CTaskPlayerOnFoot::ms_bAnalogueLockonAimControl )
	{
		if( pControl->GetPedAimWeaponLeftRight().HistoryPressedAndHeld( (fwTimer::GetTimeStepInMilliseconds() * s_fHeldAndPressedMod) , NULL, -ms_PlayerSoftTargetSwitchBoundary) )
		{
			return true;
		}
	}

	return false;
}

//-------------------------------------------------------------------------
// Returns true if the player is  trying to select the next target to the right
//-------------------------------------------------------------------------
bool CPlayerInfo::IsSelectingRightTarget(bool bCheckAimButton)
{
	// IF the player is hard aiming now, return true if they weren't last frame
	CPed * pPlayer = CGameWorld::FindLocalPlayer();
	if( !pPlayer ) { return false; }
	CControl* pControl = pPlayer->GetControlFromPlayer();
	if( !pControl ) { return false; }

	bool bAssistedAiming = pPlayer->GetPlayerInfo()->GetPlayerResetFlags().IsFlagSet(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON);

	if( (!bCheckAimButton || IsHardAiming() || IsDriverFiring() || bAssistedAiming) &&
		CTaskPlayerOnFoot::ms_bAnalogueLockonAimControl )
	{
		if(pControl->GetPedAimWeaponLeftRight().HistoryPressedAndHeld((fwTimer::GetTimeStepInMilliseconds() * s_fHeldAndPressedMod), NULL, ms_PlayerSoftTargetSwitchBoundary) )
		{
			return true;
		}
	}

	return false;
}


void CPlayerInfo::SetModelId(fwModelId modelId)
{
	const CPedModelInfo* pModelInfo = static_cast<CPedModelInfo*>(CModelInfo::GetBaseModelInfo(modelId));
	Assert(pModelInfo);

	if (!pModelInfo->GetIsPlayerModel())
	{
		return;
	}

	CPed * pPed = GetPlayerPed();

	//mjc - why is this not called in the CPed::SetModelId ?
	pPed->SetInitialState();

	// load in the initial data for the player
	CPedVariationStream::RequestStreamPedFiles(pPed, &pPed->GetPedDrawHandler().GetVarData(), (STRFLAG_FORCE_LOAD | STRFLAG_PRIORITY_LOAD));	

	CPedVariationStream::ProcessStreamRequests();
}

void CPlayerInfo::SetupPlayerGroup()
{
	const s32 playerGroup = CPedGroups::GetPlayersGroup(GetPlayerPed());
	m_PlayerGroup = playerGroup;
	CPedGroups::AddGroupAtIndex(POPTYPE_PERMANENT, playerGroup);
	if (AssertVerify(CPedGroups::ms_groups[playerGroup].GetGroupMembership()))
	{
		CPedGroups::ms_groups[playerGroup].GetGroupMembership()->SetLeader(GetPlayerPed(), NetworkInterface::IsNetworkOpen());
	}
	CPedGroups::ms_groups[playerGroup].Process();
}


bool CPlayerInfo::CanPlayerStartMission()
{
	//RAGE	if (CGameLogic::GameState != CGameLogic::GAMESTATE_PLAYING || CGameLogic::IsCoopGameGoingOn()) return false;	// Obbe: put in to fix the problem with ending a 2 player game on a mission trigger.
	CPed * pPed = GetPlayerPed();
	if (pPed->IsPedInControl() == false && !pPed->GetIsDrivingVehicle() )
	{
		return false;
	}

	return true;
}


// Name			:	KeepAreaAroundPlayerClear
// Purpose		:	When a player is in a cutscene, tries to keep the area around him clear of peds and cars
// Parameters	:	
// Returns		:	
void CPlayerInfo::KeepAreaAroundPlayerClear()
{
	if (!m_bStopNearbyVehicles)
	{
		return;
	}

	// check peds
	int i;
	CEntity		*ppResults[8]; // whatever.. temp list
	CVehicle	*pVehicle;
	CVehicle	*pPlayersVehicle = NULL;
	s32		Num;
	Vector3		vecDistance, vecPosition;
	

	if(GetPlayerPed()->GetIsInVehicle())
	{
		vecPosition = VEC3V_TO_VECTOR3(GetPlayerPed()->GetMyVehicle()->GetTransform().GetPosition());
		pPlayersVehicle = GetPlayerPed()->GetMyVehicle();
	}
	else
		vecPosition = VEC3V_TO_VECTOR3(GetPlayerPed()->GetTransform().GetPosition());
	
	// check for threat peds in cars
	// Should the first parameter below actually be vecPosition?
	CGameWorld::FindObjectsInRange(VEC3V_TO_VECTOR3(GetPlayerPed()->GetTransform().GetPosition()), 15.0f, true, &Num, 6, ppResults, 0, 1, 0, 0, 0);	// Just Cars we're interested in
	// cars in range stored in ppResults

	for(i=0; i<Num; i++) // find cars close by
	{
		pVehicle = (CVehicle *)ppResults[i];

		if( pVehicle!=pPlayersVehicle &&
			pVehicle->GetDriver() &&
			!pVehicle->PopTypeIsMission() && 
			!pVehicle->IsNetworkClone() &&
			pVehicle->GetStatus() != STATUS_PLAYER &&
			pVehicle->GetStatus() != STATUS_PLAYER_DISABLED &&
			!pVehicle->InheritsFromTrailer() &&
			!pVehicle->m_nVehicleFlags.bHasParentVehicle && 
			!pVehicle->GetDriver()->IsDead())
		{
			const Vector3 vecVehiclePosition = VEC3V_TO_VECTOR3(pVehicle->GetVehiclePosition());
			vecDistance = vecVehiclePosition - vecPosition;
			
			// Depending on whether the car is facing away or towards the player we will reverse or go foward a bit
			Vector3 vecForward(VEC3V_TO_VECTOR3(pVehicle->GetVehicleForwardDirection()));
			if ( (vecForward.x * (vecPosition.x - vecVehiclePosition.x)) +
				 (vecForward.y * (vecPosition.y - vecVehiclePosition.y)) > 0.0f)
			{	
				// cars are far enough away, stop them moving
				if(vecDistance.Mag2() > 5.0f*5.0f)
				{
					// Tell cars to stop for 5 secs so that they will automatically start driving again after a while
					aiTask *pTask = rage_new CTaskVehicleWait(NetworkInterface::GetSyncedTimeInMilliseconds() + 5000);
					pVehicle->GetIntelligence()->AddTask(VEHICLE_TASK_TREE_PRIMARY, pTask, VEHICLE_TASK_PRIORITY_PRIMARY, false);
				}
				else
				{
					// reverse away
					aiTask *pTask = rage_new CTaskVehicleReverse(NetworkInterface::GetSyncedTimeInMilliseconds() + 2000, CTaskVehicleReverse::Reverse_Opposite_Direction);
					pVehicle->GetIntelligence()->AddTask(VEHICLE_TASK_TREE_PRIMARY, pTask, VEHICLE_TASK_PRIORITY_PRIMARY, false);
				}
			}
			else
			{	
				if(vecDistance.Mag2() > 5.0f*5.0f)
				{
					//nothing, continue on our way
				}
				else
				{
					// go forward
					aiTask *pTask = rage_new CTaskVehicleGoForward(NetworkInterface::GetSyncedTimeInMilliseconds() + 2000);
					pVehicle->GetIntelligence()->AddTask(VEHICLE_TASK_TREE_PRIMARY, pTask, VEHICLE_TASK_PRIORITY_PRIMARY, false);
				}
			}
		}	
	}
}

void CPlayerInfo::SetPlayerCanBeDamaged(bool DmgeFlag)
{
	if(m_bCanBeDamaged != DmgeFlag)
	{
		aiDebugf1("SetPlayerCanBeDamaged :: %s -> %s", m_bCanBeDamaged ? "True" : "False", DmgeFlag ? "True" : "False");
		m_bCanBeDamaged = DmgeFlag;
	}
	
	// reset this to be sure we catch all cases, we'll apply the flag from SetPlayerControl separately
	m_DamageAllowedFromSetPlayerControl = false;
}

f32 CPlayerInfo::GetPlayerMeleeDamageModifier(bool bUnarmedMelee) const
{
	if (bUnarmedMelee)
	{
		return GetPlayerMeleeUnarmedDamageModifier();
	}
	else
	{
		return GetPlayerMeleeWeaponDamageModifier();
	}
}

void CPlayerInfo::SetLastWeaponHashPickedUp(u32 uWeaponHash) 
{ 
	m_LastWeaponHashPickedUp = uWeaponHash; 
	m_TimeLastWeaponPickedUp = fwTimer::GetTimeInMilliseconds(); 
}

void CPlayerInfo::DecayStealthNoise(float timeStep)
{
	if(m_StealthNoise > ms_StealthNoiseMinValToCareAbout)
	{
		// To do linear decay, it would be appropriate to compute a decay factor like this
		//	const float mul = expf(-timeStep/ms_StealthNoiseExponentialDecayTimeConstant);
		// but the exponential function is kind of slow and as long as the time step is small
		// compared to the time constant, this is a decent enough approximation:
		const float mul = Max(1.0f - timeStep/ms_StealthNoiseExponentialDecayTimeConstant, 0.0f);

		m_StealthNoise *= mul;
		if(m_StealthNoise <= ms_StealthNoiseMinValToCareAbout)
		{
			// No point in doing this once we get to a value small enough, more
			// clear to let it snap to zero rather than letting it go infinitely small.
			m_StealthNoise = 0.0f;
		}
#if __BANK && DEBUG_DRAW
		else if (CPedDebugVisualiser::GetDebugDisplay() == CPedDebugVisualiser::eStealth)
		{
			grcDebugDraw::Circle(m_pPlayerPed->GetTransform().GetPosition(), m_StealthNoise, Color_BlanchedAlmond, m_pPlayerPed->GetTransform().GetRight(), m_pPlayerPed->GetTransform().GetForward());
		}
#endif // __BANK && DEBUG_DRAW

		// Note: originally I was thinking about linear decay
		//	m_StealthNoise = Max(m_StealthNoise - timeStep*ms_StealthNoiseLinearDecayRate, 0.0f);
		// but I'm thinking that perhaps that will give you too much of a time difference between
		// some quiet sound and a really loud one.
	}
}

void	CPlayerInfo::ReportStealthNoise(float noiseRange)
{ 
	if (noiseRange > 0.0f)
	{
		m_StealthNoise = Max(m_StealthNoise, noiseRange); 
	}
}


void CPlayerInfo::IncrementAiAttemptingArrestOnPlayer()
{
	if(aiVerifyf(m_uNumAiAttemptingArrestOnPlayer < 255, "Something went wrong, ref for arresting peds on this player is way too high!"))
	{
		m_uNumAiAttemptingArrestOnPlayer++;
	}
}

void CPlayerInfo::DecrementAiAttemptingArrestOnPlayer()
{
	if(m_uNumAiAttemptingArrestOnPlayer > 0)
	{
		m_uNumAiAttemptingArrestOnPlayer--;
	}
}

void CPlayerInfo::SetSimulateGaitInput(float fMoveBlendRatio, float fTime /*= -1.0f*/, float fHeading /*= 0.0f*/, bool bRelativeHeading /*= false*/, bool bNoInputInterruption /*= false*/)
{ 
	m_bSimulateGaitInput = true; 
	m_fSimulateGaitMoveBlendRatio = fMoveBlendRatio;
	m_fSimulateGaitDuration = fTime;
	m_fSimulateGaitTimerCount = 0.0f;
	m_fSimulateGaitHeading = fHeading;
	m_bSimulateGaitUseRelativeHeading = bRelativeHeading;
	m_fSimulateGaitStartHeading = GetPlayerPed()->GetCurrentHeading();
	m_bSimulateGaitNoInputInterruption = bNoInputInterruption;

	Assertf(!(fTime < 0.0f && bNoInputInterruption), "The player will never break out of simulated gait with infinite timer and no input interruption.");
}

void CPlayerInfo::ProcessFireProofTimer()
{
	if(m_sFireProofTimer > 0)
	{
		m_sFireProofTimer -= fwTimer::GetTimeStepInMilliseconds();
		if(m_sFireProofTimer > 0)
		{
			bFireProof = true;
		}
		else
		{
			bFireProof = false;
		}
	}
}

bool CPlayerInfo::HaveNumEnemiesChargedWithinQueryPeriod( u32 queryPeriodMS, int queryCount ) const
{
	// initialize count to zero
	int numEnemiesCount = 0;

	// check the current time
	u32 currentTimeMS = fwTimer::GetTimeInMilliseconds();

	// compute the comparison time for this query
	u32 queryWindowStartTimeMS = 0;
	if( queryPeriodMS < currentTimeMS )
	{
		queryWindowStartTimeMS = currentTimeMS - queryPeriodMS;
	}

	// traverse the history
	for(int i=0; i < CHARGER_HISTORY_SIZE; ++i)
	{
		// if the recorded time is within the query window
		if( m_ChargedByEnemyHistoryMS[i] > queryWindowStartTimeMS )
		{
			// add to the count of qualifying charge incidents
			numEnemiesCount++;

			// check if the query count has been reached
			if( numEnemiesCount >= queryCount )
			{
				return true;
			}
		}
	}

	// fewer than query count occurrences in query window
	return false;
}

bool CPlayerInfo::IsBestChargerCandidate( const CPed* pCandidatePed ) const
{
	// Check if the acceptance period is not open
	if( m_BestCandidateAcceptanceEndTimeMS == 0 )
	{
		// no best candidate yet
		return false;
	}

	// Check if the given ped is the stored best candidate
	Assert(pCandidatePed);
	if( m_BestEnemyChargerCandidatePed.Get() == pCandidatePed )
	{
		return true;
	}

	// by default the given ped is not the best candidate
	return false;
}

void CPlayerInfo::ReportEnemyStartedCharge()
{
	// get the current time to record
	u32 uTimeToAddMS = fwTimer::GetTimeInMilliseconds();

	// find the oldest entry
	Assert(CHARGER_HISTORY_SIZE > 1);
	u32 uMinRecordTime = m_ChargedByEnemyHistoryMS[0];
	int iMinIndex = 0;
	for(int i=1; i < CHARGER_HISTORY_SIZE; ++i)
	{
		if( m_ChargedByEnemyHistoryMS[i] < uMinRecordTime )
		{
			uMinRecordTime = m_ChargedByEnemyHistoryMS[i];
			iMinIndex = i;
		}
	}

	// overwrite the oldest entry with this new time
	m_ChargedByEnemyHistoryMS[iMinIndex] = uTimeToAddMS;

	// clear the stored best candidate for charging
	m_BestEnemyChargerCandidatePed = NULL;

	// close the acceptance period as the winning ped has charged
	m_BestCandidateAcceptanceEndTimeMS = 0;

	aiDebugf3("CHARGING: CPlayerInfo::ReportEnemyStartedCharge() at time %d", uTimeToAddMS);
}

void CPlayerInfo::RegisterCandidateCharger( const CPed* pCandidatePed )
{
	// If the player ped is null
	if( m_pPlayerPed.Get() == NULL )
	{
		// no work to do
		return;
	}

	// If the acceptance period is open
	if( m_BestCandidateAcceptanceEndTimeMS > 0 )
	{
		// not accepting candidates at this time
		return;
	}

	// If there is an active election for smoke throw
	if( m_BestEnemySmokeThrowerCandidatePed.Get() != NULL )
	{
		// not accepting candidates at this time
		return;
	}

	// If the registration end time is zero
	if( m_CandidateRegistrationEndTimeMS == 0 )
	{
		// schedule an end time for the registration
		const u32 uRegistrationPeriodMS = 2000;
		m_CandidateRegistrationEndTimeMS = fwTimer::GetTimeInMilliseconds() + uRegistrationPeriodMS;
		aiDebugf3("CHARGING: CPlayerInfo::RegisterCandidateCharger scheduled new registration end: %d",m_CandidateRegistrationEndTimeMS);
	}

	// Check if the candidate ped is dead
	if( m_BestEnemyChargerCandidatePed.Get() && m_BestEnemyChargerCandidatePed.Get()->GetIsDeadOrDying() )
	{
		// clear the handle
		m_BestEnemyChargerCandidatePed = NULL;
		aiDebugf3("CHARGING: CPlayerInfo::RegisterCandidateCharger set candidate to NULL");
	}

	// If the candidate is null
	if( m_BestEnemyChargerCandidatePed.Get() == NULL )
	{
		// set the candidate as the current best
		m_BestEnemyChargerCandidatePed = pCandidatePed;
		aiDebugf3("CHARGING: CPlayerInfo::RegisterCandidateCharger candidate was NULL, now [0x%p]", pCandidatePed);
	}
	else // candidate exists
	{
		// compute the distances between candidates and the player
		const Vec3V playerPosition = m_pPlayerPed->GetTransform().GetPosition();
		const ScalarV candidateDistSq = DistSquared(pCandidatePed->GetTransform().GetPosition(), playerPosition);
		const ScalarV bestCandidateDistSq = DistSquared(m_BestEnemyChargerCandidatePed->GetTransform().GetPosition(), playerPosition);

		// if this candidate is closer to the player
		if( IsLessThanAll(candidateDistSq, bestCandidateDistSq) )
		{
			aiDebugf3("CHARGING: CPlayerInfo::RegisterCandidateCharger candidate was [0x%p], now [0x%p]", m_BestEnemyChargerCandidatePed.Get(), pCandidatePed);

			// then this is the new best candidate
			m_BestEnemyChargerCandidatePed = pCandidatePed;
		}
	}
}

// Check if at least queryCount throws have occurred within the given query period.
bool CPlayerInfo::HaveNumEnemiesThrownSmokeWithinQueryPeriod(u32 queryPeriodMS, int queryCount) const
{
	// initialize count to zero
	int numEnemiesCount = 0;

	// check the current time
	u32 currentTimeMS = fwTimer::GetTimeInMilliseconds();

	// compute the comparison time for this query
	u32 queryWindowStartTimeMS = 0;
	if( queryPeriodMS < currentTimeMS )
	{
		queryWindowStartTimeMS = currentTimeMS - queryPeriodMS;
	}

	// traverse the history
	for(int i=0; i < SMOKE_THROWER_HISTORY_SIZE; ++i)
	{
		// if the recorded time is within the query window
		if( m_SmokeThrownByEnemyHistoryMS[i] > queryWindowStartTimeMS )
		{
			// add to the count of qualifying incidents
			numEnemiesCount++;

			// check if the query count has been reached
			if( numEnemiesCount >= queryCount )
			{
				return true;
			}
		}
	}

	// fewer than query count occurrences in query window
	return false;
}

// Check if the given ped is the best thrower candidate in the most recent registration period.
bool CPlayerInfo::IsBestSmokeThrowerCandidate(const CPed* pCandidatePed) const
{
	// Check if the acceptance period is not open
	if( m_BestCandidateAcceptanceEndTimeMS == 0 )
	{
		// no best candidate yet
		return false;
	}

	// Check if the given ped is the stored best candidate
	Assert(pCandidatePed);
	if( m_BestEnemySmokeThrowerCandidatePed.Get() == pCandidatePed )
	{
		return true;
	}

	// by default the given ped is not the best candidate
	return false;
}

// The best thrower candidate has started his throw.
// Adds current time to history and closes the acceptance period.
void CPlayerInfo::ReportEnemyStartedSmokeThrow()
{
	// get the current time to record
	u32 uTimeToAddMS = fwTimer::GetTimeInMilliseconds();

	// find the oldest entry
	Assert(SMOKE_THROWER_HISTORY_SIZE > 1);
	u32 uMinRecordTime = m_SmokeThrownByEnemyHistoryMS[0];
	int iMinIndex = 0;
	for(int i=1; i < SMOKE_THROWER_HISTORY_SIZE; ++i)
	{
		if( m_SmokeThrownByEnemyHistoryMS[i] < uMinRecordTime )
		{
			uMinRecordTime = m_SmokeThrownByEnemyHistoryMS[i];
			iMinIndex = i;
		}
	}

	// overwrite the oldest entry with this new time
	m_SmokeThrownByEnemyHistoryMS[iMinIndex] = uTimeToAddMS;

	// clear the stored best candidate
	m_BestEnemySmokeThrowerCandidatePed = NULL;

	// close the acceptance period as the winning ped has executed
	m_BestCandidateAcceptanceEndTimeMS = 0;

	aiDebugf3("SMOKE THROW: CPlayerInfo::ReportEnemyStartedSmokeThrow() at time %d", uTimeToAddMS);
}

// Consider the given ped as a candidate.
// Opens a new registration period if appropriate.
// Does nothing if the acceptance period is open.
void CPlayerInfo::RegisterCandidateSmokeThrower(const CPed* pCandidatePed)
{
	// If the player ped is null
	if( m_pPlayerPed.Get() == NULL )
	{
		// no work to do
		return;
	}

	// If the acceptance period is open
	if( m_BestCandidateAcceptanceEndTimeMS > 0 )
	{
		// not accepting candidates at this time
		return;
	}

	// If there is an active election for charging
	if( m_BestEnemyChargerCandidatePed.Get() != NULL )
	{
		// not accepting candidates at this time
		return;
	}

	// If the registration end time is zero
	if( m_CandidateRegistrationEndTimeMS == 0 )
	{
		// schedule an end time for the registration
		const u32 uRegistrationPeriodMS = 2000;
		m_CandidateRegistrationEndTimeMS = fwTimer::GetTimeInMilliseconds() + uRegistrationPeriodMS;
		aiDebugf3("SMOKE THROW: CPlayerInfo::RegisterCandidateSmokeThrower scheduled new registration end: %d",m_CandidateRegistrationEndTimeMS);
	}

	// Check if the candidate ped is dead
	if( m_BestEnemySmokeThrowerCandidatePed.Get() && m_BestEnemySmokeThrowerCandidatePed.Get()->GetIsDeadOrDying() )
	{
		// clear the handle
		m_BestEnemySmokeThrowerCandidatePed = NULL;
		aiDebugf3("SMOKE THROW: CPlayerInfo::RegisterCandidateSmokeThrower set candidate to NULL");
	}

	// If the candidate is null
	if( m_BestEnemySmokeThrowerCandidatePed.Get() == NULL )
	{
		// set the candidate as the current best
		m_BestEnemySmokeThrowerCandidatePed = pCandidatePed;
		aiDebugf3("SMOKE THROW: CPlayerInfo::RegisterCandidateSmokeThrower candidate was NULL, now [0x%p]", pCandidatePed);
	}
	else // candidate exists
	{
		// compute the distances between candidates and the player
		const Vec3V playerPosition = m_pPlayerPed->GetTransform().GetPosition();
		const ScalarV candidateDistSq = DistSquared(pCandidatePed->GetTransform().GetPosition(), playerPosition);
		const ScalarV bestCandidateDistSq = DistSquared(m_BestEnemySmokeThrowerCandidatePed->GetTransform().GetPosition(), playerPosition);

		// if this candidate is closer to the player
		if( IsLessThanAll(candidateDistSq, bestCandidateDistSq) )
		{
			aiDebugf3("SMOKE THROW: CPlayerInfo::RegisterCandidateSmokeThrower candidate was [0x%p], now [0x%p]", m_BestEnemySmokeThrowerCandidatePed.Get(), pCandidatePed);

			// then this is the new best candidate
			m_BestEnemySmokeThrowerCandidatePed = pCandidatePed;
		}
	}
}

void CPlayerInfo::ShoutTargetPosition()
{
	if(!m_pPlayerPed)
	{
		return;
	}

	if(fwTimer::GetTimeInMilliseconds() - m_LastShoutTargetPositionTime < ms_Tunables.m_TimeBetweenShoutTargetPosition)
	{
		return;
	}

	CEntity* pTargetEntity = m_targeting.GetTarget();
	if(pTargetEntity && pTargetEntity->GetIsTypePed())
	{
		CPedGroup* pPedGroup = m_pPlayerPed->GetPedsGroup();
		if(pPedGroup)
		{
			CEventShoutTargetPosition shoutTargetEvent(m_pPlayerPed, static_cast<CPed*>(pTargetEntity));
			pPedGroup->GiveEventToAllMembers(shoutTargetEvent, m_pPlayerPed);

			m_LastShoutTargetPositionTime = fwTimer::GetTimeInMilliseconds();
		}
	}
}

s32 CPlayerInfo::GetTimeSinceLastAimedMS()
{
	return fwTimer::GetTimeInMilliseconds() - m_uLastTimeStopAiming;
}

void CPlayerInfo::ClearPreviousVariationData()
{
	m_PreviousVariationComponent = PV_COMP_INVALID;
	m_PreviousVariationDrawableId = 0;
	m_PreviousVariationDrawableAltId = 0;
	m_PreviousVariationTexId = 0;
	m_PreviousVariationPaletteId = 0;
}

void CPlayerInfo::GetPreviousVariationInfo(u32& rPreviousVariationDrawableId, u32& rPreviousVariationDrawableAltId, u32& rPreviousVariationTexId, u32& rPreviousVariationPaletteId)
{
	rPreviousVariationDrawableId = m_PreviousVariationDrawableId;
	rPreviousVariationDrawableAltId = m_PreviousVariationDrawableAltId;
	rPreviousVariationTexId = m_PreviousVariationTexId;
	rPreviousVariationPaletteId = m_PreviousVariationPaletteId;
}

void CPlayerInfo::SetPreviousVariationData(ePedVarComp ePreviousVariationComponent, u32 uPreviousVariationDrawableId, u32 uPreviousVariationDrawableAltId,
												u32 uPreviousVariationTexId, u32 uPreviousVariationPaletteId)
{
	m_PreviousVariationComponent = ePreviousVariationComponent;
	m_PreviousVariationDrawableId = uPreviousVariationDrawableId;
	m_PreviousVariationDrawableAltId = uPreviousVariationDrawableAltId;
	m_PreviousVariationTexId = uPreviousVariationTexId;
	m_PreviousVariationPaletteId = uPreviousVariationPaletteId;
}

void CPlayerInfo::SetPedParachuteVariationOverride(ePedVarComp slotId, u32 drawblId, u32 texId, u32 altDrawblId)
{
	m_ParachuteVariationComponent = slotId;
	m_ParachuteVariationDrawable = drawblId;
	m_ParachuteVariationAltDrawable = altDrawblId;
	m_ParachuteVariationTexId = texId;
}

void CPlayerInfo::ClearPedParachuteVariationOverride()
{
	m_ParachuteVariationComponent = PV_COMP_INVALID;
	m_ParachuteVariationDrawable = 0;
	m_ParachuteVariationAltDrawable = 0;
	m_ParachuteVariationTexId = 0;
}

#if __BANK

void CPlayerInfo::AddAimWidgets(bkBank & bank)
{
	bank.AddSlider("Low aim boundary",				&ms_PlayerLowAimBoundry, 0.0f, 1.0f, 0.05f);
	bank.AddSlider("Soft/Hard aim boundary",		&ms_PlayerSoftAimBoundary, 0.0f, 1.0f, 0.05f);
	bank.AddSlider("Soft target switch boundary",	&ms_PlayerSoftTargetSwitchBoundary, 0.0f, 1.0f, 0.05f);
	bank.AddToggle("Reverse LT soft/hard aim",		&ms_ReverseAimBoundarys);
#if FPS_MODE_SUPPORTED
	bank.AddSlider("FPS fire boundary low",			&ms_PlayerFPSFireBoundaryLow, 0.0f, 1.0f, 0.05f);
	bank.AddSlider("FPS fire boundary high",		&ms_PlayerFPSFireBoundaryHigh, 0.0f, 1.0f, 0.05f);
#endif
}

#endif // __BANK

#if __BANK
void CPlayerInfo::AddWeaponTuningWidgets(bkBank& bank)
{
	bank.AddSlider("Player Weapon Damage Modifier", &m_fWeaponDamageModifier, 0.0f, 10.0f, 0.01f);
	bank.AddSlider("Player Weapon Defense Modifier", &m_fWeaponDefenseModifier, 0.0f, 10.0f, 0.01f);
	bank.AddSlider("Player Weapon Minigun Defense Modifier", &m_fWeaponMinigunDefenseModifier, 0.0f, 10.0f, 0.01f);
	bank.AddSlider("Player Melee Weapon Damage Modifier", &m_fMeleeWeaponDamageModifier, 0.0f, 10.0f, 0.01f);
	bank.AddSlider("Player Melee Unarmed Damage Modifier", &m_fMeleeUnarmedDamageModifier, 0.0f, 10.0f, 0.01f);
	bank.AddSlider("Player Melee Weapon Defense Modifier", &m_fMeleeWeaponDefenseModifier, 0.0f, 10.0f, 0.01f);
	bank.AddSlider("Player Melee Weapon Force Modifier", &m_fMeleeWeaponForceModifier, 1.0f, 10.0f, 0.01f);
	bank.AddSlider("Player Vehicle Damage Modifier", &m_fVehicleDamageModifier, 0.0f, 10.0f, 0.01f);
	bank.AddSlider("Player Vehicle Defense Modifier", &m_fVehicleDefenseModifier, 0.0f, 10.0f, 0.01f);
	bank.AddSlider("Player Max Explosive Damage", &m_fMaxExplosiveDamage, -1.0f, 10000.0f, 0.01f);
	bank.AddSlider("Player Explosive Damage Modifier", &m_fExplosiveDamageModifier, 0.0f, 10.0f, 0.01f);
	bank.AddSlider("Player Weapon Takedown Defense Modifier", &m_fWeaponTakedownDefenseModifier, 0.0f, 10.0f, 0.01f);
	bank.AddSlider("Ai Weapon Damage Modifier", CPedDamageCalculator::GetAiWeaponDamageModifierPtr(), 0.0f, 10.0f, 0.01f);
	bank.AddSlider("Ai Melee Weapon Damage Modifier", CPedDamageCalculator::GetAiMeleeWeaponDamageModifierPtr(), 0.0f, 10.0f, 0.01f);
}
#endif // __BANK

bool CPlayerInfo::IsPlayerStateValidForFPSManipulation() const
{
	if(m_pPlayerPed && 
		!m_pPlayerPed->GetMovePed().GetTaskNetwork() &&
		m_pPlayerPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_AMBIENT_CLIPS) && 
		!m_pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_DisableIdleExtraHeadingChange) &&
		!m_pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_PreserveAnimatedAngularVelocity) &&
		m_pPlayerPed->IsLocalPlayer() && !AreControlsDisabled() &&
		m_pPlayerPed->IsFirstPersonShooterModeEnabledForPlayer(false, false, true))
	{
		return true;
	}
	return false;
}

SpecialAbilityType CPlayerInfo::GetMPSpecialAbility(ePlayerSpecialAbilitySlot eAbilitySlot /*= PSAS_PRIMARY*/) const
{
	if (CPed::IsValidSpecialAbilitySlot(eAbilitySlot))
	{
		return m_eSpecialAbilitiesMP[eAbilitySlot];
	}
	else
	{
		return SAT_NONE;
	}
}

void CPlayerInfo::SetMPSpecialAbility(SpecialAbilityType eAbility, ePlayerSpecialAbilitySlot eAbilitySlot /*= PSAS_PRIMARY*/)
{ 
	if (!CPed::IsValidSpecialAbilitySlot(eAbilitySlot))
		return;
	
	const bool bHasChanged = m_eSpecialAbilitiesMP[eAbilitySlot] != eAbility;	
	m_eSpecialAbilitiesMP[eAbilitySlot] = eAbility;
	
	if (m_pPlayerPed)
	{
		// if special ability has been changed, or has not been initialised on ped, or is different to what's been set on the ped, init special ability on ped
		if (bHasChanged || !m_pPlayerPed->GetSpecialAbility(eAbilitySlot) || m_eSpecialAbilitiesMP[eAbilitySlot] != m_pPlayerPed->GetSpecialAbilityType(eAbilitySlot))
		{
			m_pPlayerPed->InitSpecialAbility(eAbilitySlot);
		}		
	}
}

bool CPlayerInfo::CanPlayerPerformVehicleMelee() const
{
	TUNE_GROUP_BOOL(VEHICLE_MELEE, bDisableInSP, true);
	if (bDisableInSP && !NetworkInterface::IsGameInProgress())
	{
		return false;
	}

	const CVehicle* pVeh = m_pPlayerPed->GetVehiclePedInside();
	if (!pVeh)
		return false;

	if (!pVeh->InheritsFromBike() && !pVeh->InheritsFromQuadBike() && !pVeh->InheritsFromAmphibiousQuadBike())
		return false;

	if (MI_BIKE_OPPRESSOR.IsValid() && MI_BIKE_OPPRESSOR == pVeh->GetModelIndex())
		return false;

	if (m_pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsSwitchingHelmetVisor))
		return false;

	if (m_pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DisableVehicleCombat))
		return false;

	if (m_pPlayerPed->GetNetworkObject())
	{
		CNetObjPlayer* pOwnerNetObjPlayer = static_cast<CNetObjPlayer*>(m_pPlayerPed->GetNetworkObject());
		if (pOwnerNetObjPlayer->IsPassiveMode())
		{
			return false;
		}
	}

	TUNE_GROUP_BOOL(VEHICLE_MELEE, bDisableStationaryKicks, true);
	if (bDisableStationaryKicks && ((pVeh->GetVelocity().Mag() <= CTaskMountThrowProjectile::ms_fBikeStationarySpeedThreshold) || (pVeh->GetThrottle() < -0.01f)))
	{
		const CPedWeaponManager* pWeaponMgr = m_pPlayerPed->GetWeaponManager();
		if (pWeaponMgr && pWeaponMgr->GetEquippedWeaponInfo() && (pWeaponMgr->GetEquippedWeaponInfo()->GetIsUnarmed() || pWeaponMgr->GetEquippedWeaponInfo()->GetIsThrownWeapon()))
		{
			return false;
		}
	}

	if (m_pPlayerPed->GetWeaponManager())
	{
		const CWeaponInfo* pWeaponInfo = m_pPlayerPed->GetWeaponManager()->GetEquippedWeaponInfo();
		if (pWeaponInfo)
		{
			const CVehicleDriveByAnimInfo* pDriveByClipInfo = CVehicleMetadataMgr::GetDriveByAnimInfoForWeapon(m_pPlayerPed, pWeaponInfo->GetHash());

#if FPS_MODE_SUPPORTED
			// FP is not supported for now for bike melee
			//bool bFirstPerson = false;
			//if (m_pPlayerPed->IsFirstPersonShooterModeEnabledForPlayer(false) || m_pPlayerPed->IsInFirstPersonVehicleCamera())
			//	bFirstPerson = true;

			//if (bFirstPerson && pDriveByClipInfo && pDriveByClipInfo->GetFirstPersonVehicleMeleeClipSet() == CLIP_SET_ID_INVALID)
			//{
			//	return false;
			//}
			//else
#endif //FPS_MODE_SUPPORTED
			if (pDriveByClipInfo && pDriveByClipInfo->GetVehicleMeleeClipSet() == CLIP_SET_ID_INVALID)
			{
				return false;
			}
		}
	}


	return true;
}

// Static member initialisation
CAnimSpeedUps::Tunables CAnimSpeedUps::ms_Tunables;	
IMPLEMENT_PLAYER_INFO_TUNABLES(CAnimSpeedUps, 0xcd9f180b);

#if RSG_PC
void ReportPlayerResetMismatch(u32 expected, u32 tampered)
{
	// Here, we want to 
	aiAssertf(false, "Unexpected mismatch in CPlayeResetFlags; please create a B* on Amir Soofi & CC Sean Casey");
	LinkDataReport r(expected^tampered, LinkDataReporterCategories::LDRC_PLAYERRESET);
	LinkDataReporterPlugin_Report(r);
	return;
}
#endif