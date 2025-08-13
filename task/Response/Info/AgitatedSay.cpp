// File header
#include "Task/Response/Info/AgitatedSay.h"

// Game headers
#include "task/Response/Info/AgitatedConditions.h"

AI_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////
// CAgitatedSay
////////////////////////////////////////////////////////////////////////////////

CAgitatedSay::CAgitatedSay()
: m_Flags(0)
{

}

CAgitatedSay::~CAgitatedSay()
{

}

bool CAgitatedSay::IsAudioFlagSet(u8 uFlag) const
{
	//Check if the audio is valid.
	if(m_Audio.IsValid())
	{
		//Check if the flag is set.
		return m_Audio.GetFlags().IsFlagSet(uFlag);
	}

	//Check if the audios are valid.
	int iCount = m_Audios.GetCount();
	if(iCount > 0)
	{
		//Iterate over the audios.
		for(int i = 0; i < iCount; ++i)
		{
			//Check if the flag is set.
			if(m_Audios[i].GetFlags().IsFlagSet(uFlag))
			{
				return true;
			}
		}
	}

	return false;
}

bool CAgitatedSay::IsValid() const
{
	//Check if the audio is valid.
	if(m_Audio.IsValid())
	{
		return true;
	}

	//Check if the audios are valid.
	int iCount = m_Audios.GetCount();
	if(iCount > 0)
	{
		//Iterate over the audios.
		for(int i = 0; i < iCount; ++i)
		{
			//Check if the audio is valid.
			if(m_Audios[i].IsValid())
			{
				return true;
			}
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionalSay
////////////////////////////////////////////////////////////////////////////////

CAgitatedConditionalSay::CAgitatedConditionalSay()
: m_Condition(NULL)
{

}

CAgitatedConditionalSay::~CAgitatedConditionalSay()
{
	delete m_Condition;
}

////////////////////////////////////////////////////////////////////////////////
// CAgitatedTalkResponse
////////////////////////////////////////////////////////////////////////////////

CAgitatedTalkResponse::CAgitatedTalkResponse()
{

}

CAgitatedTalkResponse::~CAgitatedTalkResponse()
{

}

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionalTalkResponse
////////////////////////////////////////////////////////////////////////////////

CAgitatedConditionalTalkResponse::CAgitatedConditionalTalkResponse()
: m_Condition(NULL)
{

}

CAgitatedConditionalTalkResponse::~CAgitatedConditionalTalkResponse()
{
	delete m_Condition;
}
