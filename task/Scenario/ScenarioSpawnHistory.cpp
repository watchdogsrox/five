#include "ScenarioSpawnHistory.h"

// Rage includes
#include "grcore/debugdraw.h"

// Framework headers
#include "ai/task/taskchannel.h"
#include "ai/aichannel.h"

// Game headers
#include "Network/Network.h"
#include "Network/Objects/Entities/NetObjVehicle.h"
#include "Peds/ped.h"
#include "scene/world/GameWorld.h"
#include "task/Scenario/Info/ScenarioInfo.h"
#include "task/Scenario/ScenarioDebug.h"
#include "task/Scenario/ScenarioManager.h"
#include "task/Scenario/ScenarioPointManager.h"
#include "task/Scenario/ScenarioPointRegion.h"
#if __ASSERT
#include "Task/Scenario/info/ScenarioInfoManager.h" 
#endif
#include "Vehicles/vehicle.h"

AI_OPTIMISATIONS()

CScenarioSpawnHistory::CScenarioSpawnHistory () 
: m_ClearPedDelay(kDefaultClearPedDelay)
, m_ClearPedDelayLimitedUse(kDefaultClearPedDelaySingleUse)
, m_ClearVehicleDelay(kDefaultClearVehicleDelay)
, m_ClearVehicleDelayLimitedUse(kDefaultClearVehicleDelaySingleUse)
, m_KilledPedDelay(kDefaultKilledPedDelay)
, m_PedLikelySeenThresholdDist(70.0f)
{

}

CScenarioSpawnHistory::~CScenarioSpawnHistory()
{
	// If we have references to any scenario chaining user objects, make sure to get those released.
	const int numPeds = m_SpawnedPeds.GetCount();
	for(int i = 0; i < numPeds; i++)
	{
		if(m_SpawnedPeds[i].m_ChainUseInfo)
		{
			m_SpawnedPeds[i].m_ChainUseInfo->RemoveRefInSpawnHistory();
			m_SpawnedPeds[i].m_ChainUseInfo = NULL;
		}
	}
	const int numVeh = m_SpawnedVehicles.GetCount();
	for(int i = 0; i < numVeh; i++)
	{
		if(m_SpawnedVehicles[i].m_ChainUseInfo)
		{
			m_SpawnedVehicles[i].m_ChainUseInfo->RemoveRefInSpawnHistory();
			m_SpawnedVehicles[i].m_ChainUseInfo = NULL;
		}
	}
}


void CScenarioSpawnHistory::Add(CVehicle& vehicle, CScenarioPoint& scenarioPt, int realScenarioType, CScenarioPointChainUseInfo* chainUseInfo)
{
	// If we are full, try to free some space if we can.
	if(m_SpawnedVehicles.IsFull())
	{
		AttemptToRemoveLeastRelevantVehicleEntry();
	}

#if __ASSERT && DR_ENABLED
	// Dump before the assert, to make sure we get the dump in the logs if bugs get added.
	if(m_SpawnedVehicles.IsFull())
	{
		static bool dumped = false;
		if (!dumped)
		{
			//dump out some debug info ... 
			debugPlayback::TTYTextOutputVisitor output;
			output.AddLine("=============================================================================");
			output.AddLine("Vehicle spawn history list is full here is a list of all the ones we currently have around");
			DebugPrintVehs(output);
			output.AddLine("=============================================================================");
			dumped = true;
		}		
	}
#endif
	if (aiVerifyf(!m_SpawnedVehicles.IsFull(), "History storage full for vehicles. Size %d", kMaxSpawnedVehicles))
	{
		float radius = 50.0f;// 50 meters by default
		const CScenarioInfo* pScenarioInfo = CScenarioManager::GetScenarioInfo(realScenarioType);
		if(pScenarioInfo)
		{
			radius = pScenarioInfo->GetSpawnHistoryRange();
		}

		CSpawnedVehicleInfo& spawnedInfo = m_SpawnedVehicles.Append();
		spawnedInfo.m_Vehicle = &vehicle;
		spawnedInfo.m_ScenarioPt = &scenarioPt;
		spawnedInfo.m_RangeSquared = square(radius);
		spawnedInfo.m_TimeLeftOrDead = 0;
		spawnedInfo.m_IsLimitedUseScenario = FindIsLimitedUseScenario(scenarioPt);

		// Add a reference to the chain user object, if the caller passed one in. This ensures
		// that as long as this vehicle is in the history, even after despawning, we won't let
		// another one spawn anywhere else on the chain (unless it supports >1 users).
		aiAssert(!spawnedInfo.m_ChainUseInfo);
		spawnedInfo.m_ChainUseInfo = chainUseInfo;
		if(chainUseInfo)
		{
			chainUseInfo->AddRefInSpawnHistory();
		}

		scenarioPt.SetRunTimeFlag(CScenarioPointRuntimeFlags::HasHistory, true);
	}
}

void CScenarioSpawnHistory::Add(CPed& ped, CScenarioPoint& scenarioPt, int realScenarioType, CScenarioPointChainUseInfo* chainUseInfo)
{
	if(ped.IsNetworkClone())
	{
		for(int i = 0; i < m_SpawnedPeds.GetCount(); i++)
		{
			if( m_SpawnedPeds[i].m_Ped == &ped &&
				m_SpawnedPeds[i].m_TimeLeftOrDead > 0)
			{
				//B* 537926
				//The same clone ped at an existing history has been found but with the time out about to finish,
				// so don't wait for time out and remove that history here so the ped can get re-added below 
				DeleteSpawnPedIndex(i);
				i--; // step back in case there are multiple of the same ped.
			}
		}
	}

#if __ASSERT && DR_ENABLED
	// Dump before the assert, to make sure we get the dump in the logs if bugs get added.
	if(m_SpawnedPeds.IsFull())
	{
		static bool dumped = false;
		if (!dumped)
		{
			//dump out some debug info ... 
			debugPlayback::TTYTextOutputVisitor output;
			output.AddLine("=============================================================================");
			output.AddLine("Ped spawn history list is full here is a list of all the ones we currently have around");
			DebugPrintPeds(output);
			output.AddLine("=============================================================================");
			dumped = true;
		}		
	}
#endif
	if (aiVerifyf(!m_SpawnedPeds.IsFull(), "History storage full for peds. Size %d", kMaxSpawnedPeds))
	{
		float radius = 50.0f;// 50 meters by default
		const CScenarioInfo* pScenarioInfo = CScenarioManager::GetScenarioInfo(realScenarioType);
		if(pScenarioInfo)
		{
			radius = pScenarioInfo->GetSpawnHistoryRange();
		}

		CSpawnedPedInfo& spawnedInfo = m_SpawnedPeds.Append();
		spawnedInfo.m_Ped = &ped;
		spawnedInfo.m_ScenarioPt = &scenarioPt;
		spawnedInfo.m_RangeSquared = square(radius);
		spawnedInfo.m_TimeLeftOrDead = 0;
		spawnedInfo.m_IsLimitedUseScenario = FindIsLimitedUseScenario(scenarioPt);
		spawnedInfo.m_LikelySeenByPlayer = false;
		spawnedInfo.m_OnChain = scenarioPt.IsChained();
		spawnedInfo.m_WasKilled = false;

		// Add a reference to the chain user object, if the caller passed one in. This ensures
		// that as long as this ped is in the history, even after despawning, we won't let
		// another one spawn anywhere else on the chain (unless it supports >1 users).
		aiAssert(!spawnedInfo.m_ChainUseInfo);
		spawnedInfo.m_ChainUseInfo = chainUseInfo;
		if(chainUseInfo)
		{
			chainUseInfo->AddRefInSpawnHistory();
		}

		scenarioPt.SetRunTimeFlag(CScenarioPointRuntimeFlags::HasHistory, true);
	}
}

CScenarioPointChainUseInfo* CScenarioSpawnHistory::FindChainUserFromHistory(CPed& ped, int regionIndex, int chainIndex) const
{
	// Loop over the ped history.
	const int numPeds = m_SpawnedPeds.GetCount();
	for(int i = 0; i < numPeds; i++)
	{
		const CSpawnedPedInfo& info = m_SpawnedPeds[i];

		// We are only interested in entries matching this ped.
		if(info.m_Ped != &ped)
		{
			continue;
		}

		// We are only interested in entries with a chaining user.
		CScenarioPointChainUseInfo* pUser = info.m_ChainUseInfo;
		if(!pUser)
		{
			continue;
		}

		// If any of these pointers are set, then it's not only used by the history
		// but is still used by a task, and we can't return it to the caller since that
		// other task is still considered the owner. Probably similar thing if it's a dummy, somehow.
		if(pUser->GetPed() || pUser->GetVehicle() || pUser->IsDummy())
		{
			continue;
		}

		// Can't use it if it's not bound to an active scenario point.
		if(!pUser->IsIndexInfoValid())
		{
			continue;
		}

		// Check if it matches the chain we are looking for.
		if(pUser->GetRegionIndex() != regionIndex)
		{
			continue;
		}
		if(pUser->GetChainIndex() != chainIndex)
		{
			continue;
		}

		return pUser;
	}

	return NULL;
}


bool CScenarioSpawnHistory::ReleasePointChainUseInfo(CScenarioPointChainUseInfo& user)
{
	// Check if this user is for one of the peds.
	const int numPeds = m_SpawnedPeds.GetCount();
	for(int i = 0; i < numPeds; i++)
	{
		CSpawnedPedInfo& info = m_SpawnedPeds[i];
		if(info.m_ChainUseInfo == &user)
		{
			// Remove the reference we hold to it, clear our pointer,
			// and let RemoveChainUserIfUnused() completely remove it,
			// if it's not in use by somebody else.
			info.m_ChainUseInfo->RemoveRefInSpawnHistory();
			info.m_ChainUseInfo = NULL;
			return CScenarioChainingGraph::RemoveChainUserIfUnused(user);
		}
	} 

	// Check if this user is for one of the vehicles.
	const int numSpawnedVehicles = m_SpawnedVehicles.GetCount();
	for(int i = 0; i < numSpawnedVehicles; i++)
	{
		CSpawnedVehicleInfo& info = m_SpawnedVehicles[i];
		if(info.m_ChainUseInfo == &user)
		{
			// Remove the reference we hold to it, clear our pointer,
			// and let RemoveChainUserIfUnused() completely remove it,
			// if it's not in use by somebody else.
			info.m_ChainUseInfo->RemoveRefInSpawnHistory();
			info.m_ChainUseInfo = NULL;
			return CScenarioChainingGraph::RemoveChainUserIfUnused(user);
		}
	}

	return false;
}


void CScenarioSpawnHistory::Update()
{
	// Maintain m_SpawnedVehicles[] by removing elements that are not needed anymore.
	// We don't really have to do this on every frame, but should be very
	// cheap as we don't need any data outside of the tiny array.
	int numSpawnedVehicles = m_SpawnedVehicles.GetCount();
	for(int i = numSpawnedVehicles - 1; i >= 0; i--)
	{
#if __ASSERT
		if (m_SpawnedVehicles[i].m_ScenarioPt)
		{
			aiAssert(m_SpawnedVehicles[i].m_ScenarioPt->HasHistory());
		}
#endif

		// If the chaining user was a reference to a chain that no longer exists,
		// we don't need to hold on to the user object.
		if(m_SpawnedVehicles[i].m_ChainUseInfo)
		{
			bool canRemove = false;

			CSpawnedVehicleInfo& info = m_SpawnedVehicles[i];
			if(!info.m_ChainUseInfo->IsIndexInfoValid())
			{
				// This chain user is not tied to any region that's currently streamed in. We used to remove it in this case,
				// but there were issues with airplanes respawning undesirably on chains, so don't do it if these conditions are fulfilled:
				// A. This is a "limited use" chain (in practice, just means that it should only spawn one).
				// B. The vehicle still exists (probably, it would be marked to be removed aggressively in that case).
				// C. We have a memory of the chain in its unstreamed region (if we didn't, we don't have any use for the chaining user anyway).
				if(!(info.m_IsLimitedUseScenario && info.m_Vehicle && info.m_ChainUseInfo->IsInUnstreamedRegion()))
				{
					canRemove = true;
				}
			}

			if(canRemove)
			{
				info.m_ChainUseInfo->RemoveRefInSpawnHistory();
				info.m_ChainUseInfo = NULL;
			}
		}

		int timeUntilNotNeededMs = 0;	// Not used here.
		if (HistoryNeeded(m_SpawnedVehicles[i], timeUntilNotNeededMs))
		{
			continue;
		}

		numSpawnedVehicles--;

		RemoveVehicleEntry(i);
	}

	// Maintain m_SpawnedPeds[] by removing elements that are not needed anymore.
	// We don't really have to do this on every frame, but should be very
	// cheap as we don't need any data outside of the tiny array.
	int numSpawnedPeds = m_SpawnedPeds.GetCount();
	for(int i = numSpawnedPeds - 1; i >= 0; i--)
	{
#if __ASSERT
		if (m_SpawnedPeds[i].m_ScenarioPt)
		{
			aiAssert(m_SpawnedPeds[i].m_ScenarioPt->HasHistory());
		}
#endif

		// If the chaining user was a reference to a chain that no longer exists,
		// we don't need to hold on to the user object.
		if(m_SpawnedPeds[i].m_ChainUseInfo)
		{
			if(!m_SpawnedPeds[i].m_ChainUseInfo->IsIndexInfoValid())
			{
				m_SpawnedPeds[i].m_ChainUseInfo->RemoveRefInSpawnHistory();
				m_SpawnedPeds[i].m_ChainUseInfo = NULL;
			}
		}

		if (HistoryNeeded(m_SpawnedPeds[i]))
		{
			continue;
		}
		
		DeleteSpawnPedIndex(i);
	}
}

void CScenarioSpawnHistory::DeleteSpawnPedIndex(int indexToRemove )
{
#if __ASSERT
	aiAssert( m_SpawnedPeds.GetCount() > 0);
	aiAssert( indexToRemove >= 0);
	aiAssert( indexToRemove < m_SpawnedPeds.GetCount());
#endif
	// This is basically an atFixedArray::DeleteFast(), except that we reset
	// the RegdPed and RegdScenarioPt to NULL to avoid using up extra references nodes past the
	// end of the array.
	int endIndex = m_SpawnedPeds.GetCount()-1;

	m_SpawnedPeds[indexToRemove].m_Ped.Reset(NULL);

	if(m_SpawnedPeds[indexToRemove].m_ChainUseInfo)
	{
		// If the element we are deleting held a reference to a chaining user object,
		// get that released right now.
		m_SpawnedPeds[indexToRemove].m_ChainUseInfo->RemoveRefInSpawnHistory();
		m_SpawnedPeds[indexToRemove].m_ChainUseInfo = NULL;
	}

	if (m_SpawnedPeds[indexToRemove].m_ScenarioPt)
		m_SpawnedPeds[indexToRemove].m_ScenarioPt->SetRunTimeFlag(CScenarioPointRuntimeFlags::HasHistory, false);
	m_SpawnedPeds[indexToRemove].m_ScenarioPt.Reset(NULL);

	if(endIndex != indexToRemove)
	{
		m_SpawnedPeds[indexToRemove] = m_SpawnedPeds[endIndex];

		m_SpawnedPeds[endIndex].m_Ped.Reset(NULL);
		m_SpawnedPeds[endIndex].m_ScenarioPt.Reset(NULL);

		// We intentionally just clear out this handle and don't call RemoveRefInSpawnHistory()
		// here - we just moved this element in the array, but the number of these references to
		// the chaining user object should be the same after that operation.
		m_SpawnedPeds[endIndex].m_ChainUseInfo = NULL;
	}
	m_SpawnedPeds.Pop();
}


void CScenarioSpawnHistory::ClearHistory()
{
	const int numSpawnedVehicles = m_SpawnedVehicles.GetCount();
	for(int i = 0; i < numSpawnedVehicles; i++)
	{
		CSpawnedVehicleInfo& info = m_SpawnedVehicles[i];
		CScenarioPoint* pt = info.m_ScenarioPt;
		if(pt)
		{
			// Think this is the right thing to do, presumably nothing else
			// could clear it if we didn't do it here:
			pt->SetRunTimeFlag(CScenarioPointRuntimeFlags::HasHistory, false);
		}
		info.m_ScenarioPt = NULL;
	}

	const int numSpawnedPeds = m_SpawnedPeds.GetCount();
	for(int i = 0; i < numSpawnedPeds; i++)
	{
		CSpawnedPedInfo& info = m_SpawnedPeds[i];
		CScenarioPoint* pt = info.m_ScenarioPt;
		if(pt)
		{
			// Think this is the right thing to do, presumably nothing else
			// could clear it if we didn't do it here:
			pt->SetRunTimeFlag(CScenarioPointRuntimeFlags::HasHistory, false);
		}
		info.m_ScenarioPt = NULL;
	}
}

#if __BANK

bool CScenarioSpawnHistory::AreInSameChain(const CScenarioPoint * pScenarioPtA, const CScenarioPoint * pScenarioPtB) const
{
	if(!pScenarioPtA || !pScenarioPtB || !pScenarioPtA->IsChained() || !pScenarioPtB->IsChained()) 
	{ 
		return false; 
	}

	for (int i = 0; i < SCENARIOPOINTMGR.GetNumActiveRegions(); ++i)
	{
		CScenarioPointRegion &rRegion = SCENARIOPOINTMGR.GetActiveRegion(i);
		const CScenarioChainingGraph & rGraph = rRegion.GetChainingGraph();
		int iChainNodeIndexA = rGraph.FindChainingNodeIndexForScenarioPoint(*pScenarioPtA);
		int iChainNodeIndexB = rGraph.FindChainingNodeIndexForScenarioPoint(*pScenarioPtB);
		if (iChainNodeIndexA != -1 && iChainNodeIndexB != -1)
		{
			int iChainIndexA = rGraph.GetNodesChainIndex(iChainNodeIndexA);
			int iChainIndexB = rGraph.GetNodesChainIndex(iChainNodeIndexB);
			if (iChainIndexA == iChainIndexB && iChainIndexA != -1 && iChainIndexB != -1)
			{
				return true;
			}
		}
	}

	return false;
}

void CScenarioSpawnHistory::ClearHistoryForPoint(const CScenarioPoint& scenarioPt)
{
	const int numSpawnedVehicles = m_SpawnedVehicles.GetCount();
	for(int i = 0; i < numSpawnedVehicles; i++)
	{
		if (m_SpawnedVehicles[i].m_ScenarioPt == &scenarioPt || AreInSameChain(m_SpawnedVehicles[i].m_ScenarioPt, &scenarioPt))
		{
			// Probably need to clear this manually, as nothing else could do it once we've cleared the pointer - see ClearHistory().
			m_SpawnedVehicles[i].m_ScenarioPt->SetRunTimeFlag(CScenarioPointRuntimeFlags::HasHistory, false);

			m_SpawnedVehicles[i].m_ScenarioPt = NULL; //clear this out by making it no longer tied to the point
		}
	}

	const int numSpawnedPeds = m_SpawnedPeds.GetCount();
	for(int i = 0; i < numSpawnedPeds; i++)
	{
		if (m_SpawnedPeds[i].m_ScenarioPt == &scenarioPt || AreInSameChain(m_SpawnedPeds[i].m_ScenarioPt, &scenarioPt))
		{
			// Probably need to clear this manually, as nothing else could do it once we've cleared the pointer - see ClearHistory().
			m_SpawnedPeds[i].m_ScenarioPt->SetRunTimeFlag(CScenarioPointRuntimeFlags::HasHistory, false);

			m_SpawnedPeds[i].m_ScenarioPt = NULL; //clear this out by making it no longer tied to the point
		}
	}
}

void CScenarioSpawnHistory::DebugRender()
{
	int numSpawnedVehicles = m_SpawnedVehicles.GetCount();
	for(int i = 0; i < numSpawnedVehicles; i++)
	{
		if (!m_SpawnedVehicles[i].m_Vehicle || !m_SpawnedVehicles[i].m_ScenarioPt)
			continue;

		Vec3V vehPos = m_SpawnedVehicles[i].m_Vehicle->GetTransform().GetPosition();
		Vec3V scpPos = m_SpawnedVehicles[i].m_ScenarioPt->GetPosition();
		grcDebugDraw::Line(vehPos, scpPos, Color_orange, 1);
	}

	int numSpawnedPeds = m_SpawnedPeds.GetCount();
	for(int i = 0; i < numSpawnedPeds; i++)
	{
		if (!m_SpawnedPeds[i].m_Ped || !m_SpawnedPeds[i].m_ScenarioPt)
			continue;

		Vec3V pedPos = m_SpawnedPeds[i].m_Ped->GetTransform().GetPosition();
		Vec3V scpPos = m_SpawnedPeds[i].m_ScenarioPt->GetPosition();
		grcDebugDraw::Line(pedPos, scpPos, Color_pink, 1);
	}
}

#if DR_ENABLED
void CScenarioSpawnHistory::DebugRenderToScreen(debugPlayback::OnScreenOutput& output)
{
	if (CScenarioDebug::ms_bRenderSpawnHistoryTextPed)
	{
		DebugPrintPeds(output);
	}
	
	if (CScenarioDebug::ms_bRenderSpawnHistoryTextVeh)
	{
		DebugPrintVehs(output);
	}
}

void CScenarioSpawnHistory::DebugPrintPeds(debugPlayback::TextOutputVisitor& output)
{
	CPed* player = CGameWorld::FindLocalPlayer();

	int numSpawnedPeds = m_SpawnedPeds.GetCount();
	output.AddLine("Peds %d of %d", numSpawnedPeds, m_SpawnedPeds.GetMaxCount());
	output.PushIndent();
	char line[RAGE_MAX_PATH];
	formatf(line, RAGE_MAX_PATH, "%-10s%-32s%-10s%-24s%6s%8s%8s%8s%10s%5s%3s", "Pnt Add", "Pnt Type", "Ped Add", "Ped Type", "Dist", "Time", "Total", "DtoP", "ChUsr Add", "Seen", "Ch");
	output.AddLine(line);
	for(int i = 0; i < numSpawnedPeds; i++)
	{
		const char* pedname = (m_SpawnedPeds[i].m_Ped)? m_SpawnedPeds[i].m_Ped->GetModelName() : "";
		const char* typname = (m_SpawnedPeds[i].m_ScenarioPt)? m_SpawnedPeds[i].m_ScenarioPt->GetScenarioName() : "";

		ScalarV dist(V_ZERO);
		ScalarV distToPlayer(V_ZERO);
		Vec3V pedPos(V_ZERO);
		if (m_SpawnedPeds[i].m_Ped && player)
		{
			pedPos = m_SpawnedPeds[i].m_Ped->GetTransform().GetPosition();
			distToPlayer = Dist(pedPos, player->GetTransform().GetPosition());
		}

		if (m_SpawnedPeds[i].m_Ped && m_SpawnedPeds[i].m_ScenarioPt)
		{
			const Vec3V scpPos = m_SpawnedPeds[i].m_ScenarioPt->GetPosition();
			dist = Dist(pedPos, scpPos);
		}

		u32 sinceLeftOrDead = 0;
		if (m_SpawnedPeds[i].m_TimeLeftOrDead)
		{
			sinceLeftOrDead = CNetwork::GetSyncedTimeInMilliseconds() - m_SpawnedPeds[i].m_TimeLeftOrDead;
		}

		u32 delay = GetDelayForPed(m_SpawnedPeds[i]);
		formatf(line, RAGE_MAX_PATH, "%-10p%-32s%-10p%-24s%6.2f%8d%8d%8.2f%10p%5d%3d",
				m_SpawnedPeds[i].m_ScenarioPt.Get(), typname, m_SpawnedPeds[i].m_Ped.Get(), pedname,
				dist.Getf(), sinceLeftOrDead, delay, distToPlayer.Getf(),
				(void*)(CScenarioPointChainUseInfo*)m_SpawnedPeds[i].m_ChainUseInfo,
				(int)m_SpawnedPeds[i].m_LikelySeenByPlayer,
				(int)m_SpawnedPeds[i].m_OnChain);
		output.AddLine(line);
	}
	output.PopIndent();
}

void CScenarioSpawnHistory::DebugPrintVehs(debugPlayback::TextOutputVisitor& output)
{
	CPed* player = CGameWorld::FindLocalPlayer();

	int numSpawnedVehicles = m_SpawnedVehicles.GetCount();
	output.AddLine("Vehicles %d of %d", numSpawnedVehicles, m_SpawnedVehicles.GetMaxCount());
	output.PushIndent();
	char line[RAGE_MAX_PATH];
	formatf(line, RAGE_MAX_PATH, "%-10s%-32s%-10s%-24s%6s%8s%8s%8s%10s", "Pnt Add", "Pnt Type", "Veh Add", "Veh Type", "Dist", "Time", "Total", "DtoP", "ChUsr Add" );
	output.AddLine(line);
	for(int i = 0; i < numSpawnedVehicles; i++)
	{
		const char* pedname = (m_SpawnedVehicles[i].m_Vehicle)? m_SpawnedVehicles[i].m_Vehicle->GetModelName() : "";
		const char* typname = (m_SpawnedVehicles[i].m_ScenarioPt)? m_SpawnedVehicles[i].m_ScenarioPt->GetScenarioName() : "";

		ScalarV dist(V_ZERO);
		ScalarV distToPlayer(V_ZERO);
		Vec3V pedPos(V_ZERO);
		if (m_SpawnedVehicles[i].m_Vehicle && player)
		{
			pedPos = m_SpawnedVehicles[i].m_Vehicle->GetTransform().GetPosition();
			distToPlayer = Dist(pedPos, player->GetTransform().GetPosition());
		}

		if (m_SpawnedVehicles[i].m_Vehicle && m_SpawnedVehicles[i].m_ScenarioPt)
		{
			const Vec3V scpPos = m_SpawnedVehicles[i].m_ScenarioPt->GetPosition();
			dist = Dist(pedPos, scpPos);
		}

		u32 delay = m_SpawnedVehicles[i].m_IsLimitedUseScenario ? m_ClearVehicleDelayLimitedUse : m_ClearVehicleDelay;
		u32 sinceLeftOrDead = 0;
		if (m_SpawnedVehicles[i].m_TimeLeftOrDead)
		{
			sinceLeftOrDead = CNetwork::GetSyncedTimeInMilliseconds() - m_SpawnedVehicles[i].m_TimeLeftOrDead;
		}
		formatf(line, RAGE_MAX_PATH, "%-10p%-32s%-10p%-24s%6.2f%8d%8d%8.2f%10p", m_SpawnedVehicles[i].m_ScenarioPt.Get(), typname, m_SpawnedVehicles[i].m_Vehicle.Get(), pedname, dist.Getf(), sinceLeftOrDead, delay, distToPlayer.Getf(), (void*)(CScenarioPointChainUseInfo*)m_SpawnedVehicles[i].m_ChainUseInfo);
		output.AddLine(line);
	}
	output.PopIndent();
}

#endif
#endif

bool CScenarioSpawnHistory::FindIsLimitedUseScenario(const CScenarioPoint& pt) const
{
	if(pt.IsChained())
	{
		// Note: this is potentially a bit slow, in that we loop over the active regions
		// and call FindChainingNodeIndexForScenarioPoint() (which is an O(n) operation).
		// I don't think it will be a big problem as long as we only do this once per spawned
		// scenario.
		const int numRegs = SCENARIOPOINTMGR.GetNumActiveRegions();
		for(int i = 0; i < numRegs; i++)
		{
			const CScenarioPointRegion& reg = SCENARIOPOINTMGR.GetActiveRegion(i);
			const CScenarioChainingGraph& graph = reg.GetChainingGraph();
			const int nodeIndex = graph.FindChainingNodeIndexForScenarioPoint(pt);
			if(nodeIndex >= 0)
			{
				const int chainIndex = graph.GetNodesChainIndex(nodeIndex);
				if(chainIndex >= 0)
				{
					if(graph.GetChain(chainIndex).GetMaxUsers() == 1)
					{
						return true;
					}
				}
				return false;
			}
		}
	}
	// I'm a little bit unsure on how we want unchained scenarios to work. For now,
	// they will work like before, i.e. are not treated as limited use scenarios.
	//	else
	//	{
	//		return true;
	//	}
	return false;
}


bool CScenarioSpawnHistory::AttemptToRemoveLeastRelevantVehicleEntry()
{
	const int numSpawnedVehicles = m_SpawnedVehicles.GetCount();

	// First, loop over all the entries and call HistoryNeeded(). In some cases this might
	// find a free entry (though we normally clear those in Update()), but otherwise, it tells
	// us which is the entry that will expire first.
	int bestEntry = -1;
	int bestTimeUntilNotNeededMs = 0x7fffffff;
	for(int i = 0; i < numSpawnedVehicles; i++)
	{
		int timeUntilNotNeededMs = 0;
		if(HistoryNeeded(m_SpawnedVehicles[i], timeUntilNotNeededMs))
		{
			// Check if we got a finite time that's smaller than the best we've found so far.
			if(timeUntilNotNeededMs >= 0 && timeUntilNotNeededMs < bestTimeUntilNotNeededMs)
			{
				bestTimeUntilNotNeededMs = timeUntilNotNeededMs;
				bestEntry = i;
			}
		}
		else
		{
			bestEntry = i;
			break;
		}
	}

	// Remove an entry from the history, if we found one.
	if(bestEntry >= 0)
	{
#if !__NO_OUTPUT
		const CScenarioPoint* pt = m_SpawnedVehicles[bestEntry].m_ScenarioPt;
		if(pt)
		{
			aiDebugf1("AttemptToRemoveLeastRelevantVehicleEntry() removed entry %d, a %s at %.1f, %.1f, %.1f, would expire in %d ms.", bestEntry,
					CScenarioManager::GetScenarioName(pt->GetScenarioTypeVirtualOrReal()),
					pt->GetWorldPosition().GetXf(),
					pt->GetWorldPosition().GetYf(),
					pt->GetWorldPosition().GetZf(),
					bestTimeUntilNotNeededMs
					);
		}
#endif	// !__NO_OUTPUT

		RemoveVehicleEntry(bestEntry);
		return true;
	}

	// If we didn't find anything, we may still have entries that will not expire because
	// the vehicle using them is too close. In that case, we try to find the entry where
	// the vehicle is closest to being outside of its threshold distance.

	ScalarV smallestRemainingDistV(V_FLT_MAX);
	for(int i = 0; i < numSpawnedVehicles; i++)
	{
		const CSpawnedVehicleInfo& history = m_SpawnedVehicles[i];

		// Like in HistoryNeeded(), don't let anything with NoRespawnUntilStreamedOut or
		// m_IsLimitedUseScenario set be removed. Also, make sure that we have a vehicle, since
		// we require it for the code below (though this will probably always be true in practice,
		// since otherwise we should have returned already).
		if(!history.m_ScenarioPt->IsFlagSet(CScenarioPointFlags::NoRespawnUntilStreamedOut)
				&& !history.m_IsLimitedUseScenario
				&& history.m_Vehicle)
		{
			// Compute the difference between the threshold distance and the actual distance.
			const ScalarV thresholdDistV = Sqrt(ScalarV(history.m_RangeSquared));
			const Vec3V pedPosV = history.m_Vehicle->GetTransform().GetPosition();
			const ScalarV distV = Dist(pedPosV, history.m_ScenarioPt->GetPosition());
			const ScalarV remainingDistV = Subtract(thresholdDistV, distV);

			// Check if this is the closest so far to getting outside of the threshold distance.
			if(IsLessThanAll(remainingDistV, smallestRemainingDistV))
			{
				smallestRemainingDistV = remainingDistV;
				bestEntry = i;
			}
		}
	}

	// Remove the least relevant entry, if we found one we can use.
	if(bestEntry >= 0)
	{
#if !__NO_OUTPUT
		const CScenarioPoint* pt = m_SpawnedVehicles[bestEntry].m_ScenarioPt;
		if(pt)
		{
			aiDebugf1("AttemptToRemoveLeastRelevantVehicleEntry() removed entry %d, a %s at %.1f, %.1f, %.1f, would expire after moving %.1f m.", bestEntry,
					CScenarioManager::GetScenarioName(pt->GetScenarioTypeVirtualOrReal()),
					pt->GetWorldPosition().GetXf(),
					pt->GetWorldPosition().GetYf(),
					pt->GetWorldPosition().GetZf(),
					smallestRemainingDistV.Getf()
					);
		}
#endif	// !__NO_OUTPUT

		RemoveVehicleEntry(bestEntry);
		return true;
	}

	return false;
}


void CScenarioSpawnHistory::RemoveVehicleEntry(int entryIndex)
{
	// This is basically an atFixedArray::DeleteFast(), except that we reset
	// the RegdVeh and RegdScenarioPt to NULL to avoid using up extra references nodes past the
	// end of the array.
	m_SpawnedVehicles[entryIndex].m_Vehicle.Reset(NULL);
	if(m_SpawnedVehicles[entryIndex].m_ChainUseInfo)
	{
		// If the element we are deleting held a reference to a chaining user object,
		// get that released right now.
		m_SpawnedVehicles[entryIndex].m_ChainUseInfo->RemoveRefInSpawnHistory();
		m_SpawnedVehicles[entryIndex].m_ChainUseInfo = NULL;
	}
	if (m_SpawnedVehicles[entryIndex].m_ScenarioPt)
	{
		m_SpawnedVehicles[entryIndex].m_ScenarioPt->SetRunTimeFlag(CScenarioPointRuntimeFlags::HasHistory, false);
	}
	m_SpawnedVehicles[entryIndex].m_ScenarioPt.Reset(NULL);

	int lastEntryIndex = m_SpawnedVehicles.GetCount() - 1;
	if(lastEntryIndex != entryIndex)
	{
		m_SpawnedVehicles[entryIndex] = m_SpawnedVehicles[lastEntryIndex];

		m_SpawnedVehicles[lastEntryIndex].m_Vehicle.Reset(NULL);
		m_SpawnedVehicles[lastEntryIndex].m_ScenarioPt.Reset(NULL);

		// We intentionally just clear out this handle and don't call RemoveRefInSpawnHistory()
		// here - we just moved this element in the array, but the number of these references to
		// the chaining user object should be the same after that operation.
		m_SpawnedVehicles[lastEntryIndex].m_ChainUseInfo = NULL;
	}
	m_SpawnedVehicles.Pop();
}


bool CScenarioSpawnHistory::HistoryNeeded(CSpawnedVehicleInfo& history, int& timeUntilNotNeededMsOut) const
{
	// Make sure we initialize this: -1 is to be considered as infinite.
	timeUntilNotNeededMsOut = -1;

	const CScenarioPoint* pPoint = history.m_ScenarioPt;
	if(!pPoint)
	{
		// Normally, we would return false if the scenario point is gone. But, if we still have a chain user
		// and the vehicle still exists and its a limited use chain, the region probably just streamed out,
		// and if it streams back in again, we want to reconnect to it to prevent undesirable respawning.
		if(!history.m_Vehicle || !history.m_IsLimitedUseScenario || !history.m_ChainUseInfo)
		{
			return false;
		}
	}
	else if(pPoint->IsFlagSet(CScenarioPointFlags::NoRespawnUntilStreamedOut))
	{
		return true;
	}

	// First, we have an opportunity to return true if we're still valid
	// and within range, and haven't already started the timer.
	if (history.m_Vehicle && !history.m_TimeLeftOrDead)
	{
		if(history.m_IsLimitedUseScenario)
		{
			return true;
		}
		else if(pPoint)
		{
			// Put the square of the radius in a vector register.
			const ScalarV thresholdDistSqV(history.m_RangeSquared);

			// Compute the squared distance and check if it's outside the range.
			const Vec3V pedPosV = history.m_Vehicle->GetTransform().GetPosition();
			const ScalarV distSqV = DistSquared(pedPosV, pPoint->GetPosition());
			if(!IsGreaterThanOrEqualAll(distSqV, thresholdDistSqV))
			{
				return true;
			}
		}
	}

	// As long as the scenario point exists, start the timer if not started already,
	// then return true until it has expired.
	if (!history.m_TimeLeftOrDead)
	{
		history.m_TimeLeftOrDead = CNetwork::GetSyncedTimeInMilliseconds();
	}

	u32 delay = history.m_IsLimitedUseScenario ? m_ClearVehicleDelayLimitedUse : m_ClearVehicleDelay;
	if (CNetwork::GetSyncedTimeInMilliseconds() <= delay + history.m_TimeLeftOrDead)
	{
		// In this case, it's just a matter of time until this entry will be released:
		// compute how much time that is, and return it to the user.
		timeUntilNotNeededMsOut = delay + history.m_TimeLeftOrDead - CNetwork::GetSyncedTimeInMilliseconds();

		return true;
	}

	return false;
}

bool CScenarioSpawnHistory::HistoryNeeded(CSpawnedPedInfo& history) const
{
	if(!history.m_ScenarioPt)
	{
		return false;
	}

	if(history.m_ScenarioPt->IsFlagSet(CScenarioPointFlags::NoRespawnUntilStreamedOut))
	{
		return true;
	}

	CPed* pPed = history.m_Ped;
	if(!history.m_LikelySeenByPlayer && pPed)
	{
		if(pPed->GetIsVisibleInSomeViewportThisFrame())
		{
			// TODO: May want to think some more about networking here.

			const CPed* pPlayer = CGameWorld::FindLocalPlayer();
			if(pPlayer)
			{
				const ScalarV likelySeenThresholdDistV(m_PedLikelySeenThresholdDist);
				const ScalarV likelySeenThresholdDistSqV = Scale(likelySeenThresholdDistV, likelySeenThresholdDistV);

				const Vec3V pedPosV = pPed->GetMatrixRef().GetCol3();
				const Vec3V playerPosV = pPlayer->GetMatrixRef().GetCol3();
				const ScalarV distSqV = DistSquared(pedPosV, playerPosV);
				if(IsLessThanAll(distSqV, likelySeenThresholdDistSqV))
				{
					history.m_LikelySeenByPlayer = true;
				}
			}
			else
			{
				history.m_LikelySeenByPlayer = true;
			}
		}
	}

	if (!history.m_TimeLeftOrDead)
	{
		// Start the countdown if the ped is dead.
		if (pPed && pPed->IsInjured())
		{
			history.m_WasKilled = true;
		}
		else if(pPed && history.m_IsLimitedUseScenario)
		{
			return true;
		}
		else if(pPed)
		{
			if(history.m_OnChain)
			{
				const CScenarioPoint* pt = CPed::GetScenarioPoint(*pPed);
				if(pt && pt->IsChained())
				{
					return true;
				}
				history.m_OnChain = false;
			}

			// Put the square of the radius in a vector register.
			const ScalarV thresholdDistSqV(history.m_RangeSquared);

			// Compute the squared distance and check if it's outside the range.
			const Vec3V pedPosV = pPed->GetTransform().GetPosition();
			const ScalarV distSqV = DistSquared(pedPosV, history.m_ScenarioPt->GetPosition());
			
			// Start the countdown if the ped left the scenario point.
			if(!IsGreaterThanOrEqualAll(distSqV, thresholdDistSqV))
			{
				return true;
			}
		}
		else
		{
			// ped despawned or otherwise got destroyed?
		}
	}

	if (!history.m_TimeLeftOrDead)
	{
		history.m_TimeLeftOrDead = CNetwork::GetSyncedTimeInMilliseconds();
	}

	const u32 delay = GetDelayForPed(history);
	if (CNetwork::GetSyncedTimeInMilliseconds() > delay + history.m_TimeLeftOrDead)
	{
		return false;
	}

	return true;
}
