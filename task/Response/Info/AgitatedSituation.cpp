// File header
#include "Task/Response/Info/AgitatedSituation.h"

// Game headers
#include "task/Response/Info/AgitatedConditions.h"

AI_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////
// CAgitatedSituation
////////////////////////////////////////////////////////////////////////////////

CAgitatedSituation::CAgitatedSituation()
: m_Condition(NULL)
, m_Response()
{

}

CAgitatedSituation::~CAgitatedSituation()
{
	delete m_Condition;
}
