//
// weapons/weapon.cpp
//
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

// File header
#include "Weapons/Weapon.h"

// Framework headers
#include "fwmaths/vectorutil.h"
#include "vfx/ptfx/ptfxreg.h"

// Rage includes
#include "phbound/boundcomposite.h"

// Game headers
#include "animation/FacialData.h"
#include "Animation/MoveObject.h"
#include "Camera/CamInterface.h"
#include "camera/cinematic/CinematicDirector.h"
#include "Camera/Gameplay/Aim/ThirdPersonAimCamera.h"
#include "Camera/Gameplay/GameplayDirector.h"
#include "Event/EventDamage.h"
#include "Event/EventShocking.h"
#include "Event/EventSound.h"
#include "Event/EventWeapon.h"
#include "Event/ShockingEvents.h"
#include "Frontend/MiniMap.h"
#include "Frontend/NewHud.h"
#include "game/Wanted.h"
#include "ModelInfo/PedModelInfo.h"
#include "Network/Sessions/NetworkSession.h"
#include "Network/Events/NetworkEventTypes.h"
#include "network/objects/entities/NetObjPlayer.h"
#include "Peds/Ped.h"
#include "Peds/PedCapsule.h"
#include "Peds/PedDebugVisualiser.h"
#include "Peds/PedIntelligence.h"
#include "Physics/Physics.h"
#include "Pickups/Pickup.h"
#include "Renderer/Water.h"
#include "security/ragesecgameinterface.h"
#include "Shaders/CustomShaderEffectWeapon.h"
#include "Stats/StatsInterface.h"
#include "Stats/StatsMgr.h"
#include "Stats/StatsUtils.h"
#include "System/ControlMgr.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/Motion/Locomotion/TaskHumanLocomotion.h"
#include "Task/Motion/Locomotion/TaskMotionPed.h"
#include "Task/System/TaskHelpers.h"
#include "Task/Vehicle/TaskVehicleWeapon.h"
#include "Task/Weapons/Gun/TaskReloadGun.h"
#include "Vehicles/Metadata/VehicleSeatInfo.h"
#include "Vehicles/VehicleGadgets.h"
#include "Vehicles/VehiclePopulation.h"
#include "Vfx/Systems/VfxWeapon.h"
#include "Vfx/VfxHelper.h"
#include "Weapons/Bullet.h"
#include "Weapons/Components/WeaponComponentGroup.h"
#include "Weapons/Components/WeaponComponentLaserSight.h"
#include "Weapons/Info/WeaponInfoManager.h"
#include "Weapons/Inventory/PedEquippedWeapon.h"
#include "Weapons/Inventory/PedWeaponManager.h"
#include "Weapons/Projectiles/ProjectileManager.h"
#include "Weapons/Projectiles/ProjectileRocket.h"
#include "Weapons/WeaponDamage.h"
#include "Weapons/WeaponDebug.h"
#include "scene/world/gameWorld.h"
#include "shaders/CustomShaderEffectWeapon.h"
#include "Cutscene/CutSceneManagerNew.h"
#include "vehicles/Submarine.h"
#include "Task/Vehicle/TaskInVehicle.h"
#include "camera/cinematic/camera/mounted/CinematicMountedCamera.h"
#include "Task/Weapons/Gun/TaskVehicleDriveBy.h"
#include "weapons/AirDefence.h"


#if __ASSERT
#include "Stats/StatsTypes.h"
#endif

#define DR_WEAPON DR_ENABLED && 0
#if DR_WEAPON
	#define DR_WEAPON_ONLY(x) x
#else
	#define DR_WEAPON_ONLY(x)
#endif

// Macro to disable optimisations if set
WEAPON_OPTIMISATIONS()
NETWORK_OPTIMISATIONS()

// Pool - the size of this will be set in the InitPool() call, since it
// depends on MAXNOOFPEDS which we can't know until we've read the configuration
// file.
FW_INSTANTIATE_CLASS_POOL_SPILLOVER(CWeapon, 0, 0.46f, atHashString("CWeapon",0x3a2bc128));

const f32 CWeapon::STANDARD_WEAPON_ACCURACY_RANGE = 120.f;
const float CWeapon::sf_OffscreenRandomPerceptionOverrideRange = 45.0f;

#if __BANK
bool CWeapon::bEnableHiDetail = false;
bool CWeapon::bForceLoDetail = false;
#endif	//__BANK

////////////////////////////////////////////////////////////////////////////////
const fwMvFlagId CWeapon::ms_WeaponIdleFlagId("HasIdleAnim",0x896CBFE4);
const fwMvFlagId CWeapon::ms_WeaponInVehicleIdleFlagId("HasInVehicleIdleAnim",0x4016fa8e);
const fwMvFlagId CWeapon::ms_DisableIdleFilterFlagId("DisableIdleFilter", 0x91307e5b);
const fwMvFlagId CWeapon::ms_HasGripAnimId("HasGripAnim",0x80dfdc00);
const fwMvClipId CWeapon::ms_WeaponFireClipId("w_fire",0xA3AE8610);
const fwMvClipId CWeapon::ms_GripClipId("Grip",0x6fad1258);
const fwMvFloatId CWeapon::ms_GripBlendDurationId("Grip_BlendDuration",0x8463ed3f);
const fwMvFloatId CWeapon::ms_WeaponIdleBlendDuration("WeaponIdle_BlendDuration",0xD05C89C7);
const fwMvFloatId CWeapon::ms_WeaponIdlePhase("WeaponIdle_Phase",0xd8d547b4);
const fwMvFloatId CWeapon::ms_WeaponIdleRate("WeaponIdle_Rate",0xe75ca3f6);
const fwMvRequestId CWeapon::ms_RestartGripId("RestartGrip",0x5e976d94);
const fwMvBooleanId CWeapon::ms_UseActionModeGripId("USE_ACTIONMODE_GRIP",0x71d783e6);
const fwMvRequestId CWeapon::ms_InterruptFireId("InterruptFire", 0xac50e0a3);
const fwMvBooleanId CWeapon::ms_WeaponFireEndedId("Fire_Ended",0x09813eaa);
dev_float CWeapon::ms_fBlindFireSpreadModifier = 2.0f;
dev_float CWeapon::ms_fBlindFireBatchSpreadModifier = 2.0f; // Applies to non-player only
dev_float CWeapon::ms_fMinBlindFireSpread = 4.0f;

////////////////////////////////////////////////////////////////////////////////

bool CWeapon::RefillPedsWeaponAmmoInstant(CPed& rPed)
{
	CPedWeaponManager* pWeaponMgr = rPed.GetWeaponManager();
	if(weaponVerifyf(pWeaponMgr, "RefillPedsWeaponAmmoInstant - ped %s(%p) requires a weapon manager", rPed.GetModelName(), &rPed))
	{
		//If the ped is reloading, abort
		CTaskReloadGun* task = (CTaskReloadGun *)rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_RELOAD_GUN);
		if(task)
		{
			task->RequestInterrupt();
		}

		//If the weapon has an projectile then create it so we don't attend to reload again.
		CPedEquippedWeapon* pPedEquippedWeapon = rPed.GetWeaponManager()->GetPedEquippedWeapon();
		if(pPedEquippedWeapon && pPedEquippedWeapon->GetWeaponInfo() && pPedEquippedWeapon->GetWeaponInfo()->GetCreateVisibleOrdnance())
		{
			pPedEquippedWeapon->CreateProjectileOrdnance();
		}

		//Do the weapon reload
		CWeapon* pWeapon = pWeaponMgr->GetEquippedWeapon();
		if(pWeapon)
		{
			pWeapon->DoReload();
			return true;
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

CWeapon::CWeapon(u32 uWeaponHash, s32 iAmmoTotal, CDynamicEntity* pDrawable, bool isScriptWeapon, u8 tintIndex)
: m_muzzleRot(V_IDENTITY)
, m_muzzlePos(V_ZERO)
, m_LastOffsetPosition(V_ZERO)
, m_pWeaponInfo(CWeaponInfoManager::GetInfo<CWeaponInfo>(uWeaponHash))
, m_uGlobalTime(0)
, m_uTimer(0)
, m_uNextShotAllowedTime(0)
, m_iAmmoTotal((s16)Min(iAmmoTotal, SHRT_MAX))
, m_iAmmoInClip(0)
, m_pDrawableEntity(pDrawable)
, m_pObserver(NULL)
, m_pClipComponent(NULL)
, m_pScopeComponent(NULL)
, m_pSuppressorComponent(NULL)
, m_pTargetingComponent(NULL)
, m_pFlashLightComponent(NULL)
, m_pLaserSightComponent(NULL)
, m_pVariantModelComponent(NULL)
, m_pMoveNetworkHelper(NULL)
, m_pFPSMoveNetworkHelper(NULL)
, m_uCookTime(0)
, m_pMuzzleEntity(NULL)
, m_iMuzzleBoneIndex(-1)
, m_muzzleSmokeLevel(0.0f)
, m_vMuzzleOffset(VEC3_ZERO)
, m_uLastStealthBlipTime(0)
, m_fBarrelSpin(0.0f)
, m_bNeedsToRotateBarrel(false)
, m_fBarrelHasRotated(0.0f)
, m_fIdleAnimPhase(0.0f)
, m_fPrevIdleAnimPhase(0.0f)
, m_FiringStartTime(0)
, m_uLastShotTimer(0)
, m_PropEndOfLifeTimeoutMS(PROP_END_OF_LIFE_TIMEOUT_IN_MS)
, m_pStandardDetailShaderEffect(NULL)
, m_GripClipSetId(CLIP_SET_ID_INVALID)
, m_GripClipId(CLIP_ID_INVALID)
, m_State(STATE_READY)
, m_weaponLodState(WLS_HD_NONE)
, m_uTintIndex(tintIndex)
, m_bTemporaryNetworkWeapon(false)
, m_bNeedsToSpinDown(false)
, m_bValidMuzzleMatrix(false)
, m_bCooking(false)
, m_pCookEntity(NULL)
, m_bCanActivateMoveNetwork(true)
, m_bExplicitHdRequest(false)
, m_bReloadClipNotEmpty(false)
, m_bSilenced(false)
, m_bNoSpinUp(false)
, m_bHasWeaponIdleAnim(false)
, m_bHasInVehicleWeaponIdleAnim(false)
, m_bOutOfAmmoAnimIsPlaying(false)
#if FPS_MODE_SUPPORTED
, m_bHasGripAttachmentComponent(false)
#endif
, m_bFpsSightDofFirstUpdate(true)
, m_bFpsSightDofPlayerWasFirstPersonLastUpdate(false)
, m_FpsSightDofValue(0.0f)
#if __BANK
, m_pDebugData(NULL)
#endif // __BANK
{
	// If the uWeaponHash was invalid, fix it up to use the default UNARMED
	if( !weaponVerifyf( m_pWeaponInfo, "Weapon hash [%d] does not exist", uWeaponHash ) )
	{
		uWeaponHash = WEAPONTYPE_UNARMED;
		m_pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>( uWeaponHash );
		weaponFatalAssertf( m_pWeaponInfo, "Weapon hash [%d] does not exist", uWeaponHash );
	}
	
	// Trigger a reload to properly initialise our clip
	DoReload(true);

	// Ensure the skeleton is created for animation
	//  fragments are not able to be animated ... 
	if(m_pDrawableEntity && !m_pDrawableEntity->GetFragInst() &&weaponVerifyf(m_pDrawableEntity->GetDrawable(), "Weapon entity '%s' has no drawable", m_pDrawableEntity->GetModelName()))
	{
		if(weaponVerifyf(m_pDrawableEntity->GetDrawable()->GetSkeletonData(), "Drawable entity '%s' does not have skeleton data", m_pDrawableEntity->GetModelName()))
		{
			if(!m_pDrawableEntity->GetSkeleton())
			{
				m_pDrawableEntity->CreateSkeleton();
			}

			if(weaponVerifyf(m_pDrawableEntity->GetSkeleton(), "Failed to create a skeleton"))
			{
				if(!m_pDrawableEntity->GetIsOnSceneUpdate())
				{
					m_pDrawableEntity->AddToSceneUpdate();
				}
			}
		}
	}

	// Create an anim director is we use expressions
	if(m_pDrawableEntity && m_pDrawableEntity->GetArchetype() && m_pDrawableEntity->GetArchetype()->GetExpressionDictionaryIndex().IsValid())
	{
		if(!m_pDrawableEntity->GetAnimDirector())
		{
			m_pDrawableEntity->CreateAnimDirector(*m_pDrawableEntity->GetDrawable());
		}
	}

	// Init the audio
	m_WeaponAudioComponent.Init(GetWeaponInfo()->GetAudioHash(), this, isScriptWeapon);

	// Init the shader tint
	weaponitemDebugf3("CWeapon::CWeapon-->invoke UpdateShaderVariables m_uTintIndex[%u]",m_uTintIndex);
	UpdateShaderVariables(m_uTintIndex);

	// Store silenced
	m_bSilenced = m_pWeaponInfo->GetIsSilenced();
}

////////////////////////////////////////////////////////////////////////////////

CWeapon::~CWeapon()
{
	if(m_weaponLodState == WLS_HD_AVAILABLE)
	{
		ShaderEffect_HD_DestroyInstance();
		m_weaponLodState = WLS_HD_REMOVING;
	}

	if(m_weaponLodState != WLS_HD_NONE && m_weaponLodState != WLS_HD_NA)
	{
		CBaseModelInfo* pModelInfo = m_pDrawableEntity ? m_pDrawableEntity->GetBaseModelInfo() : NULL;
		if (Verifyf(pModelInfo, "No model info to remove HD instance"))
		{
			static_cast<CWeaponModelInfo*>(pModelInfo)->RemoveFromHDInstanceList((size_t)this);
		}
	}

	if(m_pMoveNetworkHelper && m_pMoveNetworkHelper->IsNetworkAttached())
	{
		if(m_pDrawableEntity && m_pDrawableEntity->GetIsTypeObject() && m_pDrawableEntity->GetAnimDirector())
		{
			CObject* pDrawableObject = static_cast<CObject*>(m_pDrawableEntity.Get());
			pDrawableObject->GetMoveObject().ClearNetwork(*m_pMoveNetworkHelper, NORMAL_BLEND_DURATION);
		}
	}

	ptfxReg::UnRegister(this);

	for(s32 i = 0; i < m_Components.GetCount(); i++)
	{
		delete m_Components[i];
	}
	m_Components.Reset();

	if(m_pMoveNetworkHelper)
	{
		delete m_pMoveNetworkHelper;
		m_pMoveNetworkHelper = NULL;
	}

#if FPS_MODE_SUPPORTED
	if(m_pFPSMoveNetworkHelper)
	{
		delete m_pMoveNetworkHelper;
		m_pMoveNetworkHelper = NULL;
	}
#endif

#if __BANK
	if(m_pDebugData)
	{
		delete m_pDebugData;
		m_pDebugData = NULL;
	}
#endif // __BANK
}

////////////////////////////////////////////////////////////////////////////////

float CWeapon::sm_fDamageOverrideForAPPistol = -1.0f;
bool  CWeapon::sm_bDisableMarksmanRecoilFix = false;

void CWeapon::InitTunables()
{
	sm_fDamageOverrideForAPPistol = ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("LOWRIDER_DAMAGE_OVERRIDE_FOR_AP_PISTOL", 0xc7961fb9), sm_fDamageOverrideForAPPistol);
	sm_bDisableMarksmanRecoilFix =	::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("XMAS17_DISABLE_MARKSMAN_RECOIL_FIX", 0xd801264f), sm_bDisableMarksmanRecoilFix);
}

////////////////////////////////////////////////////////////////////////////////

bool CWeapon::Fire(const sFireParams& params)
{
	PF_AUTO_PUSH_TIMEBAR("CWeapon Fire");
	weaponDebugf3("CWeapon::Fire fc[%u] pFiringEntity[%p][%s] IsLocal[%d] start[%f %f %f] end[%f %f %f]",fwTimer::GetFrameCount(),params.pFiringEntity,params.pFiringEntity ? params.pFiringEntity->GetModelName() : "",params.pFiringEntity ? !NetworkUtils::IsNetworkClone(params.pFiringEntity) : true,params.pvStart ? params.pvStart->x : 0.f,params.pvStart ? params.pvStart->y : 0.f,params.pvStart ? params.pvStart->z : 0.f,params.pvEnd ? params.pvEnd->x : 0.f,params.pvEnd ? params.pvEnd->y : 0.f,params.pvEnd ? params.pvEnd->z : 0.f);

#if __BANK
	if (Channel_weapon.FileLevel == DIAG_SEVERITY_DEBUG3 || Channel_weapon.TtyLevel == DIAG_SEVERITY_DEBUG3)
	{
		sysStack::PrintStackTrace();
	}
#endif

	if(!m_pWeaponInfo)
	{
		// No valid weapon info
		return false;
	}

	// Network stuff
	if(NetworkInterface::IsGameInProgress() && params.pFiringEntity)
	{
		// ped firing....
        if(params.pFiringEntity->GetIsTypePed())
        {
            CPed           *pPed           = SafeCast(CPed, params.pFiringEntity);
            CNetGamePlayer *pNetGamePlayer = (pPed->GetNetworkObject() && pPed->GetNetworkObject()->GetPlayerOwner()) ? SafeCast(CNetGamePlayer, pPed->GetNetworkObject()->GetPlayerOwner()) : 0;			

            if(pPed->IsPlayer() && pNetGamePlayer)
            {
				CNetObjPlayer *netObjPlayer = pPed->GetNetworkObject() ? SafeCast(CNetObjPlayer, pPed->GetNetworkObject()) : 0;

				// Don't allow firing by hidden network players during MP tutorials or if they are concealed
				if(pNetGamePlayer->IsInDifferentTutorialSession() || (netObjPlayer && netObjPlayer->IsConcealed()))
				{
					return false;
				}

				// if the firing ped is currently invincible after respawning in a death match, turn invincibility off....
				if(!NetworkUtils::IsNetworkClone(params.pFiringEntity) /*&& NetworkInterface::IsInFreeMode()*/)
				{
					if((static_cast<CNetObjPlayer*>(pPed->GetNetworkObject()))->GetRespawnInvincibilityTimer() > 0)
					{
						(static_cast<CNetObjPlayer*>(pPed->GetNetworkObject()))->SetRespawnInvincibilityTimer(0);
					}
				}
			}
        }

		// Don't allow firing by hidden peds during a network cutscene 
		if(NetworkInterface::IsInMPCutscene() && !params.pFiringEntity->GetIsVisible())
		{
			return false;
		}

		if(GetWeaponInfo()->GetFireType() == FIRE_TYPE_PROJECTILE)
		{
			bool bProceedWithRemoteProcessing = false;

			if (params.pFiringEntity->GetIsTypeVehicle() && params.pFiringEntity->GetIsAttached())
			{
				CVehicle* pVehicle = static_cast<CVehicle*>(params.pFiringEntity);
				if (pVehicle)
				{
					CPed* pDriver = pVehicle->GetDriver();
					if (!pDriver || pDriver->IsNetworkClone())
					{
						bProceedWithRemoteProcessing = true;
					}
					
					//ELSE pDriver is local - allow firing to proceed as if the vehicle was local
				}
			}
			else if (NetworkUtils::IsNetworkClone(params.pFiringEntity))
			{
				bProceedWithRemoteProcessing = true;
			}

			if (bProceedWithRemoteProcessing)
			{
				weaponDebugf3("FIRE_TYPE_PROJECTILE bProceedWithRemoteProcessing");

				if(params.pFiringEntity->GetIsTypePed())
				{
					CPed* pPed = SafeCast(CPed, params.pFiringEntity);

					CProjectile* pProjectileObject = NULL;	
					if( pPed && !GetHasInfiniteClips( pPed ) )
					{
						CPedEquippedWeapon* pEquippedWeapon = pPed->GetWeaponManager()->GetPedEquippedWeapon();
						pProjectileObject = pEquippedWeapon ? pEquippedWeapon->GetProjectileOrdnance() : NULL;
						if( pProjectileObject )
						{
							pProjectileObject->SetOrdnance( false );
							pProjectileObject->DetachFromParent( DETACH_FLAG_ACTIVATE_PHYSICS | DETACH_FLAG_APPLY_VELOCITY );
							pProjectileObject->SetNoCollision( pPed, NO_COLLISION_RESET_WHEN_NO_IMPACTS );

							// Mark this drawable for delete on next process
							pProjectileObject->RemovePhysics();
							pProjectileObject->SetOwnedBy(ENTITY_OWNEDBY_TEMP);
							pProjectileObject->m_nEndOfLifeTimer = 1;
							pProjectileObject->DisableCollision();
							pProjectileObject->SetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY, false, true);
							pProjectileObject = NULL;
						}
					}
				}

				if(m_State == STATE_READY)
				{
					Vector3 vStart(params.pvStart ? *params.pvStart : params.weaponMatrix.d);
					Vector3 vEnd(params.pvEnd ? *params.pvEnd : (vStart + params.weaponMatrix.a * GetRange()));

#if DEBUG_DRAW
					TUNE_GROUP_BOOL(TURRET_TUNE, SYNCING_bDrawTurretPlayerLocalFireVector, false);
					if( SYNCING_bDrawTurretPlayerLocalFireVector )
					{
						static float fArrowHeadSize = 0.5f;
						grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(vStart),VECTOR3_TO_VEC3V(vEnd),fArrowHeadSize,Color_red, CTaskVehicleMountedWeapon::iTurretTuneDebugTimeToLive);
					}
#endif

					DoWeaponFire(params.pFiringEntity, params.weaponMatrix, vEnd, params.iVehicleWeaponIndex, params.iVehicleWeaponBoneIndex, 1, params.fFireAnimRate, true, false, params.bAllowRumble, params.bFiredFromAirDefence);
					return true;
				}

				return false;
			}
		}
	}


/*#if __BANK
	// Assert to catch any cases where a ped is firing but is in an air defence zone.
	if (params.pFiringEntity && params.pFiringEntity->GetIsTypePed() && !params.bFiredFromAirDefence)
	{
		CPed *pPed = SafeCast(CPed, params.pFiringEntity);
		if (pPed && !pPed->IsDead() && pPed->GetPedResetFlag(CPED_RESET_FLAG_InAirDefenceSphere))
		{
			u8 uIntersectingZoneIdx = 0;
			if (CAirDefenceManager::IsPositionInDefenceZone(pPed->GetTransform().GetPosition(), uIntersectingZoneIdx))
			{
				weaponAssertf(0, "CWeapon::Fire: ped is firing but is in an air defence zone!");
			}
		}
	}
#endif*/

	// Do we need to spin up
	SpinUp(params.pFiringEntity);

	// The start point is either passed in, or we use the weapon matrix
	Vector3 vStart(params.pvStart ? *params.pvStart : params.weaponMatrix.d);

	// The end point is either passed in, or we use the weapon matrix
	Vector3 vEnd(params.pvEnd ? *params.pvEnd : (vStart + params.weaponMatrix.a * GetRange()));

#if DEBUG_DRAW
	TUNE_GROUP_BOOL(TURRET_TUNE, SYNCING_bDrawVehicleOwnerFireVector, false);
	if( SYNCING_bDrawVehicleOwnerFireVector )
	{
		static float fArrowHeadSize = 0.5f;
		grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(vStart),VECTOR3_TO_VEC3V(vEnd),fArrowHeadSize,Color_red, CTaskVehicleMountedWeapon::iTurretTuneDebugTimeToLive);
	}
#endif

	// Find out which type of weapon this is and try to fire it
	switch(GetWeaponInfo()->GetFireType())
	{
	case FIRE_TYPE_INSTANT_HIT:
	case FIRE_TYPE_DELAYED_HIT:
		return FireDelayedHit(params, vStart, vEnd);

	case FIRE_TYPE_PROJECTILE:
		return FireProjectile(params, vStart, vEnd);

	case FIRE_TYPE_VOLUMETRIC_PARTICLE:
		return FireVolumetric(params.pFiringEntity, params.weaponMatrix, params.iVehicleWeaponIndex, params.iVehicleWeaponBoneIndex, vStart, vEnd, params.fFireAnimRate);

	case FIRE_TYPE_NONE:
	case FIRE_TYPE_MELEE:
		return false;

	default:
		weaponAssertf(false, "Unknown weapon fire type [%d]", GetWeaponInfo()->GetFireType());
		return false;
	}
}

////////////////////////////////////////////////////////////////////////////////

void CWeapon::Process(CEntity* pFiringEntity, u32 uTimeInMilliseconds)
{
	if(!m_pWeaponInfo)
	{
		// No valid weapon info
		return;
	}

	// Update state
	ProcessState(pFiringEntity, uTimeInMilliseconds);

	// Handle animation
	ProcessAnimation(pFiringEntity);

	// Update stats
	if(CStatsUtils::CanUpdateStats())
	{
		ProcessStats(pFiringEntity);
	}

	for(s32 i = 0; i < m_Components.GetCount(); i++)
	{
		m_Components[i]->Process(pFiringEntity);
	}
	
	// Update cook timer
	ProcessCookTimer(pFiringEntity, uTimeInMilliseconds);

	// Update muzzle smoke
	ProcessMuzzleSmoke();

#if __BANK
	if(CWeaponInfoManager::GetForceShaderUpdate())
	{
		weaponitemDebugf3("CWeapon::Process-->invoke UpdateShaderVariables");
		UpdateShaderVariables(m_uTintIndex);
	}
#endif // __BANK
}

////////////////////////////////////////////////////////////////////////////////

void CWeapon::PostPreRender()
{
	if(!m_pDrawableEntity || m_pDrawableEntity->GetIsVisible() || (m_pDrawableEntity->GetIsPhysical() && static_cast<CPhysical*>(m_pDrawableEntity.Get())->GetIsAttached() && GetHasFirstPersonScope()))
	{
		for(s32 i = 0; i < m_Components.GetCount(); i++)
		{
			CEntity* pFiringEntity = m_pDrawableEntity && m_pDrawableEntity->GetIsPhysical() ? (CPhysical *) static_cast<CPhysical*>(m_pDrawableEntity.Get())->GetAttachParent() : NULL;
			m_Components[i]->ProcessPostPreRender(pFiringEntity);
		}
	}

	m_bValidMuzzleMatrix = false;
	if(m_pSuppressorComponent && m_pSuppressorComponent->GetDrawable() && m_pSuppressorComponent->GetDrawable()->GetIsVisible())
	{
		s32 iMuzzleBoneIndex = m_pSuppressorComponent->GetMuzzleBoneIndex();
		if(iMuzzleBoneIndex != -1)
		{
			Matrix34 muzzleMatrix;
			m_pSuppressorComponent->GetDrawable()->GetGlobalMtx(iMuzzleBoneIndex, muzzleMatrix);
			m_muzzleRot = QuatVFromMat34V(MATRIX34_TO_MAT34V(muzzleMatrix));
			m_muzzlePos = VECTOR3_TO_VEC3V(muzzleMatrix.d);
			m_bValidMuzzleMatrix = true;
		}
	}

	if(!m_bValidMuzzleMatrix)
	{
		const CWeaponModelInfo* pWeaponModelInfo = GetWeaponModelInfo();
		if(pWeaponModelInfo)
		{
			s32 iBoneIndex = pWeaponModelInfo->GetBoneIndex(WEAPON_MUZZLE);
			if(m_pDrawableEntity && iBoneIndex != -1 && Verifyf(m_pDrawableEntity->GetSkeleton(), "Weapon '%s' has no skeleton!", pWeaponModelInfo->GetModelName()))
			{
				Matrix34 muzzleMatrix;
				m_pDrawableEntity->GetGlobalMtx(iBoneIndex, muzzleMatrix);
				m_muzzleRot = QuatVFromMat34V(MATRIX34_TO_MAT34V(muzzleMatrix));
				m_muzzlePos = VECTOR3_TO_VEC3V(muzzleMatrix.d);
				m_bValidMuzzleMatrix = true;
			}
		}
	}

	if(m_pDrawableEntity && m_pDrawableEntity->GetIsClass<CProjectile>())
	{
		CProjectile* pProjectile = static_cast<CProjectile*>(m_pDrawableEntity.Get());
		pProjectile->PostPreRender();
	}

	// update the muzzle smoke effects
	if (m_pMuzzleEntity && m_muzzleSmokeLevel>0.0f)
	{
		CEntity* pFiringEntity = m_pDrawableEntity && m_pDrawableEntity->GetIsPhysical() ? (CPhysical *) static_cast<CPhysical*>(m_pDrawableEntity.Get())->GetAttachParent() : NULL;
		g_vfxWeapon.UpdatePtFxMuzzleSmoke(pFiringEntity, this, m_pMuzzleEntity, m_iMuzzleBoneIndex, m_muzzleSmokeLevel, VECTOR3_TO_VEC3V(m_vMuzzleOffset));
	}
}

void CWeapon::ProcessGunFeed(bool visible)
{
	if(m_pDrawableEntity && m_pDrawableEntity->GetSkeleton())
	{
		int iBoneId = 0;
		if( m_pDrawableEntity->GetBaseModelInfo() )
		{
			iBoneId = m_pWeaponInfo->GetGunFeedBone().GetBoneIndex(m_pDrawableEntity->GetBaseModelInfo());
		}

		if(iBoneId != 0)
		{
			if(!visible)
			{
				m_pDrawableEntity->GetObjectMtxNonConst(iBoneId).Zero3x3();
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

f32 CWeapon::GetTimeBeforeNextShot() const
{
	switch(GetState())
	{
	case STATE_READY:
		return 0.f;

	case STATE_WAITING_TO_FIRE:
		return (static_cast<f32>(m_uTimer - m_uGlobalTime) / 1000.f);

	case STATE_RELOADING:
	case STATE_OUT_OF_AMMO:
	default:
		return -1.f;
	};
}

u32 CWeapon::GetTimeOfNextShot() const
{
	switch(GetState())
	{
	case STATE_WAITING_TO_FIRE:
		return m_uTimer;

	case STATE_RELOADING:
	case STATE_OUT_OF_AMMO:
	case STATE_READY:
	default:
		return 0;
	};
}

////////////////////////////////////////////////////////////////////////////////

bool CWeapon::CalcFireVecFromPos(const CEntity* pFiringEntity, const Matrix34& weaponMatrix, Vector3& vStart, Vector3& vEnd, const Vector3& vTarget) const
{
	// Get muzzle position
	GetMuzzlePosition(weaponMatrix, vStart);
	vEnd = vTarget;

	// Normalise shot vector
	Vector3 vShot = vEnd - vStart;

	if(pFiringEntity && pFiringEntity->GetIsTypePed())
	{
		static dev_float SMALL_DIST = 2.f;
		if(vShot.Mag2() < square(SMALL_DIST))
		{
			const CPed* pFiringPed = static_cast<const CPed*>(pFiringEntity);

			Matrix34 m;
			pFiringPed->GetGlobalMtx(pFiringPed->GetBoneIndexFromBoneTag(BONETAG_R_UPPERARM), m);
			vStart = m.d;
			vShot = vEnd - vStart;
		}
	}

	vShot.NormalizeFast();

	// Now scale up to weapon range
	f32 fWeaponRange = GetRange();
	vEnd = vStart + vShot * fWeaponRange;
	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CWeapon::CalcFireVecFromEnt(const CEntity* UNUSED_PARAM(pFiringEntity), const Matrix34& weaponMatrix, Vector3& vStart, Vector3& vEnd, CEntity* pTargetEnt, Vector3* pvOffset, s32 iBoneIndex) const
{
	weaponFatalAssertf(pTargetEnt, "pTargetEnt is NULL");

	// Get muzzle position
	GetMuzzlePosition(weaponMatrix, vStart);
	vEnd = VEC3V_TO_VECTOR3(pTargetEnt->GetTransform().GetPosition());

	if(iBoneIndex != -1 && pTargetEnt->GetIsDynamic() && static_cast<CDynamicEntity*>(pTargetEnt)->GetSkeleton())
	{
		Matrix34 matTarget;
		static_cast<CDynamicEntity*>(pTargetEnt)->GetGlobalMtx(iBoneIndex, matTarget);

		if(pvOffset)
		{
			matTarget.Transform(*pvOffset, vEnd);
		}
		else
		{
			vEnd = matTarget.d;
		}
	}
	else if(pvOffset)
	{
		pTargetEnt->TransformIntoWorldSpace(vEnd, *pvOffset);
	}

	// Normalise shot vector
	Vector3 vShot = vEnd - vStart;
	vShot.NormalizeFast();

	// Now scale up to weapon range
	f32 fWeaponRange = GetRange();
	vEnd = vStart + vShot * fWeaponRange;
	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CWeapon::CalcFireVecFromAimCamera(const CEntity* pFiringEntity, const Matrix34& weaponMatrix, Vector3& vStart, Vector3& vEnd, bool bSkipAimCheck) const
{
	// Get muzzle position
	Matrix34 matGunMuzzle;
	GetMuzzleMatrix(weaponMatrix, matGunMuzzle);

	const camFrame& aimFrame = camInterface::GetPlayerControlCamAimFrame();

	Vector3 vShot;
	vStart = aimFrame.GetPosition();
	vShot  = aimFrame.GetFront();

	bool bFirstPersonMountedCam = false;
	const camBaseCamera* pDominantRenderedCamera = camInterface::GetDominantRenderedCamera();
	if(pDominantRenderedCamera && pDominantRenderedCamera->GetIsClassId(camCinematicMountedCamera::GetStaticClassId()))
	{
		const camCinematicMountedCamera* pMountedCamera	= static_cast<const camCinematicMountedCamera*>(pDominantRenderedCamera);
		// Do not check IsFirstPersonCamera, so this is done for both pov AND bonnet cameras.
		{
			const camFrame& mountedCameraFrame = pMountedCamera->GetFrame();
			vStart = mountedCameraFrame.GetPosition();
			vShot  = mountedCameraFrame.GetFront();
			bFirstPersonMountedCam = true;
		}
	}

	// Move source of gunshot vector along to the gun muzzle so can't hit anything behind the gun.
	//Don't move the position if we are using the fps camera.
	if(!bFirstPersonMountedCam && !camInterface::GetGameplayDirector().IsFirstPersonAiming())
	{
		f32 fShotDot = vShot.Dot(matGunMuzzle.d - vStart);
		//Add a small offset to bring the position back along the cam front. This is to help with collision issues when aiming high
		static dev_float SMALL_DIST_OFFSET = 0.15f;
		fShotDot -= SMALL_DIST_OFFSET;
		vStart += fShotDot * vShot;
	}

	// Now scale up to weapon range
	f32 fWeaponRange = GetRange();
#if FPS_MODE_SUPPORTED
	if(pFiringEntity && pFiringEntity->GetIsTypePed() && static_cast<const CPed*>(pFiringEntity)->IsFirstPersonShooterModeEnabledForPlayer(false))
		fWeaponRange = Max(1.f, fWeaponRange);
#endif // FPS_MODE_SUPPORTED
	vEnd = vStart + vShot * fWeaponRange;

	return (pFiringEntity && !bSkipAimCheck) ? camInterface::GetGameplayDirector().IsAiming(pFiringEntity) : true;
}

////////////////////////////////////////////////////////////////////////////////

bool CWeapon::CalcFireVecFromGunOrientation(const CEntity* UNUSED_PARAM(pFiringEntity), const Matrix34& weaponMatrix, Vector3& vStart, Vector3& vEnd) const
{
	Matrix34 muzzleMatrix;
	GetMuzzleMatrix(weaponMatrix, muzzleMatrix);

	// Get muzzle position
	vStart = muzzleMatrix.d;
	vEnd   = muzzleMatrix.d + (muzzleMatrix.a * GetRange());
	return true;
}

////////////////////////////////////////////////////////////////////////////////

void CWeapon::SpinUp(CEntity* pFiringEntity)
{
	if(m_pWeaponInfo && m_pWeaponInfo->GetSpinUpTime() > 0.f)
	{
		if(m_State == STATE_SPINNING_DOWN)
		{
			// Spin up
			SetTimer(m_uGlobalTime + Max<u32>(static_cast<u32>(1000.0f * GetWeaponInfo()->GetSpinUpTime()) - (m_uTimer - m_uGlobalTime), 0));
			SetState(STATE_SPINNING_UP);
			CreateSpinUpSonarBlip(pFiringEntity);
		}
		else if(!m_bNeedsToSpinDown && m_State == STATE_READY)
		{
			if(m_bNoSpinUp)
			{
				// Start spinning at full speed
				m_bNeedsToSpinDown = true;
				m_bNoSpinUp = false;// reset
			}
			else
			{
				// Spin up
				SetTimer(m_uGlobalTime + static_cast<u32>(1000.0f * GetWeaponInfo()->GetSpinUpTime()));
				SetState(STATE_SPINNING_UP);
				CreateSpinUpSonarBlip(pFiringEntity);
			}
		}
	}

	// Keep spinning
	if(m_pWeaponInfo && m_bNeedsToSpinDown && m_State == STATE_READY)
	{
		SetTimer(m_uGlobalTime + static_cast<u32>(1000.0f * GetWeaponInfo()->GetSpinTime()));
	}
}

void CWeapon::CreateSpinUpSonarBlip(CEntity* pFiringEntity)
{
	weaponAssert(pFiringEntity);
	if (pFiringEntity && pFiringEntity->GetIsTypePed() && m_pWeaponInfo && !m_pWeaponInfo->GetIsTurret())
	{
		CPed* pFiringPed = static_cast<CPed*>(pFiringEntity);
		CMiniMap::CreateSonarBlipAndReportStealthNoise(pFiringPed, pFiringPed->GetTransform().GetPosition(), CMiniMap::sm_Tunables.Sonar.fSoundRange_WeaponSpinUp, HUD_COLOUR_RED);
	}
}

////////////////////////////////////////////////////////////////////////////////

Matrix34 CWeapon::BuildViewMatrix(const Vector3& vPosition, Vector3 vForwardVector) const
{
	Vector3 vWorldUpVector(0.0f, 0.0f, 1.0f);

	// Steps (Gram Schmidt method using the cross product):
	// 1. Normalize the direction vector and the up vector
	vForwardVector.Normalize();

	// 2. If given forward is up, set up to -X (LHR), to avoid a <0,0,0> cross result
	if (vForwardVector == vWorldUpVector)
	{
		vWorldUpVector.Set(-1.0f, 0.0f, 0.0f);
	}

	// 3. Take the cross product between the forward vector and the
	// world up vector
	Vector3 vRightVector = vForwardVector;
	vRightVector.Cross(vWorldUpVector);

	// We need to normalize this vector since the cross product of two
	// unit vectors is only guaranteed to be normalized if the angle 
	// between them is 90 degrees
	vRightVector.NormalizeSafe();

	// 4. Take the cross product between the new vector and the
	// window normal
	// We know for a fact that these two vectors are perpendicular
	// hence, there is no need to normalize the result afterwards
	Vector3 vUpVector = vRightVector;
	vUpVector.Cross(vForwardVector);

	// Build the matrix and return it
	return Matrix34( vRightVector.x, vRightVector.y, vRightVector.z,
					 vForwardVector.x, vForwardVector.y, vForwardVector.z,
					 vUpVector.x, vUpVector.y, vUpVector.z,
					 vPosition.x, vPosition.y, vPosition.z );
}

////////////////////////////////////////////////////////////////////////////////

bool CWeapon::CalcFireVecFromWeaponMatrix(const Matrix34& weaponMatrix, Vector3& vStart, Vector3& vEnd) const
{
	vStart = weaponMatrix.d;
	vEnd   = weaponMatrix.d + (weaponMatrix.a * GetRange());
	return true;
}

////////////////////////////////////////////////////////////////////////////////

void CWeapon::GetAccuracy(const CPed* pFiringPed, float fDesiredTargetDist, sWeaponAccuracy& accuracy) const
{
	if(pFiringPed)
	{
		weaponAssert(pFiringPed->GetWeaponManager());
		pFiringPed->GetWeaponManager()->GetAccuracy().CalculateAccuracyRange(*pFiringPed, *GetWeaponInfo(), fDesiredTargetDist, accuracy);
	}

	for(s32 i = 0; i < m_Components.GetCount(); i++)
	{
		m_Components[i]->ModifyAccuracy(accuracy);
	}

	accuracy.fAccuracyMin = Clamp(accuracy.fAccuracyMin, 0.f, 1.f);
	accuracy.fAccuracyMax = Clamp(accuracy.fAccuracyMax, 0.f, 1.f);

	// Apply the weapon accuracy
	//Printf( "fAccuracy<%f> - Random( fAccuracyMin(%f), accuracy.fAccuracyMax(%f) ) ", accuracy.fAccuracy, accuracy.fAccuracyMin, accuracy.fAccuracyMax );
	accuracy.fAccuracy = NetworkInterface::IsNetworkOpen() ? g_ReplayRand.GetRanged(accuracy.fAccuracyMin, accuracy.fAccuracyMax) : fwRandom::GetRandomNumberInRange(accuracy.fAccuracyMin, accuracy.fAccuracyMax);
	//Printf( "-> fAccuracy<%f> \n\n\n", accuracy.fAccuracy );

	// Check to see if we should always miss
	if( accuracy.fAccuracy <= VERY_SMALL_FLOAT )
	{
		accuracy.bShouldAlwaysMiss = true;
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CWeapon::GetCanReload() const
{
	if (GetWeaponInfo()->GetCanReload())
	{
		if (GetWeaponInfo()->GetCreateVisibleOrdnance())
		{
			bool bNoOrdnance = m_pDrawableEntity && m_pDrawableEntity->GetIsTypeObject() && !CPedEquippedWeapon::GetProjectileOrdnance(static_cast<const CObject*>(m_pDrawableEntity.Get()));
			return (((GetAmmoTotal() - GetAmmoInClip()) > 0 || (GetAmmoTotal() > 0 && GetAmmoTotal() == GetAmmoInClip() && bNoOrdnance) || GetAmmoTotal() == INFINITE_AMMO)) && 
				    (GetAmmoInClip() < GetClipSize() || bNoOrdnance);
		}
		else
		{
			return ((GetAmmoTotal() - GetAmmoInClip()) > 0 || GetAmmoTotal() == INFINITE_AMMO ) && GetAmmoInClip() < GetClipSize();
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CWeapon::GetNeedsToReload(bool bHasLosToTarget) const
{
	if(GetWeaponInfo()->GetUsesAmmo() && GetClipSize() > 0)
	{
		TUNE_GROUP_FLOAT(WEAPON_TUNE, LOW_AMMO_NO_LOS, 0.75f, 0.0f, 1.0f, .01f);
		bool bIsAmmoLow = bHasLosToTarget ? GetAmmoInClip() == 0 : GetAmmoInClip() < ((float)GetClipSize() * LOW_AMMO_NO_LOS);
		return (bIsAmmoLow || (GetWeaponInfo()->GetCreateVisibleOrdnance() && m_pDrawableEntity && m_pDrawableEntity->GetIsTypeObject() && !CPedEquippedWeapon::GetProjectileOrdnance(static_cast<const CObject*>(m_pDrawableEntity.Get()))));
	}

 	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CWeapon::StartReload(CEntity* pFiringEntity)
{
	s32 iReloadTime = GetReloadTime(pFiringEntity);
	if(iReloadTime < 0)
	{
		m_uTimer = 0;
	}
	else
	{
		SetTimer(fwTimer::GetTimeInMilliseconds() + iReloadTime);
	}

	// If we reload, trigger the spin down
	m_bNeedsToSpinDown = false;

	if(GetAmmoInClip() > 0)
	{
		m_bReloadClipNotEmpty = true;
	}
	else
	{
		m_bReloadClipNotEmpty = false;
	}
	// Reload
    if(pFiringEntity && pFiringEntity->GetIsTypePed())
	{
		//@@: location CWEAPON_STARTRELOAD_CHECK_LOCAL_PLAYER
		const CPed* pFiringPed = static_cast<const CPed*>(pFiringEntity);
		if(pFiringPed->IsLocalPlayer())
		{
			//@@: range CWEAPON_STARTRELOAD_RAGESEC_REACTION {
			RAGE_SEC_POP_REACTION
			//@@: } CWEAPON_STARTRELOAD_RAGESEC_REACTION
		}
        //@@: location CWEAPON_STARTRELOAD_PROCESS_STATE
	}
	SetState(STATE_RELOADING);
}

void CWeapon::CancelReload()
{
	if (GetState()==STATE_RELOADING)
	{
		if(GetAmmoTotal() == 0 && !GetWeaponInfo()->GetIsMelee())
		{
			SetState(STATE_OUT_OF_AMMO);			
		}
		else
		{
			SetState(STATE_READY);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CWeapon::DoReload(bool bInit)
{
#if FPS_MODE_SUPPORTED
	// Don't set those weapons that don't use ammo to STATE_OUT_OF_AMMO.
	// Fixed the bug where the out of ammo audio was triggered when swapping out them in FPS.
	if(!GetWeaponInfo()->GetUsesAmmo())
	{
		SetState(STATE_READY);
		return;
	}
#endif // FPS_MODE_SUPPORTED

	s32 iBulletsToReload = GetClipSize();

	if (GetWeaponInfo()->GetUseLoopedReloadAnim() && !bInit)
	{
		iBulletsToReload = GetWeaponInfo()->GetBulletsPerAnimLoop();
	}

	//@@: location CWEAPON_DORELOAD_GET_REMAINING_AMMO
	const s32 iRemainingAmmo = GetAmmoTotal() - GetAmmoInClip();

	// No spare ammo - can't reload
	if(GetAmmoTotal() == 0 && !GetWeaponInfo()->GetIsMelee())
	{
		SetState(STATE_OUT_OF_AMMO);
		return;
	}

	if(iRemainingAmmo < iBulletsToReload && GetAmmoTotal() != INFINITE_AMMO)
	{
		// The clip is the current ammo plus the remaining ammo
		SetAmmoInClip(GetAmmoInClip()+iRemainingAmmo);
	}
	else
	{
		// Add bullets to clip
		SetAmmoInClip(GetAmmoInClip()+iBulletsToReload);
	}

	if(m_pMoveNetworkHelper && m_pMoveNetworkHelper->IsNetworkActive())
	{
		static const fwMvRequestId s_OnReloadedRequestId("OnReloaded",0xbbc89d2c);
		m_pMoveNetworkHelper->SendRequest(s_OnReloadedRequestId);
	}

	// Ready to fire again
	SetState(STATE_READY);
	// Process the weapon state machine and update the global time to be the current time
	ProcessState(NULL, fwTimer::GetTimeInMilliseconds());
}

////////////////////////////////////////////////////////////////////////////////

bool CWeapon::GetHasInfiniteClips(const CEntity* pFiringEntity) const
{
	return (pFiringEntity && pFiringEntity->GetIsTypePed()) ? static_cast<const CPed*>(pFiringEntity)->GetInventory()->GetAmmoRepository().GetIsUsingInfiniteClips() : false;
}

////////////////////////////////////////////////////////////////////////////////

void CWeapon::StartAnim(const fwMvClipSetId& clipSetId, const fwMvClipId& clipId, f32 fBlendDuration, f32 fRate, f32 fPhase, bool bLoop, bool bPhaseSynch, bool bForceAnimUpdate)
{
	static const fwMvRequestId s_AnimRequestId("Anim",0xA802776);
	static const fwMvClipId s_AnimClipId("Anim_Clip",0xCB86D6C8);
	static const fwMvFloatId s_AnimBlendDurationId("Anim_BlendDuration",0x78DDCE60);
	static const fwMvFloatId s_AnimRateId("Anim_Rate",0x6A045254);
	static const fwMvFloatId s_AnimPhaseId("Anim_Phase",0xBA25CA2B);
	static const fwMvBooleanId s_AnimLoopedId("Anim_Looped",0x78172511);
	static const fwMvFlagId s_PhaseSync("PhaseSync", 0xf734f793);

	fwClipSet* pClipSet = fwClipSetManager::GetClipSet(clipSetId);
	if(pClipSet)
	{
		if(pClipSet->Request_DEPRECATED())
		{
			const crClip* pClip = fwAnimManager::GetClipIfExistsBySetId(clipSetId, clipId);
			if(pClip)
			{
				if(!m_pMoveNetworkHelper)
				{
					CreateMoveNetworkHelper();
				}

				if(m_pMoveNetworkHelper && m_pMoveNetworkHelper->IsNetworkActive())
				{
					m_pMoveNetworkHelper->SendRequest(s_AnimRequestId);
					m_pMoveNetworkHelper->SetClip(pClip, s_AnimClipId);
					m_pMoveNetworkHelper->SetFloat(s_AnimBlendDurationId, fBlendDuration);
					m_pMoveNetworkHelper->SetFloat(s_AnimRateId, fRate);
					m_pMoveNetworkHelper->SetFloat(s_AnimPhaseId, fPhase);
					m_pMoveNetworkHelper->SetBoolean(s_AnimLoopedId, bLoop);
					m_pMoveNetworkHelper->SetFlag(bPhaseSynch, s_PhaseSync);

					if (bForceAnimUpdate)
					{
						static_cast<CObject*>(m_pDrawableEntity.Get())->ForcePostCameraAnimUpdate(true, true);
					}
				}
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CWeapon::StopAnim(f32 fBlendDuration)
{
	if(m_pMoveNetworkHelper && m_pMoveNetworkHelper->IsNetworkActive())
	{
		static const fwMvRequestId s_NoAnimRequestId("NoAnim",0x7571364B);
		static const fwMvFloatId s_NoAnimBlendDurationId("NoAnim_BlendDuration",0xB5BF6A2B);
		m_pMoveNetworkHelper->SendRequest(s_NoAnimRequestId);
		m_pMoveNetworkHelper->SetFloat(s_NoAnimBlendDurationId, fBlendDuration);

		m_bOutOfAmmoAnimIsPlaying = false;
	}
}

////////////////////////////////////////////////////////////////////////////////

void CWeapon::ProcessGripAnim(CEntity* pFiringEntity)
{
	const CWeaponInfo* pWeaponInfo = GetWeaponInfo();

	if(pWeaponInfo->GetFlag(CWeaponInfoFlags::ProcessGripAnim))
	{
		//Grab the firing ped.
		CPed* pFiringPed = NULL;
		if(pFiringEntity && pFiringEntity->GetIsTypePed())
		{
			pFiringPed = static_cast<CPed *>(pFiringEntity);
		}

		if(pFiringPed)
		{
			static const fwMvClipId s_GripClipId("w_grip_l",0x8cba7c0a);
			static const fwMvClipId s_ActionModeGripClipId("w_actionmode_grip_l",0xf052c9d0);
			static const fwMvClipId s_CoverGripClipId("w_cover_grip_l",0xf252b855);

			float fBlendDelta = NORMAL_BLEND_DURATION;

			const CTaskMotionBase* pMotionTask = pFiringPed->GetPrimaryMotionTask();
			const CTaskMotionPed* pMotionPedTask = pMotionTask->GetTaskType() == CTaskTypes::TASK_MOTION_PED ? static_cast<const CTaskMotionPed*>(pMotionTask) : NULL;

			const CTaskMotionBase* pCurrentMotionTask = pFiringPed->GetCurrentMotionTask();
			const CTaskHumanLocomotion* pLocoTask = pCurrentMotionTask->GetTaskType() == CTaskTypes::TASK_HUMAN_LOCOMOTION ? static_cast<const CTaskHumanLocomotion*>(pCurrentMotionTask) : NULL;

			fwMvClipSetId clipSetId = m_pMoveNetworkHelper ? m_pMoveNetworkHelper->GetClipSetId() : pWeaponInfo->GetAppropriateWeaponClipSetId(pFiringPed);
			fwMvClipId clipId;
			if((pFiringPed->GetIsInCover() || pFiringPed->GetPedResetFlag(CPED_RESET_FLAG_IsEnteringCover)) && !pFiringPed->GetPedResetFlag(CPED_RESET_FLAG_IsAimingFromCover) && !pFiringPed->GetPedResetFlag(CPED_RESET_FLAG_IsDoingCoverAimIntro))
			{
				clipId = s_CoverGripClipId;
			}
			else if(pMotionPedTask && (pMotionPedTask->GetState() == CTaskMotionPed::State_ActionMode || (pMotionPedTask->GetPreviousState() == CTaskMotionPed::State_ActionMode && pMotionPedTask->GetTimeInState() < 0.1f)))
			{
				clipId = s_ActionModeGripClipId;
			}
			else if(pLocoTask && pLocoTask->GetMoveNetworkHelper() && pLocoTask->GetMoveNetworkHelper()->GetBoolean(ms_UseActionModeGripId))
			{
				clipId = s_ActionModeGripClipId;
			}
			else
			{
				clipId = s_GripClipId;
				fBlendDelta = REALLY_SLOW_BLEND_DURATION;
			}

			const crClip* pClip = NULL;
			if(clipSetId != CLIP_SET_ID_INVALID)
			{
				pClip = fwAnimManager::GetClipIfExistsBySetId(clipSetId, clipId);
			}

			TUNE_GROUP_BOOL(BUG_2058873, FORCE_BLEND_OUT_GRIP_WHEN_TRANSITIONING_TO_FIRST_PERSON, true);
			bool bForceInstantBlendOutGripAnim = false;
			if (FORCE_BLEND_OUT_GRIP_WHEN_TRANSITIONING_TO_FIRST_PERSON && pFiringPed->IsLocalPlayer() && pFiringPed->GetPlayerInfo() && 
				(pFiringPed->GetPlayerInfo()->GetSwitchedToFirstPersonCount() > 0 || (pFiringPed->GetMotionData()->GetUsingFPSMode() && !pFiringPed->GetMotionData()->GetWasUsingFPSMode())))
			{
				bForceInstantBlendOutGripAnim = true;
			}

			if(pClip && !bForceInstantBlendOutGripAnim)
			{
				if(!m_pMoveNetworkHelper)
				{
					fwClipSet* pClipSet = fwClipSetManager::GetClipSet(clipSetId);
					if(pClipSet && pClipSet->Request_DEPRECATED())
					{
						CreateMoveNetworkHelper();
						if(m_pMoveNetworkHelper && m_pMoveNetworkHelper->IsNetworkActive())
						{
							// Set the clip set
							m_pMoveNetworkHelper->SetClipSet(clipSetId);
						}
					}
				}

				if(m_pMoveNetworkHelper)
				{
					m_pMoveNetworkHelper->SetClip(pClip, ms_GripClipId);
					m_pMoveNetworkHelper->SetFlag(true, ms_HasGripAnimId);
					m_pMoveNetworkHelper->SetFloat(ms_GripBlendDurationId, fBlendDelta);

					if(m_GripClipSetId != clipSetId || m_GripClipId != clipId)
					{
						m_pMoveNetworkHelper->SendRequest(ms_RestartGripId);

						m_GripClipSetId = clipSetId;
						m_GripClipId = clipId;
					}
				}
			}
			else
			{
				if(m_pMoveNetworkHelper)
				{
					m_pMoveNetworkHelper->SetFlag(false, ms_HasGripAnimId);

					if (bForceInstantBlendOutGripAnim)
					{
						m_pMoveNetworkHelper->SetFloat(ms_GripBlendDurationId, INSTANT_BLEND_DURATION);
						static_cast<CObject*>(m_pDrawableEntity.Get())->ForcePostCameraAnimUpdate(true, true);
					}
				}

				m_GripClipSetId = CLIP_SET_ID_INVALID;
				m_GripClipId = CLIP_ID_INVALID;
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CWeapon::ProcessIdleAnim(CEntity* pFiringEntity)
{
	TUNE_GROUP_FLOAT(DOUBLE_ACTION, INPUT_ACTION_START_PHASE, 0.2f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(DOUBLE_ACTION, INPUT_ACTION_END_PHASE, 1.0f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(DOUBLE_ACTION, INPUT_ACTION_APPROACH_RATE, 10.0f, 0.0f, 50.0f, 0.01f);
	TUNE_GROUP_FLOAT(DOUBLE_ACTION, INPUT_ZERO_APPROACH_RATE, 7.5f, 0.0f, 50.0f, 0.01f);

	if (m_pMoveNetworkHelper)
	{
		// Drive the phase of the idle animation based on the trigger input for a realistic double-action mechanism
		if (GetWeaponInfo() && GetWeaponInfo()->GetIdlePhaseBasedOnTrigger())
		{
			float fIdleAnimTargetPhase = 0.0f;
			bool bSkipApproach = false;
			CPed* pFiringPed = NULL;

			if(pFiringEntity && pFiringEntity->GetIsTypePed())
			{
				pFiringPed = static_cast<CPed *>(pFiringEntity);
			}

			if(pFiringPed)
			{
				bool bPassesTargetChecks = false;
				if (pFiringPed->IsLocalPlayer() && pFiringPed->GetPlayerInfo())
				{
					const CPlayerPedTargeting & targeting = pFiringPed->GetPlayerInfo()->GetTargeting();				
					const CEntity* pTargetEntity = targeting.GetLockOnTarget() ? targeting.GetLockOnTarget() : targeting.GetFreeAimTargetRagdoll();	
					bPassesTargetChecks = pFiringPed->IsAllowedToDamageEntity(GetWeaponInfo(), pTargetEntity);
				}

				bool bPassesStateChecks = (GetState() == STATE_READY || GetState() == STATE_OUT_OF_AMMO)
										&& pFiringPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsAimingGun)
										&& !pFiringPed->GetPedConfigFlag(CPED_CONFIG_FLAG_WasFiring) 
										&& !pFiringPed->GetPedResetFlag(CPED_RESET_FLAG_IsDoingDriveby);

				bool bWeaponWheelActive = false;
				if (pFiringPed->IsLocalPlayer())
				{
					CControl* pControl = pFiringPed->GetControlFromPlayer();
					bWeaponWheelActive = CNewHud::IsWeaponWheelActive() || (pControl && (pControl->GetSelectWeapon().IsPressed() || pControl->GetSelectWeapon().IsDown()));
				}

				bSkipApproach = pFiringPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsFiring);

				if (bPassesTargetChecks && bPassesStateChecks && !bWeaponWheelActive)
				{
					float fAttackValue = pFiringPed->GetPlayerInfo()->GetFiringAttackValue();
					float fBoundaryValue = pFiringPed->GetPlayerInfo()->GetFiringBoundaryValue();
					float fAttackPhase = Min(fAttackValue / fBoundaryValue, 1.0f);

					if (fAttackPhase >= INPUT_ACTION_START_PHASE)
					{
						float fAnimPhaseMin = fBoundaryValue * INPUT_ACTION_START_PHASE;
						float fAnimPhaseMax = fBoundaryValue * INPUT_ACTION_END_PHASE;
						fIdleAnimTargetPhase = (fAttackValue - fAnimPhaseMin) / (fAnimPhaseMax - fAnimPhaseMin);
					}
				}
			}

			if (bSkipApproach)
			{
				m_fIdleAnimPhase = fIdleAnimTargetPhase;
			}
			else
			{
				float fApproachRate = (fIdleAnimTargetPhase <= 0.0f) ? INPUT_ZERO_APPROACH_RATE : INPUT_ACTION_APPROACH_RATE;
				rage::Approach(m_fIdleAnimPhase, fIdleAnimTargetPhase, fApproachRate, fwTimer::GetTimeStep());
			}

			if(pFiringPed && pFiringPed->IsLocalPlayer())
			{
				g_WeaponAudioEntity.SetPlayerDoubleActionWeaponPhase(this, m_fPrevIdleAnimPhase, m_fIdleAnimPhase);
			}

			m_pMoveNetworkHelper->SetFloat(ms_WeaponIdleRate, 0.0f);
			m_pMoveNetworkHelper->SetFloat(ms_WeaponIdlePhase, m_fIdleAnimPhase);
		}
	}

	m_fPrevIdleAnimPhase = m_fIdleAnimPhase;
}

////////////////////////////////////////////////////////////////////////////////

void CWeapon::SendFireMessage(CEntity* pFiringEntity, const Vector3& vWeaponPos, WorldProbe::CShapeTestResults& results, s32 iNumIntersections, bool bUseDamageOverride, float fDamageOverride, const u32 damageFlags, const u32 actionResultId /* = 0 */, const u16 meleeId /* = 0 */, u32 nForcedReactionDefinitionID /* = 0 */, CWeaponDamage::NetworkWeaponImpactInfo * const networkWeaponImpactInfo /* = NULL */, const float fRecoilAccuracyWhenFired /* = 1.f*/, const float fFallOffRangeModifier /* = 1.f*/, const float fFallOffDamageModifier /* = 1.f*/, const Vector3* const forceDir /* = 0 */)
{
	weaponDebugf3("CWeapon::SendFireMessage");

	if(!NetworkInterface::IsGameInProgress())
	{
		return;
	}

	weaponAssert(pFiringEntity && !NetworkUtils::IsNetworkClone(pFiringEntity));

	struct sEntityWeaponDamageInfo
	{
		sEntityWeaponDamageInfo()
			: pHitEntity(NULL)
			, iHitComponent(-1)
			, iSuspensionIndex(-1)
			, iTyreIndex(-1)
			, bHitEntityWeapon(false)
			, bHitWeaponAttachment(false)
		{
		}

		CEntity* pHitEntity;
		Vector3  vHitPosition;
		s32	     iHitComponent;
		s32      iSuspensionIndex;
		s32      iTyreIndex;
		f32      fSumDamage;
		bool	 bHitEntityWeapon;
		bool	 bHitWeaponAttachment;
	};

	// We need to send a weapon damage event for each entity hit
	f32 fDamage = (bUseDamageOverride ? fDamageOverride : GetWeaponInfo()->GetDamage());

	bool bMeleeDamage = ((damageFlags & CPedDamageCalculator::DF_MeleeDamage) != 0);

	// Incorporate the weapon damage multiplier for this player ped
	if( pFiringEntity && pFiringEntity->GetIsTypePed() )
	{
		CPed* pFiringPed = static_cast<CPed*>(pFiringEntity);
		Assert( pFiringPed );
		if( pFiringPed->IsPlayer() )
		{
			Assert( pFiringPed->GetPlayerInfo() );
			if( bMeleeDamage )
				fDamage *= pFiringPed->GetPlayerInfo()->GetPlayerMeleeDamageModifier(GetWeaponInfo()->GetIsUnarmed());
			else 
				fDamage *= pFiringPed->GetPlayerInfo()->GetPlayerWeaponDamageModifier();
		}
		else
		{
			if (bMeleeDamage)
			{
				fDamage *= CPedDamageCalculator::GetAiMeleeWeaponDamageModifier();
			}
			else
			{
				fDamage *= CPedDamageCalculator::GetAiWeaponDamageModifier();
			}

			if (GetWeaponInfo()->GetIsArmed())
			{
				fDamage *= pFiringPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatWeaponDamageModifier);
			}
		}
	}

	// first get the distinct entities hit
	u32 uNumHitEntities = 0;
	static const s32 FIRE_INSTANT_HIT_NUM_RESULTS = 8;
	weaponAssertf(iNumIntersections <= FIRE_INSTANT_HIT_NUM_RESULTS, "More intersections [%d] than we can handle [%d]", iNumIntersections, FIRE_INSTANT_HIT_NUM_RESULTS);
	sEntityWeaponDamageInfo hitEntities[FIRE_INSTANT_HIT_NUM_RESULTS];

	for(s32 i = 0; i < iNumIntersections; i++)
	{
		WorldProbe::CShapeTestHitPoint& result = results[i];

		if(result.GetHitDetected())
		{
			CEntity* pHitEntity = CPhysics::GetEntityFromInst(result.GetHitInst());

			//---

			// Guns are local objects that aren't networked - if we hit a gun (or the ammo box attached to the gun) held by a ped, 
			// we still need the fire message to get sent across as CWeaponDamage::DoWeaponImpactWeapon extends the ray cast from 
			// weapon to target to see if we hit the ped holding the gun (making them penetrable objects)
			bool hitWeapon				= false;
			bool hitWeaponAttachment	= false;

			// Have we hit a ped behind a weapon or ammo box attached to a weapon?
			if(networkWeaponImpactInfo && networkWeaponImpactInfo->GetDamagedHoldingPed())
			{
				if(pHitEntity && pHitEntity->GetIsTypeObject())
				{
					CEntity* pWeaponHoldingEntity = NULL;

					const CObject* pHitObject = static_cast<CObject*>(pHitEntity);
					
					// we've hit the weapon...
					if(pHitObject->GetWeapon())
					{
						pWeaponHoldingEntity = (CPhysical*)pHitObject->GetAttachParent();
					}
					// we've hit the ammo magazine attached to the weapon...
					else if(pHitObject->GetAttachParent() && ((CEntity*)pHitObject->GetAttachParent())->GetIsTypeObject() && ((CObject*)pHitObject->GetAttachParent())->GetWeapon())
					{
						pWeaponHoldingEntity = (CEntity*)((CObject*)pHitObject->GetAttachParent())->GetAttachParent();
					}

					// if we've got the ped...
					if(pWeaponHoldingEntity && pWeaponHoldingEntity->GetIsTypePed())
					{
						pHitEntity				= pWeaponHoldingEntity;
						hitWeapon				= true;
						hitWeaponAttachment		= networkWeaponImpactInfo->GetAmmoAttachment();
					}
					else
					{
						// something has went wrong?
						continue;
					}
				}				
			}

			//---

			if(pHitEntity && NetworkUtils::GetNetworkObjectFromEntity(pHitEntity))
			{
				bool bEntityFound = false;
				u32  uEntityIndex = 0;

				for(u32 j = 0; j < uNumHitEntities && !bEntityFound; j++)
				{
					if(hitEntities[j].pHitEntity == pHitEntity)
					{
						bEntityFound = true;
						uEntityIndex = j;
					}
				}

				if(!bEntityFound)
				{
					hitEntities[uNumHitEntities].pHitEntity				= pHitEntity;
					hitEntities[uNumHitEntities].vHitPosition			= result.GetHitPosition();
					hitEntities[uNumHitEntities].iHitComponent			= result.GetHitComponent();
					
#if __ASSERT					
					//Trying to track down the cause of B* 2021715
					if (pHitEntity->GetIsTypePed())
					{
						s32 iHitComponent = hitEntities[uNumHitEntities].iHitComponent;
						weaponAssertf(iHitComponent>=0 && iHitComponent < HIGH_RAGDOLL_LOD_NUM_COMPONENTS, "CWeapon::SendFireMessage - Invalid hit component (%d) for ped returned from shape test", iHitComponent);
					}
#endif //__ASSERT
					hitEntities[uNumHitEntities].fSumDamage				= -1.0f;
					hitEntities[uNumHitEntities].bHitEntityWeapon		= hitWeapon;
					hitEntities[uNumHitEntities].bHitWeaponAttachment	= hitWeaponAttachment;
					uEntityIndex = uNumHitEntities;
					uNumHitEntities++;
				}
				else
				{
					float fFallOffDamage = CWeaponDamage::ApplyFallOffDamageModifier(pHitEntity, GetWeaponInfo(), vWeaponPos, fFallOffRangeModifier, fFallOffDamageModifier, fDamage);

					if(hitEntities[uEntityIndex].fSumDamage < 0.0f)
					{
						hitEntities[uEntityIndex].fSumDamage = fFallOffDamage;
					}

					// If weapon hits the same entity again, need to accumulate damage for each hit (e.g. multiple pellets from shotgun)
					hitEntities[uEntityIndex].fSumDamage += fFallOffDamage;
				}
				
				if(pHitEntity->GetIsTypeVehicle())
				{
					CVehicle* pHitVehicle = static_cast<CVehicle*>(pHitEntity);

					Vector3 vImpactPosLocal = hitEntities[uEntityIndex].vHitPosition;
					if(vImpactPosLocal.IsNonZero())
					{
						vImpactPosLocal = VEC3V_TO_VECTOR3(pHitVehicle->GetTransform().UnTransform(VECTOR3_TO_VEC3V(hitEntities[uEntityIndex].vHitPosition)));
					}

					for(s32 w = 0; w < pHitVehicle->GetNumWheels(); w++)
					{
						Vector3 vSusPos;
						float fSusRadius = pHitVehicle->GetWheel(w)->GetSuspensionPos(vSusPos);
						if((vImpactPosLocal - vSusPos).Mag2() < fSusRadius*fSusRadius)
						{
							hitEntities[uEntityIndex].iSuspensionIndex = w;
							break;
						}
					}

					if(PGTAMATERIALMGR->GetPolyFlagVehicleWheel(result.GetHitMaterialId()))
					{
						hitEntities[uEntityIndex].iTyreIndex = result.GetHitPartIndex();
					}
				}
			}
		}
	}

	weaponDebugf3("CWeapon::SendFireMessage--uNumHitEntities[%d]",uNumHitEntities);

	for(u32 i = 0; i < uNumHitEntities; i++)
	{
		CEntity* pHitEntity = hitEntities[i].pHitEntity;
		weaponAssert(pHitEntity);

#if !__FINAL
		if (pHitEntity)
		{
			weaponDebugf3("CWeapon::SendFireMessage--pHitEntity[%d]->GetModelName()[%s]",i,pHitEntity->GetModelName());
		}
#endif

		if(NetworkUtils::GetNetworkObjectFromEntity(pFiringEntity) && pHitEntity && (bMeleeDamage || NetworkUtils::IsNetworkCloneOrMigrating(pHitEntity)))
		{
            // Don't send damage events for entities in different tutorial sessions
            CNetObjGame *hitNetObj = SafeCast(CNetObjGame, NetworkUtils::GetNetworkObjectFromEntity(pHitEntity));
            if(hitNetObj && hitNetObj->GetPlayerOwner() && hitNetObj->GetPlayerOwner()->IsInDifferentTutorialSession())
            {
				weaponDebugf3("CWeapon::SendFireMessage--(hitNetObj && hitNetObj->GetPlayerOwner() && hitNetObj->GetPlayerOwner()->IsInDifferentTutorialSession())--> continue");
                continue; //need to ensure we iterate the other uNumHitEntities
            }

			// Check whether we should send this damage event based on the friendly fire settings
			if(pFiringEntity->GetIsTypePed())
			{
				CPed* pFiringPed = static_cast<CPed*>(pFiringEntity);
				if(!pFiringPed->IsAllowedToDamageEntity(GetWeaponInfo(), pHitEntity) && !(damageFlags & CPedDamageCalculator::DF_MeleeDamage))
				{
					weaponDebugf3("CWeapon::SendFireMessage--(!pFiringPed->IsAllowedToDamageEntity(GetWeaponInfo(), pHitEntity) && !(damageFlags & CPedDamageCalculator::DF_MeleeDamage))--> continue");
					continue; //need to ensure we iterate the other uNumHitEntities
				}
			}

			float fUseDamage = fDamage;

			// If impacts hit the same entity multiple times then we have to force the damage passed down
			if(hitEntities[i].fSumDamage > 0.0f)
			{
				bUseDamageOverride = true;
				fUseDamage = hitEntities[i].fSumDamage;
			}
			else
			{
				fUseDamage = CWeaponDamage::ApplyFallOffDamageModifier(pHitEntity, GetWeaponInfo(), vWeaponPos, fFallOffRangeModifier, fFallOffDamageModifier, fUseDamage, pFiringEntity);
			}

			// don't bother sending weapon damage events for doors and ambient props
			if (pHitEntity->GetIsTypeObject())
			{
				CObject* pObject = static_cast<CObject*>(pHitEntity);

				if( pObject->IsADoor() || ( pObject->GetOwnedBy() != ENTITY_OWNEDBY_SCRIPT && !pObject->GetIsParachute() ) )
				{
					weaponDebugf3("CWeapon::SendFireMessage--pObject->IsADoor() || ( pObject->GetOwnedBy() != ENTITY_OWNEDBY_SCRIPT && !pObject->GetIsParachute() )--> continue");
					continue; //need to ensure we iterate the other uNumHitEntities
				}
			}

			// don't bother sending weapon damage events for dead peds
			if (pHitEntity->GetIsTypePed())
			{
				CPed* pPed = static_cast<CPed*>(pHitEntity);

				if (pPed->GetIsDeadOrDying())
				{
					weaponDebugf3("CWeapon::SendFireMessage--pPed->GetIsDeadOrDying()--> continue");
					continue; //need to ensure we iterate the other uNumHitEntities
				}
			}

			// Add the head shot flag, if recoil is ok
			u32 damageFlagsToSend = damageFlags;
			if(pFiringEntity && pHitEntity && pHitEntity->GetIsTypePed())
			{
				float fDist = VEC3V_TO_VECTOR3(pFiringEntity->GetTransform().GetPosition()).Dist(hitEntities[i].vHitPosition);
				if(GetWeaponInfo()->GetDoesRecoilAccuracyAllowHeadShotModifier(static_cast<const CPed*>(pHitEntity), fRecoilAccuracyWhenFired, fDist))
				{
					damageFlagsToSend |= CPedDamageCalculator::DF_AllowHeadShot;
				}
			}

			bool firstBullet = false;
			if( pFiringEntity && pFiringEntity->GetIsTypePed() )
			{
				CPed* pFiringPed = static_cast<CPed*>(pFiringEntity);
				if (pFiringPed && pFiringPed->IsPlayer())
				{
					CTaskAimGun* pAimGunTask = static_cast<CTaskAimGun*>(pFiringPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_ON_FOOT));
					if(!pAimGunTask)
					{
						pAimGunTask = static_cast<CTaskAimGun*>(pFiringPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_VEHICLE_DRIVE_BY));
					}

					if(!pAimGunTask)
					{
						pAimGunTask = static_cast<CTaskAimGun*>(pFiringPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_BLIND_FIRE));
					}

					if(pAimGunTask)
					{
						firstBullet = (pAimGunTask->GetFireCounter() == 1);
					}
				}
			}

#if __ASSERT					
			//Trying to track down the cause of B* 2021715
			if (pHitEntity->GetIsTypePed())
			{
				s32 iHitComponent = hitEntities[i].iHitComponent;
				weaponAssertf(iHitComponent>=0 && iHitComponent < HIGH_RAGDOLL_LOD_NUM_COMPONENTS, "CWeapon::SendFireMessage - Ped has invalid hit component (%d)", iHitComponent);
			}
#endif //__ASSERT

			CWeaponDamageEvent::Trigger(pFiringEntity, pHitEntity, hitEntities[i].vHitPosition, hitEntities[i].iHitComponent, bUseDamageOverride, GetWeaponHash(), fUseDamage, hitEntities[i].iTyreIndex, hitEntities[i].iSuspensionIndex, damageFlagsToSend, actionResultId, meleeId, nForcedReactionDefinitionID, hitEntities[i].bHitEntityWeapon, hitEntities[i].bHitWeaponAttachment, m_bSilenced, firstBullet, forceDir ? forceDir : 0);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

struct WeaponShapeResultsSplitFunctor
{
	WeaponShapeResultsSplitFunctor(int const targetIdx)
	:
		m_targetIdx(targetIdx)
	{};

	bool operator()(const WorldProbe::CShapeTestHitPoint& UNUSED_PARAM(result), int const index) const
	{
		if(index == m_targetIdx)
		{
			return true;
		}

		return false;
	};

	int m_targetIdx;
};

////////////////////////////////////////////////////////////////////////////////

bool CWeapon::ReceiveFireMessage(CEntity* pFiringEntity, CEntity* pHitEntity, const u32 uWeaponHash, const f32 fDamage, const Vector3& vWorldHitPos, const s32 iComponent, const s32 iTyreIndex, const s32 iSuspensionIndex, const u32 damageFlags, const u32 actionResultId, const u16 meleeId, const u32 nForcedReactionDefinitionID, CWeaponDamage::NetworkWeaponImpactInfo * const networkWeaponImpactInfo /* = NULL */, const bool silenced /* = false */, const Vector3* forceDirection /* = NULL */, u8 damageAggregationCount /*= 0*/)
{
	weaponDebugf3("CWeapon::ReceiveFireMessage pFiringEntity[%p] pHitEntity[%p] uWeaponHash[%u] fDamage[%f] vWorldHitPos[%f %f %f] iComponent[%d] iTyreIndex[%d] iSuspensionIndex[%d] damageFlags[%u] actionResultId[%u] meleeId[%u] nForcedReactionDefinitionID[%u] networkWeaponImpactInfo[%p] silenced[%d]",pFiringEntity,pHitEntity,uWeaponHash,fDamage,vWorldHitPos.x,vWorldHitPos.y,vWorldHitPos.z,iComponent,iTyreIndex,iSuspensionIndex,damageFlags,actionResultId,meleeId,nForcedReactionDefinitionID,networkWeaponImpactInfo,silenced);

	weaponAssert(pFiringEntity);

	if(!pFiringEntity || !pHitEntity)
	{
		weaponDebugf3("Rejected: !pFiringEntity || !pHitEntity");
		return false;
	}

	CPhysical* pFiringPhysical = SafeCast(CPhysical, pFiringEntity);
	if(pFiringPhysical && pFiringPhysical->GetNetworkObject())
	{
		CNetObjPhysical* netFiringPhysical = SafeCast(CNetObjPhysical, pFiringPhysical->GetNetworkObject());
		if(netFiringPhysical)
		{
			if(!netFiringPhysical->IsLocalEntityVisibleOverNetwork() && !netFiringPhysical->IsLocalEntityCollidableOverNetwork())
			{
				weaponDebugf3("Rejected: !pFiringEntity->GetIsVisible() && !pFiringEntity->IsCollisionEnabled()");
				return false;
			}
		}
		else
		{
			if(!pFiringEntity->GetIsVisible() && !pFiringEntity->IsCollisionEnabled())
			{
				weaponDebugf3("Rejected: !pFiringEntity->GetIsVisible() && !pFiringEntity->IsCollisionEnabled()");
				return false;
			}
		}
	}
	else
	{
		if(!pFiringEntity->GetIsVisible() && !pFiringEntity->IsCollisionEnabled())
		{
			weaponDebugf3("Rejected: !pFiringEntity->GetIsVisible() && !pFiringEntity->IsCollisionEnabled()");
			return false;
		}
	}

	// Can't apply weapon damage to entities not in the physics level
	if(!pHitEntity->GetCurrentPhysicsInst() || (pHitEntity->GetCurrentPhysicsInst()->GetLevelIndex() == phInst::INVALID_INDEX))
	{
		weaponDebugf3("Rejected: !pHitEntity->GetCurrentPhysicsInst() || (pHitEntity->GetCurrentPhysicsInst()->GetLevelIndex() == phInst::INVALID_INDEX");
		return false;
	}

	// Get the weapon info
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(uWeaponHash);

	static dev_float MULTIPLAYER_WEAPON_RANGE = 80.0f;
	f32 fWeaponRange = MAX(MULTIPLAYER_WEAPON_RANGE, pWeaponInfo ? pWeaponInfo->GetRange() : 0.0f);

	Vec3V vFiringEntityPos = pFiringEntity->GetTransform().GetPosition();
	const CPed* pFiringPed = pFiringEntity->GetIsTypePed() ? static_cast<CPed*>(pFiringEntity) : nullptr;
	if (pFiringPed && pFiringPed->IsLocalPlayer() && pFiringPed->GetPedResetFlag(CPED_RESET_FLAG_UsingDrone))
	{
		CNetGamePlayer* localPlayer = NetworkInterface::GetLocalPlayer();
		if (localPlayer)
		{
			vFiringEntityPos = VECTOR3_TO_VEC3V(NetworkInterface::GetPlayerFocusPosition(*localPlayer));
		}
	}

	Vector3 vDeltaPos = VEC3V_TO_VECTOR3(vFiringEntityPos - pHitEntity->GetTransform().GetPosition());
	// Increase the range slightly to compensate for lag
	fWeaponRange *= 1.2f;

	bool bIgnoreDistCheck = (damageFlags & CPedDamageCalculator::DF_IgnoreRemoteDistCheck) != 0;

	// Don't accept weapon damage events from parent entities too far away
	if(!bIgnoreDistCheck && (vDeltaPos.Mag2() > (fWeaponRange * fWeaponRange)))
	{
		bool cameraCloseEnough = false;
		if(pFiringEntity->GetIsTypePed())
		{
			CPed* firingPed = SafeCast(CPed, pFiringEntity);
			if(firingPed->IsPlayer() && firingPed->GetNetworkObject())
			{
				CNetObjPlayer* netFirignPlayer = SafeCast(CNetObjPlayer, firingPed->GetNetworkObject());
				if(netFirignPlayer->IsUsingFreeCam())
				{
					vDeltaPos = VEC3V_TO_VECTOR3(netFirignPlayer->GetViewport()->GetCameraPosition() - pHitEntity->GetTransform().GetPosition());
					if(vDeltaPos.Mag2() < (fWeaponRange * fWeaponRange))
					{
						cameraCloseEnough = true;
					}
				}
			}
		}
		if(!cameraCloseEnough)
		{
			weaponDebugf3("Rejected: pFiringEntity too far away");
			return false;
		}
	}

	// Get weapon fire position
	Matrix34 weaponMatrix = MAT34V_TO_MATRIX34(pFiringEntity->GetMatrix());
	bool bDriveBy = false;
	if(pFiringEntity->GetIsTypePed())
	{
		CPed* pFiringPed = static_cast<CPed*>(pFiringEntity);
		const CPlayerInfo* pPlayerInfo = pFiringPed->GetPlayerInfo();
		if (pFiringPed->IsNetworkPlayer() && pPlayerInfo && pFiringPed->GetPedResetFlag(CPED_RESET_FLAG_UseScriptedWeaponFirePosition))
		{
			weaponMatrix.d = VEC3V_TO_VECTOR3(pPlayerInfo->GetScriptedWeaponFiringPos());
			weaponDebugf3("Changing weaponMatrix position due to PRF_UseScriptedWeaponFirePosition - using scripted firing pos <<%.2f,%.2f,%.2f>>", VEC3V_ARGS(pPlayerInfo->GetScriptedWeaponFiringPos()));
		}
		else
		{
			const s32 iBoneIndex = pFiringPed->GetBoneIndexFromBoneTag(BONETAG_R_HAND);
			if (iBoneIndex != BONETAG_INVALID)
			{
				pFiringPed->GetGlobalMtx(iBoneIndex, weaponMatrix);
			}

			if (pFiringPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle))
			{
				bDriveBy = true;
			}
		}
	}

    s32 iCappedComponent = iComponent;

	Vector3 vHitPos = vWorldHitPos;
	if(pHitEntity->GetIsTypePed())
	{
		CPed* pHitPed = static_cast<CPed*>(pHitEntity);
		weaponAssert(iComponent >= 0);
 
		if(pHitPed->GetRagdollInst() && pHitPed->GetRagdollInst()->GetCached())
        {
			Matrix34 ragdollCompMatrix;

			iCappedComponent = pHitPed->GetRagdollInst()->MapRagdollLODComponentHighToCurrent(iComponent);		
			if (weaponVerifyf(iCappedComponent != -1 && iCappedComponent < ((phBoundComposite*)pHitPed->GetRagdollInst()->GetArchetype()->GetBound())->GetNumBounds(), "Ped (%s). Mapped ragdoll component [%d] out of range 0..[%d].  Original component [%d]", pHitPed->GetModelName(), iCappedComponent, ((phBoundComposite*)pHitPed->GetRagdollInst()->GetArchetype()->GetBound())->GetNumBounds()-1, iComponent))
			{
				//Player victim sends explicit local world pos that is converted to vWorldHitPos for above set, player vs ped and ped vs ped don't send the hit pos and rely on the component hit position - less accurate but all right for ped hits. lavalley
				if(!pHitPed->IsPlayer() && pHitPed->GetRagdollComponentMatrix(ragdollCompMatrix, iCappedComponent))
				{
					vHitPos = ragdollCompMatrix.d;
				}
			}
			else
			{
				return false;
			}
		}
	}

	// Create a temporary weapon, no need to pass a drawable in, we're not going to render this and 
	// certainly don't want to change the peds weapon. We only use the weapon to compute the muzzle position
	// which will get set the weaponMatrix.d because m_bValidMuzzleMatrix gets initialised to false in the CWeapon constructor
	CWeapon tempWeapon(uWeaponHash, 1, NULL);
	tempWeapon.MarkAsTemporaryNetworkWeapon();
	tempWeapon.SetIsSilenced(silenced);

	// Get muzzle position
	Vector3 vWeaponFirePos;
	tempWeapon.GetMuzzlePosition(weaponMatrix, vWeaponFirePos);

	// Create a firing line that starts very close to the impact point so that it does not intersect anything else - 
	// this is to create the correct particle effects, decals etc for the damage
	Vector3 vFireVect = vHitPos - vWeaponFirePos;
	vFireVect.Normalize();

	Vector3 vIntersectionStart = vHitPos - vFireVect * 50.0f;
	Vector3 vIntersectionEnd = vHitPos + vFireVect * 50.0f;

	WorldProbe::CShapeTestProbeDesc probeDesc;
	WorldProbe::CShapeTestResults* probe_results = NULL;
	WorldProbe::CShapeTestFixedResults<1> probeResult;
	WorldProbe::CShapeTestFixedResults<5> probeResults;

	if(!bDriveBy)
	{
		probe_results = &probeResult;
	}
	else
	{		
		// B* 808748 - If the firing ped is in a car and shooting the front window, 
		// the length of raytest here means we can shoot the back of the car instead.
		// For drive bys we get all intersections and pick the nearest one to our hit pos.
		probe_results = &probeResults;
	}

	probeDesc.SetResultsStructure(probe_results);
	probeDesc.SetStartAndEnd(vIntersectionStart, vIntersectionEnd);
	probeDesc.SetDomainForTest(WorldProbe::TEST_AGAINST_INDIVIDUAL_OBJECTS);

	//Include the same tests that bullet usees
	int nLosOptions = 0;
	nLosOptions |= WorldProbe::LOS_IGNORE_SHOOT_THRU;
	nLosOptions |= WorldProbe::LOS_IGNORE_POLY_SHOOT_THRU;
	nLosOptions |= WorldProbe::LOS_TEST_VEH_TYRES;

	probeDesc.SetOptions(nLosOptions);

	probeDesc.SetTypeFlags(ArchetypeFlags::GTA_WEAPON_TEST);

	//Test against only the ragdoll or frag
	phInst* pHitInst = pHitEntity->GetFragInst();
	if(!pHitInst) pHitInst = pHitEntity->GetCurrentPhysicsInst();
	
	probeDesc.SetIncludeInstance(pHitInst);

	bool bSuccess = WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc);
	if(!bSuccess)
	{
		Vector3 vCollisionCentre(VEC3V_TO_VECTOR3(pHitEntity->GetTransform().GetPosition()));
		if(pHitEntity->GetCurrentPhysicsInst())
		{
			vCollisionCentre.Set(VEC3V_TO_VECTOR3(pHitEntity->GetCurrentPhysicsInst()->GetMatrix().GetCol3()));
		}

		// Move from the expected world hit pos towards the centre of the entity and test again
		for(s32 i = 0; i < 5; i++)
		{
			Vector3 vTestPos(vHitPos);
			vTestPos.Add(vCollisionCentre - vHitPos * (i / 4.0f));

			vIntersectionStart = vTestPos - vFireVect * 50.0f;
			vIntersectionEnd = vTestPos + vFireVect * 50.0f;

			probe_results->Reset();
			probeDesc.SetStartAndEnd(vIntersectionStart, vIntersectionEnd);
			bSuccess = WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc);

			if(bSuccess)
			{
				break;
			}
		}
	}

	//---

	weaponDebugf3("bSuccess[%d]",bSuccess);

	if(bSuccess && bDriveBy)
	{
		int nearestIdx	= -1;
		int count		= 0;
		float minDistSq	= FLT_MAX;

		for(WorldProbe::ResultIterator it = probe_results->begin(); it < probe_results->last_result(); ++it)
		{
			Vector3 intersectionPos = it->GetHitPosition();
			float distFromHitPosSq = intersectionPos.Dist2(vHitPos);

			if(distFromHitPosSq < minDistSq)
			{
				minDistSq = distFromHitPosSq;
				nearestIdx = count;
			}

			++count;
		}		

#if __BANK

		if(nearestIdx == -1 || nearestIdx >= probe_results->GetNumHits())
		{
			Displayf("pFiringEntity = %p\npHitEntity = %p\nuWeaponHash = %d\nfDamage = %f\nvWorldHitPos = %f %f %f\niComponent = %d\niTyreIndex = %d\niSuspensionIndex = %d\ndamageFlags = %d\nactionResultId = %d\n",
			pFiringEntity, 
			pHitEntity, 
			uWeaponHash, 
			fDamage, 
			vWorldHitPos.x, vWorldHitPos.y, vWorldHitPos.z,
			iComponent, 
			iTyreIndex, 
			iSuspensionIndex,
			damageFlags, 
			actionResultId);

			Displayf("%s : nearestIdx = %d : probe_results->GetNumHits( ) = %d", __FUNCTION__, nearestIdx, probe_results->GetNumHits());

			Vector3 start	= probeDesc.GetStart();
			Vector3 end		= probeDesc.GetEnd();
			Displayf("probeDesc start %f %f %f : end %f %f %f", start.x, start.y, start.z, end.x, end.y, end.z);

			int count = 0;
			for(WorldProbe::ResultIterator it = probe_results->begin(); it < probe_results->last_result(); ++it)
			{
				Vector3 pos				= it->GetHitPosition();
				CEntity const* entity	= it->GetHitEntity();
				Displayf("%d : it->GetHitPosition() = %f %f %f : hitEntity = %p %s", count++, pos.x, pos.y, pos.z, entity, entity ? entity->GetModelName() : "null entity");
			}
		}

#endif /* __BANK */

		Assert(nearestIdx != -1 && nearestIdx < probe_results->GetNumHits());
		//Assert(minDistSq < 0.5f);

		WorldProbe::CShapeTestFixedResults<5> discardResults;
		WorldProbe::CShapeTestFixedResults<1> finalResult;
	
		// split the discarded results off from the result we want to use...
		WeaponShapeResultsSplitFunctor functor(nearestIdx);
		probe_results->SplitHits<WeaponShapeResultsSplitFunctor>(functor, finalResult, discardResults);
		probe_results = &finalResult;
	}

	//---

	if(bSuccess)
	{
		if(iCappedComponent >= 0 && iCappedComponent < 65535)
		{
			// Need to set the component
			(*probe_results)[0].SetHitComponent((u16)iCappedComponent);
		}

		if(pFiringEntity != NULL && (damageFlags & CPedDamageCalculator::DF_MeleeDamage) != 0)
		{
			// If we used a custom pBound generated from the motion of the bones / weapon
			// then we can use that information to get a normal based on the motion of the strike.
			Vector3 vImpactNormal((*probe_results)[0].GetHitNormal());
			Vector3 vPedForward(VEC3V_TO_VECTOR3(pFiringEntity->GetTransform().GetB()));
			if(pHitEntity->GetIsTypePed())
			{
				// check we're not going to apply an force back towards the attacking ped
				static bank_float sfCosMaxImpactAngle = Cosf(70.0f * DtoR);
				if(DotProduct(vPedForward, vImpactNormal) < sfCosMaxImpactAngle)
				{
					vImpactNormal = vPedForward;
				}
			}
			else if(vImpactNormal.Dot(vPedForward) < 0.0f)
			{
				vImpactNormal = -vImpactNormal;
			}

			(*probe_results)[0].SetHitNormal(vImpactNormal);
		}

		CWeaponDamage::sMeleeInfo meleeInfo;
		meleeInfo.uParentMeleeActionResultID = actionResultId;
		meleeInfo.uNetworkActionID = meleeId;
		meleeInfo.uForcedReactionDefinitionID = nForcedReactionDefinitionID;

		// We might not want to do the same damage to objects and vehicles as we do to peds
		CWeaponDamage::sDamageModifiers damModifiers;
		// Scale damage caused to vehicles from melee attacks down a bit.
		if(tempWeapon.GetWeaponInfo()->GetIsMelee())
		{
			damModifiers.fVehDamageMult = CWeaponDamage::MELEE_VEHICLE_DAMAGE_MODIFIER;
		}

		float fWeaponImpactDamage = CWeaponDamage::DoWeaponImpact(&tempWeapon, pFiringEntity, weaponMatrix.d, *probe_results, fDamage, damageFlags, bDriveBy, tempWeapon.m_bTemporaryNetworkWeapon, &damModifiers, &meleeInfo, false, networkWeaponImpactInfo, 1.f, 1.f, 1.f, true, true, 0, forceDirection, damageAggregationCount);
		if (fWeaponImpactDamage)
		{
			// Apply the tyre damage if it was hit
			CVehicle* pHitVehicle = pHitEntity->GetIsTypeVehicle() ? static_cast<CVehicle*>(pHitEntity) : NULL;
			if(pHitVehicle)
			{
				if(iTyreIndex != -1)
				{
					CWheel* pWheel = pHitVehicle->GetWheel(iTyreIndex);
					if(weaponVerifyf(pWheel, "pWheel with index [%d] is NULL", iTyreIndex))
					{
						// Don't apply the stat update (pass NULL as damage entity) as that's taken care of in NetObjVehicle::SetVehicleHealth
						pWheel->ApplyTyreDamage(NULL, fDamage, (*probe_results)[0].GetHitPosition(), (*probe_results)[0].GetHitNormal(), DAMAGE_TYPE_BULLET, iTyreIndex);

						// Immediately replicate the health when tires are damaged to get the health and tire state out to the remotes so tires explode as fast as possible while still invoking from owner - lavalley
						netObject* pVehicleNetworkObject = pHitVehicle->GetNetworkObject();
						if ( pVehicleNetworkObject && !pVehicleNetworkObject->IsClone() && pVehicleNetworkObject->GetSyncData() && pVehicleNetworkObject->GetSyncTree())
						{
							pVehicleNetworkObject->GetSyncTree()->Update(pVehicleNetworkObject, pVehicleNetworkObject->GetActivationFlags(), netInterface::GetSynchronisationTime());
							pVehicleNetworkObject->GetSyncTree()->ForceSendOfNodeData(SERIALISEMODE_FORCE_SEND_OF_DIRTY, pVehicleNetworkObject->GetActivationFlags(), pVehicleNetworkObject, *SafeCast(CVehicleSyncTree, pVehicleNetworkObject->GetSyncTree())->GetVehicleHealthNode());
						}
					}
				}

				if(iSuspensionIndex != -1)
				{
					CWheel* pWheel = pHitVehicle->GetWheel(iSuspensionIndex);
					if(weaponVerifyf(pWheel, "pWheel with index %d is NULL", iSuspensionIndex))
					{
						pWheel->ApplySuspensionDamage(fDamage);
					}
				}
			}
		}
	}
	else
	{
#if __ASSERT
		bool bMeleeDamage = ((damageFlags & CPedDamageCalculator::DF_MeleeDamage) != 0);
		bool bPlayer = pHitEntity && pHitEntity->GetIsTypePed() && static_cast<CPed*>(pHitEntity)->IsPlayer();
		if(!bMeleeDamage && bPlayer)
		{
			// In some cases this can happen for weapon damages. For example when ragdolling, the player's position and animation desyncs and that will cause the probe to fail. 
			weaponWarningf("Shot missed a player!"); //ignore this if Melee damage - some smaller melee weapons can possibly miss the mark on remote players (knuckduster) - melee is also sent to other players rather than just the impact victim.
		}
#endif //__ASSERT
	}

	return bSuccess;
}

////////////////////////////////////////////////////////////////////////////////

void CWeapon::UpdateShaderVariables(u8 uTintIndex)
{
	weaponitemDebugf3("CWeapon::UpdateShaderVariables pWeapon[%p] uTintIndex[%u]",this,uTintIndex);

	m_uTintIndex = uTintIndex;

	UpdateShaderVariablesForDrawable(m_pDrawableEntity.Get(), m_uTintIndex);

	for(s32 i = 0; i < m_Components.GetCount(); i++)
	{
		if (m_Components[i]->GetInfo()->GetApplyWeaponTint())
		{
			UpdateShaderVariablesForDrawable(m_Components[i]->GetDrawable(), m_uTintIndex);
		}
	}
}

void CWeapon::UpdateShaderVariablesForDrawable(CDynamicEntity* pDrawableEntity, u8 uTintIndex)
{
	if(pDrawableEntity)
	{
		weaponitemDebugf3("CWeapon::UpdateShaderVariablesForDrawable pDrawableEntity->SetTintIndex");

		pDrawableEntity->SetTintIndex(uTintIndex);

		fwDrawData *drawHandler = pDrawableEntity->GetDrawHandlerPtr();
		if(drawHandler)
		{
			weaponitemDebugf3("CWeapon::UpdateShaderVariablesForDrawable drawHandler");

			fwCustomShaderEffect* pShader = drawHandler->GetShaderEffect();
			if( pShader )
			{
				u32 type = pShader->GetType();

				weaponitemDebugf3("CWeapon::UpdateShaderVariablesForDrawable GetDrawHandlerPtr pShader[%p] pShader->GetType[%d]",pShader,pShader ? pShader->GetType() : -9);

				switch(type)
				{
				case CSE_WEAPON:
					{
						weaponitemDebugf3("CWeapon::UpdateShaderVariablesForDrawable CSE_WEAPON");

						const CWeaponTintSpecValues::CWeaponSpecValue* pSpecValues = GetWeaponInfo()->GetSpecValues(uTintIndex);
						if(pSpecValues)
						{
							CCustomShaderEffectWeapon* pWeaponShader = static_cast<CCustomShaderEffectWeapon*>(pShader);
							pWeaponShader->SetSpecFalloffMult(pSpecValues->SpecFalloffMult);
							pWeaponShader->SetSpecIntMult(pSpecValues->SpecIntMult);
							pWeaponShader->SetSpecFresnel(pSpecValues->SpecFresnel);
							pWeaponShader->SetSpec2Factor(pSpecValues->Spec2Factor);
							pWeaponShader->SetSpec2ColorInt(pSpecValues->Spec2ColorInt);
							pWeaponShader->SetSpec2Color(pSpecValues->Spec2Color);

							weaponitemDebugf3("CWeapon::UpdateShaderVariablesForDrawable CSE_WEAPON pSpecValues m_uTintIndex[%u] pWeaponShader->GetMaxTintPalette()[%u]",m_uTintIndex,pWeaponShader->GetMaxTintPalette());

							if(uTintIndex < pWeaponShader->GetMaxTintPalette())
							{
								weaponitemDebugf3("CWeapon::UpdateShaderVariablesForDrawable pWeaponShader->SelectTintPalette");
								pWeaponShader->SelectTintPalette(uTintIndex, pDrawableEntity);
							}

							pWeaponShader->SetDiffusePalette(uTintIndex);
						}
					}
					break;
				case CSE_TINT:
					{
						CCustomShaderEffectTint* pTintShader = static_cast<CCustomShaderEffectTint*>(pShader);

						weaponitemDebugf3("CWeapon::UpdateShaderVariablesForDrawable CSE_TINT m_uTintIndex[%u] pTintShader->GetMaxTintPalette()[%u]",m_uTintIndex,pTintShader->GetMaxTintPalette());

						if(uTintIndex < pTintShader->GetMaxTintPalette())
						{
							weaponitemDebugf3("CWeapon::UpdateShaderVariablesForDrawable pTintShader->SelectTintPalette");
							pTintShader->SelectTintPalette(uTintIndex, pDrawableEntity);
						}
					}
					break;
				default:
					/* NoOp */
					break;
				}
			}
		}
	}
}

u32 CWeapon::GetCamoDiffuseTexIdx() const
{
	if(m_pDrawableEntity)
	{
		fwDrawData* drawHandler = m_pDrawableEntity->GetDrawHandlerPtr();
		if(drawHandler)
		{
			fwCustomShaderEffect* pShader = drawHandler->GetShaderEffect();
			if(pShader)
			{
				u32 type = pShader->GetType();
				switch(type)
				{
					case CSE_WEAPON:
					{
						CCustomShaderEffectWeapon* pWeaponShader = static_cast<CCustomShaderEffectWeapon*>(pShader);
						return pWeaponShader->GetCamoDiffuseTexIdx();
					}
					default:
					{
						break;
					}
				}
			}
		}
	}

	return 0;
}

void CWeapon::SetCamoDiffuseTexIdx(u32 uIdx)
{
	if(m_pDrawableEntity)
	{
		fwDrawData* drawHandler = m_pDrawableEntity->GetDrawHandlerPtr();
		if(drawHandler)
		{
			fwCustomShaderEffect* pShader = drawHandler->GetShaderEffect();
			if(pShader)
			{
				u32 type = pShader->GetType();
				switch(type)
				{
					case CSE_WEAPON:
					{
						CCustomShaderEffectWeapon* pWeaponShader = static_cast<CCustomShaderEffectWeapon*>(pShader);
						pWeaponShader->SetCamoDiffuseTexIdx(uIdx);

						break;
					}
					default:
					{
						break;
					}
				}
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

#if __BANK
void CWeapon::RenderDebug(CEntity* pFiringEntity) const
{
	if(!m_pDebugData)
	{
		USE_DEBUG_MEMORY();
		m_pDebugData = rage_new sDebug;
	}

	if(m_pDrawableEntity)
	{
		Vector3 vEntityPos(VEC3V_TO_VECTOR3(m_pDrawableEntity->GetTransform().GetPosition()));

#if FPS_MODE_SUPPORTED
		if(pFiringEntity)
		{
			TUNE_GROUP_FLOAT(DEBUG_TEXT, WEAPON_DEFAULT_SIDE_OFFSET, 1.5f, 0.0f, 4.0f, 0.01f);
			TUNE_GROUP_FLOAT(DEBUG_TEXT, WEAPON_SCOPED_SIDE_OFFSET, 0.5f, 0.0f, 4.0f, 0.01f);
			TUNE_GROUP_FLOAT(DEBUG_TEXT, WEAPON_DEFAULT_UP_OFFSET, 0.5f, 0.0f, 4.0f, 0.01f);
			TUNE_GROUP_FLOAT(DEBUG_TEXT, WEAPON_SCOPED_UP_OFFSET, 0.3f, 0.0f, 4.0f, 0.01f);
			const bool bScoped = (pFiringEntity->GetIsTypePed() && static_cast<const CPed*>(pFiringEntity)->GetMotionData()->GetIsFPSScope());
			const float fSideOffset = bScoped ? WEAPON_SCOPED_SIDE_OFFSET : WEAPON_DEFAULT_SIDE_OFFSET;
			const float fUpOffset = bScoped ? WEAPON_SCOPED_UP_OFFSET : WEAPON_DEFAULT_UP_OFFSET;
			if(camInterface::GetDominantRenderedCamera() == (camBaseCamera*)camInterface::GetGameplayDirector().GetFirstPersonShooterCamera() &&
				pFiringEntity == ((camBaseCamera*)camInterface::GetGameplayDirector().GetFirstPersonShooterCamera())->GetAttachParent())
			{
				Matrix34 camMatrix = ((camBaseCamera*)camInterface::GetGameplayDirector().GetFirstPersonShooterCamera())->GetFrame().GetWorldMatrix();
				vEntityPos = camMatrix.d + camMatrix.b * 3.0f + camMatrix.c * fUpOffset - camMatrix.a * fSideOffset;
			}
			else if (camInterface::GetDominantRenderedCamera() == (camBaseCamera*)camInterface::GetGameplayDirector().GetFirstPersonVehicleCamera() &&
				pFiringEntity->GetIsTypePed() && static_cast<const CPed*>(pFiringEntity)->IsInFirstPersonVehicleCamera()) 
			{
				Matrix34 camMatrix = ((camBaseCamera*)camInterface::GetGameplayDirector().GetFirstPersonVehicleCamera())->GetFrame().GetWorldMatrix();
				vEntityPos = camMatrix.d + camMatrix.b * 3.0f + camMatrix.c * fUpOffset - camMatrix.a * fSideOffset;
			}
		}
#endif // FPS_MODE_SUPPORTED

		Vector3 vBonePos;
		GetMuzzlePosition(MAT34V_TO_MATRIX34(m_pDrawableEntity->GetMatrix()), vBonePos);
		grcDebugDraw::Sphere(vBonePos, 0.05f, Color_blue);

		char buff[64];
		sprintf(buff, "State: %s", GetStateName(GetState()));
		grcDebugDraw::Text(vEntityPos, Color_blue, 0, 0, buff);

		sprintf(buff, "Timer: %d, NextShotAllowedTime: %d", m_uTimer, m_uNextShotAllowedTime);
		grcDebugDraw::Text(vEntityPos, Color_blue, 0, 10, buff);

		if(m_pDebugData)
		{
			sprintf(buff, "Firing Rate: %.2f", m_pDebugData->m_fFiringRate);
			grcDebugDraw::Text(vEntityPos, Color_blue, 0, 20, buff);

			if(m_pDebugData->m_iAccuracyShotsFired > 0)
			{
				sprintf(buff, "Shots hit: %d(%.2f)", m_pDebugData->m_iAccuracyShotsHit, (f32)m_pDebugData->m_iAccuracyShotsHit / (f32)m_pDebugData->m_iAccuracyShotsFired);
				grcDebugDraw::Text(vEntityPos, Color_blue, 0, 30, buff);

				sprintf(buff, "Blanks: %d(%.2f)", m_pDebugData->m_iAccuracyBlanksFired, (f32)m_pDebugData->m_iAccuracyBlanksFired / (f32)m_pDebugData->m_iAccuracyShotsFired);
				grcDebugDraw::Text(vEntityPos, Color_blue, 0, 40, buff);

				sprintf(buff, "Accurate shots: %d(%.2f)", m_pDebugData->m_iAccuracyAccurateShots, (f32)m_pDebugData->m_iAccuracyAccurateShots / (f32)m_pDebugData->m_iAccuracyShotsFired);
				grcDebugDraw::Text(vEntityPos, Color_blue, 0, 50, buff);

				sprintf(buff, "Restricted shots: %d(%.2f)", m_pDebugData->m_iAccuracyRestrictedShots, (f32)m_pDebugData->m_iAccuracyRestrictedShots / (f32)m_pDebugData->m_iAccuracyShotsFired);
				grcDebugDraw::Text(vEntityPos, Color_blue, 0, 60, buff);
			}
		}
	}

	for(s32 i = 0; i < m_Components.GetCount(); i++)
	{
		m_Components[i]->RenderDebug(pFiringEntity);
	}
}
#endif // __BANK

////////////////////////////////////////////////////////////////////////////////

void CWeapon::CreateMoveNetworkHelper()
{
	if(m_pDrawableEntity)
	{
		if(!m_pDrawableEntity->GetAnimDirector())
		{
			m_pDrawableEntity->CreateAnimDirector(*m_pDrawableEntity->GetDrawable());
		}

		if(weaponVerifyf(m_pDrawableEntity->GetAnimDirector(), "Failed to create an AnimDirector"))
		{
			if(weaponVerifyf(!m_pMoveNetworkHelper, "MoveNetworkHelper already exists!"))
			{
				m_pMoveNetworkHelper = rage_new CMoveNetworkHelper;
			}

			if(weaponVerifyf(m_pMoveNetworkHelper, "Failed to create a MoveNetworkHelper!"))
			{
				if(!m_pMoveNetworkHelper->IsNetworkActive())
				{
					m_pMoveNetworkHelper->RequestNetworkDef(CClipNetworkMoveInfo::ms_NetworkWeapon);
					if(m_pMoveNetworkHelper->IsNetworkDefStreamedIn(CClipNetworkMoveInfo::ms_NetworkWeapon))
					{
						CObject* pDrawableObject = static_cast<CObject*>(m_pDrawableEntity.Get());
						m_pMoveNetworkHelper->CreateNetworkPlayer(pDrawableObject, CClipNetworkMoveInfo::ms_NetworkWeapon);
						pDrawableObject->GetMoveObject().SetNetwork(m_pMoveNetworkHelper->GetNetworkPlayer(), INSTANT_BLEND_DURATION);
					}
				}
			}
		}
	}
}

void CWeapon::CreateFPSMoveNetworkHelper()
{
	if(m_pDrawableEntity)
	{
		if(!m_pDrawableEntity->GetAnimDirector())
		{
			m_pDrawableEntity->CreateAnimDirector(*m_pDrawableEntity->GetDrawable());
		}

		if(weaponVerifyf(m_pDrawableEntity->GetAnimDirector(), "Failed to create an AnimDirector"))
		{
			if(weaponVerifyf(!m_pFPSMoveNetworkHelper, "FPS MoveNetworkHelper already exists!"))
			{
				m_pFPSMoveNetworkHelper = rage_new CMoveNetworkHelper;
			}

			if(weaponVerifyf(m_pFPSMoveNetworkHelper, "Failed to create a FPS MoveNetworkHelper!"))
			{
				if(!m_pFPSMoveNetworkHelper->IsNetworkActive())
				{
					CreateFPSWeaponNetworkPlayer();
				}
			}
		}
	}
}

void CWeapon::CreateFPSWeaponNetworkPlayer()
{
	if(m_pFPSMoveNetworkHelper)
	{
		m_pFPSMoveNetworkHelper->RequestNetworkDef(CClipNetworkMoveInfo::ms_NetworkProjectileFPS);
		if(m_pFPSMoveNetworkHelper->IsNetworkDefStreamedIn(CClipNetworkMoveInfo::ms_NetworkProjectileFPS))
		{
			CObject* pDrawableObject = static_cast<CObject*>(m_pDrawableEntity.Get());
			m_pFPSMoveNetworkHelper->CreateNetworkPlayer(pDrawableObject, CClipNetworkMoveInfo::ms_NetworkProjectileFPS);
			pDrawableObject->GetMoveObject().SetNetwork(m_pFPSMoveNetworkHelper->GetNetworkPlayer(), NORMAL_BLEND_DURATION);
		}
	}
}


////////////////////////////////////////////////////////////////////////////////

void CWeapon::CreateWeaponIdleAnim(const fwMvClipSetId& ClipSetId, bool bInVehicle)
{
	if(ClipSetId != CLIP_SET_ID_INVALID)
	{
		fwClipSet* pClipSet = fwClipSetManager::GetClipSet(ClipSetId);
		if(pClipSet)
		{
			bool bCreatedNetworkHelper = false;

			const atArray<atHashString>& moveFlags = pClipSet->GetMoveNetworkFlags();
			for(s32 i = 0; i < moveFlags.GetCount(); i++)
			{
				m_bHasWeaponIdleAnim = moveFlags[i] == ms_WeaponIdleFlagId;
				m_bHasInVehicleWeaponIdleAnim = (bInVehicle && moveFlags[i] == ms_WeaponInVehicleIdleFlagId);

				if(!bCreatedNetworkHelper && (m_bHasWeaponIdleAnim || m_bHasInVehicleWeaponIdleAnim) )
				{
					CreateMoveNetworkHelper();

					bCreatedNetworkHelper = true;
			
					if(m_pMoveNetworkHelper && m_pMoveNetworkHelper->IsNetworkActive())
					{
						if(pClipSet->Request_DEPRECATED())
						{
							// B*1903074 - Disable filter for musket
							if (m_pWeaponInfo && m_pWeaponInfo->GetIsIdleAnimationFilterDisabled())
							{
								m_pMoveNetworkHelper->SetFlag(true, ms_DisableIdleFilterFlagId);
							}
							// Set the clip
							m_pMoveNetworkHelper->SetClipSet(ClipSetId);
						}

						if(m_pDrawableEntity->GetIsTypeObject())
						{
							//Force an anim update
							CObject* pDrawableObject = static_cast<CObject*>(m_pDrawableEntity.Get());
							pDrawableObject->ForcePostCameraAnimUpdate(true, true);
						}
					}
				}
			}		
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CWeapon::ProcessState(CEntity* pFiringEntity, u32 uTimeInMilliseconds)
{
	// Store the current time we are testing against
	m_uGlobalTime = uTimeInMilliseconds;

	u8 oldState = m_State;
	bool bUpdate = true;
	while(bUpdate)
	{
		switch(m_State)
		{
		case STATE_READY:
			StateReadyUpdate(pFiringEntity);
			break;

		case STATE_WAITING_TO_FIRE:
			StateWaitingToFireUpdate(pFiringEntity);
			break;

		case STATE_RELOADING:
			StateReloadingUpdate(pFiringEntity);
			break;

		case STATE_OUT_OF_AMMO:
			StateOutOfAmmoUpdate(pFiringEntity);
			break;

		case STATE_SPINNING_UP:
			StateSpinUp(pFiringEntity);
			break;

		case STATE_SPINNING_DOWN:
			StateSpinDown(pFiringEntity);
			break;

		default:
			break;
		}

		bUpdate = m_State != oldState;
		oldState = m_State;
	}
}

////////////////////////////////////////////////////////////////////////////////

void CWeapon::UpdateAnimationAfterFiring(CEntity* pFiringEntity, f32 fFireAnimRate)
{
	if(!m_pMoveNetworkHelper)
	{
		if(m_pDrawableEntity && m_pDrawableEntity->GetIsTypeObject())
		{
			CreateMoveNetworkHelper();

			if(m_pMoveNetworkHelper && m_pMoveNetworkHelper->IsNetworkActive())
			{
				//Grab the firing ped.
				CPed* pFiringPed = NULL;
				if(pFiringEntity && pFiringEntity->GetIsTypePed())
				{
					pFiringPed = static_cast<CPed *>(pFiringEntity);
				}

				fwMvClipSetId ClipSetId = pFiringPed ? GetWeaponInfo()->GetAppropriateWeaponClipSetId(pFiringPed) : GetWeaponInfo()->GetWeaponClipSetId();
				if(ClipSetId != CLIP_SET_ID_INVALID && ClipSetId != m_pMoveNetworkHelper->GetClipSetId())
				{
					fwClipSet* pClipSet = fwClipSetManager::GetClipSet(ClipSetId);
					if(pClipSet && pClipSet->Request_DEPRECATED())
					{
						// Set the clip set
						m_pMoveNetworkHelper->SetClipSet(ClipSetId);
					}
				}
			}
		}
	}

	// Play a firing anim if one exists
	if(m_pMoveNetworkHelper && m_pMoveNetworkHelper->IsNetworkActive())
	{
		static const fwMvClipId s_Fire("w_fire",0xA3AE8610);
		const crClip* pClip = fwAnimManager::GetClipIfExistsBySetId(m_pMoveNetworkHelper->GetClipSetId(), s_Fire);
		if(pClip)
		{
			// If we've disabled the idle animation filter, re-enable it while we play the fire anim
			if (m_bHasWeaponIdleAnim && m_pWeaponInfo && m_pWeaponInfo->GetIsIdleAnimationFilterDisabled())
			{
				m_pMoveNetworkHelper->SetFlag(false, ms_WeaponIdleFlagId);
				if (m_pWeaponInfo->GetFlag(CWeaponInfoFlags::UseInstantAnimBlendFromIdleToFire))
				{
					//Instant blend out from idle to fire
					m_pMoveNetworkHelper->SetFloat(ms_WeaponIdleBlendDuration, INSTANT_BLEND_DURATION);
				}
			}

			bool bIsStunGunMP = m_pWeaponInfo && m_pWeaponInfo->GetHash() == ATSTRINGHASH("WEAPON_STUNGUN_MP", 0x45CD9CF3);
			float fFireStartPhase = 0.0f;
			// If the firing anim has some kind of gauge/dial that matches up with UI, scale playback rate to match it
			if(m_pWeaponInfo && m_pWeaponInfo->GetFireAnimRateScaledToTimeBetweenShots())
			{
				const float fFireClipLength = pClip->GetDuration();
				const float fFireTimerLength = m_pWeaponInfo->GetTimeBetweenShots();
				fFireAnimRate = fFireClipLength / fFireTimerLength;

				if(bIsStunGunMP)
				{
					const float fTimeLeftToFire = (static_cast<f32>(m_uTimer) - static_cast<f32>(fwTimer::GetTimeInMilliseconds())) / 1000.0f;
					if(fTimeLeftToFire >= 0.0f)
					{
						fFireStartPhase = (fFireTimerLength - fTimeLeftToFire) / fFireTimerLength;
					}
				}
			}

			static const fwMvRequestId s_FireRequestId("Fire",0xD30A036B);
			m_pMoveNetworkHelper->SendRequest(s_FireRequestId);

			if(bIsStunGunMP)
			{
				static const fwMvFloatId s_FirePase("Fire_Phase",0x6E8F556F);
				m_pMoveNetworkHelper->SetFloat(s_FirePase, fFireStartPhase);
			}

			static const fwMvFloatId s_FireRate("Fire_Rate",0x225D53C3);
			m_pMoveNetworkHelper->SetFloat(s_FireRate, fFireAnimRate);

			static const fwMvFlagId s_OutOfAmmo("OutOfAmmo",0x2DDE6C35);
			m_pMoveNetworkHelper->SetFlag(GetIsClipEmpty(), s_OutOfAmmo);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CWeapon::SetWeaponRechargeAnimation()
{
	UpdateAnimationAfterFiring(nullptr, 1.0f);
}

////////////////////////////////////////////////////////////////////////////////

void CWeapon::UpdateStateAfterFiring(CEntity* pFiringEntity)
{
	if(!GetIsClipEmpty())
	{
		SetState(STATE_WAITING_TO_FIRE);

		SetTimer(m_uGlobalTime + static_cast<u32>(1000.0f * GetWeaponInfo()->GetTimeBetweenShots()));
		m_uNextShotAllowedTime = m_uTimer;
	}
	else if(GetAmmoTotal() > 0 || GetAmmoTotal() == INFINITE_AMMO)
	{
		// If we have an animated reload, or we have a reload timer (so we will exit the reload state), reload
		if(GetWeaponInfo()->GetCanReload() || GetReloadTime(pFiringEntity) > 0.f)
		{
			StartReload(pFiringEntity);
		}
		else
		{
			DoReload();
			SetState(STATE_WAITING_TO_FIRE); 

			SetTimer(m_uGlobalTime + static_cast<u32>(1000.0f * GetWeaponInfo()->GetTimeBetweenShots()));
			m_uNextShotAllowedTime = m_uTimer;
		}
	}
	else
	{
		SetState(STATE_OUT_OF_AMMO);
	}

	// Cache off the state so we can initialise with this for recharging weapons
	if (m_pWeaponInfo && m_pWeaponInfo->GetHash() == ATSTRINGHASH("WEAPON_STUNGUN_MP", 0x45CD9CF3))
	{
		CPedInventory* pInventory = static_cast<CPed*>(pFiringEntity)->GetInventory();
		if(pInventory)
		{
			CWeaponItem* pWeaponItem = pInventory->GetWeapon(m_pWeaponInfo->GetHash());
			if(pWeaponItem)
			{
				pWeaponItem->SetCachedWeaponState(GetState());
			}
		}
	}

#if __BANK
	if(m_pDebugData)
	{
		if(m_State == STATE_RELOADING || m_State == STATE_OUT_OF_AMMO || (m_uGlobalTime - m_pDebugData->m_iLastShotTime) > static_cast<s32>(4000.f*m_pWeaponInfo->GetTimeBetweenShots()))
		{
			m_pDebugData->m_iFirstShotTime = -1;
			m_pDebugData->m_iShotsFired = 0;
		}

		if(m_State == STATE_WAITING_TO_FIRE)
		{
			if(m_pDebugData->m_iFirstShotTime == -1)
			{
				m_pDebugData->m_iFirstShotTime = m_uGlobalTime - static_cast<u32>(1000.0f * GetWeaponInfo()->GetTimeBetweenShots());
				m_pDebugData->m_iLastShotTime = m_pDebugData->m_iFirstShotTime;
				m_pDebugData->m_iShotsFired = 0;
			}

			m_pDebugData->m_iShotsFired++;
			f32 fTime = static_cast<f32>(m_uGlobalTime - m_pDebugData->m_iFirstShotTime);
			if(fTime > 0.f)
			{
				m_pDebugData->m_fFiringRate = (1000.f * static_cast<f32>(m_pDebugData->m_iShotsFired)) / fTime;
			}
		}
		m_pDebugData->m_iLastShotTime = m_uGlobalTime;
	}
#endif // __BANK
}

////////////////////////////////////////////////////////////////////////////////

void CWeapon::StateReadyUpdate(CEntity* UNUSED_PARAM(pFiringEntity))
{
	if(m_bNeedsToSpinDown)
	{
		if(m_uGlobalTime >= m_uTimer)
		{
			SetState(STATE_SPINNING_DOWN);
			SetTimer(m_uGlobalTime + static_cast<u32>(1000.0f * GetWeaponInfo()->GetSpinDownTime()));
		}
	}

	m_uNextShotAllowedTime = m_uGlobalTime;
}

////////////////////////////////////////////////////////////////////////////////

void CWeapon::StateWaitingToFireUpdate(CEntity* UNUSED_PARAM(pFiringEntity))
{
	if(m_uGlobalTime >= m_uTimer)
	{
		SetState(STATE_READY);

		SetTimer(m_uGlobalTime + static_cast<u32>(1000.0f * GetWeaponInfo()->GetSpinTime()));
	}
}

////////////////////////////////////////////////////////////////////////////////

void CWeapon::StateReloadingUpdate(CEntity* pFiringEntity)
{
	if(m_uTimer != 0)
	{
		if(m_uGlobalTime >= m_uTimer)
		{
			bool bPedInsideVehicle = false;

			// If we are reloading an RPG in a vehicle make sure we create 
			// the projectile before we actually update the weapon ammo data
			// See B*2122143
			if( pFiringEntity && pFiringEntity->GetIsTypePed() )
			{
				CPed* pFiringPed = static_cast<CPed*>(pFiringEntity);
				const CWeaponInfo* pWeaponInfo = GetWeaponInfo();
				bPedInsideVehicle = pFiringPed->GetIsInVehicle();

				if( pFiringPed->GetVehiclePedInside()	&& 
					pWeaponInfo							&& 
					pWeaponInfo->GetIsRpg()				&&
					m_pDrawableEntity					&&
					m_pDrawableEntity->GetIsTypeObject() )
				{
					CPedEquippedWeapon::CreateProjectileOrdnance( static_cast<CObject*>(m_pDrawableEntity.Get()), pFiringEntity );
				}
			}

			// If driveby, run reload as initial state to always get full ammo amount for looped animation
			DoReload(bPedInsideVehicle);
		}						
	}
}

////////////////////////////////////////////////////////////////////////////////

void CWeapon::StateOutOfAmmoUpdate(CEntity* UNUSED_PARAM(pFiringEntity))
{
	ProcessGunFeed(false);

	//Play the out of anim weapon anim.
	if(m_pWeaponInfo->GetPlayOutOfAmmoAnim() && !m_bOutOfAmmoAnimIsPlaying)
	{
		fwMvClipSetId weaponClipSetId = m_pWeaponInfo->GetWeaponClipSetId();
		if(weaponClipSetId != CLIP_SET_ID_INVALID)
		{
			static const fwMvClipId s_weaponEmptyClipId("w_empty",0x2FAB2155);
			if(fwAnimManager::GetClipIfExistsBySetId(weaponClipSetId, s_weaponEmptyClipId))
			{
				StartAnim(weaponClipSetId, s_weaponEmptyClipId, NORMAL_BLEND_DURATION, 1.0f, 0.0f, true);
				m_bOutOfAmmoAnimIsPlaying = true;

				// Disable idle anim while playing the empty anim.
				SetWeaponIdleFlag(false);
			}
			else
			{
				weaponAssertf(false, "Weapon %s doesn't have w_empty anim in clipset %s", m_pWeaponInfo->GetName(), weaponClipSetId.GetCStr());
			}
		}
	}

	if(GetAmmoTotal() != 0)
	{
		SetState(STATE_READY);

		if(m_bOutOfAmmoAnimIsPlaying)
		{
			StopAnim(INSTANT_BLEND_DURATION);
		
			// Reset idle flag
			SetWeaponIdleFlag(true);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CWeapon::StateSpinUp(CEntity* UNUSED_PARAM(pFiringEntity))
{
	if(m_uGlobalTime >= m_uTimer)
	{
		SetState(STATE_READY);
		SetTimer(m_uGlobalTime + static_cast<u32>(1000.0f * GetWeaponInfo()->GetSpinTime()));
		m_bNeedsToSpinDown = true;
	}
}

////////////////////////////////////////////////////////////////////////////////

void CWeapon::StateSpinDown(CEntity* UNUSED_PARAM(pFiringEntity))
{
	if(m_uGlobalTime >= m_uTimer)
	{
		SetState(STATE_READY);
		m_bNeedsToSpinDown = false;
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CWeapon::FireDelayedHit(const sFireParams& params, const Vector3& vStart, const Vector3& vEnd)
{
	bool bSetPerfectAccuracy					= params.iFireFlags.IsFlagSet(FF_SetPerfectAccuracy);
	bool bFireBlank								= params.iFireFlags.IsFlagSet(FF_FireBlank);
	const bool bAllowDamageToVehicle			= params.iFireFlags.IsFlagSet(FF_AllowDamageToVehicle);
	const bool bAllowDamageToVehicleOccupants	= params.iFireFlags.IsFlagSet(FF_AllowDamageToVehicleOccupants);
	const bool bForceBulletTrace				= params.iFireFlags.IsFlagSet(FF_ForceBulletTrace);
	const bool bForceNoBulletTrace				= params.iFireFlags.IsFlagSet(FF_ForceNoBulletTrace);
	const bool bBlindFire						= params.iFireFlags.IsFlagSet(FF_BlindFire);
	const bool bPassThroughBulletProofGlass		= params.iFireFlags.IsFlagSet(FF_PassThroughOwnVehicleBulletProofGlass);

	weaponDebugf3("CWeapon::FireDelayedHit params.pFiringEntity[%p][%s] bSetPerfectAccuracy[%d] bFireBlank[%d] bAllowDamageToVehicle[%d] bAllowDamageToVehicleOccupants[%d] bForceBulletTrace[%d] bForceNoBulletTrace[%d] bBlindFire[%d]",params.pFiringEntity,params.pFiringEntity ? params.pFiringEntity->GetModelName() : "",bSetPerfectAccuracy,bFireBlank,bAllowDamageToVehicle,bAllowDamageToVehicleOccupants,bForceBulletTrace,bForceNoBulletTrace,bBlindFire);

	s32 iMaxShots = 1;
	bool bShouldCheckSetRange = true;
	CPed* pPed = NULL;
	if( params.pFiringEntity && params.pFiringEntity->GetIsTypePed())
	{
		pPed = static_cast<CPed*>(params.pFiringEntity);
		if(pPed->IsPlayer())
		{
			static dev_s32 MAX_PLAYER_SHOTS = 2;
			iMaxShots = MAX_PLAYER_SHOTS;
		}
		bShouldCheckSetRange = !(pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON) || 
								 pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_RUN_AND_GUN) || 
								 !pPed->IsLocalPlayer());
		CPedIntelligence* pPedIntel = pPed->GetPedIntelligence();
		if (pPedIntel)
		{
			bSetPerfectAccuracy |= pPedIntel->GetCombatBehaviour().IsFlagSet(CCombatData::BF_PerfectAccuracy);
		}
	}

	// Adding a slight delay to the explosive ammo of the Pump Shotgun Mk II so the weapon isn't full hit-scan
	// TODO: Nerf the Heavy Sniper Mk II here as well. Pretty please?
	float fShotVelocityOverride = -1.0f;
	if (m_pClipComponent)
	{
		const u32 uClipAmmoInfoHash = m_pClipComponent->GetAmmoInfo();
		if (uClipAmmoInfoHash != 0)
		{
			const CAmmoInfo* pAmmoInfo = CWeaponInfoManager::GetInfo<CAmmoInfo>(uClipAmmoInfoHash);
			if (pAmmoInfo && pAmmoInfo->GetIsExplosive())
			{
				if (GetWeaponInfo()->GetGroup() == WEAPONGROUP_SHOTGUN)
				{
					fShotVelocityOverride = 550.0f;
				}
			}
		}
	}

	const float fShotVelocity = (fShotVelocityOverride > 0.0f) ? fShotVelocityOverride : GetWeaponInfo()->GetSpeed();
	float fRange = GetRange();
	if(m_pTargetingComponent && bShouldCheckSetRange)
	{
		if(!m_pTargetingComponent->GetIsRangeSet())
		{
			// Flag the component to calculate the range
			m_pTargetingComponent->SetRange();

			// If we have just set the range - as it is on the fire trigger, suppress firing for a frame
			SetState(STATE_WAITING_TO_FIRE);
			
			SetTimer(m_uGlobalTime);
			m_uNextShotAllowedTime = m_uTimer;
			return false;
		}
		else if(m_pTargetingComponent->GetIsRangeSet())
		{
			// Set the range to the targeting components and then clear it
			fRange = m_pTargetingComponent->GetRange();
			m_pTargetingComponent->ClearSetRange();
		}
	}
	else
	{
		// Allow the script to modify the weapon range
		TUNE_GROUP_FLOAT(AIMING_TUNE, EXTENDED_FIRE_DISTANCE, 250.0f, 0.0f, 999.0f, 1.0f);
		if( params.pFiringEntity && params.pFiringEntity->GetIsTypePed() )
		{
			if( static_cast<const CPed*>(params.pFiringEntity)->GetPedResetFlag(CPED_RESET_FLAG_ExtraLongWeaponRange) )
			{
				fRange = EXTENDED_FIRE_DISTANCE;
			}
		}	
	}

	const float fLifeTime = fRange / fShotVelocity;

	// Perform any bullet bending
	Vector3 vAdjustedEndAfterBulletBending = vEnd;
	const bool bBulletBent = ComputeBulletBending(params.pFiringEntity, vStart, vAdjustedEndAfterBulletBending, fRange);

	// Compute the damage
	float fDamage = Selectf(params.fApplyDamage, params.fApplyDamage, GetWeaponInfo()->GetDamage());
	for(s32 i = 0; i < m_Components.GetCount(); i++)
	{
		m_Components[i]->ModifyDamage(fDamage);
	}

	//HACK: Override AP Pistol's damage value via cloud tunable, in case we need to change it.
	if (GetWeaponHash() == ATSTRINGHASH("WEAPON_APPISTOL", 0x22d8fe39) && sm_fDamageOverrideForAPPistol > 0.0f)
	{
		fDamage = sm_fDamageOverrideForAPPistol;
	}

	// Loop through, starting at the next time we can shoot, until this exceeds our end time
	u32 uEndTime = fwTimer::GetTimeInMilliseconds() + fwTimer::GetTimeStepInMilliseconds();
	s32 iShotsFired = 0;

	eAnimBoneTag nForcedRandomHitBone = BONETAG_INVALID;
	if(CWanted::ShouldCompensateForSlowMovingVehicle(params.pFiringEntity, params.pTargetEntity))
	{
		bool bRandom = NetworkInterface::IsNetworkOpen() ?
			g_ReplayRand.GetBool() : fwRandom::GetRandomTrueFalse();
		nForcedRandomHitBone = bRandom ?
			BONETAG_HEAD : BONETAG_NECK;
	}

	while(m_uNextShotAllowedTime < uEndTime && (m_State == STATE_READY || m_State == STATE_WAITING_TO_FIRE) && iShotsFired < iMaxShots)
	{
		// Update the weapon state
		ProcessState(params.pFiringEntity, m_uNextShotAllowedTime);
		weaponAssertf(m_State == STATE_READY, "Should always be ready after ProcessState in this case");

		// Apply accuracy modifiers
		Vector3 vAdjustedEnd = vAdjustedEndAfterBulletBending;
		sWeaponAccuracy weaponAccuracy;
		if(ComputeAccuracy(params.pFiringEntity, params.pTargetEntity, RCC_VEC3V(vStart), RC_VEC3V(vAdjustedEnd), weaponAccuracy, bSetPerfectAccuracy, params.fDesiredTargetDistance, bBlindFire, nForcedRandomHitBone))
		{
			if(!NetworkInterface::IsGameInProgress() || !params.pFiringEntity || !params.pFiringEntity->GetIsDynamic() || !static_cast<const CDynamicEntity*>(params.pFiringEntity)->IsNetworkClone())
			{
				// Don't fire blanks if we are accurate, 
				bFireBlank = false;
			}
		}

		// B*4473189: There's a rare issue where vAdjustedEnd is being turned to garbage in ComputeAccuracy. 
		// Instead of spending hours debugging math, let's just sanity check and restore a valid pre-accuracy value instead of crashing when someone tries to do something with it.
		if (!weaponVerifyf(vAdjustedEnd.FiniteElements(), "CWeapon::FireDelayedHit - vAdjustedEnd is invalid, something went wrong in ComputeAccuracy!"))
		{
			vAdjustedEnd = vAdjustedEndAfterBulletBending;
		}

		if(!bFireBlank)
		{
			if(CBullet::GetPool()->GetNoOfFreeSpaces() > 0)
			{
				bool bCreateTraceVfx = false;
				if (bForceNoBulletTrace==false)
				{
					bCreateTraceVfx = bForceBulletTrace || ShouldDoBulletTrace(params.pFiringEntity);
				}

				Vector3 vMuzzlePosition,vMuzzlePositionEnd, vMuzzleDirection;
				CalcFireVecFromWeaponMatrix( params.weaponMatrix, vMuzzlePosition, vMuzzlePositionEnd );
				GetMuzzlePosition(params.weaponMatrix, vMuzzlePosition);
				vMuzzleDirection = vMuzzlePositionEnd - vMuzzlePosition;
				bool bUseBulletPenetration = GetUsesBulletPenetration(params.pFiringEntity);

				float fFallOffRangeModifier = 1.f;
				float fFallOffDamageModifier = 1.f;
				for(s32 i = 0; i < m_Components.GetCount(); i++)
				{
					m_Components[i]->ModifyFallOff(fFallOffRangeModifier, fFallOffDamageModifier);
				}

				CBullet* pBullet = rage_new CBullet(params.pFiringEntity, 
					GetWeaponInfo(), 
					vStart, 
					vAdjustedEnd, 
					fShotVelocity, fLifeTime, 
					fDamage, GetWeaponHash(), 
					bCreateTraceVfx, 
					bAllowDamageToVehicle, 
					bAllowDamageToVehicleOccupants, 
					vMuzzlePosition, 
					vMuzzleDirection, 
					GetIsAccurate(vAdjustedEndAfterBulletBending, vAdjustedEnd), 
					bUseBulletPenetration, 
					bPassThroughBulletProofGlass,
					weaponAccuracy.fAccuracyMin, 
					m_bSilenced, 
					fFallOffRangeModifier, 
					fFallOffDamageModifier,
					bBulletBent,
					this);

				CBulletManager::Add(pBullet);

				if(GetBulletsInBatch() > 1)
				{
					static dev_float WEAPON_PROBE_MULT_LENGTH_MULT = 0.4f;
					float fMultiShotLength = (vAdjustedEnd - vStart).Mag() * WEAPON_PROBE_MULT_LENGTH_MULT;
					float fMultiShotSpread = fMultiShotLength * GetWeaponInfo()->GetBatchSpread();

					Vector3 vecHalfWidth(VEC3_ZERO);
					vecHalfWidth.y = fMultiShotLength / 2.0f;

					Matrix34 matShot;
					matShot.b = vAdjustedEnd - vStart;
					matShot.b.Normalize();
					matShot.a.Cross(matShot.b, ZAXIS);
					matShot.a.Normalize();
					matShot.c.Cross(matShot.a, matShot.b);
					matShot.d = vStart + vecHalfWidth.y * matShot.b;

					Vector3 vecMultShotEnd = vStart + fMultiShotLength * matShot.b;

					// Get the number of bullets we need to fire (-1 for the one we've already added)
					u32 uBullets = GetBulletsInBatch() - 1;
					if(params.pFiringEntity && params.pFiringEntity->GetIsTypePed() && !static_cast<CPed*>(params.pFiringEntity)->IsAPlayerPed())
					{
						// Halve the number of bullets non players do
						uBullets >>= 1;

						// Increase spread of non player blind firing
						if (bBlindFire)
						{
							fMultiShotSpread *= ms_fBlindFireBatchSpreadModifier;
						}
					}

					static dev_u32 ZONES = 3;
					const f32 ONE_OVER_ZONES = 1.f / (float)ZONES;
					const u32 uBulletsPerZone = (u32)ceilf((float)uBullets / (float)ZONES);
					static dev_float SEGMENT_SIZE_MOD = 0.25f;
					float fMultiShotZoneSpread = fMultiShotSpread * ONE_OVER_ZONES;

					for(s32 z = 0; z < ZONES; z++)
					{
						static dev_float SPREAD_SIZE_MOD = 0.25f;
						float fThisBulletSpreadMin = fMultiShotZoneSpread*SPREAD_SIZE_MOD + fMultiShotZoneSpread*z;
						float fThisBulletSpreadMax = fMultiShotZoneSpread*(1.f-SPREAD_SIZE_MOD) + fMultiShotZoneSpread*z;
		
						u32 uBulletsInZone = Min(uBullets, uBulletsPerZone);
						float fSegmentSize = TWO_PI/(float)uBulletsInZone;
						for(s32 i = 0; i < uBulletsInZone; i++)
						{
							float fThisBulletSpread = 0.f;
							float fSegmentAngle = 0.f;
							fThisBulletSpread = NetworkInterface::IsNetworkOpen() ? g_ReplayRand.GetRanged(fThisBulletSpreadMin, fThisBulletSpreadMax) : fwRandom::GetRandomNumberInRange(fThisBulletSpreadMin, fThisBulletSpreadMax);
							fSegmentAngle = NetworkInterface::IsNetworkOpen() ? g_ReplayRand.GetRanged(0.f, fSegmentSize*(1.f-SEGMENT_SIZE_MOD)) : fwRandom::GetRandomNumberInRange(0.f, fSegmentSize*(1.f-SEGMENT_SIZE_MOD));
							Vector2 offset(Cosf(fSegmentAngle) * fThisBulletSpread, Sinf(fSegmentAngle) * fThisBulletSpread);
							offset.Rotate(((float)i*fSegmentSize)+fSegmentSize*SEGMENT_SIZE_MOD);

							Vector3 vBatchEnd = vecMultShotEnd + offset.x*matShot.a + offset.y*matShot.c;
							if(CBulletManager::ms_bUseBatchTestsForSingleShotWeapons)
							{
								pBullet->Add(vStart, vBatchEnd, fShotVelocity, GetWeaponHash(), bCreateTraceVfx, GetIsAccurate(vAdjustedEndAfterBulletBending, vAdjustedEnd));
							}
							else
							{
								if(weaponVerifyf(CBullet::GetPool()->GetNoOfFreeSpaces() > 0, "CBullet: Pool full, max %d", CBullet::GetPool()->GetSize()))
								{
									CBullet* pBullet = rage_new CBullet(params.pFiringEntity, 
										GetWeaponInfo(), 
										vStart, 
										vBatchEnd, 
										fShotVelocity, 
										fLifeTime, 
										fDamage, 
										GetWeaponHash(), 
										bCreateTraceVfx, 
										bAllowDamageToVehicle, 
										bAllowDamageToVehicleOccupants, 
										vMuzzlePosition, 
										vMuzzleDirection, 
										GetIsAccurate(vAdjustedEndAfterBulletBending, vAdjustedEnd), 
										bUseBulletPenetration, 
										bPassThroughBulletProofGlass,
										weaponAccuracy.fAccuracyMin, 
										m_bSilenced, 
										fFallOffRangeModifier,
										fFallOffDamageModifier,
										bBulletBent,
										this);

									CBulletManager::Add(pBullet);
								}
							}
						}

						uBullets -= uBulletsInZone;
					}
				}
			}
			else
			{
#if __ASSERT
				// Don't bother people with an assert if the game's paused
				if (!fwTimer::IsGamePaused())
				{
					weaponAssertf(0, "CBullet: Pool full, max %d", CBullet::GetPool()->GetSize());
				}
				else
				{
					weaponErrorf("CBullet: Pool full, max %d", CBullet::GetPool()->GetSize());
				}
#endif // __ASSERT
			}
		}
#if __BANK
		else
		{
			if(m_pDebugData)
			{
				m_pDebugData->m_iAccuracyBlanksFired++;
			}
		}
#endif // __BANK

		// Do everything that needs to be done when a gun is fired
		DoWeaponFire(params.pFiringEntity, params.weaponMatrix, vAdjustedEnd, params.iVehicleWeaponIndex, params.iVehicleWeaponBoneIndex, 1, params.fFireAnimRate, iShotsFired == 0, bFireBlank, params.bAllowRumble, params.bFiredFromAirDefence);

		++iShotsFired;
	}

#if __BANK
	if(m_pDebugData)
	{
		m_pDebugData->m_iAccuracyShotsFired += iShotsFired;
	}
#endif // __BANK

	weaponDebugf3("CWeapon::FireDelayedHit--complete (iShotsFired[%d] > 0)-->return [%d]",iShotsFired,(iShotsFired > 0));

	return iShotsFired > 0;
}

////////////////////////////////////////////////////////////////////////////////
static dev_float minThrowVel = 2.5f;
static dev_float maxThrowVelCoeff = 1.0f;
static dev_float throwVelRangeModifier = 1.0f;
static dev_float heightReductionForClosingVehicles = 0.5f;
static dev_float closingVelocityReduction = 0.7f;
static dev_float maxVerticalLaunchInside = 0.35f;
//static dev_float minPlatformThrowVel = 8.0f;
static dev_float thrownWeaponPlatformVelocityRangeCoeff = 1.0f;	//seconds worth of velocity to boost your target range?
static dev_float maxDistanceAheadToLerpPlatformVelCoeff = 0.6f;
static dev_float distanceAheadPaddingToLerpPlatformVel = 3.0f;
static dev_float sidePaddingForPlatformVelCoeff = 0.45f;
static dev_float sideDistPaddingForPlatformVel = 1.0f;
static dev_float minDirectionZToLerpPlatformVel = 0.2f;
bool CWeapon::FireProjectile(const sFireParams& params, const Vector3& vStart, const Vector3& vEnd)
{
	weaponDebugf3("CWeapon::FireProjectile m_State[%d]",m_State);

	if(!weaponVerifyf(GetWeaponInfo()->GetAmmoInfo(), "Attempting to fire a projectile but weapon [%s] has no ammo", GetWeaponInfo()->GetName()))
	{
		return false;
	}

	if (m_State != STATE_READY)
	{
		if (!NetworkInterface::IsGameInProgress())
		{
			//SP - weapon not ready
			return false;
		}
		else
		{
			//MP - allow on remotes unless under special situation where vehicle that is attached to helicopter
			bool bProceedWithFiring = false;
			
			if (params.pFiringEntity)
			{
				if (params.pFiringEntity->GetIsTypeVehicle() && params.pFiringEntity->GetIsAttached())
				{
					CVehicle* pVehicle = static_cast<CVehicle*>(params.pFiringEntity);
					if (pVehicle)
					{
						CPed* pDriver = pVehicle->GetDriver();
						if (!pDriver || pDriver->IsNetworkClone())
						{
							bProceedWithFiring = true;
						}

						//ELSE pDriver is local - stop firing from proceeding as if the vehicle was local
					}
				}
				else if (NetworkUtils::IsNetworkClone(params.pFiringEntity))
				{
					bProceedWithFiring = true;
				}
			}

			if (!bProceedWithFiring)
				return false;
		}
	}

	Vector3 vAdjustedEnd = vEnd;
	Vector3 vTargetVelocity = VEC3_ZERO;
	bool bUseTargetVelocity = false;
	bool bValidateGroundSpeedFromPlatform = false;

	CPed* pFiringPed = NULL;
	bool bLimitAccuracyTurret = false;
	if( params.pFiringEntity )
	{
		if( params.pFiringEntity->GetIsTypePed() )
			pFiringPed = static_cast<CPed*>(params.pFiringEntity);
		else if( params.pFiringEntity->GetIsTypeVehicle() )
		{
			CVehicle* pVehicle = static_cast<CVehicle*>(params.pFiringEntity);
			pFiringPed = pVehicle->GetDriver();
			
			if(pVehicle->GetVehicleWeaponMgr() && pVehicle->GetVehicleWeaponMgr()->GetNumTurrets() != 0)
			{
				bLimitAccuracyTurret = true;
			}
		}
	}

	// Tez: Need to know which firing vec calc type to use - maybe put this info in RAVE?
	ComputeBulletBending(params.pFiringEntity, vStart, vAdjustedEnd, params.fDesiredTargetDistance);
	weaponDebugf1("CWeapon::FireProjectile: ComputeBulletBending - vStart (%.2f,%.2f,%.2f), vEnd (%.2f,%.2f,%.2f), vAdjustedEnd (%.2f,%.2f,%.2f)", vStart.x, vStart.y, vStart.z, vEnd.x, vEnd.y, vEnd.z, vAdjustedEnd.x, vAdjustedEnd.y, vAdjustedEnd.z);

	bool bIsAccurate = true;
	if(pFiringPed && !pFiringPed->IsAPlayerPed())
	{
		sWeaponAccuracy weaponAccuracy;
		bIsAccurate = ComputeAccuracy(pFiringPed, params.pTargetEntity, RCC_VEC3V(vStart), RC_VEC3V(vAdjustedEnd), weaponAccuracy, false, params.fDesiredTargetDistance);
	
		//Stop turrets from firing at incorrect angles due to high accuracy
		if(bLimitAccuracyTurret)
		{
			Vector3 vAdjustedDir = (vAdjustedEnd - vStart);
			vAdjustedDir.Normalize();
			Vector3 vDir = (vEnd - vStart);
			vDir.Normalize();
			
			static dev_float sfAngularThreshold = 1 - (14.9f * DtoR);
			if( (vDir).Dot(vAdjustedDir) < sfAngularThreshold )
			{
				vAdjustedEnd = vEnd;
			}
		}
	}

	const CWeaponInfo* pWeaponInfo = GetWeaponInfo();

	// JB: Can pFiringEntity be a vehicle?  If so we will need more than 10 exclusion entities
	s32 iFlags = ArchetypeFlags::GTA_ALL_TYPES_WEAPON;

	// Need projectiles to adjust position for ragdoll bounds too (e.g. for hitting peds on bikes that are outside collision capsule).
	if(pWeaponInfo && pWeaponInfo->GetIsThrownWeapon())
	{
		iFlags |= ArchetypeFlags::GTA_RAGDOLL_TYPE;
	}

	const s32 EXCLUSION_MAX = MAX_INDEPENDENT_VEHICLE_ENTITY_LIST;
	const CEntity* pExclusionList[EXCLUSION_MAX];
	s32 iExclusions = 0;

	// If we have a pObjOverride and its not attached to the pParentEntity it wont be picked up by GeneratePhysExclusionList
	// so add it in here
	if(params.pObjOverride && !params.pObjOverride->GetIsAttached() && iExclusions < EXCLUSION_MAX)
	{
		pExclusionList[iExclusions++] = params.pObjOverride;
	}

	if( pFiringPed )
	{
		if(pFiringPed->GetMyMount())
		{
			pFiringPed->GeneratePhysExclusionList(pExclusionList, iExclusions, EXCLUSION_MAX, iFlags, TYPE_FLAGS_ALL, pFiringPed->GetMyMount());
		}
		else if(pFiringPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle) && pFiringPed->GetMyVehicle() && !params.iFireFlags.IsFlagSet(FF_AllowDamageToVehicle))
		{
			pFiringPed->GeneratePhysExclusionList(pExclusionList, iExclusions, EXCLUSION_MAX, iFlags, TYPE_FLAGS_ALL, pFiringPed->GetMyVehicle());
		}
		else
		{
			pFiringPed->GeneratePhysExclusionList(pExclusionList, iExclusions, EXCLUSION_MAX, iFlags, TYPE_FLAGS_ALL, pFiringPed);
		}
	}
	else
	{
		if (params.pFiringEntity)
		{
			params.pFiringEntity->GeneratePhysExclusionList(pExclusionList, iExclusions, EXCLUSION_MAX, iFlags, TYPE_FLAGS_ALL, params.pFiringEntity);
		}
	}

	//Are we on a platform?
	bool bHasPlatformVelocity = false;
	Vector3 vPlatformVelocity;
	vPlatformVelocity.Zero();

	//TMS:	If we're using the target velocity that takes precedence, otherwise we'll modify throwing angle based on the
	//		platform velocity. This means that we can target the ground in front of a vehicle.
	CPhysical *pParentVelEntity = 0;
	if(pFiringPed)
	{
		if ( pFiringPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle) && pFiringPed->GetMyVehicle()) //add vehicle velocity
		{
			pParentVelEntity = pFiringPed->GetMyVehicle();
		}
		else if (pFiringPed->GetPedConfigFlag(CPED_CONFIG_FLAG_OnMount) && pFiringPed->GetMyMount())
		{
			pParentVelEntity = pFiringPed->GetMyMount();
		}
		else if (pFiringPed->GetGroundPhysical())
		{
			//On a train?
			pParentVelEntity = pFiringPed->GetGroundPhysical();
		}

		if (pParentVelEntity)
		{
			bHasPlatformVelocity = true;
			vPlatformVelocity = pParentVelEntity->GetVelocity();

			if (GetWeaponInfo()->GetIsThrownWeapon())
			{
				//Adjust vAdjustedEnd to allow for your platform velocity
				Vector3 vDir = vAdjustedEnd - vStart;
				vDir.Normalize();
				float fRangeTestBoost = vDir.Dot(vPlatformVelocity);
				if (fRangeTestBoost > 0.0f)
				{
					fRangeTestBoost *= thrownWeaponPlatformVelocityRangeCoeff;
					vAdjustedEnd += vDir*fRangeTestBoost;
					DR_WEAPON_ONLY(debugPlayback::RecordTaggedFloatValue(*pFiringPed->GetCurrentPhysicsInst(), fRangeTestBoost, "fRangeTestBoost"));
				}
			}
		}
	}

	// Check line of sight from entity that's firing to the position the projectile is getting added,
	// to make sure we're not adding a projectile on the other side of a wall
	bool bLOSClear = true;
	bool bFiredFromCover = false;
	CPhysical* pTargetPhysical = 0;
	if(params.pFiringEntity)
	{
		// Calculate the test start and end positions, push the end out slightly to take the objects size into account
		Vector3 vTestStart = VEC3V_TO_VECTOR3(params.pFiringEntity->GetTransform().GetPosition());
		bool bForceLosAsClear = false;

		// We want to skip this check on certain weapon types where the firing ped is inside an interior in another part of the map
		if (GetWeaponInfo()->GetSkipProjectileLOSCheck())
		{
			bForceLosAsClear = true;
		}

		// If the player can fire from cover, we don't need to do the LOS check, rely on the cover
		// collisions to validate the firing position
		s32 iCoverState = 0;
		bool bCanFireFromCover = false;
		if(pFiringPed && pFiringPed->GetPedIntelligence()->GetPedCoverStatus(iCoverState, bCanFireFromCover))
		{
			bForceLosAsClear = true;
		}
		// Otherwise use the peds head bone as the start position for collision checks.
		else if(pFiringPed && !pFiringPed->GetIsCrouching())
		{
			vTestStart.z += pFiringPed->GetCapsuleInfo()->GetMaxSolidHeight() - 0.15f;
		}

		Vector3 vTestEnd;
		if(params.pObjOverride)
		{
			vTestEnd = params.weaponMatrix.d;
		}
		else
		{
			vTestEnd = vStart;
		}

		Vector3 vAdjustedStart = vStart;
		if (pFiringPed && pFiringPed->GetIsInCover() && pFiringPed->IsAPlayerPed())
		{
			// B*2013547: Projectiles already handle this in CTaskThrowProjectile::ProcessPostPreRender. We use the camera vector but offset it so it starts from the peds position.
			// Ensures we don't fire the projectile backwards if we detect a collision between ped and camera.
			if(!params.bDisablePlayerCoverStartAdjustment && pWeaponInfo && !pWeaponInfo->GetIsProjectile())
			{
				//When in cover, player's should use camera position
				const camFrame& aimFrame = camInterface::GetPlayerControlCamAimFrame();			
				vAdjustedStart = aimFrame.GetPosition();
			}

			bFiredFromCover = true;
		}
		// Debug
#if __BANK
		// Draw a debug line to show the shot direction
		if(CPedDebugVisualiserMenu::GetDisplayDebugShooting())
		{			
			CPedAccuracy::ms_debugDraw.AddLine(RCC_VEC3V(vAdjustedStart), RCC_VEC3V(vAdjustedEnd), Color_pink, 1000);
		}

#endif // __BANK

		DR_WEAPON_ONLY(debugPlayback::RecordTaggedVectorValue(*pFiringPed->GetCurrentPhysicsInst(), RCC_VEC3V(vTestEnd), "vTestEnd", debugPlayback::eVectorTypePosition));

		WorldProbe::CShapeTestProbeDesc probeDesc;
		probeDesc.SetStartAndEnd(vTestStart, vTestEnd);
		probeDesc.SetIncludeFlags(iFlags);
		probeDesc.SetExcludeTypeFlags(ArchetypeFlags::GTA_PICKUP_TYPE);
		probeDesc.SetExcludeEntities(pExclusionList, iExclusions);
		bLOSClear = bForceLosAsClear || !WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc);
		DR_WEAPON_ONLY(debugPlayback::AddSimpleLine("LOSTest", RCC_VEC3V(vTestStart), RCC_VEC3V(vTestEnd), Color32(0,0,0), bLOSClear ? Color32(0,255,0) : Color32(255,0,0)));
				
		WorldProbe::CShapeTestFixedResults<> probeResults;
		probeDesc.Reset();
		probeDesc.SetResultsStructure(&probeResults);
		probeDesc.SetStartAndEnd(vAdjustedStart, vAdjustedEnd);
		DR_WEAPON_ONLY(debugPlayback::AddSimpleLine("HitTestLine", RCC_VEC3V(vTestStart), RCC_VEC3V(vAdjustedEnd), Color32(255,0,0), Color32(255,0,0)));
		DR_WEAPON_ONLY(debugPlayback::AddSimpleSphere("vAdjustedEnd", RCC_VEC3V(vAdjustedEnd), 0.1f, Color32(255,0,0)));
		probeDesc.SetIncludeFlags(iFlags);
		probeDesc.SetExcludeTypeFlags(ArchetypeFlags::GTA_PICKUP_TYPE);
		probeDesc.SetExcludeEntities(pExclusionList, iExclusions);
		if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
		{
			vAdjustedEnd = probeResults[0].GetHitPosition();
			DR_WEAPON_ONLY(debugPlayback::AddSimpleSphere("HitTestPos", RCC_VEC3V(vAdjustedEnd), 0.1f, Color32(255,0,0)));

			//Adjust for target velocity
			CEntity* pTargetEntity = probeResults[0].GetHitEntity();
			if (pTargetEntity)
			{
				// B*2160323: Disabling velocity adjustment code for now.
				TUNE_GROUP_BOOL(PROJECTILE_VELOCITY_ADJUSTMENT, bEnableTargetVelocityAdjustmentAgainstPedsAndVehicles, false);
				if (bEnableTargetVelocityAdjustmentAgainstPedsAndVehicles && (pTargetEntity->GetIsTypeVehicle() || pTargetEntity->GetIsTypePed()))
				{
					pTargetPhysical = static_cast<CPhysical*>(pTargetEntity);
					vTargetVelocity = pTargetPhysical->GetVelocity();
					weaponDebugf1("CWeapon::FireProjectile - pTargetPhysical 0x%p, V:%d, P:%d, vTargetVelocity (%.2f,%.2f,%.2f)", pTargetPhysical, pTargetPhysical->GetIsTypeVehicle(), pTargetPhysical->GetIsTypePed(), vTargetVelocity.x, vTargetVelocity.y, vTargetVelocity.z);

					//Make sure target velocity makes sense coming from this shooter:
					if (pFiringPed) 
					{
						bUseTargetVelocity = true;

						Vector3 vFiringEntityVelocity = ( pFiringPed->GetIsInVehicle() ? pFiringPed->GetMyVehicle()->GetVelocity() : pFiringPed->GetVelocity());
						//Moving in opposite directions?
						float dot = vFiringEntityVelocity.Dot(vTargetVelocity);

						//B*2110170: Extrapolate the velocity (if target entity is travelling over a certain threshold): 
						//If the entity is going to be behind the ped after 0.5s (~duration of the throw), then don't scale based on the target velocity.
						bool bEntityWillBeBehindPedOnceThrown = false;
						static dev_float fVelocityThreshold = 25.0f;
						if (vFiringEntityVelocity.Mag() > fVelocityThreshold)
						{
							static dev_float fThrowDuration = 0.5f;
							Vector3 vTargetPositionAfterThrow = VEC3V_TO_VECTOR3(pTargetEntity->GetTransform().GetPosition()) + (vTargetVelocity * fThrowDuration);

							const Vec3V vCarPos = VECTOR3_TO_VEC3V(vTargetPositionAfterThrow);
							const Vec3V vPedForward = pFiringPed->GetTransform().GetForward();
							const Vec3V vPedPos = pFiringPed->GetTransform().GetPosition();
							const Vec3V vDelta = vCarPos - vPedPos;

							bEntityWillBeBehindPedOnceThrown = IsLessThanAll(Dot(vDelta, vPedForward), ScalarV(V_ZERO)) != 0;
						}
					
						if (dot < 0.0f && !bEntityWillBeBehindPedOnceThrown)
						{
							dot /= vTargetVelocity.Mag() * vFiringEntityVelocity.Mag();
							dot = -dot;
							
							//dot is now 0 -> 1.0, 1.0 when heading in opposite directions
							//Scale down the target velocity to avoid it overriding and forcing
							//a backwards throw (may cause a miss)
							vTargetVelocity.Scale(1.0f - dot*closingVelocityReduction);
						}						
					}					
				}
				else if (pTargetEntity->GetIsTypeBuilding())
				{
					if (pFiringPed && pFiringPed->GetIsInVehicle())
					{
						bUseTargetVelocity = true;
						bValidateGroundSpeedFromPlatform = true;	//Need to make sure we're not throwing backwards!
						vTargetVelocity.Zero();
					}
				}
			}
		}	
	}
	
	const CVehicle* pFiringPedVehicle = pFiringPed ? pFiringPed->GetMyVehicle() : NULL;
	
	bool bManuallyCalculateProjectileOrientation = false;

	CProjectile* pProjectileObject = NULL;
	if(pFiringPed)
	{
		const bool bPedHasInfiniteAmmo = GetHasInfiniteClips(pFiringPed);
		const bool bPhantomRearSeats = pFiringPedVehicle && pFiringPedVehicle->GetSeatManager() && pFiringPedVehicle->GetSeatManager()->GetPedsSeatIndex(pFiringPed) > 1 && 
									   ((MI_CAR_PHANTOM2.IsValid() && pFiringPedVehicle->GetModelIndex() == MI_CAR_PHANTOM2) || 
										(MI_CAR_PHANTOM3.IsValid() && pFiringPedVehicle->GetModelIndex() == MI_CAR_PHANTOM3));
		const bool bVehicleShouldManualCalc = pFiringPedVehicle && (pFiringPedVehicle->GetVehicleType() == VEHICLE_TYPE_HELI || bPhantomRearSeats);
		const bool bPedIsUsingLauncher = pWeaponInfo->GetIsRpg() || pWeaponInfo->GetIsLaunched();

		if( bPedIsUsingLauncher && 
			bVehicleShouldManualCalc &&
			pFiringPed->IsInFirstPersonVehicleCamera() )
		{
			bManuallyCalculateProjectileOrientation = true;
		}
		else if(bPedIsUsingLauncher && bPedHasInfiniteAmmo)
		{
			bManuallyCalculateProjectileOrientation = true;
		}

		if( !bPedHasInfiniteAmmo && !params.bCreateNewProjectileObject)
		{
			CPedEquippedWeapon* pEquippedWeapon = pFiringPed->GetWeaponManager()->GetPedEquippedWeapon();
			pProjectileObject = pEquippedWeapon ? pEquippedWeapon->GetProjectileOrdnance() : NULL;
			if( pProjectileObject )
			{
				// If using existing projectile, ensure attachments are up to date
				pFiringPed->ProcessAllAttachments();

				pProjectileObject->SetOrdnance( false );
				pProjectileObject->DetachFromParent( DETACH_FLAG_ACTIVATE_PHYSICS | DETACH_FLAG_APPLY_VELOCITY );
				pProjectileObject->SetNoCollision( pFiringPed, NO_COLLISION_RESET_WHEN_NO_IMPACTS );
				if (params.pIgnoreCollisionEntity)
					pProjectileObject->SetNoCollision( params.pIgnoreCollisionEntity, NO_COLLISION_NEEDS_RESET);

				if(bManuallyCalculateProjectileOrientation)
				{
					pProjectileObject->SetMatrix( BuildViewMatrix(params.weaponMatrix.d, vAdjustedEnd - params.weaponMatrix.d), true, true, true );
				}
			}
		}
	}

	// OK it's safe to add the projectile as normal
	if(bLOSClear)
	{
		// Create the projectile if not previous set
		if( !pProjectileObject )
		{
			if (pWeaponInfo->GetIsThrownWeapon())
				pProjectileObject = CProjectileManager::CreateProjectile(pWeaponInfo->GetAmmoInfo()->GetHash(), pWeaponInfo->GetHash(), params.pFiringEntity, params.weaponMatrix, pWeaponInfo->GetDamage(), pWeaponInfo->GetDamageType(), pWeaponInfo->GetEffectGroup(), NULL, NULL, m_uTintIndex, params.bProjectileCreatedByScriptWithNoOwner, params.bProjectileCreatedFromGrenadeThrow);
			else
			{
				Matrix34 projectileOrientation;
				if(bManuallyCalculateProjectileOrientation)
				{
					projectileOrientation = BuildViewMatrix(params.weaponMatrix.d, vAdjustedEnd - params.weaponMatrix.d);
				}
				else
				{
					GetMuzzleMatrix(params.weaponMatrix, projectileOrientation);
				}

				pProjectileObject = CProjectileManager::CreateProjectile( pWeaponInfo->GetAmmoInfo()->GetHash(), 
																		  pWeaponInfo->GetHash(), params.pFiringEntity, 
																		  projectileOrientation, pWeaponInfo->GetDamage(), 
																		  pWeaponInfo->GetDamageType(), pWeaponInfo->GetEffectGroup(), 
																		  NULL, NULL, m_uTintIndex, params.bProjectileCreatedByScriptWithNoOwner );
			}			
		}

		if(pProjectileObject)
		{
			if (params.bFreezeProjectileWaitingOnCollision)
				pProjectileObject->SetAllowFreezeWaitingOnCollision(true);

			if (params.pIgnoreCollisionEntity)
			{
				if (params.bIgnoreCollisionResetNoBB)
				{
					pProjectileObject->SetNoCollision(params.pIgnoreCollisionEntity, NO_COLLISION_RESET_WHEN_NO_BBOX);
				}
				else
				{
					pProjectileObject->SetNoCollision(params.pIgnoreCollisionEntity, NO_COLLISION_NEEDS_RESET);
				}
			}


			// Make sure that the fired projectile is visible (completely ignoring the HideDrawable projectile info flag for no apparent reason...)
			if( !pProjectileObject->GetIsVisible() && GetWeaponHash() != ATSTRINGHASH("WEAPON_RAYPISTOL", 0xAF3696A1) && GetWeaponHash() != ATSTRINGHASH("VEHICLE_WEAPON_BOMB_WATER", 0xF444C4C8))
				pProjectileObject->SetIsVisibleForModule( SETISVISIBLE_MODULE_GAMEPLAY, true );

			// If we are allowing damage, let projectile hit vehicle by assuming it was thrown outside it.
			if(params.iFireFlags.IsFlagSet(FF_AllowDamageToVehicle) && !pFiringPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DontHitVehicleWithProjectiles))
			{
				pProjectileObject->FlagAsThrownFromOutsideVehicle();
			}

			// Grab the distance to the selected target
			Vector3 vWeaponPosition = VEC3V_TO_VECTOR3( pProjectileObject->GetTransform().GetPosition() );
			float fDist = vWeaponPosition.Dist( vAdjustedEnd );
			static dev_float sf_MinCoverFireDistance = 2.0f;
			if (bFiredFromCover && fDist <= sf_MinCoverFireDistance)
				bFiredFromCover = false;

			pProjectileObject->SetIgnoreDamageEntity(params.pIgnoreDamageEntity);

			if (params.bIgnoreDamageEntityAttachParent)
				pProjectileObject->SetIgnoreDamageEntityAttachParent(true);

			if(CProjectileRocket* pRocket = pProjectileObject->GetAsProjectileRocket())
			{
// 				Matrix34 mat(MAT34V_TO_MATRIX34(pProjectileObject->GetTransform().GetMatrix()));
// 				mat.b = (vAdjustedEnd - vWeaponPosition);
// 				mat.b.Normalize();
// 				mat.c = Vector3(0.0f, 0.0f, 1.0f);
// 				mat.a.Cross(mat.b, mat.c);
// 				mat.a.Normalize();
// 				mat.c.Cross(mat.a, mat.b);
// 				mat.c.Normalize();
// 				pProjectileObject->SetMatrix(mat);

				// Set initial launcher speed base 
				if( params.fInitialVelocity != -1.0f)
				{
					pRocket->SetInitialLauncherSpeed(params.fInitialVelocity);
				}

				// Set the rocket target if defined previously (refer to CFixedVehicleWeapon::FireInternal)
				if( params.pTargetEntity != NULL )
				{
					pRocket->SetTarget( params.pTargetEntity );

					// Set a flag that will defined whether or not we apply miss logic
					pRocket->SetIsAccurate( bIsAccurate );
					pRocket->SetCachedTargetPosition( vAdjustedEnd );
				}
			}

			Vector3 vDirection( vAdjustedEnd - vWeaponPosition );
			vDirection.Normalize();
			DR_WEAPON_ONLY(if (pFiringPed){debugPlayback::RecordTaggedVectorValue(*pFiringPed->GetCurrentPhysicsInst(), RCC_VEC3V(vDirection), "vDirection(Initial)", debugPlayback::eVectorTypeVelocity)});

			// Some weapons desire a slight offset to the vertical launch direction (grenade launcher)
			float fVerticalLaunchAdjustment = pWeaponInfo->GetVerticalLaunchAdjustment();

			// Special case launch adjustment when trying to throw indoors
			if( pWeaponInfo->GetIsThrownWeapon() )
			{	
				if(pFiringPed && pFiringPed->GetIsInInterior() )
				{
					fVerticalLaunchAdjustment = fVerticalLaunchAdjustment>0.0f ? Min(fVerticalLaunchAdjustment, maxVerticalLaunchInside) : maxVerticalLaunchInside;	
				}
				// Otherwise lets throw out a probe to see if we are in a low ceiling situation
				else
				{
					static dev_float sfHeightThreshold = 4.0f;
					WorldProbe::CShapeTestProbeDesc probeDesc;
					WorldProbe::CShapeTestSingleResult probeResult;
					probeDesc.SetResultsStructure( &probeResult );

					Vector3 vTestDirection = vDirection;
					vTestDirection.z += fVerticalLaunchAdjustment;
					vTestDirection.Normalize();

					Vector3 vTestEndPoint = vWeaponPosition + VEC3V_TO_VECTOR3( Scale( RCC_VEC3V( vTestDirection ), ScalarV( pWeaponInfo->GetRange() * 0.5f ) ) );
					probeDesc.SetStartAndEnd( vWeaponPosition, vTestEndPoint );
					probeDesc.SetIncludeFlags( iFlags );
					probeDesc.SetExcludeEntities( pExclusionList, iExclusions );
					if( WorldProbe::GetShapeTestManager()->SubmitTest( probeDesc ) )
					{
						// Attempt to lower the vertical launch adjustment by half
						float fHeightDiff = probeResult[0].GetHitPosition().z - vWeaponPosition.z;
						if( fHeightDiff < sfHeightThreshold )
						{
							fVerticalLaunchAdjustment = fVerticalLaunchAdjustment>0.0f ? Min(fVerticalLaunchAdjustment, maxVerticalLaunchInside) : maxVerticalLaunchInside;	
						}
					}
				}
			}

			float fLaunchSpeedOverride = params.fOverrideLaunchSpeed;
			bool bAllowDamping = true;
			if (params.iFireFlags.IsFlagSet(FF_DropProjectile))
			{ //this is a drop
				fLaunchSpeedOverride = 1.0f;
			}
			else if( pWeaponInfo->GetIsThrownWeapon() || pWeaponInfo->GetIsLaunched() )
			{
				// Form the forward vector used to calculate the launch angle
				Vector3 vWeaponForward = vDirection;
				if( params.pFiringEntity )
					vWeaponForward.z = VEC3V_TO_VECTOR3( params.pFiringEntity->GetTransform().GetB() ).z;
				vWeaponForward.Normalize();
				
				Vector3 vHorizontalAdjustedEnd = vAdjustedEnd;
				vHorizontalAdjustedEnd.z = vWeaponPosition.z;
				float fVX = vWeaponPosition.Dist( vHorizontalAdjustedEnd );
				float fVY = vAdjustedEnd.z - vWeaponPosition.z;

				const float fGravity = Abs(PHSIM->GetGravity().z);

				//1) Estimate speed needed to throw
				//Conservation of entergy says that mgh=0.5mv^2
				const float fVelForVerticalRange = fVY > 0.0f ? Sqrtf(2.0f * fGravity * fVY) : 0.0f;
				//@45 degrees our horizontal range is defined by v^2=dg
				float fVelForHorizontalRange = Sqrtf(Abs(fVX)*fGravity);
				//Fudge these two together
				fLaunchSpeedOverride = throwVelRangeModifier * Sqrtf(fVelForHorizontalRange*fVelForHorizontalRange + fVelForVerticalRange*fVelForVerticalRange);

				//2) Bake again, this time taking the speed difference to the target entity into account
				float fHeightScaleForClosing = 1.0f;
				if (pTargetPhysical)
				{
					float fEstimatedTravelTime = fVX / fLaunchSpeedOverride;

					//Project our extra range difference at contact time
					float fTargetVelDelta = vDirection.Dot(pTargetPhysical->GetVelocity() - vPlatformVelocity);
					if (fTargetVelDelta > 0.0f)
					{
						DR_WEAPON_ONLY(if (pFiringPed){debugPlayback::RecordTaggedFloatValue(*pFiringPed->GetCurrentPhysicsInst(), fTargetVelDelta, "fTargetVelDelta")});
						fVX += fTargetVelDelta * fEstimatedTravelTime;

						//And recalculate with this in mind (rk4 would be more accurate if needed)
						fVelForHorizontalRange = Sqrtf(Abs(fVX)*fGravity);
						//Fudge these two together (again)
						fLaunchSpeedOverride = throwVelRangeModifier * Sqrtf(fVelForHorizontalRange*fVelForHorizontalRange + fVelForVerticalRange*fVelForVerticalRange);
						fLaunchSpeedOverride += fTargetVelDelta;//?? TMS - speed up throw towards max for more accuracy
					}
					else
					{
						//Closing
						//Keep fast but throw lower?
						fLaunchSpeedOverride = throwVelRangeModifier * Sqrtf(fVelForHorizontalRange*fVelForHorizontalRange + fVelForVerticalRange*fVelForVerticalRange + fTargetVelDelta*fTargetVelDelta);
						fHeightScaleForClosing = 1.0f - Clamp((-fTargetVelDelta*fEstimatedTravelTime) / (fVX + 1.0f), 0.0f, heightReductionForClosingVehicles);
					}
				}

				//Adjust depending on the fVerticalLaunchAdjustment
				if (fVerticalLaunchAdjustment > 0.0f)
				{
					//This will still get clamped later, but meanwhile allows the weapon to generally shoot faster and lower
					fLaunchSpeedOverride *= Sqrtf(1.0f / fVerticalLaunchAdjustment);
				}

				//Always throw a bit faster than the minimum needed
				fLaunchSpeedOverride += minThrowVel;

				//Limit, arm's are only so strong, weapons only have so much range
				const float fMaxHorzRangeVelLimit = Sqrtf(pWeaponInfo->GetRange() * fGravity);
				DR_WEAPON_ONLY(if (pFiringPed){debugPlayback::RecordTaggedFloatValue(*pFiringPed->GetCurrentPhysicsInst(), fMaxHorzRangeVelLimit, "fMaxHorzRangeVelLimit")});
				fLaunchSpeedOverride = Min(fLaunchSpeedOverride, maxThrowVelCoeff * fMaxHorzRangeVelLimit);

				//Add in our platform velocity
				if (bHasPlatformVelocity)
				{
					float fDot = vPlatformVelocity.Dot(vDirection);
					fLaunchSpeedOverride += Abs(fDot);	//To Abs or not to Abs that is the question
				}

				//Now use this velocity to find the angle
				//http://en.wikipedia.org/wiki/Trajectory_of_a_projectile#Angle_required_to_hit_coordinate_.28x.2Cy.29
				const float fVel2 = fLaunchSpeedOverride*fLaunchSpeedOverride;
				
				float fVel4 = fVel2*fVel2;	//This will be - fB for the content of the sqrt
				float fB = fGravity*(fGravity*fVX*fVX + 2.0f*fVY*fVel2);

				fB = Min(fB, fVel4);	//Clamp to max angle, not going to actually hit it if we clamp!
				float fC = fGravity*fVX;
				if(fC > 0.0f)
				{
					float fTanTheta=0.0f;
					float fPlusOrMinus = Sqrtf(fVel4 - fB);
					fTanTheta = (fVel2 - fPlusOrMinus) / fC;

					//Adjust depending on the fVerticalLaunchAdjustment
					float fHorzDirection = vDirection.XYMag();
					if (fVerticalLaunchAdjustment > 0.0f)
					{
						//For some weapons (grenade launcher for example) we
						//don't want to shoot very far above the aim direction
						//In theory we've scaled speed up to compensate and hit 
						//the target point but worst case we just clamp and players
						//can adjust shooting direction to get maximum range. Skillz.
						//This will also help with shooting under ceilings/
						float fLaunchDirFlatMag = fHorzDirection;
						if (fLaunchDirFlatMag > 0.0f)
						{
							float fLaunchTan = vDirection.z / fLaunchDirFlatMag;
							fTanTheta = Min(fTanTheta, fVerticalLaunchAdjustment+fLaunchTan);
						}
						else
						{
							fTanTheta = 1000.0f; //Shouldn't get here but will be close to vertical when normalized
						}
					}

					//tanX = opp / adj so we can reset our launch direction
					vDirection.z = fHorzDirection * fTanTheta * fHeightScaleForClosing;
					vDirection.Normalize();
					bAllowDamping = false;
				}

				DR_WEAPON_ONLY(if (pFiringPed){debugPlayback::RecordTaggedFloatValue(*pFiringPed->GetCurrentPhysicsInst(), fLaunchSpeedOverride, "fLaunchSpeedOverride")});

				//Make sure we miss the front of the vehicle / platform if we're on one and aiming down
				if (vDirection.z < 0.0f)
				{
					if (bValidateGroundSpeedFromPlatform && bHasPlatformVelocity && bUseTargetVelocity && pParentVelEntity && pParentVelEntity->GetArchetype())
					{
						//Lerp effect out towards the horizontal
						float fDirectionLerp = minDirectionZToLerpPlatformVel > 0.0f ? 1.0f - (minDirectionZToLerpPlatformVel-vDirection.z) / minDirectionZToLerpPlatformVel : 1.0f;

						//Are we in the trouble zone in front of the vehicle
						Matrix34 matParent = MAT34V_TO_MATRIX34( pParentVelEntity->GetTransform().GetMatrix() );
						Vector3 vOffsetLocal;
						matParent.UnTransform( vAdjustedEnd, vOffsetLocal );
						
						//Get a bounding box
						const spdAABB &aabb = pParentVelEntity->GetArchetype()->GetBoundingBox();

						//We need to work out how much of the platform velocity to lerp in to 
						float fLerpInPlatformVel = 0.0f;

						float fPlatformVelForwards = vPlatformVelocity.Dot(matParent.b);
						float fDistanceToLerpTo = maxDistanceAheadToLerpPlatformVelCoeff * fPlatformVelForwards;
						float fFrontOfPlatform = aabb.GetMax().GetYf();
						
						//Always add a little allowance to help clear your vehicle
						float fFrontDist = vOffsetLocal.y - (fFrontOfPlatform + distanceAheadPaddingToLerpPlatformVel);
						if (fFrontDist < fDistanceToLerpTo)
						{
							float fSidePadding = 0.0f;
							if (vOffsetLocal.x > 0.0f)
							{
								fSidePadding = vOffsetLocal.x - aabb.GetMax().GetXf();
							}
							else
							{
								fSidePadding = aabb.GetMin().GetXf() - vOffsetLocal.x;
							}

							//Always add a little allowance to help clear your vehicle
							fSidePadding -= sideDistPaddingForPlatformVel;

							//Create a lerp factor that is reduced (no platform vel) as the target
							//point gets further from the front or the side of the vehicle.

							float fSideLerpIn = 0.0f;	//No platform vel applied
							float fSidePaddingAllowance = sidePaddingForPlatformVelCoeff * fPlatformVelForwards;
							if (fSidePadding < fSidePaddingAllowance)
							{
								if (fSidePadding < 0.0f)
								{
									fSideLerpIn = 1.0f; //Up to full platform vel
								}
								else
								{
									fSideLerpIn = 1.0f - (fSidePadding / fSidePaddingAllowance);
								}

							}

							if (fSideLerpIn > 0.0f)
							{
								if (fFrontDist <= 0.0f)
								{
									fLerpInPlatformVel = 1.0f;
								}
								else
								{
									fLerpInPlatformVel = 1.0f - (fFrontDist * fSideLerpIn * fDirectionLerp / fDistanceToLerpTo);
								}
							}
						}

						//We need to make sure you are going to be launching in the direction you are pointing
						if (fLerpInPlatformVel > 0.0f)
						{
							//bUseTargetVelocity = false;
							//vTargetVelocity += vDirection * (minPlatformThrowVel - fTargetLaunchSpdDelta);
							vTargetVelocity = Lerp(fLerpInPlatformVel, vTargetVelocity, vPlatformVelocity);
							//DR_WEAPON_ONLY(debugPlayback::RecordTaggedFloatValue(*pFiringPed->GetCurrentPhysicsInst(), fLaunchSpeedOverride, "fLaunchSpeedOverrideAdjusted"));
						}
					}
				}

#if DR_WEAPON
				if (pFiringPed)
				{
					debugPlayback::RecordTaggedVectorValue(*pFiringPed->GetCurrentPhysicsInst(), RCC_VEC3V(vDirection), "vDirection(Final)", debugPlayback::eVectorTypeVelocity));
					debugPlayback::RecordTaggedFloatValue(*pFiringPed->GetCurrentPhysicsInst(), bUseTargetVelocity ? 1.0f : 0.0f, "bUseTargetVelocity"));
					debugPlayback::RecordTaggedFloatValue(*pFiringPed->GetCurrentPhysicsInst(), fC, "fC(should be > 0.0f)"));
					debugPlayback::RecordTaggedFloatValue(*pFiringPed->GetCurrentPhysicsInst(), fHeightScaleForClosing, "fHeightScaleForClosing"));
					debugPlayback::RecordTaggedVectorValue(*pFiringPed->GetCurrentPhysicsInst(), RCC_VEC3V(vTargetVelocity), "vTargetVelocity", debugPlayback::eVectorTypeVelocity));
				}
#endif
			}

			f32 fLifeTime = -1.0f;
			if(m_pTargetingComponent)
			{
				fLifeTime = m_pTargetingComponent->GetRange() / pProjectileObject->GetInfo()->GetLaunchSpeed();
				if (params.fOverrideLifeTime > -1.0f && params.fOverrideLifeTime < fLifeTime)
				{
					fLifeTime = params.fOverrideLifeTime;
				}
			}
			else if (params.fOverrideLifeTime > -1.0f)
			{
				fLifeTime = params.fOverrideLifeTime;
			}
			
			if(params.bCommandFireSingleBullet && params.fakeStickyBombSequenceId != INVALID_FAKE_SEQUENCE_ID)
			{
				pProjectileObject->SetTaskSequenceId(params.fakeStickyBombSequenceId);
			}

			bool bAllowToSetOwnerAsNoCollision = params.pIgnoreCollisionEntity ? false : true;
			pProjectileObject->SetFiredFromCover(bFiredFromCover);
			pProjectileObject->Fire( vDirection, fLifeTime, fLaunchSpeedOverride, bAllowDamping, params.bScriptControlled, params.bCommandFireSingleBullet, params.iFireFlags.IsFlagSet(FF_DropProjectile), bUseTargetVelocity ? &vTargetVelocity : NULL, params.iFireFlags.IsFlagSet(FF_DisableProjectileTrail), bAllowToSetOwnerAsNoCollision );
			weaponDebugf1("CWeapon::FireProjectile: Fire( (%.2f,%.2f,%.2f), %.2f, %.2f, %d, %d, %d, %d, %d ? (%.2f,%.2f,%.2f) : NULL, %d", vDirection.x, vDirection.y, vDirection.z, fLifeTime, fLaunchSpeedOverride, bAllowDamping, params.bScriptControlled, params.bCommandFireSingleBullet, params.iFireFlags.IsFlagSet(FF_DropProjectile), bUseTargetVelocity, vTargetVelocity.x, vTargetVelocity.y, vTargetVelocity.z, params.iFireFlags.IsFlagSet(FF_DisableProjectileTrail));
		}
	}
	// Uh oh, there's something in the way
	else
	{
		// Create the temp projectile only if we are not using the ordnance
		if( !pProjectileObject )
			pProjectileObject = CProjectileManager::CreateProjectile( pWeaponInfo->GetAmmoInfo()->GetHash(), pWeaponInfo->GetHash(), params.pFiringEntity, params.weaponMatrix, pWeaponInfo->GetDamage(), pWeaponInfo->GetDamageType(), pWeaponInfo->GetEffectGroup(), NULL, NULL, m_uTintIndex, params.bProjectileCreatedByScriptWithNoOwner);

		// Simply drop it where it is
		if( pProjectileObject )
		{
			if (params.bFreezeProjectileWaitingOnCollision)
				pProjectileObject->SetAllowFreezeWaitingOnCollision(true);

			if (params.pIgnoreCollisionEntity)
				pProjectileObject->SetNoCollision( params.pIgnoreCollisionEntity, NO_COLLISION_NEEDS_RESET );

			if (params.bIgnoreDamageEntityAttachParent)
				pProjectileObject->SetIgnoreDamageEntityAttachParent(true);

			pProjectileObject->SetIgnoreDamageEntity(params.pIgnoreDamageEntity);

			Vector3 vDirection( vAdjustedEnd - VEC3V_TO_VECTOR3( pProjectileObject->GetTransform().GetPosition() ) );	
			vDirection.Normalize();		
			bool bAllowToSetOwnerAsNoCollision = params.pIgnoreCollisionEntity ? false : true;
			pProjectileObject->Fire( vDirection, params.fOverrideLifeTime, 0, true, params.bScriptControlled, params.bCommandFireSingleBullet, true, NULL, false, bAllowToSetOwnerAsNoCollision);
		}
	}

	// Do everything that needs to be done when a gun is fired
	DoWeaponFire(params.pFiringEntity, params.weaponMatrix, vAdjustedEnd, params.iVehicleWeaponIndex, params.iVehicleWeaponBoneIndex, 1, params.fFireAnimRate, true, false, params.bAllowRumble);
	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CWeapon::FireVolumetric(CEntity* pFiringEntity, const Matrix34& weaponMatrix, s32 iVehicleWeaponIndex, s32 iVehicleWeaponBoneIndex, const Vector3& /*vStart*/, const Vector3& vEnd, float fFireAnimRate)
{
	weaponDebugf3("CWeapon::FireVolumetric");

	if(m_State != STATE_READY && !NetworkUtils::IsNetworkClone(pFiringEntity))
	{
		// Weapon not ready
		return false;
	}

	//hacky scaled ammo reduction dt scaling for use with volumetric weapons
	static const float fBaseTimeStepSeconds = 1.0f / 60.0f;
	const float fCurrentTimeStep = MAX( fwTimer::GetTimeStep(), fBaseTimeStepSeconds );
	s32 iAmmoToDeplete = (s32)Floorf((fCurrentTimeStep / fBaseTimeStepSeconds) + 0.5f);

	// Do everything that needs to be done when a gun is fired
	DoWeaponFire(pFiringEntity, weaponMatrix, vEnd, iVehicleWeaponIndex, iVehicleWeaponBoneIndex, iAmmoToDeplete, fFireAnimRate, true, false, true);
	return true;
}

////////////////////////////////////////////////////////////////////////////////

void CWeapon::DoWeaponFire(CEntity* pFiringEntity, const Matrix34& weaponMatrix, const Vector3& vEnd, s32 iVehicleWeaponIndex, s32 iVehicleWeaponBoneIndex, s32 iAmmoToDeplete, f32 fFireAnimRate, bool bFirstShot, bool BANK_ONLY(bBlank), bool bAllowRumble, bool bFiredFromAirDefence)
{
	weaponDebugf3("CWeapon::DoWeaponFire");

	// Cache the weapon info
	const CWeaponInfo* pWeaponInfo = GetWeaponInfo();

	DoWeaponFirePlayer(pFiringEntity, bAllowRumble);
	DoWeaponFirePhysics();

	// MN - these all use vEnd which is no longer the position where the bullet stops as the bullet processing hasn't been done yet
	DoWeaponFireAI(pFiringEntity, weaponMatrix, vEnd, bFirstShot);
	DoWeaponFireAudio(pFiringEntity, weaponMatrix, vEnd);

	// Vfx - muzzle flashes etc.
	
	bool bRenderMuzzleFlashAndShell = true;
	if(pFiringEntity && pFiringEntity->GetIsTypePed())
	{
		if(CTaskAimGunVehicleDriveBy::IsHidingGunInFirstPersonDriverCamera(static_cast<CPed*>(pFiringEntity)))
		{
			bRenderMuzzleFlashAndShell=false;
		}
	}

	if(bRenderMuzzleFlashAndShell)
	{
		if (bFiredFromAirDefence)
		{
			bool useAlternateFx = g_DrawRand.GetFloat() <= pWeaponInfo->GetBulletFlashAltPtFxChance();
			g_vfxWeapon.TriggerPtFxMuzzleFlash_AirDefence(this, MATRIX34_TO_MAT34V(weaponMatrix), useAlternateFx);
			g_vfxWeapon.DoWeaponFiredVfx(this);
		}
		else
		{
			DoWeaponMuzzle(pFiringEntity, iVehicleWeaponBoneIndex);
			g_vfxWeapon.DoWeaponFiredVfx(this);
		}

		// Vfx - shell ejects
		if (pWeaponInfo->GetForceEjectShellAfterFiring())
		{
			DoWeaponGunshell(pFiringEntity, iVehicleWeaponIndex, iVehicleWeaponBoneIndex);
		}
	}

	//Note: Disabled facial clips when firing for now, since the anim being used for this is way too long.
	//		It causes the facial clip to be run permanently.
	//		This should be re-enabled when we have a short clip for it.
	// ped facial animation
// 	if (pFiringEntity && pFiringEntity->GetIsTypePed() && pWeaponInfo->GetTimeBetweenShots() > 0.04)
// 	{
// 		CPed* pPed = static_cast<CPed*>(pFiringEntity);
// 
// 		if (pPed->GetFacialData())
// 		{
// 			pPed->GetFacialData()->PlayFacialAnim(pPed, FACIAL_CLIP_FIRE_WEAPON);
// 		}
// 	}


	// Update the ammo before we update the weapon state
	bool bUpdateAmmo = true;
	bool bIsNetworkClone = NetworkUtils::IsNetworkClone(pFiringEntity);

	if (pFiringEntity && pFiringEntity->GetIsTypePed())
	{
		CPed* pPed = static_cast<CPed*>(pFiringEntity);
		if (pPed)
		{
			if ((!pWeaponInfo->GetCanReload() && pPed->GetInventory()->GetIsWeaponUsingInfiniteAmmo(pWeaponInfo)))
			{
				bUpdateAmmo = false;
			}

			if (bIsNetworkClone && pPed->GetIsInVehicle())
				bUpdateAmmo = false;
		}
	}

	if (bUpdateAmmo)
	{
		UpdateAmmoAfterFiring(pFiringEntity, pWeaponInfo, iAmmoToDeplete);
	}

	// Update animation
	UpdateAnimationAfterFiring(pFiringEntity, fFireAnimRate);

	// Update the state
	UpdateStateAfterFiring(pFiringEntity);

	if(pFiringEntity)
	{
		camInterface::RegisterWeaponFire(*this, *pFiringEntity, bIsNetworkClone);
	}

	//! Add battle awareness if local player fires gun.
	if(pFiringEntity && pFiringEntity->GetIsTypePed())
	{
		CPed *pFiringPed = static_cast<CPed*>(pFiringEntity);
		if(pFiringPed->IsLocalPlayer() && !pWeaponInfo->GetIsNonViolent())
		{
			switch(pWeaponInfo->GetFireType())
			{
			case(FIRE_TYPE_INSTANT_HIT):
			case(FIRE_TYPE_DELAYED_HIT):
			case(FIRE_TYPE_PROJECTILE):
				{
					pFiringPed->GetPedIntelligence()->IncreaseBattleAwareness(CPedIntelligence::BATTLE_AWARENESS_GUNSHOT_LOCAL_PLAYER, true);
				}
				break;
			default:
				break;
			}
		}
	}

	// Debug
#if __BANK
	Vector3 vStart = weaponMatrix.d;

	// Draw a debug line to show the shot direction
	if(CPedDebugVisualiserMenu::GetDisplayDebugShooting())
	{
		static u32 DEBUG_SHOOTING_KEY = ATSTRINGHASH("DEBUG_SHOOTING_KEY", 0x2BDFD7B7);
		CPedAccuracy::ms_debugDraw.AddLine(RCC_VEC3V(vStart), RCC_VEC3V(vEnd), bBlank ? Color_red : Color_aquamarine, 100, DEBUG_SHOOTING_KEY);
	}
#endif // __BANK
}

////////////////////////////////////////////////////////////////////////////////

void CWeapon::StoppedFiring()
{
	m_FiringStartTime = 0;
}

////////////////////////////////////////////////////////////////////////////////

void CWeapon::DoWeaponFirePlayer(CEntity* pFiringEntity, bool bAllowRumble)
{
	// Only do player weapon code if shot originated from player
	if(!pFiringEntity)
	{
		return;
	}

	const bool bIsLocalPlayerPed = pFiringEntity->GetIsTypePed() && static_cast<CPed*>(pFiringEntity)->IsLocalPlayer();
	const bool bDriverIsLocalPlayer = pFiringEntity->GetIsTypeVehicle() && static_cast<CVehicle*>(pFiringEntity)->GetDriver() && static_cast<CVehicle*>(pFiringEntity)->GetDriver()->IsLocalPlayer();

	CPed* pPlayerPed = NULL;
	if( bIsLocalPlayerPed && m_pWeaponInfo )
	{
		pPlayerPed = static_cast<CPed*>(pFiringEntity);
		if(!GetIsSilenced() && (m_pWeaponInfo->GetDamageType() == DAMAGE_TYPE_BULLET || m_pWeaponInfo->GetDamageType() == DAMAGE_TYPE_BULLET_RUBBER))
		{
			CCrime::ReportCrime(CRIME_FIREARM_DISCHARGE, NULL, pPlayerPed);
		}
		else if (m_pWeaponInfo->GetEffectGroup() == WEAPON_EFFECT_GROUP_GRENADE)
		{
			const CAmmoProjectileInfo* pAmmoInfo = static_cast<const CAmmoProjectileInfo*>(m_pWeaponInfo->GetAmmoInfo());
			if(!pAmmoInfo || !pAmmoInfo->GetIsSticky())
			{
				CCrime::ReportCrime(CRIME_THROW_GRENADE, NULL, pPlayerPed);
			}
		}
		else if (m_pWeaponInfo->GetEffectGroup() == WEAPON_EFFECT_GROUP_MOLOTOV)
			CCrime::ReportCrime(CRIME_MOLOTOV, NULL, pPlayerPed);
	}
	else if( bDriverIsLocalPlayer )
		pPlayerPed = static_cast<CVehicle*>(pFiringEntity)->GetDriver();
	else
		return;

	if (bIsLocalPlayerPed || bDriverIsLocalPlayer)
	{
		CStatsMgr::PlayerFireWeapon(this);
	}

	//don't reduce street population if player is firing a flying vehicle, or is firing from a flying vehicle
	bool bPlayerInsideFlyingVehicle = false;
	if(bIsLocalPlayerPed || bDriverIsLocalPlayer)
	{
		CVehicle* pVehicle = bIsLocalPlayerPed ? static_cast<CPed*>(pFiringEntity)->GetVehiclePedInside() : static_cast<CVehicle*>(pFiringEntity);
		if(pVehicle && pVehicle->IsInAir() && (pVehicle->InheritsFromPlane() || pVehicle->InheritsFromHeli()))
		{
			bPlayerInsideFlyingVehicle = true;
		}
	}

	if(!bPlayerInsideFlyingVehicle)
	{
		CVehiclePopulation::MakeStreetsALittleMoreDeserted();
	}

	if (bAllowRumble)
	{
		const u32 curTime = fwTimer::GetTimeInMilliseconds();
		u32 initRumbleDur = GetWeaponInfo()->GetInitialRumbleDuration();
		u32 rumbleDur = GetWeaponInfo()->GetRumbleDuration();
		float initRumbleIntensity = GetWeaponInfo()->GetInitialRumbleIntensity();
		float rumbleIntensity = GetWeaponInfo()->GetRumbleIntensity();
	#if FPS_MODE_SUPPORTED
		if (pPlayerPed->IsFirstPersonShooterModeEnabledForPlayer(false))
		{
			// Replace with first person values, if they have been set.
			if (GetWeaponInfo()->GetFirstPersonInitialRumbleDuration() != 0)		{ initRumbleDur = GetWeaponInfo()->GetFirstPersonInitialRumbleDuration();			}
			if (GetWeaponInfo()->GetFirstPersonRumbleDuration() != 0)				{ rumbleDur = GetWeaponInfo()->GetFirstPersonRumbleDuration();						}
			if (GetWeaponInfo()->GetFirstPersonInitialRumbleIntensity() != 0.0f)	{ initRumbleIntensity = GetWeaponInfo()->GetFirstPersonInitialRumbleIntensity();	}
			if (GetWeaponInfo()->GetFirstPersonRumbleIntensity() != 0.0f)			{ rumbleIntensity = GetWeaponInfo()->GetFirstPersonRumbleIntensity();				}
		}
	#endif

		if (m_FiringStartTime == 0)
		{
			// just started firing again
			m_FiringStartTime = curTime;
		}

		if (curTime < (m_FiringStartTime + initRumbleDur))
		{
			CControlMgr::StartPlayerPadShakeByIntensity(initRumbleDur, initRumbleIntensity);
		}
		else if (rumbleDur > 0)
		{
			CControlMgr::StartPlayerPadShakeByIntensity(rumbleDur, rumbleIntensity);
		}
	}

	// Keep track of when we last fired
	if(!m_pWeaponInfo->GetIsVehicleWeapon())
	{
		pPlayerPed->GetWeaponManager()->SetLastEquipOrFireTime(fwTimer::GetTimeInMilliseconds());
	}

#if FPS_MODE_SUPPORTED
	if (pPlayerPed->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		CTaskPlayerOnFoot* pTaskPlayer = static_cast<CTaskPlayerOnFoot*>(pPlayerPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PLAYER_ON_FOOT));

		if (pTaskPlayer && pTaskPlayer->GetFirstPersonIkHelper())
		{
			pTaskPlayer->GetFirstPersonIkHelper()->OnFireEvent(pPlayerPed, m_pWeaponInfo);
		}
	}
	else if(pPlayerPed->IsInFirstPersonVehicleCamera())
	{
		CTaskAimGunVehicleDriveBy* pTaskDriveby = static_cast<CTaskAimGunVehicleDriveBy*>(pPlayerPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_VEHICLE_DRIVE_BY));
		if (pTaskDriveby && pTaskDriveby->GetFirstPersonIkHelper())
		{
			pTaskDriveby->GetFirstPersonIkHelper()->OnFireEvent(pPlayerPed, m_pWeaponInfo);
		}
	}
#endif // FPS_MODE_SUPPORTED
}

////////////////////////////////////////////////////////////////////////////////

void CWeapon::DoWeaponFirePhysics()
{
	if(GetWeaponInfo()->GetIsGun())
	{
		if(m_pDrawableEntity && m_pDrawableEntity->GetIsTypeObject())
		{
			CObject* pDrawableObject = static_cast<CObject*>(m_pDrawableEntity.Get());
			if(pDrawableObject->IsCollisionEnabled())
			{
				if(weaponVerifyf(pDrawableObject->GetCurrentPhysicsInst(), "Weapon %s is missing physics", pDrawableObject->GetModelName()))
				{
					if(weaponVerifyf(pDrawableObject->GetCurrentPhysicsInst()->IsInLevel(), "Gun object not in level"))
					{
						Matrix34 muzzleMatrix;
						GetMuzzleMatrix(MAT34V_TO_MATRIX34(pDrawableObject->GetMatrix()), muzzleMatrix);

						static dev_float APPLY_FORCE_TO_WEAPON = 0.05f;
						float fForce = rage::Min(5.0f, APPLY_FORCE_TO_WEAPON * GetWeaponInfo()->GetForce()) * pDrawableObject->GetMass();

						pDrawableObject->ApplyImpulse(-muzzleMatrix.a * fForce, Vector3(0.0f, 0.0f, 0.0f));
					}
				}
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CWeapon::DoWeaponFireAI(CEntity* pFiringEntity, const Matrix34& weaponMatrix, const Vector3& vEnd, bool bFirstShot)
{
	const bool bSilent = GetIsSilenced();
	const bool bThrownWeapon = GetWeaponInfo()->GetIsThrownWeapon();
	CPed* pFiringPed = (pFiringEntity && pFiringEntity->GetIsTypePed()) ? static_cast<CPed*>(pFiringEntity) : NULL;
	bool bBlockFiringEvents = bThrownWeapon || (pFiringPed && pFiringPed->GetPedResetFlag(CPED_RESET_FLAG_SupressGunfireEvents));
	bool bLimitPerceptionOfFiringEvents = false;

	if (pFiringPed)
	{
		// Hard limit offscreen shooters to a distance of 45m.
		// This is done because it can look strange to the player when someone reacts to a shooter who is far away and offscreen (and
		// unlikely to be noticed).
		if (pFiringPed->PopTypeIsRandom() && !pFiringPed->GetIsVisibleInSomeViewportThisFrame())
		{
			bLimitPerceptionOfFiringEvents = true;
		}
	}

	const CWeaponInfo& weapInfo = *GetWeaponInfo();
	float overrideRange = weapInfo.GetAiSoundRange();

	weaponDebugf3("CWeapon::DoWeaponFireAI bBlockFiringEvents[%d]",bBlockFiringEvents);

	if(!bBlockFiringEvents)
	{
		// Only add fire events once in a frame
		if(bFirstShot)
		{
			weaponDebugf3("CWeapon::DoWeaponFireAI bFirstShot[%d]",bFirstShot);
			// Make sure this weapon isn't suppressing gunshot events
			if(!weapInfo.GetSuppressGunshotEvent())
			{
				float gunshotEventRange = Selectf(overrideRange, overrideRange, LARGE_FLOAT);

				if (bLimitPerceptionOfFiringEvents)
				{
					gunshotEventRange = Min(gunshotEventRange, sf_OffscreenRandomPerceptionOverrideRange);
				}

				// These are still generated for non-lethal weapons.  The stungun has a relatively small auditory range, so only peds really close by to the shot
				// will respond to the shooter.
				weaponDebugf3("CWeapon::DoWeaponFireAI invoke CEventGunShot");
				CEventGunShot eventGunShot(pFiringEntity, weaponMatrix.d, vEnd, bSilent, GetWeaponHash(), gunshotEventRange);
				GetEventGlobalGroup()->Add(eventGunShot);  
			}

			if(pFiringEntity)
			{
				// Note: we used to call GetWeaponHash() here and pass that in with the shocking event.
				// However, CShockingEventsManager ignored it, only for the VisibleWeapon event was it used.

				// Don't do shocking events for non-lethal weapons like the stungun or silent weapons.
				weaponDebugf3("CWeapon::DoWeaponFireAI GetIsNonLethal[%d] bSilent[%d]",GetWeaponInfo()->GetIsNonLethal(),bSilent);
				if (!GetWeaponInfo()->GetIsNonLethal() && !bSilent)
				{
					weaponDebugf3("CWeapon::DoWeaponFireAI invoke CEventShockingGunshotFired");
					CEventShockingGunshotFired ev(*pFiringEntity);
					
					if (bLimitPerceptionOfFiringEvents)
					{
						ev.SetAudioReactionRangeOverride(sf_OffscreenRandomPerceptionOverrideRange);
						ev.SetVisualReactionRangeOverride(sf_OffscreenRandomPerceptionOverrideRange);
					}

					CShockingEventsManager::Add(ev);
				}

				// If the firing ped has a player info, try to shout our target's position
				if(pFiringPed && pFiringPed->GetPlayerInfo())
				{
					pFiringPed->GetPlayerInfo()->ShoutTargetPosition();
				}
			}

			if (weapInfo.CreatesAPotentialExplosionEventWhenFired() && pFiringEntity)
			{
				weaponDebugf3("CWeapon::DoWeaponFireAI invoke CEventShockingPotentialBlast");
				CEventShockingPotentialBlast ev(*pFiringEntity);
				ev.SetVisualReactionRangeOverride(weapInfo.GetPotentialBlastEventRange());
				ev.SetAudioReactionRangeOverride(weapInfo.GetPotentialBlastEventRange());
				CShockingEventsManager::Add(ev);
			}
		}
	}

	if(pFiringEntity)
	{
		CPed* pFiringPed = NULL;
		if( pFiringEntity->GetIsTypePed() )
		{
			pFiringPed = (CPed*)pFiringEntity;
		}
		else if(pFiringEntity->GetIsTypeVehicle())
		{
			pFiringPed = ((CVehicle*)pFiringEntity)->GetDriver();
		}

		if(pFiringPed)
		{
			if (GetWeaponInfo()->GetIsProjectile() && GetWeaponInfo()->GetProjectileFuseTime()<0.0f)
				pFiringPed->SetPedResetFlag( CPED_RESET_FLAG_PlacingCharge, true );
			pFiringPed->SetPedResetFlag( CPED_RESET_FLAG_FiringWeapon, true );
		}
	}

	// Don't register projectiles as fired till they reach the ground
	if(!weapInfo.GetIsProjectile() && bFirstShot)
	{
		// Register this as an 'interesting event'
		g_InterestingEvents.Add(CInterestingEvents::EGunshotFired, pFiringEntity);
	}

	if((pFiringPed && pFiringPed->IsPlayer()))
	{
		float stealthBlipNoiseRange = Selectf(overrideRange, overrideRange, CMiniMap::sm_Tunables.Sonar.fSoundRange_Gunshot);

		bool bCreateSonarBlip = false;
		if( (m_uLastStealthBlipTime + CMiniMap::fMinTimeBetweenBlipsMS) < fwTimer::GetTimeInMilliseconds() && pFiringPed->IsLocalPlayer() && !m_pWeaponInfo->GetIsTurret())
		{
			bCreateSonarBlip = true;
			m_uLastStealthBlipTime = fwTimer::GetTimeInMilliseconds();
		}

		if (CMiniMap::ms_bUseWeaponRangeForMPSonarDistance)
		{
			if (!NetworkInterface::IsGameInProgress())
			{
				if (!bSilent)
				{
					CMiniMap::CreateSonarBlipAndReportStealthNoise(pFiringPed, VECTOR3_TO_VEC3V(weaponMatrix.d), stealthBlipNoiseRange, HUD_COLOUR_RED, bCreateSonarBlip);
				}
			}
			else
			{
				if (bSilent)
				{
					static float sfMPSilenceRange = 10.f;
					CMiniMap::CreateSonarBlipAndReportStealthNoise(pFiringPed, VECTOR3_TO_VEC3V(weaponMatrix.d), sfMPSilenceRange, HUD_COLOUR_RED, bCreateSonarBlip);
				}
				else
				{
					const static float sfMPWeaponRangeMultiplier = 2.5f;
					const static float sfMaxMPRange = 400.f;

					float fRange = weapInfo.GetRange();
					fRange *= sfMPWeaponRangeMultiplier;
					fRange = rage::Min(fRange, sfMaxMPRange);

					CMiniMap::CreateSonarBlipAndReportStealthNoise(pFiringPed, VECTOR3_TO_VEC3V(weaponMatrix.d), fRange, HUD_COLOUR_RED, bCreateSonarBlip);
				}
			}
		}
		else if (!bSilent)
		{
			CMiniMap::CreateSonarBlipAndReportStealthNoise(pFiringPed, VECTOR3_TO_VEC3V(weaponMatrix.d), stealthBlipNoiseRange, HUD_COLOUR_RED, bCreateSonarBlip);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CWeapon::DoWeaponFireAudio(CEntity* pFiringEntity, const Matrix34& weaponMatrix, const Vector3& vEnd)
{
	const bool bIsPetrolCan = GetWeaponInfo() && GetWeaponInfo()->GetGroup() == WEAPONGROUP_PETROLCAN;
	if(!bIsPetrolCan)
	{
		// Treat stungun as silenced so we don't get loud sound triggers etc.
		m_WeaponAudioComponent.PlayWeaponFire(pFiringEntity, GetIsSilenced() || GetWeaponHash() == ATSTRINGHASH("WEAPON_STUNGUN", 0x3656C8C1));

		if(!GetWeaponInfo()->GetIsProjectile() && GetWeaponHash() != ATSTRINGHASH("WEAPON_FIREEXTINGUISHER", 0x60EC506))
		{
			// Trigger bullet bys
			g_WeaponAudioEntity.PlayBulletBy(GetWeaponInfo()->GetAudioHash(), weaponMatrix.d, vEnd);
		}
	}
}

void CWeapon::DoWeaponFireKillShotAudio(CEntity* pFiringEntity, bool isKillShot)
{
	if(GetWeaponInfo()->GetIsGun())
	{
		// Treat stungun as silenced so we don't get loud sound triggers etc.
		m_WeaponAudioComponent.PlayWeaponFire(pFiringEntity, GetIsSilenced() || GetWeaponHash() == ATSTRINGHASH("WEAPON_STUNGUN", 0x3656C8C1), true, isKillShot);
	}
}


////////////////////////////////////////////////////////////////////////////////

bool CWeapon::ComputeBulletBending(CEntity* pFiringEntity, const Vector3& vStart, Vector3& vAdjustedEnd, float fWeaponRange) const
{
	if (!CPlayerPedTargeting::ms_Tunables.GetEnableBulletBending())
	{
		return false;
	}

	if(pFiringEntity && pFiringEntity->GetIsTypePed() && static_cast<CPed*>(pFiringEntity)->IsLocalPlayer())
	{
		CPed* pFiringPlayer = static_cast<CPed*>(pFiringEntity);

#if FPS_MODE_SUPPORTED
		//! Don't do any projectile bullet bending in 1st person (including driveby)
		if((pFiringPlayer->IsFirstPersonShooterModeEnabledForPlayer(false) || pFiringPlayer->IsInFirstPersonVehicleCamera()) && m_pWeaponInfo->GetIsProjectile())
		{
			return false;
		}
#endif

		const CPlayerPedTargeting & targeting = pFiringPlayer->GetPlayerInfo()->GetTargeting();
		const CEntity* pFreeAimAssistTarget = targeting.GetFreeAimAssistTarget();
		if(pFreeAimAssistTarget && !targeting.GetFreeAimTargetRagdoll())
		{
			bool bIsTargetFriendly = false;
			if(pFreeAimAssistTarget->GetIsTypePed())
			{
				const CPed* pFreeAimAssistTargetPed = static_cast<const CPed*>(pFreeAimAssistTarget);

				// B*2103351: Don't do bullet bending if target ped is in the same vehicle as the firing ped.
				if (pFiringPlayer->GetIsInVehicle() && pFreeAimAssistTargetPed->GetIsInVehicle() && (pFiringPlayer->GetVehiclePedInside() == pFreeAimAssistTargetPed->GetVehiclePedInside()))
				{
					return false;
				}

				if(pFiringPlayer->GetPedIntelligence()->IsFriendlyWith(*pFreeAimAssistTargetPed))
				{
					bIsTargetFriendly = true;
				}
			}

			// We only need to set the adjusted end point to the free target position since this is all
			// done in CPlayerPedTargeting::GetMoreAccurateFreeAimAssistTargetPos
			if(!bIsTargetFriendly)
			{
				bool bAccurateMode = CPlayerPedTargeting::IsCameraInAccurateMode();

				if(targeting.GetIsFreeAimAssistTargetPosWithinConstraints(this, vStart, vAdjustedEnd, bAccurateMode))
				{
					// Scale the adjusted vector in the direction of the bent shot
					Vector3 vDir = targeting.GetFreeAimAssistTargetPos() - vStart;
					vDir.NormalizeSafe();
					vDir.Scale( fWeaponRange );

					vAdjustedEnd = vStart + vDir;

					return true;
				}
			}
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CWeapon::ComputeAccuracy(CEntity* pFiringEntity, const CEntity* pTargetEntity, Vec3V_In vStart, Vec3V_InOut vEnd, sWeaponAccuracy& weaponAccuracy, bool bSetPerfectAccuracy, float fDesiredTargetDist, bool bIsBlindFire, eAnimBoneTag nForcedRandomHitBone )
{
	// If we are within a certain distance, alter the firing so shots don't look odd
	TUNE_GROUP_FLOAT(AIMING_TUNE, RESTRICTED_BOUNDING_BOX_DIST_MIN, 3.0f, 0.0f, 120.0f, 0.1f);
	TUNE_GROUP_FLOAT(AIMING_TUNE, RESTRICTED_BOUNDING_BOX_DIST_MAX, 20.0f, 0.0f, 120.0f, 0.1f);
	weaponAssertf(RESTRICTED_BOUNDING_BOX_DIST_MIN < RESTRICTED_BOUNDING_BOX_DIST_MAX, "RESTRICTED_BOUNDING_BOX_DIST_MIN %.2f, has to be less than RESTRICTED_BOUNDING_BOX_DIST_MAX %.2f", RESTRICTED_BOUNDING_BOX_DIST_MIN, RESTRICTED_BOUNDING_BOX_DIST_MAX);
	const bool bRestrictedBoundingBox = fDesiredTargetDist < RESTRICTED_BOUNDING_BOX_DIST_MAX;

	if (VEC3V_TO_VECTOR3(vStart).IsEqual(VEC3V_TO_VECTOR3(vEnd))) //placed object
		return true;

	CPed* pFiringPed = NULL;
	if(pFiringEntity && pFiringEntity->GetIsTypeVehicle())
	{
		pFiringPed = ((CVehicle*)pFiringEntity)->GetDriver();
	}
	else if(pFiringEntity && pFiringEntity->GetIsTypePed())
	{
		pFiringPed = (CPed*)pFiringEntity;
	}

	if(!bSetPerfectAccuracy)
	{
		GetAccuracy(pFiringPed, fDesiredTargetDist, weaponAccuracy);
	}

	bool bIgnoreSpread = false;

#if __BANK
	static dev_bool s_bTEST_ALWAYS_MISS = false;
	if(s_bTEST_ALWAYS_MISS)
		weaponAccuracy.bShouldAlwaysMiss = true;
#endif // __BANK

	// Apply random miss chance (not to players)
	f32 fRandomMiss = NetworkInterface::IsNetworkOpen() ? g_ReplayRand.GetRanged(0.0f, 1.0f) : fwRandom::GetRandomNumberInRange(0.0f, 1.0f); //moved this out so the same # of ranged calc's will occur and the randoms will match up
	if(!pFiringPed || !pFiringPed->IsLocalPlayer())
	{
		if(!pTargetEntity)
		{
			// Only when we have a target
			weaponAccuracy.bShouldAlwaysMiss = false;
		}
		else if(!weaponAccuracy.bShouldAlwaysMiss)
		{
			if(fRandomMiss > weaponAccuracy.fAccuracy)
			{
				weaponAccuracy.bShouldAlwaysMiss = true;
			}
			else
			{
				// Otherwise lets ignore the weapon spread since spread at large distances 
				// will result in consistent misses
				bIgnoreSpread = true;
			}
		}
	}

	Vec3V vShot = vEnd - vStart;
	ScalarV vShotRange = Mag(vShot);
	const float fShotRange = vShotRange.Getf();

	if(!weaponAccuracy.bShouldAlwaysMiss)
	{
		// Regular cone accuracy - for player and AI without a target, or with a non ped target, or we have a restricted bounding box
		if((pFiringPed && pFiringPed->IsPlayer()) || !pTargetEntity || !pTargetEntity->GetIsTypePed() || bRestrictedBoundingBox)
		{
			if(fShotRange > 0.1f)
			{
				vShot /= vShotRange;
			}

			Vec3V vRight, vUp;
			vRight = CrossZAxis(vShot);
			vRight = NormalizeFast(vRight);
			vUp = Cross(vRight, vShot);
			vUp = NormalizeFast(vUp);

			// Invert the accuracy before applying spread
			float fAccuracy = 1.0f - weaponAccuracy.fAccuracy;
			float fSpread = bIgnoreSpread ? 1.0f : GetWeaponInfo()->GetAccuracySpread();
			if (bIsBlindFire)
				fSpread = Max(ms_fMinBlindFireSpread, fSpread*ms_fBlindFireSpreadModifier);

			// Make weapons more easily tunable by normalizing the spread at the same distance
			float fScaledRange = fShotRange / STANDARD_WEAPON_ACCURACY_RANGE;
			fSpread *= fScaledRange;

#if __BANK
			if(CPedDebugVisualiserMenu::GetDisplayDebugAccuracy())
			{
				static const u32 DEBUG_ACCURACY_CONE_1_KEY = ATSTRINGHASH("DEBUG_ACCURACY_CONE_1_KEY", 0x8DB34EBA);
				const float fDebugSpreadRange = fShotRange * GetWeaponInfo()->GetBatchSpread();
				float fRadius = ((1.0f - weaponAccuracy.fAccuracyMin) * fSpread) + fDebugSpreadRange;
				CPedAccuracy::ms_debugDraw.AddCone(vStart, vEnd, fRadius, Color_orange, true, false, 12, 1000, DEBUG_ACCURACY_CONE_1_KEY);
			}
#endif	//__BANK

			float fRotAngle = NetworkInterface::IsNetworkOpen() ? g_ReplayRand.GetRanged(0.0f, TWO_PI) : fwRandom::GetRandomNumberInRange(0.0f, TWO_PI);
			vEnd += vRight * ScalarVFromF32(fAccuracy * fSpread * rage::Sinf(fRotAngle));
			vEnd += vUp * ScalarVFromF32(fAccuracy * fSpread * rage::Cosf(fRotAngle));
		}
		else
		{
			// Pick a bone to hit
			static const int NUM_HIT_BONES = 16;
			static const eAnimBoneTag boneIds[NUM_HIT_BONES] =
			{
				BONETAG_NECK,
				BONETAG_HEAD,
				BONETAG_SPINE0,
				BONETAG_SPINE1,
				BONETAG_SPINE2,
				BONETAG_SPINE3,
				BONETAG_R_CLAVICLE,
				BONETAG_R_UPPERARM,
				BONETAG_R_FOREARM,
				BONETAG_R_THIGH,
				BONETAG_R_CALF,
				BONETAG_L_CLAVICLE,
				BONETAG_L_UPPERARM,
				BONETAG_L_FOREARM,
				BONETAG_L_THIGH,
				BONETAG_L_CALF,
			};

			eAnimBoneTag nHitBone = nForcedRandomHitBone;
			if(nHitBone == BONETAG_INVALID)
			{
				int iHitBone = NetworkInterface::IsNetworkOpen() ? g_ReplayRand.GetRanged(0, NUM_HIT_BONES-1) : fwRandom::GetRandomNumberInRange(0, NUM_HIT_BONES-1);
				nHitBone = boneIds[iHitBone];
			}
			
			int iBoneIndex = static_cast<const CPed*>(pTargetEntity)->GetBoneIndexFromBoneTag(nHitBone);
			if(iBoneIndex == -1)
				iBoneIndex = 0;
			
			Matrix34 mBone;
			pTargetEntity->GetGlobalMtx(iBoneIndex, mBone);

			Vec3V vDirection(Subtract(VECTOR3_TO_VEC3V(mBone.d), vStart));
			vDirection = Normalize(vDirection);

			Vec3V vOldDirection(Subtract(vEnd, vStart));
			ScalarV vLength(Mag(vOldDirection));
			vEnd = Add(vStart, Scale(vDirection, vLength));
#if __BANK
			if(CPedDebugVisualiserMenu::GetDisplayDebugAccuracy())
			{	
				CPedAccuracy::ms_debugDraw.AddSphere(vEnd, 0.05f, Color_orange, 1000);
			}
#endif	//__BANK
		}

		// Reset the history
		m_LastOffsetPosition.ZeroComponents();

#if __BANK
		if(m_pDebugData)
		{
			m_pDebugData->m_iAccuracyAccurateShots++;
		}
#endif // __BANK

		return true;
	}

	if(pTargetEntity)
	{
		// Find the longest extent (which we will plan to randomize)
		bool bHeightIsLongestExtent = false;

		Vec3V vBoundingBoxMin = VECTOR3_TO_VEC3V(pTargetEntity->GetBoundingBoxMin());
		Vec3V vBoundingBoxMax = VECTOR3_TO_VEC3V(pTargetEntity->GetBoundingBoxMax());

		// Don't do this for tanks etc, they miss by too much and it looks odd.
		CVehicle* pVehicle = NULL;
		if(pFiringPed)
		{
			pVehicle = pFiringPed->GetVehiclePedInside();
		}

		// Transform the bounding box into the firing ped space, so we can easily work out the miss vectors
		static const int NUM_CORNERS = 8;
		Vec3V vCorners[NUM_CORNERS] = 
		{
			vBoundingBoxMin, 
			Vec3V(vBoundingBoxMax.GetX(), vBoundingBoxMin.GetY(), vBoundingBoxMin.GetZ()), 
			Vec3V(vBoundingBoxMax.GetX(), vBoundingBoxMax.GetY(), vBoundingBoxMin.GetZ()), 
			Vec3V(vBoundingBoxMin.GetX(), vBoundingBoxMax.GetY(), vBoundingBoxMin.GetZ()),
			vBoundingBoxMax, 
			Vec3V(vBoundingBoxMax.GetX(), vBoundingBoxMin.GetY(), vBoundingBoxMax.GetZ()), 
			Vec3V(vBoundingBoxMax.GetX(), vBoundingBoxMax.GetY(), vBoundingBoxMax.GetZ()), 
			Vec3V(vBoundingBoxMin.GetX(), vBoundingBoxMax.GetY(), vBoundingBoxMax.GetZ()),
		};

		Mat34V mTargetEntity(pTargetEntity->GetTransform().GetMatrix());
		Mat34V mFiringEntity;

		// Construct matrix from firing vector
		Vec3V vDir(vEnd - vStart);
		vDir = Normalize(vDir);
		mFiringEntity.Setb(vDir);
		mFiringEntity.Seta(Cross(Vec3V(V_Z_AXIS_WZERO), mFiringEntity.b()));
		mFiringEntity.Setc(Cross(mFiringEntity.b(), mFiringEntity.a()));
		mFiringEntity.Setd(vStart);

		for(s32 i = 0; i < NUM_CORNERS; i++)
		{
			vCorners[i] = Transform(mTargetEntity, vCorners[i]);
			vCorners[i] = UnTransformFull(mFiringEntity, vCorners[i]);
		}

		// vMin/vMax will be in local firing entity space
		Vec3V vMin(vCorners[0]);
		Vec3V vMax(vCorners[0]);
		for(s32 i = 1; i < NUM_CORNERS; i++)
		{
			if(IsLessThan(vCorners[i].GetX(), vMin.GetX()).Getb())
			{
				vMin.SetX(vCorners[i].GetX());
			}
			else if(IsGreaterThan(vCorners[i].GetX(), vMax.GetX()).Getb())
			{
				vMax.SetX(vCorners[i].GetX());
			}

			// Only interested in vMin.y
			if(IsLessThan(vCorners[i].GetY(), vMin.GetY()).Getb())
			{
				vMin.SetY(vCorners[i].GetY());
				vMax.SetY(vCorners[i].GetY());
			}

			if(IsLessThan(vCorners[i].GetZ(), vMin.GetZ()).Getb())
			{
				vMin.SetZ(vCorners[i].GetZ());
			}
			else if(IsGreaterThan(vCorners[i].GetZ(), vMax.GetZ()).Getb())
			{
				vMax.SetZ(vCorners[i].GetZ());
			}
		}

		// Increase miss amount for objects that explode
		if(GetWeaponInfo()->GetDamageType() == DAMAGE_TYPE_EXPLOSIVE && !pVehicle)
		{
			static float s_ExplosionRadius = 2.0f;
			Vec3V vExplosionPadding(s_ExplosionRadius, 0.f, s_ExplosionRadius);
			vMin -= vExplosionPadding;
			vMax += vExplosionPadding;
		}

#if __BANK
		if(CPedDebugVisualiserMenu::GetDisplayDebugAccuracy())
		{
			static const u32 DEBUG_ACCURACY_BOX = ATSTRINGHASH("DEBUG_ACCURACY_BOX", 0x56DBBED4);
			CPedAccuracy::ms_debugDraw.AddOBB(vMin, vMax, mFiringEntity, Color_blue, 1000, DEBUG_ACCURACY_BOX);
		}
#endif	//__BANK

		Vec3V vTargetExtents(vMax - vMin);
		if(IsGreaterThan(vTargetExtents.GetZ(), vTargetExtents.GetX()).Getb())
		{
			bHeightIsLongestExtent = true;
		}

		// Determine the relative offset where we are supposed to hit
		Vec3V vOffsetPosition(V_ZERO);

		// Check to see if we have a valid last missed bullet
		TUNE_GROUP_INT(AIMING_TUNE, BURST_FIRE_TIME_BETWEEN_SHOTS_MAX, 500, 0, 1000, 1);
		u32 uTimer = fwTimer::GetTimeInMilliseconds() - m_uLastShotTimer;
		if(IsZeroNone(m_LastOffsetPosition) && uTimer < BURST_FIRE_TIME_BETWEEN_SHOTS_MAX)
		{
			vOffsetPosition = m_LastOffsetPosition;

			TUNE_GROUP_FLOAT(AIMING_TUNE, BURST_FIRE_PRIMARY_OFFSET, 0.1f, 0.0f, 1.0f, 0.01f);
			TUNE_GROUP_FLOAT(AIMING_TUNE, BURST_FIRE_PRIMARY_RESTRICTED_OFFSET, 0.05f, 0.0f, 1.0f, 0.01f);
			TUNE_GROUP_FLOAT(AIMING_TUNE, BURST_FIRE_SECONDARY_OFFSET, 0.05f, 0.0f, 1.0f, 0.01f);

			const float fRandomPrimaryOffset = NetworkInterface::IsNetworkOpen() ? g_ReplayRand.GetRanged(0.f, bRestrictedBoundingBox ? BURST_FIRE_PRIMARY_RESTRICTED_OFFSET : BURST_FIRE_PRIMARY_OFFSET) : fwRandom::GetRandomNumberInRange(0.f, bRestrictedBoundingBox ? BURST_FIRE_PRIMARY_RESTRICTED_OFFSET : BURST_FIRE_PRIMARY_OFFSET);
			const float fRandomSecondaryOffset = NetworkInterface::IsNetworkOpen() ? g_ReplayRand.GetRanged(0.f, BURST_FIRE_SECONDARY_OFFSET) : fwRandom::GetRandomNumberInRange(0.f, BURST_FIRE_SECONDARY_OFFSET);
			if(bHeightIsLongestExtent)
			{
				vOffsetPosition.SetZ(vOffsetPosition.GetZf() + fRandomPrimaryOffset);

				float fMidPoint = (vMax.GetXf() + vMin.GetXf()) * 0.5f;
				if(m_LastOffsetPosition.GetXf() < fMidPoint)
				{
					float fX = m_LastOffsetPosition.GetXf() - fRandomSecondaryOffset;
					if(!bRestrictedBoundingBox)
						fX = Min(fX, vMin.GetXf());
					vOffsetPosition.SetX(fX);
				}
				else
				{
					float fX = m_LastOffsetPosition.GetXf() + fRandomSecondaryOffset;
					if(!bRestrictedBoundingBox)
						fX = Max(fX, vMax.GetXf());
					vOffsetPosition.SetX(fX);
				}
			}
			else
			{
				vOffsetPosition.SetX(vOffsetPosition.GetXf() + fRandomPrimaryOffset);

				float fMidPoint = (vMax.GetZf() + vMin.GetZf()) * 0.5f;
				if(m_LastOffsetPosition.GetXf() < fMidPoint)
				{
					float fZ = m_LastOffsetPosition.GetZf() - fRandomSecondaryOffset;
					if(!bRestrictedBoundingBox)
						fZ = Min(fZ, vMin.GetZf());
					vOffsetPosition.SetZ(fZ);
				}
				else
				{
					float fZ = m_LastOffsetPosition.GetZf() + fRandomSecondaryOffset;
					if(!bRestrictedBoundingBox)
						fZ = Max(fZ, vMax.GetZf());
					vOffsetPosition.SetZ(fZ);
				}
			}
			vOffsetPosition.SetY(vMin.GetY());
		}
		else
		{
			float fMinX, fMaxX, fMinZ, fMaxZ;
			if(bRestrictedBoundingBox)
			{
				// Convert the firing vector hit position into firing entity space
				Vec3V vLocalStart = UnTransformFull(mFiringEntity, vStart);
				Vec3V vLocalEnd = UnTransformFull(mFiringEntity, vEnd);
				Vec3V vLocalFiringVec = vLocalEnd - vLocalStart;
				Vec3V vLocalFiringPoint = Normalize(vLocalFiringVec);
				// This will restrict the point to be on the vMin Y plane
				vLocalFiringPoint *= ScalarV((vMin.GetYf() - vLocalStart.GetYf()));
				vLocalFiringPoint += vLocalStart;

				const float fDistRatio = Max(fDesiredTargetDist - RESTRICTED_BOUNDING_BOX_DIST_MIN, 0.f) / (RESTRICTED_BOUNDING_BOX_DIST_MAX - RESTRICTED_BOUNDING_BOX_DIST_MIN);
				const float fWidth = (vMax.GetXf() - vMin.GetXf()) * fDistRatio * 0.5f;
				const float fHeight = (vMax.GetZf() - vMin.GetZf()) * fDistRatio * 0.5f;

				fMinX = Max(vLocalFiringPoint.GetXf() - fWidth, vMin.GetXf());
				fMaxX = Min(vLocalFiringPoint.GetXf() + fWidth, vMax.GetXf());
				fMinZ = Max(vLocalFiringPoint.GetZf() - fHeight, vMin.GetZf());
				fMaxZ = Min(vLocalFiringPoint.GetZf() + fHeight, vMax.GetZf());

#if __BANK
				if(CPedDebugVisualiserMenu::GetDisplayDebugAccuracy())
				{	
					// Render firing vec
					CPedAccuracy::ms_debugDraw.AddLine(vStart, vEnd, Color_white, 1000);

					// Render firing point
					Vec3V vWorldFiringPoint = Transform(mFiringEntity, vLocalFiringPoint);
					CPedAccuracy::ms_debugDraw.AddSphere(vWorldFiringPoint, 0.05f, Color_white, 1000);

					// Render the restricted bound box
					Vec3V vRestrictedMin(vMin);
					vRestrictedMin.SetXf(fMinX);
					vRestrictedMin.SetZf(fMinZ);
					Vec3V vRestrictedMax(vMax);
					vRestrictedMax.SetXf(fMaxX);
					vRestrictedMax.SetZf(fMaxZ);

					static const u32 DEBUG_ACCURACY_BOX_RESTRICTED = ATSTRINGHASH("DEBUG_ACCURACY_BOX_RESTRICTED", 0x10FBB615);
					CPedAccuracy::ms_debugDraw.AddOBB(vRestrictedMin, vRestrictedMax, mFiringEntity, Color_orange, 1000, DEBUG_ACCURACY_BOX_RESTRICTED);
				}

				if(m_pDebugData)
				{
					m_pDebugData->m_iAccuracyRestrictedShots++;
				}
#endif	//__BANK
			}
			else
			{
				fMinX = vMin.GetXf();
				fMaxX = vMax.GetXf();
				fMinZ = vMin.GetZf();
				fMaxZ = vMax.GetZf();
			}

			bool bRandBool = NetworkInterface::IsNetworkOpen() ? g_ReplayRand.GetBool() : fwRandom::GetRandomTrueFalse();
			if(bHeightIsLongestExtent)
			{

				vOffsetPosition.SetZ(NetworkInterface::IsNetworkOpen() ? g_ReplayRand.GetRanged(fMinZ, fMaxZ) : fwRandom::GetRandomNumberInRange(fMinZ, fMaxZ));
				vOffsetPosition.SetX(bRandBool ? fMinX : fMaxX);
			}
			else
			{
				vOffsetPosition.SetX(NetworkInterface::IsNetworkOpen() ? g_ReplayRand.GetRanged(fMinX, fMaxX) : fwRandom::GetRandomNumberInRange(fMinX, fMaxX));
				vOffsetPosition.SetZ(bRandBool ? fMinZ : fMaxZ);
			}
			vOffsetPosition.SetY(vMin.GetY());
		}

		m_LastOffsetPosition = vOffsetPosition;
		m_uLastShotTimer = fwTimer::GetTimeInMilliseconds();

		// Transform offset into the targets local space and then add that to the current target position
		vEnd = Transform(mFiringEntity, vOffsetPosition);

		Vec3V vNewShot = vEnd - vStart;
		if(Dot(vShot, vNewShot).Getf() <= 0.f)
		{
			// Use the original vEnd if the resultant shot is going to go backwards
			vEnd = vStart + vShot;
		}

#if __BANK
		if(CPedDebugVisualiserMenu::GetDisplayDebugAccuracy())
		{	
			CPedAccuracy::ms_debugDraw.AddSphere(vEnd, 0.05f, Color_red, 1000);
		}
#endif	//__BANK
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CWeapon::UpdateAmmoAfterFiring(CEntity* pFiringEntity, const CWeaponInfo* pWeaponInfo, s32 iAmmoToDeplete)
{
	if(!GetHasInfiniteClips(pFiringEntity))
	{
		if( !NetworkInterface::IsGameInProgress() && pWeaponInfo && pFiringEntity && pFiringEntity->GetIsTypePed() )
		{
			CPed* pFiringPed = static_cast<CPed*>(pFiringEntity);
			weaponAssert( pFiringPed );

			// B*2061589: This seems a bit dodgy for 1-shot weapons (such as the railgun) as we don't always get a reload despite having fired,
			// due to the "GetAmmoInClip < GetClipSize" check returning false in CWeapon::CanReload. For now, just disabling this code if the weapon has a max clip size of 1.
			// We should probably re-visit this in the future, perhaps we should just scale the max clip size up as opposed to not decrementing the ammo in the clip.
			if( pFiringPed->GetPedConfigFlag( CPED_CONFIG_FLAG_UseDiminishingAmmoRate ) && pWeaponInfo->GetClipSize() > 1)
			{
				// decrement rate
				s32 nCurrentBulletCount = static_cast<CPed*>(pFiringEntity)->GetAmmoDiminishingCount();
				if( nCurrentBulletCount < pWeaponInfo->GetAmmoDiminishingRate() )
				{
					// Increment the count and return
					static_cast<CPed*>(pFiringEntity)->SetAmmoDiminishingCount( nCurrentBulletCount + iAmmoToDeplete );
					return;
				}
			}

			// Reset
			static_cast<CPed*>(pFiringEntity)->SetAmmoDiminishingCount( 0 );
		}

		// Decrement clip
		SetAmmoInClip(GetAmmoInClip()-iAmmoToDeplete);

		// Decrement total
		AddToAmmoTotal(-iAmmoToDeplete);	
	}
}

////////////////////////////////////////////////////////////////////////////////

void CWeapon::ProcessAnimation(CEntity* pFiringEntity)
{
	//Grab the firing ped.
	CPed* pFiringPed = NULL;
	if(pFiringEntity && pFiringEntity->GetIsTypePed())
	{
		pFiringPed = static_cast<CPed *>(pFiringEntity);
	}

	ProcessFpsSightDof(pFiringEntity);

	bool bDoPostCameraAnimUpdate = false;

	if(m_pDrawableEntity)
	{
		if(m_bCanActivateMoveNetwork)
		{
			fwMvClipSetId ClipSetId = pFiringPed ? GetWeaponInfo()->GetAppropriateWeaponClipSetId(pFiringPed) : GetWeaponInfo()->GetWeaponClipSetId();
			if(ClipSetId != CLIP_SET_ID_INVALID)
			{
				if(m_pMoveNetworkHelper)
				{
					if(m_pMoveNetworkHelper->IsNetworkActive())
					{
						if(ClipSetId != m_pMoveNetworkHelper->GetClipSetId())
						{
							fwClipSet* pClipSet = fwClipSetManager::GetClipSet(ClipSetId);
							if(pClipSet && pClipSet->Request_DEPRECATED())
							{
								// Set the clip set
								m_pMoveNetworkHelper->SetClipSet(ClipSetId);
							}
						}

						static const fwMvBooleanId s_ShellEjected("ShellEjected",0xB98A4092);
						static const fwMvBooleanId s_ShellEjectedClipNotEmpty("ShellEjectedClipNotEmpty",0x79455735);
						if(m_pMoveNetworkHelper->GetBoolean(s_ShellEjected) || (m_bReloadClipNotEmpty && m_pMoveNetworkHelper->GetBoolean(s_ShellEjectedClipNotEmpty)))
						{
							bool bRenderShell = true;
							if( pFiringPed )
							{
								if(CTaskAimGunVehicleDriveBy::IsHidingGunInFirstPersonDriverCamera(pFiringPed))
								{
									bRenderShell=false;
								}
							}

							if(bRenderShell)
							{
								DoWeaponGunshell(pFiringEntity);
							}

							m_bReloadClipNotEmpty = false;
						}
					}
				}

				float fIdleOnOffBlendDuration = SLOW_BLEND_DURATION;

				bool bIsRevolver = GetWeaponInfo() && GetWeaponInfo()->GetFlag(CWeaponInfoFlags::UseInstantAnimBlendFromFireToIdle);

				if ( m_pMoveNetworkHelper && ( (m_bHasWeaponIdleAnim && (m_pMoveNetworkHelper->GetBoolean(ms_WeaponFireEndedId) && !(GetState() == STATE_RELOADING && m_pWeaponInfo->GetIsIdleAnimationFilterDisabledWhenReloading()))) || m_bHasInVehicleWeaponIdleAnim ))
				{
					m_pMoveNetworkHelper->SetFlag(true, ms_WeaponIdleFlagId);

					// B*2586161: Revolver - Instantly blend the idle animation on when finished firing.
					if (bIsRevolver)
					{
						fIdleOnOffBlendDuration = INSTANT_BLEND_DURATION;
						bDoPostCameraAnimUpdate = true;
					}
				}
				
				// Prevent the idle animation from playing while reloading.
				if (bIsRevolver && m_pMoveNetworkHelper && m_bHasWeaponIdleAnim && GetState() == STATE_RELOADING)
				{
					m_pMoveNetworkHelper->SetFlag(false, ms_WeaponIdleFlagId);
				}

				if(m_pMoveNetworkHelper)
				{
					if(pFiringPed && pFiringPed->IsLocalPlayer())
					{
						const CClipEventTags::CPadShakeTag* pPadShake = static_cast<const CClipEventTags::CPadShakeTag*>(m_pMoveNetworkHelper->GetProperty(CClipEventTags::PadShake));
						if (pPadShake)
						{
							u32 iRumbleDuration = pPadShake->GetPadShakeDuration();
							float fRumbleIntensity = Clamp(pPadShake->GetPadShakeIntensity(), 0.0f, 1.0f);
							CControlMgr::StartPlayerPadShakeByIntensity(iRumbleDuration, fRumbleIntensity);
						}

#if FPS_MODE_SUPPORTED
						bool bInFPSMode = pFiringPed->IsFirstPersonShooterModeEnabledForPlayer(false, false, true);

						if(pFiringPed->GetPlayerInfo() && (pFiringPed->GetPlayerInfo()->GetSwitchedToFirstPersonCount() > 0 || ( bInFPSMode != pFiringPed->GetMotionData()->GetWasUsingFPSMode())))
						{
							fIdleOnOffBlendDuration = INSTANT_BLEND_DURATION;
							if(!bInFPSMode && pFiringPed->GetMotionData()->GetWasUsingFPSMode() && GetWeaponHash() == ATSTRINGHASH("WEAPON_MICROSMG", 0x13532244))
							{
								bDoPostCameraAnimUpdate = true;
							}
						}
#endif
					}
					m_pMoveNetworkHelper->SetFloat(ms_WeaponIdleBlendDuration, fIdleOnOffBlendDuration);
				}

				ProcessGripAnim(pFiringEntity);

				ProcessIdleAnim(pFiringEntity);
			}
			else
			{
				m_bCanActivateMoveNetwork = false;
			}
		}

		// Handle barrel spin
		ProcessWeaponSpin(pFiringEntity);
		if(m_fBarrelSpin != 0.f)
		{
			// This doesn't behave the same without an anim director
			if(!m_pMoveNetworkHelper)
				CreateMoveNetworkHelper();

			const CWeaponModelInfo* pWeaponModelInfo = GetWeaponModelInfo();
			if(pWeaponModelInfo)
			{
				s32 iBarrelIndex = pWeaponModelInfo->GetBoneIndex(WEAPON_BARREL);
				if(iBarrelIndex != -1)
				{
					Matrix34 m;
					m_pDrawableEntity->GetGlobalMtx(iBarrelIndex, m);
					m.RotateLocalX(m_fBarrelSpin);
					m_pDrawableEntity->SetGlobalMtx(iBarrelIndex, m);
				}
			}
		}

		// Rotate barrel if we define a bone (for grenade launcher / revolver)
		int iBarrelIndex = GetWeaponInfo()->GetRotateBarrelBone().GetBoneIndex(m_pDrawableEntity->GetBaseModelInfo());
		if(iBarrelIndex > 0)
		{
			if(m_pMoveNetworkHelper && m_pMoveNetworkHelper->IsNetworkActive())
			{
				static const fwMvBooleanId s_RotateBarrel("RotateBarrel",0x9379EEBD);
				if(m_pMoveNetworkHelper->GetBoolean(s_RotateBarrel))
				{
					m_bNeedsToRotateBarrel = true;
					static const int NUM_BARRELS = 6;
					float fAngleNeedsToRotatePerShot = 2.0f*PI/NUM_BARRELS;
					m_fBarrelNeedsToRotate = fwAngle::LimitRadianAngle(m_fBarrelHasRotated+fAngleNeedsToRotatePerShot);
				}
			}

			if(m_bNeedsToRotateBarrel)
			{
				const CWeaponModelInfo* pWeaponModelInfo = GetWeaponModelInfo();
				if(pWeaponModelInfo)
				{
					// Maximum angle to rotate per frame
					TUNE_GROUP_FLOAT(GRENADE_LAUNCHER_TUNE, GRENADE_LAUNCHER_BARREL_ROTATE_RATE, 15.0f, 0.0f, 50.0f, 0.1f);

					float fAngleForThisFrame = fwAngle::LimitRadianAngle(m_fBarrelNeedsToRotate - m_fBarrelHasRotated);
					fAngleForThisFrame = Clamp(fAngleForThisFrame, 0.0f, DtoR*GRENADE_LAUNCHER_BARREL_ROTATE_RATE);
					m_fBarrelHasRotated += fAngleForThisFrame;
					m_fBarrelHasRotated = fwAngle::LimitRadianAngle(m_fBarrelHasRotated);

					Matrix34 m;
					m_pDrawableEntity->GetGlobalMtx(iBarrelIndex, m);
					m.RotateLocalX(m_fBarrelHasRotated);
					m_pDrawableEntity->SetGlobalMtx(iBarrelIndex, m);

					// Also need to rotate a second bone if defined
					int iBarrelIndex2 = GetWeaponInfo()->GetRotateBarrelBone2().GetBoneIndex(m_pDrawableEntity->GetBaseModelInfo());
					if (iBarrelIndex2 > 0)
					{
						// HACK: The two bones on the Revolver are facing in opposite directions...
						bool bIsRevolver = GetWeaponInfo()->GetFlag(CWeaponInfoFlags::UseSingleActionBehaviour);

						m_pDrawableEntity->GetGlobalMtx(iBarrelIndex2, m);
						m.RotateLocalX(bIsRevolver ? -m_fBarrelHasRotated : m_fBarrelHasRotated);
						m_pDrawableEntity->SetGlobalMtx(iBarrelIndex2, m);
					}
				}
			}
		}

		if(bDoPostCameraAnimUpdate && m_pDrawableEntity->GetIsTypeObject())
		{
			static_cast<CObject*>(m_pDrawableEntity.Get())->ForcePostCameraAnimUpdate(true, true);
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void CWeapon::ProcessFpsSightDof(CEntity* pFiringEntity)
{
	CPed* pParentPed = pFiringEntity && pFiringEntity->GetIsTypePed() ? static_cast<CPed*>(pFiringEntity) : NULL;

	if (pParentPed && pParentPed->IsLocalPlayer())
	{
		TUNE_GROUP_FLOAT(WEAPON_EXPRESSIONS, fSightDofInterpRate, 7.500f, 0.1f, 100000.0f, 0.0001f);
		TUNE_GROUP_BOOL(WEAPON_EXPRESSIONS, bAlwaysLowerSightsForScope, true);

		bool bIsFirstPersonActive = pParentPed->IsFirstPersonShooterModeEnabledForPlayer(false);
		bool bScopeActive = (GetScopeComponent()!=NULL);
		bool bIronSightsActive = pParentPed->GetMotionData() && pParentPed->GetMotionData()->GetIsFPSScope();

		// we can snap the dof value if the drawable isn't loaded yet, it's the first update, or we're switching to or from first person camera this frame.
		bool bShouldSnapValue = (m_pDrawableEntity==NULL) || m_bFpsSightDofFirstUpdate || (bIsFirstPersonActive!=m_bFpsSightDofPlayerWasFirstPersonLastUpdate);

		if (bIsFirstPersonActive && bScopeActive && (bIronSightsActive || bAlwaysLowerSightsForScope))
		{
			if (bShouldSnapValue)
			{
				m_FpsSightDofValue = 1.0f;
			}
			else
			{
				m_FpsSightDofValue += fwTimer::GetTimeStep()*fSightDofInterpRate;
			}		
		}
		else
		{
			if (bShouldSnapValue)
			{
				m_FpsSightDofValue = 0.0f;
			}
			else
			{
				m_FpsSightDofValue -= fwTimer::GetTimeStep()*fSightDofInterpRate;
			}
		}

		m_FpsSightDofValue = Clamp(m_FpsSightDofValue, 0.0f, 1.0f);

		if (m_pDrawableEntity && m_pDrawableEntity->GetAnimDirector())
		{
			static u8 s_SightTrack = 25;
			static u16 s_FrontSightDofId = 16959;
			static u16 s_RearSightDofId = 44178;

			crFrame* pFrame = static_cast<CObject*>(m_pDrawableEntity.Get())->GetMoveObject().GetExternalOverrideFrame();
			pFrame->SetVector3(s_SightTrack, s_FrontSightDofId, Vec3V(0.0f, m_FpsSightDofValue, 0.0f));
			pFrame->SetVector3(s_SightTrack, s_RearSightDofId, Vec3V(0.0f, m_FpsSightDofValue, 0.0f));

			// if we're switching camera this frame, we need to force a post camera update on the weapon
			// to get the state change on the camera cut.
			if (bIsFirstPersonActive!=m_bFpsSightDofPlayerWasFirstPersonLastUpdate && m_pDrawableEntity->GetIsTypeObject())
			{
				static_cast<CObject*>(m_pDrawableEntity.Get())->ForcePostCameraAnimUpdate(true, true);
			}
		}

		// If the drawable entity doesn't exist, there's no need to lerp the value.
		m_bFpsSightDofFirstUpdate = (m_pDrawableEntity==NULL);
		m_bFpsSightDofPlayerWasFirstPersonLastUpdate = bIsFirstPersonActive;
	}
	else
	{
		m_bFpsSightDofFirstUpdate = true;
	}
}

////////////////////////////////////////////////////////////////////////////////

float CWeapon::ProcessWeaponSpin(const CEntity* pFiringEntity, s32 boneIndex)
{
	static dev_float BARREL_SPIN_RATE = 10.0f;
	static dev_float TRIBARREL_SPIN_RATE = 15.0f;
	float fSpinRate = GetWeaponInfo()->GetUsesHighSpinRate() ? TRIBARREL_SPIN_RATE : BARREL_SPIN_RATE;

	switch(GetState())
	{
	case STATE_SPINNING_UP:
		{
			f32 fBarrelSpinRate = 1.0f - ((float)(m_uTimer - m_uGlobalTime)*0.001f)/GetWeaponInfo()->GetSpinUpTime();
			fBarrelSpinRate = Clamp(fBarrelSpinRate, 0.f, 1.f);
			m_fBarrelSpin += Lerp(fBarrelSpinRate, 0.f, fSpinRate * fwTimer::GetTimeStep());
			m_fBarrelSpin = fwAngle::LimitRadianAngle(m_fBarrelSpin);
			g_WeaponAudioEntity.SpinUpWeapon(pFiringEntity, this, boneIndex);
		}
		break;

	case STATE_SPINNING_DOWN:
		{
			f32 fBarrelSpinRate = 1.0f - ((float)(m_uTimer - m_uGlobalTime)*0.001f)/GetWeaponInfo()->GetSpinDownTime();
			fBarrelSpinRate  = Clamp(fBarrelSpinRate, 0.f, 1.f);
			m_fBarrelSpin += Lerp(fBarrelSpinRate, fSpinRate * fwTimer::GetTimeStep(), 0.f);
			m_fBarrelSpin = fwAngle::LimitRadianAngle(m_fBarrelSpin);	
			GetAudioComponent().SpinDownWeapon();	
		}
		break;

	default:
		if(m_bNeedsToSpinDown)
		{
			m_fBarrelSpin += fSpinRate * fwTimer::GetTimeStep();
			m_fBarrelSpin = fwAngle::LimitRadianAngle(m_fBarrelSpin);
			g_WeaponAudioEntity.SpinUpWeapon(pFiringEntity, this, boneIndex);
		}
		break;
	}

	GetAudioComponent().UpdateSpin();
	return m_fBarrelSpin;
}

////////////////////////////////////////////////////////////////////////////////

void CWeapon::ProcessStats(CEntity* pFiringEntity)
{
	if(pFiringEntity && pFiringEntity->GetIsTypePed())
	{
		CPed* pFiringPed = static_cast<CPed*>(pFiringEntity);
		if(pFiringPed->IsLocalPlayer())
		{
			if ((m_pDrawableEntity && m_pDrawableEntity->GetIsVisible()) || (GetWeaponInfo() && GetWeaponInfo()->GetIsVehicleWeapon()))
			{
				StatId statWeaponHeld = StatsInterface::GetLocalPlayerWeaponInfoStatId(StatsInterface::WEAPON_STAT_HELDTIME, GetWeaponInfo(), pFiringPed);
				if(StatsInterface::IsKeyValid(statWeaponHeld))
				{
					StatsInterface::IncrementStat(statWeaponHeld, static_cast<float>(fwTimer::GetTimeStepInMilliseconds()), STATUPDATEFLAG_ASSERTONLINESTATS);

					StatId favHeldTimeStat = StatsInterface::GetStatsModelHashId("FAVORITE_WEAPON_HELDTIME");
					if(StatsInterface::IsKeyValid(favHeldTimeStat))
					{
						u32 currHeldTime = StatsInterface::GetUInt32Stat(statWeaponHeld);
						u32 favHeldTime = StatsInterface::GetUInt32Stat(favHeldTimeStat);

						if(currHeldTime > favHeldTime)
						{
							if(StatsInterface::IsKeyValid(favHeldTimeStat))
							{
								StatsInterface::SetStatData(favHeldTimeStat, currHeldTime, STATUPDATEFLAG_ASSERTONLINESTATS);
							}

							// Lets cut down on spam
							bool hasChanged = true;
							StatId favHeldStat = StatsInterface::GetStatsModelHashId("FAVORITE_WEAPON");
							if(StatsInterface::IsKeyValid(favHeldStat) && StatsInterface::GetUInt32Stat(favHeldStat) == GetHumanNameHash())
							{
								hasChanged = false;
							}

							if(hasChanged)
							{
								weaponDisplayf("CWeapon::ProcessStats - Favorite Weapon is now %s", GetWeaponInfo()->GetName());
								StatsInterface::SetStatData(favHeldStat, GetHumanNameHash(), STATUPDATEFLAG_ASSERTONLINESTATS);
							}
						}
					}
				}
#if __ASSERT
				//During mp to sp transitions the model prefix might not right since 
				//  the network is not open anymore but the model is still a multiplayer model.
				else if(pFiringPed->IsArchetypeSet() && StatsInterface::GetStatsPlayerModelValid())
				{
					CStatsMgr::MissingWeaponStatName(STATTYPE_WEAPON_HELDTIME, statWeaponHeld, GetWeaponInfo()->GetHash(), GetWeaponInfo()->GetDamageType(), GetWeaponInfo()->GetName());
				}
#endif // __ASSERT

				//Update Drive by weapon held time
				StatId stat = StatsInterface::GetLocalPlayerWeaponInfoStatId(StatsInterface::WEAPON_STAT_DB_HELDTIME, GetWeaponInfo(), pFiringPed);
				if (pFiringPed->GetIsInVehicle() && StatsInterface::IsKeyValid(stat))
				{
					StatsInterface::IncrementStat(stat, static_cast< float >(fwTimer::GetTimeStepInMilliseconds()), STATUPDATEFLAG_ASSERTONLINESTATS);
				}
			}
		}
	}
}

void CWeapon::ProcessCookTimer(CEntity* pFiringEntity, u32 uTimeInMilliseconds)
{
	//! DMKH. If firing entity is NULL, use cook entity (which could be ped that dropped it).
	if(pFiringEntity == NULL)
	{
		pFiringEntity = m_pCookEntity;
	}

	if (m_bCooking && pFiringEntity)
	{						
		CPed* pFiringPed = (pFiringEntity && pFiringEntity->GetIsTypePed()) ? static_cast<CPed*>(pFiringEntity) : NULL;
		float cookTimeLeft = GetCookTimeLeft(pFiringPed, uTimeInMilliseconds);
		if(cookTimeLeft < 2.f && cookTimeLeft > 0.f)
		{
			GetAudioComponent().CookProjectile(pFiringEntity);
		}

		// Instantly run out the cook timer if we're in a defensive sphere that's hostile to us
		u8 uIntersectingZoneIdx = 0;
		if (pFiringPed && !NetworkUtils::IsNetworkClone(pFiringEntity) && pFiringPed->GetPedResetFlag(CPED_RESET_FLAG_InAirDefenceSphere) && CAirDefenceManager::IsPositionInDefenceZone(pFiringPed->GetTransform().GetPosition(), uIntersectingZoneIdx))
		{
			CAirDefenceZone *pDefZone = CAirDefenceManager::GetAirDefenceZone(uIntersectingZoneIdx);
			if (pDefZone && pDefZone->IsEntityTargetable(pFiringEntity))
			{
				cookTimeLeft = 0.0f;
			}
		}

		if (m_pWeaponInfo->GetProjectileFuseTime(pFiringPed ? pFiringPed->GetIsInVehicle() : false) > 0.0f && cookTimeLeft<=0 && !m_pWeaponInfo->GetDropWhenCooked() )
		{ //Boom
			GetAudioComponent().StopCookProjectile();
			if(m_pDrawableEntity)
			{
				if (pFiringPed)
				{					
					if (pFiringPed->GetWeaponManager())
						pFiringPed->GetWeaponManager()->DestroyEquippedWeaponObject();
				}
				//x64 Release version does not like MAT34V_TO_MATRIX34 being passed directly to the function 
				// see bug 1844055 : James.A
				//CWeapon::sFireParams params(pFiringEntity, MAT34V_TO_MATRIX34(m_pDrawableEntity->GetMatrix()));				
				Matrix34 tmp = MAT34V_TO_MATRIX34(m_pDrawableEntity->GetMatrix());
				CWeapon::sFireParams params(pFiringEntity, tmp);
				params.fOverrideLifeTime = 0;
				Fire(params);
				CancelCookTimer();
			}
		}
	}
	else
	{
		if(m_pWeaponInfo->GetIsProjectile())
		{
			GetAudioComponent().StopCookProjectile();
		}
	}
}

float CWeapon::GetCookTimeLeft(CPed* pFiringPed, u32 uTimeInMilliseconds) const 
{
	if (!m_bCooking) return m_pWeaponInfo->GetProjectileFuseTime(pFiringPed ? pFiringPed->GetIsInVehicle() : false);
	const float fFuseTime = m_pWeaponInfo->GetProjectileFuseTime(pFiringPed ? pFiringPed->GetIsInVehicle() : false);
	float cookTime = (float)(uTimeInMilliseconds-m_uCookTime)/1000.0f;
	return fFuseTime - cookTime;
}

////////////////////////////////////////////////////////////////////////////////

void CWeapon::GetMuzzleMatrix(const Matrix34& weaponMatrix, Matrix34& muzzleMatrix) const
{
	if(m_bValidMuzzleMatrix)
	{
		Mat34VFromQuatV(RC_MAT34V(muzzleMatrix), m_muzzleRot, m_muzzlePos);
	}
	else
	{
		muzzleMatrix = weaponMatrix;
	}
}

////////////////////////////////////////////////////////////////////////////////

void CWeapon::GetMuzzlePosition(const Matrix34& weaponMatrix, Vector3& vBonePos) const
{
	if(m_bValidMuzzleMatrix)
	{
		if(!m_vMuzzleOffset.IsZero() && GetWeaponHash() == ATSTRINGHASH("WEAPON_MILITARYRIFLE", 0x9D1F17E6))
		{
			// Add the muzzle offset to the muzzle bone local position
			Mat34V vMtxBase;
			CVfxHelper::GetMatrixFromBoneIndex(vMtxBase, GetEntity(), GetMuzzleBoneIndex());
			
			Mat34V vMtxOffset;
			vMtxOffset.SetIdentity3x3();
			RC_MATRIX34(vMtxOffset).RotateY(m_muzzleRot.GetYf());
			RC_MATRIX34(vMtxOffset).RotateX(m_muzzleRot.GetXf());
			RC_MATRIX34(vMtxOffset).RotateZ(m_muzzleRot.GetZf());
			vMtxOffset.SetCol3(VECTOR3_TO_VEC3V(m_vMuzzleOffset));

			Mat34V vMtxFinal;
			Transform(vMtxFinal, vMtxBase, vMtxOffset);

			vBonePos = VEC3V_TO_VECTOR3(vMtxFinal.GetCol3());
		}
		else
		{
			vBonePos = VEC3V_TO_VECTOR3(m_muzzlePos);
		}
	}
	else
	{
		vBonePos = weaponMatrix.d;
	}
}

//////////////////////////////////////////////////////////////////////////

void CWeapon::SetMuzzlePosition(const Vector3& muzzlePos)
{
	m_muzzlePos = VECTOR3_TO_VEC3V(muzzlePos);
	m_bValidMuzzleMatrix = true;
}

////////////////////////////////////////////////////////////////////////////////

void CWeapon::ProcessMuzzleSmoke()
{
	// update the level
	if (GetWeaponInfo()->GetMuzzleSmokePtFxDecPerSec()>0.0f)
	{
		float decLevel = GetWeaponInfo()->GetMuzzleSmokePtFxDecPerSec()*fwTimer::GetTimeStep();
		m_muzzleSmokeLevel = Max(0.0f, m_muzzleSmokeLevel-(decLevel));
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CWeapon::ShouldDoBulletTrace(CEntity* pFiringEntity)
{
	// reject non-player tracer - usefule for debugging
// 	const bool bIsLocalPlayerPed = pFiringEntity->GetIsTypePed() && static_cast<CPed*>(pFiringEntity)->IsLocalPlayer();
// 	if (bIsLocalPlayerPed==false)
// 	{
// 		return false;
// 	}

	// allow if we're forcing traces
#if __BANK
 	if(g_vfxWeapon.GetForceBulletTraces())
	{
		return true;
	}
#endif

	// reject traces in first person driver camera 
	if (pFiringEntity && pFiringEntity->GetIsTypePed())
	{
		if (CTaskAimGunVehicleDriveBy::IsHidingGunInFirstPersonDriverCamera(static_cast<CPed*>(pFiringEntity)))
		{
			return false;
		}
	}
 
	if (m_pClipComponent && m_pClipComponent->GetInfo())
	{
		s32 numBulletsLeft = GetAmmoInClip();

		s32 tracerFxForceLast = m_pClipComponent->GetInfo()->GetTracerFxForceLast();
		if (tracerFxForceLast>-1)
		{
			if (numBulletsLeft<tracerFxForceLast)
			{
				return true;
			}
		}

		s32 tracerFxSpacing = m_pClipComponent->GetInfo()->GetTracerFxSpacing();
		if (tracerFxSpacing>-1)
		{
			if (tracerFxSpacing==0)
			{
				return false;
			}
			else if (tracerFxSpacing==1)
			{
				return true;
			}
			else
			{
				return (numBulletsLeft%tracerFxSpacing)==0;
			}
		}
	}

 	// reject in first person aiming mode
// 	if (pFiringEntity && camInterface::GetGameplayDirector().IsFirstPersonAiming(pFiringEntity))
// 	{
// 		return false;
// 	}

	// LB requested that all bullets leave traces (bug #535485)
	return true;

// 	// Do the bullet trace effect
// 	float bulletFxProb = 0.0f;
// 	if (NetworkInterface::IsGameInProgress())
// 	{
// 		bulletFxProb = GetWeaponInfo()->GetBulletTracerPtFxChanceMP();
// 	}
// 	else
// 	{
// 		bulletFxProb = GetWeaponInfo()->GetBulletTracerPtFxChanceSP();
// 	}
// 
// #if __BANK
// 	if(g_vfxWeapon.GetForceBulletTraces())
// 	{
// 		bulletFxProb = 1.0f;
// 	}
// #endif // __BANK
// 
// 	// Drive-bys always create traces
// 	if(pFiringEntity && pFiringEntity->GetIsTypePed())
// 	{
// 		CPed* pPed = static_cast<CPed*>(pFiringEntity);
// 		if(pPed->IsPlayer() && pPed->GetIsDrivingVehicle())
// 		{
// 			bulletFxProb = 1.0f;
// 		}
// 	}
// 
// 	if(g_DrawRand.GetFloat() <= bulletFxProb)
// 	{
// 		return true;
// 	}
// 
// 	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CWeapon::GetIsAccurate(const Vector3& vEnd, const Vector3& vAdjustedEnd) const
{
	Vector3 vDiff = vAdjustedEnd - vEnd;
	static dev_float ACCURATE_HIT_DEVIATION_SQ = square(0.3f);
	return vDiff.Mag2() < ACCURATE_HIT_DEVIATION_SQ;
}

////////////////////////////////////////////////////////////////////////////////

bool CWeapon::GetUsesBulletPenetration(const CEntity* pFiringEntity) const
{
	static dev_bool FORCE_BULLET_PENETRATION = false;
	if(FORCE_BULLET_PENETRATION)
	{
		return true;
	}

	if(pFiringEntity && pFiringEntity->GetIsTypePed())
	{
		if (static_cast<const CPed*>(pFiringEntity)->GetPedResetFlag(CPED_RESET_FLAG_UseBulletPenetration))
		{
			return true;
		}
	}

	if (GetWeaponInfo()->GetPenetrateVehicles())
	{
		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

s32 CWeapon::GetReloadTime(const CEntity* pFiringEntity) const
{
	bool bInVehicle = false;
	if( pFiringEntity && pFiringEntity->GetIsTypePed() )
	{
		if( ((CPed*)pFiringEntity)->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) )
		{
			bInVehicle = true;
		}
		else if(((CPed *)pFiringEntity)->GetIsParachuting() || ((CPed *)pFiringEntity)->GetPedResetFlag(CPED_RESET_FLAG_IsUsingJetpack))
		{
			bInVehicle = true;
		}
	}

	s32 iReloadTime = 0;
	if( bInVehicle )
		iReloadTime = static_cast<s32>(1000.0f * GetWeaponInfo()->GetVehicleReloadTime());
	else
		iReloadTime = static_cast<s32>(1000.0f * GetWeaponInfo()->GetReloadTime());
	return iReloadTime;
}

////////////////////////////////////////////////////////////////////////////////

void CWeapon::DoWeaponMuzzle(CEntity* pFiringEntity, s32 iVehicleWeaponBoneIndex)
{
	m_pMuzzleEntity = NULL;
	m_iMuzzleBoneIndex = -1;
	m_vMuzzleOffset = VEC3_ZERO;

	if (iVehicleWeaponBoneIndex>-1)
	{
		// this is a vehicle weapon
		m_pMuzzleEntity = pFiringEntity;
		m_iMuzzleBoneIndex = iVehicleWeaponBoneIndex;

		// if this is a turret weapon, need to lookup vehicle for muzzle entity
		if (m_pWeaponInfo && (m_pWeaponInfo->GetIsTurret() || m_pWeaponInfo->GetIsVehicleWeapon()))
		{
			if (pFiringEntity && pFiringEntity->GetIsTypePed())
			{
				const CPed* pPed = static_cast<const CPed*>(pFiringEntity);
				if (pPed->GetIsInVehicle())
				{
					m_pMuzzleEntity = pPed->GetMyVehicle();
				}
			}
		}
	}
	else
	{
		// this is a non-vehicle weapon
		m_pMuzzleEntity = m_pDrawableEntity;

		if (m_pSuppressorComponent && m_pSuppressorComponent->GetDrawable() && m_pSuppressorComponent->GetDrawable()->GetIsVisible())
		{
			m_iMuzzleBoneIndex = m_pSuppressorComponent->GetMuzzleBoneIndex();
			if (m_iMuzzleBoneIndex != -1)
			{
				m_pMuzzleEntity = m_pSuppressorComponent->GetDrawable();
			}
		}

		if (m_iMuzzleBoneIndex == -1)
		{
			const CWeaponModelInfo* pWeaponModelInfo = GetWeaponModelInfo();
			if (pWeaponModelInfo)
			{
				m_iMuzzleBoneIndex = pWeaponModelInfo->GetBoneIndex(WEAPON_MUZZLE);
			}
		}
	}

	if (m_iMuzzleBoneIndex>-1)
	{
		if (m_pWeaponInfo->GetFireType() == FIRE_TYPE_VOLUMETRIC_PARTICLE)
		{
			g_vfxWeapon.ProcessPtFxVolumetric(this, m_pMuzzleEntity, m_iMuzzleBoneIndex);
		}
		else
		{
			float flashFxProb = 0.0f;
			if (NetworkInterface::IsGameInProgress())
			{
				flashFxProb = GetWeaponInfo()->GetBulletFlashPtFxChanceMP();
			}
			else
			{
				flashFxProb = GetWeaponInfo()->GetBulletFlashPtFxChanceSP();
			}

			if (g_DrawRand.GetFloat()<=flashFxProb)
			{
				// test for using the alternate muzzle effect
				bool useAlternateFx = g_DrawRand.GetFloat()<=GetWeaponInfo()->GetBulletFlashAltPtFxChance();

				if (!m_pSuppressorComponent)
				{
					// Some w_fire animations work with new DLC weapons, but the barrel is shorter so muzzle flash jumps around. Allow an offset from WeaponInfo to override this.
					m_vMuzzleOffset = GetWeaponInfo()->GetMuzzleOverrideOffset();
					if (!m_vMuzzleOffset.IsZero() && !GetWeaponInfo()->GetIsVehicleWeapon())
					{
						//! Do relative to root as we can't rely on a consistent offset if the muzzle bone is animated.
						const CWeaponModelInfo* pWeaponModelInfo = GetWeaponModelInfo();
						s32 iRootBoneIndex = pWeaponModelInfo->GetBoneIndex(WEAPON_ROOT);
						if(iRootBoneIndex != -1)
						{
							m_iMuzzleBoneIndex = iRootBoneIndex;
						}
					}
				}

				//B*1819450: Spycar - Use alternate muzzle fx if weapon is underwater
				if (pFiringEntity && pFiringEntity->GetIsTypeVehicle())
				{
					CVehicle* pVehicle = static_cast<CVehicle*>(pFiringEntity);
					if (pVehicle && pVehicle->InheritsFromSubmarineCar())
					{
						bool bUnderwaterAltFx = false;

						const CPed *pPed = pVehicle->GetDriver();
						if (pPed)
						{  
							const CVehicleWeapon* pVehicleWeapon = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedVehicleWeapon() : NULL;
							if (pVehicleWeapon && pVehicleWeapon->IsUnderwater(pVehicle))
							{
								bUnderwaterAltFx = true;
							}
						}

						useAlternateFx = bUnderwaterAltFx;
					}
				}

				g_vfxWeapon.ProcessPtFxMuzzleFlash(pFiringEntity, this, m_pMuzzleEntity, m_iMuzzleBoneIndex, useAlternateFx, VECTOR3_TO_VEC3V(m_vMuzzleOffset));
			}
		}

		// update the muzzle smoke (only for the player)
		if (pFiringEntity && GetWeaponInfo()->GetMuzzleSmokePtFxIncPerShot()>0.0f)
		{
			const bool bIsLocalPlayerPed = pFiringEntity->GetIsTypePed() && static_cast<CPed*>(pFiringEntity)->IsLocalPlayer();
			const bool bDriverIsLocalPlayer = pFiringEntity->GetIsTypeVehicle() && static_cast<CVehicle*>(pFiringEntity)->GetDriver() && static_cast<CVehicle*>(pFiringEntity)->GetDriver()->IsLocalPlayer();

			if (bIsLocalPlayerPed || bDriverIsLocalPlayer)
			{
				m_muzzleSmokeLevel += GetWeaponInfo()->GetMuzzleSmokePtFxIncPerShot();
				m_muzzleSmokeLevel = Clamp(m_muzzleSmokeLevel, GetWeaponInfo()->GetMuzzleSmokePtFxMinLevel(), 1.0f);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CWeapon::DoWeaponGunshell(CEntity* pFiringEntity, s32 iVehicleWeaponIndex, s32 iVehicleWeaponBoneIndex) const
{
	CEntity* pGunshellEntity = NULL;
	s32 iGunshellBoneIndex = -1;
	if (iVehicleWeaponBoneIndex>-1)
	{
		// this is a vehicle weapon
		pGunshellEntity = pFiringEntity;
		iGunshellBoneIndex = iVehicleWeaponBoneIndex;

		// if this is a turret weapon, need to lookup vehicle for shell entity
		if (m_pWeaponInfo && (m_pWeaponInfo->GetIsTurret() || m_pWeaponInfo->GetIsVehicleWeapon()))
		{
			if (pFiringEntity && pFiringEntity->GetIsTypePed())
			{
				const CPed* pPed = static_cast<const CPed*>(pFiringEntity);
				if (pPed->GetIsInVehicle())
				{
					pGunshellEntity = pPed->GetMyVehicle();
				}
			}
		}
	}
	else
	{
		// this is a non-vehicle weapon
		pGunshellEntity = m_pDrawableEntity;
		const CWeaponModelInfo* pWeaponModelInfo = GetWeaponModelInfo();
		if(pWeaponModelInfo)
		{
			iGunshellBoneIndex = pWeaponModelInfo->GetBoneIndex(WEAPON_VFX_EJECT);
		}
	}

	if (iGunshellBoneIndex>-1)
	{
		// vehicle weapons won't have a separate gunshell entity so use the firing entity instead
		if (pGunshellEntity==NULL)
		{
			pGunshellEntity = pFiringEntity;
		}

		g_vfxWeapon.ProcessPtFxGunshell(pFiringEntity, this, pGunshellEntity, iGunshellBoneIndex, iVehicleWeaponIndex);

		if (pFiringEntity && pFiringEntity->GetIsTypePed())
		{
			static_cast<CPed*>(pFiringEntity)->GetPedAudioEntity()->TriggerShellCasing(GetAudioComponent());
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CWeapon::InitComponent(CWeaponComponent* pComponent, const bool bDoReload)
{
	if(pComponent)
	{
		weaponAssert(pComponent->GetInfo()->GetModelHash() == 0 || !pComponent->GetInfo()->GetCreateObject() || pComponent->GetDrawable());

		switch(pComponent->GetInfo()->GetClassId())
		{
		case WCT_Clip:
			{
				weaponAssert(!m_pClipComponent);
				m_pClipComponent = static_cast<CWeaponComponentClip*>(pComponent);

				// Ensure our ammo is up to date
				if (bDoReload)
				{
					DoReload();
				}
			}
			break;

		case WCT_Scope:
			{
				weaponAssert(!m_pScopeComponent);
				m_pScopeComponent = static_cast<CWeaponComponentScope*>(pComponent);
			}
			break;

		case WCT_Suppressor:
			{
				weaponAssert(!m_pSuppressorComponent);
				m_pSuppressorComponent = static_cast<CWeaponComponentSuppressor*>(pComponent);
				if (m_pSuppressorComponent && m_pSuppressorComponent->GetInfo()->GetShouldSilence())
				{
					m_bSilenced = true;
				}
			}
			break;

		case WCT_ProgrammableTargeting:
			{
				weaponAssert(!m_pTargetingComponent);
				m_pTargetingComponent = static_cast<CWeaponComponentProgrammableTargeting*>(pComponent);
			}
			break;

		case WCT_FlashLight:
			{
				weaponAssert(!m_pFlashLightComponent);
				m_pFlashLightComponent = static_cast<CWeaponComponentFlashLight*>(pComponent);
			}
			break;

		case WCT_LaserSight:
			{
				weaponAssert(!m_pLaserSightComponent);
				m_pLaserSightComponent = static_cast<CWeaponComponentLaserSight*>(pComponent);
			}
			break;

		case WCT_VariantModel:
			{
				weaponAssert(!m_pVariantModelComponent);
				m_pVariantModelComponent = static_cast<CWeaponComponentVariantModel*>(pComponent);
			}
			break;

		case WCT_Group:
			{
				CWeaponComponentGroup* pGroup = static_cast<CWeaponComponentGroup*>(pComponent);
				const CWeaponComponentGroup::Components& components = pGroup->GetComponents();
				for(s32 i = 0; i < components.GetCount(); i++)
				{
					InitComponent(components[i]);
				}
			}
			break;

		default:
			break;
		}

#if FPS_MODE_SUPPORTED
		if(pComponent->GetInfo()->GetHash() == ATSTRINGHASH("COMPONENT_AT_AR_AFGRIP", 0xC164F53) ||
		   pComponent->GetInfo()->GetHash() == ATSTRINGHASH("COMPONENT_AT_AR_AFGRIP_02", 0x9D65907A))
		{
			m_bHasGripAttachmentComponent = true;
		}
#endif
	}
}

void CWeapon::ShutdownComponent(CWeaponComponent* pComponent)
{
	if(pComponent)
	{
		switch(pComponent->GetInfo()->GetClassId())
		{
			case WCT_Clip:
				{
					if(m_pClipComponent == pComponent)
						m_pClipComponent = NULL;
				}
				break;

			case WCT_Scope:
				{
					if(m_pScopeComponent == pComponent)
						m_pScopeComponent = NULL;
				}
				break;

			case WCT_Suppressor:
				{
					if(m_pSuppressorComponent == pComponent)
					{
						m_pSuppressorComponent = NULL;
						m_bSilenced = m_pWeaponInfo->GetIsSilenced();
					}
				}
				break;

			case WCT_ProgrammableTargeting:
				{
					if(m_pTargetingComponent == pComponent)
						m_pTargetingComponent = NULL;
				}
				break;

			case WCT_FlashLight:
				{
					if(m_pFlashLightComponent == pComponent)
						m_pFlashLightComponent = NULL;
				}
				break;

			case WCT_LaserSight:
				{
					if(m_pLaserSightComponent == pComponent)
						m_pLaserSightComponent = NULL;
				}
				break;

			case WCT_Group:
				{
					CWeaponComponentGroup* pGroup = static_cast<CWeaponComponentGroup*>(pComponent);
					const CWeaponComponentGroup::Components& components = pGroup->GetComponents();
					for(s32 i = 0; i < components.GetCount(); i++)
					{
						ShutdownComponent(components[i]);
					}
				}
				break;

		default:
			break;
		}

#if FPS_MODE_SUPPORTED
		if(pComponent->GetInfo()->GetHash() == ATSTRINGHASH("COMPONENT_AT_AR_AFGRIP", 0xC164F53) ||
		   pComponent->GetInfo()->GetHash() == ATSTRINGHASH("COMPONENT_AT_AR_AFGRIP_02", 0x9D65907A))
		{
			m_bHasGripAttachmentComponent = false;
		}
#endif
	}
}

////////////////////////////////////////////////////////////////////////////////

#if __BANK
const char* CWeapon::GetStateName(State state)
{
	switch(state)
	{
	case STATE_READY:
		return "Ready";
	case STATE_WAITING_TO_FIRE:
		return "Waiting to Fire";
	case STATE_RELOADING:
		return "Reloading";
	case STATE_OUT_OF_AMMO:
		return "Out of Ammo";
	case STATE_SPINNING_UP:
		return "Spinning Up";
	case STATE_SPINNING_DOWN:
		return "Spinning Down";
	default:
		weaponAssertf(0, "Unhandled state [%d]", state);
		return "Unknown";
	}
}
#endif // __BANK

////////////////////////////////////////////////////////////////////////////////

void CWeapon::AddComponent(CWeaponComponent* pComponent, bool bDoReload)
{
	if(!m_Components.IsFull())
	{
		m_Components.Push(pComponent);
		InitComponent(pComponent, bDoReload);
	}
	else
	{
		delete pComponent;
	}
}

bool CWeapon::ReleaseComponent(CWeaponComponent* pWeaponComponentToDelete)
{
	// Do not try to release a NULL weapon component
	if( !pWeaponComponentToDelete)
		return false;

	// Unfortunately we need to walk through the entire array. 
	// Possible optimization is to make this a hash table based on the weapon component hash
	for(s32 i = 0; i < m_Components.GetCount(); i++)
	{
		if( pWeaponComponentToDelete == m_Components[i] )
		{
			// Clean-up class specific pointers
			ShutdownComponent( m_Components[i] );
			m_Components.Delete(i);
			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////
void CWeapon::Update_HD_Models(CEntity* pFiringEntity)
{
    bool forceHd = m_bExplicitHdRequest;
    m_bExplicitHdRequest = false;

	if (m_weaponLodState == WLS_HD_NA)
	{
		return;
	}
	
	if (!pFiringEntity && m_pDrawableEntity)
	{
		pFiringEntity = m_pDrawableEntity;
	}

	CWeaponModelInfo *pWeaponModelInfo = NULL;

	CBaseModelInfo* pModelInfo = m_pDrawableEntity ? m_pDrawableEntity->GetBaseModelInfo() : NULL;
	if(pModelInfo && pModelInfo->GetModelType() == MI_TYPE_WEAPON)	
	{
		pWeaponModelInfo = static_cast<CWeaponModelInfo*>(pModelInfo);
	}

	if (!pWeaponModelInfo)
	{
		return;
	}

	if (!pWeaponModelInfo->GetHasHDFiles())
	{
		m_weaponLodState = WLS_HD_NA;
		return;
	}

	const Vector3& camPos = camInterface::GetPos();
	float dist = pFiringEntity ? camPos.Dist(VEC3V_TO_VECTOR3(pFiringEntity->GetTransform().GetPosition())) : 0.0f;
	CPed *pPlayer = CGameWorld::FindLocalPlayer();
	CWeapon* pPlayerWeapon = NULL;
	if (pPlayer && pPlayer->GetWeaponManager())
	{
		pPlayerWeapon = pPlayer->GetWeaponManager()->GetEquippedWeapon();
	}

	// Do we need a high model ?
	bool requireHighModel = DEV_ONLY(!bForceLoDetail &&)
							(dist < pWeaponModelInfo->GetHDDist() || (this == pPlayerWeapon) DEV_ONLY(|| bEnableHiDetail)
							 || CutSceneManager::GetInstance()->IsCutscenePlayingBack() || forceHd);
	
	// B*4168217 - Facility Property - Part of minigun in security room becomes translucent 
	if(GetWeaponModelInfo())
	{
		CDynamicEntity *pWeaponEntity = m_pDrawableEntity;
		CObject *pWeaponObject = static_cast<CObject*>(pWeaponEntity);	// every weapon is a CObject
		if(pWeaponObject->IsPickup())
		{
			CPickup *pWeaponPickup = static_cast<CPickup*>(pWeaponObject);

			CPickupPlacement *pPickupPlacement = pWeaponPickup->GetPlacement();
			if(pPickupPlacement)
			{
				if(pPickupPlacement->GetIsHDWeaponModelDisabled())
				{
					requireHighModel = false;	// forceLoDetail
				}
			}
		}
	}

	switch(m_weaponLodState)
	{
		case WLS_HD_NONE:
			if (requireHighModel)
			{
				pWeaponModelInfo->AddToHDInstanceList((size_t)this);
				m_weaponLodState = WLS_HD_REQUESTED;
			}
			break;
		case WLS_HD_REQUESTED:
			if (pWeaponModelInfo->GetAreHDFilesLoaded())
			{
				ShaderEffect_HD_CreateInstance();
				m_weaponLodState = WLS_HD_AVAILABLE;
			}
			break;
		case WLS_HD_AVAILABLE:
			if (!requireHighModel)
			{
				ShaderEffect_HD_DestroyInstance();
				m_weaponLodState = WLS_HD_REMOVING;
			}
			break;
		case WLS_HD_REMOVING:
			pWeaponModelInfo->RemoveFromHDInstanceList((size_t)this);
			m_weaponLodState = WLS_HD_NONE;
			break;
		case WLS_HD_NA :
		default :
			weaponAssert(false);		// illegal cases
			break;
	}

	for (int i = 0; i < m_Components.GetCount(); ++i)
	{
		m_Components[i]->Update_HD_Models(requireHighModel);
	}
}


void CWeapon::ShaderEffect_HD_CreateInstance()
{
	FastAssert(sysThreadType::IsUpdateThread());

	Assertf(m_pDrawableEntity, "m_pDrawableEntity is NULL");

	m_pStandardDetailShaderEffect = m_pDrawableEntity->GetDrawHandler().GetShaderEffect();		// store the old one
	// Some weapons (molotov) don't have shader effects
	if (m_pStandardDetailShaderEffect)
	{
		CWeaponModelInfo *pWeaponInfo  =  static_cast<CWeaponModelInfo *>(m_pDrawableEntity->GetBaseModelInfo());

		CCustomShaderEffectWeaponType *pMasterShaderEffectType = pWeaponInfo->GetHDMasterCustomShaderEffectType();
		if(pMasterShaderEffectType)
		{
			CCustomShaderEffectWeapon* pHDShaderEffect = NULL;
			Assert(pMasterShaderEffectType->GetIsHighDetail());
			pHDShaderEffect = static_cast<CCustomShaderEffectWeapon*>(pMasterShaderEffectType->CreateInstance(m_pDrawableEntity));
			Assert(pHDShaderEffect);

			// copy CSE std->HD settings:
			pHDShaderEffect->CopySettings((CCustomShaderEffectWeapon*)m_pStandardDetailShaderEffect);

			m_pDrawableEntity->GetDrawHandler().SetShaderEffect(pHDShaderEffect);
			weaponitemDebugf3("CWeapon::ShaderEffect_HD_CreateInstance-->invoke UpdateShaderVariables");
			UpdateShaderVariables(m_uTintIndex);
		}
		else
		{
			m_pStandardDetailShaderEffect = NULL;
		}
	}
}

void CWeapon::ShaderEffect_HD_DestroyInstance()
{
	if (m_pStandardDetailShaderEffect)
	{
		fwCustomShaderEffect* pHDShaderEffect = m_pDrawableEntity->GetDrawHandler().GetShaderEffect();
		Assert(pHDShaderEffect);

		if(pHDShaderEffect)
		{
			Assert(pHDShaderEffect->GetType()==CSE_WEAPON);
			Assert((static_cast<CCustomShaderEffectWeapon*>(pHDShaderEffect))->GetCseType()->GetIsHighDetail());
			if(m_pStandardDetailShaderEffect)
			{
				// copy CSE HD->std settings back:
				((CCustomShaderEffectWeapon*)m_pStandardDetailShaderEffect)->CopySettings((CCustomShaderEffectWeapon*)pHDShaderEffect);
			}
			pHDShaderEffect->DeleteInstance();
		}

		m_pDrawableEntity->GetDrawHandler().SetShaderEffect(m_pStandardDetailShaderEffect);

		weaponitemDebugf3("CWeapon::ShaderEffect_HD_DestroyInstance-->invoke UpdateShaderVariables");
		UpdateShaderVariables(m_uTintIndex);
		m_pStandardDetailShaderEffect = NULL;
	}
}

void CWeapon::SetWeaponIdleFlag(bool bUseIdle)
{
	if(m_pMoveNetworkHelper && m_bHasWeaponIdleAnim)
	{
		m_pMoveNetworkHelper->SetFlag(bUseIdle, ms_WeaponIdleFlagId);
	}
}

#if __BANK
void CWeapon::AddWidgets(bkBank &rBank)
{
	rBank.AddToggle("Enable High Detail for all weapons", &bEnableHiDetail);
	rBank.AddToggle("Force Low Detail for all weapons", &bForceLoDetail);
}
#endif	//__BANK

#if __BANK
template<> void fwPool<CWeapon>::PoolFullCallback()
{
	USE_DEBUG_MEMORY();

	s32 size = GetSize();
	int iIndex = 0;
	int iTotalNumVehicleWeapons = 0;
	int iTotalNumUnarmedWeapons = 0;
	int iTotalNumPickupWeapons = 0;

	while(size--)
	{
		CWeapon* pWeapon = GetSlot(iIndex);
		if(pWeapon)
		{
			const CEntity* pEntity = pWeapon->GetEntity();
			const fwEntity* pAttachedParent = NULL; 
			if(pEntity && pEntity->GetIsAttached())
			{
				pAttachedParent = pEntity->GetAttachParent();
			}

			bool bPickup = false;
			s32 PickupSize = CPickup::GetPool()->GetSize();
			int iPickupIndex = 0;
			while(PickupSize--)
			{
				CPickup* pPickup = CPickup::GetPool()->GetSlot(iPickupIndex);
				if(pPickup)
				{
					CWeapon* pPickupWeapon = pPickup->GetWeapon();
					if(pPickupWeapon == pWeapon)
					{
						bPickup = true;
						iTotalNumPickupWeapons++;
						break;
					}
				}
				iPickupIndex++;
			}

			bool bVehicleWeapon = false;
			s32 VehicleSize = (s32) CVehicle::GetPool()->GetSize();
			int iVehicleIndex = 0;
			while(VehicleSize-- && !bVehicleWeapon)
			{
				const CVehicle* pVehicle = CVehicle::GetPool()->GetSlot(iVehicleIndex);
				if(pVehicle)
				{
					const CVehicleWeaponMgr* pWpnMgr = pVehicle->GetVehicleWeaponMgr();
					if(pWpnMgr)
					{
						for(s32 i = 0; i < pWpnMgr->GetNumVehicleWeapons(); i++)
						{
							const CVehicleWeapon* pVehWeapon = pWpnMgr->GetVehicleWeapon(i);
							if(pVehWeapon)
							{
								if(pVehWeapon->HasWeapon(pWeapon))
								{
									bVehicleWeapon = true;
									iTotalNumVehicleWeapons++;
									pAttachedParent = pVehicle;
									break;
								}
							}
						}
					}
				}
				iVehicleIndex++;
			}

			if(!bPickup && !bVehicleWeapon)
			{
				if(pWeapon->GetWeaponInfo()->GetIsUnarmed())
				{
					iTotalNumUnarmedWeapons++; 
				}
			}

			if(bPickup)
			{
				Displayf("%3i, PICKUP: name: %s:%d, weapon pointer: %p, weapon is attached to: %10p:%s, weapon model: %s.", 
					iIndex, pWeapon->GetWeaponInfo()->GetName(), pWeapon->GetWeaponHash(), pWeapon, pAttachedParent, pAttachedParent ? pAttachedParent->GetModelName() : "null", pEntity ? pEntity->GetModelName() : "null");
			}
			else if(bVehicleWeapon)
			{
				Displayf("%3i, VEHICLE: name: %s:%d, weapon pointer: %p, weapon is attached to: %10p:%s, clone: %s, wrecked: %s, weapon model: %s.", 
					iIndex, pWeapon->GetWeaponInfo()->GetName(), pWeapon->GetWeaponHash(), pWeapon, pAttachedParent, pAttachedParent ? pAttachedParent->GetModelName() : "null", pAttachedParent && static_cast<const CVehicle*>(pAttachedParent)->IsNetworkClone() ? "Y" : "N", pAttachedParent && static_cast<const CVehicle*>(pAttachedParent)->GetStatus() == STATUS_WRECKED ? "Y" : "N", pEntity ? pEntity->GetModelName() : "null");
			}
			else
			{
				Displayf("%3i, name: %s:%d, weapon pointer: %p, weapon is attached to: %10p:%s, weapon model: %s.", 
					iIndex, pWeapon->GetWeaponInfo()->GetName(), pWeapon->GetWeaponHash(), pWeapon, pAttachedParent, pAttachedParent ? pAttachedParent->GetModelName() : "null", pEntity ? pEntity->GetModelName() : "null");
			}
		}
		else
		{
			Displayf("%3i, NULL weapon.", iIndex);
		}

		iIndex++;
	}

	Displayf("Total number of peds: %d", CPed::GetPool()->GetNoOfUsedSpaces());
	Displayf("Total number of vehicle weapons: %d", iTotalNumVehicleWeapons);
	Displayf("Total number of unarmed weapons: %d", iTotalNumUnarmedWeapons);
	Displayf("Total number of pickup weapons: %d", iTotalNumPickupWeapons);
}
#endif
