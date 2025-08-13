//----------------------------------------------------------------------------------
// CTaskCowerScenario
// When a ped must respond to an event but stay in one place due to navmesh/script
// constraints).
//----------------------------------------------------------------------------------

#include "Task/Scenario/Types/TaskCowerScenario.h"

//RAGE INCLUDES
#include "fwmaths/angle.h"
#include "math/angmath.h"

//GAME INCLUDES
#include "animation/Move.h"
#include "animation/MovePed.h"
#include "control/replay/Replay.h"
#include "ik/IkManager.h"
#include "peds/Ped.h"
#include "peds/PedIntelligence.h"
#include "peds/PedIKSettings.h"
#include "task/Movement/TaskCollisionResponse.h"
#include "task/Physics/TaskNM.h"
#include "task/Response/TaskFlee.h"
#include "Task/Response/TaskReactAndFlee.h"
#include "Task/Scenario/Types/TaskUseScenario.h"
#include "weapons/WeaponTypes.h"
#include "weapons/inventory/PedWeaponManager.h"

CTaskCowerScenario::Tunables CTaskCowerScenario::sm_Tunables;
IMPLEMENT_SCENARIO_TASK_TUNABLES(CTaskCowerScenario, 0xa28a1054);

AI_OPTIMISATIONS()

const fwMvStateId CTaskCowerScenario::ms_State_Base("Base",0x44e21c90);

const fwMvRequestId CTaskCowerScenario::ms_PlayBaseIntroRequest("PlayBaseIntroRequest",0xEFC34923);
const fwMvRequestId CTaskCowerScenario::ms_PlayDirectedIntroRequest("PlayDirectedIntroRequest",0x2EC7CF9F);
const fwMvRequestId	CTaskCowerScenario::ms_PlayBaseRequest("PlayBaseRequest",0x7F61BB38);
const fwMvRequestId	CTaskCowerScenario::ms_PlayVariationRequest("PlayVariationRequest",0x3FBF5251);
const fwMvRequestId CTaskCowerScenario::ms_PlayFlinchRequest("PlayFlinchRequest",0x1A9697B4);
const fwMvRequestId	CTaskCowerScenario::ms_PlayBaseOutroRequest("PlayBaseOutroRequest",0x99AA350A);
const fwMvRequestId CTaskCowerScenario::ms_PlayDirectedOutroRequest("PlayDirectedOutroRequest",0x6B3E0BD1);

const fwMvBooleanId CTaskCowerScenario::ms_IntroDoneId("IntroEnded",0xC8D01D14);
const fwMvBooleanId CTaskCowerScenario::ms_OutroDoneId("OutroEnded",0xBAACD4C5);
const fwMvBooleanId CTaskCowerScenario::ms_VariationDoneId("VariationEnded",0x195EB20D);
const fwMvBooleanId CTaskCowerScenario::ms_FlinchDoneId("FlinchEnded",0x216ABBAF);

const fwMvBooleanId CTaskCowerScenario::ms_OnEnterIntroId("OnEnterIntro",0xE312777C);
const fwMvBooleanId CTaskCowerScenario::ms_OnEnterBaseId("OnEnterBase",0xCF4C2726);
const fwMvBooleanId CTaskCowerScenario::ms_OnEnterDirectedBaseId("OnEnterDirectedBase",0xA7F3EC41);
const fwMvBooleanId CTaskCowerScenario::ms_OnEnterOutroId("OnEnterOutro",0x4CBE6874);
const fwMvBooleanId CTaskCowerScenario::ms_OnEnterVariationId("OnEnterVariation",0x9A76B6FB);

const fwMvClipId CTaskCowerScenario::ms_IntroClipId("IntroClipId",0xB44BF129);
const fwMvClipId CTaskCowerScenario::ms_BaseClipId("BaseClipId",0x572ACC4B);
const fwMvClipId CTaskCowerScenario::ms_DirectedForwardTransClipId("DirectedForwardTransClipId",0xCF16F2A9);
const fwMvClipId CTaskCowerScenario::ms_DirectedLeftTransClipId("DirectedLeftTransClipId",0xD819F84E);
const fwMvClipId CTaskCowerScenario::ms_DirectedRightTransClipId("DirectedRightTransClipId",0xEC5FAE9);
const fwMvClipId CTaskCowerScenario::ms_DirectedBackLeftTransClipId("DirectedBackLeftTransClipId",0x1250356C);
const fwMvClipId CTaskCowerScenario::ms_DirectedBackRightTransClipId("DirectedBackRightTransClipId",0x3A59C3B8);
const fwMvClipId CTaskCowerScenario::ms_DirectedForwardBaseClipId("DirectedForwardBaseClipId",0xF30BA6A7);
const fwMvClipId CTaskCowerScenario::ms_DirectedLeftBaseClipId("DirectedLeftBaseClipId",0xB8F071C9);
const fwMvClipId CTaskCowerScenario::ms_DirectedBackLeftBaseClipId("DirectedBackLeftBaseClipId",0x5ABFC97F);
const fwMvClipId CTaskCowerScenario::ms_DirectedRightBaseClipId("DirectedRightBaseClipId",0xCE7CC38D);
const fwMvClipId CTaskCowerScenario::ms_DirectedBackRightBaseClipId("DirectedBackRightBaseClipId",0x9F9277C5);
const fwMvClipId CTaskCowerScenario::ms_VariationClipId("VariationClipId",0x3285F89F);
const fwMvClipId CTaskCowerScenario::ms_FlinchClipId("FlinchClipId",0xC72094F);
const fwMvClipId CTaskCowerScenario::ms_OutroClipId("OutroClipId",0x1E5B7D7D);

const fwMvFloatId CTaskCowerScenario::ms_TargetHeadingId("TargetAngle",0x15188CF);

const fwMvFloatId CTaskCowerScenario::ms_RateId("RateId",0xF11C4C9B);

float CTaskCowerScenario::sm_fLastCowerRate = 0.0f;
u32 CTaskCowerScenario::sm_uLastTimeAnyoneFlinched = 0;


CTaskCowerScenario::CTaskCowerScenario(s32 iScenarioIndex, CScenarioPoint* pScenarioPoint, const CConditionalAnims* pConditionalAnims, const CAITarget& target, s16 iInitialEventType, s16 iInitialEventPriority)
: CTaskScenario(iScenarioIndex, pScenarioPoint, false)
, m_pConditionalAnims(pConditionalAnims)
, m_Target(target)
, m_FlinchIgnoreEvent(NULL)
, m_iCurrentEventType(iInitialEventType)
, m_iCurrentEventPriority(iInitialEventPriority)
, m_fApproachDirection(0.0f)
, m_fCowerTime(0.0f)
, m_fVariationTimer(0.0f)
, m_uTimeDenyTargetChange(0)
, m_BaseClipSetId(CLIP_SET_ID_INVALID)
, m_OtherClipSetId(CLIP_SET_ID_INVALID)
, m_bSwitchingBetweenCowerTypes(false)
, m_bApproaching(false)
, m_bCowerForever(false)
, m_bDoPanicExit(false)
, m_bHasAVariationAnimation(false)
, m_bHasDirectedBaseAnimations(false)
, m_bQuitTaskOnEvent(false)
, m_bHasAFlinchAnimation(false)
, m_bEventTriggersFlinch(false)
, m_bFullyInVariationState(false)
, m_bMoveInTargetState(false)
, m_bOnlyDoUndirectedExit(false)
, m_bSkipExitDistanceCheck(false)
, m_bMigrateDontClearMoveNetwork(false)
, m_bMigrateMoveNetworkForceStateChange(false)
{
	SetInternalTaskType(CTaskTypes::TASK_COWER_SCENARIO);
}

CTaskCowerScenario::~CTaskCowerScenario()
{

}
aiTask* CTaskCowerScenario::Copy() const
{
	return rage_new CTaskCowerScenario(GetScenarioType(), GetScenarioPoint(), m_pConditionalAnims, m_Target, m_iCurrentEventType, m_iCurrentEventPriority);
}


CTask::FSM_Return CTaskCowerScenario::ProcessPreFSM()
{
	if( !m_bCowerForever )
	{
		//Decrease the cower timer.
		m_fCowerTime -= GetTimeStep();
	}

	// If the ped didn't flinch soon enough after an event then forget about it.
	if (m_bEventTriggersFlinch)
	{
		if (fwTimer::GetTimeInMilliseconds() - m_uTimeOfCowerTrigger > sm_Tunables.m_FlinchDecayTime)
		{
			m_bEventTriggersFlinch = false;
		}
	}
	
	UpdateHeadIK();

	CPed* pPed = GetPed();
	if( pPed )
	{
		PossiblyDestroyPedCoweringOffscreen(pPed, m_OffscreenDeletePedTimer);
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskCowerScenario::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_StreamIntro)
			FSM_OnUpdate
				return StreamIntro_OnUpdate();

		FSM_State(State_Intro)
			FSM_OnEnter
				Intro_OnEnter();
			FSM_OnUpdate
				return Intro_OnUpdate();
			
		FSM_State(State_DirectedIntro)
			FSM_OnEnter
				DirectedIntro_OnEnter();
			FSM_OnUpdate
				return DirectedIntro_OnUpdate();

		FSM_State(State_Base)
			FSM_OnEnter
				Base_OnEnter();
			FSM_OnUpdate
				return Base_OnUpdate();

		FSM_State(State_DirectedBase)
			FSM_OnEnter
				DirectedBase_OnEnter();
			FSM_OnUpdate
				return DirectedBase_OnUpdate();

		FSM_State(State_StreamVariation)
			FSM_OnEnter
				StreamVariation_OnEnter();
			FSM_OnUpdate
				return StreamVariation_OnUpdate();

		FSM_State(State_Variation)
			FSM_OnEnter
				Variation_OnEnter();
			FSM_OnUpdate
				return Variation_OnUpdate();
			FSM_OnExit
				Variation_OnExit();

		FSM_State(State_StreamFlinch)
			FSM_OnEnter
				StreamFlinch_OnEnter();
			FSM_OnUpdate
				return StreamFlinch_OnUpdate();

		FSM_State(State_Flinch)
			FSM_OnEnter
				Flinch_OnEnter();
			FSM_OnUpdate
				return Flinch_OnUpdate();

		FSM_State(State_Outro)
			FSM_OnEnter
				Outro_OnEnter();
			FSM_OnUpdate
				return Outro_OnUpdate();

		FSM_State(State_DirectedOutro)
			FSM_OnEnter
				DirectedOutro_OnEnter();
			FSM_OnUpdate
				return DirectedOutro_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

// Called every frame so the base clip does not stream out.
CTask::FSM_Return CTaskCowerScenario::ProcessPostFSM()
{
	KeepStreamingBaseClip();

	return FSM_Continue;
}

bool CTaskCowerScenario::ProcessMoveSignals()
{
	s32 iState = GetState();

	switch(iState)
	{
		case State_Intro:
		{
			Intro_OnProcessMoveSignals();
			return true;
		}
		case State_DirectedIntro:
		{
			UpdateDirectedCowerBlend();
			DirectedIntro_OnProcessMoveSignals();
			return true;
		}
		case State_DirectedBase:
		{
			UpdateDirectedCowerBlend();
			DirectedBase_OnProcessMoveSignals();
			return true;
		}
		case State_Base:
		{
			Base_OnProcessMoveSignals();
			return true;
		}
		case State_Flinch:
		{
			Flinch_OnProcessMoveSignals();
			return true;
		}
		case State_Variation:
		{
			Variation_OnProcessMoveSignals();
			return true;
		}
		case State_Outro:
		{
			Outro_OnProcessMoveSignals();
			return true;
		}
		case State_DirectedOutro:
		{
			UpdateDirectedCowerBlend();
			DirectedOutro_OnProcessMoveSignals();
			return true;
		}
		default:
		{
			return false;
		}
	}
}

// Abort only for immediate priority, otherwise the event is consumed and adds to the cower timer.
// Depending on the event, the type of cower can be changed.
bool CTaskCowerScenario::ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent)
{
	if (iPriority == ABORT_PRIORITY_IMMEDIATE)
	{
		return true;
	}

	if (pEvent)
	{
		const CEvent* pEEvent = (const CEvent*)pEvent;
		if( pEEvent->RequiresAbortForMelee( GetPed() ) )
			return true;

		// Check the event to see if it is important enough to kill this task.
		if (ShouldStopCowering(*pEvent))
		{
			// Decide on how to kill this task.
			StopCowering(*pEvent);
		}
		else
		{
			if (ShouldUpdateCowerStatus(*pEvent))
			{
				// Handle the event.
				UpdateCowerStatus(*pEvent);
			}

			// Make the event expire.
			static_cast<CEvent*>(const_cast<aiEvent*>(pEvent))->Tick();
		}
	}
	else
	{
		TriggerNormalExit();
	}

	return m_bQuitTaskOnEvent;
}

CTaskInfo* CTaskCowerScenario::CreateQueriableState() const
{
	return rage_new CClonedCowerScenarioInfo(GetState(), static_cast<s16>(GetScenarioType()), GetScenarioPoint(), GetIsWorldScenario(), GetWorldScenarioPointIndex());
}

void CTaskCowerScenario::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	CTaskScenario::ReadQueriableState(pTaskInfo);
}

void CTaskCowerScenario::OnCloneTaskNoLongerRunningOnOwner()
{
    SetStateFromNetwork(State_Finish);
}

aiTask::FSM_Return CTaskCowerScenario::UpdateClonedFSM(const s32 iState, const FSM_Event iEvent)
{
	if(GetStateFromNetwork() == State_Finish)
	{
		return FSM_Quit;
	}

	if( (iEvent == OnUpdate) && GetIsWaitingForEntity() )
	{
		return FSM_Continue;	
	}

	return UpdateFSM(iState,iEvent);
}

bool CTaskCowerScenario::IsInScope(const CPed* pPed)
{
    return CTaskFSMClone::IsInScope(pPed);
}

// Choose the initial cowering state based on the kind of event being responded to.
CTask::FSM_Return CTaskCowerScenario::Start_OnUpdate()
{
	CPed* pPed = GetPed();

	// Cache off what animations are supported by this cower scenario.
	CacheOffAnimationSupport();

	// Cache off the initial position of the target.
	Vector3 vTarget;
	if (m_Target.GetIsValid())
	{
		m_Target.GetPosition(vTarget);
	}
	m_vTargetOriginalPosition = VECTOR3_TO_VEC3V(vTarget);

	// Decide what kind of cowering to do to the event.
	HandleEventType(m_iCurrentEventType, m_iCurrentEventPriority);

	// Build the condition struct to select an appropriate animation.
	CScenarioCondition::sScenarioConditionData conditionData;
	conditionData.pPed = pPed;
	if (m_bPlayDirectedCower)
	{
		//Set up data for directed cower intro.
		conditionData.eAmbientEventType = AET_Directed_In_Place;
	}
	else
	{
		//Set up data for undirected cower intro.
		conditionData.eAmbientEventType = AET_In_Place;
	}
	
	// Pick the intro clip.
	m_OtherClipSetId = ChooseConditionalClipSet(CConditionalAnims::AT_PANIC_INTRO, conditionData);

	// Pick the base clip.
	m_BaseClipSetId = ChooseConditionalClipSet(CConditionalAnims::AT_PANIC_BASE, conditionData);

	if(NetworkInterface::IsNetworkOpen())
	{
		if(NetworkCheckSetMigrateStateAndClips())
		{
			return FSM_Continue;
		}
	}

	// Let go of whatever you're holding so it doesn't clip.
	DropProps();

	// Scream.
	PlayInitialAudio();

	// Proceed to stream in the selected clip.
	SetState(State_StreamIntro);
	return FSM_Continue;
}


bool CTaskCowerScenario::NetworkCheckSetMigrateStateAndClips()
{
	CPed* pPed = GetPed();

	if(taskVerifyf(pPed,"Expect valid ped") && taskVerifyf(NetworkInterface::IsNetworkOpen(),"Only used in network games"))
	{
		bool bTaskUseScenarioWithCower = false;
		bool bTryToForceStateChange = false;
		CTaskReactAndFlee* pTaskReactAndFlee = static_cast<CTaskReactAndFlee *>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_REACT_AND_FLEE));

		if (pTaskReactAndFlee && pTaskReactAndFlee->GetIncomingMigratingCower())
		{
			bTryToForceStateChange = true;
		}

		if (!bTryToForceStateChange)
		{
			CTaskUseScenario* pTaskUseScenario = static_cast<CTaskUseScenario*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_USE_SCENARIO));
			if (pTaskUseScenario && pTaskUseScenario->GetMigratingCowerStartingTask())
			{
				bTaskUseScenarioWithCower = true;
				bTryToForceStateChange = true;
			}

			CTaskSmartFlee* pTaskSmartFlee = static_cast<CTaskSmartFlee *>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_SMART_FLEE));

			if (pTaskSmartFlee && pTaskSmartFlee->GetIncomingMigratingCower())
			{
				bTryToForceStateChange = true;
			}

			CTaskCower* pTaskCower = static_cast<CTaskCower *>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COWER));

			if (pTaskCower && pTaskCower->ShouldForceStateChangeOnMigrate())
			{
				bTryToForceStateChange = true;
			}
		}

		if(bTryToForceStateChange)
		{
			bool bNetworkIsStreamedIn = fwAnimDirector::RequestNetworkDef(CClipNetworkMoveInfo::ms_NetworkTaskCowerScenario);
			m_MoveNetworkHelper.CreateNetworkPlayer(pPed, CClipNetworkMoveInfo::ms_NetworkTaskCowerScenario);
			pPed->GetMovePed().SetTaskNetwork(m_MoveNetworkHelper.GetNetworkPlayer(), SLOW_BLEND_DURATION);
			m_bMigrateMoveNetworkForceStateChange = true;
			bool bBaseStreamed	= m_BaseClipSetRequestHelper.Request(m_BaseClipSetId);

			if(!bBaseStreamed && bTaskUseScenarioWithCower)
			{
				//This can happen when a clone comes into scope and has
				//the TaskUseScenario with cower sub task
				return false;
			}

			if(taskVerifyf(bNetworkIsStreamedIn,"Expect network") && bBaseStreamed) //fully possible that the bBaseStreamed hasn't happened yet as this is a request - might need to wait for it to become available
			{
				SetState(CTaskCowerScenario::State_Base);

				return true;
			}
		}
	}

	return false;
}

// Block until the animation is streamed in.
// If this task is run from a scenario like a chair where we don't want the ped popping into the idle pose while this streams,
// then the panic animation clipset should be streamed before this task is run.
CTask::FSM_Return CTaskCowerScenario::StreamIntro_OnUpdate()
{
	// Request the intro clip.
	m_OtherClipSetRequestHelper.Request(m_OtherClipSetId);

	bool bNetworkIsStreamedIn = fwAnimDirector::RequestNetworkDef(CClipNetworkMoveInfo::ms_NetworkTaskCowerScenario);
	bool bClipIsLoaded = m_OtherClipSetRequestHelper.IsLoaded();

	if (bNetworkIsStreamedIn && bClipIsLoaded)
	{
		// Create and blend in the move network.
		CPed* pPed = GetPed();
		m_MoveNetworkHelper.CreateNetworkPlayer(pPed, CClipNetworkMoveInfo::ms_NetworkTaskCowerScenario);
		pPed->GetMovePed().SetTaskNetwork(m_MoveNetworkHelper.GetNetworkPlayer(), SLOW_BLEND_DURATION);

		if (m_bPlayDirectedCower)
		{
			SetState(State_DirectedIntro);
		}
		else
		{
			SetState(State_Intro);
		}
	}
	return FSM_Continue;
}

// Undirected cower intro.
void CTaskCowerScenario::Intro_OnEnter()
{
	// Send clip information to MoVE
	fwMvClipId clip;
	ChooseClip(m_OtherClipSetId, clip, false);
	SendClipInformation(m_OtherClipSetId, clip, ms_IntroClipId);

	// Tell MoVE to play the clip.
	m_MoveNetworkHelper.SendRequest(ms_PlayBaseIntroRequest);
	m_MoveNetworkHelper.WaitForTargetState(ms_IntroDoneId);

	// Reset the cached state variables.
	m_bMoveInTargetState = false;

	// Request signals to be sent back to the task from MoVE.
	RequestProcessMoveSignalCalls();
}

// Continue playing the undirected cower intro until it finishes.
CTask::FSM_Return CTaskCowerScenario::Intro_OnUpdate()
{
	bool bClipIsLoaded = m_BaseClipSetRequestHelper.IsLoaded();
	if (bClipIsLoaded && m_bMoveInTargetState)
	{
		// Proceed to playing the undirected cower base anims.
		SetState(State_Base);
	}
	return FSM_Continue;
}

void CTaskCowerScenario::Intro_OnProcessMoveSignals()
{
	// Cache off signals sent from MoVE.
	m_bMoveInTargetState |= m_MoveNetworkHelper.IsInTargetState();
}

// Directed cower intro.
void CTaskCowerScenario::DirectedIntro_OnEnter()
{
	// Tell MoVE which clip to play.
	SendDirectedClipInformation(m_OtherClipSetId);

	// Tell MoVE to play the clip.
	m_MoveNetworkHelper.SendRequest(ms_PlayDirectedIntroRequest);
	m_MoveNetworkHelper.WaitForTargetState(ms_IntroDoneId);

	// Reset the cached state variables.
	m_bMoveInTargetState = false;

	// Request signals to be sent back to the task from MoVE.
	RequestProcessMoveSignalCalls();

	m_fLastDirection = GetAngleToTarget();
	m_bApproaching = false;

	UpdateDirectedCowerBlend();
}

// Continue playing the directed cower intro until it finishes.
CTask::FSM_Return CTaskCowerScenario::DirectedIntro_OnUpdate()
{
	bool bClipIsLoaded = m_BaseClipSetRequestHelper.IsLoaded();

	if (bClipIsLoaded && m_bMoveInTargetState)
	{
		// Proceed to playing the directed base anims.
		SetState(State_DirectedBase);
	}
	return FSM_Continue;
}

void CTaskCowerScenario::DirectedIntro_OnProcessMoveSignals()
{
	// Cache off signals sent from MoVE.
	m_bMoveInTargetState |= m_MoveNetworkHelper.IsInTargetState();
}

// Undirected base animations.
void CTaskCowerScenario::Base_OnEnter()
{
	// The ped is no longer switching between cower types.
	m_bSwitchingBetweenCowerTypes = false;

	// Any events that trigger flincing won't be respected until fully in the state.
	m_bEventTriggersFlinch = false;

	// Send the clip information to MoVE.
	fwMvClipId clip;
	ChooseClip(m_BaseClipSetId, clip, false);
	SendClipInformation(m_BaseClipSetId, clip, ms_BaseClipId);

	// Tell MoVE to play the clip.
	if(NetworkInterface::IsNetworkOpen() && m_bMigrateMoveNetworkForceStateChange)
	{
		m_bMigrateMoveNetworkForceStateChange = false; //clear this once first used after migrate
		m_MoveNetworkHelper.ForceStateChange(ms_State_Base);
	}
	else
	{
		m_MoveNetworkHelper.SendRequest(ms_PlayBaseRequest);
	}
	m_MoveNetworkHelper.WaitForTargetState(ms_OnEnterBaseId);

	// Reset the cached state variables.
	m_bMoveInTargetState = false;

	// Request signals to be sent back to the task from MoVE.
	RequestProcessMoveSignalCalls();

	// Select the next clip in the sequence.
	CScenarioCondition::sScenarioConditionData conditionData;
	conditionData.pPed = GetPed();
	conditionData.eAmbientEventType = AET_In_Place;
	m_OtherClipSetId = ChooseConditionalClipSet(CConditionalAnims::AT_PANIC_OUTRO, conditionData);

	SetTimeUntilNextVariation();
}

// Continue playing the cower animation until time expires, or a change in the event necessitates a switch to directed cowering.
CTask::FSM_Return CTaskCowerScenario::Base_OnUpdate()
{
	if (!m_bMoveInTargetState)
	{
		return FSM_Continue;
	}

	// If nothing has happened in a while and the target got close by then switch to directed.
	CheckForDirectedCowerSwitchDueToDistance();

	// If we are not currently switching to a new form of cowering and a switch is desired, select a suitable directed cowering clip.
	if (m_bPlayDirectedCower && !m_bSwitchingBetweenCowerTypes)
	{
		m_bSwitchingBetweenCowerTypes = true;
		CScenarioCondition::sScenarioConditionData conditionData;
		conditionData.pPed = GetPed();
		conditionData.eAmbientEventType = AET_Directed_In_Place;
		Vector3 vPosition;
		m_Target.GetPosition(vPosition);
		m_BaseClipSetId = ChooseConditionalClipSet(CConditionalAnims::AT_PANIC_BASE, conditionData);

		// Reset the request helper so it doesn't still think we are streaming in the directed base clip.
		m_BaseClipSetRequestHelper.Request(m_BaseClipSetId);
	}
	else if (m_bDoPanicExit)
	{
		// Switch to the panic exit clip.
		CScenarioCondition::sScenarioConditionData conditionData;
		conditionData.pPed = GetPed();
		conditionData.eAmbientEventType = AET_In_Place;
		m_OtherClipSetId = ChooseConditionalClipSet(CConditionalAnims::AT_PANIC_EXIT, conditionData);
	}
	else if (ShouldPlayAFlinch())
	{
		SetState(State_StreamFlinch);
	}
	else if (ShouldPlayVariation())
	{
		SetState(State_StreamVariation);
		return FSM_Continue;
	}

	// Stream in the next clip in the sequence - this is the outro.
	m_OtherClipSetRequestHelper.Request(m_OtherClipSetId);

	bool bClipIsLoaded = m_bPlayDirectedCower ? m_BaseClipSetRequestHelper.IsLoaded() : m_OtherClipSetRequestHelper.IsLoaded();

	// We are done if the clip is loaded and either we are switching to directional cowering or time has expired.
	if (bClipIsLoaded && (CanPedLeaveUndirectedCowering() || m_bPlayDirectedCower))
	{
		if (m_bPlayDirectedCower)
		{
			// Play some audio during the transition.
			PlayIdleAudio();
			SetState(State_DirectedBase);
		}
		else
		{
			SetState(State_Outro);
		}
	}
	return FSM_Continue;
}

void CTaskCowerScenario::Base_OnProcessMoveSignals()
{
	// Cache off signals sent from MoVE.
	m_bMoveInTargetState |= m_MoveNetworkHelper.IsInTargetState();
}

// Directed base animations.
void CTaskCowerScenario::DirectedBase_OnEnter()
{
	// The ped is no longer switching between cower types.
	m_bSwitchingBetweenCowerTypes = false;

	// Tell MoVE which clip to play.
	SendDirectedClipInformation(m_BaseClipSetId);

	// Tell MoVE to play the clip.
	m_MoveNetworkHelper.SendRequest(ms_PlayBaseRequest);
	m_MoveNetworkHelper.WaitForTargetState(ms_OnEnterDirectedBaseId);
	
	// Reset the cached state variables.
	m_bMoveInTargetState = false;

	// Request signals to be sent back to the task from MoVE.
	RequestProcessMoveSignalCalls();

	// Set up the condition struct to select the next clipset (directed outro).
	CScenarioCondition::sScenarioConditionData conditionData;
	conditionData.pPed = GetPed();

	// Allow an override on the exit type.
	if (m_bOnlyDoUndirectedExit)
	{
		conditionData.eAmbientEventType = AET_In_Place;
	}
	else
	{
		conditionData.eAmbientEventType = AET_Directed_In_Place;
	}
	m_OtherClipSetId = ChooseConditionalClipSet(CConditionalAnims::AT_PANIC_OUTRO, conditionData);

	m_fLastDirection = GetAngleToTarget();
	m_bApproaching = false;
}

// Continue to play the directed base animations until time expires or we need to switch to undirected cowering.
CTask::FSM_Return CTaskCowerScenario::DirectedBase_OnUpdate()
{	
	if (!m_bMoveInTargetState)
	{
		return FSM_Continue;
	}

	PlayDirectedAudio();

	// If nothing much has happened in a while, if the source gets far away we might want to go back to undirected cowering.
	CheckForUndirectedCowerSwitchDueToDistance();

	// If we are not currently switching to a new form of cowering and a switch is desired, select a suitable undirected cowering clip.
	if (!m_bPlayDirectedCower && !m_bSwitchingBetweenCowerTypes)
	{
		m_bSwitchingBetweenCowerTypes = true;
		CScenarioCondition::sScenarioConditionData conditionData;
		conditionData.pPed = GetPed();
		conditionData.eAmbientEventType = AET_In_Place;
		m_BaseClipSetId = ChooseConditionalClipSet(CConditionalAnims::AT_PANIC_BASE, conditionData);

		// Reset the request helper so it doesn't still think we are streaming in the directed base clip.
		m_BaseClipSetRequestHelper.Request(m_BaseClipSetId);
	}
	else if(m_bDoPanicExit)
	{
		// Pick a panic exit.
		CScenarioCondition::sScenarioConditionData conditionData;
		conditionData.pPed = GetPed();
		conditionData.eAmbientEventType = AET_In_Place;
		m_OtherClipSetId = ChooseConditionalClipSet(CConditionalAnims::AT_PANIC_EXIT, conditionData);
	}

	// Stream in the next clip in the sequence (either the base undirected base animation or the directed outro).
	m_OtherClipSetRequestHelper.Request(m_OtherClipSetId);

	bool bClipIsLoaded = !m_bPlayDirectedCower ? m_BaseClipSetRequestHelper.IsLoaded() : m_OtherClipSetRequestHelper.IsLoaded();

	// We are done if the clip is loaded and either we are switching to directional cowering or time has expired.
	if (bClipIsLoaded && (m_fCowerTime <= 0.0f || !m_bPlayDirectedCower))
	{
		if (!m_bPlayDirectedCower)
		{
			// Play a small scream during the transition.
			PlayFlinchAudio();
			SetState(State_Base);
		}
		else
		{
			if (m_bDoPanicExit)
			{
				// All panic exits head through the Outro state.
				SetState(State_Outro);
			}
			else
			{
				if (m_bOnlyDoUndirectedExit)
				{
					SetState(State_Outro);
				}
				else
				{
					SetState(State_DirectedOutro);
				}

			}
		}
	}

	return FSM_Continue;
}

void CTaskCowerScenario::DirectedBase_OnProcessMoveSignals()
{
	// Cache off signals sent from MoVE.
	m_bMoveInTargetState |= m_MoveNetworkHelper.IsInTargetState();
}

void CTaskCowerScenario::StreamVariation_OnEnter()
{
	PickVariationAnimation();
}

CTask::FSM_Return CTaskCowerScenario::StreamVariation_OnUpdate()
{
	m_OtherClipSetRequestHelper.Request(m_OtherClipSetId);

	bool bClipIsLoaded = m_OtherClipSetRequestHelper.IsLoaded();

	if (bClipIsLoaded)
	{
		// Start to play the variation clip.
		SetState(State_Variation);
	}
	return FSM_Continue;
}

// Play a varied animation.
void CTaskCowerScenario::Variation_OnEnter()
{
	fwMvClipId clip;
	ChooseClip(m_OtherClipSetId, clip, false);
	SendClipInformation(m_OtherClipSetId, clip, ms_VariationClipId);

	// Build the condition struct to select an appropriate animation.
	CScenarioCondition::sScenarioConditionData conditionData;
	conditionData.pPed = GetPed();
	// Set up data for undirected cower intro.
	conditionData.eAmbientEventType = AET_In_Place;
	m_BaseClipSetId = ChooseConditionalClipSet(CConditionalAnims::AT_PANIC_BASE, conditionData);
	// Request it once in case the base clip changed this go around.
	m_BaseClipSetRequestHelper.Request(m_BaseClipSetId);

	// Tell MoVE to play the clip.
	m_MoveNetworkHelper.SendRequest(ms_PlayVariationRequest);
	m_MoveNetworkHelper.WaitForTargetState(ms_VariationDoneId);

	// Reset the cached state variables.
	m_bMoveInTargetState = false;

	// Request signals to be sent back to the task from MoVE.
	RequestProcessMoveSignalCalls();

	// Not fully in the variation state yet.
	m_bFullyInVariationState = false;

	// Say a whimper.
	PlayIdleAudio();
}


// Keep playing the variation clip until it finishes, then go back to undirected cowering.
CTask::FSM_Return CTaskCowerScenario::Variation_OnUpdate()
{
	if (m_bFullyInVariationState && ShouldPlayAFlinch())
	{
		SetState(State_StreamFlinch);
	} 
	else if (m_bMoveInTargetState && m_BaseClipSetRequestHelper.IsLoaded())
	{
		SetState(State_Base);
	}
	return FSM_Continue;
}

void CTaskCowerScenario::Variation_OnExit()
{
	// Not fully in the variation state anymore.
	m_bFullyInVariationState = false;
}

void CTaskCowerScenario::Variation_OnProcessMoveSignals()
{
	// Cache off signals sent from MoVE.
	if (!m_bFullyInVariationState && m_MoveNetworkHelper.GetBoolean(ms_OnEnterVariationId))
	{
		m_bFullyInVariationState = true;
	}

	m_bMoveInTargetState |= m_MoveNetworkHelper.IsInTargetState();
}

void CTaskCowerScenario::StreamFlinch_OnEnter()
{
	// Reset the fact that an event has triggered a flinch.
	m_bEventTriggersFlinch = false;

	PickFlinchAnimation();
}

CTask::FSM_Return CTaskCowerScenario::StreamFlinch_OnUpdate()
{
	m_OtherClipSetRequestHelper.Request(m_OtherClipSetId);

	bool bClipIsLoaded = m_OtherClipSetRequestHelper.IsLoaded();

	if (bClipIsLoaded)
	{
		// Start to play the flinch clip.
		SetState(State_Flinch);
	}
	return FSM_Continue;
}

void CTaskCowerScenario::Flinch_OnEnter()
{
	fwMvClipId clip;
	ChooseClip(m_OtherClipSetId, clip, false);
	SendClipInformation(m_OtherClipSetId, clip, ms_FlinchClipId);

	// Build the condition struct to select an appropriate animation.
	CScenarioCondition::sScenarioConditionData conditionData;
	conditionData.pPed = GetPed();
	// Set up data for undirected cower intro.
	conditionData.eAmbientEventType = AET_In_Place;
	m_BaseClipSetId = ChooseConditionalClipSet(CConditionalAnims::AT_PANIC_BASE, conditionData);
	// Request it once in case the base clip changed this go around.
	m_BaseClipSetRequestHelper.Request(m_BaseClipSetId);

	// Tell MoVE to play the clip.
	m_MoveNetworkHelper.SendRequest(ms_PlayFlinchRequest);
	m_MoveNetworkHelper.WaitForTargetState(ms_FlinchDoneId);

	// Reset the cached state variables.
	m_bMoveInTargetState = false;

	// Request signals to be sent back to the task from MoVE.
	RequestProcessMoveSignalCalls();

	// Scream.
	PlayFlinchAudio();

	// Note the time of the flinch.
	sm_uLastTimeAnyoneFlinched = fwTimer::GetTimeInMilliseconds();
}

CTask::FSM_Return CTaskCowerScenario::Flinch_OnUpdate()
{
	if (m_bMoveInTargetState && m_BaseClipSetRequestHelper.IsLoaded())
	{
		SetState(State_Base);
	}
	return FSM_Continue;
}

void CTaskCowerScenario::Flinch_OnProcessMoveSignals()
{
	// Cache off signals sent from MoVE.
	m_bMoveInTargetState |= m_MoveNetworkHelper.IsInTargetState();
}

// Play the undirected cower exit anim.
void CTaskCowerScenario::Outro_OnEnter()
{
	// Tell MoVE which clip to play.
	fwMvClipId clip;
	ChooseClip(m_OtherClipSetId, clip, false);
	SendClipInformation(m_OtherClipSetId, clip, ms_OutroClipId);

	// Tell MoVE to play the clip.
	m_MoveNetworkHelper.SendRequest(ms_PlayBaseOutroRequest);
	m_MoveNetworkHelper.WaitForTargetState(ms_OutroDoneId);

	// Reset the cached state variables.
	m_bMoveInTargetState = false;

	// Request signals to be sent back to the task from MoVE.
	RequestProcessMoveSignalCalls();
}

// Continue playing the exit clip until it finishes.
CTask::FSM_Return CTaskCowerScenario::Outro_OnUpdate()
{
	if (m_bMoveInTargetState)
	{
		m_bQuitTaskOnEvent = true;
		SetState(State_Finish);
	}
	return FSM_Continue;
}

void CTaskCowerScenario::Outro_OnProcessMoveSignals()
{
	// Cache off signals sent from MoVE.
	m_bMoveInTargetState |= m_MoveNetworkHelper.IsInTargetState();
}

// Play the directed cower exit anim.
void CTaskCowerScenario::DirectedOutro_OnEnter()
{
	// Tell MoVE which clip to play.
	SendDirectedClipInformation(m_OtherClipSetId);

	// Tell MoVE to play the clip.
	m_MoveNetworkHelper.SendRequest(ms_PlayDirectedOutroRequest);
	m_MoveNetworkHelper.WaitForTargetState(ms_OutroDoneId);

	// Reset the cached state variables.
	m_bMoveInTargetState = false;

	// Request signals to be sent back to the task from MoVE.
	RequestProcessMoveSignalCalls();

	m_fLastDirection = GetAngleToTarget();
	m_bApproaching = false;
}

// Continue playing the exit clip until it finishes.
CTask::FSM_Return CTaskCowerScenario::DirectedOutro_OnUpdate()
{
	if (m_bMoveInTargetState)
	{
		m_bQuitTaskOnEvent = true;
		SetState(State_Finish);
	}
	return FSM_Continue;
}

void CTaskCowerScenario::DirectedOutro_OnProcessMoveSignals()
{
	// Cache off signals sent from MoVE.
	m_bMoveInTargetState |= m_MoveNetworkHelper.IsInTargetState();
}

void CTaskCowerScenario::CleanUp()
{
	CPed* pPed = GetPed();
	
	const CTask* pParentTask = GetParent();
	if(pParentTask && pParentTask->GetTaskType() == CTaskTypes::TASK_USE_SCENARIO)
	{
		m_bMigrateDontClearMoveNetwork = static_cast<const CTaskUseScenario*>(pParentTask)->GetMigratingCowerExitingTask();
	}
	
	if(pParentTask && pParentTask->GetTaskType() == CTaskTypes::TASK_COWER)
	{
		m_bMigrateDontClearMoveNetwork = static_cast<const CTaskCower*>(pParentTask)->LeaveCowerAnimsForClone();
	}

	CTaskReactAndFlee* pTaskReactAndFlee = static_cast<CTaskReactAndFlee *>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_REACT_AND_FLEE));
	if(pTaskReactAndFlee)
	{
		m_bMigrateDontClearMoveNetwork = pTaskReactAndFlee->GetOutgoingMigratingCower();
	}

	if(!m_bMigrateDontClearMoveNetwork)
	{
		CTaskSmartFlee* pTaskSmartFlee = static_cast<CTaskSmartFlee *>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_SMART_FLEE));
		if(pTaskSmartFlee)
		{
			m_bMigrateDontClearMoveNetwork = pTaskSmartFlee->GetOutgoingMigratingCower();
		}
	}

	//Check if the move network is active.
	if(m_MoveNetworkHelper.IsNetworkActive() )
	{	
		if(!m_bMigrateDontClearMoveNetwork)
		{
			pPed->GetMovePed().ClearTaskNetwork(m_MoveNetworkHelper, SLOW_BLEND_DURATION);
		}

		m_MoveNetworkHelper.ReleaseNetworkPlayer();
	}
}

void CTaskCowerScenario::CacheOffAnimationSupport()
{
	// Build the condition struct.
	CScenarioCondition::sScenarioConditionData conditionData;
	conditionData.pPed = GetPed();
	conditionData.eAmbientEventType = AET_In_Place;


	// Check for variations.
	fwMvClipSetId clipset = CLIP_SET_ID_INVALID;
	clipset = ChooseConditionalClipSet(CConditionalAnims::AT_PANIC_VARIATION, conditionData);
	m_bHasAVariationAnimation = clipset != CLIP_SET_ID_INVALID;

	// Reset the test clipset.
	clipset = CLIP_SET_ID_INVALID;
	
	// Rebuild the struct.
	conditionData.eAmbientEventType = AET_Directed_In_Place;

	// Check for directed base animations.
	clipset = ChooseConditionalClipSet(CConditionalAnims::AT_PANIC_BASE, conditionData);
	m_bHasDirectedBaseAnimations = clipset != CLIP_SET_ID_INVALID;

	// Rebuild the struct.
	conditionData.eAmbientEventType = AET_Flinch;

	// Check for flinch animations.
	clipset = ChooseConditionalClipSet(CConditionalAnims::AT_REACTION, conditionData);
	m_bHasAFlinchAnimation = clipset != CLIP_SET_ID_INVALID;
}

// The ped can leave if the cower time has expired and the ped has left the immediate vicinity.
bool CTaskCowerScenario::CanPedLeaveUndirectedCowering()
{
	if (m_fCowerTime <= 0)
	{
		if (m_bSkipExitDistanceCheck)
		{
			return true;
		}
		const CEntity* pSourceEntity = m_Target.GetEntity();
		if (pSourceEntity)
		{
			Vec3V vPosition = pSourceEntity->GetTransform().GetPosition();

			if (IsGreaterThanAll(DistSquared(vPosition, GetPed()->GetTransform().GetPosition()), ScalarV(sm_Tunables.m_ReturnToNormalDistanceSq)))
			{
				return true;
			}
		}
		else
		{
			return true;
		}

	}
	return false;
}

void CTaskCowerScenario::CheckForUndirectedCowerSwitchDueToDistance()
{
	// Make sure directed cowering is going on.
	if (GetState() != State_DirectedBase)
	{
		return;
	}

	// Make sure enough time has passed.
	if (GetTimeInState() < sm_Tunables.m_EventlessSwitchStateTimeRequirement)
	{
		return;
	}

	// Make sure enough time has passed since the last event.
	if (fwTimer::GetTimeInMilliseconds() - m_uTimeOfCowerTrigger < sm_Tunables.m_EventlessSwitchInactivityTimeRequirement)
	{
		return;
	}

	// Make sure there is a target.
	if (!m_Target.GetIsValid())
	{
		return;
	}

	Vector3 vPosition;
	m_Target.GetPosition(vPosition);
	Vec3V vPedPosition = GetPed()->GetTransform().GetPosition();

	// See if the player is too close to go to directed.
	if (IsLessThanAll(DistSquared(VECTOR3_TO_VEC3V(vPosition), vPedPosition), ScalarV(sm_Tunables.m_EventlessSwitchDistanceRequirement * sm_Tunables.m_EventlessSwitchDistanceRequirement)))
	{
		return;
	}

	// It is ok to switch to undirected cowering.
	m_bPlayDirectedCower = false;
}

void CTaskCowerScenario::CheckForDirectedCowerSwitchDueToDistance()
{
	// Make sure directed cowering is going on.
	if (GetState() != State_Base)
	{
		return;
	}

	// Make sure enough time has passed since the last event.
	if (fwTimer::GetTimeInMilliseconds() - m_uTimeOfCowerTrigger < sm_Tunables.m_EventlessSwitchInactivityTimeRequirement)
	{
		return;
	}

	// Make sure there is a target.
	if (!m_Target.GetIsValid())
	{
		return;
	}

	Vector3 vPosition;
	m_Target.GetPosition(vPosition);
	Vec3V vPedPosition = GetPed()->GetTransform().GetPosition();

	// See if the player is too far away to go to directed.
	if (IsGreaterThanAll(DistSquared(VECTOR3_TO_VEC3V(vPosition), vPedPosition), ScalarV(sm_Tunables.m_EventlessSwitchDistanceRequirement * sm_Tunables.m_EventlessSwitchDistanceRequirement)))
	{
		return;
	}

	// It is ok to switch to directed cowering.
	m_bPlayDirectedCower = true;
}

// Return a conditional anims array corresponding to the type desired.
fwMvClipSetId CTaskCowerScenario::ChooseConditionalClipSet(CConditionalAnims::eAnimType animType, CScenarioCondition::sScenarioConditionData& conditionData) const
{
	fwMvClipSetId retval = CLIP_SET_ID_INVALID;
	{
		if (m_pConditionalAnims)
		{
			const CConditionalAnims::ConditionalClipSetArray* pClipSetArray = m_pConditionalAnims->GetClipSetArray(animType);
			if (pClipSetArray)
			{
				const CConditionalClipSet* pClipSet = CConditionalAnims::ChooseClipSet(*pClipSetArray, conditionData);
				if (pClipSet)
				{
					retval = pClipSet->GetClipSetId();
				}
			}
		}
	}
	return retval;
}

// Set the rClipId outparam - either its randomly chosen or it is the clip that is tagged as best rotating towards the target. 
bool CTaskCowerScenario::ChooseClip(const fwMvClipSetId& rClipSetId, fwMvClipId& rClipId, bool bChooseDirectedToTarget)
{
	CPed* pPed = GetPed();

	// Choose best rotation.
	if (bChooseDirectedToTarget)
	{
		const fwClipSet* pSet = fwClipSetManager::GetClipSet(rClipSetId);
		fwMvClipId bestClip = CLIP_ID_INVALID;

		// Iterate over each clip in the clipset, finding the one that most closely matches the angle needed to rotate towards the target.
		if (Verifyf(pSet, "Invalid clipset!"))
		{
			float fBestScore = FLT_MAX;

			Vector3 vTarget;
			m_Target.GetPosition(vTarget);
			float fHeading = fwAngle::GetRadianAngleBetweenPoints(vTarget.x, vTarget.y, pPed->GetTransform().GetPosition().GetXf(), pPed->GetTransform().GetPosition().GetYf());
			fHeading = SubtractAngleShorter(fHeading, pPed->GetCurrentHeading());

			for (int i=0; i < pSet->GetClipItemCount(); i++)
			{
				fwMvClipId testClipId = pSet->GetClipItemIdByIndex(i);
				fwClipItem* pClipItem = pSet->GetClipItemByIndex(i);
				float fDirection = GetClipItemDirection(pClipItem);
				float fScore = fabs(SubtractAngleShorter(fDirection, fHeading));

				if (fScore < fBestScore)
				{
					fBestScore = fScore;
					bestClip = testClipId;
				}
			}
		}
		rClipId = bestClip;
		return bestClip != CLIP_ID_INVALID;
	}
	else
	{
		// Undirected, so pick a random clip from the set.
		return CAmbientClipRequestHelper::PickClipByGroup_SimpleConditions(pPed, rClipSetId, rClipId);
	}
}

// Return the relative angle from the perspective of the cowering ped to the threat (in radians).
float CTaskCowerScenario::GetAngleToTarget()
{
	CPed* pPed = GetPed();

	Vector3 vTarget;
	m_Target.GetPosition(vTarget);
	float fHeading = fwAngle::GetRadianAngleBetweenPoints(vTarget.x, vTarget.y, pPed->GetTransform().GetPosition().GetXf(), pPed->GetTransform().GetPosition().GetYf());
	fHeading = SubtractAngleShorter(fHeading, pPed->GetCurrentHeading());

	return fHeading;
}

// Return the rate that the cowering animation should play at.
float CTaskCowerScenario::GetCoweringRate()
{
	static dev_float s_fMinRate = 0.85f;
	static dev_float s_fMaxRate = 1.15f;
	static dev_float s_fMinDifferenceFromLastRate = 0.05f;

	float fRate = fwRandom::GetRandomNumberInRange(s_fMinRate, s_fMaxRate);
	if (fabs(fRate - sm_fLastCowerRate) < s_fMinDifferenceFromLastRate)
	{
		fRate += 0.07f;
	}
	
	sm_fLastCowerRate = fRate;

	return fRate;
}

// How long the ped should cower depends on the event type.
// TODO - push into data somehow?  Not all events have tunables..but this could go in scenariotriggers.meta.
float CTaskCowerScenario::GetTimeToCower(int iEventType)
{
	switch(iEventType)
	{
	case EVENT_GUN_AIMED_AT:
		return 10.0f;
	case EVENT_MELEE_ACTION:
		return 10.0f;
	case EVENT_SHOCKING_SEEN_MELEE_ACTION:
		return 10.0f;
	default:
		return CTaskSmartFlee::GetBaseTimeToCowerS();
	}
}

// Called in AffectsPedCore() of certain events to notify the cowering ped that the event has occured, even if responding to some
// event with higher priority.
void CTaskCowerScenario::NotifyCoweringPedOfEvent(CPed& rPed, const CEvent& rEvent)
{
	// Special code for cowering peds to get them to react to gunshots even when being aimed at.
	// This is done because CEventGunAimedAt has a higher priority for peds, but we still need them to react here.
	CTaskCowerScenario* pTaskCowerScenario = static_cast<CTaskCowerScenario*>(rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COWER_SCENARIO));
	if (pTaskCowerScenario)
	{
		if (pTaskCowerScenario->ShouldUpdateCowerStatus(rEvent))
		{
			pTaskCowerScenario->UpdateCowerStatus(rEvent);
		}
	}
}

// Increase the cower timer to handle the new event.
// Decide if the event warrants switching from directed/undirected.
void CTaskCowerScenario::HandleEventType(s16 iEventType, s16 iEventPriority, const CEvent* pEvent)
{
	m_iCurrentEventType = iEventType;
	m_iCurrentEventPriority = iEventPriority;

	m_fCowerTime = GetTimeToCower(iEventType);

	m_bPlayDirectedCower = HasDirectedBaseAnimations() && ShouldPlayDirectedCowerAnimation(iEventType);

	m_bEventTriggersFlinch = DoesEventTriggerFlinch(pEvent);

	m_uTimeOfCowerTrigger = fwTimer::GetTimeInMilliseconds();
}

// Send the clip information being played over to MoVE.
void CTaskCowerScenario::SendClipInformation(fwMvClipSetId& rClipSetId, const fwMvClipId& rClipId, const fwMvClipId& moveSignalClipId)
{
	fwClipSet* pSet = fwClipSetManager::GetClipSet(rClipSetId);
	if (Verifyf(pSet, "Undirected Clipset %s did not exist for CTaskCowerScenario!", rClipSetId.GetCStr()))
	{
		if (Verifyf(pSet->Request_DEPRECATED(), "Undirected Clipset %s was not streamed in for CTaskCowerScenario!", rClipSetId.GetCStr()))
		{
			m_MoveNetworkHelper.SetClipSet(rClipSetId);

			const crClip* pClip = fwClipSetManager::GetClip(rClipSetId, rClipId);

#if __ASSERT
			if(!NetworkInterface::IsGameInProgress())
			{
				taskAssertf(pClip, "Failed to find clip %s in clip set %s", rClipId.GetCStr(), rClipSetId.GetCStr());
			}
#endif

			if (pClip)
			{
				m_MoveNetworkHelper.SetClip(pClip, moveSignalClipId);
			}
		}
	}

	// Send the rate variable.
	float fRate = GetCoweringRate();
	m_MoveNetworkHelper.SetFloat(ms_RateId, fRate);
}

// Walk through all the clips in the clipset and look for clips corresponding to each of the five basic cower directions:
// Front, left, right, backleft, and backright.
void CTaskCowerScenario::SendDirectedClipInformation(fwMvClipSetId& rClipSetId)
{
	static const float s_fForward = 0.0f;
	static const float s_fLeft = HALF_PI;
	static const float s_fRight = -HALF_PI;
	static const float s_fBackLeft = HALF_PI + (HALF_PI / 2);
	static const float s_fBackRight = -HALF_PI - (HALF_PI / 2);
	static const float s_fTolerance = 0.1f;

	fwClipSet* pSet = fwClipSetManager::GetClipSet(rClipSetId);
	if (Verifyf(pSet && pSet->Request_DEPRECATED(), "Directed Clipset %s not streamed in for TaskCowerScenario!", rClipSetId.GetCStr()))
	{
		m_MoveNetworkHelper.SetClipSet(rClipSetId);

		// Iterate over the clips, looking for the right tags.
		for (int i=0; i < pSet->GetClipItemCount(); i++)
		{
			const fwMvClipId testClipId = pSet->GetClipItemIdByIndex(i);
			const fwClipItem* pClipItem = pSet->GetClipItemByIndex(i);
			const crClip* pClip = fwAnimManager::GetClipIfExistsBySetId(rClipSetId, testClipId);
			float fDirection = GetClipItemDirection(pClipItem);

			if (Verifyf(pClip && pClipItem && pClipItem->IsDirectedClipItemWithProps(), "Invalid clip format!"))
			{
				if (fabs(fDirection - s_fForward) <= s_fTolerance)
				{
					SendForwardClipInformation(pClip);
				}
				else if (fabs(fDirection - s_fLeft) <= s_fTolerance)
				{
					SendLeftClipInformation(pClip);
				}
				else if (fabs(fDirection - s_fRight) <= s_fTolerance)
				{
					SendRightClipInformation(pClip);
				}
				else if (fabs(fDirection - s_fBackLeft) <= s_fTolerance)
				{
					SendBackLeftClipInformation(pClip);
				}
				else if (fabs(fDirection - s_fBackRight) <= s_fTolerance)
				{
					SendBackRightClipInformation(pClip);
				}
			}
		}
	}

	// Send the rate variable.
	float fRate = GetCoweringRate();
	m_MoveNetworkHelper.SetFloat(ms_RateId, fRate);
}

void CTaskCowerScenario::SendForwardClipInformation(const crClip* pClip)
{
	s32 iState = GetState();
	
	switch(iState)
	{
	case State_DirectedIntro:
	case State_DirectedOutro:
		m_MoveNetworkHelper.SetClip(pClip, ms_DirectedForwardTransClipId);
		break;
	case State_DirectedBase:
		m_MoveNetworkHelper.SetClip(pClip, ms_DirectedForwardBaseClipId);
		break;
	default:
		break;
	}
}

void CTaskCowerScenario::SendLeftClipInformation(const crClip* pClip)
{
	s32 iState = GetState();

	switch(iState)
	{
	case State_DirectedIntro:
	case State_DirectedOutro:
		m_MoveNetworkHelper.SetClip(pClip, ms_DirectedLeftTransClipId);
		break;
	case State_DirectedBase:
		m_MoveNetworkHelper.SetClip(pClip, ms_DirectedLeftBaseClipId);
		break;
	default:
		break;
	}
}

void CTaskCowerScenario::SendRightClipInformation(const crClip* pClip)
{
	s32 iState = GetState();

	switch(iState)
	{
	case State_DirectedIntro:
	case State_DirectedOutro:
		m_MoveNetworkHelper.SetClip(pClip, ms_DirectedRightTransClipId);
		break;
	case State_DirectedBase:
		m_MoveNetworkHelper.SetClip(pClip, ms_DirectedRightBaseClipId);
		break;
	default:
		break;
	}
}

void CTaskCowerScenario::SendBackLeftClipInformation(const crClip* pClip)
{
	s32 iState = GetState();

	switch(iState)
	{
	case State_DirectedIntro:
	case State_DirectedOutro:
		m_MoveNetworkHelper.SetClip(pClip, ms_DirectedBackLeftTransClipId);
		break;
	case State_DirectedBase:
		m_MoveNetworkHelper.SetClip(pClip, ms_DirectedBackLeftBaseClipId);
		break;
	default:
		break;
	}
}

void CTaskCowerScenario::SendBackRightClipInformation(const crClip* pClip)
{
	s32 iState = GetState();

	switch(iState)
	{
	case State_DirectedIntro:
	case State_DirectedOutro:
		m_MoveNetworkHelper.SetClip(pClip, ms_DirectedBackRightTransClipId);
		break;
	case State_DirectedBase:
		m_MoveNetworkHelper.SetClip(pClip, ms_DirectedBackRightBaseClipId);
		break;
	default:
		break;
	}
}

void CTaskCowerScenario::SetTimeUntilNextVariation()
{
	if (m_pConditionalAnims)
	{
		// +/- 10% of the value in the scenario info.
		m_fVariationTimer = m_pConditionalAnims->GetNextIdleTime() * fwRandom::GetRandomNumberInRange(0.9f, 1.1f);
	}
}

bool CTaskCowerScenario::HasAVariationAnimation() const
{
	return m_bHasAVariationAnimation;
}

bool CTaskCowerScenario::HasDirectedBaseAnimations() const
{
	return m_bHasDirectedBaseAnimations;
}

bool CTaskCowerScenario::HasAFlinchAnimation() const
{
	return m_bHasAFlinchAnimation;
}

// Determine if the ped should play a variation animation.
bool CTaskCowerScenario::ShouldPlayVariation() const
{
	// Don't play a variation if:
	// 1.  We don't have one.
	// 2.  Cowering is about to stop anyway.
	// 3.  It's been too short of a time since the ped started their base animation.
	if (HasAVariationAnimation() && m_fCowerTime >= 5.0f && GetTimeInState() > m_fVariationTimer)
	{
		return true;
	}
	else
	{
		return false;
	}
}

void CTaskCowerScenario::PickVariationAnimation()
{
	CScenarioCondition::sScenarioConditionData conditionData;
	conditionData.pPed = GetPed();
	conditionData.eAmbientEventType = AET_In_Place;
	m_OtherClipSetId = ChooseConditionalClipSet(CConditionalAnims::AT_PANIC_VARIATION, conditionData);
}

bool CTaskCowerScenario::ShouldPlayAFlinch() const
{
	if (HasAFlinchAnimation() && m_bEventTriggersFlinch && (fwTimer::GetTimeInMilliseconds()- sm_uLastTimeAnyoneFlinched >  sm_Tunables.m_MinTimeBetweenFlinches))
	{
		return true;
	}
	else
	{
		return false;
	}
}

void CTaskCowerScenario::PickFlinchAnimation()
{
	CScenarioCondition::sScenarioConditionData conditionData;
	conditionData.pPed = GetPed();
	conditionData.eAmbientEventType = AET_Flinch;
	m_OtherClipSetId = ChooseConditionalClipSet(CConditionalAnims::AT_REACTION, conditionData);
}

// Return the kind of cowering associated with the event type.
// TODO - push into data?  Maybe not as almost everything is undirected except for the targetting events.
bool CTaskCowerScenario::ShouldPlayDirectedCowerAnimation(int iEventType)
{
	// Look at the event type.
	switch(iEventType)
	{
		case EVENT_AGITATED_ACTION:
		case EVENT_MELEE_ACTION:
		case EVENT_GUN_AIMED_AT:
			return true;
		default:
			return false;
	}
}

// Return if this event should cause someone to flinch.
bool CTaskCowerScenario::DoesEventTriggerFlinch(const CEvent* pEvent) const
{
	// Make sure the event is valid.
	if (!pEvent)
	{
		return false;
	}

	// See if we should ignore this event.
	if (pEvent == m_FlinchIgnoreEvent)
	{
		return false;
	}

	// Look at the event type.
	switch(pEvent->GetEventType())
	{
		case EVENT_SHOCKING_GUNSHOT_FIRED:
		case EVENT_SHOCKING_GUN_FIGHT:
		case EVENT_SHOT_FIRED:
		case EVENT_SHOT_FIRED_BULLET_IMPACT:
		case EVENT_SHOT_FIRED_WHIZZED_BY:
		case EVENT_SHOCKING_SEEN_MELEE_ACTION:
		case EVENT_EXPLOSION:
		case EVENT_SHOCKING_EXPLOSION:
		case EVENT_SHOCKING_CAR_CRASH:
			return true;
		default:
			return false;
	}
}

// Look at the event and decide if it warrants updating out cower state.
bool CTaskCowerScenario::ShouldUpdateCowerStatus(const aiEvent& rEvent)
{
	s16 iType = (s16)rEvent.GetEventType();
	if (HasDirectedBaseAnimations() && ShouldPlayDirectedCowerAnimation(iType))
	{
		// Allow directed events through if the last event responded to was a long time ago or if it is the same event.
		if ((rEvent.GetEventType() == m_iCurrentEventType) || fwTimer::GetTimeInMilliseconds() - m_uTimeOfCowerTrigger >= sm_Tunables.m_EventDecayTimeMS)
		{	
			if (NetworkInterface::IsGameInProgress())
			{
				if (m_uTimeDenyTargetChange > fwTimer::GetSystemTimeInMilliseconds())
				{
					const CEvent& rEEvent = static_cast<const CEvent&>(rEvent);
					const CEntity* pSourceEntity = rEEvent.GetSourceEntity();
					const CEntity* pTargetEntity = m_Target.GetEntity();
					
					if (pSourceEntity && pTargetEntity && (pSourceEntity != pTargetEntity))
						return false;
				}
			}

			return true;
		}
	}
	else
	{
		if (rEvent.GetEventPriority() >= m_iCurrentEventPriority)
		{
			// Event has a higher priority and should definitely update the cower state.
			return true;
		}
		else if (iType == EVENT_SHOT_FIRED_WHIZZED_BY || iType == EVENT_SHOT_FIRED || iType == EVENT_SHOT_FIRED_BULLET_IMPACT)
		{
			// Let the ped react to gunshots, even if responding to something with a higher priority.
			return true;
		}
	}
	return false;
}

// Perform logic to change cowering in response to a new event.
void CTaskCowerScenario::UpdateCowerStatus(const aiEvent& rEvent)
{
	s16 iType = (s16)rEvent.GetEventType();
	HandleEventType(iType, (short)rEvent.GetEventPriority(), static_cast<const CEvent*>(&rEvent));

	// Update the target if necessary - only for events where the position is important or dramatic events like bullets.
	if ((HasDirectedBaseAnimations() && ShouldPlayDirectedCowerAnimation(iType)) || (iType == EVENT_SHOT_FIRED_WHIZZED_BY || iType == EVENT_SHOT_FIRED || iType == EVENT_SHOT_FIRED_BULLET_IMPACT))
	{
		UpdateCowerTarget(rEvent);
	}
}

// Allow script events and damage to end cowering.
bool CTaskCowerScenario::ShouldStopCowering(const aiEvent& rEvent)
{
	if (IsRagdollEvent())
	{
		return true;
	}
	else
	{
		switch(rEvent.GetEventType())
		{
			case EVENT_GIVE_PED_TASK:
			case EVENT_SCRIPT_COMMAND:
			{
				return true;
			}
			default:
			{
				return false;
			}
		}
	}
}

// Look at the event and decide how best to quit this task.
void CTaskCowerScenario::StopCowering(const aiEvent& rEvent)
{
	if (IsRagdollEvent())
	{
		TriggerImmediateExit();
	}
	else
	{
		switch(rEvent.GetEventType())
		{
			case EVENT_GIVE_PED_TASK:
			case EVENT_SCRIPT_COMMAND:
			{
				// A scripter wants this ped to stop cowering, so do a normal exit.
				TriggerNormalExit();
				break;
			}
			default:
			{
				Assertf(false, "Unexpected event type %d.", rEvent.GetEventType());
				break;
			}
		}
	}
}

// Check if the ped has a ragdoll physical event response task (on fire, writhe, etc.).
bool CTaskCowerScenario::IsRagdollEvent()
{
	CPed* pPed = GetPed();
	aiTask* pEventResponseTask = pPed->GetPedIntelligence()->GetPhysicalEventResponseTask();

	if (!pEventResponseTask)
	{
		pEventResponseTask = pPed->GetPedIntelligence()->GetEventResponseTask();
	}

	const bool bNmRagdollTask = pEventResponseTask && CTaskNMBehaviour::DoesResponseHandleRagdoll(pPed, pEventResponseTask);

	return bNmRagdollTask;
}

// Look at the event and update the cowering target.
void CTaskCowerScenario::UpdateCowerTarget(const aiEvent& rEvent)
{
	const CEvent& rEEvent = static_cast<const CEvent&>(rEvent);

	if (rEEvent.GetSourceEntity())
	{
		m_Target.SetEntity(rEEvent.GetSourceEntity());

		if (NetworkInterface::IsGameInProgress())
		{
			static const u32 targetChangeDelay = 10000;
			m_uTimeDenyTargetChange = fwTimer::GetSystemTimeInMilliseconds() + targetChangeDelay;
		}
	}
	else
	{

		Vector3 vTargetPosition = VEC3V_TO_VECTOR3(rEEvent.GetEntityOrSourcePosition());
		Assertf(!vTargetPosition.IsClose(VEC3_ZERO, SMALL_FLOAT), "Position for event type %d was near the origin, is this valid?", rEEvent.GetEventType());
		m_Target.SetPosition(vTargetPosition);
	}
}

void CTaskCowerScenario::UpdateHeadIK()
{
	s32 iState = GetState();

	bool bDoHeadIK = false;

	switch(iState)
	{
		case State_DirectedIntro:
		case State_DirectedBase:
		case State_DirectedOutro:
			bDoHeadIK = true;
			break;
		default:
			bDoHeadIK = false;
			break;
	}

	if (bDoHeadIK)
	{
		CPed* pPed = GetPed();

		if (m_Target.GetIsValid())
		{
			const CEntity* pSourceEntity = m_Target.GetEntity();
			Vector3 vTarget;
			m_Target.GetPosition(vTarget);

			// x = yaw, y = roll, z = pitch MAGIC - seems to work well for melee IK
			Vector3 vMin(DtoR * -125.0f, 0.0f, -PI);
			Vector3 vMax(DtoR * 125.0f, 0.0f, PI);

			float fHeightDiff = Abs(vTarget.z - pPed->GetTransform().GetPosition().GetZf());

			bool bWithinFOV = pPed->GetIkManager().IsTargetWithinFOV(vTarget, vMin, vMax);

			bool bWithinHeight = fHeightDiff < 1.5f; // MAGIC

			if (bWithinFOV && bWithinHeight)
			{
				if (!pPed->GetIKSettings().IsIKFlagDisabled(IKManagerSolverTypes::ikSolverTypeHead))
				{	

					// look at head of peds
					if (pSourceEntity && pSourceEntity->GetIsTypePed())
					{
						pPed->GetIkManager().LookAt(0, pSourceEntity, 100, BONETAG_HEAD, NULL);
					}
					// look at position of cars
					else if (pSourceEntity && pSourceEntity->GetIsTypeVehicle())
					{
						pPed->GetIkManager().LookAt(0, pSourceEntity, 100, BONETAG_INVALID, NULL);
					}
					// otherwise just look at the position of the event source (for explosions, etc.)
					else
					{
						pPed->GetIkManager().LookAt(0, NULL, 100, BONETAG_INVALID, &vTarget);
					}
				}
				else
				{
					// cant do headlook, so at least do eye IK
					pPed->GetIkManager().LookAt(0, NULL, 100, BONETAG_INVALID, &vTarget, LF_USE_EYES_ONLY, 500, 500, CIkManager::IK_LOOKAT_HIGH );
				}
			}
		}
	}
}

// Send the direction signal to MoVE based on where the threat is relative to the cowering ped.
void CTaskCowerScenario::UpdateDirectedCowerBlend()
{
	float fHeading = GetAngleToTarget();


	fHeading = Clamp(fHeading, -PI, PI);


	static float s_fLeftBoundry = 0.875f * PI;
	static float s_fRightBoundry = s_fLeftBoundry * -1.0f;

	//B*1832261: Don't worry about processing heading if we are behind the ped 
	static float s_fMaxHeading = PI - 0.3f;
	bool bNotBehindPed = Abs(fHeading) < s_fMaxHeading;

	if (m_fLastDirection > s_fLeftBoundry && bNotBehindPed)
	{
		if (fHeading > m_fLastDirection)
		{
			fHeading = s_fLeftBoundry;
		}
		else if (fHeading < 0.0f)
		{
			//interp towards fHeading
			if (!m_bApproaching)
			{
				m_bApproaching = true;
				m_fApproachDirection = s_fLeftBoundry;
			}
		}
	}
	else if (m_fLastDirection < s_fRightBoundry && bNotBehindPed)
	{
		if (fHeading < m_fLastDirection)
		{
			fHeading = s_fRightBoundry;
		}
		else if (fHeading > 0.0f)
		{
			//interp towards heading
			if (!m_bApproaching)
			{
				m_bApproaching = true;
				m_fApproachDirection = s_fRightBoundry;
			}
		}
	}
	else
	{
		m_fLastDirection = fHeading;
	}

	if (m_bApproaching)
	{
		Approach(m_fApproachDirection, fHeading, sm_Tunables.m_BackHeadingInterpRate, fwTimer::GetTimeStep());

		if (fabs(m_fApproachDirection - fHeading) < SMALL_FLOAT)
		{
			m_bApproaching = false;
			m_fLastDirection = fHeading;
		}

		fHeading = m_fApproachDirection;
	}

	// Convert to a MoVE signal [0,1]
	fHeading = (fHeading/PI + 1.0f)/2.0f;

	m_MoveNetworkHelper.SetFloat(ms_TargetHeadingId, fHeading);
}

void CTaskCowerScenario::KeepStreamingBaseClip()
{
	m_BaseClipSetRequestHelper.Request(m_BaseClipSetId);
}

void CTaskCowerScenario::PossiblyDestroyPedCoweringOffscreen(CPed* pPed, CTaskGameTimer& inout_timer)
{
	// Check if the ped can be deleted
	const bool bDeletePedIfInCar = true; // allow deletion in cars
	const bool bDoNetworkChecks = true; // do network checks (this is the default)
	if( pPed->CanBeDeleted(bDeletePedIfInCar, bDoNetworkChecks) )
	{
		// Get handle to local player ped
		const CPed* pLocalPlayerPed = CGameWorld::FindLocalPlayer();
		if( pLocalPlayerPed )
		{
			// Check distance to local player is beyond minimum threshold
			const ScalarV distToLocalPlayerSq_MIN = REPLAY_ONLY(CReplayMgr::IsRecording() ? ScalarVFromF32( rage::square(CTaskCowerScenario::GetTunables().m_MinDistFromPlayerToDeleteCoweringForeverRecordingReplay) ) :) ScalarVFromF32( rage::square(CTaskCowerScenario::GetTunables().m_MinDistFromPlayerToDeleteCoweringForever) );
			const ScalarV distToLocalPlayerSq = DistSquared(pLocalPlayerPed->GetTransform().GetPosition(), pPed->GetTransform().GetPosition());
			if( IsGreaterThanAll(distToLocalPlayerSq, distToLocalPlayerSq_MIN) )
			{
				// Check if ped is offscreen
				const bool bCheckNetworkPlayersScreens = true;
				if( !pPed->GetIsOnScreen(bCheckNetworkPlayersScreens) )
				{
					// Check if the ped deletion timer was already set and is now expired
					if( inout_timer.IsOutOfTime() )
					{
						// Mark ped for deletion
						pPed->FlagToDestroyWhenNextProcessed();
					}
					// Check if the ped deletion timer needs to be set
					else if( !inout_timer.IsSet() )
					{
						// Roll a random time to delay
						const int iDurationMS = fwRandom::GetRandomNumberInRange((int)CTaskCowerScenario::GetTunables().m_CoweringForeverDeleteOffscreenTimeMS_MIN, (int)CTaskCowerScenario::GetTunables().m_CoweringForeverDeleteOffscreenTimeMS_MAX);
						
						// Set the deletion timer
						inout_timer.Set(fwTimer::GetTimeInMilliseconds(), iDurationMS);
					}

					// ped continues to pass conditions for removal
					return;
				}
			}
		}
	}

	// Stop the deletion timer in case it is active
	inout_timer.Unset();
}

void CTaskCowerScenario::DropProps()
{
	CPed* pPed = GetPed();
	CPedWeaponManager* pWeaponMgr = pPed->GetWeaponManager();
	if (pWeaponMgr)
	{
		const CWeaponInfo* pWeaponInfo = pWeaponMgr->GetEquippedWeaponInfo();
		if (pWeaponInfo && !pWeaponInfo->GetIsGun() && pWeaponInfo->GetHash() != WEAPONTYPE_UNARMED)
		{
			pWeaponMgr->DropWeapon(pWeaponInfo->GetHash(), true);
		}
	}
}

// Plead with the ped aiming a gun at you.
void CTaskCowerScenario::PlayDirectedAudio()
{
	CPed* pPed = GetPed();

	const CEntity* pTargetEntity = m_Target.GetEntity();

	// Check if the target is a ped.
	if (pTargetEntity && pTargetEntity->GetIsTypePed())
	{
		const CPed* pTargetPed = static_cast<const CPed*>(pTargetEntity);

		// Check if the target ped is armed.
		if (pTargetPed->GetWeaponManager() && pTargetPed->GetWeaponManager()->GetIsArmed())
		{
			pPed->NewSay("GUN_BEG");
		}
	}
}

// Have the cowering ped say initial shocking scream.
void CTaskCowerScenario::PlayInitialAudio()
{
	CPed* pPed = GetPed();

	if (pPed->GetSpeechAudioEntity())
	{
		audDamageStats damageStats;
		damageStats.RawDamage = 0.0f;
		damageStats.DamageReason = AUD_DAMAGE_REASON_SCREAM_PANIC;
		pPed->GetSpeechAudioEntity()->InflictPain(damageStats);
	}
}

// Play a whimpering sound during idle variations.
void CTaskCowerScenario::PlayIdleAudio()
{
	CPed* pPed = GetPed();

	if (pPed->GetSpeechAudioEntity())
	{
		audDamageStats damageStats;
		damageStats.RawDamage = 0.0f;
		damageStats.DamageReason = AUD_DAMAGE_REASON_WHIMPER;
		pPed->GetSpeechAudioEntity()->InflictPain(damageStats);
	}
}

// Play a short scream during flinches.
void CTaskCowerScenario::PlayFlinchAudio()
{
	CPed* pPed = GetPed();

	if (pPed->GetSpeechAudioEntity())
	{
		audDamageStats damageStats;
		damageStats.RawDamage = 0.0f;
		damageStats.DamageReason = AUD_DAMAGE_REASON_SCREAM_SHOCKED;
		pPed->GetSpeechAudioEntity()->InflictPain(damageStats);
	}
}

// Debug functions.
#if !__FINAL
const char * CTaskCowerScenario::GetStaticStateName(s32 iState)
{
	taskAssert( iState >= State_Start && iState <= State_Finish);

	switch (iState)
	{
	case State_Start:
			return "Start";
		case State_StreamIntro:
			return "Stream Intro";
		case State_Intro:
			return "Intro";
		case State_DirectedIntro:
			return "Directed Intro";
		case State_Base:
			return "Base";
		case State_DirectedBase:
			return "Directed Base";
		case State_StreamVariation:
			return "Stream Variation";
		case State_Variation:
			return "Variation";
		case State_StreamFlinch:
			return "StreamFlinch";
		case State_Flinch:
			return "Flinch";
		case State_Outro:
			return "Outro";
		case State_DirectedOutro:
			return "Directed Outro";
		case State_Finish:
			return "Finish";
		default: taskAssert(0); 	
	}

	return "State_Invalid";
}
#endif

//----------------------------------------------------
// CClonedCowerScenarioInfo
//----------------------------------------------------

CClonedCowerScenarioInfo::CClonedCowerScenarioInfo()
{
}

CClonedCowerScenarioInfo::CClonedCowerScenarioInfo(s32 cowerState, s16 scenarioType, CScenarioPoint* pScenarioPoint, bool bIsWorldScenario, int worldScenarioPointIndex)
	: CClonedScenarioFSMInfo( cowerState, scenarioType, pScenarioPoint, bIsWorldScenario, worldScenarioPointIndex)
{
	SetStatusFromMainTaskState(cowerState);
}

void CClonedCowerScenarioInfo::Serialise(CSyncDataBase& serialiser)
{
	CClonedScenarioFSMInfo::Serialise(serialiser);
}

