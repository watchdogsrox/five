#include "Task/Scenario/Info/ScenarioAction.h"

//Game Headers
#include "Event/EventResponse.h"
#include "Event/EventWeapon.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Task/Combat/TaskThreatResponse.h"
#include "Task/Scenario/Types/TaskUseScenario.h"

//RAGE Headers
#include "fwanimation/animdefines.h"

AI_SCENARIO_OPTIMISATIONS()


INSTANTIATE_RTTI_CLASS(CScenarioAction,0x59A130BB);
INSTANTIATE_RTTI_CLASS(CScenarioActionFlee,0x3ACB3CB3);
INSTANTIATE_RTTI_CLASS(CScenarioActionHeadTrack,0xF776B73F);
INSTANTIATE_RTTI_CLASS(CScenarioActionShockReaction,0x4B414B8B);
INSTANTIATE_RTTI_CLASS(CScenarioActionThreatResponseExit,0x6514CE51);
INSTANTIATE_RTTI_CLASS(CScenarioActionCombatExit,0x81148679);
INSTANTIATE_RTTI_CLASS(CScenarioActionCowardExitThenRespondToEvent,0x1A38DDF8);
INSTANTIATE_RTTI_CLASS(CScenarioActionImmediateExit,0x80B87373);
INSTANTIATE_RTTI_CLASS(CScenarioActionNormalExitThenRespondToEvent,0x8B49345);
INSTANTIATE_RTTI_CLASS(CScenarioActionNormalExit,0x8F435444);
INSTANTIATE_RTTI_CLASS(CScenarioActionScriptExit,0x4431FABD);

////////////////////////////////////////////////////////////////////////////////
// CScenarioActionTriggers
////////////////////////////////////////////////////////////////////////////////

CScenarioActionTriggers::CScenarioActionTriggers()
: m_Triggers()
{

}

CScenarioActionTriggers::~CScenarioActionTriggers()
{
	// Delete pointers owned by this class.
	for(int i=0; i < m_Triggers.GetCount(); i++)
	{
		delete m_Triggers[i];
	}
}




////////////////////////////////////////////////////////////////////////////////
// CScenarioAcionTrigger
////////////////////////////////////////////////////////////////////////////////

CScenarioActionTrigger::~CScenarioActionTrigger()
{
	// Delete the pointers owned by this class.

	for(int i=0; i < m_Conditions.GetCount(); i++)
	{
		delete m_Conditions[i];
	}

	delete m_Action;
}


bool CScenarioActionTrigger::Check(ScenarioActionContext& rContext) const
{
	for(int i=0; i < m_Conditions.GetCount(); i++)
	{
		if (!m_Conditions[i]->Check(rContext))
		{
			return false;
		}
	}
	return true;
}

////////////////////////////////////////////////////////////////////////////////
// CScenarioActionFlee
// Trigger a Scenario Flee Sequence
////////////////////////////////////////////////////////////////////////////////


bool CScenarioActionFlee::Execute(ScenarioActionContext& rContext) const
{
	CTask* pTask = rContext.m_Task;
	if (Verifyf(pTask && pTask->GetTaskType() == CTaskTypes::TASK_USE_SCENARIO, "ScenarioActions not supported for tasks other than CTaskUseScenario yet."))
	{
		CTaskUseScenario* pTaskUseScenario = static_cast<CTaskUseScenario*>(pTask);

		// Set the flee flags.
		pTaskUseScenario->TriggerExitToFlee(rContext.m_Event);
		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////
// CScenarioActionHeadTrack
// Trigger a head track.
////////////////////////////////////////////////////////////////////////////////


bool CScenarioActionHeadTrack::Execute(ScenarioActionContext& rContext) const
{
	CTask* pTask = rContext.m_Task;
	if (Verifyf(pTask && pTask->GetTaskType() == CTaskTypes::TASK_USE_SCENARIO, "ScenarioActions not supported for tasks other than CTaskUseScenario yet."))
	{	
		CTaskUseScenario* pTaskScenario = static_cast<CTaskUseScenario*>(pTask);

		pTaskScenario->TriggerHeadLook(rContext.m_Event);
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////
// CScenarioActionShockReaction
// The ped plays a brief shocking event animation and then returns to normal use of the point.
//////////////////////////////////////////////////////////////////////////////////////////////

bool CScenarioActionShockReaction::Execute(ScenarioActionContext& rContext) const
{
	CTask* pTask = rContext.m_Task;
	if (Verifyf(pTask && pTask->GetTaskType() == CTaskTypes::TASK_USE_SCENARIO, "ScenarioActions not supported for tasks other than CTaskUseScenario yet."))
	{
		CTaskUseScenario* pTaskScenario = static_cast<CTaskUseScenario*>(pTask);
		pTaskScenario->TriggerShockAnimation(rContext.m_Event);
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// CScenarioActionThreatResponse
// The ped determines if it should go into combat or flee and forces the scenario to exit accordingly.
//////////////////////////////////////////////////////////////////////////////////////////////////////

bool CScenarioActionThreatResponseExit::Execute(ScenarioActionContext& rContext) const
{
	CTask* pTask = rContext.m_Task;
	if (Verifyf(pTask && pTask->GetTaskType() == CTaskTypes::TASK_USE_SCENARIO, "ScenarioActions not supported for tasks other than CTaskUseScenario yet."))
	{
		CTaskUseScenario* pTaskScenario = static_cast<CTaskUseScenario*>(pTask);
		CPed* pPed = pTaskScenario->GetPed();

		// Figure out what the threat response will be at this moment in time.
		CTaskThreatResponse::ThreatResponse iResponse = CTaskThreatResponse::PreDetermineThreatResponseStateFromEvent(*pPed, rContext.m_Event);

		// Trigger the appropriate scenario exit.
		if (iResponse == CTaskThreatResponse::TR_Fight)
		{
			pTaskScenario->TriggerExitToCombat(rContext.m_Event);
		}
		else if (iResponse == CTaskThreatResponse::TR_Flee)
		{
			pTaskScenario->TriggerExitToFlee(rContext.m_Event);
		}

		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// CScenarioActionCombatExit
// The ped determines if it should go into combat and exit accordingly.
//////////////////////////////////////////////////////////////////////////////////////////////////////

bool CScenarioActionCombatExit::Execute(ScenarioActionContext& rContext) const
{
	CTask* pTask = rContext.m_Task;
	if (Verifyf(pTask && pTask->GetTaskType() == CTaskTypes::TASK_USE_SCENARIO, "ScenarioActions not supported for tasks other than CTaskUseScenario yet."))
	{
		CTaskUseScenario* pTaskScenario = static_cast<CTaskUseScenario*>(pTask);
		pTaskScenario->TriggerExitToCombat(rContext.m_Event);
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// CScenarioActionCowardExitThenRespondToEvent
// Play a coward exit then run whatever the normal ped response to the event is.
//////////////////////////////////////////////////////////////////////////////////////////////////////

bool CScenarioActionCowardExitThenRespondToEvent::Execute(ScenarioActionContext& rContext) const
{
	CTask* pTask = rContext.m_Task;
	if (Verifyf(pTask && pTask->GetTaskType() == CTaskTypes::TASK_USE_SCENARIO, "ScenarioActions not supported for tasks other than CTaskUseScenario yet."))
	{
		CTaskUseScenario* pTaskScenario = static_cast<CTaskUseScenario*>(pTask);
		pTaskScenario->TriggerCowardThenExitToEvent(rContext.m_Event);
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// CScenarioActionImmediateExit
// Quit the scenario with the metadata derived blendout on the ambient clips subtask.
//////////////////////////////////////////////////////////////////////////////////////////////////////

bool CScenarioActionImmediateExit::Execute(ScenarioActionContext& rContext) const
{
	CTask* pTask = rContext.m_Task;
	if (Verifyf(pTask && pTask->GetTaskType() == CTaskTypes::TASK_USE_SCENARIO, "ScenarioActions not supported for tasks other than CTaskUseScenario yet."))
	{
		CTaskUseScenario* pTaskScenario = static_cast<CTaskUseScenario*>(pTask);
		pTaskScenario->TriggerImmediateExitOnEvent();
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// CScenarioActionNormalExitThenRespondToEvent
// The ped plays their normal exit and then responds to the event.
//////////////////////////////////////////////////////////////////////////////////////////////////////

bool CScenarioActionNormalExitThenRespondToEvent::Execute(ScenarioActionContext& rContext) const
{
	CTask* pTask = rContext.m_Task;
	if (Verifyf(pTask && pTask->GetTaskType() == CTaskTypes::TASK_USE_SCENARIO, "ScenarioActions not supported for tasks other than CTaskUseScenario yet."))
	{
		CTaskUseScenario* pTaskScenario = static_cast<CTaskUseScenario*>(pTask);
		pTaskScenario->TriggerNormalExitToEvent(rContext.m_Event);
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// CScenarioActionNormalExit
// The ped plays their normal exit.
//////////////////////////////////////////////////////////////////////////////////////////////////////

bool CScenarioActionNormalExit::Execute(ScenarioActionContext& rContext) const
{
	CTask* pTask = rContext.m_Task;
	if (Verifyf(pTask && pTask->GetTaskType() == CTaskTypes::TASK_USE_SCENARIO, "ScenarioActions not supported for tasks other than CTaskUseScenario yet."))
	{
		CTaskUseScenario* pTaskScenario = static_cast<CTaskUseScenario*>(pTask);
		pTaskScenario->TriggerNormalExit();
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// CScenarioActionScriptExit
// The ped plays an exit based on what ped config flags have been set on it.
//////////////////////////////////////////////////////////////////////////////////////////////////////

bool CScenarioActionScriptExit::Execute(ScenarioActionContext& rContext) const
{
	CTask* pTask = rContext.m_Task;
	if (Verifyf(pTask && pTask->GetTaskType() == CTaskTypes::TASK_USE_SCENARIO, "ScenarioActions not supported for tasks other than CTaskUseScenario yet."))
	{
		CTaskUseScenario* pTaskScenario = static_cast<CTaskUseScenario*>(pTask);

		// Stop any scripted cowering.
		pTaskScenario->TriggerStopScriptedCowerInPlace();

		CPed* pPed = pTask->GetPed();
		if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ForcePlayDirectedNormalScenarioExitOnNextScriptCommand))
		{
			// Make sure the scenario type actually has exit clips.  Otherwise do an immediate exit.
			if (pTaskScenario->HasAValidNormalExitAnimation())
			{
				// Examine the scenario's reaction position.  If it has not been set yet, then assume the scripter
				// wants the ped to face forward.
				if (pTaskScenario->IsAmbientPositionInvalid())
				{
					pTaskScenario->SetAmbientPosition(pPed->GetTransform().GetPosition() + pPed->GetTransform().GetForward());
				}

				// Trigger a directed normal exit.
				pTaskScenario->TriggerDirectedExitFromScript();
			}
			else
			{
				pTaskScenario->TriggerImmediateExitOnEvent();
			}
		}
		else if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ForcePlayImmediateScenarioExitOnNextScriptCommand))
		{
			// Blend out with a normal blend duration.
			pTaskScenario->TriggerImmediateExitOnEvent();
		}
		else if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ForcePlayFleeScenarioExitOnNextScriptCommand))
		{
			// Do a flee exit.
			pTaskScenario->TriggerFleeExitFromScript();
		}
		else
		{
			// Default to normal exits, but only if the ped actually has one.  Otherwise do an immediate exit.
			if (pTaskScenario->HasAValidNormalExitAnimation())
			{
				pTaskScenario->TriggerNormalExitFromScript();
			}
			else
			{
				pTaskScenario->TriggerImmediateExitOnEvent();
			}
		}
		return true;
	}
	return false;
}
