#ifndef SCENARIO_ACTION_MANAGER_H
#define SCENARIO_ACTION_MANAGER_H

// Rage headers
#include "Data/Base.h"

// Game headers
#include "Task/Scenario/Info/ScenarioAction.h"


////////////////////////////////////////////////////////////////////////////////
// CScenarioActionManager
// Singleton class responsible for managing temporary responses to events.
////////////////////////////////////////////////////////////////////////////////

class CScenarioActionManager : public datBase
{

public:

	static CScenarioActionManager& GetInstance()
	{	FastAssert(sm_Instance); return *sm_Instance;	}

	static void	Init(unsigned initMode);
	static void	Shutdown(unsigned shutdownMode);

	// Find an appropriate trigger for the task and event combo and then run it.
	// Returns true if an action was executed.
	bool ExecuteTrigger(ScenarioActionContext& rContext);

private:

	explicit CScenarioActionManager();
	~CScenarioActionManager();

private:

	static CScenarioActionManager* sm_Instance;

	CScenarioActionTriggers m_Triggers;

};


#endif	// SCENARIO_ACTION_MANAGER_H
