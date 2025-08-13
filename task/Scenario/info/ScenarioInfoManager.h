#ifndef INC_SCENARIO_INFO_MANAGER_H
#define INC_SCENARIO_INFO_MANAGER_H

// Rage headers
#include "ai/task/taskchannel.h"
#include "atl/singleton.h"

// Game headers
#include "ScenarioInfo.h"
#include "task/Scenario/scdebug.h" // for SCENARIO_DEBUG

////////////////////////////////////////////////////////////////////////////////
// CScenarioTypeGroupEntry
////////////////////////////////////////////////////////////////////////////////

/*
PURPOSE
	A CScenarioTypeGroupEntry object specifies one possible mapping from a virtual
	scenario type to a real scenario type, with an associated probability of being chosen.
*/
class CScenarioTypeGroupEntry
{
public:
	CScenarioTypeGroupEntry()
			: m_ScenarioType(NULL)
			, m_ScenarioTypeIndex(-1)
			, m_ProbabilityWeight(1.0f)
	{}

	// PURPOSE:	Pointer to the info object.
	// NOTE:	This is basically redundant with m_ScenarioTypeIndex, but not sure if
	//			it's possible to make the parser map strings directly to integers in the
	//			same way as it lets us map strings to pointers.
	const CScenarioInfo*	m_ScenarioType;

	// PURPOSE:	The real scenario type, matching m_ScenarioType.
	int						m_ScenarioTypeIndex;

	// PURPOSE:	A probability weight used when making choices between this type and
	//			other possible types. Probably should be kept in the 0..1 range.
	float					m_ProbabilityWeight;

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CScenarioTypeGroup
////////////////////////////////////////////////////////////////////////////////

/*
PURPOSE
	A CScenarioTypeGroup specifies how a specific virtual scenario type can
	be mapped to a real scenario type.
*/
class CScenarioTypeGroup
{
public:
	int GetNumTypes() const { return m_Types.GetCount(); }
	CScenarioTypeGroupEntry& GetType(int index) { return m_Types[index]; }
	const CScenarioTypeGroupEntry& GetType(int index) const { return m_Types[index]; }

	u32 GetHash() const { return m_Name.GetHash(); }

#if !__FINAL
	const char* GetName() const { return m_Name.GetCStr(); }
#endif

protected:
	// PURPOSE:	The name of this type group, or "virtual type".
	atHashWithStringNotFinal			m_Name;

	// PURPOSE:	Array of the real types in this group.
	atArray<CScenarioTypeGroupEntry>	m_Types;

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CScenarioInfoManager
////////////////////////////////////////////////////////////////////////////////

class CScenarioInfoManager : public datBase
{
public:
	// PURPOSE:	Max supported number of types in a CScenarioTypeGroup. Should be safe to
	//			increase as needed, within reason, since it's just used to determine
	//			the size of some arrays on the stack.
	static const int kMaxRealScenarioTypesPerPoint = 8;

	CScenarioInfoManager();
	~CScenarioInfoManager();
	void Init();
	void Shutdown();
	void ClearDLCScenarioInfos(atHashString fileHash = atHashString(atHashString::Null()));
	void HandleDLCData(const char* fileName);
	void Append(const char* fileName);

	void KillExistingScenarios();

	void Remove(const char* fileName);

	void PostLoad(SCENARIO_DEBUG_ONLY(atArray<u32>& indices, bool dlc));

	void CheckUniqueConditionalAnimNames();

	void ResetScenarioTypesEnabledToDefaults();

	bool IsScenarioTypeEnabled(int typeIndex) const
	{	return m_ScenarioTypesEnabled[typeIndex];	}

	void SetScenarioTypeEnabled(int typeIndex, bool b)
	{
		m_ScenarioTypesEnabled[typeIndex] = b; 
		m_Scenarios[typeIndex]->SetIsEnabled(b);
	}

	// Access a scenario info
	const CScenarioInfo* GetScenarioInfo(atHashWithStringNotFinal hash) const
	{
		return const_cast<const CScenarioInfo*>(GetScenarioInfoNonConst(hash));
	}

	// Access a scenario info
	CScenarioInfo* GetScenarioInfoNonConst(atHashWithStringNotFinal hash) const
	{
		int scenarioInfoIndex = GetScenarioIndex(hash);
		if( scenarioInfoIndex != -1) // GetScenarioIndex returns -1 if not found
		{
			if( AssertVerify(scenarioInfoIndex >= 0) && AssertVerify(scenarioInfoIndex < m_Scenarios.GetCount()) )
			{
				return m_Scenarios[scenarioInfoIndex];
			}
		}

		return NULL;
	}

	// Access a scenario info
	const CScenarioInfo* GetScenarioInfoByIndex(s32 iIndex) const
	{
		if(iIndex != Scenario_Invalid)
		{
			if(Verifyf(iIndex >= 0 && iIndex < m_Scenarios.GetCount(), "Index [%d] out of range [0..%d]", iIndex, m_Scenarios.GetCount()))
			{
				return m_Scenarios[iIndex];
			}
		}
		return NULL;
	}

	// Access a scenario info
	CScenarioInfo* GetScenarioInfoByIndex(s32 iIndex)
	{
		if(iIndex != Scenario_Invalid)
		{
			if(Verifyf(iIndex >= 0 && iIndex < m_Scenarios.GetCount(), "Index [%d] out of range [0..%d]", iIndex, m_Scenarios.GetCount()))
			{
				return m_Scenarios[iIndex];
			}
		}
		return NULL;
	}

	bool IsVirtualIndex(int virtualOrRealIndex) const
	{
		return virtualOrRealIndex >= m_Scenarios.GetCount();
	}

	int GetTypeGroupIndex(int virtualIndex) const
	{
		taskAssert(IsVirtualIndex(virtualIndex));
		return virtualIndex - m_Scenarios.GetCount();
	}

	u32 GetHashForScenario(int virtualOrRealTypeIndex)
	{
		if(IsVirtualIndex(virtualOrRealTypeIndex))
		{
			return m_ScenarioTypeGroups[GetTypeGroupIndex(virtualOrRealTypeIndex)]->GetHash();
		}
		else
		{
			return m_Scenarios[virtualOrRealTypeIndex]->GetHash();
		}
	}

#if !__FINAL
	const char* GetNameForScenario(int virtualOrRealIndex)
	{
		if(IsVirtualIndex(virtualOrRealIndex))
		{
			return m_ScenarioTypeGroups[GetTypeGroupIndex(virtualOrRealIndex)]->GetName();
		}
		else
		{
			const CScenarioInfo* pInfo = m_Scenarios[virtualOrRealIndex];
			return pInfo ? pInfo->GetName() : "INVALID SCENARIO";
		}
	}
#endif	// !__FINAL

	// Get the index from the hash
	s32 GetScenarioIndex(atHashWithStringNotFinal hash, bool ASSERT_ONLY(assertIfNotFound) = true, bool includeVirtual = false) const;

	// Get the total numbers of scenarios
	int GetScenarioCount(bool includeVirtual) const
	{
		int cnt = m_Scenarios.GetCount();
		if(includeVirtual)
		{
			cnt += m_ScenarioTypeGroups.GetCount();
		}
		return cnt;
	}

	int GetNumRealScenarioTypes(int scenarioTypeVirtualOrReal) const
	{
		if(IsVirtualIndex(scenarioTypeVirtualOrReal))
		{
			return m_ScenarioTypeGroups[GetTypeGroupIndex(scenarioTypeVirtualOrReal)]->GetNumTypes();
		}
		return 1;
	}

	int GetRealScenarioType(int scenarioTypeVirtualOrReal, int indexWithinTypeGroup) const
	{
		if(IsVirtualIndex(scenarioTypeVirtualOrReal))
		{
			return m_ScenarioTypeGroups[GetTypeGroupIndex(scenarioTypeVirtualOrReal)]->GetType(indexWithinTypeGroup).m_ScenarioTypeIndex;
		}
		taskAssert(indexWithinTypeGroup == 0);
		return scenarioTypeVirtualOrReal;
	}

	float GetRealScenarioTypeProbability(int scenarioTypeVirtualOrReal, int indexWithinTypeGroup) const
	{
		if(IsVirtualIndex(scenarioTypeVirtualOrReal))
		{
			return m_ScenarioTypeGroups[GetTypeGroupIndex(scenarioTypeVirtualOrReal)]->GetType(indexWithinTypeGroup).m_ProbabilityWeight;
		}
		return 1.0f;
	}

	// Reset static variables dependent on fwTimer::GetTimeInMilliseconds()
	void ResetLastSpawnTimes();

	static const CScenarioInfo* FromStringToScenarioInfoCB(const char* str);
	static const char* ToStringFromScenarioInfoCB(const CScenarioInfo* info);

#if SCENARIO_DEBUG
	// Add widgets
	void AddWidgets(bkBank& bank);
	void ClearScenarioTypesWidgets();
	static void ClearPedUsedScenarios();
	void UpdateScenarioTypesWidgets();
#endif // SCENARIO_DEBUG
	
private:

	// Delete the data
	void Reset();

	// Load the data
	void Load();

	// After loading data
	void ParserPostLoad();

#if __BANK
	// Save the data
	void Save();
	void BankLoad();

	// Add widgets
	void AddParserWidgets(bkBank* pBank);
#endif // __BANK

	//
	// Members
	//

	// The scenario info storage
	atArray<CScenarioInfo*> m_Scenarios;

	// PURPOSE:	"Scenario type groups" specifying how to map virtual scenario types to real ones.
	// NOTES:	A virtual scenario type index would first index into m_Scenarios, and then into m_ScenarioTypeGroups.
	//			That is, m_Scenarios.GetCount() would be the index of the first scenario type group.
	atArray<CScenarioTypeGroup*>	m_ScenarioTypeGroups;

	// PURPOSE:	Parallel array to m_Scenarios[], specifying whether each scenario type is currently enabled or not.
	atArray<bool>			m_ScenarioTypesEnabled;

	// PURPOSE: Provide fast lookup from a hash value to its corresponding index in the m_Scenarios array
	atBinaryMap<int, u32> m_ScenariosHashToIndexMap;

#if __BANK
	// PURPOSE:	Pointer to widget group for scenario types enabled widgets.
	bkGroup*				m_ScenarioTypesEnabledWidgetGroup;
#endif // __BANK
	struct sScenarioSourceInfo
	{
		sScenarioSourceInfo(){};
		sScenarioSourceInfo(int _infoIndex,int _infoCount, int _groupIndex,int _groupCount, const char* _source, bool _isDLC=false)
		:infoStartIndex(_infoIndex),infoCount(_infoCount),groupStartIndex(_groupIndex),groupCount(_groupCount),source(_source),isDLC(_isDLC){}
		int infoStartIndex;
		int infoCount;
		int groupStartIndex;
		int groupCount;
		atFinalHashString source;
		bool isDLC;
	};
	atArray<sScenarioSourceInfo> m_dScenarioSourceInfo;
	// PURPOSE:	Static pointer used by parser callbacks.
	static CScenarioInfoManager*	sm_ManagerBeingLoaded;

	PAR_SIMPLE_PARSABLE;
};

typedef atSingleton<CScenarioInfoManager> CScenarioInfoManagerSingleton;
#define SCENARIOINFOMGR CScenarioInfoManagerSingleton::InstanceRef()
#define INIT_SCENARIOINFOMGR											\
	do {																\
		if(!CScenarioInfoManagerSingleton::IsInstantiated()) {			\
			CScenarioInfoManagerSingleton::Instantiate();				\
			SCENARIOINFOMGR.Init();										\
		}																\
	} while(0)															\
	//END
#define SHUTDOWN_SCENARIOINFOMGR				\
	SCENARIOINFOMGR.Shutdown();					\
	CScenarioInfoManagerSingleton::Destroy()	\

#endif // INC_SCENARIO_INFO_MANAGER_H
