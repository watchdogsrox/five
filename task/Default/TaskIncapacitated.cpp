// Header
#include "task/Default/TaskIncapacitated.h"

// Game
#include "Peds/ped.h"
#include "Task/Physics/TaskNMRelax.h"

#include "Event/EventResponse.h"
#include "Peds/PedIntelligence.h"
#include "Task/Physics/TaskBlendFromNM.h"
#include "Task/movement/TaskGetUp.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/Motion/TaskMotionBase.h"
#include "vehicles/Metadata/VehicleSeatInfo.h"

#if CNC_MODE_ENABLED

const fwMvBooleanId				CTaskIncapacitated::ms_EventOnEnterEscape("OnEnterEscape", 0xD85F4982);
const fwMvBooleanId				CTaskIncapacitated::ms_EventEscapeFinished("TransitionToEscapeFinished", 0xBA4BC861);
const fwMvRequestId				CTaskIncapacitated::ms_RequestEscape("RequestEscape", 0xED934A3);
const fwMvClipSetId				CTaskIncapacitated::ms_IncapacitatedMaleDictVer1("clipset@anim_cnc@gameplay@arrest@stunned@ver_01@from_kneel_to_stand", 0xBCF1A3F8);
const fwMvClipSetId				CTaskIncapacitated::ms_IncapacitatedMaleDictVer2("clipset@anim_cnc@gameplay@arrest@stunned@ver_02@from_kneel_to_stand", 0x52D8AEEB);
const fwMvFloatId				CTaskIncapacitated::ms_GetupHeading("GetupHeading",0x4D798C29);
const fwMvFlagId				CTaskIncapacitated::ms_UseLeftSideGetups("UseLeftSideGetups",0xFE53FEE6);
const fwMvFlagId				CTaskIncapacitated::ms_UseRightSideGetups("UseRightSideGetups",0x4A6D337C);

CTaskIncapacitated::Tunables CTaskIncapacitated::sm_Tunables;
IMPLEMENT_DEFAULT_TASK_TUNABLES(CTaskIncapacitated, 0x7E8E6E1F);

s32 CTaskIncapacitated::ms_TimeSinceRecoveryToIgnoreEnduranceDamage = -1;
s32 CTaskIncapacitated::ms_IncapacitateTime = -1;
s32 CTaskIncapacitated::ms_GetUpFasterModifier = -1;
s32 CTaskIncapacitated::ms_GetUpFasterLimit = -1;

void CTaskIncapacitated::InitTunables()
{
	ms_TimeSinceRecoveryToIgnoreEnduranceDamage = ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("CNC_ENDURANCE_DMG_DURATION_IGNORE", 0xD8A61DBF), -1);
	ms_IncapacitateTime = ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("CNC_INCAPACITATED_RECOVERY_TIME", 0x60F3DB79), -1);
	ms_GetUpFasterModifier = ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("CNC_INCAPACITATED_RECOVERY_BUTTON", 0xCE94653B), -1);
	ms_GetUpFasterLimit = ::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("CNC_INCAPACITATED_RECOVERY_BUTTON_THRESHOLD", 0x50462B35), -1);
}

u32 CTaskIncapacitated::GetTimeSinceRecoveryToIgnoreEnduranceDamage()
{
	return ms_TimeSinceRecoveryToIgnoreEnduranceDamage != -1 ? (u32)ms_TimeSinceRecoveryToIgnoreEnduranceDamage : sm_Tunables.m_TimeSinceRecoveryToIgnoreEnduranceDamage;
}

u32 CTaskIncapacitated::GetIncapacitateTime()
{
	return ms_IncapacitateTime != -1 ? (u32)ms_IncapacitateTime : sm_Tunables.m_IncapacitateTime;
}

u32 CTaskIncapacitated::GetRecoverFasterModifier()
{
	return ms_GetUpFasterModifier != -1 ? (u32)ms_GetUpFasterModifier : sm_Tunables.m_GetUpFasterModifier;
}

u32 CTaskIncapacitated::GetRecoverFasterLimit()
{
	return ms_GetUpFasterLimit != -1 ? (u32)ms_GetUpFasterLimit : sm_Tunables.m_GetUpFasterLimit;
}

CTaskIncapacitated::CTaskIncapacitated()
	: m_bCloneQuit(false)
	, m_bArrested(false)
	, m_bGetupComplete(false)
	, m_pArrester(nullptr)
	, m_iFastGetupOffset(0)
	, m_iGetupTime(0)
	, m_iTimeRagdollLastMoved(0)
	, m_fDesiredEscapeHeading(0.0f)
	, m_fDesiredEscapeHeadingLocal(0.0f)
	, m_bUseClipsetVeriation01(true)
	, m_bTimerStarted(false)
	, m_bInVehicleAnim(false)
	, m_pForcedNmTask(NULL)
{
	SetInternalTaskType(CTaskTypes::TASK_INCAPACITATED);
}

CTaskIncapacitated::~CTaskIncapacitated()
{
	if (m_pForcedNmTask)
	{
		delete m_pForcedNmTask;
	}
}

bool CTaskIncapacitated::AddGetUpSets(CNmBlendOutSetList& sets, CPed* UNUSED_PARAM(pPed))
{
	if(m_bUseClipsetVeriation01)
	{
		sets.Add(NMBS_INCAPACITATED_GETUPS);
	}
	else
	{
		sets.Add(NMBS_INCAPACITATED_GETUPS_02);
	}

	return true;
}

void CTaskIncapacitated::GetUpComplete(eNmBlendOutSet UNUSED_PARAM(setMatched), CNmBlendOutItem* UNUSED_PARAM(pLastItem), CTaskMove* UNUSED_PARAM(pMoveTask), CPed* UNUSED_PARAM(pPed))
{
	if (!m_moveNetworkHelper.GetNetworkPlayer() && m_ClipRequestHelper.Request(ms_IncapacitatedDict) && m_moveNetworkHelper.IsNetworkDefStreamedIn(CClipNetworkMoveInfo::ms_TaskIncapacitatedNetwork))
	{
		m_moveNetworkHelper.CreateNetworkPlayer(GetPed(), CClipNetworkMoveInfo::ms_TaskIncapacitatedNetwork);
		m_moveNetworkHelper.SetClipSet(m_ClipRequestHelper.GetClipSetId());
		GetPed()->GetMovePed().SetTaskNetwork(m_moveNetworkHelper, INSTANT_BLEND_DURATION);
	}

	m_bGetupComplete = true;
}

bool CTaskIncapacitated::ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent)
{
	//  Don't interfere with Immediate priorities
	if (iPriority == AbortPriority::ABORT_PRIORITY_IMMEDIATE) {
		return CTaskFSMClone::ShouldAbort(iPriority, pEvent);
	}

	if (!pEvent) 
	{
		return false;
	}
	// Always abort for death
	if (pEvent->GetEventPriority() >= E_PRIORITY_DEATH)
	{
		return CTask::ShouldAbort(iPriority, pEvent);
	} 

	if ((pEvent->GetEventPriority() == E_PRIORITY_DAMAGE_WITH_RAGDOLL || pEvent->GetEventPriority() == E_PRIORITY_SWITCH_2_NM_SCRIPT))
	{
		const CEvent* pEventDetail = (CEvent*)pEvent;

		CEventResponse response;
		if (pEventDetail->CreateResponseTask(GetPed(), response))
		{
			if (response.GetEventResponse(CEventResponse::PhysicalResponse) && !response.GetRunOnSecondary())
			{
				switch (response.GetEventResponse(CEventResponse::PhysicalResponse)->GetTaskType())
				{
					case CTaskTypes::TASK_NM_CONTROL:
						taskDebugf3("Frame %d : %s CTaskIncapacitated::ShouldAbort - ditching abort for task NM_CONTROL", fwTimer::GetFrameCount(), GetPed()->GetDebugName());
						SetForcedNaturalMotionTask(response.ReleaseEventResponse(CEventResponse::PhysicalResponse));
						break;
					default:
						// These are often more complex physical responses like electrocution, decisions may need to be made on them separately
						// Generally we control environmental conditions enough to not see them but if we do it's safest to break out of incapacitation until we can test
						taskErrorf("Frame %d : %s CTaskIncapacitated::ShouldAbort - encountered unexpected task type %d", fwTimer::GetFrameCount(), GetPed()->GetDebugName(), response.GetEventResponse(CEventResponse::PhysicalResponse)->GetTaskType());
						return true;
						break;
				}
			}
		}
		const_cast<aiEvent*>(pEvent)->SetFlagForDeletion(true);
		return false;
	}

	return false;
}

void CTaskIncapacitated::OnCloneTaskNoLongerRunningOnOwner()
{
	m_bCloneQuit = true;
}

CTaskInfo* CTaskIncapacitated::CreateQueriableState() const
{
	return rage_new CClonedTaskIncapacitatedInfo(GetState(), m_bArrested, m_bUseClipsetVeriation01,  m_fDesiredEscapeHeading);
}

void CTaskIncapacitated::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	Assert(pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_INCAPACITATED);
	CClonedTaskIncapacitatedInfo* pInfo = static_cast<CClonedTaskIncapacitatedInfo*>(pTaskInfo);

	m_bArrested = pInfo->GetShouldArrest();
	m_bUseClipsetVeriation01 = pInfo->GetClipVeriation();
	m_fDesiredEscapeHeading = pInfo->GetEscapeHeading();

	CTaskFSMClone::ReadQueriableState(pTaskInfo);
}


bool CTaskIncapacitated::ControlPassingAllowed(CPed *UNUSED_PARAM(pPed), const netPlayer& UNUSED_PARAM(player), eMigrationType UNUSED_PARAM(migrationType))
{
	return false;
}

CTask::FSM_Return CTaskIncapacitated::UpdateClonedFSM(const s32 iState, const FSM_Event iEvent)
{
	if (iEvent == OnUpdate)
	{
		if (GetStateFromNetwork() != -1 && GetStateFromNetwork() != GetState())
		{
			bool bDoesNetworkStateUseStreamedAnims = DoesStateUseStreamedAnims(GetStateFromNetwork());
			bool bDoesLocalStateUseStreamedAnims = DoesStateUseStreamedAnims(GetState());
			bool bAnimsLoaded = (!bDoesNetworkStateUseStreamedAnims || m_ClipRequestHelper.IsLoaded() || (m_bInVehicleAnim && m_InVehicleClipset.GetHash() == 0) );

			// Clones will do ragdoll logic themselves. Once we hit an animated state, we need to make sure anims are loaded and we ensure clones begin at the incapacitation state.
			if(bDoesNetworkStateUseStreamedAnims && bDoesLocalStateUseStreamedAnims && bAnimsLoaded )
			{
				if(GetStateFromNetwork() == State_Incapacitated && GetState() != State_Incapacitated)
				{
					//If we aren't locally in incapacitation then we need to force it to start the anims before we allow it transitioning further.
					SetState(State_Incapacitated);
				}
				else
				{
					//Otherwise accept the network state
					SetState(GetStateFromNetwork());
				}
				return FSM_Continue;
			}
		}

	}

	return UpdateFSM(iState, iEvent);
}


fwMvClipSetId CTaskIncapacitated::GetCorrectInVehicleAnim() const
{
	const CPed* pPed = GetPed();
	CVehicle* vehPedVehicle = pPed->GetVehiclePedInside();
	const s32 iPedSeat =  vehPedVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
	fwMvClipSetId stunnedClipset = vehPedVehicle->GetSeatAnimationInfo(iPedSeat)->GetStunnedClipSet();
	taskAssertf(stunnedClipset.GetHash() != 0, "CTaskIncapacitated::GetCorrectInVehicleAnim no Stunned anim exists for vehicle (%s - %d) player is incapacitated in", vehPedVehicle->GetDebugName(), iPedSeat);
	return stunnedClipset;
}

int CTaskIncapacitated::CalculateTimeTilRecovery() const 
{
	int iTimeTilRecovery = m_iGetupTime - m_iFastGetupOffset - NetworkInterface::GetNetworkTime(); 
	return iTimeTilRecovery > 0 ? iTimeTilRecovery : 0;
}
bool CTaskIncapacitated::DoesStateUseStreamedAnims(const s32 iState) const
{
	switch (iState)
	{
	case State_Incapacitated:
	case State_Arrested:
	case State_Escape:
		return true;
	default:
		return false;
	}
}

CTask::FSM_Return CTaskIncapacitated::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	if (m_bCloneQuit)
	{
		return FSM_Quit;
	}

	CPed *pPed = GetPed();

	FSM_Begin
		FSM_State(State_Start)
			FSM_OnEnter
				return Start_OnEnter();
			FSM_OnUpdate
				return Start_OnUpdate(pPed);

		FSM_State(State_DownedRagdoll)
			FSM_OnEnter
				return DownedRagdoll_OnEnter(pPed);
			FSM_OnUpdate
				return DownedRagdoll_OnUpdate();
			FSM_OnExit
				return DownedRagdoll_OnExit();
	
		FSM_State(State_Incapacitated)
			FSM_OnEnter
				return Incapacitated_OnEnter();
			FSM_OnUpdate
				return Incapacitated_OnUpdate(pPed);

		FSM_State(State_Escape)
			FSM_OnEnter
				return Escape_OnEnter();
			FSM_OnUpdate
				return Escape_OnUpdate(pPed);

		FSM_State(State_Arrested)
				FSM_OnEnter
				return Arrest_OnEnter();
			FSM_OnUpdate
				return Arrest_OnUpdate();
	
		FSM_State(State_Finish)
			FSM_OnUpdate
			return FSM_Quit;
	FSM_End
}

void CTaskIncapacitated::CleanUp()
{	
	m_ClipRequestHelper.Release();
	CPed* pPed = GetPed();
	
	if (m_moveNetworkHelper.GetNetworkPlayer())
	{
		pPed->GetMovePed().ClearTaskNetwork(m_moveNetworkHelper, SLOW_BLEND_DURATION);
		m_moveNetworkHelper.ReleaseNetworkPlayer();
	}


	if (!NetworkUtils::IsNetworkCloneOrMigrating(pPed))
	{
		pPed->SetEndurance(pPed->GetMaxEndurance());

		if (pPed->GetPlayerInfo())
		{
			CPlayerEnduranceManager& enduranceManager = pPed->GetPlayerInfo()->GetEnduranceManager();
			enduranceManager.NotifyEnduranceRecovered();
		}
	}

	// There is a small window of opportunity at the end of incapacitation to trigger a takedown again
	// After the move network reaches get up (and the player is hittable) but before the task actually ends
	// Task priority ensures this is ignored but don't let the status effects apply either
	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_KilledByStealth, false);
	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_KilledByTakedown, false);
	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_Knockedout, false);

	ShowWeapon(true);
}

#if !__FINAL
const char * CTaskIncapacitated::GetStaticStateName(s32 iState)
{
	taskAssert(iState >= 0 && iState <= State_Finish);
	static const char* aRunClipStateNames[] =
	{
		"State_Start",
		"State_DownedRagdoll",
		"State_Incapacitated",
		"State_Escape",
		"State_Arrested",
		"State_Finish"
	};

	return aRunClipStateNames[iState];
}
#endif // !__FINAL

CTask::FSM_Return CTaskIncapacitated::Start_OnEnter()
{
	//Pick a clip set veriation to use
	if(GetPed()->IsLocalPlayer())
	{
		m_bUseClipsetVeriation01 = fwRandom::GetRandomTrueFalse();
	}
	ms_IncapacitatedDict = m_bUseClipsetVeriation01 ? ms_IncapacitatedMaleDictVer1 : ms_IncapacitatedMaleDictVer2;

	taskDebugf3("Frame %d : %s CTaskIncapacitated::Start_OnEnter - Entering start", fwTimer::GetFrameCount(), GetPed()->GetDebugName());
	return FSM_Continue;
}

CTask::FSM_Return CTaskIncapacitated::Start_OnUpdate(CPed* pPed)
{

	// These flags are normally cleared by ped death - but if ped is incapacitated, they didn't die
	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_KilledByStealth, false);
	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_KilledByTakedown, false);
	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_Knockedout, false);


	if (!m_bTimerStarted)
	{
		m_iGetupTime = NetworkInterface::GetNetworkTime() + GetIncapacitateTime();
		m_bTimerStarted = true;
	}
	// TODO: Check if we can do ragdoll here instead of DownedRagdoll_OnEnter, and if not then go to State_DownedAnimatedFallback instead and play animated fallback task
	CVehicle* vehPedVehicle = pPed->GetVehiclePedInside();
	if (!vehPedVehicle || vehPedVehicle->IsBikeOrQuad() || vehPedVehicle->GetIsJetSki())
	{
		SetState(State_DownedRagdoll);
	}
	else
	{
		if (!m_bInVehicleAnim)
		{
			m_InVehicleClipset = GetCorrectInVehicleAnim();
			m_bInVehicleAnim = true;
		}
		
		if (m_InVehicleClipset.GetHash() != 0)  // Edge case, no associated anim
		{
			if (m_ClipRequestHelper.Request(m_InVehicleClipset) && m_moveNetworkHelper.IsNetworkDefStreamedIn(CClipNetworkMoveInfo::ms_TaskIncapacitatedInVehicleNetwork))
			{
				taskDebugf3("Frame %d : %s CTaskIncapacitated::Start_OnUpdate - Player in Vehicle moving directly to Incapacitation", fwTimer::GetFrameCount(), GetPed()->GetDebugName());
				m_bInVehicleAnim = true;
				SetState(State_Incapacitated);
				return FSM_Continue;
			}
		}
		else
		{
			SetState(State_Incapacitated);
		}
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskIncapacitated::DownedRagdoll_OnEnter(CPed* pPed)
{
	taskDebugf3("Frame %d : %s CTaskIncapacitated::DownedRagdoll_OnEnter - Entering downed ragdoll", fwTimer::GetFrameCount(), pPed->GetDebugName());
	if (m_pForcedNmTask) 
	{
		SetNewTask(m_pForcedNmTask);
		m_pForcedNmTask = NULL;
	}
	else if (pPed->GetUsingRagdoll() || CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_NETWORK))
	{
		taskDebugf3("Frame %d : %s CTaskIncapacitated::DownedRagdoll_OnEnter - Creating new ragdoll", fwTimer::GetFrameCount(), pPed->GetDebugName());
		CTask* pTask = rage_new CTaskNMControl(500, 5000, rage_new CTaskNMRelax(300, 10000), CTaskNMControl::BALANCE_FAILURE);
		SetNewTask(pTask);
	}

	m_bInVehicleAnim = false;
	m_ClipRequestHelper.Request(ms_IncapacitatedDict);
	m_moveNetworkHelper.RequestNetworkDef(CClipNetworkMoveInfo::ms_TaskIncapacitatedNetwork);
	
	if (m_moveNetworkHelper.GetNetworkPlayer())
	{
		pPed->GetMovePed().ClearTaskNetwork(m_moveNetworkHelper, INSTANT_BLEND_DURATION);
		m_moveNetworkHelper.ReleaseNetworkPlayer();
	}

	m_bGetupComplete = false;
	return FSM_Continue;
}


CTask::FSM_Return CTaskIncapacitated::DownedRagdoll_OnUpdate()
{
	CPed* pPed = GetPed();

	HandleUserInput(pPed);

	if (m_pForcedNmTask)
	{
		SetNewTask(m_pForcedNmTask);
		m_pForcedNmTask = NULL;
	}

	if (m_ClipRequestHelper.Request(ms_IncapacitatedDict) && m_moveNetworkHelper.IsNetworkDefStreamedIn(CClipNetworkMoveInfo::ms_TaskIncapacitatedNetwork))
	{
		bool shouldCompleteRagdoll = m_bGetupComplete;
		if (!shouldCompleteRagdoll)
		{
			if (pPed->GetVelocity().Mag2() >= 1.0f)
			{
				m_iTimeRagdollLastMoved = NetworkInterface::GetNetworkTime();
			}

			// If we are still in ragdoll, check if we should recover (when not in the air or moving)
			const u32 iTimeRagdollShouldNotMove = 250;
			shouldCompleteRagdoll = NetworkInterface::GetNetworkTime() + m_iFastGetupOffset > m_iGetupTime
				&& !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsInTheAir)
				&& NetworkInterface::GetNetworkTime() - m_iTimeRagdollLastMoved >= iTimeRagdollShouldNotMove;
		}

		if (shouldCompleteRagdoll)
		{
			taskDebugf3("Frame %d : %s CTaskIncapacitated::DownedRagdoll_OnUpdate - Moving to Incapacitation", fwTimer::GetFrameCount(), GetPed()->GetDebugName());
			SetState(State_Incapacitated);
			return FSM_Continue;
		}
	}

	if (pPed->GetUsingRagdoll())
	{
		if (!GetSubTask() || !GetSubTask()->HandlesRagdoll(pPed))
		{
			// If the ped is ragdolling but there is no task to handle it then restart the state
			taskDebugf3("Frame %d : %s CTaskIncapacitated::DownedRagdoll_OnUpdate - Subtask doesn't handle ragdolling ped - restarting state", fwTimer::GetFrameCount(), GetPed()->GetDebugName());
			SetFlag(aiTaskFlags::RestartCurrentState);
		}
	}
	else
	{
		if ((m_bGetupComplete || !GetSubTask()) && GetTimeInState() > 5.f)
		{
			aiAssertf(0, "Stuck in CTaskIncapacitated::DownedRagdoll - quitting");
			return FSM_Quit;
		}
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskIncapacitated::DownedRagdoll_OnExit()
{
	// Avoid immediately re-entering ragdoll
	if (m_pForcedNmTask)
	{
		delete m_pForcedNmTask;
		m_pForcedNmTask = NULL;
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskIncapacitated::Incapacitated_OnEnter()
{
	taskDebugf3("Frame %d : %s CTaskIncapacitated::Incapacitated_OnEnter - Entering Incapacitated", fwTimer::GetFrameCount(), GetPed()->GetDebugName());
	if (!m_moveNetworkHelper.GetNetworkPlayer())
	{
		if (!m_bInVehicleAnim || m_InVehicleClipset.GetHash() != 0)
		{
			m_moveNetworkHelper.CreateNetworkPlayer(GetPed(), m_bInVehicleAnim ? CClipNetworkMoveInfo::ms_TaskIncapacitatedInVehicleNetwork : CClipNetworkMoveInfo::ms_TaskIncapacitatedNetwork);
			m_moveNetworkHelper.SetClipSet(m_ClipRequestHelper.GetClipSetId());
			GetPed()->GetMovePed().SetTaskNetwork(m_moveNetworkHelper, NORMAL_BLEND_DURATION);
		}
	}

	ShowWeapon(false);

	return FSM_Continue;
}

CTask::FSM_Return CTaskIncapacitated::Incapacitated_OnUpdate(CPed* pPed)
{
	if (m_bArrested && !pPed->IsNetworkClone())
	{
		SetState(State_Arrested);
		return FSM_Continue;
	}

	if (pPed->GetUsingRagdoll() || m_pForcedNmTask)
	{
		taskDebugf3("Frame %d : %s CTaskIncapacitated::Incapacitated_OnUpdate - Ped in ragdoll, moving to downed ragdoll", fwTimer::GetFrameCount(), GetPed()->GetDebugName());
		SetState(State_DownedRagdoll);
		return FSM_Continue;
	}

	if (pPed->IsLocalPlayer())
	{
		HandleUserInput(pPed);
		if (NetworkInterface::GetNetworkTime() + m_iFastGetupOffset > m_iGetupTime)
		{
			taskDebugf3("Frame %d : %s CTaskIncapacitated::Incapacitated_OnUpdate - moving to escaped", fwTimer::GetFrameCount(), GetPed()->GetDebugName());
			SetState(State_Escape);
		}
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskIncapacitated::Escape_OnEnter()
{
	if (m_moveNetworkHelper.GetNetworkPlayer())
	{
		CPed* pPed = GetPed();
		//Find ped heading
		if (pPed->IsLocalPlayer())
		{
			m_fDesiredEscapeHeading = pPed->GetDesiredHeading(); 
			m_fDesiredEscapeHeadingLocal = SubtractAngleShorter( m_fDesiredEscapeHeading,  pPed->GetCurrentHeading());
		}
		else
		{
			m_fDesiredEscapeHeadingLocal = SubtractAngleShorter( m_fDesiredEscapeHeading,  pPed->GetCurrentHeading());
		}
		TUNE_GROUP_FLOAT(CNC_ARREST, fEscapeHeading, 0.0f, 0.0f, PI, 0.01f);
		TUNE_GROUP_BOOL(CNC_ARREST, bUseTuneHeading, false);
		if(bUseTuneHeading)
		{
			m_fDesiredEscapeHeadingLocal = fEscapeHeading;
		}

		SetupNetworkForEscape();

		m_moveNetworkHelper.SendRequest(ms_RequestEscape);
		m_moveNetworkHelper.WaitForTargetState(ms_EventOnEnterEscape);
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskIncapacitated::Escape_OnUpdate(CPed* pPed)
{
	SetupNetworkForEscape();

	if (pPed->GetUsingRagdoll() || m_pForcedNmTask)
	{
		taskDebugf3("Frame %d : %s CTaskIncapacitated::Incapacitated_OnUpdate - Ped in ragdoll, moving to downed ragdoll", fwTimer::GetFrameCount(), GetPed()->GetDebugName());
		SetState(State_DownedRagdoll);
		return FSM_Continue;
	}
	if (m_bInVehicleAnim && !m_moveNetworkHelper.GetNetworkPlayer())
	{
		return FSM_Quit;
	}
	if (!m_moveNetworkHelper.GetNetworkPlayer() || !m_moveNetworkHelper.IsInTargetState())
	{
		if (GetTimeInState() > 4.f)
		{
			aiAssertf(0, "Stuck in Escape state");
			return FSM_Quit;
		}

		return FSM_Continue;
	}

	// if hit the Clip ended event, quit the task
	if (m_moveNetworkHelper.GetBoolean(ms_EventEscapeFinished))
	{
		ShowWeapon(true);

		return FSM_Quit;
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskIncapacitated::Arrest_OnEnter()
{
	return FSM_Continue;
}

CTask::FSM_Return CTaskIncapacitated::Arrest_OnUpdate()
{
	if (GetTimeInState() > 8.f)
	{
		aiAssertf(0, "Stuck in Arrest state");
		return FSM_Quit;
	}
	return FSM_Continue;
}

aiTask* CTaskIncapacitated::Copy() const
{
	CTaskIncapacitated* pTask = rage_new CTaskIncapacitated();
	pTask->SetIsArrested(m_bArrested);
	pTask->SetClipVeriation(m_bUseClipsetVeriation01);
	pTask->SetGetupTime(m_iGetupTime);
	pTask->SetFastGetupOffset(m_iFastGetupOffset);
	pTask->SetDesiredEscapeHeading(m_fDesiredEscapeHeading);
	return pTask;
}

s32 CTaskIncapacitated::GetDefaultStateAfterAbort() const
{
	return State_Finish;
}

void CTaskIncapacitated::HandleUserInput(CPed* pPed)
{
	if (pPed && pPed->IsLocalPlayer())
	{
		CControl *pControl = pPed->GetControlFromPlayer();
		if (pControl && pControl->GetRespawnFaster().IsPressed())
		{
			m_iFastGetupOffset += GetRecoverFasterModifier();
			if (m_iFastGetupOffset > GetRecoverFasterLimit())
			{
				m_iFastGetupOffset = GetRecoverFasterLimit();
			}
		}
	}
}

void CTaskIncapacitated::SetForcedNaturalMotionTask(aiTask* pSubtask)
{
	taskDebugf3("Frame %d : %s CTaskIncapacitated::SetForcedNaturalMotionTask - Setting new subtask", fwTimer::GetFrameCount(), GetPed()->GetDebugName());
	if (m_pForcedNmTask)
	{
		delete m_pForcedNmTask;
	}
	m_pForcedNmTask = pSubtask;

	aiTask* pTaskToPrint = pSubtask;
	while (pTaskToPrint)
	{
		Printf("name: %s\n", (const char*)pTaskToPrint->GetName());
		pTaskToPrint = pTaskToPrint->GetSubTask();
	}
}

bool CTaskIncapacitated::IsArrestable() const 
{
	const CPed* pPed = GetPed();

	// Ped must not be in these two state to allow arrest
	if (GetState() == State_Escape || GetState() == State_Finish)
	{
		return false;
	}

	// Player must be wanted
	if (pPed->IsPlayer() && pPed->GetPlayerWanted()->GetWantedLevel() == WANTED_CLEAN)
	{
		return false;
	}

	// If player is in vehicle it must be moving slowly
	CVehicle* vCrookVehicle = pPed->GetVehiclePedInside();
	if (vCrookVehicle)
	{
		if (vCrookVehicle->GetVelocity().Mag2() >= square(CTaskPlayerOnFoot::sm_Tunables.m_ArrestVehicleSpeed))
		{
			return false;
		}
	}
	return true;
}

bool CTaskIncapacitated::IsArrestableByPed(CPed* pArresterPed) const 
{
	if (!IsArrestable())
	{
		return false;
	}

	CTaskPlayerOnFoot* pTaskPlayerOnFoot = (CTaskPlayerOnFoot*)pArresterPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PLAYER_ON_FOOT);
	if(!pTaskPlayerOnFoot)
	{
		return false;
	}

	const CPed* pTargetPed = GetPed();

	const float fArrestDistSqr = square(CTaskPlayerOnFoot::sm_Tunables.m_ArrestDistance);
	
	// Handle Ped rules
	const float fPedDistSqr = DistSquared(pTargetPed->GetTransform().GetPosition(), pArresterPed->GetTransform().GetPosition()).Getf();
	if (fPedDistSqr > fArrestDistSqr)
	{
		return false;
	}

	// LOS check
	const u32 includeFlags = ArchetypeFlags::GTA_MAP_TYPE_MOVER | ArchetypeFlags::GTA_OBJECT_TYPE | ArchetypeFlags::GTA_VEHICLE_TYPE;
	static const s32 iNumExceptions = 2;
	const CEntity* ppExceptions[iNumExceptions] = { NULL };
	ppExceptions[0] = pArresterPed;
	ppExceptions[1] = pTargetPed;
	WorldProbe::CShapeTestProbeDesc probeDesc;
	probeDesc.SetStartAndEnd(VEC3V_TO_VECTOR3(pArresterPed->GetTransform().GetPosition()), VEC3V_TO_VECTOR3(pTargetPed->GetTransform().GetPosition()));
	probeDesc.SetIncludeFlags(includeFlags);
	probeDesc.SetExcludeEntities(ppExceptions, iNumExceptions);
	probeDesc.SetExcludeTypeFlags(ArchetypeFlags::GTA_PICKUP_TYPE);
	probeDesc.SetContext(WorldProbe::LOS_GeneralAI);
	return !WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc);
}

void CTaskIncapacitated::ShowWeapon(bool show)
{
	CPed *pPed = GetPed();
	taskAssert(pPed);

	CObject* pWeapon = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponObject() : NULL;

	if (pWeapon)
	{
		pWeapon->SetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY, show, true);
	}
}

void CTaskIncapacitated::SetupNetworkForEscape()
{
	//Update heading for network
	float DesiredEscapeHeading = CTaskMotionBase::ConvertRadianToSignal(m_fDesiredEscapeHeadingLocal);
	m_moveNetworkHelper.SetFloat(ms_GetupHeading, DesiredEscapeHeading);

	if(DesiredEscapeHeading > 0.5f)
	{
		m_moveNetworkHelper.SetFlag(true, ms_UseRightSideGetups);
	}
	else
	{
		m_moveNetworkHelper.SetFlag(true, ms_UseLeftSideGetups);
	}
}

bool CTaskIncapacitated::OverridesNetworkBlender(CPed* UNUSED_PARAM(pPed))
{
	if(GetState() >= State_Incapacitated)
	{
		return true; //if in animated state
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
// CClonedTaskIncapacitatedInfo
//////////////////////////////////////////////////////////////////////////

CClonedTaskIncapacitatedInfo::CClonedTaskIncapacitatedInfo(s32 iState, bool bShouldArrest, bool bUseVeriation01, float fDesiredEscapeHeading)
:m_bShouldArrest(bShouldArrest)
,m_bUseVeriation01(bUseVeriation01)
,m_fEscapeHeading(fDesiredEscapeHeading)
{
	CClonedFSMTaskInfo::SetStatusFromMainTaskState(iState);
}

CClonedTaskIncapacitatedInfo::CClonedTaskIncapacitatedInfo()
{}

CClonedTaskIncapacitatedInfo::~CClonedTaskIncapacitatedInfo()
{}

CTaskFSMClone* CClonedTaskIncapacitatedInfo::CreateCloneFSMTask()
{
	return rage_new CTaskIncapacitated();
}

#endif // CNC_MODE_ENABLED