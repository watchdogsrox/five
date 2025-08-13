#include "TaskMotionDrunk.h"

#if ENABLE_DRUNK

#include "Task/Default/TaskPlayer.h"
#include "Task/Physics/TaskNM.h"
#include "Task/Physics/TaskNMDrunk.h"
#include "Task/Physics/TaskBlendFromNM.h"

//////////////////////////////////////////////////////////////////////////

CTaskMotionDrunk::CTaskMotionDrunk()
{
	SetInternalTaskType(CTaskTypes::TASK_MOTION_DRUNK);
}

//////////////////////////////////////////////////////////////////////////

CTaskMotionDrunk::~CTaskMotionDrunk()
{
}

//////////////////////////////////////////////////////////////////////////

void CTaskMotionDrunk::GetOverriddenGameplayCameraSettings(tGameplayCameraSettings& UNUSED_PARAM(settings)) const
{
	// TODO
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskMotionDrunk::ProcessPreFSM()
{
	
	m_GetupReqHelper.Request(NMBS_DRUNK_GETUPS);

	if (GetState()==State_NMBehaviour && GetPed())
	{
		// Update the current heading variable. Need to do this as
		// no-one is calling set heading whilst the ped is in ragdoll
		const Matrix34& pedMat = RCC_MATRIX34(GetPed()->GetMatrixRef());
		GetMotionData().SetCurrentHeading(pedMat.GetEulersFast().z);
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskMotionDrunk::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		// Entry point
		FSM_State(State_Start)
		FSM_OnUpdate
		return Start_OnUpdate();

	FSM_State(State_NMBehaviour)
		FSM_OnEnter
		NmBehaviour_OnEnter();
	FSM_OnUpdate
		return NmBehaviour_OnUpdate();

	FSM_State(State_Exit)
		FSM_OnUpdate
		return FSM_Quit;
	FSM_End
}

//////////////////////////////////////////////////////////////////////////

void	CTaskMotionDrunk::CleanUp()
{
	
}

//////////////////////////////////////////////////////////////////////////
//	FSM state functions
//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskMotionDrunk::Start_OnUpdate()
{
	SetState(State_NMBehaviour);
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

void CTaskMotionDrunk::NmBehaviour_OnEnter()
{
	CPed* pPed = GetPed();
	if (pPed && CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_DRUNK))
	{
		//start an nm control task with the drunk behaviour running
		CTask* pTaskChild = rage_new CTaskNMDrunk(100, 65535);
		CTask* pTaskNM = rage_new CTaskNMControl(100, 65535, pTaskChild, CTaskNMControl::DO_BLEND_FROM_NM | CTaskNMControl::ON_MOTION_TASK_TREE);
		SetNewTask(pTaskNM);
	}
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskMotionDrunk::NmBehaviour_OnUpdate()
{
	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished) || !GetSubTask())
	{
		//abort because the subtask has ended
		SetState(State_Exit);
	}
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////
// Motion task functions
//////////////////////////////////////////////////////////////////////////

CTask*  CTaskMotionDrunk::CreatePlayerControlTask() 
{
	CTask* pPlayerTask = NULL;
	pPlayerTask = rage_new CTaskPlayerOnFoot();
	return pPlayerTask;
}


void CTaskMotionDrunk::GetMoveSpeeds(CMoveBlendRatioSpeeds &speeds)
{
	speeds.Zero();
}

//////////////////////////////////////////////////////////////////////////

bool CTaskMotionDrunk::IsInMotion(const CPed * UNUSED_PARAM(pPed)) const
{
	return true;
}

#if !__FINAL

const char * CTaskMotionDrunk::GetStaticStateName( s32 iState )
{
	Assert(iState>=State_Start&&iState<=State_Exit);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_NmBehaviour",
		"State_Exit"
	};

	return aStateNames[iState];
}
#endif // !__FINAL

#endif // ENABLE_DRUNK