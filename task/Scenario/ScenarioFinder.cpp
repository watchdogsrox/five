//
// task/Scenario/ScenarioFinder.cpp
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "task/Default/AmbientAnimationManager.h"
#include "task/Motion/Locomotion/TaskFishLocomotion.h"
#include "task/Scenario/ScenarioFinder.h"
#include "task/Scenario/ScenarioChaining.h"	// CScenarioChainingEdge::eNavMode
#include "task/Scenario/ScenarioManager.h"
#include "task/Scenario/ScenarioPoint.h"
#include "task/Scenario/ScenarioPointManager.h"
#include "task/Scenario/info/ScenarioInfo.h"
#include "task/Scenario/Info/ScenarioInfoManager.h"
#include "ai/BlockingBounds.h"
#include "ai/ambient/AmbientModelSetManager.h"
#include "ai/task/task.h"					// For FSM_Begin, etc.
#include "game/Clock.h"
#include "Peds/ped.h"
#include "Peds/TaskData.h"
#include "Peds/pedpopulation.h"
#include "scene/2dEffect.h"

AI_OPTIMISATIONS()

int CScenarioFinder::PickRealScenarioTypeFromGroup(const int* realTypesToPickFrom, const float* probToPick, int numRealTypesToPickFrom, float probSum)
{
	int chosenWithinTypeGroupIndex = -1;
	if(numRealTypesToPickFrom)
	{
		if(numRealTypesToPickFrom == 1)
		{
			chosenWithinTypeGroupIndex = 0;
		}
		else
		{
			float p = g_ReplayRand.GetFloat()*probSum;
			chosenWithinTypeGroupIndex = numRealTypesToPickFrom - 1;
			for(int j = 0; j < numRealTypesToPickFrom - 1; j++)
			{
				if(p < probToPick[j])
				{
					chosenWithinTypeGroupIndex = j;
					break;
				}
				p -= probToPick[j];
			}
		}
	}
	if(chosenWithinTypeGroupIndex >= 0)
	{
		return realTypesToPickFrom[chosenWithinTypeGroupIndex];
	}
	else
	{
		return -1;
	}
}

//-----------------------------------------------------------------------------

CScenarioFinder::FindOptions::FindOptions()
		: m_FleeFromPos(V_ZERO)
		, m_DesiredScenarioClassId(0)
		, m_IgnorePoint(NULL)
		, m_pRequiredAttachEntity(NULL)
		, m_NoChainingNodesWithIncomingLinks(false)
		, m_bExcludeChainNodes(false)
		, m_bRandom(true)
		, m_bCheckBlockingAreas(true)
		, m_bCheckProbability(false)
		, m_bFindPropsInEnvironment(false)
		, m_bIgnoreTime(false)
		, m_bIgnoreHasPropCheck(false)
		, m_bIgnorePointsBehindPed(false)
		, m_bBiasToNearPlayer(false)
		, m_sIgnoreScenarioType(-1)
		, m_bDontIgnoreNoSpawnPoints(false)
		, m_bMustBeReusable(false)
		, m_bMustNotHaveStationaryReactionsEnabled(false)
		, m_bCheckApproachAngle(false)
{
}


CScenarioFinder::CScenarioFinder()
		: m_pScenarioPoint(NULL)
		, m_pPed(NULL)
		, m_iRealScenarioPointType(-1)
		, m_Action(CScenarioChainingEdge::kActionDefault)
		, m_NavMode(CScenarioChainingEdge::kNavModeDefault)
		, m_NavSpeed(CScenarioChainingEdge::kNavSpeedDefault)
		, m_uInformationFlags(0)
		, m_bHoldsUseCountOnScenario(false)
		, m_bIsActive(false)
		, m_bNewSearch(false)
		, m_bSearchForNextInChain(false)
{
}


CScenarioFinder::~CScenarioFinder()
{
	SetScenarioPoint(NULL, -1);
}


void CScenarioFinder::SetScenarioPoint(CScenarioPoint* pScenarioPoint, int realScenarioType, bool increaseUseCount)
{
    if (m_pScenarioPoint != pScenarioPoint || increaseUseCount != m_bHoldsUseCountOnScenario)
    {
        if (m_pScenarioPoint && m_bHoldsUseCountOnScenario)
        {
            m_pScenarioPoint->RemoveUse();
        }
		m_bHoldsUseCountOnScenario = false;

        if (pScenarioPoint && increaseUseCount)
        {
            pScenarioPoint->AddUse();
			m_bHoldsUseCountOnScenario = true;
        }

        m_pScenarioPoint = pScenarioPoint;
    }

	m_iRealScenarioPointType = realScenarioType;
}

void CScenarioFinder::Reset()
{
	SetScenarioPoint(NULL, -1);
	m_Action = CScenarioChainingEdge::kActionDefault;
	m_NavMode = CScenarioChainingEdge::kNavModeDefault;
	m_NavSpeed = CScenarioChainingEdge::kNavSpeedDefault;
	m_pPed = NULL;
	m_bIsActive = false;
	m_bNewSearch = false;
	m_bSearchForNextInChain = false;
}


void CScenarioFinder::StartSearch(CPed& ped, const FindOptions& opts)
{
	Reset();

	m_bIsActive = true;
	m_pPed = &ped;
	m_bNewSearch = true;
	m_bSearchForNextInChain = false;
	m_FindOptions = opts;
	m_uInformationFlags.ClearAllFlags();
}


void CScenarioFinder::StartSearchForNextInChain(CPed& ped, const FindOptions& opts)
{
	Assert(!m_pPed || m_pPed == &ped);

	m_bIsActive = true;
	m_pPed = &ped;
	m_bSearchForNextInChain = true;
	m_FindOptions = opts;
	m_uInformationFlags.ClearAllFlags();
}


CTaskHelperFSM::FSM_Return CScenarioFinder::ProcessPreFSM()
{
	return CTaskHelperFSM::ProcessPreFSM();
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	FindNewScenario
// PURPOSE:		Find a scenario that the supplied ped is allowed to use, within the range of the center pos
// PARAMS:		Ped in question, the center point to look from, the range to look within
// RETURN:		A valid CScenarioInfo, if one is found
CScenarioPoint* CScenarioFinder::FindNewScenario(const CPed* pPed, const Vector3& vCenterPos, float fRange, const FindOptions& opts, int& realScenarioTypeOut)
{
	realScenarioTypeOut = -1;

	bool bRandom = opts.m_bRandom;
    CScenarioPoint* areaPoints[1000];
    const s32 effectCount = (s32)SCENARIOPOINTMGR.FindPointsInArea(RCC_VEC3V(vCenterPos), ScalarV(fRange), areaPoints, NELEM(areaPoints));

	float fClosestDistanceSq = 0.0f;
	CScenarioPoint* retSpoint = NULL;

	CScenarioChainSearchResults* apValidScenarioPoints = Alloca(CScenarioChainSearchResults, effectCount);
	u16* aValidScenarioPointTypes = Alloca(u16, effectCount);
	s32 iNumStored2dEffects = 0;

	//Randomize the points
	//Completely random	- not requested so possibly not really needed so not in....
	//for (int i=0 ; i<effectCount ; i++)
	//{
	//	const s32 rndIndex = fwRandom::GetRandomNumberInRange(0, effectCount);
	//	CScenarioPoint* pTmp = areaPoints[i];
	//	areaPoints[i] = areaPoints[rndIndex];
	//	areaPoints[rndIndex] = pTmp;
	//}

	int areaPointsIndices[1000];
	for (int i=0 ; i<effectCount ; i++)
	{
		areaPointsIndices[i] = i;
	}

	//Potentially sorted towards being closer to the player
	if (opts.m_bBiasToNearPlayer)
	{
		CPed *pPlayerPed = FindPlayerPed();
		if (pPlayerPed)
		{
			//Grab the camera position
			Vec3V vBiasPos = pPlayerPed->GetTransform().GetPosition();

			//Biased closer to the case - only swap towards closer distances
			for (int i=0 ; i<effectCount ; i++)
			{
				int pntTmp = areaPointsIndices[i];
				const s32 rndIndex = fwRandom::GetRandomNumberInRange(0, effectCount-1);
		
				ScalarV distI = Dist(vBiasPos, areaPoints[areaPointsIndices[i]]->GetPosition());
				ScalarV distRnd = Dist(vBiasPos, areaPoints[areaPointsIndices[rndIndex]]->GetPosition());
				if (	IsGreaterThan(distI, distRnd).Getb()
					^	(i > rndIndex)
					)
				{
					areaPointsIndices[i] = areaPointsIndices[rndIndex];
					areaPointsIndices[rndIndex] = pntTmp;
				}			
			}
		}
	}

	for(s32 k = 0; k < effectCount; ++k)
	{
		CScenarioPoint *pPoint = areaPoints[ areaPointsIndices[ k ] ];

		if(pPoint == opts.m_IgnorePoint)
		{
			// We can't use this point, perhaps because the user just came from it.
			continue;
		}
		
		if(opts.m_pRequiredAttachEntity && (opts.m_pRequiredAttachEntity != pPoint->GetEntity()))
		{
			continue;
		}

		if(opts.m_bMustNotHaveStationaryReactionsEnabled && pPoint->IsFlagSet(CScenarioPointFlags::StationaryReactions))
		{
			continue;
		}
		
		Vec3V effectPosV = pPoint->GetPosition();
		const Vector3 effectPos = VEC3V_TO_VECTOR3(effectPosV);

		// get the distance from the player to the attractor
		const float fDistanceSq = vCenterPos.Dist2(effectPos);
		if( fDistanceSq > rage::square(fRange) )
		{
			continue;
		}

		if(opts.m_bIgnorePointsBehindPed)
		{
			Vector3 vFromPedToPoint = effectPos - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			vFromPedToPoint.Normalize();
			
			const Vector3 vPedDir = VEC3V_TO_VECTOR3(pPed->GetTransform().GetB());
			static float MAX_DOT_TO_ATTRACT_PED = 0.707f;
			if(vPedDir.Dot(vFromPedToPoint) < MAX_DOT_TO_ATTRACT_PED)
			{
				continue;
			}
		}

		if(opts.m_bCheckBlockingAreas && CScenarioBlockingAreas::IsPointInsideBlockingAreas(effectPos, true, false))
		{
			continue;
		}

		const int scenarioTypeVirtualOrReal = pPoint->GetScenarioTypeVirtualOrReal();
		const int numRealTypes = SCENARIOINFOMGR.GetNumRealScenarioTypes(scenarioTypeVirtualOrReal);
		taskAssert(numRealTypes <= CScenarioInfoManager::kMaxRealScenarioTypesPerPoint);
		int realTypesToPickFrom[CScenarioInfoManager::kMaxRealScenarioTypesPerPoint];
		float probToPick[CScenarioInfoManager::kMaxRealScenarioTypesPerPoint];
		int numRealTypesToPickFrom = 0;
		float probSum = 0.0f;
		for(int j = 0; j < numRealTypes; j++)
		{
			if(!CheckCanScenarioBeUsed(*(pPoint), *pPed, opts, j, NULL, CScenarioChainingEdge::kMove))
			{
				continue;
			}

			//PF_START(CollisionChecks);
			s32 scenarioType = SCENARIOINFOMGR.GetRealScenarioType(scenarioTypeVirtualOrReal, j);
			bool checkMaxInRange = !pPoint->IsFlagSet(CScenarioPointFlags::IgnoreMaxInRange);
			if(!CScenarioManager::CheckScenarioPopulationCount(scenarioType, effectPos, CPedPopulation::GetClosestAllowedScenarioSpawnDist(), pPed, checkMaxInRange))
			{
				//PF_STOP(CollisionChecks);
				continue;
			}

// Disabled for now, it didn't actually have any effect!
// TODO: If needed, maybe repair and re-enable this, looking at how GeneratePedsFromScenarioPointList()
// does the same thing.
#if 0
			// Check this position is free of objects (ignoring non-uprooted and this object)
			CEntity* pBlockingEntities[MAX_COLLIDED_OBJECTS] = {NULL, NULL, NULL, NULL, NULL, NULL};
			if(!CPedPlacement::IsPositionClearForPed(effectPos, PED_HUMAN_RADIUS, true, MAX_COLLIDED_OBJECTS, pBlockingEntities, true, false, true))
			{
				for(s32 i = 0; pBlockingEntities[i] != NULL && i < MAX_COLLIDED_OBJECTS; ++i)
				{
					// Ignore this entity
					if(pBlockingEntities[i] == pAttachedEntity)
					{
						continue;
					}
					//can only ignore objects
					if(!pBlockingEntities[i]->GetIsTypeObject())
					{
						break;
					}
					// Ignore doors, for qnan matrix problem
					if(((CObject*)pBlockingEntities[i])->IsADoor())
					{
						continue;
					}
					// Ignore non-uprooted objects
					if(((CObject*)pBlockingEntities[i])->m_nObjectFlags.bHasBeenUprooted)
					{
						break;
					}
				}
			}
#endif	// 0

			const float prob = SCENARIOINFOMGR.GetRealScenarioTypeProbability(scenarioTypeVirtualOrReal, j);
			realTypesToPickFrom[numRealTypesToPickFrom] = SCENARIOINFOMGR.GetRealScenarioType(scenarioTypeVirtualOrReal, j);
			probToPick[numRealTypesToPickFrom] = prob;
			probSum += prob;
			numRealTypesToPickFrom++;
		}

		int chosenRealTypeIndex = PickRealScenarioTypeFromGroup(realTypesToPickFrom, probToPick, numRealTypesToPickFrom, probSum);

		if(chosenRealTypeIndex < 0)
		{
			continue;
		}

		if(chosenRealTypeIndex == opts.m_sIgnoreScenarioType)
		{
			// We can't use this point, perhaps because the user just came from one like it.
			continue;
		}

		//if (CScenarioManager::IsVehicleScenarioType(chosenRealTypeIndex)) //Vehicles only right now ... 
		{
			if (pPoint->IsChained() && pPoint->IsUsedChainFull())
			{
				continue;
			}
		}

		//PF_STOP(CollisionChecks);

		if(bRandom)
		{
			apValidScenarioPoints[iNumStored2dEffects].m_Point = pPoint;	
			apValidScenarioPoints[iNumStored2dEffects].m_Action = CScenarioChainingEdge::kActionDefault;
			apValidScenarioPoints[iNumStored2dEffects].m_NavMode = CScenarioChainingEdge::kNavModeDefault;
			apValidScenarioPoints[iNumStored2dEffects].m_NavSpeed = CScenarioChainingEdge::kNavSpeedDefault;
			taskAssert(chosenRealTypeIndex <= 0xffff);
			aValidScenarioPointTypes[iNumStored2dEffects] = (u16)chosenRealTypeIndex;
			++iNumStored2dEffects;
			if( iNumStored2dEffects == effectCount )
			{
				break;
			}
		}
		else
		{
			if((retSpoint == NULL) || (fDistanceSq < fClosestDistanceSq))
			{
				// Check some additional conditions before accepting the point.
				// Since these could potentially be expensive it would perhaps be better to only
				// do this until we really know it's the the closest point of all, but then,
				// if it fails, we wouldn't have a good way of picking the next closest point instead.
				// May be worth changing the algorithm to keep track of all the points if this becomes
				// a common case.
				if(CheckFinalConditions(*pPoint, chosenRealTypeIndex, opts))
				{
					retSpoint = pPoint;
					realScenarioTypeOut = chosenRealTypeIndex;
					fClosestDistanceSq = fDistanceSq;
				}
			}
		}
	}
	if(bRandom)
	{
		int scenarioType = -1;
		u8 action = 0, navMode = 0, navSpeed = 0;
		CScenarioPoint* pt = MakeRandomChoiceFromArrays(apValidScenarioPoints, aValidScenarioPointTypes, iNumStored2dEffects, scenarioType, action, navMode, navSpeed, opts);
		if(pt)
		{
			realScenarioTypeOut = scenarioType;
			return pt;
		}
		return NULL;
	}
	else
	{
		return retSpoint;
	}
}


bool CScenarioFinder::CheckCanScenarioBeUsed(CScenarioPoint& scenarioPoint, const CPed& ped, const FindOptions& opts, int indexWithinTypeGroup, bool* pMayBecomeValidSoonOut, int edgeAction)
{
	//Initialize the output parameters.
	if(pMayBecomeValidSoonOut)
	{
		*pMayBecomeValidSoonOut = false;
	}
	
	// ignore if flagged as obstructed
	if(scenarioPoint.IsObstructedAndReset())
	{
		return false;
	}

	//If we don't care about whether the scenario will become valid soon,
	//check the usage first, since the check is fast.  If we do care,
	//this will be checked last.
	//Note: if we are planning on moving into a vehicle as a passenger, we don't
	//car about this, it will be the driver that should increase the use count.
	if(!pMayBecomeValidSoonOut && edgeAction != CScenarioChainingEdge::kMoveIntoVehicleAsPassenger)
	{
		// check if our scenario is already in use
		if( CPedPopulation::IsEffectInUse(scenarioPoint) )
		{
			return false;
		}
	}

	if(!CScenarioManager::CheckScenarioPointEnabled(scenarioPoint, indexWithinTypeGroup))
	{
		return false;
	}

	if(!opts.m_bIgnoreTime && scenarioPoint.HasTimeOverride() && !CClock::IsTimeInRange(scenarioPoint.GetTimeStartOverride(), scenarioPoint.GetTimeEndOverride()))
	{
		return false;
	}

	const int scenarioTypeVirtualOrReal = scenarioPoint.GetScenarioTypeVirtualOrReal();
	const s32 scenarioType = SCENARIOINFOMGR.GetRealScenarioType(scenarioTypeVirtualOrReal, indexWithinTypeGroup);
	if(opts.m_bCheckProbability)
	{
		if(!CScenarioManager::CheckScenarioProbability(&scenarioPoint, scenarioType))
		{
			return false;
		}
	}

	if(!CScenarioManager::CheckScenarioPointMapData(scenarioPoint))
	{
		return false;
	}

	if (CScenarioManager::IsScenarioPointAttachedEntityUprootedOrBroken(scenarioPoint))
	{
		return false;
	}

	const CScenarioInfo* pScenarioInfo = SCENARIOINFOMGR.GetScenarioInfoByIndex(scenarioType);

	if(!pScenarioInfo)
	{
		return false;
	}

	if (opts.m_bCheckApproachAngle && pScenarioInfo->GetIsFlagSet(CScenarioInfoFlags::RejectApproachIfTooSteep))
	{
		// Reject certain scenario points if the angle to approach is too steep.
		Vec3V vScenario = scenarioPoint.GetWorldPosition();
		Vec3V vPed = ped.GetTransform().GetPosition();
		const float fZDiff = vPed.GetZf() - vScenario.GetZf();
		const float fXYMag = DistXY(vScenario, vPed).Getf();
		const float sf_TooSteepTolerance = Tanf(CTaskFishLocomotion::ms_fBigFishMaxPitch);

		if (fXYMag < SMALL_FLOAT || fabs(fZDiff) / fabs(fXYMag) > fabs(sf_TooSteepTolerance))
		{
			return false;
		}
	}

	if(!pScenarioInfo->GetFlags().BitSet().IsSuperSetOf(opts.m_RequiredScenarioInfoFlags.BitSet()))
	{
		// The required scenario info flags were not matched.
		return false;
	}

	if (pScenarioInfo->GetBlockedModelSet() && ped.GetPedModelInfo())
	{
		u32 uModelHash = ped.GetPedModelInfo()->GetModelNameHash();
		if(pScenarioInfo->GetBlockedModelSet()->GetContainsModel(uModelHash))
		{
			return false;
		}
	}

	if (!opts.m_bDontIgnoreNoSpawnPoints)
	{
		// If the attached entity doesn't spawn peds and it's not being overriden, OR the point itself is set to NoSpawn, don't allow this point.  [1/10/2013 mdawe]
		if ((scenarioPoint.GetEntity() && (!scenarioPoint.GetEntity()->m_nFlags.bWillSpawnPeds && !scenarioPoint.IsEntityOverridePoint())) || scenarioPoint.IsFlagSet(CScenarioPointFlags::NoSpawn))
		{
			//...unless you're chaining to it
			if (!SCENARIOPOINTMGR.IsChainedWithIncomingEdges(scenarioPoint))
			{
				return false;
			}
		}
	}

	//Create the condition data.
	CScenarioCondition::sScenarioConditionData conditionData;
	conditionData.pPed = &ped;

	if(opts.m_bIgnoreTime)
	{
		conditionData.iScenarioFlags |= SF_IgnoreTime;
	}

	if (opts.m_bIgnoreHasPropCheck)
	{
		conditionData.iScenarioFlags |= SF_IgnoreHasPropCheck;
	}
	
	//Check the condition.
	if(pScenarioInfo->GetCondition() && !pScenarioInfo->GetCondition()->Check(conditionData))
	{
		return false;
	}

	const u32 uDesiredScenario = opts.m_DesiredScenarioClassId;

	// We want to check to see if this is the desired scenario type and if the desired is not flee then we will not accept a flee scenario
	if(uDesiredScenario != 0 && !pScenarioInfo->GetIsClassId(uDesiredScenario))
	{
		return false;
	}

	if(uDesiredScenario == 0 && pScenarioInfo->GetIsClass<CScenarioFleeInfo>())
	{
		return false;
	}

	// Unless the user specifically requested a CScenarioLookAtInfo, reject it.
	if (uDesiredScenario == 0 && pScenarioInfo->GetIsClass<CScenarioLookAtInfo>())
	{
		return false;
	}

	if (opts.m_bMustBeReusable && pScenarioInfo->GetIsFlagSet(CScenarioInfoFlags::CantBeReused))
	{
		return false;
	}


	// Get the model set we are trying to use, either from the scenario point or from the scenario info.
	const CAmbientPedModelSet* pModelSet = NULL;
	unsigned iModelSetIndex = CScenarioPointManager::kNoCustomModelSet;
	if(!pScenarioInfo->GetIsClass<CScenarioVehicleInfo>())
	{
		iModelSetIndex = scenarioPoint.GetModelSetIndex();
	}
	if(iModelSetIndex != CScenarioPointManager::kNoCustomModelSet)
	{
		pModelSet = CAmbientModelSetManager::GetInstance().GetPedModelSet(iModelSetIndex);
	}
	else
	{
		pModelSet = pScenarioInfo->GetModelSet();
	}

	// For now we have to verify that the scenario is restricted to this ped type, otherwise we might have
	// animals trying to use human scenarios and vice versa
	if(pModelSet)
	{
		u32 uPedModelHash = ped.GetBaseModelInfo()->GetHashKey();

		if(!pModelSet->GetContainsModel(uPedModelHash))
		{
			return false;
		}
	}
	else
	{
		const bool useScenariosWithNoModelSet = !ped.GetTaskData().GetIsFlagSet(CTaskFlags::DisableUseScenariosWithNoModelSet);

		if(!useScenariosWithNoModelSet)
		{
			return false;
		}
	}

	// if we are finding a flee scenario and have a flee source position, make sure the scenario is far enough away
	if(uDesiredScenario == CScenarioFleeInfo::GetStaticClassId())
	{
		const Vector3 effectPos = VEC3V_TO_VECTOR3(scenarioPoint.GetPosition());

		const float fDistanceFromSourceSq = RCC_VECTOR3(opts.m_FleeFromPos).Dist2(effectPos);
		const float fMinRange = ((CScenarioFleeInfo*)pScenarioInfo)->GetSafeRadius();
		if(fDistanceFromSourceSq < (fMinRange * fMinRange))
		{
			return false;
		}
	}

	if(opts.m_NoChainingNodesWithIncomingLinks)
	{
		if (SCENARIOPOINTMGR.IsChainedWithIncomingEdges(scenarioPoint))
		{
			return false;
		}
	}

	if (opts.m_bExcludeChainNodes && scenarioPoint.IsChained())
	{
		return false;
	}

	// Check if the bulky item flag is set on the scenario.
	if(pScenarioInfo->GetIsFlagSet(CScenarioInfoFlags::NoBulkyItems))
	{
		// Check if the bulky item flag is set on any of the variation or prop components
		// of the ped. If so, we can't use this scenario. Note: this may be somewhat slow,
		// which is why it's one of the last checks we make.
		if(CPedPropsMgr::DoesPedHavePropOrComponentWithFlagSet(ped, PV_FLAG_BULKY))
		{
			return false;
		}
	}

	if (!pScenarioInfo->GetConditionalAnimsGroup().CheckForMatchingConditionalAnims(conditionData))
	{
		// None of the conditional animations sets of the scenario matched the ped.
		// Saving this check for last as it is rather expensive potentially.
		return false;
	}
	
	if(pMayBecomeValidSoonOut)
	{
		//The scenario has passed all other conditions.
		//Even if it is occupied, it still may become available shortly.
		*pMayBecomeValidSoonOut = true;

		// Check if our scenario is already in use.
		// Note: if we are planning on moving into a vehicle as a passenger, we don't
		// car about this, it will be the driver that should increase the use count.
		if(edgeAction != CScenarioChainingEdge::kMoveIntoVehicleAsPassenger && CPedPopulation::IsEffectInUse(scenarioPoint))
		{
			return false;
		}
	}

	return true;
}

CTaskHelperFSM::FSM_Return CScenarioFinder::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Waiting)
			FSM_OnUpdate
				return Waiting_OnUpdate();

		FSM_State(State_SearchForScenario)
			FSM_OnEnter
				SearchForScenario_OnEnter();
			FSM_OnUpdate
				return SearchForScenario_OnUpdate();

		FSM_State(State_FoundScenario)
			FSM_OnEnter
				FoundScenario_OnEnter();
			FSM_OnUpdate
				return FoundScenario_OnUpdate();

		FSM_State(State_SearchForNextScenarioInChain)
			FSM_OnEnter
				SearchForNextScenarioInChain_OnEnter();
			FSM_OnUpdate
				return SearchForNextScenarioInChain_OnUpdate();
	FSM_End
}


void CScenarioFinder::CleanUp()
{
}


CTaskHelperFSM::FSM_Return CScenarioFinder::Waiting_OnUpdate()
{
	if(m_bSearchForNextInChain)
	{
		SetState(State_SearchForNextScenarioInChain);
	}
	else if(m_bNewSearch)
	{
		SetState(State_SearchForScenario);
	}

	return FSM_Continue;
}


void CScenarioFinder::SearchForScenario_OnEnter()
{
	m_bNewSearch = false;
}


CTaskHelperFSM::FSM_Return CScenarioFinder::SearchForScenario_OnUpdate()
{
	if(!m_bIsActive)
	{
		Reset();
		SetState(State_Waiting);
		return FSM_Continue;
	}

	CPed& ped = *m_pPed;
	int realScenarioType = -1;
	CScenarioPoint* pSPoint = NULL;
	
	if (!CScenarioManager::IsScenarioAttractionSuppressed())
	{
		pSPoint = FindNewScenario(&ped, VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition()), ped.GetTaskData().GetScenarioAttractionDistance(), m_FindOptions, realScenarioType);
	}

	if(pSPoint)
	{
		m_Action = CScenarioChainingEdge::kActionDefault;
		m_NavMode = CScenarioChainingEdge::kNavModeDefault;
		m_NavSpeed = CScenarioChainingEdge::kNavSpeedDefault;
		SetScenarioPoint(pSPoint, realScenarioType);
		SetState(State_FoundScenario);
	}
	else
	{
		// Currently, I don't think FindNewScenario() is likely to succeed if it just failed,
		// because it tends to look at all nearby points. So, this is considered a failure,
		// and it's up to the user to decide when to try again.
		Reset();
		SetState(State_Waiting);
	}

	return FSM_Continue;
}


void CScenarioFinder::FoundScenario_OnEnter()
{
	m_bIsActive = false;
}


CTaskHelperFSM::FSM_Return CScenarioFinder::FoundScenario_OnUpdate()
{
	if(m_bNewSearch || !m_pScenarioPoint)
	{
		SetState(State_Waiting);
		return FSM_Continue;
	}

	if(m_bSearchForNextInChain)
	{
		SetState(State_SearchForNextScenarioInChain);
		return FSM_Continue;
	}

	return FSM_Continue;
}


void CScenarioFinder::SearchForNextScenarioInChain_OnEnter()
{
	m_bSearchForNextInChain = false;
}


CTaskHelperFSM::FSM_Return CScenarioFinder::SearchForNextScenarioInChain_OnUpdate()
{
	if(!m_bIsActive)
	{
		Reset();
		SetState(State_Waiting);
		return FSM_Continue;
	}

	int numPts = 0;
	static const int kMaxPts = 16;
	u16 realScenarioTypeArray[kMaxPts];
	CScenarioChainSearchResults searchResultsArray[kMaxPts];
	if(m_pScenarioPoint)
	{
		numPts = SCENARIOPOINTMGR.FindChainedScenarioPoints(*m_pScenarioPoint.Get(), searchResultsArray, NELEM(searchResultsArray));
	}

	CPed& ped = *m_pPed;
	for(int i = 0; i < numPts; i++)
	{
		// TODO: It may be better to pick a random point, use CheckCanScenarioBeUsed(),
		// and if it returns true, that's what we use. If it returns false, remove it
		// and repeat. That way, we wouldn't have to call CheckCanScenarioBeUsed() as many times.

		CScenarioPoint* pt = searchResultsArray[i].m_Point;

		const int scenarioTypeVirtualOrReal = pt->GetScenarioTypeVirtualOrReal();
		const int numRealTypes = SCENARIOINFOMGR.GetNumRealScenarioTypes(scenarioTypeVirtualOrReal);
		taskAssert(numRealTypes <= CScenarioInfoManager::kMaxRealScenarioTypesPerPoint);
		int realTypesToPickFrom[CScenarioInfoManager::kMaxRealScenarioTypesPerPoint];
		float probToPick[CScenarioInfoManager::kMaxRealScenarioTypesPerPoint];
		int numRealTypesToPickFrom = 0;
		float probSum = 0.0f;
		for(int j = 0; j < numRealTypes; j++)
		{
			bool bMayBecomeValidSoon = false;
			if(CheckCanScenarioBeUsed(*pt, ped, m_FindOptions, j, &bMayBecomeValidSoon, searchResultsArray[i].m_Action))
			{
				const float prob = SCENARIOINFOMGR.GetRealScenarioTypeProbability(scenarioTypeVirtualOrReal, j);

				realTypesToPickFrom[numRealTypesToPickFrom] = SCENARIOINFOMGR.GetRealScenarioType(scenarioTypeVirtualOrReal, j);
				probToPick[numRealTypesToPickFrom] = prob;
				probSum += prob;
				numRealTypesToPickFrom++;
			}
			
			//Check if the scenario may become valid soon.
			if(bMayBecomeValidSoon)
			{
				m_uInformationFlags.SetFlag(IF_ChainedScenarioMayBecomeValidSoon);
			}
		}

		const int chosenRealTypeIndex = PickRealScenarioTypeFromGroup(realTypesToPickFrom, probToPick, numRealTypesToPickFrom, probSum);
		if(chosenRealTypeIndex >= 0)
		{
			taskAssert(chosenRealTypeIndex <= 0xffff);
			realScenarioTypeArray[i] = (u16)chosenRealTypeIndex;
		}
		else
		{
			// Not able or allowed to use this point, remove it from
			// the arrays.
			numPts--;
			searchResultsArray[i] = searchResultsArray[numPts];
			realScenarioTypeArray[i] = realScenarioTypeArray[numPts];
			i--;
		}
	}

	int scenarioType = -1;
	u8 action = 0, navMode = 0, navSpeed = 0;
	CScenarioPoint* pt = MakeRandomChoiceFromArrays(searchResultsArray, realScenarioTypeArray, numPts, scenarioType, action, navMode, navSpeed, m_FindOptions);
	if(pt)
	{
		bool increaseUseCount = (action != CScenarioChainingEdge::kMoveIntoVehicleAsPassenger);
		SetScenarioPoint(pt, scenarioType, increaseUseCount);
		m_Action = action;
		m_NavMode = navMode;
		m_NavSpeed = navSpeed;

		SetState(State_FoundScenario);
		return FSM_Continue;
	}

	Reset();
	SetState(State_Waiting);
	return FSM_Continue;
}


bool CScenarioFinder::CheckFinalConditions(const CScenarioPoint& pt, int realScenarioType, const FindOptions& opts)
{
	if(opts.m_bFindPropsInEnvironment)
	{
		CObject* pPropInEnvironment = NULL;
		bool allowUprooted = false;
		if(!CScenarioManager::FindPropsInEnvironmentForScenario(pPropInEnvironment, pt, realScenarioType, allowUprooted))
		{
			return false;
		}
	}

	return true;
}


CScenarioPoint* CScenarioFinder::MakeRandomChoiceFromArrays(CScenarioChainSearchResults* searchResultsArray
														   , u16* realScenarioTypeArray
														   , int numPoints
														   , int& realScenarioTypeOut
														   , u8& actionOut
														   , u8& navModeOut
														   , u8& navSpeedOut
														   , const FindOptions& opts)
{
	// Note: this function is destructive in the sense of removing entries from the arrays
	// if they can't be used.

	int numAttempts = 0;
	const int maxAttempts = kMaxAttemptsToFindPropsInEnvironment;
	while(numPoints > 0 && numAttempts < maxAttempts)
	{
		numAttempts++;

		const int chosenIndex = g_ReplayRand.GetRanged(0, numPoints - 1);

		CScenarioPoint* pt = searchResultsArray[chosenIndex].m_Point;
		const int realScenarioType = realScenarioTypeArray[chosenIndex];

		taskAssert(pt);
		if(!CheckFinalConditions(*pt, realScenarioType, opts))
		{
			// Remove the point that didn't pass the conditions, from all of the arrays.
			numPoints--;
			searchResultsArray[chosenIndex] = searchResultsArray[numPoints];
			realScenarioTypeArray[chosenIndex] = realScenarioTypeArray[numPoints];
			continue;
		}

		realScenarioTypeOut = realScenarioTypeArray[chosenIndex];
		actionOut = searchResultsArray[chosenIndex].m_Action;
		navModeOut = searchResultsArray[chosenIndex].m_NavMode;
		navSpeedOut = searchResultsArray[chosenIndex].m_NavSpeed;
		return pt;
	}

	return NULL;
}

//-----------------------------------------------------------------------------

// End of file 'task/Scenario/ScenarioFinder.cpp'
