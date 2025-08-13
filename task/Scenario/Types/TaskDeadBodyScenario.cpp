// Class Header
#include "TaskDeadBodyScenario.h"

// Game Headers
#include "audio/speechaudioentity.h"
#include "Event/EventDamage.h"
#include "Peds/ped.h"
#include "Peds/PedIntelligence.h"
#include "Task/Combat/TaskDamageDeath.h"
#include "Task/Scenario/info/ScenarioInfo.h"

AI_OPTIMISATIONS()

//-------------------------------------------------------------------------
// Spawn a dead ped at a point.
//-------------------------------------------------------------------------
CTaskDeadBodyScenario::CTaskDeadBodyScenario( s32 scenarioType, CScenarioPoint* pScenarioPoint )
: CTaskScenario(scenarioType, pScenarioPoint)
, m_bHasSentKillEvent(false)
{
	SetInternalTaskType(CTaskTypes::TASK_DEAD_BODY_SCENARIO);
}

CTaskDeadBodyScenario::~CTaskDeadBodyScenario()
{

}

aiTask* CTaskDeadBodyScenario::Copy() const
{
	return rage_new CTaskDeadBodyScenario(m_iScenarioIndex, GetScenarioPoint());
}

CTask::FSM_Return CTaskDeadBodyScenario::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	FSM_Begin

		FSM_State(State_Setup)
			FSM_OnEnter
				Setup_OnEnter();
			FSM_OnUpdate
				Setup_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

// Kill the ped in a special way that generates no sound, etc.
void CTaskDeadBodyScenario::Setup_OnEnter()
{
	// Grab the ped.
	CPed* pPed = GetPed();

	// Force ragdoll so that the NM system can place this dead body on the ground properly.
	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_ForceRagdollUponDeath, true);

	// Don't let them scream / make any noise.
	audSpeechAudioEntity* pSpeech = pPed->GetSpeechAudioEntity();
	if (pSpeech)
	{
		pSpeech->DisableSpeaking(true);
	}
}

// Wait for death to come - this task will be interrupted by the death event.
CTask::FSM_Return CTaskDeadBodyScenario::Setup_OnUpdate()
{
	static float timeTillKill = 0.25f;
	//Delay so task unalerted will setup everything to idle state ... 
	if (!m_bHasSentKillEvent && GetTimeInState() > timeTillKill)
	{
		// Grab the ped and kill it ... 
		CPed* pPed = GetPed();
		CEventDeath death(false, fwTimer::GetTimeInMilliseconds(), true);
		pPed->GetPedIntelligence()->AddEvent(death, false, true);
		m_bHasSentKillEvent = true;
	}

	return FSM_Continue;
}

#if !__FINAL
const char* CTaskDeadBodyScenario::GetStaticStateName(s32 iState)
{
	taskAssert(iState >= State_Setup && iState <= State_Finish);

	switch (iState)
	{		
		case State_Setup:
		{
			return "State_Setup";
		}
		case State_Finish:
		{
			return "State_Finish";
		}
		default:
		{
			taskAssert(0); 	
			return "INVALID";
		}
	}
}
#endif // !__FINAL
