#include "TaskVehicleBlock.h"

// Game headers
#include "Vehicles/Automobile.h"
#include "Vehicles/vehicle.h"
#include "VehicleAi\task\TaskVehicleMissionBase.h"
#include "VehicleAi\task\TaskVehicleGoto.h"
#include "vehicleAi/task/TaskVehicleGoToAutomobile.h"
#include "VehicleAi\task\TaskVehiclePark.h"
#include "VehicleAi\task\TaskVehicleTempAction.h"
#include "VehicleAi\VehicleIntelligence.h"
#include "VehicleAi\VehicleNavigationHelper.h"
#include "peds/ped.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

//=========================================================================
// CTaskVehicleBlock
//=========================================================================

CTaskVehicleBlock::Tunables CTaskVehicleBlock::sm_Tunables;
IMPLEMENT_VEHICLE_AI_TASK_TUNABLES(CTaskVehicleBlock, 0x250e4abd);

CTaskVehicleBlock::CTaskVehicleBlock(const sVehicleMissionParams& rParams, fwFlags8 uConfigFlags)
: CTaskVehicleMissionBase(rParams)
, m_fWidthOfRoad(-1.0f)
, m_uLastTimeProcessedRoad(0)
, m_uConfigFlags(uConfigFlags)
, m_uRunningFlags(0)
{
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_BLOCK);
}

CTaskVehicleBlock::~CTaskVehicleBlock()
{

}

bool CTaskVehicleBlock::IsCruisingInFront() const
{
	//Ensure the current state is cruise in front.
	if(GetState() != State_CruiseInFront)
	{
		return false;
	}

	return true;
}

bool CTaskVehicleBlock::IsGettingInFront() const
{
	//Ensure the current state is get in front.
	if(GetState() != State_GetInFront)
	{
		return false;
	}

	return true;
}

#if !__FINAL
void CTaskVehicleBlock::Debug() const
{
	CTask::Debug();
}

const char* CTaskVehicleBlock::GetStaticStateName(s32 iState)
{
	Assert(iState >= State_Start && iState <= State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_GetInFront",
		"State_CruiseInFront",
		"State_WaitInFront",
		"State_BrakeInFront",
		"State_BackAndForth",
		"State_Seek",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif // !__FINAL

CTask::FSM_Return CTaskVehicleBlock::ProcessPreFSM()
{
	//Ensure the target vehicle is valid.
	if(!GetTargetVehicle())
	{
		return FSM_Quit;
	}

	//Process the situation.
	ProcessSituation();

	//Process the probes.
	ProcessProbes();

	//Process the road.
	ProcessRoad();

	//Process brake in front.
	ProcessBrakeInFront();

	GetVehicle()->m_nVehicleFlags.bDisableAvoidanceProjection = m_uConfigFlags.IsFlagSet(CF_DisableAvoidanceProjection);

	return FSM_Continue;
}

CTask::FSM_Return CTaskVehicleBlock::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_GetInFront)
			FSM_OnEnter
				GetInFront_OnEnter();
			FSM_OnUpdate
				return GetInFront_OnUpdate();
			FSM_OnExit
				GetInFront_OnExit();

		FSM_State(State_CruiseInFront)
			FSM_OnEnter
				CruiseInFront_OnEnter();
			FSM_OnUpdate
				return CruiseInFront_OnUpdate();

		FSM_State(State_WaitInFront)
			FSM_OnEnter
				WaitInFront_OnEnter();
			FSM_OnUpdate
				return WaitInFront_OnUpdate();

		FSM_State(State_BrakeInFront)
			FSM_OnEnter
				BrakeInFront_OnEnter();
			FSM_OnUpdate
				return BrakeInFront_OnUpdate();

		FSM_State(State_BackAndForth)
			FSM_OnEnter
				BackAndForth_OnEnter();
			FSM_OnUpdate
				return BackAndForth_OnUpdate();
			FSM_OnExit
				BackAndForth_OnExit();

		FSM_State(State_Seek)
			FSM_OnEnter
				Seek_OnEnter();
			FSM_OnUpdate
				return Seek_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskVehicleBlock::Start_OnUpdate()
{
	//Get in front of the target.
	SetState(State_GetInFront);

	return FSM_Continue;
}

void CTaskVehicleBlock::GetInFront_OnEnter()
{
	//Grab the vehicle.
	CVehicle* pVehicle = GetVehicle();

	//Set the avoidance cache.
	CVehicleIntelligence::AvoidanceCache& rCache = pVehicle->GetIntelligence()->GetAvoidanceCache();
	rCache.m_pTarget = GetTargetVehicle();
	rCache.m_fDesiredOffset = -sm_Tunables.m_MinDistanceToLookAhead;
	
	//Create the vehicle mission parameters.
	sVehicleMissionParams params = m_Params;
	
	//Use the target position.
	params.m_iDrivingFlags.SetFlag(DF_TargetPositionOverridesEntity);
	
	//Create the task.
	CTask* pTask = CVehicleIntelligence::CreateAutomobileGotoTask(params, sVehicleMissionParams::DEFAULT_SWITCH_TO_STRAIGHT_LINE_DIST);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskVehicleBlock::GetInFront_OnUpdate()
{
	//Update the vehicle mission.
	UpdateVehicleMissionForGetInFront();
	
	//Choose an appropriate state.
	s32 iState = ChooseAppropriateState();
	if(iState != State_Invalid)
	{
		//Move to the state.
		SetState(iState);
	}
	//Check if the task has finished.
	else if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Finish the task.
		SetState(State_Finish);
	}

	return FSM_Continue;
}

void CTaskVehicleBlock::GetInFront_OnExit()
{
	//Grab the vehicle.
	CVehicle* pVehicle = GetVehicle();

	//Clear the avoidance cache.
	pVehicle->GetIntelligence()->ClearAvoidanceCache();
}

void CTaskVehicleBlock::CruiseInFront_OnEnter()
{
	//Create the task.
	CTask* pTask = rage_new CTaskVehicleBlockCruiseInFront(m_Params);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskVehicleBlock::CruiseInFront_OnUpdate()
{
	//Check if cruise in front is no longer appropriate.
	if(IsCruiseInFrontNoLongerAppropriate())
	{
		//Get in front of the target.
		SetState(State_GetInFront);
	}
	//Check if the task has finished.
	else if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Get in front of the target.
		SetState(State_GetInFront);
	}

	return FSM_Continue;
}

void CTaskVehicleBlock::WaitInFront_OnEnter()
{
	//Create the task.
	CTask* pTask = rage_new CTaskVehicleStop(CTaskVehicleStop::SF_DontTerminateWhenStopped | CTaskVehicleStop::SF_UseFullBrake);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskVehicleBlock::WaitInFront_OnUpdate()
{
	//Check if cruise in front is no longer appropriate.
	if(IsCruiseInFrontNoLongerAppropriate())
	{
		//Get in front of the target.
		SetState(State_GetInFront);
	}
	//Check if the task has finished.
	else if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Get in front of the target.
		SetState(State_GetInFront);
	}

	return FSM_Continue;
}

void CTaskVehicleBlock::BrakeInFront_OnEnter()
{
	//Create the task.
	CTask* pTask = rage_new CTaskVehicleBlockBrakeInFront(m_Params);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskVehicleBlock::BrakeInFront_OnUpdate()
{
	//Choose an appropriate state.
	s32 iState = ChooseAppropriateStateForBrakeInFront();
	if(iState != State_Invalid)
	{
		//Move to the state.
		SetState(iState);
	}
	//Check if the task has finished.
	else if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Get in front of the target.
		SetState(State_GetInFront);
	}

	return FSM_Continue;
}

void CTaskVehicleBlock::BackAndForth_OnEnter()
{
	//Create the task.
	CTask* pTask = rage_new CTaskVehicleBlockBackAndForth(m_Params);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskVehicleBlock::BackAndForth_OnUpdate()
{
	//Choose an appropriate state.
	s32 iState = ChooseAppropriateStateForBackAndForth();
	if(iState != State_Invalid)
	{
		//Move to the state.
		SetState(iState);
	}
	//Check if the task has finished.
	else if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Get in front of the target.
		SetState(State_GetInFront);
	}

	return FSM_Continue;
}

void CTaskVehicleBlock::BackAndForth_OnExit()
{

}

void CTaskVehicleBlock::Seek_OnEnter()
{
	//Create the task.
	static dev_float s_fStraightLineDistance = -1.0f;
	CTask* pTask = CVehicleIntelligence::CreateAutomobileGotoTask(m_Params, s_fStraightLineDistance);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskVehicleBlock::Seek_OnUpdate()
{
	//Check if seek is no longer appropriate.
	if(IsSeekNoLongerAppropriate())
	{
		//Get in front of the target.
		SetState(State_GetInFront);
	}
	//Check if the task has finished.
	else if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Get in front of the target.
		SetState(State_GetInFront);
	}

	return FSM_Continue;
}

bool CTaskVehicleBlock::AreWeMovingAwayFromTarget(float fThreshold) const
{
	//Ensure the dot is above the threshold.
	ScalarV scThreshold = ScalarVFromF32(fThreshold);
	if(IsGreaterThanAll(m_Situation.m_scDotMovingAwayFromTarget, scThreshold))
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool CTaskVehicleBlock::AreWeMovingTowardsTarget(float fThreshold) const
{
	//Ensure the dot is above the threshold.
	ScalarV scThreshold = ScalarVFromF32(fThreshold);
	if(IsGreaterThanAll(m_Situation.m_scDotMovingTowardsTarget, scThreshold))
	{
		return true;
	}
	else
	{
		return false;
	}
}

s32 CTaskVehicleBlock::ChooseAppropriateState() const
{
	//Check if we aren't stuck.
	if(!IsStuck())
	{
		//Check if cruise in front is appropriate.
		if(IsCruiseInFrontAppropriate())
		{
			//Check if cruise in front is not disabled.
			if(!m_uConfigFlags.IsFlagSet(CF_DisableCruiseInFront))
			{
				return State_CruiseInFront;
			}
			else
			{
				return State_WaitInFront;
			}
		}
		//Check if brake in front is appropriate.
		else if(IsBrakeInFrontAppropriate())
		{
			return State_BrakeInFront;
		}
		//Check if back and forth is appropriate.
		else if(IsBackAndForthAppropriate())
		{
			return State_BackAndForth;
		}
		//Check if seek is appropriate.
		else if(IsSeekAppropriate())
		{
			return State_Seek;
		}
	}

	return State_Invalid;
}

s32 CTaskVehicleBlock::ChooseAppropriateStateForBackAndForth() const
{
	//Check if we aren't stuck.
	if(!IsStuck())
	{
		//Check if back and forth is no longer appropriate.
		if(IsBackAndForthNoLongerAppropriate())
		{
			return State_GetInFront;
		}
	}

	return State_Invalid;
}

s32 CTaskVehicleBlock::ChooseAppropriateStateForBrakeInFront() const
{
	//Check if we aren't stuck.
	if(!IsStuck())
	{
		//Check if brake in front is no longer appropriate.
		if(IsBrakeInFrontNoLongerAppropriate())
		{
			return State_GetInFront;
		}
	}

	return State_Invalid;
}

const CPed* CTaskVehicleBlock::GetTargetPed() const
{
	return (GetTargetVehicle()->GetDriver());
}

const CVehicle* CTaskVehicleBlock::GetTargetVehicle() const
{
	//Grab the target entity.
	const CEntity* pTargetEntity = m_Params.GetTargetEntity().GetEntity();
	if(!pTargetEntity)
	{
		return NULL;
	}

	//Ensure the entity is a vehicle.
	if(!pTargetEntity->GetIsTypeVehicle())
	{
		return NULL;
	}

	return static_cast<const CVehicle *>(pTargetEntity);
}

bool CTaskVehicleBlock::IsBackAndForthAppropriate() const
{
	//Back and forth is appropriate when all conditions are valid:
	//	1) We are not obstructed by map geometry.
	//	2) The target is moving towards us.
	//	3) The target is moving towards our side.

	//Ensure we are not obstructed by map geometry.
	if(m_uRunningFlags.IsFlagSet(RF_IsObstructedByMapGeometry))
	{
		return false;
	}

	//Ensure we aren't stuck.
	if(IsStuck())
	{
		return false;
	}

	//Ensure the target is moving towards us.
	if(!IsTargetMovingTowardsUs(sm_Tunables.m_MinDotTargetMovingTowardsUsToStartBackAndForth))
	{
		return false;
	}

	//Ensure the target is moving towards our side.
	if(!IsTargetMovingTowardsOurSide(sm_Tunables.m_MinDotTargetMovingTowardsOurSideToStartBackAndForth))
	{
		return false;
	}

	//Ensure the speed/distance is appropriate.
	if(!IsTargetSpeedAndDistanceAppropriateForBackAndForth())
	{
		return false;
	}

	return true;
}

bool CTaskVehicleBlock::IsBackAndForthNoLongerAppropriate() const
{
	//Back and forth is no longer appropriate when one condition is valid:
	//	1) We are obstructed by map geometry.
	//	2) The target is no longer moving towards us.

	//Ensure we are not obstructed by map geometry.
	if(m_uRunningFlags.IsFlagSet(RF_IsObstructedByMapGeometry))
	{
		return true;
	}

	//Ensure we aren't stuck.
	if(IsStuck())
	{
		return true;
	}

	//Ensure the target is moving towards us.
	if(!IsTargetMovingTowardsUs(sm_Tunables.m_MinDotTargetMovingTowardsUsToContinueBackAndForth))
	{
		return true;
	}

	//Ensure the speed/distance is appropriate.
	if(!IsTargetSpeedAndDistanceAppropriateForBackAndForth())
	{
		return true;
	}

	return false;
}

bool CTaskVehicleBlock::IsBrakeInFrontAppropriate() const
{
	//Brake in front is appropriate when all conditions are valid:
	//	1) We are not obstructed by map geometry.
	//	2) The disabled flag is not set.
	//	3) The target is moving towards us.
	//	4) We are moving towards the target.

	//Ensure we are not obstructed by map geometry.
	if(m_uRunningFlags.IsFlagSet(RF_IsObstructedByMapGeometry))
	{
		return false;
	}

	//Ensure the disabled flag is not set.
	if(m_uRunningFlags.IsFlagSet(RF_DisableBlockInFront))
	{
		return false;
	}

	//Ensure we aren't stuck.
	if(IsStuck())
	{
		return false;
	}

	//Ensure the target is moving towards us.
	if(!IsTargetMovingTowardsUs(sm_Tunables.m_MinDotTargetMovingTowardsUsToStartBrakeInFront))
	{
		return false;
	}

	//Ensure we are moving towards the target.
	if(!AreWeMovingTowardsTarget(sm_Tunables.m_MinDotMovingTowardsTargetToStartBrakeInFront))
	{
		return false;
	}

	return true;
}

bool CTaskVehicleBlock::IsBrakeInFrontNoLongerAppropriate() const
{
	//Brake in front is no longer appropriate when one condition is valid:
	//	1) We are obstructed by map geometry.
	//	2) The disabled flag is set.
	//	3) The target is no longer moving towards us.
	//	4) We are no longer moving towards the target.

	//Ensure we are not obstructed by map geometry.
	if(m_uRunningFlags.IsFlagSet(RF_IsObstructedByMapGeometry))
	{
		return true;
	}

	//Ensure the disabled flag is not set.
	if(m_uRunningFlags.IsFlagSet(RF_DisableBlockInFront))
	{
		return true;
	}

	//Ensure we aren't stuck.
	if(IsStuck())
	{
		return true;
	}

	//Ensure the target is moving towards us.
	if(!IsTargetMovingTowardsUs(sm_Tunables.m_MinDotTargetMovingTowardsUsToContinueBrakeInFront))
	{
		return true;
	}

	//Ensure the target is moving towards us.
	if(!AreWeMovingTowardsTarget(sm_Tunables.m_MinDotMovingTowardsTargetToContinueBrakeInFront))
	{
		return true;
	}

	return false;
}

bool CTaskVehicleBlock::IsCruiseInFrontAppropriate() const
{
	//Cruise in front is appropriate when all conditions are valid:
	//	1) We are not obstructed by map geometry.
	//	2) The target is moving towards us.
	//	3) We are moving away the target.

	//Ensure we are not obstructed by map geometry.
	if(m_uRunningFlags.IsFlagSet(RF_IsObstructedByMapGeometry))
	{
		return false;
	}

	//Ensure we aren't stuck.
	if(IsStuck())
	{
		return false;
	}

	//Ensure the target is moving towards us.
	if(!IsTargetMovingTowardsUs(sm_Tunables.m_MinDotTargetMovingTowardsUsToStartCruiseInFront))
	{
		return false;
	}

	//Ensure the target is moving towards us.
	if(!AreWeMovingAwayFromTarget(sm_Tunables.m_MinDotMovingAwayFromTargetToStartCruiseInFront))
	{
		return false;
	}

	return true;
}

bool CTaskVehicleBlock::IsCruiseInFrontNoLongerAppropriate() const
{
	//Cruise in front is no longer appropriate when one condition is valid:
	//	1) We are obstructed by map geometry.
	//	2) The target is no longer moving towards us.

	//Ensure we are not obstructed by map geometry.
	if(m_uRunningFlags.IsFlagSet(RF_IsObstructedByMapGeometry))
	{
		return true;
	}

	//Ensure we aren't stuck.
	if(IsStuck())
	{
		return true;
	}

	//Ensure the target is moving towards us.
	if(!IsTargetMovingTowardsUs(sm_Tunables.m_MinDotTargetMovingTowardsUsToContinueCruiseInFront))
	{
		return true;
	}

	return false;
}

bool CTaskVehicleBlock::IsSeekAppropriate() const
{
	//Ensure we aren't stuck.
	if(IsStuck())
	{
		return false;
	}

	//Check if we are obstructed by map geometry.
	if(m_uRunningFlags.IsFlagSet(RF_IsObstructedByMapGeometry))
	{
		return true;
	}

	//Check if the target ped is inside an enclosed search region.
	const CPed* pTargetPed = GetTargetPed();
	if(pTargetPed && pTargetPed->GetPedResetFlag(CPED_RESET_FLAG_InsideEnclosedSearchRegion))
	{
		return true;
	}

	return false;
}

bool CTaskVehicleBlock::IsSeekNoLongerAppropriate() const
{
	//Ensure we aren't stuck.
	if(IsStuck())
	{
		return false;
	}

	return !IsSeekAppropriate();
}

bool CTaskVehicleBlock::IsStuck() const
{
	return (CVehicleStuckDuringCombatHelper::IsStuck(*GetVehicle()));
}

bool CTaskVehicleBlock::IsTargetInRange(float fMinDistance, float fMaxDistance) const
{
	//Check if the target is in range.
	ScalarV scMinDistanceSq = ScalarVFromF32(square(fMinDistance));
	ScalarV scMaxDistanceSq = ScalarVFromF32(square(fMaxDistance));
	if(IsGreaterThanAll(m_Situation.m_scDistanceSq, scMinDistanceSq) && IsLessThanAll(m_Situation.m_scDistanceSq, scMaxDistanceSq))
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool CTaskVehicleBlock::IsTargetMovingTowardsOurSide(float fThreshold) const
{
	//Check if the left dot exceeds the threshold.
	ScalarV scThreshold = ScalarVFromF32(fThreshold);
	if(IsGreaterThanAll(m_Situation.m_scDotTargetMovingTowardsOurLeftSide, scThreshold))
	{
		return true;
	}

	//Check if the right dot exceeds the threshold.
	if(IsGreaterThanAll(m_Situation.m_scDotTargetMovingTowardsOurRightSide, scThreshold))
	{
		return true;
	}

	return false;
}

bool CTaskVehicleBlock::IsTargetMovingTowardsUs(float fThreshold) const
{
	//Ensure the dot is above the threshold.
	ScalarV scThreshold = ScalarVFromF32(fThreshold);
	if(IsGreaterThanAll(m_Situation.m_scDotTargetMovingTowardsUs, scThreshold))
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool CTaskVehicleBlock::IsTargetSpeedAndDistanceAppropriateForBackAndForth() const
{
	//Calculate the min speed.
	float fDistance = SqrtFastSafe(m_Situation.m_scDistanceSq, ScalarV(V_ZERO)).Getf();
	static dev_float s_fMinDistance = 4.0f;
	static dev_float s_fMaxDistance = 100.0f;
	fDistance = Clamp(fDistance, s_fMinDistance, s_fMaxDistance);
	float fLerp = (fDistance - s_fMinDistance) / (s_fMaxDistance - s_fMinDistance);
	static dev_float s_fMinSpeed = 1.0f;
	static dev_float s_fMaxSpeed = 25.0f;
	float fMinSpeed = Lerp(fLerp, s_fMinSpeed, s_fMaxSpeed);

	//Ensure the speed is valid.
	float fSpeedSq = GetTargetVehicle()->GetVelocity().XYMag2();
	float fMinSpeedSq = square(fMinSpeed);
	if(fSpeedSq < fMinSpeedSq)
	{
		return false;
	}

	return true;
}

void CTaskVehicleBlock::ProcessBrakeInFront()
{
	//Set the flag.
	m_uRunningFlags.ChangeFlag(RF_DisableBlockInFront, ShouldDisableBrakeInFront());
}

void CTaskVehicleBlock::ProcessProbes()
{
	//Check if the probe is not waiting or ready.
	if(!m_ProbeResults.GetResultsWaitingOrReady())
	{
		//Create the probe.
		WorldProbe::CShapeTestProbeDesc probeDesc;
		probeDesc.SetResultsStructure(&m_ProbeResults);
		probeDesc.SetStartAndEnd(VEC3V_TO_VECTOR3(GetVehicle()->GetTransform().GetPosition()), VEC3V_TO_VECTOR3(GetTargetVehicle()->GetTransform().GetPosition()));
		probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
		probeDesc.SetExcludeInstance(GetVehicle()->GetCurrentPhysicsInst());
		//exclude us and target hierarchy (vans can contain GTA_ALL_MAP_TYPES objects)
		const CVehicle* pVehicle = GetTargetVehicle();
		if(pVehicle)
		{
			VehicleNavigationHelper::HelperExcludeCollisionEntities(probeDesc, *pVehicle);
		}
		
		//Start the test.
		WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_ASYNCHRONOUS_TEST);
	}
	else
	{
		//Check if the probe is waiting.
		if(m_ProbeResults.GetWaitingOnResults())
		{
			//Nothing to do, we are waiting on the results.
		}
		else
		{
			//Update the flag.
			m_uRunningFlags.ChangeFlag(RF_IsObstructedByMapGeometry, (m_ProbeResults.GetNumHits() != 0));

			//Reset the results.
			m_ProbeResults.Reset();
		}
	}
}

void CTaskVehicleBlock::ProcessRoad()
{
	//Ensure we have not processed recently.
	static dev_u32 s_uMinTime = 500;
	if(CTimeHelpers::GetTimeSince(m_uLastTimeProcessedRoad) < s_uMinTime)
	{
		return;
	}

	//Set the timer.
	m_uLastTimeProcessedRoad = fwTimer::GetTimeInMilliseconds();

	//Clear the road width.
	m_fWidthOfRoad = -1.0f;

	//Check if we are on the road (we will have a node list in this case).
	const CVehicleNodeList* pNodeList = GetVehicle()->GetIntelligence()->GetNodeList();
	bool bIsOnRoad = (pNodeList != NULL);
	m_uRunningFlags.ChangeFlag(RF_IsOnRoad, bIsOnRoad);
	if(bIsOnRoad)
	{
		//Check if our previous and next node indices are valid.
		s32 iTargetNodeIndex	= pNodeList->GetTargetNodeIndex();
		s32 iPrevNodeIndex		= iTargetNodeIndex - 1;
		if(iPrevNodeIndex >= 0)
		{
			//Check if our previous and next nodes are valid.
			const CPathNode* pPrevNode		= ThePaths.FindNodePointerSafe(pNodeList->GetPathNodeAddr(iPrevNodeIndex));
			const CPathNode* pTargetNode	= ThePaths.FindNodePointerSafe(pNodeList->GetPathNodeAddr(iTargetNodeIndex));
			if(pPrevNode && pTargetNode)
			{
				//Check if the link between is valid.
				const CPathNodeLink* pLink = ThePaths.FindLinkBetween2Nodes(pPrevNode, pTargetNode);
				if(pLink)
				{
					//Find the width of the road.
					float fWidthOfRoadOnLeft	= 0.0f;
					float fWidthOfRoadOnRight	= 0.0f;
					ThePaths.FindRoadBoundaries(*pLink, fWidthOfRoadOnLeft, fWidthOfRoadOnRight);
					m_fWidthOfRoad = (fWidthOfRoadOnLeft + fWidthOfRoadOnRight);
				}
			}
		}
	}

	//Check if the target is on the road.
	const CVehicle* pTargetVehicle = GetTargetVehicle();
	const CVehicleNodeList* pTargetNodeList = pTargetVehicle->GetIntelligence()->GetNodeList();
	const CPed* pTargetDriver = pTargetVehicle->GetDriver();
	const CNodeAddress targetNodeAddress = (pTargetDriver && pTargetDriver->IsPlayer()) ?
		pTargetDriver->GetPlayerInfo()->GetPlayerDataNodeAddress() : CNodeAddress();
	bool bIsTargetOnRoad = ((pTargetNodeList != NULL) || !targetNodeAddress.IsEmpty());
	m_uRunningFlags.ChangeFlag(RF_IsTargetOnRoad, bIsTargetOnRoad);

	//Check if we are on the same road.
	bool bIsOnSameRoad = false;
	if(bIsOnRoad && bIsTargetOnRoad)
	{
		//Check if our target node is valid.
		const CNodeAddress& rNodeAddress = pNodeList->GetPathNodeAddr(pNodeList->GetTargetNodeIndex());
		if(!rNodeAddress.IsEmpty())
		{
			//Check if the target node address is valid.
			const CNodeAddress* pTargetNodeAddress = &targetNodeAddress;
			if(pTargetNodeAddress->IsEmpty())
			{
				taskAssert(pTargetNodeList != NULL);
				pTargetNodeAddress = &pTargetNodeList->GetPathNodeAddr(pTargetNodeList->GetTargetNodeIndex());
			}
			if(!pTargetNodeAddress->IsEmpty())
			{
				//Create the input.
				CPathFind::FindNodeInDirectionInput input(*pTargetNodeAddress, VEC3V_TO_VECTOR3(m_Situation.m_vTargetVehicleDirection), 0.0f);
				input.m_fMaxDistanceTraveled = 200.0f;
				input.m_bFollowRoad = true;
				input.m_bIncludeSwitchedOffNodes = true;
				input.m_bIncludeDeadEndNodes = true;
				input.m_bCanFollowIncomingLinks = true;
				input.m_NodeToFind = rNodeAddress;

				//Check if the target vehicle is heading towards our node.
				CPathFind::FindNodeInDirectionOutput output;
				if(ThePaths.FindNodeInDirection(input, output))
				{
					taskAssert(output.m_Node == rNodeAddress);

					//Note that we are on the same road.
					bIsOnSameRoad = true;
				}
			}
		}
	}
	m_uRunningFlags.ChangeFlag(RF_IsOnSameRoad, bIsOnSameRoad);
}

void CTaskVehicleBlock::ProcessSituation()
{
	//Grab the vehicle.
	const CVehicle* pVehicle = GetVehicle();

	//Grab the target vehicle.
	const CVehicle* pTargetVehicle = GetTargetVehicle();

	//Grab the vehicle position.
	Vec3V vVehiclePosition = pVehicle->GetTransform().GetPosition();

	//Grab the target vehicle position.
	Vec3V vTargetVehiclePosition = pTargetVehicle->GetTransform().GetPosition();

	//Generate a vector from the target vehicle to the vehicle.
	Vec3V vTargetVehicleToVehicle = Subtract(vVehiclePosition, vTargetVehiclePosition);

	//Calculate the distance.
	m_Situation.m_scDistanceSq = MagSquared(vTargetVehicleToVehicle);

	//Grab the target vehicle velocity.
	Vec3V vTargetVehicleVelocity = RCC_VEC3V(pTargetVehicle->GetVelocity());
	
	//Grab the target vehicle forward.
	Vec3V vTargetVehicleForward = pTargetVehicle->GetTransform().GetForward();

	//Calculate the target vehicle direction.
	static dev_float s_fMinTargetVelocityToUseIt = 1.0f;
	Vec3V vMinTargetVelocityToUseItSq = Vec3VFromF32(square(s_fMinTargetVelocityToUseIt));
	Vec3V vTargetVehicleDirection = NormalizeFastSafe(vTargetVehicleVelocity, vTargetVehicleForward, vMinTargetVelocityToUseItSq);

	//Calculate the direction from the target vehicle to the vehicle.
	Vec3V vTargetVehicleToVehicleDirection = NormalizeFastSafe(vTargetVehicleToVehicle, Vec3V(V_ZERO));

	//Calculate the dot product for the target moving towards us.
	m_Situation.m_scDotTargetMovingTowardsUs = Dot(vTargetVehicleDirection, vTargetVehicleToVehicleDirection);

	//Grab the vehicle right vector.
	Vec3V vVehicleRight = pVehicle->GetTransform().GetRight();

	//Calculate the dot product for the target moving towards the left side.
	m_Situation.m_scDotTargetMovingTowardsOurLeftSide = Dot(vTargetVehicleDirection, vVehicleRight);

	//Calculate the dot product for the target moving towards the right side.
	m_Situation.m_scDotTargetMovingTowardsOurRightSide = Negate(m_Situation.m_scDotTargetMovingTowardsOurLeftSide);

	//Grab the vehicle velocity.
	Vec3V vVehicleVelocity = RCC_VEC3V(pVehicle->GetVelocity());
	
	//Grab the vehicle forward.
	Vec3V vVehicleForward = pVehicle->GetTransform().GetForward();

	//Calculate the vehicle direction.
	Vec3V vVehicleDirection = NormalizeFastSafe(vVehicleVelocity, vVehicleForward);

	//Calculate the dot product for moving away from the target.
	m_Situation.m_scDotMovingAwayFromTarget = Dot(vVehicleDirection, vTargetVehicleToVehicleDirection);

	//Calculate the dot product for moving towards the target.
	m_Situation.m_scDotMovingTowardsTarget = Negate(m_Situation.m_scDotMovingAwayFromTarget);

	//Calculate the dot product for the directions.
	m_Situation.m_scDotDirections = Dot(vVehicleDirection, vTargetVehicleDirection);

	//Set the target vehicle direction.
	m_Situation.m_vTargetVehicleDirection = vTargetVehicleDirection;
}

bool CTaskVehicleBlock::ShouldDisableBrakeInFront() const
{
	//Check if both vehicles are on roads, but not the same one.
	if(m_uRunningFlags.IsFlagSet(RF_IsOnRoad) && m_uRunningFlags.IsFlagSet(RF_IsTargetOnRoad) && !m_uRunningFlags.IsFlagSet(RF_IsOnSameRoad))
	{
		return true;
	}

	return false;
}

void CTaskVehicleBlock::UpdateCruiseSpeedForGetInFront(CTaskVehicleMissionBase& rTask)
{
	//The goal here is to cap the cruise speed to the target speed + X m/s, when we are close.
	//This will allow us to pass the target slowly, and not fly right by them.
	
	//Clamp the distance to values we care about.
	float fDistanceSq = m_Situation.m_scDistanceSq.Getf();
	float fMinDistanceSq = square(sm_Tunables.m_DistanceToCapSpeed);
	float fMaxDistanceSq = square(sm_Tunables.m_DistanceToStartCappingSpeed);
	fDistanceSq = Clamp(fDistanceSq, fMinDistanceSq, fMaxDistanceSq);
	
	//Normalize the value.
	float fValue = (fDistanceSq - fMinDistanceSq) / (fMaxDistanceSq - fMinDistanceSq);
	
	//Grab the target vehicle.
	const CVehicle* pTargetVehicle = GetTargetVehicle();
	
	//Grab the target vehicle speed.
	float fTargetVehicleSpeed = pTargetVehicle->GetAiXYSpeed();
	
	//Calculate the cruise speed for the minimum distance.
	float fCruiseSpeedForMinDistance = fTargetVehicleSpeed + sm_Tunables.m_AdditionalSpeedCap;
	
	//Calculate the cruise speed for the maximum distance.
	float fCruiseSpeedForMaxDistance = m_Params.m_fCruiseSpeed;
	
	//Calculate the cruise speed.
	float fCruiseSpeed = Lerp(fValue, fCruiseSpeedForMinDistance, fCruiseSpeedForMaxDistance);
	
	//Set the cruise speed.
	rTask.SetCruiseSpeed(fCruiseSpeed);
}

void CTaskVehicleBlock::UpdateStraightLineForGetInFront(CTaskVehicleMissionBase& rTask)
{
	//Check if we should force straight line mode.
	ScalarV scMaxDistanceFromTargetToForceStraightLineModeSq = ScalarVFromF32(square(sm_Tunables.m_MaxDistanceFromTargetToForceStraightLineMode));
	if(IsLessThanAll(m_Situation.m_scDistanceSq, scMaxDistanceFromTargetToForceStraightLineModeSq))
	{
		//Force straight-line mode.
		rTask.SetDrivingFlag(DF_ForceStraightLine, true);
	}
	else
	{
		//Do not force straight-line mode.
		rTask.SetDrivingFlag(DF_ForceStraightLine, false);
	}
}

void CTaskVehicleBlock::UpdateTargetPositionForGetInFront(CTaskVehicleMissionBase& rTask)
{
	//Grab the target vehicle.
	const CVehicle* pTargetVehicle = GetTargetVehicle();

	//Grab the target vehicle position.
	Vec3V vTargetVehiclePosition = pTargetVehicle->GetTransform().GetPosition();

	//Grab the target vehicle velocity.
	Vec3V vTargetVehicleVelocity = VECTOR3_TO_VEC3V(pTargetVehicle->GetVelocity());

	//Calculate the target vehicle offset.
	Vec3V vTargetVehicleOffset = Scale(vTargetVehicleVelocity, ScalarVFromF32(sm_Tunables.m_TimeToLookAhead));

	//Ensure the distance exceeds the threshold.
	ScalarV scDistSq = MagSquared(vTargetVehicleOffset);
	ScalarV scMinDist = ScalarVFromF32(sm_Tunables.m_MinDistanceToLookAhead);
	ScalarV scMinDistSq = Scale(scMinDist, scMinDist);
	if(IsLessThanAll(scDistSq, scMinDistSq))
	{
		//Grab the target vehicle forward.
		Vec3V vTargetVehicleForward = pTargetVehicle->GetTransform().GetForward();
		
		//Calculate the target vehicle direction.
		Vec3V vTargetVehicleDirection = NormalizeFastSafe(vTargetVehicleVelocity, vTargetVehicleForward);
		
		//Re-calculate the target vehicle offset using the min distance.
		vTargetVehicleOffset = Scale(vTargetVehicleDirection, scMinDist);
	}

	//Calculate the target position.
	Vec3V vTargetPosition = Add(vTargetVehiclePosition, vTargetVehicleOffset);
	
	//Set the target position.
	rTask.SetTargetPosition(&RCC_VECTOR3(vTargetPosition));
}

void CTaskVehicleBlock::UpdateVehicleMissionForGetInFront()
{
	//Grab the sub-task.
	CTask* pSubTask = GetSubTask();
	if(!pSubTask)
	{
		return;
	}

	//Ensure the sub-task is the correct type.
	if(!taskVerifyf(pSubTask->GetTaskType() == CTaskTypes::TASK_VEHICLE_GOTO_AUTOMOBILE_NEW, "Sub-task is the wrong type: %d.", pSubTask->GetTaskType()))
	{
		return;
	}
	
	//Grab the task.
	CTaskVehicleMissionBase* pTask = static_cast<CTaskVehicleMissionBase *>(pSubTask);
	
	//Update the straight-line.
	UpdateStraightLineForGetInFront(*pTask);
	
	//Update the target position.
	UpdateTargetPositionForGetInFront(*pTask);
	
	//Update the cruise speed.
	UpdateCruiseSpeedForGetInFront(*pTask);
}

//=========================================================================
// CTaskVehicleBlockCruiseInFront
//=========================================================================

CTaskVehicleBlockCruiseInFront::Tunables CTaskVehicleBlockCruiseInFront::sm_Tunables;
IMPLEMENT_VEHICLE_AI_TASK_TUNABLES(CTaskVehicleBlockCruiseInFront, 0x80fe69aa);

CTaskVehicleBlockCruiseInFront::CTaskVehicleBlockCruiseInFront(const sVehicleMissionParams& rParams)
: CTaskVehicleMissionBase(rParams)
{
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_BLOCK_CRUISE_IN_FRONT);
}

CTaskVehicleBlockCruiseInFront::~CTaskVehicleBlockCruiseInFront()
{

}

#if !__FINAL
void CTaskVehicleBlockCruiseInFront::Debug() const
{
	CTask::Debug();
}

const char* CTaskVehicleBlockCruiseInFront::GetStaticStateName(s32 iState)
{
	Assert(iState >= State_Start && iState <= State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_Cruise",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif // !__FINAL

void CTaskVehicleBlockCruiseInFront::CleanUp()
{
	//Grab the vehicle.
	CVehicle* pVehicle = GetVehicle();

	//Clear the avoidance cache.
	pVehicle->GetIntelligence()->ClearAvoidanceCache();
}

CTask::FSM_Return CTaskVehicleBlockCruiseInFront::ProcessPreFSM()
{
	//Ensure the target vehicle is valid.
	if(!GetTargetVehicle())
	{
		return FSM_Quit;
	}

	//Process the probes.
	ProcessProbes();

	return FSM_Continue;
}

CTask::FSM_Return CTaskVehicleBlockCruiseInFront::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_Cruise)
			FSM_OnEnter
				Cruise_OnEnter();
			FSM_OnUpdate
				return Cruise_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskVehicleBlockCruiseInFront::Start_OnUpdate()
{
	//Move to the cruise state.
	SetState(State_Cruise);
	
	return FSM_Continue;
}

void CTaskVehicleBlockCruiseInFront::Cruise_OnEnter()
{
	//Grab the vehicle.
	CVehicle* pVehicle = GetVehicle();

	//Set the avoidance cache.
	CVehicleIntelligence::AvoidanceCache& rCache = pVehicle->GetIntelligence()->GetAvoidanceCache();
	rCache.m_pTarget = GetTargetVehicle();
	rCache.m_fDesiredOffset = -sm_Tunables.m_MinDistanceToLookAhead;
	
	//Create the vehicle mission parameters.
	sVehicleMissionParams params = m_Params;
	
	//Do not avoid the target.
	params.m_iDrivingFlags.SetFlag(DF_DontAvoidTarget);
	
	//Use the target position.
	params.m_iDrivingFlags.SetFlag(DF_TargetPositionOverridesEntity);
	
	//Create the task.
	CTask* pTask = CVehicleIntelligence::CreateAutomobileGotoTask(params, sm_Tunables.m_StraightLineDistance);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskVehicleBlockCruiseInFront::Cruise_OnUpdate()
{
	//Update the target position and cruise speed.
	UpdateTargetPositionAndCruiseSpeed();
	
	//Check if the task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Finish the task.
		SetState(State_Finish);
	}
	
	return FSM_Continue;
}

const CVehicle* CTaskVehicleBlockCruiseInFront::GetTargetVehicle() const
{
	//Grab the target entity.
	const CEntity* pTargetEntity = m_Params.GetTargetEntity().GetEntity();
	if(!pTargetEntity)
	{
		return NULL;
	}

	//Ensure the entity is a vehicle.
	if(!pTargetEntity->GetIsTypeVehicle())
	{
		return NULL;
	}

	return static_cast<const CVehicle *>(pTargetEntity);
}

void CTaskVehicleBlockCruiseInFront::ProcessProbes()
{
	//Check if the probe is not waiting or ready.
	if(!m_ProbeResults.GetResultsWaitingOrReady())
	{
		//Calculate the start.
		const float fLocalZ = -GetVehicle()->GetHeightAboveRoad() + sm_Tunables.m_Probes.m_Collision.m_HeightAboveGround;
		Vector3 vStart = GetVehicle()->TransformIntoWorldSpace(Vector3(0.0f, GetVehicle()->GetBoundingBoxMax().y, fLocalZ));

		//Calculate the end.
		Vector3 vForward = VEC3V_TO_VECTOR3(GetVehicle()->GetTransform().GetForward());
		float fSpeed = GetVehicle()->GetAiVelocity().Dot(vForward);
		fSpeed = Clamp(fSpeed, sm_Tunables.m_Probes.m_Collision.m_SpeedForMinLength, sm_Tunables.m_Probes.m_Collision.m_SpeedForMaxLength);
		float fLerp = (fSpeed - sm_Tunables.m_Probes.m_Collision.m_SpeedForMinLength) / (sm_Tunables.m_Probes.m_Collision.m_SpeedForMaxLength - sm_Tunables.m_Probes.m_Collision.m_SpeedForMinLength);
		float fProbeLength = Lerp(fLerp, sm_Tunables.m_Probes.m_Collision.m_MinLength, sm_Tunables.m_Probes.m_Collision.m_MaxLength);
		Vector3 vEnd = vStart + (vForward * fProbeLength);

#if DEBUG_DRAW
		if(sm_Tunables.m_Rendering.m_Enabled && sm_Tunables.m_Rendering.m_Probe)
		{
			grcDebugDraw::Line(vStart, vEnd, Color_yellow, -1);
		}
#endif

		//Create the probe.
		WorldProbe::CShapeTestProbeDesc probeDesc;
		probeDesc.SetResultsStructure(&m_ProbeResults);
		probeDesc.SetStartAndEnd(vStart, vEnd);
		probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);

		//Start the test.
		WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_ASYNCHRONOUS_TEST);
	}
	else
	{
		//Check if the probe is waiting.
		if(m_ProbeResults.GetWaitingOnResults())
		{
			//Nothing to do, we are waiting on the results.
		}
		else
		{
			//Update the flag.
			bool bIsCollisionImminent = (m_ProbeResults.GetNumHits() != 0);
			m_uRunningFlags.ChangeFlag(RF_IsCollisionImminent, bIsCollisionImminent);
			if(bIsCollisionImminent)
			{
				//Set the collision position.
				m_vCollisionPosition = m_ProbeResults[0].GetPosition();

				//Set the collision normal.
				m_vCollisionNormal = m_ProbeResults[0].GetNormal();

#if DEBUG_DRAW
				if(sm_Tunables.m_Rendering.m_Enabled && sm_Tunables.m_Rendering.m_ProbeResults)
				{
					grcDebugDraw::Arrow(m_vCollisionPosition, Add(m_vCollisionPosition, m_vCollisionNormal), 0.25f, Color_red, -1);
				}
#endif
			}

			//Reset the results.
			m_ProbeResults.Reset();
		}
	}
}

void CTaskVehicleBlockCruiseInFront::UpdateTargetPositionAndCruiseSpeed()
{
	//Grab the sub-task.
	CTask* pSubTask = GetSubTask();
	if(!pSubTask)
	{
		return;
	}

	//Ensure the sub-task is the correct type.
	if(!taskVerifyf(pSubTask->GetTaskType() == CTaskTypes::TASK_VEHICLE_GOTO_AUTOMOBILE_NEW, "Sub-task is the wrong type: %d.", pSubTask->GetTaskType()))
	{
		return;
	}

	//Grab the vehicle.
	const CVehicle* pVehicle = GetVehicle();
	
	//Grab the vehicle position.
	Vec3V vVehiclePosition = pVehicle->GetTransform().GetPosition();
	
	//Grab the vehicle velocity.
	Vec3V vVehicleVelocity = VECTOR3_TO_VEC3V(pVehicle->GetVelocity());

	//Grab the vehicle forward.
	Vec3V vVehicleForward = pVehicle->GetTransform().GetForward();

	//Calculate the vehicle direction.
	Vec3V vVehicleDirection = NormalizeFastSafe(vVehicleVelocity, vVehicleForward);
	
	//Grab the target vehicle.
	const CVehicle* pTargetVehicle = GetTargetVehicle();

	//Grab the target vehicle position.
	Vec3V vTargetVehiclePosition = pTargetVehicle->GetTransform().GetPosition();

	//Grab the target vehicle velocity.
	Vec3V vTargetVehicleVelocity = VECTOR3_TO_VEC3V(pTargetVehicle->GetVelocity());

	//Grab the target vehicle forward.
	Vec3V vTargetVehicleForward = pTargetVehicle->GetTransform().GetForward();

	//Calculate the target vehicle direction.
	Vec3V vTargetVehicleDirection = NormalizeFastSafe(vTargetVehicleVelocity, vTargetVehicleForward);
	
	//Calculate the future vehicle offset.
	Vec3V vFutureVehicleOffset = Scale(vVehicleVelocity, ScalarVFromF32(sm_Tunables.m_TimeToLookAhead));
	
	//Ensure the distance exceeds the threshold.
	ScalarV scDistSq = MagSquared(vFutureVehicleOffset);
	ScalarV scMinDist = ScalarVFromF32(sm_Tunables.m_MinDistanceToLookAhead);
	ScalarV scMinDistSq = Scale(scMinDist, scMinDist);
	if(IsLessThanAll(scDistSq, scMinDistSq))
	{
		//Re-calculate the future vehicle offset using the min distance.
		vFutureVehicleOffset = Scale(vVehicleDirection, scMinDist);
	}
	
	//Calculate the future position for the vehicle.
	Vec3V vVehicleFuturePosition = Add(vVehiclePosition, vFutureVehicleOffset);
	
	//Generate a vector from the target vehicle to the future vehicle position.
	Vec3V vTargetToFuture = Subtract(vVehicleFuturePosition, vTargetVehiclePosition);
	
	//Project the vector on to the target vehicle direction.
	Vec3V vTargetToFutureProjection = Scale(vTargetVehicleDirection, Dot(vTargetToFuture, vTargetVehicleDirection));
	
	//Calculate the target position.
	Vec3V vTargetPosition = Add(vTargetVehiclePosition, vTargetToFutureProjection);

	//Check if a collision is imminent.
	if(m_uRunningFlags.IsFlagSet(RF_IsCollisionImminent))
	{
		//Offset the collision position a bit, towards us.
		Vec3V vCollisionToVehicle = Subtract(GetVehicle()->GetTransform().GetPosition(), m_vCollisionPosition);
		Vec3V vCollisionToVehicleDirection = NormalizeFastSafe(vCollisionToVehicle, Vec3V(V_ZERO));
		Vec3V vCollisionPositionWithOffset = AddScaled(m_vCollisionPosition, vCollisionToVehicleDirection, ScalarVFromF32(GetVehicle()->GetBoundingBoxMax().x));

		//Calculate the reflection direction.
		Vec3V vCollisionNormalFlat = m_vCollisionNormal;
		vCollisionNormalFlat.SetZ(ScalarV(V_ZERO));
		Vec3V vCross = Cross(m_vCollisionNormal, Vec3V(V_Z_AXIS_WZERO));
		ScalarV scDot = Dot(vCross, VECTOR3_TO_VEC3V(GetVehicle()->GetAiVelocity()));
		Vec3V vReflectionDirection = (IsGreaterThanAll(scDot, ScalarV(V_ZERO)) != 0) ? vCross : Negate(vCross);

		//Calculate the reflected target position.
		ScalarV scDistToTargetPosition = Dist(GetVehicle()->GetTransform().GetPosition(), vTargetPosition);
		ScalarV scDistToCollision = Dist(GetVehicle()->GetTransform().GetPosition(), vCollisionPositionWithOffset);
		ScalarV scReflectionLength = Subtract(scDistToTargetPosition, scDistToCollision);
		scReflectionLength = Max(ScalarV(V_ZERO), scReflectionLength);
		vTargetPosition = AddScaled(vCollisionPositionWithOffset, vReflectionDirection, scReflectionLength);

#if DEBUG_DRAW
		if(sm_Tunables.m_Rendering.m_Enabled && sm_Tunables.m_Rendering.m_CollisionReflectionDirection)
		{
			grcDebugDraw::Arrow(vCollisionPositionWithOffset, Add(vCollisionPositionWithOffset, vReflectionDirection), 0.25f, Color_green, -1);
		}

		if(sm_Tunables.m_Rendering.m_Enabled && sm_Tunables.m_Rendering.m_CollisionReflectedTargetPosition)
		{
			grcDebugDraw::Sphere(vTargetPosition, 0.5f, Color_green, true, -1);
		}
#endif
	}
	
	//Calculate the distance.
	float fDistSq = DistSquared(vVehiclePosition, vTargetVehiclePosition).Getf();
	
	//Generate the direction from the target vehicle to the vehicle.
	Vec3V vTargetVehicleToVehicle = Subtract(vVehiclePosition, vTargetVehiclePosition);
	Vec3V vTargetVehicleToVehicleDirection = NormalizeFastSafe(vTargetVehicleToVehicle, Vec3V(V_ZERO));
	
	//Keep track of the cruise speed multiplier.
	float fCruiseSpeedMultiplier = 1.0f;
	
	//Check if we should slow down.
	float fDot = Dot(vTargetVehicleToVehicleDirection, vTargetVehicleDirection).Getf();
	if(fDot >= sm_Tunables.m_MinDotForSlowdown)
	{
		//Clamp the distance to values we care about.
		float fMinDistSq = square(sm_Tunables.m_MinDistanceForSlowdown);
		float fMaxDistSq = square(sm_Tunables.m_MaxDistanceForSlowdown);
		fDistSq = Clamp(fDistSq, fMinDistSq, fMaxDistSq);

		//Normalize the value.
		float fValue = fDistSq - fMinDistSq;
		fValue /= (fMaxDistSq - fMinDistSq);

		//Calculate the cruise speed multiplier.
		fCruiseSpeedMultiplier = Lerp(fValue, sm_Tunables.m_CruiseSpeedMultiplierForMinSlowdown, sm_Tunables.m_CruiseSpeedMultiplierForMaxSlowdown);
	}
	else
	{
		//Check if we are inside the ideal distance.
		float fIdealDistSq = square(sm_Tunables.m_IdealDistance);
		if(fDistSq < fIdealDistSq)
		{
			//Clamp the distance to values we care about.
			float fMinDistSq = square(sm_Tunables.m_MinDistanceToAdjustSpeed);
			fDistSq = Clamp(fDistSq, fMinDistSq, fIdealDistSq);
			
			//Normalize the value.
			float fValue = fDistSq - fMinDistSq;
			fValue /= (fIdealDistSq - fMinDistSq);

			//Calculate the cruise speed multiplier.
			fCruiseSpeedMultiplier = Lerp(fValue, sm_Tunables.m_MaxCruiseSpeedMultiplier, 1.0f);
		}
		else
		{
			//Clamp the distance to values we care about.
			float fMaxDistSq = square(sm_Tunables.m_MaxDistanceToAdjustSpeed);
			fDistSq = Clamp(fDistSq, fIdealDistSq, fMaxDistSq);

			//Normalize the value.
			float fValue = fDistSq - fIdealDistSq;
			fValue /= (fMaxDistSq - fIdealDistSq);

			//Calculate the cruise speed multiplier.
			fCruiseSpeedMultiplier = Lerp(fValue, 1.0f, sm_Tunables.m_MinCruiseSpeedMultiplier);
		}
	}
	
	//Calculate the cruise speed.
	float fVehSpeed = pTargetVehicle->GetVelocity().XYMag();
	float fCruiseSpeed = fVehSpeed * fCruiseSpeedMultiplier;

#if __BANK
	if (fCruiseSpeed >= MAX_CRUISE_SPEED)
		AI_LOG_WITH_ARGS("CTaskVehicleBlockCruiseInFront::UpdateTargetPositionAndCruiseSpeed() - fCruiseSpeed (%.2f) >= MAX_CRUISE_SPEED (%.2f). fVehSpeed - %.2f, fCruiseSpeedMultiplier - %.2f\n",fCruiseSpeed, MAX_CRUISE_SPEED, fVehSpeed, fCruiseSpeedMultiplier);
#endif //__BANK

	//Grab the task.
	CTaskVehicleMissionBase* pTask = static_cast<CTaskVehicleMissionBase *>(pSubTask);

	//Set the target position.
	pTask->SetTargetPosition(&RCC_VECTOR3(vTargetPosition));
	
	//Set the cruise speed.
	pTask->SetCruiseSpeed(fCruiseSpeed);
}

//=========================================================================
// CTaskVehicleBlockBrakeInFront
//=========================================================================

CTaskVehicleBlockBrakeInFront::Tunables CTaskVehicleBlockBrakeInFront::sm_Tunables;
IMPLEMENT_VEHICLE_AI_TASK_TUNABLES(CTaskVehicleBlockBrakeInFront, 0x742cf182);

CTaskVehicleBlockBrakeInFront::CTaskVehicleBlockBrakeInFront(const sVehicleMissionParams& rParams)
: CTaskVehicleMissionBase(rParams)
{
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_BLOCK_BRAKE_IN_FRONT);
}

CTaskVehicleBlockBrakeInFront::~CTaskVehicleBlockBrakeInFront()
{

}

#if !__FINAL
void CTaskVehicleBlockBrakeInFront::Debug() const
{
	CTask::Debug();
}

const char* CTaskVehicleBlockBrakeInFront::GetStaticStateName(s32 iState)
{
	Assert(iState >= State_Start && iState <= State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_GetInPosition",
		"State_Brake",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif // !__FINAL

CTask::FSM_Return CTaskVehicleBlockBrakeInFront::ProcessPreFSM()
{
	//Ensure the target vehicle is valid.
	if(!GetTargetVehicle())
	{
		return FSM_Quit;
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskVehicleBlockBrakeInFront::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_GetInPosition)
			FSM_OnEnter
				GetInPosition_OnEnter();
			FSM_OnUpdate
				return GetInPosition_OnUpdate();
		
		FSM_State(State_Brake)
			FSM_OnEnter
				Brake_OnEnter();
			FSM_OnUpdate
				return Brake_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskVehicleBlockBrakeInFront::Start_OnUpdate()
{
	//Get in position.
	SetState(State_GetInPosition);

	return FSM_Continue;
}

void CTaskVehicleBlockBrakeInFront::GetInPosition_OnEnter()
{
	//Create the vehicle mission parameters.
	sVehicleMissionParams params = m_Params;
	
	//Do not avoid the target.
	params.m_iDrivingFlags.SetFlag(DF_DontAvoidTarget);
	
	//Use the target position.
	params.m_iDrivingFlags.SetFlag(DF_TargetPositionOverridesEntity);
	
	//Create the task.
	CTask* pTask = CVehicleIntelligence::CreateAutomobileGotoTask(params, sVehicleMissionParams::DEFAULT_SWITCH_TO_STRAIGHT_LINE_DIST);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskVehicleBlockBrakeInFront::GetInPosition_OnUpdate()
{
	//Update the target position.
	UpdateVehicleMissionForGetInFront();

	//Check if we should brake.
	if(UpdateControlsForBrake())
	{
		//Brake.
		SetState(State_Brake);
	}

	return FSM_Continue;
}

void CTaskVehicleBlockBrakeInFront::Brake_OnEnter()
{
}

CTask::FSM_Return CTaskVehicleBlockBrakeInFront::Brake_OnUpdate()
{
	//Update the controls.
	UpdateControlsForBrake();
	
	//Copy the ideal controls.  They will be changed when they are humanized.
	CVehControls IdealControlsCopy;
	IdealControlsCopy.Copy(m_IdealControls);

	//Smooth the controls.
	CVehicle* pVehicle = GetVehicle();
	HumaniseCarControlInput(pVehicle, &IdealControlsCopy, true, (pVehicle->GetVelocity().Mag2() < square(5.0f)));
	
	//Check if the time has expired.
	if(GetTimeInState() > sm_Tunables.m_MaxTimeForBrake)
	{
		//Finish the task.
		SetState(State_Finish);
	}

	return FSM_Continue;
}

bool CTaskVehicleBlockBrakeInFront::CalculateFutureCollisionDistanceAndDirection(float& fDistance, bool& bIsOnRight) const
{
	Vec3V vPosMe = GetVehicle()->GetTransform().GetPosition();
	Vec3V vPosOther = GetTargetVehicle()->GetTransform().GetPosition();
	Vec3V vVelMe = VECTOR3_TO_VEC3V(GetVehicle()->GetAiVelocity());
	Vec3V vVelOther = VECTOR3_TO_VEC3V(GetTargetVehicle()->GetAiVelocity());

	ScalarV numer = Dot(vPosMe - vPosOther, vVelOther);
	ScalarV denom = Dot(vVelOther - vVelMe, vVelOther);
	if(IsEqualAll(denom, ScalarV(V_ZERO)))
	{
		return false;
	}

	ScalarV t = (numer / denom);

	//Check if we are on a wide road.
	static dev_float s_fMinWidthOfRoad = 20.0f;
	float fWidthOfRoad;
	bool bIsOnWideRoad = (GetWidthOfRoad(fWidthOfRoad) && (fWidthOfRoad >= s_fMinWidthOfRoad));

	//Calculate the time ahead for brake.
	ScalarV fTimeAheadForBrake = !bIsOnWideRoad ? ScalarV(sm_Tunables.m_TimeAheadForBrake) : ScalarV(sm_Tunables.m_TimeAheadForBrakeOnWideRoads);
	
	if (IsGreaterThanAll(t, fTimeAheadForBrake))
	{
		return false;
	}
	
	//Calculate the up vector.
	Vec3V vUp = Vec3V(V_UP_AXIS_WZERO);
	Vec3V vFwd = NormalizeFastSafe(vVelMe, GetVehicle()->GetTransform().GetForward());

	//Calculate the side vector.
	Vec3V vSideRight = Cross(vFwd, vUp);

	Vec3V vFutureMe = AddScaled(vPosMe, vVelMe, t);
	Vec3V vFutureOther = AddScaled(vPosOther, vVelOther, t);
	Vec3V vDiff = vFutureOther - vFutureMe;
	fDistance = Mag(vDiff).Getf();
	if (IsGreaterThanAll(Dot(vSideRight, vDiff), ScalarV(V_ZERO)))
	{
		bIsOnRight = true;
	}
	else
	{
		bIsOnRight = false;
	}

	return true;
}

const CVehicle* CTaskVehicleBlockBrakeInFront::GetTargetVehicle() const
{
	//Grab the target entity.
	const CEntity* pTargetEntity = m_Params.GetTargetEntity().GetEntity();
	if(!pTargetEntity)
	{
		return NULL;
	}

	//Ensure the entity is a vehicle.
	if(!pTargetEntity->GetIsTypeVehicle())
	{
		return NULL;
	}

	return static_cast<const CVehicle *>(pTargetEntity);
}

bool CTaskVehicleBlockBrakeInFront::GetWidthOfRoad(float& fWidth) const
{
	//Ensure the parent is valid.
	const CTask* pParent = GetParent();
	if(!pParent)
	{
		return false;
	}

	//Ensure the parent is the right type.
	if(pParent->GetTaskType() != CTaskTypes::TASK_VEHICLE_BLOCK)
	{
		return false;
	}

	//Get the width of the road.
	const CTaskVehicleBlock* pTask = static_cast<const CTaskVehicleBlock *>(pParent);
	return (pTask->GetWidthOfRoad(fWidth));
}

bool CTaskVehicleBlockBrakeInFront::UpdateControlsForBrake()
{
	//Get the future collision distance and direction.
	float fFutureDistance;
	bool bIsOnRight;
	if(!CalculateFutureCollisionDistanceAndDirection(fFutureDistance, bIsOnRight))
	{
		return false;
	}
	
	//If we are moving too fast, use the brake instead of the hand brake.  This gives more consistent results.
	float fMaxSpeedToUseHandBrake = sm_Tunables.m_MaxSpeedToUseHandBrake;
	if(GetVehicle()->GetAiXYSpeed() < fMaxSpeedToUseHandBrake)
	{
		//Use the hand brake.
		m_IdealControls.SetBrake(0.0f);
		m_IdealControls.SetHandBrake(true);

		//Set the steer angle.
		m_IdealControls.SetSteerAngle( GetVehicle()->GetIntelligence()->FindMaxSteerAngle() * (bIsOnRight ? -1.0f : 1.0f) );
	}
	else
	{
		//Use the brake.
		m_IdealControls.SetBrake(1.0f);
		m_IdealControls.SetHandBrake(false);

		//Clamp the distance to values we care about.
		fFutureDistance = Clamp(fFutureDistance, sm_Tunables.m_FutureDistanceForMinSteerAngle, sm_Tunables.m_FutureDistanceForMaxSteerAngle);

		//Calculate the steer angle.
		float fSteerAngle = (fFutureDistance - sm_Tunables.m_FutureDistanceForMinSteerAngle) / (sm_Tunables.m_FutureDistanceForMaxSteerAngle - sm_Tunables.m_FutureDistanceForMinSteerAngle);
		fSteerAngle *= (bIsOnRight ? -1.0f : 1.0f);

		//Set the steer angle.
		m_IdealControls.SetSteerAngle( fSteerAngle );
	}

	//Set the throttle.
	m_IdealControls.m_throttle = 0.0f;
	
	return true;
}

void CTaskVehicleBlockBrakeInFront::UpdateVehicleMissionForGetInFront()
{
	//Grab the target vehicle.
	const CVehicle* pTargetVehicle = GetTargetVehicle();

	//Grab the sub-task.
	CTask* pSubTask = GetSubTask();
	if(!pSubTask)
	{
		return;
	}

	//Ensure the sub-task is the correct type.
	if(!taskVerifyf(pSubTask->GetTaskType() == CTaskTypes::TASK_VEHICLE_GOTO_AUTOMOBILE_NEW, "Sub-task is the wrong type: %d.", pSubTask->GetTaskType()))
	{
		return;
	}

	//Grab the target vehicle position.
	Vec3V vTargetVehiclePosition = pTargetVehicle->GetTransform().GetPosition();

	//Grab the target vehicle velocity.
	Vec3V vTargetVehicleVelocity = VECTOR3_TO_VEC3V(pTargetVehicle->GetVelocity());

	//Grab the target vehicle forward.
	Vec3V vTargetVehicleForward = pTargetVehicle->GetTransform().GetForward();

	//Calculate the target vehicle direction.
	Vec3V vTargetVehicleDirection = NormalizeFastSafe(vTargetVehicleVelocity, vTargetVehicleForward);

	//Calculate the offset.
	Vec3V vOffset = Scale(vTargetVehicleVelocity, ScalarVFromF32(sm_Tunables.m_TimeAheadForGetInPosition));

	//Check if the offset is large enough.
	ScalarV scOffsetSq = MagSquared(vOffset);
	ScalarV scMinOffset = ScalarVFromF32(sm_Tunables.m_MinOffsetForGetInPosition);
	ScalarV scMinOffsetSq = Scale(scMinOffset, scMinOffset);
	if(IsLessThanAll(scOffsetSq, scMinOffsetSq))
	{
		//Calculate a new offset.
		vOffset = Scale(vTargetVehicleDirection, scMinOffset);
	}

	//Calculate the target position.
	Vec3V vTargetPosition = vTargetVehiclePosition + vOffset;

	//Calculate the cruise speed.
	Vec3V vTargetToUs = Subtract(GetVehicle()->GetTransform().GetPosition(), pTargetVehicle->GetTransform().GetPosition());
	Vec3V vTargetToUsDirection = NormalizeFastSafe(vTargetToUs, Vec3V(V_ZERO));
	ScalarV scDot = Dot(vTargetToUsDirection, vTargetVehicleDirection);
	ScalarV scMinDot = ScalarVFromF32(sm_Tunables.m_MinDotToClampCruiseSpeed);
	float fCruiseSpeed = m_Params.m_fCruiseSpeed;
	if(IsGreaterThanAll(scDot, scMinDot))
	{
		ScalarV scDistSq = DistSquared(GetVehicle()->GetTransform().GetPosition(), vTargetVehiclePosition);
		ScalarV scMaxDistSq = ScalarVFromF32(square(sm_Tunables.m_MaxDistanceToClampCruiseSpeed));
		if(IsLessThanAll(scDistSq, scMaxDistSq))
		{
			fCruiseSpeed = Min(fCruiseSpeed, sm_Tunables.m_MaxCruiseSpeedWhenClamped);
		}
	}

	//Grab the task.
	CTaskVehicleMissionBase* pTask = static_cast<CTaskVehicleMissionBase *>(pSubTask);

	//Set the target position.
	pTask->SetTargetPosition(&RCC_VECTOR3(vTargetPosition));
	pTask->SetCruiseSpeed(fCruiseSpeed);
}

//=========================================================================
// CTaskVehicleBlockBackAndForth
//=========================================================================

CTaskVehicleBlockBackAndForth::Tunables CTaskVehicleBlockBackAndForth::sm_Tunables;

IMPLEMENT_VEHICLE_AI_TASK_TUNABLES(CTaskVehicleBlockBackAndForth, 0xcbde4e51);

CTaskVehicleBlockBackAndForth::CTaskVehicleBlockBackAndForth(const sVehicleMissionParams& rParams)
: CTaskVehicleMissionBase(rParams)
, m_IdealControls()
{
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_BLOCK_BACK_AND_FORTH);
}

CTaskVehicleBlockBackAndForth::~CTaskVehicleBlockBackAndForth()
{

}

#if !__FINAL
void CTaskVehicleBlockBackAndForth::Debug() const
{
	CTask::Debug();
}

const char* CTaskVehicleBlockBackAndForth::GetStaticStateName(s32 iState)
{
	Assert(iState >= State_Start && iState <= State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_Block",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif // !__FINAL

CTask::FSM_Return CTaskVehicleBlockBackAndForth::ProcessPreFSM()
{
	//Ensure the target vehicle is valid.
	if(!GetTargetVehicle())
	{
		return FSM_Quit;
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskVehicleBlockBackAndForth::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_Block)
			FSM_OnEnter
				Block_OnEnter();
			FSM_OnUpdate
				return Block_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskVehicleBlockBackAndForth::Start_OnUpdate()
{
	//Block the target.
	SetState(State_Block);

	return FSM_Continue;
}

void CTaskVehicleBlockBackAndForth::Block_OnEnter()
{
}

CTask::FSM_Return CTaskVehicleBlockBackAndForth::Block_OnUpdate()
{
	//Grab the vehicle.
	CVehicle* pVehicle = GetVehicle();

	//Update the ideal controls.
	UpdateIdealControls();

	//Smooth the controls.
	HumaniseCarControlInput(pVehicle, &m_IdealControls, true, (pVehicle->GetVelocity().Mag2() < square(5.0f)));

	return FSM_Continue;
}

const CVehicle* CTaskVehicleBlockBackAndForth::GetTargetVehicle() const
{
	//Grab the target entity.
	const CEntity* pTargetEntity = m_Params.GetTargetEntity().GetEntity();
	if(!pTargetEntity)
	{
		return NULL;
	}

	//Ensure the entity is a vehicle.
	if(!pTargetEntity->GetIsTypeVehicle())
	{
		return NULL;
	}

	return static_cast<const CVehicle *>(pTargetEntity);
}

void CTaskVehicleBlockBackAndForth::UpdateIdealControls()
{
	//Grab the vehicle.
	const CVehicle* pVehicle = GetVehicle();

	//Grab the target vehicle.
	const CVehicle* pTargetVehicle = GetTargetVehicle();

	//Note: This code was copy/pasted from the old task, it seemed to work decently.

	Vector3	Side, Front;

	m_IdealControls.SetSteerAngle( 0.0f );
	m_IdealControls.m_handBrake = false;

	// Find out how far in front or behind this guy the player would pass.
	// Work out how long it will take until the target hits our 'line'.
	Side = VEC3V_TO_VECTOR3(pTargetVehicle->GetTransform().GetA());
	Side.z = 0.0f;
	Side.Normalize();

	float SideDist = DotProduct(VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition()) -(VEC3V_TO_VECTOR3(pTargetVehicle->GetTransform().GetPosition())+1.0f*pTargetVehicle->GetVelocity()), Side);

	float RequestedForwardSpeed = SideDist;

	// Depending on our orientation we have to move forward or backwards
	Vector3 vecForward(VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetB()));
	if(DotProduct(Side, vecForward) > 0.0f)
	{
		RequestedForwardSpeed = -RequestedForwardSpeed;
	}

	float ForwardSpeed = DotProduct(pVehicle->GetVelocity(), vecForward); //Side);
	float RequestedSpeedDiff = RequestedForwardSpeed - ForwardSpeed;

	if(RequestedSpeedDiff > 0.0f)
	{		// Accelerate
		m_IdealControls.m_throttle = rage::Min(1.0f, RequestedSpeedDiff * sm_Tunables.m_ThrottleMultiplier);
		m_IdealControls.m_brake = 0.0f;
	}
	else
	{		// Slow down and reverse
		if(DotProduct(pVehicle->GetVelocity(), vecForward) < 3.0f)
		{		// reverse
			m_IdealControls.m_throttle = rage::Max(-1.0f, RequestedSpeedDiff * sm_Tunables.m_ThrottleMultiplier);
			m_IdealControls.m_brake = 0.0f;
		}
		else
		{		// brake
			m_IdealControls.m_throttle = 0.0f;
			m_IdealControls.m_brake = rage::Min(1.0f, -RequestedSpeedDiff * sm_Tunables.m_ThrottleMultiplier);
		}
	}

	//Account for things in the way.
	const CCollisionRecord* pCollisionRecord = pVehicle->GetFrameCollisionHistory()->GetMostSignificantCollisionRecord();
	if(pCollisionRecord)
	{
		bool bIgnore = (pCollisionRecord->m_pRegdCollisionEntity && pCollisionRecord->m_pRegdCollisionEntity->GetIsTypePed());
		if(!bIgnore)
		{
			if(((RequestedSpeedDiff > 0.0f) && (pCollisionRecord->m_MyCollisionPosLocal.y > 0.0f)) ||
				((RequestedSpeedDiff < 0.0f) && (pCollisionRecord->m_MyCollisionPosLocal.y < 0.0f)))
			{
				m_IdealControls.m_throttle = 0.0f;
				m_IdealControls.m_brake = 1.0f;
				m_IdealControls.m_handBrake = true;
			}
		}
	}

	Assert(m_IdealControls.m_throttle >= -1.0f && m_IdealControls.m_throttle <= 1.0f);
	Assert(m_IdealControls.m_brake >= 0.0f && m_IdealControls.m_brake <= 1.0f);

	// We also do a little steering to try and get perpendiculal with the target vehicle.
	vecForward = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetB());
	Vector3 vecTargetEntityRight = VEC3V_TO_VECTOR3(GetTargetEntity()->GetTransform().GetA());
	float	dirToTargetOrientation = fwAngle::GetATanOfXY(vecTargetEntityRight.x, vecTargetEntityRight.y);
	float	CurrentOrientation = fwAngle::GetATanOfXY(vecForward.x, vecForward.y);
	float	DesiredChange = dirToTargetOrientation - CurrentOrientation;
	while(DesiredChange < -(HALF_PI)) DesiredChange += PI;
	while(DesiredChange >(HALF_PI)) DesiredChange -= PI;

	m_IdealControls.SetSteerAngle( DesiredChange * 0.5f );
	// If we are reversing at the moment we have to reverse the steering.
	if(DotProduct(pVehicle->GetVelocity(), vecForward) < 0.0f)
	{
		m_IdealControls.SetSteerAngle( -m_IdealControls.m_steerAngle );
	}

	const float maxSteerAngle = pVehicle->GetIntelligence()->FindMaxSteerAngle();
	m_IdealControls.SetSteerAngle( rage::Clamp(m_IdealControls.m_steerAngle, -maxSteerAngle, maxSteerAngle) );
}
