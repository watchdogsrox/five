///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	WaterCannon.cpp
//	BY	: 	Mark Nicholson (Based on Obbe's original code)
//	FOR	:	Rockstar North
//	ON	:	03 Jan 2007
//	WHAT:	Water Cannon Control and Rendering
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "WaterCannon.h"

// Rage headers
#include "math/random.h"
#include "rmptfx/ptxmanager.h"

// Framework headers
#include "grcore/debugdraw.h"
#include "fwmaths/angle.h"
#include "vfx/channel.h"
#include "vfx/decal/decalmanager.h"
#include "vfx/ptfx/ptfxreg.h"
#include "vfx/vfxwidget.h"

// game Headers
#include "Camera/CamInterface.h"
#include "camera/base/BaseCamera.h"
#include "Camera/Gameplay/GameplayDirector.h"
#include "Core/Game.h"
#include "Event/EventDamage.h"
#include "Event/Events.h"
#include "Event/EventShocking.h"
#include "Event/ShockingEvents.h"
#include "Game/ModelIndices.h"
#include "Network/NetworkInterface.h"
#include "Peds/PedIntelligence.h"
#include "Peds/Ped.h"
#include "physics/WorldProbe/worldprobe.h"
#include "System/ControlMgr.h"
#include "Task/Physics/TaskNM.h"
#include "Task/Physics/TaskNMFlinch.h"
#include "Vehicles/Automobile.h"
#include "Vfx/Decals/DecalManager.h"
#include "Vfx/Misc/Fire.h"
#include "Vfx/Systems/VfxLens.h"
#include "Vfx/Systems/VfxWater.h"
#include "Vfx/VfxHelper.h"


///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_MISC_OPTIMISATIONS()


///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////

CWaterCannons		g_waterCannonMan;

dev_s32				WATER_CANNON_NEW_POINT_DELAY_MS				= 30;//150;
dev_s32				WATER_CANNON_COLN_ACTIVE_TIMER				= 150;

bank_float			WATER_CANNON_HITPED_FORCE_FACTOR			= 0.75f;				// mass independent
bank_float			WATER_CANNON_HITPED_UPWARDS_FORCE			= 0.5f;
dev_float			WATER_CANNON_HITPED_DAMAGE					= 2.5f;

dev_float			WATER_CANNON_HITVEH_FORCE					= 1000.0f;				// not mass independent
dev_float			WATER_CANNON_HITVEH_MAX_FORCE_MASS_RATIO	= 2.2f;				// must be lower than 150.0f
dev_float			WATER_CANNON_HITVEH_DAMAGE					= 0.1f;

dev_float			WATER_CANNON_HITPEDVEH_FORCE_MAX_OFFSET		= 0.5f;

dev_float			WATER_CANNON_SPRAY_DIST_EVO_THRESH_MIN		= 25.0f;
dev_float			WATER_CANNON_SPRAY_DIST_EVO_THRESH_MAX		= 60.0f;
dev_float			WATER_CANNON_SPRAY_ANGLE_EVO_THRESH_MIN		= 0.0f;
dev_float			WATER_CANNON_SPRAY_ANGLE_EVO_THRESH_MAX		= 1.0f;

dev_float			WATER_CANNON_COLN_CAPSULE_RADIUS			= 0.3f;


const atHashWithStringNotFinal CWaterCannon::ms_WaterCannonWeaponHash("WEAPON_HIT_BY_WATER_CANNON",0xCC34325E);


///////////////////////////////////////////////////////////////////////////////
//  CLASS CWaterCannon
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

void			CWaterCannon::Init							(CVehicle* pVehicle, eHierarchyId iWeaponBone)
{
	m_id = 0;
	m_currPoint = 0;
	m_lastTimeStamp = fwTimer::GetTimeInMilliseconds();

	m_regdVehicle = pVehicle;
	m_iWeaponBone = iWeaponBone;

	for (s32 i=0; i<NUM_CANNON_POINTS; i++)
	{
		m_pointInfos[i].isValid = false;
	}
	
	m_registeredThisFrame = false;

	m_vColnPos = Vec3V(V_ZERO);
	m_vColnNormal = Vec3V(V_ZERO);
	m_vColnVel = Vec3V(V_ZERO);
	m_vColnDir = Vec3V(V_ZERO);
	m_lastColnTime = 0;
}


///////////////////////////////////////////////////////////////////////////////
//  Update
///////////////////////////////////////////////////////////////////////////////

void			CWaterCannon::Update						(float deltaTime)
{
	// check if the vehicle has been deleted
	if (m_regdVehicle.Get()==NULL)
	{
		ptfxReg::UnRegister(this);
		m_id = 0;
		return;
	}

	// check if the timer has expired
	if (fwTimer::GetTimeInMilliseconds() > m_lastTimeStamp+WATER_CANNON_NEW_POINT_DELAY_MS)
	{	
		// time to shift along one
		s32 prevPoint = m_currPoint;
		m_currPoint = (m_currPoint+1) % NUM_CANNON_POINTS;
		m_lastTimeStamp = fwTimer::GetTimeInMilliseconds();
		
		// set sensible coordinate in new point
		m_pointInfos[m_currPoint].isValid = false;
		m_pointInfos[m_currPoint].vPos = m_pointInfos[prevPoint].vPos;
		m_pointInfos[m_currPoint].vVel = Vec3V(V_ZERO);
	}

	UpdateWaterJetCollision();

	// update the velocity and position of the points
	for (s32 i=0; i<NUM_CANNON_POINTS; i++)
	{
		if (m_pointInfos[i].isValid)
		{
			m_pointInfos[i].vVel.SetZf(m_pointInfos[i].vVel.GetZf() - (9.81f * deltaTime));
			m_pointInfos[i].vPos += m_pointInfos[i].vVel * ScalarVFromF32(deltaTime);

#if __BANK
			if (g_waterCannonMan.m_renderDebugPoints)
			{
				if (i==m_currPoint)
				{
					grcDebugDraw::Sphere(RCC_VECTOR3(m_pointInfos[i].vPos), 0.2f, Color32(0.0f, 0.0f, 1.0f, 1.0f));
				}
				else if (m_pointInfos[i].isValid)
				{
					grcDebugDraw::Sphere(RCC_VECTOR3(m_pointInfos[i].vPos), 0.2f, Color32(0.0f, 1.0f, 0.0f, 1.0f));
				}
			}
#endif
		}
	}

	// apply water to the game world 
	for (s32 i=0; i<NUM_CANNON_POINTS; i++)
	{
		if (m_pointInfos[i].isValid)
		{
			// extinguish any fires in range
			g_fireMan.ExtinguishAreaSlowly(m_pointInfos[i].vPos, 2.0f);

			// wash off any dirt textures slowly
			g_decalMan.WashInRange(0.04f, m_pointInfos[i].vPos, ScalarV(V_TWO), NULL);
		}
	}

	// collision 
	bool colnActive = fwTimer::GetTimeInMilliseconds()<m_lastColnTime+WATER_CANNON_COLN_ACTIVE_TIMER;
	if (colnActive)
	{
#if __BANK
		if (g_waterCannonMan.m_renderDebugCollisions)
		{
			grcDebugDraw::Sphere(m_vColnPos, 1.0f, Color32(1.0f, 0.0f, 0.0f, 1.0f), false);
			grcDebugDraw::Line(m_vColnPos, m_vColnPos+m_vColnNormal, Color32(0.0f, 1.0f, 0.0f, 1.0f));
			grcDebugDraw::Line(m_vColnPos, m_vColnPos+m_vColnVel, Color32(0.0f, 0.0f, 1.0f, 1.0f));
		}
#endif

		// update spray particles
#if __BANK
		if (!g_waterCannonMan.m_disableSprayFx)
#endif
		{
			g_vfxWater.UpdatePtFxWaterCannonSpray(this, m_vColnPos, m_vColnNormal, m_vColnDir);
		}
	}

	// update jet particles
#if __BANK
	if (!g_waterCannonMan.m_disableJetFx)
#endif
	{
		s32 boneIndex = m_regdVehicle->GetVehicleModelInfo()->GetBoneIndex(m_iWeaponBone);
		if (boneIndex>-1)
		{
			Vec3V vNozzleOffset(V_Y_AXIS_WZERO);

			if (MI_VAN_RIOT_2.IsValid() && m_regdVehicle->GetModelIndex()==MI_VAN_RIOT_2)
			{
				vNozzleOffset = Vec3V(0.0f, 0.0f, 0.0f);
			}

			g_vfxWater.UpdatePtFxWaterCannonJet(this, m_registeredThisFrame, boneIndex, vNozzleOffset, m_pointInfos[m_currPoint].vVel, colnActive, m_vColnPos, m_vColnNormal, m_regdVehicle->GetVelocity().Mag());
		}
	}

	if (colnActive)
	{
		ProcessHitVehicles();
		ProcessHitPeds();
		ProcessImpactEvents();
	}

	// check if the water cannon is inactive
	bool stillActive = false;
	if (m_registeredThisFrame || colnActive)
	{
		stillActive = true;
	}
	else
	{
		for (s32 i=0; i<NUM_CANNON_POINTS; i++)
		{
			if (m_pointInfos[i].isValid)
			{
				stillActive = true;
				break;
			}
		}
	}

	m_registeredThisFrame = false;

	// shutdown if no longer active
	if (!stillActive)
	{
		ptfxReg::UnRegister(this);
		m_id = 0;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdateWaterJetCollision
///////////////////////////////////////////////////////////////////////////////

void			CWaterCannon::UpdateWaterJetCollision			()
{
	// have to start one further along or things will go wrong
	s32 currIndex = (m_currPoint) % NUM_CANNON_POINTS;
	s32 nextIndex = (currIndex - 1);
	s32 prevIndex = currIndex;
	if (nextIndex<0) 
	{
		nextIndex += NUM_CANNON_POINTS;
	}

	bool anySegmentCollided = false;
	s32 numPointsProcessed = 0;
	s32 activeSegmentCount = 0;
	while (numPointsProcessed<NUM_CANNON_POINTS-1)
	{
// 		if (anySegmentCollided)
// 		{
// 			m_pointInfos[currIndex].isValid = false;
// 		}
// 		else
		{
			// Draw a line if both elements are used.
			if (m_pointInfos[currIndex].isValid && m_pointInfos[nextIndex].isValid)
			{
				// test if this segment collides
				WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
				WorldProbe::CShapeTestHitPoint probeResult;
				WorldProbe::CShapeTestResults probeResults(probeResult);
				capsuleDesc.SetResultsStructure(&probeResults);
				capsuleDesc.SetCapsule(RCC_VECTOR3(m_pointInfos[currIndex].vPos), RCC_VECTOR3(m_pointInfos[nextIndex].vPos), WATER_CANNON_COLN_CAPSULE_RADIUS);
				capsuleDesc.SetIsDirected(true);
				capsuleDesc.SetIncludeFlags(ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_PED_TYPE | ArchetypeFlags::GTA_MAP_TYPE_MOVER | ArchetypeFlags::GTA_GLASS_TYPE);
				if (WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
				{
					// fill out the collision position and normal for the first collision
					if (anySegmentCollided==false)
					{
						m_vColnPos = RCC_VEC3V(probeResults[0].GetHitPosition());
						m_vColnNormal = RCC_VEC3V(probeResults[0].GetHitNormal());
						m_vColnVel = m_pointInfos[currIndex].vVel;
						m_vColnDir = m_pointInfos[nextIndex].vPos - m_pointInfos[currIndex].vPos;
						m_vColnDir = Normalize(m_vColnDir);
						m_lastColnTime = fwTimer::GetTimeInMilliseconds();
					}

					g_WeaponAudioEntity.PlayWaterCannonHit(m_vColnPos, probeResults[0].GetHitEntity(), probeResults[0].GetHitMaterialId(), probeResults[0].GetHitComponent());

					anySegmentCollided = true;
					m_pointInfos[nextIndex].isValid = false;
					if (activeSegmentCount==0)
					{
						m_pointInfos[currIndex].isValid = false;
					}

					activeSegmentCount = 0;
				}

#if __BANK
				if (g_waterCannonMan.m_renderDebugSegments)
				{
					Color32 colRed(1.0f, 0.0f, 0.0f, 1.0f);
					grcDebugDraw::Line(RCC_VECTOR3(m_pointInfos[currIndex].vPos), RCC_VECTOR3(m_pointInfos[nextIndex].vPos), colRed);
				}
#endif

				// test for camera collision 
				Vec3V_ConstRef vCamPos = RCC_VEC3V(camInterface::GetGameplayDirector().GetFrame().GetPosition());
				Vec3V vCamToSphere = m_pointInfos[currIndex].vPos - vCamPos;
				if (MagSquared(vCamToSphere).Getf()<=1.0f)
				{
					g_vfxLens.Register(VFXLENSTYPE_WATER, 1.0f, 0.0f, 0.0f, 1.0f);
				}

				activeSegmentCount++;
			}
			else
			{
				activeSegmentCount = 0;
			}
		}

		prevIndex = currIndex;
		currIndex = nextIndex;
		nextIndex = currIndex-1;
		if (nextIndex<0)
		{
			nextIndex += NUM_CANNON_POINTS;
		}

		numPointsProcessed++;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  RegisterPointData
///////////////////////////////////////////////////////////////////////////////

void			CWaterCannon::RegisterPointData				(Vec3V_In vPosition, Vec3V_In vVelocity)
{
	m_pointInfos[m_currPoint].isValid = true;
	m_pointInfos[m_currPoint].vPos = vPosition;
	m_pointInfos[m_currPoint].vVel = vVelocity;

	m_registeredThisFrame = true;

#if __BANK
	if (g_waterCannonMan.m_renderDebugPoints)
	{
		grcDebugDraw::Sphere(RCC_VECTOR3(vPosition), 0.25f, Color32(0.0f, 1.0f, 0.0f, 1.0f));
	}
#endif
}


///////////////////////////////////////////////////////////////////////////////
// ProcessHitVehicles
///////////////////////////////////////////////////////////////////////////////

void			CWaterCannon::ProcessHitVehicles		()
{
	// get the collision point sphere data
	spdSphere sphereWld(m_vColnPos, ScalarV(V_ONE));

	// go through the vehicle pool
	CVehicle::Pool* pVehiclePool = CVehicle::GetPool();
	s32 i = (s32) pVehiclePool->GetSize();	
	while (i--)
	{
		CVehicle* pVehicle = pVehiclePool->GetSlot(i);

		// check that the vehicle is valid 
		if (pVehicle && pVehicle!=m_regdVehicle && pVehicle->IsCollisionEnabled() && !pVehicle->IsNetworkClone())
		{
			// get the oriented bounding box of the vehicle
			spdAABB aabbVehicleLcl;
			aabbVehicleLcl = pVehicle->GetLocalSpaceBoundBox(aabbVehicleLcl);

#if __BANK
			if (g_waterCannonMan.m_renderDebugHitVehicles)
			{
				grcDebugDraw::BoxOriented(aabbVehicleLcl.GetMin(), aabbVehicleLcl.GetMax(), pVehicle->GetMatrix(), Color32(0.0f, 1.0f, 0.0f, 1.0f), false);
			}
#endif

			bool bFriendlyFireThisVehicleAllowed = true;
			if (NetworkInterface::IsGameInProgress() && m_regdVehicle && m_regdVehicle->GetDriver())
			{
				bFriendlyFireThisVehicleAllowed = NetworkInterface::IsFriendlyFireAllowed(pVehicle,m_regdVehicle->GetDriver());
			}

			// check if the water point intersects the vehicle 
			spdSphere sphereLcl = UnTransformOrthoSphere(pVehicle->GetMatrix(), sphereWld);
			if (bFriendlyFireThisVehicleAllowed && aabbVehicleLcl.IntersectsSphere(sphereLcl))
			{
#if __BANK
				if (g_waterCannonMan.m_renderDebugHitVehicles)
				{
					grcDebugDraw::Sphere(m_vColnPos, 1.0f, Color32(1.0f, 1.0f, 0.0f, 1.0f), true);
				}
#endif

				// audio 
				g_WeaponAudioEntity.PlayWaterCannonHitVehicle(pVehicle, m_vColnPos);

				// extinguish all 'fake' fires
				pVehicle->GetVehicleDamage()->SetPetrolTankHealth( Max(pVehicle->GetVehicleDamage()->GetPetrolTankHealth(), (VEH_DAMAGE_HEALTH_PTANK_ONFIRE + 1.0f) ) );
				pVehicle->m_Transmission.SetEngineHealth( Max(pVehicle->m_Transmission.GetEngineHealth(), (ENGINE_DAMAGE_ONFIRE + 1.0f) ) );
				for (int i=0; i<pVehicle->GetNumWheels(); i++)
				{
					pVehicle->GetWheel(i)->ClearWheelOnFire();
				}

				// apply some damage so that the AI and scripts can react properly
				const Vec3V vWaterNormal = Normalize(-m_vColnVel);
				const Vec3V vWaterPos = m_vColnPos;

				CEntity* pInflictor = m_regdVehicle;
				if (m_regdVehicle->GetDriver())
				{
					pInflictor = m_regdVehicle->GetDriver();
				}
				
				pVehicle->m_VehicleDamage.ApplyDamage(pInflictor, DAMAGE_TYPE_WATER_CANNON, ms_WaterCannonWeaponHash, WATER_CANNON_HITVEH_DAMAGE, RCC_VECTOR3(vWaterPos), RCC_VECTOR3(vWaterNormal), -RCC_VECTOR3(vWaterNormal), 0);

				// calc force
				Vec3V vHitVehicleForce = m_vColnVel * ScalarVFromF32(WATER_CANNON_HITVEH_FORCE);

				// clamp max force to avoid pushing light vehicles around too much
				const ScalarV fHitVehicleForceMag = Mag(vHitVehicleForce);
				const ScalarV vehMass = ScalarVFromF32(pVehicle->GetMass());
				const ScalarV maxMassRatio = ScalarVFromF32(WATER_CANNON_HITVEH_MAX_FORCE_MASS_RATIO);
				const ScalarV vehMaxForceRatio = Scale(vehMass, maxMassRatio);
				vHitVehicleForce = SelectFT( IsGreaterThan(fHitVehicleForceMag, vehMaxForceRatio), vHitVehicleForce, Scale(vHitVehicleForce , InvScale(vehMaxForceRatio, fHitVehicleForceMag)) );

				// GTAV B*2789896 - The volatus model is too tall, with the wheels too close together, his makes it unstable and the water cannon can knock it over too easily.
				// To fix this scale down the force that is applied.
				if( pVehicle->GetVehicleModelInfo()->GetModelNameHash() == MI_HELI_VOLATUS.GetName().GetHash() )
				{
					static ScalarV volatusWaterCannonForceScale = ScalarV( 0.4f );
					vHitVehicleForce = Scale( vHitVehicleForce, volatusWaterCannonForceScale );
				}

				// calc offset
				Vec3V vHitVehicleOffset = vWaterPos - pVehicle->GetTransform().GetPosition();

				// clamp offset
				const ScalarV maxForceOffset = ScalarVFromF32(WATER_CANNON_HITPEDVEH_FORCE_MAX_OFFSET);
				if (IsGreaterThanAll(MagSquared(vHitVehicleOffset), Scale(maxForceOffset, maxForceOffset)) != 0)
				{
					const Vec3V vHitVehicleOffsetDir = Normalize(vHitVehicleOffset);
					vHitVehicleOffset = Scale(vHitVehicleOffsetDir, maxForceOffset);
				}

				const ScalarV apparentlyArbitraryForceScale(V_FOUR);
				vHitVehicleForce = Scale(vHitVehicleForce, apparentlyArbitraryForceScale);
				pVehicle->ApplyForce(RCC_VECTOR3(vHitVehicleForce), RCC_VECTOR3(vHitVehicleOffset));
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
// ProcessHitPeds
///////////////////////////////////////////////////////////////////////////////

void			CWaterCannon::ProcessHitPeds						()
{
	// get the collision point sphere data
	spdSphere sphereWld(m_vColnPos, ScalarV(V_ONE));

	// go through the ped pool
	CPed::Pool *PedPool = CPed::GetPool();
	s32 i = PedPool->GetSize();	
	while (i--)
	{
		CPed* pPed = PedPool->GetSlot(i);

		// check that the ped is valid 
		if (pPed && /*pPed->GetUsesCollision() &&*/ CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_WATERJET, m_regdVehicle))
		{
			// get the axis aligned bounding box of the ped
			spdAABB aabbPed;
			aabbPed = pPed->GetBoundBox(aabbPed);

#if __BANK
			if (g_waterCannonMan.m_renderDebugHitPeds)
			{
				grcDebugDraw::BoxAxisAligned(aabbPed.GetMin(), aabbPed.GetMax(), Color32(1.0f, 0.0f, 0.0f, 1.0f), false);
			}
#endif

			bool bFriendlyFireThisPedAllowed = true;
			if (NetworkInterface::IsGameInProgress() && m_regdVehicle && m_regdVehicle->GetDriver())
			{
				bFriendlyFireThisPedAllowed = NetworkInterface::IsFriendlyFireAllowed(pPed,m_regdVehicle->GetDriver());
			}

			// check if the water point intersects the ped 
			if (bFriendlyFireThisPedAllowed && aabbPed.IntersectsSphere(sphereWld))
			{
#if __BANK
				if (g_waterCannonMan.m_renderDebugHitPeds)
				{
					grcDebugDraw::Sphere(m_vColnPos, 1.0f, Color32(1.0f, 0.5f, 0.0f, 1.0f), true);
				}
#endif

				// audio
				g_WeaponAudioEntity.PlayWaterCannonHitPed(pPed, m_vColnPos);

				// make the ped wet
				pPed->IncWetClothing(1.0f);

				// create an event to get the ped to fall over
				Vec3V_ConstRef vWaterSpeed = m_vColnVel;
				Vec3V_ConstRef vWaterPos = m_vColnPos;

				CEntity* pInflictor = m_regdVehicle;
				if (m_regdVehicle->GetDriver())
				{
					pInflictor = m_regdVehicle->GetDriver();
				}

				CEventDamage damageEvent(pInflictor,fwTimer::GetTimeInMilliseconds(), ms_WaterCannonWeaponHash);
				float fDamage = WATER_CANNON_HITPED_DAMAGE;
				const CItemInfo* pItemInfo = CWeaponInfoManager::GetInfo(ms_WaterCannonWeaponHash);
				if(pItemInfo && pItemInfo->GetIsClassId(CWeaponInfo::GetStaticClassId()))
				{
					const CWeaponInfo* pWeaponInfo = static_cast<const CWeaponInfo*>(pItemInfo);
					{
						fDamage *= pWeaponInfo->GetWeaponDamageModifier();
					}
				}
				CPedDamageCalculator damageCalculator(pInflictor, fDamage, ms_WaterCannonWeaponHash, 0, false);
				damageCalculator.ApplyDamageAndComputeResponse(pPed, damageEvent.GetDamageResponseData(), CPedDamageCalculator::DF_None);

				Vec3V vZero(V_ZERO);
				CTaskNMControl* pNMResponse = rage_new CTaskNMControl(2500, 50000, rage_new CTaskNMFlinch(2500, 50000, RCC_VECTOR3(vZero), m_regdVehicle, CTaskNMFlinch::FLINCHTYPE_WATER_CANNON), CTaskNMControl::DO_BLEND_FROM_NM);
				damageEvent.SetPhysicalResponseTask(pNMResponse, true);

				if (damageEvent.AffectsPedCore(pPed))
				{
					pPed->GetPedIntelligence()->AddEvent(damageEvent);
				}

				// calc force
				Vec3V vForce = (vWaterSpeed*ScalarVFromF32(WATER_CANNON_HITPED_FORCE_FACTOR) + Vec3V(V_Z_AXIS_WZERO)*ScalarVFromF32(WATER_CANNON_HITPED_UPWARDS_FORCE));
                static dev_float sfMaxPedMass = 150.0f;
                float fPedMass = Clamp(pPed->GetMass(), 0.0f, sfMaxPedMass);
                vForce *= ScalarVFromF32(fPedMass);

				// calc offset
				Vec3V vOffset = vWaterPos - pPed->GetTransform().GetPosition();

				// clamp offset at 1m
				if (MagSquared(vOffset).Getf() > WATER_CANNON_HITPEDVEH_FORCE_MAX_OFFSET*WATER_CANNON_HITPEDVEH_FORCE_MAX_OFFSET)
				{
					vOffset = Normalize(vOffset);
					vOffset *= ScalarVFromF32(WATER_CANNON_HITPEDVEH_FORCE_MAX_OFFSET);
				}

				if (pPed->GetRagdollState() == RAGDOLL_STATE_PHYS)
				{
					pPed->ApplyForceCg(4.0f*RCC_VECTOR3(vForce));
					pPed->ApplyTorque(4.0f*VEC3V_TO_VECTOR3(Cross(vOffset, vForce)));
				}
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// ProcessImpactEvents
///////////////////////////////////////////////////////////////////////////////

void CWaterCannon::ProcessImpactEvents()
{
	// Throw an explosion event in the immediate vicinity of impact for AI peds to react against.
	// The explosion owner is the driver of the vehicle.
	if (m_regdVehicle && m_regdVehicle->GetDriver())
	{
		CPed* pExplosionOwner = m_regdVehicle->GetDriver();
		
		// Limit this to only player peds.
		if (pExplosionOwner->IsAPlayerPed())
		{
			CEventShockingExplosion event;
			event.Set(m_vColnPos, pExplosionOwner, NULL);

			// Don't say the explosion audio.
			event.SetShouldUseGenericAudio(true);

			// Limit the event's range.
			event.SetVisualReactionRangeOverride(8.0f);
			event.SetAudioReactionRangeOverride(8.0f);

			// Add the shocking event to the world.
			CShockingEventsManager::Add(event);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// ClearWaterCannonPoints
///////////////////////////////////////////////////////////////////////////////

void CWaterCannon::ClearWaterCannonPoints()
{
	for(int i = 0; i < NUM_CANNON_POINTS; i++)
	{
		m_pointInfos[i].isValid = false;
	}
}




///////////////////////////////////////////////////////////////////////////////
//  CLASS CWaterCannons
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

void			CWaterCannons::Init							(unsigned initMode)
{
	if (initMode == INIT_CORE)
	{
		for (s32 i=0; i<MAX_WATER_CANNONS; i++)
		{
			m_waterCannons[i].Init(NULL, VEH_INVALID_ID);
		}

#if __BANK
		m_renderDebugPoints = false;
		m_renderDebugSegments = false;
		m_renderDebugFires = false;
		m_renderDebugCollisions = false;
		m_renderDebugHitVehicles = false;
		m_renderDebugHitPeds = false;
		m_disableJetFx = false;
		m_disableSprayFx = false;
		m_useConstDirection = false;
#endif
	}
}


///////////////////////////////////////////////////////////////////////////////
//  Shutdown
///////////////////////////////////////////////////////////////////////////////

void			CWaterCannons::Shutdown						(unsigned UNUSED_PARAM(shutdownMode))
{	
}


///////////////////////////////////////////////////////////////////////////////
//  Update
///////////////////////////////////////////////////////////////////////////////

void			CWaterCannons::Update						(float deltaTime)
{
	g_WeaponAudioEntity.CleanWaterCannonMapFlags();
	g_WeaponAudioEntity.CleanWaterCannonFlags();

	for (s32 i=0; i<MAX_WATER_CANNONS; i++)
	{
		if (m_waterCannons[i].m_id)
		{
			m_waterCannons[i].Update(deltaTime);
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  RegisterCannon
///////////////////////////////////////////////////////////////////////////////

void			CWaterCannons::RegisterCannon				(CVehicle* pVehicle, fwUniqueObjId cannonId, Vec3V_In vPosition, Vec3V_In vVelocity, eHierarchyId iWeaponBone)
{
	// try to find a cannon with this id
	for (s32 i=0; i<MAX_WATER_CANNONS; i++)
	{
		if (m_waterCannons[i].m_id == cannonId)
		{
			vfxAssertf(pVehicle==m_waterCannons[i].m_regdVehicle, "CWaterCannons::RegisterCannon - vehicle mismatch found");
			m_waterCannons[i].RegisterPointData(vPosition, vVelocity);
			return;
		}
	}

	// we haven't found a cannon with this id then find an idle one
	for (s32 i=0; i<MAX_WATER_CANNONS; i++)
	{
		if (!m_waterCannons[i].m_id)
		{
			m_waterCannons[i].Init(pVehicle, iWeaponBone);
			m_waterCannons[i].m_id = cannonId;
			m_waterCannons[i].RegisterPointData(vPosition, vVelocity);
			return;
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  ClearAllWatercannonSprayInWorld
///////////////////////////////////////////////////////////////////////////////

void			CWaterCannons::ClearAllWatercannonSprayInWorld		()
{
	for (s32 i=0; i<MAX_WATER_CANNONS; i++)
	{
		if (m_waterCannons[i].m_id)
		{
			m_waterCannons[i].ClearWaterCannonPoints();
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  InitWidgets
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void			CWaterCannons::InitWidgets							()
{
	bkBank* pVfxBank = vfxWidget::GetBank();
	pVfxBank->PushGroup("Water Cannon Fx", false);

#if __DEV
	pVfxBank->AddSlider("New Point Delay (ms)", &WATER_CANNON_NEW_POINT_DELAY_MS, 1, 5000, 1);
	pVfxBank->AddSlider("Collision Active Timer (ms)", &WATER_CANNON_COLN_ACTIVE_TIMER, 1, 5000, 1);

	pVfxBank->AddSlider("Hit Ped Force Factor", &WATER_CANNON_HITPED_FORCE_FACTOR, 0.0f, 10.0f, 0.05f);
	pVfxBank->AddSlider("Hit Ped Updwards Force", &WATER_CANNON_HITPED_UPWARDS_FORCE, 0.0f, 10.0f, 0.05f);
	pVfxBank->AddSlider("Hit Ped Damage", &WATER_CANNON_HITPED_DAMAGE, 0.0f, 100.0f, 0.05f);

	pVfxBank->AddSlider("Hit Veh Force", &WATER_CANNON_HITVEH_FORCE, 0.0f, 2000.0f, 0.5f);
	pVfxBank->AddSlider("Hit Veh Force Mass Ratio", &WATER_CANNON_HITVEH_MAX_FORCE_MASS_RATIO, 0.0f, 200.0f, 0.5f);
	pVfxBank->AddSlider("Hit Veh Damage", &WATER_CANNON_HITVEH_DAMAGE, 0.0f, 10.0f, 0.05f);

	pVfxBank->AddSlider("Hit Ped/Veh Force Max Offset", &WATER_CANNON_HITPEDVEH_FORCE_MAX_OFFSET, 0.0f, 10.0f, 0.05f);

	pVfxBank->AddSlider("Spray Dist Evo Thresh Min", &WATER_CANNON_SPRAY_DIST_EVO_THRESH_MIN, 0.0f, 100.0f, 0.5f);
	pVfxBank->AddSlider("Spray Dist Evo Thresh Max", &WATER_CANNON_SPRAY_DIST_EVO_THRESH_MAX, 0.0f, 100.0f, 0.5f);
	pVfxBank->AddSlider("Spray Angle Evo Thresh Min", &WATER_CANNON_SPRAY_ANGLE_EVO_THRESH_MIN, 0.0f, 1.0f, 0.01f);
	pVfxBank->AddSlider("Spray Angle Evo Thresh Max", &WATER_CANNON_SPRAY_ANGLE_EVO_THRESH_MAX, 0.0f, 1.0f, 0.01f);

	pVfxBank->AddSlider("Collision Capsule Radius", &WATER_CANNON_COLN_CAPSULE_RADIUS, 0.0f, 2.0f, 0.01f);
#endif

	pVfxBank->AddToggle("Render Debug Points", &m_renderDebugPoints);
	pVfxBank->AddToggle("Render Debug Segments", &m_renderDebugSegments);
	pVfxBank->AddToggle("Render Debug Fires", &m_renderDebugFires);
	pVfxBank->AddToggle("Render Debug Collisions", &m_renderDebugCollisions);
	pVfxBank->AddToggle("Render Debug Hit Vehicles", &m_renderDebugHitVehicles);
	pVfxBank->AddToggle("Render Debug Hi Peds", &m_renderDebugHitPeds);
	pVfxBank->AddToggle("Disable Jet Fx", &m_disableJetFx);
	pVfxBank->AddToggle("Disable Spray Fx", &m_disableSprayFx);
	pVfxBank->AddToggle("Use Const Direction", &m_useConstDirection);

	pVfxBank->PopGroup();
}
#endif 

