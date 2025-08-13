// FILE :    TaskVehicleChase.h
// PURPOSE : Subtask of combat used to chase a vehicle

// File header
#include "Task/Combat/Subtasks/TaskVehicleChase.h"

// Game headers
#include "control/Replay/ReplayBufferMarker.h"
#include "event/EventShocking.h"
#include "event/ShockingEvents.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "task/Combat/Subtasks/TaskVehicleCombat.h"
#include "task/Combat/Subtasks/VehicleChaseDirector.h"
#include "task/Vehicle/TaskCar.h"
#include "vehicleAi/task/TaskVehicleBlock.h"
#include "vehicleAi/task/TaskVehicleCruise.h"
#include "vehicleAi/task/TaskVehicleGoToAutomobile.h"
#include "vehicleAi/task/TaskVehiclePullAlongside.h"
#include "vehicleAi/task/TaskVehiclePursue.h"
#include "vehicleAi/task/TaskVehicleRam.h"
#include "vehicleAi/task/TaskVehicleSpinOut.h"
#include "vehicleAi/task/TaskVehicleTempAction.h"
#include "vehicleAi/VehicleIntelligence.h"
#include "Vehicles/vehicle.h"
#include "Vehicles/Trailer.h"
#include "network/NetworkInterface.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

//=========================================================================
// CTaskVehicleChase
//=========================================================================

CTaskVehicleChase::Tunables CTaskVehicleChase::sm_Tunables;
IMPLEMENT_COMBAT_TASK_TUNABLES(CTaskVehicleChase, 0x2b1f8cb6);

CTaskVehicleChase::CTaskVehicleChase(const CAITarget& rTarget)
: m_Overrides()
, m_Situation()
, m_Target(rTarget)
, m_fAggressiveMoveDelay(0.0f)
, m_fAggressiveMoveDelayMin(sm_Tunables.m_AggressiveMove.m_MinDelay)
, m_fAggressiveMoveDelayMax(sm_Tunables.m_AggressiveMove.m_MaxDelay)
, m_fCarChaseShockingEventCounter(0.0f)
, m_fFrustrationTimer(0.0f)
, m_uLastTimeTargetPositionWasOffRoad(0)
, m_uTimeStateBecameInvalid(0)
, m_uRunningFlags(0)
{
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_CHASE);
}

CTaskVehicleChase::~CTaskVehicleChase()
{

}

void CTaskVehicleChase::ChangeBehaviorFlag(Overrides::Source nSource, BehaviorFlags nBehaviorFlag, bool bValue)
{
	//Change the behavior flag.
	m_Overrides[nSource].m_uBehaviorFlags.ChangeFlag((u16)nBehaviorFlag, bValue);
}

Vec3V_Out CTaskVehicleChase::GetDrift() const
{
	//Check if the pursue task is valid.
	const CTaskVehiclePursue* pTask = GetVehicleTaskForPursue();
	if(pTask)
	{
		return pTask->GetDrift();
	}
	
	return Vec3V(V_ZERO);
}

bool CTaskVehicleChase::IsBlocking() const
{
	//Ensure the current state is block.
	if(GetState() != State_Block)
	{
		return false;
	}
	
	return true;
}

bool CTaskVehicleChase::IsCruisingInFrontToBlock() const
{
	//Ensure the task is valid.
	const CTaskVehicleBlock* pTask = GetVehicleTaskForBlock();
	if(!pTask)
	{
		return false;
	}
	
	return pTask->IsCruisingInFront();
}

bool CTaskVehicleChase::IsGettingInFrontToBlock() const
{
	//Ensure the task is valid.
	const CTaskVehicleBlock* pTask = GetVehicleTaskForBlock();
	if(!pTask)
	{
		return false;
	}
	
	return pTask->IsGettingInFront();
}

bool CTaskVehicleChase::IsMakingAggressiveMove() const
{
	//Check the previous state.
	switch(GetPreviousState())
	{
		case State_Analyze:
		{
			return IsMakingAggressiveMoveFromAnalyze();
		}
		case State_Pursue:
		{
			return IsMakingAggressiveMoveFromPursue();
		}
		default:
		{
			return false;
		}
	}
}

bool CTaskVehicleChase::IsPullingAlongside() const
{
	//Ensure the current state is pull alongside.
	if(GetState() != State_PullAlongside &&
	   GetState() != State_PullAlongsideInfront)
	{
		return false;
	}

	return true;
}

bool CTaskVehicleChase::IsPursuing() const
{
	//Ensure the current state is pursue.
	if(GetState() != State_Pursue)
	{
		return false;
	}

	return true;
}

bool CTaskVehicleChase::IsRamming() const
{
	//Ensure the current state is ram.
	if(GetState() != State_Ram)
	{
		return false;
	}

	return true;
}

void CTaskVehicleChase::SetDesiredOffsetForPursue(Overrides::Source nSource, float fDesiredOffset)
{
	//Grab the desired offset.
	float& rDesiredOffset = m_Overrides[nSource].m_Pursue.m_fDesiredOffset;
	if(rDesiredOffset == fDesiredOffset)
	{
		return;
	}

	//Set the desired offset.
	rDesiredOffset = fDesiredOffset;

	//Note that the desired offset changed.
	OnDesiredOffsetForPursueChanged();
}

void CTaskVehicleChase::SetIdealDistanceForPursue(Overrides::Source nSource, float fIdealDistance)
{
	//Grab the ideal distance.
	float& rIdealDistance = m_Overrides[nSource].m_Pursue.m_fIdealDistance;

	//Ensure the ideal distance is changing.
	if(rIdealDistance == fIdealDistance)
	{
		return;
	}

	//Set the ideal distance.
	rIdealDistance = fIdealDistance;

	//Note that the ideal distance changed.
	OnIdealDistanceForPursueChanged();
}

void CTaskVehicleChase::TargetPositionIsOffRoad()
{
	//Set the time.
	m_uLastTimeTargetPositionWasOffRoad = fwTimer::GetTimeInMilliseconds();
}

#if !__FINAL
void CTaskVehicleChase::Debug() const
{
	CTask::Debug();
}

const char* CTaskVehicleChase::GetStaticStateName(s32 iState)
{
	Assert(iState >= State_Start && iState <= State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_Analyze",
		"State_CloseDistance",
		"State_Block",
		"State_Pursue",
		"State_Ram",
		"State_SpinOut",
		"State_PullAlongside",
		"State_PullAlongsideInfront",
		"State_Fallback",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif // !__FINAL

void CTaskVehicleChase::CleanUp()
{
	//Remove from the vehicle chase director.
	RemoveFromVehicleChaseDirector();

	//Reset the racing modifier.
	GetPed()->GetPedIntelligence()->SetDriverRacingModifier(0.0f);

	//Clear the flags.
	CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
	if(pVehicle)
	{
		pVehicle->m_nVehicleFlags.bIsRacing = false;
		pVehicle->m_nVehicleFlags.bAllowBoundsToBeRaised = false;
		pVehicle->m_nVehicleFlags.bForceEntityScansAtHighSpeeds = false;
		pVehicle->m_nVehicleFlags.bIsRacingConservatively = false;
	}
}

CTask::FSM_Return CTaskVehicleChase::ProcessPreFSM()
{
	//Ensure we are driving a vehicle.
	if(!GetPed()->GetIsDrivingVehicle())
	{
		return FSM_Quit;
	}
	
	//Ensure the target is valid.
	if(!GetDominantTarget())
	{
		return FSM_Quit;
	}
	
	//Process the aggressive move.
	ProcessAggressiveMove();
	
	//Process the situation.
	ProcessSituation();
	
	//Process the hand brake.
	ProcessHandBrake();

	//Process shocking events thrown by this task.
	ProcessShockingEvents();

	//Process stuck.
	ProcessStuck();
	
	//Check for low speed
	ProcessLowSpeedChase();

	//Set the flag.
	GetPed()->SetPedResetFlag(CPED_RESET_FLAG_IsInVehicleChase, true);
	
	return FSM_Continue;
}

CTask::FSM_Return CTaskVehicleChase::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();
				
		FSM_State(State_Analyze)
			FSM_OnUpdate
				return Analyze_OnUpdate();

		FSM_State(State_CloseDistance)
			FSM_OnEnter
				CloseDistance_OnEnter();
			FSM_OnUpdate
				return CloseDistance_OnUpdate();

		FSM_State(State_Block)
			FSM_OnEnter
				Block_OnEnter();
			FSM_OnUpdate
				return Block_OnUpdate();
			FSM_OnExit
				Block_OnExit();
				
		FSM_State(State_Pursue)
			FSM_OnEnter
				Pursue_OnEnter();
			FSM_OnUpdate
				return Pursue_OnUpdate();
			FSM_OnExit
				Pursue_OnExit();
				
		FSM_State(State_Ram)
			FSM_OnEnter
				Ram_OnEnter();
			FSM_OnUpdate
				return Ram_OnUpdate();
				
		FSM_State(State_SpinOut)
			FSM_OnEnter
				SpinOut_OnEnter();
			FSM_OnUpdate
				return SpinOut_OnUpdate();

		FSM_State(State_PullAlongside)
			FSM_OnEnter
				PullAlongside_OnEnter();
			FSM_OnUpdate
				return PullAlongside_OnUpdate();

		FSM_State(State_PullAlongsideInfront)
			FSM_OnEnter
				PullAlongsideInfront_OnEnter();
			FSM_OnUpdate
				return PullAlongsideInfront_OnUpdate();

		FSM_State(State_Fallback)
			FSM_OnEnter
				FallBack_OnEnter();
			FSM_OnUpdate
				return FallBack_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

bool CTaskVehicleChase::ProcessMoveSignals()
{
	//Process the cheat power increase.
	ProcessCheatPowerIncrease();

	//Process the cheat flags.
	ProcessCheatFlags();

	//Check if the vehicle is valid.
	CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
	if(pVehicle)
	{
		pVehicle->m_nVehicleFlags.bAvoidanceDirtyFlag = true;
	}

	return true;
}

CTask::FSM_Return CTaskVehicleChase::Start_OnUpdate()
{
	//Add to the vehicle chase director.
	AddToVehicleChaseDirector();

	//Set the racing modifier.
	static dev_float s_fRacingModifier = 0.5f;
	GetPed()->GetPedIntelligence()->SetDriverRacingModifier(s_fRacingModifier);

	//Set the flags.
	CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
	if(pVehicle)
	{
		pVehicle->m_nVehicleFlags.bIsRacing = true;
		pVehicle->m_nVehicleFlags.bAllowBoundsToBeRaised = true;
		pVehicle->m_nVehicleFlags.bForceEntityScansAtHighSpeeds = true;
		pVehicle->m_nVehicleFlags.bIsRacingConservatively = true;
	}
	
	//Analyze the situation.
	SetState(State_Analyze);

	//Request process move signal calls.
	RequestProcessMoveSignalCalls();

	return FSM_Continue;
}

CTask::FSM_Return CTaskVehicleChase::Analyze_OnUpdate()
{
	//Choose an appropriate state.
	s32 iState = ChooseAppropriateStateForAnalyze();
	if(taskVerifyf(iState != State_Invalid, "Analyze did not return a valid state."))
	{
		//Move to the state.
		SetState(iState);
	}
	
	return FSM_Continue;
}

void CTaskVehicleChase::CloseDistance_OnEnter()
{
	//Load the vehicle mission parameters.
	sVehicleMissionParams params;
	LoadVehicleMissionParameters(params);

	//Create the vehicle task.
	CTask* pVehicleTask = CVehicleIntelligence::CreateAutomobileGotoTask(params, sVehicleMissionParams::DEFAULT_SWITCH_TO_STRAIGHT_LINE_DIST);

#if __ASSERT
	taskAssert(pVehicleTask->GetTaskType() == CTaskTypes::TASK_VEHICLE_GOTO_AUTOMOBILE_NEW);
	CTaskVehicleGoToAutomobileNew* pVehicleTaskGoTo = static_cast<CTaskVehicleGoToAutomobileNew *>(pVehicleTask);
	pVehicleTaskGoTo->SetDontAssertIfTargetInvalid(true);
#endif

	//Create the task.
	CTask* pTask = rage_new CTaskControlVehicle(GetPed()->GetVehiclePedInside(), pVehicleTask);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskVehicleChase::CloseDistance_OnUpdate()
{
	//Choose an appropriate state.
	s32 iState = ChooseAppropriateStateForCloseDistance();
	if(iState != State_Invalid)
	{
		//Move to the state.
		SetState(iState);
	}
	//Check if the task has finished.
	else if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Analyze the state.
		SetState(State_Analyze);
	}

	return FSM_Continue;
}

void CTaskVehicleChase::Block_OnEnter()
{
	//Load the vehicle mission parameters.
	sVehicleMissionParams params;
	LoadVehicleMissionParameters(params);

	//Create the vehicle task.
	CTaskVehicleBlock* pVehicleTask = rage_new CTaskVehicleBlock(params);

	//Check if we can't cruise in front.
	if(IsBehaviorFlagSet(BF_CantCruiseInFrontDuringBlock) || GetPed()->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_DisableCruiseInFrontDuringBlockDuringVehicleChase))
	{
		pVehicleTask->GetConfigFlags().SetFlag(CTaskVehicleBlock::CF_DisableCruiseInFront);
	}

	if(IsMakingAggressiveMoveFromPursue())
	{
		pVehicleTask->GetConfigFlags().SetFlag(CTaskVehicleBlock::CF_DisableAvoidanceProjection);
	}

	//Create the sub-task.
	CAITarget target(m_Target);
	CTask* pSubTask = rage_new CTaskVehicleCombat(&target);

	//Create the task.
	CTask* pTask = rage_new CTaskControlVehicle(GetPed()->GetVehiclePedInside(), pVehicleTask, pSubTask, CTaskControlVehicle::CF_IgnoreNewCommand);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskVehicleChase::Block_OnUpdate()
{
	//Check for aggressive move success.
	CheckForAggressiveMoveSuccess();

	//Choose an appropriate state.
	s32 iState = ChooseAppropriateStateForBlock();
	if(iState != State_Invalid)
	{
		//Move to the state.
		SetState(iState);
	}
	//Check if the sub-task has finished.
	else if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Analyze the state.
		SetState(State_Analyze);
	}

	return FSM_Continue;
}

void CTaskVehicleChase::Block_OnExit()
{
	//Clear the time.
	m_uTimeStateBecameInvalid = 0;
}

void CTaskVehicleChase::Pursue_OnEnter()
{
	//Load the vehicle mission parameters.
	sVehicleMissionParams params;
	LoadVehicleMissionParameters(params);

	//Extend the route with wander results.
	params.m_iDrivingFlags.SetFlag(DF_ExtendRouteWithWanderResults);
	
	//Calculate the ideal distance.
	float fIdealDistance = CalculateIdealDistanceForPursue();
	
	//Calculate the desired offset.
	float fDesiredOffset = CalculateDesiredOffsetForPursue();

	//Create the vehicle task.
	CTask* pVehicleTask = rage_new CTaskVehiclePursue(params, fIdealDistance, fDesiredOffset);

	//Create the sub-task.
	CAITarget target(m_Target);
	CTask* pSubTask = rage_new CTaskVehicleCombat(&target);

	//Create the task.
	CTask* pTask = rage_new CTaskControlVehicle(GetPed()->GetVehiclePedInside(), pVehicleTask, pSubTask, CTaskControlVehicle::CF_IgnoreNewCommand);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskVehicleChase::Pursue_OnUpdate()
{
	//Update the pursue.
	UpdatePursue();

	//Choose an appropriate state.
	s32 iState = ChooseAppropriateStateForPursue();
	if(iState != State_Invalid)
	{
		//Move to the state.
		SetState(iState);
	}
	//Check if the sub-task has finished.
	else if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Analyze the state.
		SetState(State_Analyze);
	}

	return FSM_Continue;
}

void CTaskVehicleChase::Pursue_OnExit()
{
	//Clear the time.
	m_uTimeStateBecameInvalid = 0;
}

void CTaskVehicleChase::Ram_OnEnter()
{
	//Load the vehicle mission parameters.
	sVehicleMissionParams params;
	LoadVehicleMissionParameters(params);

	//Create the vehicle task.
	CTaskVehicleRam* pVehicleTask = rage_new CTaskVehicleRam(params, sm_Tunables.m_Ram.m_StraightLineDistance);

	//Check if we should use a continuous ram.
	if(ShouldUseContinuousRam())
	{
		pVehicleTask->GetConfigFlags().SetFlag(CTaskVehicleRam::CF_Continuous);
	}

	//Create the sub-task.
	CAITarget target(m_Target);
	CTask* pSubTask = rage_new CTaskVehicleCombat(&target);

	//Create the task.
	CTask* pTask = rage_new CTaskControlVehicle(GetPed()->GetVehiclePedInside(), pVehicleTask, pSubTask);

	//Start the task.
	//@@: location CTASKVEHICLECHASE_RAMONENTER
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskVehicleChase::Ram_OnUpdate()
{
	//Check for aggressive move success.
	CheckForAggressiveMoveSuccess();

	//Choose an appropriate state.
	s32 iState = ChooseAppropriateStateForRam();
	if(iState != State_Invalid)
	{
		//Move to the state.
		SetState(iState);
	}
	//Check if the sub-task has finished.
	else if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Analyze the state.
		SetState(State_Analyze);
	}

	return FSM_Continue;
}

void CTaskVehicleChase::SpinOut_OnEnter()
{
	//Load the vehicle mission parameters.
	sVehicleMissionParams params;
	LoadVehicleMissionParameters(params);

	//Create the vehicle task.
	CTask* pVehicleTask = rage_new CTaskVehicleSpinOut(params, sm_Tunables.m_SpinOut.m_StraightLineDistance);

	//Create the sub-task.
	CAITarget target(m_Target);
	CTask* pSubTask = rage_new CTaskVehicleCombat(&target);

	//Create the task.
	CTask* pTask = rage_new CTaskControlVehicle(GetPed()->GetVehiclePedInside(), pVehicleTask, pSubTask);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskVehicleChase::SpinOut_OnUpdate()
{
	//Check for aggressive move success.
	CheckForAggressiveMoveSuccess();

	//Choose an appropriate state.
	s32 iState = ChooseAppropriateStateForSpinOut();
	if(iState != State_Invalid)
	{
		//Move to the state.
		SetState(iState);
	}
	//Check if the sub-task has finished.
	else if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Analyze the state.
		SetState(State_Analyze);
	}

	return FSM_Continue;
}

void CTaskVehicleChase::PullAlongside_OnEnter()
{
	//Load the vehicle mission parameters.
	sVehicleMissionParams params;
	LoadVehicleMissionParameters(params);

	//Create the vehicle task.
	CTask* pVehicleTask = rage_new CTaskVehiclePullAlongside(params, sm_Tunables.m_PullAlongside.m_StraightLineDistance, false);

	//Create the sub-task.
	CAITarget target(m_Target);
	CTask* pSubTask = rage_new CTaskVehicleCombat(&target);

	//Create the task.
	CTask* pTask = rage_new CTaskControlVehicle(GetPed()->GetVehiclePedInside(), pVehicleTask, pSubTask);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskVehicleChase::PullAlongside_OnUpdate()
{
	//Check for aggressive move success.
	CheckForAggressiveMoveSuccess();

	//Choose an appropriate state.
	s32 iState = ChooseAppropriateStateForPullAlongside();
	if(iState != State_Invalid)
	{
		//Move to the state.
		SetState(iState);
	}
	//Check if the sub-task has finished.
	else if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Analyze the state.
		SetState(State_Analyze);
	}

	return FSM_Continue;
}

void CTaskVehicleChase::AddToVehicleChaseDirector()
{
	//Remove from the vehicle chase director.
	RemoveFromVehicleChaseDirector();

	//Find the vehicle chase director for the target vehicle.
	CVehicleChaseDirector* pVehicleChaseDirector = CVehicleChaseDirector::FindVehicleChaseDirector(*GetDominantTarget());
	if(!pVehicleChaseDirector)
	{
		return;
	}

	//Add to the vehicle chase director.
	pVehicleChaseDirector->Add(this);
}

bool CTaskVehicleChase::AreWeForcedIntoBlock() const
{
	//Ensure we can pursue.
	//If we can't, we are being forced to block.
	if(!CanPursue())
	{
		return true;
	}
	
	return false;
}

bool CTaskVehicleChase::AreWeForcedIntoPursue() const
{
	//Ensure we can block.  If we can't, we are being forced to pursue.
	if(!CanBlock())
	{
		return true;
	}

	return false;
}

float CTaskVehicleChase::CalculateAggressiveMoveDelay() const
{
	//Check the time between aggressive moves.
	float fTimeBetweenAggressiveMoves = GetPed()->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatTimeBetweenAggressiveMovesDuringVehicleChase);
	if(fTimeBetweenAggressiveMoves < 0.0f)
	{
		//No time between aggressive moves has been specified.  This means we should choose a random delay.

		//Choose a random delay.
		float fAggressiveMoveDelay = fwRandom::GetRandomNumberInRange(m_fAggressiveMoveDelayMin, m_fAggressiveMoveDelayMax);

		//Modify the delay for low speed chases.
		if(m_uRunningFlags.IsFlagSet(RF_IsLowSpeedChase))
		{	
			static dev_float sfMultiplierForLowSpeedChases = 0.75f;
			fAggressiveMoveDelay *= sfMultiplierForLowSpeedChases;
		}

		return fAggressiveMoveDelay;
	}
	else if(fTimeBetweenAggressiveMoves == 0.0f)
	{
		//Time between aggressive moves has been specified, and it's zero.  This means we should disable aggressive moves.

		return 0.0f;
	}
	else
	{
		//Time between aggressive moves has been specified, and it's positive.  This means we should use the time.

		return fTimeBetweenAggressiveMoves;
	}
}

float CTaskVehicleChase::CalculateCheatPowerIncrease() const
{
	//Ensure the vehicle is valid.
	const CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
	if(!pVehicle)
	{
		return 1.0f;
	}

	//Grab the vehicle speed.
	float fSpeed = pVehicle->GetVelocity().XYMag();

	//Grab the desired speed.
	float fDesiredSpeed = GetDesiredSpeed();

	//Calculate the speed difference.
	float fSpeedDifference = fDesiredSpeed - fSpeed;

	//Ensure the difference is positive.
	if(fSpeedDifference <= 0.0f)
	{
		return 1.0f;
	}

	//Calculate the cheat power adjustment range.
	float fCheatPowerAdjustmentRange = sm_Tunables.m_Cheat.m_MaxSpeedDifferenceForPowerAdjustment - sm_Tunables.m_Cheat.m_MinSpeedDifferenceForPowerAdjustment;
	taskAssertf(fCheatPowerAdjustmentRange != 0.0f, "Range is zero.");

	//Normalize the cheat power adjustment.
	float fCheatPowerAdjustment = (fSpeedDifference - sm_Tunables.m_Cheat.m_MinSpeedDifferenceForPowerAdjustment) / fCheatPowerAdjustmentRange;

	//Lerp the cheat power increase.
	float fMaxCheatPowerIncrease = (NetworkInterface::IsGameInProgress() && !pVehicle->PopTypeIsMission()) ? 
								sm_Tunables.m_Cheat.m_PowerForMaxAdjustmentNetwork : sm_Tunables.m_Cheat.m_PowerForMaxAdjustment;
	float fCheatPowerIncrease = Lerp(fCheatPowerAdjustment, sm_Tunables.m_Cheat.m_PowerForMinAdjustment, fMaxCheatPowerIncrease);

	return fCheatPowerIncrease;
}

float CTaskVehicleChase::CalculateCruiseSpeed() const
{
	return (CalculateCruiseSpeed(*GetPed(), *GetPed()->GetVehiclePedInside()));
}

float CTaskVehicleChase::CalculateDesiredOffsetForPursue() const
{
	//Iterate over the overrides.
	for(int i = Overrides::FirstSource; i < Overrides::MaxSources; ++i)
	{
		//Check if the desired offset has been overridden.
		float fDesiredOffset = m_Overrides[i].m_Pursue.m_fDesiredOffset;
		if(Abs(fDesiredOffset) > SMALL_FLOAT)
		{
			return fDesiredOffset;
		}
	}

	return 0.0f;
}

Vec3V_Out CTaskVehicleChase::CalculateDirectionForVehicle(const CVehicle& rVehicle) const
{
	//Grab the velocity.
	Vec3V vVelocity = VECTOR3_TO_VEC3V(rVehicle.GetVelocity());
	
	//Grab the forward.
	Vec3V vForward = rVehicle.GetVehicleForwardDirection();
	
	//Normalize the velocity.
	Vec3V vDirection = NormalizeFastSafe(vVelocity, vForward);
	
	return vDirection;
}

float CTaskVehicleChase::CalculateIdealDistanceForPursue() const
{
	//Iterate over the overrides.
	for(int i = Overrides::FirstSource; i < Overrides::MaxSources; ++i)
	{
		//Check if the ideal distance has been overridden.
		float fIdealDistance = m_Overrides[i].m_Pursue.m_fIdealDistance;
		if(fIdealDistance >= 0.0f)
		{
			return fIdealDistance;
		}
	}
	
	return sm_Tunables.m_Pursue.m_IdealDistance;
}

bool CTaskVehicleChase::CanAnyoneInVehicleDoDrivebys(const CVehicle& rVehicle) const
{
	//Iterate over the seats.
	for(int i = 0; i < rVehicle.GetSeatManager()->GetMaxSeats(); ++i)
	{
		//Check if the ped in the seat can do drivebys.
		const CPed* pPed = rVehicle.GetSeatManager()->GetPedInSeat(i);
		if(pPed && CanPedDoDrivebys(*pPed))
		{
			return true;
		}
	}

	return false;
}

bool CTaskVehicleChase::CanBlock() const
{
	//Ensure the vehicle is a car.
	if(GetPed()->GetVehiclePedInside()->GetVehicleType() != VEHICLE_TYPE_CAR)
	{
		return false;
	}

	//Don't try and block flying vehicles
	const CVehicle* pTargetVehicle = GetTargetVehicle();
	if(!pTargetVehicle || !pTargetVehicle->GetIsLandVehicle())
	{
		return false;
	}

	//Ensure the behavior flag is not set.
	if(IsBehaviorFlagSet(BF_CantBlock))
	{
		return false;
	}
	
	return true;
}

bool CTaskVehicleChase::CanDoDrivebys() const
{
	return (CanPedDoDrivebys(*GetPed()));
}

bool CTaskVehicleChase::CanMakeAggressiveMove() const
{
	//Ensure the running flag is not set.
	if(m_uRunningFlags.IsFlagSet(RF_DisableAggressiveMoves))
	{
		return false;
	}

	//Ensure the behavior flag is not set.
	if(IsBehaviorFlagSet(BF_CantMakeAggressiveMove))
	{
		return false;
	}

	//Ensure the vehicle is real.
	if(GetPed()->GetVehiclePedInside()->GetVehicleAiLod().GetDummyMode() != VDM_REAL)
	{
		return false;
	}

	//we can't be aggressive to flying vehicles
	const CVehicle* pTargetVehicle = GetTargetVehicle();
	if(!pTargetVehicle || !pTargetVehicle->GetIsLandVehicle())
	{
		return false;
	}

	//Ensure the facing dot is valid.
	static dev_float s_fMinFacingDot = 0.707f;
	if(m_Situation.m_fFacingDot < s_fMinFacingDot)
	{
		return false;
	}

	return true;
}

bool CTaskVehicleChase::CanPedDoDrivebys(const CPed& rPed) const
{
	return (rPed.GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanDoDrivebys));
}

bool CTaskVehicleChase::CanPullAlongside() const
{
	//Ensure the behavior flag is not set.
	if(IsBehaviorFlagSet(BF_CantPullAlongside))
	{
		return false;
	}

	//Ensure the flag is not set.
	if(GetPed()->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_DisablePullAlongsideDuringVehicleChase))
	{
		return false;
	}

	//Check if we can do drivebys.
	if(CanDoDrivebys())
	{
		return true;
	}
	//Check if anyone in our vehicle can do a driveby.
	else if(GetPed()->GetIsInVehicle() && CanAnyoneInVehicleDoDrivebys(*GetPed()->GetVehiclePedInside()))
	{
		return true;
	}

	return false;
}

bool CTaskVehicleChase::CanPursue() const
{
	//Ensure the behavior flag is not set.
	if(IsBehaviorFlagSet(BF_CantPursue))
	{
		return false;
	}

	return true;
}

bool CTaskVehicleChase::CanRam() const
{
	//Ensure the vehicle is a car.
	if(GetPed()->GetVehiclePedInside()->GetVehicleType() != VEHICLE_TYPE_CAR)
	{
		return false;
	}

	//don't ram big vehicles
	const CVehicle* pTargetVehicle = GetTargetVehicle();
	if(pTargetVehicle)
	{
		CVehicleModelInfo* pVehicleInfo = pTargetVehicle->GetVehicleModelInfo();
		if(pVehicleInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_BIG))
		{
			return false;
		}

		//don't ram flying vehicles...
		if(!pTargetVehicle->GetIsLandVehicle())
		{
			return false;
		}
	}
	
	//Ensure the behavior flag is not set.
	if(IsBehaviorFlagSet(BF_CantRam))
	{
		return false;
	}

	return true;
}

bool CTaskVehicleChase::CanSpinOut() const
{
	//Ensure the vehicle is a car.
	if(GetPed()->GetVehiclePedInside()->GetVehicleType() != VEHICLE_TYPE_CAR)
	{
		return false;
	}

	//don't spin flying vehicles...
	const CVehicle* pTargetVehicle = GetTargetVehicle();
	if(!pTargetVehicle || !pTargetVehicle->GetIsLandVehicle())
	{
		return false;
	}

	//Ensure the behavior flag is not set.
	if(IsBehaviorFlagSet(BF_CantSpinOut))
	{
		return false;
	}

	return true;
}

bool CTaskVehicleChase::CanUseHandBrakeForTurns() const
{
	//Check the state.
	switch(GetState())
	{
		case State_Start:
		case State_Analyze:
		case State_CloseDistance:
		case State_Finish:
		{
			return true;
		}
		case State_Block:
		case State_Pursue:
		case State_Ram:
		case State_SpinOut:
		case State_PullAlongside:
		case State_PullAlongsideInfront:
		{
			break;
		}
		default:
		{
			taskAssertf(false, "The state is invalid: %d.", GetState());
			return true;
		}
	}
	
	//Grab the vehicle.
	const CVehicle* pVehicle = GetPed()->GetVehiclePedInside();

	//Grab the vehicle position.
	Vec3V vVehiclePosition = pVehicle->GetVehiclePosition();
	
	//Calculate the vehicle direction.
	Vec3V vVehicleDirection = CalculateDirectionForVehicle(*pVehicle);

	//Grab the target.
	const CPhysical* pTarget = GetDominantTarget();

	//Grab the target position.
	Vec3V vTargetPosition = pTarget->GetTransform().GetPosition();
	
	//Calculate the direction from the vehicle to the target.
	Vec3V vVehicleToTarget = Subtract(vTargetPosition, vVehiclePosition);
	Vec3V vVehicleToTargetDirection = NormalizeFastSafe(vVehicleToTarget, Vec3V(V_ZERO));
	
	//Check if we can use the hand brake.
	ScalarV scDot = Dot(vVehicleDirection, vVehicleToTargetDirection);
	ScalarV scMaxDot = ScalarVFromF32(sm_Tunables.m_MaxDotForHandBrake);
	if(IsGreaterThanAll(scDot, scMaxDot))
	{
		return false;
	}
	else
	{
		return true;
	}
}

void CTaskVehicleChase::CheckForAggressiveMoveSuccess()
{
	//Ensure the flag is not set.
	if(m_uRunningFlags.IsFlagSet(RF_AggressiveMoveWasSuccessful))
	{
		return;
	}

	//Ensure we are making an aggressive move.
	if(!m_uRunningFlags.IsFlagSet(RF_IsMakingAggressiveMove))
	{
		return;
	}

	//Ensure our aggressive move is successful.
	if(!IsAggressiveMoveSuccessful())
	{
		return;
	}

	//Set the flag.
	m_uRunningFlags.SetFlag(RF_AggressiveMoveWasSuccessful);
}

s32 CTaskVehicleChase::ChooseAggressiveMoveFromPursue() const
{
	//Enumerate the possible states.
	enum State
	{
		Ram = 0,
		Block,
		SpinOut,
		PullAlongside,
		MaxStates
	};

	//Create weights for each state.
	float afWeights[MaxStates];
	afWeights[Ram]				= IsRamAppropriate()			? sm_Tunables.m_AggressiveMove.m_WeightToRamFromPursue				: 0.0f;
	afWeights[Block]			= IsBlockAppropriate()			? sm_Tunables.m_AggressiveMove.m_WeightToBlockFromPursue			: 0.0f;
	afWeights[SpinOut]			= IsSpinOutAppropriate()		? sm_Tunables.m_AggressiveMove.m_WeightToSpinOutFromPursue			: 0.0f;
	afWeights[PullAlongside]	= IsPullAlongsideAppropriate()	? sm_Tunables.m_AggressiveMove.m_WeightToPullAlongsideFromPursue	: 0.0f;
	
	if (m_uRunningFlags.IsFlagSet(RF_IsLowSpeedChase))
	{
		static dev_float sfMultiplierForBlockInLowSpeedChase = 2.0f;
		static dev_float sfMultiplierForRamInLowSpeedChase = 0.5f;
		afWeights[Block] *= sfMultiplierForBlockInLowSpeedChase;
		afWeights[Ram] *= sfMultiplierForRamInLowSpeedChase;
	}

	//If we are frustrated, and can ram, use it.
	if(IsFrustrated() && (afWeights[Ram] > 0.0f))
	{
		return State_Ram;
	}

	//Sum up the weights.
	float fWeightTotal = 0.0f;
	for(int i = 0; i < MaxStates; ++i)
	{
		fWeightTotal += afWeights[i];
	}

	//Ensure the weight total is valid.
	if(fWeightTotal < FLT_EPSILON)
	{
		return State_Invalid;
	}

	//Choose a random number.
	float fRandom = fwRandom::GetRandomNumberInRange(0.0f, fWeightTotal);

	//Choose the state.
	State iState = Ram;
	for(int i = 0; i < MaxStates; ++i)
	{
		//Grab the weight.
		float fWeight = afWeights[i];

		//Check if the current weight has been hit.
		if(fRandom < fWeight)
		{
			iState = (State)i;
			break;
		}
		//Decrease by the weight.
		else
		{
			fRandom -= fWeight;
		}
	}

	//Check the state.
	switch(iState)
	{
		case Ram:
		{
			return State_Ram;
		}
		case Block:
		{
			return State_Block;
		}
		case SpinOut:
		{
			return State_SpinOut;
		}
		case PullAlongside:
		{
			return State_PullAlongside;
		}
		default:
		{
			taskAssertf(false, "Invalid state: %d.", iState);
			return State_Invalid;
		}
	}
}

s32 CTaskVehicleChase::ChooseAppropriateStateForAnalyze() const
{
	//Check if close distance is appropriate.
	if(IsPullAlongsideInfrontAppropriate())
	{
		return State_PullAlongsideInfront;
	}
	//Check if close distance is appropriate.
	else if(IsCloseDistanceAppropriate())
	{
		return State_CloseDistance;
	}
	//Check if pursue is appropriate.
	else if(IsPursueAppropriate())
	{
		return State_Pursue;
	}
	//Check if block is appropriate.
	else if(IsBlockAppropriate())
	{
		return State_Block;
	}
	
	return State_Invalid;
}

s32 CTaskVehicleChase::ChooseAppropriateStateForBlock()
{
	//Check if a block is no longer appropriate.
	if(IsBlockNoLongerAppropriate())
	{
		//Check if we are making an aggressive move.
		if(IsMakingAggressiveMove())
		{
			return State_Analyze;
		}
		else
		{
			//Check if the state has just become invalid.
			if(m_uTimeStateBecameInvalid == 0)
			{
				//Set the time.
				m_uTimeStateBecameInvalid = fwTimer::GetTimeInMilliseconds();
			}
			else
			{
				//Check if the state has been invalid long enough.
				static dev_u32 s_uMinTime = 1000;
				if(CTimeHelpers::GetTimeSince(m_uTimeStateBecameInvalid) > s_uMinTime)
				{
					return State_Analyze;
				}
			}
		}
	}
	else
	{
		//Clear the time.
		m_uTimeStateBecameInvalid = 0;
	}
	
	return State_Invalid;
}

s32 CTaskVehicleChase::ChooseAppropriateStateForCloseDistance() const
{
	//Check if close distance is no longer appropriate.
	if(IsCloseDistanceNoLongerAppropriate())
	{
		return State_Analyze;
	}
	else
	{
		return State_Invalid;
	}
}

s32 CTaskVehicleChase::ChooseAppropriateStateForPullAlongside() const
{
	//Check if pull alongside is no longer appropriate.
	if(IsPullAlongsideNoLongerAppropriate())
	{
		if(ShouldFallBack())
		{
			return State_Fallback;
		}
		else
		{
			return State_Analyze;
		}	
	}
	else
	{
		return State_Invalid;
	}
}

s32 CTaskVehicleChase::ChooseAppropriateStateForPursue()
{
	//Check if a pursue is no longer appropriate.
	if(IsPursueNoLongerAppropriate())
	{
		//Check if the state has just become invalid.
		if(m_uTimeStateBecameInvalid == 0)
		{
			//Set the time.
			m_uTimeStateBecameInvalid = fwTimer::GetTimeInMilliseconds();
		}
		else
		{
			//Check if the state has been invalid long enough.
			static dev_u32 s_uMinTime = 1000;
			if(CTimeHelpers::GetTimeSince(m_uTimeStateBecameInvalid) > s_uMinTime)
			{
				return State_Analyze;
			}
		}
	}
	else
	{
		//Clear the time.
		m_uTimeStateBecameInvalid = 0;

		//Check if we should make an aggressive move.
		if(ShouldMakeAggressiveMove())
		{
			return ChooseAggressiveMoveFromPursue();
		}
	}

	return State_Invalid;
}

s32 CTaskVehicleChase::ChooseAppropriateStateForRam() const
{
	//Check if a ram is no longer appropriate.
	if(IsRamNoLongerAppropriate())
	{
		return State_Analyze;
	}
	else
	{
		return State_Invalid;
	}
}

s32 CTaskVehicleChase::ChooseAppropriateStateForSpinOut() const
{
	//Check if a spin out is no longer appropriate.
	if(IsSpinOutNoLongerAppropriate())
	{
		return State_Analyze;
	}
	else
	{
		return State_Invalid;
	}
}

float CTaskVehicleChase::GetDesiredSpeed() const
{
	//Ensure the vehicle is valid.
	const CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
	if(!pVehicle)
	{
		return 0.0f;
	}

	//Ensure the active task is valid.
	CTaskVehicleMissionBase* pTask = pVehicle->GetIntelligence()->GetActiveTask();
	if(!pTask)
	{
		return 0.0f;
	}

	//Use the cruise speed.
	return pTask->GetCruiseSpeed();
}

const CPhysical* CTaskVehicleChase::GetDominantTarget() const
{
	//Ensure the target entity is valid.
	const CEntity* pEntity = m_Target.GetEntity();
	if(!pEntity)
	{
		return NULL;
	}

	const CEntity* pRetVal = pEntity;

	//Check if the entity is a ped.
	if(pEntity->GetIsTypePed())
	{
		//Grab the ped.
		const CPed* pPed = static_cast<const CPed *>(pEntity);

		//Check if the ped is in a vehicle.
		const CVehicle* pVehicle = pPed->GetVehiclePedInside();
		const CPed* pMount = pPed->GetMountPedOn();
		if(pVehicle)
		{
			//return pVehicle;
			pRetVal = pVehicle;
		}
		else if(pMount) //Check if the ped is on a mount.
		{
			return pMount;
		}
		else
		{
			return pPed;
		}
	}

	//Check if the entity is a vehicle.
	if(pRetVal->GetIsTypeVehicle())
	{
		const CVehicle* pRetVeh = static_cast<const CVehicle *>(pRetVal);
		const CTrailer* pAttachedTrailer = pRetVeh->GetAttachedTrailer();
		if (pAttachedTrailer)
		{
			return static_cast<const CVehicle *>(pAttachedTrailer);
		}
		else
		{
			return static_cast<const CVehicle *>(pRetVeh);
		}
	}
	else
	{
		return NULL;
	}
}

const CVehicle* CTaskVehicleChase::GetTargetVehicle() const
{
	//Get the target.
	const CPhysical* pTarget = GetDominantTarget();

	//Check if the target is a ped.
	if(pTarget->GetIsTypePed())
	{
		//Grab the ped.
		const CPed* pTargetPed = static_cast<const CPed *>(pTarget);

		return pTargetPed->GetVehiclePedInside();
	}
	//Check if the target is a vehicle.
	else if(pTarget->GetIsTypeVehicle())
	{
		//Grab the vehicle.
		const CVehicle* pTargetVehicle = static_cast<const CVehicle *>(pTarget);

		return pTargetVehicle;
	}
	else
	{
		return NULL;
	}
}

CTaskVehicleBlock* CTaskVehicleChase::GetVehicleTaskForBlock() const
{
	//Ensure we are blocking.
	if(!IsBlocking())
	{
		return NULL;
	}

	//Ensure the vehicle is valid.
	const CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
	if(!pVehicle)
	{
		return NULL;
	}

	//Ensure the vehicle task is valid.
	CTask* pVehicleTask = pVehicle->GetIntelligence()->GetActiveTask();
	if(!pVehicleTask)
	{
		return NULL;
	}

	//Ensure the vehicle task is the correct type.
	if(pVehicleTask->GetTaskType() != CTaskTypes::TASK_VEHICLE_BLOCK)
	{
		return NULL;
	}
	
	return static_cast<CTaskVehicleBlock *>(pVehicleTask);
}

CTaskVehiclePursue* CTaskVehicleChase::GetVehicleTaskForPursue() const
{
	//Ensure we are pursuing.
	if(!IsPursuing())
	{
		return NULL;
	}

	//Ensure the vehicle is valid.
	const CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
	if(!pVehicle)
	{
		return NULL;
	}

	//Ensure the vehicle task is valid.
	CTask* pVehicleTask = pVehicle->GetIntelligence()->GetActiveTask();
	if(!pVehicleTask)
	{
		return NULL;
	}

	//Ensure the vehicle task is the correct type.
	if(pVehicleTask->GetTaskType() != CTaskTypes::TASK_VEHICLE_PURSUE)
	{
		return NULL;
	}

	return static_cast<CTaskVehiclePursue *>(pVehicleTask);
}

CTaskVehicleRam* CTaskVehicleChase::GetVehicleTaskForRam() const
{
	//Ensure we are ramming.
	if(!IsRamming())
	{
		return NULL;
	}

	//Ensure the vehicle is valid.
	const CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
	if(!pVehicle)
	{
		return NULL;
	}

	//Ensure the vehicle task is valid.
	CTask* pVehicleTask = pVehicle->GetIntelligence()->GetActiveTask();
	if(!pVehicleTask)
	{
		return NULL;
	}

	//Ensure the vehicle task is the correct type.
	if(pVehicleTask->GetTaskType() != CTaskTypes::TASK_VEHICLE_RAM)
	{
		return NULL;
	}

	return static_cast<CTaskVehicleRam *>(pVehicleTask);
}

bool CTaskVehicleChase::HasRamMadeContact() const
{
	//Grab the vehicle.
	const CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
	
	//Grab the vehicle task.
	const CTask* pVehicleTask = pVehicle->GetIntelligence()->GetActiveTask();
	if(!pVehicleTask)
	{
		return false;
	}
	
	//Ensure the vehicle task is the correct type.
	if(pVehicleTask->GetTaskType() != CTaskTypes::TASK_VEHICLE_RAM)
	{
		return false;
	}
	
	//Grab the ram task.
	const CTaskVehicleRam* pTask = static_cast<const CTaskVehicleRam *>(pVehicleTask);
	
	return (pTask->GetNumContacts() != 0);
}

bool CTaskVehicleChase::HasSpinOutMadeContact() const
{
	//Grab the vehicle.
	const CVehicle* pVehicle = GetPed()->GetVehiclePedInside();

	//Grab the vehicle task.
	const CTask* pVehicleTask = pVehicle->GetIntelligence()->GetActiveTask();
	if(!pVehicleTask)
	{
		return false;
	}

	//Ensure the vehicle task is the correct type.
	if(pVehicleTask->GetTaskType() != CTaskTypes::TASK_VEHICLE_SPIN_OUT)
	{
		return false;
	}

	//Grab the ram task.
	const CTaskVehicleSpinOut* pTask = static_cast<const CTaskVehicleSpinOut *>(pVehicleTask);

	return (pTask->HasMadeContact());
}

bool CTaskVehicleChase::IsAggressiveMoveSuccessful() const
{
	//Check the state.
	switch(GetState())
	{
		case State_Block:
		{
			return IsBlockSuccessful();
		}
		case State_Ram:
		{
			return IsRamSuccessful();
		}
		case State_SpinOut:
		{
			return IsSpinOutSuccessful();
		}
		case State_PullAlongside:
		{
			return IsPullAlongsideSuccessful();
		}
		default:
		{
			return false;
		}
	}
}

bool CTaskVehicleChase::IsBehaviorFlagSet(BehaviorFlags nBehaviorFlag) const
{
	//Iterate over the overrides.
	for(int i = Overrides::FirstSource; i < Overrides::MaxSources; ++i)
	{
		//Check if the behavior flag has been set.
		if(m_Overrides[i].m_uBehaviorFlags.IsFlagSet((u16)nBehaviorFlag))
		{
			return true;
		}
	}
	
	return false;
}

bool CTaskVehicleChase::IsBlockAppropriate() const
{
	//Ensure we can block.
	if(!CanBlock())
	{
		return false;
	}

	//Check the state.
	switch(GetState())
	{
		case State_Analyze:
		{
			return IsBlockAppropriateFromAnalyze();
		}
		case State_Pursue:
		{
			return IsBlockAppropriateFromPursue();
		}
		default:
		{
			return false;
		}
	}
}

bool CTaskVehicleChase::IsBlockAppropriateFromAnalyze() const
{
	//Check if we are forced into block.
	if(AreWeForcedIntoBlock())
	{
		return true;
	}
	
	//Create the input.
	IsSituationValidInput input;
	input.m_fMaxDot = sm_Tunables.m_Block.m_MaxDotToStartFromAnalyze;
	input.m_uFlags = IsSituationValidInput::CheckMaxDot;
	
	return IsSituationValid(input);
}

bool CTaskVehicleChase::IsBlockAppropriateFromPursue() const
{
	//Ensure we are not stuck.
	if(m_uRunningFlags.IsFlagSet(RF_IsStuck))
	{
		return false;
	}

	//Ensure the behavior flag is not set.
	if(IsBehaviorFlagSet(BF_CantBlockFromPursue))
	{
		return false;
	}

	//Ensure the flag is not set.
	if(GetPed()->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_DisableBlockFromPursueDuringVehicleChase))
	{
		return false;
	}
	
	//Create the input.
	IsSituationValidInput input;
	LoadSituationToStartAggressiveMoveFromPursue(input);
	input.m_fMinTargetSpeed = sm_Tunables.m_Block.m_MinTargetSpeedToStartFromPursue;
	input.m_uFlags.SetFlag(IsSituationValidInput::CheckMinTargetSpeed);
	
	return IsSituationValid(input);
}

bool CTaskVehicleChase::IsBlockNoLongerAppropriate() const
{
	//Ensure we are not stuck.
	if(m_uRunningFlags.IsFlagSet(RF_IsStuck))
	{
		return false;
	}

	//Check if we can't block.
	if(!CanBlock())
	{
		return true;
	}

	//Check the previous state.
	switch(GetPreviousState())
	{
		case State_Analyze:
		{
			return IsBlockNoLongerAppropriateFromAnalyze();
		}
		case State_Pursue:
		{
			return IsBlockNoLongerAppropriateFromPursue();
		}
		default:
		{
			return true;
		}
	}
}

bool CTaskVehicleChase::IsBlockNoLongerAppropriateFromAnalyze() const
{
	//Check if we are forced into block.
	if(AreWeForcedIntoBlock())
	{
		return false;
	}
	
	//Create the input.
	IsSituationValidInput input;
	input.m_fMaxDot = sm_Tunables.m_Block.m_MaxDotToContinueFromAnalyze;
	input.m_uFlags = IsSituationValidInput::CheckMaxDot;
	
	return !IsSituationValid(input);
}

bool CTaskVehicleChase::IsBlockNoLongerAppropriateFromPursue() const
{
	//Ensure we are not stuck.
	if(m_uRunningFlags.IsFlagSet(RF_IsStuck))
	{
		return true;
	}

	//Create the input.
	IsSituationValidInput input;
	LoadSituationToContinueAggressiveMoveFromPursue(input);
	input.m_fMinTargetSpeed = sm_Tunables.m_Block.m_MinTargetSpeedToContinueFromPursue;
	input.m_uFlags.SetFlag(IsSituationValidInput::CheckMinTargetSpeed);
	
	//Remove the dot checks.  We will be moving from behind the target to the front.
	input.m_uFlags.ClearFlag(IsSituationValidInput::CheckMinDot);
	input.m_uFlags.ClearFlag(IsSituationValidInput::CheckMaxDot);
	
	return !IsSituationValid(input);
}

bool CTaskVehicleChase::IsBlockSuccessful() const
{
	//Ensure the situation is valid.
	IsSituationValidInput input;
	input.m_fMaxDot = -0.707f;
	input.m_uFlags = IsSituationValidInput::CheckMaxDot;
	if(!IsSituationValid(input))
	{
		return false;
	}

	//Block successful
#if GTA_REPLAY 
	//If the block was close to the player then add event
	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if(pPlayer && (VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition()) - VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition())).Mag2() < rage::square(20) )
	{
		ReplayBufferMarkerMgr::AddMarker(4000,2000,IMPORTANCE_LOW);
	}
#endif  //GTA_REPLAY

	return true;
}

bool CTaskVehicleChase::IsCloseDistanceAppropriate() const
{
	//Create the input.
	IsSituationValidInput input;
	input.m_fMinDistance = sm_Tunables.m_CloseDistance.m_MinDistanceToStart;
	input.m_uFlags = IsSituationValidInput::CheckMinDistance;

	return IsSituationValid(input);
}

bool CTaskVehicleChase::IsCloseDistanceNoLongerAppropriate() const
{
	//Create the input.
	IsSituationValidInput input;
	input.m_fMinDistance = sm_Tunables.m_CloseDistance.m_MinDistanceToContinue;
	input.m_uFlags = IsSituationValidInput::CheckMinDistance;

	return !IsSituationValid(input);
}

bool CTaskVehicleChase::IsFrustrated() const
{
	static dev_float s_fMinTime = 10.0f;
	return (m_fFrustrationTimer > s_fMinTime);
}

bool CTaskVehicleChase::IsMakingAggressiveMoveFromAnalyze() const
{
	//Check the state.
	switch(GetState())
	{
		case State_Ram:
		{
			return true;
		}
		default:
		{
			return false;
		}
	}
}

bool CTaskVehicleChase::IsMakingAggressiveMoveFromPursue() const
{
	//Check the state.
	switch(GetState())
	{
		case State_Block:
		{
			return IsGettingInFrontToBlock();
		}
		case State_Ram:
		case State_SpinOut:
		case State_PullAlongside:
		{
			return true;
		}
		default:
		{
			return false;
		}
	}
}

bool CTaskVehicleChase::IsPullAlongsideAppropriate() const
{
	//Ensure we can spin out.
	if(!CanPullAlongside())
	{
		return false;
	}

	//Ensure we are not stuck.
	if(m_uRunningFlags.IsFlagSet(RF_IsStuck))
	{
		return false;
	}

	//Check the state.
	switch(GetState())
	{
		case State_Pursue:
		{
			return IsPullAlongsideAppropriateFromPursue();
		}
		default:
		{
			return false;
		}
	}
}

bool CTaskVehicleChase::IsPullAlongsideAppropriateFromPursue() const
{
	//Create the input.
	IsSituationValidInput input;
	LoadSituationToStartAggressiveMoveFromPursue(input);
	input.m_fMinTargetSpeed = sm_Tunables.m_PullAlongside.m_MinTargetSpeedToStartFromPursue;
	input.m_uFlags.SetFlag(IsSituationValidInput::CheckMinTargetSpeed);

	return IsSituationValid(input);
}

bool CTaskVehicleChase::IsPullAlongsideNoLongerAppropriate() const
{
	//Ensure we can spin out.
	if(!CanPullAlongside())
	{
		return true;
	}

	//Ensure we are not stuck.
	if(m_uRunningFlags.IsFlagSet(RF_IsStuck))
	{
		return true;
	}

	//Check the previous state.
	switch(GetPreviousState())
	{
		case State_Pursue:
		{
			return IsPullAlongsideNoLongerAppropriateFromPursue();
		}
		default:
		{
			return true;
		}
	}
}	

bool CTaskVehicleChase::IsPullAlongsideNoLongerAppropriateFromPursue() const
{
	//Create the input.
	IsSituationValidInput input;
	LoadSituationToContinueAggressiveMoveFromPursue(input);
	input.m_fMinTargetSpeed = sm_Tunables.m_PullAlongside.m_MinTargetSpeedToContinueFromPursue;
	input.m_uFlags.SetFlag(IsSituationValidInput::CheckMinTargetSpeed);

	return !IsSituationValid(input);
}

bool CTaskVehicleChase::IsPullAlongsideSuccessful() const
{
	//Grab the vehicle.
	const CVehicle* pVehicle = GetPed()->GetVehiclePedInside();

	//Grab the vehicle task.
	const CTask* pVehicleTask = pVehicle->GetIntelligence()->GetActiveTask();
	if(!pVehicleTask)
	{
		return false;
	}

	//Ensure the vehicle task is the correct type.
	if(pVehicleTask->GetTaskType() != CTaskTypes::TASK_VEHICLE_PULL_ALONGSIDE)
	{
		return false;
	}

	//Grab the pull alongside task.
	const CTaskVehiclePullAlongside* pTask = static_cast<const CTaskVehiclePullAlongside *>(pVehicleTask);

	bool bSuccessful = (pTask->HasBeenAlongside());

#if GTA_REPLAY 
	//If the pull alongside was close to the player then add event 
	if(bSuccessful)
	{
		CPed* pPlayer = CGameWorld::FindLocalPlayer();
		if(pPlayer && (VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition()) - VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition())).Mag2() < rage::square(20) )
		{
			ReplayBufferMarkerMgr::AddMarker(4000,2000,IMPORTANCE_LOW);
		}
	}
#endif  //GTA_REPLAY

	return bSuccessful;
}

bool CTaskVehicleChase::IsPursueAppropriate() const
{
	//Ensure we can pursue.
	if(!CanPursue())
	{
		return false;
	}

	//Check the state.
	switch(GetState())
	{
		case State_Analyze:
		{
			return IsPursueAppropriateFromAnalyze();
		}
		default:
		{
			return false;
		}
	}
}

bool CTaskVehicleChase::IsPursueAppropriateFromAnalyze() const
{
	//Check if we are forced into pursue.
	if(AreWeForcedIntoPursue())
	{
		return true;
	}
	
	//Create the input.
	IsSituationValidInput input;
	input.m_fMinDot = sm_Tunables.m_Pursue.m_MinDotToStartFromAnalyze;
	input.m_uFlags = IsSituationValidInput::CheckMinDot;

	return IsSituationValid(input);
}

bool CTaskVehicleChase::IsPursueNoLongerAppropriate() const
{
	//Ensure we aren't stuck.
	if(m_uRunningFlags.IsFlagSet(RF_IsStuck))
	{
		return false;
	}

	//Check if we can't pursue.
	if(!CanPursue())
	{
		return true;
	}

	//Check the previous state.
	switch(GetPreviousState())
	{
		case State_Analyze:
		{
			return IsPursueNoLongerAppropriateFromAnalyze();
		}
		default:
		{
			return true;
		}
	}
}

bool CTaskVehicleChase::IsPursueNoLongerAppropriateFromAnalyze() const
{
	//Check if we are forced into pursue.
	if(AreWeForcedIntoPursue())
	{
		return false;
	}
	
	//Create the input.
	IsSituationValidInput input;
	input.m_fMinDot = sm_Tunables.m_Pursue.m_MinDotToContinueFromAnalyze;
	input.m_uFlags = IsSituationValidInput::CheckMinDot;

	return !IsSituationValid(input);
}

bool CTaskVehicleChase::IsRamAppropriate() const
{
	//Ensure we can ram.
	if(!CanRam())
	{
		return false;
	}

	//Ensure we are not stuck.
	if(m_uRunningFlags.IsFlagSet(RF_IsStuck))
	{
		return false;
	}

	//Check the state.
	switch(GetState())
	{
		case State_Pursue:
		{
			return IsRamAppropriateFromPursue();
		}
		default:
		{
			return false;
		}
	}
}
	
bool CTaskVehicleChase::IsRamAppropriateFromPursue() const
{
	//Create the input.
	IsSituationValidInput input;
	LoadSituationToStartAggressiveMoveFromPursue(input);
	input.m_fMinTargetSpeed = sm_Tunables.m_Ram.m_MinTargetSpeedToStartFromPursue;
	input.m_uFlags.SetFlag(IsSituationValidInput::CheckMinTargetSpeed);

	return IsSituationValid(input);
}

bool CTaskVehicleChase::IsRamNoLongerAppropriate() const
{
	//Ensure we can ram.
	if(!CanRam())
	{
		return true;
	}

	//Ensure we are not stuck.
	if(m_uRunningFlags.IsFlagSet(RF_IsStuck))
	{
		return true;
	}

	//Check the previous state.
	switch(GetPreviousState())
	{
		case State_Pursue:
		{
			return IsRamNoLongerAppropriateFromPursue();
		}
		default:
		{
			return true;
		}
	}
}

bool CTaskVehicleChase::IsRamNoLongerAppropriateFromPursue() const
{
	//Check if the ram has made contact, and we are not using a continuous ram.
	bool bHasRamMadeContact = HasRamMadeContact();
	bool bShouldUseContinuousRam = ShouldUseContinuousRam();
	if(bHasRamMadeContact && !bShouldUseContinuousRam)
	{
		return true;
	}
	
	//Create the input.
	IsSituationValidInput input;
	LoadSituationToContinueAggressiveMoveFromPursue(input);
	input.m_fMinTargetSpeed = sm_Tunables.m_Ram.m_MinTargetSpeedToContinueFromPursue;
	input.m_uFlags.SetFlag(IsSituationValidInput::CheckMinTargetSpeed);

	//If we have made contact, and are using a continuous ram, ignore some of the checks.
	if(bHasRamMadeContact && bShouldUseContinuousRam)
	{
		input.m_uFlags.ClearFlag(IsSituationValidInput::CheckMinDot);
		input.m_uFlags.ClearFlag(IsSituationValidInput::CheckMinTargetSpeed);
		input.m_uFlags.ClearFlag(IsSituationValidInput::CheckMaxTargetSteerAngle);
	}

	return !IsSituationValid(input);
}

bool CTaskVehicleChase::IsRamSuccessful() const
{
	//Ensure the ram has made contact.
	if(!HasRamMadeContact())
	{
		return false;
	}

#if GTA_REPLAY 
	//If the ram was close to the player then add event
	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if(pPlayer && (VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition()) - VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition())).Mag2() < rage::square(20) )
	{
		ReplayBufferMarkerMgr::AddMarker(4000,2000,IMPORTANCE_LOW);
	}
#endif  //GTA_REPLAY

	return true;
}

bool CTaskVehicleChase::IsSpinOutAppropriate() const
{
	//Ensure we can spin out.
	if(!CanSpinOut())
	{
		return false;
	}

	//Ensure we are not stuck.
	if(m_uRunningFlags.IsFlagSet(RF_IsStuck))
	{
		return false;
	}

	//Ensure the flag is not set.
	if(GetPed()->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_DisableSpinOutDuringVehicleChase))
	{
		return false;
	}

	//Check the state.
	switch(GetState())
	{
		case State_Pursue:
		{
			return IsSpinOutAppropriateFromPursue();
		}
		default:
		{
			return false;
		}
	}
}

bool CTaskVehicleChase::IsSpinOutAppropriateFromPursue() const
{
	//Create the input.
	IsSituationValidInput input;
	LoadSituationToStartAggressiveMoveFromPursue(input);
	input.m_fMinTargetSpeed = sm_Tunables.m_SpinOut.m_MinTargetSpeedToStartFromPursue;
	input.m_uFlags.SetFlag(IsSituationValidInput::CheckMinTargetSpeed);

	return IsSituationValid(input);
}

bool CTaskVehicleChase::IsSpinOutNoLongerAppropriate() const
{
	//Ensure we can spin out.
	if(!CanSpinOut())
	{
		return true;
	}

	//Ensure we are not stuck.
	if(m_uRunningFlags.IsFlagSet(RF_IsStuck))
	{
		return true;
	}

	//Check the previous state.
	switch(GetPreviousState())
	{
		case State_Pursue:
		{
			return IsSpinOutNoLongerAppropriateFromPursue();
		}
		default:
		{
			return true;
		}
	}
}	

bool CTaskVehicleChase::IsSpinOutNoLongerAppropriateFromPursue() const
{
	//Create the input.
	IsSituationValidInput input;
	LoadSituationToContinueAggressiveMoveFromPursue(input);
	input.m_fMinTargetSpeed = sm_Tunables.m_SpinOut.m_MinTargetSpeedToContinueFromPursue;
	input.m_uFlags.SetFlag(IsSituationValidInput::CheckMinTargetSpeed);

	return !IsSituationValid(input);
}

bool CTaskVehicleChase::IsSpinOutSuccessful() const
{
	//Ensure the spin out has made contact.
	if(!HasSpinOutMadeContact())
	{
		return false;
	}

	return true;
}

bool CTaskVehicleChase::IsStuck() const
{
	//Ensure our vehicle is valid.
	const CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
	if(!pVehicle)
	{
		return false;
	}

	return CVehicleStuckDuringCombatHelper::IsStuck(*pVehicle);
}

void CTaskVehicleChase::LoadSituationToContinueAggressiveMoveFromPursue(IsSituationValidInput& rInput) const
{
	//Load the input.
	rInput.m_fMaxDistance = sm_Tunables.m_AggressiveMove.m_MaxDistanceToContinueFromPursue;
	rInput.m_fMinDot = sm_Tunables.m_AggressiveMove.m_MinDotToContinueFromPursue;
	rInput.m_fMaxTimeInState = sm_Tunables.m_AggressiveMove.m_MaxTimeInStateToContinueFromPursue;
	rInput.m_fMaxTargetSteerAngle = sm_Tunables.m_AggressiveMove.m_MaxTargetSteerAngleToContinueFromPursue;
	rInput.m_uFlags = IsSituationValidInput::CheckMaxDistance | IsSituationValidInput::CheckMinDot | IsSituationValidInput::CheckMaxTimeInState | IsSituationValidInput::CheckMaxTargetSteerAngle;
	if (m_uRunningFlags.IsFlagSet(RF_IsLowSpeedChase))
	{
		rInput.m_uFlags.ClearFlag(IsSituationValidInput::CheckMaxTargetSteerAngle);
	}
}

void CTaskVehicleChase::LoadSituationToStartAggressiveMoveFromPursue(IsSituationValidInput& rInput) const
{
	//Load the input.
	rInput.m_fMaxDistance = sm_Tunables.m_AggressiveMove.m_MaxDistanceToStartFromPursue;
	rInput.m_fMinDot = sm_Tunables.m_AggressiveMove.m_MinDotToStartFromPursue;
	rInput.m_fMinSpeedLeeway = sm_Tunables.m_AggressiveMove.m_MinSpeedLeewayToStartFromPursue;
	rInput.m_fMaxTargetSteerAngle = sm_Tunables.m_AggressiveMove.m_MaxTargetSteerAngleToStartFromPursue;
	rInput.m_uFlags = IsSituationValidInput::CheckMaxDistance | IsSituationValidInput::CheckMinDot | IsSituationValidInput::CheckMinSpeedLeeway | IsSituationValidInput::CheckMaxTargetSteerAngle;
	if (m_uRunningFlags.IsFlagSet(RF_IsLowSpeedChase))
	{
		rInput.m_uFlags.ClearFlag(IsSituationValidInput::CheckMaxTargetSteerAngle);
	}
}

void CTaskVehicleChase::LoadVehicleMissionParameters(sVehicleMissionParams& rParams)
{
	//Load the vehicle mission parameters.
	if (aiVerify(GetPed() && GetPed()->GetVehiclePedInside() && GetDominantTarget()))
	{
		LoadVehicleMissionParameters(*GetPed(), *GetPed()->GetVehiclePedInside(), *GetDominantTarget(), rParams);
	}
}

void CTaskVehicleChase::OnDesiredOffsetForPursueChanged()
{
	//Ensure the task is valid.
	CTaskVehiclePursue* pTask = GetVehicleTaskForPursue();
	if(!pTask)
	{
		return;
	}
	
	//Set the desired offset.
	pTask->SetDesiredOffset(CalculateDesiredOffsetForPursue());
}

void CTaskVehicleChase::OnIdealDistanceForPursueChanged()
{
	//Ensure the task is valid.
	CTaskVehiclePursue* pTask = GetVehicleTaskForPursue();
	if(!pTask)
	{
		return;
	}
	
	//Set the ideal distance.
	pTask->SetIdealDistance(CalculateIdealDistanceForPursue());
}

void CTaskVehicleChase::ProcessAggressiveMove()
{
	//Check if aggressive moves should be disabled.
	float fTimeBetweenAggressiveMoves = GetPed()->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatTimeBetweenAggressiveMovesDuringVehicleChase);
	bool bDisableAggressiveMoves = (fTimeBetweenAggressiveMoves == 0.0f);
	if(bDisableAggressiveMoves)
	{
		//Set the flag.
		m_uRunningFlags.SetFlag(RF_DisableAggressiveMoves);
	}
	//Check if aggressive moves were disabled.
	else if(m_uRunningFlags.IsFlagSet(RF_DisableAggressiveMoves))
	{
		//Clear the flag.
		m_uRunningFlags.ClearFlag(RF_DisableAggressiveMoves);

		//Re-calculate the delay.
		m_fAggressiveMoveDelay = CalculateAggressiveMoveDelay();
	}

	//Check if we can make an aggressive move.
	if(CanMakeAggressiveMove())
	{
		//Increase the frustration timer.
		m_fFrustrationTimer += GetTimeStep();
	}

	//Check if we were making an aggressive move.
	bool bWasMakingAggressiveMove = m_uRunningFlags.IsFlagSet(RF_IsMakingAggressiveMove);
	
	//Check if we are making an aggressive move.
	bool bIsMakingAggressiveMove = IsMakingAggressiveMove();
	
	//Update the flag.
	m_uRunningFlags.ChangeFlag(RF_IsMakingAggressiveMove, bIsMakingAggressiveMove);
	
	//Check if we have started an aggressive move.
	if(!bWasMakingAggressiveMove && bIsMakingAggressiveMove)
	{
		//Reset the delay.
		m_fAggressiveMoveDelay = CalculateAggressiveMoveDelay();
	}
	//Check if we have stopped making an aggressive move.
	else if(bWasMakingAggressiveMove && !bIsMakingAggressiveMove)
	{
		//Check if our aggressive move was not successful.
		if(!m_uRunningFlags.IsFlagSet(RF_AggressiveMoveWasSuccessful))
		{
			//Clear the aggressive move delay.
			m_fAggressiveMoveDelay = 0.0f;
		}
		else
		{
			//Clear the flag.
			m_uRunningFlags.ClearFlag(RF_AggressiveMoveWasSuccessful);

			//Clear the frustration timer.
			m_fFrustrationTimer = 0.0f;
		}
	}
	//Check if we are no longer making an aggressive move.
	else if(!bIsMakingAggressiveMove)
	{
		//Decrease the delay.
		m_fAggressiveMoveDelay -= GetTimeStep();
	}
}

void CTaskVehicleChase::ProcessCheatFlags()
{
	//Ensure the vehicle is valid.
	CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
	if(!pVehicle)
	{
		return;
	}

	//Check if we should use cheat flags.
	if(ShouldUseCheatFlags())
	{
		pVehicle->SetCheatFlag(VEH_CHEAT_GRIP2);
		pVehicle->SetCheatFlag(VEH_CHEAT_TC);
		pVehicle->SetCheatFlag(VEH_CHEAT_SC);
	}
	else
	{
		pVehicle->ClearCheatFlag(VEH_CHEAT_GRIP2);
		pVehicle->ClearCheatFlag(VEH_CHEAT_TC);
		pVehicle->ClearCheatFlag(VEH_CHEAT_SC);
	}
}

void CTaskVehicleChase::ProcessCheatPowerIncrease()
{
	//Ensure the vehicle is valid.
	CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
	if(!pVehicle)
	{
		return;
	}

	//Calculate the cheat power increase.
	float fCheatPowerIncrease = ShouldUseCheatPowerIncrease() ? CalculateCheatPowerIncrease() : 1.0f;

	//Set the cheat power increase.
	pVehicle->SetCheatPowerIncrease(fCheatPowerIncrease);
}

void CTaskVehicleChase::ProcessHandBrake()
{
	//Grab the vehicle.
	CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
	
	//Ensure the vehicle task is valid.
	CTaskVehicleMissionBase* pVehicleTask = pVehicle->GetIntelligence()->GetActiveTask();
	if(!pVehicleTask)
	{
		return;
	}
	
	//Ensure the task is valid.
	CTaskVehicleGoToPointAutomobile* pTask = static_cast<CTaskVehicleGoToPointAutomobile *>(pVehicleTask->FindSubTaskOfType(CTaskTypes::TASK_VEHICLE_GOTO_POINT_AUTOMOBILE));
	if(!pTask)
	{
		return;
	}
	
	//Set whether we can use the hand brake for turns.
	bool bCanUseHandBrakeForTurns = CanUseHandBrakeForTurns();
	pTask->SetCanUseHandBrakeForTurns(bCanUseHandBrakeForTurns);
}

void CTaskVehicleChase::ProcessSituation()
{
	//Grab the vehicle.
	const CVehicle* pVehicle = GetPed()->GetVehiclePedInside();

	//Grab the vehicle position.
	Vec3V vVehiclePosition = pVehicle->GetTransform().GetPosition();

	//Grab the target.
	const CPhysical* pTarget = GetDominantTarget();

	//Grab the target position.
	Vec3V vTargetPosition = pTarget->GetTransform().GetPosition();

	//Generate a vector from the vehicle to the target.
	Vec3V vVehicleToTarget = Subtract(vTargetPosition, vVehiclePosition);
	vVehicleToTarget.SetZ(ScalarV(V_ZERO));

	//Calculate the distance.
	m_Situation.m_fDistSq = MagSquared(vVehicleToTarget).Getf();

	//Grab the target velocity.
	Vec3V vTargetVelocity = VECTOR3_TO_VEC3V(pTarget->GetVelocity());
	vTargetVelocity.SetZ(ScalarV(V_ZERO));

	//Grab the target forward.
	Vec3V vTargetForward = pTarget->GetTransform().GetForward();
	vTargetForward.SetZ(ScalarV(V_ZERO));
	vTargetForward = NormalizeFastSafe(vTargetForward, Vec3V(V_ZERO));

	//Calculate the target direction.
	static dev_float s_fMinTargetVelocityToUseIt = 1.0f;
	Vec3V vMinTargetVelocityToUseItSq = Vec3VFromF32(square(s_fMinTargetVelocityToUseIt));
	Vec3V vTargetDirection = NormalizeFastSafe(vTargetVelocity, vTargetForward, vMinTargetVelocityToUseItSq);

	//Calculate the direction from the vehicle to the target.
	Vec3V vVehicleToTargetDirection = NormalizeFastSafe(vVehicleToTarget, Vec3V(V_ZERO));

	//Calculate the dot.
	m_Situation.m_fDot = Dot(vTargetDirection, vVehicleToTargetDirection).Getf();
	m_Situation.m_fDot = Clamp(m_Situation.m_fDot, -1.0f, 1.0f);
	
	//Calculate the target speed.
	m_Situation.m_fTargetSpeed = VEC3V_TO_VECTOR3(vTargetVelocity).XYMag();
	
	//Calculate the speed.
	float fSpeed = pVehicle->GetVelocity().XYMag();
	
	//Calculate the max speed.
	float fMaxSpeed = pVehicle->m_Transmission.GetDriveMaxVelocity();
	
	//Calculate the speed leeway.
	m_Situation.m_fSpeedLeeway = Max(0.0f, fMaxSpeed - fSpeed);
	
	//Calculate the steer angle.
	//Use the absolute value, at this point we don't care which direction they are turning, just that they are.
	//This only applies if the target is in a vehicle.
	const CVehicle* pTargetVehicle = GetTargetVehicle();
	m_Situation.m_fTargetSteerAngle = pTargetVehicle ? Abs(pTargetVehicle->GetSteerAngle()) : 0.0f;

	//Grab the forward.
	Vec3V vForward = pVehicle->GetTransform().GetForward();
	vForward.SetZ(ScalarV(V_ZERO));
	vForward = NormalizeFastSafe(vForward, Vec3V(V_ZERO));

	//Calculate the dot.
	m_Situation.m_fFacingDot = Dot(vForward, vVehicleToTargetDirection).Getf();
	m_Situation.m_fFacingDot = Clamp(m_Situation.m_fFacingDot, -1.0f, 1.0f);

	// Various situation info
	m_Situation.m_fSpeed = fSpeed;
	m_Situation.m_fSmoothedSpeedSq = pVehicle->GetIntelligence()->GetSmoothedSpeedSq();
	m_Situation.m_fTargetSmoothedSpeedSq = pTargetVehicle ? pTargetVehicle->GetIntelligence()->GetSmoothedSpeedSq() : 0.0f;
}

// Thrown events in the world for peds to react against.
void CTaskVehicleChase::ProcessShockingEvents()
{
	// Decide if the chase target is important enough to throw shocking events.
	bool bCanThrowShockingEvent = false;
	const CEntity* pTarget = m_Target.GetEntity();
	if (pTarget)
	{
		if (pTarget->GetIsTypePed())
		{
			const CPed* pPed = static_cast<const CPed*>(pTarget);
			if (pPed->IsAPlayerPed())
			{
				bCanThrowShockingEvent = true;
			}
		}
		else if (pTarget->GetIsTypeVehicle())
		{
			const CVehicle* pVehicle = static_cast<const CVehicle*>(pTarget);
			if (pVehicle->ContainsPlayer())
			{
				bCanThrowShockingEvent = true;
			}
		}
	}

	if (!bCanThrowShockingEvent)
	{
		return;
	}

	// Increment the counter.
	m_fCarChaseShockingEventCounter += GetTimeStep();

	// Check if its been a while since firing a shocking event.
	if (m_fCarChaseShockingEventCounter < sm_Tunables.m_TimeBetweenCarChaseShockingEvents)
	{
		return;
	}

	CVehicle* pVehicle = GetPed()->GetVehiclePedInside();

	// Check if the pursuing vehicle has their siren on.
	if (!pVehicle->m_nVehicleFlags.GetIsSirenOn())
	{
		return;
	}

	// Check if the distance to the target is small.
	Vec3V vTargetPosition = GetDominantTarget()->GetTransform().GetPosition();

	if (IsGreaterThanAll(DistSquared(vTargetPosition, pVehicle->GetTransform().GetPosition()), ScalarV(rage::square(sm_Tunables.m_DistanceForCarChaseShockingEvents))))
	{
		return;
	}

	// Add the event.
	CEventShockingCarChase chaseEvent(*pVehicle);
	CShockingEventsManager::Add(chaseEvent);

	// Reset the counter.
	m_fCarChaseShockingEventCounter = 0.0f;
}

void CTaskVehicleChase::ProcessStuck()
{
	//Change the flag.
	m_uRunningFlags.ChangeFlag(RF_IsStuck, IsStuck());
}

void CTaskVehicleChase::ProcessLowSpeedChase()
{
	m_uRunningFlags.ClearFlag(RF_IsLowSpeedChase);
	
	static dev_float sfMaxSpeedToConsiderLowSpeedChase = 12.0f;
	float MaxSpeedToConsiderLowSpeedChaseSq = sfMaxSpeedToConsiderLowSpeedChase * sfMaxSpeedToConsiderLowSpeedChase; 
	if ((m_Situation.m_fTargetSpeed < sfMaxSpeedToConsiderLowSpeedChase) && 
		(m_Situation.m_fTargetSmoothedSpeedSq < MaxSpeedToConsiderLowSpeedChaseSq))
	{
		if (!IsMakingAggressiveMove())
		{
			if ((m_Situation.m_fSpeed < sfMaxSpeedToConsiderLowSpeedChase) && 
				(m_Situation.m_fSmoothedSpeedSq < MaxSpeedToConsiderLowSpeedChaseSq))
			{
				m_uRunningFlags.SetFlag(RF_IsLowSpeedChase);
			}
		}
		else
		{
			m_uRunningFlags.SetFlag(RF_IsLowSpeedChase);
		}
	}
}

void CTaskVehicleChase::RemoveFromVehicleChaseDirector()
{
	//Find the vehicle chase director for the target vehicle.
	CVehicleChaseDirector* pVehicleChaseDirector = CVehicleChaseDirector::FindVehicleChaseDirector(*GetDominantTarget());
	if(!pVehicleChaseDirector)
	{
		return;
	}
	
	//Remove from the vehicle chase director.
	pVehicleChaseDirector->Remove(this);
}

bool CTaskVehicleChase::ShouldMakeAggressiveMove() const
{
	//Ensure we can make an aggressive move.
	if(!CanMakeAggressiveMove())
	{
		return false;
	}

	//Ensure the delay has expired.
	if(m_fAggressiveMoveDelay > 0.0f)
	{
		return false;
	}

	return true;
}

bool CTaskVehicleChase::ShouldUseCheatFlags() const
{
	//Ensure the vehicle is valid.
	const CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
	if(!pVehicle)
	{
		return false;
	}

	//Ensure we are not doing a handbrake turn.
	if(pVehicle->m_nVehicleFlags.bIsDoingHandBrakeTurn)
	{
		return false;
	}

	return true;
}

bool CTaskVehicleChase::ShouldUseCheatPowerIncrease() const
{
	//Ensure the vehicle is valid.
	const CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
	if(!pVehicle)
	{
		return false;
	}

	//Ensure the steer angle is valid.
	static dev_float s_fMaxSteerAngle = 0.08f;
	if(Abs(pVehicle->GetSteerAngle()) > s_fMaxSteerAngle)
	{
		return false;
	}

	//Check the state.
	switch(GetState())
	{
		case State_CloseDistance:
		case State_Block:
		case State_Pursue:
		case State_Ram:
		case State_SpinOut:
		case State_PullAlongside:
		{
			break;
		}
		default:
		{
			return false;
		}
	}

	//don't give cheat power to network dummies - it gives them far too much accel to be stable
	if(NetworkInterface::IsGameInProgress() )
	{
		if(!pVehicle->PopTypeIsMission() && (pVehicle->IsDummy() || GetState() == State_Pursue))
		{
			return false;
		}
	}

	return true;
}

bool CTaskVehicleChase::ShouldUseContinuousRam() const
{
	//Check if the flag is set.
	if(IsBehaviorFlagSet(BF_UseContinuousRam))
	{
		return true;
	}

	return false;
}

void CTaskVehicleChase::UpdatePursue()
{
	//Ensure the pursue task is valid.
	CTaskVehiclePursue* pTask = GetVehicleTaskForPursue();
	if(!pTask)
	{
		return;
	}

	//Set the cruise speed.
	float fCruiseSpeed = CalculateCruiseSpeed();
	pTask->SetCruiseSpeed(fCruiseSpeed);
}

bool CTaskVehicleChase::IsSituationValid(const IsSituationValidInput& rInput) const
{
	//Check the min distance.
	if(rInput.m_uFlags.IsFlagSet(IsSituationValidInput::CheckMinDistance))
	{
		float fMinDistSq = square(rInput.m_fMinDistance);
		if(m_Situation.m_fDistSq < fMinDistSq)
		{
			return false;
		}
	}
	
	//Check the max distance.
	if(rInput.m_uFlags.IsFlagSet(IsSituationValidInput::CheckMaxDistance))
	{
		float fMaxDistSq = square(rInput.m_fMaxDistance);
		if(m_Situation.m_fDistSq > fMaxDistSq)
		{
			return false;
		}
	}
	
	//Check the min dot.
	if(rInput.m_uFlags.IsFlagSet(IsSituationValidInput::CheckMinDot))
	{
		float fMinDot = rInput.m_fMinDot;
		if(m_Situation.m_fDot < fMinDot)
		{
			return false;
		}
	}
	
	//Check the max dot.
	if(rInput.m_uFlags.IsFlagSet(IsSituationValidInput::CheckMaxDot))
	{
		float fMaxDot = rInput.m_fMaxDot;
		if(m_Situation.m_fDot > fMaxDot)
		{
			return false;
		}
	}
	
	//Check the min time in state.
	if(rInput.m_uFlags.IsFlagSet(IsSituationValidInput::CheckMinTimeInState))
	{
		float fTimeInState = GetTimeInState();
		float fMinTimeInState = rInput.m_fMinTimeInState;
		if(fTimeInState < fMinTimeInState)
		{
			return false;
		}
	}
	
	//Check the max time in state.
	if(rInput.m_uFlags.IsFlagSet(IsSituationValidInput::CheckMaxTimeInState))
	{
		float fTimeInState = GetTimeInState();
		float fMaxTimeInState = rInput.m_fMaxTimeInState;
		if(fTimeInState > fMaxTimeInState)
		{
			return false;
		}
	}
	
	//Check the min target speed.
	if(rInput.m_uFlags.IsFlagSet(IsSituationValidInput::CheckMinTargetSpeed))
	{
		float fMinTargetSpeed = rInput.m_fMinTargetSpeed;
		if(m_Situation.m_fTargetSpeed < fMinTargetSpeed)
		{
			return false;
		}
	}
	
	//Check the max target speed.
	if(rInput.m_uFlags.IsFlagSet(IsSituationValidInput::CheckMaxTargetSpeed))
	{
		float fMaxTargetSpeed = rInput.m_fMaxTargetSpeed;
		if(m_Situation.m_fTargetSpeed > fMaxTargetSpeed)
		{
			return false;
		}
	}
	
	//Check the min speed leeway.
	if(rInput.m_uFlags.IsFlagSet(IsSituationValidInput::CheckMinSpeedLeeway))
	{
		float fMinSpeedLeeway = rInput.m_fMinSpeedLeeway;
		if(m_Situation.m_fSpeedLeeway < fMinSpeedLeeway)
		{
			return false;
		}
	}
	
	//Check the max target steer angle.
	if(rInput.m_uFlags.IsFlagSet(IsSituationValidInput::CheckMaxTargetSteerAngle))
	{
		float fMaxTargetSteerAngle = rInput.m_fMaxTargetSteerAngle;
		if(m_Situation.m_fTargetSteerAngle > fMaxTargetSteerAngle)
		{
			return false;
		}
	}
	
	return true;
}

float CTaskVehicleChase::CalculateCruiseSpeed(const CPed& rPed, const CVehicle& rVehicle)
{
	//Calculate the cruise speed.
	float fCruiseSpeed = rVehicle.m_Transmission.GetDriveMaxVelocity();
	fCruiseSpeed *= rPed.GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatAutomobileSpeedModifier);

	//limit top speed in network games as much more likely to be dummy chasing remote player
	if(NetworkInterface::IsGameInProgress() && rVehicle.GetIsLandVehicle() && !rVehicle.PopTypeIsMission())
	{
		fCruiseSpeed = rage::Min(35.0f, fCruiseSpeed);
	}

	return fCruiseSpeed;
}

void CTaskVehicleChase::LoadVehicleMissionParameters(const CPed& rPed, const CVehicle& rVehicle, const CEntity& rTarget, sVehicleMissionParams& rParams)
{
	//Set the vehicle mission parameters.
	rParams.SetTargetEntity(const_cast<CEntity *>(&rTarget));
	rParams.m_fTargetArriveDist = 0.0f;
	rParams.m_iDrivingFlags = DMode_AvoidCars|DF_DriveIntoOncomingTraffic|DF_DontTerminateTaskWhenAchieved|
		DF_SteerAroundPeds|DF_UseStringPullingAtJunctions|DF_PreventJoinInRoadDirectionWhenMoving|DF_UseSwitchedOffNodes;

	if (rPed.GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_PreferNavmeshDuringVehicleChase))
	{
		rParams.m_iDrivingFlags.SetFlag(DF_PreferNavmeshRoute);
	}
	if (rPed.GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_AllowedToAvoidOffroadDuringVehicleChase))
	{
		rParams.m_iDrivingFlags.SetFlag(DF_GoOffRoadWhenAvoiding);
	}

	rParams.m_fCruiseSpeed = CalculateCruiseSpeed(rPed, rVehicle);
}

bool CTaskVehicleChase::CanPullAlongsideInfront() const
{
	//Ensure the behavior flag is not set.
	if(IsBehaviorFlagSet(BF_CantPullAlongsideInfront))
	{
		return false;
	}

	//only allow bikes to do this for now - cars will block
	if(!GetPed()->GetVehiclePedInside() || GetPed()->GetVehiclePedInside()->GetVehicleType() != VEHICLE_TYPE_BIKE )
	{
		return false;
	}

	//Ensure the cruise while ahead is set
	if(!GetPed()->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CruiseAndBlockInVehicle))
	{
		return false;
	}

	//Ensure the flag is not set.
	if(GetPed()->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_DisablePullAlongsideDuringVehicleChase))
	{
		return false;
	}

	//Check if we can do drivebys.
	if(CanDoDrivebys())
	{
		return true;
	}
	//Check if anyone in our vehicle can do a driveby.
	else if(GetPed()->GetIsInVehicle() && CanAnyoneInVehicleDoDrivebys(*GetPed()->GetVehiclePedInside()))
	{
		return true;
	}

	return false;
}

s32 CTaskVehicleChase::ChooseAppropriateStateForPullAlongsideInfront()
{
	//Check if pull alongside is no longer appropriate.
	if(IsPullAlongsideInfrontNoLongerAppropriate())
	{
		//disable this so we don't try and re-enter
		ChangeBehaviorFlag(Overrides::Code,BF_CantPullAlongsideInfront, true);
		return State_Analyze;
	}
	else
	{
		return State_Invalid;
	}
}

void CTaskVehicleChase::PullAlongsideInfront_OnEnter()
{
	//Load the vehicle mission parameters.
	sVehicleMissionParams params;
	LoadVehicleMissionParameters(params);

	//Create the vehicle task.
	CTask* pVehicleTask = rage_new CTaskVehiclePullAlongside(params, sm_Tunables.m_PullAlongside.m_StraightLineDistance, true);

	//Create the sub-task.
	CAITarget target(m_Target);
	CTask* pSubTask = rage_new CTaskVehicleCombat(&target);

	//Create the task.
	CTask* pTask = rage_new CTaskControlVehicle(GetPed()->GetVehiclePedInside(), pVehicleTask, pSubTask);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskVehicleChase::PullAlongsideInfront_OnUpdate()
{
	m_uRunningFlags.SetFlag(RF_DisableAggressiveMoves);

	//Check for aggressive move success.
	if(IsPullAlongsideSuccessful())
	{
		ChangeBehaviorFlag(Overrides::Code,BF_CantPullAlongsideInfront, true);
		SetState(State_Analyze);
		return FSM_Continue;
	}

	//Choose an appropriate state.
	s32 iState = ChooseAppropriateStateForPullAlongsideInfront();
	if(iState != State_Invalid)
	{
		//Move to the state.
		SetState(iState);
	}
	//Check if the sub-task has finished.
	else if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Analyze the state.
		SetState(State_Analyze);
	}

	return FSM_Continue;
}

void CTaskVehicleChase::PullAlongsideInfront_OnExit()
{
	m_uRunningFlags.ClearFlag(RF_DisableAggressiveMoves);
}

bool CTaskVehicleChase::IsPullAlongsideInfrontAppropriate() const
{
	//Ensure we can spin out.
	if(!CanPullAlongsideInfront())
	{
		return false;
	}

	//Ensure we are not stuck.
	if(m_uRunningFlags.IsFlagSet(RF_IsStuck))
	{
		return false;
	}

	//Check the state.
	switch(GetState())
	{
	case State_Analyze:
		{
			return IsPullAlongsideInfrontAppropriateFromAnalyze();
		}
	default:
		{
			return false;
		}
	}
}

bool CTaskVehicleChase::IsPullAlongsideInfrontNoLongerAppropriate() const
{
	//Ensure we can spin out.
	if(!CanPullAlongsideInfront())
	{
		return true;
	}

	//Ensure we are not stuck.
	if(m_uRunningFlags.IsFlagSet(RF_IsStuck))
	{
		return true;
	}

	//Check the previous state.
	switch(GetPreviousState())
	{
	case State_Analyze:
		{
			return IsPullAlongsideInfrontNoLongerAppropriateFromAnalyze();
		}
	default:
		{
			return true;
		}
	}
}

bool CTaskVehicleChase::IsPullAlongsideInfrontAppropriateFromAnalyze() const
{
	//Success
	//1)Close enough
	static dev_float s_fMaxDistance = 30.0f;
	if(m_Situation.m_fDistSq > s_fMaxDistance*s_fMaxDistance)
	{
		return false;
	}

	//2)We're moving fast enough
	static dev_float s_fMinSpeed = 10.0f;
	if(GetPed()->GetVehiclePedInside()->GetVelocity().Mag2() < s_fMinSpeed*s_fMinSpeed)
	{
		return false;
	}

	//3)Target moving fast enough
	if(GetTargetVehicle()->GetVelocity().Mag2() < s_fMinSpeed*s_fMinSpeed)
	{
		return false;
	}

	//4)We're ahead of target car
	Vec3V vThemToUs = Normalize(GetPed()->GetVehiclePedInside()->GetTransform().GetPosition() - GetTargetVehicle()->GetTransform().GetPosition());
	float fPositionDot = rage::Dot(vThemToUs,GetTargetVehicle()->GetTransform().GetForward()).Getf();
	static dev_float s_fMinFacingDot = 0.707f;
	if(fPositionDot < s_fMinFacingDot)
	{
		return false;
	}

	//5)Forward vector dot > 45 degrees
	float fForwardDot = rage::Dot(GetPed()->GetVehiclePedInside()->GetTransform().GetForward(),GetTargetVehicle()->GetTransform().GetForward()).Getf();
	if(fForwardDot < s_fMinFacingDot)
	{
		return false;
	}

	return true;
}

bool CTaskVehicleChase::IsPullAlongsideInfrontNoLongerAppropriateFromAnalyze() const
{
	//Fail
	//1)Close enough
	static dev_float s_fMaxDistance = 32.0f;
	if(m_Situation.m_fDistSq > s_fMaxDistance*s_fMaxDistance)
	{
		return true;
	}

	//2)We're moving fast enough
	static dev_float s_fMinSpeed = 5.0f;
	const CVehicle* pVehicle = GetPed()->GetVehiclePedInside();

	if(pVehicle->GetVelocity().Mag2() < s_fMinSpeed*s_fMinSpeed)
	{
		return true;
	}

	//3)Target moving fast enough
	if(GetTargetVehicle()->GetVelocity().Mag2() < s_fMinSpeed*s_fMinSpeed)
	{
		return true;
	}

	//4)Target facing away from us
	//project our vehicle forward a bit
	Vec3V vOurForward = pVehicle->GetTransform().GetForward();
	Vec3V vOurProjectedPoint = Add(pVehicle->GetTransform().GetPosition(), Scale(vOurForward, ScalarV(5.0f)));
	Vec3V vToUs = vOurProjectedPoint - GetTargetVehicle()->GetTransform().GetPosition();
	vToUs.SetZ(0.0f);
	vToUs = Normalize(vToUs);

	if(rage::Dot(GetTargetVehicle()->GetTransform().GetForward(), vToUs).Getf() < 0.0f)
	{
		return true;
	}

	return false;
}

void CTaskVehicleChase::FallBack_OnEnter()
{
	//tell the vehicle to brake briefly
	CTask* pVehicleTask = rage_new CTaskVehicleBrake(NetworkInterface::GetSyncedTimeInMilliseconds() + 500);
	CAITarget target(m_Target);
	CTask* pSubTask = rage_new CTaskVehicleCombat(&target);
	CTask* pTask = rage_new CTaskControlVehicle(GetPed()->GetVehiclePedInside(), pVehicleTask, pSubTask);
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskVehicleChase::FallBack_OnUpdate()
{
	CVehicle* pVehicle = GetPed()->GetVehiclePedInside();

	if(pVehicle)
	{
		const CVehicle* pTargetVehicle = GetTargetVehicle();
		if (pTargetVehicle)
		{
			Vec3V vTargetVehiclePositionLocalToVehicle = pVehicle->GetTransform().UnTransform(pTargetVehicle->GetTransform().GetPosition());

			float fOverlap = vTargetVehiclePositionLocalToVehicle.GetYf();

			spdAABB boundingBox;
			const spdAABB& targetBounds = pTargetVehicle->GetLocalSpaceBoundBox(boundingBox);
			const spdAABB& ourBounds = pVehicle->GetLocalSpaceBoundBox(boundingBox);

			float requiredOverlap = targetBounds.GetMax().GetYf() + ourBounds.GetMax().GetYf() + 0.25f;

			CTaskVehicleMissionBase* pVehicleTask = pVehicle->GetIntelligence()->GetActiveTask();

			//can exit if we're no longer alongside
			if((pVehicleTask && pVehicleTask->GetIsFlagSet(aiTaskFlags::HasFinished)) || fOverlap > requiredOverlap)
			{
				SetState(State_Analyze);
			}
		}
		else
		{
			SetState(State_Analyze);
		}
	}
	else
	{
		SetState(State_Analyze);
	}

	return FSM_Continue;
}

bool CTaskVehicleChase::ShouldFallBack() const
{
	CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
	if(pVehicle)
	{
		//if we're a bike and are now alongside we should enter fallback state instead of trying to be aggressive
		if( GetState() == State_PullAlongside && pVehicle->GetVehicleType() == VEHICLE_TYPE_BIKE )
		{
			const CVehicle* pTargetVehicle = GetTargetVehicle();
			if(pTargetVehicle)
			{
				//Transform the target ped's position into local space for the vehicle.
				Vec3V vTargetVehiclePositionLocalToVehicle = pVehicle->GetTransform().UnTransform(pTargetVehicle->GetTransform().GetPosition());

				float fOverlap = vTargetVehiclePositionLocalToVehicle.GetYf();

				spdAABB boundingBox;
				const spdAABB& targetBounds = pTargetVehicle->GetLocalSpaceBoundBox(boundingBox);
				const spdAABB& ourBounds = pVehicle->GetLocalSpaceBoundBox(boundingBox);

				float requiredOverlap = targetBounds.GetMax().GetYf() + ourBounds.GetMax().GetYf() + 0.25f;

				//need to fallback if we're alongside
				if(fOverlap < requiredOverlap)
				{
					return true;
				}	
			}
		}
	}

	return false;
}