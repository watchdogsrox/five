#ifndef SCENARIO_ACTION_CONDITION_H
#define SCENARIO_ACTION_CONDITION_H

#include "Data/Base.h"
#include "atl/array.h"
#include "parser/macros.h"

#include "Event/Event.h"
#include "Event/EventEditable.h"
#include "Event/System/EventData.h"
#include "Task/System/Task.h"
#include "Task/System/TaskTypes.h"


////////////////////////////////////////////////////////////////////////////////
// ScenarioActionContext
// A set of data from which to make a decision on which trigger to execute.
////////////////////////////////////////////////////////////////////////////////

struct ScenarioActionContext
{
	explicit ScenarioActionContext(CTask *pTask, const CEvent& rEvent, s16 iOldEvent, CTaskTypes::eTaskType iResponseTaskType)
		: m_Task(pTask)
		, m_Event(rEvent)
		, m_EventCurrentlyBeingHandledbyTask(iOldEvent)
		, m_ResponseTaskType(iResponseTaskType)
	{ Assert(pTask); }

	CTask*						m_Task;
	CTaskTypes::eTaskType		m_ResponseTaskType;
	const CEvent&				m_Event;
	s16							m_EventCurrentlyBeingHandledbyTask;

};

///////////////////////////////////////////////////////////////////////////////
// CScenarioActionCondition
// Some condition or group of conditions that must be cleared to run a trigger.
///////////////////////////////////////////////////////////////////////////////

class CScenarioActionCondition : public datBase
{
	DECLARE_RTTI_BASE_CLASS(CScenarioActionCondition);

public:

	virtual bool Check(ScenarioActionContext& rContext) const = 0;

private:

	PAR_PARSABLE;
};

///////////////////////////////////////////////////////////////////////////////
// CScenarioActionConditionNot
// Negate the contained condition.
///////////////////////////////////////////////////////////////////////////////

class CScenarioActionConditionNot : public CScenarioActionCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioActionConditionNot, CScenarioActionCondition);

public:

	CScenarioActionConditionNot() : m_Condition(NULL) {}

	virtual ~CScenarioActionConditionNot() { delete m_Condition; }

	virtual bool Check(ScenarioActionContext& rContext) const;

private:

	CScenarioActionCondition*		m_Condition;
	PAR_PARSABLE;
};

///////////////////////////////////////////////////////////////////////////////
// CScenarioActionConditionEvent
// Return true if the event type matches what is specified in metadata.
///////////////////////////////////////////////////////////////////////////////

class CScenarioActionConditionEvent : public CScenarioActionCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioActionConditionEvent, CScenarioActionCondition);

public:

	virtual bool Check(ScenarioActionContext& rContext) const;

private: 


	eEventType				m_EventType;
	PAR_PARSABLE;
};

///////////////////////////////////////////////////////////////////////////////
// CScenarioActionConditionResponseTask
// Return true if the response task matches what is specified in metadata.
///////////////////////////////////////////////////////////////////////////////

class CScenarioActionConditionResponseTask : public CScenarioActionCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioActionConditionResponseTask, CScenarioActionCondition);

public:

	virtual bool Check(ScenarioActionContext& rContext) const;

private:


	rage::atHashString			m_TaskType;
	PAR_PARSABLE;
};

///////////////////////////////////////////////////////////////////////////////
// CScenarioActionConditionForceAction
// Return true if the force type matches what is specified in metadata.
///////////////////////////////////////////////////////////////////////////////

enum eScenarioActionType
{
	eScenarioActionFlee,
	eScenarioActionHeadTrack,
	eScenarioActionTempCowardReaction,
	eScenarioActionCowardExit,
	eScenarioActionShockReaction,
	eScenarioActionThreatResponseExit,
	eScenarioActionCombatExit,
};


class CScenarioActionConditionForceAction : public CScenarioActionCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CScenarioActionConditionForceAction, CScenarioActionCondition);

public:

	virtual bool Check(ScenarioActionContext& rContext) const;

private:


	int	m_ScenarioActionType;
	PAR_PARSABLE;
};




///////////////////////////////////////////////////////////////////////////////////
// CScenarioActionConditionInRange
// Return true if the event occurred within some radius to the ped running the task.
///////////////////////////////////////////////////////////////////////////////////

class CScenarioActionConditionInRange : public CScenarioActionCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioActionConditionInRange, CScenarioActionCondition);

public:

	virtual bool Check(ScenarioActionContext& rContext) const;

private:

	float					m_Range;
	PAR_PARSABLE;
};

///////////////////////////////////////////////////////////////////////////////////
// CScenarioActionConditionCloseOrRecent
// Return true if the event happened within some radius or was recently observed.
///////////////////////////////////////////////////////////////////////////////////

class CScenarioActionConditionCloseOrRecent : public CScenarioActionCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioActionConditionCloseOrRecent, CScenarioActionCondition);

public:

	virtual bool Check(ScenarioActionContext& rContext) const;

private:

	float					m_Range;
	PAR_PARSABLE;
};


////////////////////////////////////////////////////////////////////////////////////
// CScenarioActionConditionHasCowardReact
// Returns true if the scenario task has a valid coward reaction to use.
////////////////////////////////////////////////////////////////////////////////////
class CScenarioActionConditionHasCowardReact : public CScenarioActionCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioActionConditionHasCowardReact, CScenarioActionCondition);

public:

	virtual bool Check(ScenarioActionContext& rContext) const;

	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////////
// CScenarioActionConditionHasShockingReact
// Return true if the scenario task has a shocking reaction animation to use.
// This is hopefully a temporary condition that will only exist until 
// there are animations in place for all scenarios.
////////////////////////////////////////////////////////////////////////////////////

class CScenarioActionConditionHasShockingReact : public CScenarioActionCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioActionConditionHasShockingReact, CScenarioActionCondition);

public:

	virtual bool Check(ScenarioActionContext& rContext) const;

	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////////
// CScenarioActionConditionIsAGangPed
// Return true if the ped using the scenario is a gang ped.
////////////////////////////////////////////////////////////////////////////////////

class CScenarioActionConditionIsAGangPed : public CScenarioActionCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioActionConditionIsAGangPed, CScenarioActionCondition);

public:

	virtual bool Check(ScenarioActionContext& rContext) const;

	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////////
// CScenarioActionConditionIsACopPed
// Return true if the ped using the scenario is a cop (or swat).
////////////////////////////////////////////////////////////////////////////////////

class CScenarioActionConditionIsACopPed : public CScenarioActionCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioActionConditionIsACopPed, CScenarioActionCondition);

public:

	virtual bool Check(ScenarioActionContext& rContext) const;

	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////////
// CScenarioActionConditionIsASecurityPed
// Return true if the ped using the scenario is a security guard.
////////////////////////////////////////////////////////////////////////////////////

class CScenarioActionConditionIsASecurityPed : public CScenarioActionCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioActionConditionIsASecurityPed, CScenarioActionCondition);

public:

	virtual bool Check(ScenarioActionContext& rContext) const;

	PAR_PARSABLE;
};


/////////////////////////////////////////////////////////////////////////////////////
// CScenarioActionConditionResponseType
// Return true if the response type for the event matches the stored hash.
/////////////////////////////////////////////////////////////////////////////////////

class CScenarioActionConditionResponseType : public CScenarioActionCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioActionConditionResponseType, CScenarioActionCondition);

public:

	virtual bool Check(ScenarioActionContext& rContext) const;

private:

	atHashString	m_ResponseHash;

	PAR_PARSABLE;
};

/////////////////////////////////////////////////////////////////////////////////////
// CScenarioActionConditionCurrentlyRespondingToOtherEvent
// Return true if the scenario ped is in the middle of some other response.
/////////////////////////////////////////////////////////////////////////////////////

class CScenarioActionConditionCurrentlyRespondingToOtherEvent : public CScenarioActionCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioActionConditionCurrentlyRespondingToOtherEvent, CScenarioActionCondition);

public:

	virtual bool Check(ScenarioActionContext& rContext) const;

	PAR_PARSABLE;
};

/////////////////////////////////////////////////////////////////////////////////////
// CScenarioActionConditionCanDoQuickBlendout
// Return true if the scenario info is configured to allow quick blendouts.
/////////////////////////////////////////////////////////////////////////////////////

class CScenarioActionConditionCanDoQuickBlendout : public CScenarioActionCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioActionConditionCanDoQuickBlendout, CScenarioActionCondition);

public:

	virtual bool Check(ScenarioActionContext& rContext) const;

	PAR_PARSABLE;
};

#endif // SCENARIO_ACTION_CONDITION