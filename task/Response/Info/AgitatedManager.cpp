// File header
#include "Task/Response/Info/AgitatedManager.h"

// Rage headers
#include "parser/macros.h"
#include "parser/manager.h"

// Game headers
#include "ai/ambient/ConditionalAnims.h"
#include "task/Response/Info/AgitatedActions.h"
#include "task/Response/Info/AgitatedConditions.h"
#include "task/Response/Info/AgitatedEnums.h"
#include "task/Response/Info/AgitatedReaction.h"
#include "task/Response/Info/AgitatedSituation.h"
#include "task/Response/Info/AgitatedState.h"

// Parser files
#include "AgitatedActions_parser.h"
#include "AgitatedConditions_parser.h"
#include "AgitatedContext_parser.h"
#include "AgitatedEnums_parser.h"
#include "AgitatedPersonality_parser.h"
#include "AgitatedReaction_parser.h"
#include "AgitatedResponse_parser.h"
#include "AgitatedSay_parser.h"
#include "AgitatedSituation_parser.h"
#include "AgitatedState_parser.h"
#include "AgitatedTrigger_parser.h"

AI_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////
// CAgitatedManager
////////////////////////////////////////////////////////////////////////////////

CAgitatedManager *CAgitatedManager::sm_Instance = NULL;

CAgitatedManager::CAgitatedManager()
: m_Personalities()
, m_Responses()
, m_Triggers()
, m_bAgitationEventsAreSuppressed(false)
{
	//Parse the personalities.
	PARSER.LoadObject("common:/data/ai/agitatedpersonalities", "meta", m_Personalities);
	
	//Parse the responses.
	PARSER.LoadObject("common:/data/ai/agitatedresponses", "meta", m_Responses);

	//Parse the triggers.
	PARSER.LoadObject("common:/data/ai/agitatedtriggers", "meta", m_Triggers);
}

CAgitatedManager::~CAgitatedManager()
{
}

void CAgitatedManager::Init(unsigned UNUSED_PARAM(initMode))
{
	Assert(!sm_Instance);
	sm_Instance = rage_new CAgitatedManager;
}

void CAgitatedManager::Shutdown(unsigned UNUSED_PARAM(shutdownMode))
{
	Assert(sm_Instance);
	delete sm_Instance;
	sm_Instance = NULL;
}
