#ifndef INC_AGITATED_STATE_H
#define INC_AGITATED_STATE_H

// Rage headers
#include "atl/array.h"
#include "atl/hashstring.h"
#include "fwutil/Flags.h"
#include "parser/macros.h"

// Game headers
#include "task/Default/AmbientAudio.h"
#include "Task/Response/Info/AgitatedActions.h"

////////////////////////////////////////////////////////////////////////////////
// CAgitatedState
////////////////////////////////////////////////////////////////////////////////

class CAgitatedState
{

public:

	enum Flags
	{
		InitialState			= BIT0,
		FinalState				= BIT1,
		UseWhenForcedAudioFails	= BIT2,
		ClearHostility			= BIT3,
	};

public:

	CAgitatedState();
	~CAgitatedState();
	
public:

	const	CAgitatedAction*						GetAction()						const { return m_Action; }
	const	CAgitatedConditionalAction&				GetConditionalAction()			const { return m_ConditionalAction; }
	const	atArray<CAgitatedConditionalAction>&	GetConditionalActions()			const { return m_ConditionalActions; }
	const	CAgitatedConditionalTalkResponse&		GetConditionalTalkResponse()	const { return m_ConditionalTalkResponse; }
			fwFlags8								GetFlags()						const { return m_Flags; }
	const	atArray<atHashWithStringNotFinal>&		GetReactions()					const { return m_Reactions; }
			float									GetTimeToListen()				const { return m_TimeToListen; }
	const	CAgitatedTalkResponse&					GetTalkResponse()				const { return m_TalkResponse; }
	const	atArray<CAgitatedTalkResponse>&			GetTalkResponses()				const { return m_TalkResponses; }
	
private:

	CAgitatedAction*					m_Action;
	CAgitatedConditionalAction			m_ConditionalAction;
	atArray<CAgitatedConditionalAction>	m_ConditionalActions;
	fwFlags8							m_Flags;
	atArray<atHashWithStringNotFinal>	m_Reactions;
	float								m_TimeToListen;
	CAgitatedTalkResponse				m_TalkResponse;
	CAgitatedConditionalTalkResponse	m_ConditionalTalkResponse;
	atArray<CAgitatedTalkResponse>		m_TalkResponses;

	PAR_SIMPLE_PARSABLE;
};

#endif // INC_AGITATED_STATE_H
