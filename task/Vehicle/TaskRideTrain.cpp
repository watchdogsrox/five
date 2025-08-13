//
// Task/Vehicle/TaskRideTrain.cpp
//
// Copyright (C) 2012-2012 Rockstar Games.  All Rights Reserved.
//

// File header
#include "Task/Vehicle/TaskRideTrain.h"

// Rage headers

// Game headers
#include "Peds/ped.h"
#include "event/EventShocking.h"
#include "Task/General/TaskBasic.h"
#include "Task/Movement/TaskNavMesh.h"
#include "Task/Scenario/ScenarioFinder.h"
#include "Task/Scenario/Types/TaskUseScenario.h"
#include "Vehicles/train.h"

AI_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////
// CTaskRideTrain
////////////////////////////////////////////////////////////////////////////////

CTaskRideTrain::Tunables CTaskRideTrain::sm_Tunables;
IMPLEMENT_VEHICLE_TASK_TUNABLES(CTaskRideTrain, 0xe7e0bb53);

CTaskRideTrain::CTaskRideTrain(CTrain* pTrain)
: m_pTrain(pTrain)
, m_pScenarioTask(NULL)
, m_fDelayForGetOff(0.0f)
, m_uRunningFlags(0)
, m_bOriginalDontActivateRagdollFromExplosionsFlag(false)
{
	SetInternalTaskType(CTaskTypes::TASK_RIDE_TRAIN);
}

CTaskRideTrain::CTaskRideTrain(CTrain* pTrain, CTask* pScenarioTask)
: m_pTrain(pTrain)
, m_pScenarioTask(pScenarioTask)
, m_fDelayForGetOff(0.0f)
, m_uRunningFlags(0)
, m_bOriginalDontActivateRagdollFromExplosionsFlag(false)
{
	SetInternalTaskType(CTaskTypes::TASK_RIDE_TRAIN);
}

CTaskRideTrain::CTaskRideTrain(const CTaskRideTrain& rOther)
: m_pTrain(rOther.m_pTrain)
, m_pScenarioTask(rOther.m_pScenarioTask ? rOther.m_pScenarioTask->Copy() : NULL)
, m_fDelayForGetOff(rOther.m_fDelayForGetOff)
, m_uRunningFlags(rOther.m_uRunningFlags)
, m_bOriginalDontActivateRagdollFromExplosionsFlag(rOther.m_bOriginalDontActivateRagdollFromExplosionsFlag)
{
	SetInternalTaskType(CTaskTypes::TASK_RIDE_TRAIN);
}

CTaskRideTrain::~CTaskRideTrain()
{
	if(m_pScenarioTask)
	{
		delete m_pScenarioTask;
	}
}


CScenarioPoint* CTaskRideTrain::GetScenarioPoint() const
{
	// This is used by CPed::GetScenarioPoint(), important for preventing
	// too many peds using the same scenario near each other.

	CScenarioPoint* pt = NULL;

	if(m_pScenarioTask)
	{
		// Shouldn't be possible for this to be anything but a CTask.
		taskAssert(dynamic_cast<const CTask*>(m_pScenarioTask.Get()));

		pt = static_cast<const CTask*>(m_pScenarioTask.Get())->GetScenarioPoint();
	}

// May want to also do some of this, but not sure that it's necessary:
#if 0
	const CTask* pSubTask = GetSubTask();
	if(pSubTask)
	{
		pt = pSubTask->GetScenarioPoint();
	}
#endif

	return pt;
}


int CTaskRideTrain::GetScenarioPointType() const
{
	int type = -1;
	if(m_pScenarioTask)
	{
		// Shouldn't be possible for this to be anything but a CTask.
		taskAssert(dynamic_cast<const CTask*>(m_pScenarioTask.Get()));

		type = static_cast<const CTask*>(m_pScenarioTask.Get())->GetScenarioPointType();
	}
	return type;
}



void CTaskRideTrain::GetOff()
{
	//Set the running flag for getting off the train.
	m_uRunningFlags.SetFlag(RF_GetOff);
	
	//Set the delay.
	float fMinDelayForGetOff = sm_Tunables.m_MinDelayForGetOff;
	float fMaxDelayForGetOff = sm_Tunables.m_MaxDelayForGetOff;
	m_fDelayForGetOff = fwRandom::GetRandomNumberInRange(fMinDelayForGetOff, fMaxDelayForGetOff);
}

bool CTaskRideTrain::IsGettingOff() const
{
	//Ensure the flag is set.
	if(!m_uRunningFlags.IsFlagSet(RF_GetOff))
	{
		return false;
	}
	
	return true;
}

bool CTaskRideTrain::IsGettingOn() const
{
	//Check the state.
	switch(GetState())
	{
		case State_FindScenario:
		case State_GetOn:
		{
			return true;
		}
		default:
		{
			return false;
		}
	}
}

void CTaskRideTrain::CleanUp()
{
	//Ensure the ped is valid.
	CPed* pPed = GetPed();
	if(pPed)
	{
		//Note that the ped is no longer riding a train.
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_RidingTrain, false);
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromExplosions, m_bOriginalDontActivateRagdollFromExplosionsFlag);
		
		//Ensure the train is valid.
		if(m_pTrain)
		{
			//Remove the ped from the train.
			m_pTrain->RemovePassenger(pPed);
		}
	}
}

void CTaskRideTrain::DoAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
	//Check if we are using a scenario.
	if(GetState() == State_UseScenario)
	{
		//Ensure the sub-task is valid.
		CTask* pSubTask = GetSubTask();
		if(pSubTask)
		{
			//Ensure the sub-task is the correct type.
			if(pSubTask->GetTaskType() == CTaskTypes::TASK_USE_SCENARIO)
			{
				//Check if we do not have a scenario task.
				if(!m_pScenarioTask)
				{
					//Grab the scenario task.
					CTaskUseScenario* pTask = static_cast<CTaskUseScenario *>(pSubTask);
					
					//Create the scenario task.
					//This will allow this task to resume correctly.
					m_pScenarioTask = CreateScenarioTask(pTask->GetScenarioPoint(), pTask->GetScenarioType());
				}
			}
		}
	}
	
	//Call the base class version.
	CTask::DoAbort(iPriority, pEvent);
}

CTask::FSM_Return CTaskRideTrain::ProcessPreFSM()
{
	//Ensure the train is valid.
	if(!m_pTrain)
	{
		return FSM_Quit;
	}
	
	//Process the delay for get off.
	ProcessDelayForGetOff();

	//Process the LOD.
	ProcessLod();

	return FSM_Continue;
}

CTask::FSM_Return CTaskRideTrain::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();
				
		FSM_State(State_FindScenario)
			FSM_OnEnter
				FindScenario_OnEnter();
			FSM_OnUpdate
				return FindScenario_OnUpdate();
				
		FSM_State(State_GetOn)
			FSM_OnEnter
				GetOn_OnEnter();
			FSM_OnUpdate
				return GetOn_OnUpdate();
				
		FSM_State(State_UseScenario)
			FSM_OnEnter
				UseScenario_OnEnter();
			FSM_OnUpdate
				return UseScenario_OnUpdate();
				
		FSM_State(State_Wait)
			FSM_OnEnter
				Wait_OnEnter();
			FSM_OnUpdate
				return Wait_OnUpdate();
				
		FSM_State(State_GetOff)
			FSM_OnEnter
				GetOff_OnEnter();
			FSM_OnUpdate
				return GetOff_OnUpdate();
			FSM_OnExit
				GetOff_OnExit();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskRideTrain::Start_OnUpdate()
{
	CPed* pPed = GetPed();

	//Note that the ped is riding a train.
	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_RidingTrain, true);
	m_bOriginalDontActivateRagdollFromExplosionsFlag = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromExplosions);
	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollFromExplosions, true);

	//Add the ped to the train.
	if(!m_pTrain->AddPassenger(GetPed()))
	{
		//Finish the task.
		SetState(State_Finish);
	}
	//Check if we have a scenario task.
	else if(m_pScenarioTask)
	{
		//Use the scenario.
		SetState(State_UseScenario);
	}
	else
	{
		//Find a scenario.
		SetState(State_FindScenario);
	}
	
	return FSM_Continue;
}

void CTaskRideTrain::FindScenario_OnEnter()
{
}

CTask::FSM_Return CTaskRideTrain::FindScenario_OnUpdate()
{
	//Check if have not yet failed to get on in time.
	if(!FailedToGetOnInTime())
	{
		//Grab the train position.
		Vec3V vTrainPosition = m_pTrain->GetTransform().GetPosition();
		
		//Calculate the range.
		float fRange = m_pTrain->GetBaseModelInfo()->GetBoundingSphereRadius();
		
		//Create the options.
		CScenarioFinder::FindOptions opts;
		opts.m_pRequiredAttachEntity = m_pTrain;
		
		//Find a scenario attached to the train.
		int iRealScenarioPointType = -1;
		CScenarioPoint* pScenarioPoint = CScenarioFinder::FindNewScenario(GetPed(), VEC3V_TO_VECTOR3(vTrainPosition), fRange, opts, iRealScenarioPointType);
		
		//Create the scenario task.
		m_pScenarioTask = CreateScenarioTask(pScenarioPoint, iRealScenarioPointType);
		if(m_pScenarioTask)
		{
			//Get on the train.
			SetState(State_GetOn);
		}
		else
		{
			//Couldn't find a scenario.
			SetState(State_Finish);
		}
	}
	else
	{
		//Finish the task.
		SetState(State_Finish);
	}

	return FSM_Continue;
}

void CTaskRideTrain::GetOn_OnEnter()
{
	//Get on the train.
	SetNewTask(rage_new CTaskGetOnTrain(m_pTrain));
}

CTask::FSM_Return CTaskRideTrain::GetOn_OnUpdate()
{
	//Check if the scenario is invalid.
	if(!m_pScenarioTask)
	{
		//Finish the task.
		SetState(State_Finish);
	}
	//Check if the sub-task has finished.
	else if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Use the scenario.
		SetState(State_UseScenario);
	}
	//Check if we failed to get on in time.
	else if(FailedToGetOnInTime())
	{
		//Finish the task.
		SetState(State_Finish);
	}
	
	return FSM_Continue;
}

void CTaskRideTrain::UseScenario_OnEnter()
{
	//Assign the scenario task.
	taskAssertf(m_pScenarioTask, "Invalid scenario task.");
	aiTask* pSubTask = m_pScenarioTask;
	m_pScenarioTask = NULL;
	
	//Start the task.
	SetNewTask(pSubTask);
}

CTask::FSM_Return CTaskRideTrain::UseScenario_OnUpdate()
{
	//Check if we should wait.
	if(ShouldWait())
	{
		//Move to the wait state.
		SetState(State_Wait);
	}
	//Check if we should exit the scenario.
	else if(ShouldExitScenario())
	{
		//Exit the scenario.
		ExitScenario();
	}
	
	return FSM_Continue;
}

void CTaskRideTrain::Wait_OnEnter()
{
	//Do nothing.
	SetNewTask(rage_new CTaskDoNothing(-1));
}

CTask::FSM_Return CTaskRideTrain::Wait_OnUpdate()
{
	//Check if we should get off.
	if(ShouldGetOff())
	{
		//Move to the get off state.
		SetState(State_GetOff);
	}
	else if(GetTimeInState() > CTaskRideTrain::sm_Tunables.m_fMaxWaitSeconds && !IsPhysicallyOn())
	{
		// We expect the ped's ground physical isn't necessarily updated on the exact frame this state starts,
		//  which is why State_Wait exists in the first palce. But if for some reason 
		//  we're no longer on the train after a long time, quit [5/20/2013 mdawe]
		SetState(State_Finish);
		return FSM_Continue;
	}
	
	return FSM_Continue;
}

void CTaskRideTrain::GetOff_OnEnter()
{
	//Create the task.
	CTask* pTask = rage_new CTaskGetOffTrain(m_pTrain);
	
	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskRideTrain::GetOff_OnUpdate()
{
	//Check if the sub-task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Nothing left to do.
		SetState(State_Finish);
	}
	//Check if we failed to get off in time.
	else if(FailedToGetOffInTime())
	{
		//Move to the waiting state.
		SetState(State_Wait);
	}
	
	return FSM_Continue;
}

void CTaskRideTrain::GetOff_OnExit()
{
	//Clear the running flag.
	m_uRunningFlags.ClearFlag(RF_GetOff);
}

#if !__FINAL
void CTaskRideTrain::Debug() const
{
	CTask::Debug();
}

const char * CTaskRideTrain::GetStaticStateName( s32 iState )
{
	Assert(iState>=State_Start&&iState<=State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_FindScenario",
		"State_GetOn",
		"State_UseScenario",
		"State_Wait",
		"State_GetOff",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif // !__FINAL

CTask* CTaskRideTrain::CreateScenarioTask(CScenarioPoint* pScenarioPoint, int realScenarioType) const
{
	//Ensure the scenario point is valid.
	if(!pScenarioPoint)
	{
		return NULL;
	}

	return rage_new CTaskUseScenario(realScenarioType, pScenarioPoint, CTaskUseScenario::SF_CheckConditions);
}

void CTaskRideTrain::ExitScenario()
{
	//Ensure the sub-task is valid.
	CTask* pTask = GetSubTask();
	if(!pTask)
	{
		return;
	}

	//Disable stationary reactions.
	CTaskUseScenario::SetStationaryReactionsEnabledForPed(*GetPed(), false);
	
	//Abort the sub-task.
	pTask->MakeAbortable(ABORT_PRIORITY_URGENT, NULL);
}

bool CTaskRideTrain::FailedToGetOffInTime() const
{
	//Ensure we can't get off the train.
	if(m_pTrain->CanGetOff())
	{
		return false;
	}
	
	//Ensure we are physically on the train.
	if(!IsPhysicallyOn())
	{
		return false;
	}
	
	return true;
}

bool CTaskRideTrain::FailedToGetOnInTime() const
{
	//Ensure we can't get on the train.
	if(m_pTrain->CanGetOn())
	{
		return false;
	}
	
	//Ensure we are not physically on the train.
	if(IsPhysicallyOn())
	{
		return false;
	}
	
	return true;
}

bool CTaskRideTrain::IsPhysicallyOn() const
{
	//Grab the ped.
	const CPed* pPed = GetPed();
	
	//Ensure we are physically on the train.
	if(pPed->GetGroundPhysical() != m_pTrain)
	{
		return false;
	}
	
	return true;
}

void CTaskRideTrain::ProcessDelayForGetOff()
{
	//Ensure the flag is set.
	if(!m_uRunningFlags.IsFlagSet(RF_GetOff))
	{
		return;
	}
	
	//Decrease the delay.
	m_fDelayForGetOff -= fwTimer::GetTimeStep();
}

void CTaskRideTrain::ProcessLod()
{
	//Check if we can enable lod.
	if((GetState() == State_UseScenario) && !ShouldExitScenario())
	{
		GetPed()->GetPedAiLod().ClearBlockedLodFlag(CPedAILod::AL_LodPhysics);
		GetPed()->GetPedAiLod().ClearBlockedLodFlag(CPedAILod::AL_LodMotionTask);
	}
	else
	{
		GetPed()->GetPedAiLod().SetBlockedLodFlag(CPedAILod::AL_LodPhysics);
		GetPed()->GetPedAiLod().SetBlockedLodFlag(CPedAILod::AL_LodMotionTask);
	}
}

bool CTaskRideTrain::ShouldExitScenario() const
{
	//Ensure the flag is set.
	if(!m_uRunningFlags.IsFlagSet(RF_GetOff))
	{
		return false;
	}
	
	//Ensure the delay has expired.
	if(m_fDelayForGetOff > 0.0f)
	{
		return false;
	}
	
	return true;
}

bool CTaskRideTrain::ShouldGetOff() const
{
	//Ensure the flag is set.
	if(!m_uRunningFlags.IsFlagSet(RF_GetOff))
	{
		return false;
	}

	//Ensure we are on the train, physically.  This is required for path-finding.
	if(!IsPhysicallyOn())
	{
		return false;
	}
	
	return true;
}

bool CTaskRideTrain::ShouldWait() const
{
	//Check if the sub-task is invalid.
	if(!GetSubTask())
	{
		return true;
	}
	
	//Check if the sub-task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		return true;
	}
	
	return false;
}

bool CTaskRideTrain::ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
	//Check if the priority is not immediate.
	if(iPriority != ABORT_PRIORITY_IMMEDIATE)
	{
		//Lots of peds cowering due to the train running over rats; this is an attempt to alleviate that.

		//Check if the event is valid.
		if(pEvent)
		{
			//Check the event type.
			CEvent* pGameEvent = static_cast<CEvent *>(const_cast<aiEvent *>(pEvent));
			switch(pGameEvent->GetEventType())
			{
				case EVENT_SHOCKING_DEAD_BODY:
				{
					//Check if the source is an animal.
					if(pGameEvent->GetSourcePed() &&
						(pGameEvent->GetSourcePed()->GetPedType() == PEDTYPE_ANIMAL))
					{
						//Remove the event.
						pGameEvent->Tick();

						return false;
					}

					break;
				}
				case EVENT_SHOCKING_INJURED_PED:
				case EVENT_SHOCKING_PED_RUN_OVER:
				{
					//Check if the other entity is an animal.
					CEventShocking* pEventShocking = static_cast<CEventShocking *>(pGameEvent);
					if(pEventShocking->GetOtherEntity() &&
						pEventShocking->GetOtherEntity()->GetIsTypePed() &&
						(static_cast<CPed *>(pEventShocking->GetOtherEntity())->GetPedType() == PEDTYPE_ANIMAL))
					{
						//Remove the event.
						pGameEvent->Tick();

						return false;
					}

					break;
				}
				default:
				{
					break;
				}
			}
		}
	}

	return CTask::ShouldAbort(iPriority, pEvent);
}

////////////////////////////////////////////////////////////////////////////////

CTaskTrainBase::Tunables CTaskTrainBase::sm_Tunables;
IMPLEMENT_VEHICLE_TASK_TUNABLES(CTaskTrainBase, 0x67575365);

CTaskTrainBase::CTaskTrainBase(CTrain* pTrain)
: m_pTrain(pTrain)
, m_BoardingPoint()
{
}

CTaskTrainBase::CTaskTrainBase(const CTaskTrainBase& rOther)
: m_pTrain(rOther.m_pTrain)
, m_BoardingPoint(rOther.m_BoardingPoint)
{

}

CTaskTrainBase::~CTaskTrainBase()
{

}

void CTaskTrainBase::CleanUp()
{

}

CTask::FSM_Return CTaskTrainBase::ProcessPreFSM()
{
	//Ensure the train is valid.
	if(!m_pTrain)
	{
		return FSM_Quit;
	}

	return FSM_Continue;
}

CTask* CTaskTrainBase::CreateNavTask(Vec3V_In vPosition, float fHeading) const
{
	//Set up the nav mesh.
	CNavParams params;
	params.m_vTarget = VEC3V_TO_VECTOR3(vPosition);
	params.m_fMoveBlendRatio = MOVEBLENDRATIO_WALK;
	params.m_fTargetRadius = sm_Tunables.m_TargetRadius;
	params.m_fCompletionRadius = sm_Tunables.m_CompletionRadius;
	params.m_fSlowDownDistance = sm_Tunables.m_SlowDownDistance;

	//Create the move task.
	CTaskMoveFollowNavMesh* pTask = rage_new CTaskMoveFollowNavMesh(params);

	//Quit the move task if there is no route.
	pTask->SetQuitTaskIfRouteNotFound(true);
	
	//Set the target stop heading.
	pTask->SetTargetStopHeading(fHeading);

	//Stop exactly.
	pTask->SetStopExactly(true);
	
	return rage_new CTaskComplexControlMovement(pTask);
}

Mat34V CTaskTrainBase::GenerateMatrixForBoardingPoint() const
{
	//Create a local space matrix for the boarding point.
	Mat34V mLocalBoardingPoint;
	Mat33V mLocalBoardingPointRot;
	Mat33VFromZAxisAngle(mLocalBoardingPointRot, ScalarVFromF32(m_BoardingPoint.GetHeading()));
	mLocalBoardingPoint.Set3x3(mLocalBoardingPointRot);
	mLocalBoardingPoint.Setd(VECTOR3_TO_VEC3V(m_BoardingPoint.GetPosition()));

	//Transform the local boarding point matrix into world space.
	Mat34V mWorldBoardingPoint;
	Transform(mWorldBoardingPoint, m_pTrain->GetTransform().GetMatrix(), mLocalBoardingPoint);
	
	return mWorldBoardingPoint;
}

void CTaskTrainBase::GeneratePositionAndHeadingToEnter(Vec3V_InOut vPosition, float& fHeading) const
{
	//Generate the boarding point matrix.
	Mat34V mBoardingPoint = GenerateMatrixForBoardingPoint();
	
	//Translate the position forwards.
	Vec3V vBoardingPointPosition = mBoardingPoint.GetCol3();
	vPosition = Add(vBoardingPointPosition, mBoardingPoint.GetCol1());
	
	//Generate the heading from the position to the boarding point.
	fHeading = fwAngle::GetRadianAngleBetweenPoints(vBoardingPointPosition.GetXf(), vBoardingPointPosition.GetYf(), vPosition.GetXf(), vPosition.GetYf());
}

void CTaskTrainBase::GeneratePositionAndHeadingToExit(Vec3V_InOut vPosition, float& fHeading) const
{
	//Generate the boarding point matrix.
	Mat34V mBoardingPoint = GenerateMatrixForBoardingPoint();

	//Translate the position backwards.
	Vec3V vBoardingPointPosition = mBoardingPoint.GetCol3();
	vPosition = Subtract(vBoardingPointPosition, mBoardingPoint.GetCol1());

	//Generate the heading from the position to the boarding point.
	fHeading = fwAngle::GetRadianAngleBetweenPoints(vBoardingPointPosition.GetXf(), vBoardingPointPosition.GetYf(), vPosition.GetXf(), vPosition.GetYf());
}

Vec3V_Out CTaskTrainBase::GeneratePositionToEnter() const
{
	//Generate the position and heading to enter.
	Vec3V vPosition;
	float fHeading;
	GeneratePositionAndHeadingToEnter(vPosition, fHeading);
	
	return vPosition;
}

Vec3V_Out CTaskTrainBase::GeneratePositionToExit() const
{
	//Generate the position and heading to exit.
	Vec3V vPosition;
	float fHeading;
	GeneratePositionAndHeadingToExit(vPosition, fHeading);
	
	return vPosition;
}

bool CTaskTrainBase::LoadBoardingPoint()
{
	//Find the closest boarding point.
	return m_pTrain->FindClosestBoardingPoint(GetPed()->GetTransform().GetPosition(), m_BoardingPoint);
}

#if !__FINAL
void CTaskTrainBase::Debug() const
{
	CTask::Debug();
	
#if DEBUG_DRAW
	Mat34V mBoardingPoint = GenerateMatrixForBoardingPoint();
	grcDebugDraw::Axis(MAT34V_TO_MATRIX34(mBoardingPoint), 1.0f);
#endif
}
#endif // !__FINAL

////////////////////////////////////////////////////////////////////////////////

CTaskGetOnTrain::CTaskGetOnTrain(CTrain* pTrain)
: CTaskTrainBase(pTrain)
{
	SetInternalTaskType(CTaskTypes::TASK_GET_ON_TRAIN);
}

CTaskGetOnTrain::CTaskGetOnTrain(const CTaskGetOnTrain& rOther)
: CTaskTrainBase(rOther)
{
	SetInternalTaskType(CTaskTypes::TASK_GET_ON_TRAIN);
}

CTaskGetOnTrain::~CTaskGetOnTrain()
{

}

CTask::FSM_Return CTaskGetOnTrain::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_MoveToBoardingPoint)
			FSM_OnEnter
				MoveToBoardingPoint_OnEnter();
			FSM_OnUpdate
				return MoveToBoardingPoint_OnUpdate();

		FSM_State(State_GetOn)
			FSM_OnEnter
				GetOn_OnEnter();
			FSM_OnUpdate
				return GetOn_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskGetOnTrain::Start_OnUpdate()
{
	//Load the boarding point.
	if(!LoadBoardingPoint())
	{
		//Finish the task.
		SetState(State_Finish);
	}
	else
	{
		//Move to the boarding point.
		SetState(State_MoveToBoardingPoint);
	}

	return FSM_Continue;
}

void CTaskGetOnTrain::MoveToBoardingPoint_OnEnter()
{
	//Generate the position and heading to enter.
	Vec3V vPosition;
	float fHeading;
	GeneratePositionAndHeadingToEnter(vPosition, fHeading);
	
	//Create the task.
	CTask* pTask = CreateNavTask(vPosition, fHeading);
	
	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskGetOnTrain::MoveToBoardingPoint_OnUpdate()
{
	//Wait for the task to finish.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Get on the train.
		SetState(State_GetOn);
	}

	return FSM_Continue;
}

void CTaskGetOnTrain::GetOn_OnEnter()
{
	//Grab the ped.
	CPed* pPed = GetPed();

	//Note that the ped just got on a train.
	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_JustGotOnTrain, true);
	
	//Generate the position.
	Vec3V vPosition = GeneratePositionToExit();
	
	//Create the move task.
	CTask* pMoveTask = rage_new CTaskMoveGoToPoint(MOVEBLENDRATIO_WALK, VEC3V_TO_VECTOR3(vPosition), 0.25f);
	
	//Create the task.
	CTask* pTask = rage_new CTaskComplexControlMovement(pMoveTask);
	
	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskGetOnTrain::GetOn_OnUpdate()
{
	//Check if the sub-task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Finish the task.
		SetState(State_Finish);
	}

	return FSM_Continue;
}

#if !__FINAL
void CTaskGetOnTrain::Debug() const
{
	CTaskTrainBase::Debug();
}

const char * CTaskGetOnTrain::GetStaticStateName( s32 iState )
{
	Assert(iState>=State_Start&&iState<=State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_MoveToBoardingPoint",
		"State_GetOn",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif // !__FINAL

////////////////////////////////////////////////////////////////////////////////

CTaskGetOffTrain::CTaskGetOffTrain(CTrain* pTrain)
: CTaskTrainBase(pTrain)
{
	SetInternalTaskType(CTaskTypes::TASK_GET_OFF_TRAIN);
}

CTaskGetOffTrain::CTaskGetOffTrain(const CTaskGetOffTrain& rOther)
: CTaskTrainBase(rOther)
{
	SetInternalTaskType(CTaskTypes::TASK_GET_OFF_TRAIN);
}

CTaskGetOffTrain::~CTaskGetOffTrain()
{

}

CTask::FSM_Return CTaskGetOffTrain::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_MoveToBoardingPoint)
			FSM_OnEnter
				MoveToBoardingPoint_OnEnter();
			FSM_OnUpdate
				return MoveToBoardingPoint_OnUpdate();

		FSM_State(State_GetOff)
			FSM_OnEnter
				GetOff_OnEnter();
			FSM_OnUpdate
				return GetOff_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskGetOffTrain::Start_OnUpdate()
{
	//Load the boarding point.
	if(!LoadBoardingPoint())
	{
		//Finish the task.
		SetState(State_Finish);
	}
	else
	{
		//Move to the boarding point.
		SetState(State_MoveToBoardingPoint);
	}

	return FSM_Continue;
}

void CTaskGetOffTrain::MoveToBoardingPoint_OnEnter()
{
	//Generate the position and heading to exit.
	Vec3V vPosition;
	float fHeading;
	GeneratePositionAndHeadingToExit(vPosition, fHeading);
	
	//Create the task.
	CTask* pTask = CreateNavTask(vPosition, fHeading);
	
	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskGetOffTrain::MoveToBoardingPoint_OnUpdate()
{
	//Wait for the sub-task to finish.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Get off the train.
		SetState(State_GetOff);
	}
	
	return FSM_Continue;
}

void CTaskGetOffTrain::GetOff_OnEnter()
{
	//Grab the ped.
	CPed* pPed = GetPed();

	//Note that the ped just got off a train.
	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_JustGotOffTrain, true);
	
	//Generate the position.
	Vec3V vPosition = GeneratePositionToEnter();

	//Create the move task.
	CTask* pMoveTask = rage_new CTaskMoveGoToPoint(MOVEBLENDRATIO_WALK, VEC3V_TO_VECTOR3(vPosition), 0.25f);

	//Create the task.
	CTask* pTask = rage_new CTaskComplexControlMovement(pMoveTask);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskGetOffTrain::GetOff_OnUpdate()
{
	//Check if the sub-task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Finish the task.
		SetState(State_Finish);
	}

	return FSM_Continue;
}

#if !__FINAL
void CTaskGetOffTrain::Debug() const
{
	CTaskTrainBase::Debug();
}

const char * CTaskGetOffTrain::GetStaticStateName( s32 iState )
{
	Assert(iState>=State_Start&&iState<=State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_MoveToBoardingPoint",
		"State_GetOff",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif // !__FINAL

////////////////////////////////////////////////////////////////////////////////
