// File header
#include "Task/Scenario/ScenarioManager.h"

// Framework headers
#include "ai/aichannel.h"

// Game headers
#include "ai/ambient/AmbientModelSetManager.h"
#include "ai/ambient/ConditionalAnimManager.h"
#include "ai/BlockingBounds.h"
#include "Audio/speechmanager.h"
#include "Bank/Group.h"
#include "Camera/CamInterface.h"
#include "Camera/Debug/DebugDirector.h"
#include "Debug/DebugScene.h"
#include "Game/Clock.h"
#include "Game/ModelIndices.h"
#include "Game/Weather.h"
#include "Network/Network.h"
#include "Peds/Ped.h"
#include "Peds/PedCapsule.h"
#include "Peds/PedDebugVisualiser.h"
#include "Peds/PedFactory.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PedPlacement.h"
#include "Peds/PedPopulation.h"
#include "Peds/PopCycle.h"
#include "Peds/population_channel.h"
#include "peds/rendering/PedVariationDebug.h"
#include "Peds/WildlifeManager.h"
#include "Physics/Physics.h"
#include "Scene/World/GameWorld.h"
#include "scene/EntityIterator.h"
#include "script/script.h"
#include "script/script_cars_and_peds.h"
#include "script/script_brains.h"
#include "streaming/PopulationStreaming.h"
#include "streaming/streaming.h"
#include "streaming/streamingvisualize.h"
#include "Task/Default/AmbientAnimationManager.h"
#include "Task/Default/Patrol/PatrolRoutes.h"
#include "task/Default/TaskPlayer.h"
#include "Task/Default/TaskUnalerted.h"
#include "Task/General/TaskBasic.h"
#include "Task/Scenario/Info/ScenarioInfoConditions.h"
#include "Task/Scenario/Info/ScenarioInfoManager.h"
#include "Task/Scenario/ScenarioDebug.h"
#include "Task/Scenario/ScenarioChaining.h"
#include "Task/Scenario/ScenarioChainingTests.h"
#include "Task/Scenario/ScenarioClustering.h"
#include "Task/Scenario/ScenarioPointManager.h"
#include "Task/Scenario/ScenarioPointRegion.h"
#include "Task/Scenario/ScenarioVehicleManager.h"
#include "Task/Scenario/Types/TaskCoupleScenario.h"
#include "Task/Scenario/Types/TaskDeadBodyScenario.h"
#include "Task/Scenario/Types/TaskMoveBetweenPointsScenario.h"
#include "Task/Scenario/Types/TaskParkedVehicleScenario.h"
#include "Task/Scenario/Types/TaskUseScenario.h"
#include "Task/Scenario/Types/TaskWanderingInRadiusScenario.h"
#include "Task/Scenario/Types/TaskWanderingScenario.h"
#include "task/Service/Army/TaskArmy.h"
#include "Task/Service/Fire/TaskFirePatrol.h"
#include "Task/Service/Medic/TaskAmbulancePatrol.h"
#include "Task/Service/Police/TaskPolicePatrol.h"
#include "Task/Vehicle/TaskCar.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Task/Vehicle/TaskRideTrain.h"
#include "Vehicles/cargen.h"
#include "Vehicles/Trailer.h"
#include "Vehicles/train.h"
#include "vehicles\boat.h"
#include "Vehicles/VehicleFactory.h"
#include "Vehicles/vehiclepopulation.h"
#include "Vehicles/LandingGear.h"
#include "Vehicles/Metadata/VehicleLayoutInfo.h"
#include "Vehicles/Metadata/VehicleMetadataManager.h"
#include "Vehicles/Metadata/VehicleScenarioLayoutInfo.h"
#include "Vehicles/Metadata/VehicleSeatInfo.h"
#include "Vehicles/Planes.h"

AI_OPTIMISATIONS()

EXT_PF_GROUP(ScenarioSpawning);
EXT_PF_TIMER(CreateScenarioPedTotal);
EXT_PF_TIMER(CreateScenarioPedPreSpawnSetup);
EXT_PF_TIMER(CreateScenarioPedPostSpawnSetup);
EXT_PF_TIMER(CreateScenarioPedZCoord);
EXT_PF_TIMER(CreateScenarioPedAddPed);
EXT_PF_TIMER(CreateScenarioPedConvertToDummy);
EXT_PF_TIMER(SetupGroupSpawnPoint);

EXT_PF_TIMER(CollisionChecks);	// pedpopulation.cpp

namespace AIStats
{
	EXT_PF_TIMER(CreateScenarioPed);
	EXT_PF_TIMER(GetPedTypeForScenario);
}
using namespace AIStats;

//-----------------------------------------------------------------------------


CAmbientModelSetFilterForScenario::CAmbientModelSetFilterForScenario()
: m_RequiredGender(GENDER_DONTCARE)
, m_BlockedModels(NULL)
, m_ConditionalAnims(NULL)
, m_NumBlockedModelSets(0)
, m_bProhibitBulkyItems(false)
#if __ASSERT
, m_bModelWasSuppressed(false)
, m_bIgnoreGenderFailure(false)
, m_bAmbientModelWasBlocked(false)
#endif
{}

bool CAmbientModelSetFilterForScenario::Match(u32 modelHash) const
{
	fwModelId modelId;
	if(modelHash != 0)
	{
		CModelInfo::GetBaseModelInfoFromHashKey(modelHash, &modelId);
	}

	if (!CPedPopulation::CheckPedModelGenderAndBulkyItems(modelId, m_RequiredGender, m_bProhibitBulkyItems))
	{
		return false;
	}

	//TODO ... other checks here

	//////////////////////////////////////////////////////////////////////////
	//NOTE: This should always be the last thing to be executed because if a model meets all requirements 
	// but is prevented from spawning because of restriction or suppression then it is not a data
	// error which we assert about in CAmbientModelSet::GetRandomModelHash if we dont find any valid models
	//////////////////////////////////////////////////////////////////////////
	if (modelId.IsValid() && CScriptPeds::HasPedModelBeenRestrictedOrSuppressed(modelId.GetModelIndex()))
	{
#if __ASSERT
		m_bModelWasSuppressed = true;
#endif
		return false;
	}

	if (CScenarioManager::BlockedModelSetsArrayContainsModelHash(m_BlockedModels, m_NumBlockedModelSets, modelHash))
	{
		ASSERT_ONLY(m_bAmbientModelWasBlocked = true);
		return false;
	}

	if (m_ConditionalAnims)
	{
		CScenarioCondition::sScenarioConditionData conditionData;
		conditionData.m_ModelId = modelId;
		if (!m_ConditionalAnims->CheckForMatchingPopulationConditionalAnims(conditionData))
		{
			return false;
		}
	}

	return true;
}

#if __ASSERT
bool CAmbientModelSetFilterForScenario::FailureShouldTriggerAsserts() const
{
	//if the reason no valid models were found was because something was suppressed then 
	// we should not assert because things are working as expected and there is no
	// data setup issue.
	if (m_bModelWasSuppressed)
	{
		return false;
	}
	else if (m_bIgnoreGenderFailure)
	{
		return false;
	}
	else if (m_bAmbientModelWasBlocked)
	{
		return false;
	}

	return CAmbientModelSetFilter::FailureShouldTriggerAsserts();
}
#endif	// __ASSERT

#if __BANK
void CAmbientModelSetFilterForScenario::GetFilterDebugInfoString( char* strOut, int strMaxLen ) const
{
	const char* genderStr;
	switch(m_RequiredGender)
	{
	case GENDER_MALE:
		genderStr = "Male";
		break;
	case GENDER_FEMALE:
		genderStr = "Female";
		break;
	case GENDER_DONTCARE:
		genderStr = "Don't care";
		break;
	default:
		Assert(0);
		genderStr = "?";
		break;
	}
	formatf(strOut, strMaxLen, "CAmbientModelSetFilterForScenario gender = %s", genderStr);
}
#endif // __BANK

namespace
{
	/*
	PURPOSE
		Model set filter to that only allows vehicles that can be used for broken down scenarios.
	*/
	class CAmbientModelSetFilterScenarioVehicle : public CAmbientModelSetFilter
	{
	public:
		explicit CAmbientModelSetFilterScenarioVehicle(const CScenarioVehicleInfo& info)
				: m_MustBeAbleToBreakDown(false)
		{
			if(info.GetIsFlagSet(CScenarioInfoFlags::VehicleCreationRuleBrokenDown))
			{
				m_MustBeAbleToBreakDown = true;
			}
		}

		virtual bool Match(u32 modelHash) const
		{
			fwModelId modelId;
			if(modelHash != 0)
			{
				const CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey(modelHash, &modelId);
				if(pModelInfo && aiVerify(pModelInfo->GetModelType() == MI_TYPE_VEHICLE))
				{
					const CVehicleModelInfo* pVehInfo = static_cast<const CVehicleModelInfo*>(pModelInfo);
					if(m_MustBeAbleToBreakDown)
					{
						if(!CScenarioManager::CanVehicleModelBeUsedForBrokenDownScenario(*pVehInfo))
						{
							return false;
						}
					}

					if(CScriptCars::GetSuppressedCarModels().HasModelBeenSuppressed(modelId.GetModelIndex()))
					{
						return false;
					}
					return true;
				}
			}

			return false;
		}

#if __BANK
		virtual void GetFilterDebugInfoString(char* strOut, int strMaxLen) const
		{
			formatf(strOut, strMaxLen, "CAmbientModelSetFilterScenarioVehicle");
		}
#endif

	protected:
		bool m_MustBeAbleToBreakDown;
	};
}

//-----------------------------------------------------------------------------
// CScenarioSpawnCheckCache

void CScenarioSpawnCheckCache::Entry::UpdateTime(u32 timeUntilInvalidMs)
{
	// Add a little bit of randomness (1/8th) to the time, so that if we start
	// out by checking multiple points in a single frame, it's more likely that
	// the cache entries don't all expire at the same time.
	const u32 currentTime = fwTimer::GetTimeInMilliseconds();
	const u32 maxRandomAdd = (timeUntilInvalidMs >> 3);
	const u32 randomAdd = fwRandom::GetRandomNumberInRange(0, maxRandomAdd);
	m_TimeNoLongerValid = currentTime + timeUntilInvalidMs + randomAdd;
}

void CScenarioSpawnCheckCache::Update()
{
	u32 currentTime = fwTimer::GetTimeInMilliseconds();

	// Loop over all cache entries and remove any that have expired, or
	// point to points that no longer exist. This doesn't completely eliminate
	// the risk of a point getting destroyed and then recycled with the same pointer
	// on the same frame, but it's not very likely.
	int numEntries = m_CacheEntries.GetCount();
	for(int i = 0; i < numEntries; i++)
	{
		bool remove = false;
		if(!CScenarioPoint::_ms_pPool->IsValidPtr(m_CacheEntries[i].m_Point))
		{
			remove = true;
		}
		else if(currentTime >= m_CacheEntries[i].m_TimeNoLongerValid)
		{
			remove = true;
		}

		if(remove)
		{
			m_CacheEntries.DeleteFast(i);
			i--;
			numEntries--;
		}
	}
}


CScenarioSpawnCheckCache::Entry& CScenarioSpawnCheckCache::FindOrCreateEntry(CScenarioPoint& pt, int scenarioTypeReal)
{
	// First, do a linear search over all used cache entries and see if the point
	// is already in there.
	const int numEntries = m_CacheEntries.GetCount();
	for(int i = 0; i < numEntries; i++)
	{
		Entry& e = m_CacheEntries[i];
		if(e.m_Point == &pt && e.m_ScenarioType == scenarioTypeReal)
		{
			return e;
		}
	}

	// We need to insert into the cache, so if it's full, we need to remove something.
	if(m_CacheEntries.IsFull())
	{
		// Loop over all the entries and remove the oldest one (or, the one that would
		// expire next), or any that doesn't have a valid pointer.
		int entryToDelete = -1;
		u32 oldestTime = 0xffffffff;
		for(int i = 0; i < numEntries; i++)
		{
			const Entry& e = m_CacheEntries[i];
			if(!CScenarioPoint::_ms_pPool->IsValidPtr(e.m_Point))
			{
				entryToDelete = i;
				break;
			}

			if(e.m_TimeNoLongerValid <= oldestTime)
			{
				oldestTime = e.m_TimeNoLongerValid;
				entryToDelete = i;
			}
		}

		m_CacheEntries.DeleteFast(entryToDelete);
	}

	// We must now have space in the cache, so create a new entry.
	Entry& e = m_CacheEntries.Append();
	e.m_Point = &pt;
	e.m_ScenarioType = scenarioTypeReal;
	e.m_FailedCheck = false;
	e.m_PassedEarlyCollisionCheck = false;
	e.m_PassedEarlyModelCheck = false;
	e.m_TimeNoLongerValid = 0;	// Will potentially be deleted right away if we don't use it.
	return e;
}


void CScenarioSpawnCheckCache::RemoveEntries(CScenarioPoint& pt)
{
	// Remove all entries pointing for the given scenario point. Note that
	// there could conceivably be more than one with different scenario types,
	// but in that case, we probably would want to remove them all (since we
	// probably just spawned something at this point).
	int numEntries = m_CacheEntries.GetCount();
	for(int i = 0; i < numEntries; i++)
	{
		Entry& e = m_CacheEntries[i];
		if(e.m_Point == &pt)
		{
			m_CacheEntries.DeleteFast(i);
			i--;
			numEntries--;
		}
	}
}

#if !__FINAL

void CScenarioSpawnCheckCache::DebugDraw() const
{
#if DEBUG_DRAW
	const int numEntries = m_CacheEntries.GetCount();
	for(int i = 0; i < numEntries; i++)
	{
		const Entry& e = m_CacheEntries[i];
		if(!CScenarioPoint::_ms_pPool->IsValidPtr(e.m_Point))
		{
			continue;
		}

		char buff[256];
		formatf(buff, "%s%s%s", e.m_FailedCheck ? "Failed\n" : "", e.m_PassedEarlyCollisionCheck ? "Passed Collision\n" : "", e.m_PassedEarlyModelCheck ? "Passed Model" : "");
		if(!buff[0])
		{
			buff[0] = '?';
			buff[1] = '\0';
		}
		const Vec3V posV = e.m_Point->GetPosition();
		grcDebugDraw::Text(posV, Color_yellow, 0, 0, buff);
	}
#endif	// DEBUG_DRAW
}

#endif	// !__FINAL

//-----------------------------------------------------------------------------

CScenarioChainTests* CScenarioManager::sm_ScenarioChainTests;
CVehicleScenarioManager* CScenarioManager::sm_VehicleScenarioManager;
CScenarioSpawnCheckCache CScenarioManager::sm_SpawnCheckCache;

bank_u32   CScenarioManager::ms_iExtendedScenarioLodDist = 500;
atArray<CScenarioPoint*> CScenarioManager::sm_ScenarioPointForCarGen;
atFixedArray<CScenarioManager::CSpawnedVehicleInfo, CScenarioManager::kMaxSpawnedVehicles> CScenarioManager::sm_SpawnedVehicles;
CScenarioSpawnHistory CScenarioManager::sm_SpawnHistory;
int CScenarioManager::sm_ScenarioProbabilitySeedOffset;

atHashString CScenarioManager::MODEL_SET_USE_POPULATION("UsePopulation",0xA7548A2);

bool CScenarioManager::sm_ReadyForOnAddCalls = false;

bool CScenarioManager::sm_AreNormalScenarioExitsSuppressed = false;
bool CScenarioManager::sm_IsScenarioAttractionSuppressed = false;
bool CScenarioManager::sm_ScenarioBreakoutExitsSuppressed = false;
bool CScenarioManager::sm_RespectSpawnInSameInteriorFlag = true;

float CScenarioManager::sm_ClusterRangeForVehicleSpawn = 250.0f;
float CScenarioManager::sm_ExtendedExtendedRangeForVehicleSpawn = 400.0f;
float CScenarioManager::sm_ExtendedExtendendRangeMultiplier = 2.0f;
float CScenarioManager::sm_MaxOnScreenMinSpawnDistForCluster = 140.0f;
float CScenarioManager::sm_StreamingUnderPressureSpeedThreshold = 22.0f;
s16 CScenarioManager::sm_CurrentRegionForCarGenUpdate = -1;
s16 CScenarioManager::sm_CurrentPointWithinRegionForCarGenUpdate = -1;
s16 CScenarioManager::sm_CurrentClusterWithinRegionForCarGenUpdate = -1;
u8 CScenarioManager::sm_CarGenHour = 0xff;
bool CScenarioManager::sm_CarGensNeedUpdate = false;
bool CScenarioManager::sm_ShouldExtendExtendedRangeFurther = false;
bool CScenarioManager::sm_StreamingUnderPressure = false;
u32 CScenarioManager::sm_AbandonedVehicleCheckTimer = 0;

void CScenarioManager::Init(unsigned initMode)
{
	if(initMode == INIT_BEFORE_MAP_LOADED)
	{
		taskAssert(!sm_VehicleScenarioManager);	// Not expected to exist at this point.
		delete sm_VehicleScenarioManager;		// Just in case, to guard against leaks.
		sm_VehicleScenarioManager = rage_new CVehicleScenarioManager;

		taskAssert(!sm_ScenarioChainTests);
		delete sm_ScenarioChainTests;
		sm_ScenarioChainTests = rage_new CScenarioChainTests;

		INIT_SCENARIOINFOMGR;
		INIT_SCENARIOPOINTMGR;
		INIT_SCENARIOCLUSTERING;

		CScenarioEntityOverridePostLoadUpdateQueue::InitClass();

		CConditionalAnimStreamingVfxManager::InitClass();

#if SCENARIO_VERIFY_ENTITY_OVERRIDES
		CScenarioEntityOverrideVerification::InitClass();
#endif	// SCENARIO_VERIFY_ENTITY_OVERRIDES
	}
	else if(initMode == INIT_SESSION)
	{
		// At this point, both SCENARIOINFOMGR and CONDITIONALANIMSMGR have hopefully loaded,
		// so we can check to make sure we have unique names on the conditional animation groups: 
		SCENARIOINFOMGR.CheckUniqueConditionalAnimNames();

		// Reset the last spawn times on each of the scenario infos.
		SCENARIOINFOMGR.ResetLastSpawnTimes();

		// Set up the array for mapping car generators to CScenarioPoints. We could get
		// away without this if we searched through all scenario points, or we could have had
		// just a compact array of {cargen,point} pairs, but that would require some more
		// searching at runtime. This data could also be in a regular member of CCarGenerator,
		// but 
		sm_ScenarioPointForCarGen.Reset();
		int iSize = CTheCarGenerators::GetSize();
		sm_ScenarioPointForCarGen.Resize(iSize);
		for(int i = 0; i < iSize; i++)
		{
			sm_ScenarioPointForCarGen[i] = NULL;
		}

		// Grab a random number to use as the base for computing our own random seeds.
		// Without this, the probability rolls for vehicle scenarios could conceivably
		// be predictable to the player based on the time since the game started.
		sm_ScenarioProbabilitySeedOffset = g_ReplayRand.GetInt();

		// If we already have points that we couldn't call OnAddScenario() for when they
		// were adedd (because other systems may not have been initialized yet), call
		// OnAddScenario() now.
		if(!sm_ReadyForOnAddCalls)
		{
			sm_ReadyForOnAddCalls = true;

			SCENARIOPOINTMGR.HandleInitOnAddCalls();
		}

		sm_AbandonedVehicleCheckTimer = 0;
	}
}

void CScenarioManager::Shutdown(unsigned shutdownMode)
{
	if(shutdownMode == SHUTDOWN_SESSION)
	{
		// If we don't do this, CScenarioPoint::CleanUp() could be clearing out
		// CScenarioPoint references in the tracked data of clusters and cause
		// crashes. Note that we probably don't want to delete the CSPClusterFSMWrapper
		// objects here since their existence is tied to the CScenarioPointRegions,
		// and we don't explicitly delete those here.
		SCENARIOCLUSTERING.ResetAllClusters();

		SCENARIOPOINTMGR.HandleShutdownOnRemoveCalls();
		sm_ReadyForOnAddCalls = false;

		// Any regions that were loaded would have had ClearAllKnownRefs() called
		// on the points. In the chaining graph, we use RegdScenarioPnt internally
		// for the mapping to the chaining nodes, and those will all be NULL now!
		// We can fix that by forcing an update of this mapping.
		const int numActiveRegions = SCENARIOPOINTMGR.GetNumActiveRegions();
		for(int i = 0; i < numActiveRegions; i++)
		{
			CScenarioPointRegion& reg = SCENARIOPOINTMGR.GetActiveRegion(i);
			reg.UpdateNodeToScenarioPointMapping(true);
		}

		sm_ScenarioPointForCarGen.Reset();

		CConditionalAnimStreamingVfxManager::GetInstance().Reset();
		SCENARIOINFOMGR.ClearDLCScenarioInfos();
	}
	else if(shutdownMode == SHUTDOWN_WITH_MAP_LOADED)
	{
#if SCENARIO_VERIFY_ENTITY_OVERRIDES
		CScenarioEntityOverrideVerification::ShutdownClass();
#endif	// SCENARIO_VERIFY_ENTITY_OVERRIDES

		CConditionalAnimStreamingVfxManager::ShutdownClass();

		CScenarioEntityOverridePostLoadUpdateQueue::ShutdownClass();

		SHUTDOWN_SCENARIOCLUSTERING;
		SHUTDOWN_SCENARIOPOINTMGR;
		SHUTDOWN_SCENARIOINFOMGR;

		taskAssert(sm_ScenarioChainTests);
		delete sm_ScenarioChainTests;
		sm_ScenarioChainTests = NULL;

		taskAssert(sm_VehicleScenarioManager);
		delete sm_VehicleScenarioManager;
		sm_VehicleScenarioManager = NULL;
	}
}


void CScenarioManager::Update()
{
#if GTA_REPLAY
	// Don't spawn anything during replay playback as the replay should be
	// responsible for populating the world with what we need.
	if(CReplayMgr::IsEditModeActive())
		return;
#endif
	STRVIS_AUTO_CONTEXT(strStreamingVisualize::SCENARIOMGR);

	UpdateConditions();
	UpdateStreamingUnderPressure();

	CConditionalAnimStreamingVfxManager::GetInstance().Update();

	sm_ScenarioChainTests->Update();

	// Update the spawn check cache entries.
	sm_SpawnCheckCache.Update();

	// Check if we should use an extended range because we are flying, etc.
	UpdateExtendExtendedRangeFurther();

	// Maintain sm_SpawnedVehicles[] by removing elements with NULL vehicle
	// pointers. We don't really have to do this on every frame, but should be very
	// cheap as we don't need any data outside of the tiny array.
	int numSpawnedVehicles = sm_SpawnedVehicles.GetCount();
	for(int i = numSpawnedVehicles - 1; i >= 0; i--)
	{
		if(sm_SpawnedVehicles[i].m_Vehicle)
		{
			continue;
		}

		// This is basically an atFixedArray::DeleteFast(), except that we reset
		// the RegdVeh to NULL to avoid using up extra references nodes past the
		// end of the array.
		numSpawnedVehicles--;
		if(numSpawnedVehicles != i)
		{
			sm_SpawnedVehicles[i] = sm_SpawnedVehicles[numSpawnedVehicles];
			sm_SpawnedVehicles[numSpawnedVehicles].m_Vehicle.Reset(NULL);
		}
		sm_SpawnedVehicles.Pop();
	}		
	
	sm_SpawnHistory.Update();

	CScenarioChainingGraph::UpdatePointChainUserInfo();
	SCENARIOCLUSTERING.Update();

	CScenarioEntityOverridePostLoadUpdateQueue::GetInstance().Update();

	// Add or remove car generators for time of day changes, etc.
	UpdateCarGens();

#if SCENARIO_VERIFY_ENTITY_OVERRIDES
	CScenarioEntityOverrideVerification::GetInstance().Update();
#endif	// SCENARIO_VERIFY_ENTITY_OVERRIDES

	//Update the spatial array.
	SCENARIOPOINTMGR.UpdateNonRegionPointSpatialArray();
}

void CScenarioManager::ResetExclusiveScenarioGroup()
{
	SCENARIOPOINTMGR.SetExclusivelyEnabledScenarioGroupIndex(-1);
}

#if __ASSERT

void CScenarioManager::GetInfoStringForScenarioUsingCarGen(char* strBuff, int buffSize, int carGenIndex)
{
	if(carGenIndex < 0 || carGenIndex >= sm_ScenarioPointForCarGen.GetCount())
	{
		formatf(strBuff, buffSize, "(index %d out of range)", carGenIndex);
		return;
	}

	const CScenarioPoint* pt = sm_ScenarioPointForCarGen[carGenIndex];
	if(pt)
	{
		const Vec3V pos = pt->GetWorldPosition();
		formatf(strBuff, buffSize, "%s at %.1f, %.1f, %.1f (%p)", GetScenarioName(pt->GetScenarioTypeVirtualOrReal()), pos.GetXf(), pos.GetYf(), pos.GetZf(), pt);
	}
	else
	{
		formatf(strBuff, buffSize, "(no valid point)");
	}
}

#endif	// __ASSERT


void CScenarioManager::UpdateConditions()
{
	sm_RespectSpawnInSameInteriorFlag = true;

	// A bit of a game-specific hack: if the player is riding a train, allow spawning
	// on OnlySpawnInSameInterior points even if not in the same interior (for approaching
	// subway stations).
	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if(pPlayer)
	{
		const CTaskPlayerOnFoot* playerOnFootTask = static_cast<const CTaskPlayerOnFoot*>(pPlayer->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PLAYER_ON_FOOT));
		if(playerOnFootTask && playerOnFootTask->GetState() == CTaskPlayerOnFoot::STATE_RIDE_TRAIN)
		{
			sm_RespectSpawnInSameInteriorFlag = false;
		}
	}
}

void CScenarioManager::UpdateStreamingUnderPressure()
{
	bool underPressure = false;

	// Skip this if in the population is in startup mode. If so, we are hopefully
	// already faded out or in some similar situation where we expect to stream a lot
	// in order to end up with a populated world.
	if(!CPedPopulation::ShouldUseStartupMode())
	{
		// The streaming system is under pressure if loading priority objects.
		if(strStreamingEngine::GetIsLoadingPriorityObjects())
		{
			underPressure = true;
		}
		else
		{
			// We think the streaming system is under pressure because the player is driving fast in a vehicle.
			const float fPlayerSpeedSq = CGameWorld::FindLocalPlayerSpeed().Mag2();
			if(fPlayerSpeedSq > square(sm_StreamingUnderPressureSpeedThreshold))
			{
				underPressure = true;
			}
		}
	}

	sm_StreamingUnderPressure = underPressure;
}

void CScenarioManager::ProcessPreScripts()
{
	SetScenarioExitsSuppressed(false);

	SetScenarioAttractionSuppressed(false);
	
	SetScenarioBreakoutExitsSuppressed(false);
}

//-------------------------------------------------------------------------
// Returns the scenario name from the type
//-------------------------------------------------------------------------
const char* CScenarioManager::GetScenarioName(s32 BANK_ONLY(iScenarioType))
{
#if __BANK
	if(SCENARIOINFOMGR.IsVirtualIndex(iScenarioType))
	{
		return SCENARIOINFOMGR.GetNameForScenario(iScenarioType);
	}
	const CScenarioInfo* pInfo = SCENARIOINFOMGR.GetScenarioInfoByIndex(iScenarioType);
	if(pInfo)
	{
		return pInfo->GetName();
	}
	aiAssertf(0,"Scenario type [%d] not found", iScenarioType);
#endif
	return "UNKNOWN_SCENARIO";
}

//-------------------------------------------------------------------------
// Returns the scenario type from the name
//-------------------------------------------------------------------------
s32 CScenarioManager::GetScenarioTypeFromName(const char* szScenario)
{
	u32 uiHash = atStringHash(szScenario);

	s32 iScenarioType = GetScenarioTypeFromHashKey(uiHash);

	aiAssertf(iScenarioType != Scenario_Invalid,"Scenario: %s not found in metadata, does it need to be added to RAVE?", szScenario);

	return iScenarioType;
}

//-------------------------------------------------------------------------
// Returns the scenario type from the hash key
//-------------------------------------------------------------------------
s32 CScenarioManager::GetScenarioTypeFromHashKey(u32 uHashKey)
{
	return SCENARIOINFOMGR.GetScenarioIndex(uHashKey);
}


//-------------------------------------------------------------------------
// Returns the conditional clip data for a scenario type
//-------------------------------------------------------------------------
const CConditionalAnimsGroup * CScenarioManager::GetConditionalAnimsGroupForScenario(s32 scenarioType)
{
	const CScenarioInfo * pScenarioInfo = GetScenarioInfo(scenarioType);
	Assertf(pScenarioInfo,"Unknown scenario type %d",scenarioType);

	if (pScenarioInfo)
	{
		return pScenarioInfo->GetClipData();
	}

	return NULL;
}

//-------------------------------------------------------------------------
// Returns the scenario info data for a particular scenario
//-------------------------------------------------------------------------
const CScenarioInfo* CScenarioManager::GetScenarioInfo(s32 iScenarioType)
{
	return SCENARIOINFOMGR.GetScenarioInfoByIndex(iScenarioType);
}

/*
PURPOSE
	Check if a given scenario type has a spawn interval limit, and if so,
	return false if it spawned something within that interval.
PARAMS
	scenarioType - the index of the scenario info
RETURNS
	True if we are allowed to spawn, i.e. if nothing spawned within the
	interval.
*/
bool CScenarioManager::CheckScenarioSpawnInterval(int scenarioType)
{
	const CScenarioInfo* pScenarioInfo = GetScenarioInfo(scenarioType);

	const bool bHasNoTimeOverride = (pScenarioInfo->GetSpawnInterval() == 0 || pScenarioInfo->GetLastSpawnTime() == 0);
	if(bHasNoTimeOverride || (pScenarioInfo->GetLastSpawnTime() + pScenarioInfo->GetSpawnInterval()) < fwTimer::GetTimeInMilliseconds())
	{
		return true;
	}
	return false;
}

//-------------------------------------------------------------------------
// Returns true if its a valid time for the scenario
//-------------------------------------------------------------------------
bool CScenarioManager::CheckScenarioTimesAndProbability(const CScenarioPoint* pt, s32 iScenarioType, bool checkProbability, bool checkInterval BANK_ONLY(, bool bRegisterSpawnFailure))
{
	unsigned iModelSetIndex = pt ? pt->GetModelSetIndex() : CScenarioPointManager::kNoCustomModelSet;

	TUNE_GROUP_BOOL(SCENARIO_SPAWN_DEBUG, bIgnoreVariousSpawnConds, false);
	if(!bIgnoreVariousSpawnConds && !CheckVariousSpawnConditions(iScenarioType, iModelSetIndex))
	{
#if __BANK
		if (bRegisterSpawnFailure)
			CPedPopulation::ms_populationFailureData.RegisterSpawnFailure(CPedPopulation::PedPopulationFailureData::ST_Scenario, CPedPopulation::PedPopulationFailureData::FR_scenarioTimeAndProbabilityFailVarious, pt ? VEC3V_TO_VECTOR3(pt->GetPosition()) : Vector3(Vector3::ZeroType));
#endif
		return false;
	}

	TUNE_GROUP_BOOL(SCENARIO_SPAWN_DEBUG, bIgnoreSpawnInterval, false);
	if(!bIgnoreSpawnInterval && checkInterval && !CheckScenarioSpawnInterval(iScenarioType))
	{
#if __BANK
		if (bRegisterSpawnFailure)
			CPedPopulation::ms_populationFailureData.RegisterSpawnFailure(CPedPopulation::PedPopulationFailureData::ST_Scenario, CPedPopulation::PedPopulationFailureData::FR_scenarioTimeAndProbabilityFailSpawnInterval, pt ? VEC3V_TO_VECTOR3(pt->GetPosition()) : Vector3(Vector3::ZeroType));
#endif
		return false;
	}

	CScenarioCondition::sScenarioConditionData conditionData;

	TUNE_GROUP_BOOL(SCENARIO_SPAWN_DEBUG, bIgnoreTimeOverrides, false);
	if(pt && pt->HasTimeOverride())
	{	
		if(!bIgnoreTimeOverrides && !CClock::IsTimeInRange(pt->GetTimeStartOverride(), pt->GetTimeEndOverride()))
		{
#if __BANK
			if (bRegisterSpawnFailure)
				CPedPopulation::ms_populationFailureData.RegisterSpawnFailure(CPedPopulation::PedPopulationFailureData::ST_Scenario, CPedPopulation::PedPopulationFailureData::FR_scenarioTimeAndProbabilityFailTimeOverride, pt ? VEC3V_TO_VECTOR3(pt->GetPosition()) : Vector3(Vector3::ZeroType));
#endif
			return false;
		}

		// Ignore the time condition in the check below, as we have already passed the time condition override on the point.
		conditionData.iScenarioFlags |= CTaskUseScenario::SF_IgnoreTimeCondition;
	}

	TUNE_GROUP_BOOL(SCENARIO_SPAWN_DEBUG, bIgnoreStreamingPressure, false);
	const CScenarioInfo* pScenarioInfo = GetScenarioInfo(iScenarioType);

	// If the scenario is set to not spawn if streaming is under pressure and that's the current situation,
	// stop now, unless the point is set to high priority or in a cluster.
	if(pScenarioInfo->GetIsFlagSet(CScenarioInfoFlags::DontSpawnIfUnderStreamingPressure))
	{
		if(!bIgnoreStreamingPressure && CScenarioManager::IsStreamingUnderPressure() && (!pt || (!pt->IsHighPriority() && !pt->IsInCluster())))
		{
#if __BANK
			if (bRegisterSpawnFailure)
				CPedPopulation::ms_populationFailureData.RegisterSpawnFailure(CPedPopulation::PedPopulationFailureData::ST_Scenario, CPedPopulation::PedPopulationFailureData::FR_scenarioTimeAndProbabilityFailStreamingPressure, pt ? VEC3V_TO_VECTOR3(pt->GetPosition()) : Vector3(Vector3::ZeroType));
#endif
			return false;
		}
	}

	if (pt && pt->IsFlagSet(CScenarioPointFlags::IgnoreWeatherRestrictions))
	{
		conditionData.iScenarioFlags |= SF_IgnoreWeather;
	}

	TUNE_GROUP_BOOL(SCENARIO_SPAWN_DEBUG, bIgnoreConditions, false);
	const CScenarioConditionWorld* condition = pScenarioInfo->GetCondition();
	if(!bIgnoreConditions && (condition && !condition->Check(conditionData)))
	{
#if __BANK
		if (bRegisterSpawnFailure)
			CPedPopulation::ms_populationFailureData.RegisterSpawnFailure(CPedPopulation::PedPopulationFailureData::ST_Scenario, CPedPopulation::PedPopulationFailureData::FR_scenarioTimeAndProbabilityFailConditions, pt ? VEC3V_TO_VECTOR3(pt->GetPosition()) : Vector3(Vector3::ZeroType));
#endif
		return false;
	}

	TUNE_GROUP_BOOL(SCENARIO_SPAWN_DEBUG, bIgnoreProbability, false);
	if(!bIgnoreProbability && checkProbability && !CheckScenarioProbability(pt, iScenarioType))
	{
#if __BANK
		if (bRegisterSpawnFailure)
			CPedPopulation::ms_populationFailureData.RegisterSpawnFailure(CPedPopulation::PedPopulationFailureData::ST_Scenario, CPedPopulation::PedPopulationFailureData::FR_scenarioTimeAndProbabilityFailProb, pt ? VEC3V_TO_VECTOR3(pt->GetPosition()) : Vector3(Vector3::ZeroType));
#endif
		return false;
	}

	return true;
}


bool CScenarioManager::CheckScenarioProbability(const CScenarioPoint* pt, int scenarioType)
{
	static const int kRandPeriod = 30000;	// Allow a new random roll every 30 seconds.

	// Compute some integer to use as the seed for the seed. This is somewhat arbitrary,
	// but using the pointer of the point should be OK here, dividing by CScenarioPoint
	// should give us a denser range without giving adjacent points the same value.
	const int num = pt ? (((int)(size_t)pt)/sizeof(CScenarioPoint)) : scenarioType;

	// To compute the seed, we take this initial number, and multiple by some arbitrary constant.
	// We also take the current time and divide by the update period to get something that
	// changes occasionally, and we add a multiple of the initial number to the time so that not
	// all points change at the same instant. And, add the global offset so we don't always get the
	// same seed when running the game multiple times.
	const int seed = 100*num + (fwTimer::GetTimeInMilliseconds() + 500*num)/kRandPeriod + sm_ScenarioProbabilitySeedOffset;

	// Initialize a temporary random number generator using this seed.
	mthRandom rnd(seed);

	// If the point has a probability override, use that.
	if(pt && pt->HasProbabilityOverride())
	{
		// We do the check using integers here to prevent expensive int<->float conversions.
		return rnd.GetRanged(0, 99) < pt->GetProbabilityOverrideAsIntPercentage();
	}
	else
	{
		// Use the probability from the CScenarioInfo.
		Assert(scenarioType >= 0);
		const CScenarioInfo* pScenarioInfo = GetScenarioInfo(scenarioType);
		if(pScenarioInfo)
		{
			const float prob = pScenarioInfo->GetSpawnProbability();
			return rnd.GetFloat() < prob;
		}
		return false;
	}
}


bool CScenarioManager::CheckVariousSpawnConditions(int scenarioType, unsigned modelSetIndex)
{
	const CScenarioInfo* pScenarioInfo = GetScenarioInfo(scenarioType);

	if(!aiVerifyf(pScenarioInfo, "NULL Scenario Info for type [%d]", scenarioType))
	{
		return false;
	}

	// Never spawn peds at LookAt scenario points.
	if (pScenarioInfo->GetIsClass<CScenarioLookAtInfo>())
	{
		return false;
	}

	// Snipers only appear with a wanted level of 5 or higher
	if(pScenarioInfo->GetIsFlagSet(CScenarioInfoFlags::OnlySpawnWithHighWantedLevel))
	{
		if( CGameWorld::FindLocalPlayerWanted() && CGameWorld::FindLocalPlayerWanted()->GetWantedLevel() < WANTED_LEVEL5 )
		{
			return false;
		}
	}

	// Don't spawn the scenario in MP if flagged
	if(NetworkInterface::IsGameInProgress() && pScenarioInfo->GetIsFlagSet(CScenarioInfoFlags::NotAvailableInMultiplayer))
	{	
		return false;
	}

	// Don't spawn cops normal cops when the player has a high wanted level
	if(pScenarioInfo->GetIsFlagSet(CScenarioInfoFlags::DontSpawnWithHighWantedLevel))
	{
		if (CGameWorld::FindLocalPlayerWanted() && CGameWorld::FindLocalPlayerWanted()->GetWantedLevel() > WANTED_LEVEL1)
		{
			return false;
		}
	}

	// Don't spawn cop scenarios that should be turned off
	if (!CPedPopulation::GetAllowCreateRandomCopsOnScenarios())
	{
		// Scenario infos specifically marked as "don't spawn" because cops are involved
		if(pScenarioInfo->GetIsFlagSet(CScenarioInfoFlags::DontSpawnIfCopsAreOff))
		{
			return false;
		}

		// Scenario points that have cop models as part of the ambient model set
		if (modelSetIndex != CScenarioPointManager::kNoCustomModelSet)
		{
			const CAmbientPedModelSet* pModelSet = CAmbientModelSetManager::GetInstance().GetPedModelSet(modelSetIndex);
			if (pModelSet)
			{
				// Can't just check to see if cop model is in model set due to generic large ones :(
				if (pModelSet->GetHash() == ATSTRINGHASH("PARK_RANGER", 0x6fa71d2f) ||
					pModelSet->GetHash() == ATSTRINGHASH("PATROL", 0xf35d60fa) ||
					pModelSet->GetHash() == ATSTRINGHASH("PATROL_TORCH", 0x90959ab7) ||
					pModelSet->GetHash() == ATSTRINGHASH("POLICE", 0x79fbb0c5) ||
					pModelSet->GetHash() == ATSTRINGHASH("POLICE_FEMALE", 0x8031884f) ||
					pModelSet->GetHash() == ATSTRINGHASH("POLICE_GUARD", 0x084af246) ||
					pModelSet->GetHash() == ATSTRINGHASH("POLICE_HIGHWAY", 0xbeb3a4e4) ||
					pModelSet->GetHash() == ATSTRINGHASH("POLICE_IDLES", 0x778da8a8))
				{
					return false;
				}
			}
		}
	}

	return true;
}


bool CScenarioManager::CheckScenarioPointEnabled (const CScenarioPoint& scenarioPt, int indexWithinTypeGroup)
{
	if(!CheckScenarioPointEnabledByGroupAndAvailability(scenarioPt))
	{
		return false;
	}
	return CheckScenarioPointEnabledByType(scenarioPt, indexWithinTypeGroup);
}


bool CScenarioManager::CheckScenarioPointEnabledByGroupAndAvailability (const CScenarioPoint& scenarioPt)
{
	// Note: this function gets called by ShouldHaveCarGenUnderCurrentConditions(), i.e. influences whether the car
	// generator should be created or not. Therefore, if we add some additional conditions here, we
	// may also need to make sure that the car generators get refreshed when those conditions change.

	const int onlyScenarioGroupEnabled = SCENARIOPOINTMGR.GetExclusivelyEnabledScenarioGroupIndex();
	if(scenarioPt.GetScenarioGroup() != CScenarioPointManager::kNoGroup)
	{
		const int scenarioGroupIndex = scenarioPt.GetScenarioGroup() - 1;
		if(onlyScenarioGroupEnabled >= 0 && onlyScenarioGroupEnabled != scenarioGroupIndex)
		{
			return false;
		}
		if(!SCENARIOPOINTMGR.IsGroupEnabled(scenarioGroupIndex))
		{
			return false;
		}
	}
	else if(onlyScenarioGroupEnabled >= 0)
	{
		return false;
	}

	if(!scenarioPt.IsAvailability(CScenarioPoint::kBoth))
	{
		Assert(scenarioPt.IsAvailability(CScenarioPoint::kOnlySp) || scenarioPt.IsAvailability(CScenarioPoint::kOnlyMp));
		bool inMp = NetworkInterface::IsGameInProgress();
		bool needToBeInMp = scenarioPt.IsAvailability(CScenarioPoint::kOnlyMp);
		if(inMp != needToBeInMp)
		{
			return false;
		}
	}

	return true;
}

/*
PURPOSE
	If a scenario point has requirements of certain map data objects being loaded,
	this function will return false if these requirements are not currently satisfied.
PARAMS
	scenarioPt		- The point to check.
RETURNS
	True if the point has no special requirements, or if the requirements are satisfied.
*/
bool CScenarioManager::CheckScenarioPointMapData(const CScenarioPoint& scenarioPt)
{
	unsigned rimId = scenarioPt.GetRequiredIMap();
	if (rimId == CScenarioPointManager::kNoRequiredIMap)
		return true;

	strLocalIndex slotID = strLocalIndex(SCENARIOPOINTMGR.GetRequiredIMapSlotId(rimId));
	if (slotID.Get() != CScenarioPointManager::kRequiredImapIndexInMapDataStoreNotFound)
	{
		fwMapDataDef* def = fwMapDataStore::GetStore().GetSlot(slotID);
		if(def && def->IsLoaded())
		{
			return true;
		}
	}

	return false;
}

bool CScenarioManager::IsScenarioPointAttachedEntityUprootedOrBroken (const CScenarioPoint& scenarioPoint)
{
	// Is the object is broken?
	CEntity* pAttachedEntity = scenarioPoint.GetEntity();
	if(pAttachedEntity && pAttachedEntity->GetFragInst())
	{
		if(pAttachedEntity->GetFragInst()->GetPartBroken())
		{
			return true;
		}
	}

	if(pAttachedEntity && pAttachedEntity->GetIsTypeObject())
	{
		CObject* pObject = static_cast<CObject *>(pAttachedEntity);
		// Is the object in pieces?
		if ( pObject->GetOwnedBy() == ENTITY_OWNEDBY_FRAGMENT_CACHE )
		{
			return true;
		}

		// Has the object been moved or overturned?
		if ( pObject->m_nObjectFlags.bHasBeenUprooted )
		{
			return true;
		}
	}

	return false;
}


bool CScenarioManager::IsScenarioPointInBlockingArea(const CScenarioPoint& pt)
{
	Vec3V posV = pt.GetPosition();

	const int type = pt.GetScenarioTypeVirtualOrReal();

	const bool forVehicle = CScenarioManager::IsVehicleScenarioType(type);
	const bool forPed = !forVehicle;

	return CScenarioBlockingAreas::IsPointInsideBlockingAreas(RCC_VECTOR3(posV), forPed, forVehicle);
}


bool CScenarioManager::CheckScenarioPointEnabledByType(const CScenarioPoint& scenarioPt, int indexWithinTypeGroup)
{
	// Check if this scenario type has been disabled.
	const int scenarioTypeVirtualOrReal = scenarioPt.GetScenarioTypeVirtualOrReal();
	const int scenarioType = SCENARIOINFOMGR.GetRealScenarioType(scenarioTypeVirtualOrReal, indexWithinTypeGroup);
	if(!SCENARIOINFOMGR.IsScenarioTypeEnabled(scenarioType))
	{
		return false;
	}

	return true;
}

//-------------------------------------------------------------------------
// Returns true if its a valid time for the scenario
//-------------------------------------------------------------------------
bool CScenarioManager::CheckScenarioPointTimesAndProbability(const CScenarioPoint& scenarioPt, bool checkProbability, bool checkInterval, int indexWithinTypeGroup BANK_ONLY(, bool bRegisterSpawnFailure))
{
	const int scenarioTypeVirtualOrReal = scenarioPt.GetScenarioTypeVirtualOrReal();
	const int scenarioType = SCENARIOINFOMGR.GetRealScenarioType(scenarioTypeVirtualOrReal, indexWithinTypeGroup);

	return CheckScenarioTimesAndProbability(&scenarioPt, scenarioType, checkProbability, checkInterval BANK_ONLY(, bRegisterSpawnFailure));
}

/////////////////////////////////////////////////////////////////////////////////
// Checks to see if the area surrounding this position is overpopulated by similar scenarios
/////////////////////////////////////////////////////////////////////////////////
bool CScenarioManager::CheckScenarioPopulationCount(const s32 scenarioType, const Vector3& effectPos, float fRequestedClosestRange
		, const CDynamicEntity* pException, bool checkMaxInRange)
{	
	// Check for entities too close to allow this ped to spawn
	// AND check the scenario range conditions to make sure there aren't
	// already too many of these types of peds in the area
	const CScenarioInfo* pScenarioInfo = CScenarioManager::GetScenarioInfo(scenarioType);
	Assert(pScenarioInfo);

	// Note: this may be a bit misplaced, but we need something like it to prevent peds
	// from spawning at or using vehicle scenarios.
	if(pScenarioInfo->GetIsClass<CScenarioVehicleInfo>())
	{
		return false;
	}

	// Work out how many of this type of scenario are allowed and in what range
	float fScenarioInfoRange = 0.0f;
	s32 iMaxAllowedInRange = 999;
	bool bIncludeAllCopsOnFoot = false;
	bool bCheckNumberInRange = checkMaxInRange && CScenarioManager::GetRangeValuesForScenario(scenarioType, fScenarioInfoRange, iMaxAllowedInRange);

	// Override these values for cop scenarios when the player has a wanted level.
	// To limit the number of cops spawned on foot that interfere with the wanted level.
	if( pScenarioInfo->GetIsFlagSet(CScenarioInfoFlags::DontSpawnIfCopsAreOff) )
	{
		if( CGameWorld::FindLocalPlayerVehicle() && CGameWorld::FindLocalPlayerWanted() && CGameWorld::FindLocalPlayerWanted()->GetWantedLevel() > WANTED_LEVEL1 )
		{
			bCheckNumberInRange = checkMaxInRange;
			iMaxAllowedInRange = 2;
			fScenarioInfoRange = 250.0f;
			bIncludeAllCopsOnFoot = true;
		}
	}

	// fScenarioSearchRange is the maximum range we will look for peds performing the same scenario. [11/28/2012 mdawe]
	const float fScenarioSearchRange	= MAX( fRequestedClosestRange, fScenarioInfoRange );
	float fRejectIfPedWithinDisSq			= rage::square(fScenarioSearchRange);

	// fEntitySearchRange is the maximum range we will look for peds blocking the spawn just by being there. [11/28/2012 mdawe]
	const float fEntitySearchRange		= MAX( fScenarioSearchRange, CPedPopulation::GetMinDistFromScenarioSpawnToDeadPed() );

	// Maximum z-distance to consider peds within [11/28/2012 mdawe]
	const float fMaxZDiff = 2.0f;


	// There are some cases where we will use a different fRejectIfPedWithinDisSq. [11/28/2012 mdawe]
	Vector3 adjustedEffectPos = effectPos;
	if (pScenarioInfo->GetIsClass<CScenarioPlayAnimsInfo>() || 
		pScenarioInfo->GetIsClass<CScenarioSniperInfo>() || 
		pScenarioInfo->GetIsClass<CScenarioWanderingInfo>() || 
		pScenarioInfo->GetIsClass<CScenarioJoggingInfo>())
	{
		adjustedEffectPos.z += PED_HUMAN_GROUNDTOROOTOFFSET;
		fRejectIfPedWithinDisSq = rage::square(0.5f);
	}
	// Don't allow peds running control ambient task to spawn too close to another ped
	if (pScenarioInfo->GetIsClass<CScenarioControlAmbientInfo>() || pScenarioInfo->GetIsClass<CScenarioSkiingInfo>())
	{
		// Randomise the allowed spawn distance away from a nearby ped so for instance skiing peds aren't generated at uniform intervals
		fRejectIfPedWithinDisSq = CPedPopulation::ms_fCloseSpawnPointDistControlSq + fwRandom::GetRandomNumberInRange(0.0f, CPedPopulation::ms_fCloseSpawnPointDistControlOffsetSq);
	}

	// Iterate over all peds and dummies checking they aren't too close to prevent spawning
	// and make sure there aren't too many of each scenario being spawned in the same place
	
	// Note that even if fRejectIfPedWithinDisSq changed, we still get all entities within fEntitySearchRange [11/28/2012 mdawe]
	Vec3V vIteratorPos = VECTOR3_TO_VEC3V(adjustedEffectPos);
	CEntityIterator entityIterator(IteratePeds, pException, &vIteratorPos, fEntitySearchRange);
	CDynamicEntity* pEntity = (CDynamicEntity*)entityIterator.GetNext();
	s32 iNumPedsDoingTheSameThing = 0;
	while(pEntity)
	{
		const Vector3 diff(adjustedEffectPos - VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition()));

		const float xyMag2 = diff.XYMag2();
		if ( Abs(diff.z) < fMaxZDiff && xyMag2 < fRejectIfPedWithinDisSq )
		{
			return false;
		}
		
		const CPed* pPed = pEntity->GetIsTypePed() ? static_cast<CPed*>(pEntity) : NULL;
		if(pPed)
		{
			if (pPed->IsDead())
			{
				if ( Abs(diff.z) < fMaxZDiff && xyMag2 < rage::square(CPedPopulation::GetMinDistFromScenarioSpawnToDeadPed()) )
				{
					return false;
				}
			}

			// If this dummy or ped is doing the same scenario count up...
			// The range check is already done in the entity iterator.
			if(bCheckNumberInRange && xyMag2 < rage::square(fScenarioSearchRange))
			{
				s32 thisPedsScenario = (s32)pPed->GetScenarioType(*pEntity);

				if(thisPedsScenario == scenarioType ||
					(bIncludeAllCopsOnFoot && CPedType::IsCopType(pPed->GetPedType()) && pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle )) )

				{
					++iNumPedsDoingTheSameThing;
					if(iNumPedsDoingTheSameThing >= iMaxAllowedInRange)
					{
						return false;
					}
				}
			}
		}

		pEntity = (CDynamicEntity*)entityIterator.GetNext();
	}

	return true;
}

/*
PURPOSE
	Check if we may be allowed to spawn another vehicle of a given scenario type,
	or if too many vehicles have already spawned within a radius.
PARAMS
	scenarioType	- Scenario info index.
	spawnPosV		- The position we are considering spawning at.
RETURNS
	True unless there are already too many vehicles using this scenario around the point.
*/
bool CScenarioManager::CheckVehicleScenarioPopulationCount(int scenarioType, Vec3V_In spawnPosV)
{
	// Get the range info for the scenario type.
	float fRange = 0.0f;
	s32 iMaxAllowedInRange = 0;
	bool bCheckNumberInRange = CScenarioManager::GetRangeValuesForScenario(scenarioType, fRange, iMaxAllowedInRange);
	if(bCheckNumberInRange)
	{
		taskAssert(iMaxAllowedInRange > 0);	// Otherwise wouldn't expect bCheckNumberInRange to be true.

		// Note: this is currently based on the type of the scenarios the vehicles spawned at.
		// If they drive around and end up using other scenarios, we may want to do something else.
		// However, for the most part I think the current behavior is good, because it for example
		// still counts the vehicles while they drive through regular DRIVE chaining points.

		// Put the square of the radius in a vector register.
		const ScalarV thresholdDistSqV(square(fRange));

		// Loop over the spawned vehicles.
		const int numSpawnedVehicles = sm_SpawnedVehicles.GetCount();
		int numInRange = 0;
		for(int i = 0; i < numSpawnedVehicles; i++)
		{
			// Check if this entry has a valid vehicle and a relevant type
			// (we do try to clean up invalid vehicles from this array, but not necessarily
			// immediately).
			const CSpawnedVehicleInfo& spawnedInfo = sm_SpawnedVehicles[i];
			const CVehicle* pVeh = spawnedInfo.m_Vehicle;
			if(pVeh != NULL && spawnedInfo.m_ScenarioType == scenarioType)
			{
				// Compute the squared distance and check if it's close enough.
				const Vec3V vehPosV = pVeh->GetTransform().GetPosition();
				const ScalarV distSqV = DistSquared(vehPosV, spawnPosV);
				if(IsLessThanAll(distSqV, thresholdDistSqV))
				{
					// Count it, and return false if the allowed count has been exceeded.
					numInRange++;
					if(numInRange >= iMaxAllowedInRange)
					{
						return false;
					}
				}
			}
		}

		// TODO: In networking games, we may have to do something else, or make sure
		// that remote vehicles end up in sm_SpawnedVehicles.
	}

	return true;
}


int CScenarioManager::ChooseRealScenario(int virtualOrRealIndex, CPed* UNUSED_PARAM(pPed))
{
	const int numRealTypes = SCENARIOINFOMGR.GetNumRealScenarioTypes(virtualOrRealIndex);
	taskAssert(numRealTypes <= CScenarioInfoManager::kMaxRealScenarioTypesPerPoint);
	int realTypesToPickFrom[CScenarioInfoManager::kMaxRealScenarioTypesPerPoint];
	float probToPick[CScenarioInfoManager::kMaxRealScenarioTypesPerPoint];
	int numRealTypesToPickFrom = 0;
	float probSum = 0.0f;
	for(int j = 0; j < numRealTypes; j++)
	{
		const float prob = SCENARIOINFOMGR.GetRealScenarioTypeProbability(virtualOrRealIndex, j);
		realTypesToPickFrom[numRealTypesToPickFrom] = SCENARIOINFOMGR.GetRealScenarioType(virtualOrRealIndex, j);
		probToPick[numRealTypesToPickFrom] = prob;
		probSum += prob;
		numRealTypesToPickFrom++;
	}

	int chosenWithinTypeGroupIndex = -1;
	if(numRealTypesToPickFrom)
	{
		if(numRealTypesToPickFrom == 1)
		{
			chosenWithinTypeGroupIndex = 0;
		}
		else
		{
			float p = g_ReplayRand.GetFloat()*probSum;
			chosenWithinTypeGroupIndex = numRealTypesToPickFrom - 1;
			for(int j = 0; j < numRealTypesToPickFrom - 1; j++)
			{
				if(p < probToPick[j])
				{
					chosenWithinTypeGroupIndex = j;
					break;
				}
				p -= probToPick[j];
			}
		}
	}

	if(chosenWithinTypeGroupIndex >= 0)
	{
		return realTypesToPickFrom[chosenWithinTypeGroupIndex];
	}
	return -1;
}


bool CScenarioManager::IsVehicleScenarioPoint(const CScenarioPoint& scenarioPoint, int indexWithinTypeGroup)
{
	const int scenarioType = scenarioPoint.GetScenarioTypeReal(indexWithinTypeGroup);
	if(scenarioType < 0)
	{
		return false;
	}

	return IsVehicleScenarioType(scenarioType);
}


bool CScenarioManager::IsVehicleScenarioType(int virtualOrRealIndex)
{
	if(SCENARIOINFOMGR.IsVirtualIndex(virtualOrRealIndex))
	{
		// TODO: we should probably keep track of if each group is a group of vehicle scenarios or ped scenarios.
		return false;
	}
	else
	{
		const CScenarioInfo* pInfo = SCENARIOINFOMGR.GetScenarioInfoByIndex(virtualOrRealIndex);
		if(pInfo && IsVehicleScenarioType(*pInfo))
		{
			return true;
		}
	}
	return false;
}

// PURPOSE: Check if a given scenario type, which can be virtual or real, is a LookAt scenario.
bool CScenarioManager::IsLookAtScenarioType(int virtualOrRealIndex)
{
	if(SCENARIOINFOMGR.IsVirtualIndex(virtualOrRealIndex))
	{
		return false;
	}
	else
	{
		const CScenarioInfo* pInfo = SCENARIOINFOMGR.GetScenarioInfoByIndex(virtualOrRealIndex);
		if(pInfo && IsLookAtScenarioType(*pInfo))
		{
			return true;
		}
	}
	return false;
}

bool CScenarioManager::IsVehicleScenarioType(const CScenarioInfo& info)
{
	if(info.GetIsClass<CScenarioVehicleInfo>())	// || info.GetIsClass<CScenarioParkedVehicleInfo>())
	{
		return true;
	}
	return false;
}

bool CScenarioManager::IsLookAtScenarioType(const CScenarioInfo& info)
{
	if(info.GetIsClass<CScenarioLookAtInfo>())
	{
		return true;
	}
	return false;
}


bool CScenarioManager::ShouldHaveCarGen(const CScenarioPoint& point, const CScenarioVehicleInfo& info)
{
	// Only add a car generator if the NoSpawn flag is not set, and only if the probability for the
	// scenario type is not zero, unless the point itself has a probability override (which is guaranteed to be non-zero).
	// Also, don't spawn at vehicle attractors, even if they have a probability set on the point (which is useful
	// for attraction purposes).
	if((point.HasProbabilityOverride() || info.GetSpawnProbability() > SMALL_FLOAT)
			&& !point.IsFlagSet(CScenarioPointFlags::NoSpawn)
			&& !info.GetIsFlagSet(CScenarioInfoFlags::WillAttractVehicles))
	{
		return true;
	}
	return false;
}


bool CScenarioManager::ShouldHaveCarGenUnderCurrentConditions(const CScenarioPoint& point, const CScenarioVehicleInfo& info)
{
	// Check if the group is enabled, and that we are in the right game mode.
	if(!CheckScenarioPointEnabledByGroupAndAvailability(point))
	{
		return false;
	}

	// Use the override from the point, if it's set.
	if(point.HasTimeOverride())
	{
		if(!CClock::IsTimeInRange(point.GetTimeStartOverride(), point.GetTimeEndOverride()))
		{
			return false;
		}
	}
	else
	{
		// Check the time condition on the scenario type. Note: while we could potentially
		// always call Check() on this, to cover other conditions than just time (such as weather), we would
		// then have to be careful to make sure we refresh the car generators whenever those conditions
		// could potentially change, or do it periodically (more frequently than once per hour).
		const CScenarioConditionWorld* condition = info.GetCondition();
		CScenarioCondition::sScenarioConditionData conditionData;
		if(condition && condition->GetIsClass<CScenarioConditionTime>())
		{
			if(!condition->Check(conditionData))
			{
				return false;
			}
		}
	}

	if(!HasValidVehicleModelFromIndex(info, point.GetModelSetIndex()))
	{
		return false;
	}

	return true;
}


bool CScenarioManager::IsCurrentCarGenInvalidUnderCurrentConditions(const CScenarioPoint& point)
{
	CCarGenerator* pCarGen = point.GetCarGen();
	if(popVerifyf(pCarGen, "Expected car generator for scenario point."))
	{
		if(pCarGen->HasPreassignedModel())
		{
			u32 modelId = pCarGen->GetVisibleModel();
			if(CScriptCars::GetSuppressedCarModels().HasModelBeenSuppressed(modelId))
			{
				return true;
			}
		}
	}
	return false;
}


void CScenarioManager::OnAddScenario(CScenarioPoint& point)
{
	if(!sm_ReadyForOnAddCalls)
	{
		// This can happen for scenario points that get created before other systems that
		// we need have been initialized. In that case, they will get another OnAddScenario()
		// call later, when these other systems are ready.
		return;
	}

	if(point.GetOnAddCalled())
	{
		return;
	}

	const int scenarioType = point.GetScenarioTypeVirtualOrReal();
	// TODO: Maybe pick one randomly here, if we need to support groups of vehicle scenarios?
	if(scenarioType >= 0 && !SCENARIOINFOMGR.IsVirtualIndex(scenarioType))
	{
		const CScenarioInfo* pInfo = SCENARIOINFOMGR.GetScenarioInfoByIndex(scenarioType);
		if(pInfo)
		{
			// Check the class and the spawn probability - if it's 0, we wouldn't want to create a vehicle generator.
			if(pInfo->GetIsClass<CScenarioVehicleInfo>())
			{
				const CScenarioVehicleInfo* pSpawnVehInfo = static_cast<const CScenarioVehicleInfo*>(pInfo);

				if(ShouldHaveCarGen(point, *pSpawnVehInfo) && ShouldHaveCarGenUnderCurrentConditions(point, *pSpawnVehInfo))
				{
					u32 modelHash = GetRandomVehicleModelHashOverrideFromIndex(*pSpawnVehInfo, point.GetModelSetIndex());

					Vec3V posV = point.GetPosition();

					Vec3V dirV = point.GetDirection();
					const float dirXNonNorm = dirV.GetXf();
					const float dirYNonNorm = dirV.GetYf();
					const float dirMag = sqrtf(dirXNonNorm*dirXNonNorm + dirYNonNorm*dirYNonNorm);

					if(dirMag > SMALL_FLOAT)
					{
						const float dirX = dirXNonNorm/dirMag;
						const float dirY = dirYNonNorm/dirMag;

						const float centreX = posV.GetXf();
						const float centreY = posV.GetYf();
						const float centreZ = posV.GetZf();

						// MAGIC! We could make these parameters of CScenarioVehicleInfo, if needed.
						const float maxLen = 5.0f;
						const float maxWidth = 3.0f;

						const float vec1X = dirX*maxLen;
						const float vec1Y = dirY*maxLen;

						u32 ipl = 0;

						bool bCreatedByScript = false;	// Not sure
						u32 creationRule = CREATION_RULE_ALL;
						if(pSpawnVehInfo->GetIsFlagSet(CScenarioInfoFlags::VehicleCreationRuleBrokenDown))
						{
							creationRule = CREATION_RULE_CAN_BE_BROKEN_DOWN;
						}

						Assert(point.m_CarGenId == CScenarioPoint::kCarGenIdInvalid);

						u32 flags = 0;
						flags |= point.IsHighPriority() ? CARGEN_FORCESPAWN : 0;
						flags |= CARGEN_DURING_DAY;
						flags |= CARGEN_AT_NIGHT;
						flags |= CARGEN_SINGLE_PLAYER;
						flags |= CARGEN_NETWORK_PLAYER;
						s32 carGenId = CTheCarGenerators::CreateCarGenerator(centreX, centreY, centreZ, vec1X, vec1Y, maxWidth, modelHash,
								-1, -1, -1, -1, -1, -1, flags, ipl,
								bCreatedByScript, creationRule, scenarioType, atHashString(), -1, -1, false, true);

						if(carGenId >= 0)
						{
							if(Verifyf(carGenId < sm_ScenarioPointForCarGen.GetCount(), "Invalid cargen ID %d / %d.", carGenId, sm_ScenarioPointForCarGen.GetCount()))
							{
								Assert(carGenId != CScenarioPoint::kCarGenIdInvalid);
								Assert(carGenId <= 0xffff);
								point.m_CarGenId = (u16)carGenId;

								// Set the pointer in the reverse lookup table.
								Assertf(!sm_ScenarioPointForCarGen[carGenId], "Car generator %d may have been improperly destroyed outside of the scenario system.", (int)carGenId);
								sm_ScenarioPointForCarGen[carGenId] = &point;
							}
						}
						else
						{
#if !__FINAL
							static bool s_FirstTime = true;
							if(s_FirstTime)
							{
								CTheCarGenerators::DumpCargenInfo();
								s_FirstTime = false;
							}
#endif	// !__FINAL

							Assertf(0, "Failed to create car generator, probably out of space in the pool (size %d). See output spew for more info", 
									sm_ScenarioPointForCarGen.GetCount());
						}
					}
				}

				if (point.IsInCluster())
				{
					//disable the cargenerator as the cluster will turn it on when it is valid to spawn stuff.
					CCarGenerator* cargen = point.GetCarGen();
					if (cargen)
					{
						cargen->SwitchOff();
					}
				}

				if(pInfo->GetIsFlagSet(CScenarioInfoFlags::WillAttractVehicles))
				{
					GetVehicleScenarioManager().AddAttractor(point);
				}
			}
		}
	}

	point.SetRunTimeFlag(CScenarioPointRuntimeFlags::OnAddCalled, true);
}


void CScenarioManager::OnRemoveScenario(CScenarioPoint& point)
{
	Assert(point.GetOnAddCalled());
	point.SetRunTimeFlag(CScenarioPointRuntimeFlags::OnAddCalled, false);

	const int scenarioType = point.GetScenarioTypeVirtualOrReal();
	if(scenarioType >= 0 && !SCENARIOINFOMGR.IsVirtualIndex(scenarioType))	// Note: see OnAddScenario() with regards to using vehicle scenarios as virtual types.
	{
		const CScenarioInfo* pInfo = SCENARIOINFOMGR.GetScenarioInfoByIndex(scenarioType);
		if(pInfo)
		{
			if(pInfo->GetIsClass<CScenarioVehicleInfo>())
			{
				if(pInfo->GetIsFlagSet(CScenarioInfoFlags::WillAttractVehicles))
				{
					GetVehicleScenarioManager().RemoveAttractor(point);
				}

				const int carGenId = point.m_CarGenId;
				if(carGenId != CScenarioPoint::kCarGenIdInvalid)
				{
					if(taskVerifyf(carGenId < CTheCarGenerators::GetSize(),
							"Got car gen index %d, but expected a value in the 0..%d range. Possible memory corruption? Activating workaround to avoid crash.",
							carGenId, CTheCarGenerators::GetSize() - 1))
					{
						CCarGenerator* pCarGen = CTheCarGenerators::GetCarGenerator(carGenId);
						if(taskVerifyf(pCarGen, "Car generator missing, somehow."))
						{
							if(taskVerifyf(pCarGen->IsUsed(), "Car generator already destroyed, somehow."))
							{
								CTheCarGenerators::DestroyCarGeneratorByIndex(carGenId, true);
							}
						}

						Assert(sm_ScenarioPointForCarGen[carGenId] == &point);
						sm_ScenarioPointForCarGen[carGenId] = NULL;
					}

					point.m_CarGenId = CScenarioPoint::kCarGenIdInvalid;
				}
			}
		}
	}
}


CScenarioManager::CarGenConditionStatus CScenarioManager::CheckCarGenScenarioConditionsA(const CCarGenerator& cargen)
{
	// Check if we will have space to keep track of this vehicle, otherwise we
	// can't spawn it. Shouldn't be much of an issue since space in this array is
	// cheap.
	if(sm_SpawnedVehicles.IsFull())
	{
		BANK_ONLY(cargen.RegisterSpawnFailure(CPedPopulation::PedPopulationFailureData::FR_poolOutOfSpace));
		return kCarGenCondStatus_Failed;
	}

	const CScenarioPoint* pt = GetScenarioPointForCarGen(cargen);
	if(!taskVerifyf(pt, "Failed to find scenario point for car generator."))
	{
		return kCarGenCondStatus_Failed;
	}

	// If the NoSpawn flag is set we probably shouldn't even have created the car generator
	// in the first place, but if we did anyway, make sure we don't spawn.
	if(pt->IsFlagSet(CScenarioPointFlags::NoSpawn))
	{
		return kCarGenCondStatus_Failed;
	}

	// Check to see if the chain is in use.
	// Note: clusters are treated as an exception here, since they should check internally
	// and create a dummy user before deciding to spawn, so by the time we get here, the
	// chain is expected to often be full, due to the reservation for our own vehicle.
	if (pt->IsChained() && !pt->IsInCluster() && pt->IsUsedChainFull() && !AllowExtraChainedUser(*pt))
	{
		return kCarGenCondStatus_Failed;
	}

	taskAssertf(!SCENARIOINFOMGR.IsVirtualIndex(pt->GetScenarioTypeVirtualOrReal()),
			"Virtual scenarios are not supported for vehicles yet.");
	int indexWithinTypeGroup = 0;

	if(!CheckScenarioPointEnabled(*pt, indexWithinTypeGroup))
	{
		BANK_ONLY(cargen.RegisterSpawnFailure(CPedPopulation::PedPopulationFailureData::FR_scenarioPointDisabled));
		return kCarGenCondStatus_Failed;
	}

	if(!CheckScenarioPointMapData(*pt))
	{
		BANK_ONLY(cargen.RegisterSpawnFailure(CPedPopulation::PedPopulationFailureData::FR_scenarioPointRequiresImap));
		return kCarGenCondStatus_Failed;
	}

	// Check time conditions. Note: this does not include the spawn time interval,
	// that's checked later.
	if(!CheckScenarioPointTimesAndProbability(*pt, false, false, indexWithinTypeGroup))
	{
		BANK_ONLY(cargen.RegisterSpawnFailure(CPedPopulation::PedPopulationFailureData::FR_scenarioTimeAndProbabilityFailCarGen));
		return kCarGenCondStatus_Failed;
	}

	// Check that the point we want to spawn at is not in the spawn history.
	if (pt->HasHistory())
	{
		BANK_ONLY(cargen.RegisterSpawnFailure(CPedPopulation::PedPopulationFailureData::FR_scenarioPointInUse));
		return pt->IsHighPriority() ? kCarGenCondStatus_NotReady : kCarGenCondStatus_Failed;
	}

	const Vec3V posV = cargen.GetCentre();

	if(CScenarioBlockingAreas::IsPointInsideBlockingAreas(VEC3V_TO_VECTOR3(posV), false, true))
	{		
		BANK_ONLY(cargen.RegisterSpawnFailure(CPedPopulation::PedPopulationFailureData::FR_spawnPointInBlockedArea));
		return pt->IsHighPriority() ? kCarGenCondStatus_NotReady : kCarGenCondStatus_Failed;
	}

	const int scenarioType = cargen.GetScenarioType();

	// Check if there are too many vehicles using the same scenario already close to this point.
	if(!CheckVehicleScenarioPopulationCount(scenarioType, posV))
	{
		BANK_ONLY(cargen.RegisterSpawnFailure(CPedPopulation::PedPopulationFailureData::FR_collisionChecks));
		return kCarGenCondStatus_Failed;
	}

	const CScenarioInfo* pScenarioInfo = CScenarioManager::GetScenarioInfo(scenarioType);

	if(!taskVerifyf(pScenarioInfo, "Failed to find scenario info for type %d.", scenarioType))
	{
		return kCarGenCondStatus_Failed;
	}
	if(!taskVerifyf(pScenarioInfo->GetIsClass<CScenarioVehicleInfo>(), "Not a CScenarioVehicleInfo scenario, for some reason (type %d).", scenarioType))
	{
		return kCarGenCondStatus_Failed;
	}

	if(!CheckScenarioProbability(pt, scenarioType))
	{
		BANK_ONLY(cargen.RegisterSpawnFailure(CPedPopulation::PedPopulationFailureData::FR_scenarioTimeAndProbabilityFailCarGen));
		return kCarGenCondStatus_Failed;
	}

	// We need the keyhole shape for the code below, both in the AerialVehiclePoint
	// case and the other one.
	if(!CVehiclePopulation::HasPopGenShapeBeenInitialized())
	{
		return kCarGenCondStatus_NotReady;
	}

	return kCarGenCondStatus_Ready;
}

CScenarioManager::CarGenConditionStatus CScenarioManager::CheckCarGenScenarioConditionsB(const CCarGenerator& cargen, fwModelId carModelToUse, fwModelId trailerModelToUse)
{
	const CScenarioPoint* pt = GetScenarioPointForCarGen(cargen);
	const Vec3V posV = cargen.GetCentre();
	const int scenarioType = cargen.GetScenarioType();
	const CScenarioInfo* pScenarioInfo = CScenarioManager::GetScenarioInfo(scenarioType);

	// If we are freezing exterior vehicles, don't allow aircraft to spawn.
	// Note: possibly we might not want to spawn any vehicles in that case,
	// kind of seems like a waste, but it was specifically requested for aircraft
	// (by audio people).
	if(CGameWorld::GetFreezeExteriorVehicles())
	{
		const CBaseModelInfo* pBaseInfo = CModelInfo::GetBaseModelInfo(carModelToUse);
		if(pBaseInfo && popVerifyf(pBaseInfo->GetModelType() == MI_TYPE_VEHICLE, "Expected vehicle model."))
		{
			const CVehicleModelInfo* pModelInfo = static_cast<const CVehicleModelInfo*>(pBaseInfo);

			if(pModelInfo->GetIsRotaryAircraft() || pModelInfo->GetIsPlane())
			{
				return kCarGenCondStatus_Failed;
			}
		}
	}

	if(pt->IsFlagSet(CScenarioPointFlags::AerialVehiclePoint))
	{
		// When spawning in the air, we do visibility checks a little differently than when
		// spawning on ground, to make sure the aircraft spawn in places where we can see them,
		// and don't pop in too close on screen.

		// MAGIC!
		const float radius = 20.0f;
		const float rejectAboveCosAngle = 0.766f;	// = cosf(40.0f*DtoR)
		const float fromAheadMinDistToPlaneAlongMoveDir = 50.0f;

		// Get a vector from the player (or other population center), to the point.
		const CPopGenShape& popShape = CVehiclePopulation::GetPopGenShape();
		const Vec3V playerToPointV = Subtract(posV, VECTOR3_TO_VEC3V(popShape.GetCenter()));

		// Reject anything within a cone above the population center. Spawning there is generally
		// undesirable, the player is unlikely to see it since we don't usually pan up the camera that
		// far.
		const ScalarV thresholdV = Scale(ScalarV(rejectAboveCosAngle), MagFast(playerToPointV));
		if(IsGreaterThanAll(playerToPointV.GetZ(), thresholdV))
		{
			return kCarGenCondStatus_NotReady;
		}

		// Get the forward direction of the population shape, and the forward direction of the spawn point.
		const Vec2V popGenShapeDirV(popShape.GetDir().x, popShape.GetDir().y);
		const Vec2V spawnFwdDirXYV = pt->GetWorldDirectionFwd().GetXY();
		Assert(fabsf(MagSquared(popGenShapeDirV).Getf() - 1.0f) <= 0.001f);		// Assumed to be normalized.

		// Compute the dot product between these directions.
		const ScalarV dirDotV = Dot(popGenShapeDirV, spawnFwdDirXYV);
		if(IsLessThanAll(dirDotV, ScalarV(V_ZERO)))
		{
			// The spawn point is facing towards the player. Compute the dot product with the
			// forward direction to get the distance to the plane through the population center.

			const Vec2V playerToPointXYV = playerToPointV.GetXY();
			const ScalarV distToPlaneThroughPopGenCenterV = Dot(playerToPointXYV, popGenShapeDirV);

			// Divide the distance by the dot product we computed earlier. This gives us the distance
			// along the direction of movement, until we hit the plane through the population center.
			// Reject any point that's too close to this plane, since it's probably a plane that will fly
			// by without us noticing.
			const ScalarV distToPlaneAlongMoveDirV = InvScale(distToPlaneThroughPopGenCenterV, Negate(dirDotV));
			if(IsLessThanAll(distToPlaneAlongMoveDirV, ScalarV(fromAheadMinDistToPlaneAlongMoveDir)))
			{
				return kCarGenCondStatus_NotReady;
			}

		}

		// Compute a minimum on screen spawn distance, similar to what is done by CheckPosAgainstVehiclePopulationKeyhole().
		const bool shouldReduceInner = pt->IsInCluster();
		float minRangeOnScreen = popShape.GetOuterBandRadiusMin();
		if(shouldReduceInner)
		{
			minRangeOnScreen = Min(minRangeOnScreen, sm_MaxOnScreenMinSpawnDistForCluster);
		}
		const float distScaleFactor = GetVehicleScenarioDistScaleFactorForModel(carModelToUse, true);
		minRangeOnScreen *= distScaleFactor;

		// Check if it's on screen at a closer distance than what's allowed.
		if(CPedPopulation::IsCandidateInViewFrustum(RCC_VECTOR3(posV), radius, minRangeOnScreen))
		{
			return kCarGenCondStatus_NotReady;
		}
		if(NetworkInterface::IsGameInProgress())
		{
			const unsigned int numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
			const netPlayer * const *remotePhysicalPlayers = netInterface::GetRemotePhysicalPlayers();
			for(unsigned int index = 0; index < numRemotePhysicalPlayers; index++)
			{
				const CNetGamePlayer *pPlayer = SafeCast(const CNetGamePlayer, remotePhysicalPlayers[index]);
				if(pPlayer->GetPlayerPed())
				{
					if(NetworkInterface::IsVisibleToPlayer(pPlayer, RCC_VECTOR3(posV), radius, minRangeOnScreen))
					{
						return kCarGenCondStatus_NotReady;
					}
				}
			}
		}
	}
	else
	{
		//Check the position based on the keyhole conditions used when spawning out 
		// if the point is not an AerialVehiclePoint
		const float distScaleFactor = GetVehicleScenarioDistScaleFactorForModel(carModelToUse, false);

		// Skip the upper distance limits if we are in a cluster.
		const bool noMaxLimit = pt->IsInCluster();
		if (!CheckPosAgainstVehiclePopulationKeyhole(posV, ScalarVFromF32(distScaleFactor), pt->HasExtendedRange(), pt->IsInCluster(), noMaxLimit))
		{
			return kCarGenCondStatus_NotReady;
		}
	}

	// Check the spawn time condition. Note that this is considered a temporary condition,
	// so the car generator doesn't get disabled if it fails and we'll try again shortly.
	// For this reason, we'll probably want to do it after the more permanent condition failures
	// above, but probably before we check the ped models (which may cause them to stream in).
	if(!CheckScenarioSpawnInterval(scenarioType))
	{
		BANK_ONLY(cargen.RegisterSpawnFailure(CPedPopulation::PedPopulationFailureData::FR_scenarioTimeAndProbabilityFailCarGen));
		return kCarGenCondStatus_NotReady;
	}

	const CScenarioVehicleInfo& info = *static_cast<const CScenarioVehicleInfo*>(pScenarioInfo);

	CScenarioManager::CarGenConditionStatus ret = kCarGenCondStatus_Ready;

	// Stream in the trailer, if any.
	if(trailerModelToUse.IsValid())
	{
		if(!CModelInfo::HaveAssetsLoaded(trailerModelToUse))
		{
			// Not streamed in yet, request it if needed.
			if(!CModelInfo::AreAssetsRequested(trailerModelToUse))
			{
				CTheCarGenerators::RequestVehicleModelForScenario(trailerModelToUse, pt->IsHighPriority());
			}
			ret = kCarGenCondStatus_NotReady;
			BANK_ONLY(cargen.RegisterSpawnFailure(CPedPopulation::PedPopulationFailureData::FR_scenarioVehModel));
		}
	}

	// If the probabilities for drivers or passengers are 1, we would probably never want to spawn
	// the vehicle if we don't have people streamed in. On the other hand, if the probability is lower,
	// it's probably better to let the vehicle spawn empty.
	bool gotOccupantModels = true;
	if(info.AlwaysNeedsDriver())
	{
		strLocalIndex pedModel = strLocalIndex(fwModelId::MI_INVALID);
		if(!GetPedTypeForScenario(scenarioType, *pt, pedModel, VEC3V_TO_VECTOR3(posV), 0, NULL, 0, carModelToUse))
		{
			gotOccupantModels = false;
		}
	}
	if(info.AlwaysNeedsPassengers())
	{
		strLocalIndex pedModel = strLocalIndex(fwModelId::MI_INVALID);
		if(!GetPedTypeForScenario(scenarioType, *pt, pedModel, VEC3V_TO_VECTOR3(posV), 0))
		{
			gotOccupantModels = false;
		}
	}
	if(!gotOccupantModels)
	{
		BANK_ONLY(cargen.RegisterSpawnFailure(CPedPopulation::PedPopulationFailureData::FR_noPedModelFound));

		// Note: we may want to return kCarGenCondStatus_Failed here instead, in the case
		// of forced models not being used.
		ret = kCarGenCondStatus_NotReady;
	}

	u32 layoutHash = info.GetScenarioLayoutForPedsHash();
	if(layoutHash)
	{
		const CVehicleScenarioLayoutInfo* pLayout = CVehicleMetadataMgr::GetInstance().GetVehicleScenarioLayoutInfo(layoutHash);
		if(taskVerifyf(pLayout, "Failed to find vehicle scenario layout %s.", info.GetScenarioLayoutForPeds()))
		{
			CScenarioManager::CarGenConditionStatus ret2 = CheckPedsNearVehicleUsingLayoutConditions(cargen, *pLayout, carModelToUse);
			if(ret2 == kCarGenCondStatus_Failed || ret == kCarGenCondStatus_Failed)
			{
				ret = kCarGenCondStatus_Failed;
			}
			else if(ret2 == kCarGenCondStatus_NotReady)
			{
				ret = kCarGenCondStatus_NotReady;
			}
#if __BANK
			if(ret2 != kCarGenCondStatus_Ready)
			{
				cargen.RegisterSpawnFailure(CPedPopulation::PedPopulationFailureData::FR_noPedModelFound);
			}
#endif	// __BANK
		}
	}

	return ret;
}

float CScenarioManager::GetCarGenSpawnRangeMultiplyer(const CCarGenerator& cargen)
{
	const CScenarioPoint* pt = GetScenarioPointForCarGen(cargen);
	if(taskVerifyf(pt, "Failed to find scenario point for car generator."))
	{
		if (pt->IsFlagSet(CScenarioPointFlags::NoVehicleSpawnMaxDistance))
		{
			return 1000.0f; //Make this huge ... 
		}

		if (pt->HasExtendedRange())
		{
			float mul = CVehiclePopulation::GetCreationDistMultExtendedRange();

			// Possibly use a higher scale factor, if we are flying and this is an extended range scenario.
			if(ShouldExtendExtendedRangeFurther())
			{
				mul = Max(mul, sm_ExtendedExtendendRangeMultiplier);
			}
			return mul;
		}
	}

	return 1.0f;
}

bool CScenarioManager::CheckPosAgainstVehiclePopulationKeyhole(Vec3V_In spawnPosV, ScalarV_In distScaleFactorV, bool extendedRange, bool relaxForCluster, bool noMaxLimit)
{
	Assert(CVehiclePopulation::HasPopGenShapeBeenInitialized());

	const CPopGenShape* pPopShape = &CVehiclePopulation::GetPopGenShape();

	// In some cases, we need to copy the shape data.
	CPopGenShape cpy;

	// Is this an extended range point, and if so, should we extend it even further because we are flying (or similar)?
	bool shouldExtendOuter = noMaxLimit || (extendedRange && ShouldExtendExtendedRangeFurther());

	// Should we reduce the on-screen min distance? We should if we are trying to spawn a cluster point, probably,
	// since the distance can otherwise be pretty large, preventing the whole cluster from spawning.
	bool shouldReduceInner = relaxForCluster;

	if(shouldExtendOuter || shouldReduceInner)
	{
		// Make a copy of the shape.
		cpy = *pPopShape;

		if(shouldReduceInner)
		{
			float minRangeOnScreen = Min(cpy.GetOuterBandRadiusMin(), sm_MaxOnScreenMinSpawnDistForCluster);
			cpy.SetOuterBandRadiusMin(minRangeOnScreen);
		}

		if(noMaxLimit)
		{
			cpy.SetInnerBandRadiusMax(LARGE_FLOAT);
			cpy.SetOuterBandRadiusMax(LARGE_FLOAT);
		}
		else if(shouldExtendOuter)
		{
			float maxRangeOnScreen = Max(cpy.GetOuterBandRadiusMax(), GetExtendedExtendedRangeForVehicleSpawn());
			cpy.SetOuterBandRadiusMax(maxRangeOnScreen);
		}

		// Point to the copy.
		pPopShape = &cpy;
	}

	// To apply the scaling factor, we could probably make a copy of the population shape,
	// and scale the distances. Or, we could pass it in to CategorisePoint(). But, it's
	// probably simpler and less obtrusive to just inversely scale the spawn position instead,
	// around the center of the population shape:
	const Vec3V centerV = VECTOR3_TO_VEC3V(pPopShape->GetCenter());
	const Vec3V deltaV = Subtract(spawnPosV, centerV);
	const Vec3V scaledDeltaV = InvScale(deltaV, distScaleFactorV);
	const Vec3V scaledSpawnPosV = Add(centerV, scaledDeltaV);

	//2d position for keyhole shape.
	Vector2 xyPos;
	xyPos.x = scaledSpawnPosV.GetXf();
	xyPos.y = scaledSpawnPosV.GetYf();

	// Do the categorization.
	CPopGenShape::GenCategory genCategory =  pPopShape->CategorisePoint(xyPos);

	//If the category is On then that means the position passed the keyhole shape test.
	bool valid = (	
					genCategory == CPopGenShape::GC_KeyHoleInnerBand_on ||
					genCategory == CPopGenShape::GC_KeyHoleOuterBand_on ||
					genCategory == CPopGenShape::GC_KeyHoleSideWall_on
				 );

	if(noMaxLimit && !valid)
	{
		valid = (genCategory == CPopGenShape::GC_InFOV_usableIfOccluded);
	}

	return valid;
}

bool CScenarioManager::PopulateCarGenVehicle(CVehicle& veh, const CCarGenerator& cargen, bool calledByCluster, const CAmbientVehicleModelVariations** ppVehVarOut)
{
	if(ppVehVarOut)
	{
		*ppVehVarOut = NULL;
	}

	CScenarioPoint* pt = GetScenarioPointForCarGen(cargen);
	if(!taskVerifyf(pt, "Failed to find scenario point for car generator."))
	{
		return true;
	}

	// If this is a cluster point, don't do anything except for if this call came from
	// the cluster code. We don't do the rest untill all vehicles in the cluster are ready.
	if(pt->IsInCluster() && !calledByCluster)
	{
		return true;
	}

#if __ASSERT
	for(int i = 0; i < sm_SpawnedVehicles.GetCount(); i++)
	{
		taskAssertf(sm_SpawnedVehicles[i].m_Vehicle != &veh, "Scenario vehicle unexpectedly already found in array.");
	}
#endif
	// We already checked this in CheckCarGenScenarioConditions(), but it might be possible
	// that the situation has changed since then (may take a couple of frames to do the
	// actual car placement after the conditions were evaluated).
	if(sm_SpawnedVehicles.IsFull())
	{
		return false;
	}

	const int scenarioType = cargen.GetScenarioType();

	taskAssertf(!SCENARIOINFOMGR.IsVirtualIndex(pt->GetScenarioTypeVirtualOrReal()),
			"Virtual scenarios are not supported for vehicles yet.");
	int indexWithinTypeGroup = 0;

	// Check this again, for the LastSpawnTime checks, in case another scenario of this type
	// spawned while we were considering spawning this vehicle.
	if(!CheckScenarioPointTimesAndProbability(*pt, false, true, indexWithinTypeGroup))
	{
		return false;
	}

	if(!CheckScenarioPointMapData(*pt))
	{
		return false;
	}

	// Not sure if the position we pass in to GetPedTypeForScenario() needs to be more accurate than this.
	const Vec3V vehPosV = veh.GetTransform().GetPosition();

	// It might be possible for a scenario car generator to schedule a vehicle, and if after
	// that a scenario blocking area got added, a vehicle could probably spawn within the blocking
	// area. Even disabling the car generators and clearing vehicles may not fully protect against
	// this, so we do an extra check here.
	// See B* 736229: "Cargen interfering with Robbie Nails scene."
	if(CScenarioBlockingAreas::IsPointInsideBlockingAreas(RCC_VECTOR3(vehPosV), false, true))
	{
		return false;
	}

	// Again, already checked by CheckCarGenScenarioConditions(), but we might have spawned another
	// vehicle nearby at around the same time, so let's check again.
	if(!CheckVehicleScenarioPopulationCount(scenarioType, vehPosV))
	{
		return false;
	}

	// Keep track of the vehicle in the array.
	// Note: we could probably move this to ReportCarGenVehicleSpawnFinished() if we wanted,
	// but even if we decide to not spawn the vehicle because some condition further down
	// is failing, the entry in the array should just be cleaned up on the next update because
	// the vehicle isn't valid, so it doesn't matter much.
	CSpawnedVehicleInfo& spawnedInfo = sm_SpawnedVehicles.Append();
	spawnedInfo.m_Vehicle = &veh;
	taskAssert(scenarioType >= 0 && scenarioType <= 0xffff);
	spawnedInfo.m_ScenarioType = (u16)scenarioType;

	const CScenarioInfo* pScenarioInfo = CScenarioManager::GetScenarioInfo(scenarioType);
	if(!taskVerifyf(pScenarioInfo, "Failed to find scenario info for type %d.", scenarioType))
	{
		return true;
	}
	if(!taskVerifyf(pScenarioInfo->GetIsClass<CScenarioVehicleInfo>(), "Not a CScenarioVehicleInfo scenario, for some reason (type %d).", scenarioType))
	{
		return true;
	}

	const CScenarioVehicleInfo& info = *static_cast<const CScenarioVehicleInfo*>(pScenarioInfo);

	// If VehicleCreationRuleBrokenDown is set, CCarGenerator should normally have filtered
	// out unusable vehicle types already. There might be semi-valid reasons for why that didn't happen, though,
	// in particular, it seems like we might not know the approximate bonnet height until the vehicle
	// model is streamed in, and for some vehicles, we may find ourselves without the required bonnet.
	if(info.GetIsFlagSet(CScenarioInfoFlags::VehicleCreationRuleBrokenDown))
	{
		if(!CTaskParkedVehicleScenario::CanVehicleBeUsedForBrokenDownScenario(veh))
		{
			return false;
		}
	}

	// Make sure we don't spawn anything that's supposed to be suppressed. Normally this
	// should have been filtered out at a higher level.
	if(CScriptCars::GetSuppressedCarModels().HasModelBeenSuppressed(veh.GetModelId().GetModelIndex()))
	{
		return false;
	}

	const CAmbientVehicleModelVariations* vehVar = NULL;
	// Find the CAmbientVehicleModelVariations associated with this model.
	const CAmbientModelSet* pModelSet = CScenarioManager::GetVehicleModelSetFromIndex(info, pt->GetModelSetIndex());
	const CVehicleModelInfo* pModelInfo = veh.GetVehicleModelInfo();
	if(pModelSet && pModelInfo)
	{
		u32 modelHash = pModelInfo->GetModelNameHash();
		if(!FindVehicleModelInModelSet(modelHash, *pModelSet, vehVar))
		{
			taskAssertf(0, "Got incorrect model '%s' for vehicle scenario, not in model set '%s'.",
					pModelInfo->GetModelName(), pModelSet->GetName());
			return false;
		}
	}

	// If there is an attached trailer, look inside the trailer model set to find the right variations.
	const CAmbientVehicleModelVariations* trailerVar = NULL;
	CTrailer* pTrailer = veh.GetAttachedTrailer();
	if(pTrailer)
	{
		const CVehicleModelInfo* pTrailerModelInfo = pTrailer->GetVehicleModelInfo();
		const CAmbientModelSet* pTrailerModelSet = info.GetTrailerModelSet();
		if(pTrailerModelSet && pTrailerModelInfo)
		{
			u32 modelHash = pTrailerModelInfo->GetModelNameHash();
			if(!FindVehicleModelInModelSet(modelHash, *pTrailerModelSet, trailerVar))
			{
				taskAssertf(0, "Got incorrect trailer model '%s' for vehicle scenario, not in model set '%s'.",
						pTrailerModelInfo->GetModelName(), pTrailerModelSet->GetName());
				return false;
			}
		}
	}

	if(g_ReplayRand.GetFloat() < info.GetProbabilityForDriver())
	{
		strLocalIndex desiredDriverModel = strLocalIndex(fwModelId::MI_INVALID);
		const CAmbientModelVariations* variations = NULL;
		if(GetPedTypeForScenario(scenarioType, *pt, desiredDriverModel, VEC3V_TO_VECTOR3(vehPosV), 0, &variations, 0, veh.GetModelId()))
		{
			veh.SetUpDriver(false, true /* canLeaveVehicle - not sure */, desiredDriverModel.Get(), variations);
		}
	}

	CPed* pDriver = veh.GetDriver();
	if(info.AlwaysNeedsDriver() && !pDriver)
	{
		// Didn't get the driver that we need, fail.
		return false;
	}

	int numPassengersSpawned = 0;

	const int vehMaxPassengers = veh.GetMaxPassengers();
	const int minPassengers = Min((int)info.GetMinNumPassengers(), vehMaxPassengers);
	const int maxPassengers = Min((int)info.GetMaxNumPassengers(), vehMaxPassengers);
	taskAssertf(minPassengers <= maxPassengers, "Passenger min/max error: %d > %d.", minPassengers, maxPassengers);

	const CSeatManager& seatMgr = *veh.GetSeatManager();
	const int maxSeats = seatMgr.GetMaxSeats();
	if(g_ReplayRand.GetFloat() < info.GetProbabilityForPassengers())
	{

		// Note: CVehiclePopulation::AddDriverAndPassengersForVeh() uses a different random distribution,
		// may want to consider that.
		int numPassengers = g_ReplayRand.GetRanged(minPassengers, maxPassengers);

		// Respect the total number of passenger peds permitted for performance
		int numPassengersPermitted = CVehicleAILodManager::ComputeRealDriversAndPassengersRemaining();
		if( numPassengers > numPassengersPermitted )
		{
			numPassengers = Max(minPassengers, numPassengersPermitted);
		}

		const CVehicleLayoutInfo* pLayoutInfo = veh.GetLayoutInfo();

		for(int i = 0; numPassengersSpawned < numPassengers; i++)
		{
			const int seatIndex = i + 1;
			if(seatIndex >= maxSeats)
			{
				break;
			}

			if(info.GetIsFlagSet(CScenarioInfoFlags::OnlyUseIdleVehicleSeats))
			{
				if(Verifyf(pLayoutInfo, "Expected layout info for vehicle."))
				{
					const CVehicleSeatInfo* pSeatInfo = pLayoutInfo->GetSeatInfo(seatIndex);
					if(!pSeatInfo || !pSeatInfo->GetIsIdleSeat())
					{
						continue;
					}
				}
			}

			// TODO: Call CanRegisterObjects() earlier (when checking conditions) to see if
			// we have room for the driver and min number of passengers, if required.

			// only add passengers if we have room
			if(!NetworkInterface::IsGameInProgress() || NetworkInterface::CanRegisterObject(NET_OBJ_TYPE_PED, false))
			{
				strLocalIndex desiredPassengerModel = strLocalIndex(fwModelId::MI_INVALID);
				const CAmbientModelVariations* variations = NULL;
				
				// Try to make the passenger modelset match the driver's, if possible.
				if(GetPedTypeForScenario(scenarioType, *pt, desiredPassengerModel, VEC3V_TO_VECTOR3(vehPosV), 0, &variations, 0, veh.GetModelId()))
				{
					if (CPedPopulation::SchedulePedInVehicle(&veh, seatIndex, true, desiredPassengerModel.Get(), variations))
					{
						numPassengersSpawned++;
					}
				}
			}
		}
	}

	if(info.AlwaysNeedsPassengers() && numPassengersSpawned < minPassengers)
	{
		return false;
	}

	CPed* driver = seatMgr.GetDriver();
	for(int i = 0; i < maxSeats; i++)
	{
		CPed* pOccupant = seatMgr.GetPedInSeat(i);
		if(pOccupant)
		{
			//NOTE: passengers of a vehicle dont need to know about the scenario point
			// that they are about to be part of, because they get their conditional anims
			// from the driver (CTaskUnalerted::CreateTaskForPassenger). So to remove some 
			// one frame users being counted for the scenario point we pass in null for the 
			// point. The current max for CScenarioPoint::m_iUsers is 8 and for vehicles with
			// many passengers this results in a large user count for 1 or 2 frames while their 
			// task is switched in CTaskUnalerted::InVehicleAsPassenger_OnEnter.
			CTask* pControlTask = NULL;
			if (driver == pOccupant)
			{
				pControlTask = SetupScenarioControlTask(*pOccupant, *pScenarioInfo, NULL, pt, scenarioType);
			}
			else
			{
				pControlTask = SetupScenarioControlTask(*pOccupant, *pScenarioInfo, NULL, NULL, scenarioType);
			}
			pOccupant->GetPedIntelligence()->AddTaskDefault(pControlTask);
		}
	}

	// For now, we probably shouldn't use pretend occupants for these vehicles, otherwise
	// I'm a bit afraid that the people we just set up could be replaced or retasked.
	veh.m_nVehicleFlags.bDisablePretendOccupants = true;

	// For clustered points, we will do this through the cluster code, so we can keep
	// track of the peds that spawned.
	if(!pt->IsInCluster())
	{
		// Note: maybe we should be checking the return value here, and destroy the vehicle if
		// we failed? Not sure why we don't, but not in a good position to change that now.
		SpawnPedsNearVehicleUsingLayout(veh, scenarioType);
	}

	if(ppVehVarOut)
	{
		*ppVehVarOut = vehVar;
	}

	// Apply the variations to the vehicles.
	if(vehVar)
	{
		ApplyVehicleVariations(veh, *vehVar);
	}
	if(trailerVar)
	{
		Assert(pTrailer);	// Should be impossible for trailerVar to have been set otherwise.
		ApplyVehicleVariations(*pTrailer, *trailerVar);
	}


	//Populated vehicles should be unlocked ... 
	veh.SetCarDoorLocks(CARLOCK_UNLOCKED);
	//Scenario generated cars dont need to be hotwired ... 
	// IE cars on chains that a ped walks up to an uses.
	veh.m_nVehicleFlags.bCarNeedsToBeHotwired = false;

	if(info.GetIsFlagSet(CScenarioInfoFlags::VehicleSetupBrokenDown))
	{
		VehicleSetupBrokenDown(veh);
	}

	// If this model uses an increased spawn distance, bump up m_ExtendedRemovalRange
	// to hopefully prevent it from being removed at a too close distance.
	float distScale = GetVehicleScenarioDistScaleFactorForModel(veh.GetModelId(), pt->IsFlagSet(CScenarioPointFlags::AerialVehiclePoint));
	float extendedRemovalRange = 0.0f;
	if(distScale > 1.01f)
	{
		extendedRemovalRange = distScale*CVehiclePopulation::GetCullRange(1.0f, 1.0f);
	}

	// Also, if we are trying to spawn at an extra extended range because we are flying,
	// crank up the removal range so we don't immediately despawn.
	if(pt->HasExtendedRange() && ShouldExtendExtendedRangeFurther())
	{
		extendedRemovalRange = Max(extendedRemovalRange, GetExtendedExtendedRangeForVehicleSpawn()*distScale);
	}

	if(extendedRemovalRange > 0.0f)
	{
		int range = Min(0x7fff, (int)(extendedRemovalRange));
		veh.m_ExtendedRemovalRange = Max(veh.m_ExtendedRemovalRange, (s16)range);
	}

	// Force the vehicle to stay in real physics mode, if this flag is set.
	if(info.GetIsFlagSet(CScenarioInfoFlags::MustUseRealVehiclePhysics))
	{
		veh.m_nVehicleFlags.bCanMakeIntoDummyVehicle = false;
	}

	if (veh.GetVehicleType() == VEHICLE_TYPE_BOAT)
	{
		//Chained boats should not be anchored ... 
		if(!pt->IsChained())
		{
			((CBoat&)veh).GetAnchorHelper().Anchor(true);
		}
	}

	if (veh.InheritsFromPlane() && !pt->IsFlagSet(CScenarioPointFlags::LandVehicleOnArrival) && pt->IsFlagSet(CScenarioPointFlags::AerialVehiclePoint))
	{
		CPlane& p = static_cast<CPlane &>(veh);
		p.GetLandingGear().ControlLandingGear(&p, CLandingGear::COMMAND_RETRACT_INSTANT);
	}

	return true;
}


void CScenarioManager::ApplyVehicleColors(CVehicle& veh, const CAmbientVehicleModelVariations& vehVar)
{
	const CVehicleModelInfo* pModelInfo = veh.GetVehicleModelInfo();

	// Apply a whole color combination, if one is specified.
	if(vehVar.m_ColourCombination >= 0)
	{
		const int comb = vehVar.m_ColourCombination;
		if(taskVerifyf(comb < pModelInfo->GetNumPossibleColours(),
				"Invalid colour combination %d for vehicle model '%s'.",
				comb, pModelInfo->GetModelName()))
		{
			veh.SetBodyColour1(pModelInfo->GetPossibleColours(0, comb));
			veh.SetBodyColour2(pModelInfo->GetPossibleColours(1, comb));
			veh.SetBodyColour3(pModelInfo->GetPossibleColours(2, comb));
			veh.SetBodyColour4(pModelInfo->GetPossibleColours(3, comb));
			veh.SetBodyColour5(pModelInfo->GetPossibleColours(4, comb));
			veh.SetBodyColour6(pModelInfo->GetPossibleColours(5, comb));
		}
	}

	// Apply individual colors.
	if(vehVar.m_BodyColour1 >= 0)
	{
		veh.SetBodyColour1((u8)vehVar.m_BodyColour1);
	}
	if(vehVar.m_BodyColour2 >= 0)
	{
		veh.SetBodyColour2((u8)vehVar.m_BodyColour2);
	}
	if(vehVar.m_BodyColour3 >= 0)
	{
		veh.SetBodyColour3((u8)vehVar.m_BodyColour3);
	}
	if(vehVar.m_BodyColour4 >= 0)
	{
		veh.SetBodyColour4((u8)vehVar.m_BodyColour4);
	}
	if(vehVar.m_BodyColour5 >= 0)
	{
		veh.SetBodyColour5((u8)vehVar.m_BodyColour5);
	}
	if(vehVar.m_BodyColour6 >= 0)
	{
		veh.SetBodyColour6((u8)vehVar.m_BodyColour6);
	}
}


void CScenarioManager::ApplyVehicleVariations(CVehicle& veh, const CAmbientVehicleModelVariations& vehVar)
{
	const CVehicleModelInfo* pModelInfo = veh.GetVehicleModelInfo();

	// Apply the livery, if one is specified.
	if(vehVar.m_Livery >= 0)
	{
		veh.SetLiveryId(vehVar.m_Livery);
	}

	if(vehVar.m_Livery2 >= 0)
	{
		veh.SetLivery2Id(vehVar.m_Livery2);
	}

	// Turn extras on or off, if needed.
	for(int i = 0; i < CAmbientVehicleModelVariations::kNumExtras; i++)
	{
		eHierarchyId id = (eHierarchyId)(VEH_EXTRA_1 + i);

		CAmbientVehicleModelVariations::UseExtra e = vehVar.GetExtra(i);
		if(e == CAmbientVehicleModelVariations::CantUse)
		{
			if(veh.GetIsExtraOn(id))
			{
				veh.TurnOffExtra(id, true);
			}
		}
		else if(e == CAmbientVehicleModelVariations::MustUse)
		{
			if(!veh.GetIsExtraOn(id))
			{
				veh.TurnOffExtra(id, false);
			}
		}
	}

	ApplyVehicleColors(veh, vehVar);
	veh.UpdateBodyColourRemapping();

	if(vehVar.m_WindowTint >= 0)
	{
		vehicleAssertf(vehVar.m_WindowTint < CVehicleModelInfo::GetVehicleColours()->m_WindowColors.GetCount(), "VehicleVariation has an undefined WindowTint. Please update vehiclemodelsets.meta.");
		veh.GetVariationInstance().SetWindowTint((u8)vehVar.m_WindowTint);
	}

	// Next, we'll set up mods, starting by the kit.
	CVehicleVariationInstance& variation = veh.GetVariationInstance();
	const int kitIndex = vehVar.m_ModKit;
	if(kitIndex >= 0)
	{
		if(taskVerifyf(kitIndex < pModelInfo->GetNumModKits(),
				"Invalid mod kit %d for vehicle model '%s'.",
				kitIndex, pModelInfo->GetModelName()))
		{
			Assert(pModelInfo->GetModKit(kitIndex) < CVehicleModelInfo::GetVehicleColours()->m_Kits.GetCount());
			variation.SetKitIndex(pModelInfo->GetModKit(kitIndex));
		}
	}

	// Loop over the array of mods, and apply them one by one.
	const int numMods = vehVar.m_Mods.GetCount();
	for(int i = 0; i < numMods; i++)
	{
		const eVehicleModType modSlot = vehVar.m_Mods[i].m_ModType;
		if(modSlot >= VMT_TOGGLE_MODS && modSlot < VMT_MISC_MODS)
		{
			variation.ToggleMod(modSlot, true);
		}
		else if(taskVerifyf(modSlot >= 0 && modSlot < VMT_TOGGLE_MODS || modSlot == VMT_WHEELS || modSlot == VMT_WHEELS_REAR_OR_HYDRAULICS,
				"Invalid mod slot %d for vehicle '%s'.",
				(int)modSlot, pModelInfo->GetModelName()))
		{
			variation.SetModIndexForType(modSlot, vehVar.m_Mods[i].m_ModIndex, &veh, false);
		}
	}
}


void CScenarioManager::VehicleSetupBrokenDown(CVehicle& veh)
{
	if(fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < 0.5f)
	{	
		CCarDoor* pDSideDoor = veh.GetDoorFromId(VEH_DOOR_DSIDE_F);
		if(pDSideDoor)
			pDSideDoor->SetTargetDoorOpenRatio(fwRandom::GetRandomNumberInRange(0.1f, 0.5f), CCarDoor::DRIVEN_NORESET);
	}

	CCarDoor* pBonnet = veh.GetDoorFromId(VEH_BONNET);
	if(pBonnet)
		pBonnet->SetTargetDoorOpenRatio(1.0f, CCarDoor::DRIVEN_NORESET);

	veh.SetCarDoorLocks(CARLOCK_UNLOCKED);
	veh.m_nVehicleFlags.bCarNeedsToBeHotwired = false;
	veh.GetVehicleDamage()->SetEngineHealth(ENGINE_DAMAGE_FINSHED);

	// The broken down vehicles tend to not work well as parked superdummies, the hood
	// doesn't seem to open until they switch out from that mode. Don't let that happen:
	veh.m_nVehicleFlags.bMayBecomeParkedSuperDummy = false;
}


bool CScenarioManager::ReportCarGenVehicleSpawning(CVehicle& veh, const CCarGenerator& cargen)
{
	CScenarioPoint* point = GetScenarioPointForCarGen(cargen);
	if(point && point->IsInCluster())
	{
		//begin tracking this object ... 
		return SCENARIOCLUSTERING.RegisterCarGenVehicleData(*point, veh);
	}
	return true;
}


void CScenarioManager::ReportCarGenVehicleSpawnFinished(CVehicle& veh, const CCarGenerator& cargen, CScenarioPointChainUseInfo* pChainUseInfo)
{
	const int scenarioTypeFromCarGen = cargen.GetScenarioType();
	CScenarioInfo* pScenarioInfo = SCENARIOINFOMGR.GetScenarioInfoByIndex(scenarioTypeFromCarGen);
	if(taskVerifyf(pScenarioInfo, "Failed to find scenario info for type %d.", scenarioTypeFromCarGen))
	{
		pScenarioInfo->SetLastSpawnTime(fwTimer::GetTimeInMilliseconds());
	}

	CScenarioPoint* point = GetScenarioPointForCarGen(cargen);
	if (point)
	{
		// If we are a clustered point
		if (point->IsInCluster())
		{
			taskAssert(!pChainUseInfo);		// The cargen shouldn't have created one of those in the clustered case.

			// Tracking is now done from ReportCarGenVehicleSpawning(), nothing to do here now.

			// Note: we don't add to the history here - that's done only if the spawning of
			// the whole cluster was considered a success.
		}
		else
		{
			const int scenarioType = point->GetScenarioTypeVirtualOrReal();
			taskAssert(!SCENARIOINFOMGR.IsVirtualIndex(scenarioType));

			// This was added to see if we somehow get the history messed up by misidentifying which point we spawned from.
			taskAssertf(scenarioType == scenarioTypeFromCarGen, "Apparent scenario type mismatch, %d/%d.", scenarioTypeFromCarGen, scenarioType);

			sm_SpawnHistory.Add(veh, *point, scenarioType, pChainUseInfo);
		}
	}
}


bool CScenarioManager::AllowExtraChainedUser(const CScenarioPoint& pt)
{
	Assert(pt.IsChained());

	if(pt.IsInCluster())
	{
		const int scenarioType = pt.GetScenarioTypeVirtualOrReal();
		if(scenarioType >= 0 && !SCENARIOINFOMGR.IsVirtualIndex(scenarioType))
		{
			const CScenarioInfo* pScenarioInfo = SCENARIOINFOMGR.GetScenarioInfoByIndex(scenarioType);
			if(pScenarioInfo)
			{
				if(pScenarioInfo->GetIsClass<CScenarioVehicleInfo>())
				{
					const CScenarioVehicleInfo& vehInfo = *static_cast<const CScenarioVehicleInfo*>(pScenarioInfo);
					if(vehInfo.GetProbabilityForDriver() <= 0.0f)
					{
						// Driver-less vehicles are allowed to go beyond the normal limit.
						return true;
					}
				}
			}
		}
	}

	return false;
}


CScenarioPoint* CScenarioManager::GetScenarioPointForCarGen(const CCarGenerator& cargen)
{
	int cargenIndex = CTheCarGenerators::GetIndexForCarGenerator(&cargen);
	if (Verifyf(cargenIndex >= 0 && cargenIndex < sm_ScenarioPointForCarGen.GetCount(), 
		"CargenIndex %d is invalid, scenario array size is %d.", cargenIndex, sm_ScenarioPointForCarGen.GetCount()))
	{
		Assert(&cargen == CTheCarGenerators::GetCarGenerator(cargenIndex));
		return sm_ScenarioPointForCarGen[cargenIndex];
	}
	return NULL;
}


void CScenarioManager::GetCarGenRequiredObjects(const CCarGenerator& cargen,
		int& numRequiredPedsOut,
		int& numRequiredVehiclesOut, int& numRequiredObjectsOut)
{
	numRequiredPedsOut = 0;
	numRequiredVehiclesOut = 0;
	numRequiredObjectsOut = 0;

	const CScenarioVehicleInfo* pInfo = GetScenarioVehicleInfoForCarGen(cargen);
	if(!pInfo)
	{
		return;
	}
	const CScenarioVehicleInfo& info = *pInfo;

	if(info.AlwaysNeedsDriver())
	{
		numRequiredPedsOut++;
	}	

	if(info.AlwaysNeedsPassengers())
	{
		numRequiredPedsOut += info.GetMinNumPassengers();
	}

	fwModelId trailer;
	if(GetCarGenTrailerToUse(cargen, trailer))
	{
		// Not sure if this is 100% correct or if the network system needs to make a distinction
		// between a trailer and an automobile.
		numRequiredVehiclesOut++;
	}

	u32 layoutHash = info.GetScenarioLayoutForPedsHash();
	if(layoutHash)
	{
		const CVehicleScenarioLayoutInfo* pLayout = CVehicleMetadataMgr::GetInstance().GetVehicleScenarioLayoutInfo(layoutHash);
		if(taskVerifyf(pLayout, "Failed to find vehicle scenario layout %s.", info.GetScenarioLayoutForPeds()))
		{
			// Note: perhaps we should loop over the points and check the probability for spawning, only considering
			// a ped to be required if the probability is high (1?) to spawn. However, it doesn't look like
			// SpawnPedsNearVehicleUsingLayout() is currently using a per-point spawn probability, so this
			// just matches that.
			const int numPoints = pLayout->GetNumScenarioPoints();
			numRequiredPedsOut += numPoints;
		}
	}
}


bool CScenarioManager::GetCarGenTrailerToUse(const CCarGenerator& cargen, fwModelId& trailerModelOut)
{
	const CScenarioVehicleInfo* pInfo = GetScenarioVehicleInfoForCarGen(cargen);
	if(!pInfo)
	{
		return false;
	}

	const CAmbientModelSet* pTrailerModels = pInfo->GetTrailerModelSet();
	if(pTrailerModels && pTrailerModels->GetNumModels() > 0)
	{
		const int cargenIndex = CTheCarGenerators::GetIndexForCarGenerator(&cargen);

		static const int kRandPeriod = 30000;	// Allow a new random roll every 30 seconds.
		static const int kExtraOffset = 2838;	// MAGIC! Just some offset to decorrelate from the mthRandom used elsewhere for vehicle generators.
		const int seed = 100*cargenIndex + fwTimer::GetTimeInMilliseconds()/kRandPeriod + sm_ScenarioProbabilitySeedOffset + kExtraOffset;

		mthRandom rnd(seed);

		// Check the probability for trailers, using the special random number generator
		// from earlier giving us consistent results across calls.
		if(rnd.GetFloat() < pInfo->GetProbabilityForTrailer())
		{
			u32 modelHash = pTrailerModels->GetRandomModelHash(rnd.GetFloat());
			CModelInfo::GetBaseModelInfoFromHashKey(modelHash, &trailerModelOut);
			if(trailerModelOut.IsValid())
			{
				return true;
			}
		}
	}

	return false;
}


const CScenarioVehicleInfo* CScenarioManager::GetScenarioVehicleInfoForCarGen(const CCarGenerator& cargen)
{
	const int scenarioType = cargen.GetScenarioType();
	if(!taskVerifyf(scenarioType >= 0, "Invalid scenario type %d.", scenarioType))
	{
		return NULL;
	}
	const CScenarioInfo* pScenarioInfo = CScenarioManager::GetScenarioInfo(scenarioType);
	if(!taskVerifyf(pScenarioInfo, "Failed to find scenario info for type %d.", scenarioType))
	{
		return NULL;
	}
	if(!taskVerifyf(pScenarioInfo->GetIsClass<CScenarioVehicleInfo>(), "Not a CScenarioVehicleInfo scenario, for some reason (type %d).", scenarioType))
	{
		return NULL;
	}
	return static_cast<const CScenarioVehicleInfo*>(pScenarioInfo);
}

bool CScenarioManager::ShouldWakeUpVehicleScenario(const CCarGenerator& cargen, bool desiredWakeUp)
{		
	// we can never have negative case here so quick out if desired is true
	if (desiredWakeUp)
		return desiredWakeUp;
		
	bool retval = desiredWakeUp;
	CScenarioPoint* pt = GetScenarioPointForCarGen(cargen);
	if (pt)
	{
		//If the point is in a cluster then we want to force it to attempt to wake up ...
		// the scenario clustering system will turn off the cargenerator when it is not attempting to spawn
		if (pt->IsInCluster())
		{
			retval = true;
		}
	}
	return retval;
}

bool CScenarioManager::ShouldDoOutsideRangeOfAllPlayersCheck(const CCarGenerator& cargen)
{
	bool retval = true;
	CScenarioPoint* pt = GetScenarioPointForCarGen(cargen);
	if (pt)
	{
		//If the point is in a cluster then we want to force it to attempt to wake up ...
		// the scenario clustering system will turn off the cargenerator when it is not attempting to spawn
		if (pt->IsInCluster())
		{
			retval = false;
		}
	}
	return retval;
}

CScenarioManager::CarGenConditionStatus CScenarioManager::CheckPedsNearVehicleUsingLayoutConditions(const CCarGenerator& UNUSED_PARAM(cargen), const CVehicleScenarioLayoutInfo& layout, fwModelId carModelToUse)
{
	CScenarioManager::CarGenConditionStatus ret = CScenarioManager::kCarGenCondStatus_Ready;

	const int numPoints = layout.GetNumScenarioPoints();
	for(int i = 0; i < numPoints; i++)
	{
		const CExtensionDefSpawnPoint* def = layout.GetScenarioPointInfo(i);
		if(!def)
		{
			continue;
		}

		CScenarioPoint tempPt;
		tempPt.InitArchetypeExtensionFromDefinition(def, NULL);

		// Note: we could transform the point to world space here, using something similar to
		// TransformScenarioPointForEntity(), but there shouldn't currently be a need for that.

		const int scenarioType = tempPt.GetScenarioTypeVirtualOrReal();

		// We currently don't support virtual scenario types here. It would be easy enough to
		// pick one, but we don't really want to just get one randomly since then we'll end
		// up with different results between the call to GedPedTypeForScenario() and the later
		// call to CreateScenarioPed(), etc.
		taskAssertf(!SCENARIOINFOMGR.IsVirtualIndex(scenarioType), "Virtual scenarios not supported for spawning peds around vehicles.");

		// TODO: Maybe check conditions for if we should spawn here
		// TODO: Maybe check probability, and don't reject if not 100%

		fwModelId vehModelIdToPickDriverFor;
		const CScenarioInfo* pInfo = GetScenarioInfo(scenarioType);
		if(pInfo && pInfo->GetIsFlagSet(CScenarioInfoFlags::ChooseModelFromDriversWhenAttachedToVehicle))
		{
			vehModelIdToPickDriverFor = carModelToUse;
		}

		strLocalIndex newPedModelIndex = strLocalIndex(fwModelId::MI_INVALID);
		const Vec3V posV = tempPt.GetPosition();
		if(!GetPedTypeForScenario(scenarioType, tempPt, newPedModelIndex, RCC_VECTOR3(posV), 0, NULL, 0, vehModelIdToPickDriverFor))
		{
			ret = CScenarioManager::kCarGenCondStatus_NotReady;
		}
	}

	return ret;
}


bool CScenarioManager::SpawnPedsNearVehicleUsingLayout(/*const*/ CVehicle& veh, int scenarioType,
		int* numSpawnedOut, CPed** spawnedPedArrayOut, int spaceInArray)
{
	if(numSpawnedOut)
	{
		*numSpawnedOut = 0;
	}

	const CScenarioInfo* pScenarioInfo = CScenarioManager::GetScenarioInfo(scenarioType);
	if(!pScenarioInfo || !pScenarioInfo->GetIsClass<CScenarioVehicleInfo>())
	{
		// Probably shouldn't happen.
		return true;
	}

	const CScenarioVehicleInfo& info = *static_cast<const CScenarioVehicleInfo*>(pScenarioInfo);
	u32 layoutHash = info.GetScenarioLayoutForPedsHash();
	if(!layoutHash)
	{
		return true;
	}

	const CVehicleScenarioLayoutInfo* pLayout = CVehicleMetadataMgr::GetInstance().GetVehicleScenarioLayoutInfo(layoutHash);
	if(!taskVerifyf(pLayout, "Failed to find vehicle scenario layout %s.", info.GetScenarioLayoutForPeds()))
	{
		return true;
	}

	const CVehicleScenarioLayoutInfo& layout = *pLayout;
	eVehicleScenarioPedLayoutOrigin origin = info.GetScenarioLayoutOrigin();

	const int numPoints = layout.GetNumScenarioPoints();
	int numFailed = 0;

	int numSpawned = 0;

	for(int i = 0; i < numPoints; i++)
	{
		const CExtensionDefSpawnPoint* def = layout.GetScenarioPointInfo(i);
		if(!def)
		{
			continue;
		}

		// Not sure if we should have this temporary object or not.
		CScenarioPoint tempPt;
		tempPt.InitArchetypeExtensionFromDefinition(def, NULL);

		Vec3V localPosV = tempPt.GetLocalPosition();
		switch(origin)
		{
			case kLayoutOriginVehicle:
				break;
			case kLayoutOriginVehicleFront:
				localPosV.SetYf(localPosV.GetYf() + veh.GetBoundingBoxMax().y);
				break;
			case kLayoutOriginVehicleBack:
				localPosV.SetYf(localPosV.GetYf() + veh.GetBoundingBoxMin().y);
				break;
			default:
				Assert(0);
				break;
		};
		tempPt.SetLocalPosition(localPosV);

		// Transform the point to world space, as it's not actually going to be used as an attached point.
		TransformScenarioPointForEntity(tempPt, veh);

		// TODO: Probably should remove CScenarioPoint::SetEntity(), doesn't look like we ended up needing it here.
		//	tempPt.SetEntity(&veh);

		const int scenarioType = tempPt.GetScenarioTypeVirtualOrReal();
		taskAssert(!SCENARIOINFOMGR.IsVirtualIndex(scenarioType));

		const CScenarioInfo* pScenarioInfo = CScenarioManager::GetScenarioInfo(scenarioType);

		// Note: would be good to call SetupScenarioAndTask(), but there is a bit of an issue with how
		// it sets up the tasks, due to how we use a temporary point that's not actually in the world.

		Vec3V spawnPosV;
		float fHeading;
		CTask* pScenarioTask = NULL;
		if(taskVerifyf(pScenarioInfo->GetIsClass<CScenarioPlayAnimsInfo>(), "Unsupported scenario class for type '%s'.", pScenarioInfo->GetName()))
		{
			spawnPosV = tempPt.GetPosition();
			fHeading = SetupScenarioPointHeading(pScenarioInfo, tempPt);

			s32 iFlags = CTaskUseScenario::SF_SkipEnterClip | CTaskUseScenario::SF_Warp | CTaskUseScenario::SF_FlagsForAmbientUsage;
			pScenarioTask = rage_new CTaskUseScenario(scenarioType, iFlags);
		}
		else
		{
			continue;
		}

		if(pScenarioTask)
		{
			const PedTypeForScenario* pPredeterminedPedType = NULL;
			PedTypeForScenario pedType;
			if(pScenarioInfo && pScenarioInfo->GetIsFlagSet(CScenarioInfoFlags::ChooseModelFromDriversWhenAttachedToVehicle))
			{
				if(!SelectPedTypeForScenario(pedType, scenarioType, tempPt, RCC_VECTOR3(spawnPosV), 0, 0, veh.GetModelId(), true))
				{
					delete pScenarioTask;
					pScenarioTask = NULL;

					numFailed++;
					continue;
				}
				pPredeterminedPedType = &pedType;
			}

			// TODO: Think more about these. We may want to bypass visibility checks and stuff here, as we may have
			// already done them for the vehicle.
			float maxFrustumDistToCheck = 0.0f;
			bool allowDeepInteriors = false;
			u32 iSpawnOptions = CPedPopulation::SF_forceSpawn;
			bool findZCoorForPed = true;	// The vehicle may be on uneven ground, so it's probably worth probing in this case.
			CPed* pPed = CreateScenarioPed(scenarioType, tempPt, pScenarioTask, RC_VECTOR3(spawnPosV), maxFrustumDistToCheck, allowDeepInteriors, fHeading, iSpawnOptions, 0, findZCoorForPed, NULL, pPredeterminedPedType);
			if(!pPed)
			{
				numFailed++;
			}
			else
			{
				// If we got an array from the user, populate it and stop counting if we hit the limit.
				// If we don't have an array, just count how many we spawned.
				if(spawnedPedArrayOut)
				{
					if(aiVerifyf(numSpawned < spaceInArray, "Ran out of space to return peds spawned in layout near vehicle, size was %d.", spaceInArray))
					{
						spawnedPedArrayOut[numSpawned++] = pPed;
					}
				}
				else
				{
					numSpawned++;
				}

				pPed->SetMyVehicle(&veh);
			}
		}
	}

	if(numSpawnedOut)
	{
		*numSpawnedOut = numSpawned;
	}

	// Check if entering this vehicle generates shocking events.
	if (info.GetIsFlagSet(CScenarioInfoFlags::ForceAlertWhenVehicleStartedByPlayer))
	{
		veh.m_nVehicleFlags.bAlertWhenEntered = true;
	}

	return numFailed == 0;
}


const CAmbientModelSet* CScenarioManager::GetVehicleModelSetFromIndex(const CScenarioVehicleInfo& info, const unsigned vehicleModelSetIndexFromPoint)
{
	const CAmbientModelSet* pModelSet = NULL;
	if(vehicleModelSetIndexFromPoint != CScenarioPointManager::kNoCustomModelSet)
	{
		pModelSet = &CAmbientModelSetManager::GetInstance().GetModelSet(CAmbientModelSetManager::kVehicleModelSets, vehicleModelSetIndexFromPoint);
	}
	else
	{
		pModelSet = info.GetModelSet();
	}
	return pModelSet;
}


bool CScenarioManager::HasValidVehicleModelFromIndex(const CScenarioVehicleInfo& info, const unsigned vehicleModelSetIndexFromPoint)
{
	const CAmbientModelSet* pModelSet = GetVehicleModelSetFromIndex(info, vehicleModelSetIndexFromPoint);
	if(pModelSet)
	{
		CAmbientModelSetFilterScenarioVehicle filter(info);
		if(!pModelSet->HasAnyValidModel(&filter))
		{
			return false;
		}
	}
	return true;
}

u32 CScenarioManager::GetRandomVehicleModelHashOverrideFromIndex(const CScenarioVehicleInfo& info, const unsigned vehicleModelSetIndexFromPoint)
{
	u32 modelHash = 0;

	const CAmbientModelSet* pModelSet = GetVehicleModelSetFromIndex(info, vehicleModelSetIndexFromPoint);

	// If there is a vehicle model set specified (in the point or info), pick a model to tell the
	// car generator to spawn.
	// Note: we may want to revisit this - perhaps we should only do this in the ForceModel case,
	// and perhaps even then we would be better off making the choice of which model to use later,
	// once we actually try to spawn there.
	if(pModelSet)
	{
		CAmbientModelSetFilterScenarioVehicle filter(info);

		// Get a random model from the set.
		modelHash = pModelSet->GetRandomModelHash(NULL, &filter, "VehicleScenario");
	}

	return modelHash;
}

bool CScenarioManager::CanVehicleModelBeUsedForBrokenDownScenario(const CVehicleModelInfo& modelInfo)
{
	return !modelInfo.GetVehicleFlag(CVehicleModelInfoFlags::FLAG_NO_BROKEN_DOWN_SCENARIO)
			&& modelInfo.m_data && modelInfo.GetApproximateBonnetHeight() < MIN_BONNET_HEIGHT_FOR_BONNET_CLIPS;
}


bool CScenarioManager::FindVehicleModelInModelSet(u32 modelNameHash, const CAmbientModelSet& modelSet,
		const CAmbientVehicleModelVariations* &varOut)
{
	varOut = NULL;
	const CAmbientModelVariations* pVariations = NULL;
	if(modelSet.GetContainsModel(modelNameHash, &pVariations))
	{
		if(pVariations)
		{
			if(taskVerifyf(pVariations->GetIsClass<CAmbientVehicleModelVariations>(),
					"Expected CAmbientVehicleModelVariations for vehicle model set '%s'.", modelSet.GetName()))
			{
				varOut = static_cast<const CAmbientVehicleModelVariations*>(pVariations);
			}
		}
		return true;
	}
	else
	{
		return false;
	}
}


bool CScenarioManager::FindPropsInEnvironmentForScenario(CObject*& pPropOut, const CScenarioPoint& point, int realScenarioType, bool allowUprooted)
{
	pPropOut = NULL;
	taskAssert(realScenarioType >= 0);
	const CScenarioInfo* pInfo = GetScenarioInfo(realScenarioType);
	if(!pInfo)
	{
		return true;
	}

	if(!pInfo->GetIsClass<CScenarioPlayAnimsInfo>())
	{
		return true;
	}

	if(!pInfo->GetIsFlagSet(CScenarioInfoFlags::UsePropFromEnvironment))
	{
		return true;
	}

	CObject* pObj = CTaskUseScenario::FindPropInEnvironmentNearPoint(point, realScenarioType, allowUprooted);
	if(!pObj)
	{
		return false;
	}

	pPropOut = pObj;
	return true;
}


void CScenarioManager::TransformScenarioPointForEntity(CScenarioPoint& pointInOut, const CEntity& ent)
{
	const fwTransform& transform = ent.GetTransform();

	const Vec3V transformedPosV = transform.Transform(pointInOut.GetPosition());

	Vec3V vDir(V_Y_AXIS_WZERO);
	vDir = RotateAboutZAxis(vDir, ScalarV(pointInOut.GetDirectionf()));
	vDir = transform.Transform3x3(vDir);

	pointInOut.SetDirection(rage::Atan2f(-vDir.GetXf(), vDir.GetYf()));
	pointInOut.SetPosition(transformedPosV);
}

u32 CScenarioManager::LegacyDefaultModelSetConvert(const u32 modelSetHash)
{
	static atHashString MODEL_SET_ANY("Any",0xDF3407B5);
	u32 retval = modelSetHash;
	if (retval == MODEL_SET_ANY)
	{
		retval = CScenarioManager::MODEL_SET_USE_POPULATION;
	}

	return retval;
}

bool CScenarioManager::StreamScenarioPeds(fwModelId pedModel, bool highPri, int ASSERT_ONLY(realScenarioType))
{
	const char* debugSourceName = NULL;
#if __ASSERT
	debugSourceName = GetScenarioName(realScenarioType);
#endif

	const bool startupMode = CPedPopulation::ShouldUseStartupMode();
	return gPopStreaming.StreamScenarioPeds(pedModel, highPri, startupMode, debugSourceName);
}

bool CScenarioManager::GetRangeValuesForScenario( s32 iScenarioType, float& fRange, s32& iMaxNum )
{
	const CScenarioInfo* pInfo = SCENARIOINFOMGR.GetScenarioInfoByIndex(iScenarioType);
	if( pInfo )
	{
		if( pInfo->GetMaxNoInRange() > 0 )
		{
			fRange = pInfo->GetRange();
			iMaxNum = pInfo->GetMaxNoInRange();
			return true;
		}
		else
		{
			return false;
		}
	}

	return false;
}

static u32 TIME_BETWEEN_ABANDONED_VEHICLE_CHECKS = 2000;
static float  MAX_DISTANCE_FROM_PED_TO_ABANDONED_CAR = 80.0f;

DefaultScenarioInfo CScenarioManager::GetDefaultScenarioInfoForRandomPed(const Vector3& pos, u32 modelIndex)
{
	if(CScenarioBlockingAreas::IsPointInsideBlockingAreas(pos, true, false))
	{
		return DefaultScenarioInfo(DS_Invalid, NULL);
	}

	// Make sure the model is valid.
	fwModelId potentialThiefModel((strLocalIndex(modelIndex)));
	if (!potentialThiefModel.IsValid())
	{
		return DefaultScenarioInfo(DS_Invalid, NULL);
	}

	// Make sure the model type isn't an animal model.
	const CPedModelInfo* pPedModelInfo = (CPedModelInfo*)CModelInfo::GetBaseModelInfo(potentialThiefModel);
	if (Verifyf(pPedModelInfo, "Ped model ID was valid, but no CPedModelInfo could be loaded!"))
	{
		if (pPedModelInfo->GetDefaultPedType() == PEDTYPE_ANIMAL)
		{
			return DefaultScenarioInfo(DS_Invalid, NULL); 
		}
	}
	else
	{
		return DefaultScenarioInfo(DS_Invalid, NULL);
	}

	// DEBUG!! -AC, Maybe this should only be for shady looking character types?  An office woman jumping into and stealing a car might look odd...
	// Every once in a while check if there are any abandoned
	// vehicles are laying about to steal.
	if((fwTimer::GetTimeInMilliseconds() - sm_AbandonedVehicleCheckTimer) > TIME_BETWEEN_ABANDONED_VEHICLE_CHECKS)
	{
		sm_AbandonedVehicleCheckTimer = fwTimer::GetTimeInMilliseconds();

		const CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();
		bool bSearchForVehicles = true;
		static dev_u32 s_nTimeSinceLastBattleEventMS = 40000;
		const u32 nLastBattleEventTime = pLocalPlayer ? pLocalPlayer->GetPedIntelligence()->GetLastBattleEventTime(true) : 0;
		if (pLocalPlayer && (nLastBattleEventTime > NetworkInterface::GetSyncedTimeInMilliseconds() - s_nTimeSinceLastBattleEventMS))
		{
			bSearchForVehicles = false;
		}

		if (bSearchForVehicles)
		{
			// Look for abandoned vehicles
			Vec3V vIteratorPos = VECTOR3_TO_VEC3V(pos);
			CEntityIterator entityIterator(IterateVehicles, NULL, &vIteratorPos, MAX_DISTANCE_FROM_PED_TO_ABANDONED_CAR);
			CVehicle* pVehicle = (CVehicle*)entityIterator.GetNext();
			while(pVehicle)
			{
				if(pVehicle->ShouldBeRemovedByAmbientPed())
				{	
					return DefaultScenarioInfo(DS_StealVehicle, pVehicle);
				}

				pVehicle = (CVehicle*)entityIterator.GetNext();
			}
		}
	}
	// END DEBUG!!

	return DefaultScenarioInfo(DS_Invalid, NULL);
}

void CScenarioManager::ApplyDefaultScenarioInfoForRandomPed(const DefaultScenarioInfo& scenarioInfo, CPed* pPed)
{
 	if(scenarioInfo.GetType() != DS_Invalid)
 	{
//  		if(scenarioInfo.GetType() == DS_StreetSweeper)
//  		{
//  			static s32 iScenario = CScenarioManager::Scenario_Sweeper;
//  			aiFatalAssert(iScenario != Scenario_Invalid );
//  			pPed->GetPedIntelligence()->AddTaskDefault(rage_new CTaskWanderingScenario(iScenario));
//  		}
 		/*else*/ if(scenarioInfo.GetType() == DS_StealVehicle && scenarioInfo.GetVehicleToSteal())
 		{
			// Note: the vehicle might have been removed after we planned on stealing it,
			// so we check for NULL above and just skip giving the task in that case.
	
			VehicleEnterExitFlags vehicleFlags;
			vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DontJackAnyone);

			pPed->GetPedIntelligence()->SetDriverAggressivenessOverride(1.0f);
			pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_JackedAbandonedCar, true);

			//clear this out so we don't try and get someone else to jack it right away.
			//if something happens to this ped, it will get reset if still causing an issue
			//for other cars later on, but it will give the initial ped enough time to
			//get to the car
			scenarioInfo.GetVehicleToSteal()->m_nVehicleFlags.bShouldBeRemovedByAmbientPed = false;
			scenarioInfo.GetVehicleToSteal()->m_nVehicleFlags.bDisablePretendOccupants = true;

			pPed->GetPedIntelligence()->AddTaskDefault(
				rage_new CTaskEnterVehicle(scenarioInfo.GetVehicleToSteal(), SR_Specific, scenarioInfo.GetVehicleToSteal()->GetDriverSeat()
				, vehicleFlags));
 		}
 	}
}

typedef struct
{
	Vector3 vPedPos;
	float	fPedHeading;
	s32	iPedModelIndex;
	CDummyPed*	pDummyPed;
} PedSpawnInfo;

#define MAX_GROUP_SPAWNED_PEDS 3

// dev_float DISTANCE_BETWEEN_PEDS_MIN = 0.7f;
// dev_float DISTANCE_BETWEEN_PEDS_MAX = 0.8f;
// dev_float DISTANCE_BETWEEN_PEDS_AV = (DISTANCE_BETWEEN_PEDS_MAX + DISTANCE_BETWEEN_PEDS_MIN)*0.5f;
// dev_float RANGE_VARIATION_MIN = -0.65f;
// dev_float RANGE_VARIATION_MAX = 0.65f;
// 
static const float GROUND_TEST_Z_HEIGHT_ABOVE = 1.0f;
static const float GROUND_TEST_Z_HEIGHT_BELOW = 1.5f;

int CScenarioManager::SetupCoupleScenario(CScenarioPoint& scenarioPoint, float maxFrustumDistToCheck, bool allowDeepInteriors, s32 iSpawnOptions, int scenarioType)
{
	const CScenarioInfo* pScenarioInfo = GetScenarioInfo(scenarioType);
	aiAssert(pScenarioInfo && pScenarioInfo->GetIsClass<CScenarioCoupleInfo>());

	const CScenarioCoupleInfo& coupleInfo = *static_cast<const CScenarioCoupleInfo*>(pScenarioInfo);

	// Create the peds
	CTask* pTask1 = rage_new CTaskCoupleScenario(scenarioType, &scenarioPoint, NULL, CTaskCoupleScenario::CF_Leader | CTaskCoupleScenario::CF_UseScenario);
	Vector3 vSpawnPos1 = VEC3V_TO_VECTOR3(scenarioPoint.GetPosition());
	Vector3 vOffset = coupleInfo.GetMoveFollowOffset();
	vOffset.RotateZ(scenarioPoint.GetDirectionf());
	Vector3 vSpawnPos2 = vSpawnPos1 + vOffset;

	float fHeading = SetupScenarioPointHeading(pScenarioInfo, scenarioPoint);

	int numPeds = 0;

	CPed* pPed1 = CreateScenarioPed(scenarioType, scenarioPoint, pTask1, vSpawnPos1, maxFrustumDistToCheck, allowDeepInteriors, fHeading, iSpawnOptions, 0, true);
	if(pPed1)
	{
		CTask* pTask2 = rage_new CTaskCoupleScenario(scenarioType, &scenarioPoint, pPed1, CTaskCoupleScenario::CF_UseScenario);
		CPed* pPed2 = CreateScenarioPed(scenarioType, scenarioPoint, pTask2, vSpawnPos2, maxFrustumDistToCheck, allowDeepInteriors, fHeading, iSpawnOptions | CPedPopulation::SF_ignoreScenarioHistory, 1, true);
		if(pPed2)
		{

			CTask* pPed1ActualTask = pPed1->GetPedIntelligence()->GetTaskDefault();
			if(pPed1ActualTask && pPed1ActualTask->GetTaskType() == CTaskTypes::TASK_UNALERTED)
			{
				pPed1ActualTask = static_cast<CTaskUnalerted*>(pPed1ActualTask)->GetScenarioTask();
			}
			if(taskVerifyf(pPed1ActualTask && pPed1ActualTask->GetTaskType() == CTaskTypes::TASK_COUPLE_SCENARIO, "Expected couple scenario task."))
			{
				CTaskCoupleScenario* pPed1CoupleTask = static_cast<CTaskCoupleScenario*>(pPed1ActualTask);
				pPed1CoupleTask->SetPartnerPed(pPed2);
			}

			numPeds = 2;
		}
		else
		{
			CPedFactory::GetFactory()->DestroyPed(pPed1);
			numPeds = 0;
		}
	}

	return numPeds;
}

//-------------------------------------------------------------------------
// Sets up a group ped scenario
//-------------------------------------------------------------------------
// DEBUG!! -AC, This isn't checking 	CPedPopulation::IsPedGenerationCoordCurrentlyValid() or anything like it, so it needs to add that here somewhere...
s32 CScenarioManager::SetupGroupScenarioPoint( s32 UNUSED_PARAM(scenario), CScenarioPoint& UNUSED_PARAM(scenarioPoint), s32 UNUSED_PARAM(iMaxPeds), s32 UNUSED_PARAM(iSpawnOptions) )
{
#if 0
	PF_FUNC(CreateScenarioPedTotal);

	if( pAttachedEntity )	
	{
		return 0;
	}

	if( iMaxPeds <= 1 )
	{
		return 0;
	}

	PedSpawnInfo pedSpawnInfo[MAX_GROUP_SPAWNED_PEDS];
	s32 iNumPeds = MIN(fwRandom::GetRandomNumberInRange(2, MAX_GROUP_SPAWNED_PEDS+1), iMaxPeds);
	Vector3 vDir = pEffect->GetSpawnPoint()->GetSpawnPointDirection(pAttachedEntity);
	float	fEffectHeading = rage::Atan2f(-vDir.x, vDir.y);
	s32 iChattingPed = 0;
	//s32 iListeningPed = fwRandom::GetRandomNumberInRange(1, iNumPeds);

	Vector3 vEffectPos = pEffect->GetWorldPosition(pAttachedEntity);
	CSpawnPoint* sp = pEffect->GetSpawnPoint();
	aiFatalAssertf(sp, "Null spawnpoint pointer!");

	CScenarioInfo* pScenarioInfo = CScenarioManager::GetScenarioInfo(scenario);
	// If the spawnpoint is outside and its raining, dont spawn
	if( (pAttachedEntity == NULL && sp->iInteriorState == CSpawnPoint::IS_Outside) || 
		(pAttachedEntity && !pAttachedEntity->m_nFlags.bInMloRoom) )
	{
		if( g_weather.IsRaining() && pScenarioInfo && (pScenarioInfo->GetIsFlagSet(CScenarioInfoFlags::DontSpawnInRain)) )
		{
			return 0;
		}
	}

	{PF_FUNC(CreateScenarioPedSetup);

	s32 iRequiredSpeech = 0;
	// Spawn the peds
	for( s32 i = 0; i < iNumPeds; i++ )
	{
		CInteriorInst* pInteriorInst = NULL;
		if( pAttachedEntity && pAttachedEntity->GetIsInInterior() )
			pInteriorInst = pAttachedEntity->GetInteriorLocation().GetInteriorInst();

		s32		newPedModelIndex;
		bool bGotPedType = GetPedTypeForScenario(scenario, pEffect, newPedModelIndex, vEffectPos, pAttachedEntity, pInteriorInst, iRequiredSpeech );
		if( bGotPedType && i > 0 && iRequiredSpeech != 0 && newPedModelIndex == pedSpawnInfo[0].pDummyPed->GetModelIndex() )
		{
			iRequiredSpeech = 0;
			bGotPedType = GetPedTypeForScenario(scenario, pEffect, newPedModelIndex, vEffectPos, pAttachedEntity, pInteriorInst, iRequiredSpeech );
		}
		if( !bGotPedType )
		{
			if( pAttachedEntity == NULL && (sp->iInteriorState == CSpawnPoint::IS_Inside || sp->iInteriorState == CSpawnPoint::IS_InsideOffice) )
				sp->iInteriorState = CSpawnPoint::IS_Unknown;

			for( s32 j = 0; j < i; j++ )
			{
				CPedPopulation::RemovePed(pedSpawnInfo[j].pDummyPed);
			}
			return 0;
		}

		if( i == 0 )
		{
			CPedModelInfo* pPedModelInfo = (CPedModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(newPedModelIndex));
			iRequiredSpeech = static_cast<u32>(g_SpeechManager.GetPossibleConversationTopicsForPvg(pPedModelInfo->GetPedVoiceGroup())) & (AUD_CONVERSATION_TOPIC_SMOKER|AUD_CONVERSATION_TOPIC_BUSINESS|AUD_CONVERSATION_TOPIC_BUM|AUD_CONVERSATION_TOPIC_CONSTRUCTION|AUD_CONVERSATION_TOPIC_GANG);
		}

		float fRandRotation = ((TWO_PI * i)/(float)iNumPeds) + ( fwRandom::GetRandomNumberInRange(RANGE_VARIATION_MIN, RANGE_VARIATION_MAX) * (float)(2.0f/iNumPeds));

		float fHeading = fEffectHeading + fRandRotation;
		Vector3 vPedRootPos = vDir*(-fwRandom::GetRandomNumberInRange(DISTANCE_BETWEEN_PEDS_MIN, DISTANCE_BETWEEN_PEDS_MAX));
		vPedRootPos.RotateZ(fRandRotation);

		Vector3 EffectPos;
		pEffect->GetPos(EffectPos);

		vPedRootPos += EffectPos + (vDir*DISTANCE_BETWEEN_PEDS_AV);

		s32 iRoomIdFromCollision = 0;

		//		Vector3 vOriginalPedRootPos = vPedRootPos;
		Vector3 vGroundAdjustedPedRootPos = vPedRootPos; 
		{
			PF_FUNC(CreateScenarioPedZCoord);
			if( !CPedPlacement::FindZCoorForPed(BOTTOM_SURFACE, &vGroundAdjustedPedRootPos, NULL, &iRoomIdFromCollision, &pInteriorInst, GROUND_TEST_Z_HEIGHT_ABOVE, GROUND_TEST_Z_HEIGHT_BELOW) &&
				!CPedPlacement::FindZCoorForPed(BOTTOM_SURFACE, &vGroundAdjustedPedRootPos, NULL, &iRoomIdFromCollision, &pInteriorInst, GROUND_TEST_Z_HEIGHT_ABOVE, GROUND_TEST_Z_HEIGHT_BELOW, ArchetypeFlags::GTA_OBJECT_TYPE ) )
			{
				if( pAttachedEntity == NULL && ( sp->iInteriorState == CSpawnPoint::IS_Inside || sp->iInteriorState == CSpawnPoint::IS_InsideOffice ) )
					sp->iInteriorState = CSpawnPoint::IS_Unknown;
				for( s32 j = 0; j < i; j++ )
				{
					CPedPopulation::RemovePed(pedSpawnInfo[j].pDummyPed);
				}
				return 0;
			}
		}

		// Update the interior state of the spawn point if this isn't an attached point
		if( pAttachedEntity == NULL && sp->iInteriorState == CSpawnPoint::IS_Unknown )
		{
			if( iRoomIdFromCollision != 0 && pInteriorInst )
			{
				// If only just tagged as an interior, allow it to scan next time round
				// As it changes the ped types available
				if( pInteriorInst->IsOfficeMLO() )
				{
					sp->iInteriorState = CSpawnPoint::IS_InsideOffice;
					for( s32 j = 0; j < i; j++ )
					{
						CPedPopulation::RemovePed(pedSpawnInfo[j].pDummyPed);
					}
					return 0;
				}
				else
				{
					sp->iInteriorState = CSpawnPoint::IS_Inside;
				}
			}
			else
			{
				sp->iInteriorState = CSpawnPoint::IS_Outside;
				if(!(iSpawnOptions & CPedPopulation::SF_forceSpawn ))
				{
					for( s32 j = 0; j < i; j++ )
					{
						CPedPopulation::RemovePed(pedSpawnInfo[j].pDummyPed);
					}
					return 0;
				}
			}
		}

		// If the spawnpoint is outside and its raining, dont spawn
		if( (pAttachedEntity == NULL && sp->iInteriorState == CSpawnPoint::IS_Outside) )
		{
			if( g_weather.IsRaining() && pScenarioInfo && (pScenarioInfo->GetIsFlagSet(CScenarioInfoFlags::DontSpawnInRain)) )
			{
				for( s32 j = 0; j < i; j++ )
				{
					CPedPopulation::RemovePed(pedSpawnInfo[j].pDummyPed);
				}
				return 0;
			}
		}

		CScenarioInfo* pScenarioInfo = GetScenarioInfo(scenario);
		if( !pScenarioInfo->GetIsClass<CScenarioSeatedGroupInfo>() || i != iChattingPed )
		{
			vPedRootPos = vGroundAdjustedPedRootPos;
		}

		if (pScenarioInfo->GetIsClass<CScenarioSeatedInfo>())
		{
			vPedRootPos += static_cast<CScenarioSeatedInfo*>(pScenarioInfo)->GetSpawnPointAdditionalOffset();
		}

		DefaultScenarioInfo defaultScenarioInfo(DS_Invalid, NULL);
		fHeading = fwAngle::LimitRadianAngle(fHeading);

		// Try to create and add a dummy ped into the world (possibly in an interior).
		pedSpawnInfo[i].pDummyPed = NULL;
		if( iRoomIdFromCollision != 0 )
		{
			aiFatalAssertf(pInteriorInst, "NULL interior inst!");
			// 			spdSphere testSphere(RCC_VEC3V(vOriginalPedRootPos), ScalarV(V_FLT_SMALL_1));
			// 			g_pIntInst = NULL;
			// 			CGameWorld::ForAllEntitiesIntersecting(testSphere,  ScenarioInteriorCollisionCB, (void*)&testSphere, ENTITY_TYPE_MASK_MLO, SEARCH_LOCATION_EXTERIORS);
			pedSpawnInfo[i].pDummyPed = CPedPopulation::AddDummyPed(newPedModelIndex, vPedRootPos, fHeading, NULL, true, iRoomIdFromCollision, pInteriorInst, true, false, false, true, defaultScenarioInfo, true );
		}
		else
		{
			pedSpawnInfo[i].pDummyPed = CPedPopulation::AddDummyPed(newPedModelIndex, vPedRootPos, fHeading, NULL, false, -1, NULL, true, false, false, true, defaultScenarioInfo, true );
		}

		if(!pedSpawnInfo[i].pDummyPed)
		{
			for( s32 j = 0; j < i; j++ )
			{
				CPedPopulation::RemovePed(pedSpawnInfo[j].pDummyPed);
			}
			return 0;
		}

	}
	}


	// assign the tasks
	for( s32 i = 0; i < iNumPeds; i++ )
	{
		PF_FUNC(CreateScenarioPedAddPed);
		s32 standingScenario = scenario;

		CTaskStationaryScenario* pTask = rage_new CTaskStationaryScenario(standingScenario, pEffect, NULL, true, true);

		// Get the dummy task for this scenario.
		CDummyTask* pDummyTask = pTask->GetDummyTaskEquivalent();
		aiFatalAssertf(pDummyTask, "Null dummytask generated!");
		if(!pedSpawnInfo[i].pDummyPed->SetDummyPedTask(pDummyTask, CDummyTask::ABORT_PRIORITY_IMMEDIATE))
		{
			aiFatalAssertf(0, "Couldn't assign new dummy task!");
		}

		// Do one update of the task right now, so that the task has a chance to set
		// blends on clips before they go away.
		bool taskFinished = pDummyTask->ProcessDummyPed(pedSpawnInfo[i].pDummyPed);
		if(taskFinished)
		{
			aiFatalAssertf(false,"Dummy Task %s has incorrectly finished on its very first update",pDummyTask->GetName());
		}

		delete pTask;
		pTask = NULL;
		// 		if( i == iChattingPed )
		// 		{
		// 			s32 iSubTaskType = CTaskTypes::TASK_STATIONARY_SCENARIO;
		// 			if( scenario == Seat_OnStepsHangOut || scenario == Seat_OnWallHangOut )
		// 			{
		// 				iSubTaskType = CTaskTypes::TASK_SEATED_SCENARIO;		
		// 			}
		// 			pTask = new CTaskChatScenario(scenario, pedSpawnInfo[iListeningPed].pPed, pEffect, CTaskChatScenario::CS_AboutToSayStatement, iSubTaskType );
		// 			
		// 		}
		// 		else if( i == iListeningPed )
		// 		{
		// 			pTask = new CTaskChatScenario(standingScenario, pedSpawnInfo[iChattingPed].pPed, pEffect, CTaskChatScenario::CS_AboutToListenToStatement );
		// 		}
		// 		else
		// 		{
		// 			pTask = new CTaskStationaryScenario(standingScenario, pEffect, NULL, true, true);
		// 		}
		// 		pedSpawnInfo[i].pPed->GetPedIntelligence()->AddTaskDefault(pTask);
		// 
		// 		if( !pedSpawnInfo[i].pPed->GetPedIntelligence()->GetTaskDefault() )
		// 		{
		// 			pedSpawnInfo[i].pPed->GetPedIntelligence()->AddTaskDefault( pedSpawnInfo[i].pPed->ComputeDefaultTask(*pedSpawnInfo[i].pPed));
		// 		}
		//		pedSpawnInfo[i].pPed->PopTypeSet(RANDOM_SCENARIO);
		//		pedSpawnInfo[i].pPed->SetDefaultDecisionMaker();
		//		pedSpawnInfo[i].pPed->SetCharParamsBasedOnManagerType();
		//		pedSpawnInfo[i].pPed->GetPedIntelligence()->SetDefaultRelationshipGroup();
		// 
		// 		// Update this peds queriable state, this is used to establish which props are available
		// 		pedSpawnInfo[i].pPed->GetPedIntelligence()->UpdateQueriableState();
		// 		CTaskSimplePlayRandomAmbients::CheckForAmbientPropAtStartup(*pedSpawnInfo[i].pPed);
		// 
		// 		// Check if we are using the dummy ped system.
		// 		if(CPedPopulation::ms_allowDummyPeds)
		// 		{
		// 			PF_START(CreateScenarioPedConvertToDummy);
		// 
		// 			// Try to convert the new ped into a dummy ped, if this fails then we will
		// 			// will keep the real ped.
		// 			CPedPopulation::TryToConvertPedIntoDummyPed(pedSpawnInfo[i].pPed, false);
		// 			PF_STOP(CreateScenarioPedConvertToDummy);
		// 		}
	}

	return iNumPeds;
#endif
	return 0;
}
// END DEBUG!!


//------------------------------I gues-------------------------------------------
// Sets up the task and the scenario for a ped todo with a particular scenario
// Returns NULL for no ped
//-------------------------------------------------------------------------
CTask* CScenarioManager::SetupScenarioAndTask( s32 scenario, Vector3& vPedPos, float& fHeading, CScenarioPoint& scenarioPoint, s32 /*iSpawnOptions*/, bool bStartInCurrentPos, bool bUsePointForTask, bool bForAmbientUsage, bool bSkipIntroClip )
{
	CTaskScenario* pTask = NULL;
	const CScenarioInfo* pScenarioInfo = CScenarioManager::GetScenarioInfo(scenario);

	CScenarioPoint* pPointForTask = bUsePointForTask ? &scenarioPoint : NULL;

	if (aiVerifyf(pScenarioInfo,"NULL scenario info"))
	{
		if (pScenarioInfo->GetIsClass<CScenarioPlayAnimsInfo>())
		{
			vPedPos = VEC3V_TO_VECTOR3(scenarioPoint.GetPosition());
			fHeading = SetupScenarioPointHeading(pScenarioInfo, scenarioPoint);
			s32 iFlags = CTaskUseScenario::SF_CheckShockingEvents;
			if (bSkipIntroClip)
			{
				iFlags |= CTaskUseScenario::SF_SkipEnterClip;
			}
			if (!bStartInCurrentPos)
			{
				if (pPointForTask && !pPointForTask->IsInWater())
				{
					iFlags |= CTaskUseScenario::SF_Warp;
				}

				// For random peds that spawn in place, reduce the use time by a random amount.
				if(bForAmbientUsage)
				{
					iFlags |= CTaskUseScenario::SF_AdvanceUseTimeRandomly;

					// In MP games, we flag the task to add to the history, if we're spawning at this point.
					// This makes the scenario history mechanism work better over the network, because the
					// cloned tasks will add to the history on the remote machines.
					if(NetworkInterface::IsGameInProgress())
					{
						iFlags |= CTaskUseScenario::SF_AddToHistory;
					}
				}
			}

			if(bForAmbientUsage)
			{
				iFlags |= CTaskUseScenario::SF_FlagsForAmbientUsage;
			}

			if(pPointForTask)
			{
				pTask = rage_new CTaskUseScenario(scenario, &scenarioPoint, iFlags);
			}
			else
			{
				pTask = rage_new CTaskUseScenario(scenario, vPedPos, fHeading, iFlags);
			}
			// Use the below call instead if we want to skip the intro clip and start in the current position (maybe add warp flag instead of current position
			// to make sure the ped ends up in the right coords)
			// pTask = rage_new CTaskUseScenario( scenario, pEffect, pAttachedEntity, CTaskUseScenario::SF_StartInCurrentPosition | CTaskUseScenario::SF_SkipEnterClip);
		}
		else if (pScenarioInfo->GetIsClass<CScenarioFleeInfo>())
		{
			Vector3 vSpawnOffset = pScenarioInfo->GetSpawnPropOffset();
			if (scenarioPoint.GetEntity() && (vSpawnOffset.x != 0.0f || vSpawnOffset.y != 0.0f || vSpawnOffset.z != 0.0))
			{
				GetCreationParams(pScenarioInfo, scenarioPoint.GetEntity(), vPedPos, fHeading);
			}
			else
			{
				fHeading = SetupScenarioPointHeading(pScenarioInfo, scenarioPoint);
			}

			s32 iFlags = 0;

			if(bForAmbientUsage)
			{
				iFlags |= CTaskUseScenario::SF_FlagsForAmbientUsage;
			}

			if(pPointForTask)
			{
				pTask = rage_new CTaskUseScenario(scenario, &scenarioPoint, iFlags);
			}
			else
			{
				pTask = rage_new CTaskUseScenario(scenario, vPedPos, fHeading, iFlags);
			}
		}
		// scenarios where peds move around between points
		else if (pScenarioInfo->GetIsClass<CScenarioMoveBetweenInfo>())
		{
			vPedPos = VEC3V_TO_VECTOR3(scenarioPoint.GetPosition());
			fHeading = SetupScenarioPointHeading(pScenarioInfo, scenarioPoint);
			pTask = rage_new CTaskMoveBetweenPointsScenario(scenario, pPointForTask);
		}
		else if (pScenarioInfo->GetIsClass<CScenarioJoggingInfo>() || pScenarioInfo->GetIsClass<CScenarioWanderingInfo>())
		{
			vPedPos = VEC3V_TO_VECTOR3(scenarioPoint.GetPosition());
			fHeading = SetupScenarioPointHeading(pScenarioInfo, scenarioPoint);
			pTask = rage_new CTaskWanderingScenario(scenario);
		}
		else if (pScenarioInfo->GetIsClass<CScenarioWanderingInRadiusInfo>())
		{
			vPedPos = VEC3V_TO_VECTOR3(scenarioPoint.GetPosition());
			fHeading = SetupScenarioPointHeading(pScenarioInfo, scenarioPoint);
			pTask = rage_new CTaskWanderingInRadiusScenario(scenario, pPointForTask);
		}
		else if (pScenarioInfo->GetIsClass<CScenarioDeadPedInfo>())
		{
			vPedPos = VEC3V_TO_VECTOR3(scenarioPoint.GetPosition());
			fHeading = SetupScenarioPointHeading(pScenarioInfo, scenarioPoint);
			pTask = rage_new CTaskDeadBodyScenario(scenario, pPointForTask);
		}
#if 0
		else if (pScenarioInfo->GetIsClass<CScenarioSkiingInfo>())
		{
			vPedPos = pEffect->GetWorldPosition(pAttachedEntity);
			fHeading = SetupScenarioPointHeading(pScenarioInfo,pEffect,pAttachedEntity);
			pTask = rage_new  CTaskControlScenario(scenario, NULL, pAttachedEntity, true, true, true, ST_Skiing);
		}
		else if (pScenarioInfo->GetIsClass<CScenarioControlAmbientInfo>())
		{
			vPedPos = pEffect->GetWorldPosition(pAttachedEntity);
			fHeading = SetupScenarioPointHeading(pScenarioInfo,pEffect,pAttachedEntity);
			pTask = rage_new CTaskControlScenario(scenario, NULL, pAttachedEntity, true, true);
		}
#endif
		else
		{
			aiAssertf(0, "No scenario task for scenario (%s)", CScenarioManager::GetScenarioName(scenario) );
		}
	}

	if (pTask)
	{
		// Flag to attempt to create a prop on construction
		pTask->CreateProp();
	}
	return pTask;
}


//-------------------------------------------------------------------------
// Get the heading from the spawn point, unless flagged not to
//-------------------------------------------------------------------------
float CScenarioManager::SetupScenarioPointHeading( const CScenarioInfo* pScenarioInfo, CScenarioPoint& scenarioPoint )
{
	if ( pScenarioInfo->GetIsFlagSet(CScenarioInfoFlags::IgnoreScenarioPointHeading) )
	{
		return fwRandom::GetRandomNumberInRange(-PI, PI);
	}

	Vector3 vDir = VEC3V_TO_VECTOR3(scenarioPoint.GetDirection());
	return rage::Atan2f(-vDir.x, vDir.y);
}


//-------------------------------------------------------------------------
// Randomly Creates peds and scenarios to the scenariopoint passed
//-------------------------------------------------------------------------
s32 CScenarioManager::GiveScenarioToScenarioPoint(CScenarioPoint& scenarioPoint, float maxFrustumDistToCheck, bool allowDeepInteriors
		, s32 iMaxPeds, s32 iSpawnOptions, int scenarioTypeIndexWithinGroup, CObject* pPropInEnvironment
		, const CollisionCheckParams* pCollisionCheckParams
		, const PedTypeForScenario* predeterminedPedType
		, CScenarioPointChainUseInfo* chainUserInfo)
{
	// Checked elsewhere, but trying to narrow down how this mysteriously is reported to fail sometimes:
	Assert(((iSpawnOptions & CPedPopulation::SF_ignoreScenarioHistory) != 0) || !scenarioPoint.HasHistory());

	if(!CheckScenarioPointEnabled(scenarioPoint, scenarioTypeIndexWithinGroup))
	{
		return 0;
	}

	if( !CheckScenarioPointTimesAndProbability(scenarioPoint, true, true, scenarioTypeIndexWithinGroup) )
	{
		return 0;
	}
	// Set up the scenario information
	Vector3 vSpawnPos;
	float fHeading = 0.0f;
	s32 iNumScenariosSpawned = 0;

	s32 scenario = (s32)scenarioPoint.GetScenarioTypeReal(scenarioTypeIndexWithinGroup);

	const CScenarioInfo* pScenarioInfo = GetScenarioInfo(scenario);

	if (aiVerifyf(pScenarioInfo,"No scenario info for spawn point"))
	{
		if (!pScenarioInfo->GetIsClass<CScenarioGroupInfo>() &&
			!pScenarioInfo->GetIsClass<CScenarioCoupleInfo>() )
		{
			CTask* pScenarioTask = SetupScenarioAndTask( scenario, vSpawnPos, fHeading, scenarioPoint, 0, false, true, true);

			// Create the ped
			if( pScenarioTask )
			{
				if(pPropInEnvironment && pScenarioTask->GetTaskType() == CTaskTypes::TASK_USE_SCENARIO)
				{
					static_cast<CTaskUseScenario*>(pScenarioTask)->SetPropInEnvironmentToUse(pPropInEnvironment);
				}

				// Checked elsewhere, but trying to narrow down how this mysteriously is reported to fail sometimes:
				Assert(((iSpawnOptions & CPedPopulation::SF_ignoreScenarioHistory) != 0) || !scenarioPoint.HasHistory());

				CPed* pPed = CreateScenarioPed( scenario, scenarioPoint, pScenarioTask, vSpawnPos, maxFrustumDistToCheck, allowDeepInteriors, fHeading, iSpawnOptions, 0, false, pCollisionCheckParams, predeterminedPedType, false, chainUserInfo);
				iNumScenariosSpawned = 0;
				if (pPed)
				{
					if (chainUserInfo)
					{
						//get the task we were setup with ... "this should be task_unalerted"
						// Note: this probably doesn't work properly in the CTaskPolice case.
						CTask* task = pPed->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_UNALERTED);
						if (task)
						{
							CTaskUnalerted* tua = (CTaskUnalerted*)task;
							CScenarioChainingGraph::UpdateDummyChainUser(*chainUserInfo, pPed, NULL);
							tua->SetScenarioChainUserInfo(chainUserInfo);
						}
						else
						{
							if(chainUserInfo->GetNumRefsInSpawnHistory())
							{
								// If we are in the history, we can't just call UnregisterPointChainUser(). Instead,
								// we turn it into a non-dummy user with the ped and vehicle pointers as NULL.
								// It will get cleaned up once there is no longer a reference from the history.
								// Note that in this case, if it got added to the history the ped would probably
								// actually exist, so I don't think it's the case that we need to remove it from the
								// history or anything.
								CScenarioChainingGraph::UpdateDummyChainUser(*chainUserInfo, NULL, NULL);
							}
							else
							{
								CScenarioChainingGraph::UnregisterPointChainUser(*chainUserInfo);
							}
						}
					}
					iNumScenariosSpawned = 1;
				}
			}
		}
		else
		{
			// Normally we pass this pCollisionCheckParams thing on to CreateScenarioPed(), but for couples/groups,
			// we may want to just carry out the test now, for simplicity.
			if(pCollisionCheckParams)
			{
				if(!PerformCollisionChecksForScenarioPoint(*pCollisionCheckParams))
				{
					return 0;
				}
			}

			// Couple scenario
			if ( pScenarioInfo->GetIsClass<CScenarioCoupleInfo>())
			{
				iNumScenariosSpawned = SetupCoupleScenario(scenarioPoint, maxFrustumDistToCheck, allowDeepInteriors, iSpawnOptions, scenario);
			}
			else
			{
				// Must be a group scenario
				iNumScenariosSpawned = SetupGroupScenarioPoint( scenario, scenarioPoint, iMaxPeds, iSpawnOptions );
			}
		}
	}

	// If any were spawned, mark the last spawn time
	if( iNumScenariosSpawned > 0 )
	{
		CScenarioInfo* pScenarioInfo = SCENARIOINFOMGR.GetScenarioInfoByIndex(scenario);
		pScenarioInfo->SetLastSpawnTime(fwTimer::GetTimeInMilliseconds());
	}
	return iNumScenariosSpawned;
}


bool CScenarioManager::PerformCollisionChecksForScenarioPoint(const CollisionCheckParams& params)
{
	static const int kMaxCollidedObjects = 6;

	CScenarioPoint& scenarioPoint = *params.m_pScenarioPoint;
	int scenarioTypeReal = params.m_ScenarioTypeReal;
	Vec3V_In scenarioPosIn = params.m_ScenarioPos;
	CEntity* pAttachedEntity = params.m_pAttachedEntity;
	CTrain* pAttachedTrain = params.m_pAttachedTrain;
	CObject* pPropInEnvironment = params.m_pPropInEnvironment;

	const Vec3V scenarioPosV = scenarioPosIn;

	bool checkMaxInRange = !scenarioPoint.IsFlagSet(CScenarioPointFlags::IgnoreMaxInRange);
	if(!CheckScenarioPopulationCount(scenarioTypeReal, RCC_VECTOR3(scenarioPosV), CPedPopulation::GetClosestAllowedScenarioSpawnDist(), NULL, checkMaxInRange))
	{
		BANK_ONLY(CPedPopulation::ms_populationFailureData.RegisterSpawnFailure(CPedPopulation::PedPopulationFailureData::ST_Scenario, CPedPopulation::PedPopulationFailureData::FR_collisionChecks, RCC_VECTOR3(scenarioPosV)));
		return false;
	}

	bool bValidPosition = true;
	// Check this position is free of objects (ignoring non-uprooted and this object)
	CEntity* pBlockingEntities[kMaxCollidedObjects] = {NULL, NULL, NULL, NULL, NULL, NULL};
	const bool bUseBoundingBoxes  = true;
	const bool bConsiderVehicles	= true;
	const bool bConsiderPeds			= false; // Don't check for Peds, as we've already checked them in CheckScenarioPopulationCount [11/28/2012 mdawe]
	const bool bConsiderObjects		= true;
	if(!CPedPlacement::IsPositionClearForPed(RCC_VECTOR3(scenarioPosV), PED_HUMAN_RADIUS, bUseBoundingBoxes, kMaxCollidedObjects, pBlockingEntities,	bConsiderVehicles, bConsiderPeds, bConsiderObjects))
	{
		for(s32 i = 0; (pBlockingEntities[i] != NULL) && (i < kMaxCollidedObjects); ++i)
		{
			// Ignore this entity
			if(pBlockingEntities[i] == pAttachedEntity)
			{
				continue;
			}
			//Ignore previous/next train carriages (they are very close together, to support linking).
			if(pAttachedTrain && ((pBlockingEntities[i] == pAttachedTrain->GetLinkedToBackward()) || (pBlockingEntities[i] == pAttachedTrain->GetLinkedToForward())))
			{
				continue;
			}
			if(pBlockingEntities[i] == pPropInEnvironment)
			{
				// This is the prop we intend to use, so we shouldn't consider that one to be blocking us.
				continue;
			}
			//can only ignore objects
			if(!pBlockingEntities[i]->GetIsTypeObject())
			{
				bValidPosition = false;
				break;
			}
			// Ignore doors, for qnan matrix problem
			if(((CObject*)pBlockingEntities[i])->IsADoor())
			{
				continue;
			}
			// Ignore non-uprooted objects
			if(((CObject*)pBlockingEntities[i])->m_nObjectFlags.bHasBeenUprooted)
			{
				bValidPosition = false;
				break;
			}
		}
	}
	if(!bValidPosition)
	{
		popDebugf3("Failed to spawn scenario ped, spawn model would collid with object");
		BANK_ONLY(CPedPopulation::ms_populationFailureData.RegisterSpawnFailure(CPedPopulation::PedPopulationFailureData::ST_Scenario, CPedPopulation::PedPopulationFailureData::FR_spawnCollisionFail, RCC_VECTOR3(scenarioPosV)));
		return false;
	}

	return true;
}

float CScenarioManager::GetVehicleScenarioDistScaleFactorForModel(fwModelId vehicleModelId, bool aerial)
{
	// MAGIC! When spawning at aerial points, we generally tend to be more visible since it stands out
	// more from the background (sky).
	float scale = aerial ? 1.5f : 1.0f;

	const CBaseModelInfo* pBaseInfo = CModelInfo::GetBaseModelInfo(vehicleModelId);
	if(pBaseInfo && popVerifyf(pBaseInfo->GetModelType() == MI_TYPE_VEHICLE, "Expected vehicle model."))
	{
		const CVehicleModelInfo* pModelInfo = static_cast<const CVehicleModelInfo*>(pBaseInfo);
		scale *= pModelInfo->GetVisibleSpawnDistScale();
	}
	return scale;
}

//-------------------------------------------------------------------------
// Similar to CVehicleModelInfo::GetRandomDriver() except it uses a filter to determine valid ped model indices
//-------------------------------------------------------------------------
u32 CScenarioManager::GetRandomDriverModelIndex(fwModelId vehicleModelId, const CAmbientModelSetFilter* pModelFilter)
{
	Assert(pModelFilter);
	const int MAX_NUM_DRIVERS_IN_VEHICLE = 128;
	atRangeArray<u32, MAX_NUM_DRIVERS_IN_VEHICLE> driverIndices; // valid indices go into one array, random selection later (if needed)
	u32 numDriverIndices = 0;
	CVehicleModelInfo* pVehicleModelInfo = static_cast<CVehicleModelInfo *>(CModelInfo::GetBaseModelInfo(vehicleModelId));
	Assert(pVehicleModelInfo);
	if (pVehicleModelInfo)
	{
		// we try to pick the valid model with the least amount of occurrences
		u32 iDriverCount = pVehicleModelInfo->GetDriverCount();
		s32 iSmallestOccurrence = 999;
		s32 iSmallestOccurrenceLoaded = 999;
		s32 iLargestOccurrence = 0;
		fwModelId spawnDriverId;
		fwModelId spawnDriverLoadedId;
		for (u32 i = 0; i < iDriverCount; ++i)
		{
			u32	iDriverModelHash = pVehicleModelInfo->GetDriverModelHash(i);
			fwModelId driverModelId;
			CPedModelInfo *pDriverModelInfo = static_cast<CPedModelInfo *>(CModelInfo::GetBaseModelInfoFromName(iDriverModelHash, &driverModelId));
			Assert(pDriverModelInfo);
			if (pModelFilter->Match(iDriverModelHash))
			{
				s32 occ = pDriverModelInfo->GetNumPedModelRefs() + 1;
				driverIndices[numDriverIndices++] = driverModelId.GetModelIndex();

				if (occ < iSmallestOccurrence)
				{
					iSmallestOccurrence = occ;
					spawnDriverId = driverModelId;
				}

				if (occ > iLargestOccurrence)
				{
					iLargestOccurrence = occ;
				}

				if (CModelInfo::HaveAssetsLoaded(driverModelId) && occ < iSmallestOccurrenceLoaded)
				{
					iSmallestOccurrence = occ;
					spawnDriverLoadedId = driverModelId;
				}
			}
		}

		if (!spawnDriverLoadedId.IsValid())
		{
			if (iSmallestOccurrence == 1 && iLargestOccurrence == 1 && iDriverCount > 1)	
			{
				s32 iRandIndex = fwRandom::GetRandomNumberInRange(0, numDriverIndices);
				spawnDriverId.SetModelIndex(driverIndices[iRandIndex]);
				Assert(CModelInfo::GetBaseModelInfo(spawnDriverId));
			}
		}
		else
		{
			spawnDriverId = spawnDriverLoadedId;
		}
		return spawnDriverId.GetModelIndex();
	}
	return fwModelId::MI_INVALID;
}

//-------------------------------------------------------------------------
// Returns a ped type and model index for the scenario type passed
//-------------------------------------------------------------------------
bool CScenarioManager::GetPedTypeForScenario( s32 scenarioType, const CScenarioPoint& scenarioPoint, strLocalIndex& newPedModelIndex, const Vector3& vPos, s32 iDesiredAudioContexts,
		const CAmbientModelVariations** variationsOut, int pedIndexWithinScenario, fwModelId vehicleModelToPickDriverFor)
{
	// Call SelectPedTypeForScenario() to find a ped model to use, which may or may not be resident already.
	PedTypeForScenario tempPedType;
	if(SelectPedTypeForScenario(tempPedType, scenarioType, scenarioPoint, vPos, iDesiredAudioContexts,
			pedIndexWithinScenario, vehicleModelToPickDriverFor, variationsOut != NULL))
	{
		// If this ped model still needs to be streamed, carry out the streaming request and return false
		// (meaning that we don't have a model we can use yet).
		if(tempPedType.m_NeedsToBeStreamed)
		{
			StreamScenarioPeds(fwModelId(strLocalIndex(tempPedType.m_PedModelIndex)), scenarioPoint.IsHighPriority(), scenarioType);
			return false;
		}

		// We have a model that should be resident, return it.
		newPedModelIndex = tempPedType.m_PedModelIndex;
		if(variationsOut)
		{
			*variationsOut = tempPedType.m_Variations;
		}
		return true;
	}
	else
	{
		// Couldn't find any model to use.
		newPedModelIndex = fwModelId::MI_INVALID;
		if(variationsOut)
		{
			*variationsOut = NULL;
		}
		return false;
	}
}


bool CScenarioManager::SelectPedTypeForScenario(PedTypeForScenario& pedTypeOut,
		s32 scenarioType, const CScenarioPoint& scenarioPoint, const Vector3& vPos, s32 iDesiredAudioContexts,
		int pedIndexWithinScenario, fwModelId vehicleModelToPickDriverFor,
		bool needVariations, bool bAllowFallbackPed)
{
	PF_FUNC(GetPedTypeForScenario);
	CAmbientModelSetFilterForScenario modelsFilter;
	pedTypeOut.m_PedModelIndex = fwModelId::MI_INVALID;
	pedTypeOut.m_Variations = NULL;
	pedTypeOut.m_NeedsToBeStreamed = false;

	strLocalIndex newPedModelIndex = strLocalIndex(fwModelId::MI_INVALID);

	const CAmbientModelVariations** variationsOut = NULL;
	if(needVariations)
	{
		variationsOut = &pedTypeOut.m_Variations;
	}

	bool bForceLoad = false;

	const bool highPri = scenarioPoint.IsHighPriority();

	//const bool bPedInInterior = pAttachedEntity && pAttachedEntity->m_nFlags.bInMloRoom;

	const CScenarioInfo* pScenarioInfo = CScenarioManager::GetScenarioInfo(scenarioType);

	if( iDesiredAudioContexts == 0 && pScenarioInfo->GetIsClass<CScenarioGroupInfo>() )
	{
		iDesiredAudioContexts = AUD_CONVERSATION_TOPIC_SMOKER|AUD_CONVERSATION_TOPIC_BUSINESS|AUD_CONVERSATION_TOPIC_BUM|AUD_CONVERSATION_TOPIC_CONSTRUCTION|AUD_CONVERSATION_TOPIC_GANG;
	}

	unsigned iModelSetIndex = CScenarioPointManager::kNoCustomModelSet;
	// Use CScenarioPoint::GetModelSetIndex() as an override only if it's not a vehicle scenario, if it is,
	// this is an override on the vehicle model which we shouldn't apply to the peds.
	if(!pScenarioInfo->GetIsClass<CScenarioVehicleInfo>())
	{
		iModelSetIndex = scenarioPoint.GetModelSetIndex();
	}
	
	if (pScenarioInfo->GetIsFlagSet(CScenarioInfoFlags::SpawnMalePedsOnly))
	{
		Assertf(!pScenarioInfo->GetIsFlagSet(CScenarioInfoFlags::SpawnFemalePedsOnly), "CScenarioInfoFlags::SpawnMalePedsOnly is set and so is CScenarioInfoFlags::SpawnFemalePedsOnly for scenario (%s) this seems wrong.", pScenarioInfo->GetName());
		modelsFilter.m_RequiredGender = GENDER_MALE;
	}
	else if (pScenarioInfo->GetIsFlagSet(CScenarioInfoFlags::SpawnFemalePedsOnly))
	{
		Assertf(!pScenarioInfo->GetIsFlagSet(CScenarioInfoFlags::SpawnMalePedsOnly), "CScenarioInfoFlags::SpawnMalePedsOnly is set and so is CScenarioInfoFlags::SpawnFemalePedsOnly for scenario (%s) this seems wrong.", pScenarioInfo->GetName());
		modelsFilter.m_RequiredGender = GENDER_FEMALE;
	}
#if __ASSERT
	bool bIsVirtual = SCENARIOINFOMGR.IsVirtualIndex(scenarioPoint.GetScenarioTypeVirtualOrReal());
	if (bIsVirtual)
	{
		modelsFilter.m_bIgnoreGenderFailure = true;
	}
#endif 


	if(pScenarioInfo->GetIsClass<CScenarioCoupleInfo>())
	{
		//const CScenarioCoupleInfo* pCoupleInfo = static_cast<const CScenarioCoupleInfo*>(pScenarioInfo);
		if(pedIndexWithinScenario == 0)
		{
			modelsFilter.m_RequiredGender = GENDER_MALE;
		}
		else
		{
			modelsFilter.m_RequiredGender = GENDER_FEMALE;
		}
	}

	if( iModelSetIndex == CScenarioPointManager::kNoCustomModelSet && !pScenarioInfo->GetModelSet() )
	{
		//Check if we should choose our model from the vehicle drivers set.
		if( pScenarioInfo->GetIsFlagSet(CScenarioInfoFlags::ChooseModelFromDriversWhenAttachedToVehicle))
		{
			// If this is a vehicle, and we're looking in the boot, use the pedtype specified
			if( scenarioPoint.GetEntity() && scenarioPoint.GetEntity()->GetIsTypeVehicle() )
			{
				newPedModelIndex = GetRandomDriverModelIndex(scenarioPoint.GetEntity()->GetModelId(), &modelsFilter);
				fwModelId newPedModelId(newPedModelIndex);
				if( newPedModelId.IsValid())
				{
					if(CModelInfo::HaveAssetsLoaded(newPedModelId))
					{
						pedTypeOut.m_PedModelIndex = newPedModelIndex.Get();

						return true;
					}
					return false;
				}
			}
		}
	}

	// Get the model set we are trying to use, either from the scenario point or from the scenario info.
	const CAmbientPedModelSet* pModelSet = NULL;
	if(iModelSetIndex != CScenarioPointManager::kNoCustomModelSet)
	{
		pModelSet = CAmbientModelSetManager::GetInstance().GetPedModelSet(iModelSetIndex);
		bForceLoad = true;
		aiAssert(pModelSet && pModelSet->GetNumModels() > 0);
	}
	else
	{
		pModelSet = pScenarioInfo->GetModelSet();
		bForceLoad = pModelSet && pScenarioInfo->GetIsFlagSet(CScenarioInfoFlags::ForceModel);
	}

#if __ASSERT
	// Iterate over the models in the set to make sure they are valid.
	// This will let us know if something bad is about to happen down the line when this modelset is actually used.
	if (pModelSet)
	{
		for(int i=0; i < pModelSet->GetNumModels(); i++)
		{
			fwModelId testId(pModelSet->GetModelIndex(i));
			Assertf(testId.IsValid(), "Invalid model contained within modelset %s", pModelSet->GetName());
		}
	}
#endif

	// set up our blocked modelsets array
	const CAmbientPedModelSet* pBlockedModelSetsArrary[kMaxBlockedModelSets] = {NULL};
	int iBlockedModelSetIndex = 0;
	// will add the scenario info blocked models if needed
	Assert(pScenarioInfo);
	bool bBlockBulkyItems = false;
	GatherBlockedModelSetsFromChainPt(scenarioPoint, *pScenarioInfo, pBlockedModelSetsArrary, iBlockedModelSetIndex, bBlockBulkyItems);

	// Setup our blocked models set filter
	modelsFilter.m_BlockedModels = pBlockedModelSetsArrary;
	modelsFilter.m_NumBlockedModelSets = iBlockedModelSetIndex;
	modelsFilter.m_bProhibitBulkyItems = bBlockBulkyItems;

	pedTypeOut.m_NoBulkyItems = bBlockBulkyItems;

	// If we don't have a model set from the scenario type or the point, check to see if we're trying
	// to spawn a driver for a vehicle, which could have specific drivers that must be used.
	const CVehicleModelInfo* pDriverForVehicleModelInfo = NULL;
	bool mayNeedBikeHelmet = false;
	if(!pModelSet && vehicleModelToPickDriverFor.IsValid())
	{
		// Get the vehicle model info.
		const CBaseModelInfo* mi = CModelInfo::GetBaseModelInfo(vehicleModelToPickDriverFor);
		if(mi && aiVerifyf(mi->GetModelType() == MI_TYPE_VEHICLE, "Model %s is not a vehicle.", mi->GetModelName()))
		{
			pDriverForVehicleModelInfo = static_cast<const CVehicleModelInfo*>(mi);

			if(modelsFilter.m_RequiredGender == GENDER_DONTCARE)
			{
				// If we don't already have a gender set, check if there is one specified
				// in the vehicle type.
				if(pDriverForVehicleModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_DRIVER_SHOULD_BE_MALE))
				{
					modelsFilter.m_RequiredGender = GENDER_MALE;
				}
				else if(pDriverForVehicleModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_DRIVER_SHOULD_BE_FEMALE))
				{
					modelsFilter.m_RequiredGender = GENDER_FEMALE;
				}
			}

			// In AddPedInCar(), if the vehicle is a bike there is a check to make
			// sure the ped has a helmet, or is allowed to ride bikes without helmets.
			// In that case, we need to pick one that satisfies that condition
			// (if we are going to grab one from the general population).
			if(pDriverForVehicleModelInfo->GetVehicleType() == VEHICLE_TYPE_BIKE)
			{
				mayNeedBikeHelmet = true;
			}

			if(!pDriverForVehicleModelInfo->GetDriverCount())
			{
				// This vehicle model doesn't have specific drivers listed, so go ahead and try to pick
				// a model like for any scenario.
				pDriverForVehicleModelInfo = NULL;
			}
			else
			{
				// We do have specific drivers listed. Allow force-loading either if the ForceModel flag is set,
				// or if there is a model override on the point - this would probably be a vehicle model override,
				// but if we intend to spawn a specific type of vehicle, we will need the driver to match.
				bForceLoad = pScenarioInfo->GetIsFlagSet(CScenarioInfoFlags::ForceModel) || (scenarioPoint.GetModelSetIndex() != CScenarioPointManager::kNoCustomModelSet);
				if(!bForceLoad)
				{
					// If we're not going to force-load, we can just use the existing GetRandomDriver() function
					// and check if the model is streamed in (GetRandomDriver() should have picked one that was streamed in,
					// if possible).
					newPedModelIndex = GetRandomDriverModelIndex(vehicleModelToPickDriverFor, &modelsFilter);
					fwModelId newPedModelId(newPedModelIndex);
					if(newPedModelId.IsValid())
					{
						if(CModelInfo::HaveAssetsLoaded(newPedModelId))
						{
							pedTypeOut.m_PedModelIndex = newPedModelIndex.Get();
							return true;
						}
						return false;
					}
				}
			}
		}
	}

	CPedPopulation::ChooseCivilianPedModelIndexArgs args;
	args.m_BlockedModelSetsArray = pBlockedModelSetsArrary;
	args.m_NumBlockedModelSets = iBlockedModelSetIndex;
	args.m_AvoidNearbyModels = true;
	args.m_CreationCoors = &vPos;
	args.m_SpeechContextRequirement = iDesiredAudioContexts;
	args.m_RequiredPopGroupFlags = POPGROUP_SCENARIO;
	args.m_RequiredModels = pModelSet;
	args.m_PedModelVariationsOut = variationsOut;
	args.m_RequiredGender = modelsFilter.m_RequiredGender;
	args.m_MayRequireBikeHelmet = mayNeedBikeHelmet;
	args.m_ConditionalAnims = &(pScenarioInfo->GetConditionalAnimsGroup());
	args.m_ProhibitBulkyItems = modelsFilter.m_bProhibitBulkyItems;
	args.m_SortByPosition = true;

	modelsFilter.m_ConditionalAnims = args.m_ConditionalAnims;

	// The ChooseCivilianPedModelIndex() function isn't set up to deal with specific vehicle driver models,
	// but we want to go through it if we have a model set or if we're not picking a driver.
	if(!pDriverForVehicleModelInfo)
	{
		// Create a random ped, trying to avoid model duplications
		newPedModelIndex = CPedPopulation::ChooseCivilianPedModelIndex(args);
		if (CModelInfo::IsValidModelInfo(newPedModelIndex.Get()) )
		{
			pedTypeOut.m_PedModelIndex = newPedModelIndex.Get();

			return true;
		}

		// If we couldn't find a model satisfying the uniqueness requirements, we relax the
		// requirements and try again. If we are allowed to force-load, we probably shouldn't do this
		// as it may drastically reduce the amount of variation (for example, if the model set contains
		// both a male and a female ped type, we may end up never streaming in the females). The exception
		// is if this is a high priority point, if so, we should probably always try to spawn with what we've
		// got in memory.
		if(!bForceLoad || highPri)
		{
			// Can't find a unique model. so try and create any other and hope the ped variation stuff will make it look ok
			args.m_AvoidNearbyModels = false;
			newPedModelIndex = CPedPopulation::ChooseCivilianPedModelIndex(args);
			if ( CModelInfo::IsValidModelInfo(newPedModelIndex.Get()) )
			{
				pedTypeOut.m_PedModelIndex = newPedModelIndex.Get();
				return true;
			}

			// Only do this if we had a speech context requirement in the first place, otherwise
			// we'll just spend CPU time for no reason.
			if(args.m_SpeechContextRequirement)
			{
				// Can't find any with a speech requirement, take any
				args.m_SpeechContextRequirement = 0;
				newPedModelIndex = CPedPopulation::ChooseCivilianPedModelIndex(args);
				if ( CModelInfo::IsValidModelInfo(newPedModelIndex.Get()) )
				{
					pedTypeOut.m_PedModelIndex = newPedModelIndex.Get();
					return true;
				}
			}
		}
	}

	// If we are allowed to force-load, and have either a model set or
	// a vehicle with specific drivers, try to determine what to stream in.
	if(bForceLoad && (pModelSet || pDriverForVehicleModelInfo))
	{
		// This is fairly arbitrary, just determines how much stack space we reserve.
		static const int kMaxModelsPerSet = 128;

		// Since we can operate either on a model set or on a vehicle with specific
		// drivers, we build a temporary array of the models we can use.
		int iNumModels = 0;
		atRangeArray<u32, kMaxModelsPerSet> modelIndices;
		atRangeArray<u32, kMaxModelsPerSet> modelHashes;
		if(pModelSet)
		{
			aiAssert(pModelSet->GetNumModels() <= kMaxModelsPerSet);
			iNumModels = Min((int)pModelSet->GetNumModels(), kMaxModelsPerSet);
			for(int i = 0; i < iNumModels; i++)
			{
				modelIndices[i] = pModelSet->GetModelIndex(i).Get();
				modelHashes[i] = pModelSet->GetModelHash(i);
			}
		}
		else
		{
			aiAssert(pDriverForVehicleModelInfo->GetDriverCount() <= kMaxModelsPerSet);
			iNumModels = Min((int)pDriverForVehicleModelInfo->GetDriverCount(), kMaxModelsPerSet);
			for(int i = 0; i < iNumModels; i++)
			{
				modelIndices[i] = pDriverForVehicleModelInfo->GetDriver(i);
				modelHashes[i] = pDriverForVehicleModelInfo->GetDriverModelHash(i);
			}
		}

		// Try to find a model already streamed in, already requested, or in memory and marked for deletion
		// If one already exists.
		int iCurrentModelPriority = 0;

		//Create the condition data - will be needed to determine which ped type to stream in.
		CScenarioCondition::sScenarioConditionData conditionData;

		for( int iModel = 0; iModel < iNumModels; iModel++ )
		{
			u32 testPedModelIndex = modelIndices[iModel];
			u32 testPedModelHash = modelHashes[iModel];
			if( CModelInfo::IsValidModelInfo(testPedModelIndex) )
			{
				int iTestModelPriority = 0;
				fwModelId newPedModelId((strLocalIndex(testPedModelIndex)));

				if(!CPedPopulation::CheckPedModelGenderAndBulkyItems(newPedModelId, args.m_RequiredGender, modelsFilter.m_bProhibitBulkyItems))
				{
					continue;
				}

				if (CScriptPeds::HasPedModelBeenRestrictedOrSuppressed(testPedModelIndex))
				{
					continue;
				}

				if (CScenarioManager::BlockedModelSetsArrayContainsModelHash(pBlockedModelSetsArrary, iBlockedModelSetIndex, testPedModelHash))
				{
					continue;
				}

				// We are deciding which model to stream in.  It only makes sense to stream in a model here
				// if the chosen model is going to match the scenario conditions.  So build a conditional anims struct
				// and test the model.
				conditionData.m_ModelId = newPedModelId;
				if (!args.m_ConditionalAnims->CheckForMatchingPopulationConditionalAnims(conditionData))
				{
					continue;
				}

				if( CModelInfo::HaveAssetsLoaded(newPedModelId) )
				{
					if( CModelInfo::GetAssetsAreDeletable(newPedModelId) )
					{
						iTestModelPriority = 2;
					}
					else
					{
						iTestModelPriority = 3;
					}
				}
				else if( CModelInfo::AreAssetsRequested(newPedModelId) )
				{
					iTestModelPriority = 1;
				}

				// If this model is deemed higher priority than one already selected, select it
				if( iTestModelPriority > 0 && iTestModelPriority > iCurrentModelPriority )
				{
					newPedModelIndex = testPedModelIndex;
					iCurrentModelPriority = iTestModelPriority;
					if(variationsOut && pModelSet)
					{
						*variationsOut = pModelSet->GetModelVariations(iModel);
					}
				}
			}
		}

		// Get a random model to stream in. Note: it's probably possible for models that are
		// actually streamed in that we didn't find them using ChooseCivilianPedModelIndex(),
		// if they are not in the AppropriateLoadedPeds set.
		if( newPedModelIndex == fwModelId::MI_INVALID)
		{
			if(args.m_RequiredGender != GENDER_DONTCARE)
			{
				modelsFilter.m_RequiredGender = args.m_RequiredGender;
			} 

			if(pModelSet)
			{
				const char* pDebugContext = NULL;
#if __ASSERT	
				pDebugContext = pScenarioInfo->GetName();
#endif
				newPedModelIndex = pModelSet->GetRandomModelIndex(variationsOut, &modelsFilter, pDebugContext); // come back to this...
			}
			else
			{
				newPedModelIndex = GetRandomDriverModelIndex(vehicleModelToPickDriverFor, &modelsFilter);
			}
		}
	}

	// Make sure the ped model is loaded, and if not use a fall back model.
	const bool bBikesOnly   = false;
	const bool bDriversOnly = false;
	if(bAllowFallbackPed && !CModelInfo::IsValidModelInfo(newPedModelIndex.Get()) && gPopStreaming.IsFallbackPedAvailable(bBikesOnly, bDriversOnly))
	{
		newPedModelIndex = gPopStreaming.GetFallbackPedModelIndex(bDriversOnly);
	}

	if( !CModelInfo::IsValidModelInfo(newPedModelIndex.Get()) )
	{
		return false;
	}

	if( CScenarioManager::BlockedModelSetsArrayContainsModelHash(pBlockedModelSetsArrary, iBlockedModelSetIndex, CModelInfo::GetBaseModelInfo(fwModelId(newPedModelIndex))->GetHashKey() ) )
	{
		return false;
	}

	fwModelId newPedModelId(newPedModelIndex);
	if(CModelInfo::HaveAssetsLoaded(newPedModelId) &&
		!CModelInfo::GetAssetsAreDeletable(newPedModelId))// This line checks if the model is one we are trying to stream out.
	{
		pedTypeOut.m_PedModelIndex = newPedModelIndex.Get();

		return true;
	}
	else
	{
		if( bForceLoad && !CScriptPeds::HasPedModelBeenRestrictedOrSuppressed(newPedModelIndex.Get()))
		{
			pedTypeOut.m_PedModelIndex = newPedModelIndex.Get();
			pedTypeOut.m_NeedsToBeStreamed = true;

			return true;
		}
		return false;
	}
}

void CScenarioManager::GatherBlockedModelSetsFromChain(const CScenarioChainingGraph& graph, int iChainIndex, const CAmbientPedModelSet** pBlockedModelSetsArray, int& iNumBlockedModelSets, bool& bBlockBulkyItems, int iNumEdgesToCheck)
{
	// cache our chain
	const CScenarioChain& chain = graph.GetChain(iChainIndex);

	// determine how many edges we will check
	const int iNumEdges = chain.GetNumEdges();

	// if there are more edges that we want to check for
	if (iNumEdges > iNumEdgesToCheck)
	{
		// don't gather blocked modelsets, since the edges/nodes are not in any order
		return;
	}

	// loop over all the edges of this chain
	for (int iEdgeIndex = 0; iEdgeIndex < iNumEdges; ++ iEdgeIndex)
	{
		// get the index of the to nodes
		const int iEdgeId = chain.GetEdgeId(iEdgeIndex);
		const CScenarioChainingEdge& edge = graph.GetEdge(iEdgeId);
		const int iNodeIndexTo = edge.GetNodeIndexTo();

		// if we don't have a point on this edge
		const CScenarioPoint* pScenarioPoint = graph.GetChainingNodeScenarioPoint(iNodeIndexTo);
		if (!pScenarioPoint)
		{
			// go to the next edge
			continue;
		}

		// if we have a virtual scenario type
		const int iScenarioType = pScenarioPoint->GetScenarioTypeVirtualOrReal();
		if(SCENARIOINFOMGR.IsVirtualIndex(iScenarioType))
		{
			// TODO:
			//		grab a random ScenarioTypeReal to possibly not have conflicts?

			// go to the next edge
			continue;
		}

		// check if our scenario info has a blocked modelset
		const CScenarioInfo* pInfo = SCENARIOINFOMGR.GetScenarioInfoByIndex(iScenarioType);
		if (pInfo)
		{
			bBlockBulkyItems |= pInfo->GetIsFlagSet(CScenarioInfoFlags::NoBulkyItems) || pInfo->GetIsClass<CScenarioVehicleInfo>();

			if (pScenarioPoint->GetCarGen())
			{
				CCarGenerator* pCarGen = pScenarioPoint->GetCarGen();
				if (pCarGen->HasPreassignedModel())
				{
					CVehicleModelInfo *pModelInfo = (CVehicleModelInfo *)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(pCarGen->GetVisibleModel())));
					if (pModelInfo && pModelInfo->GetIsBike())
					{
						// Allow bulky items on bikes/motorcycles
						bBlockBulkyItems = false; // Not a typo, allow bulky items for any point on chain if this is a bike
					}
				}
			}

			if (pInfo->GetBlockedModelSet())
			{
				// if we can't add more blocked modelsets
				if (!AddBlockedModelSetToArray(pBlockedModelSetsArray, iNumBlockedModelSets, pInfo->GetBlockedModelSet()))
				{
					return;
				}
			}
		}
	}
}

void CScenarioManager::GatherBlockedModelSetsFromChainPt( const CScenarioPoint &scenarioPoint, const CScenarioInfo& scenarioPointInfo, const CAmbientPedModelSet** pBlockedModelSetsArray, int& iNumBlockedModelSets, bool& bBlockBulkyItems, int iNumEdgesToCheck /*= 30*/ )
{
	// if our scenario point is chained
	if(scenarioPoint.IsChained())
	{
		// traverse the scenario chain for blocked models...

		// Note: this is potentially VERY slow, in that we loop over the active regions
		// and call FindChainingNodeIndexForScenarioPoint() (which is an O(n) operation).
		// THEN, we go through all the edges and their connected nodes to find any blocked
		// model sets.
		// I don't think it will be a big problem as long as we only do this once per scenario
		// chain, so long as they are not too massive...
		const int numRegs = SCENARIOPOINTMGR.GetNumActiveRegions();
		for(int i = 0; i < numRegs; i++)
		{
			// get our active region, graph, and current node index
			const CScenarioPointRegion& reg = SCENARIOPOINTMGR.GetActiveRegion(i);
			const CScenarioChainingGraph& graph = reg.GetChainingGraph();
			const int nodeIndex = graph.FindChainingNodeIndexForScenarioPoint(scenarioPoint);

			// if our scenarioPoint (nodeIndex) is not in this region
			if (nodeIndex < 0)
			{
				// go to the next region
				continue;
			}

			// get our chain index
			const int chainIndex = graph.GetNodesChainIndex(nodeIndex);

			// now grab all of our blocked modelsets
			GatherBlockedModelSetsFromChain(graph, chainIndex, pBlockedModelSetsArray, iNumBlockedModelSets, bBlockBulkyItems, iNumEdgesToCheck);
		}
	}
	
	if (scenarioPointInfo.GetBlockedModelSet())
	{
		AddBlockedModelSetToArray(pBlockedModelSetsArray, iNumBlockedModelSets, scenarioPointInfo.GetBlockedModelSet());
	}

	bBlockBulkyItems |= scenarioPointInfo.GetIsFlagSet(CScenarioInfoFlags::NoBulkyItems) || scenarioPointInfo.GetIsClass<CScenarioVehicleInfo>();
	
	if (scenarioPoint.GetCarGen())
	{
		CCarGenerator* pCarGen = scenarioPoint.GetCarGen();
		if (pCarGen->HasPreassignedModel())
		{
			CVehicleModelInfo *pModelInfo = (CVehicleModelInfo *)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(pCarGen->GetVisibleModel())));
			if (pModelInfo && pModelInfo->GetIsBike())
			{
				// Allow bulky items on bikes/motorcycles
				bBlockBulkyItems = false;  // Not a typo, allow bulky items for any point on chain if this is a bike
			}
		}
	}
}

bool CScenarioManager::BlockedModelSetsArrayContainsModelHash( const CAmbientPedModelSet ** pBlockedModelSetsArray, const int iNumBlockedModelSets, const u32 iModelNameHash )
{
	for (int i = 0; i < iNumBlockedModelSets; ++i)
	{
		Assert(pBlockedModelSetsArray[i]);
		if (pBlockedModelSetsArray[i]->GetContainsModel(iModelNameHash))
		{
			return true;
		}
	}
	return false;
}

bool CScenarioManager::AddBlockedModelSetToArray(const CAmbientPedModelSet** pBlockedModelSetsArray, int& iNumBlockedModelSets, const CAmbientPedModelSet* pNewBlockedModelSet)
{
	// if we can add the new blocked modelset to the array
	if (CanAddBlockedModelSetToArray(pBlockedModelSetsArray, iNumBlockedModelSets, pNewBlockedModelSet))
	{
		pBlockedModelSetsArray[iNumBlockedModelSets++] = pNewBlockedModelSet;
	}

	// if we reached the limit of the blocked modelset array
	if (iNumBlockedModelSets == kMaxBlockedModelSets)
	{
		// don't allow more modelsets to be added
		return false;
	}

	// we can add more modelsets
	return true;
}

bool CScenarioManager::CanAddBlockedModelSetToArray(const CAmbientPedModelSet** pBlockedModelSetsArray, const int iNumBlockedModelSets, const CAmbientPedModelSet* pNewBlockedModelSet)
{
	// if we already reached the limit of the blocked modelset array
	if (iNumBlockedModelSets == kMaxBlockedModelSets)
	{
		// don't add the new modelset
		return false;
	}
	// loop through the blocked modelset array
	for(int iIndex = 0; iIndex < iNumBlockedModelSets; ++iIndex)
	{
		// if the new blocked modelset is already in the array
		if (pBlockedModelSetsArray[iIndex]->GetHash() == pNewBlockedModelSet->GetHash())
		{
			// don't add the new modelset
			return false;
		}
	}
	// we can add the new modelset
	return true;
}

void GatherBlockedModelSets(const CScenarioChainingGraph& graph, atArray<const CAmbientPedModelSet*>& pBlockedModels, int nNextNodeIndex, int depth/*=4*/)
{
	// if our node index is valid
	if(depth >= 0 && nNextNodeIndex >= 0)
	{
		// if our chain index is valid
		const int chainIndex = graph.GetNodesChainIndex(nNextNodeIndex);
		if(chainIndex >= 0)
		{
			// for every edge connected to this node
			const int numEdges = graph.GetChain(chainIndex).GetNumEdges();
			for (int edgeIndex = 0; edgeIndex < numEdges; ++edgeIndex)
			{
				// find the next node along this edge and if it is valid
				const int connectedNodeIndex = graph.GetEdge(graph.GetChain(chainIndex).GetEdgeId(edgeIndex)).GetNodeIndexTo();
				if (connectedNodeIndex >= 0)
				{
					// get the scenario info associated with this node
					const CScenarioInfo* pInfo = SCENARIOINFOMGR.GetScenarioInfo(graph.GetNode(connectedNodeIndex).m_ScenarioType);

					// if we have a valid scenario info and it has a blocked modelset
					if(pInfo && pInfo->GetBlockedModelSet())
					{
						// add to our array of blocked modelsets
						pBlockedModels.PushAndGrow(pInfo->GetBlockedModelSet());
					}
					// gather the next level of blocked modelsets in this chain
					GatherBlockedModelSets(graph, pBlockedModels, connectedNodeIndex, depth - 1);
				}
			}
		}
	}
}

//-------------------------------------------------------------------------
// Creates a scenario ped with the task, position and heading
//-------------------------------------------------------------------------
CPed* CScenarioManager::CreateScenarioPed(const int realScenarioType
		, CScenarioPoint& scenarioPoint
		, CTask* pScenarioTask
		, Vector3& vPedRootPos
		, float fAddRangeInViewMin
		, bool allowDeepInteriors
		, float& fHeading
		, s32 iSpawnOptions
		, int pedIndexWithinScenario
		, bool findZCoorForPed
		, const CollisionCheckParams* pCollisionCheckParams
		, const PedTypeForScenario* predeterminedPedType
		, bool bAllowFallbackPed
		, CScenarioPointChainUseInfo* chainUserInfo)
{
#if __DEV
	Assert(rage::FPIsFinite(fHeading));
	Assert(rage::FPIsFinite(vPedRootPos.x) && rage::FPIsFinite(vPedRootPos.y) && rage::FPIsFinite(vPedRootPos.z));
	Assertf(fHeading >= -2.0f * PI && fHeading <= 2.0f * PI, "Invalid heading specified");
#endif

	PF_FUNC(CreateScenarioPed);
	PF_FUNC(CreateScenarioPedTotal);

	ScenarioSpawnParams params;
	if(!DetermineScenarioSpawnParameters(params, realScenarioType, RCC_VEC3V(vPedRootPos), scenarioPoint, pScenarioTask, fAddRangeInViewMin, allowDeepInteriors, pedIndexWithinScenario, findZCoorForPed, predeterminedPedType, bAllowFallbackPed))
	{
		delete pScenarioTask;
		return NULL;
	}

	PF_START(CreateScenarioPedPreSpawnSetup);

	CInteriorInst* pInteriorInst = params.m_pInteriorInst;
	const s32 iRoomIdFromCollision = params.m_iRoomIdFromCollision;
	bool bUseGroundToRootOffset = params.m_bUseGroundToRootOffset;

	// Update the interior state of the scenariopoint if this isn't an attached point
	CEntity* pAttachedEntity = scenarioPoint.GetEntity();
	
	if (pAttachedEntity && pAttachedEntity->GetIsTypeVehicle())
	{
		CVehicle* pVehicle = static_cast<CVehicle *>(pAttachedEntity);

		// Check to see if we're trying to create a ped on a train. Maybe we could check this when scheduling?? [4/20/2013 mdawe]
		// Also check if we are using pretend occupants, in which case we should never allow anybody to spawn
		// on the train, unless SF_addingTrainPassengers is set (for when we are about to turn the pretend occupants into real).
		if ((CNetwork::IsGameInProgress() && !CTrain::ms_bEnableTrainPassengersInMP)
				|| (!(iSpawnOptions & CPedPopulation::SF_addingTrainPassengers) && pVehicle->m_nVehicleFlags.bUsingPretendOccupants))
		{
			if(pVehicle->InheritsFromTrain())
			{
				delete pScenarioTask;
				PF_STOP(CreateScenarioPedPreSpawnSetup);
				return NULL;
			}
		}
	}

	const CScenarioInfo* pScenarioInfo = GetScenarioInfo(realScenarioType);
	if( pAttachedEntity == NULL && (scenarioPoint.IsInteriorState(CScenarioPoint::IS_Unknown) || iRoomIdFromCollision) )
	{
		if( iRoomIdFromCollision != 0 && pInteriorInst )
		{
			if(pInteriorInst->GetProxy()->GetCurrentState() < CInteriorProxy::PS_FULL)
			{
				delete pScenarioTask;
				PF_STOP(CreateScenarioPedPreSpawnSetup);
				return NULL;
			}

			scenarioPoint.SetInteriorState(CScenarioPoint::IS_Inside);
		}
		else
		{
			scenarioPoint.SetInteriorState(CScenarioPoint::IS_Outside);
		}
	}

	bool bSpawnOffsetApplied = false;
	Vector3 spawnOffset(0.0f, 0.0f, 0.0f);
	if (pScenarioInfo->GetIsClass<CScenarioPlayAnimsInfo>() )
	{
		spawnOffset = static_cast<const CScenarioPlayAnimsInfo *>(pScenarioInfo)->GetSpawnPointAdditionalOffset();
		if (!spawnOffset.IsZero() )
		{
			bSpawnOffsetApplied = true;
		}
	}

	if (!bSpawnOffsetApplied)
	{
		vPedRootPos = RCC_VECTOR3(params.m_GroundAdjustedPedRootPos);
	}
	else
	{
		vPedRootPos += spawnOffset;
		bUseGroundToRootOffset = false;
	}

	// If the scenario point is outside and its raining, dont spawn
	if( pAttachedEntity == NULL && scenarioPoint.IsInteriorState(CScenarioPoint::IS_Outside) )
	{
		bool bDontSpawnInRain = g_weather.IsRaining() && pScenarioInfo && pScenarioInfo->GetIsFlagSet(CScenarioInfoFlags::DontSpawnInRain);
		if (bDontSpawnInRain && !scenarioPoint.IsFlagSet(CScenarioPointFlags::IgnoreWeatherRestrictions))
		{
			//CTaskUseScenario::UndoEntitySetupForScenario( iSpecificScenario, pAttachedEntity );
			delete pScenarioTask;
			PF_STOP(CreateScenarioPedPreSpawnSetup);
			return NULL;
		}
	}

	// If this pointer is set, we still need to do a collision check before spawning anything.
	if(pCollisionCheckParams)
	{
		PF_STOP(CreateScenarioPedPreSpawnSetup);

		PF_START(CollisionChecks);
		if(!PerformCollisionChecksForScenarioPoint(*pCollisionCheckParams))
		{
			// Failed the collision check, delete the task and return NULL for failure.
			delete pScenarioTask;
			PF_STOP(CollisionChecks);
			return NULL;
		}
		PF_STOP(CollisionChecks);

		// Continue with the spawning procedure.

		PF_START(CreateScenarioPedPreSpawnSetup);
	}

	DefaultScenarioInfo defaultScenarioInfo(DS_Invalid, NULL);
	fHeading = fwAngle::LimitRadianAngleSafe(fHeading);

	bool bCheckForStandardPedBrain = true;
	s32 ScenarioBrainIndex = CTheScripts::GetScriptsForBrains().GetBrainIndexForNewScenarioPed(pScenarioInfo->GetHash());
	if (ScenarioBrainIndex >= 0)
	{	//	This ped will be given a scenario brain so don't attempt to give it a ped brain
		bCheckForStandardPedBrain = false;
	}

	// Check any model restrictions that need to be imposed.
	u32 ModelSetFlags = 0;
	u32 ModelClearFlags = 0;

	if( pScenarioInfo->GetIsFlagSet(CScenarioInfoFlags::NoBulkyItems) || pScenarioInfo->GetIsClass<CScenarioVehicleInfo>() || (predeterminedPedType && predeterminedPedType->m_NoBulkyItems))
	{
		ModelClearFlags |= PV_FLAG_BULKY;

		if (scenarioPoint.GetCarGen())
		{
			CCarGenerator* pCarGen = scenarioPoint.GetCarGen();
			if (pCarGen->HasPreassignedModel())
			{
				CVehicleModelInfo *pModelInfo = (CVehicleModelInfo *)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(pCarGen->GetVisibleModel())));
				if (pModelInfo && pModelInfo->GetIsBike())
				{
					// Allow bulky items on bikes/motorcycles
					ModelClearFlags &= ~PV_FLAG_BULKY;
				}
			}
		}
	}

	// If the scenario point wants us to spawn something in MP that isn't allowed, block it here.
	// Theoretically don't have to do this as designers can flag points as MP/SP specific, but this is a fallback.
	if (NetworkInterface::IsGameInProgress() && predeterminedPedType)
	{
		if (!CWildlifeManager::GetInstance().CheckWildlifeMultiplayerSpawnConditions(predeterminedPedType->m_PedModelIndex))
		{
			delete pScenarioTask;
			PF_STOP(CreateScenarioPedPreSpawnSetup);
			return NULL;
		}
	}

	PF_STOP(CreateScenarioPedPreSpawnSetup);

	PF_START(CreateScenarioPedAddPed);

	// Had some issues with a similar assert failing further down, added this to see if it's
	// somehow the call to AddPed() that adds it to the history (perhaps by letting the tasks update, etc).
	Assert(((iSpawnOptions & CPedPopulation::SF_ignoreScenarioHistory) != 0) || !scenarioPoint.HasHistory());

	bool bScenarioPedCreatedByConcealeadPlayer = pScenarioInfo->GetIsFlagSet(CScenarioInfoFlags::DontSpawnOnConcealedPlayerMachines);

	// Try to create and add a ped into the world (possibly in an interior).
	CPed* pPed = NULL;
	if( pAttachedEntity && pAttachedEntity->m_nFlags.bInMloRoom )
	{
		pPed = CPedPopulation::AddPed(params.m_NewPedModelIndex, vPedRootPos, fHeading, pAttachedEntity, false, -1, NULL, true, false, false, bCheckForStandardPedBrain, defaultScenarioInfo, true, false, ModelSetFlags, ModelClearFlags, params.m_pVariations, NULL, false, 0, bScenarioPedCreatedByConcealeadPlayer);
	}
	else if( iRoomIdFromCollision != 0 )
	{
		aiFatalAssertf(pInteriorInst, "NULL interior inst pointer!");
		pPed = CPedPopulation::AddPed(params.m_NewPedModelIndex, vPedRootPos, fHeading, NULL, true, iRoomIdFromCollision, pInteriorInst, true, false, false, bCheckForStandardPedBrain, defaultScenarioInfo, true, false, ModelSetFlags, ModelClearFlags, params.m_pVariations, NULL, false, 0, bScenarioPedCreatedByConcealeadPlayer);
	}
	else
	{
		pPed = CPedPopulation::AddPed(params.m_NewPedModelIndex, vPedRootPos, fHeading, NULL, false, -1, NULL, true, false, false, bCheckForStandardPedBrain, defaultScenarioInfo, true, false, ModelSetFlags, ModelClearFlags, params.m_pVariations, NULL, false, 0, bScenarioPedCreatedByConcealeadPlayer);
	}	

	if(!pPed)
	{
		delete pScenarioTask;
		PF_STOP(CreateScenarioPedAddPed);
		return NULL;
	}

	PF_STOP(CreateScenarioPedAddPed);

	PF_START(CreateScenarioPedPostSpawnSetup);

	//Note that a ped was spawned.
	fwFlags8 uFlags = CPedPopulation::OPSF_InScenario;
	if(!pScenarioInfo->GetIsClass<CScenarioVehicleInfo>())
	{
		if(scenarioPoint.GetModelSetIndex() != CScenarioPointManager::kNoCustomModelSet)
		{
			uFlags.SetFlag(CPedPopulation::OPSF_ScenarioUsedSpecificModels);
		}
	}
	CPedPopulation::OnPedSpawned(*pPed, uFlags);

	if((iSpawnOptions & CPedPopulation::SF_ignoreScenarioHistory) == 0)
	{
		ASSERT_ONLY(Vec3V scenarioPos = scenarioPoint.GetPosition());
		aiAssertf(!scenarioPoint.HasHistory(), "ScenarioPoint (%s) at (%.2f, %.2f, %.2f) has history and should not be trying to spawn more peds", pScenarioInfo->GetName(), scenarioPos.GetXf(), scenarioPos.GetYf(), scenarioPos.GetZf());
		sm_SpawnHistory.Add(*pPed, scenarioPoint, realScenarioType, chainUserInfo);
	}

	if (ScenarioBrainIndex >= 0)
	{
		CTheScripts::GetScriptsForBrains().GiveScenarioBrainToPed(pPed, ScenarioBrainIndex);
	}

	if (pScenarioInfo->GetIsClass<CScenarioSkiingInfo>() || pScenarioInfo->GetIsClass<CScenarioControlAmbientInfo>())
	{
		weaponAssert(pPed->GetInventory());
		pPed->GetInventory()->AddWeapon(GADGETTYPE_SKIS);
		if(pPed->GetInventory()->GetWeapon(GADGETTYPE_SKIS))
		{
			pPed->GetWeaponManager()->EquipWeapon(GADGETTYPE_SKIS, -1, true);
		}
	}

	pPed->UpdateRagdollMatrix(true);
	pPed->SetDesiredHeading(fHeading);

	if (scenarioPoint.IsInWater())
	{
		vPedRootPos.z -= pPed->GetCapsuleInfo()->GetGroundToRootOffset();
		pPed->SetPosition(vPedRootPos);
	} 
	else if(bUseGroundToRootOffset)
	{
		vPedRootPos.z += pPed->GetCapsuleInfo()->GetGroundToRootOffset();
		pPed->SetPosition(vPedRootPos);
	}

	// This stuff may help to prevent physics activation if we spawn at a distance, which
	// otherwise has potential to cause vertical misplacement.
	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_IsStanding, true);
	pPed->GetPedAiLod().ClearBlockedLodFlag(CPedAILod::AL_LodPhysics);
	pPed->GetPedAiLod().SetForcedLodFlag(CPedAILod::AL_LodPhysics);

	if (scenarioPoint.IsInWater()) 
	{
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_IsSwimming, true);
	}

	aiTask* pScenarioControlTask = SetupScenarioControlTask(*pPed, *pScenarioInfo, pScenarioTask, &scenarioPoint, realScenarioType);
	pPed->GetPedIntelligence()->AddTaskDefault(pScenarioControlTask);

	// Rebuild the queriable state as soon as the task has been allocated
	pPed->GetPedIntelligence()->BuildQueriableState();

	pPed->SetDefaultDecisionMaker();
	pPed->SetCharParamsBasedOnManagerType();
	pPed->GetPedIntelligence()->SetDefaultRelationshipGroup();

	if (pScenarioInfo->GetIsFlagSet(CScenarioInfoFlags::DisableSpeaking))
	{
		pPed->GetSpeechAudioEntity()->DisableSpeaking(true);
	}

	if (pScenarioInfo->GetIsFlagSet(CScenarioInfoFlags::DontSpawnOnConcealedPlayerMachines))
	{
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_CreatedByConcealedPlayer,true);
	}

	if (pScenarioInfo->GetIsFlagSet(CScenarioInfoFlags::PermanentlyDisablePedImpacts))
	{
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromAnyPedImpact, true);
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_PermanentlyDisablePotentialToBeWalkedIntoResponse, true);
	}

#if __BANK
	CPedPopulation::DebugEventCreatePed(pPed);
#endif

	if(scenarioPoint.HasExtendedRange())
	{
		pPed->SetLodDistance(Max((u32)ms_iExtendedScenarioLodDist, pPed->GetLodDistance()));

		// Remember that this ped spawned at an extended range scenario. This may be used to prevent despawning.
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_SpawnedAtExtendedRangeScenario, true);
	}
	
	// See B* 361141: "[LB] We were arrested and were put back outside the police station, we had a wanted level and loads of scenario cops spawned and killed us."
	if(scenarioPoint.IsInteriorState(CScenarioPoint::IS_Inside) && pPed->IsRegularCop())
	{
		// Normally, cops have radios, but we had some issues of them spawning too aggressively
		// possibly related to them calling for help at too far of a distance. By disabling radios,
		// hopefully we can limit that a bit.
		pPed->GetPedIntelligence()->GetCombatBehaviour().ClearFlag(CCombatData::BF_HasRadio);

		// Also, don't make us extra aggressive in chasing down the player, as there can easily be too many.
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_NoCopWantedAggro, true);
	}

	// Mark the ped as arrestable if the scenario point is marked as such.
	if (scenarioPoint.IsFlagSet(CScenarioPointFlags::SpawnedPedIsArrestable))
	{
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_CanBeArrested, true);
	}

	// Remember that this ped originally came from a scenario. That may have some implications on how
	// we want to clean them out if they end up wandering.
	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_SpawnedAtScenario, true);

	// If this fails, we're not considered using the scenario we just spawned at, which is unexpected
	// and bad because then we may end up disrespecting the MaxNoInRange/Range parameters on the scenario.
	// Note: CTaskParkedVehicleScenario is known to not work right now. Could fix it by setting
	// the scenario type in the temporary CScenarioPoint, or by modifying CPed::GetScenarioType(),
	// but not sure that it's worth the time.
	taskAssertf(CPed::GetScenarioType(*pPed) == realScenarioType || pScenarioInfo->GetIsClass<CScenarioParkedVehicleInfo>(),
			"Unexpected scenario type for spawned ped, expected %d (%s), got %d."
			, realScenarioType, pScenarioInfo->GetName(), CPed::GetScenarioType(*pPed));

	PF_STOP(CreateScenarioPedPostSpawnSetup);

	return pPed;
}


CTask* CScenarioManager::SetupScenarioControlTask(const CPed& rPed, const CScenarioInfo& scenarioInfo, CTask* pUseScenarioTask, CScenarioPoint* pScenarioPoint, int scenarioTypeReal)
{
	if (pScenarioPoint)
	{
		//Check if the scenario is on a train.
		CEntity* pEntity = pScenarioPoint->GetEntity();
		if(pEntity && pEntity->GetIsTypeVehicle())
		{
			CVehicle* pVehicle = static_cast<CVehicle *>(pEntity);
			if(pVehicle->InheritsFromTrain())
			{
				//Grab the train.
				CTrain* pTrain = static_cast<CTrain *>(pVehicle);
			
				//Ride the train with the scenario point.
				return rage_new CTaskRideTrain(pTrain, pUseScenarioTask);
			}
		}
	}
	

	// Wrap in a CTaskUnalerted to support scenario chaining and other features.
	CTaskUnalerted* pUnalertedTask = rage_new CTaskUnalerted(pUseScenarioTask, pScenarioPoint, scenarioTypeReal);

	if(scenarioInfo.GetIsFlagSet(CScenarioInfoFlags::UseRoamTask))
	{
		pUnalertedTask->SetRoamMode(true);
	}
	
	//Check if the ped is a cop.
	if(rPed.GetPedType() == PEDTYPE_COP)
	{
		return rage_new CTaskPolice(pUnalertedTask);
	}
	//Check if the ped is army.
	else if(rPed.GetPedType() == PEDTYPE_ARMY)
	{
		return rage_new CTaskArmy(pUnalertedTask);
	}
	else if(rPed.GetPedType() == PEDTYPE_MEDIC)
	{
		return rage_new CTaskAmbulancePatrol(pUnalertedTask);
	}
	else if(rPed.GetPedType() == PEDTYPE_FIRE)
	{
		return rage_new CTaskFirePatrol(pUnalertedTask);
	}

	else
	{
		return pUnalertedTask;
	}
}

//-------------------------------------------------------------------------
// Randomly gives scenarios to cars spawned parked
//-------------------------------------------------------------------------
bool CScenarioManager::GiveScenarioToParkedCar( CVehicle* pVehicle, ParkedType parkedType, int scenarioType)
{
	if( CScenarioBlockingAreas::IsPointInsideBlockingAreas(VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition()), false, true) )
	{
		return false;
	}

	if(scenarioType < 0)
	{
		scenarioType = CTaskParkedVehicleScenario::PickScenario(pVehicle, parkedType);
		
		if(scenarioType < 0)
		{
			return false;
		}
	}

	// Set up the scenario information
	Vector3 vSpawnPos;
	float fHeading;
	CTask* pScenarioTask = CTaskParkedVehicleScenario::CreateScenarioTask(scenarioType, vSpawnPos, fHeading, pVehicle);

	// Create the ped
	if (pScenarioTask)
	{
		// Probably worth probing in this case, since the ground may not be perfectly even.
		bool findZCoorForPed = true;

		//This may not be correct here but seems like it may work out as expected. 
		// Looking at the code the only things that seem to be using the passed in vehicle entity are:
		//	Check to see if the ped should spawn in the rain
		//	Make sure the vehicle entity is not taken into account for the call to CPedPopulation::IsPedGenerationCoordCurrentlyValid
		//
		CScenarioPoint temp(NULL, pVehicle);
		return (CreateScenarioPed(scenarioType, temp, pScenarioTask, vSpawnPos, 0.0f, true/*allowDeepInteriors*/, fHeading, 0, 0, findZCoorForPed )) != NULL;
	}

	return false;
}

//-------------------------------------------------------------------------
// Return a random scenario for a parked car
//-------------------------------------------------------------------------
s32 CScenarioManager::PickRandomScenarioForDrivingCar(CVehicle* UNUSED_PARAM(pVehicle))
{
	// If the car has just parked, everybody out and wander
	// 	if( pVehicle->m_nVehicleFlags.bIsParkedParallelly || pVehicle->m_nVehicleFlags.bIsParkedPerpendicularly  )
	// 	{
	// 		return Vehicle_ParkedThenWander;	
	// 	}
	// 
	// 	// Don't give scenarios to law enforcers
	// 	if( pVehicle->IsLawEnforcementVehicle() ) 
	// 	{
	// 		return Scenario_Invalid;
	// 	}
	// 
	// 	// Scenarios associated with particular vehicles
	// 	if( CheckScenarioTimesAndProbability( Vehicle_ParkDeliveryTruck ) &&
	// 		pVehicle->GetModelIndex() == MI_DELIVERY_TRUCK ||
	// 		pVehicle->GetModelIndex() == MI_DELIVERY_VAN )
	// 	{
	// 		return Vehicle_ParkDeliveryTruck;
	// 	}
	// 
	return Scenario_Invalid;
}

//-------------------------------------------------------------------------
// Sets up a driving scenario for the vehicle
//-------------------------------------------------------------------------
bool CScenarioManager::SetupDrivingVehicleScenario( s32 UNUSED_PARAM(scenario), CVehicle* pVehicle )
{
	bool bReturn = false;
	if( !pVehicle->GetDriver() )
	{
		return false;
	}
	CTaskCarDriveWander* pCarWanderTask = (CTaskCarDriveWander*)pVehicle->GetDriver()->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_CAR_DRIVE_WANDER);

	if( !pCarWanderTask )
	{
		return false;
	}

	aiTask* pDriverScenarioTask = NULL;
	aiTask* pPassengerScenarioTask = NULL;
//	SeatRequest passengerSeat = Seat_invalid;

	// 	if( scenario == Vehicle_ParkDeliveryTruck )
	// 	{
	// 		pDriverScenarioTask = rage_new CTaskComplexDrivingScenario(scenario, pVehicle);
	// 	}
	// 	else if( scenario == Vehicle_DropPassengersOff )
	// 	{
	// 		passengerSeat = Seat_AnyPassenger;
	// 		pDriverScenarioTask = rage_new CTaskComplexDrivingScenario(scenario, pVehicle);
	// 		pPassengerScenarioTask = rage_new CTaskComplexDrivingScenario(scenario, pVehicle);
	// 	}
	// 	else if( scenario == Vehicle_LimoDropPassengerOff )
	// 	{
	// 		passengerSeat = Seat_backLeft;
	// 		pDriverScenarioTask = rage_new CTaskComplexDrivingScenario(scenario, pVehicle);
	// 		pPassengerScenarioTask = rage_new CTaskComplexDrivingScenario(scenario, pVehicle);
	// 	}
	// 	else if( scenario == Vehicle_ParkedThenWander )
	// 	{
	// 		passengerSeat = Seat_AnyPassenger;
	// 		pDriverScenarioTask = rage_new CTaskComplexNewExitVehicle(pVehicle, RF_DelayForTime, fwRandom::GetRandomNumberInRange(0.0f, 1.0f) );
	// 		pPassengerScenarioTask = rage_new CTaskComplexNewExitVehicle(pVehicle, RF_DelayForTime, fwRandom::GetRandomNumberInRange(0.0f, 1.0f) );
	// 	}

	if( pDriverScenarioTask )
	{
		//CTaskComplexCarDriveWander* pNewWanderTask = new CTaskComplexCarDriveWander( pVehicle );
		//pNewWanderTask->SetScenarioTask(pDriverScenarioTask);
		pVehicle->GetDriver()->GetPedIntelligence()->AddTaskDefault( pDriverScenarioTask );
		bReturn = true;
	}

	if( pPassengerScenarioTask )
	{
// 		s32 seatLoopStart = passengerSeat;
// 		s32 seatLoopEnd = passengerSeat;
// 		if( passengerSeat == Seat_AnyPassenger )
// 		{
// 			seatLoopStart = Seat_backLeft;
// 			seatLoopEnd = Seat_backRight;
// 		}
// 		for( s32 iSeat = seatLoopStart; iSeat <= seatLoopEnd; iSeat++ )
// 		{
// 			CPed* pPassenger = pVehicle->GetOccupierOfSeat(iSeat);
// 			if( pPassenger )
// 			{
// 				//CTaskComplexCarDriveWander* pNewWanderTask = new CTaskComplexCarDriveWander( pVehicle );
// 				//pNewWanderTask->SetScenarioTask(pPassengerScenarioTask->Clone());
// 				pPassenger->GetPedIntelligence()->AddTaskDefault( pPassengerScenarioTask->Clone() );
// 			}
// 		}
		delete pPassengerScenarioTask;
	}
	return bReturn;
}



//-------------------------------------------------------------------------
// Randomly gives scenarios to cars spawned driving
//-------------------------------------------------------------------------
bool CScenarioManager::GiveScenarioToDrivingCar( CVehicle* pVehicle, s32 specificScenario )
{
	if( CScenarioBlockingAreas::IsPointInsideBlockingAreas(VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition()), false, true) )
	{
		return false;
	}
	s32 scenario = specificScenario;
	if( scenario == Scenario_Invalid )
	{
		scenario = PickRandomScenarioForDrivingCar(pVehicle);
		if( scenario == Scenario_Invalid )
		{
			return false;
		}
	}
	SetupDrivingVehicleScenario( scenario, pVehicle);
	return true;
}


void CScenarioManager::GetCreationParams(const CScenarioInfo* pScenarioInfo, const CEntity* pAttachEntity, Vector3& vPedPos, float& fHeading)
{
	if (taskVerifyf(pScenarioInfo && pAttachEntity, "Missing scenario info or attach entity"))
	{
		Vector3 vSpawnOffset = pScenarioInfo->GetSpawnPropOffset();
		if (vSpawnOffset != Vector3(Vector3::ZeroType))
		{
			Matrix34 tempMat(Matrix34::IdentityType);
			CScenarioManager::GetCreationMatrix(tempMat, pScenarioInfo->GetSpawnPropOffset(), pScenarioInfo->GetSpawnPropRotationalOffset(), pAttachEntity);
			vPedPos = tempMat.d;
			fHeading =  rage::Atan2f(-tempMat.b.x, tempMat.b.y);
		}
		else
		{
			vPedPos = pAttachEntity->TransformIntoWorldSpace(vPedPos);
			fHeading += pAttachEntity->GetTransform().GetHeading();
		}

		// G.S. We've seen values outside of this range coming from the above.
		// assert and limit the value so we don't get crashes later on.
		aiAssertf(fHeading>-THREE_PI && fHeading<THREE_PI, "Scenario heading out of range -THREE_PI to THREE_PI : %.4f", fHeading);
		fHeading = fwAngle::LimitRadianAngleSafe(fHeading);
	}
}

void CScenarioManager::GetCreationMatrix(Matrix34& matInOut, const Vector3& vTransOffset, const Quaternion& qRotOffset, const CEntity* pEntity)
{
	matInOut = MAT34V_TO_MATRIX34(pEntity->GetMatrix());

	// Rotate the offset so it's relative to the object
	Vector3 vTempTransOffset = vTransOffset;

	matInOut.Transform3x3(vTempTransOffset);

	// Save to matrix
	matInOut.d += vTempTransOffset;

	Quaternion qObjectRotation;
	// Add the offset rotation to the object's current rotation	
	matInOut.ToQuaternion(qObjectRotation);
	qObjectRotation.Multiply(qRotOffset);		

	// Sets the rotational part of the matrix
	matInOut.FromQuaternion(qObjectRotation);
}

//////////////////////////////////////////////////////////////////////////

CTask* CScenarioManager::CreateTask(CScenarioPoint* pScenarioPoint, int scenario, bool forAmbientUsage, float overrideMoveBlendRatio)
{
	const CScenarioInfo* pScenarioInfo = SCENARIOINFOMGR.GetScenarioInfoByIndex(scenario);
	if(pScenarioInfo)
	{
		if(pScenarioInfo->GetIsClass<CScenarioFleeInfo>() )
		{
			int flags = (CTaskUseScenario::SF_StartInCurrentPosition | CTaskUseScenario::SF_SkipEnterClip);
			if(forAmbientUsage)
			{
				flags |= CTaskUseScenario::SF_FlagsForAmbientUsage;
			}
			return rage_new CTaskUseScenario(scenario, pScenarioPoint, flags);
		}
		else if(pScenarioInfo->GetIsClass<CScenarioWanderingInRadiusInfo>())
		{
			return rage_new CTaskWanderingInRadiusScenario(scenario, pScenarioPoint);
		}
		else if(pScenarioInfo->GetIsClass<CScenarioPlayAnimsInfo>())
		{
			int flags = 0;
			if(forAmbientUsage)
			{
				flags |= CTaskUseScenario::SF_FlagsForAmbientUsage;

				if(NetworkInterface::IsGameInProgress())
				{
					flags |= CTaskUseScenario::SF_AddToHistory;
				}
			}
			CTaskUseScenario* pTask = rage_new CTaskUseScenario(scenario, pScenarioPoint, flags);
			if(overrideMoveBlendRatio >= 0.0f)
			{
				pTask->OverrideMoveBlendRatio(overrideMoveBlendRatio);
			}
			return pTask;
		}
		else if(pScenarioInfo->GetIsClass<CScenarioMoveBetweenInfo>())
		{
			return rage_new CTaskMoveBetweenPointsScenario(scenario, pScenarioPoint);
		}
		else if(pScenarioInfo->GetIsClass<CScenarioJoggingInfo>() || pScenarioInfo->GetIsClass<CScenarioWanderingInfo>())
		{
			return rage_new CTaskWanderingScenario(scenario);
		}
		else if (pScenarioInfo->GetIsClass<CScenarioDeadPedInfo>())
		{
			return rage_new CTaskDeadBodyScenario(scenario, pScenarioPoint);
		}
		else
		{
			Assertf(0, "Scenario Info needs a type so that a new task can be created");
			return NULL;
		}
	}

	return NULL;
}


bool CScenarioManager::DetermineScenarioSpawnParameters(
		CScenarioManager::ScenarioSpawnParams& paramsOut,
		const int realScenarioType,
		Vec3V_ConstRef vPedRootPos, CScenarioPoint& scenarioPoint, CTask* pScenarioTask,
		const float fAddRangeInViewMin, const bool allowDeepInteriors,
		const int pedIndexWithinScenario,
		bool findZCoorForPed,
		const PedTypeForScenario* predeterminedPedType,
		bool bAllowFallbackPed)
{
	PF_START(CreateScenarioPedPreSpawnSetup);

	CInteriorInst* pInteriorInst = NULL;

	const CScenarioInfo* pScenarioInfo = GetScenarioInfo(realScenarioType);

	const CEntity* pAttachedEntity = scenarioPoint.GetEntity();

	paramsOut.m_pInteriorInst = NULL;
	paramsOut.m_iRoomIdFromCollision = 0;
	if( pAttachedEntity && pAttachedEntity->GetIsInInterior() )
	{
		fwInteriorLocation loc = pAttachedEntity->GetInteriorLocation();
		const s32 roomIdx = loc.GetRoomIndex();
		pInteriorInst = CInteriorInst::GetInteriorForLocation(loc);

		paramsOut.m_pInteriorInst = pInteriorInst;
		paramsOut.m_iRoomIdFromCollision = roomIdx;

		// Check if they are in a potentially unsee-able interior (such as a subway
		// station when above ground).
		// Only consider subways as deep interiors - some interiors have rooms with no portals close to the entrance
		// and this would stop peds spawning
		if(!allowDeepInteriors)
		{
			const bool bConsideredDeepInterior = pInteriorInst->IsSubwayMLO();
			if(bConsideredDeepInterior)
			{
				if(pInteriorInst->GetNumExitPortalsInRoom(roomIdx) == 0)
				{
					PF_STOP(CreateScenarioPedPreSpawnSetup);
					return false;
				}
			}
		}
	}

	// If the scenariopoint is outside and its raining, don't spawn.
	if( (pAttachedEntity == NULL && scenarioPoint.IsInteriorState(CScenarioPoint::IS_Outside)) || 
		(pAttachedEntity && !pAttachedEntity->m_nFlags.bInMloRoom) )
	{
		bool bDontSpawnInRain = g_weather.IsRaining() && pScenarioInfo && pScenarioInfo->GetIsFlagSet(CScenarioInfoFlags::DontSpawnInRain);
		if (bDontSpawnInRain && !scenarioPoint.IsFlagSet(CScenarioPointFlags::IgnoreWeatherRestrictions))
		{
			PF_STOP(CreateScenarioPedPreSpawnSetup);
			return false;
		}
	}

	if( (pInteriorInst && pInteriorInst->GetProxy()->GetCurrentState() < CInteriorProxy::PS_FULL) )
	{
		PF_STOP(CreateScenarioPedPreSpawnSetup);
		return false;
	}

	PedTypeForScenario tempPedType;
	if(predeterminedPedType)
	{
		Assert(predeterminedPedType->m_PedModelIndex != fwModelId::MI_INVALID);
		tempPedType = *predeterminedPedType;
	}
	else if(!SelectPedTypeForScenario(tempPedType, realScenarioType, scenarioPoint, RCC_VECTOR3(vPedRootPos), 0, pedIndexWithinScenario, fwModelId(), true, bAllowFallbackPed))
	{
		PF_STOP(CreateScenarioPedPreSpawnSetup);
		return false;
	}

	if(tempPedType.m_NeedsToBeStreamed)
	{
		// Even if m_NeedsToBeStreamed, it's possible now that the model is has already
		// been streamed in, since we queue up the scenario peds now, and the previous
		// model check may not have been this frame.
		fwModelId modelId(strLocalIndex(tempPedType.m_PedModelIndex));
		if(!CModelInfo::HaveAssetsLoaded(modelId) ||
				CModelInfo::GetAssetsAreDeletable(modelId))// This line checks if the model is one we are trying to stream out.
		{
			StreamScenarioPeds(modelId, scenarioPoint.IsHighPriority(), realScenarioType);
			PF_STOP(CreateScenarioPedPreSpawnSetup);
			return false;
		}
	}

	paramsOut.m_NewPedModelIndex = tempPedType.m_PedModelIndex;
	paramsOut.m_pVariations = tempPedType.m_Variations;

	Assertf(!CScriptPeds::GetSuppressedPedModels().HasModelBeenSuppressed(paramsOut.m_NewPedModelIndex), "Trying to create a scenario with a suppressed ped model!  Index: %u Name: %s", 
		paramsOut.m_NewPedModelIndex, CBaseModelInfo::GetModelName(paramsOut.m_NewPedModelIndex));

	// Make sure that a ped is actually placeable right now at the candidate coord.
	const float	pedRadius				= 1.21f;
	const bool	doInFrustumTest			= false;// Make sure to not spawn in the view frustum of the player.
	bool	doVisibleToAnyPlayerTest	= true;
	const bool	doObjectCollisionTest	= true;
	const CEntity* pExcludedEntity1		= pAttachedEntity;
	const CEntity* pExcludedEntity2		= NULL;
	if(scenarioPoint.IsHighPriority())
	{
		doVisibleToAnyPlayerTest = false;
	}
	if(pScenarioTask && pScenarioTask->GetTaskType() == CTaskTypes::TASK_USE_SCENARIO)
	{
		pExcludedEntity2 = static_cast<CTaskUseScenario*>(pScenarioTask)->GetPropInEnvironmentToUse();
	}
	s32 dummyRoomIdx = 0;
	CInteriorInst* pDummyInterior = NULL;
	if(!CPedPopulation::IsPedGenerationCoordCurrentlyValid(RCC_VECTOR3(vPedRootPos), false, pedRadius, false, doInFrustumTest, fAddRangeInViewMin, doVisibleToAnyPlayerTest, doObjectCollisionTest, pExcludedEntity1, pExcludedEntity2, dummyRoomIdx, pDummyInterior, false))
	{
		PF_STOP(CreateScenarioPedPreSpawnSetup);
		return false;
	}

	paramsOut.m_bUseGroundToRootOffset = true;
	paramsOut.m_GroundAdjustedPedRootPos = vPedRootPos;

	PF_STOP(CreateScenarioPedPreSpawnSetup);

	PF_FUNC(CreateScenarioPedZCoord);

	bool passedCollisionTest = true;
	if(findZCoorForPed)
	{
		// If we can't find map ground under the ped, look for object ground
		if( !CPedPlacement::FindZCoorForPed(BOTTOM_SURFACE, &RC_VECTOR3(paramsOut.m_GroundAdjustedPedRootPos), NULL, &paramsOut.m_iRoomIdFromCollision, &paramsOut.m_pInteriorInst, GROUND_TEST_Z_HEIGHT_ABOVE, GROUND_TEST_Z_HEIGHT_BELOW, ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_VEHICLE_TYPE, NULL, true ) &&
			!CPedPlacement::FindZCoorForPed(BOTTOM_SURFACE, &RC_VECTOR3(paramsOut.m_GroundAdjustedPedRootPos), NULL, &paramsOut.m_iRoomIdFromCollision, &paramsOut.m_pInteriorInst, GROUND_TEST_Z_HEIGHT_ABOVE, GROUND_TEST_Z_HEIGHT_BELOW, ArchetypeFlags::GTA_OBJECT_TYPE, NULL, true ) )
		{
			passedCollisionTest = false;
		}
	}
	else if(pAttachedEntity == NULL && scenarioPoint.GetInterior() != CScenarioPointManager::kNoInterior)
	{
		if(!CPortalTracker::ProbeForInterior(RCC_VECTOR3(vPedRootPos), paramsOut.m_pInteriorInst, paramsOut.m_iRoomIdFromCollision, NULL, CPortalTracker::SHORT_PROBE_DIST))
		{
			passedCollisionTest = false;
		}
	}
	else
	{
		// If we're not going to be attached to an entity or to the world, we may have to do
		// a check to see if the bounds are loaded:
		//	if(pAttachedEntity == NULL && !pScenarioInfo->GetIsFlagSet(CScenarioInfoFlags::AttachPed))
		//	{
		//		if(!g_StaticBoundsStore.GetBoxStreamer().HasLoadedAboutPos(vPedRootPos, fwBoxStreamerAsset::FLAG_STATICBOUNDS_MOVER))
		//		{
		//			passedCollisionTest = false;
		//		}
		//	}
	}

	if(!passedCollisionTest)
	{
		if( pAttachedEntity == NULL )
		{
			// This should prevent us from trying to use this point again, for a few seconds. The purpose
			// of this is to reduce the amount of time we spend probing points where there is no collision,
			// in particular for points placed in interiors that are not streamed in yet.
			scenarioPoint.ReportFailedCollision();

			if( scenarioPoint.IsInteriorState(CScenarioPoint::IS_Inside) )
			{
				scenarioPoint.SetInteriorState(CScenarioPoint::IS_Unknown);
			}
			return false;
		}
		paramsOut.m_GroundAdjustedPedRootPos = vPedRootPos;
		paramsOut.m_bUseGroundToRootOffset = false;	// Not sure, is this really correct? Preserving old behavior for now...
	}

	return true;
}


void CScenarioManager::UpdateCarGens()
{
	// MAGIC! This is the number of points we can look at per frame. A smaller number
	// would reduce the max CPU overhead per frame, but it would take longer time to do the update.
	static const int kMaxPtsToLookAt = 200;

	// Check if the hour has changed. If so, we may need to add or remove some car generators.
	const int hour = CClock::GetHour();
	if(hour != sm_CarGenHour)
	{
		sm_CarGensNeedUpdate = true;
		Assert(hour <= 0xff);
		sm_CarGenHour = (u8)hour;
	}

	// Check if sm_CarGensNeedUpdate is set, indicating that we need to update.
	// If we are already busy doing an update, we should probably let that continue,
	// and then we'll just have to start over again. If we stopped the update in
	// progress, there would be risk of starvation if sm_CarGensNeedUpdate got set
	// too often (shouldn't happen, but who knows, a script may be malfunctioning and
	// toggling some group on or off, or something like that).
	if(sm_CurrentRegionForCarGenUpdate < 0 && sm_CarGensNeedUpdate)
	{
		// Clear the update flag - other variables will keep track of the progress of
		// the update we are about to start.
		sm_CarGensNeedUpdate = false;

		// Start the update, at the first point in the first region.
		sm_CurrentRegionForCarGenUpdate = sm_CurrentPointWithinRegionForCarGenUpdate = 0;
	}

	// Check if we are currently doing an update. If not, we are done.
	if(sm_CurrentRegionForCarGenUpdate < 0 || !sm_ReadyForOnAddCalls)
	{
		return;
	}

	// Continue the update from the stored region, cluster, and point index.
	int currentRegion = sm_CurrentRegionForCarGenUpdate;
	int currentClusterWithinReg = sm_CurrentClusterWithinRegionForCarGenUpdate;
	int currentPointWithinReg = sm_CurrentPointWithinRegionForCarGenUpdate;
	Assert(currentPointWithinReg >= 0);

	// Continue looping over the regions.
	const int numRegions = SCENARIOPOINTMGR.GetNumRegions();
	for(; currentRegion < numRegions; currentRegion++)
	{
		CScenarioPointRegion* reg = SCENARIOPOINTMGR.GetRegion(currentRegion);
		if(!reg)
		{   
			currentPointWithinReg = 0;
			currentClusterWithinReg = -1;
			continue;
		}

		int numPointsLookedAt = 0;

		//////////////////////////////////////////////////////////////////////////
		// POINTS
		if (currentClusterWithinReg == -1) //if we have not started look at clusters yet ... 
		{
			const int numPts = reg->GetNumPoints();
			for(; currentPointWithinReg < numPts; currentPointWithinReg++)
			{
				numPointsLookedAt++;

				CScenarioPoint& pt = reg->GetPoint(currentPointWithinReg);
				if(!pt.GetOnAddCalled())
				{
					continue;
				}

				UpdateCarGen(pt);			

				// Check if we have looked at enough points. If so, break.
				if(numPointsLookedAt >= kMaxPtsToLookAt)
				{
					currentPointWithinReg++;	// Don't need to look at this one again.
					break;
				}
			}

			//reset the point so if we start looking at clusters it will be starting at 0
			if(currentPointWithinReg >= numPts)
			{
				currentPointWithinReg = 0;
				currentClusterWithinReg = 0;//start at cluster 0
			}
		}		

		//////////////////////////////////////////////////////////////////////////
		// CLUSTERS
		const int ccount = reg->GetNumClusters();
		if (currentClusterWithinReg > -1) //if we have started to look at clusters ... 
		{
			for(; currentClusterWithinReg < ccount; currentClusterWithinReg++)
			{
				// Check if this cluster is in a state where removing a car generator could be a problem.
				bool allowedToRemoveFromThisCluster = true;
				const CSPClusterFSMWrapper* cluster = SCENARIOCLUSTERING.FindClusterWrapper(reg->GetCluster(currentClusterWithinReg));
				if(cluster)
				{
					allowedToRemoveFromThisCluster = cluster->AllowedToRemoveCarGensFromCurrentState();
				}

				const int cpcount = reg->GetNumClusteredPoints(currentClusterWithinReg);
				for(; currentPointWithinReg < cpcount; currentPointWithinReg++)
				{
					numPointsLookedAt++;

					CScenarioPoint& pt = reg->GetClusteredPoint(currentClusterWithinReg, currentPointWithinReg);
					if(!pt.GetOnAddCalled())
					{
						continue;
					}

					if(!UpdateCarGen(pt, allowedToRemoveFromThisCluster))
					{
						// If we hit this case, a car generator would have been removed, but
						// doing so right now wouldn't work well from the cluster's current state.
						// We won't touch it now, but we set this flag again to indicate that
						// we'll have to do another sweep over the points after we have completed this
						// one (to make sure we bring back the car generators to their desired state
						// under the current conditions). Should be a rare occurrence.
						sm_CarGensNeedUpdate = true;
					}

					// Check if we have looked at enough points. If so, break.
					if(numPointsLookedAt >= kMaxPtsToLookAt)
					{
						currentPointWithinReg++;	// Don't need to look at this one again.
						break;
					}
				}

				// Check if we have looked at enough points. If so, break.
				if(numPointsLookedAt >= kMaxPtsToLookAt)
				{
					break; //looked at enough for now ... 
				}
				else
				{
					currentPointWithinReg = 0; // start at the first point
				}
			}
		}

		// If we are done with this region, move on to the next.
		if(currentClusterWithinReg >= ccount)
		{
			currentRegion++;
			currentPointWithinReg = 0;
			currentClusterWithinReg = -1;
		}

		// Regardless, enough for this frame. We currently never look at more than one region in a given frame.
		break;
	}

	// Check if we are done with the last region.
	if(currentRegion >= numRegions)
	{
		sm_CurrentRegionForCarGenUpdate = -1;
		sm_CurrentPointWithinRegionForCarGenUpdate = -1;
		sm_CurrentClusterWithinRegionForCarGenUpdate = -1;
	}
	else
	{
		// Not done, store the current progress so we can resume on the next frame.
		Assert(currentRegion >= 0 && currentRegion <= 0x7fff);
		Assert(currentPointWithinReg >= 0 && currentPointWithinReg <= 0x7fff);
		Assert(currentClusterWithinReg <= 0x7fff);
		sm_CurrentRegionForCarGenUpdate = (s16)currentRegion;
		sm_CurrentPointWithinRegionForCarGenUpdate = (s16)currentPointWithinReg;
		sm_CurrentClusterWithinRegionForCarGenUpdate = (s16)currentClusterWithinReg;
	}
}

bool CScenarioManager::UpdateCarGen(CScenarioPoint &point, bool allowRemoval)
{
	// Check if it's a vehicle scenario.
	const int scenarioType = point.GetScenarioTypeVirtualOrReal();
	if(scenarioType >= 0 && !SCENARIOINFOMGR.IsVirtualIndex(scenarioType))
	{
		const CScenarioInfo* pInfo = SCENARIOINFOMGR.GetScenarioInfoByIndex(scenarioType);
		if(pInfo && pInfo->GetIsClass<CScenarioVehicleInfo>())
		{
			// Check if this vehicle scenario could ever have a car generator.
			const CScenarioVehicleInfo* pSpawnVehInfo = static_cast<const CScenarioVehicleInfo*>(pInfo);
			if(ShouldHaveCarGen(point, *pSpawnVehInfo))
			{
				// Check if this vehicle scenario should currently have a car generator but doesn't,
				// or the other way around.
				const bool hasCarGen = point.HasCarGen();
				const bool shouldHave = ShouldHaveCarGenUnderCurrentConditions(point, *pSpawnVehInfo);

				bool invalid = false;
				if (hasCarGen)
				{
					invalid = IsCurrentCarGenInvalidUnderCurrentConditions(point);
				}

				if( (shouldHave != hasCarGen) || (hasCarGen && invalid))
				{
					// Check if we are about to remove the car generator, and we are not allowed to do so.
					if(hasCarGen && !allowRemoval)
					{
						return false;
					}

					// Calling these functions should clean up the previous state and set it up for
					// the desired state.
					OnRemoveScenario(point);
					OnAddScenario(point);

					// If we created a car generator, it should be one that matches the current
					// conditions. If not, we may end up re-creating it again soon, and possibly
					// repeat the same mistake.
					Assert(!point.HasCarGen() || !IsCurrentCarGenInvalidUnderCurrentConditions(point));
				}
			}
		}
	}

	return true;
}

void CScenarioManager::UpdateExtendExtendedRangeFurther()
{
	// Note: we could have ShouldExtendExtendedRangeFurther() do this directly,
	// but it's probably better to just do it once per frame in case the logic
	// here becomes more expensive, etc. 

	// Note: possibly this could be combined with CPedPopulation::ShouldUse2dDistCheck(),
	// which does a very similar thing, but it would feel odd to just call that from here.

	const CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if(pPlayer)
	{
		if(pPlayer->GetIsParachuting() || pPlayer->GetPedResetFlag(CPED_RESET_FLAG_IsFalling))
		{
			sm_ShouldExtendExtendedRangeFurther = true;
			return;
		}
		/*const*/ CVehicle* pVeh = pPlayer->GetVehiclePedInside();
		if(pVeh && pVeh->GetIsAircraft())
		{
			sm_ShouldExtendExtendedRangeFurther = true;
			return;
		}
	}

	sm_ShouldExtendExtendedRangeFurther = false;
}
