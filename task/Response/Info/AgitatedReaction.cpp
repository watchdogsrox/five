// File header
#include "Task/Response/Info/AgitatedReaction.h"

// Game headers
#include "task/Response/Info/AgitatedConditions.h"

AI_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////
// CAgitatedReaction
////////////////////////////////////////////////////////////////////////////////

CAgitatedReaction::CAgitatedReaction()
: m_Condition(NULL)
, m_Action(NULL)
, m_Anger(0.0f)
, m_Fear(0.0f)
, m_Flags(0)
{
}

CAgitatedReaction::~CAgitatedReaction()
{
	delete m_Condition;
	delete m_Action;
}
