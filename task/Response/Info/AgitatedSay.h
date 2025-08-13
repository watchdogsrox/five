#ifndef INC_AGITATED_SAY_H
#define INC_AGITATED_SAY_H

// Rage headers
#include "atl/array.h"
#include "atl/hashstring.h"
#include "fwutil/Flags.h"
#include "parser/macros.h"
#include "system/bit.h"

// Game headers
#include "task/Default/AmbientAudio.h"

// Game forward declarations
class CAgitatedCondition;

////////////////////////////////////////////////////////////////////////////////
// CAgitatedSay
////////////////////////////////////////////////////////////////////////////////

class CAgitatedSay
{

public:

	enum Flags
	{
		CanInterrupt					= BIT0,
		IgnoreForcedFailure				= BIT1,
		WaitForExistingAudioToFinish	= BIT2,
		WaitUntilFacing					= BIT3,
	};

public:
	
	CAgitatedSay();
	~CAgitatedSay();

public:

	const	CAmbientAudio&			GetAudio()	const	{ return m_Audio; }
	const	atArray<CAmbientAudio>&	GetAudios()	const	{ return m_Audios; }
			fwFlags8&				GetFlags()			{ return m_Flags; }
	const	fwFlags8				GetFlags()	const	{ return m_Flags; }

public:

	bool IsAudioFlagSet(u8 uFlag) const;
	bool IsValid() const;

private:
	
	CAmbientAudio			m_Audio;
	atArray<CAmbientAudio>	m_Audios;
	fwFlags8				m_Flags;
	
	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionalSay
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionalSay
{

public:

	CAgitatedConditionalSay();
	~CAgitatedConditionalSay();

public:

	const CAgitatedCondition*	GetCondition()	const { return m_Condition; }
	const CAgitatedSay&			GetSay()		const { return m_Say; }

public:

	bool HasValidSay() const
	{
		return m_Say.IsValid();
	}

private:

	CAgitatedCondition*	m_Condition;
	CAgitatedSay		m_Say;

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedTalkResponse
////////////////////////////////////////////////////////////////////////////////

class CAgitatedTalkResponse
{

public:

	CAgitatedTalkResponse();
	~CAgitatedTalkResponse();

public:

	const CAmbientAudio& GetAudio()	const { return m_Audio; }

public:

	bool IsValid() const
	{
		return (m_Audio.IsValid());
	}

private:

	CAmbientAudio m_Audio;

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionalTalkResponse
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionalTalkResponse
{

public:

	CAgitatedConditionalTalkResponse();
	~CAgitatedConditionalTalkResponse();

public:

	const CAgitatedCondition*		GetCondition()		const { return m_Condition; }
	const CAgitatedTalkResponse&	GetTalkResponse()	const { return m_TalkResponse; }

public:

	bool HasValidTalkResponse() const
	{
		return m_TalkResponse.IsValid();
	}

private:

	CAgitatedCondition*		m_Condition;
	CAgitatedTalkResponse	m_TalkResponse;

	PAR_SIMPLE_PARSABLE;
};

#endif // INC_AGITATED_SAY_H
