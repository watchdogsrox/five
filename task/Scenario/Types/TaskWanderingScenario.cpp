#include "TaskWanderingScenario.h"

#include "Peds/ped.h"
#include "Peds/PedCapsule.h"
#include "Peds/PedIntelligence.h"

#include "ai/ambient/AmbientModelSetManager.h"
#include "ai/BlockingBounds.h"
#include "Task/Default/AmbientAnimationManager.h"
#include "Task/General/TaskBasic.h"
#include "Task/Motion/TaskMotionBase.h"
#include "Task/Movement/TaskMoveWander.h"
#include "Task/Movement/TaskNavMesh.h"
#include "Task/Scenario/info/ScenarioInfo.h"
#include "Task/Scenario/info/ScenarioInfoManager.h"
#include "Task/Scenario/Types/TaskUseScenario.h"

AI_OPTIMISATIONS()

CTaskWanderingScenario::Tunables CTaskWanderingScenario::sm_Tunables;

IMPLEMENT_SCENARIO_TASK_TUNABLES(CTaskWanderingScenario, 0xcbaa2d4d);

CTaskWanderingScenario::CTaskWanderingScenario( s32 scenarioType )
		: CTaskScenario(scenarioType, NULL)
		, m_CurrentTargetPosV(V_ZERO)
		, m_CurrentScenario(NULL)
		, m_NextScenario(NULL)
		, m_CurrentScenarioType(-1)
		, m_NextScenarioType(-1)
		, m_CurrentScenarioOverrideMoveBlendRatio(-1.0f)
		, m_BlockingAreaLastCheckTime(0)
		, m_CurrentPositionIsBlocked(false)
		, m_CurrentScenarioIsBlocked(false)
		, m_NextScenarioIsBlocked(false)
		, m_UseCurrentScenarioInsteadOfNext(false)
		, m_NextScenarioOverrideMoveBlendRatio(-1.0f)
		, m_ActionWhenNotUsingScenario(None)
		, m_iCachedCurrWorldScenarioPointIndex(-1)
		, m_iCachedNextWorldScenarioPointIndex(-1)
{
	SetInternalTaskType(CTaskTypes::TASK_WANDERING_SCENARIO);
}

CTaskWanderingScenario::~CTaskWanderingScenario()
{
	// CleanUp() should prevent this assert.
	taskAssert(!m_pPropHelper || !m_pPropHelper->IsPropLoaded() || m_pPropHelper->IsPropSharedAmongTasks());
}

aiTask* CTaskWanderingScenario::Copy() const
{
	return rage_new CTaskWanderingScenario( m_iScenarioIndex );
}

void CTaskWanderingScenario::CleanUp()
{
	// If we allocated a prop helper
	if (m_pPropHelper)
	{
		// clean up the prop
		m_pPropHelper->ReleasePropRefIfUnshared(GetPed());
		m_pPropHelper.free();
	}
}

bool CTaskWanderingScenario::IsValidForMotionTask(CTaskMotionBase& task) const
{
	bool isValid = false;
	if (task.IsOnFoot())
	{
		isValid = true;
	}
	else if( task.IsInWater() )
	{
		if ( (m_CurrentScenario && m_CurrentScenario->IsInWater()) ||
			   (m_NextScenario    && m_NextScenario->IsInWater()) )
		{
			isValid = true;
		}
	}
	return isValid;
}


CTask::FSM_Return CTaskWanderingScenario::ProcessPreFSM()
{
	// Check if enough time has elapsed so that we should check if points are inside of blocking areas.
	const u32 currentTimeMs = fwTimer::GetTimeInMilliseconds();
	if(currentTimeMs > m_BlockingAreaLastCheckTime + sm_Tunables.m_TimeBetweenBlockingAreaChecksMS)
	{
		// Store a new time stamp.
		m_BlockingAreaLastCheckTime = currentTimeMs;

		// Check if the next point exists and is blocked.
		CScenarioPoint* nextPt = m_NextScenario;
		if(nextPt)
		{
			m_NextScenarioIsBlocked = CScenarioManager::IsScenarioPointInBlockingArea(*nextPt);
		}
		else
		{
			m_NextScenarioIsBlocked = false;
		}

		// Check if the current point exists and is blocked.
		CScenarioPoint* currentPt = m_CurrentScenario;
		if(currentPt)
		{
			m_CurrentScenarioIsBlocked = CScenarioManager::IsScenarioPointInBlockingArea(*currentPt);
		}
		else
		{
			m_CurrentScenarioIsBlocked = false;
		}

		// Check if either one of the scenario points was blocked. If so, we start caring about our
		// current position too. At least for now, we don't check that if we are just passing through.
		if(m_CurrentScenarioIsBlocked || m_NextScenarioIsBlocked)
		{
			const Vec3V& posV = GetPed()->GetMatrixRef().GetCol3ConstRef();
			m_CurrentPositionIsBlocked = CScenarioBlockingAreas::IsPointInsideBlockingAreas(RCC_VECTOR3(posV), true, false);
		}
	}

	return FSM_Continue;
}


CTask::FSM_Return CTaskWanderingScenario::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	FSM_Begin
		FSM_State(State_Start)
			FSM_OnEnter
				Start_OnEnter();
			FSM_OnUpdate
				return Start_OnUpdate();
		FSM_State(State_MovingToWanderScenario)
			FSM_OnEnter
				MovingToWanderScenario_OnEnter();
			FSM_OnUpdate
				return MovingToWanderScenario_OnUpdate();
			FSM_OnExit
				MovingToWanderScenario_OnExit();
		FSM_State(State_UsingScenario)
			FSM_OnEnter
				UsingScenario_OnEnter();
			FSM_OnUpdate
				return UsingScenario_OnUpdate();
		FSM_State(State_Wandering)
			FSM_OnEnter
				Wandering_OnEnter();
			FSM_OnUpdate
				return Wandering_OnUpdate();
		FSM_State(State_WaitForNextScenario)
			FSM_OnEnter
				WaitForNextScenario_OnEnter();
			FSM_OnUpdate
				return WaitForNextScenario_OnUpdate();
		FSM_State(State_WaitForBlockingArea)
			FSM_OnEnter
				WaitForBlockingArea_OnEnter();
			FSM_OnUpdate
				return WaitForBlockingArea_OnUpdate();
		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

#if !__FINAL
const char * CTaskWanderingScenario::GetStaticStateName( s32 iState  )
{
	const char* aStateNames[] = 
	{
		"State_Start",
		"State_MovingToWanderScenario",
		"State_UsingScenario",
		"State_Wandering",
		"State_WaitForNextScenario",
		"State_WaitForBlockingArea",
		"State_Finish",
	};
	CompileTimeAssert(NELEM(aStateNames) == kNumStates);

	return aStateNames[iState];
}

void CTaskWanderingScenario::Debug() const
{
	CTaskScenario::Debug();

//#if DEBUG_DRAW
//	if(m_CurrentScenario)
//	{
//		const Vec3V pos = m_CurrentScenario->GetPosition();
//		grcDebugDraw::Text(RCC_VECTOR3(pos), Color_white, 0, 0, "Current");
//	
//	}
//	if(m_NextScenario)
//	{
//		const Vec3V pos = m_NextScenario->GetPosition();
//		grcDebugDraw::Text(RCC_VECTOR3(pos), Color_white, 0, 0, "Next");
//	
//	}
//#endif	// DEBUG_DRAW
}

#endif // !__FINAL

void CTaskWanderingScenario::SetNextScenario(CScenarioPoint* pScenarioPoint, int scenarioType, float overrideMoveBlendRatio)
{
	Assert(!m_NextScenario);

	// If the scenario point changed, make sure we get it tested for blocking areas.
	if(m_NextScenario != pScenarioPoint)
	{
		m_BlockingAreaLastCheckTime = 0;
		m_NextScenarioIsBlocked = false;
	}

	taskAssertf(!pScenarioPoint || scenarioType >= 0, "Ped %s getting an invalid scenario type, the point is a %s at %.1f, %.1f, %.1f",
			GetPed() ? GetPed()->GetDebugName() : "INVALID",
			(pScenarioPoint->GetScenarioTypeVirtualOrReal() < SCENARIOINFOMGR.GetScenarioCount(true)) ? SCENARIOINFOMGR.GetNameForScenario(pScenarioPoint->GetScenarioTypeVirtualOrReal()) : "INVALID",
			pScenarioPoint->GetWorldPosition().GetXf(), pScenarioPoint->GetWorldPosition().GetYf(), pScenarioPoint->GetWorldPosition().GetZf());

	m_NextScenario = pScenarioPoint;
	m_NextScenarioType = scenarioType;
	m_NextScenarioOverrideMoveBlendRatio = overrideMoveBlendRatio;

	m_iCachedNextWorldScenarioPointIndex = -1;
}

void CTaskWanderingScenario::EndActiveScenarioTask()
{
	// if our subtask is a CTaskUseScenario task
	CTask* pSubTask = GetSubTask();
	if (pSubTask && (pSubTask->GetTaskType() == CTaskTypes::TASK_USE_SCENARIO || pSubTask->GetTaskType() == CTaskTypes::TASK_USE_VEHICLE_SCENARIO))
	{
		// end our subtask
		CTaskScenario* pTaskScenario = static_cast<CTaskScenario*>(pSubTask);
		pTaskScenario->SetTimeToLeave(0.0f);
	}
}

u32 CTaskWanderingScenario::GetAmbientFlags()
{
	// Base class
	u32 iFlags = CTaskScenario::GetAmbientFlags();
	iFlags |= CTaskAmbientClips::Flag_PlayIdleClips | CTaskAmbientClips::Flag_PlayBaseClip;
	return iFlags;
}


int CTaskWanderingScenario::GetWanderScenarioToUseNext(int realScenarioType, CPed* pPed)
{
	const CScenarioInfo* pScenarioInfo = CScenarioManager::GetScenarioInfo(realScenarioType);
	if(pScenarioInfo && pScenarioInfo->GetIsClass<CScenarioPlayAnimsInfo>())
	{
		const CScenarioPlayAnimsInfo& anims = *static_cast<const CScenarioPlayAnimsInfo*>(pScenarioInfo);
		const u32 nameHash = anims.GetWanderScenarioToUseAfterHash();
		if(nameHash)
		{
			const int wanderScenarioToUseAfter = CScenarioManager::ChooseRealScenario(SCENARIOINFOMGR.GetScenarioIndex(nameHash), pPed);
			if(taskVerifyf(wanderScenarioToUseAfter >= 0, "Failed to find scenario named '%s'.", anims.GetWanderScenarioToUseAfter()))
			{
				const CScenarioInfo* pWanderScenarioInfo = CScenarioManager::GetScenarioInfo(wanderScenarioToUseAfter);
				if(taskVerifyf(pWanderScenarioInfo->GetIsClass<CScenarioWanderingInfo>() || pWanderScenarioInfo->GetIsClass<CScenarioJoggingInfo>(),
						"Scenario '%s' is not a CScenarioWanderingInfo scenario.",
						anims.GetWanderScenarioToUseAfter()))
				{
					return wanderScenarioToUseAfter;
				}
			}
		}
	}

	return -1;
}


void CTaskWanderingScenario::Start_OnEnter()
{
	// dispose of the prop if needed
	CPropManagementHelper::DisposeOfPropIfInappropriate(*GetPed(), m_iScenarioIndex, m_pPropHelper);

	// If we're not restarting, see if something in the tree has a prop
	if (!m_pPropHelper)
	{
		// check if we already have one in our primary task tree
		m_pPropHelper = GetPed()->GetPedIntelligence()->GetActivePropManagementHelper();
	}

	CreatePropIfNecessary();
}


CTask::FSM_Return CTaskWanderingScenario::Start_OnUpdate()
{
	if(!m_UseCurrentScenarioInsteadOfNext)
	{
		m_CurrentScenario = NULL;
		m_CurrentScenarioType = -1;
		m_CurrentScenarioIsBlocked = false;

		m_iCachedCurrWorldScenarioPointIndex = -1;
	}

	// Cache our ped
	CPed* pPed = GetPed();

	// if we have a prop
	if (m_pPropHelper)
	{
		// update prop status
		m_pPropHelper->UpdateExistingOrMissingProp(*pPed);

		// Update the prop helper
		CPropManagementHelper::Context context(*pPed);
		context.m_ClipHelper = GetClipHelper();
		m_pPropHelper->Process(context);
	}

	if(m_ActionWhenNotUsingScenario == Quit)
	{
		return FSM_Quit;
	}

	if(GotNextScenario() || (m_UseCurrentScenarioInsteadOfNext && m_CurrentScenario))
	{
		// Check if the next scenario was found to be blocked. If so, enter a wait state
		// instead of moving to it.
		if(m_NextScenarioIsBlocked && !m_UseCurrentScenarioInsteadOfNext)
		{
			SetState(State_WaitForBlockingArea);
			return FSM_Continue;
		}

		const int newScenarioIndex = m_UseCurrentScenarioInsteadOfNext ? m_CurrentScenarioType : m_NextScenarioType;
		const CScenarioInfo* pScenarioInfo = CScenarioManager::GetScenarioInfo(newScenarioIndex);
#if __ASSERT
		const CScenarioPoint* pt = m_UseCurrentScenarioInsteadOfNext ? m_CurrentScenario : m_NextScenario;
#endif
		if(aiVerifyf(pScenarioInfo, "No scenario info for index %d. m_UseCurrentScenarioInsteadOfNext: %d, m_CurrentScenarioType: %d, m_NextScenarioType: %d, prev state: %d. Ped is %s, point is at %.1f, %.1f, %.1f.",
				newScenarioIndex, (int)m_UseCurrentScenarioInsteadOfNext, m_CurrentScenarioType, m_NextScenarioType, GetPreviousState(),
				GetPed() ? GetPed()->GetDebugName() : "INVALID",
				pt->GetWorldPosition().GetXf(), pt->GetWorldPosition().GetYf(), pt->GetWorldPosition().GetZf())
				&& (pScenarioInfo->GetIsClass<CScenarioWanderingInfo>() || pScenarioInfo->GetIsClass<CScenarioJoggingInfo>()))
		{
			SetState(State_MovingToWanderScenario);

			// If coming from a MovingToScenario state with a running movement task, we want
			// to keep it going so we can just change the destination.
			SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
		}
		else
		{
			SetState(State_UsingScenario);
			return FSM_Continue;
		}
	}
	else
	{
		SetState(State_Wandering);
	}

	return FSM_Continue;
}


void CTaskWanderingScenario::MovingToWanderScenario_OnEnter()
{
	Assert(GotNextScenario() || m_UseCurrentScenarioInsteadOfNext);

	// dispose of the prop if needed
	CPropManagementHelper::DisposeOfPropIfInappropriate(*GetPed(), m_iScenarioIndex, m_pPropHelper);

	// Make the next point the current point, unless we want to use the existing current point.
	if(!m_UseCurrentScenarioInsteadOfNext)
	{
		m_CurrentScenarioOverrideMoveBlendRatio = m_NextScenarioOverrideMoveBlendRatio;
		m_CurrentScenario = m_NextScenario;
		m_CurrentScenarioType = m_NextScenarioType;
		m_CurrentScenarioIsBlocked = m_NextScenarioIsBlocked;
		m_NextScenario = NULL;
		m_NextScenarioType = -1;
		m_NextScenarioOverrideMoveBlendRatio = -1.0f;
		m_NextScenarioIsBlocked = false;

		ClearCachedCurrAndNextWorldScenarioPointIndex();
	}
	m_UseCurrentScenarioInsteadOfNext = false;

	const Vec3V destV = m_CurrentScenario->GetPosition();
	const float overrideMoveBlendRatio = m_CurrentScenarioOverrideMoveBlendRatio;

	bool needsTask = true;
	CTask* pExistingSubTask = GetSubTask();

	// Workaround for an issue with the pointer to the previous state's subtask still being
	// set during OnEnter, even if KeepCurrentSubtaskAcrossTransition is not set.
	if(pExistingSubTask && !GetIsFlagSet(aiTaskFlags::KeepCurrentSubtaskAcrossTransition))
	{
		pExistingSubTask = NULL;
	}

	if(pExistingSubTask && pExistingSubTask->GetTaskType() == CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
	{
		CTaskComplexControlMovement* pControlTask = static_cast<CTaskComplexControlMovement*>(pExistingSubTask);

		const float mbr = GetMoveBlendRatio(overrideMoveBlendRatio);

		// Check if the move blend ratio matches what we've already got. If not, we won't try to
		// reuse the existing task. Theoretically one would think that calling
		// CTaskComplexControlMovement::SetMoveBlendRatio() would work, but somehow, that doesn't
		// actually update the move blend ratio that gets used.
		if(fabsf(mbr - pControlTask->GetMoveBlendRatio()) <= 0.01f)
		{
			if(pControlTask->GetMoveTaskType() == CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH)
			{
				// SetTarget() on the CTaskComplexControlMovement makes sure we update both the running
				// task and the backup task.
				pControlTask->SetTarget(GetPed(), RCC_VECTOR3(destV));
				needsTask = false;
			}

			// Perhaps best to call SetKeepMovingWhilstWaitingForPath() on the running task as well,
			// not sure if it's necessary but I found some time in the past where I had to do this
			// again when switching targets.
			CTask* pRunningTask = pControlTask->GetRunningMovementTask();
			if(pRunningTask && pRunningTask->GetTaskType() == CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH)
			{
				CTaskMoveFollowNavMesh* pNavMeshTask = static_cast<CTaskMoveFollowNavMesh*>(pRunningTask);
				pNavMeshTask->SetKeepMovingWhilstWaitingForPath(true);
			}
		}
	}

	if(needsTask)
	{
		CTask* pMoveTask = CreateTaskToMoveTo(destV, overrideMoveBlendRatio);
		SetNewTask(pMoveTask);
	}

	m_CurrentTargetPosV = destV;

	// Check for OpenDoor flag on our new m_CurrentScenario
	if (m_CurrentScenario && m_CurrentScenario->GetOpenDoor())
	{
		// enable open doors arm IK
		CPed* pPed = GetPed();
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_OpenDoorArmIK, true);
	}
}


CTask::FSM_Return CTaskWanderingScenario::MovingToWanderScenario_OnUpdate()
{
	CPed *pPed = GetPed(); //Get the ped ptr.
	pPed->GetPedIntelligence()->SetCheckShockFlag(true);

	// if we have a prop
	if (m_pPropHelper)
	{
		// Update prop status
		m_pPropHelper->UpdateExistingOrMissingProp(*pPed);

		// Update the prop helper
		CPropManagementHelper::Context context(*pPed);
		context.m_ClipHelper = GetClipHelper();
		m_pPropHelper->Process(context);
	}

	// Get the current position of the ped, without the root offset.
	const Vec3V currentPosWithOffsetV = pPed->GetTransform().GetPosition();
	Vec3V offsetV(V_ZERO);
	const CBaseCapsuleInfo* pCap = pPed->GetCapsuleInfo();
	if(pCap && !pPed->GetIsSwimming())
	{
		offsetV.SetZf(pCap->GetGroundToRootOffset());
	}
	const Vec3V currentPosV = Subtract(currentPosWithOffsetV, offsetV);

	// Measure the distance to the current target.
	const Vec3V tgtPosV = m_CurrentTargetPosV;
	const ScalarV distSqV  = DistSquared(tgtPosV, currentPosV);

	bool goToNext = false;
	if(GetIsSubtaskFinished(CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT))
	{
		// Don't do the check below for sharks.
		bool bCanEndEarly = !pPed->GetPedIntelligence()->GetNavCapabilities().IsFlagSet(CPedNavCapabilityInfo::FLAG_PREFER_NEAR_WATER_SURFACE);

		// This is a workaround to a nasty issue where the navigation task seems to think
		// it's done, even though the point where it's at is not the current destination but
		// the previous destination. If the navigation tasks worked as expected, this shouldn't
		// be necessary.
		static float s_MaxDistSq = square(1.0f);	// MAGIC!
		const ScalarV maxDistSqV(s_MaxDistSq);
		if(bCanEndEarly && IsGreaterThanAll(distSqV, maxDistSqV))
		{
			// We're nowhere near where we are supposed to be, so the navigation tasks appear
			// to have failed us. We will create a new navigation task to move to the position
			// where we want to be.
			SetNewTask(CreateTaskToMoveTo(m_CurrentTargetPosV, m_CurrentScenarioOverrideMoveBlendRatio));
			return FSM_Continue;
		}

		goToNext = true;
	}
	else if(m_CurrentScenario && m_CurrentScenarioIsBlocked)
	{
		// Wait for the blocking area to be lifted, but when we resume, go
		// back to m_CurrentScenario rather than m_NextScenario.
		m_UseCurrentScenarioInsteadOfNext = true;
		SetState(State_WaitForBlockingArea);
		return FSM_Continue;
	}
	else if(m_CurrentScenario)
	{
		// Check for OpenDoor flag
		if (m_CurrentScenario->GetOpenDoor())
		{
			// enable searching for doors while moving to the scenario point
			pPed->SetPedResetFlag(CPED_RESET_FLAG_SearchingForDoors, true);
		}

		// Next, we're going to check if we're close enough to the current destination to proceed to
		// the next, for smooth movement, or if we should switch to the scenario task if it's a true
		// scenario we are approaching.

		bool jogging = false;
		const CScenarioInfo* pWanderScenarioInfo = CScenarioManager::GetScenarioInfo(m_iScenarioIndex);
		if(pWanderScenarioInfo->GetIsClass<CScenarioJoggingInfo>())
		{
			jogging = true;
		}

		ScalarV thresholdDistV;

#if __ASSERT
		const CScenarioInfo* pDestScenarioInfo = CScenarioManager::GetScenarioInfo(m_CurrentScenarioType);
		taskAssert(pDestScenarioInfo->GetIsClass<CScenarioWanderingInfo>() || pDestScenarioInfo->GetIsClass<CScenarioJoggingInfo>());
#endif	// __ASSERT

		// Moving to another wander/jog scenario point, which we're just going to run through.
		if(GotNextScenario())
		{
			// Only interested in ending early if we've got another destination planned out.
			if(jogging)
			{
				thresholdDistV = LoadScalar32IntoScalarV(sm_Tunables.m_SwitchToNextPointDistJogging);
			}
			else
			{
				thresholdDistV = LoadScalar32IntoScalarV(sm_Tunables.m_SwitchToNextPointDistWalking);
			}
		}
		else
		{
			thresholdDistV = ScalarV(V_ZERO);
		}

		// Check if close enough.
		// Note: Could precompute the square this in the tuning data.
		const ScalarV thresholdDistSqV = Scale(thresholdDistV, thresholdDistV);
		if(IsLessThanAll(distSqV, thresholdDistSqV))
		{
			// Close enough, switch to the next point.
			goToNext = true;
		}
	}
	else
	{
		// If we got here, m_CurrentScenario is NULL and the scenario point we were going to must
		// have gotten deleted. Should probably move on to the next point if we've got it, or
		// perhaps it would be better to just quit?
		goToNext = true;
	}

	if(goToNext)
	{
		bool keepTask = false;
		if(m_CurrentScenario)
		{
			const int newScenarioIndex = m_CurrentScenarioType;
#if __ASSERT
			const CScenarioInfo* pScenarioInfo = CScenarioManager::GetScenarioInfo(newScenarioIndex);
			taskAssert(pScenarioInfo->GetIsClass<CScenarioWanderingInfo>() || pScenarioInfo->GetIsClass<CScenarioJoggingInfo>());
#endif	// __ASSERT

			// If we're switching from one wandering scenario to another, probably best to not
			// keep the movement task, to make sure the new scenario takes effect
			// (CTaskAmbientClips gets reinitialized).
			if(m_iScenarioIndex == newScenarioIndex)
			{
				keepTask = true;
			}
			m_iScenarioIndex = newScenarioIndex;
#if __BANK
			ValidateScenarioType(m_iScenarioIndex);
#endif
		}
		else
		{
			keepTask = true;
		}

		if(keepTask)
		{
			SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
		}

		SetState(State_Start);
		return FSM_Continue;
	}

	if (CScenarioSpeedHelper::ShouldUseSpeedMatching(*pPed))
	{
		CPed* pAmbientFriend = pPed->GetPedIntelligence()->GetAmbientFriend();
		if (pAmbientFriend)
		{
			Vec3V vTarget = GetScenarioPoint() ? GetScenarioPoint()->GetPosition() : pPed->GetTransform().GetPosition() + pPed->GetTransform().GetForward();
			float fMBR = CScenarioSpeedHelper::GetMBRToMatchSpeed(*pPed, *pAmbientFriend, vTarget);

			CTask* pTaskMovement = pPed->GetPedIntelligence()->GetActiveMovementTask();

			if (pTaskMovement)
			{
				pTaskMovement->PropagateNewMoveSpeed(fMBR);
			}
		}
	}

	return FSM_Continue;
}


void CTaskWanderingScenario::MovingToWanderScenario_OnExit()
{
	// If the navigation task issue occurs from MovingToWanderScenario_OnUpdate, we may not have an m_CurrentScenario
	// Check for OpenDoor flag on our m_CurrentScenario
	if (!m_CurrentScenario || m_CurrentScenario->GetOpenDoor())
	{
		// Turn off our arm IK flag
		GetPed()->SetPedConfigFlag(CPED_CONFIG_FLAG_OpenDoorArmIK, false);
	}
}


void CTaskWanderingScenario::UsingScenario_OnEnter()
{
	Assert(GotNextScenario() || m_UseCurrentScenarioInsteadOfNext);

	bool bDisposedOfProp = CPropManagementHelper::DisposeOfPropIfInappropriate(*GetPed(), m_iScenarioIndex, m_pPropHelper);

	// If we allocated a prop helper for a WanderingScenarioInfo
	if (m_pPropHelper && bDisposedOfProp)
	{
		// clean it up before starting a TaskUseScenario
		m_pPropHelper->ReleasePropRefIfUnshared(GetPed());
		m_pPropHelper.free();
	}

	// Advance to the next scenario.
	// Note: m_CurrentTargetPosV isn't set here, but it's only used in a different state (MovingToWanderScenario),
	// which always sets it itself.

	if(!m_UseCurrentScenarioInsteadOfNext)
	{
		m_CurrentScenario = m_NextScenario;
		m_CurrentScenarioType = m_NextScenarioType;
		m_CurrentScenarioOverrideMoveBlendRatio = m_NextScenarioOverrideMoveBlendRatio;

		m_NextScenario = NULL;
		m_NextScenarioType = -1;
		m_NextScenarioOverrideMoveBlendRatio = -1.0f;

		ClearCachedCurrAndNextWorldScenarioPointIndex();
	}
	else
	{
		m_UseCurrentScenarioInsteadOfNext = false;
	}

	const float mbr = GetMoveBlendRatio(m_CurrentScenarioOverrideMoveBlendRatio);

	Assert(m_CurrentScenario);
	// This is a ambient type task so using a scenario should do periodic checks for blocking areas and other conditions.
	CTask* pNewTask = CScenarioManager::CreateTask(m_CurrentScenario, m_CurrentScenarioType, true, mbr);

	if(pNewTask && pNewTask->GetTaskType() == CTaskTypes::TASK_USE_SCENARIO)
	{
		CTaskUseScenario* pUseScenarioTask = static_cast<CTaskUseScenario*>(pNewTask);

		const CConditionalAnimsGroup* pAnims = CScenarioManager::GetConditionalAnimsGroupForScenario(m_iScenarioIndex);
		if(pAnims)
		{
			CTaskAmbientClips* pAnimTask = rage_new CTaskAmbientClips(GetAmbientFlags(), pAnims);
			pUseScenarioTask->SetAdditionalTaskDuringApproach(pAnimTask);
		}
	}

	SetNewTask(pNewTask);
}


CTask::FSM_Return CTaskWanderingScenario::UsingScenario_OnUpdate()
{
	aiTask* pSubTask = GetSubTask();
	if(!pSubTask || m_Flags.IsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		if(m_CurrentPositionIsBlocked)
		{
			return FSM_Quit;
		}

		// Check if the scenario info for the scenario we finished using dictates
		// that we should switch to a potentially different wander scenario than the
		// one we're currently using.
		if(m_CurrentScenario)
		{
			const int wanderScenarioToUseAfter = CTaskWanderingScenario::GetWanderScenarioToUseNext(m_CurrentScenarioType, GetPed());
			if(wanderScenarioToUseAfter >= 0)
			{
				m_iScenarioIndex = wanderScenarioToUseAfter;
#if __BANK
				ValidateScenarioType(m_iScenarioIndex);
#endif
			}
		}

		SetState(State_Start);
		return FSM_Continue;
	}

	// If your subtask isn't done and it's using a Scenario, allow it to exit if you have another chained point to go to. [1/21/2013 mdawe]
	if (pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_USE_SCENARIO)
	{
		CTaskUseScenario* pUseScenarioSubtask = static_cast<CTaskUseScenario*>(pSubTask);

		// If the current scenario we are moving to is blocked, see if we should
		// quit the CTaskUseScenario subtask before we get there.
		if(m_CurrentScenarioIsBlocked)
		{
			const int useScenarioState = pUseScenarioSubtask->GetState();
			if(useScenarioState == CTaskUseScenario::State_Start || useScenarioState == CTaskUseScenario::State_GoToStartPosition)
			{
				// Wait for it to be lifted.
				m_UseCurrentScenarioInsteadOfNext = true;
				SetState(State_WaitForBlockingArea);
				return FSM_Continue;
			}
		}

		// If the next scenario is blocked, the one after our current CTaskUseScenario subtask...
		if(m_NextScenarioIsBlocked)
		{
			// Don't leave this scenario, if the current position is good.
			if(!m_CurrentPositionIsBlocked)
			{
				pUseScenarioSubtask->SetDontLeaveThisFrame();
			}
			else
			{
				// If this is not a good position to be, ask CTaskUseScenario to quit.
				// We still need to play exit animations, etc.
				pUseScenarioSubtask->SetShouldAbort();
			}

			return FSM_Continue;
		}

		if(m_CurrentScenario && m_CurrentScenario->IsChained() && GotNextScenario())
		{
			pUseScenarioSubtask->SetCanExitToChainedScenario();
		}
	}

	return FSM_Continue;
}


void CTaskWanderingScenario::Wandering_OnEnter()
{
	float fMoveBlendRatio = GetMoveBlendRatio(-1.0f);
	CTaskMoveWander* pSubTask = rage_new CTaskMoveWander( fMoveBlendRatio, 0, true, 0.05f, true, false);
	SetNewTask( rage_new CTaskComplexControlMovement( pSubTask, rage_new CTaskAmbientClips( GetAmbientFlags(), CScenarioManager::GetConditionalAnimsGroupForScenario(m_iScenarioIndex) )));
}


CTask::FSM_Return CTaskWanderingScenario::Wandering_OnUpdate()
{
	CPed *pPed = GetPed(); //Get the ped ptr.
	pPed->GetPedIntelligence()->SetCheckShockFlag(true);

	if(m_ActionWhenNotUsingScenario == Quit)
	{
		return FSM_Quit;
	}
	else if(m_ActionWhenNotUsingScenario == WaitForNextScenario)
	{
		SetState(State_WaitForNextScenario);
	}

	if( GetIsFlagSet(aiTaskFlags::SubTaskFinished) )
		return FSM_Quit;

	return FSM_Continue;
}

void CTaskWanderingScenario::WaitForNextScenario_OnEnter()
{
	//Create the move task.
	CTask* pMoveTask = rage_new CTaskMoveStandStill();
	
	//Create the task.
	CTask* pTask = rage_new CTaskComplexControlMovement(pMoveTask);
	
	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskWanderingScenario::WaitForNextScenario_OnUpdate()
{
	//Check if we have a next scenario.
	if(GotNextScenario())
	{
		SetState(State_Start);
	}

	return FSM_Continue;
}

void CTaskWanderingScenario::WaitForBlockingArea_OnEnter()
{
	//Create the move task.
	CTask* pMoveTask = rage_new CTaskMoveStandStill();
	
	//Create the task.
	CTask* pTask = rage_new CTaskComplexControlMovement(pMoveTask);
	
	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskWanderingScenario::WaitForBlockingArea_OnUpdate()
{
	// If our current position is blocked, quit. Also quit if we have been in this
	// state for too long, probably don't want anybody to just stand still indefinitely.
	if(m_CurrentPositionIsBlocked || GetTimeInState() > sm_Tunables.m_MaxTimeWaitingForBlockingArea)
	{
		return FSM_Quit;
	}

	// As long as either scenario point is blocked, stay here.
	if(m_CurrentScenarioIsBlocked || m_NextScenarioIsBlocked)
	{
		return FSM_Continue;
	}

	// Resume.
	SetState(State_Start);
	return FSM_Continue;
}

CTask* CTaskWanderingScenario::CreateTaskToMoveTo(Vec3V_ConstRef dest, float overrideMoveBlendRatio)
{
	CPed* pPed = GetPed();

	CNavParams params;
	params.m_vTarget.Set(RCC_VECTOR3(dest));
	params.m_fMoveBlendRatio = GetMoveBlendRatio(overrideMoveBlendRatio);
	params.m_fSlowDownDistance = 0.0f;
	params.m_fTargetRadius = CTaskMoveFollowNavMesh::ms_fTargetRadius;
	params.m_fCompletionRadius = CTaskMoveFollowNavMesh::ms_fDefaultCompletionRadius;

	CTaskMove* pSubTask = NULL;

	CTaskMotionBase* motionTask = pPed->GetPrimaryMotionTask();
	if (motionTask && motionTask->IsSwimming())
	{
		// Sea.
		float fRadius = CTaskMoveGoToPoint::ms_fTargetRadius;
		if (pPed->GetPedIntelligence()->GetNavCapabilities().IsFlagSet(CPedNavCapabilityInfo::FLAG_PREFER_NEAR_WATER_SURFACE))
		{
			fRadius = sm_Tunables.m_PreferNearWaterSurfaceArrivalRadius;	
		}
		pSubTask = rage_new CTaskMoveGoToPoint(GetMoveBlendRatio(overrideMoveBlendRatio), RCC_VECTOR3(dest), fRadius);
	}
	else
	{
		CTaskMoveFollowNavMesh* pNavSubTask = rage_new CTaskMoveFollowNavMesh(params);
		pNavSubTask->SetKeepMovingWhilstWaitingForPath(true);
		pNavSubTask->SetDontStandStillAtEnd(true);
		pSubTask = pNavSubTask;
	}

	// TODO: This should perhaps use CTaskGoToScenario, at least for the cases where
	// we will CTaskUseScenario next.

	return rage_new CTaskComplexControlMovement(pSubTask, rage_new CTaskAmbientClips(GetAmbientFlags(), CScenarioManager::GetConditionalAnimsGroupForScenario(m_iScenarioIndex)));
}


void CTaskWanderingScenario::CreatePropIfNecessary()
{
	// Cache our Ped pointer
	CPed* pPed = GetPed();

	// get our current ambient flags
	u32 iAmbientFlags = GetAmbientFlags();

	// get a reference to the conditional anims group
	const CConditionalAnimsGroup* pConditionalAnimsGroup = CScenarioManager::GetConditionalAnimsGroupForScenario(m_iScenarioIndex);

	// init our default anims chosen
	s32 iConditionalAnimsChosen = -1;

	// if we have a conditional anims group
	if (taskVerifyf(pConditionalAnimsGroup, "Couldn't find conditional anims group for scenario index %i", m_iScenarioIndex))
	{
		// init our condition data
		CScenarioCondition::sScenarioConditionData conditionData;
		conditionData.pPed = pPed;
		conditionData.iScenarioFlags = iAmbientFlags;
		// allow all anims in group, because we can create a prop if necessary
		conditionData.iScenarioFlags |= SF_IgnoreHasPropCheck;

		// default our priority
		s32 nPriority = 0;

		// if we found valid conditional anims
		if (CAmbientAnimationManager::ChooseConditionalAnimations(conditionData, nPriority, pConditionalAnimsGroup, iConditionalAnimsChosen, CConditionalAnims::AT_BASE))
		{
			// check if the anims exist
			const CConditionalAnims* pConditionalAnims = pConditionalAnimsGroup->GetAnims(iConditionalAnimsChosen);
			if (taskVerifyf(pConditionalAnims, "Could not get conditional anims from anims group in TaskWanderingScenario"))
			{
				// if we need a prop
				u32 uPropSetHash = pConditionalAnims->GetPropSetHash();
				if (uPropSetHash)
				{
					// check if the propset from the hash is valid
					const CAmbientPropModelSet* pPropSet = CAmbientAnimationManager::GetPropSetFromHash(uPropSetHash);
					if (taskVerifyf(pPropSet, "Could not get a propset from scenario info hash in TaskWanderingScenario"))
					{
						// if the model index from the propset is valid
						strLocalIndex uPropModelIndex = pPropSet->GetRandomModelIndex();
						if (taskVerifyf(CModelInfo::IsValidModelInfo(uPropModelIndex.Get()), "Prop set [%s] with model index [%d] not a valid model index", pPropSet->GetName(), uPropModelIndex.Get()))
						{
							// get the model info
							const CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(fwModelId(uPropModelIndex));
							taskAssert(pModelInfo);

							// if we don't have a prop helper
							if (!m_pPropHelper)
							{
								// create the prop helper
								m_pPropHelper = rage_new CPropManagementHelper();

								// request the prop
								m_pPropHelper->RequestProp(pModelInfo->GetModelNameHash());

								// set flags on the prop helper
								m_pPropHelper->SetCreatePropInLeftHand(pConditionalAnims->GetCreatePropInLeftHand());
								m_pPropHelper->SetDestroyProp(pConditionalAnims->ShouldDestroyPropInsteadOfDropping());
								m_pPropHelper->SetForcedLoading();
							}
							else
							{
								// assert here if prop helper's hash isn't the same
								taskAssertf(m_pPropHelper->GetPropNameHash() != uPropSetHash, "CTaskWanderingScenario::CreateTaskToMoveTo - A wandering scenario chain has multiple props!");
							}
						}
					}
				}
			}
		}
	}
}


float CTaskWanderingScenario::GetMoveBlendRatio(float overrideMoveBlendRatio) const
{
	if(overrideMoveBlendRatio >= 0.0f)
	{
		return overrideMoveBlendRatio;
	}

	float fMoveBlendRatio = MOVEBLENDRATIO_WALK;
	const CScenarioInfo* pScenarioInfo = CScenarioManager::GetScenarioInfo(m_iScenarioIndex);
	Assert(pScenarioInfo);
	if( pScenarioInfo->GetIsClass<CScenarioJoggingInfo>() )
	{
		fMoveBlendRatio = MOVEBLENDRATIO_RUN;
	}
	return fMoveBlendRatio;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// CTaskWanderingScenario Clone implementation
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

CTaskInfo* CTaskWanderingScenario::CreateQueriableState() const
{
	CTaskInfo* pInfo = NULL;

	if(m_CurrentScenario==NULL && m_iCachedCurrWorldScenarioPointIndex!=-1)
	{
		m_iCachedCurrWorldScenarioPointIndex=-1;
	}

	if(m_NextScenario==NULL && m_iCachedNextWorldScenarioPointIndex!=-1)
	{
		m_iCachedNextWorldScenarioPointIndex=-1;
	}

	pInfo =  rage_new CClonedTaskWanderingScenarioInfo(	GetState(),
														static_cast<s16>(GetScenarioType()),
														GetScenarioPoint(),
														GetIsWorldScenario(),
														GetWorldScenarioPointIndex(),
														GetWorldScenarioPointIndex(m_CurrentScenario, &m_iCachedCurrWorldScenarioPointIndex ASSERT_ONLY(, false)),
														GetWorldScenarioPointIndex(m_NextScenario, &m_iCachedNextWorldScenarioPointIndex ASSERT_ONLY(, false)),
														m_CurrentScenarioOverrideMoveBlendRatio,
														m_NextScenarioOverrideMoveBlendRatio);

	return pInfo;
}

void CTaskWanderingScenario::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	Assert(pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_WANDER_SCENARIO);

	CTaskFSMClone::ReadQueriableState(pTaskInfo);
}

CTask::FSM_Return CTaskWanderingScenario::UpdateClonedFSM(const s32 UNUSED_PARAM(iState), const FSM_Event UNUSED_PARAM(iEvent) )
{
	if(GetStateFromNetwork() == State_Finish)
	{
		return FSM_Quit;
	}

	return FSM_Continue;
}

CTaskFSMClone *CTaskWanderingScenario::CreateTaskForClonePed(CPed *UNUSED_PARAM(pPed))
{
	CTaskWanderingScenario* pTask = NULL;

	pTask =	static_cast<CTaskWanderingScenario*>(Copy());
	
	pTask->m_iCachedCurrWorldScenarioPointIndex = GetWorldScenarioPointIndex(m_CurrentScenario);
	pTask->m_iCachedNextWorldScenarioPointIndex = GetWorldScenarioPointIndex(m_NextScenario);

	return pTask;
}


CTaskFSMClone *CTaskWanderingScenario::CreateTaskForLocalPed(CPed *pPed)
{
	// do the same as clone ped
	return CreateTaskForClonePed(pPed);
}

bool CTaskWanderingScenario::IsInScope(const CPed* pPed)
{
	bool inScope = CTaskScenario::IsInScope(pPed);

	if(inScope)
	{
		return AreClonesCurrentAndNextScenariosValid();
	}

	return inScope;
}

bool CTaskWanderingScenario::AreClonesCurrentAndNextScenariosValid()
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	if(!pPed)
	{
		Assertf(0, "Null Ped when checking scenario point validity");
		return false;
	}
	if (pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(GetTaskType()))
	{
		CTaskInfo* pTaskInfo = pPed->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(GetTaskType(), PED_TASK_PRIORITY_MAX);

		CClonedTaskWanderingScenarioInfo* pScenarioInfo = dynamic_cast<CClonedTaskWanderingScenarioInfo*>(pTaskInfo);

		if(taskVerifyf(pScenarioInfo,"Expect ped to have current CClonedTaskUseVehicleScenarioInfo") )
		{
			if(pScenarioInfo->GetCurrScenarioPointIndex()!=-1 && !pScenarioInfo->GetCurrScenarioPoint())
			{
				return false;
			}

			if(pScenarioInfo->GetNextScenarioPointIndex()!=-1 && !pScenarioInfo->GetNextScenarioPoint())
			{
				return false;
			}
		}
	}

	return true;
}

void CTaskWanderingScenario::OnCloneTaskNoLongerRunningOnOwner()
{
	SetStateFromNetwork(State_Finish);
}

CClonedTaskWanderingScenarioInfo::CClonedTaskWanderingScenarioInfo(s32 state,
													   s16 scenarioType,
													   CScenarioPoint* pScenarioPoint,
													   bool bIsWorldScenario,
													   int worldScenarioPointIndex,		
													   s32 currScenarioPointIndex,
													   s32 nextScenarioPointIndex,
													   float 	fCurrentScenarioOverrideMBR,
													   float	fNextScenarioOverrideMBR)

: CClonedScenarioFSMInfo( state, scenarioType, pScenarioPoint, bIsWorldScenario, worldScenarioPointIndex)
, m_CurrScenarioPointIndex(currScenarioPointIndex)
, m_NextScenarioPointIndex(nextScenarioPointIndex)
{
	m_CurrentScenarioOverrideMBR = PackMoveBlendRatio(fCurrentScenarioOverrideMBR);
	m_NextScenarioOverrideMBR = PackMoveBlendRatio(fNextScenarioOverrideMBR);
	SetStatusFromMainTaskState(state);
}

CClonedTaskWanderingScenarioInfo::CClonedTaskWanderingScenarioInfo()
: m_CurrScenarioPointIndex(-1)
, m_NextScenarioPointIndex(-1)
, m_CurrentScenarioOverrideMBR(0)
, m_NextScenarioOverrideMBR(0)
{

}

CTaskFSMClone *CClonedTaskWanderingScenarioInfo::CreateCloneFSMTask()
{
	CTaskWanderingScenario* pTask=NULL;

	Assertf(GetScenarioType()!=-1,"Expect valid scenario type");

	pTask =  rage_new CTaskWanderingScenario(GetScenarioType());

	return pTask;
}

CTask *CClonedTaskWanderingScenarioInfo::CreateLocalTask(fwEntity* pEntity)
{
	CTaskWanderingScenario* pTask=NULL;

	CScenarioPoint* pCurrentSP = GetCurrScenarioPoint();
	CScenarioPoint* pNextSP = GetNextScenarioPoint();

	Assertf(GetScenarioType()!=-1,"Expect valid scenario type");

	pTask =  rage_new CTaskWanderingScenario(GetScenarioType());

	pTask->m_CurrentScenario = pCurrentSP;
	pTask->m_NextScenario = pNextSP;

	// Pick a "next" type - these aren't synched, so we could theoretically pick a different one than whoever owned this ped last.
	// In practice that won't matter much for wandering scenarios, as there aren't virtual wandering types.
	// This is probably the best we can do without synching the types (and the use current instead of next bool).
	if (pNextSP && pEntity && pEntity->GetType() == ENTITY_TYPE_PED)
	{
		CPed* pPed = (CPed*)pEntity;
		pTask->m_NextScenarioType = CScenarioManager::ChooseRealScenario(pNextSP->GetScenarioTypeVirtualOrReal(), pPed);
	}

	pTask->m_NextScenarioOverrideMoveBlendRatio = UnpackMoveBlendRatio(GetNextScenarioOverrideMBR());
	pTask->m_CurrentScenarioOverrideMoveBlendRatio = UnpackMoveBlendRatio(GetCurrentScenarioOverrideMBR());

	return pTask;
}

CScenarioPoint* CClonedTaskWanderingScenarioInfo::GetCurrScenarioPoint() 
{
	return GetScenarioPoint( m_CurrScenarioPointIndex );
}

CScenarioPoint* CClonedTaskWanderingScenarioInfo::GetNextScenarioPoint() 
{
	return GetScenarioPoint( m_NextScenarioPointIndex );
}

u8 CClonedTaskWanderingScenarioInfo::PackMoveBlendRatio(float fMBR)
{
	u8 iMoveBlendRatio = 0;

	if (fMBR == MOVEBLENDRATIO_STILL)
		iMoveBlendRatio = 1;
	else if (fMBR == MOVEBLENDRATIO_WALK)
		iMoveBlendRatio = 2;
	else if (fMBR == MOVEBLENDRATIO_RUN)
		iMoveBlendRatio = 3;
	else if (fMBR == MOVEBLENDRATIO_SPRINT)
		iMoveBlendRatio = 4;
	else if (fMBR == ONE_OVER_MOVEBLENDRATIO_SPRINT)
		iMoveBlendRatio = 5;

	return iMoveBlendRatio;
}

float CClonedTaskWanderingScenarioInfo::UnpackMoveBlendRatio(u32 iMoveBlendRatio)
{
	float fMBR = -1.0f;  // although -1.0f is the same as MOVEBLENDRATIO_REVERSE, that value is not used currently in CTaskWanderingScenario and -1.0f is used as default here

	switch (iMoveBlendRatio)
	{
	case 1:
		fMBR = MOVEBLENDRATIO_STILL;
		break;
	case 2:
		fMBR = MOVEBLENDRATIO_WALK;
		break;
	case 3:
		fMBR = MOVEBLENDRATIO_RUN;
		break;
	case 4:
		fMBR = MOVEBLENDRATIO_SPRINT;
		break;
	case 5:
		fMBR = ONE_OVER_MOVEBLENDRATIO_SPRINT;
		break;
	default:
		break;
	}

	return fMBR;
}





