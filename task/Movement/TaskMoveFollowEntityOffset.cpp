// FILE :    TaskMoveFollowEntityOffset.cpp
// PURPOSE : Moves a ped to the offset of another entity constantly.
// AUTHOR :  Chi-Wai Chiu
// CREATED : 29-01-2009

// Game headers
#include "Task/Movement/TaskMoveFollowEntityOffset.h"

#include "Debug/DebugScene.h"
#include "Task/Movement/TaskSeekEntity.h"
#include "Task/Movement/TaskGoTo.h"
#include "Task/Scenario/Types/TaskCoupleScenario.h"
#include "Peds/Ped.h"

AI_OPTIMISATIONS()

////////////////////////////////////
// TASK MOVE FOLLOW ENTITY OFFSET //
////////////////////////////////////

bank_float CTaskMoveFollowEntityOffset::ms_fQuitDistance	= 50.0f;	// Distance to quit task 
bank_float CTaskMoveFollowEntityOffset::ms_fSeekDistance = 20.0f;	// Distance to start seek task
bank_float CTaskMoveFollowEntityOffset::ms_fYOffsetDeltaWhilstMatching = 0.2f;
bank_float CTaskMoveFollowEntityOffset::ms_fMinTimeToStayInMatching = 1.0f;

bank_s32 CTaskMoveFollowEntityOffset::ms_iScanTime = 10; // 0.01 Seconds
bank_float CTaskMoveFollowEntityOffset::ms_fResetTargetDistance = 0.1f;
bank_float CTaskMoveFollowEntityOffset::ms_fwaitBeforeRestartingSeek = 5.0f;
bank_float CTaskMoveFollowEntityOffset::ms_fSlowdownEps = 1.0f;		// The distance in front of the target position we begin to slow down
bank_float CTaskMoveFollowEntityOffset::ms_fMatchingEps = 0.5f;		// The zone in which we attempt to match speed with the partner
bank_float CTaskMoveFollowEntityOffset::ms_fSpeedUpEps  = 1.0f;		// If we slip behind the partner by this amount or more, try to speed up

bank_float CTaskMoveFollowEntityOffset::ms_fSlowDownDelta = -0.5f;
bank_float CTaskMoveFollowEntityOffset::ms_fSpeedUpDelta  = 0.2f; //0.5f;

bank_bool CTaskMoveFollowEntityOffset::ms_bUseWallDetection = true;

bank_float  CTaskMoveFollowEntityOffset::ms_fLowProbeOffset = -0.25f;
bank_float  CTaskMoveFollowEntityOffset::ms_fHighProbeOffset = 0.5f;
bank_float  CTaskMoveFollowEntityOffset::ms_fProbeDistance	= 5.0f;

bank_float  CTaskMoveFollowEntityOffset::ms_fProbeResetTime = 2.0f;

bank_float CTaskMoveFollowEntityOffset::ms_fDefaultYWalkBehindOffset = -1.0f;
bank_float CTaskMoveFollowEntityOffset::ms_fDefaultTimeToUseNavMeshIfCatchingUp = 5.0f;
bank_float CTaskMoveFollowEntityOffset::ms_fDefaultCheckForStuckTime = 3.0f;
bank_float CTaskMoveFollowEntityOffset::ms_fStuckDistanceTolerance = 0.5f;

bank_float CTaskMoveFollowEntityOffset::ms_fToleranceWhilstMatching = 1.0f;

bank_float CTaskMoveFollowEntityOffset::ms_fStartMovingDistance = 1.2f;

CTaskMoveFollowEntityOffset::CTaskMoveFollowEntityOffset(const CEntity* pEntityTarget, const float fMoveBlendRatio, const float fTargetRadius, const Vector3& vOffset, int iTimeToFollow, bool bRelativeOffset)
: 
CTaskMove(fMoveBlendRatio),
m_pEntityTarget(pEntityTarget),
m_fTargetRadius(fTargetRadius),
m_vOffset(vOffset),
m_bRelativeOffset(bRelativeOffset),
m_vWorldSpacePosition(Vector3::ZeroType),
m_iTimeToFollow(iTimeToFollow),
m_waitBeforeRestartingSeekTimer(ms_fwaitBeforeRestartingSeek),
m_eCurrentProbeStatus(Probe_Low),
m_probeTimer(ms_fProbeResetTime),
m_bAllClear(true),
m_bFollowingBehind(false),
m_catchUpTimer(ms_fDefaultTimeToUseNavMeshIfCatchingUp),
m_stuckTimer(ms_fDefaultCheckForStuckTime),
m_matchingTimer(ms_fMinTimeToStayInMatching),
m_bRequestStop(false),
m_bClampMoveBlendRatio(false)
{
	CalculateWorldPosition();
	SetInternalTaskType(CTaskTypes::TASK_MOVE_FOLLOW_ENTITY_OFFSET);

	for(s32 i=0; i<Num_Probes; i++)
		m_aCurrentProbeResults[i] = true;
}

CTaskMoveFollowEntityOffset::~CTaskMoveFollowEntityOffset()
{
}

aiTask* CTaskMoveFollowEntityOffset::Copy() const
{
	CTaskMoveFollowEntityOffset* pTask = rage_new CTaskMoveFollowEntityOffset(m_pEntityTarget.Get(), m_fMoveBlendRatio, m_fTargetRadius, m_vOffset, m_iTimeToFollow, m_bRelativeOffset);
	pTask->SetClampMoveBlendRatio(GetClampMoveBlendRatio());
	
	return pTask;
}

Vector3 CTaskMoveFollowEntityOffset::GetTarget() const
{
	return m_vWorldSpacePosition;
}

float CTaskMoveFollowEntityOffset::GetTargetRadius() const
{
	if (m_pEntityTarget)
	{	
		return m_pEntityTarget->GetBoundRadius();
	}
	return 0.0f;
}

void CTaskMoveFollowEntityOffset::IssueProbe(CPed* pPed)
{
	Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	Vector3 vForwardProbe(0.0f,ms_fProbeDistance,0.0f);
	vForwardProbe.RotateZ(pPed->GetTransform().GetHeading());

	switch (m_eCurrentProbeStatus)
	{
		case Probe_Low: 
			{
				vForwardProbe.z += ms_fLowProbeOffset;
				vPedPos.z += ms_fLowProbeOffset;
			}
			break;
		case Probe_Medium: 
			{
				// Default, no need for offsets
			}
			break;
		case Probe_High: 
			{
				vForwardProbe.z += ms_fHighProbeOffset; 
				vPedPos.z += ms_fHighProbeOffset;
			}
			break;
		default: taskAssert(0); return;
	}

	// Create an exclusion list containing the ped and any attached props
	static const int nTestTypes = ArchetypeFlags::GTA_MAP_TYPE_MOVER|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_OBJECT_TYPE;
	const CEntity* exclusionList[MAX_NUM_ENTITIES_ATTACHED_TO_PED+2];
	int nExclusions = 0;
	pPed->GeneratePhysExclusionList(exclusionList, nExclusions, MAX_NUM_ENTITIES_ATTACHED_TO_PED, nTestTypes, TYPE_FLAGS_ALL);
	exclusionList[nExclusions++] = pPed;

	WorldProbe::CShapeTestProbeDesc probeData;
	probeData.SetStartAndEnd(vPedPos, vPedPos+vForwardProbe);
	probeData.SetContext(WorldProbe::EMovementAI);
	probeData.SetExcludeEntities(exclusionList, nExclusions);
	probeData.SetIncludeFlags(nTestTypes);
	m_asyncProbeHelper.StartTestLineOfSight(probeData);
}

#if !__FINAL
#if DEBUG_DRAW
void CTaskMoveFollowEntityOffset::RenderProbeLine(const Vector3& vFrom, const Vector3& vTo, bool bClear) const
{
	if (bClear)
	{
		grcDebugDraw::Line(vFrom,vTo, Color_green, Color_green);
	}	
	else
	{
		grcDebugDraw::Line(vFrom,vTo, Color_red, Color_red);
	}
}
#endif // DEBUG_DRAW
#endif // !__FINAL


CTask::FSM_Return CTaskMoveFollowEntityOffset::ProcessPreFSM()
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	// If the target becomes null quit
	if (!m_pEntityTarget || !m_pEntityTarget->GetIsPhysical())
	{	
		return FSM_Quit;
	}

	taskAssert(m_pEntityTarget);
	if (m_iTimeToFollow > 0.0f && m_followTimer.IsSet() && m_followTimer.IsOutOfTime())
	{
		return FSM_Quit;
	}

	// If the target entity moves out of the prescribed range, set the entity target again
	if (m_vWorldSpacePosition.Dist(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition())) > ms_fResetTargetDistance)
	{
		if (GetSubTask() && GetSubTask()->IsMoveTask())
		{
			CTaskMoveInterface* pMoveTask = static_cast<CTaskMove*>(GetSubTask())->GetMoveInterface();
			if (pMoveTask)
			{
				pMoveTask->SetTarget(pPed, VECTOR3_TO_VEC3V(m_vWorldSpacePosition));
			}
		}
	}

	UpdateProbes(pPed);

	// Recalculate the world position of the target entity
	if (m_bRelativeOffset)
	{
		CalculateWorldPosition();
	}

	return FSM_Continue;
}

void CTaskMoveFollowEntityOffset::CheckForStateChange(CPed* pPed)
{
	if (m_bRequestStop)
	{
		SetState(State_SlowingDown);
		return;
	}

	// Check if we should follow behind
	m_bAllClear = true;

	// Check the probe results
	for (s32 i = 0; i < Num_Probes; ++i)
	{
		// If one is blocked, move offset to behind the entity
		if (!m_aCurrentProbeResults[i])
		{
			m_bAllClear = false;
			m_bFollowingBehind = true;
			if (m_catchUpTimer.IsFinished())
			{
				m_catchUpTimer.Reset();
			}
		}
	}

	// We are following
	if (m_bFollowingBehind)
	{
		if (m_bAllClear)
		{
			m_bFollowingBehind = false;
		}
		else if (m_catchUpTimer.Tick())
		{
			SetState(State_FollowNavMesh);
			return;
		}
	}

	const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	// If we've not moved much, use seek entity as we might be stuck
	if (m_stuckTimer.Tick())
	{
		m_stuckTimer.Reset();
		if (vPedPosition.Dist(m_vPreviousPosition) < ms_fStuckDistanceTolerance)
		{
			SetState(State_FollowNavMesh);
			return;
		}
		m_vPreviousPosition = vPedPosition;
	}

	// Check the distance between me and my target position
	float fDistBetween = vPedPosition.Dist(m_vWorldSpacePosition);

	// If we're further than the quit distance, finish the task
	if (fDistBetween > ms_fQuitDistance)
	{
		SetState(State_Finish);
		return;
	}
	// If we're further than the seek distance,
	else if (fDistBetween > ms_fSeekDistance)
	{
		if (m_waitBeforeRestartingSeekTimer.Tick())
		{
			SetState(State_FollowNavMesh);
			return;
		}
	}
	// Otherwise we're close enough to try catching up, matching or slowing down
	else
	{
		// Peds at a cover point (e.g a vehicle) were staying in catching up state
		// because they weren't close enough to the vehicles position (central)
		if (!m_pEntityTarget->GetIsTypePed() && fDistBetween < 5.0f)
		{
			SetState(State_Matching);
			return;
		}

		if (m_pEntityTarget->GetIsTypePed())
		{
			const CPed* pPed = static_cast<const CPed*>(m_pEntityTarget.Get());
			if (pPed && pPed->GetIsInVehicle())
			{
				SetState(State_Matching);
				return;
			}
		}

		// Close to target, so try to match speed
		if (fDistBetween < ms_fMatchingEps)
		{
			SetState(State_Matching);
			return;
		}
		// Outside of matching zone, determine if we're in front or behind
		else
		{
			Vector3 vToTarget = vPedPosition - VEC3V_TO_VECTOR3(m_pEntityTarget->GetTransform().GetPosition());
			vToTarget.Normalize();
			Vector3 vTargetForward = VEC3V_TO_VECTOR3(m_pEntityTarget->GetTransform().GetB());
			vTargetForward.Normalize();
			float fDot = vToTarget.Dot(vTargetForward);

			// In front so slow down
			if (fDot > 0.0f) 
			{
				SetState(State_SlowingDown);
				return;
			}
			// Behind so speed up
			else
			{
				SetState(State_CatchingUp);
				return;
			}		
		}
	}
}

void CTaskMoveFollowEntityOffset::CalculateWorldPosition()
{
	if (m_pEntityTarget)
	{
		if (m_bFollowingBehind)
		{
			m_vWorldSpacePosition = Vector3(0.0f, ms_fDefaultYWalkBehindOffset, 0.0f);
		}
		else
		{
			m_vWorldSpacePosition = m_vOffset;

			if (GetState() == State_Matching)
			{
				m_vWorldSpacePosition += Vector3(0.0f, ms_fYOffsetDeltaWhilstMatching, 0.0f);
			}
		}

		// Get offset relative to entity rotation
		if (m_bRelativeOffset)
		{
			m_vWorldSpacePosition = VEC3V_TO_VECTOR3(m_pEntityTarget->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(m_vWorldSpacePosition)));
		}

		// Compute final world position
		m_vWorldSpacePosition += VEC3V_TO_VECTOR3(m_pEntityTarget->GetTransform().GetPosition());
	}
}

void CTaskMoveFollowEntityOffset::UpdateProbes(CPed* pPed)
{
	// Try to obtain probe results if it's active
	if (m_asyncProbeHelper.IsProbeActive())
	{
		ProbeStatus probeStatus;
		if (m_asyncProbeHelper.GetProbeResults(probeStatus))
		{
			if (probeStatus == PS_Blocked)
			{
				m_aCurrentProbeResults[m_eCurrentProbeStatus] = false;
			}
			else if (probeStatus == PS_Clear)
			{
				m_aCurrentProbeResults[m_eCurrentProbeStatus] = true;
			}

			if (m_eCurrentProbeStatus == Probe_High)
			{
				m_eCurrentProbeStatus = Probe_Low;
				m_probeTimer.Reset();
			}
			else
			{
				m_eCurrentProbeStatus = static_cast<eCurrentProbeStatus>(m_eCurrentProbeStatus + 1);
			}
		}
	}

	// If probe isn't active and the timer is up, issue a probe
	if (m_probeTimer.Tick())
	{
		if (!m_asyncProbeHelper.IsProbeActive())
		{
			IssueProbe(pPed);
		}
	}
}

CTask::FSM_Return CTaskMoveFollowEntityOffset::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin

		// Initialise variables and set the goto task
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate(pPed);		

		// Behind the matching zone
		FSM_State(State_CatchingUp)
			FSM_OnUpdate
				return CatchingUp_OnUpdate(pPed);

		// In the matching zone
		FSM_State(State_Matching)
			FSM_OnEnter
				m_matchingTimer.Reset();
			FSM_OnUpdate
				return Matching_OnUpdate(pPed);

		// In front of the matching zone
		FSM_State(State_SlowingDown)
			FSM_OnUpdate
				return SlowingDown_OnUpdate(pPed);

		// Dropped behind due to being stuck, find a path via nav mesh
		FSM_State(State_FollowNavMesh)
			FSM_OnEnter
				FollowNavMesh_OnEnter(pPed);
			FSM_OnUpdate
				return FollowNavMesh_OnUpdate(pPed);

		// Idling at the offset
		FSM_State(State_IdleAtOffset)
			FSM_OnEnter
				IdleAtOffset_OnEnter(pPed);
			FSM_OnUpdate
				return IdleAtOffset_OnUpdate(pPed);

		// Further than the seek distance, start seeking
		FSM_State(State_Seeking)
			FSM_OnEnter
				Seeking_OnEnter(pPed);
			FSM_OnUpdate
				return Seeking_OnUpdate(pPed);	

		FSM_State(State_Blocked)
			FSM_OnEnter
				Blocked_OnEnter(pPed);
			FSM_OnUpdate
				return Blocked_OnUpdate(pPed);	

		// Finished, quit
		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskMoveFollowEntityOffset::Start_OnUpdate(CPed* pPed)
{
	if (m_iTimeToFollow != -1)
	{	
		m_followTimer.Set(fwTimer::GetTimeInMilliseconds(),m_iTimeToFollow);
	}

	const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	TUNE_GROUP_FLOAT(MOVE_FOLLOW_OFFSET, dist, 5.0f, 0.0f, 30.0f, 0.1f);
	if (vPedPosition.Dist2(m_vWorldSpacePosition) > dist*dist)
	{
		SetState(State_FollowNavMesh);
		return FSM_Continue;
	}
	else
	{
		CTaskMoveGoToPointAndStandStill* pTask = rage_new CTaskMoveGoToPointAndStandStill(m_fMoveBlendRatio, m_vWorldSpacePosition, m_fTargetRadius);
		pTask->SetStopExactly(false);

		SetNewTask(pTask);
		SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
	}

	m_probeTimer.Reset();

	if (!m_asyncProbeHelper.IsProbeActive())
	{
		IssueProbe(pPed);
	}

	m_vPreviousPosition = vPedPosition;

	CheckForStateChange(pPed);

	return FSM_Continue;
}

CTask::FSM_Return CTaskMoveFollowEntityOffset::Matching_OnUpdate(CPed* pPed)
{		
	if (GetIsSubtaskFinished(CTaskTypes::TASK_MOVE_GO_TO_POINT_AND_STAND_STILL))
	{
		SetState(State_IdleAtOffset);
		return FSM_Continue;
	}

	SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);

	// Get the speed of the entity we're following
	Vector3 vTargetSpeed = static_cast<const CPhysical*>(m_pEntityTarget.Get())->GetVelocity();
	float fMoveBlend = CTaskMoveBase::ComputeMoveBlend(*pPed, vTargetSpeed.XYMag());

	if (fMoveBlend == MOVEBLENDRATIO_STILL)
	{
		SetState(State_IdleAtOffset);
		return FSM_Continue;
	}

	// Continually set the MBR of the goto task
	fMoveBlend = Min(fMoveBlend, MOVEBLENDRATIO_SPRINT);
	
	if(m_bClampMoveBlendRatio)
	{
		fMoveBlend = Min(fMoveBlend, GetMoveBlendRatio());
	}
	
	if (GetSubTask() && GetSubTask()->IsMoveTask())
	{
		static_cast<CTaskMove*>(GetSubTask())->SetMoveBlendRatio(fMoveBlend);
	}

	// Don't allow us to change state to catching up/slowing down unless we've been in the matching state for at least a second
	if (m_matchingTimer.Tick())
	{
		CheckForStateChange(pPed);
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskMoveFollowEntityOffset::SlowingDown_OnUpdate(CPed* pPed)
{		
	if (GetIsSubtaskFinished(CTaskTypes::TASK_MOVE_GO_TO_POINT_AND_STAND_STILL))
	{
		SetState(State_IdleAtOffset);
		return FSM_Continue;
	}
	
	SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);

	float fMoveBlend = 0.0f;

	// Get the speed of the entity we're following
	if (!m_bRequestStop)
	{
		Vector3 vTargetSpeed = static_cast<const CPhysical*>(m_pEntityTarget.Get())->GetVelocity();
		fMoveBlend = CTaskMoveBase::ComputeMoveBlend(*pPed, vTargetSpeed.XYMag());
		fMoveBlend += ms_fSlowDownDelta;
	}
	else
	{
		Vector3 vTargetSpeed = pPed->GetVelocity();
		fMoveBlend = CTaskMoveBase::ComputeMoveBlend(*pPed, vTargetSpeed.XYMag());
		fMoveBlend += ms_fSlowDownDelta;
	}

	// Continually set the MBR of the goto task
	fMoveBlend = Min(Clamp(fMoveBlend,0.0f,3.0f), MOVEBLENDRATIO_SPRINT);

	if (m_bRequestStop && fMoveBlend == 0.0f)
	{
		m_bRequestStop = false;
	}
	
	if(m_bClampMoveBlendRatio)
	{
		fMoveBlend = Min(fMoveBlend, GetMoveBlendRatio());
	}

	if (GetSubTask() && GetSubTask()->IsMoveTask())
	{
		static_cast<CTaskMove*>(GetSubTask())->SetMoveBlendRatio(fMoveBlend);
	}

	CheckForStateChange(pPed);

	return FSM_Continue;
}

CTask::FSM_Return CTaskMoveFollowEntityOffset::CatchingUp_OnUpdate(CPed* pPed)
{		
	if (GetIsSubtaskFinished(CTaskTypes::TASK_MOVE_GO_TO_POINT_AND_STAND_STILL))
	{
		SetState(State_IdleAtOffset);
		return FSM_Continue;
	}

	SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);

	// Get the speed of the entity we're following
	Vector3 vTargetSpeed = static_cast<const CPhysical*>(m_pEntityTarget.Get())->GetVelocity();
	float fMoveBlend = CTaskMoveBase::ComputeMoveBlend(*pPed, vTargetSpeed.XYMag());
	
	// Continually set the MBR of the goto task
	if (fMoveBlend < MOVEBLENDRATIO_WALK)	// If the leader has slowed, let us continue catching up
	{
		fMoveBlend = m_fMoveBlendRatio;
	}
	else 
	{
		fMoveBlend += ms_fSpeedUpDelta;
		fMoveBlend = Min(Clamp(fMoveBlend,0.0f,3.0f), MOVEBLENDRATIO_SPRINT);
	}
	
	if(m_bClampMoveBlendRatio)
	{
		fMoveBlend = Min(fMoveBlend, GetMoveBlendRatio());
	}

	if (GetSubTask() && GetSubTask()->IsMoveTask())
	{
		static_cast<CTaskMove*>(GetSubTask())->SetMoveBlendRatio(fMoveBlend);
	}

	CheckForStateChange(pPed);

	return FSM_Continue;
}

void CTaskMoveFollowEntityOffset::FollowNavMesh_OnEnter(CPed* UNUSED_PARAM(pPed))
{
	CTaskMoveFollowNavMesh* pTask = rage_new CTaskMoveFollowNavMesh(m_fMoveBlendRatio, m_vWorldSpacePosition, 1.0f);
	pTask->SetStopExactly(false);
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskMoveFollowEntityOffset::FollowNavMesh_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	if (GetIsSubtaskFinished(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH))
	{
		m_waitBeforeRestartingSeekTimer.Reset(); 
		SetState(State_Start);	
	}
	return FSM_Continue;
}

void CTaskMoveFollowEntityOffset::Seeking_OnEnter(CPed* UNUSED_PARAM(pPed))
{
	TTaskMoveSeekEntityStandard* pTask = rage_new TTaskMoveSeekEntityStandard(m_pEntityTarget, 30000, 1000, 15.0f);
	pTask->SetSlowDownDistance(0.0f);
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskMoveFollowEntityOffset::Seeking_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	if (GetIsSubtaskFinished(CTaskTypes::TASK_MOVE_SEEK_ENTITY_STANDARD))
	{
		m_waitBeforeRestartingSeekTimer.Reset(); 
		SetState(State_Start);	
	}
	return FSM_Continue;
}

void CTaskMoveFollowEntityOffset::IdleAtOffset_OnEnter(CPed* UNUSED_PARAM(pPed))
{		
	m_timer.Set(fwTimer::GetTimeInMilliseconds(), ms_iScanTime);
	SetNewTask(rage_new CTaskMoveStandStill(1000));
}

CTask::FSM_Return CTaskMoveFollowEntityOffset::IdleAtOffset_OnUpdate(CPed* pPed)
{
	if(m_timer.IsSet() && m_timer.IsOutOfTime())
	{
		if (m_vWorldSpacePosition.Dist(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition())) > ms_fStartMovingDistance)
		{
			SetState(State_Start);
		}
	}

	return FSM_Continue;
}

void CTaskMoveFollowEntityOffset::Blocked_OnEnter(CPed* UNUSED_PARAM(pPed))
{		
	SetNewTask(rage_new CTaskMoveGoToPointAndStandStill(m_fMoveBlendRatio, m_vWorldSpacePosition, m_fTargetRadius));
}

CTask::FSM_Return CTaskMoveFollowEntityOffset::Blocked_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	if (GetIsSubtaskFinished(CTaskTypes::TASK_MOVE_GO_TO_POINT_AND_STAND_STILL))
	{
		SetState(State_Matching);	
	}
	return FSM_Continue;
}

#if !__FINAL
void CTaskMoveFollowEntityOffset::Debug( ) const
{
#if __BANK
#if DEBUG_DRAW
	if(GetSubTask())
	{
		GetSubTask()->Debug();
	}
	if (CMoveFollowEntityOffsetDebug::ms_bRenderDebug)
	{
		const Vector3 vPedPosition = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition());
		if (m_pEntityTarget)
		{
			// Target position
			grcDebugDraw::Sphere(m_vWorldSpacePosition, 0.05f, Color_red);

			static dev_float s_fLineWidth = 1.0f;

			// Speed Up Zone
			Vector3 vZoneLineStart = Vector3(s_fLineWidth,-ms_fSlowdownEps,0.0f);
			vZoneLineStart.RotateZ(m_pEntityTarget->GetTransform().GetHeading());
			Vector3 vZoneLineEnd = Vector3(0.0f,-ms_fSlowdownEps,0.0f);
			vZoneLineEnd.RotateZ(m_pEntityTarget->GetTransform().GetHeading());
			Vector3 vTargetPos = VEC3V_TO_VECTOR3(m_pEntityTarget->GetTransform().GetPosition());
			grcDebugDraw::Line(vTargetPos+vZoneLineStart, vTargetPos+vZoneLineEnd, Color_red, Color_red);

			// Matching zone
			vZoneLineStart = Vector3(s_fLineWidth,-ms_fMatchingEps,0.0f);
			vZoneLineStart.RotateZ(m_pEntityTarget->GetTransform().GetHeading());
			vZoneLineEnd = Vector3(0.0f,-ms_fMatchingEps,0.0f);
			vZoneLineEnd.RotateZ(m_pEntityTarget->GetTransform().GetHeading());
			grcDebugDraw::Line(vTargetPos+vZoneLineStart, vTargetPos+vZoneLineEnd, Color_orange1, Color_orange1);

			vZoneLineStart = Vector3(s_fLineWidth,ms_fMatchingEps,0.0f);
			vZoneLineStart.RotateZ(m_pEntityTarget->GetTransform().GetHeading());
			vZoneLineEnd = Vector3(0.0f,ms_fMatchingEps,0.0f);
			vZoneLineEnd.RotateZ(m_pEntityTarget->GetTransform().GetHeading());
			grcDebugDraw::Line(vTargetPos+vZoneLineStart, vTargetPos+vZoneLineEnd, Color_orange1, Color_orange1);

			// Slow down zone
			vZoneLineStart = Vector3(s_fLineWidth,ms_fSpeedUpEps,0.0f);
			vZoneLineStart.RotateZ(m_pEntityTarget->GetTransform().GetHeading());
			vZoneLineEnd = Vector3(0.0f,ms_fSpeedUpEps,0.0f);
			vZoneLineEnd.RotateZ(m_pEntityTarget->GetTransform().GetHeading());
			grcDebugDraw::Line(vTargetPos+vZoneLineStart, vTargetPos+vZoneLineEnd, Color_green, Color_green);

			// Draw the medium probe
			Vector3 vForwardProbe(0.0f,ms_fProbeDistance,0.0f);
			vForwardProbe.RotateZ(GetPed()->GetTransform().GetHeading());
			RenderProbeLine(vPedPosition,vPedPosition+vForwardProbe, m_aCurrentProbeResults[Probe_Medium]);

			Vector3 vPedPos = vPedPosition;
			// Low probe
			vForwardProbe.z += ms_fLowProbeOffset; 
			vPedPos.z += ms_fLowProbeOffset;
			RenderProbeLine(vPedPos,vPedPos+vForwardProbe, m_aCurrentProbeResults[Probe_Low]);

			// High probe
			vForwardProbe.z -= ms_fLowProbeOffset;	// Remove the previous offsets
			vPedPos.z -= ms_fLowProbeOffset;
			vForwardProbe.z += ms_fHighProbeOffset; 
			vPedPos.z += ms_fHighProbeOffset;
			RenderProbeLine(vPedPos,vPedPos+vForwardProbe, m_aCurrentProbeResults[Probe_High]);
		}


		char szBuffer[128];
		sprintf(szBuffer,"Speed:%.2f", GetPed()->GetVelocity().XYMag());
		grcDebugDraw::Text(vPedPosition,Color_green,0,10,szBuffer);
		char szBuffer2[128];
		sprintf(szBuffer2,"Probe Reset Time: %f, Current Time: %f",ms_fProbeResetTime,m_probeTimer.GetCurrentTime());
		grcDebugDraw::Text(vPedPosition,Color_green,0,20,szBuffer2);
		char szBuffer3[128];
		sprintf(szBuffer3,"Fall behind reset time: %f, Current Time: %f",ms_fDefaultTimeToUseNavMeshIfCatchingUp,m_catchUpTimer.GetCurrentTime());
		grcDebugDraw::Text(vPedPosition,Color_green,0,30,szBuffer3);
	}
#endif // DEBUG_DRAW
#endif // __BANK
}

const char * CTaskMoveFollowEntityOffset::GetStaticStateName( s32 iState )
{
#define DEBUG_STATE_NAME(X) case X : return #X;

	taskAssert( iState >= State_Start && iState <= State_Finish);

	switch (iState)
	{
		DEBUG_STATE_NAME(State_Invalid)
		DEBUG_STATE_NAME(State_Start)
		DEBUG_STATE_NAME(State_CatchingUp)
		DEBUG_STATE_NAME(State_Matching)
		DEBUG_STATE_NAME(State_SlowingDown)
		DEBUG_STATE_NAME(State_FollowNavMesh)
		DEBUG_STATE_NAME(State_IdleAtOffset)
		DEBUG_STATE_NAME(State_Seeking)
		DEBUG_STATE_NAME(State_Blocked)
		DEBUG_STATE_NAME(State_Finish)
		default: taskAssert(0); 	
	}

	return "State_Invalid";
}
#endif // !__FINAL

#if __BANK

bool CMoveFollowEntityOffsetDebug::ms_bRenderDebug = false;

void CMoveFollowEntityOffsetDebug::InitWidgets()
{
	bkBank* pBank = BANKMGR.FindBank("A.I.");
	if (pBank)
	{
		pBank->PushGroup("Move follow entity");

		pBank->AddSlider("Min distance before slow down", &CTaskMoveFollowEntityOffset::ms_fSlowdownEps, 0.0f, 1.0f, 0.05f);
		pBank->AddSlider("Min distance before match", &CTaskMoveFollowEntityOffset::ms_fMatchingEps, 0.0f, 0.5f, 0.05f);
		pBank->AddSlider("Min distance before speed up", &CTaskMoveFollowEntityOffset::ms_fSpeedUpEps, 0.0f, 1.0f, 0.05f);
		pBank->AddSlider("Increase speed to catch up", &CTaskMoveFollowEntityOffset::ms_fSpeedUpDelta, 0.0f, 3.0f, 0.1f);
		pBank->AddSlider("Increase speed to slow down", &CTaskMoveFollowEntityOffset::ms_fSlowDownDelta, -3.0f, 0.0f, 0.1f);
		pBank->AddSlider("Reset target distance", &CTaskMoveFollowEntityOffset::ms_fResetTargetDistance, 0.0f, 3.0f, 0.01f);
		pBank->AddSlider("Seek target distance", &CTaskMoveFollowEntityOffset::ms_fSeekDistance, 0.0f, 50.0f, 0.5f);
		pBank->AddSlider("Wait time before restarting seek task", &CTaskMoveFollowEntityOffset::ms_fwaitBeforeRestartingSeek, 0.0f, 20.0f, 1.0f);
		pBank->AddSlider("Low Probe Offset", &CTaskMoveFollowEntityOffset::ms_fLowProbeOffset, -1.0f, 0.0f, 0.01f);
		pBank->AddSlider("High Probe Offset", &CTaskMoveFollowEntityOffset::ms_fHighProbeOffset, 0.0f, 1.0f, 0.01f);
		pBank->AddSlider("Wall Probe distance", &CTaskMoveFollowEntityOffset::ms_fProbeDistance, 0.0f, 20.0f, 0.5f);
		pBank->AddSlider("Default walk behind offset", &CTaskMoveFollowEntityOffset::ms_fDefaultYWalkBehindOffset, -10.0f, 10.0f, 0.1f);
		pBank->AddSlider("Probe reset time", &CTaskMoveFollowEntityOffset::ms_fProbeResetTime, 0.0f, 10.0f, 0.5f);
		pBank->AddSlider("Check for stuck time", &CTaskMoveFollowEntityOffset::ms_fDefaultCheckForStuckTime, 0.0f, 20.0f, 0.5f);
		pBank->AddSlider("Stuck tolerance", &CTaskMoveFollowEntityOffset::ms_fStuckDistanceTolerance, 0.0f, 5.0f, 0.1f);
		pBank->AddSlider("Tolerance whilst matching", &CTaskMoveFollowEntityOffset::ms_fToleranceWhilstMatching, 0.0f, 5.0f, 0.1f);
		pBank->AddToggle("Toggle use wall detection", &CTaskMoveFollowEntityOffset::ms_bUseWallDetection);
		pBank->AddSlider("Start moving distance",&CTaskMoveFollowEntityOffset::ms_fStartMovingDistance, 0.0f, 5.0f, 0.1f);
		pBank->AddSlider("Y Offset delta whilst matching", &CTaskMoveFollowEntityOffset::ms_fYOffsetDeltaWhilstMatching, 0.0f, 1.0f, 0.05f);
		pBank->AddToggle("Toggle render debug", &ms_bRenderDebug);
		pBank->PopGroup();
	}
}
#endif // __BANK