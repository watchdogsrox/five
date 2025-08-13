//
// task/Scenario/ScenarioEntityOverride.cpp
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

//Rage Headers

//Framework Headers
#if __BANK
#include "ai/task/taskchannel.h"
#include "fwscene/search/SearchVolumes.h"
#include "fwsys/timer.h"
#endif // __BANK

//Game Headers
#include "Objects/object.h"
#include "scene/Entity.h"
#include "scene/InstancePriority.h"
#include "task/Scenario/ScenarioEntityOverride.h"
#include "task/Scenario/ScenarioPointRegion.h"

#if SCENARIO_DEBUG
#include "fwscene/stores/mapdatastore.h"
#include "scene/FocusEntity.h"
#include "scene/world/GameWorld.h"
#include "task/Scenario/ScenarioPointManager.h"
#endif // SCENARIO_DEBUG

#if SCENARIO_VERIFY_ENTITY_OVERRIDES
#include "camera/CamInterface.h"
#endif	// SCENARIO_VERIFY_ENTITY_OVERRIDES

AI_OPTIMISATIONS()

namespace AIStats
{
	EXT_PF_TIMER(CScenarioEntityOverrideVerification);
}
using namespace AIStats;

///////////////////////////////////////////////////////////////////////////////
//  CScenarioEntityOverride
///////////////////////////////////////////////////////////////////////////////

#if SCENARIO_DEBUG

void CScenarioEntityOverride::Init(CEntity& entity)
{
	// Get the position and model hash.
	const Vec3V entityPosV = entity.GetTransform().GetPosition();

	u32 modelHash = 0;
	CBaseModelInfo* pModelInfo = entity.GetBaseModelInfo();
	if(Verifyf(pModelInfo, "Trying to add override for entity with no CBaseModelInfo."))
	{
		modelHash = entity.GetBaseModelInfo()->GetModelNameHash();
	}

	m_EntityMayNotAlwaysExist = GetDefaultMayNotAlwaysExist(entity);

	// Set up the binding to the entity.
	m_EntityPosition = entityPosV;
	m_EntityType = modelHash;
	m_EntityCurrentlyAttachedTo = &entity;

	// Clear out any old point data.
	m_ScenarioPoints.Reset();
	m_ScenarioPointsInUse.Reset();

	m_SpecificallyPreventArtPoints = false;

#if SCENARIO_VERIFY_ENTITY_OVERRIDES
	// We are attached to something now, so we must be working.
	m_VerificationStatus = kVerificationStatusWorking;
#endif	// SCENARIO_VERIFY_ENTITY_OVERRIDES
}

void CScenarioEntityOverride::PrepSaveObject(const CScenarioEntityOverride& prepFrom)
{
	//copy the PSO data stuff ... 
	m_EntityPosition = prepFrom.m_EntityPosition;
	m_EntityType = prepFrom.m_EntityType;
	m_ScenarioPoints = prepFrom.m_ScenarioPoints;
	m_EntityMayNotAlwaysExist = prepFrom.m_EntityMayNotAlwaysExist;
	m_SpecificallyPreventArtPoints = prepFrom.m_SpecificallyPreventArtPoints;
}

bool CScenarioEntityOverride::GetDefaultMayNotAlwaysExist(const CEntity& entity)
{
	strLocalIndex index = strLocalIndex(entity.GetIplIndex());
	if(index.Get() > 0)	// Or should this be >= 0?
	{
		if(INSTANCE_STORE.GetSlot(index)->GetIsScriptManaged())
		{
			return true;
		}
	}
	return false;
}

#endif	// SCENARIO_DEBUG

void CScenarioEntityOverride::PostPsoPlace(void* data)
{
	//ONLY PUT NON-PSO data init in here ...
	Assert(data);
	CScenarioEntityOverride* initData = reinterpret_cast<CScenarioEntityOverride*>(data);

	rage_placement_new(&initData->m_ScenarioPointsInUse) atArray<RegdScenarioPnt>();
	rage_placement_new(&initData->m_EntityCurrentlyAttachedTo) RegdEnt();

	initData->m_VerificationStatus = 0;

#if SCENARIO_DEBUG
#if SCENARIO_VERIFY_ENTITY_OVERRIDES
	// We are attached to something now, so we must be working.
	initData->m_VerificationStatus = kVerificationStatusWorking;
#endif	// SCENARIO_VERIFY_ENTITY_OVERRIDES
#endif // SCENARIO_DEBUG
}

///////////////////////////////////////////////////////////////////////////////
//  CScenarioEntityOverridePostLoadUpdateQueue
///////////////////////////////////////////////////////////////////////////////

// STATIC ---------------------------------------------------------------------
CScenarioEntityOverridePostLoadUpdateQueue* CScenarioEntityOverridePostLoadUpdateQueue::sm_Instance = NULL;

//-----------------------------------------------------------------------------
CScenarioEntityOverridePostLoadUpdateQueue::CScenarioEntityOverridePostLoadUpdateQueue()
: m_LastObject(0)
, m_LastBuilding(0)
#if SCENARIO_ENTITY_OVERRIDE_PERF_TRACK
, m_AccumulatedTime(0.0f), m_MaxTime(0.0f), m_TotalUpdates(0)
#endif
{

}

void CScenarioEntityOverridePostLoadUpdateQueue::Update()
{
	//early out ... 
	if (!m_QueuedRegions.GetCount())
		return;

	CScenarioPointRegion* curRegion = m_QueuedRegions[0];

#if SCENARIO_ENTITY_OVERRIDE_PERF_TRACK
	sysPerformanceTimer timer("CScenarioEntityOverridePostLoadUpdateQueue::Update");
	timer.Start();
#endif

	// Since we don't know if the objects are streamed in first or if the scenario region is streamed in first,
	// we need to deal with both possibilities. Here, we apply the overrides in this region, to any objects
	// that already exist.
	// Note: this has some potential to be slow, could probably spread it out over multiple frames if needed.
	const u32 numObjs = (u32) CObject::GetPool()->GetSize();
	const u32 numBuildings = (u32) CBuilding::GetPool()->GetSize();

	static const u32 NumToCheckPerFrame = 512;
	u32 remainder = NumToCheckPerFrame;

	if(curRegion->ContainsObjects())
	{
		for(; m_LastObject < numObjs;)
		{
			CObject* pObj = CObject::GetPool()->GetSlot(m_LastObject++);
			if(pObj)
			{
				curRegion->ApplyOverridesForEntity(*pObj, true, false);
				remainder--;

				if (!remainder)
				{
					break;
				}
			}
		}
	}
	else
	{
		m_LastObject = numObjs;
	}
	
	if (remainder)
	{
		if(curRegion->ContainsBuildings())
		{
			for(; m_LastBuilding < numBuildings;)
			{
				CBuilding* pBuilding = CBuilding::GetPool()->GetSlot(m_LastBuilding++);
				if(pBuilding)
				{
					curRegion->ApplyOverridesForEntity(*pBuilding, true, false);
					remainder--;

					if (!remainder)
					{
						break;
					}
				}
			}
		}
		else
		{
			m_LastBuilding = numBuildings;
		}
	}	

#if SCENARIO_ENTITY_OVERRIDE_PERF_TRACK
	timer.Stop();
	float curTime = timer.GetTimeMS();
	m_AccumulatedTime += curTime;
	m_MaxTime = Max(m_MaxTime, curTime);
	m_TotalUpdates++;
#endif

	if (numObjs == m_LastObject && numBuildings == m_LastBuilding)
	{
		//We have processed all the current objects/buildings ... 
		m_LastObject = 0;
		m_LastBuilding = 0;
#if SCENARIO_ENTITY_OVERRIDE_PERF_TRACK
		aiDebugf2("CScenarioEntityOverridePostLoadUpdateQueue::Update R {0x%p} finished in %d updates and took %.4f ms (max time %.4f ms)\n", curRegion, m_TotalUpdates, m_AccumulatedTime, m_MaxTime);
		m_AccumulatedTime = 0.0f;
		m_MaxTime = 0.0f;
		m_TotalUpdates = 0;
#endif
		m_QueuedRegions.Delete(0);
	}
	else
	{
		// If these fail, we may actually be stuck and not do anything, yet not move on to
		// the next region - I suspect that there is a bug with RemoveFromQueue() now,
		// and added the asserts to confirm.
		Assert(m_LastObject <= numObjs);
		Assert(m_LastBuilding <= numBuildings);
	}
}

void CScenarioEntityOverridePostLoadUpdateQueue::AddToQueue(CScenarioPointRegion* region)
{
#if __ASSERT
	if (Verifyf(m_QueuedRegions.Find(region) == -1, "region [%p] is already in the ScenarioEntityOverridePostLoadUpdateQueue", region))
#endif
	{
		m_QueuedRegions.PushAndGrow(region);
	}
}

void CScenarioEntityOverridePostLoadUpdateQueue::RemoveFromQueue(CScenarioPointRegion* region)
{
	const int index = m_QueuedRegions.Find(region);
	if (index != -1)
	{
		m_QueuedRegions.Delete(index);
	}
}

bool CScenarioEntityOverridePostLoadUpdateQueue::IsEntityPending(CScenarioPointRegion& region, CEntity& entity)
{
	const int index = m_QueuedRegions.Find(&region);
	
	// If the region is in the queu waiting to be processed flag as pending
	bool bPending = index > 0;
	
	// The first region in the queue is in the middle of getting processed
	// We need to check if we haven't processed it yet (making it pending) or not
	if(index == 0)
	{
		if(entity.GetIsTypeObject())
		{
			if(!static_cast<CObject&>(entity).IsPickup())
			{
				bPending = m_LastObject <= CObject::GetPool()->GetJustIndex(&entity);
			}
		}
		else if(entity.GetIsTypeBuilding())
		{
			bPending = m_LastBuilding <= CBuilding::GetPool()->GetJustIndex(&entity);
		}
	}
	
	return bPending;
}

#if SCENARIO_VERIFY_ENTITY_OVERRIDES

///////////////////////////////////////////////////////////////////////////////
//  CScenarioEntityOverrideVerification::Tunables
///////////////////////////////////////////////////////////////////////////////

CScenarioEntityOverrideVerification::Tunables::Tunables()
: m_BrokenPropSearchRange(20.0f)
, m_ExpectStreamedInDistance(40.0f)
, m_StruggleTime(15.0f)
, m_NumToCheckPerFrame(5)
{
}

///////////////////////////////////////////////////////////////////////////////
//  CScenarioEntityOverrideVerification
///////////////////////////////////////////////////////////////////////////////

// STATIC ---------------------------------------------------------------------

CScenarioEntityOverrideVerification::Tunables CScenarioEntityOverrideVerification::sm_Tunables;
CScenarioEntityOverrideVerification* CScenarioEntityOverrideVerification::sm_Instance = NULL;

//-----------------------------------------------------------------------------

CScenarioEntityOverrideVerification::CScenarioEntityOverrideVerification()
: m_CurrentRegionIndex(0)
, m_CurrentOverrideIndex(0)

{
}

void CScenarioEntityOverrideVerification::Update()
{
	PF_FUNC(CScenarioEntityOverrideVerification);

	if(fwTimer::IsGamePaused())
	{
		return;
	}

	// If we are in the process of loading some mission, we don't do this stuff,
	// and we make sure to clear any previous struggle data we have - too many things
	// that can happen in terms of teleporting around, loading synchronously, etc.
	if(!camInterface::IsFadedIn())
	{
		ClearAllStruggling();
		return;
	}

	int regionIndex = m_CurrentRegionIndex;
	int overrideIndex = m_CurrentOverrideIndex;
	int numRegionsLookedAt = 0;	
	Assert(regionIndex >= 0);

	const int numRegions = SCENARIOPOINTMGR.GetNumRegions();
	if(numRegions > 0)
	{
		int numChecked = 0;
		bool justStarted = true;
		while(numChecked < sm_Tunables.m_NumToCheckPerFrame)
		{
			if(regionIndex < numRegions)
			{
				CScenarioPointRegion* pRegion = SCENARIOPOINTMGR.GetRegion(regionIndex);
				Assert(overrideIndex >= 0);
				if(pRegion && overrideIndex < pRegion->GetNumEntityOverrides())
				{
					UpdateEntityOverride(regionIndex, overrideIndex);
					overrideIndex++;
					numChecked++;
					justStarted = false;
					continue;
				}
				else
				{
					overrideIndex = 0;
					regionIndex++;
					if(!justStarted)
					{
						numRegionsLookedAt++;
					}
					justStarted = false;

					if(numRegionsLookedAt >= numRegions)
					{
						break;
					}
				}
			}
			else
			{
				overrideIndex = regionIndex = 0;
			}
		}
	}
	m_CurrentRegionIndex = regionIndex;
	m_CurrentOverrideIndex = overrideIndex;
}


void CScenarioEntityOverrideVerification::ClearInfoForRegion(CScenarioPointRegion& region)
{
	int numStruggling = m_ScenarioOverridesStruggling.GetCount();
	if(!numStruggling)
	{
		// Nothing to do.
		return;
	}

	int regionIndex = -1;
	const int numRegions = SCENARIOPOINTMGR.GetNumRegions();
	for(int i = 0; i < numRegions; i++)
	{
		CScenarioPointRegion* pRegion = SCENARIOPOINTMGR.GetRegion(i);
		if(pRegion == &region)
		{
			regionIndex = i;
			break;
		}
	}
	if(regionIndex < 0)
	{
		// May have been some temporary object or something.
		return;
	}

	for(int i = 0; i < numStruggling; i++)
	{
		if(m_ScenarioOverridesStruggling[i].m_RegionIndex == regionIndex)
		{
			m_ScenarioOverridesStruggling.DeleteFast(i);
			i--;
			numStruggling--;
		}
	}
}


void CScenarioEntityOverrideVerification::ClearAllStruggling()
{
	const int numStruggling = m_ScenarioOverridesStruggling.GetCount();
	for(int i = 0; i < numStruggling; i++)
	{
		StrugglingOverride& s = m_ScenarioOverridesStruggling[i];
		const int regionIndex = s.m_RegionIndex;
		const int overrideIndex = s.m_OverrideIndex;		
		CScenarioPointRegion* pRegion = SCENARIOPOINTMGR.GetRegion(regionIndex);
		if(pRegion && overrideIndex < pRegion->GetNumEntityOverrides())
		{
			CScenarioPointRegionEditInterface bankRegion(*pRegion);
			CScenarioEntityOverride& override = bankRegion.GetEntityOverride(overrideIndex);
			if(override.m_VerificationStatus == CScenarioEntityOverride::kVerificationStatusStruggling)
			{
				override.m_VerificationStatus = CScenarioEntityOverride::kVerificationStatusUnknown;
			}
	
		}
	}
	m_ScenarioOverridesStruggling.Reset();
}


int CScenarioEntityOverrideVerification::FindStruggleIndex(int regionIndex, int overrideIndex) const
{
	for(int i = 0; i < m_ScenarioOverridesStruggling.GetCount(); i++)
	{
		const StrugglingOverride& s = m_ScenarioOverridesStruggling[i];
		if(s.m_OverrideIndex == overrideIndex && s.m_RegionIndex == regionIndex)
		{
			return i;
		}
	}
	return -1;
}


void CScenarioEntityOverrideVerification::UpdateEntityOverride(int regionIndex, int overrideIndex)
{
	CScenarioPointRegion* pRegion = SCENARIOPOINTMGR.GetRegion(regionIndex);
	Assert(pRegion);
	CScenarioPointRegionEditInterface bankRegion(*pRegion);
	CScenarioEntityOverride& override = bankRegion.GetEntityOverride(overrideIndex);

	const bool isStruggling = IsOverrideStruggling(override);

	int struggleIndex = FindStruggleIndex(regionIndex, overrideIndex);

	Assert(m_ScenarioOverridesStruggling.GetCount() <= 1000);

	if(isStruggling)
	{
		const float currentTime = fwTimer::GetTimeInMilliseconds()*0.001f;

		if(struggleIndex < 0)
		{
			struggleIndex = m_ScenarioOverridesStruggling.GetCount();
			StrugglingOverride& s = m_ScenarioOverridesStruggling.Grow();
			Assert(regionIndex >= 0 && regionIndex <= 0xffff);
			Assert(overrideIndex >= 0 && overrideIndex <= 0xffff);
			s.m_RegionIndex = (u16)regionIndex;
			s.m_OverrideIndex = (u16)overrideIndex;
			s.m_TimeFirstStruggling = currentTime;

			override.m_VerificationStatus = CScenarioEntityOverride::kVerificationStatusStruggling;
		}
		else
		{
			StrugglingOverride& s = m_ScenarioOverridesStruggling[struggleIndex];
			float timeStruggling = currentTime - s.m_TimeFirstStruggling;

			if(timeStruggling >= sm_Tunables.m_StruggleTime)
			{
				bool foundObject = false;
				CBaseModelInfo* pModel = CModelInfo::GetBaseModelInfoFromHashKey(override.m_EntityType.GetHash(), NULL);
				if(pModel && pModel->GetIsTypeObject())
				{
					const spdSphere scanningSphere(override.m_EntityPosition, ScalarV(sm_Tunables.m_BrokenPropSearchRange));

					CheckNearbyBrokenObjectCallbackData cbData;
					cbData.m_DesiredModelInfo = pModel;

					fwIsSphereIntersecting intersection(scanningSphere);
					CGameWorld::ForAllEntitiesIntersecting(&intersection, CheckBrokenObjectOfTypeCallback, &cbData
						, ENTITY_TYPE_MASK_OBJECT
						, SEARCH_LOCATION_INTERIORS|SEARCH_LOCATION_EXTERIORS
						, SEARCH_LODTYPE_HIGHDETAIL
						, SEARCH_OPTION_FORCE_PPU_CODEPATH
						, WORLDREP_SEARCHMODULE_PEDS);

					foundObject = cbData.m_ObjectFound;
				}

				bool foundUnstreamedInterior = false;
				if(!foundObject && !override.m_EntityMayNotAlwaysExist)
				{
					// Do a probe to see which interior is supposed to be here. Using that, we can
					// check if that interior is streamed in and populated. If it's not, we shouldn't
					// fail the assert, because the override may well be working once the interior
					// actually gets populated with props.
					s32 roomIdx = 0;
					CInteriorInst* pIntInst = NULL;
					if(!CPortalTracker::ProbeForInterior(RCC_VECTOR3(override.m_EntityPosition), pIntInst, roomIdx, NULL, CPortalTracker::NEAR_PROBE_DIST))
					{
						foundUnstreamedInterior = true;
					}
					else if(pIntInst && !pIntInst->CanReceiveObjects())
					{
						foundUnstreamedInterior = true;
					}
				}

				if(!foundObject && !foundUnstreamedInterior && !override.m_EntityMayNotAlwaysExist)
				{
					const Vec3V streamingCenterPosV = VECTOR3_TO_VEC3V(CFocusEntityMgr::GetMgr().GetPos());
#if __ASSERT
					taskAssertf(0,
						"Scenario override for prop %s at %.1f, %.1f, %.1f may be broken, no prop has been found despite being nearby for some time. Region is %s. Distance measured to %.1f, %.1f, %.1f.",
						override.m_EntityType.GetCStr(),
						override.m_EntityPosition.GetXf(), override.m_EntityPosition.GetYf(), override.m_EntityPosition.GetZf(),
						SCENARIOPOINTMGR.GetRegionName(regionIndex),
						streamingCenterPosV.GetXf(), streamingCenterPosV.GetYf(), streamingCenterPosV.GetZf());
#else
					taskErrorf(
						"Scenario override for prop %s at %.1f, %.1f, %.1f may be broken, no prop has been found despite being nearby for some time. Region is %s. Distance measured to %.1f, %.1f, %.1f.",
						override.m_EntityType.GetCStr(),
						override.m_EntityPosition.GetXf(), override.m_EntityPosition.GetYf(), override.m_EntityPosition.GetZf(),
						SCENARIOPOINTMGR.GetRegionName(regionIndex),
						streamingCenterPosV.GetXf(), streamingCenterPosV.GetYf(), streamingCenterPosV.GetZf());
#endif	// __ASSERT
				}


				m_ScenarioOverridesStruggling.DeleteFast(struggleIndex);
				if(foundObject)
				{
					override.m_VerificationStatus = CScenarioEntityOverride::kVerificationStatusBrokenProp;
				}
				else if(foundUnstreamedInterior)
				{
					override.m_VerificationStatus = CScenarioEntityOverride::kVerificationStatusInteriorNotStreamedIn;
				}
				else
				{
					override.m_VerificationStatus = CScenarioEntityOverride::kVerificationStatusBroken;
				}
			}
		}
	}
	else
	{
		if(struggleIndex >= 0)
		{
			m_ScenarioOverridesStruggling.DeleteFast(struggleIndex);
			if(override.m_VerificationStatus == CScenarioEntityOverride::kVerificationStatusStruggling)
			{
				override.m_VerificationStatus = CScenarioEntityOverride::kVerificationStatusUnknown;
			}
		}
	}
}


bool CScenarioEntityOverrideVerification::CheckBrokenObjectOfTypeCallback(CEntity* pEntity, void* pData)
{
	CheckNearbyBrokenObjectCallbackData& data = *reinterpret_cast<CheckNearbyBrokenObjectCallbackData*>(pData);

	if(!pEntity || !pEntity->GetIsTypeObject())
	{
		return true;
	}

	CObject& obj = *static_cast<CObject*>(pEntity);
	if(!obj.m_nObjectFlags.bHasBeenUprooted)
	{
		return true;
	}

	if(obj.GetBaseModelInfo() != data.m_DesiredModelInfo)
	{
		return true;
	}

	data.m_ObjectFound = true;

	return false;
}


bool CScenarioEntityOverrideVerification::IsOverrideStruggling(const CScenarioEntityOverride& override)
{
	if(override.m_EntityCurrentlyAttachedTo
		|| override.m_VerificationStatus == CScenarioEntityOverride::kVerificationStatusBroken
		|| override.m_VerificationStatus == CScenarioEntityOverride::kVerificationStatusBrokenProp
		|| override.m_VerificationStatus == CScenarioEntityOverride::kVerificationStatusWorking
		)
	{
		return false;
	}

	// In MP, this GlobalInstancePriority thing is currently set to 0, and many props don't
	// get instantiated. In that case, we can't easily know if an override is really broken or if
	// the associated prop just isn't instantiated at this priority level. For now, we just consider
	// them all "not struggling" in that case, which should prevent asserts, etc.
	// Note: checking against 0 here is pretty arbitrary. A more proper fix would be to store in the override
	// which level a prop is expected to be there at, but for the problems we are currently having, this doesn't
	// seem to be worth doing.
	if (CInstancePriority::GetCurrentPriority() <= 0)
	{
		return false;
	}

	const Vec3V streamingCenterPosV = VECTOR3_TO_VEC3V(CFocusEntityMgr::GetMgr().GetPos());
	const Vec3V posV = override.m_EntityPosition;

	const ScalarV distThresholdSqV(square(sm_Tunables.m_ExpectStreamedInDistance));

	const ScalarV distToStreamingCenterSqV = DistSquared(streamingCenterPosV, posV);


	if(IsGreaterThanAll(distToStreamingCenterSqV, distThresholdSqV))
	{
		return false;
	}

	return true;
} 

#endif	// SCENARIO_VERIFY_ENTITY_OVERRIDES
