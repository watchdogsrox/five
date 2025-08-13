//
// task/Combat/Cover/coverfilter.cpp
//
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#include "task/Combat/Cover/coverfilter.h"
#include "task/Combat/TaskNewCombat.h"
#include "task/Movement/TaskMoveWithinAttackWindow.h"
#include "Peds/PedIntelligence.h"

AI_OPTIMISATIONS()
AI_COVER_OPTIMISATIONS()

//-----------------------------------------------------------------------------

CCoverPointFilterBase::CCoverPointFilterBase(CPed& ped)
		: m_pPed(&ped)
		, m_pDefensiveArea(NULL)
		, m_scMinDistanceToTargetSq(V_ZERO)
		, m_bMaintainMinDistanceToTarget(false)
		, m_bIsTargetInDefensiveArea(false)
		, m_bCanCheckAttackWindows(true)
{
	CPedIntelligence& intel = *ped.GetPedIntelligence();

	CDefensiveArea* pDefensiveArea = intel.GetDefensiveAreaForCoverSearch();
	if(pDefensiveArea && pDefensiveArea->IsActive())
	{
		m_pDefensiveArea = pDefensiveArea;
	}

	if(CTaskCombat::ShouldMaintainMinDistanceToTarget(ped))
	{
		m_bMaintainMinDistanceToTarget = true;
		m_scMinDistanceToTargetSq = ScalarVFromF32(intel.GetCombatBehaviour().GetCombatFloat(kAttribFloatMinimumDistanceToTarget));
		m_scMinDistanceToTargetSq = square(m_scMinDistanceToTargetSq);
	}

	m_PrimaryTargetPos.ZeroComponents();
	const CEntity* pTarget = intel.GetQueriableInterface()->GetHostileTarget();
	if(pTarget)
	{
		m_PrimaryTargetPos = pTarget->GetTransform().GetPosition();

		if(m_pDefensiveArea)
		{
			float fDefensiveAreaRadius = 0.0f;
			m_pDefensiveArea->GetMaxRadius(fDefensiveAreaRadius);
			if(fDefensiveAreaRadius < CTaskCombat::ms_Tunables.m_MinDefensiveAreaRadiusForWillAdvance)
			{
				m_bCanCheckAttackWindows = false;
			}

			m_bIsTargetInDefensiveArea = m_pDefensiveArea->IsPointInDefensiveArea(VEC3V_TO_VECTOR3(m_PrimaryTargetPos));
		}
	}
}

bool CCoverPointFilterBase::CheckValidity(const CCoverPoint& UNUSED_PARAM(coverPoint), Vec3V_ConstRef coverPointCoords) const
{
	return !IsPointTooCloseToTarget(coverPointCoords);
}

bool CCoverPointFilterBase::Construct(CCoverPointFilter* pFilterOut, int numBytesAvailable, void* pData)
{
	CompileTimeAssert(sizeof(CCoverPointFilterBase) <= CCoverPointFilterInstance::kMaxSize);

	if(!Verifyf(numBytesAvailable >= sizeof(CCoverPointFilterBase), "Not enough space for cover point filter (%" SIZETFMT "d/%d).", sizeof(CCoverPointFilterBase), numBytesAvailable))
	{
		return false;
	}

	CPed* pPed = static_cast<CPed*>(pData);
	Assert(pPed);
	Assert(pPed->GetIsTypePed());
	::new((void*)pFilterOut) CCoverPointFilterBase(*pPed);

	return true;
}

bool CCoverPointFilterBase::IsPointTooCloseToTarget(Vec3V_ConstRef vCoverPointCoords) const
{
	if(m_bMaintainMinDistanceToTarget)
	{
		ScalarV scDistToTargetSq = DistSquared(vCoverPointCoords, m_PrimaryTargetPos);
		if(IsLessThanAll(scDistToTargetSq, m_scMinDistanceToTargetSq))
		{
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------

CCoverPointFilterTaskCombat::CCoverPointFilterTaskCombat(CPed& ped)
		: CCoverPointFilterBase(ped)
		, m_pCombatTask(NULL)
		, m_AttackWindowMaxSquared(0.0f)
		, m_AttackWindowMinSquared(0.0f)
		, m_MinDistanceForAltCoverSquared(0.0f)
{
	CPedIntelligence& intel = *ped.GetPedIntelligence();

	CTaskCombat* pCombatTask = static_cast<CTaskCombat*>(intel.FindTaskActiveByType(CTaskTypes::TASK_COMBAT));
	m_pCombatTask = pCombatTask;

	float fWindowMin = 0.0f;
	float fWindowMax = 0.0f;
	CTaskMoveWithinAttackWindow::GetAttackWindowMinMax(&ped, fWindowMin, fWindowMax, true);

	m_PedPos = ped.GetTransform().GetPosition();

	m_AttackWindowMinSquared = square(fWindowMin);
	m_AttackWindowMaxSquared = square(fWindowMax);
	m_MinDistanceForAltCoverSquared = square(CTaskCombat::ms_Tunables.m_fMinDistanceForAltCover);
}


bool CCoverPointFilterTaskCombat::CheckValidity(const CCoverPoint& UNUSED_PARAM(coverPoint), Vec3V_ConstRef coverPointCoords) const
{
	const CPed* pPed = m_pPed;
	const Vec3V coverPointCoordsV = coverPointCoords;
	const CTaskCombat* pCombatTask = m_pCombatTask;

	Assert(pPed);
	CCombatBehaviour& combatBehaviour = pPed->GetPedIntelligence()->GetCombatBehaviour();
	// Never move from the initial position if flagged not to.
	CCombatData::Movement combatMovement = combatBehaviour.GetCombatMovement();
	if( combatMovement == CCombatData::CM_Stationary )
	{
		return false;
	}
	// IF the ped is assigned a defensive area, don't allow them to move outside of it
	else if( m_pDefensiveArea )
	{
		if( !m_pDefensiveArea->IsPointInDefensiveArea( RCC_VECTOR3(coverPointCoords) ) )
		{
			return false;
		}
	}

	// If the ped has an attack window, make sure the coverpoint is within it
	if( pCombatTask )
	{
		const CPed* pTarget = pCombatTask->GetPrimaryTarget();
		if(!pTarget)
		{
			return false;
		}

		const ScalarV scAttackWindowMinSq = LoadScalar32IntoScalarV(m_AttackWindowMinSquared);
		const ScalarV scAttackWindowMaxSq = LoadScalar32IntoScalarV(m_AttackWindowMaxSquared);
		const Vec3V primaryTargetPosV = m_PrimaryTargetPos;

		const ScalarV scDistanceToTargetSq = DistSquared(coverPointCoordsV, primaryTargetPosV);

		bool bCheckMaxRange = (combatMovement == CCombatData::CM_WillAdvance);
		bool bCheckMinRange = (!m_pDefensiveArea || m_bCanCheckAttackWindows);

		if(m_pDefensiveArea && bCheckMaxRange && !m_bIsTargetInDefensiveArea)
		{
			Vector3 vDefensiveAreaPos;
			m_pDefensiveArea->GetCentre(vDefensiveAreaPos);
			const ScalarV scTargetToDefensiveAreaSq = DistSquared(VECTOR3_TO_VEC3V(vDefensiveAreaPos), primaryTargetPosV);
			if(IsGreaterThanAll(scTargetToDefensiveAreaSq, scAttackWindowMaxSq))
			{
				bCheckMaxRange = false;
			}
		}

		if( (bCheckMaxRange && IsGreaterThanAll(scDistanceToTargetSq, scAttackWindowMaxSq)) ||
			(bCheckMinRange && IsLessThanAll(scDistanceToTargetSq, scAttackWindowMinSq)) )
		{
			return false;
		}
	}

	// If the ped is in a group, make sure the cp isn't too close to the leader.
	CPedGroup* pMyGroup = pPed->GetPedsGroup();
	// TODO: store some of this info in CCoverPointFilterTaskCombat.
	if( pMyGroup && pMyGroup->GetGroupMembership()->GetLeader() && pMyGroup->GetGroupMembership()->GetLeader()->IsAPlayerPed() )
	{
		if( (VEC3V_TO_VECTOR3(coverPointCoordsV) - VEC3V_TO_VECTOR3(pMyGroup->GetGroupMembership()->GetLeader()->GetTransform().GetPosition()) ).XYMag2()  < rage::square(1.0f) )
		{
			return false;
		}
	}
	
	if(pCombatTask)
	{
		// if the combat task is currently in or at cover then we need to make sure this new cover point is far enough away from our ped
		if(pCombatTask->GetState() == CTaskCombat::State_InCover /*|| pCombatTask->GetState() == CTaskCombat::State_AtCoverPoint*/)
		{
			const Vec3V pedPosV = m_PedPos;

			const ScalarV distToPedSqV = DistSquared(pedPosV, coverPointCoordsV);
			const ScalarV minDistanceForAltCoverSquaredV = LoadScalar32IntoScalarV(m_MinDistanceForAltCoverSquared);

			if(IsLessThanAll(distToPedSqV, minDistanceForAltCoverSquaredV))
			{
				return false;
			}
		}
	}

	return true;
}


bool CCoverPointFilterTaskCombat::Construct(CCoverPointFilter* pFilterOut, int numBytesAvailable, void* pData)
{
	CompileTimeAssert(sizeof(CCoverPointFilterTaskCombat) <= CCoverPointFilterInstance::kMaxSize);

	if(!Verifyf(numBytesAvailable >= sizeof(CCoverPointFilterTaskCombat), "Not enough space for cover point filter (%" SIZETFMT "d/%d).", sizeof(CCoverPointFilterTaskCombat), numBytesAvailable))
	{
		return false;
	}

	CPed* pPed = static_cast<CPed*>(pData);
	Assert(pPed);
	Assert(pPed->GetIsTypePed());
	::new((void*)pFilterOut) CCoverPointFilterTaskCombat(*pPed);

	return true;
}

//-----------------------------------------------------------------------------

CCoverPointFilterDefensiveArea::CCoverPointFilterDefensiveArea(CPed& ped)
		: CCoverPointFilterBase(ped)
{
}


bool CCoverPointFilterDefensiveArea::CheckValidity(const CCoverPoint& UNUSED_PARAM(coverPoint), Vec3V_ConstRef coverPointCoords) const
{
	if(m_pDefensiveArea && !m_pDefensiveArea->IsPointInDefensiveArea( RCC_VECTOR3(coverPointCoords) ))
	{
		return false;
	}
	return true;
}


bool CCoverPointFilterDefensiveArea::Construct(CCoverPointFilter* pFilterOut, int numBytesAvailable, void* pData)
{
	CompileTimeAssert(sizeof(CCoverPointFilterDefensiveArea) <= CCoverPointFilterInstance::kMaxSize);

	if(!Verifyf(numBytesAvailable >= sizeof(CCoverPointFilterDefensiveArea), "Not enough space for cover point filter (%" SIZETFMT "d/%d).", sizeof(CCoverPointFilterDefensiveArea), numBytesAvailable))
	{
		return false;
	}

	CPed* pPed = static_cast<CPed*>(pData);
	CPedIntelligence& intel = *pPed->GetPedIntelligence();
	CDefensiveArea* pDefensiveArea = intel.GetDefensiveAreaForCoverSearch();
	if(pDefensiveArea && pDefensiveArea->IsActive())
	{
		Assert(pPed);
		Assert(pPed->GetIsTypePed());
		::new((void*)pFilterOut) CCoverPointFilterDefensiveArea(*pPed);

		return true;
	}
	return false;	// Don't really need a filter after all.
}

//-----------------------------------------------------------------------------


// End of file 'task/Combat/Cover/coverfilter.cpp'
