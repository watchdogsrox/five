//
// task/Default/TaskSwimmingWander.cpp
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "Task/Default/TaskSwimmingWander.h"


#include "Peds/NavCapabilities.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PedPopulation.h"
#include "Scene/World/GameWorldHeightMap.h"
#include "Task/General/TaskBasic.h"
#include "Task/Motion/TaskMotionBase.h"
#include "Task/Movement/TaskGoToPoint.h"
#include "Task/System/TaskHelpers.h"

CTaskSwimmingWander::Tunables CTaskSwimmingWander::sm_Tunables;
IMPLEMENT_DEFAULT_TASK_TUNABLES(CTaskSwimmingWander, 0xc6356fde);

BANK_ONLY(bool CTaskSwimmingWander::sm_bRenderProbeChecks = false;)

AI_OPTIMISATIONS()

// Cache off the water depth
CTask::FSM_Return	CTaskSwimmingWander::ProcessPreFSM()
{
	CPed* pPed = GetPed();

	// if the fish just hit a wall, there's a very good chance that we need to do probe test soon to steer away from a wall
	if (m_bCanForceProbe && pPed->GetPedResetFlag(CPED_RESET_FLAG_PedHitWallLastFrame))
	{
		if (m_Timer.GetDuration() > sm_Tunables.m_InstantProbeDurationMin)
		{
			m_Timer.ChangeDuration(0);
		}
	}

	// Allow low MotionTask LOD. The way we do AI timeslicing, we enable that only if we
	// are in low MotionTask LOD, so this is a part of a mechanism that allows timeslicing of fish.
	CPedAILod& lod = pPed->GetPedAiLod();
	lod.ClearBlockedLodFlag(CPedAILod::AL_LodMotionTask);

	return FSM_Continue;
}

CTask::FSM_Return CTaskSwimmingWander::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		FSM_State(State_Initial)
			FSM_OnUpdate
				return StateInitial_OnUpdate();
		FSM_State(State_Moving)
			FSM_OnEnter
				StateMoving_OnEnter();
			FSM_OnUpdate
				return StateMoving_OnUpdate();
	FSM_End
}

CTask::FSM_Return CTaskSwimmingWander::StateInitial_OnUpdate()
{
	// Kick off the probe test timer
	m_Timer.Set(fwTimer::GetTimeInMilliseconds(), sm_Tunables.m_AvoidanceProbeInterval);

	CPed* pPed = GetPed();

	m_bSkimSurface = pPed->GetPedIntelligence()->GetNavCapabilities().IsFlagSet(CPedNavCapabilityInfo::FLAG_PREFER_NEAR_WATER_SURFACE);

	m_fPreferredDepth =  m_bSkimSurface ? sm_Tunables.m_SurfaceSkimmerDepth : sm_Tunables.m_NormalPreferredDepth;


	if (pPed->GetTaskData().GetIsFlagSet(CTaskFlags::SwimStraightInSwimmingWander))
	{
		// Just swim straight.
		m_fWanderDirection = pPed->GetCurrentHeading();
	}
	else
	{
		// Start off swimming in a random direction.
		m_fWanderDirection = fwAngle::LimitRadianAngle(fwRandom::GetRandomNumberInRange(0.0f, TWO_PI));
	}
	

	// Start swimming.
	SetState(State_Moving);
	return FSM_Continue;
}

// Just swim forwards to do wall following.
void CTaskSwimmingWander::StateMoving_OnEnter()
{
	CPed* pPed = GetPed();

	Vector3 vPed = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	Vector3 vTarget = vPed;

	float fRangeOffset = pPed->GetTaskData().GetSwimmingWanderPointRange();
	vTarget.x = -rage::Sinf(m_fWanderDirection) * fRangeOffset + vTarget.x;
	vTarget.y = rage::Cosf(m_fWanderDirection) * fRangeOffset + vTarget.y;

	float fWaterZ = 0.0f;

	if (Water::GetWaterLevel(vTarget, &fWaterZ, true, POOL_DEPTH, REJECTIONABOVEWATER, NULL))
	{
		if (m_bSkimSurface)
		{
			// Go along the surface.
			vTarget.z = fWaterZ - m_fPreferredDepth;
		}
		else
		{
			// Only go along the surface if that is lower than your current depth.
			vTarget.z = rage::Min(vTarget.z, fWaterZ - m_fPreferredDepth);
		}
	}
	else
	{
		// This fish wants to swim where there is no water.  Try and delete it if possible.
		pPed->SetRemoveAsSoonAsPossible(true);
	}

	CTaskMoveGoToPoint* pGoToTask = rage_new CTaskMoveGoToPoint(MOVEBLENDRATIO_WALK, vTarget);
	SetNewTask(rage_new CTaskComplexControlMovement(pGoToTask));
}



//Check if we reached the target, if so go back to decide
CTask::FSM_Return CTaskSwimmingWander::StateMoving_OnUpdate()
{
	float fTurnAngle = AvoidObstacles();
	if (fabs(fTurnAngle) < SMALL_FLOAT)
	{
		fTurnAngle = AvoidNonCreationZones();
	}

	// Make sure some time has passed so we don't end up with an infinite state loop.
	if (fabs(fTurnAngle) > SMALL_FLOAT && GetTimeInState() > 0.5f)
	{
		// Flip direction.
		m_fWanderDirection += fTurnAngle;
		m_fWanderDirection = fwAngle::LimitRadianAngleSafe(m_fWanderDirection);

#if __BANK
		if (sm_bRenderProbeChecks)
		{
			grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition()), 0.2f, Color_red, false);
		}
#endif
		
		// Restart the current state.
		SetFlag(aiTaskFlags::RestartCurrentState);
	}

	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetFlag(aiTaskFlags::RestartCurrentState);
	}
	return FSM_Continue;
}

// Fire periodic probe tests to check for obstacles
// Returns the angle the fish should steer to avoid this obstacle.
float CTaskSwimmingWander::AvoidObstacles()
{
	CPed* pPed = GetPed();
	Mat34V mPed;
	CPed::ComputeMatrixFromHeadingAndPitch(mPed, m_fWanderDirection, pPed->GetCurrentPitch(), pPed->GetTransform().GetPosition());

	if (m_Timer.IsOutOfTime())
	{
		if (m_CapsuleResult.GetResultsStatus() == WorldProbe::TEST_NOT_SUBMITTED)
		{
			Vector3 vEnd = VEC3V_TO_VECTOR3(mPed.GetCol1());

			Vector3 vPed = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			vEnd -= vEnd * sm_Tunables.m_AvoidanceProbePullback; //move back the probe in case the fish is right up against a wall
			vEnd *= sm_Tunables.m_AvoidanceProbeLength;
			vEnd += vPed;

			float fRadius = pPed->GetCapsuleInfo()->GetProbeRadius() * 0.5f;

			//Create and fire the probe
			WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
			capsuleDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_BOX_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE);
			capsuleDesc.SetExcludeTypeFlags(ArchetypeFlags::GTA_PICKUP_TYPE);
			capsuleDesc.SetResultsStructure(&m_CapsuleResult);
			capsuleDesc.SetMaxNumResultsToUse(1);
			capsuleDesc.SetContext(WorldProbe::EMovementAI);
			capsuleDesc.SetIsDirected(true);
			capsuleDesc.SetCapsule(vPed, vEnd, fRadius);
			capsuleDesc.SetDoInitialSphereCheck(true);
			WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc, WorldProbe::PERFORM_ASYNCHRONOUS_TEST);

#if __BANK
			if (sm_bRenderProbeChecks) 
			{
				grcDebugDraw::Capsule(VECTOR3_TO_VEC3V(vPed), VECTOR3_TO_VEC3V(vEnd), fRadius, Color_white, false, -30);
			}
#endif
		}
		else
		{
			//Check the probe
			bool bReady = m_CapsuleResult.GetResultsReady();
			if (bReady)
			{
				//Probe is done calculating
				//do another probe test in X seconds
				m_Timer.Set(fwTimer::GetTimeInMilliseconds(), sm_Tunables.m_AvoidanceProbeInterval);
				// if we are not touching a wall currently, then allow the next collision to trigger an immediate probe test
				m_bCanForceProbe = !pPed->GetPedResetFlag(CPED_RESET_FLAG_PedHitWallLastFrame);

				if (m_CapsuleResult.GetNumHits() > 0)
				{
					Vector3 vHit = m_CapsuleResult[0].GetHitNormal();
					Vector3 vRight = VEC3V_TO_VECTOR3(Cross(mPed.GetCol0(), mPed.GetCol2()));
					m_CapsuleResult.Reset();
					if (vHit.Dot(vRight) > 0.0f)
					{
						return sm_Tunables.m_AvoidanceSteerAngleDegrees * -DtoR;
					}
					else
					{
						return sm_Tunables.m_AvoidanceSteerAngleDegrees * DtoR;
					}
				}
				m_CapsuleResult.Reset();
			}
		}
	}
	return 0.0f;
}

// Returns the angle the fish should steer to avoid this obstacle.
float CTaskSwimmingWander::AvoidNonCreationZones()
{
	CPed* pPed = GetPed();
	Mat34V mPed;
	CPed::ComputeMatrixFromHeadingAndPitch(mPed, m_fWanderDirection, pPed->GetCurrentPitch(), pPed->GetTransform().GetPosition());
	Vector3 vForward = VEC3V_TO_VECTOR3(mPed.GetCol1());

	Vector3 vPed = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	vForward *= sm_Tunables.m_AvoidanceProbeLength;
	vForward += vPed;

	if (CPedPopulation::PointFallsWithinNonCreationArea(vForward))
	{
		return sm_Tunables.m_AvoidanceSteerAngleDegrees * DtoR;
	}
	else
	{
		return 0.0f;
	}
}


//name debug info
#if !__FINAL
const char * CTaskSwimmingWander::GetStaticStateName( s32 iState )
{
	switch(iState)
	{
	case State_Initial: return "Initial";
	case State_Moving: return "Moving";
	default: return "NULL";
	}
}
#endif //!__FINAL
