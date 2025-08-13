//
// weapons/weapondamage.cpp
// 
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//
 
// File header
#include "Weapons/WeaponDamage.h" 

// Rage headers
#include "phglass/glassinstance.h"

// Game headers
#include "audio/northaudioengine.h"
#include "animation/MovePed.h"
#include "animation/FacialData.h"
#include "control/trafficlights.h"
#include "control/replay/replay.h"
#include "control/replay/Misc/GlassPackets.h"
#include "Event/EventDamage.h"
#include "Event/EventShocking.h"
#include "Event/EventWeapon.h"
#include "Event/ShockingEvents.h"
#include "modelinfo/PedModelInfo.h"
#include "network/Objects/Entities/NetObjPlayer.h"
#include "Objects/Door.h"
#include "Objects/DoorTuning.h"
#include "Peds/PedCapsule.h"
#include "Peds/PedIntelligence.h"
#include "Peds/Ped.h"
#include "Peds/rendering/PedDamage.h"
#include "Physics/GTAInst.h"
#include "Physics/Physics.h"
#include "Pickups/Pickup.h"
#include "Pickups/PickupManager.h"
#include "Renderer/PostProcessFX.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "Stats/StatsInterface.h"
#include "Stats/StatsMgr.h"
#include "Stats/StatsUtils.h"
#include "Task/Animation/TaskScriptedAnimation.h"
#include "Task/Combat/CombatMeleeDebug.h"
#include "Task/Combat/TaskAnimatedHitByExplosion.h"
#include "Task/Combat/TaskCombatMelee.h"
#include "Task/Combat/TaskDamageDeath.h"
#include "Task/Default/TaskIncapacitated.h"
#include "Task/Movement/Jumping/TaskFallGetUp.h"
#include "Task/Physics/TaskNM.h"
#include "Task/Physics/TaskNMBalance.h"
#include "Task/Physics/TaskNMElectrocute.h"
#include "Task/Physics/TaskNMExplosion.h"
#include "Task/Physics/TaskNMFallDown.h"
#include "Task/Physics/TaskNMFlinch.h"
#include "Task/Physics/TaskNMHighFall.h"
#include "Task/Physics/TaskNMOnFire.h"
#include "Task/Physics/TaskNMRelax.h"
#include "Task/Physics/TaskNMShot.h"
#include "Task/Physics/TaskNMSimple.h"
#include "Task/Scenario/Info/ScenarioInfo.h"
#include "Task/Scenario/Types/TaskUseScenario.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Task/Vehicle/TaskInVehicle.h"
#include "task/Vehicle/TaskMountAnimalWeapon.h"
#include "Vfx/Decals/DecalManager.h"
#include "Vfx/Misc/Fire.h"
#include "Vfx/Misc/LODLightManager.h"
#include "Vfx/Systems/VfxBlood.h"
#include "Vfx/Systems/VfxWeapon.h"
#include "vfx/vehicleglass/VehicleGlassManager.h"
#include "Vfx/VfxHelper.h"
#include "vehicles/heli.h"
#include "Vehicles/Bike.h"
#include "vehicles/Boat.h"
#include "vehicles/Trailer.h"
#include "Weapons/Projectiles/Projectile.h"
#include "Weapons/Weapon.h"
#include "fragment/cache.h"
#include "glassPaneSyncing/GlassPaneManager.h"
#include "Script/Script.h"

#if __ASSERT
#include "Stats/StatsTypes.h"
#endif

XPARAM(invincibleMigratingPeds);

// Macro to disable optimisations if set
WEAPON_OPTIMISATIONS()
NETWORK_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()
////////////////////////////////////////////////////////////////////////////////

// Constants
static dev_float MELEE_WEAPON_FORCE_MULT = 10.0f;
static bank_float GUN_WEAPON_CAR_DOOR_MOD = 0.066f;
static bank_float GUN_WEAPON_CAR_TURRET_MOD = 0.5f;

static dev_bool NM_APPLIES_SHOT_IMPULSE = true;  // Also need to comment out ApplyBulletImpulse message in TaskNMShot to disable

const float g_ExplosiveAmmoShakeAmplitude = 0.1f;
const float g_ExplosiveMeleeShakeAmplitude = 0.3f;

const float CWeaponDamage::MELEE_VEHICLE_DAMAGE_MODIFIER = 0.75f;

////////////////////////////////////////////////////////////////////////////////

f32 CWeaponDamage::DoWeaponImpact(CWeapon* pWeapon, CEntity* pFiringEntity, const Vector3& vWeaponPos, WorldProbe::CShapeTestResults& refResults, const f32 fApplyDamage, const fwFlags32& flags, const bool bFireDriveby /* = false */, const bool bTemporaryNetworkWeapon /* = false */, const sDamageModifiers* pModifiers /* = NULL */, const sMeleeInfo *pMeleeInfo /*= NULL*/, bool bulletUnderWater /* = false */, NetworkWeaponImpactInfo * const networkWeaponImpactInfo /* = NULL */, const float fRecoilAccuracyWhenFired /* = 1.f */, const float fFallOffRangeModifier /* = 1.f*/, const float fFallOffDamageModifier /* = 1.f*/, bool bReceiveFireMessage, bool bShouldGenerateVFX, Vector3* hitDirOut /* = 0 */,  const Vector3* hitDirIn /* = 0 */, u8 damageAggregationCount /*= 0*/)
{
	weaponDebugf3("CWeaponDamage::DoWeaponImpact fc[%u] pWeapon[%p] pFiringEntity[%p] vWeaponPos[%f %f %f] fApplyDamage[%f] bReceiveFireMessage[%d]",fwTimer::GetFrameCount(),pWeapon,pFiringEntity,vWeaponPos.x,vWeaponPos.y,vWeaponPos.z,fApplyDamage,bReceiveFireMessage);

	phMaterialMgr::Id lastMaterialForAudio = ~0U;
	u32 uNumImpactsForAudio = 0;

	f32 fDamageDone = 0.0f;
	f32 fDamageMod = 1.0f;

	CPed* pFiringPed = (pFiringEntity && pFiringEntity->GetIsTypePed()) ? static_cast<CPed*>(pFiringEntity) : NULL;
	if(!pFiringPed && pFiringEntity && pFiringEntity->GetIsTypeVehicle())
	{
		pFiringPed = static_cast<CVehicle*>(pFiringEntity)->GetDriver();
	}

	const CWeaponInfo* pWeaponInfo = pWeapon ? pWeapon->GetWeaponInfo() : NULL;
	if (pFiringPed && pFiringPed->IsPlayer() && pWeaponInfo)
	{
		const CPedModelInfo* mi = pFiringPed->GetPedModelInfo();
		Assert(mi);
		const CPedModelInfo::PersonalityData& pd = mi->GetPersonalitySettings();

		const u32 WeaponGroup = pWeaponInfo->GetGroup();
		if (WeaponGroup == WEAPONGROUP_PISTOL)
			fDamageMod = pd.GetHandGunDmgMod();
		else if (WeaponGroup == WEAPONGROUP_RIFLE)
			fDamageMod = pd.GetRifleDmgMod();
		else if (WeaponGroup == WEAPONGROUP_SMG)
			fDamageMod = pd.GetSmgDamageMod();
	}

	bool isUnderWater = bulletUnderWater;
	// Components may become invalid if the instance changes LOD during iteration
	// Store the high LOD component of instances prior to iterating below and then map to the current LOD during iteration
	for(WorldProbe::ResultIterator it = refResults.begin(); it < refResults.last_result(); ++it)
	{
		// Instances may get deleted because of ApplyImpact calls earlier in the loop
		// so we need to check they're still valid.
		if(!it->GetHitDetected() || !it->GetHitInst())
		{
			continue;
		}

		if(it->GetHitInst()->GetClassType() == PH_INST_FRAG_PED)
		{
			int highLODComponent = static_cast<fragInstNMGta*>(it->GetHitInst())->MapRagdollLODComponentCurrentToHigh(it->GetHitComponent());
			if(highLODComponent >= 0)
			{
				it->SetHitComponent(static_cast<u16>(highLODComponent));
			}
		}
	}

	for(WorldProbe::ResultIterator it = refResults.begin(); it < refResults.last_result(); ++it)
	{
		// Instances may get deleted because of ApplyImpact calls earlier in the loop
		// so we need to check they're still valid.
		if(!it->GetHitDetected() || !it->GetHitInst())
		{
			continue;
		}

		if(it->GetHitInst()->GetClassType() == PH_INST_FRAG_PED)
		{
			int currentLODComponent = static_cast<fragInstNMGta*>(it->GetHitInst())->MapRagdollLODComponentHighToCurrent(it->GetHitComponent());
			if(currentLODComponent >= 0)
			{
				it->SetHitComponent(static_cast<u16>(currentLODComponent));
			}
		}

		// Store the time position and time stamp of the first intersection	
		if(it == refResults.begin())
		{
			if(pFiringPed && pFiringPed->GetWeaponManager())
			{
				pFiringPed->GetWeaponManager()->SetLastWeaponImpactPos(it->GetHitPosition());
			}
		}

#if __BANK
		if(CExplosionManager::ms_debugWeaponExplosions && (!pFiringEntity || (pFiringEntity->GetIsTypePed() && static_cast<CPed*>(pFiringEntity)->IsLocalPlayer())))
		{
			CEntity* pHitEntity = CPhysics::GetEntityFromInst(it->GetHitInst());

			CExplosionManager::DebugCreateExplosion(pFiringEntity, it->GetHitPosition() + 0.1f * it->GetHitNormal(), it->GetHitNormal(), pHitEntity);
		}
#endif // __BANK

		bool bLocalFiringEntity = true;
		if ( NetworkInterface::IsGameInProgress() && pFiringEntity )
		{
			netObject* pNetObject = NetworkUtils::GetNetworkObjectFromEntity(pFiringEntity);
			if (pNetObject && pNetObject->IsClone())
			{
				bLocalFiringEntity = false;
			}
		}

		weaponDebugf3("CWeaponDamage::DoWeaponImpact--pWeaponInfo[%p] bLocalFiringEntity[%d]",pWeaponInfo,bLocalFiringEntity);

		// Get the hit entity
		CEntity* pHitEntity = CPhysics::GetEntityFromInst(it->GetHitInst());

		if( pWeaponInfo )
		{
			if(!flags.IsFlagSet( CPedDamageCalculator::DF_MeleeDamage ) && pWeapon->GetDamageType() == DAMAGE_TYPE_EXPLOSIVE)
			{
				eExplosionTag tag = pWeaponInfo->GetExplosionTag(pHitEntity);

				if(tag != EXP_TAG_DONTCARE)
				{
					// Don't explode if hit material is flagged to pass through (url:bugstar:2074338).
					if (!PGTAMATERIALMGR->GetMtlFlagShootThroughFx(it->GetHitMaterialId()))
					{
						CExplosionManager::CExplosionArgs explosionArgs(tag, it->GetHitPosition());

						explosionArgs.m_pEntExplosionOwner = pFiringEntity;
						explosionArgs.m_bInAir = false;
						explosionArgs.m_vDirection = it->GetHitNormal();
						explosionArgs.m_originalExplosionTag = tag;
						explosionArgs.m_weaponHash = pWeaponInfo->GetHash();
						explosionArgs.m_pAttachEntity = (pHitEntity && pHitEntity->GetDrawable()) ? const_cast<CEntity*>(pHitEntity) : NULL;

						if (pFiringEntity && pWeaponInfo->GetFiringEntityIgnoresExplosionDamage())
						{
							CEntity* pIgnoreEntity = pFiringEntity;						
							if (pWeaponInfo->GetIsVehicleWeapon() && !pFiringEntity->GetIsTypeVehicle() && pFiringPed && pFiringPed->GetMyVehicle())
							{
								pIgnoreEntity = pFiringPed->GetMyVehicle();
							}

							explosionArgs.m_pEntIgnoreDamage = pIgnoreEntity;
						}

						// Check if a custom explosion shake amplitude is defined for this weapon
						const float shakeAmplitude = pWeaponInfo->GetExplosionShakeAmplitude();
						if(shakeAmplitude >= 0.0f)
						{
							explosionArgs.m_fCamShake = shakeAmplitude;
						}

						if(!bLocalFiringEntity || CExplosionManager::AddExplosion(explosionArgs))
						{
							return 0.0f;
						}
					}
				}
			}
			else if ( pFiringPed && pFiringPed->IsPlayer() )
			{
				const CAmmoInfo* pAmmoInfo = pWeaponInfo->GetAmmoInfo(pFiringPed);
				if (pAmmoInfo && pAmmoInfo->GetIsExplosive() && !flags.IsFlagSet(CPedDamageCalculator::DF_MeleeDamage))
				{
					const bool bIsShotgun = pWeaponInfo->GetGroup() == WEAPONGROUP_SHOTGUN;
					eExplosionTag tag = bIsShotgun ? EXP_TAG_EXPLOSIVEAMMO_SHOTGUN : EXP_TAG_EXPLOSIVEAMMO;

					CExplosionManager::CExplosionArgs explosionArgs(tag, it->GetHitPosition());
					explosionArgs.m_pEntExplosionOwner = pFiringEntity;
					explosionArgs.m_bInAir = true;
					explosionArgs.m_vDirection = it->GetHitNormal();
					explosionArgs.m_originalExplosionTag = tag;
					explosionArgs.m_fCamShake = g_ExplosiveAmmoShakeAmplitude;
					explosionArgs.m_weaponHash = WEAPONTYPE_EXPLOSION;

					if (pHitEntity && (pHitEntity->GetIsTypeVehicle() || pHitEntity->GetIsTypePed()))
					{
						explosionArgs.m_pAttachEntity = pHitEntity;

						if(pHitEntity->GetIsTypeVehicle())
						{	//Don't use a delay when syncing explosion attached to 
							//vehicle as this requires immediate syncing to ensure
							//coordination with any other syncing of vehicle 
							//parts like doors opening etc.
							explosionArgs.m_activationDelay = 0;
						}
					}

					if(!bLocalFiringEntity || CExplosionManager::AddExplosion(explosionArgs))
					{
						return 0.0f;
					}
				}
				// CHEATS - BANG BANG
				else if( pWeaponInfo->GetDamageType() == DAMAGE_TYPE_BULLET && !flags.IsFlagSet( CPedDamageCalculator::DF_MeleeDamage ) &&
					pFiringPed->GetPlayerInfo()->GetPlayerResetFlags().IsFlagSet(CPlayerResetFlags::PRF_EXPLOSIVE_AMMO_ON) )
				{
					eExplosionTag tag = EXP_TAG_BULLET;
					
					CExplosionManager::CExplosionArgs explosionArgs(tag, it->GetHitPosition());
					explosionArgs.m_pEntExplosionOwner = pFiringEntity;
					explosionArgs.m_bInAir = false;
					explosionArgs.m_vDirection = it->GetHitNormal();
					explosionArgs.m_originalExplosionTag = tag;
					explosionArgs.m_fCamShake = g_ExplosiveAmmoShakeAmplitude;
					explosionArgs.m_weaponHash = WEAPONTYPE_EXPLOSION;

					if(!bLocalFiringEntity || CExplosionManager::AddExplosion(explosionArgs))
					{
						return 0.0f;
					}
				}
				else if (flags.IsFlagSet( CPedDamageCalculator::DF_MeleeDamage ) && 
						 pFiringPed->GetPlayerInfo()->GetPlayerResetFlags().IsFlagSet(CPlayerResetFlags::PRF_EXPLOSIVE_MELEE_ON))
				{
					if(pHitEntity && (pHitEntity->GetIsTypePed() || pHitEntity->GetIsTypeObject() || pHitEntity->GetIsTypeVehicle()))
					{
						eExplosionTag tag = EXP_TAG_BULLET;

						CExplosionManager::CExplosionArgs explosionArgs(tag, it->GetHitPosition());
						explosionArgs.m_pEntExplosionOwner = pFiringEntity;
						explosionArgs.m_bInAir = true;
						explosionArgs.m_vDirection = VEC3V_TO_VECTOR3(pFiringPed->GetTransform().GetForward());
						explosionArgs.m_originalExplosionTag = tag;
						explosionArgs.m_bDisableDamagingOwner = true;
						explosionArgs.m_fCamShake = g_ExplosiveMeleeShakeAmplitude;
						explosionArgs.m_weaponHash = WEAPONTYPE_EXPLOSION;

						if(!bLocalFiringEntity || CExplosionManager::AddExplosion(explosionArgs))
						{
							return 0.0f;
						}
					}
				}
			}
		}

		const phGlassInst* pGlassInst = NULL;
		if(it->GetHitInst()->GetClassType() == phInst::PH_INST_GLASS)
		{
			pGlassInst = static_cast<const phGlassInst*>(it->GetHitInst());
#if BREAKABLE_GLASS_USE_BVH
			if(!pGlassInst->GetIsElementActive(it->GetHitPartIndex()))
#else
			if(!pGlassInst->GetIsHitActive(it->GetHitPosition()))
#endif // BREAKABLE_GLASS_USE_BVH
			{
				continue;
			}
		}
		
		// Check to see if this is a clone parachute. If it is, don't apply impacts to it locally (netblender + physics can mess it up)
		CObject* pObject = static_cast<CObject*>(pHitEntity);		
		if(pHitEntity && pHitEntity->GetIsTypeObject() && pObject->GetIsParachute() && pObject->IsNetworkClone())
		{
			continue;
		}
		
		// Don't let anything shoot itself
		if(pHitEntity && pHitEntity == pFiringEntity)
		{
			weaponAssertf(0, "Something shot itself");
			return 0.0f;
		}

		if (NetworkInterface::IsGameInProgress() && pHitEntity)
		{
			bool bIsFriendlyFireAllowed = true;
			if (pHitEntity->GetIsTypePed())
				bIsFriendlyFireAllowed = NetworkInterface::IsFriendlyFireAllowed((const CPed*) pHitEntity, pFiringEntity);
			else if (pHitEntity->GetIsTypeVehicle())
				bIsFriendlyFireAllowed = NetworkInterface::IsFriendlyFireAllowed((const CVehicle*) pHitEntity, pFiringEntity);
				

			bool bFirerInSameVehicleAsHitEntity = false;
			CVehicle* pVehicle = NULL;

			if (pHitEntity->GetIsTypeVehicle())
			{
				pVehicle = static_cast<CVehicle*>(pHitEntity);
				if (pVehicle)
				{
					if (pFiringPed && pFiringPed->GetIsInVehicle() && (pFiringPed->GetMyVehicle() == pVehicle))
						bFirerInSameVehicleAsHitEntity = true;
				}
			}

			bool bIsInFirstPerson = CPedFactory::GetFactory()->GetLocalPlayer()->IsInFirstPersonVehicleCamera();
			bool bGlassHit = PGTAMATERIALMGR->GetIsSmashableGlass(it->GetHitMaterialId());
			
			// Only display vfx, no damage/smashing/decals, except if we're hitting windows from inside in 1st person.
			if (!bIsFriendlyFireAllowed && !(bFirerInSameVehicleAsHitEntity && bIsInFirstPerson && bGlassHit))
			{
				if (pHitEntity->GetIsTypeVehicle())
				{
					DoWeaponImpactVfx(pWeapon, pFiringEntity, vWeaponPos, &(*it), 0.0f, flags|CPedDamageCalculator::DF_PtFxOnly, false, false, bTemporaryNetworkWeapon);
				}

				return 0.0f;
			}

			if (bFirerInSameVehicleAsHitEntity)
				if (!pVehicle->GetVehicleDamage()->CanVehicleBeDamagedBasedOnDriverInvincibility())
					return 0.0f;

			//----------------------------------------------------------------------
			//Prevent double application of damage on the machine that owns the ped.
			//Ensure that the damage comes in from the remote killer through the receivefiremessage and that is processed and the doweaponimpact from the bullet process damage is ignored.
			//Skip the processing here if the hitped is local, the firing entity is remote, and this method wasn't called from receivefiremessage.
			//Allow it otherwise, local both, remote hitped, etc...
			//
			//This will fix issues where shotgun blasts were sending the local player victim flying. B* 1444873. lavalley
			//
			//If we are firing at a non-networked entity such as a vending machine don't do this processing. B* 1692001. lavalley (added pHitEntity network object check to if below)
			if (!bFirerInSameVehicleAsHitEntity && !bReceiveFireMessage && !flags.IsFlagSet( CPedDamageCalculator::DF_MeleeDamage ) && NetworkUtils::GetNetworkObjectFromEntity(pHitEntity))
			{
				const bool bHitEntityLocal    = !NetworkUtils::GetNetworkObjectFromEntity(pHitEntity) || !NetworkUtils::IsNetworkClone(pHitEntity);
				const bool bFiringEntityLocal = !NetworkUtils::GetNetworkObjectFromEntity(pFiringEntity) || !NetworkUtils::IsNetworkClone(pFiringEntity);


				if (!bHitEntityLocal && !bFiringEntityLocal)
				{
					weaponDebugf3("FiringEntity(%s) - HitEntity(%s)", pFiringEntity ? pFiringEntity->GetModelName() : "", pHitEntity ? pHitEntity->GetModelName() : "");
					if (pHitEntity->GetIsTypePed())
					{
						CPed *pHitPed =  static_cast<CPed*> (pHitEntity);
						WorldProbe::CShapeTestHitPoint* pResult = &(*it);
						if (pResult && !pHitPed->GetPedResetFlag(CPED_RESET_FLAG_BlockIkWeaponReactions))
						{
							Vec3V vPosition(VECTOR3_TO_VEC3V(pResult->GetHitPosition()));
							Vec3V vDirection(NormalizeFast(Subtract(vPosition, VECTOR3_TO_VEC3V(vWeaponPos))));
							CIkRequestBodyReact bodyReactRequest(vPosition, vDirection, (int)pResult->GetHitComponent());
							bodyReactRequest.SetWeaponGroup(pWeaponInfo->GetGroup());
							bodyReactRequest.SetLocalInflictor(false);
							pHitPed->GetIkManager().Request(bodyReactRequest);
						}

					}
				}

				if (bHitEntityLocal && !bFiringEntityLocal)
				{
					weaponDebugf3("bHitEntityLocal && !bFiringEntityLocal --> return 0.0f");
					return 0.0f;
				}
			}
			//----------------------------------------------------------------------
		}

#if __ASSERT
		f32 fNormalMag = it->GetHitNormal().Mag();
		weaponAssertf(fNormalMag >= 0.5f && fNormalMag <= 1.5f, "Weapon impact normal not normalised %.4f (%.2f, %.2f, %.2f) %s", fNormalMag, it->GetHitNormal().x, it->GetHitNormal().y, it->GetHitNormal().z, pHitEntity ? pHitEntity->GetModelName() : "");
#endif // __ASSERT

		if(pFiringPed)
		{
			CPed* pHitPed = (pHitEntity && pHitEntity->GetIsTypePed()) ? static_cast<CPed*>(pHitEntity) : NULL;
			if(!pHitPed)
			{
				pFiringPed->SetPedResetFlag( CPED_RESET_FLAG_MeleeStrikeAgainstNonPed, true );

				if(pHitEntity && pHitEntity->GetIsTypeObject())
				{
					CObject* pHitObj = static_cast<CObject*>(pHitEntity);
					CEntity* pHitAttachParent = (CPhysical *) pHitObj->GetAttachParent();
					if(pHitAttachParent && pHitAttachParent->GetIsTypePed())
					{
						pHitPed = static_cast<CPed*>(pHitAttachParent);
					}
				}
			}

			// Don't let AI ped's that are friends shoot each other
			bool bIsHitPedFriendly = pHitPed && pFiringPed->GetPedIntelligence()->IsFriendlyWith( *pHitPed );
			if(!pFiringPed->IsPlayer() && bIsHitPedFriendly)
			{
				continue;
			}
		}

		// Check if a ped within range has been hit in this shot
		bool bHeadShotNearby = false;
		if(fApplyDamage > 0.0f && pWeaponInfo && pWeaponInfo->GetIsGun() && pWeaponInfo->GetDamageType() != DAMAGE_TYPE_ELECTRIC)
		{
			for(WorldProbe::ResultIterator it2 = refResults.begin(); it2 < refResults.last_result(); ++it2)
			{
				if(it2 != it && PGTAMATERIALMGR->GetIsPed(it2->GetHitMaterialId()))
				{
					Vector3 v = it2->GetHitPosition() - it->GetHitPosition();
					if(v.Mag2() < 1.0f)
					{
						bHeadShotNearby = true;
					}
				}
			}
		}

		// Damage
		float fDamage = fApplyDamage * fDamageMod;
		//B*1783494: Scale harpoon damage if underwater or has been shot in the head/torso
		if (pWeaponInfo->GetIsUnderwaterGun() && pHitEntity && pHitEntity->GetIsTypePed())
		{
			CPed *pTargetPed = static_cast<CPed*>(pHitEntity);
			if (pTargetPed)
			{
				static dev_float fDamageMultiplier = 5.0f;
				s32 iHitComponent = it->GetHitComponent();
				s32 iHitBoneTag = pTargetPed->GetBoneTagFromRagdollComponent(iHitComponent);
				bool bHitHeadOrTorso = iHitBoneTag == BONETAG_NECK || iHitBoneTag == BONETAG_HEAD || iHitBoneTag == BONETAG_NECKROLL || iHitBoneTag == BONETAG_ROOT	
					|| iHitBoneTag == BONETAG_SPINE0 || iHitBoneTag == BONETAG_SPINE1 || iHitBoneTag == BONETAG_SPINE2 || iHitBoneTag == BONETAG_SPINE3;

				if (pTargetPed->GetIsSwimming() || bHitHeadOrTorso)
				{
					fDamage *= fDamageMultiplier;
				}
			}
		}
		// Apply falloff scaling (do not apply for melee weapons)
		if(!pFiringEntity || !pFiringEntity->GetIsDynamic() || !static_cast<CDynamicEntity*>(pFiringEntity)->IsNetworkClone())
		{
			fDamage = ApplyFallOffDamageModifier(pHitEntity, pWeaponInfo, vWeaponPos, fFallOffRangeModifier, fFallOffDamageModifier, fDamage, pFiringEntity);
		}
		if(pHitEntity && pHitEntity->GetIsTypePed())
		{
			f32 fPedDamage = fDamage;
			if(pModifiers)
			{
				fPedDamage *= pModifiers->fPedDamageMult;
			}

			bool bGenerateVFX = bShouldGenerateVFX;

			fDamageDone += DoWeaponImpactPed(pWeapon, pFiringEntity, vWeaponPos, &(*it), fPedDamage, bTemporaryNetworkWeapon, flags, pMeleeInfo, fRecoilAccuracyWhenFired, &bGenerateVFX, damageAggregationCount);

			// For peds this needs to be done AFTER DoWeaponImpactPed in case the ped's ragdoll was activated
			if (bGenerateVFX)
			{
				DoWeaponImpactVfx(pWeapon, pFiringEntity, vWeaponPos, &(*it), fPedDamage, flags, bHeadShotNearby, false, bTemporaryNetworkWeapon);
			}
		}
		else
		{
			bool bShouldApplyDamageToGlass = true;

			if( pWeaponInfo )
			{
				// Set Breakable Glass Crack Id.
				s32 crackId = g_vfxWeapon.GetBreakableGlassId(pWeaponInfo->GetEffectGroup());
				CPhysics::SetSelectedCrack(crackId);

				// Bullet Resistant Glass: Don't smash/damage glass when hit with a melee weapon.
				if (pWeaponInfo->GetIsMelee() && pHitEntity->GetIsTypeVehicle())
				{
					CVehicle *pVehicle = static_cast<CVehicle*>(pHitEntity);
					if (pVehicle && pVehicle->HasBulletResistantGlass())
					{
						bShouldApplyDamageToGlass = false;
					}
				}
			}

			// Add a broken glass event (in mp only)- not just when the glass fragments, but whenever you hit glass.
			if (NetworkInterface::IsGameInProgress() && pFiringEntity && bShouldApplyDamageToGlass)
			{
				bool bIsGlassForAIPurposes = PGTAMATERIALMGR->GetIsGlass(it->GetHitMaterialId());
				if (bIsGlassForAIPurposes)
				{
					CEventShockingBrokenGlass event(*pFiringEntity, it->GetHitPositionV());
					CShockingEventsManager::Add(event);
				}
			}

			// This needs done before any fragments have broken off
			DoWeaponImpactVfx(pWeapon, pFiringEntity, vWeaponPos, &(*it), fDamage, flags, bHeadShotNearby, false, bTemporaryNetworkWeapon, bShouldApplyDamageToGlass);

			if(pGlassInst)
			{
				fDamageDone += DoWeaponImpactBreakableGlass(pWeapon, pFiringEntity, vWeaponPos, &(*it), fDamage);
			}
			else
			{
				if(pHitEntity)
				{
					f32 fDoDamage = fDamage;

					if(pHitEntity->GetIsTypeVehicle())
					{
						if(pModifiers)
						{
							fDoDamage *= pModifiers->fVehDamageMult;
						}

						// Include the firing ped damage modifier as defined by script
						// Only want to do this for local players here, remote players apply the modifier through CWeapon::SendFireMessage
						if (pFiringPed && pFiringPed->IsLocalPlayer() && pWeaponInfo->GetDamageType() == DAMAGE_TYPE_MELEE)
						{
							CPlayerInfo* pPlayerInfo = pFiringPed->GetPlayerInfo();
							if (pPlayerInfo)
							{
								fDoDamage *= pPlayerInfo->GetPlayerMeleeDamageModifier(pWeaponInfo->GetIsUnarmed());
							}
						}

						fDamageDone += DoWeaponImpactCar(pWeapon, pFiringEntity, vWeaponPos, &(*it), fDoDamage, bFireDriveby, bTemporaryNetworkWeapon, flags, hitDirOut, hitDirIn, bShouldApplyDamageToGlass);
					}
					else if(pHitEntity->GetIsTypeObject())
					{
// NOTE: pure cloth ( there are no cloth and non-cloth parts in the same frag) impact is not processed here anymore
// see CBullet::ComputeImpacts
// Svetli
						fragInst* pFragInst = pHitEntity->GetFragInst();
						bool bOnlyCloth = false;
						if( pFragInst && pFragInst->GetType() && (pFragInst->GetType()->GetNumEnvCloths() > 0) )
						{
							bOnlyCloth = (NULL == pFragInst->GetType()->GetClothDrawable()) ;
						}

						if( !bOnlyCloth )
						{
							if( pWeaponInfo && pWeaponInfo->GetIsGun())
							{
								pHitEntity->ProcessFxEntityShot(it->GetHitPosition(), it->GetHitNormal(), it->GetHitComponent(), pWeaponInfo, pFiringEntity);
							}

							if(pModifiers)
							{
								fDoDamage *= pModifiers->fObjDamageMult;
							}

							fDamageDone += DoWeaponImpactObject(pWeapon, pFiringEntity, vWeaponPos, &(*it), fDoDamage, bTemporaryNetworkWeapon, flags, it->GetHitMaterialId());

							if(it->GetHitInst() && it->GetHitInst()->IsInLevel() && (CPhysics::GetLevel()->GetInstanceTypeFlags(it->GetHitInst()->GetLevelIndex()) & ArchetypeFlags::GTA_PROJECTILE_TYPE))
							{
								fDamageDone += DoWeaponImpactWeapon(pWeapon, pFiringEntity, vWeaponPos, &(*it), fDamage, bTemporaryNetworkWeapon, flags, 0, networkWeaponImpactInfo, fRecoilAccuracyWhenFired, damageAggregationCount);
							}
						}
					}
					else
					{
						pHitEntity->ProcessFxEntityShot(it->GetHitPosition(), it->GetHitNormal(), it->GetHitComponent(), pWeaponInfo, pFiringEntity);
						fDamageDone += fDamage;
					}

					// Do not do perform recoils on broken frag instances
					bool bIsBroken = false;

					// Analyze to see if the next hit will break this frag
					if( IsFragInst( it->GetHitInst() ) )
					{
						const fragInst* pFragInstance = static_cast<const fragInst*>( it->GetHitInst() );
						bIsBroken = pFragInstance->GetChildBroken( it->GetHitComponent() );
					}

					// Recoil direction threshold has been moved earlier to avoid sfx/vfx on surfaces we throw out
					if( pFiringPed && flags.IsFlagSet( CPedDamageCalculator::DF_MeleeDamage ) && !bIsBroken)
					{
						// This should always be true
						CTaskMelee* pFiringPedMeleeTask = pFiringPed->GetPedIntelligence()->GetTaskMelee();
						if( pFiringPedMeleeTask )
						{
							CTaskMeleeActionResult* pMeleeTaskActionResult = pFiringPed->GetPedIntelligence()->GetTaskMeleeActionResult();
							const CActionResult* pActionResult = pMeleeTaskActionResult ? pMeleeTaskActionResult->GetActionResult() : NULL;
							if( pActionResult && pActionResult->GetIsValidForRecoil() )
							{
								// Check to see if we can find an appropriate hit reaction that is available to do.
								CActionFlags::ActionTypeBitSetData actionTypeBitSet;
								actionTypeBitSet.Set( CActionFlags::AT_RECOIL, true );
								const CActionDefinition* pMeleeActionToDo = ACTIONMGR.SelectSuitableAction(
										actionTypeBitSet,// The type we want to search in.
										NULL, // No hit combo built up
										true,// Test whether or not it is an entry action.
										true,// Only find entry actions.
										pFiringPed,
										pActionResult->GetID(),
										pActionResult->GetPriority(),
										pFiringPed,
										false,
										false,
										false,
										CActionManager::ShouldDebugPed( pFiringPed ) );

								const CActionResult* pActionResultToForce = pMeleeActionToDo ? pMeleeActionToDo->GetActionResult( pFiringPed ) : NULL;
								if( pActionResultToForce )
								{
									pFiringPedMeleeTask->SetLocalReaction( pActionResultToForce, true, pMeleeInfo ? pMeleeInfo->uNetworkActionID : 0 );
								}
							}
						}
					}
				}
			}
		}

		// Do player shooting specific code
		if(pFiringPed && pFiringPed->IsPlayer())
		{
			if(!pFiringPed->IsNetworkClone())
			{
				DoWeaponFiredByPlayer(pWeapon, pFiringPed, vWeaponPos, &(*it), fDamageDone, flags);
			}
		}

		// AI
		DoWeaponImpactAI(pWeapon, pFiringPed, vWeaponPos, &(*it), fDamageDone);

		bool justHitWater = false;
		if(!isUnderWater)
		{
			if(it->GetHitInst() && it->GetHitInst()->IsInLevel() && (CPhysics::GetLevel()->GetInstanceTypeFlags(it->GetHitInst()->GetLevelIndex()) & ArchetypeFlags::GTA_RIVER_TYPE))
			{
				isUnderWater = true;
				justHitWater = true;
			}
		}
		f32 cameraDepth = Water::GetCameraWaterDepth();
		// Trigger a max of two impact sounds, and only if they are on different surface materials
		if( !flags.IsFlagSet( CPedDamageCalculator::DF_SuppressImpactAudio ) && it->GetHitMaterialId() != lastMaterialForAudio && uNumImpactsForAudio < 2 && (!isUnderWater || ( cameraDepth > 0 || (isUnderWater && justHitWater))))
		{
			DoWeaponImpactAudio(pWeapon, pFiringEntity, vWeaponPos, &(*it), fDamageDone, flags );
			lastMaterialForAudio = it->GetHitMaterialId();
			uNumImpactsForAudio++;
		}
	}

	return fDamageDone;
}

////////////////////////////////////////////////////////////////////////////////

f32 CWeaponDamage::GeneratePedDamageEvent(CEntity* pFiringEntity, CPed* pHitPed, const u32 uWeaponHash, const f32 fWeaponDamage, const Vector3& vStart, WorldProbe::CShapeTestHitPoint* pResult, const fwFlags32& flags, const CWeapon* UNUSED_PARAM(pWeapon), const s32 iHitComponent, f32 fForce, const sMeleeInfo *pMeleeInfo, bool bDeferRagdollActivation, const float fRecoilAccuracyWhenFired, bool bTemporaryNetworkWeapon, u8 damageAggregationCount)
{
	weaponDebugf3("CWeaponDamage::GeneratePedDamageEvent");

	// Get the weapon info
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(uWeaponHash);
	if(!pWeaponInfo)
	{
		return 0.0f;
	}

	if(!pHitPed)
	{
		return 0.0f;
	}

	s32 iHitBoneTag = 0;
	f32 fHitDir = 0.0f;

	// Extract hit information from the intersection data.
	if(pResult)
	{
		iHitBoneTag = pHitPed->GetBoneTagFromRagdollComponent(pResult->GetHitComponent());
		Vector3 vTempDir = vStart - pResult->GetHitPosition();
		fHitDir =  rage::Atan2f(-vTempDir.x, vTempDir.y);
		fHitDir -= pHitPed->GetTransform().GetHeading();
		fHitDir =  fwAngle::LimitRadianAngle(fHitDir);
	}

	// Optional ragdoll reactions.
	bool bUseRagdollEvent = false;
	bool bUseRagdollReaction = false;
	bool bUseBodyIkReaction = false;
	Vector3	vRagdollImpulseDir(Vector3::ZeroType);
	f32 fRagdollImpulseMag = 0.0f;

	// An additional force that can be applied, if the ped is rag dolled
	// Used to throw peds out of vehicles
	Vector3 vAdditionalRagdollForce(Vector3::ZeroType);

	// Todo: add to metadata
	// 	if( pHitPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pHitPed->GetMyVehicle() )
	// 	{
	// 		// BAD_SEAT_USE
	// 		s32 seat = pHitPed->GetMyVehicle()->GetPedsSeatIndex(pHitPed);
	// 		// Extra force out of the sides of the heli
	// 		if( pHitPed->GetMyVehicle()->GetVehicleType() == VEHICLE_TYPE_HELI &&  !pHitPed->GetMyVehicle()->IsSeatAFrontSeat(seat) )
	// 		{
	// 			vAdditionalRagdollForce = pHitPed->GetMass()*sfAdditionalVehicleExitForceScale*pHitPed->GetB();
	// 		}
	// 		// Extra force out of the back of the van, only if the doors are open
	// 		else if( pHitPed->GetMyVehicle()->GetVehicleModelInfo()->GetFlag(FLAG_IS_VAN) && !pHitPed->GetMyVehicle()->IsSeatAFrontSeat(seat) )
	// 		{
	// 			eHierarchyId iDoor = pHitPed->GetMyVehicle()->GetEntryDoorForSeat(seat);
	// 			CCarDoor* pDoor = pHitPed->GetMyVehicle()->GetDoorFromId(iDoor);
	// 			if( pDoor )
	// 				pDoor->SetTargetDoorOpenRatio(1.0f, CCarDoor::DRIVEN_AUTORESET);
	// 				
	// 			vAdditionalRagdollForce = pHitPed->GetMass()*sfAdditionalVehicleExitForceScale*-pHitPed->GetMyVehicle()->GetB();
	// 		}
	// 	}

	// Calculate the distance to the ped we have hit
	float fDist = FLT_MAX;
	if(pResult)
	{
		fDist = vStart.Dist(pResult->GetHitPosition());
		weaponDebugf3("vStart[%f %f %f] GetHitPosition[%f %f %f] fDist[%f]",vStart.x,vStart.y,vStart.z,pResult->GetHitPosition().x,pResult->GetHitPosition().y,pResult->GetHitPosition().x,fDist);
	}
	else if(pHitPed)
	{
		Vector3 vHitPedPosition = VEC3V_TO_VECTOR3(pHitPed->GetTransform().GetPosition());
		fDist = vStart.Dist(vHitPedPosition);
		weaponDebugf3("vStart[%f %f %f] pHitPed[%f %f %f] fDist[%f]",vStart.x,vStart.y,vStart.z,vHitPedPosition.x,vHitPedPosition.y,vHitPedPosition.z,fDist);
	}
	else
	{
		fDist = 0.f;
		weaponDebugf3("else fDist = 0.f");
	}

	fwFlags32 modifiedFlags = flags;
	if(!modifiedFlags.IsFlagSet(CPedDamageCalculator::DF_AllowHeadShot))
	{
		if(pWeaponInfo->GetDoesRecoilAccuracyAllowHeadShotModifier(pHitPed, fRecoilAccuracyWhenFired, fDist))
		{
			modifiedFlags.SetFlag(CPedDamageCalculator::DF_AllowHeadShot);
		}
	}

	// Actually calculate and apply the damage to peds health/armour and then
	// check if the damage calculator thought the damage should affect the
	// ped in other ways as well.
	bool bAllowTempDamageEventToBeAdded = true;
	CEventDamage tempDamageEvent(pFiringEntity, fwTimer::GetTimeInMilliseconds(), uWeaponHash);
	if(flags.IsFlagSet(CPedDamageCalculator::DF_DontReportCrimes))
	{
		tempDamageEvent.SetCanReportCrimes(false);
	}

	u32 uParentMeleeActionResultID = pMeleeInfo ? pMeleeInfo->uParentMeleeActionResultID : 0;

	CPedDamageCalculator damageCalculator(pFiringEntity, fWeaponDamage, uWeaponHash, iHitComponent, false, damageAggregationCount);
	weaponDebugf3("invoke ApplyDamageAndComputeResponse hitComponent[%d]",iHitComponent);
	damageCalculator.ApplyDamageAndComputeResponse(pHitPed, tempDamageEvent.GetDamageResponseData(), modifiedFlags, uParentMeleeActionResultID, fDist);

	// Flag an event with the firing entity (if it's a ped) that it's hit another ped with weapon damage
	if (pFiringEntity && pFiringEntity->GetIsTypePed() && !pHitPed->IsDead() && pHitPed != pFiringEntity)
	{
		CPed *pFiringEntityPed = static_cast<CPed *>(pFiringEntity);

		// only display hit markers when we are not within the same group
		if( !pFiringEntityPed->GetPedIntelligence()->IsFriendlyWith(*pHitPed) )
		{
			pFiringEntityPed->SetPedResetFlag(CPED_RESET_FLAG_HitPedWithWeapon, true);

			// Change AI hit ped target if previously targeting another ped
			if( flags.IsFlagSet( CPedDamageCalculator::DF_MeleeDamage ) && !pHitPed->IsAPlayerPed() )
			{
				CTaskMelee* pOpponentTaskMelee = pHitPed->GetPedIntelligence() ? pHitPed->GetPedIntelligence()->GetTaskMelee() : NULL;
				if( pOpponentTaskMelee && ( pHitPed->GetPedConfigFlag( CPED_CONFIG_FLAG_CanAttackFriendly ) || !CActionManager::ArePedsFriendlyWithEachOther( pFiringEntityPed, pHitPed ) ) )
					pOpponentTaskMelee->SetTargetEntity( pFiringEntity, true );	
			}
		}
		// If you're in freemode, and you're shooting a player ped, force the icon on.
		else if(fWeaponDamage > 0.0f &&
			NetworkInterface::IsNetworkOpen() && NetworkInterface::FriendlyFireAllowed())
		{
			pFiringEntityPed->SetPedResetFlag(CPED_RESET_FLAG_HitPedWithWeapon, true);
		}
	}

	// don't do nm reaction behaviours if the ped has specifically asked to not ragdoll untill dead
	if (pHitPed->GetPedResetFlag(CPED_RESET_FLAG_BlockWeaponReactionsUnlessDead) && !pHitPed->ShouldBeDead())
	{
		return 0.0f;
	}

	// Collect information from melee combat if available.
	// Note: Pistol whips and gun butts can cause melee reactions, so don't
	// filter this code based on weapon type...
	// Note: This possibly sets bUseRagdollReaction and the vRagdollImpulse.
	bool bIsStandardMeleeReaction = false;
	bool bTriggerDrowning = false;

	if(modifiedFlags.IsFlagSet(CPedDamageCalculator::DF_MeleeDamage))
	{
		tempDamageEvent.SetMeleeDamage(true);
	}

	const CActionDefinition* pMeleeActionToDo = NULL;
	if(uWeaponHash != 0 &&
		pFiringEntity &&
		pFiringEntity->GetIsTypePed() &&
		(modifiedFlags.IsFlagSet(CPedDamageCalculator::DF_MeleeDamage) ||
		  modifiedFlags.IsFlagSet(CPedDamageCalculator::DF_SelfDamage)))
	{
		CPed* pFiringPed = static_cast<CPed*>( pFiringEntity );

		if( pResult )
		{
			if (!pHitPed->GetPedResetFlag(CPED_RESET_FLAG_WasHitByVehicleMelee))
				CTaskMeleeActionResult::HitImpulseCalculation( pResult, -1.0f, vRagdollImpulseDir, fRagdollImpulseMag, pHitPed );
			else
				CTaskMountThrowProjectile::HitImpulseCalculation( vRagdollImpulseDir, fRagdollImpulseMag, *pHitPed, *pFiringPed );
		}

		CTaskMeleeActionResult* pPedMeleeActionResult = pFiringPed->GetPedIntelligence() ? pFiringPed->GetPedIntelligence()->GetTaskMeleeActionResult() : NULL;
		if( pPedMeleeActionResult )
			pPedMeleeActionResult->ProcessPostHitResults( pFiringPed, pHitPed, false, VECTOR3_TO_VEC3V( vRagdollImpulseDir ), fRagdollImpulseMag );

		// Check if a simple melee action task is playing on the damage parent.
		// And only try to collect the rest of the melee data if it is a melee reaction.
		bIsStandardMeleeReaction = true;
		bool bDoStandardMeleeReactionCheck = true;

		//! If we don't see ped doing melee task, don't do an animated reaction - it'll probably look a bit weird. This should be pretty 
		//! rare.
		CTaskMeleeActionResult* pFiringPedMeleeActionResultTask = pFiringPed->GetPedIntelligence()->GetTaskMeleeActionResult();

		static dev_bool s_bDoAnimatedReactsIfNotRunningMeleeTask = true;
		if(!s_bDoAnimatedReactsIfNotRunningMeleeTask)
		{
			if(bTemporaryNetworkWeapon)
			{
				u16 meleeID = pMeleeInfo ? pMeleeInfo->uNetworkActionID : 0;

				if(!pFiringPedMeleeActionResultTask || (pFiringPedMeleeActionResultTask->GetUniqueNetworkActionID() != meleeID) )
				{
					bDoStandardMeleeReactionCheck = false;
				}
			}
		
			if(flags.IsFlagSet(CPedDamageCalculator::DF_NoAnimatedMeleeReaction))
			{
				bDoStandardMeleeReactionCheck = false;
			}
		}

		// Determine if we should do a standard check to find a melee reaction to do.
		if(modifiedFlags.IsFlagSet(CPedDamageCalculator::DF_SelfDamage) && tempDamageEvent.GetDamageResponseData().m_bKilled && CTaskNMBehaviour::CanUseRagdoll(pHitPed, RAGDOLL_TRIGGER_MELEE, pFiringEntity, 0.0f))
		{
			bDoStandardMeleeReactionCheck = false;
			bUseRagdollReaction = true;
		}
		else if(pHitPed->GetPedConfigFlag( CPED_CONFIG_FLAG_IsHandCuffed ) || 
				 pHitPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) ||
#if ENABLE_DRUNK
				 pHitPed->GetPedConfigFlag( CPED_CONFIG_FLAG_IsDrunk ) ||
#endif // ENABLE_DRUNK
				 pHitPed->GetIsInCover() ||
				 pHitPed->GetPedResetFlag( CPED_RESET_FLAG_IsFalling ) ||
				 pHitPed->GetPedResetFlag( CPED_RESET_FLAG_IsJumping ) ||
				 pHitPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning( CTaskTypes::TASK_ENTER_VEHICLE_SEAT ) )
		{
			if( !pHitPed->IsNetworkClone() && CTaskNMBehaviour::CanUseRagdoll( pHitPed, RAGDOLL_TRIGGER_MELEE, pFiringEntity, 0.0f ) )
				bUseRagdollReaction = true;
			else
				bUseBodyIkReaction = true;

			bDoStandardMeleeReactionCheck = false;
		}
		else if(!modifiedFlags.IsFlagSet(CPedDamageCalculator::DF_MeleeDamage)) 
		{
			if( !pHitPed->IsNetworkClone() && pHitPed->IsFatallyInjured() && CTaskNMBehaviour::CanUseRagdoll( pHitPed, RAGDOLL_TRIGGER_MELEE, pFiringEntity, 0.0f ) )
				bUseRagdollReaction = true;

			//! Don't do any animated reactions from remote melee damage events. We want animations to sync so
			//! only reaction from local hit.
			bDoStandardMeleeReactionCheck = false;
		}
		else
		{
			// Check to see if the attack was from behind
			bool bPreferRagdollReaction = false;

			CTask* pTaskUseScenarioBase = pHitPed->GetPedIntelligence()->FindTaskActiveByType( CTaskTypes::TASK_USE_SCENARIO ); 
			if( pTaskUseScenarioBase )
			{
				CTaskUseScenario* pTaskUseScenario = static_cast<CTaskUseScenario*>(pTaskUseScenarioBase);
				const CScenarioInfo& rScenarioInfo = pTaskUseScenario->GetScenarioInfo();
				if( rScenarioInfo.GetIsFlagSet( CScenarioInfoFlags::PreferMeleeRagdoll ) )
					bPreferRagdollReaction = true;
			}

			const CPlayerSpecialAbility* pSpecialAbility = pFiringPed->GetSpecialAbility();
			if( pSpecialAbility && pSpecialAbility->GetType() == SAT_RAGE && pSpecialAbility->IsActive() )
				bPreferRagdollReaction = true;

			if (pHitPed->GetPedResetFlag(CPED_RESET_FLAG_WasHitByVehicleMelee))
			{
				bPreferRagdollReaction = true;
			}
			
			if( pHitPed->GetUsingRagdoll() || bPreferRagdollReaction )
			{
				if( CTaskNMBehaviour::CanUseRagdoll( pHitPed, RAGDOLL_TRIGGER_MELEE, pFiringEntity, 0.0f ) )
					bUseRagdollReaction = true;
				else
					bUseBodyIkReaction = true;
			}
		}

		// Possible try to find a melee reaction to do.
		if( bDoStandardMeleeReactionCheck && uParentMeleeActionResultID != 0 )
		{
			const CActionDefinition* pForcedMeleeActionToDo = NULL;
			if(pMeleeInfo && pMeleeInfo->uForcedReactionDefinitionID != 0)
			{
				u32 nIndexFound = 0;
				pForcedMeleeActionToDo = ACTIONMGR.FindActionDefinition( nIndexFound, pMeleeInfo->uForcedReactionDefinitionID );
			}
			
			// Determine if the one receiving the damage is currently blocking.
			s32 nCurrentTaskPriority = -1;
			CTaskMeleeActionResult* pHitPedCurrentSimpleMeleeActionResultTask = pHitPed->GetPedIntelligence()->GetTaskMeleeActionResult();
			if( pHitPedCurrentSimpleMeleeActionResultTask )
			{
				nCurrentTaskPriority = pHitPedCurrentSimpleMeleeActionResultTask->GetResultPriority();
			}

			// Check to see if we can find an appropriate hit reaction that is available to do.
			CActionFlags::ActionTypeBitSetData actionTypeBitSet;
			actionTypeBitSet.Set( CActionFlags::AT_HIT_REACTION, true );
			const CActionDefinition* pLocalMeleeActionToDo = ACTIONMGR.SelectSuitableAction(
				actionTypeBitSet,		// The type we want to search in.
				NULL,					// No hit combo built up
				true,					// Test whether or not it is an entry action.
				true,					// Only find entry actions.
				pHitPed,
				uParentMeleeActionResultID,
				nCurrentTaskPriority,
				pFiringPed,
				(pHitPedCurrentSimpleMeleeActionResultTask && (pFiringEntity == pHitPedCurrentSimpleMeleeActionResultTask->GetTargetEntity()))?pHitPedCurrentSimpleMeleeActionResultTask->HasLockOnTargetEntity():false,
				false,
				false,
				CActionManager::ShouldDebugPed( pHitPed ) );

			//! Clones cannot do ragdoll reactions, so the pForcedMeleeActionToDo will never prefer to ragdoll. Check that we can ragdoll using local reaction. If so, override
			//! the network forced reaction.
			if(pForcedMeleeActionToDo)
			{
				if(pLocalMeleeActionToDo && pLocalMeleeActionToDo->PreferRagdollResult() && CTaskNMBehaviour::CanUseRagdoll( pHitPed, RAGDOLL_TRIGGER_MELEE, pFiringEntity, 0.0f ))
				{
					pMeleeActionToDo = pLocalMeleeActionToDo;
				}
				else
				{
					pMeleeActionToDo = pForcedMeleeActionToDo;
				}
			}
			else
			{
				pMeleeActionToDo = pLocalMeleeActionToDo;
			}
			
			if( pMeleeActionToDo )
			{
				CTaskMelee::SetLastFoundActionDefinitionForNetworkDamage(pMeleeActionToDo);

				// if the attack has killed the ped, use the nm reaction. Animated melee reactions that kill the ped 
				// (e.g. takedowns and stealth kills) don't go through this path, and the reactions that do result 
				// in the animated hit reaction being overriden immediately by a relax task.

				bool bIsUsingNm = pHitPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning( CTaskTypes::TASK_NM_CONTROL );
				if(pHitPed->IsNetworkClone() && bIsUsingNm && !pHitPed->GetIsDeadOrDying())
				{
					bUseBodyIkReaction = true;
				}
				else if( pHitPed->IsFatallyInjured() || pMeleeActionToDo->PreferRagdollResult() )
				{
					if( CTaskNMBehaviour::CanUseRagdoll( pHitPed, RAGDOLL_TRIGGER_MELEE, pFiringEntity, 0.0f ) )
						bUseRagdollReaction = true;
					else
						bUseBodyIkReaction = true;
				}
				else if( pHitPed->GetPedResetFlag( CPED_RESET_FLAG_PreferMeleeBodyIkHitReaction ) || ( pMeleeActionToDo->PreferBodyIkResult() && !pHitPed->GetPedResetFlag( CPED_RESET_FLAG_BlockIkWeaponReactions ) ) )
					bUseBodyIkReaction = true;
			}
			else
			{
				bUseBodyIkReaction = true;
			}
		}

		// Super hack!  The melee system does not play nicely with the damage/death system.
		// If doing a ragdoll reaction then we need things to work normally and for the damage event to know that we were injured so that it can wrap our ragdoll task with a CTaskDyingDead task...
		// Otherwise, if doing a normal melee reaction we don't want CTaskDyingDead to mess with whatever it is doing so we pretend we weren't injured...
		if (!bUseRagdollReaction)
		{
			tempDamageEvent.GetDamageResponseData().m_bInjured = false;
		}
	}
	// If we plan to kill a ped via melee make sure we do not apply ragdoll yet
	else if( !pHitPed->IsDead() && 
			 ( pHitPed->GetPedConfigFlag( CPED_CONFIG_FLAG_KilledByStealth ) ||
			   pHitPed->GetPedConfigFlag( CPED_CONFIG_FLAG_KilledByTakedown ) ||
			   pHitPed->GetPedConfigFlag( CPED_CONFIG_FLAG_Knockedout ) ) )
	{
		return 0.0f;
	}
	else if(pWeaponInfo->GetDamageType() == DAMAGE_TYPE_EXPLOSIVE || pWeaponInfo->GetDamageType() == DAMAGE_TYPE_FIRE)
	{
		float fEventScore = CTaskNMBehaviour::WantsToRagdoll(pHitPed, RAGDOLL_TRIGGER_EXPLOSION, pFiringEntity, fForce);
		if(fEventScore > 0.0f)
		{
			if(NetworkInterface::IsGameInProgress())
			{
				bUseRagdollEvent = true;
			}

		    float fMultiplierScore = CTaskNMBehaviour::CalcRagdollMultiplierScore(pHitPed, RAGDOLL_TRIGGER_EXPLOSION);
		    if(CTaskNMBehaviour::CanUseRagdoll(pHitPed, RAGDOLL_TRIGGER_EXPLOSION, fEventScore * fMultiplierScore))
		    {
			    bUseRagdollReaction = true;
			    fRagdollImpulseMag = 0.0f;
		    }
		}
	}
	else if(pWeaponInfo->GetDamageType() == DAMAGE_TYPE_ELECTRIC)
	{
		if(pResult != NULL)
		{
			// need to calculate our impulse and force values here before we update the shot task below
			vRagdollImpulseDir = pResult->GetHitPosition() - vStart;
			float fDistance = NormalizeAndMag(vRagdollImpulseDir);

			bool bFront = true;
			// Determine whether this is a front or back shot
			if(weaponVerifyf(pHitPed->GetRagdollInst(), "pHitPed [0x%p] has no RagdollInst", pHitPed))
			{
				const fragInstNMGta* pRagdollInst = pHitPed->GetRagdollInst();
				if(weaponVerifyf(pRagdollInst->GetCacheEntry(), "pHitPed [0x%p]: RagdollInst [0x%p]: No Cache Entry", pHitPed, pRagdollInst))
				{
					const fragCacheEntry* pCacheEntry = pRagdollInst->GetCacheEntry();
					if(weaponVerifyf(pCacheEntry->GetBound(), "pHitPed [0x%p]: RagdollInst [0x%p]: CacheEntry [0x%p]: No Bound", pHitPed, pRagdollInst, pCacheEntry))
					{
						const phBoundComposite* pBound = pCacheEntry->GetBound();
						if(weaponVerifyf(iHitComponent >= 0 && iHitComponent < pBound->GetMaxNumBounds(), "pHitPed [0x%p]: RagdollInst [0x%p]: CacheEntry [0x%p]: Bound [0x%p]: iHitComponent [%d] out of range [0..%d]", pHitPed, pRagdollInst, pCacheEntry, pBound, iHitComponent, pBound->GetMaxNumBounds()))
						{
							Mat34V xComponentMat;
							Transform(xComponentMat, pRagdollInst->GetMatrix(), pBound->GetCurrentMatrix(iHitComponent));

							Vec3V vDirFacing = -xComponentMat.GetCol2();  
							vDirFacing.SetZ(ScalarV(V_ZERO));
							vDirFacing = NormalizeSafe(vDirFacing, Vec3V(V_X_AXIS_WZERO));
							bFront = Dot(vDirFacing, VECTOR3_TO_VEC3V(vRagdollImpulseDir)).Getf() <= 0.0f;
						}
					}
				}
			}

			eAnimBoneTag iHitBoneTag = BONETAG_ROOT;
			if(iHitComponent > -1)
			{
				iHitBoneTag = pHitPed->GetBoneTagFromRagdollComponent(iHitComponent);
			}
			fRagdollImpulseMag = pWeaponInfo->GetForceHitPed(iHitBoneTag, bFront, fDistance);
		}

		if( pHitPed->GetPedConfigFlag( CPED_CONFIG_FLAG_IsSwimming ) )
		{
			bTriggerDrowning = true;
		}
	}
	else if(pWeaponInfo->GetIsGun() && pWeaponInfo->GetDamageType() != DAMAGE_TYPE_TRANQUILIZER)
	{
		if(pHitPed->IsLocalPlayer() && pFiringEntity)
		{
			if(pFiringEntity->GetIsTypePed() && ((CPed*)pFiringEntity)->GetWeaponManager() && ((CPed*)pFiringEntity)->GetWeaponManager()->GetEquippedWeapon())
			{
				if (((CPed*)pFiringEntity)->GetWeaponManager()->GetEquippedWeapon())
				{
					((CPed*)pFiringEntity)->GetWeaponManager()->GetEquippedWeapon()->GetAudioComponent().HandleHitPlayer();
				}
			}
		}

		eRagdollTriggerTypes nTrigger = pWeaponInfo->GetDamageType() != DAMAGE_TYPE_BULLET_RUBBER ? RAGDOLL_TRIGGER_BULLET : RAGDOLL_TRIGGER_RUBBERBULLET;
		if((nTrigger == RAGDOLL_TRIGGER_BULLET || (!pHitPed->IsAPlayerPed() || (tempDamageEvent.GetDamageResponseData().m_bHeadShot || tempDamageEvent.GetDamageResponseData().m_bHitTorso))) && !tempDamageEvent.GetDamageResponseData().m_bIncapacitated)
		{
			float fEventScore = pResult != NULL ? CTaskNMBehaviour::WantsToRagdoll(pHitPed, nTrigger, pFiringEntity, fForce) : 0.0f;
			if(fEventScore > 0.0f)
			{
				if(NetworkInterface::IsGameInProgress())
				{
					bUseRagdollEvent = true;
				}
				float fMultiplierScore = CTaskNMBehaviour::CalcRagdollMultiplierScore(pHitPed, nTrigger);
				if(CTaskNMBehaviour::CanUseRagdoll(pHitPed, nTrigger, fEventScore * fMultiplierScore))
				{
					bUseRagdollReaction = true;

					fragInstNMGta* pRagdollInst = pHitPed->GetRagdollInst();

					// Calculate force now
					vRagdollImpulseDir = pResult->GetHitPosition() - vStart;
					float fDistance = NormalizeAndMag(vRagdollImpulseDir);

					// Don't want to just push ped into the ground all the time
					static dev_bool bModifyShootDir = false;
					f32 fHorizontalMag = vRagdollImpulseDir.XYMag();
					if(bModifyShootDir && fHorizontalMag > 0.05f)
					{
						vRagdollImpulseDir.x /= fHorizontalMag;
						vRagdollImpulseDir.y /= fHorizontalMag;
					}

					weaponAssertf(vRagdollImpulseDir.FiniteElements(), "CWeaponDamage::GeneratePedDamageEvent - Error: vRagdollImpulseDir.FiniteElements(). X:%.3f, Y:%.3f, Z:%.3f", vRagdollImpulseDir.x, vRagdollImpulseDir.y, vRagdollImpulseDir.z);

					s32 iHitComponent = pResult->GetHitComponent();
					eAnimBoneTag iHitBoneTag = BONETAG_ROOT;

					Mat34V xComponentMat(pRagdollInst->GetMatrix());

					if(iHitComponent >= 0 && pRagdollInst->GetCached())
					{
						iHitBoneTag = pHitPed->GetBoneTagFromRagdollComponent(iHitComponent);

						phArchetype* pArchetype = pRagdollInst->GetArchetype();
						if(nmVerifyf(pArchetype != NULL, "CWeaponDamage::GeneratePedDamageEvent: Invalid archetype") && 
							nmVerifyf(pArchetype->GetBound() != NULL && pArchetype->GetBound()->GetType() == phBound::COMPOSITE, "CWeaponDamage::GeneratePedDamageEvent: Invalid bound"))
						{
							int iMappedComponent = pRagdollInst->MapRagdollLODComponentHighToCurrent(iHitComponent);
							if(nmVerifyf(iMappedComponent != -1 && iMappedComponent < static_cast<phBoundComposite*>(pArchetype->GetBound())->GetNumBounds(), "CWeaponDamage::GeneratePedDamageEvent: Ped (%s). Mapped Ragdoll component [%d] is out of range 0..[%d]. Original component [%d]", pHitPed->GetModelName(), iMappedComponent, pRagdollInst->GetTypePhysics()->GetNumChildren() - 1, iHitComponent))
							{
								Transform(xComponentMat, static_cast<phBoundComposite*>(pArchetype->GetBound())->GetCurrentMatrix(iMappedComponent));
							}
						}
					}

					Vec3V vDirFacing = -xComponentMat.GetCol2();  
					vDirFacing.SetZ(ScalarV(V_ZERO));
					vDirFacing = NormalizeSafe(vDirFacing, Vec3V(V_X_AXIS_WZERO));
					// Determine whether this is a front or back shot
					bool bFront = Dot(vDirFacing, VECTOR3_TO_VEC3V(vRagdollImpulseDir)).Getf() <= 0.0f;

					fRagdollImpulseMag = pWeaponInfo->GetForceHitPed(iHitBoneTag, bFront, fDistance);

					// If script wants to force their control of the ragdoll, only apply minimal forces to the ragdoll
					static dev_float sfScriptControlledRagdollMult = 0.1f;
					static dev_float sfRagdollBulletForceMult      = 1.0f;
					fRagdollImpulseMag *= pHitPed->GetPedResetFlag( CPED_RESET_FLAG_ForceScriptControlledRagdoll ) ? sfScriptControlledRagdollMult : sfRagdollBulletForceMult;
				}
			}

			// Check to see if ped was hit by rubber bullet
			if( pWeaponInfo->GetDamageType()==DAMAGE_TYPE_BULLET_RUBBER  )
			{
				if( pHitPed->GetPedConfigFlag( CPED_CONFIG_FLAG_IsSwimming ) )
				{
					bTriggerDrowning = true;
				}
				else if( pHitPed->GetPedResetFlag( CPED_RESET_FLAG_IsClimbing ) && CTaskNMBehaviour::CanUseRagdoll( pHitPed, RAGDOLL_TRIGGER_FALL, pFiringEntity, fForce ) )
				{
					bUseRagdollReaction = true;
					fRagdollImpulseMag = fForce;
				}
			}
		}
	}
	else if(uWeaponHash == WEAPONTYPE_DROWNING)
	{
		eRagdollTriggerTypes trigger = RAGDOLL_TRIGGER_IN_WATER;
		if(CTaskNMBehaviour::CanUseRagdoll(pHitPed, trigger, pFiringEntity, fForce))
		{
			bUseRagdollReaction = true;
			fRagdollImpulseMag = 0.0f;
		}
	}
	// Falling damage events can cause ragdoll reactions - this is used to generically damage peds without using guns
	// but causing ragdoll reactions
	else if(uWeaponHash == WEAPONTYPE_FALL)
	{
		if(CTaskNMBehaviour::CanUseRagdoll(pHitPed, RAGDOLL_TRIGGER_FALL))
		{
			bUseRagdollReaction = true;
			fRagdollImpulseMag = 0.0f;
		}
	}
	else if((uWeaponHash == WEAPONTYPE_SMOKEGRENADE || uWeaponHash == WEAPONTYPE_DLC_BOMB_GAS || uWeaponHash == WEAPONTYPE_DLC_BZGAS_MK2) && 
		(pHitPed->HasHurtStarted() || pHitPed->GetPedConfigFlag(CPED_CONFIG_FLAG_CowerInsteadOfFlee)))
	{
		bool bBlockRagdoll = false;
		if(pFiringEntity && pFiringEntity->GetIsTypePed())
		{
			CPed* pFiringPed = (CPed*)pFiringEntity;
			bBlockRagdoll = pHitPed->GetPedIntelligence()->IsFriendlyWith(*pFiringPed);
		}

		if(!bBlockRagdoll && CTaskNMBehaviour::CanUseRagdoll(pHitPed, RAGDOLL_TRIGGER_SMOKE_GRENADE))
		{
			bUseRagdollReaction = true;
			fRagdollImpulseMag = 0.0f;
		}
	}
	else if(uWeaponHash == WEAPONTYPE_ROTORS )
	{
		if(CTaskNMBehaviour::CanUseRagdoll(pHitPed, RAGDOLL_TRIGGER_HELIBLADES))
		{
			bUseRagdollReaction = true;
		}
	}
	else if (uWeaponHash == WEAPONTYPE_DLC_SNOWBALL)
	{
		if (pResult)
		{
			bool bRagdollPermitted = false;

			vRagdollImpulseDir = -pResult->GetHitNormal();

			//	Ragdoll only if hit in the head and not getting up (unless we've killed the ped)
			bRagdollPermitted = ((iHitBoneTag==BONETAG_HEAD || iHitBoneTag==BONETAG_NECK) && !pHitPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_GET_UP)) 
				|| pHitPed->ShouldBeDead() 
				|| (pHitPed->IsNetworkClone() && (fWeaponDamage >= (pHitPed->GetHealth() - pHitPed->GetInjuredHealthThreshold())));

			if (bRagdollPermitted && CTaskNMBehaviour::CanUseRagdoll(pHitPed, RAGDOLL_TRIGGER_SNOWBALL, pFiringEntity, 0.0f))
			{
				bUseRagdollReaction = true;
			}
			else
			{
				bUseBodyIkReaction = true;
			}
		}		
	}

	if (bTriggerDrowning)
	{
		// Force the ped to start drowning (take incremental damage)
		Assert( pHitPed->GetPedIntelligence() );
		Assert( pHitPed->GetPedIntelligence()->GetEventScanner() );
		pHitPed->GetPedIntelligence()->GetEventScanner()->GetInWaterEventScanner().StartDrowningNow( *pHitPed );
	}

	// Handle damage to peds that are already dead.
	if(pHitPed->IsDead())
	{
		bool bCanQuitWhenDead = true;
		CTask* pTaskActive = pHitPed->GetPedIntelligence()->GetTaskActive();
		CDamageInfo damageInfo(uWeaponHash, &vStart, pResult, &vRagdollImpulseDir, fRagdollImpulseMag, pHitPed);
		CTaskNMBehaviour* pTaskNM = pHitPed->GetPedIntelligence()->GetLowestLevelNMTask(pHitPed);

		if(pResult)
		{
			if(pTaskNM != NULL && pTaskNM->GetTaskType() == CTaskTypes::TASK_NM_SHOT)
			{
				static_cast<CTaskNMShot*>(pTaskNM)->UpdateShot(pHitPed, pFiringEntity, uWeaponHash, pResult->GetHitComponent(), pResult->GetHitPosition(), fRagdollImpulseMag * vRagdollImpulseDir, pResult->GetHitNormal());
			}
			else if(pTaskNM != NULL && pTaskNM->GetTaskType() == CTaskTypes::TASK_NM_FLINCH)
			{
				Vector3 vHitNormal;
				vHitNormal.Negate(pResult->GetHitNormal());
				taskDisplayf("melee hit: position (x:%.3f, y:%.3f, z:%.3f) Normal (x:%.3f, y:%.3f, z:%.3f) ragdollImpulseDir (x:%.3f, y:%.3f, z:%.3f)"
					, pResult->GetHitPosition().x, pResult->GetHitPosition().y, pResult->GetHitPosition().z
					, vHitNormal.x, vHitNormal.y, vHitNormal.z
					, vRagdollImpulseDir.x, vRagdollImpulseDir.y, vRagdollImpulseDir.z);

				static_cast<CTaskNMFlinch*>(pTaskNM)->UpdateFlinch(pHitPed, pFiringEntity != NULL ? VEC3V_TO_VECTOR3(pFiringEntity->GetTransform().GetPosition()) : VEC3_ZERO, 
					pFiringEntity, CTaskNMFlinch::FLINCHTYPE_MELEE, uWeaponHash, (s32)pResult->GetHitComponent(), false, &pResult->GetHitPosition(), &vHitNormal, &vRagdollImpulseDir);

				if (CTaskNMBehaviour::ms_bUseParameterSets && !pHitPed->CheckAgilityFlags(AF_DONT_FLINCH_ON_MELEE))
				{
					static_cast<CTaskNMFlinch*>(pTaskNM)->SetType(CTaskNMFlinch::FLINCHTYPE_MELEE_PASSIVE);
				}
				else
				{
					static_cast<CTaskNMFlinch*>(pTaskNM)->SetType(CTaskNMFlinch::FLINCHTYPE_MELEE);
				}
			}
			else if(pTaskActive && pTaskActive->RecordDamage(damageInfo))
			{
				// Do nothing, the corpse task has handled the reaction
				// But try to use an IK react if the ped is in a vehicle
				if (!bUseRagdollReaction && !pHitPed->GetUsingRagdoll() && (pHitPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle) || pHitPed->GetPedConfigFlag(CPED_CONFIG_FLAG_OnMount)))
				{
					if (!pHitPed->GetPedResetFlag(CPED_RESET_FLAG_BlockIkWeaponReactions))
					{
						Vec3V vPosition(VECTOR3_TO_VEC3V(pResult->GetHitPosition()));
						Vec3V vDirection(NormalizeFast(Subtract(vPosition, VECTOR3_TO_VEC3V(vStart))));

						CIkRequestBodyReact bodyReactRequest(vPosition, vDirection, (int)pResult->GetHitComponent());
						bodyReactRequest.SetWeaponGroup(pWeaponInfo->GetGroup());
						pHitPed->GetIkManager().Request(bodyReactRequest);
					}
				}
			}
			else
			{
				// Apply force using a bullet force helper, if one is available
				if(pHitPed->GetUsingRagdoll() && bUseRagdollReaction && fRagdollImpulseMag > 0.0f)
				{
					pHitPed->GetRagdollInst()->ApplyBulletForce(pHitPed, fRagdollImpulseMag, vRagdollImpulseDir, pResult->GetHitPosition(), pResult->GetHitComponent(), uWeaponHash);
				}

				// Ragdoll is being used, but no task is controlling it, so can't quit out here
				if(pHitPed->GetRagdollState() >= RAGDOLL_STATE_PHYS_ACTIVATE)
				{
					bCanQuitWhenDead = false;
				}
			}
		}

		if(modifiedFlags.IsFlagSet(CPedDamageCalculator::DF_MeleeDamage) && pFiringEntity->GetIsTypePed())
		{
			CCrime::ReportCrime(CRIME_HIT_PED, pHitPed, static_cast<CPed*>(pFiringEntity));
		}

		// The damage has been handled so we can quick exit here.
		if(bCanQuitWhenDead)
		{
			return 0.0f;
		}
	}// END handle damage to peds that are already dead.
	
	// Allow for post death melee ragdoll reactions
	if(!modifiedFlags.IsFlagSet(CPedDamageCalculator::DF_MeleeDamage) && !tempDamageEvent.AffectsPed(pHitPed))
	{
		// It doesn't need to do anything else to the ped.
		return 0.0f;
	}

	//! DMKH. Removed old legacy code. We need to kick off hit reactions on hit ped owner's machine, so skip this code now.

	// In network games melee hit reaction animations are handled separately from when the damage
	// is applied from the ped. The hit reactions are played immediately to reduce the effects of
	// network latency, but the damage is applied when the network message is received from the owner.
	// When applying damage we don't want the damage event to affect the ped in other ways since the
	// hit reaction will already have been invoked.
	/*bool bHandlingNetworkMeleeHealthChangesLocally = !bAllowMeleeHitReactions && (bIsMeleeDamage || pWeaponInfo->GetIsMelee());
	if(bHandlingNetworkMeleeHealthChangesLocally)
	{
		return (tempDamageEvent.GetDamageResponseData().m_fHealthLost + tempDamageEvent.GetDamageResponseData().m_fArmourLost);
	}*/

	// The damage event needs to affect the ped in other ways...
	bool			bWasKilled = tempDamageEvent.GetDamageResponseData().m_bKilled;
	bool			bWasKilledOrInjured		= (bWasKilled || tempDamageEvent.GetDamageResponseData().m_bInjured);
	fwMvClipId		clipId					= CLIP_ID_INVALID;
	fwMvClipSetId	clipSetId				= CLIP_SET_ID_INVALID;
	CTask*			pTaskPhysicalResponse	= NULL;

	// It is ragdoll damage.
#if ENABLE_HORSE
	if (!bWasKilled && pHitPed->GetHorseComponent()) //mountable animals don't ragdoll until dead (TODO maybe all animals?)
	{
		bUseRagdollReaction = false;
		bUseRagdollEvent = false;
	}
#endif

	bool bUpdatedPreviousEvent = false;
	if( bUseRagdollReaction || bUseRagdollEvent )
	{
		bool bTaskAppliesImpulse = false;
		pTaskPhysicalResponse = GenerateRagdollTask(pFiringEntity, pHitPed, uWeaponHash, fWeaponDamage, modifiedFlags, bWasKilledOrInjured, vStart, pResult, vRagdollImpulseDir, fRagdollImpulseMag, bTaskAppliesImpulse, bUpdatedPreviousEvent);

		pHitPed->SetClothPinRadiusScale(0.0f);

		// Force should have already been scaled by component mass by this point
		if(pHitPed->GetUsingRagdoll() && fRagdollImpulseMag > 0.0f && pResult)
		{
			// Apply force using a bullet force helper, if one is available
			if (!bTaskAppliesImpulse || pHitPed->GetRagdollInst()->m_AgentId == -1)
				pHitPed->GetRagdollInst()->ApplyBulletForce(pHitPed, fRagdollImpulseMag, vRagdollImpulseDir, pResult->GetHitPosition(), pResult->GetHitComponent(), uWeaponHash);

			// If any additional force has been specified, apply it now
			if(!vAdditionalRagdollForce.IsZero())
			{
				s32 iHitComponent = pHitPed->GetRagdollComponentFromBoneTag(BONETAG_SPINE2);
				pHitPed->ApplyImpulse(vAdditionalRagdollForce, VEC3_ZERO, iHitComponent, true);
			}
		}

		// store the damage information for applying the damage reaction later, this is required so we can kick off local ragdoll reactions for dying peds
		if (pResult && pHitPed->IsAPlayerPed() && pHitPed->GetNetworkObject())
		{
			static_cast<CNetObjPlayer*>(pHitPed->GetNetworkObject())->SetLastDamageInfo(pResult->GetHitPosition(), vRagdollImpulseDir, pResult->GetHitComponent());
		}
	}
	else if( bUseBodyIkReaction && pResult )
	{
		CIkRequestBodyReact bodyReactRequest(RCC_VEC3V(pResult->GetHitPosition()), RC_VEC3V(vRagdollImpulseDir), (int)pResult->GetHitComponent());
		bodyReactRequest.SetWeaponGroup(pWeaponInfo->GetGroup());
		pHitPed->GetIkManager().Request(bodyReactRequest);
	}
	else if( bIsStandardMeleeReaction )
	{
		// It is melee damage

		// Determine if the ped is already doing the correct reaction.
		// This can happen when the reaction anim itself is invoking the damage.
		if(!modifiedFlags.IsFlagSet(CPedDamageCalculator::DF_MeleeDamage))
		{
			// Check if it is injured/die damage and if this melee self damage event
			// killed the ped.
			// If it isn't then we don't need to do anything since we are currently
			// playing the correct animation, so just let it play out and be removed.
			// Otherwise we are already doing the correct animation, but we need to
			// make sure it isn't removed (since we are dead and don't need to do any
			// other anims).
			if( bWasKilled )
			{
				// Make sure the health is set to be totally dead.
				pHitPed->SetHealth(0.0f);
			}
			
			// Make sure we don't do anything (see above).
			bAllowTempDamageEventToBeAdded = false;
		}
		else
		{
			// Check if it is injured/die damage.
			if( bWasKilledOrInjured )
			{
				// Make a injured/die task.
				CDeathSourceInfo info(pFiringEntity, uWeaponHash, fHitDir, iHitBoneTag);
			
				const CActionResult* pActionResult = pMeleeActionToDo ? pMeleeActionToDo->GetActionResult( pHitPed ) : NULL;
				if( pActionResult && pActionResult->GetClipSetID() != CLIP_SET_ID_INVALID && pActionResult->GetAnimID() != CLIP_ID_INVALID )
				{
					pTaskPhysicalResponse = rage_new CTaskDyingDead(&info);
					static_cast<CTaskDyingDead*>(pTaskPhysicalResponse)->SetDeathAnimationBySet( fwMvClipSetId(pActionResult->GetClipSetID()), fwMvClipId(pActionResult->GetAnimID()), NORMAL_BLEND_DURATION);
				}
				else 
				{
					CTaskMeleeActionResult* pHitPedMeleeActionTask = pHitPed->GetPedIntelligence()->GetTaskMeleeActionResult();
					if( pHitPedMeleeActionTask && pHitPedMeleeActionTask->GetActionResult() && !pHitPedMeleeActionTask->IsOffensiveMove() )
					{
						pTaskPhysicalResponse = rage_new CTaskDyingDead(&info, CTaskDyingDead::Flag_startDead);
						static_cast<CTaskDyingDead*>(pTaskPhysicalResponse)->SetDeathAnimationBySet( fwMvClipSetId( pHitPedMeleeActionTask->GetActionResult()->GetClipSetID() ), fwMvClipId( pHitPedMeleeActionTask->GetActionResult()->GetAnimID() ), INSTANT_BLEND_DURATION);
					}
					else
					{
						pTaskPhysicalResponse = rage_new CTaskDyingDead(&info);
					}

					// Force this task to activate on this frame otherwise there is a 1 frame CTaskDoNothing frame that attemps to blend into the death animation
					pHitPed->SetPedResetFlag( CPED_RESET_FLAG_ForcePostCameraAIUpdate, true );
				}
			}
			else
			{
				// Try to get a pointer to the actionResults via the actionResult Id for the action.
				const CActionResult* pActionResult = pMeleeActionToDo ? pMeleeActionToDo->GetActionResult( pHitPed ) : NULL;
				if( pActionResult )
				{
					CTaskMelee* pHitPedComplexActionTask = pHitPed->GetPedIntelligence()->GetTaskMelee();
					if( pHitPedComplexActionTask )
					{
						pHitPedComplexActionTask->SetLocalReaction( pActionResult, true, pMeleeInfo ? pMeleeInfo->uNetworkActionID : 0 );
					}
					else
					{
						bool bCanAddMeleeReactionTask = true;
						if (pHitPed->GetPedResetFlag(CPED_RESET_FLAG_IsEnteringVehicle) && pHitPed->GetIsAttached())
						{
							const CTaskEnterVehicle* pEnterVehicleTask = static_cast<const CTaskEnterVehicle*>(pHitPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE));
							if (pEnterVehicleTask)
							{
								const aiTask* pSubTask = pEnterVehicleTask->GetSubTask();
								const s32 iState = pEnterVehicleTask->GetState();
								if (iState < CTaskEnterVehicle::State_OpenDoor)
								{
									bCanAddMeleeReactionTask = true;
								}
								else if (iState == CTaskEnterVehicle::State_OpenDoor && pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_OPEN_VEHICLE_DOOR_FROM_OUTSIDE)
								{
									//! Don't do local reaction here. If the owner has proceeded past this point & we attempt to locally react ped, they'll snap into vehicle with a pretty 
									//! bad blend.
									if(pHitPed->IsNetworkClone())
									{
										bCanAddMeleeReactionTask = false;
									}
									else
									{
										bCanAddMeleeReactionTask = (pSubTask->GetState() == CTaskOpenVehicleDoorFromOutside::State_OpenDoor 
																	|| pSubTask->GetState() == CTaskOpenVehicleDoorFromOutside::State_OpenDoorBlend 
																	|| pSubTask->GetState() == CTaskOpenVehicleDoorFromOutside::State_OpenDoorCombat
																	|| pSubTask->GetState() == CTaskOpenVehicleDoorFromOutside::State_OpenDoorWater);
									}
								}
								else
								{
									bCanAddMeleeReactionTask = false;
								}
							}
						}

						if(pHitPed->IsNetworkClone() && pHitPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning( CTaskTypes::TASK_VAULT, false ))
						{
							bCanAddMeleeReactionTask = false;
						}

						// B*2001652: Don't add melee reaction task if we're running the NM task and don't have our balance.
						if (pHitPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_NM_CONTROL))
						{
							CTaskNMControl* pNMTask = smart_cast<CTaskNMControl*>(pHitPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_NM_CONTROL));
							if(pNMTask && pNMTask->IsFeedbackFlagSet(CTaskNMControl::BALANCE_FAILURE))
							{
								bCanAddMeleeReactionTask = false;
							}
						}

						if (bCanAddMeleeReactionTask)
						{
							// Add a new simple action.
							weaponAssert( pFiringEntity && pFiringEntity->GetIsTypePed() );
							pTaskPhysicalResponse = rage_new CTaskMelee( pActionResult, static_cast<CPed*>( pFiringEntity ), CTaskMelee::MF_IsDoingReaction, CSimpleImpulseTest::ImpulseNone, 0, pMeleeInfo ? pMeleeInfo->uNetworkActionID : 0 );
							tempDamageEvent.SetMeleeResponse(true);
						}
					}

					// Force update the AI state to save a frame when reaction to the strike
					pHitPed->SetPedResetFlag( CPED_RESET_FLAG_ForcePostCameraAIUpdate, true );

					static dev_bool s_bInstantUpdateForClone = true;
					if(pHitPed->IsNetworkClone() && s_bInstantUpdateForClone)
					{
						pHitPed->SetPedResetFlag( CPED_RESET_FLAG_AllowCloneForcePostCameraAIUpdate, true );
					}
				}
			}
		}
	}
	else if(pWeaponInfo->GetDamageType()==DAMAGE_TYPE_FIRE)
	{
		CTask* pCurrentTask = pHitPed->GetPedIntelligence()->GetTaskActive();
		if(pCurrentTask->GetTaskType() != CTaskTypes::TASK_COMPLEX_ON_FIRE)
		{
			pTaskPhysicalResponse = rage_new CTaskComplexOnFire(uWeaponHash);
		}
	}
	else if( !pHitPed->GetTaskData().GetIsFlagSet(CTaskFlags::DisableNonStandardDamageTypes) &&
			 ( (pWeaponInfo->GetDamageType() == DAMAGE_TYPE_ELECTRIC && !pHitPed->GetPedConfigFlag(CPED_CONFIG_FLAG_BlockElectricWeaponDamage)) ||
			   (pWeaponInfo->GetDamageType() == DAMAGE_TYPE_BULLET_RUBBER && (!pHitPed->IsAPlayerPed() || tempDamageEvent.GetDamageResponseData().m_bHeadShot || tempDamageEvent.GetDamageResponseData().m_bHitTorso)) ) )
	{
		// Calculate damage response length
		u32 uDamageTime;
		if (pHitPed->GetMinOnGroundTimeForStunGun()>=0)
		{
			TUNE_GROUP_FLOAT(STUNGUN, fFallDownTimeForTimeOverride, 2000.0f, 0.0f, 1000000.0f, 50.0f);
			uDamageTime = (u32)fFallDownTimeForTimeOverride + (u32)pHitPed->GetMinOnGroundTimeForStunGun();
		}
		else
		{
			if(pHitPed->GetVehiclePedInside())
			{
				uDamageTime = static_cast<u32>((tempDamageEvent.GetDamageResponseData().m_bHeadShot ? pWeaponInfo->GetDamageTimeInVehicleHeadShot() : pWeaponInfo->GetDamageTimeInVehicle())*1000.0f);
			}
			else
			{
				uDamageTime = static_cast<u32>(pWeaponInfo->GetDamageTime()*1000.0f);
			}
		}

		if (pHitPed->IsPlayer() && pWeaponInfo->GetBlockRagdollResponseTaskForPlayer())
		{
			// Do nothing
		}
		else
		{
			// Create ragdoll response task for given length
			CTask* pCurrentTask = pHitPed->GetPedIntelligence()->GetTaskActive();
			if(pCurrentTask->GetTaskType() != CTaskTypes::TASK_DAMAGE_ELECTRIC)
			{
				CPed *pFiringEntityPed = NULL;
				if (pFiringEntity && pFiringEntity->GetIsTypePed())
					pFiringEntityPed = static_cast<CPed *>(pFiringEntity);

				eRagdollTriggerTypes ragdollTriggerType = GetRagdollTriggerForDamageType(pWeaponInfo->GetDamageType());

				if (pResult)
				{
					vRagdollImpulseDir = pResult->GetHitPosition() - vStart;
					vRagdollImpulseDir.Normalize();

					// Apply a stun gun impulse here when dangling (otherwise the electric task is getting killed in that case with no impulse applied)
					CTask* pTaskSimplest = pHitPed->GetPedIntelligence()->GetTaskActiveSimplest();
					if (pTaskSimplest->GetTaskType() == CTaskTypes::TASK_NM_DANGLE)
						pHitPed->GetRagdollInst()->ApplyBulletForce(pHitPed, fRagdollImpulseMag, vRagdollImpulseDir, pResult->GetHitPosition(), pResult->GetHitComponent(), uWeaponHash);

					pTaskPhysicalResponse = rage_new CTaskDamageElectric(pWeaponInfo->GetPedMotionClipSetId(), uDamageTime, ragdollTriggerType,
						pFiringEntity, uWeaponHash, pResult->GetHitComponent(), pResult->GetHitPosition(), fRagdollImpulseMag * vRagdollImpulseDir, pResult->GetHitNormal());	

					if(pTaskPhysicalResponse && uWeaponHash == atHashString("WEAPON_STUNGUN_MP", 0x45CD9CF3))
					{
						pHitPed->SetPedConfigFlag(CPED_CONFIG_FLAG_DisableHealthRegenerationWhenStunned, true);
					}
				}
				else
				{
					pTaskPhysicalResponse = rage_new CTaskDamageElectric(pWeaponInfo->GetPedMotionClipSetId(), uDamageTime, ragdollTriggerType);
					((CTaskDamageElectric*)pTaskPhysicalResponse)->SetContactPosition(vStart);
				}

				// Raise this to a ragdoll event if possible (otherwise it won't activate immediately if the ped is being bumped, etc)
				if (pHitPed->GetUsingRagdoll() || CTaskNMBehaviour::CanUseRagdoll(pHitPed, ragdollTriggerType, pFiringEntity))
				{
					bUseRagdollReaction = true;
				}
			}
			else if (pResult)
			{
				vRagdollImpulseDir = pResult->GetHitPosition() - vStart;
				vRagdollImpulseDir.Normalize();
				CTaskDamageElectric* pDamageElectricTask = static_cast<CTaskDamageElectric*>(pCurrentTask);
				pDamageElectricTask->Update(uDamageTime, pFiringEntity, uWeaponHash, pResult->GetHitComponent(), pResult->GetHitPosition(), fRagdollImpulseMag * vRagdollImpulseDir, pResult->GetHitNormal());
			}
		}
	}
	else
	{
		// It is normal (non-ragdoll, non-melee) damage.
		// Check if it is injured/die damage.
		if( bWasKilledOrInjured )
		{
			// give the player a die by smoke grenade nm reaction
			const bool bGasWeapon = uWeaponHash == WEAPONTYPE_SMOKEGRENADE || uWeaponHash == WEAPONTYPE_DLC_BOMB_GAS || uWeaponHash == WEAPONTYPE_DLC_BZGAS_MK2;
			if (bGasWeapon && (pHitPed->IsLocalPlayer() || pHitPed->EstimatePose()!=POSE_STANDING) && CTaskNMBehaviour::CanUseRagdoll(pHitPed, RAGDOLL_TRIGGER_SMOKE_GRENADE))
			{
				const CTaskNMSimple::Tunables::Tuning* pTuning = CTaskNMSimple::sm_Tunables.GetTuning("Death_SmokeGrenade");
				if (pTuning != NULL)
				{
					pTaskPhysicalResponse = rage_new CTaskNMControl(500, 5000, rage_new CTaskNMSimple(*pTuning), CTaskNMControl::ALL_FLAGS_CLEAR);
					bUseRagdollReaction = true;
				}
			}
			
			if(!pTaskPhysicalResponse)
			{			// Make a injured/die task.
				CDeathSourceInfo info(pFiringEntity, uWeaponHash, fHitDir, iHitBoneTag);
				s32 iFlags = flags.IsFlagSet(CPedDamageCalculator::DF_KillPriorToClearedWantedLevel) ? CTaskDyingDead::Flag_DontSendPedKilledShockingEvent : 0;
				pTaskPhysicalResponse = rage_new CTaskDyingDead(&info, iFlags);
			}
		}
		else
		{
			bool bNeedsTask = false;
			bool bNeedsFallTask = false;
			tempDamageEvent.ComputeDamageAnim(pHitPed, pFiringEntity, fHitDir, iHitBoneTag, clipSetId, clipId, bNeedsTask, bNeedsFallTask);

			if(clipId != CLIP_ID_INVALID)
			{
#if __DEV
				if(fwAnimManager::GetClipIfExistsBySetId(clipSetId, clipId) == NULL)
				{
					atHashString clipDictName;
					atHashString clipName;
					// PH - Trying to catch a crash where an animation is selected that isn't present in the group chosen
					if(fwClipSetManager::GetClipDictionaryNameAndClipName(clipSetId, clipId, clipDictName, clipName))
					{
						weaponAssertf(0, "[Anim] Missing anim (%s, %s)", clipDictName.GetCStr(), clipName.GetCStr());
					}
				}
#endif // __DEV
				if(bNeedsFallTask)
				{
					if (pWeaponInfo->GetDamageType() == DAMAGE_TYPE_EXPLOSIVE)
					{
						pTaskPhysicalResponse = rage_new CTaskAnimatedHitByExplosion(clipSetId, clipId);
					}
					else
					{
						pTaskPhysicalResponse = rage_new CTaskFallAndGetUp(clipSetId, clipId, 0.001f);
					}
				}
				else
				{
					if(!pHitPed->GetPedResetFlag(CPED_RESET_FLAG_BlockAnimatedWeaponReactions))
					{
						CGenericClipHelper& helper = pHitPed->GetMovePed().GetAdditiveHelper();
						helper.BlendInClipBySetAndClip(pHitPed, clipSetId, clipId);
					}
				}
			}
			else
			{
				if (pResult)
				{
					const bool bGasWeapon = uWeaponHash == WEAPONTYPE_SMOKEGRENADE || uWeaponHash == WEAPONTYPE_DLC_BOMB_GAS || uWeaponHash == WEAPONTYPE_DLC_BZGAS_MK2;
					if(!pHitPed->GetPedResetFlag(CPED_RESET_FLAG_BlockIkWeaponReactions) && !bGasWeapon)
					{
						// Only sending IK react request when one or both entities are local since non-local reactions are already handled in DoWeaponImpact (avoids duplicate requests)
						const bool bHitPedLocal = !NetworkUtils::GetNetworkObjectFromEntity(pHitPed) || !NetworkUtils::IsNetworkClone(pHitPed);
						const bool bFiringEntityLocal = !NetworkUtils::GetNetworkObjectFromEntity(pFiringEntity) || !NetworkUtils::IsNetworkClone(pFiringEntity);

						if (bHitPedLocal || bFiringEntityLocal)
						{
							Vec3V vPosition(VECTOR3_TO_VEC3V(pResult->GetHitPosition()));
							Vec3V vDirection(Subtract(vPosition, VECTOR3_TO_VEC3V(vStart)));
							vDirection = NormalizeFast(vDirection);

							CIkRequestBodyReact bodyReactRequest(vPosition, vDirection, (int)pResult->GetHitComponent());
							bodyReactRequest.SetWeaponGroup(pWeaponInfo->GetGroup());
							pHitPed->GetIkManager().Request(bodyReactRequest);
						}

						// If player ped, play facial pain anim.  Non-player peds get pain animations kicked off from TaskNMShot.
						if (pHitPed->IsPlayer() && pHitPed->GetFacialData())
						{
							pHitPed->GetFacialData()->PlayPainFacialAnim(pHitPed);
						}
					}
				}
			}
		}

#if __BANK
		// Record that the animation fallback was used
		if (fragInstNMGta::ms_bLogUsedRagdolls && pHitPed && !pHitPed->IsLocalPlayer())
		{
			if (CTheScripts::GetPlayerIsOnAMission())
			{
				fragInstNMGta::ms_NumFallbackAnimsUsedCurrentLevel++;
			}
			fragInstNMGta::ms_NumFallbackAnimsUsedGlobally++;
		}
#endif
	}

	// Apply any player damage over time effects
	if (pWeaponInfo->GetPlayerDamageOverTimeWeapon() != 0 && pHitPed->IsLocalPlayer() && !flags.IsFlagSet(CPedDamageCalculator::DF_MeleeDamage))
	{
		pHitPed->GetPlayerInfo()->GetDamageOverTime().Start(pWeaponInfo->GetPlayerDamageOverTimeWeapon(), pHitPed, pFiringEntity);
	}

	// And the event and physical response (if we should and if one was found).
	bool bPhysicalResponseAdded = false;

    if (pHitPed->IsNetworkClone())
    {
        CTask *pCloneTask = pHitPed->GetPedIntelligence()->GetTaskActive();

        if(pCloneTask && (pCloneTask->GetTaskType() == CTaskTypes::TASK_SYNCHRONIZED_SCENE))
        {
            bAllowTempDamageEventToBeAdded = false;
        }
    }

	if(bAllowTempDamageEventToBeAdded)
	{
		// Don't add a new event if we just updated a previous one
		if (bUpdatedPreviousEvent)
			return (tempDamageEvent.GetDamageResponseData().m_fHealthLost + tempDamageEvent.GetDamageResponseData().m_fArmourLost);

		if (pTaskPhysicalResponse)
		{
			tempDamageEvent.SetPhysicalResponseTask(pTaskPhysicalResponse, bUseRagdollReaction);
			bPhysicalResponseAdded = true;
		}

		if (pHitPed->IsNetworkClone() && !pHitPed->GetIsInVehicle())
		{
			if (pTaskPhysicalResponse && pTaskPhysicalResponse->IsClonedFSMTask() && (pTaskPhysicalResponse->GetTaskType() != CTaskTypes::TASK_DAMAGE_ELECTRIC))
			{
				CTaskFSMClone* pTaskCopy = static_cast<CTaskFSMClone*>(pTaskPhysicalResponse->Copy());
				bool bTaskUsed = false;
				
#if CNC_MODE_ENABLED
				if (pHitPed->GetPedIntelligence()->GetTaskActive()->GetTaskType() == CTaskTypes::TASK_INCAPACITATED)
				{
					if (tempDamageEvent.GetEventPriority() > E_PRIORITY_INCAPACITATED && pTaskCopy->GetTaskType() == CTaskTypes::TASK_NM_CONTROL) 
					{
						CTaskIncapacitated* pIncapTask = (CTaskIncapacitated*)pHitPed->GetPedIntelligence()->GetTaskActive();
						pTaskCopy->SetRunningLocally(true);
						pTaskCopy->SetRunAsAClone(true);
						pTaskCopy->SetPriority(PED_TASK_PRIORITY_EVENT_RESPONSE_TEMP);
							
						pIncapTask->SetForcedNaturalMotionTask(pTaskCopy);
						bTaskUsed = true;
					}
				}
#endif
				if(flags.IsFlagSet( CPedDamageCalculator::DF_MeleeDamage ))
				{
					pHitPed->GetPedIntelligence()->AddLocalCloneTask(pTaskCopy, PED_TASK_PRIORITY_EVENT_RESPONSE_TEMP);
					bTaskUsed = true;
				}	
				else if(pWeaponInfo->GetDamageType()==DAMAGE_TYPE_BULLET)
				{
					if (pHitPed->IsAPlayerPed())
					{
						if(pHitPed->GetPedIntelligence()->NetworkLocalHitReactionsAllowed())
						{
							if (!pTaskCopy->HandlesDeadPed() && pTaskCopy->GetTaskType() == CTaskTypes::TASK_NM_CONTROL)
							{
								CDeathSourceInfo info(pFiringEntity, uWeaponHash);
								CTaskDyingDead* pDeathTask = rage_new CTaskDyingDead(&info);
								pDeathTask->SetForcedNaturalMotionTask(pTaskCopy);
								pTaskCopy = pDeathTask;
							}

							if (fWeaponDamage > 0.0f && CGameWorld::FindLocalPlayer() == pFiringEntity && static_cast<CNetObjPlayer*>(pHitPed->GetNetworkObject())->SetDeathResponseTask(pTaskCopy))
							{
								bTaskUsed = true;
							}
						}

						if (pResult && !pHitPed->GetPedResetFlag(CPED_RESET_FLAG_BlockAnimatedWeaponReactions))
						{
							Vec3V vPosition(VECTOR3_TO_VEC3V(pResult->GetHitPosition()));
							Vec3V vDirection(Subtract(vPosition, VECTOR3_TO_VEC3V(vStart)));
							vDirection = NormalizeFast(vDirection);

							CIkRequestBodyReact bodyReactRequest(vPosition, vDirection, (int)pResult->GetHitComponent());
							bodyReactRequest.SetWeaponGroup(pWeaponInfo->GetGroup());
							pHitPed->GetIkManager().Request(bodyReactRequest);

							// If player ped, play facial pain anim.  Non-player peds get pain animations kicked off from TaskNMShot.
							if (pHitPed->GetFacialData())
							{
								pHitPed->GetFacialData()->PlayPainFacialAnim(pHitPed);
							}
						}
					}
					else
					{
						if(pHitPed->GetPedIntelligence()->NetworkLocalHitReactionsAllowed())
						{
							pHitPed->GetPedIntelligence()->AddLocalCloneTask(pTaskCopy, PED_TASK_PRIORITY_EVENT_RESPONSE_TEMP);
							bTaskUsed = true;
						}
					}
				}
				else if(uWeaponHash == WEAPONTYPE_DLC_SNOWBALL && pTaskCopy->GetTaskType() == CTaskTypes::TASK_NM_CONTROL)
				{
					if(pHitPed->GetPedIntelligence()->NetworkLocalHitReactionsAllowed())
					{
						pHitPed->GetPedIntelligence()->AddLocalCloneTask(pTaskCopy, PED_TASK_PRIORITY_EVENT_RESPONSE_TEMP);
						bTaskUsed = true;
					}
				}

				if (!bTaskUsed)
				{
					delete pTaskCopy;
				}
			}
		}

		if (tempDamageEvent.RequiresAbortForRagdoll() && !pHitPed->GetUsingRagdoll() && !bDeferRagdollActivation)
		{
			pHitPed->SwitchToRagdoll(tempDamageEvent);
		}
		else if (!pHitPed->IsNetworkClone())
		{
			pHitPed->GetPedIntelligence()->AddEvent(tempDamageEvent);
		}
	}

	if (!bPhysicalResponseAdded && pTaskPhysicalResponse)
	{
		delete pTaskPhysicalResponse;
	}

	// Let the caller know how much damage was done.
	return (tempDamageEvent.GetDamageResponseData().m_fHealthLost + tempDamageEvent.GetDamageResponseData().m_fArmourLost);
}

////////////////////////////////////////////////////////////////////////////////

float CWeaponDamage::ApplyFallOffDamageModifier(const CEntity* pHitEntity, const CWeaponInfo* pWeaponInfo, const Vector3& vWeaponPos, const float fFallOffRangeModifier, const float fFallOffDamageModifier, const float fOrigDamage, CEntity* pFiringEntity)
{
	weaponAssert(pWeaponInfo);

	float fDamage = fOrigDamage;

	// Do not apply for projectiles that are being used in melee.
	bool bDoingMeleeWithProjectile = false;
	if (pFiringEntity && pFiringEntity->GetIsTypePed())
	{
		CPed* pFiringPed = static_cast<CPed*>(pFiringEntity);
		if (pFiringPed && pFiringPed->GetPedIntelligence() && (pFiringPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_MELEE) || pFiringPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsPerformingVehicleMelee))
			&& pWeaponInfo && pWeaponInfo->GetIsProjectile())
		{
			bDoingMeleeWithProjectile = true;
		}
	}

	if(pHitEntity && !pWeaponInfo->GetIsMelee() && !bDoingMeleeWithProjectile)
	{
		Vector3 vDistance = VEC3V_TO_VECTOR3(pHitEntity->GetTransform().GetPosition()) - vWeaponPos;
		float fDistance = vDistance.Mag();

		const float fFallOffRangeMin = pWeaponInfo->GetDamageFallOffRangeMin() * fFallOffRangeModifier;
		const float fFallOffRangeMax = pWeaponInfo->GetDamageFallOffRangeMax() * fFallOffRangeModifier;
		const float fFallOffDamageMod = pWeaponInfo->GetDamageFallOffModifier() * fFallOffDamageModifier;
		if(fDistance > fFallOffRangeMax)
		{
			fDamage *= fFallOffDamageMod;
		}
		else if(fDistance > fFallOffRangeMin)
		{
			float fFallOff = (fDistance - fFallOffRangeMin) / (fFallOffRangeMax - fFallOffRangeMin);
			fDamage = Lerp(fFallOff, fDamage, fDamage * fFallOffDamageMod);
		}
	}

	return fDamage;
}

////////////////////////////////////////////////////////////////////////////////

f32 CWeaponDamage::DoWeaponImpactPed(CWeapon* pWeapon, CEntity* pFiringEntity, const Vector3& vWeaponPos, WorldProbe::CShapeTestHitPoint* pResult, const f32 fApplyDamage, const bool bTemporaryNetworkWeapon, const fwFlags32& flags, const sMeleeInfo *pMeleeInfo, const float fRecoilAccuracyWhenFired, bool* bGenerateVFX, u8 damageAggregationCount)
{
	weaponDebugf3("CWeaponDamage::DoWeaponImpactPed");

	// Get the ped we've hit
	CEntity* pHitEntity = CPhysics::GetEntityFromInst(pResult->GetHitInst());
	weaponFatalAssertf(pHitEntity && pHitEntity->GetIsTypePed(), "pHitEntity is NULL or not a Ped");
	CPed* pHitPed = static_cast<CPed*>(pHitEntity);
	bool bHitPedWasDead = (pHitPed && pHitPed->ShouldBeDead());

	if (flags.IsFlagSet( CPedDamageCalculator::DF_VehicleMeleeHit ))
	{
		pHitPed->SetPedResetFlag(CPED_RESET_FLAG_WasHitByVehicleMelee, true);
	}

	f32 fAppliedDamage = fApplyDamage;
	if(pFiringEntity && NetworkUtils::IsNetworkClone(pFiringEntity))
	{
		u32 nIndexFound = 0;

		if(pHitPed->IsNetworkClone())
		{
			//! Network damage events need to generate the audio event as this is done in locally run (non clone ped) code.
			if(bTemporaryNetworkWeapon && pMeleeInfo && pResult && flags.IsFlagSet( CPedDamageCalculator::DF_MeleeDamage ) && pFiringEntity->GetIsTypePed())
			{
				CPed* pFiringPed = static_cast<CPed*>( pFiringEntity );
				const CActionResult *pActionResult = ACTIONMGR.FindActionResult( nIndexFound, pMeleeInfo->uParentMeleeActionResultID );
				if (pActionResult)
				{
					pFiringPed->GetPedAudioEntity()->PlayMeleeCombatHit( pActionResult->GetSoundID(), *pResult, pActionResult  BANK_ONLY(, pActionResult->GetName() ) );	
				}
			}

			// Clone has hit Clone ped so don't allow setting damage or progressing to set ragdoll on the hit ped on this machine
			return 0.0f;
		}

		// Decide which damage event gets to do melee damage.
		if(flags.IsFlagSet( CPedDamageCalculator::DF_MeleeDamage ) && pFiringEntity->GetIsTypePed())
		{
			CPed* pFiringPed = static_cast<CPed*>( pFiringEntity );

			static dev_bool s_bCacheRemoteMeleeDamage = true;

			bool bDoMeleeDamage = false;

			if (flags.IsFlagSet( CPedDamageCalculator::DF_VehicleMeleeHit) && pFiringEntity->GetIsTypePed())
			{
				CPed* pFiringPed = static_cast<CPed*>( pFiringEntity );

				CTaskMountThrowProjectile* pProjTask = static_cast<CTaskMountThrowProjectile*>(pFiringPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_MOUNT_THROW_PROJECTILE));

				if (pProjTask && pFiringPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsPerformingVehicleMelee))
				{
					if (!pProjTask->EntityAlreadyHitByResult(pHitEntity))
					{
						if (pProjTask->GetIsCloneInDamageWindow())
						{
							bDoMeleeDamage = true;
							pProjTask->AddToHitEntityCount(pHitPed);
							pFiringPed->GetPedAudioEntity()->PlayBikeMeleeCombatHit(*pResult);
						}
						// Cache off damage to wait for correct time
						else
						{
							CTaskMountThrowProjectile::CacheMeleeNetworkDamage(pHitPed,
								pFiringPed,
								pResult->GetHitPosition(),
								pResult->GetHitComponent(),
								fAppliedDamage, 
								flags, 
								pMeleeInfo->uParentMeleeActionResultID,
								pMeleeInfo->uForcedReactionDefinitionID,
								pMeleeInfo->uNetworkActionID,
								pResult->GetHitMaterialId());
						}
					}
				}
				else
				{
					// Just allow clone to damage
					bDoMeleeDamage = true;
				}
			}
			else if(s_bCacheRemoteMeleeDamage)
			{
				bool bRemoteHitFromClonePed = bTemporaryNetworkWeapon;		

				if(flags.IsFlagSet(CPedDamageCalculator::DF_AllowCloneMeleeDamage))
				{
					bDoMeleeDamage = true;
				}
				else if(bRemoteHitFromClonePed)
				{
					u16 meleeID = pMeleeInfo ? pMeleeInfo->uNetworkActionID : 0;

					//! Get melee task this ped.
					CTaskMeleeActionResult* pFiringPedMeleeActionResultTask = pFiringPed->GetPedIntelligence()->GetTaskMeleeActionResult();
					if(pFiringPedMeleeActionResultTask &&  
						(pFiringPedMeleeActionResultTask->GetUniqueNetworkActionID() == meleeID) && 
						(pFiringPedMeleeActionResultTask->GetState() <= CTaskMeleeActionResult::State_PlayClip) && 
						pResult && 
						pFiringPedMeleeActionResultTask->IsOffensiveMove())
					{
						if(!pFiringPedMeleeActionResultTask->EntityAlreadyHitByResult(pHitPed))
						{
							if(pFiringPedMeleeActionResultTask->ShouldProcessMeleeCollisions())
							{
								//! In damage window - apply damage.
								bDoMeleeDamage = true;
								pFiringPedMeleeActionResultTask->AddToHitEntityCount(pHitPed, pResult->GetHitComponent());
							}
							else if(pFiringPedMeleeActionResultTask->HasStartedProcessingCollisions() && !pFiringPedMeleeActionResultTask->ShouldProcessMeleeCollisions())
							{
								//! Passed damage window. Apply damage. 
								//! Note: may want some special logic in here.
								bDoMeleeDamage = true;
								pFiringPedMeleeActionResultTask->AddToHitEntityCount(pHitPed, pResult->GetHitComponent());
							}
							else
							{
								//! Pass damage event on to melee task to process when it can.
								CTaskMeleeActionResult::CacheMeleeNetworkDamage(pHitPed,
									pFiringPed,
									pResult->GetHitPosition(),
									pResult->GetHitComponent(),
									fAppliedDamage, 
									flags, 
									pMeleeInfo->uParentMeleeActionResultID,
									pMeleeInfo->uForcedReactionDefinitionID,
									pMeleeInfo->uNetworkActionID);
							}
						}
					}
					else
					{
						//! Ped isn't doing melee attack anymore, so won't be synced up. Is this ok?
						bDoMeleeDamage = true;
					}
				}
			}
			else
			{
				bDoMeleeDamage = true;
			}

			//! Network damage events need to generate the audio event as this is done in locally run (non clone ped) code.
			const CActionResult *pActionResult = ACTIONMGR.FindActionResult( nIndexFound, pMeleeInfo->uParentMeleeActionResultID );
			if(bTemporaryNetworkWeapon && pMeleeInfo && pResult && pActionResult)
			{		
				pFiringPed->GetPedAudioEntity()->PlayMeleeCombatHit( pActionResult->GetSoundID(), *pResult, pActionResult  BANK_ONLY(, pActionResult->GetName() ) );							
			}
					
			if(!bDoMeleeDamage)
			{
				return 0.0f;
			}
		}

		// A shot generated by a clone ped is not allowed to damage or effect the ped (except dead peds)
		// If m_temporaryNetworkWeapon is set then this impact has been generated by a damage
		// event from another machine and is therefore allowed to do the proper damage locally
		else if(!bTemporaryNetworkWeapon && !(pHitPed && pHitPed->GetIsDeadOrDying()))
		{
			if (pWeapon && pWeapon->GetWeaponInfo()->GetGroup() == WEAPONGROUP_SHOTGUN)
				fAppliedDamage = 0.0f;
			else
				return 0.0f;
		}
	}

	SetLastSignificantShotBone(pWeapon, pHitPed, pResult);

	Vector3 vStart = vWeaponPos;
	f32 fDamageResult = GeneratePedDamageEvent(pFiringEntity, pHitPed, pWeapon->GetWeaponHash(), fAppliedDamage, vStart, pResult, flags, pWeapon, pResult ? pResult->GetHitComponent() : -1, 0.0f, pMeleeInfo, false, fRecoilAccuracyWhenFired, bTemporaryNetworkWeapon, damageAggregationCount);

	// Only damage bodypart if it is actually damaged
	if (fDamageResult > 0.f && pResult)
	{
		// I am using this to also decide if the ped should bleed out or not
		if (pFiringEntity && 
			pFiringEntity->GetIsTypePed() &&
			pWeapon->GetWeaponInfo()->GetEffectGroup() >= WEAPON_EFFECT_GROUP_PISTOL_SMALL && // Don't accept melee
			pHitPed->GetWeaponDamageEntity())	
		{
			CPed* pFiringPed = (CPed*)pFiringEntity;
			if (!pHitPed->GetPedIntelligence()->IsFriendlyWith(*pFiringPed))
			{
				s32 iHitComponent = pResult->GetHitComponent();
				s32 iHitBoneTag = pHitPed->GetBoneTagFromRagdollComponent(iHitComponent);
				pHitPed->ReportBodyPartDamage(CPed::ConvertBoneToBodyPart(iHitBoneTag));

				if (!pHitPed->HasHurtStarted() && !pHitPed->IsBodyPartDamaged(CPed::DAMAGED_GOTOWRITHE) && pHitPed->CanBeInHurt() && !pHitPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DisableGoToWritheWhenInjured ) && !pHitPed->GetPedConfigFlag(CPED_CONFIG_FLAG_CanBeArrested))
				{
					if (pHitPed->IsBodyPartDamaged(CPed::DAMAGED_LEFT_LEG | CPed::DAMAGED_RIGHT_LEG))
						pHitPed->ReportBodyPartDamage(CPed::DAMAGED_GOTOWRITHE);

					if (pWeapon->GetWeaponInfo()->GetEffectGroup() == WEAPON_EFFECT_GROUP_STUNGUN)
						pHitPed->ReportBodyPartDamage(CPed::DAMAGED_GOTOWRITHE | CPed::DAMAGED_STUN);

					// If you get shot whilst falling over we roll the writhe dice for every hit
					static dev_float WRITHE_CHANCE = 0.5f;
					if (fwRandom::GetRandomNumberInRange(0.f, 1.f) < WRITHE_CHANCE)
					{
						CTaskNMControl* pNMTask = smart_cast<CTaskNMControl*>(pHitPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_NM_CONTROL));
						if (pNMTask && pNMTask->IsFeedbackFlagSet(CTaskNMControl::BALANCE_FAILURE))
							pHitPed->ReportBodyPartDamage(CPed::DAMAGED_GOTOWRITHE);

					}
				}
			}
		}
	}

	// Intelligence
	if(pWeapon && (pWeapon->GetDamageType() == DAMAGE_TYPE_BULLET))
	{
		pHitPed->GetPedIntelligence()->SetLastTimeShot(fwTimer::GetTimeInMilliseconds());
	}

	// Player dialogue
	if(pFiringEntity && pFiringEntity->GetIsTypePed())
	{
		CPed* pFiringPed = static_cast<CPed*>(pFiringEntity);
		if(pFiringPed->IsLocalPlayer() && pFiringPed->GetSpeechAudioEntity())
		{
			if(!bHitPedWasDead && pHitPed->ShouldBeDead() && pHitPed->GetPedType() != PEDTYPE_ANIMAL)
			{
				// We are actually killing the last guy and he is still considered in combat at this point so rather than checking for no peds we check for 1
				if(pFiringPed->GetPlayerInfo()->GetNumEnemiesInCombat() == 1)
				{
					pFiringPed->GetSpeechAudioEntity()->SayWhenSafe("KILLED_ALL");
				}
				/*else
				{
					pFiringPed->GetSpeechAudioEntity()->SayWhenSafe("STAY_DOWN");
				}*/
			}
		}
	}

	if(pFiringEntity && pFiringEntity->GetIsTypePed() && pWeapon && pWeapon->GetDamageType() == DAMAGE_TYPE_BULLET)
	{
		CPed* pFiringPed = static_cast<CPed*>(pFiringEntity);
		if(pFiringPed->IsPlayer())
		{
			// 'Flaming Bullet' Cheat
			if (pFiringPed->GetPlayerInfo()->GetPlayerResetFlags().IsFlagSet(CPlayerResetFlags::PRF_FIRE_AMMO_ON) && CFireManager::CanSetPedOnFire(pHitPed))
			{		
				g_fireMan.StartPedFire(pHitPed, pFiringPed, FIRE_DEFAULT_NUM_GENERATIONS, Vec3V(V_ZERO), BANK_ONLY(Vec3V(V_ZERO),) false, false, pWeapon ? pWeapon->GetWeaponHash() : 0);
			}

			if (pFiringPed->IsLocalPlayer() && pHitPed->GetNetworkObject() && !pHitPed->GetNetworkObject()->IsClone() && !pHitPed->GetNetworkObject()->IsPendingOwnerChange())
			{
				// if the local player is shooting at a local ped, prevent him from migrating to another machine for a while so that the shot reactions look better
				if (!PARAM_invincibleMigratingPeds.Get())
				{
					static_cast<CNetObjPed*>(pHitPed->GetNetworkObject())->KeepProximityControl();
				}
			}
		}

		// Projectile Impact (Flare Gun)
		if (pWeapon->GetWeaponInfo()->GetIsProjectile() && !flags.IsFlagSet(CPedDamageCalculator::DF_MeleeDamage))
		{
			const CAmmoProjectileInfo* pAmmoInfo = static_cast<const CAmmoProjectileInfo*>(pWeapon->GetWeaponInfo()->GetAmmoInfo());
			if (pAmmoInfo && pAmmoInfo->GetShouldSetOnFireOnImpact() && CFireManager::CanSetPedOnFire(pHitPed))
			{
				g_fireMan.StartPedFire(pHitPed, pFiringPed, FIRE_DEFAULT_NUM_GENERATIONS, Vec3V(V_ZERO), BANK_ONLY(Vec3V(V_ZERO),) false, false, pWeapon ? pWeapon->GetWeaponHash() : 0);
			}
		}
	}


	// Check if we need to register a hit with the damage overlay effect
	eDamageType damageType = DAMAGE_TYPE_UNKNOWN;
	if (pWeapon)
	{
		damageType = pWeapon->GetDamageType();
	}

	// Only interested if hit ped is the local player (and we're not transitioning)
	if (pHitPed->IsLocalPlayer() && !NetworkInterface::IsSessionLeaving() && !g_PlayerSwitch.IsActive())
	{
		// Check if we need to do endurance effects instead
		bool bIsEnduranceDamage = IsEnduranceDamage(pHitPed, pWeapon, flags);
		if (damageType == DAMAGE_TYPE_BULLET || damageType == DAMAGE_TYPE_EXPLOSIVE)
		{
			PostFX::RegisterBulletImpact(vWeaponPos, 1.0f, bIsEnduranceDamage);
		}
		else if ((damageType == DAMAGE_TYPE_MELEE) && pFiringEntity && pFiringEntity->GetIsTypePed())
		{
			Vector3 vFiringPedPos = VEC3V_TO_VECTOR3(pFiringEntity->GetTransform().GetPosition());
			PostFX::RegisterBulletImpact(vFiringPedPos, 1.0f, bIsEnduranceDamage);
		}
	}

	if (bGenerateVFX && pHitPed->IsPlayer() && pHitPed->IsNetworkClone())
	{
		// Don't display any VFX on invincible respawning players
		CNetObjPlayer* netObjPlayer = static_cast<CNetObjPlayer *>(pHitPed->GetNetworkObject());
		if(netObjPlayer && netObjPlayer->GetRespawnInvincibilityTimer() > 0)
		{
			*bGenerateVFX = false;
		}
	}

	return fDamageResult;
}

////////////////////////////////////////////////////////////////////////////////
dev_float dfMaxMeleeDeform = 0.15f;
dev_float MELEE_WEAPON_VEHICLE_DEFORM_MULT = 0.0025f;
const float FORCE_THRESHOLD_FOR_HIGH_UPDATE = 1000.0f;
const unsigned BLENDER_OVERRIDE_LENGTH = 200;
f32 CWeaponDamage::DoWeaponImpactCar(CWeapon* pWeapon, CEntity* pFiringEntity, const Vector3& vWeaponPos, WorldProbe::CShapeTestHitPoint* pResult, const f32 fApplyDamage, const bool bFireDriveby, const bool bTemporaryNetworkWeapon, const fwFlags32& flags,  Vector3* hitDirOut /* = 0 */, const Vector3* hitDirIn /* = 0 */, bool bShouldApplyDamageToGlass /* = true */)
{

	CEntity* pHitEntity = CPhysics::GetEntityFromInst(pResult->GetHitInst());
	weaponFatalAssertf(pHitEntity && pHitEntity->GetIsTypeVehicle(), "pHitEntity is NULL or not a Vehicle");
	CVehicle* pHitVehicle = static_cast<CVehicle*>(pHitEntity);

	bool bFirerInSameVehicleAsHitEntity = false;
	if (NetworkInterface::IsGameInProgress())
	{
		if(fApplyDamage > FORCE_THRESHOLD_FOR_HIGH_UPDATE)
		{
			if(pHitVehicle->IsNetworkClone())
			{
				NetworkInterface::OverrideNetworkBlenderForTime(static_cast<CPhysical*>(pHitEntity), BLENDER_OVERRIDE_LENGTH);
			}
			else
			{
				NetworkInterface::ForceHighUpdateLevel(static_cast<CPhysical*>(pHitEntity));
			}
		}

		if (pFiringEntity && pFiringEntity->GetIsTypePed())
		{
			CPed* pFiringPed = static_cast<CPed*>(pFiringEntity);
			if (pFiringPed && pFiringPed->GetIsInVehicle() && (pFiringPed->GetMyVehicle() == pHitVehicle))
				bFirerInSameVehicleAsHitEntity = true;
		}

		if (!bFirerInSameVehicleAsHitEntity)
		{
			// A local shot hitting a clone is not allowed to damage or move it
			// or, a shot generated by a clone task running on a cloned entity.
			// If m_temporaryNetworkWeapon is set then this impact has been generated by a damage
			// event from another machine and is therefore allowed to do the proper damage locally
			//
			// Allow melee damage on remotes
			if( !flags.IsFlagSet( CPedDamageCalculator::DF_MeleeDamage ) )
			{
				if(NetworkUtils::IsNetworkCloneOrMigrating(pHitVehicle) || (pFiringEntity && NetworkUtils::IsNetworkClone(pFiringEntity) && !bTemporaryNetworkWeapon))
				{
					weaponDebugf3("(NetworkUtils::IsNetworkCloneOrMigrating(pHitVehicle) || (pFiringEntity && NetworkUtils::IsNetworkClone(pFiringEntity) && !bTemporaryNetworkWeapon))-->return 0.0f");
					return 0.0f;
				}
			}
		}
	}

	// Get direction of shot
	Vector3 vDir = pResult->GetHitPosition() - vWeaponPos;
	vDir.Normalize();
	if(hitDirOut)
	{
		*hitDirOut = vDir;
	}

	if(hitDirIn)
	{
		vDir = *hitDirIn;
	}
	
	// Compute the force
	f32 fShootForce;

	if( flags.IsFlagSet( CPedDamageCalculator::DF_MeleeDamage ) )
	{
		weaponDebugf3("DF_MeleeDamage");

		// For melee damage, we scale the force by the damage directly
		// This allows the melee system to specify weak and strong attacks
		// with the same weapon from the melee action table
		fShootForce = fApplyDamage * MELEE_WEAPON_FORCE_MULT;		

		// We want the deformation to be visually the same regardless of mass
		f32 fScaleFactor = MELEE_WEAPON_VEHICLE_DEFORM_MULT * pHitVehicle->GetMass();
#if __BANK	
		if( CVehicleDeformation::ms_fWeaponImpactDamageScale != 0.0f )
		{
			fScaleFactor *= CVehicleDeformation::ms_fWeaponImpactDamageScale;
		}
#endif
		f32 fDeformCapMult = 1.0f;
		if(pHitVehicle->GetVehicleDamage()->GetDeformation()->HasDamageTexture())
		{
			void* pTexLock = pHitVehicle->GetVehicleDamage()->GetDeformation()->LockDamageTexture(grcsRead);
			if(pTexLock)
			{
				Vector3 vHitOffset = VEC3V_TO_VECTOR3(pHitVehicle->GetTransform().UnTransform(VECTOR3_TO_VEC3V(pResult->GetHitPosition())));
				Vector3 vCurrentDeformation = VEC3V_TO_VECTOR3(pHitVehicle->GetVehicleDamage()->GetDeformation()->ReadFromVectorOffset(pTexLock, VECTOR3_TO_VEC3V(vHitOffset)));
				fDeformCapMult = 1.0f - Clamp(vCurrentDeformation.Mag() / dfMaxMeleeDeform, 0.0f, 1.0f);
				pHitVehicle->GetVehicleDamage()->GetDeformation()->UnLockDamageTexture();
			}
		}

		// Do vehicle deformation for melee damage
		if(pHitVehicle->GetVehicleDamage()->GetDeformation()->ApplyCollisionImpact(fScaleFactor * fDeformCapMult * fShootForce * vDir, pResult->GetHitPosition(), pFiringEntity, false, bShouldApplyDamageToGlass))
		{
			// If we deformed the vehicle due to melee, make sure it gets to update on the next frame, otherwise
			// the deformation may be delayed due to timeslicing.
			// Note: possibly we could move this into ApplyCollisionImpact(), but for now it's safer to do it outside
			// since we don't know about problems in any other cases than deformation from melee, and we have to be
			// careful to not make things slower in other vehicle damage cases (during which we may already have
			// performance issues, e.g. during explosions and high speed car crashes).
			pHitVehicle->m_nVehicleFlags.bLodForceUpdateThisTimeslice = true;
		}

		// Add a shocking event for the damage if the inflictor is a player.
		if (pFiringEntity && pFiringEntity->GetIsTypePed())
		{
			CPed* pFiringPed = static_cast<CPed*>(pFiringEntity);
			if (pFiringPed->IsAPlayerPed())
			{
				CEventShockingPropertyDamage event(*pFiringPed, pHitVehicle);
				CShockingEventsManager::Add(event);
			}
		}
	}
	else
	{
		if(pHitVehicle->InheritsFromHeli() || pHitVehicle->InheritsFromBoat())
		{
			DoWeaponImpactSearchLight(pHitEntity, pResult->GetHitPosition());

			if(pHitVehicle->IsInAir(false))
			{
				fShootForce = pWeapon->GetWeaponInfo()->GetForceHitFlyingHeli();
			}
			else
			{
				fShootForce = pWeapon->GetWeaponInfo()->GetForceHitVehicle();
			}
		}
		else
		{
			fShootForce = pWeapon->GetWeaponInfo()->GetForceHitVehicle();
		}
	}

	// Scale to some base vehicle weight (<1200kg) so we don't shoot light vehicles across the map
	static dev_float WEAPON_VEHICLE_MASS_CAP_MIN = 1000.0f;
	static dev_float WEAPON_VEHICLE_MASS_CAP_MAX = 2000.0f;
	static dev_float WEAPON_VEHICLE_MASS_REDUCTION_MIN = 0.25f;
	static dev_float WEAPON_VEHICLE_NO_WHEELS_REDUCTION = 0.15f;

	const float fMass = pHitVehicle->GetMass();
	float fShootForceMod = (rage::Max(fMass, WEAPON_VEHICLE_MASS_CAP_MIN)-WEAPON_VEHICLE_MASS_CAP_MIN)/(WEAPON_VEHICLE_MASS_CAP_MAX-WEAPON_VEHICLE_MASS_CAP_MIN);
	fShootForceMod = WEAPON_VEHICLE_MASS_REDUCTION_MIN + fShootForceMod*(1.0f-WEAPON_VEHICLE_MASS_REDUCTION_MIN);
	fShootForce *= rage::Min(1.0f, rage::Min(fShootForceMod, fMass / WEAPON_VEHICLE_MASS_CAP_MIN));

	bool bPowerfulWeapon = false;
	if (pFiringEntity && pFiringEntity->GetIsTypePed())
	{
		CPedWeaponManager* pWeaponMgr = static_cast<CPed*>(pFiringEntity)->GetWeaponManager();
		if (pWeaponMgr && pWeaponMgr->GetEquippedWeaponHash() == WEAPONTYPE_MINIGUN)
		{
			bPowerfulWeapon = true;
		}
	}

	// Scale down bullet impulses applied to vehicles, relative to how they align with the vehicle's velocity.  This fixes a lot of the 
	// apparent inconsistencies with how bullets are effecting a vehicle's acceleration.
	static dev_float sfSpeedRangeMax = 3.0f;
	static dev_float sfNormalWeaponSpeedMultMax = 1.0f;
	static dev_float sfPowerfulWeaponSpeedMultMax = 1.8f;
	const Vector3 vHitVehicleVelocity = pHitVehicle->GetVelocity();
	float fSpeedMultMax = bPowerfulWeapon ? sfPowerfulWeaponSpeedMultMax : sfNormalWeaponSpeedMultMax;
	float fVelocityInDirOfBullet = DotProduct(vHitVehicleVelocity, vDir);
	fShootForce *= Lerp(ClampRange(fVelocityInDirOfBullet, 0.0f, sfSpeedRangeMax), fSpeedMultMax, 0.0f);

	// If a vehicle is resting upside down or on its side reduce the force applied
	if( !pHitVehicle->GetIsAircraft() && !pHitVehicle->GetIsAquatic() && (pHitVehicle->HasContactWheels() == false) )
	{
		fShootForce *= WEAPON_VEHICLE_NO_WHEELS_REDUCTION;
	}

	static dev_float WEAPON_BIKE_FORCE_REDUCTION = 0.1f;
	static dev_float WEAPON_STANDING_TRAILER_FORCE_REDUCTION = 0.2f;
	if(pHitVehicle->InheritsFromBike())
	{
		fShootForce *= WEAPON_BIKE_FORCE_REDUCTION;
	}
	else if(pHitVehicle->InheritsFromTrailer())
	{
		if(static_cast<CTrailer*>(pHitVehicle)->AreTrailerLegsLoweredFully())
		{
			// Even with shrinking the angular impulse the friction on the contacts of the trailer legs will cause the
			//   trailer to tip unrealistically since the trailer isn't that heavy. 
			fShootForce *= WEAPON_STANDING_TRAILER_FORCE_REDUCTION;
		}
	}

	if(pHitVehicle->GetVehicleModelInfo())
	{
		fShootForce *= pHitVehicle->GetVehicleModelInfo()->GetWeaponForceMult();
	}

	bool bApplyForce = true;
	// Dont apply force when hitting the windows
	if( PGTAMATERIALMGR->GetMtlFlagShootThrough(pResult->GetHitMaterialId()) )
	{
		bApplyForce = false;
	}

	//don't apply a force for moving superdummy vehicles, it causes them to activate and 
	//go a little crazy
	if (pHitVehicle->IsSuperDummy() && vHitVehicleVelocity.Mag2() > 1.0f)
	{
		bApplyForce = false;
	}

	float fDamage = fApplyDamage;
	if(pWeapon->GetWeaponInfo()->GetGroup() == WEAPONGROUP_SHOTGUN)
	{
		TUNE_GROUP_FLOAT(WEAPON_TUNE, SHOTGUN_VEHICLE_DAMAGE_MODIFIER, 0.5f, 0.1f, 2.f, 0.01f);
		fDamage *= SHOTGUN_VEHICLE_DAMAGE_MODIFIER;
	}

	eDamageType eFixedDamageType = pWeapon->GetDamageType();
	
	// Stop melee attacks from grenade launchers passing through DAMAGE_TYPE_EXPLOSIVE
	if(eFixedDamageType == DAMAGE_TYPE_EXPLOSIVE && flags.IsFlagSet(CPedDamageCalculator::DF_MeleeDamage))
	{
		// Weapons apply DAMAGE_TYPE_BULLET for melee attacks
		// DAMAGE_TYPE_MELEE only applies a sliver of damage, making it nearly impossible to dent a vehicle
		eFixedDamageType = DAMAGE_TYPE_BULLET;
	}

	bool bMeleeDamage = flags.IsFlagSet(CPedDamageCalculator::DF_MeleeDamage) || flags.IsFlagSet(CPedDamageCalculator::DF_VehicleMeleeHit);
	// Apply damage to the health of the various components of the vehicle
	f32 fDamageResult = pHitVehicle->GetVehicleDamage()->ApplyDamage(pFiringEntity, eFixedDamageType, pWeapon->GetWeaponHash(), fDamage, pResult->GetHitPosition(), pResult->GetHitNormal(), vDir, pResult->GetHitComponent(), pResult->GetHitMaterialId(), pResult->GetHitPartIndex(), bFireDriveby, flags.IsFlagSet( CPedDamageCalculator::DF_IsAccurate ), 0.0f, false, false, bMeleeDamage );

	bool bPlayerResponsible = false;
	bool bTurretResponsible = false;
	bool bNoForceDueToHitCover = false;
	if(pFiringEntity)
	{
		if(pFiringEntity->GetIsTypePed() && static_cast<CPed*>(pFiringEntity)->IsPlayer())
		{
			bPlayerResponsible = true;

			//Don't apply force to my cover object
			CPed* pFiringPlayer = static_cast<CPed*>(pFiringEntity);
			if (pFiringPlayer->GetIsInCover())
			{
				CCoverPoint* pCoverPoint = pFiringPlayer->GetCoverPoint();
				if (pCoverPoint && pCoverPoint->GetEntity() == pHitVehicle && bApplyForce)
				{
					bNoForceDueToHitCover = true;
					bApplyForce = false;
				}
			}
		}
		else if(pFiringEntity->GetIsDynamic() && static_cast<CDynamicEntity*>(pFiringEntity)->GetStatus() == STATUS_PLAYER)
		{
			bPlayerResponsible = true;
		}
		
		// Also apply force if a turret vehicle weapon is the firing entity
		if (pFiringEntity->GetIsTypeVehicle())
		{
			const CWeaponInfo* pWeaponInfo = pWeapon->GetWeaponInfo();
			if (pWeaponInfo && pWeaponInfo->GetIsTurret())
			{
				bTurretResponsible = true;
			}
		}
	}

	// Apply force to vehicle in original direction of damage (don't bother for non-player peds)
	if(bPlayerResponsible || bTurretResponsible)
	{
		Vec3V instancePosition = pHitVehicle->GetTransform().GetPosition();
		Vec3V instanceToCenterOfGravity;
		if(const fragInst* vehicleFragInst = pHitVehicle->GetFragInst())
		{
			instanceToCenterOfGravity = Transform3x3(pHitVehicle->GetTransform().GetMatrix(),vehicleFragInst->GetArchetype()->GetBound()->GetCGOffset());

			// If this vehicle is articulated, use the  CG of the link we hit
			if(const fragCacheEntry* cacheEntry = vehicleFragInst->GetCacheEntry())
			{
				const fragHierarchyInst* hierInst = cacheEntry->GetHierInst();
				if(hierInst->articulatedCollider && hierInst->body)
				{
					int linkIndex = hierInst->articulatedCollider->GetLinkFromComponent(pResult->GetHitComponent());
					instanceToCenterOfGravity = Add(instanceToCenterOfGravity,hierInst->body->GetLink(linkIndex).GetPositionV());
				}
			}
		}
		else
		{
			instanceToCenterOfGravity = Vec3V(V_ZERO);
		}
		Vec3V centerOfGravity = Add(instancePosition,instanceToCenterOfGravity);

		// Scale the lever arm of the impulse, as large torques appear to have an overpowering effect on vehicles 
		static dev_float sfSpinScale = 0.1f;
		static dev_float sfRollScale = 0.2f;
		Vec3V offsetFromCG = Subtract(pResult->GetHitPositionV(),centerOfGravity);
		Vec3V offsetFromInstance = AddScaled(instanceToCenterOfGravity,offsetFromCG,Vec3V(sfSpinScale,sfSpinScale,sfRollScale));
		
		if(pHitVehicle->InheritsFromBike())
		{
			// If an on-foot melee attack from a player hits a bike in MP, knock off all peds and apply part of the weapon damage to each ped
			if(NetworkInterface::IsGameInProgress() && flags.IsFlagSet(CPedDamageCalculator::DF_MeleeDamage) && !flags.IsFlagSet(CPedDamageCalculator::DF_VehicleMeleeHit) 
				&& pFiringEntity->GetIsTypePed() && static_cast<CPed*>(pFiringEntity)->IsPlayer())
			{
				const CSeatManager* pSeatManager = pHitVehicle->GetSeatManager();
				if (pSeatManager)
				{
					for(s32 iSeat = 0; iSeat < pSeatManager->GetMaxSeats(); iSeat++)
					{
						CPed* pPassenger = pSeatManager->GetPedInSeat(iSeat);
						if(pPassenger)
						{
							pPassenger->KnockPedOffVehicle(false, fApplyDamage * 0.75f);
						}
					}
				}
			}			
			
			// Knock over bike
			CBike *pBike = static_cast<CBike*>(pHitVehicle);
			pBike->m_nBikeFlags.bOnSideStand = false;
			pBike->RemoveConstraint();
		}

		bool bApplyForceToChassis = false;

		// Exclude vehicle doors that are being used
		const fragInst* hitFragInst = pHitVehicle->GetFragInst();
		if (hitFragInst)
		{	
			if(hitFragInst->GetCacheEntry())
			{
				if(phArticulatedCollider* pArticulatedCollider = hitFragInst->GetCacheEntry()->GetHierInst()->articulatedCollider)
				{
					if(pArticulatedCollider->IsArticulated())
					{
						s32 iComponentIndex = pResult->GetHitComponent();
						int hitLink = pArticulatedCollider->GetLinkFromComponent(iComponentIndex);
						// If we hit the root link then we can't hit an articulated door
						if(hitLink != 0)
						{
							// Check if the hit component is on the same articulated link as the the door
							for (s32 doorIndex=0; doorIndex<pHitVehicle->GetNumDoors(); doorIndex++)
							{
								const CCarDoor* pDoor = pHitVehicle->GetDoor(doorIndex);
								s32 doorComponentIndex = pDoor->GetFragChild();
								if (doorComponentIndex > -1 && pArticulatedCollider->GetLinkFromComponent(doorComponentIndex) == hitLink)
								{
									if (pDoor->GetFlag(CCarDoor::PED_DRIVING_DOOR))
									{
										// A ped is using this door don't apply any force
										bApplyForce = false;
									}
									else if (pDoor->GetFlag(CCarDoor::PED_USING_DOOR_FOR_COVER))
									{
										// A ped is using this door as cover, if we apply the force to the door we may invalidate the
										// cover as it closes, instead apply force to chassis to get some rockin
										bApplyForceToChassis = true;
									}
									else if(bNoForceDueToHitCover)
									{
										// Player is hitting the articulated door of his cover vehicle, we still want to apply the force to that door
										bApplyForce = true;
									}

									fShootForce*=GUN_WEAPON_CAR_DOOR_MOD;
									break;
								}
							}

							// Check if the hit component is a turret
							if(pHitVehicle->GetVehicleWeaponMgr())
							{
								for(int i = 0; i < pHitVehicle->GetVehicleWeaponMgr()->GetNumTurrets(); i++)
								{
									CTurret *pTurret = pHitVehicle->GetVehicleWeaponMgr()->GetTurret(i);
									if(pTurret->GetType() == VGT_TURRET_PHYSICAL)
									{
										CTurretPhysical *pTurretPhysical = static_cast<CTurretPhysical*>(pTurret);
										s32 turretComponentIndex = pTurretPhysical->GetBaseFragChild();
										if(turretComponentIndex > -1 && pArticulatedCollider->GetLinkFromComponent(turretComponentIndex) == hitLink)
										{
											fShootForce*=GUN_WEAPON_CAR_TURRET_MOD;
											break;
										}
										s32 barrelComponentIndex = pTurretPhysical->GetBarrelFragChild();
										if(barrelComponentIndex > -1 && pArticulatedCollider->GetLinkFromComponent(barrelComponentIndex) == hitLink)
										{
											fShootForce*=GUN_WEAPON_CAR_TURRET_MOD;
											break;
										}

									}
								}
							}

						}
					}
				}
			}
		}

		// B*1950644: Prevent players that are in a vehicle and somehow shooting it (e.g. Players hanging on the sides of the Granger) from applying large forces that affect the handling.
		if(bFirerInSameVehicleAsHitEntity)
		{
			// Only do this if the vehicle is being actively driven by the throttle.
			if(abs(pHitVehicle->GetThrottle()) > 0.25f)
			{
				// Reduce the force so that it won't make the vehicle slow down or swerve off course, but still makes the vehicle rock a little for effect.
				TUNE_GROUP_FLOAT(VEHICLE_PASSENGER_WEAPON_DAMAGE, FORCE_MULTIPLER, 0.2f, 0.0f, 1.0f, 0.01f);
				fShootForce *= FORCE_MULTIPLER;
			}
		}

		if (bApplyForceToChassis)
		{
			pHitVehicle->ApplyExternalImpulse(fShootForce * vDir, RCC_VECTOR3(instanceToCenterOfGravity), 0);
		}
		else if (bApplyForce)
		{
			// GTAV B*1874706 - If the player is driving and trying to reverse but the impact is trying to push the car forward
			// reduce the force of the impact.
			if( pHitVehicle->IsDriverAPlayer()  )
			{
				if( vDir.GetY() > 0.0f &&
					pHitVehicle->GetThrottle() < -0.25f &&
					pHitVehicle->GetVelocity().Mag2() < 5.0f )
				{
					fShootForce *= 1.0f - ( vDir.GetY() * 0.8f );
				}
			}

// Hack for B*7543674 .. dont allow breaking parts from patriot3 
			if( eFixedDamageType == DAMAGE_TYPE_BULLET && pHitVehicle->GetModelId() == MI_CAR_PATRIOT3 )
			{
				fShootForce = 0.0f;
			}

			pHitVehicle->ApplyExternalImpulse(fShootForce * vDir, RCC_VECTOR3(offsetFromInstance), pResult->GetHitComponent());
		}				
	}

	// CHEATS - FLAMING BULLETS - CHEAT
	if(bPlayerResponsible)
	{
		CPed* pFiringPed = static_cast<CPed*>(pFiringEntity);
		if(pFiringPed->IsLocalPlayer() && pFiringPed->GetPlayerInfo()->GetPlayerResetFlags().IsFlagSet(CPlayerResetFlags::PRF_FIRE_AMMO_ON) && eFixedDamageType == DAMAGE_TYPE_BULLET)
		{
			const float fBurnTime     = fwRandom::GetRandomNumberInRange(20.0f, 35.0f);
			const float fBurnStrength = fwRandom::GetRandomNumberInRange(0.75f, 1.0f);
			const float fPeakTime	  = fwRandom::GetRandomNumberInRange(0.5f, 1.0f);

			fragInstGta* pVehicleFragInst = pHitVehicle->GetVehicleFragInst();
			if (pVehicleFragInst)
			{
				fragPhysicsLOD* pPhysicsLod = pVehicleFragInst->GetTypePhysics();
				if (pPhysicsLod)
				{
					fragTypeChild* pFragTypeChild = pPhysicsLod->GetChild(pResult->GetHitComponent());
					if (pFragTypeChild)
					{
						u16 boneId = pFragTypeChild->GetBoneID();

						CVehicleModelInfo* pModelInfo = pHitVehicle->GetVehicleModelInfo(); 
						if (pModelInfo)
						{
							gtaFragType* pFragType = pModelInfo->GetFragType();
							if (pFragType)
							{
								s32 boneIndex = pFragType->GetBoneIndexFromID(boneId);

								Vec3V vPosLcl;
								Vec3V vNormLcl;
								if (CVfxHelper::GetLocalEntityBonePosDir(*pHitVehicle, boneIndex, VECTOR3_TO_VEC3V(pResult->GetHitPosition()), VECTOR3_TO_VEC3V(vDir), vPosLcl, vNormLcl))
								{
									g_fireMan.StartVehicleFire(pHitVehicle, boneIndex, vPosLcl, vNormLcl, pFiringPed, fBurnTime, fBurnStrength, fPeakTime, FIRE_DEFAULT_NUM_GENERATIONS, Vec3V(V_ZERO), BANK_ONLY(Vec3V(V_ZERO),) false);
								}
							}
						}
					}
				}
			}
		}
	}

	return fDamageResult;
}

////////////////////////////////////////////////////////////////////////////////

f32 CWeaponDamage::DoWeaponImpactObject(CWeapon* pWeapon, CEntity* pFiringEntity, const Vector3& vWeaponPos, WorldProbe::CShapeTestHitPoint* pResult, const f32 fApplyDamage, const bool bTemporaryNetworkWeapon, const fwFlags32& flags, phMaterialMgr::Id hitMaterial)
{
	weaponDebugf3("CWeaponDamage::DoWeaponImpactObject pWeapon[%p] pFiringEntity[%p] vWeaponPos[%f %f %f] pResult[%p] fApplyDamage[%f] bTemporaryNetworkWeapon[%d] flags[0x%x]",pWeapon,pFiringEntity,vWeaponPos.x,vWeaponPos.y,vWeaponPos.z,pResult,fApplyDamage,bTemporaryNetworkWeapon,(s32)flags);

	CEntity* pHitEntity = CPhysics::GetEntityFromInst(pResult->GetHitInst());
	weaponFatalAssertf(pHitEntity && pHitEntity->GetIsTypeObject(), "pHitEntity is NULL or not a Object");
	CObject* pHitObj = static_cast<CObject*>(pHitEntity);

	// Check for fragments that have broken apart since this impact was generated
	fragInst* pFragInst = pHitObj->GetFragInst();
	if(pFragInst && pFragInst->GetChildBroken(pResult->GetHitComponent()))
	{
		weaponDebugf3("CWeaponDamage::DoWeaponImpactObject--(pFragInst && pFragInst->GetChildBroken(pResult->GetHitComponent())) pHitEntity[%p][%s]-->return 0.0f",pHitEntity,pHitEntity ? pHitEntity->GetModelName() : "");
		return 0.0f;
	}

	bool bCanDamageObject = true;

	// A remote shot hitting a clone is not allowed to damage or move it. Also, remote shots are not permitted to start networking an object.
	// If m_temporaryNetworkWeapon is set then this impact has been generated by a damage
	// event from another machine and is therefore allowed to do the proper damage locally
	if (pFiringEntity && NetworkUtils::IsNetworkClone(pFiringEntity) && !bTemporaryNetworkWeapon)
	{
		if( (pHitObj->GetNetworkObject() || CNetObjObject::HasToBeCloned(pHitObj)) && !pHitObj->IsADoor())
		{
			weaponDebugf3("CWeaponDamage::DoWeaponImpactObject--(pFiringEntity && NetworkUtils::IsNetworkClone(pFiringEntity) && !bTemporaryNetworkWeapon) pHitEntity[%p][%s]-->return 0.0f",pHitEntity,pHitEntity ? pHitEntity->GetModelName() : "");
			return 0.0f;
		}
	}

	// Remote script objects can never be damaged
	if(NetworkUtils::IsNetworkCloneOrMigrating(pHitObj))
	{
		if (pHitObj->GetNetworkObject()->IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_SCRIPTOBJECT))
		{
			//Dont allow weapon damage to script objects: B*1931368
			weaponDebugf3("CWeaponDamage::DoWeaponImpactObject--IsNetworkCloneOrMigrating && (pHitObj->GetNetworkObject()->IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_SCRIPTOBJECT)) pHitEntity[%p][%s]-->return 0.0f",pHitEntity,pHitEntity ? pHitEntity->GetModelName() : "");
			return 0.0f;
		}

		bCanDamageObject = false;
	}

	weaponDebugf3("CWeaponDamage::DoWeaponImpactObject--bCanDamageObject[%d]",bCanDamageObject);
	if(bCanDamageObject)
	{
		Vector3 vDamagePos = pResult->GetHitPosition();
		Vector3 vDamageNorm = pResult->GetHitNormal();

		if (!pHitObj->ObjectDamage(fApplyDamage, &vDamagePos, &vDamageNorm, pFiringEntity, pWeapon->GetWeaponHash(), hitMaterial))
		{
			weaponDebugf3("CWeaponDamage::DoWeaponImpactObject--pHitObj->ObjectDamage() returned false");
			if( pHitObj->GetWeaponImpactsApplyGreaterForce() )
			{
				TUNE_GROUP_FLOAT( ARENA_MODE, sf_ScaleWeaponImpact, 8.0f, 0.0f, 25.0f, 0.1f );
				bool isMeleeAttack = flags.IsFlagSet( CPedDamageCalculator::DF_MeleeDamage );
				f32 fShootForce = isMeleeAttack ? fApplyDamage * MELEE_WEAPON_FORCE_MULT : pWeapon->GetWeaponInfo()->GetForce();
				fShootForce *= sf_ScaleWeaponImpact;
				Vector3 vDir = pResult->GetHitPosition() - vWeaponPos;
				vDir.Normalize();
				f32 fFragImpulse = pWeapon->GetWeaponInfo()->GetFragImpulse();

				pHitObj->ApplyExternalImpulse( fShootForce * vDir, pResult->GetHitPosition() - VEC3V_TO_VECTOR3( pHitObj->GetTransform().GetPosition() ), pResult->GetHitComponent(), pResult->GetHitPartIndex(), pResult->GetHitInst(), fFragImpulse, false );
			}
			
			return 0.0f;
		}

		DoWeaponImpactTrafficLight(pHitEntity, vDamagePos);
	}

	// Some pickups are collected by weapon impacts
	if(pHitObj->m_nObjectFlags.bIsPickUp && pFiringEntity && static_cast<CPickup*>(pHitObj)->ProcessWeaponImpact(pFiringEntity))
	{
		weaponDebugf3("CWeaponDamage::DoWeaponImpactObject--(pHitObj->m_nObjectFlags.bIsPickUp && pFiringEntity && static_cast<CPickup*>(pHitObj)->ProcessWeaponImpact(pFiringEntity)) pHitEntity[%p][%s]-->return 0.0f",pHitEntity,pHitEntity ? pHitEntity->GetModelName() : "");
		return 0.0f;
	}

	//----

	// we're playing multiplayer...
	if(NetworkInterface::IsGameInProgress())
	{
		// something shot an object...
		if(pFiringEntity)
		{
			CBaseModelInfo const* pModelInfo = pHitObj->GetBaseModelInfo();

			// and shot an object with glass in it....
			if(pModelInfo->GetFragType() && pModelInfo->GetFragType()->GetNumGlassPaneModelInfos() > 0)
			{
				// and the firing object is local...
				if(!NetworkUtils::IsNetworkClone(pFiringEntity))
				{		
					// so we're taking control - it's up to us to flag how it gets damaged to the other machines so we registerWithNetwork
					CGlassPaneManager::RegisterGlassGeometryObject_OnNetworkOwner( pHitObj, pResult->GetHitPosition(), (u8)pResult->GetHitComponent() );
				}
			}
		}
	}

	//----

	// Shot force is proportional to weapon damage
	bool isMeleeAttack = flags.IsFlagSet( CPedDamageCalculator::DF_MeleeDamage );
	f32 fShootForce  = isMeleeAttack ? fApplyDamage * MELEE_WEAPON_FORCE_MULT : pWeapon->GetWeaponInfo()->GetForce();		
	f32 fFragImpulse = pWeapon->GetWeaponInfo()->GetFragImpulse();

	// Get the mass
	f32 fHitMass = pHitObj->GetMass();

	// If the other inst is a frag object (one with a tune file) then the prop
	// artist might have set up this frag not to be broken/damaged/moved by melee attacks or weapons.
	// Let's see if this is the case.			
	if(pHitObj->m_nFlags.bIsFrag)
	{
		if (!pFragInst->GetCached()) 
		{
			pFragInst->PutIntoCache();
		}

		fragTypeChild* pChild = pFragInst->GetTypePhysics()->GetChild(pResult->GetHitComponent());
		int groupIndex = pChild->GetOwnerGroupPointerIndex();
		fragTypeGroup* pGroup = pFragInst->GetTypePhysics()->GetGroup(groupIndex);
		if (fragCacheEntry* pEntry = pFragInst->GetCacheEntry())
		{
			TrapLT(groupIndex, 0);
			TrapGE(groupIndex, pFragInst->GetTypePhysics()->GetNumChildGroups());
			float& weaponHealth = pEntry->GetHierInst()->groupInsts[groupIndex].weaponHealth;
			weaponHealth -= fFragImpulse;
			if (weaponHealth < 0.0f)
			{
				// 
				weaponHealth = 0.0f;
			}
			else
			{
				// Weapon health is not depleted, stop any breaking from occurring
				fFragImpulse = 0.0f;
			}
		}
		
		fFragImpulse *= isMeleeAttack ? pGroup->GetMeleeScale() : pGroup->GetWeaponScale();

		// B*1848782: Make sure to apply the frag tuning ped inverse mass scales to melee attacks.
		// This prevents objects that have been tuned to not move (or move less) due to peds from moving when hit by melee attacks.
		if(pFiringEntity && pFiringEntity->GetType() == ENTITY_TYPE_PED && isMeleeAttack)
		{
			if(pGroup->GetPedInvMassScale() < 1.0f)
				fShootForce *= pGroup->GetPedInvMassScale();
		}

		// Get the mass
		fHitMass = pChild->GetUndamagedMass();
	}

	// Don't do any fragment breaking or damage if the object has been marked immune
	if(	(pHitObj->m_nPhysicalFlags.bNotDamagedByMelee && isMeleeAttack) ||
		(pHitObj->m_nPhysicalFlags.bNotDamagedByBullets && !isMeleeAttack))
	{
		fFragImpulse = 0.0f;
	}

	static dev_float TEST_WEAPON_OBJECT_DELTAV_CAP = 10.0f;
	if(fShootForce / fHitMass > TEST_WEAPON_OBJECT_DELTAV_CAP)
	{
		fShootForce = TEST_WEAPON_OBJECT_DELTAV_CAP * fHitMass;
	}

	// Apply force to object in original direction of bullet
	Vector3 vDir = pResult->GetHitPosition() - vWeaponPos;
	vDir.Normalize();

	// Apply door weapon impulse modifier, if there is one
	if (pHitObj->IsADoor())
	{
		const CDoor* pDoor = static_cast<CDoor*>(pHitObj);

		// NOTE: NULL check is probably superfluous here
		if (const CDoorTuning* pDoorTuning = pDoor->GetTuning())
		{
			fShootForce *= pDoorTuning->m_WeaponImpulseMultiplier;
		}
	}

	// We had some issues where melee attacks could sometimes send a prop flying (chair, stool, etc) with
	// a scenario ped sitting on it, at an unrealistic speed. Ideally, the physics system would take into
	// account the mass of the ped that's attached to the prop, but since we probably have nothing like that,
	// we try to scale down the force here instead, proportionally. This won't give us the right center of
	// gravity and all that, but should still help quite a bit.
	if(isMeleeAttack)
	{
		const float originalMass = pHitObj->GetMass();
		float massWithAttachments = originalMass;

		// Loop over the attachments.
		const fwEntity* pChildAttachment = pHitObj->GetChildAttachment();
		while(pChildAttachment)
		{
			if(static_cast<const CEntity*>(pChildAttachment)->GetIsTypePed())
			{
				// If this is a ped that's sitting...
				const CPed& attachedPed = *static_cast<const CPed*>(pChildAttachment);
				if(attachedPed.GetIsSitting())
				{
					// ... and is using a scenario point which is also attached to this entity...
					const CScenarioPoint* pScenarioPt = CPed::GetScenarioPoint(attachedPed, true);
					if(pScenarioPt && pScenarioPt->GetEntity() == pHitObj)
					{
						// ... add the mass of the ped.
						massWithAttachments += attachedPed.GetMass();
					}
				}
			}

			pChildAttachment = pChildAttachment->GetSiblingAttachment();
		}

		// Recompute the force to apply, as if the mass of the prop had included
		// the mass of its occupant(s).
		const float forceScale = originalMass/massWithAttachments;
		fShootForce *= forceScale;
	}

	if( pFiringEntity &&
		pFiringEntity->GetIsTypePed() &&
		pHitEntity->GetIsPhysical() )
	{
		CPed* pPed = static_cast<CPed*>(pFiringEntity);

		if( pPed->GetGroundPhysical() == pHitEntity && !static_cast< CDynamicEntity* >(pHitEntity)->m_nDEflags.bIsBreakableGlass)
		{
			fShootForce = 0.0f;
		}
	}

	if( fShootForce > 0.0f )
	{
		if( pHitObj->GetWeaponImpactsApplyGreaterForce() )
		{
			static dev_float sf_ScaleWeaponImpact = 10.0f;
			fShootForce *= sf_ScaleWeaponImpact;
		}
		pHitObj->ApplyExternalImpulse(fShootForce * vDir, pResult->GetHitPosition() - VEC3V_TO_VECTOR3( pHitObj->GetTransform().GetPosition()), pResult->GetHitComponent(), pResult->GetHitPartIndex(), pResult->GetHitInst(), fFragImpulse, false);
	}

	// Only tell network that object has moved, if it's actually become active
	if(pHitObj->GetCurrentPhysicsInst()->IsInLevel() && CPhysics::GetLevel()->IsActive(pHitObj->GetCurrentPhysicsInst()->GetLevelIndex()))
	{
		pHitObj->ObjectHasBeenMoved(pFiringEntity);
	}

	if (pFiringEntity && pFiringEntity->GetIsTypePed())
	{
		CPed* pAttacker = static_cast<CPed*>(pFiringEntity);
		const CAmmoInfo* pAmmoInfo = pWeapon->GetWeaponInfo() ? pWeapon->GetWeaponInfo()->GetAmmoInfo(pAttacker) : NULL;
		if (pAmmoInfo && pAmmoInfo->GetIsIncendiary())
		{
			s32 boneIndex = -1;
			if (pFragInst)
			{
				fragPhysicsLOD* pPhysicsLod = pFragInst->GetTypePhysics();
				if (pPhysicsLod)
				{
					fragTypeChild* pFragTypeChild = pPhysicsLod->GetChild(pResult->GetHitComponent());
					if (pFragTypeChild)
					{
						u16 boneId = pFragTypeChild->GetBoneID();

						CBaseModelInfo* pModelInfo = pHitObj->GetBaseModelInfo(); 
						if (pModelInfo)
						{
							gtaFragType* pFragType = pModelInfo->GetFragType();
							if (pFragType)
							{
								boneIndex = pFragType->GetBoneIndexFromID(boneId);
							}
						}
					}
				}
			}

			Vec3V vPosLcl;
			Vec3V vNormLcl;
			if (CVfxHelper::GetLocalEntityBonePosDir(*pHitObj, boneIndex, pResult->GetHitPositionV(), pResult->GetHitNormalV(), vPosLcl, vNormLcl))
			{
				TUNE_GROUP_FLOAT(WEAPON_SPECIAL_AMMO, fFireStartChance, 0.25f, 0.0f, 1.f, 0.1f);
				TUNE_GROUP_FLOAT(WEAPON_SPECIAL_AMMO, fFireBurnTime, 1.5f, 0.0f, 100.f, 0.1f);
				TUNE_GROUP_FLOAT(WEAPON_SPECIAL_AMMO, fFireBurnStrength, 0.5f, 0.0f, 100.f, 0.1f);
				TUNE_GROUP_FLOAT(WEAPON_SPECIAL_AMMO, fFireBurnPeakTime, 0.750f, 0.0f, 100.f, 0.1f);
				if (fwRandom::GetRandomNumberInRange(0.0f, 1.0f) <= fFireStartChance)
					g_fireMan.StartObjectFire(pHitObj, boneIndex, vPosLcl, vNormLcl, pAttacker, fFireBurnTime,fFireBurnStrength, fFireBurnPeakTime, FIRE_DEFAULT_NUM_GENERATIONS, pAttacker->GetTransform().GetPosition(), BANK_ONLY(pAttacker->GetTransform().GetPosition(),) false, pWeapon->GetWeaponHash());
			}
		}
	}

	weaponDebugf3("CWeaponDamage::DoWeaponImpactObject-->return fApplyDamage[%f]",fApplyDamage);
	return fApplyDamage;
}

void CWeaponDamage::DoWeaponImpactTrafficLight(CEntity* pHitEntity, const Vector3& vDamagePos)
{
	CBaseModelInfo* pModelInfo = pHitEntity->GetBaseModelInfo();
	if (pModelInfo->TestAttribute(MODEL_ATTRIBUTE_IS_TRAFFIC_LIGHT))
	{
		// see if we hit one of the lights and if so, disable it.
		const BaseModelInfoBoneIndices* pExtension = CTrafficLights::GetExtension(pModelInfo);
		if(pExtension == NULL )
			return;

		Mat34V matrix;
		int boneIdx;

		for (u8 boneId = TRAFFIC_LIGHT_0; boneId < TRAFFIC_LIGHT_COUNT; boneId++)
		{
			boneIdx = pExtension->GetBoneIndice(eTrafficLightComponentId(boneId));
			if (boneIdx == 0xff)
				continue;

			CVfxHelper::GetMatrixFromBoneIndex_Lights(matrix, pHitEntity, boneIdx);

			Vector3 boneLocation = VEC3V_TO_VECTOR3(matrix.d());
			f32 distance = fabs((boneLocation - vDamagePos).Mag());

			// if we're checking against a ped walk box, we don't need to loop through any other bones/offsets
			if (boneId == PED_WALK_BOX &&
				distance < TRAFFIC_LIGHT_PED_BOX_DAMAGE_RADIUS)
			{
				TrafficLightInfos *tli = TrafficLightInfos::Get(pHitEntity);

				if( NULL == tli )
				{	
					tli = TrafficLightInfos::Add(pHitEntity);
				}
					
				// TrafficLightInfos::Add can return NULL, if its pool is full.
				if( tli )
				{
					tli->DamageLight(boneId, 0);
				}
			}
			else if (distance < TRAFFIC_LIGHT_CORONA_BULLET_DAMAGE_RADIUS)
			{
				// the traffic lights are a lot taller than they are wide, so just make sure the x/y distance isn't too far off
				if (fabs(boneLocation.x - vDamagePos.x) < TRAFFIC_LIGHT_CORONA_VERTICLE_OFFSET &&
					fabs(boneLocation.y - vDamagePos.y) < TRAFFIC_LIGHT_CORONA_VERTICLE_OFFSET)
				{
					TrafficLightInfos *tli = TrafficLightInfos::Get(pHitEntity);

					if( NULL == tli )
					{	
						tli = TrafficLightInfos::Add(pHitEntity);
					}
					
					// TrafficLightInfos::Add can return NULL, if its pool is full.
					if( tli )
					{
						// we know we are damaging this light, we just need to figure out if the bullet hit closest to
						// the red, amber, or orange light so we know which one to damage.
						static float lightOffsets[] = { -TRAFFIC_LIGHT_CORONA_VERTICLE_OFFSET, 0.0f, TRAFFIC_LIGHT_CORONA_VERTICLE_OFFSET };

						u8 lightId;
						for (lightId = LIGHT_GREEN; lightId < LIGHT_OFF; lightId++)
						{
							const ScalarV offset = ScalarVFromF32(lightOffsets[lightId]);
							Vector3 lightPos = boneLocation + VEC3V_TO_VECTOR3(matrix.GetCol2()*offset);
							f32 distance = fabs((lightPos - vDamagePos).Mag());

							if (distance <= TRAFFIC_LIGHT_HALF_DAMAGE_RADIUS)
							{
								tli->DamageLight(boneId, lightId);
								break;
							}
						}
					}

					break;
				}
			}
		}
	}
}

void CWeaponDamage::DoWeaponImpactSearchLight(CEntity* pHitEntity, const Vector3& vDamagePos)
{
	CSearchLight* pSearchLight = NULL;
	CVehicle* pHitVehicle = static_cast<CVehicle*>(pHitEntity);

	if(pHitVehicle->InheritsFromHeli())
	{
		pSearchLight = static_cast<CRotaryWingAircraft*>(pHitVehicle)->GetSearchLight();
	}
	else if(pHitVehicle->InheritsFromBoat())
	{
		pSearchLight = static_cast<CBoat*>(pHitVehicle)->GetSearchLight();
	}

	if(pSearchLight != NULL)
	{
		const SearchLightInfo& slInfo = CSearchLight::GetSearchLightInfo(pHitVehicle);

		int boneIdx = pHitVehicle->GetBoneIndex(pSearchLight->GetWeaponBone());
		if(boneIdx == 0xFF)
			return;

		Matrix34 matrix;
		pHitVehicle->GetGlobalMtx(boneIdx, matrix);

		Vector3 lightPos = matrix.d;
		Vector3 lightDir = matrix.b;
		Vector3 boneLocation = lightPos+ (lightDir * slInfo.offset);

		f32 distance = fabs((boneLocation - vDamagePos).Mag());

		if (distance < TRAFFIC_LIGHT_CORONA_BULLET_DAMAGE_RADIUS)
		{
			pSearchLight->SetLightOn(false);
			pSearchLight->SetIsDamaged();
		}
	}
}


////////////////////////////////////////////////////////////////////////////////

f32 CWeaponDamage::DoWeaponImpactWeapon(CWeapon* pWeapon, CEntity* pFiringEntity, const Vector3& vWeaponPos, WorldProbe::CShapeTestHitPoint* pResult, const f32 fApplyDamage, const bool bTemporaryNetworkWeapon, const fwFlags32& flags, const sMeleeInfo *pMeleeInfo /*= NULL*/, NetworkWeaponImpactInfo * const networkWeaponImpactInfo /* = NULL */, const float fRecoilAccuracyWhenFired /* = 1.f */, u8 damageAggregationCount /*= 0*/)
{
	// clear if we're setting information on the owner and not using it on the clone...
	if(networkWeaponImpactInfo)
	{
		networkWeaponImpactInfo->Clear();
	}

	CEntity* pHitEntity = CPhysics::GetEntityFromInst(pResult->GetHitInst());
	//Check if we've lost the hitinstance, this is possible with frag objects.
	if(!pHitEntity)
	{
		return 0.0f;
	}
	weaponFatalAssertf(pHitEntity->GetIsTypeObject(), "pHitEntity is not a Object");
	CObject* pHitObj = static_cast<CObject*>(pHitEntity);

	CEntity* pHitAttachParent = (CPhysical *) pHitObj->GetAttachParent();

	// we've hit an ammo box attached to the gun...
	if(pHitAttachParent && !pHitAttachParent->GetIsTypePed())
	{
		if(pHitAttachParent->GetIsTypeObject())
		{
			if(((CObject*)pHitAttachParent)->GetWeapon())
			{
				pHitAttachParent = (CPhysical *)pHitAttachParent->GetAttachParent();

				if(networkWeaponImpactInfo)
					networkWeaponImpactInfo->SetAmmoAttachment( true );
			}
		}
	}

	if(pHitAttachParent && pHitAttachParent->GetIsTypePed())
	{
		CPed* pHitPed = static_cast<CPed*>(pHitAttachParent);

		if(!NetworkUtils::IsNetworkCloneOrMigrating(pHitPed))
		{
			if(!pHitPed->IsAPlayerPed())
			{
				const CPed* pFiringPlayerPed = pFiringEntity && pFiringEntity->GetIsTypePed() && static_cast<const CPed*>(pFiringEntity)->IsAPlayerPed() ? static_cast<const CPed*>(pFiringEntity) : NULL;

				if(!pHitPed->PopTypeIsMission() && pFiringPlayerPed)
				{
					// Save the offset before TurnPedsObjectIntoPickup is called, in case that moves the object
					Vector3 vHitOffset(pResult->GetHitPosition());
					vHitOffset.Subtract(VEC3V_TO_VECTOR3(pHitObj->GetTransform().GetPosition()));
					
					if(pWeapon)
					{
						const CWeapon* pHitWeapon = pHitObj->GetWeapon();
						if(pHitWeapon)
						{
							//Check if the weapon can be dropped.
							if(CanDropWeaponWhenShot(*pWeapon, *pHitPed, *pHitWeapon) && pFiringPlayerPed->IsAllowedToDamageEntity(pWeapon->GetWeaponInfo(), pHitPed))
							{
								CPickup* pPickup = CPickupManager::CreatePickUpFromCurrentWeapon(pHitPed);
								if(pPickup)
								{
									// Remove from peds inventory
									if(pHitPed->GetInventory())
									{
										pHitPed->GetInventory()->RemoveWeapon(pHitObj->GetWeapon()->GetWeaponHash());
									}

									Vector3 vDir(pResult->GetHitPosition() - vWeaponPos);
									vDir.NormalizeFast();

									static dev_float fImpulseScale = 0.01f;
									vDir *= pPickup->GetMass() * fImpulseScale;

									if(pPickup->GetCurrentPhysicsInst() && pPickup->GetCurrentPhysicsInst()->IsInLevel())
									{
										pPickup->ApplyImpulse(vDir, vHitOffset, pResult->GetHitComponent());
									}
								}
							}
						}
					}
				}
			}
		}

		// Ignore the impact to the ped if the object can shoot through.
		if(!pHitObj->CanShootThroughWhenEquipped())
		{
			float fDamage = 0.0f;

			//Check if the bullet will go through the weapon and damage the ped?
			Vector3 vDir(pResult->GetHitPosition() - vWeaponPos);
			vDir.NormalizeFast();
			WorldProbe::CShapeTestProbeDesc probeData;
			WorldProbe::CShapeTestFixedResults<1> ShapeTestResults; // We're only looking at the first result, so requesting more is wasteful
			probeData.SetResultsStructure(&ShapeTestResults);
			probeData.SetStartAndEnd(pResult->GetHitPosition(), pResult->GetHitPosition() + vDir);
			probeData.SetExcludeInstance(pHitPed->GetAnimatedInst());
			probeData.SetIncludeFlags(ArchetypeFlags::GTA_RAGDOLL_TYPE);
			probeData.SetStateIncludeFlags(phLevelBase::STATE_FLAGS_ALL);
			probeData.SetIsDirected(true);
			if(WorldProbe::GetShapeTestManager()->SubmitTest(probeData))
			{
				fDamage = fApplyDamage;
			}
			else
			{
				// Test for the closest component of the ped's ragdoll to the shot point (within SPHERE_RADIUS). Make sure we exclude the animation
				// instance from the collision test; it generates spurious components otherwise.
				static dev_float SPHERE_RADIUS = 1.0f;
				WorldProbe::CShapeTestSphereDesc sphereDesc;
				sphereDesc.SetResultsStructure(&ShapeTestResults);
				sphereDesc.SetSphere(pResult->GetHitPosition(), SPHERE_RADIUS);
				sphereDesc.SetExcludeInstance(pHitPed->GetAnimatedInst());
				sphereDesc.SetIncludeFlags(ArchetypeFlags::GTA_RAGDOLL_TYPE);
				sphereDesc.SetTypeFlags(TYPE_FLAGS_ALL);
				sphereDesc.SetStateIncludeFlags(phLevelBase::STATE_FLAGS_ALL);

				if(!WorldProbe::GetShapeTestManager()->SubmitTest(sphereDesc))
				{
					return 0.0f;
				}
			}
			
			CEntity* pNewHitEntity = CPhysics::GetEntityFromInst(ShapeTestResults[0].GetHitInst());
			if(pNewHitEntity == pHitPed)
			{
				bool blockDamage = pFiringEntity && ((pFiringEntity->GetIsTypePed() && !static_cast<CPed*>(pFiringEntity)->IsAllowedToDamageEntity(pWeapon->GetWeaponInfo(), pNewHitEntity))
													   || (pFiringEntity->GetIsTypeVehicle() && static_cast<CVehicle*>(pFiringEntity)->GetDriver() && !static_cast<CVehicle*>(pFiringEntity)->GetDriver()->IsAllowedToDamageEntity(pWeapon->GetWeaponInfo(), pNewHitEntity)));

				if(!blockDamage)
				{
					// set the flag to say we've also hit the ped if we're setting information on the owner and not using it on the clone...
					if(networkWeaponImpactInfo)
					{
						networkWeaponImpactInfo->SetDamagedHoldingPed( true );
					}

					return DoWeaponImpactPed(pWeapon, pFiringEntity, vWeaponPos, &ShapeTestResults[0], fDamage, bTemporaryNetworkWeapon, flags, pMeleeInfo, fRecoilAccuracyWhenFired, NULL, damageAggregationCount);
				}
			}
		}
	}

	return 0.0f;
}

////////////////////////////////////////////////////////////////////////////////

f32 CWeaponDamage::DoWeaponImpactBreakableGlass(CWeapon* pWeapon, CEntity* UNUSED_PARAM(pFiringEntity), const Vector3& vWeaponPos, WorldProbe::CShapeTestHitPoint* pResult, const f32 fApplyDamage)
{
	weaponDebugf3("CWeaponDamage::DoWeaponImpactBreakableGlass");

	bool bIsDamageMelee = pWeapon->GetWeaponInfo()->GetDamageType() == DAMAGE_TYPE_MELEE;
	f32 fShootForce  = bIsDamageMelee ? fApplyDamage * MELEE_WEAPON_FORCE_MULT : pWeapon->GetWeaponInfo()->GetForce();
	f32 fFragImpulse = pWeapon->GetWeaponInfo()->GetFragImpulse();

	Vector3 vDir = pResult->GetHitPosition() - vWeaponPos;
	vDir.Normalize();

	PHSIM->ApplyImpetusAndBreakingImpetus(
		ScalarV(fwTimer::GetTimeStep() / CPhysics::GetNumTimeSlices()).GetIntrin128ConstRef(),
		pResult->GetHitInst()->GetLevelIndex(),
		fShootForce * vDir,
		pResult->GetHitPosition(),
		pResult->GetHitComponent(),
		pResult->GetHitPartIndex(),
		fFragImpulse
		);

	return fApplyDamage;
}

////////////////////////////////////////////////////////////////////////////////

void CWeaponDamage::DoWeaponImpactAI(CWeapon* pWeapon, CEntity* pFiringEntity, const Vector3& vWeaponPos, WorldProbe::CShapeTestHitPoint* pResult, const f32 UNUSED_PARAM(fDamageDone))
{
	const CWeaponInfo* pWeaponInfo = pWeapon->GetWeaponInfo();
	// Adding a check against if the gun is lethal or not - don't want peds overreacting to things like stunguns
	if(pWeaponInfo->GetIsGun() && !pWeaponInfo->GetIsNonLethal() && (pWeaponInfo->GetFireType() == FIRE_TYPE_INSTANT_HIT || pWeaponInfo->GetFireType() == FIRE_TYPE_DELAYED_HIT))
	{
		// Create a bullet impact for this event.
		bool bSilentGunshot = pWeapon->GetIsSilenced();
		CPed* pFiringPed = (pFiringEntity && pFiringEntity->GetIsTypePed()) ? static_cast<CPed*>(pFiringEntity) : NULL;
		bool bBlockFiringEvents = (pFiringPed && pFiringPed->GetPedResetFlag(CPED_RESET_FLAG_SupressGunfireEvents));
		if( !bBlockFiringEvents )
		{
			CEventGunShotBulletImpact eventGunShotBulletImpact(pFiringEntity, vWeaponPos, pResult->GetHitPosition(), bSilentGunshot, pWeapon->GetWeaponHash());
			GetEventGlobalGroup()->Add(eventGunShotBulletImpact);

			// Intended for friendly bullet impact events
			CEventFriendlyFireNearMiss eventFriendlyNearMiss(pFiringEntity, vWeaponPos, pResult->GetHitPosition(), bSilentGunshot, pWeapon->GetWeaponHash(), CEventFriendlyFireNearMiss::FNM_BULLET_IMPACT);
			GetEventGlobalGroup()->Add(eventFriendlyNearMiss);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CWeaponDamage::DoWeaponImpactVfx(CWeapon* pWeapon, CEntity* pFiringEntity, const Vector3& vWeaponPos, WorldProbe::CShapeTestHitPoint* pResult, const f32 fDamageDone, const fwFlags32& flags, const bool headShotNearby, const bool isExitFx, const bool bTemporaryNetworkWeapon /* = false */, const bool bShouldApplyDamageToGlass /* = true */)
{
#if !__NO_OUTPUT
	weaponDebugf3("CWeaponDamage::DoWeaponImpactVfx pWeapon[%p] pFiringEntity[%p] vWeaponPos[%f %f %f] pResult[%p] fDamageDone[%f] headShotNearby[%d] isExitFx[%d] bTemporaryNetworkWeapon[%d]",pWeapon,pFiringEntity,vWeaponPos.x,vWeaponPos.y,vWeaponPos.z,pResult,fDamageDone,headShotNearby,isExitFx,bTemporaryNetworkWeapon);
#endif

#if __BANK
	if(!CPhysics::ms_bDoWeaponImpactEffects)
	{
		return;
	}
#endif // __BANK

#if __ASSERT
	if(	!Verifyf(pResult, "No shapetest result passed in to CWeaponDamage::DoWeaponImpactVfx.") || 
		!Verifyf(pResult->GetHitDetected(), "No hit on impact passed into CWeaponDamage::DoWeaponImpactVfx.") ||
		!Verifyf(pResult->GetHitInst(), "No instance on impact passed into CWeaponDamage::DoWeaponImpactVfx.") ||
		!Verifyf(pResult->GetHitInst()->IsInLevel(), "Instance on impact passed into CWeaponDamage::DoWeaponImpactVfx doesn't exist in level."))
	{
		return;
	}
#endif

#if !__NO_OUTPUT
	weaponDebugf3("CWeaponDamage::DoWeaponImpactVfx -- pResult[%p] GetHitDetected[%d] GetHitInst[%p] IsInLevel[%d]",pResult,pResult ? pResult->GetHitDetected() : false,pResult ? pResult->GetHitInst() : 0,pResult && pResult->GetHitInst() ? pResult->GetHitInst()->IsInLevel() : 0);
#endif

	// get the collision position 
	Vec3V vColnPos = RCC_VEC3V(pResult->GetHitPosition());

	// add smoke if we're in an interior
	if (pFiringEntity && pFiringEntity->GetIsTypePed())
	{
		CPed* pFiringPed = static_cast<CPed*>(pFiringEntity);
		CObject* pWeaponObj = pFiringPed->GetWeaponManager() ? pFiringPed->GetWeaponManager()->GetEquippedWeaponObject() : NULL;
		if (pWeaponObj)
		{
			CPortalTracker* pPortalTracker = pWeaponObj->GetPortalTracker();
			if(pPortalTracker)
			{
				CInteriorInst* pIntInst = pPortalTracker->GetInteriorInst();
				if(pIntInst && pPortalTracker->m_roomIdx > 0)
				{
					// We're in a room inside an interior
					pIntInst->AddSmokeToRoom(pPortalTracker->m_roomIdx, RCC_VECTOR3(vColnPos));
				}
			}
		}
	}

	bool bHitGlass = false;

	// deal with effects when we need to have a valid entity from the inst
	CEntity* pHitEntity = CPhysics::GetEntityFromInst(pResult->GetHitInst());
	if (pHitEntity)
	{
		// set up the collision info structure
		VfxCollInfo_s vfxCollInfo;
		vfxCollInfo.regdEnt = pHitEntity;
		vfxCollInfo.vPositionWld = RCC_VEC3V(pResult->GetHitPosition());
		vfxCollInfo.vNormalWld = RCC_VEC3V(pResult->GetHitNormal());

		if (pWeapon->GetWeaponInfo()->GetIsMelee())
		{
			// the normal approximates the direction of the blow
			vfxCollInfo.vDirectionWld = vfxCollInfo.vNormalWld;
			vfxCollInfo.vNormalWld = -vfxCollInfo.vNormalWld;
		}
		else
		{
			// calc the direction of the gun shot
			vfxCollInfo.vDirectionWld = vfxCollInfo.vPositionWld - RCC_VEC3V(vWeaponPos);
		}

		vfxCollInfo.vDirectionWld = Normalize(vfxCollInfo.vDirectionWld);
		vfxCollInfo.materialId = PGTAMATERIALMGR->UnpackMtlId(pResult->GetHitMaterialId());
		vfxCollInfo.roomId = PGTAMATERIALMGR->UnpackRoomId(pResult->GetHitMaterialId());
		vfxCollInfo.componentId = pResult->GetHitComponent();
		vfxCollInfo.weaponGroup = pWeapon->GetWeaponInfo()->GetEffectGroup();
		vfxCollInfo.force = VEHICLEGLASSFORCE_WEAPON_IMPACT;
		vfxCollInfo.isBloody = headShotNearby;
		vfxCollInfo.isWater = false;
		vfxCollInfo.isExitFx = isExitFx;
		vfxCollInfo.noPtFx = false;
		vfxCollInfo.noPedDamage = false;
		vfxCollInfo.noDecal = PGTAMATERIALMGR->GetIsNoDecal(pResult->GetHitMaterialId()) || 
							  flags.IsFlagSet( CPedDamageCalculator::DF_PtFxOnly );
		vfxCollInfo.isSnowball = false;

		if (pWeapon->GetWeaponInfo())
		{
			float fWeaponDamage = CWeaponInfoManager::GetArmouredGlassDamageForWeaponGroup(pWeapon->GetWeaponInfo()->GetGroup());

			// Apply weapon-specific damage override if defined.
			float fOverridenDamage = pWeapon->GetWeaponInfo()->GetArmouredVehicleGlassDamageOverride();
			if (fOverridenDamage != -1.0f)
			{
				fWeaponDamage = fOverridenDamage;
			}

			// Spread damage out per-bullet (ie shotguns).
			vfxCollInfo.armouredGlassWeaponDamage = (fWeaponDamage / pWeapon->GetBulletsInBatch());

			vfxCollInfo.armouredGlassShotByPedInsideVehicle = false;
			// Flag if ped is shooting in FPS mode from inside it's own vehicle.
			TUNE_GROUP_BOOL(VEH_GLASS_HACKS, bInstantSmashBulletResistantGlassWhenInside, true);
			if (bInstantSmashBulletResistantGlassWhenInside && pFiringEntity && pFiringEntity->GetIsTypePed() && pHitEntity && pHitEntity->GetIsTypeVehicle())
			{
				CPed* pFiringPed = static_cast<CPed*>(pFiringEntity);
				if (pFiringPed && pFiringPed->GetIsInVehicle())
				{
					CVehicle *pVehicle = pFiringPed->GetVehiclePedInside();
					CVehicle *pHitVehicle = static_cast<CVehicle*>(pHitEntity);
					if (pVehicle && pHitVehicle && pVehicle == pHitVehicle)
					{
						vfxCollInfo.armouredGlassShotByPedInsideVehicle = true;
					}
				}
			}

			// Flag if Full Metal Jacket ammo
			vfxCollInfo.isFMJAmmo = false;
			if (pFiringEntity && pFiringEntity->GetIsTypePed())
			{
				CPed* pFiringPed = static_cast<CPed*>(pFiringEntity);			
				const CAmmoInfo* pAmmoInfo = pWeapon->GetWeaponInfo()->GetAmmoInfo(pFiringPed);
				if (pAmmoInfo && pAmmoInfo->GetIsFMJ())
				{
					vfxCollInfo.isFMJAmmo = true;
				}
			}
		}

		// deal with using 'non-melee' weapons for melee attacks
		if (flags.IsFlagSet( CPedDamageCalculator::DF_MeleeDamage ) && vfxCollInfo.weaponGroup>WEAPON_EFFECT_GROUP_MELEE_GENERIC)
		{
			vfxCollInfo.weaponGroup = WEAPON_EFFECT_GROUP_MELEE_METAL;
		}

#if !__NO_OUTPUT
		const phMaterial &mat = PGTAMATERIALMGR->GetMaterial(vfxCollInfo.materialId);
		weaponDebugf3("vfxCollInfo regdEnt[%p] vPositionWld[%f %f %f] materialId[%" I64FMT "d][%s] componentId[%d] weaponGroup[%d] force[%f] isBloody[%d] isExitFx[%d] noDecal[%d]",pHitEntity,pResult->GetHitPosition().x,pResult->GetHitPosition().y,pResult->GetHitPosition().z,vfxCollInfo.materialId,mat.GetName(),vfxCollInfo.componentId,vfxCollInfo.weaponGroup,vfxCollInfo.force,vfxCollInfo.isBloody,vfxCollInfo.isExitFx,vfxCollInfo.noDecal);
#endif


		if (vfxCollInfo.regdEnt && vfxCollInfo.regdEnt->GetIsTypeObject())
		{
// NOTE: pure cloth ( there are no cloth and non-cloth parts in the same frag) impact is not processed here anymore
// see CBullet::ComputeImpacts
// Svetli
			fragInst* pFragInst = pHitEntity->GetFragInst();
			bool bOnlyCloth = false;
			if( pFragInst && pFragInst->GetType() && (pFragInst->GetType()->GetNumEnvCloths() > 0) )
			{
				bOnlyCloth = (NULL == pFragInst->GetType()->GetClothDrawable()) ;
			}
			if( bOnlyCloth )
			{
				return;
			}
		}

		bHitGlass = vfxCollInfo.regdEnt && PGTAMATERIALMGR->GetIsSmashableGlass(pResult->GetHitMaterialId()) && IsEntitySmashable(vfxCollInfo.regdEnt);

		// Process the visual effects
		if (!flags.IsFlagSet( CPedDamageCalculator::DF_PtFxOnly ) && bHitGlass)
		{
			// Check if we want to process the glass smashing
			bool bProcessGlassSmashing = bShouldApplyDamageToGlass;

			if (CVfxHelper::GetDistSqrToCamera(vfxCollInfo.vPositionWld)>VEHICLE_GLASS_BREAK_RANGE_SQR)
			{
				bProcessGlassSmashing = false;
			}

			// MN - removed this for url:bugstar:1182317
			//      don't really see why we'd ever want to stop glass smashing or taking decals when this flag is set
// /*			if (pWeapon->GetDamageType()==DAMAGE_TYPE_ELECTRIC)
// 			{
// 				// electric weapons (e.g. stun guns) don't want to cause glass to smash
// 				bProcessGlassSmashing = false;
// 			}
// 			else*/ if (vfxCollInfo.regdEnt->GetIsPhysical())
// 			{
// 				CPhysical* pPhysical = static_cast<CPhysical*>(vfxCollInfo.regdEnt.Get());
// 				if (pPhysical->m_nPhysicalFlags.bNotDamagedByBullets)
// 				{
// 					bProcessGlassSmashing = false;
// 				}
// 			}

			// Try to smash glass if required
			if (bProcessGlassSmashing)
			{
				// disable particle effects from peds shooting their own vehicle when drive-by shooting
				// this mainly stops lots of windscreen glass bullet impact effects being created
				if (pHitEntity->GetIsTypeVehicle() && pFiringEntity && pFiringEntity->GetIsTypePed())
				{
					CVehicle* pHitVehicle = static_cast<CVehicle*>(pHitEntity);
					CPed* pFiringPed = static_cast<CPed*>(pFiringEntity);
					if (pFiringPed->GetVehiclePedInside()==pHitVehicle)
					{
						vfxCollInfo.noPtFx = true;
					}
				}

				vfxCollInfo.isSnowball = pWeapon->GetWeaponInfo()->GetHash()==WEAPONTYPE_DLC_SNOWBALL;

				// Glass collision on a smashable entity
				g_vehicleGlassMan.StoreCollision(vfxCollInfo);
			}
		}
		else if (pHitEntity->GetIsTypePed())
		{
			CPed* pPed = static_cast<CPed*>(pHitEntity);

			if (weaponVerifyf(PGTAMATERIALMGR->GetIsPed(pResult->GetHitMaterialId()), "shot a ped that isn't set up with ped materials (%s - %" I64FMT "u)", pHitEntity->GetModelName(), pResult->GetHitMaterialId()))
			{
				bool bShouldShowVFX = fDamageDone > 0.0f || IsEnduranceDamage(pPed, pWeapon, flags);

				if (bShouldShowVFX)
				{
					if (pFiringEntity && NetworkUtils::IsNetworkCloneOrMigrating(pFiringEntity) && !NetworkUtils::IsNetworkCloneOrMigrating(pHitEntity) && !bTemporaryNetworkWeapon)
					{
						// if the firing entity is a clone and the hit entity isn't,
						// I'm going to receive a message over the network from the firing entity owner telling me to apply damage
						// in that case, stop the blood being added twice
						// and only use the network event which should contain better / closer data
						// as the ped firing is randomised based on accuracy and gun firing cone dimensions

						vfxCollInfo.noPedDamage = true;
					}

					// play blood vfx
					g_vfxBlood.DoVfxBlood(vfxCollInfo, pFiringEntity);

					// play shot vfx
					g_vfxPed.TriggerPtFxShot(pPed, vfxCollInfo.vPositionWld, vfxCollInfo.vNormalWld);
				}
			}
		}
		else
		{
			// play weapon impact vfx
			g_vfxWeapon.DoWeaponImpactVfx(vfxCollInfo, pWeapon->GetDamageType(), pFiringEntity);
		}

		if (NetworkInterface::IsGameInProgress() && !vfxCollInfo.noDecal && pResult && !bHitGlass)
		{
			//Record the weapon impact points in areas for vehicles so we can replicate/fake-out the impacts onto remotes when they come into scope
			if (pHitEntity->GetIsTypeVehicle())
			{
				CVehicle* pVehicle = static_cast<CVehicle*>(pHitEntity);
				if (pVehicle && pVehicle->GetNetworkObject())
				{
					CNetObjVehicle* pNetObjVehicle = static_cast<CNetObjVehicle*>(pVehicle->GetNetworkObject());
					if (pNetObjVehicle)
					{
						pNetObjVehicle->StoreWeaponImpactPointInformation(pResult->GetHitPosition(), vfxCollInfo.weaponGroup, true);
					}
				}
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CWeaponDamage::DoWeaponImpactAudio(CWeapon* pWeapon, CEntity* pFiringEntity, const Vector3& vWeaponPos, WorldProbe::CShapeTestHitPoint* pResult, const f32 fDamageDone, const fwFlags32& flags)
{
	weaponDebugf3("CWeaponDamage::DoWeaponImpactAudio");

	bool playerShot  = false;
	CPed* pPedFiringEntity = NULL;
	if(pFiringEntity && pFiringEntity->GetIsTypePed())
	{
		pPedFiringEntity = static_cast<CPed*>(pFiringEntity);
		if (pPedFiringEntity)
			playerShot = pPedFiringEntity->IsLocalPlayer();
	}

	if( flags.IsFlagSet( CPedDamageCalculator::DF_MeleeDamage ) )
	{
		weaponDebugf3("DF_MeleeDamage");

		// Special handling of melee weapon impacts.
		//This is handled in audPedAudioEntity::PlayMeleeCombatHit g_CollisionAudioEntity.ReportMeleeCollision(pResult, fDamageDone, pFiringEntity);
	
		if (playerShot)
		{
			CPed* pPed = static_cast<CPed*>(pFiringEntity);
			CMiniMap::CreateSonarBlipAndReportStealthNoise(pPed, pPed->GetTransform().GetPosition(), CMiniMap::sm_Tunables.Sonar.fSoundRange_ObjectCollision, HUD_COLOUR_BLUEDARK);
		}

		if (pPedFiringEntity && pPedFiringEntity->IsNetworkClone())
		{
			weaponDebugf3("CLONE --> invoke ReportMeleeCollision");
			g_CollisionAudioEntity.ReportMeleeCollision(pResult, fDamageDone, pPedFiringEntity);
		}
	}
	else
	{
		bool bulletHitPlayer = false;
		CEntity* pHitEntity = CPhysics::GetEntityFromInst(const_cast<phInst *>(pResult->GetHitInst()));

		if(pHitEntity && pHitEntity->GetType() == ENTITY_TYPE_PED)
		{
			bulletHitPlayer = (static_cast<CPed*>(pHitEntity))->IsLocalPlayer();
		}
		if(bulletHitPlayer || !SUPERCONDUCTOR.GetGunFightConductor().GetFakeScenesConductor().IsFakingBulletImpacts()
			|| (SUPERCONDUCTOR.GetGunFightConductor().GetFakeScenesConductor().IsFakingBulletImpacts() && playerShot))
		{ 
			bool isAutomatic = false; 
			bool isShotgun = false; 
			f32 timeBetweenShots = 0.f;
			const CWeaponInfo* weaponInfo = pWeapon->GetWeaponInfo();
			if (weaponInfo) 
			{
				eWeaponEffectGroup effectGroup = weaponInfo->GetEffectGroup();
				if (effectGroup == WEAPON_EFFECT_GROUP_SMG || effectGroup == WEAPON_EFFECT_GROUP_RIFLE_ASSAULT)
				{
					isAutomatic = true;
				}
				else if (effectGroup == WEAPON_EFFECT_GROUP_SHOTGUN)
				{
					isShotgun = true;
				}
				timeBetweenShots = weaponInfo->GetTimeBetweenShots();
			}

			CAmmoInfo::SpecialType ammoType = CAmmoInfo::None;

			if(weaponInfo && pPedFiringEntity)
			{
				const CAmmoInfo* ammoInfo = weaponInfo->GetAmmoInfo(pPedFiringEntity);

				if(ammoInfo)
				{
					ammoType = ammoInfo->GetAmmoSpecialType();
				}
			}
			
			g_CollisionAudioEntity.ReportBulletHit(pWeapon->GetWeaponInfo()->GetAudioHash(), pResult, vWeaponPos, pWeapon->GetAudioComponent().GetWeaponSettings(pFiringEntity), ammoType, playerShot, bulletHitPlayer, isAutomatic, isShotgun, timeBetweenShots);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CWeaponDamage::DoWeaponFiredByPlayer(CWeapon* pWeapon, CEntity* pFiringEntity, const Vector3& UNUSED_PARAM(vWeaponPos), WorldProbe::CShapeTestHitPoint* pResult, const f32 fDamageDone, const fwFlags32& flags)
{
	CPed* pPlayerPed = NULL;
	if(pFiringEntity && pFiringEntity->GetIsTypePed() && static_cast<CPed*>(pFiringEntity)->IsPlayer())
	{
		pPlayerPed = static_cast<CPed*>(pFiringEntity);
	}
	weaponFatalAssertf(pPlayerPed, "pPlayerPed is NULL");

	const CWeaponInfo* wi = pWeapon->GetWeaponInfo();
	Assert(wi);

	// Get the hit entity
	CEntity* pHitEntity = CPhysics::GetEntityFromInst(pResult->GetHitInst());
	if(pHitEntity)
	{
		// Cache if we've hit a ped
		if(pHitEntity->GetIsTypePed())
		{
			CPed* pHitPed = static_cast<CPed*>(pHitEntity);

			if(wi->GetIsGun())
			{
				CCrime::ReportCrime(CRIME_POSSESSION_GUN, pHitEntity, pPlayerPed);

				if(!pHitPed->IsDead())
				{
					const WeaponSettings *settings = pWeapon->GetAudioComponent().GetWeaponSettings();
					if(settings)
					{
						bool isNonlethal = AUD_GET_TRISTATE_VALUE(settings->Flags, FLAG_ID_WEAPONSETTINGS_ISNONLETHAL)== AUD_TRISTATE_TRUE;
						if(pHitPed->IsLawEnforcementPed())
						{
							CCrime::ReportCrime(isNonlethal ? CRIME_SHOOT_NONLETHAL_COP : CRIME_SHOOT_COP, pHitEntity, pPlayerPed);
						}
						else
						{
							eCrimeType crimeToReport = isNonlethal ? CRIME_SHOOT_NONLETHAL_PED : (pWeapon->GetIsSilenced() ? CRIME_SHOOT_PED_SUPPRESSED : CRIME_SHOOT_PED);
							CCrime::ReportCrime(crimeToReport, pHitEntity, pPlayerPed);
						}
					}
				}
			}

			// If the damage inflictor is the player then increase the havoc score.
			if(fDamageDone > 0.0f && pHitPed)
			{
				pPlayerPed->GetPlayerInfo()->HavocCaused += HAVOC_PEDDAMAGED;
			}
		}

		// grab these once
		bool bIsPed      = pHitEntity->GetIsTypePed();
		bool bIsVehicle  = pHitEntity->GetIsTypeVehicle();
		bool bIsObject   = pHitEntity->GetIsTypeObject();
		bool bTrackStats = bIsPed || bIsVehicle || bIsObject;

		if(bIsPed && NetworkInterface::IsGameInProgress() && static_cast<CPed*>(pHitEntity)->m_nPhysicalFlags.bNotDamagedByAnything)
		{
			//Turns off stats in network games for entities
			//such as hairdressers in script scenes. B*1942049 Exploit
			bTrackStats = false;

			//And we need to decrease the shots fired by this weapon.
			if (pPlayerPed && pPlayerPed->IsLocalPlayer())
			{
				CStatsMgr::PlayerFiredWeaponToInvenciblePed(pWeapon);
			}
		}

		// Deal with stats
		if(CStatsUtils::CanUpdateStats() && pHitEntity != pPlayerPed && bTrackStats && pPlayerPed->IsLocalPlayer())
		{
			// track target validity, assume valid
			bool bIsValidTargetForStats = true;

			if(bIsPed)
			{
				CPed* pPed = static_cast<CPed*>(pHitEntity);
				if(pPed->IsDead())
				{
					// ped is dead, should not count for stats
					bIsValidTargetForStats = false; 
				}
			}
			else if(bIsVehicle)
			{
				CVehicle* pVehicle = static_cast<CVehicle*>(pHitEntity);
				if(pVehicle->GetStatus() == STATUS_WRECKED)
				{
					// vehicle is wrecked, should not count for stats
					bIsValidTargetForStats = false;
				}
			}

			// only do stats if we have a valid target
			if(bIsValidTargetForStats)
			{
				bool updateWeaponDamageStats = (0.0f < fDamageDone);

				//Check whether we should accept this damage event based on the friendly fire settings
				if (NetworkInterface::IsGameInProgress())
				{
					updateWeaponDamageStats = false;

					CPed* pedHit = NULL;
					CVehicle* vehicle = NULL;

					if(bIsPed)
					{
						pedHit = static_cast<CPed*>(pHitEntity);
					}
					else if(bIsVehicle)
					{
						vehicle = static_cast<CVehicle*>(pHitEntity);
					}
					else if(bIsObject)
					{
						CObject* obj = static_cast<CObject*>(pHitEntity);
						if (obj->GetIsAttached())
						{
							CPhysical* attachParent = static_cast<CObject*>(obj->GetAttachParent());
							if(attachParent)
							{
								if(attachParent->GetIsTypePed())
								{
									pedHit = static_cast<CPed*>(attachParent);
								}
								else if(attachParent->GetIsTypeVehicle())
								{
									vehicle = static_cast<CVehicle*>(attachParent);
								}
							}
						}
					}

					if (pedHit)
					{
						updateWeaponDamageStats = (0.0f < fDamageDone || (pedHit->IsNetworkClone() && NetworkInterface::IsFriendlyFireAllowed(pedHit, pPlayerPed)));
					}
					else if (vehicle && vehicle->GetSeatManager())
					{
						updateWeaponDamageStats = (0.0f < fDamageDone || (vehicle->IsNetworkClone() && NetworkInterface::IsFriendlyFireAllowed(vehicle, pPlayerPed)));
					}
				}

				if(updateWeaponDamageStats)
				{
					static u32 uLastFrameOfIncrease = 0;

					//Increment Hits for peds and non-empty vehicles
					bool incrementStatPedsVehicles = bIsPed;
					if (bIsVehicle)
					{
						incrementStatPedsVehicles = false;

						CVehicle* vehc = bIsVehicle ? static_cast<CVehicle*>(pHitEntity) : NULL;
						if (vehc && (vehc->GetDriver() || vehc->GetNumberOfPassenger() > 0))
						{
							int numSeats = vehc->GetLayoutInfo()->GetNumSeats();
							for(int s=0; s<numSeats && !incrementStatPedsVehicles; s++)
							{
								CPed * pPed = vehc->GetPedInSeat(s);
								if(pPed && !pPed->IsDead())
								{
									incrementStatPedsVehicles = true;
								}
							}
						}
					}

					//Melee attacks
					if (wi->GetIsMelee() || flags.IsFlagSet( CPedDamageCalculator::DF_MeleeDamage ))
					{
						//We already counted this frame
						if (fwTimer::GetSystemFrameCount() != uLastFrameOfIncrease)
						{
							uLastFrameOfIncrease = fwTimer::GetSystemFrameCount();

							//Count UNARMED_PED_HITS in multiplayer.
							if (bIsPed && wi->GetHash() == WEAPONTYPE_UNARMED)
							{
								if(StatsInterface::IsKeyValid(STAT_UNARMED_PED_HITS.GetStatId()))
								{
									StatsInterface::IncrementStat(STAT_UNARMED_PED_HITS.GetStatId(), 1.0f, STATUPDATEFLAG_ASSERTONLINESTATS);
								}
							}

							if (incrementStatPedsVehicles)
							{
								StatId statWeaponHit = StatsInterface::GetLocalPlayerWeaponInfoStatId(StatsInterface::WEAPON_STAT_HITS, wi, pPlayerPed);
								if(StatsInterface::IsKeyValid(statWeaponHit))
								{
									StatsInterface::IncrementStat(statWeaponHit, 1.0f, STATUPDATEFLAG_ASSERTONLINESTATS);
								}
#if __ASSERT
								else if(pPlayerPed->IsArchetypeSet() && StatsInterface::GetStatsPlayerModelValid())
								{
									CStatsMgr::MissingWeaponStatName(STATTYPE_WEAPON_HITS, statWeaponHit, wi->GetHash(), wi->GetDamageType(), wi->GetName());
								}
#endif // __ASSERT
							}
						}
					}
					//Gun attacks 
					// - If the weapon fires multiple bullets in a spread, only update the stat once
					// - If a bullet hits multiple things in the same frame, only update stat once (ie window+sit of a car, or car+driver)
					else if (fwTimer::GetSystemFrameCount() != uLastFrameOfIncrease)
					{
						uLastFrameOfIncrease = fwTimer::GetSystemFrameCount();

						if (incrementStatPedsVehicles)
						{
							StatId statWeaponFired = StatsInterface::GetLocalPlayerWeaponInfoStatId(StatsInterface::WEAPON_STAT_SHOTS, wi, pPlayerPed);
							StatId statWeaponHit   = StatsInterface::GetLocalPlayerWeaponInfoStatId(StatsInterface::WEAPON_STAT_HITS, wi, pPlayerPed);
							if(StatsInterface::IsKeyValid(statWeaponFired) && StatsInterface::IsKeyValid(statWeaponHit))
							{
								if(StatsInterface::GetIntStat(statWeaponHit) < StatsInterface::GetIntStat(statWeaponFired))
								{
									StatsInterface::IncrementStat(statWeaponHit, 1.0f, STATUPDATEFLAG_ASSERTONLINESTATS); //Increment this bullet hit in the stats

									//Hit a vehicle
									CVehicle* vehc = bIsVehicle ? static_cast<CVehicle*>(pHitEntity) : NULL;
									if (vehc)
									{
										if (VEHICLE_TYPE_CAR == vehc->GetVehicleType())
										{
											StatId hitCar = StatsInterface::GetLocalPlayerWeaponInfoStatId(StatsInterface::WEAPON_STAT_HITS_CAR, wi, pPlayerPed);
											if (StatsInterface::IsKeyValid(hitCar))
											{
												StatsInterface::IncrementStat(hitCar, 1.0f, STATUPDATEFLAG_ASSERTONLINESTATS);
											}
										}
									}
								}
							}
#if __ASSERT
							else if(pPlayerPed->IsArchetypeSet() && StatsInterface::GetStatsPlayerModelValid())
							{
								CStatsMgr::MissingWeaponStatName(STATTYPE_WEAPON_HITS, statWeaponFired, wi->GetHash(), wi->GetDamageType(), wi->GetName());
							}
#endif // __ASSERT
						}

						//Do not count for vehicle weapons.
						if (!wi->GetIsVehicleWeapon())
						{
							StatId statPedFired = STAT_SHOTS.GetStatId();
							StatId statPedHit   = STAT_HITS.GetStatId();

							if(StatsInterface::IsKeyValid(statPedFired) && StatsInterface::IsKeyValid(statPedHit))
							{
								if(StatsInterface::GetIntStat(statPedHit) < StatsInterface::GetIntStat(statPedFired))
								{
									StatsInterface::IncrementStat(statPedHit, 1.0f, STATUPDATEFLAG_ASSERTONLINESTATS);  //Increment this bullet hit in the stats
								}

								//Hits during a mission
								if (incrementStatPedsVehicles && CTheScripts::GetPlayerIsOnAMission())
								{
									StatId statPedHitMission = STAT_HITS_MISSION.GetStatId();
									if (StatsInterface::IsKeyValid(statPedHitMission))
									{
										if(StatsInterface::GetIntStat(statPedHitMission) < StatsInterface::GetIntStat(statPedFired))
										{
											StatsInterface::IncrementStat(statPedHitMission, 1.0f); //Increment this bullet hit in the stats
										}
									}
								}

								if (incrementStatPedsVehicles && StatsInterface::IsKeyValid(STAT_HITS_PEDS_VEHICLES.GetHash()))
								{
									StatsInterface::IncrementStat(STAT_HITS_PEDS_VEHICLES.GetStatId(), 1.0f, STATUPDATEFLAG_ASSERTONLINESTATS);
								}
							}
						}

						//Is inside Vehicle
						CVehicle* pVehicle = pPlayerPed->GetVehiclePedInside();
						if(pVehicle)
						{
							const bool playerIsDriver = (pPlayerPed == pVehicle->GetDriver());

							if (playerIsDriver)
							{
								StatId statPedDriveByHit = STAT_DB_HITS.GetStatId();
								if(StatsInterface::GetIntStat(statPedDriveByHit) < StatsInterface::GetIntStat(STAT_DB_SHOTS.GetStatId()))
								{
									StatsInterface::IncrementStat(statPedDriveByHit, 1.0f, STATUPDATEFLAG_ASSERTONLINESTATS);  // Increment this bullet hit in the stats
								}

								if (incrementStatPedsVehicles && StatsInterface::IsKeyValid(STAT_DB_HITS_PEDS_VEHICLES))
								{
									StatsInterface::IncrementStat(STAT_DB_HITS_PEDS_VEHICLES.GetStatId(), 1.0f, STATUPDATEFLAG_ASSERTONLINESTATS);
								}
							}
							else
							{
								StatId statPedDriveByFired = STAT_PASS_DB_SHOTS.GetStatId();
								StatId statPedDriveByHit   = STAT_PASS_DB_HITS.GetStatId();
								if(StatsInterface::GetIntStat(statPedDriveByHit) < StatsInterface::GetIntStat(statPedDriveByFired))
								{
									StatsInterface::IncrementStat(statPedDriveByHit, 1.0f, STATUPDATEFLAG_ASSERTONLINESTATS);  // Increment this bullet hit in the stats
								}

								if (incrementStatPedsVehicles && StatsInterface::IsKeyValid(STAT_PASS_DB_HITS_PEDS_VEHICLES))
								{
									StatsInterface::IncrementStat(STAT_PASS_DB_HITS_PEDS_VEHICLES.GetStatId(), 1.0f, STATUPDATEFLAG_ASSERTONLINESTATS);
								}
							}
						}
					}
				}
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CWeaponDamage::SetLastSignificantShotBone(CWeapon* pWeapon, CPed* pHitPed, WorldProbe::CShapeTestHitPoint* pResult)
{
	// Set the last significant shot bone 
	if(pResult)
	{
		s32 iHitComponent = pResult->GetHitComponent();
		s32 iHitBoneTag = pHitPed->GetBoneTagFromRagdollComponent(iHitComponent);
		if(iHitBoneTag == BONETAG_NECK || iHitBoneTag == BONETAG_HEAD || iHitBoneTag == BONETAG_NECKROLL)
		{
			// Head has been hit - all weapons are significant apart from punch/kick
			if(pWeapon->GetWeaponInfo()->GetEffectGroup() >= WEAPON_EFFECT_GROUP_MELEE_WOOD)
			{
				pHitPed->SetLastSignificantShotBoneTag(iHitBoneTag);
			}
		}
		else if(iHitBoneTag == BONETAG_ROOT	|| iHitBoneTag == BONETAG_PELVIS || iHitBoneTag == BONETAG_SPINE0 || iHitBoneTag == BONETAG_SPINE1 || iHitBoneTag == BONETAG_SPINE2 || iHitBoneTag == BONETAG_SPINE3 || iHitBoneTag == BONETAG_R_CLAVICLE || iHitBoneTag == BONETAG_L_CLAVICLE)
		{
			// Torso has been hit - gun weapons are significant
			if(pWeapon->GetWeaponInfo()->GetEffectGroup() >= WEAPON_EFFECT_GROUP_PISTOL_SMALL)
			{
				pHitPed->SetLastSignificantShotBoneTag(iHitBoneTag);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

CTask* CWeaponDamage::GenerateRagdollTask(CEntity* pFiringEntity, CPed* pHitPed, const u32 uWeaponHash, const f32 fWeaponDamage, const fwFlags32& flags, 
										  const bool bWasKilledOrInjured, const Vector3& vStart, WorldProbe::CShapeTestHitPoint* pResult, 
										  const Vector3& vRagdollImpulseDir, const f32 fRagdollImpulseMag, bool &bTaskAppliesImpulse, bool &bUpdatedPreviousEvent)
{
	// Get the weapon info
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(uWeaponHash);
	if(!pWeaponInfo)
	{
		return NULL;
	}

	if (!pHitPed)
	{
		weaponAssertf(pHitPed, "CWeaponDamage::GenerateRagdollTask - pHitPed is NULL");
		return NULL;
	}
	weaponAssertf(pHitPed->GetPedIntelligence(), "CWeaponDamage::GenerateRagdollTask - pHitPed->GetPedIntelligence() is NULL");

#if __ASSERT
	if(pResult)
	{
		weaponAssertf(pResult->GetHitNormal().IsNonZero(), "Invalid intersection normal");
	}
#endif // __ASSERT

	// First, assume that the task isn't assuming responsibility for applying the impulse
	bTaskAppliesImpulse = false;

	bool bNeedRagdollTask = false;
	if(!pHitPed->GetUsingRagdoll())
	{
		bNeedRagdollTask = true;

		f32 fKnockOffProp = fwRandom::GetRandomNumberInRange(0.0, 1.0f);
		if(fWeaponDamage > 30.0f)
		{
			fKnockOffProp *= 1.3f;
		}

		if(NetworkInterface::IsGameInProgress())
		{
			fKnockOffProp = 0.0f;
		}

		if(fKnockOffProp > 0.9f)
		{
			CPedPropsMgr::KnockOffProps(pHitPed, true, true, true);
		}
	}

	// Need a task if killed and not running an appropriate ragdoll reaction
	CTask* pTaskSimplest = pHitPed->GetPedIntelligence()->GetTaskActiveSimplest();
	
	if (pTaskSimplest && pTaskSimplest->IsNMBehaviourTask())
	{
		CTaskNMBehaviour* pRunningNmTask = smart_cast<CTaskNMBehaviour*>(pTaskSimplest);
		bNeedRagdollTask = pRunningNmTask->ShouldAbortForWeaponDamage(pFiringEntity,pWeaponInfo, fWeaponDamage, flags, bWasKilledOrInjured, vStart, pResult, vRagdollImpulseDir, fRagdollImpulseMag);
	}
	else
	{
		bNeedRagdollTask = true;
	}


	CTask* pTaskResult = NULL;
	if(bNeedRagdollTask)
	{
		CTaskNMBehaviour* pTaskNM = NULL;

		if(pWeaponInfo->GetDamageType() == DAMAGE_TYPE_FIRE)
		{
			if(bWasKilledOrInjured)
			{
				pTaskNM = rage_new CTaskNMOnFire(1000, 30000, 0.3f);

				if(CTaskNMBehaviour::ms_bUseParameterSets && !pHitPed->CheckAgilityFlags(AF_RAGDOLL_ON_FIRE_STRONG))
				{
					static_cast<CTaskNMOnFire*>(pTaskNM)->SetType(CTaskNMOnFire::ONFIRE_WEAK);
				}
			}
			else
			{
				pTaskResult = rage_new CTaskComplexOnFire(uWeaponHash);
			}
		}
		else if(pWeaponInfo->GetDamageType() == DAMAGE_TYPE_EXPLOSIVE)
		{
			Vector3 vExplosionPos = vStart;
	
			// Run explosion task.
			pTaskNM = rage_new CTaskNMExplosion(1000, 30000, vExplosionPos);

			if (pHitPed && pHitPed->GetFacialData())
			{
				if (pHitPed->IsLocalPlayer())
				{
					pHitPed->GetFacialData()->PlayDyingFacialAnim(pHitPed);
				}
				else
				{
					pHitPed->GetFacialData()->PlayPainFacialAnim(pHitPed);
				}
			}
		}
		else if (pHitPed->GetPedResetFlag(CPED_RESET_FLAG_WasHitByVehicleMelee))
		{
			u32 minTime = (u32)(pHitPed->IsPlayer() ? CTaskNMShot::sm_Tunables.m_MinimumShotReactionTimePlayerMS : CTaskNMShot::sm_Tunables.m_MinimumShotReactionTimeAIMS);
			pTaskNM = rage_new CTaskNMShot( pHitPed,
				minTime,
				10000,
				pFiringEntity,
				uWeaponHash,
				pResult->GetHitComponent(),
				pResult->GetHitPosition(),
				fRagdollImpulseMag * vRagdollImpulseDir,
				pResult->GetHitNormal() );
		}
		else if( flags.IsFlagSet( CPedDamageCalculator::DF_MeleeDamage ) && 
				!flags.IsFlagSet( CPedDamageCalculator::DF_SelfDamage ) )
		{
			Vector3 vHitNormal;
			Vector3 vHitPos;
			s32 nHitComponent;
			if(pResult)
			{
				vHitNormal.Negate(pResult->GetHitNormal());
				vHitPos = pResult->GetHitPosition();
				nHitComponent = (s32)pResult->GetHitComponent();
				taskDisplayf("melee hit: position (x:%.3f, y:%.3f, z:%.3f) Normal (x:%.3f, y:%.3f, z:%.3f) ragdollImpulseDir (x:%.3f, y:%.3f, z:%.3f)"
					, vHitPos.x, vHitPos.y, vHitPos.z
					, vHitNormal.x, vHitNormal.y, vHitNormal.z
					, vRagdollImpulseDir.x, vRagdollImpulseDir.y, vRagdollImpulseDir.z);
			}
			else
			{
				vHitNormal = -VEC3V_TO_VECTOR3(pHitPed->GetTransform().GetForward());
				vHitPos = VEC3V_TO_VECTOR3(pHitPed->GetTransform().GetPosition());
				nHitComponent = -1;
			}

			Vector3 vMeleeRagdollImpulseDir = vRagdollImpulseDir;

			// Apply script-set force modifier to impulse direction
			if (pFiringEntity && pFiringEntity->GetIsTypePed())
			{
				const CPed* pFiringPed = static_cast<CPed*>(pFiringEntity);
				if (pFiringPed->IsPlayer() && uWeaponHash != pFiringPed->GetDefaultUnarmedWeaponHash())
				{
					vMeleeRagdollImpulseDir *= pFiringPed->GetPlayerInfo()->GetPlayerMeleeWeaponForceModifier();
				}
			}

			pTaskNM = rage_new CTaskNMFlinch(1000, 10000, VEC3V_TO_VECTOR3(pFiringEntity ? pFiringEntity->GetTransform().GetPosition() : Vec3V(V_ZERO)), pFiringEntity, CTaskNMFlinch::FLINCHTYPE_MELEE, 
				uWeaponHash, nHitComponent, false, pHitPed, &vHitPos, &vHitNormal, &vMeleeRagdollImpulseDir);
			Assert(pTaskNM);

			if (CTaskNMBehaviour::ms_bUseParameterSets && !pHitPed->CheckAgilityFlags(AF_DONT_FLINCH_ON_MELEE))
			{
				static_cast<CTaskNMFlinch*>(pTaskNM)->SetType(CTaskNMFlinch::FLINCHTYPE_MELEE_PASSIVE);
			}
			else
			{
				static_cast<CTaskNMFlinch*>(pTaskNM)->SetType(CTaskNMFlinch::FLINCHTYPE_MELEE);
			}

			bTaskAppliesImpulse = true;
		}
		else if( uWeaponHash == WEAPONTYPE_FALL  || (flags.IsFlagSet( CPedDamageCalculator::DF_MeleeDamage ) && ( bWasKilledOrInjured || flags.IsFlagSet( CPedDamageCalculator::DF_SelfDamage ) )))
		{
			static float stiffness = 0.0f;
			static float damping = 0.0f;
			pTaskNM = rage_new CTaskNMRelax(2000, 60000, stiffness, damping);
		}
		else if((uWeaponHash == WEAPONTYPE_SMOKEGRENADE || uWeaponHash == WEAPONTYPE_DLC_BOMB_GAS || uWeaponHash == WEAPONTYPE_DLC_BZGAS_MK2) && pHitPed->HasHurtStarted())
		{
			pTaskNM = rage_new CTaskNMHighFall(1000, NULL, CTaskNMHighFall::HIGHFALL_SLOPE_SLIDE);
		}
		else if(uWeaponHash == WEAPONTYPE_ROTORS)
		{
			Vector3 vPedVelocity(pHitPed->GetVelocity());
			if (MagSquared(pHitPed->GetGroundVelocityIntegrated()).Getf() > vPedVelocity.Mag2())
			{
				vPedVelocity = VEC3V_TO_VECTOR3(pHitPed->GetGroundVelocityIntegrated());
			}
			pTaskNM = rage_new CTaskNMBrace(1000, 10000, pFiringEntity, CTaskNMBrace::BRACE_DEFAULT, vPedVelocity);
		}
		else if (uWeaponHash == WEAPONTYPE_DLC_SNOWBALL)
		{
			eAnimBoneTag iHitBoneTag = pHitPed->GetBoneTagFromRagdollComponent((int)pResult->GetHitComponent());
			float impulseMag = pWeaponInfo->GetForceHitPed(iHitBoneTag, true, 0.0f);

			u32 minTime = (u32)(pHitPed->IsPlayer() ? CTaskNMShot::sm_Tunables.m_MinimumShotReactionTimePlayerMS : CTaskNMShot::sm_Tunables.m_MinimumShotReactionTimeAIMS);
			pTaskNM = rage_new CTaskNMShot(pHitPed, minTime, 10000, pFiringEntity, uWeaponHash,  pResult->GetHitComponent(), pResult->GetHitPosition(), impulseMag * vRagdollImpulseDir, pResult->GetHitNormal());
		}
		else
		{
			if(pResult)
			{
				bool bShotByPlayer = false;
				if(pFiringEntity && pFiringEntity->GetIsTypePed() && ((CPed*)pFiringEntity)->IsPlayer())
				{
					bShotByPlayer = true;
				}

				if(pTaskNM == NULL)
				{
					// Check if a shot task has already been created this frame
					CTaskNMShot *shotTask = NULL;
					if (pHitPed->GetPedIntelligence())
					{
						if (CEventDamage *damageEvent = static_cast<CEventDamage*>(pHitPed->GetPedIntelligence()->GetEventOfType(EVENT_DAMAGE)))
						{
							if (damageEvent->GetPhysicalResponseTask() && damageEvent->GetPhysicalResponseTask()->GetTaskType() == CTaskTypes::TASK_NM_CONTROL)
							{
								const CTaskNMControl *controlTask = smart_cast<const CTaskNMControl*>(damageEvent->GetPhysicalResponseTask());
								if (controlTask->GetForcedSubTask() && controlTask->GetForcedSubTask()->GetTaskType() == CTaskTypes::TASK_NM_SHOT)
								{
									shotTask = (CTaskNMShot*) controlTask->GetForcedSubTask();
									shotTask->UpdateShot(pHitPed, pFiringEntity, uWeaponHash, pResult->GetHitComponent(), pResult->GetHitPosition(), fRagdollImpulseMag * vRagdollImpulseDir, pResult->GetHitNormal());
								
									// Set the task result so that the logic below doesn't choke
									pTaskResult = const_cast<CTaskNMControl*>(controlTask);
									bUpdatedPreviousEvent = true;
								}
							}
						}
						else if (pHitPed->IsNetworkClone())
						{
							CTask* pTask = pHitPed->GetPedIntelligence()->GetActiveCloneTask();
							if (pTask && pTask->GetTaskType() == CTaskTypes::TASK_NM_CONTROL)
							{
								const CTaskNMControl *controlTask = smart_cast<const CTaskNMControl*>(pTask);
								if (controlTask && controlTask->GetForcedSubTask() && controlTask->GetForcedSubTask()->GetTaskType() == CTaskTypes::TASK_NM_SHOT)
								{
									shotTask = (CTaskNMShot*) controlTask->GetForcedSubTask();
									if (shotTask)
									{
										shotTask->UpdateShot(pHitPed, pFiringEntity, uWeaponHash, pResult->GetHitComponent(), pResult->GetHitPosition(), fRagdollImpulseMag * vRagdollImpulseDir, pResult->GetHitNormal());

										// Set the task result so that the logic below doesn't choke
										pTaskResult = const_cast<CTaskNMControl*>(controlTask);
										bUpdatedPreviousEvent = true;
									}
								}
							}
						}
					}

					if (!shotTask)
					{
						u32 minTime = (u32)(pHitPed->IsPlayer() ? CTaskNMShot::sm_Tunables.m_MinimumShotReactionTimePlayerMS : CTaskNMShot::sm_Tunables.m_MinimumShotReactionTimeAIMS);
						pTaskNM = rage_new CTaskNMShot(pHitPed, minTime, 10000, pFiringEntity, uWeaponHash, pResult->GetHitComponent(), pResult->GetHitPosition(), fRagdollImpulseMag * vRagdollImpulseDir, pResult->GetHitNormal());
					}

					// NM will take care of applying the bullet impulse
					if (NM_APPLIES_SHOT_IMPULSE && (pTaskNM || shotTask) && pTaskSimplest->GetTaskType() != CTaskTypes::TASK_NM_DANGLE) // When dangling we handle the impulse here
						bTaskAppliesImpulse = true;
				}
			} 
		}

		// If injured then we're going to send a die event which will create its own complex die task. We still
		// need to wrap the NM behaviour task with a control task though.
		if(pTaskNM)
		{
			u32 uNmControlFlags = CTaskNMControl::ALL_FLAGS_CLEAR;
			if(!bWasKilledOrInjured)
			{
				uNmControlFlags |= CTaskNMControl::DO_BLEND_FROM_NM;
			}

			pTaskResult = rage_new CTaskNMControl(pTaskNM->GetMinTime(), pTaskNM->GetMaxTime(), pTaskNM, uNmControlFlags);
		}

		weaponAssert(pTaskResult);
	}
	// If no new task was created but a shot task was already running then assume that it already applied impulse(s)
	else if (NM_APPLIES_SHOT_IMPULSE && pTaskSimplest != NULL && pTaskSimplest->GetTaskType() == CTaskTypes::TASK_NM_SHOT)
	{
		bTaskAppliesImpulse = true;
	}

	nmEntityDebugf(pHitPed, "CWeaponDamage::GenerateRagdollTask - returning ragdoll task %s", pTaskResult ? pTaskResult->GetName() : "None");
	return pTaskResult;
}

bool CWeaponDamage::CanDropWeaponWhenShot(const CWeapon& /*rWeapon*/, const CPed& /*rHitPed*/, const CWeapon& /*rHitWeapon*/)
{
#if 1 // Disable weapons being shot out of hands
	return false;
#else
	//The weapon doing the damage must not be unarmed.
	if(rWeapon.GetWeaponInfo()->GetIsUnarmed())
	{
		return false;
	}
	
	//Ensure the hit weapon info is valid.
	const CWeaponInfo* pHitWeaponInfo = rHitWeapon.GetWeaponInfo();
	if(!pHitWeaponInfo)
	{
		return false;
	}
	
	//Ensure the hit weapon can generate a pickup.
	if(pHitWeaponInfo->GetPickupHash() == 0)
	{
		return false;
	}
	
	//Check if the hit weapon is a gun.
	if(pHitWeaponInfo->GetIsGun())
	{
		//Check if this is the ped's only gun.
		const CPedInventory* pInv = rHitPed.GetInventory();
		if(!pInv || (pInv->GetWeaponRepository().GetNumGuns() <= 1))
		{
			return false;
		}
	}
	
	//Ensure the ped is not in a vehicle.
	if(rHitPed.GetIsInVehicle())
	{
		return false;
	}
	
	return true;
#endif
}

eRagdollTriggerTypes CWeaponDamage::GetRagdollTriggerForDamageType(const eDamageType damageType)
{
	switch(damageType)
	{
	case DAMAGE_TYPE_ELECTRIC:			return RAGDOLL_TRIGGER_ELECTRIC;
	case DAMAGE_TYPE_BULLET_RUBBER:		return RAGDOLL_TRIGGER_RUBBERBULLET;

	default:
		weaponAssertf(0, "No ragdoll trigger type is defined for damage type %i", (int)damageType);
		break;
	}

	return RAGDOLL_TRIGGER_ELECTRIC;
}

bool CWeaponDamage::IsEnduranceDamage(CPed* pPed, const CWeapon* pWeapon, const fwFlags32& flags)
{
	const CPlayerInfo* pPlayerInfo = pPed->GetPlayerInfo();
	if (!pPlayerInfo || pPlayerInfo->GetEnduranceManager().ShouldIgnoreEnduranceDamage())
	{
		return false;
	}

	if (flags.IsFlagSet(CPedDamageCalculator::DF_EnduranceDamageOnly))
	{
		return true;
	}
	
	if (pWeapon->GetWeaponInfo()->GetEnduranceDamage() > 0.0f)
	{
		return true;
	}

	return false;
}
