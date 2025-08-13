#include "Task/Scenario/Info/ScenarioActionManager.h"

// Rage headers
#include "parser/macros.h"
#include "parser/manager.h"

// Parser files
#include "ScenarioActionCondition_parser.h"
#include "ScenarioAction_parser.h"

AI_SCENARIO_OPTIMISATIONS()


////////////////////////////////////////////////////////////////////////////////
// CScenarioActionManager
////////////////////////////////////////////////////////////////////////////////

CScenarioActionManager *CScenarioActionManager::sm_Instance = NULL;

CScenarioActionManager::CScenarioActionManager()
: m_Triggers()
{
	//Parse the triggers.
	PARSER.LoadObject("common:/data/ai/scenariotriggers", "meta", m_Triggers);
}

CScenarioActionManager::~CScenarioActionManager()
{

}

void CScenarioActionManager::Init(unsigned UNUSED_PARAM(initMode))
{
	Assert(!sm_Instance);
	sm_Instance = rage_new CScenarioActionManager();
}

void CScenarioActionManager::Shutdown(unsigned UNUSED_PARAM(shutdownMode))
{
	Assert(sm_Instance);
	delete sm_Instance;
	sm_Instance = NULL;
}

bool CScenarioActionManager::ExecuteTrigger(ScenarioActionContext& rContext)
{
	// Store the indices of all successfully cleared triggers.
	atArray<int> aWinners;

	float fTotalChance = 0.0f;

	for(int i=0; i < m_Triggers.Size(); i++)
	{
		CScenarioActionTrigger* pTrigger = m_Triggers.Get(i);
		if (pTrigger->Check(rContext))
		{
			aWinners.PushAndGrow(i);
			fTotalChance += pTrigger->GetChance();
		}
	}

	float fSelection = fwRandom::GetRandomNumberInRange(0.0f, fTotalChance);

	// Iterate over the winning triggers and decide which one to execute based on their probabilities.
	for(int i=0; i < aWinners.GetCount(); i++)
	{
		CScenarioActionTrigger* pTrigger = m_Triggers.Get(aWinners[i]);
		float fChance = pTrigger->GetChance();
		if (fSelection <= fChance)
		{
			return pTrigger->Execute(rContext);
		}
		else
		{
			fSelection -= fChance;
		}
	}

	return false;
}