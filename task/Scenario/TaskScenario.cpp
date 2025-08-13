// FILE :    TaskScenario.h
// PURPOSE : Creates spawning tasks for peds on foot and peds in cars
//			 based on spawnpoints around the world
// AUTHOR :  Phil H
// CREATED : 22-01-2007

// Framework headers
#include "ai/task/taskchannel.h"
#include "animation/MovePed.h"
#include "Debug/DebugScene.h"
#include "game/clock.h"
#include "game/weather.h"
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "Objects/DummyObject.h"
#include "Peds/NavCapabilities.h"
#include "Peds/PedCapsule.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PedPhoneComponent.h"
#include "Peds/Ped.h"
#include "Peds/pedpopulation.h"			// For CPedPopulation.
#include "Physics/Physics.h"
#include "Scene/Entity.h"
#include "scene/EntityIterator.h"
#include "Scene/World/GameWorld.h"
#include "streaming/streaming.h"		// For CStreaming::HasObjectLoaded(), etc.
#include "Task/Combat/TaskThreatResponse.h"
#include "Task/Default/AmbientAnimationManager.h"
#include "Task/Default/Patrol/PatrolRoutes.h"
#include "Task/Default/TaskChat.h"
#include "Task/Default/TaskWander.h"
#include "Task/Motion/TaskMotionBase.h"
#include "Task/Movement/Jumping/TaskFallGetUp.h"
#include "Task/Movement/TaskFlyToPoint.h"
#include "Task/Movement/TaskGoto.h"
#include "Task/Movement/TaskMoveWander.h"
#include "Task/Movement/TaskNavMesh.h"
#include "Task/Movement/TaskSlideToCoord.h"
#include "Task/Movement/TaskMoveFollowEntityOffset.h"
#include "Task/Physics/TaskNM.h"
#include "Task/Scenario/Info/ScenarioInfo.h"
#include "Task/Scenario/Info/ScenarioInfoConditions.h"
#include "Task/Scenario/Info/ScenarioInfoManager.h"
#include "Task/Scenario/ScenarioManager.h"
#include "Task/Scenario/ScenarioPoint.h"
#include "Task/Scenario/ScenarioPointManager.h"
#include "Task/Scenario/ScenarioPointRegion.h"
#include "Task/Scenario/TaskScenario.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Weapons/WeaponTypes.h"
#include "Vehicles/Train.h"

#include "Debug/DebugScene.h"

#if !__FINAL
#include "string/string.h"
#endif // !__FINAL


AI_OPTIMISATIONS()

//-------------------------------------------------------------------------
// CClonedScenarioInfo
//-------------------------------------------------------------------------

CClonedScenarioInfo::CClonedScenarioInfo(s16 scenarioType, CScenarioPoint* pScenarioPoint, bool bIsWorldScenario, int scenarioPointIndex)
: CClonedScenarioFSMInfoBase( scenarioType, pScenarioPoint,  bIsWorldScenario, scenarioPointIndex)
{

}

CClonedScenarioInfo::CClonedScenarioInfo()
: CClonedScenarioFSMInfoBase()
{
}

void CClonedScenarioInfo::Serialise(CSyncDataBase& serialiser)
{
	CSerialisedFSMTaskInfo::Serialise(serialiser);
	CClonedScenarioFSMInfoBase::Serialise(serialiser);
}

//-------------------------------------------------------------------------
// CClonedScenarioFSMInfo
//-------------------------------------------------------------------------
void CClonedScenarioFSMInfo::Serialise(CSyncDataBase& serialiser)
{
	CSerialisedFSMTaskInfo::Serialise(serialiser);
	CClonedScenarioFSMInfoBase::Serialise(serialiser);
}

//-------------------------------------------------------------------------
// CClonedScenarioFSMInfoBase
//-------------------------------------------------------------------------
CClonedScenarioFSMInfoBase::CClonedScenarioFSMInfoBase(s16 scenarioType, CScenarioPoint* pScenarioPoint, bool bIsWorldScenario, int scenarioPointIndex)
: m_scenarioType(scenarioType)
, m_bIsWorldScenario(bIsWorldScenario)
, m_ScenarioId(scenarioPointIndex)
, m_ScenarioPos(VEC3_ZERO)
{
    if (pScenarioPoint)
	{
		if (m_bIsWorldScenario)
		{
			// In this case, the user should have passed in the index of pScenarioPoint, so we shouldn't
			// have to do anything here, except some validation of what the user passed in.
#if __ASSERT
			if(scenarioPointIndex!=-1)
			{
				CScenarioPoint* pspoint = GetScenarioPoint();

				if(pspoint) //Null if region streamed out
				{
					taskAssertf(pspoint == pScenarioPoint, "Scenario point index/pointer mismatch.");
				}
			}
			else
			{
				taskAssertf(0, "scenarioPointIndex == -1");
			}
#endif	// __ASSERT
		}
		else
		{
			m_ScenarioPos = VEC3V_TO_VECTOR3(pScenarioPoint->GetPosition());
		}
	}
}

CScenarioPoint*	CClonedScenarioFSMInfoBase::GetScenarioPoint()
{
	CScenarioPoint* pspoint = NULL;

	if (m_bIsWorldScenario && m_ScenarioId != -1)
	{
		pspoint = GetScenarioPoint( m_ScenarioId );
	}

	return pspoint;
}

CScenarioPoint*	CClonedScenarioFSMInfoBase::GetScenarioPoint( s32 scenarioId )
{
	CScenarioPoint* pspoint = NULL;

	if (scenarioId != -1)
	{
		//Mask off the data that we dont want to extract all the data
		// for the point ... 
		int pointId = scenarioId & 0x00000fff;
		int clusterId = (scenarioId & 0x00fff000) >> 12;
		int regionId =  (scenarioId & 0xff000000) >> 24;

		taskAssertf(regionId >= 0, "Expected region index of world scenario point.");
		taskAssertf(pointId >= 0,  "Expected point index of world scenario point.");
		if(taskVerifyf(regionId < SCENARIOPOINTMGR.GetNumRegions(),"scenarioId %d, regionId %d, SCENARIOPOINTMGR.GetNumRegions %d", scenarioId, regionId , SCENARIOPOINTMGR.GetNumRegions()))
		{
			CScenarioPointRegion* region = SCENARIOPOINTMGR.GetRegion(regionId);
			if(region)//This region could be streamed out for me
			{
				if (clusterId != 0xfff)
				{
					if( taskVerifyf(clusterId < region->GetNumClusters(),"clusterId %d, outside expected range %d. regionId %d, scenarioId %d",clusterId, region->GetNumClusters(), regionId,scenarioId ) &&
						taskVerifyf(pointId < region->GetNumClusteredPoints(clusterId),"pointId %d, outside expected range %d. clusterId %d, regionId %d, scenarioId %d",pointId,region->GetNumClusteredPoints(clusterId), clusterId, regionId, scenarioId) )
					{
						pspoint = &region->GetClusteredPoint(clusterId, pointId);
					}
				}
				else
				{
					if(taskVerifyf(pointId < region->GetNumPoints(),"CScenarioPointRegion pointId %d, outside expected range %d. clusterId %d, regionId %d",pointId, region->GetNumPoints(), clusterId, regionId ))
					{
						pspoint = &region->GetPoint(pointId);
					}
				}
			}
		}
	}

	return pspoint;
}

//-------------------------------------------------------------------------
// base scenario class
//-------------------------------------------------------------------------
CTaskScenario::CTaskScenario( s32 iScenarioIndex, CScenarioPoint* pScenarioPoint, bool bManageUseCount )
: m_iScenarioIndex(iScenarioIndex)
, m_pScenarioPoint(NULL)
, m_iCachedWorldScenarioPointIndex(-1)
, m_waitingForEntityTimer(0)
, m_TaskFlags(0)
{
	if (pScenarioPoint)
	{
		SetHadValidEntity(pScenarioPoint->GetEntity()!=NULL);
	}

	// This is used by CreateQueriableState(), where the assumption previously was made that if
	// there was no scenario entity, the CScenario would be in the world's Scenario store.
	// This is not the case for CTaskAmbulancePatrol, so using a separate variable allows us
	// to make an exception.
	SetIsWorldScenario(!GetHadValidEntity());

	if (bManageUseCount)
	{
		m_TaskFlags.SetFlag(TS_ManageUseCount);
	}

	SetScenarioPoint(pScenarioPoint);

	// catch case where we have no entity pointer but a Scenario attached to that entity
	//Assert(!(!pScenarioEntity && pScenario && !CWorldPoints::IsWorldPoint(pScenario)));
#if __BANK
	ValidateScenarioType(iScenarioIndex);
#endif
}

CTaskScenario::CTaskScenario( const CTaskScenario& other) 
: m_iScenarioIndex(other.m_iScenarioIndex)
, m_pScenarioPoint(NULL)				// Need to go through SetScenarioPoint() to update the use count properly.
, m_iCachedWorldScenarioPointIndex(-1)	// Will get updated later.
, m_waitingForEntityTimer(other.m_waitingForEntityTimer)
, m_TaskFlags(other.m_TaskFlags)
{
	SetScenarioPoint(other.m_pScenarioPoint);
#if __BANK
	ValidateScenarioType(other.m_iScenarioIndex);
#endif
}

CTaskScenario::~CTaskScenario()
{
	RemoveScenarioPoint();
}



bool CTaskScenario::IsValidForMotionTask(CTaskMotionBase& task) const
{
	bool isValid = false;
	if (task.IsOnFoot())
	{
		isValid = true;
	}
	else if( task.IsInWater() )
	{
		if (m_pScenarioPoint && m_pScenarioPoint->IsInWater())
		{
			isValid = true;
		}
	}

	return isValid;
}


const CScenarioInfo& CTaskScenario::GetScenarioInfo() const
{
	return *SCENARIOINFOMGR.GetScenarioInfoByIndex(GetScenarioType());
}


void CTaskScenario::SetScenarioPoint(CScenarioPoint* pScenarioPoint)
{
	if(pScenarioPoint != m_pScenarioPoint)
	{
		RemoveScenarioPoint();
		m_pScenarioPoint = pScenarioPoint;
		if (m_pScenarioPoint && m_TaskFlags.IsFlagSet(TS_ManageUseCount))
		{
			m_pScenarioPoint->AddUse();
		}

		// Clear out any previously cached index, GetWorldScenarioPointIndex() will
		// recompute the next time it's called, if applicable.
		// Don't do this if the point is NULL, as it may be needed to reconstruct the point later for cloned peds.
		if (pScenarioPoint)
		{
			m_iCachedWorldScenarioPointIndex = -1;
		}
	}
}

void CTaskScenario::RemoveScenarioPoint()
{
    if( m_pScenarioPoint )
    {
		if (m_TaskFlags.IsFlagSet(TS_ManageUseCount))
		{
			m_pScenarioPoint->RemoveUse();
		}

        m_pScenarioPoint = NULL;
    }
}

u32 CTaskScenario::GetAmbientFlags()
{
	u32 return_value = CTaskAmbientClips::Flag_CanSayAudioDuringLookAt;
	
	if (GetScenarioInfo().GetIsFlagSet(CScenarioInfoFlags::AllowConversations) && !GetPed()->PopTypeIsMission())
	{
		return_value |= CTaskAmbientClips::Flag_AllowsConversation;
	}

	if (GetScenarioInfo().GetIsFlagSet(CScenarioInfoFlags::BreakForNearbyPlayer))
	{
		return_value |= CTaskAmbientClips::Flag_InBreakoutScenario;
	}

	if( GetCreateProp() )
	{
		// Only create a prop the first time through
		SetCreateProp(false);
		return_value |= CTaskAmbientClips::Flag_PickPermanentProp;
	}
	return return_value;
}

CTaskInfo*	CTaskScenario::CreateQueriableState() const
{
	// default to non-networked version, networked versions must create a CClonedScenarioInfo
	return rage_new CNonNetworkedClonedScenarioInfo(static_cast<s16>(GetScenarioType()), GetScenarioPoint(), GetIsWorldScenario(), GetWorldScenarioPointIndex());
}

void CTaskScenario::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	Assertf((pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_USE_SCENARIO) ||
			(pTaskInfo->GetTaskInfoType() >= CTaskInfo::INFO_TYPE_SCENARIO && pTaskInfo->GetTaskInfoType() < CTaskInfo::INFO_TYPE_SCENARIO_END),
			"Task Type  %d", pTaskInfo->GetTaskInfoType());
	CClonedScenarioFSMInfoBase *CScenarioInfo = dynamic_cast<CClonedScenarioFSMInfoBase*>(pTaskInfo);

	Assert(CScenarioInfo);

	if(CScenarioInfo)
	{
		m_iScenarioIndex = CScenarioInfo->GetScenarioType();
#if __BANK
		ValidateScenarioType(m_iScenarioIndex);
#endif
		if (CScenarioInfo->GetIsWorldScenario())
		{
			// Check if the scenario point has changed, and if so, go through the
			// accessor to update the use count properly.
			CScenarioPoint* pNewPoint = CScenarioInfo->GetScenarioPoint();
			if(m_pScenarioPoint != pNewPoint)
			{
				SetScenarioPoint(pNewPoint);
			}
		}
	}

	CTaskFSMClone::ReadQueriableState(pTaskInfo);
}

CTask::FSM_Return CTaskScenario::ProcessPreFSM()
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	// Allow certain lod modes
	pPed->GetPedAiLod().ClearBlockedLodFlag(CPedAILod::AL_LodPhysics);
	pPed->GetPedAiLod().ClearBlockedLodFlag(CPedAILod::AL_LodMotionTask);

	// if this is a clone task, try and find the entity associated with the Scenario (if there should be one)
	if (pPed->IsNetworkClone() && !GetScenarioPoint())
	{
		CloneFindScenarioPoint();
	}

	return FSM_Continue;
}

void CTaskScenario::CloneFindScenarioPoint()
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	if(!pPed || !pPed->IsNetworkClone())
	{
		Assertf(0,"%s when calling CloneFindScenarioPoint",pPed?"Local ped":"Null ped");
		return;
	}

	CScenarioPoint* pScenarioPoint = NULL;

	CloneCheckScenarioPointValidAndGetPointer(&pScenarioPoint);
			
	if(pScenarioPoint!=NULL)
	{
		SetScenarioPoint(pScenarioPoint);
	}
}

//Checks the tasks scenario point data is valid and attempts to get the pointer if it has been streamed in.
bool CTaskScenario::CloneCheckScenarioPointValidAndGetPointer(CScenarioPoint** ppScenarioPoint)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	if(!pPed)
	{
		Assertf(0, "Null Ped when checking scenario point validity");
		return false;
	}

	// m_waitingForEntityTimer is used to avoid calling CPedPopulation::GetScenarioPointInArea every frame as it is very expensive
	if (m_waitingForEntityTimer > 0)
	{
		m_waitingForEntityTimer--;
	}
	else
	{
		// reset the counter so if any of the below fails, it will keep retrying...
		m_waitingForEntityTimer = WAITING_FOR_ENTITY_TIME;

		if (pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(GetTaskType()))
		{
			CTaskInfo* pTaskInfo = pPed->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(GetTaskType(), PED_TASK_PRIORITY_MAX);

			CClonedScenarioFSMInfoBase* pScenarioInfo = dynamic_cast<CClonedScenarioFSMInfoBase*>(pTaskInfo);

			Assert(pScenarioInfo);

			if (pScenarioInfo && (!pScenarioInfo->GetIsWorldScenario() || pScenarioInfo->GetScenarioId()!=-1) )
			{
				if( !pScenarioInfo->GetIsWorldScenario() )
				{
					CPedPopulation::GetScenarioPointInArea( pScenarioInfo->GetScenarioPosition(), 0.5f, 0, (s32*)&m_iScenarioIndex, &*ppScenarioPoint, true, pPed, false, true );
				}
				else if( pScenarioInfo->GetScenarioId()!=-1 )
				{
					*ppScenarioPoint = pScenarioInfo->GetScenarioPoint();
				}

				if(*ppScenarioPoint!=NULL)
				{
					// success, disable the timer as we've found what we need.
					SetWaitingForEntity(false);
				}		

				return true;
			}
		}
	}

	return false;
}


int CTaskScenario::GetWorldScenarioPointIndex() const
{
	// Use the cached value if we got it, or return whatever if this is not a world effect.
	if(m_iCachedWorldScenarioPointIndex != -1 || !GetIsWorldScenario())
	{
		// Already computed, or not a world point.
		return m_iCachedWorldScenarioPointIndex;
	}

	m_iCachedWorldScenarioPointIndex = -1;
	CScenarioPoint* pScenarioPoint = GetScenarioPoint();

	m_iCachedWorldScenarioPointIndex = GetWorldScenarioPointIndex(pScenarioPoint);

	return m_iCachedWorldScenarioPointIndex;
}

int CTaskScenario::GetWorldScenarioPointIndex(CScenarioPoint* pScenarioPoint, s32 *pCacheIndex /* = NULL */ , bool ASSERT_ONLY(bAssertOnWorldPointWithEntity)) const
{
	int iWorldScenarioPointIndex = -1;

	if(pCacheIndex && *pCacheIndex != -1)
	{
		taskAssertf(pScenarioPoint, "We should be passing a valid pScenarioPoint for this cache value 0x%x. %s ", *pCacheIndex, (GetEntity() && GetPed())?GetPed()->GetDebugName():"Null ped");
#if !__ASSERT	
		// Already computed 
		return *pCacheIndex;
#endif
	}

	Assertf(!bAssertOnWorldPointWithEntity || !pScenarioPoint || !pScenarioPoint->GetEntity(),"World scenario points should not have entities (at %.1f, %.1f, %.1f)!",
			pScenarioPoint->GetWorldPosition().GetXf(), pScenarioPoint->GetWorldPosition().GetYf(), pScenarioPoint->GetWorldPosition().GetZf());
	if(pScenarioPoint && !pScenarioPoint->GetEntity())
	{
		int count = SCENARIOPOINTMGR.GetNumRegions();
		
		// Assert on the count, to detect early if we may run out of bits to store the index
		// of any point.
		taskAssertf(count <= 0xff, "%s Not enough bits to store all scenario region indices.",(GetEntity() && GetPed())?GetPed()->GetDebugName():"Null ped");
		
		for (int r = 0; r < count; r++)
		{
			const CScenarioPointRegion* region = SCENARIOPOINTMGR.GetRegion(r);
			if (region)
			{
				int clusterId = -1;
				int pointId = -1;
				if (pScenarioPoint->IsInCluster())
				{
					region->FindClusteredPointIndex(*pScenarioPoint, clusterId, pointId);
				}
				else
				{
					region->FindPointIndex(*pScenarioPoint, pointId);
				}

				if (pointId != -1)
				{
					// Assert on the count, to detect early if we may run out of bits to store the index
					// of any point.
					taskAssertf(pointId <= 0xfff, "Not enough bits to store all scenario point indices. %s",(GetEntity() && GetPed())?GetPed()->GetDebugName():"Null ped");
					taskAssertf(clusterId <= 0xfff, "Not enough bits to store all scenario point clusters. %s",(GetEntity() && GetPed())?GetPed()->GetDebugName():"Null ped");
					
					//need to mask off some of the bits from the cluster ID ... 
					iWorldScenarioPointIndex = pointId | ((clusterId << 12) & 0x00fff000) |  r << 24;
					
					if(pCacheIndex)
					{
						taskAssertf(*pCacheIndex==-1 || *pCacheIndex == iWorldScenarioPointIndex, 
							"Unexpected cached value *pCacheIndex = %d, expected iWorldScenarioPointIndex 0x%x. %s", *pCacheIndex, iWorldScenarioPointIndex,(GetEntity() && GetPed())?GetPed()->GetDebugName():"Null ped");

						*pCacheIndex = iWorldScenarioPointIndex;
					}

					return iWorldScenarioPointIndex;
				}
			}
		}

#if __ASSERT
		char scenarioName[128] = { "" };
#if __BANK
		Color32 col;
		pScenarioPoint->FindDebugColourAndText(&col, scenarioName, sizeof(scenarioName));
#endif // __BANK
		Assertf(iWorldScenarioPointIndex != -1, "Not found scenario [%s] in scenario list. Ped %s. Point at %.1f %.1f %.1f Clustered: %d", scenarioName, (GetEntity() && GetPed())?GetPed()->GetDebugName():"Null ped",
				pScenarioPoint->GetWorldPosition().GetXf(), pScenarioPoint->GetWorldPosition().GetYf(), pScenarioPoint->GetWorldPosition().GetZf(), (int)pScenarioPoint->IsInCluster());
#endif // __ASSERT
	}

	return -1;
}

#if __BANK
void CTaskScenario::ValidateScenarioType(s32 iScenarioType)
{
	if(!Verifyf(iScenarioType != -1 && iScenarioType <= SCENARIOINFOMGR.GetScenarioCount(false), "Invalid scenario type, %i", iScenarioType))
	{
		aiDisplayf("m_iScenarioIndex is invalid, index = %i", iScenarioType);
		sysStack::PrintStackTrace();
	}

}
#endif