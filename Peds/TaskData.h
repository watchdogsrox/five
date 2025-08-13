// FILE :    TaskData.h
// PURPOSE : Load in task flags 
// CREATED : 10-06-2011

#ifndef TASK_DATA_H
#define TASK_DATA_H

// Rage headers
#include "atl/hashstring.h"
#include "atl/array.h"
#include "parser/macros.h"
#include "system/noncopyable.h"
#include "TaskFlags.h"


// Framework headers

// Game headers

class CTaskDataInfo
{
public:
	// Get the hash
	u32 GetHash() { return m_Name.GetHash(); }

#if !__FINAL
	// Get the name from the metadata manager
	const char* GetName() const { return m_Name.GetCStr(); }
#endif // !__FINAL

	u32 GetTaskWanderConditionalAnimsGroupHash() const
	{	return m_TaskWanderConditionalAnimsGroup.GetHash();	}

	float GetScenarioAttractionDistance() const { return m_ScenarioAttractionDistance; }

	float GetSurfaceSwimmingDepthOffset() const { return m_SurfaceSwimmingDepthOffset; }

	float GetSwimmingWanderPointRange() const { return m_SwimmingWanderPointRange; }

	// Check a flag
	bool GetIsFlagSet(CTaskFlags::Flags f) const { return m_Flags.BitSet().IsSet(f); }

#if __BANK
	const char* GetTaskWanderConditionalAnimsGroup() const
	{	return m_TaskWanderConditionalAnimsGroup.GetCStr();	}
#endif	// __BANK

private:

	atHashWithStringNotFinal	m_Name;

	// PURPOSE:	Name of the conditional animations to use by default for CTaskWander.
	atHashWithStringBank		m_TaskWanderConditionalAnimsGroup;

	float						m_ScenarioAttractionDistance;

	float						m_SurfaceSwimmingDepthOffset;

	float						m_SwimmingWanderPointRange;

	bool						m_PreferCurrentDepthInSwimmingWander;

	// Flags
	CTaskFlags::FlagsBitSet		m_Flags;

	PAR_SIMPLE_PARSABLE;
};

class CTaskDataInfoManager
{
	NON_COPYABLE(CTaskDataInfoManager);
public:
	CTaskDataInfoManager();
	~CTaskDataInfoManager()		{}

	// Initialise
	static void Init(const char* pFileName);

	// AppendExtra
	static void AppendExtra(const char* pFileName);

	// Shutdown
	static void Shutdown();

	// Access the ped task data information
	static const CTaskDataInfo * const GetInfo(const u32 uNameHash, bool bReturnDefault = true)
	{
		if (uNameHash != 0)
		{
			for (s32 i = 0; i < m_TaskDataManagerInstance.m_aTaskData.GetCount(); i++ )
			{
				if (m_TaskDataManagerInstance.m_aTaskData[i]->GetHash() == uNameHash)
				{
					return m_TaskDataManagerInstance.m_aTaskData[i];
				}
			}
		}
		return bReturnDefault ? m_TaskDataManagerInstance.m_DefaultSet : NULL;
	}

	// Conversion functions (currently only used by xml loader)
	static const CTaskDataInfo *const GetInfoFromName(const char * name)	{ return GetInfo( atStringHash(name) ); };
#if __BANK
	static const char * GetInfoName(const CTaskDataInfo *pInfo)				{ return pInfo->GetName(); }
#else
	static const char * GetInfoName(const CTaskDataInfo *)					{ Assert(0); return ""; }
#endif // !__BANK

#if __BANK
	// Add widgets
	static void AddWidgets(bkBank& bank);
#endif // __BANK

private:

	// Delete the data
	static void Reset();

	// Load the data
	static void Load(const char* pFileName);

#if __BANK
	// Save the data
	static void Save();
#endif // __BANK

	atArray<CTaskDataInfo*>	m_aTaskData;
	CTaskDataInfo *			m_DefaultSet;

	static CTaskDataInfoManager m_TaskDataManagerInstance;

	PAR_SIMPLE_PARSABLE;
};

#endif//!TASK_DATA_H