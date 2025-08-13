// FILE :    TaskChat.cpp
// PURPOSE : Makes two peds go to each other and do a chat scenario.
//			 Used during patrols.
// AUTHOR :  Chi-Wai Chiu
// CREATED : 17-02-2009

#include "Task/Default/TaskChat.h"

// Framework headers
#include "ai/task/taskchannel.h"				// For channel asserts and verifies

// Game headers
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"				// For accessing pedintelligence
#include "Task/Movement/TaskNavMesh.h"			// For using CTaskMoveFollowNavMesh etc
#include "Task/Default/Patrol/TaskPatrol.h"
#include "Task/Scenario/ScenarioManager.h"		// For accessing CScenariomanager
#include "Task/Scenario/Types/TaskChatScenario.h"			// For using CTaskChatScenario
#include "Task/Animation/TaskAnims.h"

AI_OPTIMISATIONS()

/////////////////////////////
//		TASK CHAT		   //
/////////////////////////////

extern const u32 g_SpeechContextGuardResponsePHash;

CTaskChat::Tunables CTaskChat::sm_Tunables;

IMPLEMENT_DEFAULT_TASK_TUNABLES(CTaskChat, 0x494d4893);

CTaskChat::CTaskChat(CPed* pPedToChatTo, fwFlags32 iFlags, const Vector3 &vGoToPosition, float fHeading, float fIdleTime)
: 
m_pPedToChatTo		(pPedToChatTo),
m_clipSetId		(CLIP_SET_ID_INVALID),
m_clipId			(CLIP_ID_INVALID),
m_iTimeDelay		(0),
m_vPreviousGoToPos	(Vector3::ZeroType),
m_iFlags			(iFlags),
m_vGoToPosition		(vGoToPosition),
m_fHeading			(fHeading),
m_fIdleTime			(fIdleTime),
m_IdleTimer			(m_fIdleTime),
m_WaitTimer			(sm_Tunables.m_MaxWaitTime),
m_bStartedIdleTimer (false)
{
	SetInternalTaskType(CTaskTypes::TASK_CHAT);
}

CTask::FSM_Return CTaskChat::ProcessPreFSM()
{
	if (!m_pPedToChatTo)
	{
		return FSM_Quit;
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskChat::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		FSM_State(State_GoToPosition)
			FSM_OnEnter
				GoToPosition_OnEnter(pPed);
			FSM_OnUpdate
				return GoToPosition_OnUpdate(pPed);		

		FSM_State(State_QuickChat)
			FSM_OnUpdate
				const u32 iVoiceHash = ATSTRINGHASH("ALPINE_GUARD_01", 0x0f5c39551);
				pPed->NewSay("CONV_GENERIC_STATE", iVoiceHash, true, false, -1, m_pPedToChatTo, g_SpeechContextGuardResponsePHash);
				SetState(State_Finish);
				return FSM_Continue;

		FSM_State(State_WaitingToChat)
			FSM_OnEnter
				WaitingToChat_OnEnter(pPed);
			FSM_OnUpdate
				return WaitingToChat_OnUpdate(pPed);

		FSM_State(State_Chatting)
			FSM_OnEnter
				Chatting_OnEnter(pPed);
			FSM_OnUpdate
				return Chatting_OnUpdate(pPed);

		FSM_State(State_PlayingClip)
			FSM_OnEnter
				PlayingClip_OnEnter(pPed);
			FSM_OnUpdate
				return PlayingClip_OnUpdate(pPed);

		FSM_State(State_Idle)
			FSM_OnUpdate
				return Idle_OnUpdate(pPed);

		FSM_State(State_TurnToFacePed)
			FSM_OnEnter
				TurnToFacePed_OnEnter(pPed);
			FSM_OnUpdate
				return TurnToFacePed_OnUpdate(pPed);

		FSM_State(State_Finish)
			FSM_OnUpdate
				if (m_iFlags.IsFlagSet(CF_PlayGoodByeGestures))
				{
					if (m_iFlags.IsFlagSet(CF_IsInitiator))
					{
						pPed->NewSay("CONV_LEAVE_STATE", 0, true, false, -1, m_pPedToChatTo, ATSTRINGHASH("CONV_LEAVE_BYE", 0x0d9381a3e));
						aiTask* pTask = m_pPedToChatTo->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_CHAT);
						if (pTask)
						{
							static_cast<CTaskChat*>(pTask)->LeaveChat(false);
						}
					}
				}
				return FSM_Quit;
	FSM_End
}

void CTaskChat::GoToPosition_OnEnter(CPed* UNUSED_PARAM(pPed))
{
	if (m_iFlags.IsFlagSet(CF_GoToSpecificPosition))
	{
		taskAssertf(!m_vGoToPosition.IsEqual(Vector3(Vector3::ZeroType)),"CF_GoToSpecificPosition is set but m_vGoToPosition is zero, it's probably not been set");
		SetNewTask(rage_new CTaskComplexControlMovement(rage_new CTaskMoveFollowNavMesh(MOVEBLENDRATIO_WALK, m_vGoToPosition, 0.5f)));
		m_vPreviousGoToPos = m_vGoToPosition;
	}
	else
	{
		// Find where the ped to chat to is facing, and goto 1m in front of them then face them
		Vector3 vForwardVec = VEC3V_TO_VECTOR3(m_pPedToChatTo->GetTransform().GetB());
		vForwardVec.Normalize();
		Vector3 vMoveToPos = VEC3V_TO_VECTOR3(m_pPedToChatTo->GetTransform().GetPosition()) + vForwardVec;
		dev_float fDistBetweenPeds = 3.0f;
		SetNewTask(rage_new CTaskComplexControlMovement(rage_new CTaskMoveFollowNavMesh(MOVEBLENDRATIO_WALK, vMoveToPos, fDistBetweenPeds, CTaskMoveFollowNavMesh::ms_fSlowDownDistance, -1, true, false, NULL, 3.0f)));
		m_vPreviousGoToPos = vMoveToPos;
	}
}

CTask::FSM_Return CTaskChat::GoToPosition_OnUpdate(CPed* pPed)
{
	if (!m_iFlags.IsFlagSet(CF_GoToSpecificPosition))
	{
		const Vector3 vPedToChatToPosition = VEC3V_TO_VECTOR3(m_pPedToChatTo->GetTransform().GetPosition());
		// Close enough to chat
		if (vPedToChatToPosition.Dist2(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition())) < 5.0f)
		{
			if (m_iFlags.IsFlagSet(CF_DoQuickChat))
			{
				SetState(State_QuickChat);
			}
			else
			{
				if (m_iFlags.IsFlagSet(CF_PlayGreetingGestures))
				{
					pPed->NewSay("GENERIC_GREET", 0, true, false, -1, m_pPedToChatTo, ATSTRINGHASH("GENERIC_GREET", 0x057e9b79c));
				}
				m_iFlags.SetFlag(CF_IsInPosition);
				SetState(State_WaitingToChat);
			}
			return FSM_Continue;
		}

		// Find where the ped to chat to is facing, and goto 1m in front of them then face them -- update the position
		Vector3 vForwardVec = VEC3V_TO_VECTOR3(m_pPedToChatTo->GetTransform().GetB());
		vForwardVec.Normalize();
		Vector3 vMoveToPos = vPedToChatToPosition + vForwardVec;

		// If the new move to position is more than 5m than the last, set a new move to pos
		if (vMoveToPos.Dist2(m_vPreviousGoToPos) > 5.0f)
		{
			if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
			{
				aiTask* pMoveTask = static_cast<CTaskComplexControlMovement*>(GetSubTask())->GetRunningMovementTask(pPed);
				taskAssert(pMoveTask);
				if (pMoveTask)
				{
					taskAssert(pMoveTask->GetTaskType()  == CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH );
					static_cast<CTaskMoveFollowNavMesh*>(pMoveTask)->SetTarget(pPed, VECTOR3_TO_VEC3V(vMoveToPos));
					m_vPreviousGoToPos = vMoveToPos;
				}
			}
			return FSM_Continue;
		}
	}
	
	if (GetIsSubtaskFinished(CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT))
	{
		m_iFlags.SetFlag(CF_IsInPosition);

		if (m_iFlags.IsFlagSet(CF_DoQuickChat))
		{
			SetState(State_QuickChat);
		}
		else
		{
			if (m_iFlags.IsFlagSet(CF_PlayGreetingGestures))
			{
				pPed->NewSay("GENERIC_GREET", 0, true, false, -1, m_pPedToChatTo, ATSTRINGHASH("GENERIC_GREET", 0x057e9b79c));
			}
			SetState(State_WaitingToChat);
		}
	}

	return FSM_Continue;
}

void CTaskChat::WaitingToChat_OnEnter(CPed* UNUSED_PARAM(pPed))
{
	m_WaitTimer.Reset();
}

CTask::FSM_Return CTaskChat::WaitingToChat_OnUpdate(CPed* pPed)
{
	if (m_WaitTimer.Tick())
	{
		SetState(State_Finish);
		return FSM_Continue;
	}

	// Turn to face the ped unless we've specified a custom heading
	float desiredHeading;
	if (!m_iFlags.IsFlagSet(CF_UseCustomHeading))
	{
		const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		const Vector3 vPedToChatToPosition = VEC3V_TO_VECTOR3(m_pPedToChatTo->GetTransform().GetPosition());
		desiredHeading = fwAngle::GetRadianAngleBetweenPoints(vPedToChatToPosition.x, vPedToChatToPosition.y, vPedPosition.x, vPedPosition.y);
	}
	else
	{
		desiredHeading = m_fHeading;
	}
	pPed->SetDesiredHeading(desiredHeading);

	if (m_iFlags.IsFlagSet(CF_AutoChat))
	{
		// At least at this time, SetDesiredHeading() calls after we start the scenario probably won't work
		// (and may also be overwritten by CTaskChatScenario). So, before we leave the WaitingToChat state,
		// make sure we're facing roughly the right direction.
		const float currentHeading = pPed->GetCurrentHeading();
		const float angleDiff = fabsf(fwAngle::LimitRadianAngle(currentHeading - desiredHeading));
		if(angleDiff <= sm_Tunables.m_HeadingToleranceDegrees*DtoR)
		{
			aiTask* pTask = m_pPedToChatTo->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_CHAT);
			if (pTask && static_cast<CTaskChat*>(pTask)->IsInPosition())
			{
				SetState(State_Chatting);
			}
		}
	}
	else
	{
		SetState(State_Idle);
	}

	return FSM_Continue;
}


void CTaskChat::Chatting_OnEnter(CPed* UNUSED_PARAM(pPed))
{		
	if (!m_timer.IsSet())
	{
		m_timer.Set(fwTimer::GetTimeInMilliseconds(),10000); // chat for 10 seconds
	}

	if (m_pPedToChatTo)
	{	
		s32 iScenarioType = CScenarioManager::GetScenarioTypeFromHashKey(SCENARIOTYPE_WORLD_HUMAN_HANGOUT_STREET);
		if (m_iFlags.IsFlagSet(CF_IsInitiator))
		{	
			SetNewTask(rage_new CTaskChatScenario(iScenarioType,m_pPedToChatTo,AUD_CONVERSATION_TOPIC_GUARD,NULL,CTaskChatScenario::State_AboutToSayStatement));
		}
		else
		{
			SetNewTask(rage_new CTaskChatScenario(iScenarioType,m_pPedToChatTo,AUD_CONVERSATION_TOPIC_GUARD,NULL,CTaskChatScenario::State_AboutToListenToStatement));
		}
	}

}

CTask::FSM_Return CTaskChat::Chatting_OnUpdate(CPed* pPed)
{
	// Turn to face the ped unless we've specified a custom heading
	if (!m_iFlags.IsFlagSet(CF_UseCustomHeading))
	{
		const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		const Vector3 vPedToChatToPosition = VEC3V_TO_VECTOR3(m_pPedToChatTo->GetTransform().GetPosition());
		float fAngle = fwAngle::GetRadianAngleBetweenPoints(vPedToChatToPosition.x, vPedToChatToPosition.y, vPedPosition.x, vPedPosition.y);
		pPed->SetDesiredHeading(fAngle);
	}
	else
	{
		pPed->SetDesiredHeading(m_fHeading);
	}

	if (m_timer.IsSet() && m_timer.IsOutOfTime())
	{
		SetState(State_Finish);
	}

	return FSM_Continue;
}

void CTaskChat::PlayingClip_OnEnter(CPed* UNUSED_PARAM(pPed))
{
	m_iFlags.ClearFlag(CF_ShouldPlayClip);
	SetNewTask(rage_new CTaskRunClip(
							m_clipDictIndex, 
							m_clipHashKey, 
							m_fBlendDelta, 
							m_iPriority, 
							m_iClipFlags, 
							m_iBoneMask,		
							m_iDuration, 
							m_iTaskFlags)
);
	m_bStartedIdleTimer = false;
}

CTask::FSM_Return CTaskChat::PlayingClip_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	if (GetIsSubtaskFinished(CTaskTypes::TASK_RUN_CLIP))
	{
		m_IdleTimer.Reset();
		m_bStartedIdleTimer = true;
	}

	aiTask* pTask =  m_pPedToChatTo->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_CHAT);
	if (!pTask)
	{
		SetState(State_TurnToFacePed);
		return FSM_Continue;
	}

	if (m_bStartedIdleTimer && m_IdleTimer.Tick())
	{
		m_iFlags.ClearFlag(CF_IsPlayingClip);
		SetState(State_Idle);
	}
	return FSM_Continue;
}

void CTaskChat::TurnToFacePed_OnEnter	(CPed* UNUSED_PARAM(pPed))
{

}

CTask::FSM_Return CTaskChat::TurnToFacePed_OnUpdate	(CPed* pPed)
{
	pPed->SetDesiredHeading(VEC3V_TO_VECTOR3(m_pPedToChatTo->GetTransform().GetPosition()));

	return FSM_Continue;
	// if finished - quit
}

CTask::FSM_Return CTaskChat::Idle_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	if (m_iFlags.IsFlagSet(CF_ShouldPlayClip))
	{
		SetState(State_PlayingClip);
		return FSM_Continue;
	}

	if (m_iFlags.IsFlagSet(CF_ShouldLeave))
	{
		SetState(State_Finish);
		return FSM_Continue;
	}

	aiTask* pTask =  m_pPedToChatTo->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_CHAT);
	if (!pTask)
	{
		SetState(State_TurnToFacePed);
		return FSM_Continue;
	}

	return FSM_Continue;
}

void CTaskChat::PlayClip(const char* pClipName, const strStreamingObjectName pClipDictName, const u32 iPriority, const float fBlendDelta, const s32 flags, const s32 boneMask, const int iDuration, const s32 iTaskFlags)
{
	taskAssertf(pClipDictName.IsNotNull(), "Null clip dictionary name\n");
	taskAssertf(pClipName, "Null clip name\n");

	if (pClipDictName.IsNotNull())
	{
		m_clipDictIndex = fwAnimManager::FindSlot(pClipDictName).Get();
	}

	if (pClipName)
	{
		taskAssertf(strlen(pClipName)<ANIM_NAMELEN, "Animation name is too long : %s ", pClipName);
		m_clipHashKey = atStringHash(pClipName);
	}

	m_iPriority = iPriority;
	m_fBlendDelta = fBlendDelta;
	m_iClipFlags = flags;
	m_iBoneMask = boneMask;
	m_iDuration = iDuration;
	m_iTaskFlags = iTaskFlags;	

	m_iFlags.SetFlag(CF_ShouldPlayClip);
	m_iFlags.SetFlag(CF_IsPlayingClip);
}

#if !__FINAL
const char * CTaskChat::GetStaticStateName( s32 iState )
{
	// Make sure FSM state is valid
	taskAssert(iState >= State_GoToPosition && iState <= State_Finish);
	static const char* aChatStateNames[] = 
	{
		"State_GoToPosition",
		"State_QuickChat",
		"State_WaitingToChat",
		"State_Chatting",
		"State_PlayingAnim",
		"State_Idle",
		"State_TurnToFacePed",
		"State_Finish"
	};

	return aChatStateNames[iState];
}
#endif // !__FINAL

