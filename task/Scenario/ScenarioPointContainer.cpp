//
// task/Scenario/ScenarioPointContainer.cpp
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

//Rage Headers

//Framework Headers
#include "ai/task/taskchannel.h"

//Game Headers
#include "ai/ambient/AmbientModelSetManager.h"
#include "Peds/population_channel.h"
#include "task/Scenario/Info/ScenarioInfoManager.h"
#include "task/Scenario/ScenarioManager.h"
#include "task/Scenario/ScenarioPoint.h"
#include "task/Scenario/ScenarioPointGroup.h"
#include "task/Scenario/ScenarioPointContainer.h"
#include "task/Scenario/ScenarioPointManager.h"


AI_OPTIMISATIONS()

///////////////////////////////////////////////////////////////////////////////
//  CScenarioPointLookUps
///////////////////////////////////////////////////////////////////////////////
atHashWithStringBank none("NONE");

#if SCENARIO_DEBUG
void CScenarioPointLookUps::ResetAndInit()
{
	m_TypeNames.Reset();
	m_PedModelSetNames.Reset();
	m_VehicleModelSetNames.Reset();
	m_GroupNames.Reset();
	m_InteriorNames.Reset();
	m_RequiredIMapNames.Reset();

	m_PedModelSetNames.PushAndGrow(none);
	m_VehicleModelSetNames.PushAndGrow(none);
	m_GroupNames.PushAndGrow(none);
	m_InteriorNames.PushAndGrow(none);
	m_RequiredIMapNames.PushAndGrow(none);
}

void CScenarioPointLookUps::AppendFromPoint(CScenarioPoint& point)
{
	//////////////////////////////////////////////////////////////////////////
	//store off the model set
	{
		//translate to a hash ... 
		if (point.GetModelSetIndex() != CScenarioPointManager::kNoCustomModelSet)
		{
			int lookupId = -1;
			if (CScenarioManager::IsVehicleScenarioType(point.GetScenarioTypeVirtualOrReal()))
			{
				atHashWithStringBank msName = CAmbientModelSetManager::GetInstance().GetModelSet(CAmbientModelSetManager::kVehicleModelSets, point.GetModelSetIndex()).GetHash();
				lookupId = m_VehicleModelSetNames.Find(msName);		
				if (lookupId == -1)
				{
					//Add this as a new type
					lookupId = m_VehicleModelSetNames.GetCount();
					m_VehicleModelSetNames.PushAndGrow(msName);
				}
			}
			else
			{
				atHashWithStringBank msName = CAmbientModelSetManager::GetInstance().GetModelSet(CAmbientModelSetManager::kPedModelSets, point.GetModelSetIndex()).GetHash();
				lookupId = m_PedModelSetNames.Find(msName);		
				if (lookupId == -1)
				{
					//Add this as a new type
					lookupId = m_PedModelSetNames.GetCount();
					m_PedModelSetNames.PushAndGrow(msName);
				}
			}
			Assert(lookupId > 0);

			//set the value again in the scenario point to reference the lookup table
			point.SetModelSetIndex(lookupId);
		}
		else
		{
			//Treat it as a ped model set ... 
			//set the value again in the scenario point to reference the lookup table
			point.SetModelSetIndex(m_PedModelSetNames.Find(none));
		}
	}
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	//store off the group
	if (point.GetScenarioGroup() != CScenarioPointManager::kNoGroup)
	{
		atHashWithStringBank scenarioGroup = SCENARIOPOINTMGR.GetGroupByIndex(point.GetScenarioGroup() - 1).GetNameHashString();

		int lookupId = m_GroupNames.Find(scenarioGroup);
		if (lookupId == -1)
		{
			//Add this as a new type
			lookupId = m_GroupNames.GetCount();
			m_GroupNames.PushAndGrow(scenarioGroup);
		}

		//set the value again in the scenario point to reference the lookup table
		point.SetScenarioGroup(lookupId);

	}
	else
	{
		//set the value again in the scenario point to reference the lookup table
		point.SetScenarioGroup(m_GroupNames.Find(none));
	}
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	//store off the interior
	if (point.GetInterior() != CScenarioPointManager::kNoInterior)
	{
		u32 interiorHash = SCENARIOPOINTMGR.GetInteriorNameHash(point.GetInterior() - 1);

		int lookupId = m_InteriorNames.Find(atHashWithStringBank(interiorHash));
		if (lookupId == -1)
		{
			//Add this as a new type
			lookupId = m_InteriorNames.GetCount();
			m_InteriorNames.PushAndGrow(atHashWithStringBank(interiorHash));
		}

		//set the value again in the scenario point to reference the lookup table
		point.SetInterior(lookupId);
	}
	else
	{
		//set the value again in the scenario point to reference the lookup table
		point.SetInterior(m_InteriorNames.Find(none));
	}
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	//store off the requiredImap
	if (point.GetRequiredIMap() != CScenarioPointManager::kNoRequiredIMap)
	{
		atHashWithStringBank rimHash = SCENARIOPOINTMGR.GetRequiredIMapHash(point.GetRequiredIMap());
		int lookupId = m_RequiredIMapNames.Find(rimHash);
		if (lookupId == -1)
		{
			//Add this as a new type
			lookupId = m_RequiredIMapNames.GetCount();
			m_RequiredIMapNames.PushAndGrow(rimHash);
		}

		//set the value again in the scenario point to reference the lookup table
		point.SetRequiredIMap(lookupId);
	}
	else
	{
		//set the value again in the scenario point to reference the lookup table
		point.SetRequiredIMap(m_RequiredIMapNames.Find(none));
	}
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	//store off the type NOTE: MUST BE DONE LAST!!!!
	{
		u32 infoHash = SCENARIOINFOMGR.GetHashForScenario(point.GetScenarioTypeVirtualOrReal());

		int lookupId = m_TypeNames.Find(atHashWithStringBank(infoHash));
		if (lookupId == -1)
		{
			//Add this as a new type
			lookupId = m_TypeNames.GetCount();
			m_TypeNames.PushAndGrow(atHashWithStringBank(infoHash));
		}

		//set the value again in the scenario point to reference the lookup table
		point.SetScenarioType(lookupId);
	}
	//////////////////////////////////////////////////////////////////////////
}
#endif // SCENARIO_DEBUG

///////////////////////////////////////////////////////////////////////////////
//  CScenarioPointLookUps
///////////////////////////////////////////////////////////////////////////////

void CScenarioPointLoadLookUps::Init(const CScenarioPointLookUps& lookups)
{
	//////////////////////////////////////////////////////////////////////////
	//update the type NOTE: must be done FIRST!!!!
	const int tcount = lookups.m_TypeNames.GetCount();
	m_TypeIds.Reserve(tcount);
	for (int t = 0; t < tcount; t++)
	{
		int index = SCENARIOINFOMGR.GetScenarioIndex(lookups.m_TypeNames[t].GetHash(), true, true);
		if(index == -1)
		{
			Assertf(0, "No Scenario Type named [%s] exists the type will be set to index 0 (%s). Warning: Saving this file again will loose the link to the type.", lookups.m_TypeNames[t].GetCStr(), SCENARIOINFOMGR.GetScenarioInfoByIndex(0)->GetName());
			index = 0;
		}
		Assign(m_TypeIds.Append(), (unsigned)index);
	}
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	//update the ped/vehicle model set
	const int pmscount = lookups.m_PedModelSetNames.GetCount();
	m_PedModelSetIds.Reserve(pmscount);
	for (int ms = 0; ms < pmscount; ms++)
	{
		unsigned index = CScenarioPointManager::kNoCustomModelSet;

		if (lookups.m_PedModelSetNames[ms] != none)
		{
			int foundIndex = CAmbientModelSetManager::GetInstance().GetModelSetIndexFromHash(CAmbientModelSetManager::kPedModelSets, lookups.m_PedModelSetNames[ms]);

			if(foundIndex == -1)
			{
				Assertf(0, "No Ambient Model Set named [%s] exists the type will be set to CScenarioPointManager::kNoCustomModelSet. Warning: Saving this file again will loose the link to the modelset.", lookups.m_PedModelSetNames[ms].GetCStr());
				index = CScenarioPointManager::kNoCustomModelSet;
			}
			else
			{
				Assign(index, (unsigned)foundIndex);
			}
		}
		Assign(m_PedModelSetIds.Append(), index);
	}

	const int vmscount = lookups.m_VehicleModelSetNames.GetCount();
	m_VehicleModelSetIds.Reserve(vmscount);
	for (int ms = 0; ms < vmscount; ms++)
	{
		unsigned index = CScenarioPointManager::kNoCustomModelSet;

		if (lookups.m_VehicleModelSetNames[ms] != none)
		{
			int foundIndex = CAmbientModelSetManager::GetInstance().GetModelSetIndexFromHash(CAmbientModelSetManager::kVehicleModelSets, lookups.m_VehicleModelSetNames[ms]);

			if(foundIndex == -1)
			{
				Assertf(0, "No Ambient Model Set named [%s] exists the type will be set to CScenarioPointManager::kNoCustomModelSet. Warning: Saving this file again will loose the link to the modelset.", lookups.m_VehicleModelSetNames[ms].GetCStr());
				index = CScenarioPointManager::kNoCustomModelSet;
			}
			else
			{
				Assign(index, (unsigned)foundIndex);
			}
		}
		Assign(m_VehicleModelSetIds.Append(), index);
	}
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	//update the group
	const int gcount = lookups.m_GroupNames.GetCount();
	m_GroupIds.Reserve(gcount);
	for (int g = 0; g < gcount; g++)
	{
		int index = CScenarioPointManager::kNoGroup;

		if (lookups.m_GroupNames[g] != none)
		{
			index = SCENARIOPOINTMGR.FindGroupByNameHash(lookups.m_GroupNames[g].GetHash());
			if(index == -1)
			{
				Assertf(0, "No Scenario Group named [%s] exists the type will be set to CScenarioPointManager::kNoGroup. Warning: Saving this file again will loose the link to the group.", lookups.m_GroupNames[g].GetCStr());
				index = CScenarioPointManager::kNoGroup;
			}
		}
		Assign(m_GroupIds.Append(), (unsigned)index);
	}
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	//update the interior
	const int icount = lookups.m_InteriorNames.GetCount();
	m_InteriorIds.Reserve(icount);
	for (int i = 0; i < icount; i++)
	{
		unsigned index = CScenarioPointManager::kNoInterior;
		if (lookups.m_InteriorNames[i] != none)
		{
			index = SCENARIOPOINTMGR.FindOrAddInteriorName(lookups.m_InteriorNames[i]);
		}
		Assign(m_InteriorIds.Append(), index);
	}
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	//store off the requiredImap
	const int rimcount = lookups.m_RequiredIMapNames.GetCount();
	m_RequiredIMapIds.Reserve(rimcount);
	for (int rim = 0; rim < rimcount; rim++)
	{
		int index = CScenarioPointManager::kNoRequiredIMap;
		if (lookups.m_RequiredIMapNames[rim] != none)
		{
			index = SCENARIOPOINTMGR.FindOrAddRequiredIMapByHash(lookups.m_RequiredIMapNames[rim]);
		}
		Assign(m_RequiredIMapIds.Append(), (unsigned)index);
	}
	//////////////////////////////////////////////////////////////////////////

}

void CScenarioPointLoadLookUps::RemapPointIndices(CScenarioPoint& point) const
{
	//update the type
	unsigned type = point.GetScenarioTypeVirtualOrReal();
	Assert(type < m_TypeIds.GetCount());
	point.SetScenarioType(m_TypeIds[type]);

	//update the model set
	unsigned modelIndex = point.GetModelSetIndex();
	if (CScenarioManager::IsVehicleScenarioType(point.GetScenarioTypeVirtualOrReal()))
	{
		Assert(modelIndex < m_VehicleModelSetIds.GetCount());
		point.SetModelSetIndex(m_VehicleModelSetIds[modelIndex]);
	}
	else
	{
		Assert(modelIndex < m_PedModelSetIds.GetCount());
		point.SetModelSetIndex(m_PedModelSetIds[modelIndex]);
	}

	//update the group
	unsigned groupIndex = point.GetScenarioGroup();
	Assert(groupIndex < m_GroupIds.GetCount());
	point.SetScenarioGroup(m_GroupIds[groupIndex]);

	//update the interior
	unsigned interiorIndex = point.GetInterior();
	Assert(interiorIndex < m_InteriorIds.GetCount());
	point.SetInterior(m_InteriorIds[interiorIndex]);

	//update the requiredImap
	unsigned requiredIMapIndex = point.GetRequiredIMap();
	Assert(requiredIMapIndex < m_RequiredIMapIds.GetCount());
	point.SetRequiredIMap(m_RequiredIMapIds[requiredIMapIndex]); 
}

///////////////////////////////////////////////////////////////////////////////
//  CScenarioPointContainer
///////////////////////////////////////////////////////////////////////////////

// STATIC ---------------------------------------------------------------------

#if SCENARIO_DEBUG

CScenarioPoint* CScenarioPointContainer::TranslateIntoPoint(const CExtensionDefSpawnPoint& def)
{
	CScenarioPoint* newPt = GetNewScenarioPointForEditor();
	if (newPt)
	{
		CopyDefIntoPoint(def, *newPt);
	}
	return newPt;
}

#endif	// SCENARIO_DEBUG

void CScenarioPointContainer::CopyDefIntoPoint(const CExtensionDefSpawnPoint& def, CScenarioPoint& scenarioPoint)
{
	scenarioPoint.InitArchetypeExtensionFromDefinition(&def, NULL);
#if SCENARIO_DEBUG
	scenarioPoint.SetEditable(true);
#endif // SCENARIO_DEBUG
}

CScenarioPoint* CScenarioPointContainer::GetNewScenarioPointFromPool()
{
	// In order to prevent crashes with likely data loss if exceeding the pool limits,
	// we check before trying to use the pools. If there is not enough space, we notify
	// the user and return NULL, which the calling code needs to handle.
	if(!CScenarioPoint::_ms_pPool->GetNoOfFreeSpaces())
	{
#if __ASSERT
		popAssertf(0,	"CScenarioPoint pool is full (size %d).", CScenarioPoint::_ms_pPool->GetSize());
#else
		popErrorf(		"CScenarioPoint pool is full (size %d).", CScenarioPoint::_ms_pPool->GetSize());
#endif
		return NULL;
	}

	CScenarioPoint* newPoint = CScenarioPoint::CreateFromPool();
	return newPoint;
}

#if SCENARIO_DEBUG

CScenarioPoint* CScenarioPointContainer::GetNewScenarioPointForEditor()
{
	CScenarioPoint* newPoint = CScenarioPoint::CreateForEditor();
	return newPoint;
}


CScenarioPoint* CScenarioPointContainer::DuplicatePoint(const CScenarioPoint& toBeLike)
{
	CScenarioPoint* newPt = GetNewScenarioPointForEditor();
	Assert(newPt);
	*newPt = toBeLike;
	return newPt;
}

bool CScenarioPointContainer::ms_NoPointDeleteOnDestruct = false;

#endif	// SCENARIO_DEBUG

//-----------------------------------------------------------------------------
#if SCENARIO_DEBUG
CScenarioPointContainer::CScenarioPointContainer(const CScenarioPointContainer& tobe)
{
	*this = tobe;
}

CScenarioPointContainer& CScenarioPointContainer::operator=(const CScenarioPointContainer& BANK_ONLY(tobe))
{
	if(&tobe == this)
	{
		return *this;
	}

	const int countBefore = m_EditingPoints.GetCount();
	bool somePointsWereAdded = false;
#if __ASSERT
	bool allPointsWereAdded = true;
#endif	// __ASSERT
	for(int p = 0; p < countBefore; p++)
	{
		CScenarioPoint* pt = m_EditingPoints[p];
		if(pt->GetOnAddCalled())
		{
			CScenarioManager::OnRemoveScenario(*pt);
			somePointsWereAdded = true;
		}
		else
		{
#if __ASSERT
			allPointsWereAdded = false;
#endif	// __ASSERT
		}
	}
	Assert(allPointsWereAdded || !somePointsWereAdded);

	// if we have old stuff ... 
	if (m_EditingPoints.GetCount())
	{
		//Delete the old points ... 
		//NOTE: possible complication here given containers A and B
		// CASE: A contains some non static scenario point pointers that B also contains ... 
		//		This delete call will delete data from B that we want to keep around ... 
		//
		//If we do not do this step we have possibility of memory leaks as people are editing 
		// scenario points and leaving points loose.
		//
		// The case above should not be an issue in the known case where this is called when 
		//  a region's cluster array is resized but need to be careful for other unknown users
		//  of this code ... 

		// Could probably do this, to check what the comment above suggests could be a problem:
		//	for(int j = 0; j < tobe.m_EditingPoints.GetCount(); j++)
		//	{
		//		for(int i = 0; i < m_EditingPoints.GetCount(); i++)
		//		{
		//			Assert(m_EditingPoints[i] != tobe.m_EditingPoints[j] || m_EditingPoints[i] == NULL);
		//		}
		//	}

		DeleteNonStatic();
	}
	m_EditingPoints.Reset();

	const int count = tobe.m_EditingPoints.GetCount();
	for (int p = 0; p < count; p++)
	{
		CScenarioPoint* point = tobe.m_EditingPoints[p];

		// Duplicate the point, regardless of whether it's a static one or an editing point -
		// the copy can't assume ownership of either, as long as the original exists.
		point = DuplicatePoint(*point);

		//Add this point ... 
		AddPoint(*point);
	}

	if(somePointsWereAdded)
	{
		const int countAfter = m_EditingPoints.GetCount();
		for(int p = 0; p < countAfter; p++)
		{
			CScenarioPoint* pt = m_EditingPoints[p];
			if(!pt->GetOnAddCalled())
			{
				CScenarioManager::OnAddScenario(*pt);
			}
		}
	}

	Assert(m_EditingPoints.GetCount() == tobe.m_EditingPoints.GetCount());
	return *this;
}
#endif // SCENARIO_DEBUG

CScenarioPointContainer::~CScenarioPointContainer()
{
	CleanUp();
}

void CScenarioPointContainer::CleanUp()
{
#if SCENARIO_DEBUG
	//The ms_NoPointDeleteOnDestruct static is set for cases where we dont want to delete the points
	// on destruction of this object. The case where this is used currently is
	// for allowing editing of clusters. If we add a new cluster to a region and the 
	// array of clusters in the region needs to be resized the atArray will delete 
	// the point container that is in the CScenarioPointCluster object which will cause
	// the unexpected delete of the points in that cluster.
	if (ms_NoPointDeleteOnDestruct)
	{
		m_EditingPoints.Reset();
		return;
	}

	//"Static" points will be handled without issue ... 
	DeleteNonStatic();
	m_EditingPoints.Reset();
#endif // SCENARIO_DEBUG
}

u32 CScenarioPointContainer::GetNumPoints() const
{
#if SCENARIO_DEBUG
	return m_EditingPoints.GetCount();
#else
	return m_MyPoints.GetCount();
#endif
}

const CScenarioPoint& CScenarioPointContainer::GetPoint(const u32 index) const
{
#if SCENARIO_DEBUG
	Assert(index < m_EditingPoints.GetCount());
	Assert(m_EditingPoints[index]);
	return *m_EditingPoints[index];
#else
	Assert(index < m_MyPoints.GetCount());
	return m_MyPoints[index];
#endif
}

CScenarioPoint& CScenarioPointContainer::GetPoint(const u32 index)
{
#if SCENARIO_DEBUG
	Assert(index < m_EditingPoints.GetCount());
	Assert(m_EditingPoints[index]);
	return *m_EditingPoints[index];
#else
	Assert(index < m_MyPoints.GetCount());
	return m_MyPoints[index];
#endif
}

void CScenarioPointContainer::PostLoad(const CScenarioPointLoadLookUps& lookups)
{
	int numPoints = m_LoadSavePoints.GetCount();
	bool legacyConvert = false;
	if (numPoints)
	{
		//LEGACY CONVERT !!!
		legacyConvert = true;
		m_MyPoints.Reserve(numPoints);
	}
	else
	{
		numPoints = m_MyPoints.GetCount();
	}
	
#if SCENARIO_DEBUG
	m_EditingPoints.Reserve(numPoints);
#endif
	for(int i=0;i<numPoints;i++)
	{
		CScenarioPoint* point = NULL;
		if (legacyConvert)
		{
			CopyDefIntoPoint(m_LoadSavePoints[i], m_MyPoints.Append());
			point = &m_MyPoints[i];
		}
		else
		{
			lookups.RemapPointIndices(m_MyPoints[i]);
			point = &m_MyPoints[i];
		}

		Assert(point);
		point->MarkAsFromRsc(); //These points are from resourced assets so make them so ... 
#if SCENARIO_DEBUG
		point->SetEditable(true); //these points are editable ... 
		m_EditingPoints.Grow() = point;
#endif
	}

	//only need to do this if we are doing a legacy convert ... 
	if (legacyConvert)
	{
		// Now that we are done with adding points ... clear up this memory.
		//m_LoadSavePoints.Reset();	
	}
}

void CScenarioPointContainer::PostPsoPlace(void* data)
{
	//ONLY PUT NON-PSO data init in here ...
	Assert(data);
	CScenarioPointContainer* initData = reinterpret_cast<CScenarioPointContainer*>(data);

	rage_placement_new(&initData->m_EditingPoints) atArray<CScenarioPoint*>();
}

#if SCENARIO_DEBUG

void CScenarioPointContainer::SwapPointOrder(const u32 fromIndex, const u32 toIndex)
{
	//NOTE: this should only happen when in bank builds so only use the edit points array.
	Assert(fromIndex < m_EditingPoints.GetCount());
	Assert(m_EditingPoints[fromIndex]);
	Assert(toIndex < m_EditingPoints.GetCount());
	Assert(m_EditingPoints[toIndex]);

	CScenarioPoint* temp = m_EditingPoints[toIndex];
	m_EditingPoints[toIndex] = m_EditingPoints[fromIndex];
	m_EditingPoints[fromIndex] = temp;
}

void CScenarioPointContainer::PrepSaveObject(const CScenarioPointContainer& prepFrom, CScenarioPointLookUps& out_Lookups)
{
	DeleteNonStatic(); //just to be sure ... 
	m_LoadSavePoints.Reset();
	m_MyPoints.Reset();
	m_EditingPoints.Reset();
	
	const int numScenarioPoints = prepFrom.GetNumPoints();

	//Resize array to correct size.
	//m_LoadSavePoints.Reserve(numScenarioPoints); //Legacy save out ... 
	m_MyPoints.Reserve(numScenarioPoints);

	for(int i = 0; i < numScenarioPoints; i++)
	{
		const CScenarioPoint& spoint = prepFrom.GetPoint(i);
		if (!spoint.IsEditable())
			continue;

		//spoint.CopyToDefinition(&m_LoadSavePoints.Append()); //Legacy save out ... 
		CScenarioPoint& appended = m_MyPoints.Append();
		appended.PrepSaveObject(spoint);
		out_Lookups.AppendFromPoint(appended);
	}
}

int CScenarioPointContainer::AddPoint(CScenarioPoint& point)
{
	InsertPoint(GetNumPoints(), point);
	return GetNumPoints()-1;
}

void CScenarioPointContainer::InsertPoint(const u32 index, CScenarioPoint& point)
{
	Assertf(!CScenarioPoint::GetPool()->IsValidPtr(&point), "Scenario point passed to InsertPoint() should not come from the pool.");

	// Grow and then pop the new item... hacky way of resizing the max array size prior to the insert
	m_EditingPoints.Grow();
	m_EditingPoints.Pop();
	m_EditingPoints.Insert(index) = &point;
}

CScenarioPoint* CScenarioPointContainer::RemovePoint(const u32 index)
{
	CScenarioPoint* retval = &GetPoint(index);

	if (IsStaticPoint(*retval))
	{
		bool wasAdded = retval->GetOnAddCalled();
		if(wasAdded)
		{
			CScenarioManager::OnRemoveScenario(*retval);
		}

		//Since it is "STATIC" need to duplicate it so we dont loose the info if this struct is deleted.
		retval = DuplicatePoint(*retval);

		if(wasAdded && retval)
		{
			CScenarioManager::OnAddScenario(*retval);
		}
	}

	m_EditingPoints.Delete(index);//no longer tracked by this array
	return retval;
}


void CScenarioPointContainer::CallOnAddOnEachScenarioPoint()
{
	const int numPts = m_EditingPoints.GetCount();
	for(int i = 0; i < numPts; i++)
	{
		CScenarioPoint* pt = m_EditingPoints[i];
		Assert(!pt->GetOnAddCalled());
		CScenarioManager::OnAddScenario(*pt);
	}
}


int CScenarioPointContainer::CallOnRemoveOnEachScenarioPoint()
{
	int numRemoved = 0;

	const int numPts = m_EditingPoints.GetCount();
	for(int i = 0; i < numPts; i++)
	{
		CScenarioPoint* pt = m_EditingPoints[i];
		if(pt->GetOnAddCalled())
		{
			CScenarioManager::OnRemoveScenario(*pt);
			numRemoved++;
		}
	}

	return numRemoved;
}


bool CScenarioPointContainer::IsStaticPoint(const CScenarioPoint& point) const
{
	if (!m_MyPoints.GetCount())
		return false;

	//Get the "STATIC" point address range ... 
	const CScenarioPoint* stFirst = &m_MyPoints[0];
	const CScenarioPoint* stLast = &m_MyPoints[m_MyPoints.GetCount()-1];
	const CScenarioPoint* pntAdd = &point;

	//if the point address is between the "STATIC" point range then this is a static point.
	if (pntAdd >= stFirst && pntAdd <= stLast)
	{
		return true;
	}

	return false;
}

void CScenarioPointContainer::DeleteNonStatic()
{
	//Make sure we clean up all the points that i am holding on to.
	const int count = m_EditingPoints.GetCount();
	for(int p=0; p < count; p++)
	{
		Assert(m_EditingPoints[p]);
		if (!IsStaticPoint(*m_EditingPoints[p]))
		{
			CScenarioPoint::DestroyForEditor(*m_EditingPoints[p]);	//delete it since we are the one holding on to it ... 
		}
		m_EditingPoints[p] = NULL;
	}
}

#endif
