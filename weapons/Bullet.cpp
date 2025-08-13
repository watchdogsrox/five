// File header
#include "Bullet.h"

// Framework headers
#include "fragment/Instance.h"
#include "fwpheffects/ropeManager.h"
#include "fwpheffects/clothManager.h"
#include "fwmaths/geomutil.h"
#include "phbound/boundcomposite.h"

// Game headers
#include "Camera/CamInterface.h"
#include "Camera/Gameplay/GameplayDirector.h"
#include "Control/Replay/Replay.h"
#include "Control/Replay/Effects/ParticleWeaponFxPacket.h"
#include "Control/Replay/Misc/RopePacket.h"
#include "Debug/DebugScene.h"
#include "Event/EventDamage.h"
#include "Event/EventWeapon.h"
#include "Network/NetworkInterface.h"
#include "Network/Objects/Entities/NetObjPlayer.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Physics/Physics.h"
#include "Physics/WorldProbe/WorldProbe.h"
#include "Renderer/Water.h"
#include "Scene/Lod/LodMgr.h"
#include "Script/Script_Areas.h"
#include "Vfx/Systems/VfxWeapon.h"
#include "Vfx/VehicleGlass/VehicleGlassManager.h"
#include "Vfx/VfxHelper.h"
#include "Vehicles/Heli.h"
#include "Weapons/AirDefence.h"
#include "Weapons/WeaponDamage.h"
#include "Weapons/WeaponDebug.h"

// Macro to disable optimizations if set
WEAPON_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////
// CBullet
////////////////////////////////////////////////////////////////////////////////

FW_INSTANTIATE_CLASS_POOL_SPILLOVER(CBullet, CBullet::MAX_STORAGE, 0.54f, atHashString("CBullet",0x4552434f));
FW_INSTANTIATE_CLASS_POOL_SPILLOVER(CBullet::sBulletInstance, CBullet::sBulletInstance::MAX_STORAGE, 0.44f, atHashString("CBullet::sBulletInstance",0x636873fe));

////////////////////////////////////////////////////////////////////////////////

#if __BANK
u32 CBullet::sBulletInstance::s_uNewGenerationId = 0;

bool CBullet::ms_CollideWithMoverCollision = false;
#endif // __BANK

#if !__FINAL
u32 CBullet::m_uRejectionReason = CPedDamageCalculator::DRR_None;
#define bulletImpactRejectionDebug1f(reason) CBullet::m_uRejectionReason = reason;
#define bulletImpactRejectionDebug2f(pVictim, pInflictor, nWeaponHash, nHitComponent) if (m_uRejectionReason != CPedDamageCalculator::DRR_None) { pVictim->AddDamageRecord(CPed::sDamageRecord(pInflictor, fwTimer::GetTimeInMilliseconds(), nWeaponHash, 0.0f, 0.0f, 0.0f, pVictim->GetHealth(), nHitComponent, false, true, CBullet::m_uRejectionReason)); }
#else
#define bulletImpactRejectionDebug1f(reason)
#define bulletImpactRejectionDebug2f(pVictim, pInflictor, nWeaponHash, nHitComponent)
#endif //__FINAL

////////////////////////////////////////////////////////////////////////////////

CBullet::CBullet()
: m_pFiringEntity(NULL)
, m_pWeaponInfo(NULL)
, m_vOriginPosition(Vector3::ZeroType)
, m_vMuzzlePosition(Vector3::ZeroType)
, m_vMuzzleDirection(Vector3::ZeroType)
, m_fLifeTime(0.0f)
, m_fDamage(0.0f)
, m_fRecoilAccuracyWhenFired(1.f)
, m_fFallOffRangeModifier(1.f)
, m_fFallOffDamageModifier(1.f)
, m_bCheckVehicleTyres(false)
, m_bAllowDamageToVehicle(false)
, m_bAllowDamageToVehicleOccupants(false)
, m_bPerformPenetration(false)
, m_bSilenced(false)
, m_bCheckForAirDefenceZone(false)
, m_bIsBentBulletFromAimAssist(false)
#if __DEV
, m_WeaponOwner(NULL)
#endif // __DEV
{
}

////////////////////////////////////////////////////////////////////////////////

CBullet::CBullet(CEntity* pFiringEntity, const CWeaponInfo* pWeaponInfo, const Vector3& vStart, const Vector3& vEnd, const f32 fVelocity, const f32 fLifeTime, const f32 fDamage, u32 uWeaponHash, bool bCreatesTrace, bool bAllowDamageToVehicle, bool bAllowDamageToVehicleOccupants, const Vector3& vMuzzlePosition, const Vector3& vMuzzleDirection, bool bIsAccurate, bool bPerformPenetration, bool bPerformBulletProofGlassPenetration, float fRecoilAccuracyWhenFired, bool bSilenced, float fFallOffRangeModifer, float fFallOffDamageModifer, bool bIsBentBulletFromAimAssist, CWeapon* DEV_ONLY(pWeaponOwner))
: m_pFiringEntity(pFiringEntity)
, m_pWeaponInfo(pWeaponInfo)
, m_vOriginPosition(vStart)
, m_vMuzzlePosition(vMuzzlePosition)
, m_vMuzzleDirection(vMuzzleDirection)
, m_fLifeTime(fLifeTime)
, m_fDamage(fDamage)
, m_fRecoilAccuracyWhenFired(fRecoilAccuracyWhenFired)
, m_fFallOffRangeModifier(fFallOffRangeModifer)
, m_fFallOffDamageModifier(fFallOffDamageModifer)
, m_bAllowDamageToVehicle(bAllowDamageToVehicle)
, m_bAllowDamageToVehicleOccupants(bAllowDamageToVehicleOccupants)
, m_bPerformPenetration(bPerformPenetration)
, m_bPerformBulletProofGlassPenetration(bPerformBulletProofGlassPenetration)
, m_bSilenced(bSilenced)
, m_bCheckForAirDefenceZone(false)
, m_bIsBentBulletFromAimAssist(bIsBentBulletFromAimAssist)
#if __DEV
, m_WeaponOwner(pWeaponOwner)
#endif // __DEV
{
#if !__FINAL
	weaponDebugf3("CBullet::CBullet pFiringEntity[%p][%s] vStart[%f %f %f] vEnd[%f %f %f] fVelocity[%f] fLifeTime[%f] fDamage[%f]"
		,pFiringEntity,pFiringEntity ? pFiringEntity->GetModelName() : ""
		,vStart.x,vStart.y,vStart.z,vEnd.x,vEnd.y,vEnd.z,fVelocity,fLifeTime,fDamage);
	weaponDebugf3("                 uWeaponHash[%u] bCreatesTrace[%d] bAllowDamageToVehicle[%d] bAllowDamageToVehicleOccupants[%d]"
		,uWeaponHash,bCreatesTrace,bAllowDamageToVehicle,bAllowDamageToVehicleOccupants);
	weaponDebugf3("                 vMuzzlePosition[%f %f %f] vMuzzleDirection[%f %f %f] bIsAccurate[%d] bPerformPenetration[%d] fRecoilAccuracyWhenFired[%f]"
		,vMuzzlePosition.x,vMuzzlePosition.y,vMuzzlePosition.z,vMuzzleDirection.x,vMuzzleDirection.y,vMuzzleDirection.z,bIsAccurate,bPerformPenetration,fRecoilAccuracyWhenFired);
#endif

	weaponFatalAssertf(m_pWeaponInfo, "m_pWeaponInfo is NULL");

	// Add the original bullet to the batch - we should always have at least one bullet
	m_Batch.Push(rage_new sBulletInstance(m_pWeaponInfo, vStart, vEnd, fVelocity, uWeaponHash, bCreatesTrace, bIsAccurate));

	// Should we check vehicle tyres?
	if(m_pFiringEntity && m_pFiringEntity->GetIsTypePed() && static_cast<CPed*>(m_pFiringEntity.Get())->IsPlayer())
	{
		// Always check tyres if a player
		m_bCheckVehicleTyres = true;
	}
	else
	{
		// Randomly check tyres otherwise
		static dev_float TYRE_HIT_CHANCE = 0.35f;
		if(fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < TYRE_HIT_CHANCE)
		{
			m_bCheckVehicleTyres = true;
		}
		else
		{
			m_bCheckVehicleTyres = false;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

CBullet::~CBullet()
{
	for(s32 i = 0; i < m_Batch.GetCount(); i++)
	{
		delete m_Batch[i];
	}
	m_Batch.Reset();
}

////////////////////////////////////////////////////////////////////////////////

// Cloud tunables
bool CBullet::sm_bSpecialAmmoFMJVehicleGlassAlwaysPenetrate = true;

void CBullet::InitTunables()
{
	sm_bSpecialAmmoFMJVehicleGlassAlwaysPenetrate = ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("GUNRUN_AMMO_FMJ_VEH_GLASSPENETRATION", 0xA4509BA4), sm_bSpecialAmmoFMJVehicleGlassAlwaysPenetrate);
}

////////////////////////////////////////////////////////////////////////////////

void CBullet::Init()
{
	// Init the pools
	InitPool(MEMBUCKET_GAMEPLAY);
	sBulletInstance::InitPool(MEMBUCKET_GAMEPLAY);
}

////////////////////////////////////////////////////////////////////////////////

void CBullet::Shutdown()
{
	// Shutdown the pools
	sBulletInstance::ShutdownPool();
	ShutdownPool();
}

////////////////////////////////////////////////////////////////////////////////

bool CBullet::Process()
{
	PF_AUTO_PUSH_TIMEBAR("CBullet Process");

	if(!m_pWeaponInfo)
	{
		// Can't fire without a weapon info
		return false;
	}

	if(m_fLifeTime <= 0.0f)
	{
		// Generate AI events - these bullets never hit anything, so when the lifetime expires anyone who was near the path needs to react if they
		// came close enough to the flight path.
		GenerateWhizzedByEvents();

		// Out of time
		return false;
	}

	// Update batch tests
	for(s32 i = 0; i < m_Batch.GetCount(); i++)
	{
		if(m_Batch[i]->bToBeRemoved)
		{
			delete m_Batch[i];
			m_Batch.DeleteFast(i--);
		}
	}

	if(m_Batch.GetCount() == 0)
	{
		// No probes left
		return false;
	}

	// Update the bullet positions by their velocity
	UpdatePositions();

	// Get the impacts in one go as a batch
	sBatchImpacts impacts;
	if(ComputeImpacts(impacts))
	{
		// Do damage
		ProcessImpacts(impacts);
	}
	else
	{
		m_bCheckForAirDefenceZone = true;

		for(s32 i = 0; i < m_Batch.GetCount(); i++)
		{
			// if the bullet didn't impact with anything we still need to check if it's hit water
			// otherwise, this gets done within ProcessImpacts
			ProcessHitsOceanWater(i);
		}
	}

	if(m_bCheckForAirDefenceZone)
	{
		for(s32 i = 0; i < m_Batch.GetCount(); i++)
		{
			// Ensure sure we aren't going to pass through an air defence sphere.
			ProcessHitsAirDefenceZone(i);
		}

		m_bCheckForAirDefenceZone = false;
	}

	// bullet traces
	ProcessVfx();

	// Update the life timer
	m_fLifeTime -= fwTimer::GetTimeStep();
	m_fLifeTime  = Max(0.0f, m_fLifeTime);

	if(!NetworkInterface::IsGameInProgress() || !NetworkUtils::IsNetworkClone(m_pFiringEntity))
	{
		if(m_pWeaponInfo && m_pWeaponInfo->GetDamageType() == DAMAGE_TYPE_EXPLOSIVE && m_pWeaponInfo->GetCreateBulletExplosionWhenOutOfTime())
		{
			if(m_fLifeTime <= 0.0f)
			{
				for(s32 i = 0; i < m_Batch.GetCount(); i++)
				{
					if(!m_Batch[i]->bToBeRemoved)
					{
						eExplosionTag tag = m_pWeaponInfo->GetExplosionTag();
						if(tag != EXP_TAG_DONTCARE)
						{
							CExplosionManager::CExplosionArgs explosionArgs(tag, m_Batch[i]->vPosition);

							explosionArgs.m_pEntExplosionOwner = m_pFiringEntity;
							explosionArgs.m_bInAir = true;

							Vector3 vDir(m_Batch[i]->vPosition - m_Batch[i]->vOldPosition);
							vDir.NormalizeFast();
							explosionArgs.m_vDirection = vDir;
							explosionArgs.m_originalExplosionTag = tag;

							CExplosionManager::AddExplosion(explosionArgs);
						}
					}
				}
			}
		}
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CBullet::Add(const Vector3& vStart, const Vector3& vEnd, const f32 fVelocity, u32 uWeaponHash, bool bCreatesTrace, bool bIsAccurate)
{
	if(m_Batch.IsFull())
	{
		return false;
	}

	m_Batch.Push(rage_new sBulletInstance(m_pWeaponInfo, vStart, vEnd, fVelocity, uWeaponHash, bCreatesTrace, bIsAccurate));
	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CBullet::ComputeIsBulletInAngledArea(Vector3& vPos1, Vector3& vPos2, float areaWidth, const CEntity* pFiringEntity)
{
	// Specific entity?
	if(pFiringEntity == NULL || m_pFiringEntity == pFiringEntity)
	{
		for(s32 i = 0; i < m_Batch.GetCount(); i++)
		{
			if(CScriptAreas::IsPointInAngledArea(m_Batch[i]->vPosition, vPos1, vPos2, areaWidth, false, true, false))
			{
				return true;
			}
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CBullet::ComputeIsBulletInSphere(const Vector3& vPosition, f32 fRadius, const CEntity* pFiringEntity) const
{
	// B*1859454: If ped is in a vehicle, we use this as the firing entity for vehicle weapons 
	// (doesn't affect drive-by weapons as we check (m_pFiringEntity == pFiringEntity || ...) first
	CEntity *pVehicleEntity = NULL;
	if (pFiringEntity && pFiringEntity->GetIsTypePed())
	{
		const CPed *pPed = static_cast<const CPed*>(pFiringEntity);
		if (pPed && pPed->GetIsInVehicle())
		{
			pVehicleEntity = pPed->GetVehiclePedInside();
		}
	}
	// Specific entity?
	if(pFiringEntity == NULL || (m_pFiringEntity == pFiringEntity || (pVehicleEntity && m_pFiringEntity == pVehicleEntity)))
	{
		const float fRadiusSqr = fRadius * fRadius;

		for(s32 i = 0; i < m_Batch.GetCount(); i++)
		{
#if 0		// TODO -- consider using fwGeom::DistToLineSqr instead
			if (fwGeom::DistToLineSqr(m_Batch[i]->vOldPosition, m_Batch[i]->vPosition, vPosition) < fRadiusSqr)
			{
				continue;
			}
#else
			// Does the line pass through the sphere?
			Vector3 vClosestPoint;
			fwGeom::fwLine testLine(m_Batch[i]->vOldPosition, m_Batch[i]->vPosition);
			testLine.FindClosestPointOnLine(vPosition, vClosestPoint);

			Vector3 vFromPositionToClosest = vClosestPoint - vPosition;
			if(vFromPositionToClosest.Mag2() < fRadiusSqr)
			{
				return true;
			}
#endif
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CBullet::ComputeIsBulletInBox(const Vector3& vMin, const Vector3& vMax, const CEntity* pFiringEntity) const
{
	// Specific entity?
	if(pFiringEntity == NULL || m_pFiringEntity == pFiringEntity)
	{
		for(s32 i = 0; i < m_Batch.GetCount(); i++)
		{
			// Does the line pass through the box?
			if(geomBoxes::TestSegmentToBoxMinMax(m_Batch[i]->vOldPosition, m_Batch[i]->vPosition, vMin, vMax))
			{
				return true;
			}
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

#if __BANK
void CBullet::RenderDebug() const
{
	static dev_u32 EXPIRY_TIME = 2000;

	static const s32 MAX_STR = 64;
	char buff[MAX_STR];
	for(s32 i = 0; i < m_Batch.GetCount(); i++)
	{
		formatf(buff, MAX_STR, "bullet_%d", m_Batch[i]->uGenerationId);
		CWeaponDebug::ms_debugStore.AddLine(RCC_VEC3V(m_Batch[i]->vOldPosition), RCC_VEC3V(m_Batch[i]->vPosition), Color_red, EXPIRY_TIME, atStringHash(buff));
	}
}
#endif // __BANK

////////////////////////////////////////////////////////////////////////////////

void CBullet::UpdatePositions()
{
	f32 fTimeStep = fwTimer::GetTimeStep();

	// If the life time expires within this time step, adjust the velocity appropriately
	if(m_fLifeTime < fTimeStep)
	{
		fTimeStep = m_fLifeTime;
	}

	for(s32 i = 0; i < m_Batch.GetCount(); i++)
	{
		// Old position is now current position
		m_Batch[i]->vOldPosition = m_Batch[i]->vPosition;

		// Update the bullet position by the velocity
		m_Batch[i]->vPosition += m_Batch[i]->vVelocity * fTimeStep;
	}
}

////////////////////////////////////////////////////////////////////////////////

struct SplitImpactsFunctor
{
	SplitImpactsFunctor(Vec3V_In bulletDirection) : m_BulletDirection(bulletDirection) {}

	bool operator()(const WorldProbe::CShapeTestHitPoint& hitPoint, int const UNUSED_PARAM(idx)) const
	{
		// If the bullet is going the same way as the normal this is an exit intersection, return false
		return IsLessThanAll(Dot(m_BulletDirection,hitPoint.GetHitNormalV()),ScalarV(V_ZERO)) != 0; 
	}

	Vec3V m_BulletDirection;
};

bool CBullet::ComputeImpacts(sBatchImpacts& impacts) const
{
	PF_AUTO_PUSH_TIMEBAR("CBullet ComputeImpacts");

	// Weapon test flags
	const s32 iTypeFlags    = ArchetypeFlags::GTA_WEAPON_TEST;

#if __BANK
	s32 iIncludeFlags = ArchetypeFlags::GTA_WEAPON_TYPES;
	if(ms_CollideWithMoverCollision)
	{
		iIncludeFlags = ((ArchetypeFlags::GTA_WEAPON_TYPES & ~ArchetypeFlags::GTA_MAP_TYPE_WEAPON) | ArchetypeFlags::GTA_MAP_TYPE_MOVER);
	}
#else // __BANK
	const s32 iIncludeFlags = ArchetypeFlags::GTA_WEAPON_TYPES;
#endif // __BANK

	// Build a list of exceptions
	const CEntity* ppExceptions[1] = { NULL };
	s32 iNumExceptions = 0;
	if(m_pFiringEntity)
	{
		ppExceptions[0] = m_pFiringEntity;
		iNumExceptions = 1;
	}

	// Options for the probes.
	int nLosOptions = 0;
	nLosOptions |= WorldProbe::LOS_IGNORE_SHOOT_THRU;
	nLosOptions |= WorldProbe::LOS_IGNORE_POLY_SHOOT_THRU;
	if(m_bCheckVehicleTyres)
	{
		nLosOptions |= WorldProbe::LOS_TEST_VEH_TYRES;
	}

	int nExcludeOptions = 0;
	if(m_bAllowDamageToVehicle)
	{
		nExcludeOptions |= WorldProbe::EIEO_DONT_ADD_VEHICLE;
	}
	if(m_bAllowDamageToVehicleOccupants)
	{
		nExcludeOptions |= WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS;
	}

	// Allow multi-bullet weapons like the shotgun to reuse cull information via the batch test.
	if(m_Batch.GetCount() > 1 && CBulletManager::ms_bUseBatchTestsForSingleShotWeapons)
	{
		// Fill in the descriptor for the batch test.
		WorldProbe::CShapeTestBatchDesc batchProbeDesc;
		batchProbeDesc.SetOptions(nLosOptions);
		batchProbeDesc.SetExcludeEntities(ppExceptions, iNumExceptions, nExcludeOptions);
		batchProbeDesc.SetIncludeFlags(iIncludeFlags);
		batchProbeDesc.SetTypeFlags(iTypeFlags);
		batchProbeDesc.SetExcludeTypeFlags(TYPE_FLAGS_NONE);
		batchProbeDesc.SetContext(WorldProbe::LOS_GeneralAI);
		ALLOC_AND_SET_PROBE_DESCRIPTORS(batchProbeDesc,m_Batch.GetCount());

		// Specify a custom cull-volume for the batch test based on the spread of the bullets.
		WorldProbe::CCullVolumeCapsuleDesc cullVolDesc;
		TUNE_FLOAT(fBatchCullRadius, 1.0f, 0.1f, 10.0f, 0.01f);
		cullVolDesc.SetCapsule(m_Batch[0]->vOldPosition, m_Batch[0]->vPosition, fBatchCullRadius);
		batchProbeDesc.SetCustomCullVolume(&cullVolDesc);
		
		if(CBulletManager::ms_bUseUndirectedBatchTests)
		{
			WorldProbe::CShapeTestFixedResults<2*sBulletImpacts::MAX_IMPACTS> entryAndExitImpacts[MAX_BULLETS_IN_BATCH];

			for(s32 i = 0; i < m_Batch.GetCount(); ++i)
			{
				// Add the entry and exit probes for each bullet in m_Batch.
				WorldProbe::CShapeTestProbeDesc probeDesc;
				probeDesc.SetIsDirected(false);
				probeDesc.SetStartAndEnd(m_Batch[i]->vOldPosition, m_Batch[i]->vPosition);
				probeDesc.SetResultsStructure(&entryAndExitImpacts[i]);
				batchProbeDesc.AddLineTest(probeDesc);

				ComputeClothImpacts(m_Batch[i]->vOldPosition, m_Batch[i]->vPosition);
				ComputeRopeImpacts(m_Batch[i]->vOldPosition, m_Batch[i]->vPosition);
			}

			if(WorldProbe::GetShapeTestManager()->SubmitTest(batchProbeDesc))
			{
				for(s32 i = 0; i < m_Batch.GetCount(); i++)
				{
					const Vec3V bulletDirection = Subtract(RCC_VEC3V(m_Batch[i]->vPosition),RCC_VEC3V(m_Batch[i]->vOldPosition));
					WorldProbe::CShapeTestResults& entryImpacts = impacts.batch[i].entryImpacts;
					WorldProbe::CShapeTestResults& exitImpacts = impacts.batch[i].exitImpacts;

					// Split the entry and exit impacts into separate arrays depending on which way the impact normal points
					entryAndExitImpacts[i].SplitHits(SplitImpactsFunctor(bulletDirection),entryImpacts,exitImpacts);

					// Flip the T-value of the exit impacts since they currently represent distance from the start of the bullet
					for(int exitImpactIndex = 0; exitImpactIndex < exitImpacts.GetNumHits(); ++exitImpactIndex)
					{
						exitImpacts[exitImpactIndex].SetHitTValue(1.0f-exitImpacts[exitImpactIndex].GetHitTValue());
#if __DEV
						// NOTE: Flipping the depth here would require us to know the length of the probe, since
						//       that is expensive and nobody is using the depth, I'll just make it unusable rather than
						//       somewhat usable and wrong. 
						exitImpacts[exitImpactIndex].SetDepth(FLT_MAX);
#endif // __DEV
					}

					PreprocessImpacts(entryImpacts, CompareEntryIntersections);
					PreprocessImpacts(exitImpacts, CompareExitIntersections);
				}

				return true;
			}
		}
		else
		{
			for(s32 i = 0; i < m_Batch.GetCount(); ++i)
			{
				// Add the entry and exit probes for each bullet in m_Batch.
				WorldProbe::CShapeTestProbeDesc entryProbeDesc;
				entryProbeDesc.SetStartAndEnd(m_Batch[i]->vOldPosition, m_Batch[i]->vPosition);
				entryProbeDesc.SetResultsStructure(&impacts.batch[i].entryImpacts);

				WorldProbe::CShapeTestProbeDesc exitProbeDesc;
				exitProbeDesc.SetStartAndEnd(m_Batch[i]->vPosition, m_Batch[i]->vOldPosition);
				exitProbeDesc.SetResultsStructure(&impacts.batch[i].exitImpacts);

				batchProbeDesc.AddLineTest(entryProbeDesc);
				batchProbeDesc.AddLineTest(exitProbeDesc);

				ComputeClothImpacts(m_Batch[i]->vOldPosition, m_Batch[i]->vPosition);

				ComputeRopeImpacts(m_Batch[i]->vOldPosition, m_Batch[i]->vPosition);
			}

			if(WorldProbe::GetShapeTestManager()->SubmitTest(batchProbeDesc))
			{
				for(s32 i = 0; i < m_Batch.GetCount(); i++)
				{
					PreprocessImpacts(impacts.batch[i].entryImpacts, CompareEntryIntersections);
					PreprocessImpacts(impacts.batch[i].exitImpacts, CompareExitIntersections);
				}

				return true;
			}
		}
	}
	else // No batching enabled, just do the individual line probes and hope for the best.
	{
		// No more than one bullet can be processed this way anyway!
		Assert(m_Batch.GetCount()<=1);

		WorldProbe::CShapeTestProbeDesc entryProbeDesc;
		entryProbeDesc.SetStartAndEnd(m_Batch[0]->vOldPosition, m_Batch[0]->vPosition);
		entryProbeDesc.SetResultsStructure(&impacts.batch[0].entryImpacts);
		entryProbeDesc.SetOptions(nLosOptions);
		entryProbeDesc.SetIncludeFlags(iIncludeFlags);
		entryProbeDesc.SetExcludeEntities(ppExceptions, iNumExceptions, nExcludeOptions);
		entryProbeDesc.SetTypeFlags(iTypeFlags);
		entryProbeDesc.SetExcludeTypeFlags(TYPE_FLAGS_NONE);
		entryProbeDesc.SetContext(WorldProbe::LOS_GeneralAI);

		ComputeClothImpacts(m_Batch[0]->vOldPosition, m_Batch[0]->vPosition);

		ComputeRopeImpacts(m_Batch[0]->vOldPosition, m_Batch[0]->vPosition);

		bool bEntryHitFound = WorldProbe::GetShapeTestManager()->SubmitTest(entryProbeDesc);
		if( bEntryHitFound )
		{	
			for(s32 i = 0; i < m_Batch.GetCount(); i++)
			{
				PreprocessImpacts(impacts.batch[i].entryImpacts, CompareEntryIntersections);
			}

			if( m_bPerformPenetration )
			{
				WorldProbe::CShapeTestProbeDesc exitProbeDesc;
				exitProbeDesc.SetStartAndEnd(m_Batch[0]->vPosition, m_Batch[0]->vOldPosition);
				exitProbeDesc.SetResultsStructure(&impacts.batch[0].exitImpacts);
				exitProbeDesc.SetOptions(nLosOptions);
				exitProbeDesc.SetIncludeFlags(iIncludeFlags);
				exitProbeDesc.SetExcludeEntities(ppExceptions, iNumExceptions, nExcludeOptions);
				exitProbeDesc.SetTypeFlags(iTypeFlags);
				exitProbeDesc.SetExcludeTypeFlags(TYPE_FLAGS_NONE);
				exitProbeDesc.SetContext(WorldProbe::LOS_GeneralAI);

				bool bExitHitFound  = WorldProbe::GetShapeTestManager()->SubmitTest(exitProbeDesc);
				if( bExitHitFound )
				{
					for(s32 i = 0; i < m_Batch.GetCount(); i++)
					{
						PreprocessImpacts(impacts.batch[i].exitImpacts, CompareExitIntersections);
					}
				}
			}

			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CBullet::ComputeClothImpacts(const Vector3& vOldPosition, const Vector3& vPosition) const
{
	// NOTE: we don't want to use cloth sphere bounds but the phverletcloth bounding box information
	// might look asspensive ( is not ) here, but the result is excellent ( accurate ) and especially for cloth pieces next to each other
	// Svetli

	clothManager* clothMgr = CPhysics::GetClothManager();
	Assert(clothMgr);
	atDNode<clothInstance*>* headNode =  clothMgr->GetHead();
	for (atDNode<clothInstance*>* node = headNode; node; node=node->GetNext())
	{
		clothInstance* cinst = (clothInstance*)(node->Data);
		Assert( cinst );
		if( cinst->IsActive() && (cinst->GetType() == clothManager::Environment) )
		{
			rage::clothController* inst = cinst->GetClothController();
			Assert( inst );

			environmentCloth* ecloth = (environmentCloth*)inst->GetOwner();
			Assert( ecloth );
			phVerletCloth* cloth = ecloth->GetCloth();
			Assert( cloth );
			if( ComputeIsBulletInBox( VEC3V_TO_VECTOR3(ecloth->GetBBMinWorldSpace()), VEC3V_TO_VECTOR3(ecloth->GetBBMaxWorldSpace()), NULL ) )
			{					
				Vector3 vDir = vPosition - vOldPosition;
				vDir.Normalize();

				float clothweight = cloth->GetClothWeight();
				float fShootForce = 0.4f * clothweight * clothweight * clothweight;

// Note: if we want to unpin verts with the shot this need to be here - svetli
				rage::atFunctor4< void, int, int, int, bool > funcSwap;
				funcSwap.Reset< rage::clothController, &rage::clothController::OnClothVertexSwap >( inst );

				const atBitSet& stateList = ecloth->GetClothController()->GetBridge()->GetStateList();

				clothMgr->SkipSlowdown();
				cloth->ApplyImpulse( stateList, VECTOR3_TO_VEC3V(fShootForce * vDir), VECTOR3_TO_VEC3V(vOldPosition), &funcSwap, ecloth->GetOffset() );
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CBullet::ComputeRopeImpacts(const Vector3& vOldPosition, const Vector3& vPosition) const
{
	ropeManager* ropeMgr = CPhysics::GetRopeManager();
	Assert(ropeMgr);
	atDNode<ropeInstance*>* headNode =  ropeMgr->GetHead();
	for (atDNode<ropeInstance*>* node = headNode; node; node=node->GetNext())
	{
		environmentCloth* pEnvCloth = node->Data->GetEnvCloth();
		Assert( pEnvCloth );
		phVerletCloth* pCloth = pEnvCloth->GetCloth();
		Assert( pCloth );

		static float errThreshold = 0.06f;
// Ignore the weapons can't break ropes, e.g., stun gun.
		if( (!m_pWeaponInfo || !m_pWeaponInfo->GetDontBreakRopes()) &&
			node->Data->IsBreakable() && ComputeIsBulletInBox( VEC3V_TO_VECTOR3( rage::Add(pEnvCloth->GetBBMinWorldSpace(), Vec3V(-errThreshold, -errThreshold, -errThreshold)) ), VEC3V_TO_VECTOR3(rage::Add(pEnvCloth->GetBBMaxWorldSpace(), Vec3V(errThreshold, errThreshold, errThreshold))), NULL ) 
			)
		{					
			// Make a segment going through the cloth.
			Vector3 vDir = vPosition - vOldPosition;
			vDir.Normalize();

			float clothweight = pCloth->GetClothWeight();
			float fShootForce = 2.0f * clothweight;

			// Note: if we want to unpin verts with the shot this need to be here - svetli
			rage::atFunctor4< void, int, int, int, bool > funcSwap;
			funcSwap.Reset< rage::clothController, &rage::clothController::OnClothVertexSwap >( pEnvCloth->GetClothController() );
			const atBitSet& stateList = pEnvCloth->GetClothController()->GetBridge()->GetStateList();
			Vec3V_In impulse = VECTOR3_TO_VEC3V(fShootForce * vDir);
			ScalarV impulseMag = ScalarVFromF32(fShootForce);
			if (IsGreaterThanAll(impulseMag,ScalarV(V_ZERO)))
			{				
				Vec3V segmentStart = VECTOR3_TO_VEC3V(vOldPosition);
				Vec3V segmentEnd = VECTOR3_TO_VEC3V(vPosition);

				const int totalNumVerts = pCloth->GetNumVertices();
				const int lockedFront = pCloth->GetNumLockedEdgesFront();
				const int lockedBack = pCloth->GetNumLockedEdgesBack();
				const int lastVertexIndex = (totalNumVerts-lockedBack)-1;
				phClothData& clothdata = pCloth->GetClothData();

				Vec3V ropeStart = clothdata.GetVertexPosition( lockedFront );
				Vec3V ropeEnd = clothdata.GetVertexPosition( lastVertexIndex );

				Vec3V pointA2 = ropeEnd - ropeStart;

				Vec3V pointB1 = segmentStart - ropeStart;
				Vec3V pointB2 = segmentEnd - ropeStart;

				Vec3V pointA, pointB;
				geomPoints::FindClosestSegSegToSeg(pointA.GetIntrin128Ref(), pointB.GetIntrin128Ref(), pointA2.GetIntrin128(), pointB1.GetIntrin128(), pointB2.GetIntrin128());

				Vec3V newSeg = Subtract(pointA,pointB);
				//Vec3V normalSeq = Normalize(newSeg);
				ScalarV newSegMag = Mag(newSeg);

				static float distThreshold = 0.5f;
				if( IsLessThanAll( Scale(newSegMag,newSegMag), ScalarVFromF32(distThreshold*distThreshold) ) != 0 )
				{
					const float ropeLen = node->Data->GetLength();
					float ropeALen = Mag( pointA ).Getf();
					float ropeBLen = ropeLen - ropeALen;
// NOTE: the child ropes now have parentID and are deleted when parent is deleted later on script cleanup
					ropeInstance* ropeA = NULL;
					ropeInstance* ropeB = NULL;
					ropeMgr->BreakRope( node->Data, ropeA, ropeB, ropeALen, ropeBLen, ropeALen, pCloth->GetNumVertices()-1  );

					if( ropeA )
					{
#if GTA_REPLAY
						if(CReplayMgr::ShouldRecord())
						{
							Vec3V rot( 0.0f, 0.0f, -0.5f*PI );
							CReplayMgr::RecordPersistantFx<CPacketAddRope>(
								CPacketAddRope(ropeA->GetUniqueID(), node->Data->GetWorldPositionA(), rot, ropeALen, (float)(pCloth->GetNumVertices()-1), ropeALen, node->Data->GetLengthChangeRate(), node->Data->GetType(), (int)Clamp(ropeALen, 4.0f, 16.0f),node->Data->IsPPUOnly(),true,node->Data->GetGravityScale(),false, true),
								CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)ropeA),
								NULL, 
								true);
	
							CEntity* pAttachedA = CReplayRopeManager::GetAttachedEntityA(node->Data->GetUniqueID());
							if(	pAttachedA )
							{
								CReplayMgr::RecordPersistantFx<CPacketAttachRopeToEntity>(
									CPacketAttachRopeToEntity(ropeA->GetUniqueID(), node->Data->GetWorldPositionA(), 0, 0, ropeA->GetLocalOffset()),
									CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)ropeA),
									(CEntity*)pAttachedA,
									false);
							}
						}
#endif //GTA_REPLAY


						environmentCloth* pRopeAEnvCloth = ropeA->GetEnvCloth();
						Assert( pRopeAEnvCloth );
						phVerletCloth* pRopeACloth = pRopeAEnvCloth->GetCloth();
						Assert( pRopeACloth );

						const int totalNumVertsA = pRopeACloth->GetNumVertices();
						const int lockedBackA = pRopeACloth->GetNumLockedEdgesBack();
						const int lastVertexIndexA = (totalNumVertsA-lockedBackA)-1;
						phClothData& clothdataA = pRopeACloth->GetClothData();

						pRopeACloth->ApplyImpulse( stateList, impulse, clothdataA.GetVertexPosition(lastVertexIndexA), &funcSwap, Vec3V(V_ZERO) );
					}

					if( ropeB )
					{
#if GTA_REPLAY
						if(CReplayMgr::ShouldRecord())
						{
							Vec3V rot( 0.0f, 0.0f, -0.5f*PI );
							CReplayMgr::RecordPersistantFx<CPacketAddRope>(
								CPacketAddRope(ropeB->GetUniqueID(), node->Data->GetWorldPositionB(), rot, ropeBLen, (float)(pCloth->GetNumVertices()-1), ropeBLen, node->Data->GetLengthChangeRate(), node->Data->GetType(), (int)Clamp(ropeBLen, 4.0f, 16.0f), node->Data->IsPPUOnly(), true,node->Data->GetGravityScale(),false,(node->Data->GetInstanceB()?true:false)),
								CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)ropeB),
								NULL, 
								true);
	
							CEntity* pAttachedB = CReplayRopeManager::GetAttachedEntityB(node->Data->GetUniqueID());
							if(	pAttachedB )
							{
								CReplayMgr::RecordPersistantFx<CPacketAttachRopeToEntity>(
									CPacketAttachRopeToEntity(ropeB->GetUniqueID(), node->Data->GetWorldPositionB(), 0, 0, ropeB->GetLocalOffset()),
									CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)ropeB),
									(CEntity*)pAttachedB,
									false);
							}
						}
#endif //GTA_REPLAY
						environmentCloth* pRopeBEnvCloth = ropeB->GetEnvCloth();
						Assert( pRopeBEnvCloth );
						phVerletCloth* pRopeBCloth = pRopeBEnvCloth->GetCloth();
						Assert( pRopeBCloth );

						const int totalNumVertsB = pRopeBCloth->GetNumVertices();
						const int lockedBackB = pRopeBCloth->GetNumLockedEdgesBack();
						const int lastVertexIndexB = (totalNumVertsB-lockedBackB)-1;
						phClothData& clothdataB = pRopeBCloth->GetClothData();

						pRopeBCloth->ApplyImpulse( stateList, impulse, clothdataB.GetVertexPosition(lastVertexIndexB), &funcSwap, Vec3V(V_ZERO) );
					}
#if GTA_REPLAY
					if(CReplayMgr::ShouldRecord())
					{
						CEntity* pAttachedA = CReplayRopeManager::GetAttachedEntityA(node->Data->GetUniqueID());
						CEntity* pAttachedB = CReplayRopeManager::GetAttachedEntityB(node->Data->GetUniqueID());
						CReplayMgr::RecordPersistantFx<CPacketDeleteRope>( CPacketDeleteRope(node->Data->GetUniqueID()),	CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)node->Data), pAttachedA, pAttachedB, false);
					}
#endif //GTA_REPLAY
					ropeMgr->RemoveRope( node->Data );
				}
			} // end if
		} // end if
	} // end for
}

////////////////////////////////////////////////////////////////////////////////

void CBullet::PreprocessImpacts(sBulletImpacts::Impacts& impacts, WorldProbe::CShapeTestResults::SortFunctionCB sortFn) const
{
	// Sort the hit-points according to the rule in the call-back function.
	impacts.SortHits(sortFn);
}

////////////////////////////////////////////////////////////////////////////////

bool CBullet::IsImpactAesthetic( CEntity* pImpactEntity, const WorldProbe::CShapeTestHitPoint* pImpactHitPoint )
{
	bool bAesthetic = false;
	if( pImpactHitPoint && pImpactEntity && pImpactEntity->GetIsTypePed() )
	{
		// Check special condition hit component (ignore ARMS)
		u16 iEntryHitComponent = pImpactHitPoint->GetHitComponent();
		eAnimBoneTag nHitBoneTag = ((CPed*)pImpactEntity)->GetBoneTagFromRagdollComponent( iEntryHitComponent );
		if( nHitBoneTag==BONETAG_R_UPPERARM || nHitBoneTag==BONETAG_R_FOREARM || nHitBoneTag==BONETAG_R_HAND ||
			nHitBoneTag==BONETAG_L_UPPERARM || nHitBoneTag==BONETAG_L_FOREARM || nHitBoneTag==BONETAG_L_HAND ) 
		{
			bAesthetic = true;
		}
	}

	return bAesthetic;
}

//////////////////////////////////////////////////////////////////////////

bool CBullet::IsImpactHeadShot( CEntity* pImpactEntity, const WorldProbe::CShapeTestHitPoint* pImpactHitPoint )
{
	if( pImpactHitPoint && pImpactEntity && pImpactEntity->GetIsTypePed() )
	{
		// Check if we've hit the head
		u16 iEntryHitComponent = pImpactHitPoint->GetHitComponent();
		eAnimBoneTag nHitBoneTag = ((CPed*)pImpactEntity)->GetBoneTagFromRagdollComponent( iEntryHitComponent );
		return CPed::IsHeadShot( nHitBoneTag );
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CBullet::ProcessImpacts(sBatchImpacts& impacts)
{
	PF_AUTO_PUSH_TIMEBAR("CBullet ProcessImpacts");
	bulletImpactRejectionDebug1f(CPedDamageCalculator::DRR_None)

	bool isUnderWater = false;
	for(s32 i = 0; i < m_Batch.GetCount(); i++)
	{
		// Loop through all entry impacts
		u32 nNumEntryImpacts = impacts.batch[i].entryImpacts.GetNumHits();
		bool bStuck = false;
		m_BulletCollisionPairs.Reset();

		for(u32 j = 0; j < nNumEntryImpacts; j++)
		{
			if(!impacts.batch[i].entryImpacts[j].GetHitDetected())
			{
				continue;
			}

			if (CAirDefenceManager::AreAirDefencesActive())
			{
				Vec3V vDefenceSphereHitPos;
				Vec3V vStart = RCC_VEC3V(m_vOriginPosition);
				Vec3V vEnd = impacts.batch[i].entryImpacts[j].GetHitPositionV();
				u8 uZoneId = 0;

				// If start point is within defence zone (gun poking through), skip it
				if (CAirDefenceManager::IsPositionInDefenceZone(vStart, uZoneId))
				{
					m_bCheckForAirDefenceZone = true;
					continue;
				}

				// If firing vector intersects zone, skip it
				if(CAirDefenceManager::DoesRayIntersectDefenceZone(vStart, vEnd, vDefenceSphereHitPos, uZoneId))
				{
					m_bCheckForAirDefenceZone = true;
					continue;
				}
			}

			CEntity* pCurrentEntity = CPhysics::GetEntityFromInst( impacts.batch[i].entryImpacts[j].GetHitInst() );
			if( pCurrentEntity )
			{
				if( pCurrentEntity->GetIsTypePed() )
				{
					bool bEnityExcluded = false;
					for( s32 k = 0; k < m_BulletCollisionPairs.GetCount(); k++ )
					{
						// Exclude peds that have 1 non-aesthetic bullet entry
						if( pCurrentEntity == m_BulletCollisionPairs[k].m_pEntity &&
							!m_BulletCollisionPairs[k].m_bAesthetic )
						{
							bEnityExcluded = true;
							break;
						}
					}

					// force a trace if this hit the local player ped
					CPed* pCurrentPed = static_cast<CPed*>(pCurrentEntity);

					//Exclude if I shot a friend
					if( m_pFiringEntity && m_pFiringEntity->GetIsTypePed() )
					{
						CPed* pFiringPed = static_cast<CPed*>(m_pFiringEntity.Get());
						if(pFiringPed->IsPlayer())
						{
							NOTFINAL_ONLY(u32 uDamageEntityRejectionReason = CPedDamageCalculator::DRR_None;)
							if(!pFiringPed->IsAllowedToDamageEntity( m_pWeaponInfo, pCurrentEntity NOTFINAL_ONLY(, &uDamageEntityRejectionReason)))
							{
								NOTFINAL_ONLY(bulletImpactRejectionDebug1f(uDamageEntityRejectionReason))
								bulletImpactRejectionDebug1f(CPedDamageCalculator::DRR_NoFriendlyFire)
								bEnityExcluded = true;
							}
						}
						else if(pFiringPed->GetPedIntelligence()->IsFriendlyWith(*pCurrentPed))
						{
							bulletImpactRejectionDebug1f(CPedDamageCalculator::DRR_Friendly)
							bEnityExcluded = true;
						}
					}

					if( pCurrentPed->IsLocalPlayer() )
					{
						m_Batch[i]->bCreatesTrace = true;
					}

					//exclude is they are out of range
					if(!g_LodMgr.WithinVisibleRange(pCurrentEntity, false, 0.5f) )
					{
						bulletImpactRejectionDebug1f(CPedDamageCalculator::DRR_OutOfRange)
						bEnityExcluded = true;
					}

					// if entity is not in exception list
					if( bEnityExcluded )
					{
						bulletImpactRejectionDebug2f(pCurrentPed, m_pFiringEntity, m_Batch[i]->uWeaponHash, impacts.batch[i].entryImpacts[j].GetComponent())
						continue;
					}
				}
				
				// some bullet impacts are disabled in MP (eg a player shooting a ghost entity)
				if (m_pFiringEntity && NetworkInterface::AreBulletsImpactsDisabledInMP(*m_pFiringEntity, *pCurrentEntity))
				{
#if !__FINAL
					if (pCurrentEntity->GetIsTypePed())
					{
						CPed* pCurrentPed = static_cast<CPed*>(pCurrentEntity);
						bulletImpactRejectionDebug1f(CPedDamageCalculator::DRR_ImpactsDisabledInMP)
						bulletImpactRejectionDebug2f(pCurrentPed, m_pFiringEntity, m_Batch[i]->uWeaponHash, impacts.batch[i].entryImpacts[j].GetComponent())
					}
#endif //__FINAL
					continue;
				}

				// Skip the impact if bullet hits propeller disc bound but not count as a hit
				if(pCurrentEntity->GetIsTypeVehicle() && ((CVehicle*)pCurrentEntity)->InheritsFromHeli() && 
					((CHeli*)pCurrentEntity)->DoesBulletHitPropellerBound(impacts.batch[i].entryImpacts[j].GetComponent()))
				{
					bool bEntityExcluded = false;
					u32 nNumExitImpacts = impacts.batch[i].exitImpacts.GetResultsReady() ? impacts.batch[i].exitImpacts.GetNumHits() : 0;
					if( nNumExitImpacts > 0 )
					{
						for( u32 m = 0; m < nNumExitImpacts; m++ )
						{
							// Otherwise find the first exit entry that is larger than 1.0f - tvalue
							if( 1.0f - impacts.batch[i].exitImpacts[m].GetHitTValue() > 
								impacts.batch[i].entryImpacts[j].GetHitTValue() )
							{
								// Exclude if hits the propeller bound but doesn't count as hit
								bEntityExcluded = !((CHeli*)pCurrentEntity)->DoesBulletHitPropeller(impacts.batch[i].entryImpacts[j].GetComponent(), impacts.batch[i].entryImpacts[j].GetHitPosition(), impacts.batch[i].exitImpacts[m].GetComponent(), impacts.batch[i].exitImpacts[m].GetHitPosition());						
								break;
							}
						}
					}
					else
					{
						// Exclude if hits the propeller bound but doesn't count as hit
						bEntityExcluded = !((CHeli*)pCurrentEntity)->DoesBulletHitPropeller(impacts.batch[i].entryImpacts[j].GetComponent(), impacts.batch[i].entryImpacts[j].GetHitPosition(), impacts.batch[i].entryImpacts[j].GetComponent(), impacts.batch[i].entryImpacts[j].GetHitPosition());
					}
					
					if(bEntityExcluded)
						continue;;
				}
			}

			// Search through the remaining impacts for a tyre impact close to the initial entry point. This is
			// necessary because we can have a bound which covers the wheel arch or part of it, but with no visible
			// component and we still need to be able to burst the tyre.
			bool bAddCollisionPair = true;

			static dev_float kfTyrePenetrationDepth = 0.25f;
			static dev_float kfTyreReducedPenetrationDepth = 0.03f;

			float tyrePenetrationDepth = kfTyrePenetrationDepth;

			if( pCurrentEntity &&
				pCurrentEntity->GetIsTypeVehicle() &&
				( pCurrentEntity->GetModelId() == MI_CAR_RUSTON || 
				  pCurrentEntity->GetModelId() == MI_CAR_XA21 || 
				  pCurrentEntity->GetModelId() == MI_CAR_VAGNER ) )
			{
				tyrePenetrationDepth = kfTyreReducedPenetrationDepth;
			}

			for( s32 g = j; g < nNumEntryImpacts; g++ )
			{
				if(PGTAMATERIALMGR->UnpackMtlId(impacts.batch[i].entryImpacts[g].GetHitMaterialId())==PGTAMATERIALMGR->g_idRubber)
				{
					// Is the tyre impact too deep to consider?
					float fPenetrationDist = (impacts.batch[i].entryImpacts[g].GetHitPosition() - impacts.batch[i].entryImpacts[j].GetHitPosition()).Mag();

					if(g>j && fPenetrationDist < tyrePenetrationDepth)
					{
						bAddCollisionPair = false;
					}
					break;
				}
			}


			// Append collision pair
			if( bAddCollisionPair && m_BulletCollisionPairs.GetCount() < MAX_BULLETS_COLLISION_PAIRS )
			{
				BulletCollisionPair& collisionContainer = m_BulletCollisionPairs.Append();
				collisionContainer.m_pEntity = pCurrentEntity;
				collisionContainer.m_pEntry = &impacts.batch[i].entryImpacts[j];
				collisionContainer.m_pExit = collisionContainer.m_pEntry;
				collisionContainer.m_bAesthetic = false;

				if( IsImpactAesthetic( collisionContainer.m_pEntity, collisionContainer.m_pEntry ) )
				{
					// Walk through the remaining entry impacts to verify whether or not we have a non-aesthetic hit
					for( s32 g = j; g < nNumEntryImpacts; g++ )
					{
						CEntity* pImpactEntity = CPhysics::GetEntityFromInst( impacts.batch[i].entryImpacts[g].GetHitInst() );
						if( pImpactEntity && collisionContainer.m_pEntity == pImpactEntity &&
							!IsImpactAesthetic( pImpactEntity, &impacts.batch[i].entryImpacts[g] ) )
						{
							collisionContainer.m_bAesthetic = true;
							break;
						}
					}
				}

				if( !collisionContainer.m_bAesthetic )
				{
					if( !IsImpactHeadShot( collisionContainer.m_pEntity, collisionContainer.m_pEntry ) )
					{
						// Walk through the remaining entry impacts to verify whether or not we have a head shot
						for( s32 g = j; g < nNumEntryImpacts; g++ )
						{
							CEntity* pImpactEntity = CPhysics::GetEntityFromInst( impacts.batch[i].entryImpacts[g].GetHitInst() );
							if( pImpactEntity && collisionContainer.m_pEntity == pImpactEntity &&
								IsImpactHeadShot( pImpactEntity, &impacts.batch[i].entryImpacts[g] ) )
							{
								collisionContainer.m_bAesthetic = true;
								break;
							}
						}
					}
				}
	
				u32 nNumExitImpacts = impacts.batch[i].exitImpacts.GetResultsReady() ? impacts.batch[i].exitImpacts.GetNumHits() : 0;
				if( nNumExitImpacts > 0 )
				{
					// If we are a ped we want to match up hit components
					if( collisionContainer.m_pEntity && collisionContainer.m_pEntity->GetIsTypePed() )
					{
						for( u32 m = 0; !bStuck && m < nNumExitImpacts; m++ )
						{
							u16 iExitHitComponent = impacts.batch[i].exitImpacts[m].GetHitComponent();

							// Make sure our entity matches and our hit component
							CEntity* pExitEntity = CPhysics::GetEntityFromInst( impacts.batch[i].exitImpacts[m].GetHitInst() );
							u16 iEntryHitComponent = collisionContainer.m_pEntry->GetHitComponent();
							if( pExitEntity && pExitEntity == collisionContainer.m_pEntity &&
								iEntryHitComponent == iExitHitComponent )
							{
								if( collisionContainer.m_bAesthetic )
								{
									collisionContainer.m_pExit = &impacts.batch[i].exitImpacts[m];
									break;
								}
								else if( ComputePassesThrough( collisionContainer.m_pEntry, &impacts.batch[i].exitImpacts[m], m_Batch[i]->fPenetration ) )						
								{
									collisionContainer.m_pExit = &impacts.batch[i].exitImpacts[m];
									continue;
								}

								// Otherwise bullet cannot penetrate surface and we are stuck
								bStuck = true;
							}
						}
					}
					else 
					{
						for( u32 m = 0; !bStuck && m < nNumExitImpacts; m++ )
						{
							// Otherwise find the first exit entry that is larger than 1.0f - tvalue
							if( 1.0f - impacts.batch[i].exitImpacts[m].GetHitTValue() > 
								collisionContainer.m_pEntry->GetHitTValue() )
							{
								if( ComputePassesThrough( collisionContainer.m_pEntry, &impacts.batch[i].exitImpacts[m], m_Batch[i]->fPenetration ) )						
								{
									collisionContainer.m_pExit = &impacts.batch[i].exitImpacts[m];
									break;
								}

								// Otherwise bullet cannot penetrate surface and we are stuck
								bStuck = true;
							}
						}
					}
				}
				// Assume we shoot through the ground
				// NOTE: I do not like this solution but should cover 95% of the cases. I would
				//		 rather use PGTAMATERIALMGR->GetIsShootThrough but materials are not 
				//		 tagged correctly.
				else
				{
					if( !collisionContainer.m_bAesthetic && !ComputePassesThrough( collisionContainer.m_pEntry, collisionContainer.m_pExit, m_Batch[i]->fPenetration ) )						
					{
						bStuck = true;
					}
				}


				// Did the bullet stop?
				if( bStuck )
				{
					// Set the bullets position to the intersection at which it became stuck
					m_Batch[i]->vPosition = collisionContainer.m_pExit->GetHitPosition();
					
					// Remove next frame (so that script can still detect the bullet. Bug was that script couldn't detect bullets that were created and removed in the same frame)
					m_Batch[i]->bToBeRemoved = true;

					// Generate AI events - once we know the impact point this is the limit of the line tests used to check if a bullet's path came near a ped.
					GenerateWhizzedByEventAtEndPosition(m_Batch[i]->vPosition);
					break;
				}
			}
		}

		int nPairCount = m_BulletCollisionPairs.GetCount();

#if __BANK
		if( CWeaponDebug::GetRenderDebugBullets() )
		{
			for(int iBulletPair = 0; iBulletPair < nPairCount; ++iBulletPair)
			{
				// Entry 
				CWeaponDebug::ms_debugStore.AddSphere( RCC_VEC3V( m_BulletCollisionPairs[iBulletPair].m_pEntry->GetHitPosition() ), 0.05f, Color_green, 2000, 0, false );

				// Last Exit point
				if( iBulletPair == ( nPairCount - 1 ) )
				{
					CWeaponDebug::ms_debugStore.AddSphere( RCC_VEC3V( m_BulletCollisionPairs[iBulletPair].m_pExit->GetHitPosition() ), 0.075f, Color_red, 2000, 0, true );
				}
				// All other exit points
				else
				{
					CWeaponDebug::ms_debugStore.AddSphere( RCC_VEC3V( m_BulletCollisionPairs[iBulletPair].m_pExit->GetHitPosition() ), 0.075f, Color_blue, 2000, 0, true );
				}
			}
		}
#endif // __BANK

		if( nPairCount > 0 )
		{
			// Create a weapon
			CWeapon tempWeapon(m_pWeaponInfo->GetHash(), 1);
			tempWeapon.SetIsSilenced(m_bSilenced);

			// Copy only the valid entry impacts to pass through to produce damage / effects.
			WorldProbe::CShapeTestFixedResults<MAX_BULLETS_COLLISION_PAIRS> validHits;

			int iInsertIdx = 0;
			for(int iEntRes = 0; iEntRes < nPairCount; ++iEntRes)
			{
				if( m_BulletCollisionPairs[iEntRes].m_bAesthetic && m_BulletCollisionPairs[iEntRes].m_pEntry && m_BulletCollisionPairs[iEntRes].m_pEntry->GetHitInst())
				{
					// Just apply the bullet vfx
					CWeaponDamage::DoWeaponImpactVfx(&tempWeapon, m_pFiringEntity, m_vOriginPosition, m_BulletCollisionPairs[iEntRes].m_pEntry, m_fDamage, CPedDamageCalculator::DF_None, false, false);
					continue;
				}
				
				validHits[iInsertIdx++].Copy( *m_BulletCollisionPairs[iEntRes].m_pEntry );
			}

			// Update the hit counter in the container.
			validHits.Update();

			fwFlags32 flags;
			if( m_Batch[i]->bIsAccurate )
				flags.SetFlag( CPedDamageCalculator::DF_IsAccurate );

			// Do damage
			if(!isUnderWater)
			{
				isUnderWater = ProcessHitsOceanWater(i);
			}

			if (m_bIsBentBulletFromAimAssist)
			{
				flags.SetFlag(CPedDamageCalculator::DF_DamageFromBentBullet);
			}

			// Find out if we hit a ped's weapon and this was extended to cause damage to the ped holding it...
			CWeaponDamage::NetworkWeaponImpactInfo networkWeaponImpactInfo;
			CWeaponDamage::DoWeaponImpact(&tempWeapon, m_pFiringEntity, m_vOriginPosition, validHits, m_fDamage, flags, false, false, NULL, 0, isUnderWater, &networkWeaponImpactInfo, m_fRecoilAccuracyWhenFired, m_fFallOffRangeModifier, m_fFallOffDamageModifier);

			// Network
			if(NetworkInterface::IsGameInProgress() && m_pFiringEntity && !NetworkUtils::IsNetworkClone(m_pFiringEntity))
			{
				tempWeapon.SendFireMessage(m_pFiringEntity, m_vOriginPosition, validHits, nPairCount, true, m_fDamage, flags, 0, 0, 0, &networkWeaponImpactInfo, m_fRecoilAccuracyWhenFired, m_fFallOffRangeModifier, m_fFallOffDamageModifier);
			}

			// Record valid bullet impacts for script querying
			for(s32 i = 0; i < nPairCount; i++)
			{
				if(validHits[i].IsAHit())
				{

					// If firing entity is a vehicle, attribute bullet impact to ped with vehicle weapon equipped
					if (m_pFiringEntity && m_pFiringEntity->GetIsTypeVehicle() && m_pWeaponInfo->GetIsVehicleWeapon())
					{
						const CVehicle* pVehicle = static_cast<CVehicle*>(m_pFiringEntity.Get());
						const int iNumSeats = pVehicle->GetSeatManager()->GetMaxSeats();
						for (int iSeat = 0; iSeat < iNumSeats; ++iSeat)
						{
							if (pVehicle->GetSeatHasWeaponsOrTurrets(iSeat))
							{
								CPed* pPed = pVehicle->GetSeatManager()->GetPedInSeat(iSeat);
								if (pPed)
								{
									const CVehicleWeapon* pVehWeapon = pPed->GetWeaponManager()->GetEquippedVehicleWeapon();
									if (pVehWeapon && pVehWeapon->GetHash() == m_pWeaponInfo->GetHash())
									{
										CBulletManager::AddBulletImpact(validHits[i], pPed, true);
										break;
									}
								}
							}
						}		
					}
					else
					{
						CBulletManager::AddBulletImpact(validHits[i], m_pFiringEntity, true);
					}

#if __DEV
					// Record that we have hit a ped
					if(m_WeaponOwner && m_WeaponOwner->m_pDebugData)
					{
						if(validHits[i].GetHitEntity() && validHits[i].GetHitEntity()->GetIsTypePed())
						{
							m_WeaponOwner->m_pDebugData->m_iAccuracyShotsHit++;
						}
					}
#endif // __DEV
				}
			}

// 			// Do exit effects
// 			for(s32 j = 0; j < nPairCount; j++)
// 			{
// 				// Impacts can sometimes become invalidated by this point. For example, impacts with weapons which
// 				// get swapped out for a CPickup version when the ped is killed.
// 				WorldProbe::CShapeTestHitPoint* pResult = m_BulletCollisionPairs[j].m_pExit;
// 				if(pResult && pResult != m_BulletCollisionPairs[j].m_pEntry && pResult->GetHitDetected() && pResult->GetHitInst())
// 				{
// 					CWeaponDamage::DoWeaponImpactVfx(&tempWeapon, m_pFiringEntity, m_vOriginPosition, pResult, fApplyDamage, false, false, true);
// 				}
// 			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CBullet::ProcessVfx()
{
	PF_AUTO_PUSH_TIMEBAR("CBullet ProcessVfx");

	Vec3V vMuzzlePos = RCC_VEC3V(m_vMuzzlePosition);
	bool isFirstPerson = false;

	// when the bullet was added the ped hadn't been updated yet so the stored muzzle position is a frame behind
	// by now the ped matrices have been updated and re-stored so we try to get the updated muzzle position again here
	// the weapon pointer is only needed for debug bullet trace attachment so only try to get this for peds
	CWeapon* pWeapon = NULL;
	if (m_pFiringEntity && m_pFiringEntity->GetIsTypePed())
	{
		CPed* pPed = static_cast<CPed*>(m_pFiringEntity.Get());
		if (pPed)
		{
			isFirstPerson = pPed->IsFirstPersonShooterModeEnabledForPlayer(false);

			CPedWeaponManager* pPedWeaponManager = pPed->GetWeaponManager();
			if (pPedWeaponManager)
			{
				pWeapon = pPedWeaponManager->GetEquippedWeapon();
				if (pWeapon)
				{
					Mat34V vWeaponMtx = Mat34V(V_IDENTITY);
					vWeaponMtx.SetCol3(RCC_VEC3V(m_vMuzzlePosition));
					pWeapon->GetMuzzlePosition(RCC_MATRIX34(vWeaponMtx), RC_VECTOR3(vMuzzlePos));
				}
			}
		}
	}

	for(s32 i = 0; i < m_Batch.GetCount(); i++)
	{
		//grcDebugDraw::Line(m_Batch[i]->vOldPosition, m_Batch[i]->vPosition, Color32(1.0f, 0.0f, 0.0f, 1.0f), Color32(1.0f, 1.0f, 0.0f, 1.0f));

		if(m_Batch[i]->bCreatesTrace)
		{
			//Check if the angle is too great between the muzzle start position and the weapons forward vector and the trace vector.
			// MN - only do this when the gun being fired is close enough to notice the problem (url:bugstar:1245475)
			if (!g_vfxWeapon.GetBulletTraceNoAngleReject() && !m_pWeaponInfo->GetIgnoreTracerVfxMuzzleDirectionCheck())
			{
				float distSqrToCam = CVfxHelper::GetDistSqrToCamera(vMuzzlePos);
				if (distSqrToCam<15.0f*15.0f)
				{
					if(m_Batch[i]->bCreatesTrace && m_Batch[i]->vOldPosition == m_vOriginPosition && !m_pWeaponInfo->GetIsVehicleWeapon())
					{
						Vector3 vWeaponFire = m_vMuzzleDirection;
						vWeaponFire.NormalizeSafe();
						Vector3 vBulletFire = m_Batch[i]->vPosition - RC_VECTOR3(vMuzzlePos);
						vBulletFire.NormalizeSafe();

						float fAngle = vWeaponFire.Dot(vBulletFire);
						float fThreshold = isFirstPerson ? VFXWEAPON_TRACE_ANGLE_REJECT_THRESH_FPS : VFXWEAPON_TRACE_ANGLE_REJECT_THRESH;
						if(fAngle < fThreshold)
						{
							m_Batch[i]->bCreatesTrace = false;
							break;
						}
					}
				}
			}

			// VFX tracer is crashing occasionally due to bad input from the shape test. I haven't been able to repro it so I'm adding
			// this test to avoid the crash.
			if(Verifyf(rage::FPIsFinite(m_Batch[i]->vOldPosition.x) &&
				rage::FPIsFinite(m_Batch[i]->vOldPosition.y) &&
				rage::FPIsFinite(m_Batch[i]->vOldPosition.z) &&
				rage::FPIsFinite(m_Batch[i]->vPosition.x) &&
				rage::FPIsFinite(m_Batch[i]->vPosition.y) &&
				rage::FPIsFinite(m_Batch[i]->vPosition.z),
				"At least one of the input vectors to tracer vfx is invalid for batch %d:\nvOldPosition = (%7.3f,%7.3f,%7.3f)\nvPosition=(%7.3f,%7.3f,%7.3f)",
				i,
				m_Batch[i]->vOldPosition.x,m_Batch[i]->vOldPosition.y,m_Batch[i]->vOldPosition.z,
				m_Batch[i]->vPosition.x,m_Batch[i]->vPosition.y,m_Batch[i]->vPosition.z))
			{
				// check if we're in first person aiming mode
				bool isFirstPersonAiming = false;
				if (m_pFiringEntity && camInterface::GetGameplayDirector().IsFirstPersonAiming(m_pFiringEntity))
				{
					isFirstPersonAiming = true;
				}

				// the first bullet trace segment should come from the muzzle position rather than the bullet start position
				// when not in first person aiming mode
				bool recordTracer = false;
				if (m_Batch[i]->vOldPosition==m_vOriginPosition && !isFirstPersonAiming)
				{
					recordTracer = g_vfxWeapon.TriggerPtFxBulletTrace(m_pFiringEntity, pWeapon, m_pWeaponInfo, vMuzzlePos, RCC_VEC3V(m_Batch[i]->vPosition));

#if GTA_REPLAY
					if(recordTracer && CReplayMgr::ShouldRecord())
					{
						Vec3V vTraceDir = RCC_VEC3V(m_Batch[i]->vPosition)-vMuzzlePos;
						float traceLen = Mag(vTraceDir).Getf();
						vTraceDir = Normalize(vTraceDir);

						if(pWeapon && pWeapon->GetEntity())
						{
							atHashWithStringNotFinal effectRuleHashName = m_pWeaponInfo->GetBulletTracerPtFxHashName();
							if (pWeapon && pWeapon->GetClipComponent())
							{
								atHashWithStringNotFinal tracerOverrideHashName = pWeapon->GetClipComponent()->GetInfo()->GetTracerFxHashName();
								if (tracerOverrideHashName.IsNotNull())
								{
									effectRuleHashName = tracerOverrideHashName;
								}
							}

							CReplayMgr::RecordFx<CPacketWeaponBulletTraceFx2>(
								CPacketWeaponBulletTraceFx2(effectRuleHashName,
								RCC_VECTOR3(vTraceDir),
								traceLen,
								g_vfxWeapon.GetWeaponTintColour(pWeapon->GetTintIndex(), pWeapon->GetAmmoInClip())), 
								pWeapon->GetEntity());
						}
						else
						{
							CReplayMgr::RecordFx<CPacketWeaponBulletTraceFx>(
								CPacketWeaponBulletTraceFx(m_pWeaponInfo->GetBulletTracerPtFxHashName(),
								RCC_VECTOR3(vMuzzlePos),
								RCC_VECTOR3(vTraceDir),
								traceLen));
						}
					}
#endif
				}
				else
				{
					recordTracer = g_vfxWeapon.TriggerPtFxBulletTrace(m_pFiringEntity, pWeapon, m_pWeaponInfo, RCC_VEC3V(m_Batch[i]->vOldPosition), RCC_VEC3V(m_Batch[i]->vPosition));

#if GTA_REPLAY
					if(recordTracer && CReplayMgr::ShouldRecord())
					{
						Vec3V vTraceDir = RCC_VEC3V(m_Batch[i]->vPosition)-RCC_VEC3V(m_Batch[i]->vOldPosition);
						float traceLen = Mag(vTraceDir).Getf();
						vTraceDir = Normalize(vTraceDir);

						CReplayMgr::RecordFx<CPacketWeaponBulletTraceFx>(
							CPacketWeaponBulletTraceFx(m_pWeaponInfo->GetBulletTracerPtFxHashName(),
							m_Batch[i]->vOldPosition,
							RCC_VECTOR3(vTraceDir),
							traceLen));
					}
#endif
				}
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
bool CBullet::ProcessHitsOceanWater(u32 index)
{
		Vec3V vWaterPos;
		if (CVfxHelper::TestLineAgainstOceanWater(RCC_VEC3V(m_Batch[index]->vOldPosition), RCC_VEC3V(m_Batch[index]->vPosition), vWaterPos))
		{
			// set up the collision info structure
			VfxCollInfo_s vfxCollInfo;
			vfxCollInfo.regdEnt = NULL;
			vfxCollInfo.vPositionWld = vWaterPos;
			vfxCollInfo.vNormalWld = Vec3V(V_Z_AXIS_WZERO);
			vfxCollInfo.vDirectionWld = RCC_VEC3V(m_Batch[index]->vPosition) - RCC_VEC3V(m_Batch[index]->vOldPosition);
			vfxCollInfo.vDirectionWld = Normalize(vfxCollInfo.vDirectionWld);
			vfxCollInfo.materialId = 0;
			vfxCollInfo.roomId = 0;
			vfxCollInfo.componentId = 0;
			vfxCollInfo.weaponGroup = m_pWeaponInfo->GetEffectGroup();
			vfxCollInfo.force = VEHICLEGLASSFORCE_NOT_USED;
			vfxCollInfo.isBloody = false;
			vfxCollInfo.isWater = true;
			vfxCollInfo.isExitFx = false;
			vfxCollInfo.noPtFx = false;
			vfxCollInfo.noPedDamage = false;
			vfxCollInfo.noDecal = false;
			vfxCollInfo.isSnowball = false;
			vfxCollInfo.isFMJAmmo = false;

			// play any weapon impact effects
			g_vfxWeapon.DoWeaponImpactVfx(vfxCollInfo, m_pWeaponInfo->GetDamageType(), m_pFiringEntity);

			// trigger audio
			audSoundInitParams initParams;
			initParams.Position = RCC_VECTOR3(vWaterPos);
			g_WeaponAudioEntity.CreateAndPlaySound(ATSTRINGHASH("WATER_BULLET_IMPACTS", 0x929FDDDE), &initParams);
			return true;
		}
		return false;
}

////////////////////////////////////////////////////////////////////////////////
bool CBullet::ProcessHitsAirDefenceZone(u32 index)
{
	// Only block bullets if the firing entity is a valid ped (ie NOT the air defence turret).
	if (CAirDefenceManager::AreAirDefencesActive() && m_pFiringEntity && (m_pFiringEntity->GetIsTypePed() || m_pFiringEntity->GetIsTypeVehicle()))
	{
		Vec3V vStart = RCC_VEC3V(m_vOriginPosition);
		Vec3V vEnd = RCC_VEC3V(m_Batch[index]->vPosition);
		Vec3V vDefenceZoneHitPos = vStart;
		u8 uDefenceZoneIndex = 0;
		
		if (CAirDefenceManager::IsPositionInDefenceZone(vStart, uDefenceZoneIndex) || CAirDefenceManager::DoesRayIntersectDefenceZone(vStart, vEnd, vDefenceZoneHitPos, uDefenceZoneIndex))
		{
			CAirDefenceZone *pDefZone = CAirDefenceManager::GetAirDefenceZone(uDefenceZoneIndex);
			if (pDefZone && pDefZone->IsEntityTargetable(m_pFiringEntity))
			{
				// set up the collision info structure
				VfxCollInfo_s vfxCollInfo;
				vfxCollInfo.regdEnt = NULL;
				vfxCollInfo.vPositionWld = vDefenceZoneHitPos;
				vfxCollInfo.vNormalWld = Vec3V(V_Z_AXIS_WZERO);
				vfxCollInfo.vDirectionWld = RCC_VEC3V(m_Batch[index]->vPosition) - RCC_VEC3V(m_Batch[index]->vOldPosition);
				vfxCollInfo.vDirectionWld = Normalize(vfxCollInfo.vDirectionWld);
				vfxCollInfo.materialId = 0;
				vfxCollInfo.roomId = 0;
				vfxCollInfo.componentId = 0;
				vfxCollInfo.weaponGroup = m_pWeaponInfo->GetEffectGroup();
				vfxCollInfo.force = VEHICLEGLASSFORCE_NOT_USED;
				vfxCollInfo.isBloody = false;
				vfxCollInfo.isWater = false;
				vfxCollInfo.isExitFx = false;
				vfxCollInfo.noPtFx = false;
				vfxCollInfo.noPedDamage = false;
				vfxCollInfo.noDecal = false;
				vfxCollInfo.isSnowball = false;
				vfxCollInfo.isFMJAmmo = false;

				// play any weapon impact effects
				g_vfxWeapon.DoWeaponImpactVfx(vfxCollInfo, m_pWeaponInfo->GetDamageType(), m_pFiringEntity);

				// Set bullet pos/flags and generate events (as we do normally in CBullet::ProcessImpacts):
				// Set the bullets position to the intersection at which it became stuck
				m_Batch[index]->vPosition = VEC3V_TO_VECTOR3(vDefenceZoneHitPos);

				// Remove next frame (so that script can still detect the bullet. Bug was that script couldn't detect bullets that were created and removed in the same frame)
				m_Batch[index]->bToBeRemoved = true;

				// Generate AI events - once we know the impact point this is the limit of the line tests used to check if a bullet's path came near a ped.
				GenerateWhizzedByEventAtEndPosition(m_Batch[index]->vPosition);

				// Shoot at the impact position.
				if (pDefZone->ShouldFireWeapon())
				{
					Vec3V vFiredFromPosition(V_ZERO);
					CAirDefenceManager::FireWeaponAtPosition(uDefenceZoneIndex, vDefenceZoneHitPos, vFiredFromPosition);
				}

#if __BANK
				if (CAirDefenceManager::ShouldRenderDebug())
				{
					grcDebugDraw::Sphere(vDefenceZoneHitPos, 0.05f, Color_blue, true, 100);
				}
#endif	// __BANK

				return true;
			}
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

f32 CBullet::ComputePassesThrough(const WorldProbe::CShapeTestHitPoint* entryHitPoint, const WorldProbe::CShapeTestHitPoint* exitHitPoint, f32& fInOutPenetration) const
{
	weaponAssertf(entryHitPoint, "Inavlid Entry Impact");
	weaponAssertf(entryHitPoint->GetHitDetected(), "Invalid intersection");

	bool bIsPed = false;
	bool bIsVehicle = false;

	//Cache if this is an electric weapon hitting glass.
	bool bElectricHittingGlass = false;
	bool bHitGlass = PGTAMATERIALMGR->GetIsGlass(entryHitPoint->GetMaterialId());
	if(bHitGlass && (m_pWeaponInfo->GetDamageType()==DAMAGE_TYPE_ELECTRIC))
	{
		bElectricHittingGlass = true;
	}

	//Allow electric to pass through glass in certain instances.
	bool bShootingSmashedGlass = false;
	bool bBulletProof = false;
	const CEntity* pHitEntity = CPhysics::GetEntityFromInst(entryHitPoint->GetHitInst());
	if(pHitEntity)
	{
		if(pHitEntity->GetIsPhysical())
		{
			const CPhysical* pHitPhysical = static_cast<const CPhysical*>(pHitEntity);

			if(pHitPhysical->m_nPhysicalFlags.bNotDamagedByBullets)
			{
				// Bullet proof glass: bullet stops here				
				bBulletProof = true;
			}

			if(pHitPhysical->GetIsTypeObject() && !bBulletProof)
			{
				const CObject* pObj = static_cast<const CObject*>(pHitPhysical);

				if(pObj->m_nObjectFlags.bIsRageEnvCloth)
				{
					// Bullets pass through cloth
					return true;
				}

				if(pObj->CanShootThroughWhenEquipped())
				{
					return true;
				}
			}	

			if(pHitPhysical->GetIsTypePed())
			{
				bIsPed = true;
			}

			if(pHitPhysical->GetIsTypeVehicle())
			{
				bIsVehicle = true;
			}

			if(bIsVehicle)
			{	
				const CVehicle* pVehicle = static_cast<const CVehicle*>(pHitPhysical);

				//! if m_bPerformBulletProofGlassPenetration is set it means that we previously would have ignored bullets against our own vehicles, but now want to generate hits against it so that we
				//! can apply decals etc.
				if(m_bPerformBulletProofGlassPenetration && m_pFiringEntity && m_pFiringEntity->GetIsTypePed() && static_cast<CPed*>(m_pFiringEntity.Get())->GetVehiclePedInside() == pVehicle)
				{
					return true;
				}

				if (bElectricHittingGlass)
				{				
					if(m_pFiringEntity && m_pFiringEntity->GetIsTypePed() && static_cast<CPed*>(m_pFiringEntity.Get())->GetVehiclePedInside() == pVehicle)
					{
						bShootingSmashedGlass = true;
					}		
					else
					{
						fragInst* pFragInst = pHitPhysical->GetFragInst();
						if(pFragInst && pFragInst->GetCacheEntry()->GetHierInst())
						{
							if( PGTAMATERIALMGR->UnpackMtlId( entryHitPoint->GetMaterialId() ) != PGTAMATERIALMGR->g_idCarVoid)
							{							
								//	If this vehicle part is broken then allow to shoot through.
								if (pVehicle->IsBrokenFlagSet(entryHitPoint->GetComponent()))
								{
									bShootingSmashedGlass = true;
								}
							}
						}
					}
				}
				else if(PGTAMATERIALMGR->GetIsGlass(entryHitPoint->GetMaterialId()))
				{
					// B*1976914: Allow peds in a vehicle to shoot through the glass windows even if the vehicle has been flagged as bullet proof.
					// This allows in-vehicle shooting to work correctly in first person when driving bullet proof vehicles.
					if(m_pFiringEntity && m_pFiringEntity->GetIsTypePed() && static_cast<CPed*>(m_pFiringEntity.Get())->GetVehiclePedInside() == pHitPhysical)
						bBulletProof = false;

					// B*2765189 - Allow bullets to pass through non bulletproof windows, even if vehicle is marked as bulletproof
					if (!pVehicle->HasBulletResistantGlass())
					{
						bBulletProof = false;
					}
				}
			}
		}
	}

	CPed* pFiringPed = (m_pFiringEntity.Get() && m_pFiringEntity.Get()->GetType() == ENTITY_TYPE_PED) ? static_cast<CPed*>(m_pFiringEntity.Get()) : NULL;

	// Allow Full Metal Jacket rounds to penetrate all types of vehicle glass
	if (bHitGlass && bIsVehicle && sm_bSpecialAmmoFMJVehicleGlassAlwaysPenetrate)
	{
		const CAmmoInfo* pAmmoInfo = pFiringPed ? m_pWeaponInfo->GetAmmoInfo(pFiringPed) : NULL;
		if (pAmmoInfo && pAmmoInfo->GetIsFMJ())
		{
			return true;
		}
	}

	// Armoured glass checks.
	if (bHitGlass && bIsVehicle && !bBulletProof && pHitEntity &&
		(PGTAMATERIALMGR->UnpackMtlId(entryHitPoint->GetHitMaterialId()) == PGTAMATERIALMGR->g_idCarGlassStrong) )
	{
		const CVehicle* pVehicle = static_cast<const CVehicle*>(pHitEntity);
		if (pVehicle && pVehicle->HasBulletResistantGlass())
		{
			// Don't pass through if window is still intact.
			u16 uHitComponent = entryHitPoint->GetHitComponent();
			
			const CVehicleGlassComponent *pGlassComponent = g_vehicleGlassMan.GetVehicleGlassComponent((CPhysical*)pVehicle, uHitComponent);

			bool bCanComponentBreak = CVehicleGlassManager::CanComponentBreak(pVehicle, uHitComponent) && !pVehicle->IsBrokenFlagSet(uHitComponent);
			// If glass component is null it means it hasn't been initialised yet (from CVehicleGlassManager::ProcessCollision),
			// so just treat it as if it has full health.
			if ( (!pGlassComponent && bCanComponentBreak) || (pGlassComponent && pGlassComponent->GetArmouredGlassHealth() > 0.0f) )
			{
				return false;
			}
		}
	}

	if(bBulletProof)
	{
		return false;
	}

	bool bPerformVehiclePenetration = bIsVehicle && m_pWeaponInfo->GetPenetrateVehicles() && m_bPerformPenetration;

	if (bElectricHittingGlass && !bShootingSmashedGlass)
	{
		return false;
	}
	
	if(PGTAMATERIALMGR->GetMtlFlagShootThroughFx(entryHitPoint->GetHitMaterialId()))
	{
		// Flagged to pass through
		return true;
	}

	// B*1911245: Allow bullets to penetrate glass if reset flag has been set
	if(bHitGlass && pFiringPed && pFiringPed->GetPedResetFlag(CPED_RESET_FLAG_UseBulletPenetrationForGlass))
	{
		return true;
	}

	// B*1911245: Allow the bullet to penetrate if we hit glass which is tagged as bulletproof but also breakable (badly tagged data) and we're using a sniper rifle
	phMaterialMgr::Id unpackedMtlId = PGTAMATERIALMGR->UnpackMtlId(entryHitPoint->GetHitMaterialId());
	bool bHitBulletProofGlass = bHitGlass && (unpackedMtlId == PGTAMATERIALMGR->g_idGlassBulletproof);
	if(bHitBulletProofGlass && pFiringPed && pFiringPed->GetWeaponManager() && pFiringPed->GetWeaponManager()->GetEquippedWeaponHasFirstPersonScope())
	{
		// Is the glass breakable?
		fragInst* pFragInst = pHitEntity ? pHitEntity->GetFragInst() : NULL;
		if(pFragInst)
		{
			const fragPhysicsLOD* physics = pFragInst->GetTypePhysics();
			int numGroups = physics ? physics->GetNumChildGroups() : 0;

			// If we have more than 1 child frag groups then the glass is breakable
			if(numGroups > 1)
			{
				return true;
			}
		}
	}
	// We no longer penetrate non-ped materials
	else if(!bIsPed && (!m_bPerformPenetration || !bPerformVehiclePenetration))
	{
		return false;
	}

	f32 fMaterialResistance = PGTAMATERIALMGR->GetMtlPenetrationResistance(entryHitPoint->GetHitMaterialId());
	if(fMaterialResistance == 0.0f)
	{
		// Flagged to pass through
		return true;
	}

	// The penetration amount - determines how far we can travel through this material with out current penetration
	f32 fPenetrationAmount = fInOutPenetration / fMaterialResistance;

	// Work out the dist between entry/exit point
	dev_float fMinPenetrationDist = 0.15f;
	f32 fDist = Max( fMinPenetrationDist, entryHitPoint->GetHitPosition().Dist( exitHitPoint->GetHitPosition() ) );
	if(fPenetrationAmount < fDist)
	{
		// If penetration amount is less than the current distance, we don't pass through
		return false;
	}
	else
	{
		// Reduce the penetration so we don't pass through as much next time
		fInOutPenetration -= fDist * fMaterialResistance;
		fInOutPenetration  = Max(0.0f, fInOutPenetration);

		// Passes through
		return true;
	}
}

void CBullet::GenerateWhizzedByEvents() const
{
	weaponDebugf3("CBullet::GenerateWhizzedByEvents invoke n GenerateWhizzedByEventAtEndPosition");

	for(s32 i=0; i < m_Batch.GetCount(); i++)
	{
		Vector3 vEndPosition = m_Batch[i]->vPosition;
		GenerateWhizzedByEventAtEndPosition(vEndPosition);
	}
}

void CBullet::GenerateWhizzedByEventAtEndPosition(const Vector3& vEndPosition) const
{
	if (NetworkInterface::IsGameInProgress() && m_pWeaponInfo->GetIsNonLethal())
	{
		return;
	}

	weaponDebugf3("CBullet::GenerateWhizzedByEventAtEndPosition invoke CEventGunShotWhizzedBy");

	CEventGunShotWhizzedBy eventGunShotWhizzedBy(m_pFiringEntity, m_vMuzzlePosition, vEndPosition, m_bSilenced, m_pWeaponInfo->GetHash());
	GetEventGlobalGroup()->Add(eventGunShotWhizzedBy);

	weaponDebugf3("CBullet::GenerateWhizzedByEventAtEndPosition invoke CEventFriendlyFireNearMiss");

	CEventFriendlyFireNearMiss eventFriendlyNearMiss(m_pFiringEntity, m_vMuzzlePosition, vEndPosition, m_bSilenced, m_pWeaponInfo->GetHash(), CEventFriendlyFireNearMiss::FNM_BULLET_WHIZZED_BY);
	GetEventGlobalGroup()->Add(eventFriendlyNearMiss);
}

////////////////////////////////////////////////////////////////////////////////
// CBullet::sBulletInstance
////////////////////////////////////////////////////////////////////////////////

CBullet::sBulletInstance::sBulletInstance(const CWeaponInfo* pWeaponInfo, const Vector3& vStart, const Vector3& vEnd, const f32 fVelocity, u32 uWeaponHash, bool bCreatesTrace, bool bIsAccurate)
: vPosition(vStart)
, vOldPosition(vStart)
, uWeaponHash(uWeaponHash)
, bToBeRemoved(false)
, bCreatesTrace(bCreatesTrace)
, bIsAccurate(bIsAccurate)
#if __BANK
, uGenerationId(s_uNewGenerationId++)
#endif // __BANK
{
	weaponFatalAssertf(pWeaponInfo, "pWeaponInfo is NULL");

	vVelocity = vEnd - vStart;
	vVelocity.NormalizeFast();
	vVelocity *= fVelocity;

	Assertf(IsFiniteAll(RCC_VEC3V(vVelocity)) && IsFiniteAll(RCC_VEC3V(vStart)) && IsFiniteAll(RCC_VEC3V(vEnd)),	
		"Invalid bullet parameters on weapon '%s':"
		"\tStart:	<%5.2f, %5.2f, %5.2f>"
		"\tEnd:		<%5.2f, %5.2f, %5.2f>"
		"\tVelocity: %5.2f",
		pWeaponInfo->GetName(),VEC3V_ARGS(RCC_VEC3V(vStart)),VEC3V_ARGS(RCC_VEC3V(vEnd)),fVelocity);

	// Store the weapon penetration - this will get reduced if we hit anything
	fPenetration = pWeaponInfo->GetPenetration();
}

////////////////////////////////////////////////////////////////////////////////
// CBulletManager
////////////////////////////////////////////////////////////////////////////////

// Statics
bool CBulletManager::ms_bUseBatchTestsForSingleShotWeapons = true;
bool CBulletManager::ms_bUseUndirectedBatchTests = true;
CBulletManager::Bullets CBulletManager::ms_Bullets;
CBulletManager::BulletImpacts CBulletManager::ms_EntryImpacts;
CBulletManager::BulletImpacts CBulletManager::ms_ExitImpacts;
CBulletManager::BulletEntities CBulletManager::ms_EntryEntities;
CBulletManager::BulletEntities CBulletManager::ms_ExitEntities;

////////////////////////////////////////////////////////////////////////////////

void CBulletManager::Init()
{
	CBullet::Init();
	ResetBullets();
	ResetBulletImpacts();
}

////////////////////////////////////////////////////////////////////////////////

void CBulletManager::Shutdown()
{
	ResetBulletImpacts();
	ResetBullets();
	CBullet::Shutdown();
}

////////////////////////////////////////////////////////////////////////////////

bool CBulletManager::Add(CBullet* pBullet)
{	
	if(ms_Bullets.IsFull())
	{
		// Delete
		delete pBullet;

		// No free slots
		return false;
	}

	ms_Bullets.Push(pBullet);
	return true;
}

////////////////////////////////////////////////////////////////////////////////

void CBulletManager::Process()
{
	PF_AUTO_PUSH_TIMEBAR("CBulletManager Process");
	ResetBulletImpacts();

	for(s32 i = 0; i < ms_Bullets.GetCount(); i++)
	{
		if(!ms_Bullets[i]->Process())
		{
			// Delete
			delete ms_Bullets[i];

			// Not currently active, so delete
			ms_Bullets.DeleteFast(i--);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CBulletManager::AddBulletImpact(WorldProbe::CShapeTestHitPoint& impact, CEntity* pFiringEntity, bool bIsEntryImpact)
{
	//return early if the array is full, but this should never happen.
	//the arrays should be sized to hold the maximum number of bullet impacts
	//that can be generated in a frame
	if (bIsEntryImpact && !Verifyf(!ms_EntryImpacts.IsFull(), "CBulletManager::ms_EntryImpacts is full, think about increasing its size!"))
	{
		return;
	}
	else if (!bIsEntryImpact && !Verifyf(!ms_ExitImpacts.IsFull(), "CBulletManager::ms_ExitImpacts is full, think about increasing its size!"))
	{
		return;
	}

	if (bIsEntryImpact)
	{
		ms_EntryImpacts.Push(impact.GetHitPosition());
		ms_EntryEntities.Push(pFiringEntity);
	}
	else
	{
		ms_ExitImpacts.Push(impact.GetHitPosition());
		ms_ExitEntities.Push(pFiringEntity);
	}
}

////////////////////////////////////////////////////////////////////////////////

void CBulletManager::ResetBullets()
{
	// Cleanup the bullets
	for(s32 i = 0; i < ms_Bullets.GetCount(); i++)
	{
		delete ms_Bullets[i];
	}
	ms_Bullets.Reset();
}

////////////////////////////////////////////////////////////////////////////////

void CBulletManager::ResetBulletImpacts()
{
	ms_EntryImpacts.Reset();
	ms_EntryEntities.Reset();
	ms_ExitImpacts.Reset();
	ms_ExitEntities.Reset();
}

////////////////////////////////////////////////////////////////////////////////

bool CBulletManager::ComputeIsBulletInAngledArea(Vector3& vPos1, Vector3& vPos2, float areaWidth, const CEntity* pFiringEntity)
{
	for(s32 i = 0; i < ms_Bullets.GetCount(); i++)
	{
		if(ms_Bullets[i]->ComputeIsBulletInAngledArea(vPos1, vPos2, areaWidth, pFiringEntity))
		{
			return true;
		}
	}

	return false;
}


////////////////////////////////////////////////////////////////////////////////

bool CBulletManager::ComputeIsBulletInSphere(const Vector3& vPosition, f32 fRadius, const CEntity* pFiringEntity)
{
	for(s32 i = 0; i < ms_Bullets.GetCount(); i++)
	{
		if(ms_Bullets[i]->ComputeIsBulletInSphere(vPosition, fRadius, pFiringEntity))
		{
			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CBulletManager::ComputeIsBulletInBox(const Vector3& vMin, const Vector3& vMax, const CEntity* pFiringEntity)
{
	for(s32 i = 0; i < ms_Bullets.GetCount(); i++)
	{
		if(ms_Bullets[i]->ComputeIsBulletInBox(vMin, vMax, pFiringEntity))
		{
			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CBulletManager::ComputeHasBulletImpactedInSphere(const Vector3& vPosition, f32 fRadius, const CEntity* pFiringEntity, bool bEntryOnly)
{
	const float fRadiusSqr = fRadius * fRadius;

	for (s32 i = 0; i < ms_EntryImpacts.GetCount(); i++)
	{
		Vector3 vImpactPosition(ms_EntryImpacts[i]);

		//don't dereference this pointer!  I warned you!  (it's not registered)
		CEntity* pImpactSourceEntity = ms_EntryEntities[i];

		if (pFiringEntity == NULL || pImpactSourceEntity == pFiringEntity)
		{
			if (vPosition.Dist2(vImpactPosition) < fRadiusSqr)
			{
				return true;
			}
		}
	}

	if (!bEntryOnly)
	{
		for (s32 i = 0; i < ms_ExitImpacts.GetCount(); i++)
		{
			Vector3 vImpactPosition(ms_ExitImpacts[i]);
			CEntity* pImpactSourceEntity = ms_ExitEntities[i];

			if (pFiringEntity == NULL || pImpactSourceEntity == pFiringEntity)
			{
				if (vPosition.Dist2(vImpactPosition) < fRadiusSqr)
				{
					return true;
				}
			}
		}
	}

 	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CBulletManager::ComputeHasBulletImpactedInBox(const Vector3& vMin, const Vector3& vMax, const CEntity* pFiringEntity, bool bEntryOnly)
{
	for (s32 i = 0; i < ms_EntryImpacts.GetCount(); i++)
	{
		Vector3 vImpactPosition(ms_EntryImpacts[i]);

		//don't dereference this pointer!  I warned you!  (it's not registered)
		CEntity* pImpactSourceEntity = ms_EntryEntities[i];

		if (pFiringEntity == NULL || pImpactSourceEntity == pFiringEntity)
		{
			if (geomPoints::IsPointInBox(vImpactPosition,vMin, vMax ))
			{
				return true;
			}
		}
	}

	if (!bEntryOnly)
	{
		for (s32 i = 0; i < ms_ExitImpacts.GetCount(); i++)
		{
			Vector3 vImpactPosition(ms_ExitImpacts[i]);
			CEntity* pImpactSourceEntity = ms_ExitEntities[i];
			if (pFiringEntity == NULL || pImpactSourceEntity == pFiringEntity)
			{
				if (geomPoints::IsPointInBox(vImpactPosition,vMin, vMax ))
				{
					return true;
				}
			}
		}
	}

	return false;
}

bool CBulletManager::ComputeHasBulletImpactedBehindPlane( const Vector3& vPlanePoint, const Vector3& vPlaneNormal, bool bEntryOnly )
{
	for (s32 i = 0; i < ms_EntryImpacts.GetCount(); i++)
	{
		Vector3 vImpactPosition(ms_EntryImpacts[i]);
		if( geomPoints::IsPointBehindPlane( vImpactPosition, vPlanePoint, vPlaneNormal) )
		{
			return true;
		}
	}

	if (!bEntryOnly)
	{
		for (s32 i = 0; i < ms_ExitImpacts.GetCount(); i++)
		{
			Vector3 vImpactPosition(ms_ExitImpacts[i]);
			if( geomPoints::IsPointBehindPlane( vImpactPosition, vPlanePoint, vPlaneNormal) )
			{
				return true;
			}
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

s32 CBulletManager::ComputeNumBulletsImpactedInSphere(const Vector3& vPosition, f32 fRadius, const CEntity* pFiringEntity, bool bEntryOnly)
{
	s32 iNumBullets = 0;

	const float fRadiusSqr = fRadius * fRadius;

	for (s32 i = 0; i < ms_EntryImpacts.GetCount(); i++)
	{
		Vector3 vImpactPosition(ms_EntryImpacts[i]);

		//don't dereference this pointer!  I warned you!  (it's not registered)
		CEntity* pImpactSourceEntity = ms_EntryEntities[i];

		if (pFiringEntity == NULL || pImpactSourceEntity == pFiringEntity)
		{
			if (vPosition.Dist2(vImpactPosition) < fRadiusSqr)
			{
				++iNumBullets;
			}
		}
	}

	if (!bEntryOnly)
	{
		for (s32 i = 0; i < ms_ExitImpacts.GetCount(); i++)
		{
			Vector3 vImpactPosition(ms_ExitImpacts[i]);
			CEntity* pImpactSourceEntity = ms_ExitEntities[i];
			if (pFiringEntity == NULL || pImpactSourceEntity == pFiringEntity)
			{
				if (vPosition.Dist2(vImpactPosition) < fRadiusSqr)
				{
					++iNumBullets;
				}
			}
		}
	}

	return iNumBullets;
}

////////////////////////////////////////////////////////////////////////////////

s32 CBulletManager::ComputeNumBulletsImpactedInBox(const Vector3& vMin, const Vector3& vMax, const CEntity* pFiringEntity, bool bEntryOnly)
{
	s32 iNumBullets = 0;

	for (s32 i = 0; i < ms_EntryImpacts.GetCount(); i++)
	{
		Vector3 vImpactPosition(ms_EntryImpacts[i]);

		//don't dereference this pointer!  I warned you!  (it's not registered)
		CEntity* pImpactSourceEntity = ms_EntryEntities[i];

		if (pFiringEntity == NULL || pImpactSourceEntity == pFiringEntity)
		{
			if (geomPoints::IsPointInBox(vImpactPosition,vMin, vMax ))
			{
				++iNumBullets;
			}
		}
	}

	if (!bEntryOnly)
	{
		for (s32 i = 0; i < ms_ExitImpacts.GetCount(); i++)
		{
			Vector3 vImpactPosition(ms_ExitImpacts[i]);
			CEntity* pImpactSourceEntity = ms_ExitEntities[i];
			if (pFiringEntity == NULL || pImpactSourceEntity == pFiringEntity)
			{
				if (geomPoints::IsPointInBox(vImpactPosition,vMin, vMax ))
				{
					++iNumBullets;
				}
			}
		}
	}

	return iNumBullets;
}

////////////////////////////////////////////////////////////////////////////////

#if __BANK
void CBulletManager::RenderDebug()
{
	for(s32 i = 0; i < ms_Bullets.GetCount(); i++)
	{
		ms_Bullets[i]->RenderDebug();
	}
}
#endif // __BANK

////////////////////////////////////////////////////////////////////////////////
