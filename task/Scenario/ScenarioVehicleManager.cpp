//
// task/Scenario/ScenarioVehicleManager.cpp
//
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#include "task/Scenario/ScenarioChaining.h"
#include "task/Scenario/ScenarioChainingTests.h"
#include "task/Scenario/ScenarioDebug.h"
#include "task/Scenario/ScenarioVehicleManager.h"
#include "task/Scenario/ScenarioManager.h"
#include "task/Scenario/ScenarioPointManager.h"
#include "task/Scenario/info/ScenarioInfoManager.h"
#include "task/Default/TaskUnalerted.h"
#include "ai/ambient/AmbientModelSet.h"
#include "ai/ambient/AmbientModelSetManager.h"
#include "ai/BlockingBounds.h"
#include "bank/bank.h"
#include "fwscene/stores/staticboundsstore.h"
#include "game/config.h"
#include "Peds/PedIntelligence.h"
#include "vehicles/vehicle.h"
#include "vehicleAi/VehicleIntelligence.h"
#include "task/Scenario/ScenarioVehicleManager_parser.h"

AI_OPTIMISATIONS()

CVehicleScenarioManager::AttractorTuning::Tunables CVehicleScenarioManager::sm_Tuning;
IMPLEMENT_SCENARIO_TASK_TUNABLES(CVehicleScenarioManager::AttractorTuning, 0x6aa140bf);

//-----------------------------------------------------------------------------

void CVehicleScenarioManager::AttractorData::Init()
{
	// Initialize to the current time.
	m_TimeAllowToAttractNext = fwTimer::GetTimeInMilliseconds();
}

//-----------------------------------------------------------------------------

CVehicleScenarioManager::CVehicleScenarioManager()
		: m_NextToUpdate(0)
{
#if __BANK
	m_DebugDrawAttractors = false;
#endif	// __BANK

	// Find out how many attractors we should reserve space for.
	const int maxAttractors = fwConfigManager::GetInstance().GetSizeOfPool(ATSTRINGHASH("VehicleScenarioAttractors", 0x6f01d3c1), CONFIGURED_FROM_FILE);

	// Reserve space in the two parallel arrays.
	m_AttractorScenarios.Reserve(maxAttractors);
	m_AttractorData.Reserve(maxAttractors);
}


CVehicleScenarioManager::~CVehicleScenarioManager()
{
	// This may not be absolutely necessary, but if it fails, it means that some point
	// probably didn't unregister itself, or that things were shut down in the wrong order.
	taskAssert(m_AttractorScenarios.GetCount() == 0);
}


void CVehicleScenarioManager::Update()
{
	// Determine how many attractors we should update on this frame,
	// but never let us update any attractor more than once per frame.
	const int numAttractors = m_AttractorScenarios.GetCount();
	int numToUpdate = Min(GetTuning().m_NumToUpdatePerFrame, numAttractors);

	// Check if scenario attraction is being suppressed by script.
	numToUpdate = CScenarioManager::IsScenarioAttractionSuppressed() ? 0 : numToUpdate;

	// Loop to update attractors.
	int nextToUpdate = m_NextToUpdate;
	bool retryNextFrame = false;
	while(numToUpdate > 0)
	{
		// Start over from the beginning if passing the end.
		if(nextToUpdate >= numAttractors)
		{
			nextToUpdate = 0;
		}

		// Update this attractor, trying to attract a vehicle.
		bool retryThisNextFrame = false;
		TryToAttractVehicle(nextToUpdate, retryThisNextFrame);
		retryNextFrame |= retryThisNextFrame;

		// Update counters.
		nextToUpdate++;
		numToUpdate--;
	}

	if(!retryNextFrame)
	{
		// Remember where we should start the updates on the next frame.
		m_NextToUpdate = nextToUpdate;
	}
}


void CVehicleScenarioManager::AddAttractor(CScenarioPoint& pt)
{
	// Fail an assert if we fill up. It may not be terrible to run with a smaller capacity
	// than the absolute worst case, so we may want to take this out, but it's best to
	// start out knowing if we run out.
	if(!taskVerifyf(m_AttractorScenarios.GetCount() < m_AttractorScenarios.GetCapacity(),
			"Out of vehicle scenario attractors, increase VehicleScenarioAttractors in 'gameconfig.xml' (currently %d)",
			m_AttractorScenarios.GetCapacity()))
	{
		return;
	}

	// Extra safety check to make sure it's not already in there,
	// hopefully that's not a case we need to deal with.
	taskAssert(FindAttractor(pt) < 0);

	// Add to both arrays.
	m_AttractorScenarios.Append() = &pt;
	m_AttractorData.Append().Init();

	// Make sure that the arrays appear to be consistent.
	taskAssert(m_AttractorData.GetCount() == m_AttractorScenarios.GetCount());
}


void CVehicleScenarioManager::RemoveAttractor(CScenarioPoint& pt)
{
	// Search in the array to see if it's been added.
	const int indexOfAttractor = FindAttractor(pt);
	if(indexOfAttractor < 0)
	{
		// Not in the array, probably OK to just silently return.
		return;
	}

	// Delete it from both arrays.
	m_AttractorScenarios.DeleteFast(indexOfAttractor);
	m_AttractorData.DeleteFast(indexOfAttractor);

	// Make sure that the arrays appear to be consistent.
	taskAssert(m_AttractorData.GetCount() == m_AttractorScenarios.GetCount());
}

#if __BANK

void CVehicleScenarioManager::AddWidgets(bkBank& bank)
{
	bank.AddToggle("Draw Attractors", &m_DebugDrawAttractors);
}


void CVehicleScenarioManager::DebugRender() const
{
	if(!m_DebugDrawAttractors)
	{
		return;
	}

	const int numAttractors = m_AttractorScenarios.GetCount();
	for(int i = 0; i < numAttractors; i++)
	{
		const CScenarioPoint& pt = *m_AttractorScenarios[i];
		const AttractorData& attractor = m_AttractorData[i];

		const char* status = "Active";
		Color32 col(0x80, 0xff, 0x40, 0x80);

		if(fwTimer::GetTimeInMilliseconds() < attractor.m_TimeAllowToAttractNext)
		{
			col.Set(0xff, 0x40, 0x40, 0x80);
			status = "Not ready";
		}

		const Vec3V posV = pt.GetPosition();

		grcDebugDraw::Sphere(posV, 0.5f, col);
		grcDebugDraw::Text(posV, Color_white, 0, 0, status);
	}
}


void CVehicleScenarioManager::ClearAttractionTimers()
{
	const int numAttractors = m_AttractorScenarios.GetCount();
	for(int i = 0; i < numAttractors; i++)
	{
		AttractorData& attractor = m_AttractorData[i];
		attractor.m_TimeAllowToAttractNext = fwTimer::GetTimeInMilliseconds();
	}
}

#endif	// __BANK

bool CVehicleScenarioManager::CanAttractorBeUsed(int attractorIndex) const
{
	// Here, we check various conditions for whether this attractor can be used
	// right now or not. Most of this is standard stuff for scenarios, like
	// time of day.

	const CScenarioPoint& pt = *m_AttractorScenarios[attractorIndex];

	const int indexWithinGroup = 0;		// We don't support virtual scenario types as attractors.

	if(!CScenarioManager::CheckScenarioPointEnabled(pt, indexWithinGroup))
	{
		return false;
	}

	if(!CScenarioManager::CheckScenarioPointTimesAndProbability(pt, false, true, indexWithinGroup))
	{
		return false;
	}

	// The attractor points are a bit odd in that WORLD_VEHICLE_ATTRACTOR has SpawnProbability of 0,
	// which makes some sense in that we don't want anything to spawn there, but it seems useful to
	// be able to set a probability on the point, and respect that.
	if(pt.HasProbabilityOverride())
	{
		if(!CScenarioManager::CheckScenarioProbability(&pt, -1 /* shouldn't be used */))
		{
			return false;
		}
	}

	if(pt.HasHistory())
	{
		return false;
	}

	if(!CScenarioManager::CheckScenarioPointMapData(pt))
	{
		return false;
	}

	const Vec3V posV = pt.GetPosition();
	if(CScenarioBlockingAreas::IsPointInsideBlockingAreas(VEC3V_TO_VECTOR3(posV), false, true))
	{		
		return false;
	}

	if (pt.IsChained() && pt.IsUsedChainFull())
	{
		return false;
	}

	return true;
}


bool CVehicleScenarioManager::CanVehicleBeAttractedByAnything(const CVehicle& veh) const
{
	// Only attract random vehicles.
	if(!veh.PopTypeIsRandom())
	{
		return false;
	}

	if(!veh.m_nVehicleFlags.bUsingPretendOccupants)
	{
		// Check to make sure the vehicle has the correct number of passengers.
		const AttractorTuning::Tunables& tuning = GetTuning();
		s32 iNumberOfPassengers = veh.GetNumberOfPassenger();
		if (iNumberOfPassengers < tuning.m_MinPassengersForAttraction || iNumberOfPassengers > tuning.m_MaxPassengersForAttraction)
		{
			return false;
		}
	}

	// Probably shouldn't try to attract a cloned vehicle - we can't or shouldn't give them tasks, etc,
	// so probably no good can come from that. Similarly, it's probably best to avoid a vehicle
	// that's migrating.
	if(NetworkUtils::IsNetworkCloneOrMigrating(&veh))
	{
		return false;
	}

	// Check for occupants - we either want a driver driving around aimlessly,
	// or pretend occupants that we can turn into real occupants if we decide
	// to take control.
	CPed* pDriver = veh.GetDriver();
	if(pDriver)
	{
		// For now, only allow us to take control over somebody running a regular wander drive task.
		const CPedIntelligence& intel = *pDriver->GetPedIntelligence();
		CTask* pTask = intel.GetTaskActive();
		if(!pTask || pTask->GetTaskType() != CTaskTypes::TASK_CAR_DRIVE_WANDER)
		{
			return false;
		}
	}
	else if(!veh.m_nVehicleFlags.bUsingPretendOccupants)
	{
		// No driver but not using pretend occupants, may be an abandoned or parked vehicle
		// of some sort, probably shouldn't touch it.
		return false;
	}

	//if we are using pretend occupants, but reacting to an event, also don't allow attraction
	if (veh.m_nVehicleFlags.bUsingPretendOccupants && veh.GetIntelligence()->GetPretendOccupantEventPriority() > VehicleEventPriority::VEHICLE_EVENT_PRIORITY_NONE)
	{
		return false;
	}

	// can't use attractors if you have a trailer
 	// no idea why see bug 861861
	if (veh.HasTrailer())
	{
		return false;
	}

	// If we are not using pretend occupants, we might have some passengers scheduled to
	// spawn in as passengers. Even if we are using pretend occupants, if for some reason
	// we still have someone scheduled, we also probably wouldn't want to be attracted.
	if(veh.HasScheduledOccupants())
	{
		return false;
	}

	return true;
}


bool CVehicleScenarioManager::CanVehicleBeAttractedByPoint(const CVehicle& veh, const CScenarioPoint& pt,
		const CAmbientModelSet* pAllowedModelSet, const CAmbientModelSetFilter * pModelsFilter) const
{
	// First, check if the vehicle could be attracted by anything at all.
	if(!CanVehicleBeAttractedByAnything(veh))
	{
		return false;
	}

	// Grab the model info.
	CVehicleModelInfo* pModelInfo = veh.GetVehicleModelInfo();
	Assert(pModelInfo);

	// If we have a model set restriction, check if this vehicle is allowed.
	if(pAllowedModelSet && !pAllowedModelSet->GetContainsModel(veh.GetBaseModelInfo()->GetModelNameHash()))
	{
		return false;
	}
	// Check if the vehicle is restricted inside of vehicles.meta.
	else if (!pAllowedModelSet && pModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_BLOCK_FROM_ATTRACTOR_SCENARIO))
	{
		return false;
	}
	else if (!pAllowedModelSet && (veh.InheritsFromBike() || veh.InheritsFromQuadBike() || veh.InheritsFromAmphibiousQuadBike()) )
	{
		// Disallow vehicle attractors for things that use helmets unless specifically requested in the model info, B* 1349827 [5/15/2013 mdawe]
		return false;
	}
	else if(!pAllowedModelSet && veh.IsLawEnforcementVehicle())
	{
		// Unless directly specified, we probably wouldn't want law enforcement vehicles to become
		// attracted to scenarios. One reason for this is that they have special rules for population
		// with passengers, which we don't want, but also, just potential of not looking right or
		// not reacting properly.
		return false;
	}

	if (pModelsFilter)
	{
		// If any of the models don't pass the filter it is blocked
		for (s32 i = 0; i < veh.GetSeatManager()->GetMaxSeats(); ++i)
		{
			const CPed * pPed = veh.GetSeatManager()->GetPedInSeat(i);
			if (pPed && !pModelsFilter->Match(pPed->GetPedModelInfo()->GetModelNameHash()))
			{
				return false;
			}
		}
	}

	// Get the route helper. If we don't have one, we don't allow attraction
	// right now.
	const CVehicleIntelligence* pIntel = veh.GetIntelligence();
	if(!pIntel)
	{
		return false;
	}
	const CVehicleFollowRouteHelper* pRouteHelper = pIntel->GetFollowRouteHelper();
	if(!pRouteHelper)
	{
		return false;
	}

	// Get the scenario position.
	const Vec3V scenarioPosV = pt.GetPosition();

	// Get the vehicle position and forward direction.
	const fwTransform& transform = veh.GetTransform();
	const Vec3V vehPosV = transform.GetPosition();
	const Vec3V vehDirV = transform.GetForward();

	// Compute a vector from the vehicle to the scenario.
	const Vec3V vehToScenarioV = Subtract(scenarioPosV, vehPosV);

	// Compute the dot product between the forward direction (which should be a unit vector)
	// and the vector to the scenario.
	const ScalarV dotDirV = Dot(vehDirV, vehToScenarioV);

	const ScalarV zeroV(V_ZERO);

	const AttractorTuning::Tunables& tuning = GetTuning();

	// We will square the dot product to avoid a square root, but for that to be
	// valid, it must not be negative.
	if(IsGreaterThanAll(dotDirV, zeroV))
	{
		// Compute the square of the minimum distance (could be precomputed).
		const ScalarV minDistV = LoadScalar32IntoScalarV(tuning.m_MinDistToVehicle);
		const ScalarV minDistSqV = Scale(minDistV, minDistV);

		// Compute the squared distance to the scenario.
		const ScalarV distSqV = MagSquared(vehToScenarioV);

		// Check if the vehicle is too close to the scenario.
		if(IsLessThanAll(distSqV, minDistSqV))
		{
			return false;
		}

		// Compute the square of the dot product. This will be proportional to the
		// square of the cosine of the angle and the square of the distance.
		const ScalarV dotDirSqV = Scale(dotDirV, dotDirV);

		// Multiply the squared cosine threshold by the square of the distance.
		const ScalarV thresholdSqV(tuning.m_ForwardDirectionThresholdCosSquared);
		const ScalarV minDotDirSqV = Scale(distSqV, thresholdSqV);

		// Check if the scenario is within the desired angle of the forward direction.
		if(!IsGreaterThanAll(dotDirSqV, minDotDirSqV))
		{
			return false;
		}
	}
	else
	{
		// The point is behind the vehicle, don't allow the attraction.
		return false;
	}

	// Find the maximum allowed distance between the scenario and the closest point on the path.
	// This uses a default value, or is taken from the point's radius parameter, if it's set.
	float maxDistToPath = GetTuning().m_MaxDistToPathDefault;
	const float radiusFromPt = pt.GetRadius();
	maxDistToPath = Selectf(-radiusFromPt, maxDistToPath, radiusFromPt);

	// Let the route helper do the check.
	if(!pRouteHelper->DoesPathPassNearPoint(scenarioPosV, vehPosV, maxDistToPath))
	{
		// The route doesn't get close enough to the scenario.
		return false;
	}

	// Success!
	return true;
}


int CVehicleScenarioManager::FindAttractor(CScenarioPoint& pt)
{
	// TODO: Consider a more efficient data structure (perhaps with the point
	// keeping track of the index), if this ends up being called a lot.

	const int numAttractors = m_AttractorScenarios.GetCount();
	for(int i = 0; i < numAttractors; i++)
	{
		if(m_AttractorScenarios[i] == &pt)
		{
			return i;
		}
	}

	return -1;
}


void CVehicleScenarioManager::PreventAttractionForTime(int attractorIndex, u32 preventedForTimeMs)
{
	AttractorData& a = m_AttractorData[attractorIndex];

	// Note: this probably doesn't work properly if time wraps around.
	u32 newTimeMs = fwTimer::GetTimeInMilliseconds() + preventedForTimeMs;
	a.m_TimeAllowToAttractNext = Max(newTimeMs, a.m_TimeAllowToAttractNext);
}


bool CVehicleScenarioManager::TakeControlOverVehicle(CVehicle& veh, int attractorIndex, const CAmbientModelSetFilter * pModelsFilter)
{
	CScenarioPoint& pt = *m_AttractorScenarios[attractorIndex];

	taskAssertf(!veh.GetNumberOfPassenger(), "Scenario controlled vehicles should not have passengers: vehicle '%s' has %d (pretend occ: %d).",
			veh.GetDebugName(), veh.GetNumberOfPassenger(), (int)veh.m_nVehicleFlags.bUsingPretendOccupants);

	// When we add occupants through SetUpDriver(), apparently we pass in 0 as the seat index when calling
	// AddPedInCar(), even though theoretically, another seat might be specified as the driver seat. If so,
	// the driver might be counted as a passenger and cause other asserts to fail.
	taskAssertf(veh.GetDriverSeat() == 0, "Vehicle '%s' has driver seat set to %d, the driver might get counted as a passenger.",
			veh.GetDebugName(), veh.GetDriverSeat());

	// Try to make sure we've got actual peds as occupants in the vehicle.
	if(veh.m_nVehicleFlags.bUsingPretendOccupants)
	{
		if(!veh.TryToMakePedsFromPretendOccupants(true, NULL, pModelsFilter))
		{
			return false;
		}
		taskAssertf(!veh.GetNumberOfPassenger(),
				"Scenario controlled vehicle '%s' has %d passengers after TryToMakePedsFromPretendOccupants().",
				veh.GetDebugName(), veh.GetNumberOfPassenger());
	}

	// Make sure we have a driver.
	CPed* pDriver = veh.GetDriver();
	if(!pDriver)
	{
		return false;
	}

	// Prevent the vehicle from returning to pretend occupants.
	veh.m_nVehicleFlags.bDisablePretendOccupants = true;

	// Give the driver a task, which knows how to drive to the scenario point,
	// and use chained scenarios from there.
	CPedIntelligence& intel = *pDriver->GetPedIntelligence();
	const int scenarioType = CScenarioManager::ChooseRealScenario(pt.GetScenarioTypeVirtualOrReal(), pDriver);
	CTaskUnalerted* pTask = rage_new CTaskUnalerted(NULL, &pt, scenarioType);

	// Use road nodes when navigating towards an attractor point.
	pTask->SetScenarioNavMode(CScenarioChainingEdge::kRoads);

	// Use the speed of the first link in the chain when navigating towards an attractor point.
	CScenarioChainingEdge::eNavSpeed iNavSpeed = CScenarioChainingEdge::kNavSpeedDefault;

	CScenarioPointChainUseInfo* pChainUseInfo = NULL;
	if (pt.IsChained())
	{
		int numPts = 0;
		static const int kMaxPts = 16;
		CScenarioChainSearchResults searchResultsArray[kMaxPts];
		numPts = SCENARIOPOINTMGR.FindChainedScenarioPoints(pt, searchResultsArray, NELEM(searchResultsArray));
		// We could check the conditions on each of the return results and pick only valid points (similar to CScenarioFinder::SearchForNextScenarioInChain_OnUpdate()),
		// but that seems unnecessary as whether or not the edge meets this vehicle's conditions could change by the time the vehicle actually gets to the chain.
		// Also, we're not really expecting attractor scenario points to be connected to more than one chain, so just pick the first result in the array.
		if (numPts > 0)
		{
			iNavSpeed = (CScenarioChainingEdge::eNavSpeed)(searchResultsArray[0].m_NavSpeed);
		}

		// Register a chain user. CTaskUnalerted would have done that anyway once it updates,
		// but there would potentially be a window in time where another vehicle could get
		// attracted to this chain, or a ped could spawn on it, etc. Registering here
		// will immediately mark the chain as full (unless it can support more users),
		// and we can also pass in the user info to the history.
		pChainUseInfo = CScenarioChainingGraph::RegisterPointChainUser(pt, pDriver, &veh);
		pTask->SetScenarioChainUserInfo(pChainUseInfo);
	}

	pTask->SetScenarioNavSpeed((u8)iNavSpeed);

	intel.AddTaskDefault(pTask);

	// If the point doesn't already have history, add it.
	if(!pt.HasHistory())
	{
		CScenarioManager::GetSpawnHistory().Add(veh, pt, scenarioType, pChainUseInfo);
	}

	// TODO: Perhaps we should also register this with CScenarioManager::sm_SpawnedVehicles
	// (perhaps renaming that array sm_ScenarioVehicles, and moving it to this manager).
	// However, that probably wouldn't do too much good in the present form, since it looks
	// like it just stores the scenario type of the first scenario and uses that, and in this
	// case, the first scenario is always an attractor.

	// Success!
	return true;
}


void CVehicleScenarioManager::TryToAttractVehicle(int attractorIndex, bool& retryNextFrameOut)
{
	CScenarioPoint& pt = *m_AttractorScenarios[attractorIndex];

	retryNextFrameOut = false;

	// Allow only the selected point to attract vehicles for debug purposes.
#if SCENARIO_DEBUG
	if (CScenarioDebug::ms_bRestrictVehicleAttractionToSelectedPoint && !CScenarioDebug::ms_Editor.IsPointSelected(pt))
	{
		return;
	}
#endif // SCENARIO_DEBUG

	// Make sure the point isn't already in use.
	if(pt.GetUses() > 0)
	{
		return;
	}

	// Check if the attractor is ready.
	AttractorData& attractor = m_AttractorData[attractorIndex];
	if(fwTimer::GetTimeInMilliseconds() < attractor.m_TimeAllowToAttractNext)
	{
		// Not ready.
		return;
	}

	// Get the scenario info.
	const CScenarioInfo* pInfo = SCENARIOINFOMGR.GetScenarioInfoByIndex(pt.GetScenarioTypeVirtualOrReal());
	if(!pInfo || !pInfo->GetIsClass<CScenarioVehicleInfo>())
	{
		return;
	}
	const CScenarioVehicleInfo* pVehInfo = static_cast<const CScenarioVehicleInfo*>(pInfo);

	const AttractorTuning::Tunables& tuning = GetTuning();

	// Set up a sphere to use for the vehicle search.
	const Vec3V sphereCenterV = pt.GetPosition();
	const ScalarV sphereRadiusV(tuning.m_MaxDistToVehicle);

	// Check if physics bounds are loaded near the point.
	// TODO: Perhaps we should extend this to check within some proximity.
	if(!g_StaticBoundsStore.GetBoxStreamer().HasLoadedAboutPos(sphereCenterV, fwBoxStreamerAsset::FLAG_STATICBOUNDS_MOVER))
	{
		PreventAttractionForTime(attractorIndex, tuning.m_TimeAfterNoBoundsMs);
		return;
	}

	// Check other conditions on the attractor, before spending time to look for vehicles.
	if(!CanAttractorBeUsed(attractorIndex))
	{
		PreventAttractionForTime(attractorIndex, GetTuning().m_TimeAfterFailedConditionsMs);
		return;
	}

	// Set up a temporary array when searching for vehicles.
	static const int kMaxFound = 128;
	CSpatialArray::FindResult vehiclesFound[kMaxFound];

	const CSpatialArray& spatialArray = *CVehicle::ms_spatialArray;

	// Look for vehicles.
	int numFound = spatialArray.FindInSphere(sphereCenterV, sphereRadiusV, vehiclesFound, kMaxFound);
	if(numFound <= 0)
	{
		// Early out, may be some stuff below we don't need to do.
		return;
	}

	// Get the model set from the point, if any has been set.
	const CAmbientModelSet* pModelSet = CScenarioManager::GetVehicleModelSetFromIndex(*pVehInfo, pt.GetModelSetIndex());
	
	const int kMaxBlockedModelSets = 24;
	const CAmbientPedModelSet * pBlockedModelSets[kMaxBlockedModelSets] = { NULL };
	int iNumBlockedModelSets = 0;
	Assert(pInfo);
	bool bBlockBulkyItems = false;
	CScenarioManager::GatherBlockedModelSetsFromChainPt(pt, *pInfo, pBlockedModelSets, iNumBlockedModelSets, bBlockBulkyItems);

	CAmbientModelSetFilterForScenario modelsFilter;
	modelsFilter.m_BlockedModels = pBlockedModelSets;
	modelsFilter.m_NumBlockedModelSets = iNumBlockedModelSets;
	modelsFilter.m_bProhibitBulkyItems = bBlockBulkyItems;

	// Loop over the vehicles we found.
	while(numFound > 0)
	{
		// Pick one randomly.
		const int index = g_ReplayRand.GetRanged(0, numFound - 1);
		CVehicle& veh = *CVehicle::GetVehicleFromSpatialArrayNode(vehiclesFound[index].m_Node);

		// Check if this vehicle can be attracted.
		if(CanVehicleBeAttractedByPoint(veh, pt, pModelSet, &modelsFilter))
		{
			// Request a test to see if this chain is blocked. We won't necessarily get a response
			// back this frame.
			bool requestDone = false;
			bool usable = false;
			CParkingSpaceFreeTestVehInfo vehInfo;
			vehInfo.m_ModelInfo = veh.GetBaseModelInfo();
			CScenarioManager::GetChainTests().RequestTest(pt, vehInfo, requestDone, usable);

			// If we didn't get a result back yet, try again on the next frame.
			if(!requestDone)
			{
				retryNextFrameOut = true;
				break;	// No need to test other vehicles.
			}

			// If it wasn't usable, prevent us from trying again for some time. These tests
			// can be somewhat expensive and there is no great urgency in attracting immediately.
			if(!usable)
			{
				PreventAttractionForTime(attractorIndex, sm_Tuning.m_TimeAfterChainTestFailedMs);
				break;	// No need to test other vehicles.
			}

			// Attempt to take control over it.
			if(TakeControlOverVehicle(veh, attractorIndex, &modelsFilter))
			{
				// Success: mark the attractor as having been used, and break out.
				// TODO: It would be good to allow the point to override m_TimeAfterAttractionMs,
				// probably by repurposing CScenarioPoint::m_iTimeTillPedLeaves.
				PreventAttractionForTime(attractorIndex, sm_Tuning.m_TimeAfterAttractionMs);
				break;
			}
		}

		// Remove the vehicle we just tested from the array.
		vehiclesFound[index] = vehiclesFound[--numFound];
	}
}

//-----------------------------------------------------------------------------

// End of file 'task/Scenario/ScenarioVehicleManager.cpp'
