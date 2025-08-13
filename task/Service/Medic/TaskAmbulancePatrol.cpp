// FILE :    TaskAmbulancePatrol.cpp
// PURPOSE : Basic patrolling task of a medic, patrol in vehicle or on foot until given specific orders by dispatch
// CREATED : 25-05-2009

// File header
#include "Task/Service/Medic/TaskAmbulancePatrol.h"

// Framework headers
#include "ai/task/taskchannel.h"

// Game headers
#include "Game/Dispatch/Incidents.h"
#include "Game/Dispatch/Orders.h"
#include "Peds/Ped.h"
#include "Peds/PedGeometryAnalyser.h"
#include "Peds/PedIntelligence.h"
#include "Peds/HealthConfig.h"
#include "Physics/WorldProbe/worldprobe.h"
#include "Task/Combat/TaskDamageDeath.h"
#include "Task/Default/TaskUnalerted.h"
#include "Task/Default/TaskWander.h"
#include "Task/Movement/TaskGoToPoint.h"
#include "Task/Movement/TaskNavMesh.h"
#include "Task/Scenario/Types/TaskUseScenario.h"
#include "Task/Scenario/Info/ScenarioInfoManager.h"
#include "Task/Vehicle/TaskCar.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Task/Vehicle/TaskExitVehicle.h"
#include "Vehicles/Automobile.h"
#include "VehicleAI/driverpersonality.h"
#include "VehicleAI/task/TaskVehicleGoToAutomobile.h"
#include "VehicleAI/task/TaskVehicleCruise.h"
#include "VehicleAi/task/TaskVehiclePark.h"
#include "Vfx/Misc/Fire.h"

AI_OPTIMISATIONS()

const float CTaskAmbulancePatrol::s_fDriverWalkDistance = 5.0f;
const float CTaskAmbulancePatrol::s_fHealerWalkDistance = 5.0f;

//----------
RAGE_DECLARE_CHANNEL(taskambulancepatrol)
RAGE_DEFINE_CHANNEL(taskambulancepatrol)

#define tapAssert(cond)					RAGE_ASSERT(taskambulancepatrol,cond)
#define tapAssertf(cond,fmt,...)		RAGE_ASSERTF(taskambulancepatrol,cond,fmt,##__VA_ARGS__)
#define tapFatalAssertf(cond,fmt,...)	RAGE_FATALASSERTF(taskambulancepatrol,cond,fmt,##__VA_ARGS__)
#define tapVerifyf(cond,fmt,...)		RAGE_VERIFYF(taskambulancepatrol,cond,fmt,##__VA_ARGS__)
#define tapFatalf(fmt,...)				RAGE_FATALF(taskambulancepatrol,fmt,##__VA_ARGS__)
#define tapErrorf(fmt,...)				RAGE_ERRORF(taskambulancepatrol,fmt,##__VA_ARGS__)
#define tapWarningf(fmt,...)			RAGE_WARNINGF(taskambulancepatrol,fmt,##__VA_ARGS__)
#define tapDisplayf(fmt,...)			RAGE_DISPLAYF(taskambulancepatrol,fmt,##__VA_ARGS__)
#define tapDebugf1(fmt,...)				RAGE_DEBUGF1(taskambulancepatrol,fmt,##__VA_ARGS__)
#define tapDebugf2(fmt,...)				RAGE_DEBUGF2(taskambulancepatrol,fmt,##__VA_ARGS__)
#define tapDebugf3(fmt,...)				RAGE_DEBUGF3(taskambulancepatrol,fmt,##__VA_ARGS__)
//----------

PARAM(debugtaskambulancepatrol, "Enable visual debugging for task ambulance patrol");

//----------

CTaskAmbulancePatrol::CTaskAmbulancePatrol(CTaskUnalerted* pUnalertedTask)
: m_pUnalertedTask(pUnalertedTask)
, m_pPointToReturnTo(NULL)
, m_iPointToReturnToRealTypeIndex(-1)
, m_SPC_UseInfo(NULL)
{
	tapDebugf3("CTaskAmbulancePatrol::CTaskAmbulancePatrol");

	m_iNextTimeToCheckForNewReviver = fwTimer::GetTimeInMilliseconds();
	m_iTimeToLeaveBody = 0;
	m_bClipStarted = false;
	m_bShouldApproachInjuredPed = false;
	m_bAllowToUseTurnedOffNodes = false;
	m_bArriveAudioTriggered = false;
	m_iNavMeshBlockingBounds[0] = m_iNavMeshBlockingBounds[1] = DYNAMIC_OBJECT_INDEX_NONE;
	m_vBodyPosition.Zero();
	SetInternalTaskType(CTaskTypes::TASK_AMBULANCE_PATROL);
}

CTaskAmbulancePatrol::~CTaskAmbulancePatrol()
{
	tapDebugf3("CTaskAmbulancePatrol::~CTaskAmbulancePatrol");

	//MarkPedAsFinishedWithOrder();
	m_clipHelper.ReleaseClips();

	// In case any blocking bound was not removed, ensure to remove it here
	CPathServerGta::RemoveBlockingObject(m_iNavMeshBlockingBounds[0]);
	CPathServerGta::RemoveBlockingObject(m_iNavMeshBlockingBounds[1]);
	m_iNavMeshBlockingBounds[0] = m_iNavMeshBlockingBounds[1] = DYNAMIC_OBJECT_INDEX_NONE;	
}

CTask::FSM_Return CTaskAmbulancePatrol::ProcessPreFSM()
{	
	//Make sure the ped doesn't get culled
	CPed* pPed = GetPed();

	if (pPed)
	{
		//Make sure the ped doesn't get culled
		pPed->SetPedResetFlag( CPED_RESET_FLAG_CullExtraFarAway, true );
		pPed->SetPedResetFlag( CPED_RESET_FLAG_TaskCullExtraFarAway, true );

		//Make sure the victim doesn't get culled
		const CAmbulanceOrder* pOrder = HelperGetAmbulanceOrder();
		CPed* pVictim = pOrder ? static_cast<CPed*>(pOrder->GetIncident()->GetEntity()) : NULL;


		// Check shocking events
		if( pOrder )
			pPed->GetPedIntelligence()->SetCheckShockFlag(true);

		if(pVictim)
		{
			pVictim->SetPedResetFlag( CPED_RESET_FLAG_CullExtraFarAway, true );
			pVictim->SetPedResetFlag( CPED_RESET_FLAG_TaskCullExtraFarAway, true );
		}
	}

	return FSM_Continue;
}

bool CTaskAmbulancePatrol::ProcessMoveSignals()
{
	CPed* pPed = GetPed();
	if (pPed)
	{
		CVehicle* pVehicle = pPed->GetMyVehicle();
		if (pVehicle)
		{
			pVehicle->m_nVehicleFlags.bDisablePretendOccupants = true;
			pVehicle->m_nVehicleFlags.bForceOtherVehiclesToStopForThisVehicle = true;
			pVehicle->m_nVehicleFlags.bPreventBeingDummyUnlessCollisionLoadedThisFrame = true;
		}
	}

	return true;
}

void CTaskAmbulancePatrol::DebugAmbulancePosition()
{
#if __BANK
	if(CDispatchManager::GetInstance().DebugEnabled(DT_AmbulanceDepartment) || PARAM_debugtaskambulancepatrol.Get())
	{
		CPed* pPed = GetPed();
		if (pPed)
		{
			CVehicle* pVehicle = pPed->GetMyVehicle();
			if (pVehicle)
			{
				CPed* pLocalPlayerPed = CPedFactory::GetFactory()->GetLocalPlayer();
				if (pLocalPlayerPed)
				{
					Color32 lineColor = !pVehicle->IsNetworkClone() ? Color_green : Color_orange;
					grcDebugDraw::Line(VEC3V_TO_VECTOR3(pLocalPlayerPed->GetTransform().GetPosition()), VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition()), lineColor, lineColor);
				}
			}
		}
	}
#endif
}

CTask::FSM_Return CTaskAmbulancePatrol::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
#if __BANK
	DebugAmbulancePosition();
#endif

	FSM_Begin
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_Resume)
			FSM_OnUpdate
				return Resume_OnUpdate();

		FSM_State(State_Unalerted)
			FSM_OnEnter
				Unalerted_OnEnter();
			FSM_OnUpdate
				return Unalerted_OnUpdate();

		FSM_State(State_ChoosePatrol)
			FSM_OnUpdate
				return ChoosePatrol_OnUpdate();

		FSM_State(State_RespondToOrder)
			FSM_OnUpdate
				return RespondToOrder_OnUpdate();

		FSM_State(State_PatrolOnFoot)
			FSM_OnEnter
				PatrolOnFoot_OnEnter();
			FSM_OnUpdate
				return PatrolOnFoot_OnUpdate();

		FSM_State(State_PatrolInVehicle)
			FSM_OnEnter
				PatrolInVehicle_OnEnter();
			FSM_OnUpdate
				return PatrolInVehicle_OnUpdate();

		FSM_State(State_PatrolAsPassenger)
			FSM_OnEnter
				PatrolAsPassenger_OnEnter();
			FSM_OnUpdate
				return PatrolAsPassenger_OnUpdate();

		FSM_State(State_GoToIncidentLocationInVehicle)
			FSM_OnEnter
				GoToIncidentLocationInVehicle_OnEnter();
			FSM_OnUpdate
				return GoToIncidentLocationInVehicle_OnUpdate();
			FSM_OnExit
				GoToIncidentLocationInVehicle_OnExit();

		FSM_State(State_StopVehicle)
			FSM_OnEnter
				StopVehicle_OnEnter();
			FSM_OnUpdate
				return StopVehicle_OnUpdate();

		FSM_State(State_GoToIncidentLocationAsPassenger)
			FSM_OnEnter
				GoToIncidentLocationAsPassenger_OnEnter();
			FSM_OnUpdate
				return GoToIncidentLocationAsPassenger_OnUpdate();

		FSM_State(State_GoToIncidentLocationOnFoot)
			FSM_OnEnter
				GoToIncidentLocationOnFoot_OnEnter();
			FSM_OnUpdate
				return GoToIncidentLocationOnFoot_OnUpdate();

		FSM_State(State_PullFromVehicle)
			FSM_OnEnter
				PullFromVehicle_OnEnter();
			FSM_OnUpdate
				return PullFromVehicle_OnUpdate();

		FSM_State(State_ExitVehicleAndChooseResponse)
			FSM_OnEnter
				ExitVehicleAndChooseResponse_OnEnter();
			FSM_OnUpdate
				return ExitVehicleAndChooseResponse_OnUpdate();

		FSM_State(State_HealVictim)
			FSM_OnEnter
				HealVictim_OnEnter();
		FSM_OnUpdate
				return HealVictim_OnUpdate();

		FSM_State(State_PlayRecoverySequence)
			FSM_OnEnter
				PlayRecoverySequence_OnEnter();
			FSM_OnUpdate
				return PlayRecoverySequence_OnUpdate();
			FSM_OnExit
				PlayRecoverySequence_OnExit();

		FSM_State(State_ReturnToAmbulance)
			FSM_OnEnter
				ReturnToAmbulance_OnEnter();
			FSM_OnUpdate
				return ReturnToAmbulance_OnUpdate();

		FSM_State(State_SuperviseRecovery)
			FSM_OnEnter
				SuperviseRecovery_OnEnter();
			FSM_OnUpdate
				return SuperviseRecovery_OnUpdate();
			FSM_OnExit
				SuperviseRecovery_OnExit();

		FSM_State(State_TransportVictim)
			FSM_OnEnter
				TransportVictim_OnEnter();
			FSM_OnUpdate
				return TransportVictim_OnUpdate();

		FSM_State(State_TransportVictimAsPassenger)
			FSM_OnEnter
				TransportVictimAsPassenger_OnEnter();
			FSM_OnUpdate
				return TransportVictimAsPassenger_OnUpdate();

		FSM_State(State_ReviveFailed)
			FSM_OnEnter
				ReviveFailed_OnEnter();
			FSM_OnUpdate
				return ReviveFailed_OnUpdate();

		FSM_State(State_WaitForBothMedicsToFinish)
			FSM_OnEnter
				WaitForBothMedicsToFinish_OnEnter();
			FSM_OnUpdate
				return WaitForBothMedicsToFinish_OnUpdate();

		FSM_State(State_WaitInAmbulance)
			FSM_OnEnter
				WaitInAmbulance_OnEnter();
			FSM_OnUpdate
				return WaitInAmbulance_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

#if !__FINAL
const char * CTaskAmbulancePatrol::GetStaticStateName( s32 iState  )
{
	taskAssert(iState >= State_Start && iState <= State_Finish);

	switch (iState)
	{
	case State_Start:							return "State_Start";
	case State_Resume:							return "State_Resume";
	case State_Unalerted:						return "State_Unalerted";
	case State_ChoosePatrol:					return "State_ChoosePatrol";
	case State_RespondToOrder:					return "State_RespondToOrder";
	case State_PatrolInVehicle:					return "State_PatrolInVehicle";
	case State_PatrolOnFoot:					return "State_PatrolOnFoot";
	case State_PatrolAsPassenger:				return "State_PatrolAsPassenger";
	case State_GoToIncidentLocationOnFoot:		return "State_GoToIncidentLocationOnFoot";
	case State_GoToIncidentLocationInVehicle:	return "State_GoToIncidentLocationInVehicle";
	case State_StopVehicle:						return "State_StopVehicle";
	case State_GoToIncidentLocationAsPassenger:	return "State_GoToIncidentLocationAsPassenger";
	case State_PullFromVehicle:					return "State_PullFromVehicle";
	case State_ExitVehicleAndChooseResponse:	return "State_ExitVehicleAndChooseResponse";
	case State_HealVictim:						return "State_HealVictim";
	case State_PlayRecoverySequence:			return "State_PlayRecoverySequence";
	case State_ReturnToAmbulance:				return "State_ReturnToAmbulance";
	case State_SuperviseRecovery:				return "State_SuperviseRecovery";
	case State_TransportVictim:					return "State_TransportVictim";
	case State_TransportVictimAsPassenger:		return "State_TransportVictimAsPassenger";
	case State_ReviveFailed:					return "State_ReviveFailed";
	case State_WaitForBothMedicsToFinish:		return "State_WaitForBothMedicsToFinish";
	case State_WaitInAmbulance:					return "State_WaitInAmbulance";
	case State_Finish:							return "State_Finish";
	default: taskAssert(0); 	
	}

	return "State_Invalid";
}
#endif // !__FINAL

void CTaskAmbulancePatrol::CleanUp()
{
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);	

	CPed* pPed = GetPed();
	if( pPed )
	{ 
		pPed->SetDefaultDecisionMaker();
	}

	pPed->GetIkManager().AbortLookAt(500);
}

CScenarioPoint* CTaskAmbulancePatrol::GetScenarioPoint() const
{
	// Check the unalerted subtask.
	const CTask* pSubTask = GetSubTask();
	if(pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_UNALERTED)
	{
		return pSubTask->GetScenarioPoint();
	}
	else
	{
		// Check the cached unalerted task.
		const CTask* pUnalertedTask = m_pUnalertedTask.Get();
		if(pUnalertedTask)
		{
			return pUnalertedTask->GetScenarioPoint();
		}
	}

	// No unalerted task, cached or otherwise.
	return NULL;
}


int CTaskAmbulancePatrol::GetScenarioPointType() const
{
	int type = -1;
	// Check the unalerted subtask.
	const CTask* pSubTask = GetSubTask();
	if(pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_UNALERTED)
	{
		type = pSubTask->GetScenarioPointType();
	}
	else
	{
		// Check the cached unalerted task.
		const CTask* pUnalertedTask = m_pUnalertedTask.Get();
		if(pUnalertedTask)
		{
			type = pUnalertedTask->GetScenarioPointType();
		}
	}

	return type;
}


void CTaskAmbulancePatrol::MarkPedAsFinishedWithOrder()
{
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);	

	if (!GetEntity() || !GetPed())
	{
		return;
	}

	COrder* pOrder = GetPed()->GetPedIntelligence()->GetOrder();
	if( pOrder && pOrder->GetIsValid() )
	{
		pOrder->SetEntityFinishedWithIncident();
	}

	//aiAssertf(0, "Tried unsetting reponse in progress on an invalid order--this shouldn't happen!");
}

CTask::FSM_Return CTaskAmbulancePatrol::Start_OnUpdate()
{
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);	

	m_bShouldApproachInjuredPed = false;
	CPed* pPed = GetPed();

	RequestProcessMoveSignalCalls();

	if( pPed )
	{ 
		const u32 MEDIC_DECISION_MAKER = ATSTRINGHASH("MEDIC", 0x0b0423aa0);
		pPed->GetPedIntelligence()->SetDecisionMakerId(MEDIC_DECISION_MAKER);
	}

	if (!m_pUnalertedTask)
	{
		// If there is no unalerted task, the ped may still have a scenario point reserved.
		// We can build an unalerted task from this stored point.
		AttemptToCreateResumableUnalertedTask();
	}

	if(m_pUnalertedTask)
	{
		SetState(State_Unalerted);
	}
	else
	{
		const COrder* pOrder = GetPed()->GetPedIntelligence()->GetOrder();
		if( pOrder && pOrder->GetIsValid() )
		{
			CIncident* pIncident = pOrder->GetIncident();
			CInjuryIncident* pInjuryIncident = pIncident && pIncident->GetType() == CIncident::IT_Injury 
				? static_cast<CInjuryIncident*>(pOrder->GetIncident())
				: NULL;
			if (pIncident && pIncident->GetHasBeenAddressed())
			{
				if ( GetPed()->GetIsInVehicle())
				{
					tapDebugf3("CTaskAmbulancePatrol::Start_OnUpdate-->SetState(State_WaitInAmbulance)");
					SetState(State_WaitInAmbulance);
					return FSM_Continue;
				}
				else if ((GetPed()->GetMyVehicle() && GetPed()->GetMyVehicle()->IsInDriveableState())
					|| (pInjuryIncident && pInjuryIncident->GetEmergencyVehicle() && pInjuryIncident->GetEmergencyVehicle()->IsInDriveableState()))
				{
					tapDebugf3("CTaskAmbulancePatrol::Start_OnUpdate-->SetState(State_ReturnToAmbulance) 1");
					SetState(State_ReturnToAmbulance);
					return FSM_Continue;
				}
				else
				{
					SetState(State_ChoosePatrol);
					return FSM_Continue;
				}
			}
			else if (pIncident)
			{
				tapDebugf3("CTaskAmbulancePatrol::Start_OnUpdate--pIncident-->SetState(State_RespondToOrder)");
				SetState(State_RespondToOrder);
				return FSM_Continue;
			}
			else
			{
				SetState(State_ChoosePatrol);
				return FSM_Continue;
			}
		}
		else
		{
			if(!GetPed()->GetVehiclePedInside() && GetPed()->GetMyVehicle() && GetPed()->GetMyVehicle()->IsInDriveableState())
			{
				tapDebugf3("CTaskAmbulancePatrol::Start_OnUpdate-->SetState(State_ReturnToAmbulance) 2");
				SetState(State_ReturnToAmbulance);
				return FSM_Continue;
			}
			
			SetState(State_ChoosePatrol);
			return FSM_Continue;
		}
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskAmbulancePatrol::Resume_OnUpdate()
{
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);	

	s32 iState = State_Start;

	//Check if the sub-task is valid.
	const CTask* pSubTask = GetSubTask();
	if(pSubTask)
	{
		//Check the task type.
		switch(pSubTask->GetTaskType())
		{
			case CTaskTypes::TASK_UNALERTED:
			{
				iState = State_Unalerted;
				SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
				break;
			}
			default:
			{
				break;
			}
		}
	}

	SetState(iState);

	return FSM_Continue;
}

void CTaskAmbulancePatrol::Unalerted_OnEnter()
{
	//Check if we are resuming the state.  Keep the sub-task, if possible.
	if(GetSubTask() && (GetSubTask()->GetTaskType() == CTaskTypes::TASK_UNALERTED))
	{
		return;
	}

	//MP Fallback
	if (NetworkInterface::IsGameInProgress() && !m_pUnalertedTask)
	{
		// If there is no unalerted task, the ped may still have a scenario point reserved.
		// We can build an unalerted task from this stored point.
		AttemptToCreateResumableUnalertedTask();
	}

	//Ensure the task is valid.
	CTask* pUnalertedTask = m_pUnalertedTask;
	if(!taskVerifyf(pUnalertedTask, "The unalerted task is invalid."))
	{
		return;
	}

	//Clear the task.
	m_pUnalertedTask = NULL;

	//Start the task.
	SetNewTask(pUnalertedTask);
}

CTask::FSM_Return CTaskAmbulancePatrol::Unalerted_OnUpdate()
{
	//Check if we have a valid order.
	const COrder* pOrder = GetPed()->GetPedIntelligence()->GetOrder();
	if(pOrder && pOrder->GetIsValid())
	{
		//Cache the scenario point to return to.
		CacheScenarioPointToReturnTo();

		tapDebugf3("CTaskAmbulancePatrol::Unalerted_OnUpdate-->SetState(State_RespondToOrder)");
		SetState(State_RespondToOrder);
	}
	//Check if the sub-task has finished.
	else if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Move to the start state.
		SetState(State_Start);
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskAmbulancePatrol::ChoosePatrol_OnUpdate()
{
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);	

	//Make sure this ped has been marked as finishing with the order
	MarkPedAsFinishedWithOrder();

	if( GetPed()->GetIsInVehicle() )
	{
		if (GetPed()->GetVehiclePedInside() && GetPed()->GetVehiclePedInside()->HasAliveVictimInIt())
		{
			if( GetPed()->GetIsDrivingVehicle() )
			{
				tapDebugf3("CTaskAmbulancePatrol::ChoosePatrol_OnUpdate-->SetState(State_TransportVictim)");
				SetState(State_TransportVictim);
			}
			else
			{
				tapDebugf3("CTaskAmbulancePatrol::ChoosePatrol_OnUpdate-->SetState(State_TransportVictimAsPassenger)");
				SetState(State_TransportVictimAsPassenger);
			}
		}
		else
		{
			if( GetPed()->GetIsDrivingVehicle() )
			{
				tapDebugf3("CTaskAmbulancePatrol::ChoosePatrol_OnUpdate-->SetState(State_PatrolInVehicle)");
				SetState(State_PatrolInVehicle);
			}
			else
			{
				tapDebugf3("CTaskAmbulancePatrol::ChoosePatrol_OnUpdate-->SetState(State_PatrolAsPassenger)");
				SetState(State_PatrolAsPassenger);
			}
		}
	}
	else
	{
		if (!m_pUnalertedTask)
		{
			// If there is no unalerted task, the ped may still have a scenario point reserved.
			// We can build an unalerted task from this stored point.
			AttemptToCreateResumableUnalertedTask();
		}

		if (m_pUnalertedTask)
		{
			tapDebugf3("CTaskAmbulancePatrol::ChoosePatrol_OnUpdate-->SetState(State_Unalerted)");
			SetState(State_Unalerted);
		}
		else
		{
			tapDebugf3("CTaskAmbulancePatrol::ChoosePatrol_OnUpdate-->SetState(State_PatrolOnFoot)");
			SetState(State_PatrolOnFoot);
		}
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskAmbulancePatrol::RespondToOrder_OnUpdate()
{
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);	

	const COrder* pOrder = HelperGetAmbulanceOrder();
	if( pOrder && pOrder->GetIsValid())
	{
		if( GetPed()->GetIsInVehicle() )
		{
			if( GetPed()->GetIsDrivingVehicle() )
			{
				tapDebugf3("CTaskAmbulancePatrol::RespondToOrder_OnUpdate-->SetState(State_GoToIncidentLocationInVehicle)");
				SetState(State_GoToIncidentLocationInVehicle);
			}
			else
			{
				tapDebugf3("CTaskAmbulancePatrol::RespondToOrder_OnUpdate-->SetState(State_GoToIncidentLocationAsPassenger)");
				SetState(State_GoToIncidentLocationAsPassenger);
			}
		}
		else
		{
			tapDebugf3("CTaskAmbulancePatrol::RespondToOrder_OnUpdate-->SetState(State_GoToIncidentLocationOnFoot)");
			SetState(State_GoToIncidentLocationOnFoot);
		}
	}
	else
	{
		tapDebugf3("CTaskAmbulancePatrol::RespondToOrder_OnUpdate-->SetState(State_ChoosePatrol)");
		SetState(State_ChoosePatrol);
	}
	return FSM_Continue;
}

void CTaskAmbulancePatrol::PatrolInVehicle_OnEnter()
{
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);	

	CVehicle* pVehicle = GetPed()->GetMyVehicle();
	if( aiVerifyf(pVehicle, "PatrolInVehicle_OnEnter: Vehicle NULL!") )
	{
		SetNewTask(CTaskUnalerted::SetupDrivingTaskForEmergencyVehicle(*GetPed(), *pVehicle, false, true));
	}
}

CTask::FSM_Return CTaskAmbulancePatrol::PatrolInVehicle_OnUpdate()
{
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);	

	const CAmbulanceOrder* pOrder = HelperGetAmbulanceOrder();
	if( pOrder && pOrder->GetIsValid() && !pOrder->GetIncidentHasBeenAddressed())
	{
		tapDebugf3("CTaskAmbulancePatrol::PatrolInVehicle_OnUpdate-->SetState(State_RespondToOrder)");
		SetState(State_RespondToOrder);
		return FSM_Continue;
	}

	return FSM_Continue;
}

void CTaskAmbulancePatrol::PatrolAsPassenger_OnEnter()
{
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);	

	CVehicle* pVehicle = GetPed()->GetMyVehicle();
	if( aiVerifyf(pVehicle, "PatrolAsPassenger_OnEnter: Vehicle NULL!") )
	{
		pVehicle->TurnSirenOn(false);
		SetNewTask(rage_new CTaskInVehicleBasic(pVehicle));
	}
}

CTask::FSM_Return CTaskAmbulancePatrol::PatrolAsPassenger_OnUpdate()
{
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);	

	CPed* pPed = GetPed();
	const CAmbulanceOrder* pOrder = HelperGetAmbulanceOrder();
	if( pOrder && pOrder->GetIsValid() && !pOrder->GetIncidentHasBeenAddressed())
	{
		tapDebugf3("CTaskAmbulancePatrol::PatrolAsPassenger_OnUpdate-->SetState(State_RespondToOrder)");
		SetState(State_RespondToOrder);
		return FSM_Continue;
	}

	if( pPed )
	{
		pPed->GetPedIntelligence()->SetCheckShockFlag(true);
	}

	return FSM_Continue;
}

void CTaskAmbulancePatrol::PatrolOnFoot_OnEnter()
{
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);	

	SetNewTask(rage_new CTaskWander(MOVEBLENDRATIO_WALK, GetPed()->GetCurrentHeading(), NULL, -1, true, CTaskWander::TARGET_RADIUS, true, true));
}

CTask::FSM_Return CTaskAmbulancePatrol::PatrolOnFoot_OnUpdate()
{
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);	

	const CAmbulanceOrder* pOrder = HelperGetAmbulanceOrder();
	if( pOrder && pOrder->GetIsValid() && !pOrder->GetIncidentHasBeenAddressed())
	{
		tapDebugf3("CTaskAmbulancePatrol::PatrolOnFoot_OnUpdate-->SetState(State_RespondToOrder)");
		SetState(State_RespondToOrder);
		return FSM_Continue;
	}

	return FSM_Continue;
}


void CTaskAmbulancePatrol::GoToIncidentLocationInVehicle_OnEnter()
{
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);	

	//Reuse this timer to not leave the area if there isn't a body for a random time.
	m_iTimeToLeaveBody = fwRandom::GetRandomNumberInRange(3000, 4000);

	CAmbulanceOrder* pOrder = HelperGetAmbulanceOrder();
	if( aiVerifyf(pOrder, "GoToIncidentLocation_OnEnter:: Invalid Order" ) &&
		aiVerifyf(GetPed()->GetIsInVehicle(), "GoToIncidentLocationInVehicle_OnEnter: Not in vehicle!"))
	{
		aiAssert(GetPed()->GetMyVehicle());
		GetPed()->GetMyVehicle()->TurnSirenOn(true);
		GetPed()->GetMyVehicle()->m_nVehicleFlags.bIsAmbulanceOnDuty = true;
		const Vector3 vLocation = pOrder->GetIncident()->GetLocation();

		pOrder->SetEmergencyVehicle(GetPed()->GetMyVehicle());
		pOrder->SetDriver(GetPed());

		Vector3 vBestRoadLocation = vLocation;
		GetBestPointToDriveTo(GetPed(),vBestRoadLocation);

		const bool bFoundGoodNode = vBestRoadLocation.Dist2(vLocation) > SMALL_FLOAT;

		float fArrivalDist = 10.0f;
		sVehicleMissionParams params;
		params.m_iDrivingFlags = DMode_AvoidCars|DF_SteerAroundPeds|DF_ForceJoinInRoadDirection;
		params.m_fCruiseSpeed = 15.0f;
		params.SetTargetPosition(vBestRoadLocation);
		params.m_fTargetArriveDist = fArrivalDist;
		
		const float fStraightLineDistance = bFoundGoodNode ? 1.0f : 15.0f;
		aiTask *pCarTask = CVehicleIntelligence::CreateAutomobileGotoTask(params, fStraightLineDistance);
		SetNewTask(rage_new CTaskControlVehicle(GetPed()->GetMyVehicle(), pCarTask ));

		//Get the nearest road node, If it's off then allow the fire truck to contiune on turned off road nodes.
		CNodeAddress nearestNode = ThePaths.FindNodeClosestToCoors(VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition()),20.0f);

		if (!nearestNode.IsEmpty() && ThePaths.IsRegionLoaded(nearestNode))
		{
			const CPathNode *pCurrentNode = ThePaths.FindNodePointerSafe(nearestNode);
			if(pCurrentNode && pCurrentNode->IsSwitchedOff())
			{
				m_bAllowToUseTurnedOffNodes = true;
			}
		}
	}
}

CTask::FSM_Return CTaskAmbulancePatrol::GoToIncidentLocationInVehicle_OnUpdate()
{
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);	

	CAmbulanceOrder* pOrder = HelperGetAmbulanceOrder();
	if( pOrder ==NULL )
	{
		SetState(State_ChoosePatrol);
		return FSM_Continue;
	}

	if( !GetPed()->GetIsInVehicle() )
	{
		tapDebugf3("CTaskAmbulancePatrol::GoToIncidentLocationInVehicle_OnUpdate-->SetState(State_RespondToOrder)");
		SetState(State_RespondToOrder);
		return FSM_Continue;
	}

	float fXYDistanceSq = FLT_MAX;
	float fZDistance = FLT_MAX;

	if (pOrder->GetIncident())
	{
		Vector3 vDelta = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleBonnetPosition(GetPed()->GetMyVehicle(), false, true)) 
			- pOrder->GetIncident()->GetLocation();
		fXYDistanceSq = vDelta.XYMag2();
		fZDistance = fabsf(vDelta.z);
	}
	
	//Because we avoid junctions this can be pretty large
	const bool bWithinXYTolerance = fXYDistanceSq < rage::square(60.0f);
	const bool bWithinZTolerance = fZDistance < CTaskVehicleMissionBase::ACHIEVE_Z_DISTANCE;

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished) || GetSubTask() == NULL)
	{		
	
		if (bWithinXYTolerance && bWithinZTolerance)
		{
			if (!pOrder->GetIncident() || !pOrder->GetIncident()->GetEntity() || !pOrder->GetIncident()->GetEntity()->GetIsTypePed()  
				|| !CheckBodyCanBeReached(static_cast<CPed*>(pOrder->GetIncident()->GetEntity())))
			{
				m_iTimeToLeaveBody -= fwTimer::GetTimeStepInMilliseconds();
				if(m_iTimeToLeaveBody > 0)
				{
					return FSM_Continue;	
				}
				pOrder->SetEntityArrivedAtIncident();
				pOrder->SetEntityFinishedWithIncident();
				if(pOrder->GetIncident())
				{
					pOrder->GetIncident()->SetHasBeenAddressed(true);
				}

				tapDebugf3("CTaskAmbulancePatrol::GoToIncidentLocationInVehicle_OnUpdate--!CheckBodyCanBeReached-->SetState(State_Finish)");
				SetState(State_Finish);
				return FSM_Continue;	
			}

			GetPed()->NewSay("MEDIC_EXIT_AMBULANCE");
			tapDebugf3("CTaskAmbulancePatrol::GoToIncidentLocationInVehicle_OnUpdate-->SetState(State_ExitVehicleAndChooseResponse)");
			SetState(State_ExitVehicleAndChooseResponse);
			return FSM_Continue;	
		}
		else
		{
			//The ambulance isn't able to get to this dead body, finish
			pOrder->SetEntityArrivedAtIncident();
			pOrder->SetEntityFinishedWithIncident();
			if(pOrder->GetIncident())
			{
				pOrder->GetIncident()->SetHasBeenAddressed(true);
			}
			SetState(State_Finish);
			return FSM_Continue;
		}	
	}

	//if we're stopped, but not there yet, remove the flag that tells us to stop for
	//cars
	if (GetPed()->GetMyVehicle()->GetVelocity().Mag2() < 0.5f && GetPed()->GetMyVehicle()->GetIntelligence())
	{
		//Instead of smashing through vehicles, if we are within 40m, which is pretty close, stop the vehicle.
		if(fXYDistanceSq < rage::square(40.0f) && bWithinZTolerance)
		{
			SetState(State_StopVehicle);
			return FSM_Continue;
		}

		CTaskVehicleMissionBase* pActiveVehTask = GetPed()->GetMyVehicle()->GetIntelligence()->GetActiveTask();
		if (pActiveVehTask && pActiveVehTask->IsVehicleMissionTask())
		{
			pActiveVehTask->SetDrivingFlag(DF_StopForCars, false);
			if (pActiveVehTask->GetSubTask() && pActiveVehTask->GetSubTask()->IsVehicleMissionTask())
			{
				static_cast<CTaskVehicleMissionBase*>(pActiveVehTask->GetSubTask())->SetDrivingFlag(DF_StopForCars, false);
			}
		}	
	}

	//Stop if the next road node is turned off
	if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_CONTROL_VEHICLE)
	{
		aiTask* pTask = static_cast<CTaskControlVehicle*>(GetSubTask())->GetVehicleTask();
		if (pTask && (pTask->GetTaskType() == CTaskTypes::TASK_VEHICLE_GOTO_AUTOMOBILE_NEW))
		{
			if(!m_bAllowToUseTurnedOffNodes && GetPed()->GetMyVehicle()->GetIntelligence())
			{
				CVehicleNodeList * pNodeList = GetPed()->GetMyVehicle()->GetIntelligence()->GetNodeList();
				if(pNodeList)
				{
					//Look 2 nodes ahead to stop before going down the turned off road
					s32 iFutureNode = pNodeList->GetTargetNodeIndex() + 2;
					s32 iOldNode = pNodeList->GetTargetNodeIndex();

					for (s32 node = iOldNode; node < iFutureNode; node++)
					{
						if (node >= CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED 
							||pNodeList->GetPathNodeAddr(node).IsEmpty() 
							|| !ThePaths.IsRegionLoaded(pNodeList->GetPathNodeAddr(node)))
						{
							break;
						}

						const CPathNode *pNode = ThePaths.FindNodePointer(pNodeList->GetPathNodeAddr(node));
						if (pNode && (pNode->IsSwitchedOff() || pNode->m_2.m_deadEndness))
						{
							SetState(State_StopVehicle);
							return FSM_Continue;
						}
					}
				}
			}
		}
	}

	return FSM_Continue;
}

void CTaskAmbulancePatrol::GoToIncidentLocationInVehicle_OnExit()
{
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);	

	m_bAllowToUseTurnedOffNodes = false;
}


void CTaskAmbulancePatrol::StopVehicle_OnEnter()
{
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);	

	//Create the vehicle task for stop.
	CTask* pVehicleTaskForStop = rage_new CTaskVehicleStop(CTaskVehicleStop::SF_UseFullBrake);

	//Create the task for stop.
	CTask* pTaskForStop = rage_new CTaskControlVehicle(GetPed()->GetVehiclePedInside(), pVehicleTaskForStop);

	//Start the task.
	SetNewTask(pTaskForStop);
}

CTask::FSM_Return CTaskAmbulancePatrol::StopVehicle_OnUpdate()
{
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);	

	CAmbulanceOrder* pOrder = HelperGetAmbulanceOrder();
	if( pOrder == NULL )
	{
		tapDebugf3("CTaskAmbulancePatrol::StopVehicle_OnUpdate-->SetState(State_ChoosePatrol)");
		SetState(State_ChoosePatrol);
		return FSM_Continue;
	}

	if( !GetPed()->GetIsInVehicle() )
	{
		tapDebugf3("CTaskAmbulancePatrol::StopVehicle_OnUpdate-->SetState(State_RespondToOrder)");
		SetState(State_RespondToOrder);
		return FSM_Continue;
	}

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished) || !GetSubTask())
	{
		float fXYDistanceSq = FLT_MAX;
		float fZDistance = FLT_MAX;

		if (pOrder->GetIncident())
		{
			Vector3 vDelta = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleBonnetPosition(GetPed()->GetMyVehicle(), false, true)) 
				- pOrder->GetIncident()->GetLocation();
			fXYDistanceSq = vDelta.XYMag2();
			fZDistance = fabsf(vDelta.z);
		}
		//Because we avoid junctions this can be pretty large
		const bool bWithinXYTolerance = fXYDistanceSq < rage::square(60.0f);
		const bool bWithinZTolerance = fZDistance < CTaskVehicleMissionBase::ACHIEVE_Z_DISTANCE;

		if (bWithinXYTolerance && bWithinZTolerance)
		{
			if (!pOrder->GetIncident() || !pOrder->GetIncident()->GetEntity() || !pOrder->GetIncident()->GetEntity()->GetIsTypePed()  
				|| !CheckBodyCanBeReached(static_cast<CPed*>(pOrder->GetIncident()->GetEntity())))
			{
				m_iTimeToLeaveBody -= fwTimer::GetTimeStepInMilliseconds();
				if(m_iTimeToLeaveBody > 0)
				{
					return FSM_Continue;	
				}
				pOrder->SetEntityArrivedAtIncident();
				pOrder->SetEntityFinishedWithIncident();
				if(pOrder->GetIncident())
				{
					pOrder->GetIncident()->SetHasBeenAddressed(true);
				}

				tapDebugf3("CTaskAmbulancePatrol::StopVehicle_OnUpdate--!CheckBodyCanBeReached-->SetState(State_Finish)");
				SetState(State_Finish);
				return FSM_Continue;	
			}

			tapDebugf3("CTaskAmbulancePatrol::StopVehicle_OnUpdate-->SetState(State_ExitVehicleAndChooseResponse)");
			SetState(State_ExitVehicleAndChooseResponse);
			return FSM_Continue;	
		}
		else
		{
			//The ambulance isn't able to get to this dead body, finish
			pOrder->SetEntityArrivedAtIncident();
			pOrder->SetEntityFinishedWithIncident();
			if(pOrder->GetIncident())
			{
				pOrder->GetIncident()->SetHasBeenAddressed(true);
			}
			SetState(State_Finish);
			return FSM_Continue;
		}	
	}
	return FSM_Continue;
}

void CTaskAmbulancePatrol::GoToIncidentLocationAsPassenger_OnEnter()
{
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);	
}

CTask::FSM_Return CTaskAmbulancePatrol::GoToIncidentLocationAsPassenger_OnUpdate()
{
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);	

	CAmbulanceOrder* pOrder = HelperGetAmbulanceOrder();
	if( pOrder ==NULL )
	{
		tapDebugf3("pOrder == NULL --> SetState(State_ChoosePatrol)");
		SetState(State_ChoosePatrol);
		return FSM_Continue;
	}

	if( !GetPed()->GetIsInVehicle() )
	{
		tapDebugf3("!GetPed()->GetIsInVehicle() --> SetState(State_RespondToOrder)");
		SetState(State_RespondToOrder);
		return FSM_Continue;
	}

// 	if (pOrder->GetType() != COrder::OT_Ambulance)
// 	{
// 		return FSM_Continue;
// 	}

	//Check if the incident is finished with, this could happen if it was created by a phone call.
	if (pOrder->GetIncident() && pOrder->GetIncident()->GetHasBeenAddressed())
	{
		tapDebugf3("(pOrder->GetIncident() && pOrder->GetIncident()->GetHasBeenAddressed()) --> SetState(State_Finish)");

		pOrder->SetEntityArrivedAtIncident();
		pOrder->SetEntityFinishedWithIncident();
		SetState(State_Finish);
		return FSM_Continue;	
	}

	CPed* pDriver = GetPed()->GetMyVehicle()->GetDriver();
	if( pDriver == NULL ||
		pDriver->IsInjured() ||
		pDriver->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE) )
	{
		if (!pOrder->GetIncident() || !pOrder->GetIncident()->GetEntity() || !pOrder->GetIncident()->GetEntity()->GetIsTypePed()  
			|| !CheckBodyCanBeReached(static_cast<CPed*>(pOrder->GetIncident()->GetEntity())))
		{
			tapDebugf3("!CheckBodyCanBeReached --> SetState(State_Finish)");

			pOrder->SetEntityArrivedAtIncident();
			pOrder->SetEntityFinishedWithIncident();
			if(pOrder->GetIncident())
			{
				pOrder->GetIncident()->SetHasBeenAddressed(true);
			}
			SetState(State_Finish);
			return FSM_Continue;	
		}

		tapDebugf3("SetState(State_ExitVehicleAndChooseResponse)");
		SetState(State_ExitVehicleAndChooseResponse);
		return FSM_Continue;
	}	

	return FSM_Continue;
}

void CTaskAmbulancePatrol::GoToIncidentLocationOnFoot_OnEnter()
{
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);	

	const COrder* pOrder = GetPed()->GetPedIntelligence()->GetOrder();
	if( aiVerifyf(pOrder && pOrder->GetIsValid(), "GoToIncidentLocation_OnEnter:: Invalid Order" ) &&
		aiVerifyf(!GetPed()->GetIsInVehicle(), "GoToIncidentLocationInVehicle_OnEnter: Not on foot!") && 
		(!pOrder->GetIncident() || !pOrder->GetIncident()->GetEntity() || !pOrder->GetIncident()->GetEntity()->GetIsTypePed()  
		  || !CheckBodyCanBeReached(static_cast<CPed*>(pOrder->GetIncident()->GetEntity()))))
	{
		Vector3 vLocation = pOrder->GetIncident()->GetLocation();
		CTaskMoveFollowNavMesh* pNewSubtask = rage_new CTaskMoveFollowNavMesh(fwRandom::GetRandomNumberInRange(1.85f,2.15f), 
			vLocation,		
			20.0f, //Within 20m so use the other functions to get in the right location
			s_fHealerWalkDistance, 
			-1,
			true,
			false,
			NULL);
		pNewSubtask->SetKeepMovingWhilstWaitingForPath(true);
		SetNewTask(rage_new CTaskComplexControlMovement(pNewSubtask));
	}
}

CTask::FSM_Return CTaskAmbulancePatrol::GoToIncidentLocationOnFoot_OnUpdate()
{
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);	

	CPed* pPed = GetPed();
	COrder* pOrder = GetPed()->GetPedIntelligence()->GetOrder();
	if( pOrder ==NULL || !pOrder->GetIsValid() )
	{
		tapDebugf3("CTaskAmbulancePatrol::GoToIncidentLocationOnFoot_OnUpdate-->SetState(State_ChoosePatrol)");
		SetState(State_ChoosePatrol);
		return FSM_Continue;
	}

	if( pPed->GetIsInVehicle() )
	{
		tapDebugf3("CTaskAmbulancePatrol::GoToIncidentLocationOnFoot_OnUpdate-->SetState(State_RespondToOrder)");
		SetState(State_RespondToOrder);
		return FSM_Continue;
	}

		//Check if we are unable to find a route.
	CTaskMoveFollowNavMesh* pTask = static_cast<CTaskMoveFollowNavMesh *>(pPed->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH));
	if(pTask && pTask->IsUnableToFindRoute())
	{
		pOrder->SetEntityArrivedAtIncident();
		pOrder->SetEntityFinishedWithIncident();
		if(pOrder->GetIncident())
		{
			pOrder->GetIncident()->SetHasBeenAddressed(true);
		}

		m_bShouldApproachInjuredPed = false;
		tapDebugf3("CTaskAmbulancePatrol::GoToIncidentLocationOnFoot_OnUpdate-->SetState(State_ReturnToAmbulance)");
		SetState(State_ReturnToAmbulance);
		return FSM_Continue;	
	}

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished) || GetSubTask() == NULL )
	{

		if (!pOrder->GetIncident() || !pOrder->GetIncident()->GetEntity() || !pOrder->GetIncident()->GetEntity()->GetIsTypePed()  
			|| !CheckBodyCanBeReached(static_cast<CPed*>(pOrder->GetIncident()->GetEntity())))
		{
			tapDebugf3("No Entity, or Body cannot be reached --> Finish");

			pOrder->SetEntityArrivedAtIncident();
			pOrder->SetEntityFinishedWithIncident();
			if(pOrder->GetIncident())
			{
				pOrder->GetIncident()->SetHasBeenAddressed(true);
			}
			SetState(State_Finish);
			return FSM_Continue;	
		}

		//even though we know we aren't in a vehicle at this point,
		//we want to go to this state to check in w the blackboard to see if
		//we should be the one doing the healing or not, which happens in
		//the ExitVehicleAndChooseResponse state
		tapDebugf3("CTaskAmbulancePatrol::GoToIncidentLocationOnFoot_OnUpdate-->SetState(State_ExitVehicleAndChooseResponse)");
		SetState(State_ExitVehicleAndChooseResponse);
		return FSM_Continue;	
	}

	return FSM_Continue;
}

void CTaskAmbulancePatrol::ExitVehicleAndChooseResponse_OnEnter()
{	
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);	

	CPed* pPed = GetPed();
	CAmbulanceOrder* pOrder = HelperGetAmbulanceOrder();

	if(pOrder)
	{
		if(pOrder->GetIncident()->GetArrivedResources(DT_AmbulanceDepartment) == 0 || m_bShouldApproachInjuredPed || pOrder->GetRespondingPed() == GetPed())
		{
			m_bShouldApproachInjuredPed = true;

			if ( pOrder->GetRespondingPed() == NULL || pOrder->GetRespondingPed() != GetPed())
			{
				pOrder->SetRespondingPed(pPed);
			}
		}
		pOrder->SetEntityArrivedAtIncident();
	}

	CVehicle* pVehicle = pPed->GetVehiclePedInside();
	if (pVehicle)
	{
		VehicleEnterExitFlags vehicleFlags;
		vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DelayForTime);
		vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::WarpIfDoorBlocked);
		vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DontCloseDoor);

		CTaskExitVehicle* pTask = rage_new CTaskExitVehicle(pVehicle, vehicleFlags, fwRandom::GetRandomNumberInRange(0.0f, 1.5f));
		SetNewTask(pTask);
	}
}
CTask::FSM_Return CTaskAmbulancePatrol::ExitVehicleAndChooseResponse_OnUpdate()
{
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);	

	if(GetPed() && !GetPed()->IsNetworkClone() && (GetIsFlagSet(aiTaskFlags::SubTaskFinished) || GetSubTask() == NULL ))
	{
		if (m_bShouldApproachInjuredPed)
		{
			const CAmbulanceOrder* pOrder = HelperGetAmbulanceOrder();
			//aiAssert(pOrder);
			//aiAssert(!pOrder || pOrder->GetIncident()->GetEntity());
			aiAssert(!pOrder || !pOrder->GetIncident()->GetEntity() || pOrder->GetIncident()->GetEntity()->GetIsTypePed());

			const CPed* pVictim = pOrder ? static_cast<const CPed*>(pOrder->GetIncident()->GetEntity()) : NULL;
			if (pVictim && pVictim->GetIsInVehicle())
			{
				tapDebugf3("CTaskAmbulancePatrol::ExitVehicleAndChooseResponse_OnUpdate-->SetState(State_PullFromVehicle)");
				SetState(State_PullFromVehicle);
			}
			else
			{
				tapDebugf3("CTaskAmbulancePatrol::ExitVehicleAndChooseResponse_OnUpdate-->SetState(State_PlayRecoverySequence)");
				SetState(State_PlayRecoverySequence);
			}	
		}
		else
		{
			tapDebugf3("CTaskAmbulancePatrol::ExitVehicleAndChooseResponse_OnUpdate-->SetState(State_SuperviseRecovery)");
			SetState(State_SuperviseRecovery);
		}
	}
	return FSM_Continue;
}


void CTaskAmbulancePatrol::PlayRecoverySequence_OnEnter()
{
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);	

	const COrder* pOrder = GetPed()->GetPedIntelligence()->GetOrder();
	if( pOrder && pOrder->GetIsValid()
		&& aiVerifyf(!GetPed()->GetIsInVehicle() || GetPed()->IsNetworkClone(), "PlayRecoverySequence_OnEnter: Not on foot!") )
	{
		CEntity* pEntity = pOrder->GetIncident()->GetEntity();

		// Add navigation blocking bound at incident entity location
		if(pEntity)
		{
			if(m_iNavMeshBlockingBounds[0] == DYNAMIC_OBJECT_INDEX_NONE && m_iNavMeshBlockingBounds[1] == DYNAMIC_OBJECT_INDEX_NONE)
			{
				const Vector3 vSizeSmall(0.5f, 0.5f, 4.0f);
				const Vector3 vSizeLarge(2.0f, 2.0f, 4.0f);
				Vector3 vPos;

				if (pEntity->GetIsTypePed())
				{
					Matrix34 mWorldBoneMatrix;
					((CPed*)pEntity)->GetGlobalMtx(((CPed*)pEntity)->GetBoneIndexFromBoneTag(BONETAG_PELVIS), mWorldBoneMatrix);
					vPos = mWorldBoneMatrix.d;
				}
				else
				{
					vPos = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());
				}

				// Create a large blocking bound which prevents only wander paths from passing through
				m_iNavMeshBlockingBounds[0] = CPathServerGta::AddBlockingObject(vPos, vSizeLarge, pEntity->GetTransform().GetHeading(), TDynamicObject::BLOCKINGOBJECT_WANDERPATH);
				// Create a much smaller blocking bound which prevents ALL path types from passing through
				m_iNavMeshBlockingBounds[1] = CPathServerGta::AddBlockingObject(vPos, vSizeSmall, pEntity->GetTransform().GetHeading(), TDynamicObject::BLOCKINGOBJECT_ALL_PATH_TYPES);
			}
		}

		//create a new point to represent the position of the incident
		m_ScenarioPoint.Reset();

		//if this is a scripted incident (dialed from 911), it may not have an associated entity
		if (pEntity)
		{
			if (pEntity->GetIsTypePed())
			{
				CPed* pPed = static_cast<CPed*>(pEntity);
				const float fOffsetDist = 1.0f;
				Matrix34 mWorldBoneMatrix;
				pPed->GetGlobalMtx(pPed->GetBoneIndexFromBoneTag(BONETAG_PELVIS), mWorldBoneMatrix);
			
				Vector3 vOffsetWorldSpace = mWorldBoneMatrix.c;
				vOffsetWorldSpace.z = 0.0f;
				vOffsetWorldSpace.NormalizeSafe(ORIGIN);
				vOffsetWorldSpace.Scale(fOffsetDist);

				Vector3 position = mWorldBoneMatrix.d + vOffsetWorldSpace;
				
				//Get the ground position
				float groundz  = WorldProbe::FindGroundZFor3DCoord(TOP_SURFACE, position);	
				position.z = groundz; 

				m_ScenarioPoint.SetPosition(VECTOR3_TO_VEC3V(position));
				
				//invert the offset vector here, since we want to face the opposite direction
				float fDirection = rage::Atan2f(vOffsetWorldSpace.x, -vOffsetWorldSpace.y);
				m_ScenarioPoint.SetDirection(fDirection);
			
				//query the scenarioinfo
				static const atHashWithStringNotFinal tendToPedName("CODE_HUMAN_MEDIC_TEND_TO_DEAD",0x3F2E396F);
				s32 iScenarioIndex = SCENARIOINFOMGR.GetScenarioIndex(tendToPedName);

				// Set the scenario type in the CSpawnPoint, as it would otherwise be zero from
				// the m_ScenarioPoint.Reset() above. It's probably not used much, but it may at least
				// affect some error messages if something goes wrong, which would otherwise be
				// confusing if they just used the scenario name from type 0.
				m_ScenarioPoint.SetScenarioType((unsigned)iScenarioIndex);

				//create the task
				CTaskUseScenario* pTask = rage_new CTaskUseScenario(iScenarioIndex, &m_ScenarioPoint, CTaskUseScenario::SF_CheckConditions | CTaskUseScenario::SF_CanExitForNearbyPavement | CTaskUseScenario::SF_CheckShockingEvents);
				pTask->SetIsWorldScenario(false);	// We have our own CScenarioPoint, which is not inserted in the world.
				pTask->OverrideMoveBlendRatio(MOVEBLENDRATIO_RUN);
				SetNewTask(pTask);

				SetBodyPosition(mWorldBoneMatrix.d);	

				m_bArriveAudioTriggered = false;
			}

#if __BANK
			if(CDispatchManager::GetInstance().DebugEnabled(DT_AmbulanceDepartment) || PARAM_debugtaskambulancepatrol.Get())
			{
				grcDebugDraw::Arrow(m_ScenarioPoint.GetPosition(), m_ScenarioPoint.GetPosition() + m_ScenarioPoint.GetDirection(), 0.5f,Color_yellow3, 1000);
			}
#endif
		}
	}
}
CTask::FSM_Return CTaskAmbulancePatrol::PlayRecoverySequence_OnUpdate()
{
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);	

	CPed* pPed = GetPed();

	if (pPed && pPed->IsNetworkClone())
		return FSM_Continue;

	COrder* pOrder = pPed->GetPedIntelligence()->GetOrder();
	if( pOrder ==NULL || !pOrder->GetIsValid() || GetSubTask() == NULL || !pOrder->GetIncident()->GetEntity() ||
		(!pOrder->GetIncident()->GetEntity()->GetIsTypePed() || HasBodyMoved(static_cast<CPed*>(pOrder->GetIncident()->GetEntity()))))
	{
		//try and go back, we'll go back to patrol
		//when the enter vehicle task either succeeds or fails
		m_bShouldApproachInjuredPed = false;
		tapDebugf3("CTaskAmbulancePatrol::PlayRecoverySequence_OnUpdate-->SetState(State_ReturnToAmbulance) 1");
		SetState(State_ReturnToAmbulance);
		return FSM_Continue;
	}

	if( pPed->GetIsInVehicle() )
	{
		m_bShouldApproachInjuredPed = false;
		tapDebugf3("CTaskAmbulancePatrol::PlayRecoverySequence_OnUpdate-->SetState(State_ExitVehicleAndChooseResponse)");
		SetState(State_ExitVehicleAndChooseResponse);
		return FSM_Continue;
	}

	//Check if we are unable to find a route.
	CTaskMoveFollowNavMesh* pTask = static_cast<CTaskMoveFollowNavMesh *>(pPed->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH));
	if(pTask && pTask->IsUnableToFindRoute())
	{
		pOrder->SetEntityArrivedAtIncident();
		pOrder->SetEntityFinishedWithIncident();
		if(pOrder->GetIncident())
		{
			pOrder->GetIncident()->SetHasBeenAddressed(true);
		}

		m_bShouldApproachInjuredPed = false;
		tapDebugf3("CTaskAmbulancePatrol::PlayRecoverySequence_OnUpdate-->SetState(State_ReturnToAmbulance) 2");
		SetState(State_ReturnToAmbulance);
		return FSM_Continue;
	}

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{ 
		//Check the medic has made it close enough to the body
		if(pOrder->GetIncident() && (pOrder->GetIncident()->GetLocation() - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition())).Mag2() > rage::square(3.0f))
		{
			pOrder->SetEntityArrivedAtIncident();
			pOrder->SetEntityFinishedWithIncident();
			if(pOrder->GetIncident())
			{
				pOrder->GetIncident()->SetHasBeenAddressed(true);
			}

			m_bShouldApproachInjuredPed = false;
			tapDebugf3("CTaskAmbulancePatrol::PlayRecoverySequence_OnUpdate-->SetState(State_ReturnToAmbulance) 3");
			SetState(State_ReturnToAmbulance);
			return FSM_Continue;
		}

		SetState(State_HealVictim);
		return FSM_Continue;
	}

	CTaskUseScenario* pTaskUseScenario = static_cast<CTaskUseScenario*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_USE_SCENARIO));
	if(pTaskUseScenario && pTaskUseScenario->GetScenarioPoint())
	{
		float fDistToScenarioSQ = (VEC3V_TO_VECTOR3(pTaskUseScenario->GetScenarioPoint()->GetPosition()) - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition())).Mag2();
		if(fDistToScenarioSQ < rage::square(4.5f))
		{
			pTaskUseScenario->OverrideMoveBlendRatio(MOVEBLENDRATIO_WALK);
		}

		//Handle audio	
		if(!m_bArriveAudioTriggered && !pPed->IsInMotion() && fDistToScenarioSQ < rage::square(1.0f))
		{			
			CVehicle* pVehicle = pPed->GetMyVehicle();
			if(pVehicle)
			{
				CPed* pDriver = pVehicle->GetSeatManager()->GetLastPedInSeat(0);
				if(pDriver != NULL && pDriver == pPed)
				{
					CPed* pPassenger = pVehicle->GetSeatManager()->GetLastPedInSeat(1);
					if(pPassenger != NULL && !pPassenger->GetIsDeadOrDying())
					{
						pPed->NewSay("MEDIC_ARRIVE_AT_BODY", 0, false, false, -1, pPassenger, ATSTRINGHASH("MEDIC_REALISE_VICTIM_DEAD", 0xa6345547));
					}
					else
					{
						pPed->NewSay("MEDIC_ARRIVE_AT_BODY");
					}
				}
				else
				{
					CPed* pPassenger = pVehicle->GetSeatManager()->GetLastPedInSeat(1);
					if(pPassenger != NULL && pPassenger == pPed && pDriver)
					{
						pPed->NewSay("MEDIC_ARRIVE_AT_BODY", 0, false, false, -1, pDriver, ATSTRINGHASH("MEDIC_REALISE_VICTIM_DEAD", 0xa6345547));
					}
					else
					{
						pPed->NewSay("MEDIC_ARRIVE_AT_BODY");
					}
				}
			}
			else
			{
				pPed->NewSay("MEDIC_ARRIVE_AT_BODY");
			}

			m_bArriveAudioTriggered = true;
		}
	}
	
	// Look at the body
	if (!pPed->GetIkManager().IsLooking())
	{
		LookAtTarget(pPed, pOrder->GetIncident()->GetEntity());
	}

	return FSM_Continue;
}

void CTaskAmbulancePatrol::PlayRecoverySequence_OnExit()
{
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);	

	m_bArriveAudioTriggered = false;
	GetPed()->GetIkManager().AbortLookAt(500);
}

void CTaskAmbulancePatrol::HealVictim_OnEnter()
{
	
}

CTask::FSM_Return CTaskAmbulancePatrol::HealVictim_OnUpdate()
{
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);	

	//We no longer heal the victim so play the revive fail scenario
	SetState(State_ReviveFailed);
	return FSM_Continue;
}

void CTaskAmbulancePatrol::ReturnToAmbulance_OnEnter()
{
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);	

	// remove any navigation blocking bound now that injured ped has been treated
	CPathServerGta::RemoveBlockingObject(m_iNavMeshBlockingBounds[0]);
	CPathServerGta::RemoveBlockingObject(m_iNavMeshBlockingBounds[1]);
	m_iNavMeshBlockingBounds[0] = m_iNavMeshBlockingBounds[1] = DYNAMIC_OBJECT_INDEX_NONE;

	//don't throw an AI assert here, since other states will jump here
	//if things go wrong.  the idea is that we may decide on a way to store information
	//about the ambulance to use outside of the CIncident, so other states can jump here
	//and then we'll get the peds to their ambulance before releasing them on patrol.
	//for right now though, it relies on the COrder and the CIncident, so we'll just silently
	//return early so the OnUpdate can kick us back to State_Start -JM
	CAmbulanceOrder* pOrder = HelperGetAmbulanceOrder();

	CVehicle* pVehicle = NULL; 

	if (pOrder)
	{
		//this is set here since the supervisor is waiting for the victim to be healed to return to the ambulance
		//so it's decoupled from the actual healing process since if the heal fails, we may stand around and do a 
		//time of death scenario before returning
		pOrder->SetIncidentHasBeenAddressed(true);

		pVehicle = pOrder->GetEmergencyVehicle();
	}

	if (!pVehicle)
	{
		pVehicle = GetPed()->GetMyVehicle();
	}

	if (pVehicle && pVehicle->IsInDriveableState())
	{
		int iSeat = 1;
		CPed* pLastDriver = pVehicle->GetSeatManager()->GetLastPedInSeat(0);
		if(!pLastDriver || pLastDriver == GetPed() || pLastDriver->GetIsDeadOrDying())
		{
			iSeat = 0;
		}

		const float moveSpeed = fwRandom::GetRandomNumberInRange(0.85f,1.15f);
		VehicleEnterExitFlags vehicleFlags;
		vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DontJackAnyone);
		CTaskEnterVehicle* pTask = rage_new CTaskEnterVehicle(pVehicle, SR_Prefer, iSeat , vehicleFlags, 0.0f, moveSpeed);
		SetNewTask(pTask);
	}

	MarkPedAsFinishedWithOrder();
}

CTask::FSM_Return CTaskAmbulancePatrol::ReturnToAmbulance_OnUpdate()
{
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);	
	
	if (GetPed()->IsNetworkClone())
		return FSM_Continue;

	CAmbulanceOrder* pOrder = HelperGetAmbulanceOrder();
	if( pOrder && pOrder->GetIsValid())
	{	
		CIncident* pIncident = pOrder->GetIncident();
		if (pIncident && !pIncident->GetHasBeenAddressed())
		{
			tapDebugf3("CTaskAmbulancePatrol::ReturnToAmbulance_OnUpdate-->SetState(State_RespondToOrder)");
			SetState(State_RespondToOrder);
			return FSM_Continue;
		}
	}

	if (!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		if ( GetPed()->GetIsInVehicle())
		{
			tapDebugf3("CTaskAmbulancePatrol::ReturnToAmbulance_OnUpdate-->SetState(State_WaitInAmbulance)");
			SetState(State_WaitInAmbulance);
			return FSM_Continue;
		}

		//We're not been able to get to our vehicle, patrol the best way we can, i.e. on foot.
		tapDebugf3("CTaskAmbulancePatrol::ReturnToAmbulance_OnUpdate-->SetState(State_ChoosePatrol) 1");
		SetState(State_ChoosePatrol);
		return FSM_Continue;
	}

	CTaskMoveFollowNavMesh* pTask = static_cast<CTaskMoveFollowNavMesh *>(GetPed()->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH));
	if(pTask)
	{
		//Check if we are unable to find a route.
		if(pTask->IsUnableToFindRoute())
		{
			// Clear order info (prevent infinite loop State_ChoosePatrol->State_Unalerted->State_RespondToOrder->State_ReturnToAmbulance->State_ChoosePatrol).
			if(pOrder && pOrder->GetIsValid())
			{	
				pOrder->SetEntityArrivedAtIncident();
				pOrder->SetEntityFinishedWithIncident();
				pOrder->GetIncident()->SetHasBeenAddressed(true);
			}

			//We're not been able to get to our vehicle, patrol the best way we can, i.e. on foot.
			tapDebugf3("CTaskAmbulancePatrol::ReturnToAmbulance_OnUpdate-->SetState(State_ChoosePatrol) 2");
			SetState(State_ChoosePatrol);
			return FSM_Continue;
		}
	}

	return FSM_Continue;
}

void CTaskAmbulancePatrol::HelperUpdateWaitForVictimToBeHealed(CPed* pPed)
{
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);	

	//ask our incident what vehicle it was dispatched with
	CAmbulanceOrder* pOrder = HelperGetAmbulanceOrder();

	if( pOrder ==NULL || pOrder->GetIncidentHasBeenAddressed() || UpdateCheckForNewReviver() || HasReviverFinished())
	{
		CTaskUseScenario* pTaskUseScenario = static_cast<CTaskUseScenario*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_USE_SCENARIO));
		if(pTaskUseScenario)
		{
			pTaskUseScenario->SetTimeToLeave(0.0f);
		}
	}
}

bool CTaskAmbulancePatrol::HasReviverFinished()
{
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);	

	const CAmbulanceOrder* pOrder = HelperGetAmbulanceOrder();

	//only do this if we have a valid injury incident--not for scripted
	if( pOrder && pOrder->GetInjuryIncident())
	{
		const CPed* pReviver = pOrder->GetRespondingPed();
		if (pReviver)
		{
			CTask* pTask = pReviver->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AMBULANCE_PATROL); 
			if(pTask)
			{
				if(static_cast<CTaskAmbulancePatrol*>(pTask)->GetState() == CTaskAmbulancePatrol::State_ReviveFailed)
				{
					tapDebugf3("CTaskAmbulancePatrol::HasReviverFinished-->return true");
					return true;
				}
			}
		}
	}
	return false;
}

//kneel in place
void CTaskAmbulancePatrol::SuperviseRecovery_OnEnter()
{
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);	

	const COrder* pOrder = GetPed()->GetPedIntelligence()->GetOrder();
	if( pOrder && pOrder->GetIsValid() &&
		aiVerifyf(!GetPed()->GetIsInVehicle() || GetPed()->IsNetworkClone(), "SuperviseRecovery_OnEnter: Not on foot!") )
	{
		CEntity* pEntity = pOrder->GetIncident()->GetEntity();

		if (pEntity)
		{
			if (pEntity->GetIsTypePed())
			{
				CPed* pPed = static_cast<CPed*>(pEntity);

				//Calculate world position
				const float fOffsetDist = 1.0f;
				Matrix34 mWorldBoneMatrix;
				pPed->GetGlobalMtx(pPed->GetBoneIndexFromBoneTag(BONETAG_PELVIS), mWorldBoneMatrix);

				Vector3 vOffsetWorldSpace = mWorldBoneMatrix.c;
				vOffsetWorldSpace.z = 0.0f;
				vOffsetWorldSpace.NormalizeSafe(ORIGIN);
				vOffsetWorldSpace.Scale(fOffsetDist);

				Vector3 position = mWorldBoneMatrix.d - vOffsetWorldSpace;

				//Get the ground position
				float groundz  = WorldProbe::FindGroundZFor3DCoord(TOP_SURFACE, position);	
				position.z = groundz; 

				m_ScenarioPoint.SetPosition(VECTOR3_TO_VEC3V(position));

				//invert the offset vector here, since we want to face the opposite direction
				float fDirection = rage::Atan2f(-vOffsetWorldSpace.x, vOffsetWorldSpace.y);
				m_ScenarioPoint.SetDirection(fDirection);

				//query the scenarioinfo
				static const atHashWithStringNotFinal kneelName("CODE_HUMAN_MEDIC_KNEEL",0xA6059A2A);
				s32 iScenarioIndex = SCENARIOINFOMGR.GetScenarioIndex(kneelName);

				//create the task
				CTaskUseScenario* pTask = rage_new CTaskUseScenario(iScenarioIndex, &m_ScenarioPoint, CTaskUseScenario::SF_CheckConditions | CTaskUseScenario::SF_CanExitForNearbyPavement | CTaskUseScenario::SF_CheckShockingEvents);
				pTask->SetIsWorldScenario(false);	// We have our own CScenarioPoint, which is not inserted in the world.
				pTask->OverrideMoveBlendRatio(MOVEBLENDRATIO_RUN);
				SetNewTask(pTask);

				SetBodyPosition(mWorldBoneMatrix.d);

#if __BANK
				if(CDispatchManager::GetInstance().DebugEnabled(DT_AmbulanceDepartment) || PARAM_debugtaskambulancepatrol.Get())
				{
					grcDebugDraw::Arrow(m_ScenarioPoint.GetPosition(), m_ScenarioPoint.GetPosition() + m_ScenarioPoint.GetDirection(), 0.5f,Color_yellow, 1000);
				}
#endif
			}
		}
	}
}

CTask::FSM_Return CTaskAmbulancePatrol::SuperviseRecovery_OnUpdate()
{	
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);	

	CPed* pPed = GetPed();

	if (pPed->IsNetworkClone())
		return FSM_Continue;

	HelperUpdateWaitForVictimToBeHealed(pPed);
	
	const COrder* pOrder = GetPed()->GetPedIntelligence()->GetOrder();
	if( pOrder == NULL || !pOrder->GetIsValid() ||  !pOrder->GetIncident()->GetEntity() || 
		(!pOrder->GetIncident()->GetEntity()->GetIsTypePed() || HasBodyMoved(static_cast<CPed*>(pOrder->GetIncident()->GetEntity()))))
	{
		tapDebugf3("CTaskAmbulancePatrol::SuperviseRecovery_OnUpdate--pOrder[%p] pOrder->IsValid[%d] pOrder->GetIncident()->GetEntity[%p] pOrder->GetIncident()->GetEntity()->GetIsTypePed[%d] HasBodyMoved(static_cast<CPed*>(pOrder->GetIncident()->GetEntity()))[%d] --> SetState(State_ReturnToAmbulance) 1",pOrder,pOrder ? pOrder->GetIsValid() : false,pOrder && pOrder->GetIncident() ? pOrder->GetIncident()->GetEntity() : 0,pOrder && pOrder->GetIncident() && pOrder->GetIncident()->GetEntity() ? pOrder->GetIncident()->GetEntity()->GetIsTypePed() : false,pOrder && pOrder->GetIncident() && pOrder->GetEntity() ? HasBodyMoved(static_cast<CPed*>(pOrder->GetIncident()->GetEntity())) : false);
		SetState(State_ReturnToAmbulance);
		return FSM_Continue;
	}

	CTaskMoveFollowNavMesh* pTask = static_cast<CTaskMoveFollowNavMesh *>(pPed->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH));
	if(pTask)
	{
		//Check if we are unable to find a route.
		if(pTask->IsUnableToFindRoute())
		{
			tapDebugf3("CTaskAmbulancePatrol::SuperviseRecovery_OnUpdate--pTask && pTask->IsUnableToFindRoute --> SetState(State_ReturnToAmbulance) 2");
			SetState(State_ReturnToAmbulance);
			return FSM_Continue;
		}
	}

	if (!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		if (UpdateCheckForNewReviver())
		{
			//jump here since this state contains some code that registers
			//the ped with the incident
			tapDebugf3("CTaskAmbulancePatrol::SuperviseRecovery_OnUpdate--GetSubTask[%p] GetIsFlagSet(aiTaskFlags::SubTaskFinished)[%d] UpdateCheckForNewReviver -->SetState(State_ExitVehicleAndChooseResponse)",GetSubTask(),GetIsFlagSet(aiTaskFlags::SubTaskFinished));
			SetState(State_ExitVehicleAndChooseResponse);
			return FSM_Continue;
		}

		tapDebugf3("CTaskAmbulancePatrol::SuperviseRecovery_OnUpdate--GetSubTask[%p] GetIsFlagSet(aiTaskFlags::SubTaskFinished)[%d] -->SetState(State_WaitForBothMedicsToFinish)",GetSubTask(),GetIsFlagSet(aiTaskFlags::SubTaskFinished));
		SetState(State_WaitForBothMedicsToFinish);
		return FSM_Continue;
	}

	CTaskUseScenario* pTaskUseScenario = static_cast<CTaskUseScenario*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_USE_SCENARIO));
	if(pTaskUseScenario && pTaskUseScenario->GetScenarioPoint())
	{
		if((VEC3V_TO_VECTOR3(pTaskUseScenario->GetScenarioPoint()->GetPosition()) - VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition())).Mag2() < rage::square(4.5f))
		{
			pTaskUseScenario->OverrideMoveBlendRatio(MOVEBLENDRATIO_WALK);
		}
	}

	// Look at the entity we're approaching.
	if (!pPed->GetIkManager().IsLooking())
	{
		const COrder* pOrder = GetPed()->GetPedIntelligence()->GetOrder();
		if(pOrder && pOrder->GetIsValid())
		{
			LookAtTarget(pPed, pOrder->GetIncident()->GetEntity());
		}
	}

	return FSM_Continue;
}

void CTaskAmbulancePatrol::SuperviseRecovery_OnExit()
{
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);	

	GetPed()->GetIkManager().AbortLookAt(500);
}


//	TransportVictim
void CTaskAmbulancePatrol::TransportVictim_OnEnter()
{
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);	

	CVehicle* pVehicle = GetPed()->GetMyVehicle();
	if( aiVerifyf(pVehicle, "PatrolInVehicle_OnEnter: Vehicle NULL!") )
	{
		pVehicle->TurnSirenOn(true);
		SetNewTask(rage_new CTaskCarDriveWander(pVehicle, DMode_AvoidCars
			, CDriverPersonality::FindMaxCruiseSpeed(GetPed(), pVehicle, pVehicle->GetVehicleType() == VEHICLE_TYPE_BICYCLE) * 2.0f, false));
	}
}
CTask::FSM_Return CTaskAmbulancePatrol::TransportVictim_OnUpdate()
{
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);	

	const CAmbulanceOrder* pOrder = HelperGetAmbulanceOrder();
	if( pOrder && !pOrder->GetIncidentHasBeenAddressed())
	{
		tapDebugf3("CTaskAmbulancePatrol::TransportVictim_OnUpdate-->SetState(State_RespondToOrder)");
		SetState(State_RespondToOrder);
		return FSM_Continue;
	}

	return FSM_Continue;
}

//	TransportVictimAsPassenger
void CTaskAmbulancePatrol::TransportVictimAsPassenger_OnEnter()
{
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);	

	CVehicle* pVehicle = GetPed()->GetMyVehicle();
	if( aiVerifyf(pVehicle, "TransportVictimAsPassenger_OnEnter: Vehicle NULL!") )
	{
		SetNewTask(rage_new CTaskInVehicleBasic(pVehicle));
	}
}

CTask::FSM_Return CTaskAmbulancePatrol::TransportVictimAsPassenger_OnUpdate()
{
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);	

	const CAmbulanceOrder* pOrder = HelperGetAmbulanceOrder();
	if( pOrder && !pOrder->GetIncidentHasBeenAddressed())
	{
		tapDebugf3("CTaskAmbulancePatrol::TransportVictimAsPassenger_OnUpdate-->SetState(State_RespondToOrder)");
		SetState(State_RespondToOrder);
		return FSM_Continue;
	}

	return FSM_Continue;
}

//	ReviveFailed
void CTaskAmbulancePatrol::ReviveFailed_OnEnter()
{
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);	

	const COrder* pOrder = GetPed()->GetPedIntelligence()->GetOrder();
	if( pOrder && pOrder->GetIsValid()  &&
		aiVerifyf(!GetPed()->GetIsInVehicle(), "ReviveFailed_OnEnter: Not on foot!") )
	{
		//query the scenarioinfo
		static const atHashWithStringNotFinal timeofDeathName("CODE_HUMAN_MEDIC_TIME_OF_DEATH",0x3869617A);
		s32 iScenarioIndex = SCENARIOINFOMGR.GetScenarioIndex(timeofDeathName);

		//create the task
		CTaskUseScenario* pTask = rage_new CTaskUseScenario(iScenarioIndex, CTaskUseScenario::SF_CheckConditions | CTaskUseScenario::SF_CheckShockingEvents | CTaskUseScenario::SF_CanExitForNearbyPavement);

		if(NetworkInterface::IsGameInProgress())
		{
			pTask->SetIsWorldScenario(false);	// We have our own CScenarioPoint, which is not inserted in the world.
		}

		SetNewTask(pTask);	

		GetPed()->NewSay("MEDIC_REFLECT_ON_DEATH");
	}
}

CTask::FSM_Return CTaskAmbulancePatrol::ReviveFailed_OnUpdate()
{
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);	
	
	if (GetPed()->IsNetworkClone())
		return FSM_Continue;

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished) || GetSubTask() == NULL )
	{
		m_bShouldApproachInjuredPed = false;
		SetState(State_WaitForBothMedicsToFinish);
	}

	return FSM_Continue;
}

//	WaitInAmbulance
void CTaskAmbulancePatrol::WaitForBothMedicsToFinish_OnEnter()
{
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);	

	m_iTimeToLeaveBody = fwRandom::GetRandomNumberInRange(0, 2000);

	//query the scenarioinfo
	static const atHashWithStringNotFinal StandIdle("WORLD_HUMAN_STAND_IMPATIENT",0xD7D92E66);
	s32 iScenarioIndex = SCENARIOINFOMGR.GetScenarioIndex(StandIdle);
	
	m_ScenarioPoint.Reset();
	Vector3 position = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition());
	position.z = WorldProbe::FindGroundZFor3DCoord(TOP_SURFACE, position);	
	m_ScenarioPoint.SetPosition(VECTOR3_TO_VEC3V(position) );
	m_ScenarioPoint.SetDirection(GetPed()->GetTransform().GetHeading());

	//create the task
	CTaskUseScenario* pTask = rage_new CTaskUseScenario(iScenarioIndex, &m_ScenarioPoint, CTaskUseScenario::SF_CheckConditions | CTaskUseScenario::SF_CanExitForNearbyPavement | CTaskUseScenario::SF_CheckShockingEvents);
	pTask->SetIsWorldScenario(false);	// We have our own CScenarioPoint, which is not inserted in the world.
	SetNewTask(pTask);	
}

CTask::FSM_Return CTaskAmbulancePatrol::WaitForBothMedicsToFinish_OnUpdate()
{
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);	

	CPed* pPed = GetPed();

	if (pPed->IsNetworkClone())
		return FSM_Continue;

	if(pPed->GetIsInVehicle())
	{
		tapDebugf3("CTaskAmbulancePatrol::WaitForBothMedicsToFinish_OnUpdate-->SetState(State_WaitInAmbulance)");
		SetState(State_WaitInAmbulance);
		return FSM_Continue;
	}

	//B*1858409: If we haven't got a scenario sub-task, bail out and return to ambulance
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished) || GetSubTask() == NULL )
	{
		tapDebugf3("CTaskAmbulancePatrol::WaitForBothMedicsToFinish_OnUpdate-->SetState(State_ReturnToAmbulance) 2");
		SetState(State_ReturnToAmbulance);
		return FSM_Continue;
	}

	bool bWaitForPartner = false;
	CVehicle* pVehicle = pPed->GetMyVehicle();
	if(pVehicle)
	{
		//Check drivers seat
		CPed* pDriver = pVehicle->GetSeatManager()->GetLastPedInSeat(0);
		if(pDriver != NULL && pDriver != pPed && !pDriver->GetIsDeadOrDying())
		{
			CTask* pTask = pDriver->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AMBULANCE_PATROL); 
			if(pTask)
			{
				s32 iState = static_cast<CTaskAmbulancePatrol*>(pTask)->GetState();
				if(iState == State_PlayRecoverySequence || iState == State_HealVictim || iState == State_SuperviseRecovery || iState == State_ReviveFailed)
				{
					bWaitForPartner = true;
				}
			}
		}
		else
		{
			//Check passenger seat
			CPed* pPassenger = pVehicle->GetSeatManager()->GetLastPedInSeat(1);
			if(pPassenger != NULL && pPassenger != pPed && !pPassenger->GetIsDeadOrDying())
			{
				CTask* pTask = pPassenger->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AMBULANCE_PATROL); 
				if(pTask)
				{
					s32 iState = static_cast<CTaskAmbulancePatrol*>(pTask)->GetState();
					if(iState == State_PlayRecoverySequence || iState == State_HealVictim || iState == State_SuperviseRecovery || iState == State_ReviveFailed)
					{
						bWaitForPartner = true;
					}
				}
			}
		}
	}

	if(bWaitForPartner)
	{
		return FSM_Continue;
	}

	m_iTimeToLeaveBody -= fwTimer::GetTimeStepInMilliseconds();
	if(m_iTimeToLeaveBody > 0)
	{
		return FSM_Continue;
	}

	if(pVehicle)
	{
		CPed* pDriver = pVehicle->GetSeatManager()->GetLastPedInSeat(0);
		if(pDriver != NULL && pDriver == pPed)
		{
			CPed* pPassenger = pVehicle->GetSeatManager()->GetLastPedInSeat(1);
			if(pPassenger != NULL && !pPassenger->GetIsDeadOrDying())
			{
				pPed->NewSay("MEDIC_RETURN_TO_AMBULANCE_STATE", 0, false, false, -1, pPassenger, ATSTRINGHASH("MEDIC_RETURN_TO_AMBULANCE_RESP", 0x80a993bd));
			}
			else
			{
				pPed->NewSay("MEDIC_RETURN_TO_AMBULANCE_STATE");
			}
		}
	}

	tapDebugf3("CTaskAmbulancePatrol::WaitForBothMedicsToFinish_OnUpdate-->SetState(State_ReturnToAmbulance)");
	SetState(State_ReturnToAmbulance);
	return FSM_Continue;
}

//	WaitInAmbulance
void CTaskAmbulancePatrol::WaitInAmbulance_OnEnter()
{
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);	
}

CTask::FSM_Return CTaskAmbulancePatrol::WaitInAmbulance_OnUpdate()
{
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);	

	CPed* pPed = GetPed();

	if (pPed->IsNetworkClone())
		return FSM_Continue;

	if(!pPed->GetIsInVehicle())
	{
		tapDebugf3("CTaskAmbulancePatrol::WaitInAmbulance_OnUpdate-->SetState(State_ReturnToAmbulance) 1");
		SetState(State_ReturnToAmbulance);
		return FSM_Continue;
	}

	//Check if there has been a new order
	CAmbulanceOrder* pOrder = HelperGetAmbulanceOrder();
	if( pOrder && pOrder->GetIsValid())
	{	
		CIncident* pIncident = pOrder->GetIncident();
		if (pIncident && !pIncident->GetHasBeenAddressed())
		{
			tapDebugf3("CTaskAmbulancePatrol::WaitInAmbulance_OnUpdate-->SetState(State_RespondToOrder)");
			SetState(State_RespondToOrder);
			return FSM_Continue;
		}
	}

	bool bWaitForPartner = false;
	CVehicle* pVehicle = pPed->GetMyVehicle();
	//Check drivers seat
	CPed* pDriver = pVehicle->GetSeatManager()->GetLastPedInSeat(0);
	if(pDriver != NULL && pDriver != pPed && !pDriver->GetIsDeadOrDying() && pVehicle->GetSeatManager()->GetPedInSeat(0) == NULL)
	{
		CTask* pTask = pDriver->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AMBULANCE_PATROL); 
		if(pTask)
		{
			if(static_cast<CTaskAmbulancePatrol*>(pTask)->GetState() != CTaskAmbulancePatrol::State_PatrolOnFoot)
			{
				bWaitForPartner = true;
			}
		}
	}
	else
	{
		//Check passenger seat
		CPed* pPassenger = pVehicle->GetSeatManager()->GetLastPedInSeat(1);
		if(pPassenger != NULL && pPassenger != pPed && !pPassenger->GetIsDeadOrDying() && pVehicle->GetSeatManager()->GetPedInSeat(1) == NULL)
		{
			CTask* pTask = pPassenger->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AMBULANCE_PATROL); 
			if(pTask)
			{
				if(static_cast<CTaskAmbulancePatrol*>(pTask)->GetState() != CTaskAmbulancePatrol::State_PatrolOnFoot)
				{
					bWaitForPartner = true;
				}
			}
		}
	}

	//If I'm the passenger and the driver has died, swap seats.
	if(pVehicle->GetSeatManager()->GetPedInSeat(1) == pPed && !pVehicle->GetSeatManager()->GetPedInSeat(0) && (!pVehicle->GetSeatManager()->GetLastPedInSeat(0) || pVehicle->GetSeatManager()->GetLastPedInSeat(0)->GetIsDeadOrDying()))
	{
		tapDebugf3("CTaskAmbulancePatrol::WaitInAmbulance_OnUpdate-->SetState(State_ReturnToAmbulance) 2");
		SetState(State_ReturnToAmbulance);
		return FSM_Continue;
	}

	if(!bWaitForPartner)
	{
		tapDebugf3("CTaskAmbulancePatrol::WaitInAmbulance_OnUpdate-->SetState(State_ChoosePatrol)");
		SetState(State_ChoosePatrol);
		return FSM_Continue;
	}

	return FSM_Continue;
}

//this is disabled for now--it uses the standard "jack ped from vehicle" clips
//which don't really look appropriate for a paramedic pulling an injured person from a vehicle
//we may try and get some custom clips for this if time
#define ALLOW_PULL_VICTIMS_FROM_VEHICLES 0

#if ALLOW_PULL_VICTIMS_FROM_VEHICLES

void CTaskAmbulancePatrol::PullFromVehicle_OnEnter		()
{
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);	

	const COrder* pOrder = GetPed()->GetPedIntelligence()->GetOrder();
	if( aiVerifyf(pOrder && pOrder->GetIsValid(), "PullFromVehicle_OnEnter:: Invalid Order" ) &&
		aiVerifyf(!GetPed()->GetIsInVehicle(), "PullFromVehicle_OnEnter: Not on foot!") &&
		aiVerifyf(pOrder->GetIncident()->GetEntity(), "PullFromVehicle_OnEnter: No entity in incident!"))
	{
		aiAssert(pOrder->GetIncident()->GetEntity()->GetIsTypePed());
		const CPed* pVictim = static_cast<const CPed*>(pOrder->GetIncident()->GetEntity());
		CVehicle* pVictimVehicle = pVictim->GetVehiclePedInside();
		aiAssert(pVictimVehicle);
		const s32 iFlags = RF_ResumeIfInterupted|RF_JustPullPedOut;
		CTaskEnterVehicle* pTask = rage_new CTaskEnterVehicle(pVictimVehicle, SR_Prefer, pVictimVehicle->GetPedsSeatIndex(pVictim), iFlags);

		SetNewTask(pTask);
	}
}

CTask::FSM_Return CTaskAmbulancePatrol::PullFromVehicle_OnUpdate	()
{
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);	

	if (!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_ApproachInjuredPed);
	}

	//bail out if our dude somehow ends up outside the vehicle
	CAmbulanceOrder* pOrder = HelperGetAmbulanceOrder();
	if (pOrder && pOrder->GetIncident()->GetEntity())
	{	
		const CPed* pVictim = static_cast<const CPed*>(pOrder->GetIncident()->GetEntity());
		if (!pVictim->GetIsInVehicle())
		{
			SetState(State_ApproachInjuredPed);
		}
	}

	return FSM_Continue;
}

#else

//otherwise just run toward the car and then leave--the player
//should get the impression that we went to look at the victim in the car
//and decided against healing.  maybe request some dialog for this if 
//it doesn't look like we're going to get the clips

void CTaskAmbulancePatrol::PullFromVehicle_OnEnter		()
{
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);	

	const COrder* pOrder = GetPed()->GetPedIntelligence()->GetOrder();
	if( pOrder && pOrder->GetIsValid()  &&
		aiVerifyf(!GetPed()->GetIsInVehicle(), "PullFromVehicle_OnEnter: Not on foot!") )
	{
		Vector3 vLocation = pOrder->GetIncident()->GetEntity() ? VEC3V_TO_VECTOR3(pOrder->GetIncident()->GetEntity()->GetTransform().GetPosition())
			: pOrder->GetIncident()->GetLocation();
		CTask* pNewSubtask = rage_new CTaskMoveFollowNavMesh(MOVEBLENDRATIO_RUN, 
			vLocation,		
			4.0f, 
			6.0f, 
			-1,
			true,
			false,
			NULL);
		SetNewTask(rage_new CTaskComplexControlMovement(pNewSubtask));
	}
}

CTask::FSM_Return CTaskAmbulancePatrol::PullFromVehicle_OnUpdate	()
{
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);	

	if (GetPed()->IsNetworkClone())
		return FSM_Continue;

	CAmbulanceOrder* pOrder = HelperGetAmbulanceOrder();
	if( pOrder ==NULL )
	{
		//try and go back, we'll go back to patrol
		//when the enter vehicle task either succeeds or fails
		tapDebugf3("CTaskAmbulancePatrol::PullFromVehicle_OnUpdate-->SetState(State_ReturnToAmbulance) 1");
		SetState(State_ReturnToAmbulance);
		return FSM_Continue;
	}

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished) || GetSubTask() == NULL )
	{
		tapDebugf3("CTaskAmbulancePatrol::PullFromVehicle_OnUpdate-->SetState(State_ReturnToAmbulance) 2");
		SetState(State_ReturnToAmbulance);
		pOrder->SetIncidentHasBeenAddressed(true);
		return FSM_Continue;
	}

	return FSM_Continue;
}

#endif //ALLOW_PULL_VICTIMS_FROM_VEHICLES

CAmbulanceOrder* CTaskAmbulancePatrol::HelperGetAmbulanceOrder() 
{
	Assert(GetPed());
	Assert(GetPed()->GetPedIntelligence());
	COrder* pOrder = GetPed()->GetPedIntelligence()->GetOrder();
	if (pOrder && pOrder->GetType() == COrder::OT_Ambulance && pOrder->GetIsValid())
	{
		return static_cast<CAmbulanceOrder*>(pOrder);
	}

	return NULL;
}

const CAmbulanceOrder* CTaskAmbulancePatrol::HelperGetAmbulanceOrder() const
{
	Assert(GetPed());
	Assert(GetPed()->GetPedIntelligence());
	const COrder* pOrder = GetPed()->GetPedIntelligence()->GetOrder();
	if (pOrder && pOrder->GetType() == COrder::OT_Ambulance && pOrder->GetIsValid())
	{
		return static_cast<const CAmbulanceOrder*>(pOrder);
	}

	return NULL;
}

//check if the ped doing the healing is still alive and kicking,
//and greedily try to assume reviving duties if this turns out to not be the case
bool CTaskAmbulancePatrol::UpdateCheckForNewReviver()
{
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);	

	bool bReturn = false;

	if (fwTimer::GetTimeInMilliseconds() > m_iNextTimeToCheckForNewReviver)
	{
		const CAmbulanceOrder* pOrder = HelperGetAmbulanceOrder();

		//only do this if we have a valid injury incident--not for scripted
		if( pOrder && pOrder->GetInjuryIncident())
		{
			const CPed* pReviver = pOrder->GetRespondingPed();
			if (!pReviver || pReviver->IsFatallyInjured())
			{
				tapDebugf3("CTaskAmbulancePatrol::UpdateCheckForNewReviver--!pReviver || pReviver->IsFatallyInjured --> return true");
				m_bShouldApproachInjuredPed = true;
				bReturn = true;
			}
		}
		m_iNextTimeToCheckForNewReviver = fwTimer::GetTimeInMilliseconds() + s_TimeBetweenReviverSearches;
	}

	return bReturn;
}

bool CTaskAmbulancePatrol::CheckBodyCanBeReached(CPed* pPed)
{
	tapDebugf3("Frame %d : Ped %p %s : body[%p:%s] : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", pPed, BANK_ONLY(pPed ? pPed->GetDebugName() : ) "", __FUNCTION__);	

	if(!pPed)
	{
		tapDebugf3("CTaskAmbulancePatrol::CheckBodyCanBeReached--!pPed-->return false");
		return false;
	}

	//Check a sphere at the two positions the paramedics will stand
	//Calculate world position
	const float fOffsetDist = 0.75f;
	Matrix34 mWorldBoneMatrix;
	pPed->GetGlobalMtx(pPed->GetBoneIndexFromBoneTag(BONETAG_PELVIS), mWorldBoneMatrix);

	Vector3 vOffsetWorldSpace = mWorldBoneMatrix.c;
	vOffsetWorldSpace.z = 0.0f;
	vOffsetWorldSpace.NormalizeSafe(ORIGIN);
	vOffsetWorldSpace.Scale(fOffsetDist);

	//One side of the body
	const Vector3 vHeightOffset(0.0f,0.0f,0.4f);
	Vector3 vStart = mWorldBoneMatrix.d - vOffsetWorldSpace + vHeightOffset;	

	//Check the other side
	Vector3 vEnd = mWorldBoneMatrix.d + vOffsetWorldSpace + vHeightOffset;	

	const float fPedCapsuleRadius = 0.35f;
	WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
	WorldProbe::CShapeTestFixedResults<> capsuleResult;
	capsuleDesc.SetResultsStructure(&capsuleResult);
	capsuleDesc.SetCapsule(vStart, vEnd, fPedCapsuleRadius);
	capsuleDesc.SetIsDirected(false);
	capsuleDesc.SetDoInitialSphereCheck(false);
	capsuleDesc.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_MOVER | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE);
	capsuleDesc.SetTypeFlags(ArchetypeFlags::GTA_AI_TEST);
	capsuleDesc.SetContext(WorldProbe::LOS_Camera);
	capsuleDesc.SetExcludeEntity(pPed);

	bool bFoundObstruction = WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc);

#if __BANK
	if (CDispatchManager::GetInstance().DebugEnabled(DT_AmbulanceDepartment) || PARAM_debugtaskambulancepatrol.Get())
	{
		grcDebugDraw::Capsule(VECTOR3_TO_VEC3V(vStart), VECTOR3_TO_VEC3V(vEnd), fPedCapsuleRadius,bFoundObstruction ? Color_red : Color_green,false, 10000);
	}
#endif

	if (bFoundObstruction)
	{	
		tapDebugf3("CTaskAmbulancePatrol::CheckBodyCanBeReached--bFoundObstruction-->return false");
		return false;
	}

	
	return true;
}

bool CTaskAmbulancePatrol::HasBodyMoved(CPed* pPed)
{
	tapDebugf3("Frame %d : Ped %p %s : body[%p:%s] : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", pPed, BANK_ONLY(pPed ? pPed->GetDebugName() : ) "", __FUNCTION__);	

	if(!pPed)
	{
		tapDebugf3("CTaskAmbulancePatrol::HasBodyMoved--!pPed-->return true");
		return true;
	}

	Matrix34 mWorldBoneMatrix;
	pPed->GetGlobalMtx(pPed->GetBoneIndexFromBoneTag(BONETAG_PELVIS), mWorldBoneMatrix);

	if((mWorldBoneMatrix.d).Dist2(m_vBodyPosition) > 0.2f)
	{
		tapDebugf3("CTaskAmbulancePatrol::HasBodyMoved--Dist2 > 0.2f-->return true");
		return true;
	}

	return false;
}

void CTaskAmbulancePatrol::OnCloneTaskNoLongerRunningOnOwner()
{
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);
	SetStateFromNetwork(State_Finish);
}

CTaskInfo* CTaskAmbulancePatrol::CreateQueriableState() const 
{
	bool bEntityFinishedWithIncident = false;
	bool bIncidentHasBeenAddressed = false;

	COrder* pOrder = GetPed()->GetPedIntelligence()->GetOrder();
	if( pOrder && pOrder->GetIsValid() )
	{
		bEntityFinishedWithIncident = pOrder->GetEntityFinishedWithIncident();
	
		if (pOrder->GetIncident())
			bIncidentHasBeenAddressed = pOrder->GetIncident()->GetHasBeenAddressed();
	}

	NOTFINAL_ONLY(tapDebugf3("CTaskAmbulancePatrol::CreateQueriableState pPed[%p][%s][%s] GetState[%s] pOrder[%p] pOrder->GetIsValid[%d] bEntityFinishedWithIncident[%d] bIncidentHasBeenAddressed[%d]",GetPed(),GetPed() ? GetPed()->GetModelName() : "",GetPed() && GetPed()->GetNetworkObject() ? GetPed()->GetNetworkObject()->GetLogName() : "",GetStaticStateName(GetState()),pOrder,pOrder ? pOrder->GetIsValid() : false,bEntityFinishedWithIncident,bIncidentHasBeenAddressed);)
	return rage_new CClonedAmbulancePatrolInfo(GetState(), bEntityFinishedWithIncident, bIncidentHasBeenAddressed);
}

void CTaskAmbulancePatrol::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo) 
{
	if (!pTaskInfo)
		return;

	// Call the base implementation - syncs the state
	CTaskFSMClone::ReadQueriableState(pTaskInfo);

	CClonedAmbulancePatrolInfo* pAmbulancePatrolInfo = SafeCast(CClonedAmbulancePatrolInfo, pTaskInfo);

	NOTFINAL_ONLY(tapDebugf3("CTaskAmbulancePatrol::ReadQueriableState pPed[%p] statefromnetwork[%s] m_bEntityFinishedWithIncident[%d] m_bIncidentHasBeenAddressed[%d]",GetEntity() ? GetPed() : NULL,GetStaticStateName(GetStateFromNetwork()),pAmbulancePatrolInfo->m_bEntityFinishedWithIncident,pAmbulancePatrolInfo->m_bIncidentHasBeenAddressed);)

	if (pAmbulancePatrolInfo->m_bEntityFinishedWithIncident)
	{
		MarkPedAsFinishedWithOrder();
	}

	if (pAmbulancePatrolInfo->m_bIncidentHasBeenAddressed)
	{
		CPed* pPed = GetEntity() ? GetPed() : NULL;
		COrder* pOrder = pPed && pPed->GetPedIntelligence() ? pPed->GetPedIntelligence()->GetOrder() : NULL;
		if( pOrder && pOrder->GetIsValid() && pOrder->GetIncident() )
			pOrder->GetIncident()->SetHasBeenAddressed(true);
	}
}

rage::aiTask::FSM_Return CTaskAmbulancePatrol::UpdateClonedFSM(const s32 iState, const FSM_Event iEvent)
{
#if __BANK
	DebugAmbulancePosition();
#endif

	if(iState != State_Finish)
	{
		if(iEvent == OnUpdate)
		{
			s32 stateFromNetwork = GetStateFromNetwork();

			if(stateFromNetwork != GetState())
			{
				if(stateFromNetwork != -1)
				{
					bool bProceed = false;
					if ((stateFromNetwork == State_GoToIncidentLocationInVehicle) || (stateFromNetwork == State_GoToIncidentLocationAsPassenger) || (stateFromNetwork == State_GoToIncidentLocationOnFoot))
					{
						const COrder* pOrder = HelperGetAmbulanceOrder();
						if (pOrder && pOrder->GetIsValid())
						{
							bProceed = true;
						}
					}
					else
					{
						bProceed = true;
					}

					if (bProceed)
					{
						NOTFINAL_ONLY(tapDebugf3("CTaskAmbulancePatrol::UpdateClonedFSM --> SetState %s",GetStaticStateName(stateFromNetwork));)
						SetState(stateFromNetwork);
						return FSM_Continue;
					}
				}
			}

			Assert(GetState() == iState);
			UpdateClonedSubTasks(GetPed(), GetState());
		}
	}

FSM_Begin

	FSM_State(State_Finish)
		FSM_OnUpdate
			return FSM_Quit;

	FSM_State(State_PullFromVehicle)
		FSM_OnEnter
			PullFromVehicle_OnEnter();
		FSM_OnUpdate
			return PullFromVehicle_OnUpdate();

	FSM_State(State_ExitVehicleAndChooseResponse)
		FSM_OnEnter
			ExitVehicleAndChooseResponse_OnEnter();
		FSM_OnUpdate
			return ExitVehicleAndChooseResponse_OnUpdate();

	FSM_State(State_PlayRecoverySequence)
		FSM_OnEnter
			PlayRecoverySequence_OnEnter();
		FSM_OnUpdate
			return PlayRecoverySequence_OnUpdate();
		FSM_OnExit
			PlayRecoverySequence_OnExit();

	FSM_State(State_ReturnToAmbulance)
		FSM_OnEnter
			ReturnToAmbulance_OnEnter();
		FSM_OnUpdate
			return ReturnToAmbulance_OnUpdate();

	FSM_State(State_SuperviseRecovery)
		FSM_OnEnter
			SuperviseRecovery_OnEnter();
		FSM_OnUpdate
			return SuperviseRecovery_OnUpdate();
		FSM_OnExit
			SuperviseRecovery_OnExit();

	FSM_State(State_ReviveFailed)
		FSM_OnEnter
			ReviveFailed_OnEnter();
		FSM_OnUpdate
			return ReviveFailed_OnUpdate();

	FSM_State(State_WaitForBothMedicsToFinish)
		FSM_OnEnter
			WaitForBothMedicsToFinish_OnEnter();
		FSM_OnUpdate
			return WaitForBothMedicsToFinish_OnUpdate();

	FSM_State(State_WaitInAmbulance)
		FSM_OnEnter
			WaitInAmbulance_OnEnter();
		FSM_OnUpdate
			return WaitInAmbulance_OnUpdate();

	FSM_State(State_Start)
	FSM_State(State_Resume)
	FSM_State(State_Unalerted)
	FSM_State(State_ChoosePatrol)
	FSM_State(State_RespondToOrder)
	FSM_State(State_PatrolInVehicle)
	FSM_State(State_PatrolAsPassenger)
	FSM_State(State_PatrolOnFoot)
	FSM_State(State_GoToIncidentLocationInVehicle)
	FSM_State(State_StopVehicle)
	FSM_State(State_GoToIncidentLocationOnFoot)
	FSM_State(State_GoToIncidentLocationAsPassenger)
	FSM_State(State_HealVictim)
	FSM_State(State_TransportVictim)
	FSM_State(State_TransportVictimAsPassenger)
		return FSM_Continue;

FSM_End
}

void CTaskAmbulancePatrol::UpdateClonedSubTasks(CPed* pPed, int const iState)
{
	CTaskTypes::eTaskType expectedTaskType = CTaskTypes::TASK_INVALID_ID;
	switch(iState)
	{
		case State_GoToIncidentLocationInVehicle:
		case State_StopVehicle:
			{
				expectedTaskType = CTaskTypes::TASK_CONTROL_VEHICLE;
			}
			break;

		case State_ExitVehicleAndChooseResponse:
			{
				expectedTaskType = CTaskTypes::TASK_EXIT_VEHICLE;
			}
			break;

		case State_ReturnToAmbulance:
			{
				expectedTaskType = CTaskTypes::TASK_ENTER_VEHICLE;
			}
			break;

		case State_ReviveFailed: 
		case State_PlayRecoverySequence:
		case State_WaitForBothMedicsToFinish:
		case State_SuperviseRecovery:
			{
				expectedTaskType = CTaskTypes::TASK_USE_SCENARIO; 
			}
			break;
		default:
			return;
	}

	if(expectedTaskType != CTaskTypes::TASK_INVALID_ID)
	{
		 CreateSubTaskFromClone(pPed, expectedTaskType, false);
	}
}

CTaskFSMClone* CTaskAmbulancePatrol::CreateTaskForClonePed(CPed *UNUSED_PARAM(pPed)) 
{
	int iState = GetState();
	NOTFINAL_ONLY(tapDebugf3("Frame %d : Ped %p %s : %s -- GetState[%s]", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__, GetStaticStateName(iState));)

	CTaskUnalerted* pUnalertedTask = m_pUnalertedTask.Get() ? static_cast<CTaskUnalerted*>(m_pUnalertedTask.Get()) : NULL;
	tapDebugf3("pUnalertedTask[%p]",pUnalertedTask);
	CTaskAmbulancePatrol* pTask = rage_new CTaskAmbulancePatrol(pUnalertedTask);
	if (pTask && (iState >= State_Start))
	{
		if ((iState == State_GoToIncidentLocationInVehicle) || (iState == State_GoToIncidentLocationAsPassenger) || (iState == State_GoToIncidentLocationOnFoot))
		{
			const COrder* pOrder = HelperGetAmbulanceOrder();
			if (pOrder && pOrder->GetIsValid())
			{
				pTask->SetState(iState);
			}
		}
		else if (iState == State_Unalerted)
		{
			//This code mimics other areas where transition to unalerted is setup, if the unalerted state has the prerequisite information it proceeds - otherwise let it fall through...

			if (!pTask->m_pUnalertedTask)
			{
				// If there is no unalerted task, the ped may still have a scenario point reserved.
				// We can build an unalerted task from this stored point.
				pTask->AttemptToCreateResumableUnalertedTask();
			}

			if (pTask->m_pUnalertedTask)
			{
				pTask->SetState(State_Unalerted);
			}
		}
		else
		{
			pTask->SetState(iState);
		}
	}

	return pTask;
}

CTaskFSMClone* CTaskAmbulancePatrol::CreateTaskForLocalPed(CPed *pPed)
{
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);	

	return CreateTaskForClonePed(pPed);
}

bool CTaskAmbulancePatrol::GetBestPointToDriveTo(CPed* pPed, Vector3& vLocation)
{
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);	

	//pPed == Paremedic 
	//vLocation = the location of the dead ped
	const Vector3 vPedsPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	CNodeAddress CurrentNode;
	CurrentNode = ThePaths.FindNodeClosestToCoors(vLocation,40.0f);
	if (!CurrentNode.IsEmpty() && ThePaths.IsRegionLoaded(CurrentNode))
	{
		const int iMaxNodesToFindValidPoint = 10;
		for(int i =0; i < iMaxNodesToFindValidPoint; ++i)
		{
			//Ensure the node is valid.
			const CPathNode* pCurrentNode = ThePaths.FindNodePointerSafe(CurrentNode);
			if(!pCurrentNode)
			{
				return false;
			}

			//Grab the node position.
			Vector3 vPosition;
			pCurrentNode->GetCoors(vPosition);

			//Work out the best direction basic off the links
			Vector3 vDirection = vPedsPos - vPosition;
			vDirection.Normalize();

			//Generate the input arguments for the road node search.
			CPathFind::FindNodeInDirectionInput roadNodeInput(CurrentNode, vDirection, 1.0f);

			//Set up the parameters 
			roadNodeInput.m_fMaxDistanceTraveled = 30.0f;
			roadNodeInput.m_bFollowRoad = true;
			roadNodeInput.m_bCanFollowOutgoingLinks = true;
			roadNodeInput.m_bCanFollowIncomingLinks = true;
			roadNodeInput.m_bIncludeSwitchedOffNodes = true;
			roadNodeInput.m_bIncludeDeadEndNodes = true;

			//Find a road node in the road direction.
			CPathFind::FindNodeInDirectionOutput roadNodeOutput;
			if(!ThePaths.FindNodeInDirection(roadNodeInput, roadNodeOutput))
			{
				break;
			}

			//Ensure the node is valid.
			const CPathNode* pNode = ThePaths.FindNodePointerSafe(roadNodeOutput.m_Node);
			if(!pNode)
			{
				break;
			}

			//Make sure the node isn't invalid
			if(pNode->IsWaterNode() || pNode->IsPedNode() || pNode->IsParkingNode() || pNode->IsOpenSpaceNode() || pNode->IsJunctionNode() || pNode->IsTrafficLight())
			{
				CurrentNode = roadNodeOutput.m_Node;
				continue;
			}

			//Make sure it's not near a junction using the links
			bool bNearInvalidNode = false;
			const int iNumLinks = pNode->NumLinks();
			for (int i=0; i < iNumLinks; i++)
			{
				const CPathNodeLink& link = ThePaths.GetNodesLink(pNode, i);
				const CPathNode * pOtherNode = ThePaths.FindNodePointerSafe(link.m_OtherNode);

				if(!pOtherNode || pOtherNode->IsJunctionNode() || pOtherNode->IsTrafficLight())
				{
					bNearInvalidNode = true;
				}
			}

			if(bNearInvalidNode)
			{
				CurrentNode = roadNodeOutput.m_Node;
				continue;
			}

			//Ensure the node is valid.
			const CPathNode* pPreviousNode = ThePaths.FindNodePointerSafe(roadNodeOutput.m_PreviousNode);
			if(!pPreviousNode)
			{
				break;
			}


			//Find the link from the previous node to the node.
			s16 iLink = -1;
			if(!ThePaths.FindNodesLinkIndexBetween2Nodes(pPreviousNode, pNode, iLink))
			{
				break;
			}

			if(iLink == -1 ||  iLink >= pNode->NumLinks())
			{
				break;
			}

			//Get the link.
			const CPathNodeLink& link = ThePaths.GetNodesLink(pNode, iLink);

			//Find the road boundaries.
			bool bAllLanesThoughCentre = (link.m_1.m_Width == ALL_LANES_THROUGH_CENTRE_FLAG_VAL);
			float fRoadWidthOnLeft;
			float fRoadWidthOnRight;
			ThePaths.FindRoadBoundaries(link.m_1.m_LanesToOtherNode, link.m_1.m_LanesFromOtherNode, static_cast<float>(link.m_1.m_Width), link.m_1.m_NarrowRoad, bAllLanesThoughCentre, fRoadWidthOnLeft, fRoadWidthOnRight);

			Vector3 vPositions, vNextNode, vToNext, vRight, vLeft;
			pNode->GetCoors(vNextNode);
			vToNext = vNextNode - vPosition;
			vToNext.Normalize();
			vRight = CrossProduct(ZAXIS, vToNext );
			vRight.Normalize();
			vLeft = CrossProduct( vToNext,ZAXIS );
			vLeft.Normalize();

			//If we have a link node we can use the direction to test the side nodes of the first node we find
			Vector3 vPositionsRight = vPosition + (vRight * fRoadWidthOnRight);
			Vector3 vPositionsLeft = vPosition + (vLeft * fRoadWidthOnLeft);

			float fLeftDistance = (vPositionsLeft - vLocation).Mag2();
			float fRightDistance = (vPositionsRight - vLocation).Mag2();
			if(fLeftDistance < rage::square(5.0f) || fRightDistance < rage::square(5.0f))
			{
				CurrentNode = roadNodeOutput.m_Node;
				continue;
			}

			if(fLeftDistance < fRightDistance)
			{
				vLocation = vPositionsLeft;
				return true;
			}

			vLocation = vPositionsRight;
			return true;
		}

	}

	//There isn't a node within 40m or we have failed to find a good location, just use the closest.
	//The update handles turned off road nodes
	CurrentNode = ThePaths.FindNodeClosestToCoors(vLocation);
	if (!CurrentNode.IsEmpty() && ThePaths.IsRegionLoaded(CurrentNode))
	{
		//Ensure the node is valid.
		const CPathNode* pCurrentNode = ThePaths.FindNodePointerSafe(CurrentNode);
		if(pCurrentNode)
		{
			pCurrentNode->GetCoors(vLocation);
		}
	}

	return false;
}

void CTaskAmbulancePatrol::LookAtTarget(CPed* pPed, CEntity* pTarget)
{
	Assert(pTarget);

	pPed->GetIkManager().LookAt(0, pTarget, 10000, pTarget->GetIsTypePed() ? BONETAG_HEAD : BONETAG_INVALID, NULL, LF_FAST_TURN_RATE );
}

void CTaskAmbulancePatrol::CacheScenarioPointToReturnTo()
{
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);	

	//Ensure we are in the correct state.
	if(GetState() != State_Unalerted)
	{
		return;
	}

	//Ensure the sub-task is valid.
	CTask* pSubTask = GetSubTask();
	if(!pSubTask)
	{
		return;
	}

	//Ensure the ped is not in a vehicle.
	const CPed* pPed = GetPed();
	if(pPed->GetIsInVehicle())
	{
		return;
	}

	//Set the point to return to.
	m_pPointToReturnTo = pSubTask->GetScenarioPoint();
	m_iPointToReturnToRealTypeIndex = pSubTask->GetScenarioPointType();
	m_SPC_UseInfo = NULL;
	if (pSubTask->GetTaskType() == CTaskTypes::TASK_UNALERTED)
	{
		CTaskUnalerted* tUA = static_cast<CTaskUnalerted*>(pSubTask);
		m_SPC_UseInfo = tUA->GetScenarioChainUserData();

		// It would be dangerous to leave the pointer set in CTaskUnalerted,
		// since that task will get cleaned up next, and reset the chaining
		// user info. That means that CTaskAmbulancePatrol may have a pointer to deleted
		// data, and if we start up a CTaskUnalerted again when resuming, we would
		// be in a bad spot.
		tUA->SetScenarioChainUserInfo(NULL);
	}
}

void CTaskAmbulancePatrol::AttemptToCreateResumableUnalertedTask()
{
	tapDebugf3("Frame %d : Ped %p %s : %s", fwTimer::GetFrameCount(), GetPed(), BANK_ONLY(GetPed() ? GetPed()->GetDebugName() : ) "", __FUNCTION__);	

	//Check if we have no unalerted task.
	if(!m_pUnalertedTask)
	{
		//Check if we have a point to return to.
		if(m_pPointToReturnTo)
		{
			CPed* pPed = GetPed();

			// initialize with default condition data
			CScenarioCondition::sScenarioConditionData conditionData;
			conditionData.pPed = pPed;

			// if we can return to the cached point
			if (!CTaskUnalerted::CanScenarioBeReused(*m_pPointToReturnTo, conditionData, m_iPointToReturnToRealTypeIndex, m_SPC_UseInfo))
			{
				//cant return to this point so just reset the data ... 
				m_pPointToReturnTo = NULL;
				m_iPointToReturnToRealTypeIndex = -1;
				m_SPC_UseInfo = NULL;
			}

			//Create the unalerted task.
			CTaskUnalerted* task = rage_new CTaskUnalerted(NULL, m_pPointToReturnTo, m_iPointToReturnToRealTypeIndex);
			task->SetScenarioChainUserInfo(m_SPC_UseInfo);
			m_pUnalertedTask = task;
		}
	}
}

CClonedAmbulancePatrolInfo::CClonedAmbulancePatrolInfo(u32 const state, bool bEntityFinishedWithIncident, bool bIncidentHasBeenAddressed)
: m_bEntityFinishedWithIncident(bEntityFinishedWithIncident),
  m_bIncidentHasBeenAddressed(bIncidentHasBeenAddressed)
{
	SetStatusFromMainTaskState(state);
}

CClonedAmbulancePatrolInfo::CClonedAmbulancePatrolInfo()
: m_bEntityFinishedWithIncident(false),
  m_bIncidentHasBeenAddressed(false)
{
}

CTaskFSMClone *CClonedAmbulancePatrolInfo::CreateCloneFSMTask()
{
	CTaskAmbulancePatrol* pTask = rage_new CTaskAmbulancePatrol();

	return pTask;
}

void CClonedAmbulancePatrolInfo::Serialise(CSyncDataBase& serialiser)
{
	CSerialisedFSMTaskInfo::Serialise(serialiser);

	SERIALISE_BOOL(serialiser, m_bEntityFinishedWithIncident, "Entity Finished With Incident");
	SERIALISE_BOOL(serialiser, m_bIncidentHasBeenAddressed,	  "Incident Has Been Addressed");
}

