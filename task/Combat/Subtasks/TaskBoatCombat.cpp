// FILE :    TaskBoatCombat.h
// PURPOSE : Subtask of combat used for boat combat

// File header
#include "Task/Combat/Subtasks/TaskBoatCombat.h"

// Rage headers
#include "ai/navmesh/pathserverbase.h"
#include "grcore/debugdraw.h"

// Game headers
#include "Peds/Ped.h"
#include "task/Combat/Subtasks/TaskBoatChase.h"
#include "task/Combat/Subtasks/TaskBoatStrafe.h"
#include "task/Combat/Subtasks/TaskVehicleCombat.h"
#include "task/Vehicle/TaskCar.h"
#include "vehicleAi/VehicleIntelligence.h"
#include "vehicleAi/task/TaskVehicleCruise.h"
#include "vehicleAi/task/TaskVehicleTempAction.h"
#include "vehicleAi/VehicleIntelligence.h"
#include "Vehicles/boat.h"
#include "Vehicles/vehicle.h"

AI_OPTIMISATIONS()

//=========================================================================
// CTaskBoatCombat
//=========================================================================

CTaskBoatCombat::Tunables CTaskBoatCombat::sm_Tunables;
IMPLEMENT_COMBAT_TASK_TUNABLES(CTaskBoatCombat, 0xb083954e);

CTaskBoatCombat::CTaskBoatCombat(const CAITarget& rTarget)
: m_CollisionProbeResults()
, m_LandProbeResults()
, m_NavMeshRouteSearchHelper()
, m_Target(rTarget)
, m_uRunningFlags(0)
{
	SetInternalTaskType(CTaskTypes::TASK_BOAT_COMBAT);
}

CTaskBoatCombat::~CTaskBoatCombat()
{
}

#if !__FINAL
void CTaskBoatCombat::Debug() const
{
	CTask::Debug();
}

const char* CTaskBoatCombat::GetStaticStateName(s32 iState)
{
	Assert(iState >= State_Start && iState <= State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_Chase",
		"State_Strafe",
		"State_Wait",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif // !__FINAL

CBoat* CTaskBoatCombat::GetBoat() const
{
	//Grab the ped.
	const CPed* pPed = GetPed();
	
	//Ensure the ped is in a vehicle.
	CVehicle* pVehicle = pPed->GetVehiclePedInside();
	if(!pVehicle)
	{
		return NULL;
	}
	
	//Ensure the vehicle is a boat.
	if(!pVehicle->InheritsFromBoat())
	{
		return NULL;
	}
	
	return static_cast<CBoat *>(pVehicle);
}

void CTaskBoatCombat::ProcessProbes()
{
	//Grab the boat.
	const CBoat* pBoat = GetBoat();

	//Grab the boat position.
	Vec3V vBoatPosition = pBoat->GetTransform().GetPosition();

	//Grab the boat velocity.
	Vec3V vBoatVelocity = VECTOR3_TO_VEC3V(pBoat->GetVelocity());
	vBoatVelocity.SetZ(ScalarV(V_ZERO));

	//Calculate the look-ahead position.
	Vec3V vLookAheadPosition = AddScaled(vBoatPosition, vBoatVelocity, ScalarVFromF32(sm_Tunables.m_TimeToLookAheadForCollision));

	//Check if the collision probe is not waiting or ready.
	if(!m_CollisionProbeResults.GetResultsWaitingOrReady())
	{
#if DEBUG_DRAW
		if(sm_Tunables.m_Rendering.m_Enabled && sm_Tunables.m_Rendering.m_CollisionProbe)
		{
			grcDebugDraw::Line(vBoatPosition, vLookAheadPosition, Color_red, -1);
		}
#endif

		//Create the probe.
		WorldProbe::CShapeTestProbeDesc probeDesc;
		probeDesc.SetResultsStructure(&m_CollisionProbeResults);
		probeDesc.SetStartAndEnd(VEC3V_TO_VECTOR3(vBoatPosition), VEC3V_TO_VECTOR3(vLookAheadPosition));
		probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
		
		//Start the test.
		WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_ASYNCHRONOUS_TEST);
	}
	else
	{
		//Check if the probe is waiting.
		if(m_CollisionProbeResults.GetWaitingOnResults())
		{
			//Nothing to do, we are waiting on the results.
		}
		else
		{
			//Update the flag.
			m_uRunningFlags.ChangeFlag(RF_IsCollisionImminent, (m_CollisionProbeResults.GetNumHits() != 0));
			
			//Reset the results.
			m_CollisionProbeResults.Reset();
		}
	}

	if(pBoat->GetIntelligence()->m_bSetBoatIgnoreLandProbes)
	{
		m_uRunningFlags.ChangeFlag(RF_IsLandImminent, false);
	}
	else
	{
		//Check if the land probe is not waiting or ready.
		if(!m_LandProbeResults.GetResultsWaitingOrReady())
		{
			//Calculate the land position.
			Vec3V vLandPosition = vLookAheadPosition;
			vLandPosition.SetZf(vLookAheadPosition.GetZf() - sm_Tunables.m_DepthForLandProbe);

#if DEBUG_DRAW
			if(sm_Tunables.m_Rendering.m_Enabled && sm_Tunables.m_Rendering.m_LandProbe)
			{
				grcDebugDraw::Line(vLookAheadPosition, vLandPosition, Color_red, -1);
			}
#endif

			//Create the probe.
			WorldProbe::CShapeTestProbeDesc probeDesc;
			probeDesc.SetResultsStructure(&m_LandProbeResults);
			probeDesc.SetStartAndEnd(VEC3V_TO_VECTOR3(vLookAheadPosition), VEC3V_TO_VECTOR3(vLandPosition));
			probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);

			//Start the test.
			WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_ASYNCHRONOUS_TEST);
		}
		else
		{
			//Check if the probe is waiting.
			if(m_LandProbeResults.GetWaitingOnResults())
			{
				//Nothing to do, we are waiting on the results.
			}
			else
			{
				//Update the flag.
				m_uRunningFlags.ChangeFlag(RF_IsLandImminent, (m_LandProbeResults.GetNumHits() != 0));

				//Reset the results.
				m_LandProbeResults.Reset();
			}
		}
	}
}

void CTaskBoatCombat::ProcessTargetUnreachable()
{
	//Check if the search is not active.
	if(!m_NavMeshRouteSearchHelper.IsSearchActive())
	{
		//Grab the boat.
		const CBoat* pBoat = GetBoat();
		
		//Set the entity radius.
		float fRadius = pBoat->GetBaseModelInfo()->GetBoundingSphereRadius();
		m_NavMeshRouteSearchHelper.SetEntityRadius(fRadius);
		
		//Grab the boat position.
		Vec3V vBoatPosition = pBoat->GetTransform().GetPosition();
		
		//Grab the target position.
		Vector3 vTargetPosition;
		m_Target.GetPosition(vTargetPosition);
		
		//Generate the flags.
		u64 uFlags = PATH_FLAG_USE_LARGER_SEARCH_EXTENTS|PATH_FLAG_NEVER_LEAVE_WATER;
		
		//Start a search.
		m_NavMeshRouteSearchHelper.StartSearch(GetPed(), VEC3V_TO_VECTOR3(vBoatPosition), vTargetPosition, 0.0f, uFlags);
	}
	else
	{
		//Check if the search is complete.
		SearchStatus nSearchStatus;
		float fDistance;
		Vector3 vLastPoint;
		if(m_NavMeshRouteSearchHelper.GetSearchResults(nSearchStatus, fDistance, vLastPoint))
		{
			//Keep track of whether the target is unreachable.
			bool bIsTargetUnreachable = true;
			
			//Check if the search was successful.
			if(nSearchStatus == SS_SearchSuccessful)
			{
				bIsTargetUnreachable = false;
			}
			
			//Change the flag.
			m_uRunningFlags.ChangeFlag(RF_IsTargetUnreachable, bIsTargetUnreachable);
			
			//Reset the search.
			m_NavMeshRouteSearchHelper.ResetSearch();
		}
	}
}

bool CTaskBoatCombat::ShouldChase() const
{
	//Ensure the entity is valid.
	const CEntity* pEntity = m_Target.GetEntity();
	if(!pEntity)
	{
		return false;
	}
	
	//Ensure the entity is a ped.
	if(!pEntity->GetIsTypePed())
	{
		return false;
	}
	
	//Grab the ped.
	const CPed* pPed = static_cast<const CPed *>(pEntity);
	
	//Ensure the ped is in a vehicle.
	const CVehicle* pVehicle = pPed->GetVehiclePedInside();
	if(!pVehicle)
	{
		return false;
	}
	
	//Ensure the vehicle speed exceeds the threshold.
	float fSpeedSq = pVehicle->GetVelocity().Mag2();
	float fMinSpeedSq = square(sm_Tunables.m_MinSpeedForChase);
	if(fSpeedSq < fMinSpeedSq)
	{
		return false;
	}
	
	return true;
}

bool CTaskBoatCombat::ShouldStrafe() const
{
	return !ShouldChase();
}

bool CTaskBoatCombat::ShouldWait() const
{
	//Ensure our target is unreachable.
	if(!m_uRunningFlags.IsFlagSet(RF_IsTargetUnreachable))
	{
		return false;
	}
	
	//Ensure a collision, or land, is imminent.
	if(!m_uRunningFlags.IsFlagSet(RF_IsCollisionImminent) && !m_uRunningFlags.IsFlagSet(RF_IsLandImminent))
	{
		return false;
	}
	
	return true;
}

CTask::FSM_Return CTaskBoatCombat::ProcessPreFSM()
{
	//Ensure the boat is valid.
	if(!GetBoat())
	{
		return FSM_Quit;
	}
	
	//Ensure the target is valid.
	if(!m_Target.GetIsValid())
	{
		return FSM_Quit;
	}
	
	//Process the probes.
	ProcessProbes();
	
	//Process whether the target is unreachable.
	ProcessTargetUnreachable();
	
	return FSM_Continue;
}

CTask::FSM_Return CTaskBoatCombat::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
	
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();
				
		FSM_State(State_Chase)
			FSM_OnEnter
				Chase_OnEnter();
			FSM_OnUpdate
				return Chase_OnUpdate();
				
		FSM_State(State_Strafe)
			FSM_OnEnter
				Strafe_OnEnter();
			FSM_OnUpdate
				return Strafe_OnUpdate();
				
		FSM_State(State_Wait)
			FSM_OnEnter
				Wait_OnEnter();
			FSM_OnUpdate
				return Wait_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskBoatCombat::Start_OnUpdate()
{
	//Check if we should chase.
	if(ShouldChase())
	{
		//Move to the chase state.
		SetState(State_Chase);
	}
	//Check if we should strafe.
	else if(ShouldStrafe())
	{
		//Move to the strafe state.
		SetState(State_Strafe);
	}
	else
	{
		taskAssertf(false, "The boat does not want to chase or strafe.");
	}
	
	return FSM_Continue;
}

void CTaskBoatCombat::Chase_OnEnter()
{
	//Create the task.
	CTask* pTask = rage_new CTaskBoatChase(m_Target);
	
	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskBoatCombat::Chase_OnUpdate()
{
	//Check if the target is unreachable.
	if(ShouldWait())
	{
		//Move to the wait state.
		SetState(State_Wait);
	}
	//Check if we should strafe.
	else if(ShouldStrafe())
	{
		//Move to the strafe state.
		SetState(State_Strafe);
	}
	//Check if the sub-task has finished.
	else if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Finish the task.
		SetState(State_Finish);
	}
	
	return FSM_Continue;
}

void CTaskBoatCombat::Strafe_OnEnter()
{
	//TODO: There should probably be something controlling the radius,
	//		to ensure boats don't hit each other.
	
	//Generate the radius.
	float fRadius = fwRandom::GetRandomNumberInRange(15.0f, 25.0f);
	
	//Create the task.
	CTask* pTask = rage_new CTaskBoatStrafe(m_Target, fRadius);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskBoatCombat::Strafe_OnUpdate()
{
	//Check if the target is unreachable.
	if(ShouldWait())
	{
		//Move to the wait state.
		SetState(State_Wait);
	}
	//Check if we should chase.
	else if(ShouldChase())
	{
		//Move to the chase state.
		SetState(State_Chase);
	}
	//Check if the sub-task has finished.
	else if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Finish the task.
		SetState(State_Finish);
	}
	
	return FSM_Continue;
}

void CTaskBoatCombat::Wait_OnEnter()
{
	//Grab the boat.
	CBoat* pBoat = GetBoat();
	
	//Create the vehicle task.
	u32 uTime = NetworkInterface::GetSyncedTimeInMilliseconds() + (u32)(sm_Tunables.m_TimeToWait * 1000.0f);
	CTaskVehicleBrake* pVehicleTask = rage_new CTaskVehicleBrake(uTime);
	pVehicleTask->SetUseHandBrake(true);
	pVehicleTask->SetUseReverse(true);
	
	//Create the sub-task.
	CTask* pSubTask = rage_new CTaskVehicleCombat(&m_Target);
	
	//Create the task.
	CTask* pTask = rage_new CTaskControlVehicle(pBoat, pVehicleTask, pSubTask);
	
	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskBoatCombat::Wait_OnUpdate()
{
	//Check if the sub-task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Check if we should chase.
		if(ShouldChase())
		{
			//Move to the chase state.
			SetState(State_Chase);
		}
		//Check if we should strafe.
		else if(ShouldStrafe())
		{
			//Move to the strafe state.
			SetState(State_Strafe);
		}
		else
		{
			//Finish the task.
			SetState(State_Finish);
		}
	}
	
	return FSM_Continue;
}
