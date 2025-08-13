#ifndef INC_AGITATED_RESPONSES_H
#define INC_AGITATED_RESPONSES_H

// Rage headers
#include "atl/array.h"
#include "atl/binmap.h"
#include "atl/hashstring.h"
#include "parser/macros.h"

// Game headers
#include "task/Default/AmbientAudio.h"
#include "Task/Response/Info/AgitatedConditions.h"
#include "Task/Response/Info/AgitatedReaction.h"
#include "Task/Response/Info/AgitatedState.h"

////////////////////////////////////////////////////////////////////////////////
// CAgitatedResponse
////////////////////////////////////////////////////////////////////////////////

class CAgitatedResponse
{

public:

	enum Flags
	{
		ProcessLeader = BIT0,
	};

public:

	struct OnFootClipSet
	{
		enum Flags
		{
			DontReset	= BIT0,
		};
		
		OnFootClipSet()
		{}
		
		~OnFootClipSet()
		{
			delete m_Condition;
		}
		
		CAgitatedCondition*			m_Condition;
		atHashWithStringNotFinal	m_Name;
		fwFlags8					m_Flags;
		
		PAR_SIMPLE_PARSABLE;
	};

public:

	CAgitatedResponse();
	~CAgitatedResponse();
	
public:

	const	fwFlags8						GetFlags()			const { return m_Flags; }
	const	atArray<OnFootClipSet>&			GetOnFootClipSets()	const { return m_OnFootClipSets; }
	const	CAgitatedTalkResponse&			GetTalkResponse()	const { return m_TalkResponse; }
	const	atArray<CAgitatedTalkResponse>&	GetTalkResponses()	const { return m_TalkResponses; }

public:

	const CAgitatedTalkResponse* GetFollowUp(atHashWithStringNotFinal hName) const
	{
		return m_FollowUps.SafeGet(hName);
	}

	const CAgitatedReaction* GetReaction(atHashWithStringNotFinal hName) const
	{
		return m_Reactions.SafeGet(hName);
	}

	const CAgitatedState* GetState(atHashWithStringNotFinal hName) const
	{
		return m_States.SafeGet(hName);
	}
	
public:

	atHashWithStringNotFinal FindInitialState() const;
	atHashWithStringNotFinal FindNameForReaction(const CAgitatedReaction* pReaction) const;
	atHashWithStringNotFinal FindNameForState(const CAgitatedState* pState) const;
	atHashWithStringNotFinal FindStateToUseWhenForcedAudioFails() const;

private:

	fwFlags8														m_Flags;
	atArray<OnFootClipSet>											m_OnFootClipSets;
	CAgitatedTalkResponse											m_TalkResponse;
	atArray<CAgitatedTalkResponse>									m_TalkResponses;
	atBinaryMap<CAgitatedTalkResponse, atHashWithStringNotFinal>	m_FollowUps;
	atBinaryMap<CAgitatedState, atHashWithStringNotFinal>			m_States;
	atBinaryMap<CAgitatedReaction, atHashWithStringNotFinal>		m_Reactions;

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedResponses
////////////////////////////////////////////////////////////////////////////////

class CAgitatedResponses
{

public:

	CAgitatedResponses();
	~CAgitatedResponses();
	
public:

	const CAgitatedResponse* GetResponse(atHashWithStringNotFinal hName) const
	{
		return m_Responses.SafeGet(hName);
	}

public:

	atHashWithStringNotFinal FindNameForResponse(const CAgitatedResponse* pResponse) const;

private:

	atBinaryMap<CAgitatedResponse, atHashWithStringNotFinal> m_Responses;

	PAR_SIMPLE_PARSABLE;
};

#endif // INC_AGITATED_RESPONSES_H
