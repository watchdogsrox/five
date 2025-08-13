// File header
#include "Task/Weapons/WeaponController.h"

// Game headers
#include "Camera/CamInterface.h"
#include "Camera/Gameplay/GameplayDirector.h"
#include "FrontEnd/NewHud.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Network/Network.h"
#include "Task/Default/TaskPlayer.h"
#include "Weapons/Weapon.h"
#include "Weapons/Info/WeaponInfo.h"

// Macro to disable optimisations if set
AI_OPTIMISATIONS()
AI_WEAPON_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CWeaponControllerPlayer
//////////////////////////////////////////////////////////////////////////

WeaponControllerState CWeaponControllerPlayer::GetState(const CPed* pPed, const bool bFireAtLeastOnce) const
{
	if(pPed->IsAPlayerPed())
	{
		if( CPlayerInfo::IsCombatRolling() )
		{
			return WCS_CombatRoll;
		}
		else
		{
			if(CPlayerInfo::IsReloading())
			{
				// Get the weapon
				weaponAssert(pPed->GetWeaponManager());
				const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
				if(pWeapon && pWeapon->GetCanReload())
				{
					return WCS_Reload;
				}
			}
			
			// Is ped firing?
			if((pPed->GetPlayerInfo()->IsFiring() || pPed->GetPedResetFlag(CPED_RESET_FLAG_ForcePlayerFiring) || bFireAtLeastOnce) && !CNewHud::IsShowingHUDMenu())
			{
#if USE_TARGET_SEQUENCES
				CTargetSequenceHelper* pTargetHelper = pPed->FindActiveTargetSequence();
				if( pTargetHelper && !pTargetHelper->IsReadyToFire() )
				{
					return WCS_Aim;
				}
#endif  // USE_TARGET_SEQUENCES

				return WCS_Fire;
			}
			// Don't consider soft firing as aiming in cover and ignore free aim checks
			else if (pPed->GetIsInCover())
			{
				if (CPlayerInfo::IsAiming(false) || pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_FORCED_AIMING) || CNewHud::IsShowingHUDMenu())
				{
					return WCS_Aim;
				}
			}
			// If weapon wheel is active when we're in the gun task, have the aiming persist
			else if(CPlayerInfo::IsAiming() || pPed->GetPlayerInfo()->GetPlayerDataFreeAiming() || pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_FORCED_AIMING) || CNewHud::IsShowingHUDMenu())
			{
				return WCS_Aim;
			}
			else if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InToStrafeTransition))
			{
				return WCS_Aim;
			}
		}
	}

#if FPS_MODE_SUPPORTED
	if(pPed->IsLocalPlayer() && !pPed->GetPedIntelligence()->GetTaskClimbLadder() && pPed->IsFirstPersonShooterModeEnabledForPlayer(false, false, true))
	{
		if (!pPed->GetPedResetFlag(CPED_RESET_FLAG_IsAimingFromCover))
		{
			return WCS_Aim;
		}
		else if (CTaskCover::IsPlayerAimingDirectlyInFirstPerson(*pPed))
		{
			return WCS_Aim;
		}
		
		// Prevent infinite loop
		const CTaskPlayerOnFoot* pPlayerOnFootTask = static_cast<const CTaskPlayerOnFoot*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PLAYER_ON_FOOT));
		if (pPlayerOnFootTask && pPlayerOnFootTask->GetPlayerExitedCoverThisUpdate())
		{
			return WCS_Aim;
		}
	}
#endif // FPS_MODE_SUPPORTED

	return WCS_None;
}


////////////////////////////////////////////////////////////////////////////////
// CWeaponControllerManager
////////////////////////////////////////////////////////////////////////////////

CWeaponController* CWeaponControllerManager::ms_WeaponControllers[CWeaponController::WCT_Max] = { NULL, NULL, NULL, NULL, NULL, NULL };

void CWeaponControllerManager::Init()
{
	ms_WeaponControllers[CWeaponController::WCT_Player] = rage_new CWeaponControllerPlayer();
	ms_WeaponControllers[CWeaponController::WCT_Aim] = rage_new CWeaponControllerFixed(WCS_Aim);
	ms_WeaponControllers[CWeaponController::WCT_Fire] = rage_new CWeaponControllerFixed(WCS_Fire);
	ms_WeaponControllers[CWeaponController::WCT_FireHeld] = rage_new CWeaponControllerFixed(WCS_FireHeld);
	ms_WeaponControllers[CWeaponController::WCT_Reload] = rage_new CWeaponControllerFixed(WCS_Reload);
	ms_WeaponControllers[CWeaponController::WCT_None] = rage_new CWeaponControllerFixed(WCS_None);
}

////////////////////////////////////////////////////////////////////////////////

void CWeaponControllerManager::Shutdown()
{
	for(s32 i = 0; i < CWeaponController::WCT_Max; i++)
	{
		delete ms_WeaponControllers[i];
		ms_WeaponControllers[i] = NULL;
	}
}
