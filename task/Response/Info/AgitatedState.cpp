// File header
#include "Task/Response/Info/AgitatedState.h"

// Game headers
#include "Task/Response/Info/AgitatedActions.h"

AI_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////
// CAgitatedState
////////////////////////////////////////////////////////////////////////////////

CAgitatedState::CAgitatedState()
: m_Action(NULL)
, m_Flags(0)
, m_TimeToListen(-1.0f)
{
}

CAgitatedState::~CAgitatedState()
{
	delete m_Action;
}
