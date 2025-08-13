#ifndef INC_AGITATED_CONTEXT_H
#define INC_AGITATED_CONTEXT_H

// Rage headers
#include "atl/array.h"
#include "atl/hashstring.h"
#include "parser/macros.h"

// Game headers
#include "Task/Response/Info/AgitatedSituation.h"

////////////////////////////////////////////////////////////////////////////////
// CAgitatedContext
////////////////////////////////////////////////////////////////////////////////

class CAgitatedContext
{

public:

	CAgitatedContext();
	~CAgitatedContext();

public:

			atHashWithStringNotFinal		GetResponse()	const { return m_Response; }
	const	atArray<CAgitatedSituation>&	GetSituations()	const { return m_Situations; }

private:

	atHashWithStringNotFinal	m_Response;
	atArray<CAgitatedSituation>	m_Situations;

	PAR_SIMPLE_PARSABLE;
};

#endif // INC_AGITATED_CONTEXT_H
