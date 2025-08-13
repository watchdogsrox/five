#ifndef INC_AGITATED_REACTIONS_H
#define INC_AGITATED_REACTIONS_H

// Rage headers
#include "atl/array.h"
#include "atl/hashstring.h"
#include "fwutil/Flags.h"
#include "parser/macros.h"

// Game headers
#include "task/Response/Info/AgitatedActions.h"
#include "task/Response/Info/AgitatedSay.h"

// Forward declarations
class CAgitatedCondition;

////////////////////////////////////////////////////////////////////////////////
// CAgitatedReaction
////////////////////////////////////////////////////////////////////////////////

class CAgitatedReaction
{

public:

	enum Flags
	{
		DontClearFlags	= BIT0,
		DontClearSay	= BIT1,
	};

public:

	CAgitatedReaction();
	~CAgitatedReaction();
	
public:

	const	CAgitatedCondition*					GetCondition()			const { return m_Condition; }
	const	CAgitatedAction*					GetAction()				const { return m_Action; }
	const	CAgitatedConditionalAction&			GetConditionalAction()	const { return m_ConditionalAction; }
			atHashWithStringNotFinal			GetState()				const { return m_State; }
			float								GetAnger()				const { return m_Anger; }
			float								GetFear()				const { return m_Fear; }
	const	CAgitatedSay&						GetSay()				const { return m_Say; }
	const	CAgitatedConditionalSay&			GetConditionalSay()		const { return m_ConditionalSay; }
	const	atArray<CAgitatedConditionalSay>&	GetConditionalSays()	const { return m_ConditionalSays; }
	const	fwFlags8							GetFlags()				const { return m_Flags; }

private:

	CAgitatedCondition*					m_Condition;
	CAgitatedAction*					m_Action;
	CAgitatedConditionalAction			m_ConditionalAction;
	atHashWithStringNotFinal			m_State;
	float								m_Anger;
	float								m_Fear;
	CAgitatedSay						m_Say;
	CAgitatedConditionalSay				m_ConditionalSay;
	atArray<CAgitatedConditionalSay>	m_ConditionalSays;
	fwFlags8							m_Flags;

	PAR_SIMPLE_PARSABLE;
};

#endif // INC_AGITATED_REACTIONS_H
