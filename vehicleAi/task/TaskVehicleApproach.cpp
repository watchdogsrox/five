// File header
#include "TaskVehicleApproach.h"

// Game headers
#include "game/Dispatch/DispatchHelpers.h"
#include "game/Dispatch/DispatchServices.h"
#include "peds/ped.h"
#include "Vehicles/Automobile.h"
#include "Vehicles/vehicle.h"
#include "vehicleAi/pathfind.h"
#include "vehicleAi/task/TaskVehicleCruise.h"
#include "vehicleAi/task/TaskVehicleGoToAutomobile.h"
#include "vehicleAi/task/TaskVehicleTempAction.h"
#include "VehicleAi/VehicleIntelligence.h"

AI_OPTIMISATIONS()

//=========================================================================
// CTaskVehicleApproach
//=========================================================================

CTaskVehicleApproach::Tunables CTaskVehicleApproach::sm_Tunables;
IMPLEMENT_VEHICLE_AI_TASK_TUNABLES(CTaskVehicleApproach, 0x98584c61);

CTaskVehicleApproach::CTaskVehicleApproach(const sVehicleMissionParams& rParams)
: CTaskVehicleMissionBase(rParams)
{
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_APPROACH);
}

CTaskVehicleApproach::~CTaskVehicleApproach()
{

}

bool CTaskVehicleApproach::GetTargetPosition(Vec3V_InOut vPosition) const
{
	//Ensure the sub-task is valid.
	const CTask* pSubTask = GetSubTask();
	if(!pSubTask)
	{
		return false;
	}

	//Find the target coors.
	taskAssert(pSubTask->IsVehicleMissionTask());
	static_cast<const CTaskVehicleMissionBase *>(pSubTask)->FindTargetCoors(GetVehicle(), RC_VECTOR3(vPosition));

	return true;
}

#if !__FINAL
void CTaskVehicleApproach::Debug() const
{
	CTask::Debug();
}

const char* CTaskVehicleApproach::GetStaticStateName(s32 iState)
{
	Assert(iState >= State_Start && iState <= State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_ApproachTarget",
		"State_ApproachClosestRoadNode",
		"State_Wait",
		"State_Finish"
	};
	
	return aStateNames[iState];
}
#endif // !__FINAL

aiTask* CTaskVehicleApproach::Copy() const
{
	return rage_new CTaskVehicleApproach(m_Params);
}

CTask::FSM_Return CTaskVehicleApproach::ProcessPreFSM()
{
	//Ensure the target is valid.
	if(!m_Params.IsTargetValid())
	{
		return FSM_Quit;
	}

	//Process the closest road node.
	ProcessClosestRoadNode();

	return FSM_Continue;
}

CTask::FSM_Return CTaskVehicleApproach::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
	
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();
		
		FSM_State(State_ApproachTarget)
			FSM_OnEnter
				ApproachTarget_OnEnter();
			FSM_OnUpdate
				return ApproachTarget_OnUpdate();
			
		FSM_State(State_ApproachClosestRoadNode)
			FSM_OnEnter
				ApproachClosestRoadNode_OnEnter();
			FSM_OnUpdate
				return ApproachClosestRoadNode_OnUpdate();

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

CTask::FSM_Return CTaskVehicleApproach::Start_OnUpdate()
{
	//Approach the target.
	SetState(State_ApproachTarget);
	
	return FSM_Continue;
}

void CTaskVehicleApproach::ApproachTarget_OnEnter()
{
	//Create the task.
	CTask* pTask = CVehicleIntelligence::CreateAutomobileGotoTask(m_Params, sVehicleMissionParams::DEFAULT_SWITCH_TO_STRAIGHT_LINE_DIST);
	
	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskVehicleApproach::ApproachTarget_OnUpdate()
{
	//Update the cruise speed.
	UpdateCruiseSpeed();

	//Check if the closest road node is valid.
	if(!m_ClosestRoadNode.IsEmpty())
	{
		//Approach the closest road node.
		SetState(State_ApproachClosestRoadNode);
	}
	//Check if the sub-task has finished.
	else if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Check if we can terminate.
		if(!m_Params.m_iDrivingFlags.IsFlagSet(DF_DontTerminateTaskWhenAchieved))
		{
			//Finish the task.
			SetState(State_Finish);
		}
		else
		{
			//Wait in the vehicle.
			SetState(State_Wait);
		}
	}
	
	return FSM_Continue;
}

void CTaskVehicleApproach::ApproachClosestRoadNode_OnEnter()
{
	//Generate the target position near the closest road node.
	Vec3V vTargetPosition;
	if(!GeneratePositionNearClosestRoadNode(vTargetPosition))
	{
		return;
	}
	
	//Create the params.
	sVehicleMissionParams params(m_Params);
	params.SetTargetPosition(VEC3V_TO_VECTOR3(vTargetPosition));
	params.m_iDrivingFlags.SetFlag(DF_TargetPositionOverridesEntity);

	//Check if the closest road node is a dead end.
	taskAssert(!m_ClosestRoadNode.IsEmpty());
	const CPathNode* pNode = ThePaths.FindNodePointerSafe(m_ClosestRoadNode);
	if(pNode && (pNode->m_2.m_deadEndness != 0))
	{
		//Allow driving into oncoming traffic, since dead ends typically only have links pointing out.
		params.m_iDrivingFlags.SetFlag(DF_DriveIntoOncomingTraffic);
	}
	
	//Create the task.
	CTask* pTask = CVehicleIntelligence::CreateAutomobileGotoTask(params, sVehicleMissionParams::DEFAULT_SWITCH_TO_STRAIGHT_LINE_DIST);
	
	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskVehicleApproach::ApproachClosestRoadNode_OnUpdate()
{
	//Update the cruise speed.
	UpdateCruiseSpeed();

	bool bConsiderTargetReached = false;

	//Massive hack to handle the creek situation in GTA V.
	const CEntity* pTarget = m_Params.GetTargetEntity().GetEntity();
	if(pTarget && CTargetInWaterHelper::IsInWater(*pTarget) &&
		CCreekHackHelperGTAV::IsAffected(*GetVehicle()))
	{
		bool bIsMakingDangerousMove = (FindSubTaskOfType(CTaskTypes::TASK_VEHICLE_THREE_POINT_TURN) != NULL);
		if(!bIsMakingDangerousMove)
		{
			Vec3V vTargetPosition;
			if(GetTargetPosition(vTargetPosition))
			{
				Vec3V vVelocity = VECTOR3_TO_VEC3V(GetVehicle()->GetVelocity());
				Vec3V vToTarget = Subtract(vTargetPosition, GetVehicle()->GetTransform().GetPosition());
				ScalarV scDot = Dot(vVelocity, vToTarget);
				if(IsLessThanAll(scDot, ScalarV(V_ZERO)))
				{
					bIsMakingDangerousMove = true;
				}
			}
		}

		if(bIsMakingDangerousMove)
		{
			bConsiderTargetReached = true;
		}
	}
	
	//Check if the sub-task is invalid.
	if(!GetSubTask())
	{
		//Clear the closest road node.
		m_ClosestRoadNode.SetEmpty();

		//Approach the target.
		SetState(State_ApproachTarget);
	}
	//Check if the closest road node is invalid.
	else if(m_ClosestRoadNode.IsEmpty())
	{
		//Approach the target.
		SetState(State_ApproachTarget);
	}
	//Check if the closest road node changed.
	else if(m_uRunningFlags.IsFlagSet(RF_ClosestRoadNodeChanged) && !bConsiderTargetReached)
	{
		//Clear the flag.
		m_uRunningFlags.ClearFlag(RF_ClosestRoadNodeChanged);

		//Restart the state.
		SetFlag(aiTaskFlags::RestartCurrentState);
	}
	//Check if the sub-task has finished.
	else if(GetIsFlagSet(aiTaskFlags::SubTaskFinished) || bConsiderTargetReached)
	{
		//Check if we can terminate.
		if(!m_Params.m_iDrivingFlags.IsFlagSet(DF_DontTerminateTaskWhenAchieved))
		{
			//Finish the task.
			SetState(State_Finish);
		}
		else
		{
			//Clear the closest road node.
			m_ClosestRoadNode.SetEmpty();

			//Approach the target.
			SetState(State_ApproachTarget);
		}
	}
	
	return FSM_Continue;
}

void CTaskVehicleApproach::Wait_OnEnter()
{
	//Create the task.
	CTask* pTask = rage_new CTaskVehicleBrake();

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskVehicleApproach::Wait_OnUpdate()
{
	//Check the if the time in state has exceeded the threshold.
	static dev_float s_fMinTimeInState = 1.0f;
	if(GetTimeInState() > s_fMinTimeInState)
	{
		//Approach the target.
		SetState(State_ApproachTarget);
	}

	return FSM_Continue;
}

bool CTaskVehicleApproach::FindClosestRoadNodeFromDispatch(CNodeAddress& rClosestRoadNode) const
{
	//Ensure the target is valid.
	const CEntity* pEntity = m_Params.GetTargetEntity().GetEntity();
	if(!pEntity)
	{
		return false;
	}
	
	//Ensure the spawn helper is valid.
	const CDispatchSpawnHelper* pSpawnHelper = CDispatchService::GetSpawnHelpers().FindSpawnHelperForEntity(*pEntity);
	if(!pSpawnHelper)
	{
		return false;
	}
	
	//Set the closest road node.
	rClosestRoadNode = pSpawnHelper->GetDispatchNode();
	
	return true;
}

bool CTaskVehicleApproach::FindClosestRoadNodeFromHelper(CNodeAddress& rClosestRoadNode)
{
	//Find the target position.
	Vector3 vTargetPosition;
	FindTargetCoors(GetVehicle(), vTargetPosition);

	//Set the search position.
	m_FindNearestCarNodeHelper.SetSearchPosition(vTargetPosition);

	//Update the helper.
	m_FindNearestCarNodeHelper.Update();

	//Update the closest road node.
	return m_FindNearestCarNodeHelper.GetNearestCarNode(rClosestRoadNode);
}

bool CTaskVehicleApproach::GeneratePositionNearClosestRoadNode(Vec3V_InOut vPosition) const
{
	//Try to generate a random position near the closest road node.
	static int s_iMaxTries = 3;
	for(int i = 0; i < s_iMaxTries; ++i)
	{
		//Generate a random position near the closest road node.
		if(GenerateRandomPositionNearClosestRoadNode(vPosition))
		{
			return true;
		}
	}

	//Ensure the closest road node is valid.
	const CPathNode* pNode = ThePaths.FindNodePointerSafe(m_ClosestRoadNode);
	if(!taskVerifyf(pNode, "The closest road node is invalid."))
	{
		return false;
	}

	//Get the node position.
	pNode->GetCoors(RC_VECTOR3(vPosition));

	return true;
}

bool CTaskVehicleApproach::GenerateRandomPositionNearClosestRoadNode(Vec3V_InOut vPosition) const
{
	//Create the input.
	CFindNearestCarNodeHelper::GenerateRandomPositionNearRoadNodeInput input(m_ClosestRoadNode);
	input.m_fMaxDistance = sm_Tunables.m_MaxDistanceAroundClosestRoadNode;

	//Generate a random position near the road node.
	return CFindNearestCarNodeHelper::GenerateRandomPositionNearRoadNode(input, vPosition);
}

void CTaskVehicleApproach::ProcessClosestRoadNode()
{
	//Clear the flag.
	m_uRunningFlags.ClearFlag(RF_ClosestRoadNodeChanged);

	//Find the closest road node from dispatch.
	CNodeAddress closestRoadNode;
	if(!FindClosestRoadNodeFromDispatch(closestRoadNode))
	{
		//Find the closest road node from the helper.
		if(!FindClosestRoadNodeFromHelper(closestRoadNode))
		{
			return;
		}
	}
	
	//Check if the closest road node is changing.
	if(closestRoadNode != m_ClosestRoadNode)
	{
		//Set the closest road node.
		m_ClosestRoadNode = closestRoadNode;

		//Set the flag.
		m_uRunningFlags.SetFlag(RF_ClosestRoadNodeChanged);
	}
}

void CTaskVehicleApproach::UpdateCruiseSpeed()
{
	//Ensure the sub-task is valid.
	CTask* pSubTask = GetSubTask();
	if(!pSubTask)
	{
		return;
	}

	//Ensure the sub-task is the correct type.
	if(pSubTask->GetTaskType() != CTaskTypes::TASK_VEHICLE_GOTO_AUTOMOBILE_NEW)
	{
		return;
	}

	//Grab the task.
	CTaskVehicleGoToAutomobileNew* pTask = static_cast<CTaskVehicleGoToAutomobileNew *>(pSubTask);
	
	//Update the cruise speed.
	pTask->SetCruiseSpeed(m_Params.m_fCruiseSpeed);
}
