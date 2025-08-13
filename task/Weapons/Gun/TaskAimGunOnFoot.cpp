//
// Task/Weapons/Gun/TaskAimGunOnFoot.h
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

// File header
#include "Task/Weapons/Gun/TaskAimGunOnFoot.h"

// Rage headers
#include "ai/task/taskchannel.h"
#include "crskeleton/skeleton.h"

// framework headers
#include "fwanimation/directorcomponentragdoll.h"

// Game Headers
#include "Animation/EventTags.h"
#include "Animation/MovePed.h"
#include "Audio/ScriptAudioEntity.h"
#include "Camera/CamInterface.h"
#include "Camera/gameplay/aim/ThirdPersonAimCamera.h"
#include "Camera/gameplay/GameplayDirector.h"
#include "Camera/gameplay/aim/FirstPersonAimCamera.h"
#include "Camera/gameplay/aim/FirstPersonShooterCamera.h"
#include "Camera/scripted/ScriptDirector.h"
#include "Debug/DebugScene.h"
#include "FrontEnd/NewHud.h"
#include "network/general/NetworkPendingProjectiles.h"
#include "Network/NetworkInterface.h"
#include "Network/Objects/Entities/NetObjPlayer.h"
#include "ik/IkManagerSolverTypes.h"
#include "Peds/Ped.h"
#include "Peds/PedGeometryAnalyser.h"
#include "Peds/PedIKSettings.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PedWeapons/PedTargetEvaluator.h"
#include "Physics/WorldProbe/WorldProbe.h"
#include "system/ControlMgr.h"
#include "Task/Combat/TaskCombatMelee.h"
#include "Task/Combat/TaskNewCombat.h"
#include "Task/Combat/Cover/TaskCover.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/General/TaskBasic.h"
#include "Task/Motion/TaskMotionBase.h"
#include "Task/Motion/Locomotion/TaskCombatRoll.h"
#include "Task/Motion/Locomotion/TaskLocomotion.h"
#include "Task/Motion/Locomotion/TaskMotionAiming.h"
#include "Task/Motion/Locomotion/TaskMotionPed.h"
#include "Task/Weapons/Gun/GunFlags.h"
#include "Task/Weapons/Gun/TaskGun.h"
#include "Task/Weapons/Gun/TaskVehicleDriveBy.h"
#include "Task/Weapons/TaskProjectile.h"
#include "Task/Weapons/TaskWeaponBlocked.h"
#include "Vfx/vehicleglass/VehicleGlassManager.h"
#include "Vfx/VfxHelper.h"
#include "weapons/projectiles/ProjectileManager.h"
#include "weapons/AirDefence.h"
#include "Weapons/Weapon.h"
#include "Weapons/info/WeaponInfo.h"

// Macro to disable optimisations if set
AI_OPTIMISATIONS()
AI_WEAPON_OPTIMISATIONS()
NETWORK_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////

// Statics
const fwMvStateId CTaskAimGunOnFoot::ms_FireOnlyStateId("FireLoopMedOnly",0x9B244FA7);
const fwMvStateId CTaskAimGunOnFoot::ms_FiringStateId("Firing",0x544C888B);
const fwMvStateId CTaskAimGunOnFoot::ms_AimingStateId("Aiming",0x9A5B381);
const fwMvStateId CTaskAimGunOnFoot::ms_AimStateId("Aim State",0x3EC78C3);
const fwMvFlagId CTaskAimGunOnFoot::ms_FiringFlagId("Firing",0x544C888B);
const fwMvFlagId CTaskAimGunOnFoot::ms_HasCustomFireLoopFlagId("HasCustomFireLoop",0x62EF967C);
const fwMvFlagId CTaskAimGunOnFoot::ms_UseIntroId("UseIntro",0xE167CF2D);
const fwMvFlagId CTaskAimGunOnFoot::ms_FireLoop1FinishedCacheId("FireLoop1FinishedCache",0x6D889C9D);
const fwMvFlagId CTaskAimGunOnFoot::ms_FireLoop2FinishedCacheId("FireLoop2FinishedCache",0xC568EDFD);
const fwMvBooleanId CTaskAimGunOnFoot::ms_AimOnEnterStateId("Aiming_OnEnter",0x3990C9D8);
const fwMvBooleanId CTaskAimGunOnFoot::ms_FiringOnEnterStateId("Firing_OnEnter",0xD7836AC2);
const fwMvBooleanId CTaskAimGunOnFoot::ms_AimIntroOnEnterStateId("AimingIntro_OnEnter",0x4454AF47);
const fwMvBooleanId CTaskAimGunOnFoot::ms_EnteredFireLoopStateId("FireLoop_OnEnter",0xC2FAEB5E);
const fwMvBooleanId CTaskAimGunOnFoot::ms_AimingClipEndedId("AimingClipEnded",0xF97BAACD);
const fwMvBooleanId CTaskAimGunOnFoot::ms_FidgetClipEndedId("FidgetClipEnded",0x9e3bf337);
const fwMvBooleanId CTaskAimGunOnFoot::ms_FidgetClipNonInterruptibleId("FidgetClipNonInterruptible",0x93269422);
const fwMvBooleanId CTaskAimGunOnFoot::ms_FireId("Fire",0xD30A036B);
const fwMvBooleanId CTaskAimGunOnFoot::ms_InterruptSettleId("InterruptSettle",0x78A01364);
const fwMvBooleanId CTaskAimGunOnFoot::ms_SettleFinishedId("SettleFinished",0xBB9442F8);
const fwMvBooleanId CTaskAimGunOnFoot::ms_FireInterruptable("Interruptible",0x550A14CF);
const fwMvBooleanId CTaskAimGunOnFoot::ms_FireLoop1StateId("FireLoop1_OnEnter",0xC7C027DD);
const fwMvBooleanId CTaskAimGunOnFoot::ms_FireLoop2StateId("FireLoop2_OnEnter",0xA66EEAC9);
const fwMvBooleanId CTaskAimGunOnFoot::ms_FPSAimIntroTransition("FPSAimIntroTransition",0x7CA6884C);
const fwMvBooleanId CTaskAimGunOnFoot::ms_AimIntroInterruptibleId("AimIntroInterruptible",0xB99A476B);
const fwMvBooleanId CTaskAimGunOnFoot::ms_MoveRecoilEnd("RecoilEnd",0x1698D28);
const fwMvBooleanId CTaskAimGunOnFoot::ms_FireLoopRevolverEarlyOutReload("FireLoopRevolverEarlyOutReload",0x9f5da9a3);
const fwMvClipId CTaskAimGunOnFoot::ms_AimingClipId("AimingClip",0xA1C598FF);
const fwMvClipId CTaskAimGunOnFoot::ms_CustomFireLoopClipId("CustomFireLoopClip",0x482FDC01);
const fwMvClipId CTaskAimGunOnFoot::ms_FireMedLoopClipId("fire_med_clip",0xFA21D0C);
const fwMvFloatId CTaskAimGunOnFoot::ms_FireRateId("Fire_Rate",0x225D53C3);
const fwMvFloatId CTaskAimGunOnFoot::ms_AnimRateId("AnimRate",0x77F5D0B5);
const fwMvFloatId CTaskAimGunOnFoot::ms_AimingBreathingAdditiveWeight("AimingBreathingAdditiveWeight",0x4BCEE31D);
const fwMvFloatId CTaskAimGunOnFoot::ms_AimingLeanAdditiveWeight("AimingLeanAdditiveWeight",0x8F882A7);
const fwMvFloatId CTaskAimGunOnFoot::ms_FPSIntroBlendOutTime("FPSIntroBlendOutTime", 0x6FC78399);
const fwMvFloatId CTaskAimGunOnFoot::ms_FPSIntroPlayRate("FPSIntroPlayRate", 0x5FA414AF);
const fwMvRequestId CTaskAimGunOnFoot::ms_RestartAimId("RestartAim",0xD6D1259D);
const fwMvRequestId CTaskAimGunOnFoot::ms_UseSettleId("UseSettle",0xEE79984E);
const fwMvRequestId CTaskAimGunOnFoot::ms_EmptyFire("EmptyFire",0x8D45E8EF);

const float CTaskAimGunOnFoot::ms_fTargetDistanceFromCameraForAimIk = 20.0f;

////////////////////////////////////////////////////////////////////////////////

CTaskAimGunOnFoot::Tunables CTaskAimGunOnFoot::sm_Tunables;

IMPLEMENT_WEAPON_TASK_TUNABLES(CTaskAimGunOnFoot, 0x3d1f4f8a);

////////////////////////////////////////////////////////////////////////////////

CTaskAimGunOnFoot::CTaskAimGunOnFoot(const CWeaponController::WeaponControllerType weaponControllerType, const fwFlags32& iFlags, const fwFlags32& iGFFlags, const CWeaponTarget& target, const CGunIkInfo& ikInfo, const u32 uBlockFiringTime)
: CTaskAimGun(weaponControllerType, iFlags, iGFFlags, target)
, m_uBlockFiringTime(uBlockFiringTime)
, m_bCrouching(false)
, m_bFire(false)
, m_bFiredOnce(false)
, m_bFiredTwice(false)
, m_bFiredOnceThisTriggerPress(false)
, m_bForceFire(false)
, m_bIsReloading(false)
, m_ikInfo(ikInfo)
, m_bCrouchStateChanged(false)
, m_bForceRestart(false)
, m_uNextShotAllowedTime(0)
, m_uLastUsingAimStickTime(0)
, m_bSuppressNextFireEvent(false)
, m_bUsingFiringVariation(false)
, m_bDoneIntro(false)
, m_bWantsToFireWhenWeaponReady(false)
, m_bDoneSetupToFire(false)
, m_bForceFiringState(false)
, m_bForceRoll(false)
, m_probeTimer(.1f)
, m_bLosClear(true)
, m_bCloneWeaponControllerFiring(false)
, m_bMoveInterruptSettle(false)
, m_bMoveFire(false)
, m_bMoveFireLoop1(false)
, m_bMoveFireLoop2(false)
, m_bMoveFireLoopRevolverEarlyOutReload(false)
, m_bMoveCustomFireLoopFinished(false)
, m_bMoveCustomFireIntroFinished(false)
, m_bMoveCustomFireOutroFinished(false)
, m_bMoveCustomFireOutroBlendOut(false)
, m_bMoveDontAbortFireIntro(false)
, m_bMoveFireLoop1Last(false)
, m_bMoveFireInterruptable(false)
, m_bMoveRecoilEnd(false)
, m_LastLocalRollToken(0)
, m_LastCloneRollToken(0)
, m_bUseAlternativeAimingWhenBlocked(false)
, m_LastThrowProjectileSequence(INVALID_TASK_SEQUENCE_ID)
, m_fBreathingAdditiveWeight(FLT_MAX)
, m_fLeanAdditiveWeight(FLT_MAX)
, m_fTimeTryingToFire(0.f)
, m_bFireWhenReady(false)
, m_previousControllerState(WCS_None)
, m_uQuitTimeWhenOutOfAmmoInScope(0)
, m_bHasSetQuitTimeWhenOutOfAmmoInScope(false)
, m_bOnCloneTaskNoLongerRunningOnOwner(false)
, m_WeaponObject(NULL)
#if ENABLE_GUN_PROJECTILE_THROW
, m_uQuickThrowWeaponHash(0)
, m_bTempWeaponHasScope(false)
, m_bProjectileThrownFromDPadInput(false)
#endif	//ENABLE_GUN_PROJECTILE_THROW
, m_bMoveAimIntroInterruptible(false)
#if FPS_MODE_SUPPORTED
, m_iFPSSwitchToScopeStartTime(0)
#endif	// FPS_MODE_SUPPORTED
, m_BulletsLeftAtStartOfFire(0)
{
	SetInternalTaskType(CTaskTypes::TASK_AIM_GUN_ON_FOOT);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskAimGunOnFoot::UpdateClonedSubTasks(CPed* pPed, int const iState)
{
	CTaskTypes::eTaskType expectedTaskType = CTaskTypes::TASK_INVALID_ID;
	switch(iState)
	{
		case State_Blocked:
			expectedTaskType = CTaskTypes::TASK_WEAPON_BLOCKED; 
			break;
		default:
			return;
	}

	if(expectedTaskType != CTaskTypes::TASK_INVALID_ID)
	{
		 /*CTaskFSMClone::*/CreateSubTaskFromClone(pPed, expectedTaskType);
	}
}

////////////////////////////////////////////////////////////////////////////////

CTaskInfo *CTaskAimGunOnFoot::CreateQueriableState() const
{
	bool bRollForward = true;
	float fRollDirection = 0.f;
	bool bCombatRoll = GetPed()->GetMotionData()->GetCombatRoll();
	if (bCombatRoll)
	{
		CTask* pTask = GetPed()->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_COMBAT_ROLL);
		if (pTask)
		{
			CTaskCombatRoll* pTaskCombatRoll = static_cast<CTaskCombatRoll*>(pTask);
			if (pTaskCombatRoll)
			{
				bRollForward = pTaskCombatRoll->IsForwardRoll();
				fRollDirection = pTaskCombatRoll->GetDirection();
			}
		}
	}

	return rage_new CClonedAimGunOnFootInfo(GetState(), m_fireCounter, m_iAmmoInClip, m_bCloneWeaponControllerFiring, m_seed, m_LastLocalRollToken, GetPed()->GetPedResetFlag(CPED_RESET_FLAG_UseAlternativeWhenBlock),bCombatRoll,bRollForward,fRollDirection);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskAimGunOnFoot::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	CClonedAimGunOnFootInfo *pAimGunInfo = static_cast<CClonedAimGunOnFootInfo*>(pTaskInfo);

	// Call the base implementation - syncs the state
	CTaskFSMClone::ReadQueriableState(pTaskInfo);

	// catch the case where the ped has fired but the fire state was missed by the network update
	if (pAimGunInfo->GetFireCounter() > m_fireCounter)
	{
		if (GetState() != State_Firing)
			m_bForceFire = true;

		m_iAmmoInClip				   = pAimGunInfo->GetAmmoInClip() + 1;
	}
	else
	{
		m_iAmmoInClip				   = pAimGunInfo->GetAmmoInClip();
	}

	m_bCloneWeaponControllerFiring     = pAimGunInfo->GetWeaponControllerFiring();
	m_seed                             = pAimGunInfo->GetSeed();
    m_LastLocalRollToken               = pAimGunInfo->GetRollToken();
	m_bUseAlternativeAimingWhenBlocked = pAimGunInfo->GetUseAlternativeAimingWhenBlocked();
	
	if (pAimGunInfo->IsCombatRollActive())
	{
		CTask* pTask = GetPed()->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_COMBAT_ROLL);
		if (pTask)
		{
			CTaskCombatRoll* pTaskCombatRoll = static_cast<CTaskCombatRoll*>(pTask);
			if (pTaskCombatRoll)
			{
				pTaskCombatRoll->SetRemotActive(true);
				pTaskCombatRoll->SetRemoteForwardRoll(pAimGunInfo->GetRollForward());
				pTaskCombatRoll->SetRemoteDirection(pAimGunInfo->GetRollDirection());
			}
		}		
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskAimGunOnFoot::OnCloneTaskNoLongerRunningOnOwner()
{
	SetStateFromNetwork( State_Finish );
}

////////////////////////////////////////////////////////////////////////////////

void CTaskAimGunOnFoot::CleanUp()
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	if(pPed)
	{
		CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
		const CWeaponInfo* pWeaponInfo = pWeapon ? pWeapon->GetWeaponInfo() : NULL;
		if (pWeaponInfo && pWeaponInfo->GetHash() == ATSTRINGHASH("WEAPON_DBSHOTGUN",0xEF951FBB) && m_bFiredOnce && !m_bFiredTwice)
		{
			//Force the second shot for DBS shotgun if we clean up midway
			pWeapon->SetNextShotAllowedTime(0);
			FireGun(pPed);

			//B*2728231 - UI is updated before the cleanup and holster ammo can be wrong (depending on button press timing).
			// Make sure we reload the shotgun on re-equip after force firing the second shot.
			CNewHud::SetWheelHolsterAmmoInClip(0);
		}

		pPed->SetTaskCapsuleRadius(0.0f);
		CTaskGun::ClearToggleAim(pPed);

		if(pPed->IsNetworkClone()) 
		{
			if(pWeaponInfo && pWeaponInfo->GetEffectGroup()==WEAPON_EFFECT_GROUP_ROCKET && CNetworkPendingProjectiles::GetProjectile(pPed,GetNetSequenceID(), false))
			{   //If clone was firing a rocket and it is still in the pending list for this task then fire it now ped has 
				//probably gone out of scope and we will lose the rocket when it times out in pending list - B*2177009
				CProjectileManager::FireOrPlacePendingProjectile(pPed, GetNetSequenceID());
			}
		}
	}

	SetMoveFlag(false, ms_FiringFlagId);

	// Only set this flag as false if we have broken out unexpectedly (no subtask or sub task isn't TASK_AIM_AND_THROW_PROJECTILE)
	if (GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_ThrowingGrenadeWhileAiming) && (!GetSubTask() || (GetSubTask() && GetSubTask()->GetTaskType() != CTaskTypes::TASK_AIM_AND_THROW_PROJECTILE)))
	{
		GetPed()->GetPedIntelligence()->SetDoneAimGrenadeThrow();
		GetPed()->SetPedConfigFlag(CPED_CONFIG_FLAG_ThrowingGrenadeWhileAiming, false);
	}
#if ENABLE_GUN_PROJECTILE_THROW
	// Only set this flag as false if we have broken out unexpectedly (no subtask or sub task isn't TASK_AIM_AND_THROW_PROJECTILE)
	if (GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_ThrowingGrenadeWhileAiming) && (!GetSubTask() || (GetSubTask() && GetSubTask()->GetTaskType() != CTaskTypes::TASK_AIM_AND_THROW_PROJECTILE)))
	{
		GetPed()->GetPedIntelligence()->SetDoneAimGrenadeThrow();
		GetPed()->SetPedConfigFlag(CPED_CONFIG_FLAG_ThrowingGrenadeWhileAiming, false);
	}
#endif	//ENABLE_GUN_PROJECTILE_THROW

	// Base class
	CTaskAimGun::CleanUp();
}

////////////////////////////////////////////////////////////////////////////////

CTaskAimGunOnFoot::FSM_Return CTaskAimGunOnFoot::ProcessPreFSM()
{
	CPed *pPed = GetPed(); //Get the ped ptr.
	const s32 iState = GetState(); // Get the current state of the FSM

	//If we're running the projectile throw subtask, set the reset flag
	if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_AIM_AND_THROW_PROJECTILE)
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_IsThrowingProjectileWhileAiming, true);
	}

	bool bThrowingProjectileWhileAiming = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ThrowingGrenadeWhileAiming);

	// Is a valid weapon object still available
	weaponAssert(pPed->GetWeaponManager());
	const CObject* pWeaponObject = pPed->GetWeaponManager()->GetEquippedWeaponObject();
// 	if(!pWeaponObject && !bThrowingProjectileWhileAiming)
// 	{
// 		return FSM_Quit;
// 	}

	const CWeapon* pWeapon = pWeaponObject ? pWeaponObject->GetWeapon() : NULL;

// 	if(!pWeapon && !bThrowingProjectileWhileAiming)
// 	{
// 		return FSM_Quit;
// 	}

#if FPS_MODE_SUPPORTED
	// If we are pouring petrol disable camera switching
	if( (iState == State_Firing || iState == State_FireOutro) && pWeapon && pWeapon->GetWeaponInfo() && pWeapon->GetWeaponInfo()->GetGroup() == WEAPONGROUP_PETROLCAN )
	{
		pPed->SetPlayerResetFlag(CPlayerResetFlags::PRF_DISABLE_CAMERA_VIEW_MODE_CYCLE);
	}
#endif

	// Check we still have a valid target
	if(!m_target.GetIsValid() && !bThrowingProjectileWhileAiming 
#if FPS_MODE_SUPPORTED
		&& !pPed->IsFirstPersonShooterModeEnabledForPlayer(true)
#endif // FPS_MODE_SUPPORTED
		)
	{
		taskDebugf1("CTaskAimGunOnFoot::ProcessPreFSM: FSM_Quit - !m_target.GetIsValid. Previous State: %d", GetPreviousState());
		return FSM_Quit;
	}

	const CWeaponInfo* pWeaponInfo = pWeapon ? pWeapon->GetWeaponInfo() : NULL;
	// No ammo left, quit to parent task to switch weapons
	// Can't do this for network clones
	if(!pPed->IsNetworkClone() && GetState() != State_Firing && IsOutOfAmmo())
	{
		// Delay for sniper rifle aiming when running out of ammo.
		if(pPed->IsLocalPlayer() && IsAimingScopedWeapon())
		{
			if(!m_bHasSetQuitTimeWhenOutOfAmmoInScope)
			{
				m_uQuitTimeWhenOutOfAmmoInScope = fwTimer::GetTimeInMilliseconds() + sm_Tunables.m_DelayTimeWhenOutOfAmmoInScope;
				m_bHasSetQuitTimeWhenOutOfAmmoInScope = true;
			}

			if(fwTimer::GetTimeInMilliseconds() > m_uQuitTimeWhenOutOfAmmoInScope)
			{
				taskDebugf1("CTaskAimGunOnFoot::ProcessPreFSM: FSM_Quit - IsOutOfAmmo, fwTimer::GetTimeInMilliseconds() > m_uQuitTimeWhenOutOfAmmoInScope. Previous State: %d", GetPreviousState());
				return FSM_Quit;
			}
		}
		else
		{
			taskDebugf1("CTaskAimGunOnFoot::ProcessPreFSM: FSM_Quit - IsOutOfAmmo, !IsAimingScopedWeapon. Previous State: %d", GetPreviousState());
			return FSM_Quit;
		}
	}

	pPed->SetPedResetFlag(CPED_RESET_FLAG_ProcessPostCamera, true);
	pPed->SetPedResetFlag(CPED_RESET_FLAG_ProcessPostPreRender, true);

	// Update the blocked flags on non player peds
	if(!pPed->IsAPlayerPed())
	{
		UpdateAIBlockStatus(*pPed);
	}

	if(pWeaponInfo && !pWeaponInfo->GetAllowEarlyExitFromFireAnimAfterBulletFired()) 
	{
		m_iFlags.SetFlag(GF_OnlyExitFireLoopAtEnd);
	}
	else
	{
		m_iFlags.ClearFlag(GF_OnlyExitFireLoopAtEnd);
	}

	pPed->SetIsStrafing(true);

	if(CTaskAimGun::ms_bUseTorsoIk)
	{
		if (!pPed->GetIKSettings().IsIKFlagDisabled(IKManagerSolverTypes::ikSolverTypeTorso))
		{
			// Pitch handled via animation
			m_ikInfo.SetDisablePitchFixUp(true);

			TUNE_GROUP_FLOAT(NEW_GUN_TASK, SPINE_MATRIX_INTERP_RATE, 4.0f, 0.0f, 1000.0f, 0.1f);
			m_ikInfo.SetSpineMatrixInterpRate(SPINE_MATRIX_INTERP_RATE);

			// Apply the Ik
			m_ikInfo.ApplyIkInfo(pPed);
		}
	}

	// Need to set desired heading to compute idle turn parameter
	// This gets done for the player in post cam update
	if((!pPed->IsLocalPlayer() || pPed->GetPlayerInfo()->AreControlsDisabled()) && !pPed->IsNetworkClone())
	{
		Vector3 vTargetPos;
		if(taskVerifyf(m_target.GetPositionWithFiringOffsets(pPed, vTargetPos), "Couldn't get target position"))
		{
			if(taskVerifyf(rage::FPIsFinite(vTargetPos.x) && rage::FPIsFinite(vTargetPos.y), "Invalid Target Position"))
			{
				if(CanSetDesiredHeading(pPed))
				{
					pPed->SetDesiredHeading(vTargetPos);
				}
			}
		}
	}

	// Update our LOS probe
	if(pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_RequiresLosToShoot))
	{
		UpdateLosProbe();
	}

	// Allow to fire again once the trigger is released
	if(m_bFiredOnceThisTriggerPress && (!pPed->IsLocalPlayer() || !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_WasFiring)))
	{
		m_bFiredOnceThisTriggerPress = false;
	}
	
	// Process the streaming.
	ProcessStreaming();

	// Dampen the root if this is the player and we are not assisted aiming and we're not in low cover
	if(pPed->IsLocalPlayer() && !pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON) && 
		!pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_RUN_AND_GUN) && !pPed->GetIsInCover() && !pPed->GetIsCrouching() && GetState() != State_CombatRoll FPS_MODE_SUPPORTED_ONLY(&& !pPed->IsFirstPersonShooterModeEnabledForPlayer(false)))
	{
		const camBaseCamera* pDominantRenderedCamera = camInterface::GetDominantRenderedCamera();
		if(pDominantRenderedCamera && (pDominantRenderedCamera->GetIsClassId(camThirdPersonAimCamera::GetStaticClassId()) || pDominantRenderedCamera->GetIsClassId(camFirstPersonAimCamera::GetStaticClassId())))
		{
			taskAssert(pPed->GetPlayerInfo());
			pPed->GetPlayerInfo()->SetDampenRootTargetWeight(sm_Tunables.m_DampenRootTargetWeight);
			pPed->GetPlayerInfo()->SetDampenRootTargetHeight(sm_Tunables.m_DampenRootTargetHeight);
		}
	}

	if(pWeaponInfo && pWeaponInfo->GetExpandPedCapsuleRadius() != 0.0f)
	{
		pPed->SetTaskCapsuleRadius(pWeaponInfo->GetExpandPedCapsuleRadius());
	}

	// we need processing in every state
	RequestProcessMoveSignalCalls();

	if(m_bUseAlternativeAimingWhenBlocked)
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_UseAlternativeWhenBlock, true);
	}

#if FPS_MODE_SUPPORTED
	if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		bool bExitingCombatRoll = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ExitingFPSCombatRoll);
		WeaponControllerState controllerState = GetWeaponControllerState(pPed);
		bool bAllowFiring = pWeaponInfo && !pWeaponInfo->GetEnableFPSIdleOnly() && (!pWeaponInfo->GetEnableFPSRNGOnly() || pPed->GetMotionData()->GetIsFPSRNG() || bExitingCombatRoll) && pWeaponInfo->GetIsGunOrCanBeFiredLikeGun();
		CControl* pControl = pPed->GetControlFromPlayer();
		bool bPressingWeaponWheelInputOrWheelActive = CNewHud::IsWeaponWheelActive() || (pControl && (pControl->GetSelectWeapon().IsPressed() || pControl->GetSelectWeapon().IsDown()));

		if(bAllowFiring && (GetState() == State_Start || (GetState() == State_Aiming && !ShouldFireWeapon()) || (GetState() == State_CombatRoll && bExitingCombatRoll)) && 
		   (controllerState == WCS_Fire || controllerState == WCS_FireHeld) && !pPed->GetPedResetFlag(CPED_RESET_FLAG_IsThrowingProjectileWhileAiming)
		   && !bPressingWeaponWheelInputOrWheelActive)
		{
			m_bWantsToFireWhenWeaponReady = true;
		}

		if (pWeaponInfo && pWeaponInfo->GetOnlyFireOneShotPerTriggerPress() && m_bFiredOnceThisTriggerPress)
		{
			m_bWantsToFireWhenWeaponReady = false;
		}

		if(m_bWantsToFireWhenWeaponReady)
		{
			pPed->SetPedResetFlag(CPED_RESET_FLAG_FiringWeaponWhenReady, true);
		}

		// Start time of switching to FPS scope state.
		if(pPed->GetMotionData()->GetPreviousFPSState() != CPedMotionData::FPS_SCOPE && pPed->GetMotionData()->GetIsFPSScope())
		{
			m_iFPSSwitchToScopeStartTime = fwTimer::GetTimeInMilliseconds();
		}
	}
#endif

	// Base class
	return CTaskAimGun::ProcessPreFSM();
}

////////////////////////////////////////////////////////////////////////////////

CTaskAimGunOnFoot::FSM_Return CTaskAimGunOnFoot::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnEnter
				return Start_OnEnter();
			FSM_OnUpdate
				return Start_OnUpdate();
			FSM_OnExit
				return Start_OnExit();

		FSM_State(State_Aiming)
			FSM_OnEnter
				return Aiming_OnEnter();
			FSM_OnUpdate
				return Aiming_OnUpdate();
			FSM_OnExit
				return Aiming_OnExit();

		FSM_State(State_Firing)
			FSM_OnEnter
				return Firing_OnEnter();
			FSM_OnUpdate
				return Firing_OnUpdate();
			FSM_OnExit
				return Firing_OnExit();

		FSM_State(State_FireOutro)
			FSM_OnUpdate
				return FireOutro_Update();

		FSM_State(State_AimToIdle)
			FSM_OnEnter
				return AimToIdle_OnEnter();
			FSM_OnUpdate
				return AimToIdle_OnUpdate();
			FSM_OnExit
				return AimToIdle_OnExit();

		FSM_State(State_AimOutro)
			FSM_OnUpdate
			{
				CTaskAimGunOnFoot::FSM_Return res = AimOutro_OnUpdate();
				if( res == FSM_Quit )
					SetClothPackageIndex(0, 10);
				else
					SetClothPackageIndex(1);
				return res; 
			}

		FSM_State(State_Blocked)
			FSM_OnEnter
				return Blocked_OnEnter();
			FSM_OnUpdate
				return Blocked_OnUpdate();

		FSM_State(State_CombatRoll)
			FSM_OnEnter
				return CombatRoll_OnEnter();
			FSM_OnUpdate
				return CombatRoll_OnUpdate();

#if ENABLE_GUN_PROJECTILE_THROW
		FSM_State(State_ThrowingProjectileFromAiming)
			FSM_OnEnter
				return ThrowingProjectileFromAiming_OnEnter();
			FSM_OnUpdate
				return ThrowingProjectileFromAiming_OnUpdate();
#endif //ENABLE_GUN_PROJECTILE_THROW

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;
	
	FSM_End
}

////////////////////////////////////////////////////////////////////////////////

CTaskAimGunOnFoot::FSM_Return CTaskAimGunOnFoot::UpdateClonedFSM( s32 iState, FSM_Event iEvent)
{
	CPed *pPed = GetPed(); 

	if(iState != State_Finish)
	{
		if(iEvent == OnUpdate)
		{
			s32 stateFromNetwork = GetStateFromNetwork();

			if(stateFromNetwork != GetState())
			{
				if(stateFromNetwork != -1)
				{
					if(stateFromNetwork == State_Finish)
					{
						SetState(stateFromNetwork);
						return FSM_Continue;
					}

                    if(stateFromNetwork == State_CombatRoll)
                    {
                        const unsigned WRAP_THRESHOLD = MAX_ROLL_TOKEN / 2;
                        if(m_LastCloneRollToken < m_LastLocalRollToken || ((m_LastCloneRollToken - m_LastLocalRollToken) > WRAP_THRESHOLD))
                        {
                            SetState(stateFromNetwork);
						    return FSM_Continue;
                        }
                    }

					if(stateFromNetwork == State_Blocked)
					{
						// don't bother with FP idle blocks 
						bool bInFirstPersonIdle = false;

						if (!NetworkInterface::IsRemotePlayerInFirstPersonMode(*pPed, &bInFirstPersonIdle) || !bInFirstPersonIdle)
						{
							SetState(stateFromNetwork);
							return FSM_Continue;
						}
					}
					// or switching away from...
					else if(GetState() == State_Blocked)
					{
						SetState(stateFromNetwork);
						return FSM_Continue;
					}

					if (GetState() == State_Firing && stateFromNetwork == State_Aiming && GetTimeInState() > 1.0f && m_bForceFire)
					{
						SetState(stateFromNetwork);
						return FSM_Continue;
					}
				}
			}

			UpdateClonedSubTasks(pPed, GetState());
		}
	}

	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_Aiming)
			FSM_OnEnter
				return Aiming_OnEnter();
			FSM_OnUpdate
				return Aiming_OnUpdate();
			FSM_OnExit
				return Aiming_OnExit();

		FSM_State(State_Firing)
			FSM_OnEnter
				return Firing_OnEnter();
			FSM_OnUpdate
				return Firing_OnUpdate();
			FSM_OnExit
				return Firing_OnExit();

		FSM_State(State_AimToIdle)
			FSM_OnEnter
				return AimToIdle_OnEnter();
			FSM_OnUpdate
				return AimToIdle_OnUpdate();
			FSM_OnExit
				return AimToIdle_OnExit();

        FSM_State(State_CombatRoll)
			FSM_OnEnter
				return CombatRoll_OnEnter();
			FSM_OnUpdate
				return CombatRoll_OnUpdate();

#if ENABLE_GUN_PROJECTILE_THROW
		FSM_State(State_ThrowingProjectileFromAiming)
			FSM_OnEnter
				return ThrowingProjectileFromAiming_OnEnter();
			FSM_OnUpdate
				return ThrowingProjectileFromAiming_OnUpdate();
#endif	//ENABLE_GUN_PROJECTILE_THROW

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

        // unhandled states
		FSM_State(State_FireOutro)
		FSM_State(State_Blocked)
        FSM_State(State_AimOutro)

	FSM_End
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskAimGunOnFoot::IsInScope(const CPed *pPed)
{
    bool inScope = CTaskFSMClone::IsInScope(pPed);

#if FPS_MODE_SUPPORTED
	if (!pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
#endif // FPS_MODE_SUPPORTED
	{
 		// Is a valid weapon object still available
		weaponAssert(pPed->GetWeaponManager());
		const CObject* pWeaponObject = pPed->GetWeaponManager()->GetEquippedWeaponObject();
		if(!pWeaponObject)
		{
      		bool hasThrowProjectileSubTask = GetSubTask() && (GetSubTask()->GetTaskType() == CTaskTypes::TASK_AIM_AND_THROW_PROJECTILE);
       
			if(!hasThrowProjectileSubTask)
			{
				inScope = false;
			}
		}

		// Check we still have a valid target
		if(!m_target.GetIsValid())
		{
			inScope = false;
		}
	}

    return inScope;
}

float CTaskAimGunOnFoot::GetScopeDistance() const
{
	return static_cast<float>(CTaskGun::CLONE_TASK_SCOPE_DISTANCE);
}

////////////////////////////////////////////////////////////////////////////////

CTaskAimGunOnFoot::FSM_Return CTaskAimGunOnFoot::ProcessPostFSM()
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	// Update whether the ped is crouching
	UpdateCrouching(pPed);

	// Base class
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskAimGunOnFoot::ProcessPostCamera()
{
	CPed* pPed = GetPed();
	PostCameraUpdate(pPed, m_iFlags.IsFlagSet(GF_DisableTorsoIk));

	// Base class
	return CTaskAimGun::ProcessPostCamera();
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskAimGunOnFoot::ProcessPostPreRender()
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	if(m_bFire)
	{
		// Ignore fire events if we're not holding a gun weapon
		weaponAssert(pPed->GetWeaponManager());
		CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
		if(pWeapon)
		{
			if(pPed->IsNetworkClone() && m_uNextShotAllowedTime !=0)
			{
				pWeapon->SetNextShotAllowedTime(m_uNextShotAllowedTime);
			}

			FireGun(pPed);

			if(pPed->IsNetworkClone())
			{
				m_uNextShotAllowedTime = pWeapon->GetNextShotAllowedTime();
			}
			else if (!m_bFiredOnce && pPed->GetNetworkObject())
			{
				// force a task update when the ped starts firing from cover, this is so we see a shot on the clone side. Previously clone peds in cover where not firing because the gun task briefly runs before the
				// ped goes back into cover - so they can sometimes only fire one shot.
				if(GetParent() && GetParent()->GetParent() && GetParent()->GetParent()->GetTaskType() == CTaskTypes::TASK_IN_COVER)
				{
					CNetObjPed *netObjPed = SafeCast(CNetObjPed, pPed->GetNetworkObject());

					if(netObjPed && netObjPed->GetSyncData())
					{
						netObjPed->RequestForceSendOfTaskState();
					}
				}
			}

			if (m_bFiredOnce)
				m_bFiredTwice = true;

			m_bFire = false;
			m_bFiredOnce = true;
			m_bFiredOnceThisTriggerPress = true;
		}
	}

	// Base class
	return CTaskAimGun::ProcessPostPreRender();
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskAimGunOnFoot::ProcessMoveSignals()
{
	bool bProcessTorsoIK = false;

	bool bForceUpdate = (GetState() == State_Firing) && ShouldFireWeapon();
	if(bForceUpdate)
	{
		GetPed()->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);
	}

	//Check the state.
	const int iState = GetState();
	switch(iState)
	{
		case State_Aiming:
		{
			Aiming_OnProcessMoveSignals();
			bProcessTorsoIK = true;
			break;
		}
		case State_Firing:
		case State_FireOutro:
		{
			Firing_OnProcessMoveSignals();
			bProcessTorsoIK = true;
			break;
		}
		default:
		{
			break;
		}
	}

	if(iState != State_Start)
	{
		if(!m_bDoneIntro)
		{
			CMoveNetworkHelper* pHelper = FindAimingNetworkHelper(); 
			if(pHelper && pHelper->IsNetworkAttached())
			{
				pHelper->SetFlag(false, ms_UseIntroId);
				m_bDoneIntro = true;
			}
		}
	}

	if (bProcessTorsoIK)
	{
		CPed* pPed = GetPed();

		if (pPed->GetMotionData()->GetIsStrafing() && !CPedAILodManager::ShouldDoFullUpdate(*pPed))
		{
			// Keep torso solver alive if ped is not doing a full update
			CTaskGun::ProcessTorsoIk(*pPed, m_iFlags, m_target, GetClipHelper());
		}
	}

#if FPS_MODE_SUPPORTED
	if(GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		CMoveNetworkHelper* pHelper = FindAimingNetworkHelper(); 
		if(pHelper && pHelper->IsNetworkAttached())
		{
			const CClipEventTags::CCameraShakeTag* pCamShake = static_cast<const CClipEventTags::CCameraShakeTag*>(pHelper->GetNetworkPlayer()->GetProperty(CClipEventTags::CameraShake));
			if (pCamShake)
			{
				if(pCamShake->GetShakeRefHash() != 0)
				{
					camInterface::GetGameplayDirector().GetFirstPersonShooterCamera()->Shake( pCamShake->GetShakeRefHash() );
				}
			}
		}
	}
#endif

	// we always have some processing to do
	return true;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskAimGunOnFoot::Start_OnEnter()
{
	CPed* pPed = GetPed();

	//@@: location CTASKAIMGUNONFOOT_START_ONENTER
	if (pPed && (pPed->GetPedResetFlag(CPED_RESET_FLAG_InstantBlendToAim) || pPed->GetPedResetFlag(CPED_RESET_FLAG_InstantBlendToAimFromScript)))
	{
		pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Aiming);
	}

	if (pPed->IsLocalPlayer() && pPed->GetWeaponManager())
	{
		m_WeaponObject = pPed->GetWeaponManager()->GetEquippedWeaponObject();
	}

	return FSM_Continue;
}


////////////////////////////////////////////////////////////////////////////////

CTaskAimGunOnFoot::FSM_Return CTaskAimGunOnFoot::Start_OnUpdate()
{
	CPed* pPed = GetPed();

	//Allow going straight into blocked as the aiming network isn't needed. Avoids popping
	bool bShouldFireWeapon = ShouldFireWeapon();
	bool bIsBlocked = !pPed->IsNetworkClone() && IsWeaponBlocked(pPed, false, 1.0f, 1.0f, false, bShouldFireWeapon);
	if(bIsBlocked)
	{
		// Our weapon is blocked, so don't go into aiming or firing
		SetState(State_Blocked);
		return FSM_Continue;
	}

	WeaponControllerState controllerState = GetWeaponControllerState(pPed);
	if (m_bForceRoll || controllerState == WCS_CombatRoll)
	{
		SetState(State_CombatRoll);
		return FSM_Continue;
	}

	bool bSwitchWeapon = GetWaitingToSwitchWeapon();
	if (bSwitchWeapon)
	{
		if(GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_GUN)
		{
			static_cast<CTaskGun*>(GetParent())->GetGunFlags().SetFlag(GF_AllowAimingWithNoAmmo);
		}
		taskDebugf1("CTaskAimGunOnFoot::Start_OnUpdate: FSM_Quit - bSwitchWeapon. Previous State: %d", GetPreviousState());
		return FSM_Quit;
	}

	CMoveNetworkHelper* pHelper = FindAimingNetworkHelper(); 
	if (!pHelper)
	{
		return FSM_Continue;
	}

	if (pHelper->IsNetworkActive())
	{
		if (!m_bDoneIntro)
		{
			pHelper->SetFlag(true, ms_UseIntroId);
		}
	}

	// Wait until the strafing motion task and move network is active
	if (!pHelper->IsNetworkAttached())
	{
		return FSM_Continue;
	}

	if (pHelper->IsNetworkAttached())
	{
		if (pPed->HasHurtStarted())
		{
			float fAnimRate = fwRandom::GetRandomNumberInRange(0.9f, 1.1f);
			pHelper->SetFloat(ms_AnimRateId, fAnimRate);
		}
	}

	if (GetGunFlags().IsFlagSet(GF_InstantBlendToAim))
	{
		Vector3 vTargetPos;
		if (taskVerifyf(m_target.GetPosition(vTargetPos), "Couldn't get target position"))
		{
			if (!pPed->GetIsInCover())
			{
				const Vector3 vPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

				if(pPed->GetMovePed().GetState()==CMovePed::kStateAnimated)
				{
					pPed->SetHeading(fwAngle::GetRadianAngleBetweenPoints(vTargetPos.x, vTargetPos.y, vPos.x, vPos.y));
				}
			}
			pPed->SetDesiredHeading(vTargetPos);
		}
	}

	if (m_bForceFiringState)
	{
		// Firing state has been forced externally
		SetState(State_Firing);
		return FSM_Continue;
	}

	if (pPed->GetWeaponManager()->GetEquippedWeaponInfo() && 
		pPed->GetWeaponManager()->GetEquippedWeaponInfo()->GetOnlyAllowFiring() FPS_MODE_SUPPORTED_ONLY(&& !pPed->IsFirstPersonShooterModeEnabledForPlayer(true,true)))
	{
		ProcessSmashGlashFromBlocked(pPed);

		SetState(State_Firing);
		return FSM_Continue;
	}

// 	if (GetGunFlags().IsFlagSet(GF_FireAtLeastOnce))
// 	{
// 		SetState(State_Firing);
// 	}
// 	else
	{
		taskDebugf1("CTaskAimGunOnFoot::Start_OnUpdate: SetState(State_Aiming): Ped 0x%p [%s]", pPed, pPed->GetModelName());
		SetState(State_Aiming);
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskAimGunOnFoot::Start_OnExit()
{
	// Ensure restart flag is cleared
	m_bForceRestart = false;
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskAimGunOnFoot::Aiming_OnEnter()
{
	m_uNextShotAllowedTime = 0;

	RequestAim();

	SetClothPackageIndex(1);

	m_bMoveInterruptSettle = false;
	m_bMoveAimIntroInterruptible = false;

	// Ensure this frames move signals are up to date
	Aiming_OnProcessMoveSignals();

#if FPS_MODE_SUPPORTED
	if (GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false) && GetPed()->GetIsInCover() && CTaskCover::IsPlayerAimingDirectlyInFirstPerson(*GetPed()))
	{
		if (GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_GUN)
		{
			// Clear these here so in CTaskMotionAiming::StartAimingNetwork() we go through the code path that calls SetBlockFiringTime
			// so we can't immediately fire when firing from the relaxed pose
			CTaskGun* pGunTask = static_cast<CTaskGun*>(GetParent());
			pGunTask->GetGunFlags().ClearFlag( GF_SkipIdleTransitions );
			pGunTask->GetGunFlags().ClearFlag( GF_InstantBlendToAim );
		}
	}
#endif // FPS_MODE_SUPPORTED

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskAimGunOnFoot::Aiming_OnUpdate()
{
	bool bMoveInterruptSettle = m_bMoveInterruptSettle;
	m_bMoveInterruptSettle = false;

	bool bMoveAimIntroInterruptible = m_bMoveAimIntroInterruptible;
	m_bMoveAimIntroInterruptible = false;

	CPed* pPed = GetPed();
	if (!pPed)
	{
		taskDebugf1("CTaskAimGunOnFoot::Aiming_OnUpdate: FSM_Quit - !pPed. Previous State: %d", GetPreviousState());
		return FSM_Quit;
	}

	CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
	const bool bShouldFireWeapon = ShouldFireWeapon();
	const bool bCanFireWeapon = CanFireAtTarget(pPed);

	if(m_bForceFiringState)
	{
		// We have externally force the firing state
		SetState(State_Firing);
		return FSM_Continue;
	}

	WeaponControllerState controllerState = GetWeaponControllerState(pPed);
	if (m_bCrouchStateChanged || m_bForceRestart)
	{
		m_bCrouchStateChanged = false;
		m_bForceRestart = false;

#if FPS_MODE_SUPPORTED
		if( pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && 
			pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_FPSFidgetsAbortedOnFire) && 
			(pWeapon == NULL || pWeapon->GetState() == CWeapon::STATE_READY) &&
			bCanFireWeapon && 
			bShouldFireWeapon )
		{
			m_bForceFiringState = true;
		}

		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_FPSFidgetsAbortedOnFire, false);
#endif

		SetState(State_Start);
		return FSM_Continue;
	}

	if(controllerState == WCS_CombatRoll)
	{
		SetState(State_CombatRoll);
		return FSM_Continue;
	}

	const CWeaponInfo* pWeaponInfo = pWeapon ? pWeapon->GetWeaponInfo() : NULL;
	if(pWeaponInfo)
	{
		CTaskMotionAiming* pMotionTask = FindMotionAimingTask();
		if(pMotionTask && pPed->GetMotionData())
		{
			const bool bUsingStealth = pPed->GetMotionData()->GetUsingStealth();
			if(bUsingStealth)
			{
				UpdateAdditiveWeights(pWeaponInfo->GetStealthAimingBreathingAdditiveWeight(), pWeaponInfo->GetStealthAimingLeanAdditiveWeight());
			}
			else
			{
				UpdateAdditiveWeights(pWeaponInfo->GetAimingBreathingAdditiveWeight(), pWeaponInfo->GetAimingLeanAdditiveWeight());
			}

			pMotionTask->SetMoveFloat(m_fBreathingAdditiveWeight, ms_AimingBreathingAdditiveWeight);
			pMotionTask->SetMoveFloat(m_fLeanAdditiveWeight, ms_AimingLeanAdditiveWeight);
		}
	}

	if( pPed->IsNetworkClone() && 
		(pWeaponInfo && pWeaponInfo->GetGroup() == WEAPONGROUP_PETROLCAN) && 
		pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true) &&  // the first bool is irrelevant here as second 'true' means just checking if remote is in FPS  
		GetStateFromNetwork()==State_Firing)
	{
		SetState(State_Firing);
		return FSM_Continue;
	}

	ProcessShouldReloadWeapon();

	bool bNoLongerWantingGunTask = controllerState != WCS_Aim && controllerState != WCS_Reload && controllerState != WCS_Fire && controllerState != WCS_FireHeld && !pPed->GetPedResetFlag(CPED_RESET_FLAG_IsThrowingProjectileWhileAiming);
	bool bSwitchWeapon = GetWaitingToSwitchWeapon();
	bool bShouldFinish = GetGunFlags().IsFlagSet(GF_DisableAiming) && pPed->IsLocalPlayer() && !CPlayerInfo::IsFiring_s();
	bool bIsBlocked = !pPed->IsNetworkClone() && IsWeaponBlocked(pPed,false,1.0f,1.0f,false,bShouldFireWeapon);

	bool bIsStungunMP = pWeaponInfo && pWeaponInfo->GetHash() == ATSTRINGHASH("WEAPON_STUNGUN_MP", 0x45CD9CF3);
	bool bIsRecharging = bIsStungunMP ? (pWeaponInfo && pWeaponInfo->GetDisplayRechargeTimeHUD() && (pWeapon && pWeapon->GetState() == CWeapon::STATE_WAITING_TO_FIRE) && pWeaponInfo->GetTimeLeftBetweenShotsWhereShouldFireIsCached() == -1.0f) : false;

	if((controllerState == WCS_Fire || controllerState == WCS_FireHeld) && !GetGunFireFlags().IsFlagSet(GFF_RequireFireReleaseBeforeFiring) && !m_bFiredOnce && bCanFireWeapon && ((pWeapon && pWeapon->GetWeaponInfo() && !pWeapon->GetWeaponInfo()->GetUsesAmmo()) || (pWeapon && pWeapon->GetAmmoTotal() != 0)) && !bIsRecharging)
	{
		// If we are pressing fire while aiming, ensure we fire
		GetGunFlags().SetFlag(GF_FireAtLeastOnce);
	}
	else if(controllerState != WCS_FireHeld && controllerState != WCS_Fire)
	{
		GetGunFireFlags().ClearFlag(GFF_RequireFireReleaseBeforeFiring);		
	}

#if FPS_MODE_SUPPORTED
	// Interrupt settle and aim intro if we need to swap weapons
	if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && bSwitchWeapon)
	{
		if(!bMoveInterruptSettle)
		{
			bMoveInterruptSettle = true;
		}

		CTaskMotionAiming* pMotionTask = FindMotionAimingTask();
		if(pMotionTask)
			pMotionTask->StopAimIntro();

		// Force the aim intro animation to finish if it is playing
		SetMoveBoolean(true, ms_FPSAimIntroTransition);
	}

#endif	//FPS_MODE_SUPPORTED

	if(bIsBlocked)
	{
		// Our weapon is blocked, so we can't aim anymore
		SetState(State_Blocked);
		return FSM_Continue;
	}

#if ENABLE_GUN_PROJECTILE_THROW
	// Don't allow switch to State_Firing if we are about to throw a projectile while aiming; bCanFireWeapon gets set to false the following frame due to CPED_CONFIG_FLAG_ThrowingGrenadeWhileAiming
	if (ShouldThrowGrenadeFromGunAim(pPed, pWeaponInfo))
	{
		SetState(State_ThrowingProjectileFromAiming);
		return FSM_Continue;
	}
#endif	//ENABLE_GUN_PROJECTILE_THROW

	if(!IsInAimState())
	{
		if(bShouldFireWeapon FPS_MODE_SUPPORTED_ONLY(|| (pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && m_bWantsToFireWhenWeaponReady)) 
			|| (bMoveInterruptSettle && (bSwitchWeapon || m_bIsReloading || bShouldFinish || bIsBlocked || bNoLongerWantingGunTask)))
		{
			// Trigger the aim intro to stop
			CTaskMotionAiming* pMotionTask = FindMotionAimingTask();
			if(pMotionTask)
				pMotionTask->StopAimIntro();

			// Force the settle animation to finish if it is playing
			SetMoveBoolean(true, ms_SettleFinishedId);
		}

#if FPS_MODE_SUPPORTED
		if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
		{
			if(bShouldFireWeapon) 
			{
				if(!m_bFiredOnceThisTriggerPress && bCanFireWeapon)
				{
					m_bWantsToFireWhenWeaponReady = true;
				}

				if(bMoveAimIntroInterruptible)
				{
					// Trigger the aim intro to stop
					CTaskMotionAiming* pMotionTask = FindMotionAimingTask();
					if(pMotionTask)
						pMotionTask->StopAimIntro();

					// Force the aim intro animation to finish if it is playing
					SetMoveBoolean(true, ms_FPSAimIntroTransition);
				}
			}

			// Spin up during FPS aim transition anims.
			if(pWeapon && CanSpinUp())
			{
				pWeapon->SpinUp(pPed);
			}
		}
#endif

		RequestAim();
		return FSM_Continue;
	}

#if __DEV
	static bool ALWAYS_AIM = false;
	if(ALWAYS_AIM)
	{
		return FSM_Continue;
	}
#endif // __DEV

	// Don't allow state changes until time is up, unless aiming from cover or switching weapon in first person
	if(!pPed->IsFirstPersonShooterModeEnabledForPlayer(false) || (!pPed->GetIsInCover() && !bSwitchWeapon))
	{
		if(m_uBlockFiringTime > fwTimer::GetTimeInMilliseconds())
		{
			return FSM_Continue;
		}
	}

	// Reset the block time
	m_uBlockFiringTime = 0;

	ProcessSmashGlashFromBlocked(pPed);

	if(bNoLongerWantingGunTask)
	{
		bShouldFinish = true;

		if (pPed->IsLocalPlayer())
		{
			bool bIsFPS = false;
#if FPS_MODE_SUPPORTED
			bIsFPS = pPed->IsFirstPersonShooterModeEnabledForPlayer(true);
#endif // FPS_MODE_SUPPORTED

			if (!pPed->GetWeaponManager()->GetEquippedWeaponInfo()->GetOnlyAllowFiring() || bIsFPS)
			{
				if (bIsFPS && pPed->GetIsInCover() && !CTaskCover::IsPlayerAimingDirectlyInFirstPerson(*pPed))
				{
					TUNE_GROUP_BOOL(FIRST_PERSON_COVER_TUNE, ALLOW_QUIT_GUN_TASK, false);
					if (ALLOW_QUIT_GUN_TASK)
					{
						SetState(State_Finish);
						return FSM_Continue;
					}
					else
					{
						bShouldFinish = false;
					}
				}
				else
				{
					SetState(State_AimOutro);
					return FSM_Continue;
				}
			}
		}

		if (bShouldFinish)
		{
#if __DEV
			taskDebugf2("Frame : %u -: 0x%p : Catch infinte loop, bNoLongerWantingGunTask", fwTimer::GetFrameCount(), pPed);
#endif
			return FSM_Quit;
		}
	}

	// Cover sets player free aiming so we can't just check the weapon controller state
	if (bShouldFinish)
	{
#if __DEV
		taskDebugf2("Frame : %u -: 0x%p : Catch infinte loop, bShouldFinish", fwTimer::GetFrameCount(), pPed);
#endif
		return FSM_Quit;
	}

	if (bSwitchWeapon)
	{
		if(GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_GUN)
		{
			static_cast<CTaskGun*>(GetParent())->GetGunFlags().SetFlag(GF_AllowAimingWithNoAmmo);
		}
#if __DEV
		taskDebugf2("Frame : %u -: 0x%p : Catch infinte loop, bSwitchWeapon", fwTimer::GetFrameCount(), pPed);
#endif
		return FSM_Quit;
	}

	if (m_bIsReloading)
	{
#if __DEV
		taskDebugf2("Frame : %u -: 0x%p : Catch infinte loop, m_bIsReloading", fwTimer::GetFrameCount(), pPed);
#endif

#if FPS_MODE_SUPPORTED
		// Stay in aiming state until reloading anim is not blocked by wall.
		// Fixed the glitch caused by switching between reloading and aiming gun.
		const CTaskGun* pTaskGun = static_cast<const CTaskGun *>(GetParent());
		if(pTaskGun && pTaskGun->GetReloadIsBlocked() && GetWeaponControllerState(pPed) != WCS_Reload)
		{
			return FSM_Continue;
		}
		else
#endif // FPS_MODE_SUPPORTED
		{
			taskDebugf1("CTaskAimGunOnFoot::Aiming_OnUpdate: FSM_Quit - m_bIsReloading. Previous State: %d", GetPreviousState());
			return FSM_Quit;
		}
	}

	// Spin up if required while aiming
	if(pWeapon && CanSpinUp())
	{
		pWeapon->SpinUp(pPed);
	}
	
#if FPS_MODE_SUPPORTED	
	const CTaskMotionPed* pTaskMotionPed = static_cast<CTaskMotionPed*>(pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_PED));
	const bool bPlayingIdleFidgets = pTaskMotionPed != NULL && pTaskMotionPed->IsPlayingIdleFidgets();

	if( bCanFireWeapon && (!pPed->IsFirstPersonShooterModeEnabledForPlayer(false) || !bPlayingIdleFidgets) && (bShouldFireWeapon || m_bWantsToFireWhenWeaponReady) )
#else
	if( bCanFireWeapon && (bShouldFireWeapon || m_bWantsToFireWhenWeaponReady) )
#endif
	{
		bool bIsRecharging = pWeaponInfo->GetDisplayRechargeTimeHUD() && pWeapon->GetState() == CWeapon::STATE_WAITING_TO_FIRE && pWeaponInfo->GetTimeLeftBetweenShotsWhereShouldFireIsCached() == -1.0f;
		if (!pWeapon || pWeapon->GetState() == CWeapon::STATE_READY)
		{
			SetState(State_Firing);

			bool bSkipWantsToFireReset = false;
#if FPS_MODE_SUPPORTED
			if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && pWeaponInfo->GetGroup() != WEAPONGROUP_PETROLCAN)			
			{
				// In FPS, we wont reset m_bWantsToFireWhenWeaponReady until the weapon is actually fired 
				// since the task may still restart in the fire state in which case we could end up missing
				// the change in controller state to WCS_FIRE
				bSkipWantsToFireReset = true;
			}
#endif
			if(!bSkipWantsToFireReset)
			{
				m_bWantsToFireWhenWeaponReady = false;
			}
		}
		else if(pWeapon->GetState() == CWeapon::STATE_OUT_OF_AMMO || bIsRecharging)
		{
			if(m_bWantsToFireWhenWeaponReady || !m_bFiredOnceThisTriggerPress)
			{
				// Don't play the 'out of ammo" dialogue while recharging.
				if(!bIsRecharging)
				{
					// Swap weapon, as we have the incorrect (or holstered) weapon equipped
					if(g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::GroupShootout))
					{
						pPed->NewSay("OUT_OF_AMMO_GROUP_SHOOTOUT",0, false,false);
					}
					else
					{
						pPed->NewSay("OUT_OF_AMMO",0, false,false);
					}
				}

				pWeapon->GetAudioComponent().TriggerEmptyShot(pPed);

#if FPS_MODE_SUPPORTED
				// Play the empty fire anim
				CMoveNetworkHelper* pHelper = FindAimingNetworkHelper(); 
				if( pHelper && pHelper->IsNetworkAttached() )
				{
					CTaskMotionAiming* pMotionTask = FindMotionAimingTask();
					if(pMotionTask)
					{
						static const fwMvClipId s_dryFireClipId("dryfire_recoil",0x2F046329);
						if(fwAnimManager::GetClipIfExistsBySetId(pMotionTask->GetAimClipSet(), s_dryFireClipId))
						{
							pHelper->SendRequest(CTaskAimGunOnFoot::ms_EmptyFire);
						}
					}

					//Play the dry fire weapon anim.
					if(!pWeaponInfo->GetDontPlayDryFireAnim())
					{
						fwMvClipSetId weaponClipSetId = pWeaponInfo->GetAppropriateWeaponClipSetId(pPed);
						if(weaponClipSetId != CLIP_SET_ID_INVALID)
						{
							static const fwMvClipId s_weaponDryFireClipId("w_dryfire",0x2DC8C21D);
							if(fwAnimManager::GetClipIfExistsBySetId(weaponClipSetId, s_weaponDryFireClipId))
							{
								pWeapon->StartAnim(weaponClipSetId, s_weaponDryFireClipId, NORMAL_BLEND_DURATION, m_fFireAnimRate, 0.0f, false);
							}
						}
					}
#endif // FPS_MODE_SUPPORTED
				}
			}

#if FPS_MODE_SUPPORTED
			// Reset this flag to avoid keep playing the out-of-ammo audio in FPS RNG mode.
			m_bWantsToFireWhenWeaponReady = false;
			m_bFiredOnceThisTriggerPress = true;
#endif	// FPS_MODE_SUPPORTED
		}
		else if (pWeapon->GetState() == CWeapon::STATE_WAITING_TO_FIRE && pWeaponInfo && pWeaponInfo->GetTimeLeftBetweenShotsWhereShouldFireIsCached() != -1.0f )
		{
			//If we are waiting to fire then check if GetTimeLeftBetweenShotsWhereShouldFireIsCached is set.
			//GetTimeLeftBeforeShotWhereShouldFireIsCached is based off the time left for the weapon to be ready, set from GetTimeBetweenShots.
			//We store if we want to automatically shoot when it's ready.
			if(pWeapon->GetTimeBeforeNextShot() < pWeaponInfo->GetTimeLeftBetweenShotsWhereShouldFireIsCached())
			{
				if(!m_bWantsToFireWhenWeaponReady && pWeaponInfo->GetWantingToShootFireRateModifier() != -1.0f)
				{
					// Edit the fire rate to speed up the last shot so we can shoot the next bullet faster.
					// This won't affect fire events due to the animations needing to restart when entering state_firing
					CMoveNetworkHelper* pHelper = FindAimingNetworkHelper(); 
					if( pHelper && pHelper->IsNetworkAttached() )
					{
						m_fFireAnimRate = pWeaponInfo->GetWantingToShootFireRateModifier();
						pHelper->SetFloat( ms_FireRateId, m_fFireAnimRate );
					}
					
					pWeapon->SetTimer(fwTimer::GetTimeInMilliseconds() + static_cast<u32>((pWeapon->GetTimeBeforeNextShot() / m_fFireAnimRate) * 1000.0f)); 
				}

				m_bWantsToFireWhenWeaponReady = true;
			}
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskAimGunOnFoot::Aiming_OnExit()
{
	m_uNextShotAllowedTime = 0;

	CWeapon* pWeapon = GetPed()->GetWeaponManager()->GetEquippedWeapon();
	if(pWeapon)
	{
		bool bPairedAnimationInFPS = pWeapon->GetWeaponInfo() && pWeapon->GetWeaponInfo()->GetPairedWeaponHolsterAnimation() && GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_InFPSUnholsterTransition);
		if (!bPairedAnimationInFPS)
		{
			pWeapon->StopAnim();
		}

		SetClothPackageIndex(0);
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskAimGunOnFoot::Aiming_OnProcessMoveSignals()
{
	m_bMoveInterruptSettle |= GetMoveBoolean(ms_InterruptSettleId);
	m_bMoveAimIntroInterruptible |= GetMoveBoolean(ms_AimIntroInterruptibleId);

	return true;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskAimGunOnFoot::Firing_OnEnter()
{
	SetupToFire();

	m_bMoveFire						= false;
	m_bMoveFireLoop1				= false;
	m_bMoveFireLoop2				= false;
	m_bMoveCustomFireLoopFinished	= false;
	m_bMoveCustomFireIntroFinished  = false;
	m_bMoveCustomFireOutroFinished	= false;
	m_bMoveCustomFireOutroBlendOut  = false;
	m_bMoveDontAbortFireIntro		= false;
	m_bMoveFireLoop1Last			= true;
	m_bMoveFireInterruptable		= false;
	m_bMoveRecoilEnd				= false;
	m_bMoveFireLoopRevolverEarlyOutReload = false;

	// Ensure this frames move signals are up to date
	Firing_OnProcessMoveSignals();

	m_bFireWhenReady = false;
	m_previousControllerState = GetWeaponControllerState(GetPed());
	m_fTimeTryingToFire = 0.f;

	CPed* pPed = GetPed();
	CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
	const CWeaponInfo* pWeaponInfo = pWeapon ? pWeapon->GetWeaponInfo() : NULL;
	if (pWeaponInfo)
	{
		m_BulletsLeftAtStartOfFire = pWeapon->GetAmmoInClip();
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskAimGunOnFoot::Firing_OnUpdate()
{
	bool bMoveFire = m_bMoveFire;
	m_bMoveFire = false;

	CPed* pPed = GetPed();
	CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
	const CWeaponInfo* pWeaponInfo = pWeapon ? pWeapon->GetWeaponInfo() : NULL;
	const bool bCanFireAtTarget = CanFireAtTarget(pPed);

	if(!bCanFireAtTarget || m_bCancelledFireRequest)
	{
		m_bWantsToFireWhenWeaponReady = false;
		m_bFireWhenReady = false;
	}

	WeaponControllerState controllerState = GetWeaponControllerState(pPed);

	//	Rapid firing for pistols only
	if (pWeaponInfo && ( pWeaponInfo->GetAllowFireInterruptWhenReady() || 
						(pWeaponInfo->GetGroup() == WEAPONGROUP_PISTOL && !pWeaponInfo->GetFlag(CWeaponInfoFlags::UseRevolverBehaviour)) ))
	{
		if((controllerState == WCS_Fire || controllerState == WCS_FireHeld) && 
			(m_previousControllerState == WCS_Aim) &&
			bCanFireAtTarget)
		{
			m_bFireWhenReady = true;
		}
	}
	//	Store current controller state
	m_previousControllerState = controllerState;

	//Set the cache flags.
	CTaskMotionAiming* pMotionTask = FindMotionAimingTask();
	if(pMotionTask)
	{
		//	If we are wanting to fire again quickly then set the loop based on the interruptible flag
		if (m_bFireWhenReady)
		{
			if(m_bMoveFireLoop1Last)
			{
				m_bMoveFireLoop1 |= m_bMoveFireInterruptable;
			}
			else
			{
				m_bMoveFireLoop2 |= m_bMoveFireInterruptable;
			}
		}

		pMotionTask->SetMoveFlag(m_bMoveFireLoop1, ms_FireLoop1FinishedCacheId);
		pMotionTask->SetMoveFlag(m_bMoveFireLoop2, ms_FireLoop2FinishedCacheId);
	}

	bool bIsLastShotInRevolver = pWeaponInfo && pWeaponInfo->GetFlag(CWeaponInfoFlags::UseSingleActionBehaviour) && pWeapon && pWeapon->GetAmmoInClip() == 0;
	bool bCheckLoopEvent = (CheckLoopEvent() || (bIsLastShotInRevolver && m_bMoveFireLoopRevolverEarlyOutReload));

	m_bMoveFireLoop1 = false;
	m_bMoveFireLoop2 = false;
	m_bMoveCustomFireLoopFinished = false;
	m_bMoveFireInterruptable = false;

	bool bRecoilEnd = m_bMoveRecoilEnd;
	m_bMoveRecoilEnd = false;

	if(pPed->IsNetworkClone() && pWeaponInfo && pWeaponInfo->GetEffectGroup()==WEAPON_EFFECT_GROUP_ROCKET && CNetworkPendingProjectiles::GetProjectile(pPed,GetNetSequenceID(), false)==NULL)
	{   //If clone is firing a rocket wait until projectile is synced B*1972374
		return FSM_Continue;
	}
	
	if(m_bCrouchStateChanged || m_bForceRestart)
	{
		if (pWeaponInfo && pWeaponInfo->GetHash() == ATSTRINGHASH("WEAPON_DBSHOTGUN",0xEF951FBB) && m_bFiredOnce && !m_bFiredTwice)
		{
			//Force the second shot for DBS shotgun if we restart up midway
			pWeapon->SetNextShotAllowedTime(0);
			FireGun(pPed);
		}

		if(pMotionTask)
		{
			pMotionTask->SetMoveFlag(false, ms_FiringFlagId);
		}
		m_bCrouchStateChanged = false;
		m_bForceRestart = false;
		SetState(State_Start);
		return FSM_Continue;
	}

	if(pWeaponInfo)
	{
		if(pMotionTask)
		{
			const bool bUsingStealth = pPed->GetMotionData()->GetUsingStealth();
			if(bUsingStealth)
			{
				UpdateAdditiveWeights(pWeaponInfo->GetStealthFiringBreathingAdditiveWeight(), pWeaponInfo->GetStealthFiringLeanAdditiveWeight());
			}
			else
			{
				UpdateAdditiveWeights(pWeaponInfo->GetFiringBreathingAdditiveWeight(), pWeaponInfo->GetFiringLeanAdditiveWeight());
			}

			pMotionTask->SetMoveFloat(m_fBreathingAdditiveWeight, ms_AimingBreathingAdditiveWeight);
			pMotionTask->SetMoveFloat(m_fLeanAdditiveWeight, ms_AimingLeanAdditiveWeight);
		}
	}

	ProcessShouldReloadWeapon();

#if ENABLE_GUN_PROJECTILE_THROW
	// Switch to aiming state if we want to perform a grenade throw
	if (ShouldThrowGrenadeFromGunAim(pPed, pWeaponInfo))
	{
		SetState(State_ThrowingProjectileFromAiming);
		return FSM_Continue;
	}
#endif	//ENABLE_GUN_PROJECTILE_THROW

	if(!IsInFiringState())
		return FSM_Continue;

	if(m_bCancelledFireRequest)
	{
		taskDebugf1("CTaskAimGunOnFoot::Firing_OnUpdate: SetState(State_Aiming): Ped 0x%p [%s], m_bCancelledFireRequest %d", pPed, pPed->GetModelName(), m_bCancelledFireRequest);
		SetState(State_Aiming);
		return FSM_Continue;
	}

	// Clone just takes it's cue from the owner for blocked or not....
	if(!pPed->IsNetworkClone())
	{
		// Check if our gun is intersecting anything
		if(IsWeaponBlocked(pPed, false, 1.0f, 1.0f, false, true))
		{
			if (pWeaponInfo && pWeaponInfo->GetHash() == ATSTRINGHASH("WEAPON_DBSHOTGUN",0xEF951FBB) && m_bFiredOnce && !m_bFiredTwice)
			{
				//Force the second shot for DBS shotgun if we get blocked between the shots
				pWeapon->SetNextShotAllowedTime(0);
				FireGun(pPed);
			}

			// Our weapon is blocked, so we can't aim anymore
			SetState(State_Blocked);
			return FSM_Continue;
		}
	}

	//	We are wanting to fire again quickly so set controller state to fire
	if(m_bFireWhenReady FPS_MODE_SUPPORTED_ONLY( || m_bWantsToFireWhenWeaponReady))
	{
		controllerState = WCS_Fire;
	}

	bool bOnlyExitFireLoopAtEnd = GetGunFlags().IsFlagSet(GF_OnlyExitFireLoopAtEnd);
	if(bOnlyExitFireLoopAtEnd)
	{
		if(controllerState == WCS_None)
		{
			if(pPed->GetIsInCover() ||
				(pWeapon->GetHasFirstPersonScope() && !(pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON) || pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_RUN_AND_GUN))))
			{
				bOnlyExitFireLoopAtEnd = false;
			}
		}

		// If we somehow end up with only one bullet left on the DB Shotgun, this is a valid case to early out after only one fire event
		if (pWeaponInfo->GetForceFullFireAnimation() && m_BulletsLeftAtStartOfFire == 1 && pWeapon->GetIsClipEmpty())
		{
			bOnlyExitFireLoopAtEnd = false;
		}
	}

	// Wait until the recoil has finished.
	bool bOnlyExitAfterRecoil = false;
#if FPS_MODE_SUPPORTED
	if(!bRecoilEnd && pWeaponInfo && pWeaponInfo->GetFPSOnlyExitFireAnimAfterRecoilEnds() && pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		// Due to the way we're using existing animations, we only want this to trigger when in FPSScope state on the Marksman Rifle Mk II
		if (pWeaponInfo->GetHash() == ATSTRINGHASH("WEAPON_MARKSMANRIFLE_MK2", 0x6a6c02e0))
		{
			bOnlyExitAfterRecoil = pPed->GetMotionData()->GetIsFPSScope();
		}
		else
		{
			bOnlyExitAfterRecoil = true;
		}
	}
#endif // FPS_MODE_SUPPORTED

	bool bAllowedToExitState = true;
	if(m_bFiredOnce && ((bOnlyExitFireLoopAtEnd && !bCheckLoopEvent) || bOnlyExitAfterRecoil))
	{
		// Flag as not firing to prevent network from going into the next loop
		if(pMotionTask)
		{
			pMotionTask->SetMoveFlag(false, ms_FiringFlagId);
		}
		bAllowedToExitState = false;
	}
	else
	{
		const crClip* pClip = ChooseFiringMedClip();
		if(pClip)
		{
			if(pMotionTask)
			{
				pMotionTask->SetMoveClip(pClip, ms_FireMedLoopClipId);
			}
		}

		if(pMotionTask)
		{
			pMotionTask->SetMoveFlag(true, ms_FiringFlagId);
		}
	}
	
	//If we are using a firing variation, and the clip ends, go back to aiming.
	if(m_bUsingFiringVariation && bCheckLoopEvent)
	{
		taskDebugf1("CTaskAimGunOnFoot::Firing_OnUpdate: SetState(State_Aiming): Ped 0x%p [%s], m_bUsingFiringVariation %d, bCheckLoopEvent %d", pPed, pPed->GetModelName(), m_bUsingFiringVariation, bCheckLoopEvent);
		SetState(State_Aiming);
		return FSM_Continue;
	}

	if(bAllowedToExitState)
	{
		if(pPed->GetWeaponManager()->GetRequiresWeaponSwitch() && !m_iFlags.IsFlagSet(GF_PreventWeaponSwapping))
		{
			return FSM_Quit;
		}

		if(!pPed->IsNetworkClone() && IsOutOfAmmo())
		{
			// Allow sniper rifles to keep aiming when running out of ammo.
			if(pPed->IsLocalPlayer() && IsAimingScopedWeapon())
			{
				taskDebugf1("CTaskAimGunOnFoot::Firing_OnUpdate: SetState(State_Aiming): Ped 0x%p [%s], IsOutOfAmmo: GF_AllowAimingWithNoAmmo %d", pPed, pPed->GetModelName(), GetGunFlags().IsFlagSet(GF_AllowAimingWithNoAmmo));
				SetState(State_Aiming);
				return FSM_Continue;
			}
			else
			{
				return FSM_Quit;
			}
		}

		// We need to wait until the fire loop ends before reloading
		if(m_bIsReloading)
		{
			return FSM_Quit;
		}

		bool bOnlyAllowFiring = pWeaponInfo ? pWeaponInfo->GetOnlyAllowFiring() : false;

		if(!CanFireAtTarget(pPed) && !bOnlyAllowFiring)
		{
			taskDebugf1("CTaskAimGunOnFoot::Firing_OnUpdate: SetState(State_Aiming): Ped 0x%p [%s], CanFireAtTarget", pPed, pPed->GetModelName());
			SetState(State_Aiming);
			return FSM_Continue;
		}
	}

	if(!GetGunFlags().IsFlagSet(GF_FireAtLeastOnce))
	{
		if(bAllowedToExitState && pWeaponInfo && pWeaponInfo->GetDontBlendFireOutro() && 
		  ( m_bMoveCustomFireIntroFinished || m_bMoveDontAbortFireIntro ) && 
		  (controllerState == WCS_None || controllerState == WCS_Aim))
		{
			SetState(State_FireOutro);
			return FSM_Continue;			
		}

		if(bAllowedToExitState && controllerState == WCS_Aim)
		{
			if(pPed->GetWeaponManager()->GetEquippedWeaponInfo()->GetOnlyAllowFiring() FPS_MODE_SUPPORTED_ONLY(&& !pPed->IsFirstPersonShooterModeEnabledForPlayer(true)))
			{
				return FSM_Quit;
			}
			else
			{
				taskDebugf1("CTaskAimGunOnFoot::Firing_OnUpdate: SetState(State_Aiming): Ped 0x%p [%s], bAllowedToExitState %d, controllerState %d, FPS %d, FPS Strafe %d", pPed, pPed->GetModelName(), bAllowedToExitState, controllerState,pPed->IsFirstPersonShooterModeEnabledForPlayer(false), pPed->IsFirstPersonShooterModeEnabledForPlayer(true));
				SetState(State_Aiming);
				return FSM_Continue;
			}
		}
		else if(controllerState == WCS_Reload || controllerState == WCS_Fire || controllerState == WCS_FireHeld)
		{
			// Continue to aim
		}
		else if(bAllowedToExitState && controllerState == WCS_None)
		{
			if(pPed->IsLocalPlayer())
			{
				if(!pPed->GetWeaponManager()->GetEquippedWeaponInfo()->GetOnlyAllowFiring() FPS_MODE_SUPPORTED_ONLY(|| pPed->IsFirstPersonShooterModeEnabledForPlayer(true)))
				{
					SetState(State_AimOutro);
					return FSM_Continue;
				}
			}
			return FSM_Quit;
		}

		// Cover sets player free aiming so we can't just check the weapon controller state
		if(pPed->GetIsInCover() && pPed->IsLocalPlayer() && !CPlayerInfo::IsFiring_s() &&  bAllowedToExitState)
		{
			return FSM_Quit;
		}
	}

	if(bAllowedToExitState && pWeapon && !GetGunFlags().IsFlagSet(GF_FireAtLeastOnce) && (pWeapon->GetWeaponInfo()->GetOnlyFireOneShot() || pWeapon->GetWeaponInfo()->GetOnlyFireOneShotPerTriggerPress()) && pWeapon->GetState() == CWeapon::STATE_WAITING_TO_FIRE)
	{
		taskDebugf1("CTaskAimGunOnFoot::Firing_OnUpdate: SetState(State_Aiming): Ped 0x%p [%s], bAllowedToExitState %d, GF_FireAtLeastOnce %d, GetOnlyFireOneShot %d, GetOnlyFireOneShotPerTriggerPress %d, pWeapon->GetState() %d", pPed, pPed->GetModelName(), bAllowedToExitState, GetGunFlags().IsFlagSet(GF_FireAtLeastOnce), pWeapon->GetWeaponInfo()->GetOnlyFireOneShot(), pWeapon->GetWeaponInfo()->GetOnlyFireOneShotPerTriggerPress(), pWeapon->GetState());
		SetState(State_Aiming);
		return FSM_Continue;
	}
	else if(bAllowedToExitState && controllerState == WCS_CombatRoll)
	{
		SetState(State_CombatRoll);
		return FSM_Continue;
	}
	else if(ShouldFireWeapon() FPS_MODE_SUPPORTED_ONLY( || m_bWantsToFireWhenWeaponReady))
	{
		bool bFire;

		CTaskMotionAiming* pMotionTask = FindMotionAimingTask();
		// Firing variations always use fire events
		if((pMotionTask && pMotionTask->GetUsesFireEvents()) || m_bUsingFiringVariation)
		{
			m_bForceWeaponStateToFire = false;
			bFire = false;

			// Only fire on fire events
			if(bMoveFire)
			{
				if(m_bSuppressNextFireEvent)
				{
					m_bSuppressNextFireEvent = false;
				}
				else
				{
					bFire = true;
					m_bForceWeaponStateToFire = true;
				}
			}
		}
		else
		{
			bFire = true;
		}

		if(bFire)
		{
			TriggerFire();
		}
	}
	else if(bAllowedToExitState && (!pPed->GetWeaponManager()->GetEquippedWeaponInfo()->GetOnlyAllowFiring() FPS_MODE_SUPPORTED_ONLY(|| pPed->IsFirstPersonShooterModeEnabledForPlayer(true))))
	{
		taskDebugf1("CTaskAimGunOnFoot::Firing_OnUpdate: SetState(State_Aiming): Ped 0x%p [%s], bAllowedToExitState %d, GetOnlyAllowFiring %d, FPS %d, FPS Strafe %d", pPed, pPed->GetModelName(), bAllowedToExitState, pPed->GetWeaponManager()->GetEquippedWeaponInfo()->GetOnlyAllowFiring(), pPed->IsFirstPersonShooterModeEnabledForPlayer(false), pPed->IsFirstPersonShooterModeEnabledForPlayer(true));
		SetState(State_Aiming);
		return FSM_Continue;
	}

	if(controllerState == WCS_Fire)
	{
		m_fTimeTryingToFire += GetTimeStep();
		static dev_float FIRING_TIME_OUT = 5.f;
		if(m_fTimeTryingToFire > FIRING_TIME_OUT)
		{
			taskDebugf1("CTaskAimGunOnFoot::Firing_OnUpdate: SetState(State_Aiming): Ped 0x%p [%s], m_fTimeTryingToFire %f, FIRING_TIME_OUT %f", pPed, pPed->GetModelName(), m_fTimeTryingToFire, FIRING_TIME_OUT);
			SetState(State_Aiming);
			return FSM_Continue;
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskAimGunOnFoot::Firing_OnExit()
{
	CPed* pPed = GetPed();

	RequestAim();

	SetMoveFlag(false, ms_FiringFlagId);
	m_bFire = false;

	SetMoveFlag(false, ms_FireLoop1FinishedCacheId);
	SetMoveFlag(false, ms_FireLoop2FinishedCacheId);
	
	if(pPed && pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetEquippedWeapon())
		pPed->GetWeaponManager()->GetEquippedWeapon()->StoppedFiring();

	//Check if we were using a firing variation.
	if(m_bUsingFiringVariation)
	{
		//Note that we are no longer using a firing variation.
		taskDebugf1("CTaskAimGunOnFoot::Firing_OnExit: m_bUsingFiringVariation = FALSE, Ped 0x%p [%s], m_bUsingFiringVariation %d", pPed, pPed ? pPed->GetModelName() : "null", m_bUsingFiringVariation);
		m_bUsingFiringVariation = false;
		
		//Clear the flag for the custom fire loop.
		SetMoveFlag(false, ms_HasCustomFireLoopFlagId);
		
		//If we were forcefully restarted, don't set the last firing variation time.
		//This is a bit of a hack, but without this, the firing variations are not
		//run as frequently as they should be.
		if(GetState() != State_Start)
		{
			pPed->GetPedIntelligence()->SetLastFiringVariationTime(fwTimer::GetTimeInMilliseconds());
		}
	}

	GetGunFlags().ClearFlag(GF_FireAtLeastOnce);

	// Unflag
	m_bDoneSetupToFire = false;
	m_bForceFiringState = false;
	m_bSuppressNextFireEvent = false;

	m_bFireWhenReady = false;

	taskDebugf1("CTaskAimGunOnFoot::Firing_OnExit: m_bCancelledFireRequest = FALSE, Ped 0x%p [%s], m_bCancelledFireRequest %d", pPed, pPed ? pPed->GetModelName() : "null", m_bCancelledFireRequest);
	m_bCancelledFireRequest = false;

#if FPS_MODE_SUPPORTED
	// Reset the flag just in case
	if(pPed)
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_FPSFidgetsAbortedOnFire, false);
#endif

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskAimGunOnFoot::FireOutro_Update()
{
	WeaponControllerState controllerState = GetWeaponControllerState(GetPed());
	if(controllerState == WCS_CombatRoll)
	{
		SetState(State_CombatRoll);
		return FSM_Continue;
	}

	if(!m_bMoveCustomFireOutroFinished && !m_bMoveCustomFireOutroBlendOut)
	{
		static dev_float MAX_TIME_IN_STATE = 2.f;
		if(GetTimeInState() > MAX_TIME_IN_STATE)
		{
			taskAssertf(0, "In FireOutro state too long, probably out of sync with move network");
			return FSM_Quit;
		}
		return FSM_Continue;
	}
	return FSM_Quit;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskAimGunOnFoot::Firing_OnProcessMoveSignals()
{
	const CTaskMotionAiming* pMotionTask = FindMotionAimingTask();
	if(pMotionTask)
	{
		m_bMoveFire						|= pMotionTask->GetMoveBoolean(ms_FireId);
		m_bMoveFireLoop1				|= pMotionTask->GetMoveBoolean(CTaskMotionAiming::ms_FireLoop1);
		m_bMoveFireLoop2				|= pMotionTask->GetMoveBoolean(CTaskMotionAiming::ms_FireLoop2);
		m_bMoveCustomFireLoopFinished	|= pMotionTask->GetMoveBoolean(CTaskMotionAiming::ms_CustomFireLoopFinished);
		m_bMoveCustomFireIntroFinished	|= pMotionTask->GetMoveBoolean(CTaskMotionAiming::ms_CustomFireIntroFinished);
		m_bMoveCustomFireOutroFinished	|= pMotionTask->GetMoveBoolean(CTaskMotionAiming::ms_CustomFireOutroFinishedId);
		m_bMoveCustomFireOutroBlendOut	|= pMotionTask->GetMoveBoolean(CTaskMotionAiming::ms_CustomFireOutroBlendOut);
		m_bMoveDontAbortFireIntro		|= pMotionTask->GetMoveBoolean(CTaskMotionAiming::ms_DontAbortFireIntroId);
		m_bMoveFireInterruptable		|= pMotionTask->GetMoveBoolean(ms_FireInterruptable);
		m_bMoveRecoilEnd				|= pMotionTask->GetMoveBoolean(ms_MoveRecoilEnd);
		m_bMoveFireLoopRevolverEarlyOutReload	|= pMotionTask->GetMoveBoolean(ms_FireLoopRevolverEarlyOutReload);

		if(pMotionTask->GetMoveBoolean(ms_FireLoop1StateId))
		{
			m_bMoveFireLoop1Last = true;
		}
		if(pMotionTask->GetMoveBoolean(ms_FireLoop2StateId))
		{
			m_bMoveFireLoop1Last = false;
		}
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskAimGunOnFoot::SetClothPackageIndex( u8 packageIndex, u8 transitionFrames )
{
	CPed* pPed = GetPed();
	if(pPed && pPed->IsLocalPlayer() )
	{
		const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
		if(pWeapon)
		{
			const CWeaponInfo* pWeaponInfo = pWeapon->GetWeaponInfo();
			if(pWeaponInfo && pWeaponInfo->GetIsHeavy() )
			{
				pPed->SetClothPackageIndex(packageIndex, transitionFrames);
			}
		}
	}
}

CTask::FSM_Return CTaskAimGunOnFoot::AimToIdle_OnEnter()
{
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskAimGunOnFoot::AimToIdle_OnUpdate()
{
	CPed* pPed = GetPed();

	if (pPed->IsLocalPlayer())
	{
		WeaponControllerState controllerState = GetWeaponControllerState(pPed);

		if (controllerState == WCS_Aim || controllerState == WCS_Reload || controllerState == WCS_Fire || controllerState == WCS_FireHeld)
		{
			taskDebugf1("CTaskAimGunOnFoot::AimToIdle_OnUpdate: SetState(State_Aiming): Ped 0x%p [%s], controllerState %d", pPed, pPed->GetModelName(), controllerState);
			SetState(State_Aiming);
			return FSM_Continue;
		}

		CControl* pControl = pPed->GetControlFromPlayer();
		if (pControl)
		{
			Vector2 vecStick(pControl->GetPedWalkLeftRight().GetNorm(), -pControl->GetPedWalkUpDown().GetNorm());

			TUNE_GROUP_FLOAT(NEW_GUN_TASK, INPUT_MAG_TOL, 0.05f, 0.0f, 1.0f, 0.01f);

			if (vecStick.Mag() > INPUT_MAG_TOL)
			{
				return FSM_Quit;
			}
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskAimGunOnFoot::AimToIdle_OnExit()
{
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskAimGunOnFoot::AimOutro_OnUpdate()
{
	CPed* pPed = GetPed();
	WeaponControllerState controllerState = GetWeaponControllerState(pPed);

	// Harpoon: Play outro if we are leaving from aiming
	bool bPlayingOutro = false;
	CTaskMotionAiming* pMotionTask = FindMotionAimingTask();
	if (pMotionTask && (pMotionTask->GetState() == CTaskMotionAiming::State_SwimIdle || pMotionTask->GetState() == CTaskMotionAiming::State_SwimStrafe || pMotionTask->GetState() == CTaskMotionAiming::State_SwimIdleOutro))
	{
		pMotionTask->PlaySwimIdleOutro(true);
		bPlayingOutro = true;
		if (!pMotionTask->GetSwimIdleOutroFinished())
		{
			return FSM_Continue;
		}
	}
	// Harpoon: Don't re-aim 
	if ((controllerState == WCS_Aim || controllerState == WCS_Reload || controllerState == WCS_Fire || controllerState == WCS_FireHeld) && !bPlayingOutro)
	{
		taskDebugf1("CTaskAimGunOnFoot::AimOutro_OnUpdate: SetState(State_Aiming): Ped 0x%p [%s], controllerState %d", pPed, pPed->GetModelName(), controllerState);
		SetState(State_Aiming);
		return FSM_Continue;
	}
	else if(controllerState == WCS_CombatRoll)
	{
		SetState(State_CombatRoll);
		return FSM_Continue;
	}

	u32 uLastIsAimingTime = 0;
	if(GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_GUN)
	{
		uLastIsAimingTime = static_cast<CTaskGun *>(GetParent())->GetLastIsAimingTimeForOutroDelay();
	} 

	float fOutroTime = 0.0f;
	u32 uCurrentTime = fwTimer::GetTimeInMilliseconds();
	//@@: location CTASKAIMGUNONFOOT_AIMOUTRO_NOT_AIMING_RECENTLY
	bool bNotAimingRecently = (uCurrentTime > (uLastIsAimingTime + (u32)(sm_Tunables.m_TimeForRunAndGunOutroDelays * 1000)));

	//! If we haven't been aiming recently, or we have been attempting run and gun for a set period of time, do outro delay.
	bool bKeepInOutroIfWeaponSelectIsUp = false;
	if(bNotAimingRecently)
	{
		if (pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON))
		{
			fOutroTime = sm_Tunables.m_AssistedAimOutroTime;
			bKeepInOutroIfWeaponSelectIsUp = true;
		}
		else if (pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_RUN_AND_GUN))
		{
			fOutroTime = sm_Tunables.m_RunAndGunOutroTime;
			bKeepInOutroIfWeaponSelectIsUp = true;
		}
	}
	else
	{
		fOutroTime = sm_Tunables.m_AimOutroTime;
	}

	bool bUsingAimStick = false;

	if (pPed->IsLocalPlayer())
	{
		CControl *pControl = pPed->GetControlFromPlayer();
		Vector3 vStickInput(pControl->GetPedLookLeftRight().GetNorm(), -pControl->GetPedLookUpDown().GetNorm(), 0.0f);
		if(vStickInput.Mag2() > 0.01f)
		{
			m_uLastUsingAimStickTime = fwTimer::GetTimeInMilliseconds();
		}

		bUsingAimStick = fwTimer::GetTimeInMilliseconds() < (m_uLastUsingAimStickTime + (u32)(sm_Tunables.m_AimingOnStickExitCooldown * 1000));

		//Exit aim mode weapon select pressed or held.
		if (pControl->GetPedSprintIsDown() && (GetTimeRunning() > sm_Tunables.m_AimOutroMinTaskTimeWhenRunPressed))
		{
			return FSM_Quit;
		}

		if(bKeepInOutroIfWeaponSelectIsUp)
		{
			//If we have switched weapons, quit.
			CPedWeaponManager *pWeapMgr = pPed->GetWeaponManager();
			if (pWeapMgr)
			{
				if(pWeapMgr->GetEquippedWeaponInfoFromObject() != pWeapMgr->GetSelectedWeaponInfo())
				{
					if(GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_GUN)
					{
						static_cast<CTaskGun*>(GetParent())->GetGunFlags().SetFlag(GF_ForceAimOnce);
					}
					return FSM_Quit;
				}
			}
		}
		else if(pControl->GetSelectWeapon().IsDown())
		{
			return FSM_Quit;
		}

		const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
		if (pWeapon && pWeapon->GetHasFirstPersonScope())
		{
			if (!pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON))
			{
				return FSM_Quit;
			}

			CControl *pControl = pPed->GetControlFromPlayer();
			Vector2 vecStickInput;
			vecStickInput.x = pControl->GetPedWalkLeftRight().GetNorm();
			vecStickInput.y = -pControl->GetPedWalkUpDown().GetNorm();

			TUNE_GROUP_FLOAT(ASSISTED_AIM_TUNE, MIN_STICK_INPUT_FOR_MOVEMENT_SQR, 0.05f, 0.0f, 1.0f, 0.01f);
			if (vecStickInput.Mag2() >= MIN_STICK_INPUT_FOR_MOVEMENT_SQR)
			{
				return FSM_Quit;
			}
		}

		if( IsWeaponBlocked(pPed, true) ) 
		{
			return FSM_Quit;
		}
	}
	
	//! Note: If outro time is zero, this indicates we don't want to use it.
	if(fOutroTime > 0.0f && (sm_Tunables.m_AimOutroTimeIfAimingOnStick > fOutroTime) && bUsingAimStick)
	{
		if(GetTimeInState() > sm_Tunables.m_AimOutroTimeIfAimingOnStick)
		{
			return FSM_Quit;
		}
	}
	else if ( (GetTimeInState() > fOutroTime))
	{
		return FSM_Quit;
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskAimGunOnFoot::UpdateAIBlockStatus(CPed& ped)
{
	bool bBlockedNear = false;
	LosStatus losStatus = GetTargetLos(ped, bBlockedNear);
	if ( (bBlockedNear || ped.GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_RequiresLosToAim)) &&
		 (losStatus == Los_blocked || losStatus == Los_blockedByFriendly) )
	{
		GetGunFlags().SetFlag(GF_ForceBlockedClips);
	}
	else
	{
		GetGunFlags().ClearFlag(GF_ForceBlockedClips);
	}
}

////////////////////////////////////////////////////////////////////////////////

LosStatus CTaskAimGunOnFoot::GetTargetLos(CPed& ped, bool& bBlockedNear)
{
	// If our parent gun task is trying to force blocked clips
	CTask* pParentTask = GetParent();
	if(pParentTask && pParentTask->GetTaskType() == CTaskTypes::TASK_GUN && static_cast<CTaskGun*>(pParentTask)->GetGunFlags().IsFlagSet(GF_ForceBlockedClips))
	{
		bBlockedNear = true;
		return Los_blocked;
	}

	const CEntity* pEntity = m_target.GetEntity();
	if (pEntity && pEntity->GetIsTypePed() && !ped.GetPedIntelligence()->IsFriendlyWith(*static_cast<const CPed*>(pEntity)))
	{
		// If arresting and too close to the target then we should consider our weapon blocked
		if(ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ARREST_PED))
		{
			ScalarV scMinDistSq = LoadScalar32IntoScalarV(CTaskCombat::ms_Tunables.m_MinDistanceForAimIntro);
			scMinDistSq = (scMinDistSq * scMinDistSq);
			if(IsLessThanAll(DistSquared(ped.GetTransform().GetPosition(), pEntity->GetTransform().GetPosition()), scMinDistSq))
			{
				bBlockedNear = true;
				return Los_blocked;
			}
		}

		CPedTargetting* pTargetting = ped.GetPedIntelligence()->GetTargetting(true);
		taskAssertf(pTargetting, "Targeting failed to be initialised!");
		if (!pTargetting)
			return Los_blocked;

		CSingleTargetInfo* pTargetInfo = pTargetting->FindTarget(pEntity);
		if( pTargetInfo == NULL )
		{
			pTargetting->RegisterThreat(pEntity, true, true);
			pTargetInfo = pTargetting->FindTarget(pEntity);
		}

		if (taskVerifyf(pTargetInfo, "Target targetinfo not found!"))
		{
			bBlockedNear = pTargetInfo->GetLosBlockedNear();
			return pTargetInfo->GetLosStatus();
		}
	}
	return Los_unchecked;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskAimGunOnFoot::Blocked_OnEnter()
{
	SetNewTask( rage_new CTaskWeaponBlocked( m_weaponControllerType ) );
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskAimGunOnFoot::ProcessShouldReloadWeapon()
{
	if(ShouldReloadWeapon())
	{
		// Flag to reload when able
		m_bIsReloading = true;
		return true;
	}

	return false;
}

CTask::FSM_Return CTaskAimGunOnFoot::Blocked_OnUpdate()
{
	CPed* pPed = GetPed();

	// Don't quit task in FPS as we added weapon blocked for 1st person reload task. Quiting task will cause infinite looping between two tasks.
	if(FPS_MODE_SUPPORTED_ONLY(!pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true)) && ProcessShouldReloadWeapon() )
		return FSM_Quit;

#if FPS_MODE_SUPPORTED
	// Allow weapon swapping in FPS idle state.
	if (GetWaitingToSwitchWeapon())
	{
		if(GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_GUN)
		{
			static_cast<CTaskGun*>(GetParent())->GetGunFlags().SetFlag(GF_AllowAimingWithNoAmmo);
		}
		return FSM_Quit;
	}
#endif	// FPS_MODE_SUPPORTED

	if( !GetGunFlags().IsFlagSet( GF_ForceBlockedClips ) )
	{	
		// Get the target position
		Vector3 vTargetPos;
		if( m_target.GetIsValid() && m_target.GetPositionWithFiringOffsets( pPed, vTargetPos ) )
		{
			// increase the length and width of the capsule when unblocking weapon
 			static dev_float cfProbeLengthMultiplier = 1.25f;
			static dev_float cfCapsuleRadiusMultiplier = 1.05f;
		
			if( GetIsFlagSet( aiTaskFlags::SubTaskFinished ) )
			{
				return FSM_Quit;
			}
			else 
			{
				// Stay blocked if we are still transitioning into blocking and there is a alternative clipset
				bool bStayInState = false;
				CTaskWeaponBlocked* pBlockedTask = static_cast<CTaskWeaponBlocked*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_WEAPON_BLOCKED));
				if(pBlockedTask && pBlockedTask->GetTimeRunning() < 0.3f )
				{
					//Ensure the weapon manager is valid.
					const CPedWeaponManager* pWeaponManager = pPed->GetWeaponManager();
					if(pWeaponManager)
					{
						//Grab the weapon info.
						const CWeapon* pWeapon = pWeaponManager->GetEquippedWeapon();
						const CWeaponInfo* pWeaponInfo = pWeapon ? pWeapon->GetWeaponInfo() : NULL;
						fwMvClipSetId appropriateClipSetId = (pWeaponInfo ? pWeaponInfo->GetAlternativeClipSetWhenBlockedId(*pPed) : CLIP_SET_ID_INVALID);
						if(appropriateClipSetId != CLIP_SET_ID_INVALID)
						{
							bStayInState = true;
						}
					}
				}

				// Stay in the blocked state for a minimum time.
				TUNE_GROUP_FLOAT(WEAPON_BLOCK, fMiniTimeStayInBlockState, 0.5f, 0.0f, 5.0f, 0.1f); 
				bool bShouldFire = ShouldFireWeapon();
				if(!bStayInState && GetTimeInState() > fMiniTimeStayInBlockState && !IsWeaponBlocked( pPed, false, cfProbeLengthMultiplier, cfCapsuleRadiusMultiplier, false, bShouldFire ))
				{
					if(bShouldFire)
					{
						GetGunFireFlags().SetFlag(GFF_SmashGlassFromBlockingState);
					}

					WeaponControllerState controllerState = GetWeaponControllerState(pPed);
					bool bNoLongerWantingGunTask = controllerState != WCS_Aim && controllerState != WCS_Reload && controllerState != WCS_Fire && controllerState != WCS_FireHeld;
					if(bNoLongerWantingGunTask)
					{
						return FSM_Quit;
					}

					// Set state to decide what to do next
					SetState( State_Aiming );
					return FSM_Continue;
				}
			}
		}
	}

	// Set the reset flag to let the hud know we want to dim the reticule
	pPed->SetPedResetFlag( CPED_RESET_FLAG_DimTargetReticule, true );
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskAimGunOnFoot::FSM_Return CTaskAimGunOnFoot::CombatRoll_OnEnter()
{
    // increment the roll token to stop clone peds doing too many rolls on remote machines
    if(!GetPed()->IsNetworkClone())
    {
        m_LastLocalRollToken++;
        m_LastLocalRollToken%=MAX_ROLL_TOKEN;
    }
    else
    {
        m_LastCloneRollToken = m_LastLocalRollToken;
    }

	GetPed()->GetMotionData()->SetCombatRoll(true);
	m_bForceRoll = false;

	// clear any pending fire when we start the combat roll (url:bugstar:4729380)
	if (NetworkInterface::IsGameInProgress())
	{
		GetGunFlags().ClearFlag(GF_FireAtLeastOnce);
		m_bWantsToFireWhenWeaponReady = false;
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskAimGunOnFoot::FSM_Return CTaskAimGunOnFoot::CombatRoll_OnUpdate()
{
	if(!GetPed()->GetMotionData()->GetCombatRoll())
	{
		SetState(State_Start);
		return FSM_Continue;
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

#if ENABLE_GUN_PROJECTILE_THROW

CTaskAimGunOnFoot::FSM_Return CTaskAimGunOnFoot::ThrowingProjectileFromAiming_OnEnter()
{
	CPed *pPed = GetPed();
	CWeapon* pWeapon = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeapon() : NULL;
	if (pWeapon)
	{
		ProcessGrenadeThrowFromGunAim(pPed, pWeapon);

        // cache the net sequence when starting the subtask
        if(pPed->IsNetworkClone())
        {
            if(pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_AIM_AND_THROW_PROJECTILE))
            {
                m_LastThrowProjectileSequence = pPed->GetPedIntelligence()->GetQueriableInterface()->GetTaskNetSequenceForType(CTaskTypes::TASK_AIM_AND_THROW_PROJECTILE);
            }
        }
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskAimGunOnFoot::FSM_Return CTaskAimGunOnFoot::ThrowingProjectileFromAiming_OnUpdate()
{
	// If subtask has finished/been destroyed, return to aiming state.
	if (GetIsFlagSet( aiTaskFlags::SubTaskFinished ) || !GetSubTask())
	{
        // if we have finished the task locally and not received updated state from the owner, wait
        CPed *pPed = GetPed();

        if(pPed->IsNetworkClone())
        {
            if(GetStateFromNetwork() == State_ThrowingProjectileFromAiming)
            {
                if(pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_AIM_AND_THROW_PROJECTILE))
                {
                    u32 netSequence = pPed->GetPedIntelligence()->GetQueriableInterface()->GetTaskNetSequenceForType(CTaskTypes::TASK_AIM_AND_THROW_PROJECTILE);

                    if(netSequence == m_LastThrowProjectileSequence)
                    {
                        return FSM_Continue;
                    }
                }
            }
        }

		taskDebugf1("CTaskAimGunOnFoot::ThrowingProjectileFromAiming_OnUpdate: SetState(State_Aiming): Ped 0x%p [%s]", GetPed(), GetPed()->GetModelName());
		SetState(State_Aiming);
		return FSM_Continue;
	}
	return FSM_Continue;
}

#endif	//ENABLE_GUN_PROJECTILE_THROW

////////////////////////////////////////////////////////////////////////////////

float CTaskAimGunOnFoot::GetCurrentPitch() const
{
	const CTaskMotionAiming* pMotionTask = FindMotionAimingTask();
	if(pMotionTask)
	{
		return pMotionTask->GetCurrentPitch();
	}
	
	return 0.0f;
}

////////////////////////////////////////////////////////////////////////////////

WeaponControllerState CTaskAimGunOnFoot::GetWeaponControllerState(const CPed* pPed) const
{
    WeaponControllerState state = WCS_None;

	if(taskVerifyf(pPed, "Invalid ped passed to GetWeaponControllerState!"))
    {
        if(pPed->IsNetworkClone())
        {
			if (m_bForceFire)
			{
				state = WCS_Fire;
			}
			else
			{
				switch(GetStateFromNetwork())
				{
				case State_Aiming:
					state = WCS_Aim;
					break;
				case State_Firing:
					if(m_bCloneWeaponControllerFiring)
					{
						state = WCS_Fire;
					}
					else
					{
						state = WCS_Aim;
					}
					break;
//              case State_Reloading:
//                  state = WCS_Reload;
//                  break;
				default:
					// set the default state to aiming in the network game, as this
					// will generally be the case if we are not firing or reloading and running the task
					state = WCS_Aim;
					break;
				}
			}
        }
        else
        {
			state = CWeaponControllerManager::GetController(m_weaponControllerType)->GetState(pPed, GetGunFlags().IsFlagSet(GF_FireAtLeastOnce));

			//Keep m_bCloneWeaponControllerFiring updated for remote
			if( GetState()==State_Firing && (state == WCS_Fire || state == WCS_FireHeld) )
			{
				m_bCloneWeaponControllerFiring =true;
			}
			else
			{
				m_bCloneWeaponControllerFiring =false;
			}
        }
    }

    return state;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskAimGunOnFoot::RequestAim()
{
	CPed* ped = GetPed();

	CTaskMotionAiming* pMotionTask = FindMotionAimingTask();

	if(!pMotionTask)
	{
		return;
	}
	
	if(!GetIsFlagSet(aiTaskFlags::IsAborted))
	{
		pMotionTask->RequestAim();
	}

	if(ped && ped->GetWeaponManager())
	{
		CWeapon* pWeapon = ped->GetWeaponManager()->GetEquippedWeapon();
		if(pWeapon)
		{
			static const fwMvClipId s_WeaponAimClipId("w_aim",0xF4BFC82D);

			if(taskVerifyf(pWeapon->GetWeaponInfo(), "NULL weapon info"))
			{
				// Perform an clip on the weapon if one is specified
				pWeapon->StartAnim(pMotionTask->GetAimClipSet(), s_WeaponAimClipId, NORMAL_BLEND_DURATION, pWeapon->GetWeaponInfo()->GetAnimReloadRate(), 0.f, true);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskAimGunOnFoot::IsInAimState()
{
	CTaskMotionAiming* pMotionTask = FindMotionAimingTask();

	Assert(GetPed());

	if(!pMotionTask && GetPed()->IsNetworkClone())
	{	//Possible for a clone to be waiting for the right motion task here
		return false;
	}

	if (pMotionTask)
	{
		return pMotionTask->EnteredAimState();
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskAimGunOnFoot::IsInFiringState()
{
	CTaskMotionAiming* pMotionTask = FindMotionAimingTask();

	if(!pMotionTask && GetPed()->IsNetworkClone())
	{	//Possible for a clone to be waiting for the right motion task here
		return false;
	}

	if (pMotionTask)
	{
		return pMotionTask->EnteredFiringState();
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskAimGunOnFoot::SetMoveFlag(const bool bVal, const fwMvFlagId flagId)
{
	CTaskMotionAiming* pMotionTask = FindMotionAimingTask();
	if (pMotionTask)
	{
		pMotionTask->SetMoveFlag(bVal, flagId);
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskAimGunOnFoot::SetMoveClip(const crClip* pClip, const fwMvClipId clipId)
{
	CTaskMotionAiming* pMotionTask = FindMotionAimingTask();
	if (pMotionTask)
	{
		pMotionTask->SetMoveClip(pClip, clipId);
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskAimGunOnFoot::SetMoveFloat(const float fVal, const fwMvFloatId floatId)
{
	CTaskMotionAiming* pMotionTask = FindMotionAimingTask();
	if (pMotionTask)
	{
		pMotionTask->SetMoveFloat(fVal, floatId);
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskAimGunOnFoot::SetMoveBoolean(const bool bVal, const fwMvBooleanId boolId)
{
	CTaskMotionAiming* pMotionTask = FindMotionAimingTask();
	if (pMotionTask)
	{
		pMotionTask->SetMoveBoolean(bVal, boolId);
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskAimGunOnFoot::GetMoveBoolean(const fwMvBooleanId boolId) const
{
	const CTaskMotionAiming* pMotionTask = FindMotionAimingTask();
	if (pMotionTask)
	{
		return pMotionTask->GetMoveBoolean(boolId);
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskAimGunOnFoot::TriggerFire()
{
	m_bFire = true;
	m_bForceFire = false;
	// Calculate the rate at which the weapon fire anims should play
	CMoveNetworkHelper* pHelper = FindAimingNetworkHelper(); 
	if( pHelper && pHelper->IsNetworkAttached() )
	{
		m_fFireAnimRate = GetFiringAnimPlaybackRate();
		pHelper->SetFloat( ms_FireRateId, m_fFireAnimRate );
	}

	m_bFireWhenReady = false;
	m_fTimeTryingToFire = 0.f;

#if FPS_MODE_SUPPORTED
	if(GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		m_bWantsToFireWhenWeaponReady = false;
	}
#endif
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskAimGunOnFoot::ShouldReloadWeapon() const
{
	// Only check for a reload if we're not already reloading
	if (!m_bIsReloading)
	{
		const CPed* pPed = GetPed();

		// if we're a clone we just follow the owners' lead when it comes to reloading....
		if(pPed->IsNetworkClone())
		{
			return false;
		}

		const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();

		// If the peds weapon exists and it can reload, check if we should reload
		if(pWeapon && pWeapon->GetCanReload())
		{
			// Clip is empty
			if (pWeapon->GetNeedsToReload(true))
			{
				bool bCanReload = true;
				bool bWantsToRoll = false;

				if (NetworkInterface::IsGameInProgress() || !pPed->IsPlayer())
				{
					if(GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_GUN)
					{
						const CTaskGun* pTaskGun = static_cast<const CTaskGun *>(GetParent());
						if (pTaskGun)
						{
							bCanReload = pTaskGun->CanReload();
							bWantsToRoll = pTaskGun->GetWantsToRoll();
						}
					} 
				}

#if __DEV
				// Logging to catch url:bugstar:2289101
				taskDebugf1("CTaskAimGunOnFoot::ShouldReloadWeapon: bCanReload: %d && !bWantsToRoll: %d", bCanReload, !bWantsToRoll);
#endif
				return (bCanReload && !bWantsToRoll);
			}

			// Wants to reload
			WeaponControllerState controllerState = GetWeaponControllerState(pPed);
			if (controllerState == WCS_Reload)
			{
#if __DEV
				// Logging to catch url:bugstar:2289101
				taskDebugf1("CTaskAimGunOnFoot::ShouldReloadWeapon: controllerState == WCS_Reload");
#endif
				return true;
			}
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskAimGunOnFoot::IsOutOfAmmo() const
{
	const CPed* pPed = GetPed();
	if(pPed)
	{
		const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
		if(pWeapon)
		{
			if(pWeapon->GetWeaponInfo()->GetUsesAmmo() && pWeapon->GetAmmoTotal() == 0 && !GetGunFlags().IsFlagSet(GF_AllowAimingWithNoAmmo))
			{
				//@@: location CTASKAIMGUNONFOOT_ISOUTOFAMMO_TRUE
				return true;
			}
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskAimGunOnFoot::ShouldFireWeapon()
{
	// Hack until we can get the blend on the aiming network transition
	CPed* pPed = GetPed();

	// Indefinitely script flag to block firing
	// NOTE: This is needed in addition to the native DISABLE_PLAYER_FIRING because
	// a script WAIT or frame stepping can allow the weapon to fire
	if( pPed->GetPedResetFlag( CPED_RESET_FLAG_BlockWeaponFire ) )
	{
		return false;
	}

#if FPS_MODE_SUPPORTED
	const CWeaponInfo* pWeaponInfo = pPed->GetEquippedWeaponInfo();
	// Don't fire if our previous state is still FPS_IDLE. Fixes issue where gun sometimes fires before playing transition.
	if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && pPed->GetMotionData() && 
		(pPed->GetMotionData()->GetIsFPSIdle() || pPed->GetMotionData()->GetWasFPSIdle() || 
		 (pWeaponInfo && pWeaponInfo->GetEnableFPSRNGOnly() && !pPed->GetMotionData()->GetIsFPSRNG())))
	{
		return false;
	}
#endif

	//We can't fire until the trigger has being released
	if(GetGunFireFlags().IsFlagSet(GFF_RequireFireReleaseBeforeFiring) )
	{
		return false;
	}

	// We can't fire if we're throwing a projectile from aiming
	if (pPed->GetPedResetFlag(CPED_RESET_FLAG_IsThrowingProjectileWhileAiming))
	{
		return false;
	}

	// Kick out early if our target is a player and we should back off
	if(!pPed->IsAPlayerPed())
	{
		const CEntity* pTarget = m_target.GetEntity();
		if(pTarget)
		{
			const CPed* pPedTarget = m_target.GetEntity()->GetIsTypePed() ? static_cast<const CPed*>(m_target.GetEntity()) : NULL;
			if(pPedTarget && pPedTarget->IsAPlayerPed())
			{
				CWanted* pPlayerWanted = pPedTarget->GetPlayerWanted();
				if(pPlayerWanted && (pPlayerWanted->m_EverybodyBackOff || (pPed->IsLawEnforcementPed() && pPlayerWanted->PoliceBackOff())))
				{
					return false;
				}
			}
		}

		// If we've restricted the distance we are allowed to shoot at then make sure we are within that distance
		float fMaxShootDist = pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatMaxShootingDistance);
		if(fMaxShootDist > 0.0f)
		{
			Vector3 vTargetPosition;
			m_target.GetPosition(vTargetPosition);

			float fMaxShootDistSq = (fMaxShootDist * fMaxShootDist);
			if(DistSquared(pPed->GetTransform().GetPosition(), VECTOR3_TO_VEC3V(vTargetPosition)).Getf() > fMaxShootDistSq)
			{
				return false;
			}
		}

		// If we require a LOS to the target in order to shoot, make sure we have that LOS
		if(!m_bLosClear && !pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanShootWithoutLOS))
		{
			return false;
		}
	}
	else if(!pPed->IsNetworkClone())
	{
		CControl* pControl = pPed->GetControlFromPlayer();

		bool bPressingWeaponWheelInputOrWheelActive = CNewHud::IsWeaponWheelActive() || (pControl && (pControl->GetSelectWeapon().IsPressed() || pControl->GetSelectWeapon().IsDown()));

		if(bPressingWeaponWheelInputOrWheelActive)
		{
			return false;
		}
	}

#if USE_TARGET_SEQUENCES
	CTargetSequenceHelper* pTargetSequence = pPed->FindActiveTargetSequence();
	if (pTargetSequence && !pTargetSequence->IsReadyToFire())
	{
		return false;
	}
#endif // USE_TARGET_SEQUENCES
	
	// Can't fire until min time has expired.
	if(ShouldBlockFireDueToMinTime())
	{
		return false;
	}

	const CTaskMotionAiming* pMotionAimTask = FindMotionAimingTask();
	if (!pMotionAimTask)
	{
		return false;
	}
	
	//Ensure the aim task is not turning.
	if(pMotionAimTask->IsTurning())
	{
		return false;
	}

	const CTaskCombat* pCombatTask = static_cast<const CTaskCombat*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COMBAT));
	if(pCombatTask)
	{
		if(pCombatTask->GetIsPlayingAmbientClip())
		{
			return false;
		}
		
		if(pCombatTask->IsHoldingFire())
		{
			return false;
		}
	}

	if(pPed->GetPedIntelligence()->FindTaskSecondaryByType(CTaskTypes::TASK_RELOAD_GUN))
	{
		return false;
	}

	// Block firing if local ped is within an air defence zone.
	if (pPed->GetPedResetFlag(CPED_RESET_FLAG_InAirDefenceSphere))
	{
		return false;
	}

	if (GetPed()->IsNetworkClone() && m_bForceFire)
	{
		return true;
	}

	const CObject* pWeaponObject = pPed->GetWeaponManager()->GetEquippedWeaponObject();
	const CWeapon* pWeapon = pWeaponObject ? pWeaponObject->GetWeapon() : NULL;
	
	if (pWeapon)
	{
		const CWeaponInfo* pWeaponInfo = pWeapon->GetWeaponInfo();
		if (pWeaponInfo && !pWeaponInfo->GetIsMeleeFist())
		{
			const bool isAnAimCameraActive = camInterface::GetGameplayDirector().IsAiming(pPed)
						FPS_MODE_SUPPORTED_ONLY(|| camInterface::GetGameplayDirector().GetFirstPersonShooterCamera());

			bool isFineAiming = false;
			if( pPed->IsLocalPlayer() )
			{
				//Check the audio is loaded
				if(!NetworkInterface::IsGameInProgress() && audWeaponAudioEntity::IsWaitingOnEquippedWeaponLoad())
				{
					return false;
				}
				
				CPlayerPedTargeting &targeting	= const_cast<CPlayerPedTargeting &>(pPed->GetPlayerInfo()->GetTargeting());
				isFineAiming = targeting.GetIsFineAiming();
			}

			if ((!pPed->IsLocalPlayer() || (isAnAimCameraActive && !pWeapon->GetHasFirstPersonScope()))
				&& m_iFlags.IsFlagSet(GF_WaitForFacingAngleToFire) && !pPed->GetIsAttached() && !isFineAiming)	// Hack for B*112307 since the attachment doesn't allow the anims to rotate the ped, remove once scripted gun task is done
			{
				bool bIkReachedTarget = (pPed->GetIkManager().GetTorsoSolverStatus() & CIkManager::IK_SOLVER_REACHED_YAW) == CIkManager::IK_SOLVER_REACHED_YAW && 
										(pPed->GetIkManager().GetTorsoSolverStatus() & CIkManager::IK_SOLVER_REACHED_PITCH) == CIkManager::IK_SOLVER_REACHED_PITCH;
				if(!bIkReachedTarget)
				{
					float fHeadingDiff = SubtractAngleShorter(pPed->GetFacingDirectionHeading(), pPed->GetDesiredHeading());
					if(Abs(fHeadingDiff) > EIGHTH_PI)
					{
						return false;
					}

					float fPitchDiff = SubtractAngleShorter(pMotionAimTask->GetCurrentPitch(), pMotionAimTask->GetDesiredPitch());
					if(Abs(fPitchDiff) > 0.2f)
					{
						return false;
					}
				}
			}

			// Check the peds firing pattern to see if we can fire
			if (m_bUsingFiringVariation || (pPed->GetPedIntelligence()->GetFiringPattern().WantsToFire() && pPed->GetPedIntelligence()->GetFiringPattern().ReadyToFire()))
			{
				if(pWeaponInfo->GetFireType() != FIRE_TYPE_NONE)
				{
					const bool bIgnoreOneShotPerTriggerPress = pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON);
					if ((!pWeaponInfo->GetOnlyFireOneShotPerTriggerPress() || bIgnoreOneShotPerTriggerPress) || !m_bFiredOnceThisTriggerPress)
					{
						// We don't want presses/holds during STATE_WAITING_TO_FIRE to count for 'one shot press' weapons, so invalidate the press.
						if (pWeaponInfo->GetOnlyFireOneShotPerTriggerPress())
						{
							m_bSuppressNextFireEvent = false;

							if (pWeapon->GetState() == CWeapon::STATE_WAITING_TO_FIRE )
							{
								m_bFiredOnceThisTriggerPress = true;
								return false;
							}
						}
						
						if(GetState() == State_Firing)
						{
							// Always return true if in firing state regardless of input, as we can miss fire events this way
							return true;
						}
						// Check if we're firing
						else if (GetWeaponControllerState(pPed) == WCS_Fire)
						{
							return true;
						}
						// Check if we're holding fire with an automatic weapon, of force firing at least once
						else if ((GetWeaponControllerState(pPed) == WCS_FireHeld && pWeaponInfo->GetIsAutomatic()) || m_iFlags.IsFlagSet(GF_FireAtLeastOnce))
						{
							return true;
						}
					}
				}
			}
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskAimGunOnFoot::GetIsReadyToFire() const
{
	//Ensure the weapon has exceeded the minimum time to fire.
	if(ShouldBlockFireDueToMinTime())
	{
		return false;
	}
	if(GetState() == State_CombatRoll FPS_MODE_SUPPORTED_ONLY(&& !GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_ExitingFPSCombatRoll)))
	{
		return false;
	}

#if FPS_MODE_SUPPORTED
	// Different timer for FPS combat roll for sniper. Fixed the issue where it took too long before switching to scope camera.
	const CPed* pPed = GetPed();
	const CWeapon* pWeapon = GetPed()->GetWeaponManager()->GetEquippedWeapon();
	if(pWeapon && pWeapon->GetHasFirstPersonScope() && pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		TUNE_GROUP_FLOAT(FPS_SNIPER_SCOPE_CAMERA_SWITCH, fMinFireTimeAfterRoll, 0.4f, 0.0f, 3.0f, 0.1f);

		if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ExitingFPSCombatRoll) && pPed->GetPlayerInfo()->GetExitCombatRollToScopeTimerInFPS() < fMinFireTimeAfterRoll)
		{
			return false;
		}
	}
	else
#endif // FPS_MODE_SUPPORTED
	{
		static dev_u32 MIN_FIRE_TIME_AFTER_ROLL = 250;

		if(GetPed()->GetPedIntelligence()->GetLastCombatRollTime() + MIN_FIRE_TIME_AFTER_ROLL > fwTimer::GetTimeInMilliseconds())
		{
			return false;
		}
	}

#if FPS_MODE_SUPPORTED
	if(GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		// If in the transition to the first person sniper scope aiming camera.
		if(pWeapon && pWeapon->GetHasFirstPersonScope())
		{
			TUNE_GROUP_INT(FPS_SNIPER_SCOPE_CAMERA_SWITCH, fFpsSniperScopeWaitTime, 125, 0, 2000, 100);

			if(!GetPed()->GetMotionData()->GetIsFPSScope() || (m_iFPSSwitchToScopeStartTime + fFpsSniperScopeWaitTime > fwTimer::GetTimeInMilliseconds()))
			{
				return false;;
			}
		}
	}
#endif // FPS_MODE_SUPPORTED

	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskAimGunOnFoot::GetIsFiring() const
{
	return GetState() == State_Firing;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskAimGunOnFoot::SetupToFire(const bool bForceFiringState)
{
	if(!m_bDoneSetupToFire)
	{
		SetMoveFlag(true, ms_FiringFlagId);

		// Always fire at least one bullet
		m_iFlags.SetFlag(GF_FireAtLeastOnce);
		m_bFiredOnce = false;
		m_bFiredTwice = false;

		const crClip* pClip = ChooseFiringMedClip();
		if(pClip)
		{
			SetMoveClip(pClip, ms_FireMedLoopClipId);
		}

		//Check if we should use a firing variation.
		if(!m_bUsingFiringVariation && ShouldUseFiringVariation())
		{
			//Choose a firing variation clip.
			const crClip* pClip = ChooseFiringVariationClip();
			if(pClip)
			{
				//Set the flag.
				SetMoveFlag(true, ms_HasCustomFireLoopFlagId);

				//Set the clip.
				SetMoveClip(pClip, ms_CustomFireLoopClipId);

				//Note that we are using a firing variation.
				taskDebugf1("CTaskAimGunOnFoot::Firing_OnExit: m_bUsingFiringVariation = TRUE, Ped 0x%p [%s], m_bUsingFiringVariation %d", GetPed(), GetPed() ? GetPed()->GetModelName() : "null", m_bUsingFiringVariation);
				m_bUsingFiringVariation = true;
			}
		}
		else if(!m_bSuppressNextFireEvent && GetPed()->IsLocalPlayer())
		{
			if(ShouldFireWeapon())
			{
				TriggerFire();
				m_bSuppressNextFireEvent = true;
			}
		}

		CTaskMotionAiming* pMotionTask = FindMotionAimingTask();
		CWeapon* pWeapon = GetPed()->GetWeaponManager()->GetEquippedWeapon();

		if(pMotionTask && pWeapon)
		{
			static const fwMvClipId s_WeaponFireClipId("w_fire_looped",0x827953E6);

			// Perform an clip on the weapon if one is specified
			pWeapon->StartAnim(pMotionTask->GetAimClipSet(), s_WeaponFireClipId, NORMAL_BLEND_DURATION, 1.0f, 0.0f, true);
		}

		SetClothPackageIndex(1);

		// Don't do this again
		m_bDoneSetupToFire = true;
	}

	m_bForceFiringState = bForceFiringState;
}

//////////////////////////////////////////////////////////////////////////

void CTaskAimGunOnFoot::UpdateLosProbe()
{
	// First make sure our target is valid
	if(m_target.GetIsValid())
	{
		// We want to get our local ped plus the target position and entity (if there is one)
		CPed* pPed = GetPed();
		Vector3 vTargetPosition;
		m_target.GetPosition(vTargetPosition);
		const CEntity* pTarget = m_target.GetEntity();

		// If we have a target entity and it is a ped then we handle it differently
		if(pTarget && pTarget->GetIsTypePed())
		{
			// Get the target ped and to an async test to see if we can see the ped
			const CPed* pTargetPed = static_cast<const CPed*>(pTarget);
			const ECanTargetResult losRet = CPedGeometryAnalyser::CanPedTargetPedAsync(*pPed, *(const_cast<CPed*>(pTargetPed)));
			if(losRet == ECanTarget)
			{
				m_bLosClear = true;
			}
			else if(losRet == ECanNotTarget)
			{
				m_bLosClear = false;
			}
		}
		else
		{
			// Try to obtain probe results if it's active
			if (m_asyncProbeHelper.IsProbeActive())
			{
				// This function won't return true if it's still waiting on results
				ProbeStatus probeStatus;
				if (m_asyncProbeHelper.GetProbeResults(probeStatus))
				{
					m_bLosClear = (probeStatus == PS_Clear);
				}
			}

			// If probe isn't active and the timer is up, issue a probe
			if (m_probeTimer.Tick(GetTimeStep()))
			{
				if (!m_asyncProbeHelper.IsProbeActive())
				{
					// Restart our timer
					m_probeTimer.Reset();

					// Get the head position as the start
					Vector3 vStart(pPed->GetBonePositionCached(BONETAG_HEAD));
					const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

					// Take the X and Y components from the ped's position
					vStart.x = vPedPosition.x;
					vStart.y = vPedPosition.y;

					// If we have a valid target entity then use the lock on position as our target position
					if(pTarget)
					{
						pTarget->GetLockOnTargetAimAtPos(vTargetPosition);
					}

					// if we have a cover point we want to adjust the position for our cover
					if(pPed->GetCoverPoint())
					{
						CPedGeometryAnalyser::AdjustPositionForCover(*pPed, vTargetPosition, vStart);
					}
					
					// Lastly, build the data and start the line of sight test
					WorldProbe::CShapeTestProbeDesc probeData;
					probeData.SetStartAndEnd(vStart,vTargetPosition);
					probeData.SetContext(WorldProbe::ELosCombatAI);
					probeData.SetExcludeEntity(m_target.GetEntity());
					probeData.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_OBJECT_TYPE);
					m_asyncProbeHelper.StartTestLineOfSight(probeData);
				}
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

bool  CTaskAimGunOnFoot::CheckLoopEvent()
{
	return (m_bMoveFireLoop1 || m_bMoveFireLoop2 || m_bMoveCustomFireLoopFinished);
}

////////////////////////////////////////////////////////////////////////////////

CTaskMotionAiming*  CTaskAimGunOnFoot::FindMotionAimingTask()
{
	return static_cast<CTaskMotionAiming*>(GetPed()->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_AIMING));
}

////////////////////////////////////////////////////////////////////////////////

const CTaskMotionAiming*  CTaskAimGunOnFoot::FindMotionAimingTask() const
{
	return static_cast<const CTaskMotionAiming*>(GetPed()->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_AIMING));
}

////////////////////////////////////////////////////////////////////////////////

CMoveNetworkHelper* CTaskAimGunOnFoot::FindAimingNetworkHelper() 
{ 
	CTaskMotionAiming* pMotionTask = FindMotionAimingTask();
	return pMotionTask ? &pMotionTask->GetAimingMoveNetworkHelper() : NULL; 
}

////////////////////////////////////////////////////////////////////////////////

const crClip* CTaskAimGunOnFoot::ChooseFiringVariationClip() const
{
	//Ensure the clip set is valid.
	fwMvClipSetId clipSetId = m_FiringVariationsClipSetRequestHelper.GetClipSetId();
	if(!taskVerifyf(clipSetId != CLIP_SET_ID_INVALID, "Clip set is invalid."))
	{
		return NULL;
	}
	
	//Ensure the clip set is valid.
	fwClipSet* pClipSet = fwClipSetManager::GetClipSet(clipSetId);
	if(!taskVerifyf(pClipSet, "Clip set with hash: %d and string: %s is invalid.", clipSetId.GetHash(), clipSetId.GetCStr()))
	{
		return NULL;
	}
	
	//Ensure the count is valid.
	int iCount = pClipSet->GetClipItemCount();
	if(!taskVerifyf(iCount > 0, "No clip items in the clip set."))
	{
		return NULL;
	}
	
	//Ensure the weapon is valid.
	const CWeapon* pWeapon = GetPed()->GetWeaponManager()->GetEquippedWeapon();
	if(!taskVerifyf(pWeapon, "The weapon is invalid."))
	{
		return NULL;
	}
	
	//Count the ammo we have in our clip.
	s32 iAmmoInClip = pWeapon->GetAmmoInClip();
	if(iAmmoInClip <= 0)
	{
		return NULL;
	}
	
	//We are now ensured there is at least one bullet in the clip.
	u32 uAmmoInClip = (u32)iAmmoInClip;
	
	//Choose a random clip.
	SemiShuffledSequence shuffler(iCount);
	for(int i = 0; i < iCount; ++i)
	{
		//Grab the index.
		int iIndex = shuffler.GetElement(i);
		
		//Grab the clip id.
		fwMvClipId clipId = pClipSet->GetClipItemIdByIndex(iIndex);
		
		//Ensure the clip is valid.
		const crClip* pClip = fwAnimManager::GetClipIfExistsBySetId(clipSetId, clipId);
		if(!pClip)
		{
			continue;
		}
		
		//Count the firing events in the clip.
		u32 uFiringEventsInClip = CTaskMotionAiming::CountFiringEventsInClip(*pClip);
		
		//Ensure we have enough ammo in our clip to play the variation.
		if(uAmmoInClip < uFiringEventsInClip)
		{
			continue;
		}
		
		return pClip;
	}
	
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

const crClip* CTaskAimGunOnFoot::ChooseFiringMedClip() const
{
	const CTaskMotionAiming* pMotionTask = FindMotionAimingTask();
	Assertf(pMotionTask, "ChooseFiringMedClip() No Med Motion Aiming Task!");
	if (pMotionTask)
	{
		const int nClips = 4;
		static const fwMvClipId lMedClipId[nClips] = {
			fwMvClipId("FIRE_MED",0xB71CA63A),
			fwMvClipId("FIRE_MED_VAR1",0xC5DA55A3),	// These variations only exist for injured set atm...
			fwMvClipId("FIRE_MED_VAR2",0xB8213A31),
			fwMvClipId("FIRE_MED_VAR3",0xEF5628AA),
		};

		if (GetPed()->HasHurtStarted())
		{
			//int iClip = fwRandom::GetRandomNumberInRange(0, nClips - 1);	// Should maybe consider a more consistent approach? Use hurttimer instead?
			int iClip = GetPed()->GetHurtEndTime() % nClips;
			const crClip* pClip = fwAnimManager::GetClipIfExistsBySetId(pMotionTask->GetAimClipSet(), lMedClipId[iClip]);
			if(pClip)
			{
				return pClip;
			}
		}

		const crClip* pClip = fwAnimManager::GetClipIfExistsBySetId(pMotionTask->GetAimClipSet(), lMedClipId[0]);
		return pClip;
	}

	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CTaskAimGunOnFoot::GetFiringVariationsClipSet() const
{
	//Grab the ped.
	const CPed* pPed = GetPed();

	//Ensure the ped is not a player.
	if(pPed->IsPlayer())
	{
		return CLIP_SET_ID_INVALID;
	}

	//Ensure the weapon manager is valid.
	const CPedWeaponManager* pWeaponManager = pPed->GetWeaponManager();
	if(!pWeaponManager)
	{
		return CLIP_SET_ID_INVALID;
	}

	//Ensure the equipped weapon info is valid.
	const CWeaponInfo* pWeaponInfo = pWeaponManager->GetEquippedWeaponInfo();
	if(!pWeaponInfo)
	{
		return CLIP_SET_ID_INVALID;
	}

	return m_bCrouching ? pWeaponInfo->GetFiringVariationsCrouchingClipSetId(*pPed) : pWeaponInfo->GetFiringVariationsStandingClipSetId(*pPed);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskAimGunOnFoot::ProcessStreaming()
{
	//Check if the firing variations clip set is valid.
	fwMvClipSetId firingVariationsClipSetId = GetFiringVariationsClipSet();
	if(firingVariationsClipSetId != CLIP_SET_ID_INVALID)
	{
		//Request the firing variations clip set.
		m_FiringVariationsClipSetRequestHelper.RequestClipSet(firingVariationsClipSetId);
	}
	//Check if the firing variations clip set request is active.
	else if(m_FiringVariationsClipSetRequestHelper.IsActive())
	{
		//Release the firing variations clip set.
		m_FiringVariationsClipSetRequestHelper.ReleaseClipSet();
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskAimGunOnFoot::ShouldBlockFireDueToMinTime() const
{
	//Grab the ped.
	const CPed* pPed = GetPed();
	if(!pPed)
	{
		return false;
	}

	//Make sure the ped is actively running the in cover task
	bool bIsConsideredInCover = pPed->GetIsInCover();
	if (bIsConsideredInCover)
	{
		bIsConsideredInCover = pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_IN_COVER) ? true : false;
	}

	if(!pPed->GetPedResetFlag(CPED_RESET_FLAG_NoTimeDelayBeforeShot) && !bIsConsideredInCover)
	{
		CTaskMotionBase* pMotionTask = pPed->GetPrimaryMotionTask();
		if (pMotionTask && !pMotionTask->IsFullyAiming())
		{
			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskAimGunOnFoot::ShouldUseFiringVariation() const
{
	//Ensure the firing variations clip set request helper is active.
	if(!m_FiringVariationsClipSetRequestHelper.IsActive())
	{
		return false;
	}

	//Ensure the clip set should not be streamed out.
	if(m_FiringVariationsClipSetRequestHelper.ShouldClipSetBeStreamedOut())
	{
		return false;
	}

	//Ensure the clip set should be streamed in.
	if(!m_FiringVariationsClipSetRequestHelper.ShouldClipSetBeStreamedIn())
	{
		return false;
	}
	
	//Ensure the clip set is streamed in.
	if(!m_FiringVariationsClipSetRequestHelper.IsClipSetStreamedIn())
	{
		return false;
	}
	
	//Ensure enough time has elapsed since our last firing variation.
	u32 uTime = fwTimer::GetTimeInMilliseconds();
	if(uTime < (GetPed()->GetPedIntelligence()->GetLastFiringVariationTime() + (sm_Tunables.m_MinTimeBetweenFiringVariations * 1000.0f)))
	{
		return false;
	}
	
	//Ensure the pitch is within the threshold.
	float fCurrentPitch = GetCurrentPitch();
	if(Abs(fCurrentPitch - sm_Tunables.m_IdealPitchForFiringVariation) > sm_Tunables.m_MaxPitchDifferenceForFiringVariation)
	{
		return false;
	}

	if (GetPed()->HasHurtStarted())
	{
		return false;
	}
	
	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskAimGunOnFoot::CanSetDesiredHeading(const CPed* pPed)
{
	const CTaskMotionBase* pMotionTask = pPed->GetPrimaryMotionTask();
	if(pMotionTask && pMotionTask->GetTaskType() == CTaskTypes::TASK_MOTION_PED)
	{
		const CTaskMotionPed* pMotionPedTask = static_cast<const CTaskMotionPed*>(pMotionTask);
		return pMotionPedTask->IsAimingOrNotBlocked();
	}
	return true;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskAimGunOnFoot::UpdateAdditiveWeights(const float fDesiredBreathingAdditiveWeight, const float fDesiredLeanAdditiveWeight)
{
	static dev_float APPROACH_RATE = 4.f;

	if(m_fBreathingAdditiveWeight != FLT_MAX)
		Approach(m_fBreathingAdditiveWeight, fDesiredBreathingAdditiveWeight, APPROACH_RATE, GetTimeStep());
	else
		m_fBreathingAdditiveWeight = fDesiredBreathingAdditiveWeight;

	if(m_fLeanAdditiveWeight != FLT_MAX)
		Approach(m_fLeanAdditiveWeight, fDesiredLeanAdditiveWeight, APPROACH_RATE, GetTimeStep());
	else
		m_fLeanAdditiveWeight = fDesiredLeanAdditiveWeight;
}

bool CTaskAimGunOnFoot::IsAimingScopedWeapon() const
{
	const CPed* pPed = GetPed();

	if(pPed->GetPlayerInfo()->IsAiming())
	{
		if(pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetEquippedWeaponHasFirstPersonScope())
		{
			return true;
		}
	}

	return false;
}

void CTaskAimGunOnFoot::ProcessSmashGlashFromBlocked(CPed* pPed)
{
	//We're just finished coming out of blocked and allowed to fire
	//Smash any glass intersecting with the weapon if required.
	if(GetGunFireFlags().IsFlagSet(GFF_SmashGlassFromBlockingState))
	{
		if(	pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetEquippedWeaponObject() && pPed->GetWeaponManager()->GetEquippedWeaponObject()->GetCurrentPhysicsInst())
		{
			CPhysics::CollideInstAgainstGlass(pPed->GetWeaponManager()->GetEquippedWeaponObject()->GetCurrentPhysicsInst());
		}
	
		//Handle vehicle glass
		CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
		const CObject* pWeaponObject = pPed->GetWeaponManager()->GetEquippedWeaponObject();
		if (pWeapon && pWeaponObject)
		{
			Matrix34 mWeapon;
			pWeaponObject->GetMatrixCopy(mWeapon);
			Vector3 vMuzzle;
			pWeapon->GetMuzzlePosition(mWeapon, vMuzzle);

			// Constrain the camera against the world
			WorldProbe::CShapeTestProbeDesc CapsuleTest;
			WorldProbe::CShapeTestFixedResults<1> ShapeTestResults;
			Vector3 vStart = vMuzzle;			
			Vector3 vEnd = vStart;  
			vEnd -=  mWeapon.a * 0.7f;
			CapsuleTest.SetResultsStructure(&ShapeTestResults);
			CapsuleTest.SetStartAndEnd(vStart, vEnd);
			CapsuleTest.SetIncludeFlags(ArchetypeFlags::GTA_VEHICLE_TYPE);
			CapsuleTest.SetContext(WorldProbe::LOS_GeneralAI);
			CapsuleTest.SetExcludeEntity(pPed->GetWeaponManager()->GetEquippedWeaponObject()); 

			if(WorldProbe::GetShapeTestManager()->SubmitTest(CapsuleTest))
			{
				if(ShapeTestResults[0].GetHitInst())
				{
					CEntity* pHitEntity = CPhysics::GetEntityFromInst(ShapeTestResults[0].GetHitInst());
					if(pHitEntity->GetIsTypeVehicle())
					{
						// set up the collision info structure
						VfxCollInfo_s vfxCollInfo;
						vfxCollInfo.regdEnt = pHitEntity;
						vfxCollInfo.vPositionWld = RCC_VEC3V(ShapeTestResults[0].GetHitPosition());
						vfxCollInfo.vNormalWld = RCC_VEC3V(ShapeTestResults[0].GetHitNormal());
						vfxCollInfo.vDirectionWld = Normalize(vfxCollInfo.vDirectionWld);
						vfxCollInfo.materialId = PGTAMATERIALMGR->UnpackMtlId(ShapeTestResults[0].GetHitMaterialId());
						vfxCollInfo.roomId = PGTAMATERIALMGR->UnpackRoomId(ShapeTestResults[0].GetHitMaterialId());
						vfxCollInfo.componentId = ShapeTestResults[0].GetHitComponent();
						vfxCollInfo.weaponGroup = pWeapon->GetWeaponInfo()->GetEffectGroup();
						vfxCollInfo.force = VEHICLEGLASSFORCE_WEAPON_IMPACT;
						vfxCollInfo.isBloody = false;
						vfxCollInfo.isWater = false;
						vfxCollInfo.isExitFx = false;
						vfxCollInfo.noPtFx = false;
						vfxCollInfo.noPedDamage = false;
						vfxCollInfo.noDecal = false;
						vfxCollInfo.isSnowball = false;
						vfxCollInfo.isFMJAmmo = false;

						g_vehicleGlassMan.StoreCollision(vfxCollInfo, true);
					}
				}
			}
		}
		
		GetGunFireFlags().ClearFlag(GFF_SmashGlassFromBlockingState);
	}
}

#if ENABLE_GUN_PROJECTILE_THROW
bool CTaskAimGunOnFoot::ShouldThrowGrenadeFromGunAim(CPed *pPed, const CWeaponInfo* pWeaponInfo)
{
    if(pPed->IsNetworkClone())
    {
        return pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_AIM_GUN_ON_FOOT) &&
               pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_AIM_AND_THROW_PROJECTILE);
    }
    else
    {
		if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ForcedAimFromArrest))
		{
			return false;
		}

	    CControl* pControl = pPed->GetControlFromPlayer();

	    // Don't allow us to throw a grenade if firing has been disabled/blocked
	    bool bFiringDisabled = pControl ? pControl->IsInputDisabled(INPUT_ATTACK) || pControl->IsInputDisabled(INPUT_ATTACK2) : false;

		bool bInValidStateToThrow = (GetState() == State_Aiming || GetState() == State_Firing);

		if (pControl && !bFiringDisabled && !pPed->GetPedResetFlag(CPED_RESET_FLAG_DisableProjectileThrowsWhileAimingGun) && bInValidStateToThrow)
	    {
		    // Throw grenade if left on the D-Pad is pressed
		    bool bFireGrenade = pControl->GetPedThrowGrenade().IsPressed();

			// Cache whether we have thrown the projectile from the D-Pad input (so we know whether to restrict sticky bomb detonation if still holding it down)
			m_bProjectileThrownFromDPadInput = bFireGrenade;

#if RSG_ORBIS
		    // ...or an UP swipe on the PS4 touchpad
		    bFireGrenade = bFireGrenade || CControlMgr::GetPlayerPad()->GetTouchPadGesture().IsSwipe(CTouchPadGesture::UP);
#endif
		    if (bFireGrenade)
		    {
				// Can't quick throw a grenade when unarmed, using a fist weapon, or aiming with a non-fireable weapon
				if(pWeaponInfo->GetIsUnarmed() || pWeaponInfo->GetIsMeleeFist() || pWeaponInfo->GetCanBeAimedLikeGunWithoutFiring())
				{
					return false;
				}

				atHashWithStringNotFinal selectedThrownWeaponHashInWeaponWheel = CNewHud::GetWeaponHashFromMemory(WEAPON_WHEEL_SLOT_THROWABLE_SPECIAL);
				const CWeaponInfo* pSelectedThrownWeaponInfoInWeaponWheel = CWeaponInfoManager::GetInfo<CWeaponInfo>(selectedThrownWeaponHashInWeaponWheel);				
				if(pSelectedThrownWeaponInfoInWeaponWheel && pSelectedThrownWeaponInfoInWeaponWheel->GetGroup() == WEAPONGROUP_THROWN && (pPed->GetInventory()->GetWeaponAmmo(pSelectedThrownWeaponInfoInWeaponWheel) > 0 || pPed->GetInventory()->GetIsWeaponUsingInfiniteAmmo(pSelectedThrownWeaponInfoInWeaponWheel)))
				{
					m_uQuickThrowWeaponHash = selectedThrownWeaponHashInWeaponWheel;
				}
				else
				{
					const CWeaponInfo* pBestThrownWeaponInfo = pPed->GetWeaponManager()->GetBestWeaponInfoByGroup(WEAPONGROUP_THROWN);
					if(pBestThrownWeaponInfo)
					{
						m_uQuickThrowWeaponHash = pBestThrownWeaponInfo->GetHash();
					}
					else
					{
						// if can't find any thrown weapon
						return false;
					}
				}

			    //bool bHasGrenade = pPed->GetInventory()->GetWeapon(WEAPONTYPE_GRENADE) != NULL;
			    //bool bHasGrenadeAmmo = false;
			    //if (bHasGrenade)
			    //{
				   // const CWeaponInfo *pGrenadeInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(WEAPONTYPE_GRENADE);
				   // bHasGrenadeAmmo = pGrenadeInfo ? pPed->GetInventory()->GetWeaponAmmo(pGrenadeInfo) > 0 : false;
			    //}

			    // Don't allow for RPGs, Grenade Launchers, Minigun or Sniper Rifles
			    bool bHasUsableWeapon = true;
			    if (pWeaponInfo)
			    {
				    u32 uEquippedWeaponGroup = pWeaponInfo->GetGroup();
				    if (uEquippedWeaponGroup == WEAPONGROUP_HEAVY || uEquippedWeaponGroup == WEAPONGROUP_SNIPER || uEquippedWeaponGroup == WEAPONGROUP_PETROLCAN)
				    {
					    bHasUsableWeapon = false;
				    }
			    }

			    // Has the timer expired?
			    bool bCanThrowGrenade = pPed->GetPedIntelligence()->GetCanDoAimGrenadeThrow();

			    TUNE_GROUP_BOOL(AIMING_DEBUG, bGunGrenadeThrowUseBlockingProbe, false);
			    bool bEnoughSpaceToThrow = true;
			    if (bGunGrenadeThrowUseBlockingProbe && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ThrowingGrenadeWhileAiming))
			    {
				    bEnoughSpaceToThrow = !pPed->IsNetworkClone() && !IsWeaponBlocked(pPed,false,1.0f,1.0f,false,false,true);
			    }

				bool bInValidFPSState = true;
#if FPS_MODE_SUPPORTED
				if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && pPed->GetMotionData())
				{
					bInValidFPSState = pPed->GetMotionData()->GetFPSState() != CPedMotionData::FPS_IDLE;
				}
#endif	//FPS_MODE_SUPPORTED

				bool bPressingWeaponWheelInputOrWheelActive = CNewHud::IsWeaponWheelActive() || pControl->GetSelectWeapon().IsDown();

			    return (/*bHasGrenade && bHasGrenadeAmmo &&*/ bHasUsableWeapon && bCanThrowGrenade && bEnoughSpaceToThrow && !bPressingWeaponWheelInputOrWheelActive && bInValidFPSState);
		    }
		
	    }

	    return false;

    }
}

bool CTaskAimGunOnFoot::ProcessGrenadeThrowFromGunAim(CPed *pPed, CWeapon* pWeapon)
{
	// B*1833737: Allow ped to throw a projectile while aiming. 
	// Don't start the projectile sub-task if we are already running it
	if (!FindSubTaskOfType(CTaskTypes::TASK_AIM_AND_THROW_PROJECTILE) && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ThrowingGrenadeWhileAiming) && pPed->GetWeaponManager())
	{
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_ThrowingGrenadeWhileAiming, true);
		pPed->SetPedResetFlag(CPED_RESET_FLAG_IsThrowingProjectileWhileAiming, true);

        if(pPed->IsNetworkClone())
        {
            CNetObjPed *netObjPed = SafeCast(CNetObjPed, pPed->GetNetworkObject());
            netObjPed->SetTaskOverridingPropsWeapons(true);
        }

		// Back-up the current weapon
		pPed->GetWeaponManager()->BackupEquippedWeapon(true);

		int iAmmoInClip = pPed->GetWeaponManager()->GetEquippedWeapon() ? pPed->GetWeaponManager()->GetEquippedWeapon()->GetAmmoInClip() : 0;

		m_bTempWeaponHasScope = pWeapon && pWeapon->GetScopeComponent();

		// Kick off an AimAndThrowProjectile sub-task
        CTaskAimAndThrowProjectile *pThrowProjectileTask = rage_new CTaskAimAndThrowProjectile(m_target, NULL, false, true, iAmmoInClip, m_uQuickThrowWeaponHash, m_bProjectileThrownFromDPadInput);

		SetNewTask(pThrowProjectileTask);

		m_bProjectileThrownFromDPadInput = false;

       	if (pPed->IsNetworkClone())
		{
			pThrowProjectileTask->SetRunningLocally(true);
		}

		return true;
	}
	return false;
}
#endif	//ENABLE_GUN_PROJECTILE_THROW

void CTaskAimGunOnFoot::RestorePreviousWeapon(CPed *pPed, CObject *pWepObject, int iAmmoInClip)
{
	if (pPed && pWepObject && pPed->GetWeaponManager())
	{
        bool wasOverridingClonePropsWeapons = false;

        if(pPed->IsNetworkClone())
        {
            CNetObjPed *netObjPed = SafeCast(CNetObjPed, pPed->GetNetworkObject());

            wasOverridingClonePropsWeapons = netObjPed->GetTaskOverridingPropsWeapons();

            netObjPed->SetTaskOverridingPropsWeapons(true);
        }

		u32 uBackupWeaponhash = pPed->GetWeaponManager()->GetBackupWeapon();
		pPed->GetWeaponManager()->EquipWeapon(uBackupWeaponhash, -1, true);
		
		if (iAmmoInClip != -1)
		{
			CWeapon* pEquippedWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
			if(pEquippedWeapon)
			{
				pEquippedWeapon->SetAmmoInClip(iAmmoInClip);

				// Transfer flashlight activeness if we have a flashlight component on the weapon.
				if (pEquippedWeapon->GetFlashLightComponent() && pWepObject->GetWeapon() && pWepObject->GetWeapon()->GetFlashLightComponent())
				{
					pEquippedWeapon->GetFlashLightComponent()->TransferActiveness(pWepObject->GetWeapon()->GetFlashLightComponent());
				}
			}
		}

		//Detach and destroy the temp weapon object
		CPedEquippedWeapon::DestroyObject(pWepObject, pPed);

		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_ThrowingGrenadeWhileAiming, false);

		// Restart the upper body aiming network (so we play settle anims)
		CTaskMotionBase* pMotionTask = pPed->GetPrimaryMotionTask();
		if(pMotionTask && pMotionTask->GetTaskType() == CTaskTypes::TASK_MOTION_PED)
		{
			static_cast<CTaskMotionPed*>(pMotionTask)->SetRestartAimingUpperBodyStateThisFrame(true);
		}

        if(pPed->IsNetworkClone())
        {
            CNetObjPed *netObjPed = SafeCast(CNetObjPed, pPed->GetNetworkObject());
            netObjPed->SetTaskOverridingPropsWeapons(wasOverridingClonePropsWeapons);
        }
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskAimGunOnFoot::PostCameraUpdate(CPed* pPed, const bool bDisableTorsoIk)
{
	if(CanSetDesiredHeading(pPed))
	{
		if(pPed->IsLocalPlayer())
		{
			const camGameplayDirector& gameplayDirector = camInterface::GetGameplayDirector();

			if(!pPed->GetUsingRagdoll() && !pPed->GetPlayerInfo()->AreControlsDisabled())
			{
				// Attempt to update the ped's orientation and IK aim target *after* the camera system has updated, but before the aim IK is updated

				const camFrame& aimCameraFrame	= camInterface::GetPlayerControlCamAimFrame();

				const bool bIsAssistedAiming = pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON);

				// Update the ped's (desired) heading
				CPlayerPedTargeting &targeting	= const_cast<CPlayerPedTargeting &>(pPed->GetPlayerInfo()->GetTargeting());
				Vector3 vTargetPos;
				if (bIsAssistedAiming)
				{
					vTargetPos = targeting.GetLockonTargetPos();
					pPed->SetDesiredHeading(vTargetPos);
				}
				else
				{
					float aimHeading = aimCameraFrame.ComputeHeading();
#if FPS_MODE_SUPPORTED
 					if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false, false, true, true))
 					{
 						aimHeading = camInterface::GetPlayerControlCamHeading(*pPed);
 
 						const camFirstPersonShooterCamera* pFpsCam = camInterface::GetGameplayDirector().GetFirstPersonShooterCamera();
 						if(pFpsCam)
 						{
 							aimHeading += pFpsCam->GetRelativeHeading();
 						}
 					}
#endif // FPS_MODE_SUPPORTED

				#if RSG_PC
					aimHeading = fwAngle::LimitRadianAngleSafe(aimHeading);
				#else
					aimHeading = fwAngle::LimitRadianAngle(aimHeading);
				#endif
					pPed->SetDesiredHeading(aimHeading);
				}

				const CTaskMotionAiming* pAimingTask = static_cast<CTaskMotionAiming *>(pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_AIMING));
				if(!pAimingTask || pAimingTask->GetAimIntroFinished())
				{
					if(!bDisableTorsoIk && (bIsAssistedAiming || gameplayDirector.IsThirdPersonAiming(pPed) FPS_MODE_SUPPORTED_ONLY(|| pPed->IsFirstPersonShooterModeEnabledForPlayer(false, false, false, true, false))))
					{
						//Force an update of the point gun target for the ped to avoid a frame of lag.
						if (!bIsAssistedAiming)
						{
							const Matrix34& aimCameraWorldMatrix	= aimCameraFrame.GetWorldMatrix();
							vTargetPos = aimCameraWorldMatrix.d + (aimCameraWorldMatrix.b * ms_fTargetDistanceFromCameraForAimIk);
						}

						float fCurrentBlendAmount = pPed->GetIkManager().GetTorsoBlend();
						float fTorsoIKBlend = CTaskGun::GetTorsoIKBlendFromWeaponFlag(*pPed, vTargetPos, fCurrentBlendAmount);
						pPed->GetIkManager().PointGunAtPosition(vTargetPos, fTorsoIKBlend);
					}
				}
			}
		}		
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskAimGunOnFoot::GetWaitingToSwitchWeapon() const
{
	const CPed* pPed = GetPed();
	if(pPed)
	{
		return pPed->GetWeaponManager()->GetRequiresWeaponSwitch() && !m_iFlags.IsFlagSet(GF_PreventWeaponSwapping) && !pPed->GetPedResetFlag(CPED_RESET_FLAG_IsThrowingProjectileWhileAiming);
	}
	return false;
}

bool CTaskAimGunOnFoot::CanSpinUp() const
{
	bool bCanSpinUp = true;

#if FPS_MODE_SUPPORTED
	const CPed* pPed = GetPed();
	if( pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true) )
	{
		if (pPed->IsNetworkClone())
		{
			bCanSpinUp = !(static_cast<CNetObjPlayer*>(pPed->GetNetworkObject())->IsInFirstPersonIdle());
		}
		else
		{
			CPlayerInfo* pPlayerInfo = pPed->GetPlayerInfo();
			if(pPlayerInfo && !pPlayerInfo->IsAiming() && !pPlayerInfo->IsFiring())
			{
				// In first person mode, player is alway aiming, so ignore 
				// weapon unless player is really aiming or shooting.
				bCanSpinUp = false;
			}

			// Don't start spin up as the task will restart now. Fixed spin up started twice.
			if(pPed->GetMotionData()->GetWasFPSIdle() && !pPed->GetMotionData()->GetIsFPSIdle())
			{
				bCanSpinUp = false;
			}
		}
	}
#endif // FPS_MODE_SUPPORTED

	return bCanSpinUp;
}

////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
void CTaskAimGunOnFoot::Debug() const
{
#if DEBUG_DRAW

	if (GetPed() != CDebugScene::FocusEntities_Get(0))
	{
		return;
	}

	Vector3 vTargetPos;
	Vector3 vPedPos = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition());
	if (m_target.GetIsValid() && m_target.GetPosition(vTargetPos))
	{
		grcDebugDraw::Sphere(vPedPos, 0.05f, Color_blue);
		grcDebugDraw::Line(vPedPos, vTargetPos, Color_blue, Color_red);
		grcDebugDraw::Sphere(vTargetPos, 0.05f, Color_red);
	}
#endif

/*
	if(GetPed())
	{
		Vector3 vPedPos = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition());
		char buffer[1024];
		sprintf(buffer, "State = %s\nAmmoInClip = %d\nFireCounter = %d\nWeapon Controller Firing = %d\nWeapon = %d : %d",
		GetStaticStateName(GetState()),
		m_iAmmoInClip,
		m_fireCounter,
		m_bCloneWeaponControllerFiring,
		GetPed()->GetWeaponManager()->GetEquippedWeaponHash(),
		m_cloneWeaponHashCache);

		grcDebugDraw::Text(vPedPos, Color_white, buffer, false);
	}
*/
	// Base class
	CTaskAimGun::Debug();
}

////////////////////////////////////////////////////////////////////////////////

const char* CTaskAimGunOnFoot::GetStaticStateName( s32 iState )
{
	switch (iState)
	{
		case State_Start:		return "State_Start";
		case State_Aiming:		return "State_Aiming";
		case State_Firing:		return "State_Firing";
		case State_FireOutro:	return "State_FireOutro";
		case State_AimToIdle:	return "State_AimToIdle";
		case State_AimOutro:	return "State_AimOutro";
		case State_Blocked:		return "State_Blocked";
		case State_CombatRoll:	return "State_CombatRoll";
#if ENABLE_GUN_PROJECTILE_THROW
		case State_ThrowingProjectileFromAiming:	return "State_ThrowingProjectileFromAiming";
#endif	//ENABLE_GUN_PROJECTILE_THROW
		case State_Finish:		return "State_Finish";
		default: 
			taskAssertf(0, "Unhandled state %i", iState);
			break;
	}
	return "State_Invalid";
}
#endif // !__FINAL

////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------
// CClonedAimGunOnFootInfo
//----------------------------------------------------

CClonedAimGunOnFootInfo::CClonedAimGunOnFootInfo()
: m_fireCounter(0)
, m_iAmmoInClip(0)
, m_bWeaponControllerFiring(0)
, m_seed(0)
, m_bUseAlternativeAimingWhenBlocked(false)
, m_bCombatRoll(false)
, m_bRollForward(true)
, m_fRollDirection(0.f)
{
}

CClonedAimGunOnFootInfo::CClonedAimGunOnFootInfo(s32 aimGunOnFootState, u8 fireCounter, u8 ammoInClip, bool bWeaponControllerFiring, u32 seed, u8 rollToken, bool bUseAlternativeAimingWhenBlocked, bool bCombatRoll, bool bForwardRoll, float fRollDirection)
: m_fireCounter(fireCounter)
, m_iAmmoInClip(ammoInClip)
, m_bWeaponControllerFiring(bWeaponControllerFiring)
, m_seed(seed)
, m_RollToken(rollToken)
, m_bUseAlternativeAimingWhenBlocked(bUseAlternativeAimingWhenBlocked)
, m_bCombatRoll(bCombatRoll)
, m_bRollForward(bForwardRoll)
, m_fRollDirection(fRollDirection)
{
	SetStatusFromMainTaskState(aimGunOnFootState);

	m_bHasSeed = (seed != 0);
}

void CClonedAimGunOnFootInfo::Serialise(CSyncDataBase& serialiser)
{
	CSerialisedFSMTaskInfo::Serialise(serialiser);

	static const unsigned SIZEOF_AMMO_IN_CLIP = 8;
	static const unsigned SIZEOF_ROLL_TOKEN   = datBitsNeeded<CTaskAimGunOnFoot::MAX_ROLL_TOKEN>::COUNT;
	static const unsigned SIZEOF_HEADING = 8;

	SERIALISE_UNSIGNED(serialiser, m_iAmmoInClip, SIZEOF_AMMO_IN_CLIP, "Ammo In Clip");
	SERIALISE_UNSIGNED(serialiser, m_RollToken, SIZEOF_ROLL_TOKEN, "Roll Token");
	SERIALISE_UNSIGNED(serialiser, m_fireCounter, sizeof(m_fireCounter)<<3, "Fire counter");
	SERIALISE_BOOL(serialiser, m_bWeaponControllerFiring, "Weapon Controller Firing");
	SERIALISE_BOOL(serialiser, m_bHasSeed, "m_bHasSeed");
	if (m_bHasSeed || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_seed, 32, "m_seed");
	}
	else
	{
		m_seed = 0;
	}
	SERIALISE_BOOL(serialiser, m_bUseAlternativeAimingWhenBlocked, "m_bUseAlternativeAimingWhenBlocked");

	SERIALISE_BOOL(serialiser, m_bCombatRoll, "m_bCombatRoll");
	if (m_bCombatRoll || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_BOOL(serialiser, m_bRollForward, "m_bRollForward");
		SERIALISE_PACKED_FLOAT(serialiser, m_fRollDirection, TWO_PI, SIZEOF_HEADING, "m_fRollDirection");
	}
}

