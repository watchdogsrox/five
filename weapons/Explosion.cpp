#include "Explosion.h"

// Rage headers
#include "parser/manager.h"
#include "pharticulated/articulatedcollider.h"
#include "phbound/boundcomposite.h"
#include "physics/gtaInst.h"
#include "fwmaths/angle.h"
#include "fwmaths/random.h"
#include "fragmentnm/manager.h"
#include "fwscene/stores/psostore.h"
#include "fwsys/fileExts.h"

// Framework headers
#include "vfx/vfxutil.h"
#include "vfx/channel.h"

// Game headers
#include "audio/frontendaudioentity.h"
#include "control/replay/ReplayBufferMarker.h"
#include "core/game.h"
#include "debug/Debug.h"
#include "debug/DebugScene.h"
#include "Event/EventDamage.h"
#include "event/EventGroup.h"
#include "event/EventMovement.h"
#include "Event/EventShocking.h"
#include "Event/ShockingEvents.h"
#include "frontend/MiniMap.h"
#include "modelInfo/vehicleModelInfo.h"
#include "game/weather.h"
#include "network/NetworkInterface.h"
#include "network/events/NetworkEventTypes.h"
#include "network/general/NetworkUtil.h"
#include "network/objects/entities/NetObjPlayer.h"
#include "network/Players/NetworkPlayerMgr.h"
#include "objects/dummyobject.h"
#include "peds/Ped.h"
#include "peds/PedIntelligence.h"
#include "peds/PedIntelligence/PedAILod.h"
#include "Peds/rendering/PedVariationDebug.h"
#include "physics/GtaArchetype.h"
#include "physics/iterator.h"
#include "physics/physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "renderer/PostProcessFXHelper.h"
#include "scene/DataFileMgr.h"
#include "scene/physical.h"
#include "scene/world/GameWorld.h"
#include "Script/script_areas.h"
#include "system/FileMgr.h"
#include "system/Pad.h"
#include "Task/Combat/TaskAnimatedHitByExplosion.h"
#include "Task/Combat/TaskDamageDeath.h"
#include "Task/Physics/TaskNM.h"
#include "Task/Physics/TaskNMExplosion.h"
#include "Task/Vehicle/TaskVehicleBase.h"
#include "Vfx/Systems/VfxExplosion.h"
#include "vfx/vehicleglass/VehicleGlassManager.h"
#include "Vfx/VfxHelper.h"
#include "vfx/systems/VfxWeapon.h"
#include "weapons/Explosion.h"
#include "weapons/ExplosionInst.h"
#include "weapons/projectiles/projectile.h"
#include "weapons/Weapon.h"
#include "weapons/WeaponEnums.h"
#include "weapons/WeaponDamage.h"
#include "vehicles/vehicle.h"
#include "Vehicles/Metadata/VehicleSeatInfo.h"
#include "vehicleAi/task/TaskVehicleAnimation.h"

#include "glassPaneSyncing/GlassPaneManager.h"

// Parser headers
#include "Explosion_parser.h"

// Macro to disable optimisations if set
WEAPON_OPTIMISATIONS()
NETWORK_OPTIMISATIONS()

// Defines
#define DEBUGEXPLOSION(X)

PARAM(logExplosionCreation, "Log when and where from explosions are created");

static const int MAX_NUM_COLLIDED_INSTS = 256;

static dev_u32 PED_SPINE_BONE = 9;
static dev_float STICKYBOMB_EXPLOSION_DAMPEN_FORCE_MODIFIER = 0.15f;

phInstBehaviorExplosionGta::CollidePair phInstBehaviorExplosionGta::ms_CollidedInsts[MAX_NUM_COLLIDED_INSTS];
int phInstBehaviorExplosionGta::ms_NumCollidedInsts = 0;


bank_float phInstBehaviorExplosionGta::m_LosOffsetToInst = 0.02f;
bank_float phInstBehaviorExplosionGta::m_LosOffsetAlongUp = 0.5f;

inline phInstBehaviorExplosionGta::phInstBehaviorExplosionGta() : phInstBehavior()
{
	m_ExplosionInst = NULL;
	m_LosOffset = Vec3V(V_ZERO);
}


inline phInstBehaviorExplosionGta::~phInstBehaviorExplosionGta()
{
}


inline void phInstBehaviorExplosionGta::Reset()
{
	// Let's set our behavior back to an initial state.
	Assert(m_ExplosionInst != NULL);
	m_Radius = 0.001f;

	m_RadiusSpeed = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(m_ExplosionInst->GetExplosionTag()).initSpeed * m_ExplosionInst->GetSizeScale();
	m_Width = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(m_ExplosionInst->GetExplosionTag()).directedWidth;
	// set initial life time to a small (less than a frame) non-zero value, to ensure we hit our max radius
	m_ExplosionTimer = rage::Max(0.001f, CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(m_ExplosionInst->GetExplosionTag()).directedLifeTime);
	
	//B*1815582: Increase smoke grenade duration in MP
	if (m_ExplosionInst->GetExplosionTag() == EXP_TAG_SMOKEGRENADE && NetworkInterface::IsGameInProgress())
	{
		m_ExplosionTimer *= 2.0f;
	}

	if (auto WeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_ExplosionInst->GetWeaponHash()))
	{
		m_ExplosionTimer *= WeaponInfo->GetEffectDurationModifier();
	}

	if(m_Instance->GetArchetype()->GetBound()->GetType() == phBound::SPHERE)
	{
		phBoundSphere *BoundSphere = static_cast<phBoundSphere *>(m_Instance->GetArchetype()->GetBound());
		BoundSphere->SetSphereRadius(m_Radius);
	}
	else
	{
		phBoundCapsule *BoundCapsule = static_cast<phBoundCapsule *>(m_Instance->GetArchetype()->GetBound());

		Vec3V vCapsuleCentrePos = m_ExplosionInst->GetPosWld() + (m_ExplosionInst->GetDirWld()*ScalarVFromF32(m_Radius * 0.5f));
		Mat34V vCapsuleMtx;
		CVfxHelper::CreateMatFromVecY(vCapsuleMtx, vCapsuleCentrePos, m_ExplosionInst->GetDirWld());
		m_Instance->SetMatrix(vCapsuleMtx);

		BoundCapsule->SetCapsuleSize(m_Width, m_Radius);
	}
}


inline bool phInstBehaviorExplosionGta::IsActive() const
{
	if(!m_ExplosionInst)
	{
		// It's probably not even valid to call this function in this
		// situation, but returning false seems like the safe thing
		// to do. /FF
		return false;
	}

	CExplosionTagData& expTagData = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(m_ExplosionInst->GetExplosionTag());
	float endRadius = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionEndRadius(expTagData);
	return m_Radius < endRadius * m_ExplosionInst->GetSizeScale();
}


inline void phInstBehaviorExplosionGta::Update(float TimeStep)
{
	Assert(IsActive());
	Assert(m_ExplosionInst != NULL);

	// update the instance radius
	float prevRadius = m_Radius;
	bool radiusChanged = false;
	float fPreviousRadiusSpeed = m_RadiusSpeed;
	m_RadiusSpeed += CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(m_ExplosionInst->GetExplosionTag()).decayFactor * m_RadiusSpeed * TimeStep;
	m_Radius += m_RadiusSpeed * TimeStep;

	CExplosionTagData& expTagData = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(m_ExplosionInst->GetExplosionTag());
	float endRadius = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionEndRadius(expTagData)*m_ExplosionInst->GetSizeScale();

	// deal with directed timed explosions - they need their radius clamped until they've expired
	if(m_ExplosionTimer > 0.0f)
	{
		// update the timer
		m_ExplosionTimer -= TimeStep;

		// timer not finished yet but the radius has exceeded the final value - subtract a little from the radius so it remains active
		// want to alway hit a frame of max radius, even if timer is zero
		if (m_Radius >= endRadius)
		{
			m_Radius = endRadius -= 0.001f;
			m_RadiusSpeed = fPreviousRadiusSpeed;
		}
		else if(m_ExplosionTimer <= 0.0f)
		{
			// set time remaining to a small (less than a frame, but non-zero) so we'll get a chance to hit our max radius
			m_ExplosionTimer = 0.001f;
		}

		//Check if the attach entity is valid.
		CEntity* pAttachEntity = m_ExplosionInst->GetAttachEntity();
		if(pAttachEntity)
		{
			//Check if the attach entity will explode when finished.
			if(CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(m_ExplosionInst->GetExplosionTag()).explodeAttachEntityWhenFinished)
			{
				//Add the event.
				u32 uTimeOfExplosion = fwTimer::GetTimeInMilliseconds() + (u32)(m_ExplosionTimer * 1000.0f);
				CEventPotentialBlast event(CAITarget(pAttachEntity), endRadius, uTimeOfExplosion);
				GetEventGlobalGroup()->Add(event);

				//Add the shocking event.
				CEventShockingPotentialBlast shockingEvent(*pAttachEntity);
				CShockingEventsManager::Add(shockingEvent);
			}
		}
	}
	else if(m_Radius > endRadius)
	{
		return;
	}

	// update the physical explosion
	if (m_ExplosionInst->GetPhysicalType()==PT_SPHERE)
	{
		// sphere explosions (normal)
		Assert(m_Instance->GetArchetype()->GetBound()->GetType()==phBound::SPHERE);

		phBoundSphere* pBoundSphere = static_cast<phBoundSphere *>(m_Instance->GetArchetype()->GetBound());
		prevRadius = pBoundSphere->GetRadius();

		pBoundSphere->SetSphereRadius(m_Radius);

		if (prevRadius!=m_Radius)
		{
			radiusChanged = true;
		}

		m_Instance->SetPosition(m_ExplosionInst->GetPosWld());
	}
	else
	{
		// capsule explosions (directed)
		Assert(m_Instance->GetArchetype()->GetBound()->GetType()==phBound::CAPSULE);
		Assert(m_ExplosionInst->GetPhysicalType()==PT_DIRECTED);

		phBoundCapsule* pBoundCapsule = static_cast<phBoundCapsule *>(m_Instance->GetArchetype()->GetBound());
		prevRadius = pBoundCapsule->GetRadiusAroundCentroid();

		Vec3V vCapsuleCentrePos = m_ExplosionInst->GetPosWld() + (m_ExplosionInst->GetDirWld()*ScalarVFromF32(m_Radius * 0.5f));
		Mat34V vCapsuleMtx;
		CVfxHelper::CreateMatFromVecY(vCapsuleMtx, vCapsuleCentrePos, m_ExplosionInst->GetDirWld());
		m_Instance->SetMatrix(vCapsuleMtx);

		pBoundCapsule->SetCapsuleSize(m_Width, m_Radius);

		if (prevRadius!=0.5f*m_Radius+m_Width)
		{
			radiusChanged = true;
		}
	}

	// tell the physics level that the instance has changed, and also tell the simulator that, even though it is inactive, it could still be colliding with other inactives.
	if (radiusChanged)
	{
		PHLEVEL->UpdateObjectLocationAndRadius(m_Instance->GetLevelIndex(),(Mat34V_Ptr)(NULL));
	}
	else
	{
		PHLEVEL->UpdateObjectLocation(m_Instance->GetLevelIndex());
	}

	// add wind disturbance
	if (m_ExplosionInst->GetPhysicalType()==PT_DIRECTED)
	{
		float radius = m_Radius;
		float endRadius = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionEndRadius(expTagData);
		float length = 1.0f - (m_Radius/(endRadius*m_ExplosionInst->GetSizeScale()));
		float force = length * CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionForceFactor(expTagData);
		force *= WIND_DIREXPLOSION_WIND_DIST_FORCE_MULT;

		g_weather.AddWindDirExplosion(m_ExplosionInst->GetPosWld(), m_ExplosionInst->GetDirWld(), length, radius, force);	
	}
	else
	{
		float radius = m_Radius;
		float endRadius = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionEndRadius(expTagData);
		float length = 1.0f - (m_Radius/(endRadius*m_ExplosionInst->GetSizeScale()));
		float force = length * CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionForceFactor(expTagData);
		force *= WIND_EXPLOSION_WIND_DIST_FORCE_MULT;

		g_weather.AddWindExplosion(m_ExplosionInst->GetPosWld(), radius, force);
	}
}

PF_PAGE(GTAExplosions, "GTA explosions");
PF_GROUP(Explosions);
PF_LINK(GTAExplosions, Explosions);

PF_TIMER(ProcessAllCollisions, Explosions);
PF_TIMER(CollideObjects, Explosions);
#define ENABLE_DETAILED_EXPLOSION_TIMERS 0
#if ENABLE_DETAILED_EXPLOSION_TIMERS
#define DETAILED_EXPL_TIMERS_ONLY(x) x
#else
#define DETAILED_EXPL_TIMERS_ONLY(x)
#endif
DETAILED_EXPL_TIMERS_ONLY( PF_TIMER(FindIntersections, Explosions); )
DETAILED_EXPL_TIMERS_ONLY( PF_TIMER(Occlusion, Explosions); )
DETAILED_EXPL_TIMERS_ONLY( PF_TIMER(SpeedCalculation, Explosions); )
DETAILED_EXPL_TIMERS_ONLY( PF_TIMER(ForceCalculation, Explosions); )
DETAILED_EXPL_TIMERS_ONLY( PF_TIMER(ForceApplication, Explosions); )
DETAILED_EXPL_TIMERS_ONLY( PF_TIMER(PostCollision, Explosions); )

void phInstBehaviorExplosionGta::AddCollidedEntity(CEntity* pEntity, phInst* pInst)
{
	const CExplosionTagData& explosionTagData = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(m_ExplosionInst->GetExplosionTag());
	if(explosionTagData.bOnlyAffectsLivePeds)
	{
		if(!pEntity->GetIsTypePed() || static_cast<CPed*>(pEntity)->IsDead())
		{
			return;
		}
	}

	if(explosionTagData.bIgnoreExplodingEntity && pEntity == m_ExplosionInst->GetExplodingEntity())
	{
		return;
	}
	
	ms_CollidedInsts[ms_NumCollidedInsts++] = CollidePair(this, pInst);
}

bool phInstBehaviorExplosionGta::CollideObjects(Vec::V3Param128 UNUSED_PARAM(timeStep), phInst* /*myInst*/, phCollider* /*myCollider*/, phInst* OtherInst, phCollider* /*otherCollider*/, phInstBehavior* /*otherInstBehavior*/)
{
	PF_FUNC(CollideObjects);
	Assert(m_ExplosionInst != NULL);

	// Check the level index of the instance is valid.
	if( !PHLEVEL->LegitLevelIndex(OtherInst->GetLevelIndex()) )
	{
		return false;
	}

	if(m_RadiusSpeed > 1.0f && ms_NumCollidedInsts < MAX_NUM_COLLIDED_INSTS)
	{
		Assert(OtherInst != NULL);

		// Skip breakable glass
		// if(OtherInst->GetClassType() >= phInst::PH_INST_GLASS)
		//	return false;
		
		// Check the include flags
		if(!PHLEVEL->MatchFlags(m_Instance->GetLevelIndex(), OtherInst->GetLevelIndex()))
		{
			return false;
		}

		CEntity* pOtherEntity = CPhysics::GetEntityFromInst(OtherInst);
		// It is no longer assured that a phInst has a valid CEntity pointer. So if we don't have one, don't try
		// to use it!
		if(!pOtherEntity)
		{
			return false;
		}

		if(pOtherEntity && pOtherEntity == m_ExplosionInst->GetIgnoreDamageEntity())
		{
			return false;
		}

		CPhysical* pOtherPhysical = pOtherEntity->GetIsPhysical() ? ((CPhysical*)pOtherEntity) : NULL;
		if( !pOtherEntity->IsCollisionEnabled() )
		{
			bool bForceAllowPedWithNoCollision = false;
			if(pOtherPhysical && pOtherPhysical->GetIsTypePed())
			{
				CPed* pOtherPed = static_cast<CPed *>(pOtherPhysical);

				if(pOtherPhysical->GetIsAttached() && pOtherPhysical->GetAttachFlag(ATTACH_FLAG_AUTODETACH_ON_RAGDOLL))
				{
					bForceAllowPedWithNoCollision = true;
				}
				else if(pOtherPed->GetPedResetFlag(CPED_RESET_FLAG_ForceExplosionCollisions))
				{
					bForceAllowPedWithNoCollision = true;
				}
				else if(pOtherPed->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodPhysics))
				{
					bForceAllowPedWithNoCollision = true;
				}
			}
			
			if(!bForceAllowPedWithNoCollision)
			{
				return false;
			}
		}

		if(pOtherPhysical && (pOtherPhysical->GetIgnoresExplosions() BANK_ONLY(&& !CPedVariationDebug::ms_bLetInvunerablePedsBeAffectedByFire) ))
		{
			// HACK B*2425992: If hit entity is an invincible vehicle, check if occupants are invincible too. 
			// If not, add them to the collided entity list so we can apply damage to them.
			if (pOtherEntity->GetIsTypeVehicle())
			{
				CVehicle *pVehicle = static_cast<CVehicle*>(pOtherEntity);
				if (pVehicle) 
				{
					if (pVehicle->GetVehicleOccupantsTakeExplosiveDamage())
					{
						AddCollidedVehicleOccupantsToList(pOtherEntity, true);
					}
					else
					{	// Even if we don't process occupants in an invincible vehicle, we always want to process attachments (climbing, etc.)
						AddPedsAttachedToCar(pVehicle, true);
					}
				}
			}

			return false;
		}

		if(pOtherPhysical && pOtherPhysical->GetIsTypeVehicle())
		{
			CVehicle *pVehicle = static_cast<CVehicle*>(pOtherPhysical);
			if(pVehicle->GetNoDamageFromExplosionsOwnedByDriver() && 
				((m_ExplosionInst->GetExplosionOwner() == pVehicle->GetDriver()) || (m_ExplosionInst->GetExplosionOwner()==pVehicle)))
			{
				return false;
			}
		}

		if(pOtherPhysical && (pOtherPhysical->m_nPhysicalFlags.bNotDamagedBySteam && m_ExplosionInst->GetExplosionTag() == EXP_TAG_DIR_STEAM))
		{
			return false;
		}

		if(pOtherPhysical && (pOtherPhysical->m_nPhysicalFlags.bNotDamagedBySmoke && m_ExplosionInst->GetDamageType() == DAMAGE_TYPE_SMOKE))
		{
			return false;
		}

		//Assert(m_Instance->GetArchetype()->GetBound()->GetType() == phBound::SPHERE);

		Vector3 ThisInstCenter = VEC3V_TO_VECTOR3(m_Instance->GetMatrix().GetCol3());// m_ExplosionInst->GetExplosionPosition();

		CExplosionTagData& expTagData = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(m_ExplosionInst->GetExplosionTag());
		float endRadius = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionEndRadius(expTagData);
		float speedRatio = 1.0f - (m_Radius/(endRadius*m_ExplosionInst->GetSizeScale()));

		if( pOtherEntity && IsEntitySmashable(pOtherEntity) && m_ExplosionInst->GetPhysicalType()!=PT_DIRECTED && m_ExplosionInst->GetDamageType()!=DAMAGE_TYPE_FIRE )
		{
			VfxExpInfo_s vfxExpInfo;
			vfxExpInfo.regdEnt = pOtherEntity;
			vfxExpInfo.vPositionWld = RCC_VEC3V(ThisInstCenter);
			vfxExpInfo.radius = m_Radius;
			bool bUseDampenedForce = false;

			// This is a hack to get around the "special" stockade vehicles which do not take explosion damage normally
			if( pOtherEntity->GetIsTypeVehicle() )
			{
				CVehicle* pVehicle = static_cast<CVehicle*>( pOtherEntity );
				bUseDampenedForce = m_ExplosionInst->GetExplosionTag() == EXP_TAG_STICKYBOMB && pVehicle->GetVehicleModelInfo() && pVehicle->GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_DAMPEN_STICKBOMB_DAMAGE );
			}

			float fForceMultiplier =  bUseDampenedForce ? STICKYBOMB_EXPLOSION_DAMPEN_FORCE_MODIFIER : 1.0f;
			CExplosionTagData& expTagData = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(m_ExplosionInst->GetExplosionTag());
			vfxExpInfo.force = speedRatio * CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionForceFactor(expTagData) * fForceMultiplier;
			vfxExpInfo.flags = VfxExpInfo_s::EXP_FORCE_DETACH | VfxExpInfo_s::EXP_REMOVE_DETATCHED;
			if (vfxExpInfo.force>0.0f)
			{
				g_vehicleGlassMan.StoreExplosion(vfxExpInfo);
			}
		}

		// Peds attached to vehicles are handled separately. Doing it this way means that either the car + all attached peds get hit
		//   or nothing gets hit. 
		if(pOtherEntity->GetIsTypePed())
		{
			if(const CEntity* pAttachParent = static_cast<CEntity*>(pOtherEntity->GetAttachParent()))
			{
				if(pAttachParent->GetIsTypeVehicle())
				{
					return false;
				}
			}
		}

		AddCollidedEntity(pOtherEntity,OtherInst);

		// Add in bike and helicopter rear passenger insts
		if( pOtherEntity->GetIsTypeVehicle() )
		{
			const bool bForceAddVehicleOccupants = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(m_ExplosionInst->GetExplosionTag()).bAlwaysCollideVehicleOccupants;
			AddCollidedVehicleOccupantsToList(pOtherEntity, bForceAddVehicleOccupants);
		}
	}

	return false;
}

void phInstBehaviorExplosionGta::AddCollidedVehicleOccupantsToList(CEntity* pEntity, bool bForceAddVehicleOccupants)
{
	CVehicle* pVehicle = static_cast<CVehicle*>(pEntity);
	// Add in any peds who are attached, but not in a vehicle
	AddPedsAttachedToCar(pVehicle, bForceAddVehicleOccupants);

	// Add in peds on bikes or in a car without roof or with roof open or without doors (e.g. golf carts).
	if( bForceAddVehicleOccupants || pVehicle->GetVehicleType()==VEHICLE_TYPE_BIKE || pVehicle->GetVehicleType()==VEHICLE_TYPE_BICYCLE || !pVehicle->CarHasRoof() || pVehicle->GetConvertibleRoofState() != CTaskVehicleConvertibleRoof::STATE_RAISED || pVehicle->GetBoneIndex((eHierarchyId)(VEH_DOOR_DSIDE_F)) == -1 )
	{
		for( s32 iSeat = 0; iSeat < pVehicle->GetSeatManager()->GetMaxSeats(); iSeat++ )
		{
			CPed* pPedInSeat = pVehicle->GetSeatManager()->GetPedInSeat(iSeat);
			if( pPedInSeat && pPedInSeat->GetFragInst() && ms_NumCollidedInsts < MAX_NUM_COLLIDED_INSTS && (!bForceAddVehicleOccupants || (bForceAddVehicleOccupants && !pPedInSeat->GetIgnoresExplosions())))
			{
				AddCollidedEntity(pPedInSeat, pPedInSeat->GetFragInst());
			}
		}
	}
}

inline bool phInstBehaviorExplosionGta::ActivateWhenHit() const
{
	return false;
}

inline float phInstBehaviorExplosionGta::GetWidth() const
{
	return m_Width;
}

inline float phInstBehaviorExplosionGta::GetRadius() const
{
	return m_Radius;
}

inline ScalarV_Out phInstBehaviorExplosionGta::GetRadiusV() const
{
	return ScalarVFromF32(m_Radius);
}

inline float phInstBehaviorExplosionGta::GetRadiusSpeed() const
{
	return m_RadiusSpeed;
}


inline void phInstBehaviorExplosionGta::SetExplosionInst(phGtaExplosionInst *ExplosionInst)
{
	m_ExplosionInst = ExplosionInst;
}


class PredicateSortByExplosion : public std::binary_function<const phInstBehaviorExplosionGta::CollidePair&, const phInstBehaviorExplosionGta::CollidePair&, bool>
{
public:
	bool operator()(const phInstBehaviorExplosionGta::CollidePair& left, const phInstBehaviorExplosionGta::CollidePair& right)
	{
		return (left.first < right.first);
	}
};


//------------------------------------------------------------------
// Handle collision effects for all active explosions on this frame.
//------------------------------------------------------------------
void phInstBehaviorExplosionGta::ProcessAllCollisions()
{
	PF_FUNC(ProcessAllCollisions);
	CExplosionManager::SetInsideExplosionUpdate(true);

	// Reset the break counter on the first update each frame
	if(CPhysics::GetIsFirstTimeSlice(CPhysics::GetCurrentTimeSlice()))
	{
		CExplosionManager::ResetExplosionBreaksThisFrame();
	}

	// Remove all of the objects activated by explosions, that are currently inactive
	CExplosionManager::RemoveDeactivatedObjects();

	int numCollisions = ms_NumCollidedInsts;
	if (numCollisions > 0)
	{
		CollidePair* firstPair = &ms_CollidedInsts[0];

		// Sort the collision array so that all the collisions with the same explosion end up next to each other
		std::sort(firstPair, firstPair + numCollisions, PredicateSortByExplosion()); 

		// loop over the array, informing each explosion we find of all the insts it is colliding with
		CollidePair* pairWithCurrentExplosion = firstPair;
		int collisionsWithCurrentExplosion = 1;

		// We avoid doing dynamic memory allocation by creating our own Hitpoint structures
		//  And we re-use the results structures with each iteration as well
		WorldProbe::CShapeTestHitPoint shapeTestIsects[MAX_INTERSECTIONS];
		WorldProbe::CShapeTestResults shapeTestResults(shapeTestIsects, MAX_INTERSECTIONS);

		for (int collisionIndex = 1; collisionIndex < numCollisions; ++collisionIndex)
		{
			CollidePair* collision = &ms_CollidedInsts[collisionIndex];

			if (collision->first != pairWithCurrentExplosion->first)
			{
				pairWithCurrentExplosion->first->ProcessCollisionsForOneFrame(pairWithCurrentExplosion, collisionsWithCurrentExplosion, shapeTestResults);
				collisionsWithCurrentExplosion = 1;
				pairWithCurrentExplosion = collision;
			}
			else
			{
				++collisionsWithCurrentExplosion;
			}
		}

		// One more call to get the last explosion on the list.
		pairWithCurrentExplosion->first->ProcessCollisionsForOneFrame(pairWithCurrentExplosion, collisionsWithCurrentExplosion, shapeTestResults);
	}
	
	// Clear out the list to reclaim the memory it used.
	ms_NumCollidedInsts = 0;

	CExplosionManager::SetInsideExplosionUpdate(false);
}


void phInstBehaviorExplosionGta::ComputeLosOffset()
{
	// Don't raise directed explosions, we need a better fix for this
	if(m_ExplosionInst->GetPhysicalType() != PT_DIRECTED)
	{
		Vec3V explosionCenter = m_Instance->GetPosition();
		// We want to move the center of the explosion for LOS purposes to avoid hitting bumps or curbs on the ground
		// We need to ensure that there isn't something directly above us though or else we might pass through map collision
		//   and end up not having LOS to something right next to us. 

		// We want to move the explosion 0.5m up along the z-axis. We need to make sure that we include the explosion offset distance 
		//   and a little extra breathing room. 
		Vec3V desiredOffsetDirection = Vec3V(V_Z_AXIS_WZERO);
		ScalarV desiredOffsetDistance = ScalarVFromF32(m_LosOffsetAlongUp);
		ScalarV openSpaceBuffer = Add(ScalarVFromF32(m_LosOffsetToInst),ScalarV(V_FLT_SMALL_2));
		ScalarV requiredOpenSpace = Add(desiredOffsetDistance,openSpaceBuffer);

		WorldProbe::CShapeTestSingleResult result;
		WorldProbe::CShapeTestProbeDesc probeDesc;
		probeDesc.SetResultsStructure(&result);
		probeDesc.SetStartAndEnd(VEC3V_TO_VECTOR3(explosionCenter), VEC3V_TO_VECTOR3(AddScaled(explosionCenter,desiredOffsetDirection,requiredOpenSpace)));
		probeDesc.SetOptions(WorldProbe::LOS_IGNORE_SHOOT_THRU|WorldProbe::LOS_IGNORE_POLY_SHOOT_THRU);
		probeDesc.SetTypeFlags(ArchetypeFlags::GTA_WEAPON_TEST);
		probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_WEAPON);
		probeDesc.SetContext(WorldProbe::LOS_Weapon);

		ScalarV distanceToMoveExplosion = desiredOffsetDistance;
		if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
		{
			// We did hit something, try to move the explosion as much as possible
			Assert(result[0].IsAHit());
			ScalarV actualOpenSpace = Scale(result[0].GetTV(),requiredOpenSpace);
			distanceToMoveExplosion = Max(Subtract(actualOpenSpace,openSpaceBuffer),ScalarV(V_ZERO));
		}

		// Adjust the final LOS position
		m_LosOffset = Scale(desiredOffsetDirection,distanceToMoveExplosion);
	}
	else
	{
		m_LosOffset = Vec3V(V_ZERO);
	}
}

bool phInstBehaviorExplosionGta::DoOcclusionTest(const CEntity* pEntity)
{
	if(pEntity)
	{
		if(pEntity->GetIsTypePed())
		{
			const CPed* pPed = static_cast<const CPed*>(pEntity);
			if(pPed->GetPedResetFlag(CPED_RESET_FLAG_SkipExplosionOcclusion))
			{
				return false;
			}
		}
	}

	return true;
}

//----------------------------------------------------------------------------------
// Handle effects of this explosion touching the insts we collected in CollideObject.
//----------------------------------------------------------------------------------
dev_float sfExplosionMaxMoveSpeed = 20.0f;	// MAX_MOVE_SPEED 

#define DEFAULT_FRAME_RATE	(30.0f)

void phInstBehaviorExplosionGta::ProcessCollisionsForOneFrame(CollidePair* pairs, int pairCount, WorldProbe::CShapeTestResults& shapeTestResults)
{
	const CExplosionTagData& explosionData = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(m_ExplosionInst->GetExplosionTag());
	Vec3V ThisInstCenter = m_Instance->GetMatrix().GetCol3();// m_ExplosionInst->GetExplosionPosition();

	if(m_Instance->GetArchetype()->GetBound()->GetType() == phBound::CAPSULE)
	{
		// Shift the capsule center to the explosion source end
		ThisInstCenter = Subtract(ThisInstCenter, Scale( m_Instance->GetMatrix().GetCol1(), Scale(GetRadiusV(), ScalarV(V_HALF)) ) );
	}

	Vec3V vExplosionCenterForLOS = Add(ThisInstCenter,m_LosOffset);

	// For speed current area facing calculation doesn't do a good job of determining the actual overlap between the 
	//   explosion shape and the object. This leads to large objects receiving much higher forces since their whole
	//   bound is considered inside the explosion. 
	ScalarV MaxAreaFacing;
	if(m_ExplosionInst->GetPhysicalType() == PT_DIRECTED)
	{
		const ScalarV width = ScalarVFromF32(GetWidth());
		MaxAreaFacing = Scale(ScalarV(V_PI),Scale(width,width));
	}
	else
	{
		const ScalarV radius = GetRadiusV();
		MaxAreaFacing = Scale(ScalarV(V_PI),Scale(radius,radius));
	}

	// Loop over all the objects and process their collisions
	for (int pairIndex = 0; pairIndex < pairCount; ++pairIndex)
	{
		// Find the position on the other bound whereat to apply the force.
		shapeTestResults.Reset();

		phInst* OtherInst = pairs[pairIndex].second;
		Assert(OtherInst);
		if( !OtherInst )
		{
			Displayf("OtherInst is NULL");
			continue;
		}

		if(!CPhysics::GetLevel()->LegitLevelIndex(OtherInst->GetLevelIndex()))
			continue;

		phHandle otherHandle = PHLEVEL->GetHandle(OtherInst->GetLevelIndex());
		CExplosionOcclusionCache::COcclusionResult::Status occlusionStatus = m_ExplosionInst->GetOcclusionCache().FindOcclusionStatus(otherHandle);
		if(occlusionStatus == CExplosionOcclusionCache::COcclusionResult::OCCLUDED)
		{
			// Early out if we've already been occluded and the result isn't outdated
			continue;
		}

		// Explosions shouldn't collide against fixed instances
		Assert(PHLEVEL->GetState(OtherInst->GetLevelIndex()) != phLevelBase::OBJECTSTATE_FIXED);

		CEntity* pOtherEntity = CPhysics::GetEntityFromInst(OtherInst);
		CPhysical* pOtherPhysical = pOtherEntity->GetIsPhysical() ? ((CPhysical*)pOtherEntity) : NULL;

		if (pOtherEntity && m_ExplosionInst)
		{
			if ( m_ExplosionInst->GetExplosionOwner() && NetworkInterface::IsDamageDisabledInMP(*pOtherEntity, *m_ExplosionInst->GetExplosionOwner()))
			{
				continue;
			}

			if (m_ExplosionInst->GetExplodingEntity() && NetworkInterface::IsDamageDisabledInMP(*pOtherEntity, *m_ExplosionInst->GetExplodingEntity()))
			{
				continue;
			}
		}

		if(m_ExplosionInst && pOtherPhysical)
		{
			// Only apply damage if the physical can be damaged
			if (!pOtherPhysical->CanPhysicalBeDamaged(m_ExplosionInst->GetExplosionOwner(), m_ExplosionInst->GetWeaponHash(), !pOtherPhysical->GetIsTypePed(), m_ExplosionInst->GetDisableDamagingOwner()))
			{
				continue;
			}
			else if (NetworkInterface::IsGameInProgress() && !pOtherPhysical->GetNetworkObject() && pOtherPhysical->GetIsTypeObject())
			{
				CObject* pObject = SafeCast(CObject, pOtherPhysical);
				
				bool bClone = m_ExplosionInst->GetExplosionOwner() ? NetworkUtils::IsNetworkCloneOrMigrating(m_ExplosionInst->GetExplosionOwner()) : false;

				// prevent clone explosions affecting non-networked props (these may explode, triggering multiple explosion events)
				if ((bClone || m_ExplosionInst->GetNetworkIdentifier().IsClone()) && CNetObjObject::HasToBeCloned(pObject))
				{
					continue;
				}
			}
		}

		// Check if too many objects have activated recently
		if(!CExplosionManager::AllowActivations())
		{
			// Objects which are already active, the exploding object, peds, and vehicles are exempt
			if(	!PHLEVEL->IsActive(OtherInst->GetLevelIndex()) && 
				!pOtherEntity->GetIsTypePed() &&
				!pOtherEntity->GetIsTypeVehicle() && 
				pOtherEntity != m_ExplosionInst->GetExplodingEntity() )
			{
				continue;
			}
		}

		// B*2031517: Don't activate vehicle debris objects if we're trying to reduce how many breaking parts are flying around to help lighten the physics load in chaotic situations.
		if(pOtherEntity->GetIsTypeObject() && static_cast<CObject *>(pOtherEntity)->m_nObjectFlags.bVehiclePart && CVehicleDamage::ms_fBreakingPartsReduction >= 0.5f)
			continue;

		const bool bIsPed = pOtherEntity && pOtherEntity->GetIsTypePed();

		if( !OtherInst->GetArchetype() )
		{
			Displayf("otherInst doesn't have archetype");
			continue;
		}

		if( !OtherInst->GetArchetype()->GetBound() )
		{
			Displayf("inst: %s doesn't have bound", OtherInst->GetArchetype()->GetFilename());
			continue;
		}
		phBound &OtherBound = *OtherInst->GetArchetype()->GetBound();

		ScalarV OtherInstRadius = OtherBound.GetRadiusAroundCentroidV();
		ScalarV OtherInstRadiusSQR = Scale(OtherInstRadius, OtherInstRadius);
		ScalarV otherRadiusCubed = Scale(OtherInstRadius, OtherInstRadiusSQR);
		Vec3V otherBoundBBMax = OtherBound.GetBoundingBoxMax();
		Vec3V otherBoundBBMin = OtherBound.GetBoundingBoxMin();

		const s32 iMaxIntersections = m_Instance->GetArchetype()->GetBound()->GetType() == phBound::CAPSULE && !pOtherEntity->GetIsTypePed() ? 1 : MAX_INTERSECTIONS;

		Vec3V otherInstLosPosition;
		bool ignoreBoundOffset = false;
		if( pOtherEntity->GetIsTypeVehicle() &&
			( static_cast< CVehicle* >( pOtherEntity )->GetSpecialFlightModeRatio() == 1.0f ||
			  pOtherEntity->GetModelNameHash() == MI_CAR_PBUS2.GetName().GetHash() ) )
		{
			ignoreBoundOffset = true;
		}

		if(!FindIntersectionsOriginal(OtherInst, otherInstLosPosition, shapeTestResults, iMaxIntersections, ignoreBoundOffset))
		{
			// The explosion missed the other object.
			continue;
		}

		// Only run occlusion if our current results aren't outdated.
		if(!explosionData.bNoOcclusion && 
			DoOcclusionTest(pOtherEntity))
		{
			if(occlusionStatus == CExplosionOcclusionCache::COcclusionResult::OUTDATED)
			{
				Assert(shapeTestResults.GetNumHits()>0);

				// Use the first intersection to do the LOS check
				DETAILED_EXPL_TIMERS_ONLY( PF_START( Occlusion ); )
				bool occluded = false;

				Vec3V hitPosition = otherInstLosPosition;

				// Move the start position back a bit in case its inside the wall
				Vec3V vToInst = hitPosition - vExplosionCenterForLOS;
				vToInst = NormalizeSafe(vToInst, Vec3V(V_X_AXIS_WZERO));
				Vec3V vLosStartPos = AddScaled(vExplosionCenterForLOS, vToInst, ScalarVFromF32(m_LosOffsetToInst));

				WorldProbe::CShapeTestProbeDesc probeDesc;
				probeDesc.SetStartAndEnd(VEC3V_TO_VECTOR3(vLosStartPos), VEC3V_TO_VECTOR3(hitPosition));
				probeDesc.SetOptions(WorldProbe::LOS_IGNORE_SHOOT_THRU|WorldProbe::LOS_IGNORE_SHOOT_THRU_FX);
				probeDesc.SetTypeFlags(ArchetypeFlags::GTA_WEAPON_TEST);
				probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_WEAPON);
				probeDesc.SetContext(WorldProbe::LOS_Weapon);
				probeDesc.SetStateIncludeFlags(phLevelBase::STATE_FLAG_FIXED);
				if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
				{
					// There's something in the way.
					occluded = true;

					// Null the dynamicinst pointer so phInstBehaviorExplosionGta::ApplyFragDamage() does not apply damage to the occluded instance
					pairs[pairIndex].second = NULL;
				}
				DETAILED_EXPL_TIMERS_ONLY( PF_STOP( Occlusion ); )

				// Update the occlusion cache
				occlusionStatus = (occluded ? CExplosionOcclusionCache::COcclusionResult::OCCLUDED : CExplosionOcclusionCache::COcclusionResult::NOT_OCCLUDED);
				m_ExplosionInst->GetOcclusionCache().UpdateOcclusionStatus(otherHandle,occlusionStatus,CExplosionOcclusionCache::ComputeOcclusionResultLifeTime(pOtherEntity,occlusionStatus));

				if(occluded)
				{
					// if the first intersection with this instance is occluded, disregard all intersections and move to the next instance
					continue;
				}
			}
			// We should've early outed if we're occluded and gotten fresh results if we're out of date
			Assert(occlusionStatus == CExplosionOcclusionCache::COcclusionResult::NOT_OCCLUDED);
		}

		// Components may become invalid if the instance changes LOD during iteration
		// Store the high LOD component of instances prior to iterating below and then map to the current LOD during iteration
		for(WorldProbe::ResultIterator it = shapeTestResults.begin(); it < shapeTestResults.last_result(); ++it)
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

		// Apply forces to all intersections
		for(WorldProbe::ResultIterator it = shapeTestResults.begin(); it < shapeTestResults.last_result(); ++it)
		{
			DETAILED_EXPL_TIMERS_ONLY( PF_START( SpeedCalculation ); )

			// Instances may get deleted because of ApplyImpact calls earlier in the loop
			// so we need to check they're still valid.
			if(!it->GetHitDetected() || !it->GetHitInst())
			{
				continue;
			}

			fragInst::ePhysicsLOD eLod = fragInst::PHYSICS_LOD_MAX;
			if(it->GetHitInst()->GetClassType() == PH_INST_FRAG_PED)
			{
				eLod = static_cast<fragInstNMGta*>(it->GetHitInst())->GetCurrentPhysicsLOD();
				int currentLODComponent = static_cast<fragInstNMGta*>(it->GetHitInst())->MapRagdollLODComponentHighToCurrent(it->GetHitComponent());
				if(currentLODComponent >= 0)
				{
					it->SetHitComponent(static_cast<u16>(currentLODComponent));
				}
			}

			// Find the velocity of the other object at the point of force application.
			phCollider *OtherCollider = (OtherInst->IsInLevel() && PHLEVEL->IsActive(OtherInst->GetLevelIndex())) ? PHSIM->GetCollider(OtherInst->GetLevelIndex()) : NULL;
			Vec3V OtherInstLocalVelocity(V_ZERO);

			if(OtherCollider != NULL)
			{
				OtherInstLocalVelocity = OtherCollider->GetLocalVelocity(it->GetHitPositionV().GetIntrin128());
			}
			else
			{
				OtherInstLocalVelocity = OtherInst->GetExternallyControlledLocalVelocity(it->GetHitPositionV().GetIntrin128());
			}

			Vec3V unitContactPosToExplosionCenter = ThisInstCenter;
			unitContactPosToExplosionCenter = Subtract(unitContactPosToExplosionCenter, it->GetHitPositionV());
			// If this is the attached entity, make sure the force is always the inverse direction
			if( m_ExplosionInst->GetPhysicalType() == PT_DIRECTED &&
				m_ExplosionInst->GetAttachEntity() != NULL &&
				m_ExplosionInst->GetAttachEntity() == CPhysics::GetEntityFromInst(OtherInst) )
			{
				unitContactPosToExplosionCenter = m_Instance->GetMatrix().GetCol1();
			}
			// If its a normal directed explosion, make sure the force is within a certain tolerance of the direction
			else if( m_ExplosionInst->GetPhysicalType() == PT_DIRECTED )
			{
				static dev_float PED_SPINE_DIRECTED_PROPORTION = 0.3f;
				static dev_float PED_DIRECTED_PROPORTION = 0.4f;
				static dev_float OTHER_DIRECTED_PROPORTION = 1.0f;
				ScalarV directedProportion;
				if(!bIsPed)
				{
					directedProportion = ScalarVFromF32(OTHER_DIRECTED_PROPORTION);
				}
				else if (it->GetHitComponent() == PED_SPINE_BONE)
				{
					directedProportion = ScalarVFromF32(PED_SPINE_DIRECTED_PROPORTION);
				}
				else
				{
					directedProportion = ScalarVFromF32(PED_DIRECTED_PROPORTION);
				}
			
				unitContactPosToExplosionCenter = Subtract( Scale(unitContactPosToExplosionCenter, (Subtract(ScalarV(V_ONE), directedProportion))), Scale(m_Instance->GetMatrix().GetCol1(), directedProportion) );
				
				unitContactPosToExplosionCenter = NormalizeSafe(unitContactPosToExplosionCenter, Vec3V(V_X_AXIS_WZERO));
				it->SetHitNormalV(unitContactPosToExplosionCenter);
			}
			else
			{
				unitContactPosToExplosionCenter = NormalizeSafe(unitContactPosToExplosionCenter, Vec3V(V_X_AXIS_WZERO));
			}
			ScalarV OtherInstRelativeSpeed = Add(ScalarVFromF32(m_RadiusSpeed), Dot(OtherInstLocalVelocity, unitContactPosToExplosionCenter));

			// We're probably going to use this later on so let's save it off before we transform into world space.
			// Need to do this after any changes to ISect's normal are made
			Vec3V Normal_OtherInstSpace = it->GetHitNormalV();

			Normal_OtherInstSpace = UnTransform3x3Ortho(OtherInst->GetMatrix(), Normal_OtherInstSpace);


			phGtaExplosionCollisionInfo collisionInfo;
			collisionInfo.m_OtherInst = OtherInst;
			collisionInfo.m_HitPos = it->GetHitPositionV();
			collisionInfo.m_ForceDir = Negate(it->GetHitNormalV());
			collisionInfo.m_bSelfCollision = (pOtherEntity == m_ExplosionInst->GetExplodingEntity());
			collisionInfo.m_Component = it->GetHitComponent();
			collisionInfo.m_ForceMag = ScalarV(V_ZERO);
			collisionInfo.m_UncappedForceMag = ScalarV(V_ZERO);

			BANK_ONLY(TUNE_GROUP_BOOL(EXPLOSIVE_TUNE, SIMULATE_ZERO_REL_SPEED, false));

			// Here we linearly interpolate between 0.0f and m_RadiusSpeed here on the distance from the center of the explosion.
			// I tried this, but it didn't work very well for objects that were very close to explosion center because they would rarely get pushed very hard.
			//float ExplosionSpeedAtPointOfContact = m_RadiusSpeed * ThisInstCenter.Dist(ISect.Position) / m_Radius;
			//float OtherInstRelativeSpeed =  ExplosionSpeedAtPointOfContact + OtherInstLocalVelocity.Dot(ISect.Normal);
			DETAILED_EXPL_TIMERS_ONLY( PF_STOP( SpeedCalculation ); )
			if(IsGreaterThanAll(OtherInstRelativeSpeed, ScalarV(V_ZERO)) != 0 BANK_ONLY(&& !SIMULATE_ZERO_REL_SPEED))
			{
				DETAILED_EXPL_TIMERS_ONLY( PF_START( ForceCalculation ); )

				// In order to determine the force to apply on the object, we need to know the surface area of the object that is facing the blast.  We do this
				//   by approximating the object as either a sphere or a box, the choice of which is determined by which is a tighter fit (which is smaller).
				ScalarV ApproxAreaFacing;

				// Calculate the volume of the bounding sphere.
				ScalarV VolSphere = Scale( ScalarVFromF32((4.0f / 3.0f) * PI), otherRadiusCubed );

				// Calculate the volume of the bounding box.
				Vec3V BoxExtents(otherBoundBBMax);
				BoxExtents = Subtract(BoxExtents, otherBoundBBMin);
				ScalarV VolBox = Scale(BoxExtents.GetX(), Scale(BoxExtents.GetY(), BoxExtents.GetZ()));

				// Peds always use the sphere approximation
				if(!bIsPed && IsGreaterThanAll(VolSphere, VolBox) != 0 )
				{
					// The box is going to be the better approximation for this bound.
					ScalarV areaXAxis = Abs(Scale(Normal_OtherInstSpace.GetX(), Scale(BoxExtents.GetY(), BoxExtents.GetZ())));
					ScalarV areaYAxis = Abs(Scale(Normal_OtherInstSpace.GetY(), Scale(BoxExtents.GetX(), BoxExtents.GetZ())));
					ScalarV areaZAxis = Abs(Scale(Normal_OtherInstSpace.GetZ(), Scale(BoxExtents.GetX(), BoxExtents.GetY())));
					ApproxAreaFacing = Add(areaXAxis, Add(areaYAxis, areaZAxis));
				}
				else
				{
					// Wow, the sphere actually won and is the tighter bounding volume.
					ApproxAreaFacing = Scale(ScalarV(V_TWO_PI), OtherInstRadiusSQR );
				}
				
				if(BANK_ONLY(CExplosionManager::ms_ClampAreaApproximation &&) !bIsPed)
				{
					ApproxAreaFacing = Min(ApproxAreaFacing,MaxAreaFacing);
				}

				// At long last, here's our force!
				CEntity* pOtherEntity = CPhysics::GetEntityFromInst(OtherInst);
				bool bUseDampenedForce = false;

				// This is a hack to get around the "special" stockade vehicles which do not take explosion damage normally
				if( pOtherEntity && pOtherEntity->GetIsTypeVehicle() )
				{
					CVehicle* pVehicle = static_cast<CVehicle*>( pOtherEntity );
					bUseDampenedForce = m_ExplosionInst->GetExplosionTag() == EXP_TAG_STICKYBOMB && pVehicle->GetVehicleModelInfo() && pVehicle->GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_DAMPEN_STICKBOMB_DAMAGE );
				}

				ScalarV forceMultiplier = bUseDampenedForce ? ScalarVFromF32(STICKYBOMB_EXPLOSION_DAMPEN_FORCE_MODIFIER) : ScalarV(V_ONE);
				CExplosionTagData& expTagData = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(m_ExplosionInst->GetExplosionTag());
				ScalarV forceFactor = ScalarVFromF32(CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionForceFactor(expTagData));
				ScalarV forceMag = Scale( Scale(forceFactor, forceMultiplier), Scale(ApproxAreaFacing, Scale(OtherInstRelativeSpeed, OtherInstRelativeSpeed)) );
				ScalarV hardcodedScale = ScalarVFromF32(0.2f);
				forceMag = Scale(forceMag, hardcodedScale);

				if(m_ExplosionInst->GetPhysicalType() == PT_DIRECTED)
				{
					static dev_float PED_DIRECTED_SPINE_MULTIPLIER = 25.0f;		// Increase force on spine bone - we want majority of force to be applied near COM
					if(it->GetHitComponent() == PED_SPINE_BONE && bIsPed)
					{
						forceMag = Scale(forceMag, ScalarVFromF32(PED_DIRECTED_SPINE_MULTIPLIER));
					}
				}

				//@@: location PHINSTBEHAVIOREXPLOSIONGTA_PROCESSCOLLISIONSFORONEFRAME_SET_FORCES
				collisionInfo.m_ForceMag = forceMag;
				collisionInfo.m_UncappedForceMag = forceMag;

				// Give derived classes a chance to modify the force if they want to.
				PreApplyForce(collisionInfo, m_ExplosionInst->GetExplosionTag());

				//@@: location PHINSTBEHAVIOREXPLOSIONGTA_PROCESSCOLLISIONSFORONEFRAME_DETAILED_TIMERS
				DETAILED_EXPL_TIMERS_ONLY( PF_STOP( ForceCalculation ); )
			}

			if(IsGreaterThanAll(collisionInfo.m_ForceMag, ScalarV(V_ZERO)) != 0)
			{
				Vec3V vecApplyForce = Scale(collisionInfo.m_ForceDir, collisionInfo.m_ForceMag);
				DETAILED_EXPL_TIMERS_ONLY( PF_START( ForceApplication ); )

				bool bSkipImpulse = false;

				if(pOtherEntity && pOtherEntity->GetIsTypePed())
				{
					CPed* pOtherPed = (CPed*)pOtherEntity;
					phGtaExplosionInst* pGtaExplosionInst = (phGtaExplosionInst*)m_ExplosionInst;
					pGtaExplosionInst->GetExplosionOwner();

					if(pOtherPed->GetAudioEntity())
					{
						pOtherPed->GetPedAudioEntity()->UpBodyfallForShooting();
					}

					// Artificially increase the test force for continuous directed explosions
					ScalarV testForce = collisionInfo.m_ForceMag;
					if(m_ExplosionInst->GetPhysicalType() == PT_DIRECTED)
					{
						testForce = Scale(testForce, ScalarV(V_TEN));
					}

					bool bSkipRagdoll = false;


					if (NetworkInterface::IsGameInProgress())
					{
						CEntity* pExplosionOwner = m_ExplosionInst->GetExplosionOwner();
						if (pExplosionOwner && pExplosionOwner->GetIsTypePed() && pOtherPed != pExplosionOwner)
						{
							// If the ped should ignore the damage, e.g., in passive mode.
							CPed* pPedOwner = static_cast<CPed*>(pExplosionOwner);
							if (!pPedOwner->IsAllowedToDamageEntity(NULL, pOtherPed))
							{
								bSkipRagdoll = true;
								bSkipImpulse = true;						
							}
						}
	
						//Avoid explosion reaction and damage if in a network game and player is hit or the explosion owner is a ped
						if (!CExplosionManager::IsFriendlyFireAllowed(pOtherPed, m_ExplosionInst))
						{							
							bSkipRagdoll = true;
							bSkipImpulse = true;
						}
					}

					if (!bSkipRagdoll)
					{
						// Don't activate peds in cars hit by water hydrants
						if (pOtherPed->GetIsAttachedInCar() && pGtaExplosionInst->GetExplosionTag() == EXP_TAG_DIR_WATER_HYDRANT)
						{
							bSkipRagdoll = true;
						}
						// Don't activate player ragdolls based on steam damage unless they are dead.
						else if (pOtherPed->IsAPlayerPed() && !pOtherPed->IsInjured() && pGtaExplosionInst->GetExplosionTag() == EXP_TAG_DIR_STEAM)
						{
							bSkipRagdoll = true;
						}
					}

					if(!bSkipRagdoll && CTaskNMBehaviour::CanUseRagdoll(pOtherPed, RAGDOLL_TRIGGER_EXPLOSION, pGtaExplosionInst->GetExplosionOwner(), testForce.Getf()))
					{
						if(pOtherPed->GetRagdollState()==RAGDOLL_STATE_ANIM || pOtherPed->GetRagdollState()==RAGDOLL_STATE_ANIM_DRIVEN)
						{
							Vec3V vExplosionPos = m_ExplosionInst->GetPosWld();

							if (pOtherPed->GetIsDeadOrDying())
							{
								CTaskDyingDead* pTask = static_cast<CTaskDyingDead*>(pOtherPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_DYING_DEAD));
								if (pTask)
								{
									pTask->SwitchToRagdoll(pOtherPed);
								}
#if !__NO_OUTPUT
								else
								{
									nmEntityDebugf(pOtherPed, "Failed to activate corpse ragdoll for explosion because the dead task isn't running");
								}
#endif //!__NO_OUTPUT
							}
							else
							{
								// Directed explosions generally apply a small continuous force so no need for a high minimum time...
								CTaskNMExplosion* pTaskNM = rage_new CTaskNMExplosion(m_Instance->GetArchetype()->GetBound()->GetType() == phBound::CAPSULE ? 2000 : 5000, 10000, RCC_VECTOR3(vExplosionPos));

								if (pOtherPed->IsNetworkClone())
								{
									if (!m_ExplosionInst->IsEntityAlreadyDamaged(pOtherPed))
									{
										CTaskNMControl* pNMControlTask = rage_new CTaskNMControl(pTaskNM->GetMinTime(), pTaskNM->GetMaxTime(), pTaskNM, CTaskNMControl::DO_BLEND_FROM_NM);
										pOtherPed->GetPedIntelligence()->AddLocalCloneTask(pNMControlTask, PED_TASK_PRIORITY_EVENT_RESPONSE_TEMP);
										pOtherPed->SwitchToRagdoll(*pNMControlTask);
									}
									else
									{
										delete pTaskNM;
									}
								}
								else
								{
									CEventSwitch2NM event(10000, pTaskNM);
									pOtherPed->SwitchToRagdoll(event);
								}
							}
	
							// The ragdoll may have just activated and switched LODs - re-map the hit component from old LOD to new LOD here
							if(it->GetHitInst()->GetClassType() == PH_INST_FRAG_PED && eLod < fragInst::PHYSICS_LOD_MAX && eLod != static_cast<fragInstNMGta*>(it->GetHitInst())->GetCurrentPhysicsLOD())
							{
								int highLODComponent = fragInstNM::MapRagdollLODComponentCurrentToHigh(it->GetHitComponent(), eLod);
								if(highLODComponent >= 0)
								{
									int currentLODComponent = static_cast<fragInstNMGta*>(it->GetHitInst())->MapRagdollLODComponentHighToCurrent(highLODComponent);
									if(currentLODComponent >= 0)
									{
										it->SetHitComponent(static_cast<u16>(currentLODComponent));
										collisionInfo.m_Component = it->GetHitComponent();
									}
								}
							}
						}
					}
					else
					{
						// if ragdoll isn't going to be used, pass the collision on to the animated ped
						collisionInfo.m_OtherInst = pOtherPed->GetAnimatedInst();
						collisionInfo.m_Component = 0;
					}
				}

				if(collisionInfo.m_OtherInst->IsInLevel())
				{
					// Dont apply a force in a direction if the ped is moving beyond a certain speed
					if( pOtherPhysical && IsGreaterThanAll(Dot(collisionInfo.m_ForceDir, VECTOR3_TO_VEC3V(pOtherPhysical->GetVelocity())), ScalarVFromF32(sfExplosionMaxMoveSpeed)) != 0 )
					{
						// Nothing, the object is already moving too fast
						// pOtherPhysical = pOtherPhysical;
					}
					else
					{
						CPed* pPed = bIsPed ? static_cast<CPed*>(pOtherEntity) : NULL;

						// Avoid crashing due to ragdolls switching LOD mid loop. They probably shouldn't do that but it's safer/simpler to just early out here instead of change that. 
						const phBound* pOtherBound = collisionInfo.m_OtherInst->GetArchetype()->GetBound();
						if(phBound::IsTypeComposite(pOtherBound->GetType()) && collisionInfo.m_Component >= static_cast<const phBoundComposite*>(pOtherBound)->GetNumBounds())
						{
							bSkipImpulse = true;
						}

						if(bSkipImpulse)
						{
							//do nothing
						}
						// Apply forces to ped components at the center of gravity rather then the center of each component
						else if(pPed && !pPed->GetUsingRagdoll() && collisionInfo.m_OtherInst == pPed->GetAnimatedInst())
						{
							if(!m_ExplosionInst->IsEntityAlreadyDamaged(pPed))
							{
								// Apply explosion forces
								ApplyExplosionImpulsesToAnimatedPed(pPed, vecApplyForce, ThisInstCenter);
							}
						}
						else if(pPed && collisionInfo.m_OtherInst == pPed->GetRagdollInst())
						{
							// Apply the force directly to the ragdoll for directed explosions... this avoids it looking too artificial
							if(m_Instance->GetArchetype()->GetBound()->GetType() == phBound::CAPSULE)		
							{
								//PHSIM->ApplyForce(collisionInfo.m_OtherInst->GetLevelIndex(), vecApplyForce,  ISect.GetPosition(), ISect.GetComponent(), sfExplosionBreakScale);
								//The default break force passed to rage is just a hard-coded value.
								float fExplosionBreakForce = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(m_ExplosionInst->GetOriginalExplosionTag()).fragDamage * DEFAULT_FRAME_RATE;
								PHSIM->ApplyForceAndBreakingForce(ScalarV(fwTimer::GetTimeStep() / CPhysics::GetNumTimeSlices()).GetIntrin128ConstRef(), collisionInfo.m_OtherInst->GetLevelIndex(), VEC3V_TO_VECTOR3(vecApplyForce), it->GetHitPosition(), it->GetHitComponent(), it->GetHitPartIndex(), fExplosionBreakForce);
							}
							// Only apply non-directed impulses to a ped once
							else if (!m_ExplosionInst->IsEntityAlreadyDamaged(pPed) && PHLEVEL->IsActive(pPed->GetRagdollInst()->GetLevelIndex()) && pPed->GetUsingRagdoll())
							{
								fragInstNMGta* pFragInst = pPed->GetRagdollInst();
								phCollider *pNonArticlatedCollider = CPhysics::GetSimulator()->GetCollider(pFragInst);

								if(pNonArticlatedCollider)
								{
									Assert(pFragInst->GetCacheEntry());
									Assert(dynamic_cast<phArticulatedCollider*>(pNonArticlatedCollider));
									phArticulatedCollider* pCollider = static_cast<phArticulatedCollider*>(pNonArticlatedCollider);
									if(pCollider)
									{
										Assert(dynamic_cast<phBoundComposite*>(pFragInst->GetArchetype()->GetBound()));
										phBoundComposite* pBoundComposite = static_cast<phBoundComposite*>(pFragInst->GetArchetype()->GetBound());

										// Stop all current behaviors
										ART::MessageParams msg;
										pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_STOP_ALL_MSG), &msg);

										// Apply a blanket force to the whole body
										ScalarV bodyScale(V_ZERO);
										ScalarV forceMag = Mag(vecApplyForce);
										vecApplyForce = NormalizeSafe(vecApplyForce, Vec3V(V_ZERO));
										int torqueComponent = 0;

										// If a blast force is strong and in a similar direction to a fast moving ragdoll's velocity, reset the ped's velocities 
										// before applying the new explosion.  Otherwise, the ragdoll will end up with too large a velocity and will often hit a spread eagle pose.
										if (forceMag.Getf() >= CTaskNMExplosion::sm_Tunables.m_Explosion.m_StrongBlastMagnitude && 
											pPed->GetVelocity().Mag2() > square(CTaskNMExplosion::sm_Tunables.m_Explosion.m_FastMovingPedSpeed) &&
											Dot(vecApplyForce, RCC_VEC3V(pPed->GetVelocity())).Getf() > 0.0f)
										{
											pCollider->Freeze();
										}
											
										// Ensure impulses give a decent arch
										bool notTooFarAboveHitPed = (ThisInstCenter.GetZf() - pPed->GetTransform().GetPosition().GetZf()) < CTaskNMExplosion::sm_Tunables.m_Explosion.m_MaxDistanceAbovePedPositionToClampPitch;
										if(notTooFarAboveHitPed && !pPed->IsDead())
										{
											// TODO -- See about finding a faster way to get the same results
											ScalarV dotval = vecApplyForce.GetZ();
											float angle = HALF_PI - rage::AcosfSafe(dotval.Getf());
											float angleMin = CTaskNMExplosion::sm_Tunables.m_Explosion.m_PitchClampMin * DtoR;
											float angleMax = CTaskNMExplosion::sm_Tunables.m_Explosion.m_PitchClampMax * DtoR;
											if (angle < angleMin || angle > angleMax)
											{
												float desiredAngle = angle < angleMin ? angleMin : angleMax;
												vecApplyForce.SetZ(ScalarV(V_ZERO));
												vecApplyForce = NormalizeSafe(vecApplyForce, Vec3V(V_X_AXIS_WZERO));
												Vec3V upVec(0.0f, 0.0f, tan(desiredAngle));
												vecApplyForce = Add(vecApplyForce, upVec);
												vecApplyForce = Normalize(vecApplyForce);
											}
										}

										// Clamp the explosion magnitude to peds
										ScalarV magClamp = ScalarVFromF32(CTaskNMExplosion::sm_Tunables.m_Explosion.m_MagnitudeClamp);
										forceMag = Clamp(forceMag, ScalarV(V_ZERO), magClamp);
										static dev_float deadPedExplosionClampScale = 0.7f;
										if (pPed->IsDead())
										{
											forceMag = Clamp(forceMag, ScalarV(V_ZERO), Scale(magClamp, ScalarVFromF32(deadPedExplosionClampScale)));
										}

										// Special handling for high-lod rage ragdolls
										if (pFragInst->GetNMAgentID() == -1)
										{
											if (pFragInst->GetType()->GetARTAssetID() >= 0)
											{
												bodyScale = ScalarVFromF32(CTaskNMExplosion::sm_Tunables.m_Explosion.m_HumanBodyScale);

												torqueComponent = RAGDOLL_SPINE3;
												int ragdollTorqueComponent = pFragInst->MapRagdollLODComponentHighToCurrent(RAGDOLL_SPINE3);
												if (Verifyf(ragdollTorqueComponent != -1, "Invalid ragdoll component %d", ragdollTorqueComponent)) 
												{
													torqueComponent = ragdollTorqueComponent;
												}
												
												// Apply an extra force to the chest
												{
													int comp = RAGDOLL_BUTTOCKS;
													int ragdollComponent = pFragInst->MapRagdollLODComponentHighToCurrent(RAGDOLL_BUTTOCKS);
													if (Verifyf(ragdollComponent != -1, "Invalid ragdoll component %d", ragdollComponent)) 
													{
														comp = ragdollComponent;
													}
													Matrix34 partWorld = RCC_MATRIX34(pBoundComposite->GetCurrentMatrix(comp));
													partWorld.Dot(RCC_MATRIX34(pFragInst->GetMatrix()));
													Vec3V vImp = vecApplyForce;
													vImp = Scale(vImp, Scale(forceMag, ScalarVFromF32(CTaskNMExplosion::sm_Tunables.m_Explosion.m_HumanPelvisScale)));
													pCollider->ApplyImpulse(vImp.GetIntrin128(), RCC_VEC3V(partWorld.d).GetIntrin128(), comp);
												}
												{
													int comp = RAGDOLL_SPINE0; 
													int ragdollComponent = pFragInst->MapRagdollLODComponentHighToCurrent(RAGDOLL_SPINE0);
													if (Verifyf(ragdollComponent != -1, "Invalid ragdoll component %d", ragdollComponent)) 
													{
														comp = ragdollComponent;
													}
													Matrix34 partWorld = RCC_MATRIX34(pBoundComposite->GetCurrentMatrix(comp));
													partWorld.Dot(RCC_MATRIX34(pFragInst->GetMatrix()));
													Vec3V vImp = vecApplyForce;
													vImp = Scale(vImp, Scale(forceMag, ScalarVFromF32(CTaskNMExplosion::sm_Tunables.m_Explosion.m_HumanSpine0Scale)));
													pCollider->ApplyImpulse(vImp.GetIntrin128(), RCC_VEC3V(partWorld.d).GetIntrin128(), comp);
												}
												{
													int comp = RAGDOLL_SPINE1; 
													int ragdollComponent = pFragInst->MapRagdollLODComponentHighToCurrent(RAGDOLL_SPINE1);
													if (Verifyf(ragdollComponent != -1, "Invalid ragdoll component %d", ragdollComponent)) 
													{
														comp = ragdollComponent;
													}
													Matrix34 partWorld = RCC_MATRIX34(pBoundComposite->GetCurrentMatrix(comp));
													partWorld.Dot(RCC_MATRIX34(pFragInst->GetMatrix()));
													Vec3V vImp = vecApplyForce;
													vImp = Scale(vImp, Scale(forceMag, ScalarVFromF32(CTaskNMExplosion::sm_Tunables.m_Explosion.m_HumanSpine1Scale)));
													pCollider->ApplyImpulse(vImp.GetIntrin128(), RCC_VEC3V(partWorld.d).GetIntrin128(), comp);
												}
											}
											else
											{
												// Handling for non NM agents
												bodyScale = ScalarVFromF32(CTaskNMExplosion::sm_Tunables.m_Explosion.m_AnimalBodyScale);
												torqueComponent = RAGDOLL_BUTTOCKS;

												// Apply an extra force to the pelvis
												int comp = RAGDOLL_BUTTOCKS; 
												Matrix34 partWorld = RCC_MATRIX34(pBoundComposite->GetCurrentMatrix(comp));
												partWorld.Dot(RCC_MATRIX34(pFragInst->GetMatrix()));
												Vec3V vImp = vecApplyForce;
												vImp = Scale(vImp, Scale(forceMag, ScalarVFromF32(CTaskNMExplosion::sm_Tunables.m_Explosion.m_AnimalPelvisScale)));
												pCollider->ApplyImpulse(vImp.GetIntrin128(), RCC_VEC3V(partWorld.d).GetIntrin128(), comp);
											}
										}
										else
										{
											bodyScale = ScalarVFromF32(CTaskNMExplosion::sm_Tunables.m_Explosion.m_NMBodyScale);
										}

										// Determine whether this is a blast from the side or back, and increase the impulse if so
										phBoundComposite *bound = pPed->GetRagdollInst()->GetCacheEntry()->GetBound();
										Matrix34 pelvisMat;
										pelvisMat.Dot(RCC_MATRIX34(bound->GetCurrentMatrix(RAGDOLL_BUTTOCKS)), RCC_MATRIX34(pPed->GetRagdollInst()->GetMatrix())); // The pelvis
										Vec3V dirFacing = -(RCC_MAT34V(pelvisMat).GetCol2());  
										dirFacing.SetZ(ScalarV(V_ZERO));
										dirFacing = NormalizeSafe(dirFacing, Vec3V(V_X_AXIS_WZERO));
										Vec3V dirSide = RCC_MAT34V(pelvisMat).GetCol1();  
										dirSide.SetZ(ScalarV(V_ZERO));
										dirSide = NormalizeSafe(dirSide, Vec3V(V_Z_AXIS_WZERO));
										ScalarV pitchDot = Abs(Dot(dirSide, vecApplyForce));
										ScalarV extraBlastPitchMult = SelectFT(IsGreaterThan(pitchDot, ScalarVFromF32(Cosf(CTaskNMExplosion::sm_Tunables.m_Explosion.m_PitchSideAngle * DtoR))), ScalarV(V_ONE), ScalarVFromF32(CTaskNMExplosion::sm_Tunables.m_Explosion.m_SideScale) * pitchDot);
										// True condition means facing front and is set with m_sideScale
										extraBlastPitchMult = SelectFT(IsGreaterThan(Dot(dirFacing, vecApplyForce), ScalarV(V_ZERO)), extraBlastPitchMult, ScalarVFromF32(CTaskNMExplosion::sm_Tunables.m_Explosion.m_SideScale));

										// Add some random pitch in the direction of the impulse
										mthRandom random;
										Vec3V vPitch;
										vPitch = Cross(Vec3V(V_Z_AXIS_WZERO), vecApplyForce);
										vPitch = NormalizeSafe(vPitch, Vec3V(V_X_AXIS_WZERO));
										vPitch = Scale( Scale(vPitch, extraBlastPitchMult), random.GetRangedV(ScalarVFromF32(CTaskNMExplosion::sm_Tunables.m_Explosion.m_PitchTorqueMin), ScalarVFromF32(CTaskNMExplosion::sm_Tunables.m_Explosion.m_PitchTorqueMax)) );
										if (pFragInst->GetNMAgentID() >= 0)
										{
											pPed->ApplyAngImpulse(RCC_VECTOR3(vPitch), VEC3_ZERO, RAGDOLL_BUTTOCKS, true);
											for (int pitchPart = RAGDOLL_SPINE0; pitchPart <= RAGDOLL_SPINE3; pitchPart++)
											{
												pPed->ApplyAngImpulse(RCC_VECTOR3(vPitch), VEC3_ZERO, pitchPart, true);
											}
										}
										else
										{
											pCollider->GetBody()->ApplyAngImpulse(torqueComponent, vPitch.GetIntrin128());
										}

										// Apply a blanket force to the whole body
										vecApplyForce = Scale(vecApplyForce, Scale(bodyScale, forceMag));
										if (pFragInst->GetNMAgentID() >= 0)
										{
											phArticulatedBody *body = pCollider->GetBody();
											Vec3V impulseToApply = Scale(vecApplyForce, ScalarVFromF32(CTaskNMExplosion::sm_Tunables.m_Explosion.m_BlanketForceScale));
											for (int iGravityPart = RAGDOLL_BUTTOCKS; iGravityPart < body->GetNumBodyParts(); iGravityPart++)
											{
												msg.reset();
												msg.addInt(NMSTR_PARAM(NM_APPLYIMPULSE_PART_INDEX), iGravityPart);
												msg.addFloat(NMSTR_PARAM(NM_APPLYIMPULSE_EQUALIZE_AMOUNT), 1.0f);
												msg.addVector3(NMSTR_PARAM(NM_APPLYIMPULSE_IMPULSE), impulseToApply.GetXf(), impulseToApply.GetYf(), impulseToApply.GetZf());
												msg.addVector3(NMSTR_PARAM(NM_APPLYIMPULSE_HIT_POINT), 0.0f, 0.0f, 0.0f);
												msg.addBool(NMSTR_PARAM(NM_APPLYIMPULSE_LOCAL_HIT_POINT_INFO), true);
												pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_APPLYIMPULSE_MSG), &msg);
											}
										}
										else
										{
											pCollider->GetBody()->ApplyGravityToLinks(vecApplyForce.GetIntrin128(), ScalarV(fwTimer::GetTimeStep() / CPhysics::GetNumTimeSlices()).GetIntrin128ConstRef(), pCollider->GetGravityFactor());
										}

										// Add a bit of random twist
										Vector3 torque = Vector3(0.0f, 0.0f, fwRandom::GetRandomNumberInRange(-CTaskNMExplosion::sm_Tunables.m_Explosion.m_ExtraTorqueTwistMax, CTaskNMExplosion::sm_Tunables.m_Explosion.m_ExtraTorqueTwistMax));
										if (pFragInst->GetNMAgentID() >= 0)
										{
											msg.reset();
											msg.addInt(NMSTR_PARAM(NM_APPLYIMPULSE_PART_INDEX), torqueComponent);
											msg.addVector3(NMSTR_PARAM(NM_APPLYIMPULSE_IMPULSE), torque.x, torque.y, torque.z);
											msg.addBool(NMSTR_PARAM(NM_APPLYIMPULSE_ANGULAR_IMPULSE), true);
											pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_APPLYIMPULSE_MSG), &msg);
										}
										else
										{
											pCollider->GetBody()->ApplyAngImpulse(torqueComponent, RCC_VEC3V(torque).GetIntrin128());
										}

										// Disable the injuredOnGround behavior for powerful blasts
										float dist2 = DistSquared(pPed->GetTransform().GetPosition(), m_ExplosionInst->GetPosWld()).Getf();
										if (pPed->GetHealth() <= 0.0f && (IsGreaterThanAll(forceMag, ScalarVFromF32(CTaskNMExplosion::sm_Tunables.m_Explosion.m_DisableInjuredBehaviorDistLimit)) != 0 || dist2 < square(CTaskNMExplosion::sm_Tunables.m_Explosion.m_DisableInjuredBehaviorDistLimit)))
										{ 
											pPed->GetPedIntelligence()->GetCombatBehaviour().DisableInjuredOnGroundBehaviour();
										}
									}
								}
							}
						}
						else
						{
							phInst* pOtherInst=collisionInfo.m_OtherInst;
							// force frag objects into the cache, otherwise they won't break apart the same way as when hit by guns
							if(pOtherInst->GetClassType()==PH_INST_FRAG_OBJECT)
							{
								fragInstGta* pFragInst = (fragInstGta*)collisionInfo.m_OtherInst;
								if(!pFragInst->GetCached())
								{
									pFragInst->PutIntoCache();
								}
							}

							// Respect the explosion scale of fragments
							float explosionScale = 1.0f;
							if(IsFragInst(pOtherInst))
							{
								const fragPhysicsLOD* physicsLOD = static_cast<fragInst*>(pOtherInst)->GetTypePhysics();
								explosionScale = physicsLOD->GetGroup(physicsLOD->GetChild(it->GetHitComponent())->GetOwnerGroupPointerIndex())->GetExplosionScale();
							}

							//PHSIM->ApplyForce(collisionInfo.m_OtherInst->GetLevelIndex(), vecApplyForce, ISect.GetPosition(), ISect.GetComponent(), sfExplosionBreakScale);

							bool bSkipImpulse = false;

							// Check for animating doors as they shouldn't be affected by explosion forces
							if(pOtherEntity && pOtherEntity->GetIsTypeVehicle())
							{
								CVehicle* pVehicle = (CVehicle*)pOtherEntity;

								// walk through the doors and check against the door component and state
								CCarDoor* pCarDoor = NULL;
								for(int i=0; i< pVehicle->GetNumDoors(); i++)
								{
									pCarDoor = pVehicle->GetDoor(i);
									Assert(pCarDoor);

									if( it->GetHitComponent() == pCarDoor->GetFragChild() &&
										pCarDoor->GetFlag( CCarDoor::RELEASE_AFTER_DRIVEN ) )
									{
										bSkipImpulse = true;
										break;
									}
								}

								//Avoid explosion reaction and damage if in a network game and vehicle is driven by a passive player
								if (NetworkInterface::IsGameInProgress())
								{
									if (!CExplosionManager::IsFriendlyFireAllowed(pVehicle, m_ExplosionInst))												
										bSkipImpulse = true;									
										
									if (!pVehicle->GetVehicleDamage()->CanVehicleBeDamagedBasedOnDriverInvincibility())
										bSkipImpulse = true;
								}

								if(!bSkipImpulse)
								{
									pVehicle->m_nVehicleFlags.bInMotionFromExplosion = true;
								}

								if(CVehicle::TooManyVehicleBreaksRecently())
								{
									explosionScale = 0.0f;
								}
							}

							// Don't break if we've already broken too much this frame
							if(!CExplosionManager::AllowBreaking())
							{
								explosionScale = 0.0f;
							}

							//The default break force passed to rage is just a hard-coded value.
							if( !bSkipImpulse )
							{
								ScalarV forceMag = Mag(vecApplyForce);
								vecApplyForce = NormalizeSafe(vecApplyForce, Vec3V(V_ZERO));

								// Ensure forces give a decent arch for explosion origins not too far above the vehicle's location
								ScalarV exploisionHeigtAboveVehicle = Subtract(ThisInstCenter, it->GetHitPositionV()).GetZ();	
								if( (IsLessThan(exploisionHeigtAboveVehicle, ScalarV(V_TWO)) & IsGreaterThanOrEqual(exploisionHeigtAboveVehicle, ScalarV(V_ZERO))).Getb() )
								{
									vecApplyForce.SetZ(0.01f);
									vecApplyForce = Normalize(vecApplyForce);
								}
								ScalarV dotval = vecApplyForce.GetZ();
								if(IsGreaterThanOrEqualAll(dotval, ScalarV(V_ZERO)))
								{
									// TODO -- See about finding a faster way to get the same results
									float angle = rage::AcosfSafe(dotval.Getf()) * (180.0f / PI);
									angle = 90.0f - angle;
									float minAngle = 30.0f;
									float maxAngle = 60.0f;
									if (angle < minAngle || angle > maxAngle)
									{
										float desiredAngle = angle < minAngle ? minAngle : maxAngle;
										desiredAngle *= (PI / 180.0f);
										vecApplyForce.SetZ(ScalarV(V_ZERO));
										vecApplyForce = NormalizeSafe(vecApplyForce, Vec3V(V_X_AXIS_WZERO));
										Vec3V upVec(0.0f,0.0f,tan(desiredAngle));
										vecApplyForce = Add(vecApplyForce, upVec);
										vecApplyForce = Normalize(vecApplyForce);
									}
								}

								//Set Crack Id for Explosions on Glass
								s32 crackId = g_vfxWeapon.GetBreakableGlassId(WEAPON_EFFECT_GROUP_EXPLOSION);
								CPhysics::SetSelectedCrack(crackId);
								float fExplosionBreakForce = explosionScale*CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(m_ExplosionInst->GetOriginalExplosionTag()).fragDamage * DEFAULT_FRAME_RATE;
								if(pOtherInst->GetClassType() == phInst::PH_INST_GLASS)
								{
									PHSIM->ApplyImpulse(pOtherInst->GetLevelIndex(), VEC3V_TO_VECTOR3(Scale(vecApplyForce, forceMag)), it->GetHitPosition(), it->GetHitComponent(), it->GetHitPartIndex(), fExplosionBreakForce );
								}
								else
								{
									PHSIM->ApplyForceAndBreakingForce(ScalarV(fwTimer::GetTimeStep() / CPhysics::GetNumTimeSlices()).GetIntrin128ConstRef(), collisionInfo.m_OtherInst->GetLevelIndex(), VEC3V_TO_VECTOR3(Scale(vecApplyForce, forceMag)), it->GetHitPosition(), it->GetHitComponent(), it->GetHitPartIndex(), fExplosionBreakForce, true);
								}
							}
						}
					}
					DETAILED_EXPL_TIMERS_ONLY( PF_START(PostCollision); )
					PostCollision(collisionInfo);
					DETAILED_EXPL_TIMERS_ONLY( PF_STOP(PostCollision); )
				}
				DETAILED_EXPL_TIMERS_ONLY( PF_STOP( ForceApplication ); )
			}
			else if (m_ExplosionInst->ShouldPostProcessCollisionsWithNoForce() && collisionInfo.m_OtherInst->IsInLevel())
			{
				DETAILED_EXPL_TIMERS_ONLY( PF_START(PostCollision); )
				PostCollision(collisionInfo);
				DETAILED_EXPL_TIMERS_ONLY( PF_STOP(PostCollision); )
			}
		}
	}
}


//-------------------------------------------------------------------------
// Explosion behaviour callback to adjust the force before it gets applied.
//-------------------------------------------------------------------------
void phInstBehaviorExplosionGta::PreApplyForce( phGtaExplosionCollisionInfo &collisionInfo, eExplosionTag explosionTag ) const
{
	CExplosionManager::ExplosionPreModifyForce(collisionInfo, explosionTag);
}

//-------------------------------------------------------------------------
// Explosion behaviour callback to do game-specific things like trigger damage.
//-------------------------------------------------------------------------
void phInstBehaviorExplosionGta::PostCollision(phGtaExplosionCollisionInfo& collisionInfo) const
{
	phGtaExplosionInst* pGtaExplosion = (phGtaExplosionInst*)m_ExplosionInst;

	Vec3V vExplosionPos = m_Instance->GetMatrix().GetCol3();
	
	if(m_Instance->GetArchetype()->GetBound()->GetType() == phBound::CAPSULE)
	{
		// Shift the capsule center to the explosion source end
		vExplosionPos = Subtract(vExplosionPos, Scale(m_Instance->GetMatrix().GetCol1(), Scale(GetRadiusV(), ScalarV(V_HALF))));
	}

	CExplosionManager::ExplosionImpactCallback(collisionInfo.m_OtherInst,
		collisionInfo.m_ForceMag,
		collisionInfo.m_UncappedForceMag,
		m_Radius,
		collisionInfo.m_ForceDir,
		collisionInfo.m_HitPos,
		pGtaExplosion->GetExplosionOwner(),
		pGtaExplosion->GetExplodingEntity(),
		vExplosionPos,
		pGtaExplosion,
		collisionInfo.m_Component);
}


//----------------------------------------------------
// Determine whether the explosion intersects a sphere.
//----------------------------------------------------
bool phInstBehaviorExplosionGta::TestAgainstSphere(Vector3::Param localCenter, Vector3::Param radius)
{
	phBound& ThisBound = *m_Instance->GetArchetype()->GetBound();

	if(ThisBound.GetType() == phBound::SPHERE)
	{
		Vector3 radiiSquared = Vector3(radius) + SCALARV_TO_VECTOR3(ThisBound.GetRadiusAroundCentroidV());
		radiiSquared.Multiply(radiiSquared);
		return Vector3(localCenter).Mag2V().IsLessOrEqualThanDoNotUse(radiiSquared);
	}
	else
	{
		Assert(ThisBound.GetType() == phBound::CAPSULE);
		phBoundCapsule& capsuleBound = static_cast<phBoundCapsule&>(ThisBound);

		Vec3V sphereCenter = VECTOR3_TO_VEC3V(Vector3(localCenter));
		ScalarV SummedRadius = Add(VECTOR3_TO_SCALARV(Vector3(radius)), capsuleBound.GetRadiusV());
		return ( geomSpheres::TestSphereToSeg(sphereCenter, SummedRadius, capsuleBound.GetEndPointA(), capsuleBound.GetEndPointB()) );
	}
}

void phInstBehaviorExplosionGta::ApplyExplosionImpulsesToAnimatedPed(CPed *pPed, Vec3V_In vecApplyForceIn, Vec3V_In vecExplCenter)
{
	if( pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromExplosions) &&
		NetworkInterface::IsGameInProgress() )
	{
		// GTAV: B*3112820 - if the ped isn't going to ragdoll then don't apply a force either
		return;
	}

	// Wake up if it's not active
	if(CPhysics::GetLevel()->IsInactive(pPed->GetCurrentPhysicsInst()->GetLevelIndex()))
	{
		pPed->ActivatePhysics();
	}

	if(pPed->GetCollider())
	{
		// The effect looks better if previous velocities are canceled
		pPed->GetCollider()->Freeze();

		Vec3V vecApplyForce = vecApplyForceIn;
		ScalarV forceMag = Mag(vecApplyForce);
		vecApplyForce = NormalizeSafe(vecApplyForce, Vec3V(V_ZERO));

		// Ensure impulses give a decent arch
		static float allowedExplDistAbovePedPos = 1.5f;
		bool notTooFarAboveHitPed = (vecExplCenter.GetZf() - pPed->GetTransform().GetPosition().GetZf()) < allowedExplDistAbovePedPos;
		if(notTooFarAboveHitPed)
		{
			ScalarV dotval = vecApplyForce.GetZ();
			float angle = rage::AcosfSafe(dotval.Getf()) * (180.0f / PI);
			angle = 90.0f - angle;
			if (angle < 20.0f || angle > 60.0f)
			{
				float desiredAngle = angle < 20.0f ? 20.0f : 60.0f;
				desiredAngle *= (PI / 180.0f);
				vecApplyForce.SetZ(ScalarV(V_ZERO));
				vecApplyForce = NormalizeSafe(vecApplyForce, Vec3V(V_X_AXIS_WZERO));
				Vec3V upVec(0.0f, 0.0f, tan(desiredAngle));
				vecApplyForce = Add(vecApplyForce, upVec);
				vecApplyForce = Normalize(vecApplyForce);
			}
		}

		// Add some random pitch in the direction of the impulse
		mthRandom random;
		Vec3V vPitch;
		static float pitchTorqueMin = 5.0f;
		static float pitchTorqueMax = 8.0f;
		vPitch = Cross(Vec3V(V_Z_AXIS_WZERO), vecApplyForce);
		vPitch = NormalizeSafe(vPitch, Vec3V(V_X_AXIS_WZERO));
		static float extraBlastPitchMult = 40.0f;
		vPitch = Scale( Scale(vPitch, ScalarVFromF32(extraBlastPitchMult)), random.GetRangedV(ScalarVFromF32(pitchTorqueMin), ScalarVFromF32(pitchTorqueMax)) );
		pPed->GetCollider()->ApplyAngImpulse(vPitch.GetIntrin128ConstRef());

		// Clamp the explosion magnitude to peds
		static float fclamp = 100.0f;
		ScalarV magClamp = ScalarVFromF32(fclamp);
		forceMag = Clamp(forceMag, ScalarV(V_ZERO), magClamp);

		// Apply the linear impulse
		static float bodyScale = 10.0f;
		vecApplyForce = Scale(vecApplyForce, forceMag);
		ScalarV bodyScaleV = ScalarVFromF32(bodyScale);
		vecApplyForce = Scale(vecApplyForce, bodyScaleV);
		pPed->GetCollider()->ApplyImpulseCenterOfMassRigid(vecApplyForce.GetIntrin128ConstRef());
	}
}


//------------------------------------------------------------------
// Find out (roughly) where an explosion touches a physics instance.
//------------------------------------------------------------------
bool phInstBehaviorExplosionGta::FindIntersections( phInst *OtherInst, phIntersection* aIntersections, s32& iNumIntersections, const s32 iMaxIntersections )
{
	DETAILED_EXPL_TIMERS_ONLY( PF_FUNC(FindIntersections); )

	// Find the position on the other bound whereat to apply the force.
	Vector3 ExplosionCenter_OtherInstSpace;
	Assert(OtherInst != NULL);
	iNumIntersections = 0;

	phBound &OtherBound = *OtherInst->GetArchetype()->GetBound();
	Vector3 OtherInstCenter;

	OtherInstCenter = VEC3V_TO_VECTOR3(OtherBound.GetWorldCentroid(OtherInst->GetMatrix()));		// Use m_Instance matrix because we want the compare both centres in the local space of explosion
	Vector3 OtherBoundRadius = SCALARV_TO_VECTOR3(OtherBound.GetRadiusAroundCentroidV());

	phBound& ThisBound = *m_Instance->GetArchetype()->GetBound();
	Vector3 ThisInstCenter;

	ThisInstCenter = VEC3V_TO_VECTOR3(ThisBound.GetWorldCentroid(m_Instance->GetMatrix()));
	
	Vector3 vNormal = ThisInstCenter - OtherInstCenter;

	// Early out if we're not near the other inst
	if (false == TestAgainstSphere(vNormal, OtherBoundRadius))
	{
		return false;
	}

	vNormal.NormalizeSafe();
	aIntersections[0].SetNormal(RCC_VEC3V(vNormal));
	aIntersections[0].SetComponent(0);
	aIntersections[0].SetPosition(RCC_VEC3V(OtherInstCenter));
	iNumIntersections = 1;

	fragInst* pFragInst = NULL;
	if(IsFragInst(OtherInst)) 
		pFragInst = static_cast<fragInst*>(OtherInst);

	// Specially apply forces to peds spine bone
	if( pFragInst && pFragInst->GetClassType()==PH_INST_FRAG_PED )
	{
		Assert(pFragInst->GetArchetype()->GetBound()->GetType() == phBound::COMPOSITE);	// This is pretty much guranteed
		phBoundComposite* pBoundComposite = static_cast<phBoundComposite*>(pFragInst->GetArchetype()->GetBound());
		Matrix34 matResult = RCC_MATRIX34(pBoundComposite->GetCurrentMatrix(PED_SPINE_BONE));

		matResult.Dot(RCC_MATRIX34(pFragInst->GetMatrix()));

		Vector3 vNormal = ThisInstCenter - matResult.d;
		vNormal.NormalizeSafe();
		aIntersections[0].SetNormal(RCC_VEC3V(vNormal));
		aIntersections[0].SetComponent((u16)PED_SPINE_BONE);
		aIntersections[0].SetPosition(RCC_VEC3V(matResult.d));
		iNumIntersections = 1;
		return true;
	}

	if( pFragInst )
	{
		phBoundComposite* pBoundComposite = NULL;
		if(pFragInst->GetArchetype()->GetBound()->GetType() == phBoundComposite::COMPOSITE)
			pBoundComposite = static_cast<phBoundComposite*>(pFragInst->GetArchetype()->GetBound());

		if(pBoundComposite==NULL)
			return false;

		iNumIntersections = 0;
		int closestPart = 0;
		Vector3 closestDot = VEC3_MAX;
		for(int i=0; i<pFragInst->GetTypePhysics()->GetNumChildren() && iNumIntersections < iMaxIntersections; i++)
		{
			phBound* PartBound = pBoundComposite->GetBound(i);
			if( PartBound )
			{
				Matrix34 matResult = RCC_MATRIX34(pBoundComposite->GetCurrentMatrix(i));

				matResult.Dot(RCC_MATRIX34(pFragInst->GetMatrix()));

				Vector3 PartCenter = VEC3V_TO_VECTOR3(PartBound->GetWorldCentroid(RCC_MAT34V(matResult)));

				Vector3 PartRadius = SCALARV_TO_VECTOR3(PartBound->GetRadiusAroundCentroidV());

				if (TestAgainstSphere(PartCenter - ThisInstCenter, PartRadius))
				{
					Vector3 vNormal = ThisInstCenter - PartCenter;
					vNormal.NormalizeSafe();
					aIntersections[iNumIntersections].SetNormal(RCC_VEC3V(vNormal));
					aIntersections[iNumIntersections].SetComponent((u16)i);
					aIntersections[iNumIntersections].SetPosition(RCC_VEC3V(PartCenter));

					Vector3 partDot, partPos;
					partPos.SubtractScaled(PartCenter, vNormal, OtherBoundRadius);
					partDot = partPos.DotV(vNormal);
					if (partDot.IsLessThanDoNotUse(closestDot))
					{
						closestDot = partDot;
						closestPart = iNumIntersections;
					}

					++iNumIntersections;
				}
			}
		}

		if (iNumIntersections > 0)
		{
			// If the object is not attached to the world just use the closest part
			fragTypeGroup* group = pFragInst->GetTypePhysics()->GetAllGroups()[pFragInst->GetTypePhysics()->GetAllChildren()[0]->GetOwnerGroupPointerIndex()];
			if (pFragInst->GetTypePhysics()->GetMinMoveForce() >= 0.0f ||
				(0 < pFragInst->GetTypePhysics()->GetNumChildren() && group->GetStrength() >= 0.0f))
			{
				aIntersections[0] = aIntersections[closestPart];
				iNumIntersections = 1;
			}
		}
	}

	return iNumIntersections > 0;
}

bool phInstBehaviorExplosionGta::FindIntersectionsOriginal(phInst *OtherInst, Vec3V_InOut otherInstLosPosition, WorldProbe::CShapeTestResults& refResults, const s32 iMaxIntersections, bool ignoreBoundOffset)
{
	DETAILED_EXPL_TIMERS_ONLY( PF_FUNC(FindIntersections); )

	// Find the position on the other bound whereat to apply the force.
	Vector3 ExplosionCenter_OtherInstSpace;
	Assert(OtherInst != NULL);
	s32 iNumIntersections = 0;

	phBound &OtherBound = *OtherInst->GetArchetype()->GetBound();
	Vector3 OtherInstCenter;

	OtherInstCenter = VEC3V_TO_VECTOR3(OtherBound.GetWorldCentroid(OtherInst->GetMatrix()));		// Use m_Instance matrix because we want the compare both centres in the local space of explosion
	Vector3 OtherBoundRadius = SCALARV_TO_VECTOR3(OtherBound.GetRadiusAroundCentroidV());

	phBound& ThisBound = *m_Instance->GetArchetype()->GetBound();
	Vector3 ThisInstCenter;
	Vector3 vExplosionCentreToOtherInst = OtherInstCenter;

	ThisInstCenter = VEC3V_TO_VECTOR3(ThisBound.GetWorldCentroid(m_Instance->GetMatrix()));

	// TestAgainstSphere expects seperation in local space of explosion
	RCC_MATRIX34(m_Instance->GetMatrix()).UnTransform(vExplosionCentreToOtherInst);

	// Early out if we're not near the other inst
	if (false == TestAgainstSphere(vExplosionCentreToOtherInst, OtherBoundRadius))
	{
		return false;
	}

	fragInst* pFragInst = NULL;
	if(IsFragInst(OtherInst)) 
		pFragInst = static_cast<fragInst*>(OtherInst);
	if(m_Instance->GetArchetype()->GetBound()->GetType() == phBound::SPHERE)
	{
		WorldProbe::CShapeTestSphereDesc sphereDesc;
		sphereDesc.SetResultsStructure(&refResults);
		sphereDesc.SetFirstFreeResultOffset(0);
		sphereDesc.SetMaxNumResultsToUse(1);
		sphereDesc.SetSphere(ThisInstCenter, m_Radius);
		sphereDesc.SetDomainForTest(WorldProbe::TEST_AGAINST_INDIVIDUAL_OBJECTS);
		sphereDesc.SetIncludeInstance(OtherInst);
		sphereDesc.SetContext(WorldProbe::LOS_Weapon);
		sphereDesc.SetTreatPolyhedralBoundsAsPrimitives(true);
		if(!WorldProbe::GetShapeTestManager()->SubmitTest(sphereDesc))
		{
			return false;
		}
		iNumIntersections = 1;

		// Specially apply forces to peds spine bone, if it has one
		if( pFragInst && pFragInst->GetClassType()==PH_INST_FRAG_PED )
		{
            Matrix34 matResult;
            if(pFragInst->GetArchetype()->GetBound()->GetType() == phBound::COMPOSITE)//make sure we are actually a composite
            {
			    phBoundComposite* pBoundComposite = static_cast<phBoundComposite*>(pFragInst->GetArchetype()->GetBound());
                if(pBoundComposite->GetNumBounds() > PED_SPINE_BONE)
                {
                    matResult = RCC_MATRIX34(pBoundComposite->GetCurrentMatrix(PED_SPINE_BONE)); 
                    matResult.Dot(RCC_MATRIX34(pFragInst->GetMatrix()));
                    refResults[0].SetHitComponent((u16)PED_SPINE_BONE);
                }
                else
                {                    
                    matResult = RCC_MATRIX34(pBoundComposite->GetCurrentMatrix(0)); 
                    matResult.Dot(RCC_MATRIX34(pFragInst->GetMatrix()));
                    refResults[0].SetHitComponent(0);
                }
            }
            else
            {
                matResult = RCC_MATRIX34(pFragInst->GetMatrix());
                refResults[0].SetHitComponent(0);
            }
            
            Vector3 vNormal = ThisInstCenter - matResult.d;
            vNormal.NormalizeSafe();
            refResults[0].SetHitNormal(vNormal);
            refResults[0].SetHitPosition(matResult.d);
			otherInstLosPosition = refResults[0].GetPosition();
            iNumIntersections = 1;

			return true;
		}
	}
	else if(m_Instance->GetArchetype()->GetBound()->GetType() == phBound::CAPSULE)
	{
		ThisInstCenter = VEC3V_TO_VECTOR3(m_Instance->GetMatrix().GetCol3());//m_ExplosionInst->GetExplosionPosition();

		if(m_Instance->GetArchetype()->GetBound()->GetType() == phBound::CAPSULE)
		{
			// Shift the capsule center to the explosion source end
			ThisInstCenter -= (VEC3V_TO_VECTOR3(m_Instance->GetMatrix().GetCol1()) * GetRadius() * 0.5f);
		}

		ExplosionCenter_OtherInstSpace = VEC3V_TO_VECTOR3(UnTransformOrtho(OtherInst->GetMatrix(), RC_VEC3V(ThisInstCenter)));


		Vector3 vStart = ThisInstCenter;

		Vector3 vEnd = ThisInstCenter + (VEC3V_TO_VECTOR3(m_Instance->GetMatrix().GetCol1()) * GetRadius()*2 );

		/*
		// Need to do a directed capsule and sphere check to make sure any intersections are found, 
		// directed capsules ignore anything hit in the initial sphere
		phShapeTest<phShapeSphere> sphereTester;
		sphereTester.GetShape().InitSphere(RCC_VEC3V(vStart),ScalarVFromF32(m_Width),&aIntersections[iNumIntersections],iMaxIntersections);

		iNumIntersections = sphereTester.TestOneObject(OtherBound, *OtherInst);
		*/
		WorldProbe::CShapeTestSphereDesc sphereDesc;
		sphereDesc.SetResultsStructure(&refResults);
		sphereDesc.SetFirstFreeResultOffset(iNumIntersections);
		sphereDesc.SetMaxNumResultsToUse(iMaxIntersections-iNumIntersections);
		sphereDesc.SetSphere(vStart, m_Width);
		sphereDesc.SetDomainForTest(WorldProbe::TEST_AGAINST_INDIVIDUAL_OBJECTS);
		sphereDesc.SetIncludeInstance(OtherInst);
		sphereDesc.SetContext(WorldProbe::LOS_Weapon);
		sphereDesc.SetTreatPolyhedralBoundsAsPrimitives(true);
		WorldProbe::GetShapeTestManager()->SubmitTest(sphereDesc);
		iNumIntersections = refResults.GetNumHits();

		if( iNumIntersections < iMaxIntersections )
		{
			/*
			phSegmentV segment(RCC_VEC3V(vStart), RCC_VEC3V(vEnd));
			phShapeTest<phShapeCapsule> capsuleTester;
			capsuleTester.GetShape().InitCapsule(segment,ScalarVFromF32(m_Width),&aIntersections[0],iMaxIntersections-iNumIntersections);

			// Make sure the sphere at the end of the capsule is safe, then do a directed capsule collision
			iNumIntersections += capsuleTester.TestOneObject(OtherBound, *OtherInst);
			*/

			WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
			capsuleDesc.SetResultsStructure(&refResults);
			capsuleDesc.SetFirstFreeResultOffset(iNumIntersections);
			capsuleDesc.SetMaxNumResultsToUse(iMaxIntersections-iNumIntersections);
			capsuleDesc.SetCapsule(vStart, vEnd, m_Width);
			capsuleDesc.SetDomainForTest(WorldProbe::TEST_AGAINST_INDIVIDUAL_OBJECTS);
			capsuleDesc.SetIncludeInstance(OtherInst);
			capsuleDesc.SetIsDirected(false);
			capsuleDesc.SetContext(WorldProbe::LOS_Weapon);
			capsuleDesc.SetTreatPolyhedralBoundsAsPrimitives(true);
			WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc);

			iNumIntersections = refResults.GetNumHits();
		}

		if(iNumIntersections <= 0)
			return false;

		// Specially apply forces to peds spine bone
		// 		if( pFragInst && pFragInst->GetClassType()==PH_INST_FRAG_PED )
		// 		{
		// 			phBoundComposite* pBoundComposite = dynamic_cast<phBoundComposite*>(pFragInst->GetArchetype()->GetBound());
		// 			Matrix34 matResult = pBoundComposite->GetCurrentMatrix(9);
		// 			matResult.Dot(pFragInst->GetMatrix());
		// 			Vector3 vNormal = ThisInstCenter - matResult.d;
		// 			vNormal.NormalizeSafe();
		// 			aIntersections[0].SetNormal(vNormal);
		// 			aIntersections[0].SetComponent(9);
		// 			aIntersections[0].SetPosition(matResult.d);
		// 			iNumIntersections = 1;
		// 			return true;
		// 		}

		// We always want an intersection with the ped's spine so that we can push it away from explosion
		if( pFragInst && pFragInst->GetClassType()==PH_INST_FRAG_PED 
			&& iNumIntersections < iMaxIntersections)	// There is room to add a spine intersection
		{
			bool bHitPedSpine = false;
			for(WorldProbe::ResultIterator it = refResults.begin(); it < refResults.last_result(); ++it)
			{
				if(it->GetHitComponent() == PED_SPINE_BONE)
				{
					bHitPedSpine = true;
				}
			}

			if(!bHitPedSpine)
			{
				// Add a spine contact
				Assert(iNumIntersections <= iMaxIntersections);	// This is prevented above... Just putting adding this assert in case someone accidentally copy+pastes

                Matrix34 matResult;
                if(pFragInst->GetArchetype()->GetBound()->GetType() == phBound::COMPOSITE)//make sure we are actually a composite
                {
                    phBoundComposite* pBoundComposite = static_cast<phBoundComposite*>(pFragInst->GetArchetype()->GetBound());
                    if(pBoundComposite->GetNumBounds() > PED_SPINE_BONE)
                    {
                        matResult = RCC_MATRIX34(pBoundComposite->GetCurrentMatrix(PED_SPINE_BONE)); 
                        matResult.Dot(RCC_MATRIX34(pFragInst->GetMatrix()));
                        refResults[iNumIntersections].SetHitComponent((u16)PED_SPINE_BONE);
                    }
                    else
                    {                    
                        matResult = RCC_MATRIX34(pBoundComposite->GetCurrentMatrix(0)); 
                        matResult.Dot(RCC_MATRIX34(pFragInst->GetMatrix()));
                        refResults[iNumIntersections].SetHitComponent(0);
                    }
                }
                else
                {
                    matResult = RCC_MATRIX34(pFragInst->GetMatrix());
                    refResults[iNumIntersections].SetHitComponent(0);
                }

                Vector3 vNormal = ThisInstCenter - matResult.d;
                vNormal.NormalizeSafe();

				refResults[iNumIntersections].SetHitInst(OtherInst->GetLevelIndex(),
#if LEVELNEW_GENERATION_IDS
					PHLEVEL->GetGenerationID(OtherInst->GetLevelIndex())
#else
					0
#endif // LEVELNEW_GENERATION_IDS
					);

				refResults[iNumIntersections].SetHitNormal(vNormal);
				refResults[iNumIntersections].SetHitPosition(matResult.d);
				// We have added a 'fake' hit, we must update the results container so that it recognises this.
				refResults.Update();
				iNumIntersections++;
			}
		}

		otherInstLosPosition = refResults[0].GetPosition();
		return true;
	}

	if( pFragInst )
	{
		Assert(pFragInst->GetType());

		phBoundComposite* pBoundComposite = static_cast<phBoundComposite*>(pFragInst->GetArchetype()->GetBound());
		if(pBoundComposite==NULL)
		{
			return false;
		}
		Assert(dynamic_cast<phBoundComposite*>(pFragInst->GetArchetype()->GetBound()));

		Mat34V worldFromInstance = pFragInst->GetMatrix();

		if( !ignoreBoundOffset )
		{
			otherInstLosPosition = Transform(worldFromInstance,pBoundComposite->GetCentroidOffset());
		}
		else
		{
			otherInstLosPosition = worldFromInstance.d();
		}

		const fragPhysicsLOD* physicsLOD = pFragInst->GetTypePhysics();

		// if the base of this fragment can uproot, or we're not attached to the base any more skip out
		if(physicsLOD->GetMinMoveForce() >= 0.0f || pFragInst->GetGroupBroken(0))
		{
			return true;
		}

		// Apply 1 impulse per group. This will ensure each group gets damaged without needing tons of intersections for more complex props. 
		iNumIntersections = 0;
		for(int groupIndex=0; groupIndex < physicsLOD->GetNumChildGroups() && iNumIntersections < MAX_INTERSECTIONS; groupIndex++)
		{
			if( !pFragInst->GetGroupBroken(groupIndex) )
			{
				int childIndex = physicsLOD->GetGroup(groupIndex)->GetChildFragmentIndex();
				Vec3V vBoundCenter = (pBoundComposite->GetLocalBoxMins(childIndex) + pBoundComposite->GetLocalBoxMaxs(childIndex)) * ScalarV(V_HALF);

				vBoundCenter = Transform(pBoundComposite->GetCurrentMatrix(childIndex), vBoundCenter);
				vBoundCenter = Transform(worldFromInstance, vBoundCenter);

				Vector3 vNormal = ThisInstCenter - RCC_VECTOR3(vBoundCenter);
				vNormal.NormalizeSafe();
				refResults[iNumIntersections].SetHitInst(pFragInst->GetLevelIndex(),PHLEVEL->GetGenerationID(pFragInst->GetLevelIndex()));
				refResults[iNumIntersections].SetHitNormal(vNormal);
				refResults[iNumIntersections].SetHitComponent((u16)childIndex);
				refResults[iNumIntersections].SetHitPosition(RCC_VECTOR3(vBoundCenter));
				// We have added a 'fake' hit, we must update the results container so that it recognizes this.
				++iNumIntersections;
			}
		}
		refResults.Update();
	}
	else
	{
		otherInstLosPosition = refResults[0].GetPosition();
	}

	return iNumIntersections > 0;
}

void phInstBehaviorExplosionGta::AddPedsAttachedToCar( CVehicle* pVehicle, bool bShouldCheckIfPedIgnoresExplosions )
{
	for(CPhysical* attachedPhysical = (CPhysical*)pVehicle->GetChildAttachment(); attachedPhysical != NULL && ms_NumCollidedInsts < MAX_NUM_COLLIDED_INSTS; attachedPhysical = (CPhysical*)attachedPhysical->GetSiblingAttachment())
	{
		if(attachedPhysical->GetIsTypePed())
		{
			CPed* attachedPed = (CPed*)attachedPhysical;
			if(attachedPed->GetFragInst() && !attachedPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle) && (!bShouldCheckIfPedIgnoresExplosions || (bShouldCheckIfPedIgnoresExplosions && !attachedPed->GetIgnoresExplosions())))
			{
				AddCollidedEntity(attachedPed,attachedPed->GetFragInst());
			}
		}
	}
}

//-------------------------------------------------------------------------
// Initialises the list of predicted smashables
//-------------------------------------------------------------------------
CCachedSmashables::CCachedSmashables()
{
	for( int i = 0; i < MAX_EXPLOSION_SMASHABLES; i++ )
	{
		m_apPossibleEntities[i] = NULL;
	}
}

//-------------------------------------------------------------------------
// Shuts down the list of predicted smashables
//-------------------------------------------------------------------------
CCachedSmashables::~CCachedSmashables()
{
	Reset();
}


//-------------------------------------------------------------------------
// Tries to replace the furthest entity with the one passed if closer
//-------------------------------------------------------------------------
void CCachedSmashables::TryToReplaceFurthestEntity(CEntity* pEntity, const Vector3& vCentre)
{
	const float fDistanceToNewEntity = pEntity->GetBoundCentre().Dist(vCentre) - pEntity->GetBoundRadius();
	for( int i = 0; i < MAX_EXPLOSION_SMASHABLES; i++ )
	{
		if( m_apPossibleEntities[i] )
		{
			CEntity* pCurrentEntity = m_apPossibleEntities[i];
			const float fDistanceToCurrentEntity = pCurrentEntity->GetBoundCentre().Dist(vCentre) - pCurrentEntity->GetBoundRadius();
			if( fDistanceToCurrentEntity > fDistanceToNewEntity )
			{
				m_apPossibleEntities[i] = pEntity;
				return;
			}
		}
	}
}

#define MAX_GLASS_SMASHING_RANGE (15.0f)

//-------------------------------------------------------------------------
// Predicts the maximum range of the explosion and builds a list of
// smashable entities that will eventually be affected
//-------------------------------------------------------------------------
void CCachedSmashables::CacheSmashablesAffectedByExplosion( const phGtaExplosionInst* UNUSED_PARAM(pExplosionInst) , const Vector3& vCentre )
{
	const float fPredictedMaximumExplosionRadius = MAX_GLASS_SMASHING_RANGE;

	s32 iNextFreeEntity = 0;

	// reset any previously stored smashables
	Reset();

#if ENABLE_PHYSICS_LOCK
	phIterator it(phIterator::PHITERATORLOCKTYPE_READLOCK);
#else	// ENABLE_PHYSICS_LOCK
	phIterator it;
#endif	// ENABLE_PHYSICS_LOCK

	it.InitCull_Sphere(vCentre, fPredictedMaximumExplosionRadius);

	it.SetStateIncludeFlags(phLevelBase::STATE_FLAG_FIXED);

	u16 curObjectLevelIndex = CPhysics::GetLevel()->GetFirstCulledObject(it);

	while (curObjectLevelIndex != phInst::INVALID_INDEX)
	{
		phInst& culledInst = *CPhysics::GetLevel()->GetInstance(curObjectLevelIndex);

		// get the entity from the inst

		CEntity* pEntity = CPhysics::GetEntityFromInst(&culledInst);

		Assert(pEntity);

		if (IsEntitySmashable(pEntity))
		{
			if(iNextFreeEntity >= MAX_EXPLOSION_SMASHABLES)
			{
				TryToReplaceFurthestEntity(pEntity, vCentre);
			}
			else
			{
				m_apPossibleEntities[iNextFreeEntity] = pEntity;
				++iNextFreeEntity;
			}
		}
		curObjectLevelIndex = CPhysics::GetLevel()->GetNextCulledObject(it);
	}
}

//#define EXPLOSION_FORCE_REDUCTION_FOR_SMASHABLES (0.277f)


//-------------------------------------------------------------------------
// Updates the list of smashables with the current explosion size.
// When a smashable is hit, it is smashed and removed from the list
//-------------------------------------------------------------------------
void CCachedSmashables::UpdateCachedSmashables( const Vector3& vCentre, const float speedRatio, const float fNewRadius, const float fForceFactor )
{
	for( int i = 0; i < MAX_EXPLOSION_SMASHABLES; i++ )
	{
		if( m_apPossibleEntities[i] )
		{
			Vector3 vBoundCentre;
			m_apPossibleEntities[i]->GetBoundCentre(vBoundCentre);
			if( ( vBoundCentre.Dist(vCentre) - m_apPossibleEntities[i]->GetBoundRadius() ) < fNewRadius )
			{
				Assert( m_apPossibleEntities[i]->GetCurrentPhysicsInst() );
				// PH i've clamped this to 1 to stop the assert, not sure why its increased, the MAX force mag takes mass into account so it could be that that is pushing it out.
				const float kForceMag = MIN(1.0f,fForceFactor*speedRatio*0.03334f);	// *0.03334 is instead of dividing by 30 (which is currently the max force)
				Assertf(kForceMag>=0.0f && kForceMag<=1.0f, "kForceMag: %.3f, fForceMag: %.3f, speedRatio: %.3f", kForceMag, fForceFactor, speedRatio);	

				VfxExpInfo_s vfxExpInfo;
				vfxExpInfo.regdEnt = m_apPossibleEntities[i];
				vfxExpInfo.vPositionWld = RCC_VEC3V(vCentre);
				vfxExpInfo.radius = fNewRadius;
				vfxExpInfo.force = kForceMag;
				vfxExpInfo.flags = VfxExpInfo_s::EXP_FORCE_DETACH | VfxExpInfo_s::EXP_REMOVE_DETATCHED;
				if (vfxExpInfo.force>0.0f)
				{
					g_vehicleGlassMan.StoreExplosion(vfxExpInfo);
				}

				static bool bTestCumulativeResults = true;
				if( !bTestCumulativeResults )
				{
					m_apPossibleEntities[i] = NULL;
				}
			}
		}
	}
}

//-------------------------------------------------------------------------
// Resets
//-------------------------------------------------------------------------
void CCachedSmashables::Reset()
{
	for( int i = 0; i < MAX_EXPLOSION_SMASHABLES; i++ )
	{
		if( m_apPossibleEntities[i] )
		{
			m_apPossibleEntities[i] = NULL;
		}
	}
}


// Initialise static members
float						CExplosionManager::m_version					= 0.0f;
phGtaExplosionInst*			CExplosionManager::m_apExplosionInsts			[MAX_EXPLOSIONS];
phInstGta*					CExplosionManager::m_pExplosionPhysInsts		[MAX_EXPLOSIONS];
phInstBehaviorExplosionGta*	CExplosionManager::m_ExplosionBehaviors			= NULL;
s32						CExplosionManager::m_aActivationDelay			[MAX_EXPLOSIONS];
CCachedSmashables			CExplosionManager::m_aCachedSmashables			[MAX_EXPLOSIONS];

////////////////////////////////////////////////////////////////////////////////

// Special Ammo tunables

// We need different values for local and remote peds because damage always applies twice on remote peds (don't ask)

// EXP_TAG_EXPLOSIVEAMMO_SHOTGUN - Default Damage = 200
float CExplosionManager::sm_fSpecialAmmoExplosiveShotgunPedLocalDamageMultiplier	= 2.0f;		// 400
float CExplosionManager::sm_fSpecialAmmoExplosiveShotgunPedRemoteDamageMultiplier	= 1.0f;		// 400 (200 x 2)
float CExplosionManager::sm_fSpecialAmmoExplosiveShotgunPlayerDamageMultiplier		= 0.575f;	// 230 (115 x 2)

// EXP_TAG_EXPLOSIVEAMMO - Default Damage = 200
float CExplosionManager::sm_fSpecialAmmoExplosiveSniperPedLocalDamageMultiplier		= 1.0f;		// 200 
float CExplosionManager::sm_fSpecialAmmoExplosiveSniperPedRemoteDamageMultiplier	= 0.5f;		// 200 (100 x 2) 
float CExplosionManager::sm_fSpecialAmmoExplosiveSniperPlayerDamageMultiplier		= 0.5f;		// 200 (100 x 2)

u32 CExplosionManager::sm_uFlashGrenadeStrongEffectVFXLength	= 5000;
u32 CExplosionManager::sm_uFlashGrenadeWeakEffectVFXLength		= 2500;

float CExplosionManager::sm_fFlashGrenadeVehicleOccupantDamageMultiplier	= 0.0;
float CExplosionManager::sm_fStunGrenadeVehicleOccupantDamageMultiplier		= 0.5;

void CExplosionManager::InitTunables()
{				
	sm_fSpecialAmmoExplosiveShotgunPedLocalDamageMultiplier		= ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("XMAS17_AMMO_EXP_SHGN_PEDL_DAMAGEMULT", 0x096E6B9A), sm_fSpecialAmmoExplosiveShotgunPedLocalDamageMultiplier);					
	sm_fSpecialAmmoExplosiveShotgunPedRemoteDamageMultiplier	= ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("XMAS17_AMMO_EXP_SHGN_PEDR_DAMAGEMULT", 0x55101F99), sm_fSpecialAmmoExplosiveShotgunPedRemoteDamageMultiplier);
	sm_fSpecialAmmoExplosiveShotgunPlayerDamageMultiplier		= ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("XMAS17_AMMO_EXP_SHGN_PLYR_DAMAGEMULT", 0x137592EC), sm_fSpecialAmmoExplosiveShotgunPlayerDamageMultiplier);
				
	sm_fSpecialAmmoExplosiveSniperPedLocalDamageMultiplier		= ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("XMAS17_AMMO_EXP_SNPR_PEDL_DAMAGEMULT", 0x13570A77), sm_fSpecialAmmoExplosiveSniperPedLocalDamageMultiplier);					
	sm_fSpecialAmmoExplosiveSniperPedRemoteDamageMultiplier		= ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("XMAS17_AMMO_EXP_SNPR_PEDR_DAMAGEMULT", 0x53BB204F), sm_fSpecialAmmoExplosiveSniperPedRemoteDamageMultiplier);					
	sm_fSpecialAmmoExplosiveSniperPlayerDamageMultiplier		= ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("XMAS17_AMMO_EXP_SNPR_PLYR_DAMAGEMULT", 0xE24494EE), sm_fSpecialAmmoExplosiveSniperPlayerDamageMultiplier);					

	sm_uFlashGrenadeStrongEffectVFXLength						= ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("CNC_WPN_POST_EFFECT_DURATION_FLASHBANG_STRONG", 0x3BD36570), sm_uFlashGrenadeStrongEffectVFXLength);					
	sm_uFlashGrenadeWeakEffectVFXLength							= ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("CNC_WPN_POST_EFFECT_DURATION_FLASHBANG_WEAK", 0xEC599883), sm_uFlashGrenadeWeakEffectVFXLength);	

	sm_fFlashGrenadeVehicleOccupantDamageMultiplier				= ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("CNC_WPN_FLASHBANG_VEHICLE_OCCUPANT_DAMAGEMULT", 0x16632446), sm_fFlashGrenadeVehicleOccupantDamageMultiplier);
	sm_fStunGrenadeVehicleOccupantDamageMultiplier				= ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("CNC_WPN_STUNGRENADE_VEHICLE_OCCUPANT_DAMAGEMULT", 0xFF550F0A), sm_fStunGrenadeVehicleOccupantDamageMultiplier);
}

////////////////////////////////////////////////////////////////////////////////

bool CExplosionManager::sm_InsideExplosionUpdate = false;

BANK_ONLY(bool CExplosionManager::sm_EnableLimitedActivation = true;)
atFixedArray<phHandle, CExplosionManager::ABSOLUTE_MAX_ACTIVATED_OBJECTS> CExplosionManager::sm_ActivatedObjects;
bank_s32 CExplosionManager::sm_MaximumActivatedObjects = CExplosionManager::ABSOLUTE_MAX_ACTIVATED_OBJECTS;

int		CExplosionManager::sm_NumExplosionBreaksThisFrame = 0;
bank_s32	CExplosionManager::sm_MaxExplosionBreaksPerFrame = 3;


bank_float bfBlastExtendedRadiusRatio = 0.5f;

// Explosion tracking.		
atQueue<u32, 25> CExplosionManager::ms_recentExplosions;
Vector3	CExplosionManager::ms_vExplosionTrackCentre = Vector3(0, 0 , 0);

#if __BANK

static const char *			pExplosionTagsText								[EEXPLOSIONTAG_MAX];

bool						CExplosionManager::ms_debugRenderExplosions		= false;
bool						CExplosionManager::ms_debugWeaponExplosions		= false;
eExplosionTag				CExplosionManager::ms_eDebugExplosionTag		= EXP_TAG_GRENADE;
float						CExplosionManager::ms_sizeScale					= 1.0f;
bool						CExplosionManager::ms_attachToObjectHit			= false;
bool						CExplosionManager::ms_ClampAreaApproximation	= true;
//float						CExplosionManager::ms_fTimer					= 0.0f;
rage::bkGroup*				CExplosionManager::m_pBankGroup					= NULL;
bool						CExplosionManager::ms_needToSetupRagWidgets		= false;
// Add widgets (for everything we can reasonably tune) for this CExplosionArgs 
void CExplosionManager::CExplosionArgs::AddWidgets(bkBank & bank)
{
	bank.AddCombo("Explosion Tag",(int*)&m_explosionTag, eExplosionTag_GetTagNameCount(), eExplosionTag_GetStaticTagNames(), 0);
	bank.AddVector("Position", &m_explosionPosition, -10000.0f,10000.0f,0.1f);
	bank.AddSlider("Activation Delay (in ms)", &m_activationDelay, 0, 10000, 100); 
	bank.AddSlider("Physical Effect Scale", &m_sizeScale, 0.01f, 1.0f, 0.05f); 
	bank.AddSlider("Camera Shake", &m_fCamShake, -1.0f, 100.0f, 0.1f); 
	bank.AddToggle("Make Sound", &m_bMakeSound); 
	bank.AddToggle("No Fx", &m_bNoFx); 
	bank.AddToggle("In Air", &m_bInAir);
	bank.AddVector("Direction", &m_vDirection, -1.0f,1.0f,0.01f);
}

void CExplosionManager::AddWidgets()
{
	if(ms_needToSetupRagWidgets == false)//make sure this is only setup on the second time through that way the physics bank will have been created properly. This is due to the new "create physics widgets" button.
	{
		ms_needToSetupRagWidgets = true;
		return;
	}

	rage::bkBank* pPhysicsBank = BANKMGR.FindBank("Physics");
	if(pPhysicsBank)
	{
		if(m_pBankGroup)
		{
			pPhysicsBank->DeleteGroup(*m_pBankGroup);
		}

		m_pBankGroup = pPhysicsBank->PushGroup("Explosions", false);

		for (s32 i=0; i<CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetSize(); i++)
		{
			if (CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(static_cast<eExplosionTag>(i)).name.GetLength() != 0)
			{
				pExplosionTagsText[i] = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(static_cast<eExplosionTag>(i)).name;
			}
		}

		pPhysicsBank->AddToggle("Render", &ms_debugRenderExplosions, NullCB, "Toggles whether debug explosions are added at bullet impacts");
		pPhysicsBank->AddToggle("Explosions on Bullet Impact", &ms_debugWeaponExplosions, NullCB, "Toggles whether debug explosions are added at bullet impacts");
		pPhysicsBank->AddCombo("Explosion Tag", (int*)&ms_eDebugExplosionTag, CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetSize(), pExplosionTagsText, NullCB );
		pPhysicsBank->AddSlider("Size Scale", &ms_sizeScale, 0.0f, 1.0f, 0.05f);
		pPhysicsBank->AddToggle("Attach Explosion", &ms_attachToObjectHit, NullCB, "The explosion will attached to anything hit");
		pPhysicsBank->AddToggle("Clamp Area Approximation",&ms_ClampAreaApproximation);
		pPhysicsBank->AddToggle("Enable Limited Activation", &sm_EnableLimitedActivation, NullCB, "Limits the number of objects explosions can activate");
		pPhysicsBank->AddSlider("Max Active Objects", &sm_MaximumActivatedObjects, 0, ABSOLUTE_MAX_ACTIVATED_OBJECTS, 1, NullCB, "Maximum number of objects explosions can cause to be active at the same time");
		pPhysicsBank->AddSlider("Max Explosion Breaks Per Frame", &sm_MaxExplosionBreaksPerFrame, 0, 100, 1);
		pPhysicsBank->AddToggle("Enable Explosion Cache", &CExplosionOcclusionCache::sm_EnableOcclusionCache);
		pPhysicsBank->AddToggle("Enable Explosion Cache Lifetime Override", &CExplosionOcclusionCache::sm_EnableLifeTimeOverride);
		pPhysicsBank->AddSlider("Explosion Cache Lifetime Override", &CExplosionOcclusionCache::sm_LifeTimeOverride,0,1000,1);
		pPhysicsBank->AddSlider("LOS offset along up",&phInstBehaviorExplosionGta::m_LosOffsetAlongUp,0.0f,10.0f,0.1f);
		pPhysicsBank->AddSlider("LOS offset towards instance",&phInstBehaviorExplosionGta::m_LosOffsetToInst,0.0f,10.0f,0.1f);
		//	bank.AddSlider("Explosion (persists for extra time)", &ms_fTimer, -1.0f, 1000.0f, 0.05f);
		pPhysicsBank->AddToggle("Wreck Vehicle On First Explosion", &CVehicle::ms_bWreckVehicleOnFirstExplosion, NullCB, "Vehicle can be shown as destroyed after the first explosion, or after the delayed explosion");
		pPhysicsBank->AddSlider("Max Vehicle Explosion Delay", &CVehicle::ms_fVehicleExplosionDelayMax, 0.0f, 5.0f, 0.1f);
		pPhysicsBank->AddSlider("Min Vehicle Explosion Delay", &CVehicle::ms_fVehicleExplosionDelayMin, 0.0f, 5.0f, 0.1f);
		pPhysicsBank->AddSlider("Explosion extended blast radius ratio", &bfBlastExtendedRadiusRatio, 0.0f, 1.0f, 0.01f);

		// Widgets for altering explosion info types
		CExplosionInfoManager::AddWidgets(*pPhysicsBank);

		pPhysicsBank->PopGroup();
	}
}

void CExplosionManager::DebugCreateExplosion( CEntity* pEntity, const Vector3& vPosition, const Vector3& vNormal, CEntity* pAttachEntity )
{
	CExplosionManager::CExplosionArgs explosionArgs(ms_eDebugExplosionTag, vPosition);

	explosionArgs.m_pEntExplosionOwner = pEntity;
	explosionArgs.m_sizeScale = ms_sizeScale;
	explosionArgs.m_vDirection = vNormal;
	explosionArgs.m_pAttachEntity = ms_attachToObjectHit ? pAttachEntity : NULL;

	CExplosionManager::AddExplosion(explosionArgs);
}

#endif


//-------------------------------------------------------------------------
// Initialises the explosion manager with enough storage for MaxExplosions
// explosions
//-------------------------------------------------------------------------
void CExplosionManager::Init(void)
{
}


//-------------------------------------------------------------------------
// Removes all active explosions
//-------------------------------------------------------------------------
void CExplosionManager::Init(unsigned initMode)
{
    if(initMode == INIT_SESSION)
    {
	    m_ExplosionBehaviors = rage_new phInstBehaviorExplosionGta[MAX_EXPLOSIONS];

	    for(int ExplosionIndex = 0; ExplosionIndex < MAX_EXPLOSIONS; ++ExplosionIndex)
	    {
		    m_pExplosionPhysInsts[ExplosionIndex] = rage_new phInstGta(PH_INST_EXPLOSION);
		    phInst &CurInstance = *m_pExplosionPhysInsts[ExplosionIndex];
		    phInstBehaviorExplosionGta &CurBehavior = m_ExplosionBehaviors[ExplosionIndex];
		    phBound* pBound = NULL;
		    // Sphere explosions unsuprisingly use spheres
		    if( ExplosionIndex < MAX_SPHERE_EXPLOSIONS )
		    {
			    phBoundSphere * ExplosionBound = rage_new phBoundSphere;
			    ExplosionBound->SetSphereRadius(0.1f);  // rage_new rage doesn't like zero radius (asserts)
			    pBound = ExplosionBound;
		    }
		    // Directed explosions use capsules
		    else if( ExplosionIndex < (MAX_SPHERE_EXPLOSIONS+MAX_DIRECTED_EXPLOSIONS) )
		    {
			    phBoundCapsule *DirectedBound = rage_new phBoundCapsule();
			    DirectedBound->SetCapsuleSize(1.0f, 0.5f);
			    pBound = DirectedBound;
		    }
		    phArchetype * ExplosionArchetype = rage_new phArchetype;
		    ExplosionArchetype->SetBound(pBound);
		    ExplosionArchetype->SetMass(0.0f);
		    ExplosionArchetype->SetAngInertia(VEC3_ZERO);
		    // set flags in archetype to say what type of physics object this is
		    ExplosionArchetype->SetTypeFlags(ArchetypeFlags::GTA_EXPLOSION_TYPE);

		    // set flags in archetype to say what type of physics object we wish to collide with
		    ExplosionArchetype->SetIncludeFlags(ArchetypeFlags::GTA_EXPLOSION_INCLUDE_TYPES);

		    CurInstance.SetArchetype(ExplosionArchetype);

		    CurInstance.SetMatrix(Mat34V(V_IDENTITY));
			Assert(!CurInstance.IsInLevel());

		    //ExplosionArchetype->Release();
		    pBound->Release();

		    CurBehavior.SetInstance(CurInstance);
		    m_apExplosionInsts[ExplosionIndex] = NULL;
		    m_aActivationDelay[ExplosionIndex] = 0;

		    m_aCachedSmashables[ExplosionIndex].Reset();
	    }


#if __BANK
	    // Explosion widgets
	    AddWidgets();
#endif
    }
}







//-------------------------------------------------------------------------
// Removes all active explosions
//-------------------------------------------------------------------------
void CExplosionManager::Shutdown(unsigned shutdownMode)
{
    if(shutdownMode == SHUTDOWN_SESSION)
    {
	    ClearAllExplosions();

	    for(int ExplosionIndex = 0; ExplosionIndex < MAX_EXPLOSIONS; ++ExplosionIndex)
	    {
		    if( m_apExplosionInsts[ExplosionIndex] )
		    {
			    RemoveExplosion(ExplosionIndex);
		    }
		    if (m_pExplosionPhysInsts[ExplosionIndex])
		    {
			    Assert(m_pExplosionPhysInsts[ExplosionIndex]->GetArchetype());
			    Assert(m_pExplosionPhysInsts[ExplosionIndex]->GetArchetype()->GetRefCount() == 1);
			    Assert(m_pExplosionPhysInsts[ExplosionIndex]->GetArchetype()->GetBound());
			    Assert(m_pExplosionPhysInsts[ExplosionIndex]->GetArchetype()->GetBound()->GetRefCount() == 1);

			    delete m_pExplosionPhysInsts[ExplosionIndex];
		    }
		    m_pExplosionPhysInsts[ExplosionIndex] = NULL;
		    m_aActivationDelay[ExplosionIndex] = 0;

		    m_aCachedSmashables[ExplosionIndex].Reset();
	    }

	    if (m_ExplosionBehaviors)
	    {
		    delete [] m_ExplosionBehaviors;
	    }
	    m_ExplosionBehaviors = NULL;
    }
}

//-------------------------------------------------------------------------
// Shuts down the explosion manager, removing any active explosions and
// storage used for explosions
//-------------------------------------------------------------------------
void CExplosionManager::Shutdown()
{

}


//-------------------------------------------------------------------------
// Physics explosion manager update, monitors existing explosions and removes
// that finished
//-------------------------------------------------------------------------

dev_float dfBlastVehicleImpulse = 2500.0f;
dev_float dfBlastCarDoorPopChance = 0.75f;
dev_float dfBlastExtendedRadiusRatio = 0.5f;
dev_float dfMinExplosionRadiusRequredForExtendedBlast = 2.5f;
void CExplosionManager::Update(float fTimeStep)
{	
	// update
	for(int ExplosionIndex = 0; ExplosionIndex < MAX_EXPLOSIONS; ++ExplosionIndex)
	{
		phGtaExplosionInst* pCurrExplosionInst = m_apExplosionInsts[ExplosionIndex];
		if( pCurrExplosionInst )
		{		
			if( pCurrExplosionInst->CanActivate())
			{
				// we can't activate explosions that have a network owner, we need to wait for them to give us the go ahead first
				if( m_aActivationDelay[ExplosionIndex] > 0)
				{
					m_aActivationDelay[ExplosionIndex] -= static_cast<s32>(fTimeStep * 1000.f);//fwTimer::GetTimeStepInMilliseconds();
					if( m_aActivationDelay[ExplosionIndex] <= 0 )
					{
						m_aActivationDelay[ExplosionIndex] = 0;
						StartExplosion( ExplosionIndex );
					}
					else
					{
						continue;
					}
				}

				// update the explosion inst - calcs new world position and direction 
				pCurrExplosionInst->Update();

				phInstGta &CurInstance = *m_pExplosionPhysInsts[ExplosionIndex];
				phInstBehaviorExplosionGta &CurBehavior = m_ExplosionBehaviors[ExplosionIndex];

				CExplosionTagData& expTagData = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(pCurrExplosionInst->GetExplosionTag());
				float endRadius = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionEndRadius(expTagData);
				float speedRatio = 1.0f - (CurBehavior.GetRadius()/(endRadius*pCurrExplosionInst->GetSizeScale()));
				speedRatio = MAX(0.1f, speedRatio);

				if(pCurrExplosionInst->GetDamageType()!=DAMAGE_TYPE_FIRE)
				{
					m_aCachedSmashables[ExplosionIndex].UpdateCachedSmashables(RCC_VECTOR3(CurInstance.GetPosition()), speedRatio, CurBehavior.GetRadius(), CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionForceFactor(expTagData));
				}

				// process any timed explosion vfx
				if (CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(pCurrExplosionInst->GetExplosionTag()).directedLifeTime > 0.0f &&
					!CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(pCurrExplosionInst->GetExplosionTag()).bForceNonTimedVfx)
				{
					if (pCurrExplosionInst->GetNoVfx()==false)
					{
						g_vfxExplosion.ProcessVfxExplosion(pCurrExplosionInst, true, CurBehavior.GetRadius());
					}
				}

#if __BANK
				if( ms_debugRenderExplosions )
				{
					if (pCurrExplosionInst->GetPhysicalType()==PT_SPHERE)
					{
						grcDebugDraw::Sphere(pCurrExplosionInst->GetPosWld(), CurBehavior.GetRadius(), Color32(1.0f, 1.0f, 0.0f, 1.0f), false);
						
						// Render lethal kill radius
						static dev_float DAMAGE_TO_KILL_PED = 228.0f;	
						CExplosionTagData& expTagData = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(pCurrExplosionInst->GetExplosionTag());											
						if (expTagData.bUseDistanceDamageCalc && expTagData.damageAtCentre > DAMAGE_TO_KILL_PED)
						{					
							float fKillRadiusScale = 1 - ( (DAMAGE_TO_KILL_PED - expTagData.damageAtEdge) / (expTagData.damageAtCentre - expTagData.damageAtEdge) );
							grcDebugDraw::Sphere(pCurrExplosionInst->GetPosWld(), CurBehavior.GetRadius() * Min(fKillRadiusScale, 1.0f), Color32(1.0f, 0.0f, 0.0f, 0.3f), true);
						}

					}
					else if (pCurrExplosionInst->GetPhysicalType()==PT_DIRECTED)
					{
						grcDebugDraw::Capsule(pCurrExplosionInst->GetPosWld(), pCurrExplosionInst->GetPosWld()+(pCurrExplosionInst->GetDirWld()*ScalarVFromF32(CurBehavior.GetRadius())), CurBehavior.GetWidth(), Color32(1.0f, 1.0f, 0.0f, 1.0f), false);
					}
				}
#endif

				if (pCurrExplosionInst->GetExplosionTag() == EXP_TAG_BOMB_WATER || pCurrExplosionInst->GetExplosionTag() == EXP_TAG_BOMB_WATER_SECONDARY)
				{
					// extinguish any fires in range
					g_fireMan.ExtinguishArea(pCurrExplosionInst->GetPosWld(), CurBehavior.GetRadius());
				}

				if (pCurrExplosionInst->GetNeedsRemoved() || (CurInstance.IsInLevel() && !CurBehavior.IsActive()))
				{
					// directed explosions that are attached to entities may want to explode the entity when they are finished
					if (pCurrExplosionInst->GetAttachEntity())
					{	
						CExplosionTagData& expTagData = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(pCurrExplosionInst->GetExplosionTag());
						if (expTagData.directedLifeTime>0.0f && expTagData.explodeAttachEntityWhenFinished)
						{
							// try to blow up the entity
							if (!NetworkUtils::IsNetworkCloneOrMigrating(pCurrExplosionInst->GetAttachEntity()))
							{
								pCurrExplosionInst->GetAttachEntity()->TryToExplode(pCurrExplosionInst->GetExplosionOwner());
							}
						}
					}

					// Apply extended blast before remove the explosion
					if(CurBehavior.GetRadius() > dfMinExplosionRadiusRequredForExtendedBlast && pCurrExplosionInst->ShouldDamageVehicles())
					{
						float fExtendedRadius = CurBehavior.GetRadius() * (1.0f + bfBlastExtendedRadiusRatio);
						CVehicle::Pool * pVehiclePool = CVehicle::GetPool();
						s32 iPoolSize = (s32) pVehiclePool->GetSize();
						while(iPoolSize--)
						{
							CVehicle * pVehicle = pVehiclePool->GetSlot(iPoolSize);
							if(!pVehicle || pVehicle->GetVehicleType() != VEHICLE_TYPE_CAR)
								continue;

							Vector3 vOffsetFromExplosion = VEC3V_TO_VECTOR3(Subtract(pVehicle->GetVehiclePosition(), pCurrExplosionInst->GetPosWld()));
							float fDistanceToCentre = vOffsetFromExplosion.Mag();
							Vector3 vExtent = pVehicle->GetBoundingBoxMax();
							float fMinExtent = Min(vExtent.x, vExtent.y, vExtent.z) * 0.9f;
							if((fDistanceToCentre - fMinExtent) > CurBehavior.GetRadius() && (fDistanceToCentre - fMinExtent) < fExtendedRadius)
							{
								Vector3 vForceDirection = vOffsetFromExplosion;
								vForceDirection.NormalizeFast();
								pVehicle->ApplyImpulseCg(vForceDirection * dfBlastVehicleImpulse);
							
								for(int nDoor=0; nDoor<pVehicle->GetNumDoors(); nDoor++)
								{
									CCarDoor* pDoor = pVehicle->GetDoor(nDoor);
									if(pDoor && ( !NetworkInterface::IsGameInProgress() || !pVehicle->m_nPhysicalFlags.bIgnoresExplosions ) )
									{
										Matrix34 matDoor;
										if(pDoor->GetLocalMatrix(pVehicle, matDoor))
										{
											if(!pDoor->GetFlag(CCarDoor::LOOSE_LATCHED_DOOR) && fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < dfBlastCarDoorPopChance)
											{
												Matrix34 matVehicle;
												pVehicle->GetMatrixCopy(matVehicle);
												Vector3 vDoorOffset;
												matVehicle.Transform3x3(matDoor.d, vDoorOffset);
												if(vDoorOffset.Dot(vForceDirection) < 0.0f) // Only pop the door on the explosion side
												{
													pDoor->SetLooseLatch(pVehicle);
												}
											}
										}
									}
								}

								if (!pVehicle->GetDriver() && pVehicle->GetStatus()!=STATUS_WRECKED )		// If this car has a driver triggering the alarm would be silly (sometimes happenend in network games)
								{
									pVehicle->TriggerCarAlarm();
								}
							}
						}
					}
					RemoveExplosion(ExplosionIndex);
				}
			}
		}
	}
	
	// B*2031517: Update the recent explosions tracking. Remove explosions that occured outside of the rolling window.	
	const u32 nExplosionTimeWindow = 3000;

	u32 rollingWindowEndTime = fwTimer::GetTimeInMilliseconds();
	u32 rollingWindowStartTime = rollingWindowEndTime - nExplosionTimeWindow;
	while(!ms_recentExplosions.IsEmpty() && (ms_recentExplosions.Top() < rollingWindowStartTime || ms_recentExplosions.Top() > rollingWindowEndTime))
	{
		ms_recentExplosions.Pop();
	}

	// Find out how many real-mode vehicles are around.
	int nRealVehicles = 0;
	s32 i = (s32) CVehicle::GetPool()->GetSize();
	while(i--)
	{
		CVehicle * pVehicle = CVehicle::GetPool()->GetSlot(i);
		if (pVehicle && !pVehicle->GetIsInReusePool())
		{			
			switch(pVehicle->GetVehicleAiLod().GetDummyMode())
			{
			case VDM_REAL: { nRealVehicles++; break; }
			case VDM_DUMMY: { break; }
			case VDM_SUPERDUMMY: { break; }
			default: { break; }
			}
		}
	}

	// Update the vehicle damage reduction value.
	// This is only done when there's a sufficient number of real vehicles and the number of recent explosions is above the minimum. 
	// The reduction value approaches one as the number of recent explosions approaches the maximum number of explosions defined below. Reduction values above 1.0 have no additional effect.
	int nMinExplosionsForReduction = 1;
	int nMaxChainExplosionsForReduction = 5;
	if(nRealVehicles >= 20 && ms_recentExplosions.GetCount() > nMinExplosionsForReduction)
	{	
		CVehicleDamage::ms_fBreakingPartsReduction = (float)(ms_recentExplosions.GetCount() - nMinExplosionsForReduction) / (float)(nMaxChainExplosionsForReduction - nMinExplosionsForReduction);
	}
	else
		CVehicleDamage::ms_fBreakingPartsReduction = 0;
}

//-------------------------------------------------------------------------
// Flags all explosions for the specified entity for removal during the next post sim update
//-------------------------------------------------------------------------
void CExplosionManager::FlagsExplosionsForRemoval(CEntity* pEntity)
{
	for(int ExplosionIndex = 0; ExplosionIndex < MAX_EXPLOSIONS; ++ExplosionIndex)
	{
		if( m_apExplosionInsts[ExplosionIndex] && m_apExplosionInsts[ExplosionIndex]->GetExplodingEntity() == pEntity )
		{
			m_apExplosionInsts[ExplosionIndex]->SetNeedsRemoved(true);
		}
	}
}

//-------------------------------------------------------------------------
// Removes all explosions for by the specified entity
//-------------------------------------------------------------------------
void CExplosionManager::ClearExplosions(CEntity* pEntity)
{
	for(int ExplosionIndex = 0; ExplosionIndex < MAX_EXPLOSIONS; ++ExplosionIndex)
	{
		if( m_apExplosionInsts[ExplosionIndex] && m_apExplosionInsts[ExplosionIndex]->GetExplodingEntity() == pEntity )
		{
			RemoveExplosion(ExplosionIndex);
		}
	}
}

//-------------------------------------------------------------------------
// Clears all explosions
//-------------------------------------------------------------------------
void CExplosionManager::ClearAllExplosions( void )
{
	for(int ExplosionIndex = 0; ExplosionIndex < MAX_EXPLOSIONS; ++ExplosionIndex)
	{
		if( m_apExplosionInsts[ExplosionIndex] )
		{
			RemoveExplosion(ExplosionIndex);
		}
	}
}

//-------------------------------------------------------------------------
// Sets any explosions owned by the passed in entity to not report crimes
//-------------------------------------------------------------------------
void CExplosionManager::DisableCrimeReportingForExplosions(CEntity* pEntity)
{
	for(int ExplosionIndex = 0; ExplosionIndex < MAX_EXPLOSIONS; ++ExplosionIndex)
	{
		if( m_apExplosionInsts[ExplosionIndex] && m_apExplosionInsts[ExplosionIndex]->GetExplosionOwner() == pEntity )
		{
			m_apExplosionInsts[ExplosionIndex]->SetCanReportCrimes(false);
		}
	}
}


//-------------------------------------------------------------------------
// Actually starts the explosion in the physics world
//-------------------------------------------------------------------------
void CExplosionManager::RemoveExplosion( const s32 ExplosionIndex )
{
	weaponDebugf1("CExplosionManager::RemoveExplosion - Explosion Index %i", ExplosionIndex);
	phInstGta &CurInstance = *m_pExplosionPhysInsts[ExplosionIndex];

	if( m_aActivationDelay[ExplosionIndex] == 0 && m_apExplosionInsts[ExplosionIndex]->CanActivate())
	{
		// Remove the dummy entity associated with this instance
		CEntity* pEntity = CPhysics::GetEntityFromInst(&CurInstance);
		Assert( pEntity );
		// Switch the entities physics instance to prevent the destructor from deleting it
		// ( the CExplosionManager manages the creation/deletion of thephys insts)
		pEntity->ClearCurrentPhysicsInst();
		delete pEntity;
		CurInstance.SetEntity(NULL);

		CPhysics::GetSimulator()->RemoveInstBehavior(m_ExplosionBehaviors[ExplosionIndex]);
		CPhysics::GetSimulator()->DeleteObject(m_pExplosionPhysInsts[ExplosionIndex]->GetLevelIndex());
	}

	Assert( m_apExplosionInsts[ExplosionIndex] );
	delete m_apExplosionInsts[ExplosionIndex];
	m_apExplosionInsts[ExplosionIndex] = NULL;

	m_aCachedSmashables[ExplosionIndex].Reset();
}


//-------------------------------------------------------------------------
// Actually starts the explosion in the physics world
//-------------------------------------------------------------------------
bool CExplosionManager::StartExplosion( const s32 ExplosionIndex )
{
	phGtaExplosionInst* pExplosionInst = m_apExplosionInsts[ExplosionIndex];
	phInstGta* pExplosionPhysInst = m_pExplosionPhysInsts[ExplosionIndex];
	Assert(!pExplosionPhysInst->IsInLevel());

	const bool bIsSmoke = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(pExplosionInst->GetExplosionTag()).bIsSmokeGrenade;

	// process any non-timed explosion vfx/audio
	if (CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(pExplosionInst->GetExplosionTag()).directedLifeTime == 0.0f ||
		CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(pExplosionInst->GetExplosionTag()).bForceNonTimedVfx)
	{
		if (pExplosionInst->GetNoVfx()==false)
		{
			g_vfxExplosion.ProcessVfxExplosion(pExplosionInst, false, 0.0f);
		}
		if(pExplosionInst->GetMakesSound())
		{
			pExplosionInst->CreateAudio();
		}
	}
	else if(bIsSmoke)
	{
		// Smoke grenades are the one type of directed explosions we want to create audio for
		if(pExplosionInst->GetMakesSound())
		{
			pExplosionInst->CreateAudio();
		}
	}

	// Dont create explosion events, the molotovs will create fires which are better in this case
	if( pExplosionInst->GetDamageType()!=DAMAGE_TYPE_FIRE )
	{
		pExplosionInst->CreateCameraShake();

		phArchetype* pArchetype = pExplosionPhysInst->GetArchetype();
		Assert( pArchetype );
		if( pExplosionInst->GetDamageType() == DAMAGE_TYPE_SMOKE )
		{
			pArchetype->SetTypeFlags(ArchetypeFlags::GTA_SMOKE_TYPE);
			pArchetype->SetIncludeFlags(ArchetypeFlags::GTA_SMOKE_INCLUDE_TYPES);
		}
		else
		{
			pArchetype->SetTypeFlags(ArchetypeFlags::GTA_EXPLOSION_TYPE);
			pArchetype->SetIncludeFlags(ArchetypeFlags::GTA_EXPLOSION_INCLUDE_TYPES);
		}

		if( pExplosionInst->GetExplosionTag() != EXP_TAG_DIR_WATER_HYDRANT &&
			pExplosionInst->GetExplosionTag() != EXP_TAG_DIR_STEAM &&
			pExplosionInst->GetExplosionTag() != EXP_TAG_DIR_FLAME &&
			pExplosionInst->GetExplosionTag() != EXP_TAG_DIR_FLAME_EXPLODE &&
			pExplosionInst->GetExplosionTag() != EXP_TAG_SNOWBALL &&
			pExplosionInst->GetExplosionTag() != EXP_TAG_BIRD_CRAP)
		{
			// Construct the shocking event.
			CExplosionTagData& rData = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(pExplosionInst->GetExplosionTag());

			CEventShockingExplosion ev;

			// A bit confusing, maybe, but we still want police to see the event for bird crap even if peds can't report a crime for it.
			if(!pExplosionInst->CanReportCrime() && pExplosionInst->GetExplosionTag() != EXP_TAG_BIRD_CRAP)
			{
				ev.SetIgnoreIfLaw(true);
			}

			// We can't merge together bird crap events as we really want a localized response to it.
			if (pExplosionInst->GetExplosionTag() == EXP_TAG_BIRD_CRAP)
			{
				ev.SetCanBeMerged(false);
			}

			if (rData.shockingEventAudioRangeOverride != -1.0f)
			{
				ev.SetVisualReactionRangeOverride(rData.shockingEventAudioRangeOverride);
			}

			if (rData.shockingEventVisualRangeOverride != -1.0f)
			{
				ev.SetAudioReactionRangeOverride(rData.shockingEventVisualRangeOverride);
			}

			// Set the position / source entity of the shocking event.
			CEntity* pExplodingEntity = pExplosionInst->GetExplodingEntity();
			CEntity* pExplosionOwner = pExplosionInst->GetExplosionOwner();
			if(pExplosionOwner)
			{
				if(pExplosionInst->CanReportCrime())
				{
					// If the explosion owner exists then try to report a crime (unless we exploded the vehicle we are in)
					CPed* pInflictorPed = NULL;
					if(pExplosionOwner->GetIsTypeVehicle())
					{
						pInflictorPed = static_cast<CVehicle*>(pExplosionOwner)->GetDriver();
					}
					else if(pExplosionOwner->GetIsTypePed())
					{
						pInflictorPed = static_cast<CPed*>(pExplosionOwner);
					}

					if(pInflictorPed)
					{
						CVehicle* pVehicleInflictorInside = pInflictorPed->GetVehiclePedInside();
						if(!pVehicleInflictorInside || pVehicleInflictorInside != pExplodingEntity)
						{
							CCrime::ReportCrime(pExplosionInst->GetDamageType() == DAMAGE_TYPE_SMOKE ? CRIME_TERRORIST_ACTIVITY : CRIME_CAUSE_EXPLOSION, pExplosionInst->GetExplodingEntity(), pInflictorPed);
						}
					}
				}
			}
			else
			{
				pExplosionOwner = pExplodingEntity;
			}

			if(pExplodingEntity && pExplodingEntity->GetIsTypeVehicle())
			{
				//@@: location CEXPLOSIONMANAGER_STARTEXPLOSION
				CVehicle *pVehicle = (CVehicle *) pExplodingEntity;
				if(pVehicle->InheritsFromAutomobile() && pVehicle->GetStatus() == STATUS_WRECKED)
				{
					pVehicle->m_nPhysicalFlags.bRenderScorched = TRUE;  
				}
			}

			if(pExplosionOwner && !pExplosionOwner->GetIsTypeBuilding())
			{
				//Register this as an 'interesting event'
				g_InterestingEvents.Add(CInterestingEvents::EExplosion, pExplodingEntity);
				ev.Set(pExplosionInst->GetPosWld(), pExplosionOwner, NULL);
			}
			else
			{
				ev.Set(pExplosionInst->GetPosWld(), NULL, NULL);
			}

			// Add the shocking event to the world.
			CShockingEventsManager::Add(ev);

			REPLAY_ONLY(ReplayBufferMarkerMgr::AddMarker(2000,5000,IMPORTANCE_LOW);)
		}

		if( (pExplosionInst->GetExplosionTag() != EXP_TAG_DIR_WATER_HYDRANT) && 
			(pExplosionInst->GetExplosionTag() != EXP_TAG_FLARE) &&
			(pExplosionInst->GetExplosionTag() != EXP_TAG_BIRD_CRAP) &&
			(pExplosionInst->GetExplosionTag() != EXP_TAG_SNOWBALL) &&
			!bIsSmoke )
		{
			//Grab the explosion tag data.
			const CExplosionTagData& rData = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(pExplosionInst->GetExplosionTag());

			//Calculate the position & radius.
			Vec3V vPosition = pExplosionInst->GetPosWld();
			float fRadius = rData.endRadius * pExplosionInst->GetSizeScale();

			//Add the event.
			CEventExplosion ev(vPosition, pExplosionInst->GetExplosionOwner(), fRadius, rData.minorExplosion);
			GetEventGlobalGroup()->Add(ev);
		}
	}

	// Create a building adn associate it with the phys inst, most places assume
	// a physics inst will have a corresponding object, this is a dummy object to keep these
	// places happy.  It also enforces regreffing on the phys inst
	CBuilding *pBuilding = rage_new CBuilding( ENTITY_OWNEDBY_EXPLOSION );
	pBuilding->SetPhysicsInst(pExplosionPhysInst, false);

	CPhysics::GetSimulator()->AddInactiveObject(pExplosionPhysInst);
	CPhysics::GetLevel()->SetInactiveCollidesAgainstInactive(pExplosionPhysInst->GetLevelIndex(), true);
	if(!CPhysics::GetSimulator()->AddInstBehavior(m_ExplosionBehaviors[ExplosionIndex]))
	{
		// TODO: Rather than do this (try to add it, check for failure, and then clean up if it fails) I'd rather check first if it's going to work before anything gets done.
		CPhysics::GetSimulator()->DeleteObject(pExplosionPhysInst->GetLevelIndex());
	}

	// At the point the explosion starts, build a list of all the smashables that might 
	// be affected based on the predicted maximum radius of the explosion
	// These are used later to smash as the explosion reaches each objects radius, rather than completely
	// recalculating intersecting objects each frame.
	
	m_aCachedSmashables[ExplosionIndex].CacheSmashablesAffectedByExplosion(pExplosionInst, RCC_VECTOR3(pExplosionPhysInst->GetPosition()));
	

	// B*2031517: Keep track of explosions that happen within a small time window and are within a certain raidus of one another.
	// This is used to scale vehicle damage in chaotic situations.	
	float fMaxDistanceForExplosion = 25 * 25;

	Vector3 toNewExplosion = VEC3V_TO_VECTOR3(pExplosionInst->GetPosWld()) - ms_vExplosionTrackCentre; 
	if(!ms_recentExplosions.IsEmpty() && toNewExplosion.Mag2() <= fMaxDistanceForExplosion)
	{
		if(ms_recentExplosions.IsFull())
		{
			ms_recentExplosions.Pop();
		}

		ms_recentExplosions.Push(fwTimer::GetTimeInMilliseconds());

		ms_vExplosionTrackCentre = ms_vExplosionTrackCentre + (toNewExplosion / 2.0f);
	}
	else if(ms_recentExplosions.IsEmpty())
	{
		ms_recentExplosions.Push(fwTimer::GetTimeInMilliseconds());

		if(ms_vExplosionTrackCentre.IsZero())
			ms_vExplosionTrackCentre = VEC3V_TO_VECTOR3(pExplosionInst->GetPosWld());
		else
			ms_vExplosionTrackCentre = ms_vExplosionTrackCentre + (toNewExplosion / 2.0f);
	}

	return true;
}

#if __DEV
void CExplosionManager::DumpExplosionInfo()
{
	Displayf("Dumping Explosion Info:");
	for(int explosionIndex = 0; explosionIndex < MAX_EXPLOSIONS; ++explosionIndex)
	{
		if(const phGtaExplosionInst* pExplosionInst = m_apExplosionInsts[explosionIndex])
		{
			CExplosionTagData& explosionTagData = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(pExplosionInst->GetExplosionTag());
			Displayf("\tExplosion Index: %i",explosionIndex);
			Displayf("\t\tType: %s",explosionTagData.name.c_str());
			Displayf("\t\tPosition: %f, %f, %f", VEC3V_ARGS(pExplosionInst->GetPosWld()));
			Displayf("\t\tActivation Delay: %i", m_aActivationDelay[explosionIndex]);
		}
	}
}
#endif // __DEV

//-------------------------------------------------------------------------
// Spawns an explosion of the type passed at the given position, the manager
// handles the deletion of the explosion type
//-------------------------------------------------------------------------
phInstBehaviorExplosionGta * CExplosionManager::SpawnCustomExplosion( phGtaExplosionInst *pExplosionInst, s32 iActivationDelay )
{
	return SpawnExplosion( pExplosionInst, iActivationDelay );
}
//-------------------------------------------------------------------------
// Spawns an explosion of the type passed at the given position, the manager
// handles the deletion of the explosion type
//-------------------------------------------------------------------------
phInstBehaviorExplosionGta * CExplosionManager::SpawnExplosion( phGtaExplosionInst *pExplosionInst, s32 iActivationDelay )
{
	const bool bSphereExplosion		= pExplosionInst->GetPhysicalType() == PT_SPHERE;
	const int iStartExplosionIndex	= bSphereExplosion ? 0 : MAX_SPHERE_EXPLOSIONS;
	const int iEndExplosionIndex	= bSphereExplosion ? MAX_SPHERE_EXPLOSIONS : (MAX_SPHERE_EXPLOSIONS+MAX_DIRECTED_EXPLOSIONS);

	Assertf((float)iActivationDelay/1000.0f < 10.0f, "Spawning explosion with %f second delay, is this intended?",(float)iActivationDelay/1000.0f);

	// Search for an unused explosion
	int ExplosionIndexToUse = -1;
	for(int ExplosionIndex = iStartExplosionIndex; ExplosionIndex < iEndExplosionIndex; ++ExplosionIndex)
	{
		if( m_apExplosionInsts[ExplosionIndex] == NULL )
		{
			ExplosionIndexToUse = ExplosionIndex;
			break;
		}
	}

	if(ExplosionIndexToUse == -1)
	{
		if(CExplosionManager::InsideExplosionUpdate())
		{
			weaponDebugf1("CExplosionManager::SpawnExplosion - Failed - No available explosions at unsafe point in the frame");
			return NULL;
		}
		else
		{
			// We don't have room for a new explosion. Just kill the oldest one. 
			int oldestExplosionIndex = iStartExplosionIndex;
			for(int ExplosionIndex = iStartExplosionIndex + 1; ExplosionIndex < iEndExplosionIndex; ++ExplosionIndex)
			{
				Assert(m_apExplosionInsts[ExplosionIndex]);
				if(m_apExplosionInsts[ExplosionIndex]->GetSpawnTimeMS() < m_apExplosionInsts[oldestExplosionIndex]->GetSpawnTimeMS())
				{
					oldestExplosionIndex = ExplosionIndex;
				}
			}
#if __BANK
			sysStack::PrintStackTrace();		
			weaponDebugf1("CExplosionManager::SpawnExplosion - Deleting old explosion because the pool is full. ExplosionInst: 0x%p, PhysicsInst: 0x%p, Entity: 0x%p",m_apExplosionInsts[oldestExplosionIndex],m_pExplosionPhysInsts[oldestExplosionIndex],CPhysics::GetEntityFromInst(m_pExplosionPhysInsts[oldestExplosionIndex]));
#endif // __BANK
			RemoveExplosion(oldestExplosionIndex);
			ExplosionIndexToUse = oldestExplosionIndex;
		}
	}

	Assert( !m_apExplosionInsts[ExplosionIndexToUse] );
	// These will adjust the instance, so we need to do them before the object gets added to the level.
	m_apExplosionInsts[ExplosionIndexToUse] = pExplosionInst;
	m_ExplosionBehaviors[ExplosionIndexToUse].SetExplosionInst(m_apExplosionInsts[ExplosionIndexToUse]);
	m_ExplosionBehaviors[ExplosionIndexToUse].Reset();

	m_pExplosionPhysInsts[ExplosionIndexToUse]->SetPosition(pExplosionInst->GetPosWld());	
	m_ExplosionBehaviors[ExplosionIndexToUse].ComputeLosOffset();

	if (bSphereExplosion)
	{
		Mat34V vMtx = Mat34V(V_IDENTITY);
		vMtx.SetCol3(pExplosionInst->GetPosWld());
		m_pExplosionPhysInsts[ExplosionIndexToUse]->SetMatrix(vMtx);		
	}
	else
	{
		Vec3V vCapsuleCentrePos = pExplosionInst->GetPosWld() + (pExplosionInst->GetDirWld()*ScalarVFromF32(m_ExplosionBehaviors[ExplosionIndexToUse].GetRadius() * 0.5f));
		Mat34V vCapsuleMtx;
		CVfxHelper::CreateMatFromVecY(vCapsuleMtx, vCapsuleCentrePos, pExplosionInst->GetDirWld());
		m_pExplosionPhysInsts[ExplosionIndexToUse]->SetMatrix(vCapsuleMtx);
	}

	m_aActivationDelay[ExplosionIndexToUse] = iActivationDelay;

	// we can't activate explosions that have a network owner, we need to wait for them to give us the go ahead first
	if (m_aActivationDelay[ExplosionIndexToUse] == 0 && pExplosionInst->CanActivate())
	{
		StartExplosion( ExplosionIndexToUse );
	}

	weaponDebugf1("CExplosionManager::SpawnExplosion - Success - Explosion Index %i", ExplosionIndexToUse);
	return &m_ExplosionBehaviors[ExplosionIndexToUse];
}

//-------------------------------------------------------------------------
// Finds the first explosion in an angled area
//-------------------------------------------------------------------------
phGtaExplosionInst* CExplosionManager::FindFirstExplosionInAngledArea(const eExplosionTag TestExplosionTag, Vector3& vPos1, Vector3& vPos2, float areaWidth)
{
	int ExplosionIndex;
	for(ExplosionIndex = 0; ExplosionIndex < MAX_EXPLOSIONS ; ++ExplosionIndex)
	{
		phGtaExplosionInst* pExplosion = m_apExplosionInsts[ExplosionIndex];
		if( pExplosion && (
			pExplosion->GetExplosionTag() == TestExplosionTag ||
			TestExplosionTag == EXP_TAG_DONTCARE ) )
		{
			Vec3V vExpPos = pExplosion->GetPosWld();
			if(CScriptAreas::IsPointInAngledArea(RCC_VECTOR3(vExpPos), vPos1, vPos2, areaWidth, false, true, false))
			{
				return pExplosion;
			}
		}
	}
	return NULL;
}

//-------------------------------------------------------------------------
// Finds the first explosion in a defined sphere
//-------------------------------------------------------------------------
phGtaExplosionInst* CExplosionManager::FindFirstExplosionInSphere( const eExplosionTag TestExplosionTag, Vector3& vCentre, float fRadius )
{
	float fRadiusSquared = (fRadius * fRadius);
	int ExplosionIndex;
	for(ExplosionIndex = 0; ExplosionIndex < MAX_EXPLOSIONS ; ++ExplosionIndex)
	{
		phGtaExplosionInst* pExplosion = m_apExplosionInsts[ExplosionIndex];
		if( pExplosion && (
			pExplosion->GetExplosionTag() == TestExplosionTag ||
			TestExplosionTag == EXP_TAG_DONTCARE ) )
		{
			Vec3V vPos = pExplosion->GetPosWld();
			Vector3 vDiff = RCC_VECTOR3(vPos) - vCentre;
			float fDistSquared = vDiff.Mag2();
			if (fDistSquared < fRadiusSquared)
			{
				return pExplosion;
			}
		}
	}
	return NULL;
}

//-------------------------------------------------------------------------
// Fill query list of explosion instances by tag
//-------------------------------------------------------------------------
int CExplosionManager::FindExplosionsByTag(const eExplosionTag TestExplosionTag, phGtaExplosionInst** out_explosionInstanceList, const int iMaxNumExplosionInstances)
{
	// Track number of instances found
	int iNumInstancesFound = 0;
	
	// Traverse explosions
	for(int ExplosionIndex = 0; ExplosionIndex < MAX_EXPLOSIONS; ExplosionIndex++)
	{
		// If matching explosion found
		phGtaExplosionInst* pExplosion = m_apExplosionInsts[ExplosionIndex];
		if( pExplosion && (
			pExplosion->GetExplosionTag() == TestExplosionTag ||
			TestExplosionTag == EXP_TAG_DONTCARE ) )
		{
			// add to the output list
			out_explosionInstanceList[iNumInstancesFound] = pExplosion;
			iNumInstancesFound++;

			// stop traversal if output list limit reached
			if(iNumInstancesFound >= iMaxNumExplosionInstances)
			{
				break;
			}
		}
	}

	// report number found
	return iNumInstancesFound;
}

//-------------------------------------------------------------------------
// Finds the owner of an explosion in an angled area
//-------------------------------------------------------------------------
CEntity* CExplosionManager::GetOwnerOfExplosionInAngledArea(const eExplosionTag TestExplosionTag, Vector3& vPos1, Vector3& vPos2, float areaWidth)
{
	//Find the first explosion in the angled area.
	phGtaExplosionInst* pExplosion = FindFirstExplosionInAngledArea(TestExplosionTag, vPos1, vPos2, areaWidth);
	if(!pExplosion)
	{
		return NULL;
	}
	else
	{
		return pExplosion->GetExplosionOwner();
	}
}

//-------------------------------------------------------------------------
// Finds the owner of an explosion in a sphere
//-------------------------------------------------------------------------
CEntity* CExplosionManager::GetOwnerOfExplosionInSphere(const eExplosionTag TestExplosionTag, Vector3& vCentre, float fRadius)
{
	//Find the first explosion in the sphere.
	phGtaExplosionInst* pExplosion = FindFirstExplosionInSphere(TestExplosionTag, vCentre, fRadius);
	if(!pExplosion)
	{
		return NULL;
	}
	else
	{
		return pExplosion->GetExplosionOwner();
	}
}

//-------------------------------------------------------------------------
// Returns true if an explosion's center is present in the defined area
//-------------------------------------------------------------------------
bool CExplosionManager::TestForExplosionsInArea( const eExplosionTag TestExplosionTag, Vector3& vMin, Vector3 &vMax, bool bIncludeDelayedExplosions )
{
	int ExplosionIndex;
	for(ExplosionIndex = 0; ExplosionIndex < MAX_EXPLOSIONS ; ++ExplosionIndex)
	{
		phGtaExplosionInst* pExplosion = m_apExplosionInsts[ExplosionIndex];
		if( pExplosion && 
			( pExplosion->GetExplosionTag() == TestExplosionTag ||
			  TestExplosionTag == EXP_TAG_DONTCARE ) &&
			( bIncludeDelayedExplosions ||
			  m_aActivationDelay[ ExplosionIndex ] == 0 ) )
		{
			Vec3V vExpPos = pExplosion->GetPosWld();
			Vector3 vPos = RCC_VECTOR3(vExpPos);
			if(	(vPos.x >= vMin.x) &&
				(vPos.x <= vMax.x) &&
				(vPos.y >= vMin.y) &&
				(vPos.y <= vMax.y) &&
				(vPos.z >= vMin.z) &&
				(vPos.z <= vMax.z))
			{
				return true;
			}
		}
	}
	return false;
}

//-------------------------------------------------------------------------
// Returns true if an explosion's center is present in the defined angled area
//-------------------------------------------------------------------------
bool CExplosionManager::TestForExplosionsInAngledArea( const eExplosionTag TestExplosionTag, Vector3& vPos1, Vector3& vPos2, float areaWidth)
{
	return (FindFirstExplosionInAngledArea(TestExplosionTag, vPos1, vPos2, areaWidth) != NULL);
}

//-------------------------------------------------------------------------
// Returns true if an explosion's center is present in the defined sphere
//-------------------------------------------------------------------------
bool CExplosionManager::TestForExplosionsInSphere( const eExplosionTag TestExplosionTag, Vector3& vCentre, float fRadius )
{
	return (FindFirstExplosionInSphere(TestExplosionTag, vCentre, fRadius) != NULL);
}

//-------------------------------------------------------------------------
// Removes all explosions whose center is in the defined area
// Returns true if any were removed
//-------------------------------------------------------------------------
bool CExplosionManager::RemoveExplosionsInArea( const eExplosionTag TestExplosionTag, const Vector3& Centre, float Radius )
{
	int ExplosionIndex;
	bool bExplosionsRemoved = false;
	for(ExplosionIndex = 0; ExplosionIndex < MAX_EXPLOSIONS ; ++ExplosionIndex)
	{
		phGtaExplosionInst* pExplosion = m_apExplosionInsts[ExplosionIndex];
		if( pExplosion && (
			pExplosion->GetExplosionTag() == TestExplosionTag ||
			TestExplosionTag == EXP_TAG_DONTCARE ) )
		{
			Vec3V vPos = pExplosion->GetPosWld();
			if( RCC_VECTOR3(vPos).Dist2(Centre) < rage::square(Radius) )
			{
				RemoveExplosion(ExplosionIndex);
				bExplosionsRemoved = true;
			}
		}
	}
	return bExplosionsRemoved;
}

//-------------------------------------------------------------------------
// Creates a physics explosion and associated vfx and audio
// Parameters	:	explosionArgs - the explosion parameters
// Returns		:	TRUE if explosion added successfully, FALSE otherwise
//-------------------------------------------------------------------------
bool CExplosionManager::AddExplosion(CExplosionArgs& explosionArgs, CProjectile* pProjectile, bool bForceExplosion)
{
	/*	if (GetPhysicalType(ExplosionTag)==PT_DIRECTED)
	{
	#if __ASSERT
	Assert(pvDirection->Mag()>=0.997f && pvDirection->Mag()<=1.003f);
	#endif

	// this is a directed explosion - don't let this start if there is another active one too close by on this entity
	for (int i=0; i<MAX_EXPLOSIONS; ++i)
	{
	if (m_apExplosionInsts[i] && m_apExplosionInsts[i]->GetAttachEntity() && m_apExplosionInsts[i]->GetAttachEntity()==pAttachEntity && m_apExplosionInsts[i]->GetAttachBoneTag()==attachBoneTag)
	{
	// explosion on the same entity bone - check distance
	Matrix34 expMat = m_apExplosionInsts[i]->GetAttachOffsetMatrix();
	expMat.Dot(*m_apExplosionInsts[i]->GetAttachParentMatrix());
	Vector3 vec = vecExplosionPosition-expMat.d;
	if (vec.Mag2()<0.5f*0.5f)
	{
	return false;
	}
	}
	}
	}
	*/

#if __ASSERT
	if (GetPhysicalType(explosionArgs.m_explosionTag)==PT_DIRECTED)
	{
		Assert(explosionArgs.m_vDirection.Mag()>=0.997f && explosionArgs.m_vDirection.Mag()<=1.003f);
	}
#endif

#if __BANK
	if (!explosionArgs.m_pEntExplosionOwner)
	{
		weaponDebugf1("CExplosionManager::AddExplosion - owner is null");
		sysStack::PrintStackTrace();
	}
#endif //__BANK

#if __BANK
	weaponDebugf1("CExplosionManager::AddExplosion - explosionTag %i - weaponHash %i - pProjectile 0x%p - bForceExplosion [%s], owner - %s[%p]", explosionArgs.m_explosionTag, explosionArgs.m_weaponHash, pProjectile, bForceExplosion ? "T" : "F", explosionArgs.m_pEntExplosionOwner && explosionArgs.m_pEntExplosionOwner->GetIsDynamic() ? AILogging::GetDynamicEntityNameSafe(static_cast<const CDynamicEntity*>(explosionArgs.m_pEntExplosionOwner.Get())) : (explosionArgs.m_pEntExplosionOwner ? explosionArgs.m_pEntExplosionOwner->GetModelName() : "null"), explosionArgs.m_pEntExplosionOwner.Get());
	if (PARAM_logExplosionCreation.Get())
	{
		sysStack::PrintStackTrace();
	}
#endif

	//Don't do distance check if we have requested to force an explosion.
	if(!bForceExplosion)
	{
		// go through the explosions
		for (int i=0; i<MAX_EXPLOSIONS; ++i)
		{
			// check if this explosion is valid
			// Only do the distance check against the 'instant' explosion
			if (m_apExplosionInsts[i] && CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(m_apExplosionInsts[i]->GetExplosionTag()).directedLifeTime == 0.0f )
			{
				// test if we need to do a distance check on this explosion
				float testDistSqr = 0.0f;
				if (GetPhysicalType(explosionArgs.m_explosionTag)==PT_SPHERE && GetPhysicalType(m_apExplosionInsts[i]->GetExplosionTag())==PT_SPHERE)
				{			
					// get the size of the explosions
					const CExplosionTagData& currExpTagData = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(m_apExplosionInsts[i]->GetExplosionTag());
					float currExpSize = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionEndRadius(currExpTagData);
					const CExplosionTagData& newExpTagData = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(explosionArgs.m_explosionTag);
					float newExpSize = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionEndRadius(newExpTagData);

					// only test for explosion rejection if the new size is less or equal to the current one
					if (newExpSize<=currExpSize)
					{
						// we're trying to add a sphere explosion and this is a sphere explosion - don't let it start too closely to any other active sphere explosion
						testDistSqr = rage::square(currExpSize*0.333f);
					}
				}
				else if (GetPhysicalType(explosionArgs.m_explosionTag)==PT_DIRECTED && GetPhysicalType(m_apExplosionInsts[i]->GetExplosionTag())==PT_DIRECTED)
				{
					// we're trying to add a directed explosion and this is a directed explosion - don't let it start too closely to any other directed explosion on the same entity bone
					if (m_apExplosionInsts[i]->GetAttachEntity() && m_apExplosionInsts[i]->GetAttachEntity()==explosionArgs.m_pAttachEntity /*&& m_apExplosionInsts[i]->GetAttachBoneTag()==explosionArgs.m_attachBoneTag*/)
					{
						// explosion on the same entity bone - check distance
						testDistSqr = 0.5f*0.5f;
					}
				}

				// do the distance check if required
				TUNE_GROUP_BOOL(EXPLOSIVE_TUNE, UNDERWATER_MINES_SKIP_DISTANCE_CHECK, true);
				if (testDistSqr>0.0f && (!UNDERWATER_MINES_SKIP_DISTANCE_CHECK || explosionArgs.m_explosionTag != EXP_TAG_MINE_UNDERWATER))
				{
					Vec3V vExpPosWld = m_apExplosionInsts[i]->GetPosWld();
					Vector3 vec = explosionArgs.m_explosionPosition-RCC_VECTOR3(vExpPosWld);
					if (vec.Mag2()<testDistSqr && explosionArgs.m_activationDelay == 0)
					{
						weaponDebugf1("CExplosionManager::AddExplosion - Failed - Too close to another explosion");
						// too close - don't add the explosion 
						return false;
					}
				}
			}
		}
	}

	// clamp input so it's valid
	explosionArgs.m_sizeScale = rage::Max(explosionArgs.m_sizeScale, 0.01f);
	explosionArgs.m_sizeScale = rage::Min(explosionArgs.m_sizeScale, 1.0f);

	bool bIsPlayerExplosion = false;
	if (explosionArgs.m_pEntExplosionOwner && explosionArgs.m_pEntExplosionOwner->GetIsTypePed() && ((CPed*)explosionArgs.m_pEntExplosionOwner.Get())->IsLocalPlayer())
	{
		CGameWorld::FindLocalPlayer()->GetPlayerInfo()->HavocCaused += HAVOC_CAUSEEXPLOSION;
		bIsPlayerExplosion = true;
	}

	phGtaExplosionInst* pExplosionInst = NULL;

	if (explosionArgs.m_camShakeNameHash == 0)
	{
		// default cam shake name hash - use default value from the tag data
		explosionArgs.m_camShakeNameHash = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(explosionArgs.m_explosionTag).GetCamShakeNameHash();
	}

	if (explosionArgs.m_fCamShake==-1.0f)
	{
		// default cam shake strength - use default value from the tag data
		explosionArgs.m_fCamShake = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(explosionArgs.m_explosionTag).camShake;
	}

	if (explosionArgs.m_fCamShakeRollOffScaling == -1.0f)
	{
		// default cam shake roll-off scaling - use default value from the tag data
		explosionArgs.m_fCamShakeRollOffScaling = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(explosionArgs.m_explosionTag).camShakeRollOffScaling;
	}

	if(NetworkInterface::IsGameInProgress() && !explosionArgs.m_networkIdentifier.IsValid() && !explosionArgs.m_bIsLocalOnly)
	{
		// explosions without an identifier are assigned to our machine during a network game
		explosionArgs.m_networkIdentifier.Set(NetworkInterface::GetLocalPhysicalPlayerIndex(), CNetFXIdentifier::GetNextFXId());
	}

	// Create an explosion of the given type
	pExplosionInst = CreateExplosionOfType( explosionArgs.m_pExplodingEntity, 
											explosionArgs.m_pEntExplosionOwner, 
											explosionArgs.m_pEntIgnoreDamage, 
											explosionArgs.m_explosionTag, 
											explosionArgs.m_sizeScale, 
											explosionArgs.m_explosionPosition,
											explosionArgs.m_camShakeNameHash,
											explosionArgs.m_fCamShake,
											explosionArgs.m_fCamShakeRollOffScaling,
											explosionArgs.m_bMakeSound, 
											explosionArgs.m_bNoFx, 
											explosionArgs.m_bInAir,
											explosionArgs.m_vDirection, 
											explosionArgs.m_pAttachEntity,
											explosionArgs.m_attachBoneTag,
											explosionArgs.m_vfxTagHash,
											explosionArgs.m_networkIdentifier,
											explosionArgs.m_bNoDamage,
											explosionArgs.m_bAttachedToVehicle,
											explosionArgs.m_bDetonatingOtherPlayersExplosive,
											explosionArgs.m_weaponHash,
											explosionArgs.m_bDisableDamagingOwner,
											explosionArgs.m_interiorLocation);

	if( !pExplosionInst )
	{
		weaponDebugf1("CExplosionManager::AddExplosion - Failed - CreateExplosionOfType returned NULL");
		return false;
	}

	if((pProjectile && pProjectile->GetDisableExplosionCrimes()) || CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(explosionArgs.m_explosionTag).bSuppressCrime)
	{
		pExplosionInst->SetCanReportCrimes(false);
	}

	// If an original tag is passed, set it up
	if( explosionArgs.m_originalExplosionTag !=	EXP_TAG_DONTCARE )
		pExplosionInst->SetOriginalExplosionTag(explosionArgs.m_originalExplosionTag);

	// Check to see if we can start this explosion
	if( !SpawnExplosion( pExplosionInst, explosionArgs.m_activationDelay ) )
	{
		weaponDebugf1("CExplosionManager::AddExplosion - Failed - SpawnExplosion returned false");
		delete pExplosionInst;
		return false;
	}

	if (bIsPlayerExplosion)
	{
		if (explosionArgs.m_pEntExplosionOwner && explosionArgs.m_pEntExplosionOwner->GetIsTypePed())
		{
			const bool bCreateSonarBlip = true;
			const bool bUpdatePosition  = false; // Explosions don't move with the player that caused them.
			CPed* pExplosionPed = static_cast<CPed*>(explosionArgs.m_pEntExplosionOwner.Get());
			CMiniMap::CreateSonarBlipAndReportStealthNoise(pExplosionPed, VECTOR3_TO_VEC3V(explosionArgs.m_explosionPosition), CMiniMap::sm_Tunables.Sonar.fSoundRange_Explosion, HUD_COLOUR_RED, bCreateSonarBlip, bUpdatePosition);
		}
	}

	if (NetworkInterface::IsGameInProgress() && !pExplosionInst->GetNetworkIdentifier().IsClone() && !explosionArgs.m_bIsLocalOnly && !NetworkInterface::IsInMPCutscene())
	{
		// inform the other machines about the explosion, exploding entities sync their explosions via their sync update
		CExplosionEvent::Trigger(explosionArgs, pProjectile);
	}

	// if this explosion is on a vehicle then make sure it doesn't get deleted in the next 10 seconds
	if (explosionArgs.m_pAttachEntity)
	{
		if (explosionArgs.m_pAttachEntity->GetIsTypeVehicle())
		{
			CVehicle* pVehicle = static_cast<CVehicle*>(explosionArgs.m_pAttachEntity.Get());
			pVehicle->DelayedRemovalTimeReset(10000);
		}
		else if (NetworkInterface::IsGameInProgress() && explosionArgs.m_pAttachEntity->GetIsTypeObject() && !pExplosionInst->GetNetworkIdentifier().IsClone() && !explosionArgs.m_bIsLocalOnly)
		{
			CObject* pObject = static_cast<CObject*>(explosionArgs.m_pAttachEntity.Get());

			// register the entity the explosion is attached to with the network 
			if (pObject->GetOwnedBy() == ENTITY_OWNEDBY_RANDOM && CNetObjObject::CanBeNetworked(pObject) && NetworkInterface::CanRegisterObject(NET_OBJ_TYPE_OBJECT, false))
			{
				pObject->m_nObjectFlags.bWeWantOwnership = 1;
				NetworkInterface::RegisterObject(pObject);

				if (pObject->GetNetworkObject())
				{
					static_cast<CNetObjObject*>(pObject->GetNetworkObject())->KeepRegistered();
				}

			}
		}
	}

	weaponDebugf1("CExplosionManager::AddExplosion - Success");
	return true;
}

void CExplosionManager::RegisterExplosionActivation(u16 levelIndex)
{
	if(!sm_ActivatedObjects.IsFull())
	{
		sm_ActivatedObjects.Push(phHandle(levelIndex, PHLEVEL->GetGenerationID(levelIndex)));
	}
}

void CExplosionManager::RemoveDeactivatedObjects()
{
	for(int objectIndex = 0; objectIndex < sm_ActivatedObjects.GetCount();)
	{
		phHandle objectHandle = sm_ActivatedObjects[objectIndex];
		if(!PHLEVEL->IsLevelIndexGenerationIDCurrent(objectHandle.GetLevelIndex(), objectHandle.GetGenerationId()) || !PHLEVEL->IsActive(objectHandle.GetLevelIndex()))
		{
			// If the original object isn't valid, or is now inactive, remove it from the list of activated objects to make room for more
			sm_ActivatedObjects[objectIndex] = sm_ActivatedObjects[sm_ActivatedObjects.GetCount()-1];
			sm_ActivatedObjects.Pop();
		}
		else
		{
			objectIndex++;
		}
	}
}

int CExplosionManager::CountExplosions()
{
	int numExplosions = 0;

	for(int ExplosionIndex = 0; ExplosionIndex < MAX_EXPLOSIONS; ++ExplosionIndex)
	{
		if( m_apExplosionInsts[ExplosionIndex] )
		{
			numExplosions++;
		}
	}

	return numExplosions;
}

void CExplosionManager::ActivateNetworkExplosion(const CNetFXIdentifier& networkIdentifier)
{
	int i=0;

	// find the explosion
	for (i=0; i<MAX_EXPLOSIONS; ++i)
	{
		phGtaExplosionInst* pExplosionInst = m_apExplosionInsts[i];

		if (pExplosionInst && pExplosionInst->GetNetworkIdentifier() == networkIdentifier)
		{
			// clearing the net owner means that the explosion will now proceed
			pExplosionInst->GetNetworkIdentifier().Reset();

			if (m_aActivationDelay[i] == 0)
			{
				StartExplosion( i );	
			}

			break;
		}
	}

	Assert(i<=MAX_EXPLOSIONS);
}

void CExplosionManager::RemoveNetworkExplosion(const CNetFXIdentifier& networkIdentifier)
{
	int i=0;

	// find the explosion
	for (i=0; i<MAX_EXPLOSIONS; ++i)
	{
		if (m_apExplosionInsts[i] && m_apExplosionInsts[i]->GetNetworkIdentifier() == networkIdentifier)
		{
			RemoveExplosion(i);
			break;
		}
	}
}

static dev_float STD_ACCEL_CLAMP = 50.0f;
static dev_float RAGDOLL_ACCEL_CLAMP = 20.0f;

//-------------------------------------------------------------------------
// Callback from the physics simulator used to apply damage to an impacted
// instance
//-------------------------------------------------------------------------
void CExplosionManager::ExplosionPreModifyForce(phGtaExplosionCollisionInfo& collisionInfo, eExplosionTag explosionTag)
{
	CEntity* pEntity = CPhysics::GetEntityFromInst(collisionInfo.m_OtherInst);

	if(!pEntity)
		return;

	CPhysical* pPhysical = pEntity->GetIsPhysical() ? static_cast<CPhysical*>(pEntity) : NULL;

	if(!pPhysical)
	{
		Assert(false);
		return;
	}

	if(Verifyf(collisionInfo.m_OtherInst && collisionInfo.m_OtherInst->GetArchetype(), "Other inst=0x%p.", collisionInfo.m_OtherInst))
	{
		float fAccelClamp = STD_ACCEL_CLAMP;

		if(pPhysical->GetIsTypePed() && collisionInfo.m_OtherInst->GetClassType() == PH_INST_FRAG_PED )
		{
#if __DEBUG
			Vec3V vecPrintForce = collisionInfo.m_ForceDir * collisionInfo.m_ForceMag;
			Displayf( "Force requested for ped %f (%f, %f, %f) frame(%d)\n", collisionInfo.m_ForceMag.Getf(), vecPrintForce.GetXf(), vecPrintForce.GetYf(), vecPrintForce.GetZf(), fwTimer::GetSystemFrameCount() );
#endif
			fAccelClamp = RAGDOLL_ACCEL_CLAMP;

			const float fRagdollForceModifier = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(explosionTag).fRagdollForceModifier;
			collisionInfo.m_ForceMag = Scale(collisionInfo.m_ForceMag, ScalarVFromF32(fRagdollForceModifier));

			Assert(collisionInfo.m_Component>=0 && collisionInfo.m_Component <= 20 );

			if(IsGreaterThanAll(collisionInfo.m_ForceDir.GetZ(), ScalarV(V_ZERO)) != 0)
			{
				collisionInfo.m_ForceDir.SetZ( Min(ScalarV(V_ONE), Add(collisionInfo.m_ForceDir.GetZ(), ScalarVFromF32(0.3f))) );
				collisionInfo.m_ForceDir = Normalize(collisionInfo.m_ForceDir);
			}

#if __DEBUG
			Vec3V vecForceToApply = collisionInfo.m_ForceDir * collisionInfo.m_ForceMag;
			Displayf( "Force applied to ped  %f(%f, %f, %f) frame(%d)\n", collisionInfo.m_ForceMag.Getf(), vecForceToApply.GetXf(), vecForceToApply.GetYf(), vecForceToApply.GetZf(), fwTimer::GetSystemFrameCount() );
#endif

		}
		else if( pPhysical->GetIsTypePed() && collisionInfo.m_OtherInst->GetClassType() == PH_INST_PED )
		{
			collisionInfo.m_ForceMag = ScalarV(V_ZERO);
		}
		else if(collisionInfo.m_bSelfCollision)
		{
			const float fSelfForceModifier = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(explosionTag).fSelfForceModifier;
			collisionInfo.m_ForceMag = Scale(collisionInfo.m_ForceMag, ScalarVFromF32(fSelfForceModifier));
			fAccelClamp *= fSelfForceModifier;
		}

		ScalarV maxAccelClamp = Scale(ScalarVFromF32(fAccelClamp), ScalarVFromF32(collisionInfo.m_OtherInst->GetArchetype()->GetMass()));
		collisionInfo.m_ForceMag = Min(collisionInfo.m_ForceMag, maxAccelClamp);
	}
}

bool CExplosionManager::IsFriendlyFireAllowed(CPed* pVictimPed, phGtaExplosionInst* pExplosionInst)
{
	if (pVictimPed && pExplosionInst)
	{
		const CExplosionTagData& explosionData = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(pExplosionInst->GetExplosionTag());

		if (NetworkInterface::IsFriendlyFireAllowed(pVictimPed, pExplosionInst->GetExplosionOwner()))		
			return true;
		
		if (explosionData.bAlwaysAllowFriendlyFire)		
			return true;		
	}
	
	return false;
}

bool CExplosionManager::IsFriendlyFireAllowed(CVehicle* pVictimVehicle, phGtaExplosionInst* pExplosionInst)
{
	if (pVictimVehicle && pExplosionInst)
	{
		const CExplosionTagData& explosionData = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(pExplosionInst->GetExplosionTag());

		if (NetworkInterface::IsFriendlyFireAllowed(pVictimVehicle, pExplosionInst->GetExplosionOwner()))		
			return true;
		
		if (explosionData.bAlwaysAllowFriendlyFire)
			return true;					
	}	

	return false;
}

dev_float EXPLOSION_FORCE_VEHICLE_CHAIN_EXPLOSION_REDUCTION = 1.1f;
dev_float EXPLOSION_DAMAGE_VEHICLE_MODIFIER = 3.0f;
dev_float STICKYBOMB_EXPLOSION_DAMPEN_DAMAGE_VEHICLE_MODIFIER = 0.25f;
dev_float EXPLOSION_DAMAGE_PED_MODIFIER = 1.0f;
dev_float EXPLOSION_DAMAGE_OBJECT_MODIFIER = 10.0f;
dev_float INSTANT_EXPLOSION_DISTANCE = 5.0f;
dev_float DELAYED_EXPLOSION_DISTANCE = 2.0f;
dev_float MAX_WATER_DEPTH_TO_TRIGGER_FIRE = 0.1f;
const ScalarV RATIO_OF_EXPLOSION_THAT_TRIGGERS_FIRE(0.38f);


//-------------------------------------------------------------------------
// Callback from the physics simulator used to apply damage to an impacted
// instance
//-------------------------------------------------------------------------
void CExplosionManager::ExplosionImpactCallback( phInst *pInst,
												ScalarV_In fForceMag,
												ScalarV_In UNUSED_PARAM(fUncappedForceMag),
												const float fExplosionRadius,
												Vec3V_In vForceDir,
												Vec3V_In vHitPos,
												CEntity *pExplosionOwner,
												CEntity *pExplodingEntity,
												Vec3V_InOut vExplosionPosition,
												phGtaExplosionInst* pExplosionInst,
												int iComponent )
{
	Assert(pInst);
	if( !pInst )
	{
		return;
	}

	CEntity* pEntity = CPhysics::GetEntityFromInst(pInst);
	if( !pEntity )
	{
		return;
	}

	// Bird crap "explosions" should cause no damage.
	if (pExplosionInst && pExplosionInst->GetExplosionTag() == EXP_TAG_BIRD_CRAP)
	{
		return;
	}

	CPhysical* pPhysical=(CPhysical*)pEntity;

	// const Vec3V vForce = Scale(vForceDir, fForceMag);
	u32 weaponHash = WEAPONTYPE_EXPLOSION;
	if(pExplosionInst)
		weaponHash = pExplosionInst->GetWeaponHash();

	eDamageType damageType = weaponHash != 0 ? CWeaponInfoManager::GetInfo<CWeaponInfo>(weaponHash)->GetDamageType() : DAMAGE_TYPE_EXPLOSIVE;

	//----

	// we're playing multiplayer...
	if(NetworkInterface::IsGameInProgress())
	{
		// we're trying to blow up an object...
		if(pEntity)
		{
			if (pExplosionOwner && NetworkInterface::IsDamageDisabledInMP(*pEntity, *pExplosionOwner))
			{
				return;
			}

			if( pEntity->GetIsTypeObject())
			{
				CBaseModelInfo const* pModelInfo = pEntity->GetBaseModelInfo();

				// the object is a pane of glass...
				if(pModelInfo->GetFragType() && pModelInfo->GetFragType()->GetNumGlassPaneModelInfos() > 0)
				{
					// and the firing object is local...
					if(!NetworkUtils::IsNetworkClone(pExplosionOwner))
					{	
						// so we're taking control - we keep a network record of this pane being destroyed and will pass it to other machines accordingly...
						CGlassPaneManager::RegisterGlassGeometryObject_OnNetworkOwner( (CObject const *)pPhysical, VEC3V_TO_VECTOR3(vHitPos), (u8)iComponent );
					}
				}
			}
			else if (pEntity->GetIsTypePed() && pEntity != pExplosionOwner)
			{
				// If the ped should ignore the damage, e.g., in passive mode.
				CPed* pPed = static_cast<CPed *>(pEntity);
				if (pPed->IsPlayer())
				{
					if (pExplosionOwner && pExplosionOwner->GetIsTypePed())
					{
						CPed* pPedOwner = static_cast<CPed*>(pExplosionOwner);
						if (!pPedOwner->IsAllowedToDamageEntity(NULL, pPed))
						{
							return;
						}
					}
					//B*2182963: Don't apply damage if owner is null (ie left the game) but the target is in passive mode.
					else if (!pExplosionOwner && !NetworkInterface::IsActivitySession())
					{
						if (pPed->GetNetworkObject())
						{
							CNetObjPlayer* pNetObjPlayer = static_cast<CNetObjPlayer*>(pPed->GetNetworkObject());
							if (pNetObjPlayer && !pNetObjPlayer->IsFriendlyFireAllowed())
							{
								return;
							}
						}
					}
				}
			}
		}
	}

	//----

	// Special case: 
	// Apply constant rumble to cars that are in contact with fire hydrant spray
	// This explosion isn't set to cause continuous damage so we have to put this here
	if( pPhysical->GetIsTypeVehicle() &&
		pExplosionInst->GetExplosionTag() == EXP_TAG_DIR_WATER_HYDRANT )
	{
		CVehicle* pVehicle = (CVehicle*)pPhysical;
		
		if( pVehicle->ContainsLocalPlayer() )
		{
			static dev_u16 snRumbleDurationMin = 2;
			static dev_u16 snRumbleDurationMax = 30;
			static dev_float sfRumbleIntensityMin = 0.005f;
			static dev_float sfRumbleIntensityMax = 0.1f;
			u32 duration	= (u32)fwRandom::GetRandomNumberInRange( snRumbleDurationMin, snRumbleDurationMax );
			float intensity	= fwRandom::GetRandomNumberInRange( sfRumbleIntensityMin, sfRumbleIntensityMax );

			CControlMgr::StartPlayerPadShakeByIntensity( duration, intensity );
		}
	}

	// OLD LOGIC, REWRITTEN BELOW

	//if( !bExplosionAppliesContinuousDamage && ( bEntityAlreadyDamaged || !pExplosionInst->AddDamagedEntity( pEntity ) ) && !bPedRagdollBeingActivated )
	//{
	//	return;
	//}

	// NEW LOGIC, UNTANGLED

	// Check to see if we've been damaged before by this explosion instance
	const bool bEntityAlreadyDamaged = pExplosionInst->IsEntityAlreadyDamaged( pEntity );

	// If the damage is non-continuous (only applies once), we need to check this
	if( !pExplosionInst->ShouldApplyContinuousDamage() )
	{
		// Return early if the entity is already on the list, or we failed to add the new entity (full)
		if( bEntityAlreadyDamaged || !pExplosionInst->AddDamagedEntity(pEntity) ) 
		{
			// Don't return early if ragdoll is being activated, because we need at least one damage event to go through in order to trigger properly
			const bool bPedRagdollBeingActivated = pPhysical->GetIsTypePed() ? (((CPed*)pPhysical)->GetRagdollState() == RAGDOLL_STATE_PHYS_ACTIVATE) : false;

			// BUG - url:bugstar:6430976 - This function can loop through multiple times (due to physics time slicing), and I don't think the ragdoll state is properly updated until we hit an AI update
			// We're not 100% sure why this check exists (it's been there since IV), doesn't make sense, so we're going to bypass on certain CNC explosion types to stop damage applying twice on the ragdoll activation frame 
			const bool bEnforceDamagedEntityList = pExplosionInst->ShouldEnforceDamagedEntityList();

			if (bEnforceDamagedEntityList || !bPedRagdollBeingActivated)
			{
				return;
			}
		}
	}

    if (NetworkInterface::IsGameInProgress() && pPhysical && !pPhysical->IsNetworkClone())
    {
        NetworkInterface::OnExplosionImpact(pPhysical, fForceMag.Getf());
    }

	float fDamage = 0.0f;

	if( pExplosionInst && pExplosionInst->ShouldApplyDamage() )
	{
		bool bIsTypePed = (pExplosionOwner && pExplosionOwner->GetIsTypePed());
		fDamage = pExplosionInst->CalculateExplosionDamage(vHitPos, bIsTypePed);

		if (bIsTypePed)
		{
			CPed* pPedOwner = static_cast<CPed*>(pExplosionOwner);
			if (pPedOwner->IsPlayer())
			{
				//MP Note: removed the fDamage clear out here - allow explosive damage to player in passive mode from themselves - damage from others is prevented elsewhere.
				//Passive mode is intended to prevent damage from others undo you, not from preventing you from doing damage to yourself.  You can still blow yourself up, fall to your death or be hit
				//by another vehicle.  Passive mode just prevents damage from others.  lavalley

				const CPedModelInfo* mi = pPedOwner->GetPedModelInfo();
				Assert(mi);
				const CPedModelInfo::PersonalityData& pd = mi->GetPersonalitySettings();

				fDamage *= pd.GetExplosiveDamageMod();
			}
		}
	}

	// If "CPED_RESET_FLAG_DontDieOnVehicleWrecked" is set on the ped, apply normal explosive damage to the ped (instead of instantly killing it in CAutomobile::BlowUpCar -> KillPedsInVehicle).
	bool bApplyDamageToPedInsideVehicle = false;
	float fDamageBeforeVehicleDamageCalculated = fDamage;

	const CExplosionTagData& explosionTagData = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(pExplosionInst->GetExplosionTag());

	// Separate bits for vehicles and peds to do damage. (Also to make cars use physics)
	if(pPhysical->GetIsTypeVehicle())
	{
		CVehicle* pVehicle = (CVehicle*)pPhysical;

		for (s32 seat = 0; seat < MAX_VEHICLE_SEATS; seat++)
		{
			if (pVehicle->IsSeatIndexValid(seat))
			{
				CPed* pPed = CTaskVehicleFSM::GetPedInOrUsingSeat(*pVehicle, seat);
				if (pPed && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_UseNormalExplosionDamageWhenBlownUpInVehicle))
				{
					bApplyDamageToPedInsideVehicle = true;
				}

				const CVehicleSeatAnimInfo* pSeatAnimInfo = pVehicle->GetSeatAnimationInfo(seat);
				if (pSeatAnimInfo && (pSeatAnimInfo->GetKeepCollisionOnWhenInVehicle() || pSeatAnimInfo->IsStandingTurretSeat()))
				{
					bApplyDamageToPedInsideVehicle = true;
				}
			}
		}

		//Avoid explosion reaction and damage if in a network game and vehicle is driven by a passive player
		if (NetworkInterface::IsGameInProgress())
		{
			if (NetworkUtils::IsNetworkCloneOrMigrating(pVehicle))
				return;

			if (!IsFriendlyFireAllowed(pVehicle, pExplosionInst))						
				return;			

			if (!pVehicle->GetVehicleDamage()->CanVehicleBeDamagedBasedOnDriverInvincibility())
				return;
		}

		bool bApplyExplosionDamageToVehicle = pExplosionInst->ShouldDamageVehicles();

		if( NetworkInterface::IsGameInProgress() &&

			pExplosionInst &&
			bApplyExplosionDamageToVehicle &&
			pExplosionInst->GetAttachedToVehicle() &&
			((CEntity*)CGameWorld::FindLocalPlayer() != pExplosionOwner ||  pExplosionInst->GetDetonatingOtherPlayersExplosive() ) )
		{		
			Assert(!pPhysical->IsNetworkClone());
			pVehicle->TestExplosionPosAndBlowRearDoorsopen(VEC3V_TO_VECTOR3(vExplosionPosition));
		}

		if(bApplyExplosionDamageToVehicle)
		{
			const Vector3 vVehiclePosition = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());

			Vec3V vExpPosWld = pExplosionInst->GetPosWld();
			
			// This need to be distance from the surface of the vehicle, the center could be pretty arbitrarily placed and some vehicles 
			// are quite large such that you can have a direct impact on the side and still have the explosion center 5+ meters away from the vehicle center
			float fDistanceFromExplosionSq;
			if(pVehicle->InheritsFromPlane())
			{
				// Airplanes get the crappy treatment for distance calculation (it'd be really wrong with false positives in the space created by their wingspan)
				// but later get special treatment to always blow up anyway
				fDistanceFromExplosionSq = RCC_VECTOR3(vExpPosWld).Dist2(vVehiclePosition);
			}
			else
			{
				// For most things we're going with the approximation of the explosion center vs the OBB of the vehicle
				Vec3V vExpPosLocal, vHalfWidth, vCenter;

				// Because of doors and other such movable things we'd really prefer to use the pristine bounding box because it's more representative of the core shape of the vehicle
				const fragPhysicsLOD* pFragPhysLOD = pVehicle->GetVehicleFragInst()->GetTypePhysics();
				const phArchetype* pArch = pFragPhysLOD ? pFragPhysLOD->GetArchetype() : NULL;
				const phBound* pBound = pArch ? pArch->GetBound() : NULL;
				if(pBound)
				{
					pBound->GetBoundingBoxHalfWidthAndCenter( vHalfWidth, vCenter );
				}
				else
				{
					// But...if we have to we'll fall back on the current bounding box
					spdAABB vehLocalBox;
					const spdAABB& boundBox = pVehicle->GetLocalSpaceBoundBox(vehLocalBox);
					vHalfWidth = boundBox.GetExtent();
					vCenter = boundBox.GetCenter();
				}

				Mat34V temp = pVehicle->GetMatrix();
				Mat34V boxMatrix( temp );
				boxMatrix.SetCol3( rage::Add(boxMatrix.GetCol3(), vCenter) );
				vExpPosLocal = UnTransformOrtho(boxMatrix, vExpPosWld);

				Vec3V pointOnBox, normalOnBox;
				ScalarV depthIntoBox;
				geomPoints::FindClosestPointPointBoxSurface(vExpPosLocal, vHalfWidth, pointOnBox, normalOnBox, depthIntoBox);
				// !!! Note that the other answers (point and normal on box) are in local space here and would need transforming back out to be correct in world space

				// We're arbitrarily subtracting 0.8 meters to give a bit more fidelity to the values as the explosion gets into the vehicle box
				// while adjusting to match the expectations that INSTANT_EXPLOSION_DISTANCE and DELAYED_EXPLOSION_DISTANCE were built on (presumably around the tailgater or something like that)
				const ScalarV arbitraryDepthReduction(0.8f);
				depthIntoBox = Subtract(depthIntoBox, arbitraryDepthReduction);
				if( IsGreaterThanOrEqualAll(depthIntoBox, ScalarV(V_ZERO)) != 0 )
				{
					fDistanceFromExplosionSq = 0.0f;
				}
				else
				{
					fDistanceFromExplosionSq = Scale(depthIntoBox, depthIntoBox).Getf();
				}
			}

			CVehicleModelInfo* pVehicleModelInfo = pVehicle->GetVehicleModelInfo();
			if( pExplosionInst->GetExplosionTag() == EXP_TAG_STICKYBOMB && pVehicleModelInfo && pVehicleModelInfo->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_DAMPEN_STICKBOMB_DAMAGE ) )
			{
				fDamage *= STICKYBOMB_EXPLOSION_DAMPEN_DAMAGE_VEHICLE_MODIFIER;
			}
			else
			{
				fDamage *= EXPLOSION_DAMAGE_VEHICLE_MODIFIER;
			}

			// B*1986502: Allow vehicles with a weapon damage modifier higher than 1.0 to instantly explode from close explosions even if the raw explosion damage is lower than the threshold.
			// Mainly to allow electric cars to explode when the rail gun is shot underneath them. They don't do petrol tank damage processing (Which also causes explosions) so they don't respond like other cars.
			// Keeping it to only affecting damage multipliers > 1 to limit the scope and to prevent vehicles with damage multpliers < 1 from becoming too difficult to blow up.
			float fInstantExplodeDamage = fDamage * (pVehicle->pHandling->m_fWeaponDamageMult > 1.0f ? pVehicle->pHandling->m_fWeaponDamageMult : 1.0f);

			bool bIsChainExplosion = false;
			bool avoidExplosions = false;
			// Reduce the effect of explosions on vehicles
			if( pExplodingEntity && pExplodingEntity->GetIsTypeVehicle() )
			{
				fDamage *= EXPLOSION_FORCE_VEHICLE_CHAIN_EXPLOSION_REDUCTION;
				//If the vehicle is exploding and set to wreck from the delayed explosion, do so here
				if(!CVehicle::ms_bWreckVehicleOnFirstExplosion && pExplodingEntity == pVehicle && pVehicle->GetStatus() != STATUS_WRECKED)
				{
					pVehicle->BlowUpCar(pExplosionOwner, false , false);
				}

				if(pExplodingEntity != pVehicle)
				{
					// If we're trying to avoid chain reaction explosions don't let this vehicle explosion damage create
					//   explosions on the other vehicle
					avoidExplosions = CVehicleDamage::AvoidVehicleExplosionChainReactions();

					bIsChainExplosion = true;
				}
			}
			else
			{
				bool bShouldBlowUpVehicle = false;

				//Force the vehicle to explode if a sticky bomb is attached to a passenger inside
				const CPed* pAttachPed = (pExplosionInst->GetAttachEntity() && pExplosionInst->GetAttachEntity()->GetIsTypePed()) ?
					static_cast<CPed *>(pExplosionInst->GetAttachEntity()) : NULL;
				bool bStickyBombAttachedToPassenger = (pVehicle && (pExplosionInst->GetExplosionTag() == EXP_TAG_STICKYBOMB) && pAttachPed && (pAttachPed->GetVehiclePedInside() == pVehicle));

				//Force the vehicle to explode if flagged on the explosion type
				CExplosionTagData& explosionTagData = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(pExplosionInst->GetExplosionTag());
				bool bExplosionTypeForcesVehicleExplosion = explosionTagData.bForceVehicleExplosion;

				// Forced cases
				if(bStickyBombAttachedToPassenger || bExplosionTypeForcesVehicleExplosion)
				{
					bShouldBlowUpVehicle = true;
				}
				// If hit at a very close distance to the explosion epicenter with a high amount of damage, explode this vehicle too (except certain vehicles / explosion types)
				else if( (fDistanceFromExplosionSq < rage::square(INSTANT_EXPLOSION_DISTANCE) || pVehicle->InheritsFromPlane()) && 
					fInstantExplodeDamage > 600.0f && pExplosionInst->GetDamageType() != DAMAGE_TYPE_FIRE &&
					pVehicle->m_nVehicleFlags.bExplodesOnHighExplosionDamage &&
					pVehicle->GetVehicleModelInfo() && !pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_CAPPED_EXPLOSION_DAMAGE) && 
					(!pVehicle->pHandling || (pVehicle->pHandling->hFlags&HF_HEAVYARMOUR)==0 || pVehicle->GetHealth() <= 0.0f) &&
					pVehicle->GetVehicleType() != VEHICLE_TYPE_SUBMARINECAR &&
					pExplosionInst->GetExplosionTag() != EXP_TAG_VALKYRIE_CANNON &&
					pExplosionInst->GetExplosionTag() != EXP_TAG_BOMBUSHKA_CANNON)
				{
					bShouldBlowUpVehicle = true;
				}

				if (!pVehicle->CanPhysicalBeDamaged(pExplosionOwner, weaponHash))
				{
					bShouldBlowUpVehicle = false;
				}

				if (bShouldBlowUpVehicle)
				{
					bool bAddDelayedExplosion = false;
					if(fDistanceFromExplosionSq < rage::square(DELAYED_EXPLOSION_DISTANCE))
					{
						const CItemInfo* pItemInfo = CWeaponInfoManager::GetInfo(weaponHash);
						if(pItemInfo && pItemInfo->GetIsClassId(CWeaponInfo::GetStaticClassId()))
						{
							const CWeaponInfo* pWeaponInfo = static_cast<const CWeaponInfo*>(pItemInfo);
							eWeaponEffectGroup eEffectGroup = pWeaponInfo->GetEffectGroup();
							bAddDelayedExplosion = eEffectGroup == WEAPON_EFFECT_GROUP_GRENADE || eEffectGroup == WEAPON_EFFECT_GROUP_ROCKET;
						}
					}

					pVehicle->BlowUpCar(pExplosionOwner, false, true, false, weaponHash, bAddDelayedExplosion);

					if(pVehicle->GetVehicleType() == VEHICLE_TYPE_BICYCLE)
					{
						g_audExplosionEntity.PlayExplosion(pExplosionInst, EXP_TAG_DONTCARE, VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition()), pVehicle, false);
					}
					else
					{
						g_audExplosionEntity.PlayExplosion(pExplosionInst, EXP_TAG_CAR, VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition()), pVehicle, false);
					}
				}
			}
			
			pVehicle->GetVehicleDamage()->ApplyDamage(pExplosionOwner, damageType, weaponHash, fDamage, VEC3V_TO_VECTOR3(vExplosionPosition), VEC3V_TO_VECTOR3(-vForceDir), VEC3V_TO_VECTOR3(vForceDir), iComponent, 0, -1, false, true, fExplosionRadius, avoidExplosions, bIsChainExplosion);
			pVehicle->GetVehicleDamage()->GetDeformation()->ApplyCollisionImpact(VEC3V_TO_VECTOR3(Scale(vForceDir, fForceMag)), VEC3V_TO_VECTOR3(vHitPos), pExplosionOwner, pVehicle->InheritsFromPlane());

			if(pExplosionInst->CanReportCrime())
			{
				//Grab the ped that owns the explosion.
				CPed* pExplosionOwnerPed = NULL;
				if(pExplosionOwner && pExplosionOwner->GetIsTypePed())
				{
					pExplosionOwnerPed = static_cast<CPed*>(pExplosionOwner);
				}

				//Check if the explosion owner is a ped.
				if(pExplosionOwnerPed)
				{
					//Report the crime.
					if(bIsChainExplosion)
					{
						CCrime::ReportCrime(CRIME_CHAIN_EXPLOSION, pPhysical, pExplosionOwnerPed);
					}
					else
					{
						CCrime::ReportCrime(pExplosionInst->GetDamageType() == DAMAGE_TYPE_SMOKE ? CRIME_TERRORIST_ACTIVITY : CRIME_VEHICLE_EXPLOSION, pPhysical, pExplosionOwnerPed);
					}
				}
			}

			// Make sure the vehicle has all the physics information required
			Assert( pPhysical->GetCurrentPhysicsInst() );
			Assert( pPhysical->GetCurrentPhysicsInst()->GetArchetype() );
			Assert( pPhysical->GetCurrentPhysicsInst()->GetArchetype()->GetBound() );
		}

		// Apply any special effects to vehicles that explosions can apply

		if (explosionTagData.bApplyVehicleEMP)
		{
			pVehicle->ApplyExplosionEffectEMP(weaponHash);
		}

		if (explosionTagData.bApplyVehicleSlick)
		{
			pVehicle->ApplyExplosionEffectSlick();
		}

		if (explosionTagData.bApplyVehicleSlowdown)
		{
			pVehicle->ApplyExplosionEffectSlowdown( VEC3V_TO_VECTOR3( vExplosionPosition ), fExplosionRadius );
		}

		if (explosionTagData.bApplyVehicleTyrePop)
		{
			TUNE_GROUP_BOOL(VEHICLE_EXPLOSION_EFFECTS, TYREPOP_DEBUG_RENDERING, false);

			// Rim the nearest tyre to the explosion position
			// or affect all tyres if bApplyVehicleTyrePopToAllTyres is set
			int iNearestWheelIndex = -1;
			float fNearestWheelDistSq = LARGE_FLOAT;

			if (pVehicle->GetNumWheels() > 0)
			{
				CWheel* const * wheels = pVehicle->GetWheels();
				for (int i = 0; i < pVehicle->GetNumWheels(); i++)
				{
					Vector3 wheelPos;
					wheels[i]->GetWheelPosAndRadius(wheelPos);
					Vector3 testPos = VEC3V_TO_VECTOR3(vExplosionPosition);
					float fDistSq = wheelPos.Dist2(testPos);
#if __BANK
					if (TYREPOP_DEBUG_RENDERING)
					{
						grcDebugDraw::Line(testPos, wheelPos, Color_green, Color_green, 100);
					}
#endif // __BANK
					if (explosionTagData.bApplyVehicleTyrePopToAllTyres && wheels[i]->GetTyreHealth() > 0.0)
					{
						TriggerTyrePopOnWheel(wheels[i] BANK_ONLY(, TYREPOP_DEBUG_RENDERING));
					}
					else 
					{
						if (fDistSq < fNearestWheelDistSq)
						{
							iNearestWheelIndex = i;
							fNearestWheelDistSq = fDistSq;
						}
					}
				}
				if (!explosionTagData.bApplyVehicleTyrePopToAllTyres)
				{
					// Don't do this if wheel already on rim
					if (iNearestWheelIndex != -1 && wheels[iNearestWheelIndex]->GetTyreHealth() > 0.0)
					{
						TriggerTyrePopOnWheel(wheels[iNearestWheelIndex] BANK_ONLY(, TYREPOP_DEBUG_RENDERING));
					}
				}
			}
		}
	}

	if (pPhysical->GetIsTypePed() || (pPhysical->GetIsTypeVehicle() && bApplyDamageToPedInsideVehicle))
	{
		//Grab the ped.
		CPed* pPed = NULL;

		if (bApplyDamageToPedInsideVehicle)
		{
			CVehicle* pVehicle = (CVehicle*)pPhysical;
			for (s32 seat = 0; seat < MAX_VEHICLE_SEATS; seat++)
			{
				if (pVehicle->IsSeatIndexValid(seat))
				{
					pPed = CTaskVehicleFSM::GetPedInOrUsingSeat(*pVehicle, seat);
					if (pPed)
					{
						bool bApplyDamageToPed = false;
						if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_UseNormalExplosionDamageWhenBlownUpInVehicle))
						{
							bApplyDamageToPed = true;
						}

						const CVehicleSeatAnimInfo* pSeatAnimInfo = pVehicle->GetSeatAnimationInfo(seat);
						if (pSeatAnimInfo && (pSeatAnimInfo->GetKeepCollisionOnWhenInVehicle() || pSeatAnimInfo->IsStandingTurretSeat()))
						{
							bApplyDamageToPed = true;
						}

						if (bApplyDamageToPed && !pExplosionInst->IsEntityAlreadyDamaged(pPed))
						{
							ApplyPedDamage(pInst, pPed, pEntity, pExplosionOwner, pExplosionInst, fDamageBeforeVehicleDamageCalculated, fForceMag, vForceDir, vHitPos, vExplosionPosition, 0, bEntityAlreadyDamaged, weaponHash);
							pExplosionInst->AddDamagedEntity(pPed);
						}
					}
				}
			}
		}
		else
		{
			pPed = static_cast<CPed *>(pPhysical);
			ApplyPedDamage(pInst, pPed, pEntity, pExplosionOwner, pExplosionInst, fDamage, fForceMag, vForceDir, vHitPos, vExplosionPosition, iComponent, bEntityAlreadyDamaged, weaponHash);
		}
	}
	else if (pPhysical->GetIsTypeObject() && !NetworkUtils::IsNetworkCloneOrMigrating(pPhysical) && pExplosionInst->ShouldDamageObjects())
	{
		fDamage *= EXPLOSION_DAMAGE_OBJECT_MODIFIER;
		((CObject *)pPhysical)->ObjectDamage(fDamage, NULL, NULL, pExplosionOwner, weaponHash);
	}

	if (explosionTagData.bApplyFlashGrenadeEffect)
	{
		if (pPhysical->GetIsTypePed())
		{
			CPed* pPed = static_cast<CPed*>(pPhysical);
			ProcessFlashGrenadeEffect(pPed, pPed->GetIsInVehicle());
		}
		else if (pPhysical->GetIsTypeVehicle())
		{
			CVehicle* pVehicle = static_cast<CVehicle*>(pPhysical);
			for (s32 seat = 0; seat < MAX_VEHICLE_SEATS; seat++)
			{
				if (pVehicle->IsSeatIndexValid(seat))
				{
					CPed* pPed = CTaskVehicleFSM::GetPedInOrUsingSeat(*pVehicle, seat);
					if (pPed)
					{
						ProcessFlashGrenadeEffect(pPed, true);
					}
				}
			}
		}
	}
}

void CExplosionManager::ApplyPedDamage(phInst *pInst,
									   CPed *pPed, 
									   CEntity *pEntity, 
									   CEntity *pExplosionOwner, 
									   phGtaExplosionInst* pExplosionInst, 
									   float fDamage, 
									   ScalarV_In fForceMag, 
									   Vec3V_In vForceDir,
									   Vec3V_In vHitPos,
									   Vec3V_In vExplosionPosition, 
									   int iComponent, 
									   bool bEntityAlreadyDamaged,
									   u32 weaponHash)
{
	if (!Verifyf(pPed, "CExplosionManager::ApplyPedDamage - Invalid ped pointer"))
	{
		return;
	}

	//Avoid explosion reaction and damage if in a network game and ped is a passive player or the explosion owner is passive
	if (NetworkInterface::IsGameInProgress())
	{
		if (!IsFriendlyFireAllowed(pPed, pExplosionInst))		
			return;			
	}

	// B*2421513: Script-defined explosive damage modifier and clamp for player (ignored for orbital cannon)
	if (pPed && pPed->IsPlayer() && pPed->GetPlayerInfo() && pExplosionInst->GetExplosionTag() != EXP_TAG_ORBITAL_CANNON)
	{
		fDamage *= pPed->GetPlayerInfo()->GetPlayerExplosiveDamageModifier();
		float fMaxExplosiveDamage = pPed->GetPlayerInfo()->GetPlayerMaxExplosiveDamage();
		if (fMaxExplosiveDamage != -1.0f)
		{
			fDamage = rage::Clamp(fDamage, 0.0f, fMaxExplosiveDamage);
		}
	}

	//Apply the damage modifier.
	fDamage *= EXPLOSION_DAMAGE_PED_MODIFIER;

	//B*1815582: Increase smoke grenade damage in MP
	if(pExplosionInst->GetExplosionTag() == EXP_TAG_SMOKEGRENADE)
	{
		if (NetworkInterface::IsGameInProgress())
		{
			static dev_float MP_SMOKE_DAMAGE_MODIFIER = 2;
			fDamage *= MP_SMOKE_DAMAGE_MODIFIER;
		}

		//B*2121190: Don't apply smoke grenade damage to law enforcement peds if it was originally thrown by a law enforcement ped.
		bool bOwnerIsPed = (pExplosionOwner && pExplosionOwner->GetIsTypePed());
		bool bHitPed = (pEntity && pEntity->GetIsTypePed());
		if (bOwnerIsPed && bHitPed)
		{
			CPed* pPedOwner = static_cast<CPed*>(pExplosionOwner);
			CPed *pHitPed = static_cast<CPed*>(pEntity);
			if (pPedOwner && pHitPed)
			{
				if (pPedOwner->IsLawEnforcementPed() && pHitPed->IsLawEnforcementPed())
				{
					fDamage = 0.0f;
					return;
				}
			}		
		}
	}

	// Sever-side overrides for explosions
	if(pExplosionInst->GetExplosionTag() == EXP_TAG_EXPLOSIVEAMMO) // WEAPON_HEAVYSNIPER_MK2
	{	
		if (pPed->IsPlayer())
		{
			fDamage *= sm_fSpecialAmmoExplosiveSniperPlayerDamageMultiplier;
		}
		else
		{
			bool bCloneDamage = NetworkUtils::IsNetworkClone(pPed) || NetworkUtils::IsNetworkClone(pExplosionOwner);
			fDamage *= bCloneDamage ? sm_fSpecialAmmoExplosiveSniperPedRemoteDamageMultiplier : sm_fSpecialAmmoExplosiveSniperPedLocalDamageMultiplier;
		}
	}
	else if(pExplosionInst->GetExplosionTag() == EXP_TAG_EXPLOSIVEAMMO_SHOTGUN) // WEAPON_PUMPSHOTGUN_MK2
	{
		if (pPed->IsPlayer())
		{
			fDamage *= sm_fSpecialAmmoExplosiveShotgunPlayerDamageMultiplier;
		}
		else
		{
			bool bCloneDamage = NetworkUtils::IsNetworkClone(pPed) || NetworkUtils::IsNetworkClone(pExplosionOwner);
			fDamage *= bCloneDamage ? sm_fSpecialAmmoExplosiveShotgunPedRemoteDamageMultiplier : sm_fSpecialAmmoExplosiveShotgunPedLocalDamageMultiplier;
		}
	}
	else if(pExplosionInst->GetExplosionTag() == EXP_TAG_FLASHGRENADE)
	{
		if (pPed->GetIsInVehicle())
		{
			fDamage *= sm_fFlashGrenadeVehicleOccupantDamageMultiplier;
		}
	}
	else if(pExplosionInst->GetExplosionTag() == EXP_TAG_STUNGRENADE)
	{
		if (pPed->GetIsInVehicle())
		{
			fDamage *= sm_fStunGrenadeVehicleOccupantDamageMultiplier;
		}
	}

	// If the ped has very recently been hit by the Kinetic Ram ability by the same inflicter, throw away any additional damage application
	if (pExplosionInst->GetExplosionTag() == EXP_TAG_CNC_KINETICRAM)
	{
		const int iCount = pPed->GetWeaponDamageInfoCount();
		for (int i = 0; i < iCount; i++)
		{
			const CPhysical::WeaponDamageInfo& rDamageInfo = pPed->GetWeaponDamageInfo(i);

			// We have no way of 100% identifying Kinetic Ram right now, but this should be close enough
			if (rDamageInfo.pWeaponDamageEntity == pExplosionOwner && rDamageInfo.weaponDamageHash == WEAPONTYPE_EXPLOSION && fwTimer::GetTimeInMilliseconds() - rDamageInfo.weaponDamageTime < 1000 )
			{
				fDamage = 0.0f;
				return;
			}
		}
	}

	// Grab the total size for later
	CExplosionTagData& expTagData = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(pExplosionInst->GetExplosionTag());
	float endRadius = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionEndRadius(expTagData);
	const ScalarV expEndRadius = Scale( ScalarVFromF32(endRadius), ScalarVFromF32(pExplosionInst->GetSizeScale()) );

	//Grab the ped that owns the explosion.
	CPed* pExplosionOwnerPed = NULL;
	if(pExplosionOwner && pExplosionOwner->GetIsTypePed())
	{
		pExplosionOwnerPed = static_cast<CPed*>(pExplosionOwner);
	}

	//Check if the explosion owner is a ped.
	if(pExplosionOwnerPed && pExplosionInst->CanReportCrime())
	{
		//Report the crime.
		CCrime::ReportCrime(pExplosionInst->GetDamageType() == DAMAGE_TYPE_SMOKE ? CRIME_TERRORIST_ACTIVITY : CRIME_CAUSE_EXPLOSION, pPed, pExplosionOwnerPed);
	}

	if(pExplosionInst->GetDamageType() == DAMAGE_TYPE_FIRE || pExplosionInst->GetExplosionTag() == EXP_TAG_DIR_FLAME)
	{
		if(pExplosionInst->GetUnderwaterDepth() <= MAX_WATER_DEPTH_TO_TRIGGER_FIRE && CFireManager::CanSetPedOnFire(pPed) && !pPed->IsNetworkClone())
		{
			g_fireMan.StartPedFire(pPed, pExplosionOwnerPed, FIRE_DEFAULT_NUM_GENERATIONS, vExplosionPosition, BANK_ONLY(vExplosionPosition,) false, false, pExplosionInst->GetWeaponHash(), 0.0f, false, pExplosionInst->CanReportCrime());
			CEventOnFire event(pExplosionInst->GetWeaponHash());
			pPed->GetPedIntelligence()->AddEvent(event);
		}
	}
	else
	{
		fwFlags32 damageFlags = pExplosionInst->CanReportCrime() ? CPedDamageCalculator::DF_None : CPedDamageCalculator::DF_DontReportCrimes;
		if (CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(pExplosionInst->GetExplosionTag()).bDealsEnduranceDamage)
		{
			damageFlags.SetFlag(CPedDamageCalculator::DF_EnduranceDamageOnly);
		}

		if (NetworkUtils::IsNetworkCloneOrMigrating(pPed))
		{
			if (pExplosionOwner &&
				!bEntityAlreadyDamaged && 
				fDamage > 0.0f && 
				pExplosionInst->GetDamageType() != DAMAGE_TYPE_SMOKE && 
				!NetworkUtils::IsNetworkClone(pExplosionOwner) &&
				NetworkUtils::GetNetworkObjectFromEntity(pExplosionOwner) &&
				// Don't trigger network damage events for Endurance-only explosions as they are already processed by the victim
				!damageFlags.IsFlagSet(CPedDamageCalculator::DF_EnduranceDamageOnly))
			{
				u32 weaponHash = pExplosionInst->GetWeaponHash();
				if (0 == weaponHash)
					weaponHash = WEAPONTYPE_EXPLOSION;

				CWeaponDamageEvent::Trigger(pExplosionOwner, pPed, VEC3V_TO_VECTOR3(vExplosionPosition), 0, true, weaponHash, fDamage, -1, -1, damageFlags, 0, 0, 0);
			}
		}
		else
		{
			// We want grenades and rockets to set peds on fire too.
			const bool bSetPedOnFire = !pPed->IsPlayer() && CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(pExplosionInst->GetExplosionTag()).bCanSetPedOnFire;
			const bool bSetPlayerOnFire = pPed->IsPlayer() && CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(pExplosionInst->GetExplosionTag()).bCanSetPlayerOnFire;
			if((bSetPedOnFire || bSetPlayerOnFire) && CFireManager::CanSetPedOnFire(pPed))
			{
				const bool bIgnoreRatioCheckForFire = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(pExplosionInst->GetExplosionTag()).bIgnoreRatioCheckForFire;
				const ScalarV maxExpDistForFire = Scale(expEndRadius, RATIO_OF_EXPLOSION_THAT_TRIGGERS_FIRE);
				const ScalarV maxExpDistForFireSqrd = Scale(maxExpDistForFire, maxExpDistForFire);
				const Vec3V expToPed = Subtract(pPed->GetMatrix().GetCol3(), pExplosionInst->GetPosWld());
				if((bIgnoreRatioCheckForFire || IsLessThanOrEqualAll(Dot(expToPed, expToPed), maxExpDistForFireSqrd) != 0) && pExplosionInst->GetUnderwaterDepth() <= MAX_WATER_DEPTH_TO_TRIGGER_FIRE )
				{
					g_fireMan.StartPedFire(pPed, pExplosionOwnerPed, FIRE_DEFAULT_NUM_GENERATIONS, vExplosionPosition, BANK_ONLY(vExplosionPosition,) false, true, pExplosionInst->GetWeaponHash(), 0.0f, false, pExplosionInst->CanReportCrime());
					CEventOnFire event(pExplosionInst->GetWeaponHash());
					pPed->GetPedIntelligence()->AddEvent(event);
				}
			}

			// If damage is smoke
			if( pExplosionInst->GetDamageType() == DAMAGE_TYPE_SMOKE )
			{
				// If nonzero damage
				if( !bEntityAlreadyDamaged && fDamage > 0.0f )
				{
					// If the ped hasn't had lungs damaged yet, and is hurt, but not writhing
					if( !pPed->IsBodyPartDamaged(CPed::DAMAGED_LUNGS) && pPed->HasHurtStarted() && !pPed->GetPedResetFlag(CPED_RESET_FLAG_IsInWrithe) )
					{
						// Set the ped to end the hurt phase immediately and go to writhing
						pPed->SetHurtEndTime(fwTimer::GetTimeInMilliseconds());
					}

					// Report damage to the lungs
					pPed->ReportBodyPartDamage(CPed::DAMAGED_LUNGS);

					// If ped has no weapon damage entity, set the explosion ped as damager if possible
					// Otherwise the check in CPed::CanBeInHurt will prevent hurt behavior
					if( pPed->GetWeaponDamageEntity() == NULL && pExplosionOwnerPed )
					{
						pPed->SetWeaponDamageInfo(pExplosionOwnerPed, 0, fwTimer::GetTimeInMilliseconds());
					}
				}
			}
		}

		WorldProbe::CShapeTestHitPoint intersection;
		intersection.SetHitInst(pInst->GetLevelIndex(), PHLEVEL->GetGenerationID(pInst->GetLevelIndex()));
		intersection.SetHitComponent((u16)iComponent);
		intersection.SetHitPosition(RCC_VECTOR3(vHitPos));
		intersection.SetHitNormal(VEC3V_TO_VECTOR3(Negate(vForceDir)));
		CWeaponDamage::GeneratePedDamageEvent(pExplosionOwner, pPed, weaponHash, bEntityAlreadyDamaged ? 0.0f : fDamage, VEC3V_TO_VECTOR3(vExplosionPosition), &intersection, damageFlags, NULL, iComponent, fForceMag.Getf());

		// Register event
		if(pExplosionOwnerPed)
		{
			// Before the ped dies, tell its friends that it has been attacked so they can react.
			CEventInjuredCryForHelp ev(pPed, pExplosionOwnerPed);
			if(!pExplosionInst->CanReportCrime())
			{
				ev.SetIgnoreIfLaw(true);
			}
			ev.CEvent::CommunicateEvent(pPed, false, false);
			//((CPed *)pPhysical)->RegisterThreatWithGangPeds(pExplosionOwner);
		}
	}

	if( pPed->IsLocalPlayer() &&
		pExplosionInst->GetExplosionTag() == EXP_TAG_DIR_WATER_HYDRANT )
	{
		dev_u32 snRumbleDuration = 500;
		dev_float sfRumbleIntensity = 0.5f; // Max intensity.
		CControlMgr::StartPlayerPadShakeByIntensity( snRumbleDuration, sfRumbleIntensity );
	}
}

//-------------------------------------------------------------------------
// Creates an explosion of the given type
//-------------------------------------------------------------------------
phGtaExplosionInst* CExplosionManager::CreateExplosionOfType( CEntity *pExplodingEntity, CEntity *pEntExplosionOwner, CEntity *pEntIgnoreDamage, eExplosionTag ExplosionTag, float sizeScale,
															 const Vector3& vecExplosionPosition, u32 camShakeNameHash, float fCamShake, float fCamShakeRollOffScaling, bool bMakeSound, bool noFx, bool inAir, const Vector3& vDirection, 
															 CEntity* pAttachEntity, s32 attachBoneTag, u32 vfxTagHash, CNetFXIdentifier& networkIdentifier, bool bNoDamage, bool bAttachedToVehicle, bool bDetonatingOtherPlayersExplosive, u32 weaponHash, bool bDisableDamagingOwner, fwInteriorLocation intLoc)
{
	if (phGtaExplosionInst::GetPool()->GetNoOfFreeSpaces()==0)
	{
		return NULL;
	}

	return rage_new phGtaExplosionInst(ExplosionTag, pEntExplosionOwner, pExplodingEntity, pEntIgnoreDamage, pAttachEntity, attachBoneTag, vecExplosionPosition, vDirection, sizeScale, camShakeNameHash, fCamShake, fCamShakeRollOffScaling, bMakeSound, noFx, inAir, vfxTagHash, networkIdentifier, bNoDamage, bAttachedToVehicle, bDetonatingOtherPlayersExplosive, weaponHash, bDisableDamagingOwner, intLoc);
}


//-------------------------------------------------------------------------
// GetPhysicalType
//-------------------------------------------------------------------------

ePhysicalType CExplosionManager::GetPhysicalType(eExplosionTag expTag)
{
	if (CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(expTag).directedWidth>0.0f)
	{
		return PT_DIRECTED;
	}
	else
	{
		return PT_SPHERE;
	}
}

bool CExplosionManager::IsTimedExplosionInProgress(const CEntity* pEntity)
{
	aiAssert(pEntity);

	for(int ExplosionIndex = 0; ExplosionIndex < MAX_EXPLOSIONS; ++ExplosionIndex)
	{
		phGtaExplosionInst* pCurrExplosionInst = m_apExplosionInsts[ExplosionIndex];
		if( pCurrExplosionInst && (pEntity == pCurrExplosionInst->GetAttachEntity()))
		{
			if(CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(pCurrExplosionInst->GetExplosionTag()).explodeAttachEntityWhenFinished)
			{
				return true;
			}
		}
	}

	return false;
}

//-------------------------------------------------------------------------
// Flash Grenade Effects
//-------------------------------------------------------------------------

void CExplosionManager::ProcessFlashGrenadeEffect(const CPed* pPed, const bool bVehicleOccupant)
{
	if (bVehicleOccupant) // TODO: Exception for peds in non-interior seats / hanging from vehicles
	{
		if (pPed->IsLocalPlayer())
		{
			CExplosionManager::eFlashGrenadeStrength effectStrength = CalculateFlashGrenadeEffectStrengthInVehicle(pPed);
			TriggerFlashGrenadeEffectOnLocalPlayer(effectStrength);
		}
	}
	else // On-Foot Ped / Outside Vehicle Seat
	{
		if (pPed->IsLocalPlayer())
		{
			CExplosionManager::eFlashGrenadeStrength effectStrength = CalculateFlashGrenadeEffectStrengthOnFoot(pPed);
			TriggerFlashGrenadeEffectOnLocalPlayer(effectStrength);
		}
		else if (!pPed->IsPlayer())
		{
			// TODO: Trigger some kind of threat response reaction / shellshock task
		}
	}
}

CExplosionManager::eFlashGrenadeStrength CExplosionManager::CalculateFlashGrenadeEffectStrengthOnFoot( const CPed* UNUSED_PARAM(pPed) )
{
	// TODO: Exceptions and cases where we don't want to apply an effect / downgrade / upgrade

	return eFlashGrenadeStrength::StrongEffect;
}

CExplosionManager::eFlashGrenadeStrength CExplosionManager::CalculateFlashGrenadeEffectStrengthInVehicle( const CPed* UNUSED_PARAM(pPed) )
{
	// TODO: Exceptions and cases where we don't want to apply an effect / downgrade / upgrade

	return eFlashGrenadeStrength::WeakEffect;
}

void CExplosionManager::TriggerFlashGrenadeEffectOnLocalPlayer(const eFlashGrenadeStrength effectStrength)
{
	u32 uStrongPostEffectHash = ATSTRINGHASH("cnc_flashgrenade_strong", 0xEF6E233B);	// Hold for 1s, ease out for 4s
	u32 uWeakPostEffectHash = ATSTRINGHASH("cnc_flashgrenade_weak", 0x87D3D9F9);		// Straight to ease out for 2s

	u32 uPostEffectHash = 0;
	u32 uPostEffectLength = 0; 

	// Set parameters based on strength  - Values defined at top of file, above InitTunables()
	switch (effectStrength)
	{
	case eFlashGrenadeStrength::StrongEffect:
		uPostEffectHash = uStrongPostEffectHash;
		uPostEffectLength = sm_uFlashGrenadeStrongEffectVFXLength;
		break;
	case eFlashGrenadeStrength::WeakEffect:
		uPostEffectHash = uWeakPostEffectHash;
		uPostEffectLength = sm_uFlashGrenadeWeakEffectVFXLength;
		break;
	default:
		break;
	}

#if __BANK
	// Override parameters
	TUNE_GROUP_BOOL(FLASH_GRENADE, USE_OVERRIDE_VALUES, false);
	TUNE_GROUP_BOOL(FLASH_GRENADE, OVERRIDE_POSTFX_ALWAYS_WEAK, false);
	TUNE_GROUP_INT(FLASH_GRENADE, OVERRIDE_POSTFX_DURATION, 5000, 0, 10000, 1);
	
	if (USE_OVERRIDE_VALUES)
	{
		uPostEffectHash = OVERRIDE_POSTFX_ALWAYS_WEAK ? uStrongPostEffectHash : uWeakPostEffectHash;
		uPostEffectLength = OVERRIDE_POSTFX_DURATION;
	}
#endif //__BANK

	// Safety clamps
	weaponAssertf(uPostEffectLength <= 10000, "Flash grenade effects set to more than 10 seconds, which seems bad. Clamping.");
	if (uPostEffectLength > 10000)
	{
		uPostEffectLength = 10000;
	}
	
	if (uPostEffectHash != 0)
	{
		ANIMPOSTFXMGR.Start(uPostEffectHash, uPostEffectLength, false, false, false, 0U, AnimPostFXManager::kDefault);

		g_FrontendAudioEntity.TriggerPlayerTinnitusEffect(effectStrength);
	}
}

//-------------------------------------------------------------------------
// Tyre Pop Behaviour
//-------------------------------------------------------------------------

void CExplosionManager::TriggerTyrePopOnWheel(CWheel* affectedWheel BANK_ONLY(, bool bRenderDebug))
{
	// If you don't call ApplyTyreDamage first, the tyre doesn't go down to rim properly...
	affectedWheel->ApplyTyreDamage(NULL, 1.0f, Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 0.0f), DAMAGE_TYPE_BULLET, 0);
	affectedWheel->DoBurstWheelToRim();
#if __BANK
	if (bRenderDebug)
	{
		Vector3 wheelPos;
		affectedWheel->GetWheelPosAndRadius(wheelPos);
		grcDebugDraw::Sphere(wheelPos, 0.25f, Color_red, true, 100);
	}
#endif
}

//----------------------------------------------------------------
// CExplosionTagData
//----------------------------------------------------------------

//default constructor to at least have dummy data on failure to find a given tag
CExplosionTagData::CExplosionTagData() :
name("EXP_TAG_GRENADE")
, damageAtCentre(0.0f)
, damageAtEdge(0.0f)
, networkPlayerModifier(0.0f)
, networkPedModifier(0.0f)
, endRadius(0.0f)
, initSpeed(0.0f)
, decayFactor(0.0f)
, forceFactor(0.0f)
, fRagdollForceModifier(0.0f)
, fSelfForceModifier(1.0f)
, directedWidth(0.0f)
, directedLifeTime(0.0f)
, camShake(0.0f)
, fragDamage(0.0f)
, shockingEventVisualRangeOverride(-1.0f)
, shockingEventAudioRangeOverride(-1.0f)
, vfxTagHashName("EXP_VFXTAG_GRENADE",0x8CBD7381)
, minorExplosion(true)
, bAppliesContinuousDamage(false)
, bPostProcessCollisionsWithNoForce(false)
, bDamageVehicles(true)
, bDamageObjects(true)
, bOnlyAffectsLivePeds(false)
, bIgnoreExplodingEntity(false)
, bNoOcclusion(false)
, explodeAttachEntityWhenFinished(false)
, bCanSetPedOnFire(false)
, bCanSetPlayerOnFire(false)
, bSuppressCrime(false)
, bUseDistanceDamageCalc(false)
, bPreventWaterExplosionVFX(false)
, bIgnoreRatioCheckForFire(false)
, bAllowUnderwaterExplosion(false)
, bForceVehicleExplosion(false)
, midRadius(-1.0f)
, damageAtMid(-1.0f)
, bForcePetrolTankDamage(false)
{

}

class CExplosionFileMounter : public CDataFileMountInterface
{
	virtual bool LoadDataFile(const CDataFileMgr::DataFile& file)
	{
		CExplosionInfoManager::Append(file.m_filename);
		return true;
	}
	virtual void UnloadDataFile(const CDataFileMgr::DataFile & /*file*/)
	{
	}
} g_explosionFileMounter;


//////////////////////////////////////////////////////////////////////////
// CExplosionInfoManager
//////////////////////////////////////////////////////////////////////////

// Static Manager object
CExplosionInfoManager CExplosionInfoManager::m_ExplosionInfoManagerInstance;

// Load the info from the parser file.
void CExplosionInfoManager::Init(const char* pFileName)
{
	CDataFileMount::RegisterMountInterface(CDataFileMgr::EXPLOSION_INFO_FILE, &g_explosionFileMounter);	
	CExplosionInfoManager::Load(pFileName);
}

void CExplosionInfoManager::Append(const char* pFileName)
{
	CExplosionInfoManager tempManager;
	if(Verifyf(PARSER.LoadObject(pFileName,"meta",tempManager),"File not properly loaded"))
	{
		atArray<CExplosionTagData>& tagDataArray = tempManager.m_aExplosionTagData;
		atArray<CExplosionTagData>& managerArray = m_ExplosionInfoManagerInstance.m_aExplosionTagData;
		for (int i=0;i<tagDataArray.GetCount();i++)
		{
			bool bFound=false;
			for(int j=0;j<managerArray.GetCount();j++)
			{
				if(managerArray[j].name==tagDataArray[i].name)
				{
					bFound = true;
					break;
				}
			}
			if(!bFound)
			{
				int indexToInsert = eExplosionTag_Parse(atHashValue(tagDataArray[i].name));
				if(Verifyf(indexToInsert!=EXP_TAG_DONTCARE,"Explosion tag not defined in code"))
				{
					managerArray[indexToInsert] = tagDataArray[i];
				}
			}
		}
	}
}

// Load the data in explosion.meta parsed by Explosion.psc
void CExplosionInfoManager::Load(const char* pFileName)
{
	if(Verifyf(!m_ExplosionInfoManagerInstance.m_aExplosionTagData.size(), "Explosion info has already been loaded"))
	{
		// Reset/Delete the existing data before loading
		Reset();

		// Load
		Verifyf(fwPsoStoreLoader::LoadDataIntoObject(pFileName, META_FILE_EXT, m_ExplosionInfoManagerInstance), "Failed to load %s", pFileName);
		CExplosionTagData& dontCareData = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData( EXP_TAG_EXTINGUISHER );
		int emptyEntriesToFill = NUM_EEXPLOSIONTAG - m_ExplosionInfoManagerInstance.m_aExplosionTagData.size();
		for(int i=0;i<emptyEntriesToFill ;i++)
		{
			m_ExplosionInfoManagerInstance.m_aExplosionTagData.PushAndGrow(dontCareData);
		}
		// GTAV - B*1731672 - limit the maximum radius of the molotov explosion to 2x what it currently is in the meta file
		CExplosionTagData& molotovData = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData( EXP_TAG_MOLOTOV );
		molotovData.endRadius = Min( molotovData.endRadius, 6.0f );
	}
}

// Delete all explosion info.
void CExplosionInfoManager::Shutdown()
{
	Reset();
}

// Delete all explosion info.
void CExplosionInfoManager::Reset()
{
	m_ExplosionInfoManagerInstance.m_aExplosionTagData.Reset();
	m_ExplosionInfoManagerInstance.m_aExplosionTagData.Reserve(NUM_EEXPLOSIONTAG);
}

// Accessor method for CExplosionTagData
CExplosionTagData& CExplosionInfoManager::GetExplosionTagData(eExplosionTag tagIndex)
{
	Assertf(tagIndex >= 0 && tagIndex < m_aExplosionTagData.size(), "Invalid tag index! (%d) tagdatasize (%d)", tagIndex, m_aExplosionTagData.size());
	if (tagIndex >= 0 && tagIndex < m_aExplosionTagData.size())
	{
		return m_aExplosionTagData[tagIndex];
	}
	else
	{
		return m_Dummy;
	}
}

float CExplosionInfoManager::GetExplosionEndRadius(const CExplosionTagData& expTagData)
{
	if (NetworkInterface::IsInCopsAndCrooks())
	{
		if (atStringHash(expTagData.name.c_str()) == atStringHash("EXP_TAG_KINETICRAM"))
		{
		 	if (CPlayerSpecialAbilityManager::ms_CNCAbilityBullbarsAttackRadius > 0.f)
			{
				return CPlayerSpecialAbilityManager::ms_CNCAbilityBullbarsAttackRadius;
			}
		}	

#if ARCHIVED_SUMMER_CONTENT_ENABLED    
		if (atStringHash(expTagData.name.c_str()) == atStringHash("EXP_TAG_MINE_CNCSPIKE"))
		{
			if (CPlayerSpecialAbilityManager::ms_CNCAbilitySpikeMineRadius > 0.f)
			{
				return CPlayerSpecialAbilityManager::ms_CNCAbilitySpikeMineRadius;
			}
		}
#endif
	}

	return expTagData.endRadius;
}

float CExplosionInfoManager::GetExplosionForceFactor(const CExplosionTagData& expTagData)
{
	if (expTagData.name == "EXP_TAG_KINETICRAM" && NetworkInterface::IsInCopsAndCrooks())
	{
		if (CPlayerSpecialAbilityManager::ms_CNCAbilityBullbarsImpulseAmount > 0.f)
		{
			return CPlayerSpecialAbilityManager::ms_CNCAbilityBullbarsImpulseAmount;
		}
	}
	return expTagData.forceFactor;
}

#if __BANK

// Add widgets to a bank in RAG
void CExplosionInfoManager::AddWidgets(bkBank& bank)
{
	bank.PushGroup("Explosion Info Manager");

	bank.AddButton("Save", CExplosionInfoManager::SaveCallback);

	const int nExplosionInfoCount = m_ExplosionInfoManagerInstance.GetSize();
	if( nExplosionInfoCount > 0 )
	{
		bank.PushGroup("Explosion Types");

		for (int i=0; i < nExplosionInfoCount; i++)
		{
			bank.PushGroup( m_ExplosionInfoManagerInstance.m_aExplosionTagData[i].name.c_str());
			PARSER.AddWidgets(bank, &m_ExplosionInfoManagerInstance.m_aExplosionTagData[i]);
			bank.PopGroup();
		}

		bank.PopGroup();
	}

	bank.PopGroup(); 
}

// Save the info file when the user clicks the appropriate button
void CExplosionInfoManager::SaveCallback()
{
	// Hacky save path.
	const char* sExplosionMetadataAssetsSubPath = "export/data/metadata/explosion.pso.meta";
	char filename[RAGE_MAX_PATH];
	formatf(filename, RAGE_MAX_PATH, "%s%s", CFileMgr::GetAssetsFolder(), sExplosionMetadataAssetsSubPath);
	Verifyf(PARSER.SaveObject(filename, "", &m_ExplosionInfoManagerInstance, parManager::XML), "Failed to save explosion info file:  %s", filename);
}

#endif
