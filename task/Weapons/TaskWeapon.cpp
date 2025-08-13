// File header
#include "Task/Weapons/TaskWeapon.h"

// Game headers
#include "camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "frontend/NewHud.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "Task/Weapons/Gun/TaskAimGunOnFoot.h"
#include "Task/Weapons/Gun/TaskGun.h"
#include "Task/Weapons/Gun/TaskReloadGun.h"
#include "Task/Weapons/TaskBomb.h"
#include "task/Weapons/TaskDetonator.h"
#include "Task/Weapons/TaskProjectile.h"
#include "Task/Weapons/WeaponController.h"
#include "Weapons/Info/WeaponInfoManager.h"

// Macro to disable optimisations if set
AI_OPTIMISATIONS()
AI_WEAPON_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CTaskWeapon
//////////////////////////////////////////////////////////////////////////

CTaskWeapon::CTaskWeapon(const CWeaponController::WeaponControllerType weaponControllerType, CTaskTypes::eTaskType aimTaskType, s32 iFlags, u32 uFiringPatternHash, const CWeaponTarget& target, float fDuration)
: m_weaponControllerType(weaponControllerType)
, m_iFlags(iFlags)
, m_aimTaskType(aimTaskType)
, m_target(target)
, m_uFiringPatternHash(uFiringPatternHash)
, m_fDuration(fDuration)
, m_bShouldUseAssistedAimingCamera(false)
, m_bShouldUseRunAndGunAimingCamera(false)
{
	SetInternalTaskType(CTaskTypes::TASK_WEAPON);
}

CTaskWeapon::CTaskWeapon(const CTaskWeapon& other)
: m_weaponControllerType(other.m_weaponControllerType)
, m_iFlags(other.m_iFlags)
, m_aimTaskType(other.m_aimTaskType)
, m_target(other.m_target)
, m_uFiringPatternHash(other.m_uFiringPatternHash)
, m_fDuration(other.m_fDuration)
, m_bShouldUseAssistedAimingCamera(other.m_bShouldUseAssistedAimingCamera)
, m_bShouldUseRunAndGunAimingCamera(other.m_bShouldUseRunAndGunAimingCamera)
{
	SetInternalTaskType(CTaskTypes::TASK_WEAPON);
}

void CTaskWeapon::GetOverriddenGameplayCameraSettings(tGameplayCameraSettings& settings) const
{
	const CPed *pPed = GetPed(); //Get the ped ptr.

	//Only request aim cameras for the local player, as they require control input and free-aim input/orientation is not synced for remote players.
	if(!pPed->IsLocalPlayer())
	{
		return;
	}

	if(pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_DISABLE_AIM_CAMERA))
	{
		return;
	}

	//Maintain default camera if swimming and not holding down aim
	if (CTaskGun::IsPlayerSwimmingAndNotAiming(pPed))
	{
		return; 
	}

#if FPS_MODE_SUPPORTED
	// Don't change to sniper scope in first person unless the aiming button is pressed and we are not combat rolling
	if(pPed->IsLocalPlayer() && pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && camInterface::GetGameplayDirector().IsFirstPersonModeEnabled() && 
		((!CPlayerInfo::IsAiming() && !CNewHud::IsShowingHUDMenu()) || pPed->GetMotionData()->GetCombatRoll()))
	{
		return;
	}
#endif // FPS_MODE_SUPPORTED

	//Inhibit all aim cameras during medium- and long-range Switch transitions.
	bool isPlayerSwitchActive = g_PlayerSwitch.IsActive();
	bool isShortPlayerSwitch = (g_PlayerSwitch.GetSwitchType() == CPlayerSwitchInterface::SWITCH_TYPE_SHORT);
	if(isPlayerSwitchActive && !isShortPlayerSwitch)
	{
		return;
	}


	CTaskAimGunOnFoot* pAimGunOnFootTask = static_cast<CTaskAimGunOnFoot*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_ON_FOOT));
	if(pAimGunOnFootTask && pAimGunOnFootTask->GetIsWeaponBlocked())
	{
		return;
	}

	const CWeaponInfo* weaponInfo = camGameplayDirector::ComputeWeaponInfoForPed(*pPed);

	if(taskVerifyf(weaponInfo, "Could not obtain a weapon info object for the ped's equipped weapon"))
	{
		//NOTE: We assume the aim state in the absence of a valid weapon controller, as this allows us to override the camera for aiming
		//projectiles.
		const WeaponControllerState controllerState	= CWeaponControllerManager::GetController(m_weaponControllerType)->GetState(pPed, false);
		const bool isAnAimCameraActive				= camInterface::GetGameplayDirector().IsAiming(pPed);

		//NOTE: Do not change camera if we are not aiming and have the weapon wheel up
		if (!isAnAimCameraActive && CNewHud::IsShowingHUDMenu())
		{
			return;
		}

		//NOTE: Reloading whilst aiming should maintain the camera aim state, but reloading prior to aiming should not initiate an aim camera.
		if(((controllerState == WCS_Aim) || (controllerState == WCS_Fire) || (controllerState == WCS_FireHeld) || ((controllerState == WCS_Reload) && isAnAimCameraActive)))
		{
			const bool shouldUseCinematicShooting = m_bShouldUseAssistedAimingCamera && CPauseMenu::GetMenuPreference(PREF_CINEMATIC_SHOOTING) FPS_MODE_SUPPORTED_ONLY( && !pPed->IsFirstPersonShooterModeEnabledForPlayer());

			if(shouldUseCinematicShooting)
			{
				settings.m_CameraHash = weaponInfo->GetCinematicShootingCameraHash();

				const CTaskGun::Tunables& gunTaskTunables = CTaskGun::GetTunables();
				settings.m_InterpolationDuration = gunTaskTunables.m_AssistedAimInterpolateInDuration;

				if (settings.m_CameraHash)
				{
					return;
				}
			}

			if (m_bShouldUseRunAndGunAimingCamera || shouldUseCinematicShooting)
			{
				settings.m_CameraHash = weaponInfo->GetRunAndGunCameraHash();

				const CTaskGun::Tunables& gunTaskTunables = CTaskGun::GetTunables();
				settings.m_InterpolationDuration = gunTaskTunables.m_RunAndGunInterpolateInDuration;

				return;
			}

			//NOTE: We only request a first-person aim camera when the weapon is ready to fire (or reloading), as we don't interpolate into
			//first-person aim cameras.
			//NOTE: We explicitly exclude melee weapons here, as melee cameras should be requested by the melee combat task.
			weaponAssert(pPed->GetWeaponManager());
			const CWeapon* weapon				= pPed->GetWeaponManager()->GetEquippedWeapon();
			const bool hasFirstPersonScope		= weapon && weapon->GetHasFirstPersonScope();

			//Inhibit first person aim cameras during short-range Switch transitions, unless specifically allowed via the control flags.
			if(hasFirstPersonScope && isPlayerSwitchActive && isShortPlayerSwitch)
			{
				const CPlayerSwitchMgrShort& shortRangeSwitchManager	= g_PlayerSwitch.GetShortRangeMgr();
				const CPlayerSwitchParams& switchParams					= shortRangeSwitchManager.GetParams();
				const CPlayerSwitchMgrShort::eState switchState			= shortRangeSwitchManager.GetState();
				const bool shouldAllowSniperAim							= ((switchState == CPlayerSwitchMgrShort::STATE_INTRO) &&
																			(switchParams.m_controlFlags & CPlayerSwitchInterface::CONTROL_FLAG_ALLOW_SNIPER_AIM_INTRO)) ||
																			((switchState == CPlayerSwitchMgrShort::STATE_OUTRO) &&
																			(switchParams.m_controlFlags & CPlayerSwitchInterface::CONTROL_FLAG_ALLOW_SNIPER_AIM_OUTRO));
				if(!shouldAllowSniperAim)
				{
					return;
				}
			}

			// Don't allow sniper hud up when doing an idle reload because we won't be in aim strafe, B*1595934
			if (hasFirstPersonScope)
			{
				const CTaskReloadGun* pReloadGunTask = static_cast<const CTaskReloadGun*>(FindSubTaskOfType(CTaskTypes::TASK_RELOAD_GUN));
				if (pReloadGunTask && pReloadGunTask->IsReloadFlagSet(RELOAD_IDLE))
				{
					return;
				}
			}

			const bool isReadyToFire			= GetIsReadyToFire();
			const bool isReloading				= GetIsReloading();
			const bool isMeleeWeapon			= weaponInfo->GetIsMelee();
			const bool shouldRequestAnAimCamera	= !isMeleeWeapon && (!hasFirstPersonScope || isReadyToFire || isReloading);
			if(shouldRequestAnAimCamera)
			{
				if(pPed->GetCoverPoint())
				{
					const u32 coverReadyToFireCameraHash = weaponInfo->GetCoverReadyToFireCameraHash();
					if(coverReadyToFireCameraHash)
					{
						//Allow a weapon scope component to override the camera hash, as in CWeapon::GetDefaultCameraHash(),
						//but only when we have a valid 'cover ready to fire' camera hash to begin with.
						const CWeaponComponentScope* weaponScopeComponent = weapon ? weapon->GetScopeComponent() : NULL;
						if (weaponScopeComponent)
						{
							if(pPed->GetIsInCover() && pPed->IsFirstPersonShooterModeEnabledForPlayer(false)
								&& pPed->GetMotionData()->GetFPSState() != CPedMotionData::FPS_LT
								&& pPed->GetMotionData()->GetFPSState() != CPedMotionData::FPS_SCOPE)
							{
								//Do not override the camera.
								return;
							}
							else if(!hasFirstPersonScope)
							{
								// We have a 'cover ready to fire' camera hash (only set on sniper weapons), but we don't have a first person scope (reticule hash on the component is zero)?
								// That must mean we're using a normal scope on a weapon that's usually a sniper weapon, so return without overriding the camera. 
								// This will leave the camera using the weapon's CoverCameraHash set from the higher CTaskCover (DEFAULT_THIRD_PERSON_PED_AIM_IN_COVER_CAMERA).
								return;
							}
							else
							{
								settings.m_CameraHash = weaponScopeComponent->GetCameraHash();
							}
						}
						else
						{
							settings.m_CameraHash = coverReadyToFireCameraHash;
						}
					}
				}
				else
				{
					settings.m_CameraHash = weapon ? weapon->GetDefaultCameraHash() : weaponInfo->GetDefaultCameraHash(); //Fall-back to the default aim camera.
				}
			}
		}
	}
}

bool CTaskWeapon::GetIsReadyToFire() const
{
	switch(GetState())
	{
	case State_Gun:
		{
			const CTaskGun* pTaskGun = static_cast<const CTaskGun*>(GetSubTask());
			if(pTaskGun)
			{
				taskFatalAssertf(GetSubTask()->GetTaskType() == CTaskTypes::TASK_GUN, "Subtask is not of type Gun!");
				return pTaskGun->GetIsReadyToFire();
			}
		}
		break;

	case State_Projectile:
		{
			const CTaskAimAndThrowProjectile* pTaskProjectile = static_cast<const CTaskAimAndThrowProjectile*>(GetSubTask());
			if(pTaskProjectile)
			{
				taskFatalAssertf(GetSubTask()->GetTaskType() == CTaskTypes::TASK_AIM_AND_THROW_PROJECTILE, "Subtask is not of type Gun!");
				return pTaskProjectile->GetIsReadyToFire();
			}
		}
		break;

	default:
		break;
	}
	
	return false;
}

bool CTaskWeapon::GetIsReloading() const
{
	switch(GetState())
	{
	case State_Gun:
		{
			const CTaskGun* pTaskGun = static_cast<const CTaskGun*>(GetSubTask());
			if(pTaskGun)
			{
				return pTaskGun->GetIsReloading();
			}
		}
		break;

	default:
		break;
	}

	return false;
}

CTaskWeapon::FSM_Return CTaskWeapon::ProcessPreFSM()
{
	// If we already had assisted aiming on (m_bShouldUseAssistedAimingCamera is true) then force it to persist so that we don't trigger
	// an aim cam when we stop aiming
	m_bShouldUseAssistedAimingCamera = CTaskGun::ProcessUseAssistedAimingCamera(*GetPed(), FPS_MODE_SUPPORTED_ONLY(!GetPed()->IsFirstPersonShooterModeEnabledForPlayer() &&) m_bShouldUseAssistedAimingCamera);
	if (!m_bShouldUseAssistedAimingCamera)
	{
		m_bShouldUseRunAndGunAimingCamera = CTaskGun::ProcessUseRunAndGunAimingCamera(*GetPed(), FPS_MODE_SUPPORTED_ONLY(!GetPed()->IsFirstPersonShooterModeEnabledForPlayer() &&) m_bShouldUseRunAndGunAimingCamera);
	}
	return FSM_Continue;
}

CTaskWeapon::FSM_Return CTaskWeapon::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		FSM_State(State_Init)
			FSM_OnUpdate
				return StateInitOnUpdate(pPed);
		FSM_State(State_Gun)
			FSM_OnEnter
				return StateGunOnEnter(pPed);
			FSM_OnUpdate
				return StateGunOnUpdate(pPed);
		FSM_State(State_Projectile)
			FSM_OnEnter
				return StateProjectileOnEnter(pPed);
			FSM_OnUpdate
				return StateProjectileOnUpdate(pPed);
		FSM_State(State_Bomb)
			FSM_OnEnter
				return StateBombOnEnter(pPed);
			FSM_OnUpdate
				return StateBombOnUpdate(pPed);
		FSM_State(State_Detonator)
			FSM_OnEnter
				return StateDetonatorOnEnter(pPed);
			FSM_OnUpdate
				return StateDetonatorOnUpdate(pPed);
		FSM_State(State_Quit)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

#if !__FINAL
const char * CTaskWeapon::GetStaticStateName( s32 iState )
{
	static const char* aStateNames[] = 
	{
		"Init",
		"Gun",
		"Projectile",
		"Bomb",
		"Detonator",
		"Quit",
	};

	return aStateNames[iState];
}
#endif // !__FINAL

CTaskWeapon::FSM_Return CTaskWeapon::StateInitOnUpdate(CPed* pPed)
{
	// Ensure we have a chosen weapon
	weaponAssert(pPed->GetWeaponManager());
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(pPed->GetWeaponManager()->GetEquippedWeaponHash());
	
	bool bFPSMode = false;
#if FPS_MODE_SUPPORTED
	bFPSMode = GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false);
	const CWeaponInfo* pWeaponInfoFromObject = pPed->GetWeaponManager()->GetEquippedWeaponInfoFromObject();
#endif 

	if(!pWeaponInfo)
	{
		taskDebugf1("CTaskWeapon::StateInitOnUpdate: FSM_Quit - !pWeaponInfo. Previous State: %d", GetPreviousState());
		return FSM_Quit;
	}

	// If the player can't duck whilst using this weapon, reset the duck state
	if(!pPed->CanPedCrouch() && pPed->GetIsCrouching())
	{
		pPed->SetIsCrouching(false);
	}

	if(pWeaponInfo->GetIsBomb())
	{
		SetState(State_Bomb);
	}
	// Is the weapon a projectile? (...and not running trying to aim gun & throw projectile)
	else if(pWeaponInfo->GetIsProjectile() && pWeaponInfo->GetIsThrownWeapon() && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ThrowingGrenadeWhileAiming))
	{
		if (!bFPSMode || (pWeaponInfoFromObject && pWeaponInfoFromObject->GetIsProjectile() && pWeaponInfoFromObject->GetIsThrownWeapon()))
		{
			SetState(State_Projectile);
		}
		else
		{
			return FSM_Continue;
		}
	}
	// Check the firing state aswell (this would trigger if holding aim using an ak, then given the detonator see 35877, this is due to taskplayeronfoot checking the equipped weapon object)
	else if(CWeaponControllerManager::GetController(m_weaponControllerType)->GetState(pPed, false) == WCS_Fire && pWeaponInfo->GetIsDetonator())
	{
		SetState(State_Detonator);
	}
	else
	{
		AI_LOG_IF_PLAYER(pPed, "CTaskWeapon::StateInitOnUpdate - Setting State_Gun for ped [%s] \n", AILogging::GetDynamicEntityNameSafe(pPed));
		SetState(State_Gun);
	}

	return FSM_Continue;
}

CTaskWeapon::FSM_Return CTaskWeapon::StateGunOnEnter(CPed* pPed)
{
	CTaskGun* pTaskGun = rage_new CTaskGun(m_weaponControllerType, m_aimTaskType, m_target, m_fDuration);

	if (pPed->IsLocalPlayer())
	{
		if (pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_FORCE_SKIP_AIM_INTRO))
		{
			m_iFlags.SetFlag(GF_SkipIdleTransitions);
		}

		pPed->ClearPlayerResetFlag(CPlayerResetFlags::PRF_FORCE_SKIP_AIM_INTRO);
	}

	pTaskGun->SetFiringPatternHash(m_uFiringPatternHash);
	pTaskGun->GetGunFlags().SetFlag(m_iFlags);
	SetNewTask(pTaskGun);
	return FSM_Continue;
}

CTaskWeapon::FSM_Return CTaskWeapon::StateGunOnUpdate(CPed* UNUSED_PARAM(pPed))
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		taskDebugf1("CTaskWeapon::StateGunOnUpdate: FSM_Quit - SubTaskFinished. Previous State: %d", GetPreviousState());
		return FSM_Quit;
	}

	return FSM_Continue;
}

CTaskWeapon::FSM_Return CTaskWeapon::StateProjectileOnEnter(CPed* UNUSED_PARAM(pPed))
{
	SetNewTask(rage_new CTaskAimAndThrowProjectile());
	return FSM_Continue;
}

CTaskWeapon::FSM_Return CTaskWeapon::StateProjectileOnUpdate(CPed* pPed)
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		// Swap weapons if new weapon is a gun, projectiles can't handle swaps
		CPedWeaponManager* pWeapMgr = pPed->GetWeaponManager();
		if(pWeapMgr)
		{
			if(pWeapMgr->GetRequiresWeaponSwitch())
			{
				const CWeaponInfo* pInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(pWeapMgr->GetEquippedWeaponHash());
				if(pInfo && pInfo->GetIsGunOrCanBeFiredLikeGun())
				{
					if(!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_SwimmingTasksRunning) || pInfo->GetUsableUnderwater())
					{
						// Ensure weapon is streamed in
						if(pPed->GetInventory() && pPed->GetInventory()->GetIsStreamedIn(pInfo->GetHash()))
						{
							if(pInfo->GetIsCarriedInHand() || pWeapMgr->GetEquippedWeaponObject() != NULL)
							{
								// 
								taskDebugf1("CTaskWeapon::StateProjectileOnUpdate: SetState(State_Init) - Swap weapons. Previous State: %d", GetPreviousState());
								SetState(State_Init);
								return FSM_Continue;
							}
						}
					}				
				}
			}
		}
		taskDebugf1("CTaskWeapon::StateProjectileOnUpdate: FSM_Quit - SubTaskFinished. Previous State: %d", GetPreviousState());
		return FSM_Quit;
	}

	return FSM_Continue;
}

CTaskWeapon::FSM_Return CTaskWeapon::StateBombOnEnter(CPed* pPed)
{
	// Ensure we have a chosen weapon
	weaponAssert(pPed->GetWeaponManager());
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(pPed->GetWeaponManager()->GetEquippedWeaponHash());
	if(!pWeaponInfo)
	{
		taskDebugf1("CTaskWeapon::StateBombOnEnter: FSM_Quit - !pWeaponInfo. Previous State: %d", GetPreviousState());
		return FSM_Quit;
	}

	CTaskBomb* pTaskBomb = rage_new CTaskBomb(m_weaponControllerType, m_fDuration);
	SetNewTask(pTaskBomb);

	return FSM_Continue;
}

CTaskWeapon::FSM_Return CTaskWeapon::StateBombOnUpdate(CPed* UNUSED_PARAM(pPed))
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		taskDebugf1("CTaskWeapon::StateBombOnUpdate: FSM_Quit - SubTaskFinished. Previous State: %d", GetPreviousState());
		return FSM_Quit;
	}

	return FSM_Continue;
}

CTaskWeapon::FSM_Return CTaskWeapon::StateDetonatorOnEnter(CPed* UNUSED_PARAM(pPed))
{
	CTaskDetonator* pTaskDetonator = rage_new CTaskDetonator(m_weaponControllerType, m_fDuration);
	SetNewTask(pTaskDetonator);

	return FSM_Continue;
}

CTaskWeapon::FSM_Return CTaskWeapon::StateDetonatorOnUpdate(CPed* UNUSED_PARAM(pPed))
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		taskDebugf1("CTaskWeapon::StateDetonatorOnUpdate: FSM_Quit - SubTaskFinished. Previous State: %d", GetPreviousState());
		return FSM_Quit;
	}

	return FSM_Continue;
}
