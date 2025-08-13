// File header
#include "Weapons/Projectiles/ProjectileRocket.h"

// Game headers
#include "Audio/WeaponAudioEntity.h"
#include "Debug/DebugScene.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Scene/World/GameWorld.h"
#include "Vehicles/Planes.h"

// Debug includes
#if __BANK
#include "Physics/PhysicsHelpers.h"
#include "Weapons/Projectiles/ProjectileManager.h"
#endif

// Macro to disable optimisations if set
WEAPON_OPTIMISATIONS()

#if __BANK
bool CProjectileRocket::sm_bTargetNetPlayer = false;
bool CProjectileRocket::sm_bTargetVehicle = false;
float CProjectileRocket::sm_fHomingTestDistance = 100.0f;
float CProjectileRocket::sm_fLifeTimeOverride = -1.0f;
float CProjectileRocket::sm_fSpeedOverride = 0.0f;
char CProjectileRocket::sm_ProjectileInfo[64] = "AMMO_PLANE_ROCKET"; 
char CProjectileRocket::sm_WeaponInfo[64] = "VEHICLE_WEAPON_PLANE_ROCKET"; 
#endif	//__BANK

//////////////////////////////////////////////////////////////////////////
// CProjectileRocket
//////////////////////////////////////////////////////////////////////////

CProjectileRocket::CProjectileRocket(const eEntityOwnedBy ownedBy, u32 uProjectileNameHash, u32 uWeaponFiredFromHash, CEntity* pOwner, float fDamage, eDamageType damageType, eWeaponEffectGroup effectGroup, const CEntity* pTarget, const CNetFXIdentifier* pNetIdentifier, bool bCreatedFromScript)
: CProjectile(ownedBy, uProjectileNameHash, uWeaponFiredFromHash, pOwner, fDamage, damageType, effectGroup, pNetIdentifier, bCreatedFromScript)
, m_vCachedTargetPos(Vector3::ZeroType)
, m_vLaunchDir(Vector3::ZeroType)
, m_pTarget(pTarget)
, m_fPitch(0.0f)
, m_fRoll(0.0f)
, m_fYaw(0.0f)
, m_fSpeed(0.0f)
, m_fTimeBeforeHoming(0.0f)
, m_fTimeBeforeHomingAngleBreak(0.0f)
, m_fLauncherSpeed(0.0f)
, m_fTimeSinceLaunch(0.0f)
, m_pWhistleSound(NULL)
, m_bIsAccurate(true)
, m_bLerpToLaunchDir(false)
, m_bApplyThrust(true)
, m_bOnFootHomingWeaponLockedOn(false)
, m_bWasHoming(false)
, m_bStopHoming(false)
, m_vCachedDirection(Vector3::ZeroType)
, m_bHasBeenRedirected(false)
, m_bTorpHasBeenOutOfWater(false)
{
	if(pOwner && pOwner->GetIsPhysical())
	{
		SetVelocity(((CPhysical*)pOwner)->GetVelocity());
	}
}

CProjectileRocket::~CProjectileRocket()
{
	if(m_pWhistleSound)
	{
		m_pWhistleSound->StopAndForget();
	}
}

bool CProjectileRocket::ProcessControl()
{
	// Tick down the timer
	m_fTimeBeforeHoming -= fwTimer::GetTimeStep();

	if (m_fTimeBeforeHoming < 0.0f)
	{
		m_fTimeBeforeHomingAngleBreak -= fwTimer::GetTimeStep();
	}

	if(m_pTarget)
	{
		const Vector3 vThisPosition = VEC3V_TO_VECTOR3(GetTransform().GetPosition());
		const Vector3 vTargetPos = GetIsAccurate() ? VEC3V_TO_VECTOR3(m_pTarget->GetTransform().GetPosition()) : GetCachedTargetPosition();

		Vector3 vDistToTarget(vTargetPos);
		vDistToTarget -= vThisPosition;
		float fDistToTarget = vDistToTarget.Mag();
		if(GetInfo() && (fDistToTarget < GetInfo()->GetProximityRadius()))
		{
			const CCollisionRecord * pColRecord = GetFrameCollisionHistory() ? GetFrameCollisionHistory()->GetMostSignificantCollisionRecord() : NULL;

			// This'll probably crash if we pass in a zero collision normal? TriggerExplosion() seems safer.
			Explode(vThisPosition,	pColRecord ? pColRecord->m_MyCollisionNormal : Vector3(Vector3::ZeroType), 
				pColRecord ? pColRecord->m_pRegdCollisionEntity.Get() : NULL, true);

			return true;
		}

		bool bShouldExplode = false;
		TUNE_GROUP_FLOAT(HOMING_ATTRACTOR, THRUSTER_PROXIMITY_RADIUS, 2.25f, 0.01f, 50.0f, 0.01f);
		if (!m_bStopHoming && m_pTarget && m_pTarget->GetIsTypeVehicle())
		{
			const CVehicle* pTargetVeh = static_cast<const CVehicle*>(m_pTarget.Get());
			if (MI_JETPACK_THRUSTER.IsValid() && pTargetVeh->GetModelIndex() == MI_JETPACK_THRUSTER)
			{
				if (fDistToTarget <= THRUSTER_PROXIMITY_RADIUS)
				{
					bShouldExplode = true;
				}
			}
		}

		TUNE_GROUP_FLOAT(HOMING_ATTRACTOR, REDIRECTION_PROXIMITY_RADIUS, 3.5f, 0.01f, 50.0f, 0.01f);
		if (m_bHasBeenRedirected && GetInfo() && fDistToTarget < REDIRECTION_PROXIMITY_RADIUS)
		{
			bShouldExplode = true;
		}

		if(bShouldExplode)
		{
			TriggerExplosion();

			if (m_pTarget->GetIsTypeObject())
			{
				const CObject* pObject = static_cast<const CObject*>(m_pTarget.Get());
				const CProjectile* pProjectileTarget = pObject->GetAsProjectile();
				if (pProjectileTarget)
				{
					CProjectile* pNonConstProjectileTarget = const_cast<CProjectile*>(pProjectileTarget);

					if(NetworkInterface::IsGameInProgress() && pNonConstProjectileTarget->GetOwner() && pNonConstProjectileTarget->GetOwner()->GetIsTypePed())
					{
						CPed* ownerPed = SafeCast(CPed, pNonConstProjectileTarget->GetOwner());
						if(ownerPed && ownerPed->IsPlayer() && ownerPed->GetNetworkObject())
						{
							CRemoveProjectileEntity::Trigger(ownerPed->GetNetworkObject()->GetObjectID(), pNonConstProjectileTarget->GetTaskSequenceId());
						}
					}

					pNonConstProjectileTarget->Destroy();
				}
			}
		}

		// Update the homing information
		// UGHHH! Unfortunately changing this const to non-const leads to a tree of compile issues
		if(m_fTimeBeforeHoming < 0.0f && !m_bLerpToLaunchDir)
		{
			CEntity* pNonConstTargetEntity = const_cast<CEntity*>(m_pTarget.Get());
			if( pNonConstTargetEntity )
			{
				pNonConstTargetEntity->SetHomingProjectileDistance( fDistToTarget );
				if(pNonConstTargetEntity->GetIsTypePed())
				{
					CPed* pPed = static_cast<CPed*>(pNonConstTargetEntity); 

					if(pPed)
					{
						CVehicle* pVehicle = pPed->GetVehiclePedInside(); 

						if(pVehicle)
						{
							pVehicle->SetHomingProjectileDistance( fDistToTarget );
						}
					}
				}
			}
		}
	}

	// Base class
	return CProjectile::ProcessControl();
}

ePhysicsResult CProjectileRocket::ProcessPhysics(float fTimeStep, bool bCanPostpone, s32 iTimeSlice)
{
	if( !GetIsScriptControlled() && GetCurrentPhysicsInst() && GetCurrentPhysicsInst()->IsInLevel() && m_iFlags.IsFlagSet(PF_Active) && !GetIsAttached())
	{
		const CAmmoRocketInfo* pInfo = GetInfo();
		bool bForceMatrixUpdate = pInfo->GetShouldThrustUnderwater() && pInfo->GetUseGravityOutOfWater();

		if(m_bLerpToLaunchDir || bForceMatrixUpdate)
		{
			// Construct a new matrix
			Matrix34 mat(MAT34V_TO_MATRIX34(GetTransform().GetMatrix()));
		
			Vector3 vProjectileDirection;
			if(bForceMatrixUpdate)
			{
				vProjectileDirection = GetVelocity();
				vProjectileDirection.NormalizeSafe();
				mat.b = vProjectileDirection;
			}
			else
			{
				vProjectileDirection = m_vLaunchDir;
				static dev_float LERP_SPEED = 0.5f;
				Approach(mat.b.x, vProjectileDirection.x, LERP_SPEED, fTimeStep);
				Approach(mat.b.y, vProjectileDirection.y, LERP_SPEED, fTimeStep);
				Approach(mat.b.z, vProjectileDirection.z, LERP_SPEED, fTimeStep);
			}

			static dev_float ACHIEVED_LAUNCH_DIR = 0.01f;
			if(mat.b.Dist2(vProjectileDirection) < square(ACHIEVED_LAUNCH_DIR))
			{
				m_bLerpToLaunchDir = false;
			}

			if (m_pOwner && m_pOwner->GetIsTypeVehicle() && m_networkIdentifier.IsClone())
			{
				const CVehicle* pVehicleOwner = static_cast<const CVehicle*>(m_pOwner.Get());
				if (pVehicleOwner && pVehicleOwner->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_USE_AIRCRAFT_STYLE_WEAPON_TARGETING))
				{
					m_bLerpToLaunchDir = false;
				}
			}

			mat.b.Normalize();

			if(abs(vProjectileDirection.z) > 0.99f)
			{
				mat.a = Vector3(1.0f, 0.0f, 0.0f);
				mat.c.Cross(mat.a, mat.b);
				mat.c.Normalize();
				mat.a.Cross(mat.b, mat.c);
				mat.a.Normalize();
			}
			else
			{
				mat.c = Vector3(0.0f, 0.0f, 1.0f);
				mat.a.Cross(mat.b, mat.c);
				mat.a.Normalize();
				mat.c.Cross(mat.a, mat.b);
				mat.c.Normalize();
			}

			SetMatrix(mat);
		}

		// If we should start homing, calculate the pitch, roll and yaw
		if(m_fTimeBeforeHoming <= 0.0f && !m_bLerpToLaunchDir)
		{
			bool bHasTarget = false;
			Vector3 vTargetPos;

			if(m_pTarget && !m_bStopHoming)
			{
				bHasTarget = true;

				//Validate target lock after we start homing
				if (m_bWasHoming)
				{
					if (!IsTargetAngleValid())
					{
						bHasTarget = false;
					}
				}

				if (bHasTarget)
				{
					//Determine target position
					Vector3 vAccurateTargetPos = VEC3V_TO_VECTOR3(m_pTarget->GetTransform().GetPosition());

					TUNE_GROUP_BOOL(ROCKET_TUNE, USE_LOCK_ON_POS, TRUE);
					if (USE_LOCK_ON_POS && m_pTarget->GetIsTypeVehicle())
					{
						const CVehicle *pTargetVehicle = static_cast<const CVehicle*>(m_pTarget.Get());
						if (pTargetVehicle)
						{
							pTargetVehicle->GetLockOnTargetAimAtPos(vAccurateTargetPos);
						}
					}

					vTargetPos = GetIsAccurate() ? vAccurateTargetPos : GetCachedTargetPosition();
				}
			}
			else
			{
#if __DEV
				static dev_bool DEBUG_TARGETING = false;
				if(DEBUG_TARGETING)
				{
					if(CDebugScene::FocusEntities_Get(0))
					{
						vTargetPos = VEC3V_TO_VECTOR3(CDebugScene::FocusEntities_Get(0)->GetTransform().GetPosition());
					}
					else
					{
						vTargetPos = VEC3V_TO_VECTOR3(CGameWorld::FindLocalPlayer()->GetTransform().GetPosition());
					}

					bHasTarget = true;
				}
#endif // __DEV
			}

			// If using an on-foot homing weapon, ensure we have triggered full lock-on before homing.
			const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_uWeaponFiredFromHash);
			bool bUsingOnFootHomingWeapon = pWeaponInfo && pWeaponInfo->GetIsOnFootHoming();

			if( !m_bStopHoming && ((bHasTarget && !bUsingOnFootHomingWeapon) || (bHasTarget && bUsingOnFootHomingWeapon && m_bOnFootHomingWeaponLockedOn)))
			{
				CalcHomingProjectileInputs(vTargetPos);

				m_bWasHoming = true;
				Vector3 vVelocity = GetVelocity();
				vVelocity.Normalize();
				m_vCachedDirection = vVelocity;

				if(m_pTarget && m_pTarget->GetIsTypeVehicle())
				{
					CEntity* pNonConstTargetEntity = const_cast<CEntity*>(m_pTarget.Get());
					static_cast<CVehicle*>(pNonConstTargetEntity)->SetLastTimeHomedAt(fwTimer::GetTimeInMilliseconds());
				}
			}
			else
			{
				// B*2221044: Calculate appropriate pitch/roll/yaw parameters if no longer homing to maintain direction of movement.
				if (m_bWasHoming)
				{
					if(NetworkInterface::IsGameInProgress() && !m_bStopHoming)
					{
						PhysicalPlayerIndex ownerIndex = m_networkIdentifier.GetPlayerOwner();
						netPlayer* netOwner = netInterface::GetPhysicalPlayerFromIndex(ownerIndex);
						if(netOwner && netOwner->IsLocal())
						{
							CBreakProjectileTargetLock::Trigger(m_networkIdentifier);
						}
					}
#if __DEV
					if(NetworkInterface::IsGameInProgress() && !m_bStopHoming)
					{
						weaponDebugf1("CProjectileRocket::ProcessPhysics - Setting m_bStopHoming = true '%s' bHasTarget %s, m_pTarget %s. m_networkIdentifier GetFXId %d, GetPlayerOwner %d",
							GetModelName(),
							bHasTarget?"TRUE":"FALSE",
							m_pTarget?m_pTarget->GetDebugName():"Null target", 
							m_networkIdentifier.GetFXId(),
							m_networkIdentifier.GetPlayerOwner());
					}
#endif // __DEV	
					// Don't allow us to re-home once we've broken off.
					m_bStopHoming = true;
					Vector3 vTargetPos = VEC3V_TO_VECTOR3(GetTransform().GetPosition()) + (m_vCachedDirection * 1000.0f);
					CalcHomingProjectileInputs(vTargetPos);
				}
				else
				{
					m_fPitch = m_fRoll = m_fYaw = 0.0f;
				}
			}
		}

		// We don't want to fight against the strong physical drag applied when traveling through water so only fake the drag if we
		// aren't getting wet. Need to check 'was in water' because the 'in water' flag is only correct on the second time-slice.
		if(pInfo)
		{
			// Disable thrust if projectile is underwater and isn't flagged to thrust underwater 
			bool bInWater = GetIsInWater() || GetWasInWater();
			if(m_bApplyThrust && (bInWater && !pInfo->GetShouldThrustUnderwater()))
			{
				m_bApplyThrust = false;
				m_nPhysicalFlags.bIgnoresExplosions = true;
			}
		}

		ApplyProjectileInputs(fTimeStep);
	}

	// Base class
	return CProjectile::ProcessPhysics(fTimeStep, bCanPostpone, iTimeSlice);
}

void CProjectileRocket::Fire(const Vector3& vDirection, const f32 fLifeTime, f32 fLaunchSpeedOverride, bool bAllowDamping, bool bScriptControlled, bool bCommandFireSingleBullet, bool bIsDrop, const Vector3* pTargetVelocity, bool UNUSED_PARAM(bDisableTrail), bool bAllowToSetOwnerAsNoCollision)
{
	// If using an on-foot homing weapon, ensure we have triggered full lock-on when we fire before enabling homing.
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_uWeaponFiredFromHash);
	bool bUsingOnFootHomingWeapon = pWeaponInfo && pWeaponInfo->GetIsOnFootHoming();
	if (bUsingOnFootHomingWeapon && m_pOwner && m_pOwner->GetIsTypePed())
	{
		const CPed* pPedOwner = static_cast<const CPed*>(m_pOwner.Get());
		if (pPedOwner && pPedOwner->IsPlayer() && pPedOwner->GetPlayerInfo())
		{
			CPlayerPedTargeting &rTargeting = pPedOwner->GetPlayerInfo()->GetTargeting();
			if (rTargeting.GetOnFootHomingLockOnState(pPedOwner) == CPlayerPedTargeting::OFH_LOCKED_ON)
			{
				m_bOnFootHomingWeaponLockedOn = true;
			}
		}
	}

	const CAmmoRocketInfo* pInfo = GetInfo();
	if(pInfo)
	{
		// If we want to customize this on a per weapon basis we can put it in the weapon info
		TUNE_GROUP_FLOAT(ROCKET_TUNE, fTimeBeforeHoming, 0.15f, 0.0f, 1.0f, 0.01f);
		TUNE_GROUP_FLOAT(ROCKET_TUNE, fTimeBeforeHomingRuiner, 0.10f, 0.0f, 1.0f, 0.01f);
		TUNE_GROUP_FLOAT(ROCKET_TUNE, fTimeBeforeHomingAngleBreak, 0.0f, 0.0f, 10.0f, 0.01f);

		if (pInfo->GetShouldUseHomingParamsFromInfo())
		{
			m_fTimeBeforeHoming = pInfo->GetTimeBeforeStartingHoming();
			m_fTimeBeforeHomingAngleBreak = pInfo->GetTimeBeforeHomingAngleBreak();
		}
		else
		{
			m_fTimeBeforeHoming = fTimeBeforeHoming;
			m_fTimeBeforeHomingAngleBreak = fTimeBeforeHomingAngleBreak;

			if (m_pOwner && m_pOwner->GetIsTypeVehicle())
			{
				const CVehicle* pVehicleOwner = static_cast<const CVehicle*>(m_pOwner.Get());
				if (pVehicleOwner && MI_CAR_RUINER2.IsValid() && pVehicleOwner->GetModelIndex() == MI_CAR_RUINER2)
				{
					m_fTimeBeforeHoming = fTimeBeforeHomingRuiner;
				}
			}
		}

		// Slow down projectile from homing launcher against helicopters so they have time to evade
		if (m_bOnFootHomingWeaponLockedOn)
		{
			TUNE_GROUP_FLOAT(ROCKET_TUNE, LaunchSpeedModOnFootVsHeli, 0.75f, 0.1f, 2.0f, 0.01f);
			TUNE_GROUP_FLOAT(ROCKET_TUNE, LaunchSpeedModOnFootVsSlowPlane, 0.85f, 0.1f, 2.0f, 0.01f);

			if (m_pTarget && m_pTarget->GetIsTypeVehicle())
			{
				const CVehicle *pTargetVehicle = static_cast<const CVehicle*>(m_pTarget.Get());
				if (pTargetVehicle && pTargetVehicle->GetIsHeli())
				{
					fLaunchSpeedOverride = pInfo->GetLaunchSpeed() * LaunchSpeedModOnFootVsHeli;
				}
				else if (pTargetVehicle && pTargetVehicle->InheritsFromPlane())
				{
					const CPlane* pTargetPlane = static_cast<const CPlane*>(pTargetVehicle);
					if (pTargetPlane && !pTargetPlane->IsLargePlane() && !pTargetPlane->IsFastPlane())
					{
						fLaunchSpeedOverride = pInfo->GetLaunchSpeed() * LaunchSpeedModOnFootVsSlowPlane;
					}
				}
			}
		}

		// Audio
		DoFireAudio();
		
		// Base class
		CProjectile::Fire(vDirection, fLifeTime, fLaunchSpeedOverride, bAllowDamping, false, bCommandFireSingleBullet, bIsDrop, pTargetVelocity, false, bAllowToSetOwnerAsNoCollision);
	}

	// Cache off whether or not this projectile is script controlled
	SetIsScriptControlled( bScriptControlled );

	// Store dir
	m_vLaunchDir = vDirection;
	m_bLerpToLaunchDir = true;
}

void CProjectileRocket::SetIsRedirected(bool bRedirected)
{ 
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_uWeaponFiredFromHash);
	bool bUsingOnFootHomingWeapon = pWeaponInfo && pWeaponInfo->GetIsOnFootHoming();
	if (bUsingOnFootHomingWeapon)
		m_bOnFootHomingWeaponLockedOn = true;

	m_bHasBeenRedirected = bRedirected;
}

void CProjectileRocket::SetInitialLauncherSpeed( CEntity* pOwner )
{
	if( pOwner && pOwner->GetIsPhysical() && !pOwner->GetIsTypePed())
	{
		Vector3 vOwnerVelocity = static_cast<CPhysical&>(*pOwner).GetLocalSpeed(VEC3V_TO_VECTOR3(GetTransform().GetPosition()), true);
		float fDot = vOwnerVelocity.Dot(VEC3V_TO_VECTOR3(GetTransform().GetB()));
		if(fDot > 0.0f)
			m_fLauncherSpeed = fDot;
		Assert( m_fLauncherSpeed < 1000.0f );
	}
}

#if __BANK
void CProjectileRocket::RenderDebug() const
{
	// Render the inputs using a local flight model
	CFlightModelHelper tempFlightModel;
	tempFlightModel.DebugDrawControlInputs(this, 1.0f, m_fPitch, m_fRoll, m_fYaw);

	// Base class
	CProjectile::RenderDebug();
}
#endif // __BANK

void CProjectileRocket::DoFireAudio()
{
	const CAmmoProjectileInfo* pAmmoInfo = GetInfo();
	if(pAmmoInfo && pAmmoInfo->GetAudioHash() != 0)
	{
		g_WeaponAudioEntity.AttachProjectileLoop(pAmmoInfo->GetAudioHash(), this, &m_pSound, GetIsInWater());
	}
	else
	{
		g_WeaponAudioEntity.AttachProjectileLoop("ROCKET_FLIGHT", this, &m_pSound, GetIsInWater());
	}
}

void CProjectileRocket::ProcessAudio()
{
	if(m_pSound)
	{
		u32 uClientVar;
		m_pSound->GetClientVariable(uClientVar);

		// Check our water state and whether we are playing the appropriate sound
		// If not, kill the current sound and start a new one
		if((GetIsInWater() && uClientVar == 0) || 
			(!GetIsInWater() && uClientVar == 1))
		{
			m_pSound->StopAndForget();
			const CAmmoProjectileInfo* pAmmoInfo = GetInfo();
			if(pAmmoInfo && pAmmoInfo->GetAudioHash() != 0)
			{
				g_WeaponAudioEntity.AttachProjectileLoop(pAmmoInfo->GetAudioHash(), this, &m_pSound, GetIsInWater());
			}
			else
			{
				g_WeaponAudioEntity.AttachProjectileLoop("ROCKET_FLIGHT", this, &m_pSound, GetIsInWater());
			}
		}
	}

	CProjectile::ProcessAudio();
}

bool CProjectileRocket::IsTargetAngleValid() const
{
	if (!m_pTarget)
		return false;

	const CAmmoRocketInfo* pInfo = GetInfo();

	Vector3 v3Diff = VEC3V_TO_VECTOR3(m_pTarget->GetTransform().GetPosition() - GetTransform().GetPosition());
	float fDistanceToTarget = v3Diff.Mag();
	if (Abs(fDistanceToTarget) > 0.f)
		v3Diff.Scale(1.f / fDistanceToTarget);

	TUNE_GROUP_FLOAT(ROCKET_TUNE, DefaultHomingRocketBreakLockAngle, 0.2f, -1.f, 1.f, 0.01f);
	TUNE_GROUP_FLOAT(ROCKET_TUNE, DefaultHomingRocketBreakLockAngleClose, 0.6f, -1.f, 1.f, 0.01f);
	TUNE_GROUP_FLOAT(ROCKET_TUNE, DefaultHomingRocketBreakLockCloseDistance, 20.f, 0.f, 1000.f, 0.01f);
	TUNE_GROUP_BOOL(ROCKET_TUNE, OverrideCombatBehaviourWithDefaultBreakLock, FALSE);

	float fHomingRocketBreakLockAngle = DefaultHomingRocketBreakLockAngle;
	float fHomingRocketBreakLockAngleClose = DefaultHomingRocketBreakLockAngleClose;
	float fHomingRocketBreakLockCloseDistance = DefaultHomingRocketBreakLockCloseDistance;

	if (pInfo && pInfo->GetShouldUseHomingParamsFromInfo())
	{
		fHomingRocketBreakLockAngle = pInfo->GetDefaultHomingRocketBreakLockAngle();
		fHomingRocketBreakLockAngleClose = pInfo->GetDefaultHomingRocketBreakLockAngleClose();
		fHomingRocketBreakLockCloseDistance = pInfo->GetDefaultHomingRocketBreakLockCloseDistance();
	}

	if (pInfo && !pInfo->GetShouldIgnoreOwnerCombatBehaviour() && !OverrideCombatBehaviourWithDefaultBreakLock && m_pOwner)
	{
		const CPed* pPedOwner = m_pOwner->GetIsTypePed() ? static_cast<const CPed*>(m_pOwner.Get()) : (m_pOwner->GetIsTypeVehicle() ? static_cast<const CVehicle*>(m_pOwner.Get())->GetDriver() : NULL);
		if (pPedOwner)
		{
			fHomingRocketBreakLockAngle = pPedOwner->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatHomingRocketBreakLockAngle);
			fHomingRocketBreakLockAngleClose = pPedOwner->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatHomingRocketBreakLockAngleClose);
			fHomingRocketBreakLockCloseDistance = pPedOwner->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatHomingRocketBreakLockCloseDistance);
		}
	}

	// We should only home while target is within a certain threshold
	// this will allow online players to dodge homing rockets at the last moment
	float fDotThreshold = fDistanceToTarget < fHomingRocketBreakLockCloseDistance ? fHomingRocketBreakLockAngleClose : fHomingRocketBreakLockAngle;

	float fDir = DotProduct(v3Diff, VEC3V_TO_VECTOR3(GetTransform().GetB()));

	// B*1783431: ...except when using spycar vertical rockets as we want to home in on targets all around us
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_uWeaponFiredFromHash);
	bool bIgnoreThresholdCheck = pWeaponInfo ? pWeaponInfo->GetIgnoreHomingCloseThresholdCheck() : false;

	//Ignore angle break if we're still in the grace period
	if (m_fTimeBeforeHomingAngleBreak > 0.0f)
	{
		bIgnoreThresholdCheck = true;
	}

	TUNE_GROUP_BOOL(ROCKET_TUNE, IGNORE_DOT_THRESHOLD, FALSE);
	if (fDir > fDotThreshold || bIgnoreThresholdCheck || m_bHasBeenRedirected || IGNORE_DOT_THRESHOLD)
	{
		return true;
	}
	return false;
}

void CProjectileRocket::CalcHomingProjectileInputs(const Vector3& vTargetPos)
{
	Vector3 vSeperation = vTargetPos - VEC3V_TO_VECTOR3(GetTransform().GetPosition());
	Vector3 vSeperationLocal = VEC3V_TO_VECTOR3(GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V(vSeperation)));

	// Not sure if these control variables will need to change per rocket
	// Seems like we could just find one value that works for now
	TUNE_GROUP_FLOAT(ROCKET_TUNE, DefaultHomingRocketTurnRateModifier, 1.f, 0.f, 100.f, 0.01f);
	TUNE_GROUP_BOOL(ROCKET_TUNE, OverrideCombatBehaviourWithDefaultTurnRate, FALSE);

	float fHomingRocketTurnRateModifier = DefaultHomingRocketTurnRateModifier;

	if(!OverrideCombatBehaviourWithDefaultTurnRate && m_pOwner)
	{
		const CPed* pPedOwner = m_pOwner->GetIsTypePed() ? static_cast<const CPed*>(m_pOwner.Get()) : (m_pOwner->GetIsTypeVehicle() ? static_cast<const CVehicle*>(m_pOwner.Get())->GetDriver() : NULL);
		if(pPedOwner)
		{
			fHomingRocketTurnRateModifier = pPedOwner->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatHomingRocketTurnRateModifier);
		}
	}

	float fPitchDiff = vSeperationLocal.AngleX(YAXIS);	// Not really an angle but will do for this
	static dev_float sfTargetWidth = ( DtoR * 2.0f);
	float fTargetWidth = sfTargetWidth / vSeperation.Mag();

	// DO NOT ADD ANY MORE HACKS HERE! Tunables have been pulled into metadata: see HomingRocketParams.
	// This code is here for old vehicles that did not have it added to their metadata yet
	TUNE_GROUP_FLOAT(ROCKET_TUNE, PitchYawRollClampDefault, 1.f, 0.f, 10.f, 0.01f);
	TUNE_GROUP_FLOAT(ROCKET_TUNE, PitchYawRollClampFromNonAircraft, 8.5f, 0.f, 50.f, 0.01f);
	TUNE_GROUP_FLOAT(ROCKET_TUNE, PitchYawRollClampOnFootVsNonHeli, 1.f, 0.f, 10.f, 0.01f);
	TUNE_GROUP_FLOAT(ROCKET_TUNE, PitchYawRollClampOnFootVsHeli, 3.f, 0.f, 10.f, 0.01f);
	TUNE_GROUP_FLOAT(ROCKET_TUNE, PitchYawRollClampOnFootVsSlowPlanes, 3.f, 0.f, 10.f, 0.01f);

	TUNE_GROUP_FLOAT(ROCKET_TUNE, TurnRateModFromNonAircraft, 4.0f, 0.f, 10.f, 0.01f);
	TUNE_GROUP_FLOAT(ROCKET_TUNE, TurnRateModOnFootVsHeli, 0.65f, 0.f, 5.f, 0.01f);
	TUNE_GROUP_FLOAT(ROCKET_TUNE, TurnRateModOnFootVsSlowPlanes, 0.80f, 0.f, 5.f, 0.01f);

	TUNE_GROUP_FLOAT(ROCKET_TUNE, PitchYawRollClampFromOppressor, 2.5f, 0.f, 50.f, 0.01f);
	TUNE_GROUP_FLOAT(ROCKET_TUNE, TurnRateModFromOppressor, 2.0f, 0.f, 10.f, 0.01f);
	
	float fClampValue = PitchYawRollClampDefault; // Default is for aircraft homing rockets

	const CVehicle *pTargetVehicle = m_pTarget && m_pTarget->GetIsTypeVehicle() ? static_cast<const CVehicle*>(m_pTarget.Get()) : nullptr;

	const CAmmoRocketInfo* pRocketAmmoInfo = GetInfo();
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_uWeaponFiredFromHash);
	if (pRocketAmmoInfo && pRocketAmmoInfo->GetShouldUseHomingParamsFromInfo())
	{
		fClampValue = pRocketAmmoInfo->GetPitchYawRollClamp();
		fHomingRocketTurnRateModifier = pRocketAmmoInfo->GetTurnRateModifier();
	}
	else
	{
		// B*3177976: Needs to be tighter for Ruiner 2000 in order to hit other targets on the ground.
		if (m_pOwner && m_pOwner->GetIsTypeVehicle())
		{
			const CVehicle* pOwnerVehicle = static_cast<const CVehicle*>(m_pOwner.Get());
			if (MI_BIKE_OPPRESSOR.IsValid() && pOwnerVehicle->GetModelIndex() == MI_BIKE_OPPRESSOR)
			{
				fClampValue = PitchYawRollClampFromOppressor;
				fHomingRocketTurnRateModifier = TurnRateModFromOppressor;
			}
			else if (!pOwnerVehicle->GetIsAircraft())
			{
				fClampValue = PitchYawRollClampFromNonAircraft;
				fHomingRocketTurnRateModifier = TurnRateModFromNonAircraft;
			}
		}

		// B*2159386: On-Foot Homing Launcher: allow for larger pitch/yaw/roll parameters when target is a heli (as they can dodge more easily).
		if (pWeaponInfo && pWeaponInfo->GetIsOnFootHoming())
		{
			fClampValue = PitchYawRollClampOnFootVsNonHeli;

			if (pTargetVehicle)
			{
				if (pTargetVehicle->GetIsHeli())
				{
					fClampValue = PitchYawRollClampOnFootVsHeli;
					fHomingRocketTurnRateModifier = TurnRateModOnFootVsHeli;
				}
				else if (pTargetVehicle->InheritsFromPlane())
				{
					const CPlane* pTargetPlane = static_cast<const CPlane*>(pTargetVehicle);
					if (pTargetPlane && !pTargetPlane->IsLargePlane() && !pTargetPlane->IsFastPlane())
					{
						fClampValue = PitchYawRollClampOnFootVsSlowPlanes;
						fHomingRocketTurnRateModifier = TurnRateModOnFootVsSlowPlanes;
					}
				}
			}
		}
	}

	if (pWeaponInfo && pWeaponInfo->GetHash() == WEAPONTYPE_VEHICLE_WEAPON_OPPRESSOR2_MISSILE)
	{
		if (sm_fOppressor2MissilePitchYawRollClampOverride != 0.0f)
		{
			fClampValue = sm_fOppressor2MissilePitchYawRollClampOverride;
		}
		if (sm_fOppressor2MissileTurnRateModifierOverride != 0.0f)
		{
			fHomingRocketTurnRateModifier = sm_fOppressor2MissileTurnRateModifierOverride;
		}
	}

	//TODO: Probably wants a separate set of tunings...
	TUNE_GROUP_BOOL(HOMING_ATTRACTOR, APPLY_RUINER_SETTINGS_TO_REDIRECTED_ROCKETS, true);
	if (m_bHasBeenRedirected && APPLY_RUINER_SETTINGS_TO_REDIRECTED_ROCKETS)
	{
		fClampValue = PitchYawRollClampFromNonAircraft;
		fHomingRocketTurnRateModifier = TurnRateModFromNonAircraft;
	}

	TUNE_GROUP_FLOAT(ROCKET_TUNE, ExtraPitchYawRollClampVsThruster, 1.5f, 0.f, 50.f, 0.01f);
	TUNE_GROUP_FLOAT(ROCKET_TUNE, ExtraTurnRateModVsThruster, 0.5f, 0.f, 10.f, 0.01f);
	if (pTargetVehicle && MI_JETPACK_THRUSTER.IsValid() && pTargetVehicle->GetModelIndex() == MI_JETPACK_THRUSTER)
	{
		fClampValue += ExtraPitchYawRollClampVsThruster;
		fHomingRocketTurnRateModifier += ExtraTurnRateModVsThruster;
	}

	const float fPitchControlMult = pRocketAmmoInfo->GetPitchChangeRate() * fHomingRocketTurnRateModifier;	
	const float fYawControlMult = pRocketAmmoInfo->GetRollChangeRate() * fHomingRocketTurnRateModifier;
	const float fRollControlMult = pRocketAmmoInfo->GetYawChangeRate() * fHomingRocketTurnRateModifier;
	
	m_fPitch = CalcHomingInput(fPitchDiff,fTargetWidth,fPitchControlMult);
	m_fPitch = rage::Clamp(m_fPitch, -fClampValue, fClampValue);

	float fYawDiff = fwAngle::LimitRadianAngle(vSeperationLocal.AngleZ(YAXIS));

	m_fYaw = CalcHomingInput(fYawDiff,fTargetWidth,fYawControlMult);
	m_fYaw = rage::Clamp(m_fYaw, -fClampValue, fClampValue);

	// we want rocket to roll to this angle
	float fDesiredRollAngleSin = -pRocketAmmoInfo->GetMaxRollAngleSin() * m_fYaw;
	float fCurrentRollAngleSin = GetTransform().GetA().GetZf();

	// roll input changes our angular velocity, so figure out what angular velocity we want 
	// to achieve our desired roll	
	
	m_fRoll = CalcHomingInput(fDesiredRollAngleSin-fCurrentRollAngleSin,0.0f,fRollControlMult);
	m_fRoll = rage::Clamp(m_fRoll,-fClampValue,fClampValue);			
}

void CProjectileRocket::ApplyProjectileInputs(float fTimestep)
{
	// Roll, pitch and yaw the rocket
	// Based on input member variables which are in range 0->1

	// We override our matrix directly with new orientation instead of using physical torques

	Matrix34 matNew = MAT34V_TO_MATRIX34(GetMatrix());

	// Want to roll about previous frames axis, so that rotation axis won't change after first operation
	const fwTransform& Transform = GetTransform();
	matNew.Rotate(VEC3V_TO_VECTOR3(Transform.GetA()),-m_fPitch*fTimestep);
	matNew.Rotate(VEC3V_TO_VECTOR3(Transform.GetB()),-m_fRoll*fTimestep);
	matNew.Rotate(VEC3V_TO_VECTOR3(Transform.GetC()),-m_fYaw*fTimestep);

	SetMatrix(matNew);

	//Nothing uses gravity but it's here if we need it.
	float gravitystrength = 0.0f;

	Vector3 velNormalised = GetVelocity();
	velNormalised.NormalizeSafe();

	// Thrust component (N)
	Vector3 thrustDir;

	const CAmmoRocketInfo* pInfo = GetInfo();
	if(pInfo)
	{
		if(pInfo->GetShouldThrustUnderwater() && pInfo->GetUseGravityOutOfWater())
		{
			bool bInWater = GetIsInWater() || GetWasInWater();
			if(!bInWater)
			{
				TUNE_GROUP_FLOAT(ROCKET_TUNE, fTorpGravity, -1.5f, -100.0f, 100.0f, 0.1f);
				m_vLaunchDir.z += fTorpGravity*fTimestep;
				m_vLaunchDir.NormalizeSafe();
				m_bTorpHasBeenOutOfWater = true;
			}
			else if(bInWater && m_bTorpHasBeenOutOfWater)
			{
				TUNE_GROUP_FLOAT(ROCKET_TUNE, fTorpZWaterAlignSpeed, 1.0f, -100.0f, 100.0f, 0.1f);
				if(Approach(m_vLaunchDir.z, 0.0f, fTorpZWaterAlignSpeed, fTimestep))
				{
					m_bTorpHasBeenOutOfWater = false;
				}
			}
		}
	}

	if(m_bLerpToLaunchDir)
		thrustDir = m_vLaunchDir;	// While lerping, use launch dir
	else
	{
		thrustDir = matNew.b;

		// B*1910285: Fire straight along launch direction in MP if not a homing rocket. Ensures local & clone rockets are fired along same trajectory.
		if (NetworkInterface::IsGameInProgress() && (m_fPitch == 0.0f && m_fRoll == 0.0f && m_fYaw == 0.0f))
		{
			thrustDir = m_vLaunchDir;
		}
	}
	float forwardDrag = GetInfo()->GetForwardDragCoeff();
	float launchSpeed = m_fLauncherSpeed != 0.0f ? m_fLauncherSpeed : GetInfo()->GetLaunchSpeed();

	// Handle separation of the rocket
	if(GetInfo()->GetSeparationTime() != 0.0f)
	{
		if(m_fTimeSinceLaunch < GetInfo()->GetSeparationTime())
		{
			m_fTimeSinceLaunch += fTimestep;

			// Separation thrust direction is a cheat but saves a member variable, we just need a small force.
			thrustDir = -matNew.c;
			launchSpeed = 100.0f;
			gravitystrength = GRAVITY;
			forwardDrag = 0.0f;
			m_iFlags.SetFlag(PF_TrailInactive);
		}
		else if (m_fTimeSinceLaunch < GetInfo()->GetSeparationTime() + 10.0f )
		{
			m_fTimeSinceLaunch = m_fTimeSinceLaunch + 10.0f;
			m_iFlags.ClearFlag(PF_TrailInactive);
		}
		
	}

	if(m_bApplyThrust)
	{
		// In order to allow decent missile steering we need to cheat drag coefficients to protect against perpendicular travel - similar to slip on a tyre
		// There is some physical basis behind this!
		// Also reference vector for the dot is important as dot product is not linear in the LERP.
		// Designed to deliberately scale up as we head facing towards being perpendicular to velocity direction.
		float dp = NetworkInterface::IsGameInProgress() && !m_pOwner ? 0.0f : Abs(Dot(velNormalised, matNew.a));
		float dragCoeff = (dp * GetInfo()->GetSideDragCoeff()) + (1.0f-dp) * forwardDrag;

		Vector3 resultant_a = (thrustDir * launchSpeed);
		// with drag subtracted (N)
		resultant_a -= velNormalised * dragCoeff * GetVelocity().Mag2();

		// Now as acceleration (F/m = m/s^2)
		resultant_a /= GetMass();  // Because mass is the same we work in accel space instead of force

		// Summed with gravity (m/s^2)
		const Vector3 gravity = Vector3(0,0,-1.0f) * gravitystrength; 
		resultant_a += gravity;

		// Applied by dt to velocity.
		Vector3 vMoveSpeed = GetVelocity() + (resultant_a * fTimestep);

		// Have to clamp to max speed or physics will assert
		if(GetCurrentPhysicsInst() && GetCurrentPhysicsInst()->GetArchetype())
		{
			float fMaxSpeed = GetCurrentPhysicsInst()->GetArchetype()->GetMaxSpeed();
			//Limit the rate of change so we don't apply a crazy force to the ped.
			if(vMoveSpeed.Mag2() > (fMaxSpeed * fMaxSpeed) )
			{
				const float fSpeedScale = vMoveSpeed.Mag() / GetCurrentPhysicsInst()->GetArchetype()->GetMaxSpeed();
				if(fSpeedScale > 1.0f)
					vMoveSpeed /= fSpeedScale;
			}
		}

		SetVelocity(vMoveSpeed);

		if( m_fPitch != 0.0f || m_fRoll != 0.0f || m_fYaw != 0.0f )
		{
			Vector3 vRotAngles(
				-m_fPitch*fTimestep,// pitch delta
				-m_fRoll*fTimestep,	// roll delta
				-m_fYaw*fTimestep	// yaw delta
				);

			float fNewVelMag = vMoveSpeed.Mag();
			if(fNewVelMag < 1.0f)
				vRotAngles.z *= fNewVelMag;

			// Rotate angular velocity into worldspace & apply
			matNew.Transform3x3(vRotAngles);
			SetAngVelocity(vRotAngles.x, vRotAngles.y, vRotAngles.z);
		}
		else
		{
			// We need to zero the angular velocity so if we ricochet it doesn't come boomeranging back at us
			SetAngVelocity(VEC3_ZERO);
		}
	}
}

float CProjectileRocket::CalcHomingInput(float fValueDiff, float fTargetWidth, float fChangeRateMult)
{
	float fSignDiff = Sign(fValueDiff);
	if(fSignDiff*fValueDiff <= fTargetWidth)
	{
		return 0.0f;
	}

	// Remove dead zone
	fValueDiff = fValueDiff - fSignDiff*fTargetWidth;

	float fDesiredChangeRate = fValueDiff * fChangeRateMult;

	return fDesiredChangeRate;
}

#if __BANK
void CProjectileRocket::InitWidgets()
{
	bkBank& bank = BANKMGR.CreateBank("Projectile Rockets");
	bank.PushGroup("Spawn Rockets Behind Current Vehicle", false);
	bank.AddText("Projectile Info:", sm_ProjectileInfo, 64, false);
	bank.AddText("Weapon Info:", sm_WeaponInfo, 64, false);
	bank.AddSlider("Distance behind to spawn:", &sm_fHomingTestDistance, 1.0f, 500.0f, 0.01f);
	bank.AddSlider("Rocket lifetime override:", &sm_fLifeTimeOverride, -1.0f, 1000.0f, 0.01f);
	bank.AddSlider("Rocket speed override:", &sm_fSpeedOverride, -1.0f, 1000.0f, 0.01f);
	bank.AddToggle("Target remote player", &sm_bTargetNetPlayer);
	bank.AddToggle("Target vehicle", &sm_bTargetVehicle);
	bank.AddButton("Spawn rocket", SpawnRocketOnTail);
	bank.PopGroup();
}

void CProjectileRocket::SpawnRocketOnTail()
{
	CPed *pPedTarget = CGameWorld::FindLocalPlayer();
	CPed *pPedTargetRemote = CGameWorld::FindLocalPlayer();
	bool bScriptProjectile = false;

	if(sm_bTargetNetPlayer && NetworkInterface::IsGameInProgress())
	{
		unsigned numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
		const netPlayer * const *remotePhysicalPlayers = netInterface::GetRemotePhysicalPlayers();

		float fBestDist = FLT_MAX;

		for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
		{
			const CNetGamePlayer *remotePlayer = SafeCast(const CNetGamePlayer, remotePhysicalPlayers[index]);
			if(remotePlayer && remotePlayer->GetPlayerPed())
			{
				Vector3 vDiffRemote = VEC3V_TO_VECTOR3(remotePlayer->GetPlayerPed()->GetTransform().GetPosition() - CGameWorld::FindLocalPlayer()->GetTransform().GetPosition());
				float fDistRemote = vDiffRemote.Mag();
				if(fDistRemote < fBestDist)
				{
					fBestDist = fDistRemote;
					pPedTargetRemote = remotePlayer->GetPlayerPed();
					pPedTarget = pPedTargetRemote;
				}
			}
		}
	}
	else
	{
		// Going to fake this as a script projectile for local cases (no owner) to prevent friendly checks / collision problems
		bScriptProjectile = true;
	}

	if(pPedTarget->GetVehiclePedInside())
	{
		Matrix34 m = MAT34V_TO_MATRIX34(pPedTarget->GetVehiclePedInside()->GetTransform().GetMatrix());
		m.d -= m.b * sm_fHomingTestDistance;

		atHashString projectileInfo = atHashString(sm_ProjectileInfo);
		atHashString weaponInfo = atHashString(sm_WeaponInfo);

		const CWeaponInfo *pInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>( weaponInfo );

		CProjectile *pProjectile = CProjectileManager::CreateProjectile(projectileInfo, 
			weaponInfo, 
			bScriptProjectile ? NULL : pPedTargetRemote, 
			m, 
			pInfo->GetDamage(), 
			pInfo->GetDamageType(), 
			pInfo->GetEffectGroup(),
			sm_bTargetVehicle ? (CEntity*)pPedTarget->GetVehiclePedInside() : (CEntity*)pPedTarget, 
			NULL,
			0,
			bScriptProjectile);

		if(pProjectile)
		{
			float fSpeed = pPedTarget->GetVehiclePedInside()->GetVelocity().Mag();
			pProjectile->Fire(m.b, sm_fLifeTimeOverride, fSpeed + sm_fSpeedOverride);
		}
	}
}
#endif	//__BANK
