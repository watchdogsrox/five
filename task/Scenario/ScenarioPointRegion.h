//
// task/Scenario/ScenarioPointRegion.h
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef TASK_SCENARIO_SCENARIOPOINTREGION_H
#define TASK_SCENARIO_SCENARIOPOINTREGION_H

//Rage Headers
#include "atl/array.h"
#include "atl/hashstring.h"
#include "parser/macros.h"
#include "spatialdata/grid2d.h"

//Game Headers
#include "task/Scenario/ScenarioChaining.h"
#include "task/Scenario/ScenarioClustering.h"
#include "task/Scenario/ScenarioEditor.h"
#include "task/Scenario/ScenarioEntityOverride.h"
#include "task/Scenario/ScenarioPointContainer.h"
#include "task/Scenario/ScenarioPointGroup.h"

// Forward definitions
class CScenarioPoint;
#if __BANK
class CScenarioDebugClosestFilter;
#endif

//NOTE: this is the RUNTIME non debug version of the interface if you want to use functionality that is
// __BANK only then please create an CScenarioPointRegionEditInterface object passing in the region pointer as
// the construction parameter to get that interface. IE CScenarioPointRegionEditInterface bankRegion(*regionPtr);
//
// The thinking in doing this is to simplify the interface that the game can see when trying to perform
// operations in a non __BANK build since all operations need to go through the region interface so 
// points are handled correctly when performing actions.
class CScenarioPointRegion
{
public:
	CScenarioPointRegion();
	~CScenarioPointRegion();


	// Points -------------------------------------------
	// PURPOSE:	Find all points within an area defined by a sphere. Actually, there are two spheres,
	//			one for normal points and for extended range points. Acceleration data will be used
	//			if it exists.
	int FindPointsInArea(const spdSphere& sphere, const spdSphere& exsphere, CScenarioPoint** destArray, const int maxInDestArray, bool includePointsInClusters);
	void CallOnRemoveScenarioForPoints();
	void CallOnAddScenarioForPoints();
	void FindPointIndex(const CScenarioPoint& pt, int& pointIndex_Out) const;
	CScenarioPoint& GetPoint(const u32 pointIndex);
	u32 GetNumPoints() const;

	// Clusters -------------------------------------------
	void FindClusteredPointIndex(const CScenarioPoint& pt, int& clusterId_Out, int& pointIndex_Out) const;
	int GetNumClusters() const;
	int GetNumClusteredPoints(const int clusterId) const;
	CScenarioPoint& GetClusteredPoint(const u32 clusterId, const u32 pointIndex);
	const CScenarioPointCluster& GetCluster(int clusterIndex) const { return m_Clusters[clusterIndex]; }

	// Chaining -------------------------------------------
	bool IsChainedWithIncomingEdges(const CScenarioPoint& pt) const;
	bool RegisterPointChainUser(const CScenarioPoint& pt, CScenarioPointChainUseInfo* userInOut, bool allowOverflow = false);
	void RegisterPointChainUserForSpecificChain(CScenarioPointChainUseInfo& user, int chainIndex, int nodeIndex, bool allowOverflow = false);
	void RemovePointChainUser(CScenarioPointChainUseInfo& user);
	void UpdateChainedNodeToScenarioMappingForAddedPoint(CScenarioPoint& pt);
	void UpdateNodeToScenarioPointMapping(bool force) { m_ChainingGraph.UpdateNodeToScenarioPointMapping(*this, force); }
	int	 FindChainedScenarioPointsFromScenarioPoint(const CScenarioPoint& pt, CScenarioChainSearchResults* results, const int maxInDestArray) const;
	const atArray<CScenarioPointChainUseInfo*>& GetChainUserArrayContainingUser(CScenarioPointChainUseInfo& chainUser);
	const CScenarioChainingGraph& GetChainingGraph() const { return m_ChainingGraph; }
	void AddChainUser(CScenarioPointChainUseInfo& user) { m_ChainingGraph.AddChainUser(&user); }
	bool TryToMakeSpaceForChainUser(int chainIndex);

	// Entity Override -------------------------------------------
	// PURPOSE:	If this entity has any scenario overrides, apply them.
	// RETURNS:	True if there was an override.
	bool ApplyOverridesForEntity(CEntity& entity, bool removeExistingPoints, bool bDoDirectSearch = true);
	int  GetNumEntityOverrides() const;
	
	void RemoveFromStore();

	bool ContainsBuildings() const { return m_bContainsBuildings; }
	bool ContainsObjects() const { return m_bContainsObjects; }

	void SetNeedsToBeCheckedForExistingChainUsers(bool b) { m_bNeedsToBeCheckedForExistingChainUsers = b; }
	bool GetNeedsToBeCheckedForExistingChainUsers() const { return m_bNeedsToBeCheckedForExistingChainUsers; }

private:
#if __BANK
	friend class CScenarioPointRegionEditInterface; //For bank only interface access ... too keep the region interface a bit more clean.
#endif

	// General -------------------------------------------
	void ParserPostLoad();

	// Points -------------------------------------------
	// PURPOSE:	Like FindPointsInArea(), but operating directly on a range of points in the array. Mostly used
	//			internally by FindPointsInArea().
	int FindPointsInAreaDirect(const spdSphere& sphere, const spdSphere& exsphere, CScenarioPoint** destArray, const int maxInDestArray, const int startIndex, const int endIndex);
	int FindPointsInAreaGrid(const spdSphere& sphere, const spdSphere& exsphere, CScenarioPoint** destArray, const int maxInDestArray);
	int FindClusteredPointsInArea(const spdSphere& sphere, const spdSphere& exsphere, CScenarioPoint** destArray, const int maxInDestArray);

	// Entity Override -------------------------------------------
	CScenarioEntityOverride* FindOverrideForEntity(CEntity& entity, bool bDoDirectSearch = true);

	// Data -------------------------------------------
	// PURPOSE:	Version number that gets incremented each time the file gets saved, used
	//			for preventing accidentally overwriting data.
	// NOTES:	Not the version of the file format itself.
	int m_VersionNumber;

	// PURPOSE:	Container for all the points to be managed by this region.
	CScenarioPointContainer m_Points;

	atArray<u32>						m_EntityOverrideTypes;

	// PURPOSE:	Array of all entity scenario overrides stored in this region.
	atArray<CScenarioEntityOverride>	m_EntityOverrides;

	// PURPOSE:	Indicate if the override array contains buildings.
	bool								m_bContainsBuildings;

	// PURPOSE:	Indicate if the override array contains objects.
	bool								m_bContainsObjects;

	// PURPOSE:	If true, this region just streamed in and may need to be re-attached
	//			to scenario chain users from last time it was loaded.
	bool								m_bNeedsToBeCheckedForExistingChainUsers;

	// TODO: sort out how this should exist in different builds, etc.
	CScenarioChainingGraph			m_ChainingGraph;
	
	// PURPOSE:	Info about a grid we use as a spatial data structure for accelerating
	//			lookups from coordinates.
	// NOTES:	This member just contains grid dimension data, etc.
	spdGrid2D						m_AccelGrid;

	// PURPOSE:	Elements of the acceleration grid (m_AccelGrid). Each element is the
	//			index of the point in m_MyPoints that comes right after all points in
	//			this cell, i.e. the points in cell k range from m_AccelGridNodeIndices[k-1]
	//			to m_AccelGridNodeIndices[k]-1. Also, the upper bit (kMaskContainsExtendedRange)
	//			is masked in for cells that contain extended range scenario points.
	atArray<u16>					m_AccelGridNodeIndices;

	// PURPOSE:	Array of all scenario clusters
	atArray<CScenarioPointCluster>	m_Clusters;
	
	// PURPOSE: used for looking up string names to index values in the scenario points to facilitate
	// making the scenario points in-place loadable and the smaller CScenarioPoint structure size
	CScenarioPointLookUps m_LookUps;

	// PURPOSE:	Bit used in the elements of m_AccelGridNodeIndices to indicate a cell that
	//			contains extended range points.
	static const u16				kMaskContainsExtendedRange = 0x8000;

	PAR_SIMPLE_PARSABLE;
};

//-----------------------------------------------------------------------------

#if __BANK
class CScenarioPointRegionEditInterface
{
public:
	// PURPOSE:	Get the current allowed limit for what CountCarGensForBudget() returns.
	static int GetMaxCarGensPerRegion();

	// PURPOSE:	Get the current allowed limit for what CountPointsAndEdgesForBudget() returns.
	static int GetMaxScenarioPointsAndEdgesPerRegion();

	explicit CScenarioPointRegionEditInterface(CScenarioPointRegion& region);
	explicit CScenarioPointRegionEditInterface(const CScenarioPointRegion& region);

	// Points -------------------------------------------
	int GetNumPoints() const;
	CScenarioPoint& GetPoint(const int pointIndex);
	const CScenarioPoint& GetPoint(const int pointIndex) const;
	int AddPoint(const CExtensionDefSpawnPoint& pointDef);
	int AddPoint(CScenarioPoint& point);
	bool InsertPoint(const int pointIndex, const CExtensionDefSpawnPoint& pointDef);
	bool InsertPoint(const int pointIndex, CScenarioPoint& point);
	void DeletePoint(const int pointIndex);
	CScenarioPoint* RemovePoint(const int pointIndex, const bool deleteAttachedChainEdges);
	bool TransferPoint(const CScenarioPoint& selPoint, CScenarioPointRegionEditInterface& toRegion);
	void BuildAccelerationGrid();
	void ClearAccelerationGrid();

	// Chaining -------------------------------------------
	const CScenarioChainingGraph& GetChainingGraph()const;//ONLY const access ... non const should go through interface ... 
	int	AddChainingEdge(const CScenarioPoint& point1, const CScenarioPoint& point2);
	int	AddChainingEdge(const CScenarioChainingNode& newNode1, const CScenarioChainingNode& newNode2);
	void DeleteChainingEdgeByIndex(int chainingEdgeIndex);
	int	FindClosestChainingEdge(Vec3V_In pos) const;
	void UpdateChainingGraphMappings();
	int	RemoveDuplicateChainEdges();
	void UpdateChainEdge(const int edgeIndex, const CScenarioChainingEdge& toUpdateWith);
	void FullUpdateChainEdge(const int edgeIndex, const CScenarioChainingEdge& toUpdateWith);
	void UpdateChainNodeFromPoint(const int nodeIndex, const CScenarioPoint& toUpdateWith);
	void FlipChainEdgeDirection(const int edgeIndex);
	void UpdateChainMaxUsers(const int chainIndex, const int newMaxUsers);
	void FindCarGensOnChain(const CScenarioChain& chain, atArray<int>& nodesOut) const;
	int CountCarGensOnChain(const CScenarioChain& chain) const;

	// Clusters -------------------------------------------
	int GetNumClusters() const;
	int GetNumClusteredPoints(const int clusterId) const;
	int GetTotalNumClusteredPoints() const;
	CScenarioPoint& GetClusteredPoint(const int clusterId, const int pointIndex);
	const CScenarioPoint& GetClusteredPoint(const int clusterId, const int pointIndex) const;
	void AddClusterWidgets(const int clusterId, bkGroup* parentGroup);
	int	FindPointsClusterIndex(const CScenarioPoint& pt) const;
	int AddNewCluster();
	void DeleteCluster(const int clusterId);
	int AddPointToCluster(const int clusterId, CScenarioPoint& point);
	void RemovePointfromCluster(const int clusterId, CScenarioPoint& point, const bool deleteAttachedChainEdges);
	CScenarioPoint* RemovePointfromCluster(const int clusterId, const int pointIndex, const bool deleteAttachedChainEdges);
	void DeleteClusterPoint(const int clusterId, const int pointIndex);
	void PurgeInvalidClusters();
	void TriggerClusterSpawn(const int clusterId);

	// Entity Override -------------------------------------------
	int  GetNumEntityOverrides() const;
	const CScenarioEntityOverride& GetEntityOverride(const int index) const;
	CScenarioEntityOverride& GetEntityOverride(const int index);
	CScenarioEntityOverride& FindOrAddOverrideForEntity(CEntity& entity);
	void InsertOverrideForEntity(const int newOverrideIndex, CEntity& entity);
	CScenarioEntityOverride& AddBlankEntityOverride(const int newOverrideIndex);
	int GetIndexForEntityOverride(const CScenarioEntityOverride& override) const;
	void DeleteEntityOverride(const int index);
	CScenarioEntityOverride* FindOverrideForEntity(CEntity& entity);
	void UpdateEntityOverrideFromPoint(int overrideIndex, int indexWithinOverride);

	// All Points -------------------------------------------
	bool IsPointInRegion(const CScenarioPoint& checkPoint) const;
	bool ResetGroupForUsersOf(u8 groupId);
	bool ChangeGroup(u8 fromGroupId, u8 toGroupId);
	bool AdjustGroupIdForDeletedGroupId(u8 groupId);
	void RemapScenarioTypeIds(const atArray<u32>& oldMapping);
	bool DeletePointsInAABB(const spdAABB& aabb);
	bool UpdateInteriorForPoints();
	CScenarioPoint* FindClosestPoint(Vec3V_In fromPos, const CScenarioDebugClosestFilter& filter, CScenarioPoint* curClosest, float* closestDistInOut);

	// General -------------------------------------------
	int GetVersionNumber() const;
	// Save the region - true if successful
	bool Save(const char* filename);
	// calculate the aabb for this region
	spdAABB CalculateAABB() const;
	// PURPOSE: Count the number of car generators used by this region.
	int CountCarGensForBudget() const;
	// PURPOSE:	Count the number of points and edges in this region, for budgeting purposes.
	int CountPointsAndEdgesForBudget() const;

private:
	
	//No one should just be creating these objects ... 
	//NOTE: intentionally left as a stub constructor so link errors will happen 
	// if an object attempts to be created with this constructor.
	CScenarioPointRegionEditInterface();
	
	void OnAddPoint(CScenarioPoint& addedPoint);
	void OnRemovePoint(CScenarioPoint& removedPoint, const bool deleteAttachedChainEdges);

	void LogPointDependenciesForTransferRecurse(const CScenarioPoint& point, atMap<u32, u32>& uniqueChains, atMap<u32, u32>& uniqueClusters) const;
	int FindPointsIndex(const CScenarioPoint& point) const;

	// Prep the data to be saved out to the meta file
	void PrepSaveObject(CScenarioPointRegionEditInterface& saveObject);

	CScenarioPointRegion& Get();
	const CScenarioPointRegion& Get() const;

	CScenarioPointRegion* m_Region;
};
#endif // __BANK

#endif	// TASK_SCENARIO_SCENARIOPOINTREGION_H

// End of file 'task/Scenario/ScenarioPointRegion.h'
