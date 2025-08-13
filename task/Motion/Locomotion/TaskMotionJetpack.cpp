
//File header
#include "Task/Motion/Locomotion/TaskMotionJetpack.h"

//Game headers
#include "Peds/PedIntelligence.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/General/TaskBasic.h"
#include "Task/Movement/TaskFall.h"
#include "Task/Movement/TaskJetpack.h"

#if ENABLE_JETPACK

AI_OPTIMISATIONS()

CTaskMotionJetpack::CTaskMotionJetpack()
: CTaskMotionBase()
, m_pTask(NULL)
{
	SetInternalTaskType(CTaskTypes::TASK_MOTION_JETPACK);
}

CTaskMotionJetpack::~CTaskMotionJetpack()
{
}

CTask* CTaskMotionJetpack::CreatePlayerControlTask() 
{
	// When skydiving into water we can end up getting in an infinite loop as the fall task quits out in processprefsm
	if (taskVerifyf(GetPed(), "NULL Ped pointer") && GetPed()->GetIsSwimming())
	{
		return rage_new CTaskPlayerOnFoot();
	}
	else
	{
		//This bit of code should match the task event response for the in-air event.
		return rage_new CTaskComplexControlMovement( rage_new CTaskMovePlayer(), rage_new CTaskJetpack(), CTaskComplexControlMovement::TerminateOnSubtask );
	}
}

CTask::FSM_Return CTaskMotionJetpack::ProcessPreFSM()
{
	//Process the task.
	ProcessTask();

	return FSM_Continue;
}

CTask::FSM_Return CTaskMotionJetpack::UpdateFSM(const s32 iState, const FSM_Event iEvent)
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

const char * CTaskMotionJetpack::GetStaticStateName( s32 iState )
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

CTask::FSM_Return CTaskMotionJetpack::Start_OnUpdate()
{
	//Move to the idle state.
	SetState(State_Idle);

	return FSM_Continue;
}

void CTaskMotionJetpack::Idle_OnEnter()
{
}

CTask::FSM_Return CTaskMotionJetpack::Idle_OnUpdate()
{
	return FSM_Continue; 
}

void CTaskMotionJetpack::ProcessTask()
{
	//Ensure the task is invalid.
	if(m_pTask)
	{
		return;
	}

	//Set the task.
	m_pTask = GetPed()->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_JETPACK);
}

bool CTaskMotionJetpack::SupportsMotionState(CPedMotionStates::eMotionState state)
{
	return (state == CPedMotionStates::MotionState_Jetpack) ? true : false;
}

bool CTaskMotionJetpack::IsInMotionState(CPedMotionStates::eMotionState state) const
{
	return (state == CPedMotionStates::MotionState_Jetpack) ? true : false;
}

void CTaskMotionJetpack::GetMoveSpeeds(CMoveBlendRatioSpeeds& UNUSED_PARAM(speeds))
{
}

bool CTaskMotionJetpack::IsInMotion(const CPed* UNUSED_PARAM(pPed)) const
{
	CTaskJetpack* pTask = static_cast<CTaskJetpack *>(m_pTask.Get());
	if(pTask)
	{
		return pTask->IsInMotion();
	}
	return true;
}

void CTaskMotionJetpack::GetNMBlendOutClip(fwMvClipSetId& UNUSED_PARAM(outClipSet), fwMvClipId& UNUSED_PARAM(outClip))
{
}

Vec3V_Out CTaskMotionJetpack::CalcDesiredVelocity(Mat34V_ConstRef updatedPedMatrix, float fTimestep)
{
	//Grab the jetack task.
	CTaskJetpack* pTask = static_cast<CTaskJetpack *>(m_pTask.Get());
	if(pTask)
	{
		//Calculate the desired velocity.
		Vector3 vDesiredVelocity;
		if(pTask->CalcDesiredVelocity(RCC_MATRIX34(updatedPedMatrix), fTimestep, vDesiredVelocity))
		{
			return VECTOR3_TO_VEC3V(vDesiredVelocity);
		}
	}

	//Call the base class version.
	return CTaskMotionBase::CalcDesiredVelocity(updatedPedMatrix, fTimestep);
}

void CTaskMotionJetpack::GetPitchConstraintLimits(float& fMinOut, float& fMaxOut)
{
	//Grab the jetpack task.
	CTaskJetpack* pTask = static_cast<CTaskJetpack *>(m_pTask.Get());
	if(pTask)
	{
		pTask->GetPitchConstraintLimits(fMinOut, fMaxOut);
	}
	else
	{
		CTaskMotionBase::GetPitchConstraintLimits(fMinOut, fMaxOut);
	}
}

bool CTaskMotionJetpack::ShouldStickToFloor()
{
	//Grab the jetpack task.
	CTaskJetpack* pTask = static_cast<CTaskJetpack *>(m_pTask.Get());
	if(pTask)
	{
		return pTask->ShouldStickToFloor();
	}
	else
	{
		return CTaskMotionBase::ShouldStickToFloor();
	}
}

#endif //# ENABLE_JETPACK