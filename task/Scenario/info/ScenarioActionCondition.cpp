#include "Task/Scenario/Info/ScenarioActionCondition.h"

//Game Headers
#include "Peds/Ped.h"
#include "Task/Scenario/Info/ScenarioInfo.h"
#include "Task/Scenario/Types/TaskUseScenario.h"

AI_SCENARIO_OPTIMISATIONS()


INSTANTIATE_RTTI_CLASS(CScenarioActionCondition,0xC38B58FE);
INSTANTIATE_RTTI_CLASS(CScenarioActionConditionNot,0x229AE540);
INSTANTIATE_RTTI_CLASS(CScenarioActionConditionEvent,0xE8EB17FA);
INSTANTIATE_RTTI_CLASS(CScenarioActionConditionResponseTask,0xc47f3990)
INSTANTIATE_RTTI_CLASS(CScenarioActionConditionInRange,0xFBF45B32);
INSTANTIATE_RTTI_CLASS(CScenarioActionConditionCloseOrRecent,0x66641CA4);
INSTANTIATE_RTTI_CLASS(CScenarioActionConditionHasShockingReact,0x22BABC7D);
INSTANTIATE_RTTI_CLASS(CScenarioActionConditionHasCowardReact,0x1E3787F9);
INSTANTIATE_RTTI_CLASS(CScenarioActionConditionIsAGangPed,0xBCD22E2A);
INSTANTIATE_RTTI_CLASS(CScenarioActionConditionIsACopPed,0xF9445B35);
INSTANTIATE_RTTI_CLASS(CScenarioActionConditionIsASecurityPed,0xA28E9441);
INSTANTIATE_RTTI_CLASS(CScenarioActionConditionForceAction,0xF89BA015);
INSTANTIATE_RTTI_CLASS(CScenarioActionConditionResponseType,0x33C600FA);
INSTANTIATE_RTTI_CLASS(CScenarioActionConditionCurrentlyRespondingToOtherEvent,0x8E7379A);
INSTANTIATE_RTTI_CLASS(CScenarioActionConditionCanDoQuickBlendout,0xCB44F67F);


//////////////////////////////////////////////////////////////
// CScenarioActionConditionForceAction
//////////////////////////////////////////////////////////////

bool CScenarioActionConditionForceAction::Check(ScenarioActionContext& rContext) const
{
	if (rContext.m_Event.GetEventType() == EVENT_SCENARIO_FORCE_ACTION )
	{
		const CEventScenarioForceAction& e = static_cast<const CEventScenarioForceAction&>(rContext.m_Event);
		if (e.GetForceActionType() == m_ScenarioActionType )
		{
			CTask* pTask = rContext.m_Task;
			if (Verifyf(pTask && pTask->GetTaskType() == CTaskTypes::TASK_USE_SCENARIO, "Cannot run a scenario action when not running CTaskUseScenario"))
			{
				return true;
			}
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////
// CScenarioActionConditionNot
//////////////////////////////////////////////////////////////

bool CScenarioActionConditionNot::Check(ScenarioActionContext& rContext) const
{
	if (Verifyf(m_Condition, "Sub-condition was null!"))
	{
		return ! m_Condition->Check(rContext);
	}
	return false;
}

//////////////////////////////////////////////////////////////
// CScenarioActionConditionEvent
//////////////////////////////////////////////////////////////

bool CScenarioActionConditionEvent::Check(ScenarioActionContext& rContext) const
{
	if (rContext.m_Event.GetEventType() == m_EventType)
	{
		CTask* pTask = rContext.m_Task;
		if (Verifyf(pTask && pTask->GetTaskType() == CTaskTypes::TASK_USE_SCENARIO, "Cannot run a scenario action when not running CTaskUseScenario"))
		{
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////
// CScenarioActionConditionResponseTask
//////////////////////////////////////////////////////////////

bool CScenarioActionConditionResponseTask::Check(ScenarioActionContext& rContext) const
{
	if (rContext.m_ResponseTaskType == CEventEditableResponse::FindTaskTypeForResponse(m_TaskType))
	{
		CTask* pTask = rContext.m_Task;
		if (Verifyf(pTask && pTask->GetTaskType() == CTaskTypes::TASK_USE_SCENARIO, "Cannot run a scenario action when not running CTaskUseScenario"))
		{
			return true;
		}
	}
	return false;
}

///////////////////////////////////////////////////////////////
// CScenarioActionConditionInRange
///////////////////////////////////////////////////////////////

bool CScenarioActionConditionInRange::Check(ScenarioActionContext& rContext) const
{
	CTask* pTask = rContext.m_Task;

	if (Verifyf(pTask && pTask->GetTaskType() == CTaskTypes::TASK_USE_SCENARIO, "Cannot run a scenario action when not running CTaskUseScenario"))
	{
		const CPed* pPed = pTask->GetPed();
		if (IsLessThanAll(DistSquared(rContext.m_Event.GetEntityOrSourcePosition(), pPed->GetTransform().GetPosition()), ScalarV(rage::square(m_Range))))
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	return false;
}

///////////////////////////////////////////////////////////////
// CScenarioActionConditionCloseOrRecent
///////////////////////////////////////////////////////////////

bool CScenarioActionConditionCloseOrRecent::Check(ScenarioActionContext& rContext) const
{
	CTask* pTask = rContext.m_Task;

	if (Verifyf(pTask && pTask->GetTaskType() == CTaskTypes::TASK_USE_SCENARIO, "Cannot run a scenario action when not running CTaskUseScenario"))
	{
		const CPed* pPed = pTask->GetPed();
		CTaskUseScenario* pTaskScenario = static_cast<CTaskUseScenario*>(pTask);
		if (pTaskScenario->HasRespondedToRecently(rContext.m_Event))
		{
			return true;
		}

		if (IsLessThanAll(DistSquared(rContext.m_Event.GetEntityOrSourcePosition(), pPed->GetTransform().GetPosition()), ScalarV(rage::square(m_Range))))
		{
			return true;
		}
	}

	return false;
}

///////////////////////////////////////////////////////////////
// CScenarioActionConditionHasShockingReact
///////////////////////////////////////////////////////////////


bool CScenarioActionConditionHasShockingReact::Check(ScenarioActionContext& rContext) const
{
	CTask* pTask = rContext.m_Task;
	if (Verifyf(pTask && pTask->GetTaskType() == CTaskTypes::TASK_USE_SCENARIO, "Cannot run a scenario action when not running CTaskUseScenario"))
	{
		const CTaskUseScenario* pTaskScenario = static_cast<const CTaskUseScenario*>(pTask);
		if (pTaskScenario->HasAValidShockingReactionAnimation(rContext.m_Event))
		{
			return true;
		}
	}

	return false;
}

///////////////////////////////////////////////////////////////
// CScenarioActionConditionHasCowardReact
///////////////////////////////////////////////////////////////

bool CScenarioActionConditionHasCowardReact::Check(ScenarioActionContext& rContext) const
{
	CTask* pTask = rContext.m_Task;
	if (Verifyf(pTask && pTask->GetTaskType() == CTaskTypes::TASK_USE_SCENARIO, "Cannot run a scenario action when not running CTaskUseScenario"))
	{
		const CTaskUseScenario* pTaskScenario = static_cast<const CTaskUseScenario*>(pTask);
		if (pTaskScenario->HasAValidCowardReactionAnimation(rContext.m_Event))
		{
			return true;
		}
	}

	return false;
}

///////////////////////////////////////////////////////////////
// CScenarioActionConditionIsAGangPed
///////////////////////////////////////////////////////////////

bool CScenarioActionConditionIsAGangPed::Check(ScenarioActionContext& rContext) const
{
	CTask* pTask = rContext.m_Task;
	if (Verifyf(pTask && pTask->GetTaskType() == CTaskTypes::TASK_USE_SCENARIO, "Cannot run a scenario action when not running CTaskUseScenario"))
	{
		if (pTask->GetPed()->IsGangPed())
		{
			return true;
		}
	}
	return false;
}

///////////////////////////////////////////////////////////////
// CScenarioActionConditionIsACopPed
///////////////////////////////////////////////////////////////

bool CScenarioActionConditionIsACopPed::Check(ScenarioActionContext& rContext) const
{
	CTask* pTask = rContext.m_Task;
	if (Verifyf(pTask && pTask->GetTaskType() == CTaskTypes::TASK_USE_SCENARIO, "Cannot run a scenario action when not running CTaskUseScenario"))
	{
		if (pTask->GetPed()->IsRegularCop() || pTask->GetPed()->GetPedType() == PEDTYPE_SWAT)
		{
			return true;
		}
	}
	return false;
}

///////////////////////////////////////////////////////////////
// CScenarioActionConditionIsASecurityPed
///////////////////////////////////////////////////////////////

bool CScenarioActionConditionIsASecurityPed::Check(ScenarioActionContext& rContext) const
{
	CTask* pTask = rContext.m_Task;
	if (Verifyf(pTask && pTask->GetTaskType() == CTaskTypes::TASK_USE_SCENARIO, "Cannot run a scenario action when not running CTaskUseScenario"))
	{
		if (pTask->GetPed()->IsSecurityPed())
		{
			return true;
		}
	}
	return false;
}


/////////////////////////////////////////////////////////////////////////////////////
// CScenarioActionConditionResponseType
/////////////////////////////////////////////////////////////////////////////////////

bool CScenarioActionConditionResponseType::Check(ScenarioActionContext& rContext) const
{
	CTask* pTask = rContext.m_Task;
	if (Verifyf(pTask && pTask->GetTaskType() == CTaskTypes::TASK_USE_SCENARIO, "Cannot run a scenario action when not running CTaskUseScenario"))
	{
		if (m_ResponseHash.IsNotNull())
		{
			CTaskTypes::eTaskType iType = CEventEditableResponse::FindTaskTypeForResponse(m_ResponseHash.GetHash());
			return iType == rContext.m_Event.GetResponseTaskType();
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////////////////
// CScenarioActionConditionCurrentlyRespondingToOtherEvent
//////////////////////////////////////////////////////////////////////////////////////

bool CScenarioActionConditionCurrentlyRespondingToOtherEvent::Check(ScenarioActionContext& rContext) const
{
	return rContext.m_EventCurrentlyBeingHandledbyTask != -1;
}

//////////////////////////////////////////////////////////////////////////////////////
// CScenarioActionConditionCanDoQuickBlendout
//////////////////////////////////////////////////////////////////////////////////////

bool CScenarioActionConditionCanDoQuickBlendout::Check(ScenarioActionContext& rContext) const
{
	CTask* pTask = rContext.m_Task;
	if (Verifyf(pTask && pTask->GetTaskType() == CTaskTypes::TASK_USE_SCENARIO, "Cannot run a scenario action when not running CTaskUseScenario"))
	{
		const CTaskUseScenario* pTaskScenario = static_cast<const CTaskUseScenario*>(pTask);

		if (pTaskScenario->GetScenarioInfo().GetIsFlagSet(CScenarioInfoFlags::DoSimpleBlendoutForEvents))
		{
			return true;
		}
	}
	return false;
}
