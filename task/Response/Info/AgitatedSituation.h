#ifndef INC_AGITATED_SITUATION_H
#define INC_AGITATED_SITUATION_H

// Rage headers
#include "atl/hashstring.h"
#include "parser/macros.h"

// Forward declarations
class CAgitatedCondition;

////////////////////////////////////////////////////////////////////////////////
// CAgitatedSituation
////////////////////////////////////////////////////////////////////////////////

class CAgitatedSituation
{

public:

	CAgitatedSituation();
	~CAgitatedSituation();
	
public:

	const CAgitatedCondition*		GetCondition()	const { return m_Condition; }
	const atHashWithStringNotFinal	GetResponse()	const { return m_Response; }

private:

	CAgitatedCondition*			m_Condition;
	atHashWithStringNotFinal	m_Response;

	PAR_SIMPLE_PARSABLE;
};

#endif // INC_AGITATED_SITUATION_H
