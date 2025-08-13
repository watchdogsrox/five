//
// task/Scenario/ScenarioEntityOverride.h
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef TASK_SCENARIO_SCENARIOPOINTCONTAINER_H
#define TASK_SCENARIO_SCENARIOPOINTCONTAINER_H

// Rage headers

// Framework headers

// Game headers
#include "scene/loader/MapData_Extensions.h"

// Forwards
class CScenarioPoint;
class CScenarioPointLoadLookUps;

// NOTE: this is a structure used for looking up string names to index values in the scenario points to facilitate
// making the scenario points in-place loadable and the smaller CScenarioPoint structure size.
class CScenarioPointLookUps
{
public:

#if SCENARIO_DEBUG
	void ResetAndInit();
	void AppendFromPoint(CScenarioPoint& point);
#endif // SCENARIO_DEBUG

private:
	friend class CScenarioPointLoadLookUps; //for access to arrays to create the mappings.

	atArray<atHashWithStringBank>		m_TypeNames;
	atArray<atHashWithStringBank>		m_PedModelSetNames;
	atArray<atHashWithStringBank>		m_VehicleModelSetNames;
	atArray<atHashWithStringBank>		m_GroupNames;
	atArray<atHashWithStringBank>		m_InteriorNames;
	atArray<atHashWithStringBank>		m_RequiredIMapNames;

	PAR_SIMPLE_PARSABLE;
};

//NOTE: this structure is used as a temp look up location to store game indices for all the data in the CScenarioPointLookUps struct loaded from PSO data
class CScenarioPointLoadLookUps
{
public:
	void Init(const CScenarioPointLookUps& lookups);
	void RemapPointIndices(CScenarioPoint& point) const;

private:
	atArray<unsigned> m_TypeIds;
	atArray<unsigned> m_PedModelSetIds;
	atArray<unsigned> m_VehicleModelSetIds;
	atArray<unsigned> m_GroupIds;
	atArray<unsigned> m_InteriorIds;
	atArray<unsigned> m_RequiredIMapIds;
};

class CScenarioPointContainer
{	
public:

#if SCENARIO_DEBUG
	static CScenarioPoint* TranslateIntoPoint(const CExtensionDefSpawnPoint& def);
#endif

	static void CopyDefIntoPoint(const CExtensionDefSpawnPoint& def, CScenarioPoint& scenarioPoint);

	static CScenarioPoint* GetNewScenarioPointFromPool();
#if SCENARIO_DEBUG
	static CScenarioPoint* GetNewScenarioPointForEditor();
	static CScenarioPoint* DuplicatePoint(const CScenarioPoint& toBeLike);
#endif

	CScenarioPointContainer(){}
	~CScenarioPointContainer();

	u32 GetNumPoints() const;
	const CScenarioPoint& GetPoint(const u32 index) const;
	CScenarioPoint& GetPoint(const u32 index);

	void PostLoad(const CScenarioPointLoadLookUps& lookups);
	void CleanUp();
	static void PostPsoPlace(void* data);

#if SCENARIO_DEBUG
	void SwapPointOrder(const u32 fromIndex, const u32 toIndex);
	void PrepSaveObject(const CScenarioPointContainer& prepFrom, CScenarioPointLookUps& out_Lookups);
	// Add a new point to the spawn point array, return index of the new point
	int AddPoint(CScenarioPoint& point);
	void InsertPoint(const u32 index, CScenarioPoint& point);
	CScenarioPoint* RemovePoint(const u32 index);

	void CallOnAddOnEachScenarioPoint();
	int CallOnRemoveOnEachScenarioPoint();

	//The ms_NoPointDeleteOnDestruct static is set for cases where we dont want to delete the points
	// on destruction of this object. The case where this is used currently is
	// for allowing editing of clusters. If we add a new cluster to a region and the 
	// array of clusters in the region needs to be resized the atArray will delete 
	// the point container that is in the CScenarioPointCluster object which will cause
	// the unexpected delete of the points in that cluster.
	static bool ms_NoPointDeleteOnDestruct;
#endif

#if !SCENARIO_DEBUG //NOTE: you should not be copying these things in any build except __BANK builds when doing editing ... 
private:
#endif
	CScenarioPointContainer(const CScenarioPointContainer& tobe);
	CScenarioPointContainer& operator=(const CScenarioPointContainer& tobe);

private:

#if SCENARIO_DEBUG
	bool IsStaticPoint(const CScenarioPoint& point) const;
	void DeleteNonStatic();
#endif

	//////////////////////////////////////////////////////////////////////////
	// Pargen data
	
	// PURPOSE: Only used for loading/saving of points.
	//			for saving CScenarioPoints are translated into CExtensionDefSpawnPoints
	//			for loading CExtensionDefSpawnPoints are translated into CScenarioPoints
	atArray<CExtensionDefSpawnPoint>	m_LoadSavePoints; //legacy ... 
	atArray<CScenarioPoint>		m_MyPoints;
	//////////////////////////////////////////////////////////////////////////

	atArray<CScenarioPoint*>	m_EditingPoints;

	PAR_SIMPLE_PARSABLE;
};

#endif // TASK_SCENARIO_SCENARIOPOINTCONTAINER_H