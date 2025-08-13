#ifndef INC_SCENARIO_SPAWN_HISTORY_H
#define INC_SCENARIO_SPAWN_HISTORY_H
// Rage headers
#include "atl/array.h"

// Framework headers

// Game headers
#include "scene/RegdRefTypes.h"
#include "task/Scenario/ScenarioChaining.h"
#include "task/Scenario/ScenarioPoint.h"

// Forwards
class CVehicle;
class CPed;

class CScenarioSpawnHistory
{
public:
	CScenarioSpawnHistory();
	~CScenarioSpawnHistory();

	void Add(CVehicle& vehicle, CScenarioPoint& scenarioPt, int realScenarioType, CScenarioPointChainUseInfo* chainUseInfo);
	void Add(CPed& ped, CScenarioPoint& scenarioPt, int realScenarioType, CScenarioPointChainUseInfo* chainUseInfo);

	bool VehicleHistoryIsFull() const { return m_SpawnedVehicles.IsFull(); }
	bool PedHistoryIsFull() const { return m_SpawnedPeds.IsFull(); }

	// PURPOSE:	Check if there is an entry in the history, with a chaining user, for a specific
	//			ped using a specific chain, and this entry is not owned by anything else.
	CScenarioPointChainUseInfo* FindChainUserFromHistory(CPed& ped, int regionIndex, int chainIndex) const;

	// PURPOSE:	Check if the given scenario user is present in the history, and if so,
	//			try to release it.
	bool ReleasePointChainUseInfo(CScenarioPointChainUseInfo& user);

	void Update();

	void ClearHistory();
#if __BANK
	bool AreInSameChain(const CScenarioPoint * pScenarioPtA, const CScenarioPoint * pScenarioPtB) const;
	void ClearHistoryForPoint(const CScenarioPoint& scenarioPt);
	void DebugRender();
#if DR_ENABLED
	void DebugRenderToScreen(debugPlayback::OnScreenOutput& output);
#endif
#endif

private:

#if __BANK
#if DR_ENABLED
	void DebugPrintPeds(debugPlayback::TextOutputVisitor& output);
	void DebugPrintVehs(debugPlayback::TextOutputVisitor& output);
#endif
#endif

	void DeleteSpawnPedIndex(int indexToRemove );

	// PURPOSE:	Check if a given scenario should be considered "limited use" and have increased history delay/range.
	bool FindIsLimitedUseScenario(const CScenarioPoint& pt) const;

	// PURPOSE:	When m_SpawnedVehicles[] is full, try to find the entry that seems least relevant, and clear it.
	//			Some entries may not be allowed to be removed, so there is no guarantee that this will succeed
	//			(though it's likely in practice that it will).
	// RETURNS:	True if something was freed up.
	bool AttemptToRemoveLeastRelevantVehicleEntry();

	// PURPOSE:	Remove a given entry from m_SpawnedVehicles.
	// PARAMS:	entryIndex - Index of the entry to remove.
	void RemoveVehicleEntry(int entryIndex);

	u32 m_ClearPedDelay; //(default 30000) Time post a ped moving out of range that the spawn history is no longer valid so a new spawn can happen.
	u32 m_ClearPedDelayLimitedUse;
	u32 m_ClearVehicleDelay; //(default 40000) Time post a vehicle moving out of range, or despawning, that the spawn history is no longer valid so a new spawn can happen.
	u32 m_ClearVehicleDelayLimitedUse; //(default 120000) Used instead of m_ClearVehicleDelay for certain scenarios that shouldn't respawn frequently.
	u32 m_KilledPedDelay; //(default 120000) Time post a ped being killed that the spawn history is no longer valid so a new spawn can happen.

	float m_PedLikelySeenThresholdDist;

	//////////////////////////////////////////////////////////////////////////
	// Vehicles
	struct CSpawnedVehicleInfo
	{
		RegdVeh			m_Vehicle;
		RegdScenarioPnt	m_ScenarioPt;
		float			m_RangeSquared;
		u32				m_TimeLeftOrDead;
		bool			m_IsLimitedUseScenario;
		CScenarioPointChainUseInfoPtr	m_ChainUseInfo;
	};
	bool HistoryNeeded(CSpawnedVehicleInfo& history, int& timeUntilNotNeededMsOut) const;

	//vehicles can be doubled up in spawn history since vehicle attractors are added
	// to the spawn history ... so the case would be a vehicle is spawned at a point and
	// then is attracted via an attractor so we need to allow a bit more room to store 
	// vehicles
	// - The comment above is a bit misleading and doesn't really tell the whole story.
	//   The spawn history is a memory of vehicles that have spawned in the past, so
	//   there isn't really an upper limit based on the number of vehicles we can have
	//   at any given time, it's more a question of how many new scenario vehicles we spawn
	//   relative to how long time we keep the memory of them.
	static const int kMaxSpawnedVehicles = (int)(SCENARIO_MAX_SPAWNED_VEHICLES * 1.5f);
	atFixedArray<CSpawnedVehicleInfo, kMaxSpawnedVehicles> m_SpawnedVehicles;

	//////////////////////////////////////////////////////////////////////////
	// Peds
	struct CSpawnedPedInfo
	{
		RegdPed			m_Ped;
		RegdScenarioPnt	m_ScenarioPt;
		CScenarioPointChainUseInfoPtr	m_ChainUseInfo;
		float			m_RangeSquared;
		u32				m_TimeLeftOrDead;
		bool			m_IsLimitedUseScenario;
		bool			m_LikelySeenByPlayer;
		bool			m_OnChain;
		bool			m_WasKilled;
	};
	bool HistoryNeeded(CSpawnedPedInfo& history) const;
	static const int kMaxSpawnedPeds = (__XENON || __PS3) ? 144 : 288;
	static const u32 kDefaultClearPedDelay = 30000;
	static const u32 kDefaultClearPedDelaySingleUse = 60000;
	static const u32 kDefaultClearVehicleDelay = 40000;
	static const u32 kDefaultClearVehicleDelaySingleUse = 120000;
	static const u32 kDefaultKilledPedDelay = 120000;
	atFixedArray<CSpawnedPedInfo, kMaxSpawnedPeds> m_SpawnedPeds;

	u32 GetDelayForPed(const CSpawnedPedInfo& info) const
	{
		u32 delay;
		if(info.m_WasKilled)
		{
			delay = m_KilledPedDelay;
		}
		if(info.m_LikelySeenByPlayer)
		{
			if(info.m_IsLimitedUseScenario)
			{
				delay = m_ClearPedDelayLimitedUse;
			}
			else
			{
				delay = m_ClearPedDelay;
			}
		}
		else
		{
			// No delay for now if we don't think the player was likely to have ever
			// seen this ped.
			delay = 0;
		}
		return delay;
	}
};

#endif //INC_SCENARIO_SPAWN_HISTORY_H