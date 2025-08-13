//
// task/Scenario/ScenarioClustering.cpp
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

// Rage headers

// Framework headers
#include "fwscene/stores/staticboundsstore.h"

// Game headers
#include "ai/BlockingBounds.h"
#include "Network/Objects/NetworkObjectPopulationMgr.h"
#include "pathserver/PathServer.h"
#include "Peds/ped.h"
#include "Peds/PedFactory.h"
#include "Peds/PedIntelligence.h"
#include "Peds/pedpopulation.h"
#include "scene/FocusEntity.h"
#include "scene/world/GameWorld.h"
#include "streaming/populationstreaming.h"
#include "task/Default/TaskUnalerted.h"
#include "task/Scenario/ScenarioChaining.h"
#include "task/Scenario/ScenarioClustering.h"
#include "task/Scenario/ScenarioManager.h"
#include "task/Scenario/ScenarioPoint.h"
#include "task/Scenario/ScenarioPointManager.h"
#include "task/Scenario/info/ScenarioInfoManager.h"
#include "task/Scenario/Types/TaskUseScenario.h"
#include "task/Service/Army/TaskArmy.h"
#include "task/Service/Fire/TaskFirePatrol.h"
#include "task/Service/Medic/TaskAmbulancePatrol.h"
#include "task/Service/Police/TaskPolicePatrol.h"
#include "Vehicles/cargen.h"
#include "Vehicles/VehicleFactory.h"
#include "Vehicles/vehiclepopulation.h"

#if SCENARIO_DEBUG
#include "task/Scenario/ScenarioDebug.h"
#endif // SCENARIO_DEBUG

AI_OPTIMISATIONS()

///////////////////////////////////////////////////////////////////////////////
//  CScenarioPointCluster
///////////////////////////////////////////////////////////////////////////////
CScenarioPointCluster::CScenarioPointCluster()
: m_fNextSpawnAttemptDelay(30.0f)
, m_bAllPointRequiredForSpawn(false)
{

}

CScenarioPointCluster::~CScenarioPointCluster()
{
	CleanUp();
}

u32 CScenarioPointCluster::GetNumPoints() const
{
	return m_Points.GetNumPoints();
}

const CScenarioPoint& CScenarioPointCluster::GetPoint(const u32 index) const
{
	return m_Points.GetPoint(index);
}

CScenarioPoint& CScenarioPointCluster::GetPoint(const u32 index)
{
	return m_Points.GetPoint(index);
}

void CScenarioPointCluster::PostLoad(const CScenarioPointLoadLookUps& lookups)
{
	m_Points.PostLoad(lookups);

	//set the points as being in a cluster
	const u32 count = GetNumPoints();
	for (u32 p = 0; p < count; p++)
	{
		CScenarioPoint& point = GetPoint(p);
		point.SetRunTimeFlag(CScenarioPointRuntimeFlags::IsInCluster, true);
	}

	SCENARIOCLUSTERING.AddCluster(*this);
}

void CScenarioPointCluster::CleanUp()
{
	m_Points.CleanUp();
	SCENARIOCLUSTERING.RemoveCluster(*this);
}

const spdSphere& CScenarioPointCluster::GetSphere() const
{
	return m_ClusterSphere;
}

float CScenarioPointCluster::GetSpawnDelay() const
{
	return m_fNextSpawnAttemptDelay;
}

bool CScenarioPointCluster::GetAllPointRequiredForSpawn() const
{
	return m_bAllPointRequiredForSpawn;
}

#if SCENARIO_DEBUG
void CScenarioPointCluster::AddWidgets(bkGroup* parentGroup)
{
	//TODO: need callback info here so editor knows that region edits are happening ... 
	parentGroup->AddSlider("Failed Spawn Delay (sec)", &m_fNextSpawnAttemptDelay, 0.0f, 500.0f, 1.0f, CScenarioEditor::RegionEditedCallback);
	parentGroup->AddToggle("All Point Required for Spawn", &m_bAllPointRequiredForSpawn, CScenarioEditor::RegionEditedCallback);
}

void CScenarioPointCluster::PrepSaveObject(const CScenarioPointCluster& prepFrom, CScenarioPointLookUps& out_LookUps)
{
	m_Points.PrepSaveObject(prepFrom.m_Points, out_LookUps);

	m_ClusterSphere = prepFrom.m_ClusterSphere;
	m_fNextSpawnAttemptDelay = prepFrom.m_fNextSpawnAttemptDelay;
	m_bAllPointRequiredForSpawn = prepFrom.m_bAllPointRequiredForSpawn;
}

int CScenarioPointCluster::AddPoint(CScenarioPoint& point)
{
	const int index = m_Points.AddPoint(point);
	point.SetRunTimeFlag(CScenarioPointRuntimeFlags::IsInCluster, true);
	UpdateSphere();
	return index;
}

CScenarioPoint* CScenarioPointCluster::RemovePoint(const int pointIndex)
{
	CScenarioPoint* removed = m_Points.RemovePoint(pointIndex);
	removed->SetRunTimeFlag(CScenarioPointRuntimeFlags::IsInCluster, false);
	UpdateSphere();
	return removed;
}

void CScenarioPointCluster::UpdateSphere()
{
	m_ClusterSphere.SetV4(Vec4V(V_ZERO));
	const u32 pcount = m_Points.GetNumPoints();
	for(u32 p = 0; p < pcount; p++)
	{
		CScenarioPoint& point = m_Points.GetPoint(p);

		Vec3V position = point.GetPosition();
		ScalarV radius = FindPointSpawnRadius(point);

		if (!p)
		{
			m_ClusterSphere.Set(position, radius);
		}
		else
		{
			spdSphere sphere(position, radius);
			m_ClusterSphere.GrowSphere(sphere);
		}
	}
}

ScalarV_Out CScenarioPointCluster::FindPointSpawnRadius(const CScenarioPoint& /*point*/) const
{
	//TODO: find true value for this ... it is a guess...
	// 	if (point.HasExtendedRange())
	// 	{
	// 		return ScalarV(V_TEN);//ScalarV(CPedPopulation::ms_addRangeExtendedScenario);
	// 	}

	return ScalarV(V_HALF);
}

#endif // SCENARIO_DEBUG

///////////////////////////////////////////////////////////////////////////////
//  CScenarioClusterSpawnedTrackingData
///////////////////////////////////////////////////////////////////////////////
FW_INSTANTIATE_CLASS_POOL_SPILLOVER(CScenarioClusterSpawnedTrackingData, CONFIGURED_FROM_FILE, 0.72f, atHashString("CScenarioClusterSpawnedTrackingData",0x348c3e40));

void CScenarioClusterSpawnedTrackingData::RefCountModel(u32 modelId)
{
	// Do nothing if this model is invalid, or if it's already counted.
	if(modelId == fwModelId::MI_INVALID || modelId == m_RefCountedModelId)
	{
		return;
	}

	// Clear out any previous ref counts.
	ResetRefCountedModel();

	CBaseModelInfo* pBaseModelInfo = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(modelId)));
	if(Verifyf(pBaseModelInfo, "Expected model info for model %08x", modelId))
	{
		pBaseModelInfo->AddRef();
		m_RefCountedModelId = modelId;
	}
}

void CScenarioClusterSpawnedTrackingData::ResetRefCountedModel()
{
	if(m_RefCountedModelId == fwModelId::MI_INVALID)
	{
		return;
	}

	CBaseModelInfo* pBaseModelInfo = CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(m_RefCountedModelId)));
	if(Verifyf(pBaseModelInfo, "Expected model info for model %08x", m_RefCountedModelId))
	{
		pBaseModelInfo->RemoveRef();
	}
	m_RefCountedModelId = fwModelId::MI_INVALID;
}

///////////////////////////////////////////////////////////////////////////////
//  CSPClusterFSMWrapper
///////////////////////////////////////////////////////////////////////////////

// STATIC ---------------------------------------------------------------------

//stop tracking if the entities are greater than 10 meters from the spawn point.
float CSPClusterFSMWrapper::ms_UntrackDist = 10.0f;
float CSPClusterFSMWrapper::ms_fNextDespawnCheckDelay = 5.0f;
//-----------------------------------------------------------------------------

CSPClusterFSMWrapper::CSPClusterFSMWrapper()
: m_bContainsExteriorPoints(false)
, m_bContainsInteriorPoints(false)
, m_bContainsPeds(false)
, m_bContainsVehicles(false)
, m_bDespawnImmediately(false)
, m_bExtendedRange(false)
, m_bMaxCarGenSpawnAttempsExceeded(false)
, m_MyCluster(NULL)
, m_fNextSpawnAttemptDelay(0.0f)
, m_SpawnPedsPass(SpawnPedsPassInvalid)
, m_SpawnPedsProgress(0)
#if __ASSERT
, m_bWasInMPWhenEnteringSpawnState(false)
#endif
#if SCENARIO_DEBUG
, m_DebugSpawn(false)
, m_uFailCode(FC_NONE)
#endif
{
	m_RandomDensityThreshold = (u8)g_ReplayRand.GetRanged(0, 255);
}

CSPClusterFSMWrapper::~CSPClusterFSMWrapper()
{
	// Don't allow despawning on screen, but make sure we release all the tracked data.
	DespawnAndClearTrackedData(false, true);

	SCENARIOCLUSTERING.RemoveCluster(*this);
	m_MyCluster = NULL;
}

FW_INSTANTIATE_CLASS_POOL_SPILLOVER(CSPClusterFSMWrapper, CONFIGURED_FROM_FILE, 0.52f, atHashString("CSPClusterFSMWrapper",0x5a09f5f9));

void CSPClusterFSMWrapper::Reset()
{
	// If tracking any entities release them.
	// Note: this will destroy on screen, but should only be called now during SHUTDOWN_SESSION.
	DespawnAndClearTrackedData(true, true);
	m_TrackedData.Reset();

	// We probably can't safely do this, because there may be some asserts etc
	// that it's not valid to do a state transition ("Changing state again before previous transition has been completed").
	//	SetState(State_Start);
	// I think this should work, as long as we don't rely on any OnExit type functions to run, etc.
	fwFsm::Reset();

	m_bDespawnImmediately = false;

#if SCENARIO_DEBUG
	SetFailCode(FC_NONE);
#endif
}


void CSPClusterFSMWrapper::ResetForMpSpTransition()
{
	if(!m_MyCluster)	// Not expected to happen, but anyway.
	{
		return;
	}

	// Check if the state is such that we don't have to worry about what's
	// in the cluster already.
	const int state = GetState();
	if(state == State_Start || state == State_ReadyForSpawnAttempt || state == State_Despawn || state == State_Delay)
	{
		return;
	}

	// If already planned to despawn right away for some reason, no need to do anything else.
	if(m_bDespawnImmediately)
	{
		return;
	}

	const bool inMp = NetworkInterface::IsGameInProgress();

	// Loop over what we have in m_TrackedData[], to see if we need to despawn and restart.
	bool needRestart = false;
	const int numTracked = m_TrackedData.GetCount();
	for(int i = 0; i < numTracked; i++)
	{
		const CScenarioClusterSpawnedTrackingData* tracked = m_TrackedData[i];

		// Check if we have a point that's marked as SP-only or MP-only.
		const CScenarioPoint* pt = tracked->m_Point;
		if(pt && !pt->IsAvailability(CScenarioPoint::kBoth))
		{
			Assert(pt->IsAvailability(CScenarioPoint::kOnlySp) || pt->IsAvailability(CScenarioPoint::kOnlyMp));
			bool needToBeInMp = pt->IsAvailability(CScenarioPoint::kOnlyMp);
			if(inMp != needToBeInMp)
			{
				needRestart = true;
				break;
			}
		}

		// Check if we have a scenario type which is marked with the
		// NotAvailableInMultiplayer flag, and we are in MP.
		if(inMp)
		{
			const CScenarioInfo* pScenarioInfo = CScenarioManager::GetScenarioInfo(tracked->m_ScenarioTypeReal);
			if(pScenarioInfo && pScenarioInfo->GetIsFlagSet(CScenarioInfoFlags::NotAvailableInMultiplayer))
			{
				needRestart = true;
				break;
			}
		}
	}

	if(!needRestart)
	{
		// No need to do anything, everything that we have spawned or have planned to spawn
		// can be used in the new mode.
		return;
	}

	// Set this flag so the state machine can despawn and reset itself during the next update.
	m_bDespawnImmediately = true;	
}


void CSPClusterFSMWrapper::ResetModelRefCounts()
{
	// Clear out any model references in our tracked data.
	const int count = m_TrackedData.GetCount();
	for(int i = 0; i < count; i++)
	{
		CScenarioClusterSpawnedTrackingData* tracked = m_TrackedData[i];
		if(tracked)
		{
			tracked->ResetRefCountedModel();
		}
	}
}


bool CSPClusterFSMWrapper::RegisterCarGenVehicleData(const CScenarioPoint& point, CVehicle& vehicle)
{
	// The only time when we would want to register newly spawned vehicles with
	// the cluster is when we are in State_SpawnVehicles. If we are not, it could
	// be either because the point isn't actually in the cluster, or because we decided
	// we didn't want to spawn after all, and switched to a different state. Either way,
	// we return false and the vehicle will be destroyed if not claimed by another cluster.
	if(GetState() != State_SpawnVehicles)
	{
		return false;
	}

	const int tcount = m_TrackedData.GetCount();
	for (int t = 0; t < tcount; t++)
	{
		CScenarioClusterSpawnedTrackingData* tracked = m_TrackedData[t];
		Assert(tracked);
		
		if (tracked->m_Point.Get() == &point)
		{
			//Track this vehicle ... 
			vehicle.m_nVehicleFlags.bIsInCluster = true;
			vehicle.m_nVehicleFlags.bIsInClusterBeingSpawned = true;
			tracked->m_Vehicle = &vehicle;
			return true;
		}
	}

	return false;
}

#if __ASSERT

bool CSPClusterFSMWrapper::IsTrackingPed(const CPed& ped) const
{
	const int tcount = m_TrackedData.GetCount();
	for(int t = 0; t < tcount; t++)
	{
		CScenarioClusterSpawnedTrackingData* tracked = m_TrackedData[t];
		if(tracked->m_Ped == &ped)
		{
			return true;
		}
	}
	return false;
}


bool CSPClusterFSMWrapper::IsTrackingVehicle(const CVehicle& veh) const
{
	const int tcount = m_TrackedData.GetCount();
	for(int t = 0; t < tcount; t++)
	{
		CScenarioClusterSpawnedTrackingData* tracked = m_TrackedData[t];
		if(tracked->m_Vehicle == &veh)
		{
			return true;
		}
	}
	return false;
}

#endif	// __ASSERT

#if SCENARIO_DEBUG
void CSPClusterFSMWrapper::AddWidgets(bkGroup* parentGroup)
{
	Assert(parentGroup);
	Assert(m_MyCluster);
	m_MyCluster->AddWidgets(parentGroup);
	parentGroup->AddToggle("Render Cluster Info", &CScenarioDebug::ms_bRenderClusterInfo);
}

void CSPClusterFSMWrapper::TriggerSpawn()
{
	if (!m_MyCluster)
		return;

	m_DebugSpawn = true;
}

void CSPClusterFSMWrapper::RenderInfo() const
{
	if (!m_MyCluster)
		return;

	Vec3V ccenter = m_MyCluster->GetSphere().GetCenter();
	ccenter = ccenter + Vec3V(0.0f,0.0f,0.6f); //Add a little bit to the Z
	Color32 sphColor = Color_orange;	
	grcDebugDraw::Sphere(ccenter, 0.05f, sphColor);

	//Draw the spider web ... 
	Color32 lnColor = Color_orange;
	const u32 count = m_MyCluster->GetNumPoints();
	for (u32 p = 0; p < count; p++)
	{
		const CScenarioPoint& point = m_MyCluster->GetPoint(p);
		Vec3V ppos = point.GetPosition();
		grcDebugDraw::Line(ccenter, ppos, lnColor, 1);
		grcDebugDraw::Sphere(ppos, 0.05f, sphColor);
	}

	//Draw the status text info.
	char debugText[128];
	u32 line = 1;

	formatf(debugText, "Cluster 0x%p State: %s ", this, GetStaticStateName(GetState()));
	grcDebugDraw::Text(ccenter, Color_DarkOrchid, 0, grcDebugDraw::GetScreenSpaceTextHeight()*(line++), debugText);

	if (GetState() == State_Delay)
	{
		float secs = m_fNextSpawnAttemptDelay - GetTimeInState();
		formatf(debugText, "Fail Status: %s retry in %.02f secs", GetStaticFailCodeName(m_uFailCode), secs);
	}
	else
	{
		formatf(debugText, "Fail Status: %s", GetStaticFailCodeName(m_uFailCode));
	}
	grcDebugDraw::Text(ccenter, Color_DarkOrchid, 0, grcDebugDraw::GetScreenSpaceTextHeight()*(line++), debugText);

	Color32 lnColor2 = Color_red3;
	const int tcount = m_TrackedData.GetCount();
	for (int t = 0; t < tcount; t++)
	{
		const CScenarioClusterSpawnedTrackingData* tracked = m_TrackedData[t];
		Assert(tracked);
		const char* pedName = (tracked->m_Ped)? tracked->m_Ped->GetModelName() : "";
		const char* vehName = (tracked->m_Vehicle)? tracked->m_Vehicle->GetModelName() : "";
		formatf(debugText, "Tracked (P) %s {%x}, (V) %s {%x}", pedName, tracked->m_Ped.Get(), vehName, tracked->m_Vehicle.Get());
		grcDebugDraw::Text(ccenter, Color_DarkOrchid, 0, grcDebugDraw::GetScreenSpaceTextHeight()*(line++), debugText);

		//draw a line from the cluster to the tracked ped/vehicle
		if (tracked->m_Ped)
		{
			Vec3V ppos = tracked->m_Ped->GetTransform().GetPosition();
			grcDebugDraw::Line(ccenter, ppos, lnColor2, 1);
		}
		
		if (tracked->m_Vehicle)
		{
			Vec3V vpos = tracked->m_Vehicle->GetTransform().GetPosition();
			grcDebugDraw::Line(ccenter, vpos, lnColor2, 1);
		}
	}
	
}

#if DR_ENABLED
void CSPClusterFSMWrapper::DebugReport(debugPlayback::TextOutputVisitor& output) const
{
	char buffer[RAGE_MAX_PATH];

	char delayBuf[16];
	if (GetState() == State_Delay)
	{
		float secs = m_fNextSpawnAttemptDelay - GetTimeInState();
		formatf(delayBuf, "%.02f", secs);
	}
	else
	{
		formatf(delayBuf, "Active");
	}

	formatf(buffer, RAGE_MAX_PATH, "0x%-10p%-32s%-24s%-16s%4d", this, GetStaticStateName(GetState()), GetStaticFailCodeName(m_uFailCode), delayBuf, m_TrackedData.GetCount());

	output.AddLine(buffer);
}
#endif //DR_ENABLED

#endif // SCENARIO_DEBUG

fwFsm::Return CSPClusterFSMWrapper::UpdateState(const s32 state, const s32 /*iMessage*/, const fwFsm::Event event)
{
	fwFsmBegin

	fwFsmState(State_Start)
		fwFsmOnUpdate
			return Start_OnUpdate();	

	fwFsmState(State_ReadyForSpawnAttempt)
		fwFsmOnEnter
			return ReadyForSpawnAttempt_OnEnter();
		fwFsmOnUpdate
			return ReadyForSpawnAttempt_OnUpdate();	

	fwFsmState(State_Stream)
		fwFsmOnEnter
			return Stream_OnEnter();
		fwFsmOnUpdate
			return Stream_OnUpdate();

	fwFsmState(State_SpawnVehicles)
		fwFsmOnEnter
			return SpawnVehicles_OnEnter();
		fwFsmOnUpdate
			return SpawnVehicles_OnUpdate();

	fwFsmState(State_SpawnPeds)
		fwFsmOnEnter
			return SpawnPeds_OnEnter();
		fwFsmOnUpdate
			return SpawnPeds_OnUpdate();
		fwFsmOnExit
			return SpawnPeds_OnExit();

	fwFsmState(State_PostSpawnUpdating)
		fwFsmOnUpdate
			return PostSpawnUpdating_OnUpdate();

	fwFsmState(State_Despawn)
		fwFsmOnUpdate
			return Despawn_OnUpdate();

	fwFsmState(State_Delay)
		fwFsmOnUpdate
			return Delay_OnUpdate();
	fwFsmEnd
}

fwFsm::Return CSPClusterFSMWrapper::Start_OnUpdate()
{
	// Start with a random delay. This is done so we don't get all clusters in a region
	// perfectly synchronized, which wouldn't be ideal for performance.
	m_fNextSpawnAttemptDelay = fwRandom::GetRandomNumberInRange(0.0f, 1.0f)*Min(m_MyCluster->GetSpawnDelay(), SCENARIOCLUSTERING.GetTuning().m_MaxTimeBetweenSpawnAttempts);

	SetState(State_Delay);
	return fwFsm::Continue;
}

fwFsm::Return CSPClusterFSMWrapper::ReadyForSpawnAttempt_OnEnter()
{
	//reset fail code ... 
	SCENARIO_DEBUG_ONLY(SetFailCode(FC_NONE));
	
	// Check if any of the points have been set to have an extended range. If so,
	// the designers would probably expect the cluster to spawn further away.
	const u32 pcount = m_MyCluster->GetNumPoints();
	bool containsVehicles = false, extendedRange = false;
	bool containsPeds = false;
	bool containsExteriorPoints = false, containsInteriorPoints = false;
	for(u32 p = 0; p < pcount; p++)
	{
		const CScenarioPoint& point = m_MyCluster->GetPoint(p);

		if(point.IsFlagSet(CScenarioPointFlags::NoSpawn))
		{
			continue;
		}

		if(!extendedRange && point.HasExtendedRange())
		{
			extendedRange = true;
		}		

		if(point.GetInterior() != CScenarioPointManager::kNoInterior)
		{
			containsInteriorPoints = true;
		}
		else
		{
			containsExteriorPoints = true;
		}

		if(!containsVehicles || !containsPeds)
		{
			const int virtualType = point.GetScenarioTypeVirtualOrReal();
			if(CScenarioManager::IsVehicleScenarioType(virtualType))
			{
				containsVehicles = true;
			}
			else
			{
				containsPeds = true;
			}
		}
	}

	m_bContainsExteriorPoints = containsExteriorPoints;
	m_bContainsInteriorPoints = containsInteriorPoints;
	m_bContainsPeds = containsPeds;
	m_bContainsVehicles = containsVehicles;
	m_bExtendedRange = extendedRange;

	// Reset the spawn attempt delay, which will be used after our next potential failure,
	// if we don't set it to something else.
	m_fNextSpawnAttemptDelay = Min(m_MyCluster->GetSpawnDelay(), SCENARIOCLUSTERING.GetTuning().m_MaxTimeBetweenSpawnAttempts);

	return fwFsm::Continue;
}

fwFsm::Return CSPClusterFSMWrapper::ReadyForSpawnAttempt_OnUpdate()
{	
	Assert(m_MyCluster);

#if SCENARIO_DEBUG
	if (m_DebugSpawn)
	{
		SetState(State_Stream);
		return fwFsm::Continue;
	}
#endif

	//NOTE: the player ped is used to determine the we would like to spawn around.
	CPed* player = FindPlayerPed();
	if (player && IsAllowedByDensity())
	{
		ScalarV playerInfluenceRadius;
		GetPlayerInfluenceRadius(playerInfluenceRadius);
		bool bSwitchToSpawn = IsClusterWithinRadius(player, playerInfluenceRadius);

		if (bSwitchToSpawn)
		{
			bool boundsNotLoaded = false;

			// If this cluster may spawn vehicles, check if the bounds are streamed in before
			// going any further. Otherwise, the car generators will probably fail, and we may end
			// up having to wait for a long time until we make another attempt, and we may have streamed
			// in some ped/vehicle models unnecessarily.
			if(m_bContainsVehicles)
			{
				ScalarV testRadiusV(V_FIFTEEN);		// 15 m radius, should be enough that we don't have to do multiple tests unless it's a spread out cluster.
				ScalarV testRadiusSqV = Scale(testRadiusV, testRadiusV);

				// We might have to do more than one test if the cluster is spread out, but
				// we keep track of the tests we have done since we will commonly have multiple points
				// close together.
				static const int kMaxTests = 4;
				Vec3V testedPositions[kMaxTests];
				int numTests = 0;

				const Vec3V streamPosV = player->GetMatrixRef().GetCol3();
				const CScenarioClustering::Tuning& tuning = SCENARIOCLUSTERING.GetTuning();
				const ScalarV assumeLoadedDistV = LoadScalar32IntoScalarV(tuning.m_StaticBoundsAssumeLoadedDist);
				const ScalarV assumeLoadedDistSqV = Scale(assumeLoadedDistV, assumeLoadedDistV);

				const int numPoints = m_MyCluster->GetNumPoints();
				for(int i = 0; i < numPoints; i++)
				{
					const CScenarioPoint& point = m_MyCluster->GetPoint(i);

					// Note: we could potentially check and only do this for the vehicle points, since
					// they are the ones that seem to be struggling.
					if(CouldSpawnAt(point))
					{
						bool needsTest = true;
						const Vec3V worldPosV = point.GetWorldPosition();
						const ScalarV distSqV = DistSquared(streamPosV, worldPosV);

						// First, check if the point is close enough to the player that we should
						// assume that the bounds are there.
						if(IsLessThanAll(distSqV, assumeLoadedDistSqV))
						{
							needsTest = false;
						}
						else
						{
							// Check if this point is already covered by a previous test.
							for(int j = 0; j < numTests; j++)
							{
								if(IsLessThanAll(DistSquared(worldPosV, testedPositions[j]), testRadiusSqV))
								{
									needsTest = false;
									break;
								}
							}
						}

						// If not, do the test.
						if(needsTest)
						{
							spdAABB testBox;
							testBox.Set(Subtract(worldPosV, Vec3V(testRadiusV)), Add(worldPosV, Vec3V(testRadiusV)));
							if(!g_StaticBoundsStore.GetBoxStreamer().HasLoadedWithinAABB(testBox, fwBoxStreamerAsset::FLAG_STATICBOUNDS_MOVER))
							{
								// Bounds not loaded, don't try to spawn yet.
								boundsNotLoaded = true;
								break;
							}

							// Store the result. If we run out, break out silently.
							Assert(numTests < kMaxTests);
							testedPositions[numTests++] = worldPosV;
							if(numTests >= kMaxTests)
							{
								break;
							}
						}
					}
				}
			}

			// If we didn't have bounds problem, go ahead and start.
			if(!boundsNotLoaded)
			{
				SetState(State_Stream);
				return fwFsm::Continue;
			}
		}
	}

	// MAGIC! Check the distance again in 1 second. We don't want to wait for too long
	// since we may lose our opportunity to spawn before the player gets too close.
	m_fNextSpawnAttemptDelay = Min(1.0f, m_fNextSpawnAttemptDelay);

	// wait for a bit before we attempt again.
	SCENARIO_DEBUG_ONLY(SetFailCode(FC_RANGECHECK));
	SetState(State_Despawn);
	return fwFsm::Continue;
}

fwFsm::Return CSPClusterFSMWrapper::Stream_OnEnter()
{
	Assert(m_MyCluster);
	
	//Find the points that are valid to spawn at.   
	int possibleCount = 0;
	const u32 pcount = m_MyCluster->GetNumPoints();
	for(u32 p = 0; p < pcount; p++)
	{
		const CScenarioPoint& point = m_MyCluster->GetPoint(p);
		if (CouldSpawnAt(point))
		{
#if SCENARIO_DEBUG
			//if this is a debug spawn we should clean out the spawn history logging so we dont get asserts
			if (m_DebugSpawn)
			{
				CScenarioManager::sm_SpawnHistory.ClearHistoryForPoint(point);

				// This is probably needed to clear up the old scenario chain users:
				CScenarioManager::sm_SpawnHistory.Update();
				CScenarioChainingGraph::UpdatePointChainUserInfo();

				Assert(!point.HasHistory());		// Shouldn't have any history for the point now.
			}
#endif
			GatherStreamingDependencies(point);	 
			possibleCount++;
		}
	}

	// For car generators on chains, we need to create the chain user objects. We do this outside
	// of the GatherStreamingDependencies() calls, because there are some subtle issues where
	// we want the peds to register first, in case we have peds and empty vehicles on the same
	// chains.
	for(int i = 0; i < m_TrackedData.GetCount(); i++)
	{
		CScenarioClusterSpawnedTrackingData* tracked = m_TrackedData[i];
		if(!tracked->m_UsesCarGen)
		{
			continue;
		}

		const CScenarioPoint& pt = *tracked->m_Point;
		if(!pt.IsChained())
		{
			continue;
		}

		bool allowOverflow = CScenarioManager::AllowExtraChainedUser(pt);
		if(!allowOverflow && pt.IsUsedChainFull())
		{
			// If we hit this case, one of the chains is in use. In that case, we would probably
			// not want to continue even if !GetAllPointRequiredForSpawn(), so just clear up everything
			// and try again later. Nothing should have been spawned or streamed yet.
			DespawnAndClearTrackedData(true, true); //clean up ... 
			SCENARIO_DEBUG_ONLY(m_DebugSpawn = false);//reset this so we dont get into loop where we constantly try to spawn.
			SCENARIO_DEBUG_ONLY(SetFailCode(FC_ALL_POINTS_NOT_VALID));
			SetState(State_Despawn);
			return fwFsm::Continue;

			// If we wanted to just remove from the array rather than switching to the despawn state
			// as above, this would be the way to do it:
			//	m_TrackedData[i] = NULL;
			//	m_TrackedData.Delete(i);
			//	delete tracked;
			//	i--;
			//	continue;
		}

		Assert(!tracked->m_ChainUser);
		tracked->m_ChainUser = CScenarioChainingGraph::RegisterChainUserDummy(pt, allowOverflow);
	}

	if (m_MyCluster->GetAllPointRequiredForSpawn())
	{
		if (possibleCount != m_TrackedData.GetCount())
		{
			// Shouldn't have spawned anything at this point, I think.
			DespawnAndClearTrackedData(true, true); //clean up ... 

			// If this is an extended range cluster, and we are trying to extend the range of those further,
			// we probably shouldn't wait for the whole 30 second delay. So far, we probably haven't done any
			// actual streaming requests or anything else terribly expensive.
			if(m_bExtendedRange && CScenarioManager::ShouldExtendExtendedRangeFurther())
			{
				m_fNextSpawnAttemptDelay = Min(m_fNextSpawnAttemptDelay, 5.0f);	// MAGIC!
			}

			SCENARIO_DEBUG_ONLY(m_DebugSpawn = false);//reset this so we dont get into loop where we constantly try to spawn.
			SCENARIO_DEBUG_ONLY(SetFailCode(FC_ALL_POINTS_NOT_VALID));
			SetState(State_Despawn);
			return fwFsm::Continue;
		} 
	}

#if __ASSERT
	m_bWasInMPWhenEnteringSpawnState = NetworkInterface::IsGameInProgress();
#endif

	return fwFsm::Continue;
}

fwFsm::Return CSPClusterFSMWrapper::Stream_OnUpdate()
{
	
	//Nothing to spawn at currently
	if (!m_TrackedData.GetCount() || m_bDespawnImmediately)
	{
		SCENARIO_DEBUG_ONLY(m_DebugSpawn = false);//reset this so we dont get into loop where we constantly try to spawn.
		SCENARIO_DEBUG_ONLY(SetFailCode(FC_NO_POINTS_FOUND));
		SetState(State_Despawn);
		return fwFsm::Continue; 
	}

	//Somehow we have not been able to stream in ... so leave it for now.
	if (GetTimeInState() >= GetStreamingTimeOut())
	{
		// wait for a bit before we attempt again.
		SCENARIO_DEBUG_ONLY(SetFailCode(FC_STREAMING_TIMEOUT));
		SCENARIO_DEBUG_ONLY(m_DebugSpawn = false);//reset this so we dont get into loop where we constantly try to spawn.
		//Despawn to get rid of anything ... 
		SetState(State_Despawn);
		return fwFsm::Continue; 
	}

	//Request our dependencies. This requests ped and vehicle models, but doesn't spawn anything. 
	if(RequestStreamingDependencies(false))
	{
		// Before we decide to go to the spawn state, check if we still think we
		// should spawn anything. The player may have moved so that what was previously
		// valid no longer is.
		if(IsStillValidToSpawn())
		{
			SetState(State_SpawnVehicles);
			return fwFsm::Continue; 
		}
		else
		{
			SCENARIO_DEBUG_ONLY(SetFailCode(FC_NOT_VALID_TO_SPAWN));
			SCENARIO_DEBUG_ONLY(m_DebugSpawn = false);//reset this so we dont get into loop where we constantly try to spawn.
			//Despawn to get rid of anything ... 
			SetState(State_Despawn);
			return fwFsm::Continue; 
		}
	}

	return fwFsm::Continue;
}

fwFsm::Return CSPClusterFSMWrapper::SpawnVehicles_OnEnter()
{
	// Reset this, it will get set to true by RequestStreamingDependencies() if we
	// any car generator has been scheduled too many times, and failed to spawn.
	m_bMaxCarGenSpawnAttempsExceeded = false;

	return fwFsm::Continue;
}


fwFsm::Return CSPClusterFSMWrapper::SpawnVehicles_OnUpdate()
{
	//Somehow we have not been able to stream in ... so leave it for now.
	if (m_bMaxCarGenSpawnAttempsExceeded || GetTimeInState() >= SCENARIOCLUSTERING.GetTuning().m_SpawnStateTimeout || m_bDespawnImmediately)
	{
		// wait for a bit before we attempt again.
		SCENARIO_DEBUG_ONLY(SetFailCode(FC_SPAWNING_TIMEOUT));
		SCENARIO_DEBUG_ONLY(m_DebugSpawn = false);//reset this so we dont get into loop where we constantly try to spawn.
		//Despawn to get rid of anything ... 
		SetState(State_Despawn);
		return fwFsm::Continue; 
	}

	// Request our dependencies. Unlike during the Stream state, when we pass in true here,
	// the car generators become scheduled to spawn, and the function will return true when
	// they have spawned, and everything else is resident (but there are still no peds).
	if(RequestStreamingDependencies(true))
	{
		// At this point, we have spawned any vehicles (still invisible and out of the game world).
		// We do a final check to decide if we should go ahead and do the rest, in case the cluster would
		// now be too visible to the player.
		if(!IsStillValidToSpawn())
		{
			SCENARIO_DEBUG_ONLY(SetFailCode(FC_NOT_VALID_TO_SPAWN));
			SCENARIO_DEBUG_ONLY(m_DebugSpawn = false);//reset this so we dont get into loop where we constantly try to spawn.
			//Despawn to get rid of anything ... 
			SetState(State_Despawn);
			return fwFsm::Continue; 
		}

		// By RequestStreamingDependencies() returning true, we know that all ped models we need
		// are currently loaded. But, since we don't spawn them all on the same frame now, we need
		// to make sure they stay that way, by adding to the reference count.
		const int numTracked = m_TrackedData.GetCount();
		for(int i = 0; i < numTracked; i++)
		{
			CScenarioClusterSpawnedTrackingData* tracked = m_TrackedData[i];
			if(!tracked->m_UsesCarGen)
			{
				tracked->RefCountModel(tracked->m_PedType.m_PedModelIndex);
			}
		}

		SetState(State_SpawnPeds);
		return fwFsm::Continue;
	}

	return fwFsm::Continue;
}


fwFsm::Return CSPClusterFSMWrapper::SpawnPeds_OnEnter()
{
	SCENARIO_DEBUG_ONLY(m_DebugSpawn = false;) //reset incase we were debug spawning...

	// We will do three passes over m_TrackedData. Start at the beginning of the first...
	m_SpawnPedsPass = SpawnPedsPass1;
	m_SpawnPedsProgress = 0;

	return fwFsm::Continue;
}


fwFsm::Return CSPClusterFSMWrapper::SpawnPeds_OnUpdate()
{
	if(m_bDespawnImmediately)
	{
		SCENARIO_DEBUG_ONLY(m_DebugSpawn = false);//reset this so we dont get into loop where we constantly try to spawn.
		SCENARIO_DEBUG_ONLY(SetFailCode(FC_NO_POINTS_FOUND));
		SetState(State_Despawn);
		return fwFsm::Continue; 
	}

	// Call different update functions for the different passes.
	switch(m_SpawnPedsPass)
	{
		case SpawnPedsPass1:
			return SpawnPeds_OnUpdatePass1();
		case SpawnPedsPass2:
			return SpawnPeds_OnUpdatePass2();
		case SpawnPedsPass3:
			return SpawnPeds_OnUpdatePass3();
		default:
			Assert(0);
			return fwFsm::Continue;
	}
}


fwFsm::Return CSPClusterFSMWrapper::SpawnPeds_OnUpdatePass1()
{
	Assert(m_TrackedData.GetCount() < 255);	// m_SpawnPedsProgress uses a u8

	// Spawn all the peds at regular on-foot scenario points.
	const u32 tdcount = m_TrackedData.GetCount();
	for (u32 td = m_SpawnPedsProgress; td < tdcount; td++)
	{
		CScenarioClusterSpawnedTrackingData* tracked = m_TrackedData[td];
		if (!tracked->m_UsesCarGen)
		{
			SpawnAtPoint(*tracked);
			Assert(!tracked->m_NeedsAddToWorld);

			// If we were holding a reference on a ped model, release it now.
			tracked->ResetRefCountedModel();

			// In this case, we spawned a ped, or possibly failed to do so. We don't
			// want to do too much work each frame, so we will stop now and continue
			// with the next element on the next frame.
			m_SpawnPedsProgress = (u8)(td + 1);
			return fwFsm::Continue;
		}
	}
	// If we get here, all regular peds have had a chance to spawn.

	// validate that all the points are valid if we are required too
	if (m_MyCluster->GetAllPointRequiredForSpawn())
	{
		//Count the number of valid spawns ... 
		u32 vcount = 0;
		const u32 tdcount = m_TrackedData.GetCount();
		for (u32 td = 0; td < tdcount; td++)
		{
			CScenarioClusterSpawnedTrackingData* tracked = m_TrackedData[td];
			if (!IsInvalid(*tracked))
			{
				vcount++;
			}
		}

		if (tdcount != vcount)
		{
			// State_Despawn would despawn anyway, but we probably want to do it
			// forcefully in this case, since nothing should have actually been
			// visible on screen up to this point.
			DespawnAndClearTrackedData(true, false);

			SetState(State_Despawn);
			return fwFsm::Continue;
		}
	}

	// Continue with the next pass. If we got here, we didn't actually spawn any
	// peds this frame, so we call the update function for the next pass immediately.
	m_SpawnPedsPass = SpawnPedsPass2;
	m_SpawnPedsProgress = 0;
	return SpawnPeds_OnUpdatePass2();
}


fwFsm::Return CSPClusterFSMWrapper::SpawnPeds_OnUpdatePass2()
{
	// Do a pass over the points, and for any vehicle scenarios with ped layouts, spawn the peds, and track them.
	const u32 tdcount = m_TrackedData.GetCount();
	for(unsigned int td = m_SpawnPedsProgress; td < tdcount; td++)
	{
		if(m_TrackedData[td]->m_UsesCarGen)
		{
			CScenarioPoint* point = m_TrackedData[td]->m_Point;
			CVehicle* veh = m_TrackedData[td]->m_Vehicle;
			if(point && veh)
			{
				const int scenarioType = point->GetScenarioTypeVirtualOrReal();
				CPed* ppSpawnedPeds[16];
				int numSpawnedPeds = 0;
				if(CScenarioManager::SpawnPedsNearVehicleUsingLayout(*veh, scenarioType, &numSpawnedPeds, ppSpawnedPeds, NELEM(ppSpawnedPeds)))
				{
					for(int i = 0; i < numSpawnedPeds; i++)
					{
						CPed* pPed = ppSpawnedPeds[i];

						CScenarioClusterSpawnedTrackingData* tracked = rage_new CScenarioClusterSpawnedTrackingData;
						tracked->m_ScenarioTypeReal = (u32)scenarioType;	// Not sure, it's a bit awkward to have the vehicle scenario type on the ped.
						tracked->m_Point = point;
						tracked->m_Ped = pPed;

						pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_IsInCluster, true);
						m_TrackedData.PushAndGrow(tracked);
					}

					if(numSpawnedPeds)
					{
						// If we spawned anything using the vehicle layout (which would be rare),
						// continue with the next point on the next frame.
						m_SpawnPedsProgress = (u8)(td + 1);
						return fwFsm::Continue;
					}
				}
				else
				{
					// State_Despawn would despawn anyway, but we probably want to do it
					// forcefully in this case, since nothing should have actually been
					// visible on screen up to this point.
					DespawnAndClearTrackedData(true, false);

					SetState(State_Despawn);
					return fwFsm::Continue;
				}
			}
		}
	}

	// Continue with the next pass. If we got here, we didn't actually spawn any
	// peds this frame, so we call the update function for the next pass immediately.
	m_SpawnPedsPass = SpawnPedsPass3;
	m_SpawnPedsProgress = 0;
	return SpawnPeds_OnUpdatePass3();
}


fwFsm::Return CSPClusterFSMWrapper::SpawnPeds_OnUpdatePass3()
{
	const u32 tdcount = m_TrackedData.GetCount();
	for (u32 td = m_SpawnPedsProgress; td < tdcount; td++)
	{
		if(m_TrackedData[td]->m_UsesCarGen)
		{
			CScenarioClusterSpawnedTrackingData* tracked = m_TrackedData[td];
			CScenarioPoint* point = tracked->m_Point;
			CVehicle* veh = tracked->m_Vehicle;
			if(point && veh)
			{
				CCarGenerator* cargen = point->GetCarGen();
				if(Verifyf(cargen, "Expected car generator."))
				{
					bool ignoreLocalPopulationLimits = false;
					if(NetworkInterface::IsGameInProgress() && point->IsHighPriority())
					{
						ignoreLocalPopulationLimits = true;
						CNetworkObjectPopulationMgr::SetProcessLocalPopulationLimits(false);
					}
					bool success = CScenarioManager::PopulateCarGenVehicle(*veh, *cargen, true);
					if(ignoreLocalPopulationLimits)
					{
						CNetworkObjectPopulationMgr::SetProcessLocalPopulationLimits(true);
					}
					if(!success)
					{
						// State_Despawn would despawn anyway, but we probably want to do it
						// forcefully in this case, since nothing should have actually been
						// visible on screen up to this point.
						DespawnAndClearTrackedData(true, false);

						SetState(State_Despawn);
						return fwFsm::Continue;
					}
				}

				CPed* pDriver = veh->GetDriver();
				CScenarioPointChainUseInfo* pUserInfo = tracked->m_ChainUser;
				if(pUserInfo)
				{
					if(pDriver)
					{
						//Need to transfer ownership of this to the real user.
						//get the task we were setup with ... "this should be task_unalerted"
						CTaskUnalerted* pTaskUnalerted = FindTaskUnalertedForDriver(*pDriver);

						if(pTaskUnalerted)
						{
							pTaskUnalerted->SetScenarioChainUserInfo(pUserInfo);						
						}
					}

					CScenarioChainingGraph::UpdateDummyChainUser(*pUserInfo, pDriver, veh);
				}
				tracked->m_ChainUser = NULL;

				// If the car generator didn't already add us to the world (which is the usual
				// and desired case), add us now. This includes fading up, etc.
				if(tracked->m_NeedsAddToWorld)
				{
					if(!CGameWorld::HasEntityBeenAdded(veh))
					{
						// The car generator currently adds to the world if we were not outside,
						// for simplicity, so if we get here, we are supposed to be outside.
						fwInteriorLocation loc = CGameWorld::OUTSIDE;
						CTheCarGenerators::AddToGameWorld(*veh, NULL, loc);
					}
					tracked->m_NeedsAddToWorld = false;
				}

				veh->m_nVehicleFlags.bIsInClusterBeingSpawned = false;

				// Add to the history.
				Assert(!point->HasHistory());
				const int scenarioType = point->GetScenarioTypeVirtualOrReal();
				taskAssert(!SCENARIOINFOMGR.IsVirtualIndex(scenarioType));
				CScenarioManager::GetSpawnHistory().Add(*veh, *point, scenarioType, pUserInfo);

				// If we spawned a driver, stop here and continue on the next frame at the next
				// element. Note that it's intentional that we just check the driver here: passengers
				// are already put in a queue so we don't have to worry about spawning too many,
				// at this level.
				if(pDriver)
				{
					m_SpawnPedsProgress = (u8)(td + 1);
					return fwFsm::Continue;
				}
			}
		}
	}

	// Done with all spawn passes, switch to the update state.
	m_SpawnPedsPass = SpawnPedsPassInvalid;
	m_SpawnPedsProgress = 0;
	SetState(State_PostSpawnUpdating);
	return fwFsm::Continue;
}

fwFsm::Return CSPClusterFSMWrapper::SpawnPeds_OnExit()
{
	const u32 tdcount = m_TrackedData.GetCount();

	// Check for a friend.
	CPed* pAmbientFriend = NULL;
	CPed* pFirstNonAmbientFriend = NULL;
	for (u32 td = 0; td < tdcount; td++)
	{
		CScenarioClusterSpawnedTrackingData* pTrackData = m_TrackedData[td];

		if (pTrackData && pTrackData->m_Ped)
		{
			CPed* pPed = pTrackData->m_Ped;
			if (pPed->GetTaskData().GetIsFlagSet(CTaskFlags::DependentAmbientFriend))
			{
				pAmbientFriend = pPed;
			}
			else if (!pFirstNonAmbientFriend)
			{
				pFirstNonAmbientFriend = pPed;
			}
		}

		// Can early out once both of these are set.
		if (pAmbientFriend && pFirstNonAmbientFriend)
		{
			break;
		}
	}

	if (pAmbientFriend)
	{
		// Set the ambient friend on all peds in the cluster, except for on the ambient friend, who gets the first regular ped as his buddy.
		for(u32 td = 0; td < tdcount; td++)
		{
			CScenarioClusterSpawnedTrackingData* pTrackData = m_TrackedData[td];

			if (pTrackData && pTrackData->m_Ped)
			{
				CPed* pPed = pTrackData->m_Ped;

				if (pPed == pAmbientFriend)
				{
					pPed->GetPedIntelligence()->SetAmbientFriend(pFirstNonAmbientFriend);
				}
				else
				{
					pPed->GetPedIntelligence()->SetAmbientFriend(pAmbientFriend);
				}
			}
		}
	}

	return fwFsm::Continue;
}

fwFsm::Return CSPClusterFSMWrapper::PostSpawnUpdating_OnUpdate()
{
#if SCENARIO_DEBUG
	if (m_DebugSpawn)
	{
		//Despawn if we were spawned in... The despawn will reset us to state State_ReadyForSpawnAttempt
		SetState(State_Despawn);
		return fwFsm::Continue;
	}
#endif

	if(m_bDespawnImmediately)
	{
		SetState(State_Despawn);
		return fwFsm::Continue; 
	}

	//update all the tracked data ..
	u32 tdcount = m_TrackedData.GetCount();
	for (u32 td = 0; td < tdcount; td++)
	{
		//UpdateTracked(*m_TrackedData[td]);
		CScenarioClusterSpawnedTrackingData& tracked = *m_TrackedData[td];
		if (IsInvalid(tracked))
		{
			CPed* pPed = tracked.m_Ped;
			CVehicle* pVeh = tracked.m_Vehicle;
			if(pPed)
			{
				ClearPedTracking(*pPed);
			}
			if(pVeh)
			{
				ClearVehicleTracking(*pVeh);
			}
			tracked.m_Ped = NULL;
			tracked.m_Vehicle = NULL;

			delete m_TrackedData[td];
			m_TrackedData[td] = NULL;
			m_TrackedData.Delete(td);
			tdcount--; //step back
			td--;
		}
	}

	if (GetTimeInState() > CSPClusterFSMWrapper::ms_fNextDespawnCheckDelay)
	{
		SetTimeInState(0.f);

		bool bAreAllMembersReadyForRemoval = true;
		CPed* player = FindPlayerPed();
		if (player && tdcount)
		{
			// We used to do this, to see if the cluster was far away before trying to check visibility/distance.
			// This didn't work too well since many of our clusters involve driving away.
			//
			//	// If all members are ready to be removed, make sure the player isn't close enough that we'd just spawn again
			//	ScalarV playerInfluenceRadius;
			//	const bool bAddExtraDistance = true;
			//	GetPlayerInfluenceRadius(playerInfluenceRadius, bAddExtraDistance); // Add a little extra distance to ensure we're outside the spawn radius
			//	if (!IsAllowedByDensity() || !IsClusterWithinRadius(player, playerInfluenceRadius))
			//	{
			//		u32 tdcount = m_TrackedData.GetCount();
			//		for (u32 td = 0; td < tdcount; td++)
			//		{
			//			if (!IsTrackedMemberReadyToBeRemoved(*m_TrackedData[td]))
			//			{
			//				bAreAllMembersReadyForRemoval = false;
			//				break;
			//			}
			//		}
			//	}
			//	else
			//	{
			//		bAreAllMembersReadyForRemoval = false;
			//	}

			u32 tdcount = m_TrackedData.GetCount();
			for (u32 td = 0; td < tdcount; td++)
			{
				if (!IsTrackedMemberReadyToBeRemoved(*m_TrackedData[td]))
				{
					bAreAllMembersReadyForRemoval = false;
					break;
				}
			}
		}

		if (bAreAllMembersReadyForRemoval)
		{
			//all the data we were tracking is no longer valid so go into the delay then attempt to spawn again... 
			SetState(State_Despawn);
			return fwFsm::Continue; 
		}

	}
	return fwFsm::Continue;
}

fwFsm::Return CSPClusterFSMWrapper::Despawn_OnUpdate()
{
	// We are now in the despawn state, clear this flag.
	m_bDespawnImmediately = false;

#if SCENARIO_DEBUG
	if (m_DebugSpawn)
	{
		//if we are debug spawning just jump back to attempt spawn state.
		DespawnAndClearTrackedData(true, true);
		SetState(State_ReadyForSpawnAttempt);
		return fwFsm::Continue;
	}
#endif

	// Destroy gracefully and untrack. We'll stay in this state until everything has been destroyed.
	DespawnAndClearTrackedData(false, false);

	if(!m_TrackedData.GetCount())
	{
		//Once we have killed everything we should wait a bit before we attempt to spawn again
		SetState(State_Delay);
		return fwFsm::Continue;
	}
	return fwFsm::Continue;
}

fwFsm::Return CSPClusterFSMWrapper::Delay_OnUpdate()
{
	Assert(m_MyCluster);
	if (SCENARIO_DEBUG_ONLY(m_DebugSpawn || ) GetTimeInState() >= m_fNextSpawnAttemptDelay)
	{
		SetState(State_ReadyForSpawnAttempt);
		return fwFsm::Continue;
	}

	return fwFsm::Continue;
}

bool CSPClusterFSMWrapper::ValidatePointAndSelectRealType(const CScenarioPoint& point, int& scenarioTypeReal_OUT)
{
	scenarioTypeReal_OUT = -1;

	// points attached to entities are not valid for clusters
	if (point.GetEntity())
	{
		return false;
	}

	bool doChecks = true; //perform the validity checks ... 
	SCENARIO_DEBUG_ONLY(doChecks = !m_DebugSpawn);

	if (doChecks)
	{
		if (!CPedPopulation::SP_SpawnValidityCheck_General(point, true /* allow clustered points */))
		{
			return false;
		}

		if (!CPedPopulation::SP_SpawnValidityCheck_Interior(point, 0/*iSpawnOptions*/))
		{
			return false;
		}
	}

	//Always need to do this step ... 
	int unused = -1;
	scenarioTypeReal_OUT = CPedPopulation::SelectPointsRealType(point, 0/*iSpawnOptions*/, unused);
	if (scenarioTypeReal_OUT == -1)
	{
		return false;
	}

	//Always do this because some types can not be spawned at or are spawned in a different way
	if (!CPedPopulation::SP_SpawnValidityCheck_ScenarioType(point, scenarioTypeReal_OUT))
	{
		return false;
	}

	if (doChecks)
	{
		if (!CPedPopulation::SP_SpawnValidityCheck_ScenarioInfo(point, scenarioTypeReal_OUT))
		{
			return false;
		}

		if(!CheckVisibilityForPed(point, scenarioTypeReal_OUT))
		{
			return false;
		}
	}

	return true;
}

bool CSPClusterFSMWrapper::ValidateCarGenPoint(const CScenarioPoint& point)
{
	CCarGenerator* cargen = point.GetCarGen();
	if (!cargen)
		return false;

	Assert(cargen->IsScenarioCarGen());

	if (!cargen->CheckHasSpaceToSchedule())
	{
		//failure registered in function.
		return false;
	}

	if (!point.IsHighPriority())
	{
		if (!cargen->CheckPopulationHasSpace())
		{
			//failure registered in function.
			return false;
		}
	}

	bool doChecks = true; //perform the validity checks ... 
	SCENARIO_DEBUG_ONLY(doChecks = !m_DebugSpawn);

	if (doChecks)
	{
		if (!cargen->CheckIsActiveForGen())
		{
			//failure registered in function.
			return false;
		}

		//Early check so we dont schedule this cargen to spawn when it will fail due to this 
		// another check will happen at the point of spawning as well because the culling ped
		// could have moved while the cargen was scheduled.

		if(!CheckVisibilityForVehicle(point))
		{
			SCENARIO_DEBUG_ONLY(cargen->RegisterSpawnFailure(CPedPopulation::PedPopulationFailureData::FR_spawnPointOutsideCreationZone));
			return false;
		}
	}

	return true;
}

void CSPClusterFSMWrapper::GatherStreamingDependencies(const CScenarioPoint& point)
{
	int scenarioTypeReal = -1;
	if (point.GetCarGen())
	{
		if (ValidateCarGenPoint(point))
		{
			//for car generation points we want to just create tracking data ...
			// it will be filled in later if the cargen code is able too create a vehicle
			CScenarioClusterSpawnedTrackingData* tracked = rage_new CScenarioClusterSpawnedTrackingData();
			tracked->m_UsesCarGen = true;
			tracked->m_Point = const_cast<CScenarioPoint*>(&point);
			m_TrackedData.PushAndGrow(tracked);
		}
	}
	else if (ValidatePointAndSelectRealType(point, scenarioTypeReal))
	{
		Assert(scenarioTypeReal != -1);

		CScenarioClusterSpawnedTrackingData* tracked = rage_new CScenarioClusterSpawnedTrackingData();
		tracked->m_ScenarioTypeReal = scenarioTypeReal;

		// if chained we need to reserve the chain so others dont attempt to use it
		if (point.IsChained())
		{
			tracked->m_ChainUser = CScenarioChainingGraph::RegisterChainUserDummy(point);

			aiAssertf(tracked->m_ChainUser, "RegisterChainUserDummy() call failed, possibly pool may be full "
					"(check for previous assert), or the chain may be broken somehow. "
					"The point is at %.1f, %.1f, %.1f.", point.GetWorldPosition().GetXf(), point.GetWorldPosition().GetYf(), point.GetWorldPosition().GetZf());
		}

		tracked->m_Point = const_cast<CScenarioPoint*>(&point);
		m_TrackedData.PushAndGrow(tracked);

		Assertf(!point.IsAvailability(CScenarioPoint::kOnlySp) || !NetworkInterface::IsGameInProgress(),
				"Cluster point marked as Only in SP scheduled to spawn in MP, at %.1f, %.1f, %.1f",
				point.GetWorldPosition().GetXf(), point.GetWorldPosition().GetYf(), point.GetWorldPosition().GetZf());
	}
}

bool CSPClusterFSMWrapper::RequestStreamingDependencies(bool scheduleCarGens)
{
	//be optimistic here
	bool bAllStreamed = true;

	const u32 tdcount = m_TrackedData.GetCount();
	for (u32 td = 0; td < tdcount; td++)
	{
		CScenarioClusterSpawnedTrackingData* tracked = m_TrackedData[td];
		if (tracked->m_UsesCarGen)
		{
			if(scheduleCarGens)
			{
				//these are the cargen points so we check to see if we have a tracked vehicle yet
				if (!tracked->m_Vehicle)
				{
					bAllStreamed = false;//no vehicle yet ... 
					bool streamingReady = false;
					StreamAndScheduleCarGenPoint(*tracked, true, streamingReady);
				}
			}
			else
			{
				bool streamingReady = false;
				StreamAndScheduleCarGenPoint(*tracked, false, streamingReady);
				if(!streamingReady)
				{
					bAllStreamed = false;
				}
			}
		}
		else
		{
			bool hiPri = tracked->m_Point->IsHighPriority();

#if SCENARIO_DEBUG
			if (m_DebugSpawn)
			{
				hiPri = true;
			}
#endif

			Vector3 scenarioPos = VEC3V_TO_VECTOR3(tracked->m_Point->GetPosition());
			if(CScenarioManager::SelectPedTypeForScenario(tracked->m_PedType, tracked->m_ScenarioTypeReal, *tracked->m_Point, scenarioPos, 0, 0, fwModelId(), true))
			{
				if(tracked->m_PedType.m_NeedsToBeStreamed)
				{
					CScenarioManager::StreamScenarioPeds(fwModelId(strLocalIndex(tracked->m_PedType.m_PedModelIndex)), hiPri, tracked->m_ScenarioTypeReal);
					bAllStreamed = false;
				}
			}
			else
			{
				//if we did not select a type then we are not streamed in.
				bAllStreamed = false;
			}
		}
	}

	return bAllStreamed;
}

void CSPClusterFSMWrapper::SpawnAtPoint(CScenarioClusterSpawnedTrackingData& pntDep)
{
	Assert(pntDep.m_Point);
	CScenarioPoint& point = *pntDep.m_Point;

	Assert(!pntDep.m_Ped);

	// Set up the scenario information
	Vector3 vSpawnPos;
	float fHeading = 0.0f;

	const CScenarioInfo* pScenarioInfo = CScenarioManager::GetScenarioInfo(pntDep.m_ScenarioTypeReal);
	if (aiVerifyf(pScenarioInfo,"No scenario info for scenario point"))
	{
		CTask* pScenarioTask = CScenarioManager::SetupScenarioAndTask( pntDep.m_ScenarioTypeReal, vSpawnPos, fHeading, point, 0, false, true, true);

		// Create the ped
		if( pScenarioTask )
		{
			Assertf(!point.IsAvailability(CScenarioPoint::kOnlySp) || !NetworkInterface::IsGameInProgress(),
					"Cluster point marked as Only in SP is about to spawn in MP, at %.1f, %.1f, %.1f. m_bWasInMPWhenEnteringSpawnState = %d",
					point.GetWorldPosition().GetXf(), point.GetWorldPosition().GetYf(), point.GetWorldPosition().GetZf(), (int)m_bWasInMPWhenEnteringSpawnState);

			//TODO: Not sure here about these params ...
			const float maxFrustumDistToCheck = 80.0f;
			const bool allowDeepInteriors = false;

			Assert(point.IsChained() || !pntDep.m_ChainUser);

			// Protect against streamed out models. This shouldn't happen since we use a ref count now,
			// but before that was added, we would have gotten a hard crash inside CreateScenarioPed(),
			// so better to be safe at this point in case there are still some cases that are not covered properly.
			bool tryToSpawn = true;
			if(pntDep.m_PedType.m_PedModelIndex != fwModelId::MI_INVALID)
			{
				if(!Verifyf(CModelInfo::HaveAssetsLoaded(fwModelId(strLocalIndex(pntDep.m_PedType.m_PedModelIndex))),
						"Model %08x was expected to be loaded, but is not.", pntDep.m_PedType.m_PedModelIndex))
				{
					tryToSpawn = false;
				}
			}

			if(point.HasHistory())
			{
				aiDebugf1("Didn't spawn ped at scenario cluster point at %.1f, %.1f, %.1f, already has history (probably due to a clone added there since we last checked.",
						point.GetWorldPosition().GetXf(), point.GetWorldPosition().GetYf(), point.GetWorldPosition().GetZf());
				tryToSpawn = false;
			}

			CPed* pPed = NULL;
			if(tryToSpawn)
			{
				const bool bDoCollisionChecksForClusterPoints = pScenarioInfo->GetIsFlagSet(CScenarioInfoFlags::CheckForObstructionEvenInClusters);
				// Set up the parameters we would need in order to do a collision check.
				CScenarioManager::CollisionCheckParams collisionCheckParams;
				if (bDoCollisionChecksForClusterPoints)
				{
					collisionCheckParams.m_pScenarioPoint = &point;
					collisionCheckParams.m_ScenarioTypeReal = pntDep.m_ScenarioTypeReal;
					collisionCheckParams.m_ScenarioPos = VECTOR3_TO_VEC3V(vSpawnPos);
					collisionCheckParams.m_pAttachedEntity = point.GetEntity();
					collisionCheckParams.m_pAttachedTrain = NULL;
					collisionCheckParams.m_pPropInEnvironment = NULL;

					if (collisionCheckParams.m_pAttachedEntity && collisionCheckParams.m_pAttachedEntity->GetIsTypeVehicle())
					{
						CVehicle* pAttachedVehicle = static_cast<CVehicle *>(collisionCheckParams.m_pAttachedEntity);
						if (pAttachedVehicle->InheritsFromTrain())
						{
							collisionCheckParams.m_pAttachedTrain = (CTrain*)pAttachedVehicle;
						}
					}

					CObject* pPropInEnvironment = NULL;
					if (CScenarioManager::FindPropsInEnvironmentForScenario(pPropInEnvironment, point, pntDep.m_ScenarioTypeReal, false))
					{
						collisionCheckParams.m_pPropInEnvironment = pPropInEnvironment;

					}
				}

				pPed = CScenarioManager::CreateScenarioPed( pntDep.m_ScenarioTypeReal, point, pScenarioTask, vSpawnPos, maxFrustumDistToCheck, allowDeepInteriors, fHeading, 0, 0, false, bDoCollisionChecksForClusterPoints ? &collisionCheckParams : NULL, &pntDep.m_PedType, false, pntDep.m_ChainUser);
			}
			else
			{
				// If we are not going to call CreateScenarioPed(), we need to delete the task manually so it doesn't leak.
				delete pScenarioTask;
				pScenarioTask = NULL;
			}

			if (point.IsChained() && Verifyf(pntDep.m_ChainUser, "Missing expected dummy chain user pointer"))
			{
				bool keepUser = false;

				if (pPed)
				{
					//get the task we were setup with ... "this should be task_unalerted"
					CTask* task = pPed->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_UNALERTED);
					if (task)
					{
						CTaskUnalerted* tua = (CTaskUnalerted*)task;
						CScenarioChainingGraph::UpdateDummyChainUser(*pntDep.m_ChainUser, pPed, NULL);
						tua->SetScenarioChainUserInfo(pntDep.m_ChainUser);

						keepUser = true;
					}
				}

				if(!keepUser)
				{
					// Clean up.
					CScenarioPointChainUseInfo* pUser = pntDep.m_ChainUser;
					pntDep.m_ChainUser = NULL;

					if(pUser->GetNumRefsInSpawnHistory())
					{
						// If we are in the history, we can't just call UnregisterPointChainUser(). Instead,
						// we turn it into a non-dummy user with the ped and vehicle pointers as NULL.
						// It will get cleaned up once there is no longer a reference from the history.
						// Note that in this case, if it got added to the history the ped would probably
						// actually exist, so I don't think it's the case that we need to remove it from the
						// history or anything.
						CScenarioChainingGraph::UpdateDummyChainUser(*pUser, NULL, NULL);
					}
					else
					{
						CScenarioChainingGraph::UnregisterPointChainUser(*pUser);
					}
				}
				pntDep.m_ChainUser = NULL;
			}

			//Success so track some data ... 
			if (pPed)
			{
				pntDep.m_Ped = pPed;
				pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_IsInCluster, true);
			}
			else
			{
				SCENARIO_DEBUG_ONLY(SetFailCode(FC_PED_CREATION));
			}
		}
	}
}

void CSPClusterFSMWrapper::StreamAndScheduleCarGenPoint(CScenarioClusterSpawnedTrackingData& pntDep, bool allowSchedule, bool& readyToScheduleOut)
{
	Assert(pntDep.m_UsesCarGen);

	readyToScheduleOut = false;

	if(!pntDep.m_Point)
	{
		return;
	}

	CCarGenerator* cargen = pntDep.m_Point->GetCarGen();
	if (!cargen)
	{
		//it is possible that the point is valid and cluster is valid, but the cargen object has not been added yet
		// due to the sweep across the world has not finished yet.... 
		return;
	}

	//Already scheduled ... 
	if(cargen->IsScheduled())
	{
		return;
	}

	if(allowSchedule)
	{
		const CScenarioClustering::Tuning& tuning = SCENARIOCLUSTERING.GetTuning();
		if(pntDep.m_NumCarGenSchedules >= tuning.m_MaxCarGenScheduleAttempts)
		{
			m_bMaxCarGenSpawnAttempsExceeded = true;
			return;
		}
	}

	//Check these again as the queue or pools may have filled up since we started this process.
	if (!cargen->CheckHasSpaceToSchedule())
	{
		//failure registered in function.
		return;
	}

	if (!pntDep.m_Point->IsHighPriority())
	{
		if (!cargen->CheckPopulationHasSpace())
		{
			//failure registered in function.
			return;
		}
	}

	bool doChecks = true; //perform the validity checks ... 
	BANK_ONLY(doChecks = !m_DebugSpawn);

	if (doChecks)
	{
		if(!cargen->CheckScenarioConditionsA())
		{
			//failure registered in function.
			return;
		}
	}

	fwModelId carToUse;
	fwModelId trailerToUse;
	bool carModelShouldBeRequested = false;
	if (!cargen->PickValidModels(carToUse, trailerToUse, carModelShouldBeRequested))
	{
		//failure registered in function.
		return;
	}

	// If this is set, we have already determined that the correct model is not available and would potentially have
	// to be streamed in.
	if(carModelShouldBeRequested)
	{
		CTheCarGenerators::RequestVehicleModelForScenario(carToUse, pntDep.m_Point->IsHighPriority());
		return;//if the car is not present in memory we can get out here ... 
	}

	if (doChecks)
	{
		if(!cargen->CheckScenarioConditionsB(carToUse, trailerToUse))
		{
			//failure registered in function.
			return;
		}

		if(NetworkInterface::IsGameInProgress())
		{
			if(!cargen->CheckNetworkObjectLimits(carToUse))
			{
				//failure registered in function.
				return;
			}

			if(!NetworkInterface::IsPosLocallyControlled( VEC3V_TO_VECTOR3(cargen->GetCentre()) ) )
			{
				return;
			}
		}
	}

	if(!allowSchedule)
	{
		readyToScheduleOut = true;
		return;
	}

	Matrix34 CreationMatrix;
	cargen->BuildCreationMatrix(&CreationMatrix, carToUse.GetModelIndex());

	if (doChecks)
	{
		// do bbox test with dynamic objects to make sure we're not creating on top of another car
		if (!cargen->DoSyncProbeForBlockingObjects(carToUse, CreationMatrix, NULL))
		{
			//failure registered in function.

			// If we failed this test, still consider it a schedule attempt, since we have done
			// some expensive tests that we don't want to just keep repeating until we hit the
			// whole cluster spawn timeout.
			pntDep.m_NumCarGenSchedules++;

			return;
		}
	}

	const bool bAllowedToSwitchOffCarGenIfSomethingOnTop = !pntDep.m_Point->IsHighPriority() SCENARIO_DEBUG_ONLY(&& !m_DebugSpawn);
	bool forceSpawn = false;
#if SCENARIO_DEBUG
	forceSpawn = m_DebugSpawn;
#endif

	ScheduledVehicle* schedVeh = cargen->InsertVehForGen(carToUse, trailerToUse, CreationMatrix, bAllowedToSwitchOffCarGenIfSomethingOnTop, forceSpawn);
	if(schedVeh)
	{
		schedVeh->needsStaticBoundsStreamingTest = false;

		// We don't want the car generator to add us to the game world. We want to stay invisible
		// and without collisions until we know that we can spawn the whole cluster.
		schedVeh->addToGameWorld = false;
		pntDep.m_NeedsAddToWorld = true;

		pntDep.m_NumCarGenSchedules++;
	}
}

void CSPClusterFSMWrapper::DespawnAndClearTrackedData(bool forceDespawn, bool mustClearAllTracked)
{
	int tcount = m_TrackedData.GetCount();
	for(int t = 0; t < tcount; t++)
	{
		CScenarioClusterSpawnedTrackingData* tracked = m_TrackedData[t];
		Assert(tracked);
		if(tracked->m_Ped)
		{
			CPed* pPed = tracked->m_Ped;

			// If the ped is inside a vehicle, try to remove the vehicle.
			CVehicle* pVehPedInside = pPed->GetVehiclePedInside();
			if(pVehPedInside)
			{
				// Mark the vehicle to be removed aggressively.
				pVehPedInside->m_nVehicleFlags.bTryToRemoveAggressively = true;
				if(forceDespawn || IsVehicleReadyToBeRemoved(*pVehPedInside))
				{
					// Clear the ped's InCluster flag, as we're about to remove it.
					ClearPedTracking(*pPed);
					if(pVehPedInside != tracked->m_Vehicle && pVehPedInside->CanBeDeleted())
					{
						//The vehicle delete is expected to clean up the ped....
						CVehicleFactory::GetFactory()->Destroy(pVehPedInside);
					}
					tracked->m_Ped = NULL;
				}
			}
			else
			{
				// Mark the ped to be removed aggressively.
				pPed->SetRemoveAsSoonAsPossible(true);
				// Clear the ped's InCluster flag, as we're about to remove it.
				// Note: we intentionally do this even if we don't actually remove it here,
				// otherwise the SetRemoveAsSoonAsPossible() call wouldn't really work.
				ClearPedTracking(*pPed);

				// Destroy the ped if we can.
				if(forceDespawn || IsPedReadyToBeRemoved(*pPed))
				{
					if(pPed->CanBeDeleted())
					{
						CPedFactory::GetFactory()->DestroyPed(pPed);
					}
					tracked->m_Ped = NULL;
				}
			}
		}

		if (tracked->m_Vehicle)
		{
			CVehicle* pVehicle = tracked->m_Vehicle;

			// Mark the vehicle to be removed aggressively.
			pVehicle->m_nVehicleFlags.bTryToRemoveAggressively = true;
			// Note: we intentionally do this even if we don't actually remove it here,
			// otherwise bTryToRemoveAggressively probably wouldn't really work.
			ClearVehicleTracking(*pVehicle);

			// Remove the vehicle right now if it's not too visible.
			if(forceDespawn || IsVehicleReadyToBeRemoved(*pVehicle))
			{
				if(pVehicle->CanBeDeleted())
				{
					CVehicleFactory::GetFactory()->Destroy(pVehicle);
				}
				tracked->m_Vehicle = NULL;
			}
		}

		bool removeTracked = mustClearAllTracked;

		// If we don't need to clear everything we are tracking, check to see if this is an invalid
		// one that we may want to remove.
		if(!removeTracked && IsInvalid(*tracked))
		{
			removeTracked = true;

			// As an exception, if this point is a car generator that's still scheduled to spawn,
			// don't remove it. This way, we will remain in State_Despawn until the car generators
			// are all done processing our previous spawn attempt, so we can't make another.
			if(tracked->m_Point)
			{
				CCarGenerator* cargen = tracked->m_Point->GetCarGen();
				if(cargen && cargen->IsScheduled())
				{
					removeTracked = false;
				}
			}	
		}

		// If we are meant to clear all the tracking data, or if we have destroyed the ped/vehicle,
		// or if they have become clones, remove from the cluster.
		if(removeTracked)
		{
			// Clear out the ped pointer and the matching in cluster flag, if it still exists.
			if(tracked->m_Ped)
			{
				ClearPedTracking(*tracked->m_Ped);
				tracked->m_Ped = NULL;
			}
			// Clear out the vehicle pointer and the matching in cluster flag, if it still exists.
			if(tracked->m_Vehicle)
			{
				ClearVehicleTracking(*tracked->m_Vehicle);
				tracked->m_Vehicle = NULL;
			}

			// Clear the point and chain user, if there is anything left.
			tracked->m_Point = NULL;
			if (tracked->m_ChainUser)
			{
				CScenarioPointChainUseInfo* pUser = tracked->m_ChainUser;
				tracked->m_ChainUser = NULL;

				Assert(pUser->IsDummy());
				CScenarioChainingGraph::UnregisterPointChainUser(*pUser);
			}

			// Delete the tracking data and remove it from the array.
			delete tracked;
			m_TrackedData.DeleteFast(t);
			t--;
			tcount--;
		}
	}

	// At this point, we should be empty if mustClearAllTracked is true.
	Assert(!tcount || !mustClearAllTracked);
}

// None of this is working too well right now, since clustered peds often need to leave their positions
// to walk along chains, etc. We should perhaps measure the distance between cluster-spawned
// entities instead, to see if they got separated. They are also not expected to always
// run TASK_USE_SCENARIO when on foot.
#if 0

void CSPClusterFSMWrapper::UpdateTracked(CScenarioClusterSpawnedTrackingData& tracked)
{
	//if we no longer have a point this means it is invalid
	if (!tracked.m_Point)
		return;

	//If we dont have a ped or a vehicle it is invalid
	CPed* pPed = tracked.m_Ped;
	CVehicle* pVehicle = tracked.m_Vehicle;
	if (!tracked.m_Ped && !pVehicle)
		return;

	if (pPed)
	{
		bool untrack = false;

		CTask* task = pPed->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_USE_SCENARIO);
		if (!task)
		{
			//if the ped no longer has task use scenario then we should no longer track the ped
			untrack = true;
		}
		else
		{
			// Put the square of the radius in a vector register.
			const ScalarV thresholdDistSqV(CSPClusterFSMWrapper::ms_UntrackDist*CSPClusterFSMWrapper::ms_UntrackDist);

			// Compute the squared distance and check if it's outside the range.
			const Vec3V pedPosV = pPed->GetTransform().GetPosition();
			const ScalarV distSqV = DistSquared(pedPosV, tracked.m_Point->GetPosition());

			//if the ped left the scenario point.
			if(IsGreaterThanOrEqualAll(distSqV, thresholdDistSqV))
			{
				untrack = true;		//dont track it ...
			}
		}

		if(untrack)
		{
			pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_IsInCluster, false);
			tracked.m_Ped = NULL;
		}
	}

	if (pVehicle)
	{
		// Put the square of the radius in a vector register.
		const ScalarV thresholdDistSqV(CSPClusterFSMWrapper::ms_UntrackDist*CSPClusterFSMWrapper::ms_UntrackDist);

		// Compute the squared distance and check if it's outside the range.
		const Vec3V pedPosV = pVehicle->GetTransform().GetPosition();
		const ScalarV distSqV = DistSquared(pedPosV, tracked.m_Point->GetPosition());

		//if the ped left the scenario point.
		if(IsGreaterThanOrEqualAll(distSqV, thresholdDistSqV))
		{
			pVehicle->m_nVehicleFlags.bIsInCluster = false;
			tracked.m_Vehicle = NULL; //dont track it ...
		}
	}
}

#endif	// 0

// Returns whether this tracked member is ready to be removed from the game
bool CSPClusterFSMWrapper::IsTrackedMemberReadyToBeRemoved(CScenarioClusterSpawnedTrackingData& tracked)
{
	if (IsInvalid(tracked))
	{
		return true;
	}
	
	if (tracked.m_Ped) 
	{
		// Check if this ped is now inside of a vehicle. If so, our regular call
		// to CPedPopulation::ShouldRemovePed() would actually always return false.
		// Instead, we check if the vehicle can be removed.
		CVehicle* pVehPedInside = tracked.m_Ped->GetVehiclePedInside();
		if(pVehPedInside)
		{
			// If this vehicle belongs to a cluster, we always consider the ped removable,
			// as far as this function goes. The vehicle could possibly belong to this cluster,
			// in which case its removability will be checked separately anyway, or another cluster.
			// In that case, it still won't be forcefully destroyed while driving the other vehicle,
			// DespawnAndClearTrackedData() doesn't allow for that.
			if(pVehPedInside->m_nVehicleFlags.bIsInCluster)
			{
				return true;
			}
			else
			{
				// Check visibility to the vehicle we happen to be in.
				return IsVehicleReadyToBeRemoved(*pVehPedInside);
			}
		}

		return IsPedReadyToBeRemoved(*tracked.m_Ped);
	}

	if (tracked.m_Vehicle)
	{
		return IsVehicleReadyToBeRemoved(*tracked.m_Vehicle);
	}

	aiAssert(false); //Should never reach here. IsInvalid returns true if there's neither a vehicle or a ped.
	return true;
}

bool CSPClusterFSMWrapper::IsPedReadyToBeRemoved(CPed& ped) const
{
	float fCullRangeInView = 0.f;
	const CPedPopulation::SP_SpawnValidityCheck_PositionParams& positionParams = CPedPopulation::SP_SpawnValidityCheck_GetCurrentParams();
	if (CPedPopulation::ShouldRemovePed(&ped, positionParams.popCtrlCentre, positionParams.fRemovalRangeScale, positionParams.bInAnInterior, positionParams.fRemovalRateScale, fCullRangeInView))
	{
		return true;
	}
	else
	{
		ped.DelayedRemovalTimeReset();
		return false;
	}
}

bool CSPClusterFSMWrapper::IsVehicleReadyToBeRemoved(CVehicle& veh) const
{
	//const CPedPopulation::SP_SpawnValidityCheck_PositionParams& positionParams = CPedPopulation::SP_SpawnValidityCheck_GetCurrentParams();
	CPopCtrl popCtrl = CPedPopulation::GetPopulationControlPointInfo();

	const float	st0	= rage::Sinf(popCtrl.m_baseHeading);
	const float	ct0	= rage::Cosf(popCtrl.m_baseHeading);
	Vector4 vViewPlane(-st0, ct0, 0.0f, 0.0f);
	vViewPlane.w = - vViewPlane.Dot3(popCtrl.m_centre);

	CVehicle *pPlayerVehicle = FindPlayerVehicle();
	bool bAircraftPopCentre = pPlayerVehicle && pPlayerVehicle->GetIsAircraft();
	CVehiclePopulation::EVehLastRemovalReason removalReason = CVehiclePopulation::Removal_None;
	s32 newDelayedRemovalTime = 0;

	if (CVehiclePopulation::ShouldRemoveVehicle(&veh, popCtrl.m_centre, vViewPlane, CVehiclePopulation::GetPopulationRangeScale(), CVehiclePopulation::GetRemovalTimeScale(), bAircraftPopCentre, removalReason, newDelayedRemovalTime))
	{
		return true;
	}
	else
	{
		veh.DelayedRemovalTimeReset();
		return false;
	}
}


bool CSPClusterFSMWrapper::IsInvalid(const CScenarioClusterSpawnedTrackingData& tracked) const
{
	//if we no longer have a point this means it is invalid
	if (!tracked.m_Point)
		return true;

	//If we dont have a ped or a vehicle it is invalid
	if (!tracked.m_Ped && !tracked.m_Vehicle)
		return true;
	
	if (tracked.m_Ped && tracked.m_Ped->IsNetworkClone())
	{
		return true;
	}

	if (tracked.m_Vehicle && tracked.m_Vehicle->IsNetworkClone())
	{
		return true;
	}

	return false;
}

float CSPClusterFSMWrapper::GetStreamingTimeOut() const
{
	const float fStreamingTimeout = 3.0f;
	float retval = fStreamingTimeout;

#if SCENARIO_DEBUG
	//Allow a bit more time for debug spawning ... 
	if (m_DebugSpawn)
	{
		retval = 10.0f;
	}
#endif

	return retval;
}

bool CSPClusterFSMWrapper::CouldSpawnAt(const CScenarioPoint& point) const
{
	if(point.IsFlagSet(CScenarioPointFlags::NoSpawn))
	{
		return false;
	}

	const int virtualType = point.GetScenarioTypeVirtualOrReal();

	// If this is a vehicle scenario then check to see if it has a cargenerator as 
	// that is the only way this point could be spawned at.
	if(CScenarioManager::IsVehicleScenarioType(virtualType))
	{
		if (!point.GetCarGen()) //no cargen means we cant spawn things here ... 
		{
			return false;
		}
	}

	return true;
}


bool CSPClusterFSMWrapper::IsStillValidToSpawn() const
{
#if SCENARIO_DEBUG
	if(m_DebugSpawn)
	{
		// When debug spawning, the checks below should not apply.
		return true;
	}
#endif	// SCENARIO_DEBUG

	const int trackedCount = m_TrackedData.GetCount();
	for(int i = 0; i < trackedCount; i++)
	{
		CScenarioClusterSpawnedTrackingData* tracked = m_TrackedData[i];
		const CScenarioPoint* pt = tracked->m_Point;
		if(pt)
		{
			if(tracked->m_UsesCarGen)
			{
				if(!CheckVisibilityForVehicle(*pt))
				{
					return false;
				}
			}
			else
			{
				if(!CheckVisibilityForPed(*pt, tracked->m_ScenarioTypeReal))
				{
					return false;
				}
			}
		}
	}

	return true;
}


bool CSPClusterFSMWrapper::CheckVisibilityForPed(const CScenarioPoint& point, int scenarioTypeReal) const
{
	//Adjust the params to allow for a greater max creation radius for clustered points
	CPedPopulation::SP_SpawnValidityCheck_PositionParams curParams = CPedPopulation::SP_SpawnValidityCheck_GetCurrentParams();
	curParams.fAddRangeInViewMax = LARGE_FLOAT;
	curParams.fAddRangeOutOfViewMax = LARGE_FLOAT;
	curParams.faddRangeExtendedScenario = LARGE_FLOAT;

	// Probably don't want short range points to invalidate us - if anything, we should use it to reduce the spawn distance of the whole cluster.
	curParams.fMaxRangeShortScenario = LARGE_FLOAT;
	curParams.fMaxRangeShortAndHighPriorityScenario = LARGE_FLOAT;

	if(!CPedPopulation::SP_SpawnValidityCheck_Position(point, scenarioTypeReal, curParams))
	{
		return false;
	}

	return true;
}


bool CSPClusterFSMWrapper::CheckVisibilityForVehicle(const CScenarioPoint& point) const
{
	bool noMaxLimit = true;
	if(!CVehiclePopulation::HasPopGenShapeBeenInitialized()
			|| !CScenarioManager::CheckPosAgainstVehiclePopulationKeyhole(point.GetPosition(), ScalarV(V_ONE) /* TODO: Think more about distance scaling in the clustered case. */, point.HasExtendedRange(), true, noMaxLimit))
	{
		return false;
	}

	// The car generator code does this internally, so if we are not going to pass this test,
	// we may as well not schedule the car generators.
	CCarGenerator* cargen = point.GetCarGen();
	if(cargen)
	{
		bool bTooClose = false;
		if(!cargen->CheckIfWithinRangeOfAnyPlayers(bTooClose))
		{
			return false;
		}
	}

	return true;
}


void CSPClusterFSMWrapper::GetPlayerInfluenceRadius(ScalarV_InOut playerInfluenceRadius, bool addExtraDistance) const 
{
	if(m_bExtendedRange)
	{
		playerInfluenceRadius.Setf(200.0f);//todo: data drive this ... also this is a guess ... 
		if(CScenarioManager::ShouldExtendExtendedRangeFurther())
		{
			playerInfluenceRadius = Max(ScalarV(CPedPopulation::GetAddRangeExtendedScenarioExtended()), playerInfluenceRadius);
			if(m_bContainsVehicles)
			{
				playerInfluenceRadius = Max(ScalarV(CScenarioManager::GetExtendedExtendedRangeForVehicleSpawn()), playerInfluenceRadius);
			}
		}
	}
	else
	{
		playerInfluenceRadius = ScalarV(100.0f);//todo: data drive this ... also this is a guess ... 
	}

	// If it contains vehicles, make sure it's trying to spawn some distance away, wouldn't make
	// sense to get a spawn distance smaller than a regular vehicle scenario.
	if(m_bContainsVehicles)
	{
		playerInfluenceRadius = Max(playerInfluenceRadius, ScalarV(CScenarioManager::GetClusterRangeForVehicleSpawn()));
	}

	if (addExtraDistance)
	{
		playerInfluenceRadius.Setf(playerInfluenceRadius.Getf() * 1.1f); // Add ten percent
	}
}

bool CSPClusterFSMWrapper::IsClusterWithinRadius( const CPed* player, ScalarV_In playerInfluenceRadius ) const
{
	const spdSphere& cSphere = m_MyCluster->GetSphere();
	Vec3V playerPosn = player->GetTransform().GetPosition();
	spdSphere playerSphere(playerPosn, playerInfluenceRadius);
	if (CPedPopulation::ShouldUse2dDistCheck(player))
	{
		if (playerSphere.ContainsSphereFlat(cSphere) || playerSphere.IntersectsSphereFlat(cSphere))
		{
			return true;
		}
	}
	else
	{
		if (playerSphere.ContainsSphere(cSphere) || playerSphere.IntersectsSphere(cSphere))
		{
			return true;
		}
	}

	return false;
}

bool CSPClusterFSMWrapper::IsAllowedByDensity() const
{
	bool bAllowByDensity = true;
	if(m_bContainsPeds)
	{
		// Note: perhaps we wouldn't need to do this if we were simply
		// respecting the regular number of desired scenario peds. On the other
		// hand, maybe even then it would be a good thing to get a more even reduction
		// between clustered scenarios and non-clustered scenarios?

		// Get the total density multipliers for interior and exterior scenario peds.
		float fInteriorMult = 1.0f;
		float fExteriorMult = 1.0f;
		CPedPopulation::GetTotalScenarioPedDensityMultipliers(fInteriorMult, fExteriorMult);

		// Respect the interior and exterior multipliers, depending on what types of points are in this cluster.
		float fDensityMul = 1.0f;
		if(m_bContainsExteriorPoints)
		{
			fDensityMul = Min(fDensityMul, fExteriorMult);
		}
		if(m_bContainsInteriorPoints)
		{
			fDensityMul = Min(fDensityMul, fInteriorMult);
		}

		// If the density is desired density is less than 1, do a check against the
		// random value associated with this cluster, to determine if it should
		// spawn or not. We wouldn't want to do a full random roll here, since then
		// all clusters may spawn given enough time.
		if(fDensityMul < 1.0f)
		{
			unsigned int densityThreshold = (unsigned int)(fDensityMul*255.0f);
			if(m_RandomDensityThreshold >= densityThreshold)
			{
				bAllowByDensity = false;
			}
		}
	}

	return bAllowByDensity;
}

CTaskUnalerted* CSPClusterFSMWrapper::FindTaskUnalertedForDriver(CPed& ped) const
{
	CTaskUnalerted* pTaskUnalerted = static_cast<CTaskUnalerted*>(ped.GetPedIntelligence()->FindTaskDefaultByType(CTaskTypes::TASK_UNALERTED));

	if (pTaskUnalerted == NULL)
	{
		CTaskPolice* pTaskPolice = static_cast<CTaskPolice*>(ped.GetPedIntelligence()->FindTaskDefaultByType(CTaskTypes::TASK_POLICE));
		if (pTaskPolice)
		{
			pTaskUnalerted = pTaskPolice->GetTaskUnalerted();
		}
	}

	if (pTaskUnalerted == NULL)
	{
		CTaskArmy* pTaskArmy = static_cast<CTaskArmy*>(ped.GetPedIntelligence()->FindTaskDefaultByType(CTaskTypes::TASK_ARMY));
		if (pTaskArmy)
		{
			pTaskUnalerted = pTaskArmy->GetTaskUnalerted();
		}
	}

	if (pTaskUnalerted == NULL)
	{
		CTaskFirePatrol* pTaskFirePatrol = static_cast<CTaskFirePatrol*>(ped.GetPedIntelligence()->FindTaskDefaultByType(CTaskTypes::TASK_FIRE_PATROL));
		if (pTaskFirePatrol)
		{
			pTaskUnalerted = pTaskFirePatrol->GetTaskUnalerted();
		}
	}

	if (pTaskUnalerted == NULL)
	{
		CTaskAmbulancePatrol* pTaskAmbulancePatrol = static_cast<CTaskAmbulancePatrol*>(ped.GetPedIntelligence()->FindTaskDefaultByType(CTaskTypes::TASK_AMBULANCE_PATROL));
		if (pTaskAmbulancePatrol)
		{
			pTaskUnalerted = pTaskAmbulancePatrol->GetTaskUnalerted();
		}
	}

	return pTaskUnalerted;
}


void CSPClusterFSMWrapper::ClearPedTracking(CPed& ped)
{
	ped.SetPedConfigFlag(CPED_CONFIG_FLAG_IsInCluster, false);
}


void CSPClusterFSMWrapper::ClearVehicleTracking(CVehicle& veh)
{
	veh.m_nVehicleFlags.bIsInCluster = false;
	veh.m_nVehicleFlags.bIsInClusterBeingSpawned = false;
}

#if SCENARIO_DEBUG
// STATIC ---------------------------------------------------------------------

const char * CSPClusterFSMWrapper::GetStaticStateName( s16 iState  )
{
	switch (iState)
	{
	case State_Start:					return "State_Start";
	case State_ReadyForSpawnAttempt:	return "State_ReadyForSpawnAttempt";
	case State_Stream:					return "State_Stream";
	case State_SpawnVehicles:			return "State_SpawnVehicles";
	case State_SpawnPeds:				return "State_SpawnPeds";
	case State_PostSpawnUpdating:		return "State_PostSpawnUpdating";
	case State_Despawn:					return "State_Despawn";
	case State_Delay:					return "State_Delay";
	default: Assert(0);
	}

	return "State_Invalid";
}

const char * CSPClusterFSMWrapper::GetStaticFailCodeName( u8 failCode )
{
	switch (failCode)
	{
	case FC_NONE:					return "FC_None";
	case FC_RANGECHECK:				return "FC_RangeCheck";
	case FC_STREAMING_TIMEOUT:		return "FC_StreamingTimeout";
	case FC_SPAWNING_TIMEOUT:		return "FC_SpawningTimeout";
	case FC_PED_CREATION:			return "FC_PedCreation";
	case FC_NO_POINTS_FOUND:		return "FC_NoValidPointsFound";
	case FC_ALL_POINTS_NOT_VALID:	return "FC_AllPointsNotValid";
	case FC_NOT_VALID_TO_SPAWN:		return "FC_NotValidToSpawn";
	default: Assert(0);
	}

	return "FC_Invalid";
}

//-----------------------------------------------------------------------------

void CSPClusterFSMWrapper::SetFailCode(u8 failCode)
{
	m_uFailCode = failCode;
}

#endif

///////////////////////////////////////////////////////////////////////////////
//  CScenarioClustering
///////////////////////////////////////////////////////////////////////////////

CScenarioClustering::Tuning::Tuning()
		: m_MaxTimeBetweenSpawnAttempts(10.0f)
		, m_StaticBoundsAssumeLoadedDist(160.0f)
		, m_SpawnStateTimeout(3.0f)
		, m_MaxCarGenScheduleAttempts(2)
{
}

CScenarioClustering::CScenarioClustering()
		: m_MpGameInProgress(false)
{
}

void CScenarioClustering::AddCluster(CScenarioPointCluster& cluster)
{
	if (Verifyf(FindIndex(cluster) == -1, "Cluster already seems to be in the tracked list. Why add it twice?"))
	{
		CSPClusterFSMWrapper* wrapper = GetNewWrapper();
		if (wrapper)
		{
			wrapper->m_MyCluster = &cluster;
			ms_ActiveClusters.PushAndGrow(wrapper);
		}
	}
}

void CScenarioClustering::RemoveCluster(CScenarioPointCluster& cluster)
{
	const int foundId = FindIndex(cluster);
	if (foundId != -1)
	{
		CSPClusterFSMWrapper* toDelete = ms_ActiveClusters[foundId];
		ms_ActiveClusters.DeleteFast(foundId);
		delete toDelete;		
	}
}

void CScenarioClustering::RemoveCluster(CSPClusterFSMWrapper& cwrapper)
{
	Assert(cwrapper.m_MyCluster);
	RemoveCluster(*cwrapper.m_MyCluster);
}

void CScenarioClustering::ResetAllClusters()
{
	const int numClusters = ms_ActiveClusters.GetCount();
	for(int i = 0; i < numClusters; i++)
	{
		ms_ActiveClusters[i]->Reset();
	}
}

const CSPClusterFSMWrapper* CScenarioClustering::FindClusterWrapper(const CScenarioPointCluster& cluster) const
{
	int index = FindIndex(cluster);
	if(index >= 0)
	{
		return ms_ActiveClusters[index];
	}
	else
	{
		return NULL;
	}
}

void CScenarioClustering::Update()
{
	if(fwTimer::IsGamePaused())
	{
		return;
	}

	// Check if we have switched between SP and MP since the last update.
	bool mpGameInProgress = NetworkInterface::IsGameInProgress();
	if(mpGameInProgress != m_MpGameInProgress)
	{
		ResetForMpSpTransition();
		m_MpGameInProgress = mpGameInProgress;
	}

	const u32 count = ms_ActiveClusters.GetCount();
	for (u32 ac = 0; ac < count; ac++)
	{
		//FSM update ... 
		ms_ActiveClusters[ac]->Update();
	}

#if __ASSERT
	static u32 s_LastCheck;
	u32 timeMs = fwTimer::GetTimeInMilliseconds();
	if(timeMs > s_LastCheck + 2000)
	{
		ValidateInClusterFlags();
		s_LastCheck = timeMs;
	}
#endif	// __ASSERT
}

bool CScenarioClustering::RegisterCarGenVehicleData(const CScenarioPoint& point, CVehicle& vehicle)
{
	bool found = false;

	const u32 count = ms_ActiveClusters.GetCount();
	for (u32 ac = 0; ac < count; ac++)
	{
		if (ms_ActiveClusters[ac]->RegisterCarGenVehicleData(point, vehicle))
		{
			found = true;
			break;
		}
	}

	return found;
}

void CScenarioClustering::ResetForMpSpTransition()
{
	const int numClusters = ms_ActiveClusters.GetCount();
	for(int i = 0; i < numClusters; i++)
	{
		ms_ActiveClusters[i]->ResetForMpSpTransition();
	}
}

void CScenarioClustering::ResetModelRefCounts()
{
	// Loop over all the clusters and let each one clear out any model references.
	const int numClusters = ms_ActiveClusters.GetCount();
	for(int i = 0; i < numClusters; i++)
	{
		ms_ActiveClusters[i]->ResetModelRefCounts();
	}

}

#if __ASSERT

void CScenarioClustering::ValidateInClusterFlags()
{
	const int clusterCount = ms_ActiveClusters.GetCount();

	CPed::Pool* pPedPool = CPed::GetPool();
	const int maxPeds = pPedPool->GetSize();
	for(int i = 0; i < maxPeds; i++)
	{
		const CPed* pPed = pPedPool->GetSlot(i);
		if(!pPed)
		{
			continue;
		}

		if(!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsInCluster))
		{
			continue;
		}

		bool found = false;
		for(int j = 0; j < clusterCount; j++)
		{
			const CSPClusterFSMWrapper& w = *ms_ActiveClusters[j];
			if(w.IsTrackingPed(*pPed))
			{
				found = true;
				break;
			}
		}

		const Vec3V posV = pPed->GetMatrixRef().GetCol3();
		Assertf(found, "Ped %p has become orphaned from its scenario cluster (at %.1f, %.1f, %.1f).", (void*)pPed, posV.GetXf(), posV.GetYf(), posV.GetZf());
	}

	CVehicle::Pool* pVehiclePool = CVehicle::GetPool();
	const int maxVehicles = (int) pVehiclePool->GetSize();
	for(int i = 0; i < maxVehicles; i++)
	{
		const CVehicle* pVehicle = pVehiclePool->GetSlot(i);
		if(!pVehicle)
		{
			continue;
		}

		if(!pVehicle->m_nVehicleFlags.bIsInCluster)
		{
			continue;
		}

		bool found = false;
		for(int j = 0; j < clusterCount; j++)
		{
			const CSPClusterFSMWrapper& w = *ms_ActiveClusters[j];
			if(w.IsTrackingVehicle(*pVehicle))
			{
				found = true;
				break;
			}
		}
		const Vec3V posV = pVehicle->GetMatrixRef().GetCol3();
		Assertf(found, "Vehicle %p has become orphaned from its scenario cluster (at %.1f, %.1f, %.1f).", (void*)pVehicle, posV.GetXf(), posV.GetYf(), posV.GetZf());
	}
}

#endif	// __ASSERT

#if SCENARIO_DEBUG

void CScenarioClustering::AddStaticWidgets(bkBank& bank)
{
	bank.AddSlider("Max Time Between Spawn Attempts", &m_Tuning.m_MaxTimeBetweenSpawnAttempts, 0.0f, 100.0f, 0.1f);
	bank.AddSlider("Static Bounds Assume Loaded Dist", &m_Tuning.m_StaticBoundsAssumeLoadedDist, 0.0f, 2000.0f, 5.0f);
	bank.AddSlider("Spawn State Timeout", &m_Tuning.m_SpawnStateTimeout, 0.0f, 120.0f, 0.1f);
	bank.AddSlider("Max Car Gen Schedule Attempts", &m_Tuning.m_MaxCarGenScheduleAttempts, 0, 255, 1);
}

void CScenarioClustering::AddWidgets(CScenarioPointCluster& cluster, bkGroup* parentGroup)
{
	const int foundId = FindIndex(cluster);
	if (foundId != -1)
	{
		ms_ActiveClusters[foundId]->AddWidgets(parentGroup);
	}
}

void CScenarioClustering::TriggerSpawn(CScenarioPointCluster& cluster)
{
	const int foundId = FindIndex(cluster);
	if (foundId != -1)
	{
		ms_ActiveClusters[foundId]->TriggerSpawn();
	}
}

void CScenarioClustering::DebugRender() const
{
	const u32 count = ms_ActiveClusters.GetCount();
	for (u32 ac = 0; ac < count; ac++)
	{
		ms_ActiveClusters[ac]->RenderInfo();
	}
}

#if DR_ENABLED
void CScenarioClustering::DebugReport(debugPlayback::TextOutputVisitor& output) const
{
	const u32 count = ms_ActiveClusters.GetCount();
	for (u32 ac = 0; ac < count; ac++)
	{
		ms_ActiveClusters[ac]->DebugReport(output);
	}
}
#endif //DR_ENABLED

#endif

int CScenarioClustering::FindIndex(const CScenarioPointCluster& cluster) const
{
	const u32 count = ms_ActiveClusters.GetCount();
	for (u32 ac = 0; ac < count; ac++)
	{
		if (ms_ActiveClusters[ac]->m_MyCluster == &cluster)
		{
			return ac;
		}
	}

	return -1;
}

CSPClusterFSMWrapper* CScenarioClustering::GetNewWrapper()
{
	const s32 curSpace = CSPClusterFSMWrapper::_ms_pPool->GetNoOfFreeSpaces();

	// In order to prevent crashes with likely data loss if exceeding the pool limits,
	// we check before trying to use the pools. If there is not enough space, we notify
	// the user and return NULL, which the calling code needs to handle.
	if(!curSpace)
	{
#if __ASSERT
		Assertf(0,	"CSPClusterFSMWrapper pool is full. Size %d", CSPClusterFSMWrapper::_ms_pPool->GetSize());
#else
		Errorf(		"CSPClusterFSMWrapper pool is full. Size %d", CSPClusterFSMWrapper::_ms_pPool->GetSize());
#endif
		return NULL;
	}

	const s32 warningLimit = 5;
	if(curSpace <= warningLimit)
	{
#if __ASSERT
		Assertf(0,	"CSPClusterFSMWrapper pool has %d of %d possible slots left.", CSPClusterFSMWrapper::_ms_pPool->GetNoOfFreeSpaces(), CSPClusterFSMWrapper::_ms_pPool->GetSize());
#else
		Errorf(		"CSPClusterFSMWrapper pool has %d of %d possible slots left.", CSPClusterFSMWrapper::_ms_pPool->GetNoOfFreeSpaces(), CSPClusterFSMWrapper::_ms_pPool->GetSize());
#endif
	}

	CSPClusterFSMWrapper* newPoint = rage_new CSPClusterFSMWrapper();
	return newPoint;
}
