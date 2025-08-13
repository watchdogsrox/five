
//File header
#include "Task/Motion/Locomotion/TaskMotionParachuting.h"

//Game headers
#include "Peds/PedIntelligence.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/General/TaskBasic.h"
#include "Task/Movement/TaskFall.h"
#include "Task/Movement/TaskParachute.h"

AI_OPTIMISATIONS()

CTaskMotionParachuting::CTaskMotionParachuting()
: CTaskMotionBase()
, m_pTask(NULL)
{
	SetInternalTaskType(CTaskTypes::TASK_MOTION_PARACHUTING);
}

CTaskMotionParachuting::~CTaskMotionParachuting()
{
}

CTask* CTaskMotionParachuting::CreatePlayerControlTask() 
{
	// When skydiving into water we can end up getting in an infinite loop as the fall task quits out in processprefsm
	if (taskVerifyf(GetPed(), "NULL Ped pointer") && GetPed()->GetIsSwimming())
	{
		return rage_new CTaskPlayerOnFoot();
	}
	else
	{
		//This bit of code should match the task event response for the in-air event.
		return rage_new CTaskComplexControlMovement( rage_new CTaskMoveInAir(), rage_new CTaskFall(), CTaskComplexControlMovement::TerminateOnSubtask );
	}
}

CTask::FSM_Return CTaskMotionParachuting::ProcessPreFSM()
{
	//Process the task.
	ProcessTask();

	return FSM_Continue;
}

CTask::FSM_Return CTaskMotionParachuting::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_Idle)
			FSM_OnEnter
				Idle_OnEnter();
			FSM_OnUpdate
				return Idle_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;
			
	FSM_End
}

#if !__FINAL

const char * CTaskMotionParachuting::GetStaticStateName( s32 iState )
{
	Assert(iState >= State_Start && iState <= State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_Idle",
		"State_Finish"
	};

	return aStateNames[iState];
}

#endif // !__FINAL

CTask::FSM_Return CTaskMotionParachuting::Start_OnUpdate()
{
	//Move to the idle state.
	SetState(State_Idle);

	return FSM_Continue;
}

void CTaskMotionParachuting::Idle_OnEnter()
{
}

CTask::FSM_Return CTaskMotionParachuting::Idle_OnUpdate()
{
	return FSM_Continue; 
}

void CTaskMotionParachuting::ProcessTask()
{
	//Ensure the task is invalid.
	if(m_pTask)
	{
		return;
	}

	//Set the task.
	m_pTask = GetPed()->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PARACHUTE);
}

bool CTaskMotionParachuting::SupportsMotionState(CPedMotionStates::eMotionState state)
{
	return (state == CPedMotionStates::MotionState_Parachuting) ? true : false;
}

bool CTaskMotionParachuting::IsInMotionState(CPedMotionStates::eMotionState state) const
{
	return (state == CPedMotionStates::MotionState_Parachuting) ? true : false;
}

void CTaskMotionParachuting::GetMoveSpeeds(CMoveBlendRatioSpeeds& UNUSED_PARAM(speeds))
{
}

bool CTaskMotionParachuting::IsInMotion(const CPed* UNUSED_PARAM(pPed)) const
{
	return true;
}

void CTaskMotionParachuting::GetNMBlendOutClip(fwMvClipSetId& UNUSED_PARAM(outClipSet), fwMvClipId& UNUSED_PARAM(outClip))
{
}

Vec3V_Out CTaskMotionParachuting::CalcDesiredVelocity(Mat34V_ConstRef updatedPedMatrix, float fTimestep)
{
	//Grab the parachute task.
	CTaskParachute* pTask = static_cast<CTaskParachute *>(m_pTask.Get());
	if(pTask)
	{
		//Calculate the desired velocity.
		Vector3 vDesiredVelocity;
		if(pTask->CalcDesiredVelocity(RCC_MATRIX34(updatedPedMatrix), fTimestep, vDesiredVelocity))
		{
            CPed* pPed = GetPed();

            if(pPed)
            {
                NetworkInterface::OnDesiredVelocityCalculated(*pPed);
            }

			return VECTOR3_TO_VEC3V(vDesiredVelocity);
		}
	}

	//Call the base class version.
	return CTaskMotionBase::CalcDesiredVelocity(updatedPedMatrix, fTimestep);
}

void CTaskMotionParachuting::GetPitchConstraintLimits(float& fMinOut, float& fMaxOut)
{
	//Grab the parachute task.
	CTaskParachute* pTask = static_cast<CTaskParachute *>(m_pTask.Get());
	if(pTask)
	{
		pTask->GetPitchConstraintLimits(fMinOut, fMaxOut);
	}
	else
	{
		CTaskMotionBase::GetPitchConstraintLimits(fMinOut, fMaxOut);
	}
}

bool CTaskMotionParachuting::ShouldStickToFloor()
{
	//Grab the parachute task.
	CTaskParachute* pTask = static_cast<CTaskParachute *>(m_pTask.Get());
	if(pTask)
	{
		return pTask->ShouldStickToFloor();
	}
	else
	{
		return CTaskMotionBase::ShouldStickToFloor();
	}
}