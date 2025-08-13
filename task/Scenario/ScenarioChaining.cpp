//
// task/Scenario/ScenarioChaining.cpp
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "task/Scenario/ScenarioChaining.h"
#include "task/Scenario/Info/ScenarioInfoManager.h"
#include "task/Scenario/ScenarioManager.h"
#include "task/Scenario/ScenarioPoint.h"
#include "task/Scenario/ScenarioPointManager.h"
#include "task/Scenario/ScenarioPointRegion.h"
#include "modelinfo/ModelInfo.h"
#include "modelinfo/ModelInfo_Factories.h"
#include "scene/EntityIterator.h"

AI_OPTIMISATIONS()

// Make sure the nav modes and speeds fit in the expected u8.
CompileTimeAssert(CScenarioChainingEdge::kNumNavModes <= 0xff);
CompileTimeAssert(CScenarioChainingEdge::kNumNavSpeeds <= 0xff);

//-----------------------------------------------------------------------------
float CScenarioChainingGraph::sm_PointSearchRadius = 0.1f; //Magic
bool CScenarioChainingGraph::sm_LoadedNewScenarioRegion = false;

CScenarioChainingNode::CScenarioChainingNode()
		: m_Position(V_ZERO)
		, m_HasIncomingEdges(false)
		, m_HasOutgoingEdges(false)
{
}

#if __BANK

void CScenarioChainingNode::UpdateFromPoint(const CScenarioPoint& point)
{
	m_ScenarioType = SCENARIOINFOMGR.GetHashForScenario(point.GetScenarioTypeVirtualOrReal());
	m_Position = point.GetPosition();

	if(point.GetEntity())
	{
		m_AttachedToEntityType = point.GetEntity()->GetModelName();
	}
	else
	{
		m_AttachedToEntityType.Clear();
	}
}

#endif	// __BANK

//-----------------------------------------------------------------------------

#if __BANK

const char* CScenarioChainingEdge::sm_ActionNames[] =
{
	"Move",
	"Move Into Vehicle As Passenger",
	"Move Follow Master"
};
CompileTimeAssert(NELEM(CScenarioChainingEdge::sm_ActionNames) == CScenarioChainingEdge::kNumActions);

#endif	// __BANK

CScenarioChainingEdge::CScenarioChainingEdge()
		: m_NodeIndexFrom(0xffff)
		, m_NodeIndexTo(0xffff)
		, m_Action(kActionDefault)
		, m_NavMode(kNavModeDefault)
		, m_NavSpeed(kNavSpeedDefault)
{
}

#if __BANK

void CScenarioChainingEdge::CopyEdgeDataFrom(const CScenarioChainingEdge& src)
{
	// Here, we should probably basically copy everything except
	// m_NodeIndexFrom/m_NodeIndexTo.

	m_Action = src.m_Action;
	m_NavMode = src.m_NavMode;
	m_NavSpeed = src.m_NavSpeed;
}

//This used to copy all the data from one edge to another ... 
// Used here for undo functionality 
void CScenarioChainingEdge::CopyFull(const CScenarioChainingEdge& src)
{
	m_NodeIndexFrom = src.m_NodeIndexFrom;
	m_NodeIndexTo = src.m_NodeIndexTo;
	CopyEdgeDataFrom(src);
}

#endif	// __BANK

//-----------------------------------------------------------------------------

void CScenarioChain::AddUser(CScenarioPointChainUseInfo* user, bool allowOverflow)
{
	aiAssertf(m_Users.Find(user) == -1, "Already added this user for chain %d in region %d", user->m_ChainId, user->m_RegionId);

	if(m_Users.GetCount()+1 > m_MaxUsers && !allowOverflow)
	{
		if(NetworkInterface::IsGameInProgress())
		{
			// In networking games, we probably can't expect this to never happen, so we don't fail an assert there.
			// For example, if we just let the clones register as chain users, two machines could potentially
			// simultaneously create a local ped in different ends of the chain, and if they migrate to the same
			// owner there could be more locally controlled peds on the chain that what's really allowed. Another
			// example that could probably happen is that one machine creates a ped on the chain but another player
			// is far enough away that it doesn't get cloned, and then there is nothing preventing that other
			// player from also creating a ped on the chain. We have to allow for that unless we want to implement
			// a more robust global synchronization mechanism of chains.
#if !__NO_OUTPUT
#if __ASSERT || __BANK
			const CPed* pPed = user->m_Ped;
			const CVehicle* pVehicle = user->m_Vehicle;
			const char* pedName = pPed ? pPed->GetDebugName() : "";
			const char* vehName = pVehicle ? pVehicle->GetDebugName() : "";
#else
			const char* pedName = "";
			const char* vehName = "";
#endif
			aiDebugf1("Adding a user of a chain beyond the allowed max of %d chain %d in region %d (ped: %s veh: %s).",
					m_MaxUsers, user->m_ChainId, user->m_RegionId, pedName, vehName);
#endif	// !__NO_OUTPUT
		}
		else
		{
#if __ASSERT
			// Make sure we only report it once, to avoid excessive TTY spew if the assert is ignored
			// and reoccurs.
			static bool s_Reported = false;
			if(!s_Reported)
			{
				aiDisplayf("Existing users:");

				char buff[256];
				const int ucount = m_Users.GetCount();
				for(int i = 0; i < ucount + 1; i++)
				{
					if(i == 0 || i == ucount)
					{
						if(i == ucount)
						{
							aiDisplayf(" ");
							aiDisplayf("To be added:");
						}
						aiDisplayf("   Chain    Node     Ped                             Vehicle               D R  X      Y      Z    NetOw");
					}

					const CScenarioPointChainUseInfo* u = i < ucount ? m_Users[i] : user;

					const CPed* pPed = u->m_Ped;
					const CVehicle* pVehicle = u->m_Vehicle;
					const char* pedName = pPed ? pPed->GetModelName() : "";
					const char* vehName = pVehicle ? pVehicle->GetModelName() : "";

					const CPhysical* pPrimaryEntity = pVehicle ? static_cast<const CPhysical*>(pVehicle) : static_cast<const CPhysical*>(pPed);

					const char* pNetOwner = "";
					if(NetworkInterface::IsGameInProgress() && pPrimaryEntity)
					{
						if(pPrimaryEntity->IsNetworkClone())
						{
							pNetOwner = "Clone";
						}
						else
						{
							pNetOwner = "Local";
						}
					}

					Vec3V posV;
					if(pPrimaryEntity)
					{
						posV = pPrimaryEntity->GetMatrixRef().GetCol3();
					}
					else
					{
						posV.ZeroComponents();
					}

					formatf(buff, "%-8d %-8d %-20s %-10p %-10s %-10p %-1d %-1d %-6.1f %-6.1f %-5.1f %-5s", u->m_ChainId, u->m_NodeIdx, pedName, (void*)pPed, vehName, (void*)pVehicle, u->m_IsDummy, u->m_RefCount,
							posV.GetXf(), posV.GetYf(), posV.GetZf(), pNetOwner);

					if(i < ucount)
					{
						aiDisplayf("%1d: %s", i, buff);
					}
					else
					{
						aiDisplayf("   %s", buff);
					}
				}

				s_Reported = true;
				aiAssertf(0, "Trying to add a user of a chain beyond the allowed max of %d chain %d in region %d, see TTY for more info.", m_MaxUsers, user->m_ChainId, user->m_RegionId);
			}
#endif	// __ASSERT
		}
	}

	m_Users.PushAndGrow(user);
}

void CScenarioChain::RemoveUser(CScenarioPointChainUseInfo* user)
{
	int index = m_Users.Find(user);
	if (Verifyf(index != -1, "Trying to delete a user for chain %d in region %d that is not found. Double Delete?", user->m_ChainId, user->m_RegionId))
	{
		m_Users[index] = NULL;
		m_Users.DeleteFast(index);
	}
}

void CScenarioChain::ResetUsers(bool permanentlyRemoveUsers, bool reclaimMemory)
{
	for (int u = 0; u < m_Users.GetCount(); u++)
	{
		//Reset should not kill any ped/vehicle/dummy mappings 
		// the ped and vehicle reseting is done by individual functions in other locations
		// the dummy reseting is done in CScenarioChainingGraph::UnregisterPointChainUser
		m_Users[u]->ResetIndexInfo(permanentlyRemoveUsers);
		m_Users[u] = NULL;
	}
	if(reclaimMemory)
	{
		m_Users.Reset();
	}
	else
	{
		m_Users.ResetCount();
	}
}

bool CScenarioChain::IsFull() const
{
	return (m_Users.GetCount() >= m_MaxUsers)? true : false;
}

void CScenarioChain::AddEdgeId(u16 edgeId)
{
#if __ASSERT
	const int eidcount = m_EdgeIds.GetCount();
	for (int eid = 0; eid < eidcount; eid++)
	{
		if (m_EdgeIds[eid] == edgeId)
		{
			Assertf(m_EdgeIds[eid] != edgeId, "Duplicate add of edge [%d] to chain", edgeId);
		}
	}
#endif // __ASSERT

	m_EdgeIds.PushAndGrow(edgeId);
}

void CScenarioChain::PostPsoPlace(void* data)
{
	//ONLY PUT NON-PSO data init in here ...
	Assert(data);
	CScenarioChain* initData = reinterpret_cast<CScenarioChain*>(data);

	rage_placement_new(&initData->m_Users) atArray<CScenarioPointChainUseInfo*>();
}

#if __BANK
void CScenarioChain::Reset()
{
	ResetUsers();
	m_EdgeIds.ResetCount();
	m_MaxUsers = 1;
}

void CScenarioChain::RemoveEdgeId(u16 edgeId)
{
	const int eidcount = m_EdgeIds.GetCount();
	for (int eid = 0; eid < eidcount; eid++)
	{
		if (m_EdgeIds[eid] == edgeId)
		{
			//there should only be one
			m_EdgeIds.Delete(eid);
			break;
		}
	}
	Assertf(m_EdgeIds.Find(edgeId) == -1, "Edge [%d] is in a chain's id list multiple times.", edgeId);
}

void CScenarioChain::ShiftEdgeIdsForIdRemoval(u16 edgeId)
{
	const int eidcount = m_EdgeIds.GetCount();
	for (int eid = 0; eid < eidcount; eid++)
	{
		if (m_EdgeIds[eid] > edgeId)
		{
			//shift the edgeid down by one because the passed in edgeid is no longer valid
			m_EdgeIds[eid] = m_EdgeIds[eid]-1;
		}
	}
}

#if DR_ENABLED

void CScenarioChain::RenderUsage(const char* regionName, debugPlayback::OnScreenOutput& screenSpew) const
{
	const int ucount = m_Users.GetCount();
	for (int user = 0; user < ucount; user++)
	{
		RenderUsageSingle(regionName, *m_Users[user], screenSpew);
	}
}

void CScenarioChain::RenderUsageSingle(const char* regionName, const CScenarioPointChainUseInfo& user, debugPlayback::OnScreenOutput& screenSpew)
{
	char buffer[RAGE_MAX_PATH];
	const char* pedName = (user.m_Ped)? user.m_Ped->GetModelName() : "";
	const char* vehName = (user.m_Vehicle)? user.m_Vehicle->GetModelName() : "";
	formatf(buffer, RAGE_MAX_PATH, "%-32s%8d%8d%10p%24s%10p%15s%10p%6d%11d%5d", regionName, user.m_ChainId, user.m_NodeIdx, &user, pedName, user.m_Ped.Get(), vehName, user.m_Vehicle.Get(), user.m_IsDummy, user.m_RefCount, user.m_NumRefsInSpawnHistory);
	screenSpew.AddLine(buffer);
}

#endif // DR_ENABLED

#endif // __BANK

//-----------------------------------------------------------------------------

FW_INSTANTIATE_CLASS_POOL(CScenarioPointChainUseInfo, CONFIGURED_FROM_FILE, atHashString("CScenarioPointChainUseInfo",0x8d748d4e));

//-----------------------------------------------------------------------------

CScenarioChainingGraph::CScenarioChainingGraph()
		: m_NodeToScenarioPointMappingUpToDate(true)
{
}

CScenarioChainingGraph::~CScenarioChainingGraph()
{
	//If we have chain users then we need to reset the chain use info so when those objects get unregistered we dont
	// attempt to remove users of chains that dont exist anymore.
	// Note: in the case of a region being streamed out, where we don't want to remove them permanently,
	// we should have already called ResetChainUsers(false), so it would be empty now and
	// the true parameter here shouldn't clear them out.
	ResetChainUsers(true);
}

#if __BANK

int CScenarioChainingGraph::FindNode(const CScenarioChainingNode& node) const
{
	const u32 scenarioTypeHash = node.m_ScenarioType;
	const u32 entityTypeHash = node.m_AttachedToEntityType;
	const Vec3V pointPosV = node.m_Position;

	const ScalarV thresholdDistSqV(square(sm_PointSearchRadius));

	const int numNodes = m_Nodes.GetCount();
	for(int i = 0; i < numNodes; i++)
	{
		const CScenarioChainingNode& otherNode = m_Nodes[i];
		if(otherNode.m_ScenarioType != scenarioTypeHash)
		{
			continue;
		}
		if(otherNode.m_AttachedToEntityType.GetHash() != entityTypeHash)
		{
			continue;
		}
		const ScalarV distSqV = DistSquared(pointPosV, otherNode.m_Position);
		if(IsGreaterThanAll(distSqV, thresholdDistSqV))
		{
			continue;
		}

		return i;
	}

	return -1;
}


int CScenarioChainingGraph::FindOrAddNode(const CScenarioChainingNode& node)
{
	int nodeIndex = FindNode(node);
	if(nodeIndex < 0)
	{
		nodeIndex = m_Nodes.GetCount();
		m_Nodes.Grow() = node;

		m_NodeToScenarioPointMappingUpToDate = false;
	}
	return nodeIndex;
}

void CScenarioChainingGraph::AddEdge(int fromIndex, int toIndex)
{
	taskAssert(fromIndex != toIndex);

	u16 edgeId = 0;
	Assign(edgeId, GetNumEdges());
	CScenarioChainingEdge& edge = m_Edges.Grow();
	edge.SetNodeIndexFrom(fromIndex);
	edge.SetNodeIndexTo(toIndex);

	ResetChainUsers(true);
	AddEdgeToChain(edgeId);
	UpdateNodeToChainIndexMapping();
}


void CScenarioChainingGraph::DeleteEdge(int edgeIndex)
{
	u16 u16EId = 0;
	Assign(u16EId, edgeIndex);

	ResetChainUsers(true);

	DeleteEdgeInternal(u16EId);

	//Cleanup
	RemoveEmptyChains();
	UpdateNodeToChainIndexMapping();
	RemoveLooseNodes();
}


void CScenarioChainingGraph::DeleteAnyNodesAndEdgesForPoint(const CScenarioPoint& point)
{
	for(int i = 0; i < GetNumNodes(); i++)
	{
		Assert(m_NodeToScenarioPointMappingUpToDate);
		if(m_NodeToScenarioPointMapping[i] == &point)
		{
			DeleteNodeAndConnectedEdges(i);
			i = -1;
			//NOTE: start over from beginning as deleting here may have deleted more than one node
			// deleting of more than one node is possible if there are any loose chain nodes generated from 
			// deleting edges that contine the point we are attempting to delete.
		}
	}
}


void CScenarioChainingGraph::DeleteNodeAndConnectedEdges(int chainingNodeIndex)
{
	//Delete any edges that use the node we are about to delete
	for(int j = 0; j < GetNumEdges(); j++)
	{
		CScenarioChainingEdge& edge = GetEdge(j);
		if(edge.GetNodeIndexFrom() == chainingNodeIndex || edge.GetNodeIndexTo() == chainingNodeIndex)
		{
			//This will just delete the edge from the chains
			// node cleanup is to be handled else where.
			u16 u16ID = 0;
			Assign(u16ID, j);
			DeleteEdgeInternal(u16ID);
			j--; // step back ... 
		}
	}

	//Delete the node ... 
	DeleteNodeInternal(chainingNodeIndex);

	//Cleanup
	RemoveEmptyChains();
	UpdateNodeToChainIndexMapping();
	RemoveLooseNodes();
}

/*
PURPOSE
	Set m_HasIncomingEdges and m_HasOutgoingEdges in all CScenarioChainingNode
	objects, based on the edges in the graph.
*/
void CScenarioChainingGraph::UpdateNodesHaveEdges()
{
	const int numNodes = m_Nodes.GetCount();
	for(int i = 0; i < numNodes; i++)
	{
		m_Nodes[i].m_HasIncomingEdges = m_Nodes[i].m_HasOutgoingEdges = false;
	}

	const int numEdges = GetNumEdges();
	for(int i = 0; i < numEdges; i++)
	{
		const CScenarioChainingEdge& edge = GetEdge(i);
		m_Nodes[edge.GetNodeIndexFrom()].m_HasOutgoingEdges = true;
		m_Nodes[edge.GetNodeIndexTo()].m_HasIncomingEdges = true;
	}
}

int CScenarioChainingGraph::RemoveDuplicateEdges()
{
	int totRemoved = 0;
	//Loop through all the edges
	for(int i = 0; i < GetNumEdges(); i++)
	{
		for(int j = i+1; j < GetNumEdges(); j++)
		{
			const CScenarioChainingEdge& edge1 = GetEdge(i);
			const CScenarioChainingEdge& edge2 = GetEdge(j);
			if ( 
				edge1.GetNodeIndexFrom() == edge2.GetNodeIndexFrom() &&
				edge1.GetNodeIndexTo() == edge2.GetNodeIndexTo()
			)
			{
				//Displayf("Duplicate Edge Found e(%d,%d) == e(%d,%d)", edge1.GetNodeIndexFrom(), edge1.GetNodeIndexTo(), edge2.GetNodeIndexFrom(), edge2.GetNodeIndexTo());
				totRemoved++;
				DeleteEdge(j);
				i--;
				break;
			}
		}
	}

	return totRemoved;
}

int CScenarioChainingGraph::RemoveLooseNodes()
{	
	int totalRemoved = 0;
	for(int n = 0; n < m_NodeToChainIndexMapping.GetCount(); n++)
	{
		//If the mapping index is -1 then we are not part of a chain ... 
		if (m_NodeToChainIndexMapping[n] == -1)
		{
			DeleteNodeInternal(n);
			n = -1; //start over in the index mapping ... 
			totalRemoved++;
		}
	}

	return totalRemoved;
}

int CScenarioChainingGraph::RemoveEmptyChains()
{
	int totalRemoved = 0;
	for (int c = 0; c < m_Chains.GetCount(); c++)
	{
		if (!m_Chains[c].GetNumEdges())
		{
			DeleteChain(c);
			c--;//step back one
			totalRemoved++;
		}
	}
	return totalRemoved;
}

void CScenarioChainingGraph::DeleteChain(int chainIndex)
{
	int numChainsBefore = m_Chains.GetCount();

	// Clear out the users for the chain we are about to delete.
	m_Chains[chainIndex].ResetUsers();

	// Start iterating past the chain we are about to delete. The chain
	// indices are about to change, so we need to update those for the users.
	for(int i = chainIndex + 1; i < numChainsBefore; i++)
	{
		CScenarioChain& chain = m_Chains[i];
		const atArray<CScenarioPointChainUseInfo*>& userList = chain.GetUsers();
		const int numUsers = userList.GetCount();
		for(int j = 0; j < numUsers; j++)
		{
			Assert(userList[j]->m_ChainId == i);

			// We are about to delete a chain using atArray::Delete(), so all
			// other chains will have their index reduced by 1.
			userList[j]->m_ChainId = (s16)(i - 1);
		}
	}

	m_Chains.Delete(chainIndex);
}

void CScenarioChainingGraph::PreSave()
{
	//Perform clean up things before we save out .. 
	int removed = RemoveDuplicateEdges();
	Displayf("PreSave removed %d duplicate edges", removed);
	removed = RemoveLooseNodes();
	Displayf("PreSave removed %d loose nodes", removed);
	removed = RemoveEmptyChains();
	Displayf("PreSave removed %d empty chains", removed);
}

void CScenarioChainingGraph::PrepSaveObject(const CScenarioChainingGraph& prepFrom)
{	  
	//copy over the PSO data ... 
	m_Nodes = prepFrom.m_Nodes;
	m_Edges = prepFrom.m_Edges;
	m_Chains = prepFrom.m_Chains;
}

#if DR_ENABLED
void CScenarioChainingGraph::RenderUsage(const char* regionName, debugPlayback::OnScreenOutput& screenSpew) const
{
	const int ccount = m_Chains.GetCount();
	for (int chain = 0; chain < ccount; chain++)
	{
		m_Chains[chain].RenderUsage(regionName, screenSpew);
	}
}
#endif // DR_ENABLED

#endif // __BANK

CScenarioPointChainUseInfo* CScenarioChainingGraph::RegisterPointChainUser(const CScenarioPoint& pt, CPed* pPed, CVehicle* pVeh, bool allowOverflow)
{
	Assertf(pt.IsChained(), "Point is not chained");

	CScenarioPointChainUseInfo* retval = NULL;
	if (CScenarioPointChainUseInfo::_ms_pPool->GetNoOfFreeSpaces() > 0)
	{
		retval = rage_new CScenarioPointChainUseInfo();
	}

	if (!retval)
	{
		Assertf(0, "Ranout of space in ScenarioPointChainUserInfos pool more than %d chains are currently in use by the world.", CScenarioPointChainUseInfo::_ms_pPool->GetSize());
		return NULL;
	}

	retval->m_Ped = pPed;
	retval->m_Vehicle = pVeh;

	bool found = false;
	const int rcount = SCENARIOPOINTMGR.GetNumRegions();
	for (int i = 0; i < rcount; i++)
	{
		CScenarioPointRegion* region = SCENARIOPOINTMGR.GetRegion(i);
		if (region)
		{
			Assign(retval->m_RegionId, i);
			if (region->RegisterPointChainUser(pt, retval, allowOverflow))
			{
				found = true;
				break;
			}
		}
	}

	if (!found)
	{
#if __ASSERT
		// First, check the regions and see if we can identify which one this point was supposed to be in.
		bool foundInRegion = false;
		for(int i = 0; i < rcount; i++)
		{
			const CScenarioPointRegion* region = SCENARIOPOINTMGR.GetRegion(i);
			if(region)
			{
				int clusterId = -1;
				int pointId = -1;
				if(pt.IsInCluster())
				{
					region->FindClusteredPointIndex(pt, clusterId, pointId);
				}
				else
				{
					region->FindPointIndex(pt, pointId);
				}

				if(pointId >= 0)
				{
					// We found the point in this node. Next, call FindChainingNodeIndexForScenarioPoint() and
					// see if we can find the chaining node - this is likely to fail since that's basically what
					// we already did through CScenarioPointRegion::RegisterPointChainUser() above.
					const CScenarioChainingGraph& graph = region->GetChainingGraph();
					const int nodeidx = graph.FindChainingNodeIndexForScenarioPoint(pt);

					// If we didn't find it, dump all the nodes. It may be helpful to see what is associated
					// with the node that this point was supposed to belong to.
					if(nodeidx < 0)
					{
						static bool s_DumpedNodes = false;
						if(!s_DumpedNodes)	// Only do this once.
						{
							for(int j = 0; j < graph.GetNumNodes(); j++)
							{
								const CScenarioChainingNode& node = graph.GetNode(j);
								aiDisplayf("Node %d: %.1f %.1f %.1f - %p", j, node.m_Position.GetXf(), node.m_Position.GetYf(), node.m_Position.GetZf(), graph.GetChainingNodeScenarioPoint(j));
							}
							s_DumpedNodes = true;
						}
					}

					aiAssertf(0, "RegisterPointChainUser() about to fail: found point %p in region %s, FindChainingNodeIndexForScenarioPoint() returned %d. The point is at %.1f, %.1f, %.1f.",
							&pt, SCENARIOPOINTMGR.GetRegionName(i), nodeidx,
							pt.GetWorldPosition().GetXf(), pt.GetWorldPosition().GetYf(), pt.GetWorldPosition().GetZf());
					foundInRegion = true;	// Remember that we found it.
				}
			}
		}

		// If we didn't find it, dump a list of all loaded regions (once). Is the one that this point is supposed
		// to belong to perhaps not considered loaded?
		// Note: don't do this for entity points - it's probably possible in that case that the scenario point exists
		// even if the region that contains the graph that connects to the point isn't loaded.
		if(!foundInRegion && !pt.GetEntity())
		{
			static bool s_DumpedRegions = false;
			if(!s_DumpedRegions)
			{
				for(int i = 0; i < rcount; i++)
				{
					const CScenarioPointRegion* region = SCENARIOPOINTMGR.GetRegion(i);
					if(region)
					{
						aiDisplayf("Region %d: %s", i, SCENARIOPOINTMGR.GetRegionName(i));
					}
				}

				s_DumpedRegions = true;
			}
			aiAssertf(foundInRegion, "RegisterPointChainUser() about to fail: point %p not found in any region. See above for what was loaded.", &pt);
		}
#endif	// __ASSERT

		delete retval;
		retval = NULL;
	}

	return retval;
}


CScenarioPointChainUseInfo* CScenarioChainingGraph::RegisterPointChainUserForSpecificChain(CPed* pPed, CVehicle* pVeh,
		int regionIndex, int chainIndex, int nodeIndex)
{
	CScenarioPointRegion* region = SCENARIOPOINTMGR.GetRegion(regionIndex);
	if(!aiVerifyf(region, "Expected a valid region (%d).", regionIndex))
	{
		return NULL;
	}

	CScenarioPointChainUseInfo* retval = NULL;
	if (CScenarioPointChainUseInfo::_ms_pPool->GetNoOfFreeSpaces() > 0)
	{
		retval = rage_new CScenarioPointChainUseInfo();
	}

	if(!retval)
	{
		Assertf(0, "Ranout of space in ScenarioPointChainUserInfos pool more than %d chains are currently in use by the world.", CScenarioPointChainUseInfo::_ms_pPool->GetSize());
		return NULL;
	}
	retval->m_Ped = pPed;
	retval->m_Vehicle = pVeh;
	Assign(retval->m_RegionId, regionIndex);
	region->RegisterPointChainUserForSpecificChain(*retval, chainIndex, nodeIndex);
	return retval;
}


CScenarioPointChainUseInfo* CScenarioChainingGraph::RegisterChainUserDummy(const CScenarioPoint& pt, bool allowOverflow)
{
	CScenarioPointChainUseInfo* retval = RegisterPointChainUser(pt, NULL, NULL, allowOverflow);
	if (retval)
	{
		retval->m_IsDummy = true;
		retval->Validate();
	}

	//Vec3V pos = pt.GetPosition();
	//Displayf("Dummy Chain User {%x} <<%.5f, %.5f, %.5f>> {%x}", retval, pos.GetXf(), pos.GetYf(), pos.GetZf(), &pt);

	return retval;
}

void CScenarioChainingGraph::UpdateDummyChainUser(CScenarioPointChainUseInfo& inout, CPed* pPed, CVehicle* pVeh)
{
	Assert(!inout.m_Ped.Get());
	Assert(!inout.m_Vehicle.Get());
	Assertf(inout.m_IsDummy, "Trying to update a dummy chainUser what is not a dummy user");

	inout.m_Ped = pPed;
	inout.m_Vehicle = pVeh;
	inout.m_IsDummy = false; //This is no longer a dummy 

	inout.Validate();

	//Displayf("Undummy Chain User {%x} P %s {%x}, V %s {%x}", &inout, pPed->GetModelName(), pPed, pVeh->GetModelName(), pVeh);
}

void CScenarioChainingGraph::UnregisterPointChainUser(CScenarioPointChainUseInfo& chainUser)
{
	Assert(!chainUser.m_Ped.Get());
	Assert(!chainUser.m_Vehicle.Get());

	//might be already released ... so ignore it if it has
	if (chainUser.m_RegionId != -1)
	{
		Assert(chainUser.m_RegionId >= 0);
		Assert(chainUser.m_RegionId < SCENARIOPOINTMGR.GetNumRegions());

		CScenarioPointRegion* region = SCENARIOPOINTMGR.GetRegion(chainUser.m_RegionId);
		if (region)
		{
			region->RemovePointChainUser(chainUser);
		}

		//Displayf("Unregister Chain User {%x} P {%x}, V {%x} Dummy %d", &chainUser, chainUser.m_Ped.Get(), chainUser.m_Vehicle.Get(), chainUser.m_IsDummy);
	}

	chainUser.ResetIndexInfo(true);
	chainUser.m_IsDummy = false;

	Assert(!chainUser.m_NumRefsInSpawnHistory);

	chainUser.Validate();
}

void CScenarioChainingGraph::UpdateNewlyLoadedScenarioRegions()
{
	if(!sm_LoadedNewScenarioRegion)
	{
		return;
	}

	const int numUsers = CScenarioPointChainUseInfo::_ms_pPool->GetSize();
	const int numRegions = SCENARIOPOINTMGR.GetNumRegions();
	for(int i = 0; i < numRegions; i++)
	{
		CScenarioPointRegion* pReg = SCENARIOPOINTMGR.GetRegionFast(i);
		if(!pReg || !pReg->GetNeedsToBeCheckedForExistingChainUsers())
		{
			continue;
		}

		for(int u = 0; u < numUsers; u++)
		{
			CScenarioPointChainUseInfo* pUser = CScenarioPointChainUseInfo::_ms_pPool->GetSlot(u);
			if(pUser && !pUser->IsIndexInfoValid())
			{
				if(pUser->GetUnstreamedRegionId() == i)
				{
					pUser->RestoreIndexInfoFromUnstreamed();

					// Note: we could potentially protect against the chain being full here,
					// but there is already some stuff for that inside of AddChainUser(),
					// and the assert in RegisterPointChainUser() should have caught problems
					// already.
					pReg->AddChainUser(*pUser);
				}
			}
		}

		pReg->SetNeedsToBeCheckedForExistingChainUsers(false);
	}

	sm_LoadedNewScenarioRegion = false;
}


void CScenarioChainingGraph::UpdatePointChainUserInfo()
{
	const int numUsers = CScenarioPointChainUseInfo::_ms_pPool->GetSize();

	for(int u=0; u < numUsers; u++)
	{
		CScenarioPointChainUseInfo* pUser = CScenarioPointChainUseInfo::_ms_pPool->GetSlot(u);
		if(pUser)
		{
			RemoveChainUserIfUnused(*pUser);
		}
	}
}


bool CScenarioChainingGraph::RemoveChainUserIfUnused(CScenarioPointChainUseInfo& user)
{
	if (!user.m_Ped.Get() && !user.m_Vehicle.Get() && !user.m_IsDummy && !user.m_NumRefsInSpawnHistory)
	{
		Assertf(user.m_RefCount<=1, "Something still has a reference to this user data (R) %d, (C) %d, (N) %d 0x%p", user.m_RegionId, user.m_ChainId, user.m_NodeIdx, &user);
		UnregisterPointChainUser(user);
		Assertf(user.m_RefCount<1, "After UnregisterPointChainUser(), something has a reference to this user data (R) %d, (C) %d, (N) %d 0x%p", user.m_RegionId, user.m_ChainId, user.m_NodeIdx, &user);
		//if (Verifyf(user.m_RefCount==0, "%d refs remain for user data 0x%x ... not deleting now...", user.m_RefCount, pUser))
		{
			delete &user;
		}
		return true;
	}
	return false;
}


const atArray<CScenarioPointChainUseInfo*>& CScenarioChainingGraph::GetChainUserArrayContainingUser(CScenarioPointChainUseInfo& chainUser)
{
	Assert(chainUser.m_RegionId != -1);
	Assert(chainUser.m_RegionId >= 0);
	Assert(chainUser.m_RegionId < SCENARIOPOINTMGR.GetNumRegions());

	CScenarioPointRegion* region = SCENARIOPOINTMGR.GetRegion(chainUser.m_RegionId);
	Assert(region);
	return region->GetChainUserArrayContainingUser(chainUser);
}

void CScenarioChainingGraph::PostLoad(CScenarioPointRegion& reg)
{
	UpdateNodeToScenarioPointMapping(reg, true);

	if (!m_Chains.GetCount())//Legacy Convert
	{
		atArray<int> nodeToChainIndexMapping;
		const int ccount = LegacyUpdateNodeToChainIndexMapping(nodeToChainIndexMapping);

		m_Chains.Reset();
		m_Chains.Resize(ccount);
		const int ecount = GetNumEdges();
		for (int e = 0; e < ecount; e++)
		{
			const CScenarioChainingEdge& edge = GetEdge(e);
			const int from = edge.GetNodeIndexFrom();
			const int fromCI = nodeToChainIndexMapping[from];

#if __ASSERT
			const int to = edge.GetNodeIndexTo();
			const int toCI = nodeToChainIndexMapping[to];
			Assert(fromCI == toCI);
#endif
			m_Chains[fromCI].AddEdgeId((u16)e);
		}

		//Remove any empty chains ... if they exist.
#if __BANK
		RemoveEmptyChains();
#endif
	}

	UpdateNodeToChainIndexMapping();
}

void CScenarioChainingGraph::PostPsoPlace(void* data)
{
	//ONLY PUT NON-PSO data init in here ...
	Assert(data);
	CScenarioChainingGraph* initData = reinterpret_cast<CScenarioChainingGraph*>(data);

	rage_placement_new(&initData->m_NodeToChainIndexMapping) atArray<int>();
	rage_placement_new(&initData->m_NodeToScenarioPointMapping) atArray<RegdScenarioPnt>();
	initData->m_NodeToScenarioPointMappingUpToDate = false;
}

int CScenarioChainingGraph::FindChainingNodeIndexForScenarioPoint(const CScenarioPoint& pt) const
{
	Assert(m_NodeToScenarioPointMappingUpToDate);

	const int numNodes = GetNumNodes();
	for(int i = 0; i < numNodes; i++)
	{
		const CScenarioPoint* pScenarioPoint = m_NodeToScenarioPointMapping[i];
		if(pScenarioPoint == &pt)
		{
			return i;
		}
	}

	return -1;
}

int CScenarioChainingGraph::FindChainedScenarioPointsFromScenarioPoint(const CScenarioPoint& pt, CScenarioChainSearchResults* results, const int maxInDestArray) const
{
	const int sourceNodeIndex = FindChainingNodeIndexForScenarioPoint(pt);
	if(sourceNodeIndex < 0)
	{
		return 0;
	}

	return FindChainedScenarioPointsFromNodeIndex(sourceNodeIndex, results, maxInDestArray);
}

int CScenarioChainingGraph::FindChainedScenarioPointsFromNodeIndex(const int nodeIndex, CScenarioChainSearchResults* results, const int maxInDestArray) const
{
	int numFound = 0;

	const CScenarioChain& chain = GetChain(GetNodesChainIndex(nodeIndex));
	const int numEdges = chain.GetNumEdges();
	for(int e = 0; e < numEdges; e++)
	{
		u16 eid = chain.GetEdgeId(e);
		const CScenarioChainingEdge& edge = GetEdge(eid);
		if(edge.GetNodeIndexFrom() != nodeIndex)
		{
			continue;
		}

		CScenarioPoint* pScenarioPoint = GetChainingNodeScenarioPoint(edge.GetNodeIndexTo());
		if(!pScenarioPoint)
		{
			// Didn't find anything, could be that this node is a scenario attached to an entity
			// that's not streamed in, or has otherwise been moved or removed.
			continue;
		}

		if(Verifyf(numFound < maxInDestArray, "Filled up destination array while attempting to find points chained to a point, consider increasing."))
		{
			results[numFound].m_Point = pScenarioPoint;
			results[numFound].m_Action = (u8)edge.GetAction();
			results[numFound].m_NavMode = (u8)edge.GetNavMode();
			results[numFound].m_NavSpeed = (u8)edge.GetNavSpeed();
			numFound++;
		}
		else
		{
			break;
		}
	}

	return numFound;
}

void CScenarioChainingGraph::UpdateNodeToScenarioPointMapping(CScenarioPointRegion& reg, bool force)
{
	if(!force && m_NodeToScenarioPointMappingUpToDate)
	{
		// Shouldn't be anything to do.
		return;
	}

	const int numNodes = m_Nodes.GetCount();
	if(m_NodeToScenarioPointMapping.GetCount() != numNodes)
	{
		//reset the points we have already gotten
		for(int i = 0; i < m_NodeToScenarioPointMapping.GetCount(); i++)
		{
			if (m_NodeToScenarioPointMapping[i])
			{
				m_NodeToScenarioPointMapping[i]->SetRunTimeFlag(CScenarioPointRuntimeFlags::IsChained, false);
			}
		}

		m_NodeToScenarioPointMapping.Reset();
		m_NodeToScenarioPointMapping.Resize(numNodes);
	}

	CScenarioPoint* areaPoints[CScenarioPointManager::kMaxPtsInArea];

	// As we go through the chaining nodes, we can build a bounding box for cheap,
	// which will come in handy further down.
	Vec3V bndMaxV(V_NEG_FLT_MAX);
	Vec3V bndMinV(V_FLT_MAX);

	// Loop over all the nodes in the graph.
	int numNotFound = 0;
	const ScalarV pointSearchRadiusV = LoadScalar32IntoScalarV(sm_PointSearchRadius);
	for(int i = 0; i < numNodes; i++)
	{
		const CScenarioChainingNode& node = m_Nodes[i];

		// Get the coordinates of the node, build a tiny spdSphere around it, and update
		// the bounding box.
		const Vec3V centerPosV = node.m_Position;
		spdSphere sphere(centerPosV, pointSearchRadiusV);
		bndMaxV = Max(bndMaxV, centerPosV);
		bndMinV = Min(bndMinV, centerPosV);

		// Find a point within the sphere, if one exists. Note: we only do this
		// in the current region - the editor doesn't currently support chaining between
		// regions anyway. Note that FindPointsInArea() will stop once it's found a point.
		CScenarioPoint* pPointFound = NULL;
		int pointCount = reg.FindPointsInArea(sphere, sphere, areaPoints, 1, true);
		if(pointCount)
		{
			// Check if the point that we found matches the node. This would be the common case.
			CScenarioPoint* pFirstPoint = areaPoints[0];
			if(CheckNodeMatchesScenarioPoint(node, *pFirstPoint))
			{
				pPointFound = pFirstPoint;
			}
			else
			{
				// It's somewhat possible that we found some other point at the same position, even
				// though we have tried to make that impossible in the editor. To deal with that case,
				// we take another peek, this time getting all the points at this position.
				// Note: we only have to do this if we found a point but it didn't match, if we didn't
				// find a point there at all (which would be a more common case, since it would happen
				// for chained entity points), we wouldn't get here.
				pointCount = reg.FindPointsInArea(sphere, sphere, areaPoints, NELEM(areaPoints), true);
				for(int j = 0; j < pointCount; j++)
				{
					taskAssert(areaPoints[j]);
					CScenarioPoint& scenarioPt = *areaPoints[j];

					// Test this point, unless it's the one we already tested above.
					if(&scenarioPt != pFirstPoint && CheckNodeMatchesScenarioPoint(node, scenarioPt))
					{
						pPointFound = &scenarioPt;
						break;
					}
				}
			}
		}

		// Update the array.
		m_NodeToScenarioPointMapping[i] = pPointFound;

		// If we found a point, consider it chained. If not, keep count of it.
		if (pPointFound)
		{
			pPointFound->SetRunTimeFlag(CScenarioPointRuntimeFlags::IsChained, true);
		}
		else
		{
			numNotFound++;
		}
	}

	// If we didn't have any chained entity points in this graph, and no internal
	// inconsistency in the data, numNotFound would be zero and we would be done.
	// If we didn't manage to bind all the points, we have to look at entity-attached points.
	if(numNotFound)
	{
		// It's time to use the bounding box. We pad it a little bit so we don't run into
		// trouble at its boundaries.
		Vec3V bndPaddingV(pointSearchRadiusV);
		bndMinV = Subtract(bndMinV, bndPaddingV);
		bndMaxV = Add(bndMaxV, bndPaddingV);

		// Next, we will compute the world space positions of all entity points. This isn't
		// exactly cheap, but much cheaper than what we used to do, which was to compute all
		// positions of all entity points for all nodes.
		static const int kMaxEntityPoints = CScenarioPointManager::kMaxPtsInArea;
		Vec4V entityPointPosAndIndexArray[kMaxEntityPoints];

		// Determine the number of points that exist and we have space for.
		int numEntityPoints = SCENARIOPOINTMGR.GetNumEntityPoints();
		Assert(numEntityPoints < kMaxEntityPoints);
		numEntityPoints = Min(numEntityPoints, kMaxEntityPoints);

		// Loop over the points.
		int numEntityPointsInBB = 0;
		for(int i = 0; i < numEntityPoints; i++)
		{
			// Compute the world space position.
			const Vec3V posV = SCENARIOPOINTMGR.GetEntityPoint(i).GetPosition();

			// Check if it's within the bounding box. Otherwise, it's irrelevant
			// for this graph, and we don't have to store it, thus reducing the amount
			// of work we have to do per node.
			const VecBoolV largerThanMinV = IsGreaterThanOrEqual(posV, bndMinV);
			const VecBoolV smallerThanMaxV = IsLessThanOrEqual(posV, bndMaxV);
			const VecBoolV withinBndV = And(smallerThanMaxV, largerThanMinV);
			if(IsTrueXYZ(withinBndV))
			{
				// Construct a Vec4V with the position, where the W component is
				// the index of the entity point in SCENARIOPOINTMGR's array.
				Vec4V posAndIndex;
				posAndIndex.SetXYZ(posV);
				posAndIndex.SetWi(i);
				entityPointPosAndIndexArray[numEntityPointsInBB++] = posAndIndex;
			}
		}

		// If we found any entity scenarios within the box, try to bind them to the nodes.
		// If not, there is no point going over the nodes again.
		if(numEntityPointsInBB)
		{
			// Note: we could speed this up further, by doing some sort of spatial organization
			// (perhaps sorting by one of the coordinate axes, or arranging in a grid), or
			// just storing in SoA form and doing more efficient vector math.

			// Need the squared radius for distance checks within the loop.
			const ScalarV pointSearchRadiusSqV = Scale(pointSearchRadiusV, pointSearchRadiusV);

			// Loop over the nodes.
			for(int i = 0; i < numNodes; i++)
			{
				// Skip past all the ones that were bound successfully already.
				if(m_NodeToScenarioPointMapping[i])
				{
					continue;
				}

				// Get the center position of this node.
				const CScenarioChainingNode& node = m_Nodes[i];
				const Vec3V centerPosV = node.m_Position;

				// Loop over the attached scenario points within the bounding box.
				CScenarioPoint* pPointFound = NULL;
				for(int j = 0; j < numEntityPointsInBB; j++)
				{
					// Check if within the small sphere around the node.
					const Vec3V entityPointPosV = entityPointPosAndIndexArray[j].GetXYZ();
					const ScalarV distSqV = DistSquared(centerPosV, entityPointPosV);
					if(IsLessThanAll(distSqV, pointSearchRadiusSqV))
					{
						// Get the scenario point, and test if it matches (based on type, etc).
						const int entityPointIndex = entityPointPosAndIndexArray[j].GetElemi(3);
						CScenarioPoint& scenarioPt = SCENARIOPOINTMGR.GetEntityPoint(entityPointIndex);
						if(CheckNodeMatchesScenarioPoint(node, scenarioPt))
						{
							// Found one, no need to continue.
							pPointFound = &scenarioPt;
							break;
						}
					}
				}

				// If we found one, store it in the mapping and mark it as chained.
				if(pPointFound)
				{
					m_NodeToScenarioPointMapping[i] = pPointFound;
					pPointFound->SetRunTimeFlag(CScenarioPointRuntimeFlags::IsChained, true);

					// Count down the number we have not found. If it reaches zero, there is nothing
					// more to do. It's perfectly valid to not reach zero, though, since it could just
					// be that the props that we are chaining to haven't been streamed in yet.
					if(--numNotFound == 0)
					{
						break;
					}
				}
			}
		}
	}

	m_NodeToScenarioPointMappingUpToDate = true;
}


void CScenarioChainingGraph::UpdateNodeToScenarioMappingForAddedPoint(CScenarioPoint& pt)
{
	if(!m_NodeToScenarioPointMappingUpToDate)
	{
		// Not up to date to begin with, no use trying to maintain what we've got.
		return;
	}

	const int numNodes = GetNumNodes();
	for(int i = 0; i < numNodes; i++)
	{
		if(m_NodeToScenarioPointMapping[i])
		{
			// This chaining node is already mapped to a CScenarioPoint.
			continue;
		}

		const CScenarioChainingNode& node = GetNode(i);
		if(CheckNodeMatchesScenarioPoint(node, pt))
		{
			m_NodeToScenarioPointMapping[i] = &pt;

			//In case the game is running we need to update our chain fullness status as we could 
			// run into cases were a streamed in prop adds to a full chain, but then it is not marked as 
			// full resulting in potential duplicate users being added.
			Assert(i < m_NodeToChainIndexMapping.GetCount());
			const int chainID = m_NodeToChainIndexMapping[i];

			if (Verifyf(chainID != -1, "CScenarioChainingGraph::UpdateNodeToScenarioMappingForAddedPoint is expecting CScenarioChainingGraph::UpdateNodeToChainIndexMapping to have already been called."))
			{
				pt.SetRunTimeFlag(CScenarioPointRuntimeFlags::IsChained, true);
				pt.SetRunTimeFlag(CScenarioPointRuntimeFlags::ChainIsFull, m_Chains[chainID].IsFull());
			}
			break;
		}
	}
}

void CScenarioChainingGraph::UpdateNodeToChainIndexMapping()
{
	//OUTPUT_ONLY(sysTimer loadtime);

	//If we have chain users then we need to reset the chain use info so when those objects get unregistered we dont
	// attempt to remove users of chains that dont exist anymore.
	ResetChainUsers(true);

	//Make sure we have space for all our nodes...
	const int ncount = GetNumNodes();
	if (ncount != m_NodeToChainIndexMapping.GetCount())
	{
		m_NodeToChainIndexMapping.Reset();
		m_NodeToChainIndexMapping.Resize(ncount);
	}

	// used for diagnosing loose nodes.
	for(int n = 0; n < ncount; n++)
	{
		m_NodeToChainIndexMapping[n] = -1;
	}


#if __ASSERT
	// Set up an array for keeping track of which chain each edge belongs to.
	s16 edgesFound[2000];
	const int maxEdges = Min(NELEM(edgesFound), GetNumEdges());
	sysMemSet((void*)edgesFound, 0xff, sizeof(*edgesFound)*maxEdges);
#endif

	//Update the node chain ids ... 
	const int ccount = m_Chains.GetCount();
	for (int c = 0; c < ccount; c++)
	{
		const int ecount = m_Chains[c].GetNumEdges();
		for (int e = 0; e < ecount; e++)
		{
			const u16 eid = m_Chains[c].GetEdgeId(e);
			const CScenarioChainingEdge& edge = GetEdge(eid);

#if __ASSERT
			// Check if this edge is already a part of a chain, and if so, assert. If not,
			// store which chain it is.
			if(Verifyf(eid < maxEdges, "Edge out of range: %d", eid))
			{
				Assertf(edgesFound[eid] < 0 || edgesFound[eid] == c, "Edge %d in more than one chain (%d and %d).", eid, edgesFound[eid], c);
				Assertf(edgesFound[eid] < 0 || edgesFound[eid] != c, "Edge %d is more than once in the same chain (%d).", eid, c);
				edgesFound[eid] = (s16)c;
			}
#endif	// __ASSERT

			const int from = edge.GetNodeIndexFrom();
			const int to = edge.GetNodeIndexTo();

			Assertf(m_NodeToChainIndexMapping[from] < 0 || m_NodeToChainIndexMapping[from] == c, "Node %d in more than one chain, somehow (%d and %d).", from, m_NodeToChainIndexMapping[from], c);
			Assertf(m_NodeToChainIndexMapping[to] < 0 || m_NodeToChainIndexMapping[to] == c, "Node %d in more than one chain, somehow (%d and %d).", to, m_NodeToChainIndexMapping[to], c);

			m_NodeToChainIndexMapping[from] = c;
			m_NodeToChainIndexMapping[to] = c;
		}
	}

	//OUTPUT_ONLY(float time = loadtime.GetMsTime());
	//Displayf("Chain load UpdateNodeToChainIndexMapping use time %f ms", time);
}

int CScenarioChainingGraph::GetNodesChainIndex(const int nodeindex) const
{
	Assert(nodeindex >= 0);
	Assert(nodeindex < m_NodeToChainIndexMapping.GetCount());
	return m_NodeToChainIndexMapping[nodeindex];
}

void CScenarioChainingGraph::AddChainUser(CScenarioPointChainUseInfo* user, bool allowOverflow)
{
	Assert(user);
	Assert(user->m_ChainId >= 0);
	Assert(user->m_ChainId < m_Chains.GetCount());
	m_Chains[user->m_ChainId].AddUser(user, allowOverflow);

	//if the chain is now is full there are no slots left so set the full bit on all the user nodes.
	if (m_Chains[user->m_ChainId].IsFull())
	{
		UpdateChainFullStatus(m_Chains[user->m_ChainId], true);
	}
}

void CScenarioChainingGraph::RemoveChainUser(CScenarioPointChainUseInfo* user)
{
	Assert(user);
	Assert(user->m_ChainId >= 0);
	Assert(user->m_ChainId < m_Chains.GetCount());

	const int chainIndex = user->m_ChainId;

	const bool wasFull = m_Chains[chainIndex].IsFull();

	m_Chains[user->m_ChainId].RemoveUser(user);

	// If the chain was full need to remove the full bit on all the user nodes,
	// but only if it's not full now - it might have been overfull before, since we
	// allow that now in certain c ases.
	if (wasFull && !m_Chains[chainIndex].IsFull())
	{
		UpdateChainFullStatus(m_Chains[chainIndex], false);
	}
}

int CScenarioChainingGraph::LegacyUpdateNodeToChainIndexMapping(atArray<int>& outNodeToChainIndexMapping)
{
	//OUTPUT_ONLY(sysTimer loadtime);

	const int ncount = GetNumNodes();
	outNodeToChainIndexMapping.Resize(ncount);
	for(int n = 0; n < ncount; n++)
	{
		outNodeToChainIndexMapping[n] = -1;
	}	

	int count = 0;
	const int ecount = GetNumEdges();
	for(int e = 0; e < ecount; e++)
	{
		const CScenarioChainingEdge& edge = GetEdge(e);
		const int toI = edge.GetNodeIndexTo();
		const int frI = edge.GetNodeIndexFrom();
		Assert(toI < outNodeToChainIndexMapping.GetCount());
		Assert(frI < outNodeToChainIndexMapping.GetCount());
		const int toL = outNodeToChainIndexMapping[toI];
		const int frL = outNodeToChainIndexMapping[frI];

		if (toL == -1 && frL == -1) //Neither point logged ... make a new chain ... 
		{
			outNodeToChainIndexMapping[toI] = count;
			outNodeToChainIndexMapping[frI] = count;
			count++;
		}
		else if (toL != -1 && frL == -1) //To point logged ... 
		{
			outNodeToChainIndexMapping[frI] = toL;
		}
		else if (toL == -1 && frL != -1) //From point logged ... 
		{
			outNodeToChainIndexMapping[toI] = frL;
		}
		else if (toL != frL) //Both points are logged ...  but a merge is needed
		{
			//in the case of a needed merge the from point wins.
			// All the ones in the toL chain are now part of the frL chain
			for(int n = 0; n < ncount; n++)
			{
				if (outNodeToChainIndexMapping[n] == toL)
				{
					outNodeToChainIndexMapping[n] = frL;
				}
			}
		}

		Assert(outNodeToChainIndexMapping[toI] == outNodeToChainIndexMapping[frI]);		
	}

#if __ASSERT

	// If in a debug build, loop through the mapping and make sure everything is linked up properly.
	for(int n = 0; n < ncount; n++)
	{
		if (!Verifyf(outNodeToChainIndexMapping[n] >= 0 && outNodeToChainIndexMapping[n] < count, "Region has an invalid scenario chain mapping, resave using RAG!"))
		{
			break;
		}
	}

#endif

	//OUTPUT_ONLY(float time = loadtime.GetMsTime());
	//Displayf("Chain load LegacyUpdateNodeToChainIndexMapping use time %f ms", time);

	return count;
}


void CScenarioChainingGraph::ResetChainUsers(bool permanentlyRemoveUsers, bool reclaimMemory)
{
	for (int c = 0; c < m_Chains.GetCount(); c++)
	{
		m_Chains[c].ResetUsers(permanentlyRemoveUsers, reclaimMemory);
	}
}

void CScenarioChainingGraph::RemoveFromStore()
{
	ResetChainUsers(false, true);
	m_NodeToChainIndexMapping.Reset();
	m_NodeToScenarioPointMapping.Reset();
	m_NodeToScenarioPointMappingUpToDate = false; 
}

void CScenarioChainingGraph::UpdateChainFullStatus(const CScenarioChain& chain, const bool full) const
{
	const int eidcount = chain.GetNumEdges();
	for (int eid = 0; eid < eidcount; eid++)
	{
		const u16 e = chain.GetEdgeId(eid);
		const CScenarioChainingEdge& edge = GetEdge(e);

		CScenarioPoint* spoint = GetChainingNodeScenarioPoint(edge.GetNodeIndexFrom());
		if (spoint)
		{
			spoint->SetRunTimeFlag(CScenarioPointRuntimeFlags::ChainIsFull, full);
		}

		spoint = GetChainingNodeScenarioPoint(edge.GetNodeIndexTo());
		if (spoint)
		{
			spoint->SetRunTimeFlag(CScenarioPointRuntimeFlags::ChainIsFull, full);
		}
	}
}

#if __BANK
CScenarioChain& CScenarioChainingGraph::GetNewChain()
{
	CScenarioChain& newChain = m_Chains.Grow();
	newChain.Reset();//need to reset here as previous delete calls may have left residuals of 
					 //old chains which we want to ignore.
	return newChain;
}

void CScenarioChainingGraph::DeleteNodeInternal(u32 nodeIndex)
{
	Assert(nodeIndex < m_Nodes.GetCount());

	//Adjust the edges to be updated for the removed node ... 
	for(int j = 0; j < GetNumEdges(); j++)
	{
		CScenarioChainingEdge& edge = GetEdge(j);

		if(edge.GetNodeIndexFrom() > nodeIndex)
		{
			edge.SetNodeIndexFrom(edge.GetNodeIndexFrom() - 1);
			Assert(edge.GetNodeIndexFrom() >= 0);
			Assert(edge.GetNodeIndexFrom() < (m_Nodes.GetCount()-1));
		}
		if(edge.GetNodeIndexTo() > nodeIndex)
		{
			edge.SetNodeIndexTo(edge.GetNodeIndexTo() - 1);
			Assert(edge.GetNodeIndexTo() >= 0);
			Assert(edge.GetNodeIndexTo() < (m_Nodes.GetCount()-1));
		}
	}

	//Delete the node
	m_Nodes.Delete(nodeIndex);
	Assert(m_NodeToScenarioPointMappingUpToDate);
	if (m_NodeToScenarioPointMapping[nodeIndex])
	{
		m_NodeToScenarioPointMapping[nodeIndex]->SetRunTimeFlag(CScenarioPointRuntimeFlags::IsChained, false);
	}
	m_NodeToScenarioPointMapping.Delete(nodeIndex);
	m_NodeToChainIndexMapping.Delete(nodeIndex);
}

void CScenarioChainingGraph::AddEdgeToChain(u16 edgeId)
{
	const CScenarioChainingEdge& edge = GetEdge(edgeId);
	const int fromIndex = edge.GetNodeIndexFrom();
	const int toIndex = edge.GetNodeIndexTo();

	//Figure out what chain we should belong too
	int fromCI = -1;
	if (fromIndex < m_NodeToChainIndexMapping.GetCount())
	{
		fromCI = GetNodesChainIndex(fromIndex);
	}

	int toCI = -1;
	if (toIndex < m_NodeToChainIndexMapping.GetCount())
	{
		toCI = GetNodesChainIndex(toIndex);
	}

	if (fromCI == -1 && toCI == -1)
	{
		//Add a new chain as these dont currently belong to any chain
		CScenarioChain& newChain = GetNewChain();
		newChain.AddEdgeId(edgeId);
	}
	else if (fromCI != -1 && toCI == -1)
	{
		//Be part of the from chain
		m_Chains[fromCI].AddEdgeId(edgeId);
	}
	else if (fromCI == -1 && toCI != -1)
	{
		//Be part of the to chain
		m_Chains[toCI].AddEdgeId(edgeId);
	}
	else if (fromCI != toCI)
	{
		//Both points are already part of different chains ... need to merge chains
		// current standard is to merge all the "to" chain items into the "from" chain

		//Add the new edge
		m_Chains[fromCI].AddEdgeId(edgeId);	

		//Add all of the edges to the "from" chain 
		const int ecount = m_Chains[toCI].GetNumEdges();
		for (int e = 0; e < ecount; e++)
		{
			u16 transferId = m_Chains[toCI].GetEdgeId(e);
			m_Chains[fromCI].AddEdgeId(transferId);
		}

		//Remove the "to" chain as it is now merged with the "from" chain
		DeleteChain(toCI);
	}
	else
	{
		//Both points are already part of THE SAME chain ... so just add the edge
		m_Chains[fromCI].AddEdgeId(edgeId);	
	}
}

void CScenarioChainingGraph::RemoveEdgeFromChains(u16 edgeId)
{
	const CScenarioChainingEdge& edge = GetEdge(edgeId);
	const int fromIndex = edge.GetNodeIndexFrom();
	const int fromCI = GetNodesChainIndex(fromIndex);

#if __ASSERT
	const int toIndex = edge.GetNodeIndexTo();
	const int toCI = GetNodesChainIndex(toIndex);
	taskAssert(fromCI == toCI);
#endif

	m_Chains[fromCI].RemoveEdgeId(edgeId); // remove the desired edgeid

#if __ASSERT
	//There should only be one chain that contains this edge
	const int ccountAssert = m_Chains.GetCount();
	for (int c = 0; c < ccountAssert; c++)
	{
		const int ecount = m_Chains[c].GetNumEdges();
		for (int e = 0; e < ecount; e++)
		{
			Assertf(m_Chains[c].GetEdgeId(e) != edgeId, "Edge [%d] seems to be in multiple chains.", edgeId);
		}
	}
#endif

	//We now need to verify that the edgeId we removed did not create two chains out of our starting chain.
	atMap<int, int> checkedNodes;
	atArray<u16> splitChainEdges;

	const int ecount = m_Chains[fromCI].GetNumEdges();
	// if we have 0 or 1 edge after we removed the passed in edge id then we dont need to do this stuff
	if (ecount > 1) 
	{
		u16 eid = m_Chains[fromCI].GetEdgeId(0); //get the first
		const CScenarioChainingEdge& edgeToCheck = GetEdge(eid);
		const int fICheck = edgeToCheck.GetNodeIndexFrom();
		const int tICheck = edgeToCheck.GetNodeIndexTo();

		checkedNodes.Insert(fICheck, fICheck);
		checkedNodes.Insert(tICheck, tICheck);
		splitChainEdges.PushAndGrow(eid);

		atQueue<int, 20> checkQueue;
		checkQueue.Push(fICheck);
		checkQueue.Push(tICheck);

		while(!checkQueue.IsEmpty())
		{
			int nodeId = checkQueue.Pop();

			//any edge that uses this nodeid needs to be added to the old chain.
			for (int esub = 1; esub < ecount; esub++)
			{
				u16 eidSub = m_Chains[fromCI].GetEdgeId(esub);
				const CScenarioChainingEdge& edgeSub = GetEdge(eidSub);
				const int fiSub = edgeSub.GetNodeIndexFrom();
				const int tiSub = edgeSub.GetNodeIndexTo();

				if (fiSub == nodeId || tiSub == nodeId)
				{
					//Add this edge if we have not already
					if (splitChainEdges.Find(eidSub) == -1)
					{
						splitChainEdges.PushAndGrow(eidSub);
					}

					// if we have not checked this nodeid before
					if (!checkedNodes.Access(fiSub))
					{
						checkQueue.Push(fiSub);
						checkedNodes.Insert(fiSub, fiSub);
					}

					// if we have not checked this nodeid before
					if (!checkedNodes.Access(tiSub))
					{
						checkQueue.Push(tiSub);
						checkedNodes.Insert(tiSub, tiSub);
					}
				}
			}
		}

		//At this point edges that are linked should have been found
		// the chain should now show if a split happened when removing of the passed in edgeid
		if (splitChainEdges.GetCount() != ecount)
		{
			// a split happened so lets make a new chain out of the splitChainEdges array.
			CScenarioChain& newChain = GetNewChain();
			newChain.SetMaxUsers(m_Chains[fromCI].GetMaxUsers());
			const int ncount = splitChainEdges.GetCount();
			for (int e = 0; e < ncount; e++)
			{
				m_Chains[fromCI].RemoveEdgeId(splitChainEdges[e]);//remove from old
				newChain.AddEdgeId(splitChainEdges[e]);//add to new
			}			
		}
	}
}

void CScenarioChainingGraph::DeleteEdgeInternal(u16 edgeId)
{
	RemoveEdgeFromChains(edgeId);

	//If any of the edges are a higher index than this one we need to shift it down
	const int ccount = m_Chains.GetCount();
	for (int c = 0; c < ccount; c++)
	{
		m_Chains[c].ShiftEdgeIdsForIdRemoval(edgeId);
	}
	m_Edges.Delete(edgeId);
}
#endif

bool CScenarioChainingGraph::CheckNodeMatchesScenarioPoint(const CScenarioChainingNode& node, const CScenarioPoint& scenarioPoint)
{
	const u32 modelNameHash = scenarioPoint.GetEntity() ? scenarioPoint.GetEntity()->GetBaseModelInfo()->GetModelNameHash() : 0;
	if(node.m_AttachedToEntityType.GetHash() != modelNameHash)
	{
		return false;
	}

	u32 scenarioTypeHash = SCENARIOINFOMGR.GetHashForScenario(scenarioPoint.GetScenarioTypeVirtualOrReal());
	if(node.m_ScenarioType.GetHash() != scenarioTypeHash)
	{
		return false;
	}

	const Vec3V nodePosV = node.m_Position;
	const float range = sm_PointSearchRadius;

	const ScalarV thresholdDistSqV(square(range));

	const Vec3V scenarioPosV = scenarioPoint.GetPosition();

	const ScalarV distSqV = DistSquared(nodePosV, scenarioPosV);
	if(IsGreaterThanAll(distSqV, thresholdDistSqV))
	{
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------

// End of file 'task/Scenario/ScenarioChaining.cpp'
