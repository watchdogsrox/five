//
// task/Scenario/ScenarioClustering.h
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef INC_SCENARIO_CLUSTERING_H
#define INC_SCENARIO_CLUSTERING_H

// Rage headers

// Framework headers
#include "fwsys/fsm.h"

// Game headers
#include "task/Scenario/ScenarioChaining.h"
#include "task/Scenario/ScenarioManager.h"
#include "task/Scenario/ScenarioPointContainer.h"

class CScenarioPointChainUseInfo;
class CScenarioPointLookUps;
class CTaskUnalerted;

class CScenarioPointCluster
{
public:

	static const u16 INVALID_ID = 0xffff;

	CScenarioPointCluster();
	~CScenarioPointCluster();

	u32 GetNumPoints() const;
	const CScenarioPoint& GetPoint(const u32 index) const;
	CScenarioPoint& GetPoint(const u32 index);
	void PostLoad(const CScenarioPointLoadLookUps& lookups);
	void CleanUp();

	const spdSphere& GetSphere() const;
	float GetSpawnDelay() const;
	bool GetAllPointRequiredForSpawn() const;

#if SCENARIO_DEBUG
	void AddWidgets(bkGroup* parentGroup);
	void PrepSaveObject(const CScenarioPointCluster& prepFrom, CScenarioPointLookUps& out_LookUps);
	int AddPoint(CScenarioPoint& point);
	CScenarioPoint* RemovePoint(const int pointIndex);

	void CallOnAddOnEachScenarioPoint() { m_Points.CallOnAddOnEachScenarioPoint(); }
	int CallOnRemoveOnEachScenarioPoint() { return m_Points.CallOnRemoveOnEachScenarioPoint(); }
#endif

private:

#if SCENARIO_DEBUG
	void UpdateSphere();
	ScalarV_Out FindPointSpawnRadius(const CScenarioPoint& point) const;
#endif

	CScenarioPointContainer m_Points;
	spdSphere m_ClusterSphere;
	float m_fNextSpawnAttemptDelay; //amount to delay by
	bool m_bAllPointRequiredForSpawn;

	PAR_SIMPLE_PARSABLE;
};

//-----------------------------------------------------------------------------

struct CScenarioClusterSpawnedTrackingData
{
	CScenarioClusterSpawnedTrackingData()
		: m_Ped(NULL)
		, m_Vehicle(NULL)
		, m_Point(NULL)
		, m_ChainUser(NULL)
		, m_ScenarioTypeReal(0)
		, m_RefCountedModelId(fwModelId::MI_INVALID)
		, m_NumCarGenSchedules(0)
		, m_NeedsAddToWorld(false)
		, m_UsesCarGen(false)
	{
	}

	~CScenarioClusterSpawnedTrackingData()
	{
		// Peds and vehicles need to be cleanly untracked, if these asserts fail we may have
		// left an "in cluster" flag on, which could prevent the entity from being destroyed, indefinitely.
		Assert(!m_Ped);
		Assert(!m_Vehicle);

		// Reset any streaming reference count we may be hold on to.
		ResetRefCountedModel();

		m_ChainUser = NULL; //reset this pointer....
	}

	FW_REGISTER_CLASS_POOL(CScenarioClusterSpawnedTrackingData);

	// PURPOSE:	Add a ref count to the given model, and keep track of it reliably.
	void RefCountModel(u32 modelId);

	// PURPOSE:	Reset any streaming reference count we may be hold on to, from a call to RefCountModel().
	void ResetRefCountedModel();

	RegdPed m_Ped;
	RegdVeh m_Vehicle;
	RegdScenarioPnt m_Point;

	//Setup Data
	CScenarioManager::PedTypeForScenario m_PedType;
	CScenarioPointChainUseInfoPtr m_ChainUser;
	u32 m_ScenarioTypeReal;

	// PURPOSE:	fwModelId for a model that we are holding a reference count on, to prevent it
	//			from streaming out, or fwModelId::MI_INVALID.
	// NOTES:	Currently, this is only used for ped models.
	u32 m_RefCountedModelId;

	// PURPOSE:	The number of times during this spawn attempt that we have scheduled
	//			a car generator.
	u8 m_NumCarGenSchedules;

	bool m_NeedsAddToWorld;
	bool m_UsesCarGen;
};

//-----------------------------------------------------------------------------

class CSPClusterFSMWrapper : public fwFsm
{
public:
	CSPClusterFSMWrapper();
	~CSPClusterFSMWrapper();

	FW_REGISTER_CLASS_POOL(CSPClusterFSMWrapper);

	void Reset();

	// PURPOSE:	Reset things as necessary for a transition between single player and
	//			multiplayer, or the other way around.
	void ResetForMpSpTransition();

	// PURPOSE:	Clear out any ref counts that scenario clusters hold on any ped or
	//			vehicle models.
	void ResetModelRefCounts();

	bool RegisterCarGenVehicleData(const CScenarioPoint& point, CVehicle& vehicle);

	// PURPOSE:	Check if it would be OK to remove a car generator from this cluster
	//			at this time, due to time of day, etc.
	bool AllowedToRemoveCarGensFromCurrentState() const
	{
		const int state = GetState();

		// Don't allow car generators to be removed while we are trying to stream/spawn.
		// We create the vehicles during State_SpawnVehicles now, but finalize the
		// setup from State_SpawnPeds.
		if(state == State_Stream || state == State_SpawnVehicles || state == State_SpawnPeds)
		{
			return false;
		}
		return true;
	}

#if __ASSERT
	bool IsTrackingPed(const CPed& ped) const;
	bool IsTrackingVehicle(const CVehicle& veh) const;
#endif	// __ASSERT

#if SCENARIO_DEBUG
	void AddWidgets(bkGroup* parentGroup);
	void TriggerSpawn();
	void RenderInfo() const;
#if DR_ENABLED
	void DebugReport(debugPlayback::TextOutputVisitor& output) const;
#endif
	bool TryingDebugSpawn() const { return m_DebugSpawn; }
	void SetMyCluster(CScenarioPointCluster* ptr) { m_MyCluster = ptr; }
#endif

private:
	friend class CScenarioClustering; //Mainly for access to the m_MyCluster

	enum
	{
		State_Start = 0,
		State_ReadyForSpawnAttempt,
		State_Stream,
		State_SpawnVehicles,
		State_SpawnPeds,
		State_PostSpawnUpdating,
		State_Despawn,
		State_Delay,
	};

	// PURPOSE:	These are basically sub-states within State_SpawnPeds.
	enum
	{
		SpawnPedsPassInvalid,
		SpawnPedsPass1,
		SpawnPedsPass2,
		SpawnPedsPass3
	};

	fwFsm::Return UpdateState(const s32 state, const s32 iMessage, const fwFsm::Event event);

	//State machine functions
	fwFsm::Return Start_OnUpdate();
	fwFsm::Return ReadyForSpawnAttempt_OnEnter();
	fwFsm::Return ReadyForSpawnAttempt_OnUpdate();
	fwFsm::Return Stream_OnEnter();
	fwFsm::Return Stream_OnUpdate();
	fwFsm::Return SpawnVehicles_OnEnter();
	fwFsm::Return SpawnVehicles_OnUpdate();
	fwFsm::Return SpawnPeds_OnEnter();
	fwFsm::Return SpawnPeds_OnUpdate();
	fwFsm::Return SpawnPeds_OnExit();
	fwFsm::Return SpawnPeds_OnUpdatePass1();
	fwFsm::Return SpawnPeds_OnUpdatePass2();
	fwFsm::Return SpawnPeds_OnUpdatePass3();
	fwFsm::Return PostSpawnUpdating_OnUpdate();
	fwFsm::Return Despawn_OnUpdate();
	fwFsm::Return Delay_OnUpdate();

	//Support functions
	bool ValidatePointAndSelectRealType(const CScenarioPoint& point, int& scenarioTypeReal_OUT);
	bool ValidateCarGenPoint(const CScenarioPoint& point);
	void GatherStreamingDependencies(const CScenarioPoint& point);
	bool RequestStreamingDependencies(bool scheduleCarGens);
	void SpawnAtPoint(CScenarioClusterSpawnedTrackingData& pntDep);
	void StreamAndScheduleCarGenPoint(CScenarioClusterSpawnedTrackingData& pntDep, bool allowSchedule, bool& readyToScheduleOut);
	void DespawnAndClearTrackedData(bool forceDespawn, bool mustClearAllTracked);
	//void UpdateTracked(CScenarioClusterSpawnedTrackingData& tracked);
	bool IsTrackedMemberReadyToBeRemoved(CScenarioClusterSpawnedTrackingData& tracked);
	bool IsPedReadyToBeRemoved(CPed& ped) const;
	bool IsVehicleReadyToBeRemoved(CVehicle& veh) const;
	bool IsInvalid(const CScenarioClusterSpawnedTrackingData& tracked) const;
	float GetStreamingTimeOut() const;
	bool CouldSpawnAt(const CScenarioPoint& point) const;
	bool IsStillValidToSpawn() const;
	bool CheckVisibilityForPed(const CScenarioPoint& point, int scenarioTypeReal) const;
	bool CheckVisibilityForVehicle(const CScenarioPoint& point) const;

	void GetPlayerInfluenceRadius(ScalarV_InOut playerInfluenceRadius, bool addExtraDistance = false) const;
	bool IsClusterWithinRadius( const CPed* player, ScalarV_In playerInfluenceRadius ) const ;
	bool IsAllowedByDensity() const;

	CTaskUnalerted* FindTaskUnalertedForDriver(CPed& ped) const;

	static void ClearPedTracking(CPed& ped);
	static void ClearVehicleTracking(CVehicle& veh);

#if SCENARIO_DEBUG
	static const char * GetStaticStateName( s16 );
	static const char * GetStaticFailCodeName( u8 );
	void SetFailCode(u8 failCode);
#endif

	static float ms_UntrackDist;
	// Time in seconds between checks to despawn this cluster
	static float ms_fNextDespawnCheckDelay;

	CScenarioPointCluster* m_MyCluster;
	atArray<CScenarioClusterSpawnedTrackingData*> m_TrackedData;

	// PURPOSE:	Time in seconds until we try again. This gets reset to some default value when we begin each
	//			spawn attempt, but in some cases, we may not want to wait for that long before trying again,
	//			if we fail.
	float m_fNextSpawnAttemptDelay;

	// PURPOSE:	When in State_SpawnPeds, this indicates which pass over the points we are working on.
	u8 m_SpawnPedsPass;

	// PURPOSE:	When in State_SpawnPeds, this keeps track of the progress on the current pass
	//			(probably the next index in m_TrackedData that we want to process).
	u8 m_SpawnPedsProgress;

	u8 m_RandomDensityThreshold;	// Random number between 0 and 255, used to implement lower density support.

	bool m_bContainsExteriorPoints;	// The cluster contains spawnable points in exteriors.
	bool m_bContainsInteriorPoints;	// The cluster contains spawnable points in interiors.
	bool m_bContainsPeds;			// The cluster may have one or more ped scenarios.
	bool m_bContainsVehicles;		// The cluster may have (or desires) one or more vehicle scenarios in it.
	bool m_bDespawnImmediately;		// The cluster needs to switch to its despawn state ASAP.
	bool m_bExtendedRange;			// The points have been marked to spawn at an extended range.

	// PURPOSE:	Gets set to true by StreamAndScheduleCarGenPoint() if we have made too many attempts at scheduling a car generator.
	bool m_bMaxCarGenSpawnAttempsExceeded;


#if __ASSERT
	bool m_bWasInMPWhenEnteringSpawnState;
#endif

#if SCENARIO_DEBUG
	enum
	{ 
		FC_NONE,
		FC_RANGECHECK,
		FC_STREAMING_TIMEOUT,
		FC_SPAWNING_TIMEOUT,
		FC_PED_CREATION,
		FC_NO_POINTS_FOUND,
		FC_ALL_POINTS_NOT_VALID,
		FC_NOT_VALID_TO_SPAWN
	};
	u8	m_uFailCode;

	bool m_DebugSpawn;
#endif
};


//-----------------------------------------------------------------------------

//This should be a fully static class.
class CScenarioClustering
{
public:
	struct Tuning
	{
		Tuning();

		// PURPOSE:	Upper limit to how large of a m_fNextSpawnAttemptDelay we accept.
		float	m_MaxTimeBetweenSpawnAttempts;

		// PURPOSE:	Distance around the player within which we assume bounds to be loaded.
		float	m_StaticBoundsAssumeLoadedDist;

		// PURPOSE:	Max time spent in State_SpawnVehicles. This basically just has to be large enough
		//			to allow the car generators to go through the scheduled queue and place
		//			the vehicles on ground.
		float	m_SpawnStateTimeout;

		// PURPOSE:	Max number of times we can schedule a car generator to spawn. If nothing has
		//			spawned after this, we give up, trying more times usually doesn't help.
		u8		m_MaxCarGenScheduleAttempts;
	};

	explicit CScenarioClustering();

	void AddCluster(CScenarioPointCluster& cluster);
	void RemoveCluster(CScenarioPointCluster& cluster);
	void RemoveCluster(CSPClusterFSMWrapper& cwrapper);
	void ResetAllClusters();

	// PURPOSE:	Look through the cluster wrappers and see if we have one
	//			set up for the specified cluster.
	const CSPClusterFSMWrapper* FindClusterWrapper(const CScenarioPointCluster& cluster) const;

	void Update();
	bool RegisterCarGenVehicleData(const CScenarioPoint& point, CVehicle& vehicle);

	const Tuning& GetTuning() { return m_Tuning; }

	// PURPOSE:	Reset things as necessary for a transition between single player and
	//			multiplayer, or the other way around.
	void ResetForMpSpTransition();

	// PURPOSE:	Clear out any ref counts that scenario clusters hold on any ped or
	//			vehicle models.
	void ResetModelRefCounts();

#if __ASSERT
	void ValidateInClusterFlags();
#endif

#if SCENARIO_DEBUG
	void AddStaticWidgets(bkBank& bank);
	void AddWidgets(CScenarioPointCluster& cluster, bkGroup* parentGroup);
	void TriggerSpawn(CScenarioPointCluster& cluster);
	void DebugRender() const;
#if DR_ENABLED
	void DebugReport(debugPlayback::TextOutputVisitor& output) const;
#endif
#endif

private:
	friend class CScenarioPointRegionEditInterface;	// For FindIndex() and ms_ActiveClusters.

	int FindIndex(const CScenarioPointCluster& cluster) const;

	static CSPClusterFSMWrapper* GetNewWrapper();

	atArray<CSPClusterFSMWrapper*> ms_ActiveClusters;
	Tuning	m_Tuning;

	// PURPOSE:	Value of NetworkInterface::IsGameInProgress() during the last update, used to
	//			detect transitions.
	bool	m_MpGameInProgress;
};

typedef atSingleton<CScenarioClustering> CScenarioClusteringSingleton;
#define SCENARIOCLUSTERING CScenarioClusteringSingleton::InstanceRef()
#define INIT_SCENARIOCLUSTERING											\
	do {																\
	if (!CScenarioClusteringSingleton::IsInstantiated()){			\
	CScenarioClusteringSingleton::Instantiate();					\
	}																\
	} while(0)															\
	//END
#define SHUTDOWN_SCENARIOCLUSTERING CScenarioClusteringSingleton::Destroy()

#endif //INC_SCENARIO_CLUSTERING_H
