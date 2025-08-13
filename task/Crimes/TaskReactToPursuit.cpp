//
// Task/Vehicle/TaskEnterVehicle.h
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

// File header
#include "Task/crimes/TaskReactToPursuit.h"

// Rage Headers
#include "math/angmath.h"

// Game Headers
#include "Event/System/EventData.h"
#include "Game/Dispatch/IncidentManager.h"
#include "Peds/Ped.h"
#include "Peds/PedFactory.h"
#include "Peds/PedIntelligence.h"
#include "Task/Crimes/TaskPursueCriminal.h"
#include "task/Combat/TaskNewCombat.h"
#include "Task/Combat/TaskThreatResponse.h"
#include "Task/General/TaskBasic.h"
#include "Task/Movement/TaskGotoPoint.h"
#include "Task/Response/TaskFlee.h"
#include "Task/Service/Police/TaskPolicePatrol.h"
#include "Task/Vehicle/TaskCar.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Task/Vehicle/TaskExitVehicle.h"
#include "vehicleAi/VehicleIntelligence.h"
#include "VehicleAi/Task/TaskVehicleCruise.h"
#include "VehicleAi/Task/TaskVehicleGoTo.h"
#include "vehicleAi/Task/TaskVehiclePark.h"

AI_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////

// Statics
CTaskReactToPursuit::Tunables CTaskReactToPursuit::ms_Tunables;

IMPLEMENT_CRIME_TASK_TUNABLES(CTaskReactToPursuit, 0x3f46fb6c);

////////////////////////////////////////////////////////////////////////////////

CTaskReactToPursuit::CTaskReactToPursuit(CPed* pCop) :
m_bForceFlee(false),
m_bFleeOnFootFromVehicle(false),
m_bPreviousDisablePretendOccupantsCached(false)
{
	AddCop(pCop);
	m_FleeTimer.Unset();
	SetInternalTaskType(CTaskTypes::TASK_REACT_TO_PURSUIT);
}

////////////////////////////////////////////////////////////////////////////////

CTaskReactToPursuit::~CTaskReactToPursuit()
{
	m_pCop.Reset();
}


aiTask* CTaskReactToPursuit::Copy() const
{
	CTaskReactToPursuit* pTask = rage_new CTaskReactToPursuit(m_pCop[0]); 

	for(int i = 1 ; i < m_pCop.GetCount(); ++i)
	{
		if(m_pCop[i])
		{
			pTask->AddCop(m_pCop[i]);
		}
	}

	return pTask;
}
////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
const char * CTaskReactToPursuit::GetStaticStateName( s32 iState )
{
    taskAssert(iState >= State_Start && iState <= State_Finish);

    switch (iState)
	{
		case State_Start:								return "State_Start";
		case State_FleeInVehicle:						return "State_FleeInVehicle";
		case State_LeaveVehicle:						return "State_LeaveVehicle";
		case State_CombatOnFoot:						return "State_CombatOnFoot";
		case State_ReturnToVehicle:						return "State_ReturnToVehicle";
		case State_FleeArea:							return "State_FleeArea";
		case State_Finish:								return "State_Finish";

        default: taskAssert(0); 	
    }
    return "State_Invalid";
}
#endif //!__FINAL

void CTaskReactToPursuit::CleanUp()
{
	GetPed()->SetPedConfigFlag(CPED_CONFIG_FLAG_WillTakeDamageWhenVehicleCrashes, true);
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskReactToPursuit::ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent)
{
	if(pEvent && pEvent->GetEventType() == EVENT_WRITHE)
		return true;

	//Check if the priority is not immediate.
	if(iPriority != ABORT_PRIORITY_IMMEDIATE)
	{
		//Allow abortion for being jacked
		if (pEvent && pEvent->GetEventType() == EVENT_GIVE_PED_TASK)
		{
			if (static_cast<const CEventGivePedTask*>(pEvent)->GetTask() && static_cast<const CEventGivePedTask*>(pEvent)->GetTask()->GetTaskType() == CTaskTypes::TASK_EXIT_VEHICLE)
			{
				return true;
			}
		}

		//Check if we aren't injured.
		if(!GetPed()->IsInjured())
		{
			return false;
		}
	}

	//Call the base class version.
	return CTask::ShouldAbort(iPriority, pEvent);
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskReactToPursuit::ProcessPreFSM()
{	
	if (GetPed())
	{
		GetPed()->SetPedResetFlag( CPED_RESET_FLAG_TaskCullExtraFarAway, true );
		GetPed()->SetPedResetFlag( CPED_RESET_FLAG_CullExtraFarAway, true );
	}

	return FSM_Continue;
}
////////////////////////////////////////////////////////////////////////////////

bool CTaskReactToPursuit::ProcessMoveSignals()
{
	if(GetPed() && GetPed()->GetIsInVehicle())
	{
		GetPed()->GetMyVehicle()->m_nVehicleFlags.bMadDriver = true;
		GetPed()->GetMyVehicle()->m_nVehicleFlags.bPreventBeingDummyUnlessCollisionLoadedThisFrame = true;
	}

	return true;
}

CTask::FSM_Return CTaskReactToPursuit::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
    FSM_Begin
	
		FSM_State(State_Start) 
			FSM_OnUpdate
				return ReactToPursuit_OnUpdate();

		FSM_State(State_FleeInVehicle)
			FSM_OnEnter
				FleeInVehicle_OnEnter();
			FSM_OnUpdate
				return FleeInVehicle_OnUpdate();
			FSM_OnExit
				FleeInVehicle_OnExit();
	
		FSM_State(State_LeaveVehicle)
			FSM_OnEnter
				LeaveVehicle_OnEnter();
			FSM_OnUpdate
				return LeaveVehicle_OnUpdate();

		FSM_State(State_CombatOnFoot)
			FSM_OnEnter
				CombatOnFoot_OnEnter();
			FSM_OnUpdate
				return CombatOnFoot_OnUpdate();

		FSM_State(State_ReturnToVehicle)
			FSM_OnEnter
				ReturnToVehicle_OnEnter();
			FSM_OnUpdate
				return ReturnToVehicle_OnUpdate();

		FSM_State(State_FleeArea)
			FSM_OnEnter
				FleeArea_OnEnter();
			FSM_OnUpdate
				return FleeArea_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

    FSM_End
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskReactToPursuit::ReactToPursuit_OnUpdate()
{
	CPed* pCop = GetBestCop();
	if(!pCop)
	{
		SetState(State_Finish);
		return FSM_Continue;
	}

	if(pCop->GetIsInVehicle())
	{
		CTaskPursueCriminal *pTaskPursueCriminal = (CTaskPursueCriminal*)pCop->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_PURSUE_CRIMINAL);

		//Decided if we should flee or instantly pull over.
		if (!pTaskPursueCriminal)
		{
			//The cop has lost interest with the criminal, could have been killed etc
			SetState(State_Finish);
			return FSM_Continue;
		}		
	}

	RequestProcessMoveSignalCalls();

	GetPed()->SetPedConfigFlag(CPED_CONFIG_FLAG_WillTakeDamageWhenVehicleCrashes, false);

	SetState(State_FleeInVehicle);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskReactToPursuit::FleeInVehicle_OnEnter()
{
	CPed* pCop = GetBestCop();

	if(!pCop)
	{
		return;
	}

	CPed* pPed = GetPed();
	
	pPed->GetPedIntelligence()->GetCombatBehaviour().SetFlag(CCombatData::BF_FleeWhilstInVehicle);
	pPed->GetPedIntelligence()->GetCombatBehaviour().SetCombatFloat(kAttribFloatSpeedToFleeInVehicle, 22.0f);

	//Disable pretend occupants.
	CVehicle* pVehicle = pPed->GetVehiclePedInside();
	if(pVehicle)
	{
		m_bPreviousDisablePretendOccupantsCached = pVehicle->m_nVehicleFlags.bDisablePretendOccupants;
		pVehicle->m_nVehicleFlags.bDisablePretendOccupants = true;
	}

	SetNewTask(rage_new CTaskCombat(pCop));

	m_FleeTimer.Set(fwTimer::GetTimeInMilliseconds(), CTaskReactToPursuit::ms_Tunables.m_MaxTimeToFleeInVehicle*1000);
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskReactToPursuit::FleeInVehicle_OnUpdate()
{
	CPed* pCop = GetBestCop();
	if(!pCop)
	{
		SetState(State_Finish);
		return FSM_Continue;
	}

	CPed* pPed = GetPed();

	if(!pPed->GetIsInVehicle())
	{
		SetState(State_CombatOnFoot);
		return FSM_Continue;
	}

	//Check if we are stuck, at a dead end or wreck, if so flee.
	bool bStuck = GetPed()->GetMyVehicle()->GetVelocity().Mag2() < 10.0f && m_FleeTimer.GetTimeElapsed() > CTaskReactToPursuit::ms_Tunables.m_MinTimeToFleeInVehicle*1000 ; 
	if (!pPed->GetMyVehicle()->IsInDriveableState() || bStuck )
	{
		m_bFleeOnFootFromVehicle = true;
		SetState(State_LeaveVehicle);
		return FSM_Continue;
	}

	if((GetIsFlagSet(aiTaskFlags::SubTaskFinished) || !GetSubTask()))
	{
		SetFlag(aiTaskFlags::RestartCurrentState);
		return FSM_Continue;
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskReactToPursuit::FleeInVehicle_OnExit()
{
	//Re-enable pretend occupants.
	CVehicle* pVehicle = GetPed()->GetMyVehicle();
	if(pVehicle)
	{
		pVehicle->m_nVehicleFlags.bDisablePretendOccupants = m_bPreviousDisablePretendOccupantsCached;
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskReactToPursuit::LeaveVehicle_OnEnter()
{
	CPed* pPed = GetPed();

	CVehicle* pVehicle = pPed->GetVehiclePedInside();
	if (!aiVerify(pVehicle))
	{
		return;
	}

	VehicleEnterExitFlags vehicleFlags;
	vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DontCloseDoor);
	SetNewTask(rage_new CTaskExitVehicle(pVehicle, vehicleFlags));
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskReactToPursuit::LeaveVehicle_OnUpdate()
{
	CPed* pCop = GetBestCop();

	if(!pCop)
	{
		SetState(State_Finish);
		return FSM_Continue;
	}

	if(!GetSubTask() || GetIsSubtaskFinished(CTaskTypes::TASK_EXIT_VEHICLE) )
	{
		SetState(State_CombatOnFoot);
		return FSM_Continue;
	}

	return FSM_Continue;
}


////////////////////////////////////////////////////////////////////////////////

void CTaskReactToPursuit::CombatOnFoot_OnEnter()
{
	CPed* pCop = GetBestCop(true);
	if(!pCop)
	{
		return;
	}

	CPed* pPed = GetPed();

	if (!pPed->GetWeaponManager()->GetIsArmed())
	{
		pPed->GetInventory()->AddWeaponAndAmmo(WEAPONTYPE_PISTOL, CWeapon::LOTS_OF_AMMO);
	}

	//Don't allow the ped to use vehicles.
	pPed->GetPedIntelligence()->GetCombatBehaviour().ClearFlag(CCombatData::BF_CanUseVehicles);

	CTaskThreatResponse* pTaskThreatResponse =  rage_new CTaskThreatResponse(pCop);
	pTaskThreatResponse->SetThreatResponseOverride(CTaskThreatResponse::TR_Fight);
	SetNewTask(pTaskThreatResponse);
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskReactToPursuit::CombatOnFoot_OnUpdate()
{
	CPed* pCop = GetBestCop();
	CPed* pPed = GetPed();
	if(!pCop)
	{
		if(pPed->GetIsInVehicle())
		{
			SetState(State_Finish);
			return FSM_Continue;
		}

		SetState(State_ReturnToVehicle);
		return FSM_Continue;
	}

	if(!GetSubTask() || GetIsSubtaskFinished(CTaskTypes::TASK_THREAT_RESPONSE) )
	{
		GetPed()->GetPedIntelligence()->GetCombatBehaviour().SetFlag(CCombatData::BF_CanUseVehicles);

		if(pPed->GetIsInVehicle())
		{
			SetState(State_FleeArea);
			return FSM_Continue;
		}

		SetState(State_ReturnToVehicle);
		return FSM_Continue;
	}
	return FSM_Continue;
}


////////////////////////////////////////////////////////////////////////////////

void CTaskReactToPursuit::ReturnToVehicle_OnEnter()
{
	CVehicle* pVehicle = GetPed()->GetMyVehicle();
	if(pVehicle && pVehicle->IsInDriveableState())
	{
		VehicleEnterExitFlags vehicleFlags;
		vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::UseDirectEntryOnly);
		vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DontJackAnyone);
		SetNewTask(rage_new CTaskEnterVehicle(pVehicle, SR_Prefer, pVehicle->GetDriverSeat(), vehicleFlags, 0.0f, MOVEBLENDRATIO_RUN));
	}
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskReactToPursuit::ReturnToVehicle_OnUpdate()
{
	if(!GetSubTask() || GetIsSubtaskFinished(CTaskTypes::TASK_ENTER_VEHICLE))
	{
		SetState(State_FleeArea);
		return FSM_Continue;
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskReactToPursuit::FleeArea_OnEnter()
{
	CPed* pFleePed = GetBestCop();
	if(pFleePed)
	{
		CTaskThreatResponse* pTaskThreatResponse =  rage_new CTaskThreatResponse(pFleePed);
		pTaskThreatResponse->SetThreatResponseOverride(CTaskThreatResponse::TR_Flee);
		pTaskThreatResponse->GetConfigFlagsForFlee().SetFlag(CTaskSmartFlee::CF_DisableExitVehicle);
		SetNewTask(pTaskThreatResponse);
	}
	else
	{
		CTaskSmartFlee* pSmartFlee = rage_new CTaskSmartFlee(CAITarget(VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition())));
		pSmartFlee->SetConfigFlags(CTaskSmartFlee::CF_DisableExitVehicle);
		SetNewTask(pSmartFlee);
	}
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskReactToPursuit::FleeArea_OnUpdate()
{
	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished) )
	{
		SetState(State_Finish);
		return FSM_Continue;
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskReactToPursuit::AddCop(CPed* pCop)
{
	taskAssert(m_pCop.GetCount() < m_pCop.GetMaxCount());
	if (m_pCop.GetCount() < m_pCop.GetMaxCount())
	{
		m_pCop.Push(RegdPed(pCop));
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskReactToPursuit::GiveTaskToPed(CPed& rPed, CPed* pCop)
{
	//Assert that the cop is valid.
	taskAssert(pCop);

	//Create the task.
	CTask* pTask = rage_new CTaskReactToPursuit(pCop);

	//Give the task to the ped.
	CEventGivePedTask event(PED_TASK_PRIORITY_PRIMARY, pTask);
	rPed.GetPedIntelligence()->AddEvent(event);
}

////////////////////////////////////////////////////////////////////////////////

CPed* CTaskReactToPursuit::GetBestCop(bool bClosest)
{
	float fClosestDistance = -1.0f;
	int iBestIndex = -1;
	Vector3 vPedPos = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition());
	for(int i = 0 ; i < m_pCop.GetCount(); ++i)
	{
		if(m_pCop[i] && !m_pCop[i]->IsDead())
		{
			if(bClosest)
			{
				float fDist = VEC3V_TO_VECTOR3(m_pCop[i]->GetTransform().GetPosition()).Dist2(vPedPos);
				if( fDist < fClosestDistance || fClosestDistance == -1 )
				{
					fClosestDistance = fDist;
					iBestIndex = i;
				}
			}
			else
			{
				return m_pCop[i];
			}
		}
	}

	if(fClosestDistance != -1)
	{
		return m_pCop[iBestIndex];
	}
	return NULL;
}
