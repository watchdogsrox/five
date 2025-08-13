#include "Task/Movement/TaskGenericMoveToPoint.h"


#include "Task/Motion/TaskMotionBase.h"
#include "Task/Movement/TaskGoToPoint.h"
#include "Task/Movement/TaskNavMesh.h"
#include "Task/Movement/TaskFlyToPoint.h"
#include "Peds/NavCapabilities.h"
#include "Peds/PedIntelligence.h"

AI_OPTIMISATIONS()


const float CTaskGenericMoveToPoint::ms_fTargetRadius=0.5f;

CTaskGenericMoveToPoint::CTaskGenericMoveToPoint(const float fMoveBlend, const Vector3& vTarget, const float fTargetRadius) :
	CTaskMove			(fMoveBlend)
,	m_vTarget			(vTarget)
,	m_fTargetRadius		(fTargetRadius)
{
	SetState(State_Moving);
	SetInternalTaskType(CTaskTypes::TASK_GENERIC_MOVE_TO_POINT);
}

#if !__FINAL
	void CTaskGenericMoveToPoint::Debug() const
	{
#if DEBUG_DRAW
		//draw a line from the ped to the target point
		const Vector3 v1= VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition());
		const Vector3& v2=m_vTarget;
		grcDebugDraw::Line(v1,v2,Color_BlueViolet);
		//draw a sphere around target using target radius
		grcDebugDraw::Sphere(v2,m_fTargetRadius,Color_blue,false);
#endif
	}
#endif


CTaskGenericMoveToPoint::~CTaskGenericMoveToPoint()
{

}

CTask::FSM_Return CTaskGenericMoveToPoint::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		FSM_State(State_Moving)
			FSM_OnEnter
				return StateMoving_OnEnter();
			FSM_OnUpdate
				return StateMoving_OnUpdate();
	FSM_End
}

CTask::FSM_Return CTaskGenericMoveToPoint::StateMoving_OnEnter()
{
	//check the preferences of the ped here to determine the movement type
	CPedNavCapabilityInfo nav = GetPed()->GetPedIntelligence()->GetNavCapabilities();
	CTaskMove* tSubtask;
	//check if swimming
	//if not swimming, then check if can fly
	//otherwise follow navmesh
	
	CTaskMotionBase* motionTask = GetPed()->GetPrimaryMotionTask();
	if (motionTask && motionTask->IsSwimming()) {
		//sea
		tSubtask = rage_new CTaskMoveGoToPoint(GetMoveBlendRatio(), m_vTarget, m_fTargetRadius);
	} else if (nav.IsFlagSet(CPedNavCapabilityInfo::FLAG_MAY_FLY)) {
		//air
		tSubtask = rage_new CTaskFlyToPoint(GetMoveBlendRatio(), m_vTarget, m_fTargetRadius);
	} else {
		//land
		tSubtask = rage_new CTaskMoveFollowNavMesh(GetMoveBlendRatio(), m_vTarget, m_fTargetRadius);
		
	}
	SetNewTask(tSubtask);
	return FSM_Continue;
}

CTask::FSM_Return CTaskGenericMoveToPoint::StateMoving_OnUpdate()
{
	//keep moving until the move subtask has finished (we got there)
	if (GetSubTask()) {
		if (GetIsSubtaskFinished(GetSubTask()->GetTaskType())) {
			//subtask is finished
			return FSM_Quit;
		} else {
			//subtask is still running
			return FSM_Continue;
		}
	} else {
		//subtask was null but not ever finished...quit
		return FSM_Quit;
	}
}

s32 CTaskGenericMoveToPoint::GetDefaultStateAfterAbort() const
{
	return State_Moving;
}

float CTaskGenericMoveToPoint::GetTargetRadius() const
{
	return m_fTargetRadius;
}


//name debug info
#if !__FINAL
const char * CTaskGenericMoveToPoint::GetStaticStateName( s32 iState )
{
	switch(iState)
	{
		case State_Moving: return "Moving";
		default: return "NULL";
	}
}
#endif //!__FINAL