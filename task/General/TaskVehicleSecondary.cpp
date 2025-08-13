//
// task/General/TaskVehicleSecondary.cpp
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

//rage headers

// game headers
#include "event/EventShocking.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Task/Default/TaskAmbient.h"
#include "Task/General/TaskVehicleSecondary.h"
#include "task/General/Phone/TaskCallPolice.h"
#include "Vehicles/Vehicle.h"


AI_OPTIMISATIONS()

CTaskAggressiveRubberneck::CTaskAggressiveRubberneck(const CEvent* pEvent)
: m_pEvent(NULL)
{
	// We can't just store a pointer to the user's CEvent object, need to make a copy.
	if(pEvent && CEvent::CanCreateEvent())
	{
		m_pEvent = static_cast<const CEvent*>(pEvent->Clone());
	}
	SetInternalTaskType(CTaskTypes::TASK_AGGRESSIVE_RUBBERNECK);
}

CTaskAggressiveRubberneck::~CTaskAggressiveRubberneck()
{
	delete m_pEvent;
}


#if !__FINAL
const char* CTaskAggressiveRubberneck::GetStaticStateName( s32 iState )
{
	switch(iState)
	{
		case State_Look: return "Looking";
		case State_Finish: return "Finishing";
		default: return "NULL";
	}
}
#endif // !__FINAL

CTask::FSM_Return CTaskAggressiveRubberneck::ProcessPreFSM()
{
	// Check that the event still exists (we made a copy, but it could have failed to clone).
	if (!m_pEvent)
	{
		return FSM_Quit;
	}

	GetPed()->SetPedResetFlag(CPED_RESET_FLAG_PanicInVehicle, true);

	return FSM_Continue;
}

CTask::FSM_Return CTaskAggressiveRubberneck::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	FSM_Begin
		FSM_State(State_Look)
			FSM_OnEnter
				Look_OnEnter();
			FSM_OnUpdate
				return Look_OnUpdate();
		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

// Change the properties of the ped to be an aggressive driver.
void CTaskAggressiveRubberneck::Look_OnEnter()
{
	//Grab the ped.
	CPed* pPed = GetPed();

	pPed->GetPedIntelligence()->SetDriverAggressivenessOverride(1.0f);
	pPed->GetPedIntelligence()->SetDriverAbilityOverride(0.0f);

	CVehicle* pMyVehicle = pPed->GetMyVehicle();
	if (pMyVehicle)
	{
		pMyVehicle->m_nVehicleFlags.bMadDriver = true;
	}

	//Check if we can call the police.
	CPed* pSourcePed = m_pEvent->GetSourcePed();
	if( m_pEvent->IsShockingEvent() && static_cast<const CEventShocking *>(m_pEvent.Get())->GetTunables().m_CanCallPolice &&
		pSourcePed && CTaskCallPolice::CanMakeCall(*GetPed(), pSourcePed) )
	{
		CWanted* pPlayerWanted = pSourcePed->GetPlayerWanted();
		if( !pPlayerWanted || pPlayerWanted->GetWantedLevel() > WANTED_CLEAN ||
			static_cast<const CEventShocking *>(m_pEvent.Get())->GetStartTime() > pPlayerWanted->GetTimeWantedLevelCleared() )
		{
			CEntity* pVictim = static_cast<const CEventShocking *>(m_pEvent.Get())->GetOtherEntity();
			if(!pVictim || !pVictim->GetIsTypePed() || !static_cast<CPed*>(pVictim)->GetPedConfigFlag(CPED_CONFIG_FLAG_DontInfluenceWantedLevel))
			{
				//Check if we should call the police.
				static dev_float s_fChances = 0.33f;
				if(fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < s_fChances)
				{
					SetNewTask(rage_new CTaskCallPolice(pSourcePed));
					return;
				}
			}
		}
	}

	SetNewTask(rage_new CTaskAmbientLookAtEvent(m_pEvent));
}

// Keep looking until the event expires.
CTask::FSM_Return CTaskAggressiveRubberneck::Look_OnUpdate()
{
	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Finish);
	}
	return FSM_Continue;
}

bool CTaskAggressiveRubberneck::MakeAbortable(const AbortPriority priority, const aiEvent* pEvent)
{
	// We do not want to interrupt the task if it has not ran for a minimum amount of time.
	static const float fMIN_TIME_TO_BE_ABORTED_BY_EQUAL_PRIORITY_EVENT = 5.0f;
	if (priority != ABORT_PRIORITY_IMMEDIATE && 
		m_pEvent && pEvent && 
		pEvent->GetEventPriority() <= m_pEvent->GetEventPriority() && 
		GetTimeRunning() < fMIN_TIME_TO_BE_ABORTED_BY_EQUAL_PRIORITY_EVENT)
	{
		return false;
	}

	return aiTask::MakeAbortable(priority, pEvent);
}
