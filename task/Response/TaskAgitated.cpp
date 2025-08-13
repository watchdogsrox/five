// FILE :    TaskAgitated.h
// CREATED : 10-5-2011

// File header
#include "Task/Response/TaskAgitated.h"

// Game headers
#include "ai/ambient/ConditionalAnims.h"
#include "animation/FacialData.h"
#include "Event/EventShocking.h"
#include "Event/EventWeapon.h"
#include "Event/ShockingEvents.h"
#include "game/Crime.h"
#include "modelinfo/PedModelInfo.h"
#include "Peds/ped.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PedIntelligence/PedAILodManager.h"
#include "Peds/PopZones.h"
#include "scene/world/GameWorld.h"
#include "Task/Combat/TaskThreatResponse.h"
#include "Task/Default/AmbientAudio.h"
#include "task/General/Phone/TaskCallPolice.h"
#include "task/Motion/TaskMotionBase.h"
#include "Task/Movement/TaskMoveFollowEntityOffset.h"
#include "Task/Movement/TaskNavMesh.h"
#include "Task/Response/TaskFlee.h"
#include "Task/Response/Info/AgitatedActions.h"
#include "Task/Response/Info/AgitatedConditions.h"
#include "Task/Response/Info/AgitatedManager.h"
#include "Task/Response/Info/AgitatedResponse.h"
#include "Task/Response/Info/AgitatedSay.h"
#include "Task/Response/Info/AgitatedState.h"
#include "task/Scenario/Types/TaskUseScenario.h"
#include "task/Vehicle/TaskCar.h"
#include "task/Weapons/Gun/TaskVehicleDriveBy.h"

AI_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////
// CTaskAgitated
////////////////////////////////////////////////////////////////////////////////

CTaskAgitated::Tunables CTaskAgitated::sm_Tunables;
IMPLEMENT_RESPONSE_TASK_TUNABLES(CTaskAgitated, 0xe046cddf);

CTaskAgitated::CTaskAgitated(CPed* pAgitator)
: m_pAgitator(pAgitator)
, m_pLeader(NULL)
, m_pResponse(NULL)
, m_pState(NULL)
, m_pSay(NULL)
, m_fTimeInAgitatedState(0.0f)
, m_fTimeSinceAudioEndedForState(0.0f)
, m_fTimeSpentWaitingInState(0.0f)
, m_fDistance(0.0f)
, m_fClosestDistance(FLT_MAX)
, m_fTimeAgitatorHasBeenTalking(0.0f)
, m_fTimeSinceLastLookAt(LARGE_FLOAT)
, m_fTimeSpentWaitingInListen(0.0f)
, m_hContext("Default",0xE4DF46D5)
, m_nLastAgitationApplied(AT_None)
, m_uLastAgitationTime(0)
, m_uTimeBecameHostile(0)
, m_uAgitatedFlags(0)
, m_uRunningFlags(0)
, m_uConfigFlags(0)
, m_bStartedFromScript(false)
{
	SetInternalTaskType(CTaskTypes::TASK_AGITATED);
}

CTaskAgitated::~CTaskAgitated()
{
}

void CTaskAgitated::AddAnger(float fAnger)
{
	//Add the anger.
	GetPed()->GetPedIntelligence()->GetPedMotivation().IncreasePedMotivation(CPedMotivation::ANGRY_STATE, fAnger);
}

void CTaskAgitated::AddFear(float fFear)
{
	//Add the fear.
	GetPed()->GetPedIntelligence()->GetPedMotivation().IncreasePedMotivation(CPedMotivation::FEAR_STATE, fFear);
}

void CTaskAgitated::ApplyAgitation(AgitatedType nType)
{
	//Assert that the type is valid.
	taskAssert(nType > AT_None);

	//Set the flag.
	u32 uFlag = (1 << (int)nType);
	m_uAgitatedFlags.SetFlag(uFlag);

	//Set the last agitation applied.
	m_nLastAgitationApplied = nType;

	//Set the last time we were agitated.
	m_uLastAgitationTime = fwTimer::GetTimeInMilliseconds();

	//If we just became hostile, set the time.
	if(IsHostile() && (m_uTimeBecameHostile == 0))
	{
		m_uTimeBecameHostile = fwTimer::GetTimeInMilliseconds();
	}

	//Check if we have been hit.
	if((nType == AT_Bumped) || (nType == AT_BumpedByVehicle))
	{
		m_uRunningFlags.SetFlag(RF_HasBeenHit);
	}
}

bool CTaskAgitated::CanCallPolice() const
{
	//Grab the ped.
	const CPed* pPed = GetPed();

	//Ensure the ped can make the call.
	if(!CTaskCallPolice::CanMakeCall(*pPed, GetAgitator()))
	{
		return false;
	}

	//Ensure the ped has the audio.
	if(!CTaskCallPolice::DoesPedHaveAudio(*pPed))
	{
		return false;
	}

	return true;
}

bool CTaskAgitated::CanFight() const
{
	//Ensure the ped can fight the agitator.
	if(!CTaskThreatResponse::CanFight(*GetPed(), *GetAgitator()))
	{
		return false;
	}

	return true;
}

bool CTaskAgitated::CanHurryAway() const
{
	//Check if pavement is required to hurry away, and we don't have it.
	if(IsPavementRequiredForHurryAway() && !HasPavement())
	{
		return false;
	}

	return true;
}

bool CTaskAgitated::CanWalkAway() const
{
	return (CanHurryAway());
}

void CTaskAgitated::ChangeResponse(atHashWithStringNotFinal hResponse, atHashWithStringNotFinal hState)
{
	//Ensure the response is valid.
	if(hResponse.IsNull())
	{
		return;
	}
	
	//Find the response.
	const CAgitatedResponse* pResponse = CAgitatedManager::GetInstance().GetResponses().GetResponse(hResponse);
	if(!taskVerifyf(pResponse, "Could not find response: %s.", hResponse.GetCStr()))
	{
		return;
	}
	
	//Ensure the response is changing.
	if(pResponse == m_pResponse)
	{
		return;
	}
	
	//Set the response.
	m_pResponse = pResponse;
	
	//Set the on-foot clip set.
	SetOnFootClipSet();
	
	//Check if the state is invalid.
	if(hState.IsNull())
	{
		//Find the initial state.
		hState = m_pResponse->FindInitialState();
		if(!taskVerifyf(hState.IsNotNull(), "The initial state is invalid for response: %s.", hResponse.GetCStr()))
		{
			return;
		}
	}

	//Change the state.
	ChangeState(hState);
}

void CTaskAgitated::ChangeState(atHashWithStringNotFinal hState)
{
	//Ensure the state is valid.
	if(hState.IsNull())
	{
		return;
	}
	
	//Ensure the state is valid.
	const CAgitatedState* pState = m_pResponse->GetState(hState.GetHash());
	if(!taskVerifyf(pState, "The state: %s is invalid for response: %s.", hState.GetCStr(), FindNameForResponse().GetCStr()))
	{
		return;
	}

	//Check if the state is changing.
	bool bIsStateChanging = (pState != m_pState);
	if(bIsStateChanging)
	{
		//Add logic here to execute ONLY when the state changes.

		//Set the state.
		m_pState = pState;

		//Set the talk response.
		SetTalkResponse();

		//Set the reactions.
		SetReactions();

		//Check if we should clear hostility.
		if(m_pState->GetFlags().IsFlagSet(CAgitatedState::ClearHostility))
		{
			//Clear hostility.
			ClearHostility();
		}
	}
	else
	{
		//Add logic here to execute ONLY when the state DOES NOT change.
	}

	//Add logic here to execute both when the state changes, and when the state does not change.  This will allow us to 'restart' states.

	//Execute the action for the state.
	ExecuteActionForState();

	//Clear the time in the agitated state.
	m_fTimeInAgitatedState = 0.0f;

	//Clear the time since audio ended in this state.
	m_fTimeSinceAudioEndedForState = 0.0f;

	//Clear the time spent waiting in this state.
	m_fTimeSpentWaitingInState = 0.0f;
}

void CTaskAgitated::ChangeTask(CTask* pTask)
{
	//Ensure the task is valid.
	if(!taskVerifyf(pTask, "The task is invalid."))
	{
		return;
	}
	
	//Check if the task is on the secondary task tree.
	if(m_uRunningFlags.IsFlagSet(CTaskAgitated::RF_IsOnSecondaryTaskTree))
	{
		//Give the task to the ped.
		CEventAgitatedAction event(GetAgitator(), pTask);
		GetPed()->GetPedIntelligence()->AddEvent(event);

		//Check if we should force an AI update.
		if(ShouldForceAiUpdateWhenChangingTask())
		{
			GetPed()->SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAIUpdate, true);
		}
	}
	else
	{
		//Start the task.
		SetNewTask(pTask);
	}
}

void CTaskAgitated::ClearAgitation(AgitatedType nType)
{
	//Assert that the type is valid.
	taskAssert(nType > AT_None);

	//Clear the flag.
	u32 uFlag = (1 << (int)nType);
	m_uAgitatedFlags.ClearFlag(uFlag);

	//Check if we just became no longer hostile.
	if(!IsHostile() && (m_uTimeBecameHostile != 0))
	{
		m_uTimeBecameHostile = 0;
	}
}

void CTaskAgitated::ClearHash(atHashWithStringNotFinal hValue)
{
	//Iterate over the hashes.
	for(int i = 0; i < m_aHashes.GetCount(); ++i)
	{
		//Check if the hash matches.
		if(hValue == m_aHashes[i])
		{
			//Clear the hash.
			m_aHashes.DeleteFast(i--);
		}
	}
}

aiTask* CTaskAgitated::Copy() const
{
	//Create the task.
	CTaskAgitated* pTask = rage_new CTaskAgitated(m_pAgitator);
	
	//Copy the data.
	pTask->SetResponseOverride(m_hResponseOverride);

	//Copy the config flags.
	pTask->m_uConfigFlags = m_uConfigFlags;
	
	return pTask;
}

void CTaskAgitated::CleanUp()
{
	//Grab the ped.
	CPed* pPed = GetPed();
	
	//Note that the ped is no longer agitated.
	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_IsAgitated, false);

	if (m_bStartedFromScript)
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_CanBeAgitated,false);
	
	//Reset the on-foot clip set.
	ResetOnFootClipSet();
	
	//Reset the action.
	ResetAction();
	
	//Reset the facial idle clip.
	ResetFacialIdleClip();

	//Clear the talk response.
	ClearTalkResponse();
}

CTask::FSM_Return CTaskAgitated::ProcessPreFSM()
{
	//Grab the ped.
	const CPed* pPed = GetPed();

	//Ensure the ped is not injured.
	if(pPed->IsInjured())
	{
		return FSM_Quit;
	}

	//Ensure the ped can be agitated.
	if(!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_CanBeAgitated))
	{
		return FSM_Quit;
	}

	//Ensure the ped has not disabled agitation.
	if(pPed->GetPedResetFlag(CPED_RESET_FLAG_DisableAgitation))
	{
		return FSM_Quit;
	}

	//Ensure the ped's audio entity is valid.
	if(!pPed->GetSpeechAudioEntity())
	{
		return FSM_Quit;
	}
	
	//Ensure the agitator is valid.
	const CPed* pAgitator = GetAgitator();
	if(!pAgitator)
	{
		return FSM_Quit;
	}

	//Check if the agitator is injured.
	if(pAgitator->IsInjured())
	{
		//Set the flag.
		m_uRunningFlags.SetFlag(RF_HasAgitatorBeenKilled);
	}
	//Ensure the agitator has not been killed.
	else if(m_uRunningFlags.IsFlagSet(RF_HasAgitatorBeenKilled))
	{
		return FSM_Quit;
	}

	//Ensure the agitator has not disabled agitation.
	if(pAgitator->GetPedResetFlag(CPED_RESET_FLAG_DisableAgitation))
	{
		return FSM_Quit;
	}
	
	//Ensure the agitator's audio entity is valid.
	if(!pAgitator->GetSpeechAudioEntity())
	{
		return FSM_Quit;
	}

	//Process agitator flags.
	ProcessAgitatorFlags();
	
	//Process the look at.
	ProcessLookAt();
	
	//Process the on-foot clip set.
	ProcessOnFootClipSet();
	
	//Process the distance.
	ProcessDistance();
	
	//Process the agitator talking.
	ProcessAgitatorTalking();
	
	//Process whether we have been actively provoked.
	ProcessActivelyProvoked();

	//Process the facial idle clip.
	ProcessFacialIdleClip();

	//Process the desired heading.
	ProcessDesiredHeading();

	//Process the armed state.
	ProcessArmed();

	//Process gestures.
	ProcessGestures();

	//Process the leader.
	ProcessLeader();

	//Process kinematic physics.
	ProcessKinematicPhysics();

	//Disable time slicing... this task doesn't work very well with it on.
	if(CPedAILodManager::IsTimeslicingEnabled())
	{
		GetPed()->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);
	}
	
	return FSM_Continue;
}

bool CTaskAgitated::ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
	//Check if the priority is not immediate.
	if(iPriority != ABORT_PRIORITY_IMMEDIATE)
	{
		//Check if the event is valid.
		if(pEvent)
		{
			//Check the response task type.
			switch(((CEvent *)pEvent)->GetResponseTaskType())
			{
				case CTaskTypes::TASK_AMBIENT_LOOK_AT_EVENT:
				{
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

CTask::FSM_Return CTaskAgitated::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	//Keep the sub-task across transitions.
	SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
	
	FSM_Begin
	
		FSM_State(State_Start)
			FSM_OnEnter
				Start_OnEnter();
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_Resume)
			FSM_OnUpdate
				return Resume_OnUpdate();
				
		FSM_State(State_Listen)
			FSM_OnEnter
				Listen_OnEnter();
			FSM_OnUpdate
				return Listen_OnUpdate();
				
		FSM_State(State_React)
			FSM_OnEnter
				React_OnEnter();
			FSM_OnUpdate
				return React_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

void CTaskAgitated::Start_OnEnter()
{
	//Grab the ped.
	CPed* pPed = GetPed();
	
	//Check whether we are on the secondary task tree.
	if(pPed->GetPedIntelligence()->HasTaskSecondary(this))
	{
		//Set the flag.
		m_uRunningFlags.SetFlag(RF_IsOnSecondaryTaskTree);
	}
	
	//Note that the ped is agitated.
	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_IsAgitated, true);

	//Check if we are friendly with the agitator.
	bool bIsFriendly = pPed->GetPedIntelligence()->IsFriendlyWith(*GetAgitator());
	if(bIsFriendly)
	{
		//Set the flag.
		m_uRunningFlags.SetFlag(RF_WasFriendlyWithAgitator);

		//Set the friendly exception.
		SetFriendlyException();
	}
	else
	{
		//Create the event.
		CEventRequestHelpWithConfrontation event(pPed, GetAgitator());

		//Communicate the event.
		CEvent::CommunicateEventInput input;
		input.m_bUseRadioIfPossible = false;
		input.m_fMaxRangeWithoutRadio = 10.0f;
		event.CommunicateEvent(pPed, input);
	}
	
	//Initialize the response.
	InitializeResponse();
}

CTask::FSM_Return CTaskAgitated::Start_OnUpdate()
{
	//Check if we should finish.
	if(ShouldFinish())
	{
		//Finish the task.
		SetState(State_Finish);
	}
	//Check if we should listen.
	else if(ShouldListen())
	{
		//Listen to the agitator.
		SetState(State_Listen);
	}
	else
	{
		//React to the agitation.
		SetState(State_React);
	}
	
	return FSM_Continue;
}

CTask::FSM_Return CTaskAgitated::Resume_OnUpdate()
{
	//Set the state.
	SetState(State_Start);

	return FSM_Continue;
}

void CTaskAgitated::Listen_OnEnter()
{
	//Clear the time spent waiting.
	m_fTimeSpentWaitingInListen = 0.0f;

	//Check if we have no talk response.
	if(HasNoTalkResponse())
	{
		//Set the talk response.
		SetTalkResponse();
	}
}

CTask::FSM_Return CTaskAgitated::Listen_OnUpdate()
{
	//Check if we should finish.
	if(ShouldFinish())
	{
		//Finish the task.
		SetState(State_Finish);
	}
	//Check if we should not listen.
	else if(!ShouldListen())
	{
		//Wait a random amount of time before reacting.
		static dev_float s_fMinTime = 0.0f;
		static dev_float s_fMaxTime = 0.4f;
		float fMinTime = GetPed()->GetRandomNumberInRangeFromSeed(s_fMinTime, s_fMaxTime);
		if(m_fTimeSpentWaitingInListen > fMinTime)
		{
			//React to the agitation.
			SetState(State_React);
		}
		else
		{
			m_fTimeSpentWaitingInListen += GetTimeStep();
		}
	}
	
	return FSM_Continue;
}

void CTaskAgitated::React_OnEnter()
{
}

CTask::FSM_Return CTaskAgitated::React_OnUpdate()
{
	//Check if we should listen.
	if(ShouldListen())
	{
		//Listen to the agitator.
		SetState(State_Listen);
	}
	else
	{
		//Update the timers for the state.
		UpdateTimersForState();
		
		//Update the reactions.
		UpdateReactions();

		//Update the say.
		UpdateSay();
		
		//Check if we should finish.
		if(ShouldFinish())
		{
			//Finish the task.
			SetState(State_Finish);
		}
	}
	
	return FSM_Continue;
}

bool CTaskAgitated::HasAudioEnded() const
{
	//Check if the agitator is talking.
	if(IsAgitatorTalking())
	{
		return false;
	}
	//Check if we are talking.
	else if(IsTalking())
	{
		return false;
	}
	//Check if we have something to say.
	else if(m_pSay)
	{
		return false;
	}

	return true;
}

bool CTaskAgitated::HasBeenHostileFor(float fMinTime, float fMaxTime) const
{
	return (HasBeenHostileFor(GetPed()->GetRandomNumberInRangeFromSeed(fMinTime, fMaxTime)));
}

bool CTaskAgitated::HasBeenHostileFor(float fMinTime) const
{
	//Ensure we are hostile.
	if(!IsHostile())
	{
		return false;
	}

	//Ensure the time has exceeded the minimum.
	taskAssert(m_uTimeBecameHostile != 0);
	float fTimeSinceBecomingHostile = (float)(fwTimer::GetTimeInMilliseconds() - m_uTimeBecameHostile) * 0.001f;
	if(fTimeSinceBecomingHostile < fMinTime)
	{
		return false;
	}

	return true;
}

bool CTaskAgitated::HasFriendsNearby(u8 uMin) const
{
	return (GetPed()->GetPedIntelligence()->CountFriends(10.0f, true) >= uMin);
}

bool CTaskAgitated::HasLeader() const
{
	return (m_pLeader != NULL);
}

bool CTaskAgitated::HasLeaderBeenFightingFor(float fMinTime, float fMaxTime) const
{
	return (HasLeaderBeenFightingFor(GetPed()->GetRandomNumberInRangeFromSeed(fMinTime, fMaxTime)));
}

bool CTaskAgitated::HasLeaderBeenFightingFor(float fMinTime) const
{
	//Ensure the leader is valid.
	if(!m_pLeader)
	{
		return false;
	}

	//Ensure the leader is fighting.
	const CTask* pTask = GetPed()->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COMBAT);
	if(!pTask)
	{
		return false;
	}

	//Ensure the time running exceeds the min time.
	if(pTask->GetTimeRunning() < fMinTime)
	{
		return false;
	}

	return true;
}

bool CTaskAgitated::HasPavement() const
{
#if __DEV
	TUNE_GROUP_BOOL(TASK_AGITATED, bForceHasPavement, false);
	if(bForceHasPavement)
	{
		return true;
	}
#endif

	const bool bHasPavement = (GetPed()->GetNavMeshTracker().GetIsValid() && GetPed()->GetNavMeshTracker().GetNavPolyData().m_bPavement);
	return bHasPavement;
}

bool CTaskAgitated::IsAgitated(AgitatedType nType) const
{
	//Assert that the type is valid.
	taskAssert(nType > AT_None);

	//Check if the flag is set.
	u32 uFlag = (1 << (int)nType);
	return m_uAgitatedFlags.IsFlagSet(uFlag);
}

bool CTaskAgitated::IsAgitatorArmed() const
{
	//Ensure the agitator is valid.
	const CPed* pAgitator = GetAgitator();
	if(!pAgitator)
	{
		return false;
	}

	//Ensure the weapon manager is valid.
	const CPedWeaponManager* pWeaponManager = pAgitator->GetWeaponManager();
	if(!pWeaponManager)
	{
		return false;
	}

	//Ensure the equipped weapon is valid.
	const CWeapon* pWeapon = pWeaponManager->GetEquippedWeapon();
	if(!pWeapon)
	{
		return false;
	}

	//Ensure the weapon is armed.
	if(!pWeapon->GetWeaponInfo()->GetIsArmed())
	{
		return false;
	}

	//Ensure the weapon is violent.
	if(pWeapon->GetWeaponInfo()->GetIsNonViolent())
	{
		return false;
	}

	return true;
}

bool CTaskAgitated::IsAgitatorEnteringVehicle() const
{
	//Ensure the agitator is valid.
	if(!m_pAgitator)
	{
		return false;
	}

	return m_pAgitator->GetIsEnteringVehicle();
}

bool CTaskAgitated::IsAgitatorInOurTerritory() const
{
	//Ensure the agitator is valid.
	if(!m_pAgitator)
	{
		return false;
	}

	//Ensure our last used scenario point is valid.
	const CScenarioPoint* pPoint = GetPed()->GetPedIntelligence()->GetLastUsedScenarioPoint();
	if(!pPoint)
	{
		return false;
	}

	//Ensure the point is marked.
	if(!pPoint->IsFlagSet(CScenarioPointFlags::TerritorialScenario))
	{
		return false;
	}

	//Ensure the target is within the radius.
	ScalarV scDistSq = DistSquared(pPoint->GetWorldPosition(), m_pAgitator->GetTransform().GetPosition());
	ScalarV scMaxDistSq = ScalarVFromF32(square(pPoint->GetRadius()));
	if(IsGreaterThanAll(scDistSq, scMaxDistSq))
	{
		return false;
	}

	return true;
}

bool CTaskAgitated::IsAgitatorMovingAway(float fMinDot) const
{
	//Ensure the agitator is valid.
	const CPed* pAgitator = GetAgitator();
	if(!pAgitator)
	{
		return false;
	}

	//Ensure the agitator is moving.
	Vec3V vAgitatorVelocity = VECTOR3_TO_VEC3V(pAgitator->GetVelocity());
	ScalarV vVelocityMagSquared = DistSquared(vAgitatorVelocity, vAgitatorVelocity);
	if(IsLessThanAll(vVelocityMagSquared, ScalarV(sm_Tunables.m_MovingAwayVelocityMSThreshold)))
	{
		return false;
	}

	//Calculate the direction.
	Vec3V vAgitatorDirection = NormalizeFast(vAgitatorVelocity);

	//Calculate the direction from the ped to the agitator.
	Vec3V vPedToAgitator = Subtract(pAgitator->GetTransform().GetPosition(), GetPed()->GetTransform().GetPosition());
	Vec3V vPedToAgitatorDirection = NormalizeFastSafe(vPedToAgitator, Vec3V(V_ZERO));

	//Ensure the dot is valid.
	ScalarV scDot = Dot(vAgitatorDirection, vPedToAgitatorDirection);
	if(IsLessThanAll(scDot, ScalarVFromF32(fMinDot)))
	{
		return false;
	}

	return true;
}

bool CTaskAgitated::IsAGunPulled() const
{
	return (IsGunPulled(GetPed()) || IsGunPulled(GetAgitator()));
}

bool CTaskAgitated::IsAngry(const float fThreshold) const
{
	return IsAngry(GetPed(), fThreshold);
}

bool CTaskAgitated::IsArgumentative() const
{
	return IsArgumentative(GetPed());
}

bool CTaskAgitated::IsBecomingArmed() const
{
	return IsAgitated(AT_BecomingArmed);
}

bool CTaskAgitated::IsBumped() const
{
	return IsAgitated(AT_Bumped);
}

bool CTaskAgitated::IsBumpedByVehicle() const
{
	return IsAgitated(AT_BumpedByVehicle);
}

bool CTaskAgitated::IsBumpedInVehicle() const
{
	return IsAgitated(AT_BumpedInVehicle);
}

bool CTaskAgitated::IsConfrontational() const
{
	return IsConfrontational(GetPed());
}

bool CTaskAgitated::IsDodged() const
{
	return IsAgitated(AT_Dodged);
}

bool CTaskAgitated::IsDodgedVehicle() const
{
	return IsAgitated(AT_DodgedVehicle);
}

bool CTaskAgitated::IsFacing() const
{
	//Ensure the agitated action is valid.
	CTask* pTask = FindAgitatedActionTaskForPed(*GetPed());
	if(!pTask)
	{
		return false;
	}

	//Ensure the sub-task is valid.
	CTask* pSubTask = pTask->GetSubTask();
	if(!pSubTask)
	{
		return false;
	}

	//Ensure the sub-task is the correct type.
	if(pSubTask->GetTaskType() != CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
	{
		return false;
	}

	//Ensure the move task type is valid.
	const CTaskComplexControlMovement* pTaskComplexControlMovement = static_cast<CTaskComplexControlMovement *>(pSubTask);
	if(pTaskComplexControlMovement->GetMoveTaskType() != CTaskTypes::TASK_MOVE_FACE_TARGET)
	{
		return false;
	}

	return true;
}

bool CTaskAgitated::IsFacing(float fMinDot) const
{
	//Ensure the dot is valid.
	Vec3V vToAgitator = Subtract(GetAgitator()->GetTransform().GetPosition(), GetPed()->GetTransform().GetPosition());
	vToAgitator.SetZf(0.0f);
	Vec3V vToAgitatorDirection = NormalizeFastSafe(vToAgitator, Vec3V(V_ZERO));
	ScalarV scDot = Dot(GetPed()->GetTransform().GetForward(), vToAgitatorDirection);
	ScalarV scMinDot = ScalarVFromF32(fMinDot);
	if(IsLessThanAll(scDot, scMinDot))
	{
		return false;
	}

	return true;
}

bool CTaskAgitated::IsFearful(const float fThreshold) const
{
	return IsFearful(GetPed(), fThreshold);
}

bool CTaskAgitated::IsFlippingOff() const
{
	return (CTaskAimGunVehicleDriveBy::IsFlippingSomeoneOff(*GetPed()));
}

bool CTaskAgitated::IsFollowing() const
{
	return IsAgitated(AT_Following);
}

bool CTaskAgitated::IsFriendlyTalking() const
{
	//Iterate over the nearby peds.
	const CEntityScannerIterator entityList = GetPed()->GetPedIntelligence()->GetNearbyPeds();
	for(const CEntity* pOtherEntity = entityList.GetFirst(); pOtherEntity; pOtherEntity = entityList.GetNext())
	{
		//Grab the other ped.
		const CPed* pOtherPed = static_cast<const CPed *>(pOtherEntity);

		//Ensure the other ped is agitated.
		CTaskAgitated* pTask = CTaskAgitated::FindAgitatedTaskForPed(*pOtherPed);
		if(!pTask)
		{
			continue;
		}

		//Ensure the agitators match.
		if(m_pAgitator != pTask->GetAgitator())
		{
			continue;
		}

		//Ensure the other ped is talking.
		if(!IsPedTalking(*pOtherPed))
		{
			continue;
		}

		return true;
	}

	return false;
}

bool CTaskAgitated::IsGriefing() const
{
	return IsAgitated(AT_Griefing);
}

bool CTaskAgitated::IsGunAimedAt() const
{
	return IsAgitated(AT_GunAimedAt);
}

bool CTaskAgitated::IsHarassed() const
{
	return IsAgitated(AT_Harassed);
}

bool CTaskAgitated::IsHash(atHashWithStringNotFinal hValue) const
{
	//Iterate over the hashes.
	for(int i = 0; i < m_aHashes.GetCount(); ++i)
	{
		//Check if the hash matches.
		if(hValue == m_aHashes[i])
		{
			return true;
		}
	}

	return false;
}

bool CTaskAgitated::IsHonkedAt() const
{
	return IsAgitated(AT_HonkedAt);
}

bool CTaskAgitated::IsHostile() const
{
	return IsAgitated(AT_Hostile);
}

bool CTaskAgitated::IsHurryingAway() const
{
	return (GetPed()->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_SHOCKING_EVENT_HURRYAWAY));
}

bool CTaskAgitated::IsInState(atHashWithStringNotFinal hState) const
{
	//Ensure the state is valid.
	if(hState.IsNull())
	{
		return false;
	}

	//Ensure the state matches.
	if(hState != FindNameForState())
	{
		return false;
	}

	return true;
}

bool CTaskAgitated::IsRevvedAt() const
{
	return IsAgitated(AT_RevvedAt);
}

bool CTaskAgitated::IsSirenOn() const
{
	//Ensure the vehicle is valid.
	const CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
	if(!pVehicle)
	{
		return false;
	}

	//Ensure the siren is on.
	if(!pVehicle->m_nVehicleFlags.GetIsSirenOn())
	{
		return false;
	}

	return true;
}

bool CTaskAgitated::IsTalking() const
{
	return IsPedTalking(*GetPed());
}

void CTaskAgitated::ReportCrime()
{
	//Report the crime.
	CCrime::ReportCrime(CRIME_HASSLE, GetPed(), GetAgitator());
}

void CTaskAgitated::ResetAction()
{
	//Find the agitated action task for the ped.
	CTaskAgitatedAction* pTask = FindAgitatedActionTaskForPed(*GetPed());
	if(!pTask)
	{
		return;
	}

	//Quit the task.
	pTask->GetRunningFlags().SetFlag(CTaskAgitatedAction::AARF_Quit);
}

void CTaskAgitated::ResetClosestDistance()
{
	//Reset the closest distance.
	m_fClosestDistance = LARGE_FLOAT;
}

void CTaskAgitated::Say(const CAgitatedSay& rSay)
{
	//Set the say.
	m_pSay = &rSay;
}

void CTaskAgitated::SetAnger(float fAnger)
{
	//Grab the motivation.
	CPedMotivation& rMotivation = GetPed()->GetPedIntelligence()->GetPedMotivation();

	//Set the anger.
	rMotivation.SetMotivation(CPedMotivation::ANGRY_STATE, fAnger);
}

void CTaskAgitated::SetFear(float fFear)
{
	//Grab the motivation.
	CPedMotivation& rMotivation = GetPed()->GetPedIntelligence()->GetPedMotivation();

	//Set the fear.
	rMotivation.SetMotivation(CPedMotivation::FEAR_STATE, fFear);
}

void CTaskAgitated::SetHash(atHashWithStringNotFinal hValue)
{
	//Check if the hash has already been set.
	if(IsHash(hValue))
	{
		return;
	}

	//Ensure the hashes are not full.
	if(!taskVerifyf(!m_aHashes.IsFull(), "The hashes are full."))
	{
		return;
	}

	//Add the hash.
	m_aHashes.Append() = hValue;
}

void CTaskAgitated::SetMinAnger(float fMinAnger)
{
	//Grab the motivation.
	CPedMotivation& rMotivation = GetPed()->GetPedIntelligence()->GetPedMotivation();

	//Clamp the anger.
	float fAnger = rMotivation.GetMotivation(CPedMotivation::ANGRY_STATE);
	fAnger = Clamp(fAnger, fMinAnger, LARGE_FLOAT);

	//Set the anger.
	rMotivation.SetMotivation(CPedMotivation::ANGRY_STATE, fAnger);
}

void CTaskAgitated::SetMinFear(float fMinFear)
{
	//Grab the motivation.
	CPedMotivation& rMotivation = GetPed()->GetPedIntelligence()->GetPedMotivation();

	//Clamp the fear.
	float fFear = rMotivation.GetMotivation(CPedMotivation::FEAR_STATE);
	fFear = Clamp(fFear, fMinFear, LARGE_FLOAT);

	//Set the anger.
	rMotivation.SetMotivation(CPedMotivation::FEAR_STATE, fFear);
}

void CTaskAgitated::SetMaxAnger(float fMaxAnger)
{
	//Grab the motivation.
	CPedMotivation& rMotivation = GetPed()->GetPedIntelligence()->GetPedMotivation();

	//Clamp the anger.
	float fAnger = rMotivation.GetMotivation(CPedMotivation::ANGRY_STATE);
	fAnger = Clamp(fAnger, -LARGE_FLOAT, fMaxAnger);

	//Set the anger.
	rMotivation.SetMotivation(CPedMotivation::ANGRY_STATE, fAnger);
}

void CTaskAgitated::SetMaxFear(float fMaxFear)
{
	//Grab the motivation.
	CPedMotivation& rMotivation = GetPed()->GetPedIntelligence()->GetPedMotivation();

	//Clamp the anger.
	float fFear = rMotivation.GetMotivation(CPedMotivation::FEAR_STATE);
	fFear = Clamp(fFear, -LARGE_FLOAT, fMaxFear);

	//Set the anger.
	rMotivation.SetMotivation(CPedMotivation::FEAR_STATE, fFear);
}

void CTaskAgitated::TurnOnSiren(float fTime)
{
	//Grab the ped.
	CPed* pPed = GetPed();

	//Find the in-vehicle task.
	CTaskInVehicleBasic* pTask = static_cast<CTaskInVehicleBasic *>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_IN_VEHICLE_BASIC));
	if(!pTask)
	{
		return;
	}

	//Request the siren.
	pTask->RequestSiren(fTime);
}

bool CTaskAgitated::WasLeaderHit() const
{
	//Ensure the leader is valid.
	if(!GetLeader())
	{
		return false;
	}

	//Check if we have been hit.
	if(GetLeader()->HasBeenDamagedByEntity(GetAgitator()))
	{
		return true;
	}

	//Check if the flag is set.
	if(m_uRunningFlags.IsFlagSet(RF_HasBeenHit))
	{
		return true;
	}

	return false;
}

bool CTaskAgitated::WasRecentlyBumpedWhenStill(float fMaxTime) const
{
	//Ensure the time is valid.
	if(CTimeHelpers::GetTimeSince(GetPed()->GetPedIntelligence()->GetLastTimeBumpedWhenStill()) > (u32)(fMaxTime * 1000.0f))
	{
		return false;
	}

	return true;
}

bool CTaskAgitated::WasUsingTerritorialScenario() const
{
	//Ensure the last used scenario point is valid.
	const CScenarioPoint* pPoint = GetPed()->GetPedIntelligence()->GetLastUsedScenarioPoint();
	if(!pPoint)
	{
		return false;
	}

	return (pPoint->IsFlagSet(CScenarioPointFlags::TerritorialScenario));
}

bool CTaskAgitated::IsInjured() const
{
	return IsInjured(GetPed());
}

bool CTaskAgitated::IsInsulted() const
{
	return IsAgitated(AT_Insulted);
}

bool CTaskAgitated::IsIntervene() const
{
	return IsAgitated(AT_Intervene);
}

bool CTaskAgitated::IsIntimidate() const
{
	return IsAgitated(AT_Intimidate);
}

bool CTaskAgitated::IsLastAgitationApplied(AgitatedType nType) const
{
	return (nType == m_nLastAgitationApplied);
}

bool CTaskAgitated::IsLeaderUsingResponse(atHashWithStringNotFinal hResponse) const
{
	//Ensure the leader is valid.
	const CPed* pLeader = GetLeader();
	if(!pLeader)
	{
		return false;
	}

	//Ensure the task is valid.
	const CTaskAgitated* pTask = CTaskAgitated::FindAgitatedTaskForPed(*pLeader);
	if(!pTask)
	{
		return false;
	}

	//Ensure the leader is using the response.
	if(hResponse != pTask->FindNameForResponse())
	{
		return false;
	}

	return true;
}

bool CTaskAgitated::IsLoitering() const
{
	return IsAgitated(AT_Loitering);
}

bool CTaskAgitated::IsMeleeLockOn() const
{
	return IsAgitated(AT_MeleeLockon);
}

bool CTaskAgitated::IsTerritoryIntruded() const
{
	return IsAgitated(AT_TerritoryIntruded);
}

bool CTaskAgitated::IsOutsideClosestDistance(const float fDistance) const
{
	//Ensure the distance outside the closest has exceeded the threshold.
	if((m_fDistance - m_fClosestDistance) < fDistance)
	{
		return false;
	}
	
	return true;
}

bool CTaskAgitated::IsOutsideDistance(const float fDistance) const
{
	//Ensure the distance has exceeded the threshold.
	if(m_fDistance < fDistance)
	{
		return false;
	}
	
	return true;
}

bool CTaskAgitated::IsProvoked(bool bIgnoreHostility) const
{
	u32 uMask = (!bIgnoreHostility ? ~(u32)0 : ~(u32)(1 << AT_Hostile));
	return ((m_uAgitatedFlags.GetAllFlags() & uMask) != 0);
}

bool CTaskAgitated::IsRanting() const
{
	return IsAgitated(AT_Ranting);
}

CTaskAgitated* CTaskAgitated::CreateDueToEvent(const CPed* pPed, const CEvent& rEvent)
{
	//Ensure the ped is valid.
	if(!pPed)
	{
		return NULL;
	}

	//Ensure the agitator is valid.
	CPed* pAgitator = GetAgitatorForEvent(rEvent);
	if(!pAgitator)
	{
		return NULL;
	}

	//Ensure the ped is not the agitator.
	if(!aiVerifyf(pPed != pAgitator, "The ped is agitated with themselves due to event: %d.", rEvent.GetEventType()))
	{
		return NULL;
	}

	//Ensure the agitator is a player.
	if(!pAgitator->IsPlayer())
	{
		return NULL;
	}

	//Ensure the type is valid.
	AgitatedType nType = GetTypeForEvent(*pPed, rEvent);
	if(nType == AT_None)
	{
		return NULL;
	}

	//Ensure the ped is not already agitated.
	if(!aiVerifyf(!FindAgitatedTaskForPed(*pPed), "The ped is already agitated, but this event doesn't care: %d.", rEvent.GetEventType()))
	{
		return NULL;
	}

	//Create the task.
	CTaskAgitated* pTask = rage_new CTaskAgitated(pAgitator);

	//Apply the agitation.
	pTask->ApplyAgitation(nType);

	return pTask;
}

CTaskAgitatedAction* CTaskAgitated::FindAgitatedActionTaskForPed(const CPed& rPed)
{
	//Check if the task is on the primary tree.
	CTask* pTask = rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AGITATED_ACTION);
	if(!pTask)
	{
		return NULL;
	}

	return static_cast<CTaskAgitatedAction *>(pTask);
}

CTaskAgitated* CTaskAgitated::FindAgitatedTaskForPed(const CPed& rPed)
{
	//Ensure the ped is agitated.
	if(!rPed.GetPedConfigFlag(CPED_CONFIG_FLAG_IsAgitated))
	{
		return NULL;
	}

	//Check if the task is on the secondary tree. This is searched first, because it is most common.
	CTask* pTask = rPed.GetPedIntelligence()->FindTaskActiveByTreeAndType(PED_TASK_TREE_SECONDARY, CTaskTypes::TASK_AGITATED);
	if(!pTask)
	{
		//Check if the task is on the primary tree.
		pTask = rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AGITATED);
		if(!pTask)
		{
			return NULL;
		}
	}
	
	return static_cast<CTaskAgitated *>(pTask);
}

CPed* CTaskAgitated::GetAgitatorForEvent(const CEvent& rEvent)
{
	//Check the event type.
	switch(rEvent.GetEventType())
	{
		case EVENT_AGITATED:
		{
			return static_cast<const CEventAgitated &>(rEvent).GetAgitator();
		}
		case EVENT_RESPONDED_TO_THREAT:
		case EVENT_SHOUT_TARGET_POSITION:
		{
			return rEvent.GetTargetPed();
		}
		default:
		{
			return rEvent.GetSourcePed();
		}
	}
}

AgitatedType CTaskAgitated::GetTypeForEvent(const CPed& rPed, const CEvent& rEvent)
{
	//Check the event type.
	switch(rEvent.GetEventType())
	{
		case EVENT_ACQUAINTANCE_PED_WANTED:
		case EVENT_DAMAGE:
		case EVENT_RESPONDED_TO_THREAT:
		case EVENT_SHOCKING_POTENTIAL_BLAST:
		case EVENT_SHOT_FIRED:
		case EVENT_SHOT_FIRED_BULLET_IMPACT:
		case EVENT_SHOT_FIRED_WHIZZED_BY:
		case EVENT_SHOUT_TARGET_POSITION:
		case EVENT_VEHICLE_DAMAGE_WEAPON:
		{
			return AT_Hostile;
		}
		case EVENT_AGITATED:
		{
			return static_cast<const CEventAgitated &>(rEvent).GetType();
		}
		case EVENT_FRIENDLY_AIMED_AT:
		case EVENT_GUN_AIMED_AT:
		case EVENT_SHOCKING_SEEN_WEAPON_THREAT:
		{
			//Check if the ped is friendly with the agitator.
			const CPed* pAgitator = GetAgitatorForEvent(rEvent);
			if(pAgitator && rPed.GetPedIntelligence()->IsFriendlyWith(*pAgitator))
			{
				return AT_GunAimedAt;
			}
			else
			{
				return AT_Hostile;
			}
		}
		case EVENT_MELEE_ACTION:
		{
			return AT_MeleeLockon;
		}
		case EVENT_SHOCKING_SEEN_CAR_STOLEN:
		{
			//Check if our vehicle was stolen.
			if(rPed.GetMyVehicle() && (rPed.GetMyVehicle() == static_cast<const CEventShockingSeenCarStolen &>(rEvent).GetOtherEntity()))
			{
				return AT_Hostile;
			}

			return AT_Intimidate;
		}
		case EVENT_SHOCKING_SEEN_MELEE_ACTION:
		{
			//Check if we are law enforcement.
			if(rPed.IsLawEnforcementPed())
			{
				//Note: This is considered griefing even though it seems pretty aggressive.
				//		This is to handle the case where the player swings and misses.
				//		If they make contact, the injured/damage events will take over to
				//		provide the appropriate response.

				return AT_Griefing;
			}
			else
			{
				return AT_Hostile;
			}
		}
		case EVENT_PED_ON_CAR_ROOF:
		case EVENT_SHOCKING_PROPERTY_DAMAGE:
		case EVENT_SHOCKING_BROKEN_GLASS:
		{
			return AT_Griefing;
		}
		default:
		{
			return AT_Intimidate;
		}
	}
}

bool CTaskAgitated::HasContext(const CPed& rPed, atHashWithStringNotFinal hContext)
{
	return (GetContext(rPed, hContext) != NULL);
}

bool CTaskAgitated::IsAggressive(const CPed& rPed)
{
	//Ensure the personality is valid.
	const CAgitatedPersonality* pPersonality = GetPersonality(rPed);
	if(!pPersonality)
	{
		return false;
	}

	return (pPersonality->GetFlags().IsFlagSet(CAgitatedPersonality::IsAggressive));
}

bool CTaskAgitated::ShouldIgnoreEvent(const CPed& rPed, const CEvent& rEvent)
{
	//Ensure the task is valid.
	CTaskAgitated* pTask = FindAgitatedTaskForPed(rPed);
	if(!pTask)
	{
		return false;
	}

	//Ensure the agitator is valid.
	const CPed* pAgitator = GetAgitatorForEvent(rEvent);
	if(!pAgitator)
	{
		return false;
	}

	//Ensure the agitator matches.
	if(pTask->GetAgitator() != pAgitator)
	{
		return false;
	}

	//Get the type for the event.
	AgitatedType nType = GetTypeForEvent(rPed, rEvent);

	//Check if the type is hostile.
	if(nType == AT_Hostile)
	{
		//Check if the agitator is injured.
		if(pAgitator->IsInjured())
		{
			nType = AT_None;
		}
		//Check if we were friendly with the agitator.
		else if(pTask->WasFriendlyWithAgitator())
		{
			//Check the event type.
			switch(rEvent.GetEventType())
			{
				case EVENT_GUN_AIMED_AT:
				{
					nType = AT_None;
					break;
				}
				default:
				{
					break;
				}
			}
		}
	}

	//Check if the type is valid.
	if(nType != AT_None)
	{
		pTask->ApplyAgitation(nType);
	}

	return true;
}

bool CTaskAgitated::CanSay() const
{
	//Ensure the say is valid.
	if(!m_pSay)
	{
		return false;
	}
	
	//Check if we can't interrupt.
	bool bCanInterrupt = m_pSay->GetFlags().IsFlagSet(CAgitatedSay::CanInterrupt);
	if(!bCanInterrupt)
	{
		//Ensure the agitator is not talking.
		if(IsAgitatorTalking())
		{
			return false;
		}
	}

	//Check if we should wait for existing audio to finish.
	if(m_pSay->GetFlags().IsFlagSet(CAgitatedSay::WaitForExistingAudioToFinish))
	{
		//Ensure we are not talking.
		if(IsTalking())
		{
			return false;
		}
	}

	//Check if we should wait until facing.
	if(m_pSay->GetFlags().IsFlagSet(CAgitatedSay::WaitUntilFacing))
	{
		//Ensure we are facing.
		static dev_float s_fMinDot = 0.85f;
		if(!IsFacing(s_fMinDot))
		{
			return false;
		}
	}
	
	return true;
}

void CTaskAgitated::ClearFriendlyException()
{
	//Clear the friendly exception.
	GetPed()->GetPedIntelligence()->SetFriendlyException(NULL);
}

void CTaskAgitated::ClearHostility()
{
	//Clear the flag.
	ClearAgitation(AT_Hostile);
}

void CTaskAgitated::ClearTalkResponse()
{
	//Clear the talk response.
	GetPed()->GetPedIntelligence()->ClearTalkResponse();
}

void CTaskAgitated::ExecuteAction(const CAgitatedAction& rAction)
{
	//Create the action context.
	CAgitatedActionContext context;
	context.m_Task = this;

	//Execute the action.
	rAction.Execute(context);
}

void CTaskAgitated::ExecuteActionForReaction(const CAgitatedReaction& rReaction)
{
	//Find the first valid action for the reaction.
	const CAgitatedAction* pAction = FindFirstValidActionForReaction(rReaction);
	if(!pAction)
	{
		return;
	}

	//Execute the action.
	ExecuteAction(*pAction);
}

void CTaskAgitated::ExecuteActionForState()
{
	//Ensure the state is valid.
	if(!m_pState)
	{
		return;
	}

	//Execute the action.
	ExecuteActionForState(*m_pState);
}

void CTaskAgitated::ExecuteActionForState(const CAgitatedState& rState)
{
	//Find the first valid action for the state.
	const CAgitatedAction* pAction = FindFirstValidActionForState(rState);
	if(!pAction)
	{
		return;
	}

	//Execute the action.
	ExecuteAction(*pAction);
}

atHashWithStringNotFinal CTaskAgitated::FindNameForReaction(const CAgitatedReaction& rReaction) const
{
	//Check if the response is valid.
	if(m_pResponse)
	{
		return m_pResponse->FindNameForReaction(&rReaction);
	}

	return atHashWithStringNotFinal::Null();
}

atHashWithStringNotFinal CTaskAgitated::FindNameForResponse() const
{
	//Check if the response is valid.
	if(m_pResponse)
	{
		return CAgitatedManager::GetInstance().GetResponses().FindNameForResponse(m_pResponse);
	}

	return atHashWithStringNotFinal::Null();
}

atHashWithStringNotFinal CTaskAgitated::FindNameForState() const
{
	//Check if the response and state are valid.
	if(m_pResponse && m_pState)
	{
		return m_pResponse->FindNameForState(m_pState);
	}

	return atHashWithStringNotFinal::Null();
}

atHashWithStringNotFinal CTaskAgitated::GetResponse() const
{
#if __BANK
	TUNE_GROUP_BOOL(TASK_AGITATED, bForceCoward, false);
	if(bForceCoward)
	{
		static atHashWithStringNotFinal s_hCoward("Coward");
		return s_hCoward;
	}
#endif

#if __BANK
	TUNE_GROUP_BOOL(TASK_AGITATED, bForceConfront, false);
	if(bForceConfront)
	{
		static atHashWithStringNotFinal s_hConfront("Confront");
		return s_hConfront;
	}
#endif

	//Check if the override is valid.
	if(m_hResponseOverride.IsNotNull())
	{
		return m_hResponseOverride;
	}

	//Ensure the context is valid.
	const CAgitatedContext* pContext = GetContext(*GetPed(), m_hContext);
	if(!pContext)
	{
		return atHashWithStringNotFinal::Null();
	}

	//Check if the response is valid.
	atHashWithStringNotFinal hResponse = pContext->GetResponse();
	if(hResponse.IsNotNull())
	{
		return hResponse;
	}

	//Create the condition context.
	CAgitatedConditionContext context;
	context.m_Task = this;

	//Iterate over the situations.
	const atArray<CAgitatedSituation>& rSituations = pContext->GetSituations();
	for(int i = 0; i < rSituations.GetCount(); ++i)
	{
		//Grab the situation.
		const CAgitatedSituation& rSituation = rSituations[i];

		//Check if the condition is valid.
		const CAgitatedCondition* pCondition = rSituation.GetCondition();
		if(pCondition)
		{
			//Ensure the condition has been satisfied.
			if(!pCondition->Check(context))
			{
				continue;
			}
		}

		return rSituation.GetResponse();
	}

	return atHashWithStringNotFinal::Null();
}

bool CTaskAgitated::HasBeenActivelyProvokedThisFrame() const
{
	return (IsInsulted() || IsBumped() || IsBumpedByVehicle() || IsBumpedInVehicle() ||
		IsDodged() || IsDodgedVehicle() || IsHonkedAt() || IsRevvedAt() || IsHostile());
}

bool CTaskAgitated::HasNoTalkResponse() const
{
	return (!GetPed()->GetPedIntelligence()->GetTalkResponse().IsValid());
}

void CTaskAgitated::InitializeResponse()
{
	//Get the response.
	atHashWithStringNotFinal hResponse = GetResponse();
	
	//Change the response.
	ChangeResponse(hResponse, atHashWithStringNotFinal::Null());
}

bool CTaskAgitated::IsAgitatorBeingShoved() const
{
	//Ensure the agitator is valid.
	const CPed* pAgitator = GetAgitator();
	if(!pAgitator)
	{
		return false;
	}

	return (pAgitator->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_SHOVED) != NULL);
}

bool CTaskAgitated::IsAgitatorTalking() const
{
	//Ensure the agitator is valid.
	const CPed* pAgitator = GetAgitator();
	if(!pAgitator)
	{
		return false;
	}

	return IsPedTalking(*pAgitator);
}

bool CTaskAgitated::IsConfronting() const
{
	return (GetPed()->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_CONFRONT) != NULL);
}

bool CTaskAgitated::IsEnteringOrExitingScenario() const
{
	//Ensure the task is valid.
	const CTaskUseScenario* pTask = static_cast<CTaskUseScenario *>(
		GetPed()->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_USE_SCENARIO));
	if(!pTask)
	{
		return false;
	}

	//Ensure the task is entering or exiting.
	if(!pTask->IsEntering() && !pTask->IsExiting())
	{
		return false;
	}

	return true;
}

bool CTaskAgitated::IsGettingUp() const
{
	return (GetPed()->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_GET_UP, true));
}

bool CTaskAgitated::IsInCombat() const
{
	return (GetPed()->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT, true));
}

bool CTaskAgitated::IsIntimidating() const
{
	return (GetPed()->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_INTIMIDATE) != NULL);
}

bool CTaskAgitated::IsListening() const
{
	return (GetState() == State_Listen);
}

bool CTaskAgitated::IsPavementRequiredForHurryAway() const
{
	//Get the position.
	Vector3 vPosition = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition());

	//Ensure the pop zone is valid.
	CPopZone* pPopZone = CPopZones::FindSmallestForPosition(&vPosition, ZONECAT_ANY, ZONETYPE_ANY);
	if(!pPopZone)
	{
		return true;
	}

	return (!pPopZone->m_bLetHurryAwayLeavePavements);
}

bool CTaskAgitated::IsPedTalking(const CPed& rPed) const
{
	return rPed.GetSpeechAudioEntity()->IsAmbientSpeechPlaying();
}

bool CTaskAgitated::IsShoving() const
{
	return (GetPed()->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_SHOVE) != NULL);
}

bool CTaskAgitated::IsWaiting() const
{
	//Ensure the audio has ended.
	if(!HasAudioEnded())
	{
		return false;
	}

	//Ensure the ped is not getting up.
	if(IsGettingUp())
	{
		return false;
	}

	//Ensure the ped is not avoiding.
	if(IsAvoiding())
	{
		return false;
	}

	//Ensure the ped is not shoving.
	if(IsShoving())
	{
		return false;
	}

	//Ensure the ped is not entering or exiting a scenario.
	if(IsEnteringOrExitingScenario())
	{
		return false;
	}

	//Ensure the agitator is not being shoved.
	if(IsAgitatorBeingShoved())
	{
		return false;
	}

	//Ensure we are not confronting, and in motion.
	if(IsConfronting() && GetPed()->IsInMotion())
	{
		return false;
	}

	//Ensure we are facing in the correct direction.
	static dev_float s_fMinDot = 0.85f;
	if(IsFacing() && !IsFacing(s_fMinDot))
	{
		return false;
	}

	return true;
}

void CTaskAgitated::LookAtAgitator(int iTime)
{
	//Look at the agitator.
	GetPed()->GetIkManager().LookAt(0, GetAgitator(),
		iTime, BONETAG_HEAD, NULL, LF_WHILE_NOT_IN_FOV, 500, 500, CIkManager::IK_LOOKAT_HIGH);
}

void CTaskAgitated::ProcessActivelyProvoked()
{
	//Ensure the flag is not set.
	if(m_uRunningFlags.IsFlagSet(RF_HasBeenActivelyProvoked))
	{
		return;
	}
	
	//Ensure we have been actively provoked this frame.
	if(!HasBeenActivelyProvokedThisFrame())
	{
		return;
	}
	
	//Set the flag.
	m_uRunningFlags.SetFlag(RF_HasBeenActivelyProvoked);
}

void CTaskAgitated::ProcessAgitatorFlags()
{
	//Check if the agitator is injured.
	if(GetAgitator()->IsInjured())
	{
		//Clear hostility.
		ClearHostility();
	}
}

void CTaskAgitated::ProcessAgitatorTalking()
{
	//Check if the agitator is talking.
	if(IsAgitatorTalking())
	{
		//Increase the time the agitator has been talking.
		m_fTimeAgitatorHasBeenTalking += GetTimeStep();
	}
	else
	{
		//Clear the time the agitator has been talking.
		m_fTimeAgitatorHasBeenTalking = 0.0f;
	}
}

void CTaskAgitated::ProcessArmed()
{
	//Update the flags.
	bool bWasAgitatorArmedLastFrame	= m_uRunningFlags.IsFlagSet(RF_IsAgitatorArmed);
	bool bIsAgitatorArmedThisFrame	= IsAgitatorArmed();
	m_uRunningFlags.ChangeFlag(RF_IsAgitatorArmed, bIsAgitatorArmedThisFrame);

	//Check if the agitator is becoming armed.
	if(!bWasAgitatorArmedLastFrame && bIsAgitatorArmedThisFrame)
	{
		//If the agitator starts armed, ignore it.
		if(GetTimeRunning() > 0.25f)
		{
			//Apply the agitation.
			ApplyAgitation(AT_BecomingArmed);
		}
	}
}

void CTaskAgitated::ProcessDesiredHeading()
{
	//Check if we should process the desired heading.
	if(!ShouldProcessDesiredHeading())
	{
		return;
	}

	//Set the desired heading.
	GetPed()->SetDesiredHeading(VEC3V_TO_VECTOR3(GetAgitator()->GetTransform().GetPosition()));
}

void CTaskAgitated::ProcessDistance()
{
	//Grab the ped.
	const CPed* pPed = GetPed();
	
	//Grab the agitator.
	const CPed* pAgitator = GetAgitator();
	
	//Grab the ped position.
	Vec3V vPedPosition = pPed->GetTransform().GetPosition();
	
	//Grab the agitator position.
	Vec3V vAgitatorPosition = pAgitator->GetTransform().GetPosition();
	
	//Calculate the distance.
	m_fDistance = Dist(vPedPosition, vAgitatorPosition).Getf();
	
	//Update the closest distance.
	m_fClosestDistance = Min(m_fClosestDistance, m_fDistance);
}

void CTaskAgitated::ProcessFacialIdleClip()
{
	//Grab the ped.
	CPed* pPed = GetPed();

	//Ensure the facial data is valid.
	CFacialDataComponent* pFacialData = pPed->GetFacialData();
	if(!pFacialData)
	{
		return;
	}

	//Grab the anger.
	float fAnger = pPed->GetPedIntelligence()->GetPedMotivation().GetMotivation(CPedMotivation::ANGRY_STATE);

	//Grab the fear.
	float fFear = pPed->GetPedIntelligence()->GetPedMotivation().GetMotivation(CPedMotivation::FEAR_STATE);

	//Check if we are angry.
	if(fAnger > 0.0f && fAnger >= fFear)
	{
		pFacialData->RequestFacialIdleClip(CFacialDataComponent::FICT_Angry);
	}
	//Check if we are scared.
	else if(fFear > 0.0f)
	{
		pFacialData->RequestFacialIdleClip(CFacialDataComponent::FICT_Stressed);
	}
}

void CTaskAgitated::ProcessGestures()
{
	//Ensure we are not intimidating.
	if(IsIntimidating())
	{
		return;
	}

	//Ensure we are not shoving.
	if(IsShoving())
	{
		return;
	}

	//Ensure we are not in combat.
	if(IsInCombat())
	{
		return;
	}

	//Ensure we are not getting up.
	if(IsGettingUp())
	{
		return;
	}

	//Ensure we are not flipping off.
	if(IsFlippingOff())
	{
		return;
	}

	//Allow gestures.
	GetPed()->SetGestureAnimsAllowed(true);
}

void CTaskAgitated::ProcessKinematicPhysics()
{
	//Check if we should use kinematic physics.
	bool bUseKinematicPhysics = (IsFacing() || IsConfronting());
	GetPed()->SetPedConfigFlag(CPED_CONFIG_FLAG_UseKinematicModeWhenStationary, bUseKinematicPhysics);
}

void CTaskAgitated::ProcessLeader()
{
	//Ensure the response is valid.
	if(!m_pResponse)
	{
		return;
	}

	//Ensure the leader should be processed.
	if(!m_pResponse->GetFlags().IsFlagSet(CAgitatedResponse::ProcessLeader))
	{
		return;
	}

	//Calculate the max distance.
	static float s_fMaxDist = 15.0f;
	ScalarV scMaxDistSq = ScalarVFromF32(square(s_fMaxDist));

	//Keep track of the best task.
	CTaskAgitated* pBestTask = this;

	//Scan the nearby peds.
	const CEntityScannerIterator entityList = GetPed()->GetPedIntelligence()->GetNearbyPeds();
	for(const CEntity* pOtherEntity = entityList.GetFirst(); pOtherEntity; pOtherEntity = entityList.GetNext())
	{
		//Grab the other ped.
		const CPed* pOtherPed = static_cast<const CPed *>(pOtherEntity);

		//Ensure the other ped is close.
		ScalarV scDistSq = DistSquared(GetPed()->GetTransform().GetPosition(), pOtherPed->GetTransform().GetPosition());
		if(IsGreaterThanAll(scDistSq, scMaxDistSq))
		{
			break;
		}

		//Ensure the other ped is agitated.
		CTaskAgitated* pTask = CTaskAgitated::FindAgitatedTaskForPed(*pOtherPed);
		if(!pTask)
		{
			continue;
		}

		//Ensure the agitators match.
		if(m_pAgitator != pTask->GetAgitator())
		{
			continue;
		}

		//Ensure the responses match.
		if(m_pResponse != pTask->m_pResponse)
		{
			continue;
		}

		//Check if the task is better.
		if(pTask->GetLastAgitationTime() > pBestTask->GetLastAgitationTime())
		{
			pBestTask = pTask;
		}
		else if((pTask->GetLastAgitationTime() == pBestTask->GetLastAgitationTime()) &&
			(pTask->GetPed() < pBestTask->GetPed()))
		{
			pBestTask = pTask;
		}
	}

	//Set the leader.
	if(pBestTask == this)
	{
		m_pLeader = NULL;
	}
	else
	{
		m_pLeader = pBestTask->GetPed();
	}
}

void CTaskAgitated::ProcessLookAt()
{
	//Update the time since the last look at.
	m_fTimeSinceLastLookAt += GetTimeStep();
	
	//Check if the time has exceeded the threshold.
	float fTimeBetweenLookAts = sm_Tunables.m_TimeBetweenLookAts;
	if(m_fTimeSinceLastLookAt < fTimeBetweenLookAts)
	{
		return;
	}
	
	//Look at the agitator.
	static dev_float s_fExtraTime = 5.0f;
	float fTimeToLookAt = fTimeBetweenLookAts + s_fExtraTime;
	LookAtAgitator((int)(fTimeToLookAt * 1000.0f));
}

void CTaskAgitated::ProcessOnFootClipSet()
{
	//Ensure we have not overridden the on-foot clip set.
	if(m_uRunningFlags.IsFlagSet(RF_OverrodeOnFootClipSet))
	{
		return;
	}
	
	//Ensure the clip set has loaded.
	if(!m_OnFootClipSetRequestHelper.Request())
	{
		return;
	}
	
	//Grab the clip set id.
	fwMvClipSetId clipSetId = m_OnFootClipSetRequestHelper.GetClipSetId();
	
	//Set the on-foot clip set.
	GetPed()->GetPrimaryMotionTask()->SetOnFootClipSet(clipSetId);
	
	//Note that we have overridden the on-foot clip set.
	m_uRunningFlags.SetFlag(RF_OverrodeOnFootClipSet);
}

void CTaskAgitated::ResetAgitatedFlags()
{
	//Generate the mask.
	u32 uMask = 0;
	uMask |= (1 << AT_Hostile);

	//Reset the flags.
	u32 uFlags = m_uAgitatedFlags.GetAllFlags();
	uFlags &= uMask;
	m_uAgitatedFlags.SetAllFlags(uFlags);
}

void CTaskAgitated::ResetFacialIdleClip()
{
	//Grab the ped.
	CPed* pPed = GetPed();
	
	//Ensure the facial data is valid.
	CFacialDataComponent* pFacialData = pPed->GetFacialData();
	if(!pFacialData)
	{
		return;
	}
	
	//Reset the facial idle clip.
	pFacialData->ResetFacialIdleAnimation(pPed);
}

void CTaskAgitated::ResetOnFootClipSet()
{
	//Ensure the flag is not set.
	if(m_uRunningFlags.IsFlagSet(RF_DontResetOnFootClipSet))
	{
		return;
	}
	
	//Release the clip set.
	m_OnFootClipSetRequestHelper.Release();
	
	//Ensure we have overridden the on-foot clip set.
	if(!m_uRunningFlags.IsFlagSet(RF_OverrodeOnFootClipSet))
	{
		return;
	}
	
	//Ensure the ped is valid.
	CPed* pPed = GetPed();
	if(!pPed)
	{
		return;
	}
	
	//Reset the on-foot clip set.
	pPed->GetPrimaryMotionTask()->ResetOnFootClipSet(true);
	
	//Note that we have no longer overridden the on-foot clip set.
	m_uRunningFlags.ClearFlag(RF_OverrodeOnFootClipSet);
}

void CTaskAgitated::ResetReactions()
{
	//Reset the reactions.
	m_aReactions.Reset();
}

void CTaskAgitated::Say()
{
	//Grab the ped.
	CPed* pPed = GetPed();

	//Check if we are not already talking.
	//If we are, some other system probably kicked off an audio line,
	//due to some special event logic or something.  In this case, ignore ours.
	if(!IsTalking())
	{
		//Keep track of whether we said any audio.
		bool bSaidAudio = false;

		//Check if the audio is valid.
		const CAmbientAudio& rAudio = m_pSay->GetAudio();
		if(rAudio.IsValid())
		{
			//Say the audio.
			bSaidAudio = CTalkHelper::Talk(*pPed, *GetAgitator(), rAudio);
		}
		else
		{
			//Grab the audios.
			const atArray<CAmbientAudio>& rAudios = m_pSay->GetAudios();

			//Iterate over the audios.
			SemiShuffledSequence shuffler(rAudios.GetCount());
			for(int i = 0; i < shuffler.GetNumElements(); ++i)
			{
				//Get the index.
				int iIndex = shuffler.GetElement(i);

				//Say the audio.
				bSaidAudio = CTalkHelper::Talk(*pPed, *GetAgitator(), rAudios[iIndex]);
				if(bSaidAudio)
				{
					break;
				}
			}
		}
		
		//Check if we did not say any audio.
		if(!bSaidAudio)
		{
			//Check if we tried to force it.
			bool bIgnoreForcedFailure = (m_pSay->GetFlags().IsFlagSet(CAgitatedSay::IgnoreForcedFailure) || m_uConfigFlags.IsFlagSet(CF_IgnoreForcedAudioFailures));
			if(!bIgnoreForcedFailure && m_pSay->IsAudioFlagSet(CAmbientAudio::ForcePlay))
			{
				//Check if we have a state for when forced audio fails.
				atHashWithStringNotFinal hState = m_pResponse->FindStateToUseWhenForcedAudioFails();
				if(taskVerifyf(hState.IsNotNull(), "The state to use when forced audio fails is invalid for response: %s.", FindNameForResponse().GetCStr()))
				{
					//Change the state.
					ChangeState(hState);
				}
			}
		}
	}
	else
	{
		//The say did not come from us -- we need to make sure
		//the agitator is aware that the ped said something to them.

		//Check if the audio would not be ignored.
		if(!m_pSay->IsAudioFlagSet(CAmbientAudio::IsIgnored))
		{
			//Note that the ped talked to the agitator.
			CPedIntelligence::TalkInfo talkedToPedInfo(GetAgitator());
			pPed->GetPedIntelligence()->SetTalkedToPedInfo(talkedToPedInfo);

			//Note that the ped talked to us.
			CPedIntelligence::TalkInfo pedTalkedToUsInfo(pPed);
			GetAgitator()->GetPedIntelligence()->SetPedTalkedToUsInfo(pedTalkedToUsInfo);
		}	
	}
}

void CTaskAgitated::SetFriendlyException()
{
	//Set the friendly exception.
	GetPed()->GetPedIntelligence()->SetFriendlyException(GetAgitator());
}

void CTaskAgitated::SetOnFootClipSet()
{
	//Reset the on-foot clip set.
	ResetOnFootClipSet();
	
	//Create the context.
	CAgitatedConditionContext context;
	context.m_Task = this;
	
	//Iterate over the clip sets.
	const atArray<CAgitatedResponse::OnFootClipSet>& rOnFootClipSets = m_pResponse->GetOnFootClipSets();
	for(int i = 0; i < rOnFootClipSets.GetCount(); ++i)
	{
		//Grab the on foot clip set.
		const CAgitatedResponse::OnFootClipSet& rOnFootClipSet = rOnFootClipSets[i];
		
		//Check if the condition is valid.
		const CAgitatedCondition* pCondition = rOnFootClipSet.m_Condition;
		if(pCondition)
		{
			//Ensure the condition has been satisfied.
			if(!pCondition->Check(context))
			{
				continue;
			}
		}
		
		//Request the on-foot clip set.
		fwMvClipSetId clipSetId(rOnFootClipSet.m_Name.GetHash());
		m_OnFootClipSetRequestHelper.Request(clipSetId);
		
		//Update the flag.
		m_uRunningFlags.ChangeFlag(RF_DontResetOnFootClipSet, rOnFootClipSet.m_Flags.IsFlagSet(CAgitatedResponse::OnFootClipSet::DontReset));
		
		//Nothing else to do.
		break;
	}
}

void CTaskAgitated::SetReactions()
{
	//Reset the reactions.
	ResetReactions();

	//Ensure the response is valid.
	if(!m_pResponse)
	{
		return;
	}

	//Ensure the state is valid.
	if(!m_pState)
	{
		return;
	}

	//Iterate over the reaction names.
	const atArray<atHashWithStringNotFinal>& aReactions = m_pState->GetReactions();
	for(int i = 0; i < aReactions.GetCount(); ++i)
	{
		//Grab the reaction.
		atHashWithStringNotFinal hReaction = aReactions[i];

		//Ensure the reaction is valid.
		const CAgitatedReaction* pReaction = m_pResponse->GetReaction(hReaction);
		if(!taskVerifyf(pReaction, "The reaction: %s is invalid for response: %s.", hReaction.GetCStr(), FindNameForResponse().GetCStr()))
		{
			continue;
		}

		//Ensure the reactions are not full.
		if(!taskVerifyf(!m_aReactions.IsFull(), "There are too many reactions in state: %s.  The maximum is: %d.", FindNameForState().GetCStr(), m_aReactions.GetMaxCount()))
		{
			break;
		}

		//Add the reaction.
		m_aReactions.Append() = pReaction;
	}
}

void CTaskAgitated::SetTalkResponse()
{
	//Clear the talk response.
	ClearTalkResponse();

	//Ensure the talk response is valid.
	const CAgitatedTalkResponse* pTalkResponse = FindFirstValidTalkResponse();
	if(!pTalkResponse)
	{
		return;
	}

	//Set the talk response.
	GetPed()->GetPedIntelligence()->SetTalkResponse(pTalkResponse->GetAudio());
}

bool CTaskAgitated::ShouldFinish() const
{
	//Ensure the response is valid.
	if(!m_pResponse)
	{
		return true;
	}
	
	//Ensure the state is valid.
	if(!m_pState)
	{
		return true;
	}
	
	//Ensure this is not the final state.
	if(m_pState->GetFlags().IsFlagSet(CAgitatedState::FinalState))
	{
		return true;
	}
	
	return false;
}

bool CTaskAgitated::ShouldForceAiUpdateWhenChangingTask() const
{
	//Check if we are wandering.
	if(GetPed()->GetPedResetFlag(CPED_RESET_FLAG_Wandering))
	{
		return true;
	}

	return false;
}

bool CTaskAgitated::ShouldListen() const
{
	//Ensure the agitator is talking.
	if(!IsAgitatorTalking())
	{
		return false;
	}
	
	//Check if the state is valid.
	if(m_pState)
	{
		//Check if the time to listen is valid.
		float fTimeToListen = m_pState->GetTimeToListen();
		if(fTimeToListen >= 0.0f)
		{
			//Ensure the time the agitator has been talking does not exceed the threshold.
			if(m_fTimeAgitatorHasBeenTalking >= fTimeToListen)
			{
				return false;
			}
		}
	}
	
	return true;
}

bool CTaskAgitated::ShouldProcessDesiredHeading()
{
	//Check if the ped is getting up.
	if(IsGettingUp())
	{
		return true;
	}

	return false;
}

void CTaskAgitated::UpdateReactions()
{
	//Find the first valid reaction.
	const CAgitatedReaction* pReaction = FindFirstValidReaction();
	if(!pReaction)
	{
		return;
	}

#if CAPTURE_AGITATED_HISTORY
	//Check if the history is full.
	if(m_History.IsFull())
	{
		//Remove an entry.
		m_History.Pop();
	}

	//Add the entry.
	HistoryEntry& rEntry = m_History.Append();
	rEntry.m_hReaction	= FindNameForReaction(*pReaction);
	rEntry.m_uFlags		= m_uAgitatedFlags;
#endif
		
	//Grab the ped.
	CPed* pPed = GetPed();
	
	//Apply the anger response.
	pPed->GetPedIntelligence()->GetPedMotivation().IncreasePedMotivation(CPedMotivation::ANGRY_STATE, pReaction->GetAnger());
	
	//Apply the fear response.
	pPed->GetPedIntelligence()->GetPedMotivation().IncreasePedMotivation(CPedMotivation::FEAR_STATE, pReaction->GetFear());

	//Check if we should set the say.
	bool bDontClearSay = pReaction->GetFlags().IsFlagSet(CAgitatedReaction::DontClearSay);
	bool bDontSetSay = bDontClearSay && (m_pSay != NULL);
	bool bSetSay = !bDontSetSay;
	if(bSetSay)
	{
		//Set the say.
		m_pSay = FindFirstValidSayForReaction(*pReaction);
		if(m_pSay && !m_pSay->IsValid())
		{
			m_pSay = NULL;
		}
	}

	//Execute the action for the reaction.
	ExecuteActionForReaction(*pReaction);
	
	//Check if the state is valid.
	atHashWithStringNotFinal hState = pReaction->GetState();
	if(hState.IsNotNull())
	{
		//Change the state.
		ChangeState(hState);
	}

	//Check if we should clear the flags.
	bool bDontClearFlags = pReaction->GetFlags().IsFlagSet(CAgitatedReaction::DontClearFlags);
	if(!bDontClearFlags)
	{
		//Reset the agitated flags.
		ResetAgitatedFlags();
	}
}

void CTaskAgitated::UpdateSay()
{
	//Ensure we can say.
	if(!CanSay())
	{
		return;
	}
	
	//Say the line.
	Say();
	
	//Clear the say.
	m_pSay = NULL;
}

void CTaskAgitated::UpdateTimersForState()
{
	//Grab the time step.
	float fTimeStep = GetTimeStep();

	//Update the time.
	m_fTimeInAgitatedState += fTimeStep;

	//Check if the audio has ended.
	if(HasAudioEnded())
	{
		m_fTimeSinceAudioEndedForState += fTimeStep;
	}

	//Check if we are waiting.
	if(IsWaiting())
	{
		m_fTimeSpentWaitingInState += fTimeStep;
	}
}

const CAgitatedAction* CTaskAgitated::FindFirstValidActionForReaction(const CAgitatedReaction& rReaction) const
{
	//Check if the action is valid.
	const CAgitatedAction* pAction = rReaction.GetAction();
	if(pAction)
	{
		return pAction;
	}

	//Check if the conditional action has an action.
	const CAgitatedConditionalAction& rConditionalAction = rReaction.GetConditionalAction();
	if(rConditionalAction.HasAction())
	{
		//Create the condition context.
		CAgitatedConditionContext conditionContext;
		conditionContext.m_Task = this;

		//Check if the condition is valid.
		const CAgitatedCondition* pCondition = rConditionalAction.GetCondition();
		if(!pCondition || pCondition->Check(conditionContext))
		{
			return rConditionalAction.GetAction();
		}
	}

	return NULL;
}

const CAgitatedAction* CTaskAgitated::FindFirstValidActionForState(const CAgitatedState& rState) const
{
	//Check if the action is valid.
	const CAgitatedAction* pAction = rState.GetAction();
	if(pAction)
	{
		return pAction;
	}

	//Check if the conditional action has an action.
	const CAgitatedConditionalAction& rConditionalAction = rState.GetConditionalAction();
	if(rConditionalAction.HasAction())
	{
		//Create the condition context.
		CAgitatedConditionContext conditionContext;
		conditionContext.m_Task = this;

		//Check if the condition is valid.
		const CAgitatedCondition* pCondition = rConditionalAction.GetCondition();
		if(!pCondition || pCondition->Check(conditionContext))
		{
			return rConditionalAction.GetAction();
		}
	}

	//Check if the conditional actions are valid.
	const atArray<CAgitatedConditionalAction>& rConditionalActions = rState.GetConditionalActions();
	int iNumConditionalActions = rConditionalActions.GetCount();
	if(iNumConditionalActions > 0)
	{
		//Create the condition context.
		CAgitatedConditionContext conditionContext;
		conditionContext.m_Task = this;

		//Iterate over the conditional actions.
		for(int i = 0; i < iNumConditionalActions; ++i)
		{
			//Grab the conditional action.
			const CAgitatedConditionalAction& rConditionalAction = rConditionalActions[i];

			//Check if the condition is valid.
			const CAgitatedCondition* pCondition = rConditionalAction.GetCondition();
			if(!pCondition || pCondition->Check(conditionContext))
			{
				return rConditionalAction.GetAction();
			}
		}
	}

	return NULL;
}

const CAgitatedReaction* CTaskAgitated::FindFirstValidReaction() const
{
	//Create the context.
	CAgitatedConditionContext context;
	context.m_Task = this;

	//Iterate over the reactions.
	for(int i = 0; i < m_aReactions.GetCount(); ++i)
	{
		//Grab the reaction.
		const CAgitatedReaction* pReaction = m_aReactions[i];

		//Ensure the condition is valid.
		const CAgitatedCondition* pCondition = pReaction->GetCondition();
		if(pCondition && !pCondition->Check(context))
		{
			continue;
		}

		return pReaction;
	}

	return NULL;
}

const CAgitatedSay* CTaskAgitated::FindFirstValidSayForReaction(const CAgitatedReaction& rReaction) const
{
	//Check if the say is valid.
	const CAgitatedSay& rSay = rReaction.GetSay();
	if(rSay.IsValid())
	{
		return &rSay;
	}

	//Check if the conditional say has a valid say.
	const CAgitatedConditionalSay& rConditionalSay = rReaction.GetConditionalSay();
	if(rConditionalSay.HasValidSay())
	{
		//Create the context.
		CAgitatedConditionContext context;
		context.m_Task = this;

		//Check if the condition is valid.
		const CAgitatedCondition* pCondition = rConditionalSay.GetCondition();
		if(!pCondition || pCondition->Check(context))
		{
			return &rConditionalSay.GetSay();
		}
	}

	//Check if the conditional says are valid.
	const atArray<CAgitatedConditionalSay>& rConditionalSays = rReaction.GetConditionalSays();
	int iNumConditionalSays = rConditionalSays.GetCount();
	if(iNumConditionalSays > 0)
	{
		//Create the context.
		CAgitatedConditionContext context;
		context.m_Task = this;

		//Iterate over the conditional says.
		for(int i = 0; i < iNumConditionalSays; ++i)
		{
			//Grab the conditional say.
			const CAgitatedConditionalSay& rConditionalSay = rConditionalSays[i];

			//Ensure the condition is valid.
			const CAgitatedCondition* pCondition = rConditionalSay.GetCondition();
			if(pCondition && !pCondition->Check(context))
			{
				continue;
			}

			return &rConditionalSay.GetSay();
		}
	}

	return NULL;
}

const CAgitatedTalkResponse* CTaskAgitated::FindFirstValidTalkResponse() const
{
	//Check if the response is valid.
	if(m_pResponse)
	{
		//Check if the agitator recently talked to us.
		static float s_fMaxTime = 7.5f;
		if(GetAgitator()->GetPedIntelligence()->HasRecentlyTalkedToPed(*GetPed(), s_fMaxTime))
		{
			//Check if the audio is valid.
			const CAmbientAudio* pAudio = GetAgitator()->GetPedIntelligence()->GetLastAudioSaidToPed(*GetPed());
			if(pAudio)
			{
				//Check if a follow-up exists.
				const CAgitatedTalkResponse* pTalkResponse = m_pResponse->GetFollowUp(pAudio->GetFinalizedHash());
				if(pTalkResponse)
				{
					return pTalkResponse;
				}
			}
		}
	}

	//Check if the state is valid.
	if(m_pState)
	{
		//Check if the talk response is valid.
		const CAgitatedTalkResponse& rTalkResponse = m_pState->GetTalkResponse();
		if(rTalkResponse.IsValid())
		{
			return &rTalkResponse;
		}

		//Check if the conditional talk response is valid.
		const CAgitatedConditionalTalkResponse& rConditionalTalkResponse = m_pState->GetConditionalTalkResponse();
		if(rConditionalTalkResponse.HasValidTalkResponse())
		{
			//Create the context.
			CAgitatedConditionContext context;
			context.m_Task = this;

			//Check if the condition is valid.
			const CAgitatedCondition* pCondition = rConditionalTalkResponse.GetCondition();
			if(!pCondition || pCondition->Check(context))
			{
				return &rConditionalTalkResponse.GetTalkResponse();
			}
		}

		//Check if the talk responses are valid.
		const atArray<CAgitatedTalkResponse>& rTalkResponses = m_pState->GetTalkResponses();
		int iCount = rTalkResponses.GetCount();
		if(iCount > 0)
		{
			//Choose a random talk response.
			int iRandom = fwRandom::GetRandomNumberInRange(0, iCount);
			return &rTalkResponses[iRandom];
		}
	}

	//Check if the response is valid.
	if(m_pResponse)
	{
		//Check if the talk response is valid.
		const CAgitatedTalkResponse& rTalkResponse = m_pResponse->GetTalkResponse();
		if(rTalkResponse.IsValid())
		{
			return &rTalkResponse;
		}
		else
		{
			//Check if the talk responses are valid.
			const atArray<CAgitatedTalkResponse>& rTalkResponses = m_pResponse->GetTalkResponses();
			int iCount = rTalkResponses.GetCount();
			if(iCount > 0)
			{
				//Choose a random talk response.
				int iRandom = fwRandom::GetRandomNumberInRange(0, iCount);
				return &rTalkResponses[iRandom];
			}
		}
	}

	return NULL;
}

bool CTaskAgitated::IsAngry(const CPed* pPed, const float fThreshold)
{
	return (pPed->GetPedIntelligence()->GetPedMotivation().GetMotivation(CPedMotivation::ANGRY_STATE) > fThreshold);
}

bool CTaskAgitated::IsArgumentative(const CPed* pPed)
{
	return (pPed->CheckBraveryFlags(BF_ARGUMENTATIVE));
}

bool CTaskAgitated::IsAvoiding() const
{
	return (GetPed()->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMPLEX_EVASIVE_STEP, true));
}

bool CTaskAgitated::IsConfrontational(const CPed* pPed)
{
	return (pPed->CheckBraveryFlags(BF_CONFRONTATIONAL));
}

bool CTaskAgitated::IsFearful(const CPed* pPed, const float fThreshold)
{
	return (pPed->GetPedIntelligence()->GetPedMotivation().GetMotivation(CPedMotivation::FEAR_STATE) > fThreshold);
}

bool CTaskAgitated::IsGunPulled(const CPed* pPed)
{
	//Grab the weapon manager.
	const CPedWeaponManager* pWeaponManager = pPed->GetWeaponManager();
	if(!pWeaponManager)
	{
		return false;
	}

	//Grab the weapon info.
	const CWeaponInfo* pWeaponInfo = pWeaponManager->GetEquippedWeaponInfo();
	if(!pWeaponInfo)
	{
		return false;
	}

	return pWeaponInfo->GetIsGun();
}

bool CTaskAgitated::IsInjured(const CPed* pPed)
{
	return pPed->IsInjured();
}

const CAgitatedContext* CTaskAgitated::GetContext(const CPed& rPed, atHashWithStringNotFinal hContext)
{
	//Ensure the personality is valid.
	const CAgitatedPersonality* pPersonality = GetPersonality(rPed);
	if(!pPersonality)
	{
		return NULL;
	}

	//Ensure the context is valid.
	if(hContext.IsNull())
	{
		return NULL;
	}

	//Ensure the context is valid.
	const CAgitatedContext* pContext = pPersonality->GetContext(hContext);
	if(!pContext)
	{
		return NULL;
	}

	return pContext;
}

const CAgitatedPersonality* CTaskAgitated::GetPersonality(const CPed& rPed)
{
	//Ensure the model info is valid.
	CPedModelInfo* pPedModelInfo = rPed.GetPedModelInfo();
	if(!taskVerifyf(pPedModelInfo, "The ped model info is invalid."))
	{
		return NULL;
	}

	//Ensure the personality is valid.
	atHashString hPersonality = pPedModelInfo->GetPersonalitySettings().GetAgitatedPersonality();
	if(hPersonality.IsNull())
	{
		return NULL;
	}

	//Ensure the personality is valid.
	const CAgitatedPersonality* pPersonality = CAgitatedManager::GetInstance().GetPersonalities().GetPersonality(hPersonality);
	if(!taskVerifyf(pPersonality, "The personality: %s is invalid.", hPersonality.GetCStr()))
	{
		return NULL;
	}

	return pPersonality;
}

#if !__FINAL
void CTaskAgitated::Debug() const
{
#if DEBUG_DRAW
	//Ensure rendering is enabled.
	if(!sm_Tunables.m_Rendering.m_Enabled)
	{
		return;
	}

	//Grab the ped.
	const CPed* pPed = GetPed();

	//Create the buffer.
	char buf[128];

	//Keep track of the line.
	int iLine = 0;

	//Calculate the text position.
	Vector3 vTextPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) + (ZAXIS * 1.0f);

	//Check if info is enabled.
	if(sm_Tunables.m_Rendering.m_Info)
	{		
		formatf(buf, "Response: %s", FindNameForResponse().GetCStr());
		grcDebugDraw::Text(vTextPosition, Color_blue, 0, iLine++ * grcDebugDraw::GetScreenSpaceTextHeight(), buf);
		
		formatf(buf, "State: %s", FindNameForState().GetCStr());
		grcDebugDraw::Text(vTextPosition, Color_blue, 0, iLine++ * grcDebugDraw::GetScreenSpaceTextHeight(), buf);
		
		formatf(buf, "Fear: %.2f", pPed->GetPedIntelligence()->GetPedMotivation().GetMotivation(CPedMotivation::FEAR_STATE));
		grcDebugDraw::Text(vTextPosition, Color_blue, 0, iLine++ * grcDebugDraw::GetScreenSpaceTextHeight(), buf);
		
		formatf(buf, "Anger: %.2f", pPed->GetPedIntelligence()->GetPedMotivation().GetMotivation(CPedMotivation::ANGRY_STATE));
		grcDebugDraw::Text(vTextPosition, Color_blue, 0, iLine++ * grcDebugDraw::GetScreenSpaceTextHeight(), buf);

		formatf(buf, "Time In State: %.2f", m_fTimeInAgitatedState);
		grcDebugDraw::Text(vTextPosition, Color_blue, 0, iLine++ * grcDebugDraw::GetScreenSpaceTextHeight(), buf);

		formatf(buf, "Time Since Audio Ended For State: %.2f", m_fTimeSinceAudioEndedForState);
		grcDebugDraw::Text(vTextPosition, Color_blue, 0, iLine++ * grcDebugDraw::GetScreenSpaceTextHeight(), buf);

		formatf(buf, "Time Spent Waiting In State: %.2f", m_fTimeSpentWaitingInState);
		grcDebugDraw::Text(vTextPosition, Color_blue, 0, iLine++ * grcDebugDraw::GetScreenSpaceTextHeight(), buf);
	}

	//Check if hashes are enabled.
	if(sm_Tunables.m_Rendering.m_Hashes)
	{
		for(int i = 0; i < m_aHashes.GetCount(); ++i)
		{
			formatf(buf, "%s", m_aHashes[i].GetCStr());
			grcDebugDraw::Text(vTextPosition, Color_green, 0, iLine++ * grcDebugDraw::GetScreenSpaceTextHeight(), buf);
		}
	}

#if CAPTURE_AGITATED_HISTORY
	//Check if history is enabled.
	if(sm_Tunables.m_Rendering.m_History)
	{
		//Grab the ped.
		const CPed* pPed = GetPed();

		//Create the buffer.
		char buf[128];

		//Keep track of the line.
		int iLine = 0;

		//Calculate the text position.
		Vector3 vTextPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) + (ZAXIS * 2.0f);

		//Iterate over the history.
		for(int i = 0; i < m_History.GetCount(); ++i)
		{
			formatf(buf, "Reaction: %s, Flags: %d", m_History[i].m_hReaction.GetCStr(), m_History[i].m_uFlags.GetAllFlags());
			grcDebugDraw::Text(vTextPosition, Color_red, 0, iLine++ * grcDebugDraw::GetScreenSpaceTextHeight(), buf);
		}
	}
#endif
#endif

	CTask::Debug();
}
#endif // !__FINAL

#if !__FINAL
const char* CTaskAgitated::GetStaticStateName(s32 iState)
{
	Assert(iState >= State_Start && iState <= State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_Resume",
		"State_Listen",
		"State_React",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif // !__FINAL

#if __DEV
namespace CTaskAgitatedDebug
{
	static char sm_cResponseNameForDisputes[128];

	CPed* GetFocusPed()
	{
		//Ensure the focus entity is valid.
		CEntity* pFocusEntity = CDebugScene::FocusEntities_Get(0);
		if(!pFocusEntity)
		{
			return NULL;
		}

		//Ensure the focus entity is a ped.
		if(!pFocusEntity->GetIsTypePed())
		{
			return NULL;
		}

		//Ensure the focus ped is a player.
		return static_cast<CPed *>(pFocusEntity);
	}

	void ApplyAgitation(AgitatedType nType)
	{
		//Ensure the focus ped is valid.
		CPed* pFocusPed = GetFocusPed();
		if(!pFocusPed)
		{
			return;
		}

		//Ensure the agitated task is valid.
		CTaskAgitated* pTask = CTaskAgitated::FindAgitatedTaskForPed(*pFocusPed);
		if(!pTask)
		{
			return;
		}

		//Apply the agitation.
		pTask->ApplyAgitation(nType);
	}

	void ApplyAgitationForBumped()
	{
		ApplyAgitation(AT_Bumped);
	}

	void ApplyAgitationForBumpedByVehicle()
	{
		ApplyAgitation(AT_BumpedByVehicle);
	}

	void ApplyAgitationForBumpedInVehicle()
	{
		ApplyAgitation(AT_BumpedInVehicle);
	}

	void ApplyAgitationForDodged()
	{
		ApplyAgitation(AT_Dodged);
	}

	void ApplyAgitationForDodgedVehicle()
	{
		ApplyAgitation(AT_DodgedVehicle);
	}

	void ApplyAgitationForHonkedAt()
	{
		ApplyAgitation(AT_HonkedAt);
	}

	void ApplyAgitationForRevvedAt()
	{
		ApplyAgitation(AT_RevvedAt);
	}

	void ApplyAgitationForInsulted()
	{
		ApplyAgitation(AT_Insulted);
	}

	void ApplyAgitationForIntimidate()
	{
		ApplyAgitation(AT_Intimidate);
	}

	void ApplyAgitationForLoitering()
	{
		ApplyAgitation(AT_Loitering);
	}

	void ApplyAgitationForFollowing()
	{
		ApplyAgitation(AT_Following);
	}

	void ApplyAgitationForTerritoryIntruded()
	{
		ApplyAgitation(AT_TerritoryIntruded);
	}

	void StartResponse()
	{
		//Ensure the focus ped is valid.
		CPed* pFocusPed = GetFocusPed();
		if(!pFocusPed)
		{
			return;
		}

		//Ensure the focus ped is a player.
		if(pFocusPed->IsPlayer())
		{
			return;
		}

		//Ensure the player is valid.
		CPed* pPlayer = CGameWorld::FindLocalPlayer();
		if(!pPlayer)
		{
			return;
		}

		//Create the task.
		CTaskAgitated* pTask = rage_new CTaskAgitated(pPlayer);

		//Set the response override.
		pTask->SetResponseOverride(atHashWithStringNotFinal(sm_cResponseNameForDisputes));

		//Give the focus ped the task.
		CEventGivePedTask event(PED_TASK_PRIORITY_PRIMARY, pTask);
		pFocusPed->GetPedIntelligence()->AddEvent(event);
	}
}

void CTaskAgitated::InitWidgets()
{
	bkBank* pBank = BANKMGR.FindBank("A.I.");
	if(pBank)
	{
		pBank->PushGroup("Disputes");

			pBank->PushGroup("Response");

				pBank->AddText("Response Name:", CTaskAgitatedDebug::sm_cResponseNameForDisputes, sizeof(CTaskAgitatedDebug::sm_cResponseNameForDisputes), false);
				pBank->AddButton("Start Response", CTaskAgitatedDebug::StartResponse);

			pBank->PopGroup();

			pBank->PushGroup("Agitations");

				pBank->AddButton("Insulted", CTaskAgitatedDebug::ApplyAgitationForInsulted);
				pBank->AddButton("Bumped", CTaskAgitatedDebug::ApplyAgitationForBumped);
				pBank->AddButton("Bumped By Vehicle", CTaskAgitatedDebug::ApplyAgitationForBumpedByVehicle);
				pBank->AddButton("Bumped In Vehicle", CTaskAgitatedDebug::ApplyAgitationForBumpedInVehicle);
				pBank->AddButton("Dodged", CTaskAgitatedDebug::ApplyAgitationForDodged);
				pBank->AddButton("Dodged Vehicle", CTaskAgitatedDebug::ApplyAgitationForDodgedVehicle);
				pBank->AddButton("Honked At", CTaskAgitatedDebug::ApplyAgitationForHonkedAt);
				pBank->AddButton("Revved At", CTaskAgitatedDebug::ApplyAgitationForRevvedAt);
				pBank->AddButton("Loitering", CTaskAgitatedDebug::ApplyAgitationForLoitering);
				pBank->AddButton("Following", CTaskAgitatedDebug::ApplyAgitationForFollowing);
				pBank->AddButton("Territory Intruded", CTaskAgitatedDebug::ApplyAgitationForTerritoryIntruded);
				pBank->AddButton("Intimidate", CTaskAgitatedDebug::ApplyAgitationForIntimidate);

			pBank->PopGroup();

		pBank->PopGroup();
	}
}
#endif

////////////////////////////////////////////////////////////////////////////////
// CTaskAgitatedAction
////////////////////////////////////////////////////////////////////////////////

CTaskAgitatedAction::CTaskAgitatedAction(aiTask* pTask)
: m_pTask(pTask)
, m_uRunningFlags(0)
{
	SetInternalTaskType(CTaskTypes::TASK_AGITATED_ACTION);
}

CTaskAgitatedAction::~CTaskAgitatedAction()
{
	//Free the task.
	if(m_pTask)
	{
		delete m_pTask;
	}
}

bool CTaskAgitatedAction::IsValidForMotionTask(CTaskMotionBase& task) const
{
	bool bIsValid = (GetState() == State_Start || GetState() == State_Resume); // Wait until we got us a proper task first
	if (!bIsValid)
		bIsValid = CTask::IsValidForMotionTask(task);

	if (!bIsValid && GetSubTask())
		bIsValid = GetSubTask()->IsValidForMotionTask(task);

	return bIsValid;
}

#if !__FINAL
void CTaskAgitatedAction::Debug() const
{
	//Call the base class version.
	CTask::Debug();
}

const char* CTaskAgitatedAction::GetStaticStateName(s32 iState)
{
	Assert(iState >= State_Start && iState <= State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_Resume",
		"State_Task",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif // !__FINAL

CTask::FSM_Return CTaskAgitatedAction::ProcessPreFSM()
{
	//Ensure the agitated task is still running.
	if(!CTaskAgitated::FindAgitatedTaskForPed(*GetPed()))
	{
		return FSM_Quit;
	}
	
	//Check if we should quit.
	if(m_uRunningFlags.IsFlagSet(AARF_Quit))
	{
		return FSM_Quit;
	}

	//Check for shocking events.
	GetPed()->GetPedIntelligence()->SetCheckShockFlag(true);
	
	return FSM_Continue;
}

CTask::FSM_Return CTaskAgitatedAction::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_Resume)
			FSM_OnUpdate
				return Resume_OnUpdate();

		FSM_State(State_Task)
			FSM_OnEnter
				Task_OnEnter();
			FSM_OnUpdate
				return Task_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskAgitatedAction::Start_OnUpdate()
{
	//Move to the task state.
	SetState(State_Task);

	return FSM_Continue;
}

CTask::FSM_Return CTaskAgitatedAction::Resume_OnUpdate()
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

void CTaskAgitatedAction::Task_OnEnter()
{
	//Check if we are resuming the state.  Keep the sub-task, if possible.
	if(GetSubTask() && m_pTask && (GetSubTask()->GetTaskType() == m_pTask->GetTaskType()))
	{
		return;
	}

	//Ensure the task is valid.
	if(!m_pTask)
	{
		return;
	}
	
	//Create the task.
	aiTask* pTask = m_pTask->Copy();
	
	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskAgitatedAction::Task_OnUpdate()
{
	//Wait for the task to end.
	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Finish the task.
		SetState(State_Finish);
	}
	
	return FSM_Continue;
}

int CTaskAgitatedAction::ChooseStateToResumeTo(bool& bKeepSubTask) const
{
	//Keep the sub-task.
	bKeepSubTask = true;

	return State_Task;
}
