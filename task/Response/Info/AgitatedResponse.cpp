// File header
#include "Task/Response/Info/AgitatedResponse.h"

AI_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////
// CAgitatedResponse
////////////////////////////////////////////////////////////////////////////////

CAgitatedResponse::CAgitatedResponse()
: m_Flags(0)
{
}

CAgitatedResponse::~CAgitatedResponse()
{
}

atHashWithStringNotFinal CAgitatedResponse::FindInitialState() const
{
	//Find the initial state.
	for(int i = 0; i < m_States.GetCount(); ++i)
	{
		//Ensure the flag is set.
		if(!m_States.GetItem(i)->GetFlags().IsFlagSet(CAgitatedState::InitialState))
		{
			continue;
		}

		return *m_States.GetKey(i);
	}

	return atHashWithStringNotFinal::Null();
}

atHashWithStringNotFinal CAgitatedResponse::FindNameForReaction(const CAgitatedReaction* pReaction) const
{
	//Check if the reaction is valid.
	if(pReaction)
	{
		//Iterate over the reactions.
		for(int i = 0; i < m_Reactions.GetCount(); ++i)
		{
			//Ensure the reaction matches.
			if(pReaction != m_Reactions.GetItem(i))
			{
				continue;
			}

			return *m_Reactions.GetKey(i);
		}
	}

	return atHashWithStringNotFinal::Null();
}

atHashWithStringNotFinal CAgitatedResponse::FindNameForState(const CAgitatedState* pState) const
{
	//Check if the state is valid.
	if(pState)
	{
		//Iterate over the states.
		for(int i = 0; i < m_States.GetCount(); ++i)
		{
			//Ensure the state matches.
			if(pState != m_States.GetItem(i))
			{
				continue;
			}

			return *m_States.GetKey(i);
		}
	}

	return atHashWithStringNotFinal::Null();
}

atHashWithStringNotFinal CAgitatedResponse::FindStateToUseWhenForcedAudioFails() const
{
	//Iterate over the states.
	for(int i = 0; i < m_States.GetCount(); ++i)
	{
		//Ensure the flag is set.
		if(!m_States.GetItem(i)->GetFlags().IsFlagSet(CAgitatedState::UseWhenForcedAudioFails))
		{
			continue;
		}

		return *m_States.GetKey(i);
	}

	return atHashWithStringNotFinal::Null();
}

////////////////////////////////////////////////////////////////////////////////
// CAgitatedResponses
////////////////////////////////////////////////////////////////////////////////

CAgitatedResponses::CAgitatedResponses()
: m_Responses()
{

}

CAgitatedResponses::~CAgitatedResponses()
{

}

atHashWithStringNotFinal CAgitatedResponses::FindNameForResponse(const CAgitatedResponse* pResponse) const
{
	//Check if the response is valid.
	if(pResponse)
	{
		//Iterate over the responses.
		for(int i = 0; i < m_Responses.GetCount(); ++i)
		{
			//Ensure the response matches.
			if(pResponse != m_Responses.GetItem(i))
			{
				continue;
			}

			return *m_Responses.GetKey(i);
		}
	}

	return atHashWithStringNotFinal::Null();
}
