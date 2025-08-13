// FILE :    TaskAnimatedHitByExplosion.cpp
// CREATED : 07/02/2012

//rage headers
#include "math/angmath.h"

// game headers
#include "Peds/ped.h"
#include "Task/Combat/TaskAnimatedHitByExplosion.h"
#include "task/Combat/TaskDamageDeath.h"
#include "Task/Movement/Jumping/TaskFallGetUp.h"
#include "event/Events.h"

AI_OPTIMISATIONS()

CTaskAnimatedHitByExplosion::Tunables CTaskAnimatedHitByExplosion::sm_Tunables;
IMPLEMENT_COMBAT_TASK_TUNABLES(CTaskAnimatedHitByExplosion, 0x228bf772);

//////////////////////////////////////////////////////////////////////////
// CtaskAnimatedHitByExplosion
//////////////////////////////////////////////////////////////////////////

CTaskAnimatedHitByExplosion::CTaskAnimatedHitByExplosion(const fwMvClipSetId& clipSetId, const fwMvClipId& clipId, float startPhase)
: m_clipSetId(clipSetId),
m_clipId(clipId),
m_startPhase(startPhase)
{
	SetInternalTaskType(CTaskTypes::TASK_ANIMATED_HIT_BY_EXPLOSION);
	m_ragdollBlockingFlags.BitSet().Reset();
}

CTaskAnimatedHitByExplosion::~CTaskAnimatedHitByExplosion()
{
}

bool CTaskAnimatedHitByExplosion::ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
	//Check if the priority is not immediate.
	if(iPriority != ABORT_PRIORITY_IMMEDIATE)
	{
		//Check if the event is valid.
		if(pEvent)
		{
			//Check the event type.
			switch(((CEvent *)pEvent)->GetEventType())
			{
				case EVENT_SCRIPT_COMMAND:
				{
					//Don't allow scripters to interrupt.  The blend looks like crap.
					return false;
				}
				default:
				{
					break;
				}
			}
		}
	}

	return CTask::ShouldAbort(iPriority, pEvent);
}

#if !__FINAL
void CTaskAnimatedHitByExplosion::Debug() const
{
#if DEBUG_DRAW && __DEV
	grcDebugDraw::Axis(m_lastMatrix, 1.0f, true);
#endif
}
#endif // !__FINAL

#if !__FINAL
const char* CTaskAnimatedHitByExplosion::GetStaticStateName(s32 iState)
{
	Assert(iState >= State_Dive && iState <= State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Dive",
		"State_GetUp",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif // !__FINAL

CTask::FSM_Return CTaskAnimatedHitByExplosion::ProcessPreFSM()
{
	//Ensure the ped is valid.
	CPed* pPed = GetPed();
	if(!pPed)
	{
		return FSM_Quit;
	}

	if (m_ragdollBlockingFlags.BitSet().AreAnySet() && GetTimeRunning()>=sm_Tunables.m_InitialRagdollDelay)
	{
		pPed->ClearRagdollBlockingFlags(m_ragdollBlockingFlags);
		m_ragdollBlockingFlags.BitSet().Reset();
	}

	return FSM_Continue;
}

void CTaskAnimatedHitByExplosion::CleanUp()
{
	CPed* pPed = GetPed();

	pPed->ClearBound();
}

CTask::FSM_Return CTaskAnimatedHitByExplosion::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		FSM_State(State_Dive)
			FSM_OnEnter
				Dive_OnEnter(pPed);
			FSM_OnUpdate
				return Dive_OnUpdate(pPed);

		FSM_State(State_GetUp)
			FSM_OnUpdate
				return GetUp_OnUpdate(pPed);

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

void CTaskAnimatedHitByExplosion::Dive_OnEnter(CPed* pPed)
{
	if (sm_Tunables.m_InitialRagdollDelay>0.0f)
	{
		m_ragdollBlockingFlags.BitSet().SetBits(BIT(RBF_EXPLOSION) | BIT(RBF_IMPACT_OBJECT) | BIT(RBF_ALLOW_BLOCK_DEAD_PED));
		m_ragdollBlockingFlags.BitSet().IntersectNegate(pPed->GetRagdollBlockingFlags().BitSet());

		pPed->ApplyRagdollBlockingFlags(m_ragdollBlockingFlags);
	}

	// Detach from a vehicle or anything else
	pPed->SetPedOutOfVehicle();
	pPed->DetachFromParent(0);

	// B* 1582472: Pass the reaction clip we're using up to the parent dead task if one exists, so it can use it to wait for a ragdoll settle.
	CTaskDyingDead* pDeadTask = static_cast<CTaskDyingDead*>(FindParentTaskOfType(CTaskTypes::TASK_DYING_DEAD));
	if (pDeadTask != NULL)
	{
		pDeadTask->SetDeathAnimationBySet(m_clipSetId, m_clipId);
	}

	CTaskFallAndGetUp* pNewTask = rage_new CTaskFallAndGetUp(m_clipSetId, m_clipId, 3.0f, m_startPhase);
	pNewTask->SetRunningLocally(true);

	SetNewTask(pNewTask);

	FreeMoverCapsule();
}

void CTaskAnimatedHitByExplosion::FreeMoverCapsule()
{
	CPed* pPed = GetPed();

	pPed->SetPedResetFlag( CPED_RESET_FLAG_DisablePedCapsuleControl, true );
	if (!NetworkInterface::IsGameInProgress() || !pPed->IsNetworkClone())
	{
		if (sm_Tunables.m_AllowPitchAndRoll)
			pPed->RemoveRotationalConstraint();
	}
	else
	{
		NetworkInterface::UseAnimatedRagdollFallbackBlending(*pPed);
	}

	//// Reduce angular inertia on the sides (to override a hack to force spherical inertia)
	//if (pPed->GetCollider())
	//{
	//	m_SavedAngInertia = pPed->GetCollider()->GetAngInertia();
	//	float inertia = m_SavedAngInertia.GetXf();
	//	static float mult1 = 1.0f;
	//	static float mult2 = 1.0f;
	//	static float mult3 = 0.5f;
	//	Vec3V newInertia(inertia*mult1, inertia*mult2, inertia*mult3);
	//	pPed->GetCollider()->SetInertia(m_SavedAngInertia.GetW().GetIntrin128ConstRef(), newInertia.GetIntrin128ConstRef());
	//}
}

CTask::FSM_Return CTaskAnimatedHitByExplosion::Dive_OnUpdate(CPed* pPed)
{
	pPed->SetPedResetFlag( CPED_RESET_FLAG_DisablePedCapsuleControl, true );
	if (pPed->IsNetworkClone())
	{
		NetworkInterface::UseAnimatedRagdollFallbackBlending(*pPed);
	}

	//the dying clip is finished, progress to the dead animated state
	if (GetIsSubtaskFinished(CTaskTypes::TASK_FALL_AND_GET_UP) || static_cast<CTaskFallAndGetUp*>(GetSubTask())->GetClipPhase() >= 1.0f)
	{
		SetState(State_GetUp);
		SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
		return FSM_Continue;
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskAnimatedHitByExplosion::GetUp_OnUpdate(CPed* pPed)
{
	if (GetIsSubtaskFinished(CTaskTypes::TASK_FALL_AND_GET_UP) || 
		(pPed->IsNetworkClone() && FindParentTaskOfType(CTaskTypes::TASK_DYING_DEAD) && pPed->IsDead()))
	{
		SetState(State_Finish);
	}

	return FSM_Continue;
}

