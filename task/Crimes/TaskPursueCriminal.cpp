//
// Task/Vehicle/TaskEnterVehicle.h
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

// File header
#include "Task/crimes/TaskPursueCriminal.h"

// Rage Headers
#include "math/angmath.h"
#include "grcore/debugdraw.h"

// Game Headers
#include "Animation/Move.h"
#include "Animation/MovePed.h"
#include "Event/Events.h"
#include "Game/Dispatch/OrderManager.h"
#include "Game/Dispatch/IncidentManager.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Task/Combat/TaskNewCombat.h"
#include "Task/Combat/TaskThreatResponse.h"
#include "Task/Crimes/TaskReactToPursuit.h"
#include "Task/Motion/Locomotion/TaskMotionAiming.h"
#include "Task/Movement/TaskGoToPointAiming.h"
#include "Task/Movement/TaskMoveFollowEntityOffset.h"
#include "Task/Movement/TaskSeekEntity.h"
#include "Task/Response/TaskFlee.h"
#include "Task/Service/Police/TaskPolicePatrol.h"
#include "Task/Vehicle/TaskCar.h"
#include "Task/Vehicle/TaskCarUtils.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Task/Vehicle/TaskExitVehicle.h"
#include "Task/Vehicle/TaskGoToVehicleDoor.h"
#include "Vehicles/Bike.h"
#include "Vehicles/Train.h"
#include "Vehicles/Vehicle.h"
#include "vehicleAi/pathfind.h"
#include "vehicleAi/Task/TaskVehicleGoTo.h"
#include "vehicleAi/Task/TaskVehiclePark.h"


AI_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////

// Statics
CTaskPursueCriminal::Tunables CTaskPursueCriminal::ms_Tunables;

IMPLEMENT_CRIME_TASK_TUNABLES(CTaskPursueCriminal, 0x35386d44);

////////////////////////////////////////////////////////////////////////////////

CTaskPursueCriminal::CTaskPursueCriminal(CEntity* pPed, bool instantlychase, bool leader ) :
m_pCriminal(pPed),
m_pCriminalVehicle(NULL),
m_Type(RC_INVALID),
m_bInstantlyChase(instantlychase),
m_bLeader(leader)
{
	m_DelayStateChangeTimer.Unset();
	m_PoliceFollowTimer.Unset();
	SetInternalTaskType(CTaskTypes::TASK_PURSUE_CRIMINAL);
}

////////////////////////////////////////////////////////////////////////////////

CTaskPursueCriminal::~CTaskPursueCriminal()
{
	if (m_pArrestIncident)
	{
		CIncidentManager::GetInstance().ClearIncident(static_cast<CArrestIncident*>(m_pArrestIncident.Get()));
	}
}
////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
const char * CTaskPursueCriminal::GetStaticStateName( s32 iState )
{
	taskAssert(iState >= State_Start && iState <= State_Finish);

	switch (iState)
	{
	case State_Start:									return "State_Start";
	case State_ApproachSuspectInVehicle:				return "State_ApproachSuspectInVehicle";
	case State_RespondToFleeInVehicle:					return "State_RespondToFleeInVehicle";
	case State_ReturnToVehicle:							return "State_ReturnToVehicle";
	case State_Finish:									return "State_Finish";

	default: taskAssert(0); 	
	}
	return "State_Invalid";
}
#endif //!__FINAL

////////////////////////////////////////////////////////////////////////////////

bool CTaskPursueCriminal::ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent)
{
	//Check if the priority is not immediate.
	if(iPriority != ABORT_PRIORITY_IMMEDIATE)
	{
		// Allow aborting if we're being jacked
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
			//Check the state.
			if(GetState() == State_RespondToFleeInVehicle)
			{
				return false;
			}
		}
	}

	//Call the base class version.
	return CTask::ShouldAbort(iPriority, pEvent);
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskPursueCriminal::ProcessPreFSM()
{	
#if !__FINAL

	Debug();

#endif //!__FINAL

	//Cull us extra far away.
	CullExtraFarAway(*GetPed());

	//Check if the criminal ped is valid.
	CPed* pCriminal = GetCriminalPed();
	if(pCriminal)
	{
		//Cull the criminal extra far away.
		CullExtraFarAway(*pCriminal);
	}

	//Check if the police vehicle is valid.
	CVehicle* pPoliceVehicle = GetPoliceVehicle(GetPed());
	if(pPoliceVehicle)
	{
		//Cull the police vehicle extra far away.
		CullExtraFarAway(*pPoliceVehicle);
	}

	//Check if the criminal vehicle is valid.
	if(m_pCriminalVehicle)
	{
		//Cull the criminal vehicle extra far away.
		CullExtraFarAway(*m_pCriminalVehicle);
	}

	return FSM_Continue;
}

bool CTaskPursueCriminal::ProcessMoveSignals()
{
	//Check if the police vehicle is valid.
	CVehicle* pPoliceVehicle = GetPoliceVehicle(GetPed());
	if(pPoliceVehicle)
	{
		//Process the flags for the police vehicle.
		ProcessFlags(*pPoliceVehicle);
	}

	//Check if the criminal vehicle is valid.
	if(m_pCriminalVehicle)
	{
		//Process the flags for the criminal vehicle.
		ProcessFlags(*m_pCriminalVehicle);
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskPursueCriminal::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		FSM_State(State_Start) 
			FSM_OnUpdate
				return PursueCriminal_Start_OnUpdate();

		FSM_State(State_ApproachSuspectInVehicle)
			FSM_OnEnter
				ApproachSuspectInVehicle_OnEnter();
			FSM_OnUpdate
				return ApproachSuspectInVehicle_OnUpdate();

		FSM_State(State_RespondToFleeInVehicle)
			FSM_OnEnter
				RespondToFleeInVehicle_OnEnter();
			FSM_OnUpdate
				return RespondToFleeInVehicle_OnUpdate();
			FSM_OnExit
				RespondToFleeInVehicle_OnExit();

		FSM_State(State_ReturnToVehicle)
			FSM_OnEnter
				ReturnToVehicle_OnEnter();
		FSM_OnUpdate
			return ReturnToVehicle_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
			return FSM_Quit;	
	FSM_End
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskPursueCriminal::PursueCriminal_Start_OnUpdate()
{
	//Check that the criminal ped still exists
	if(!m_pCriminal)
	{
		SetState(State_Finish);
		return FSM_Continue;
	}

	//Cache Ped
	CPed* pPed = GetPed();

	if(pPed->GetIsOnFoot() || (GetCriminalPed() && GetCriminalPed()->GetIsOnFoot()))
	{
		SetState(State_Finish);
		return FSM_Continue;
	}

	//Check if the ped is in a vehicle.
	if(pPed->GetIsInVehicle())
	{
		//Check if the criminal is a vehicle.
		if(m_pCriminal->GetIsTypeVehicle())
		{
			//Set the criminal vehicle.
			m_pCriminalVehicle = static_cast<CVehicle*>(m_pCriminal.Get());
		}

		//Check if we are driving the vehicle.
		if(pPed->GetIsDrivingVehicle())
		{
			if(m_bInstantlyChase)
			{
				CPed* pCriminalPed = GetCriminalPed();

				if(!m_bLeader)
				{
					if ( pCriminalPed )
					{
						CTaskReactToPursuit *pTaskReactToPursuit = static_cast<CTaskReactToPursuit*>(pCriminalPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_REACT_TO_PURSUIT));
						
						if(pTaskReactToPursuit)
						{
							pTaskReactToPursuit->AddCop(pPed);
						}
					}
					SetState(State_RespondToFleeInVehicle);
					return FSM_Continue;
				}

				if(pCriminalPed)
				{
					CVehicle* pVehicle = pCriminalPed->GetMyVehicle();
					if(pVehicle)
					{
						for (s32 i=0; i<pVehicle->GetSeatManager()->GetMaxSeats(); i++)
						{
							CPed* pSeatedPed = pVehicle->GetSeatManager()->GetPedInSeat(i);

							if (pSeatedPed)
							{
								//Signal to the criminal to react
								CTask* pTaskToAdd = rage_new CTaskReactToPursuit(pSeatedPed);
								CEventGivePedTask event(PED_TASK_PRIORITY_PRIMARY,pTaskToAdd);
								pSeatedPed->GetPedIntelligence()->AddEvent(event);
							}
						}
					}

					pPed->GetMyVehicle()->TurnSirenOn(true);

					SetState(State_RespondToFleeInVehicle);

					m_Type = RC_COP_PURSUE_VEHICLE_FLEE_SPAWNED;
					CRandomEventManager::GetInstance().SetEventStarted(RC_COP_PURSUE_VEHICLE_FLEE_SPAWNED);
					return FSM_Continue;
				}				
			}
			
			m_Type = RC_COP_PURSUE;
			CRandomEventManager::GetInstance().SetEventStarted(RC_COP_PURSUE);
			
			SetState(State_RespondToFleeInVehicle);
			return FSM_Continue;
		}
		else
		{
			SetState(State_RespondToFleeInVehicle);
			return FSM_Continue;
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskPursueCriminal::ApproachSuspectInVehicle_OnEnter()
{
	//Check that the criminal ped still exists
	if(!m_pCriminal)
	{
		return;
	}

	CPed* pPed = GetPed();

	//Make sure the siren is off
	pPed->GetMyVehicle()->TurnSirenOn(false);

	sVehicleMissionParams params;
	params.SetTargetEntity(m_pCriminal);
	params.m_fCruiseSpeed = 20.0f;
	params.m_iDrivingFlags = DMode_AvoidCars;
	CTaskVehicleFollow* pTask = rage_new CTaskVehicleFollow(params, CTaskPursueCriminal::ms_Tunables.m_DistanceToFollowVehicleBeforeFlee);
	SetNewTask( rage_new CTaskControlVehicle(pPed->GetMyVehicle(), pTask ) );

	//Start timer
	m_PoliceFollowTimer.Set(fwTimer::GetTimeInMilliseconds(),CTaskPursueCriminal::ms_Tunables.m_TimeToSignalVehiclePursuitToCriminalMax*1000);
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskPursueCriminal::ApproachSuspectInVehicle_OnUpdate()
{
	//Check that the criminal ped still exists
	if(!m_pCriminal)
	{
		SetState(State_Finish);
		return FSM_Continue;
	}

	CPed* pPed = GetPed();

	if (m_DelayStateChangeTimer.IsSet())
	{
		if (m_DelayStateChangeTimer.IsOutOfTime())
		{
			return FSM_Quit;
		}
	}

	bool startFlee = false;
	if (GetDistanceToCriminalVehicle() < CTaskPursueCriminal::ms_Tunables.m_DistanceToSignalVehiclePursuitToCriminal && 
		m_PoliceFollowTimer.GetTimeElapsed() > CTaskPursueCriminal::ms_Tunables.m_TimeToSignalVehiclePursuitToCriminalMin*1000)
	{
		startFlee = true;
	}

	if( startFlee )
	{
		CPed* pCriminalPed = GetCriminalPed();
		CTaskReactToPursuit *pTaskReactToPursuit = NULL;

		if ( pCriminalPed )
		{
			pTaskReactToPursuit = (CTaskReactToPursuit*)pCriminalPed->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_REACT_TO_PURSUIT);
		}
		else if (m_pCriminal->GetIsTypeVehicle() )
		{
			if (m_pCriminalVehicle)
			{
				//Force peds to spawn in vehicle 
				m_pCriminalVehicle->TryToMakePedsFromPretendOccupants();
				return FSM_Continue;
			}
		}

		if (pCriminalPed)
		{
			if(pTaskReactToPursuit == NULL)
			{

				CVehicle* pVehicle = pCriminalPed->GetMyVehicle();
				if(pVehicle)
				{
					for (s32 i=0; i<pVehicle->GetSeatManager()->GetMaxSeats(); i++)
					{
						CPed* pSeatedPed = pVehicle->GetSeatManager()->GetPedInSeat(i);

						if (pSeatedPed)
						{
							//Signal to the criminal to react
							CTask* pTaskToAdd = rage_new CTaskReactToPursuit(pSeatedPed);
							CEventGivePedTask event(PED_TASK_PRIORITY_PRIMARY,pTaskToAdd);
							pSeatedPed->GetPedIntelligence()->AddEvent(event);
						}
					}
				}
			}

			SetState(State_RespondToFleeInVehicle);

			//Turn on the sirens
			pPed->GetMyVehicle()->TurnSirenOn(true);

			//Stop the timer
			m_PoliceFollowTimer.Unset();
			return FSM_Continue;
		}

		return FSM_Continue;
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskPursueCriminal::RespondToFleeInVehicle_OnEnter()
{
	//Ensure the criminal ped is valid.
	CPed* pCriminalPed = GetCriminalPed();
	if(!pCriminalPed)
	{
		return;
	}

	//Check if we are driving the vehicle.
	if(GetPed()->GetIsDrivingVehicle())
	{
		//Turn on the siren.
		GetPed()->GetMyVehicle()->TurnSirenOn(true);
	}

	//Never lose the target.
	GetPed()->GetPedIntelligence()->GetCombatBehaviour().SetTargetLossResponse(CCombatData::TLR_NeverLoseTarget);

	//Create the task.
	CTaskCombat* pTask = rage_new CTaskCombat(pCriminalPed);

	//Start the task.
	SetNewTask(pTask);
}
////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskPursueCriminal::RespondToFleeInVehicle_OnUpdate()
{
	//Check if the sub-task has finished.
	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Check if we are in a vehicle.
		if(GetPed()->GetIsInVehicle())
		{
			//Check if we are driving the vehicle.
			if(GetPed()->GetIsDrivingVehicle())
			{
				//Turn the siren off.
				GetPed()->GetMyVehicle()->TurnSirenOn(false);
			}
			
			//Finish the task.
			SetState(State_Finish);
		}
		else
		{
			//Return to our vehicle.
			SetState(State_ReturnToVehicle);
		}
	}
	else
	{
		//Check the criminal tasks.
		CheckCriminalTasks();

		//Check the player audio.
		CheckPlayerAudio(GetPed());
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskPursueCriminal::RespondToFleeInVehicle_OnExit()
{
	//Set the default target loss response.
	CCombatBehaviour::SetDefaultTargetLossResponse(*GetPed());
}

////////////////////////////////////////////////////////////////////////////////

void CTaskPursueCriminal::CheckCriminalTask(CPed& rCriminal)
{
	//Ensure the task does not exist.
	if(rCriminal.IsPlayer() || rCriminal.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_REACT_TO_PURSUIT) != NULL)
	{
		return;
	}

	//Give the task to the criminal.
	CTaskReactToPursuit::GiveTaskToPed(rCriminal, GetPed());
}

////////////////////////////////////////////////////////////////////////////////

void CTaskPursueCriminal::CheckCriminalTasks()
{
	//Get the criminal ped.
	CPed* pCriminalPed = GetCriminalPed();

	//Check if the criminal vehicle is valid.
	if(m_pCriminalVehicle)
	{
		//Check if the criminal vehicle is using pretend occupants.
		if(m_pCriminalVehicle->m_nVehicleFlags.bUsingPretendOccupants)
		{
			//Try to make some peds.
			m_pCriminalVehicle->TryToMakePedsFromPretendOccupants();
		}

		//Iterate over the seats.
		for(int i = 0; i < m_pCriminalVehicle->GetSeatManager()->GetMaxSeats(); ++i)
		{
			//Check if the criminal in the seat is valid.
			CPed* pCriminalInSeat = m_pCriminalVehicle->GetSeatManager()->GetPedInSeat(i);
			if(pCriminalInSeat && (pCriminalInSeat != pCriminalPed))
			{
				//Check the task.
				CheckCriminalTask(*pCriminalInSeat);
			}
		}
	}

	//Check if the criminal ped is valid.
	if(pCriminalPed)
	{
		//Check the task.
		CheckCriminalTask(*pCriminalPed);
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskPursueCriminal::CheckPlayerAudio(CPed* pPed)
{
	if(!pPed->GetIsInVehicle() || !pPed->GetMyVehicle()->InheritsFromAutomobile() || pPed->GetMyVehicle()->GetVelocity().XYMag2() < rage::square(5.0f))
	{
		return;
	}

	CPed* pPlayerPed = FindPlayerPed();
	if (!pPlayerPed)
	{
		return;
	}
	
	float fDistanceSq = (VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition())).Mag2();
	if(fDistanceSq > rage::square(20.0f))
	{
		return;
	}

	if(pPlayerPed->GetIsInVehicle())
	{
		float fRelativeVelSq = (pPed->GetMyVehicle()->GetVelocity() - pPlayerPed->GetMyVehicle()->GetVelocity()).Mag2();
		if( fRelativeVelSq < rage::square(20.0f))
		{
			return;
		}

		if(pPlayerPed->GetVehiclePedInside()->GetNumberOfPassenger() <= 0)
		{
			return;
		}
	}
	else
	{
		static dev_float s_fMaxDistance = 15.0f;

		//Check if the player is in a group.
		const CPedGroup* pPedGroup = pPlayerPed->GetPedsGroup();
		if(!pPedGroup || (pPedGroup->FindDistanceToNearestMember() > s_fMaxDistance))
		{
			if(!CFindClosestFriendlyPedHelper::Find(*pPlayerPed, s_fMaxDistance))
			{
				return;
			}
		}
	}

	if(!pPed->GetIsVisibleInSomeViewportThisFrame())
	{
		return;
	}

	pPlayerPed->NewSay("POLICE_PURSUIT_FALSE_ALARM");
}

void CTaskPursueCriminal::CullExtraFarAway(CPed& rPed)
{
	rPed.SetPedResetFlag(CPED_RESET_FLAG_TaskCullExtraFarAway, true);
	rPed.SetPedResetFlag(CPED_RESET_FLAG_CullExtraFarAway, true);
}

void CTaskPursueCriminal::CullExtraFarAway(CVehicle& rVehicle)
{
	for(int i = 0; i < rVehicle.GetSeatManager()->GetMaxSeats(); ++i)
	{
		CPed* pPed = rVehicle.GetSeatManager()->GetPedInSeat(i);
		if(pPed)
		{
			CullExtraFarAway(*pPed);
		}
	}
}

void CTaskPursueCriminal::ProcessFlags(CVehicle& rVehicle)
{
	rVehicle.m_nVehicleFlags.bDisablePretendOccupants = true;
	rVehicle.m_nVehicleFlags.bForceOtherVehiclesToStopForThisVehicle = true;
	rVehicle.m_nVehicleFlags.bPreventBeingDummyUnlessCollisionLoadedThisFrame = true;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskPursueCriminal::ReturnToVehicle_OnEnter()
{
	CVehicle* pCopVehicle = GetPoliceVehicle(GetPed());
	if(pCopVehicle)
	{
		VehicleEnterExitFlags vehicleFlags;
		vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::UseDirectEntryOnly);
		vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DontJackAnyone);
		SetNewTask(rage_new CTaskEnterVehicle(pCopVehicle, SR_Specific,pCopVehicle->GetDriverSeat(), vehicleFlags, 0.0f, MOVEBLENDRATIO_WALK));
	}
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskPursueCriminal::ReturnToVehicle_OnUpdate()
{
	if(GetIsSubtaskFinished(CTaskTypes::TASK_ENTER_VEHICLE))
	{
		CVehicle* pCopVehicle = GetPoliceVehicle(GetPed());
		if(pCopVehicle)
		{
			pCopVehicle->TurnSirenOn(false);
		}
		SetState(State_Finish);
		return FSM_Continue;
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CPed* CTaskPursueCriminal::GetCriminalPed()
{
	if(m_pCriminal)
	{
		if (m_pCriminal->GetIsTypeVehicle())
		{
			CVehicle* pCriminalVeh = static_cast<CVehicle*>(m_pCriminal.Get());
			if (pCriminalVeh)
			{
				if (pCriminalVeh->GetDriver())
				{
					return pCriminalVeh->GetDriver();
				}
				else
				{
					return pCriminalVeh->GetSeatManager()->GetLastPedInSeat(0);
				}
			}
		}
		else
		{
			Assertf(m_pCriminal->GetIsTypePed(), "If the criminal isn't a vehicle it should be a ped!");
			return static_cast<CPed*>(m_pCriminal.Get());
		}
	}

	return NULL;
}

CVehicle* CTaskPursueCriminal::GetPoliceVehicle(CPed* pPed)
{
	if (pPed)
	{
		return pPed->GetMyVehicle();
	}
	return NULL;
}

void CTaskPursueCriminal::GiveTaskToPed(CPed& rPed, CEntity* pCriminal, bool bInstantlyChase, bool bLeader)
{
	//Assert that the ped is law enforcement.
	taskAssert(rPed.IsLawEnforcementPed());

	//Assert that the criminal is valid.
	taskAssert(pCriminal);

	//Create the task.
	CTaskPursueCriminal* pTask = rage_new CTaskPursueCriminal(pCriminal, bInstantlyChase, bLeader);

	//Give the task to the ped.
	CEventGivePedTask event(PED_TASK_PRIORITY_PRIMARY, pTask);
	rPed.GetPedIntelligence()->AddEvent(event);
}

////////////////////////////////////////////////////////////////////////////////

float CTaskPursueCriminal::GetDistanceToCriminalVehicle()
{
	taskAssert(m_pCriminal);
	taskAssert(m_pCriminal->GetIsTypeVehicle());

	CVehicle* pVehicle = static_cast<CVehicle*>(m_pCriminal.Get());
	const Vector3 vTargetToUse = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
	const Vector3 vDiff = vTargetToUse - VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition());
	return vDiff.Mag();
}

////////////////////////////////////////////////////////////////////////////////

#if !__FINAL

// PURPOSE: Display debug information specific to this task
void CTaskPursueCriminal::Debug() const
{
#if __DEV
	if (!CTaskPursueCriminal::ms_Tunables.m_DrawDebug)
	{
		return;
	}

	const CPed* pPed = GetPed();

	if (!pPed)
	{
		return;
	}

	CVehicle* pCriminalVehicle = NULL;
	CPed* pCriminalPed = NULL;
	if ( m_pCriminal && m_pCriminal->GetIsTypeVehicle() )
	{
		pCriminalVehicle = static_cast<CVehicle*>(m_pCriminal.Get());
		if (pCriminalVehicle)
		{
			pCriminalPed = pCriminalVehicle->GetDriver();
		}
	}
	else if (m_pCriminal && m_pCriminal->GetIsTypePed() )
	{
		pCriminalPed = static_cast<CPed*>(m_pCriminal.Get());
	}

	if ( pCriminalPed && pCriminalVehicle)
	{
		const Vector3 vVehicle = VEC3V_TO_VECTOR3(pCriminalVehicle->GetTransform().GetPosition());
		const Vector3 vPed = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

		CTaskReactToPursuit *pTaskReactToPursuit = pTaskReactToPursuit = (CTaskReactToPursuit*)pCriminalPed->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_REACT_TO_PURSUIT);
		if ( pTaskReactToPursuit )
		{
			grcDebugDraw::Line(vVehicle, vPed, Color_purple, Color_purple);
		}
		else
		{
			grcDebugDraw::Line(vVehicle, vPed, Color_green, Color_green);
		}
	}
	else if ( pCriminalVehicle)
	{
		const Vector3 vVehicle = VEC3V_TO_VECTOR3(pCriminalVehicle->GetTransform().GetPosition());
		const Vector3 vPed = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

		grcDebugDraw::Line(vVehicle, vPed, Color_red, Color_red);
	}	
	else if ( pCriminalPed )
	{
		const Vector3 vVehicle = VEC3V_TO_VECTOR3(pCriminalPed->GetTransform().GetPosition());
		const Vector3 vPed = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

		grcDebugDraw::Line(vVehicle, vPed, Color_blue, Color_blue);
	}


	CVehicle* pVeh = pPed->GetMyVehicle();

	if (!pVeh || !pCriminalVehicle || !pVeh)
	{
		return;
	}

	Vector3 vPedPos = VEC3V_TO_VECTOR3(pVeh->GetTransform().GetPosition());
	const Vector3 vVehiclePos = VEC3V_TO_VECTOR3(pCriminalVehicle->GetTransform().GetPosition());
	float fDistToPed = (vPedPos - vVehiclePos).Mag();

	//Height check
	float fHeightDiff = Abs(vPedPos.z - vVehiclePos.z);

	//Direction check - don't jack a vehicle if the ped has to turn around
	Vector3 vPedVehicle = VEC3V_TO_VECTOR3(pCriminalVehicle->GetTransform().GetForward());
	vPedVehicle.Normalize();
	Vector3 vPedForward = VEC3V_TO_VECTOR3(pVeh->GetTransform().GetForward());
	vPedForward.Normalize();

	float fDot = vPedForward.Dot(vPedVehicle);

	//Behind check
	Vector3 vPedToVehicle = (vVehiclePos - vPedPos );
	vPedToVehicle.Normalize();
	float fDirection = vPedForward.Dot( vPedToVehicle );

	//Timer 
	s32 timer = 0;
	if ( m_PoliceFollowTimer.IsSet() && m_PoliceFollowTimer.GetTimeLeft() > 0)
		timer = m_PoliceFollowTimer.GetTimeLeft();

	//Current crime type
	const char* pCrimeName = CRandomEventManager::GetInstance().GetEventName(m_Type);

	char distanceDebug[512] = "";
	formatf(distanceDebug,
		255, 
		"Vehicle Pursuit\n Cop State:%s\n Pursuit Type:%s\n Distance:%f\n Height diff:%f\n Facing alignment:%f\n Check if vehicle is behind:%f\n Police follow timer:%f\n",
		GetStateName(GetState()),
		pCrimeName,
		fDistToPed,
		fHeightDiff,
		fDot,
		fDirection, 
		timer);

	grcDebugDraw::Text(Vector2(0.1f,0.1f),Color32(255,255,255),distanceDebug);
#endif
}

#endif
