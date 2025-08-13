#include "TaskWanderingInRadiusScenario.h"

#include "Peds/ped.h"
#include "Peds/PedIntelligence.h"

#include "Task/Default/TaskAmbient.h"
#include "Task/Default/TaskWander.h"

#include "Task/Scenario/info/ScenarioInfo.h"

AI_OPTIMISATIONS()

//-------------------------------------------------------------------------
// Radius based wandering scenario task
//-------------------------------------------------------------------------
CTaskWanderingInRadiusScenario::CTaskWanderingInRadiusScenario( s32 scenarioType, CScenarioPoint* pScenarioPoint )
: CTaskScenario(scenarioType, pScenarioPoint)
{
	SetInternalTaskType(CTaskTypes::TASK_WANDERING_IN_RADIUS_SCENARIO);
}

CTaskWanderingInRadiusScenario::~CTaskWanderingInRadiusScenario()
{
}

aiTask* CTaskWanderingInRadiusScenario::Copy() const
{
	return rage_new CTaskWanderingInRadiusScenario( m_iScenarioIndex, GetScenarioPoint() );
}

CTask::FSM_Return CTaskWanderingInRadiusScenario::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		FSM_State(State_Wandering)
		FSM_OnEnter
		float fMoveBlendRatio = MOVEBLENDRATIO_WALK;
	const CScenarioInfo* pScenarioInfo = CScenarioManager::GetScenarioInfo(m_iScenarioIndex);
	Assertf(pScenarioInfo && pScenarioInfo->GetIsClass<CScenarioWanderingInRadiusInfo>(), "Scenario info does not exist or is not a wander in radius scenario");
	float fWanderRadius = static_cast<const CScenarioWanderingInRadiusInfo*>(pScenarioInfo)->GetWanderRadius();

	CTaskWanderInArea* pSubTask = rage_new CTaskWanderInArea( fMoveBlendRatio, fWanderRadius, VEC3V_TO_VECTOR3(GetScenarioPoint()->GetPosition()) );
	SetNewTask( pSubTask );
	FSM_OnUpdate
		pPed->GetPedIntelligence()->SetCheckShockFlag(true);
	if( GetIsFlagSet(aiTaskFlags::SubTaskFinished) )
		return FSM_Quit;
	FSM_State(State_Finish)
		FSM_OnUpdate
		return FSM_Quit;
	FSM_End
}

#if !__FINAL
const char * CTaskWanderingInRadiusScenario::GetStaticStateName( s32 iState  )
{
	const char* aStateNames[] = 
	{
		"State_Wandering",
		"State_Finish",
	};	

	return aStateNames[iState];
}
#endif // !__FINAL

u32 CTaskWanderingInRadiusScenario::GetAmbientFlags()
{
	// Base class
	u32 iFlags = CTaskScenario::GetAmbientFlags();
	iFlags |= CTaskAmbientClips::Flag_PlayIdleClips;
	return iFlags;
}
