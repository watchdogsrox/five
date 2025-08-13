// FILE :    DefaultTaskData.h
// PURPOSE : Load in default task data set information
// CREATED : 26-09-2011

#ifndef DEFAULT_TASK_DATA_H
#define DEFAULT_TASK_DATA_H

// Rage headers
#include "atl/hashstring.h"
#include "atl/array.h"
#include "parser/macros.h"

// Framework headers

// Game headers

enum DefaultTaskType
{
	WANDER = 0,
	DO_NOTHING,

	MAX_DEFAULT_TASK_TYPES
};

struct sDefaultTaskData
{
	DefaultTaskType m_Type;

	PAR_SIMPLE_PARSABLE;
};

class CDefaultTaskDataSet
{
public:
	CDefaultTaskDataSet() 
		: m_onFoot(NULL)
	{}

	const rage::atHashString& GetName() const	{ return m_Name; }
	const sDefaultTaskData* GetOnFootData() const { return m_onFoot; }

private:
	rage::atHashString	m_Name;
	sDefaultTaskData*	m_onFoot;

	PAR_SIMPLE_PARSABLE;
};

/////////////////////////////////////
// CDefaultTaskDataManager
/////////////////////////////////////

class CDefaultTaskDataManager
{
public:
	// Constructor
	CDefaultTaskDataManager();

	// Init
	static void Init(const char* pFileName);

	// Shutdown
	static void Shutdown();

	// Get the static manager
	static CDefaultTaskDataManager& GetInstance() { return ms_DefaultTaskDataManagerInstance; }

	// Find a data set
	static const CDefaultTaskDataSet* const GetDataSet(const u32 dataSetHash);

	// Conversion functions (currently only used by xml loader)
	static const CDefaultTaskDataSet *const GetInfoFromName(const char * name)	{ return GetDataSet(atStringHash(name) ); };
#if __BANK
	static const char * GetInfoName(const CDefaultTaskDataSet *pInfo)			{ return pInfo->GetName().GetCStr(); }
#else
	static const char * GetInfoName(const CDefaultTaskDataSet *)				{ Assert(0); return ""; }
#endif // !__BANK

private:

	// Our array of default task data sets
	rage::atArray<CDefaultTaskDataSet*>	m_aDefaultTaskData;
	CDefaultTaskDataSet *				m_DefaultSet;

	// Our static manager/instance
	static CDefaultTaskDataManager		ms_DefaultTaskDataManagerInstance;

	PAR_SIMPLE_PARSABLE;
};

#endif //DEFAULT_TASK_DATA_H