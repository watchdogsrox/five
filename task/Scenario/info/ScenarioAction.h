#ifndef SCENARIO_ACTION_H
#define SCENARIO_ACTION_H

// Rage headers
#include "Data/Base.h"
#include "atl/array.h"
#include "parser/macros.h"

// Game headers
#include "Task/Scenario/Info/ScenarioActionCondition.h"

////////////////////////////////////////////////////////////////////////////////
// CScenarioAction
// Perform some effect on a currently running task.
////////////////////////////////////////////////////////////////////////////////

class CScenarioAction : public datBase
{
	DECLARE_RTTI_BASE_CLASS(CScenarioAction);

public:
	// Returns true if the action successfully executed.
	virtual bool Execute(ScenarioActionContext& rContext) const = 0;

private:

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CScenarioActionTrigger
// Contains a condition, a probability, and an action.
////////////////////////////////////////////////////////////////////////////////

class CScenarioActionTrigger
{
public:

	~CScenarioActionTrigger();

	bool Check(ScenarioActionContext& rContext)	const;
	// Returns true if the trigger successfully fired its action.
	bool Execute(ScenarioActionContext& rContext)		const	{ Assert(m_Action); return m_Action->Execute(rContext);}
	float GetChance()									const	{ return m_Probability; }

private:

	atArray<CScenarioActionCondition*>					m_Conditions;
	CScenarioAction*									m_Action;
	float												m_Probability;

	PAR_SIMPLE_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CScenarioActionTriggers
// Array of triggers loaded by metadata.
////////////////////////////////////////////////////////////////////////////////

class CScenarioActionTriggers
{

public:

	CScenarioActionTriggers();
	~CScenarioActionTriggers();

public:

	CScenarioActionTrigger*	Get(int iIndex) const		{ return m_Triggers[iIndex]; }

	int Size() const									{ return m_Triggers.GetCount(); }

private:

	atArray<CScenarioActionTrigger*>	m_Triggers;

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CScenarioActionFlee
// Trigger a Scenario Flee Sequence
////////////////////////////////////////////////////////////////////////////////

class CScenarioActionFlee : public CScenarioAction
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioActionFlee, CScenarioAction);

public:

	virtual bool Execute(ScenarioActionContext& rContext) const;

private:

	PAR_SIMPLE_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CScenarioActionHeadTrack
// Trigger a head track for the ped using the scenario.
////////////////////////////////////////////////////////////////////////////////

class CScenarioActionHeadTrack : public CScenarioAction
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioActionHeadTrack, CScenarioAction);

public:

	virtual bool Execute(ScenarioActionContext& rContext) const;

private:

	PAR_SIMPLE_PARSABLE;

};

//////////////////////////////////////////////////////////////////////////////////////////////
// CScenarioActionShockReaction
// The ped plays a brief shocking event animation and then returns to normal use of the point.
//////////////////////////////////////////////////////////////////////////////////////////////

class CScenarioActionShockReaction : public CScenarioAction
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioActionShockReaction, CScenarioAction);

public:

	virtual bool Execute(ScenarioActionContext& rContext) const;

private:

	PAR_SIMPLE_PARSABLE;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// CScenarioActionThreatResponseExit
// The ped determines if it should go into combat or flee and forces the scenario to exit accordingly.
//////////////////////////////////////////////////////////////////////////////////////////////////////

class CScenarioActionThreatResponseExit : public CScenarioAction
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioActionThreatResponseExit, CScenarioAction);

public:

	virtual bool Execute(ScenarioActionContext& rContext) const;

private:

	PAR_SIMPLE_PARSABLE;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// CScenarioActionThreatResponseExit
// The ped determines if it should go into combat and exit accordingly.
//////////////////////////////////////////////////////////////////////////////////////////////////////

class CScenarioActionCombatExit : public CScenarioAction
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioActionCombatExit, CScenarioAction);

public:

	virtual bool Execute(ScenarioActionContext& rContext) const;

private:

	PAR_SIMPLE_PARSABLE;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// CScenarioActionCowardExitThenRespondToEvent
// Play a coward exit then run whatever the normal ped response to the event is.
//////////////////////////////////////////////////////////////////////////////////////////////////////

class CScenarioActionCowardExitThenRespondToEvent : public CScenarioAction
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioActionCowardExitThenRespondToEvent, CScenarioAction);

public:

	virtual bool Execute(ScenarioActionContext& rContext) const;

private:

	PAR_SIMPLE_PARSABLE;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// CScenarioActionImmediateExit
// Force ShouldAbort() to return true so the ped will immediately start blending out of the scenario.
//////////////////////////////////////////////////////////////////////////////////////////////////////

class CScenarioActionImmediateExit : public CScenarioAction
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioActionImmediateExit, CScenarioAction);

public:

	virtual bool Execute(ScenarioActionContext& rContext) const;

private:

	PAR_SIMPLE_PARSABLE;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// CScenarioActionNormalExitThenRespondToEvent
// The ped plays their normal exit and then responds to the event.
//////////////////////////////////////////////////////////////////////////////////////////////////////

class CScenarioActionNormalExitThenRespondToEvent : public CScenarioAction
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioActionNormalExitThenRespondToEvent, CScenarioAction);

public:

	virtual bool Execute(ScenarioActionContext& rContext) const;

private:

	PAR_SIMPLE_PARSABLE;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// CScenarioActionNormalExit
// The ped plays their normal exit.
//////////////////////////////////////////////////////////////////////////////////////////////////////

class CScenarioActionNormalExit : public CScenarioAction
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioActionNormalExit, CScenarioAction);

public:

	virtual bool Execute(ScenarioActionContext& rContext) const;

private:

	PAR_SIMPLE_PARSABLE;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// CScenarioActionScriptExit
// The ped plays an exit based on what ped config flags have been set on it.
//////////////////////////////////////////////////////////////////////////////////////////////////////

class CScenarioActionScriptExit : public CScenarioAction
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioActionScriptExit, CScenarioAction);

public:

	virtual bool Execute(ScenarioActionContext& rContext) const;

private:

	PAR_SIMPLE_PARSABLE;
};


#endif // SCENARIO_ACTION_H
