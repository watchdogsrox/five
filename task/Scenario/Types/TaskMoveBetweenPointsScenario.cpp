#include "TaskMoveBetweenPointsScenario.h"

#include "pathserver/PathServer.h"

#include "Peds/ped.h"
#include "Peds/PedIntelligence.h"
#include "Peds/pedpopulation.h"

#include "Task/General/TaskBasic.h"
#include "Task/Scenario/info/ScenarioInfo.h"
#include "Task/Scenario/Types/TaskUseScenario.h"
#include "Task/Motion/TaskMotionBase.h"

#include "Task/Movement/TaskGoToPoint.h"
#include "Task/Movement/TaskNavMesh.h"

AI_OPTIMISATIONS()

//-------------------------------------------------------------------------
// MoveAbout scenario, moves between nearby spawnpoints and stops for random
// time (e.g. builders walking around a building site, industrial workers, dancers etc...)
//-------------------------------------------------------------------------
CTaskMoveBetweenPointsScenario::CTaskMoveBetweenPointsScenario( s32 scenarioType, CScenarioPoint* pScenarioPoint )
: CTaskScenario(scenarioType,pScenarioPoint)
, m_fTimer(0.0f)
, m_hPathHandle(PATH_HANDLE_NULL)
{
	SetInternalTaskType(CTaskTypes::TASK_MOVE_BETWEEN_POINTS_SCENARIO);
}

CTaskMoveBetweenPointsScenario::~CTaskMoveBetweenPointsScenario()
{
	if(m_hPathHandle != PATH_HANDLE_NULL)
	{
		CPathServer::CancelRequest(m_hPathHandle);
		m_hPathHandle = PATH_HANDLE_NULL;
	}
}

aiTask* CTaskMoveBetweenPointsScenario::Copy() const
{
	return rage_new CTaskMoveBetweenPointsScenario( m_iScenarioIndex, GetScenarioPoint() );
}

dev_float MAX_BETWEEN_POINTS_RANGE = 20.0f;

CTask::FSM_Return CTaskMoveBetweenPointsScenario::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin

		FSM_State(State_Stationary)
		FSM_OnEnter
		Stationary_OnEnter(pPed);
	FSM_OnUpdate
		return Stationary_OnUpdate(pPed);

	FSM_State(State_SearchingForNewPoint)
		FSM_OnEnter
		m_hPathHandle = CPathServer::RequestPath(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), m_vNewPointPos, pPed->GetPedIntelligence()->GetNavCapabilities().BuildPathStyleFlags()|pPed->GetCurrentMotionTask()->GetNavFlags(), 0.0f, pPed);
	FSM_OnUpdate
		return SearchingForNewPoint_OnUpdate(pPed);
	FSM_OnExit
		if(m_hPathHandle != PATH_HANDLE_NULL)
		{
			CPathServer::CancelRequest(m_hPathHandle);
			m_hPathHandle = PATH_HANDLE_NULL;
		}

		FSM_State(State_MovingToNewPoint)
			FSM_OnEnter
			MovingToNewPoint_OnEnter(pPed);
		FSM_OnUpdate
			if( GetIsFlagSet(aiTaskFlags::SubTaskFinished) )
				SetState(State_Stationary);

	FSM_State(State_Finish)
		FSM_OnUpdate
		return FSM_Quit;

	FSM_End
}


CTask::FSM_Return CTaskMoveBetweenPointsScenario::Start_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	//SetState(State_Stationary);
	SetState(State_SearchingForNewPoint);

	return FSM_Continue;
}

void CTaskMoveBetweenPointsScenario::Stationary_OnEnter(CPed* UNUSED_PARAM(pPed))
{
	const CScenarioInfo* pScenarioInfo = CScenarioManager::GetScenarioInfo(m_iScenarioIndex);
	taskAssert(pScenarioInfo->GetIsClass<CScenarioMoveBetweenInfo>());
	m_fTimer = fwRandom::GetRandomNumberInRange( 0.0f, static_cast<const CScenarioMoveBetweenInfo*>(pScenarioInfo)->GetTimeBetweenMoving()*1.2f );
	SetNewTask(rage_new CTaskUseScenario(m_iScenarioIndex, GetScenarioPoint(), (CTaskUseScenario::SF_StartInCurrentPosition | CTaskUseScenario::SF_CheckConditions) ));
}

CTask::FSM_Return CTaskMoveBetweenPointsScenario::Stationary_OnUpdate(CPed* pPed)
{
	if (GetIsSubtaskFinished(CTaskTypes::TASK_USE_SCENARIO)) //( GetFSMFlags().IsFlagSet( Flag_subTaskFinished ) )
	{
		SetState(State_Finish);
		return FSM_Continue;
	}

	m_fTimer -= fwTimer::GetTimeStep();

	if( m_fTimer < 0.0f )
	{
		const CScenarioInfo* pScenarioInfo = CScenarioManager::GetScenarioInfo(m_iScenarioIndex);
		const float fTimeBetweenMoving = static_cast<const CScenarioMoveBetweenInfo*>(pScenarioInfo)->GetTimeBetweenMoving();
		m_fTimer = fwRandom::GetRandomNumberInRange( fTimeBetweenMoving*0.8f, fTimeBetweenMoving*1.2f );
		// Find a new random point
		CScenarioPoint* pNewScenarioPoint = NULL;
		const float fDistance = MAX_BETWEEN_POINTS_RANGE;
		//C2dEffect* pNewEffect = CScenarioManager::FindNearest2dEffect(pPed->GetPosition(), vPos, 20.0f, 3.0f, m_iScenarioIndex, true );
		const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		if( CPedPopulation::GetScenarioPointInArea( vPedPosition, fDistance, 1, (s32*)&m_iScenarioIndex, &pNewScenarioPoint, true, pPed, true ) )
		{
			Assert(pNewScenarioPoint);
			Vector3 vPos = VEC3V_TO_VECTOR3(pNewScenarioPoint->GetPosition());
			if( vPos.Dist2(vPedPosition) > rage::square(2.0f) )
			{
				m_vNewPointPos = vPos;
				// Keep the scenario point accross this transition
				SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
				SetState(State_SearchingForNewPoint);
				return FSM_Continue;
			}
		}

		/*
		#if __DEV
		else if( PARAM_clipviewer.Get() )
		{
		m_vNewPointPos.Set(0.0f, 1.0f, 0.0f);
		m_vNewPointPos.RotateZ(fwRandom::GetRandomNumberInRange(0.0f, TWO_PI));
		m_vNewPointPos.Scale(5.0f);
		m_vNewPointPos+=pPed->GetPosition();
		SetState(State_MovingToNewPoint);
		}
		#endif
		*/
	}

	return FSM_Continue;
}

// void CTaskMoveBetweenPointsScenario::SearchingForNewPoint_OnEnter(CPed* UNUSED_PARAM(pPed))
// {
// 	
// }

CTask::FSM_Return CTaskMoveBetweenPointsScenario::SearchingForNewPoint_OnUpdate(CPed* pPed)
{
	if( m_hPathHandle == PATH_HANDLE_NULL )
	{
		// Keep the scenario point across this transition
		SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
		SetState(State_Stationary);
		return FSM_Continue;
	}
	// The ped is searching for a route to the new position.
	EPathServerRequestResult ret = CPathServer::IsRequestResultReady(m_hPathHandle);
	if(ret == ERequest_Ready)
	{
		// Lock the path & get pointer to result
		int iNumPts = 0;
		Vector3 * pPts = NULL;
		TNavMeshWaypointFlag * pWptFlags = NULL;
		int pathRetVal = CPathServer::LockPathResult(m_hPathHandle, &iNumPts, pPts, pWptFlags);
		const bool bPathFound = pathRetVal == PATH_FOUND;
		CPathServer::UnlockPathResultAndClear(m_hPathHandle);
		m_hPathHandle = PATH_HANDLE_NULL;

		if( bPathFound )
		{
			CScenarioPoint* pNewScenarioPoint = NULL;
			if( CPedPopulation::GetScenarioPointInArea( m_vNewPointPos, 1.0f, 1, (s32*)&m_iScenarioIndex, &pNewScenarioPoint, true, pPed, false ) )
			{
				Assert(pNewScenarioPoint);
				m_vNewPointPos = VEC3V_TO_VECTOR3(pNewScenarioPoint->GetPosition());
                SetScenarioPoint(pNewScenarioPoint);
				SetState(State_MovingToNewPoint);
				return FSM_Continue;
			}
		}

		// No path found, the ped stays where it is
		SetState(State_Stationary);
	}
	else if(ret == ERequest_NotFound)
	{
		m_hPathHandle = PATH_HANDLE_NULL;
		// Keep the scenario point across this transition
		SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
		SetState(State_Stationary);
		return FSM_Continue;
	}

	return FSM_Continue;
}

void CTaskMoveBetweenPointsScenario::MovingToNewPoint_OnEnter(CPed* UNUSED_PARAM(pPed))
{ 
	bool ignoreScenarioPointDirection = false;
	bool ignoreNavMesh = false;

	const CScenarioInfo* pScenarioInfo = CScenarioManager::GetScenarioInfo(GetScenarioType());
	if ( pScenarioInfo )
	{
		ignoreScenarioPointDirection = pScenarioInfo->GetIsFlagSet(CScenarioInfoFlags::IgnoreScenarioPointHeading);
		ignoreNavMesh = pScenarioInfo->GetIsFlagSet(CScenarioInfoFlags::IgnoreNavMesh);
	}

	const CConditionalAnimsGroup * pClips = CScenarioManager::GetConditionalAnimsGroupForScenario(GetScenarioType());
	CTaskSequenceList* pSequence = rage_new CTaskSequenceList();
	if ( !ignoreNavMesh )
	{
		pSequence->AddTask(rage_new CTaskComplexControlMovement( rage_new CTaskMoveFollowNavMesh( MOVEBLENDRATIO_WALK, m_vNewPointPos ), rage_new CTaskAmbientClips(GetAmbientFlags(), pClips) ));
	}
	else
	{
		// Don't use navmesh, just goto point (added for things like fish)
		pSequence->AddTask(rage_new CTaskComplexControlMovement( rage_new CTaskMoveGoToPoint( MOVEBLENDRATIO_WALK, m_vNewPointPos ), rage_new CTaskAmbientClips(GetAmbientFlags(), pClips) ));
	}

	Assert(GetScenarioPoint());

	if ( !ignoreScenarioPointDirection )
	{
		const Vector3 vDir = VEC3V_TO_VECTOR3(GetScenarioPoint()->GetDirection());
		float	fEffectHeading = rage::Atan2f(-vDir.x, vDir.y);
		pSequence->AddTask(rage_new CTaskComplexControlMovement( rage_new CTaskMoveAchieveHeading( fEffectHeading ), rage_new CTaskAmbientClips(GetAmbientFlags(), pClips)));
	}

	CTaskUseSequence* pTask = rage_new CTaskUseSequence(*pSequence);
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskMoveBetweenPointsScenario::MovingToNewPoint_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	return FSM_Continue;
}

CTask::FSM_Return CTaskMoveBetweenPointsScenario::Finish_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	return FSM_Quit;
}

CTask::FSM_Return CTaskMoveBetweenPointsScenario::ProcessPreFSM()
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	pPed->GetPedIntelligence()->SetCheckShockFlag(true);
	if( !ValidateScenarioInfo() )
	{
		return FSM_Quit;
	}
	return FSM_Continue;
}

#if !__FINAL
const char * CTaskMoveBetweenPointsScenario::GetStaticStateName( s32 iState  )
{
	taskFatalAssertf(iState >=0 && iState <= 3, "State outside range!");

	const char* aStateNames[] = 
	{
		"State_Stationary",
		"State_SearchingForNewPoint",
		"State_MovingToNewPoint",
		"State_Finish",
	};	

	return aStateNames[iState];
}
#endif

u32 CTaskMoveBetweenPointsScenario::GetAmbientFlags()
{
	// Base class
	u32 iFlags = CTaskScenario::GetAmbientFlags();
	iFlags |= CTaskAmbientClips::Flag_PlayIdleClips;
	return iFlags;
}
