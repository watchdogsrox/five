// File header
#include "Task/Weapons/Gun/TaskGun.h"

// Framework headers
#include "ai/task/taskchannel.h"
#include "math/angmath.h"

// Game headers
#include "Animation/FacialData.h"
#include "Animation/MovePed.h"
#include "Audio/ScriptAudioEntity.h"
#include "Camera/Base/BaseObject.h"
#include "Camera/Base/BaseCamera.h"
#include "Camera/CamInterface.h"
#include "camera/gameplay/aim/FirstPersonShooterCamera.h"
#include "Camera/Gameplay/GameplayDirector.h"
#include "Event/EventWeapon.h"
#include "FrontEnd/NewHud.h"
#include "Ik/IkManager.h"
#include "Network/Players/NetGamePlayer.h"
#include "Network/Objects/Entities/NetObjPlayer.h"
#include "Peds/PedIntelligence.h"
#include "Peds/Ped.h"
#include "Scene/PlayerSwitch/PlayerSwitchInterface.h"
#include "task/Combat/CombatDirector.h"
#include "Task/Combat/TaskCombatMelee.h"
#include "Task/Combat/TaskNewCombat.h"
#include "Task/Combat/TaskReact.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/General/TaskBasic.h"
#include "Task/Motion/Locomotion/TaskMotionAiming.h"
#include "Task/Motion/Locomotion/TaskMotionPed.h"
#include "Task/Movement/TaskNavMesh.h"
#include "Task/Movement/TaskGoToPoint.h"
#include "Task/movement/TaskGetUp.h"
#include "Task/Service/Swat/TaskSwat.h"
#include "Task/Weapons/Gun/GunFlags.h"
#include "Task/Weapons/Gun/TaskAimGun.h"
#include "Task/Weapons/Gun/TaskAimGunFactory.h"
#include "Task/Weapons/Gun/TaskReloadGun.h"
#include "Task/Weapons/Gun/Metadata/ScriptedGunTaskInfo.h"
#include "Task/Weapons/Gun/Metadata/ScriptedGunTaskInfoMetadataMgr.h"
#include "Task/Weapons/Gun/TaskVehicleDriveBy.h"
#include "Task/Weapons/TaskProjectile.h"
#include "Task/Weapons/TaskSwapWeapon.h"
#include "Task/Weapons/WeaponController.h"
#include "Vehicles/VehicleGadgets.h"
#include "Vehicles/Metadata/VehicleMetadataManager.h"
#include "Vehicles/Metadata/VehicleSeatInfo.h"
#include "Weapons/Weapon.h"
#include "weapons/info/weaponinfo.h"

#include "control/replay/ReplaySettings.h"

#if GTA_REPLAY
#include "control/replay/Ped/PedPacket.h"
#endif

// Macro to disable optimisations if set
AI_OPTIMISATIONS()
AI_WEAPON_OPTIMISATIONS()
NETWORK_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CTaskGun
//////////////////////////////////////////////////////////////////////////

fwMvClipSetId CTaskGun::ms_BulletReactionPistolClipSetId("combat_bullet_flinches_pistol",0xE2809DFD);
fwMvClipSetId CTaskGun::ms_BulletReactionRifleClipSetId("combat_bullet_flinches_rifle",0xE565A158);
fwMvBooleanId CTaskGun::ms_ReactionFinished("ReactionFinished",0x86579467);
fwMvRequestId CTaskGun::ms_React("React",0x13A3EFDC);
fwMvFloatId	CTaskGun::ms_DirectionId("Direction",0x34DF9828);
fwMvFlagId	CTaskGun::ms_VariationId("Variation",0xCFF74A31);
fwMvFlagId	CTaskGun::ms_ForceOverheadId("ForceOverhead",0x5450D328);

u32 CTaskGun::ms_uLastTimeOverheadUsed = 0;

IMPLEMENT_WEAPON_TASK_TUNABLES(CTaskGun, 0x5e5ff934);
CTaskGun::Tunables CTaskGun::ms_Tunables;

bool CTaskGun::CanPedTurn(const CPed& rPed)
{
	//Check if we are aiming.
	CTaskMotionAiming* pTask = static_cast<CTaskMotionAiming *>(rPed.GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_AIMING));
	if(pTask)
	{
		//Ensure we are not playing a transition.
		if(pTask->IsPlayingTransition())
		{
			return false;
		}
	}

	return true;
}

bool CTaskGun::ProcessUseAssistedAimingCamera(CPed& ped, bool bForceAssistedAimingOn, bool bTestResetFlag)
{
	if (ped.IsLocalPlayer())
	{
		bool bReturnValue = false;

		const CWeaponInfo* pWeaponInfo = ped.GetWeaponManager()->GetEquippedWeaponInfo();
		if (pWeaponInfo)
		{
			const bool bWeaponWheelActive = CNewHud::IsShowingHUDMenu();
			const bool bIsAssistedAiming = bTestResetFlag ? ped.GetPlayerResetFlag(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON) : (bForceAssistedAimingOn ? true : (!bWeaponWheelActive && ped.GetPlayerInfo()->IsAssistedAiming()));

			// Flag for custom camera behaviour when assisted aiming
			if (bIsAssistedAiming)
			{
				ped.SetPlayerResetFlag(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON);
				bReturnValue = true;
			}

			if (!bWeaponWheelActive)
			{
				// Deactivate the custom camera behaviour when we start aiming normally
				if (ped.GetPlayerInfo()->IsAiming(false))
				{
					bReturnValue = false;
				}
			}
		}

		return bReturnValue;
	}
	return false;
}

bool CTaskGun::ProcessUseRunAndGunAimingCamera(CPed& ped, bool bForceRunAndGunAimingOn, bool bTestResetFlag)
{
	if (ped.IsLocalPlayer())
	{
		bool bReturnValue = false;

		const CWeaponInfo* pWeaponInfo = ped.GetWeaponManager()->GetEquippedWeaponInfo();
		if (pWeaponInfo)
		{
			const bool bWeaponWheelActive = CNewHud::IsShowingHUDMenu();
			const bool bOnlyAllowedToFire = pWeaponInfo->GetOnlyAllowFiring();
			const bool bIsRunAndGunning = bTestResetFlag ? ped.GetPlayerResetFlag(CPlayerResetFlags::PRF_RUN_AND_GUN) : (bForceRunAndGunAimingOn ? true : (!bWeaponWheelActive && ped.GetPlayerInfo()->IsRunAndGunning()));
			const bool bIs3rdPersonRollingWithSniper = ped.GetMotionData()->GetCombatRoll() && pWeaponInfo->GetGroup() == WEAPONGROUP_SNIPER && !ped.IsFirstPersonShooterModeEnabledForPlayer(false);

			if (bIsRunAndGunning)
			{
				ped.SetPlayerResetFlag(CPlayerResetFlags::PRF_RUN_AND_GUN);
				bReturnValue = true;
			}

			if (!bOnlyAllowedToFire && !bWeaponWheelActive)
			{
				// Deactivate the custom camera behaviour when we start aiming normally
				if (ped.GetPlayerInfo()->IsAiming(false))
				{
					bReturnValue = false;
				}
			}

			if(bIs3rdPersonRollingWithSniper)
			{
				bReturnValue = true;
			}
		}

		return bReturnValue;
	}
	return false;
}

void CTaskGun::ProcessTorsoIk(CPed& ped, const fwFlags32& flags, const CWeaponTarget& target, const CClipHelper* UNUSED_PARAM(pClipHelper))
{
	weaponAssert(ped.GetWeaponManager());
	const CObject* pWeaponObject = ped.GetWeaponManager()->GetEquippedWeaponObject();
	if(!pWeaponObject && !ped.GetWeaponManager()->GetIsArmedMelee())
	{
		// No weapon to aim with
		return;
	}

	const CTaskMotionAiming* pAimingTask = static_cast<CTaskMotionAiming *>(ped.GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_AIMING));
	if(!pAimingTask || !pAimingTask->GetAimIntroFinished())
	{
		// Don't do torso ik in aim intros
		return;
	}

	if(pWeaponObject && target.GetIsValid())
	{
		// Do nothing if torso Ik is disabled
		const CWeapon* pWeapon = pWeaponObject->GetWeapon();
		if(!flags.IsFlagSet(GF_DisableTorsoIk) && (!pWeapon || !pWeapon->GetHasFirstPersonScope()))
		{
			Vector3 vTargetPos;
			if(ped.IsLocalPlayer() && !ped.GetPlayerInfo()->AreControlsDisabled())
			{
				const bool bIsAssistedAiming = ped.GetPlayerResetFlag(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON);
				CPlayerPedTargeting& targeting = const_cast<CPlayerPedTargeting &>(ped.GetPlayerInfo()->GetTargeting());
				if(bIsAssistedAiming && targeting.GetLockOnTarget())
				{
					vTargetPos = targeting.GetLockonTargetPos();
				}
				else
				{
					// Take it from the camera for the player
					const camFrame& aimCameraFrame	= camInterface::GetPlayerControlCamAimFrame();
					const Matrix34& aimCameraWorldMatrix	= aimCameraFrame.GetWorldMatrix();
					vTargetPos = aimCameraWorldMatrix.d + (aimCameraWorldMatrix.b * CTaskAimGunOnFoot::ms_fTargetDistanceFromCameraForAimIk);
				}
			}
			else
			{
				if(!target.GetPositionWithFiringOffsets(&ped, vTargetPos))
				{
					return;
				}
			}


			float fCurrentBlendAmount = ped.GetIkManager().GetTorsoBlend();
			float fBlend = GetTorsoIKBlendFromWeaponFlag(ped, vTargetPos, fCurrentBlendAmount);

			if(vTargetPos.x != 0.0f && vTargetPos.y != 0.0f && taskVerifyf(rage::FPIsFinite(vTargetPos.x) && rage::FPIsFinite(vTargetPos.y), "Target Position Was Invalid") && ShouldPointGunAtPosition(ped, vTargetPos))
			{
				// Point the gun at a specific position
				ped.GetIkManager().PointGunAtPosition(vTargetPos, fBlend);
			}
			else
			{
				// Use the yaw from the direction the ped is looking and no pitch
				ped.GetIkManager().PointGunInDirection(ped.GetCurrentHeading(), 0.0f, false, fBlend);
			}
		}
	}
}

bool CTaskGun::ShouldPointGunAtPosition(const CPed& rPed, const Vector3& vTargetPos)
{
	//Check if the ped is not able to turn.
	if(!CanPedTurn(rPed))
	{
		//Grab the ped position.
		Vec3V vPedPosition = rPed.GetTransform().GetPosition();

		//Grab the ped's (XY) forward vector.
		Vec3V vPedForward = rPed.GetTransform().GetForward();
		vPedForward.SetZ(ScalarV(V_ZERO));
		vPedForward = NormalizeFastSafe(vPedForward, Vec3V(V_ZERO));

		//Calculate the direction (XY) from the ped to the target.
		Vec3V vPedToTarget = Subtract(RCC_VEC3V(vTargetPos), vPedPosition);
		vPedToTarget.SetZ(ScalarV(V_ZERO));
		vPedToTarget = NormalizeFastSafe(vPedToTarget, Vec3V(V_ZERO));

		//Ensure the dot exceeds the threshold.
		ScalarV scMinDot = ScalarVFromF32(ms_Tunables.m_MinDotToPointGunAtPositionWhenUnableToTurn);
		ScalarV scDot = Dot(vPedForward, vPedToTarget);
		if(IsLessThanAll(scDot, scMinDot))
		{
			return false;
		}
	}

	return true;
}

float CTaskGun::GetTorsoIKBlendFromWeaponFlag(const CPed &rPed, const Vector3& vTargetPos, const float fCurrentBlendAmount)
{
	float fBlendOutAmount = -1.0f;
	// B*2233547: Flare Gun - Blend out torso IK when aiming pitch is above threshold.
	if (rPed.GetEquippedWeaponInfo() && rPed.GetEquippedWeaponInfo()->GetShouldDisableTorsoIKAboveAngleThreshold() && rPed.GetWeaponManager())
	{
		const CWeapon *pWeapon = rPed.GetWeaponManager()->GetEquippedWeapon();
		const CObject* pWeaponObject = rPed.GetWeaponManager()->GetEquippedWeaponObject();
		if (pWeapon && pWeaponObject)
		{
			Matrix34 weaponMatrix = MAT34V_TO_MATRIX34(pWeaponObject->GetTransform().GetMatrix());
			Vector3 vStart = VEC3_ZERO; 
			pWeapon->GetMuzzlePosition(weaponMatrix, vStart);

			float fMaxPitch = rPed.GetEquippedWeaponInfo()->GetTorsoIKAngleLimit();
			weaponAssertf(fMaxPitch != -1.0f, "CTaskGun::ShouldDisableTorsoIKFromWeaponFlag: TorsoIKAngleLimit uninitialized but DisableTorsoIKAboveAngleThreshold flag is set! Weapon hash: %d", rPed.GetEquippedWeaponInfo()->GetHash());
			if (fMaxPitch != -1.0f)
			{
				fMaxPitch *= DtoR;
				float fDesiredPitch = CTaskAimGun::CalculateDesiredPitch(&rPed, vStart, vTargetPos);

				const CWeaponInfo *pWeaponInfo = pWeapon->GetWeaponInfo();
				const CAimingInfo* pAimingInfo = pWeaponInfo ? pWeaponInfo->GetAimingInfo() : NULL;
				const float fSweepPitchMax = pAimingInfo->GetSweepPitchMax() * DtoR;
				const float fPitchLimitsDelta = fSweepPitchMax - fMaxPitch;
				if (pAimingInfo && fPitchLimitsDelta != 0.0f && (fDesiredPitch > fMaxPitch || fCurrentBlendAmount < 1.0f))
				{
					// Calculate the blend amount
					const float fActualPitchDelta = fSweepPitchMax - fDesiredPitch;
					fBlendOutAmount = (fActualPitchDelta / fPitchLimitsDelta);
					fBlendOutAmount = rage::Clamp(fBlendOutAmount, 0.0f, 1.0f);

					// Smooth the blend amount 
					static dev_float fLerpRate = 0.1f;
					fBlendOutAmount = rage::Lerp(fLerpRate, fCurrentBlendAmount, fBlendOutAmount);
					return fBlendOutAmount;
				}
			}
		}
	}

	return fBlendOutAmount;
}

float CTaskGun::GetTargetHeading(const CPed& ped)
{
	// Query the gun task for the target
	const CTaskGun* pGunTask = static_cast<const CTaskGun*>(ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_GUN));
	if(pGunTask)
	{
		if(pGunTask->GetTarget().GetIsValid())
		{
			Vector3 vTargetPos;
			if(pGunTask->GetTarget().GetPosition(vTargetPos))
			{
				const Vector3 vPedPos = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition());
				return fwAngle::GetRadianAngleBetweenPoints(vTargetPos.x, vTargetPos.y, vPedPos.x, vPedPos.y);
			}
		}
	}
	else
	{
		const CTaskAimAndThrowProjectile* pProjectileTask = static_cast<const CTaskAimAndThrowProjectile*>(ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_AND_THROW_PROJECTILE));
		if(pProjectileTask)
		{
			if(pProjectileTask->GetTarget().GetIsValid())
			{
				Vector3 vTargetPos;
				if(pProjectileTask->GetTarget().GetPosition(vTargetPos))
				{
					const Vector3 vPedPos = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition());
					return fwAngle::GetRadianAngleBetweenPoints(vTargetPos.x, vTargetPos.y, vPedPos.x, vPedPos.y);
				}
			}
		}

	}
	return ped.GetDesiredHeading();
}

void CTaskGun::ClearToggleAim(CPed* KEYBOARD_MOUSE_ONLY(pPed))
{
#if KEYBOARD_MOUSE_SUPPORT
	if(pPed)
	{
		if(pPed->IsLocalPlayer())
		{
			CControl* pControl = pPed->GetControlFromPlayer();
			if(pControl)
			{
				pControl->SetToggleAimOn(false);
			}
		}
	}
#endif // KEYBOARD_MOUSE_SUPPORT
}

bool CTaskGun::IsPlayerSwimmingAndNotAiming(const CPed* pPed)
{
	if (pPed->GetIsSwimming() && pPed->GetPlayerInfo() && !pPed->GetPlayerInfo()->IsAiming(false))
	{
		return true;
	}
	return false;
}

CTaskGun::CTaskGun(CWeaponController::WeaponControllerType weaponControllerType, CTaskTypes::eTaskType aimTaskType, const CWeaponTarget& target, float fDuration)
: m_weaponControllerType(weaponControllerType)
, m_iFlags(fDuration < 0.0f ? GF_InfiniteDuration : 0)
, m_iGFFlags(0)
, m_aimTaskType(aimTaskType)
, m_target(target)
, m_uFiringPatternHash(0)
, m_bHasFiringPatternOverride(false)
, m_fDuration(fDuration)
, m_fTimeActive(0.0f)
, m_fTimeTillNextLookAt(0.0f)
, m_fTimeSinceLastEyeIkWasProcessed(0.0f)
, m_uBlockFiringTime(0)
, m_overrideAimClipSetId(CLIP_SET_ID_INVALID)
, m_overrideAimClipId(CLIP_ID_INVALID)
, m_overrideFireClipSetId(CLIP_SET_ID_INVALID)
, m_overrideFireClipId(CLIP_ID_INVALID)
, m_overrideClipSetId(CLIP_SET_ID_INVALID)
, m_uScriptedGunTaskInfoHash(0)
, m_uLastSwapTime(0)
, m_bShouldUseAssistedAimingCamera(false)
, m_bShouldUseRunAndGunAimingCamera(false)
, m_bWantsToReload(false)
, m_bAllowRestartAfterAbort(false)
, m_ReloadingOnEmpty(false)
, m_bMoveReactionFinished(false)
, m_bPlayAimIntro(false)
, m_bWantsToRoll(false)
, m_vGunEventPosition(V_ZERO)
, m_fGunEventReactionDir(0.0f)
, m_TargetOutOfScopeId_Clone(NETWORK_INVALID_OBJECT_ID)
, m_uLastIsAimingTimeForOutroDelay(0)
, m_uLastIsAimingTimeForOutroDelayCached(0)
, m_uLastFirstPersonRunAndGunTimeWhileSprinting(0)
, m_iTimeToAllowSprinting(-1)
, m_bReloadIsBlocked(false)
, m_vPedPosWhenReloadIsBlocked(V_ZERO)
, m_vPedHeadingWhenReloadIsBlocked(0.0f)
, m_bSkipDrivebyIntro(false)
, m_bInstantlyBlendDrivebyAnims(false)
{
	SetInternalTaskType(CTaskTypes::TASK_GUN);
}


CTaskGun::CTaskGun(const CTaskGun& other)
: m_weaponControllerType(other.m_weaponControllerType)
, m_iFlags(other.m_iFlags)
, m_iGFFlags(other.m_iGFFlags)
, m_aimTaskType(other.m_aimTaskType)
, m_target(other.m_target)
, m_uFiringPatternHash(other.m_uFiringPatternHash)
, m_bHasFiringPatternOverride(other.m_bHasFiringPatternOverride)
, m_fDuration(other.m_fDuration)
, m_fTimeActive(other.m_fTimeActive)
, m_fTimeTillNextLookAt(other.m_fTimeTillNextLookAt)
, m_fTimeSinceLastEyeIkWasProcessed(other.m_fTimeSinceLastEyeIkWasProcessed)
, m_ikInfo(other.m_ikInfo)
, m_uBlockFiringTime(other.m_uBlockFiringTime)
, m_overrideAimClipSetId(other.m_overrideAimClipSetId)
, m_overrideAimClipId(other.m_overrideAimClipId)
, m_overrideFireClipSetId(other.m_overrideFireClipSetId)
, m_overrideFireClipId(other.m_overrideFireClipId)
, m_overrideClipSetId(other.m_overrideClipSetId)
, m_uScriptedGunTaskInfoHash(other.m_uScriptedGunTaskInfoHash)
, m_bShouldUseAssistedAimingCamera(other.m_bShouldUseAssistedAimingCamera)
, m_bShouldUseRunAndGunAimingCamera(other.m_bShouldUseRunAndGunAimingCamera)
, m_bWantsToReload(other.m_bWantsToReload)
, m_bAllowRestartAfterAbort(other.m_bAllowRestartAfterAbort)
, m_ReloadingOnEmpty(other.m_ReloadingOnEmpty)
, m_bMoveReactionFinished(other.m_bMoveReactionFinished)
, m_bPlayAimIntro(other.m_bPlayAimIntro)
, m_bWantsToRoll(other.m_bWantsToRoll)
, m_vGunEventPosition(other.m_vGunEventPosition)
, m_fGunEventReactionDir(other.m_fGunEventReactionDir)
, m_TargetOutOfScopeId_Clone(other.m_TargetOutOfScopeId_Clone)
, m_uLastFirstPersonRunAndGunTimeWhileSprinting(other.m_uLastFirstPersonRunAndGunTimeWhileSprinting)
, m_iTimeToAllowSprinting(-1)
, m_bReloadIsBlocked(false)
, m_vPedPosWhenReloadIsBlocked(V_ZERO)
, m_vPedHeadingWhenReloadIsBlocked(0.0f)
, m_bSkipDrivebyIntro(other.m_bSkipDrivebyIntro)
, m_bInstantlyBlendDrivebyAnims(other.m_bInstantlyBlendDrivebyAnims)
{
	SetInternalTaskType(CTaskTypes::TASK_GUN);
}

CTaskGun::~CTaskGun()
{
}

void CTaskGun::CleanUp()
{
	//Clear the move network.
	GetPed()->GetMovePed().ClearTaskNetwork(m_MoveNetworkHelper, NORMAL_BLEND_DURATION);
	m_MoveNetworkHelper.ReleaseNetworkPlayer();
}

void CTaskGun::GetOverriddenGameplayCameraSettings(tGameplayCameraSettings& settings) const
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
	if (IsPlayerSwimmingAndNotAiming(pPed))
	{
		return; 
	}

#if FPS_MODE_SUPPORTED
	// Don't change to sniper scope in first person unless the aiming button is pressed and we are not combat rolling
	if(pPed->IsLocalPlayer() &&  pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && camInterface::GetGameplayDirector().IsFirstPersonModeEnabled() && 
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

	const bool isAnAimCameraActive = camInterface::GetGameplayDirector().IsAiming(pPed);
	if( isPlayerSwitchActive && isShortPlayerSwitch &&
		camInterface::GetGameplayDirector().GetFollowPedCamera() && !isAnAimCameraActive &&
		camInterface::GetGameplayDirector().IsShortRangePlayerSwitchFromFirstPerson() )
	{
		// If doing a short range switch, do not change to aim camera if we are in follow ped camera.
		return;
	}

	if(GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_AIM_GUN_ON_FOOT)
	{
		const CTaskAimGunOnFoot* pAimGunTask = static_cast<const CTaskAimGunOnFoot*>(GetSubTask());
		if(pAimGunTask && pAimGunTask->GetIsWeaponBlocked())
		{
			return;
		} 
	}

	//Request the camera specified in the metadata associated with the ped's equipped weapon.
	const CWeaponInfo* weaponInfo = camGameplayDirector::ComputeWeaponInfoForPed(*pPed);

	// If we're doing a gun-grenade throw, check if our temp weapon had a scope attachment so we can use scoped camera hash
	bool bTempWeaponHasScope = false;
	if (pPed->GetPedResetFlag(CPED_RESET_FLAG_IsThrowingProjectileWhileAiming))
	{
		if(GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_AIM_GUN_ON_FOOT)
		{
			const CTaskAimGunOnFoot *pAimGunTask = static_cast<const CTaskAimGunOnFoot*>(GetSubTask());
			if (pAimGunTask)
			{
				bTempWeaponHasScope = pAimGunTask->GetTempWeaponObjectHasScope();
			}
		}
	}

	if(taskVerifyf(weaponInfo, "Could not obtain a weapon info object for the ped's equipped weapon"))
	{
		//NOTE: Do not change camera if we are not aiming and have the weapon wheel up
		if (!isAnAimCameraActive && CNewHud::IsShowingHUDMenu())
		{
			return;
		}

		const bool isMeleeWeapon = weaponInfo->GetIsMelee();
		const bool isAimedLikeGun = weaponInfo->GetCanBeAimedLikeGunWithoutFiring();

		//If we fall back into querying the gun task for a gameplay camera when parachuting, use the associated drive-by camera as a special case,
		//so long as the ped isn't using a melee weapon.
		if(!isMeleeWeapon && (pPed->GetIsParachuting() || pPed->GetPedResetFlag(CPED_RESET_FLAG_IsUsingJetpack)) )
		{
			const CVehicleDriveByInfo* pDriveByInfo = CVehicleMetadataMgr::GetVehicleDriveByInfoFromPed(pPed);
			if (pDriveByInfo)
			{
				const u32 cameraHash = pDriveByInfo->GetDriveByCamera().GetHash();
				if (cameraHash)
				{
					settings.m_CameraHash = cameraHash;

					return;
				}
			}
		}

		const CWeapon* weapon = pPed->GetWeaponManager()->GetEquippedWeapon();

		// Use the weaponInfo if we are throwing a grenade while aiming as our equipped weapon will be the grenade.
		if (pPed->GetPedResetFlag(CPED_RESET_FLAG_IsThrowingProjectileWhileAiming))
		{
			weapon = NULL;
		}

		// This value is used on MG weapons - We hold the weapon in two different ways depending on whether the weapon has a scope attachment
		// Therefore we need two different aim cameras for no scope (HIP_AIM_CAMERA) and scope (DEFAULT_THIRD_PERSON_PED_AIM_CAMERA)
		bool bShouldUseAlternativeOrScopedCameraHash = ((weapon && weapon->GetScopeComponent()) || pPed->GetPedResetFlag(CPED_RESET_FLAG_UseAlternativeWhenBlock) || bTempWeaponHasScope);

		const bool shouldUseCinematicShooting = m_bShouldUseAssistedAimingCamera && CPauseMenu::GetMenuPreference(PREF_CINEMATIC_SHOOTING) FPS_MODE_SUPPORTED_ONLY( && !pPed->IsFirstPersonShooterModeEnabledForPlayer());

		if(shouldUseCinematicShooting)
		{
			if(bShouldUseAlternativeOrScopedCameraHash && weaponInfo->GetCinematicShootingAlternativeOrScopedCameraHash())
			{
				settings.m_CameraHash = weaponInfo->GetCinematicShootingAlternativeOrScopedCameraHash();
			}
			else
			{
				settings.m_CameraHash = weaponInfo->GetCinematicShootingCameraHash();
			}

			settings.m_InterpolationDuration = ms_Tunables.m_AssistedAimInterpolateInDuration;

			if (settings.m_CameraHash)
			{
				return;
			}
		}

		if (m_bShouldUseRunAndGunAimingCamera || shouldUseCinematicShooting)
		{
			if(bShouldUseAlternativeOrScopedCameraHash && weaponInfo->GetRunAndGunAlternativeOrScopedCameraHash())
			{
				settings.m_CameraHash = weaponInfo->GetRunAndGunAlternativeOrScopedCameraHash();
			}
			else
			{
				settings.m_CameraHash = weaponInfo->GetRunAndGunCameraHash();
			}
			settings.m_InterpolationDuration = ms_Tunables.m_RunAndGunInterpolateInDuration;
			return;
		}

		const WeaponControllerState controllerState	= GetWeaponControllerState(pPed);

		//NOTE: Reloading whilst aiming should maintain the camera aim state, but reloading prior to aiming should not initiate an aim camera.
		if(GetIsAiming() || ((controllerState == WCS_Reload) && isAnAimCameraActive))
		{
			//NOTE: We only request a first-person aim camera when the weapon is ready to fire (or reloading), as we don't interpolate into
			//first-person aim cameras.
			//NOTE: We explicitly exclude melee weapons here, as melee cameras should be requested by the melee combat task.
			const bool hasFirstPersonScope = weapon && weapon->GetHasFirstPersonScope();

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

			const bool isReadyToFire			= GetIsReadyToFire();
			const bool isReloading				= GetIsReloading();
			const bool shouldRequestAnAimCamera	= (!isMeleeWeapon || isAimedLikeGun) && (!hasFirstPersonScope || (hasFirstPersonScope && m_iFlags.IsFlagSet(GF_SkipIdleTransitions)) || isReadyToFire || isReloading) 
#if FPS_MODE_SUPPORTED
				&& (!pPed->IsLocalPlayer() || controllerState != WCS_Aim || CPlayerInfo::IsAiming())
#endif // FPS_MODE_SUPPORTED
				;
			if(shouldRequestAnAimCamera)
			{
				u32 desiredCameraHash = weapon ? weapon->GetDefaultCameraHash() : weaponInfo->GetDefaultCameraHash();

				if(m_aimTaskType == CTaskTypes::TASK_AIM_GUN_STANDING || m_aimTaskType == CTaskTypes::TASK_AIM_GUN_STRAFE ||
					m_aimTaskType == CTaskTypes::TASK_AIM_GUN_VEHICLE_DRIVE_BY)
				{
					if (pPed->IsLocalPlayer())
					{
						const CControl *pControl = pPed->GetControlFromPlayer();
						if (pControl->GetPedTargetIsUp(ioValue::ANALOG_BUTTON_DOWN_THRESHOLD))
						{
							//Do not override the camera.
							desiredCameraHash = 0;
						}
					}
				}
				else if(pPed->GetCoverPoint() && pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_IN_COVER))
				{
					//NOTE: If no ready-to-fire camera has been specified, we allow the parent cover task to define the camera settings.
					desiredCameraHash = weaponInfo->GetCoverReadyToFireCameraHash();
					if(desiredCameraHash)
					{
						//Allow a weapon scope component to override the camera hash, as in CWeapon::GetDefaultCameraHash(),
						//but only when we have a valid 'cover ready to fire' camera hash to begin with.
						const CWeaponComponentScope* weaponScopeComponent = weapon ? weapon->GetScopeComponent() : NULL;
						if(weaponScopeComponent)
						{
							if(pPed->GetIsInCover() && pPed->IsFirstPersonShooterModeEnabledForPlayer(false) 
								&& pPed->GetMotionData()->GetFPSState() != CPedMotionData::FPS_LT
								&& pPed->GetMotionData()->GetFPSState() != CPedMotionData::FPS_SCOPE)
							{
								//Do not override the camera.
								desiredCameraHash = 0;
							}
							else if(!hasFirstPersonScope)
							{
								// We have a 'cover ready to fire' camera hash (only set on sniper weapons), but we don't have a first person scope (reticule hash on the component is zero)?
								// That must mean we're using a normal scope on a weapon that's usually a sniper weapon, so reset the desired camera has back to zero. 
								// This will leave the camera using the weapon's CoverCameraHash set from the higher CTaskCover (DEFAULT_THIRD_PERSON_PED_AIM_IN_COVER_CAMERA).
								desiredCameraHash = 0;
							}
							else
							{
								desiredCameraHash = weaponScopeComponent->GetCameraHash();
							}
						}
					}
				}
#if FPS_MODE_SUPPORTED
				else if(hasFirstPersonScope && pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
				{
					if(GetSubTask() && (GetSubTask()->GetTaskType() == CTaskTypes::TASK_AIM_GUN_ON_FOOT))
					{
						const CTaskAimGunOnFoot* pAimGunTask = static_cast<const CTaskAimGunOnFoot*>(GetSubTask());
						if(pAimGunTask->IsReloading())
						{
							//Do not override the camera.
							desiredCameraHash = 0;
						}
					}
				}
#endif // FPS_MODE_SUPPORTED
				// For cases where we have different third-person animations depending on scope attachments (MG weapons), nothing to do with first person scopes
				else if(bShouldUseAlternativeOrScopedCameraHash && weaponInfo->GetAlternativeOrScopedCameraHash())
				{
					desiredCameraHash = weaponInfo->GetAlternativeOrScopedCameraHash();
				}

				if(desiredCameraHash)
				{
					settings.m_CameraHash = desiredCameraHash;
				}
			}
		}
	}
}

const CWeaponInfo* CTaskGun::GetPedWeaponInfo() const
{
	const CPed *pPed = GetPed();
	weaponAssert(pPed->GetWeaponManager());
	u32 uWeaponHash = pPed->GetWeaponManager()->GetEquippedWeaponHash();
	if( uWeaponHash == 0 )
	{
		const CVehicleWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedVehicleWeapon();
		if( pWeapon )
		{
			uWeaponHash = pWeapon->GetHash();
		}
	}
	return CWeaponInfoManager::GetInfo<CWeaponInfo>(uWeaponHash);
}

bool CTaskGun::GetIsReadyToFire() const
{
	if(GetState() == State_Aim && GetSubTask())
	{
		taskFatalAssertf(dynamic_cast<const CTaskAimGun*>(GetSubTask()), "Subtask expected to be AimGun!");
		return static_cast<const CTaskAimGun*>(GetSubTask())->GetIsReadyToFire();
	}

	return false;
}

bool CTaskGun::GetIsFiring() const
{
	if(GetState() == State_Aim && GetSubTask())
	{
		taskFatalAssertf(dynamic_cast<const CTaskAimGun*>(GetSubTask()), "Subtask is not of type CTaskAimGun!");
		return static_cast<const CTaskAimGun*>(GetSubTask())->GetIsFiring();
	}

	return false;
}

float CTaskGun::GetTimeSpentFiring() const
{
	if(GetState() == State_Aim && GetSubTask())
	{
		taskFatalAssertf(dynamic_cast<const CTaskAimGun*>(GetSubTask()), "Subtask is not of type CTaskAimGun!");
		const CTaskAimGun* pAimGunTask = static_cast<const CTaskAimGun*>(GetSubTask());
		if(pAimGunTask->GetIsFiring())
		{
			return pAimGunTask->GetTimeInState();
		}
	}

	return 0.0f;
}

void CTaskGun::SetBlockFiringTime(u32 uBlockFiringTime)
{
	 m_uBlockFiringTime = uBlockFiringTime; 

	 if(GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_AIM_GUN_ON_FOOT)
	 {
		 CTaskAimGunOnFoot * pAimGunTask = static_cast<CTaskAimGunOnFoot*>(GetSubTask());
		 pAimGunTask->SetBlockFiringTime(uBlockFiringTime);
	 }
}

#if FPS_MODE_SUPPORTED
bool CTaskGun::ShouldDelayAfterSwap() const
{
	const CPed* pPed = GetPed();
	static dev_u32 MIN_SWAP_TIME = 300;
	if(!pPed->IsFirstPersonShooterModeEnabledForPlayer(false) || (fwTimer::GetTimeInMilliseconds() - m_uLastSwapTime) > MIN_SWAP_TIME)
	{
		return false;
	}
	return true;
}
#endif // FPS_MODE_SUPPORTED

#if !__FINAL
void CTaskGun::Debug() const
{
#if DEBUG_DRAW
// 	if (!CTaskAimGun::ms_bUseNewGunTasks)
 /*	{
		if(GetPed())
		{
 			Vector3 vTargetPos;
			Vector3 vTargetPosWithOffsets;
			if(m_target.GetIsValid())
			{
				if(m_target.GetPosition(vTargetPos))
				{
 					if(m_target.GetPositionWithFiringOffsets(GetPed(), vTargetPosWithOffsets))
 					{
						Displayf("CTaskGun::Debug - TargetPos = %f %f %f", vTargetPos.x, vTargetPos.y, vTargetPos.z);
						Displayf("CTaskGun::Debug - TargetPosWithOffsets = %f %f %f", vTargetPosWithOffsets.x, vTargetPosWithOffsets.y, vTargetPosWithOffsets.z);

 						grcDebugDraw::Sphere(vTargetPos, 0.3f, Color_blue, false);
						grcDebugDraw::Sphere(vTargetPosWithOffsets, 0.5f, Color_orange, false);
			 
 						weaponAssert(GetPed()->GetWeaponManager());
 						const CObject* pWeaponObject = GetPed()->GetWeaponManager()->GetEquippedWeaponObject();
 						if(pWeaponObject)
 						{
 							grcDebugDraw::Line(VEC3V_TO_VECTOR3(pWeaponObject->GetTransform().GetPosition()), vTargetPos, Color_orange);
 						}
 					}
			 
					char buffer[1024];
					Vector3 targetPos;
					m_target.GetPosition(targetPos);
					Vector3 offset = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition()) - targetPos;

					sprintf(buffer, "TargetWithOffsets = %f %f %f/nTarget Pos = %f %f %f/nTarget Dist = %f/nOffset = %f %f %f", 
									vTargetPosWithOffsets.x, vTargetPosWithOffsets.y, vTargetPosWithOffsets.z,
									targetPos.x, targetPos.y, targetPos.z,
									(VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition()) - targetPos).Mag(),
									offset.x, offset.y, offset.z						
					);
					grcDebugDraw::Text(GetPed()->GetTransform().GetPosition(), Color_white, buffer);
				}
			}
		}*/
// 		if (GetPed()->GetIkManager().HasSolver(CIkManager::ikSolverTypeTorso))
// 		{
// 			float fYaw = GetPed()->GetIkManager().GetTorsoYaw();
// 			Vector3 vPedForward(-rage::Sinf(fYaw), rage::Cosf(fYaw), 0.0f);
// 			vPedForward.NormalizeFast();
// 			const Vector3 vPedPosition = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition());
// 			grcDebugDraw::Line(vPedPosition, vPedPosition + vPedForward, Color_magenta);
// 		}
//	}
#endif // DEBUG_DRAW

	// Base class
	CTaskFSMClone::Debug();
}
#endif // !__FINAL

WeaponControllerState CTaskGun::GetWeaponControllerState(const CPed* pPed) const
{
	return CWeaponControllerManager::GetController(m_weaponControllerType)->GetState(pPed, false);
}

CTaskGun::FSM_Return CTaskGun::ProcessPreFSM()
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	// Check we have a valid weapon (Don't allow blind firing to exit early B*30748
	if(!pPed->IsNetworkClone() && !(m_iFlags & GF_BlindFiring) && !HasValidWeapon(pPed))
	{
		// B*2330178: Interrupt reload if we're reloading and ammo caching is active. 
		if (pPed->GetPlayerInfo() && pPed->GetPlayerInfo()->ms_Tunables.m_bUseAmmoCaching && GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_RELOAD_GUN)
		{
			CTaskReloadGun* pReloadTask = static_cast<CTaskReloadGun*>(GetSubTask());
			pReloadTask->RequestInterrupt();
		}

		// Quit due to invalid weapon
		taskDebugf1("CTaskGun::ProcessPreFSM: FSM_Quit - !HasValidWeapon. Previous State: %d", GetPreviousState());
		return FSM_Quit;
	}

	if(GetIsFlagSet(aiTaskFlags::IsAborted) && !m_bAllowRestartAfterAbort)
	{
		// Quit
		taskDebugf1("CTaskGun::ProcessPreFSM: FSM_Quit - aiTaskFlags::IsAborted. Previous State: %d", GetPreviousState());
		return FSM_Quit;
	}

	bool bFirstPersonAiming = false;
	bool bFirstPersonRunAndGunWhileSprinting = false;

	bool bUsingKeyboardAndMouse = false;
#if KEYBOARD_MOUSE_SUPPORT
	bUsingKeyboardAndMouse = pPed->GetControlFromPlayer() && pPed->GetControlFromPlayer()->WasKeyboardMouseLastKnownSource();
#endif

#if FPS_MODE_SUPPORTED
	if(pPed->GetPlayerInfo() && (pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true)
#if KEYBOARD_MOUSE_SUPPORT
		// Use FPS sprint logic for mouse and keyboard controls
		|| (bUsingKeyboardAndMouse)
#endif
		))
	{
		bFirstPersonAiming = pPed->GetPlayerInfo()->IsAiming();
		const CControl* pControl = pPed->GetControlFromPlayer();
		const WeaponControllerState controllerState = GetWeaponControllerState(pPed);

		const u32 uCurrentTime = fwTimer::GetTimeInMilliseconds();
		const CTaskGun::Tunables taskGunTunables = GetTunables();

		// B*2278001: Allow reload from sprint if using mouse/keyboard or in a toggle-run FPS configuration on gamepad.
		bool bIsReload = false;
		if ((bUsingKeyboardAndMouse || (pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && camFirstPersonShooterCamera::WhenSprintingUseRightStick(pPed, *pControl))) && pPed->GetWeaponManager())
		{
			const CWeapon* pWeaponUsable = pPed->GetWeaponManager()->GetEquippedWeapon();
			if (pWeaponUsable)
			{
				bool bCanReload = pWeaponUsable->GetCanReload();
				bool bWantsToReload = (pControl->GetPedReload().IsPressed() || pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ForceReload)) && bCanReload;
				bool bNeedsToReload = pWeaponUsable->GetNeedsToReload(true) || (pPed->GetWeaponManager()->GetEquippedWeaponInfo() && pPed->GetWeaponManager()->GetEquippedWeaponInfo()->GetCreateVisibleOrdnance() && pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetPedEquippedWeapon()->GetProjectileOrdnance() == NULL );
				bIsReload = (bWantsToReload || bNeedsToReload) && bCanReload;
			}
		}

		if(pPed->GetPlayerInfo()->IsSoftFiring() || controllerState == WCS_Fire || controllerState == WCS_FireHeld || bIsReload)
		{
			m_uLastFirstPersonRunAndGunTimeWhileSprinting = uCurrentTime;
			m_iTimeToAllowSprinting = taskGunTunables.m_uFirstPersonRunAndGunWhileSprintTime;
		}

		bool bPreventSprintDelay = false;
		if(pControl != NULL)
		{
			const u32 uElapsedTimeSinceLastShot = uCurrentTime - m_uLastFirstPersonRunAndGunTimeWhileSprinting;
			const bool bSprintWasPressedBeforeShooting = pControl->GetPedSprintHistoryHeldDown(uElapsedTimeSinceLastShot + 100);
			bPreventSprintDelay = (bSprintWasPressedBeforeShooting && (pControl->GetPedSprintIsReleased() || pControl->GetPedSprintIsDown())) || pControl->GetPedSprintIsPressed();
		}
		
		if( bPreventSprintDelay && m_iTimeToAllowSprinting != (s32)taskGunTunables.m_uFirstPersonMinTimeToSprint )
		{
			m_iTimeToAllowSprinting = taskGunTunables.m_uFirstPersonMinTimeToSprint;
		}

		bFirstPersonRunAndGunWhileSprinting = m_uLastFirstPersonRunAndGunTimeWhileSprinting > 0 && 
			((uCurrentTime - m_uLastFirstPersonRunAndGunTimeWhileSprinting) < m_iTimeToAllowSprinting);

		// Reset m_bReloadIsBlocked flag if the ped has moved or turned.
		if(m_bReloadIsBlocked)
		{
			TUNE_GROUP_FLOAT(WEAPON_BLOCK, fMinHeadingDiff, 1.0f, 0.0f, 90.0f, 1.0f); 
			TUNE_GROUP_FLOAT(WEAPON_BLOCK, fMinDistanceDiff, 0.1f, 0.0f, 1.0f, 0.1f); 

			if(Abs(m_vPedHeadingWhenReloadIsBlocked - pPed->GetCurrentHeading()) > DtoR*fMinHeadingDiff)
			{
				m_bReloadIsBlocked = false;
			}
			else
			{
				Vec3V vPedPos = pPed->GetTransform().GetPosition();

				// Check the relative position if the ped is on a ground physical.
				CPhysical* pGroundPhysical = pPed->GetGroundPhysical();
				if(pGroundPhysical)
				{
					vPedPos = UnTransformFull(pGroundPhysical->GetTransform().GetMatrix(), vPedPos);
				}

				if(IsGreaterThanAll(Mag(vPedPos - m_vPedPosWhenReloadIsBlocked), ScalarVFromF32(fMinDistanceDiff)))
				{
					m_bReloadIsBlocked = false;
				}
			}
		}
	}
#endif

	//Don't do sprint/aim breakout if swimming 
#if FPS_MODE_SUPPORTED
	const bool bInFpsMode = pPed->IsFirstPersonShooterModeEnabledForPlayer(false);
	const bool bInFpsStrafeMode = pPed->IsFirstPersonShooterModeEnabledForPlayer();
#endif // FPS_MODE_SUPPORTED
	if(pPed->IsLocalPlayer() && !pPed->GetIsSwimming() && !bFirstPersonAiming && !bFirstPersonRunAndGunWhileSprinting 
#if FPS_MODE_SUPPORTED
		&& !bInFpsStrafeMode
#endif // FPS_MODE_SUPPORTED
#if KEYBOARD_MOUSE_SUPPORT
		&& ((pPed->GetControlFromPlayer() && !pPed->GetControlFromPlayer()->WasKeyboardMouseLastKnownSource()) || !pPed->GetPlayerInfo()->IsAiming()) 
#endif // KEYBOARD_MOUSE_SUPPORT
		)
	{
		bool bHitSprintMBR = pPed->GetMotionData()->GetIsDesiredSprinting(pPed->GetMotionData()->GetFPSCheckRunInsteadOfSprint());

		float fSprintResult = pPed->GetPlayerInfo()->ControlButtonSprint(CPlayerInfo::SPRINT_ON_FOOT, true);

		static dev_float s_fSprintResultThreshold = 1.25f;

		bool bSprinting = bHitSprintMBR && (fSprintResult >= s_fSprintResultThreshold);

		bool bWeaponBlockedInFPSMode = false;  //pPed->GetPedResetFlag(CPED_RESET_FLAG_WeaponBlockedInFPSMode) && bInFpsMode;

		if((bSprinting || (bInFpsMode && !bInFpsStrafeMode)) && GetState() != State_Reload && GetState() != State_Swap && !pPed->GetMotionData()->GetCombatRoll() && !CPlayerInfo::IsCombatRolling() && !bWeaponBlockedInFPSMode && 
#if FPS_MODE_SUPPORTED
			(!bInFpsMode || !pPed->GetWeaponManager()->GetRequiresWeaponSwitch() || m_iFlags.IsFlagSet(GF_PreventWeaponSwapping))
#endif // FPS_MODE_SUPPORTED
			)
		{
#if FPS_MODE_SUPPORTED
			if(!ShouldDelayAfterSwap() && !bWeaponBlockedInFPSMode)
#endif // FPS_MODE_SUPPORTED
			{
				pPed->GetPlayerInfo()->SprintAimBreakOut();
				taskDebugf1("CTaskGun::ProcessPreFSM: FSM_Quit - bSprinting. Previous State: %d", GetPreviousState());
				return FSM_Quit;
			}
		}
	}

	//
	// Set up general task flags
	//

	pPed->SetPedResetFlag( CPED_RESET_FLAG_IsAiming, true );
	pPed->SetPedResetFlag( CPED_RESET_FLAG_MoveBlend_bTaskComplexGunRunning, true );

	CTask* pTask = pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_AIMING);
	if (pTask)
	{
		if (pTask->GetState() == CTaskMotionAiming::State_Initial || pTask->GetState() == CTaskMotionAiming::State_StartMotionNetwork)
		{
			pPed->SetPedResetFlag(CPED_RESET_FLAG_NotAllowedToChangeCrouchState, true);
		}
	}

	//
	// Perform some general processing
	//

	// Update the head look at
	ProcessLookAt(pPed);

	//Process the eye ik.
	ProcessEyeIk();

	// Update the target
	UpdateTarget(pPed);

	if(!pPed->IsNetworkClone())
	{
		if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_SWAP_WEAPON)
		{
			if (static_cast<CTaskSwapWeapon*>(GetSubTask())->GetFlags().IsFlagSet(SWAP_TO_AIM) && m_target.GetIsValid())
			{
				Vector3 vTargetPos;
				if (m_target.GetPositionWithFiringOffsets(pPed, vTargetPos))
				{
					pPed->SetDesiredHeading(vTargetPos);
				}
			}
		}
	}
	
	//Choose the bullet reaction clip set.
	fwMvClipSetId bulletReactionClipSetId = ChooseClipSetForBulletReaction();
	if(bulletReactionClipSetId != CLIP_SET_ID_INVALID)
	{
		m_BulletReactionClipSetRequestHelper.RequestClipSet(bulletReactionClipSetId);
	}
	else
	{
		m_BulletReactionClipSetRequestHelper.ReleaseClipSet();
	}

	m_bShouldUseAssistedAimingCamera = CTaskGun::ProcessUseAssistedAimingCamera(*pPed, false, true);

	if (!m_bShouldUseAssistedAimingCamera)
	{
		m_bShouldUseRunAndGunAimingCamera = CTaskGun::ProcessUseRunAndGunAimingCamera(*pPed, false, true);
	}

	if(m_iFlags.IsFlagSet(GF_AllowAimingWithNoAmmo))
	{
		// If at any point our weapon receives ammo, unset the flag
		if(pPed->GetWeaponManager())
		{
			const CWeaponInfo* pWeaponInfo = pPed->GetWeaponManager()->GetEquippedWeaponInfo();
			if(pWeaponInfo && pPed->GetInventory() && !pPed->GetInventory()->GetIsWeaponOutOfAmmo(pWeaponInfo))
			{
				m_iFlags.ClearFlag(GF_AllowAimingWithNoAmmo);
			}
		}
	}
	
	//Process the facial idle clip.
	ProcessFacialIdleClip();

	if(pPed->IsLocalPlayer())
	{
		if(GetState() == State_Aim)
		{
			bool bRunAndGunning = pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON) || pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_RUN_AND_GUN);

			//! Cache last time we were aiming for outro delay purposes.
			if(pPed->GetPlayerInfo()->IsAiming() && !bRunAndGunning)
			{
				m_uLastIsAimingTimeForOutroDelay = fwTimer::GetTimeInMilliseconds();
			}
		}
		else
		{
			//! Note: When not in aim, we don't want to include other states as part of the delay for triggering assisted aim
			//! outro times. So, to counter this, we extend aim time by time in state.
			m_uLastIsAimingTimeForOutroDelay = m_uLastIsAimingTimeForOutroDelayCached + (u32)(GetTimeInState() * 1000.0f); 
		}
	}

	return FSM_Continue;
}

CTaskGun::FSM_Return CTaskGun::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		FSM_State(State_Init)
			FSM_OnUpdate
				return StateInit(pPed);
		FSM_State(State_Decide)
			FSM_OnUpdate
				return StateDecide(pPed);
		FSM_State(State_AimIntro)
			FSM_OnEnter
				return StateAimIntroOnEnter();
			FSM_OnUpdate
				return StateAimIntroOnUpdate();
		FSM_State(State_Aim)
			FSM_OnEnter
				return StateAimOnEnter(pPed);
			FSM_OnUpdate
				return StateAimOnUpdate(pPed);
			FSM_OnExit
				return StateAimOnExit(pPed);
		FSM_State(State_Reload)
			FSM_OnEnter
				return StateReloadOnEnter(pPed);
			FSM_OnUpdate
				return StateReloadOnUpdate(pPed);
			FSM_OnExit
				return StateReloadOnExit(pPed);
		FSM_State(State_Swap)
			FSM_OnEnter
				return StateSwapOnEnter(pPed);
			FSM_OnUpdate
				return StateSwapOnUpdate(pPed);
		FSM_State(State_BulletReaction)
			FSM_OnEnter
				return StateBulletReactionOnEnter();
			FSM_OnUpdate
				return StateBulletReactionOnUpdate();
			FSM_OnExit
				return StateBulletReactionOnExit(pPed);
		FSM_State(State_Quit)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

CTaskGun::FSM_Return CTaskGun::ProcessPostFSM()
{
	// Clear the bullet reaction flag (it should only happen on the frame we were told)
	m_iFlags.ClearFlag((u32)GF_PlayBulletReaction);

	// Decrement the duration remaining in seconds
	m_fDuration -= GetTimeStep();

	// Increment the time the task has been active
	m_fTimeActive += GetTimeStep();

	return FSM_Continue;
}

bool CTaskGun::ProcessMoveSignals()
{
	//Check the state.
	switch(GetState())
	{
		case State_BulletReaction:
		{
			return StateBulletReaction_OnProcessMoveSignals();
		}
		default:
		{
			break;
		}
	}

	return false;
}

CTaskInfo *CTaskGun::CreateQueriableState() const
{
	const CPed *pPed = GetEntity() ? GetPed() : NULL; //Get the ped ptr.

	// don't sync target info for players, they use the CPlayerCameraDataNode instead
	const CWeaponTarget* pWeaponTarget = NULL;

	if (!pPed || !pPed->IsPlayer())
	{
		pWeaponTarget = &m_target;
	}

	return rage_new CClonedGunInfo(GetState(), pWeaponTarget, GetGunFlags(), m_aimTaskType, m_uScriptedGunTaskInfoHash, m_fGunEventReactionDir, m_ReloadingOnEmpty);
}

void CTaskGun::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	CClonedGunInfo* gunInfo = static_cast<CClonedGunInfo*>(pTaskInfo);
	Assert(gunInfo);
	
	m_fGunEventReactionDir = gunInfo->GetBulletReactionDir();

	gunInfo->GetWeaponTargetHelper().ReadQueriableState(m_target);
	if(gunInfo->GetWeaponTargetHelper().HasTargetEntity())
	{
		SetTargetOutOfScopeID(gunInfo->GetWeaponTargetHelper().GetTargetEntityId());
	}

	m_ReloadingOnEmpty = gunInfo->GetReloadingOnEmpty();

	// Call the base implementation - syncs the state
	CTaskFSMClone::ReadQueriableState(pTaskInfo);
}

void CTaskGun::OnCloneTaskNoLongerRunningOnOwner()
{
    SetStateFromNetwork(State_Quit);
}

bool CTaskGun::StateChangeHandledByCloneFSM(s32 iState)
{
    switch(iState)
    {
	case State_Reload:
	case State_Swap:
        return true;
    default:
        return false;
    }
}

void CTaskGun::UpdateClonedSubTasks(CPed* pPed, int const iState)
{
	CTaskTypes::eTaskType expectedTaskType = CTaskTypes::TASK_INVALID_ID;
	switch(iState)
	{
		case State_AimIntro:
			expectedTaskType = CTaskTypes::TASK_REACT_AIM_WEAPON;
			break;
		default:
			return;
	}

	if(expectedTaskType != CTaskTypes::TASK_INVALID_ID)
	{
		 /*CTaskFSMClone::*/CreateSubTaskFromClone(pPed, expectedTaskType);
	}
}

CTaskGun::FSM_Return CTaskGun::UpdateClonedFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	bool bInFirstPerson = false;
	
#if FPS_MODE_SUPPORTED 
	bInFirstPerson = pPed->IsFirstPersonShooterModeEnabledForPlayer(true, true);
#endif

	if(!bInFirstPerson && !HasValidWeapon(pPed) && GetStateFromNetwork() != State_Quit)
	{
		// wait for the weapon before proceeding
		return FSM_Continue;
	}

    // if the state change is not handled by the clone FSM block below
    // we need to keep the state in sync with the values received from the network
    if(!StateChangeHandledByCloneFSM(iState))
    {
        if(iEvent == OnUpdate)
        {
            if(GetStateFromNetwork() != -1 && GetStateFromNetwork() != GetState())
            {
                SetState(GetStateFromNetwork());
				return FSM_Continue;
            }

			UpdateClonedSubTasks(pPed, GetState());
        }
    }

    FSM_Begin
		FSM_State(State_Aim)
			FSM_OnUpdate
				return StateAimOnUpdateClone(pPed);
			FSM_OnExit
				return StateAimOnExit(pPed);
		FSM_State(State_Reload)
			FSM_OnEnter
				return StateReloadOnEnter(pPed);
			FSM_OnUpdate
				return StateReloadOnUpdateClone(pPed);
			FSM_OnExit
				return StateReloadOnExit(pPed);

		FSM_State(State_BulletReaction)
			FSM_OnEnter
				return StateBulletReactionOnEnter();
			FSM_OnUpdate
				return FSM_Continue;
			FSM_OnExit
				return StateBulletReactionOnExit(pPed);

		FSM_State(State_Swap)
			FSM_OnEnter
				return StateSwapOnEnterClone(pPed);
			FSM_OnUpdate
				return StateSwapOnUpdateClone(pPed);

		FSM_State(State_Quit)
			FSM_OnUpdate
				return FSM_Quit;

		FSM_State(State_Init)
		FSM_State(State_Decide)
		FSM_State(State_AimIntro)

	FSM_End
}

#if !__FINAL
const char * CTaskGun::GetStaticStateName( s32 iState )
{
	static const char* aStateNames[] = 
	{
		"Init",
		"Decide",
		"AimIntro",
		"Aim",
		"Reload",
		"Swap",
		"BulletReaction",
		"Quit",
	};

	if (aiVerify(iState>=State_Init && iState<=State_Quit))
	{
		return aStateNames[iState];
	}

	return 0;
}
#endif // !__FINAL

CTaskGun::FSM_Return CTaskGun::StateInit(CPed* pPed)
{
	// Initialise the CFiringPattern with the specified firing pattern
	const bool bForceReset = m_iFlags.IsFlagSet(GF_ForceFiringPatternReset);

	u32 uFiringPatternHash = m_bHasFiringPatternOverride ? m_uFiringPatternHash : (u32)pPed->GetPedIntelligence()->GetCombatBehaviour().GetFiringPatternHash();
	pPed->GetPedIntelligence()->GetFiringPattern().SetFiringPattern(uFiringPatternHash, bForceReset);

	const CWeaponInfo* pWeaponInfo = pPed->GetWeaponManager()->GetEquippedWeaponInfo();
	if(pWeaponInfo)
	{
		if(pWeaponInfo->GetOnlyAllowFiring())
		{
			m_iFlags.SetFlag(GF_DisableBlockingClip);
		}

		// If we start the task with a weapon out of ammo, allow it to continue
		if(pPed->GetInventory() && pPed->GetInventory()->GetIsWeaponOutOfAmmo(pWeaponInfo) && !pWeaponInfo->GetDiscardWhenOutOfAmmo())
		{
			m_iFlags.SetFlag(GF_AllowAimingWithNoAmmo);
		}
	}

	if(pPed->GetPedResetFlag(CPED_RESET_FLAG_InstantBlendToAim) || pPed->GetPedResetFlag(CPED_RESET_FLAG_InstantBlendToAimFromScript))
	{
		// CS: Don't set the gun flag, as this causes a heading pop also.
		//GetGunFlags().SetFlag(GF_InstantBlendToAim);
		pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Aiming);
	}

	if(m_aimTaskType == CTaskTypes::TASK_AIM_GUN_SCRIPTED)
	{
		m_iFlags.SetFlag(GF_PreventWeaponSwapping);
	}

	// Set state to decide what to do next
	SetState(State_Decide);

	return FSM_Continue;
}

CTaskGun::FSM_Return CTaskGun::StateDecide(CPed* pPed)
{
	// Wait until the ped is out of swimming to avoid state machine infinite looping.
	if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_SwimmingTasksRunning) && !pPed->GetEquippedWeaponInfo()->GetUsableUnderwater())
	{
		return FSM_Continue;
	}

	// Cache the state of the controller
	WeaponControllerState controllerState = GetWeaponControllerState(pPed);
	CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
	bool bShouldReload = false;

	if(m_bPlayAimIntro && m_target.GetIsValid())
	{
		const CWeaponInfo* pEquippedWeaponInfo = pPed->GetWeaponManager()->GetEquippedWeaponInfo();
		if(pEquippedWeaponInfo && (pEquippedWeaponInfo->GetIsGun() || pEquippedWeaponInfo->GetCanBeAimedLikeGunWithoutFiring()))
		{
			SetState(State_AimIntro);
			return FSM_Continue;
		}
	}

	if(pWeapon)
	{
		// Check if we have LOS to our target if it's an entity
		bool bHasLosToTarget = true;
		if( !pPed->IsLocalPlayer() && !pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanShootWithoutLOS) &&
			m_target.GetEntity() )
		{
			CPedTargetting* pPedTargetting = pPed->GetPedIntelligence()->GetTargetting( true );
			if(pPedTargetting)
			{
				LosStatus losStatusToTarget = pPedTargetting->GetLosStatus(m_target.GetEntity());
				bHasLosToTarget = (losStatusToTarget == Los_clear || losStatusToTarget == Los_blockedByFriendly);
			}
		}

		// Do we want/have to reload?
		bShouldReload = (controllerState == WCS_Reload || m_bWantsToReload || pWeapon->GetNeedsToReload(bHasLosToTarget)) && pWeapon->GetCanReload() && CanReload() && !m_bWantsToRoll;
#if __DEV
		// Logging to catch url:bugstar:2289101
		if (!bShouldReload)
		{
			taskDebugf1("CTaskGun::StateDecide: !bShouldReload: (controllerState == WCS_Reload: %d, m_bWantsToReload: %d, pWeapon->GetNeedsToReload(bHasLosToTarget): %d), pWeapon->GetCanReload(): %d, CanReload(): %d, !m_bWantsToRoll: %d",
				controllerState == WCS_Reload, m_bWantsToReload, pWeapon->GetNeedsToReload(bHasLosToTarget), pWeapon->GetCanReload(), CanReload(), !m_bWantsToRoll);
		}
#endif
	}

	// Do we have ammo?
	weaponAssert(pPed->GetWeaponManager());
	const CWeaponInfo* pEquippedWeaponInfo = pPed->GetWeaponManager()->GetEquippedWeaponInfo();
	if(!m_iFlags.IsFlagSet(GF_AllowAimingWithNoAmmo) && pEquippedWeaponInfo && pPed->GetInventory()->GetIsWeaponOutOfAmmo(pEquippedWeaponInfo))
	{
		const CCombatDirector* pCombatDirector = pPed->GetPedIntelligence()->GetCombatDirector();
		if(pCombatDirector && (pCombatDirector->GetNumPeds() >= 3))
		{
			pPed->NewSay("OUT_OF_AMMO_GROUP_SHOOTOUT");
		}
		else
		{
			pPed->NewSay("OUT_OF_AMMO");
		}

		if(m_iFlags.IsFlagSet(GF_PreventAutoWeaponSwitching))
		{
			taskDebugf1("CTaskGun::ProcessPreFSM: FSM_Quit - GF_PreventAutoWeaponSwitching. Previous State: %d", GetPreviousState());
			// The selected weapon is out of ammo and the flags are preventing us from swapping, so quit
			return FSM_Quit;
		}
		else
		{
			// If we block weapon switching then simply equip unarmed
			// This will allow us to holster the no-ammo gun
			if( pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_BlockWeaponSwitching) )
			{
				pPed->GetWeaponManager()->EquipWeapon(pPed->GetDefaultUnarmedWeaponHash(), -1, true);
			}
			else if (pEquippedWeaponInfo->GetIsNonLethal() && pPed->IsPlayer() && pPed->GetPlayerInfo()->GetArcadeInformation().GetTeam() == eArcadeTeam::AT_CNC_COP)
			{
				// When a Cop player runs out of non-lethal ammo, we don't want to auto-switch to a lethal weapon and accidentally kill someone
				pPed->GetWeaponManager()->EquipBestMeleeWeapon();
			}
			else 
			{
				// Select the next best item
				// We should always have an empty slot to fall back to, 
				// which will result in us throwing our current weapon away and equipping nothing
 				pPed->GetWeaponManager()->EquipBestWeapon();
			}
		}
	}

	// Decide what to do based on controllerState
	bool bShouldUnEquipWeapon = pPed->GetWeaponManager()->GetShouldUnEquipWeapon();

	// Fix problem where the local player can get into a state where he will not load with infinite ammo the rocket launcher and will continue to toggle between aim and decide states visibly jittering.
	if(pWeapon)
	{
		if (NetworkInterface::IsGameInProgress())
		{
			if (pWeapon->GetWeaponInfo()->GetCreateVisibleOrdnance() && (pWeapon->GetAmmoTotal() == 0) && pPed->GetInventory() && pPed->GetInventory()->GetAmmoRepository().GetIsUsingInfiniteAmmo())
			{
				pWeapon->SetAmmoTotalInfinite(); //ensure this is set for infinite ammo
				bShouldReload = true;
			}
		}
	}

	if(!bShouldReload && controllerState == WCS_None && !m_iFlags.IsFlagSet(GF_ForceAimState) && !m_iFlags.IsFlagSet(GF_FireAtLeastOnce) && !m_iFlags.IsFlagSet(GF_AllowAimingWithNoAmmo) && !m_iFlags.IsFlagSet(GF_ForceAimOnce))
	{
		if(bShouldUnEquipWeapon)
			// Swap weapon, as we have the incorrect (or holstered) weapon equipped
			SetState(State_Swap);
		else
		{
			taskDisplayf("Frame %i, Ped %s quiting gun task due to not aiming, wanting to reload or wanting to unequip weapon", fwTimer::GetFrameCount(), pPed->GetModelName());
			return FSM_Quit;
		}
	}
	else if(pPed->GetWeaponManager()->GetRequiresWeaponSwitch() && !m_iFlags.IsFlagSet(GF_PreventWeaponSwapping) && (!GetSubTask() || GetSubTask()->GetTaskType() != CTaskTypes::TASK_AIM_GUN_ON_FOOT || static_cast<CTaskAimGunOnFoot*>(GetSubTask())->GetState() != CTaskAimGunOnFoot::State_CombatRoll))
	{
#if FPS_MODE_SUPPORTED
		const bool bIsFirstPersonShooterModeEnabledForPlayer = pPed->IsFirstPersonShooterModeEnabledForPlayer(false);
		if(!bIsFirstPersonShooterModeEnabledForPlayer || pPed->GetInventory()->GetIsStreamedIn(pPed->GetWeaponManager()->GetEquippedWeaponHash()))
#endif // FPS_MODE_SUPPORTED
		{
			const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
			if(pWeapon && pWeapon->GetState() == CWeapon::STATE_OUT_OF_AMMO)
			{
				// Swap weapon, as we have the incorrect (or holstered) weapon equipped
				if(g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::GroupShootout))
				{
					pPed->NewSay("OUT_OF_AMMO_GROUP_SHOOTOUT",0, true,false);
				}
				else
				{
					pPed->NewSay("OUT_OF_AMMO",0, true,false);
				}

				//We clear this flag if the drawn weapon doesn't require it
				m_iGFFlags.SetFlag(GFF_RequireFireReleaseBeforeFiring);
			}

			// B*1892410: If swapping to jerry can, use weapon swap state from CTaskPlayerOnFoot instead of swap from within gun task.
			if(!bIsFirstPersonShooterModeEnabledForPlayer) 
			{
				const CWeaponInfo* pSelectedWeaponInfo = pPed->GetWeaponManager()->GetSelectedWeaponInfo();
				if (pSelectedWeaponInfo && pSelectedWeaponInfo->GetFlag(CWeaponInfoFlags::OnlyAllowFiring))
				{
					taskDebugf1("CTaskGun::StateDecide: FSM_Quit - JerryCan - Don't use gun task for swap. Previous State: %d", GetPreviousState());
					return FSM_Quit;
				}
			}

			SetState(State_Swap);
		}
	}
	else
	{
		if(pWeapon)
		{
			// Do we want/have to reload?
			if(bShouldReload)
			{
				if(m_iFlags.IsFlagSet(GF_DisableReload))
				{
					// We can't reload, so quit
					taskDebugf1("CTaskGun::StateDecide: FSM_Quit - bShouldReload. Previous State: %d", GetPreviousState());
					return FSM_Quit;
				}
				else
				{
					// Start reloading
#if FPS_MODE_SUPPORTED
					if (!pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
#endif // FPS_MODE_SUPPORTED
					{
						taskAssertf(!pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_IN_COVER), "Ped reloading whilst out of cover");
					}
					SetState(State_Reload);
				}
			}
			else
			{
				// Ensure the target is up to date
				UpdateTarget(pPed);

				// Check we still have a valid target
				if(!m_target.GetIsValid())
				{
					taskDebugf1("CTaskGun::StateDecide: FSM_Quit - !m_target.GetIsValid(). Previous State: %d", GetPreviousState());
					return FSM_Quit;
				}

				// If we're only allowed to fire the weapon and the controller state isn't fire, then quit
				bool bShouldQuit = false;
				if (pWeapon)
				{
					const CWeaponInfo* pWeaponInfo = pWeapon->GetWeaponInfo();
					if (pWeaponInfo->GetOnlyAllowFiring() 
#if FPS_MODE_SUPPORTED
						&& !pPed->IsFirstPersonShooterModeEnabledForPlayer()
#endif // FPS_MODE_SUPPORTED
						)
					{
						if (controllerState != WCS_Fire && controllerState != WCS_FireHeld)
						{
							bShouldQuit = true;
						}
					}
					else if (!pWeaponInfo->GetIsGunOrCanBeFiredLikeGun() && !pWeaponInfo->GetCanBeAimedLikeGunWithoutFiring() && m_aimTaskType != CTaskTypes::TASK_AIM_GUN_VEHICLE_DRIVE_BY 
#if FPS_MODE_SUPPORTED
						&& (!pPed->IsFirstPersonShooterModeEnabledForPlayer(!ShouldDelayAfterSwap() && !pPed->GetPedResetFlag(CPED_RESET_FLAG_WeaponBlockedInFPSMode)) || pWeaponInfo->GetIsThrownWeapon())
#endif // FPS_MODE_SUPPORTED
						)
					{
						bShouldQuit = true;
					}
					
					// Harpoon: Only allow aiming if SwimIdleOutro isn't playing (do we don't get stuck in the outro state)
					if (pPed->GetIsSwimming())
					{
						const CTaskMotionBase* pMotionTask = pPed->GetCurrentMotionTask(false);
						if(pMotionTask && pMotionTask->GetTaskType() == CTaskTypes::TASK_MOTION_AIMING && pMotionTask->GetState() == CTaskMotionAiming::State_SwimIdleOutro)
						{
							bShouldQuit = true;
						}
					}

				}

				if(!bShouldQuit && 
					(controllerState == WCS_Fire || controllerState == WCS_FireHeld || controllerState == WCS_CombatRoll || (controllerState == WCS_Aim && !m_iFlags.IsFlagSet(GF_DisableAiming)) || m_iFlags.IsFlagSet(GF_AlwaysAiming) || m_iFlags.IsFlagSet(GF_FireAtLeastOnce) || m_iFlags.IsFlagSet(GF_ForceAimState) || m_iFlags.IsFlagSet(GF_ForceAimOnce)))
				{
					// We are aiming or firing
					SetState(State_Aim);
				}
				else
				{
					// No action, so quit
					taskDebugf1("CTaskGun::StateDecide: FSM_Quit - No Action. Previous State: %d", GetPreviousState());
					return FSM_Quit;
				}
			}
		}
		else
		{
			// No weapon, so quit
			taskDebugf1("CTaskGun::StateDecide: FSM_Quit - !pWeapon. Previous State: %d", GetPreviousState());
			return FSM_Quit;
		}
	}

	return FSM_Continue;
}

CTaskGun::FSM_Return CTaskGun::StateAimIntroOnEnter()
{
	fwFlags8 uConfigFlags = 0;
	CTaskReactAimWeapon* pTask = rage_new CTaskReactAimWeapon(m_target, uConfigFlags);

	SetNewTask(pTask);
	return FSM_Continue;
}

CTaskGun::FSM_Return CTaskGun::StateAimIntroOnUpdate()
{
	if(GetIsSubtaskFinished(CTaskTypes::TASK_REACT_AIM_WEAPON))
	{
		SetState(State_Aim);
	}

	return FSM_Continue;
}

CTaskGun::FSM_Return CTaskGun::StateAimOnEnter(CPed* pPed)
{
	m_iFlags.ClearFlag(GF_ForceAimOnce);
	//---

	// if we're playing MP, turn off invincibility as soon as we start aiming if we've got a lock on target....
	if(NetworkInterface::IsGameInProgress())
	{
		if (pPed->IsNetworkClone())
		{
			CObject* pWeapon = pPed->GetWeaponManager()->GetEquippedWeaponObject();
			if (pWeapon && !pWeapon->GetIsVisible())
			{
				pWeapon->SetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY, true, true);
			}
		}

		CPed* pPed = GetPed();

		if(pPed && !pPed->IsNetworkClone() && pPed->IsPlayer())
		{
			CPlayerPedTargeting& targeting	= pPed->GetPlayerInfo()->GetTargeting();					
			if(targeting.GetLockOnTarget())
			{
				CNetGamePlayer *pNetGamePlayer = (pPed->GetNetworkObject() && pPed->GetNetworkObject()->GetPlayerOwner()) ? SafeCast(CNetGamePlayer, pPed->GetNetworkObject()->GetPlayerOwner()) : 0;

				if(pNetGamePlayer)
				{
					if((static_cast<CNetObjPlayer*>(pPed->GetNetworkObject()))->GetRespawnInvincibilityTimer() > 0)
					{
						(static_cast<CNetObjPlayer*>(pPed->GetNetworkObject()))->SetRespawnInvincibilityTimer(0);
					}
				}
			}
		}
	}

	//---

	// Once we enter the aim task we don't want to play the aim intro again from this point on
	m_bPlayAimIntro = false;

	if(m_weaponControllerType == CWeaponController::WCT_Fire || m_weaponControllerType == CWeaponController::WCT_FireHeld)
	{
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_HasBeenInArmedCombat, true);
	}

	CTaskAimGun* pAimGunTask = CTaskAimGunFactory::CreateTask(m_aimTaskType, m_weaponControllerType, m_iFlags, m_iGFFlags, m_target, m_ikInfo, &m_MoveNetworkHelper, m_uBlockFiringTime);
	if(pAimGunTask)
	{
		pAimGunTask->SetOverrideAimClipId(m_overrideAimClipSetId, m_overrideAimClipId);
		pAimGunTask->SetOverrideFireClipId(m_overrideFireClipSetId, m_overrideFireClipId);
		pAimGunTask->SetOverrideClipSetId(m_overrideClipSetId);
		pAimGunTask->SetScriptedGunTaskInfoHash(m_uScriptedGunTaskInfoHash);
		if(pAimGunTask->GetTaskType() == CTaskTypes::TASK_AIM_GUN_ON_FOOT)
			static_cast<CTaskAimGunOnFoot*>(pAimGunTask)->SetForceCombatRoll(m_bWantsToRoll);
		else if(pAimGunTask->GetTaskType() == CTaskTypes::TASK_AIM_GUN_VEHICLE_DRIVE_BY)
			static_cast<CTaskAimGunVehicleDriveBy*>(pAimGunTask)->SetShouldInstantlyBlendDrivebyAnims(m_bInstantlyBlendDrivebyAnims);
		m_bWantsToRoll = false;
		m_bInstantlyBlendDrivebyAnims = false;
		SetNewTask(pAimGunTask);
		pPed->SetPedResetFlag(CPED_RESET_FLAG_HasGunTaskWithAimingState, true);

#if GTA_REPLAY
		//we need to know if this task was running so we can do some extra camera magic during playback
		if(pPed && CReplayMgr::IsReplayInControlOfWorld() == false && m_aimTaskType == CTaskTypes::TASK_AIM_GUN_ON_FOOT)
		{
			pPed->GetReplayRelevantAITaskInfo().SetAITaskRunning(REPLAY_TASK_AIM_GUN_ON_FOOT);
		}
#endif

		return FSM_Continue;
	}
	else
	{
		taskDebugf1("CTaskGun::StateAimOnEnter: FSM_Quit - !pAimGunTask. Previous State: %d", GetPreviousState());
		return FSM_Quit;
	}
}

CTaskGun::FSM_Return CTaskGun::StateAimOnUpdate(CPed* pPed)
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		int taskType = GetSubTask() ? GetSubTask()->GetTaskType() : CTaskTypes::TASK_INVALID_ID;
		if(taskType == CTaskTypes::TASK_AIM_GUN_RUN_AND_GUN || taskType == CTaskTypes::TASK_AIM_GUN_BLIND_FIRE || taskType == CTaskTypes::TASK_AIM_GUN_VEHICLE_DRIVE_BY)
		{
			taskDebugf1("CTaskGun::StateAimOnUpdate: FSM_Quit - SubTaskFinished. Previous State: %d", GetPreviousState());
			return FSM_Quit;
		}
		else
		{
			if(GetSubTask())
			{
				if(GetSubTask()->GetTimeRunning() == 0.0f)
				{
					Displayf("Possible infinite loop : aim task type  : %u", m_aimTaskType);
				}
			}

			if(taskType == CTaskTypes::TASK_AIM_GUN_ON_FOOT)
			{
				const CTaskAimGunOnFoot* pGunTask = static_cast<const CTaskAimGunOnFoot*>(GetSubTask());
				m_bWantsToReload = pGunTask->IsReloading();
			}

			SetState(State_Decide);
		}
	}
	else
	{
		taskFatalAssertf(dynamic_cast<CTaskAimGun*>(GetSubTask()) != NULL, "Subtask is not of type CTaskAimGun!");

		// Has the duration run out?
		if(HasDurationExpired())
		{
			if (GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_AIM_GUN_BLIND_FIRE)
			{
				static_cast<CTaskAimGunBlindFire*>(GetSubTask())->RequestTaskEnd();
			}
			else
			{
				taskDebugf1("CTaskGun::StateAimOnUpdate: FSM_Quit - !GetSubTask() && GetSubTask()->GetTaskType()!=CTaskTypes::TASK_AIM_GUN_BLIND_FIRE. Previous State: %d", GetPreviousState());
				return FSM_Quit;
			}
		}
		else
		{
			// Cache the aim task
			CTaskAimGun* pAimGunTask = static_cast<CTaskAimGun*>(GetSubTask());
			if( pAimGunTask )
			{
				// Update the aim sub task
				pAimGunTask->SetTarget(m_target);
				pAimGunTask->SetWeaponControllerType(m_weaponControllerType);

				// Check for bullet reactions (required a valid event, the ped not crouching, currently running aim gun on foot and have a 1 handed weapon)
				if(!pAimGunTask->GetIsWeaponBlocked() && ShouldPlayBulletReaction())
				{
					SetState(State_BulletReaction);
					return FSM_Continue;
				}
			}
				
			pPed->SetPedResetFlag(CPED_RESET_FLAG_HasGunTaskWithAimingState, true);
		}
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////
// Notification that a gun shot whizzed by event happened
//////////////////////////////////////////////////////////////////////////

void CTaskGun::OnGunShotWhizzedBy(const CEventGunShotWhizzedBy& rEvent)
{
	if(GetPed())
	{
		// Calculate the direction from our ped to the source entity of the event
		const Vec3V vPedPosition = GetPed()->GetTransform().GetPosition();
		if(rEvent.IsBulletInRange(VEC3V_TO_VECTOR3(vPedPosition), m_vGunEventPosition))
		{
			m_iFlags.SetFlag((u32)GF_PlayBulletReaction);
		}
		else
		{
			m_vGunEventPosition.ZeroComponents();
		}
	}
}

fwMvClipSetId CTaskGun::ChooseClipSetForBulletReaction() const
{
	//Ensure the weapon manager is valid.
	const CPedWeaponManager* pWeaponManager = GetPed()->GetWeaponManager();
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

	u32 iWeaponGroup = pWeaponInfo->GetGroup();

	if(iWeaponGroup == WEAPONGROUP_PISTOL)
	{
		return ms_BulletReactionPistolClipSetId;
	}
	else if(iWeaponGroup == WEAPONGROUP_RIFLE || iWeaponGroup == WEAPONGROUP_SHOTGUN || iWeaponGroup == WEAPONGROUP_SMG || iWeaponGroup == WEAPONGROUP_SNIPER)
	{
		return ms_BulletReactionRifleClipSetId;
	}

	return CLIP_SET_ID_INVALID;
}

//////////////////////////////////////////////////////////////////////////
// Determine if we should play a bullet reaction
//////////////////////////////////////////////////////////////////////////
bool CTaskGun::ShouldPlayBulletReaction()
{
	//Ensure bullet reactions are not disabled.
	if(m_iFlags.IsFlagSet(GF_DisableBulletReactions))
	{
		return false;
	}

	//Ensure the clip set request helper is active.
	if(!m_BulletReactionClipSetRequestHelper.IsActive())
	{
		return false;
	}

	//Ensure the clip set should not be streamed out.
	if(m_BulletReactionClipSetRequestHelper.ShouldClipSetBeStreamedOut())
	{
		return false;
	}

	//Ensure the clip set should be streamed in.
	if(!m_BulletReactionClipSetRequestHelper.ShouldClipSetBeStreamedIn())
	{
		return false;
	}

	//Ensure the clip set is streamed in.
	if(!m_BulletReactionClipSetRequestHelper.IsClipSetStreamedIn())
	{
		return false;
	}
	
	//Grab the ped.
	CPed* pPed = GetPed();

	//Ensure bullet reactions are not disabled.
	if(pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_DisableBulletReactions))
	{
		return false;
	}

	if(pPed->GetPedIntelligence()->GetTimeUntilNextBulletReaction() > 0.0f)
	{
		return false;
	}

	if(pPed->HasHurtStarted())
	{
		return false;
	}

	// Make sure we were told to play the bullet reaction
	if(!m_iFlags.IsFlagSet((u32)GF_PlayBulletReaction))
	{
		return false;
	}

	// Make sure we have an aim gun on foot task
	CTask* pSubTask = GetSubTask();
	if(!pSubTask || pSubTask->GetTaskType() != CTaskTypes::TASK_AIM_GUN_ON_FOOT/* || pSubTask->GetState() != CTaskAimGunOnFoot::State_Aiming*/)
	{
		return false;
	}
	
	// Require that we have a gun
	if(!pPed->GetWeaponManager()->GetIsArmedGun())
	{
		return false;
	}

	// Check if the combat task is playing an animation. It looks bad interrupting the ambient combat anims
	CTaskCombat* pCombatTask = static_cast<CTaskCombat*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COMBAT));
	if(pCombatTask && pCombatTask->GetIsPlayingAmbientClip())
	{
		return false;
	}

	return true;
}

bool CTaskGun::CanReload() const
{
	//If has started then don't reload weapon
	if(GetPed()->HasHurtStarted())
	{
		return false;
	}

	//If we're running the swimming task, return false
	if (GetPed()->GetPedIntelligence()->GetMotionTaskActiveSimplest()->GetTaskType() == CTaskTypes::TASK_MOTION_SWIMMING)
	{
		return false;
	}

	// Cannot reload while throwing grenade
	if(GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_ThrowingGrenadeWhileAiming))
	{
		return false;
	}

	//For the following aiming tasks, we don't want to reload in this task -- leave it to them.
	//They may have custom aiming/reloading animations that do not match up with this task.
	
	//Check the aim task type.
	switch(m_aimTaskType)
	{
		case CTaskTypes::TASK_AIM_GUN_SCRIPTED:
		{
			return false;
		}
		case CTaskTypes::TASK_AIM_GUN_VEHICLE_DRIVE_BY:
		{
			if(GetPed()->GetMyMount()) 
			{
				return true;
			}
			else
			{
				return false;
			}
		}
		default:
		{
			return true;
		}
	}
}

void CTaskGun::ProcessFacialIdleClip()
{
	//Ensure the facial data is valid.
	CFacialDataComponent* pFacialData = GetPed()->GetFacialData();
	if(!pFacialData)
	{
		return;
	}

	const CPed* pPed = GetPed();

	//! In 1st person, don't consider us aiming unless we are not in relax pose.
#if FPS_MODE_SUPPORTED	
	if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true))
	{
		if(pPed->GetMotionData()->GetFPSState() == CPedMotionData::FPS_IDLE)
		{
			return;
		}
	}
#endif
	
	//Check if we are aiming.
	if (m_iFlags & GF_BlindFiring)
	{
		if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_AIM_GUN_BLIND_FIRE)
		{
			if (GetSubTask()->GetState() == CTaskAimGunBlindFire::State_FireNew)
				pFacialData->RequestFacialIdleClip(CFacialDataComponent::FICT_Angry);
		}
	}
	else if(GetState() == State_Aim) 
	{
		if(!GetSubTask() || GetSubTask()->GetTaskType() != CTaskTypes::TASK_AIM_GUN_ON_FOOT || static_cast<const CTaskAimGunOnFoot*>(GetSubTask())->GetState() != CTaskAimGunOnFoot::State_Blocked)
		{
			const CWeaponInfo* pWeaponInfo = CTaskMotionPed::GetWeaponInfo(pPed);
			bool bUseAlternativeWhenBlock = pPed->GetPedResetFlag(CPED_RESET_FLAG_UseAlternativeWhenBlock);
			bool bIsScopedWeapon = false;
			const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
			if(pWeapon && pWeapon->GetScopeComponent())
			{
				bIsScopedWeapon = true;
			}
			//Request the aiming facial idle clip.
			// pWeaponInfo could be NULL during weapon holster.
			if (!pWeaponInfo || pWeaponInfo->GetIsUnarmed()) //the bird
			{
				pFacialData->RequestFacialIdleClip(CFacialDataComponent::FICT_Angry);
			}
			else if(!bUseAlternativeWhenBlock && !bIsScopedWeapon && !pWeaponInfo->GetAimingDownTheBarrel(*pPed))
			{
				// For low aiming weapons
				pFacialData->RequestFacialIdleClip(CFacialDataComponent::FICT_Angry);
			}
			else
			{
				pFacialData->RequestFacialIdleClip(CFacialDataComponent::FICT_Aiming);
			}
		}
	}
	else if(GetState() != State_Reload) // Reload task handles facial
	{
		//Request the angry facial idle clip.
		pFacialData->RequestFacialIdleClip(CFacialDataComponent::FICT_Angry);
	}
}

#if !__NO_OUTPUT
rage::atString CTaskGun::GetName() const
{
	const char* pStr = TASKCLASSINFOMGR.GetTaskName(GetTaskType());
	if (pStr)
	{
		atString name(pStr);
		name += ":";
		name += (GetStateName(GetState()));

		if (GetIsAiming())
		{
			char vPos[64] = "";
			Vector3 vTargetPos; 
			if (m_target.GetIsValid() && m_target.GetPosition(vTargetPos))
			{
				formatf(vPos, ":TargetPosition::%.2f %.2f %.2f", vTargetPos.x, vTargetPos.y, vTargetPos.z);
				name += vPos;
			}
		}
	
		return name;
	}
	else
	{
		return atString("Unnamed_task");
	}
}
#endif

CTaskGun::FSM_Return CTaskGun::StateAimOnExit(CPed* REPLAY_ONLY(pPed))
{
	m_uLastIsAimingTimeForOutroDelayCached = m_uLastIsAimingTimeForOutroDelay;

	m_iFlags.ClearFlag(GF_SkipIdleTransitions);		// Clear the skip idle flag
	m_iFlags.ClearFlag(GF_FireAtLeastOnce);			// Clear the fire at least once flag, as this should have happened

#if GTA_REPLAY
	//clear replay AI task flags	
	if(pPed && CReplayMgr::IsReplayInControlOfWorld() == false)
	{
		pPed->GetReplayRelevantAITaskInfo().ClearAITaskRunning(REPLAY_TASK_AIM_GUN_ON_FOOT);
	}
#endif
	
	return FSM_Continue;
}

CTaskGun::FSM_Return CTaskGun::StateReloadOnEnter(CPed* pPed)
{
	if (pPed->IsLocalPlayer())
	{
		CTask* pTask = pPed->GetPedIntelligence()->FindTaskActiveByTreeAndType(PED_TASK_TREE_SECONDARY, CTaskTypes::TASK_RELOAD_GUN);
		if (pTask)
		{
			pPed->GetPedIntelligence()->ClearSecondaryTask(PED_TASK_SECONDARY_PARTIAL_ANIM);
		}
	}
	const CWeaponInfo* pWeaponInfo = pPed->GetWeaponManager()->GetEquippedWeaponInfo();
	if (pWeaponInfo)
	{
		const bool bFromIdle = (m_iFlags & GF_ReloadFromIdle) != 0;
		SetNewTask(rage_new CTaskReloadGun(m_weaponControllerType, bFromIdle ? RELOAD_IDLE : 0));
	}

	m_bReloadIsBlocked = false;
	m_vPedPosWhenReloadIsBlocked = Vec3V(V_ZERO);
	m_vPedHeadingWhenReloadIsBlocked = 0.0f;

	return FSM_Continue;
}

CTaskGun::FSM_Return CTaskGun::StateReloadOnUpdate(CPed* pPed)
{
	//Block animated and Ik weapon reactions.
	pPed->SetPedResetFlag(CPED_RESET_FLAG_BlockAnimatedWeaponReactions, true);
	pPed->SetPedResetFlag(CPED_RESET_FLAG_BlockIkWeaponReactions, true);
	
	if(pPed->GetMotionData()->GetIsStrafing())
	{
		ProcessTorsoIk(*GetPed(), m_iFlags, m_target, GetClipHelper());
	}

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		const CTaskMotionBase* pMotionTask = pPed->GetCurrentMotionTask(false);
		if(pMotionTask && pMotionTask->GetTaskType() == CTaskTypes::TASK_MOTION_AIMING)
		{
			// We will be aiming after reloading, so skip idle to aim transition
			bool bSkipIdleTransitions = true;
#if FPS_MODE_SUPPORTED
			if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && m_bWantsToRoll)
			{
				bSkipIdleTransitions = false;
			}
#endif
			if(bSkipIdleTransitions)
			{
				m_iFlags.SetFlag(GF_SkipIdleTransitions);
			}
		}

		// Clear the reload from idle flag after reload
		m_iFlags.ClearFlag(GF_ReloadFromIdle);

		// If player ped is swimming and no longer aiming, quit out so we don't flick back to aiming or keep shooting if fire button is held down
		if (IsPlayerSwimmingAndNotAiming(pPed))
		{
			return FSM_Quit;
		}

#if FPS_MODE_SUPPORTED
		if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true) && m_bReloadIsBlocked)
		{
			SetState(State_Aim);
			return FSM_Continue;
		}
#endif // FPS_MODE_SUPPORTED

		//Always go back into the aim gun task if assisted aiming
		if (m_bShouldUseAssistedAimingCamera)
		{
			SetState(State_Aim);
			return FSM_Continue;
		}
		else
		{
			// Set state to decide what to do next
			SetState(State_Decide);
		}
	}
	else if(HasDurationExpired())
	{
		taskDebugf1("CTaskGun::StateReloadOnUpdate: FSM_Quit - HasDurationExpired. Previous State: %d", GetPreviousState());
		return FSM_Quit;
	}

	if(GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_RELOAD_GUN)
	{
		WeaponControllerState controllerState = GetWeaponControllerState(pPed);
		if(controllerState == WCS_CombatRoll)
		{
			m_bWantsToRoll = true;
		}

		if(m_bWantsToRoll)
		{
			CTaskReloadGun* pReloadTask = static_cast<CTaskReloadGun*>(GetSubTask());
			pReloadTask->RequestInterrupt();
		}
	}

	//If we're running the swimming task, request interrupt
	if (pPed->GetPedIntelligence()->GetMotionTaskActiveSimplest()->GetTaskType() == CTaskTypes::TASK_MOTION_SWIMMING)
	{
		CTaskReloadGun* pReloadTask = static_cast<CTaskReloadGun*>(GetSubTask());
		pReloadTask->RequestInterrupt();
		
		//Set reload interrupted flag. Triggers timer in CPedEquippedWeapon::Process() to automatically reload the weapon after the specified time.
		CPedEquippedWeapon* pPedEquippedWeapon = pPed->GetWeaponManager()->GetPedEquippedWeapon();
		if (pPedEquippedWeapon)
		{
			pPedEquippedWeapon->SetIsReloadInterrupted(true);
		}
	}

	return FSM_Continue;
}

CTaskGun::FSM_Return CTaskGun::StateReloadOnExit(CPed* pPed)
{
	if(pPed && pPed->GetMotionData()->GetIsStrafing())
	{
		// Don't force aim if the weapon has a scope
		const bool bWeaponHasFirstPersonScope = pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetEquippedWeaponHasFirstPersonScope();
		if(!bWeaponHasFirstPersonScope)
		{
			m_iFlags.SetFlag(GF_ForceAimOnce);
		}
	}
	m_bWantsToReload = false;
	return FSM_Continue;
}

CTaskGun::FSM_Return CTaskGun::StateSwapOnEnter(CPed* pPed)
{
	s32 iFlags = SWAP_HOLSTER | SWAP_DRAW | SWAP_TO_AIM;

	if (pPed->IsLocalPlayer())
	{
		aiTask* pTask = pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PLAYER_ON_FOOT);
		if (pTask)
		{
			if (static_cast<CTaskPlayerOnFoot*>(pTask)->GetShouldStartGunTask())
			{
				iFlags |= SWAP_INSTANTLY;
				static_cast<CTaskPlayerOnFoot*>(pTask)->SetShouldStartGunTask(false);
			}
		}
	}

	const CWeaponInfo* pWeaponInfo = GetPedWeaponInfo();
	if(pWeaponInfo && !pWeaponInfo->GetIsCarriedInHand())
	{
		if(GetWeaponControllerState(pPed) == WCS_None)
		{
			// Holster
			iFlags = SWAP_HOLSTER;
		}
	}

	const CWeaponInfo* pWeaponObjectInfo = pPed->GetCurrentWeaponInfoForHeldObject();
	if(!pWeaponObjectInfo || pWeaponObjectInfo->GetDelayedFiringAfterAutoSwapPreviousWeapon())
	{
		m_iGFFlags.SetFlag(GF_NotAllowToClearFireReleaseFlag);
	}

	SetNewTask(rage_new CTaskSwapWeapon(iFlags));
	return FSM_Continue;
}

CTaskGun::FSM_Return CTaskGun::StateSwapOnUpdate(CPed* pPed)
{
#if FPS_MODE_SUPPORTED
	const bool bInFpsMode = pPed->IsFirstPersonShooterModeEnabledForPlayer(false);
	if(bInFpsMode)
		GetPed()->SetIsStrafing(true);
#endif // FPS_MODE_SUPPORTED

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
#if FPS_MODE_SUPPORTED
		if(!bInFpsMode)
#endif // FPS_MODE_SUPPORTED
		{
			CTaskMotionAiming* pTask = static_cast<CTaskMotionAiming *>(pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_AIMING));
			if(pTask)
			{
				// We will be aiming after swapping, so skip idle to aim transition
				// Only if we are currently aiming
				m_iFlags.SetFlag(GF_SkipIdleTransitions);
			}
		}

		const CWeaponInfo* pWeaponInfo = GetPedWeaponInfo();
		if(!pWeaponInfo || (!pWeaponInfo->GetDelayedFiringAfterAutoSwap() && !m_iGFFlags.IsFlagSet(GF_NotAllowToClearFireReleaseFlag)))
		{
			m_iGFFlags.ClearFlag(GFF_RequireFireReleaseBeforeFiring);
		}
		m_iGFFlags.ClearFlag(GF_NotAllowToClearFireReleaseFlag);

		//Reset the GF_DisableBlockingClip depending on the new weapon
		m_iFlags.ClearFlag(GF_DisableBlockingClip);
		if(pWeaponInfo && pWeaponInfo->GetOnlyAllowFiring())
		{
			m_iFlags.SetFlag(GF_DisableBlockingClip);
		}

		m_uLastSwapTime = fwTimer::GetTimeInMilliseconds();

		// Set state to decide what to do next
		SetState(State_Decide);
	}

	return FSM_Continue;
}

CTaskGun::FSM_Return CTaskGun::StateBulletReactionOnEnter()
{
	CPed* pPed = GetPed();

	// If our network is active release the player
	if(m_MoveNetworkHelper.IsNetworkActive())
	{
		m_MoveNetworkHelper.ReleaseNetworkPlayer();
	}

	// when a clone comes into scope or a migration happens, its possible that the clip set might not be loaded....
	if(m_BulletReactionClipSetRequestHelper.IsClipSetStreamedIn())
	{
		// Just clear this for safety even though post fsm clears it
		m_iFlags.ClearFlag((u32)GF_PlayBulletReaction);	

		// Request the network definition
		taskAssertf(fwAnimDirector::RequestNetworkDef(CClipNetworkMoveInfo::ms_NetworkTaskGunBulletReactions), "Bullet reactions network is not streamed in!");

		// Create the network player and set it on the move ped plus set our clip set and send the reaction request
		m_MoveNetworkHelper.CreateNetworkPlayer(pPed, CClipNetworkMoveInfo::ms_NetworkTaskGunBulletReactions);
		pPed->GetMovePed().SetTaskNetwork(m_MoveNetworkHelper.GetNetworkPlayer(), NORMAL_BLEND_DURATION);

		fwMvClipSetId clipSetId = m_BulletReactionClipSetRequestHelper.GetClipSetId();
		m_MoveNetworkHelper.SetClipSet(clipSetId);
		m_MoveNetworkHelper.SetFlag(fwRandom::GetRandomTrueFalse(), ms_VariationId);
		m_MoveNetworkHelper.SendRequest(ms_React);

		if(!pPed->IsNetworkClone())
		{
			// Find out if we should be doing the overhead anim			

			bool bUseOverhead = false;
			if(fwTimer::GetTimeInMilliseconds() - ms_uLastTimeOverheadUsed > ms_Tunables.m_MinTimeBetweenOverheadBulletReactions)
			{
				Vec3V vHeadPosition = VECTOR3_TO_VEC3V(pPed->GetBonePositionCached(BONETAG_HEAD));
				Vec3V vTempEventPosition = m_vGunEventPosition;
				vTempEventPosition.SetZ(vTempEventPosition.GetZ() + ScalarVFromF32(ms_Tunables.m_fBulletReactionPosAdjustmentZ));
				if( IsGreaterThanAll(vTempEventPosition.GetZ(), vHeadPosition.GetZ()) &&
					IsLessThanAll(vTempEventPosition.GetZ(), vHeadPosition.GetZ() + ScalarVFromF32(ms_Tunables.m_fMaxAboveHeadForOverheadReactions)) )
				{
					vTempEventPosition.SetZ(V_ZERO);
					vHeadPosition.SetZ(V_ZERO);
					ScalarV scDistSq = DistSquared(vTempEventPosition, vHeadPosition);
					ScalarV scMaxDistSq = ScalarVFromF32(square(ms_Tunables.m_fMaxDistForOverheadReactions));
					if(IsLessThanAll(scDistSq, scMaxDistSq))
					{
						bUseOverhead = true;
						ms_uLastTimeOverheadUsed = fwTimer::GetTimeInMilliseconds();
					}
				}
			}

			m_MoveNetworkHelper.SetFlag(bUseOverhead, ms_ForceOverheadId);

			if(!bUseOverhead)
			{
				const Vec3V vPedPosition = pPed->GetTransform().GetPosition();

				// Get our heading to the event position (closest position of the bullet trajectory to our ped) and our current heading
				float fHeadingToEvent = fwAngle::GetRadianAngleBetweenPoints(m_vGunEventPosition.GetXf(), m_vGunEventPosition.GetYf(), vPedPosition.GetXf(), vPedPosition.GetYf());
				fHeadingToEvent = fwAngle::LimitRadianAngleSafe(fHeadingToEvent);
				const float fCurrentHeading = fwAngle::LimitRadianAngleSafe(pPed->GetCurrentHeading());

				// Get the delta between the two headings and clamp it
				m_fGunEventReactionDir = SubtractAngleShorter(fHeadingToEvent, fCurrentHeading);
				m_fGunEventReactionDir = Clamp(m_fGunEventReactionDir/PI, -1.0f, 1.0f);
				m_fGunEventReactionDir = ((-m_fGunEventReactionDir * 0.5f) + 0.5f);
			}
		}

		// finally convert the heading delta to a 0-1.0 value and set it as our direction
		m_MoveNetworkHelper.SetFloat(ms_DirectionId, m_fGunEventReactionDir);

		RequestProcessMoveSignalCalls();
		m_bMoveReactionFinished = false;
	}

	return FSM_Continue;
}

CTaskGun::FSM_Return CTaskGun::StateBulletReactionOnUpdate()
{
	// Once we've finished clear the task network and decide what to do next
	if(m_bMoveReactionFinished || !m_MoveNetworkHelper.IsNetworkActive() || GetTimeInState() > ms_Tunables.m_MaxTimeInBulletReactionState)
	{
		SetState(State_Decide);
	}

	GetPed()->SetIsStrafing(true);

	ProcessTorsoIk(*GetPed(), m_iFlags, m_target, GetClipHelper());

	return FSM_Continue;
}

CTaskGun::FSM_Return CTaskGun::StateBulletReactionOnExit(CPed* pPed)
{
	if(!pPed->IsNetworkClone())
	{
		pPed->GetPedIntelligence()->SetTimeUntilNextBulletReaction(fwRandom::GetRandomNumberInRange(ms_Tunables.m_fMinTimeBetweenBulletReactions, ms_Tunables.m_fMaxTimeBetweenBulletReactions));
	}
	
	//Clear the move network.
	pPed->GetMovePed().ClearTaskNetwork(m_MoveNetworkHelper, NORMAL_BLEND_DURATION);
	m_MoveNetworkHelper.ReleaseNetworkPlayer();

	return FSM_Continue;
}

bool CTaskGun::StateBulletReaction_OnProcessMoveSignals()
{
	if(m_MoveNetworkHelper.IsNetworkActive())
	{
		m_bMoveReactionFinished |= m_MoveNetworkHelper.GetBoolean(ms_ReactionFinished);
	}
	
	return (!m_bMoveReactionFinished);
}

CTaskGun::FSM_Return CTaskGun::StateAimOnUpdateClone(CPed* pPed)
{
    if(pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(m_aimTaskType))
    {
        if(GetSubTask() == 0 || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
        {
			if (GetSubTask() && GetSubTask()->GetTaskType() == m_aimTaskType)
			{
				taskDebugf3("CTaskGun::StateAimOnUpdateClone--Subtask has finished and is of same type as m_aimTaskType (%d). Wait.", m_aimTaskType);
				return FSM_Continue;
			}

			// if we've changed weapon, we're just going to get in an infinite loop of starting the task and it quitting immediately before it gets recreated here...
			if(!pPed->GetPedResetFlag(CPED_RESET_FLAG_EquippedWeaponChanged))
			{
				if (m_aimTaskType == CTaskTypes::TASK_AIM_GUN_BLIND_FIRE)
				{
					if (!GetPed()->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_IN_COVER))
					{
						taskDebugf3("CTaskGun::StateAimOnUpdateClone--TASK_AIM_GUN_BLIND_FIRE required and no cover task running. Wait.");
						return FSM_Continue;
					}
				}

				taskDebugf3("CTaskGun::StateAimOnUpdateClone--invoke CTaskAimGunFactory::CreateTask");

				CTaskAimGun* pAimGunTask = CTaskAimGunFactory::CreateTask(m_aimTaskType, m_weaponControllerType, m_iFlags, m_iGFFlags, m_target, m_ikInfo, &m_MoveNetworkHelper);

				if(pAimGunTask)
				{
					pAimGunTask->SetOverrideAimClipId(m_overrideAimClipSetId, m_overrideAimClipId);
					pAimGunTask->SetOverrideFireClipId(m_overrideFireClipSetId, m_overrideFireClipId);
					pAimGunTask->SetOverrideClipSetId(m_overrideClipSetId);
					pAimGunTask->SetScriptedGunTaskInfoHash(m_uScriptedGunTaskInfoHash);
					if(pAimGunTask->GetTaskType() == CTaskTypes::TASK_AIM_GUN_ON_FOOT)
					{
						static_cast<CTaskAimGunOnFoot*>(pAimGunTask)->SetForceCombatRoll(m_bWantsToRoll);
					}
					if(pAimGunTask->GetTaskType() == CTaskTypes::TASK_AIM_GUN_VEHICLE_DRIVE_BY)
					{
						static_cast<CTaskAimGunVehicleDriveBy*>(pAimGunTask)->SetShouldSkipIntro(m_bSkipDrivebyIntro);
						m_bSkipDrivebyIntro = false;
					}
					m_bWantsToRoll = false;
					SetNewTask(pAimGunTask);
				}
			}
        }
        else
        {
            if(GetSubTask()->GetTaskType() == m_aimTaskType)
            {
                // Cache the aim task
			    CTaskAimGun* pAimGunTask = static_cast<CTaskAimGun*>(GetSubTask());

			    //
			    // Update sub task target
			    //

			    pAimGunTask->SetTarget(m_target);
            }

            if(GetStateFromNetwork() == State_Reload)
            {
                GetSubTask()->MakeAbortable( ABORT_PRIORITY_IMMEDIATE, 0);
                SetState(GetStateFromNetwork());
            }
        }
    }
    else
    {
        if(GetStateFromNetwork() != State_Aim)
        {
            SetState(GetStateFromNetwork());
        }
    }

    return FSM_Continue;
}

CTaskGun::FSM_Return CTaskGun::StateReloadOnUpdateClone(CPed* UNUSED_PARAM(pPed))
{
    if(GetSubTask() == 0 || GetSubTask()->GetTaskType() != CTaskTypes::TASK_RELOAD_GUN)
	{
        if(GetStateFromNetwork() != State_Reload)
        {
		    // We will be aiming after reloading, so skip idle to aim transition
		    m_iFlags.SetFlag(GF_SkipIdleTransitions);

		    SetState(GetStateFromNetwork());
        }
	}

	return FSM_Continue;
}

CTaskGun::FSM_Return CTaskGun::StateSwapOnEnterClone(CPed* pPed)
{
	CreateSubTaskFromClone(pPed, CTaskTypes::TASK_SWAP_WEAPON, true);

	return FSM_Continue;
}

CTaskGun::FSM_Return CTaskGun::StateSwapOnUpdateClone(CPed* UNUSED_PARAM(pPed))
{
	if (!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(GetStateFromNetwork());
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

bool CTaskGun::AddGetUpSets( CNmBlendOutSetList& sets, CPed* pPed )
{
	if (m_target.GetIsValid() && pPed->GetWeaponManager())
	{
		return SetupAimingGetupSet(sets, pPed, m_target);
	}

	return false;
}

bool CTaskGun::SetupAimingGetupSet( CNmBlendOutSetList& sets, CPed* pPed, const CAITarget& target )
{
	TUNE_GROUP_INT(BLEND_FROM_NM, iTimeToBlockAimFromGround, 3000, 0, 10000, 1);
	TUNE_GROUP_INT(BLEND_FROM_NM, iMaxSimultaneousAimingFromGroundPeds, 1, 0, 100, 1);

	bool bArmed1Handed = false;
	bool bArmed2Handed = false;
	if (pPed->GetWeaponManager())
	{
		CPedWeaponManager* pWeaponManager = pPed->GetWeaponManager();

		if (pWeaponManager->GetEquippedWeaponHash()==WEAPONTYPE_UNARMED)
			pWeaponManager->EquipBestWeapon(true, true);
		else
			pWeaponManager->EquipWeapon(pWeaponManager->GetWeaponSelector()->GetSelectedWeapon(), pWeaponManager->GetWeaponSelector()->GetSelectedVehicleWeapon(),true, true);

		const CWeaponInfo* pEquippedWeaponInfo = pWeaponManager->GetEquippedWeaponInfo();
		if (!pEquippedWeaponInfo || pEquippedWeaponInfo->GetIsMelee())
		{
			return CTaskMelee::SetupMeleeGetup(sets, pPed, target.GetEntity());
		}

		// reload the weapon
		if (pPed->GetWeaponManager()->GetEquippedWeapon())
		{
			pPed->GetWeaponManager()->GetEquippedWeapon()->DoReload(false);
		}

		bArmed1Handed = pPed->GetWeaponManager()->GetIsArmed1Handed() || ms_Tunables.m_bDisable2HandedGetups;
		bArmed2Handed = pPed->GetWeaponManager()->GetIsArmed2Handed();	// Really, what if we got a 0 handed weapon! Lazor eyes you know!
	}

	// create a movement task to set the desired heading to the target entities position throughout the getup
	sets.SetMoveTask(rage_new CTaskMoveFaceTarget(target));
	if (target.GetIsValid())
	{
		// make sure we start turning to face the character as soon as possible
		Vector3 targetPos(VEC3_ZERO);
		target.GetPosition(targetPos);
		pPed->SetDesiredHeading(targetPos); // I assume this happening further down! Don't just remove please / Traffe

		s32 pelvisBoneIndex = -1;
		pPed->GetSkeletonData().ConvertBoneIdToIndex(BONETAG_PELVIS, pelvisBoneIndex);
		Mat34V pelvisMtx;
		Mat34V rootMtx;
		pPed->GetGlobalMtx(pelvisBoneIndex, RC_MATRIX34(pelvisMtx));
		pPed->GetGlobalMtx(BONETAG_ROOT, RC_MATRIX34(rootMtx));

		//copy the rotation from the root for now
		pelvisMtx.Set3x3(rootMtx);
	
		// add the appropriate standing aiming set based on the angle from the pelvis bone to the target position
		Mat34V relativeTarget;
		relativeTarget.Setd(RC_VEC3V(targetPos));

		UnTransformOrtho(relativeTarget, pelvisMtx, relativeTarget);
		TUNE_GROUP_FLOAT(BLEND_FROM_NM, fPelvisHeadingAdjustForStandingCombatBlendOut, 0.0f, -PI, PI, 0.01f);
		TUNE_GROUP_FLOAT(BLEND_FROM_NM, fPelvisHeadingRangeForStandingCombatBlendOut, 1.30f, 0.0f, PI, 0.01f);
		float fAngleToTarget = fwAngle::GetRadianAngleBetweenPoints(relativeTarget.d().GetXf(), relativeTarget.d().GetYf(), 0.0f, 1.0f)+fPelvisHeadingAdjustForStandingCombatBlendOut;
		if (fAngleToTarget>fPelvisHeadingRangeForStandingCombatBlendOut)
		{
			if (pPed->ShouldDoHurtTransition() && !bArmed2Handed)
				sets.AddWithFallback(NMBS_STANDING_AIMING_RIGHT_PISTOL_HURT, NMBS_STANDING);
			else if (pPed->ShouldDoHurtTransition())
				sets.AddWithFallback(NMBS_STANDING_AIMING_RIGHT_RIFLE_HURT, NMBS_STANDING);
			else if (bArmed1Handed)
				sets.AddWithFallback(NMBS_STANDING_AIMING_RIGHT, NMBS_STANDING);
			else
				sets.AddWithFallback(NMBS_STANDING_AIMING_RIGHT_RIFLE, NMBS_STANDING);
		}
		else if (fAngleToTarget<-fPelvisHeadingRangeForStandingCombatBlendOut)
		{
			if (pPed->ShouldDoHurtTransition() && !bArmed2Handed)
				sets.AddWithFallback(NMBS_STANDING_AIMING_LEFT_PISTOL_HURT, NMBS_STANDING);
			else if (pPed->ShouldDoHurtTransition())
				sets.AddWithFallback(NMBS_STANDING_AIMING_LEFT_RIFLE_HURT, NMBS_STANDING);
			else if (bArmed1Handed)
				sets.AddWithFallback(NMBS_STANDING_AIMING_LEFT, NMBS_STANDING);
			else
				sets.AddWithFallback(NMBS_STANDING_AIMING_LEFT_RIFLE, NMBS_STANDING);
		}
		else
		{
			if (pPed->ShouldDoHurtTransition() && !bArmed2Handed)
				sets.AddWithFallback(NMBS_STANDING_AIMING_CENTRE_PISTOL_HURT, NMBS_STANDING);
			else if (pPed->ShouldDoHurtTransition())
				sets.AddWithFallback(NMBS_STANDING_AIMING_CENTRE_RIFLE_HURT, NMBS_STANDING);
			else if (bArmed1Handed)
				sets.AddWithFallback(NMBS_STANDING_AIMING_CENTRE, NMBS_STANDING);
			else
				sets.AddWithFallback(NMBS_STANDING_AIMING_CENTRE_RIFLE, NMBS_STANDING);
		}
	}
	else
	{
		sets.Add(NMBS_STANDING);
	}

	if (pPed->ShouldDoHurtTransition())
	{
		sets.AddWithFallback(NMBS_WRITHE, pPed->IsMale() ? NMBS_STANDARD_GETUPS : NMBS_STANDARD_GETUPS_FEMALE);
		if (target.GetIsValid())
			sets.SetTarget(target);
	}
	// do we have ammo in our clip? if so use the fire from ground group
	else if ((fwTimer::GetTimeInMilliseconds() - CTaskGetUp::ms_LastAimFromGroundStartTime) > iTimeToBlockAimFromGround
		&& CTaskGetUp::ms_NumPedsAimingFromGround < iMaxSimultaneousAimingFromGroundPeds
		&& pPed->GetWeaponManager()	&& pPed->GetWeaponManager()->GetEquippedWeapon() 
		&& !pPed->GetWeaponManager()->GetEquippedWeapon()->GetNeedsToReload(true)
		&& target.GetIsValid()
		&& !pPed->IsPlayer()
		&& !NetworkInterface::IsGameInProgress())
	{
		bool bEarlyOutShootFromGround = false;
		CNmBlendOutSet* poseSet = CNmBlendOutSetManager::GetBlendOutSet(bArmed1Handed ? NMBS_ARMED_1HANDED_SHOOT_FROM_GROUND : NMBS_ARMED_2HANDED_SHOOT_FROM_GROUND);
		if (poseSet)
		{
			for (s32 i=0; i<poseSet->m_items.GetCount(); i++) // run over the pose set and request the anim set for each entry
			{
				if (poseSet->m_items[i]->m_type==NMBT_AIMFROMGROUND)
				{
					const CScriptedGunTaskInfo* pScriptedGunTaskInfo = CScriptedGunTaskMetadataMgr::GetScriptedGunTaskInfoByHash(poseSet->m_items[i]->GetClipSet().GetHash());
					if (pScriptedGunTaskInfo)
					{
						float fAngleToTarget = fwAngle::LimitRadianAngle(pPed->GetDesiredHeading() - pPed->GetCurrentHeading()) * RtoD;

						if (pScriptedGunTaskInfo->GetMaxAimSweepHeadingAngleDegs() < fAngleToTarget ||
							pScriptedGunTaskInfo->GetMinAimSweepHeadingAngleDegs() > fAngleToTarget)
						{
							bEarlyOutShootFromGround = true;
						}
					}
				}
			}
		}

		if (bEarlyOutShootFromGround)
		{
			sets.AddWithFallback( bArmed1Handed ? NMBS_ARMED_1HANDED_GETUPS : NMBS_ARMED_2HANDED_GETUPS, pPed->IsMale() ? NMBS_STANDARD_GETUPS : NMBS_STANDARD_GETUPS_FEMALE);
			sets.SetTarget(target);
		}
		else
		{
			sets.AddWithFallback( bArmed1Handed ? NMBS_ARMED_1HANDED_SHOOT_FROM_GROUND : NMBS_ARMED_2HANDED_SHOOT_FROM_GROUND, pPed->IsMale() ? NMBS_STANDARD_GETUPS : NMBS_STANDARD_GETUPS_FEMALE);
			sets.SetTarget(target);
		}
	}
	else
	{
		sets.AddWithFallback( bArmed1Handed ? NMBS_ARMED_1HANDED_GETUPS : NMBS_ARMED_2HANDED_GETUPS, pPed->IsMale() ? NMBS_STANDARD_GETUPS : NMBS_STANDARD_GETUPS_FEMALE);
		sets.SetTarget(target);
	}

	return true;
}
//////////////////////////////////////////////////////////////////////////

void CTaskGun::GetUpComplete(eNmBlendOutSet setMatched, CNmBlendOutItem* UNUSED_PARAM(pLastItem), CTaskMove* UNUSED_PARAM(pMoveTask), CPed* pPed)
{
	if (
		setMatched==NMBS_ARMED_1HANDED_GETUPS
		|| setMatched== NMBS_ARMED_2HANDED_GETUPS
		|| setMatched==NMBS_ARMED_1HANDED_SHOOT_FROM_GROUND
		|| setMatched==NMBS_ARMED_2HANDED_SHOOT_FROM_GROUND
		|| setMatched==NMBS_STANDING_AIMING_CENTRE
		|| setMatched==NMBS_STANDING_AIMING_LEFT
		|| setMatched==NMBS_STANDING_AIMING_RIGHT
		|| setMatched==NMBS_STANDING_AIMING_CENTRE_PISTOL_HURT
		|| setMatched==NMBS_STANDING_AIMING_LEFT_PISTOL_HURT
		|| setMatched==NMBS_STANDING_AIMING_RIGHT_PISTOL_HURT
		|| setMatched==NMBS_STANDING_AIMING_CENTRE_RIFLE_HURT
		|| setMatched==NMBS_STANDING_AIMING_LEFT_RIFLE_HURT
		|| setMatched==NMBS_STANDING_AIMING_RIGHT_RIFLE_HURT
		|| setMatched==NMBS_STANDING_AIMING_CENTRE_RIFLE
		|| setMatched==NMBS_STANDING_AIMING_LEFT_RIFLE
		|| setMatched==NMBS_STANDING_AIMING_RIGHT_RIFLE
		|| setMatched==NMBS_WRITHE
		)
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_InstantBlendToAim, true);
		pPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAIUpdate, true); //need a post camera update to process the start state of the next task
		pPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true); //need a post camera update to process the start state of the next task
		pPed->SetPedResetFlag(CPED_RESET_FLAG_ePostCameraAnimUpdateUseZeroTimestep, true);
	}
}

//////////////////////////////////////////////////////////////////////////

void CTaskGun::RequestGetupAssets(CPed* )
{
	m_GetupReqHelpers[0].Request(NMBS_ARMED_1HANDED_GETUPS);
	m_GetupReqHelpers[1].Request(NMBS_ARMED_1HANDED_SHOOT_FROM_GROUND);
	m_GetupReqHelpers[2].Request(NMBS_ARMED_2HANDED_GETUPS);
	m_GetupReqHelpers[3].Request(NMBS_ARMED_2HANDED_SHOOT_FROM_GROUND);
	m_GetupReqHelpers[4].Request(NMBS_STANDING_AIMING_CENTRE);
	m_GetupReqHelpers[5].Request(NMBS_STANDING_AIMING_CENTRE_RIFLE);
	m_GetupReqHelpers[6].Request(NMBS_WRITHE);
	m_GetupReqHelpers[7].Request(NMBS_STANDING_AIMING_CENTRE_PISTOL_HURT);
	m_GetupReqHelpers[8].Request(NMBS_STANDING_AIMING_CENTRE_RIFLE_HURT);
}

void CTaskGun::ProcessLookAt(CPed* pPed)
{
	float fTimeStep = GetTimeStep();
	m_fTimeTillNextLookAt -= fTimeStep;

	// If this ped is not a player ped and is moving randomly look in the direction it is moving in
	if(!pPed->IsAPlayerPed() && 
		m_fTimeTillNextLookAt <= 0.0f && 
		!CPedMotionData::GetIsStill(pPed->GetPedIntelligence()->GetMoveBlendRatioFromGoToTask()))
	{
		static dev_float MIN_MOVE_SPEED_SQ = 0.1f;
		if(pPed->GetVelocity().XYMag() > MIN_MOVE_SPEED_SQ)
		{
			// Generate a position to look at in the direction the ped is moving
			Vector3 vLookatPos = pPed->GetVelocity();
			vLookatPos.Normalize();
			vLookatPos.Scale(10.0f);
			vLookatPos += VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

			static dev_s32 BLEND_IN_TIME  = 200;
			static dev_s32 BLEND_OUT_TIME = 200;

			s32 iLookTime = fwRandom::GetRandomNumberInRange(ms_Tunables.m_iMinLookAtTime, ms_Tunables.m_iMaxLookAtTime);
			pPed->GetIkManager().LookAt(0, NULL, iLookTime, BONETAG_INVALID, &vLookatPos, 0, BLEND_IN_TIME, BLEND_OUT_TIME);

			// Reset the timer
			m_fTimeTillNextLookAt = fwRandom::GetRandomNumberInRange(ms_Tunables.m_fMinTimeBetweenLookAt, ms_Tunables.m_fMaxTimeBetweenLookAt);
		}
	}
}

void CTaskGun::ProcessEyeIk()
{
	//Increase the time since the last eye ik was processed.
	float fTimeStep = GetTimeStep();
	m_fTimeSinceLastEyeIkWasProcessed += fTimeStep;

	//Ensure we should process eye ik.
	if(!ShouldProcessEyeIk())
	{
		return;
	}

	//Grab the ped.
	CPed* pPed = GetPed();

	//Get the target position.
	Vector3 vTargetPosition;
	if(!taskVerifyf(m_target.GetPosition(vTargetPosition), "Unable to get the target position."))
	{
		return;
	}

	//Check if the target has an entity.
	const CEntity* pTargetEntity = m_target.GetEntity();
	if(pTargetEntity)
	{
		//Transform the target position to object space.
		vTargetPosition = VEC3V_TO_VECTOR3(pTargetEntity->GetTransform().UnTransform(VECTOR3_TO_VEC3V(vTargetPosition)));
	}

	//Look at the target (eyes only).
	float fTime = ms_Tunables.m_TimeForEyeIk;
	pPed->GetIkManager().LookAt(0, pTargetEntity, (s32)(fTime * 1000.0f), BONETAG_INVALID, &vTargetPosition, LF_USE_EYES_ONLY, 500, 500, CIkManager::IK_LOOKAT_HIGH);

	//Clear the time since the eye ik was last processed.
	m_fTimeSinceLastEyeIkWasProcessed = 0.0f;
}

bool CTaskGun::ShouldProcessEyeIk() const
{
	//Grab the ped.
	const CPed* pPed = GetPed();

	//Ensure the ped is not a player.
	if(pPed->IsPlayer())
	{
		return false;
	}

	//Ensure we are aiming.
	if(GetState() != State_Aim)
	{
		return false;
	}

	//Ensure the target is valid.
	if(!m_target.GetIsValid())
	{
		return false;
	}

	//Ensure the time has expired.
	float fMinTimeBetweenEyeIkProcesses = ms_Tunables.m_MinTimeBetweenEyeIkProcesses;
	if(m_fTimeSinceLastEyeIkWasProcessed < fMinTimeBetweenEyeIkProcesses)
	{
		return false;
	}

	return true;
}

bool CTaskGun::HasValidWeapon(CPed* UNUSED_PARAM(pPed)) const
{
	const CWeaponInfo* pWeaponInfo = GetPedWeaponInfo();
	if(!pWeaponInfo)
	{
		return false;
	}

#if FPS_MODE_SUPPORTED
	// Quit if equipped weapon changes to projectile
	bool bWantToSwapWeapon = GetPed()->GetWeaponManager() && GetPed()->GetWeaponManager()->GetRequiresWeaponSwitch();
	if(pWeaponInfo->GetIsProjectile() && pWeaponInfo->GetIsThrownWeapon() && !GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_ThrowingGrenadeWhileAiming) && !bWantToSwapWeapon)
	{
		return false;
	}
#endif // FPS_MODE_SUPPORTED

	if (GetState() != State_Swap )
	{
#if FPS_MODE_SUPPORTED
		const bool bFpsDontCheckStrafing = GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false) && ((GetPed()->GetWeaponManager() && GetPed()->GetWeaponManager()->GetRequiresWeaponSwitch() && !m_iFlags.IsFlagSet(GF_PreventWeaponSwapping)) || GetPed()->GetPedResetFlag(CPED_RESET_FLAG_WeaponBlockedInFPSMode));
#endif // FPS_MODE_SUPPORTED
		if(!pWeaponInfo->GetIsGunOrCanBeFiredLikeGun() && !pWeaponInfo->GetCanBeAimedLikeGunWithoutFiring() && m_aimTaskType != CTaskTypes::TASK_AIM_GUN_VEHICLE_DRIVE_BY && !GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_ThrowingGrenadeWhileAiming) 
#if FPS_MODE_SUPPORTED
			&& !GetPed()->IsFirstPersonShooterModeEnabledForPlayer(!bFpsDontCheckStrafing) 
#endif // FPS_MODE_SUPPORTED
			)
		{
			return false;
		}
	}

	return true;
}

void CTaskGun::UpdateTarget(CPed* pPed)
{
	if( (m_iFlags.IsFlagSet(GF_UpdateTarget) || pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON)) && pPed && pPed->IsPlayer())
	{
		weaponAssert(pPed->GetWeaponManager());
		if(pPed->IsNetworkClone())
		{
			if (static_cast<CNetObjPlayer*>(pPed->GetNetworkObject())->HasValidTarget())
			{
				m_target.SetPosition(pPed->GetWeaponManager()->GetWeaponAimPosition());
			}
		}
		else
		{
			m_target.Update(pPed);

			Vector3 vTargetPosition;
			if(m_target.GetIsValid() && m_target.GetPositionWithFiringOffsets(pPed, vTargetPosition))
			{
				pPed->GetWeaponManager()->SetWeaponAimPosition(vTargetPosition);
			}
		}
	}
}

//----------------------------------------------------
// CClonedGunInfo
//----------------------------------------------------

CClonedGunInfo::CClonedGunInfo()
: m_GunFlags(0)
, m_AimTaskType(0)
, m_uScriptedGunTaskInfoHash(0)
, m_BulletReactionDir(0.0f)
{

}

CClonedGunInfo::CClonedGunInfo(s32 gunState, const CWeaponTarget* pTarget, u32 gunFlags, s32 aimTaskType, u32 uScriptedGunTaskInfoHash, float gunEventDir, bool reloadingOnEmpty)
: m_GunFlags(GetPackedGunFlags(gunFlags))
, m_AimTaskType(GetPackedAimTaskType(aimTaskType))
, m_uScriptedGunTaskInfoHash(uScriptedGunTaskInfoHash)
, m_BulletReactionDir(gunEventDir)
, m_weaponTargetHelper(pTarget)
, m_ReloadingOnEmpty(reloadingOnEmpty)
{
    SetStatusFromMainTaskState(gunState);
}

CTaskFSMClone *CClonedGunInfo::CreateCloneFSMTask()
{
	CTaskTypes::eTaskType taskType = static_cast<CTaskTypes::eTaskType>(GetUnPackedAimTaskType(m_AimTaskType)); 
	
	// extract target
	CAITarget target; 
	m_weaponTargetHelper.UpdateTarget(target);

	CTaskGun* gunTask = rage_new CTaskGun(CWeaponController::WCT_Aim, taskType, target);
	if(gunTask)
	{
		gunTask->SetScriptedGunTaskInfoHash(m_uScriptedGunTaskInfoHash);
		gunTask->GetGunFlags() = GetUnPackedGunFlags(m_GunFlags);

		if(gunTask)
		{
			if(m_weaponTargetHelper.HasTargetEntity())
			{
				gunTask->SetTargetOutOfScopeID(m_weaponTargetHelper.GetTargetEntityId());
			}
		}
	}

	return gunTask;
}

CTask *CClonedGunInfo::CreateLocalTask(fwEntity *UNUSED_PARAM(pEntity))
{
	return CreateCloneFSMTask();
}

bool CTaskGun::IsInScope(const CPed* pPed)
{
	Assert(pPed->IsNetworkClone());

	// Players' don't send over target data....
	if(!pPed->IsPlayer())
	{
		// Update the target...
		ObjectId targetId = GetTargetOutOfScopeID();
		CNetObjPed::UpdateNonPlayerCloneTaskTarget(pPed, m_target, targetId);
		SetTargetOutOfScopeID(targetId);

		// Still not valid, gun task shouldn't be running if we don't have a target...
		if(!m_target.GetIsValid())
		{
			return false;
		}
	}
	else
	{
		return CTaskFSMClone::IsInScope(pPed);
	}

	return true;
}

float CTaskGun::GetScopeDistance() const
{
	return static_cast<float>(CLONE_TASK_SCOPE_DISTANCE);
}

u32 CClonedGunInfo::GetPackedGunFlags(s32 unpackedFlags)
{
    u32 packedFlags = 0;

    if(unpackedFlags & GF_UpdateTarget)
    {
        packedFlags |= (1 << 0);
    }

	if(unpackedFlags & GF_DisableTorsoIk)
	{
		packedFlags |= (1 << 1);
	}

    return packedFlags;
}

s32 CClonedGunInfo::GetUnPackedGunFlags(u32 packedFlags)
{
    u32 unpackedFlags = 0;

    if(packedFlags & (1 << 0))
    {
        unpackedFlags |= GF_UpdateTarget;
    }

	if(packedFlags & (1 << 1))
	{
		unpackedFlags |= GF_DisableTorsoIk;
	}

    return unpackedFlags;
}

u32 CClonedGunInfo::GetPackedAimTaskType(s32 unpackedType)
{
    switch(unpackedType)
    {
	case CTaskTypes::TASK_AIM_GUN_ON_FOOT:
		return 0;
    case CTaskTypes::TASK_AIM_GUN_CLIMBING:
        return 1;
	case CTaskTypes::TASK_AIM_GUN_STRAFE:
		return 2;
	case CTaskTypes::TASK_AIM_GUN_BLIND_FIRE:
		return 3;
	case CTaskTypes::TASK_AIM_GUN_VEHICLE_DRIVE_BY:
		return 4;
	case CTaskTypes::TASK_AIM_GUN_SCRIPTED:
		return 5;
    default:
        taskErrorf("Invalid unpacked aim task type!");
        return 0;
    }
}

s32 CClonedGunInfo::GetUnPackedAimTaskType(u32 packedType)
{
    switch(packedType)
    {
	case 0:
		return CTaskTypes::TASK_AIM_GUN_ON_FOOT;
    case 1:
        return CTaskTypes::TASK_AIM_GUN_CLIMBING;
	case 2:
		return CTaskTypes::TASK_AIM_GUN_STRAFE;
	case 3:
		return CTaskTypes::TASK_AIM_GUN_BLIND_FIRE;
	case 4:
		return CTaskTypes::TASK_AIM_GUN_VEHICLE_DRIVE_BY;
	case 5:
		return CTaskTypes::TASK_AIM_GUN_SCRIPTED;
    default:
        taskErrorf("Invalid packed aim task type!");
        return CTaskTypes::TASK_AIM_GUN_ON_FOOT;
    }
}


//----------------------------------------------------
// CTaskAimSweep
//----------------------------------------------------

#define AIM_ANGLE_TOLERANCE .375f * PI

CTaskAimSweep::CTaskAimSweep(CTaskAimSweep::AimType aimType, bool bIgnoreGroup)
: m_aimType(aimType)
, m_vTargetPosition(VEC3_ZERO)
, m_vAheadPosition(VEC3_ZERO)
, m_fUpdateAimTimer(0.0f)
, m_bIgnoreGroup(bIgnoreGroup)
{
	SetInternalTaskType(CTaskTypes::TASK_AIM_SWEEP);
}

void CTaskAimSweep::UpdateAimTarget()
{
	CPed* pPed = GetPed();
	CPed* pLeader = CTaskSwat::GetGroupLeader(*pPed);
	if(pPed)
	{
		m_fUpdateAimTimer -= GetTimeStep();

		// For now we only prevent the leader from constantly updating (or a single ped not in a group)
		if((m_bIgnoreGroup || !pLeader || pLeader == pPed) && m_fUpdateAimTimer > 0.0f)
		{
			return;
		}

		// Need to get the nav mesh task, if we have one, as well as our position (we use it multiple times) and a zero vector for the aim target
		CTaskMoveFollowNavMesh* pNavMeshTask = (CTaskMoveFollowNavMesh*) pPed->GetPedIntelligence()->FindMovementTaskByType(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH);
		Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		Vector3 vAimTarget(VEC3_ZERO);

		// If we have a leader but we are not the leader use the leader's position as the base aim target (we'll adjust later)
		if(!m_bIgnoreGroup && pLeader && pLeader != pPed)
		{
			vAimTarget = VEC3V_TO_VECTOR3(pLeader->GetTransform().GetPosition());
		}
		else if(pNavMeshTask)
		{
			// If we have a nav mesh task we try to get the point ahead on the route
			if(!pNavMeshTask->GetPositionAheadOnRoute(pPed, 4.0f, vAimTarget))
			{
				// if we fail it could mean a few things, so if we have a look ahead position go ahead and use it
				// otherwise we'll use our forward facing direction to offset from our position
				if(m_vAheadPosition.IsNonZero())
				{
					vAimTarget = m_vAheadPosition;
				}
				else
				{
					vAimTarget = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) + (VEC3V_TO_VECTOR3(pPed->GetTransform().GetB()) * 2.5f);
				}
			}
		}
		else
		{
			// If we don't have a nav mesh task we'll check for a move go to point task
			CTaskMoveGoToPoint* pGoToPointTask = (CTaskMoveGoToPoint*) pPed->GetPedIntelligence()->FindMovementTaskByType(CTaskTypes::TASK_MOVE_GO_TO_POINT);
			if(pGoToPointTask)
			{
				// Set our aim target as the target for the task, if we're within a small distance of that target we'll aim at the look ahead position
				vAimTarget = pGoToPointTask->GetTarget();
				if(vAimTarget.Dist2(vPedPosition) < 1.0f)
				{
					vAimTarget = m_vAheadPosition;
				}
			}
			else if(pPed->GetPedIntelligence()->FindMovementTaskByType(CTaskTypes::TASK_MOVE_STAND_STILL) || pPed->GetMotionData()->GetIsStill())
			{
				// otherwise let's hope we are still or currently in a stand still task, in which case we'll use our ahead pos if it is non-zero
				if(m_vAheadPosition.IsNonZero())
				{
					vAimTarget = m_vAheadPosition;
				}
			}
		}

		// Only set our desired target if we found some sort of position to aim at
		if(vAimTarget.IsNonZero())
		{
			// I don't like this but if our current target is zero we need to start somewhere more relevant
			if(m_vTargetPosition.IsZero())
			{
				m_vTargetPosition = vAimTarget;
			}

			// Get the vector from our ped to the base target
			Vector3 vToTarget = vAimTarget - vPedPosition;

			// Get the vector from the ped to the current aim position
			Vector3 vToAimTarget = m_vTargetPosition - vPedPosition;

			vToTarget.Normalize();	
			vToAimTarget.Normalize();		

			// If this isn't our look ahead position and we're further than a small distance away, we want to calculate a new ahead position
			if(vAimTarget != m_vAheadPosition && vAimTarget.Dist2(vPedPosition) > 1.0f)
			{
				m_vAheadPosition = vAimTarget + (vToTarget * 2.0f);
			}

			// This is where we will calculate the desired calculated from the "base" target (vAimTarget)
			float fTestAngle = abs(vToTarget.FastAngleZ(vToAimTarget));
			switch(m_aimType)
			{
				case Aim_Forward:
					m_vTargetPosition = vAimTarget;
					break;
				case Aim_Varied:
				case Aim_Left:
				case Aim_Right:
					// Check our timer and also check that we are withing our angle tolerance as we might need to choose a new position to aim at
					if(m_fUpdateAimTimer <= 0.0f || fTestAngle > (AIM_ANGLE_TOLERANCE))
					{
						// Rotate the normalized to target vector left/right/varied depending on our aim type and make it slightly random
						if(m_aimType == Aim_Left || (m_aimType == Aim_Varied && fwRandom::GetRandomTrueFalse()))
						{
							vToTarget.RotateZ(fwRandom::GetRandomNumberInRange(.2f, .65f));
						}
						else
						{
							vToTarget.RotateZ(fwRandom::GetRandomNumberInRange(-.65f, -.2f));
						}

						// Our desired target will be a position "x" units away from us in the newly rotated direction
						m_vDesiredTarget = vPedPosition + (vToTarget * fwRandom::GetRandomNumberInRange(10.0f, 12.0f));
						m_fUpdateAimTimer = -1.0f;
					}

					// We need to lerp from our current target to our desired target every update
					m_vTargetPosition.Lerp(.1f, m_vDesiredTarget);
					break;
				default:
					break;
			}
		}

		// reset our timer if need be
		if(m_fUpdateAimTimer <= 0.0f)
		{
			// leader updates frequently, followers use this to determine when to update to a new desired aim position, so less frequent
			m_fUpdateAimTimer = (m_bIgnoreGroup || pLeader == pPed) ? .25f : fwRandom::GetRandomNumberInRange(4.0f, 5.25f);
		}
	}
}

CTaskAimSweep::FSM_Return CTaskAimSweep::ProcessPreFSM()
{
	// Attempt to update our aim target
	UpdateAimTarget();

	return FSM_Continue;
}

CTaskAimSweep::FSM_Return CTaskAimSweep::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_Aim)
			FSM_OnEnter
				Aim_OnEnter();
			FSM_OnUpdate
				return Aim_OnUpdate();

		FSM_State(State_Finish)
			return FSM_Quit;

	FSM_End
}

CTaskAimSweep::FSM_Return CTaskAimSweep::Start_OnUpdate()
{
	// Only continue once we have a target to aim at
	if(m_vTargetPosition.IsNonZero())
	{
		SetState(State_Aim);
	}

	return FSM_Continue;
}

void CTaskAimSweep::Aim_OnEnter()
{
	// Currently passing in WCS_Aim because we go into combat once the target is seen
	SetNewTask(rage_new CTaskGun(CWeaponController::WCT_Aim, CTaskTypes::TASK_AIM_GUN_ON_FOOT, m_vTargetPosition));
}

CTaskAimSweep::FSM_Return CTaskAimSweep::Aim_OnUpdate()
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Finish);
	}

	// We want to constantly update our target as it is changing frequently
	CTask* pSubTask = GetSubTask();
	if(pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_GUN)
	{
		CTaskGun* pGunTask = static_cast<CTaskGun*>(pSubTask);
		pGunTask->SetTarget(CWeaponTarget(m_vTargetPosition));
	}

	return FSM_Continue;
}

#if !__FINAL
const char * CTaskAimSweep::GetStaticStateName( s32 iState )
{
	static const char* aStateNames[] = 
	{
		"Start",
		"Aim",
		"Finish",
	};

	if (aiVerify(iState>=State_Start && iState<=State_Finish))
	{
		return aStateNames[iState];
	}

	return 0;
}
#endif // !__FINAL
