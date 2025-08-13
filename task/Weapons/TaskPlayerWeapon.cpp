// Class header
#include "Task/Weapons/TaskPlayerWeapon.h"

// Framework headers
#include "ai/task/taskchannel.h"

// Game headers
#include "Debug/DebugScene.h"
#include "Event/EventWeapon.h"
#include "Frontend/PauseMenu.h"
#include "Frontend/MobilePhone.h"
#include "PedGroup/PedGroup.h"
#include "Peds/PedIntelligence.h"
#include "Peds/Ped.h"
#include "Physics/Physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "system/control.h"
#include "Task/Combat/Cover/TaskCover.h"
#include "Task/Combat/TaskCombatMelee.h"
#include "Task/Combat/TaskThreatResponse.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/Weapons/Gun/GunFlags.h"
#include "Task/Weapons/TaskProjectile.h"
#include "Task/Weapons/TaskWeapon.h"
#include "Task/Weapons/TaskWeaponBlocked.h"
#include "Task/Weapons/WeaponController.h"
#include "Task/Weapons/Gun/TaskAimGun.h"
#include "Task/Weapons/Gun/TaskAimGunOnFoot.h"
#include "Vehicles/Vehicle.h"
#include "Weapons/Weapon.h"

// Macro to disable optimisations if set
AI_OPTIMISATIONS()
AI_WEAPON_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CTaskPlayerWeapon
//////////////////////////////////////////////////////////////////////////

// Static initialisation
u32 CTaskPlayerWeapon::ms_uiFrameLastSwitchedCrouchedState = 0;

CTaskPlayerWeapon::CTaskPlayerWeapon(s32 iGunFlags)
: m_bDoingRunAndGun(false)
, m_bSkipIdleTransition(false)
, m_iGunFlags(iGunFlags)
{
	SetInternalTaskType(CTaskTypes::TASK_PLAYER_WEAPON);
}

void CTaskPlayerWeapon::CleanUp()
{
	// Cleanup the strafing flag
	CPed* pPed = GetPed();
	if(pPed)
	{
		pPed->SetIsStrafing(pPed->GetPedResetFlag(CPED_RESET_FLAG_ForcePedToStrafe));
	}

	CTask::CleanUp();
}

CTaskPlayerWeapon::FSM_Return CTaskPlayerWeapon::ProcessPreFSM()
{
	CPed *pPed = GetPed(); //Get the ped ptr.

#if __ASSERT
	if(!pPed->IsAPlayerPed())
	{
		Assert(pPed->IsAPlayerPed());
		return FSM_Quit;
	}
#endif // __ASSERT

	// Update the peds movement task
	UpdateMovementTask(pPed);

	// Update our flags
	m_bDoingRunAndGun = pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_RUN_AND_GUN) ? true : false;

	pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableCellphoneAnimations, true );
	pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPostPreRender, true );
	CPhoneMgr::SetRemoveFlags(CPhoneMgr::WANTS_PHONE_REMOVED_WEAPON_SWITCHING_S1);

	return FSM_Continue;
}

CTaskPlayerWeapon::FSM_Return CTaskPlayerWeapon::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		FSM_State(State_Weapon)
			FSM_OnEnter
				return Weapon_OnEnter();
			FSM_OnUpdate
				return Weapon_OnUpdate(pPed);
		FSM_State(State_Quit)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

#if !__FINAL
const char * CTaskPlayerWeapon::GetStaticStateName( s32 iState )
{
	static const char* aStateNames[] = 
	{
		"State_Weapon",
		"State_Quit",
	};

	return aStateNames[iState];
}
#endif // !__FINAL

CTaskPlayerWeapon::FSM_Return CTaskPlayerWeapon::Weapon_OnEnter()
{
	s32 iFlags = GetGunFlags();
	if (!CTaskAimGun::ms_bUseTorsoIk)
		iFlags |= GF_DisableTorsoIk;

	iFlags |= GF_WaitForFacingAngleToFire;
	SetNewTask(rage_new CTaskWeapon(CWeaponController::WCT_Player, CTaskTypes::TASK_AIM_GUN_ON_FOOT, iFlags, GetFiringPatternHash()));
	return FSM_Continue;
}

CTaskPlayerWeapon::FSM_Return CTaskPlayerWeapon::Weapon_OnUpdate(CPed*)
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		taskDebugf1("CTaskPlayerWeapon::Weapon_OnUpdate: FSM_Quit - SubTaskFinished. Previous State: %d", GetPreviousState());
		return FSM_Quit;
	}

	return FSM_Continue;
}

void CTaskPlayerWeapon::UpdateMovementTask(CPed* pPed)
{
	taskAssert(pPed && pPed->GetPedIntelligence());

	// No movement for sniper rifles
// 	const bool bReloadingButNotZoomed =
// 		pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_RELOAD_GUN) && 
// 		!(CPlayerInfo::IsSoftAiming() || CPlayerInfo::IsHardAiming());

	taskAssert(pPed->GetWeaponManager());
	const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
//	const bool bAssistedAimingOrRunAndGunOn = pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON) || pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_RUN_AND_GUN);
// 	if(pWeapon && pWeapon->GetHasFirstPersonScope() && !bReloadingButNotZoomed && !bAssistedAimingOrRunAndGunOn)
// 	{
// 		// However, we do want the player to be able to toggle between crouching states
// 		// Do this here directly, rather than going via a movement task
// 		const CControl* pControl = pPed->GetControlFromPlayer();
// 		taskAssert(pControl);
// 
// 		// Make sure its not already switched this frame
// 		if(!CPlayerInfo::IsAiming() && pControl->GetPedDuck().IsPressed() && ms_uiFrameLastSwitchedCrouchedState != fwTimer::GetSystemFrameCount())
// 		{
// 			ms_uiFrameLastSwitchedCrouchedState = fwTimer::GetSystemFrameCount();
// 			if(pPed->GetIsCrouching())
// 			{
// 				pPed->SetIsCrouching(false);
// 			}
// 			else if(pPed->CanPedCrouch())
// 			{
// 				pPed->SetIsCrouching(true);
// 			}
// 		}
// 	}
// 	else
	{		
		// It might not be valid to cast to CTaskMovePlayer here (e.g. c4 tasks)
		CTask* pMovementTask = pPed->GetPedIntelligence()->GetGeneralMovementTask();
		if(!pMovementTask)
		{
			pMovementTask = rage_new CTaskMovePlayer();
			pPed->GetPedIntelligence()->AddTaskMovement(pMovementTask);
		}
		taskAssert(pMovementTask);

		if(m_bDoingRunAndGun)
		{
		}
		else if (!pMovementTask || pMovementTask->GetTaskType() != CTaskTypes::TASK_MOVE_GOTO_POINT_STAND_STILL_ACHIEVE_HEADING)
		{
			taskAssert(pPed->GetWeaponManager());
			const CWeaponInfo* pSelectedWeaponInfo = pPed->GetWeaponManager()->GetEquippedWeaponInfo();
			if(!pSelectedWeaponInfo || !pSelectedWeaponInfo->GetIgnoreStrafing())
			{
				if(pMovementTask->GetTaskType() != CTaskTypes::TASK_MOVE_PLAYER || pMovementTask->GetIsFlagSet(aiTaskFlags::HasFinished))
				{
					// Restart the move task
					pMovementTask = rage_new CTaskMovePlayer();
					pPed->GetPedIntelligence()->AddTaskMovement(pMovementTask);
				}

				taskAssert(pPed->GetPedIntelligence()->GetGeneralMovementTask());
				taskAssert(pPed->GetPedIntelligence()->GetGeneralMovementTask()->GetTaskType() == CTaskTypes::TASK_MOVE_PLAYER);

				// Set strafing
				// We will be strafing during this task except when reloading, and not holding aim OR fire buttons
				if(GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_PLAYER_ON_FOOT)
				{
					//const bool bProjectileWeapon    = (pWeapon && pWeapon->GetWeaponInfo()) ? pWeapon->GetWeaponInfo()->GetIsThrownWeapon() : false;
					const bool bProjectileWeapon    = pSelectedWeaponInfo ? pSelectedWeaponInfo->GetIsThrownWeapon() : false;					
					const bool bConsiderFireTrigger = !bProjectileWeapon;
					
					taskAssert(pPed->GetPlayerInfo());
					const bool bAimingOrFiring      = pPed->GetPedResetFlag(CPED_RESET_FLAG_ForcePlayerFiring) || pPed->GetPlayerInfo()->IsAiming(bConsiderFireTrigger) || (bConsiderFireTrigger && pPed->GetPlayerInfo()->IsFiring()) FPS_MODE_SUPPORTED_ONLY(|| pPed->IsFirstPersonShooterModeEnabledForPlayer());
					const bool bIsReloading         = FindSubTaskOfType(CTaskTypes::TASK_RELOAD_GUN) || FindSubTaskOfType(CTaskTypes::TASK_SWAP_WEAPON);
					const bool bIsAssistedAiming	= FindSubTaskOfType(CTaskTypes::TASK_GUN) ? static_cast<CTaskGun*>(FindSubTaskOfType(CTaskTypes::TASK_GUN))->GetIsAssistedAiming() : false;										
					
					bool bStrafing = true;
					if(((!bAimingOrFiring && bIsReloading && !bIsAssistedAiming) || (pWeapon && pWeapon->GetDefaultCameraHash() == 0)))
					{
						bStrafing  = false;
					}
					else if (bProjectileWeapon)
					{
						CTaskAimAndThrowProjectile* pThrowTask = static_cast<CTaskAimAndThrowProjectile*>(FindSubTaskOfType(CTaskTypes::TASK_AIM_AND_THROW_PROJECTILE));

						if (!bAimingOrFiring FPS_MODE_SUPPORTED_ONLY( && !pPed->IsFirstPersonShooterModeEnabledForPlayer(false)))
						{
							bool bFalseAiming = pThrowTask && pThrowTask->IsFalseAiming();
							bool bCombatRolling = pThrowTask && (pThrowTask->GetState() == CTaskAimAndThrowProjectile::State_CombatRoll);
							if (!bFalseAiming && !bCombatRolling && ((pThrowTask && (pThrowTask->IsDroppingProjectile() || pThrowTask->GetState()==CTaskAimAndThrowProjectile::State_Outro)) || pPed->GetPlayerInfo()->IsFiring()))
							{								
								bStrafing  = false;
							}
						} 
						else if (pThrowTask && pThrowTask->IsDroppingProjectile() && pThrowTask->GetState()==CTaskAimAndThrowProjectile::State_ThrowProjectile
							     FPS_MODE_SUPPORTED_ONLY( && !pPed->IsFirstPersonShooterModeEnabledForPlayer(false)))
						{								
							bStrafing  = false;
						}						
					}
					pPed->SetIsStrafing(bStrafing);
				}
				else
				{
					pPed->SetIsStrafing(true);
				}
			}
			
			// Mark the movement task as still in use this frame
			CTaskMoveInterface* pMoveInterface = pMovementTask->GetMoveInterface();
			if(pMoveInterface)
			{
				pMoveInterface->SetCheckedThisFrame(true);
				NOTFINAL_ONLY(pMoveInterface->SetOwner(this));
			}
		}
	}
}

u32 CTaskPlayerWeapon::GetFiringPatternHash() const
{
	u32 uFiringPatternHash = FIRING_PATTERN_FULL_AUTO;

#if __DEV
	static dev_u32 FIRING_PATTERN_OVERRIDE = 0;
	if(FIRING_PATTERN_OVERRIDE != 0)
	{
		uFiringPatternHash = FIRING_PATTERN_OVERRIDE;
	}
#endif // __DEV

	return uFiringPatternHash;
}

s32 CTaskPlayerWeapon::GetGunFlags() const
{
	const CPed* pPed = GetPed();

	s32 iFlags = m_iGunFlags;
	
	iFlags |= GF_DontUpdateHeading | GF_FireAtIkLimitsWhenReached | GF_UpdateTarget;

	// We're going to swap the gun instantly and be in a position to fire immediately
	if(pPed->IsLocalPlayer())
	{
		aiTask* pTask = pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PLAYER_ON_FOOT);
		if (pTask)
		{
			if(static_cast<CTaskPlayerOnFoot*>(pTask)->GetShouldStartGunTask())
			{
				iFlags |= GF_SkipIdleTransitions;
			}
		}
	}

	if (m_bSkipIdleTransition)
	{
		iFlags |= GF_SkipIdleTransitions;
	}
	return iFlags;
}
