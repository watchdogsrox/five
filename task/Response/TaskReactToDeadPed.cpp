// FILE :    TaskReactToDeadPed.h
// PURPOSE : A task to handle the reactions of peds when they acknowledge the fact a dead ped has been found
// AUTHOR :  Chi-Wai Chiu
// CREATED : 11-03-2009
// TODO:	 Use more clips
//			 Sound, cowardly peds to flee when seeing a dead body
//			 Questioning peds around the crime scene
//			 Check to see if there are other dead peds I can see, if so, mark them as reported

// File header
#include "Task/Response/TaskReactToDeadPed.h"

// Framework headers
#include "ai/task/taskchannel.h"

// Game headers
#include "Event/Events.h"
#include "Event/EventReactions.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Task/Movement/TaskNavMesh.h"
#include "Task/Combat/TaskInvestigate.h"
#include "Task/Combat/TaskReact.h"
#include "Task/Response/TaskFlee.h"
#include "Task/Scenario/ScenarioManager.h"
#include "Task/Scenario/TaskScenario.h"

AI_OPTIMISATIONS()

/////////////////////////
//	REACT TO DEAD PED  //
/////////////////////////

const u32 CTaskReactToDeadPed::ms_iLastReactTimeResponseThreshold = 10000; // 10 seconds
const int	CTaskReactToDeadPed::ms_iMaxNumNearbyPeds = 5;
const float CTaskReactToDeadPed::ms_fMaxQuestionDistance = 10.0f;

CTaskReactToDeadPed::CTaskReactToDeadPed(CPed* pDeadPed, bool bWitnessedFirstHand)
:
m_pDeadPed				(pDeadPed),
m_clipId				(CLIP_ID_INVALID),
m_clipSetId				(CLIP_SET_ID_INVALID),
m_bWitnessedFirstHand	(bWitnessedFirstHand),
m_response				(Response_Ignore),
m_pThreatPed			(NULL),
m_priorityEvent			(EVENT_NONE),
m_bNewPriorityEvent		(false)
{
	SetInternalTaskType(CTaskTypes::TASK_REACT_TO_DEAD_PED);
}

CTaskReactToDeadPed::~CTaskReactToDeadPed()
{
}

CTask::FSM_Return CTaskReactToDeadPed::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	if (!m_pDeadPed)
	{
		return FSM_Quit;
	}

	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin

	// Start
	FSM_State(State_Start)
		FSM_OnEnter
			Start_OnEnter(pPed);
		FSM_OnUpdate
			return Start_OnUpdate(pPed);	

	// ReactToDeadPed
	FSM_State(State_ReactToDeadPed)
		FSM_OnEnter
			ReactToDeadPed_OnEnter(pPed);
		FSM_OnUpdate
			return ReactToDeadPed_OnUpdate(pPed);

	FSM_State(State_Flee)	// Do flee as an event?
		FSM_OnEnter
		SetNewTask(rage_new CTaskSmartFlee(CAITarget(m_pDeadPed)));
	FSM_OnUpdate
		if (GetIsSubtaskFinished(CTaskTypes::TASK_SMART_FLEE))
		{
			return FSM_Quit;
		}
		return FSM_Continue;

	// End quit - its finished.
	FSM_State(State_Finish)
		FSM_OnEnter
			Finish_OnEnter(pPed);
		FSM_OnUpdate
			if (m_response == Response_Flee)
			{
				SetState(State_Flee);
				return FSM_Continue;
			}
			return FSM_Quit;
 		FSM_OnExit 
 			Finish_OnExit(pPed);

	FSM_End
}

void CTaskReactToDeadPed::CheckForNewPriorityEvent(CPed* UNUSED_PARAM(pPed))
{
	if (!m_bNewPriorityEvent)
	{
		return;
	}

	switch (m_priorityEvent)
	{
	case EVENT_NONE:
		return;
	case EVENT_SHOUT_TARGET_POSITION:
	case EVENT_ACQUAINTANCE_PED_HATE:
	case EVENT_GUN_AIMED_AT:
	case EVENT_SHOT_FIRED:
	case EVENT_SHOT_FIRED_BULLET_IMPACT:
	case EVENT_SHOT_FIRED_WHIZZED_BY:
		{
			m_response = Response_Threat_Analysis;
			SetState(State_Finish);
		}
		break;
	default:
		taskErrorf("Unhandled event being sent to task investigate");
		break;
	}

	m_bNewPriorityEvent = false;
	m_priorityEvent = EVENT_NONE; // clear the event, we've handled it
}

void CTaskReactToDeadPed::Start_OnEnter (CPed* pPed)
{
	u32 iTimeBetweenLastReaction = fwTimer::GetTimeInMilliseconds() - pPed->GetPedIntelligence()->GetLastReactedToDeadPedTime();

	// If we've already reacted within the response threshold time, don't react again, but default to threat analysis
	if (iTimeBetweenLastReaction < ms_iLastReactTimeResponseThreshold)
	{
		m_response = Response_Investigate;
		return; // We will not play the clip
	}

	// Reset the last reacted time to be the current time
	pPed->GetPedIntelligence()->SetLastReactedToDeadPedTime(fwTimer::GetTimeInMilliseconds());

	// TODO: Have a range of reaction clips

	// Calculate the reaction response criteria

	// Get the ped's forward vector
	Vector3 fv = VEC3V_TO_VECTOR3(pPed->GetTransform().GetB());
	fv.Normalize();


	Vector3 vTargetPos = VEC3V_TO_VECTOR3(m_pDeadPed->GetTransform().GetPosition());
	Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

	// Get the vector from the target ped to this ped
	Vector3 tv = vTargetPos - vPedPos;
	tv.Normalize();

	// Dot product doesn't return sign information - need to use cross to determine which side ped is on
	float fAngle = rage::Acosf(fv.Dot(tv));
	fAngle = fwAngle::LimitRadianAngle(fAngle);

	// Use cross product to determine which side the target is, if > 0, left, if < 0, right
	fv.Cross(tv);
	float fLeftOrRight = fv.z;

	float fDistToTarget = vTargetPos.Dist(vPedPos);

	ChooseReactionClip(pPed, fDistToTarget, fAngle, fLeftOrRight);
}

CTask::FSM_Return CTaskReactToDeadPed::Start_OnUpdate(CPed* pPed)
{
	// If we've already reacted within the response threshold time, don't react again, but default to investigate
	pPed->GetPedIntelligence()->SetAlertnessState(CPedIntelligence::AS_Alert);
	SetState(State_ReactToDeadPed);
	return FSM_Continue;
}

void CTaskReactToDeadPed::ReactToDeadPed_OnEnter (CPed* pPed)
{
	m_response = Response_Investigate;

	// Report event to nearby allies
// 	CEventDeadPedFound event(m_pDeadPed,false);
// 	event.InformRespectedFriends(pPed);
	m_pDeadPed->SetPedConfigFlag( CPED_CONFIG_FLAG_HasDeadPedBeenReported, 1 );

	if (m_bWitnessedFirstHand)
	{	
		// I've seen a dead ped!
		pPed->NewSay("WE_GOT_SITUATION",0, true, true);
	}
	else
	{
		// Did someone say dead ped?
		pPed->NewSay("SURPRISED_LOW",0, true, true);
	}

	// Make the ped look the dead ped
	if (!pPed->GetIkManager().IsLooking())
	{
		pPed->GetIkManager().LookAt(0, m_pDeadPed, 3000, BONETAG_HEAD, NULL, LF_FAST_TURN_RATE );
	}
}

CTask::FSM_Return CTaskReactToDeadPed::ReactToDeadPed_OnUpdate (CPed* UNUSED_PARAM(pPed))
{
	SetState(State_Finish);
	return FSM_Continue;
}


void CTaskReactToDeadPed::Finish_OnEnter(CPed* pPed)
{
	weaponAssert(pPed->GetWeaponManager());
	if (m_pDeadPed && pPed->GetPedIntelligence()->IsFriendlyWith(*m_pDeadPed.Get()))
	{
		// We've become very alert during reaction/or were already very alert, go into threat analysis
		if (m_response != Response_Threat_Analysis)
		{
			pPed->GetPedIntelligence()->SetAlertnessState(CPedIntelligence::AS_VeryAlert);
			m_response = Response_Investigate;
		}
	}
	// Unarmed civilian
	else if (!pPed->GetWeaponManager()->GetIsArmed())//if (!pPed->PopTypeIsMission()  && pPed->GetPedModelInfo()->GetBravery() < 3)
	{
		m_response = Response_Flee; // If we're not brave
	}
}

void CTaskReactToDeadPed::Finish_OnExit(CPed* pPed)
{
	// Generate the appropriate event to react to
	switch (m_response)
	{
	case Response_Investigate:
		{
			if (m_pDeadPed && pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanInvestigate))
			{
				CEventReactionInvestigateDeadPed reactEvent(m_pDeadPed);	// Investigate the dead ped
				pPed->GetPedIntelligence()->AddEvent(reactEvent);
			}
		}
		break;
	case Response_Threat_Analysis:	
		if (taskVerifyf(m_pThreatPed,"Threat ped isn't set, but we're going into threat response"))
		{
			CEventReactionEnemyPed reactEvent(m_pThreatPed);	// React to the enemy
			pPed->GetPedIntelligence()->AddEvent(reactEvent); 
		}
		break;
// 	case Response_Flee:
// 	{
// 		CEventReactionFleeFromDeadPed reactEvent(m_pDeadPed); // Run away!
// 		pPed->GetPedIntelligence()->AddEvent(reactEvent); 
// 	}
	default:
		break;
	}
}

#if !__FINAL
void CTaskReactToDeadPed::Debug() const
{
	if(m_pDeadPed)
	{
		DEBUG_DRAW_ONLY(grcDebugDraw::Line(VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition()), VEC3V_TO_VECTOR3(m_pDeadPed->GetTransform().GetPosition()), Color_yellow1, Color_red1));
	}
}
#endif // !__FINAL

// TODO: civilian or guard?
typedef enum { ReactionShort,	ReactionMedium, ReactionLong }					DeadPedReactionRange;
typedef enum { ReactionFwd,		ReactionBack,	ReactionLeft,	ReactionRight}	DeadPedReactionDirection;
typedef enum { ReactionFirstHand, ReactionSecondHand}							DeadPedReactionEvent;

struct sDeadPedReactionClipInfo
{
	DeadPedReactionRange		m_eRange;
	DeadPedReactionDirection	m_eDirection;
	DeadPedReactionEvent		m_eEvent;
	fwMvClipId						m_clipId;
};

// Chooses the reaction clip based on constraints on the input parameters
void CTaskReactToDeadPed::ChooseReactionClip(CPed* UNUSED_PARAM(pPed), float UNUSED_PARAM(fDistanceToTarget), float UNUSED_PARAM(fAngle), float UNUSED_PARAM(fLeftOrRight))
{
}

#if !__FINAL
const char * CTaskReactToDeadPed::GetStaticStateName( s32 iState )
{
	// Make sure FSM state is valid
	taskFatalAssertf(iState >= State_Start && iState <= State_Finish, "State outside range!");
	static const char* aReactToDeadPedStateNames[] = 
	{
		"State_Start", 
		"State_ReactToDeadPed",
		"State_Finish"
	};

	return aReactToDeadPedStateNames[iState];
}
#endif

/////////////////////////
//	CHECK PED IS DEAD  //
/////////////////////////

CTaskCheckPedIsDead::CTaskCheckPedIsDead(CPed* pDeadPed)
:
m_pDeadPed(pDeadPed),
m_checkState(State_Start)
{
	SetInternalTaskType(CTaskTypes::TASK_CHECK_PED_IS_DEAD);
}

CTaskCheckPedIsDead::~CTaskCheckPedIsDead()
{
}

CTask::FSM_Return CTaskCheckPedIsDead::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin

		// Start
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate(pPed);	

		// Stand Aiming
		FSM_State(State_StandAiming)
			FSM_OnEnter
				StandAiming_OnEnter(pPed);
			FSM_OnUpdate
				return StandAiming_OnUpdate(pPed);

		// UseRadio
		FSM_State(State_UseRadio)
			FSM_OnEnter
				UseRadio_OnEnter(pPed);
			FSM_OnUpdate
				return UseRadio_OnUpdate(pPed);

		// GoToDeadPed
		FSM_State(State_GoToDeadPed)
			FSM_OnEnter
				GoToDeadPed_OnEnter(pPed);
			FSM_OnUpdate
			return GoToDeadPed_OnUpdate(pPed);

		// ExamineBody
		FSM_State(State_ExamineDeadPed)
			FSM_OnEnter
				ExamineDeadPed_OnEnter(pPed);
			FSM_OnUpdate
				return ExamineDeadPed_OnUpdate(pPed);

		// End quit - its finished.
		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskCheckPedIsDead::Start_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	SetState(State_StandAiming);
	return FSM_Continue;
}

void CTaskCheckPedIsDead::StandAiming_OnEnter (CPed* UNUSED_PARAM(pPed))
{
// 	static s32 iScenarioFleeType = CScenarioManager::GetScenarioTypeFromHashKey(ATSTRINGHASH("Scenario_StandAiming", 0x3BEC505B));
// 	SetNewTask(rage_new CTaskStationaryScenario(iScenarioFleeType, NULL, NULL, true, false,true,true)); // Don't return to orignal position
}

CTask::FSM_Return CTaskCheckPedIsDead::StandAiming_OnUpdate (CPed* UNUSED_PARAM(pPed))
{
	//if (GetIsSubtaskFinished(CTaskTypes::TASK_STATIONARY_SCENARIO))
	{
		SetState(State_UseRadio);
	}

	return FSM_Continue;
}

void CTaskCheckPedIsDead::UseRadio_OnEnter (CPed* UNUSED_PARAM(pPed))
{
// 	switch (m_checkState)
// 	{
// 		case State_Start:
// 			SetNewTask(rage_new CTaskUseRadio("RADIO_TARGET_KILLED"));
// 			break;
// 		case State_ExamineDeadPed:
// 			SetNewTask(rage_new CTaskUseRadio("RADIO_AREA_CLEAR"));
// 			break;
// 		default:
// 			break;
// 	}	
}

CTask::FSM_Return CTaskCheckPedIsDead::UseRadio_OnUpdate (CPed* UNUSED_PARAM(pPed))
{
	//if (GetIsSubtaskFinished(CTaskTypes::TASK_USE_RADIO))
	{
		switch (m_checkState)
		{
		case State_Start:
			SetState(State_GoToDeadPed);
			break;
		case State_ExamineDeadPed:
			SetState(State_Finish);
			break;
		default:
			break;
		}	
	}
	return FSM_Continue;
}

void CTaskCheckPedIsDead::GoToDeadPed_OnEnter (CPed* UNUSED_PARAM(pPed))
{
	SetNewTask(rage_new CTaskComplexControlMovement(rage_new CTaskMoveFollowNavMesh(MOVEBLENDRATIO_WALK, VEC3V_TO_VECTOR3(m_pDeadPed->GetTransform().GetPosition()), 2.0f)));
}

CTask::FSM_Return CTaskCheckPedIsDead::GoToDeadPed_OnUpdate (CPed* UNUSED_PARAM(pPed))
{
	if (GetIsSubtaskFinished(CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT))
	{
		SetState(State_ExamineDeadPed);
	}
	return FSM_Continue;
}

void CTaskCheckPedIsDead::ExamineDeadPed_OnEnter (CPed* UNUSED_PARAM(pPed))
{
// 	static s32 iScenarioType = CScenarioManager::GetScenarioTypeFromHashKey(ATSTRINGHASH("Scenario_Check_Corpse", 0xC3189ED3));
// 	SetNewTask(rage_new CTaskStationaryScenario(iScenarioType, NULL, NULL, true, false,true,true)); // Don't return to original position
}

CTask::FSM_Return CTaskCheckPedIsDead::ExamineDeadPed_OnUpdate (CPed* UNUSED_PARAM(pPed))
{
// 	if (GetIsSubtaskFinished(CTaskTypes::TASK_STATIONARY_SCENARIO))
// 	{
 		m_checkState = State_ExamineDeadPed;
 		SetState(State_UseRadio);
// 	}
	return FSM_Continue;
}

#if !__FINAL
void CTaskCheckPedIsDead::Debug() const
{
	if(m_pDeadPed)
	{
		DEBUG_DRAW_ONLY(grcDebugDraw::Line(VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition()), VEC3V_TO_VECTOR3(m_pDeadPed->GetTransform().GetPosition()), Color_yellow1, Color_red1));
	}
}
#endif // !__FINAL

#if !__FINAL
const char * CTaskCheckPedIsDead::GetStaticStateName( s32 iState )
{
	// Make sure FSM state is valid
	taskFatalAssertf(iState >= State_Start && iState <= State_Finish, "State outside range!");
	static const char* aStateNames[] = 
	{
		"State_Start", 
		"State_StandAiming",
		"State_UseRadio",
		"State_GoToDeadPed",
		"State_ExamineDeadPed",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif