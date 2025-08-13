//
// task/Scenario/ScenarioPointGroup.h
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef TASK_SCENARIO_SCENARIOPOINTGROUP_H
#define TASK_SCENARIO_SCENARIOPOINTGROUP_H

#include "atl/hashstring.h"
#include "parser/macros.h"

//-----------------------------------------------------------------------------

class CScenarioPointGroup
{
public:
	CScenarioPointGroup();

	u32 GetNameHash() const
	{	return m_Name.GetHash();	}

	void SetName(const char* name)
	{	m_Name = name;	}

	void SetEnabledByDefault(bool b)
	{	m_EnabledByDefault = b;	}

	bool GetEnabledByDefault() const
	{	return m_EnabledByDefault;	}

#if __BANK
	const char* GetName() const
	{	return m_Name.GetCStr();	}
#endif

	const atHashString GetNameHashString() const
	{	return m_Name;	}

protected:

#if __BANK
	friend class CScenarioEditor;
#endif

	atHashString		m_Name;
	bool				m_EnabledByDefault;

	PAR_SIMPLE_PARSABLE;
};

//-----------------------------------------------------------------------------

#endif	// TASK_SCENARIO_SCENARIOPOINTGROUP_H

// End of file 'task/Scenario/ScenarioPointGroup.h'
