// FILE :    TaskCharge.h
//
// AUTHORS : Deryck Morales
//

// File header
#include "Task/Combat/Subtasks/TaskCharge.h"

// Game headers
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "task/Combat/TaskNewCombat.h"
#include "task/Movement/TaskNavMesh.h"

AI_OPTIMISATIONS()

//=========================================================================
// CTaskCharge
//=========================================================================

CTaskCharge::CTaskCharge(Vec3V_In vChargeGoalPos, Vec3V_In vTargetInitialPos, float fCompletionRadius)
: m_vChargeGoalPosition(vChargeGoalPos)
, m_vTargetInitialPosition(vTargetInitialPos)
, m_pSubTaskToUseDuringMovement(NULL)
, m_fCompletionRadius(fCompletionRadius)
, m_bPrev_CanShootWithoutLOS(false)
{
	SetInternalTaskType(CTaskTypes::TASK_CHARGE);
}

CTaskCharge::~CTaskCharge()
{
	//Clear the sub-task to use during movement.
	ClearSubTaskToUseDuringMovement();
}

void CTaskCharge::SetSubTaskToUseDuringMovement(CTask* pTask)
{
	//Clear the sub-task to use during movement.
	ClearSubTaskToUseDuringMovement();

	//Set the sub-task to use during movement.
	m_pSubTaskToUseDuringMovement = pTask;
}

#if !__FINAL
void CTaskCharge::Debug() const
{
#if DEBUG_DRAW
	// any debug drawing specific to this method goes here
#endif // DEBUG_DRAW

	if( GetSubTask() )
	{
		GetSubTask()->Debug();
	}

	CTask::Debug();
}

const char* CTaskCharge::GetStaticStateName(s32 iState)
{
	Assert(iState >= State_Start && iState <= State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_Charge",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif // !__FINAL

aiTask* CTaskCharge::Copy() const
{
	//Create the task.
	CTaskCharge* pTask = rage_new CTaskCharge(m_vChargeGoalPosition, m_vTargetInitialPosition, m_fCompletionRadius);

	//Set the sub-task to use during movement.
	pTask->SetSubTaskToUseDuringMovement(CreateSubTaskToUseDuringMovement());

	return pTask;
}

CTask::FSM_Return CTaskCharge::ProcessPreFSM()
{
	return FSM_Continue;
}

CTask::FSM_Return CTaskCharge::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
	
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_Charge)
			FSM_OnEnter
				Charge_OnEnter();
			FSM_OnUpdate
				return Charge_OnUpdate();
			FSM_OnExit
				Charge_OnExit();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskCharge::Start_OnUpdate()
{
	// Charge!
	SetState(State_Charge);

	return FSM_Continue;
}


void CTaskCharge::Charge_OnEnter()
{
	//Grab the ped.
	CPed* pPed = GetPed();

	// Clear ped crouching
	pPed->SetIsCrouching(false);

	// store existing shoot without LOS setting
	if( pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanShootWithoutLOS) )
	{
		m_bPrev_CanShootWithoutLOS = true;
	}
	// randomly decide
	if( fwRandom::GetRandomTrueFalse() )
	{
		// Set the ped to shoot without LOS for charge
		pPed->GetPedIntelligence()->GetCombatBehaviour().SetFlag(CCombatData::BF_CanShootWithoutLOS);
	}
	
	// Set up navmesh task parameters
	CNavParams navParams;
	navParams.m_pInfluenceSphereCallbackFn = CTaskCombat::SpheresOfInfluenceBuilder;
	navParams.m_vTarget = VEC3V_TO_VECTOR3(m_vChargeGoalPosition);
	navParams.m_fMoveBlendRatio = MOVEBLENDRATIO_RUN;
	navParams.m_bFleeFromTarget = false;
	navParams.m_fCompletionRadius = m_fCompletionRadius;
	navParams.m_fTargetRadius = m_fCompletionRadius;
	
	// Create the navmesh movement task
	CTaskMoveFollowNavMesh* pNavmeshTask = rage_new CTaskMoveFollowNavMesh(navParams);
	pNavmeshTask->SetNeverEnterWater(true);
	pNavmeshTask->SetQuitTaskIfRouteNotFound(true);

	//Get the sub-task.
	CTask* pSubTask = CreateSubTaskToUseDuringMovement();

	// Set the new subtask
	SetNewTask(rage_new CTaskComplexControlMovement( pNavmeshTask, pSubTask) );

	// Test for nearby friendlies to possibly play dialogue
	static dev_s32 siMaxNumPedsToTest = 8;
	int iTestedPeds = 0;
	const CEntityScannerIterator entityList = pPed->GetPedIntelligence()->GetNearbyPeds();
	for(const CEntity* pEnt = entityList.GetFirst(); pEnt && iTestedPeds < siMaxNumPedsToTest; pEnt = entityList.GetNext(), iTestedPeds++)
	{
		const CPed * pNearbyPed = static_cast<const CPed*>(pEnt);
		if( !pNearbyPed->IsDead() && pNearbyPed->GetPedIntelligence()->IsFriendlyWith(*pPed) )
		{
			float fRandom = fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
			if(fRandom <= 0.33f)
			{
				pPed->NewSay("COVER_ME");
			}
			else if(fRandom <= 0.66f)
			{
				pPed->NewSay("GET_HIM");
			}
			else
			{
				pPed->NewSay("MOVE_IN");
			}

			break;
		}
	}
}

CTask::FSM_Return CTaskCharge::Charge_OnUpdate()
{
	//Check if the sub-task has finished.
	if( GetIsSubtaskFinished(CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT) )
	{
		//Finish the task.
		SetState(State_Finish);
	}

	return FSM_Continue;
}

void CTaskCharge::Charge_OnExit()
{
	//Grab the ped.
	CPed* pPed = GetPed();

	// unless previous setting was true
	if( !m_bPrev_CanShootWithoutLOS )
	{
		// clear the shoot without LOS flag
		pPed->GetPedIntelligence()->GetCombatBehaviour().ClearFlag(CCombatData::BF_CanShootWithoutLOS);
	}
}

void CTaskCharge::ClearSubTaskToUseDuringMovement()
{
	//Free the task.
	delete m_pSubTaskToUseDuringMovement;

	//Clear the task handle
	m_pSubTaskToUseDuringMovement = NULL;
}

CTask* CTaskCharge::CreateSubTaskToUseDuringMovement() const
{
	//Ensure the task is valid.
	if(!m_pSubTaskToUseDuringMovement)
	{
		return NULL;
	}

	return static_cast<CTask *>(m_pSubTaskToUseDuringMovement->Copy());
}
