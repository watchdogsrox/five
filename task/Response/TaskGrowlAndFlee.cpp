// Class Header
#include "Task/Response/TaskGrowlAndFlee.h"

// Game Headers
#include "ai/AITarget.h"
#include "Peds/Ped.h"
#include "Peds/PedMotionData.h"
#include "Task/Motion/TaskMotionBase.h"
#include "Task/Movement/TaskIdle.h"
#include "Task/Response/TaskFlee.h"

AI_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CTaskGrowlAndFlee
//////////////////////////////////////////////////////////////////////////

CTaskGrowlAndFlee::Tunables CTaskGrowlAndFlee::sm_Tunables;
IMPLEMENT_RESPONSE_TASK_TUNABLES(CTaskGrowlAndFlee, 0x2c136b73);

CTaskGrowlAndFlee::CTaskGrowlAndFlee(CEntity* pTarget)
: m_pTarget(pTarget)
, m_ClipSet(CLIP_SET_GROWL)
, m_Clip(CLIP_GROWL)
{
	SetInternalTaskType(CTaskTypes::TASK_GROWL_AND_FLEE);
}

CTaskGrowlAndFlee::~CTaskGrowlAndFlee()
{
}

void CTaskGrowlAndFlee::CleanUp()
{

}

CTask::FSM_Return CTaskGrowlAndFlee::ProcessPreFSM()
{
	// Target no longer exists.
	if(!m_pTarget)
	{
		return FSM_Quit;
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskGrowlAndFlee::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		FSM_State(State_Initial)
			FSM_OnUpdate
		return Initial_OnUpdate();

		FSM_State(State_Face)
			FSM_OnEnter
				Face_OnEnter();
			FSM_OnUpdate
				return Face_OnUpdate();

		FSM_State(State_StreamGrowl)
			FSM_OnUpdate
				return StreamGrowl_OnUpdate();

		FSM_State(State_Growl)
			FSM_OnEnter
				Growl_OnEnter();
			FSM_OnUpdate
				return Growl_OnUpdate();

		FSM_State(State_Retreat)
			FSM_OnEnter
				Retreat_OnEnter();
			FSM_OnUpdate
				return Retreat_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskGrowlAndFlee::Initial_OnUpdate()
{
	SetState(State_Face);
	return FSM_Continue;
}

void CTaskGrowlAndFlee::Face_OnEnter()
{
	CTaskTurnToFaceEntityOrCoord* pTask = rage_new CTaskTurnToFaceEntityOrCoord(m_pTarget);
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskGrowlAndFlee::Face_OnUpdate()
{
	m_AmbientClipSetRequestHelper.RequestClipSet(m_ClipSet);
	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		if (m_AmbientClipSetRequestHelper.IsClipSetStreamedIn())
		{
			SetState(State_Growl);
		}
		else
		{
			SetState(State_StreamGrowl);
		}
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskGrowlAndFlee::StreamGrowl_OnUpdate()
{
	m_AmbientClipSetRequestHelper.RequestClipSet(m_ClipSet);
	if (m_AmbientClipSetRequestHelper.IsClipSetStreamedIn())
	{
		SetState(State_Growl);
	}
	return FSM_Continue;
}

void CTaskGrowlAndFlee::Growl_OnEnter()
{
	StartClipBySetAndClip(GetPed(), m_ClipSet, m_Clip, NORMAL_BLEND_IN_DELTA);

	CTaskSmartFlee* pTask = rage_new CTaskSmartFlee(CAITarget(m_pTarget));
	pTask->SetMoveBlendRatio(MOVEBLENDRATIO_STILL);
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskGrowlAndFlee::Growl_OnUpdate()
{
	if (!GetClipHelper() || GetClipHelper()->IsBlendingOut())
	{
		SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
		SetState(State_Retreat);
	}
	return FSM_Continue;
}

void CTaskGrowlAndFlee::Retreat_OnEnter()
{
	CTask* pSubTask = GetSubTask();
	if (pSubTask)
	{
		if (Verifyf(pSubTask->GetTaskType() == CTaskTypes::TASK_SMART_FLEE, "Unexpected subtask in TaskGrowlAndFlee."))
		{
			CTaskSmartFlee* pFleeTask = static_cast<CTaskSmartFlee*>(pSubTask);
			pFleeTask->SetMoveBlendRatio(sm_Tunables.m_FleeMBR);
		}
	}
	else
	{
		CTaskSmartFlee* pTask = rage_new CTaskSmartFlee(CAITarget(m_pTarget));
		pTask->SetMoveBlendRatio(sm_Tunables.m_FleeMBR);
		SetNewTask(pTask);
	}
}

CTask::FSM_Return CTaskGrowlAndFlee::Retreat_OnUpdate()
{
	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Finish);
	}
	else
	{
		CPed* pPed = GetPed();
		if (!pPed->GetUsingRagdoll())
		{
			float fCurrentHeading = pPed->GetCurrentHeading();
			float fDesiredHeading = pPed->GetDesiredHeading();
			float fDelta = CTaskMotionBase::InterpValue(fCurrentHeading, fDesiredHeading, 1.0f, true);

			pPed->GetMotionData()->SetExtraHeadingChangeThisFrame(fDelta);
		}
	}
	return FSM_Continue;
}


#if !__FINAL
const char * CTaskGrowlAndFlee::GetStaticStateName( s32 iState )
{
	taskAssert(iState >= State_Initial && iState <= State_Finish);

	switch (iState)
	{
	case State_Initial:				return "State_Initial";			
	case State_Face:				return "State_Face";
	case State_StreamGrowl:			return "State_StreamGrowl";
	case State_Growl:				return "State_Growl";
	case State_Retreat:				return "State_Retreat";
	case State_Finish:				return "State_Finish";
	default: taskAssert(0); 	
	}

	return "State_Invalid";
}
#endif // !__FINAL