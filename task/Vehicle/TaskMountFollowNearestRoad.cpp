#include "Task/Vehicle/TaskMountFollowNearestRoad.h"

#if ENABLE_HORSE

#include "peds/ped.h"
#include "Task/Movement/TaskNavMesh.h"

AI_OPTIMISATIONS()
// OPTIMISATIONS_OFF()

////////////////////////////////////////////////////////////////////////////////
CTaskMountFollowNearestRoad::Tunables CTaskMountFollowNearestRoad::sm_Tunables;

IMPLEMENT_MOVEMENT_TASK_TUNABLES(CTaskMountFollowNearestRoad, 0x923ca50b);

CTaskMountFollowNearestRoad::CTaskMountFollowNearestRoad()
	: CTaskMove(MOVEBLENDRATIO_STILL)
	, m_FollowNearestPathHelper()
{
	SetInternalTaskType(CTaskTypes::TASK_MOUNT_FOLLOW_NEAREST_ROAD);
}

CTaskMountFollowNearestRoad::~CTaskMountFollowNearestRoad()
{
}

void CTaskMountFollowNearestRoad::CleanUp()
{
}

CTask::FSM_Return CTaskMountFollowNearestRoad::ProcessPreFSM()
{
	const CPed* pPed = GetPed();
	if (!pPed)
	{
		return FSM_Quit;
	}

	m_FollowNearestPathHelper.Process(pPed);

	return FSM_Continue;
}

CTask::FSM_Return CTaskMountFollowNearestRoad::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		FSM_State(State_Find_Route)
			FSM_OnEnter
				FindRoute_OnEnter();
			FSM_OnUpdate
				return FindRoute_OnUpdate();

		FSM_State(State_Follow_Route)
			FSM_OnEnter
				FollowRoute_OnEnter();
			FSM_OnUpdate
				return FollowRoute_OnUpdate();
	FSM_End
}

void CTaskMountFollowNearestRoad::FindRoute_OnEnter()
{
	// Cache the entity
	const CPed* pPed = GetPed();
	Assert(pPed);

	// Request the path
	CFollowNearestPathHelper::HelperSettings settings;
	settings.m_vPositionToTestFrom = pPed->GetTransform().GetPosition();
	settings.m_vHeadingToTestWith = pPed->GetTransform().GetForward();
	settings.m_vVelocity = pPed->GetGroundVelocity();
	settings.m_fCutoffDistForNodeSearch = GetTunables().m_fCutoffDistForNodeSearch;
	settings.m_fMinDistanceBetweenNodes = GetTunables().m_fMinDistanceBetweenNodes;
	settings.m_fForwardProjectionInSeconds = GetTunables().m_fForwardProjectionInSeconds;
	settings.m_fDistSqBeforeMovingToNextNode = GetTunables().m_fDistSqBeforeMovingToNextNode;
	settings.m_fDistBeforeMovingToNextNode = GetTunables().m_fDistBeforeMovingToNextNode;
	settings.m_fMaxHeadingAdjustAllowed = GetTunables().m_fMaxHeadingAdjustDeg * DtoR;
	settings.m_fMaxDistSqFromRoad = GetTunables().m_fMaxDistSqFromRoad;
	settings.m_bIgnoreSwitchedOffNodes = GetTunables().m_bIgnoreSwitchedOffNodes;
	settings.m_bUseWaterNodes = GetTunables().m_bUseWaterNodes;
	settings.m_bUseOnlyHighwayNodes = GetTunables().m_bUseOnlyHighwayNodes;
	settings.m_bSearchUpFromPosition = GetTunables().m_bSearchUpFromPosition;

	m_FollowNearestPathHelper.StartSearch(settings);
}

CTask::FSM_Return CTaskMountFollowNearestRoad::FindRoute_OnUpdate()
{
	// If we don't have a ped
	const CPed* pPed = GetPed();
	if (!pPed)
	{
		return FSM_Quit;
	}

	// If the helper has a valid path
	if (m_FollowNearestPathHelper.HasValidRoute())
	{
		SetState(State_Follow_Route);
	}
	// If the helper couldn't find a path
	else if (m_FollowNearestPathHelper.HasNoRoute())
	{
		// Restart after some time has passed
		if (GetTimeInState() > GetTunables().m_fTimeBeforeRestart)
		{
			SetFlag(aiTaskFlags::RestartCurrentState);
		}
	}
	return FSM_Continue;
}

void CTaskMountFollowNearestRoad::FollowRoute_OnEnter()
{
	// Cache the entity
	const CPed* pPed = GetPed();
	Assert(pPed);

	// Get the MBR
	const float fMoveBlendRatio = pPed->GetMotionData()->GetCurrentMbrY();

	// Create a follow navmesh task
	const float fTargetRadius = GetTunables().m_fDistBeforeMovingToNextNode;
	const float fSlowDownDistance = 0.0f;
	CTaskMoveFollowNavMesh* pNavTask = rage_new CTaskMoveFollowNavMesh(fMoveBlendRatio, GetTarget(), fTargetRadius, fSlowDownDistance);
	pNavTask->SetDontStandStillAtEnd(true);
	pNavTask->SetNeverEnterWater(!pPed->WillPedEnterWater());
	pNavTask->SetKeepMovingWhilstWaitingForPath(true);
	pNavTask->SetAvoidObjects(true);
 	pNavTask->SetAvoidPeds(true);
 	pNavTask->SetScanForObstructions(true);

	// Set our subtask
	SetNewTask(pNavTask);
}

CTask::FSM_Return CTaskMountFollowNearestRoad::FollowRoute_OnUpdate()
{
	// If our follow navmesh task has ended
	if (!GetSubTask() || GetIsSubtaskFinished(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH))
	{
		// If the helper has a valid path
		if (m_FollowNearestPathHelper.HasValidRoute())
		{
			SetFlag(aiTaskFlags::RestartCurrentState);
		}
		else // Look for a new path
		{
			SetState(State_Find_Route);
		}
	}
	// If the navmesh task is running
	else if (GetSubTask()->GetTaskType() == CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH)
	{
		// Update the navmesh task's MBR based on any changes to the ped's MBR
		CPed* pPed = GetPed();
		const float fMoveBlendRatio = pPed->GetMotionData()->GetCurrentMbrY();
		CTaskMoveFollowNavMesh* pNavTask = static_cast<CTaskMoveFollowNavMesh*>(GetSubTask());
		pNavTask->SetMoveBlendRatio(fMoveBlendRatio, true);
	}
	return FSM_Continue;
}

Vector3 CTaskMountFollowNearestRoad::GetTarget() const
{
	// Get the position to move towards
	Vector3 vReturnVal = VEC3V_TO_VECTOR3(m_FollowNearestPathHelper.GetPositionToMoveTowards());

	return vReturnVal;
}

#if !__FINAL
void CTaskMountFollowNearestRoad::Debug() const
{
#if __BANK
	const CPed* pPed = GetPed();

	m_FollowNearestPathHelper.RenderDebug(pPed);

	// Call our base class
	CTask::Debug();
#endif // __BANK
}
#endif // __FINAL

#endif // ENABLE_HORSE
