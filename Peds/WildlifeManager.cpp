// FILE :    WildlifeManager.h
// PURPOSE : Spawn ambient animals in the world.

// class header
#include "WildlifeManager.h"

// Game headers
#include "ai/EntityScanner.h"
#include "camera/CamInterface.h"
#include "camera/debug/DebugDirector.h"
#include "camera/helpers/Frame.h"
#include "Cutscene/CutSceneManagerNew.h"
#include "Event/Events.h"
#include "Game/ModelIndices.h"
#include "Peds/NavCapabilities.h"
#include "Network/NetworkInterface.h"
#include "Network/Objects/Entities/NetObjPed.h"
#include "Peds/Ped.h"
#include "Peds/PedFactory.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PedPopulation.h"
#include "Peds/PedMotionData.h"
#include "Peds/PopCycle.h"
#include "physics/WaterTestHelper.h"
#include "Scene/world/GameWorld.h"
#include "Scene/world/GameWorldHeightMap.h"
#include "Script/script_cars_and_peds.h"
#include "Task/Combat/TaskSharkAttack.h"
#include "Task/Default/TaskSwimmingWander.h"
#include "Task/General/TaskBasic.h"
#include "Task/Scenario/ScenarioManager.h"
#include "Task/Scenario/info/ScenarioInfo.h"
#include "Task/System/MotionTaskData.h"

// Rage headers
#include "fwscene/world/WorldLimits.h"

AI_OPTIMISATIONS()

// Parser headers
#include "Peds/WildlifeManagerMetadata_parser.h"

#define IMPLEMENT_WILDLIFE_MANAGER_TUNABLES(classname, hash)	IMPLEMENT_TUNABLES_PSO(classname, #classname, hash, "Wildlife Manager", "Ped Population")

CWildlifeManager::Tunables CWildlifeManager::sm_Tunables;
IMPLEMENT_WILDLIFE_MANAGER_TUNABLES(CWildlifeManager, 0xecdf7966)

// Static singleton instance
CWildlifeManager CWildlifeManager::m_WildlifeManagerInstance;

void CWildlifeManager::Init()
{
	m_WildlifeManagerInstance.m_bHasGoodWaterProbeSpot = false;
	m_WildlifeManagerInstance.m_LastKindOfWildlifeSpawned = DSP_NORMAL;
	m_WildlifeManagerInstance.m_bCachedPedGenPointsAvailable = true;
	m_WildlifeManagerInstance.m_bCanSpawnSharkModels = true;
	m_WildlifeManagerInstance.m_uLastSharkSpawn = 0;
	m_WildlifeManagerInstance.m_fPlayerDeepWaterTimer = -1.0f;
	m_WildlifeManagerInstance.m_fScriptForcedBirdMinFlightHeight = -1.0f;
}

void CWildlifeManager::Shutdown()
{

}

CWildlifeManager& CWildlifeManager::GetInstance()
{
	return m_WildlifeManagerInstance;
}

// Returns true if the modelIndex prefers to spawn in the air.
bool CWildlifeManager::IsFlyingModel(u32 modelIndex) const
{
	fwModelId pedModelId((strLocalIndex(modelIndex)));
	Assert(pedModelId.IsValid());

	const CPedModelInfo* pPedModelInfo =(CPedModelInfo*)CModelInfo::GetBaseModelInfo(pedModelId);
	if (Verifyf(pPedModelInfo, "Ped model ID was valid, but no CPedModelInfo could be loaded!"))
	{
		return pPedModelInfo->ShouldSpawnInAirByDefault();
	}
	return false;
}

// Return true if the model index prefers to spawn in the water.
bool CWildlifeManager::IsAquaticModel(u32 modelIndex) const
{
	fwModelId pedModelId((strLocalIndex(modelIndex)));
	Assert(pedModelId.IsValid());

	const CPedModelInfo* pPedModelInfo =(CPedModelInfo*)CModelInfo::GetBaseModelInfo(pedModelId);
	if (Verifyf(pPedModelInfo, "Ped model ID was valid, but no CPedModelInfo could be loaded!"))
	{
		return pPedModelInfo->ShouldSpawnInWaterByDefault();
	}
	return false;
}

// Return true if the model index prefers to spawn as ground wildlife.
bool CWildlifeManager::IsGroundWildlifeModel(u32 modelIndex) const
{
	fwModelId pedModelId((strLocalIndex(modelIndex)));
	Assert(pedModelId.IsValid());

	const CPedModelInfo* pPedModelInfo =(CPedModelInfo*)CModelInfo::GetBaseModelInfo(pedModelId);
	if (Verifyf(pPedModelInfo, "Ped model ID was valid, but no CPedModelInfo could be loaded!"))
	{
		return pPedModelInfo->ShouldSpawnAsWildlifeByDefault();
	}
	return false;
}


void CWildlifeManager::Process(bool bCachedPedGenPointsRemaining)
{
	// Note whether other kinds of peds are going to ambiently spawn.
	m_bCachedPedGenPointsAvailable = bCachedPedGenPointsRemaining;

	float fDT = fwTimer::GetTimeStep();

	if (!m_GroundProbeHelper.IsProbeActive())
	{
		m_fTimeSinceLastGroundProbe += fDT;

		if (m_fTimeSinceLastGroundProbe >= sm_Tunables.m_TimeBetweenGroundProbes)
		{
			FireGroundWildlifeProbe();
		}
	}
	else
	{
		CheckGroundWildlifeProbe();
	}

	if (!m_WaterProbeHelper.IsProbeActive())
	{

		m_fTimeSinceLastWaterHeightCheck += fDT;

		if (!m_bHasGoodWaterProbeSpot && m_fTimeSinceLastWaterHeightCheck >= sm_Tunables.m_TimeBetweenWaterHeightMapChecks)
		{
			FindWaterProbeSpot();
			m_fTimeSinceLastWaterHeightCheck = 0.0f;
		}

		m_fTimeSinceLastWaterProbe += fDT;
	
		if (m_bHasGoodWaterProbeSpot && m_fTimeSinceLastWaterProbe >= sm_Tunables.m_TimeBetweenWaterProbes)
		{
			FireWaterProbe();
		}
	}
	else
	{
		CheckWaterProbe();
	}

	ProcessSpecialStreamingConditions();

	if (ShouldDispatchAShark())
	{
		DispatchASharkNearPlayer(false);
	}
}

// Reset script dependent variables in the wildlife system.
void CWildlifeManager::ProcessPreScripts()
{
	m_fScriptForcedBirdMinFlightHeight = -1.0f;
}

// If the player is in the water, we probably don't want to stream out the fish model, even if it is the oldest in the streaming rotation.
void CWildlifeManager::ManageAquaticModelStreamingConditions(u32 modelIndex)
{
	// Only manage if the array is not full and the model isn't already being managed.
	if (!m_aManagedAquaticModels.IsFull() && m_aManagedAquaticModels.Find(modelIndex) == -1)
	{
		fwModelId modelId((strLocalIndex(modelIndex)));
		if (Verifyf(modelId.IsValid(), "Invalid ped modelID!"))
		{
			CPedModelInfo* pPedModelInfo = (CPedModelInfo*)CModelInfo::GetBaseModelInfo(modelId);
			if (pPedModelInfo)
			{
				pPedModelInfo->SetCanStreamOutAsOldest(false);
				m_aManagedAquaticModels.Push(modelIndex);
			}
		}
	}
}

// Check if the player is out of water, and if so clear any previously set streaming preferences.
void CWildlifeManager::ProcessSpecialStreamingConditions()
{
	bool bNeedMoreAquaticPeds = ShouldSpawnMoreAquaticPeds();

	if (!bNeedMoreAquaticPeds)
	{
		for(int i=0; i < m_aManagedAquaticModels.GetCount(); i++)
		{
			fwModelId modelId((strLocalIndex(m_aManagedAquaticModels[i])));
			if (Verifyf(modelId.IsValid(), "Invalid ped model id!"))
			{
				CPedModelInfo* pPedModelInfo = (CPedModelInfo*)CModelInfo::GetBaseModelInfo(modelId);
				if (pPedModelInfo)
				{
					pPedModelInfo->SetCanStreamOutAsOldest(true);
				}
			}
			m_aManagedAquaticModels.Delete(i);
		}
	}
}

bool CWildlifeManager::MaterialSupportsWildlife(const phMaterial& rHitMaterial)
{
	atHashString sMaterialName(rHitMaterial.GetName());
	return CPopCycle::IsMaterialSuitableForWildlife(sMaterialName);
}

void CWildlifeManager::AddGroundPointToCollection(const Vector3& vPoint)
{
	if (m_aGroundWildlifePoints.IsFull())
	{
		m_aGroundWildlifePoints.Pop();
	}

	m_aGroundWildlifePoints.Push(vPoint);
}

void CWildlifeManager::FireGroundWildlifeProbe()
{
	CPed* pPlayer = CGameWorld::FindLocalPlayer();

	// Spawning is centered around the player's position, so if we couldn't get a valid player then no spawning can be done.
	if (pPlayer)
	{
		float fRandomHeading = fwAngle::LimitRadianAngle(fwRandom::GetRandomNumberInRange(0.0f, TWO_PI));
		float fSpawnDistance = fwRandom::GetRandomNumberInRange(sm_Tunables.m_MinDistanceToSearchForGroundWildlifePoints, sm_Tunables.m_MaxDistanceToSearchForGroundWildlifePoints);


		// Randomize where the probe starts from in the X,Y try to stay just above the ground in the Z.
		Vector3 vStart = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());
		vStart.x += rage::Cosf(fRandomHeading) * fSpawnDistance;
		vStart.y += rage::Sinf(fRandomHeading) * fSpawnDistance;
		vStart.z = CGameWorldHeightMap::GetMinHeightFromWorldHeightMap(vStart.x, vStart.y) + sm_Tunables.m_GroundMaterialProbeOffset;

		// Check to make sure it is contained within the map.
		if (!IsSpawnPointInsideWorldLimits(RCC_VEC3V(vStart)))
		{
			return;
		}

		//Create and fire the probe
		WorldProbe::CShapeTestProbeDesc probeData;
		Vector3 vEnd = vStart;
		vEnd.z -= sm_Tunables.m_GroundMaterialProbeDepth;
		probeData.SetStartAndEnd(vStart, vEnd);
		probeData.SetContext(WorldProbe::EMovementAI);
		probeData.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_OBJECT_TYPE|ArchetypeFlags::GTA_VEHICLE_TYPE);
		probeData.SetOptions(WorldProbe::LOS_IGNORE_SEE_THRU); 
		m_GroundProbeHelper.StartTestLineOfSight(probeData);
	}
}

void CWildlifeManager::CheckGroundWildlifeProbe()
{
	//Check the probe
	ProbeStatus probeStatus;
	Vector3 vPos;
	Vector3 vNormal;
	u16 iIntersectionPhysInstLevelIndex;
	u16 iIntersectionPhysInstGenerationId;
	u16 iIntersectionComponent;
	phMaterialMgr::Id iIntersectionMtlId;
	float fIntersectionTValue;
	if (m_GroundProbeHelper.GetProbeResultsVfx(probeStatus, &vPos, &vNormal, &iIntersectionPhysInstLevelIndex, &iIntersectionPhysInstGenerationId, &iIntersectionComponent, &iIntersectionMtlId, &fIntersectionTValue))
	{
		//Probe is done calculating
		if (probeStatus == PS_Blocked)
		{
			// Make sure the ground is reasonably flat.
			if (vNormal.z >= sm_Tunables.m_GroundMaterialSpawnCoordNormalZTolerance)
			{			
				// Check the material that was hit.
				phMaterial rHitMaterial = phMaterialMgr::GetInstance().GetMaterial(iIntersectionMtlId);
				if (MaterialSupportsWildlife(rHitMaterial))
				{
					// Reject points that were in the water.
					float fWaterHeight = 0.0f;
					bool bInWater = false;
					if (CWaterTestHelper::GetWaterHeightAtPositionIncludingRivers(vPos, &fWaterHeight))
					{
						if (fWaterHeight >= vPos.z)
						{
							bInWater = true;
						}
					}

					if (!bInWater)
					{
						// Offset the Z so a critter doesn't spawn under the ground.
						AddGroundPointToCollection(vPos);
					}
				}
			}
		}
		// Do another check in 3 seconds.
		m_fTimeSinceLastGroundProbe = 0.0f;
	}
}

void CWildlifeManager::FindWaterProbeSpot()
{

	CPed* pPlayer = CGameWorld::FindLocalPlayer();

	// Spawning is centered around the player's position, so if we couldn't get a valid player then no spawning can be done.
	if (!pPlayer)
	{
		return;
	}

	Vector3 vCoord = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());


	// Randomly choose a point
	float fRandomHeading = fwAngle::LimitRadianAngle(fwRandom::GetRandomNumberInRange(0.0f, TWO_PI));
	float fSpawnDistance = fwRandom::GetRandomNumberInRange(sm_Tunables.m_MinDistanceToSearchForAquaticPoints, sm_Tunables.m_MaxDistanceToSearchForAquaticPoints);


	vCoord.x += rage::Cosf(fRandomHeading) * fSpawnDistance;
	vCoord.y += rage::Sinf(fRandomHeading) * fSpawnDistance;

	// Calculate the water level at the position.
	float fWaterZ;
	if(!Water::GetWaterLevelNoWaves(vCoord, &fWaterZ, POOL_DEPTH, REJECTIONABOVEWATER, NULL))
	{
		// The point was not in the water.
		return;
	}

	// Probe from either just below the water level or 10m above the player's root, whichever is deeper.
	vCoord.z = Min(fWaterZ - 1.0f, vCoord.z + sm_Tunables.m_AquaticSpawnMaxHeightAbovePlayer);

	// Check to make sure it is contained within the map.
	if (!IsSpawnPointInsideWorldLimits(RCC_VEC3V(vCoord)))
	{
		return;
	}

	// The point appears fine, later it will be further checked with an async probe.
	m_vGoodWaterProbeSpot = vCoord;
	m_bHasGoodWaterProbeSpot = true;
}

// Fire a probe downward from slightly above the water's surface checking for obstructions.
void CWildlifeManager::FireWaterProbe()
{
	Vector3 vStart = m_vGoodWaterProbeSpot;
	vStart.z += sm_Tunables.m_WaterProbeOffset;

	//Create and fire the probe
	WorldProbe::CShapeTestProbeDesc probeData;
	Vector3 vEnd = vStart;
	vEnd.z -= sm_Tunables.m_WaterProbeDepth;
	probeData.SetStartAndEnd(vStart, vEnd);
	probeData.SetContext(WorldProbe::EMovementAI);
	probeData.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_OBJECT_TYPE|ArchetypeFlags::GTA_VEHICLE_TYPE);
	probeData.SetOptions(WorldProbe::LOS_IGNORE_SEE_THRU); 
	m_WaterProbeHelper.StartTestLineOfSight(probeData);

	//grcDebugDraw::Capsule(VECTOR3_TO_VEC3V(vStart), VECTOR3_TO_VEC3V(vEnd), 0.5f, Color_white, false, -10);
}

// See if the probe is clear, if so add a point to the collection of suitable aquatic points.
void CWildlifeManager::CheckWaterProbe()
{
	ProbeStatus probeStatus;
	Vector3 vIntersection;
	if (m_WaterProbeHelper.GetProbeResultsWithIntersection(probeStatus, &vIntersection))
	{
		const float fWaterHeight = m_vGoodWaterProbeSpot.z;
		const float fEpsilon = 1.5f;

		bool bFoundASpot = false;
		float fStart = fWaterHeight - fEpsilon;
		float fEnd = 0.0f;

		if (probeStatus == PS_Clear)
		{
			// Didn't hit anything - anything up to the depth is valid (minus an epsilon to make sure we don't spawn them too close to the ground).
			fEnd = (m_vGoodWaterProbeSpot.z + sm_Tunables.m_WaterProbeOffset) - sm_Tunables.m_WaterProbeDepth;
			fEnd += fEpsilon;
			if (fEnd < fStart)
			{
				// Ensure the water level is somewhat close to the heightmap - this prevents fish spawning deep underground.
				float fMaxHeightMapZ = CGameWorldHeightMap::GetMaxHeightFromWorldHeightMap(m_vGoodWaterProbeSpot.x, m_vGoodWaterProbeSpot.y);
				if (fMaxHeightMapZ - fWaterHeight < sm_Tunables.m_WaterProbeOffset)
				{
					bFoundASpot = true;
				}
			}
		}
		else
		{
			// We hit something, but this might still be a valid location.
			// Allowed to spawn up to the point of collision (minus an epsilon to make sure we don't spawn them too close to the ground).
			fEnd = vIntersection.z;
			fEnd += fEpsilon;

			if (fEnd < fStart)
			{
				bFoundASpot = true;
			}
		}

		if (bFoundASpot)
		{
			m_vGoodWaterProbeSpot.z = fwRandom::GetRandomNumberInRange(fEnd, fStart);
			AddWaterPointToCollection(m_vGoodWaterProbeSpot);
			//grcDebugDraw::Sphere(m_vGoodWaterProbeSpot, 0.5f, Color_green, false, -5);
		}
		//else
		//{
		//	grcDebugDraw::Sphere(m_vGoodWaterProbeSpot, 0.5f, Color_red, false, -5);
		//}

		// Reset the point finder.
		m_bHasGoodWaterProbeSpot = false;
	}
}

// Add the point to the array of points, removing the oldest if necessary.
void CWildlifeManager::AddWaterPointToCollection(const Vector3& vPoint)
{
	if (m_aAquaticSpawnPoints.IsFull())
	{
		m_aAquaticSpawnPoints.Pop();
	}

	m_aAquaticSpawnPoints.Push(vPoint);
}

// Returns true if the spawnpoint is inside the world extents AABB.
bool CWildlifeManager::IsSpawnPointInsideWorldLimits(Vec3V_ConstRef vPoint)
{
	return g_WorldLimits_AABB.ContainsPoint(vPoint);
}

// Generate a random point in the portion of the circle between fAddRangeInViewMin and fAddRangeOutofViewMin.
// Set the Z-coordinate equal to the heightmap + some delta at that location.
void CWildlifeManager::GeneratePossibleFlyingCoord(Vector3& pCoord)
{
	float fRandomHeading = fwAngle::LimitRadianAngle(fwRandom::GetRandomNumberInRange(0.0f, TWO_PI));
	float fSpawnDistance = fwRandom::GetRandomNumberInRange(GetTunables().m_BirdSpawnXYRangeMin, GetTunables().m_BirdSpawnXYRangeMax);


	pCoord.x += rage::Cosf(fRandomHeading) * fSpawnDistance;
	pCoord.y += rage::Sinf(fRandomHeading) * fSpawnDistance;
	float fHeightMapZ = CGameWorldHeightMap::GetMaxHeightFromWorldHeightMap(pCoord.x, pCoord.y) + fwRandom::GetRandomNumberInRange(sm_Tunables.m_BirdHeightMapDeltaMin, sm_Tunables.m_BirdHeightMapDeltaMax);
	pCoord.z = (m_fScriptForcedBirdMinFlightHeight < 0.0f || fHeightMapZ > m_fScriptForcedBirdMinFlightHeight) ? fHeightMapZ : m_fScriptForcedBirdMinFlightHeight;
}

// Check to make sure the point is in a good place for the bird to spawn in at.
bool CWildlifeManager::TestAerialSpawnPoint(Vec3V_In vCenter, Vec3V_In vCoord, float pedRadius, float fAddRangeInViewMin, float fAddRangeOutOfViewMin, bool doInFrustumTest)
{
	// The point is not inside the world, reject.
	if (!IsSpawnPointInsideWorldLimits(vCoord))
	{
		return false;
	}

	// Reject anything within a cone above the center spawning point:
	
	// Angle of the line of the cone.
	const float fRejectAboveTanAngle = 0.839f; // 40 degrees

	// Vector from the center to the possible spawn point.
	const Vec3V vPlayerToCoord = vCoord - vCenter;
	const ScalarV vDistXY = MagXYFast(vPlayerToCoord);

	// Check if inside the cone.
	const ScalarV vThreshold = Scale(ScalarV(fRejectAboveTanAngle), vDistXY);
	if (IsGreaterThanAll(vPlayerToCoord.GetZ(), vThreshold))
	{
		return false;
	}

	// Check if the point can be seen.
	if(doInFrustumTest)
	{
		// If a cutscene is activate, don't spawn on screen ever.
		if (CutSceneManager::GetInstance()->IsActive())
		{
			fAddRangeInViewMin = 9999.0f;
		}

		bool bOnScreen = CPedPopulation::IsCandidateInViewFrustum(RCC_VECTOR3(vCoord), pedRadius, fAddRangeInViewMin);
		if(bOnScreen)
		{
			return false;
		}
	}
	
	// Not in view, but might still be too close.
	if (IsLessThanAll(MagSquared(vPlayerToCoord), ScalarV(fAddRangeOutOfViewMin * fAddRangeOutOfViewMin)))
	{
		return false;
	}

	// The point is valid to spawn at.
	return true;
}

// Perform extra checks that need to be done in multiplayer.  Return true if the model is valid.
bool CWildlifeManager::CheckWildlifeMultiplayerSpawnConditions(u32 pedModelIndex) const
{
	if (NetworkInterface::IsGameInProgress())
	{
		fwModelId pedModelId((strLocalIndex(pedModelIndex)));

		if (Verifyf(pedModelId.IsValid(), "Invalid ped model id!"))
		{
			CPedModelInfo* pPedModelInfo = (CPedModelInfo*)CModelInfo::GetBaseModelInfo(pedModelId);

			if (pPedModelInfo)
			{
				atHashString sMotionHash = pPedModelInfo->GetMotionTaskDataSetHash();
				const CMotionTaskDataSet* pMotionSet = CMotionTaskDataManager::GetDataSet(sMotionHash.GetHash());
				if (pMotionSet)
				{
					// Valid only if CNetObjPed supports the creation of the ped's motion task.
					u32 motionTaskType = pMotionSet->GetOnFootData()->m_Type;
					if( motionTaskType == PED_ON_FOOT /*|| motionTaskType == PED_ON_MOUNT || motionTaskType == HORSE_ON_FOOT */) 
					{
						// Valid only if CNetObjPed supports the creation of the ped's motion task.
						return true;
					}
				}
			}
		}
		// Invalid ped model index of some sort...was probably the result of some bad data.
		return false;
	}
	// If not in multiplayer, the pedModelIndex is always valid.
	return true;
}

// Return true if a valid spawn coordinate for the flying ped was generated
// newPedGroundPosOut = outParam for spawn point for ped
// bNewPedGroundPosOutIsInterior = always false, never spawn inside
// popCtrlIsInInterior = whether the population control is inside a building, this will cause false to be returned (never spawn inside)
// fAddRangeInViewMin = close generation band
// pedRadius = how far a ped needs to be out of range for the frustum test
// doInFrustumTest = whether to block potential spawnpoints that the game camera can see
bool CWildlifeManager::GetFlyingPedCoord (Vector3& newPedGroundPosOut, bool& bNewPedGroundPosOutIsInInterior, bool popCtrlIsInInterior, float fAddRangeInViewMin, float fAddRangOutOfViewMin, float pedRadius, bool doInFrustumTest, u32 pedModelIndex)
{
	CPed* pPlayer = CGameWorld::FindLocalPlayer();

	// Spawning is centered around the player's position, so if we couldn't get a valid player then no spawning can be done.
	if (!pPlayer)
	{
		return false;
	}

	bNewPedGroundPosOutIsInInterior = false;
	if (popCtrlIsInInterior)
	{
		// Never try to spawn in an interior.
		return false;
	}

	// Check if the ped model being spawned is allowed to spawn in multiplayer.
	if (!CheckWildlifeMultiplayerSpawnConditions(pedModelIndex))
	{
		return false;
	}

	bool foundGoodPoint = false;
	u32 numTries = 0;
	Vec3V vPopulationCenter = pPlayer->GetTransform().GetPosition();
	while (!foundGoodPoint && numTries < MAX_NUM_AIR_SPAWN_TRIES)
	{
		newPedGroundPosOut = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());
		GeneratePossibleFlyingCoord(newPedGroundPosOut);
		foundGoodPoint = TestAerialSpawnPoint(vPopulationCenter, VECTOR3_TO_VEC3V(newPedGroundPosOut), pedRadius, fAddRangeInViewMin, fAddRangOutOfViewMin, doInFrustumTest);
		numTries++;
	}
	if (foundGoodPoint)
	{
		m_LastKindOfWildlifeSpawned = DSP_AERIAL;
	}
	return foundGoodPoint;
}

bool CWildlifeManager::GetGroundWildlifeCoord(Vector3& newPedGroundPosOut, bool& bNewPedGroundPosOutIsInInterior, bool popCtrlIsInInterior, float fAddRangeInViewMin, float pedRadius, bool doInFrustumTest, u32 pedModelIndex)
{
	CPed* pPlayer = CGameWorld::FindLocalPlayer();

	// Spawning is centered around the player's position, so if we couldn't get a valid player then no spawning can be done.
	if (!pPlayer)
	{
		return false;
	}

	bNewPedGroundPosOutIsInInterior = false;
	if (popCtrlIsInInterior)
	{
		// Never try to spawn in an interior.
		return false;
	}

	// Check if the ped model being spawned is allowed to spawn in multiplayer.
	if (!CheckWildlifeMultiplayerSpawnConditions(pedModelIndex))
	{
		return false;
	}

	s32 roomIdx = -1;
	CInteriorInst* pIntInst = NULL;

	// Iterate over the stored up points suitable for spawning.
	for(int i=0; i < m_aGroundWildlifePoints.GetCount(); i++)
	{
		Vector3& pCoord = m_aGroundWildlifePoints[i];

		Vec3V vCoord = RCC_VEC3V(pCoord);

		// Verify the point is at least some distance from the player.
		if (IsLessThanAll(DistSquared(vCoord, pPlayer->GetTransform().GetPosition()), ScalarV(rage::square(sm_Tunables.m_MinDistanceToSearchForGroundWildlifePoints))))
		{
			continue;
		}

		// Ensure the point is relatively close to the player.
		if (IsGreaterThanAll(DistSquared(vCoord, pPlayer->GetTransform().GetPosition()), ScalarV(rage::square(sm_Tunables.m_MaxDistanceToSearchForGroundWildlifePoints))))
		{
			continue;
		}

		if (CPedPopulation::IsPedGenerationCoordCurrentlyValid(pCoord, bNewPedGroundPosOutIsInInterior, pedRadius, popCtrlIsInInterior, 
			doInFrustumTest, fAddRangeInViewMin, true, true, NULL, NULL, roomIdx, pIntInst, true))
		{
			newPedGroundPosOut = pCoord;
			m_aGroundWildlifePoints.Delete(i);
			m_LastKindOfWildlifeSpawned = DSP_GROUND_WILDLIFE;
			return true;
		}
	}
	return false;
}

// Return true if a valid spawn coordinate for water spawning was generated.
bool CWildlifeManager::GetWaterSpawnCoord(Vector3& newWaterPositionOut, bool& bNewWaterPosOutIsInInterior, bool popCtrlIsInInterior, float fAddRangeInViewMin, float pedRadius, bool doInFrustumTest, u32 pedModelIndex)
{
	CPed* pPlayer = CGameWorld::FindLocalPlayer();

	// Spawning is centered around the player's position, so if we couldn't get a valid player then no spawning can be done.
	if (!pPlayer)
	{
		return false;
	}

	bNewWaterPosOutIsInInterior = false;
	if (popCtrlIsInInterior)
	{
		// Never try to spawn in an interior.
		return false;
	}

	// Check if the ped model being spawned is allowed to spawn in multiplayer.
	if (!CheckWildlifeMultiplayerSpawnConditions(pedModelIndex))
	{
		return false;
	}

	fwModelId pedModelId((strLocalIndex(pedModelIndex)));

	if (!(Verifyf(pedModelId.IsValid(), "Invalid ped model index!")))
	{
		return false;
	}

	CPedModelInfo* pPedModelInfo = (CPedModelInfo*)CModelInfo::GetBaseModelInfo(pedModelId);

	if (Verifyf(pPedModelInfo, "Inavlid ped model info!"))
	{
		// Certain ped types can modify the add range minimum.
		fAddRangeInViewMin *= pPedModelInfo->AllowsCloseSpawning() ? sm_Tunables.m_CloseSpawningViewMultiplier : 1.0f;
	}
	else
	{
		return false;
	}

	s32 roomIdx = -1;
	CInteriorInst* pIntInst = NULL;

	// Iterate over the stored up points suitable for spawning.
	for(int i=0; i < m_aAquaticSpawnPoints.GetCount(); i++)
	{
		Vector3& vCoord = m_aAquaticSpawnPoints[i];

		// Ensure the point is at least some min distance away, even if we are generally OK with spawning the ped close to the player.
		if (IsLessThanAll(DistSquared(VECTOR3_TO_VEC3V(vCoord), pPlayer->GetTransform().GetPosition()), ScalarV(rage::square(sm_Tunables.m_MinDistanceToSearchForAquaticPoints))))
		{
			continue;
		}

		// Ensure that the point is smaller than some max distance.
		if (IsGreaterThanAll(DistSquared(VECTOR3_TO_VEC3V(vCoord), pPlayer->GetTransform().GetPosition()), ScalarV(rage::square(sm_Tunables.m_MaxDistanceToSearchForAquaticPoints))))
		{
			continue;
		}

		if (CPedPopulation::IsPedGenerationCoordCurrentlyValid(vCoord, bNewWaterPosOutIsInInterior, pedRadius, popCtrlIsInInterior, 
			doInFrustumTest, fAddRangeInViewMin, true, true, NULL, NULL, roomIdx, pIntInst, true))
		{
			newWaterPositionOut = vCoord;
			m_aAquaticSpawnPoints.Delete(i);
			m_LastKindOfWildlifeSpawned = DSP_AQUATIC;
			return true;
		}
	}

	return false;
}

// When the player is swimming try and spawn more aquatic peds.
bool CWildlifeManager::ShouldSpawnMoreAquaticPeds() const
{
	// Don't increase percentages in multiplayer games.
	if (!NetworkInterface::IsGameInProgress())
	{
		const CPed* pPlayer = CGameWorld::FindLocalPlayer();

		// Only increase percentage of fish if there is a player.
		if (pPlayer)
		{
			// Increase the percentage of fish if the player is swimming.
			return pPlayer->GetPedConfigFlag(CPED_CONFIG_FLAG_IsSwimming);
		}
	}
	return false;
}

// If we can't spawn anything that uses wander navmesh, then we might as well try and spawn birds.
bool CWildlifeManager::ShouldSpawnMoreAerialPeds() const
{
	// Don't increase percentages in multiplayer games.
	if (!NetworkInterface::IsGameInProgress())
	{
		return !m_bCachedPedGenPointsAvailable && !ShouldSpawnMoreAquaticPeds();
	}
	return false;
}

// If we can't spawn anything that uses wander navmesh, then we might as well try and spawn wildlife.
bool CWildlifeManager::ShouldSpawnMoreGroundWildlifePeds() const
{
	// Don't increase percentages in multiplayer games.
	if (!NetworkInterface::IsGameInProgress())
	{
		return !m_bCachedPedGenPointsAvailable && !ShouldSpawnMoreAquaticPeds();
	}
	return false;
}

bool CWildlifeManager::ShouldDispatchAShark()
{
	const Tunables& tune = GetTunables();
	// Make sure that there is a player, and that they are swimming.
	if (!NetworkInterface::IsGameInProgress() && ShouldSpawnMoreAquaticPeds() && CanSpawnSharkModels() && (!HasSpawnedAShark() || fwTimer::GetTimeInMilliseconds() - m_uLastSharkSpawn >= tune.m_MinTimeBetweenSharkDispatches))
	{
		const CPed* pPlayer = CGameWorld::FindLocalPlayer();

		if (Verifyf(pPlayer, "Expected to get a player pointer if ShouldSpawnMoreAquaticPeds() returned true."))
		{
			// Check to make sure the model is not being suppressed by script.
			fwModelId sharkModel = CModelInfo::GetModelIdFromName(tune.m_SharkModelName);

			if (sharkModel.IsValid() && !CScriptPeds::GetSuppressedPedModels().HasModelBeenSuppressed(sharkModel.GetModelIndex()))
			{
				Vector3 vStart = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());

				if (m_SharkDepthProbeHelper.IsProbeActive())
				{
					// We have fired and are waiting on probe test results.
					ProbeStatus probeStatus;
					if (m_SharkDepthProbeHelper.GetProbeResults(probeStatus))
					{
						// The probe check test is complete.
						if (probeStatus == PS_Clear)
						{
							// Probe results are ready and the status is clear, this is deep enough water to try to spawn a shark in.
							if (m_fPlayerDeepWaterTimer < 0.0f)
							{
								// No successful checks have been made - this is the first time we have seen deep water since a reset.
								m_fPlayerDeepWaterTimer = 0.0f;
							}
							else
							{
								// Do nothing, the timer will continue to increment below.
							}
						}
						else if (probeStatus == PS_Blocked)
						{
							// The water isn't deep enough, reset the timer.
							m_fPlayerDeepWaterTimer = -1.0f;
						}
					}
				}
				else
				{
					// Fire a probe and wait on the results.
					WorldProbe::CShapeTestProbeDesc probeData;
					Vector3 vEnd = vStart;
					vEnd.z -= GetTunables().m_DeepWaterThreshold;
					probeData.SetStartAndEnd(vStart, vEnd);
					probeData.SetContext(WorldProbe::LOS_GeneralAI);
					probeData.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
					m_SharkDepthProbeHelper.StartTestLineOfSight(probeData);
				}

				// Increment the depth timer.
				if (m_fPlayerDeepWaterTimer >= 0.0f)
				{
					m_fPlayerDeepWaterTimer += fwTimer::GetTimeStep();

					// Enough time has passed in deep water for a shark to spawn.
					if (m_fPlayerDeepWaterTimer > GetTunables().m_PlayerSwimTimeThreshold)
					{
						// Reset the deep water timer.
						m_fPlayerDeepWaterTimer = -1.0f;

						// Now that we've met all the conditions, do a random roll to see if we actually do want to spawn a shark.
						// Otherwise the timer gets reset and we'll start ticking again.
						if (fwRandom::GetRandomNumberInRange(0.0f, 1.0f) <= GetTunables().m_SharkSpawnProbability)
						{
							// Check to make sure there aren't any sharks near the player already.
							// This is a relatively expensive check as it iterates over the peds nearby the player, but it is only done in the case where all of the above
							// succeeded.
							if (AreThereAnySharksNearThePlayer())
							{
								return false;
							}
							else
							{
								return true;
							}
						}
					}
				}
			}
		}
	}
	return false;
}

bool CWildlifeManager::HasSpawnedAShark()
{
	return m_uLastSharkSpawn > 0;
}

bool CWildlifeManager::CanSpawnSharkModels()
{
	return m_bCanSpawnSharkModels;
}

bool CWildlifeManager::AreThereAnySharksNearThePlayer() const
{
	// There is a shark in the combat task.
	if (CTaskSharkAttack::SharksArePresent())
	{
		return true;
	}
	
	CPed* pPlayer = CGameWorld::FindLocalPlayer();

	// Ensure there is a local player.
	if (!pPlayer)
	{
		return false;
	}

	// Grab the shark model and make sure it is valid.
	fwModelId sharkModel = CModelInfo::GetModelIdFromName(sm_Tunables.m_SharkModelName);
	if (!sharkModel.IsValid())
	{
		return false;
	}

	// Run through the peds near the player and see if one of them is a shark.
	const CEntityScannerIterator entityList = pPlayer->GetPedIntelligence()->GetNearbyPeds();
	for(const CEntity* nearbyEntity = entityList.GetFirst(); nearbyEntity; nearbyEntity = entityList.GetNext())
	{
		const CPed* pNearbyPed = static_cast<const CPed*>(nearbyEntity);
		if (pNearbyPed->GetModelId() == sharkModel)
		{
			return true;
		}
	}

	return false;
}

bool CWildlifeManager::DispatchASharkNearPlayer(bool bTellItToKillThePlayer, bool DEV_ONLY(bForceToCameraPosition))
{
	CPed* pPlayer = CGameWorld::FindLocalPlayer();

	if (Verifyf(pPlayer, "Cannot dispatch a shark to the player when there is no player..."))
	{
		const Tunables& rTunables = GetTunables();

		fwModelId modelId = CModelInfo::GetModelIdFromName(rTunables.m_SharkModelName);

		if (Verifyf(modelId.IsValid(), "Shark model %s did not exist!", rTunables.m_SharkModelName.GetCStr()))
		{
			// Attempt to stream in the model.
			if (!CModelInfo::AreAssetsRequested(modelId) && !CModelInfo::HaveAssetsLoaded(modelId))
			{
				CModelInfo::RequestAssets(modelId, STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
			}

			if (!CModelInfo::HaveAssetsLoaded(modelId))
			{
				// Couldn't stream in the shark yet.
				return false;
			}

			Vector3 vSpawnCoords;
			float fPedRadius = 1.0f;
			bool bIsInInterior = false;

			float fAddRangeInViewDist = rTunables.m_SharkAddRangeInViewMin;
			bool bUseLargerSpawnRange = !bTellItToKillThePlayer;

			if (bUseLargerSpawnRange)
			{
				fAddRangeInViewDist = rTunables.m_SharkAddRangeInViewMinFar;
			}

			// Spawn near the player.
			if (GetWaterSpawnCoord(vSpawnCoords, bIsInInterior, false, fAddRangeInViewDist, fPedRadius, true, modelId.GetModelIndex()) DEV_ONLY( || bForceToCameraPosition))
			{

#if __DEV //def CAM_DEBUG
				if (bForceToCameraPosition)
				{
					camDebugDirector& debugDirector = camInterface::GetDebugDirector();
					if(debugDirector.IsFreeCamActive())
					{
						const camFrame& freeCamFrame = debugDirector.GetFreeCamFrame();
						vSpawnCoords = freeCamFrame.GetPosition();
					}
					else
					{
						return false;
					}
				}
#endif

				// Ped will be created without a default task.
				const float fHeading = fwAngle::LimitRadianAngle(fwRandom::GetRandomNumberInRange(0.0f, TWO_PI));
				const DefaultScenarioInfo defaultScenarioInfo = CScenarioManager::GetDefaultScenarioInfoForRandomPed(vSpawnCoords, modelId.GetModelIndex());
				CPed* pShark = CPedPopulation::AddPed(modelId.GetModelIndex(), vSpawnCoords, fHeading, NULL, false, 
					-1, NULL, false, false, false, false, defaultScenarioInfo, false, false);

				if (pShark)
				{
					// Do all the normal spawning stuff done in the pedpopulation.cpp code.
					CPedPopulation::OnPedSpawned(*pShark);
					pShark->SetDefaultDecisionMaker();
					pShark->SetCharParamsBasedOnManagerType();
					pShark->GetPedIntelligence()->SetDefaultRelationshipGroup();
					pShark->SetDefaultRemoveRangeMultiplier();
					pShark->SetPedConfigFlag(CPED_CONFIG_FLAG_PreferNoPriorityRemoval, true);

					// If onscreen, tell it to fade in.
					if (CPedPopulation::IsCandidateInViewFrustum(vSpawnCoords, pShark->GetCapsuleInfo()->GetHalfWidth(), 9999.0f))
					{
						pShark->GetLodData().SetResetDisabled(false);
						pShark->GetLodData().SetResetAlpha(true);
					}

					// Give it something to do.
					CTask* pTask = NULL;
					if (bTellItToKillThePlayer)
					{
						// Tell the shark to kill the player.
						pTask = rage_new CTaskSharkAttack(pPlayer);
					}
					else
					{
						// Just swim around.
						pTask = rage_new CTaskSwimmingWander();
					}
					CEventGivePedTask taskEvent(PED_TASK_PRIORITY_PRIMARY, pTask);
					pShark->GetPedIntelligence()->AddEvent(taskEvent);

					// Note that a shark has been released.
					m_uLastSharkSpawn = fwTimer::GetTimeInMilliseconds();
					return true;
				}
				else
				{
					// PedFactory failed.
					return false;
				}
			}
			else
			{
				// Unable to find a good place to spawn the shark.
				return false;
			}
		}
		else
		{
			// Couldn't find the right shark model, consider this as an unrecoverable failure.
			m_bCanSpawnSharkModels = false;
			return false;
		}
	}
	// Without a player you can't spawn sharks ever.
	m_bCanSpawnSharkModels = false;
	return false;
}
