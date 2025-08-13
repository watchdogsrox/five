// FILE :    TaskReactAndFlee.h
// CREATED : 11-8-2011

// File header
#include "Task/Response/TaskReactAndFlee.h"

// Rage headers
#include "math/angmath.h"

// Game headers
#include "AI/Ambient/ConditionalAnimManager.h"
#include "Animation/MovePed.h"
#include "event/EventWeapon.h"
#include "Peds/ped.h"
#include "Peds/PedIntelligence.h"
#include "Task/Motion/TaskMotionBase.h"
#include "Task/Movement/TaskNavMesh.h"
#include "Task/Response/TaskFlee.h"
#include "Task/Response/TaskShockingEvents.h"
#include "Task/Combat/TaskThreatResponse.h"
#include "Task/Scenario/Types/TaskCowerScenario.h"

AI_OPTIMISATIONS()

//Static Initialization -- Tuning
CTaskReactAndFlee::Tunables CTaskReactAndFlee::sm_Tunables;
IMPLEMENT_RESPONSE_TASK_TUNABLES(CTaskReactAndFlee, 0x3797fe17);

//Static Initialization -- MoVE
fwMvBooleanId CTaskReactAndFlee::ms_BlendOutId("BlendOut",0x6DD1ABBC);
fwMvBooleanId CTaskReactAndFlee::ms_ReactClipEndedId("ReactClipEnded",0x98C3FDFD);
fwMvBooleanId CTaskReactAndFlee::ms_ReactToFleeClipEndedId("ReactToFleeClipEnded",0x4E3F7A35);

fwMvClipId CTaskReactAndFlee::ms_ReactClipId("ReactClip",0x5BC14662);
fwMvClipId CTaskReactAndFlee::ms_ReactToFleeClipId("ReactToFleeClip",0x6B493E9D);

fwMvFlagId CTaskReactAndFlee::ms_FleeBackId("FleeBack",0xA92E6404);
fwMvFlagId CTaskReactAndFlee::ms_FleeLeftId("FleeLeft",0x42D0CC5F);
fwMvFlagId CTaskReactAndFlee::ms_FleeFrontId("FleeFront",0x2D1AE7A);
fwMvFlagId CTaskReactAndFlee::ms_FleeRightId("FleeRight",0x12872467);
fwMvFlagId CTaskReactAndFlee::ms_ReactBackId("ReactBack",0x582D458);
fwMvFlagId CTaskReactAndFlee::ms_ReactLeftId("ReactLeft",0xAD3A73C7);
fwMvFlagId CTaskReactAndFlee::ms_ReactFrontId("ReactFront",0x49FA785A);
fwMvFlagId CTaskReactAndFlee::ms_ReactRightId("ReactRight",0xA2F5709A);

fwMvFloatId CTaskReactAndFlee::ms_RateId("Rate",0x7E68C088);
fwMvFloatId CTaskReactAndFlee::ms_PhaseId("Phase",0xA27F482B);

fwMvRequestId CTaskReactAndFlee::ms_ReactId("React",0x13A3EFDC);
fwMvRequestId CTaskReactAndFlee::ms_ReactToFleeId("ReactToFlee",0x1D9A0AC5);

u32 CTaskReactAndFlee::ms_uLastTimeReactedInDirection = 0;

namespace
{
	const float g_fMaxWptDistanceSqr = rage::square(1.75f);
	const float g_fMinReactionDistanceSqr = rage::square(2.25f);
	const float g_fMaxWptDot = rage::Cosf(DtoR * 15.f);
};

////////////////////
//Main task response info
////////////////////

CClonedReactAndFleeInfo::CClonedReactAndFleeInfo( CClonedReactAndFleeInfo::InitParams const& initParams ) 
:
	m_targetEntity(const_cast<CEntity*>(initParams.m_targetEntity)), // NOTE[egarcia]: Should CSynced entity need a non const Entity?
	m_targetEntityWasInvalidated(initParams.m_targetEntityWasInvalidated),
	m_targetPosition(initParams.m_targetPosition),
	m_controlFlags(initParams.m_controlFlags),
	m_rate(initParams.m_rate),
	m_ambientClipHash(initParams.m_ambientClipHash),
	m_reactClipSetId(initParams.m_reactClipSetId),
	m_reactToFleeClipSetId(initParams.m_reactToFleeClipSetId),
	m_isResponseToEventType(static_cast<u8>(initParams.m_isResponseToEventType)),
	m_reacted(initParams.m_reacted),
	m_reactionCount(initParams.m_reactionCount),
	m_clipHash(initParams.m_clipHash),
	m_screamActive(initParams.m_screamActive)
{
	SetStatusFromMainTaskState(initParams.m_state);	

	taskAssert(m_reactClipSetId != CLIP_SET_ID_INVALID);
	taskAssert(static_cast<s32>(initParams.m_isResponseToEventType) >= 0 && static_cast<s32>(initParams.m_isResponseToEventType) <= 255);
}

CClonedReactAndFleeInfo::CClonedReactAndFleeInfo()
{}

CClonedReactAndFleeInfo::~CClonedReactAndFleeInfo()
{}

CTaskFSMClone *CClonedReactAndFleeInfo::CreateCloneFSMTask()
{
    return rage_new CTaskReactAndFlee(GetFleeAITarget(), m_reactClipSetId, m_reactToFleeClipSetId, m_controlFlags, static_cast<eEventType>(m_isResponseToEventType), m_screamActive);
}

CTask*	CClonedReactAndFleeInfo::CreateLocalTask(fwEntity* UNUSED_PARAM(pEntity))
{
	return rage_new CTaskReactAndFlee(GetFleeAITarget(), m_reactClipSetId, m_reactToFleeClipSetId, m_controlFlags, static_cast<eEventType>(m_isResponseToEventType), m_screamActive);
}

CAITarget CClonedReactAndFleeInfo::GetFleeAITarget() const
{
	CAITarget target;
	if (m_targetEntity.IsValid() && m_targetEntity.GetEntity())
	{
		target.SetEntity(m_targetEntity.GetEntity());
	}
	else
	{
		target.SetPosition(VEC3V_TO_VECTOR3(m_targetPosition));
	}
	return target;
}

////////////////////
//Main task
////////////////////

CTaskReactAndFlee::CTaskReactAndFlee(const CAITarget& rTarget, fwMvClipSetId reactClipSetId, fwMvClipSetId reactToFleeClipSetId, fwFlags8 uConfigFlags, eEventType eIsResponseToEventType, bool bScreamActive)
: m_vLastTargetPosition(V_ZERO)
, m_Target(rTarget)
, m_bTargetEntityWasInvalidated(false)
, m_ReactClipSetId(reactClipSetId)
, m_ReactToFleeClipSetId(reactToFleeClipSetId)
, m_nBasicAnimations(BA_None)
, m_fRate(0.0f)
, m_fMoveBlendRatio(MOVEBLENDRATIO_RUN)
, m_fExitBlendDuration(REALLY_SLOW_BLEND_DURATION)
, m_nReactDirection(D_None)
, m_nFleeDirection(D_None)
, m_uConfigFlagsForFlee(0)
, m_uConfigFlags(uConfigFlags)
, m_eIsResponseToEventType(eIsResponseToEventType)
, m_clipHash(CLIP_ID_INVALID)
, m_reacted(false)
, m_restartReaction(false)
, m_reactionCount(0)
, m_networkReactionCount(0)
, m_bOutgoingMigratedWhileCowering(false)
, m_bIncomingMigratedWhileCowering(false)
, m_screamActive(bScreamActive)
{
	m_vPrevWaypoint = VEC3_ZERO;
	m_vNextWaypoint = VEC3_ZERO;
	SetInternalTaskType(CTaskTypes::TASK_REACT_AND_FLEE);

	taskAssert(m_ReactClipSetId != CLIP_SET_ID_INVALID);
	taskAssert(m_ReactToFleeClipSetId != CLIP_SET_ID_INVALID);
}

CTaskReactAndFlee::CTaskReactAndFlee(const CAITarget& rTarget, BasicAnimations nBasicAnimations, fwFlags8 uConfigFlags, eEventType eIsResponseToEventType, bool bScreamActive)
: m_vLastTargetPosition(V_ZERO)
, m_Target(rTarget)
, m_bTargetEntityWasInvalidated(false)
, m_ReactClipSetId(CLIP_SET_ID_INVALID)
, m_ReactToFleeClipSetId(CLIP_SET_ID_INVALID)
, m_nBasicAnimations(nBasicAnimations)
, m_fRate(0.0f)
, m_fMoveBlendRatio(MOVEBLENDRATIO_RUN)
, m_fExitBlendDuration(REALLY_SLOW_BLEND_DURATION)
, m_nReactDirection(D_None)
, m_nFleeDirection(D_None)
, m_uConfigFlagsForFlee(0)
, m_uConfigFlags(uConfigFlags)
, m_eIsResponseToEventType(eIsResponseToEventType)
, m_clipHash(CLIP_ID_INVALID)
, m_reacted(false)
, m_restartReaction(false)
, m_reactionCount(0)
, m_networkReactionCount(0)
, m_bOutgoingMigratedWhileCowering(false)
, m_bIncomingMigratedWhileCowering(false)
, m_screamActive(bScreamActive)
{
	m_vPrevWaypoint = VEC3_ZERO;
	m_vNextWaypoint = VEC3_ZERO;
	SetInternalTaskType(CTaskTypes::TASK_REACT_AND_FLEE);

	taskAssert(m_nBasicAnimations != BA_None);
}

CTaskReactAndFlee::~CTaskReactAndFlee()
{

}

void CTaskReactAndFlee::SetMoveBlendRatio(float fMoveBlendRatio)
{
	//Ensure the move/blend ratio is changing.
	if(Abs(m_fMoveBlendRatio - fMoveBlendRatio) < SMALL_FLOAT)
	{
		return;
	}

	//Set the move/blend ratio.
	m_fMoveBlendRatio = fMoveBlendRatio;

	//Check the state.
	switch(GetState())
	{
		case State_Flee:
		{
			//Check if the sub-task is valid.
			CTaskSmartFlee* pSubTask = static_cast<CTaskSmartFlee *>(GetSubTask());
			if(pSubTask)
			{
				pSubTask->SetMoveBlendRatio(m_fMoveBlendRatio);
			}
			break;
		}
		default:
		{
			break;
		}
	}
}

bool CTaskReactAndFlee::SpheresOfInfluenceBuilder(TInfluenceSphere* apSpheres, int& iNumSpheres, const CPed* pPed)
{
	//Ensure the task is valid.
	CTaskReactAndFlee* pTask = static_cast<CTaskReactAndFlee *>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_REACT_AND_FLEE));
	if(!pTask)
	{
		return false;
	}

	return CTaskSmartFlee::SpheresOfInfluenceBuilderForTarget(apSpheres, iNumSpheres, pPed, pTask->m_Target);
}

bool CTaskReactAndFlee::IsValidForMotionTask(CTaskMotionBase& task) const
{
	bool bIsValid = task.IsInWater();
	if (!bIsValid)
		bIsValid = CTask::IsValidForMotionTask(task);

	if (!bIsValid && GetSubTask())
		bIsValid = GetSubTask()->IsValidForMotionTask(task);

	return bIsValid;
}

void CTaskReactAndFlee::CleanUp()
{
	//Check if the move network is active.
	if(m_MoveNetworkHelper.IsNetworkActive())
	{	
		//Clear the move network.
		GetPed()->GetMovePed().ClearTaskNetwork(m_MoveNetworkHelper, SLOW_BLEND_DURATION);
	}
}

aiTask* CTaskReactAndFlee::Copy() const
{
	//Create the task.
	CTaskReactAndFlee* pTask = (m_nBasicAnimations == BA_None) ?
		rage_new CTaskReactAndFlee(m_Target, m_ReactClipSetId, m_ReactToFleeClipSetId, m_uConfigFlags, m_eIsResponseToEventType,m_screamActive) :
		rage_new CTaskReactAndFlee(m_Target, m_nBasicAnimations, m_uConfigFlags, m_eIsResponseToEventType,m_screamActive);
	
	//Set the ambient clips.
	pTask->SetAmbientClips(m_hAmbientClips);

	//Set the move blend ratio.
	pTask->SetMoveBlendRatio(m_fMoveBlendRatio);

	//Set the config flags for flee.
	pTask->SetConfigFlagsForFlee(m_uConfigFlagsForFlee);

	//Set the last target position.
	pTask->SetLastTargetPosition(m_vLastTargetPosition);

	pTask->m_vPrevWaypoint = m_vPrevWaypoint;
	pTask->m_vNextWaypoint = m_vNextWaypoint;
	
	//Set target entity no longer valid
	pTask->SetTargetEntityWasInvalidated(m_bTargetEntityWasInvalidated);

	return pTask;
}

#if !__FINAL
void CTaskReactAndFlee::Debug() const
{
#if DEBUG_DRAW
	DebugRender();
#endif

	//Call the base class version.
	CTask::Debug();
}
#endif // !__FINAL

#if !__FINAL
const char * CTaskReactAndFlee::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_Start&&iState<=State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_Resume",
		"State_StreamReact",
		"State_React",
		"State_StreamReactToFlee",
		"State_ReactToFlee",
		"State_Flee",
		"State_StreamReactComplete_Clone",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif // !__FINAL

CTask::FSM_Return CTaskReactAndFlee::ProcessPreFSM()
{
	TASK_REACT_AND_FLEE_TTY_FUNCTION_OUTPUT

	//Check if we should process the targets.
	if(ShouldProcessTargets())
	{
		//Ensure the target is valid.
		if(!CFleeHelpers::ProcessTargets(m_Target, m_SecondaryTarget, m_vLastTargetPosition))
		{
			return FSM_Quit;
		}
	}

	// Check if the target entity is no longer valid and notify
	if (m_Target.GetMode() == CAITarget::Mode_Entity && !IsValidTargetEntity(m_Target.GetEntity()))
	{
		OnTargetEntityNoLongerValid();
	}

	//Process the move task.
	ProcessMoveTask();

	//Process the streaming.
	ProcessStreaming();
	
    CPed *pPed = GetPed();

    if (pPed && pPed->GetNetworkObject() && !pPed->IsNetworkClone())
    {
        NetworkInterface::ForceHighUpdateLevel(pPed);
    }

	return FSM_Continue;
}

void CTaskReactAndFlee::OnCloneTaskNoLongerRunningOnOwner()
{
	SetStateFromNetwork(State_Finish);
	return;
}

CTaskFSMClone* CTaskReactAndFlee::CreateTaskForClonePed(CPed* pPed)	
{
	TASK_REACT_AND_FLEE_TTY_FUNCTION_OUTPUT

	// we can only migrate when we're fleeing so by now we've passed playing reaction so we should just flee....
	CTaskReactAndFlee* reactTask = rage_new CTaskReactAndFlee(m_Target, m_ReactClipSetId, m_ReactToFleeClipSetId, m_uConfigFlags, m_eIsResponseToEventType);
	reactTask->GetConfigFlags().SetFlag(CF_DisableReact);
	reactTask->SetReactionCount(m_reactionCount);

	//Set the ambient clips.
	reactTask->SetAmbientClips(m_hAmbientClips);

	if(CheckForCowerMigrate(pPed))
	{
		reactTask->m_bIncomingMigratedWhileCowering = true;
		m_bOutgoingMigratedWhileCowering = true;
	}

	return reactTask;
}

bool CTaskReactAndFlee::ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
	if( NetworkInterface::IsGameInProgress() && 
		m_bIncomingMigratedWhileCowering && 
		GetState() == State_Start )
	{	
		bool bRagDoll = (pEvent && ((CEvent*)pEvent)->RequiresAbortForRagdoll()) || (GetPed() && GetPed()->GetUsingRagdoll());

		if(!bRagDoll)
		{
			//Prevents any non rag-doll event from destroying the task tree when cowering ped migrates to local 
			//and the event is processed before the cower sub task is created and set
			return false;
		}
	}
	
	return CTask::ShouldAbort(iPriority, pEvent);
}

CTaskFSMClone* CTaskReactAndFlee::CreateTaskForLocalPed(CPed* pPed)
{
	TASK_REACT_AND_FLEE_TTY_FUNCTION_OUTPUT

	// we can only migrate when we're fleeing so by now we've passed playing reaction so we should just flee....
	CTaskReactAndFlee* reactTask = rage_new CTaskReactAndFlee(m_Target, m_ReactClipSetId, m_ReactToFleeClipSetId, m_uConfigFlags, m_eIsResponseToEventType);
	reactTask->GetConfigFlags().SetFlag(CF_DisableReact);
	reactTask->SetReactionCount(m_reactionCount);

	//Set the ambient clips.
	reactTask->SetAmbientClips(m_hAmbientClips);

	if(CheckForCowerMigrate(pPed))
	{
		reactTask->m_bIncomingMigratedWhileCowering = true;
		m_bOutgoingMigratedWhileCowering = true;
	}

	//Keep smart flee task info for the creation of the new one
	if (GetState() == State_Flee)
	{
		const CTask* pSubTask = GetSubTask(); 
		const CTaskSmartFlee* pFleeSubTask = pSubTask && (pSubTask->GetTaskType() == CTaskTypes::TASK_SMART_FLEE) ? SafeCast(const CTaskSmartFlee, pSubTask) : NULL;
		if (pFleeSubTask)
		{
			reactTask->m_CreateSmartFleeTaskParams.Set(pFleeSubTask->GetIgnorePreferPavementsTimeLeft());
		}
	}

	return reactTask;
}

bool CTaskReactAndFlee::CheckForCowerMigrate(const CPed* pPed)
{
	if(taskVerifyf(pPed,"Expect valid ped") && taskVerifyf(NetworkInterface::IsNetworkOpen(),"Only used in network games"))
	{
		CTaskCowerScenario* pTaskCowerScenario = static_cast<CTaskCowerScenario *>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COWER_SCENARIO));
		if( pTaskCowerScenario && 
			pTaskCowerScenario->GetState() > CTaskCowerScenario::State_DirectedIntro &&
			pTaskCowerScenario->GetState() < CTaskCowerScenario::State_Outro)
		{
			return true;
		}
	}

	return false;
}

bool CTaskReactAndFlee::ControlPassingAllowed(CPed *pPed, const netPlayer& UNUSED_PARAM(player), eMigrationType UNUSED_PARAM(migrationType))
{
	if (pPed && pPed->IsNetworkClone() && !m_cloneTaskInScope)
	{
		return true;
	}

	// if I'm streaming in or reacting, don't pass control - wait until I'm running in a few frames time....
	if(GetState() == State_Flee)
	{
		return true;
	}

	return false;
}

CTaskInfo* CTaskReactAndFlee::CreateQueriableState() const
{
	TASK_REACT_AND_FLEE_TTY_FUNCTION_OUTPUT

	Assert(GetState() != State_StreamReactComplete_Clone);

	Vec3V vTargetPosition(V_ZERO);
	
	if(m_Target.GetIsValid())
	{
		m_Target.GetPosition(RC_VECTOR3(vTargetPosition));
	}
	else
	{
		// the target has went out of scope but the task is coded to keep going - we send last viable position instead....
		vTargetPosition = m_vLastTargetPosition;
	}

	bool bScreamActive = false;
	CEvent* pEvent = GetPed()->GetPedIntelligence()->GetCurrentEvent();
	if (pEvent && pEvent->GetSourcePed() && (pEvent->GetEventType() == EVENT_SHOCKING_SEEN_WEAPON_THREAT || pEvent->GetEventType() == EVENT_GUN_AIMED_AT))
	{
		bScreamActive = true;
	}

	CClonedReactAndFleeInfo::InitParams initParams(	
													GetState(), 
													m_uConfigFlags,
													m_hAmbientClips,
													m_fRate,
													m_Target.GetEntity(),
													m_bTargetEntityWasInvalidated,
													vTargetPosition,
													m_ReactClipSetId,
													m_ReactToFleeClipSetId,
													m_reacted,
													m_reactionCount,
													m_clipHash,
													m_eIsResponseToEventType,
													bScreamActive
												);

	return rage_new CClonedReactAndFleeInfo( initParams); 
}

void CTaskReactAndFlee::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	TASK_REACT_AND_FLEE_TTY_FUNCTION_OUTPUT

	Assert( pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_REACT_AND_FLEE );
    CClonedReactAndFleeInfo *reactAndFleeInfo = static_cast<CClonedReactAndFleeInfo*>(pTaskInfo);

	Assert(reactAndFleeInfo->GetState() != State_StreamReactComplete_Clone);

	if (reactAndFleeInfo->GetTargetEntity())
	{
		if (IsValidTargetEntity(reactAndFleeInfo->GetTargetEntity()))
		{
			m_Target.SetEntity(reactAndFleeInfo->GetTargetEntity());
			m_bTargetEntityWasInvalidated = false;
		}
	}
	else
	{
		m_Target.SetPosition(VEC3V_TO_VECTOR3(reactAndFleeInfo->GetTargetPosition()));
	}

	m_fRate					= reactAndFleeInfo->GetRate();
	m_uConfigFlags			= reactAndFleeInfo->GetControlFlags();
	m_hAmbientClips			= reactAndFleeInfo->GetAmbientClipHash();
	m_ReactClipSetId		= reactAndFleeInfo->GetReactClipSetId();
	m_ReactToFleeClipSetId	= reactAndFleeInfo->GetReactToFleeClipSetId();
	m_clipHash				= reactAndFleeInfo->GetClipHash();
	m_networkReactionCount	  = reactAndFleeInfo->GetReactionCount();
	m_eIsResponseToEventType  = static_cast<eEventType>(reactAndFleeInfo->GetIsResponseToEventType());

	m_screamActive			= reactAndFleeInfo->GetScreamActive();

	CTaskFSMClone::ReadQueriableState(pTaskInfo);
}

void CTaskReactAndFlee::UpdateClonedSubTasks(CPed* pPed, int const iState)
{
	TASK_REACT_AND_FLEE_TTY_FUNCTION_OUTPUT

	CTaskTypes::eTaskType expectedTaskType = CTaskTypes::TASK_INVALID_ID;
	switch(iState)
	{
		case State_Flee:
			expectedTaskType = CTaskTypes::TASK_SMART_FLEE; 
			break;
		default:
			return;
	}

	if(expectedTaskType != CTaskTypes::TASK_INVALID_ID)
	{
		/*CTaskFSMClone::*/CreateSubTaskFromClone(pPed, expectedTaskType);
	}
}

CTaskReactAndFlee::FSM_Return CTaskReactAndFlee::UpdateClonedFSM(const s32 iState, const FSM_Event iEvent)
{
	TASK_REACT_AND_FLEE_TTY_UPDATE_FSM_OUTPUT

	if(iState != State_Finish)
	{
		if(iEvent == OnUpdate)
		{
			// only state we're interested in from the network...
			if(GetStateFromNetwork() == State_Finish)
			{
				TaskSetState(GetStateFromNetwork());
				return FSM_Continue;
			}

			UpdateClonedSubTasks(GetPed(), GetState());
		}
	}

	FSM_Begin
		
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_Resume)

		FSM_State(State_StreamReact)
			FSM_OnUpdate
				return StreamReact_OnUpdate();
				
		FSM_State(State_React)
			FSM_OnEnter
				React_OnEnter();
			FSM_OnUpdate
				return React_OnUpdate();
			FSM_OnExit
				React_OnExit();
				
		FSM_State(State_StreamReactToFlee)
			FSM_OnUpdate
				return StreamReactToFlee_OnUpdate();

		FSM_State(State_ReactToFlee)
			FSM_OnEnter
				ReactToFlee_OnEnter();
			FSM_OnUpdate
				return ReactToFlee_OnUpdate();
			FSM_OnExit
				ReactToFlee_OnExit();
		
		FSM_State(State_Flee)
			FSM_OnEnter
				Flee_OnEnter();
			FSM_OnUpdate
				return Flee_Clone_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

		FSM_State(State_StreamReactComplete_Clone)
			FSM_OnUpdate
				return StreamReactComplete_Clone_OnUpdate();

	FSM_End
}

CTask::FSM_Return CTaskReactAndFlee::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	TASK_REACT_AND_FLEE_TTY_UPDATE_FSM_OUTPUT

	// Check if we need to restart the current reaction
	if (m_restartReaction && iEvent == OnUpdate)
	{
		RestartReaction();
	}

	FSM_Begin
	
		FSM_State(State_Start)
			FSM_OnEnter
				Start_OnEnter();
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_Resume)
			FSM_OnUpdate
				return Resume_OnUpdate();
				
		FSM_State(State_StreamReact)
			FSM_OnUpdate
				return StreamReact_OnUpdate();
				
		FSM_State(State_React)
			FSM_OnEnter
				React_OnEnter();
			FSM_OnUpdate
				return React_OnUpdate();
			FSM_OnExit
				React_OnExit();
				
		FSM_State(State_StreamReactToFlee)
			FSM_OnUpdate
				return StreamReactToFlee_OnUpdate();
				
		FSM_State(State_ReactToFlee)
			FSM_OnEnter
				ReactToFlee_OnEnter();
			FSM_OnUpdate
				return ReactToFlee_OnUpdate();
			FSM_OnExit
				ReactToFlee_OnExit();
				
		FSM_State(State_Flee)
			FSM_OnEnter
				Flee_OnEnter();
			FSM_OnUpdate
				return Flee_OnUpdate();

		FSM_State(State_StreamReactComplete_Clone)
			FSM_OnUpdate
				return StreamReactComplete_Clone_OnUpdate();
				
		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

void CTaskReactAndFlee::Start_OnEnter()
{
	//Initialize the secondary target.
	CFleeHelpers::InitializeSecondaryTarget(m_Target, m_SecondaryTarget);

	//Initialize the clip sets.
	switch(m_nBasicAnimations)
	{
		case BA_Gunfire:
		{
			//Get the position.
			Vec3V vPosition;
			m_Target.GetPosition(RC_VECTOR3(vPosition));

			//Set the clip sets.
			m_ReactClipSetId		= PickDefaultGunfireReactionClipSet(vPosition, *GetPed());
			m_ReactToFleeClipSetId	= CLIP_SET_REACTION_GUNFIRE_RUNS;

			break;
		}
		case BA_ShockingEvent:
		{
			//Get the position.
			Vec3V vPosition;
			m_Target.GetPosition(RC_VECTOR3(vPosition));

			//Set the clip sets.
			m_ReactClipSetId		= PickDefaultShockingEventReactionClipSet(vPosition, *GetPed());
			m_ReactToFleeClipSetId	= CLIP_SET_REACTION_SHOCKING_WALKS;

			break;
		}
		default:
		{
			break;
		}
	}

	TryToScream();
}

CTask::FSM_Return CTaskReactAndFlee::Start_OnUpdate()
{
	TASK_REACT_AND_FLEE_TTY_FUNCTION_OUTPUT

	CPed* pPed = GetPed();

	//Check if we are swimming or in a vehicle or attached.
	if(pPed->GetIsSwimming() || pPed->GetIsInVehicle() || pPed->GetIsAttached() || IsOnTrain())
	{
		//Move to the flee state.
		SetState(State_Flee);

		return FSM_Continue;
	}

	//Stream the MoVE network.
	bool bNetworkIsStreamedIn = fwAnimDirector::RequestNetworkDef(CClipNetworkMoveInfo::ms_NetworkTaskReactAndFlee);

	if (bNetworkIsStreamedIn)
	{
		//Set up the move network on the ped.
		m_MoveNetworkHelper.CreateNetworkPlayer(pPed, CClipNetworkMoveInfo::ms_NetworkTaskReactAndFlee);

		pPed->GetMovePed().SetTaskNetwork(m_MoveNetworkHelper.GetNetworkPlayer(), SLOW_BLEND_DURATION);

		//Generate the react direction value.
		float fReactDirectionValue = GenerateReactDirectionValue();

		//Generate the react direction.
		float fNewReactDirectionValue = fReactDirectionValue;
		m_nReactDirection = GenerateDirectionFromValue(fNewReactDirectionValue);

		Assert(m_nReactDirection != D_None);

		//Set the react direction.
		switch(m_nReactDirection)
		{
			case D_Back:
			{
				m_MoveNetworkHelper.SetFlag(true, ms_ReactBackId);
				break;
			}
			case D_Left:
			{
				m_MoveNetworkHelper.SetFlag(true, ms_ReactLeftId);
				break;
			}
			case D_Front:
			{
				m_MoveNetworkHelper.SetFlag(true, ms_ReactFrontId);
				break;
			}
			case D_Right:
			{
				m_MoveNetworkHelper.SetFlag(true, ms_ReactRightId);
				break;
			}
			default:
			{
				taskAssertf(false, "Unknown direction: %d", m_nReactDirection);
				break;
			}
		}

		//Check if the react is disabled.
		if(m_uConfigFlags.IsFlagSet(CF_DisableReact))
		{
			//Move to the stream react to flee state.
			TaskSetState(State_StreamReactToFlee);
		}
		else
		{
			//Move to the stream react state.
			TaskSetState(State_StreamReact);
		}
	}
	
	return FSM_Continue;
}

CTask::FSM_Return CTaskReactAndFlee::Resume_OnUpdate()
{
	//Set the state to resume to.
	bool bKeepSubTask = false;
	int iStateToResumeTo = ChooseStateToResumeTo(bKeepSubTask);
	SetState(iStateToResumeTo);

	//Check if we should keep the sub-task.
	if(bKeepSubTask)
	{
		//Keep the sub-task across the transition.
		SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskReactAndFlee::StreamReact_OnUpdate()
{
	TASK_REACT_AND_FLEE_TTY_FUNCTION_OUTPUT

	//Stream the clip set.
	bool bClipSetIsStreamedIn = m_ReactClipSetRequestHelper.IsLoaded();
	
	if(!GetPed()->IsNetworkClone())
	{
		//Check if everything is streamed in.
		if(bClipSetIsStreamedIn)
		{	
			//Set the rate.
			m_fRate = fwRandom::GetRandomNumberInRange(sm_Tunables.m_MinRate, sm_Tunables.m_MaxRate);
			m_MoveNetworkHelper.SetFloat(ms_RateId, m_fRate);
			
			//Move to the react state.
			TaskSetState(State_React);
		}
		else if(GetTimeInState() > sm_Tunables.m_MaxReactionTime)
		{
			//If everything can't stream in at a normal(ish) reaction time, the reaction will look weird, just flee.
			TaskSetState(State_Flee);
		}
	}
	else
	{
		//Check if everything is streamed in... 
		if(bClipSetIsStreamedIn)
		{	
			Assertf( m_ReactClipSetRequestHelper.GetClipSetId().GetHash() == m_ReactClipSetId.GetHash(),
				"%s has mismatch m_ReactClipSetRequestHelper %s [0x%x], m_ReactClipSetId %s [0x%x]. WasLocallyStarted %s",
				GetPed()->GetDebugName(),
				m_ReactClipSetRequestHelper.GetClipSetId().GetCStr(),
				m_ReactClipSetRequestHelper.GetClipSetId().GetHash(),
				m_ReactClipSetId.GetCStr(),
				m_ReactClipSetId.GetHash(),
				WasLocallyStarted()?"TRUE":"FALSE");

			Assert(m_MoveNetworkHelper.GetNetworkPlayer());

			//Set a rate for the clips.
			m_MoveNetworkHelper.SetFloat(ms_RateId, m_fRate);

			TaskSetState(State_StreamReactComplete_Clone);
		}
		// if the owner flees, we flee ignoring if the owner played the reaction while we've been streaming the anim in...
		else if(GetStateFromNetwork() == State_Flee /*&& !m_reacted*/) 
		{
			TaskSetState(State_StreamReactComplete_Clone);
		}
	}
	
	return FSM_Continue;
}

static bank_float sfReactRagdollIncomingAnimVelocityScale = 0.5f;

void CTaskReactAndFlee::React_OnEnter()
{
	TASK_REACT_AND_FLEE_TTY_FUNCTION_OUTPUT

	//Set the clip set.
	SetClipSetForState();

	CPed* pPed = GetPed();
	fragInstNMGta* pRagdollInst = pPed->GetRagdollInst();
	if (pRagdollInst != NULL)
	{
		// The initial velocity of the ragdoll will be calculated from the previous and current skeleton bounds.
		// This can be a problem when reacting to fleeing since the ped ducks down somewhat before
		// starting to run. If the character ragdolls as they are coming out of the duck they can end up inheriting 
		// quite a bit of upwards velocity (see bug #1928204). Trying to keep this change as local as possible so
		// only enabling this limit while reacting to fleeing

		// The next time the ragdoll is activated, the initial velocity will be scaled
		pRagdollInst->SetIncomingAnimVelocityScaleReset(sfReactRagdollIncomingAnimVelocityScale);

		taskWarningf("CTaskReactAndFlee::React_OnEnter: Scaling initial ragdoll velocity by %f", sfReactRagdollIncomingAnimVelocityScale);
	}

	if(!GetPed()->IsNetworkClone())
	{
		static dev_bool s_bUseBlackBoard = false;
		if(s_bUseBlackBoard)
		{
			// Pick a random clip from the clipset, using the anim blackboard
			bool bValidPicked = CAmbientClipRequestHelper::PickRandomClipInClipSetUsingBlackBoard( GetPed(), m_ReactClipSetId, m_clipHash );
			if (!bValidPicked)
			{
				// Failed to pick an animation, so try again next frame.
				SetFlag(aiTaskFlags::RestartCurrentState);
				return;
			}
		}
		else
		{
			Verifyf(CAmbientClipRequestHelper::PickRandomClipInClipSet(m_ReactClipSetId, m_clipHash), "No clips inside the specified clipset!");
		}

		// We have a valid clip to play. Set the flag to say we've reacted.
		m_reacted = true;
	}

	Assertf(m_clipHash != CLIP_ID_INVALID, "ERROR : CTaskReactAndFlee::React_OnEnter : Clip Hash not valid!?");
	const crClip* pClip = fwAnimManager::GetClipIfExistsByDictIndex(fwAnimManager::GetClipDictIndex(m_ReactClipSetId), m_clipHash);
	if (Verifyf(pClip, "Picked a valid clip, but it did not exist."))
	{
// temporary logging to track down a clip set bug....
#if TASK_REACT_AND_FLEE_TTY_DEBUG_ENABLE && __BANK

		char buffer[256] = "NO CLIP";
		pClip->GetDebugName(buffer, 256);

		if(NetworkDebug::IsTaskDebugStateLoggingEnabled()) 
		{
			Displayf("Task %p : %s : Ped %p %s : SetClip %s : ClipId %s", this, __FUNCTION__, GetEntity(), GetEntity() ? GetPed()->GetDebugName() : "NO PED", buffer, ms_ReactClipId.GetCStr()); 
		}
#endif /* TASK_REACT_AND_FLEE_TTY_DEBUG_ENABLE && __BANK */

		m_MoveNetworkHelper.SetClip(pClip, ms_ReactClipId);
	}

// temporary logging to track down a clip set bug....
#if TASK_REACT_AND_FLEE_TTY_DEBUG_ENABLE && __BANK
	if(NetworkDebug::IsTaskDebugStateLoggingEnabled()) 
	{
		Displayf("Task %p : %s : Ped %p %s : SendRequest %s", this, __FUNCTION__, GetEntity(), GetEntity() ? GetPed()->GetDebugName() : "NO PED", ms_ReactId.GetCStr()); 
	}
#endif /* TASK_REACT_AND_FLEE_TTY_DEBUG_ENABLE && __BANK*/
	
	//Send the react request.
	m_MoveNetworkHelper.SendRequest(ms_ReactId);
	
	//Wait for the react clip to end.
	m_MoveNetworkHelper.WaitForTargetState(ms_ReactClipEndedId);
}

CTask::FSM_Return CTaskReactAndFlee::React_OnUpdate()
{
	TASK_REACT_AND_FLEE_TTY_FUNCTION_OUTPUT

	//Check if we have spent enough time blending in.
	static dev_float s_fMinTime = 0.5f;
	if(GetTimeInState() > s_fMinTime)
	{
		//Force the motion state.
		GetPed()->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Idle);
	}

	if(m_MoveNetworkHelper.IsInTargetState())
	{
		TaskSetState(State_StreamReactToFlee);
	}
	
	return FSM_Continue;
}

void CTaskReactAndFlee::React_OnExit()
{
	CPed* pPed = GetPed();

	//Force the motion state.
	pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Idle);

	fragInstNMGta* pRagdollInst = pPed->GetRagdollInst();
	if (pRagdollInst != NULL)
	{
		// Reset the incoming anim velocity scale
		pRagdollInst->SetIncomingAnimVelocityScaleReset(-1.0f);

		taskWarningf("CTaskReactAndFlee::React_OnExit: Reset initial ragdoll velocity");
	}
}

CTask::FSM_Return CTaskReactAndFlee::StreamReactToFlee_OnUpdate()
{
	TASK_REACT_AND_FLEE_TTY_FUNCTION_OUTPUT

#if __BANK
	TUNE_GROUP_BOOL(TASK_REACT_AND_FLEE, bDisableReactToFlee, false);
	if(bDisableReactToFlee)
	{
		TaskSetState(State_Flee);
		return FSM_Continue;
	}
#endif

	bool bClipSetIsStreamedIn = m_ReactToFleeClipSetRequestHelper.IsLoaded();
	bool bHasRoute = HasRoute();

	if(bClipSetIsStreamedIn && GetPed()->IsNetworkClone())
	{
		TaskSetState(State_ReactToFlee);
		return FSM_Continue;
	}

	if(bHasRoute && !CanDoFullReactionAndGetWpt(m_vPrevWaypoint, m_vNextWaypoint))
	{
		TaskSetState(State_Flee);
	}
	else if(bClipSetIsStreamedIn && bHasRoute)
	{
		TaskSetState(State_ReactToFlee);
	}
	else
	{
		//At this point we have played a reaction and the transition isn't ready, just flee.
		TaskSetState(State_Flee);
	}
	
	return FSM_Continue;
}

void CTaskReactAndFlee::ReactToFlee_OnEnter()
{
	TASK_REACT_AND_FLEE_TTY_FUNCTION_OUTPUT

	//Set the clip set.
	SetClipSetForState();
	
// temporary logging to track down a clip set bug....
#if TASK_REACT_AND_FLEE_TTY_DEBUG_ENABLE && __BANK
	if(NetworkDebug::IsTaskDebugStateLoggingEnabled()) 
	{
		Displayf("Task %p : %s : Ped %p %s : SendRequest %s", this, __FUNCTION__, GetEntity(), GetEntity() ? GetPed()->GetDebugName() : "NO PED", ms_ReactToFleeId.GetCStr()); 
	}
#endif /* TASK_REACT_AND_FLEE_TTY_DEBUG_ENABLE && __BANK*/

	CPed* pPed = GetPed();
	fragInstNMGta* pRagdollInst = pPed->GetRagdollInst();
	if (pRagdollInst != NULL)
	{
		// The initial velocity of the ragdoll will be calculated from the previous and current skeleton bounds.
		// This can be a problem when reacting to fleeing since the ped ducks down somewhat before
		// starting to run. If the character ragdolls as they are coming out of the duck they can end up inheriting 
		// quite a bit of upwards velocity (see bug #1928204). Trying to keep this change as local as possible so
		// only enabling this limit while reacting to fleeing

		// The next time the ragdoll is activated, the initial velocity will be scaled
		pRagdollInst->SetIncomingAnimVelocityScaleReset(sfReactRagdollIncomingAnimVelocityScale);

		taskWarningf("CTaskReactAndFlee::ReactToFlee_OnEnter: Scaling initial ragdoll velocity by %f", sfReactRagdollIncomingAnimVelocityScale);
	}

	//Send the react to flee request.
	m_MoveNetworkHelper.SendRequest(ms_ReactToFleeId);

	// Allow callers to force the transition animation to begin facing forward.
	if (GetConfigFlags().IsFlagSet(CTaskReactAndFlee::CF_ForceForwardReactToFlee))
	{
		ClearReactDirectionFlag();
		m_MoveNetworkHelper.SetFlag(true, ms_ReactFrontId);
	}
	
	//Generate the flee direction value.
	float fOriginalFleeDirectionValue = GenerateFleeDirectionValue();

	//Generate the flee direction.
	float fRoundedFleeDirectionValue = fOriginalFleeDirectionValue;
	m_nFleeDirection = GenerateDirectionFromValue(fRoundedFleeDirectionValue);

	//Check if we have chosen the same animation recently.
	static Direction	s_nLastReactDirection	= D_None;
	static Direction	s_nLastFleeDirection	= D_None;
	if(!m_uConfigFlags.IsFlagSet(CF_ForceForwardReactToFlee) && 
		GetPed()->GetIsOnScreen(true) &&
		((s_nLastReactDirection == m_nReactDirection) && (s_nLastFleeDirection == m_nFleeDirection))
		&& ((ms_uLastTimeReactedInDirection + (u32)(sm_Tunables.m_MinTimeToRepeatLastAnimation * 1000.0f)) > fwTimer::GetTimeInMilliseconds()))
	{
		if(fOriginalFleeDirectionValue >= fRoundedFleeDirectionValue)
		{
			fRoundedFleeDirectionValue = fOriginalFleeDirectionValue + 0.25f;
			if(fRoundedFleeDirectionValue > 1.0f)
			{
				fRoundedFleeDirectionValue -= 1.0f;
			}
		}
		else
		{
			fRoundedFleeDirectionValue = fOriginalFleeDirectionValue - 0.25f;
			if(fRoundedFleeDirectionValue < 0.0f)
			{
				fRoundedFleeDirectionValue += 1.0f;
			}
		}

		//Calculate the new flee direction.
		m_nFleeDirection = GenerateDirectionFromValue(fRoundedFleeDirectionValue);
	}

	//Set the last values.
	s_nLastReactDirection	= m_nReactDirection;
	s_nLastFleeDirection	= m_nFleeDirection;
	ms_uLastTimeReactedInDirection				= fwTimer::GetTimeInMilliseconds();
	
	//Set the flee direction.
	switch(m_nFleeDirection)
	{
		case D_Back:
		{
			m_MoveNetworkHelper.SetFlag(true, ms_FleeBackId);
			break;
		}
		case D_Left:
		{
			m_MoveNetworkHelper.SetFlag(true, ms_FleeLeftId);
			break;
		}
		case D_Front:
		{
			m_MoveNetworkHelper.SetFlag(true, ms_FleeFrontId);
			break;
		}
		case D_Right:
		{
			m_MoveNetworkHelper.SetFlag(true, ms_FleeRightId);
			break;
		}
		default:
		{
			taskAssertf(false, "Unknown direction: %d", m_nFleeDirection);
			break;
		}
	}
		
	//Wait for the blend out event.
	m_MoveNetworkHelper.WaitForTargetState(ms_BlendOutId);

	if (pPed && pPed->IsNetworkClone())
	{
		TryToScream();
	}
}
	
CTask::FSM_Return CTaskReactAndFlee::ReactToFlee_OnUpdate()
{
	TASK_REACT_AND_FLEE_TTY_FUNCTION_OUTPUT

	//Force the motion state.
	GetPed()->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Run);

	CPed* pPed = GetPed();

	// Prevent avoidance against other peds like this
	pPed->SetPedResetFlag( CPED_RESET_FLAG_MoveBlend_bFleeTaskRunning, true );

	//Grab the clip.
	const crClip* pClip = m_MoveNetworkHelper.GetClip(ms_ReactToFleeClipId);
	if(pClip)
	{
		//Get the phase.
		float fPhase = m_MoveNetworkHelper.GetFloat(ms_PhaseId);

		//Calculate the maximum amount of heading change for this frame.
		float fHeadingChangeRate = sm_Tunables.m_HeadingChangeRate;
#if __BANK
		TUNE_GROUP_BOOL(TASK_REACT_AND_FLEE, bDisableHeadingFixup, false);
		if(bDisableHeadingFixup)
		{
			fHeadingChangeRate = 0.0f;
		}
#endif 
		float fMaxHeadingChange = fHeadingChangeRate * GetTimeStep();

		if (Verifyf(!pPed->GetIsAttached(), "ReactToFlee_OnUpdate ped is attached.  In vehicle? %d Attach parent?  %s", pPed->GetIsInVehicle(), pPed->GetAttachParent() ? pPed->GetAttachParent()->GetModelName() : "NULL ATTACH PARENT"))
		{
			//Apply the heading fixup.
			CHeadingFixupHelper::Apply(*pPed, pPed->GetDesiredHeading(), *pClip, fPhase, fMaxHeadingChange);
		}

#if !__FINAL
		//This is a failsafe for the animations not being properly tagged with blend out events.
		{
			//Ensure the clip has the blend out event.
			const crTag* pTag = CClipEventTags::FindFirstEventTag(pClip, CClipEventTags::MoveEvent);
			if(!taskVerifyf(pTag, "No blend out event found on clip: %s.", pClip->GetName()))
			{
				//Check if a sufficient amount of time has passed.
				if(GetTimeInState() > 1.25f)
				{
					//Move to the flee state.
					TaskSetState(State_Flee);
				}
			}
		}
#endif
	}

	if(pPed->IsNetworkClone() && GetStateFromNetwork() == State_ReactToFlee)
	{
		return FSM_Continue;
	}
	
	// Probably expensive stuff
	Vector3 vToNextWpt = m_vNextWaypoint - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	vToNextWpt.z = 0.f;
	float fDistToNextSqr = vToNextWpt.XYMag2();
	vToNextWpt.Normalize();

	Vector3 vPrefDirToWpt = m_vNextWaypoint - m_vPrevWaypoint;
	vPrefDirToWpt.z = 0.f;
	vPrefDirToWpt.Normalize();	

	if(!IsFleeDirectionValid())
	{
		//Move to the flee state.
		m_fExitBlendDuration = SUPER_SLOW_BLEND_DURATION;
		TaskSetState(State_Flee);
	}
	else if (vToNextWpt.Dot(vPrefDirToWpt) < g_fMaxWptDot || fDistToNextSqr < g_fMaxWptDistanceSqr)
	{
		// Ok so we are either not running towards the wpt or we are too close to it
		// Blend duration might want to be in relation to how far we ran and close to the run pose we are
		m_fExitBlendDuration = SUPER_SLOW_BLEND_DURATION;
		TaskSetState(State_Flee);
	}
	else if(m_MoveNetworkHelper.IsInTargetState()) //Check if the target state has been reached.
	{
		//Move to the flee state.
		m_fExitBlendDuration = SUPER_SLOW_BLEND_DURATION;	//B*1862190: Increased blend duration from 0.5f to 1.0f.
		TaskSetState(State_Flee);
	}
	
	return FSM_Continue;
}

void CTaskReactAndFlee::ReactToFlee_OnExit()
{
	CPed* pPed = GetPed();

	fragInstNMGta* pRagdollInst = pPed->GetRagdollInst();
	if (pRagdollInst != NULL)
	{
		// Reset the incoming anim velocity scale
		pRagdollInst->SetIncomingAnimVelocityScaleReset(-1.0f);

		taskWarningf("CTaskReactAndFlee::ReactToFlee_OnExit: Reset initial ragdoll velocity");
	}
}

void CTaskReactAndFlee::Flee_OnEnter()
{
	TASK_REACT_AND_FLEE_TTY_FUNCTION_OUTPUT

	//Check if we are resuming the state.  Keep the sub-task, if possible.
	if(GetSubTask() && (GetSubTask()->GetTaskType() == CTaskTypes::TASK_SMART_FLEE))
	{
		return;
	}

	//Grab the ped.
	CPed* pPed = GetPed();

	//Check if the ped is not a clone.
	if(!pPed->IsNetworkClone())
	{
		//Check if the target is valid.
		if(m_Target.GetIsValid())
		{
			// Adjust target
			// TODO[egarcia]: Refactor. Create a CalculateSmartFleeTaskParams method and config smartflee task to update the move task.
			if (!m_CreateSmartFleeTaskParams.bValid)
			{
				AdjustMoveTaskTargetPositionByNearByPeds(pPed, m_pExistingMoveTask, true);
			}

			//Create the task.
			CTaskSmartFlee* pTask = rage_new CTaskSmartFlee(m_Target);

			if (m_CreateSmartFleeTaskParams.bValid)
			{
				pTask->SetIgnorePreferPavementsTimeLeft(m_CreateSmartFleeTaskParams.fIgnorePreferPavementsTimeLeft);
			}

			//Set the config flags.
			pTask->SetConfigFlags(m_uConfigFlagsForFlee);

			//Use the move task.
			//@@: location CTASKREACTANDFLEE_FLEEONENTER
			pTask->UseExistingMoveTask(m_pExistingMoveTask);
			m_pExistingMoveTask = NULL;

			// Set the move blend ratio.
			pTask->SetMoveBlendRatio(m_fMoveBlendRatio);

			//Set stop distance
			pTask->SetStopDistance(CalculateFleeStopDistance());

			//Quit the task if we are out of range.
			pTask->SetQuitTaskIfOutOfRange(true);

			//Set the ambient clips.
			if(m_hAmbientClips.IsNotNull())
			{
				pTask->SetAmbientClips(m_hAmbientClips);
			}

			//Consider run-starts for path look-ahead.
			pTask->SetConsiderRunStartForPathLookAhead(true);

			//Set the last target position.
			pTask->SetLastTargetPosition(m_vLastTargetPosition);

			//Start the task.
			SetNewTask(pTask);

			//
			m_CreateSmartFleeTaskParams.Clear();
		}
	}

	//Check if the move network is active.
	if(m_MoveNetworkHelper.IsNetworkActive())
	{
		//Clear the move network.
		GetPed()->GetMovePed().ClearTaskNetwork(m_MoveNetworkHelper, m_fExitBlendDuration, CMovePed::Task_TagSyncTransition);
	}
}

CTask::FSM_Return CTaskReactAndFlee::Flee_OnUpdate()
{
	TASK_REACT_AND_FLEE_TTY_FUNCTION_OUTPUT

	if (GetTimeInState() < m_fExitBlendDuration * 2.f)
		GetPed()->SetPedResetFlag(CPED_RESET_FLAG_UseTighterTurnSettings, true);

	//Check if the sub-task has finished.
	if(!GetSubTask() || GetIsSubtaskFinished(CTaskTypes::TASK_SMART_FLEE))
	{
		// After fleeing, go to a hurry away behavior for multiplayer ambient peds
		if (NetworkInterface::IsGameInProgress() && GetPed()->PopTypeIsRandom())
		{
			GivePedHurryAwayTask();
		}

		// Finish
		TaskSetState(State_Finish);
	} 

	return FSM_Continue;
}

// PURPOSE: Assign ped a hurry away task
void CTaskReactAndFlee::GivePedHurryAwayTask()
{
	CEventShockingSeenConfrontation event;

	Vector3 vTargetPosition;
	m_Target.GetPosition(vTargetPosition);
	// TODO[egarcia]: Add CAITarget with non const entity
	event.Set(VECTOR3_TO_VEC3V(vTargetPosition), const_cast<CEntity*>(m_Target.GetEntity()), GetPed());

	CTaskShockingEventHurryAway* pTask = rage_new CTaskShockingEventHurryAway(&event);
	pTask->SetShouldPlayShockingSpeech(false);
	pTask->SetGoDirectlyToWaitForPath(true);
	pTask->SetForcePavementNotRequired(true);

	TUNE_GROUP_FLOAT(TASK_REACT_AND_FLEE, fHurryAwayTimeMinVal, 10.0f, 0.0f, 1000.0f, 0.5f);
	TUNE_GROUP_FLOAT(TASK_REACT_AND_FLEE, fHurryAwayTimeMaxVal, 15.0f, 0.0f, 1000.0f, 0.5f);

	pTask->SetHurryAwayMaxTime(fwRandom::GetRandomNumberInRange(fHurryAwayTimeMinVal, fHurryAwayTimeMaxVal));

	CEventGivePedTask eventGivePedTask(PED_TASK_PRIORITY_EVENT_RESPONSE_NONTEMP, pTask);
	GetPed()->GetPedIntelligence()->AddEvent(eventGivePedTask);
}


CTask::FSM_Return CTaskReactAndFlee::Flee_Clone_OnUpdate()
{
	TASK_REACT_AND_FLEE_TTY_FUNCTION_OUTPUT

	// NOTE[egarcia]: We have already reacted, but we can now react again after we started running if we receive a threat from another player
	const State eNetworkState = static_cast<State>(GetStateFromNetwork());
	if  ((m_reactionCount != m_networkReactionCount) && (eNetworkState == State_React || eNetworkState == State_StreamReact))
	{
		// Reset complete reaction
		TaskSetState(State_Start);
		m_reactionCount = m_networkReactionCount;
	}

	return FSM_Continue;
}


CTask::FSM_Return CTaskReactAndFlee::StreamReactComplete_Clone_OnUpdate(void)
{
	TASK_REACT_AND_FLEE_TTY_FUNCTION_OUTPUT

	if (GetPed() && !GetPed()->IsNetworkClone())
	{
		TaskSetState(State_StreamReact);
		return FSM_Continue;
	}

/*
	// we sit and do nothing unil either
	// a) the state from network is State_Flee and m_reacted == false
	// b) the state from network is State_React *or* m_reacted == true

	if(GetStateFromNetwork() == State_Flee)
	{
		// if i've actually reacted first but i've been 
		// streaming so the owner has moved onto flee already....
		if(m_reacted)
		{
			TaskSetState(State_React);
		}
		else
		{
			TaskSetState(State_Flee);
		}
	}
	else if((GetStateFromNetwork() == State_React) || (m_reacted))
	{
		TaskSetState(State_React);
	}
*/

	// if the owner flees, we flee, ignoring that the owner may have reacted while we've been streaming in the anim
	// this means that we don't get a pop when the clone goes from react (network blender blocked) to flee (network blender on)
	// and the owner has already ran a large distance away from us....

	if(GetStateFromNetwork() == State_Flee)
	{
		TaskSetState(State_Flee);
	}
	else if(GetStateFromNetwork() == State_React)
	{
		TaskSetState(State_React);
	}

	return FSM_Continue;
}

int CTaskReactAndFlee::ChooseStateToResumeTo(bool& bKeepSubTask) const
{
	//Do not keep the sub-task.
	bKeepSubTask = false;

	//Check if the sub-task is valid.
	const CTask* pSubTask = GetSubTask();
	if(pSubTask)
	{
		//Check the task type.
		switch(pSubTask->GetTaskType())
		{
			case CTaskTypes::TASK_SMART_FLEE:
			{
				bKeepSubTask = true;
				break;
			}
			default:
			{
				break;
			}
		}
	}

	return State_Flee;
}

// Clear all the react direction flags for a new reaction flag to get set.
void CTaskReactAndFlee::ClearReactDirectionFlag()
{
	m_MoveNetworkHelper.SetFlag(false, ms_ReactFrontId);
	m_MoveNetworkHelper.SetFlag(false, ms_ReactLeftId);
	m_MoveNetworkHelper.SetFlag(false, ms_ReactBackId);
	m_MoveNetworkHelper.SetFlag(false, ms_ReactRightId);
}

float CTaskReactAndFlee::GenerateDirection(Vec3V_In vPos) const
{
	//Grab the ped.
	const CPed* pPed = GetPed();

	//Grab the ped position.
	Vec3V_ConstRef vCurPos = pPed->GetTransform().GetPosition();

	//Grab the current heading.
	float fHeading = fwAngle::LimitRadianAngleSafe(pPed->GetCurrentHeading());

	//Generate the heading from the ped to the event.
	float fEventHeading = fwAngle::LimitRadianAngleSafe(fwAngle::GetRadianAngleBetweenPoints(vPos.GetXf(), vPos.GetYf(), vCurPos.GetXf(), vCurPos.GetYf()));
	
	//Normalize the heading to [-1, 1].
	float fHeadingDelta = SubtractAngleShorter(fHeading, fEventHeading);
	fHeadingDelta = Clamp(fHeadingDelta / PI, -1.0f, 1.0f);
	
	//Normalize the heading to [0, 1].
	return ((fHeadingDelta * 0.5f) + 0.5f);
}

CTaskReactAndFlee::Direction CTaskReactAndFlee::GenerateDirectionFromValue(float& fValue) const
{
	taskAssertf(fValue >= 0.0f && fValue <= 1.0f, "Value is not normalized: %.2f.", fValue);
	//0.00, 1.00	- Back
	//0.25			- Left
	//0.50			- Front
	//0.75			- Right
	
	//Convert the value to the closest direction.
	fValue *= 4.0f;
	u32 uValue = (u32)rage::round(fValue);
	fValue = (float)uValue / 4.0f;
	
	//Convert the value to a direction.
	switch(uValue)
	{
		case 0:
		{
			return D_Back;
		}
		case 1:
		{
			return D_Left;
		}
		case 2:
		{
			return D_Front;
		}
		case 3:
		{
			return D_Right;
		}
		case 4:
		{
			return D_Back;
		}
		default:
		{
			taskAssertf(false, "No valid direction for value: %d.", uValue);
			return D_Back;
		}
	}
}

float CTaskReactAndFlee::GenerateFleeDirectionValue()
{
	//Check if the override direction is active.
	if(sm_Tunables.m_OverrideDirections)
	{
		return sm_Tunables.m_OverrideFleeDirection;
	}
	
	//Grab the first route position.
	Vec3V vPosition;
	if(!GetFirstRoutePosition(vPosition))
	{
		return 0.0f;
	}
	
	return GenerateDirection(vPosition);
}

float CTaskReactAndFlee::GenerateReactDirectionValue()
{
	//Check if the override direction is active.
	if(sm_Tunables.m_OverrideDirections)
	{
		return sm_Tunables.m_OverrideReactDirection;
	}

	//Grab the target position.
	Vec3V vTargetPosition;
	m_Target.GetPosition(RC_VECTOR3(vTargetPosition));

	return GenerateDirection(vTargetPosition);
}

bool CTaskReactAndFlee::GetFirstRoutePosition(Vec3V_InOut vPosition)
{
	//Get the nav-mesh task.
	CTaskMoveFollowNavMesh* pTask = GetNavMeshTask();
	if(!pTask)
	{
		return false;
	}
	
	return pTask->GetCurrentWaypoint(RC_VECTOR3(vPosition));
}

bool CTaskReactAndFlee::CanDoFullReactionAndGetWpt(Vector3& vPrevWaypoint, Vector3& vNextWaypoint)
{
	// So we basically check if the route is long enough and if the start and route matches
	// Since the start is what makes us chose a direction so therefor it must match the route
//	static const float fMaxOffAngle = rage::Cosf(3.f * DtoR);
	CTaskMoveFollowNavMesh* pTask = GetNavMeshTask();
	if (pTask && pTask->GetPreviousWaypoint(vPrevWaypoint) && pTask->GetCurrentWaypoint(vNextWaypoint))
	{
		// Could possibly also do some angle sanity checks between segments
		Vector3 vToEnd = vNextWaypoint - vPrevWaypoint;
		if (vToEnd.XYMag2() > g_fMinReactionDistanceSqr)
			return true;

	}
	
	return false;
}

CTaskMoveFollowNavMesh* CTaskReactAndFlee::GetNavMeshTask()
{
	//Ensure the active movement task is valid.
	CTask* pTask = GetPed()->GetPedIntelligence()->GetActiveMovementTask();
	if(!pTask)
	{
		return NULL;
	}

	//Ensure the type is valid.
	if(pTask->GetTaskType() != CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH)
	{
		return NULL;
	}

	return static_cast<CTaskMoveFollowNavMesh *>(pTask);
}

bool CTaskReactAndFlee::HasRoute()
{
	//Check if the first route position is valid.
	Vec3V vPosition;
	return GetFirstRoutePosition(vPosition);
}

bool CTaskReactAndFlee::IsFleeDirectionValid() const
{
	//Ensure the direction is valid.
	if(m_nFleeDirection == D_None)
	{
		return false;
	}
	
	return true;
}

bool CTaskReactAndFlee::IsOnTrain() const
{
	const CPhysical* pGround = GetPed()->GetGroundPhysical();
	if(!pGround)
	{
		return false;
	}

	if(!pGround->GetIsTypeVehicle())
	{
		return false;
	}
	const CVehicle* pVehicle = static_cast<const CVehicle *>(pGround);

	if(!pVehicle->InheritsFromTrain())
	{
		return false;
	}

	return true;
}

// PURPOSE:  Helper method for selecting a reaction clip from a conditional animations group.
// Returns CLIP_SET_ID_INVALID if no matching clipset could be found.
fwMvClipSetId CTaskReactAndFlee::PickAppropriateReactionClipSet(Vec3V_In vEventPos, const CConditionalAnimsGroup& rAnimGroup, const CPed& rPed)
{
	CScenarioCondition::sScenarioConditionData conditionData;
	conditionData.pPed = &rPed;
	conditionData.vAmbientEventPosition = vEventPos;

	// Walk through the clipsets in the conditional anim group.  Return the first that matches the conditions.
	int numAnims = rAnimGroup.GetNumAnims();
	for(int i = 0; i < numAnims; i++)
	{
		const CConditionalAnims* pConditionalAnims = rAnimGroup.GetAnims(i);
		if (pConditionalAnims)
		{
			const CConditionalAnims::ConditionalClipSetArray* pConditionalClipArray = pConditionalAnims->GetClipSetArray(CConditionalAnims::AT_PANIC_EXIT);
			const CConditionalClipSet* pConditionalClipSet = CConditionalAnims::ChooseClipSet(*pConditionalClipArray, conditionData);
			if (pConditionalClipSet)
			{
				return pConditionalClipSet->GetClipSetId();
			}
		}
	}
	return CLIP_SET_ID_INVALID;
}

// PURPOSE:  Return the appropriate reaction clip given a hash for the conditional anim group.
fwMvClipSetId CTaskReactAndFlee::PickAppropriateReactionClipSetFromHash(Vec3V_In vEventPos, atHashWithStringNotFinal conditionalGroupHash, const CPed& rPed)
{
	const CConditionalAnimsGroup* pReactGroup = CONDITIONALANIMSMGR.GetConditionalAnimsGroup(conditionalGroupHash);
	fwMvClipSetId reactClipSet = CLIP_SET_ID_INVALID;
	if(taskVerifyf(pReactGroup, "The react group: %s is invalid.", conditionalGroupHash.GetCStr()))
	{
		reactClipSet = CTaskReactAndFlee::PickAppropriateReactionClipSet(vEventPos, *pReactGroup, rPed);
	}

	return reactClipSet;
}

// PURPOSE:  Return the default gunfire reaction clipset for responding to an event with a given position.
fwMvClipSetId CTaskReactAndFlee::PickDefaultGunfireReactionClipSet(Vec3V_In vEventPos, const CPed& rPed, int iReactionNumber)
{
	atHashWithStringNotFinal reactionHash; 
	switch (iReactionNumber)
	{
	case 0:
		reactionHash = atHashWithStringNotFinal("REACTION_GUNFIRE_INTRO",0x18A1CC22);
		break;
	case 1:
		reactionHash = atHashWithStringNotFinal("REACTION_GUNFIRE_INTRO_V1",0x52479a6f);
		break;
	case 2:
		reactionHash = atHashWithStringNotFinal("REACTION_GUNFIRE_INTRO_V2",0x85057fea);
		break;
	default:
		reactionHash = atHashWithStringNotFinal("REACTION_GUNFIRE_INTRO",0x18A1CC22);
		break;
	}

	return PickAppropriateReactionClipSetFromHash(vEventPos, reactionHash, rPed);
}

// PURPOSE:  Return a milder reaction clipset for responding to an event with a given position.
fwMvClipSetId CTaskReactAndFlee::PickDefaultShockingEventReactionClipSet(Vec3V_In vEventPos, const CPed& rPed)
{
	return PickAppropriateReactionClipSetFromHash(vEventPos, atHashWithStringNotFinal("REACTION_SHOCKING_INTRO",0xFD04F075), rPed);
}

// PURPOSE: Notify task of a new GunAimedAt event
void CTaskReactAndFlee::OnGunAimedAt(const CEventGunAimedAt& rEvent)
{
	const CEntity* pEventSourceEntity = rEvent.GetSourceEntity();
	if (pEventSourceEntity && pEventSourceEntity != m_Target.GetEntity() && pEventSourceEntity->GetIsTypePed() && SafeCast(const CPed, pEventSourceEntity)->IsAPlayerPed())
	{
		// Update target
		m_Target.SetEntity(pEventSourceEntity);
		m_bTargetEntityWasInvalidated = false;

		// Update event info
		m_eIsResponseToEventType = static_cast<eEventType>(rEvent.GetEventType());

		// Request a reaction restart in the next tick
		m_restartReaction = true;
	}
}

// PURPOSE: Check if the target entity is valid (Eg: not a dead player)
bool CTaskReactAndFlee::IsValidTargetEntity(const CEntity* pTargetEntity) const
{
	bool bValid = true;

	// In multiplayer games, we invalidate dead players
	if (NetworkInterface::IsGameInProgress())
	{
		if (pTargetEntity && pTargetEntity->GetIsTypePed())
		{
			const CPed* pTargetPed = SafeCast(const CPed, pTargetEntity);
			bValid = !pTargetPed->IsPlayer() || !pTargetPed->IsDead();
		}
	}

	return bValid;
}

// PURPOSE: If the target entity is no longer valid, we have to stop updating info from it
void CTaskReactAndFlee::OnTargetEntityNoLongerValid()
{
	taskAssert(!IsCloseAll(m_vLastTargetPosition, Vec3V(V_ZERO), ScalarVFromF32(SMALL_FLOAT)));
	m_Target.SetPosition(VEC3V_TO_VECTOR3(m_vLastTargetPosition));
	m_bTargetEntityWasInvalidated = true;

	CTask* pSubTask = GetSubTask();
	CTaskSmartFlee* pFleeSubTask = pSubTask && (pSubTask->GetTaskType() == CTaskTypes::TASK_SMART_FLEE) ? SafeCast(CTaskSmartFlee, pSubTask) : NULL;
	if (pFleeSubTask)
	{
		pFleeSubTask->OnTargetEntityNoLongerValid();
	}
}

// PURPOSE: Restart reaction with the current target
void CTaskReactAndFlee::RestartReaction()
{
	taskAssert(m_restartReaction);

	// Allow new reaction
	m_uConfigFlags.ClearFlag(CF_DisableReact);
	m_reacted = false;

	// 
	IncReactionCount();

	// Reset task
	TaskSetState(State_Start);

	// Clean flag
	m_restartReaction = false;
}

void CTaskReactAndFlee::ProcessMoveTask()
{
	//Ensure we should process the move task.
	if(!ShouldProcessMoveTask())
	{
		return;
	}

	//Check if the existing move task is invalid.
	if(!m_pExistingMoveTask)
	{
		//Check if we should acquire the existing move task.
		if(m_uConfigFlags.IsFlagSet(CF_AcquireExistingMoveTask))
		{
			//Check if the existing move task is valid.
			CTask* pExistingMoveTask = GetPed()->GetPedIntelligence()->GetActiveMovementTask();
			if(pExistingMoveTask && (pExistingMoveTask->GetTaskType() == CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH))
			{
				//Set the existing move task.
				m_pExistingMoveTask = pExistingMoveTask;
			}
		}

		//Check if the existing move task is invalid.
		if(!m_pExistingMoveTask)
		{
			//Calculate the target position.
			Vec3V vTargetPosition;
			m_Target.GetPosition(RC_VECTOR3(vTargetPosition));

			//Create the move task.
			CTaskMoveFollowNavMesh* pTask = CTaskSmartFlee::CreateMoveTaskForFleeOnFoot(vTargetPosition);
			m_pExistingMoveTask = pTask;

			//Set the spheres of influence builder.
			pTask->SetInfluenceSphereBuilderFn(SpheresOfInfluenceBuilder);

			//Keep moving while waiting for a path.
			pTask->SetKeepMovingWhilstWaitingForPath(true);

			//Don't stand still at the end.
			pTask->SetDontStandStillAtEnd(true);

			// Make sure we keep to pavement if we prefer that
			if (GetPed()->GetPedIntelligence() && GetPed()->GetPedIntelligence()->GetFleeBehaviour().GetFleeFlags().IsFlagSet(CFleeBehaviour::BF_PreferPavements))
			{
				pTask->SetKeepToPavements(true);
			}

			//Consider run-starts for path look-ahead.
			pTask->SetConsiderRunStartForPathLookAhead(true);

			//Set the move blend ratio.
			pTask->SetMoveBlendRatio(m_fMoveBlendRatio);

			//Add the move task.
			GetPed()->GetPedIntelligence()->AddTaskMovement(pTask);

			//Set the owner.
			NOTFINAL_ONLY(pTask->GetMoveInterface()->SetOwner(this));
		}
	}

	//Keep the existing move task alive.
	m_pExistingMoveTask->GetMoveInterface()->SetCheckedThisFrame(true);
}


void CTaskReactAndFlee::AdjustMoveTaskTargetPositionByNearByPeds(const CPed* pPed, CTask* pMoveTask, bool bForceScanNearByPeds)
{
	if (!taskVerifyf(pPed && pMoveTask && (pMoveTask->GetTaskType() == CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH), "CTaskReactAndFlee::AdjustMoveTaskTargetPositionByNearByPeds: Invalid pPed or pMoveTask!"))
	{
		return;
	}

	//Adjust position
	CTaskMoveFollowNavMesh* pFollowNavMeshTask = SafeCast(CTaskMoveFollowNavMesh, pMoveTask);

	//Calculate the target position.
	Vec3V vTargetPosition;
	m_Target.GetPosition(RC_VECTOR3(vTargetPosition));

#if !__FINAL
	m_DEBUG_scFleeCrowdSize = ScalarV(V_ZERO);
	m_DEBUG_vFleePedInitPosition = pPed->GetTransform().GetPosition();
	m_DEBUG_vFleeTargetInitPosition = vTargetPosition;
#endif // !__FINAL

	ScalarV scCrowdNormalizedSize(V_ZERO);
	vTargetPosition = CTaskSmartFlee::AdjustTargetPosByNearByPeds(*pPed, vTargetPosition, m_Target.GetEntity(), bForceScanNearByPeds, &scCrowdNormalizedSize NOTFINAL_ONLY(, &m_DEBUG_vExitCrowdDir, &m_DEBUG_vWallCollisionPosition, &m_DEBUG_vWallCollisionNormal, &m_DEBUG_vBeforeWallCheckFleeAdjustedDir, &m_DEBUG_scFleeNearbyPedsTotalWeight, &m_DEBUG_vFleeNearbyPedsCenterOfMass, &m_DEBUG_aFleePositions, &m_DEBUG_aFleeWeights));
#if !__FINAL
	m_DEBUG_scFleeCrowdSize = scCrowdNormalizedSize;
	m_DEBUG_vFleeTargetAdjustedPosition = vTargetPosition;
#endif // !__FINAL

	// Make sure we keep to pavement if we prefer that
	bool bFleeingInCrowd = pPed->PopTypeIsRandom() &&  IsGreaterThanAll(scCrowdNormalizedSize, ScalarV(V_ZERO));
	if  (bFleeingInCrowd)
	{
		const CTask* pSubTask = GetSubTask();
		if (!pSubTask || pSubTask->GetTaskType() != CTaskTypes::TASK_SMART_FLEE)
		{
			m_CreateSmartFleeTaskParams.Set(sm_Tunables.m_IgnorePreferPavementsTimeWhenInCrowd);
		}

		pFollowNavMeshTask->SetKeepToPavements(sm_Tunables.m_IgnorePreferPavementsTimeWhenInCrowd < 0.0f);
		pFollowNavMeshTask->SetKeepMovingWhilstWaitingForPath(true); // Prevent them from stopping in the middle of the road while they recalculate the route. 
		pFollowNavMeshTask->SetTarget(pPed, vTargetPosition, true);
	}
}

void CTaskReactAndFlee::ProcessStreaming()
{
	TASK_REACT_AND_FLEE_TTY_FUNCTION_OUTPUT

	//Check if we should stream in the clip set for the react.
	if(ShouldStreamInClipSetForReact())
	{
		m_ReactClipSetRequestHelper.Request(m_ReactClipSetId);
	}
	else
	{
		m_ReactClipSetRequestHelper.Release();
	}

	//Check if we should stream in the clip set for the react to flee.
	if(ShouldStreamInClipSetForReactToFlee())
	{
		m_ReactToFleeClipSetRequestHelper.Request(m_ReactToFleeClipSetId);
	}
	else
	{
		m_ReactToFleeClipSetRequestHelper.Release();
	}
}

bool CTaskReactAndFlee::ShouldProcessMoveTask() const
{
	//Ensure the ped is not a clone.
	if(GetPed()->IsNetworkClone())
	{
		return false;
	}

	//Ensure we have not yet hit the flee state.
	if(GetState() >= State_Flee)
	{
		return false;
	}

	return true;
}

bool CTaskReactAndFlee::ShouldProcessTargets() const
{
	//Ensure we have not yet hit the flee state.
	if(GetState() >= State_Flee)
	{
		return false;
	}

	return true;
}

bool CTaskReactAndFlee::ShouldStreamInClipSetForReact() const
{
	TASK_REACT_AND_FLEE_TTY_FUNCTION_OUTPUT
	
	s32 iState = GetState();

	if(iState==State_StreamReactComplete_Clone)
	{	//The clones can be waiting in State_StreamReactComplete_Clone which is a higher value
		//than State_React but clones still need to keep hold of their streamed in anim 
		//so use previous state value here
		Assertf(GetPed()->IsNetworkClone(),"%s is not a clone",GetPed()->GetDebugName());

		iState = GetPreviousState();
	}

	//Ensure the state is valid.
	if(iState > State_React)
	{
		return false;
	}

	return true;
}

bool CTaskReactAndFlee::ShouldStreamInClipSetForReactToFlee() const
{
	TASK_REACT_AND_FLEE_TTY_FUNCTION_OUTPUT

	s32 iState = GetState();

	if(iState==State_StreamReactComplete_Clone)
	{	//The clones can be waiting in State_StreamReactComplete_Clone which is a higher value
		//than State_ReactToFlee but clones still need to keep hold of their streamed in anim 
		//so use previous state value here
		Assertf(GetPed()->IsNetworkClone(),"%s is not a clone",GetPed()->GetDebugName());


		iState = GetPreviousState();
	}

	//Ensure the state is valid.
	if(iState > State_ReactToFlee)
	{
		return false;
	}

	return true;
}

void CTaskReactAndFlee::UseExistingMoveTask(CTask* pTask)
{
	//Set the existing move task.
	m_pExistingMoveTask = pTask;
}

fwMvClipSetId CTaskReactAndFlee::GetClipSetForState() const
{
	TASK_REACT_AND_FLEE_TTY_FUNCTION_OUTPUT

	switch(GetState())
	{
		case State_React:
			taskAssert(m_ReactClipSetId != CLIP_SET_ID_INVALID);
			return m_ReactClipSetId;
			break;
		case State_ReactToFlee:
			taskAssert(m_ReactToFleeClipSetId != CLIP_SET_ID_INVALID);
			return m_ReactToFleeClipSetId;
			break;
		case State_Start:						
		case State_StreamReact:					
		case State_StreamReactToFlee:			
		case State_StreamReactComplete_Clone:	
		case State_Flee:
		case State_Finish:						
		default:
			Assertf(false, "ERROR : CTaskReactAndFlee::GetClipSetForState - unknown state?!");
			break;
	}

	return CLIP_SET_ID_INVALID;
}

void CTaskReactAndFlee::SetClipSetForState()
{
	//Assert that the move network is active.
	taskAssert(m_MoveNetworkHelper.IsNetworkActive());

	//Set the clip set.
	fwMvClipSetId clipSetId = GetClipSetForState();

#if TASK_REACT_AND_FLEE_TTY_DEBUG_ENABLE && __BANK
	if(NetworkDebug::IsTaskDebugStateLoggingEnabled())
	{
		Displayf("%s : State %s : Setting clipSet %s : Ped %p %s", __FUNCTION__, GetStaticStateName(GetState()), clipSetId.GetCStr(), GetEntity(), GetEntity() ? GetPed()->GetDebugName() : "NO PED");
	}
#endif /* TASK_REACT_AND_FLEE_TTY_DEBUG_ENABLE && __BANK */

	m_MoveNetworkHelper.SetClipSet(clipSetId);
}


void CTaskReactAndFlee::IncReactionCount()
{
	m_reactionCount = (m_reactionCount + 1) % (REACTION_COUNT_MAX_VALUE + 1);
}


void  CTaskReactAndFlee::TaskSetState(u32 const iState)
{
	TASK_REACT_AND_FLEE_TTY_SET_STATE_OUTPUT

	SetState(iState);
}

void CTaskReactAndFlee::Scream()
{
	//Assert that we can scream.
	taskAssertf(CanScream(), "We are not allowed to scream.");

	//Grab the ped.
	CPed* pPed = GetPed();

	//Check if there has been speech data set up for the event response
	CEvent* pEvent = GetPed()->GetPedIntelligence()->GetCurrentEvent();
	if(pEvent && pEvent->HasEditableResponse())
	{
		CEventEditableResponse* pEventEditable = static_cast<CEventEditableResponse*>(pEvent);
		if(pEventEditable->GetSpeechHash() != 0)
		{
			atHashString hLine(pEventEditable->GetSpeechHash());
			pPed->NewSay(hLine.GetHash());
			return;
		}
	}

	if(pEvent && pPed->GetSpeechAudioEntity())
	{
		const int iEventType = GetPed()->GetPedIntelligence()->GetCurrentEventType();
		pPed->GetSpeechAudioEntity()->Panic(iEventType, true);
	}


	//Generate the scream events.
	GenerateAIScreamEvents();
}

void CTaskReactAndFlee::TryToScream()
{
	//Ensure we can scream.
	if(!CanScream())
	{
		return;
	}

	//Scream.
	Scream();
}

bool CTaskReactAndFlee::CanScream() const
{
	//Ensure we can scream.
	/*if(m_uConfigFlags.IsFlagSet(CF_DontScream))
	{
		return false;
	}*/

	//Grab the ped.
	const CPed* pPed = GetPed();

	//Ensure the flee flag is set.
	if(!pPed->GetPedIntelligence()->GetFleeBehaviour().GetFleeFlags().IsFlagSet(CFleeBehaviour::BF_CanScream))
	{
		return false;
	}

	//Ensure the bravery flag is not set.
	//@@: location CTASKREACTANDFLEE_CANSCREAM_CHECK_BRAVERY
	if(pPed->CheckBraveryFlags(BF_DONT_SCREAM_ON_FLEE))
	{
		return false;
	}

	return true;
}

void CTaskReactAndFlee::GenerateAIScreamEvents()
{
	// Cache the ped.
	CPed* pPed = GetPed();

	// Check the event type.
	CEvent* pEvent = pPed->GetPedIntelligence()->GetCurrentEvent();
	if (pEvent && (pEvent->GetEventType() == EVENT_SHOCKING_SEEN_WEAPON_THREAT || pEvent->GetEventType() == EVENT_GUN_AIMED_AT))
	{
		// The source ped is the ped causing the problem that other peds need to run away from.
		CPed* pSourcePed = pEvent->GetSourcePed();

		if (pSourcePed)
		{
			// The victim is the one with the gun being pointed at them if EVENT_SHOCKING_SEEN_WEAPON_THREAT, but us if responding to EVENT_GUN_AIMED_AT.
			CPed* pVictim = pEvent->GetEventType() == EVENT_SHOCKING_SEEN_WEAPON_THREAT ? pEvent->GetTargetPed() : pPed;

			// Make sure the victim exists and the assaulter isn't friends with them.
			if (pVictim && !pSourcePed->GetPedIntelligence()->IsFriendlyWith(*pVictim))
			{
				CEventCrimeCryForHelp::CreateAndCommunicateEvent(pVictim, pSourcePed); 
			}
		}
	}
	else if (pPed && pPed->IsNetworkClone() && m_screamActive && m_Target.GetEntity())
	{
		const CPed* pTarget = static_cast<const CPed*>(m_Target.GetEntity());
		CEventCrimeCryForHelp::CreateAndCommunicateEvent(pPed, pTarget); 
	}
}

float CTaskReactAndFlee::CalculateFleeStopDistance() const
{
	float fStopDistance = -1.0f;

	// Calculate Flee Distance Ratio
	float fFleeDistanceRatio = fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
	const CEntity* pTargetEntity = m_Target.GetEntity();
	const CPed* pTargetPed = pTargetEntity && pTargetEntity->GetIsTypePed() ? SafeCast(const CPed, pTargetEntity) : NULL;
	if   (pTargetPed)
	{
		const CPed* pPed = GetPed();
		taskAssert(pPed);

		const CPedModelInfo::PersonalityThreatResponse* pPersonalityThreatResponse = CTaskThreatResponse::GetThreatResponse(*pPed, pTargetPed);
		if (pPersonalityThreatResponse)
		{
			static const float fFLEE_DISTANCE_PERSONALITY_WEIGHT = 0.5f;
			fFleeDistanceRatio = fFleeDistanceRatio * (1.0f - fFLEE_DISTANCE_PERSONALITY_WEIGHT) + pPersonalityThreatResponse->m_Action.m_Weights.m_Flee * fFLEE_DISTANCE_PERSONALITY_WEIGHT;
		}
	}

	if (!m_bTargetEntityWasInvalidated)
	{
		if (NetworkInterface::IsGameInProgress())
		{
			fStopDistance = Lerp(fFleeDistanceRatio, sm_Tunables.m_MPMinFleeStopDistance, sm_Tunables.m_MPMaxFleeStopDistance);
		}
		else
		{
			fStopDistance = Lerp(fFleeDistanceRatio, sm_Tunables.m_SPMinFleeStopDistance, sm_Tunables.m_SPMaxFleeStopDistance);
		}
	}
	else
	{
		fStopDistance = Lerp(fFleeDistanceRatio, sm_Tunables.m_TargetInvalidatedMinFleeStopDistance, sm_Tunables.m_TargetInvalidatedMaxFleeStopDistance);
	}
	
	return fStopDistance;
}


#if !__FINAL && DEBUG_DRAW

void CTaskReactAndFlee::DebugRender() const
{
	if(GetPed())
	{
#if __BANK

		static const u32 uTEXT_BUFFER_SIZE = 128;
		char szTextBufferSize[uTEXT_BUFFER_SIZE];


		TUNE_GROUP_BOOL(TASK_REACT_AND_FLEE, bShowAdjustFleePositionCalculations, true);
		if (bShowAdjustFleePositionCalculations)
		{
			grcDebugDraw::Line(m_DEBUG_vFleePedInitPosition, m_DEBUG_vFleeTargetInitPosition, Color_orange, Color_orange);
			grcDebugDraw::Line(m_DEBUG_vFleePedInitPosition, m_DEBUG_vFleeTargetAdjustedPosition, Color_red, Color_red);
			grcDebugDraw::Line(m_DEBUG_vFleeTargetInitPosition, m_DEBUG_vFleeTargetAdjustedPosition, Color_orange, Color_red);
			grcDebugDraw::Sphere(m_DEBUG_vFleeTargetInitPosition, 1.0f, Color_orange, false);
			grcDebugDraw::Sphere(m_DEBUG_vFleeTargetAdjustedPosition, 1.0f, Color_red, false);

			const u32 uNumFleePositions = m_DEBUG_aFleePositions.GetCount();
			taskAssert(m_DEBUG_aFleeWeights.GetCount() == (int)uNumFleePositions);
			for (u32 uPositionIdx = 0; uPositionIdx < uNumFleePositions; ++uPositionIdx)
			{
				const Vec3V& vPosition = m_DEBUG_aFleePositions[uPositionIdx];
				const ScalarV& scWeight = m_DEBUG_aFleeWeights[uPositionIdx];

				grcDebugDraw::Sphere(vPosition, scWeight.Getf() * 1.0f, Color_yellow, false);
				grcDebugDraw::Line(vPosition, m_DEBUG_vFleeNearbyPedsCenterOfMass, Color_yellow, Color_yellow);
			}
			if (uNumFleePositions > 0)
			{
				grcDebugDraw::Line(m_DEBUG_vFleePedInitPosition, m_DEBUG_vFleeNearbyPedsCenterOfMass, Color_yellow1, Color_yellow1);
				grcDebugDraw::Sphere(m_DEBUG_vFleeNearbyPedsCenterOfMass, 0.1f, Color_yellow1, true);

				snprintf(szTextBufferSize, uTEXT_BUFFER_SIZE, "Weight: %f", m_DEBUG_scFleeNearbyPedsTotalWeight.Getf());
				grcDebugDraw::Text(m_DEBUG_vFleeNearbyPedsCenterOfMass + Vec3V(V_Z_AXIS_WZERO), Color_yellow1, szTextBufferSize);
			}
		}
#endif // __BANK

		const Vec3V& vPedPosition = GetPed()->GetTransform().GetPosition();
		grcDebugDraw::Line(vPedPosition, m_DEBUG_vFleePedInitPosition, Color_blue, Color_blue);

		const Color32 colCrowd = IsGreaterThanAll(m_DEBUG_scFleeCrowdSize, ScalarV(V_ZERO)) ? Color_blue : Color_green;
		grcDebugDraw::Sphere(m_DEBUG_vFleePedInitPosition, 0.1f + m_DEBUG_scFleeCrowdSize.Getf() * 0.2f, colCrowd, true);
		snprintf(szTextBufferSize, uTEXT_BUFFER_SIZE, "CrowdSize: %f", m_DEBUG_scFleeCrowdSize.Getf());
		grcDebugDraw::Text(m_DEBUG_vFleePedInitPosition + Vec3V(V_Z_AXIS_WZERO), colCrowd, szTextBufferSize);

		const Vec3V vFleePosToPed = m_DEBUG_vFleePedInitPosition - m_DEBUG_vFleeTargetInitPosition;
		const Vec3V vFleeDir = NormalizeSafe(vFleePosToPed, Vec3V(V_ZERO));
		grcDebugDraw::Arrow(m_DEBUG_vFleePedInitPosition, m_DEBUG_vFleePedInitPosition + vFleeDir, 0.1f, Color_orange);

		const Vec3V vNearbyPedsPosToPed = m_DEBUG_vFleePedInitPosition - m_DEBUG_vFleeNearbyPedsCenterOfMass;
		const Vec3V vNearByPedsFleeDir = NormalizeSafe(vNearbyPedsPosToPed, Vec3V(V_ZERO));
		grcDebugDraw::Arrow(m_DEBUG_vFleePedInitPosition, m_DEBUG_vFleePedInitPosition + vNearByPedsFleeDir, 0.1f, Color_yellow);

		const Vec3V vFleeAdjustedPosToPed = m_DEBUG_vFleePedInitPosition - m_DEBUG_vFleeTargetAdjustedPosition;
		const Vec3V vFleeAdjustedDir = NormalizeSafe(vFleeAdjustedPosToPed, Vec3V(V_ZERO));
		grcDebugDraw::Arrow(m_DEBUG_vFleePedInitPosition, m_DEBUG_vFleePedInitPosition + vFleeAdjustedDir, 0.1f, Color_red);

		grcDebugDraw::Arrow(m_DEBUG_vFleePedInitPosition, m_DEBUG_vFleePedInitPosition + m_DEBUG_vExitCrowdDir, 0.1f, Color_green);

		if (!IsZeroAll(m_DEBUG_vWallCollisionPosition))
		{
			grcDebugDraw::Sphere(m_DEBUG_vWallCollisionPosition, 0.1f, Color_pink, true);
			grcDebugDraw::Arrow(m_DEBUG_vWallCollisionPosition, m_DEBUG_vWallCollisionPosition + m_DEBUG_vWallCollisionNormal, 0.1f, Color_pink);
			grcDebugDraw::Arrow(m_DEBUG_vFleePedInitPosition, m_DEBUG_vFleePedInitPosition + m_DEBUG_vBeforeWallCheckFleeAdjustedDir, 0.1f, Color_pink);
		}

		//Grab the target position.
		//Vector3 vTargetPosition;
		//m_Target.GetPosition(vTargetPosition);

		//char buffer[1024];
		//sprintf(buffer, "Flee Direction = %d\nReact Direction = %d\nControl flags %d\nAmbientClipHash %d\nEventPos %f %f %f\nRate %f\nReactClipSetId %d\nReactToFleeClipSetId %d", 
		//					m_nFleeDirection,
		//					m_nReactDirection,				
		//					m_uConfigFlags.GetAllFlags(),
		//					m_hAmbientClips.GetHash(),
		//					vTargetPosition.x, vTargetPosition.y, vTargetPosition.z,
		//					m_fRate,
		//					m_ReactClipSetId.GetHash(),
		//					m_ReactToFleeClipSetId.GetHash());

		//grcDebugDraw::Text(GetPed()->GetTransform().GetPosition(), Color_white, buffer);

		//if(GetSubTask())
		//{
		//	Assert(GetSubTask()->GetTaskType() == CTaskTypes::TASK_SMART_FLEE);
		//	
		//	grcDebugDraw::Sphere(GetPed()->GetTransform().GetPosition(), 2.0f, Color_red, false);

		//	GetSubTask()->Debug();
		//}
	}
}

#endif /* !__FINAL && DEBUG_DRAW*/
