// File header
#include "Event/EventEditable.h"

// Framework headers
#include "ai/aichannel.h"

// Game headers
#include "Event/Decision/EventDecisionMaker.h"
#include "Event/EventSource.h"
#include "PedGroup/PedGroup.h"
#include "Peds/Ped.h"
#include "Event/System/EventDataManager.h"
#include "Peds/PedIntelligence.h"
#include "Vehicles/Vehicle.h"

AI_OPTIMISATIONS()

CEventEditableResponse::CEventEditableResponse()
: m_iTaskType(CTaskTypes::TASK_INVALID_ID)
, m_bWitnessedFirstHand(true)
, m_FleeFlags(0)
{
}

CEvent* CEventEditableResponse::Clone() const
{
	CEventEditableResponse* pClone = CloneEditable();
	pClone->m_iTaskType				= m_iTaskType;
	pClone->m_bWitnessedFirstHand	= m_bWitnessedFirstHand;
	pClone->m_FleeFlags				= m_FleeFlags;
	pClone->m_SpeechHash			= m_SpeechHash;
	return pClone;
}

void CEventEditableResponse::ComputeResponseTaskType(CPed* pPed)
{
	if(aiVerifyf(m_iTaskType == CTaskTypes::TASK_INVALID_ID, "m_iTaskType already computed, value is [%d]", m_iTaskType))
	{
		m_iTaskType = CTaskTypes::TASK_NONE;

		CPedEventDecisionMaker& rPedDecisionMaker = pPed->GetPedIntelligence()->GetPedDecisionMaker();

		CEventResponseTheDecision decision;
		rPedDecisionMaker.MakeDecision(pPed, this, decision);

		const CTaskTypes::eTaskType taskType = (CTaskTypes::eTaskType)decision.GetTaskType();
		if(taskType != CTaskTypes::TASK_INVALID_ID)
		{
			Assign(m_FleeFlags,decision.GetFleeFlags());
			m_iTaskType = taskType;
			m_SpeechHash = decision.GetSpeechHash();
		}
	}
}


CTaskTypes::eTaskType CEventEditableResponse::FindTaskTypeForResponse(u32 taskHash)
{
	CTaskTypes::eTaskType taskType = CTaskTypes::TASK_INVALID_ID;

	const CEventDataResponseTask* pEventResponseTaskData = CEventDataManager::GetInstance().GetEventResponseTask(taskHash);
	if(pEventResponseTaskData)
	{
		const void * pResponseParserType = pEventResponseTaskData->parser_GetStructure();
		if (pResponseParserType == CEventDataResponseTaskCombat::parser_GetStaticStructure())
			taskType = CTaskTypes::TASK_COMBAT;
		else if (pResponseParserType == CEventDataResponseTaskThreat::parser_GetStaticStructure())
			taskType = CTaskTypes::TASK_THREAT_RESPONSE;
		else if (pResponseParserType == CEventDataResponseTaskCower::parser_GetStaticStructure())
			taskType = CTaskTypes::TASK_COWER;
		else if (pResponseParserType == CEventDataResponseTaskHandsUp::parser_GetStaticStructure())
			taskType = CTaskTypes::TASK_HANDS_UP;
		else if (pResponseParserType == CEventDataResponseTaskGunAimedAt::parser_GetStaticStructure())
			taskType = CTaskTypes::TASK_REACT_TO_GUN_AIMED_AT;
		else if (pResponseParserType == CEventDataResponseTaskFlee::parser_GetStaticStructure())
			taskType = CTaskTypes::TASK_SMART_FLEE;
		else if (pResponseParserType == CEventDataResponseTaskTurnToFace::parser_GetStaticStructure())
			taskType = CTaskTypes::TASK_MOVE_ACHIEVE_HEADING;
		else if (pResponseParserType == CEventDataResponseTaskCrouch::parser_GetStaticStructure())
			taskType = CTaskTypes::TASK_CROUCH;
		else if (pResponseParserType == CEventDataResponseTaskLeaveCarAndFlee::parser_GetStaticStructure())
			taskType = CTaskTypes::TASK_COMPLEX_LEAVE_CAR_AND_FLEE;
		else if (pResponseParserType == CEventDataResponseTaskWalkRoundFire::parser_GetStaticStructure())
			taskType = CTaskTypes::TASK_WALK_ROUND_FIRE;
		else if (pResponseParserType == CEventDataResponsePoliceTaskWanted::parser_GetStaticStructure())
			taskType = CTaskTypes::TASK_POLICE_WANTED_RESPONSE;
		else if (pResponseParserType == CEventDataResponseSwatTaskWanted::parser_GetStaticStructure())
			taskType = CTaskTypes::TASK_SWAT_WANTED_RESPONSE;
		else if (pResponseParserType == CEventDataResponseTaskScenarioFlee::parser_GetStaticStructure())
			taskType = CTaskTypes::TASK_SCENARIO_FLEE;
		else if (pResponseParserType == CEventDataResponseTaskFlyAway::parser_GetStaticStructure())
			taskType = CTaskTypes::TASK_FLY_AWAY;
		else if (pResponseParserType == CEventDataResponseTaskWalkRoundEntity::parser_GetStaticStructure())
			taskType = CTaskTypes::TASK_WALK_ROUND_ENTITY;
		else if (pResponseParserType == CEventDataResponseTaskHeadTrack::parser_GetStaticStructure())
			taskType = CTaskTypes::TASK_AMBIENT_LOOK_AT_EVENT;
		else if (pResponseParserType == CEventDataResponseTaskShockingEventGoto::parser_GetStaticStructure())
			taskType = CTaskTypes::TASK_SHOCKING_EVENT_GOTO;
		else if (pResponseParserType == CEventDataResponseTaskShockingEventHurryAway::parser_GetStaticStructure())
			taskType = CTaskTypes::TASK_SHOCKING_EVENT_HURRYAWAY;
		else if (pResponseParserType == CEventDataResponseTaskShockingEventWatch::parser_GetStaticStructure())
			taskType = CTaskTypes::TASK_SHOCKING_EVENT_WATCH;
		else if (pResponseParserType == CEventDataResponseTaskShockingPoliceInvestigate::parser_GetStaticStructure())
			taskType = CTaskTypes::TASK_SHOCKING_POLICE_INVESTIGATE;
		else if (pResponseParserType == CEventDataResponseTaskEvasiveStep::parser_GetStaticStructure())
			taskType = CTaskTypes::TASK_COMPLEX_EVASIVE_STEP;
		else if (pResponseParserType == CEventDataResponseTaskEscapeBlast::parser_GetStaticStructure())
			taskType = CTaskTypes::TASK_ESCAPE_BLAST;
		else if (pResponseParserType == CEventDataResponseTaskAgitated::parser_GetStaticStructure())
			taskType = CTaskTypes::TASK_AGITATED;
		else if (pResponseParserType == CEventDataResponseTaskExplosion::parser_GetStaticStructure())
			taskType = CTaskTypes::TASK_REACT_TO_EXPLOSION;
		else if (pResponseParserType == CEventDataResponseTaskDuckAndCover::parser_GetStaticStructure())
			taskType = CTaskTypes::TASK_ON_FOOT_DUCK_AND_COVER;
		else if (pResponseParserType == CEventDataResponseTaskShockingEventReactToAircraft::parser_GetStaticStructure())
			taskType = CTaskTypes::TASK_SHOCKING_EVENT_REACT_TO_AIRCRAFT;
		else if (pResponseParserType == CEventDataResponseTaskShockingEventReact::parser_GetStaticStructure())
			taskType = CTaskTypes::TASK_SHOCKING_EVENT_REACT;
		else if (pResponseParserType == CEventDataResponseTaskShockingEventBackAway::parser_GetStaticStructure())
			taskType = CTaskTypes::TASK_SHOCKING_EVENT_BACK_AWAY;
		else if (pResponseParserType == CEventDataResponseTaskShockingEventStopAndStare::parser_GetStaticStructure())
			taskType = CTaskTypes::TASK_SHOCKING_EVENT_STOP_AND_STARE;
		else if (pResponseParserType == CEventDataResponseTaskSharkAttack::parser_GetStaticStructure())
			taskType = CTaskTypes::TASK_SHARK_ATTACK;
		else if (pResponseParserType == CEventDataResponseTaskExhaustedFlee::parser_GetStaticStructure())
			taskType = CTaskTypes::TASK_EXHAUSTED_FLEE;
		else if (pResponseParserType == CEventDataResponseTaskGrowlAndFlee::parser_GetStaticStructure())
			taskType = CTaskTypes::TASK_GROWL_AND_FLEE;
		else if (pResponseParserType == CEventDataResponseTaskWalkAway::parser_GetStaticStructure())
			taskType = CTaskTypes::TASK_WALK_AWAY;
		else if (pResponseParserType == CEventDataResponseAggressiveRubberneck::parser_GetStaticStructure())
			taskType = CTaskTypes::TASK_AGGRESSIVE_RUBBERNECK;
		else if (pResponseParserType == CEventDataResponseTaskShockingNiceCar::parser_GetStaticStructure())
			taskType = CTaskTypes::TASK_SHOCKING_NICE_CAR_PICTURE;
		else if (pResponseParserType == CEventDataResponseFriendlyNearMiss::parser_GetStaticStructure())
			taskType = CTaskTypes::TASK_FRIENDLY_FIRE_NEAR_MISS;
		else if (pResponseParserType == CEventDataResponseFriendlyAimedAt::parser_GetStaticStructure())
			taskType = CTaskTypes::TASK_FRIENDLY_AIMED_AT;
		else if (pResponseParserType == CEventDataResponseDeferToScenarioPointFlags::parser_GetStaticStructure())
			taskType = CTaskTypes::TASK_DEFER_TO_SCENARIO;
		else if (pResponseParserType == CEventDataResponseTaskShockingEventThreatResponse::parser_GetStaticStructure())
			taskType = CTaskTypes::TASK_SHOCKING_EVENT_THREAT_RESPONSE;
		else if (pResponseParserType == CEventDataResponsePlayerDeath::parser_GetStaticStructure())
			taskType = CTaskTypes::TASK_PLAYER_DEATH_REACT;
		else
		{
			aiAssertf(0, "Unhandled case [%s] as part of CEventEditableResponse::FindTaskTypeForResponse", pEventResponseTaskData->parser_GetStructure()->GetName());
		}
	}

	return taskType;
}


CEventEditableResponse* CEventEditableResponse::CloneWithDefaultParams() const
{
	CEventEditableResponse* pClone = static_cast<CEventEditableResponse*>(Clone());
	pClone->m_iTaskType            = CTaskTypes::TASK_INVALID_ID;
	pClone->m_bWitnessedFirstHand  = false;
	pClone->m_FleeFlags			   = 0;
	return pClone;
}

bool CEventEditableResponse::WillRespond() const
{
	return CTaskTypes::TASK_NONE != m_iTaskType || ((GetEventType() == EVENT_DAMAGE || GetEventType() == EVENT_MELEE_ACTION) && GetWitnessedFirstHand());
}
