/////////////////////////////////////////////////////////////////////////////////
// FILE :		TaskScenarioAnimDebug.cpp
// PURPOSE :	Task used for debugging scenario animations
// AUTHOR :		Jason Jurecka.
// CREATED :	4/30/2012
/////////////////////////////////////////////////////////////////////////////////

#include "task/Scenario/TaskScenarioAnimDebug.h"

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

// framework headers

// game includes
#include "Peds/PedIntelligence.h"
#include "task/Movement/TaskNavMesh.h"
#include "task/Scenario/ScenarioAnimDebugHelper.h"
#include "task/Scenario/ScenarioPoint.h"
#include "task/Scenario/Types/TaskUseScenario.h"

AI_OPTIMISATIONS()

#if SCENARIO_DEBUG

///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////
CTaskScenarioAnimDebug::CTaskScenarioAnimDebug(CScenarioAnimDebugHelper& helper)
: m_ScenarioAnimDebugHelper(&helper)
, m_UsedMoveExit(false)
{
	SetInternalTaskType(CTaskTypes::TASK_SCENARIO_ANIM_DEBUG);
}

CTaskScenarioAnimDebug::~CTaskScenarioAnimDebug()
{

}

CTask::FSM_Return CTaskScenarioAnimDebug::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

	FSM_State(State_WaitForAnimQueueStart)
		FSM_OnUpdate
			return StateWaitForAnimQueueStart_OnUpdate();

	FSM_State(State_RequestQueueAnims)
		FSM_OnEnter
			return StateRequestQueueAnims_OnEnter();
		FSM_OnUpdate
			return StateRequestQueueAnims_OnUpdate();
		
	FSM_State(State_ProcessAnimQueue)
		FSM_OnEnter
			return StateProcessAnimQueue_OnEnter();
		FSM_OnUpdate
			return StateProcessAnimQueue_OnUpdate();
	
	FSM_End
}

CTask::FSM_Return CTaskScenarioAnimDebug::StateWaitForAnimQueueStart_OnUpdate()
{
	DoStandStill();

	if (m_ScenarioAnimDebugHelper && m_ScenarioAnimDebugHelper->WantsToPlay())
	{
		SetState(State_RequestQueueAnims);
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskScenarioAnimDebug::StateRequestQueueAnims_OnEnter()
{
	Assert(m_ScenarioAnimDebugHelper);
	m_RequestHelper.ReleaseAllClipSets();
	m_ScenarioAnimDebugHelper->GoToQueueStart();
	do 
	{
		fwMvClipSetId wanted = m_ScenarioAnimDebugHelper->GetCurrentInfo().m_ClipSetId;
		if (!m_RequestHelper.HaveAddedClipSet(wanted))
		{
			m_RequestHelper.AddClipSetRequest(wanted);
		}
	} while (m_ScenarioAnimDebugHelper->GoToQueueNext(true));
	m_ScenarioAnimDebugHelper->GoToQueueStart();//go back to the start ...

	return FSM_Continue;
}

CTask::FSM_Return CTaskScenarioAnimDebug::StateRequestQueueAnims_OnUpdate()
{
	if (!m_ScenarioAnimDebugHelper->WantsToPlay())
	{
		SetState(State_WaitForAnimQueueStart);
		return FSM_Continue;
	}

	if(!m_RequestHelper.RequestAllClipSets())
	{
		static float timeSec = 5.0f;
		if (GetTimeInState() > timeSec)
		{
			//Go back to wait state and pop up an error
			Errorf("STOPPING QUEUE PLAYBACK Waited %f secs to stream in clipsets please make sure they are correct and loadable.", timeSec);
			m_ScenarioAnimDebugHelper->OnStopQueuePressed();//stop the queue ... 
		}
		return FSM_Continue;
	}

	SetState(State_ProcessAnimQueue);
	return FSM_Continue;
}

CTask::FSM_Return CTaskScenarioAnimDebug::StateProcessAnimQueue_OnEnter()
{	
	//start the party... 
	m_ScenarioAnimDebugHelper->GoToQueueStart();
	StartUseScenario();
	return FSM_Continue;
}

CTask::FSM_Return CTaskScenarioAnimDebug::StateProcessAnimQueue_OnUpdate()
{
	Assert(m_ScenarioAnimDebugHelper);
	
	if (!GetSubTask())
	{
		if (m_ScenarioAnimDebugHelper->WantsToPlay() &&
			!m_UsedMoveExit && 
			(
				m_ScenarioAnimDebugHelper->GetCurrentInfo().m_AnimType == CConditionalAnims::AT_EXIT || 
				m_ScenarioAnimDebugHelper->GetCurrentInfo().m_AnimType == CConditionalAnims::AT_PANIC_EXIT
			) && 
			m_ScenarioAnimDebugHelper->UsesEnterExitOff()
		   )
		{
			CScenarioPoint* scenarioPoint = m_ScenarioAnimDebugHelper->GetScenarioPoint();
			Assert(scenarioPoint);

			CNavParams navParams;
			navParams.m_vTarget = VEC3V_TO_VECTOR3(scenarioPoint->GetPosition() + m_ScenarioAnimDebugHelper->GetEnterExitOff());
			
			SetNewTask(rage_new CTaskComplexControlMovement(rage_new CTaskMoveFollowNavMesh(navParams)));
			m_UsedMoveExit = true;
		}
		else if (m_ScenarioAnimDebugHelper->WantsToPlay() && m_ScenarioAnimDebugHelper->GoToQueueNext())
		{
			StartUseScenario();
		}
		else
		{
			//There is no next so go to wait state.
			SetState(State_WaitForAnimQueueStart);
		}
	}
	else if (!m_ScenarioAnimDebugHelper->WantsToPlay())
	{
		GetSubTask()->MakeAbortable(ABORT_PRIORITY_IMMEDIATE, NULL);
	}
	else if (GetSubTask()->GetTaskType() == CTaskTypes::TASK_USE_SCENARIO)
	{
		CTaskUseScenario* useTask = static_cast<CTaskUseScenario*>(GetSubTask());
		if (useTask->ReadyForNextOverride())
		{
			if (m_ScenarioAnimDebugHelper->GoToQueueNext())
			{
				//If the next queue item is not an intro anim then set the next one else we need to 
				// step back and let it be handled using a new task.
				if (m_ScenarioAnimDebugHelper->GetCurrentInfo().m_AnimType != CConditionalAnims::AT_ENTER)
				{
					if (m_ScenarioAnimDebugHelper->GetCurrentInfo().m_AnimType == CConditionalAnims::AT_EXIT || m_ScenarioAnimDebugHelper->GetCurrentInfo().m_AnimType == CConditionalAnims::AT_PANIC_EXIT)
					{
						m_UsedMoveExit = false;
					}
					useTask->SetOverrideAnimData(m_ScenarioAnimDebugHelper->GetCurrentInfo());
				}
				else
				{
					useTask->SetOverrideAnimData(ScenarioAnimDebugInfo());
					m_ScenarioAnimDebugHelper->GoToQueuePrev();
				}
			}
			else
			{
				useTask->SetOverrideAnimData(ScenarioAnimDebugInfo());
				m_ScenarioAnimDebugHelper->GoToQueuePrev(); //step back so we finish what we were doing ... 
			}
		}
	}
	return FSM_Continue;
}

void CTaskScenarioAnimDebug::DoStandStill()
{
	// Only set the moveblend ratio to still if this ped has no movement task which is moving
	CPed* pPed = GetPed();
	Assert(pPed);
	CTaskMove * pTask = pPed->GetPedIntelligence()->GetActiveSimplestMovementTask();
	if(!pTask || !pTask->IsTaskMoving())
	{
		pPed->GetMotionData()->SetDesiredMoveBlendRatio(MOVEBLENDRATIO_STILL);
	}
}

void CTaskScenarioAnimDebug::StartUseScenario()
{
	CScenarioPoint* scenarioPoint = m_ScenarioAnimDebugHelper->GetScenarioPoint();
	Assert(scenarioPoint);
	int flags = CTaskUseScenario::SF_Warp;
	const CConditionalAnims::eAnimType type = m_ScenarioAnimDebugHelper->GetCurrentInfo().m_AnimType;
	if (type == CConditionalAnims::AT_ENTER && m_ScenarioAnimDebugHelper->UsesEnterExitOff())
	{
		flags &= ~CTaskUseScenario::SF_Warp;
		CPed* pPed = GetPed();
		Assert(pPed);
		const float heading = pPed->GetCurrentHeading();
		pPed->Teleport(VEC3V_TO_VECTOR3(scenarioPoint->GetPosition() + m_ScenarioAnimDebugHelper->GetEnterExitOff()), heading, true);
	}
	if (type == CConditionalAnims::AT_EXIT || type == CConditionalAnims::AT_PANIC_EXIT)
	{
		m_UsedMoveExit = false;
	}
	CTaskUseScenario* pTask = rage_new CTaskUseScenario(scenarioPoint->GetScenarioTypeVirtualOrReal(), scenarioPoint, flags);
	pTask->SetOverrideAnimData(m_ScenarioAnimDebugHelper->GetCurrentInfo());
	
	// Flag to attempt to create a prop on construction
	pTask->CreateProp();

	SetNewTask(pTask);
}

#endif // SCENARIO_DEBUG