#include "TaskWitness.h"

#include "game/Wanted.h"
#include "game/WitnessInformation.h"
#include "task/Movement/TaskNavMesh.h"
#include "task/Combat/TaskSearch.h"
#include "Peds/ped.h"
#include "Peds/PedGeometryAnalyser.h"
#include "peds/PedIntelligence.h"
#include "system/nelem.h"

AI_OPTIMISATIONS()

IMPLEMENT_SERVICE_TASK_TUNABLES(CTaskWitness, 0x787e434e);

CTaskWitness::Tunables CTaskWitness::sm_Tunables;

CTaskWitness::CTaskWitness(CPed* pCriminal, Vec3V_ConstRef crimePos)
		: m_Criminal(pCriminal)
		, m_CrimePos(crimePos)
		, m_TimeEnterStateMs(0)
{
	SetInternalTaskType(CTaskTypes::TASK_WITNESS);
}

bool CTaskWitness::CanGiveToWitness(const CPed& rPed)
{
	//Ensure the ped will move to law enforcement.
	if(!CWitnessInformationManager::GetInstance().WillMoveToLawEnforcement(rPed))
	{
		return false;
	}

	return true;
}

bool CTaskWitness::GiveToWitness(CPed& rPed, CPed& rCriminal, Vec3V_In vCrimePos)
{
	//Assert that we can give the task to the witness.
	taskAssert(CanGiveToWitness(rPed));

	CTask* pTask = rage_new CTaskWitness(&rCriminal, vCrimePos);
	if(!pTask)
	{
		return false;
	}

	// Not sure exactly about priority and stuff here. PED_TASK_PRIORITY_EVENT_RESPONSE_NONTEMP
	// seems to work better than PED_TASK_PRIORITY_EVENT_RESPONSE_TEMP, though, because with TEMP,
	// they appear to lose their task if you aim at them. /FF
	CEventGivePedTask event(PED_TASK_PRIORITY_EVENT_RESPONSE_NONTEMP, pTask);
	CEvent* pEvent = rPed.GetPedIntelligence()->AddEvent(event);
	if(!pEvent)
	{
		// I think ~CEventGivePedTask() would delete the task in this case.
		return false;
	}

	return true;
}

CTask::FSM_Return CTaskWitness::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_MoveNearCrime)
			FSM_OnEnter
				return MoveNearCrime_OnEnter();
			FSM_OnUpdate
				return MoveNearCrime_OnUpdate();			

		FSM_State(State_MoveToLaw)
			FSM_OnEnter
				return MoveToLaw_OnEnter();
			FSM_OnUpdate
				return MoveToLaw_OnUpdate();			

		FSM_State(State_Search)
			FSM_OnEnter
				return Search_OnEnter();
			FSM_OnUpdate
				return Search_OnUpdate();			

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}


void CTaskWitness::CleanUp()
{
}

#if !__FINAL

const char * CTaskWitness::GetStaticStateName( s32 iState )
{
	static const char* s_StateNames[] =
	{
		"Start",
		"MoveNearCrime",
		"MoveToLaw",
		"Search",
		"Finish"
	};
	CompileTimeAssert(NELEM(s_StateNames) == State_Finish + 1);
	taskAssert(iState >= State_Start && iState <= State_Finish);

	return s_StateNames[iState];
}

#endif	// !__FINAL

CTask::FSM_Return CTaskWitness::Start_OnUpdate()
{
	SetState(State_MoveNearCrime);
	return FSM_Continue;
}


CTask::FSM_Return CTaskWitness::MoveNearCrime_OnEnter()
{
	m_TimeEnterStateMs = fwTimer::GetTimeInMilliseconds();

	CTaskMoveFollowNavMesh* pMoveTask = rage_new CTaskMoveFollowNavMesh(MOVEBLENDRATIO_WALK, RCC_VECTOR3(m_CrimePos));
	CTask* pNewTask = rage_new CTaskComplexControlMovement(pMoveTask, NULL);
	SetNewTask(pNewTask);

	return FSM_Continue;
}


CTask::FSM_Return CTaskWitness::MoveNearCrime_OnUpdate()
{
	if(CheckSeesCriminal())
	{
		SetState(State_MoveToLaw);
		return FSM_Continue;
	}

	if(HasSpentTimeInState(sm_Tunables.m_MaxTimeMoveNearCrimeMs))
	{
		ReportWitnessUnsuccessful();
		SetState(State_Finish);
		return FSM_Continue;
	}

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Search);
		return FSM_Continue;
	}

	CPed* pPed = GetPed();

	CTask* pSubTask = GetSubTask();
	if(pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
	{
		CTaskComplexControlMovement* pCtrlMove = static_cast<CTaskComplexControlMovement*>(GetSubTask());
		CTask* pMoveTask = pCtrlMove->GetRunningMovementTask(pPed);
		if(pMoveTask && pMoveTask->GetTaskType() == CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH)
		{
			//eNavMeshRouteResult routeResult = static_cast<CTaskMoveFollowNavMesh*>(pMoveTask)->GetNavMeshRouteResult();
			//if (routeResult == NAVMESHROUTE_ROUTE_NOT_FOUND)
			if(static_cast<CTaskMoveFollowNavMesh*>(pMoveTask)->IsUnableToFindRoute())
			{
				SetState(State_Search);
				return FSM_Continue;
			}
		}
	}

	return FSM_Continue;
}


CTask::FSM_Return CTaskWitness::MoveToLaw_OnEnter()
{
	m_TimeEnterStateMs = fwTimer::GetTimeInMilliseconds();

	const Vec3V& lawReportPoint = CCrimeWitnesses::GetWitnessReportPosition();

	CTaskMoveFollowNavMesh* pMoveTask = rage_new CTaskMoveFollowNavMesh(MOVEBLENDRATIO_RUN, RCC_VECTOR3(lawReportPoint));
	CTask* pNewTask = rage_new CTaskComplexControlMovement(pMoveTask, NULL);
	SetNewTask(pNewTask);

	return FSM_Continue;
}


CTask::FSM_Return CTaskWitness::MoveToLaw_OnUpdate()
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		ReportCrimes();

		SetState(State_Finish);
		return FSM_Continue;
	}

	if(HasSpentTimeInState(sm_Tunables.m_MaxTimeMoveToLawMs))
	{
		ReportWitnessUnsuccessful();

		SetState(State_Finish);
		return FSM_Continue;
	}

	CPed* pPed = GetPed();

	CTask* pSubTask = GetSubTask();
	if(pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
	{
		CTaskComplexControlMovement* pCtrlMove = static_cast<CTaskComplexControlMovement*>(GetSubTask());
		CTask* pMoveTask = pCtrlMove->GetRunningMovementTask(pPed);
		if(pMoveTask && pMoveTask->GetTaskType() == CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH)
		{
			//eNavMeshRouteResult routeResult = static_cast<CTaskMoveFollowNavMesh*>(pMoveTask)->GetNavMeshRouteResult();
			//if (routeResult == NAVMESHROUTE_ROUTE_NOT_FOUND)
			if(static_cast<CTaskMoveFollowNavMesh*>(pMoveTask)->IsUnableToFindRoute())
			{
				if(HasSpentTimeInState(sm_Tunables.m_MaxTimeMoveToLawFailedPathfindingMs))
				{
					ReportWitnessUnsuccessful();

					SetState(State_Finish);
					return FSM_Continue;
				}
			}
		}
	}

	return FSM_Continue;
}


CTask::FSM_Return CTaskWitness::Search_OnEnter()
{
	m_TimeEnterStateMs = fwTimer::GetTimeInMilliseconds();
	
	//Create the params.
	CTaskSearchBase::Params params;
	CTaskSearchBase::LoadDefaultParamsForPedAndTarget(*GetPed(), *m_Criminal, params);
	
	//Create the task.
	CTask* pTask = rage_new CTaskSearch(params);
	
	//Start the task.
	SetNewTask(pTask);

	return FSM_Continue;

}


CTask::FSM_Return CTaskWitness::Search_OnUpdate()
{
	if(CheckSeesCriminal())
	{
		SetState(State_MoveToLaw);
		return FSM_Continue;
	}

	if(HasSpentTimeInState(sm_Tunables.m_MaxTimeSearchMs))
	{
		ReportWitnessUnsuccessful();

		SetState(State_Finish);
		return FSM_Continue;
	}

	// For now, we stay in this state even if the task finishes, until we time
	// out. Right now, CTaskSearch tends to end within a few seconds when used
	// outside of a combat situation (CPedTargeting not active).
#if 0
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_MoveToLaw);
		return FSM_Continue;
	}
#endif

	return FSM_Continue;
}


bool CTaskWitness::CheckSeesCriminal()
{
	CPed* pCriminal = m_Criminal;
	if(!pCriminal)
	{
		return false;
	}

	// Not entirely sure about what flags to use. We may want to adapt more of
	// the code from CPedAcquaintanceScanner::CanPedSeePed() here, using CPedTargetting
	// if possible, etc. /FF
	const s32 iTargetFlags = TargetOption_UseFOVPerception;

	CPed* pPed = GetPed();
	const ECanTargetResult losRet = CPedGeometryAnalyser::CanPedTargetPedAsync(*pPed, *pCriminal, iTargetFlags);

#if (DEBUG_DRAW) && (__BANK)
	if(CCrimeWitnesses::ms_BankDrawVisibilityTests)		// May as well reuse this option, used for the first observation of crimes.
	{
		Color32 col;
		if(losRet == ECanTarget)
		{
			col = Color_green;
		}
		else if(losRet == ECanNotTarget)
		{
			col = Color_red;
		}
		else
		{
			col = Color_grey;
		}
		grcDebugDraw::Line(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), VEC3V_TO_VECTOR3(pCriminal->GetTransform().GetPosition()), col);
	}
#endif

	return losRet == ECanTarget;
}


void CTaskWitness::ReportCrimes()
{
	CPed* pCriminal = m_Criminal;
	if(!pCriminal)
	{
		return;
	}

	CWanted* pWanted = pCriminal->GetPlayerWanted();
	if(!pWanted)
	{
		return;
	}

	pWanted->ReportWitnessSuccessful(GetPed());
}


void CTaskWitness::ReportWitnessUnsuccessful()
{
	CPed* pCriminal = m_Criminal;
	if(!pCriminal)
	{
		return;
	}

	CWanted* pWanted = pCriminal->GetPlayerWanted();
	if(!pWanted)
	{
		return;
	}

	pWanted->ReportWitnessUnsuccessful(GetPed());
}


bool CTaskWitness::HasSpentTimeInState(u32 timeMs) const
{
	u32 timeElapsed = fwTimer::GetTimeInMilliseconds() - m_TimeEnterStateMs;
	return timeElapsed >= timeMs;
}


// End of file 'task/Service/Witness/TaskWitness.cpp'
