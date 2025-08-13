#ifndef INC_AGITATED_PERSONALITY_H
#define INC_AGITATED_PERSONALITY_H

// Rage headers
#include "atl/binmap.h"
#include "atl/hashstring.h"
#include "parser/macros.h"
#include "fwutil/Flags.h"

// Game headers
#include "Task/Response/Info/AgitatedContext.h"

////////////////////////////////////////////////////////////////////////////////
// CAgitatedPersonality
////////////////////////////////////////////////////////////////////////////////

class CAgitatedPersonality
{

public:

	enum Flags
	{
		IsAggressive = BIT0,
	};

public:

	CAgitatedPersonality();
	~CAgitatedPersonality();

public:

	fwFlags8 GetFlags() const { return m_Flags; }

public:

	const CAgitatedContext* GetContext(atHashWithStringNotFinal hName) const
	{
		return m_Contexts.SafeGet(hName);
	}

private:

	fwFlags8												m_Flags;
	atBinaryMap<CAgitatedContext, atHashWithStringNotFinal>	m_Contexts;

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedPersonalities
////////////////////////////////////////////////////////////////////////////////

class CAgitatedPersonalities
{

public:

	CAgitatedPersonalities();
	~CAgitatedPersonalities();
	
public:

	const CAgitatedPersonality* GetPersonality(atHashWithStringNotFinal hName) const
	{
		return m_Personalities.SafeGet(hName);
	}

private:

	atBinaryMap<CAgitatedPersonality, atHashWithStringNotFinal> m_Personalities;

	PAR_SIMPLE_PARSABLE;
};

#endif // INC_AGITATED_PERSONALITY_H
