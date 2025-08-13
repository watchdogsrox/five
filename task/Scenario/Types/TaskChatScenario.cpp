//-------------------------------------------------------------------------
// Basic chat task implementation
//-------------------------------------------------------------------------

#include "TaskChatScenario.h"
#include "TaskUseScenario.h"

#include "Event/Event.h"

#include "Peds/ped.h"
#include "Peds/PedIntelligence.h"

AI_OPTIMISATIONS()

CTaskChatScenario::CTaskChatScenario( s32 scenarioType, CPed* pOtherPed, s32 iChattingContext, CScenarioPoint* pScenarioPoint, ChatState initialChatState, s32 iSubTaskType )
: CTaskScenario(scenarioType,pScenarioPoint),
m_pOtherPed(pOtherPed),
m_initialChatState(initialChatState),
m_fSpeechDelay(0.0f),
m_fListenDelay(0.5f),
m_iSubTaskType(iSubTaskType),
m_iChattingContext(iChattingContext),
m_OtherPedChatTaskState(0),
m_WaitForOtherPed(false)
{
	SetInternalTaskType(CTaskTypes::TASK_CHAT_SCENARIO);
}

CTaskChatScenario::~CTaskChatScenario( )
{
	if (m_pOtherPed)
	{
		m_pOtherPed = NULL;
	}
}

#if	!__FINAL

const char * CTaskChatScenario::GetStaticStateName( s32 iState  )
{
	taskFatalAssertf(iState >=0 && iState <= State_ListeningToResponse, "State outside range!");

	const char* aStateNames[] = 
	{
		"State_Start",
		"State_AboutToSayStatement",
		"State_AboutToSayResponse",
		"State_SayStatement",
		"State_SayResponse",
		"State_AboutToListenToStatement",
		"State_ListeningToStatement",
		"State_ListeningToResponse"
	};	

	return aStateNames[iState];
}
#endif // !__FINAL


aiTask* CTaskChatScenario::Copy() const
{
	return rage_new CTaskChatScenario( m_iScenarioIndex, m_pOtherPed, m_iChattingContext, GetScenarioPoint(), m_initialChatState, m_iSubTaskType );
}

float CTaskChatScenario::TIME_BETWEEN_STATEMENTS_MIN = 10.0f;
float CTaskChatScenario::TIME_BETWEEN_STATEMENTS_MAX = 20.0f;
float CTaskChatScenario::TIME_BETWEEN_RESPONSE_MIN = 0.0f;
float CTaskChatScenario::TIME_BETWEEN_RESPONSE_MAX = 0.2f;

bool CTaskChatScenario::ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent)
{
	// Don't abort if its to be converted into a dummy ped
	if( iPriority != CTask::ABORT_PRIORITY_IMMEDIATE && 
		pEvent &&
		((CEvent*)pEvent)->GetEventType() == EVENT_DUMMY_CONVERSION )
	{
		return false;
	}

	// Base class
	return CTaskScenario::ShouldAbort( iPriority, pEvent );
}

void CTaskChatScenario::DoAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	// Stop any chat
	if( ( GetState() == State_SayStatement ||
		GetState() == State_SayResponse ) &&
		pPed->GetSpeechAudioEntity()->IsAmbientSpeechPlaying() )
	{
		pPed->GetSpeechAudioEntity()->StopSpeech();
	}

	pPed->SetSpeakerListenedTo(NULL);

	// Base class
	CTaskScenario::DoAbort( iPriority, pEvent );
}

const char* CTaskChatScenario::GetSayString( bool bStatement )
{
	switch(m_iChattingContext)
	{
	case AUD_CONVERSATION_TOPIC_SMOKER:
		if( bStatement )
		{
			return "CONV_SMOKE_STATE";
		}
		else
		{
			return "CONV_SMOKE_RESP";
		}
	case AUD_CONVERSATION_TOPIC_BUSINESS:
		if( bStatement )
		{
			return "CONV_BIZ_STATE";
		}
		else
		{
			return "CONV_BIZ_RESP";
		}
	case AUD_CONVERSATION_TOPIC_GANG:
		if( bStatement )
		{
			return "CONV_GANG_STATE";
		}
		else
		{
			return "CONV_GANG_RESP";
		}
	case AUD_CONVERSATION_TOPIC_CONSTRUCTION:
		if( bStatement )
		{
			return "CONV_CONSTRUCTION_STATE";
		}
		else
		{
			return "CONV_CONSTRUCTION_RESP";
		}
	case AUD_CONVERSATION_TOPIC_BUM:
		if( bStatement )
		{
			return "CONV_BUM_STATE";
		}
		else
		{
			return "CONV_BUM_RESP";
		}
	case AUD_CONVERSATION_TOPIC_GUARD:
		if( bStatement )
		{
			return "CONV_GENERIC_STATE";
		}
		else
		{
			return "CONV_GENERIC_RESP";
		}
	default:
		{
			taskAssertf(0, "No chat string for chat context [%d]", m_iChattingContext);
			return "UNKNOWN";
		}
	}
}


CTask::FSM_Return CTaskChatScenario::ProcessPreFSM()
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	m_WaitForOtherPed = false;

	if( !ValidateScenarioInfo() )
	{
		return FSM_Quit;
	}

	// Sanity check the other ped
	CTaskChatScenario* pOtherPedChatTask = NULL;
	if( !m_pOtherPed )
	{
		return FSM_Quit;
	}
	else
	{
		pOtherPedChatTask = static_cast<CTaskChatScenario*>(m_pOtherPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_CHAT_SCENARIO)); 

		if( !pOtherPedChatTask )
		{
			// Check to see if the chat scenario has come from chat task
			aiTask* pTask = m_pOtherPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_CHAT);
			if (pTask)
			{
				// If it has wait and retry to get the chat scenario task
				m_WaitForOtherPed = true;
				return FSM_Continue;
			}
			else
			{
				return FSM_Quit;
			}
		}

		// This is used elsewhere, storing it in a member variable saves us from having to find the task again.
		m_OtherPedChatTaskState = pOtherPedChatTask->GetState();

		const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		const Vector3 vOtherPedPosition = VEC3V_TO_VECTOR3(m_pOtherPed->GetTransform().GetPosition());
		float fAngle = fwAngle::GetRadianAngleBetweenPoints(vOtherPedPosition.x, vOtherPedPosition.y, vPedPosition.x, vPedPosition.y);
		// Turn to face the ped
		pPed->SetDesiredHeading(fAngle);
	}
	pPed->GetPedIntelligence()->SetCheckShockFlag(true);

	return FSM_Continue;
}


CTask::FSM_Return CTaskChatScenario::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	// This task never changes tasks during transitions
	SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);

	FSM_Begin
		FSM_State(State_Start)
			FSM_OnEnter
				Start_OnEnter();
			FSM_OnUpdate
				Start_OnUpdate();

		FSM_State(State_AboutToSayStatement)
			FSM_OnUpdate
				AboutTo_OnUpdate(pPed);

		FSM_State(State_AboutToSayResponse)
			FSM_OnUpdate
				AboutTo_OnUpdate(pPed);

		FSM_State(State_AboutToListenToStatement)
			FSM_OnUpdate
				pPed->SetSpeakerListenedTo(m_pOtherPed);
				SetState(State_ListeningToStatement);

		FSM_State(State_SayStatement)
			FSM_OnUpdate
				Say_OnUpdate(pPed);

		FSM_State(State_SayResponse)
			FSM_OnUpdate
				Say_OnUpdate(pPed);

		FSM_State(State_ListeningToStatement)
			FSM_OnUpdate
				Listening_OnUpdate(m_OtherPedChatTaskState, pPed);

		FSM_State(State_ListeningToResponse)
			FSM_OnEnter
				m_fListenDelay = 0.5f;
			FSM_OnUpdate
				// A delay to stop peds who are saying statements from instantly going into another statement
				m_fListenDelay -= fwTimer::GetSystemTimeStep();
				if (m_fListenDelay <= 0)
				{
					Listening_OnUpdate(m_OtherPedChatTaskState, pPed);
				}
	FSM_End
}

void CTaskChatScenario::AboutTo_OnUpdate( CPed* pPed )
{
	// Start chatting after a random delay
	m_fSpeechDelay -= fwTimer::GetSystemTimeStep();
	if (m_fSpeechDelay <= 0.0f )
	{
		if (GetState() == State_AboutToSayStatement )
		{
			if (pPed->NewSay(GetSayString(true),0,true)) // TODO: remove hard coded voice assignment 
			{
				SetState(State_SayStatement);
			}
			// we'll sit in the AboutToSayState until we successfully say something
		}
		else
		{
			pPed->NewSay(GetSayString(false),0, true); // force a reply // TODO: remove hard coded voice assignment 	
			SetState(State_SayResponse);
		}
	}
}

void CTaskChatScenario::Say_OnUpdate( CPed* pPed )
{
	if( !pPed->GetSpeechAudioEntity()->IsAmbientSpeechPlaying() )
	{
		// If we've finished saying a response, randomly follow it up with another statement
		if( GetState() == State_SayResponse &&
			( fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < 0.33f ) )
		{
			SetState(State_AboutToSayStatement);
			// for gang convs, 50:50 chance they're carry on immediately, or leave a decent length pause
			if (m_iChattingContext== AUD_CONVERSATION_TOPIC_GANG && fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < 0.5f)
			{
				m_fSpeechDelay = fwRandom::GetRandomNumberInRange(0.5f, 1.0f);
			}
			else
			{
				m_fSpeechDelay = fwRandom::GetRandomNumberInRange(TIME_BETWEEN_STATEMENTS_MIN, TIME_BETWEEN_STATEMENTS_MAX);
			}
		}
		// otherwise wait for the other person to speak again
		else if( GetState() == State_SayResponse )
		{
			pPed->SetSpeakerListenedTo(m_pOtherPed);
			SetState(State_ListeningToStatement);
		}
		// We were speaking a statement, now listen to the other peds response
		else
		{
			pPed->SetSpeakerListenedTo(m_pOtherPed);
			SetState(State_ListeningToResponse);
		}
	}
}


void CTaskChatScenario::Start_OnEnter()
{
	m_fSpeechDelay = 1.0f;// seems about right to stand the greatest chance of hearing and seeing it. //fwRandom::GetRandomNumberInRange(TIME_BETWEEN_STATEMENTS_MIN, TIME_BETWEEN_STATEMENTS_MAX);
	if( m_iSubTaskType == CTaskTypes::TASK_USE_SCENARIO )
	{
		SetNewTask(rage_new CTaskUseScenario(GetScenarioType(), GetScenarioPoint(), (CTaskUseScenario::SF_StartInCurrentPosition | CTaskUseScenario::SF_SkipEnterClip | CTaskUseScenario::SF_CheckConditions) ) );
	}
}


CTask::FSM_Return CTaskChatScenario::Start_OnUpdate()
{
	if(m_WaitForOtherPed)
	{
		// The other ped hasn't starting its CTaskChatScenario yet, wait for it.
		return FSM_Continue;
	}

	SetState(m_initialChatState);

	return FSM_Continue;
}


void CTaskChatScenario::Listening_OnUpdate( s32 otherChatState, CPed* pPed )
{
	if( otherChatState == State_ListeningToStatement )
	{
		SetState(State_AboutToSayStatement);	
		// for gang convs, 50:50 chance we'll leave a decent pause, or carry right on
		if (m_iChattingContext==AUD_CONVERSATION_TOPIC_GANG && fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < 0.5f)
		{
			m_fSpeechDelay = fwRandom::GetRandomNumberInRange(0.5f, 1.0f);
		}
		else
		{
			m_fSpeechDelay = fwRandom::GetRandomNumberInRange(TIME_BETWEEN_STATEMENTS_MIN, TIME_BETWEEN_STATEMENTS_MAX);
		}
		pPed->RemoveSpeakerListenedTo();
	}
	// If we were listening and the other ped is now waiting for us to speak, speak to them
	else if (otherChatState == State_ListeningToResponse)
	{
		SetState(State_AboutToSayResponse);
		m_fSpeechDelay = fwRandom::GetRandomNumberInRange(TIME_BETWEEN_RESPONSE_MIN, TIME_BETWEEN_RESPONSE_MAX);
		pPed->RemoveSpeakerListenedTo();
	}
	// If we were listening to a response and the other ped has decided to speak, listen to the statement
	else if( GetState() == State_ListeningToResponse && otherChatState == State_SayStatement )
	{
		pPed->SetSpeakerListenedTo(m_pOtherPed);
		SetState(State_ListeningToStatement);
	}
}
