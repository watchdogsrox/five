// File header
#include "Weapons/Projectiles/Projectile.h"

//	Task header
#include "Task/Weapons/TaskProjectile.h"

// Rage headers
#include "audiosoundtypes/sound.h" 
#include "fwscene/stores/staticboundsstore.h"
#include "fwscene/world/worldlimits.h"

// Game headers
#include "Audio/WeaponAudioEntity.h"
#include "camera/CamInterface.h"
#include "cutscene/CutSceneManagerNew.h"
#include "Event/Events.h"
#include "Event/EventDamage.h"
#include "Event/EventGroup.h"
#include "event/EventMovement.h"
#include "event/EventShocking.h"
#include "Event/EventSound.h"
#include "Event/EventWeapon.h"
#include "event/ShockingEvents.h"
#include "Frontend/MiniMap.h"
#include "modelinfo/VehicleModelInfoExtensions.h"
#include "Network/Arrays/NetworkArrayHandlerTypes.h"
#include "Network/Arrays/NetworkArrayMgr.h"
#include "Network/Events/NetworkEventTypes.h"
#include "Network/Players/NetworkPlayerMgr.h"
#include "network/objects/entities/NetObjPlayer.h"
#include "Peds/Pedpopulation.h"
#include "Peds/Ped.h"
#include "peds/PedIntelligence.h"
#include "Physics/gtaInst.h"
#include "Physics/Physics.h"
#include "Renderer/Water.h"
#include "scene/EntityIterator.h"
#include "Scene/FocusEntity.h"
#include "task/Movement/TaskParachute.h"
#include "timecycle/TimeCycleConfig.h"
#include "vehicles/heli.h"
#include "Vehicles/Planes.h"
#include "Vehicles/Trailer.h"
#include "Vehicles/Metadata/VehicleSeatInfo.h"
#include "Vfx/Misc/Coronas.h"
#include "Vfx/Particles/PtFxManager.h"
#include "Vfx/Systems/VfxProjectile.h"
#include "Vfx/Systems/VfxWeapon.h"
#include "Vfx/VehicleGlass/VehicleGlassManager.h"
#include "Vfx/VfxHelper.h"
#include "Task/Movement/TaskParachute.h"
#include "Weapons/AirDefence.h"
#include "Weapons/Explosion.h"
#include "Weapons/Info/WeaponInfoManager.h"
#include "Weapons/Projectiles/ProjectileManager.h"
#include "Weapons/Projectiles/ProjectileRocket.h"
#include "Weapons/Weapon.h"
#include "Weapons/WeaponDamage.h"
#include "Weapons/WeaponEnums.h"
#include "Weapons/WeaponDebug.h"
#include "Weapons/WeaponTypes.h"

INSTANTIATE_RTTI_CLASS(CProjectile,0x49DC4195)

// Macro to disable optimisations if set
WEAPON_OPTIMISATIONS()

static dev_float PROJECTILE_CEILING_PLANE			= WORLDLIMITS_ZMAX - 45.0f;		// Using the limits is not enough due to update order

static dev_float s_fProjectileFriction					= 0.875f;
static dev_float s_fProjectileFlareFriction				= 1.5f;
static dev_float s_fProjectileFirstHitElasticity		= 0.4f;
static dev_float s_fProjectileFirstHitFriction			= 3.0f;
static dev_float s_fProjectileFlareFirstHitFriction		= 7.0f;

static dev_u32 s_uWhizzByEventCheckPeriodMS = 500;
static dev_float s_fWhizzByEventRadius = 2.0f; // peds within this distance to the flight path will get whizz by events
static dev_float s_fWhizzByEventMinVelocity = 4.0f; // projectile velocity minimum to generate whizz by events

const float CProjectile::sm_fMinDistSqForProjectileSonarBlip = 4.0f;

static bank_bool s_grenadeAmbientModMPOnly = true;

//////////////////////////////////////////////////////////////////////////
// CProjectile
//////////////////////////////////////////////////////////////////////////

bool CProjectile::IsEntityInvisibleCardboardBoxAttachedToVehicle(const CEntity& rEnt)
{
	if (rEnt.GetIsTypeObject())
	{
		static const u32 suCardboardBoxHash = ATSTRINGHASH("prop_cs_cardbox_01",0x4da19524);
		const CObject* pObject = static_cast<const CObject*>(&rEnt);
		if (!pObject->GetIsVisible() && pObject->GetAttachParent() && 
			static_cast<CEntity*>(pObject->GetAttachParent())->GetIsTypeVehicle() && 
			pObject->GetArchetype() && pObject->GetArchetype()->GetModelNameHash() == suCardboardBoxHash)
		{
			return true;
		}
	}
	return false;
}

CProjectile::CProjectile(const eEntityOwnedBy ownedBy, u32 uProjectileNameHash, u32 uWeaponFiredFromHash, CEntity* pOwner, float fDamage, eDamageType damageType, eWeaponEffectGroup effectGroup, const CNetFXIdentifier* pNetIdentifier, bool bCreatedFromScript)
: CObject(ownedBy, CWeaponInfoManager::GetInfo(uProjectileNameHash)->GetModelIndex())
, m_matPrevious(V_IDENTITY)
, m_vLastWhizzByPosition(Vector3::ZeroType)
, m_vExplodePos(Vector3::ZeroType)
, m_vExplodeNormal(Vector3::ZeroType)
, m_vOldSpeed(Vector3::ZeroType)
, m_vStickPos(Vector3::ZeroType)
, m_vStickNormal(Vector3::ZeroType)
, m_networkIdentifier(pNetIdentifier)
, m_uHash(uProjectileNameHash)
, m_pInfo(static_cast<const CAmmoProjectileInfo*>(CWeaponInfoManager::GetInfo(uProjectileNameHash)))
, m_uWeaponFiredFromHash(uWeaponFiredFromHash)
, m_pOwner(pOwner)
, m_taskSequenceId(-1)
, m_fAge(0.0f)
, m_fLifeTime(0.0f)
, m_fLifeTimeAfterImpact(0.0f)
, m_fLifeTimeAfterExplosion(0.0f)
, m_fTimeStepTimer(0.0f)
, m_fExplosionTime(0.0f)
, m_fLightIntensityMult(0.0f)
, m_fWaterLevel(0.0f)
, m_fDamage(fDamage)
, m_damageType(damageType)
, m_effectGroup(effectGroup)
, m_trailEvo(1.0f)
, m_FadeTime(0)
, m_lightBone(-1)
, m_pSound(NULL)
, m_pHitEntity(NULL)
, m_pOtherInst(NULL)
, m_pIgnoreDamageEntity(NULL)
, m_iOtherComponent(0)
, m_iOtherMaterialId(0)
, m_pStickEntity(NULL)
, m_iStickComponent(0)
, m_iStickMaterialId(0)
, m_vStickDeformation(Vector3::ZeroType)
, m_pHitPed(NULL)
, m_uDestroyTime(0)
, m_uExplodeTime(0)
, m_vDir(Vector3::ZeroType)
, m_pCollisionEntity(NULL)
, m_uLastWhizzByEventCheckTimeMS(0)
, m_iFlags(0)
, m_bThrownFromOutsideOfVehicle(false)
, m_bHitPedFriendly(false)
, m_bAppliedImpactDamage(false)
, m_bCreatedWithoutOwnerFromScript(bCreatedFromScript)
, m_bNetworkHasHitPlayer(false)
, m_bCanDetonateInstantly(true)
, m_bProximityMineTriggered(false)
, m_fProximityExplodeTimer(0.0f)
, m_bProximityMineTriggeredByVehicle(false)
, m_bProximityMineTriggeredByVehicleSpeed(0.0f)
, m_fProximityMineStuckTime(0.0f)
, m_bProximityMineActive(false)
, m_bProximityMineRepeatingDetonation(false)
, m_bProximityMineActivationSafeMode(true)
, m_bHasTriggeredAttachSound(false)
, m_bHideStuckProjectileInVehicle(false)
, m_bUseAirDefenceExplosion(false)
, m_uTimeAirDefenceExplosionTriggered(0)
, m_iLastTimeProcessedHomingAttractor(0)
, m_iTimeProjectileWasFired(0)
, m_vAirDefenceFireDirection(Vector3::ZeroType)
, m_ignoreDamageEntityAttachParent(false)
, m_vPositionFiredFrom(Vector3::ZeroType)
#if __BANK
, m_vLastWhizzByDebugHeadPosition(Vector3::ZeroType)
, m_vLastWhizzByDebugTailPosition(Vector3::ZeroType)
, m_bWhizzByEventCheckDebugDrawPending(false)
#endif // __BANK
{
	SetOwnedBy( ENTITY_OWNEDBY_GAME );

	if(NetworkInterface::IsGameInProgress() && !m_networkIdentifier.IsValid())
	{
		// projectiles without an identifier are assigned to our machine during a network game
		m_networkIdentifier.Set(NetworkInterface::GetLocalPhysicalPlayerIndex(), CNetFXIdentifier::GetNextFXId());
	}

	if(GetInfo()->GetShouldHideDrawable())
	{
		SetIsVisibleForModule( SETISVISIBLE_MODULE_GAMEPLAY, false, true );
	}

	bool bSetDamping = true;
	if (CBaseModelInfo* pBaseModelInfo = GetBaseModelInfo())
	{
		Assert(pBaseModelInfo->GetModelType() == MI_TYPE_WEAPON);

		bSetDamping = !static_cast<CWeaponModelInfo*>(pBaseModelInfo)->GetDampingTunedViaTunableObjects();
	}

	if (bSetDamping)
	{
		phInst* pInst = GetCurrentPhysicsInst();
		if (pInst)
		{
			phArchetype* pArch =  pInst->GetArchetype();
			if (pArch)
			{
				phArchetypeDamp* pInstArchDamp = dynamic_cast<phArchetypeDamp*>(pArch);			
				if (pInstArchDamp)
				{
					float fdamping = GetInfo()->GetDamping();
					Vector3 vDamping(fdamping, fdamping, fdamping);
					// Zero out the non-linearV2 velocity to maintain old damping values now that CWeaponModelInfo::SetPhysics sets all
					//  damping values. 
					// NOTE: This archetype is shared between all projectiles of this type. 
					pInstArchDamp->ActivateDamping(phArchetypeDamp::LINEAR_C, VEC3_ZERO);
					pInstArchDamp->ActivateDamping(phArchetypeDamp::LINEAR_V, VEC3_ZERO);
					pInstArchDamp->ActivateDamping(phArchetypeDamp::LINEAR_V2, vDamping);
					pInstArchDamp->ActivateDamping(phArchetypeDamp::ANGULAR_C, VEC3_ZERO);
					pInstArchDamp->ActivateDamping(phArchetypeDamp::ANGULAR_V, VEC3_ZERO);
					pInstArchDamp->ActivateDamping(phArchetypeDamp::ANGULAR_V2, VEC3_ZERO);
					pInstArchDamp->SetGravityFactor(GetInfo()->GetGravityFactor());
				}					
			}
		}	
	}

	if( GetBaseModelInfo() )
	{
		m_lightBone = GetInfo()->GetLightBone().GetBoneIndex(GetBaseModelInfo());
	}

	//B*1752582: Fixes bug where smoke grenades which have landed on the peds car fall through when the projectile owner enters it
	//If owner ped isn't in vehicle, set ThrownFromOutside flag (used in PreComputeImpacts)
	if(m_pOwner && m_pOwner->GetIsTypePed())
	{
		CPed* pOwnerPed = static_cast<CPed*>( m_pOwner.Get() ) ;
		if (!pOwnerPed->GetIsInVehicle())
		{
			m_bThrownFromOutsideOfVehicle = true;
		}

		if (NetworkInterface::IsGameInProgress())
		{
			s32 taskType = -1;

			if (pOwnerPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_THROW_PROJECTILE))
			{
				taskType = CTaskTypes::TASK_THROW_PROJECTILE;
			}
			else if (pOwnerPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_AIM_AND_THROW_PROJECTILE))
			{
				taskType = CTaskTypes::TASK_AIM_AND_THROW_PROJECTILE;
			}
			else if (pOwnerPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_MOUNT_THROW_PROJECTILE))
			{
				taskType = CTaskTypes::TASK_MOUNT_THROW_PROJECTILE;
			}

			if (taskType != -1)
			{
				m_taskSequenceId = pOwnerPed->GetPedIntelligence()->GetQueriableInterface()->GetTaskNetSequenceForType(taskType);
				
				if (Verifyf(m_taskSequenceId != -1, "Failed to find net sequence for task type %d", taskType) && 
					!m_networkIdentifier.IsValid())
				{
					// projectiles without an identifier are assigned to our machine during a network game
					m_networkIdentifier.Set(NetworkInterface::GetLocalPhysicalPlayerIndex(), CNetFXIdentifier::GetNextFXId());
				}
			}
		}
	}

	REPLAY_ONLY(CReplayMgr::RecordObject(this));
}

CProjectile::~CProjectile()
{
	if(m_pSound)
	{
		m_pSound->StopAndForget();
	}

	if(m_pInfo && m_pInfo->GetIsSticky())
	{
		if(NetworkInterface::IsGameInProgress() && m_networkIdentifier.IsValid())
		{
			CProjectileManager::RemoveNetSyncProjectile(this);
		}
	}

	// finish any proximity effect
	g_vfxProjectile.RemovePtFxProjProximity(this);

	// finish any fuse effect
	if(GetInfo()->GetFuseFxHashName().GetHash()!=0 || GetInfo()->GetFuseFirstPersonFxHashName().GetHash()!=0)
	{
		g_vfxProjectile.RemovePtFxProjFuse(this, true, true);
	}
	
	RestoreDamping();
}

bool CProjectile::sm_bProximityMineUseActivationSafeMode = true;
bool CProjectile::sm_bProximityMineUseBetterVehicleDetection = true;
float CProjectile::sm_fOppressor2MissilePitchYawRollClampOverride = 0.0f;
float CProjectile::sm_fOppressor2MissileTurnRateModifierOverride = 0.0f;

void CProjectile::InitTunables()
{
	sm_bProximityMineUseActivationSafeMode =			::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("PROX_MINE_USE_ACTIVATION_SAFE_MODE", 0xFE51FFC7), sm_bProximityMineUseActivationSafeMode);
	sm_bProximityMineUseBetterVehicleDetection =		::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("PROX_MINE_USE_BETTER_VEHICLE_DETECTION", 0x4785C94E), sm_bProximityMineUseBetterVehicleDetection);
	sm_fOppressor2MissilePitchYawRollClampOverride =	::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("OPPRESSOR2_MISSILE_PITCH_YAW_CLAMP", 0xBC857F81), sm_fOppressor2MissilePitchYawRollClampOverride);
	sm_fOppressor2MissileTurnRateModifierOverride =		::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("OPPRESSOR2_MISSILE_TURN_RATE_MODIFIER", 0x794AFEEA), sm_fOppressor2MissileTurnRateModifierOverride);
}

bool CProjectile::ProcessControl()
{
	// Reset the reset flags
	m_iFlags.SetAllFlags(m_iFlags.GetAllFlags() & ~PRF_ResetMask);

	m_fAge += fwTimer::GetTimeStep();

	if(m_iFlags.IsFlagSet(PF_UsingDestroyTimer))
	{
		if(m_uDestroyTime <= fwTimer::GetTimeInMilliseconds())
		{
			Displayf("CProjectile::ProcessControl: Destroy(). Reason: PF_UsingDestroyTimer: m_uDestroyTime <= fwTimer::GetTimeInMilliseconds.");
			Destroy();
			return true;
		}
	}

	if(m_iFlags.IsFlagSet(PF_UsingExplodeTimer))
	{
		if(m_uExplodeTime <= fwTimer::GetTimeInMilliseconds())
		{
			Displayf("CProjectile::ProcessControl: Explode(). Reason: PF_UsingExplodeTimer: m_uExplodeTime <= fwTimer::GetTimeInMilliseconds.");
			Explode(VEC3V_TO_VECTOR3(GetTransform().GetPosition()), m_vDir, m_pCollisionEntity, m_pCollisionEntity ? true : false, 0);	
			return true;
		}
	}

	// If the projectile has been thrown from a gun-aim throw don't allow it to be detonated until the player has released the detonate button
	if (!m_bCanDetonateInstantly)
	{
		CPed *pOwnerPed = static_cast<CPed*>(m_pOwner.Get());
		if (pOwnerPed && pOwnerPed->IsLocalPlayer() && pOwnerPed->GetControlFromPlayer() && pOwnerPed->GetControlFromPlayer()->GetPedThrowGrenade().IsReleased())
		{
			m_bCanDetonateInstantly = true;
		}
	}

	if(m_iFlags.IsFlagSet(PF_Active))
	{
		if (m_fLifeTimeAfterExplosion > 0.0f)
		{
			// Tick down the LifeTimeAfterExplosion lifetime timer
			m_fLifeTimeAfterExplosion -= fwTimer::GetTimeStep();
			if (m_fLifeTimeAfterExplosion <= 0.0f)
			{
				Displayf("CProjectile::ProcessControl: Destroy(). Reason: m_fLifeTimeAfterExplosion <= 0.0f.");
				Destroy();
			}
			return true;
		}

		if(m_iFlags.IsFlagSet(PF_UsingLifeTimeAfterImpact))
		{
			// Tick down the impact lifetime timer
			m_fLifeTimeAfterImpact -= fwTimer::GetTimeStep();

			if (GetInfo()->GetShouldStickToPeds() && !m_iFlags.IsFlagSet(PF_StuckToPed))
			{
				// Check if we have expired
				if(m_fLifeTimeAfterImpact <= 0.0f)
				{
					if (FadeOutProjectile())
					{
						Displayf("CProjectile::ProcessControl: Destroy(). Reason: GetInfo()->GetShouldStickToPeds() && !m_iFlags.IsFlagSet(PF_StuckToPed): m_fLifeTimeAfterImpact <= 0.0f.");
						Destroy();
						return true;
					}
				}
			}
			else
			{
				// Check if we have expired
				if(m_fLifeTimeAfterImpact <= 0.0f)
				{
					Displayf("CProjectile::ProcessControl: TriggerExplosion(). Reason: m_fLifeTimeAfterImpact <= 0.0f.");
					TriggerExplosion();
					return true;
				}

				CreatePotentialBlastEvent(m_fLifeTimeAfterImpact);
			}
		}
		
		if(m_iFlags.IsFlagSet(PF_UsingLifeTimer))
		{
			// Tick down the explosion timer
			m_fExplosionTime -= fwTimer::GetTimeStep();

			if(!m_iFlags.IsFlagSet(PF_ExplosionTriggered))
			{
				// Check if we have expired
				if(m_fExplosionTime <= 0.0f)
				{
					Displayf("CProjectile::ProcessControl: TriggerExplosion(). Reason: m_fExplosionTime <= 0.0f.");
					TriggerExplosion();
					return true;
				}
			}

			// Tick down the lifetime timer
			m_fLifeTime -= fwTimer::GetTimeStep();

			// Check if we have expired
			if(m_fLifeTime <= 0.0f)
			{
				// Projectile lifetime has expired and has no explosion (was only using trail vfx), so clean it up after a delay (5s)
				if (GetInfo()->GetHash() == AMMOTYPE_DLC_FLAREGUN && m_fLifeTime <= -5.0f)
				{
					Displayf("CProjectile::ProcessControl: Destroy(). Reason: GetInfo()->GetHash() == AMMOTYPE_DLC_FLAREGUN && m_fLifeTime <= -5.0f.");
					Destroy();
					return true;
				}

				TriggerExplosion();
				return true;
			}

			if(!m_iFlags.IsFlagSet(PF_ExplosionTriggered) && (m_fExplosionTime > 0.0f) && (m_fLifeTime > 0.0f))
			{
				CreatePotentialBlastEvent(Min(m_fExplosionTime, m_fLifeTime));
			}
		}

		// Explode the projectile if it's entered an air defence zone.
		u8 uDefenceZoneIndex = 0;
		if (CAirDefenceManager::IsPositionInDefenceZone(GetTransform().GetPosition(), uDefenceZoneIndex) && !m_bUseAirDefenceExplosion)
		{
#if __BANK
			if (CAirDefenceManager::ShouldRenderDebug())
			{
				grcDebugDraw::Sphere(GetTransform().GetPosition(), 0.05f, Color_blue, true, 100);
			}
#endif	// __BANK
			
			CAirDefenceZone *pDefZone = CAirDefenceManager::GetAirDefenceZone(uDefenceZoneIndex);
			if (pDefZone && pDefZone->IsEntityTargetable(m_pOwner))
			{
				if (pDefZone->ShouldFireWeapon())
				{
					// Shoot at the impact position.
					Vec3V vFiredFromPosition(V_ZERO);
					CAirDefenceManager::FireWeaponAtPosition(uDefenceZoneIndex, GetTransform().GetPosition(), vFiredFromPosition);

					// Only trigger explosion on local machine (explosion is synced).
					if (!NetworkUtils::IsNetworkClone(m_pOwner))
					{
						// Use overridden explosion vfx and fire direction.
						m_bUseAirDefenceExplosion = true;
						m_uTimeAirDefenceExplosionTriggered = fwTimer::GetTimeInMilliseconds();
						m_vAirDefenceFireDirection = VEC3V_TO_VECTOR3(Normalize(vFiredFromPosition - GetTransform().GetPosition()));
					}
				}
				else
				{
					// Just trigger simple instant explosion of projectile on local machine
					if (!NetworkUtils::IsNetworkClone(m_pOwner))
					{
						TriggerExplosion();
						return true;
					}
				}
			}
		}
		else if (m_bUseAirDefenceExplosion && (fwTimer::GetTimeInMilliseconds() - m_uTimeAirDefenceExplosionTriggered) > CAirDefenceManager::GetExplosionDetonationTime())
		{
			weaponDisplayf("CProjectile::ProcessControl: TriggerExplosion(). Reason: CAirDefenceManager::IsPositionInDefenceZone.");
			TriggerExplosion();
			return true;
		}

		// B*1893038: Destroy projectile if stuck to respawning ped
		if (!m_networkIdentifier.IsClone() && NetworkInterface::IsGameInProgress() && m_pStickEntity && m_pStickEntity.Get()->GetIsTypePed())
		{
			const CPed *pStuckPed = static_cast<CPed*>(m_pStickEntity.Get());
			if (pStuckPed && pStuckPed->GetNetworkObject() && (static_cast<CNetObjPlayer*>(pStuckPed->GetNetworkObject()))->GetRespawnInvincibilityTimer()>0)
			{
				weaponDisplayf("CProjectile::ProcessControl: Destroy(). Reason: GetRespawnInvincibilityTimer()>0.");
				Destroy();
				return true;
			}
		}

		// B*1894180: Destroy projectile if it's stuck to ped in spectator mode
		if (NetworkInterface::IsGameInProgress() && m_pStickEntity )
		{
			if(m_pStickEntity.Get()->GetIsTypePed())
			{
				const CPed *pPed = static_cast<const CPed*>(m_pStickEntity.Get());
				if (pPed && pPed->IsAPlayerPed())
				{
					CNetObjPlayer* pNetPlayer =  pPed->GetNetworkObject() ? SafeCast(CNetObjPlayer, pPed->GetNetworkObject()) : 0;
					if (pNetPlayer && pNetPlayer->IsSpectating())
					{
						Displayf("CProjectile::ProcessControl: Destroy(). Reason: IsSpectating().");
						m_iFlags.SetFlag(PF_StuckToSpectatorPedOrGhostVeh); 
						Destroy();
						return true;
					}
				}
			}
			else if(m_pStickEntity.Get()->GetIsTypeVehicle())
			{
				if(GetOwner() && NetworkInterface::IsDamageDisabledInMP(*m_pStickEntity.Get(), *GetOwner()))
				{
					m_iFlags.SetFlag(PF_StuckToSpectatorPedOrGhostVeh);
				}
				else 
				{
					m_iFlags.ClearFlag(PF_StuckToSpectatorPedOrGhostVeh);
				}
			}
		}

		bool bPassiveMode = false;
		if(NetworkInterface::IsGameInProgress())
		{
			if (m_pOwner && m_pOwner->GetIsTypePed())
			{
				CPed* pOwnerPed = static_cast<CPed *>(m_pOwner.Get());

				//! Also, if owner goes passive, prevent prox mines detonating as this could be exploited.
				if(pOwnerPed->IsPlayer() && pOwnerPed->GetNetworkObject())
				{
					CNetObjPlayer* pOwnerNetObjPlayer = static_cast<CNetObjPlayer*>(pOwnerPed->GetNetworkObject());
					if (pOwnerNetObjPlayer && pOwnerNetObjPlayer->IsPassiveMode())
					{
						bPassiveMode = true;
					}
				}
			}

			if (m_pStickEntity && m_pStickEntity->GetIsTypePed())
			{
				CPed* pStickyPed = static_cast<CPed *>(m_pStickEntity.Get());

				if(pStickyPed->IsPlayer() && pStickyPed->GetNetworkObject())
				{
					CNetObjPlayer* pStickyNetObjPlayer = static_cast<CNetObjPlayer*>(pStickyPed->GetNetworkObject());
					if (pStickyNetObjPlayer && pStickyNetObjPlayer->IsPassiveMode())
					{
						bPassiveMode = true;
					}
				}

				if(GetOwner() && NetworkInterface::IsDamageDisabledInMP(*pStickyPed, *GetOwner()))
				{
					bPassiveMode = true;
				}

				if(bPassiveMode)
				{
					DetachFromStickEntity();
				}
			}
		}

		// B*2148142: Proximity Mines - only detonate if stuck and enemy ped steps within the bomb explosion radius and has LOS to the bomb. Don't do this if a cutscene is running!
		if(!bPassiveMode && GetInfo()->GetIsProximityDetonation() && m_iFlags.IsFlagSet(PF_Sticked) && !CutSceneManager::GetInstance()->IsCutscenePlayingBack())
		{		
			bool bProximityExploded = ProcessProximityMine();
			if (bProximityExploded)
			{
				return true;
			}
		}

		// Destroy the projectile if it is outside the projectile ceiling
		
		Vec3V vNewProjectilePosition = GetTransform().GetPosition();  

		vNewProjectilePosition += VECTOR3_TO_VEC3V(GetVelocity() * fwTimer::GetTimeStep());
		if(!g_WorldLimits_AABB.ContainsPoint(vNewProjectilePosition) || vNewProjectilePosition.GetZf() - PROJECTILE_CEILING_PLANE > 0.0f )
		{
#if !__NO_OUTPUT
			if(vNewProjectilePosition.GetZf() - PROJECTILE_CEILING_PLANE <= 0.0f)
			{
				weaponWarningf("CProjectile is going outside of world limits. Position:%f,%f,%f, PF_UsingLifeTimer:%i, Script Controlled:%i, Name:%s", 
				            vNewProjectilePosition.GetXf(), vNewProjectilePosition.GetYf(), vNewProjectilePosition.GetZf(), m_iFlags.IsFlagSet(PF_UsingLifeTimer), GetIsScriptControlled(), GetInfo()->GetName());
			}
#endif //__NO_OUTPUT
			Displayf("CProjectile::ProcessControl: TriggerExplosion(). Reason: g_WorldLimits_AABB.");
			m_iFlags.SetFlag(PF_ForceExplosion);
			TriggerExplosion();
 			return true;
		}

		if(NetworkInterface::IsGameInProgress())
		{
			// Hack! Destroy sticky projectiles at the origin!
			const CAmmoProjectileInfo* pAmmoInfo = GetInfo();
			if(pAmmoInfo->GetIsSticky())
			{
				Vector3 vCurPos = VEC3V_TO_VECTOR3(vNewProjectilePosition);
				static const Vector3 vOrigin = VEC3_ZERO;
				if(vCurPos.IsClose(vOrigin, 0.2f))
				{
					Assertf(0, "CProjectile::ProcessControl: Deleting sticky projectile at the origin!");
					Destroy();
					return true;
				}
			}
		}

		// Trigger the explosion and start ticking down the lifetime timer.
		if(m_iFlags.IsFlagSet(PF_ExplodeNextFrame) && !m_iFlags.IsFlagSet(PF_ExplosionTriggered))
		{
			Displayf("CProjectile::ProcessControl: TriggerExplosion(). Reason: m_iFlags.IsFlagSet(PF_ExplodeNextFrame) && !m_iFlags.IsFlagSet(PF_ExplosionTriggered).");
			m_iFlags.SetFlag(PF_ForceExplosion);
			TriggerExplosion();
			m_fLifeTime -= m_fExplosionTime;
			m_iFlags.SetFlag(PF_UsingLifeTimer);
			m_iFlags.ClearFlag(PF_ExplodeNextFrame);
		}

		// Attract nearby homing missiles if they're close enough
		ProcessHomingAttractor();

		// Handle entering water
		ProcessInWater();

		// Process the audio
		ProcessAudio();

		// Process WhizzBy Events
		ProcessWhizzByEvents();
	}

	// Process the effects
	ProcessEffects();

	// B*1909349 - Hide projectiles that are stuck to peds in a vehicle to prevent clipping with seat (except bikes etc).
	if (m_pStickEntity && m_pStickEntity->GetIsTypePed())
	{
		const CPed *pPed = static_cast<const CPed*>(m_pStickEntity.Get());
		if (pPed && pPed->GetIsInVehicle() && pPed->GetVehiclePedInside())
		{
			const CVehicle *pVehicle = static_cast<const CVehicle*>(pPed->GetVehiclePedInside());
			if (pVehicle)
			{
				bool bIsInVehicleExclusions = pVehicle->InheritsFromBike() || pVehicle->InheritsFromQuadBike() || pVehicle->InheritsFromAmphibiousQuadBike() || pVehicle->GetIsJetSki();
				if (!bIsInVehicleExclusions && GetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY))
				{
					m_bHideStuckProjectileInVehicle = true;
					SetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY, false, true);
				}
			}
		}
		else if (!GetInfo()->GetShouldHideDrawable() && !pPed->GetIsInVehicle() && m_bHideStuckProjectileInVehicle)
		{
			m_bHideStuckProjectileInVehicle = false;
			SetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY, true, true);
		}
	}

	// Base class
	return CObject::ProcessControl();
}

void CProjectile::ApplyDeformation(const CVehicle *pAttachedVehicle, const void* basePtr, bool bInit)
{
	if(pAttachedVehicle && basePtr)
	{
		int iBoneIndex = GetHitBoneIndexFromFrag();
		const crSkeletonData& skeletonData = pAttachedVehicle->GetSkeletonData();
		Matrix34 boneMat;
		if(const crBoneData* pBoneData = skeletonData.GetBoneData(iBoneIndex))
		{
			// Use default translation because the wheels local matrix gets messed around with for rendering
			Quaternion defaultRotation = RCC_QUATERNION(pBoneData->GetDefaultRotation());
			boneMat.FromQuaternion(defaultRotation);
			boneMat.d = RCC_VECTOR3(pBoneData->GetDefaultTranslation());
			pBoneData = pBoneData->GetParent();
			while(pBoneData)
			{
				const Matrix34& matParent = pAttachedVehicle->GetLocalMtx(pBoneData->GetIndex());
				boneMat.Dot(matParent);
				pBoneData = pBoneData->GetParent();
			}
		}
		else
		{
			boneMat = pAttachedVehicle->GetLocalMtx(iBoneIndex);
		}


		if(fwAttachmentEntityExtension *extension = GetAttachmentExtension())
		{
			Vector3 vOffset;
			boneMat.Transform(extension->GetAttachOffset(), vOffset);

			if(bInit)
			{
				m_vStickDeformation = VEC3V_TO_VECTOR3(pAttachedVehicle->GetVehicleDamage()->GetDeformation()->ReadFromVectorOffset(basePtr, VECTOR3_TO_VEC3V(vOffset)));
			}
			else
			{
				vOffset -= m_vStickDeformation;
				Vector3 vNewDeformation = VEC3V_TO_VECTOR3(pAttachedVehicle->GetVehicleDamage()->GetDeformation()->ReadFromVectorOffset(basePtr, VECTOR3_TO_VEC3V(vOffset)));
				if(!vNewDeformation.IsClose(m_vStickDeformation, 0.001f))
				{
					vOffset += vNewDeformation;
					boneMat.UnTransform(vOffset);
					extension->SetAttachOffset(vOffset);
					m_vStickDeformation = vNewDeformation;
				}

			}
		}
	}
}

ePhysicsResult CProjectile::ProcessPhysics(float fTimeStep, bool bCanPostpone, s32 iTimeSlice)
{
	// GTAV - B*1790836 - Just return physics done if the physics inst isn't in the level.
	if( GetCurrentPhysicsInst()->GetLevelIndex() == phInst::INVALID_INDEX )
	{
		return PHYSICS_DONE;
	}

	if( m_iFlags.IsFlagSet(PF_Active) )
	{
		// make sure the grenade from grenade launcher play straight.
		// X axis is forward for this specific model (w_lr_40mm)
		phCollider* pCollider = GetCollider();
		if(pCollider && m_pInfo && (m_pInfo->GetAlignWithTrajectory() || m_pInfo->GetAlignWithTrajectoryYAxis()))
		{
			Vec3V vVelocity = pCollider->GetVelocity();
			ScalarV vVelocityMagSquared = MagSquared(vVelocity);
			ScalarV MINIMUM_VELOCITY_SQUARED(V_ONE);
			if (IsTrue(vVelocityMagSquared > MINIMUM_VELOCITY_SQUARED))
			{
				Vec3V vMotionDirection = vVelocity * InvSqrt(vVelocityMagSquared);
				Vec3V vAimDirection = m_pInfo->GetAlignWithTrajectoryYAxis() ? GetMatrix().b() : GetMatrix().a();

				Vec3V vCrossProduct = Cross(vAimDirection, vMotionDirection);
				ScalarV sSine = Mag(vCrossProduct);
				BoolV bTooSmall = sSine < ScalarV(V_FLT_MIN);
				Vec3V vRotationAxis = SelectFT(bTooSmall, InvScaleSafe(vCrossProduct, sSine), Vec3V(V_ZERO));
				ScalarV sAngle = SelectFT(bTooSmall, Arcsin(Min(ScalarV(V_ONE), sSine)), ScalarV(V_ZERO));

				Vec3V vAngVel = vRotationAxis * sAngle * Invert(ScalarV(fTimeStep));
				Assertf(IsFiniteAll(vAngVel), "Projectile Angular velocity is invalid. timestep = %f angle = %f rotation axis = %f %f %f",fTimeStep,  sAngle.Getf(), vRotationAxis.GetXf(), vRotationAxis.GetYf(), vRotationAxis.GetZf());
				pCollider->SetAngVelocity(RCC_VECTOR3(vAngVel));
			}
		}
		
		Vector3 vPos = CFocusEntityMgr::GetMgr().GetPos();

		// Determine which include flags to use based on distance
		float fDistToFocusSq = vPos.Dist2(VEC3V_TO_VECTOR3(GetTransform().GetPosition()));
		if (GetIsAttached())
		{
			CPhysics::GetLevel()->SetInstanceIncludeFlags(GetCurrentPhysicsInst()->GetLevelIndex(), u32(ArchetypeFlags::GTA_BASIC_ATTACHMENT_INCLUDE_TYPES));
		}
		else if(fDistToFocusSq < square(WEAPON_BOUNDS_PRESTREAM))
		{
			CPhysics::GetLevel()->SetInstanceTypeAndIncludeFlags(GetCurrentPhysicsInst()->GetLevelIndex(), ArchetypeFlags::GTA_PROJECTILE_TYPE, ArchetypeFlags::GTA_PROJECTILE_NEAR_INCLUDE_TYPES);
		}
		else
		{
			CPhysics::GetLevel()->SetInstanceTypeAndIncludeFlags(GetCurrentPhysicsInst()->GetLevelIndex(), ArchetypeFlags::GTA_PROJECTILE_TYPE, ArchetypeFlags::GTA_PROJECTILE_INCLUDE_TYPES);
		}
	}

	if( m_iFlags.IsFlagSet( PF_Sticked ) && m_pStickEntity )
	{
		if( m_pStickEntity->GetIsPhysical() )
		{
			CPhysical* pHitPhysical = static_cast<CPhysical*>( m_pStickEntity.Get() );
			fragInst* pFragInst = pHitPhysical->GetFragInst();
			if( pFragInst )
			{
				const float fProjectileWidth = GetBoundingBoxMax().GetZ() - GetBoundingBoxMin().GetZ();

				// Check for broken glass
				if( pHitPhysical->GetIsTypeVehicle() )
				{
					phBoundComposite* pBoundComposite = static_cast<phBoundComposite*>( pFragInst->GetArchetype()->GetBound() );
					if( pBoundComposite )
					{
						//	If this vehicle part is smashed then don't try to attach
						CVehicle* pVehicle = static_cast<CVehicle*>( pHitPhysical );
						if( pVehicle->IsHiddenFlagSet( m_iStickComponent ) )
						{
							// Detect how much has broken and when we overlap, detach from my parent entity
 							spdSphere projectileSphere;
 							projectileSphere.Set( RCC_VEC3V( m_vStickPos ), ScalarV( fProjectileWidth ) );
 							spdSphere smashSphere( CVehicleGlassManager::GetVehicleGlassComponentSmashSphere( pHitPhysical, m_iStickComponent, false ) );
 							if( projectileSphere.IntersectsSphere( smashSphere ) )
 							{
 								DetachFromStickEntity();
 							}
						}

						// Ensure we're still attached
						if(m_pStickEntity)
						{
							// Detach if we're on a window that's rolling/rolled down
							const CVehicle* pStickVehicle = static_cast<const CVehicle*>(pVehicle);
							for(int windowHierarchyId = VEH_FIRST_WINDOW; windowHierarchyId < VEH_LAST_WINDOW; ++windowHierarchyId)
							{
								if(pVehicle->GetFragmentComponentIndex((eHierarchyId)windowHierarchyId) == m_iStickComponent)
								{
									bool windowRollingDown = false;
									if(const CConvertibleRoofWindowInfo* pWindowExtension = pVehicle->GetBaseModelInfo()->GetExtension<CConvertibleRoofWindowInfo>())
									{
										windowRollingDown = (pStickVehicle->GetConvertibleRoofProgress() != 0.0f) && pWindowExtension->ContainsThisWindowId((eHierarchyId)windowHierarchyId);
									}

									if(pVehicle->IsWindowDown((eHierarchyId)windowHierarchyId) || windowRollingDown)
									{
										DetachFromStickEntity();
									}
									break;
								}
							}
						}
					}
				}
				// Check for non-vehicle cracked glass
				// Note: There is no easy way to 
				else if( pFragInst->GetCached() && pFragInst->GetCacheEntry()->GetHierInst() && m_iStickComponent < pFragInst->GetTypePhysics()->GetNumChildren() && pFragInst->GetChildBroken( m_iStickComponent ) )
				{
					Vec3V vStartPosition = GetTransform().GetPosition();
					Vec3V vEndPosition = vStartPosition + Scale( Negate( GetTransform().GetC() ), ScalarV( fProjectileWidth ) );

					WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
					WorldProbe::CShapeTestSingleResult capsuleResults;
					capsuleDesc.SetResultsStructure( &capsuleResults );
					capsuleDesc.SetCapsule( VEC3V_TO_VECTOR3( vStartPosition ), VEC3V_TO_VECTOR3( vEndPosition ), fProjectileWidth );
					capsuleDesc.SetExcludeEntity( this );
					capsuleDesc.SetIncludeFlags( ArchetypeFlags::GTA_GLASS_TYPE );
					capsuleDesc.SetTypeFlags( ArchetypeFlags::GTA_WEAPON_TEST );
					capsuleDesc.SetIsDirected( true );
					capsuleDesc.SetTreatPolyhedralBoundsAsPrimitives( false );

					if( !WorldProbe::GetShapeTestManager()->SubmitTest( capsuleDesc ) )
						DetachFromStickEntity();
				}
			}
		}
	}

	// B*2225632: Flare Gun - Lerp down the X/Y velocity if it's trajectory is close to the max pitch limits.
	// Allows players to shoot flares upwards which land near their original firing position.
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_uWeaponFiredFromHash);
	if (pWeaponInfo && pWeaponInfo->GetHash() == ATSTRINGHASH("WEAPON_FLAREGUN", 0x47757124))
	{
		const CAimingInfo* pAimingInfo = pWeaponInfo->GetAimingInfo();
		if (pAimingInfo)
		{
			// Calculate the projectile pitch
			Vector3 vVelocity = GetVelocity();
			float fProjectilePitch = 0.0f;
			Vector2 vDir;
			vVelocity.GetVector2XY(vDir);
			const float fVelMag = vVelocity.Mag();

			if (fVelMag != 0.0f)
			{
				fProjectilePitch = Sign(vVelocity.z) * rage::AcosfSafe(vDir.Mag() / fVelMag);
				fProjectilePitch = fwAngle::LimitRadianAngleForPitch(fProjectilePitch);

				const float fSweepPitchMax = pAimingInfo->GetSweepPitchMax() * DtoR;
				static dev_float fMaxPitchScaler = 0.95f;
				const float fPitchLimit = fSweepPitchMax * fMaxPitchScaler;

				// Is projectile pitch over the limit?
				if (fProjectilePitch > fPitchLimit)
				{
					// Lerp down X/Y velocity down to zero.
					// Scale lerp rate based on delta over pitch limit.
					static dev_float fMaxLerpRate = 0.025f;	// Max rate to use if aiming along max pitch limit
					static dev_float fMinLerpRate = 0.001f;	// Min rate to use if aiming along min pitch limit

					const float fPitchLimitsDelta = fSweepPitchMax - fPitchLimit;
					const float fActualPitchDelta = fSweepPitchMax - fProjectilePitch;

					float fLerpRateMult = fActualPitchDelta / fPitchLimitsDelta;
					float fLerpRate = fMaxLerpRate - (fLerpRateMult * fMaxLerpRate);
					fLerpRate = rage::Clamp(fLerpRate, fMinLerpRate, fMaxLerpRate);

					vVelocity.x = rage::Lerp(fLerpRate, vVelocity.x, 0.0f);
					vVelocity.y = rage::Lerp(fLerpRate, vVelocity.y, 0.0f);
					SetVelocity(vVelocity);
				}
			}
		}
	}

	//return PHYSICS_DONE;
	return CObject::ProcessPhysics(fTimeStep, bCanPostpone, iTimeSlice);
}

void CProjectile::ProcessCollision(phInst const * myInst, CEntity* pHitEnt, phInst const* hitInst, const Vector3& vMyHitPos,
								   const Vector3& vOtherHitPos, float fImpulseMag, const Vector3& vMyNormal, int iMyComponent,
								   int iOtherComponent, phMaterialMgr::Id iOtherMaterial, bool bIsPositiveDepth, bool bIsNewContact)
{
	if(pHitEnt && pHitEnt != m_pOwner)
	{
		phIntersection intersection;
		intersection.Set(hitInst->GetLevelIndex(), PHLEVEL->GetGenerationID(hitInst->GetLevelIndex()),RCC_VEC3V(vOtherHitPos), RCC_VEC3V(vMyNormal), 0.0f, 1.0f, 0, static_cast<rage::u16>(iOtherComponent), iOtherMaterial);  
		ProcessImpact(intersection);
	}

	CObject::ProcessCollision(myInst, pHitEnt, hitInst, vMyHitPos, vOtherHitPos, fImpulseMag, vMyNormal, iMyComponent, iOtherComponent,
		iOtherMaterial, bIsPositiveDepth, bIsNewContact);
}

void CProjectile::OnActivate(phInst* pInst, phInst* pOtherInst)
{
	// Must be called first or GetCollider won't work correctly
	CObject::OnActivate(pInst, pOtherInst);

	const CAmmoProjectileInfo* pAmmoInfo = GetInfo();
	if (pAmmoInfo->GetDoubleDamping())
	{
		if (phCollider* collider = GetCollider())
		{
			collider->SetDoubleDampingEnabled(true);
		}
	}
}

void CProjectile::PostPreRender()
{
	if (m_pInfo==NULL)
	{
		return;
	}

	// lights
	bool lightActive = m_lightBone>-1;
	if (m_pInfo->GetLightOnlyActiveWhenStuck() && m_iFlags.IsFlagSet(PF_Sticked)==false)
	{
		lightActive = false;
	}

	if (m_iFlags.IsFlagSet(PF_TrailInactive) && !m_iFlags.IsFlagSet(PF_Sticked))
	{
		lightActive = false;
	}

	if (!GetIsVisible())
	{
		lightActive = false;
	}

	if (GetInfo()->GetIsProximityDetonation() && !m_bProximityMineActive)
	{
		lightActive = false;
	}

	if (m_iFlags.IsFlagSet(PF_Exploded))
	{
		lightActive = false;
	}

	if (lightActive)
	{
		Matrix34 lightMatrix;
		GetGlobalMtx(m_lightBone, lightMatrix);

		Vector3 pos = lightMatrix.d;

		float time = (float)(CNetwork::GetSyncedTimeInMilliseconds() + (u32)GetRandomSeed());
		float mult = 1.0f;

		bool bUsingTimeStep = false;

		if (m_iFlags.IsFlagSet(PF_UsingLifeTimer) && m_pInfo->GetLightSpeedsUp())
		{
			TUNE_GROUP_FLOAT(EXPLOSIVE_TUNE, FLASH_START_PHASE, 0.5f, 0.0f, 1.0f, 0.01f);
			TUNE_GROUP_FLOAT(EXPLOSIVE_TUNE, FLASH_END_MULT, 3.0f, 1.0f, 5.0f, 0.01f);
			
			float fTimeMult = 1.0f;
			float fPhase = 1 - (m_fLifeTime / m_pInfo->GetLifeTime());
			if (fPhase > FLASH_START_PHASE)
			{
				float fDelta = (fPhase - FLASH_START_PHASE) / (1 - FLASH_START_PHASE);
				fTimeMult = 1.0f + ((FLASH_END_MULT - 1.0f) * fDelta);
			}

			bUsingTimeStep = true;
			m_fTimeStepTimer += (fwTimer::GetTimeStep() * 1000 * fTimeMult);
		}

		// B*2148142: Proximity mine has been triggered, flash rapidly until it explodes.
		if (m_bProximityMineTriggered)
		{
			bUsingTimeStep = true;
			m_fTimeStepTimer += (fwTimer::GetTimeStep() * 1000 * m_pInfo->GetProximityLightFrequencyMultiplierTriggered());
		}

		if (m_pInfo->GetLightFrequency()>0.0f)
		{
			float fSin = Sinf((bUsingTimeStep ? m_fTimeStepTimer : time) * m_pInfo->GetLightFrequency());
			mult = powf((fSin + 1.0f) / 2.0f, m_pInfo->GetLightPower());
		}

		if (m_pInfo->GetLightFlickers() && !fwTimer::IsGamePaused())
		{
			mult = g_DrawRand.GetRanged(0.5f, 1.0f);
		}

		Vec3V vLightColour = m_pInfo->GetLightColour();

		// Proximity mine: use alternate light colour while untriggered.
		if (GetInfo()->GetIsProximityDetonation() && !m_bProximityMineTriggered)
		{
			Vec3V vUntriggeredColour = m_pInfo->GetProximityLightColourUntriggered();
			if (!IsEqualAll(vUntriggeredColour, Vec3V(V_ZERO)))
			{
				vLightColour = vUntriggeredColour;
			}
		}

		CLightSource light(LIGHT_TYPE_POINT, LIGHTFLAG_FX | LIGHTFLAG_NO_SPECULAR, pos, VEC3V_TO_VECTOR3(vLightColour), m_pInfo->GetLightIntensity() * mult, LIGHT_ALWAYS_ON);
		light.SetRadius(m_pInfo->GetLightRange());
		light.SetInInterior(GetInteriorLocation());
		light.SetFalloffExponent(m_pInfo->GetLightFalloffExp());
		Lights::AddSceneLight(light);
		
		Color32 col(vLightColour);

		g_coronas.Register(RCC_VEC3V(pos),
			m_pInfo->GetCoronaSize(), 
			col, 
			m_pInfo->GetCoronaIntensity() * mult, 
			m_pInfo->GetCoronaZBias(), 
			Vec3V(V_X_AXIS_WZERO),
			0.0f,
			0.0f,
			0.0f,
			CORONA_DONT_REFLECT);

		if(m_pInfo->GetLightOnlyActiveWhenStuck())
		{
			if(mult > m_fLightIntensityMult)
			{
				if(!m_iFlags.IsFlagSet(PRF_LightIntensityGrowing))
				{
					// Trigger a beep on each cycle of the light flashing
					m_iFlags.SetFlag(PRF_TriggerBeep);
					m_iFlags.SetFlag(PRF_LightIntensityGrowing);
				}
			}
			else if(mult < m_fLightIntensityMult)
			{
				m_iFlags.ClearFlag(PRF_LightIntensityGrowing);
			}
		}

		m_fLightIntensityMult = mult;
	}
	else
	{
		m_fLightIntensityMult = 0.0f;
	}
}

//Use these if .dat values haven't been filled in yet
const float DEFAULT_SHOOT_FORCE = 50.0f;
const float DEFAULT_FRAG_IMPULSE = 100.0f;

void CProjectile::ProcessImpact(phIntersection& intersection)
{
	phInst* pOtherInst = intersection.GetInstance();
	CEntity* pHitEntity = CPhysics::GetEntityFromInst( pOtherInst );
	if( pHitEntity )
	{
		const CAmmoProjectileInfo* pAmmoInfo = GetInfo();
		const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_uWeaponFiredFromHash);
		bool bIsGlass = false;
		
		if (CPhysics::GetLevel()->GetInstanceTypeFlags( pOtherInst->GetLevelIndex() ) & ArchetypeFlags::GTA_GLASS_TYPE)
		{
			bIsGlass = true;

			phBound* pOtherBound = pOtherInst->GetArchetype()->GetBound();
			if (pOtherBound->GetType() == phBound::COMPOSITE)
			{
				phBoundComposite* pOtherComposite = static_cast<phBoundComposite*>(pOtherBound);
				if (u32* pTypeAndIncludeFlags = pOtherComposite->GetTypeAndIncludeFlags())
				{
					bIsGlass = (pOtherComposite->GetTypeFlags(pTypeAndIncludeFlags, intersection.GetComponent()) & ArchetypeFlags::GTA_GLASS_TYPE) != 0;
				}
			}
		}
		if( bIsGlass && !pAmmoInfo->GetIsSticky() && pWeaponInfo && pHitEntity->GetIsTypeObject() && pOtherInst->GetClassType() == PH_INST_FRAG_OBJECT )
		{
			float fShootForce  = pWeaponInfo->GetForce()       > 0 ?  pWeaponInfo->GetForce()       : DEFAULT_SHOOT_FORCE;
			float fFragImpulse = pWeaponInfo->GetFragImpulse() > 0 ?  pWeaponInfo->GetFragImpulse() : DEFAULT_FRAG_IMPULSE;
	
			fragInst *fInst = pHitEntity->GetFragInst();
			if(fInst)
			{				
				int component = intersection.GetComponent();
				fragTypeChild* child = fInst->GetTypePhysics()->GetAllChildren()[component];
				int groupIndex = child->GetOwnerGroupPointerIndex();
				fragTypeGroup *group = fInst->GetTypePhysics()->GetAllGroups()[groupIndex];

				if (group->GetMadeOfGlass() && group->GetStrength() > 0.0f)
				{
					fShootForce = group->GetStrength();
					if(fShootForce >= fFragImpulse)
						fFragImpulse += fShootForce;
				}
				
				Vector3 vForce = VEC3V_TO_VECTOR3(intersection.GetPosition() - GetMatrix().d());
				vForce.Normalize();
				vForce *= fShootForce;

				CObject *pHitObject = (CObject *)pHitEntity;

				//Set Crack Id for Explosions on Glass

				s32 crackId = 0;
				if(m_effectGroup >= 0)
				{
					crackId = g_vfxWeapon.GetBreakableGlassId(m_effectGroup);
				}

				CPhysics::SetSelectedCrack(crackId);
				pHitObject->ApplyExternalImpulse(
					vForce, 
					VEC3V_TO_VECTOR3(intersection.GetPosition() - pHitEntity->GetTransform().GetPosition()), 
					intersection.GetComponent(), 
					intersection.GetPartIndex(), 
					pOtherInst, 
					fFragImpulse
					);
			}
		}

		// Don't allow sticky bombs to fall asleep when touching peds (and not attached to them) -- peds can balance the sticky bomb and cause it to freeze mid-air
		if(pAmmoInfo->GetIsSticky() && pHitEntity && pHitEntity->GetIsTypePed() && !GetIsAttached())
		{
			phCollider* pCollider = GetCollider();
			if(pCollider)
			{
				phSleep* pSleep = pCollider->GetSleep();
				if(pSleep)
				{
					pSleep->Reset();
				}
			}
		}
	}
}

void CProjectile::ProcessPreComputeImpacts(phContactIterator impacts)
{
	const CAmmoProjectileInfo* pAmmoInfo = GetInfo();
	const bool bIsFlare = ( pAmmoInfo->GetHash() == atHashWithStringNotFinal("AMMO_FLAREGUN") );

	impacts.Reset();
	while( !impacts.AtEnd() )
	{
		// If this impact is from a constraint, let it pass through. One scenario is that when ped is holding an grenade while switching to ragdoll
		if(impacts.GetRootManifold().IsConstraint())
		{
			impacts++;
			continue;
		}

		// Should we even process impacts for this ammo type?
		// or are we processing the collision via shape tests?
		if( !pAmmoInfo->GetShouldProcessImpacts() || m_iFlags.IsFlagSet( PF_ProcessCollisionProbe ) )
		{
			impacts.DisableImpact();
			impacts++;
			continue;
		}

		RestoreDamping();

		if(m_bNetworkHasHitPlayer)
		{  
			Assertf(NetworkInterface::IsGameInProgress(),"Only expect m_bNetworkHasHitPlayer to be used in net games");
			impacts.SetElasticity(0.0f); // Make it harder to bounce off and away from players B*2031735
		}

		phInst* pOtherInst = impacts.GetOtherInstance();

		if(pOtherInst)
		{
			CEntity* pHitEntity = CPhysics::GetEntityFromInst( pOtherInst );

			if (pHitEntity)
			{
				if(GetOwner() && NetworkInterface::IsDamageDisabledInMP(*pHitEntity, *GetOwner()))
				{
					impacts.DisableImpact();
					impacts++;
					continue;
				}
				else if( m_ignoreDamageEntityAttachParent &&
						 m_pIgnoreDamageEntity &&
						 pHitEntity->GetAttachParent() == m_pIgnoreDamageEntity )
				{
					impacts.DisableImpact();
					impacts++;
					continue;
				}
				else if(pHitEntity->GetIsTypeVehicle())
				{
					CVehicle *pVehicle = static_cast<CVehicle*>(pHitEntity);
					if(pVehicle->InheritsFromBike() || pVehicle->InheritsFromQuadBike() || pVehicle->InheritsFromAmphibiousQuadBike())
					{
						int nOtherComponent = impacts.GetOtherComponent();

						CVehicleModelInfo* pVehicleModelInfo = pVehicle->GetVehicleModelInfo();

						bool bHitPedSeatBound = false;
						if(pVehicleModelInfo && pVehicleModelInfo->GetHasSeatCollision())
						{
							for(int i = 0; i < pVehicle->GetSeatManager()->GetMaxSeats(); ++i)
							{
								if(pVehicle->GetSeatManager()->GetPedInSeat(i))
								{
									s32 iFragChild = pVehicleModelInfo->GetFragChildForSeat(i);
									if(iFragChild == nOtherComponent)
									{
										bHitPedSeatBound = true;
										continue;
									}
								}
							}

							if(bHitPedSeatBound)
							{
								impacts.DisableImpact();
								impacts++;
								continue;
							}
						}
					}

					const CEntity* pNoCollisionEntity = (const CEntity*)GetNoCollisionEntity();
					// B*3648993: If the projectile is ignoring a trailer or its cab, ignore the other part of it.
					if (pNoCollisionEntity && GetIsEntityPartOfTrailer(pVehicle,pNoCollisionEntity))
					{
						impacts.DisableImpact();
						impacts++;
						continue;
					}
				}
			}
		}
		
		// Ignore all shoot through materials with an exception of glass since we want it to shatter
		bool bIsGlass = pOtherInst ? ( CPhysics::GetLevel()->GetInstanceTypeFlags( pOtherInst->GetLevelIndex() ) & ArchetypeFlags::GTA_GLASS_TYPE) > 0 : false;
		if(bIsGlass)
		{
			// If the instance is marked as being glass, ensure that we aren't hitting a non-glass part of the bound.
			const phBound* pOtherBound = pOtherInst->GetArchetype()->GetBound();
			if(phBound::IsTypeComposite(pOtherBound->GetType()))
			{
				const phBoundComposite* pOtherBoundComposite = static_cast<const phBoundComposite*>(pOtherBound);
				if(pOtherBoundComposite->GetTypeAndIncludeFlags() && ((pOtherBoundComposite->GetTypeFlags(impacts.GetOtherComponent()) & ArchetypeFlags::GTA_GLASS_TYPE) == 0))
				{
					bIsGlass = false;
				}
			}

#if __DEV
			if(bIsGlass)
			{
				weaponDebugf1("CProjectile::ProcessPreComputeImpacts - Disabling '%s' collision with '%s' (component %i) because it's made of glass.", GetModelName(), pOtherInst->GetArchetype()->GetFilename(), impacts.GetOtherComponent());
			}
#endif // __DEV
		}

		if(!bIsGlass || pAmmoInfo->GetIsSticky())
		{
			// Ignore impacts against car void materials
			bool bHitCarVoidMaterial = ( PGTAMATERIALMGR->UnpackMtlId(impacts.GetOtherMaterialId()) == PGTAMATERIALMGR->g_idCarVoid );
			if( bHitCarVoidMaterial )
			{
#if __DEV
				Vec3V otherLocalPos = impacts.GetInstanceA() == impacts.GetMyInstance() ? impacts.GetContact().GetLocalPosB() : impacts.GetContact().GetLocalPosA();
				weaponDebugf1("CProjectile::ProcessPreComputeImpacts - Disabling '%s' collision with '%s' (component %i, primitive %i, local pos <%f, %f, %f>) because it's car_void material..", GetModelName(), pOtherInst->GetArchetype()->GetFilename(), impacts.GetOtherComponent(), impacts.GetOtherElement(), VEC3V_ARGS(otherLocalPos));
#endif // __DEV
				impacts.DisableImpact();
				impacts++;
				continue;
			}

			float fDotNormalLimit = pAmmoInfo->GetRicochetTolerance();
			CEntity* pHitEntity = CPhysics::GetEntityFromInst( pOtherInst );
			if( pHitEntity )
			{
				// We should never hit the owner
				if( pHitEntity == m_pOwner )
				{
					weaponDebugf1("CProjectile::ProcessPreComputeImpacts - Disabling '%s' collision with '%s' because it is the owner.", GetModelName(), pOtherInst ? pOtherInst->GetArchetype()->GetFilename() : NULL);
					impacts.DisableImpact();
					impacts++;
					continue;
				}

				if(m_pOwner && m_pOwner->GetIsTypePed() )
				{
					CPed* pOwnerPed = static_cast<CPed*>( m_pOwner.Get() ) ;

					// Ignore impacts against your own mount or vehicle
					if( pHitEntity == pOwnerPed->GetMyMount() ||
						(pOwnerPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pHitEntity == pOwnerPed->GetMyVehicle() && !m_bThrownFromOutsideOfVehicle) )
					{
						weaponDebugf1("CProjectile::ProcessPreComputeImpacts - Disabling '%s' collision with '%s' because it is the owner's mount.", GetModelName(), pOtherInst ? pOtherInst->GetArchetype()->GetFilename() : NULL);
						impacts.DisableImpact();
						impacts++;
						continue;
					}

					// Also ignore impacts against other occupants of the same vehicle
					if (pHitEntity->GetIsTypePed() && pOwnerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle))
					{
						CPed* pHitPed = static_cast<CPed*>(pHitEntity);
						if (pHitPed->GetMyVehicle() == pOwnerPed->GetMyVehicle())
						{
							weaponDebugf1("CProjectile::ProcessPreComputeImpacts - Disabling '%s' collision with '%s' because they are both in the same vehicle.", GetModelName(), pOtherInst ? pOtherInst->GetArchetype()->GetFilename() : NULL);
							impacts.DisableImpact();
							impacts++;
							continue;
						}
					}
				}

				// B*5056893: OPPRESSOR2 missiles are hitting the driver ped's capsule when firing and driving fast in reverse (not sure why, possibly inheriting some backward velocity / lagging capsule collision)
				// We already set the ignore collision entity on the projectile (the vehicle) and can only have one, so here's an exception for the ped as well
				if (m_pOwner && m_pOwner->GetIsTypeVehicle())
				{
					const CVehicle* pOwnerVehicle = static_cast<CVehicle*>(m_pOwner.Get());
					if (MI_BIKE_OPPRESSOR2.IsValid() && pOwnerVehicle->GetModelIndex() == MI_BIKE_OPPRESSOR2)
					{
						// If the projectile still has the owner vehicle as the no collision entity, then the missile hasn't left the vehicle's bounding box yet
						if (pHitEntity == pOwnerVehicle->GetDriver() && GetNoCollisionEntity() == pOwnerVehicle)
						{
							weaponDebugf1("CProjectile::ProcessPreComputeImpacts - Disabling '%s' collision with '%s' because owner is OPPRESSOR2 and hit entity is driver ped.", GetModelName(), pOtherInst ? pOtherInst->GetArchetype()->GetFilename() : NULL);
							impacts.DisableImpact();
							impacts++;
							continue;
						}
					}
				}
				
				// B*1638542
				if (IsEntityInvisibleCardboardBoxAttachedToVehicle(*pHitEntity))
				{
					weaponDebugf1("CProjectile::ProcessPreComputeImpacts - Disabling '%s' collision with '%s' because it is an invisible cardboard box attached to a vehicle.", GetModelName(), pOtherInst ? pOtherInst->GetArchetype()->GetFilename() : NULL);
					impacts.DisableImpact();
					impacts++;
					continue;
				}

				// Calculate ricochet tolerance
				if( pHitEntity->GetIsTypeVehicle() )
				{
					fDotNormalLimit = pAmmoInfo->GetVehicleRicochetTolerance();
					impacts.SetElasticity(0.0f); // make it harder to bounce off vehicle
				}
				else if( pHitEntity->GetIsTypePed() )
				{
					m_pHitPed = static_cast<CPed *>(pHitEntity);

					if(NetworkInterface::IsGameInProgress() && m_pHitPed->IsPlayer())
					{
						m_bNetworkHasHitPlayer = true;	// Set this so to ensure projectile will always have elasticity turned off from now on
						impacts.SetElasticity(0.0f);	// which makes it harder to bounce off and away players. B*2031735
					}

					m_iFlags.SetFlag(PF_HitPedThisFrame);
					fDotNormalLimit = pAmmoInfo->GetPedRicochetTolerance();

					//B*1837611: Check if hit ped is friendly so we can determine whether or not to stick stickybombs to the peds back in CProjectile::ShouldStickToEntity
					if (m_pHitPed && m_pOwner && m_pOwner->GetIsTypePed())
					{
						const CPed* pOwnerPed = static_cast<const CPed*>( m_pOwner.Get());
						if (pOwnerPed)
						{
							const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_uWeaponFiredFromHash);
							if (pWeaponInfo)
							{
								m_bHitPedFriendly = !pOwnerPed->IsAllowedToDamageEntity(pWeaponInfo, m_pHitPed);
							}
						}
					}
				}
			}

			//If we hit a wall just after we fired from cover, we probably hit the cover
			if (m_iFlags.IsFlagSet(PF_FiredFromCover))
			{
				if (!pHitEntity || !pHitEntity->GetIsTypePed())
				{
					if(m_pOwner && m_pOwner->GetIsTypePed())
					{
						CPed* pOwnerPed = static_cast<CPed*>( m_pOwner.Get() );
						if (!pOwnerPed->GetPedResetFlag(CPED_RESET_FLAG_IsThrowingProjectileWhileAiming))
						{
							Vector3 pedPos = VEC3V_TO_VECTOR3(pOwnerPed->GetTransform().GetPosition());						
							pedPos.Subtract(VEC3V_TO_VECTOR3(impacts.GetOtherPosition()));
							Vector3 projectileVel = GetVelocity();
							static dev_float sf_MinTravelDistance = 2.0f;
							static dev_float sf_MinHorizontalVel = 4.0f;
							if (pedPos.XYMag() < sf_MinTravelDistance && projectileVel.XYMag() > sf_MinHorizontalVel)
							{
								Vector3 vMyNormal;
								impacts.GetMyNormal( vMyNormal );
								if (vMyNormal.z <= 0.7f) //do hit the ground.
								{
									weaponDebugf1("CProjectile::ProcessPreComputeImpacts - Disabling '%s' collision with '%s' because we fired from cover.", GetModelName(), pOtherInst ? pOtherInst->GetArchetype()->GetFilename() : NULL);
									impacts.DisableImpact();
									impacts++;			
									continue;
								}				
							}
						}
					}
				}
			}

			if(pHitEntity && pHitEntity->GetIsTypeVehicle() && ((CVehicle *)pHitEntity)->InheritsFromHeli())
			{
				CHeli *pHitHeli = (CHeli *)pHitEntity;
				if(pAmmoInfo->GetIsSticky() && pHitHeli->DoesBulletHitPropellerBound(impacts.GetOtherComponent()))
				{
					impacts.DisableImpact();
					impacts++;
					continue;
				}

				if(pHitHeli->IsPropeller(impacts.GetOtherComponent()) && !pHitHeli->DoesProjectileHitPropeller(impacts.GetOtherComponent()))
				{
					impacts.DisableImpact();
					impacts++;
					continue;
				}
			}

			// B*1954149: Do the same for plane propellers.
			if(pHitEntity && pHitEntity->GetIsTypeVehicle() && ((CVehicle *)pHitEntity)->InheritsFromPlane())
			{
				CPlane *pHitPlane = (CPlane *)pHitEntity;
				if(pAmmoInfo->GetIsSticky() && pHitPlane->DoesBulletHitPropellerBound(impacts.GetOtherComponent()))
				{
					impacts.DisableImpact();
					impacts++;
					continue;
				}

				if(pHitPlane->IsPropeller(impacts.GetOtherComponent()) && !pHitPlane->DoesProjectileHitPropeller(impacts.GetOtherComponent()))
				{
					impacts.DisableImpact();
					impacts++;
					continue;
				}
			}

			if(GetInfo()->GetShouldStickToPeds())
			{
				const bool bIsPedCapsule = pOtherInst ? ( CPhysics::GetLevel()->GetInstanceTypeFlags( pOtherInst->GetLevelIndex() ) & ArchetypeFlags::GTA_PED_TYPE) > 0 : false;
				if(pAmmoInfo->GetIsSticky() && bIsPedCapsule)
				{
					impacts.DisableImpact();
					impacts++;
					continue;
				}
			}			

			if(m_iFlags.IsFlagSet(PF_Active))
			{
				Vector3 vMyNormal;
				impacts.GetMyNormal( vMyNormal );
				float fDotNormal = rage::Abs( DotProduct( vMyNormal, VEC3V_TO_VECTOR3( GetTransform().GetB() ) ) );

				if(!m_iFlags.IsFlagSet(PF_AnyImpactDetected) && !pAmmoInfo->GetCanBounce())
				{
					// Tag that an impact has been detected
					m_iFlags.SetFlag(PF_AnyImpactDetected);

					if(!m_bNetworkHasHitPlayer)
					{
						impacts.SetElasticity( s_fProjectileFirstHitElasticity );
					}
#if __ASSERT
					else
					{
						Assertf(NetworkInterface::IsGameInProgress(),"Only expect m_bNetworkHasHitPlayer to be used in net games");
					}
#endif

					phArchetype* myArch = GetCurrentPhysicsInst()->GetArchetype();
					phBound* myBound = myArch != NULL ? myArch->GetBound() : NULL;
					if(myBound != NULL)
					{
						float projectileFriction = s_fProjectileFriction;
						const float closeProjectileFriction = s_fProjectileFriction * pAmmoInfo->GetFrictionMultiplier();

						if( m_pOwner && m_pOwner->GetIsTypePed() )
						{
							CPed* pOwnerPed = static_cast<CPed*>( m_pOwner.Get() );
							Vector3 pedPos = VEC3V_TO_VECTOR3(pOwnerPed->GetTransform().GetPosition());						
							pedPos.Subtract(VEC3V_TO_VECTOR3(impacts.GetOtherPosition()));
							static dev_float sf_MinDistanceForFrictionIncrease = 2.0f;
							static dev_float sf_MaxDistanceForFrictionIncrease = 10.0f;
							float distanceToPed = pedPos.XYMag();

							if( distanceToPed < sf_MinDistanceForFrictionIncrease )
							{
								// B*1928201: Allow projectiles to specify a friction multiplier so they can better control how much they will slide on the ground.
								projectileFriction = pAmmoInfo->GetFrictionMultiplier() * s_fProjectileFriction;
							}
							else if( distanceToPed < sf_MaxDistanceForFrictionIncrease )
							{
								distanceToPed -= sf_MinDistanceForFrictionIncrease;
								projectileFriction = closeProjectileFriction + ( ( s_fProjectileFirstHitFriction - closeProjectileFriction ) * ( distanceToPed / ( sf_MaxDistanceForFrictionIncrease - sf_MinDistanceForFrictionIncrease ) ) );									
								
								// B*1928201: Projectile friction multiplier is applied with strength that diminishes as the projectile moves towards the high-friction distance threshold.
								projectileFriction *= pAmmoInfo->GetFrictionMultiplier() * ( 1.0f - ( distanceToPed / ( sf_MaxDistanceForFrictionIncrease - sf_MinDistanceForFrictionIncrease ) ) );
							}
							else
							{
								// Keep projectiles that are fired over long distances accurate by preventing them from sliding away from the target.
								projectileFriction = s_fProjectileFirstHitFriction;
							}
						}
						else
						{
							projectileFriction = s_fProjectileFirstHitFriction;
						}

						
						if( bIsFlare )
						{
							impacts.SetFriction( s_fProjectileFlareFirstHitFriction );
						}
						else
						{
							impacts.SetFriction( projectileFriction );
						}
						impacts.SetMyPositionLocal(myBound->GetCGOffset());
					}
				}
				else
				{
					if( bIsFlare )
					{
						impacts.SetFriction( s_fProjectileFlareFriction );
					}
					else
					{
						impacts.SetFriction( s_fProjectileFriction );

						// If a bouncing projectile (ball), we also need to set this flag so that impact events can fire later on.
						if(pAmmoInfo->GetCanBounce())
						{
							m_iFlags.SetFlag(PF_AnyImpactDetected);
						}
					}
				}

				if(m_bNetworkHasHitPlayer)
				{   
					Assertf(NetworkInterface::IsGameInProgress(),"m_bNetworkHasHitPlayer only expected set in net games");
					//Keep friction high - reduces distance skittering away
					impacts.SetFriction( s_fProjectileFirstHitFriction );
				}

				if(fDotNormal > fDotNormalLimit ||
					( pHitEntity && pHitEntity->GetIsTypeObject() && 
					  static_cast< CObject* >( pHitEntity )->GetProjectilesShouldExplodeOnImpact() ) )
				{
					m_vExplodePos = VEC3V_TO_VECTOR3( impacts.GetOtherPosition() );
					m_vExplodeNormal = vMyNormal;
					m_pHitEntity = pHitEntity;
					m_pOtherInst = pOtherInst;
					m_iOtherComponent = impacts.GetOtherComponent();
					m_iOtherMaterialId = impacts.GetOtherMaterialId();
					m_iFlags.SetFlag(PF_Impacted);
				}

				if(pAmmoInfo->GetDelayUntilSettled())
				{
					if(!m_iFlags.IsFlagSet(PF_ExplosionTriggered) && GetAge() > GetInfo()->GetExplosionTime() && m_vOldSpeed.Mag2() < 0.1f)
					{
						m_vExplodePos = VEC3V_TO_VECTOR3( impacts.GetOtherPosition() );
						m_vExplodeNormal = vMyNormal;
						if(!pHitEntity || !pHitEntity->GetIsTypePed())
						{
							m_pHitEntity = pHitEntity;
						}
						m_pOtherInst = pOtherInst;
						m_iOtherComponent = impacts.GetOtherComponent();
						m_iOtherMaterialId = impacts.GetOtherMaterialId();
						m_iFlags.SetFlag(PF_Impacted);
						m_iFlags.SetFlag(PF_ExplodeNextFrame);
					}
				}

				// if the projectile has impacted then its explosion position will be set to the collision position
				// check if we need to override this to be the trail effect position instead
				if (m_iFlags.IsFlagSet(PF_Impacted) && pAmmoInfo->GetExplodeAtTrailFxPos())
				{
					CWeaponModelInfo* pModelInfo = static_cast<CWeaponModelInfo*>(GetBaseModelInfo());
					s32 trailBoneId = pModelInfo->GetBoneIndex(WEAPON_VFX_PROJTRAIL);
					Mat34V vBoneMtx;
					CVfxHelper::GetMatrixFromBoneIndex(vBoneMtx, this, trailBoneId);
					m_vExplodePos = VEC3V_TO_VECTOR3(vBoneMtx.GetCol3());
				}

				//B*1837611: Set m_pHitEntity m_pOtherInst for sticky bombs so we can apply a small amount of damage on stick.
				if (GetInfo()->GetExplosionTag() == EXP_TAG_STICKYBOMB)
				{
					m_pHitEntity = pHitEntity;
					m_pOtherInst = pOtherInst;
					m_vExplodePos = VEC3V_TO_VECTOR3( impacts.GetOtherPosition() );
					m_vExplodeNormal = vMyNormal;
				}
				else
				{
					// Determine whether or not we should toggle the life time flag on
					if(!m_iFlags.IsFlagSet(PF_UsingLifeTimeAfterImpact) && GetInfo()->GetLifeTimeAfterImpact() > 0.0f)
					{
						m_iFlags.SetFlag(PF_UsingLifeTimeAfterImpact);
					}
				}
			}
			
			// Process sticky collisions
			const float fProjectileWidth = GetBoundingBoxMax().GetZ() - GetBoundingBoxMin().GetZ();
			if( !m_iFlags.IsFlagSet( PF_ProcessCollisionProbe ) && pHitEntity && pAmmoInfo->GetIsSticky() && CProjectile::ShouldStickToEntity( pHitEntity, GetOwner(), fProjectileWidth, impacts.GetOtherPosition(), impacts.GetOtherComponent(), impacts.GetOtherMaterialId(), false, GetInfo()->GetShouldStickToPeds(), m_bHitPedFriendly ) )
			{
				// We do not actually want to impact with the object it sticks to otherwise it will apply an undesired impulse
				impacts.DisableImpact();
				m_iFlags.SetFlag( PF_ProcessCollisionProbe );
			}
		}

		impacts++;
	}
}

void CProjectile::ProcessPostPhysics()
{
	//B*1737659 - Moved sticky test from PostSimUpdate to ProcessPostPhysics to fix projectiles passing through peds before popping back to attach position
	if( m_iFlags.IsFlagSet( PF_ProcessCollisionProbe ) && !m_iFlags.IsFlagSet( PF_Sticked ) )
	{
		ProcessStickyShapeTest();
	}
	m_matPrevious = GetMatrix();

	if(m_pStickEntity)
	{
		StickToEntity();
	}

	CheckOwner();

	CPhysical::ProcessPostPhysics();
}

void CProjectile::PostSimUpdate()
{
	if(m_iFlags.IsFlagSet(PF_Impacted) && !m_iFlags.IsFlagSet(PF_DisableImpactExplosion))
	{
		//Check if the explosion position isn't under water before exploding
		//Example of this is Molotov hitting shallow water and the projectile position not classed as in water. 
		if(!GetIsInWater() && m_iFlags.IsFlagSet(PF_Active))
		{
			ProcessInWater(true);
			if(!m_iFlags.IsFlagSet(PF_Active))
			{
				m_iFlags.ClearFlag(PF_Impacted);
				return;
			}
		}

		bool bCheckForImpactDamage = true; 
        bool bForceExplodeOnImpact = m_pHitEntity && m_pHitEntity->GetIsTypeObject() && static_cast< CObject* >( m_pHitEntity.Get() )->GetProjectilesShouldExplodeOnImpact();

		if(GetInfo()->GetExplosionTag(m_pHitEntity) != EXP_TAG_DONTCARE)
		{	
			// If the explosion doesn't cause damage, we might want to apply impact damage later instead
			const CExplosionTagData& explosionData = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(GetInfo()->GetExplosionTag(m_pHitEntity));
			if (explosionData.damageAtCentre > 0.0f || explosionData.damageAtEdge > 0.0f)
			{
				bCheckForImpactDamage = false;
			}

			m_iFlags.SetFlag(PF_ForceExplosion);
			Explode(m_vExplodePos, m_vExplodeNormal, m_pHitEntity, true);
		}

		if(m_pHitEntity && (m_pHitEntity != m_pIgnoreDamageEntity) && !GetIsEntityPartOfTrailer(m_pHitEntity, m_pIgnoreDamageEntity) && GetInfo()->GetShouldApplyDamageOnImpact() && !m_bAppliedImpactDamage && bCheckForImpactDamage)
		{
			if(GetInfo()->GetDamage() > 0.0f)
			{
				// Setup a temp weapon
				CWeapon tempWeapon(m_uWeaponFiredFromHash, 1);

				// Setup a temp intersection from our parameters
				WorldProbe::CShapeTestHitPoint tempResult;
				tempResult.SetHitInst(m_pOtherInst->GetLevelIndex(), PHLEVEL->GetGenerationID(m_pOtherInst->GetLevelIndex()));
				tempResult.SetHitPosition(m_vExplodePos);
				tempResult.SetHitNormal(m_vExplodeNormal);
				tempResult.SetHitTValue(0.0f);
				tempResult.SetHitDepth(0.0f);
				tempResult.SetHitPartIndex(0);
				tempResult.SetHitComponent((u16)m_iOtherComponent);
				tempResult.SetHitMaterialId(m_iOtherMaterialId);

				WorldProbe::CShapeTestFixedResults<> tempResults;
				tempResults.Push(tempResult);

				// Apply impact damage
				CWeaponDamage::DoWeaponImpact(&tempWeapon, m_pOwner, VEC3V_TO_VECTOR3(GetTransform().GetPosition()), tempResults, GetInfo()->GetDamage(), CPedDamageCalculator::DF_IgnoreArmor);

                // Send a network damage event for the projectile hit
                if(NetworkInterface::IsGameInProgress() && !NetworkUtils::IsNetworkClone(m_pOwner) && NetworkUtils::IsNetworkClone(m_pHitEntity))
                {
                    CWeaponDamageEvent::Trigger(m_pOwner, m_pHitEntity, m_vExplodePos, m_iOtherComponent, false, m_uWeaponFiredFromHash, GetInfo()->GetDamage(), -1, -1, CPedDamageCalculator::DF_IgnoreArmor, 0, 0, 0);
					tempWeapon.SendFireMessage(m_pOwner, m_vExplodePos, tempResults, 1, true, GetInfo()->GetDamage(), CPedDamageCalculator::DF_IgnoreArmor);
                }

				m_bAppliedImpactDamage = true;
			}
		}

		if(GetInfo()->GetTrailFxRemovedOnImpact())
		{
			// Remove active particle effects
			g_ptFxManager.RemovePtFxFromEntity(this);
			m_iFlags.SetFlag(PF_TrailInactive);
		}

		if(GetInfo()->GetShouldBeDestroyedOnImpact() ||
            bForceExplodeOnImpact )
		{
			m_iFlags.SetFlag(PF_ForceExplosion);
			TriggerExplosion();
		}
	}


	//only make the sound event once and only if we detected an impact
	//handling events should be done at the end of physics not the beginning, which is why we check now 
	//instead of before during PreComputeImpacts
	//but dont react until the frame after we hit the ped...this will prevent getting replaced by the NMBalance task
	if(m_iFlags.IsFlagSet(PF_AnyImpactDetected) && !m_iFlags.IsFlagSet(PF_HitPedThisFrame) && !m_iFlags.IsFlagSet(PF_MadeSoundEvent)) 
	{
		m_iFlags.SetFlag(PF_MadeSoundEvent);
		CreateImpactEvents();
	}

	m_iFlags.ChangeFlag(PF_HitPedLastFrame, m_iFlags.IsFlagSet(PF_HitPedThisFrame));
	m_iFlags.ClearFlag(PF_HitPedThisFrame);
	if(!m_iFlags.IsFlagSet(PF_HitPedLastFrame))
	{
		m_pHitPed = NULL;
	}

	if(m_iFlags.IsFlagSet(PF_Active))
	{
		m_vOldSpeed = GetVelocity();
	}
}

void CProjectile::SetPosition(const Vector3& vec, bool bUpdateGameWorld, bool bUpdatePhysics, bool bWarp)
{
	CObject::SetPosition(vec, bUpdateGameWorld, bUpdatePhysics, bWarp);
	if (bWarp)
	{
		m_matPrevious = GetMatrix();
	}
}

void CProjectile::SetMatrix(const Matrix34& mat, bool bUpdateGameWorld, bool bUpdatePhysics, bool bWarp)
{
	CObject::SetMatrix(mat, bUpdateGameWorld, bUpdatePhysics, bWarp);
	if (bWarp)
	{
		m_matPrevious = GetMatrix();
	}
}

void CProjectile::Fire(const Vector3& vDirection, const f32 fLifeTime, f32 fLaunchSpeedOverride, bool bAllowDamping, bool bScriptControlled, bool bCommandFireSingleBullet, bool bIsDrop, const Vector3* vTargetVelocity, bool bDisableTrail, bool bAllowToSetOwnerAsNoCollision)
{
	m_iTimeProjectileWasFired = fwTimer::GetTimeInMilliseconds();
	m_vPositionFiredFrom = VEC3V_TO_VECTOR3(GetTransform().GetPosition());

	// Set as active
	m_iFlags.SetFlag(PF_Active);

	if (bDisableTrail)
	{
		m_iFlags.SetFlag(PF_TrailInactive);
	}

	InitLifetimeValues(fLifeTime);

	bool bBoom = false;
	if(m_iFlags.IsFlagSet(PF_UsingLifeTimer) && m_fExplosionTime==0.0f)
		bBoom = true;

	// Uses collision records
	SetRecordCollisionHistory(true);

	if(m_pOwner && m_pOwner->GetIsPhysical() && !m_pOwner->GetIsTypePed())
	{
		const Vector3 vLocalSpeed = static_cast<CPhysical&>(*m_pOwner).GetLocalSpeed(VEC3V_TO_VECTOR3(GetTransform().GetPosition()), true);

		// Hack to remove velocity impulse for torpedoes on the Kosatka
		if(MI_SUB_KOSATKA.IsValid() && m_pOwner->GetModelIndex()==MI_SUB_KOSATKA && m_uWeaponFiredFromHash == ATSTRINGHASH("VEHICLE_WEAPON_KOSATKA_TORPEDO", 0x62E2140E))
		{
			const Vector3 vLocalSpeedXY = Vector3(vLocalSpeed.x, vLocalSpeed.y, 0.0f);
			Displayf("CProjectile::Fire: SetVelocity - vLocalSpeed(%.2f,%.2f,%.2f)", vLocalSpeedXY.GetX(), vLocalSpeedXY.GetY(), vLocalSpeedXY.GetZ());
			SetVelocity(vLocalSpeedXY);
		}
		else
		{
			Displayf("CProjectile::Fire: SetVelocity - vLocalSpeed(%.2f,%.2f,%.2f)", vLocalSpeed.GetX(), vLocalSpeed.GetY(), vLocalSpeed.GetZ());
			SetVelocity(vLocalSpeed);
		}
	}

	CPed* pPed = NULL;

	if(m_pOwner && m_pOwner->GetIsTypePed())
	{
		pPed = static_cast<CPed*>(m_pOwner.Get());
	}

	

	// Apply any initial impulse
	if(!GetAsProjectileRocket() && !bBoom)
	{
		float fLaunchSpeed = fLaunchSpeedOverride != -1.0f? fLaunchSpeedOverride : GetInfo()->GetLaunchSpeed();		
		Vector3 vLaunchVelocity = vDirection * fLaunchSpeed;
		
		Vector3 vReferenceFrameVelocity(0.0f,0.0f,0.0f);
		if (pPed)
		{
			if(GetHash() == ATSTRINGHASH("AMMO_BIRD_CRAP", 0x4298C094))
			{
				audSoundInitParams initParams;
				initParams.TrackEntityPosition = true;
				audSoundSet soundSet;
				soundSet.Init(ATSTRINGHASH("PEYOTE_BIRD_POOP_SOUNDS", 0x6DCF1C4B));
				if(soundSet.IsInitialised() && pPed->GetPedAudioEntity())
				{
					pPed->GetPedAudioEntity()->CreateAndPlaySound(soundSet.Find(ATSTRINGHASH("POOP", 0x21F0FA65)), &initParams);
				}
			}

			//vReferenceFrameVelocity = pPed->GetVelocity();  B* 1415564 Leave ped velocity zero if they are not riding something, to ensure accurate hits on reticule
			if (!bIsDrop)
			{
				//Add vehicle velocity modifiers 
				if ( pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle) && pPed->GetMyVehicle()) //add vehicle velocity
				{
					vReferenceFrameVelocity = pPed->GetMyVehicle()->GetVelocity();
				} 
				else if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_OnMount) && pPed->GetMyMount())
				{
					vReferenceFrameVelocity = pPed->GetMyMount()->GetVelocity();
				}
				else if (pPed->GetPedResetFlag(CPED_RESET_FLAG_IncludePedReferenceVelocityWhenFiringProjectiles))
				{
					vReferenceFrameVelocity = pPed->GetVelocity();
				}
				else if (pPed->GetPedResetFlag(CPED_RESET_FLAG_IsParachuting))
				{
					const CTaskParachute* pParachuteTask = static_cast<const CTaskParachute*>(pPed->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_PARACHUTE)); 
					if(pParachuteTask && pParachuteTask->GetParachute())
					{
						vReferenceFrameVelocity = pParachuteTask->GetParachute()->GetVelocity();
					}
				}
#if ENABLE_JETPACK
				else if (pPed->GetPedResetFlag(CPED_RESET_FLAG_IsUsingJetpack) && pPed->GetHasJetpackEquipped())
				{
					vReferenceFrameVelocity = pPed->GetVelocity();
				}
#endif
			}
		}

		
		
		// If we're trying to hit a target add the target velocity regardless of how fast we're going, otherwise use the reference frame velocity. 
		vLaunchVelocity += vTargetVelocity ? *vTargetVelocity : vReferenceFrameVelocity;
#if __BANK
		const Vector3 vTargetVelocityDebug = vTargetVelocity ? *vTargetVelocity : Vector3(0.0f, 0.0f, 0.0f);
		Displayf("CProjectile::Fire: SetVelocity - vLaunchVelocity(%.2f,%.2f,%.2f), vTargetVelocity(%.2f,%.2f,%.2f), vReferenceFrameVelocity(%.2f,%.2f,%.2f)", vLaunchVelocity.GetX(), vLaunchVelocity.GetY(), vLaunchVelocity.GetZ(), vTargetVelocityDebug.GetX(), vTargetVelocityDebug.GetY(), vTargetVelocityDebug.GetZ(), vReferenceFrameVelocity.GetX(), vReferenceFrameVelocity.GetY(), vReferenceFrameVelocity.GetZ());
#endif	// __BANK
		SetVelocity(vLaunchVelocity);

		phCollider* pCollider = GetCollider();
		if(pCollider)
		{
			if (!bAllowDamping)
			{
				// Completely disable damping
				pCollider->SetDampingEnabled(false);
			}
			else
			{
				// If we aren't totally getting rid of damping, just set a temporary reference frame velocity
				//   so we slowly transition into becoming damped. 
				pCollider->SetReferenceFrameVelocity(vReferenceFrameVelocity);
				TUNE_GROUP_FLOAT(PROJECTILE_THROW_TUNE, REFERENCE_FRAME_DAMPING_RATE, 0.2f,0.0f,5.0f,0.1f);
				pCollider->SetReferenceFrameVelocityDampingRate(REFERENCE_FRAME_DAMPING_RATE);
			}
		}

		// make it spin clockwise
		Vector3 vAngDireciton = CrossProduct(vDirection, Vector3(0.0f, 0.0f, 1.0f));
		float fCentroidRadius = 0.0f;
		Vector3 vWorldCentroid;
		fCentroidRadius = GetBoundCentreAndRadius(vWorldCentroid);
		ApplyAngImpulse(vAngDireciton * fLaunchSpeed * GetMass(), Vector3(0.0f, 0.0f, fCentroidRadius*0.8f), 0, false, IF_InitialImpulse);
	}
	else
	{
		if (pPed)
		{
			Vector3 vGroundVelocity = VEC3V_TO_VECTOR3(pPed->GetGroundVelocity());		
			Displayf("CProjectile::Fire: SetVelocity - vGroundVelocity(%.2f,%.2f,%.2f)", vGroundVelocity.GetX(), vGroundVelocity.GetY(), vGroundVelocity.GetZ());
			SetVelocity(vGroundVelocity);
		}
		else if (m_pOwner && m_pOwner->GetIsTypeVehicle())
		{
			CVehicle *pVehicle = NULL;
			pVehicle = static_cast<CVehicle*>(m_pOwner.Get());

			if (pVehicle)
			{
				Vector3 vVehicleVelocity = pVehicle->GetVelocity();

				// Hack to remove velocity impulse for torpedoes on the Kosatka
				if(MI_SUB_KOSATKA.IsValid() && pVehicle->GetModelIndex()==MI_SUB_KOSATKA && m_uWeaponFiredFromHash == ATSTRINGHASH("VEHICLE_WEAPON_KOSATKA_TORPEDO", 0x62E2140E))
				{
					const Vector3 vVehicleVelXY = Vector3(vVehicleVelocity.x, vVehicleVelocity.y, 0.0f);
					Displayf("CProjectile::Fire: SetVelocity - vVehicleVelocity(%.2f,%.2f,%.2f)", vVehicleVelXY.GetX(), vVehicleVelXY.GetY(), vVehicleVelXY.GetZ());
					SetVelocity(vVehicleVelXY);
				}
				else
				{
					Displayf("CProjectile::Fire: SetVelocity - vVehicleVelocity(%.2f,%.2f,%.2f)", vVehicleVelocity.GetX(), vVehicleVelocity.GetY(), vVehicleVelocity.GetZ());
					SetVelocity(vVehicleVelocity);
				}
			}
		}
	}

	if (bAllowToSetOwnerAsNoCollision)
		SetNoCollision(m_pOwner, NO_COLLISION_RESET_WHEN_NO_BBOX);

	// Create projectile across the network
	if(NetworkInterface::IsGameInProgress())
	{
		if (m_pOwner && m_pOwner->GetIsTypeVehicle())
		{
			CVehicle *pVehicle = NULL;
			pVehicle = static_cast<CVehicle*>(m_pOwner.Get());

			if (pVehicle)
			{
				if(MI_SUB_KOSATKA.IsValid() && (pVehicle->GetModelIndex()==MI_SUB_KOSATKA) && (m_uWeaponFiredFromHash == ATSTRINGHASH("VEHICLE_WEAPON_KOSATKA_TORPEDO", 0x62E2140E)))
				{
					GetEventScriptNetworkGroup()->Add(CEventNetworkFiredVehicleProjectile(pVehicle, pVehicle->GetDriver(), this, m_uWeaponFiredFromHash));
				}
			}
		}

		if(!m_networkIdentifier.IsClone())
		{
			float fSynchedLaunchSpeedOverride = fLaunchSpeedOverride;

			CProjectileRocket* pRocket= GetAsProjectileRocket();

			if(pRocket && pRocket->GetLauncherSpeed()>0.0f)
			{
				fSynchedLaunchSpeedOverride = pRocket->GetLauncherSpeed();
			}

			if(GetWeaponFiredFromHash() == ATSTRINGHASH("WEAPON_FLAREGUN", 0x47757124))
			{
				SetTaskSequenceId(CProjectileManager::AllocateNewFlareSequenceId());
			}
			else if(pRocket)
			{
				if(pPed && pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_AIM_GUN_ON_FOOT))
				{   //Coordinate with TASK_AIM_GUN_ON_FOOT and sync task sequence when firing rockets B*1972374
					pRocket->SetTaskSequenceId(pPed->GetPedIntelligence()->GetQueriableInterface()->GetTaskNetSequenceForType(CTaskTypes::TASK_AIM_GUN_ON_FOOT));
				}
			}

			CStartProjectileEvent::Trigger(this, vDirection, bCommandFireSingleBullet, bAllowDamping, fSynchedLaunchSpeedOverride);

			// Fire out a script event if this is a special ped firing a special kind of projectile. Script need to pick it up
			if(pPed && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_FiresDummyRockets))
			{
				GetEventScriptNetworkGroup()->Add(CEventNetworkFiredDummyProjectile(pPed, this, m_uWeaponFiredFromHash));			
			}
		}
		else if(GetAsProjectileRocket())
		{
			//Check what to do with clone rockets and our vehicle if we are a passenger
			CVehicle* pVehicle = CGameWorld::FindLocalPlayerVehicle();
			if( pVehicle && pVehicle->IsNetworkClone() )
			{
				//We're passenger so don't allow collision with our vehicle to destroy or detonate rocket, just turn off collision to allow 
				//the rocket to continue on its path with its trail if comes close. 
				//Only the owner/driver of the vehicle should decide the end point and explosion result of the projectile
				SetNoCollision(pVehicle, NO_COLLISION_NEEDS_RESET);
			}

			// Fire out a script event if this is a special ped firing a special kind of projectile. Script need to pick it up
			if(pPed && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_FiresDummyRockets))
			{
				GetEventScriptNetworkGroup()->Add(CEventNetworkFiredDummyProjectile(pPed, this, m_uWeaponFiredFromHash));			
			}
		}
	}

	// Cache off whether or not this projectile is script controlled
	SetIsScriptControlled( bScriptControlled );

	if (bBoom) 
	{
		//attach to ped in case it is moving B* 1088915
		if(m_pOwner && m_pOwner->GetIsTypePed())
		{
			CPed* pPed = static_cast<CPed*>(m_pOwner.Get());		
			Vector3 vAttachOffset = VEC3_ZERO;
			//TODO: left hand?  Offset?
			AttachToPhysicalBasic(pPed, s16(pPed->GetBoneIndexFromBoneTag(BONETAG_R_PH_HAND)), ATTACH_STATE_BASIC|ATTACH_FLAG_DELETE_WITH_PARENT, &vAttachOffset, NULL);
		}
		m_iFlags.SetFlag(PF_ForceExplosion);
		TriggerExplosion();
	}
}

void CProjectile::RestoreDamping()
{
	if(phCollider* pCollider = GetCollider())
	{
		pCollider->SetDampingEnabled(true);
	}
}

void CProjectile::OnAttachment()
{
	if(m_pStickEntity && m_pStickEntity->GetIsTypeVehicle())
	{
		CVehicle *pStickVehicle = (CVehicle *)m_pStickEntity.Get();
		const void *basePtr = NULL;
		if(pStickVehicle->GetVehicleDamage() && pStickVehicle->GetVehicleDamage()->GetDeformation() && pStickVehicle->GetVehicleDamage()->GetDeformation()->HasDamageTexture())
		{
			basePtr = pStickVehicle->GetVehicleDamage()->GetDeformation()->LockDamageTexture(grcsRead); //Lock the texture once for all wheels
		}
		if(basePtr)
		{
			ApplyDeformation(pStickVehicle, basePtr, true);
			pStickVehicle->GetVehicleDamage()->GetDeformation()->UnLockDamageTexture();
		}
	}

	m_iFlags.SetFlag(PF_Sticked);

	// Set as active
	m_iFlags.SetFlag(PF_Active);

	if(m_pHitEntity && (m_pHitEntity != m_pIgnoreDamageEntity) && !GetIsEntityPartOfTrailer(m_pHitEntity, m_pIgnoreDamageEntity) && m_pOtherInst && !NetworkUtils::IsNetworkClone(m_pHitEntity))
	{
		bool bOverrideStickyBombDamage = false;
		if (m_pHitEntity->GetIsTypePed())
		{
			m_iFlags.SetFlag(PF_StuckToPed);
			bOverrideStickyBombDamage =  !m_bHitPedFriendly && NetworkInterface::IsGameInProgress() && GetInfo()->GetExplosionTag() == EXP_TAG_STICKYBOMB;
		}

		if(GetInfo()->GetDamage() > 0.0f || bOverrideStickyBombDamage)
		{
			//Set weapon type to WEAPONTYPE_SMOKEGRENADE if we're applying a small amount of sticky hit damage
			//Can't use WEAPONTYPE_STICKYBOMB as causes explosion reaction, and can't use WEAPONTYPE_FALL as some code in CEventDamage::ComputePersonalityResponseToDamage clears the threat response if that's the damage type
			u32 uWeaponFiredFromHashCached = m_uWeaponFiredFromHash;
			if (bOverrideStickyBombDamage)
			{
				m_uWeaponFiredFromHash = WEAPONTYPE_SMOKEGRENADE;
			}

			// Setup a temp weapon
			CWeapon tempWeapon(m_uWeaponFiredFromHash, 1);

			// Setup a temp intersection from our parameters
			WorldProbe::CShapeTestHitPoint tempResult;
			tempResult.SetHitInst(m_pOtherInst->GetLevelIndex(), PHLEVEL->GetGenerationID(m_pOtherInst->GetLevelIndex()));
			tempResult.SetHitPosition(m_vExplodePos);
			tempResult.SetHitNormal(m_vExplodeNormal);
			tempResult.SetHitTValue(0.0f);
			tempResult.SetHitDepth(0.0f);
			tempResult.SetHitPartIndex(0);
			tempResult.SetHitComponent((u16)m_iOtherComponent);
			tempResult.SetHitMaterialId(m_iOtherMaterialId);

			WorldProbe::CShapeTestFixedResults<> tempResults;
			tempResults.Push(tempResult);

			CPed *pHitPed = NULL;
			if (bOverrideStickyBombDamage && m_pHitEntity->GetIsTypePed())
			{
				CEntity *pEntity = m_pHitEntity;
				if (pEntity)
				{
					pHitPed = static_cast<CPed*>(pEntity);
				}
			}

			//Disable ragdolling if we're doing stickybomb stick damage
			bool bIsRagdollEnabled = pHitPed ? pHitPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromFalling) : false;
			bool bDisableRagdoll = pHitPed && bOverrideStickyBombDamage;
			if (bDisableRagdoll)
				pHitPed->SetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromFalling, true);

			bool bGenerateVFX = true;
			if (bOverrideStickyBombDamage) 
			{
				bGenerateVFX = false;
			}

			// Apply damage
			const float fStickyBombStickDamage = 0.1f;
			CWeaponDamage::DoWeaponImpact(&tempWeapon, m_pOwner, VEC3V_TO_VECTOR3(GetTransform().GetPosition()), tempResults, bOverrideStickyBombDamage ? fStickyBombStickDamage : GetInfo()->GetDamage(), CPedDamageCalculator::DF_None, false, false, NULL, NULL,false,NULL,1.f,1.f,1.f,false, bGenerateVFX);

			// Restore the weapon hash to what it was before we changed it to WEAPONTYPE_SMOKEGRENADE
			if (bOverrideStickyBombDamage)
			{
				m_uWeaponFiredFromHash = uWeaponFiredFromHashCached;
			}

			//Re-enable ragdolling
			if (bDisableRagdoll)
				pHitPed->SetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromFalling, bIsRagdollEnabled);

			// send a network damage event for the projectile hit
			if(NetworkInterface::IsGameInProgress() && !NetworkUtils::IsNetworkClone(m_pOwner) && NetworkUtils::IsNetworkClone(m_pHitEntity))
			{
				CWeaponDamageEvent::Trigger(m_pOwner, m_pHitEntity, m_vExplodePos, m_iOtherComponent, false, m_uWeaponFiredFromHash, 0.0f, -1, -1, CPedDamageCalculator::DF_None, 0, 0, 0);
			}
		}
	}
}

void CProjectile::Destroy()
{
	// Make sure the projectile is added to the scene update.
	if (!GetIsOnSceneUpdate())
	{
		AddToSceneUpdate();
	}

	// Only delete projectile objects that are not already about to be deleted
	if(IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD))
	{
		return;
	}

	// Deactivate
	m_iFlags.ClearFlag(PF_Active);

	// Mark to be deleted
	SetBaseFlag(fwEntity::REMOVE_FROM_WORLD);

	// Hide - as it wont get deleted this frame
	SetIsVisibleForModule( SETISVISIBLE_MODULE_GAMEPLAY, false, true );

	// Turn off collision
	RemovePhysics();
	DisableCollision();

	const CEntity* pOwnerConst = GetOwner();
	if(!m_iFlags.IsFlagSet(PF_NoStickyBombOwnership) && pOwnerConst && pOwnerConst->GetIsTypePed())
	{
		CEntity* pOwner = const_cast<CEntity*>(pOwnerConst);
		CPed* pPedOwner = static_cast<CPed*>(pOwner);
		if(m_pInfo && m_pInfo->GetIsSticky())
		{
			if (m_pInfo->GetShouldStickToPeds())
			{
				pPedOwner->DecrementStickToPedProjectileCount();
			}
			else
			{
				pPedOwner->DecrementStickyCount();
			}
		}
		else if (m_pInfo && m_pInfo->GetHash() == AMMOTYPE_DLC_FLAREGUN)
		{
			pPedOwner->DecrementFlareGunProjectileCount();
		}
	}
}

bool CProjectile::FadeOutProjectile()
{
	TUNE_GROUP_FLOAT(PROJECTILE_STICK_TO_PED_TUNE, FADE_TIME, 1.0f, 1.0f, 2500.0f, 1.0f);

	m_FadeTime += fwTimer::GetTimeStepInMilliseconds();

	if (m_FadeTime < FADE_TIME)
	{
		u32 alpha = static_cast<u8>(255.0f - ((((float)m_FadeTime / FADE_TIME)) * 255.0f));
		SetAlpha(alpha);
	}

	if (m_FadeTime >= FADE_TIME)
	{
		return true;
	}

	return false;
}

void CProjectile::TriggerExplosion(const u32 iRandomDelayMax /*= 0*/)
{
	if(!m_iFlags.IsFlagSet(PF_ExplosionTriggered) && !m_iFlags.IsFlagSet(PF_StuckToSpectatorPedOrGhostVeh))
	{
		physicsAssert(IsRecordingCollisionHistory());
		const CCollisionRecord * pMostSignificantRecord = GetFrameCollisionHistory()->GetMostSignificantCollisionRecord();

		Vector3 vDir = GetVelocity();
		if(vDir.Mag2() > 0.0f)
		{
			vDir.NormalizeFast();
		}
		else if(pMostSignificantRecord)
		{
			vDir = pMostSignificantRecord->m_MyCollisionNormal;
			physicsAssertf(vDir.Mag()>=0.997f && vDir.Mag()<=1.003f, "Normal: (%5.3f, %5.3f, %5.3f)", vDir.x, vDir.y, vDir.z);
		}
		else
		{
			vDir.Zero();
		}

		if (vDir.IsZero())
		{
			vDir = Vector3(0.0f, 0.0f, 1.0f);
		}

		const CEntity* pCollisionEntity = pMostSignificantRecord ? pMostSignificantRecord->m_pRegdCollisionEntity.Get() : NULL;

		if (!pCollisionEntity)
			pCollisionEntity = m_pHitEntity;

		if(pCollisionEntity != NULL)
		{
			// If owner entity is a vehicle, attribute explosion stat to ped with vehicle weapon equipped
			const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_uWeaponFiredFromHash);
			if (m_pOwner && m_pOwner->GetIsTypeVehicle() && pWeaponInfo && pWeaponInfo->GetIsVehicleWeapon())
			{
				const CVehicle* pVehicle = static_cast<CVehicle*>(m_pOwner.Get());
				const int iNumSeats = pVehicle->GetSeatManager()->GetMaxSeats();
				for (int iSeat = 0; iSeat < iNumSeats; ++iSeat)
				{
					if (pVehicle->GetSeatHasWeaponsOrTurrets(iSeat))
					{
						CPed* pPed = pVehicle->GetSeatManager()->GetPedInSeat(iSeat);
						if (pPed)
						{
							const CVehicleWeapon* pVehWeapon = pPed->GetWeaponManager()->GetEquippedVehicleWeapon();
							if (pVehWeapon && pVehWeapon->GetHash() == m_uWeaponFiredFromHash)
							{
								CStatsMgr::RegisterExplosionHitAnything(pPed, m_uWeaponFiredFromHash);
								break;
							}
						}
					}
				}		
			}
			else
			{
				CStatsMgr::RegisterExplosionHitAnything(m_pOwner, m_uWeaponFiredFromHash);
			}
		}

		m_iFlags.SetFlag(PF_ExplosionTriggered);

		// B*2232729: Handle staggered explosions in projectile code instead of passing through activation timer to explosion code.
		if (iRandomDelayMax != 0)
		{
			m_vDir = vDir;
			m_pCollisionEntity = pMostSignificantRecord ? pMostSignificantRecord->m_pRegdCollisionEntity.Get() : NULL;
			m_uExplodeTime = fwTimer::GetTimeInMilliseconds() + (iRandomDelayMax + fwRandom::GetRandomNumberInRange(0,CTaskAimAndThrowProjectile::GetTunables().m_iMaxRandomExplosionTime));
			m_iFlags.SetFlag(PF_UsingExplodeTimer);
		}
		else
		{
			Explode(VEC3V_TO_VECTOR3(GetTransform().GetPosition()), vDir, pCollisionEntity, pCollisionEntity ? true : false, iRandomDelayMax);	
		}
	}
	else if(NetworkInterface::IsGameInProgress() &&
		    m_networkIdentifier.IsValid() && 
			m_networkIdentifier.IsClone())
	{
		if (GetInfo()->GetHash() != AMMOTYPE_DLC_FLAREGUN)
		{
			Displayf("CProjectile::TriggerExplosion: Destroy(). Reason: GetInfo()->GetHash() != AMMOTYPE_DLC_FLAREGUN.");
			Destroy();
		}
	}
}

#if __BANK
void CProjectile::RenderDebug() const
{
#if DEBUG_DRAW
	// Render the axis of the projectile
	static bank_float AXIS_LENGTH = 0.2f;
	ScalarV AXIS_LENGTHV = ScalarVFromF32(AXIS_LENGTH);
	const Vec3V vThisPosition = GetTransform().GetPosition();
 	grcDebugDraw::Line(vThisPosition, vThisPosition + GetTransform().GetA() * AXIS_LENGTHV, Color_red,   Color_red);
 	grcDebugDraw::Line(vThisPosition, vThisPosition + GetTransform().GetB() * AXIS_LENGTHV, Color_green, Color_green);
 	grcDebugDraw::Line(vThisPosition, vThisPosition + GetTransform().GetC() * AXIS_LENGTHV, Color_blue,  Color_blue);

	if(GetCurrentPhysicsInst())
	{
		if(!fwTimer::IsGamePaused())
		{
			// Render the path of the projectile
			CWeaponDebug::ms_debugStore.AddLine(vThisPosition, m_matPrevious.GetCol3(), Color_green, 5000);
		}

		// Render WhizzBy events processing
		static dev_bool s_bDebugRenderWhizzByEvents = false;
		if( s_bDebugRenderWhizzByEvents && m_bWhizzByEventCheckDebugDrawPending )
		{
			grcDebugDraw::Capsule(VECTOR3_TO_VEC3V(m_vLastWhizzByDebugHeadPosition), VECTOR3_TO_VEC3V(m_vLastWhizzByDebugTailPosition), s_fWhizzByEventRadius, Color_yellow, false, 30);
		}
	}
#endif // DEBUG_DRAW
}
#endif // __BANK

bool CProjectile::CalculateTimeUntilExplosion(float& fTime) const
{
	//Set the time.
	fTime = FLT_MAX;

	//Check if we are active.
	if(m_iFlags.IsFlagSet(PF_Active))
	{
		//Check if we are using a life timer.
		if(m_iFlags.IsFlagSet(PF_UsingLifeTimer))
		{
			//Check if the explosion has not triggered.
			if(!m_iFlags.IsFlagSet(PF_ExplosionTriggered))
			{
				//Set the time.
				fTime = m_fExplosionTime;

				return true;
			}
		}
	}

	return false;
}

void CProjectile::Explode(const Vector3& vPosition, const Vector3& vNormal, const CEntity* pHitEntity, bool bHasCollided, const u32 iRandomDelayMax)
{
	// Projectile should be destroyed so don't explode. This can happen if owner vehicle was removed in this frame
	if(CheckOwner())
	{
		return;
	}

#if __BANK
	weaponDebugf1("CProjectile::Explode() triggered for projectile: %d:%d of owner: %s", m_networkIdentifier.GetPlayerOwner(), m_networkIdentifier.GetFXId(), m_pOwner?m_pOwner->GetLogName():"No owner");

	TUNE_GROUP_BOOL(PROJECTILES, bTraceExplodeCalls, false);
	if (bTraceExplodeCalls)
	{
		Displayf("CProjectile::Explode(): vPosition - <%.2f,%.2f,%.2f>, pHitEntity - %s[%p], bHasCollided - %s, iRandomDelayMax - %i",
			vPosition.GetX(), vPosition.GetY(), vPosition.GetZ(),
			pHitEntity ? (pHitEntity->GetIsDynamic() ? AILogging::GetDynamicEntityNameSafe(static_cast<const CDynamicEntity*>(pHitEntity)) : pHitEntity->GetModelName()) : "null",
			pHitEntity,
			AILogging::GetBooleanAsString(bHasCollided),
			iRandomDelayMax);

		sysStack::PrintStackTrace();
	}
#endif

	// Ensure that we generate whizz by events for the last bit of distance travelled before impact.
	const bool bForceUpdate = true;
	ProcessWhizzByEvents(bForceUpdate);

	bool bDetonatingOtherPlayersExplosive = false;
	RestoreDamping();

	// Clone projectiles do not explode (the machine which generated the projectile will also generate the explosion)
	if(NetworkInterface::IsGameInProgress() && m_networkIdentifier.IsClone())
	{
		bDetonatingOtherPlayersExplosive = GetInfo() && GetInfo()->GetIsSticky(); // trying to detonate another player sticky bomb

		if( pHitEntity && GetAsProjectileRocket() )
		{
			if( pHitEntity->GetIsTypePed() )
			{
				const CPed* pPed = static_cast<const CPed*>(pHitEntity);
				if (pPed->IsLocalPlayer())
				{
					bDetonatingOtherPlayersExplosive = true; //a rocket has hit us so make sure we take control of the explosion
				}
			}
			else if( pHitEntity->GetIsTypeVehicle() )
			{
				const CVehicle* pVehicle = static_cast< const CVehicle* >( pHitEntity );

				if( !pVehicle->IsNetworkClone() && pVehicle->ContainsLocalPlayer() && !pVehicle->InheritsFromBoat())
				{
					bDetonatingOtherPlayersExplosive = true;
				}
			}
		}

		if(!bDetonatingOtherPlayersExplosive)
		{
			if(GetAsProjectileRocket()!=NULL)
			{
				Displayf("CProjectile::Explode: Destroy(). Reason: !bDetonatingOtherPlayersExplosive && GetAsProjectileRocket()!=NULL.");
				Destroy();
			}

			return;
		}
	}

	if(!m_iFlags.IsFlagSet(PF_Exploded))
	{
		eExplosionTag tag = GetInfo()->GetExplosionTag(pHitEntity);

		// Override explosion tag and explosion normal if the projectile has been exploded by air defences.
		Vector3 vModifiedNormal = vNormal;
		if (m_bUseAirDefenceExplosion)
		{
			tag = EXP_TAG_AIR_DEFENCE;
			vModifiedNormal = m_vAirDefenceFireDirection;
		}

		if(tag != EXP_TAG_DONTCARE)
		{
			if(m_damageType != DAMAGE_TYPE_FIRE || !GetIsInWater())
			{
				if (tag==EXP_TAG_TORPEDO && GetIsInWater())
				{
					tag = EXP_TAG_TORPEDO_UNDERWATER;
				}

				bool bIsExplodingInAir = GetIsExplodingInAir(bHasCollided, pHitEntity);

				CExplosionManager::CExplosionArgs explosionArgs(tag, vPosition);

				explosionArgs.m_pEntExplosionOwner = m_pOwner;
				explosionArgs.m_pEntIgnoreDamage = m_pIgnoreDamageEntity;
				explosionArgs.m_bInAir = bIsExplodingInAir;
				explosionArgs.m_vDirection = vModifiedNormal;
				explosionArgs.m_originalExplosionTag = GetInfo()->GetExplosionTag();
				explosionArgs.m_weaponHash = m_uWeaponFiredFromHash;
				explosionArgs.m_bDetonatingOtherPlayersExplosive = bDetonatingOtherPlayersExplosive;
				explosionArgs.m_interiorLocation = GetInteriorLocation();

				if(iRandomDelayMax != 0)
				{
					explosionArgs.m_activationDelay = iRandomDelayMax + fwRandom::GetRandomNumberInRange(0,CTaskAimAndThrowProjectile::GetTunables().m_iMaxRandomExplosionTime);
				}

				// Check to see if the projectile was attached within the rear door space of the vehicle
				CEntity* pAttachedEntity = (CEntity*)GetAttachParentForced();
				if( pAttachedEntity )
				{
					explosionArgs.m_pAttachEntity = (pAttachedEntity && pAttachedEntity->GetDrawable()) ? pAttachedEntity : NULL;
					if( pAttachedEntity->GetIsTypeVehicle() )
					{
						if(NetworkInterface::IsGameInProgress())
						{	//Don't use a delay when syncing explosion attached to 
							//vehicle as this requires immediate syncing to ensure
							//coordination with any other syncing of vehicle 
							//parts like doors opening etc.
							explosionArgs.m_activationDelay = 0;
						}

						CVehicle* pVehicle = (CVehicle*)pAttachedEntity;
						explosionArgs.m_bAttachedToVehicle = true;

						// Check for rear opening doors
						if( pVehicle->GetStatus() != STATUS_WRECKED )
							pVehicle->TestExplosionPosAndBlowRearDoorsopen( vPosition );
					}
				}
				else
					explosionArgs.m_pAttachEntity = (pHitEntity && pHitEntity->GetDrawable()) ? const_cast<CEntity *>(pHitEntity) : NULL;

				if(	NetworkInterface::IsGameInProgress()	&&
					explosionArgs.m_pAttachEntity			&&
					NetworkUtils::GetObjectIDFromGameObject(explosionArgs.m_pAttachEntity.Get())==NETWORK_INVALID_OBJECT_ID )
				{
					explosionArgs.m_pAttachEntity = NULL;
				}

				//B*1752582: Smoke grenade fix
				//If projectile isn't flagged to be fixed after explosion and not sticky, and hit entity is dynamic, then attach the explosion to the projectile
				//Or if hit entity is null, attach explosion to the projectile
				//bool bAttachToProjecitle = GetInfo()->GetFixedAfterExplosion() && !GetInfo()->GetIsSticky() && pHitEntity && pHitEntity->GetIsDynamic();
				bool bAttachToProjectile = GetInfo()->GetFixedAfterExplosion() && !GetInfo()->GetIsSticky() && explosionArgs.m_pAttachEntity && explosionArgs.m_pAttachEntity->GetIsDynamic();
				if ((bAttachToProjectile || !explosionArgs.m_pAttachEntity) && tag == EXP_TAG_SMOKEGRENADE)
				{
					explosionArgs.m_pAttachEntity = static_cast<CEntity*>(this);
				}

				if( CExplosionManager::AddExplosion(explosionArgs, this, m_iFlags.IsFlagSet(PF_ForceExplosion)))
				{
					// Exploded
					m_iFlags.SetFlag(PF_Exploded);
					m_iFlags.ClearFlag(PF_ExplosionTriggered);

					//B*1752582: Smoke grenade fix
					if(!bAttachToProjectile)
					{
						//Set fixed physics if we have a valid hit entity
						if (pHitEntity)
						{
							CObject::SetFixedPhysics(true);	
						}
						//Stick to entity if we have valid attachment entity (which isn't ourselves)
						if(explosionArgs.m_pAttachEntity && explosionArgs.m_pAttachEntity != static_cast<CEntity*>(this))
						{
							StickToEntity(explosionArgs.m_pAttachEntity, GetTransform().GetPosition(), GetTransform().GetUp(), 0, 0);
						}
					}

					if(GetInfo()->GetAddSmokeOnExplosion())
					{
						CPortalTracker* pPortalTracker = GetPortalTracker();
						if (pPortalTracker)
						{
							CInteriorInst* pIntInst = pPortalTracker->GetInteriorInst();
							if (pIntInst && pPortalTracker->m_roomIdx>0)
							{
								// we're in a room inside an interior
								pIntInst->AddSmokeToRoom(pPortalTracker->m_roomIdx, VEC3V_TO_VECTOR3(GetTransform().GetPosition()), true);
							}
						}
					}

					// Additional radius explosions of a different type
					if (GetInfo()->GetIsCluster())
					{	
						TUNE_GROUP_BOOL(CLUSTER_BOMBS, bRenderDebug, false);

						explosionArgs.m_activationDelay += (u32)(GetInfo()->GetClusterInitialDelay() * 1000);
						explosionArgs.m_explosionTag = GetInfo()->GetClusterExplosionTag();

						Vector3 newExplosionPos;
						float minRadius = GetInfo()->GetClusterMinRadius();
						float maxRadius = GetInfo()->GetClusterMaxRadius();		
						int uExplosionCount = (int)(GetInfo()->GetClusterExplosionCount());
						SemiShuffledSequence shuffledSegment(uExplosionCount);
#if __BANK
						if (bRenderDebug)
						{
							grcDebugDraw::Circle(vPosition, minRadius, Color_magenta, Vector3(1.f,0.f,0.f), Vector3(0.f,1.f,0.f), false, false, 100);
							grcDebugDraw::Circle(vPosition, maxRadius, Color_magenta, Vector3(1.f,0.f,0.f), Vector3(0.f,1.f,0.f), false, false, 100);

							for (int i = 0; i < uExplosionCount; i++)
							{
								float fDebugAngle = i * (TWO_PI / (float)uExplosionCount);
								Vector3 vDebugStart(vPosition.x + (minRadius * rage::Sinf(fDebugAngle)), vPosition.y + (minRadius * rage::Cosf(fDebugAngle)), vPosition.z);
								Vector3 vDebugEnd(vPosition.x + (maxRadius * rage::Sinf(fDebugAngle)), vPosition.y + (maxRadius * rage::Cosf(fDebugAngle)), vPosition.z);
								grcDebugDraw::Line(vDebugStart, vDebugEnd, Color_magenta, Color_magenta, 100);
							}
						}
#endif // __BANK
						for (int i = 0; i < uExplosionCount; i++)
						{
							// Pick a random shuffled segment so that secondary explosions always go in different directions
							int iSegment = shuffledSegment.GetElement(i);
							float fSegmentAngle = TWO_PI / (float)uExplosionCount;

							// Try several times, as the explosion may be rejected (most common reason being the random position was too close to an existing explosion)
							const int randomAttempts = 8;
							for (int j = 0; j < randomAttempts; j++)
							{
								// Random angle from our segment shuffler
								float angle = fwRandom::GetRandomNumberInRange(fSegmentAngle * iSegment, fSegmentAngle * (iSegment+1));

								// Evenly distributed random radius within a ring
								float radius = 2 / (maxRadius * maxRadius - minRadius * minRadius);
								radius = Sqrtf(2 * fwRandom::GetRandomNumberInRange(0.0f, 1.0f) / radius + minRadius * minRadius);

								// Offset position and adjust for ground height
								newExplosionPos.x = vPosition.x + (radius * rage::Sinf(angle));
								newExplosionPos.y = vPosition.y + (radius * rage::Cosf(angle));
								newExplosionPos.z = WorldProbe::FindGroundZFor3DCoord(TOP_SURFACE, newExplosionPos.x, newExplosionPos.y, vPosition.z + 3.0f);
								explosionArgs.m_explosionPosition = newExplosionPos;

								// Attempt to create explosion at new position
								if (CExplosionManager::AddExplosion(explosionArgs, this, m_iFlags.IsFlagSet(PF_ForceExplosion)))
								{
#if __BANK
									if (bRenderDebug)
									{
										grcDebugDraw::Sphere(newExplosionPos, 0.125f, Color_green, true, 100);
										grcDebugDraw::Line(newExplosionPos, vPosition, Color_green, Color_green, 100);
									}
#endif // __BANK
									explosionArgs.m_activationDelay += (u32)(GetInfo()->GetClusterInbetweenDelay() * 1000);								
									break;
								}
							}
						}				
					}

					// Specify when we should destroy the ourselves
					m_fLifeTimeAfterExplosion = GetInfo()->GetLifeTimeAfterExplosion();

					//B*1815582: Increase smoke grenade duration in MP
					if (tag == EXP_TAG_SMOKEGRENADE && NetworkInterface::IsGameInProgress())
					{
						m_fLifeTimeAfterExplosion *= 2.0f;
					}

					//Scale explosion life time by weapon effect modifier
					if (auto pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_uWeaponFiredFromHash))
					{
						m_fLifeTimeAfterExplosion *= pWeaponInfo->GetEffectDurationModifier();
					}
					
					if (m_fLifeTimeAfterExplosion <= 0.0f )
					{
						if (m_pInfo->GetIsProximityRepeatedDetonation() && (!m_iFlags.IsFlagSet(PF_UsingLifeTimer) || m_fExplosionTime > 0.0f))
						{
							m_iFlags.ClearFlag(PF_Exploded);
							m_bProximityMineActive = false;
							m_bProximityMineTriggered = false;
							m_bProximityMineTriggeredByVehicle = false;
							m_bProximityMineRepeatingDetonation = true;
							m_fProximityMineStuckTime = 0;
						} 
						else if(explosionArgs.m_activationDelay > 0)
						{
							m_iFlags.SetFlag(PF_UsingDestroyTimer);
							m_uDestroyTime = fwTimer::GetTimeInMilliseconds() + explosionArgs.m_activationDelay + 1;
						}
						else
						{
							Displayf("CProjectile::Explode: Destroy(). Reason: explosionArgs.m_activationDelay <= 0.");
							Destroy();
						}
					}
				}
				else
				{
					//Make sure the projectile explodes next frame
					m_iFlags.SetFlag(PF_ExplodeNextFrame);

					m_iFlags.ClearFlag(PF_ExplosionTriggered);
				}
			}

#if __BANK
			CProjectileManager::SetLastExplosionPoint(vPosition, vNormal);
#endif // __BANK
		}
	}
}

void CProjectile::ProcessHomingAttractor()
{
	TUNE_GROUP_FLOAT(HOMING_ATTRACTOR, ATTRACTOR_MIN_DIST_TO_TARGET, 50.0f, 0.01f, 500.0f, 0.01f);
	TUNE_GROUP_FLOAT(HOMING_ATTRACTOR, ATTRACTOR_MAX_DIST_FROM_ATTRACTOR, 100.0f, 0.01f, 500.0f, 0.01f);
	TUNE_GROUP_INT(HOMING_ATTRACTOR, iTimeBeweenProcessingMS, 150, 0, 5000, 100);
	TUNE_GROUP_INT(HOMING_ATTRACTOR, iTimeToProcessAfterBeingFiredMS, 1000, 0, 30000, 500);

	// Allow flare projectiles to attract homing projectiles
	const CAmmoProjectileInfo* pAmmoInfo = GetInfo();
	if (pAmmoInfo && pAmmoInfo->GetIsHomingAttractor())
	{
		CPed* pOwnerPed = NULL;
		if(m_pOwner && m_pOwner->GetIsTypePed())
		{
			pOwnerPed = static_cast<CPed*>(m_pOwner.Get());
		}

		CVehicle* pOwningVehicle = NULL;
		if(m_pOwner && m_pOwner->GetIsTypeVehicle())
		{
			pOwningVehicle = static_cast<CVehicle*>(m_pOwner.Get());
		}

		if ( (pOwnerPed && !pOwnerPed->IsNetworkClone()) || (pOwningVehicle && !pOwningVehicle->IsNetworkClone()) )
		{
			if (m_iTimeProjectileWasFired == 0)
				return;

			// Stop redirecting missiles after certain amount of time from being fired;
			u32 uTimeSinceFired = CTimeHelpers::GetTimeSince(m_iTimeProjectileWasFired);
			if (uTimeSinceFired >= iTimeToProcessAfterBeingFiredMS)
				return;

			u32 uTimeSinceLastUpdate = CTimeHelpers::GetTimeSince(m_iLastTimeProcessedHomingAttractor);
			if (m_iLastTimeProcessedHomingAttractor == 0 || uTimeSinceLastUpdate >= iTimeBeweenProcessingMS)
			{
				m_iLastTimeProcessedHomingAttractor = fwTimer::GetTimeInMilliseconds();

				const CEntity* pOwningEntity = m_pOwner.Get();
				if (pOwningEntity)
				{
					// Get a list of projectile rockets targeting us within a range, and switch the targets
					atArray<CProjectileRocket*> projectileArray;
					CProjectileManager::GetProjectilesToRedirect(pOwningEntity, GetTransform().GetPosition(), projectileArray, ATTRACTOR_MIN_DIST_TO_TARGET, ATTRACTOR_MAX_DIST_FROM_ATTRACTOR);

					for (int i = 0; i < projectileArray.GetCount(); i++)
					{
						projectileArray[i]->SetTarget(this);
						projectileArray[i]->SetIsRedirected(true);

						if(NetworkInterface::IsGameInProgress())
						{
							CUpdateProjectileTargetEntity::Trigger(projectileArray[i], GetTaskSequenceId());
						}
					}
				}
			}
		}
	}
}

void CProjectile::ProcessInWater(const bool bUseExplosionPos)
{
	// Cache the position
	Vector3 vPos = VEC3V_TO_VECTOR3(GetTransform().GetPosition());
	
	//Use the explosion position if requested
	if(bUseExplosionPos)
	{
		vPos = m_vExplodePos;
	}

	// Test for water splashes
	if(m_Buoyancy.GetWaterLevelIncludingRivers(vPos, &m_fWaterLevel, false, POOL_DEPTH, REJECTIONABOVEWATER, NULL, this))
	{
		if(m_fWaterLevel > vPos.z)
		{
			// The projectile is underwater 
			SetIsInWater( true );

			// B*2153566: Clamp the projectile explosion time to 3.0 seconds once we've hit water (unless we're a torpedo).
			if(!GetInfo()->GetShouldThrustUnderwater())
			{
				m_fExplosionTime = rage::Clamp(m_fExplosionTime, 0.0f, 3.0f);
			}

			// Check if the projectile has just gone underwater
			if(!m_nPhysicalFlags.bWasInWater)
			{
				m_iFlags.SetFlag(PRF_TriggerSplash);

				// Set timer and flag for any projectiles of DAMAGE_TYPE_FIRE/DAMAGE_TYPE_SMOKE if they are underwater to be destroyed.
				if(m_damageType == DAMAGE_TYPE_FIRE || m_damageType == DAMAGE_TYPE_SMOKE)
				{
					// Make sure they won't explode.
					m_iFlags.ClearFlag(PF_Active);

					if(!m_iFlags.IsFlagSet(PF_UsingDestroyTimer))
					{
						m_iFlags.SetFlag(PF_UsingDestroyTimer); 
						m_uDestroyTime = fwTimer::GetTimeInMilliseconds() + 2000;
					}
				}
			}
		}
		else
		{
			// The projectile is not underwater 
			SetIsInWater( false );

			// Check if the projectile has just come out of water
			if(m_nPhysicalFlags.bWasInWater)
			{
				m_iFlags.SetFlag(PRF_TriggerSplash);
			}
		}
	}

	// make the trail inactive if in water
	if(GetIsInWater() && GetInfo()->GetTrailFxInactiveOnceWet())
	{
		m_iFlags.SetFlag(PF_TrailInactive);
	}

	// Determine whether or not we should toggle the life time flag on due to hitting water.
	const CAmmoProjectileInfo* pAmmoInfo = GetInfo();
	if(GetIsInWater() && !pAmmoInfo->GetDelayUntilSettled() && pAmmoInfo->GetShouldProcessImpacts())
	{
		if(!m_iFlags.IsFlagSet(PF_UsingLifeTimeAfterImpact) && GetLifeTimeAfterImpact() > 0.0f)
		{
			m_iFlags.SetFlag(PF_UsingLifeTimeAfterImpact);
		}
	}
}

void CProjectile::ProcessEffects()
{
	CBaseModelInfo* pBaseModelInfo = GetBaseModelInfo();
	if (weaponVerifyf(pBaseModelInfo && pBaseModelInfo->GetHasLoaded(), "trying to update vfx on a projectile (%s) whose base model info isn't loaded - any vfx dependencies may also not be loaded. Active: %s; Removed from world: %s; Visible: %s; Location: %f, %f, %f", 
		pBaseModelInfo->GetModelName(), m_iFlags.IsFlagSet(PF_Active) ? "T" : "F", IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD) ? "T" : "F", IsBaseFlagSet( fwEntity::IS_VISIBLE ) ? "T" : "F", 
		GetTransform().GetPosition().GetXf(), GetTransform().GetPosition().GetYf(), GetTransform().GetPosition().GetZf()))
	{
		if(m_iFlags.IsFlagSet(PF_Active))
		{
			// calc the trail evolution value
			m_trailEvo = 1.0f;
			if(m_iFlags.IsFlagSet(PF_UsingLifeTimer))
			{
				if(m_fLifeTime < GetInfo()->GetTrailFxFadeOutTime())
				{
					m_trailEvo = CVfxHelper::GetInterpValue(m_fLifeTime, 0.0f, GetInfo()->GetTrailFxFadeOutTime());
				}
				else if(m_fLifeTime > GetInfo()->GetLifeTime() - GetInfo()->GetTrailFxFadeInTime())
				{
					m_trailEvo = CVfxHelper::GetInterpValue(m_fLifeTime, GetInfo()->GetLifeTime(), GetInfo()->GetLifeTime() - GetInfo()->GetTrailFxFadeInTime());
				}
			}

			// do any trail effect
			if(!m_iFlags.IsFlagSet(PF_TrailInactive) && m_trailEvo>0.0f)
			{
				g_vfxProjectile.UpdatePtFxProjTrail(this, m_trailEvo, m_weaponTintIndex);
			}

			// probe downward looking for ground to play disturbance effects
			if (GetInfo()->GetDoesGroundDisturbanceFx())
			{
				Vector3 startPos = VEC3V_TO_VECTOR3(this->GetTransform().GetPosition());
				WorldProbe::CShapeTestFixedResults<> probeResult;
				WorldProbe::CShapeTestProbeDesc probeDesc;
				probeDesc.SetStartAndEnd(startPos, startPos-Vector3(0.0f, 0.0f, GetInfo()->GetDisturbFxProbeDist()));
				probeDesc.SetResultsStructure(&probeResult);
				probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);

				if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
				{
					// Play any weapon impact effects.
					g_vfxProjectile.UpdatePtFxProjGround(this, probeResult[0]);
				}
			}
			
			// Do we need a glow ?
			if( !s_grenadeAmbientModMPOnly || NetworkInterface::IsGameInProgress() )
			{
				const eExplosionTag expTag = GetInfo()->GetExplosionTag();
				if( expTag == EXP_TAG_GRENADE)
				{
					SetUseLightOverride(true);
				}
			}

			g_vfxProjectile.ProcessPtFxProjProximity(this, m_bProximityMineActive);
		}
		else
		{
			// do any fuse effect
			if(!m_iFlags.IsFlagSet(PF_TrailInactive))
			{
				g_vfxProjectile.ProcessPtFxProjFuse(this, false);

				if(m_iFlags.IsFlagSet(PF_Priming))
				{
					if(!GetIsInWater())
					{
						g_vfxProjectile.ProcessPtFxProjFuse(this, true);
					}
				}
			}	
		}

		// do any splash effects - going in and out of the water surface
		// this is not in any active/inactive section on purpose 
		// - some projectiles (e.g. molotovs) get made inactive when they hit water but we still want the splash to happen
		if(m_iFlags.IsFlagSet(PRF_TriggerSplash))
		{
			Vector3 vSplashPos(VEC3V_TO_VECTOR3(GetTransform().GetPosition()));
			vSplashPos.z = m_fWaterLevel;

			// Set up the effects info structure
			VfxCollInfo_s vfxCollInfo;
			vfxCollInfo.regdEnt = NULL;
			vfxCollInfo.vPositionWld = RCC_VEC3V(vSplashPos);
			vfxCollInfo.vNormalWld = RCC_VEC3V(ZAXIS);
			vfxCollInfo.vDirectionWld = RCC_VEC3V(ZAXIS);
			vfxCollInfo.materialId = 0;
			vfxCollInfo.roomId = 0;
			vfxCollInfo.componentId = 0;
			vfxCollInfo.weaponGroup = m_effectGroup;
			vfxCollInfo.force = VEHICLEGLASSFORCE_NOT_USED;
			vfxCollInfo.isBloody = false;
			vfxCollInfo.isWater = true;
			vfxCollInfo.isExitFx = false;
			vfxCollInfo.noPtFx = false;
			vfxCollInfo.noPedDamage = false;
			vfxCollInfo.noDecal = false;
			vfxCollInfo.isSnowball = false;
			vfxCollInfo.isFMJAmmo = false;

			// Play any weapon impact effects
			g_vfxWeapon.DoWeaponImpactVfx(vfxCollInfo, m_damageType, m_pOwner);
		}
	}
}

void CProjectile::ProcessAudio()
{
	if(m_iFlags.IsFlagSet(PRF_TriggerSplash))
	{
		Vector3 vSplashPos(VEC3V_TO_VECTOR3(GetTransform().GetPosition()));
		vSplashPos.z = m_fWaterLevel;

		audSoundInitParams initParams;
		initParams.Position = vSplashPos;
		g_WeaponAudioEntity.CreateAndPlaySound(g_WeaponAudioEntity.FindSound(ATSTRINGHASH("ProjectileSplash", 0x7B7C6F7F)), &initParams);
	}

	if(m_iFlags.IsFlagSet(PRF_TriggerBeep))
	{		
		audSoundInitParams initParams;
		initParams.Position = VEC3V_TO_VECTOR3(GetTransform().GetPosition());
		
		if (m_iFlags.IsFlagSet(PF_UsingLifeTimer) && m_pInfo->GetLightSpeedsUp())
		{
			TUNE_GROUP_FLOAT(EXPLOSIVE_TUNE, PITCH_START_PHASE, 0.5f, 0.0f, 1.0f, 0.01f);
			TUNE_GROUP_FLOAT(EXPLOSIVE_TUNE, PITCH_END_MULT, 1.2f, 1.0f, 5.0f, 0.01f);
			float fPhase = 1 - (m_fLifeTime / m_pInfo->GetLifeTime());
			if (fPhase > PITCH_START_PHASE)
			{
				float fDelta = (fPhase - PITCH_START_PHASE) / (1 - PITCH_START_PHASE);
				float fMult = 1.0f + ((PITCH_END_MULT - 1.0f) * fDelta);
				initParams.Pitch = (s16)((fMult - 1) * 1200);
			}
		}


		if(m_pInfo->GetIsProximityDetonation())
		{
			audSoundSet proxSet;
			u32 proxSetName = GetMineSoundsetHash(GetWeaponFiredFromHash());
			proxSet.Init(proxSetName);
			if(proxSet.IsInitialised()  && !m_bProximityMineTriggered)
			{
				g_WeaponAudioEntity.CreateAndPlaySound(proxSet.Find(ATSTRINGHASH("ambient", 0xC06BF8AB)), &initParams);
			}
		}
		else
		{
			g_WeaponAudioEntity.CreateAndPlaySound(g_WeaponAudioEntity.FindSound(ATSTRINGHASH("sticky_bomb_beep", 0xB66B909E)), &initParams);
		}

		// Have to manually reset the flag as it gets set in PostPreRender, before the reset flags are automatically cleared
		m_iFlags.ClearFlag(PRF_TriggerBeep);
	}
}

u32 CProjectile::GetMineSoundsetHash(const u32 weaponNameHash)
{
	u32 soundsetHash = 0u;

	// From weapon_enums.sch
	switch (weaponNameHash)
	{
	case 0x59EAE9A4: // VEHICLE_WEAPON_MINE
		soundsetHash = ATSTRINGHASH("gr_vehicle_proxmine_sounds", 0xBCA4F642);
		break;
	case 0x3C09584E: // VEHICLE_WEAPON_MINE_KINETIC
	case 0x69E10D60: // VEHICLE_WEAPON_MINE_EMP
	case 0xD96DA06C: // VEHICLE_WEAPON_MINE_SPIKE
	case 0x56FACAC7: // VEHICLE_WEAPON_MINE_SLICK
	case 0xF4418BA0: // VEHICLE_WEAPON_MINE_TAR
		soundsetHash = ATSTRINGHASH("DLC_AW_Vehicle_Mine_Sounds", 0x291A73D2);
		break;
	default:
		soundsetHash = ATSTRINGHASH("SPL_PROXMINE_SOUNDS", 0xAD9FD3AA);
		break;
	}

	return soundsetHash;
}

bool CProjectile::ShouldStickToEntity( const CEntity* pHitEntity, const CEntity* pOwner, const float fProjectileWidth, Vec3V_In vStickPos, s32 iStickComponent, phMaterialMgr::Id iStickMaterialId, bool bIgnoreWheelSphereExclusion, bool bShouldStickToPeds, bool bIsHitEntityFriendly )
{
	if( PGTAMATERIALMGR->UnpackMtlId( iStickMaterialId ) == PGTAMATERIALMGR->g_idCarVoid )
		return false;

	spdSphere projectileSphere( vStickPos, ScalarV( fProjectileWidth ) );

	Assert( pHitEntity );

	//B*1837611: MP Only: Sticky bombs attach to non-friendly peds backs
	bool bShouldStickToPedsBack = false;
	if (pHitEntity->GetIsTypePed() && !bShouldStickToPeds && !bIsHitEntityFriendly && NetworkInterface::IsGameInProgress())
	{
		const CPed *pHitPed = static_cast<const CPed*>(pHitEntity);

		// In vehicle: only stick to peds on bike/quadbike/jetski.
		bool bInValidVehicle = true;
		if (pHitPed->GetIsInVehicle())
		{
			const CVehicle *pVehicle = static_cast<CVehicle*>(pHitPed->GetVehiclePedInside());
			if (pVehicle)
			{
				bInValidVehicle = pVehicle->InheritsFromBike() || pVehicle->InheritsFromQuadBike() || pVehicle->GetIsJetSki() || pVehicle->InheritsFromAmphibiousAutomobile();
			}
		}

		if (pHitPed && !pHitPed->IsDead() && bInValidVehicle)
		{
			s32 iHitBoneTag = pHitPed->GetBoneTagFromRagdollComponent(iStickComponent);

			if (iHitBoneTag == BONETAG_SPINE0 || iHitBoneTag == BONETAG_SPINE1 || iHitBoneTag == BONETAG_SPINE2 || iHitBoneTag == BONETAG_SPINE3)
			{
				// Ensure that the bomb is behind the plane along the peds back (pelvis->head) before sticking.
				Vector3 vPelvisPosition = VEC3_ZERO;
				Vector3 vHeadPosition = VEC3_ZERO;
				pHitPed->GetBonePosition(vPelvisPosition, BONETAG_PELVIS);
				pHitPed->GetBonePosition(vHeadPosition, BONETAG_HEAD);
				Vector3 vPlane = vHeadPosition - vPelvisPosition;

				Vector3 vNormal = VEC3_ZERO;
				Vector3 vPedRight = VEC3V_TO_VECTOR3(pHitPed->GetTransform().GetA());
				vNormal.Cross(vPlane, vPedRight);
				vNormal.Normalize();

				Vector3 vStickPosVector3 = VEC3V_TO_VECTOR3(vStickPos);
				Vector3 vHalfPlane = vPlane;
				vHalfPlane.Scale(0.5f);
				Vector3 vPointOnPlane = vPelvisPosition + vHalfPlane;
				Vector3 vPlaneToBomb = vStickPosVector3 - vPointOnPlane;
				float fDot = vPlaneToBomb.Dot(vNormal);

				bShouldStickToPedsBack = (fDot < 0.0f);
			}
		}
	}

	if(pOwner && NetworkInterface::IsDamageDisabledInMP(*pHitEntity, *pOwner))
	{
		return false;
	}

	if( pHitEntity->GetIsTypePed() && !bShouldStickToPeds && !bShouldStickToPedsBack)
	{
		return false;
	}
	else if( pHitEntity->GetIsTypeObject() )
	{
		const CObject* pObject = static_cast<const CObject*>(pHitEntity);

		// No props
		if( pObject->m_nObjectFlags.bAmbientProp || pObject->m_nObjectFlags.bPedProp )
			return false;

		// No pickups
		if( pObject->m_nObjectFlags.bIsPickUp )
			return false;

		// No invisible cardboard boxes attached to vehicles. B*1638542...
		if( IsEntityInvisibleCardboardBoxAttachedToVehicle(*pObject) )
			return false;
	}
	else if( pHitEntity->GetIsTypeVehicle() )
	{
		// Do not allow sticks to bikes or bicycles
		const CVehicle* pHitVehicle = static_cast<const CVehicle*>( pHitEntity );
		if( pHitVehicle->GetVehicleType() == VEHICLE_TYPE_BICYCLE )
			return false;

		// Walk through each wheel and determine if we are inside the wheel bounds
		// Analyze stick position against each wheel as we do not want sticky bombs on wheels
		const CWheel* pWheel = NULL;
		int nNumWheels = pHitVehicle->GetNumWheels();
		for( int i = 0; i < nNumWheels; i++ )
		{
			// If we strike a wheel component we need to return immediately as we do not allow it
			pWheel = pHitVehicle->GetWheel(i);
			for( int j = 0; j < MAX_WHEEL_BOUNDS_PER_WHEEL; j++ )
			{
				if( pWheel->GetFragChild(j) == iStickComponent )
				{
					return false;
				}
			}

			if( !bIgnoreWheelSphereExclusion )
			{
				// There are cases where the wheel in clipping with the chassis and therefore we need to run this check as well
				Vector3 vWheelPos;
				float fWheelRadius = pWheel->GetWheelPosAndRadius( vWheelPos );
				spdSphere wheelSphere( VECTOR3_TO_VEC3V( vWheelPos ), ScalarV( fWheelRadius ) );
				if( projectileSphere.IntersectsSphere( wheelSphere ) )
				{
					return false;
				}
			}
		}
	}

	if ( !bShouldStickToPeds )
	{
		// Do not stick if this is something that is attached to a ped
		if( pHitEntity->GetIsPhysical() )
		{
			const CPhysical* pHitPhysical = static_cast<const CPhysical*>(pHitEntity);
			const CEntity* pAttachedEntity = pHitPhysical->GetIsAttached() ? static_cast<const CEntity*>(pHitPhysical->GetAttachParent()) : NULL;
			if( pAttachedEntity && pAttachedEntity->GetIsTypePed() )
				return false;

			fragInst* pFragInst = pHitPhysical->GetFragInst();
			if( pFragInst && pFragInst->GetCached() )
			{
				// The component we hit has been broken, so don't try to attach as the matrix will be zeroed out
				if( pFragInst->GetCacheEntry()->GetHierInst() && pFragInst->GetChildBroken( iStickComponent ) )
				{
					return false;
				}

				phBoundComposite* pBoundComposite = static_cast<phBoundComposite*>( pFragInst->GetArchetype()->GetBound() );
				if( pBoundComposite )
				{
					if( pHitPhysical->IsHiddenFlagSet( iStickComponent ) )
					{
						//Detect how much has broken and when we overlap, detach from my parent entity
						spdSphere smashSphere( CVehicleGlassManager::GetVehicleGlassComponentSmashSphere( pHitPhysical, iStickComponent, false ) );
						if( projectileSphere.IntersectsSphere( smashSphere ) )
						{
							return false;		
						}
					}
				}
			}
		}
	}

	return true;
}

void CProjectile::StickToEntity( CEntity* pStickEntity, Vec3V_In vStickPosition, Vec3V_In vStickNormal, s32 iStickComponent, phMaterialMgr::Id iStickMaterialId)
{
	Assert( pStickEntity );
	Assert( !IsZeroAll( vStickPosition ) );
	Assert( !IsZeroAll( vStickNormal ) );
	Assert( iStickComponent > -1 );

	m_pStickEntity = pStickEntity;
	m_vStickPos = VEC3V_TO_VECTOR3( vStickPosition );
	m_vStickNormal = VEC3V_TO_VECTOR3( vStickNormal );
	m_iStickComponent = iStickComponent;
	m_iStickMaterialId = static_cast<phMaterialMgr::Id>(iStickMaterialId);

#if __BANK
	if (pStickEntity)
	{
		weaponDebugf3("[projectile] StickToEntity(): m_pStickEntity - %s[%p], m_vStickPos - <%.2f,%.2f,%.2f>, m_vStickNormal - <%.2f,%.2f,%.2f>, m_iStickComponent - %i", pStickEntity->GetIsDynamic() ? AILogging::GetDynamicEntityNameSafe(static_cast<CDynamicEntity*>(pStickEntity)) : pStickEntity->GetModelName(), pStickEntity,
			m_vStickPos.GetX(),m_vStickPos.GetY(),m_vStickPos.GetZ(), m_vStickNormal.GetX(), m_vStickNormal.GetY(), m_vStickNormal.GetZ(), m_iStickComponent);
	}
#endif

	StickToEntity();
}

bool CProjectile::NetworkStick(bool bStickEntity, CEntity* pStickEntity, Vector3& vStickPosition, Quaternion& stickQuatern, s32 iStickComponent, u32 /*iStickMaterialId*/)
{
	m_pStickEntity = pStickEntity;

#if __BANK
	if (pStickEntity)
	{
		weaponDebugf3("[projectile] NetworkStick(): m_pStickEntity - %s[%p], vStickPosition - <%.2f,%.2f,%.2f>, iStickComponent - %i", pStickEntity->GetIsDynamic() ? AILogging::GetDynamicEntityNameSafe(static_cast<CDynamicEntity*>(pStickEntity)) : pStickEntity->GetModelName(), pStickEntity,
			vStickPosition.GetX(),vStickPosition.GetY(),vStickPosition.GetZ(), iStickComponent);
	}
#endif

	if (pStickEntity && AssertVerify(pStickEntity->GetIsPhysical()))
	{
		u32 nFlags = ATTACH_STATE_BASIC|ATTACH_FLAG_INITIAL_WARP|ATTACH_FLAG_COL_ON|ATTACH_FLAG_DELETE_WITH_PARENT;

		m_iStickComponent = iStickComponent;

		// wake the target entity  
		if (!pStickEntity->GetIsTypePed() && pStickEntity->GetIsPhysical())
		{	
			static_cast<CPhysical*>(pStickEntity)->ActivatePhysics();
		}

		AttachToPhysicalBasic((CPhysical*)pStickEntity, GetHitBoneIndexFromFrag(), nFlags, &vStickPosition, &stickQuatern);
	}
	else
	{
		if (bStickEntity)
		{
			// the projectile is stuck to a non-networked entity, so just stick to the ground at this location
			if (!PlaceOnGroundProperly())
			{
				return false;
			}
		}

		Matrix34 projM = MAT34V_TO_MATRIX34(GetTransform().GetMatrix());

		Quaternion q;
		q.FromMatrix34(projM);

		// Attach to the world
		AttachToWorldBasic(ATTACH_STATE_WORLD|ATTACH_FLAG_INITIAL_WARP|ATTACH_FLAG_COL_ON, projM.d, &q);
	}

	OnAttachment();

	return true;
}

void CProjectile::DetachFromStickEntity()
{
	m_pStickEntity = NULL;
	m_vStickPos.Zero();
	m_vStickNormal.Zero();
	m_iStickComponent = 0;
	m_iStickMaterialId = 0;

	m_iFlags.ClearFlag( PF_ProcessCollisionProbe );
	m_iFlags.ClearFlag( PF_Sticked );
	DetachFromParent( DETACH_FLAG_ACTIVATE_PHYSICS|DETACH_FLAG_APPLY_VELOCITY );
}

bool CProjectile::StickToEntity()
{
	if( !GetIsAttached() && m_pStickEntity && m_uDestroyTime == 0 ) // don't try reattaching this if it has exploded
	{
		bool bIsSticky = false;

		const float fImpulseMag = GetVelocity().Mag();

		if(GetInfo() && GetInfo()->GetIsSticky())
		{
			bIsSticky = true;
			audSoundInitParams initParams;
			initParams.Position = VEC3V_TO_VECTOR3(GetTransform().GetPosition());

			if(GetHash() == ATSTRINGHASH("AMMO_PROXMINE", 0xAF2208A7))
			{
				audSoundInitParams initParams;
				initParams.Position = VEC3V_TO_VECTOR3(GetTransform().GetPosition());

				audSoundSet proxSet;
				proxSet.Init(ATSTRINGHASH("SPL_PROXMINE_SOUNDS", 0xAD9FD3AA));
				if(proxSet.IsInitialised())
				{
					g_WeaponAudioEntity.TriggerStickyBombAttatch( VECTOR3_TO_VEC3V( m_vStickPos ), m_pStickEntity, m_iStickComponent, m_iStickMaterialId, fImpulseMag, GetOwner(), proxSet.Find(ATSTRINGHASH("attach", 0xD6AD822D)));			
				}
			}
			else if(GetWeaponFiredFromHash() == ATSTRINGHASH("VEHICLE_WEAPON_MINE", 0x59EAE9A4))
			{
				if(!m_bHasTriggeredAttachSound)
				{
					audSoundSet proxSet;
					proxSet.Init(ATSTRINGHASH("gr_vehicle_proxmine_sounds", 0xBCA4F642));
					if(proxSet.IsInitialised())
					{
						g_WeaponAudioEntity.TriggerStickyBombAttatch( VECTOR3_TO_VEC3V( m_vStickPos ), m_pStickEntity, m_iStickComponent, m_iStickMaterialId, fImpulseMag, GetOwner(), proxSet.Find(ATSTRINGHASH("deploy", 0xC9200261)));			
					}

					m_bHasTriggeredAttachSound = true;
				}
			}
			else if (GetWeaponFiredFromHash() == ATSTRINGHASH("VEHICLE_WEAPON_MINE_KINETIC", 0x3C09584E) ||
					 GetWeaponFiredFromHash() == ATSTRINGHASH("VEHICLE_WEAPON_MINE_EMP", 0x69E10D60) ||
					 GetWeaponFiredFromHash() == ATSTRINGHASH("VEHICLE_WEAPON_MINE_SPIKE", 0xD96DA06C) ||
					 GetWeaponFiredFromHash() == ATSTRINGHASH("VEHICLE_WEAPON_MINE_SLICK", 0x56FACAC7) ||
					 GetWeaponFiredFromHash() == ATSTRINGHASH("VEHICLE_WEAPON_MINE_TAR", 0xF4418BA0))
			{
				if (!m_bHasTriggeredAttachSound)
				{
					audSoundSet proxSet;
					proxSet.Init(ATSTRINGHASH("DLC_AW_Vehicle_Mine_Sounds", 0x291A73D2));
					if (proxSet.IsInitialised())
					{
						g_WeaponAudioEntity.TriggerStickyBombAttatch(VECTOR3_TO_VEC3V(m_vStickPos), m_pStickEntity, m_iStickComponent, m_iStickMaterialId, fImpulseMag, GetOwner(), proxSet.Find(ATSTRINGHASH("deploy", 0xC9200261)));
					}

					m_bHasTriggeredAttachSound = true;
				}
			}
			else
			{
				g_WeaponAudioEntity.TriggerStickyBombAttatch( VECTOR3_TO_VEC3V( m_vStickPos ), m_pStickEntity, m_iStickComponent, m_iStickMaterialId, fImpulseMag, GetOwner(), g_WeaponAudioEntity.FindSound(ATSTRINGHASH("sticky_bomb_hit", 2782906396)));			
			}
		}

		if(bIsSticky &&	NetworkInterface::IsGameInProgress() && m_networkIdentifier.IsValid())
		{
			if(NetworkInterface::GetLocalPhysicalPlayerIndex()==m_networkIdentifier.GetPlayerOwner())
			{
				// this can be unset in certain situations (eg a dying player dropping a sticky bomb)
				if (m_taskSequenceId != -1)
				{
					if(CProjectileManager::GetExistingNetSyncProjectile(m_networkIdentifier) == NULL)
					{
						//Send network sync array information for this sticky bombs existence and location
						s32 indexAddedAt =-1;
						CProjectileManager::AddNetSyncProjectile(indexAddedAt,this);
					}		

					if (!NetworkUtils::GetNetworkObjectFromEntity(m_pStickEntity) && m_pStickEntity->GetIsTypeObject())
					{
						CObject* pObject = (CObject*)m_pStickEntity.Get();

						if (pObject->GetOwnedBy() == ENTITY_OWNEDBY_RANDOM && CNetObjObject::CanBeNetworked(pObject))
						{
							CBaseModelInfo* pModelInfo = pObject->GetBaseModelInfo();
							if ( pModelInfo && !pModelInfo->IsStreamedArchetype() )
							{
								NetworkInterface::RegisterObject(pObject, 0, 0);

								CNetObjObject* pNetObj = static_cast<CNetObjObject*>(pObject->GetNetworkObject());

								if (pNetObj)
								{
									pNetObj->KeepRegistered();
								}
							}
						}
					}
				}
			}
			else
			{	
				//Don't allow sticky until we have received a sync saying it has landed
				//so we can change the matrix pos before attaching to anything
				if (m_pOwner && m_pOwner->GetIsTypePed())
				{
					CPed* pPed = static_cast<CPed*>(m_pOwner.Get());

					if (m_taskSequenceId == -1)
					{
#if __ASSERT
						CNetGamePlayer* pPlayerOwner = NetworkInterface::GetPhysicalPlayerFromIndex(m_networkIdentifier.GetPlayerOwner());
						Assertf(0, "A networked sticky bomb has no task sequence assigned (FX id: %d, Player Owner %s (%d))", m_networkIdentifier.GetFXId(), pPlayerOwner ? pPlayerOwner->GetLogName() : "?", m_networkIdentifier.GetPlayerOwner());
#endif
					}
					else
					{
						CProjectileManager::FireOrPlacePendingProjectile(pPed, m_taskSequenceId, this);
					}
				}

				return true;
			}
		}

		// If we've hit a vehicle chassis we need to try and see if we've hit any components other than the chassis
		if(m_pStickEntity->GetIsTypeVehicle() && m_iStickComponent == 0)
		{
			const int NUM_INTERSECTIONS = 8;
			phIntersection intersections[NUM_INTERSECTIONS];

			static dev_float RADIUS = 0.03f;
			s32 iNumResults = CPhysics::GetLevel()->TestSphere(m_vStickPos, RADIUS, intersections, NULL, ArchetypeFlags::GTA_VEHICLE_TYPE, TYPE_FLAGS_ALL, phLevelBase::STATE_FLAGS_ALL, NUM_INTERSECTIONS);

			// Go over the intersections
			for(s32 i = 0; i < iNumResults; i++)
			{
				// If the intersection component isn't 0 and it belongs to the same phInst as our hit entity
				// set this to the new iComponent
				// The reason for this is that the chassis component and doors etc. overlap collision and we want to
				// "stick" to the door rather than the chassis in case it gets opened
				if(intersections[i].GetComponent() != 0 && intersections[i].GetInstance() == m_pStickEntity->GetCurrentPhysicsInst())
				{
					bool bIsCarVoidMaterial = PGTAMATERIALMGR->UnpackMtlId( intersections[i].GetMaterialId() ) == PGTAMATERIALMGR->g_idCarVoid;
					if(!bIsCarVoidMaterial)
					{
						// Change the component to the new one
						m_iStickComponent = intersections[i].GetComponent();
						break;
					}
				}
			}
		}

		if(m_pStickEntity->GetIsPhysical())
		{
			// Attachment vars
			Vector3 vOffset(m_vStickPos);
			int iStickComponent = m_iStickComponent;

			//
			// We are attaching to a physical object
			//

			CPhysical* pHitPhysical = static_cast<CPhysical*>(m_pStickEntity.Get());
			fragInst* pFragInst = pHitPhysical->GetFragInst();
			if(pFragInst && pFragInst->GetCached())
			{
				// GTAV - B*1953436 - Make sure we use the correct component from the current lod.
				if( pHitPhysical->GetIsTypePed() )
				{
					CPed* pPed = static_cast<CPed*>( pHitPhysical );

					if( pPed->GetRagdollInst() )
					{
						// Maintaining the original behavior of using the passed in component if it can not be mapped
						// Would bailing out made more sense though?
						int ragdollComponent = pPed->GetRagdollInst()->MapRagdollLODComponentHighToCurrent( m_iStickComponent );
						if (Verifyf(ragdollComponent != -1, "Invalid ragdoll component %d", ragdollComponent))
						{
							iStickComponent = ragdollComponent;
						}
					}
				}

				if(pFragInst->GetCacheEntry()->GetHierInst())
				{
					if( pFragInst->GetChildBroken( iStickComponent ) )
					{
						// The component we hit has been broken, so don't try to attach as the matrix will be zeroed out
						return false;
					}
				}

				//	If we have hit a driver/passenger of a bike or quad, then just bounce off
				if(pHitPhysical->GetIsTypeVehicle())
				{
					phBoundComposite* pBoundComposite = static_cast<phBoundComposite*>(pFragInst->GetArchetype()->GetBound());

					if (pBoundComposite)
					{
						CVehicle* pVehicle = static_cast<CVehicle*>(pHitPhysical);

						//	If this vehicle part is smashed then don't try to attach
						if( pVehicle->IsHiddenFlagSet( iStickComponent ) )
						{
							spdSphere projectileSphere;
							const float fProjectileWidth = GetBoundingBoxMax().GetZ() - GetBoundingBoxMin().GetZ();
							projectileSphere.Set( RCC_VEC3V( m_vStickPos ), ScalarV( fProjectileWidth ) );
							spdSphere smashSphere( CVehicleGlassManager::GetVehicleGlassComponentSmashSphere( pHitPhysical, iStickComponent, false ) );
							if( projectileSphere.IntersectsSphere( smashSphere ) )
							{
								return false;
							}
						}

						if (pVehicle->InheritsFromBike() || pVehicle->InheritsFromQuadBike() || pVehicle->InheritsFromAmphibiousQuadBike())
						{
							//	If the GTA_VEH_SEAT_INCLUDE_FLAGS is set on this component then we hit a ped driver/passenger
							if (pBoundComposite->GetIncludeFlags( iStickComponent ) == (ArchetypeFlags::GTA_VEH_SEAT_INCLUDE_FLAGS))
							{
								return false;
							}
						}
					}
				}
			}

			// wake the target entity  
			pHitPhysical->ActivatePhysics();

			// Get the bone matrix
			Matrix34 boneMat;
			if(pHitPhysical->GetSkeleton())
			{
				pHitPhysical->GetGlobalMtx(GetHitBoneIndexFromFrag(), boneMat);

				// B*1853481: Frag objects that had switched to a damaged model were returning zeroed out bone matrices and so sticky bombs couldn't stick to them.
				// Check if a matrix from the undamaged skeleton is zeroed out and if it is, uses the corresponding bone from the damaged skeleton.
				// This allows sticky bombs to attach to damaged objects like blown up gas tanks.
				if(boneMat.a.IsZero())
				{
					if(pHitPhysical->GetFragInst() && pHitPhysical->GetFragInst()->GetCacheEntry() && pHitPhysical->GetFragInst()->GetCacheEntry()->GetHierInst() 
						&& pHitPhysical->GetFragInst()->GetCacheEntry()->GetHierInst()->damagedSkeleton)
					{
						Mat34V boneMatV;
						pHitPhysical->GetFragInst()->GetCacheEntry()->GetHierInst()->damagedSkeleton->GetGlobalMtx(GetHitBoneIndexFromFrag(), boneMatV);
						boneMat.Set(MAT34V_TO_MATRIX34(boneMatV));
					}
				}				

				//B*1789795: don't attach if matrix is not orthonormal
				if (!boneMat.IsOrthonormal(0.05f))
				{
					return false;
				}
			}
			else
			{
				boneMat.Set(MAT34V_TO_MATRIX34(pHitPhysical->GetMatrix()));
				//B*1789795: don't attach if matrix is not orthonormal
				if (!boneMat.IsOrthonormal(0.05f))
				{
					return false;
				}
			}

			if(!weaponVerifyf(!boneMat.a.IsZero(), "CProjectile::StickToEntity - Attaching to broken off bone on '%s'",pHitPhysical->GetDebugName()))
			{
				return false;
			}

			//DR_PROJECTILE_ONLY(debugPlayback::AddSimpleMatrix("boneMat", RCC_MAT34V(boneMat), 0.5f));
			// Convert the world space offset into bone space
			boneMat.UnTransform(vOffset);

			Assertf(vOffset.Mag2() < 1000000.0f, "Invalid offset %.2f specified for attachment (offset too large). vOffset: X:%.2f Y:%.2f Z:%.2f, m_vStickPos: X:%.2f Y:%.2f Z:%.2f, hit bone: %i, bone pos: X:%.2f Y:%.2f Z:%.2f, hit entity: %s", 
				vOffset.Mag2(), vOffset.GetX(), vOffset.GetY(), vOffset.GetZ(),
				m_vStickPos.GetX(), m_vStickPos.GetY(), m_vStickPos.GetZ(),
				GetHitBoneIndexFromFrag(),
				boneMat.d.GetX(), boneMat.d.GetY(), boneMat.d.GetZ(),
				pHitPhysical->GetDebugName()
				);

			// Rotation offset (preserve as much of the original rotation as possible)
 			Matrix34 m;
			if (!GetInfo()->GetDontAlignOnStick())
			{
				m.c = m_vStickNormal;
				m.a.Cross(m.c, VEC3V_TO_VECTOR3( GetTransform().GetB()));
				m.a.Normalize();
				m.b.Cross(m.c, m.a);
				m.b.Normalize();
			}
			else
			{
				m = MAT34V_TO_MATRIX34(GetTransform().GetMatrix());
			}
			m.d = m_vStickPos;
			m.Dot3x3Transpose(boneMat);

#if DR_PROJECTILE_ENABLED
			Vector3 reTransformed;
			boneMat.Transform(vOffset, reTransformed);
			DR_PROJECTILE_ONLY(debugPlayback::AddSimpleLine("vOffset", VECTOR3_TO_VEC3V( reTransformed ), VECTOR3_TO_VEC3V( boneMat.d ), Color32(0,0,0), Color32(0,0,0)));
			DR_PROJECTILE_ONLY(debugPlayback::AddSimpleSphere("m_vStickPos", VECTOR3_TO_VEC3V( m_vStickPos ), 0.03f, Color32(0,0,0)));
			DR_PROJECTILE_ONLY(debugPlayback::AddSimpleLine("m_vStickNormal", VECTOR3_TO_VEC3V( m_vStickPos ), VECTOR3_TO_VEC3V( m_vStickPos+m_vStickNormal ), Color32(128,128,128), Color32(0,0,0)));
#endif
			Quaternion q;
			m.ToQuaternion(q);

			//B*1784521: Ensure sticky projectile is destroyed with parent
			u32 nFlags = ATTACH_STATE_BASIC|ATTACH_FLAG_INITIAL_WARP|ATTACH_FLAG_COL_ON|ATTACH_FLAG_DELETE_WITH_PARENT;

			// Attach to the hit object
			AttachToPhysicalBasic(pHitPhysical, GetHitBoneIndexFromFrag( m_pStickEntity, iStickComponent ), nFlags, &vOffset, &q);

			OnAttachment();
		}
		else
		{
			// Rotation offset (preserve as much of the original rotation as possible)
			Matrix34 m;
			if(!GetInfo()->GetDontAlignOnStick())
			{
				m.c = m_vStickNormal;
				m.a.Cross(m.c, VEC3V_TO_VECTOR3(GetTransform().GetB()));
				m.a.Normalize();
				m.b.Cross(m.c, m.a);
				m.b.Normalize();
			}
			else
			{
				m = MAT34V_TO_MATRIX34(GetTransform().GetMatrix());
			}
			m.d = m_vStickPos;

			Quaternion q;
			q.FromMatrix34(m);

			// Attach to the world
			AttachToWorldBasic(ATTACH_STATE_WORLD|ATTACH_FLAG_INITIAL_WARP|ATTACH_FLAG_COL_ON, m.d, &q);

			OnAttachment();
		}
	}

	return true;
}

s16 CProjectile::GetHitBoneIndexFromFrag() const
{
	return GetHitBoneIndexFromFrag( m_pStickEntity,  m_iStickComponent);
}

s16 CProjectile::GetHitBoneIndexFromFrag(const CEntity* pStickEntity, s32 iStickComponent)
{
	// Get the bone from the passed in component - this is the part of the physical we are attaching too
	s16 iHitBone = 0;

	if(pStickEntity)
	{
		fragInst* pFragInst = pStickEntity->GetFragInst();
		if(pFragInst && pFragInst->GetCached())
		{
			if(weaponVerifyf(iStickComponent >= 0 && iStickComponent < pFragInst->GetTypePhysics()->GetNumChildren(), "Trying to index stick component: %d with num children: %d.", iStickComponent, pFragInst->GetTypePhysics()->GetNumChildren()))
			{
				fragTypeChild* pChild = pFragInst->GetTypePhysics()->GetAllChildren()[iStickComponent];
				iHitBone = (s16) pFragInst->GetType()->GetBoneIndexFromID((s16)pChild->GetBoneID());

				if(pStickEntity->GetIsTypeVehicle())
				{
					// If we're hitting vehicle glass we want to attach to the parent bone, otherwise we will lose the attachment
					//   as soon as the window cracks since the bone matrix gets zeroed out
					// NOTE: We probably want to do this on non-vehicles too
					if(CVehicleGlassManager::IsComponentSmashableGlass(static_cast<const CPhysical*>(pStickEntity),iStickComponent))
					{
						s16 parentBone = (s16)pFragInst->GetType()->GetSkeletonData().GetParentIndex(iHitBone);
						if(parentBone >= 0)
						{
							iHitBone = parentBone;
						}
					}
					else if( pFragInst->GetCacheEntry()->GetHierInst() && pFragInst->GetChildBroken( iStickComponent ) )
					{  
						iHitBone = 0; //B*2056552 Synced component is broken. It's not glass so we don't know if it has a  parent, just fall back to root bone 
					}
				}
				else if(pStickEntity->GetIsTypeObject())
				{
					if( pFragInst->GetCacheEntry()->GetHierInst() && pFragInst->GetChildBroken( iStickComponent ) )
					{
						physicsDisplayf("B*3196515: Object with sticky bomb is broken!");
					}
				}
			}
		}
	}

	return iHitBone;
}

void CProjectile::GetStartAndEndVectorsForProjectile(Vector3& vStart, Vector3& vEnd) const
{
	// Calculate the direction vector of the line
	Vector3 vDir(m_vOldSpeed);

	if(vDir.Mag2() == 0.0f)
	{
		// If we have no valid last speed - just pretend we are falling down
		vDir = -ZAXIS;
	}

	// Normalise
	vDir.NormalizeFast();

	// Create a start and end position for the test that are based on the current 
	// and last positions extended by the direction vector
	const Vector3 vThisPosition = VEC3V_TO_VECTOR3(GetTransform().GetPosition());
	vStart = vThisPosition - vDir;
	vEnd   = vThisPosition + vDir;
}

bool CProjectile::GetIsExplodingInAir(bool bCollided, const CEntity* pHitEntity) const
{
	// Check if we are colliding and not doing a Molotov type explosion
	if(bCollided && GetInfo()->GetExplosionTag() != EXP_TAG_MOLOTOV)
	{	
		if(!pHitEntity || !pHitEntity->GetIsTypeVehicle())
		{
			// No collision entity (or it's not a vehicle), but we are colliding with something, so we are not exploding in the air
			return false;
		}

		// If we are colliding with an aircraft, force the code to fall through to the line test
		const CVehicle* pHitVehicle = static_cast<const CVehicle*>(pHitEntity);
		if(!pHitVehicle->GetIsAircraft())
		{
			return false;
		}
	}

	// Work out how far the line test will be
	float fDistBelow = GetInfo()->GetGroundFxProbeDistance();

	Vector3 vTestPos = VEC3V_TO_VECTOR3(GetTransform().GetPosition());

	// Move it up slightly
	static dev_float DIST_ABOVE = 0.05f;
	vTestPos.z += DIST_ABOVE;

	WorldProbe::CShapeTestProbeDesc probeDesc;
	probeDesc.SetStartAndEnd(vTestPos, vTestPos - Vector3(0.0f, 0.0f, fDistBelow));
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
	if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
	{
		// We have detected a collision, so the explosion isn't counted as being in the air.
		return false;
	}

	// We are exploding in the air
	return true;
}

//create events for peds to react against
void CProjectile::CreateImpactEvents()
{
	if (m_pInfo && m_pInfo->GetDontFireAnyEvents())
	{
		return;
	}

	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_uWeaponFiredFromHash);
	if (m_pOwner)
	{
		if(!pWeaponInfo || !pWeaponInfo->GetSuppressGunshotEvent())
		{
			Vector3 vPos = VEC3V_TO_VECTOR3(GetTransform().GetPosition());
			Vector3 vOwnerPos = VEC3V_TO_VECTOR3(m_pOwner->GetTransform().GetPosition());

			if(!m_iFlags.IsFlagSet(PF_UsingLifeTimer) && !m_iFlags.IsFlagSet(PF_UsingLifeTimeAfterImpact))
			{
				bool bMustBeSeenWhenInVehicle = GetInfo()->GetIsSticky();
				CEventGunShotBulletImpact eventImpact(m_pOwner, vOwnerPos, vPos, true, m_uWeaponFiredFromHash, bMustBeSeenWhenInVehicle);
				eventImpact.SetHitPed(m_pHitPed);
				GetEventGlobalGroup()->Add(eventImpact);
			}

			// Intended for friendly Bullet impact events
			CEventFriendlyFireNearMiss eventFriendlyNearMiss(m_pOwner, vOwnerPos, vPos, true, m_uWeaponFiredFromHash, CEventFriendlyFireNearMiss::FNM_BULLET_IMPACT);
			GetEventGlobalGroup()->Add(eventFriendlyNearMiss);

			if (m_pOwner->GetIsTypePed())
			{
				CPed* pPed = static_cast<CPed*>(m_pOwner.Get());
				if (pPed->IsPlayer())
				{
					// create a blip on mini map for the impact event. Note that the owner could move before this impact, so this isn't precisely correct.
					// If sm_fMinDistSqForProjectileSonarBlip is large this could be an issue.
					if (IsGreaterThanAll(DistSquared(VECTOR3_TO_VEC3V(vPos), VECTOR3_TO_VEC3V(vOwnerPos)), ScalarV(sm_fMinDistSqForProjectileSonarBlip)))
					{
						const bool bScriptRequested = false;
						const bool bOverridePos     = false;
						CMiniMap::CreateSonarBlip(vPos, CMiniMap::sm_Tunables.Sonar.fSoundRange_ProjectileLanding, HUD_COLOUR_RED, pPed, bScriptRequested, bOverridePos);
					}
				}
			}
		}
		else if(m_pOwner->GetIsTypePed() && m_pHitPed && pWeaponInfo && pWeaponInfo->GetIsNonViolent())
		{
			CEventAgitated event(static_cast<CPed *>(m_pOwner.Get()), AT_Griefing);
			m_pHitPed->GetPedIntelligence()->AddEvent(event);
		}
	}
}

void CProjectile::CreatePotentialBlastEvent(float fTimeUntilExplosion)
{
	if (m_pInfo && m_pInfo->GetDontFireAnyEvents())
	{
		return;
	}

	// Get the explosion tag
	const eExplosionTag expTag = GetInfo()->GetExplosionTag();
	if (expTag != EXP_TAG_DONTCARE)
	{
		bool bIsSmoke = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(expTag).bIsSmokeGrenade;

		//Get the radius.
		float fRadius = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(expTag).endRadius;

		//Calculate the time of explosion.
		u32 uTimeOfExplosion = fwTimer::GetTimeInMilliseconds() + (u32)(fTimeUntilExplosion * 1000.0f);

		//Add the event.
		CEventPotentialBlast event(CAITarget(this), fRadius, uTimeOfExplosion);
		event.SetIsSmoke(bIsSmoke);
		GetEventGlobalGroup()->Add(event);

		//Create the shocking event.
		CEventShockingPotentialBlast shockingEvent(*this);
		if(m_iFlags.IsFlagSet(PF_AnyImpactDetected))
		{
			//If the projectile has hit a surface, allow it to be heard.
			shockingEvent.SetAudioReactionRangeOverride(4.0f);
		}
		//Add the shocking event.
		CShockingEventsManager::Add(shockingEvent);
	}
}

void CProjectile::ProcessWhizzByEvents(bool bForceUpdate)
{
#if __BANK
	//Keep debug drawing relevant
	m_bWhizzByEventCheckDebugDrawPending = false;
#endif // __BANK

	if (m_pInfo && m_pInfo->GetDontFireAnyEvents())
	{
		return;
	}

	// Early out if not enough time has elapsed for a check
	const u32 uCurrentTimeMS = fwTimer::GetTimeInMilliseconds();
	if( !bForceUpdate && uCurrentTimeMS < m_uLastWhizzByEventCheckTimeMS + s_uWhizzByEventCheckPeriodMS )
	{
		return;
	}
	else
	{
		m_uLastWhizzByEventCheckTimeMS = uCurrentTimeMS;
	}

	// Get current projectile position
	const Vec3V& vCurrentPosition = GetTransform().GetPosition();
	
	// If last position has not been initialized yet
	if( m_vLastWhizzByPosition.IsZero() )
	{
		// initialize last position
		// NOTE: this results in the same last and current on first time through, and that is OK
		m_vLastWhizzByPosition = VEC3V_TO_VECTOR3(vCurrentPosition);
	}

	// Check that the projectile is moving fast enough
	if(	GetVelocity().Mag2() > rage::square(s_fWhizzByEventMinVelocity) )
	{
		// Perform a spatial array query for peds in radius of the segment from last position to current position
		const int kPedsToAffectMAX = 8;
		CSpatialArrayNode* results[kPedsToAffectMAX];
		int numFound = CPed::ms_spatialArray->FindNearSegment(vCurrentPosition, VECTOR3_TO_VEC3V(m_vLastWhizzByPosition), s_fWhizzByEventRadius, results, kPedsToAffectMAX);
		if( numFound > 0 )
		{
			// Define the event to be handed out
			const bool bIsSilent = false;
			CEventGunShotWhizzedBy whizzedByEvent(m_pOwner, m_vLastWhizzByPosition, VEC3V_TO_VECTOR3(vCurrentPosition), bIsSilent, m_uWeaponFiredFromHash);

			// Add the event to peds in range
			CPed::Pool* pPedPool = CPed::GetPool();
			for(int i = 0; i < numFound; i++)
			{
				CPed* pPed = CPed::GetPedFromSpatialArrayNode(results[i]);
				if(pPed && pPedPool->IsValidPtr(pPed))
				{
					pPed->GetPedIntelligence()->AddEvent(whizzedByEvent);
				}
			}
		}
#if __BANK
		// For debug rendering
		m_vLastWhizzByDebugHeadPosition = VEC3V_TO_VECTOR3(vCurrentPosition);
		m_vLastWhizzByDebugTailPosition = m_vLastWhizzByPosition;
		m_bWhizzByEventCheckDebugDrawPending = true;
#endif // __BANK
	}

	// set the last position using the current position for the next check
	m_vLastWhizzByPosition = VEC3V_TO_VECTOR3(vCurrentPosition);
}

bool CProjectile::IsComponentPartOfSpine(const CPed& rPed, s32 iComponent) const
{
	s32 iBoneTag = rPed.GetBoneTagFromRagdollComponent(iComponent);

	return (iBoneTag == BONETAG_SPINE0 || iBoneTag == BONETAG_SPINE1 || iBoneTag == BONETAG_SPINE2 || iBoneTag == BONETAG_SPINE3);
}

bool CProjectile::GetIsEntityPartOfTrailer(const CEntity* pTargetEntity, const CEntity* pTrailerEntity) const
{
	if (pTrailerEntity && pTrailerEntity->GetIsTypeVehicle())
	{
		const CVehicle* pVehicle = static_cast<const CVehicle*>(pTrailerEntity);
		if (pVehicle->InheritsFromTrailer())
		{
			const CTrailer* pTrailer = static_cast<const CTrailer*>(pVehicle);
			const CVehicle* pAttachedVehicle = pTrailer->GetAttachedToParent();
			if (pAttachedVehicle && pAttachedVehicle == pTargetEntity)
			{
				return true;
			}
		}
		else if (pVehicle->GetAttachedTrailer())
		{
			const CTrailer* pTrailer = static_cast<const CTrailer*>(pVehicle->GetAttachedTrailer());
			if (pTargetEntity == pTrailer)
			{
				return true;
			}
		}
	}

	return false;
}

void CProjectile::ProcessStickyShapeTest( void )
{
	ProcessStickyShapeTest( VEC3V_TO_VECTOR3( m_matPrevious.GetCol3() ), VEC3V_TO_VECTOR3( GetMatrix().GetCol3() ) );
}

void CProjectile::ProcessStickyShapeTest( const Vector3 &vStart, const Vector3 &vEnd )
{
	const float fProjectileWidth = GetBoundingBoxMax().GetZ() - GetBoundingBoxMin().GetZ();

	WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
	WorldProbe::CShapeTestFixedResults<> capsuleResults;
	capsuleDesc.SetResultsStructure( &capsuleResults );
	Mat34V mat = GetMatrix();
	Vector3 vMovement = vEnd - vStart;
	if (vMovement.IsZero())
	{
		vMovement = VEC3V_TO_VECTOR3( Negate( mat.GetCol2() ) );
	}
	vMovement.Normalize();

	capsuleDesc.SetCapsule( vStart, vEnd, fProjectileWidth );
	DR_PROJECTILE_ONLY(debugPlayback::AddSimpleLine("CapsuleTest", VECTOR3_TO_VEC3V(vStart), VECTOR3_TO_VEC3V(vEnd), Color32(0,0,0), Color32(255,255,255)));
	const CEntity* pNoCollisionEntity = (const CEntity*)GetNoCollisionEntity();
	static const s32 iNumExceptions = 2;
	const CEntity* ppExceptions[iNumExceptions];
	ppExceptions[0] = this;
	ppExceptions[1] = pNoCollisionEntity;

#if __BANK
	if (pNoCollisionEntity)
		weaponDebugf3("[projectile] ProcessStickyShapeTest(): pNoCollisionEntity - %s[%p]",pNoCollisionEntity->GetIsDynamic() ? AILogging::GetDynamicEntityNameSafe(static_cast<const CDynamicEntity*>(pNoCollisionEntity)) : pNoCollisionEntity->GetModelName(), pNoCollisionEntity);
#endif

	capsuleDesc.SetExcludeEntities(ppExceptions,iNumExceptions);
	capsuleDesc.SetIncludeFlags( (ArchetypeFlags::GTA_WEAPON_TYPES & ~ArchetypeFlags::GTA_PROJECTILE_TYPE & ~ArchetypeFlags::GTA_PED_TYPE
		& ~ArchetypeFlags::GTA_RIVER_TYPE) );
	capsuleDesc.SetTypeFlags( ArchetypeFlags::GTA_WEAPON_TEST );
	capsuleDesc.SetIsDirected( true );
	capsuleDesc.SetTreatPolyhedralBoundsAsPrimitives(false);

	bool bCheckResults = false;
	bool bIgnoreWheelSphereExclusion = false;
	if( WorldProbe::GetShapeTestManager()->SubmitTest( capsuleDesc ) )
	{
		bCheckResults = true;
	}
	// Check in the opposite direction to compensate for interior bounds (vehicles mostly) that do not have collision
	else
	{
		capsuleDesc.SetCapsule( vEnd, vStart, fProjectileWidth );
		if( WorldProbe::GetShapeTestManager()->SubmitTest( capsuleDesc ) )
		{
			bCheckResults = true;
			bIgnoreWheelSphereExclusion = true;
		}
	}

	if( bCheckResults )
	{
		//Find the first contact along movement
		int nNumHitResults = capsuleResults.GetNumHits();
		float fMinDot = FLT_MAX;
		int iBestHit = -1;
		for( int i = 0; i < nNumHitResults; ++i )
		{
			CEntity* pStickEntity = capsuleResults[i].GetHitEntity();

			if( m_ignoreDamageEntityAttachParent )
			{
#if __BANK
				if (m_pIgnoreDamageEntity)
				{
					weaponDebugf3("[projectile] ProcessStickyShapeTest(): m_ignoreDamageEntityAttachParent - true, m_pIgnoreDamageEntity - %s[%p], pStickEntity - %s[%p]",
						m_pIgnoreDamageEntity.Get()->GetIsDynamic() ? AILogging::GetDynamicEntityNameSafe(static_cast<CDynamicEntity*>(m_pIgnoreDamageEntity.Get())) : m_pIgnoreDamageEntity.Get()->GetModelName(),m_pIgnoreDamageEntity.Get(),
						pStickEntity && pStickEntity->GetIsDynamic() ? AILogging::GetDynamicEntityNameSafe(static_cast<CDynamicEntity*>(pStickEntity)) : (pStickEntity ? pStickEntity->GetModelName() : ""),pStickEntity);
				}
#endif

				if (m_pIgnoreDamageEntity && pStickEntity && pStickEntity->GetAttachParent() == m_pIgnoreDamageEntity )
				{
#if __BANK
					weaponDebugf3("[projectile] ProcessStickyShapeTest: Ignoring impact with entity %s[%p], because its attach parent is %s[%p]",
						pStickEntity->GetIsDynamic() ? AILogging::GetDynamicEntityNameSafe(static_cast<CDynamicEntity*>(pStickEntity)) : pStickEntity->GetModelName(), pStickEntity,
						m_pIgnoreDamageEntity.Get()->GetIsDynamic() ? AILogging::GetDynamicEntityNameSafe(static_cast<CDynamicEntity*>(m_pIgnoreDamageEntity.Get())) : m_pIgnoreDamageEntity.Get()->GetModelName(), m_pIgnoreDamageEntity.Get());
#endif

					continue;
				}
			}

			if( pStickEntity && CProjectile::ShouldStickToEntity( pStickEntity, GetOwner(), fProjectileWidth, capsuleResults[i].GetHitPositionV(), capsuleResults[i].GetHitComponent(), capsuleResults[i].GetHitMaterialId(), bIgnoreWheelSphereExclusion, GetInfo()->GetShouldStickToPeds(), m_bHitPedFriendly ) )
			{
				float fTestDot = capsuleResults[i].GetHitPosition().Dot(vMovement);
				if (fTestDot < fMinDot)
				{
					iBestHit = i;
					fMinDot = fTestDot;
				}
			}
		}

		if (iBestHit > -1)
		{
			m_pStickEntity = capsuleResults[iBestHit].GetHitEntity(); 
			m_iStickComponent = capsuleResults[iBestHit].GetHitComponent();
			m_iStickMaterialId = capsuleResults[iBestHit].GetHitMaterialId();
		
			//Okay capsuleResults[iBestHit].GetHitNormal(); is our fallback normal but lets try probing to average out bumps a little lower down
			m_vStickNormal = capsuleResults[iBestHit].GetHitNormal();

			// Push back the position slightly to match up with the projectile mesh (since all objects use the root position)
			Vec3V vHitPos = capsuleResults[iBestHit].GetHitPositionV();
			Vector3 vPositionNudge = m_vStickNormal;
			vPositionNudge.Scale( fProjectileWidth * 0.5f );

			// Check if we should adjust the position nudge; super hack due to female MP player ragdoll bounds not matching up well
			if (NetworkInterface::IsGameInProgress() && m_pStickEntity && m_pStickEntity->GetIsTypePed())
			{
				CPed* pStickPed = static_cast<CPed *>(m_pStickEntity.Get());
				if (!pStickPed->IsMale() && pStickPed->IsPlayer() && IsComponentPartOfSpine(*pStickPed, m_iStickComponent) && !CTaskParachute::IsParachutePackVariationActiveForPed(*pStickPed))
				{
					static dev_float s_fAdjustment = 0.05f;
					Vector3 vAdjustment = m_vStickNormal;
					vAdjustment.Negate();
					vAdjustment.Scale(s_fAdjustment);
					vPositionNudge += vAdjustment;
				}
			}

			if (m_pStickEntity)
			{
				Vec3V vNormal = RCC_VEC3V(m_vStickNormal);
				FindAveragedStickySurfaceNormal(m_pOwner, *m_pStickEntity, vHitPos, vNormal, fProjectileWidth);
				m_vStickNormal = VEC3V_TO_VECTOR3(vNormal);

				// GTAV B*1953436 - if the entity is a ped make sure we store the high lod 
				// component. We can then map it to the correct current lod when we attach it
				if( m_pStickEntity->GetIsTypePed() )
				{
					CPhysical* pHitPhysical = static_cast<CPhysical*>(m_pStickEntity.Get());
					fragInst* pFragInst = pHitPhysical->GetFragInst();
					if(pFragInst && pFragInst->GetCached())
					{
						int ragdollComponent = fragInstNM::MapRagdollLODComponentCurrentToHigh(m_iStickComponent, pFragInst->GetCurrentPhysicsLOD());
						if (weaponVerifyf(ragdollComponent != -1, "Invalid ragdoll component %d", ragdollComponent))
						{
							m_iStickComponent = ragdollComponent;
						}
						

					}
				}
			}

			DR_PROJECTILE_ONLY(debugPlayback::AddSimpleSphere("_position nudge", vHitPos, 0.01f, Color32(255,255,255)));
			m_vStickPos = VEC3V_TO_VECTOR3(vHitPos + VECTOR3_TO_VEC3V(vPositionNudge));
			DR_PROJECTILE_ONLY(debugPlayback::AddSimpleLine("position nudge", vHitPos, VECTOR3_TO_VEC3V(m_vStickPos), Color32(255,255,255), Color32(255,255,255)));

#if __BANK
			CEntity* pHitEntity = capsuleResults[iBestHit].GetHitEntity();
			if (pHitEntity)
			{
				weaponDebugf3("[projectile] ProcessStickyShapeTest(): m_pStickEntity - %s[%p], m_vStickPos - <%.2f,%.2f,%.2f>, m_vStickNormal - <%.2f,%.2f,%.2f>, m_iStickComponent - %i",
					pHitEntity->GetIsDynamic() ? AILogging::GetDynamicEntityNameSafe(static_cast<CDynamicEntity*>(pHitEntity)) : pHitEntity->GetModelName(), pHitEntity,
					m_vStickPos.GetX(),m_vStickPos.GetY(),m_vStickPos.GetZ(), m_vStickNormal.GetX(),m_vStickNormal.GetY(),m_vStickNormal.GetZ(), m_iStickComponent);
			}
#endif
		}
	}
}

bool CProjectile::FindAveragedStickySurfaceNormal(const CEntity *pThrower, const CEntity &rStickEntity, Vec3V_In vHitPos, Vec3V_InOut rNormalInAndOut, float fProjectileWidth)
{
	DR_PROJECTILE_ONLY(debugPlayback::AddSimpleSphere("inhitpos", vHitPos, 0.02f, Color32(255,255,255)));
	DR_PROJECTILE_ONLY(debugPlayback::AddSimpleLine("inhit", vHitPos, vHitPos+rNormalInAndOut, Color32(128,128,128), Color32(0,0,0)));

	//a little set of points to see what we get
	Vec3V vAdjustProbeN = rNormalInAndOut;
	if (pThrower)
	{
		//Just use the thrower's forward direction to try and stick to something
		vAdjustProbeN = pThrower->GetTransform().GetB();
	}
	else
	{
		//Bias towards centre of object (to avoid some of the influence of dodgy
		//external normals)
		Vec3V vAdjustProbe = rStickEntity.GetTransform().GetPosition() - vHitPos;
		vAdjustProbeN = rNormalInAndOut - Normalize(vAdjustProbe);
		vAdjustProbeN = Negate( Normalize(vAdjustProbeN) );
	}

	WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
	capsuleDesc.SetIncludeFlags( (ArchetypeFlags::GTA_ALL_TYPES_WEAPON & ~ArchetypeFlags::GTA_PROJECTILE_TYPE & ~ArchetypeFlags::GTA_PED_TYPE
		& ~ArchetypeFlags::GTA_RIVER_TYPE) );
	capsuleDesc.SetTypeFlags( ArchetypeFlags::GTA_WEAPON_TEST );
	capsuleDesc.SetIsDirected( true );
	capsuleDesc.SetTreatPolyhedralBoundsAsPrimitives(false);
	capsuleDesc.SetDomainForTest(WorldProbe::TEST_AGAINST_INDIVIDUAL_OBJECTS);
	capsuleDesc.SetIncludeInstance( rStickEntity.GetCurrentPhysicsInst() );

	//Probe a little patch
	Vec3V vOffsets[4];
	//vOffsets[0] = Vec3V(0.0f, fProjectileWidth*0.5f, 0.0f);
	//vOffsets[1] = Vec3V(0.05f, -fProjectileWidth*0.5f, 0.0f);
	//vOffsets[2] = Vec3V(-fProjectileWidth*0.5f,-fProjectileWidth*0.5f, 0.0f);
	//TMS: Pulled in slightly from the edges, allow some penetration there if the overall normal looks good
	vOffsets[0] = Vec3V(-fProjectileWidth*0.8f, 0.0f, 0.0f);
	vOffsets[1] = Vec3V(+fProjectileWidth*0.8f, 0.0f, 0.0f);
	vOffsets[2] = Vec3V(0.0f, fProjectileWidth*0.4f, 0.0f);
	vOffsets[3] = Vec3V(0.0f,-fProjectileWidth*0.4f, 0.0f);
	Vec3V vResults[4];
	int iNumResults=0;

	//Build a matrix to offset these in a way that might find us a hit result
	Mat34V matTrans;
	matTrans.SetCol3( vHitPos );
	if (Abs(vAdjustProbeN.GetZf()) > 0.5f)
	{
		matTrans.SetCol0( Cross(Vec3V(V_X_AXIS_WZERO), vAdjustProbeN) );
	}
	else
	{
		matTrans.SetCol0( Cross(Vec3V(V_Z_AXIS_WZERO), vAdjustProbeN) );
	}
	matTrans.SetCol1( Cross(matTrans.GetCol0(), vAdjustProbeN)) ;
	matTrans.SetCol2( Cross(matTrans.GetCol1(), matTrans.GetCol0() ) );
	ReOrthonormalize3x3(matTrans, matTrans);

	for (int i=0 ; i<COUNTOF(vOffsets) ; i++)
	{
		WorldProbe::CShapeTestFixedResults<> capsuleResults2;
		capsuleDesc.SetResultsStructure( &capsuleResults2 );

		Vec3V vPoint = Transform(matTrans, vOffsets[i]);
		vPoint = vPoint - vAdjustProbeN * ScalarV(0.5f);

		DR_PROJECTILE_ONLY(debugPlayback::AddSimpleLine("normalprobetest", vPoint, vPoint+vAdjustProbeN, Color32(255,0,0), Color32(0,255,0)));

		capsuleDesc.SetCapsule( RCC_VECTOR3(vPoint), VEC3V_TO_VECTOR3(vPoint+vAdjustProbeN), 0.01f );

		if( WorldProbe::GetShapeTestManager()->SubmitTest( capsuleDesc ) )
		{
			ScalarV vMinDot(V_FLT_MAX);
			Vec3V vBestPos;
			int iBestHit = -1;
			int nNumHitResults2 = capsuleResults2.GetNumHits();
			for( int ii = 0; ii < nNumHitResults2; ++ii )
			{
				if(&rStickEntity == capsuleResults2[ii].GetHitEntity())
				{
					Vec3V vTestPos = capsuleResults2[ii].GetHitPositionV();
					ScalarV vTestDot = Dot( vAdjustProbeN, vTestPos );
					if (IsLessThanAll(vTestDot, vMinDot))
					{
						iBestHit = ii;
						vMinDot = vTestDot;
						vBestPos = vTestPos;
					}
				}
			}

			if (iBestHit > -1)
			{
				DR_PROJECTILE_ONLY(debugPlayback::AddSimpleSphere("normalprobepos", vBestPos, 0.01f, Color32(0,0,255)));
				vResults[iNumResults] = vBestPos;
				++iNumResults;
			}
		}
	}

	if (iNumResults == COUNTOF(vResults))
	{
		//New normal from averaged positions!
		//rNormalInAndOut = Cross(vResults[2] - vResults[1], vResults[1] - vResults[0]);
		rNormalInAndOut = Cross(vResults[3] - vResults[2], vResults[1] - vResults[0]);
		rNormalInAndOut = Normalize(rNormalInAndOut);
		DR_PROJECTILE_ONLY(debugPlayback::AddSimpleLine("newnormal", vHitPos, vHitPos + rNormalInAndOut, Color32(255,0,0), Color32(0,0,0)));
		return true;
	}
	else
	{
		DR_PROJECTILE_ONLY(debugPlayback::AddSimpleLine("useoriginalnormal", vHitPos, vHitPos + rNormalInAndOut, Color32(255,0,0), Color32(0,0,0)));
	}
	return false;
}

// Destroys the projectile and causes it to explode.
bool CProjectile::ObjectDamage(float fImpulse, const Vector3* pColPos, const Vector3* pColNormal, CEntity* pEntityResponsible, u32 nWeaponUsedHash, phMaterialMgr::Id hitMaterial)
{

	bool bAttachedToPed = GetAttachParent() && GetAttachParent()->GetType() == ENTITY_TYPE_PED;
	bool bIsSticky = m_uWeaponFiredFromHash == WEAPONTYPE_STICKYBOMB.GetHash();

	// don't let projectiles be destroyed when attached to a ped or are ordnances (...unless it's a sticky bomb on a ped!)
	if (GetIsOrdnance() || (bAttachedToPed && !bIsSticky))
	{
		return false;
	}

	if( NetworkInterface::IsGameInProgress() &&
		bIsSticky )
	{
		if(m_iFlags.IsFlagSet(PF_StuckToSpectatorPedOrGhostVeh))
		{
			return false;
		}

		if( bAttachedToPed &&
			pEntityResponsible &&
			pEntityResponsible->GetIsTypePed() &&
			static_cast<CPed*>(pEntityResponsible)->IsAPlayerPed() &&
			static_cast<CPed*>(GetAttachParent())->IsAPlayerPed() )
		{
			const CPed *pVictimPlayer = static_cast<const CPed*>(GetAttachParent());
			if(!NetworkInterface::IsFriendlyFireAllowed(pVictimPlayer, pEntityResponsible))
			{
				return false;
			}
		}

	}

	// don't blow up if flagged not to
	if (m_pInfo && !m_pInfo->GetCanBeDestroyedByDamage())
	{
		return false;
	}

	// don't let impacts from vehicles cause explosions
	if (nWeaponUsedHash == WEAPONTYPE_RAMMEDBYVEHICLE.GetHash() || nWeaponUsedHash == WEAPONTYPE_RUNOVERBYVEHICLE.GetHash())
	{
		return false;
	}

	// only destroy the object if the base CObject damage call is successful
	if (CObject::ObjectDamage(fImpulse, pColPos, pColNormal, pEntityResponsible, nWeaponUsedHash, hitMaterial))
	{
		//Decrement the owners sticky count before passing ownership
		if(m_pOwner && m_pOwner->GetIsTypePed())
		{
			CPed* pPedOwner = static_cast<CPed*>(m_pOwner.Get());
			if(m_pInfo && m_pInfo->GetIsSticky())
			{
				pPedOwner->DecrementStickyCount();

				//Set flag so new owner doesn't decrement their sticky bomb count
				m_iFlags.SetFlag(PF_NoStickyBombOwnership);
			}
		}

		// Change the culprit to the entity who triggered the explosion.
		if (pEntityResponsible)
			m_pOwner = pEntityResponsible;

		m_iFlags.SetFlag(PF_ExplodeNextFrame);
		if (m_pInfo->GetIsSticky() && !m_iFlags.IsFlagSet(PF_Active))
		{
			m_iFlags.SetFlag(PF_Active);
		}
		return true;
	}
	return false;
}

// Init the lifetime values (separated as it didn't always hit in Fire)
void CProjectile::InitLifetimeValues(const f32 fLifeTime)
{
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_uWeaponFiredFromHash);

	if (fLifeTime >= 0.0f)
	{
		m_fLifeTime = fLifeTime;
	}
	else
	{
		m_fLifeTime = GetInfo()->GetLifeTime();

		//B*2133442: Increase lifetime of rockets in MP if locked on and GetLifeTimePlayerLockedOverrideMP parameter is set on the rocket ammo info.
		if (NetworkInterface::IsGameInProgress() && m_pOwner && m_pOwner->GetIsTypeVehicle() && pWeaponInfo && pWeaponInfo->GetAmmoInfo() && pWeaponInfo->GetAmmoInfo()->GetClassId() == CAmmoRocketInfo::GetStaticClassId())
		{
			const CAmmoRocketInfo *pRocketInfo = static_cast<const CAmmoRocketInfo*>(pWeaponInfo->GetAmmoInfo());
			if (pRocketInfo && pRocketInfo->GetLifeTimePlayerVehicleLockedOverrideMP() != -1.0f)
			{
				if (GetAsProjectileRocket() && GetAsProjectileRocket()->GetTarget())
				{
					m_fLifeTime = pRocketInfo->GetLifeTimePlayerVehicleLockedOverrideMP();
				}
			}
		}
	}

	// If we've fired an on-foot homing projectile and aren't locked on, clamp the projectile lifetime to 10 seconds.
	bool bUsingOnFootHomingWeapon = pWeaponInfo && pWeaponInfo->GetIsOnFootHoming();
	if (bUsingOnFootHomingWeapon && GetAsProjectileRocket())
	{
		static dev_float fMaxUnlockedLifeTime = 10.0f;
		if (GetAsProjectileRocket() && !GetAsProjectileRocket()->GetWasLockedOnWhenFired() && m_fLifeTime > fMaxUnlockedLifeTime)
		{
			m_fLifeTime = fMaxUnlockedLifeTime;
		}
	}

	m_fLifeTimeAfterImpact = GetInfo()->GetLifeTimeAfterImpact();
	if(m_fLifeTime >= 0.0f && m_fLifeTimeAfterImpact == 0.0f && (!GetInfo() || !GetInfo()->GetDelayUntilSettled()))
	{
		// Set that we are using the life timer
		m_iFlags.SetFlag(PF_UsingLifeTimer);
	}

	m_fExplosionTime = GetInfo()->GetExplosionTime();
	if(m_fExplosionTime == 0.0f)
	{
		m_fExplosionTime = m_fLifeTime;
	}

	if(m_fLifeTime <= m_fLifeTimeAfterImpact && pWeaponInfo && pWeaponInfo->GetDropWhenCooked())
	{ 
		m_fLifeTimeAfterImpact = m_fLifeTime;	
	}
}

bool CProjectile::ProcessProximityMine()
{
	ProcessProximityMineActivation();

#if DEBUG_DRAW
	TUNE_GROUP_BOOL(PROXIMITY_MINES, bRenderProximityMineDebug, false);
	if (bRenderProximityMineDebug)
	{
		float fProximityRadius = m_pInfo->GetProximityTriggerRadius();
		if (fProximityRadius <= 0.0f)
		{
			// If not defined in projectile info, use the explosion radius
			CExplosionTagData& expTagData = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(GetInfo()->GetExplosionTag());
			fProximityRadius = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionEndRadius(expTagData);			
		}
		grcDebugDraw::Sphere(GetTransform().GetPosition(), fProximityRadius, m_bProximityMineActive ? (m_bProximityMineTriggered ? Color_red : Color_green) : Color_orange, false);
	}
#endif	// DEBUG_DRAW

	if (m_bProximityMineActive)
	{


		ProcessProximityMineTrigger();

		if (m_bProximityMineTriggered)
		{
			// Wait for timer before exploding.
			float fExplodeTime = m_pInfo->GetProximityFuseTimePed();

			if (m_bProximityMineTriggeredByVehicle)
			{
				float fExplodeTimeVehicleSpeed = m_pInfo->GetProximityFuseTimeVehicleSpeed();
				float fScalar = Clamp(m_bProximityMineTriggeredByVehicleSpeed, 0.0f, fExplodeTimeVehicleSpeed) / fExplodeTimeVehicleSpeed;
				fExplodeTime = Lerp(fScalar, m_pInfo->GetProximityFuseTimeVehicleMin(), m_pInfo->GetProximityFuseTimeVehicleMax());
			}


			if (m_fProximityExplodeTimer >= fExplodeTime)
			{
				Displayf("CProjectile::ProcessControl: TriggerExplosion(). Reason: m_fProximityExplodeTimer >= fExplodeTime.");
				m_bProximityMineTriggered = false;
				m_bProximityMineTriggeredByVehicle = false;
				m_bProximityMineActive = false;
				m_fProximityExplodeTimer = 0.0f;
				m_fProximityMineStuckTime = 0.0f;
				m_iFlags.SetFlag(PF_ForceExplosion);
				TriggerExplosion();
				return true;
			}

			m_fProximityExplodeTimer += fwTimer::GetTimeStep();
		}
	}

	return false;
}

void CProjectile::ProcessProximityMineActivation()
{
#if DEBUG_DRAW
	TUNE_GROUP_BOOL(PROXIMITY_MINES, bRenderProximityMineActivationDebug, false);
#endif	// DEBUG_DRAW

	// No need to run any of this if we're already active
	if (m_bProximityMineActive)
	{
		return;
	}

	// Allow script to turn off the whole safe mode system via server tunable
	if (!sm_bProximityMineUseActivationSafeMode)
	{
		m_bProximityMineActivationSafeMode = false;
	}

	// Don't run safe mode checks if we already disabled it by leaving at any point
	if (m_bProximityMineActivationSafeMode)
	{
		// While safe mode is active, block activation of proximity mine if we're still within the radius
		bool bOwnerCurrentlyWithinRadius = false;

		if (m_pOwner && m_pOwner->GetIsTypePed())
		{
			const CPed* pPedOwner = static_cast<CPed*>(m_pOwner.Get());

			Vec3V vBombPosition = GetTransform().GetPosition();

			float fProximityRadius = m_pInfo->GetProximityTriggerRadius();
			if (fProximityRadius <= 0.0f)
			{
				// If not defined in projectile info, use the explosion radius
				CExplosionTagData& expTagData = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(GetInfo()->GetExplosionTag());
				fProximityRadius = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionEndRadius(expTagData);
			}

			if (pPedOwner->GetIsInVehicle())
			{
				const CVehicle* pOwnerVehicle = pPedOwner->GetMyVehicle();

				// If the owner is in a vehicle, we need to check against vehicle bounds for better accuracy
				spdAABB tempBox;
				spdAABB vehLocalBounds = pOwnerVehicle->GetLocalSpaceBoundBox(tempBox);
				Vec3V vBombPositionLocalToVeh = UnTransformOrtho(pOwnerVehicle->GetMatrix(), vBombPosition);
				spdSphere proxSphere(vBombPositionLocalToVeh, ScalarV(fProximityRadius));

				if (vehLocalBounds.IntersectsSphere(proxSphere))
				{
					bOwnerCurrentlyWithinRadius = true;
#if DEBUG_DRAW
					if (bRenderProximityMineActivationDebug)
					{
						grcDebugDraw::BoxOriented(vehLocalBounds.GetMin(), vehLocalBounds.GetMax(), pOwnerVehicle->GetMatrix(), Color_orange, false);
					}
#endif	// DEBUG_DRAW
				}
			}
			else
			{
				// Otherwise, just do a basic distance check
				Vec3V vOwnerPosition = pPedOwner->GetTransform().GetPosition();
				float fDistanceSq = MagSquared(vBombPosition - vOwnerPosition).Getf();

				if (fDistanceSq < square(fProximityRadius))
				{
					bOwnerCurrentlyWithinRadius = true;
#if DEBUG_DRAW
					if (bRenderProximityMineActivationDebug)
					{
						grcDebugDraw::Sphere(vOwnerPosition, 0.25f, Color_orange, false);
					}
#endif	// DEBUG_DRAW
				}
			}
		}

		// If the owner is outside the radius at any point, we should disable safe mode and mine should activate (once the timer is up)
		// This is so we don't block activation when throwing a proximity mine at a far distance, or if the owner leaves and re-enters while activation timer is still running
		if (!bOwnerCurrentlyWithinRadius)
		{
			m_bProximityMineActivationSafeMode = false;
		}
	}
	float fActivationTime = m_bProximityMineRepeatingDetonation ? m_pInfo->GetProximityRepeatedDetonationActivationTime() : m_pInfo->GetProximityActivationTime();
	if (!m_bProximityMineActivationSafeMode && m_fProximityMineStuckTime >= fActivationTime)
	{
		m_bProximityMineActive = true;
	}

	// Always update timer whether safe mode is active or not
	if (m_fProximityMineStuckTime < fActivationTime)
	{
		m_fProximityMineStuckTime += fwTimer::GetTimeStep();
	}
}

void CProjectile::ProcessProximityMineTrigger()
{
#if DEBUG_DRAW
	TUNE_GROUP_BOOL(PROXIMITY_MINES, bRenderProximityMineTriggerDebug, false);
#endif	// DEBUG_DRAW

	// No need to run any of this if we're already triggered
	if (m_bProximityMineTriggered)
	{
		return;
	}

	Vec3V vBombPosition = GetTransform().GetPosition();

	float fProximityRadius = m_pInfo->GetProximityTriggerRadius();
	if (fProximityRadius <= 0.0f)
	{
		// If not defined in projectile info, use the explosion radius
		CExplosionTagData& expTagData = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(GetInfo()->GetExplosionTag());
		fProximityRadius = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionEndRadius(expTagData);
	}

	bool bTriggered = false;

	// Use the entity iterator to gather a list of nearby vehicles and peds
	// We need to search slightly wider than the radius so that we can catch the root position of large vehicles that may be crossing trigger bounds
	TUNE_GROUP_FLOAT(PROXIMITY_MINES, fAdditionalTriggerScanRadius, 5.0f, 0.0f, 20.0f, 0.01f);
	s32 iteratorFlags = IterateVehicles;
	if (m_pInfo->GetProximityCanBeTriggeredByPeds())
	{
		iteratorFlags |= IteratePeds;
	}
	CEntityIterator entityIterator(IteratePeds|IterateVehicles, NULL, &vBombPosition, fProximityRadius + fAdditionalTriggerScanRadius);
	for (CEntity* pEntity = entityIterator.GetNext(); pEntity; pEntity = entityIterator.GetNext())
	{
		if (pEntity->GetIsTypeVehicle())
		{
			const CVehicle* pTriggerVehicle = static_cast<CVehicle*>(pEntity);

			// Ignore vehicles if script turns off new vehicle detection code via server tunable
			if (!sm_bProximityMineUseBetterVehicleDetection)
			{
				continue;
			}

			// Ignore if the vehicle is not moving
			if (pTriggerVehicle->GetVelocity().XYMag2() < SMALL_FLOAT)
			{
				continue;
			}

			if (pTriggerVehicle->GetDriver() && pTriggerVehicle->GetDriver()->IsPlayer())
			{
				// Ignore if remote player driver (as detection should be done on each local machine for accuracy)
				if (!pTriggerVehicle->GetDriver()->IsLocalPlayer())
				{
					continue;
				}

				// Ignore if player driver is ghosted / in passive mode
				if (NetworkInterface::IsGameInProgress() && pTriggerVehicle->GetDriver()->GetNetworkObject())
				{
					CNetObjPlayer* pNearbyNetObjPlayer = static_cast<CNetObjPlayer*>(pTriggerVehicle->GetDriver()->GetNetworkObject());
					if (pNearbyNetObjPlayer && pNearbyNetObjPlayer->IsPassiveMode())
					{
						continue;
					}
				}

			}


			if (!m_pInfo->GetProximityAffectsFiringPlayer() && m_pOwner && m_pOwner->GetIsTypePed())
			{
				const CPed* pPedOwner = static_cast<CPed*>(m_pOwner.Get());
				const CVehicle* vOwnerVehicle = pPedOwner->GetVehiclePedInside();
				if (vOwnerVehicle == pTriggerVehicle)
				{
					continue;
				}
			}

			// Check against vehicle bounds to see if we're intersecting our trigger sphere
			// If the owner is in a vehicle, we need to check against vehicle bounds for better accuracy
			spdAABB tempBox;
			spdAABB vehLocalBounds = pTriggerVehicle->GetLocalSpaceBoundBox(tempBox);
			Vec3V vBombPositionLocalToVeh = UnTransformOrtho(pTriggerVehicle->GetMatrix(), vBombPosition);
			spdSphere proxSphere(vBombPositionLocalToVeh, ScalarV(fProximityRadius));

			// Scale down bounding box slightly to get closer to previous behaviour (when the driver inside the vehicle was the trigger entity)
			TUNE_GROUP_FLOAT(PROXIMITY_MINES, fVehicleTriggerBoundsScale, 0.5f, 0.01f, 1.0f, 0.01f);
			vehLocalBounds.SetMin(vehLocalBounds.GetMin() * ScalarV(fVehicleTriggerBoundsScale));
			vehLocalBounds.SetMax(vehLocalBounds.GetMax() * ScalarV(fVehicleTriggerBoundsScale));

#if DEBUG_DRAW
			if (bRenderProximityMineTriggerDebug)
			{
				grcDebugDraw::BoxOriented(vehLocalBounds.GetMin(), vehLocalBounds.GetMax(), pTriggerVehicle->GetMatrix(), Color_red, false);
			}
#endif	// DEBUG_DRAW

			if (vehLocalBounds.IntersectsSphere(proxSphere))
			{
				// SUCCESS!
				bTriggered = true;

				// Take note of our speed, as we'll use this to adjust trigger fuse time later
				m_bProximityMineTriggeredByVehicle = true;
				m_bProximityMineTriggeredByVehicleSpeed = pTriggerVehicle->GetVelocity().XYMag();

				// Don't need to check any more entities
				break;
			}
		}
		else if (pEntity->GetIsTypePed())
		{
			const CPed* pTriggerPed = static_cast<CPed*>(pEntity);

			// Ignore if in vehicle (as we're doing separate vehicle entity checks to detect those, unless the system is turned off)
			if (pTriggerPed->GetIsInVehicle() && sm_bProximityMineUseBetterVehicleDetection)
			{
				continue;
			}

			// Ignore if ped is dead and hasn't died very recently
			if (pTriggerPed->ShouldBeDead() && fwTimer::GetTimeInMilliseconds() > pTriggerPed->GetTimeOfDeath() + 2000)
			{
				continue;
			}

			if (pTriggerPed->IsPlayer())
			{
				// Ignore if remote player (as detection should be done on each local machine for accuracy)
				if (!pTriggerPed->IsLocalPlayer())
				{
					continue;
				}

				if (!m_pInfo->GetProximityAffectsFiringPlayer() && m_pOwner && m_pOwner->GetIsTypePed())
				{
					const CPed* pPedOwner = static_cast<CPed*>(m_pOwner.Get());
					if (pPedOwner->IsLocalPlayer()) 
					{
						continue;
					}
				}

				// Ignore if player is ghosted / in passive mode
				if (NetworkInterface::IsGameInProgress() && pTriggerPed->GetNetworkObject())
				{
					CNetObjPlayer* pNearbyNetObjPlayer = static_cast<CNetObjPlayer*>(pTriggerPed->GetNetworkObject());
					if (pNearbyNetObjPlayer && pNearbyNetObjPlayer->IsPassiveMode())
					{
						continue;
					}
				}
			}

			// Ignore if too far away from actual trigger radius
			float fPedDistanceSq = MagSquared(vBombPosition - pTriggerPed->GetTransform().GetPosition()).Getf();
			if (fPedDistanceSq > square(fProximityRadius))
			{
				continue;
			}

			// Make sure we have line of sight between the ped and the proximity mine
			WorldProbe::CShapeTestHitPoint probeIsect;
			WorldProbe::CShapeTestResults probeResult(probeIsect);
			WorldProbe::CShapeTestProbeDesc probeDesc;
			probeDesc.SetStartAndEnd(VEC3V_TO_VECTOR3(vBombPosition), VEC3V_TO_VECTOR3(pTriggerPed->GetTransform().GetPosition()));
			probeDesc.SetResultsStructure(&probeResult);
			probeDesc.SetExcludeEntity(pTriggerPed);
			probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_MOVER);
			probeDesc.SetContext(WorldProbe::LOS_Weapon);

#if DEBUG_DRAW
			if (bRenderProximityMineTriggerDebug)
			{
				grcDebugDraw::Line(vBombPosition, pTriggerPed->GetTransform().GetPosition(), Color_red);
			}
#endif	// DEBUG_DRAW

			if (!WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
			{
				// SUCCESS!
				bTriggered = true;

				// Don't need to check any more entities
				break;
			}
		}
	}

	if (bTriggered)
	{
		m_bProximityMineTriggered = true;

		audSoundSet proxSet;
		const u32 soundSetNameHash = GetMineSoundsetHash(GetWeaponFiredFromHash());
		
		if(proxSet.Init(soundSetNameHash))
		{
			// Play trigger audio
			audSoundInitParams initParams;
			initParams.Position = VEC3V_TO_VECTOR3(GetTransform().GetPosition());

			g_WeaponAudioEntity.CreateAndPlaySound(proxSet.Find(ATSTRINGHASH("trigger", 0xC3AFD061)), &initParams);

			if (NetworkInterface::IsGameInProgress())
			{
				CGameScriptId scriptId;
				CPlaySoundEvent::Trigger(initParams.Position, soundSetNameHash, ATSTRINGHASH("trigger", 0xC3AFD061), 0xff, scriptId, 50);
			}
		}
	}
}