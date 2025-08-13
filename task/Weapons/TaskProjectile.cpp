// File header
#include "Task/Weapons/TaskProjectile.h"

// Game headers
#include "animation/MovePed.h"
#include "Camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "debug/DebugScene.h"
#include "Event/EventDamage.h"
#include "Event/Events.h"
#include "fwscene/stores/framefilterdictionarystore.h"
#include "grcore/debugdraw.h"
#include "Network/General/NetworkPendingProjectiles.h"
#include "network/Objects/Entities/NetObjPlayer.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Peds/Ped.h"
#include "physics/WorldProbe/worldprobe.h"
#include "scene/world/GameWorld.h"
#include "script/script_hud.h"
#include "Task/Combat/Cover/TaskCover.h"
#include "Task/Combat/TaskCombat.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/Motion/Locomotion/TaskCombatRoll.h"
#include "Task/Motion/Locomotion/TaskLocomotion.h"
#include "Task/Motion/Locomotion/TaskMotionAiming.h"
#include "Task/Motion/Locomotion/TaskMotionPed.h"
#include "Task/Motion/TaskMotionBase.h"
#include "Task/Movement/TaskSlideToCoord.h"
#include "Task/Weapons/TaskBomb.h"
#include "Task/Weapons/TaskSwapWeapon.h"
#include "Task/Weapons/TaskWeaponBlocked.h"
#include "Task/Weapons/Gun/TaskAimGunOnFoot.h"
#include "Weapons/projectiles/Projectile.h"
#include "Weapons/projectiles/ProjectileManager.h"
#include "Weapons/ThrownWeaponInfo.h"
#include "Weapons/Weapon.h"
#include "camera/gameplay/aim/FirstPersonShooterCamera.h"

// Macro to disable optimizations if set
AI_OPTIMISATIONS()
AI_WEAPON_OPTIMISATIONS()

// Constants
const float PLAYER_DEFAULT_PROJECTILE_VELOCITY = 33.0f;
const float AI_DEFAULT_PROJECTILE_VELOCITY     = 17.0f;
const float PED_AIM_OFFSET_Z                   = 0.05f;
const float MIN_PROJECTILE_VELOCITY            = 10.0f;
const float MIN_PROJECTILE_DISTANCE            = 5.0f;
const float MAX_PROJECTILE_DISTANCE            = 25.0f;

//////////////////////////////////////////////////////////////////////////
// CThrowProjectileHelper
//////////////////////////////////////////////////////////////////////////

bool CThrowProjectileHelper::CalculateAimTrajectory(CPed* pPed, const Vector3& vStart, const Vector3& vTarget, const Vector3& vTargetVelocity, Vector3& vTrajectory)
{
	const int NUMBER_TRAJECTORY_ATTEMPTS = 3;

	float fProjectileVelocity = GetProjectileVelocity(pPed);

	Vector3 vTargetPos(vTarget);
	vTargetPos.z += PED_AIM_OFFSET_Z;

	// Work out how far the target will have moved before the projectile hits, and add that onto the target pos
	float fDistToTarget = (vTargetPos - vStart).Mag();

	// Scale the velocity based on the target distance
	float fProportion = Clamp((fDistToTarget - MIN_PROJECTILE_DISTANCE)/(MAX_PROJECTILE_DISTANCE - MIN_PROJECTILE_DISTANCE), 0.0f, 1.0f);
	fProjectileVelocity = MIN_PROJECTILE_VELOCITY + ((fProjectileVelocity-MIN_PROJECTILE_VELOCITY)*fProportion);

	// Start with a lower speed and try to adjust to hit the target avoiding any collisions
	float fSpeed = fProjectileVelocity * 1.2f;
	bool bValidTrajectoryFound = false;

	float fTime = fDistToTarget / fProjectileVelocity;
	Vector3 vAdjustedTarget = vTargetPos + ( vTargetVelocity * fTime );

	// Try NUMBER_TRAJECTORY_ATTEMPTS times to avoid hitting anything
	for(int i = 0; i < NUMBER_TRAJECTORY_ATTEMPTS; i++)
	{
		// Calculate the trajectory needed to hit the target with the desired speed
		float fTime = CalculateAimTrajectoryWithSpeed(vStart, vAdjustedTarget, fSpeed, vTrajectory);

		Vector3 vHalfwayPos = vStart + (vTrajectory * fTime * 0.5f) + (Vector3(0.0f, 0.0f, GRAVITY * 0.5f) * rage::square(fTime * 0.5f));

		// Check the trajectory for collisions, if a collision occurs, try adjusting the speed
		WorldProbe::CShapeTestProbeDesc probeDesc;
		probeDesc.SetStartAndEnd(vStart, vHalfwayPos);
		probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
		if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
		{
			if (pPed->GetIsInInterior())
				// favour faster (lower) throws if indoors
				fSpeed += fProjectileVelocity * 0.2f;
			else
				fSpeed -= fProjectileVelocity * 0.2f;

			continue;
		}

		bValidTrajectoryFound = true;
		break;
	}

	return bValidTrajectoryFound;
}

Vector3 CThrowProjectileHelper::GetPedThrowPosition(CPed* pPed, const fwMvClipId &clipId)
{
	Vector3 vPos = GetThrowClipOffset(pPed, clipId);
	vPos = pPed->TransformIntoWorldSpace(vPos);

#if __DEV
	static bool RENDER_POSITION = false;
	if(RENDER_POSITION)
	{
		grcDebugDraw::Sphere(vPos, 0.3f, Color_green, true);
	}
#endif

	return vPos;
}

float CThrowProjectileHelper::GetProjectileVelocity(CPed* pPed)
{
	weaponAssert(pPed->GetWeaponManager());
	const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
	if(pWeapon && pWeapon->GetWeaponInfo()->GetIsThrownWeapon())
	{
		return pWeapon->GetWeaponInfo()->GetProjectileForce();
	}

	if(pPed->IsAPlayerPed())
	{
		return PLAYER_DEFAULT_PROJECTILE_VELOCITY;
	}

	return AI_DEFAULT_PROJECTILE_VELOCITY;
}

bool CThrowProjectileHelper::IsPlayerPriming(const CPed* pPed)
{
	bool bUseFPSAimIK = false;
#if FPS_MODE_SUPPORTED
	bUseFPSAimIK = pPed->GetEquippedWeaponInfo() ? pPed->GetEquippedWeaponInfo()->GetUseFPSAimIK() : false;
#endif

	if(pPed->IsLocalPlayer() FPS_MODE_SUPPORTED_ONLY( && (!bUseFPSAimIK || !pPed->IsFirstPersonShooterModeEnabledForPlayer(false))))
	{
		const CControl* pControl = pPed->GetControlFromPlayer();
		if( pControl )
		{
			//const u8 PLAYER_FIRE_PROJECTILE_BOUNDARY = 190;
			const float PLAYER_FIRE_PROJECTILE_BOUNDARY = 0.745f;
			const float fAttackValue = MAX(pControl->GetPedAttack2().GetNorm(), pControl->GetPedAttack().GetNorm());
			return fAttackValue >= PLAYER_FIRE_PROJECTILE_BOUNDARY;
		}
	}

	return false;
}

bool CThrowProjectileHelper::IsPlayerRolling(const CPed* pPed)
{
	if(pPed->IsLocalPlayer() && pPed->GetMotionData()->GetIsStrafing() && !pPed->GetMotionData()->GetIsStill())
	{
		const CControl* pControl = pPed->GetControlFromPlayer();
		if( pControl )
		{			
			return pControl->GetPedJump().IsPressed() && pPed->GetPedIntelligence()->GetCanCombatRoll();
		}
	}

	return false;
}

bool CThrowProjectileHelper::IsPlayerFiring(const CPed* pPed)
{
	if(pPed->IsLocalPlayer() )
	{
		const CControl* pControl = pPed->GetControlFromPlayer();
		if( pControl )
		{
			//const u8 PLAYER_FIRE_PROJECTILE_BOUNDARY = 190;
			//@@: location CTHROWPROJECTILEHELPER_ISPLAYER_FIRING
			const float PLAYER_FIRE_PROJECTILE_BOUNDARY = 0.745f;
			const float fAttackValue = MAX(pControl->GetPedAttack2().GetNorm(), pControl->GetPedAttack().GetNorm());
			return fAttackValue <= PLAYER_FIRE_PROJECTILE_BOUNDARY;
		}
	}

	return false;
}

bool CThrowProjectileHelper::IsPlayerFirePressed(const CPed* pPed)
{
	if(pPed->IsLocalPlayer() )
	{
		const CControl* pControl = pPed->GetControlFromPlayer();
		if( pControl )
		{
			const float PLAYER_FIRE_PROJECTILE_BOUNDARY = 0.745f;
			return pControl->GetPedAttack2().IsPressed(PLAYER_FIRE_PROJECTILE_BOUNDARY) || pControl->GetPedAttack().IsPressed(PLAYER_FIRE_PROJECTILE_BOUNDARY);
		}
	}

	return false;
}

bool CThrowProjectileHelper::HasHeldProjectileTooLong(CPed* pPed, float fTimeHeld)
{
	weaponAssert(pPed->GetWeaponManager());
	const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
	if(pWeapon)
	{
		const CWeaponInfo* pWeaponInfo = pWeapon->GetWeaponInfo();
		if(pWeaponInfo)
		{
			const float fFuseTime = pWeaponInfo->GetProjectileFuseTime();
			if (fFuseTime > 0.0f)
				return fTimeHeld > fFuseTime;
		}
	}

	return false;
}

void CThrowProjectileHelper::SetWeaponSlotAfterFiring(CPed* pPed)
{
	Assert(pPed);

	weaponAssert(pPed->GetWeaponManager());	
	if(pPed->GetWeaponManager()->GetEquippedWeaponInfo() && pPed->GetWeaponManager()->GetEquippedWeaponInfo()->GetIsThrownWeapon()
#if 0 // CS
		|| (pPed->GetWeaponManager()->GetEquippedWeaponHash() == WEAPONSLOT_SPECIAL && pPed->GetWeaponManager()->GetChosenWeaponType() == WEAPONTYPE_OBJECT)
#endif // 0
		)
	{
		int nAmmo = pPed->GetInventory()->GetWeaponAmmo(pPed->GetWeaponManager()->GetEquippedWeaponInfo());

		if(nAmmo == 0)
		{
			// Out of ammo - swap weapon
			const CWeaponInfo* pWeaponInfo = pPed->GetWeaponManager()->GetEquippedWeaponInfo();
			pPed->GetWeaponManager()->DropWeapon(pWeaponInfo->GetHash(), true);
			
			// Don't equip a weapon if the dropped weapon was non lethal
			if(pWeaponInfo && !pWeaponInfo->GetDontSwapWeaponIfNoAmmo())
			{
				if (pWeaponInfo->GetSwapToUnarmedWhenOutOfThrownAmmo())
				{
					pPed->GetWeaponManager()->EquipWeapon(pPed->GetDefaultUnarmedWeaponHash());
				}
				else
				{
					pPed->GetWeaponManager()->EquipBestWeapon();
				}
			}
		}
		else
		{
			// Still have some ammo left - draw a new projectile
 			pPed->GetWeaponManager()->EquipWeapon(pPed->GetWeaponManager()->GetEquippedWeaponHash());
		}
	}
}

dev_float VERTICAL_DIFFERENCE_MULTIPLIER = 2.0f;

float CThrowProjectileHelper::CalculateAimTrajectoryWithSpeed(const Vector3& vStart, const Vector3& vTarget, const float fSpeed, Vector3& vTrajectory)
{
	vTrajectory = vTarget - vStart;

	const float	f2dVelocity = fSpeed;
	const float	f2dDistance = vTrajectory.XYMag();
	const float	fTime = f2dDistance / f2dVelocity;
	const float	fHeightChange = -GRAVITY * rage::square(fTime);
	const float	fVerticalVelocity = (fHeightChange) + (vTrajectory.z*VERTICAL_DIFFERENCE_MULTIPLIER);

	vTrajectory.z = 0.0f;
	vTrajectory.Normalize();
	vTrajectory.Scale(f2dVelocity);
	vTrajectory.z += fVerticalVelocity;

	return fTime;
}

Vector3 CThrowProjectileHelper::GetThrowClipOffset(CPed* UNUSED_PARAM(pPed), const fwMvClipId &clipId) // DEV_ONLY(pPed)
{
	static fwMvClipId s_WeaponThrowLongClipId("grenade_throw_overarm",0x1C10DF01); 
	static fwMvClipId s_WeaponThrowShortClipId("grenade_throw_short",0x2E8019FC);

	static Vector3 svShortAnimOffset(0.308427393f, -0.265351206f,  1.00279045f);
	static Vector3 svLongAnimOffset	(0.390442640f, -0.0503886044f, 0.148556828f);
	//static bool bShortCalculated = false;
	//static bool bLongCalculated  = false;

	//bool bCalculated = false;
	Vector3* pvOffset = NULL;
	
	if(clipId == s_WeaponThrowShortClipId)
	{
		//bCalculated = bShortCalculated;
		pvOffset = &svShortAnimOffset;
		//bShortCalculated = true;
	}
	else if(clipId == s_WeaponThrowLongClipId)
	{
		//bCalculated = bLongCalculated;
		pvOffset = &svLongAnimOffset;
		//bLongCalculated = true;
	}
	else
	{
		pvOffset = &svShortAnimOffset;
		return *pvOffset;
	}

	Assert(pvOffset);
	return *pvOffset;
}

//////////////////////////////////////////////////////////////////////////
// CTaskAimAndThrowProjectile
//////////////////////////////////////////////////////////////////////////
const fwMvRequestId CTaskAimAndThrowProjectile::ms_StartPrime("Prime",0x4CC8E30C);
const fwMvRequestId CTaskAimAndThrowProjectile::ms_StartReload("Reload",0x5DA46B68);
const fwMvRequestId CTaskAimAndThrowProjectile::ms_StartFinish("Finish",0xCB6420E3);
const fwMvRequestId CTaskAimAndThrowProjectile::ms_StartOutro("Outro",0x5D40B28D);
const fwMvRequestId CTaskAimAndThrowProjectile::ms_StartHold("Hold",0x981F195B);
const fwMvRequestId CTaskAimAndThrowProjectile::ms_StartDropPrime("DropPrime",0xB928EA79);
//FPS mode
const fwMvRequestId CTaskAimAndThrowProjectile::ms_FPSFidget("FPSFidget", 0xCCE4A656);
const fwMvFlagId	CTaskAimAndThrowProjectile::ms_UsingFPSMode("FPSMode",0x50E5C59C);
const fwMvFlagId	CTaskAimAndThrowProjectile::ms_HasBreatheHighAdditives("HasBreatheHighAdditives",0xD5ECF3B5);
const fwMvFlagId	CTaskAimAndThrowProjectile::ms_HasBwdAdditive("HasBwdAdditive",0x33F3E71E);
//End FPS Mode
const fwMvFlagId	CTaskAimAndThrowProjectile::ms_SkipPullPinId("SkipPullPin",0x239DF644);
const fwMvFlagId	CTaskAimAndThrowProjectile::ms_DropProjectileFlagId("DropProjectileFlag",0x3C3CEA90);
const fwMvFlagId	CTaskAimAndThrowProjectile::ms_DoDropId("DoDrop",0x81362756);
const fwMvFlagId	CTaskAimAndThrowProjectile::ms_SkipIntroId("SkipIntro",0x4D7836E0);
const fwMvFlagId	CTaskAimAndThrowProjectile::ms_OverhandOnlyId("OverhandOnly",0xAD95CA85);
const fwMvFlagId	CTaskAimAndThrowProjectile::ms_UseThrowPlaceId("UseThrowPlace",0xBA541D80);
const fwMvFlagId	CTaskAimAndThrowProjectile::ms_UseDropUnderhandId("DropUnderhand",0xAC294D26);
const fwMvFlagId	CTaskAimAndThrowProjectile::ms_NoStaticDropId("NoStaticDrop",0x62bd6f70);
const fwMvFlagId	CTaskAimAndThrowProjectile::ms_IsRollingId("IsRolling",0x153c3a80);
const fwMvFloatId	CTaskAimAndThrowProjectile::ms_PitchPhase("PitchPhase",0xDA36894E);
const fwMvFloatId	CTaskAimAndThrowProjectile::ms_ThrowPitchPhase("ThrowPitchPhase",0xA472A004);
const fwMvFloatId	CTaskAimAndThrowProjectile::ms_ThrowPhase("ThrowPhase",0x9D46658E);
const fwMvFloatId	CTaskAimAndThrowProjectile::ms_ReloadPhase("ReloadPhase",0x24823469);
const fwMvFloatId	CTaskAimAndThrowProjectile::ms_TransitionPhase("TransitionPhase",0x5DD0E7EB);
const fwMvFloatId	CTaskAimAndThrowProjectile::ms_StrafeSpeed("StrafeSpeed",0x59A5FA46);
const fwMvFloatId	CTaskAimAndThrowProjectile::ms_DesiredStrafeSpeed("DesiredStrafeSpeed",0x8e945f2d);
const fwMvFloatId	CTaskAimAndThrowProjectile::ms_StrafeDirectionId("StrafeDirection",0xCF6AA9C6);
const fwMvFloatId	CTaskAimAndThrowProjectile::ms_GaitAdditive("GaitAdditive",0x31E5933F);
const fwMvFloatId	CTaskAimAndThrowProjectile::ms_GaitAdditiveTransPhase("GaitAdditiveTransPhase",0xDA38C1ED);
const fwMvFloatId	CTaskAimAndThrowProjectile::ms_ShadowWeight("ShadowWeight",0x243D1D53);
const fwMvFloatId	CTaskAimAndThrowProjectile::ms_PullPinPhase("PullPinPhase",0x182B6AA5);
//FPS Mode
const fwMvFloatId	CTaskAimAndThrowProjectile::ms_AimingLeanAdditiveWeight("AimingLeanAdditiveWeight", 0x8F882A7);
const fwMvFloatId	CTaskAimAndThrowProjectile::ms_AimingBreathingAdditiveWeight("AimingBreathingAdditiveWeight", 0x4BCEE31D);
const fwMvFlagId	CTaskAimAndThrowProjectile::ms_UseGripClip("UseGripClip",0x72838804);
const fwMvFloatId	CTaskAimAndThrowProjectile::ms_StrafeSpeed_AdditiveId("StrafeSpeed_Additive",0xb4f01ffa);
const fwMvFloatId	CTaskAimAndThrowProjectile::ms_StrafeDirection_AdditiveId("StrafeDirection_Additive",0xf72f7955);
//End FPS Mode
const fwMvBooleanId	CTaskAimAndThrowProjectile::ms_ThrowCompleteEvent("ThrowComplete",0x4BFCEDBF);
const fwMvBooleanId	CTaskAimAndThrowProjectile::ms_ReloadCompleteEvent("ReloadComplete",0x1CA52CBF);
const fwMvBooleanId CTaskAimAndThrowProjectile::ms_EnterIntroEvent("OnEnterIntro",0xE312777C);
const fwMvBooleanId CTaskAimAndThrowProjectile::ms_EnterHoldEvent("OnEnterHold",0xD7DF049F);
const fwMvBooleanId CTaskAimAndThrowProjectile::ms_EnterOutroEvent("OnEnterOutro",0x4CBE6874);
const fwMvBooleanId CTaskAimAndThrowProjectile::ms_ReloadEvent("OnEnterReload",0xB2573ED7);
//FPS Mode
const fwMvBooleanId CTaskAimAndThrowProjectile::ms_FPSReachPin("FPSReachPin",0x177D7851);
//End FPS Mode
const fwMvClipSetVarId CTaskAimAndThrowProjectile::ms_StreamedClipSet("StreamedClipSet",0x83D4FC68);
const fwMvFilterId	CTaskAimAndThrowProjectile::ms_GripFilterId("GripFilter",0x57C40DEC);
const fwMvFilterId	CTaskAimAndThrowProjectile::ms_FrameFilterId("FrameFilter",0x4BED5890);
const fwMvFilterId	CTaskAimAndThrowProjectile::ms_PullPinFilterId("PullPinFilter",0x5BD0DFA8);
float CTaskAimAndThrowProjectile::m_sfThrowingMainMoverCapsuleRadius = 0.55f; //0.65f; retuned B* 493871
const fwMvFlagId	CTaskAimAndThrowProjectile::ms_AimGunThrowId("IsAiming",0x9d10c0bd);
const fwMvFlagId	CTaskAimAndThrowProjectile::ms_FPSCookingProjectile("FPSCookingProjectile",0xC45C0ACF);

CTaskAimAndThrowProjectile::Tunables CTaskAimAndThrowProjectile::sm_Tunables;
IMPLEMENT_WEAPON_TASK_TUNABLES(CTaskAimAndThrowProjectile, 0xf2077a8f);

CTaskAimAndThrowProjectile::CTaskAimAndThrowProjectile(const CWeaponTarget& targeting, CEntity* pIgnoreCollisionEntity, bool bCreateInvincibileProjectile
#if ENABLE_GUN_PROJECTILE_THROW
	, bool bAimGunAndThrow, int iAmmoInClip, u32 uAimAndThrowWeaponHash, bool bProjectileThrownFromDPadInput
#endif	//ENABLE_GUN_PROJECTILE_THROW
)													  
: m_target(targeting)
, m_pIgnoreCollisionEntity(pIgnoreCollisionEntity)
, m_bFire(false)
, m_bHasThrownProjectile(false)
, m_bPriming(false)
, m_bDropProjectile(false)
, m_bDropUnderhand(false)
, m_bForceStandingThrow(false)
, m_bFalseAiming(false)
, m_fTimeMoving(0.0f)
, m_fBlockAimingIndependentMoverTime(0.0f)
, m_fCurrentPitch(0.5f)
, m_fDesiredPitch(0.5f)
, m_fDesiredThrowPitch(0.5f)
, m_fThrowPitch(0.5f)
, m_fThrowPhase(0.5f)
, m_fReloadPhase(0.9f)
, m_fInterruptPhase(0.75f)
, m_fStrafeSpeed(-1.0f)
, m_fStrafeDirection(0.5f)
, m_fStrafeDirectionSignal(0)
, m_fCurrentShadowWeight(1.0f)
, m_pFrameFilter(NULL)
, m_pGripFrameFilter(NULL)
, m_pPullPinFilter(NULL)
, m_bIsAiming(false)
, m_bIsFiring(false)
, m_bHasCachedWeaponValues(false)
, m_bCanBePlaced(false)
, m_bPrimedButPlacing(false)
, m_bThrowPlace(false)
, m_bHasNoPullPin(false)
, m_bPinPulled(false)
, m_bWaitingToDie(false)
, m_bMoveTreeIsDropMode(false)
, m_bAllowPriming(false)
, m_bWasAiming(false)
, m_bHasTargetPosition(false)
, m_bPlayFPSTransition(false)
, m_bSkipFPSOutroState(false)
, m_bPlayFPSRNGIntro(false)
, m_bPlayFPSPullPinIntro(false)
, m_bPlayFPSFidget(false)
, m_bMoveFPSFidgetEnded(false)
, m_bFPSReachPinOnIntro(false)
, m_bFPSWantsToThrowWhenReady(false)
, m_bFPSUseAimIKForWeapon(false)
, m_pFPSWeaponMoveNetworkHelper(NULL)
, m_bFPSPlayingUnholsterIntro(false)
, m_bCreateInvincibileProjectile(bCreateInvincibileProjectile)
, m_LastLocalRollToken(0)
, m_LastCloneRollToken(0)
, m_vTargetPosition(VEC3_ZERO)
, m_vLastProjectilePos(VEC3_ZERO)
, m_NetworkDefId(fwMvNetworkDefId((u32)0))
#if ENABLE_GUN_PROJECTILE_THROW
, m_bAimGunAndThrow(bAimGunAndThrow)
, m_pTempWeaponObject(NULL)
, m_iBackupWeaponClipAmmo(iAmmoInClip)
, m_uAimAndThrowWeaponHash(uAimAndThrowWeaponHash)
, m_bProjectileThrownFromDPadInput(bProjectileThrownFromDPadInput)
#endif	//ENABLE_GUN_PROJECTILE_THROW
, m_fFPSStateToTransitionBlendTime(-1.0f)
, m_fFPSTransitionToStateBlendTime(-1.0f)
, m_fStrafeSpeedAdditive(0.f)
, m_fStrafeDirectionAdditive(0.f)
, m_fFPSTaskNetworkBlendDuration(-1.0f)
, m_fFPSTaskNetworkBlendTimer(0.f)
, m_fFPSDropThrowSwitchTimer(-1.0f)
{
	SetInternalTaskType(CTaskTypes::TASK_AIM_AND_THROW_PROJECTILE);
}

CTaskAimAndThrowProjectile::~CTaskAimAndThrowProjectile()
{
}

bool CTaskAimAndThrowProjectile::ProcessPostPreRender()
{
	CPed* pPed = GetPed();

	if(!pPed->IsNetworkClone())
	{
		bool bFPSMode = false;
#if FPS_MODE_SUPPORTED
		bFPSMode = pPed->IsFirstPersonShooterModeEnabledForPlayer(false);
		bool pFPSPopPitch = bFPSMode && (pPed->GetMotionData()->GetWasFPSUnholster() || !pPed->GetMotionData()->GetWasUsingFPSMode());
#endif
		UpdateAiming(pPed FPS_MODE_SUPPORTED_ONLY(, pFPSPopPitch));
		if(!bFPSMode)
		{
			ProcessThrowProjectile();
		}
	}
	else
	{
		// update MoVE params
		if(m_MoveNetworkHelper.IsNetworkActive()) 
		{
			m_MoveNetworkHelper.SetFloat(ms_PitchPhase, m_fCurrentPitch);
			m_MoveNetworkHelper.SetFloat(ms_ThrowPitchPhase, m_fThrowPitch);
			if(m_bHasTargetPosition)
				pPed->GetIkManager().PointGunAtPosition(m_vTargetPosition, -1.0f);
			else
				pPed->GetIkManager().PointGunInDirection(pPed->GetCurrentHeading(), 0.0f);
		}
	}

	// update MoVE params
	if (m_MoveNetworkHelper.IsNetworkActive()) 
	{	
		if(m_pFrameFilter)
			m_MoveNetworkHelper.SetFilter(m_pFrameFilter, ms_FrameFilterId);

		if(m_pGripFrameFilter)
			m_MoveNetworkHelper.SetFilter(m_pGripFrameFilter, ms_GripFilterId);

		if(m_pPullPinFilter)
			m_MoveNetworkHelper.SetFilter(m_pPullPinFilter, ms_PullPinFilterId);		

		if(!sm_Tunables.m_bEnableGaitAdditive)
			m_MoveNetworkHelper.SetFloat(ms_GaitAdditive, 0.0f);
		else
		{
			float gaitAdditivePhase = m_MoveNetworkHelper.GetFloat(ms_GaitAdditiveTransPhase);
			if (gaitAdditivePhase >= 0.0f)
				m_MoveNetworkHelper.SetFloat(ms_GaitAdditive, 1.0f-gaitAdditivePhase);
		}

		// update gait info:
		Vector2 vDesiredMBR = pPed->GetMotionData()->GetGaitReducedDesiredMoveBlendRatio();
		float fDesiredStrafeSpeed = vDesiredMBR.Mag() / MOVEBLENDRATIO_SPRINT;	

		Vector2 vCurrentMBR = pPed->GetMotionData()->GetCurrentMoveBlendRatio();
		m_fStrafeSpeed = vCurrentMBR.Mag() / MOVEBLENDRATIO_SPRINT;

		//Time moving is used to help determine if one should do a moving throw
		if (m_fStrafeSpeed >= 0.3f)
			m_fTimeMoving += fwTimer::GetTimeStep();
		else
			m_fTimeMoving = 0;		

		if (!pPed->IsStrafing() || (GetState()==State_ThrowProjectile && m_bForceStandingThrow))
		{
			m_fStrafeSpeed = 0.0f; 
			fDesiredStrafeSpeed = 0.0f;
		}

		m_MoveNetworkHelper.SetFloat(ms_StrafeSpeed, m_fStrafeSpeed);
		m_MoveNetworkHelper.SetFloat(ms_DesiredStrafeSpeed, fDesiredStrafeSpeed);	

		static dev_float m_sfShadowWeightApproachRate = 3.0f;
		if (m_bMoveTreeIsDropMode)
			Approach(m_fCurrentShadowWeight, 0.0f, m_sfShadowWeightApproachRate, fwTimer::GetTimeStep());		
		else
			Approach(m_fCurrentShadowWeight, 1.0f, m_sfShadowWeightApproachRate, fwTimer::GetTimeStep());
		m_MoveNetworkHelper.SetFloat(ms_ShadowWeight, m_fCurrentShadowWeight);


		m_fStrafeDirection = fwAngle::LimitRadianAngleSafe( atan2( vCurrentMBR.y, vCurrentMBR.x ) - HALF_PI );	
		float fStrafeSignal = CTaskMotionBasicLocomotion::ConvertRadianToSignal(m_fStrafeDirection);
		static dev_float STRAFE_DIRECTION_APPROACH_RATE = 3.0f;
		if (fabs(fStrafeSignal - m_fStrafeDirectionSignal)>0.5f)
		{
			if (m_fStrafeDirectionSignal > 0.5f)
			{	
				m_fStrafeDirectionSignal -= 1.0f;
				Approach(m_fStrafeDirectionSignal, fStrafeSignal, STRAFE_DIRECTION_APPROACH_RATE, fwTimer::GetTimeStep()); 
				if (m_fStrafeDirectionSignal < 0)
					m_fStrafeDirectionSignal +=1.0f;
			} else
			{
				m_fStrafeDirectionSignal += 1.0f;
				Approach(m_fStrafeDirectionSignal, fStrafeSignal, STRAFE_DIRECTION_APPROACH_RATE, fwTimer::GetTimeStep()); 
				if (m_fStrafeDirectionSignal > 1.0f)
					m_fStrafeDirectionSignal -=1.0f;
			}
		}
		else
			Approach(m_fStrafeDirectionSignal, fStrafeSignal, STRAFE_DIRECTION_APPROACH_RATE, fwTimer::GetTimeStep()); 
		m_MoveNetworkHelper.SetFloat(ms_StrafeDirectionId, m_fStrafeDirectionSignal);

#if FPS_MODE_SUPPORTED
		if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true) && m_bFPSUseAimIKForWeapon)
		{
			m_MoveNetworkHelper.SetFloat(ms_AimingLeanAdditiveWeight, 1.0f);
			m_MoveNetworkHelper.SetFloat(ms_AimingBreathingAdditiveWeight, 1.0f);

			m_MoveNetworkHelper.SetFloat(ms_StrafeSpeed_AdditiveId, m_fStrafeSpeedAdditive);
			m_MoveNetworkHelper.SetFloat(ms_StrafeDirection_AdditiveId, CTaskMotionBase::ConvertRadianToSignal(m_fStrafeDirectionAdditive));

			if(m_pFPSWeaponMoveNetworkHelper && m_pFPSWeaponMoveNetworkHelper->IsNetworkActive())
			{
				m_pFPSWeaponMoveNetworkHelper->SetFloat(ms_AimingLeanAdditiveWeight, 1.0f);
				m_pFPSWeaponMoveNetworkHelper->SetFloat(ms_AimingBreathingAdditiveWeight, 1.0f);

				m_pFPSWeaponMoveNetworkHelper->SetFloat(ms_StrafeSpeed, m_fStrafeSpeed);
				m_pFPSWeaponMoveNetworkHelper->SetFloat(ms_StrafeDirectionId, m_fStrafeDirectionSignal);
			}
		}
#endif
	}

	m_bWasAiming =  IsAiming(pPed);

	return true;
}

bool CTaskAimAndThrowProjectile::ProcessPostPreRenderAfterAttachments()
{
	CPed* pPed = GetPed();
	if(!pPed->IsNetworkClone())
	{
#if FPS_MODE_SUPPORTED
		if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
		{
			ProcessThrowProjectile();
		}
#endif // FPS_MODE_SUPPORTED
	}

	return true;
}

void CTaskAimAndThrowProjectile::ProcessThrowProjectile()
{
	CPed* pPed = GetPed();

	Vector3 vStartOverride(VEC3_ZERO);
	bool bOverrideStartPos = false;
	bool bAimGunThrowInCoverInFPS = false;
#if FPS_MODE_SUPPORTED
	bAimGunThrowInCoverInFPS = m_bAimGunAndThrow && pPed->GetIsInCover() && pPed->IsFirstPersonShooterModeEnabledForPlayer(false);
#endif	//FPS_MODE_SUPPORTED
	if (GetState()== State_ThrowProjectile && !m_bHasThrownProjectile && !bAimGunThrowInCoverInFPS)
	{
		CObject* pProjectile = pPed->GetWeaponManager()->GetEquippedWeaponObject();		
		if (pProjectile)
		{
			Vector3 vStart = m_vLastProjectilePos;
			Vector3 vEnd = VEC3V_TO_VECTOR3(pProjectile->GetTransform().GetPosition());	
			if (!vStart.IsZero())
			{					
				Vector3 vDir = vEnd-vStart;	
				Vector3 vDot = vEnd - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()); vDot.z=0; vDot.Normalize();
				Vector3 vNorm = VEC3V_TO_VECTOR3(pPed->GetTransform().GetForward());
				vNorm.z=0; vNorm.Normalize();
				if ( vDot.Dot(vNorm) >0 )
				{
					vEnd.AddScaled(vDir, 0.5f);
					WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
					capsuleDesc.SetCapsule(vStart, vEnd, 0.025f);
					capsuleDesc.SetIncludeFlags( (ArchetypeFlags::GTA_ALL_TYPES_WEAPON & ~ArchetypeFlags::GTA_PROJECTILE_TYPE & ~ArchetypeFlags::GTA_RIVER_TYPE) );
					capsuleDesc.SetTypeFlags( ArchetypeFlags::GTA_WEAPON_TEST );
					capsuleDesc.SetExcludeEntity(pPed);
					capsuleDesc.SetIsDirected(true);
#if __BANK
					char szDebugText[128];
					formatf( szDebugText, "OVERRIDE_COLLISION_SPHERE_START" );
					CTask::ms_debugDraw.AddSphere( RCC_VEC3V(vStart), 0.025f, Color_yellow, 2, atStringHash(szDebugText), false );
					formatf( szDebugText, "OVERRIDE_COLLISION_SPHERE_END" );
					CTask::ms_debugDraw.AddSphere( RCC_VEC3V(vEnd), 0.025f, Color_yellow4, 2, atStringHash(szDebugText), false );
					formatf( szDebugText, "OVERRIDE_COLLISION_ARROW" );
					CTask::ms_debugDraw.AddArrow( RCC_VEC3V(vStart), RCC_VEC3V(vEnd), 0.025f, Color_pink, 2, atStringHash(szDebugText) );
#endif 		
					if(WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc))
					{
						m_bFire = true;
						bOverrideStartPos = true;
						vStartOverride = m_vLastProjectilePos;
					}

				}					
			}
			m_vLastProjectilePos = vEnd;
		}
	}
	// throw the projectile
	if(m_bFire)
	{
		weaponAssert(pPed->GetWeaponManager());
		CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
		if(pWeapon)
		{
			m_bFire = false;
			ThrowProjectileNow(pPed, pWeapon, bOverrideStartPos ? &vStartOverride : NULL);
		}
	}
}

void CTaskAimAndThrowProjectile::UpdateAdditives()
{
#if FPS_MODE_SUPPORTED
	float fDesiredStrafeSpeed = m_fStrafeSpeed;

	if (GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false, true))
	{
		TUNE_GROUP_BOOL(FIRST_PERSON_TUNE, REMAP_MBR_TO_ANALOGUE_CONTROL, true);
		if(REMAP_MBR_TO_ANALOGUE_CONTROL)
		{
			if(fDesiredStrafeSpeed > 0.5f)
			{
				fDesiredStrafeSpeed = 0.66f;
			}
			else if(fDesiredStrafeSpeed > 0.f)
			{
				fDesiredStrafeSpeed = 0.33f;
			}
			else
			{
				fDesiredStrafeSpeed = 0.f;
			}

			CTaskMotionAiming* pTaskMotionAiming = static_cast<CTaskMotionAiming*>(GetPed()->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_AIMING));
			if(pTaskMotionAiming)
			{
				if(pTaskMotionAiming->GetState() == CTaskMotionAiming::State_StandingIdle || pTaskMotionAiming->GetState() == CTaskMotionAiming::State_StopStrafing)
				{
					fDesiredStrafeSpeed = 0.f;
				}
			}
		}

		TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, ADDITIVE_SPEED_LERP_RATE_DEC, 2.f, 0.f, 100.f, 0.01f);
		TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, ADDITIVE_SPEED_LERP_RATE_ACC, 1.7f, 0.f, 100.f, 0.01f);

		float fLerpRate = fDesiredStrafeSpeed >= m_fStrafeSpeedAdditive ? ADDITIVE_SPEED_LERP_RATE_ACC : ADDITIVE_SPEED_LERP_RATE_DEC;
		Approach(m_fStrafeSpeedAdditive, fDesiredStrafeSpeed, fLerpRate, GetTimeStep());

		static dev_float ADD_DIR_LERP_RATE = 3.f;
		CTaskMotionBase::InterpValue(m_fStrafeDirectionAdditive, m_fStrafeDirection, ADD_DIR_LERP_RATE, true, GetTimeStep());
		m_fStrafeDirectionAdditive = fwAngle::LimitRadianAngle(m_fStrafeDirectionAdditive);
	}
#endif // FPS_MODE_SUPPORTED
}

void CTaskAimAndThrowProjectile::GetOverriddenGameplayCameraSettings(tGameplayCameraSettings& settings) const
{
#if ENABLE_GUN_PROJECTILE_THROW
	// If we're throwing a projectile whilst aiming, use the gun camera settings
	if (m_bAimGunAndThrow)
	{
		CTaskGun *pTaskGun = static_cast<CTaskGun*>(GetPed()->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_GUN));
		if (pTaskGun)
		{
			pTaskGun->GetOverriddenGameplayCameraSettings(settings);
			return;
		}
	}
#endif	//ENABLE_GUN_PROJECTILE_THROW

	// force the camera to stay put!
	if(GetState() == State_PlaceProjectile || m_bMoveTreeIsDropMode || GetState() == State_StreamingClips )
	{
		settings.m_CameraHash = ATSTRINGHASH("INVALID_CAMERA", 0x05aad591b);
	}
	else
	{ 
		//force default as the aim button may not necssarily be down, B* 1054819
		const CWeaponInfo* weaponInfo = camGameplayDirector::ComputeWeaponInfoForPed(*GetPed());
		if (weaponInfo)
			settings.m_CameraHash = weaponInfo->GetDefaultCameraHash();
	}
}

void CTaskAimAndThrowProjectile::CleanUp() 
{
	CNetObjPed     *netObjPed  = static_cast<CNetObjPed *>(GetPed()->GetNetworkObject());
	if(netObjPed)
	{   //Make sure this is not left set
		netObjPed->SetTaskOverridingPropsWeapons(false);
	}

	if (m_bPriming) 
	{//turn off priming FX and reset timer
		CWeapon* pWeapon = GetPed()->GetWeaponManager()->GetEquippedWeapon();
		if (pWeapon)
		{
			if (pWeapon->GetWeaponInfo()->GetDropWhenCooked())
				pWeapon->CancelCookTimer();
		}
		SetPriming(GetPed(), false);
	}

	float fBlendDuration = SLOW_BLEND_DURATION;

#if FPS_MODE_SUPPORTED
	if(GetPed()->IsLocalPlayer() && GetPed()->GetMotionData()->GetWasUsingFPSMode() && !GetPed()->GetMotionData()->GetUsingFPSMode())
	{
		fBlendDuration = INSTANT_BLEND_DURATION;
	}
	else if(GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false) && GetPed()->GetPlayerInfo() && !(GetPed()->GetPlayerInfo()->IsSprintAimBreakOutOver()))
	{
		TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE_PROJECTILE, fBlendOutAimToSprint, 0.5f, 0.0f, 1.0f, 0.01f);
		fBlendDuration = fBlendOutAimToSprint;
	}
#endif

	GetPed()->GetMovePed().ClearTaskNetwork(m_MoveNetworkHelper, fBlendDuration, CMovePed::Task_TagSyncTransition);
	//clear projectile strafe clipset	
	CTaskMotionBase* pTask = GetPed()->GetPrimaryMotionTask();
	taskAssert(pTask!=NULL);
	if (pTask)
	{
		m_StrafeClipSetRequestHelper.Release();
		pTask->ResetStrafeClipSet();		
	}

	if(m_NetworkDefId == CClipNetworkMoveInfo::ms_NetworkTaskAimAndThrowProjectileThrow)
	{
		CTaskMotionBase* pMotionTask = GetPed()->GetPrimaryMotionTask();
		if(pMotionTask && pMotionTask->GetTaskType() == CTaskTypes::TASK_MOTION_PED)
		{
			CTaskMotionPed* pMotionPedTask = static_cast<CTaskMotionPed*>(pMotionTask);
			pMotionPedTask->SetDoAimingIndependentMover(false);
		}
	}

#if ENABLE_GUN_PROJECTILE_THROW
	// Clean-up the temporary weapon object in the peds hand
	if (GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_ThrowingGrenadeWhileAiming) && m_pTempWeaponObject)
	{
		GetPed()->GetPedIntelligence()->SetDoneAimGrenadeThrow();
		CTaskAimGunOnFoot::RestorePreviousWeapon(GetPed(), m_pTempWeaponObject.Get(), m_iBackupWeaponClipAmmo);
	}
	// Only set this flag as false if we have broken out unexpectedly (else we want to restore weapons/play settle anims through CTaskAimGunOnFoot::ProcessGrenadeThrowFromGunAim)
	else if (GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_ThrowingGrenadeWhileAiming))
	{
		GetPed()->GetPedIntelligence()->SetDoneAimGrenadeThrow();
		GetPed()->SetPedConfigFlag(CPED_CONFIG_FLAG_ThrowingGrenadeWhileAiming, false);
	}
#endif	//ENABLE_GUN_PROJECTILE_THROW
}

void CTaskAimAndThrowProjectile::UpdateAiming(CPed* pPed, bool popIt) 
{
	if (m_MoveNetworkHelper.IsNetworkActive())
	{
		//////////////////////////////////////
		// Calculate and send MoVE parameters
		//////////////////////////////////////

		//////////////////////////////////
		//// COMPUTE AIM PITCH SIGNAL ////
		//////////////////////////////////
		
		float fSweepPitchMin = 0; 
		float fSweepPitchMax = QUARTER_PI;

#if FPS_MODE_SUPPORTED
		if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && m_bFPSUseAimIKForWeapon)
		{
			fSweepPitchMin = -PI/3.0f;
			fSweepPitchMax = PI/3.0f;
		}
#endif

		const float fThrowPitchMin = -QUARTER_PI; 

		// Calculate the aim vector (this determines the heading and pitch angles to point the anims in the correct direction)
		Vector3 vStart(ORIGIN);
		Vector3 vEnd(ORIGIN);		
		//CalculateAimVector(pPed, vStart, vEnd);	// Based on camera direction	
		if (pPed->IsLocalPlayer()) 
		{
			weaponAssert(pPed->GetWeaponManager());
			const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
			if (pWeapon)
			{
				float fWeaponRange = pWeapon->GetRange();

				CPed * pPlayer = CGameWorld::FindLocalPlayer();
				CControl *pControl = pPlayer->GetControlFromPlayer();
				if (pControl->GetPedTargetIsUp(ioValue::ANALOG_BUTTON_DOWN_THRESHOLD))
				{
					CEntity* pLockOnTarget = pPlayer->GetPlayerInfo()->GetTargeting().GetLockOnTarget();
					if (pLockOnTarget)
					{
						vStart = camInterface::GetPos();
						vEnd = VEC3V_TO_VECTOR3(pLockOnTarget->GetTransform().GetPosition());
						Assert(vEnd.FiniteElements()); // Attempting to track down an INF (B*682884)
						
						return; //?? does nothing
					}
				}

				const camFrame& aimFrame = camInterface::GetPlayerControlCamAimFrame();
				vStart = aimFrame.GetPosition();
				Vector3	vecAim = aimFrame.GetFront();
				vecAim.Normalize();
				vEnd = vStart + (fWeaponRange * vecAim);
				Assert(vEnd.FiniteElements()); // Attempting to track down an INF (B*682884)

				// Compute desired pitch angle value
				float fDesiredPitch = CTaskAimGun::CalculateDesiredPitch(pPed, vStart, vEnd);

				// Map the angle to the range 0.0->1.0
				m_fDesiredPitch = CTaskAimGun::ConvertRadianToSignal(fDesiredPitch, fSweepPitchMin, fSweepPitchMax, false);
				m_fDesiredThrowPitch = CTaskAimGun::ConvertRadianToSignal(fDesiredPitch, fThrowPitchMin, fSweepPitchMax, false);

				static dev_float s_fMinPitchForPlacedProjectileThrow = 0.25f;
				if (m_bCanBePlaced) //placed weapons can switch from hold to prime  B* 1041752			
				{
					CPlayerPedTargeting &playerTargeting = pPed->GetPlayerInfo()->GetTargeting();
					Vector3 vTarget;
					if (playerTargeting.GetLockOnTarget())
						vTarget = playerTargeting.GetLockonTargetPos();
					else 
						vTarget = playerTargeting.GetClosestFreeAimTargetPos();	
					vTarget.Subtract(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()));
					float fTargetDist2 = vTarget.XYMag2();			
					static dev_float s_fMinThrowPlaceDistance2 = 3.5f*3.5f;
					static dev_float s_fMinForcePlaceDistance2 = 1.0f*1.0f;
					if (fTargetDist2 < s_fMinForcePlaceDistance2)	
					{
						m_bThrowPlace = false;
						m_bPrimedButPlacing = true;
					}
					else if (fTargetDist2 < s_fMinThrowPlaceDistance2 || m_fThrowPitch < s_fMinPitchForPlacedProjectileThrow)		
					{
						m_bThrowPlace = true;	
						m_bPrimedButPlacing = false;
					}
					else
					{
						m_bThrowPlace = m_bPrimedButPlacing = false;
					}
				}
			}
		}
		else 
		{
			vStart = pPed->GetBonePositionCached((eAnimBoneTag)BONETAG_R_HAND);	
			if (m_target.GetIsValid())
			{
				m_target.GetPosition(vEnd);
				Assert(vEnd.FiniteElements()); // Attempting to track down an INF (B*682884)
			}
			else
				vEnd.Zero(); //?

			// Compute desired pitch angle value
			float fDesiredPitch = CTaskAimGun::CalculateDesiredPitch(pPed, vStart, vEnd);

			// Map the angle to the range 0.0->1.0
			m_fDesiredPitch = CTaskAimGun::ConvertRadianToSignal(fDesiredPitch, fSweepPitchMin, fSweepPitchMax, false);
			m_fDesiredThrowPitch = CTaskAimGun::ConvertRadianToSignal(fDesiredPitch, fThrowPitchMin, fSweepPitchMax, false);		
			
		}

		// Compute the final signal value
		static float PitchChangeRate = 1.0f; //TODO
		
		if (popIt)
		{
			m_fCurrentPitch = m_fDesiredPitch;
			m_fThrowPitch = m_fDesiredThrowPitch;
		} 
		else
		{
			bool bUseDesiredPitch = false;
#if ENABLE_GUN_PROJECTILE_THROW
			bUseDesiredPitch = m_bAimGunAndThrow;
#endif	//ENABLE_GUN_PROJECTILE_THROW
			if ((GetState() != State_ThrowProjectile || m_bHasThrownProjectile) && !bUseDesiredPitch) 
				m_fThrowPitch = CTaskAimGun::SmoothInput(m_fThrowPitch, m_fDesiredThrowPitch, PitchChangeRate); 
			else if (bUseDesiredPitch)
				m_fThrowPitch = m_fDesiredThrowPitch;
			m_fCurrentPitch = CTaskAimGun::SmoothInput(m_fCurrentPitch, m_fDesiredPitch, PitchChangeRate);
		}
	
		// Send the signal
		m_MoveNetworkHelper.SetFloat(ms_PitchPhase, m_fCurrentPitch);
		m_MoveNetworkHelper.SetFloat(ms_ThrowPitchPhase, m_fThrowPitch);
		//Displayf("AimPhase: %f, Desired: %f >>> PitchPhase: %f, Desired: %f", m_fCurrentHeading, fDesiredYaw*RtoD, m_fCurrentPitch, fDesiredPitch*RtoD);

#if FPS_MODE_SUPPORTED
		if(m_bFPSUseAimIKForWeapon && pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
		{
			if(m_pFPSWeaponMoveNetworkHelper && m_pFPSWeaponMoveNetworkHelper->IsNetworkActive())
			{
				m_pFPSWeaponMoveNetworkHelper->SetFloat(ms_PitchPhase, m_fCurrentPitch);
			}
		}
#endif

		//Torso IK:
		if (IsAiming(pPed))
		{		
			m_bHasTargetPosition = !vEnd.IsZero();
			if(m_bHasTargetPosition)
			{
				m_vTargetPosition = vEnd;
				pPed->GetIkManager().PointGunAtPosition(vEnd, -1.0f);
			}
			else
				pPed->GetIkManager().PointGunInDirection(pPed->GetCurrentHeading(), 0.0f);				
		}
	}
}

dev_float TIME_TO_THROW_LONG = 0.33f;

bool CTaskAimAndThrowProjectile::ShouldUseProjectileStrafeClipSet()
{
	return GUN_PROJECTILE_THROW_ONLY(!m_bAimGunAndThrow) FPS_MODE_SUPPORTED_ONLY(&& !GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false));
}

CTask::FSM_Return CTaskAimAndThrowProjectile::Start_OnUpdate(CPed* pPed)
{
	m_bAllowPriming = true;

	if(pPed && pPed->IsLocalPlayer() && pPed->GetWeaponManager())
	{
		const CWeaponInfo* pWeaponInfo = pPed->GetWeaponManager()->GetEquippedWeapon() ? pPed->GetWeaponManager()->GetEquippedWeapon()->GetWeaponInfo() : NULL;
		if(pWeaponInfo && pWeaponInfo->GetDropWhenCooked() && pPed->GetPedResetFlag(CPED_RESET_FLAG_StartProjectileTaskWithPrimingDisabled))
		{
			m_bAllowPriming = false;
		}
	}

	SetState(State_StreamingClips);
	return FSM_Continue;
}

void CTaskAimAndThrowProjectile::StreamingClips_OnEnter(CPed* UNUSED_PARAM(pPed))
{
	m_iClipSet = CLIP_SET_ID_INVALID;
	m_iStreamedClipSet = CLIP_SET_ID_INVALID;
	
	//load filters
	m_pFrameFilter = g_FrameFilterDictionaryStore.FindFrameFilter(CMovePed::s_UpperBodyFilterHashId);
	
	static fwMvFilterId s_GripFilterHashId("Grip_R_Only_Filter",0xB455BA3A);	
	m_pGripFrameFilter = g_FrameFilterDictionaryStore.FindFrameFilter(s_GripFilterHashId);

	static fwMvFilterId s_PullPinFilterHashId("BothArms_NoSpine_filter",0x6C2DD2B3);	
	m_pPullPinFilter = g_FrameFilterDictionaryStore.FindFrameFilter(s_PullPinFilterHashId);	

	//Set projectile strafe clipset	
	CTaskMotionBase* pTask = GetPed()->GetPrimaryMotionTask();
	taskAssert(pTask!=NULL);
	if (pTask && ShouldUseProjectileStrafeClipSet())
	{
		m_StrafeClipSetRequestHelper.Request(pTask->GetDefaultAimingStrafingClipSet(false));
	}

}

CTask::FSM_Return CTaskAimAndThrowProjectile::StreamingClips_OnUpdate(CPed* pPed)
{
#if FPS_MODE_SUPPORTED
	bool bUseFPSModeBehaviour = pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && m_bFPSUseAimIKForWeapon;
	bool bRNGTransitionOnStart = false;
	//If we are in starting up the task without having the drop/or throw flags, we want to make sure we play a transition first
	if(bUseFPSModeBehaviour && pPed->GetMotionData()->GetIsFPSRNG() && !m_bDropProjectile && !m_bFPSWantsToThrowWhenReady && !m_bPlayFPSTransition)
	{
		HandleFPSStateUpdate();

		if(m_bAllowPriming)
		{
			m_bPlayFPSTransition = true;
			bRNGTransitionOnStart = true;
		}
	}

	// B*2313574: Block camera switching when in State_StreamingClips (we already block when in State_Holding as task doesn't support being restarted).
	if(pPed->GetPlayerInfo())
	{
		if(!pPed->IsFirstPersonShooterModeEnabledForPlayer(false) || !pPed->GetMotionData()->GetIsFPSIdle())
		{
			pPed->SetPlayerResetFlag(CPlayerResetFlags::PRF_DISABLE_CAMERA_VIEW_MODE_CYCLE);
		}
	}
#endif

	if (m_iClipSet == CLIP_SET_ID_INVALID)
	{
		CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
		if(pWeapon)
		{
			const CWeaponInfo* pWeaponInfo = pWeapon->GetWeaponInfo();
			if (!pWeaponInfo || (!pWeaponInfo->GetIsThrownWeapon() GUN_PROJECTILE_THROW_ONLY(&& !m_bAimGunAndThrow)))
			{
				if(!pPed->IsNetworkClone())
				{
					SetState(State_Finish);
				}
				return FSM_Continue;
			}

			// If we're doing a gun grenade throw use the backup weapon clipset info
#if ENABLE_GUN_PROJECTILE_THROW
			if (m_bAimGunAndThrow)
			{
				// Only create the dummy weapon object once.
				if (!m_pTempWeaponObject)
				{
					CreateDummyWeaponObject(pPed, pWeapon, pWeaponInfo);
				}
				u32 uBackupWeaponHash = pPed->GetWeaponManager()->GetBackupWeapon();
				const CWeaponInfo *pBackupWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(uBackupWeaponHash);

				if (pBackupWeaponInfo)
				{
					pWeaponInfo = pBackupWeaponInfo;
					m_iClipSet = pWeaponInfo->GetAimGrenadeThrowClipsetId(*pPed);
				}
			}
			else
#endif	//ENABLE_GUN_PROJECTILE_THROW
			{
				FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault;

#if FPS_MODE_SUPPORTED
				if(bUseFPSModeBehaviour)
				{
					if(!m_bAllowPriming)
					{
						if(pPed->GetMotionData()->GetIsFPSRNG())
						{
							if(IsAiming(pPed))
							{
								fpsStreamingFlags = FPS_StreamLT;
							}
							else
							{
								fpsStreamingFlags = FPS_StreamIdle;
							}
						}
		
					}
					else if(m_bDropProjectile)
					{
						fpsStreamingFlags = FPS_StreamIdle;
					}
					else if(m_bFPSWantsToThrowWhenReady)
					{
						fpsStreamingFlags = FPS_StreamRNG;
					}
				}
#endif

				m_iClipSet = pWeaponInfo->GetAppropriateWeaponClipSetId(pPed, fpsStreamingFlags);

#if FPS_MODE_SUPPORTED
				//bool bSkipFPSTransition = true;
				if(bUseFPSModeBehaviour)
				{
					//If the fidget just ended or we want to switch FPS states (play a transition), do not grab the fidget clipset this update
					if(m_bMoveFPSFidgetEnded || m_bPlayFPSTransition)
					{
						bool bFidgetConfigFlag = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PlayFPSIdleFidgetsForProjectile);
						pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_PlayFPSIdleFidgetsForProjectile, false);
						m_iClipSet = pWeaponInfo->GetAppropriateWeaponClipSetId(pPed, fpsStreamingFlags);
						pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_PlayFPSIdleFidgetsForProjectile, bFidgetConfigFlag);
					}

					if(pPed->GetMotionData()->GetWasFPSUnholster())
					{
						m_bPlayFPSTransition = true;
					}

					bool bPlayUnholsterTransition = !pPed->GetPedResetFlag(CPED_RESET_FLAG_ForceSkipFPSAimIntro) && 
													!bRNGTransitionOnStart &&
													(pPed->GetPedResetFlag(CPED_RESET_FLAG_WasFPSJumpingWithProjectile) || pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_WasPlayingFPSGetup) || 
													 pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_WasPlayingFPSMeleeActionResult) || 
													 (GetPreviousState() == State_CombatRoll && !m_bFPSWantsToThrowWhenReady && !m_bDropProjectile) ||
													 (GetPreviousState() == State_ThrowProjectile && (pPed->GetMotionData()->GetIsFPSIdle() || !m_bAllowPriming)));

					if(bPlayUnholsterTransition)
					{
						m_bPlayFPSTransition = true;
					}

					if(pWeapon->GetIsCooking())
					{
						m_bPlayFPSTransition = false;
					}


					if(m_bPlayFPSTransition)
					{
							bool bJustThrownProjectileWhileAiming = GetPreviousState() == State_ThrowProjectile && IsAiming(pPed);
							bool bJustThrownProjectileRTOnly =  GetPreviousState() == State_ThrowProjectile && !IsAiming(pPed) && pPed->GetMotionData()->GetIsFPSRNG() && m_bAllowPriming;
						
							fwMvClipSetId clipsetId;
							
							FPSStreamingFlags fpsTransitionSet = FPS_StreamDefault;
							CPedMotionData::eFPSState prevFPSStateOverride = CPedMotionData::FPS_MAX;
							CPedMotionData::eFPSState currFPSStateOverride = CPedMotionData::FPS_MAX;
							m_bPlayFPSPullPinIntro = false;
							m_bPlayFPSRNGIntro = false;
							m_bFPSPlayingUnholsterIntro = false;
							
							if(!m_bAllowPriming)
							{
								if(pPed->GetMotionData()->GetIsFPSRNG())
								{
									if(IsAiming(pPed))
									{
										fpsTransitionSet = FPS_StreamLT;
										if(!m_bWasAiming)
										{
											prevFPSStateOverride = CPedMotionData::FPS_IDLE;
										}
										currFPSStateOverride = CPedMotionData::FPS_LT;

									}
									else
									{
										fpsTransitionSet = FPS_StreamIdle;
										if(m_bWasAiming)
										{
											prevFPSStateOverride = CPedMotionData::FPS_LT;
										}
										currFPSStateOverride = CPedMotionData::FPS_IDLE;
									}
								}
							}
							else if(bJustThrownProjectileWhileAiming)
							{
								fpsTransitionSet = FPS_StreamLT;
								prevFPSStateOverride = CPedMotionData::FPS_IDLE;
								currFPSStateOverride = CPedMotionData::FPS_LT;
							}
							else if(m_bDropProjectile || bJustThrownProjectileRTOnly)
							{
								if(bJustThrownProjectileRTOnly)
								{
									m_bDropProjectile = true;
								}

								fpsTransitionSet = FPS_StreamRNG;
								prevFPSStateOverride = CPedMotionData::FPS_IDLE;
								currFPSStateOverride = CPedMotionData::FPS_RNG;
							}
							else if(m_bFPSWantsToThrowWhenReady)
							{
								fpsTransitionSet = FPS_StreamRNG;
								prevFPSStateOverride = CPedMotionData::FPS_LT;
							    currFPSStateOverride = CPedMotionData::FPS_RNG;
							}

							if(bPlayUnholsterTransition)
							{
								prevFPSStateOverride = CPedMotionData::FPS_UNHOLSTER;
							}

							clipsetId = pWeaponInfo->GetAppropriateFPSTransitionClipsetId(*pPed, prevFPSStateOverride, fpsTransitionSet);
		
							if(clipsetId != CLIP_SET_ID_INVALID)
							{
								m_fFPSStateToTransitionBlendTime = SelectFPSBlendTime(pPed, true, prevFPSStateOverride, currFPSStateOverride);
								m_fFPSTransitionToStateBlendTime = SelectFPSBlendTime(pPed, false, prevFPSStateOverride, currFPSStateOverride);

								if(((m_bFPSWantsToThrowWhenReady || pPed->GetMotionData()->GetIsFPSRNG()) && !bJustThrownProjectileWhileAiming && m_bAllowPriming) || m_bDropProjectile)
								{
									m_bPlayFPSRNGIntro = true;
								}
								else if(prevFPSStateOverride == CPedMotionData::FPS_UNHOLSTER || pPed->GetMotionData()->GetWasFPSUnholster())
								{
									m_bFPSPlayingUnholsterIntro = true;
								}
								//If we have just thrown the projectile, we should pull the pin first and not just skip ahead to the throwing/dropping
								else if(pPed->GetMotionData()->GetIsFPSLT() || bJustThrownProjectileWhileAiming)
								{
									m_bPlayFPSPullPinIntro = true;
								}

								m_iClipSet = clipsetId;
							}
							else
							{
								m_bPlayFPSTransition = false;
							}
					}


					bool bCreatedNetworkHelperThisUpdate = false;
					CWeapon* pWeapon = (GetPed()->GetWeaponManager()) ? GetPed()->GetWeaponManager()->GetEquippedWeapon() : NULL;
					const CWeaponInfo* pWeaponInfo = pWeapon->GetWeaponInfo();
					if(pWeapon && pWeaponInfo && pWeaponInfo->GetHasFPSProjectileWeaponAnims() && !m_pFPSWeaponMoveNetworkHelper)
					{
						if(!pWeapon->GetFPSMoveNetworkHelper())
						{
							pWeapon->CreateFPSMoveNetworkHelper();
							bCreatedNetworkHelperThisUpdate = true;							
						}
						m_pFPSWeaponMoveNetworkHelper = pWeapon->GetFPSMoveNetworkHelper();
					}

					if(m_pFPSWeaponMoveNetworkHelper)
					{
						if(pWeapon)
						{
							pWeapon->CreateFPSWeaponNetworkPlayer();
							if(m_pFPSWeaponMoveNetworkHelper->IsNetworkActive())
							{
								static fwMvRequestId	s_WeaponHoldRequest("WeaponHolding",0x43D07274);
								static fwMvRequestId	s_WeaponIntroRequest("WeaponIntro",0x7DC2F8E9);
								static fwMvRequestId	s_WeaponPullPinRequest("PullPin",0x76E39996);

								static const fwMvClipId s_weaponHoldClipId("w_settle_med",0x150399A9);
								static const fwMvClipId s_weaponIntroClipId("w_aim_trans_med",0xEF3589E7);
								static const fwMvClipId s_weaponFidgetClipId("w_fidget_med_loop", 0x73C669F1);

								if(m_iClipSet != CLIP_SET_ID_INVALID)
								{
									m_pFPSWeaponMoveNetworkHelper->SetClipSet(m_iClipSet);

									if(m_bPlayFPSTransition && fwAnimManager::GetClipIfExistsBySetId(m_iClipSet, s_weaponIntroClipId))
									{
										m_pFPSWeaponMoveNetworkHelper->SendRequest(s_WeaponIntroRequest);
									}
									else if(fwAnimManager::GetClipIfExistsBySetId(m_iClipSet, s_weaponHoldClipId))
									{
										m_pFPSWeaponMoveNetworkHelper->SendRequest(s_WeaponHoldRequest);
										if(m_bDropProjectile && !m_bPlayFPSTransition)
										{
											m_pFPSWeaponMoveNetworkHelper->SendRequest(s_WeaponPullPinRequest);
										}
									}
									if(m_bPlayFPSFidget && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PlayFPSIdleFidgetsForProjectile) &&  fwAnimManager::GetClipIfExistsBySetId(m_iClipSet, s_weaponFidgetClipId))
									{
										m_pFPSWeaponMoveNetworkHelper->SendRequest(s_WeaponHoldRequest);
										m_pFPSWeaponMoveNetworkHelper->SendRequest(ms_FPSFidget);
									}
								}

								if(bCreatedNetworkHelperThisUpdate && pPed->GetWeaponManager())
								{
									CObject* pWeaponObj = pPed->GetWeaponManager()->GetEquippedWeaponObject();
									if(pWeaponObj)
									{
										pWeaponObj->ForcePostCameraAnimUpdate(true, true);
									}
								}
							}
						}
					}
				}
#endif
			}
			m_iStreamedClipSet = pWeaponInfo->GetAppropriateReloadWeaponClipSetId(pPed); //TODO: defer this streaming until later			
		}
		else if(!pPed->IsNetworkClone())
		{				
			SetState(State_Finish);		
			return FSM_Continue;
		}
	} 

	if (m_ClipSetRequestHelper.Request(m_iClipSet) && m_StreamedClipSetRequestHelper.Request(m_iStreamedClipSet))
	{
		if (pPed->IsNetworkClone())
		{
			if (GetStateFromNetwork() == State_PlaceProjectile)
			{
				SetState(State_PlaceProjectile);
			}
		}
		else if(!IsAiming(pPed) FPS_MODE_SUPPORTED_ONLY(|| (bUseFPSModeBehaviour && GetPreviousState() == State_Blocked)))
		{
			bool bFPSRNGRelease = false;
			bool bFPSRNG = false;
#if FPS_MODE_SUPPORTED
			// We may want to place a projectile so we enter this logic to check. We should not handle dropping here as that is handled by HandleFPSStateUpdate()
			if(bUseFPSModeBehaviour)
			{
				bFPSRNG = pPed->GetMotionData()->GetIsFPSRNG();
				bFPSRNGRelease =  !bFPSRNG && pPed->GetMotionData()->GetWasFPSRNG();
			}

#endif
			if (m_bCanBePlaced && (m_bPriming || CThrowProjectileHelper::IsPlayerPriming(pPed) || bFPSRNGRelease))
			{
				if(!bFPSRNGRelease) 
				{
					SetPriming(pPed, true);
					m_bDropProjectile = true;
				}

				if(IsFiring(pPed))
				{
					// Switch to move to state
					if(CanPlaceProjectile(pPed))
					{
						SetState(State_PlaceProjectile);
						m_bDropProjectile = false;
						m_bFPSWantsToThrowWhenReady = false;
						return FSM_Continue;
					}											
				}
				else
					return FSM_Continue;
			}
			else if (CThrowProjectileHelper::IsPlayerPriming(pPed) && m_bAllowPriming FPS_MODE_SUPPORTED_ONLY( && !bFPSRNG))
				m_bDropProjectile = true;
			else
			{
				CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
				if(pWeapon && pWeapon->GetIsCooking() FPS_MODE_SUPPORTED_ONLY( && !bFPSRNG))
				{
					m_bDropProjectile = true;
				}
			}
		}

		if (NetworkInterface::UseFirstPersonIdleMovementForRemotePlayer(*pPed))
		{
			SetState(State_CloneIdle);
			return FSM_Continue;
		}
	
		fwMvNetworkDefId networkDefId(CClipNetworkMoveInfo::ms_NetworkTaskAimAndThrowProjectile);


		float fBlendDuration = SLOW_BLEND_DURATION;
#if FPS_MODE_SUPPORTED
		if(bUseFPSModeBehaviour && m_fFPSStateToTransitionBlendTime >= 0.0f && m_fFPSTransitionToStateBlendTime >= 0.0f)
		{
			if(m_bPlayFPSTransition)
			{
				fBlendDuration = m_fFPSStateToTransitionBlendTime;
			}
			else
			{
				fBlendDuration = m_fFPSTransitionToStateBlendTime;
				m_fFPSStateToTransitionBlendTime = -1.0f;
				m_fFPSTransitionToStateBlendTime = -1.0f;
			}
		}

		if(bUseFPSModeBehaviour)
		{
			const CTaskMotionPed* pPedMotionTask = static_cast<const CTaskMotionPed*>(GetPed()->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_PED));

			if(!pPed->GetMotionData()->GetWasUsingFPSMode())
			{
				fBlendDuration = INSTANT_BLEND_DURATION;
			} // B* 2101014: Slow blend duration for blending in fidgets, makes it look better
			else if(m_bPlayFPSFidget)
			{
				const float fFidgetBlendInDuration = 0.4f;
				fBlendDuration = fFidgetBlendInDuration;
			}
			else if(pPedMotionTask && pPedMotionTask->GetFPSPreviouslySprinting())
			{
				TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE_PROJECTILE, fAimBlendInAfterSprint, 0.35f, 0.0f, 1.0f, 0.01f);
				fBlendDuration = fAimBlendInAfterSprint;
			}
			
			m_fFPSTaskNetworkBlendDuration = fBlendDuration;
			m_fFPSTaskNetworkBlendTimer = 0.0f;

			if(GetPreviousState() == State_CombatRoll || GetPreviousState() == State_ThrowProjectile || GetPreviousState() == State_PlaceProjectile)
			{
				fBlendDuration *= 0.7f; //Speed up the blend a bit
				//We DON't want to pause transition playback until the network has fully blended in
				m_fFPSTaskNetworkBlendTimer = fBlendDuration;
			}
		}
#endif 
		CreateNetworkPlayer(networkDefId);
		pPed->GetMovePed().SetTaskNetwork(m_MoveNetworkHelper, fBlendDuration, CMovePed::Task_TagSyncContinuous);	
		m_MoveNetworkHelper.SetClipSet(m_iClipSet);
		m_MoveNetworkHelper.SetClipSet(m_iStreamedClipSet, ms_StreamedClipSet); //TODO: defer this streaming until later

#if FPS_MODE_SUPPORTED
		if(bUseFPSModeBehaviour)
		{
			m_MoveNetworkHelper.SetFlag(true, ms_UsingFPSMode);
			m_MoveNetworkHelper.SetFloat(ms_AimingLeanAdditiveWeight, 1.0f);
			m_MoveNetworkHelper.SetFloat(ms_AimingBreathingAdditiveWeight, 1.0f);
			
			CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
			if(pWeapon && pWeapon->GetWeaponInfo() && pWeapon->GetWeaponInfo()->GetCookWhileAiming() &&
			   (pWeapon->GetDamageType() == DAMAGE_TYPE_EXPLOSIVE) && m_bDropProjectile && !m_bPlayFPSTransition)
			{
				m_MoveNetworkHelper.SetFlag(true, ms_FPSCookingProjectile);
			}

			if(!pPed->GetMotionData()->GetWasUsingFPSMode() && pPed->GetPedResetFlag(CPED_RESET_FLAG_ForcePreCameraAnimUpdate))
			{
				ProcessPostPreRender();
			}
		}
#endif

		CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
		if (!IsAiming(pPed) && IsPriming(pPed))
			m_bMoveTreeIsDropMode = true;

		if (IsAiming(pPed) && !IsPriming(pPed))
			m_bFalseAiming = true;

		static fwMvFilterId s_RightArmHashId("RightArm_NoSpine_filter",0x72BC0F25);			
		m_pFrameFilter = g_FrameFilterDictionaryStore.FindFrameFilter(m_bMoveTreeIsDropMode ? s_RightArmHashId : CMovePed::s_UpperBodyFilterHashId);		

		SetGripClip();				

		if(pWeapon)
		{
#if FPS_MODE_SUPPORTED
			if(bUseFPSModeBehaviour)
			{
				Vector3 vTargetPos(0.01f, 0.01f, 0.01f);
				if(CTaskWeaponBlocked::IsWeaponBlocked(pPed, vTargetPos) && GetPreviousState() != State_Blocked && !m_bAimGunAndThrow)
				{
					m_bPlayFPSTransition = false;
					SetState(State_Blocked);
					return FSM_Continue;
				}
				else if(m_bPlayFPSTransition == false && !m_bAimGunAndThrow)
				{
					m_MoveNetworkHelper.SetFlag(true, ms_SkipIntroId);
					m_bPlayFPSTransition = false;
					SetState(State_HoldingProjectile);
					return FSM_Continue;
				}
			}
#endif
			// in case the last projectile blew up in my hands
			if(pWeapon->GetNeedsToReload(true)) 
			{ 
				pPed->GetWeaponManager()->DestroyEquippedWeaponObject();
				pPed->GetWeaponManager()->CreateEquippedWeaponObject();
			}

#if ENABLE_GUN_PROJECTILE_THROW
			//Throw automatically if we are coming directly from the aim gun task
			if (m_bAimGunAndThrow)
			{
				SetState(State_ThrowProjectile);
				return FSM_Continue;
			}
#endif	//ENABLE_GUN_PROJECTILE_THROW

			SetState(State_Intro);
		} 
		else
			SetState(State_Finish);
	}

	return FSM_Continue;
}	

void CTaskAimAndThrowProjectile::Intro_OnEnter(CPed* pPed)
{	
	if(pPed->IsNetworkClone())
	{
		CNetObjPed* pNetObjPed = static_cast<CNetObjPed*>(pPed->GetNetworkObject());
		if (pNetObjPed)
		{
			// make sure the ped is holding the correct weapon first
			pNetObjPed->SetTaskOverridingPropsWeapons(false);
			pNetObjPed->UpdateCachedWeaponInfo(true);

			pNetObjPed->SetTaskOverridingPropsWeapons(true);
		}
	}

	m_bPriming = false;	
	m_bIsFiring = false;

	m_MoveNetworkHelper.WaitForTargetState(ms_EnterIntroEvent);		
}

dev_float ReachPinPhaseBeforeWallBlock = 0.567f;
CTask::FSM_Return CTaskAimAndThrowProjectile::Intro_OnUpdate(CPed* pPed)
{
	if(!m_MoveNetworkHelper.IsInTargetState())
		return FSM_Continue;

#if FPS_MODE_SUPPORTED
	if(HandleFPSStateUpdate() && m_bFPSUseAimIKForWeapon)
	{
		SetState(State_StreamingClips);
		return FSM_Continue;
	}
#endif

	float fPhase = m_MoveNetworkHelper.GetFloat(ms_TransitionPhase);
	static dev_float sf_IntroPhaseOut = 0.8f;

	bool bFPSReleaseOnReachPin = false;
	bool bFPSAimIntroTransition = false;
	bool bUseFPSModeBehaviour = pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && m_bFPSUseAimIKForWeapon;

#if FPS_MODE_SUPPORTED
	if(bUseFPSModeBehaviour)
	{
		Vector3 vTargetPos(0.1f, 0.1f, 0.1f);
		bool bIsFPSBlocked = CTaskWeaponBlocked::IsWeaponBlocked(pPed, vTargetPos);
		if(bIsFPSBlocked)
		{
			if (fPhase >= ReachPinPhaseBeforeWallBlock)
			{
				if(m_bFPSUseAimIKForWeapon && (m_bFPSWantsToThrowWhenReady || m_bDropProjectile))
				{
					CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
					if(pWeapon && !pWeapon->GetIsCooking() && pWeapon->GetWeaponInfo() && pWeapon->GetWeaponInfo()->GetCookWhileAiming()) //don't pre cook weapons that won't show in UI
						pWeapon->StartCookTimer(fwTimer::GetTimeInMilliseconds());	
				}		
			}

			// Our weapon is blocked, so don't go into aiming or firing
			SetState(State_Blocked);
			return FSM_Continue;
		}

		m_bFPSReachPinOnIntro |= m_MoveNetworkHelper.GetBoolean(ms_FPSReachPin);
		bFPSReleaseOnReachPin = m_bFPSReachPinOnIntro && (m_bFPSWantsToThrowWhenReady || (m_bDropProjectile && !pPed->GetMotionData()->GetIsFPSRNG()));
		bFPSAimIntroTransition = m_MoveNetworkHelper.GetBoolean(CTaskMotionAiming::ms_FPSAimIntroTransitionId);

		TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE_PROJECTILE, fProjectileIntroPlayRate, 1.0f, 0.0f, 5.0f, 0.1f);
		float fIntroPlayRate = fProjectileIntroPlayRate;
		bool bTaskNetworkBlendedIn = m_fFPSTaskNetworkBlendTimer > m_fFPSTaskNetworkBlendDuration;

		if(!bTaskNetworkBlendedIn && m_bFPSPlayingUnholsterIntro)
		{
			fIntroPlayRate = 0.0f;
		}

		m_MoveNetworkHelper.SetFloat(CTaskAimGunOnFoot::ms_FPSIntroPlayRate, fIntroPlayRate);
	}
#endif

	if (fPhase >= sf_IntroPhaseOut || bFPSReleaseOnReachPin || bFPSAimIntroTransition)
	{
#if FPS_MODE_SUPPORTED
		if(m_bPlayFPSTransition && m_bFPSUseAimIKForWeapon)
		{
			//Restart again to change clipsets
			m_bPlayFPSTransition = false;

			if(m_bPlayFPSPullPinIntro && m_bFPSWantsToThrowWhenReady)
			{
				m_bPlayFPSTransition = true;
			}

			SetState(State_StreamingClips);
			return FSM_Continue;
		}
#endif
		m_MoveNetworkHelper.SendRequest(ms_StartHold);
		SetState(State_HoldingProjectile);
		return FSM_Continue;
	}

	if (CThrowProjectileHelper::IsPlayerRolling(pPed))
	{
		SetState(State_CombatRoll);
		return FSM_Continue;
	}

	if (pPed->IsNetworkClone() && GetStateFromNetwork() == State_PlaceProjectile)
	{
		SetState(State_PlaceProjectile);
	}

	//Head IK	
	if (GetPed()->IsLocalPlayer() FPS_MODE_SUPPORTED_ONLY(&& !bUseFPSModeBehaviour))
		GetPed()->GetIkManager().LookAt(0, NULL, 1000, BONETAG_INVALID, NULL, LF_USE_CAMERA_FOCUS|LF_WHILE_NOT_IN_FOV, 500, 500, CIkManager::IK_LOOKAT_HIGH);

	CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();

#if FPS_MODE_SUPPORTED
	bool bFPSUseModeBehaviour = m_bFPSUseAimIKForWeapon && pPed->IsFirstPersonShooterModeEnabledForPlayer(false);
#endif

	bool bProceed = m_bDropProjectile && m_bAllowPriming; 

	if (!pPed->IsNetworkClone())
	{
		bProceed |= (CThrowProjectileHelper::IsPlayerPriming(pPed) && m_bAllowPriming) || (pWeapon && pWeapon->GetIsCooking());
	}

	if (bProceed FPS_MODE_SUPPORTED_ONLY(&& !bFPSUseModeBehaviour))
	{
		if (!pPed->IsNetworkClone())
		{
			// start priming
			SetPriming(pPed, true);

			// weapon manager verified in PreProcessFSM
			if (pWeapon && pWeapon->GetWeaponInfo()->GetCookWhileAiming())
			{
				if(!pWeapon->GetIsCooking())
					pWeapon->StartCookTimer(fwTimer::GetTimeInMilliseconds());	
				else
					m_bPinPulled = true;
			}
		}

		m_MoveNetworkHelper.SetFlag(m_bDropProjectile, ms_DropProjectileFlagId);				
		m_MoveNetworkHelper.SetFlag(m_bPinPulled, ms_SkipPullPinId);			
		if (m_bDropProjectile)
		{
			m_MoveNetworkHelper.SendRequest(ms_StartDropPrime);
			m_bPinPulled = true;
			SetState(State_HoldingProjectile);
		}
		else
		{
			if (m_bPrimedButPlacing)
			{
				m_MoveNetworkHelper.SendRequest(ms_StartHold);
				SetState(State_HoldingProjectile);
			} 
			else 
			{		
				m_MoveNetworkHelper.SendRequest(ms_StartPrime);			
				if (!m_bPinPulled)
				{
					m_bPinPulled = true;
					SetState(State_PullPin);
				}
				else 
				{
					SetState(State_HoldingProjectile);
				}
			}
		}
	}
	return FSM_Continue;
}

#if FPS_MODE_SUPPORTED
void CTaskAimAndThrowProjectile::Intro_OnExit(CPed *pPed)
{
	if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && m_bFPSUseAimIKForWeapon)
	{
		m_bPlayFPSPullPinIntro = false;
		m_bFPSReachPinOnIntro = false;
		m_bPlayFPSRNGIntro = false;
		m_bFPSPlayingUnholsterIntro = false;
	}
}
#endif


CTask::FSM_Return CTaskAimAndThrowProjectile::PullPin_OnUpdate()
{
	float fPinPhase = 0.0f;
	m_MoveNetworkHelper.GetFloat(ms_PullPinPhase, fPinPhase);
	if (fPinPhase >= 0.8f)
	{
		if (m_fThrowPitch < sm_Tunables.m_fMinHoldThrowPitch)
			m_MoveNetworkHelper.SendRequest(ms_StartHold);
		else
			m_MoveNetworkHelper.SendRequest(ms_StartPrime);
		m_MoveNetworkHelper.SetFlag(m_bPinPulled, ms_SkipPullPinId);	
		SetState(State_HoldingProjectile);
	}

	if (CThrowProjectileHelper::IsPlayerRolling(GetPed()))
	{
		SetState(State_CombatRoll);
		return FSM_Continue;
	}
	return FSM_Continue;
}

void CTaskAimAndThrowProjectile::HoldingProjectile_OnEnter(CPed* FPS_MODE_SUPPORTED_ONLY(pPed))
{
	if (GetPreviousState() != State_CombatRoll)
		m_MoveNetworkHelper.WaitForTargetState(ms_EnterHoldEvent);	

#if FPS_MODE_SUPPORTED
	if(m_bPlayFPSFidget && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PlayFPSIdleFidgetsForProjectile))
	{
		m_MoveNetworkHelper.SendRequest(ms_FPSFidget);
	}
	else
	{
		m_bPlayFPSFidget = false;
	}
#endif
}

CTask::FSM_Return CTaskAimAndThrowProjectile::HoldingProjectile_OnUpdate(CPed* pPed)
{
#if FPS_MODE_SUPPORTED
	if(pPed->GetPlayerInfo())
	{
		if(!pPed->IsFirstPersonShooterModeEnabledForPlayer(false) || !pPed->GetMotionData()->GetIsFPSIdle())
		{
			pPed->SetPlayerResetFlag(CPlayerResetFlags::PRF_DISABLE_CAMERA_VIEW_MODE_CYCLE);
		}
	}
#endif // FPS_MODE_SUPPORTED

	if(!m_MoveNetworkHelper.IsInTargetState())
		return FSM_Continue;
	
#if FPS_MODE_SUPPORTED
	if(HandleFPSStateUpdate() && m_bFPSUseAimIKForWeapon)
	{
		SetState(State_StreamingClips);
		return FSM_Continue;
	}
	else if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && m_bFPSUseAimIKForWeapon)
	{
		bool bFPSRestart = false;

		if((pPed->GetMotionData()->GetIsFPSIdle() || pPed->GetMotionData()->GetIsFPSLT()) && !m_bFPSWantsToThrowWhenReady && 
			!m_bDropProjectile && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PlayFPSIdleFidgetsForProjectile) && !m_bPlayFPSFidget)
		{
			m_bPlayFPSTransition = false;
			m_bPlayFPSFidget = true;
			bFPSRestart = true;
		}

		m_bMoveFPSFidgetEnded |= m_MoveNetworkHelper.GetBoolean(CTaskAimGunOnFoot::ms_FidgetClipEndedId);

		//Continue for a frame after fidget ends so that TaskMotionPed can check the "clip ended" tag
		if(m_bPlayFPSFidget && m_bMoveFPSFidgetEnded)
		{
			return FSM_Continue;
		}
		else if (m_bPlayFPSFidget && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PlayFPSIdleFidgetsForProjectile))
		{
			m_bPlayFPSFidget = false;
			bFPSRestart = true;
		}

		if(bFPSRestart)
		{
			SetState(State_StreamingClips);
			return FSM_Continue;
		}

		Vector3 vTargetPos(0.1f, 0.1f, 0.1f);
		bool bIsBlocked = CTaskWeaponBlocked::IsWeaponBlocked(pPed, vTargetPos);
		if(bIsBlocked)
		{
			CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
			if(m_bFPSUseAimIKForWeapon && (m_bFPSWantsToThrowWhenReady || m_bDropProjectile))
			{
				if(pWeapon && !pWeapon->GetIsCooking() && pWeapon->GetWeaponInfo() && pWeapon->GetWeaponInfo()->GetCookWhileAiming()) //don't pre cook weapons that won't show in UI
					pWeapon->StartCookTimer(fwTimer::GetTimeInMilliseconds());	
			}

			// Our weapon is blocked, so don't go into aiming or firing

			m_bPlayFPSFidget = false;
			SetState(State_Blocked);
			return FSM_Continue;
		}
	}
#endif

	if(!pPed->IsPlayer())
	{
		// just throw at target
		SetState(State_ThrowProjectile);	
		return FSM_Continue;
	}

	// weapon manager verified in PreProcessFSM
	CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();

	//Emulate left trigger if started this way
	static dev_float s_fMinHoldTime = 0.0f;
	if (m_bFalseAiming && GetTimeInState() > s_fMinHoldTime)
		m_bFalseAiming = false;

	if (CThrowProjectileHelper::IsPlayerRolling(pPed))
	{
		SetState(State_CombatRoll);
		return FSM_Continue;
	}
	
	bool bReadyToThrow = m_bPriming;

#if FPS_MODE_SUPPORTED
	if (!bReadyToThrow && pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true))
	{
		if (pPed->IsNetworkClone())
		{
			bReadyToThrow = (m_bDropProjectile || m_bIsFiring);
		}
		else
		{
			bReadyToThrow = m_bFPSUseAimIKForWeapon && (m_bDropProjectile || m_bFPSWantsToThrowWhenReady);
		}
	}
#endif 

	if (bReadyToThrow)
	{	
		pPed->SetPedResetFlag( CPED_RESET_FLAG_TemporarilyBlockWeaponSwitching, true );

#if FPS_MODE_SUPPORTED
		if(m_bFPSUseAimIKForWeapon && (m_bFPSWantsToThrowWhenReady || m_bDropProjectile))
		{
			if(pWeapon && !pWeapon->GetIsCooking() && pWeapon->GetWeaponInfo() && pWeapon->GetWeaponInfo()->GetCookWhileAiming()) //don't pre cook weapons that won't show in UI
				pWeapon->StartCookTimer(fwTimer::GetTimeInMilliseconds());	
		}
#endif

		// check for drop
		if(m_bDropProjectile)
		{
			if(IsFiring(pPed))
			{			
				// Check to see if this is placeable
				if((pWeapon && pWeapon->GetWeaponInfo() && pWeapon->GetWeaponInfo()->GetFlag(CWeaponInfoFlags::PlacedOnly)) || (m_bCanBePlaced && CanPlaceProjectile(pPed)))
				{
					// Switch to move to state
					SetState(State_PlaceProjectile);
					return FSM_Continue;
				}

				SetState(State_ThrowProjectile);	
				return FSM_Continue;
			} 

			if(pPed->IsNetworkClone())
			{
				ProjectileInfo* pProjectileInfo = CNetworkPendingProjectiles::GetProjectile(pPed, GetNetSequenceID(), false);
				if(pProjectileInfo)
				{
					SetState(State_ThrowProjectile);
				}
				return FSM_Continue;
			}
		}
		
		if(IsAiming(pPed) FPS_MODE_SUPPORTED_ONLY(&& (!m_bFPSUseAimIKForWeapon || !pPed->IsFirstPersonShooterModeEnabledForPlayer(false))))
		{
			m_MoveNetworkHelper.SetFlag(false, ms_DropProjectileFlagId);			
			if (!m_bMoveTreeIsDropMode)
			{
				if (m_bPrimedButPlacing || ((pWeapon && pWeapon->GetWeaponInfo()->GetFlag(CWeaponInfoFlags::PlacedOnly)) || m_fThrowPitch < sm_Tunables.m_fMinHoldThrowPitch))
					m_MoveNetworkHelper.SendRequest(ms_StartHold);
				else
					m_MoveNetworkHelper.SendRequest(ms_StartPrime);
				m_MoveNetworkHelper.SetFlag(m_bPinPulled, ms_SkipPullPinId);
			}
			else
			{
				HoldPrimeMoveNetworkRestart(pPed, true);
			}
				
			m_bDropProjectile = false;	
		}
		m_MoveNetworkHelper.SetFloat(ms_ShadowWeight, 1.0f);

		// only valid on local players
		if(!pPed->IsNetworkClone())
		{
			if(pWeapon)
			{
				Assert(pWeapon->GetWeaponInfo());
				if(pWeapon->GetWeaponInfo() && pWeapon->GetWeaponInfo()->GetProjectileFuseTime() > 0.0f && pWeapon->GetCookTimeLeft(pPed, fwTimer::GetTimeInMilliseconds()) <= 0)
				{
					if (pWeapon->GetWeaponInfo()->GetDropWhenCooked())
					{
						SetState(State_ThrowProjectile);
						m_bAllowPriming = false;
						m_bDropProjectile = true;
						return FSM_Continue;
					}						
					else
					{
						ThrowProjectileNow(pPed, pWeapon);
						m_bWaitingToDie = true;
						m_MoveNetworkHelper.SendRequest(ms_StartFinish);	
					}
		
				}
			}
			else if (!m_bWaitingToDie)
			{
				// The equipped weapon (which existed when this sequence started) has been taken away. bail...
				SetState(State_Finish);
				return FSM_Continue;
			}
		}

		bool bSwitchedOutOfRNG = false;

#if FPS_MODE_SUPPORTED
		if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && m_bFPSUseAimIKForWeapon)
		{
			bSwitchedOutOfRNG = pPed->GetMotionData()->GetWasFPSRNG() && !pPed->GetMotionData()->GetIsFPSRNG();
		}
#endif
		// player is firing, move to thrown
		if(IsFiring(pPed) || bSwitchedOutOfRNG)
		{
			if (m_bPrimedButPlacing && CanPlaceProjectile(pPed))
			{
				// just place
				SetState(State_PlaceProjectile);
				return FSM_Continue;
			}
			else if(!pPed->IsNetworkClone())
			{
				//Test for placed-only weapons
				if (pWeapon && pWeapon->GetWeaponInfo() && pWeapon->GetWeaponInfo()->GetFlag(CWeaponInfoFlags::PlacedOnly))
				{
					SetState(State_PlaceProjectile);
				}
				else
				{
					m_bIsFiring = true;

					//make sure I'm facing forward
					static dev_float s_fMaxHeadingDelta = PI*0.25f;
					const float fCurrentHeading = fwAngle::LimitRadianAngleSafe(pPed->GetCurrentHeading());
					const float fDesiredHeading = fwAngle::LimitRadianAngleSafe(pPed->GetDesiredHeading());
					const float	fHeadingDelta = SubtractAngleShorter(fDesiredHeading, fCurrentHeading);

					if (fabs(fHeadingDelta) < s_fMaxHeadingDelta)
					{
						SetState(State_ThrowProjectile);
					}
				}
			}
			else
			{
				ProjectileInfo* pProjectileInfo = CNetworkPendingProjectiles::GetProjectile(pPed, GetNetSequenceID(), false);
				if(pProjectileInfo)
				{
					if (pWeapon && pWeapon->GetWeaponInfo() && pWeapon->GetWeaponInfo()->GetFlag(CWeaponInfoFlags::PlacedOnly))
					{
						SetState(State_PlaceProjectile);
					}
				}

				if(GetStateFromNetwork() == State_ThrowProjectile)
				{
					SetState(State_ThrowProjectile);
				}
			}
		}
	} 
	else if(!pPed->IsNetworkClone() && CThrowProjectileHelper::IsPlayerPriming(pPed) && m_bAllowPriming) //Not priming but should
	{
		// start priming
		SetPriming(pPed, true);

		if(pWeapon && !pWeapon->GetIsCooking() && pWeapon->GetWeaponInfo() && pWeapon->GetWeaponInfo()->GetCookWhileAiming()) //don't pre cook weapons that won't show in UI
			pWeapon->StartCookTimer(fwTimer::GetTimeInMilliseconds());	
			
		if(CPlayerInfo::IsAiming() || (m_bFalseAiming && !m_bDropProjectile)) 
		{	
			if (m_bPrimedButPlacing)
				m_MoveNetworkHelper.SendRequest(ms_StartHold);
			else
			{
				if (m_bPinPulled && ((pWeapon && pWeapon->GetWeaponInfo() && pWeapon->GetWeaponInfo()->GetFlag(CWeaponInfoFlags::PlacedOnly)) || m_fThrowPitch < sm_Tunables.m_fMinHoldThrowPitch))
					m_MoveNetworkHelper.SendRequest(ms_StartHold);
				else
					m_MoveNetworkHelper.SendRequest(ms_StartPrime);

				if (!m_bPinPulled)
				{
					SetState(State_PullPin);
				}
				m_MoveNetworkHelper.SetFlag(false, ms_DropProjectileFlagId);			
				m_MoveNetworkHelper.SetFlag(m_bPinPulled, ms_SkipPullPinId);	
				m_bPinPulled = true;
			}
		}
		else
		{			
			m_bDropProjectile = true;
		}
	}

	if(pPed->IsNetworkClone())
	{
#if FPS_MODE_SUPPORTED
		if (NetworkInterface::UseFirstPersonIdleMovementForRemotePlayer(*pPed))
		{
			SetState(State_CloneIdle);
			return FSM_Continue;
		}
#endif

		// catch the early out case, tracked below for local peds
		if(GetStateFromNetwork() == State_Finish)
		{
			SetState(State_Finish);
			m_MoveNetworkHelper.SendRequest(ms_StartFinish);
		}
		// catch the place projectile state
		else if(GetStateFromNetwork() == State_PlaceProjectile || GetStateFromNetwork() == State_Outro)
			SetState(GetStateFromNetwork());
	}
	else
	{
		const CWeaponInfo* pWeaponInfo = pPed->GetWeaponManager()->GetEquippedWeaponInfo();
		if((!IsAiming(pPed) && GetState() == State_HoldingProjectile) || (pWeaponInfo && !pWeaponInfo->GetIsThrownWeapon()))
		{
			if(IsPriming(pPed) && m_bAllowPriming && (!m_bCanBePlaced || pPed->GetMotionData()->GetDesiredMbrY() >= MOVEBLENDRATIO_WALK))
			{
				if ( GetTimeInState() > s_fMinHoldTime)
				{
					if (!m_bMoveTreeIsDropMode)
						HoldPrimeMoveNetworkRestart(pPed, false);
					m_bDropProjectile = true;
				}				
			}
			else
			{
				if (!m_bFalseAiming FPS_MODE_SUPPORTED_ONLY ( && (!m_bFPSUseAimIKForWeapon || !pPed->IsFirstPersonShooterModeEnabledForPlayer(false))))
				{
					SetState(State_Outro);
				}
			}
		}

		if (pPed->GetWeaponManager()->GetRequiresWeaponSwitch())
		{
			SetState(State_Outro);
		}
	}

	return FSM_Continue;
}

void CTaskAimAndThrowProjectile::HoldPrimeMoveNetworkRestart(CPed* pPed, bool bHoldToPrime)
{
	pPed->GetMovePed().ClearTaskNetwork(m_MoveNetworkHelper, NORMAL_BLEND_DURATION);
	fwMvNetworkDefId networkDefId(CClipNetworkMoveInfo::ms_NetworkTaskAimAndThrowProjectile);
	CreateNetworkPlayer(networkDefId);
	m_MoveNetworkHelper.SetFlag(true, ms_SkipIntroId);
	m_MoveNetworkHelper.SetFlag(m_bPinPulled, ms_SkipPullPinId);
	if (bHoldToPrime)
	{
		m_MoveNetworkHelper.SendRequest(ms_StartPrime);
		if (!m_bPinPulled)
		{
			SetState(State_PullPin);
		}
		m_bPinPulled = true;
		m_bMoveTreeIsDropMode = false;
	}
	else
	{
		m_MoveNetworkHelper.SendRequest(ms_StartHold);
		m_bMoveTreeIsDropMode = true;
	}
	pPed->GetMovePed().SetTaskNetwork(m_MoveNetworkHelper, SLOW_BLEND_DURATION, CMovePed::Task_TagSyncContinuous);	
	m_MoveNetworkHelper.SetClipSet(m_iClipSet);	
	m_MoveNetworkHelper.SetClipSet(m_iStreamedClipSet, ms_StreamedClipSet); 

	static fwMvFilterId s_RightArmHashId("RightArm_NoSpine_filter",0x72BC0F25);
	m_pFrameFilter = g_FrameFilterDictionaryStore.FindFrameFilter(bHoldToPrime ? CMovePed::s_UpperBodyFilterHashId : s_RightArmHashId);
	if(m_pFrameFilter)
		m_MoveNetworkHelper.SetFilter(m_pFrameFilter, ms_FrameFilterId);

	SetGripClip();
}

void CTaskAimAndThrowProjectile::SetPriming(CPed* pPed, bool bPrime)
{
	m_bPriming = bPrime;
	CProjectile* pProjectile = static_cast<CProjectile*>(pPed->GetWeaponManager()->GetEquippedWeaponObject());
	if (pProjectile)
		pProjectile->SetPrimed(bPrime);
}

void CTaskAimAndThrowProjectile::SetGripClip()
{
	//See if grip anim exists
	static fwMvFlagId sUseGripClip("UseGripClip",0x72838804);
	const fwClipSet *pClipSet = m_MoveNetworkHelper.GetClipSet();

	bool bSetGripClip = true;
#if FPS_MODE_SUPPORTED
	if(m_bFPSUseAimIKForWeapon && GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		bSetGripClip = false;
		if(GetState() == State_ThrowProjectile)
		{
			m_MoveNetworkHelper.SetFlag(false, sUseGripClip);
		}
	}
#endif

	if(bSetGripClip)
	{
		if (pClipSet && pClipSet->GetClip(fwMvClipId("grip",0x6FAD1258)) != NULL)
			m_MoveNetworkHelper.SetFlag(true, sUseGripClip);			
		else
			m_MoveNetworkHelper.SetFlag(false, sUseGripClip);
	}
}

void CTaskAimAndThrowProjectile::ThrowProjectile_OnEnter(CPed* pPed)
{
	m_bHasThrownProjectile = false;	
	m_bHasReloaded = false;	
	m_vLastProjectilePos.Zero();
	bool bOverHandOnly = m_bCanBePlaced;	
#if FPS_MODE_SUPPORTED
	m_bFPSWantsToThrowWhenReady = false;
	m_fFPSDropThrowSwitchTimer = -1.0f;
#endif
	CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
	float fTargetDist2 = 0;
	Vector3 vTarget(Vector3::ZeroType);
	if(pWeapon)
	{
		if(pWeapon->GetWeaponInfo()->GetCookWhileAiming())
			pPed->GetPedAudioEntity()->HandleAnimEventFlag("GRENADE_PULL_PIN");		

		if( !m_bCanBePlaced && pPed->IsLocalPlayer()) //if target is too close, just drop it
		{
			CPlayerPedTargeting &playerTargeting = pPed->GetPlayerInfo()->GetTargeting();
			if (playerTargeting.GetLockOnTarget())
				vTarget = playerTargeting.GetLockonTargetPos();
			else 
				vTarget = playerTargeting.GetClosestFreeAimTargetPos();

#if __BANK
			CTask::ms_debugDraw.AddSphere(RCC_VEC3V(vTarget), 0.3f, Color_green, 1000);
#endif 					
			vTarget.Subtract(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()));
			fTargetDist2 = vTarget.XYMag2();			

			static dev_float s_fMinThrowDistance2 = 1.5f*1.5f;
			if (fTargetDist2 < s_fMinThrowDistance2 && !pWeapon->GetWeaponInfo()->GetThrowOnly())
				m_bDropProjectile = true;			
		}
	}

	m_bForceStandingThrow = false;
	if (pPed->IsLocalPlayer())
	{
		//Ground check, if missing play throw while standing still	
		static dev_float s_fForwardGroundProbeDepth = 2.5f;
		Vector3 vTestStart = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		Vector3 vDirection = VEC3V_TO_VECTOR3(pPed->GetTransform().GetForward());
		vDirection.RotateZ(m_fStrafeDirection);
		vTestStart.AddScaled(vDirection, s_fForwardGroundProbeDepth);	
		
		//vTestStart.z += 0.5f;
		static dev_float s_fDownGroundProbeDepth = 2.0f;
		Vector3 vTestEnd = vTestStart;
		vTestEnd.z -= s_fDownGroundProbeDepth;
#if __BANK
		CTask::ms_debugDraw.AddLine(RCC_VEC3V(vTestStart), RCC_VEC3V(vTestEnd), Color_yellow, 0);
#endif 		

		WorldProbe::CShapeTestHitPoint probeIsect;
		WorldProbe::CShapeTestResults probeResult(probeIsect);
		WorldProbe::CShapeTestProbeDesc probeDesc;
		probeDesc.SetStartAndEnd(vTestStart, vTestEnd);
		probeDesc.SetResultsStructure(&probeResult);
		probeDesc.SetExcludeEntity(pPed);
		probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_OBJECT_TYPE);
		probeDesc.SetContext(WorldProbe::LOS_GeneralAI);
		
		if(!WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
		{
			m_bForceStandingThrow = true;
		}
	}

	pPed->GetMovePed().ClearTaskNetwork(m_MoveNetworkHelper, NORMAL_BLEND_DURATION);
	fwMvNetworkDefId networkDefId(CClipNetworkMoveInfo::ms_NetworkTaskAimAndThrowProjectileThrow);
	CreateNetworkPlayer(networkDefId);

#if ENABLE_GUN_PROJECTILE_THROW
	// Disable projectile drop from AimGunAndThrow
	if (m_bAimGunAndThrow && m_bDropProjectile)
	{
		m_bDropProjectile = false;
	}

	if (m_bAimGunAndThrow && m_bThrowPlace)
	{
		m_bThrowPlace = false;
	}
#endif	//ENABLE_GUN_PROJECTILE_THROW

	if (m_bDropProjectile)
	{
		static fwMvFilterId s_UpperNoLeftArmPartialHeadHashId("UpperbodyFeathered_NoLeftArm_PartialHead_filter",0x7D55CA4B);	

		fwMvFilterId userFilterId = s_UpperNoLeftArmPartialHeadHashId;
#if FPS_MODE_SUPPORTED
		if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
		{
			static const fwMvFilterId s_RootUpperbodyFilterHashId("RootUpperbody_filter",0x503C54E9);	
			userFilterId = s_RootUpperbodyFilterHashId;
		}
#endif
		pPed->GetMovePed().SetTaskNetwork(m_MoveNetworkHelper, NORMAL_BLEND_DURATION, CMovePed::Task_None, userFilterId);	
	}	
	else
	{
		float fBlend = NORMAL_BLEND_DURATION;

#if ENABLE_GUN_PROJECTILE_THROW
		if (m_bAimGunAndThrow)
		{
			fBlend = SLOW_BLEND_DURATION;
		}
#endif	//ENABLE_GUN_PROJECTILE_THROW

		pPed->GetMovePed().SetTaskNetwork(m_MoveNetworkHelper, fBlend);	
	}
	m_MoveNetworkHelper.SetClipSet(m_iClipSet);	
	m_MoveNetworkHelper.SetClipSet(m_iStreamedClipSet, ms_StreamedClipSet); 

	SetGripClip();

	//if throwing over sf_MinUnderHandDistance2, don't try and underhand it.
	static dev_float sf_MinUnderHandDistance2 = 7.0f*7.0f;
	if (fTargetDist2 >= sf_MinUnderHandDistance2)
		bOverHandOnly = true;

	// If we're set to do an underhand throw, do a probe test to check that there isn't anything blocking between the ped and the target position
	if (pPed->IsLocalPlayer() && !bOverHandOnly && m_fThrowPitch <= 0.4f && vTarget.IsNonZero())
	{
		//Ground check, if missing play throw while standing still	
		Vector3 vTestStart = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

		// Scale the target position back slightly so we don't register a collision with the terrain/object at the target position
		Vector3 vTargetPosition = vTarget;
		vTargetPosition.Scale(0.8f);
		vTargetPosition = vTestStart + vTargetPosition;

		// Start the probe slightly lower down than the peds centre, around thigh-height.
		static dev_float fStartOffset = 0.25f;
		vTestStart.z -= fStartOffset;

#if __BANK
		CTask::ms_debugDraw.AddLine(RCC_VEC3V(vTestStart), RCC_VEC3V(vTargetPosition), Color_yellow, 0);
#endif 		

		WorldProbe::CShapeTestHitPoint probeIsect;
		WorldProbe::CShapeTestResults probeResult(probeIsect);
		WorldProbe::CShapeTestProbeDesc probeDesc;
		probeDesc.SetStartAndEnd(vTestStart, vTarget);
		probeDesc.SetResultsStructure(&probeResult);
		probeDesc.SetExcludeEntity(pPed);
		probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_OBJECT_TYPE);
		probeDesc.SetContext(WorldProbe::LOS_GeneralAI);

		if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
		{
			// We have a collision, force an over hand throw
			bOverHandOnly = true;

#if __BANK
			if (probeResult.GetNumHits() > 0)
			{
				CTask::ms_debugDraw.AddSphere(RCC_VEC3V(probeResult[0].GetHitPosition()), 0.2f, Color_red, 1000);
			}
#endif 				
		}
	}

	m_MoveNetworkHelper.SetFlag(bOverHandOnly, ms_OverhandOnlyId);		
	m_MoveNetworkHelper.SetFlag(m_bThrowPlace, ms_UseThrowPlaceId);
	m_MoveNetworkHelper.SetFlag(m_bDropProjectile, ms_DoDropId);	
#if ENABLE_GUN_PROJECTILE_THROW
	m_MoveNetworkHelper.SetFlag(m_bAimGunAndThrow, ms_AimGunThrowId);
#endif	//ENABLE_GUN_PROJECTILE_THROW
	m_MoveNetworkHelper.SetFloat(ms_ThrowPitchPhase, m_fThrowPitch);
	if (m_bDropProjectile)
	{		
// 		static fwMvFilterId s_UpperNoLeftArmPartialHeadHashId("UpperbodyFeathered_NoLeftArm_PartialHead_filter",0x7D55CA4B);	
// 		m_pFrameFilter = g_FrameFilterDictionaryStore.FindFrameFilter(s_UpperNoLeftArmPartialHeadHashId);
// 		if(m_pFrameFilter)
// 			m_MoveNetworkHelper.SetFilter(m_pFrameFilter, ms_FrameFilterId);
		m_pFrameFilter = NULL;
				
		bool bThrowOnly = pWeapon && pWeapon->GetWeaponInfo()->GetThrowOnly();
		if (bThrowOnly)
		{			
			m_bDropUnderhand = (pPed->GetMotionData()->GetDesiredMbrY() == MOVEBLENDRATIO_STILL || (pWeapon && pWeapon->GetWeaponHash() == WEAPONTYPE_DLC_SNOWBALL) FPS_MODE_SUPPORTED_ONLY(|| pPed->IsFirstPersonShooterModeEnabledForPlayer(false))) &&
								pWeapon && pWeapon->GetWeaponInfo()->GetDropForwardVelocity()!=0.0f;								
		}	
		else
			m_bDropUnderhand = false;
		m_MoveNetworkHelper.SetFlag(bThrowOnly, ms_NoStaticDropId);		
		m_MoveNetworkHelper.SetFlag(m_bDropUnderhand, ms_UseDropUnderhandId);	
		
	}
	else
	{
		m_pFrameFilter = g_FrameFilterDictionaryStore.FindFrameFilter(CMovePed::s_UpperBodyFilterHashId);		
		m_MoveNetworkHelper.SetFilter(m_pFrameFilter, ms_FrameFilterId);
	}	
	Vector2 vCurrentMBR = pPed->GetMotionData()->GetCurrentMoveBlendRatio();
	float fStrafeSpeed = vCurrentMBR.Mag() / MOVEBLENDRATIO_SPRINT;
	static dev_float sf_MinMovingTimeForMovingThrow = 0.5f;
	if ((m_bForceStandingThrow || !pPed->IsStrafing() || m_fTimeMoving <= sf_MinMovingTimeForMovingThrow) GUN_PROJECTILE_THROW_ONLY(&& !m_bAimGunAndThrow))
		fStrafeSpeed = 0;
	m_MoveNetworkHelper.SetFloat(ms_StrafeSpeed, fStrafeSpeed);
}

CTask::FSM_Return CTaskAimAndThrowProjectile::ThrowProjectile_OnUpdate(CPed* pPed)
{
	static const fwMvClipId s_ThrowClip("ThrowClip",0xC4C387F);
	
	// Grab a pointer to the peds weapon, this should always be valid for a local ped
	CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();		
	if(!pPed->IsNetworkClone() && !m_bHasThrownProjectile && !pWeapon)
	{
		taskDebugf1("CTaskAimAndThrowProjectile::ThrowProjectile_OnUpdate: FSM_Quit - !m_bHasThrownProjectile && !pWeapon. Previous State: %d", GetPreviousState());
		return FSM_Quit; //weapon where?
	}

	pPed->SetPedResetFlag( CPED_RESET_FLAG_ThrowingProjectile, true );

	const crClip* pClip = m_MoveNetworkHelper.GetClip(s_ThrowClip);
	if (pClip)
	{
		CClipEventTags::FindEventPhase(pClip, CClipEventTags::Interruptible, m_fInterruptPhase);	
	}


	if (pPed->IsNetworkClone() && !m_bHasThrownProjectile)
	{
		//Clones check and wait here for the synced projectile and throw as soon as they see it
		if(CNetworkPendingProjectiles::GetProjectile(GetPed(), GetNetSequenceID(), false))
		{
			//Clones throw as soon as they see a projectile synced
			ThrowProjectileNow(pPed, pWeapon);
		}
		else
		{
			if(GetStateFromNetwork()==State_Finish)
			{
				SetState(State_Finish);
			}
			return FSM_Continue;
		}
	}

	float fPhase = m_MoveNetworkHelper.GetFloat(ms_ThrowPhase);

#if ENABLE_GUN_PROJECTILE_THROW
	if (m_bAimGunAndThrow)
	{
		// If we're throwing from gun aim, set the grenade to be visible once we've reached the specified threshold in the anim
		bool bClipPastVisibilityPhase = false;

		float fVisibilityPhase = -1.0f;
		if (pClip)
		{
			CClipEventTags::FindEventPhase<crPropertyAttributeBool>(pClip, CClipEventTags::Object, CClipEventTags::Create, true, fVisibilityPhase);
		}

		// Check if we have a specified tag in the throw clip
		if (fVisibilityPhase != -1.0f)
		{
			bClipPastVisibilityPhase = fPhase >= fVisibilityPhase;
		}
		else
		{ 
			// We don't have a tag, use this as the visibility threshold
			static dev_float fThrowPhaseThreshold = 0.35f;
			bClipPastVisibilityPhase = fPhase >= fThrowPhaseThreshold;
		}

		if (bClipPastVisibilityPhase)
		{
			CObject* pGrenadeObject = pPed->GetWeaponManager()->GetEquippedWeaponObject();
			if (pGrenadeObject)
			{
				pGrenadeObject->SetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY, true, true);
			}
		}
	}
#endif	//ENABLE_GUN_PROJECTILE_THROW

	if(m_MoveNetworkHelper.GetBoolean(ms_ThrowCompleteEvent) || m_MoveNetworkHelper.GetBoolean(ms_ReloadEvent) || fPhase>=m_fInterruptPhase)
	{
		if (!m_bHasThrownProjectile)
		{
			ThrowProjectileNow(pPed, pWeapon);
		}		

		if(pPed->IsNetworkClone())
		{
#if ENABLE_GUN_PROJECTILE_THROW
            // aim gun thrown projectile tasks are run locally so shouldn't wait for the network state on the owner
            if(m_bAimGunAndThrow)
            {
                SetState(State_Finish);
            }
#endif	//ENABLE_GUN_PROJECTILE_THROW

			if(GetStateFromNetwork() != State_ThrowProjectile && GetStateFromNetwork() > State_Intro)
			{
#if FPS_MODE_SUPPORTED
				if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false,true) && m_pFrameFilter == NULL)
				{
					pPed->GetWeaponManager()->CreateEquippedWeaponObject();
					SetState(State_StreamingClips);
					return FSM_Continue;
				}
#endif
				if(GetStateFromNetwork() == State_Finish)
					m_MoveNetworkHelper.SendRequest(ms_StartFinish);

				if(GetStateFromNetwork()!=State_PullPin)
				{
					SetState(GetStateFromNetwork());
				}
			}
		}
		else
		{
			bool bIsOutOfAmmo = pPed->GetInventory()->GetIsWeaponOutOfAmmo(pPed->GetWeaponManager()->GetEquippedWeaponInfo());
			if(bIsOutOfAmmo)
			{
				// Out of ammo - swap weapon		
				CThrowProjectileHelper::SetWeaponSlotAfterFiring(pPed);			

				// Quit if not a thrown weapon
				const CWeaponInfo* pEquippedWeaponInfo = pPed->GetWeaponManager()->GetEquippedWeaponInfo();
				if(pEquippedWeaponInfo && pEquippedWeaponInfo->GetIsThrownWeapon())
				{
					bIsOutOfAmmo = pPed->GetInventory()->GetIsWeaponOutOfAmmo(pEquippedWeaponInfo);
				}

				if(bIsOutOfAmmo)
				{
					SetState(State_Finish);
					return FSM_Continue;
				}
			}

			if(pPed->GetWeaponManager()->GetRequiresWeaponSwitch()) 
			{				
				if(!m_bHasReloaded)
				{					
					if(!bIsOutOfAmmo GUN_PROJECTILE_THROW_ONLY(&& !m_bAimGunAndThrow))
					{
						m_bHasReloaded = true;	
						pPed->GetWeaponManager()->CreateEquippedWeaponObject();
					}
				}				
			} 

#if FPS_MODE_SUPPORTED
			if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && m_bFPSUseAimIKForWeapon && !m_bAimGunAndThrow)
			{
				// Restart
				m_bDropProjectile = false;
				HandleFPSStateUpdate();
				m_bPlayFPSTransition = true;
				SetState(State_StreamingClips);
				return FSM_Continue;					
			}
#endif

			if (m_bDropProjectile)
			{
				SetState(State_Finish);
				m_MoveNetworkHelper.SendRequest(ms_StartFinish);
				return FSM_Continue;
			}

			if(pPed->IsPlayer())
			{
				m_bPriming = false;	
				m_bIsFiring = false;
				m_bPinPulled = m_bHasNoPullPin;

#if ENABLE_GUN_PROJECTILE_THROW
				if (m_bAimGunAndThrow)
				{	
					m_bAimGunAndThrow = false;
					pPed->GetPedIntelligence()->SetDoneAimGrenadeThrow();
					SetState(State_Finish);
					return FSM_Continue;
				}
				else
#endif	//ENABLE_GUN_PROJECTILE_THROW
				{
					SetState(State_HoldingProjectile);
				}
			}
			else
				SetState(State_Finish); //AI only toss once, for now (?)
			
		}

		return FSM_Continue;
	}	
	else if(m_bHasThrownProjectile && m_MoveNetworkHelper.GetClip(s_ThrowClip) == NULL && pPed->IsNetworkClone())
	{
		if(GetStateFromNetwork() != State_ThrowProjectile && GetStateFromNetwork() > State_Intro)
		{
			if(GetStateFromNetwork() == State_Finish)
				m_MoveNetworkHelper.SendRequest(ms_StartFinish);
			SetState(GetStateFromNetwork());
		}
	}

	if(!m_bHasThrownProjectile)
	{
		pPed->SetPedResetFlag( CPED_RESET_FLAG_TemporarilyBlockWeaponSwitching, true );
		if(pClip)
			CClipEventTags::FindEventPhase(pClip, CClipEventTags::Fire, m_fThrowPhase);	 		

		// When the fire anim event happens, throw the projectile 		
		if(fPhase >= m_fThrowPhase)
		{
			if(!pPed->IsNetworkClone())
				m_bFire = true;
			else
            {
                if(CNetworkPendingProjectiles::GetProjectile(pPed, GetNetSequenceID(), false))
                {
				    ThrowProjectileNow(pPed, pWeapon);
                }
            }
		}
	} else {
		if (pClip)
		{			
			CClipEventTags::FindEventPhase<crPropertyAttributeBool>(pClip, CClipEventTags::Object, CClipEventTags::Create, true, m_fReloadPhase);	
		}

		// When the reload anim event happens, create the projectile 
		if(!m_bHasReloaded && fPhase >= m_fReloadPhase && !pPed->IsNetworkClone() GUN_PROJECTILE_THROW_ONLY(&& !m_bAimGunAndThrow))
		{
			if(!pPed->GetInventory()->GetIsWeaponOutOfAmmo(pPed->GetWeaponManager()->GetEquippedWeaponInfo()))
			{
				m_bHasReloaded = true;	
				pPed->GetWeaponManager()->CreateEquippedWeaponObject();
			}
		}

		if (CThrowProjectileHelper::IsPlayerRolling(pPed) && !m_bAimGunAndThrow)
		{
			m_bPriming = false;	
			m_bIsFiring = false;
			m_bPinPulled = m_bHasNoPullPin;
			SetState(State_CombatRoll);
			return FSM_Continue;
		}
	}

#if FPS_MODE_SUPPORTED
	if(GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		const CClipEventTags::CCameraShakeTag* pCamShake = static_cast<const CClipEventTags::CCameraShakeTag*>(m_MoveNetworkHelper.GetNetworkPlayer()->GetProperty(CClipEventTags::CameraShake));
		if (pCamShake)
		{
			if(pCamShake->GetShakeRefHash() != 0)
			{
				camInterface::GetGameplayDirector().GetFirstPersonShooterCamera()->Shake( pCamShake->GetShakeRefHash() );
			}
		}
	}
#endif
	return FSM_Continue;
}

void CTaskAimAndThrowProjectile::ThrowProjectile_OnExit(CPed* pPed)
{	
	if (GetState() == State_HoldingProjectile)
	{
		float fBlendOutDuration = NORMAL_BLEND_DURATION;
		pPed->GetMovePed().ClearTaskNetwork(m_MoveNetworkHelper, fBlendOutDuration, CMovePed::Task_TagSyncTransition);
		m_fBlockAimingIndependentMoverTime = fBlendOutDuration;

		fwMvNetworkDefId networkDefId(CClipNetworkMoveInfo::ms_NetworkTaskAimAndThrowProjectile);
		CreateNetworkPlayer(networkDefId);
		m_MoveNetworkHelper.SetFlag(true, ms_SkipIntroId);
		pPed->GetMovePed().SetTaskNetwork(m_MoveNetworkHelper, SLOW_BLEND_DURATION, CMovePed::Task_TagSyncContinuous);	
		m_MoveNetworkHelper.SetClipSet(m_iClipSet);	
		m_MoveNetworkHelper.SetClipSet(m_iStreamedClipSet, ms_StreamedClipSet); 

		SetGripClip();

		if (m_bHasThrownProjectile && pPed->IsNetworkClone())
		{
			pPed->GetWeaponManager()->CreateEquippedWeaponObject();
		}
	}

#if FPS_MODE_SUPPORTED
	m_pFPSWeaponMoveNetworkHelper = NULL;
#endif
}

CTask::FSM_Return CTaskAimAndThrowProjectile::StatePlaceProjectile_OnEnter(CPed* UNUSED_PARAM(pPed))
{
	SetNewTask(rage_new CTaskBomb());
	GetPed()->GetMovePed().ClearTaskNetwork(m_MoveNetworkHelper, SLOW_BLEND_DURATION);
	return FSM_Continue;
}

CTask::FSM_Return CTaskAimAndThrowProjectile::StatePlaceProjectile_OnUpdate(CPed* pPed)
{


#if FPS_MODE_SUPPORTED
	bool bUseFPSModeBehaviour = pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && m_bFPSUseAimIKForWeapon;
	if(bUseFPSModeBehaviour)
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_FPSPlacingProjectile, true);
	}
	
	if(!bUseFPSModeBehaviour)
#endif
	{
		GetPed()->SetIsStrafing(false);
	}

	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		if(pPed->IsNetworkClone())
		{
			if(GetStateFromNetwork() != State_PlaceProjectile)
			{
				if(GetStateFromNetwork() == State_Finish && m_MoveNetworkHelper.IsNetworkActive())
					m_MoveNetworkHelper.SendRequest(ms_StartFinish);
				SetState(GetStateFromNetwork());
			}
		}
		else
		{
			int nAmmo = pPed->GetInventory()->GetWeaponAmmo( pPed->GetWeaponManager()->GetEquippedWeaponInfo() );
			if( pPed->GetWeaponManager()->GetRequiresWeaponSwitch() )
			{
				if( nAmmo == 0 )
					pPed->GetWeaponManager()->EquipBestWeapon();				
				else if (m_MoveNetworkHelper.IsNetworkActive()) 
					m_MoveNetworkHelper.SendRequest(ms_StartFinish);

				SetState(State_Finish);
				return FSM_Continue;
			}
			
			// Out of ammo - swap weapon		
			if( nAmmo == 0 )
			{
				if (m_MoveNetworkHelper.IsNetworkActive()) 
					m_MoveNetworkHelper.SendRequest(ms_StartFinish);
				SetState(State_Finish);
				return FSM_Continue;
			}
			
			// Otherwise just reload (now through swap weapon)
			SetState(State_Finish);
			//SetState(State_Reload);
		}
	}
#if FPS_MODE_SUPPORTED
	else if(bUseFPSModeBehaviour)
	{
		CTask* pSubTask = GetSubTask();
		if(pSubTask && pSubTask->GetSubTask() && pSubTask->GetTaskType() == CTaskTypes::TASK_BOMB &&
			pSubTask->GetSubTask()->GetTaskType() == CTaskTypes::TASK_SLIDE_TO_COORD)
		{
			const CClipHelper* pClipHelper = ((CTaskSlideToCoord*) (pSubTask->GetSubTask()))->GetSubTaskClip();
			if(pClipHelper && pClipHelper->IsEvent( CClipEventTags::Interruptible ))
			{
				Vector3 vTargetPos(0.01f, 0.01f, 0.01f);
				if(CTaskWeaponBlocked::IsWeaponBlocked( pPed, vTargetPos))
				{
					SetState(State_Blocked);
				}
				else
				{
					SetState(State_Finish);
				}
			}
		}
	}
#endif

	return FSM_Continue;
}


////////////////////////////////////////////////////////////////////////////////
void CTaskAimAndThrowProjectile::CombatRoll_OnEnter(CPed* pPed)
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

	pPed->GetMotionData()->SetCombatRoll(true);	

	pPed->GetMovePed().ClearTaskNetwork(m_MoveNetworkHelper, NORMAL_BLEND_DURATION);		
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskAimAndThrowProjectile::CombatRoll_OnUpdate(CPed* pPed)
{
	if(!pPed->GetMotionData()->GetCombatRoll())
	{
#if FPS_MODE_SUPPORTED 
		if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
		{
			// Update FPS state at the end of combat roll.
			HandleFPSStateUpdate();
			m_bPlayFPSTransition = true;
			SetState(State_StreamingClips);
		}
		else
#endif
		{
			SetState(State_HoldingProjectile);
		}
		return FSM_Continue;
	}

	if (m_MoveNetworkHelper.HasNetworkExpired())
	{
		CTask* pTask = pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_COMBAT_ROLL);
		if (pTask)
		{
			if (static_cast<CTaskCombatRoll*>(pTask)->GetIsLanding())
			{
				fwMvNetworkDefId networkDefId(CClipNetworkMoveInfo::ms_NetworkTaskAimAndThrowProjectile);
				CreateNetworkPlayer(networkDefId);
				m_MoveNetworkHelper.SetFlag(true, ms_SkipIntroId);
				pPed->GetMovePed().SetTaskNetwork(m_MoveNetworkHelper, REALLY_SLOW_BLEND_DURATION, CMovePed::Task_TagSyncContinuous);	
				m_MoveNetworkHelper.SetClipSet(m_iClipSet);	
				m_MoveNetworkHelper.SetClipSet(m_iStreamedClipSet, ms_StreamedClipSet); 

				SetGripClip();	
			}
		}
	}
	

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////
void CTaskAimAndThrowProjectile::Reload_OnEnter(CPed* pPed)
{	
	//DEPRECATED
	pPed->GetMovePed().ClearTaskNetwork(m_MoveNetworkHelper, NORMAL_BLEND_DURATION);
	//fwMvNetworkDefId networkDefId(CClipNetworkMoveInfo::ms_NetworkTaskAimAndThrowProjectileReload);
	//CreateNetworkPlayer(networkDefId);
	pPed->GetMovePed().SetTaskNetwork(m_MoveNetworkHelper, SLOW_BLEND_DURATION, CMovePed::Task_TagSyncContinuous);	
	m_MoveNetworkHelper.SetClipSet(m_iClipSet);	
	m_MoveNetworkHelper.SetClipSet(m_iStreamedClipSet, ms_StreamedClipSet); 

	SetGripClip();	
	
	m_bHasReloaded = false;	
	m_MoveNetworkHelper.WaitForTargetState(ms_ReloadEvent);	
}

CTask::FSM_Return CTaskAimAndThrowProjectile::Reload_OnUpdate(CPed* pPed)
{
	if(!m_MoveNetworkHelper.IsInTargetState())
		return FSM_Continue;

	// Doing this in Reload_OnEnter is always a frame early
	static const fwMvClipId s_ReloadClip("ReloadClip",0x7F6D7B74);
	const crClip* pClip = m_MoveNetworkHelper.GetClip(s_ReloadClip);
	if (pClip)
	{
		CClipEventTags::FindEventPhase<crPropertyAttributeBool>(pClip, CClipEventTags::Object, CClipEventTags::Create, true, m_fReloadPhase);	
	}

	// When the reload anim event happens, create the projectile 
	float fPhase = m_MoveNetworkHelper.GetFloat(ms_ReloadPhase);
	if(!m_bHasReloaded && fPhase >= m_fReloadPhase && !pPed->IsNetworkClone())
	{
		pPed->GetWeaponManager()->CreateEquippedWeaponObject();
	}

	if(m_MoveNetworkHelper.GetBoolean(ms_ReloadCompleteEvent))
	{
		if(pPed->IsPlayer())
		{
			m_bPriming = false;	
			m_bIsFiring = false;
			SetState(State_HoldingProjectile);
		}
		else
			SetState(State_Finish); //AI only toss once, for now (?)
	}	

	// catch an exit on the owner
	if(pPed->IsNetworkClone())
	{
		if(GetStateFromNetwork() == State_Finish)
		{
			SetState(State_Finish);
			m_MoveNetworkHelper.SendRequest(ms_StartFinish);
		}
	}
	
	return FSM_Continue;
}

void CTaskAimAndThrowProjectile::Reload_OnExit(CPed* pPed)
{	
	if (GetState() == State_HoldingProjectile)
	{
		pPed->GetMovePed().ClearTaskNetwork(m_MoveNetworkHelper, NORMAL_BLEND_DURATION);
		fwMvNetworkDefId networkDefId(CClipNetworkMoveInfo::ms_NetworkTaskAimAndThrowProjectile);
		CreateNetworkPlayer(networkDefId);
		m_MoveNetworkHelper.SetFlag(true, ms_SkipIntroId);
		pPed->GetMovePed().SetTaskNetwork(m_MoveNetworkHelper, SLOW_BLEND_DURATION, CMovePed::Task_TagSyncContinuous);	
		m_MoveNetworkHelper.SetClipSet(m_iClipSet);	
		m_MoveNetworkHelper.SetClipSet(m_iStreamedClipSet, ms_StreamedClipSet); 

		SetGripClip();	
	}
}

void CTaskAimAndThrowProjectile::CloneIdle_OnEnter(CPed* pPed)
{
	if (m_MoveNetworkHelper.GetNetworkPlayer())
	{
		pPed->GetMovePed().ClearTaskNetwork(m_MoveNetworkHelper, SLOW_BLEND_DURATION);
	}
}

CTask::FSM_Return CTaskAimAndThrowProjectile::CloneIdle_OnUpdate(CPed* pPed)
{
	if (!NetworkInterface::UseFirstPersonIdleMovementForRemotePlayer(*pPed))
	{
		SetState(State_StreamingClips);
	}

	return FSM_Continue;
}

void CTaskAimAndThrowProjectile::Outro_OnEnter(CPed* UNUSED_PARAM(pPed))
{	
	m_MoveNetworkHelper.WaitForTargetState(ms_EnterOutroEvent);
}

CTask::FSM_Return CTaskAimAndThrowProjectile::Outro_OnUpdate(CPed* pPed)
{
	//standing delays the animation for the strafe longer outro B* 942701
	static dev_float s_fStandOutroDelay = 0.5f;
	if (pPed->GetMotionData()->GetCurrentMbrY() == MOVEBLENDRATIO_STILL && GetTimeInState()<s_fStandOutroDelay && !pPed->GetWeaponManager()->GetRequiresWeaponSwitch())
		return FSM_Continue;
	m_MoveNetworkHelper.SendRequest(ms_StartOutro);	

	if(!m_MoveNetworkHelper.IsInTargetState())
		return FSM_Continue;

	float fPhase = m_MoveNetworkHelper.GetFloat(ms_TransitionPhase);
	static dev_float sf_OutroPhaseOut = 0.8f;
	if (fPhase >= sf_OutroPhaseOut)
	{
		SetState(State_Finish);
	}

	if(!pPed->IsNetworkClone() && m_bCanBePlaced && !IsAiming(pPed) && (m_bPriming || CThrowProjectileHelper::IsPlayerPriming(pPed)))
	{
		// hold up on any animation to make sure the player doesn't want to just place
		SetPriming(pPed, true);
		if (IsFiring(pPed) && CanPlaceProjectile(pPed))
		{
			// Switch to move to state
			SetState(State_PlaceProjectile);				
		}
	}
	return FSM_Continue;
}

dev_float ROLL_ANIM_SCALE = 0.5f;
dev_float MIN_COVER_THROW_HEIGHT_RELATIVE_TO_PED = 0.65f;

void CTaskAimAndThrowProjectile::ThrowProjectileNow(CPed* pPed, CWeapon* pWeapon, Vector3 *pvOverrideStartPosition) 
{
	if(!pPed->IsNetworkClone())
	{
		Assert(pWeapon);
		Assert(pWeapon->GetWeaponInfo());
	}
	Assert(pPed->GetWeaponManager());
	
	// time to throw the projectile	
	if(pPed->IsNetworkClone())
	{
		CProjectileManager::FireOrPlacePendingProjectile(pPed, GetNetSequenceID());

		// Ensure object is removed
		pPed->GetWeaponManager()->DestroyEquippedWeaponObject();		
	}
	else
	{		
		Assert(pPed->GetWeaponManager());
		CObject* pProjOverride = pPed->GetWeaponManager()->GetEquippedWeaponObject();
		
		// Calculate the launch position
		const s32 nBoneIndex = GUN_PROJECTILE_THROW_ONLY(m_bAimGunAndThrow ? pPed->GetBoneIndexFromBoneTag(BONETAG_L_PH_HAND) :) pPed->GetBoneIndexFromBoneTag(BONETAG_R_HAND);

		Assert(nBoneIndex != BONETAG_INVALID);

		//Get an up-to-date position before throwing (saves us from calling pProjOverride->ProcessAttachment()).
		Matrix34 matOffset; 
		matOffset.Identity();
		Matrix34 matUpdated;
		bool bUseMatUpdated = false;
		fwAttachmentEntityExtension* pExtension = pProjOverride ? pProjOverride->GetAttachmentExtension() : NULL;
		if (pExtension)
		{
			Matrix34 matParent; 
			pPed->GetGlobalMtx(nBoneIndex, matParent);
			matOffset.FromQuaternion(pExtension->GetAttachQuat()); 
			matOffset.d = pExtension->GetAttachOffset();
			matUpdated.Dot(matOffset, matParent);
			bUseMatUpdated = true;
		}
		
		Matrix34 weaponMat;
		if (bUseMatUpdated)
			weaponMat = matUpdated;
		else if( pProjOverride)
			weaponMat = MAT34V_TO_MATRIX34(pProjOverride->GetMatrix());
		else
			pPed->GetGlobalMtx(nBoneIndex, weaponMat);

		Vector3 vStart(Vector3::ZeroType);
		Vector3 vEnd(Vector3::ZeroType);
		if (pvOverrideStartPosition!=NULL)
			vStart = *pvOverrideStartPosition;
		else if (pPed->IsLocalPlayer())
		{
			const camFrame& aimFrame = camInterface::GetPlayerControlCamAimFrame();
			vStart = aimFrame.GetPosition();
			Vector3 vShot  = aimFrame.GetFront();
			// Move source of gunshot vector along to the gun muzzle so can't hit anything behind the gun.
			f32 fShotDot = vShot.Dot(weaponMat.d - vStart);
			vStart += fShotDot * vShot;
		}
		else
		{
			vStart = GUN_PROJECTILE_THROW_ONLY(m_bAimGunAndThrow ? pPed->GetBonePositionCached((eAnimBoneTag)BONETAG_L_PH_HAND) :) pPed->GetBonePositionCached((eAnimBoneTag)BONETAG_R_HAND);	
		}

		//If this is a drop..
		if( m_bDropProjectile )
		{
			Vector3 trajectory;
			if (!pWeapon->GetWeaponInfo()->GetThrowOnly())
			{
				trajectory.Zero();
			}
			else if( m_bDropUnderhand )  
			{
				trajectory = VEC3V_TO_VECTOR3(pPed->GetTransform().GetB()) * pWeapon->GetWeaponInfo()->GetDropForwardVelocity();
			}
			else
			{
				trajectory = VEC3V_TO_VECTOR3(pPed->GetTransform().GetB()) * -(pWeapon->GetWeaponInfo()->GetDropForwardVelocity() * 0.5f);			
			}
			vEnd = weaponMat.d + trajectory;
			vStart = weaponMat.d;
		}
		else 
		{
			bool bValidTargetPosition = false;
			if( pPed->IsLocalPlayer() && pPed->GetPlayerInfo() && !pPed->GetPlayerInfo()->AreControlsDisabled() )
			{
				// First check for the more precise reading, the camera aim at position
				const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
				const CObject* pWeaponObject = pPed->GetWeaponManager()->GetEquippedWeaponObject();
				if( pWeaponObject )
				{
					Vector3 vStartTemp( Vector3::ZeroType );
					bValidTargetPosition = pWeapon->CalcFireVecFromAimCamera( pPed->GetIsInVehicle() ? static_cast<CEntity*>(pPed->GetMyVehicle()) : pPed, weaponMat, vStartTemp, vEnd );
				}
			}

			// Now scale up to weapon range
			if( !bValidTargetPosition && m_target.GetIsValid() )
				bValidTargetPosition = m_target.GetPositionWithFiringOffsets( pPed, vEnd );
		}

		float fOverrideLifeTime = -1.0f;			

		if(!pWeapon->GetIsCooking()) //NOW everything cooks that can
			pWeapon->StartCookTimer(fwTimer::GetTimeInMilliseconds());	
		if (pWeapon->GetWeaponInfo()->GetProjectileFuseTime() > 0.0f)
			fOverrideLifeTime = Max(0.0f, pWeapon->GetCookTimeLeft(pPed, fwTimer::GetTimeInMilliseconds()));

		//if( m_bDropProjectile ) removed per B* 1296215
		//	fOverrideLifeTime *= 2.0f;  //dropped projectiles get more time.

		// Throw the projectile
#if __BANK
		CTask::ms_debugDraw.AddSphere(RCC_VEC3V(vEnd), 0.1f, Color_orange, 1000);
#endif 	
		CWeapon::sFireParams params(pPed, weaponMat, &vStart, &vEnd);
		params.fOverrideLifeTime = fOverrideLifeTime;
		params.fDesiredTargetDistance = vStart.Dist( vEnd );
		params.pIgnoreCollisionEntity = m_pIgnoreCollisionEntity;
		if (m_pIgnoreCollisionEntity)
		{
			params.pIgnoreDamageEntity = m_pIgnoreCollisionEntity;
			params.bIgnoreDamageEntityAttachParent = true;
		}

		if (m_bDropProjectile)
		{
			if (pWeapon && (pWeapon->GetWeaponHash()!=ATSTRINGHASH("WEAPON_MOLOTOV", 0x24B17070) &&
							pWeapon->GetWeaponHash()!=ATSTRINGHASH("WEAPON_FLARE", 0x497facc3)))
			{
				params.iFireFlags.SetFlag(CWeapon::FF_DisableProjectileTrail);
			}
		}

#if ENABLE_GUN_PROJECTILE_THROW
		// Only set the "bProjectileCreatedFromGrenadeThrow" flag if we threw from using the DPad input as this temporarily blocks bomb detonation until it is released.
		params.bProjectileCreatedFromGrenadeThrow = m_bAimGunAndThrow && m_bProjectileThrownFromDPadInput;
#endif	//ENABLE_GUN_PROJECTILE_THROW

		pWeapon->Fire(params);
		// Ensure object is removed
		pPed->GetWeaponManager()->DestroyEquippedWeaponObject();		
	}

	m_bHasThrownProjectile = true;
}

void CTaskAimAndThrowProjectile::Blocked_OnEnter(CPed* UNUSED_PARAM(pPed))
{
	SetNewTask( rage_new CTaskWeaponBlocked( CWeaponController::WCT_Player ) );
	return;
}

CTask::FSM_Return CTaskAimAndThrowProjectile::Blocked_OnUpdate(CPed *pPed)
{

	if( GetIsFlagSet( aiTaskFlags::SubTaskFinished ) )
	{
		SetState(State_StreamingClips);
	}
	else
	{
		bool bUseFPSModeBehaviour = pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && m_bFPSUseAimIKForWeapon;

		Vector3 vTargetPos(0.01f, 0.01f, 0.01f);
		if(!CTaskWeaponBlocked::IsWeaponBlocked( pPed, vTargetPos))
		{
#if FPS_MODE_SUPPORTED
			if(bUseFPSModeBehaviour)
			{
				HandleFPSStateUpdate();
				CWeapon* pWeapon = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeapon() : NULL;
				//Play a "to RNG" transition only if we want to cook the weapon after exiting the block state
				if(pWeapon && pWeapon->GetWeaponInfo() && pWeapon->GetWeaponInfo()->GetCookWhileAiming() && 
				   !pWeapon->GetIsCooking() && (m_bFPSWantsToThrowWhenReady || m_bDropProjectile))
				{
					m_bPlayFPSTransition = true;
				}

			}
#endif
			SetState(State_StreamingClips);
		}
		else if(bUseFPSModeBehaviour && m_bCanBePlaced && !pPed->GetPedResetFlag(CPED_RESET_FLAG_InAirDefenceSphere))
		{
#if FPS_MODE_SUPPORTED
			if(!pPed->GetMotionData()->GetIsFPSRNG() && pPed->GetMotionData()->GetWasFPSRNG() && CanPlaceProjectile(pPed))
			{
				HandleFPSStateUpdate();
				m_bPlayFPSTransition = false;
				SetState(State_StreamingClips);
			}
#endif
		}
	}

	return FSM_Continue;
}


void CTaskAimAndThrowProjectile::UpdateTarget(CPed* pPed)
{
	if(pPed && pPed->IsPlayer())
	{
		weaponAssert(pPed->GetWeaponManager());
		if(pPed->IsNetworkClone())
		{
			if(static_cast<CNetObjPlayer*>(pPed->GetNetworkObject())->HasValidTarget())
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

void CTaskAimAndThrowProjectile::DoAbort(const AbortPriority UNUSED_PARAM(iPriority), const aiEvent* pEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	if(pEvent && ((CEvent*)pEvent)->GetEventType() == EVENT_DAMAGE && GetState() == State_HoldingProjectile)
	{
		// Drop Molotov's of holding it ready to fir. Drop them straight down.
		CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
		if(pWeapon && pWeapon->GetWeaponInfo() && pWeapon->GetWeaponInfo()->GetDamageType() == DAMAGE_TYPE_FIRE)
		{
			m_bDropProjectile = true;
			ThrowProjectileNow(pPed,pWeapon, NULL);
		}
	}
}

bool CTaskAimAndThrowProjectile::HasValidWeapon(CPed* pPed) const
{
	const CWeaponInfo* pWeaponInfo = pPed->GetWeaponManager()->GetEquippedWeaponInfo();
	if(!pWeaponInfo)
	{
		return false;
	}
	
	//Allow any weapon to be valid if we are throwing a grenade while aiming (we already filter the weapon types before setting this flag)
	if (pPed->GetPedResetFlag(CPED_RESET_FLAG_IsThrowingProjectileWhileAiming))
	{
		return true;
	}

	if(!pWeaponInfo->GetIsThrownWeapon())
	{
		return false;
	}

	return true;
}

void CTaskAimAndThrowProjectile::CreateNetworkPlayer(fwMvNetworkDefId networkDefId)
{
	m_MoveNetworkHelper.CreateNetworkPlayer(GetPed(), networkDefId);
	m_NetworkDefId = networkDefId;
}

#if ENABLE_GUN_PROJECTILE_THROW
void CTaskAimAndThrowProjectile::CreateDummyWeaponObject(CPed *pPed, CWeapon *pWeapon, const CWeaponInfo *pWeaponInfo)
{
	if (pPed && pWeapon && pWeaponInfo)
	{
		// Create a temp weapon object to attach to the peds hand
		m_pTempWeaponObject = pPed->GetWeaponManager()->DropWeapon(pPed->GetWeaponManager()->GetEquippedWeaponHash(), false, false, true);
		if (m_pTempWeaponObject)
		{
			s32 iPedBoneIndex = pPed->GetBoneIndexFromBoneTag(BONETAG_R_PH_HAND);
			// Get the object bone
			s32 iObjectBoneIndex = -1;
			CWeaponModelInfo* pWeaponModelInfo = m_pTempWeaponObject->GetBaseModelInfo() ? static_cast<CWeaponModelInfo*>(m_pTempWeaponObject->GetBaseModelInfo()) : NULL;
			if(pWeaponModelInfo)
			{
				iObjectBoneIndex = pWeaponModelInfo->GetBoneIndex(WEAPON_GRIP_R);
				CPedEquippedWeapon::AttachObjects(pPed, m_pTempWeaponObject, iPedBoneIndex, iObjectBoneIndex);
			}
		}

		// Equip & create the grenade
		if(pPed->IsNetworkClone())
		{
			if(pPed->GetInventory() && !pPed->GetInventory()->GetWeapon(m_uAimAndThrowWeaponHash))
			{
				pPed->GetInventory()->AddWeapon(m_uAimAndThrowWeaponHash);
			}

			pPed->GetWeaponManager()->EquipWeapon(m_uAimAndThrowWeaponHash, -1, true, false, CPedEquippedWeapon::AP_LeftHand);
		}
		else
		{			
			pPed->GetWeaponManager()->EquipWeapon(m_uAimAndThrowWeaponHash, -1, true, false, CPedEquippedWeapon::AP_LeftHand);
		}

		// Make the grenade invisible (gets set to visible again once we have reached a certain point in the throw animation).
		CObject *pGrenadeObject = pPed->GetWeaponManager()->GetEquippedWeaponObject();
		if (pGrenadeObject)
		{
			pGrenadeObject->SetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY, false, true);
		}

		// Update the pWeapon/pWeaponInfo
		pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
		pWeaponInfo = pWeapon ? pWeapon->GetWeaponInfo() : NULL;
	}
}
#endif	//ENABLE_GUN_PROJECTILE_THROW

#if FPS_MODE_SUPPORTED
bool CTaskAimAndThrowProjectile::HandleFPSStateUpdate()
{
	CPed* pPed = GetPed();
	bool rVal = false;

	if(pPed->IsLocalPlayer())
	{
		bool bUseFPSMode = pPed->IsFirstPersonShooterModeEnabledForPlayer(false);

		if(m_MoveNetworkHelper.IsNetworkActive() && m_bFPSUseAimIKForWeapon)
		{
			m_MoveNetworkHelper.SetFlag(bUseFPSMode, ms_UsingFPSMode);
		}

		if(bUseFPSMode)
		{
			CPedMotionData* pMotionData = pPed->GetMotionData();
			TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE_PROJECTILE, fDropThrowSwitchDelay, 0.5f, 0.0, 2.0f, 0.01f);
			bool bDropThrowSwitch = m_fFPSDropThrowSwitchTimer >= fDropThrowSwitchDelay || m_fFPSDropThrowSwitchTimer < 0.0f;
			bool bNonInterruptibleIntro = m_bPlayFPSRNGIntro || m_bPlayFPSPullPinIntro;

			if(!m_bAllowPriming)
			{
				bool bSwitchedAimingState =  m_bWasAiming != IsAiming(pPed);
				if(bSwitchedAimingState)
				{
					m_bPlayFPSTransition = true;
					rVal = true;
				}
			}
			else if(pPed->GetMotionData()->GetIsFPSRNG() && m_bAllowPriming)
			{
				if(!m_bFPSWantsToThrowWhenReady && !m_bDropProjectile)
				{
					if(IsAiming(pPed) || m_bPlayFPSPullPinIntro)
					{
						m_bFPSWantsToThrowWhenReady = true;
					}
					else
					{
						m_bDropProjectile = true;
					}

					// We just switched to RNG and aren't already playig a RNG intro (if we are, don't interrupt it)
					if(!pPed->GetMotionData()->GetWasFPSRNG() && !bNonInterruptibleIntro)
					{
						if(!m_bCanBePlaced)
						{
							m_bPlayFPSTransition = true;
						}

						m_fFPSDropThrowSwitchTimer = 0.0f;
						m_bPlayFPSFidget = false;
						rVal = true;
					}
				} 
				else if(!bNonInterruptibleIntro && bDropThrowSwitch)
				{
					if(IsAiming(pPed))
					{
						if(m_bDropProjectile)
						{
							m_bPlayFPSTransition = false;
							m_bDropProjectile = false;
							m_bFPSWantsToThrowWhenReady = true;
							m_fFPSDropThrowSwitchTimer = 0.0f;
							rVal = true;
						}
					}
					else
					{
						if(!m_bDropProjectile)
						{
							m_bPlayFPSTransition = false;
							m_bDropProjectile = true;
							m_bFPSWantsToThrowWhenReady = false;
							m_fFPSDropThrowSwitchTimer = 0.0f;
							rVal = true;
						}
					}
				}
			}
			else if(m_bDropProjectile && IsFiring(pPed) && m_bCanBePlaced && CanPlaceProjectile(pPed))
			{
				m_bPlayFPSTransition = false;
				rVal = true;
			}

			if(GetState() != State_ThrowProjectile && GetState() != State_StreamingClips && 
				!m_bDropProjectile && !m_bFPSWantsToThrowWhenReady &&
				pMotionData && (pMotionData->GetFPSState() != pMotionData->GetPreviousFPSState() || pMotionData->GetToggledStealthWhileFPSIdle()) && !pMotionData->GetWasFPSRNG())
			{
				//Restart task
				m_bPlayFPSTransition = true;
				rVal = true;
			}
		}

		// In this case, the unholster transition may already be playing so we want to skip this
		/*if(!m_bWasInFPSMode && bUseFPSMode && !m_bPlayFPSTransition)
		{
			//m_bSkipFPSOutroState = true;
			m_bPlayFPSTransition = true;
			rVal = true;
		}
		else if(m_bWasInFPSMode && !bUseFPSMode)
		{
			m_bPlayFPSTransition = false;
			rVal = true;
		}

		m_bWasInFPSMode = bUseFPSMode;*/
	}

	return rVal;
}
#endif

CTask::FSM_Return CTaskAimAndThrowProjectile::ProcessPreFSM()
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	CPedWeaponManager* pWM = pPed->GetWeaponManager();
	if(!weaponVerifyf(pWM, "CTaskAimAndThrowProjectile :: Player has no weapon manager!"))
		return FSM_Quit;

	if(!pPed->IsNetworkClone() && !HasValidWeapon(pPed))
	{
		taskDebugf1("CTaskAimAndThrowProjectile::ProcessPreFSM: FSM_Quit - !HasValidWeapon. Previous State: %d", GetPreviousState());
		return FSM_Quit;
	}

	// Update the heading
	pPed->SetPedResetFlag( CPED_RESET_FLAG_SyncDesiredHeadingToCurrentHeading, false );
	pPed->SetPedResetFlag( CPED_RESET_FLAG_IsAiming, true );
	pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPostPreRender, true );
	pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPostPreRenderAfterAttachments, true );

#if FPS_MODE_SUPPORTED 
	bool bFPSMode = pPed->IsFirstPersonShooterModeEnabledForPlayer(false);

	if(!m_bFPSUseAimIKForWeapon && bFPSMode && pPed->GetEquippedWeaponInfo() && pPed->GetEquippedWeaponInfo()->GetUseFPSAimIK())
	{
		m_bFPSUseAimIKForWeapon = true;
	}

	bool bUseFPSModeBehaviour = bFPSMode && m_bFPSUseAimIKForWeapon;

	if(bUseFPSModeBehaviour && m_fFPSTaskNetworkBlendDuration >= 0.0f && m_fFPSTaskNetworkBlendTimer <= m_fFPSTaskNetworkBlendDuration)
	{
		m_fFPSTaskNetworkBlendTimer += fwTimer::GetTimeStep();
	}

	if(bUseFPSModeBehaviour && m_fFPSDropThrowSwitchTimer >= 0.0f)
	{
		m_fFPSDropThrowSwitchTimer += fwTimer::GetTimeStep();
	}

	bool bJustSwitchedToFPSMode = bFPSMode && !pPed->GetMotionData()->GetWasUsingFPSMode();

	bool bTaskNetworkBlendedIn = m_fFPSTaskNetworkBlendTimer > m_fFPSTaskNetworkBlendDuration;
	if(bUseFPSModeBehaviour && (!m_bFPSPlayingUnholsterIntro || bTaskNetworkBlendedIn) &&
		(bJustSwitchedToFPSMode || m_bPlayFPSPullPinIntro || pPed->GetMotionData()->GetIsFPSRNG() || GetState() == State_HoldingProjectile || GetState() == State_Intro || GetState() == State_ThrowProjectile || GetState() == State_Blocked))
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_FPSAllowAimIKForThrownProjectile, true);
	}

	if (bUseFPSModeBehaviour && GetState() == State_PlaceProjectile)
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_DisableFPSArmIK, true);
	}

	if(bUseFPSModeBehaviour && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PlayFPSIdleFidgetsForProjectile))
	{
		m_bMoveFPSFidgetEnded = false;
	}
#endif

	CWeapon* pWeapon = pWM->GetEquippedWeapon();
	bool bAttackTriggerHeld = pPed->IsLocalPlayer() && CThrowProjectileHelper::IsPlayerPriming(pPed);
#if FPS_MODE_SUPPORTED
	if(bUseFPSModeBehaviour)
	{
		bAttackTriggerHeld |= pPed->GetMotionData()->GetIsFPSRNG();
	}
#endif

	if(pWeapon && pWeapon->GetWeaponInfo() && pWeapon->GetWeaponInfo()->GetIsThrownWeapon() && !bAttackTriggerHeld)
	{
		m_bAllowPriming = true;
	}

#if ENABLE_GUN_PROJECTILE_THROW
	if (m_bAimGunAndThrow)
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_IsThrowingProjectileWhileAiming, true);

		// If the player releases the grenade throw DPad input, unflag it so we can detonate the projectile instantly.
		CControl *pControl = pPed->GetControlFromPlayer();
		if (m_bProjectileThrownFromDPadInput && pControl && pControl->GetPedThrowGrenade().IsReleased())
		{
			m_bProjectileThrownFromDPadInput = false;
		}
	}
#endif	//ENABLE_GUN_PROJECTILE_THROW


	// cache weapon attributes
	if(!m_bHasCachedWeaponValues)
	{
		bool bHasCached = true;

		if(pWeapon)
		{
			// check to see if this is placeable
			const CWeaponInfo* pWeaponInfo = pWeapon->GetWeaponInfo();
			if(pWeaponInfo)
			{
				m_bCanBePlaced = pWeaponInfo->GetProjectileCanBePlaced(); 

#if ENABLE_GUN_PROJECTILE_THROW
				if (m_bAimGunAndThrow)
				{
					m_bCanBePlaced = false;
				}
#endif	//ENABLE_GUN_PROJECTILE_THROW

				const CAmmoInfo* pAmmoInfo = pWeaponInfo->GetAmmoInfo();
				if(pAmmoInfo && pAmmoInfo->GetIsClassId(CAmmoProjectileInfo::GetStaticClassId()))
				{
					m_bPinPulled = m_bHasNoPullPin = static_cast<const CAmmoProjectileInfo*>(pAmmoInfo)->GetNoPullPin() || pWeapon->GetIsCooking(); 
				}
				else
				{
					bHasCached = false;
				}
			}
			else
				bHasCached = false;
		}
		else
			bHasCached = false;

		m_bHasCachedWeaponValues = bHasCached;
	}

	// force target update
	UpdateTarget(pPed);

	s32 nCurrentState = GetState();
	if (pPed->IsLocalPlayer())
	{
		if(!pPed->GetUsingRagdoll() && !pPed->GetPlayerInfo()->AreControlsDisabled())
		{
			if( nCurrentState != State_PlaceProjectile && /*nCurrentState != State_StreamingClips && */!m_bDropProjectile)
			{
				const camFrame& frame = camInterface::GetPlayerControlCamAimFrame();
				float aimHeading = frame.ComputeHeading();
#if FPS_MODE_SUPPORTED
				if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false, false, true, true))
				{
					aimHeading = camInterface::GetPlayerControlCamHeading(*pPed);
				}
#endif
				aimHeading = fwAngle::LimitRadianAngle(aimHeading);
				pPed->SetDesiredHeading(aimHeading);
			}
		}
	}
	else
	{
		bool bClonedPlayer = pPed->IsNetworkClone() && pPed->IsPlayer();

		if (m_target.GetIsValid() && !bClonedPlayer)
		{
			Vector3 vTargetPos;
			m_target.GetPosition(vTargetPos);
			pPed->SetDesiredHeading(vTargetPos);
		}
	}

	// B*2027357: Don't adjust the capsule size for the non-aiming holding projectile state in first person mode.
	// In first person mode, the holding projectile state is run even if the ped isn't actually aiming unlike third-person.
	// Therefore the holding projectile state while not aiming is analogous to the non-aiming state in third-person and should therefore have the same behavior as that.
	bool bAdjustCapsuleForHolding = true;
	if(pPed->IsLocalPlayer() && pPed->IsFirstPersonShooterModeEnabledForPlayer() && !IsAiming(pPed))
		bAdjustCapsuleForHolding = false;

	if( (nCurrentState == State_HoldingProjectile && bAdjustCapsuleForHolding) ||
		nCurrentState == State_ThrowProjectile ||
		nCurrentState == State_Reload )
	{
		if (!pPed->GetPedResetFlag(CPED_RESET_FLAG_IsNearDoor)) //not near doors!
			pPed->SetDesiredMainMoverCapsuleRadius( CTaskAimAndThrowProjectile::m_sfThrowingMainMoverCapsuleRadius );
	}

	if (CTaskAimGun::ms_bUseTorsoIk)
	{
		// Pitch handled via animation
		m_ikInfo.SetDisablePitchFixUp(true);

		//TUNE_GROUP_FLOAT(NEW_GUN_TASK, SPINE_MATRIX_INTERP_RATE, 4.0f, 0.0f, 1000.0f, 0.1f);
		m_ikInfo.SetSpineMatrixInterpRate(4.0f);

		// Apply the Ik
		m_ikInfo.ApplyIkInfo(pPed);
	}

	m_fBlockAimingIndependentMoverTime -= GetTimeStep();
	if(m_fBlockAimingIndependentMoverTime >= 0.f)
	{
		CTaskMotionBase* pMotionTask = pPed->GetPrimaryMotionTask();
		if(pMotionTask && pMotionTask->GetTaskType() == CTaskTypes::TASK_MOTION_PED)
		{
			CTaskMotionPed* pMotionPedTask = static_cast<CTaskMotionPed*>(pMotionTask);
			pMotionPedTask->SetDoAimingIndependentMover(true);
		}
	}

#if FPS_MODE_SUPPORTED
	bool bFPSLTorRNG = pPed->GetMotionData() && (pPed->GetMotionData()->GetIsFPSLT() || pPed->GetMotionData()->GetIsFPSRNG());
	//Don't do sprint/aim breakout if swimming 
	if(pPed->IsLocalPlayer() && bUseFPSModeBehaviour && !pPed->GetIsSwimming() && !bFPSLTorRNG && !pPed->IsFirstPersonShooterModeEnabledForPlayer() && !m_bFPSWantsToThrowWhenReady && !m_bDropProjectile)
	{
		bool bHitSprintMBR = false;
		const Vector2 &desiredMBR = pPed->GetMotionData()->GetDesiredMoveBlendRatio();
		if(abs(desiredMBR.x) > 0.01f)
		{
			bHitSprintMBR = pPed->GetMotionData()->GetIsSprinting(desiredMBR.Mag());
		}
		else
		{
			bHitSprintMBR = pPed->GetMotionData()->GetIsSprinting(desiredMBR.y);
		}

		float fSprintResult = pPed->GetPlayerInfo()->ControlButtonSprint(CPlayerInfo::SPRINT_ON_FOOT, true);

		static dev_float s_fSprintResultThreshold = 1.25f;

		bool bSprinting = bHitSprintMBR && (fSprintResult >= s_fSprintResultThreshold);

		if(bSprinting && GetState() != State_ThrowProjectile && GetState() != State_StreamingClips && GetState() != State_Blocked
#if FPS_MODE_SUPPORTED
		&& !pPed->GetPedResetFlag(CPED_RESET_FLAG_WeaponBlockedInFPSMode)
#endif // FPS_MODE_SUPPORTED
		)
		{
			pPed->GetPlayerInfo()->SprintAimBreakOut();
			taskDebugf1("CTaskAimAndThrowProjectile::ProcessPreFSM: FSM_Quit - bSprinting. Previous State: %d", GetPreviousState());
			return FSM_Quit;
		}
	}

	UpdateAdditives();

#endif // FPS_MODE_SUPPORTED

	return FSM_Continue;	
}

CTask::FSM_Return CTaskAimAndThrowProjectile::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		FSM_State(State_Start)
			FSM_OnUpdate
				Start_OnUpdate(pPed);
			
		// Task start
		FSM_State(State_StreamingClips)
			FSM_OnEnter
				StreamingClips_OnEnter(pPed);
			FSM_OnUpdate
				return StreamingClips_OnUpdate(pPed);

		// Task intro
		FSM_State(State_Intro)	
			FSM_OnEnter
				Intro_OnEnter(pPed);
			FSM_OnUpdate
				return Intro_OnUpdate(pPed);
#if FPS_MODE_SUPPORTED
			FSM_OnExit
				Intro_OnExit(pPed);
#endif
		// Pull Pin
		FSM_State(State_PullPin)			
			FSM_OnUpdate
				return PullPin_OnUpdate();

		// Holding Projectile
		FSM_State(State_HoldingProjectile)
			FSM_OnEnter
				HoldingProjectile_OnEnter(pPed);
			FSM_OnUpdate
				return HoldingProjectile_OnUpdate(pPed);

		// Throw Projectile
		FSM_State(State_ThrowProjectile)
			FSM_OnEnter
				ThrowProjectile_OnEnter(pPed);
			FSM_OnUpdate
				return ThrowProjectile_OnUpdate(pPed);
			FSM_OnExit
				ThrowProjectile_OnExit(pPed);
		
		// Place Projectile
		FSM_State(State_PlaceProjectile)
			FSM_OnEnter
				StatePlaceProjectile_OnEnter(pPed);
			FSM_OnUpdate
				return StatePlaceProjectile_OnUpdate(pPed);

		FSM_State(State_CombatRoll)
			FSM_OnEnter
				CombatRoll_OnEnter(pPed);
			FSM_OnUpdate
				return CombatRoll_OnUpdate(pPed);

		// Reload Projectile
		FSM_State(State_Reload)
			FSM_OnEnter
				Reload_OnEnter(pPed);
			FSM_OnUpdate
				return Reload_OnUpdate(pPed);
			FSM_OnExit
				Reload_OnExit(pPed);

		// Task intro
		FSM_State(State_CloneIdle)
			FSM_OnEnter
				CloneIdle_OnEnter(pPed);
			FSM_OnUpdate
				return CloneIdle_OnUpdate(pPed);

		// Task intro
		FSM_State(State_Outro)
			FSM_OnEnter
			Outro_OnEnter(pPed);
		FSM_OnUpdate
			return Outro_OnUpdate(pPed);

		// Weapon blocked
		FSM_State(State_Blocked)
			FSM_OnEnter
			Blocked_OnEnter(pPed);
		FSM_OnUpdate
			return Blocked_OnUpdate(pPed);

		// Finish
		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

CTask::FSM_Return CTaskAimAndThrowProjectile::ProcessPostFSM()
{
	CPed* pPed = GetPed();
	
	if (m_bCreateInvincibileProjectile)
	{
		CPedWeaponManager* pWeaponManager = pPed->GetWeaponManager();
		CObject* pWeaponObj = pWeaponManager ? pWeaponManager->GetEquippedWeaponObject() : NULL;

		if (pWeaponObj && !pWeaponObj->m_nPhysicalFlags.bNotDamagedByAnything)
		{
			pWeaponObj->m_nPhysicalFlags.bNotDamagedByAnything = true;
		}
	}

	return FSM_Continue;
}

bool CTaskAimAndThrowProjectile::IsAiming(CPed* FPS_MODE_SUPPORTED_ONLY(pPed))
{
	bool bUseFPSAimIK = false;
#if FPS_MODE_SUPPORTED
	bUseFPSAimIK = pPed->GetEquippedWeaponInfo() ? pPed->GetEquippedWeaponInfo()->GetUseFPSAimIK() : false;
#endif

	return CPlayerInfo::IsAiming(false) || m_bIsAiming FPS_MODE_SUPPORTED_ONLY(|| (pPed->IsFirstPersonShooterModeEnabledForPlayer() && !bUseFPSAimIK));
}

bool CTaskAimAndThrowProjectile::IsPriming(CPed* pPed)
{
	return CThrowProjectileHelper::IsPlayerPriming(pPed) || m_bPriming;
}

// Note: this function was written assuming the logic CTaskAimAndThrowProjectile::IsFiring() passes
bool CTaskAimAndThrowProjectile::CanPlaceProjectile( CPed* pPed )
{
	// Check the current ground normal slope
	Vector3 vGroundNormal = pPed->GetGroundNormal();
	if( vGroundNormal.z < CTaskBomb::ms_fMaxPlantGroundNormalZ )
		return false;

	static dev_float sfProbeRadius = 0.1f;	

	// Fill in the descriptor for the batch test.
	WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
	WorldProbe::CShapeTestSingleResult capsuleResult;

	u32 nIncludeFlags = (ArchetypeFlags::GTA_ALL_TYPES_WEAPON & ~ArchetypeFlags::GTA_PROJECTILE_TYPE & ~ArchetypeFlags::GTA_PED_TYPE & ~ArchetypeFlags::GTA_RIVER_TYPE);
	u32 nTypeFlags = ArchetypeFlags::GTA_WEAPON_TEST;

	float fDesiredHeading = pPed->GetDesiredHeading();
	Vec3V vPedForward( -rage::Sinf( fDesiredHeading ), rage::Cosf( fDesiredHeading ), 0.0f );
	vPedForward = NormalizeSafe( vPedForward, pPed->GetTransform().GetB() );

	bool bFoundTarget = false;
	float fZOffset = -0.3f;
	
	Vec3V vPedPositionFlat = pPed->GetTransform().GetPosition();
	vPedPositionFlat.SetZf( 0.0f );

	Vec3V vStart( VEC3_ZERO );
	Vec3V vEnd( VEC3_ZERO );

	const int nNumProbes = 3;
	Vec3V vHitPositions[nNumProbes];
	Vec3V vHitNormals[nNumProbes];
	ScalarV fFlatDistSq[nNumProbes];
	ScalarV fShortestFlatDistSq( 10000.0f );

	// Walk through each probe and determine the overall distance to the closest surface (in the desired direction)
	for( int i = 0; i < nNumProbes; ++i )
	{
		fFlatDistSq[i].Setf( 10000.0f );

		vStart = pPed->GetTransform().GetPosition();
		vStart.SetZf( vStart.GetZf() + fZOffset );
		vEnd = vStart + Scale( vPedForward, ScalarV( CTaskBomb::ms_fVerticalProbeDistance ) );

#if __BANK
		char szDebugText[128];
		formatf( szDebugText, "BOMB_COLLISION_SPHERE_START_%d", i );
		CTask::ms_debugDraw.AddSphere( vStart, sfProbeRadius, Color_green, 2, atStringHash(szDebugText), false );
		formatf( szDebugText, "BOMB_COLLISION_SPHERE_END_%d", i );
		CTask::ms_debugDraw.AddSphere( vEnd, sfProbeRadius, Color_green4, 2, atStringHash(szDebugText), false );
		formatf( szDebugText, "BOMB_COLLISION_ARROW_%d", i );
		CTask::ms_debugDraw.AddArrow( vStart, vEnd, sfProbeRadius, Color_blue, 2, atStringHash(szDebugText) );
#endif // __BANK

		capsuleResult.Reset();
		capsuleDesc.SetResultsStructure( &capsuleResult );
		capsuleDesc.SetCapsule( VEC3V_TO_VECTOR3( vStart ), VEC3V_TO_VECTOR3( vEnd ), sfProbeRadius );
		capsuleDesc.SetExcludeEntity( pPed );
		capsuleDesc.SetIncludeFlags( nIncludeFlags );
		capsuleDesc.SetTypeFlags( nTypeFlags );
		capsuleDesc.SetIsDirected( true );
		capsuleDesc.SetTreatPolyhedralBoundsAsPrimitives( false );

		if( WorldProbe::GetShapeTestManager()->SubmitTest( capsuleDesc ) )
		{
			if( !CPhysics::GetEntityFromInst( capsuleResult[0].GetHitInst() ) )
				continue;
#if __BANK
			char szDebugText[128];
			formatf( szDebugText, "BOMB_COLLISION_SPHERE_%d", i );
			CTask::ms_debugDraw.AddSphere( capsuleResult[0].GetHitPositionV(), 0.05f, Color_red, 2, atStringHash(szDebugText), false );
#endif // __BANK

			bFoundTarget = true;
			vHitPositions[i] = capsuleResult[0].GetHitPositionV();
			vHitNormals[i] = capsuleResult[0].GetHitNormalV();

			Vec3V vHitPositionFlat = capsuleResult[0].GetHitPositionV();
			vHitPositionFlat.SetZf( 0.0f );
			fFlatDistSq[i] = MagSquared( vHitPositionFlat - vPedPositionFlat );

			// Determine if this is the shortest flat distance
			if( IsTrue( IsLessThan( fFlatDistSq[i], fShortestFlatDistSq ) ) )
				fShortestFlatDistSq = fFlatDistSq[i];
		}

		fZOffset += 0.3f;
	}

	// If we found a target we need to analyze the shortest distance
	if( bFoundTarget )
	{
		static dev_float sfFlatDistanceThreshold = 0.15f;

		// If the shortest distance is within a designated threshold then we should allow the plant
		if( IsTrue( IsClose( fShortestFlatDistSq, fFlatDistSq[1], ScalarV( sfFlatDistanceThreshold ) ) ) )
			return true;
	}

	// Disable ground plants on uphill slopes to prevent hand clipping into ground
	bool bDisableGroundPlantOnUphillSlopes = (vGroundNormal.Dot(VEC3V_TO_VECTOR3(pPed->GetTransform().GetForward())) < 0.) && vGroundNormal.z < CTaskBomb::ms_fUphillMaxPlantGroundNormalZ;

	// Otherwise, if we didn't find an intended surface and are still then allow the ground plant
	if( !bFoundTarget && pPed->GetMotionData()->GetCurrentMoveBlendRatio().Mag2() <= rage::square( MBR_WALK_BOUNDARY ) && !bDisableGroundPlantOnUphillSlopes )
		return true;

	return false;
}

bool CTaskAimAndThrowProjectile::IsFiring(CPed* pPed)
{
	bool bRNGifFPS = false;

#if FPS_MODE_SUPPORTED
	bRNGifFPS = pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && m_bFPSUseAimIKForWeapon && pPed->GetMotionData() && pPed->GetMotionData()->GetIsFPSRNG();
#endif
	return (CThrowProjectileHelper::IsPlayerFiring(pPed) || m_bIsFiring) && !bRNGifFPS;
} 

CTaskInfo* CTaskAimAndThrowProjectile::CreateQueriableState() const
{
	bool bHasAimTarget = false;
	Vector3 vAimTarget = VEC3_ZERO; 

	if(m_target.GetIsValid())
	{
		bHasAimTarget = true;
		m_target.GetPosition(vAimTarget);
	}

	bool bValidTarget = m_bHasTargetPosition && GetState() == State_HoldingProjectile;
	return rage_new CClonedAimAndThrowProjectileInfo(m_pIgnoreCollisionEntity, GetState(), m_bPriming, CPlayerInfo::IsAiming(), m_bIsFiring, m_bDropProjectile, m_bThrowPlace, 
#if ENABLE_GUN_PROJECTILE_THROW
		m_bAimGunAndThrow, m_uAimAndThrowWeaponHash,
#endif	//ENABLE_GUN_PROJECTILE_THROW
		m_fThrowPitch, m_fCurrentPitch, bValidTarget, m_vTargetPosition, bHasAimTarget, m_LastLocalRollToken, vAimTarget);
}

void CTaskAimAndThrowProjectile::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	Assert(pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_AIM_PROJECTILE);
	CTaskFSMClone::ReadQueriableState(pTaskInfo);

	CClonedAimAndThrowProjectileInfo* pThisInfo = static_cast<CClonedAimAndThrowProjectileInfo*>(pTaskInfo);

	m_bPriming           = pThisInfo->IsPriming();
	m_bIsAiming          = pThisInfo->IsAiming();
	m_bIsFiring          = pThisInfo->IsFiring();
	m_bDropProjectile    = pThisInfo->IsDropping();
	m_bThrowPlace		 = pThisInfo->IsThrowPlace();
#if ENABLE_GUN_PROJECTILE_THROW
    m_bAimGunAndThrow    = pThisInfo->IsAimGunAndThrow();
	m_uAimAndThrowWeaponHash = pThisInfo->GetAimAndThrowWeaponHash();
#endif	//ENABLE_GUN_PROJECTILE_THROW
	m_fThrowPitch        = pThisInfo->GetThrowPitch();
	m_fCurrentPitch      = pThisInfo->GetCurrentPitch();
	m_bHasTargetPosition = pThisInfo->HasTargetPosition();
    m_LastLocalRollToken = pThisInfo->GetRollToken();
	m_vTargetPosition    = pThisInfo->GetTargetPosition();
	m_uAimAndThrowWeaponHash = pThisInfo->GetAimAndThrowWeaponHash();
	m_pIgnoreCollisionEntity = pThisInfo->GetIgnoreCollisionEntity();

	if(pThisInfo->HasAimTarget())
		m_target.SetPosition(pThisInfo->GetAimTarget());
}

void CTaskAimAndThrowProjectile::OnCloneTaskNoLongerRunningOnOwner()
{
	SetStateFromNetwork(State_Finish);
}

CTaskAimAndThrowProjectile::FSM_Return CTaskAimAndThrowProjectile::UpdateClonedFSM(const s32 iState, const FSM_Event iEvent)
{
	if(iState != State_Finish)
	{
		if(iEvent == OnUpdate)
		{
			// Add a catch that stops us trying to switch states and create (and apply) a move network player to a ped that's just been blown up...
			if(GetPed() && GetPed()->GetUsingRagdoll())
			{
				SetState(State_Finish);
				return FSM_Continue;
			}

			s32 iStateFromNetwork = GetStateFromNetwork();

			// if the network state is telling us to finish...
			if(iStateFromNetwork != GetState() && iStateFromNetwork == State_Finish && GetState() != State_ThrowProjectile GUN_PROJECTILE_THROW_ONLY(&& !m_bAimGunAndThrow))
			{
				SetState(iStateFromNetwork);
			}

            if(iStateFromNetwork == State_CombatRoll)
            {
                const unsigned WRAP_THRESHOLD = MAX_ROLL_TOKEN / 2;
                if(m_LastCloneRollToken < m_LastLocalRollToken || ((m_LastCloneRollToken - m_LastLocalRollToken) > WRAP_THRESHOLD))
                {
                    SetState(iStateFromNetwork);
				    return FSM_Continue;
                }
            }
		}
	}

	return UpdateFSM(iState, iEvent);
}

CTaskFSMClone* CTaskAimAndThrowProjectile::CreateTaskForClonePed(CPed* UNUSED_PARAM(pPed))
{
	return rage_new CTaskAimAndThrowProjectile(m_target, m_pIgnoreCollisionEntity);
}

CTaskFSMClone* CTaskAimAndThrowProjectile::CreateTaskForLocalPed(CPed* pPed)
{
	return CreateTaskForClonePed(pPed);
}

#if !__FINAL
const char * CTaskAimAndThrowProjectile::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_Start&&iState<=State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_StreamingClips",
		"State_Intro",
		"State_PullPin",
		"State_HoldingProjectile",
		"State_ThrowProjectile",
		"State_PlaceProjectile",
		"State_CombatRoll",
		"State_Reload",
		"State_CloneIdle",
		"State_Outro",
		"State_Blocked",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif

//////////////////////////////////////////////////////////////////////////
// CClonedAimAndThrowProjectileInfo
//////////////////////////////////////////////////////////////////////////
CClonedAimAndThrowProjectileInfo::CClonedAimAndThrowProjectileInfo(CEntity* pIgnoreCollisionEntity, const u32 state, bool bPriming, bool bAiming, bool bFiring, bool bDropping, bool bThrowPlace, 
#if ENABLE_GUN_PROJECTILE_THROW
	bool bAimGunAndThrow, u32 uAimAndThrowWeaponHash,
#endif	//ENABLE_GUN_PROJECTILE_THROW
	float fThrowPitch, float fCurrentPitch, bool bHasTargetVector, const Vector3& vTarget, bool bHasAimTarget, u8 rollToken, const Vector3& vAimTarget)
	: m_bIsPriming(bPriming)
	, m_bIsAiming(bAiming)
	, m_bIsFiring(bFiring)
	, m_bIsDropping(bDropping)
	, m_bThrowPlace(bThrowPlace)
#if ENABLE_GUN_PROJECTILE_THROW
    , m_bAimGunAndThrow(bAimGunAndThrow)
	, m_uAimAndThrowWeaponHash(uAimAndThrowWeaponHash)
#endif	//ENABLE_GUN_PROJECTILE_THROW
    , m_RollToken(rollToken)
	, m_fThrowPitch(fThrowPitch)
	, m_fCurrentPitch(fCurrentPitch)
	, m_bHasTargetPosition(bHasTargetVector)
	, m_vTargetPosition(vTarget)
	, m_bHasAimTarget(bHasAimTarget)
	, m_vAimTarget(vAimTarget)
	, m_IgnoreCollisionEntity(pIgnoreCollisionEntity)
{
	CSerialisedFSMTaskInfo::SetStatusFromMainTaskState(state);
}

CTaskFSMClone* CClonedAimAndThrowProjectileInfo::CreateCloneFSMTask()
{
#if ENABLE_GUN_PROJECTILE_THROW
    // aim guns and throws are always created locally
    if(m_bAimGunAndThrow)
    {
        return 0;
    }
#endif	//ENABLE_GUN_PROJECTILE_THROW

	CWeaponTarget tTarget; 
	if(m_bHasAimTarget)
		tTarget.SetPosition(m_vAimTarget);

	return rage_new CTaskAimAndThrowProjectile(tTarget, m_IgnoreCollisionEntity.GetEntity());
}

CTaskFSMClone* CClonedAimAndThrowProjectileInfo::CreateLocalTask(fwEntity* UNUSED_PARAM(pEntity))
{
	return CreateCloneFSMTask();
}

//////////////////////////////////////////////////////////////////////////
// CClonedThrowProjectileInfo
//////////////////////////////////////////////////////////////////////////
CClonedThrowProjectileInfo::CClonedThrowProjectileInfo(const u32 state, bool bHasTarget, const Vector3& vTarget, const fwMvClipSetId& clipsetId, const fwMvClipId& introClipId, const fwMvClipId& baseClipId, const fwMvClipId& throwLongClipId, const fwMvClipId& throwShortClipId, const u32 weaponNameHash, const bool rightHand)
	: m_bHasTargetPosition(bHasTarget)
	, m_vTargetPosition(vTarget)
	, m_clipSetId(clipsetId)
	, m_IntroClipId(introClipId)
	, m_BaseClipId(baseClipId)	
	, m_ThrowLongClipId(throwLongClipId)
	, m_ThrowShortClipId(throwShortClipId)
	, m_WeaponNameHash(weaponNameHash)
	, m_bRightHand(rightHand)
{
	CSerialisedFSMTaskInfo::SetStatusFromMainTaskState(state);
}

CTaskFSMClone *CClonedThrowProjectileInfo::CreateCloneFSMTask()
{
	CWeaponTarget tTarget; 
	if(m_bHasTargetPosition)
		tTarget.SetPosition(m_vTargetPosition);

	return rage_new CTaskThrowProjectile(tTarget, false);
}

CTaskFSMClone* CClonedThrowProjectileInfo::CreateLocalTask(fwEntity* UNUSED_PARAM(pEntity))
{
	return CreateCloneFSMTask();
}

//////////////////////////////////////////////////////////////////////////
// CTaskThrowProjectile
//////////////////////////////////////////////////////////////////////////

const fwMvRequestId CTaskThrowProjectile::ms_StartThrow("Throw",0xCDEC4431);
const fwMvRequestId CTaskThrowProjectile::ms_StartPrime("Prime",0x4CC8E30C);
const fwMvRequestId CTaskThrowProjectile::ms_TurnRight("TurnRight",0xFC128324);
const fwMvRequestId CTaskThrowProjectile::ms_TurnLeft("TurnLeft",0x5EF76E0C);
const fwMvClipId CTaskThrowProjectile::ms_MvIntroClip("IntroClip",0x4139FEC0);
const fwMvClipId CTaskThrowProjectile::ms_MvPrimeClip("PrimeClip",0xB7DB17D8);
const fwMvClipId CTaskThrowProjectile::ms_MvThrowClip("ThrowClip",0xC4C387F);
const fwMvClipId CTaskThrowProjectile::ms_MvThrowFaceCoverClip("ThrowClipFaceCover",0x1d3819b3);
const fwMvClipId CTaskThrowProjectile::ms_MvTurnLeftClip("TurnLeftClip",0xF4BD0E1F);
const fwMvClipId CTaskThrowProjectile::ms_MvTurnRightClip("TurnRightClip",0xBB79BF56);
const fwMvClipId CTaskThrowProjectile::ms_MvThrowShortClip("ThrowShortClip",0xCD4A3700);
const fwMvClipId CTaskThrowProjectile::ms_MvThrowShortFaceCoverClip("ThrowShortClipFaceCover",0x54e9685b);
const fwMvBooleanId	CTaskThrowProjectile::ms_IntroCompleteEvent("IntroComplete",0x44A36F76);
const fwMvBooleanId	CTaskThrowProjectile::ms_ThrowCompleteEvent("ThrowComplete",0x4BFCEDBF);
const fwMvBooleanId	CTaskThrowProjectile::ms_EnterPrimeEvent("EnterPrime",0x8C99CF04);
const fwMvFloatId	CTaskThrowProjectile::ms_ThrowPhase("ThrowPhase",0x9D46658E);
const fwMvFloatId	CTaskThrowProjectile::ms_TurnPhase("TurnPhase",0x9B1DE516);
const fwMvFloatId	CTaskThrowProjectile::ms_ThrowDirection("ThrowDirection",0x1C45680E);

CTaskThrowProjectile::CTaskThrowProjectile(const CWeaponTarget& targeting, const bool bRotateToFaceTarget, const fwMvClipSetId &clipSetId)
: m_target(targeting)
, m_clipSetId(clipSetId)
, m_bRotateToFaceTarget(bRotateToFaceTarget)
, m_bHasThrownProjectile(false)
, m_bPriming(false)
, m_IntroClipId(CLIP_ID_INVALID)
, m_BaseClipId(CLIP_ID_INVALID)
, m_ThrowLongClipId(CLIP_ID_INVALID)
, m_ThrowLongFaceCoverClipId(CLIP_ID_INVALID)
, m_ThrowShortClipId(CLIP_ID_INVALID)
, m_ThrowShortFaceCoverClipId(CLIP_ID_INVALID)
, m_pRightTurnClip(NULL)
, m_pLeftTurnClip(NULL)
, m_fThrowPhase(0.5f)
, m_iTurnTimer(0)
, m_bFire(false)
, m_bTurning(false)
{
	SetInternalTaskType(CTaskTypes::TASK_THROW_PROJECTILE);
}
	
CTaskThrowProjectile::~CTaskThrowProjectile()
{
}

void CTaskThrowProjectile::CleanUp() 
{
	GetPed()->GetMovePed().ClearTaskNetwork(m_MoveNetworkHelper, NORMAL_BLEND_DURATION);
}

CTask::FSM_Return CTaskThrowProjectile::ProcessPreFSM()
{
	CPed *pPed = GetPed(); 

	pPed->SetPedResetFlag( CPED_RESET_FLAG_IsAiming, true );
	pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPostPreRender, true );

	// Force the do nothing motion state so we don't get any underlying motion coming through
	pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_AnimatedVelocity);

	if(!weaponVerifyf(pPed->GetWeaponManager(), "Weapon Manager is NULL!"))
		return FSM_Continue;

	if(!pPed->IsNetworkClone() && GetState()!=State_ThrowProjectile)
	{
		CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
		if(!pWeapon || !pWeapon->GetWeaponInfo()->GetIsThrownWeapon())
		{
			if(pPed->GetIsInCover()) 
			{
				TUNE_GROUP_FLOAT(COVER_TUNE, PROJECTILE_BLEND_OUT_DURATION, 1.0f, 0.0f, 10.0f, 0.01f);
				StopClip(PROJECTILE_BLEND_OUT_DURATION);
			}
			return FSM_Quit;
		}
	}

	// update target
	if(pPed && pPed->IsPlayer())
	{
		if(pPed->IsNetworkClone())
		{
			if(static_cast<CNetObjPlayer*>(pPed->GetNetworkObject())->HasValidTarget())
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
	return FSM_Continue;	
}

bool CTaskThrowProjectile::ProcessPostPreRender()
{
	if (m_bFire)
	{
		CPed* pPed = GetPed();
		// grab a pointer to the peds weapon, this should always be valid for a local ped, its an exit condition in ProcessPreFSM
		CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
		if (pWeapon)
		{
			taskFatalAssertf(pWeapon, "Valid weapon expected!");

			// Calculate the launch position
			const s32 nBoneIndex = pPed->GetBoneIndexFromBoneTag(BONETAG_R_HAND);
			Assert(nBoneIndex != BONETAG_INVALID);

			Matrix34 weaponMat;
			CObject* pWeaponObject = pPed->GetWeaponManager()->GetEquippedWeaponObject();
			if( pWeaponObject )
			{
				weaponMat = MAT34V_TO_MATRIX34(pWeaponObject->GetMatrix());
			}
			else
			{
				pPed->GetGlobalMtx(nBoneIndex, weaponMat);
			}

			// Initialize start and end positions to zero
			Vector3 vStart(Vector3::ZeroType);
			Vector3 vEnd(Vector3::ZeroType);

			// Player ped
			if( pPed->IsLocalPlayer() )
			{
				// Use camera aim
				const camFrame& aimFrame = camInterface::GetPlayerControlCamAimFrame();
				vStart = aimFrame.GetPosition();
				Vector3 vShot  = aimFrame.GetFront();
				f32 fShotDot = vShot.Dot(weaponMat.d - vStart);
				vStart += fShotDot * vShot;

				bool bValidTargetPosition = false;
				if( !pPed->GetPlayerInfo() || !pPed->GetPlayerInfo()->AreControlsDisabled() )
				{
					// First check for the more precise reading, the camera aim at position
					if( pWeaponObject )
					{
						Vector3 vStartTemp( Vector3::ZeroType );
						bValidTargetPosition = pWeapon->CalcFireVecFromAimCamera( pPed->GetIsInVehicle() ? static_cast<CEntity*>(pPed->GetMyVehicle()) : pPed, weaponMat, vStartTemp, vEnd );
					}
				}

				// B*1983883: If we're throwing from cover and aim vector from camera isn't colliding with the cover, but the projectile is going to,
				// then offset it slightly by the height difference (within reason).
				float fOffsetZ = 0.0f;
				if (OffsetProjectileIfNecessaryInCover(pPed, aimFrame.GetPosition(), vEnd, weaponMat.d, vEnd, weaponMat, fOffsetZ))
				{
					weaponMat.d.z += fOffsetZ;
				}

				// Now scale up to weapon range
				if( !bValidTargetPosition && m_target.GetIsValid() )
				{
					bValidTargetPosition = m_target.GetPositionWithFiringOffsets( pPed, vEnd );
				}
			}
			else // AI Ped
			{
				// Compute vStart
				vStart = pPed->GetBonePositionCached((eAnimBoneTag)BONETAG_R_HAND);

				// Compute vEnd
				if(m_target.GetIsValid())
				{
					m_target.GetPosition(vEnd);
					Assert(vEnd.FiniteElements()); // Attempting to track down an INF (B*682884)
				}
			}

			float fOverrideLifeTime = -1.0f;			
			if(!pWeapon->GetIsCooking())
			{
				pWeapon->StartCookTimer(fwTimer::GetTimeInMilliseconds());
			}
			if (pWeapon->GetWeaponInfo()->GetProjectileFuseTime() > 0.0f)
			{
				fOverrideLifeTime = Max( 0.0f, pWeapon->GetCookTimeLeft(pPed, fwTimer::GetTimeInMilliseconds()) );
			}

			// Throw the projectile
			CWeapon::sFireParams params(pPed, weaponMat, &vStart, &vEnd);
			params.fOverrideLifeTime = fOverrideLifeTime;
			params.fDesiredTargetDistance = vStart.Dist( vEnd );
			if(pWeapon->Fire(params))
			{
				// Ensure object is removed
				pPed->GetWeaponManager()->DestroyEquippedWeaponObject();
			}
			m_bFire = false;
		}
	}
	return true;
}

CTask::FSM_Return CTaskThrowProjectile::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	FSM_Begin
		// Task start
		FSM_State(State_Start)
			FSM_OnUpdate
				Start_OnUpdate();

		FSM_State(State_Intro)
			FSM_OnEnter
				return Intro_OnEnter();
			FSM_OnUpdate
				return Intro_OnUpdate();

		FSM_State(State_HoldingProjectile)
			FSM_OnEnter
				return HoldingProjectile_OnEnter();
			FSM_OnUpdate
				return HoldingProjectile_OnUpdate();

		FSM_State(State_ThrowProjectile)
			FSM_OnEnter
				return ThrowProjectile_OnEnter();
			FSM_OnUpdate
				return ThrowProjectile_OnUpdate();			

		// Finish
		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

CTaskInfo* CTaskThrowProjectile::CreateQueriableState() const
{
	bool bHasAimTarget = false;
	Vector3 vAimTarget = VEC3_ZERO; 

	if(m_target.GetIsValid())
	{
		bHasAimTarget = true;
		m_target.GetPosition(vAimTarget);
	}

	const CPed* pPed = GetPed();
	u32 uWeaponNameHash = 0;
	bool bRightHand = true;
	const CPedWeaponManager* pWeaponManager = pPed ? pPed->GetWeaponManager() : NULL;
	if (pWeaponManager && pWeaponManager->GetPedEquippedWeapon())
	{
		uWeaponNameHash = pWeaponManager->GetEquippedWeaponHash();
		bRightHand = pWeaponManager->GetPedEquippedWeapon()->GetAttachPoint() == CPedEquippedWeapon::AP_RightHand ? true : false;
	}

	const fwMvClipSetId cloneClipSetId = pPed ? CTaskInCover::ms_Tunables.GetThrowProjectileClipSetIdForPed(*pPed, true) : m_clipSetId;
	return rage_new CClonedThrowProjectileInfo(GetState(), bHasAimTarget, vAimTarget, cloneClipSetId, m_IntroClipId, m_BaseClipId, m_ThrowLongClipId, m_ThrowShortClipId, uWeaponNameHash, bRightHand);
}

void CTaskThrowProjectile::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	Assert(pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_THROW_PROJECTILE);
	CTaskFSMClone::ReadQueriableState(pTaskInfo);

	CClonedThrowProjectileInfo* pThisInfo = static_cast<CClonedThrowProjectileInfo*>(pTaskInfo);

	m_clipSetId = pThisInfo->GetThrowClipSetId();
	m_IntroClipId = pThisInfo->GetIntroClipId();
	if (GetState() != State_HoldingProjectile || m_BaseClipId == CLIP_ID_INVALID) // We set this locally during the hold state
	{
		m_BaseClipId = pThisInfo->GetBaseClipId();
	}
	m_ThrowLongClipId = pThisInfo->GetThrowLongClipId();
	m_ThrowShortClipId = pThisInfo->GetThrowShortClipId();
	
	m_WeaponNameHash = pThisInfo->GetWeaponNameHash();
	m_bRightHand = pThisInfo->GetRightHand();

	if(pThisInfo->HasTargetPosition())
		m_target.SetPosition(pThisInfo->GetTargetPosition());
}

void CTaskThrowProjectile::OnCloneTaskNoLongerRunningOnOwner()
{
	SetStateFromNetwork(State_Finish);
}

CTaskThrowProjectile::FSM_Return CTaskThrowProjectile::UpdateClonedFSM(const s32 iState, const FSM_Event iEvent)
{
	if(iState != State_Finish)
	{
		if(iEvent == OnUpdate)
		{
			// if the network state is telling us to finish...
			s32 iStateFromNetwork = GetStateFromNetwork();
			if(iStateFromNetwork != GetState() && iStateFromNetwork == State_Finish)
			{
				SetState(iStateFromNetwork);
			}
		}
	}

	return UpdateFSM(iState, iEvent);
}

CTaskFSMClone* CTaskThrowProjectile::CreateTaskForClonePed(CPed* UNUSED_PARAM(pPed))
{
	return rage_new CTaskThrowProjectile(m_target, m_bRotateToFaceTarget);
}

CTaskFSMClone* CTaskThrowProjectile::CreateTaskForLocalPed(CPed* pPed)
{
	return CreateTaskForClonePed(pPed);;
}

#if !__FINAL
const char * CTaskThrowProjectile::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_Start&&iState<=State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_Intro",
		"State_HoldingProjectile",
		"State_ThrowProjectile",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif

void CTaskThrowProjectile::DoAbort(const AbortPriority UNUSED_PARAM(iPriority), const aiEvent* pEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	if(pEvent && ((CEvent*)pEvent)->GetEventType() == EVENT_DAMAGE)
	{
		// DROP WEAPON IF SHOT IN THE MIDDLE OF THROWING!
		// drop the weapon straight down, should be pretty effective.
		// Calculate the launch position
		const s32 nBoneIndex = pPed->GetBoneIndexFromBoneTag(BONETAG_R_HAND);
		Assert(nBoneIndex != BONETAG_INVALID);

		Matrix34 boneMat;
		pPed->GetGlobalMtx(nBoneIndex, boneMat);

		// Fire the weapon, note the target position is this peds so it should just drop
		weaponAssert(pPed->GetWeaponManager());
		CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
		if(pWeapon && pWeapon->GetWeaponInfo()->GetIsThrownWeapon())
		{
			float fOverrideLifeTime = -1.0f;
			if (pWeapon->GetWeaponInfo()->GetProjectileFuseTime() > 0.0f)
				fOverrideLifeTime = Max(0.0f, pWeapon->GetCookTimeLeft(pPed, fwTimer::GetTimeInMilliseconds()));

			Vector3 v = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			CWeapon::sFireParams params(pPed, boneMat, NULL, &v);
			params.fOverrideLifeTime = fOverrideLifeTime;
			if(pWeapon->Fire(params))
			{
				pPed->GetWeaponManager()->DestroyEquippedWeaponObject();
			}
		}
	}

	// We are going to abort, so blend out the animation
	StopClip(NORMAL_BLEND_OUT_DELTA);
}

CTask::FSM_Return CTaskThrowProjectile::Start_OnUpdate()
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	if (NetworkInterface::IsGameInProgress() && pPed->IsNetworkClone())
	{
		const CTaskCover* pCoverTask = static_cast<const CTaskCover *>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COVER));
		if( (pCoverTask && !pCoverTask->IsWeaponClipSetLoaded()) || (pPed->GetWeaponManager()->GetEquippedWeaponObject()==NULL) )
		{
			return FSM_Continue;
		}

		if(!CTaskCover::RequestCoverClipSetReturnIfLoaded(m_clipSetId))
		{
			return FSM_Continue;
		}

		const CWeaponInfo* pWeaponInfo = pPed->GetWeaponManager()->GetEquippedWeaponInfo();
		if(!pWeaponInfo || !pWeaponInfo->GetIsThrownWeapon())
		{
			CNetObjPed* pNetObjPed = static_cast<CNetObjPed*>(pPed->GetNetworkObject());
			if (pNetObjPed)
			{
				//Change flag so that we can equip the weapon here
				pNetObjPed->SetTaskOverridingPropsWeapons(true);

				//Ensure the weapon is in the remote peds inventory
				if (!pPed->GetInventory()->GetWeapon(m_WeaponNameHash))
					pPed->GetInventory()->AddWeapon(m_WeaponNameHash);
	
				//Equip the new weapon
				pPed->GetWeaponManager()->EquipWeapon(m_WeaponNameHash,-1,true,false,m_bRightHand ? CPedEquippedWeapon::AP_RightHand : CPedEquippedWeapon::AP_LeftHand);

				//Change the flag back so we can equip weapons going forward through the basic replication means
				pNetObjPed->SetTaskOverridingPropsWeapons(false);
			}

			return FSM_Continue;
		}

		u32 desiredWeapon = pPed->GetWeaponManager()->GetEquippedWeaponHash();
		u32 currentWeapon = pPed->GetWeaponManager()->GetEquippedWeapon() ? GetPed()->GetWeaponManager()->GetEquippedWeapon()->GetWeaponInfo()->GetHash() : 0;

		// wait if the player is holding the wrong weapon object, that does not correspond to the hash of the weapon he should be holding
		if (desiredWeapon != currentWeapon)
		{
			return FSM_Continue;
		}
	}

	fwMvNetworkDefId networkDefId(CClipNetworkMoveInfo::ms_NetworkTaskAimAndThrowProjectileCover);
	m_MoveNetworkHelper.CreateNetworkPlayer(pPed, networkDefId);
	static dev_float sf_BlendInDuration = FAST_BLEND_DURATION;
	pPed->GetMovePed().SetTaskNetwork(m_MoveNetworkHelper, sf_BlendInDuration, CMovePed::Task_TagSyncContinuous);	
	m_MoveNetworkHelper.SetClipSet(m_clipSetId);		
	taskAssert(m_IntroClipId != CLIP_ID_INVALID);
	const crClip* pIntroClip = fwClipSetManager::GetClip(m_clipSetId, m_IntroClipId);
	if (taskVerifyf(pIntroClip, "NULL Clip"))
	{
		m_MoveNetworkHelper.SetClip(pIntroClip, ms_MvIntroClip);
	}	

	//See if grip anim exists TODO
// 	static fwMvFlagId sUseGripClip("UseGripClip",0x72838804);
// fwClipSet *pClipSet = m_MoveNetworkHelper.GetClipSet();
// 	if (pClipSet && pClipSet->GetClip(fwMvClipId("grip",0x6FAD1258)) != NULL)
// 		m_MoveNetworkHelper.SetFlag(true, sUseGripClip);			
// 	else
// 		m_MoveNetworkHelper.SetFlag(false, sUseGripClip);					

	if (GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_IN_COVER)
	{
		SetState(State_Intro);
		return FSM_Continue;
	}
	
	weaponAssertf(0, "Should only arrive here via TASK_IN_COVER!");
	return FSM_Quit;
}

CTask::FSM_Return CTaskThrowProjectile::Intro_OnEnter()
{
	return FSM_Continue;
}

CTask::FSM_Return CTaskThrowProjectile::Intro_OnUpdate()
{
	CPed* pPed = GetPed();

	// We will need our update every frame for the throw
	pPed->GetPedAiLod().SetBlockedLodFlag(CPedAILod::AL_LodTimesliceIntelligenceUpdate|CPedAILod::AL_LodTimesliceAnimUpdate);
	if(m_MoveNetworkHelper.GetBoolean(ms_IntroCompleteEvent))
	{
		SetState(State_HoldingProjectile);
		return FSM_Continue;
	}	

	if (CThrowProjectileHelper::IsPlayerFiring(pPed))
	{
		CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
		if(pWeapon)
		{
			// check to see if this is placeable
			const CWeaponInfo* pWeaponInfo = pWeapon->GetWeaponInfo();
			if(pWeaponInfo)
			{
				const CAmmoInfo* pAmmoInfo = pWeaponInfo->GetAmmoInfo();
				if(pAmmoInfo && pAmmoInfo->GetIsClassId(CAmmoProjectileInfo::GetStaticClassId()))
				{
					if (static_cast<const CAmmoProjectileInfo*>(pAmmoInfo)->GetNoPullPin())
					{
						SetState(State_HoldingProjectile);
						return FSM_Continue;
					}
				}
			}
		}
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskThrowProjectile::HoldingProjectile_OnEnter()
{
	taskAssert(m_BaseClipId != CLIP_ID_INVALID);
	const crClip* pBaseClip = fwClipSetManager::GetClip(m_clipSetId, m_BaseClipId);
	m_MoveNetworkHelper.SendRequest(ms_StartPrime);
	if (taskVerifyf(pBaseClip, "NULL Clip"))
	{
		m_MoveNetworkHelper.SetClip(pBaseClip, ms_MvPrimeClip);
	}	

	m_MoveNetworkHelper.WaitForTargetState(ms_EnterPrimeEvent);	
	return FSM_Continue;
}

CTask::FSM_Return CTaskThrowProjectile::HoldingProjectile_OnUpdate()
{
	CPed* pPed = GetPed();

	// We will need our update every frame for the throw
	pPed->GetPedAiLod().SetBlockedLodFlag(CPedAILod::AL_LodTimesliceIntelligenceUpdate|CPedAILod::AL_LodTimesliceAnimUpdate);
	if (m_bTurning)
	{
		const crClip* pBaseClip = fwClipSetManager::GetClip(m_clipSetId, m_BaseClipId);
		if (taskVerifyf(pBaseClip, "NULL Clip"))
		{
			m_MoveNetworkHelper.SetClip(pBaseClip, ms_MvPrimeClip);
		}	

		float fClipPhase = m_MoveNetworkHelper.GetFloat(ms_TurnPhase);
		if (fClipPhase>=1.0f || fClipPhase<0)
			m_bTurning = false;
		if (!m_bTurnSwappedAnims && fClipPhase >=0.6f)
		{
			fwMvClipSetId clipSetId = CLIP_SET_ID_INVALID;
			fwMvClipId introClipId	= CLIP_ID_INVALID;
			fwMvClipId baseClipId	= CLIP_ID_INVALID;
			fwMvClipId throwLongClipId	= CLIP_ID_INVALID;
			fwMvClipId throwShortClipId	= CLIP_ID_INVALID;
			fwMvClipId throwLongFaceCoverClipId	= CLIP_ID_INVALID;
			fwMvClipId throwShortFaceCoverClipId	= CLIP_ID_INVALID;

			CTask* pTask = pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_IN_COVER);			
			Assert(pTask);			
			if (CTaskInCover::GetThrownProjectileClipSetAndClipIds(pPed, static_cast<CTaskInCover*>(pTask), clipSetId, introClipId, baseClipId, throwLongClipId, throwShortClipId, throwLongFaceCoverClipId, throwShortFaceCoverClipId, static_cast<CTaskInCover*>(pTask)->IsCoverFlagSet(CTaskCover::CF_TooHighCoverPoint)))
			{					
				SetClipSetId(clipSetId);
				SetClipIds(introClipId, baseClipId, throwLongClipId, throwShortClipId, throwLongFaceCoverClipId, throwShortFaceCoverClipId);
			}
			m_bTurnSwappedAnims = true;
		}
		if (!pPed->IsNetworkClone() && fClipPhase>=0)
		{		
			bool bFacingLeft = GetPed()->GetPedResetFlag(CPED_RESET_FLAG_InCoverFacingLeft);
			Vector3 vCoverDirection = VEC3V_TO_VECTOR3(pPed->GetCoverPoint()->GetCoverDirectionVector());
			vCoverDirection.RotateZ(bFacingLeft ? HALF_PI : -HALF_PI);
			float fGoalHeading = fwAngle::LimitRadianAngle(rage::Atan2f(-vCoverDirection.x, vCoverDirection.y)) + (bFacingLeft ? 0.01f : -0.01f); //extra fudge so SubtractAngleShorter chooses a good direction
			vCoverDirection.RotateZ(PI);
			float fInitHeading = fwAngle::LimitRadianAngle(rage::Atan2f(-vCoverDirection.x, vCoverDirection.y));
			fGoalHeading = fInitHeading + (SubtractAngleShorter(fGoalHeading, fInitHeading) * Min(fClipPhase * 1.5f, 1.0f));
			float fCorrection = GetPed()->GetCurrentHeading();
			fCorrection = SubtractAngleShorter(fGoalHeading, fCorrection);			
			pPed->SetPedResetFlag(CPED_RESET_FLAG_SyncDesiredHeadingToCurrentHeading, false);
			pPed->GetMotionData()->SetExtraHeadingChangeThisFrame(fCorrection);
		}

		return FSM_Continue;
	}

	if(!m_MoveNetworkHelper.IsInTargetState())
	{		
		return FSM_Continue;
	} 

	if(pPed->IsNetworkClone())
	{
		if(GetStateFromNetwork() == State_Finish)
			SetState(State_Finish);
		else
		{
			ProjectileInfo* pProjectileInfo = CNetworkPendingProjectiles::GetProjectile(pPed, GetNetSequenceID(), false);
			if(pProjectileInfo)
			{
				SetState(State_ThrowProjectile);
				return FSM_Continue;
			}
			else if (CanTriggerTurn(pPed))
			{
				const bool bFacingLeft = pPed->GetPedResetFlag(CPED_RESET_FLAG_InCoverFacingLeft);
				TriggerTurn(pPed, bFacingLeft);
			}
		}
	}
	else
	{
		if (CanTriggerTurn(pPed))
		{
			//Check for turn
			bool bFacingLeft = GetPed()->GetPedResetFlag(CPED_RESET_FLAG_InCoverFacingLeft);
			// Compute aim heading
			Vector3 vAimHeading;
			// Player ped
			if( pPed->IsLocalPlayer() )
			{
				const camFrame& aimFrame = camInterface::GetPlayerControlCamAimFrame();
				vAimHeading  = aimFrame.GetFront();
			}
			else // AI ped
			{
				Vector3 vTargetPosition;
				m_target.GetPosition(vTargetPosition);
				vAimHeading = vTargetPosition - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			}
			vAimHeading.z=0; vAimHeading.Normalize();
			Vector3 vNormal = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetForward());
			vNormal.z=0; vNormal.Normalize();
			float fTheta = vAimHeading.FastAngle(vNormal);
			Vector3 vRight = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetRight()); vRight.z=0; vRight.Normalize();
			float fDot = vAimHeading.Dot(vRight);
			if ((!bFacingLeft && fDot > 0) || (bFacingLeft && fDot < 0))
				fTheta = -fTheta;
			//static dev_float s_fMinTossAngle = -105.0f*DtoR;
			//static dev_float s_fMaxTossAngle = 135.0f*DtoR;	
			TUNE_GROUP_FLOAT(PROJECTILE_BUG, MinTossAngle, -105.0f, -180.0f, 180.0f, 1.0f);
			TUNE_GROUP_FLOAT(PROJECTILE_BUG, MaxTossAngle, 135.0f, -180.0f, 180.0f, 1.0f);
			const float s_fMinTossAngle = MinTossAngle * DtoR;
			const float s_fMaxTossAngle = MaxTossAngle * DtoR;

			if (fTheta < s_fMinTossAngle || fTheta > s_fMaxTossAngle)	
			{
				TriggerTurn(pPed, bFacingLeft);
				return FSM_Continue;
			}
		}

		if( !pPed->IsLocalPlayer() )
		{
			SetState(State_ThrowProjectile);
			return FSM_Continue;
		}

		if(CThrowProjectileHelper::IsPlayerFiring(pPed))
		{			
			SetState(State_ThrowProjectile);	
			return FSM_Continue;
		} 

		weaponAssert(pPed->GetWeaponManager());
		CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
		if(pWeapon)
		{
			if(!m_bPriming && CThrowProjectileHelper::IsPlayerPriming(pPed))
			{
				m_bPriming = true;
				CProjectile* pProjectile = static_cast<CProjectile*>(pPed->GetWeaponManager()->GetEquippedWeaponObject());
				if (pProjectile)
					pProjectile->SetPrimed(true);
				u32 nCurrentTime = fwTimer::GetTimeInMilliseconds();
				if(!pWeapon->GetIsCooking() && pWeapon->GetWeaponInfo()->GetCookWhileAiming())
					pWeapon->StartCookTimer(nCurrentTime);	
			}

			if(pWeapon->GetIsCooking() && pWeapon->GetWeaponInfo()->GetProjectileFuseTime() > 0.0f && pWeapon->GetCookTimeLeft(pPed, fwTimer::GetTimeInMilliseconds()) <= 0)
			{
				DropProjectile(pPed, pWeapon);
				SetState(State_Finish);
				return FSM_Continue;
			}
		}
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskThrowProjectile::ThrowProjectile_OnEnter()
{		 
	m_MoveNetworkHelper.SendRequest(ms_StartThrow);	
	taskAssert(m_ThrowLongClipId != CLIP_ID_INVALID);
	const crClip* pThrowClip = fwClipSetManager::GetClip(m_clipSetId, m_ThrowLongClipId);	
	if (taskVerifyf(pThrowClip, "NULL Clip"))
	{
		m_MoveNetworkHelper.SetClip(pThrowClip, ms_MvThrowClip);
	}	
	pThrowClip = fwClipSetManager::GetClip(m_clipSetId, m_ThrowShortClipId);
	if (taskVerifyf(pThrowClip, "NULL short throw Clip"))
	{
		m_MoveNetworkHelper.SetClip(pThrowClip, ms_MvThrowShortClip);
	}
	
	const CPed* pPed = GetPed();
#if FPS_MODE_SUPPORTED
	if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && pPed->GetIsInCover())
	{
		m_MoveNetworkHelper.SetFlag(true, CTaskMotionInCover::ms_IsFirstPersonFlagId);
		taskAssert(m_ThrowLongFaceCoverClipId != CLIP_ID_INVALID);
		const crClip* pFaceCoverThrowClip = fwClipSetManager::GetClip(m_clipSetId, m_ThrowLongFaceCoverClipId);	
		m_MoveNetworkHelper.SetClip(pFaceCoverThrowClip, ms_MvThrowFaceCoverClip);

		taskAssert(m_ThrowShortFaceCoverClipId != CLIP_ID_INVALID);
		pFaceCoverThrowClip = fwClipSetManager::GetClip(m_clipSetId, m_ThrowShortFaceCoverClipId);	
		m_MoveNetworkHelper.SetClip(pFaceCoverThrowClip, ms_MvThrowShortFaceCoverClip);
	}
#endif // FPS_MODE_SUPPORTED

#if __DEV
	if (CClipEventTags::CountEvent<crPropertyAttributeInt>(pThrowClip, CClipEventTags::Fire, CClipEventTags::Index, 1)==0)
	{
		// no fire event - BAD
		Assertf(0, "Animation has no AEF_FIRE_1 event");
	}
#endif // __DEV
	if(pThrowClip)
		CClipEventTags::FindEventPhase(pThrowClip, CClipEventTags::Fire, m_fThrowPhase);	 

	//compute throw direction	
	bool bFacingLeft = GetPed()->GetPedResetFlag(CPED_RESET_FLAG_InCoverFacingLeft);


	// Compute aim heading
	Vector3 vAimHeading;

	// Player ped
	if( pPed->IsLocalPlayer() )
	{
		const camFrame& aimFrame = camInterface::GetPlayerControlCamAimFrame();	
		vAimHeading  = aimFrame.GetFront();
	}
	else // AI ped
	{
		Vector3 vTargetPosition;
		if( !m_target.GetPosition(vTargetPosition) )
		{
			return FSM_Quit;
		}
		vAimHeading = vTargetPosition - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	}
	vAimHeading.z=0; vAimHeading.Normalize();
	Vector3 vNormal = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetForward());
	vNormal.z=0; vNormal.Normalize();
	float fTheta = vAimHeading.FastAngle(vNormal);
	Vector3 vRight = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetRight()); vRight.z=0; vRight.Normalize();
	float fDot = vAimHeading.Dot(vRight);
	if ((!bFacingLeft && fDot > 0) || (bFacingLeft && fDot < 0))
		fTheta = -fTheta;
	static dev_float s_fMinTossAngle = -45.0f*DtoR;
	static dev_float s_fMaxTossAngle = 135.0f*DtoR;	
	float fDirection = Clamp((fTheta - s_fMinTossAngle)/(s_fMaxTossAngle-s_fMinTossAngle), 0.0f, 1.0f);
	//Displayf("Angle = %f, direction = %f", fTheta*RtoD, fDirection);
	m_MoveNetworkHelper.SetFloat(ms_ThrowDirection, fDirection);

	return FSM_Continue;
}

CTask::FSM_Return CTaskThrowProjectile::ThrowProjectile_OnUpdate()
{
	CPed* pPed = GetPed();

#if FPS_MODE_SUPPORTED
	if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && pPed->GetIsInCover())
	{
		const float fCamHeading = CTaskCover::ComputeMvCamHeadingForPed(*pPed);
		m_MoveNetworkHelper.SetFloat(CTaskMotionInCover::ms_CamHeadingId, pPed->GetPedResetFlag(CPED_RESET_FLAG_InCoverFacingLeft) ? fCamHeading : 1.0f - fCamHeading);
	}
#endif // FPS_MODE_SUPPORTED

	// We will need our update every frame for the throw
	pPed->GetPedAiLod().SetBlockedLodFlag(CPedAILod::AL_LodTimesliceIntelligenceUpdate|CPedAILod::AL_LodTimesliceAnimUpdate);

	// if crossed fire time then fire weapon
	float fPhase = m_MoveNetworkHelper.GetFloat(ms_ThrowPhase);
	if(!m_bHasThrownProjectile && fPhase >= m_fThrowPhase)
	{			
		if(pPed->IsNetworkClone())
		{
			CProjectileManager::FireOrPlacePendingProjectile(pPed, GetNetSequenceID());
		}
		else
		{
			m_bFire = true;
		}

		m_bHasThrownProjectile = true;
	}

	if( m_bRotateToFaceTarget && pPed->IsLocalPlayer() )
	{
		pPed->SetPedResetFlag( CPED_RESET_FLAG_SyncDesiredHeadingToCurrentHeading, false );
		
		float fCamOrient = camInterface::GetPlayerControlCamHeading(*pPed);
		pPed->SetDesiredHeading( fCamOrient );
	}

	// Animation finished....
	if(m_MoveNetworkHelper.GetBoolean(ms_ThrowCompleteEvent))
	{

		int nAmmo = pPed->GetInventory()->GetWeaponAmmo(pPed->GetWeaponManager()->GetEquippedWeaponInfo());		
		if(nAmmo == 0)
		{
			// Out of ammo - swap weapon		
			CThrowProjectileHelper::SetWeaponSlotAfterFiring(pPed);			
			SetState(State_Finish);
			return FSM_Continue;
		}

		if(pPed->GetWeaponManager()->GetRequiresWeaponSwitch()) 
		{				
			if(!pPed->IsNetworkClone())
			{				
				pPed->GetWeaponManager()->CreateEquippedWeaponObject();				
			}				
		} 

		SetState(State_Finish);
		return FSM_Continue;
	}

	return FSM_Continue;
}

bool CTaskThrowProjectile::CanTriggerTurn(CPed* pPed)
{
	if (pPed->IsNetworkClone())
	{
		TUNE_GROUP_FLOAT(COVER_PROJECTILE_BUG, MIN_DELTA_TO_TRIGGER_TURN_CLONE, 0.25f, 0.0f, 3.0f, 0.01f);
		const float fCurrentHeading = pPed->GetCurrentHeading();
		const float fDesiredHeading = pPed->GetDesiredHeading();
		const float fDelta = fwAngle::LimitRadianAngle(fCurrentHeading - fDesiredHeading);
		if (Abs(fDelta) < MIN_DELTA_TO_TRIGGER_TURN_CLONE)
		{
			return false;
		}
	}

	static dev_s32 s_iTurnDelay = 1000;
	return (m_iTurnTimer + s_iTurnDelay) < fwTimer::GetTimeInMilliseconds();
}

void CTaskThrowProjectile::TriggerTurn(CPed* pPed, bool bFacingLeft)
{
	CTask* pTask = pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COVER);
	if (!taskVerifyf(pTask, "Frame : %i, %s Ped %s couldn't find active cover task", fwTimer::GetFrameCount(), pPed->IsNetworkClone() ? "CLONE" : "LOCAL", pPed->GetDebugName()))
		return;

	if (bFacingLeft)
	{
		m_MoveNetworkHelper.SendRequest(ms_TurnRight);
		static_cast<CTaskCover*>(pTask)->ClearCoverFlag(CTaskCover::CF_FacingLeft);
	}
	else
	{
		m_MoveNetworkHelper.SendRequest(ms_TurnLeft);
		static_cast<CTaskCover*>(pTask)->SetCoverFlag(CTaskCover::CF_FacingLeft);
	}

	//temporary until a real asset is available(?)
	fwMvClipSetId clipSetId = CLIP_SET_ID_INVALID;
	fwMvClipId introClipId	= CLIP_ID_INVALID;
	fwMvClipId baseClipId	= CLIP_ID_INVALID;
	fwMvClipId throwLongClipId	= CLIP_ID_INVALID;
	fwMvClipId throwShortClipId	= CLIP_ID_INVALID;
	fwMvClipId throwLongFaceCoverClipId	= CLIP_ID_INVALID;
	fwMvClipId throwShortFaceCoverClipId	= CLIP_ID_INVALID;

	pTask = pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_IN_COVER);			
	if (!taskVerifyf(pTask, "Frame : %i, %s Ped %s couldn't find active in cover task", fwTimer::GetFrameCount(), pPed->IsNetworkClone() ? "CLONE" : "LOCAL", pPed->GetDebugName()))
		return;

	if (CTaskInCover::GetThrownProjectileClipSetAndClipIds(pPed, static_cast<CTaskInCover*>(pTask), clipSetId, introClipId, baseClipId, throwLongClipId, throwShortClipId, throwLongFaceCoverClipId, throwShortFaceCoverClipId, static_cast<CTaskInCover*>(pTask)->IsCoverFlagSet(CTaskCover::CF_TooHighCoverPoint)))
	{		
		if (bFacingLeft)
		{
			m_pRightTurnClip = fwClipSetManager::GetClip(clipSetId, baseClipId);
			m_MoveNetworkHelper.SetClip(m_pRightTurnClip, ms_MvTurnRightClip);
		}
		else
		{
			m_pLeftTurnClip = fwClipSetManager::GetClip(clipSetId, baseClipId);
			m_MoveNetworkHelper.SetClip(m_pLeftTurnClip, ms_MvTurnLeftClip);
		}					
	}

	m_iTurnTimer = fwTimer::GetTimeInMilliseconds();	
	m_MoveNetworkHelper.WaitForTargetState(ms_EnterPrimeEvent);	
	m_bTurning = true;
	m_bTurnSwappedAnims = false;
}

void CTaskThrowProjectile::DropProjectile(CPed* pPed , CWeapon* pWeapon)
{
	CObject* pProjOverride = pPed->GetWeaponManager()->GetEquippedWeaponObject();

	Matrix34 weaponMat;
	if (pProjOverride && pWeapon)
	{
		// Calculate the launch position
		weaponMat = MAT34V_TO_MATRIX34(pProjOverride->GetMatrix());

		Vector3 vStart = weaponMat.d;
		Vector3 vEnd(Vector3::ZeroType);

		Vector3 trajectory;
		static dev_float BACKWARDS_FIRE_VELOCITY = -8.0f;
		// Always throw it backwards if it will generate fire on impact.
		if( pWeapon->GetWeaponInfo()->GetDamageType() == DAMAGE_TYPE_FIRE )
		{
			trajectory = VEC3V_TO_VECTOR3(pPed->GetTransform().GetB()) * BACKWARDS_FIRE_VELOCITY;
		}
		else
		{
			trajectory = -ZAXIS;
		}
		vEnd = weaponMat.d + trajectory;
		vStart = weaponMat.d;

		float fOverrideLifeTime = -1.0f;	
		if(!pWeapon->GetIsCooking())
			pWeapon->StartCookTimer(fwTimer::GetTimeInMilliseconds());	
		if (pWeapon->GetWeaponInfo()->GetProjectileFuseTime() > 0.0f)
			fOverrideLifeTime = Max(0.0f, pWeapon->GetCookTimeLeft(pPed, fwTimer::GetTimeInMilliseconds()));

		// Throw the projectile
		CWeapon::sFireParams params(pPed, weaponMat, &vStart, &vEnd);
		params.fOverrideLifeTime = fOverrideLifeTime;
		params.fDesiredTargetDistance = vStart.Dist( vEnd );
		if(pWeapon->Fire(params))
		{
			// Ensure object is removed
			pPed->GetWeaponManager()->DestroyEquippedWeaponObject();
		}
	}
}

bool CTaskThrowProjectile::OffsetProjectileIfNecessaryInCover(const CPed *pPed, const Vector3 vTestStartCam, const Vector3 vTestEndCam, const Vector3 vTestStartThrow, const Vector3 vTestEndThrow, const Matrix34 weaponMat, float &fOffsetZ)
{
	// B*1983883: If we're throwing from cover and aim vector from camera isn't colliding with the cover, but the projectile is going to,
	// then offset it slightly by the height difference (within reason). Called from CTaskThrowProjectile::ProcessPostPreRender if we're about to release the projectile.
	if (pPed->GetIsInCover())
	{
		CCoverPoint *pCoverPoint = pPed->GetCoverPoint();
		// Only do this if cover point is of "low" height; ie we can throw over it.
		if (pCoverPoint && pCoverPoint->GetHeight() == CCoverPoint::COVHEIGHT_LOW)
		{
			CEntity *pCoverEntity = pPed->GetPlayerInfo() ? pPed->GetPlayerInfo()->ms_DynamicCoverHelper.GetCoverEntryEntity() : NULL;
			if (pCoverEntity && pCoverEntity->GetBaseModelInfo())
			{
				// Probe test along camera aim vector.
				bool bHitCoverFromCamera = false;

				WorldProbe::CShapeTestHitPoint probeIsectCam;
				WorldProbe::CShapeTestResults probeResultCam(probeIsectCam);
				WorldProbe::CShapeTestProbeDesc probeDescCam;
				probeDescCam.SetStartAndEnd(vTestStartCam, vTestEndCam);
				probeDescCam.SetResultsStructure(&probeResultCam);
				probeDescCam.SetExcludeEntity(pPed);
				probeDescCam.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_OBJECT_TYPE);
				probeDescCam.SetContext(WorldProbe::LOS_GeneralAI);

				if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDescCam, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
				{
					for (int i = 0; i < probeResultCam.GetNumHits(); i++)
					{
						if (pCoverEntity == probeResultCam[i].GetHitEntity())
						{
							// We've hit the cover entity from the camera vector.
							bHitCoverFromCamera = true;
						}
					}
				}

				if (!bHitCoverFromCamera)
				{
					// We haven't hit cover from the camera vector, so do a probe test from projectile position to end position.
					bool bHitCoverFromThrow = false;

					WorldProbe::CShapeTestHitPoint probeIsectThrow;
					WorldProbe::CShapeTestResults probeResultThrow(probeIsectThrow);
					WorldProbe::CShapeTestProbeDesc probeDescThrow;
					probeDescThrow.SetStartAndEnd(vTestStartThrow, vTestEndThrow);
					probeDescThrow.SetResultsStructure(&probeResultThrow);
					probeDescThrow.SetExcludeEntity(pPed);
					probeDescThrow.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_OBJECT_TYPE);
					probeDescThrow.SetContext(WorldProbe::LOS_GeneralAI);

					if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDescThrow, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
					{
						for (int i = 0; i < probeResultThrow.GetNumHits(); i++)
						{
							if (pCoverEntity == probeResultThrow[i].GetHitEntity())
							{
								// We've hit the cover entity from the throw vector.
								bHitCoverFromThrow = true;
							}
						}
					}

					// We didn't hit the cover from the aim vector, but we did from the throw vector,
					// so try and offset the projectile slightly based on the cover height.
					if (bHitCoverFromThrow)
					{
						// Calculate height difference between cover and projectile.
						const float fLocalCoverHeight = pCoverEntity->GetBoundingBoxMax().z - pCoverEntity->GetBoundingBoxMin().z;
						float fCoverHeight = pCoverEntity->GetTransform().GetPosition().GetZf() + fLocalCoverHeight;
						float fProjectileHeight = weaponMat.d.z;

						float fHeightOffset = Abs(fCoverHeight - fProjectileHeight);

						// Clamp the change so we don't get a crazy pop.
						static dev_float fMaxHeightOffset = 0.2f;
						fHeightOffset = Clamp(fHeightOffset, 0.0f, fMaxHeightOffset);

						// Set the weapon matrix Z offset variable.
						fOffsetZ = fHeightOffset;
						return true;
					}
#if __BANK
					static dev_bool bEnableOffsetDebugRender = false;
					if (bEnableOffsetDebugRender)
					{
						CTask::ms_debugDraw.AddLine(RCC_VEC3V(vTestStartCam), RCC_VEC3V(vTestEndCam), Color_yellow, 0);
						CTask::ms_debugDraw.AddLine(RCC_VEC3V(vTestStartThrow), RCC_VEC3V(vTestEndThrow), Color_blue, 0);
						CTask::ms_debugDraw.AddSphere( RCC_VEC3V(weaponMat.d), 0.025f, Color_CornflowerBlue);
					}
#endif	// __BANK
				}
			}
		}
	}
	return false;
}

float CTaskAimAndThrowProjectile::SelectFPSBlendTime(CPed *pPed, bool bStateToTransition, CPedMotionData::eFPSState prevFPSStateOverride, CPedMotionData::eFPSState currFPSStateOverride)
{
	float fBlendTime = 0.2f;

	//Idle_To_RNG
	TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE_PROJECTILE, fIdle_To_IdleToRNG_BlendTime, 0.09f, 0.0f, 2.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE_PROJECTILE, fIdleToRNG_To_RNG_BlendTime, 0.15f, 0.0f, 2.0f, 0.01f);

	//Idle_To_LT
	TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE_PROJECTILE, fIdle_To_IdleToLT_BlendTime, 0.15f, 0.0f, 2.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE_PROJECTILE, fIdleToLT_To_LT_BlendTime, 0.15f, 0.0f, 2.0f, 0.01f);

	//LT_To_Idle
	TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE_PROJECTILE, fLT_To_LTToIdle_BlendTime, 0.15f, 0.0f, 2.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE_PROJECTILE, fLTToIdle_To_Idle_BlendTime, 0.25f, 0.0f, 2.0f, 0.01f);

	//LT_To_RNG
	TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE_PROJECTILE, fLT_To_LTToRNG_BlendTime, 0.09f, 0.0f, 2.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE_PROJECTILE, fLTToRNG_To_RNG_BlendTime, 0.15f, 0.0f, 2.0f, 0.01f);

	//Unholster_To_Idle
	TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE_PROJECTILE, fUnholster_To_UnholsterToIdle_BlendTime, 0.15f, 0.0f, 2.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE_PROJECTILE, fUnholsterToIdle_To_Idle_BlendTime, 0.3f, 0.0f, 2.0f, 0.01f);

	//Unholster_To_LT
	TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE_PROJECTILE, fUnholster_To_UnholsterToLT_BlendTime, 0.15f, 0.0f, 2.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE_PROJECTILE, fUnholsterToLT_To_LT_BlendTime, 0.3f, 0.0f, 2.0f, 0.01f);

	int iCurrState = (prevFPSStateOverride != CPedMotionData::FPS_MAX) ? currFPSStateOverride : pPed->GetMotionData()->GetFPSState();
	int iPrevState = (currFPSStateOverride != CPedMotionData::FPS_MAX) ? prevFPSStateOverride : pPed->GetMotionData()->GetPreviousFPSState();

	if(iPrevState == CPedMotionData::FPS_IDLE && iCurrState == CPedMotionData::FPS_RNG)
	{
		fBlendTime = (bStateToTransition) ? fIdle_To_IdleToRNG_BlendTime : fIdleToRNG_To_RNG_BlendTime;
	}
	else if(iPrevState == CPedMotionData::FPS_IDLE && iCurrState == CPedMotionData::FPS_LT)
	{
		fBlendTime = (bStateToTransition) ? fIdle_To_IdleToLT_BlendTime : fIdleToLT_To_LT_BlendTime;
	}
	else if(iPrevState == CPedMotionData::FPS_LT && iCurrState == CPedMotionData::FPS_IDLE)
	{
		fBlendTime = (bStateToTransition) ? fLT_To_LTToIdle_BlendTime : fLTToIdle_To_Idle_BlendTime;
	}
	else if(iPrevState == CPedMotionData::FPS_LT && iCurrState == CPedMotionData::FPS_RNG)
	{
		fBlendTime = (bStateToTransition) ? fLT_To_LTToRNG_BlendTime : fLTToRNG_To_RNG_BlendTime;
	}
	else if(iPrevState == CPedMotionData::FPS_UNHOLSTER && iCurrState == CPedMotionData::FPS_IDLE)
	{
		fBlendTime = (bStateToTransition) ? fUnholster_To_UnholsterToIdle_BlendTime : fUnholsterToIdle_To_Idle_BlendTime;
	}
	else if(iPrevState == CPedMotionData::FPS_UNHOLSTER && iCurrState == CPedMotionData::FPS_LT)
	{
		fBlendTime = (bStateToTransition) ? fUnholster_To_UnholsterToLT_BlendTime : fUnholsterToLT_To_LT_BlendTime;
	}

	return fBlendTime;
}
