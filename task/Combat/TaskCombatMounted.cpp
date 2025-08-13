//
// task/Combat/TaskCombatMounted.cpp
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "task/Combat/TaskCombatMounted.h"
#include "task/Combat/CombatManager.h"
#include "task/General/TaskBasic.h"
#include "task/Movement/TaskGotoPoint.h"
#include "task/Movement/TaskMoveFollowEntityOffset.h"
#include "task/Movement/TaskNavMesh.h"
#include "task/Vehicle/TaskVehicleWeapon.h"
#include "fwsys/config.h"
#include "pathserver/PathServer.h"
#include "Peds/ped.h"
#include "vector/geometry.h"
#include "weapons/inventory/PedWeaponManager.h"

AI_OPTIMISATIONS()

//-----------------------------------------------------------------------------

void CTaskCombatMountedTransitions::Init()
{
	AddTransition(CTaskMoveCombatMounted::State_Start,		CTaskMoveCombatMounted::State_Circling,	1.0f, CTaskMoveCombatMounted::CF_TargetStationary);
	AddTransition(CTaskMoveCombatMounted::State_Start,		CTaskMoveCombatMounted::State_Chasing,	1.0f, CTaskMoveCombatMounted::CF_TargetMoving);

	AddTransition(CTaskMoveCombatMounted::State_Chasing,	CTaskMoveCombatMounted::State_Circling,	1.0f, CTaskMoveCombatMounted::CF_TargetStationary);
	AddTransition(CTaskMoveCombatMounted::State_Circling,	CTaskMoveCombatMounted::State_Chasing,	1.0f, CTaskMoveCombatMounted::CF_StateEnded);
}

//-----------------------------------------------------------------------------

CTaskCombatMounted::CTaskCombatMounted(const CEntity* pPrimaryTarget)
		: m_PrimaryTarget(pPrimaryTarget)
{
	SetInternalTaskType(CTaskTypes::TASK_COMBAT_MOUNTED);
}


aiTask* CTaskCombatMounted::Copy() const
{
	return rage_new CTaskCombatMounted(m_PrimaryTarget);
}


CTask::FSM_Return CTaskCombatMounted::ProcessPreFSM()
{
	if(!m_PrimaryTarget)
	{
		return FSM_Quit;
	}

	CPed* pPed = GetPed();
	CPed* pMount = pPed->GetMyMount();
	if(!pMount)
	{
		// No longer mounted, end the task.
		return FSM_Quit;
	}

	// We'll probably have to deal with the mount dying, in some way.
	// Currently, this doesn't work well, because CTaskCombat just restarts
	// the task right away.
#if 0
	if(pMount->IsFatallyInjured())
	{
		return FSM_Quit;
	}
#endif

	return FSM_Continue;
}


CTask::FSM_Return CTaskCombatMounted::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Active)
			FSM_OnEnter
				Active_OnEnter();
			FSM_OnUpdate
				return Active_OnUpdate();
	FSM_End
}

#if !__FINAL

void CTaskCombatMounted::Debug() const
{
}


const char * CTaskCombatMounted::GetStaticStateName( s32 iState )
{
	static const char* s_StateNames[] =
	{
		"Active"
	}; 
	CompileTimeAssert(NELEM(s_StateNames) == kNumStates);

	if(iState >= 0 && iState < kNumStates)
	{
		return s_StateNames[iState];
	}
	return "Invalid";
}

#endif	// !__FINAL

void CTaskCombatMounted::Active_OnEnter()
{
	CPed* pPed = GetPed();
	CPedWeaponManager* pWeapons = pPed->GetWeaponManager();
	if(pWeapons)
	 	pWeapons->EquipBestWeapon();

	CTaskVehicleGun* pGunTask = CreateGunTask();

	CTaskMoveCombatMounted* pMoveTask = rage_new CTaskMoveCombatMounted(m_PrimaryTarget);
	CTask* pTask = rage_new CTaskComplexControlMovement(pMoveTask, pGunTask /*, CTaskComplexControlMovement::TerminateOnSubtask*/);
	SetNewTask(pTask);

}


CTask::FSM_Return CTaskCombatMounted::Active_OnUpdate()
{
// TODO: Deal with target changes

	return FSM_Continue;
}


CTaskVehicleGun* CTaskCombatMounted::CreateGunTask()
{
	const CEntity* pGunTgt = m_PrimaryTarget;
	CAITarget target(pGunTgt);
	CTaskVehicleGun* pGunTask = rage_new CTaskVehicleGun(CTaskVehicleGun::Mode_Fire, FIRING_PATTERN_BURST_FIRE, &target);
	return pGunTask;
}

//-----------------------------------------------------------------------------

CTaskMoveCombatMounted::Tunables CTaskMoveCombatMounted::sm_Tunables;

IMPLEMENT_COMBAT_TASK_TUNABLES(CTaskMoveCombatMounted, 0x066d26e6);

CTaskCombatMountedTransitions CTaskMoveCombatMounted::sm_Transitions;

void CTaskMoveCombatMounted::InitTransitionTables()
{
	sm_Transitions.Init();
}

CTaskMoveCombatMounted::CTaskMoveCombatMounted(const CEntity* pPrimaryTarget)
		: m_PrimaryTarget(pPrimaryTarget)
		, CTaskMove(MOVEBLENDRATIO_RUN)		// Hope this isn't used for anything.
{
	m_TargetNotInCircle = false;
	m_RunningCirclingTask = false;
	m_TimeWaitingForCircleMs = 0;
	SetInternalTaskType(CTaskTypes::TASK_MOVE_COMBAT_MOUNTED);
}


aiTask* CTaskMoveCombatMounted::Copy() const
{
	return rage_new CTaskMoveCombatMounted(m_PrimaryTarget);
}


void CTaskMoveCombatMounted::CleanUp()
{
	RegisterAttacker(NULL);

	if(m_Attacker.GetPed())
	{
		m_Attacker.Shutdown();
	}
}


CTask::FSM_Return CTaskMoveCombatMounted::ProcessPreFSM()
{
	if(!m_PrimaryTarget)
	{
		return FSM_Quit;
	}

	UpdateRootTarget();

	RegisterAttacker(m_RootTarget);

	return FSM_Continue;
}


CTask::FSM_Return CTaskMoveCombatMounted::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();	

		FSM_State(State_Chasing)
			FSM_OnEnter
				Chasing_OnEnter();
			FSM_OnUpdate
				return Chasing_OnUpdate();
			FSM_OnExit
				Chasing_OnExit();

		FSM_State(State_Circling)
			FSM_OnEnter
				Circling_OnEnter();
			FSM_OnUpdate
				return Circling_OnUpdate();
			FSM_OnExit
				Circling_OnExit();

	FSM_End
}

#if !__FINAL

void CTaskMoveCombatMounted::Debug() const
{
}


const char * CTaskMoveCombatMounted::GetStaticStateName( s32 iState )
{
	static const char* s_StateNames[] =
	{
		"Start",
		"Chasing",
		"Circling"
	}; 
	CompileTimeAssert(NELEM(s_StateNames) == kNumStates);

	if(iState >= 0 && iState < kNumStates)
	{
		return s_StateNames[iState];
	}
	return "Invalid";
}

#endif	// !__FINAL

s64 CTaskMoveCombatMounted::GenerateTransitionConditionFlags(bool bStateEnded)
{
	s64 iConditionFlags = 0;

	if(bStateEnded)
	{
		iConditionFlags |= CF_StateEnded;
	}

	// Note: square could be precomputed.
	const ScalarV velThresholdV = LoadScalar32IntoScalarV(CTaskMoveCombatMounted::sm_Tunables.m_VelStartCircling);
	const ScalarV velThresholdSquaredV = Scale(velThresholdV, velThresholdV);

	const Vec3V velV = GetTargetVelocityV();
	if(IsLessThanAll(MagSquared(velV), velThresholdSquaredV))
	{
		iConditionFlags |= CF_TargetStationary;
	}
	else
	{
		iConditionFlags |= CF_TargetMoving;
	}

	return iConditionFlags;
}


Vec3V_Out CTaskMoveCombatMounted::GetTargetVelocityV() const
{
	const CEntity* pTgt = m_RootTarget;
	Vec3V velV;
	if(pTgt->GetIsPhysical())
	{
		return RCC_VEC3V(static_cast<const CPhysical*>(pTgt)->GetVelocity());
	}
	else
	{
		return Vec3V(V_ZERO);
	}
}



CTask::FSM_Return CTaskMoveCombatMounted::Start_OnUpdate()
{
	PickNewStateTransition(State_Start);
	return FSM_Continue;
}


void CTaskMoveCombatMounted::Chasing_OnEnter()
{
	const CEntity* pMoveTgt = m_RootTarget;

	CTask* pSubTask = GetSubTask();
	if(!pSubTask || pSubTask->GetTaskType() != CTaskTypes::TASK_MOVE_FOLLOW_ENTITY_OFFSET)
	{
		Vector3 offs(0.0f, -3.0f, 0.0f);
		CTaskMoveFollowEntityOffset* pMoveTask = rage_new CTaskMoveFollowEntityOffset(pMoveTgt, MOVEBLENDRATIO_SPRINT, 3.0f, offs, -1);
		SetNewTask(pMoveTask);
	}
}



CTask::FSM_Return CTaskMoveCombatMounted::Chasing_OnUpdate()
{
	// TODO: Add check to make sure we're chasing the right target.

	if(PeriodicallyCheckForStateTransitions(sm_Tunables.m_TransitionReactionTime))
	{
		return FSM_Continue;
	}

	return FSM_Continue;
}


void CTaskMoveCombatMounted::Chasing_OnExit()
{
	// May not be the right place, may only want to do this if transitioning to circling.
	SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
}


void CTaskMoveCombatMounted::Circling_OnEnter()
{
	m_Attacker.SetRequestCircle(true);

	m_TargetNotInCircle = false;

	m_TimeWaitingForCircleMs = 0;
}


CTask::FSM_Return CTaskMoveCombatMounted::Circling_OnUpdate()
{
	if(PeriodicallyCheckForStateTransitions(sm_Tunables.m_TransitionReactionTime))
	{
		return FSM_Continue;
	}

	CCombatMountedAttackerGroup* pGrp = m_Attacker.GetAttackerGroup();
	if(pGrp)
	{
		pGrp->RequestCircleTests();
	}

	const CCombatMountedAttackerGroup::CircleData* pCircle = m_Attacker.GetCircle();

	if(m_RunningCirclingTask)
	{
		if((pCircle && m_Attacker.GetCircleIsNew()) || !pCircle)
		{
			const ScalarV velDistThresholdV = LoadScalar32IntoScalarV(CTaskMoveCombatMounted::sm_Tunables.m_VelStopCircling);
			const ScalarV velDistThresholdSquaredV = Scale(velDistThresholdV, velDistThresholdV);
			const Vec3V velV = GetTargetVelocityV();
			if(IsLessThanAll(MagSquared(velV), velDistThresholdSquaredV))
			{

			}
			else
			{
				// Note: maybe we should use condition flags instead?
				SetState(State_Chasing);
				return FSM_Continue;
			}
		}
		m_TimeWaitingForCircleMs = 0;
	}
	else
	{
		if(m_TimeWaitingForCircleMs > sm_Tunables.m_MaxTimeWaitingForCircleMs)
		{
			PickNewStateTransition(State_Start, true);
			SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
			return FSM_Continue;
		}

		m_TimeWaitingForCircleMs += fwTimer::GetTimeStepInMilliseconds();
	}

	if(pCircle)
	{
		if(m_Attacker.GetCircleIsNew())
		{
			const Vec3V circleCenterV = pCircle->m_CenterAndRadius.GetXYZ();
			float circleRadius = pCircle->m_CenterAndRadius.GetWf();

			bool clockwise = true;
			//bool clockwise = fwRandom::GetRandomTrueFalse();

			CTaskMoveCircle* pMoveTask = rage_new CTaskMoveCircle(circleCenterV, circleRadius, clockwise, MOVEBLENDRATIO_RUN);

			SetNewTask(pMoveTask);

			m_RunningCirclingTask = true;

			m_Attacker.SetCircleIsNew(false);
		}
	}
	else
	{
		if(GetSubTask())
		{
			SetNewTask(NULL);
		}
		m_RunningCirclingTask = false;
	}

	return FSM_Continue;
}


void CTaskMoveCombatMounted::Circling_OnExit()
{
	CCombatMountedAttackerGroup* pGrp = m_Attacker.GetAttackerGroup();
	if(pGrp)
	{
		pGrp->RemoveMemberFromCircles(m_Attacker);
	}

	m_RunningCirclingTask = false;
	m_TargetNotInCircle = false;

	m_Attacker.SetRequestCircle(false);
}


void CTaskMoveCombatMounted::RegisterAttacker(const CEntity* pNewTgt)
{
	if(!m_Attacker.GetPed())
	{
		CPed* pPed = GetPed();
		Assert(pPed);
		m_Attacker.Init(*pPed);
	}

	CCombatMountedAttackerGroup* pGrp = m_Attacker.GetAttackerGroup();
	const CEntity* pOldTgt = pGrp ? pGrp->GetTarget() : NULL;
	if(pNewTgt != pOldTgt)
	{
		if(pGrp)
		{
			pGrp->Remove(m_Attacker);
			pGrp = NULL;
		}

		if(pNewTgt)
		{
			pGrp = CCombatManager::GetMountedCombatManager()->FindOrCreateAttackerGroup(*pNewTgt);
			if(pGrp)
			{
				pGrp->Insert(m_Attacker);
			}
		}
	}
}


void CTaskMoveCombatMounted::UpdateRootTarget()
{
	const CEntity* pTgt = m_PrimaryTarget;
	if(pTgt && pTgt->GetIsTypePed())
	{
		CPed* pMount = static_cast<const CPed*>(pTgt)->GetMyMount();
		if(pMount)
		{
			pTgt = pMount;
		}
	}
	m_RootTarget = pTgt;
}

//-----------------------------------------------------------------------------

CTaskMoveCircle::CTaskMoveCircle(
		Vec3V_In circleCenterV, float radius, bool dirClockwise, float moveBlendRatio)
		: CTaskMove(moveBlendRatio)
		, m_DirClockwise(dirClockwise)
{
	m_CircleCenterAndRadius.SetXYZ(circleCenterV);
	m_CircleCenterAndRadius.SetWf(radius);
	SetInternalTaskType(CTaskTypes::TASK_MOVE_CIRCLE);
}


aiTask* CTaskMoveCircle::Copy() const
{
	return rage_new CTaskMoveCircle(m_CircleCenterAndRadius.GetXYZ(), m_CircleCenterAndRadius.GetWf(), m_DirClockwise, m_fMoveBlendRatio);
}


void CTaskMoveCircle::CleanUp()
{
}


CTask::FSM_Return CTaskMoveCircle::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();	

		FSM_State(State_ApproachingCircle)
			FSM_OnEnter
				ApproachingCircle_OnEnter();
			FSM_OnUpdate
				return ApproachingCircle_OnUpdate();

		FSM_State(State_Circling)
			FSM_OnEnter
				Circling_OnEnter();
			FSM_OnUpdate
				return Circling_OnUpdate();

	FSM_End
}

#if !__FINAL

void CTaskMoveCircle::Debug() const
{
}


const char * CTaskMoveCircle::GetStaticStateName( s32 iState )
{
	static const char* s_StateNames[] =
	{
		"Start",
		"ApproachingCircle",
		"Circling"
	}; 
	CompileTimeAssert(NELEM(s_StateNames) == kNumStates);

	if(iState >= 0 && iState < kNumStates)
	{
		return s_StateNames[iState];
	}
	return "Invalid";
}

#endif	// !__FINAL

CTask::FSM_Return CTaskMoveCircle::Start_OnUpdate()
{
	SetState(State_ApproachingCircle);
	return FSM_Continue;
}


void CTaskMoveCircle::ApproachingCircle_OnEnter()
{
	Vec3V ptOnCircle;
	ComputePointOnCircle(ptOnCircle, true);

	const float mbr = m_fMoveBlendRatio;

	CNavParams params;
	params.m_vTarget.Set(RCC_VECTOR3(ptOnCircle));
	params.m_fMoveBlendRatio = mbr;
	params.m_fTargetRadius = CTaskMoveFollowNavMesh::ms_fTargetRadius;
	params.m_iWarpTimeMs = -1;
	params.m_bFleeFromTarget = false;
	params.m_fCompletionRadius = CTaskMoveFollowNavMesh::ms_fDefaultCompletionRadius;
	params.m_fSlowDownDistance = 0.0f;

	CTaskMoveFollowNavMesh* pMoveTask = rage_new CTaskMoveFollowNavMesh(params);
	pMoveTask->SetKeepMovingWhilstWaitingForPath(true);

	SetNewTask(pMoveTask);
}


CTask::FSM_Return CTaskMoveCircle::ApproachingCircle_OnUpdate()
{
	// TODO: Think more about the case of the task failing.

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Circling);
		return FSM_Continue;
	}

	if(TestPosAndDirOnCircle(false))
	{
		SetState(State_Circling);
		return FSM_Continue;
	}

	CTask* pSubTask = GetSubTask();
	if(pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH)
	{
		Vec3V ptOnCircle;
		ComputePointOnCircle(ptOnCircle, true);

		CTaskMoveFollowNavMesh* pMoveTask = static_cast<CTaskMoveFollowNavMesh*>(pSubTask);
		pMoveTask->SetTarget(GetPed(), ptOnCircle);
	}

	return FSM_Continue;
}


void CTaskMoveCircle::Circling_OnEnter()
{
}


CTask::FSM_Return CTaskMoveCircle::Circling_OnUpdate()
{
	Vec3V ptOnCircle;
	ComputePointOnCircle(ptOnCircle, false);

	// If we don't have a subtask, create it.
	if(!GetSubTask())
	{
		const float mbr = m_fMoveBlendRatio;
		CTaskMoveGoToPoint* pMoveTask = rage_new CTaskMoveGoToPoint(mbr, RCC_VECTOR3(ptOnCircle), 0.0f);
		SetNewTask(pMoveTask);
	}

	// If we have a subtask, update the destination.
	CTask* pSubTask = GetSubTask();
	if(pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_MOVE_GO_TO_POINT)
	{
		CTaskMoveGoToPoint* pMoveTask = static_cast<CTaskMoveGoToPoint*>(pSubTask);
		pMoveTask->SetTarget(GetPed(), ptOnCircle);
	}

	return FSM_Continue;
}


void CTaskMoveCircle::ComputePointOnCircle(Vec3V_Ref ptOut, bool useIsectIfInside)
{
	const Vec3V circleCenter = m_CircleCenterAndRadius.GetXYZ();
	const float radius = m_CircleCenterAndRadius.GetWf();
	const fwTransform& transform = GetPed()->GetTransform();
	const Vec3V pos = transform.GetPosition();

	const float circleCenterX = circleCenter.GetXf();
	const float circleCenterY = circleCenter.GetYf();

	const float posX = pos.GetXf();
	const float posY = pos.GetYf();

	Assert(radius >= SMALL_FLOAT);

	const float dx = posX - circleCenterX;
	const float dy = posY - circleCenterY;

	const float distSq = square(dx) + square(dy);

	float tangentPtX = 0.0f, tangentPtY = 0.0f;
	const float dist = sqrtf(distSq);

	// TODO: Move these to tuning data.
	static float s_MinDistAhead = 2.5f;
	static float s_MinDistAheadIsect = 15.0f;
	static float s_MinDistFromCircleForIsect = 2.0f;

	bool found = false;

	float extraDistOnCircle = 0.0f;
	if(useIsectIfInside && dist < radius - s_MinDistFromCircleForIsect)
	{
		const Vec3V fwdV = transform.GetForward();
		const float fwdX = fwdV.GetXf();
		const float fwdY = fwdV.GetYf();
		const float radiusSq = square(radius);

// This piece of code finds the radius of a circle through the ped, so that it intersects
// the surrounding circle at a common tangent point. In its current form this doesn't
// produce very good results, because we won't actually move along that circle if we just
// use the tangent point as the destination.
#if 0
		const float m = sqrtf(square(fwdX) + square(fwdY));
		if(m > SMALL_FLOAT)
		{
			float ex = fwdY/m;
			float ey = -fwdX/m;
			if(!m_DirClockwise)
			{
				ex = -ex;
				ey = -ey;
			}

			const float b = 2*dx*ex + 2*dy*ey + 2*radius;
			const float c = square(dx) + square(dy) - radiusSq;
			//	b*r + c = 0;
			if(b != 0.0f)
			{
				const float r = -c/b;
				if(r > 0.0f)
				{
					const float cx = dx + ex*r;
					const float cy = dy + ey*r;

					//const Vec3V drawCenterV(cx + circleCenterX, cy + circleCenterY, circleCenter.GetZf());			
					//grcDebugDraw::Circle(RCC_VECTOR3(drawCenterV), r, Color_red, XAXIS, YAXIS);

					const float q = square(cx) + square(cy);
					if(q > SMALL_FLOAT)
					{
						const float s = radius/sqrtf(q);
						tangentPtX = circleCenterX + cx*s;
						tangentPtY = circleCenterY + cy*s;
						found = true;
					}
				}
			}
		}
#endif

		// Find the intersection point between the forward direction and the circle,
		// and use that as the destination (after additional rotation along the circle,
		// to match up the direction with the tangent as we get closer).
		if(!found)
		{
			//	px = dx + t*fwdX;
			//	py = dy + t*fwdY
			//	px*px + py*py = radiusSq
			//	(dx + t*fwdX)*(dx + t*fwdX) + (dy + t*fwdY)*(dy + t*fwdY) = radiusSq
			//	dx*dx + 2*t*fwdX*dx + t*t*fwdX*fwdX + dy*dy + 2*t*fwdY*dy + t*t*fwdY*fwdY = radiusSq
			//	t*t*fwdX*fwdX + t*t*fwdY*fwdY + 2*t*fwdX*dx + 2*t*fwdY*dy - radiusSq + dx*dx + dy*dy = 0
			//	t*t*(fwdX*fwdX + fwdY*fwdY) + t*(2*fwdX*dx + 2*fwdY*dy) - radiusSq  + dx*dx + dy*dy = 0

			const float a = square(fwdX) + square(fwdY);
			const float b = 2.0f*(fwdX*dx + fwdY*dy);
			const float c = distSq - radiusSq;
			if(a != 0.0f)
			{
				const float p = b/a;
				const float q = c/a;
				const float r2 = p*p - 4*q;
				if(r2 > 0.0f)
				{
					const float r = sqrtf(r2);
					const float t = 0.5f*(-p + r);

					tangentPtX = circleCenterX + dx + fwdX*t;
					tangentPtY = circleCenterY + dy + fwdY*t;

					found = true;

					const float distToIsect = t*sqrtf(square(fwdX) + square(fwdY));	// Note: may be able to treat fwdX/fwdY as a unit vector here.
					extraDistOnCircle = Max(s_MinDistAheadIsect - distToIsect, 0.0f);
				}
			}
		}
	}

	if(!found)
	{
		float xi1, yi1, xi2, yi2;
		if(dist < radius || !geom2D::IntersectTwoCircles(circleCenterX, circleCenterY, radius,
				0.5f*(circleCenterX + posX),
				0.5f*(circleCenterY + posY),
				0.5f*dist, xi1, yi1, xi2, yi2))
		{
			float s = radius/Max(dist, SMALL_FLOAT);
			tangentPtX = circleCenterX + dx*s;
			tangentPtY = circleCenterY + dy*s;

			const float distToCircle = radius - dist;
			extraDistOnCircle = Max(s_MinDistAhead - distToCircle, 0.0f);
		}
		else
		{
			if(m_DirClockwise)
			{
				tangentPtX = xi2;
				tangentPtY = yi2;
			}
			else
			{
				tangentPtX = xi1;
				tangentPtY = yi1;
			}

			const float distToTgt = sqrtf(square(posX - tangentPtX) + square(posY - tangentPtY));
			extraDistOnCircle = Max(s_MinDistAhead - distToTgt, 0.0f);
		}
	}

	// Apply extra rotation along the circle.

	float extraAngle = Min(extraDistOnCircle/Max(radius, SMALL_FLOAT), PI);
	if(m_DirClockwise)
	{
		extraAngle = -extraAngle;
	}

	float rotCos, rotSin;
	cos_and_sin(rotCos, rotSin, extraAngle);

	const float deltaX = tangentPtX - circleCenterX;
	const float deltaY = tangentPtY - circleCenterY;

	const float newDeltaX = deltaX*rotCos - deltaY*rotSin;
	const float newDeltaY = deltaX*rotSin + deltaY*rotCos;

	tangentPtX = circleCenterX + newDeltaX;
	tangentPtY = circleCenterY + newDeltaY;

	// Store the output.

	ptOut.SetXf(tangentPtX);
	ptOut.SetYf(tangentPtY);
	ptOut.SetZf(circleCenter.GetZf());
}


bool CTaskMoveCircle::TestPosAndDirOnCircle(bool alreadyThere) const
{
	// TODO: Move these to tuning data.
	static float s_PosThreshold = 0.5f;
	static float s_PosThresholdMaxFractionOfRadius = 0.1f;
	static float s_PosAlreadyThereFactor = 2.0f;
	static float s_AngThreshold = 0.985f;				// cosf(10*DtoR)
	static float s_AngThresholdAlreadyThere = 0.940f;	// cosf(20*DtoR)

	const Vec3V circleCenter = m_CircleCenterAndRadius.GetXYZ();
	const float radius = m_CircleCenterAndRadius.GetWf();
	const fwTransform& transform = GetPed()->GetTransform();
	const Vec3V pos = transform.GetPosition();

	const float circleCenterX = circleCenter.GetXf();
	const float circleCenterY = circleCenter.GetYf();

	const float posX = pos.GetXf();
	const float posY = pos.GetYf();

	Assert(radius >= SMALL_FLOAT);

	const float dx = posX - circleCenterX;
	const float dy = posY - circleCenterY;

	const float distSq = square(dx) + square(dy);

	float distThreshold = Min(s_PosThreshold, radius*s_PosThresholdMaxFractionOfRadius);
	if(alreadyThere)
	{
		distThreshold *= s_PosAlreadyThereFactor;
	}

	const float minDist = Max(radius - distThreshold, 0.0f);
	const float maxDist = radius + distThreshold;
	const float minDistSq = square(minDist);
	const float maxDistSq = square(maxDist);
	if(distSq < minDistSq)
	{
		return false;
	}
	if(distSq > maxDistSq)
	{
		return false;
	}

	float tangentDirX = dy;
	float tangentDirY = -dx;
	if(!m_DirClockwise)
	{
		tangentDirX = - tangentDirX;
		tangentDirY = - tangentDirY;
	}

	const Vec3V fwdV = transform.GetForward();
	const float fwdX = fwdV.GetXf();
	const float fwdY = fwdV.GetYf();
	const float dot = fwdX*tangentDirX + fwdY*tangentDirY;
	const float m1 = sqrtf(square(fwdX) + square(fwdY));
	const float m2 = sqrtf(distSq);
	const float cosAngleThreshold = alreadyThere ? s_AngThresholdAlreadyThere : s_AngThreshold;
	if(dot < cosAngleThreshold*m1*m2)
	{
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------

// End of file 'task/Combat/TaskCombatMounted.cpp'
