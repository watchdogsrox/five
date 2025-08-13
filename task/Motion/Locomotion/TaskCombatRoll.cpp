// File header
#include "Task/Motion/Locomotion/TaskCombatRoll.h"

// Game headers
#include "Peds/PedIntelligence.h"
#include "Task/Default/TaskPlayer.h"

// Statics
dev_float CTaskCombatRoll::ms_FwdMinAngle = PI * -0.75f;
dev_float CTaskCombatRoll::ms_FwdMaxAngle = QUARTER_PI;
// Move Signals
fwMvClipSetId CTaskCombatRoll::ms_ClipSetId("move_strafe@roll",0x80228F7A);
fwMvClipSetId CTaskCombatRoll::ms_FpsClipSetId("move_strafe@roll_fps",0xFE257E11);
fwMvRequestId CTaskCombatRoll::ms_RequestLaunchId("Launch",0x3C50EB34);
fwMvRequestId CTaskCombatRoll::ms_RequestLandId("Land",0xE355D038);
fwMvBooleanId CTaskCombatRoll::ms_LaunchOnEnterId("Launch_OnEnter",0x2A988106);
fwMvBooleanId CTaskCombatRoll::ms_LandOnEnterId("Land_OnEnter",0xA604BC66);
fwMvBooleanId CTaskCombatRoll::ms_ExitLaunchId("ExitLaunch",0xC5DDBCB4);
fwMvBooleanId CTaskCombatRoll::ms_ExitLandId("ExitLand",0x57C11B06);
fwMvBooleanId CTaskCombatRoll::ms_RestartUppderBodyId("RestartUpperBody",0xA1759E42);
fwMvFloatId CTaskCombatRoll::ms_DirectionId("Direction",0x34DF9828);

AI_OPTIMISATIONS()
AI_MOTION_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CTaskCombatRoll
//////////////////////////////////////////////////////////////////////////

CTaskCombatRoll::CTaskCombatRoll()
: m_fDirection(FLT_MAX)
, m_bForwardRoll(false)
, m_bMoveExitLaunch(false)
, m_bMoveExitLand(false)
, m_bMoveRestartUpperBody(false)
, m_bRemoteActive(false)
, m_bRemoteForwardRoll(true)
, m_fRemoteDirection(0.f)
{
	SetInternalTaskType(CTaskTypes::TASK_COMBAT_ROLL);
}

void CTaskCombatRoll::GetMoveSpeeds(CMoveBlendRatioSpeeds& speeds)
{
	taskAssert(ms_ClipSetId != CLIP_SET_ID_INVALID);

	speeds.Zero();

	bool bUseFirstPerson = false;
#if FPS_MODE_SUPPORTED
	CPed* pPed = GetPed();
	bUseFirstPerson = (pPed) ? pPed->IsFirstPersonShooterModeEnabledForPlayer(false) : false;
#endif
	const fwMvClipSetId clipSetId = (!bUseFirstPerson) ? ms_ClipSetId : ms_FpsClipSetId;
	fwClipSet* pClipSet = fwClipSetManager::GetClipSet(clipSetId);
	taskAssert(pClipSet);

	static fwMvClipId s_walkClip("COMBATROLL_FWD_P1_00",0xE0BCFC81);
	static fwMvClipId s_runClip("COMBATROLL_FWD_P1_00",0xE0BCFC81);
	static fwMvClipId s_sprintClip("COMBATROLL_FWD_P1_00",0xE0BCFC81);

	fwMvClipId clipNames[3] = { s_walkClip, s_runClip, s_sprintClip };

	RetrieveMoveSpeeds(*pClipSet, speeds, clipNames);
}

void CTaskCombatRoll::GetNMBlendOutClip(fwMvClipSetId& outClipSet, fwMvClipId& outClip)
{
	static fwMvClipId s_outClip("COMBATROLL_FWD_P1_00",0xE0BCFC81);
	outClipSet = ms_ClipSetId;
	outClip    = s_outClip;
}

CTask* CTaskCombatRoll::CreatePlayerControlTask()
{
	return rage_new CTaskPlayerOnFoot();
}

bool CTaskCombatRoll::ShouldTurnOffUpperBodyAnimations() const
{
	if(!m_MoveNetworkHelper.IsNetworkActive())
	{
		return false;
	}

	switch(GetState())
	{
	case State_Initial:
	case State_RollLaunch:
		return true;
	case State_RollLand:
		if(m_bMoveRestartUpperBody || m_MoveNetworkHelper.GetBoolean(ms_RestartUppderBodyId))
		{
			return false;
		}
		return true;
	case State_Quit:
		return false;
	default:
		taskAssertf(0, "Unhandled case [%d]", GetState());
		return true;
	}
}

CTaskCombatRoll::FSM_Return CTaskCombatRoll::ProcessPreFSM()
{
	CPed* pPed = GetPed();
	pPed->SetPedResetFlag(CPED_RESET_FLAG_TemporarilyBlockWeaponSwitching, true);

	// Don't switch camera during combat roll.
	pPed->SetPedResetFlag(CPED_RESET_FLAG_BlockCameraSwitching, true );

	return FSM_Continue;
}

CTaskCombatRoll::FSM_Return CTaskCombatRoll::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		FSM_State(State_Initial)
			FSM_OnUpdate
				return StateInitial_OnUpdate();
		FSM_State(State_RollLaunch)
			FSM_OnEnter
				return StateRollLaunch_OnEnter();
			FSM_OnUpdate
				return StateRollLaunch_OnUpdate();
		FSM_State(State_RollLand)
			FSM_OnEnter
				return StateRollLand_OnEnter();
			FSM_OnUpdate
				return StateRollLand_OnUpdate();
		FSM_State(State_Quit)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

CTaskCombatRoll::FSM_Return CTaskCombatRoll::ProcessPostFSM()
{
	CPed* pPed = GetPed();
	if(m_MoveNetworkHelper.IsNetworkActive())
	{
		bool bDirectionSet = false;

		if (NetworkInterface::IsGameInProgress() && pPed && pPed->IsNetworkClone())
		{
			if (m_bRemoteActive)
			{
				m_bForwardRoll	= m_bRemoteForwardRoll;
				m_fDirection	= m_fRemoteDirection;
				bDirectionSet	= true;
			}
		}

		if (!bDirectionSet)
		{
			float fDirection = m_fDirection;

			if(pPed->GetMotionData()->GetIsStrafing())
			{
				Vector3 vMBR(pPed->GetMotionData()->GetGaitReducedDesiredMbrX(), pPed->GetMotionData()->GetGaitReducedDesiredMbrY(), 0.f);

				if(pPed->GetMotionData()->GetIndependentMoverTransition() != 0)
				{
					// While the independent mover is active, reinterpret the desired MBR
					float fHeadingDelta = -pPed->GetMotionData()->GetIndependentMoverHeadingDelta();

					Quaternion q;
					q.FromEulers(Vector3(0.f, 0.f, fHeadingDelta));
					q.Transform(vMBR);
				}

				if(vMBR.Mag2() > 0.01f)
				{
					vMBR.Normalize();
					fDirection = rage::Atan2f(-vMBR.x, vMBR.y);
				}
			}

			// Clamp
			if(m_bForwardRoll)
			{
				fDirection = Clamp(fDirection, ms_FwdMinAngle, ms_FwdMaxAngle);
			}
			else
			{
				if(fDirection >= ms_FwdMinAngle && fDirection <= ms_FwdMaxAngle)
				{
					float fToMin = fDirection - ms_FwdMinAngle;
					float fToMax = ms_FwdMaxAngle - fDirection;
					fDirection = fToMin < fToMax ? ms_FwdMinAngle : ms_FwdMaxAngle;
				}
			}

			/// static dev_float RATE = PI;
			m_fDirection = fwAngle::Lerp(m_fDirection, fDirection, PI * GetTimeStep());
		}

		m_MoveNetworkHelper.SetFloat(ms_DirectionId, CTaskMotionBase::ConvertRadianToSignal(m_fDirection));
	}
	
	pPed->SetPedResetFlag(CPED_RESET_FLAG_DoingCombatRoll, true);

	// Break lock on for players that are locking on to this ped.
	if(GetState() == State_RollLaunch)
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_BreakTargetLock, true);
	}

	return FSM_Continue;
}

void CTaskCombatRoll::CleanUp()
{
	CPed* pPed = GetPed();
	pPed->GetMovePed().ClearTaskNetwork(m_MoveNetworkHelper, SLOW_BLEND_DURATION);

	// Keep track of when we last did a roll
	pPed->GetPedIntelligence()->SetDoneCombatRoll();

	pPed->SetPedResetFlag(CPED_RESET_FLAG_CheckFPSSwitchInCameraUpdate, true);
}

bool CTaskCombatRoll::ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
	bool bMustAbort = false;
	if(pEvent)
	{
		CEvent* pNewEvent = (CEvent*)pEvent;
		if(pNewEvent->RequiresAbortForRagdoll() || pNewEvent->GetEventPriority() == E_PRIORITY_SCRIPT_COMMAND_SP)
		{
			bMustAbort = true;
		}
	}

	if(iPriority == CTask::ABORT_PRIORITY_URGENT && !bMustAbort)
	{
		return false;
	}

	// Base class
	return CTask::ShouldAbort(iPriority, pEvent);
}

#if !__FINAL
const char* CTaskCombatRoll::GetStaticStateName(s32 iState)
{
	switch(iState)
	{
	case State_Initial:		return "Initial";
	case State_RollLaunch:	return "RollLaunch";
	case State_RollLand:	return "RollLand";
	case State_Quit:		return "Quit";
	default:				taskAssertf(0, "Unhandled state %d", iState); return "Unknown";
	};
}
#endif // !__FINAL

bool CTaskCombatRoll::ProcessMoveSignals()
{
	m_bMoveExitLaunch		= m_MoveNetworkHelper.GetBoolean(ms_ExitLaunchId);
	m_bMoveExitLand			= m_MoveNetworkHelper.GetBoolean(ms_ExitLandId);
	m_bMoveRestartUpperBody	= m_MoveNetworkHelper.GetBoolean(ms_RestartUppderBodyId);
	return true;
}

CTaskCombatRoll::FSM_Return CTaskCombatRoll::StateInitial_OnUpdate()
{
	// Cache the ped
	CPed* pPed = GetPed();

	bool bDirectionSet = false;

	if (NetworkInterface::IsGameInProgress() && pPed && pPed->IsNetworkClone())
	{
		if (m_bRemoteActive)
		{
			m_bForwardRoll	= m_bRemoteForwardRoll;
			m_fDirection	= m_fRemoteDirection;
			bDirectionSet	= true;
		}
	}

	if(!m_MoveNetworkHelper.IsNetworkActive())
	{
		m_MoveNetworkHelper.RequestNetworkDef(CClipNetworkMoveInfo::ms_NetworkTaskCombatRoll);

		// Check if clip sets are streamed in
		bool bUseFirstPerson = false;
	#if FPS_MODE_SUPPORTED
		bUseFirstPerson = pPed->IsFirstPersonShooterModeEnabledForPlayer(false);
	#endif
		const fwMvClipSetId clipSetId = (!bUseFirstPerson) ? ms_ClipSetId : ms_FpsClipSetId;
		fwClipSet* pClipSet = fwClipSetManager::GetClipSet(clipSetId);
		taskAssertf(pClipSet, "Clip set [%s] does not exist", clipSetId.TryGetCStr());
		if(pClipSet->Request_DEPRECATED() && m_MoveNetworkHelper.IsNetworkDefStreamedIn(CClipNetworkMoveInfo::ms_NetworkTaskCombatRoll))
		{
			m_MoveNetworkHelper.CreateNetworkPlayer(pPed, CClipNetworkMoveInfo::ms_NetworkTaskCombatRoll);
			m_MoveNetworkHelper.SetClipSet(clipSetId);
		}
	}

	if(!bDirectionSet && m_fDirection == FLT_MAX)
	{
		Vector3 vMBR(pPed->GetMotionData()->GetGaitReducedDesiredMbrX(), pPed->GetMotionData()->GetGaitReducedDesiredMbrY(), 0.f);

		if(pPed->GetMotionData()->GetIndependentMoverTransition() != 0)
		{
			// While the independent mover is active, reinterpret the desired MBR
			float fHeadingDelta = -pPed->GetMotionData()->GetIndependentMoverHeadingDelta();

			Quaternion q;
			q.FromEulers(Vector3(0.f, 0.f, fHeadingDelta));
			q.Transform(vMBR);
		}

		if(vMBR.Mag2() > 0.01f)
		{
			vMBR.Normalize();
			m_fDirection = rage::Atan2f(-vMBR.x, vMBR.y);
		}
		else
		{
			m_fDirection = 0.f;
		}

		if(m_fDirection >= ms_FwdMinAngle && m_fDirection <= ms_FwdMaxAngle)
		{
			m_bForwardRoll = true;
		}
	}

	if(m_MoveNetworkHelper.IsNetworkActive())
	{
		if(!m_MoveNetworkHelper.IsNetworkAttached())
		{
			m_MoveNetworkHelper.DeferredInsert();
		}

		SetState(State_RollLaunch);
		return FSM_Continue;
	}

	return FSM_Continue;
}

CTaskCombatRoll::FSM_Return CTaskCombatRoll::StateRollLaunch_OnEnter()
{
	m_MoveNetworkHelper.SendRequest(ms_RequestLaunchId);
	m_MoveNetworkHelper.WaitForTargetState(ms_LaunchOnEnterId);
	RequestProcessMoveSignalCalls();
	return FSM_Continue;
}

CTaskCombatRoll::FSM_Return CTaskCombatRoll::StateRollLaunch_OnUpdate()
{
	if(!m_MoveNetworkHelper.IsInTargetState())
	{
		return FSM_Continue;
	}

	if(m_bMoveExitLaunch)
	{
		SetState(State_RollLand);
		return FSM_Continue;
	}

	return FSM_Continue;
}

CTaskCombatRoll::FSM_Return CTaskCombatRoll::StateRollLand_OnEnter()
{
	m_MoveNetworkHelper.SendRequest(ms_RequestLandId);
	m_MoveNetworkHelper.WaitForTargetState(ms_LandOnEnterId);
	return FSM_Continue;
}

CTaskCombatRoll::FSM_Return CTaskCombatRoll::StateRollLand_OnUpdate()
{
	if(!m_MoveNetworkHelper.IsInTargetState())
	{
		return FSM_Continue;
	}

	if(m_bMoveExitLand)
	{
		return FSM_Quit;
	}

	return FSM_Continue;
}
