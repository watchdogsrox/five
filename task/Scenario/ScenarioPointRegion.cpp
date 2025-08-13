//
// task/Scenario/ScenarioPointRegion.cpp
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

//Rage Headers
#if __BANK
#include "parser/visitorxml.h"
#endif

//Framework Headers
#include "ai/task/taskchannel.h"
#include "fwscene/world/SceneGraphNode.h"

//Game Headers
#include "ai/stats.h"
#include "Objects/object.h"
#include "Pathserver/ExportCollision.h"
#include "Peds/Ped.h"
#include "scene/Building.h"
#include "scene/Entity.h"
#include "task/Scenario/ScenarioManager.h"
#include "task/Scenario/ScenarioPointManager.h"
#include "task/Scenario/ScenarioPointRegion.h"

#if __BANK
#include "scene/FocusEntity.h"
#include "scene/world/GameWorld.h"
#include "task/Scenario/ScenarioDebug.h"
#include "task/Scenario/Info/ScenarioInfoManager.h"
#endif

AI_OPTIMISATIONS()

using namespace AIStats;

///////////////////////////////////////////////////////////////////////////////
//  CScenarioPointRegion
///////////////////////////////////////////////////////////////////////////////

CScenarioPointRegion::CScenarioPointRegion()
	: m_VersionNumber(0),
	m_bContainsBuildings(true), m_bContainsObjects(true), // Set those to true because we may access the overrides for entitys that where just created before we processed the final flag values
	m_bNeedsToBeCheckedForExistingChainUsers(true)
{
}

CScenarioPointRegion::~CScenarioPointRegion()
{
	RemoveFromStore();
}


void CScenarioPointRegion::RemoveFromStore()
{
	CallOnRemoveScenarioForPoints();

	m_Points.CleanUp();

	// When the scenario region gets destroyed, it probably makes the most
	// sense to also delete any override points. Technically, we could leave them
	// in there and they would get cleared out and replaced with fresh ones
	// when the scenario region streams in again, but there is some potential
	// for issues when having points that came in with a region and the region
	// is no longer in memory.
	// Note: it may seem natural to let the entity get its regular art-attached
	// points back, since that's what they would have had before this region streamed
	// in the first time. However, I can't think of anything good coming from that,
	// just increases the risk of us spawning something undesirable.
	const int numOverrides = GetNumEntityOverrides();
	if(numOverrides > 0)
	{
		for(int i = 0; i < numOverrides; i++)
		{
			CScenarioEntityOverride& override = m_EntityOverrides[i];
			CEntity* pEntity = override.m_EntityCurrentlyAttachedTo;
			if(pEntity)
			{
				SCENARIOPOINTMGR.DeletePointsWithAttachedEntity(pEntity);
				override.m_EntityCurrentlyAttachedTo = NULL;
			}
		}

		//in case it is still in the queue
		CScenarioEntityOverridePostLoadUpdateQueue::GetInstance().RemoveFromQueue(this);
	}

#if SCENARIO_VERIFY_ENTITY_OVERRIDES
	CScenarioEntityOverrideVerification* ver = CScenarioEntityOverrideVerification::GetInstancePtr();
	if(ver)
	{
		ver->ClearInfoForRegion(*this);
	}
#endif	// SCENARIO_VERIFY_ENTITY_OVERRIDES

	// Clear out all the CSPClusterFSMWrapper objects associated with this region.
	// This would be done anyway when destroying m_Clusters, but that's after the call
	// to SCENARIOPOINTMGR.InvalidateCachedRegionPtrs(), and in the process we could end up
	// indirectly calling CScenarioPointManager::UpdateCachedRegionPtrs(), before we have
	// cleared out the pointer to this CScenarioPointRegion in the metadata store, causing
	// crashes later by allowing access to deleted scenario data.
	const int numClusters = m_Clusters.GetCount();
	for(int i = 0; i < numClusters; i++)
	{
		m_Clusters[i].CleanUp();
	}

	SCENARIOPOINTMGR.InvalidateCachedRegionPtrs();

	m_ChainingGraph.RemoveFromStore();

	// Clear out some arrays that are not a part of the loaded data, so we don't
	// leak memory due to not running their destructors.
	m_EntityOverrideTypes.Reset();
	for(int i = 0; i < m_EntityOverrides.GetCount(); i++)
	{
		CScenarioEntityOverride& override = m_EntityOverrides[i];
		override.m_ScenarioPointsInUse.Reset();
		Assert(!override.m_EntityCurrentlyAttachedTo);	// This is a RegdEnt, make sure we don't leave it dangling since the destructor may not run.
	}
}


int CScenarioPointRegion::FindPointsInArea(const spdSphere& sphere, const spdSphere& exsphere, CScenarioPoint** destArray, const int maxInDestArray, bool includePointsInClusters)
{
	int numFound = 0;
	if(includePointsInClusters)
	{
		numFound += FindClusteredPointsInArea(sphere, exsphere, destArray, maxInDestArray);
	}

	//we filled our array ... 
	if (numFound >= maxInDestArray)
	{
		return numFound;
	}

	//Offset stuff so we add to the right position in the destArray
	int destSlotsLeft = maxInDestArray - numFound;
	CScenarioPoint** destPosition = &(destArray[numFound]);

	// If m_AccelGridNodeIndices is empty, there is no grid, and we have to look at all the points in the region.
	if(m_AccelGridNodeIndices.GetCount() == 0)
	{
		numFound += FindPointsInAreaDirect(sphere, exsphere, destPosition, destSlotsLeft, 0, m_Points.GetNumPoints());
	}
	else
	{
		numFound += FindPointsInAreaGrid(sphere, exsphere, destPosition, destSlotsLeft);
	}

	return numFound;
}

void CScenarioPointRegion::CallOnRemoveScenarioForPoints()
{
	const int pcount = m_Points.GetNumPoints();
	for (int p=0; p < pcount; p++)
	{
		CScenarioPoint& point = m_Points.GetPoint(p);
		if (point.GetOnAddCalled())
			CScenarioManager::OnRemoveScenario(point);
		point.CleanUp();
	}

	const int count = m_Clusters.GetCount();
	for (int c=0; c < count; c++)
	{
		const int pcount = m_Clusters[c].GetNumPoints();
		for (int p=0; p < pcount; p++)
		{
			CScenarioPoint& point = m_Clusters[c].GetPoint(p);
			if (point.GetOnAddCalled())
				CScenarioManager::OnRemoveScenario(point);
			point.CleanUp();
		}
	}
}

void CScenarioPointRegion::CallOnAddScenarioForPoints()
{
	const int pcount = m_Points.GetNumPoints();
	for (int p=0; p < pcount; p++)
	{
		CScenarioPoint& point = m_Points.GetPoint(p);
		CScenarioManager::OnAddScenario(point);
	}

	const int count = m_Clusters.GetCount();
	for (int c=0; c < count; c++)
	{
		const int pcount = m_Clusters[c].GetNumPoints();
		for (int p=0; p < pcount; p++)
		{
			CScenarioPoint& point = m_Clusters[c].GetPoint(p);
			CScenarioManager::OnAddScenario(point);
		}
	}
}

void CScenarioPointRegion::FindPointIndex(const CScenarioPoint& pt, int& pointIndex_Out) const
{
	pointIndex_Out = -1;
	const int pcount = m_Points.GetNumPoints();
	for (int p=0; p < pcount; p++)
	{
		const CScenarioPoint& point = m_Points.GetPoint(p);
		if (&point == &pt)
		{
			pointIndex_Out = p;
			break;
		}
	}
}

CScenarioPoint& CScenarioPointRegion::GetPoint(const u32 pointIndex)
{
	Assert(pointIndex < m_Points.GetNumPoints());
	return m_Points.GetPoint(pointIndex);
}

u32 CScenarioPointRegion::GetNumPoints() const
{
	return m_Points.GetNumPoints();
}

void CScenarioPointRegion::FindClusteredPointIndex(const CScenarioPoint& pt, int& clusterId_Out, int& pointIndex_Out) const
{
	clusterId_Out = -1;
	pointIndex_Out = -1;

	const int count = m_Clusters.GetCount();
	for (int c=0; c < count; c++)
	{
		const int pcount = m_Clusters[c].GetNumPoints();
		for (int p=0; p < pcount; p++)
		{
			const CScenarioPoint& point = m_Clusters[c].GetPoint(p);
			if (&point == &pt)
			{
				clusterId_Out = c;
				pointIndex_Out = p;
				break;
			}
		}
	}
}

int CScenarioPointRegion::GetNumClusters() const
{
	return m_Clusters.GetCount();
}

int CScenarioPointRegion::GetNumClusteredPoints(const int clusterId) const
{
	Assert(clusterId < m_Clusters.GetCount());
	return m_Clusters[clusterId].GetNumPoints();
}

CScenarioPoint& CScenarioPointRegion::GetClusteredPoint(const u32 clusterId, const u32 pointIndex)
{
	Assert(clusterId < m_Clusters.GetCount());
	Assert(pointIndex < m_Clusters[clusterId].GetNumPoints());
	return m_Clusters[clusterId].GetPoint(pointIndex);
}

bool CScenarioPointRegion::IsChainedWithIncomingEdges(const CScenarioPoint& pt) const
{
	if (pt.IsChained())
	{
		const int nodeIndex = m_ChainingGraph.FindChainingNodeIndexForScenarioPoint(pt);
		//NOTE: It is possible that this point is not in the region so need to check that the nodeidx is valid.
		if(nodeIndex >= 0)
		{
			return m_ChainingGraph.GetNode(nodeIndex).m_HasIncomingEdges;	
		}
	}
	
	return false;	// Not chained.
	
}

bool CScenarioPointRegion::RegisterPointChainUser(const CScenarioPoint& pt, CScenarioPointChainUseInfo* userInOut, bool allowOverflow)
{
	Assert(pt.IsChained());
	int nodeidx = m_ChainingGraph.FindChainingNodeIndexForScenarioPoint(pt);
	//NOTE: It is possible that this point is not in the region so need to check that the nodeidx is valid.
	if (nodeidx != -1)
	{
		const int chainIndex = m_ChainingGraph.GetNodesChainIndex(nodeidx);
		RegisterPointChainUserForSpecificChain(*userInOut, chainIndex, nodeidx, allowOverflow);
		return true;
	}
	return false;
}


void CScenarioPointRegion::RegisterPointChainUserForSpecificChain(CScenarioPointChainUseInfo& user, int chainIndex, int nodeIndex, bool allowOverflow)
{
	// If this fails, we may not yet have restored chain users from last time
	// this region was streamed in, so it's probably not a good idea to add
	// additional chain users now. Probably need to add a call to
	// CScenarioChainingGraph::UpdateNewlyLoadedScenarioRegions() to fix properly.
	Assertf(!m_bNeedsToBeCheckedForExistingChainUsers, "Chain users in scenario region may not have been properly restored: sm_LoadedNewScenarioRegion = %d", (int)CScenarioChainingGraph::GetLoadedNewScenarioRegion());

	Assign(user.m_ChainId, chainIndex);
	Assert(nodeIndex >= -0x8000 && nodeIndex <= 0x7fff);
	user.m_NodeIdx = (s16)nodeIndex;
	m_ChainingGraph.AddChainUser(&user, allowOverflow);
}


void CScenarioPointRegion::RemovePointChainUser(CScenarioPointChainUseInfo& user)
{
	m_ChainingGraph.RemoveChainUser(&user);
}

void CScenarioPointRegion::UpdateChainedNodeToScenarioMappingForAddedPoint(CScenarioPoint& pt)
{
	m_ChainingGraph.UpdateNodeToScenarioMappingForAddedPoint(pt);
}

int	CScenarioPointRegion::FindChainedScenarioPointsFromScenarioPoint(const CScenarioPoint& pt, CScenarioChainSearchResults* results, const int maxInDestArray) const
{
	Assert(pt.IsChained());
	return m_ChainingGraph.FindChainedScenarioPointsFromScenarioPoint(pt, results, maxInDestArray);
}

const atArray<CScenarioPointChainUseInfo*>& CScenarioPointRegion::GetChainUserArrayContainingUser(CScenarioPointChainUseInfo& chainUser)
{
	Assert(chainUser.m_ChainId >= 0);
	Assert(chainUser.m_ChainId < m_ChainingGraph.GetNumChains());
	const CScenarioChain& chain = m_ChainingGraph.GetChain(chainUser.m_ChainId);
#if __ASSERT
	const atArray<CScenarioPointChainUseInfo*>& users = chain.GetUsers();
	bool found = false;
	for (int u = 0; u < users.GetCount(); u++)
	{
		if (users[u] == &chainUser)
		{
			found = true;
			break;
		}
	}
	Assertf(found, "CScenarioChainingGraph::GetChainUserArrayContainingUser ... ChainUser seems to point to a chain that does not think it is a user.");
#endif
	return chain.GetUsers();
}

bool CScenarioPointRegion::TryToMakeSpaceForChainUser(int chainIndex)
{
	const CScenarioChain& chain = m_ChainingGraph.GetChain(chainIndex);

	// First, try to just free up one that would have been freed anyway,
	// during the next call toCScenarioChainingGraph::UpdatePointChainUserInfo()
	const atArray<CScenarioPointChainUseInfo*>& users = chain.GetUsers();
	const int numUsers = users.GetCount();
	for(int i = 0; i < numUsers; i++)
	{
		if(CScenarioChainingGraph::RemoveChainUserIfUnused(*users[i]))
		{
			return true;
		}
	}

	// Check if there are any unattached entries on this chain in the scenario
	// history. There is no physical presence there, and the chain will still be
	// marked as in use by the new user that wanted to make space.
	for(int i = 0; i < numUsers; i++)
	{
		CScenarioPointChainUseInfo& user = *users[i];
		if(user.GetPed() || user.GetVehicle() || user.IsDummy())
		{
			// Not free, something else is using this already.
			continue;
		}

		if(CScenarioManager::GetSpawnHistory().ReleasePointChainUseInfo(user))
		{
			return true;
		}
	}

	// Next, check if there are any dead peds that still are considered
	// chain users. If so, detach those from the chain.
	for(int i = 0; i < numUsers; i++)
	{
		CScenarioPointChainUseInfo& user = *users[i];
		CPed* pPed = user.GetPed();
		if(!pPed || user.GetVehicle() || user.IsDummy() || !pPed->IsInjured())
		{
			continue;
		}

		// Remove it from the chain, and clear its association with it.
		// Note that the actual object will persist, since probably a suspended
		// CTaskUnalerted or something in the dead ped still has a pointer to it.
		m_ChainingGraph.RemoveChainUser(&user);
		user.ResetIndexInfo(true);

		return true;
	}

	// Finally, in a networking game, allow chain users to be taken from
	// clones. This can perhaps happen if we somehow have too many users,
	// and need one for a locally controlled ped. Since this only happens
	// when the chain is already full, the new user should keep the chain
	// full anyway, and if the clone migrates and becomes a locally controlled
	// ped, I think his CTaskUnalerted will be replaced, and he himself will
	// run this code and have the chance to steal a chain user object if needed.
	if(NetworkInterface::IsGameInProgress())
	{
		for(int i = 0; i < numUsers; i++)
		{
			CScenarioPointChainUseInfo& user = *users[i];
			if(user.IsDummy())
			{
				continue;
			}
			CPed* pPed = user.GetPed();
			if(pPed && !pPed->IsNetworkClone())
			{
				continue;
			}
			CVehicle* pVehicle = user.GetVehicle();
			if(pVehicle && !pVehicle->IsNetworkClone())
			{
				continue;
			}

			Assert(pPed || pVehicle);

			m_ChainingGraph.RemoveChainUser(&user);
			user.ResetIndexInfo(true);
			return true;
		}
	}

	return false;
}

bool CScenarioPointRegion::ApplyOverridesForEntity(CEntity& entity, bool removeExistingPoints, bool bDoDirectSearch)
{
	const CBaseModelInfo* pModelInfo = entity.GetBaseModelInfo();
	if(!pModelInfo)
	{
		Assertf(entity.GetOwnedBy() == ENTITY_OWNEDBY_STATICBOUNDS || entity.GetOwnedBy() == ENTITY_OWNEDBY_EXPLOSION,
				"No base model info, unexpected owner: %d", (int)entity.GetOwnedBy());
		return false;	// Seems to happen for some CBuildings, not sure exactly what they are.
	}

#if __ASSERT
	if (entity.GetIplIndex() != 0)
	{
		if (!INSTANCE_STORE.HasObjectLoaded(strLocalIndex(entity.GetIplIndex())))
		{
			if (!INSTANCE_STORE.IsObjectLoading(strLocalIndex(entity.GetIplIndex())))
			{
				Assertf(false, "owning %s.imap for entity (%s) is not loaded!", INSTANCE_STORE.GetName(strLocalIndex(entity.GetIplIndex())), entity.GetModelName());
			}
		}
	}
#endif //__ASSERT

	// Search for an existing override.
	CScenarioEntityOverride* pFoundOverride = FindOverrideForEntity(entity, bDoDirectSearch);
	if(!pFoundOverride)
	{
		// No matching override found.
		return false;
	}

	// Remove existing scenario points from the entity.
	if(removeExistingPoints)
	{
		SCENARIOPOINTMGR.DeletePointsWithAttachedEntity(&entity);
	}

	// Note: we may want to do something with non-upright objects, but it's probably not as simple
	// as doing this, when considering the interaction with the editor code.
	//	// Ignore entities not the right way up-ish
	//	if(entity.GetTransform().GetC().GetZf() < 0.9f)	// ENTITY_SPAWN_UP_LIMIT
	//	{
	//		return true;
	//	}

	// Make sure m_ScenarioPointsInUse[] matches m_ScenarioPoints[].
	const int numPts = pFoundOverride->m_ScenarioPoints.GetCount();
	if(!pFoundOverride->m_ScenarioPointsInUse.GetCapacity())
	{
		pFoundOverride->m_ScenarioPointsInUse.Reserve(numPts);
		pFoundOverride->m_ScenarioPointsInUse.Resize(numPts);
	}
	Assert(numPts == pFoundOverride->m_ScenarioPointsInUse.GetCount());

	// Make sure m_EntityCurrentlyAttachedTo is properly set.
	Assert(pFoundOverride->m_EntityCurrentlyAttachedTo == NULL || pFoundOverride->m_EntityCurrentlyAttachedTo == &entity);
	pFoundOverride->m_EntityCurrentlyAttachedTo = &entity;

#if SCENARIO_VERIFY_ENTITY_OVERRIDES
	pFoundOverride->m_VerificationStatus = CScenarioEntityOverride::kVerificationStatusWorking;
#endif	// SCENARIO_VERIFY_ENTITY_OVERRIDES

	// Set up the points.
	for(int i = 0; i < numPts; i++)
	{
		// Create the point and initialize it from the stored data.
		CScenarioPoint* newPoint = CScenarioPointContainer::GetNewScenarioPointFromPool();
		if(!newPoint)	// Don't crash if we run out - asserts/errors are already taken care of within GetNewScenarioPoint().
		{
			continue;
		}
		newPoint->InitArchetypeExtensionFromDefinition(&pFoundOverride->m_ScenarioPoints[i], NULL /*?*/);

		// Attach it to the entity.
		newPoint->SetEntity(&entity);
		newPoint->SetIsEntityOverridePoint(true);

		//These points are from resourced data so mark them
		newPoint->MarkAsFromRsc();

		// Store it.
		Assert(!pFoundOverride->m_ScenarioPointsInUse[i]);
		pFoundOverride->m_ScenarioPointsInUse[i] = newPoint;

#if SCENARIO_DEBUG
		// These points are considered editable.
		newPoint->SetEditable(true);
#endif	// SCENARIO_DEBUG

		// Add it to the manager, as a regular entity point.
		SCENARIOPOINTMGR.AddEntityPoint(*newPoint);
	}

	// If we had any points, make sure that the entity and its container is marked accordingly.
	// This is important, because otherwise the points may not be cleaned up properly when the
	// entity is deleted.
	if(numPts > 0)
	{
		entity.m_nFlags.bHasSpawnPoints = true;

		if(entity.GetOwnerEntityContainer() && entity.GetOwnerEntityContainer()->GetOwnerSceneNode())
		{
			entity.GetOwnerEntityContainer()->GetOwnerSceneNode()->SetContainsSpawnPoints(true);
		}
	}

	// Applied the override successfully.
	return true;
}

int CScenarioPointRegion::GetNumEntityOverrides() const
{ 
	return m_EntityOverrides.GetCount(); 
}

int CScenarioPointRegion::FindPointsInAreaDirect(const spdSphere& sphere, const spdSphere& exsphere, CScenarioPoint** destArray, const int maxInDestArray, const int startIndex, const int endIndex)
{
	int curWanted = 0;

	taskAssert(endIndex <= m_Points.GetNumPoints());
	for(int i = startIndex; i < endIndex; i++)
	{
		CScenarioPoint& pt = m_Points.GetPoint(i);

		bool valid = false;
		Vec3V pos = pt.GetPosition();
		if(pt.HasExtendedRange())
		{
			valid = exsphere.ContainsPoint(pos);
		}
		else
		{
			valid = sphere.ContainsPoint(pos);
		}

		if(valid)
		{
			if(curWanted < maxInDestArray)
			{
				destArray[curWanted] = &pt;
				curWanted++;

				if(curWanted == maxInDestArray)
				{
					break;
				}
			}
		}
	}

	return curWanted;
}

int CScenarioPointRegion::FindPointsInAreaGrid(const spdSphere& sphere, const spdSphere& exsphere, CScenarioPoint** destArray, const int maxInDestArray)
{
	taskAssert(exsphere.GetRadiusf() >= sphere.GetRadiusf());

	// Compute the coordinates covered by the extended range sphere.
	// Note: we basically assume here that the normal sphere is contained within
	// the extended sphere.
	const float cullCircleCenterX = exsphere.GetCenter().GetXf();
	const float cullCircleCenterY = exsphere.GetCenter().GetYf();
	const float cullCircleRadius = exsphere.GetRadiusf();
	const float circleMinX = cullCircleCenterX - cullCircleRadius;
	const float circleMaxX = cullCircleCenterX + cullCircleRadius;
	const float circleMinY = cullCircleCenterY - cullCircleRadius;
	const float circleMaxY = cullCircleCenterY + cullCircleRadius;

	// Compute the cell coordinates for the rectangle we need to look within.
	int cellX1, cellX2, cellY1, cellY2;
	m_AccelGrid.CalcClampedCellX(cellX1, circleMinX);
	m_AccelGrid.CalcClampedCellX(cellX2, circleMaxX);
	m_AccelGrid.CalcClampedCellY(cellY1, circleMinY);
	m_AccelGrid.CalcClampedCellY(cellY2, circleMaxY);

	// Get the center and radius of the normal and extended spheres.
	const float sphereNormalCenterX = sphere.GetCenter().GetXf();
	const float sphereNormalCenterY = sphere.GetCenter().GetYf();
	const float sphereNormalRadius = sphere.GetRadiusf();
	const float sphereExtCenterX = exsphere.GetCenter().GetXf();
	const float sphereExtCenterY = exsphere.GetCenter().GetYf();
	const float sphereExtRadius = exsphere.GetRadiusf();

	// Loop over the rectangular part of the grid.
	int numFound = 0;
	for(int cellY = cellY1; cellY <= cellY2; cellY++)
	{
		for(int cellX = cellX1; cellX <= cellX2; cellX++)
		{
			// Get the cell index, and read the element in the array.
			const int cellIndex = m_AccelGrid.CalcCellIndex(cellX, cellY);
			const u16 gridElement = m_AccelGridNodeIndices[cellIndex];

			// For the start point index, we need to look at the cell before this one.
			int startIndex = 0;
			if(cellIndex > 0)
			{
				startIndex = (m_AccelGridNodeIndices[cellIndex - 1] & ~kMaskContainsExtendedRange);
			}

			// The end point index we get from the current element, just have to mask off the extended range bit.
			const int endIndex = (gridElement & ~kMaskContainsExtendedRange);

			// Some cells may actually not contain any points. In that case, there is no need to
			// even test them against our sphere.
			if(startIndex == endIndex)
			{
				continue;
			}

			// Test the grid cell against the sphere, either the normal one or the
			// extended range one.
			bool inCircle;
			if(gridElement & kMaskContainsExtendedRange)
			{
				inCircle = m_AccelGrid.TestCircle(sphereExtCenterX, sphereExtCenterY, sphereExtRadius, cellX, cellY);
			}
			else
			{
				inCircle = m_AccelGrid.TestCircle(sphereNormalCenterX, sphereNormalCenterY, sphereNormalRadius, cellX, cellY);
			}

			// If this cell overlapped the circle, we need to test the points within that cell.
			if(inCircle)
			{
				if(taskVerifyf(endIndex <= m_Points.GetNumPoints(), "endIndex [%d] is out of range [0..%d]", endIndex, m_Points.GetNumPoints()))
				{
					// Let FindPointsInAreaDirect() look through all the points in this range.
					const int numFoundInCell = FindPointsInAreaDirect(sphere, exsphere, destArray + numFound, maxInDestArray - numFound, startIndex, endIndex);
					numFound += numFoundInCell;

					if(numFound >= maxInDestArray)
					{
						return numFound;
					}
				}
			}
		}
	}

	return numFound;
}

int CScenarioPointRegion::FindClusteredPointsInArea(const spdSphere& sphere, const spdSphere& exsphere, CScenarioPoint** destArray, const int maxInDestArray)
{
	int numFound = 0;
	const int count = m_Clusters.GetCount();
	for (int c=0; c < count; c++)
	{
		const int pcount = m_Clusters[c].GetNumPoints();
		for (int p=0; p < pcount; p++)
		{
			CScenarioPoint& pt = m_Clusters[c].GetPoint(p);

			bool valid = false;
			Vec3V pos = pt.GetPosition();
			if(pt.HasExtendedRange())
			{
				valid = exsphere.ContainsPoint(pos);
			}
			else
			{
				valid = sphere.ContainsPoint(pos);
			}

			if(valid)
			{
				if(numFound < maxInDestArray)
				{
					destArray[numFound] = &pt;
					numFound++;

					if(numFound == maxInDestArray)
					{
						return numFound;
					}
				}
			}
		}
	}
	
	return numFound;
}

void CScenarioPointRegion::ParserPostLoad()
{
#if NAVMESH_EXPORT
	if(CNavMeshDataExporter::IsExportingCollision())
		return;
#endif

	PF_FUNC(CScenarioPointRegion_ParserPostLoad);

	SCENARIOPOINTMGR.InvalidateCachedRegionPtrs();

	// This is padded .psc data, it's pointing to garbage
	rage_placement_new(&m_EntityOverrideTypes) atArray<u32>();

	// We may have loaded from a PSO file, and the calculated members inside
	// the spdGrid2D object may not have been set. Recalculate.
	m_AccelGrid.InitCalculatedMembers();

	CScenarioPointLoadLookUps loadLookUps;
	loadLookUps.Init(m_LookUps);
	m_Points.PostLoad(loadLookUps);
	
	const int count = m_Clusters.GetCount();
	for (int c=0; c < count; c++)
	{
		m_Clusters[c].PostLoad(loadLookUps);
	}

#if __ASSERT
	const int numClusters = m_Clusters.GetCount();
	if(numClusters > 0)
	{
		int invalidCount = 0;
		for (int c = 0; c < numClusters; c++)
		{
			//1 or fewer points means this is no longer a valid cluster
			if (m_Clusters[c].GetNumPoints() <= 1)
			{
				invalidCount++;
			}
		}

		taskAssertf(!invalidCount, "Scenario point region has {%d} invalid clusters, probably due to changes made outside of the scenario editor. This needs to be fixed by saving out the file from the editor.", invalidCount);
	}
#endif	// __ASSERT

	CallOnAddScenarioForPoints();

	m_ChainingGraph.PostLoad(*this);

#if __ASSERT
	const int numGridCells = m_AccelGridNodeIndices.GetCount();
	if(numGridCells > 0)
	{
		// For a properly generated acceleration grid, the index in the last grid cell should
		// match the number of points in the region.
		const int cellIndex = numGridCells - 1;
		const u16 gridElement = m_AccelGridNodeIndices[cellIndex];
		const int endIndex = (gridElement & ~kMaskContainsExtendedRange);
		taskAssertf((u32)endIndex == m_Points.GetNumPoints(), "Scenario point region acceleration grid is mismatched with point data, probably due to changes made outside of the scenario editor. This needs to be fixed by saving out the file from the editor (point count %d/%d).", endIndex, m_Points.GetNumPoints());
	}
#endif	// __ASSERT

	// Set to false and look for the actual values
	m_bContainsBuildings = false;
	m_bContainsObjects = false;

	// After loading, make sure that the m_ScenarioPointsInUse[] arrays
	// are of the right size.
	for(int i = 0; i < m_EntityOverrides.GetCount(); i++)
	{
		CScenarioEntityOverride& override = m_EntityOverrides[i];
		const int numPts = override.m_ScenarioPoints.GetCount();
		rage_placement_new(&override.m_ScenarioPointsInUse) atArray<RegdScenarioPnt>();
		rage_placement_new(&override.m_EntityCurrentlyAttachedTo) RegdEnt();
		if(override.m_ScenarioPointsInUse.GetCount() == 0 && numPts > 0)
		{
			override.m_ScenarioPointsInUse.Reserve(numPts);
			override.m_ScenarioPointsInUse.Resize(numPts);
		}

		// These arrays are supposed to be parallel at this point, if this fails something went wrong with that.
		Assert(override.m_ScenarioPointsInUse.GetCount() == override.m_ScenarioPoints.GetCount());

#if SCENARIO_VERIFY_ENTITY_OVERRIDES
		override.m_VerificationStatus = CScenarioEntityOverride::kVerificationStatusUnknown;
#endif	// SCENARIO_VERIFY_ENTITY_OVERRIDES

		// Check if this override uses a model we didn't process before
		if(m_EntityOverrideTypes.Find(override.m_EntityType.GetHash()) == -1)
		{
			CBaseModelInfo* pModel = CModelInfo::GetBaseModelInfoFromHashKey(override.m_EntityType.GetHash(), NULL);
			if(pModel)
			{
				// Update the content flags based on the type
				m_bContainsBuildings = m_bContainsBuildings || !pModel->GetIsTypeObject();
				m_bContainsObjects = m_bContainsObjects || pModel->GetIsTypeObject();
			}
			else
			{
				// GetBaseModelInfoFromHashKey() can return NULL now if the archetype isn't streamed in.
				// I believe we use these streamable archetypes both for buildings and objects, and we don't
				// seem to have a way to know if it's one or the other, so to be safe we have to consider
				// both possibilities.
				m_bContainsBuildings = m_bContainsObjects = true;
			}

			// Add the model type to the array
			m_EntityOverrideTypes.PushAndGrow(override.m_EntityType.GetHash(), 4);
		}
	}

	// Make sure the flags make sense
	Assertf(m_EntityOverrides.GetCount() == 0 || m_bContainsBuildings || m_bContainsObjects, "ScenarioPointRegion with no buildings or objects - is this region valid?");

	if(GetNumEntityOverrides() > 0)
	{
		CScenarioEntityOverridePostLoadUpdateQueue::GetInstance().AddToQueue(this);
	}

	// If this region has been streamed in before and now comes back in, we may need to re-attach
	// scenario chain users that may still be floating around.
	m_bNeedsToBeCheckedForExistingChainUsers = true;
	CScenarioChainingGraph::ReportLoadedScenarioRegion();

	// This assert can be turned on if we want to enforce the max size:
#if 0 && ( SCENARIO_DEBUG )
	if(CountPointsAndEdgesForBudget() > GetMaxScenarioPointsAndEdgesPerRegion())
	{
		const char* regionName = NULL;
		for(int i = 0; i < SCENARIOPOINTMGR.GetNumRegions(); i++)
		{
			if(SCENARIOPOINTMGR.GetRegion(i) == this)
			{
				regionName = SCENARIOPOINTMGR.GetRegionName(i);
			}
		}
		if(Verifyf(regionName, "Failed to find name of region %p.", (void*)this))
		{
#if __ASSERT
			taskAssertf(0, "Scenario point region %s has too many points and chaining edges: %d, max allowed is %d.",
				regionName, CountPointsAndEdgesForBudget(), GetMaxScenarioPointsAndEdgesPerRegion());
#else
			taskErrorf("Scenario point region %s has too many points and chaining edges: %d, max allowed is %d.",
				regionName, CountPointsAndEdgesForBudget(), GetMaxScenarioPointsAndEdgesPerRegion());
#endif	// __ASSERT
		}
	}
#endif
}

CScenarioEntityOverride* CScenarioPointRegion::FindOverrideForEntity(CEntity& entity, bool bDoDirectSearch)
{
	const int numEntityOverrides = m_EntityOverrides.GetCount();
	if(!numEntityOverrides)
	{
		// If there are no overrides, no need to get the position, model name hash, etc.
		return NULL;
	}

	// Don't bother to search for stuff that doesn't exist
	if((!m_bContainsBuildings && entity.GetIsTypeBuilding()) || (!m_bContainsObjects && entity.GetIsTypeObject()))
	{
		return NULL;
	}

	// If we don't want to do the direct search just skip to the distance search
	if(bDoDirectSearch)
	{
		// TODO: Consider improving this, should be able to accelerate it for example
		// by sorting on the model type, separating out the data we use for search for
		// improved cache efficiency, etc.

		// First, check if this entity is already bound to any of the overrides. If so,
		// we need to use that.
		for(int i = 0; i < numEntityOverrides; i++)
		{
			CScenarioEntityOverride& override = m_EntityOverrides[i];
			if(override.m_EntityCurrentlyAttachedTo == &entity)
			{
#if __ASSERT
				CBaseModelInfo* pModelInfo = entity.GetBaseModelInfo();
				Assertf(pModelInfo, "Entity override search for entity with no model info");
#endif // __ASSERT

				return &override;
			}
		}
	}
#if __ASSERT
	else
	{
		for(int i = 0; i < numEntityOverrides ; i++)
		{
			CScenarioEntityOverride& override = m_EntityOverrides[i];
			if(override.m_EntityCurrentlyAttachedTo)
			{
				Assertf(override.m_EntityCurrentlyAttachedTo != &entity, "Override entity direct link skipped but the link excisted - this is not good for performance");
			}
		}
	}
#endif // __ASSERT

	// Get the model hash of the entity.
	CBaseModelInfo* pModelInfo = entity.GetBaseModelInfo();
	Assertf(pModelInfo, "Entity override search for entity with no model info");
	const u32 modelHash = pModelInfo->GetModelNameHash();

	if(m_EntityOverrideTypes.Find(modelHash) == -1)
	{
		return NULL;
	}

	// Get the position of the entity.
	const Mat34V& entityMatrix = entity.GetTransform().GetMatrix();
	const Vec3V entityPosV = entityMatrix.GetCol3();

	// Use some small distance threshold for the binding.
	ScalarV closestDistSqV(square(0.5f));

	// This is a larger distance threshold, within which we may check if a point is within the bounding box.
	const ScalarV boundingBoxDistSqV(square(2.5f));

	const spdAABB& localBoundingBox = pModelInfo->GetBoundingBox();

	CScenarioEntityOverride* pClosestOverride = NULL;

	// Loop over the overrides.
	for(int i = 0; i < numEntityOverrides; i++)
	{
		CScenarioEntityOverride& override = m_EntityOverrides[i];

		// Check if the model matches.
		if(override.m_EntityType.GetHash() != modelHash)
		{
			continue;
		}

		// Check if the this override is already attached to something else.
		if(override.m_EntityCurrentlyAttachedTo != NULL)
		{
			// Already attached to something else.
			continue;
		}

		// Measure the distance, and check if close enough.
		const Vec3V overridePosV = override.m_EntityPosition;
		const ScalarV distSqV = DistSquared(entityPosV, overridePosV);
		if(IsLessThanAll(distSqV, closestDistSqV))
		{
			// Note: we used to just return here, but now when we have a larger
			// threshold, we should probably keep searching to reduce the risk of picking
			// up an override meant for a nearby prop.
			closestDistSqV = distSqV;
			pClosestOverride = &override;
		}
		// If we haven't already found any matching override, and we are reasonably close...
		else if(!pClosestOverride && IsLessThanAll(distSqV, boundingBoxDistSqV))
		{
			// ... check if this override is within the bounding box of the prop.
			// If so, it's very possible that the artists just adjusted the position or
			// rotation a little bit, and we should probably allow this override to be used.
			const Vec3V localOverridePosV = UnTransformOrtho(entityMatrix, overridePosV);
			if(localBoundingBox.ContainsPoint(localOverridePosV))
			{
				// Remember this override. Note that we intentionally don't set the closest
				// distance here - we wouldn't want any other override that happens to be closer
				// to the prop center to be considered better. However, we do let any override
				// that's closer than the regular threshold take priority, still.
				pClosestOverride = &override;
			}
		}
	}

	return pClosestOverride;
}

///////////////////////////////////////////////////////////////////////////////
//  CScenarioPointRegionEditInterface
///////////////////////////////////////////////////////////////////////////////

#if SCENARIO_DEBUG

// STATIC ---------------------------------------------------------------------

int CScenarioPointRegionEditInterface::GetMaxCarGensPerRegion()
{
	return fwConfigManager::GetInstance().GetSizeOfPool(ATSTRINGHASH("ScenarioCarGensPerRegion", 0x62c56790), CONFIGURED_FROM_FILE);
}

int CScenarioPointRegionEditInterface::GetMaxScenarioPointsAndEdgesPerRegion()
{
	return fwConfigManager::GetInstance().GetSizeOfPool(ATSTRINGHASH("ScenarioPointsAndEdgesPerRegion", 0x5916dc52), CONFIGURED_FROM_FILE);
}

//-----------------------------------------------------------------------------

CScenarioPointRegionEditInterface::CScenarioPointRegionEditInterface(CScenarioPointRegion& region)
: m_Region(&region)
//  please use the Get() functions when working with pointer
{}

CScenarioPointRegionEditInterface::CScenarioPointRegionEditInterface(const CScenarioPointRegion& region)
: m_Region(const_cast<CScenarioPointRegion*>(&region)) //removing constness intentionally ... 
//  please use the Get() functions when working with pointer
{}

int CScenarioPointRegionEditInterface::GetNumPoints() const
{
	return Get().m_Points.GetNumPoints();
}

CScenarioPoint& CScenarioPointRegionEditInterface::GetPoint(const int pointIndex)
{
	return Get().m_Points.GetPoint(pointIndex);
}

const CScenarioPoint& CScenarioPointRegionEditInterface::GetPoint(const int pointIndex) const
{
	return Get().m_Points.GetPoint(pointIndex);
}

int CScenarioPointRegionEditInterface::AddPoint(const CExtensionDefSpawnPoint& pointDef)
{
	int index = -1;
	if (InsertPoint( GetNumPoints(), pointDef ))
	{
		index = GetNumPoints()-1;
	}
	return index;
}

int CScenarioPointRegionEditInterface::AddPoint(CScenarioPoint& point)
{
	int index = -1;
	if (InsertPoint( GetNumPoints(), point ))
	{
		index = GetNumPoints()-1;
	}
	return index;
}

bool CScenarioPointRegionEditInterface::InsertPoint(const int pointIndex, const CExtensionDefSpawnPoint& pointDef)
{
	bool retval = false;
	CScenarioPoint* newPT = CScenarioPointContainer::TranslateIntoPoint(pointDef);
	if (newPT)
	{
		retval = InsertPoint(pointIndex, *newPT);
	}
	return retval;
}

bool CScenarioPointRegionEditInterface::InsertPoint(const int pointIndex, CScenarioPoint& point)
{
	Get().m_Points.InsertPoint(pointIndex, point);
	OnAddPoint(point);
	return true;
}

void CScenarioPointRegionEditInterface::DeletePoint(const int pointIndex)
{
	CScenarioPoint* removed = RemovePoint(pointIndex, true /* clear out chains */);
	if(removed)
	{
		CScenarioPoint::DestroyForEditor(*removed);
	}
}

bool CScenarioPointRegionEditInterface::TransferPoint(const CScenarioPoint& point, CScenarioPointRegionEditInterface& toRegion)
{
	//Note: ENTITY OVERRIDES are currently not transferable ... 
	if (!point.IsEditable() || point.GetEntity())
	{
		return false;
	}
	
	if ( !point.IsChained() && !point.IsInCluster()) // not in chain and not in cluster
	{
		const int p = FindPointsIndex(point);
		Assert(p != -1);
		CScenarioPoint* removed = RemovePoint(p, false /*should have no chains to detach*/);

		// This probably doesn't hold up any longer, since we specifically create
		// a copy of the point in some cases:
		//	Assert(removed == &point);

		toRegion.AddPoint(*removed);
	}
	else 
	{
		//Find the point dependencies ...
		atMap<u32, u32> loggedChains;
		atMap<u32, u32> loggedClusters;
		LogPointDependenciesForTransferRecurse(point, loggedChains, loggedClusters);

		//Clusters
		atMap<u32, u32>::Iterator iterClu = loggedClusters.CreateIterator();
		for (iterClu.Start(); !iterClu.AtEnd(); iterClu.Next())
		{
			const int clusterId = iterClu.GetData();

			//create a new cluster
			const int newClusterId = toRegion.AddNewCluster();

			//add all the points
			while(GetNumClusteredPoints(clusterId))
			{
				CScenarioPoint* removed = RemovePointfromCluster(clusterId, 0, false /* leave the chains alone */);
				toRegion.AddPointToCluster(newClusterId, *removed);
			}

			//NOTE: cluster deletion will happen at end ... 
		}

		//Chains
		atMap<CScenarioPoint*, CScenarioPoint*> loggedPoints;
		atBinaryMap<u32, u32> deleteEdges; //assume that any edge is only in ONE chain ... 
		atMap<u32, u32>::Iterator iterCha = loggedChains.CreateIterator();
		for (iterCha.Start(); !iterCha.AtEnd(); iterCha.Next())
		{
			const int chainId = iterCha.GetData();
			const CScenarioChain& chain = Get().m_ChainingGraph.GetChain(chainId);

			//Gather edges, nodes, and points to be transfered.
			const int eidCount = chain.GetNumEdges();
			for (int eid = 0; eid < eidCount; eid++)
			{
				const int e = chain.GetEdgeId(eid);
				const CScenarioChainingEdge& edge = Get().m_ChainingGraph.GetEdge(e);
				const int fromNodeIdx = edge.GetNodeIndexFrom();
				const int toNodeIdx = edge.GetNodeIndexTo();

				const CScenarioChainingNode& fnode = Get().m_ChainingGraph.GetNode(fromNodeIdx);
				const CScenarioChainingNode& tnode = Get().m_ChainingGraph.GetNode(toNodeIdx);

				// create a new edge in the toRegion.
				const int ne = toRegion.AddChainingEdge(fnode, tnode);
				CScenarioChainingEdge& nedge = toRegion.Get().m_ChainingGraph.GetEdge(ne);
				nedge.CopyEdgeDataFrom(edge);
				deleteEdges.Insert(e, e);

				//Log Points
				//NOTE: points in clusters are taken care of when the clusters are transfered and points
				// that are entity points can not be transfered.
				CScenarioPoint* fromPoint = Get().m_ChainingGraph.GetChainingNodeScenarioPoint(fromNodeIdx);
				if (fromPoint && fromPoint->IsEditable() && !fromPoint->GetEntity() && !fromPoint->IsInCluster())
				{
					if (!loggedPoints.Access(fromPoint))
					{
						loggedPoints.Insert(fromPoint, fromPoint);
					}
				}

				CScenarioPoint* toPoint = Get().m_ChainingGraph.GetChainingNodeScenarioPoint(toNodeIdx);
				if (toPoint && toPoint->IsEditable() && !toPoint->GetEntity() && !toPoint->IsInCluster())
				{
					if (!loggedPoints.Access(toPoint))
					{
						loggedPoints.Insert(toPoint, toPoint);
					}
				}
			}
		}
		deleteEdges.FinishInsertion();

		//Now transfer all the points we still need to transfer
		atMap<CScenarioPoint*, CScenarioPoint*>::Iterator iter = loggedPoints.CreateIterator();
		for (iter.Start(); !iter.AtEnd(); iter.Next())
		{
			const int pointId = FindPointsIndex(*iter.GetData());
			Assert(pointId != -1);
			CScenarioPoint* removed = RemovePoint(pointId, false /*Delete chains happens next*/);
			toRegion.AddPoint(*removed);
		}

		//Delete the edges - go through this backwards so we dont have to adjust ids as we go.
		atArray< atBinaryMap<u32, u32>::DataPair >& array = deleteEdges.GetRawDataArray();
		for (int i = array.GetCount()-1; i >= 0; i--)
		{
			Get().m_ChainingGraph.DeleteEdge(array[i].key);
		}

		//Now update all the region stuff
		//Need to update the region AABB so when we search for points in the chain point mappings we
 		// find the newly added points to the region.
		toRegion.UpdateChainingGraphMappings();
		toRegion.PurgeInvalidClusters(); //this removes invalid clusters
		UpdateChainingGraphMappings();
		PurgeInvalidClusters(); //this removes invalid clusters
 		SCENARIOPOINTMGR.UpdateRegionAABB(toRegion.Get());
 		SCENARIOPOINTMGR.UpdateRegionAABB(Get());
	}

	return true;
}

void CScenarioPointRegionEditInterface::BuildAccelerationGrid()
{
	//NOTE: acceleration grid is only setup to be used for the world points in the CScenarioPointRegion::m_Points container.
	// other things like clusters or entity overrides are not part of this at this time.

	// MAGIC! These control the granularity of the grid, as a trade-off between
	// memory and the number of individual points we have to test against.
	static float s_MinCellDim = 32.0f;
	static int s_MaxNumCells = 1024;

	// Make sure we don't keep any old data around.
	ClearAccelerationGrid();

	const int numPoints = Get().m_Points.GetNumPoints();

	// Don't build a grid if we don't have any points, or if we have so many
	// points that we can't fit the indices in m_AccelGridNodeIndices[].
	if(!numPoints || numPoints >= CScenarioPointRegion::kMaskContainsExtendedRange)
	{
		// Fail an assert if we have too many points, we should probably either split up the region
		// in that case, or improve the data structure to handle it, since the last thing we would
		// want to do is to have to iterate over them all when we cull points...
		taskAssertf(!numPoints, "Too many points for grid, %d.", numPoints);

		return;
	}

	// Find the min and max coordinates of the region.
	Vec3V minBounds(V_FLT_MAX), maxBounds(V_NEG_FLT_MAX);
	for(int i = 0; i < numPoints; i++)
	{
		const Vec3V posV = Get().m_Points.GetPoint(i).GetPosition();
		minBounds = Min(minBounds, posV);
		maxBounds = Max(maxBounds, posV);
	}

	// Find the grid cell dimensions to use, satisfying the condition on the max number of cells
	// and doubling the cell dimensions in each attempt.
	float dim = s_MinCellDim;
	while(1)
	{
		Get().m_AccelGrid.Init(minBounds.GetXf(), maxBounds.GetXf(), dim, minBounds.GetYf(), maxBounds.GetYf(), dim);
		if(Get().m_AccelGrid.GetNumCells() <= s_MaxNumCells)
		{
			break;
		}
		else
		{
			dim *= 2.0f;
		}
	}

	// Check how many cells we have. If we ended up with just a single one somehow,
	// we may as well not have the grid.
	const int numCells = Get().m_AccelGrid.GetNumCells();
	if(numCells == 1)
	{
		ClearAccelerationGrid();
		return;
	}

	taskAssert(numCells <= 0xffff);

	// Create a temporary array, in which each element is the index of the cell in which
	// a point belongs.
	u16* cellIndexPerPoint = Alloca(u16, numPoints);
	for(int i = 0; i < numPoints; i++)
	{
		const Vec3V posV = Get().m_Points.GetPoint(i).GetPosition();
		int cellIndex;
		taskVerifyf(Get().m_AccelGrid.CalcCellIndex(cellIndex, posV.GetXf(), posV.GetYf()), "Got scenario point outside of the grid, this shouldn't happen.");

		taskAssert(cellIndex >= 0 && cellIndex <= 0xffff);
		cellIndexPerPoint[i] = (u16)cellIndex;
	}

	// Reserve space for the array.
	Get().m_AccelGridNodeIndices.Reset();
	Get().m_AccelGridNodeIndices.Reserve(numCells);

	// Deal with one cell at a time, reordering the array of points and filling
	// in the index array.
	int numSortedPoints = 0;
	for(int cellIndex = 0; cellIndex < numCells; cellIndex++)
	{
		bool containsExtendedRange = false;

		// Loop over the remaining points, which haven't yet been assigned to
		// any of the cells we have already looked at.
		for(int i = numSortedPoints; i < numPoints; i++)
		{
			// Check if point i belongs to this cell.
			if(cellIndexPerPoint[i] == cellIndex)
			{
				// Keep track of if we have found any point with extended range, in this cell.
				if(Get().m_Points.GetPoint(i).HasExtendedRange())
				{
					containsExtendedRange = true;
				}

				// If we are not looking at the first unsorted point, do a swap between this point
				// and the first unsorted point.
				if(numSortedPoints != i)
				{
					// Swap the point pointer.
					Get().m_Points.SwapPointOrder(numSortedPoints, i);

					// We also need to swap the cell index now, in the parallel cellIndexPerPoint array.
					const u16 otherPointsCellIndex = cellIndexPerPoint[numSortedPoints];
					cellIndexPerPoint[numSortedPoints] = cellIndexPerPoint[i];
					cellIndexPerPoint[i] = otherPointsCellIndex;
				}

				// Either way, we now have one more point that's been sorted into its proper position.
				numSortedPoints++;
			}
		}

		// Compute the array element we'll store in m_AccelGridNodeIndices.
		// The point index is numSortedPoints, i.e. the index after the last point
		// in this cell. We also mask in an extra bit to keep track of whether this
		// cell contained any extended range points or not.
		taskAssert(numSortedPoints < CScenarioPointRegion::kMaskContainsExtendedRange);
		u16 arrayElement = (u16)numSortedPoints;
		if(containsExtendedRange)
		{
			arrayElement |= CScenarioPointRegion::kMaskContainsExtendedRange;
		}

		Get().m_AccelGridNodeIndices.Append() = arrayElement;
	}

	taskAssert(numSortedPoints == numPoints);
}

void CScenarioPointRegionEditInterface::ClearAccelerationGrid()
{
	Get().m_AccelGrid.Init(0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f);
	Get().m_AccelGridNodeIndices.Reset();
}

CScenarioPoint* CScenarioPointRegionEditInterface::RemovePoint(const int pointIndex, const bool deleteAttachedChainEdges)
{
	CScenarioPoint* removed = Get().m_Points.RemovePoint(pointIndex);
	if (removed)
	{
		OnRemovePoint(*removed, deleteAttachedChainEdges);
	}
	return removed;
}

const CScenarioChainingGraph& CScenarioPointRegionEditInterface::GetChainingGraph()const
{
	return Get().m_ChainingGraph;
}

int CScenarioPointRegionEditInterface::AddChainingEdge(const CScenarioPoint& point1, const CScenarioPoint& point2)
{
	CScenarioChainingNode newNode1;
	newNode1.UpdateFromPoint(point1);

	CScenarioChainingNode newNode2;
	newNode2.UpdateFromPoint(point2);

	return AddChainingEdge(newNode1, newNode2);
}


int CScenarioPointRegionEditInterface::AddChainingEdge(const CScenarioChainingNode& newNode1, const CScenarioChainingNode& newNode2)
{
	CScenarioChainingGraph& graph = Get().m_ChainingGraph;
	const int nodeIndex1 = graph.FindOrAddNode(newNode1);
	const int nodeIndex2 = graph.FindOrAddNode(newNode2);

	// Check if it already exists.
	const int numEdges = graph.GetNumEdges();
	for(int i = 0; i < numEdges; i++)
	{
		const CScenarioChainingEdge& edge = graph.GetEdge(i);
		if(edge.GetNodeIndexFrom() == nodeIndex1 && edge.GetNodeIndexTo() == nodeIndex2)
		{
			return i;
		}
	}

	graph.AddEdge(nodeIndex1, nodeIndex2);
	graph.UpdateNodeToScenarioPointMapping(Get());
	graph.UpdateNodesHaveEdges();
	graph.UpdateNodeToChainIndexMapping();

	return numEdges;
}

void CScenarioPointRegionEditInterface::DeleteChainingEdgeByIndex(int chainingEdgeIndex)
{
	Get().m_ChainingGraph.DeleteEdge(chainingEdgeIndex);
	Get().m_ChainingGraph.UpdateNodesHaveEdges();
}

int CScenarioPointRegionEditInterface::FindClosestChainingEdge(Vec3V_In pos) const
{
	Vec3V mouseScreen, mouseFar;
	CDebugScene::GetMousePointing(RC_VECTOR3(mouseScreen), RC_VECTOR3(mouseFar), false);

	// Nan check - not sure if needed.
	Assert(VEC3V_TO_VECTOR3(mouseScreen) == VEC3V_TO_VECTOR3(mouseScreen));

	static float s_MaxDist = 2.0f;
	static float s_ExtraRange = 1.0f;

	const Vec3V dV = Normalize(Subtract(pos, mouseScreen));
	const Vec3V endPos = AddScaled(pos, dV, ScalarV(s_ExtraRange));

	ScalarV closestDistSquaredV(square(s_MaxDist));

	int closestEdge = -1;
	const CScenarioChainingGraph& graph = Get().m_ChainingGraph;
	const int numEdges = graph.GetNumEdges();
	for(int i = 0; i < numEdges; i++)
	{
		const CScenarioChainingEdge& edge = graph.GetEdge(i);
		const CScenarioChainingNode& node1 = graph.GetNode(edge.GetNodeIndexFrom());
		const CScenarioChainingNode& node2 = graph.GetNode(edge.GetNodeIndexTo());

		const Vec3V edgePos1V = node1.m_Position;
		const Vec3V edgePos2V = node2.m_Position;
		const Vec3V edge1To2V = Subtract(edgePos2V, edgePos1V);

		const Vec3V mousePos1V = mouseScreen;
		//const Vec3V mousePos2V = endPos;
		const Vec3V mouse1To2V = Subtract(endPos, mouseScreen);

		ScalarV edgeTV, mouseTV;
		geomTValues::FindTValuesSegToSeg(edgePos1V, edge1To2V, mousePos1V, mouse1To2V, edgeTV, mouseTV);

		const Vec3V closestOnEdgeV = AddScaled(edgePos1V, edge1To2V, edgeTV);
		const Vec3V closestOnMouseV = AddScaled(mousePos1V, mouse1To2V, mouseTV);

		const ScalarV distSquaredV = DistSquared(closestOnEdgeV, closestOnMouseV);
		if(IsLessThanAll(distSquaredV, closestDistSquaredV))
		{
			closestDistSquaredV = distSquaredV;
			closestEdge = i;
		}
	}

	return closestEdge;
}

void CScenarioPointRegionEditInterface::UpdateChainingGraphMappings()
{
	Get().m_ChainingGraph.UpdateNodeToScenarioPointMapping(Get(), true);
	Get().m_ChainingGraph.UpdateNodesHaveEdges();
	Get().m_ChainingGraph.UpdateNodeToChainIndexMapping();
}

int	CScenarioPointRegionEditInterface::RemoveDuplicateChainEdges()
{
	return Get().m_ChainingGraph.RemoveDuplicateEdges();
}

void CScenarioPointRegionEditInterface::UpdateChainEdge(const int edgeIndex, const CScenarioChainingEdge& toUpdateWith)
{
	Get().m_ChainingGraph.GetEdge(edgeIndex).CopyEdgeDataFrom(toUpdateWith);
}

void CScenarioPointRegionEditInterface::FullUpdateChainEdge(const int edgeIndex, const CScenarioChainingEdge& toUpdateWith)
{
	Get().m_ChainingGraph.GetEdge(edgeIndex).CopyFull(toUpdateWith);
	Get().m_ChainingGraph.UpdateNodesHaveEdges();
}

void CScenarioPointRegionEditInterface::UpdateChainNodeFromPoint(const int nodeIndex, const CScenarioPoint& toUpdateWith)
{
	Get().m_ChainingGraph.GetNode(nodeIndex).UpdateFromPoint(toUpdateWith);
}

void CScenarioPointRegionEditInterface::FlipChainEdgeDirection(const int edgeIndex)
{
	CScenarioChainingEdge& edge = Get().m_ChainingGraph.GetEdge(edgeIndex);

	int from = edge.GetNodeIndexFrom();
	int to = edge.GetNodeIndexTo();
	edge.SetNodeIndexFrom(to);
	edge.SetNodeIndexTo(from);

	Get().m_ChainingGraph.UpdateNodesHaveEdges();
}

void CScenarioPointRegionEditInterface::UpdateChainMaxUsers(const int chainIndex, const int newMaxUsers)
{
	CScenarioChain& chain = Get().m_ChainingGraph.GetChain(chainIndex);
	u8 users = 0;
	Assign(users, newMaxUsers);
	chain.SetMaxUsers(users);
}

void CScenarioPointRegionEditInterface::FindCarGensOnChain(const CScenarioChain& chain, atArray<int>& nodesOut) const
{
	atArray<int> visitedNodes;

	const CScenarioChainingGraph& graph = GetChainingGraph();

	const int numEdges = chain.GetNumEdges();
	for(int j = 0; j < numEdges; j++)
	{
		const int edgeIndex = chain.GetEdgeId(j);
		const CScenarioChainingEdge& e = graph.GetEdge(edgeIndex);

		// Note: we currently only look at the To node of the edge. This conveniently
		// prevents us from finding the chain entry points, which is probably what we
		// want in the context of where this function is used (wouldn't want to eliminate
		// spawning for those when reducing car generators, since that would make the point
		// itself useless).
		const int node2 = e.GetNodeIndexTo();
		const CScenarioPoint* pt2 = graph.GetChainingNodeScenarioPoint(node2);

		// Make sure we have a point which we haven't already looked at.
		if(visitedNodes.Find(node2) < 0 && pt2)
		{
			const int type = pt2->GetScenarioTypeVirtualOrReal();
			if(!SCENARIOINFOMGR.IsVirtualIndex(type))	// Don't really support virtual vehicle scenario types in other places either.
			{
				const CScenarioInfo* pInfo = SCENARIOINFOMGR.GetScenarioInfoByIndex(type);

				if(pInfo && pInfo->GetIsClass<CScenarioVehicleInfo>())
				{
					if(CScenarioManager::ShouldHaveCarGen(*pt2, static_cast<const CScenarioVehicleInfo&>(*pInfo)))
					{
						nodesOut.Grow() = node2;
					}
				}
			}

			visitedNodes.Grow() = node2;
		}
	}
}

int CScenarioPointRegionEditInterface::CountCarGensOnChain(const CScenarioChain& chain) const
{
	// This is a pretty lazy implementation, if this is needed outside of tools we shouldn't
	// have the overhead of building a temporary array just to find the count.
	atArray<int> carGenNodesOnChain;
	FindCarGensOnChain(chain, carGenNodesOnChain);
	return carGenNodesOnChain.GetCount();
}

int CScenarioPointRegionEditInterface::GetNumClusters() const
{
	return Get().GetNumClusters();
}

int CScenarioPointRegionEditInterface::GetNumClusteredPoints(const int clusterId) const
{
	return Get().GetNumClusteredPoints(clusterId);
}

int CScenarioPointRegionEditInterface::GetTotalNumClusteredPoints() const
{
	int cpcount = 0;
	const int ccount = GetNumClusters();
	for (int c = 0; c < ccount; c++)
	{
		cpcount += GetNumClusteredPoints(c);
	}

	return cpcount;
}

CScenarioPoint& CScenarioPointRegionEditInterface::GetClusteredPoint(const int clusterId, const int pointIndex)
{
	Assert(clusterId < GetNumClusters());
	return Get().m_Clusters[clusterId].GetPoint(pointIndex);
}

const CScenarioPoint& CScenarioPointRegionEditInterface::GetClusteredPoint(const int clusterId, const int pointIndex) const
{
	Assert(clusterId < GetNumClusters());
	return Get().m_Clusters[clusterId].GetPoint(pointIndex);
}

void CScenarioPointRegionEditInterface::AddClusterWidgets(const int clusterId, bkGroup* parentGroup)
{
	Assert(clusterId < GetNumClusters());
	SCENARIOCLUSTERING.AddWidgets(Get().m_Clusters[clusterId], parentGroup);
}

int	CScenarioPointRegionEditInterface::FindPointsClusterIndex(const CScenarioPoint& pt) const
{
	const int clusters = Get().m_Clusters.GetCount();
	for(int c = 0; c < clusters; c++)
	{
		const CScenarioPointCluster& cluster = Get().m_Clusters[c];
		const int points = cluster.GetNumPoints();
		for(int p = 0; p < points; p++)
		{
			if (&cluster.GetPoint(p) == &pt)
			{
				return c;
			}
		}
	}

	return CScenarioPointCluster::INVALID_ID;
}

int CScenarioPointRegionEditInterface::AddNewCluster()
{
	//NOTE: in the case that the grow operation will resize the array then we need to remove all the clusters from the active list
	// then add them back because the addresses of the clusters could change which would cause bad memory access issues.
	bool resized = false;
	if (Get().m_Clusters.GetCount()+1 > Get().m_Clusters.GetCapacity())
	{
		//The ms_NoPointDeleteOnDestruct static is set for cases where we dont want to delete the points
		// on destruction of this object. The case where this is used currently is
		// for allowing editing of clusters. If we add a new cluster to a region and the 
		// array of clusters in the region needs to be resized the atArray will delete 
		// the point container that is in the CScenarioPointCluster object which will cause
		// the unexpected delete of the points in that cluster.
		CScenarioPointContainer::ms_NoPointDeleteOnDestruct = true;
		

		const int ccount = GetNumClusters();
		for (int c = 0; c < ccount; c++)
		{
			// If we delete the clusters, the owned points will also get destroyed,
			// and the destructor of CScenarioPoint is unhappy if that's done without
			// first doing OnRemoveScenario(), so we have to go through and call
			// OnRemoveScenario() for each point in the cluster (removing the car generators).
			CScenarioPointCluster& cluster = Get().m_Clusters[c];
			const int numPoints = cluster.GetNumPoints();
			for(int i = 0; i < numPoints; i++)
			{
				CScenarioPoint& pt = cluster.GetPoint(i);
				if(pt.GetOnAddCalled())
				{
					CScenarioManager::OnRemoveScenario(pt);
				}
			}

			SCENARIOCLUSTERING.RemoveCluster(cluster);
		}

		resized = true;
	}
	
	Get().m_Clusters.Grow();

	if (resized)
	{
		CScenarioPointContainer::ms_NoPointDeleteOnDestruct = false;

		//Because of resize we need to add them all back ... 
		const int ccount = GetNumClusters();
		for (int c = 0; c < ccount; c++)
		{
			CScenarioPointCluster& cluster = Get().m_Clusters[c];

			// Presumably we will want the scenario points to have gone
			// through OnAddScenario(), so do so now. We could try to remember
			// exactly which point was added before, but it would be pretty messy.
			const int numPoints = cluster.GetNumPoints();
			for(int i = 0; i < numPoints; i++)
			{
				CScenarioPoint& pt = cluster.GetPoint(i);
				if(!pt.GetOnAddCalled())
				{
					CScenarioManager::OnAddScenario(pt);
				}
			}

			SCENARIOCLUSTERING.AddCluster(cluster);
		}
	}
	else
	{
		SCENARIOCLUSTERING.AddCluster(Get().m_Clusters[Get().m_Clusters.GetCount()-1]);
	}

	const int clusterIndex = Get().m_Clusters.GetCount()-1;

	// The cluster we just added should be empty. If it's not, there is probably a bug
	// in what we do with the m_Clusters[] array - for example, if we delete an element
	// from it, we may explicitly need to clear out the element we copied from, so that
	// it's empty when it gets reused.
	Assert(Get().m_Clusters[clusterIndex].GetNumPoints() == 0);

	//Displayf("Create cluster %d - %d", Get().m_Clusters.GetCount()-1, Get().m_Clusters.GetCount());
	return clusterIndex;
}

void CScenarioPointRegionEditInterface::DeleteCluster(const int clusterId)
{
	Assert(clusterId < GetNumClusters());

	//Move all the points back to being unclustered
	while(GetNumClusteredPoints(clusterId))
	{
		CScenarioPoint* removed = RemovePointfromCluster(clusterId, 0, false /* leave the chains alone */);
		AddPoint(*removed);
	}

	//No longer should be getting updated ... 
	SCENARIOCLUSTERING.RemoveCluster(Get().m_Clusters[clusterId]);



	atArray<CScenarioPointCluster>& clusterArray = Get().m_Clusters;

	// Set up a temporary array of pointers to the CSPClusterFSMWrapper wrappers,
	// parallel to m_Clusters[].
	atArray<CSPClusterFSMWrapper*> wrapperRemapArray;
	wrapperRemapArray.Reserve(clusterArray.GetCount());
	wrapperRemapArray.Resize(clusterArray.GetCount());

	for(int i = 0; i < clusterArray.GetCount(); i++)
	{
		clusterArray[i].CallOnRemoveOnEachScenarioPoint();

		// Populate the wrapper pointer array.
		const int wrapperIndex = SCENARIOCLUSTERING.FindIndex(clusterArray[i]);
		if(wrapperIndex >= 0)
		{
			wrapperRemapArray[i] = SCENARIOCLUSTERING.ms_ActiveClusters[wrapperIndex];
		}
		else
		{
			Assert(i == clusterId);
			wrapperRemapArray[i] = NULL;
		}
	}

	clusterArray.Delete(clusterId);

	// Delete from the wrapper pointer array in the same way as we deleted from the
	// cluster array. This should keep them parallel.
	wrapperRemapArray.Delete(clusterId);

	for(int i = 0; i < clusterArray.GetCount(); i++)
	{
		clusterArray[i].CallOnAddOnEachScenarioPoint();

		// Directly set the pointers to the clusters in the wrappers, so that
		// they point to the same clusters they were associated with before
		// we deleted the element from the array.
		Assert(wrapperRemapArray[i]);
		if(wrapperRemapArray[i])
		{
			wrapperRemapArray[i]->SetMyCluster(&clusterArray[i]);
		}
	}

	// The element that was previously the last element in the array has now been
	// moved, to where the deleted element was. However, the destructor wouldn't
	// have been called in this case, so that element past the end of the array
	// would still have points in it, which could be a problem if we reuse it later.
	// The code below extends the array to include that last element again, clears it out,
	// and shrinks the array again.
	clusterArray.Append().CleanUp();
	clusterArray.Pop();

	//Displayf("Delete cluster %d - %d", clusterId, Get().m_Clusters.GetCount());

	// FF: Added this, after getting a crash on some node index when deleting a cluster
	// where one of the nodes was chained to a point outside of the cluster.
	UpdateChainingGraphMappings();
}

int CScenarioPointRegionEditInterface::AddPointToCluster(const int clusterId, CScenarioPoint& point)
{
	Assert(clusterId < GetNumClusters());

	//if we are adding something that is already in a cluster then make sure the cluster is not the same.
	if (point.IsInCluster())
	{
		int oldCluster = FindPointsClusterIndex(point);
		if (oldCluster != clusterId)
		{
			RemovePointfromCluster(oldCluster, point, false /* leave the chains alone */);
		}
		else
		{
			int cpID = -1;
			const int pcount = GetNumClusteredPoints(oldCluster);
			for(int p =0; p < pcount; p++)
			{
				if (&GetClusteredPoint(oldCluster, p) == &point)
				{
					cpID = p;
					break;
				}
			}

			Assert(cpID != -1);

			//Already in the cluster so ignore the add ... 
			//Displayf("Point 0x%x already in cluster %d - %d", &point, clusterId, Get().m_Clusters[clusterId].GetNumPoints());
			return cpID;
		}
	}
	const int cpID = Get().m_Clusters[clusterId].AddPoint(point);
	OnAddPoint(point);
	//Displayf("Add Point 0x%x to cluster %d - %d", &point, clusterId, Get().m_Clusters[clusterId].GetNumPoints());
	return cpID;
}

void CScenarioPointRegionEditInterface::RemovePointfromCluster(const int clusterId, CScenarioPoint& point, const bool deleteAttachedChainEdges)
{
	Assert(point.IsInCluster());
	Assert(FindPointsClusterIndex(point) == clusterId);
	Assert(clusterId < GetNumClusters());

	const CScenarioPointCluster& cluster = Get().m_Clusters[clusterId];
	const int points = cluster.GetNumPoints();
	int removeIdx = -1;
	for(int p = 0; p < points; p++)
	{
		if (&cluster.GetPoint(p) == &point)
		{
			removeIdx = p;
			break;
		}
	}

	if (removeIdx >= 0)
	{
		ASSERT_ONLY(CScenarioPoint* removed =) RemovePointfromCluster(clusterId, removeIdx, deleteAttachedChainEdges);
		Assertf(removed == &point, "We removed the wrong point ... This could be really bad");
	}
}

CScenarioPoint* CScenarioPointRegionEditInterface::RemovePointfromCluster(const int clusterId, const int pointIndex, const bool deleteAttachedChainEdges)
{
	Assert(clusterId < GetNumClusters());

	// Call OnRemoveScenario() on the point about to be removed. CScenarioPoint::RemovePoint()
	// creates a copy of the point (for some reason), and there could be some issues with
	// the car generator management etc if we don't do this first.
	CScenarioPoint* pointToBeRemoved = &Get().m_Clusters[clusterId].GetPoint(pointIndex);
	if(pointToBeRemoved->GetOnAddCalled())
 	{
 		CScenarioManager::OnRemoveScenario(*pointToBeRemoved);
 	}

	CScenarioPoint* removed = Get().m_Clusters[clusterId].RemovePoint(pointIndex);
	if (removed)
	{
		OnRemovePoint(*removed, deleteAttachedChainEdges);
	}
	//Displayf("Remove Point %d from cluster %d - %d", pointIndex, clusterId, Get().m_Clusters[clusterId].GetNumPoints());
	return removed;
}

void CScenarioPointRegionEditInterface::DeleteClusterPoint(const int clusterId, const int pointIndex)
{
	CScenarioPoint* removed = RemovePointfromCluster(clusterId, pointIndex, true /* clear out chains */);
	if(removed)
	{
		CScenarioPoint::DestroyForEditor(*removed);
	}
}

void CScenarioPointRegionEditInterface::PurgeInvalidClusters()
{
	for (int c = 0; c < Get().m_Clusters.GetCount(); c++)
	{
		//1 or fewer points means this is no longer a valid cluster
		if (Get().m_Clusters[c].GetNumPoints() <= 1)
		{
			//Displayf("Purge cluster %d ", c);
			DeleteCluster(c);
			c--;//step back
		}
	}

	//print cluster usage
// 	Displayf("----------------------------------------------");
// 	for (int c = 0; c < Get().m_Clusters.GetCount(); c++)
// 	{
// 		Displayf("- %d ", c);
// 		for (int p = 0; p < Get().m_Clusters[c].GetNumPoints(); p++)
// 		{
// 			Displayf("-- 0x%x ", &Get().m_Clusters[c].GetPoint(p));
// 		}
// 	}
}

void CScenarioPointRegionEditInterface::TriggerClusterSpawn(const int clusterId)
{
	Assert(clusterId != CScenarioPointCluster::INVALID_ID);
	Assert(clusterId >= 0 && clusterId < Get().m_Clusters.GetCount());
	SCENARIOCLUSTERING.TriggerSpawn(Get().m_Clusters[clusterId]);
}

int CScenarioPointRegionEditInterface::GetNumEntityOverrides() const
{
	return Get().GetNumEntityOverrides();
}

const CScenarioEntityOverride& CScenarioPointRegionEditInterface::GetEntityOverride(const int index) const
{
	Assert(index >= 0 && index < Get().m_EntityOverrides.GetCount());
	return Get().m_EntityOverrides[index];
}

CScenarioEntityOverride& CScenarioPointRegionEditInterface::GetEntityOverride(const int index)
{
	Assert(index >= 0 && index < Get().m_EntityOverrides.GetCount());
	return Get().m_EntityOverrides[index];
}

CScenarioEntityOverride& CScenarioPointRegionEditInterface::FindOrAddOverrideForEntity(CEntity& entity)
{
	// See if an override already exists.
	CScenarioEntityOverride* pExistingOverride = FindOverrideForEntity(entity);
	if(pExistingOverride)
	{
		return *pExistingOverride;
	}

	// Extend the override array and return the new element.
	const int newIndex = Get().m_EntityOverrides.GetCount();
	InsertOverrideForEntity(newIndex, entity);
	return Get().m_EntityOverrides[newIndex];
}


void CScenarioPointRegionEditInterface::InsertOverrideForEntity(const int newOverrideIndex, CEntity& entity)
{
	CScenarioEntityOverride& override = AddBlankEntityOverride(newOverrideIndex);

	// Update the flags
	if(entity.GetIsTypeBuilding())
		Get().m_bContainsBuildings = true;
	else if(entity.GetIsTypeObject())
		Get().m_bContainsObjects = true;

	override.Init(entity);
}

CScenarioEntityOverride& CScenarioPointRegionEditInterface::AddBlankEntityOverride(const int newOverrideIndex)
{
	// Make space and insert the override at the desired position.
	Get().m_EntityOverrides.Grow();
	Get().m_EntityOverrides.Pop();
	return Get().m_EntityOverrides.Insert(newOverrideIndex);
}

int CScenarioPointRegionEditInterface::GetIndexForEntityOverride(const CScenarioEntityOverride& override) const
{
	const int index = ptrdiff_t_to_int(&override - Get().m_EntityOverrides.GetElements());
	Assert(index >= 0 && index < Get().m_EntityOverrides.GetCount());
	return index;
}

void CScenarioPointRegionEditInterface::DeleteEntityOverride(const int index)
{
	Assert(index >= 0 && index < Get().m_EntityOverrides.GetCount());
	Get().m_EntityOverrides.Delete(index);
}

CScenarioEntityOverride* CScenarioPointRegionEditInterface::FindOverrideForEntity(CEntity& entity)
{
	return Get().FindOverrideForEntity(entity);
}

void CScenarioPointRegionEditInterface::UpdateEntityOverrideFromPoint(int overrideIndex, int indexWithinOverride)
{
	CScenarioEntityOverride& override = Get().m_EntityOverrides[overrideIndex];
	const CScenarioPoint* pt = override.m_ScenarioPointsInUse[indexWithinOverride];
	if(pt)
	{
		pt->CopyToDefinition(&override.m_ScenarioPoints[indexWithinOverride]);
	}
}

bool CScenarioPointRegionEditInterface::IsPointInRegion(const CScenarioPoint& checkPoint) const
{
	const int pcount = Get().m_Points.GetNumPoints();
	for (int p=0; p < pcount; p++)
	{
		const CScenarioPoint& point = Get().m_Points.GetPoint(p);
		if (&point == &checkPoint)
		{
			return true;
		}
	}

	const int count = Get().m_Clusters.GetCount();
	for (int c=0; c < count; c++)
	{
		const int pcount = Get().m_Clusters[c].GetNumPoints();
		for (int p=0; p < pcount; p++)
		{
			const CScenarioPoint& point = Get().m_Clusters[c].GetPoint(p);
			if (&point == &checkPoint)
			{
				return true;
			}
		}
	}

	return false;
}

bool CScenarioPointRegionEditInterface::ResetGroupForUsersOf(u8 groupId)
{
	bool resetAny = false;
	const int pcount = Get().m_Points.GetNumPoints();
	for (int p=0; p < pcount; p++)
	{
		CScenarioPoint& point = Get().m_Points.GetPoint(p);
		if (point.GetScenarioGroup() == groupId)
		{
			point.SetScenarioGroup(CScenarioPointManager::kNoGroup);
			resetAny = true;
		}
	}

	const int count = Get().m_Clusters.GetCount();
	for (int c=0; c < count; c++)
	{
		const int pcount = Get().m_Clusters[c].GetNumPoints();
		for (int p=0; p < pcount; p++)
		{
			CScenarioPoint& point = Get().m_Clusters[c].GetPoint(p);
			if (point.GetScenarioGroup() == groupId)
			{
				point.SetScenarioGroup(CScenarioPointManager::kNoGroup);
				resetAny = true;
			}
		}
	}

	return resetAny;
}

bool CScenarioPointRegionEditInterface::ChangeGroup(u8 fromGroupId, u8 toGroupId)
{
	bool changed = false;
	const int pcount = Get().m_Points.GetNumPoints();
	for (int p=0; p < pcount; p++)
	{
		CScenarioPoint& point = Get().m_Points.GetPoint(p);
		if (point.GetScenarioGroup() == fromGroupId)
		{
			point.SetScenarioGroup(toGroupId);
			changed = true;
		}
	}

	const int count = Get().m_Clusters.GetCount();
	for (int c=0; c < count; c++)
	{
		const int pcount = Get().m_Clusters[c].GetNumPoints();
		for (int p=0; p < pcount; p++)
		{
			CScenarioPoint& point = Get().m_Clusters[c].GetPoint(p);
			if (point.GetScenarioGroup() == fromGroupId)
			{
				point.SetScenarioGroup(toGroupId);
				changed = true;
			}
		}
	}

	return changed;
}

bool CScenarioPointRegionEditInterface::AdjustGroupIdForDeletedGroupId(u8 groupId)
{
	bool changed = false;
	const int pcount = Get().m_Points.GetNumPoints();
	for (int p=0; p < pcount; p++)
	{
		CScenarioPoint& point = Get().m_Points.GetPoint(p);
		if (point.GetScenarioGroup() > groupId)
		{
			point.SetScenarioGroup(point.GetScenarioGroup()-1);
			changed = true;
		}
	}

	const int count = Get().m_Clusters.GetCount();
	for (int c=0; c < count; c++)
	{
		const int pcount = Get().m_Clusters[c].GetNumPoints();
		for (int p=0; p < pcount; p++)
		{
			CScenarioPoint& point = Get().m_Clusters[c].GetPoint(p);
			if (point.GetScenarioGroup() > groupId)
			{
				point.SetScenarioGroup(point.GetScenarioGroup()-1);
				changed = true;
			}
		}
	}

	return changed;
}

void CScenarioPointRegionEditInterface::RemapScenarioTypeIds(const atArray<u32>& oldMapping)
{
	const int pcount = Get().m_Points.GetNumPoints();
	for (int p=0; p < pcount; p++)
	{
		CScenarioPoint& point = Get().m_Points.GetPoint(p);
		unsigned remapped = point.GetScenarioTypeVirtualOrReal();
		remapped = SCENARIOINFOMGR.GetScenarioIndex(oldMapping[remapped], true, true);
		point.SetScenarioType(remapped);
	}

	const int count = Get().m_Clusters.GetCount();
	for (int c=0; c < count; c++)
	{
		const int pcount = Get().m_Clusters[c].GetNumPoints();
		for (int p=0; p < pcount; p++)
		{
			CScenarioPoint& point = Get().m_Clusters[c].GetPoint(p);
			unsigned remapped = point.GetScenarioTypeVirtualOrReal();
			remapped = SCENARIOINFOMGR.GetScenarioIndex(oldMapping[remapped], true, true);
			point.SetScenarioType(remapped);
		}
	}
}

bool CScenarioPointRegionEditInterface::DeletePointsInAABB(const spdAABB& aabb)
{
	bool changed = false;

	//First add anything that is in the cluster to the general point list
	// The pass on the general list will properly delete the point.
	const int count = Get().m_Clusters.GetCount();
	for (int c=0; c < count; c++)
	{
		int pcount = Get().m_Clusters[c].GetNumPoints();
		for (int p=0; p < pcount; p++)
		{
			const CScenarioPoint& point = Get().m_Clusters[c].GetPoint(p);
			if (aabb.ContainsPoint(point.GetPosition()))
			{
				CScenarioPoint* removed = RemovePointfromCluster(c, p, false /* leave the chains alone */);
				AddPoint(*removed);
				changed = true;
				p--;//stepback
				pcount--;
			}
		}
	}

	int pcount = Get().m_Points.GetNumPoints();
	for (int p=0; p < pcount; p++)
	{
		CScenarioPoint& point = Get().m_Points.GetPoint(p);
		if (aabb.ContainsPoint(point.GetPosition()))
		{
			DeletePoint(p);
			changed = true;
			p--;//stepback
			pcount--;
		}
	}

	return changed;
}

bool CScenarioPointRegionEditInterface::UpdateInteriorForPoints()
{
	bool changed = false;
	const int pcount = Get().m_Points.GetNumPoints();
	for (int p=0; p < pcount; p++)
	{
		CScenarioPoint& point = Get().m_Points.GetPoint(p);
		if (SCENARIOPOINTMGR.UpdateInteriorForPoint(point))
		{
			changed = true;
		}
	}

	const int count = Get().m_Clusters.GetCount();
	for (int c=0; c < count; c++)
	{
		const int pcount = Get().m_Clusters[c].GetNumPoints();
		for (int p=0; p < pcount; p++)
		{
			CScenarioPoint& point = Get().m_Clusters[c].GetPoint(p);
			if (SCENARIOPOINTMGR.UpdateInteriorForPoint(point))
			{
				changed = true;
			}
		}
	}

	return changed;
}

CScenarioPoint* CScenarioPointRegionEditInterface::FindClosestPoint(Vec3V_In fromPos, const CScenarioDebugClosestFilter& filter, CScenarioPoint* curClosest, float* closestDistInOut)
{
	Assert(closestDistInOut);
	
	CScenarioPoint* found = curClosest;
	const int pcount = Get().m_Points.GetNumPoints();
	for (int p=0; p < pcount; p++)
	{
		CScenarioPoint& point = Get().m_Points.GetPoint(p);
		const Vec3V posV = point.GetPosition();
		const float distanceSq = DistSquared(posV, fromPos).Getf();
		if(distanceSq < (*closestDistInOut))
		{
			if(filter.Match(point))
			{
				(*closestDistInOut) = distanceSq;
				found = &point;
			}
		}
	}

	const int count = Get().m_Clusters.GetCount();
	for (int c=0; c < count; c++)
	{
		const int pcount = Get().m_Clusters[c].GetNumPoints();
		for (int p=0; p < pcount; p++)
		{
			CScenarioPoint& point = Get().m_Clusters[c].GetPoint(p);
			const Vec3V posV = point.GetPosition();
			const float distanceSq = DistSquared(posV, fromPos).Getf();
			if(distanceSq < (*closestDistInOut))
			{
				if(filter.Match(point))
				{
					(*closestDistInOut) = distanceSq;
					found = &point;
				}
			}
		}
	}

	return found;
}

int CScenarioPointRegionEditInterface::GetVersionNumber() const 
{
	return Get().m_VersionNumber;
}

bool CScenarioPointRegionEditInterface::Save(const char* filename)
{
	ASSET.CreateLeadingPath(filename);
	fiStream* pFile = ASSET.Create(filename, "");
	if(Verifyf(pFile, "Unable to save scenario point file '%s'.", filename))
	{
		CScenarioPointRegion saveObject;
		CScenarioPointRegionEditInterface saveFace(saveObject);
		// We call this manually now, instead of as a callback through the parser, because
		// parXmlWriterVisitor probably doesn't currently support pre-save callbacks.
		PrepSaveObject(saveFace);

		// Save using parXmlWriterVisitor rather than PARSER.SaveObject(), because
		// the latter uses a lot of memory to store the whole parser tree before saving,
		// which could make us run out of memory and lose data.
		parXmlWriterVisitor v1(pFile);
		v1.Visit(saveObject);
		pFile->Close();
		pFile = NULL;
		Displayf("Saved scenario points to '%s'.", filename);
		
		return true;
	}

	return false;
}

static void sGrowAABBForScenarioPoint(const CScenarioPoint& pt, spdAABB& boxInOut, bool& firstInOut)
{
	// MAGIC! Make sure we have at least 80 m of extra horizontal padding around high priority
	// scenario points, so the region is streamed in at a good distance and we hopefully don't
	// have to do the high priority spawn on screen.
	static float s_HighPriDistXY = 80.0f;
	static float s_HighPriDistZ = 0.0f;

	// For clusters, make sure we have some extra space, since we tend to have to try spawning
	// at a longer range.
	static float s_ClusterExtraDistXY = 60.0f;

	Vec3V posV = pt.GetPosition();

	Vec3V boxMinV = posV;
	Vec3V boxMaxV = posV;

	if(pt.IsHighPriority())
	{
		Vec3V offsV(s_HighPriDistXY, s_HighPriDistXY, s_HighPriDistZ);
		boxMinV = Subtract(boxMinV, offsV);
		boxMaxV = Add(boxMaxV, offsV);
	}

	if(pt.IsInCluster())
	{
		Vec3V offsV(s_ClusterExtraDistXY, s_ClusterExtraDistXY, 0.0f);
		boxMinV = Subtract(boxMinV, offsV);
		boxMaxV = Add(boxMaxV, offsV);
	}

	spdAABB box(boxMinV, boxMaxV);

	if (firstInOut)
	{
		boxInOut = box;
		firstInOut = false;
	}
	else
	{
		boxInOut.GrowAABB(box);
	}
}

spdAABB CScenarioPointRegionEditInterface::CalculateAABB() const
{
	spdAABB output;
	bool first = true;

	// World Points
	for(int i = 0; i < Get().m_Points.GetNumPoints(); i++)
	{
		const CScenarioPoint& point = Get().m_Points.GetPoint(i);
		if (point.IsEditable())
		{
			sGrowAABBForScenarioPoint(point, output, first);
		}
	}

	//Cluster Points
	const int count = Get().m_Clusters.GetCount();
	for (int c=0; c < count; c++)
	{
		const int pcount = Get().m_Clusters[c].GetNumPoints();
		for (int p=0; p < pcount; p++)
		{
			const CScenarioPoint& point = Get().m_Clusters[c].GetPoint(p);
			if (point.IsEditable())
			{
				sGrowAABBForScenarioPoint(point, output, first);
			}
		}
	}

	// Make sure the box is large enough to include all the props we are overriding.
	// Note: we currently don't look at the individual points here, hopefully not necessary.
	for(int i = 0; i < Get().m_EntityOverrides.GetCount(); i++)
	{
		const CScenarioEntityOverride& override = Get().m_EntityOverrides[i];
		const Vec3V posV = override.m_EntityPosition;
		if (first)
		{
			output.Set(posV,posV);
			first = false;
		}
		else
		{
			output.GrowPoint(posV);
		}
	}

	// Loop over the nodes too. Most nodes will be points in this region, which we would
	// have already included, but it is possible to chain to entity-attached points (even
	// without an override), and if that's done, we probably want to make sure that the
	// region gets streamed in accordingly.
	for(int i = 0; i < Get().m_ChainingGraph.GetNumNodes(); i++)
	{
		const CScenarioChainingNode& node = Get().m_ChainingGraph.GetNode(i);
		if (first)
		{
			output.Set(node.m_Position,node.m_Position);
			first = false;
		}
		else
		{
			output.GrowPoint(node.m_Position);
		}
	}

	return output;
}

int CScenarioPointRegionEditInterface::CountCarGensForBudget() const
{
	int numCarGens = 0;

	const int numPts = Get().GetNumPoints();
	for(int i = 0; i < numPts; i++)
	{
		const CScenarioPoint& pt = Get().m_Points.GetPoint(i);
		const int type = pt.GetScenarioTypeVirtualOrReal();
		if(!SCENARIOINFOMGR.IsVirtualIndex(type))
		{
			const CScenarioInfo* pInfo = SCENARIOINFOMGR.GetScenarioInfoByIndex(type);
			if(pInfo && pInfo->GetIsClass<CScenarioVehicleInfo>())
			{
				if(CScenarioManager::ShouldHaveCarGen(pt, static_cast<const CScenarioVehicleInfo&>(*pInfo)))
				{
					numCarGens++;
				}
			}
		}
	}
	return numCarGens;
}

int CScenarioPointRegionEditInterface::CountPointsAndEdgesForBudget() const
{
	int numPtsAndEdges = 0;

	// World Points
	numPtsAndEdges += Get().m_Points.GetNumPoints();
	
	// Cluster Points
	const int count = Get().m_Clusters.GetCount();
	for (int c=0; c < count; c++)
	{
		numPtsAndEdges += Get().m_Clusters[c].GetNumPoints();
	}

	// Entity Overrides
	const int numOverrides = Get().GetNumEntityOverrides();
	for(int i = 0; i < numOverrides; i++)
	{
		const CScenarioEntityOverride& override = GetEntityOverride(i);

		// Count the number of points in the override, or if it's empty,
		// count it as 1, so we get some sort of cost also if we're just
		// disabling art-attached points.
		numPtsAndEdges += Max(override.m_ScenarioPoints.GetCount(), 1);
	}

	numPtsAndEdges += GetChainingGraph().GetNumEdges();

	return numPtsAndEdges;
}

void CScenarioPointRegionEditInterface::OnAddPoint(CScenarioPoint& addedPoint)
{
	// This spatial data structure won't really be valid after we add the point,
	// so we clear it out.
	ClearAccelerationGrid();

	CScenarioManager::OnAddScenario(addedPoint);
}

void CScenarioPointRegionEditInterface::OnRemovePoint(CScenarioPoint& removedPoint, const bool deleteAttachedChainEdges)
{
	// The spatial data structure won't be valid after we remove a point.
	ClearAccelerationGrid();

	if(deleteAttachedChainEdges && removedPoint.IsChained())
 	{
 		Get().m_ChainingGraph.DeleteAnyNodesAndEdgesForPoint(removedPoint);
 	}
 
 	if (removedPoint.GetOnAddCalled())
 	{
 		CScenarioManager::OnRemoveScenario(removedPoint);
 	}
}

void CScenarioPointRegionEditInterface::LogPointDependenciesForTransferRecurse(const CScenarioPoint& point, atMap<u32, u32>& uniqueChains, atMap<u32, u32>& uniqueClusters) const
{
	if (point.IsInCluster())
	{
		const int clusterId = FindPointsClusterIndex(point);
		Assert(clusterId != CScenarioPointCluster::INVALID_ID);

		if (!uniqueClusters.Access(clusterId))
		{
			uniqueClusters.Insert(clusterId, clusterId);

			//Go through all the points in the cluster and make sure they dont have dependencies as well.
			const int cpcount = GetNumClusteredPoints(clusterId);
			for (int cp=0; cp < cpcount; cp++)
			{
				LogPointDependenciesForTransferRecurse(GetClusteredPoint(clusterId, cp), uniqueChains, uniqueClusters);
			}
		}
	}

	if (point.IsChained())
	{
		const int nodeIdx = Get().m_ChainingGraph.FindChainingNodeIndexForScenarioPoint(point);
		Assert(nodeIdx != -1);
		const int chainIdx = Get().m_ChainingGraph.GetNodesChainIndex(nodeIdx);
		Assert(chainIdx != -1);

		if (!uniqueChains.Access(chainIdx))
		{
			uniqueChains.Insert(chainIdx, chainIdx);

			//Go through all the points in the chain and make sure they dont have dependencies as well.
			const CScenarioChain& chain = Get().m_ChainingGraph.GetChain(chainIdx);
			const int eidCount = chain.GetNumEdges();
			for (int eid = 0; eid < eidCount; eid++)
			{
				const int e = chain.GetEdgeId(eid);
				const CScenarioChainingEdge& edge = Get().m_ChainingGraph.GetEdge(e);
				const int fromNodeIdx = edge.GetNodeIndexFrom();
				const int toNodeIdx = edge.GetNodeIndexTo();

				CScenarioPoint* fromPoint = Get().m_ChainingGraph.GetChainingNodeScenarioPoint(fromNodeIdx);
				if (fromPoint) //possibly not present because of being a prop point whos prop is not streamed in ...
				{
					LogPointDependenciesForTransferRecurse(*fromPoint, uniqueChains, uniqueClusters);
				}				

				CScenarioPoint* toPoint = Get().m_ChainingGraph.GetChainingNodeScenarioPoint(toNodeIdx);
				if (toPoint) //possibly not present because of being a prop point whos prop is not streamed in ...
				{
					LogPointDependenciesForTransferRecurse(*toPoint, uniqueChains, uniqueClusters);
				}
			}
		}
	}
}

int CScenarioPointRegionEditInterface::FindPointsIndex(const CScenarioPoint& point) const
{
	const int pcount = GetNumPoints();
	for (int p=0; p < pcount; p++)
	{
		if (&GetPoint(p) == &point)
		{
			return p;
		}
	}

	return -1;
}

void CScenarioPointRegionEditInterface::PrepSaveObject(CScenarioPointRegionEditInterface& saveObject)
{
	// Before saving, rebuild the spatial data structure. Note that this needs
	// to happen before we do the stuff below, because we don't want to build
	// the saveObject until we know the proper order of the points.
	BuildAccelerationGrid();

	saveObject.Get().m_LookUps.ResetAndInit();
	
	//////////////////////////////////////////////////////////////////////////
	// World Points
	saveObject.Get().m_Points.PrepSaveObject(Get().m_Points, saveObject.Get().m_LookUps);

	//////////////////////////////////////////////////////////////////////////
	// Cluster Points
	const int precount = Get().m_Clusters.GetCount();
	PurgeInvalidClusters();// just in case we have invalid ones.
	const int count = Get().m_Clusters.GetCount();
	Displayf("PreSave removed %d invalid clusters", (precount - count));

	saveObject.Get().m_Clusters.Reserve(count);
	for (int c=0; c < count; c++)
	{
		saveObject.Get().m_Clusters.Append().PrepSaveObject(Get().m_Clusters[c], saveObject.Get().m_LookUps);
	}

	//////////////////////////////////////////////////////////////////////////
	// Update the CExtensionDefSpawnPoints from the CScenarioPoints, for all entity overrides.
	// This may not be strictly necessary since unlike for the non-attached points, we keep
	// the CExtensionDefSpawnPoints objects around and maintain them while using the editor.
	const int overrideCount = Get().m_EntityOverrides.GetCount();
	saveObject.Get().m_EntityOverrides.Reserve(overrideCount);
	for(int i = 0; i < overrideCount; i++)
	{
		//Update all the points to the latest settings ... 
		CScenarioEntityOverride& override = Get().m_EntityOverrides[i];
		for(int j = 0; j < override.m_ScenarioPointsInUse.GetCount(); j++)
		{
			UpdateEntityOverrideFromPoint(i, j);
		}

		//Put the data into the save object ... 
		CScenarioEntityOverride& overrideSO = saveObject.Get().m_EntityOverrides.Append();
		overrideSO.PrepSaveObject(override);
	}

	//////////////////////////////////////////////////////////////////////////
	// Chains 
	Get().m_ChainingGraph.PreSave(); //Used to remove any dup or loose edges/nodes ... 
	saveObject.Get().m_ChainingGraph.PrepSaveObject(Get().m_ChainingGraph);

	//////////////////////////////////////////////////////////////////////////
	// Accel grid
	saveObject.Get().m_AccelGrid = Get().m_AccelGrid;
	saveObject.Get().m_AccelGridNodeIndices = Get().m_AccelGridNodeIndices;

	//////////////////////////////////////////////////////////////////////////
	// Increment the version number, right before we save.
	Get().m_VersionNumber++;
	saveObject.Get().m_VersionNumber = Get().m_VersionNumber; //these are the same ... 
}

CScenarioPointRegion& CScenarioPointRegionEditInterface::Get()
{
	Assert(m_Region);
	return *m_Region;
}

const CScenarioPointRegion& CScenarioPointRegionEditInterface::Get() const
{
	Assert(m_Region);
	return *m_Region;
}

#endif // SCENARIO_DEBUG

// End of file 'task/Scenario/ScenarioPointRegion.cpp'
