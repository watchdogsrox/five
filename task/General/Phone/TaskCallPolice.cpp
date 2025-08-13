// FILE :    TaskCallPolice.h

// File header
#include "task/General/Phone/TaskCallPolice.h"

// Game headers
#include "audio/policescanner.h"
#include "audio/speechaudioentity.h"
#include "game/WitnessInformation.h"
#include "game/Wanted.h"
#include "Peds/Ped.h"
#include "peds/PedIntelligence.h"
#include "scene/world/GameWorld.h"
#include "task/Default/AmbientAudio.h"
#include "task/General/Phone/TaskMobilePhone.h"
#include "task/Response/TaskFlee.h"
#include "task/Response/TaskShockingEvents.h"

AI_OPTIMISATIONS()

//=========================================================================
// CTaskCallPolice
//=========================================================================

CTaskCallPolice::Tunables CTaskCallPolice::sm_Tunables;
IMPLEMENT_GENERAL_TASK_TUNABLES(CTaskCallPolice, 0xc7ca9242);

CTaskCallPolice::CTaskCallPolice(const CPed* pTarget)
: m_pTarget(pTarget)
, m_fTimeSinceTalkingEnded(0.0f)
, m_fTimeSinceTargetTalkingEnded(0.0f)
, m_fTimeTargetHasBeenTalking(0.0f)
, m_fTimeSpentInEarLoop(0.0f)
, m_uRunningFlags(0)
{
	SetInternalTaskType(CTaskTypes::TASK_CALL_POLICE);
}

CTaskCallPolice::~CTaskCallPolice()
{

}

bool CTaskCallPolice::CanGiveToWitness(const CPed& rPed, const CPed& rTarget)
{
	//Ensure the ped will call law enforcement.
	if(!CWitnessInformationManager::GetInstance().WillCallLawEnforcement(rPed))
	{
		return false;
	}

	//Ensure the ped can make the call.
	if(!CanMakeCall(rPed, &rTarget))
	{
		return false;
	}

	//Check if the ped is fleeing.
	const CTaskSmartFlee* pTaskSmartFlee = static_cast<CTaskSmartFlee *>(
		rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_SMART_FLEE));
	if(pTaskSmartFlee && !pTaskSmartFlee->HasCalledPolice())
	{
		return true;
	}

	//Check if the ped is hurrying away.
	const CTaskShockingEventHurryAway* pTaskShockingEventHurryAway = static_cast<CTaskShockingEventHurryAway *>(
		rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_SHOCKING_EVENT_HURRYAWAY));
	if(pTaskShockingEventHurryAway && !pTaskShockingEventHurryAway->HasCalledPolice())
	{
		return true;
	}

	return false;
}

bool CTaskCallPolice::CanMakeCall(const CPed& rPed, const CPed* pTarget)
{
	//Ensure the ped is a civilian.
	if(!rPed.IsCivilianPed())
	{
		return false;
	}

	//Ensure the ped is random.
	if(!rPed.PopTypeIsRandom())
	{
		return false;
	}

	//Ensure the ped has a phone.
	if(!DoesPedHavePhone(rPed))
	{
		return false;
	}

	//Ensure the ped is not calling the police.
	if(rPed.GetPedResetFlag(CPED_RESET_FLAG_IsCallingPolice))
	{
		return false;
	}

	//Ensure the ped has not recently called the police.
	static dev_u32 s_uMinTimeSinceLastCalledPolice = 30000;
	u32 uLastTimeCalledPolice = rPed.GetPedIntelligence()->GetLastTimeCalledPolice();
	if((uLastTimeCalledPolice > 0) && (CTimeHelpers::GetTimeSince(uLastTimeCalledPolice) < s_uMinTimeSinceLastCalledPolice))
	{
		return false;
	}

	//Ensure the ped is not already wanted, with cops in the area.
	static dev_float s_fMaxDistance = 40.0f;
	if(pTarget && pTarget->IsPlayer())
	{
		if(pTarget->GetPlayerWanted()->m_DisableCallPoliceThisFrame)
		{
			return false;
		}

		if(pTarget->GetPlayerWanted()->GetWithinAmnestyTime())
		{
			return false;
		}

		if( (pTarget->GetPlayerWanted()->GetWantedLevel() > WANTED_CLEAN) &&
			(CWanted::WorkOutPolicePresence(rPed.GetTransform().GetPosition(), s_fMaxDistance) > 0) )
		{
			return false;
		}
	}

	//Ensure the target is not an AI ped.
	if(pTarget && !pTarget->IsPlayer())
	{
		return false;
	}

	return true;
}

bool CTaskCallPolice::DoesPedHaveAudio(const CPed& rPed)
{
	//Ensure the ped has the commit context.
	if(!GetAudioForCommit().DoesContextExistForPed(rPed))
	{
		return false;
	}

	//Ensure the ped has the call context.
	if(!GetAudioForCall().DoesContextExistForPed(rPed))
	{
		return false;
	}

	return true;
}

bool CTaskCallPolice::DoesPedHavePhone(const CPed& rPed)
{
	//Ensure the ped has a phone.
	if(!rPed.GetPhoneComponent())
	{
		return false;
	}

	return true;
}

bool CTaskCallPolice::GiveToWitness(CPed& rPed, const CPed& rTarget)
{
	//Assert that we can give the task to the witness.
	taskAssert(CanGiveToWitness(rPed, rTarget));

	//Calculate the min time moving away.
	float fMinTimeMovingAway = sm_Tunables.m_MinTimeMovingAwayToGiveToWitness;
	fMinTimeMovingAway *= rPed.GetRandomNumberInRangeFromSeed(0.8f, 1.2f);

	//Check if the ped is fleeing.
	CTaskSmartFlee* pTaskSmartFlee = static_cast<CTaskSmartFlee *>(
		rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_SMART_FLEE));
	if(pTaskSmartFlee)
	{
		//Check if the ped is fleeing on foot.
		if(pTaskSmartFlee->IsFleeingOnFoot() &&
			(pTaskSmartFlee->GetTimeFleeingOnFoot() > fMinTimeMovingAway))
		{
			//Set the sub-task for on-foot.
			return pTaskSmartFlee->SetSubTaskForOnFoot(rage_new CTaskCallPolice(&rTarget));
		}
	}

	//Check if the ped is hurrying away.
	CTaskShockingEventHurryAway* pTaskShockingEventHurryAway = static_cast<CTaskShockingEventHurryAway *>(
		rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_SHOCKING_EVENT_HURRYAWAY));
	if(pTaskShockingEventHurryAway)
	{
		//Check if the ped is hurrying away, not backing away, and the criminal matches the target.
		if(pTaskShockingEventHurryAway->IsHurryingAway() &&
			(pTaskShockingEventHurryAway->GetTimeHurryingAway() > fMinTimeMovingAway) &&
			!pTaskShockingEventHurryAway->IsBackingAway() && pTaskShockingEventHurryAway->IsPlayerCriminal(rTarget))
		{
			//Set the sub-task for hurrying away.
			return pTaskShockingEventHurryAway->SetSubTaskForHurryAway(rage_new CTaskCallPolice(&rTarget));
		}
	}

	return false;
}

#if !__FINAL
void CTaskCallPolice::Debug() const
{
	CTask::Debug();
}

const char* CTaskCallPolice::GetStaticStateName(s32 iState)
{
	Assert(iState >= State_Start && iState <= State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_WaitForTalkingToFinish",
		"State_MakeCall",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif // !__FINAL

bool CTaskCallPolice::IsInEarLoop() const
{
	//Ensure the sub-task is valid.
	const CTask* pSubTask = GetSubTask();
	if(!pSubTask)
	{
		return false;
	}

	//Ensure the sub-task is the correct type.
	if(pSubTask->GetTaskType() != CTaskTypes::TASK_MOBILE_PHONE)
	{
		return false;
	}

	//Ensure the phone is up to the ear.
	const CTaskMobilePhone* pTask = static_cast<const CTaskMobilePhone *>(pSubTask);
	if(pTask->GetState() != CTaskMobilePhone::State_EarLoop)
	{
		return false;
	}

	return true;
}

bool CTaskCallPolice::IsTalking() const
{
	return IsTalking(*GetPed());
}

bool CTaskCallPolice::IsTalking(const CPed& rPed) const
{
	const audSpeechAudioEntity* pSpeechAudioEntity = rPed.GetSpeechAudioEntity();
	return (pSpeechAudioEntity && pSpeechAudioEntity->IsAmbientSpeechPlaying());
}

bool CTaskCallPolice::IsTargetTalking() const
{
	return (m_pTarget && IsTalking(*m_pTarget));
}

void CTaskCallPolice::OnCalledPolice()
{
	//Note that the police were called.
	taskAssert(!m_uRunningFlags.IsFlagSet(RF_CalledPolice));
	taskAssert(!m_uRunningFlags.IsFlagSet(RF_FailedToCallPolice));
	m_uRunningFlags.SetFlag(RF_CalledPolice);

	//Check if the target is valid.
	if(m_pTarget)
	{
		//Check if the target is a player.
		if(m_pTarget->IsPlayer())
		{
			//Check if the player has pending crimes.
			CWanted* pWanted = m_pTarget->GetPlayerWanted();
			if(pWanted->HasPendingCrimesNotWitnessedByLaw())
			{
				//Report the crimes as witnessed.
				pWanted->ReportWitnessSuccessful(GetPed());

				//Check if the player heard either of the audio lines.
				if(m_uRunningFlags.IsFlagSet(RF_PlayerHeardAudioForCommit) || m_uRunningFlags.IsFlagSet(RF_PlayerHeardAudioForCall))
				{
					const Vector3& pos = VEC3V_TO_VECTOR3(m_pTarget->GetTransform().GetPosition());
					//Give the player 1*.  It is confusing for a player to think someone is calling the cops, but not get a WL.
					pWanted->SetWantedLevelNoDrop(pos, WANTED_LEVEL1, 0, WL_FROM_CALL_POLICE_TASK);
					g_PoliceScanner.ReportPlayerCrime(pos, audEngineUtil::ResolveProbability(0.5f) ? CRIME_DISTURBANCE : CRIME_CIVILIAN_NEEDS_ASSISTANCE);
				}
			}
			else if(m_uRunningFlags.IsFlagSet(RF_PlayerHeardAudioForCommit) || m_uRunningFlags.IsFlagSet(RF_PlayerHeardAudioForCall))
			{
				//Give the player 1*.
				const Vector3& pos = VEC3V_TO_VECTOR3(m_pTarget->GetTransform().GetPosition());
				pWanted->SetWantedLevelNoDrop(pos, WANTED_LEVEL1, 0, WL_FROM_CALL_POLICE_TASK);
				g_PoliceScanner.ReportPlayerCrime(pos, audEngineUtil::ResolveProbability(0.5f) ? CRIME_DISTURBANCE : CRIME_CIVILIAN_NEEDS_ASSISTANCE);
			}
		}
		else
		{
			//TODO: Dispatch some cops to the target's location.
		}
	}
	else
	{
		//TODO: Dispatch some cops to the ped's location.
	}
}

void CTaskCallPolice::OnFailedToCallPolice()
{
	//Note that we failed to call the police.
	taskAssert(!m_uRunningFlags.IsFlagSet(RF_CalledPolice));
	taskAssert(!m_uRunningFlags.IsFlagSet(RF_FailedToCallPolice));
	m_uRunningFlags.SetFlag(RF_FailedToCallPolice);

	//Check if the target is valid.
	if(m_pTarget)
	{
		//Check if the target is a player.
		if(m_pTarget->IsPlayer())
		{
			//Check if the player has pending crimes.
			CWanted* pWanted = m_pTarget->GetPlayerWanted();
			if(pWanted->HasPendingCrimesNotWitnessedByLaw())
			{
				//Report the witness was unsuccessful.
				pWanted->ReportWitnessUnsuccessful(GetPed());
			}
		}
	}
}

void CTaskCallPolice::ProcessFlags()
{
	//Set the flag.
	GetPed()->SetPedResetFlag(CPED_RESET_FLAG_IsCallingPolice, true);
}

void CTaskCallPolice::ProcessHeadIk()
{
	//Ensure the target is valid.
	if(!m_pTarget)
	{
		return;
	}

	//Look at the jacker.
	GetPed()->GetIkManager().LookAt(0, m_pTarget, 1000,
		BONETAG_HEAD, NULL, 0, 500, 500, CIkManager::IK_LOOKAT_VERY_HIGH);
}

void CTaskCallPolice::ProcessTimers()
{
	//Check if we are talking.
	if(IsTalking())
	{
		m_fTimeSinceTalkingEnded = 0.0f;
	}
	else
	{
		m_fTimeSinceTalkingEnded += GetTimeStep();
	}

	//Check if the target is talking.
	if(IsTargetTalking())
	{
		m_fTimeSinceTargetTalkingEnded = 0.0f;
		m_fTimeTargetHasBeenTalking += GetTimeStep();
	}
	else
	{
		m_fTimeSinceTargetTalkingEnded += GetTimeStep();
		m_fTimeTargetHasBeenTalking = 0.0f;
	}

	//Check if we are not in an ear loop.
	if(!IsInEarLoop())
	{
		m_fTimeSpentInEarLoop = 0.0f;
	}
	else
	{
		m_fTimeSpentInEarLoop += GetTimeStep();
	}
}

void CTaskCallPolice::PutDownPhone()
{
	//Ensure the sub-task is valid.
	CTask* pSubTask = GetSubTask();
	if(!aiVerify(pSubTask))
	{
		return;
	}

	//Ensure the sub-task is the correct type.
	if(!aiVerify(pSubTask->GetTaskType() == CTaskTypes::TASK_MOBILE_PHONE))
	{
		return;
	}

	//Put down the phone.
	CTaskMobilePhone* pTask = static_cast<CTaskMobilePhone *>(pSubTask);
	pTask->PutDownToPrevState();

	//Note that we have put down the phone.
	m_uRunningFlags.SetFlag(RF_PutDownPhone);
}

void CTaskCallPolice::SayAudio(const CAmbientAudio& rAudio, bool& bIsPlayingAudio, bool& bPlayerHeardAudio)
{
	//Clear the flags.
	bIsPlayingAudio		= false;
	bPlayerHeardAudio	= false;

	//Say the audio.
	bIsPlayingAudio = CTalkHelper::Talk(*GetPed(), rAudio);
	if(!bIsPlayingAudio)
	{
		return;
	}

	//Ensure the player is valid.
	const CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if(!pPlayer)
	{
		return;
	}

	//Ensure the player is in range.
	ScalarV scDistSq = DistSquared(GetPed()->GetTransform().GetPosition(), pPlayer->GetTransform().GetPosition());
	static dev_float s_fMaxDistance = 30.0f;
	ScalarV scMaxDistSq = ScalarVFromF32(square(s_fMaxDistance));
	if(IsGreaterThanAll(scDistSq, scMaxDistSq))
	{
		return;
	}

	//Set the flag.
	bPlayerHeardAudio = true;
}

bool CTaskCallPolice::ShouldPutDownPhone() const
{
	//Ensure we have not put down the phone.
	if(m_uRunningFlags.IsFlagSet(RF_PutDownPhone))
	{
		return false;
	}

	//Ensure we said the context for the call.
	if(!m_uRunningFlags.IsFlagSet(RF_SaidContextForCall))
	{
		return false;
	}

	//Ensure we are in the ear loop.
	if(!IsInEarLoop())
	{
		return false;
	}

	//Check if we are playing audio for the call.
	if(m_uRunningFlags.IsFlagSet(RF_IsPlayingAudioForCall))
	{
		//Check if we are not talking.
		if(!IsTalking())
		{
			return true;
		}
	}
	else
	{
		//Check if the time has elapsed.
		float fMinTimeToSpendInEarLoop = sm_Tunables.m_MinTimeToSpendInEarLoopToPutDownPhone;
		float fMaxTimeToSpendInEarLoop = sm_Tunables.m_MaxTimeToSpendInEarLoopToPutDownPhone;
		float fTimeToSpendInEarLoop = GetPed()->GetRandomNumberInRangeFromSeed(fMinTimeToSpendInEarLoop, fMaxTimeToSpendInEarLoop);
		if(m_fTimeSpentInEarLoop >= fTimeToSpendInEarLoop)
		{
			return true;
		}
	}

	return false;
}

bool CTaskCallPolice::ShouldSayContextForCall() const
{
	//Ensure we have not said the context for the call.
	if(m_uRunningFlags.IsFlagSet(RF_SaidContextForCall))
	{
		return false;
	}

	//Ensure we are not talking.
	if(IsTalking())
	{
		return false;
	}

	//Ensure the time since talking ended exceeds the threshold.
	float fMinTimeSinceTalkingEnded = sm_Tunables.m_MinTimeSinceTalkingEndedToSayContextForCall;
	if(m_fTimeSinceTalkingEnded < fMinTimeSinceTalkingEnded)
	{
		return false;
	}

	//Ensure we are in the ear loop.
	if(!IsInEarLoop())
	{
		return false;
	}

	//Ensure the time spent in ear loop exceeds the threshold.
	float fMinTimeSpentInEarLoop = sm_Tunables.m_MinTimeSpentInEarLoopToSayContextForCall;
	if(m_fTimeSpentInEarLoop < fMinTimeSpentInEarLoop)
	{
		return false;
	}

	return true;
}

const CAmbientAudio& CTaskCallPolice::GetAudioForCall()
{
	static CAmbientAudio s_Audio("PHONE_CALL_COPS", CAmbientAudio::ForcePlay|CAmbientAudio::AllowRepeat);
	return s_Audio;
}

const CAmbientAudio& CTaskCallPolice::GetAudioForCommit()
{
	static CAmbientAudio s_Audio("CALL_COPS_COMMIT", CAmbientAudio::ForcePlay|CAmbientAudio::AllowRepeat);
	return s_Audio;
}

void CTaskCallPolice::CleanUp()
{
	//Check if the police were not called.
	if(!m_uRunningFlags.IsFlagSet(RF_CalledPolice) && !m_uRunningFlags.IsFlagSet(RF_FailedToCallPolice))
	{
		//Note that we failed to call the police.
		OnFailedToCallPolice();
	}
}

aiTask* CTaskCallPolice::Copy() const
{
	//Create the task.
	CTaskCallPolice* pTask = rage_new CTaskCallPolice(m_pTarget);

	return pTask;
}

CTask::FSM_Return CTaskCallPolice::ProcessPreFSM()
{
	//Process the flags.
	ProcessFlags();

	//Process the timers.
	ProcessTimers();

	//Process head ik.
	ProcessHeadIk();

	return FSM_Continue;
}

CTask::FSM_Return CTaskCallPolice::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
	
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_WaitForTalkingToFinish)
			FSM_OnUpdate
				return WaitForTalkingToFinish_OnUpdate();

		FSM_State(State_MakeCall)
			FSM_OnEnter
				MakeCall_OnEnter();
			FSM_OnUpdate
				return MakeCall_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskCallPolice::Start_OnUpdate()
{
	//Assert that the ped has a phone.
	taskAssert(DoesPedHavePhone(*GetPed()));

	//Check if we are talking, or our target is talking.
	if(IsTalking() || IsTargetTalking())
	{
		//Wait for talking to finish.
		SetState(State_WaitForTalkingToFinish);
	}
	else
	{
		//Make the call.
		SetState(State_MakeCall);
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskCallPolice::WaitForTalkingToFinish_OnUpdate()
{
	//Check if the time since talking ended exceeds the threshold.
	float fMinTimeSinceTalkingEnded = sm_Tunables.m_MinTimeSinceTalkingEndedToMakeCall;
	if(m_fTimeSinceTalkingEnded >= fMinTimeSinceTalkingEnded)
	{
		//Check if the time since the target talking ended exceeds the threshold, OR
		//the time the target has been talking exceeds the threshold.
		float fMinTimeSinceTargetTalkingEnded	= sm_Tunables.m_MinTimeSinceTargetTalkingEndedToMakeCall;
		float fMinTimeTargetHasBeenTalking		= sm_Tunables.m_MinTimeTargetHasBeenTalkingToMakeCall;
		if((m_fTimeSinceTargetTalkingEnded >= fMinTimeSinceTargetTalkingEnded) ||
			(m_fTimeTargetHasBeenTalking >= fMinTimeTargetHasBeenTalking))
		{
			//Make the call.
			SetState(State_MakeCall);
		}
	}

	return FSM_Continue;
}

void CTaskCallPolice::MakeCall_OnEnter()
{
	//Note that the ped has called police.
	GetPed()->GetPedIntelligence()->CalledPolice();

	//Say the audio for the commit.
	bool bIsPlayingAudio	= false;
	bool bPlayerHeardAudio	= false;
	SayAudio(GetAudioForCommit(), bIsPlayingAudio, bPlayerHeardAudio);

	//Update the flags.
	m_uRunningFlags.ChangeFlag(RF_IsPlayingAudioForCommit,		bIsPlayingAudio);
	m_uRunningFlags.ChangeFlag(RF_PlayerHeardAudioForCommit,	bPlayerHeardAudio);

	//Create the task.
	CTask* pTask = rage_new CTaskMobilePhone(CTaskMobilePhone::Mode_ToCall, -1, false);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskCallPolice::MakeCall_OnUpdate()
{
	//Check if the sub-task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Finish the task.
		SetState(State_Finish);
	}
	else
	{
		//Check if we should say the context for the call.
		if(ShouldSayContextForCall())
		{
			//Say the audio for the call.
			bool bIsPlayingAudio	= false;
			bool bPlayerHeardAudio	= false;
			SayAudio(GetAudioForCall(), bIsPlayingAudio, bPlayerHeardAudio);

			//Update the flags.
			m_uRunningFlags.ChangeFlag(RF_IsPlayingAudioForCall,	bIsPlayingAudio);
			m_uRunningFlags.ChangeFlag(RF_PlayerHeardAudioForCall,	bPlayerHeardAudio);

			//Set the flag.
			m_uRunningFlags.SetFlag(RF_SaidContextForCall);
		}
		//Check if we should put down the phone.
		else if(ShouldPutDownPhone())
		{
			//Put down the phone.
			PutDownPhone();

			//Note that the police were called.
			OnCalledPolice();
		}
	}

	return FSM_Continue;
}
