//Rage headers
#include "vector/vector3.h"

// Framework headers
#include "fwmaths/Angle.h"
#include "fwmaths/Vector.h"

//Game headers
#include "AI/Ambient/ConditionalAnimManager.h"
#include "Game/ModelIndices.h"
#include "game/Riots.h"
#include "Event/Events.h"
#include "Event/EventDamage.h"
#include "Event/EventReactions.h"
#include "event/EventWeapon.h"
#include "Peds/Ped.h"
#include "Peds/PedGeometryAnalyser.h"
#include "Peds/PedIntelligence.h"
#include "performance/clearinghouse.h"
#include "Physics/Physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "scene/world/GameWorld.h"
#include "Streaming/Streaming.h"
#include "Task/Default/TaskAmbient.h"
#include "Task/Default/TaskChat.h"
#include "Task/Animation/TaskAnims.h"
#include "task/combat/taskcombatmelee.h"
#include "Task/Combat/TaskInvestigate.h"
#include "Task/Combat/TaskNewCombat.h"
#include "Task/Combat/TaskReact.h"
#include "Task/General/Phone/TaskMobilePhone.h"
#include "Task/General/TaskBasic.h"
#include "Task/Vehicle/TaskCar.h"
#include "Task/Scenario/TaskScenario.h"
#include "Task/Response/TaskFlee.h"
#include "Task/Response/TaskGangs.h"
#include "Task/Response/TaskReactAndFlee.h"
#include "Task/Response/TaskReactToDeadPed.h"
#include "Task/Weapons/Gun/TaskGun.h"
#include "Task/Motion/Locomotion/TaskMotionPed.h"
#include "Task/Movement/TaskGoto.h"
#include "Task/Movement/TaskNavMesh.h"
#include "Task/Physics/TaskBlendFromNM.h"
#include "Task/Service/Police/TaskPolicePatrol.h"
#include "Task/Vehicle/TaskCarUtils.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Task/Vehicle/TaskExitVehicle.h"
#include "Task/General/TaskSecondary.h"
#include "Task/Combat/TaskThreatResponse.h"
#include "Task/Weapons/TaskSwapWeapon.h"
#include "Event/EventShocking.h"
#include "Event/EventWeapon.h"
#include "Vfx/Misc/Fire.h"
#include "Weapons/Weapon.h"
#include "network/Events/NetworkEventTypes.h"
#include "vehicleAi/task/TaskVehicleCruise.h"
#include "Vehicles/Metadata/VehicleMetadataManager.h"
#include "Vehicles/Metadata/VehicleSeatInfo.h"

AI_OPTIMISATIONS()

////////////////////
//Main combat task response - decides whether the ped should fight or run
////////////////////
CTaskThreatResponse::CTaskThreatResponse(const CPed* pTarget, float fCombatTime)
: m_DecisionCache()
, m_pTarget(pTarget)
, m_threatResponse(State_Invalid)
, m_pTargetInfo(NULL)
, m_fCombatTime(fCombatTime)
, m_fMoveBlendRatioForFlee(MOVEBLENDRATIO_RUN)
, m_bGoDirectlyToFlee(false)
, m_bSuppressSlowingForCorners(false)
, m_pInitialMoveTask(NULL)
, m_reactClipSetId(CLIP_SET_ID_INVALID)
, m_reactToFleeClipSetId(CLIP_SET_ID_INVALID)
, m_nThreatResponseOverride(TR_None)
, m_uConfigFlags(0)
, m_uConfigFlagsForCombat(0)
, m_uConfigFlagsForVehiclePursuit(0)
, m_uConfigFlagsForFlee(0)
, m_uConfigFlagsForVehicleFlee(0)
, m_uRunningFlags(0)
, m_FleeFlags(0)
, m_eIsResponseToEventType(EVENT_NONE)
, m_uTimerToFlee(0)
{
	SetInternalTaskType(CTaskTypes::TASK_THREAT_RESPONSE);
}

CTaskThreatResponse::~CTaskThreatResponse()
{
}

CTaskThreatResponse* CTaskThreatResponse::CreateForTargetDueToEvent(const CPed* pPed, const CPed* pTarget, const CEvent& rEvent)
{
	wantedDebugf3("CTaskThreatResponse::CreateForTargetDueToEvent pPed[%p] pTarget[%p] rEvent[%s]",pPed,pTarget,rEvent.GetName().c_str());

	//Ensure the target is valid.
	if(!pTarget)
	{
		return NULL;
	}

	//Ensure the ped is not the target.
	if(!aiVerifyf(pPed != pTarget, "The ped feels threatened by themselves due to event: %d.", rEvent.GetEventType()))
	{
		return NULL;
	}

	//Ensure the event is valid.
	switch(rEvent.GetEventType())
	{
		case EVENT_FRIENDLY_FIRE_NEAR_MISS:
		{
			//Note: This is required because sometimes this event is allowed through, even when not a 'near miss'.
			if(!static_cast<const CEventFriendlyFireNearMiss &>(rEvent).IsBulletInRange(*pPed))
			{
				return NULL;
			}

			break;
		}
		case EVENT_SHOCKING_POTENTIAL_BLAST:
		{
			if(pPed && pPed->ShouldBehaveLikeLaw())
			{
				CProjectile* pProjectile = static_cast<const CEventShockingPotentialBlast &>(rEvent).GetProjectile();
				if(pProjectile && pProjectile->GetDisableExplosionCrimes())
				{
					return NULL;
				}
			}
			break;
		}
		default:
		{
			break;
		}
	}

	//Create the task.
	CTaskThreatResponse* pTask = rage_new CTaskThreatResponse(pTarget);

	if(IsEventAnIndirectThreat(pPed, rEvent))
	{
		pTask->GetConfigFlags().SetFlag(CF_ThreatIsIndirect);
	}

	if(ShouldDisableReactBeforeFlee(pPed, pTarget))
	{
		pTask->GetConfigFlags().SetFlag(CF_DisableReactBeforeFlee);
	}

	//Check if we should set the friendly exception.
	if(ShouldSetFriendlyException(pPed, pTarget, rEvent))
	{
		pTask->GetConfigFlags().SetFlag(CF_SetFriendlyException);
	}

	//Check if we should set a force flee if unarmed exception.
	if(ShouldSetForceFleeIfUnarmed(pPed, rEvent))
	{
		pTask->GetConfigFlags().SetFlag(CF_ForceFleeIfNotArmed);
	}

	//Check if we should prefer to fight.
	if(ShouldPreferFight(pPed, rEvent))
	{
		pTask->GetConfigFlags().SetFlag(CF_PreferFight);
	}

	//Check if we should flee in vehicle after we jack it.
	if(ShouldFleeInVehicleAfterJack(pPed, rEvent))
	{
		//Flee in vehicle after jack.
		pTask->FleeInVehicleAfterJack();
	}

	//Check if the event should cause a reaction before the aim intro.
	bool bUseFlinchAimIntro		= false;
	bool bUseSurprisedAimIntro	= false;
	bool bUseSniperAimIntro		= false;
	if(CTaskCombat::ShouldReactBeforeAimIntroForEvent(pPed, rEvent, bUseFlinchAimIntro, bUseSurprisedAimIntro, bUseSniperAimIntro))
	{
		if(bUseFlinchAimIntro)
		{
			pTask->GetConfigFlagsForCombat().SetFlag(CTaskCombat::ComF_UseFlinchAimIntro);
		}
		else if(bUseSurprisedAimIntro)
		{
			pTask->GetConfigFlagsForCombat().SetFlag(CTaskCombat::ComF_UseSurprisedAimIntro);
		}
		else if(bUseSniperAimIntro)
		{
			pTask->GetConfigFlagsForCombat().SetFlag(CTaskCombat::ComF_UseSniperAimIntro);
		}
	}

	if(CTaskCombat::ShouldArrestTargetForEvent(pPed, pTarget, rEvent))
	{
		pTask->GetConfigFlagsForCombat().SetFlag(CTaskCombat::ComF_ArrestTarget);
	}

	if(CTaskCombat::IsAnImmediateMeleeThreat(pPed, pTarget, rEvent))
	{
		pTask->GetConfigFlagsForCombat().SetFlag(CTaskCombat::ComF_MeleeImmediateThreat);
	}

	if(CTaskSmartFlee::ShouldExitVehicleDueToEvent(pPed, pTarget, rEvent))
	{
		pTask->GetConfigFlagsForFlee().SetFlag(CTaskSmartFlee::CF_ForceLeaveVehicle);
		pTask->SetThreatResponseOverride(TR_Flee);
	}
	else
	{
		if(CTaskSmartFlee::ReverseInVehicleForEvent(pPed, rEvent))
		{
			pTask->GetConfigFlagsForFlee().SetFlag(CTaskSmartFlee::CF_ReverseInVehicle);
		}

		if(CTaskSmartFlee::AccelerateInVehicleForEvent(pPed, rEvent))
		{
			pTask->GetConfigFlagsForFlee().SetFlag(CTaskSmartFlee::CF_AccelerateInVehicle);
		}
	}
	
	if(CTaskSmartFlee::ShouldEnterLastUsedVehicleImmediatelyIfCloserThanTarget(pPed, rEvent))
	{
		pTask->GetConfigFlagsForFlee().SetFlag(CTaskSmartFlee::CF_EnterLastUsedVehicleImmediatelyIfCloserThanTarget);
	}

	if(CTaskSmartFlee::ShouldCowerDueToEvent(pPed, rEvent))
	{
		pTask->GetConfigFlagsForFlee().SetFlag(CTaskSmartFlee::CF_Cower);
	}

	if(CTaskSmartFlee::CanCallPoliceDueToEvent(pTarget, rEvent))
	{
		pTask->GetConfigFlagsForFlee().SetFlag(CTaskSmartFlee::CF_CanCallPolice);
	}

	if(CTaskSmartFlee::ShouldNeverEnterWaterDueToEvent(rEvent))
	{
		pTask->GetConfigFlagsForFlee().SetFlag(CTaskSmartFlee::CF_NeverEnterWater);
	}

	//Check if we should swerve.
	if(CTaskVehicleFlee::ShouldSwerveForEvent(pPed, rEvent))
	{
		pTask->GetConfigFlagsForVehicleFlee().SetFlag(CTaskVehicleFlee::CF_Swerve);
	}
	//Check if we should hesitate.
	else if(CTaskVehicleFlee::ShouldHesitateForEvent(pPed, rEvent))
	{
		pTask->GetConfigFlagsForVehicleFlee().SetFlag(CTaskVehicleFlee::CF_Hesitate);
	}

	//Set the move/blend ratio for flee.
	if (pPed->GetTaskData().GetIsFlagSet(CTaskFlags::AlwaysSprintWhenFleeingInThreatResponse))
	{
		pTask->SetMoveBlendRatioForFlee(MOVEBLENDRATIO_SPRINT);
	}
	else
	{
		pTask->SetMoveBlendRatioForFlee(GetMoveBlendRatioForFleeForEvent(rEvent));
	}

	pTask->SetIsResponseToEventType(static_cast<eEventType>(rEvent.GetEventType()));

	return pTask;
}

float CTaskThreatResponse::GetMoveBlendRatioForFleeForEvent(const CEvent& UNUSED_PARAM(rEvent))
{
	static float s_fMinMoveBlendRatio = 1.8f;
	static float s_fMaxMoveBlendRatio = 2.2f;
	return fwRandom::GetRandomNumberInRange(s_fMinMoveBlendRatio, s_fMaxMoveBlendRatio);
}

void CTaskThreatResponse::CleanUp()
{

}

aiTask* CTaskThreatResponse::Copy() const
{
	//Create the task.
	CTaskThreatResponse* pTask = rage_new CTaskThreatResponse(m_pTarget, m_fCombatTime);
	
	//Copy the response overrides.
	pTask->SetThreatResponseOverride(m_nThreatResponseOverride);
	
	//Copy the flags.
	pTask->SetConfigFlags(m_uConfigFlags);
	pTask->SetConfigFlagsForCombat(m_uConfigFlagsForCombat);
	pTask->SetConfigFlagsForVehiclePursuit(m_uConfigFlagsForVehiclePursuit);
	pTask->SetConfigFlagsForFlee(m_uConfigFlagsForFlee);
	pTask->SetConfigFlagsForVehicleFlee(m_uConfigFlagsForVehicleFlee);

	//Copy the flee MBR.
	pTask->SetMoveBlendRatioForFlee(m_fMoveBlendRatioForFlee);
	
	//Copy highest priority event type
	pTask->SetIsResponseToEventType(m_eIsResponseToEventType);

	return pTask;
}

void CTaskThreatResponse::DoAbort(const AbortPriority UNUSED_PARAM(iPriority), const aiEvent* UNUSED_PARAM(pEvent))
{
	m_bGoDirectlyToFlee=false;

	//Check if we are doing a react & flee.
	if(m_threatResponse == State_ReactAndFlee)
	{
		//Go directly to the flee state, when we resume.
		//We have already played our initial reaction.
		m_threatResponse = State_Flee;
	}
}

CTask::FSM_Return CTaskThreatResponse::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin

		// Analyse the threat and decide course of action
		FSM_State(State_Analysis)
			FSM_OnEnter
				Analysis_OnEnter();
			FSM_OnUpdate
				return Analysis_OnUpdate(pPed);

		// Resume
		FSM_State(State_Resume)
			FSM_OnUpdate
				return Resume_OnUpdate();

		// Attack the target
		FSM_State(State_Combat)
			FSM_OnEnter
				Combat_OnEnter(pPed);
			FSM_OnUpdate
				return Combat_OnUpdate(pPed);
			FSM_OnExit
				Combat_OnExit();

		// Flee the current target
		FSM_State(State_Flee)
			FSM_OnEnter
				Flee_OnEnter(pPed);
			FSM_OnUpdate
				return Flee_OnUpdate(pPed);
			FSM_OnExit
				Flee_OnExit();
				
		// React and Flee the current target
		FSM_State(State_ReactAndFlee)
			FSM_OnEnter
				ReactAndFlee_OnEnter(pPed);
			FSM_OnUpdate
				return ReactAndFlee_OnUpdate(pPed);
			FSM_OnExit
				ReactAndFlee_OnExit();

		// Finished, quit
		FSM_State(State_Finish)
			FSM_OnUpdate
				return Finish_OnUpdate(pPed);

	FSM_End
}

CTask::FSM_Return CTaskThreatResponse::ProcessPreFSM()
{
	//Check if the target is invalid.
	if(!m_pTarget)
	{
		//Check if the target must be valid.
		if(MustTargetBeValid())
		{
// Logs to try to catch B*2177569/B*2175380
#if __BANK
			CAILogManager::GetLog().Log("CTaskThreatResponse::ProcessPreFSM, Ped %s, Aborting TaskThreatResponse because target is not longer valid.\n", 
				AILogging::GetEntityDescription(GetPed()).c_str());
#endif // __BANK

			return FSM_Quit;
		}
	}

	if (m_fCombatTime > 0.0f && GetTimeRunning() >= m_fCombatTime)
	{
// Logs to try to catch B*2177569/B*2175380
#if __BANK
		CAILogManager::GetLog().Log("CTaskThreatResponse::ProcessPreFSM, Ped %s, Aborting TaskThreatResponse CombatTime %f is over.\n", 
			AILogging::GetEntityDescription(GetPed()).c_str(), m_fCombatTime);
#endif // __BANK

		return FSM_Quit;
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskThreatResponse::ProcessPostFSM()
{
	// Allow certain lod modes
	if(GetState() != State_Combat)
	{	
		CPed *pPed = GetPed(); //Get the ped ptr.

		pPed->GetPedAiLod().ClearBlockedLodFlag(CPedAILod::AL_LodPhysics);
		pPed->GetPedAiLod().ClearBlockedLodFlag(CPedAILod::AL_LodMotionTask);
	}

	//Reset the decision flag.
	m_uRunningFlags.ClearFlag(RF_MakeNewDecision);

	return FSM_Continue;
}


#if !__FINAL || __FINAL_LOGGING
const char * CTaskThreatResponse::GetStaticStateName( s32 iState )
{
	Assert(iState>=State_Analysis&&iState<=State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Analysis",
		"State_Resume",
		"State_Combat",
		"State_Flee",
		"State_ReactAndFlee",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif // !__FINAL

//-------------------------------------------------------------------------
// PH - I'VE TEMPORARILY MOVED THIS ACCROSS FROM IV - THIS WILL ULTIMATELY
//		BE RAVE'IFIED
// Returns true if this ped should retreat
// Based on 5 variations
//? = Jack Car Back? Fight Unarmed? Fight Melee Weapon? Run from Gun?
//1 = 0%,	0%,		0%,		0%
//2 = 25%,	50%,	25%,	0%
//3 = 50%,	100%,	50%,	0%
//4 = 75%,	100%,	50%,	50%
//5 = 100%, 100%,	100%,	100%
//float CTaskThreatResponse::ms_attackProbabilities[BRAVERY_MAX][CTaskThreatResponse::PA_Max] = 
//{
//	{0.0f,	0.0f,	0.0f},	// 0%,		0%,		0%
//	{0.1f,	0.1f,	0.0f},	// 50%,		25%,	0%
//	{0.2f,	0.2f,	0.0f},	// 100%,	50%,	0%
//	{0.3f,	0.3f,	0.0f},	// 100%,	50%,	50%
//	{1.0f,	1.0f,	1.0f}	// 100%,	100%,	100%
//
//};

CTaskThreatResponse::ThreatResponse CTaskThreatResponse::DetermineThreatResponseState(const CPed& rPed) const
{
	return (DetermineThreatResponseState(rPed, m_pTarget, m_nThreatResponseOverride, m_uConfigFlags, m_uRunningFlags));
}

CTaskThreatResponse::ThreatResponse CTaskThreatResponse::GetThreatResponse() const
{
	//Check the threat response.
	switch(m_threatResponse)
	{
		case State_Combat:
		{
			return TR_Fight;
		}
		case State_Flee:
		case State_ReactAndFlee:
		{
			return TR_Flee;
		}
		default:
		{
			return TR_None;
		}
	}
}

void CTaskThreatResponse::SetFleeFromCombat(bool bFleeFromCombat)				
{ 
	const CPed* pPed = GetPed();
	if (bFleeFromCombat && pPed && pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_DisableFleeFromCombat))
		return;

	taskDebugf2("Frame %d : %s : ped %s [0x%p] : Setting RF_FleeFromCombat to %d.", fwTimer::GetFrameCount(), __FUNCTION__, GetPed()->GetDebugName(), GetPed(), bFleeFromCombat);

	m_uRunningFlags.ChangeFlag(RF_FleeFromCombat, bFleeFromCombat);
}

bool CTaskThreatResponse::IsFleeing() const
{
	switch(GetState())
	{
		case State_Flee:
		case State_ReactAndFlee:
		{
			return true;
		}
		default:
		{
			return false;
		}
	}
}

void CTaskThreatResponse::FleeInVehicleAfterJack()
{
	//Set the flag.
	m_uConfigFlags.SetFlag(CF_FleeInVehicleAfterJack);

	//Set the combat flags.
	m_uConfigFlagsForCombat.SetFlag(CTaskCombat::ComF_QuitIfDriverJacked);
}

void CTaskThreatResponse::OnFriendlyFireNearMiss(const CEventFriendlyFireNearMiss& UNUSED_PARAM(rEvent))
{
	//Note that we were directly threatened.
	OnDirectlyThreatened();
}

void CTaskThreatResponse::OnGunShot(const CEventGunShot& rEvent)
{
	//Note that we perceived a gun.
	OnPerceivedGun();

	//If we have an arrest sub task then they might possibly be resisting arrest
	PossiblyIncreaseTargetWantedLevel(rEvent);
}

void CTaskThreatResponse::OnGunShotBulletImpact(const CEventGunShotBulletImpact& rEvent)
{
	//Note that a gun shot was heard.
	OnGunShot(rEvent);

	//Note that we were directly threatened.
	OnDirectlyThreatened();
}

void CTaskThreatResponse::OnGunShotWhizzedBy(const CEventGunShotWhizzedBy& rEvent)
{
	//Note that a gun shot was heard.
	OnGunShot(rEvent);

	//Note that we were directly threatened.
	OnDirectlyThreatened();
}

void CTaskThreatResponse::OnGunAimedAt(const CEventGunAimedAt& rEvent)
{
	//If we have an arrest sub task then they might possibly be resisting arrest
	PossiblyIncreaseTargetWantedLevel(rEvent);

	//Note that we were directly threatened.
	OnDirectlyThreatened();

	//Note that we perceived a gun.
	OnPerceivedGun();
}

void CTaskThreatResponse::SetMoveBlendRatioForFlee(float fMoveBlendRatio)
{
	//Ensure the move/blend ratio is changing.
	if(Abs(m_fMoveBlendRatioForFlee - fMoveBlendRatio) < SMALL_FLOAT)
	{
		return;
	}

	//Set the move/blend ratio.
	m_fMoveBlendRatioForFlee = fMoveBlendRatio;

	//Check the state.
	switch(GetState())
	{
		case State_Flee:
		{
			//Check if the sub-task is valid.
			CTaskSmartFlee* pSubTask = static_cast<CTaskSmartFlee *>(GetSubTask());
			if(pSubTask)
			{
				pSubTask->SetMoveBlendRatio(m_fMoveBlendRatioForFlee);
			}
			break;
		}
		case State_ReactAndFlee:
		{
			//Check if the sub-task is valid.
			CTaskReactAndFlee* pSubTask = static_cast<CTaskReactAndFlee *>(GetSubTask());
			if(pSubTask)
			{
				pSubTask->SetMoveBlendRatio(m_fMoveBlendRatioForFlee);
			}
			break;
		}
		default:
		{
			break;
		}
	}
}

bool CTaskThreatResponse::CanFight(const CPed& rPed, const CPed& rTarget)
{
	//This function should return true if it is *possible* for the ped to fight the target.
	//At the moment, this is possible as long as the rules do not force the ped to take another action.
	//This can be achieved through overrides, flags, etc.

	//Ensure the rules are not forcing a (non-fight) action.
	ThreatResponse nThreatResponse = ChooseThreatResponseFromRules(rPed, &rTarget, 0, 0);
	if((nThreatResponse != TR_None) && (nThreatResponse != TR_Fight))
	{
		return false;
	}

	return true;
}

CTaskThreatResponse::ThreatResponse CTaskThreatResponse::PreDetermineThreatResponseStateFromEvent(CPed& rPed, const CEvent& rEvent)
{
	// Pretend to create the response task so we can copy over the config flags.
	CEventResponse response;
	rEvent.CreateResponseTask(&rPed, response);

	CTaskThreatResponse::ThreatResponse iResponse = TR_None;

	aiTask* pResponseTask = response.GetEventResponse(CEventResponse::EventResponse);
	if (pResponseTask && pResponseTask->GetTaskType() == CTaskTypes::TASK_THREAT_RESPONSE)
	{
		CTaskThreatResponse* pTaskThreat = static_cast<CTaskThreatResponse*>(pResponseTask);
		iResponse = pTaskThreat->DetermineThreatResponseState(rPed);
	}

	return iResponse;
}

CTaskThreatResponse::ThreatResponse CTaskThreatResponse::DetermineThreatResponseState(const CPed& rPed, const CPed* pTarget, ThreatResponse nOverride, fwFlags16 uConfigFlags, fwFlags8 uRunningFlags)
{
#if __DEV
	//Check for the fight override.
	TUNE_GROUP_BOOL(TASK_THREAT_RESPONSE, bForceFight, false);
	if(bForceFight)
	{
		return TR_Fight;
	}

	//Check for the flee override.
	TUNE_GROUP_BOOL(TASK_THREAT_RESPONSE, bForceFlee, false);
	if(bForceFlee)
	{
		return TR_Flee;
	}
#endif

	//The threat response is determined using the following priorities:
	//	1) The rules indicating a certain action should always be taken.
	//	2) The override indicating a certain action should always be taken.
	//	3) The combat behavior indicating a certain action should always be taken.
	//	4) The attributes of the ped indicating a certain action should always be taken.
	//	5) The personality of the ped, which distributes the actions using weighted values.
	//	6) The fallbacks, which will give a default threat response.

	//Choose the threat response from the rules.
	ThreatResponse nThreatResponse = ChooseThreatResponseFromRules(rPed, pTarget, uConfigFlags, uRunningFlags);
	if(nThreatResponse != TR_None)
	{
		taskDebugf2("Frame %d : %s : ped %s [0x%p] : Chose to %s due to rules.", fwTimer::GetFrameCount(), __FUNCTION__, rPed.GetDebugName(), &rPed, ToString(nThreatResponse));

		return nThreatResponse;
	}

	//Choose the threat response from the override.
	nThreatResponse = nOverride;
	if(nThreatResponse != TR_None)
	{
		taskDebugf2("Frame %d : %s : ped %s [0x%p] : Chose to %s due to override.", fwTimer::GetFrameCount(), __FUNCTION__, rPed.GetDebugName(), &rPed, ToString(nThreatResponse));

		return nThreatResponse;
	}

	//Choose the threat response from the combat behavior.
	nThreatResponse = ChooseThreatResponseFromCombatBehavior(rPed);
	if(nThreatResponse != TR_None)
	{
		taskDebugf2("Frame %d : %s : ped %s [0x%p] : Chose to %s due to combat behavior.", fwTimer::GetFrameCount(), __FUNCTION__, rPed.GetDebugName(), &rPed, ToString(nThreatResponse));

		return nThreatResponse;
	}

	//Choose the threat response from the attributes.
	nThreatResponse = ChooseThreatResponseFromAttributes(rPed, pTarget, uConfigFlags, uRunningFlags);
	if(nThreatResponse != TR_None)
	{
		taskDebugf2("Frame %d : %s : ped %s [0x%p] : Chose to %s due to attributes.", fwTimer::GetFrameCount(), __FUNCTION__, rPed.GetDebugName(), &rPed, ToString(nThreatResponse));

		return nThreatResponse;
	}

	//Choose the threat response from the personality.
	nThreatResponse = ChooseThreatResponseFromPersonality(rPed, pTarget, uConfigFlags);
	if(nThreatResponse != TR_None)
	{
		taskDebugf2("Frame %d : %s : ped %s [0x%p] : Chose to %s due to personality.", fwTimer::GetFrameCount(), __FUNCTION__, rPed.GetDebugName(), &rPed, ToString(nThreatResponse));

		return nThreatResponse;
	}

	//Choose the threat response from the fallbacks.
	nThreatResponse = ChooseThreatResponseFromFallbacks(rPed, uConfigFlags);
	if(nThreatResponse != TR_None)
	{
		taskDebugf2("Frame %d : %s : ped %s [0x%p] : Chose to %s due to fallbacks.", fwTimer::GetFrameCount(), __FUNCTION__, rPed.GetDebugName(), &rPed, ToString(nThreatResponse));

		return nThreatResponse;
	}

	return TR_None;
}

int CTaskThreatResponse::ChooseStateToResumeTo(bool& bKeepSubTask) const
{
	//Keep the sub-task.
	bKeepSubTask = true;

	//Check if the sub-task is valid.
	const CTask* pSubTask = GetSubTask();
	if(pSubTask)
	{
		//Check the task type.
		switch(pSubTask->GetTaskType())
		{
			case CTaskTypes::TASK_COMBAT:
			{
				return State_Combat;
			}
			case CTaskTypes::TASK_REACT_AND_FLEE:
			{
				return State_ReactAndFlee;
			}
			case CTaskTypes::TASK_SMART_FLEE:
			{
				return State_Flee;
			}
			default:
			{
				break;
			}
		}
	}

	//Do not keep the sub-task.
	bKeepSubTask = false;

	return State_Analysis;
}

s32 CTaskThreatResponse::GetThreatResponseState(CPed* pPed)
{
	//Check if the threat response is valid.
	if(IsThreatResponseValid(pPed))
	{
		return m_threatResponse;
	}
	
	//Update the decision cache.
	BuildDecisionCache(m_DecisionCache, pPed);
	
	//Determine the threat response state.
	ThreatResponse nThreatResponse = DetermineThreatResponseState(*pPed, m_pTarget, m_nThreatResponseOverride, m_uConfigFlags, m_uRunningFlags);

	taskDebugf2("Frame %d : %s : ped %s [0x%p] : Determined a threat response state of %s.", fwTimer::GetFrameCount(), __FUNCTION__, pPed->GetDebugName(), pPed, ToString(nThreatResponse));

#if __BANK
	//Validate the threat response state.
	if(nThreatResponse == TR_Fight)
	{
		//Check if the weapon manager is invalid.
		if(!taskVerifyf(pPed->GetWeaponManager(), "Ped must have a weapon manager before entering combat."))
		{
			nThreatResponse = TR_Flee;
		}
	}
#endif

	//Check the threat response state.
	switch(nThreatResponse)
	{
		case TR_Fight:
		{
			m_threatResponse = State_Combat;
			break;
		}
		case TR_Flee:
		{
			m_threatResponse = ChooseFleeState(pPed);
			break;
		}
		default:
		{
			taskAssertf(false, "Invalid threat response: %d.", nThreatResponse);
			m_threatResponse = State_Invalid;
			break;
		}
	}

	taskDebugf2("Frame %d : %s : ped %s [0x%p] : Setting the threat response to state: %s.", fwTimer::GetFrameCount(), __FUNCTION__, pPed->GetDebugName(), pPed, GetStaticStateName(m_threatResponse));
	
	//Check if we have not made our initial decision.
	if(!m_uRunningFlags.IsFlagSet(RF_HasMadeInitialDecision))
	{
		//Update the wanted level.
		wantedDebugf3("CTaskThreatResponse::GetThreatResponseState-->SetTargetWantedLevel ped[%p] pPed[%p]", GetEntity() ? GetPed() : NULL, pPed);
		SetTargetWantedLevel(pPed);
		
		//Note that we have made our initial decision.
		m_uRunningFlags.SetFlag(RF_HasMadeInitialDecision);
	}

	//This flag is good for one decision at a time.
	m_uConfigFlags.ClearFlag(CF_DisableReactBeforeFlee);
	
	return m_threatResponse;
}

bool CTaskThreatResponse::AddGetUpSets( CNmBlendOutSetList& sets, CPed* pPed )
{
	if (pPed)
	{
		s32 responseState = GetThreatResponseState(pPed);

		switch(responseState)
		{
		case State_Combat:
			{
				if(pPed->GetWeaponManager())
				{
					const CEntity* pTarget = NULL;

					CTask* pSubTask = GetSubTask();
					if (pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_COMBAT)
					{
						CTaskCombat* pCombatTask = static_cast<CTaskCombat*>(pSubTask);
						pTarget = pCombatTask->GetPrimaryTarget();
					}

					if (!pTarget && m_pTarget)
					{
						pTarget = m_pTarget.Get();
					}

					if (pTarget)
					{
						CPedWeaponManager* pWeaponManager = pPed->GetWeaponManager();

						if (pWeaponManager->GetSelectedWeaponHash()==WEAPONTYPE_UNARMED)
							pWeaponManager->EquipBestWeapon(true, true);
						else
							pWeaponManager->EquipWeapon(pWeaponManager->GetWeaponSelector()->GetSelectedWeapon(), pWeaponManager->GetWeaponSelector()->GetSelectedVehicleWeapon(),true, true);

						const CWeaponInfo* pEquippedWeaponInfo = pWeaponManager->GetEquippedWeaponInfo();
						if (!pEquippedWeaponInfo || pEquippedWeaponInfo->GetIsMelee())
						{
							return CTaskMelee::SetupMeleeGetup(sets, pPed, pTarget);
						}
						else
						{
							return CTaskGun::SetupAimingGetupSet(sets, pPed, CAITarget(pTarget));
						}
					}
				}

				return false;
			}
			break;
		case State_ReactAndFlee:
		case State_Flee:
			{
				if (m_pTarget)
				{
					Vector3 targetPos = VEC3V_TO_VECTOR3(m_pTarget->GetMatrix().d());
					CTaskSmartFlee::SetupFleeingGetup(sets, targetPos, pPed);
					return true;
				}
				return false;

			}
			break;
		default:
			{
				//do nothing
			}
			break;
		}
	}

	// no custom blend out set specified. Use the default.
	return false;
}

void CTaskThreatResponse::GetUpComplete(eNmBlendOutSet setMatched, CNmBlendOutItem* UNUSED_PARAM(lastItem), CTaskMove* pMoveTask, CPed* pPed)
{
	taskAssert(pPed);

	if (setMatched==NMBS_WRITHE)
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_InstantBlendToAim, true);
		pPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAIUpdate, true); //need a post camera update to process the start state of the next task
		pPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true); //need a post camera update to process the start state of the next task
		pPed->SetPedResetFlag(CPED_RESET_FLAG_ePostCameraAnimUpdateUseZeroTimestep, true);
		m_threatResponse = State_Finish;
	}
	else if (setMatched==NMBS_PANIC_FLEE
		|| setMatched==NMBS_PANIC_FLEE_INJURED)
	{
		// set up for a directional fleeing getup
		m_bGoDirectlyToFlee = true;
		m_pInitialMoveTask = pMoveTask;
		
		//No need to react and flee after a get up, just do a regular flee.
		if(m_threatResponse == State_ReactAndFlee)
		{
			m_threatResponse = State_Flee;
		}
	}
	else if (setMatched==NMBS_ARMED_1HANDED_GETUPS
		|| setMatched==NMBS_ARMED_1HANDED_SHOOT_FROM_GROUND
		|| setMatched==NMBS_ARMED_2HANDED_GETUPS
		|| setMatched==NMBS_ARMED_2HANDED_SHOOT_FROM_GROUND
		)
	{

		//Force fire.
		m_uConfigFlagsForCombat.SetFlag(CTaskCombat::ComF_ForceFire);
		
		//Set the combat state.
		m_threatResponse = State_Combat;

		pPed->SetPedResetFlag(CPED_RESET_FLAG_InstantBlendToAim, true);
		pPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAIUpdate, true); //need a post camera update to process the start state of the next task
		pPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true); //need a post camera update to process the start state of the next task
		pPed->SetPedResetFlag(CPED_RESET_FLAG_ePostCameraAnimUpdateUseZeroTimestep, true);

		if (!pPed->IsPlayer() && pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetEquippedWeapon())
			pPed->GetWeaponManager()->GetEquippedWeapon()->DoReload(false);
	}
	else if (
		setMatched==NMBS_STANDING_AIMING_CENTRE
		|| setMatched==NMBS_STANDING_AIMING_LEFT
		|| setMatched==NMBS_STANDING_AIMING_RIGHT
		|| setMatched==NMBS_STANDING_AIMING_CENTRE_PISTOL_HURT
		|| setMatched==NMBS_STANDING_AIMING_LEFT_PISTOL_HURT
		|| setMatched==NMBS_STANDING_AIMING_RIGHT_PISTOL_HURT
		|| setMatched==NMBS_STANDING_AIMING_CENTRE_RIFLE_HURT
		|| setMatched==NMBS_STANDING_AIMING_LEFT_RIFLE_HURT
		|| setMatched==NMBS_STANDING_AIMING_RIGHT_RIFLE_HURT
		|| setMatched==NMBS_STANDING_AIMING_CENTRE_RIFLE
		|| setMatched==NMBS_STANDING_AIMING_LEFT_RIFLE
		|| setMatched==NMBS_STANDING_AIMING_RIGHT_RIFLE
		)
	{
		const CWeaponInfo* pWeaponInfo = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponInfo() : NULL;
		if (pWeaponInfo && !pWeaponInfo->GetIsUnarmed() && m_pTarget && (m_threatResponse==State_Combat))
		{
			pPed->SetPedResetFlag(CPED_RESET_FLAG_InstantBlendToAim, true);
			pPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAIUpdate, true); //need a post camera update to process the start state of the next task
			pPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true); //need a post camera update to process the start state of the next task
			pPed->SetPedResetFlag(CPED_RESET_FLAG_ePostCameraAnimUpdateUseZeroTimestep, true);
		}
	}
	else if (setMatched==NMBS_STANDING)
	{
		// standing flee
		if(m_threatResponse == State_ReactAndFlee || m_threatResponse == State_Flee)
		{
			pPed->SetPedResetFlag(CPED_RESET_FLAG_SkipReactInReactAndFlee, true);
			pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Idle);
			pPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAIUpdate, true); //need a post camera update to process the start state of the next task
			pPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true); //need a post camera update to process the start state of the next task
			pPed->SetPedResetFlag(CPED_RESET_FLAG_ePostCameraAnimUpdateUseZeroTimestep, true);
			m_threatResponse = State_Flee;
			m_pInitialMoveTask = pMoveTask;

			// Set the initial heading based on the target location. This will only be used until the navmesh route returns
			// and stops peds from running forward toward the target
			if (m_pTarget)
			{
				m_bSuppressSlowingForCorners = true;
				Vector3 targetPos = VEC3V_TO_VECTOR3(m_pTarget->GetMatrix().d());
				CPedMotionData* pMotionData = pPed->GetMotionData();
				pMotionData->SetDesiredMoveBlendRatio(3.0f, 0.0f);
				pPed->SetDesiredHeading(targetPos);
				pMotionData->SetDesiredHeading(pPed->GetMotionData()->GetDesiredHeading()+PI);
				pPed->SetPedResetFlag(CPED_RESET_FLAG_ForceMotionStateLeaveDesiredMBR, true);
			}
			
		}
	}
}

void CTaskThreatResponse::RequestGetupAssets(CPed* pPed)
{
	bool bArmed1Handed = false;
	bool bArmed2Handed = false;
	if (pPed && pPed->GetWeaponManager())
	{
		CPedWeaponManager const* pWeaponManager = pPed->GetWeaponManager();

		bArmed1Handed = pWeaponManager->GetIsArmed1Handed();
		bArmed2Handed = pWeaponManager->GetIsArmed2Handed();
	}

	int idx = 0;

	if(bArmed1Handed)
	{
		m_CombatGetupReqHelpers[idx++].Request(NMBS_ARMED_1HANDED_GETUPS);
		NetworkInterface::RequestNetworkAnimRequest(pPed, NMBS_ARMED_1HANDED_GETUPS, true);

		m_CombatGetupReqHelpers[idx++].Request(NMBS_ARMED_1HANDED_SHOOT_FROM_GROUND);
		NetworkInterface::RequestNetworkAnimRequest(pPed, NMBS_ARMED_1HANDED_SHOOT_FROM_GROUND);

		if(!pPed->ShouldDoHurtTransition())
		{
			m_CombatGetupReqHelpers[idx++].Request(NMBS_STANDING_AIMING_CENTRE);
			NetworkInterface::RequestNetworkAnimRequest(pPed, NMBS_STANDING_AIMING_CENTRE);
		}
		else
		{
			m_CombatGetupReqHelpers[idx++].Request(NMBS_STANDING_AIMING_CENTRE_PISTOL_HURT);
			NetworkInterface::RequestNetworkAnimRequest(pPed, NMBS_STANDING_AIMING_CENTRE_PISTOL_HURT);
		}
	}
	else if(bArmed2Handed)
	{
		m_CombatGetupReqHelpers[idx++].Request(NMBS_ARMED_2HANDED_GETUPS);
		NetworkInterface::RequestNetworkAnimRequest(pPed, NMBS_ARMED_2HANDED_GETUPS, true);

		m_CombatGetupReqHelpers[idx++].Request(NMBS_ARMED_2HANDED_SHOOT_FROM_GROUND);
		NetworkInterface::RequestNetworkAnimRequest(pPed, NMBS_ARMED_2HANDED_SHOOT_FROM_GROUND);

		if(!pPed->ShouldDoHurtTransition())
		{
			m_CombatGetupReqHelpers[idx++].Request(NMBS_STANDING_AIMING_CENTRE_RIFLE);
			NetworkInterface::RequestNetworkAnimRequest(pPed, NMBS_STANDING_AIMING_CENTRE_RIFLE);
		}
		else
		{
			m_CombatGetupReqHelpers[idx++].Request(NMBS_STANDING_AIMING_CENTRE_RIFLE_HURT);
			NetworkInterface::RequestNetworkAnimRequest(pPed, NMBS_STANDING_AIMING_CENTRE_RIFLE_HURT);
		}
	}
	else if(!bArmed1Handed && !bArmed2Handed)
	{
		if (pPed->ShouldDoHurtTransition())
		{
			m_FleeGetupReqHelper.Request(NMBS_WRITHE);
			NetworkInterface::RequestNetworkAnimRequest(pPed, NMBS_WRITHE, true);		
		}
		else
		{
			m_FleeGetupReqHelper.Request(NMBS_PANIC_FLEE);
			NetworkInterface::RequestNetworkAnimRequest(pPed, NMBS_PANIC_FLEE, true);
		}
	}

	if(pPed->ShouldDoHurtTransition())
	{
		m_CombatGetupReqHelpers[idx++].Request(NMBS_WRITHE);
		NetworkInterface::RequestNetworkAnimRequest(pPed, NMBS_WRITHE);
	}
}

bool CTaskThreatResponse::IsConsideredInCombat() 
{ 
	if (m_threatResponse == State_Combat)
		return true;
	else if (GetSubTask())
		return GetSubTask()->IsConsideredInCombat();

	return false;
}

bool CTaskThreatResponse::IsValidForMotionTask(CTaskMotionBase& task) const
{
	bool bIsValid = CTask::IsValidForMotionTask(task);

	if (!bIsValid && GetSubTask())
		bIsValid = GetSubTask()->IsValidForMotionTask(task);

	return bIsValid;
}

const CPedModelInfo::PersonalityThreatResponse* CTaskThreatResponse::GetThreatResponse(const CPed& rPed, const CPed* pTarget)
{	
	//Check the target armament.
	PedArmament nTargetArmament = GetTargetArmament(pTarget);
	switch(nTargetArmament)
	{
		case PA_Unarmed:
		{
			return &rPed.GetPedModelInfo()->GetPersonalitySettings().GetThreatResponseUnarmed();
		}
		case PA_Melee:
		{
			return &rPed.GetPedModelInfo()->GetPersonalitySettings().GetThreatResponseMelee();
		}
		case PA_Armed:
		{
			return &rPed.GetPedModelInfo()->GetPersonalitySettings().GetThreatResponseArmed();
		}
		default:
		{
			taskAssertf(false, "Unknown target armament: %d.", nTargetArmament);
			return NULL;
		}
	}
}

bool CTaskThreatResponse::CheckForResponseStateChange( CPed* pPed )
{
	s32 state = GetThreatResponseState(pPed);
	if( state != GetState() )
	{
		taskDebugf2("Frame %d : %s : ped %s [0x%p] : Setting state from %s to %s due to a response state change.", fwTimer::GetFrameCount(), __FUNCTION__, pPed->GetDebugName(), pPed, GetStaticStateName(GetState()), GetStaticStateName(state));

		SetState(state);
		return true;
	}
	return false;
}

void CTaskThreatResponse::Analysis_OnEnter()
{
	//Update the inventory.
	CRiots::GetInstance().UpdateInventory(*GetPed());

	//Check if we should set the friendly exception.
	if(m_uConfigFlags.IsFlagSet(CF_SetFriendlyException))
	{
		//Set the friendly exception.
		SetFriendlyException();
	}
}


CTask::FSM_Return CTaskThreatResponse::Analysis_OnUpdate( CPed* pPed )
{
	if(!CheckForResponseStateChange(pPed))
	{
		taskAssert(false);

		//Finish the task.
		SetState(State_Finish);
	}
	else
	{
		taskAssert(pPed);
		if (ShouldPedBroadcastRespondedToThreatEvent(*pPed))
		{
			BroadcastPedRespondedToThreatEvent(*pPed, m_pTarget);
		}
	}

	return FSM_Continue;
}


CTask::FSM_Return CTaskThreatResponse::Resume_OnUpdate()
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

void CTaskThreatResponse::Combat_OnEnter( CPed* pPed )
{
	// We shouldn't have this set when coming into combat initially
	m_uRunningFlags.ClearFlag(RF_FleeFromCombat);

	//Check if we are resuming the state.  Keep the sub-task, if possible.
	if(GetSubTask() && (GetSubTask()->GetTaskType() == CTaskTypes::TASK_COMBAT))
	{
		return;
	}

	//Ensure we are not friendly with the target.
	if(pPed->GetPedIntelligence()->IsFriendlyWith(*m_pTarget))
	{
		// Logs to try to catch B*2177569/B*2175380
#if __BANK
		CAILogManager::GetLog().Log("CTaskThreatResponse::Combat_OnEnter, Ped %s, target %s is friendly with the ped. Returning...\n", 
			AILogging::GetEntityDescription(GetPed()).c_str(), AILogging::GetEntityDescription(m_pTarget).c_str());
#endif // __BANK

		return;
	}

	static dev_s32 siMaxNumPedsToTest = 8;
	int iTestedPeds = 0;

	CEntityScannerIterator pedList = pPed->GetPedIntelligence()->GetNearbyPeds();
	for(CEntity* pEntity = pedList.GetFirst(); pEntity && iTestedPeds < siMaxNumPedsToTest; pEntity = pedList.GetNext(), ++iTestedPeds)
	{
		CPed * pNearbyPed = static_cast<CPed*>(pEntity);
		if (pNearbyPed->GetPedIntelligence()->IsFriendlyWith(*pPed))	// It's friendly to this ped
		{
			pPed->NewSay( "PLAYER_OVER_THERE" );
			break;
		}
	}

	// kill phone task when we enter combat
	if( CTaskMobilePhone::IsRunningMobilePhoneTask(*pPed) )
	{
		pPed->GetPedIntelligence()->ClearSecondaryTask(PED_TASK_SECONDARY_PARTIAL_ANIM);
	}

	if(!pPed->IsNetworkClone())
	{
		//Create the config flags for combat.
		fwFlags32 uConfigFlagsForCombat = m_uConfigFlagsForCombat;
		
		//Grab the threat response.
		const CPedModelInfo::PersonalityThreatResponse* pResponse = GetThreatResponse(*GetPed(), m_pTarget);
		if(pResponse)
		{
			//Check if the ped will draw their weapon when they are losing.
			if(fwRandom::GetRandomNumberInRange(0.0f, 1.0f) <= pResponse->m_Fight.m_ProbabilityDrawWeaponWhenLosing)
			{
				//Set the flag.
				uConfigFlagsForCombat.SetFlag(CTaskCombat::ComF_DrawWeaponWhenLosing);
			}
		}

		//Create the task.
		CTaskCombat* pTask = rage_new CTaskCombat(m_pTarget, 0.0f, uConfigFlagsForCombat);
		pTask->SetConfigFlagsForVehiclePursuit(m_uConfigFlagsForVehiclePursuit);

		//Start the task.
		SetNewTask(pTask);
	}
}

CTask::FSM_Return CTaskThreatResponse::Combat_OnUpdate( CPed* pPed )
{
	perfClearingHouse::Increment(perfClearingHouse::COMBAT_BEHAVIOURS);

	const bool bExitingVehicle = FindSubTaskOfType(CTaskTypes::TASK_EXIT_VEHICLE) ? true : false;
	if (bExitingVehicle)
		return FSM_Continue;

	// Check if we should force the ped to flee
	if(ShouldFleeFromCombat() || CheckIfEstablishedDecoy())
	{
		taskDebugf2("Frame %d : %s : ped %s [0x%p] : Setting state to State_Flee due to choosing to flee from combat.", fwTimer::GetFrameCount(), __FUNCTION__, pPed->GetDebugName(), pPed);

		//Move to the flee state.
		SetState(State_Flee);
	}
	//Check if the response has changed.
	else if(CheckForResponseStateChange(pPed))
	{
		//The state has been changed.
	}
	//Check if the sub-task has finished.
	else if(GetIsSubtaskFinished(CTaskTypes::TASK_COMBAT))
	{
		if(m_uConfigFlags.IsFlagSet(CF_FleeInVehicleAfterJack) && GetPed()->GetIsDrivingVehicle())
		{
			//Set the flee flags.
			m_uConfigFlagsForFlee.SetFlag(CTaskSmartFlee::CF_DisableExitVehicle);

			taskDebugf2("Frame %d : %s : ped %s [0x%p] : Setting state to State_Flee due to fleeing in vehicle after jack.", fwTimer::GetFrameCount(), __FUNCTION__, pPed->GetDebugName(), pPed);

			SetState(State_Flee);
		}
		else
		{
			// Logs to try to catch B*2177569/B*2175380
#if __BANK
			CAILogManager::GetLog().Log("CTaskThreatResponse::Combat_OnUpdate, Ped %s, Task combat finished. Aborting TaskThreatResponse.\n", 
				AILogging::GetEntityDescription(GetPed()).c_str());
#endif // __BANK

			SetState(State_Finish);
		}
	}
	else
	{
		// In some cases we'll want to update the target wanted level
		UpdateTargetWantedLevel(pPed);
	}

	return FSM_Continue;
}

void CTaskThreatResponse::Combat_OnExit()
{
	//Clear the force fire flag.  This should only apply once.
	m_uConfigFlagsForCombat.ClearFlag(CTaskCombat::ComF_ForceFire);

	//Clear the aim intro flags.  This should only happen on the initial event reaction.
	m_uConfigFlagsForCombat.ClearFlag(CTaskCombat::ComF_UseFlinchAimIntro);
	m_uConfigFlagsForCombat.ClearFlag(CTaskCombat::ComF_UseSurprisedAimIntro);

	//Check if we are now fleeing.
	if(IsFleeing())
	{
		//Say some audio.
		GetPed()->NewSay("GENERIC_FRIGHTENED_HIGH");
	}
}

void CTaskThreatResponse::Flee_OnEnter( CPed* pPed )
{
	//Check if we are resuming the state.  Keep the sub-task, if possible.
	if(GetSubTask() && (GetSubTask()->GetTaskType() == CTaskTypes::TASK_SMART_FLEE))
	{
		return;
	}

	//Generate the flee target.
	CAITarget target;
	GenerateFleeTarget(target);
	if(!target.GetIsValid())
	{
		return;
	}

	//Create the task.
	CTaskSmartFlee* pTask = rage_new CTaskSmartFlee(target);
	pTask->SetMoveBlendRatio(m_fMoveBlendRatioForFlee);
	pTask->SetFleeFlags(m_FleeFlags);
	pTask->SetConfigFlagsForVehicle(m_uConfigFlagsForVehicleFlee);

	//Set the config flags.
	pTask->SetConfigFlags(m_uConfigFlagsForFlee);

	static dev_bool bPullBackFromPed = false;
	pTask->SetPullBackFleeTargetFromPed(bPullBackFromPed);
	pTask->SetConsiderRunStartForPathLookAhead(true);

	if (m_pInitialMoveTask)
	{
		pTask->UseExistingMoveTask(m_pInitialMoveTask);
		m_pInitialMoveTask = NULL;
	}

	if (m_bGoDirectlyToFlee)
	{
		pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Run);
		m_bGoDirectlyToFlee = false;
	}

	SetNewTask( pTask );
}

CTask::FSM_Return CTaskThreatResponse::Flee_OnUpdate( CPed* pPed)
{
	// In some rare cases we can go back to combat when we are fleeing, check for that here
	if(ShouldReturnToCombatFromFlee())
	{
		taskDebugf2("Frame %d : %s : ped %s [0x%p] : Setting state to State_Combat due to choosing to return to combat from flee.", fwTimer::GetFrameCount(), __FUNCTION__, pPed->GetDebugName(), pPed);

		pPed->SetRemoveAsSoonAsPossible(false);

		SetState(State_Combat);
		return FSM_Continue;
	}

	if (m_bSuppressSlowingForCorners && GetTimeInState()<5.0f)
		pPed->SetPedResetFlag( CPED_RESET_FLAG_SuppressSlowingForCorners, true );

	if (pPed->GetTaskData().GetIsFlagSet(CTaskFlags::AlwaysSprintWhenFleeingInThreatResponse))
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_UseTighterTurnSettings, true);
	}

	//Check if we should make a new decision.
	if(m_uRunningFlags.IsFlagSet(RF_MakeNewDecision) && CheckForResponseStateChange(pPed))
	{
		//The state has been changed.
		m_uRunningFlags.ClearFlag(RF_MakeNewDecision);
	}
	//Check if the sub-task has finished.
	else if(!GetSubTask() || GetIsSubtaskFinished(CTaskTypes::TASK_SMART_FLEE))
	{
		//Finish the task.
		SetState(State_Finish);
	}

	return FSM_Continue;
}

void CTaskThreatResponse::Flee_OnExit()
{
	//Clear the 'can reverse in vehicle' flag.
	m_uConfigFlagsForFlee.ClearFlag(CTaskSmartFlee::CF_ReverseInVehicle);

	//Not sure, let's clear this one too just to be safe
	m_uConfigFlagsForFlee.ClearFlag(CTaskSmartFlee::CF_ForceLeaveVehicle);

	//Clear the vehicle flee flags.
	m_uConfigFlagsForVehicleFlee.ClearFlag(CTaskVehicleFlee::CF_Swerve);
	m_uConfigFlagsForVehicleFlee.ClearFlag(CTaskVehicleFlee::CF_Hesitate);

	m_bSuppressSlowingForCorners = false;
}

void CTaskThreatResponse::ReactAndFlee_OnEnter( CPed* pPed )
{
	//Check if we are resuming the state.  Keep the sub-task, if possible.
	if(GetSubTask() && (GetSubTask()->GetTaskType() == CTaskTypes::TASK_REACT_AND_FLEE))
	{
		return;
	}

	//Generate the flee target.
	CAITarget target;
	GenerateFleeTarget(target);
	if(!target.GetIsValid())
	{
		return;
	}
	
	//Choose the clip sets.
	if(!pPed->IsNetworkClone())
	{
		Assert(CLIP_SET_ID_INVALID == m_reactClipSetId);
		Assert(CLIP_SET_ID_INVALID == m_reactToFleeClipSetId);
		if(!ChooseReactAndFleeClipSets(m_reactClipSetId, m_reactToFleeClipSetId))
		{
			return;
		}
	}
	else
	{
		if((CLIP_SET_ID_INVALID == m_reactClipSetId) && (CLIP_SET_ID_INVALID == m_reactToFleeClipSetId))
		{
			return;
		}
	}
	
	fwFlags8 uConfigFlags = 0;

	if(pPed->GetPedResetFlag(CPED_RESET_FLAG_SkipReactInReactAndFlee))
	{
		uConfigFlags.SetFlag(CTaskReactAndFlee::CF_DisableReact);
		uConfigFlags.SetFlag(CTaskReactAndFlee::CF_AcquireExistingMoveTask);
	}
	 
	if (m_reactToFleeClipSetId == CLIP_SET_REACTION_SHOCKING_WALKS || pPed->GetPedResetFlag(CPED_RESET_FLAG_ForceForwardTransitionInReactAndFlee))
	{
		// Hack when using these reactions - they are not set up with a full set of react animations, only the facing forward ones.
		uConfigFlags.SetFlag(CTaskReactAndFlee::CF_ForceForwardReactToFlee);
	}

	//Create the task.
	CTaskReactAndFlee* pFleeTask = rage_new CTaskReactAndFlee(target, m_reactClipSetId, m_reactToFleeClipSetId, uConfigFlags, m_eIsResponseToEventType);
	pFleeTask->SetMoveBlendRatio(m_fMoveBlendRatioForFlee);
	pFleeTask->SetConfigFlagsForFlee(m_uConfigFlagsForFlee);

	//Start the task.
	SetNewTask(pFleeTask);
}

CTask::FSM_Return CTaskThreatResponse::ReactAndFlee_OnUpdate( CPed* pPed )
{
	// In some rare cases we can go back to combat when we are fleeing, check for that here
	if(ShouldReturnToCombatFromFlee())
	{
		taskDebugf2("Frame %d : %s : ped %s [0x%p] : Setting state to State_Combat due to choosing to return to combat from flee.", fwTimer::GetFrameCount(), __FUNCTION__, pPed->GetDebugName(), pPed);

		SetState(State_Combat);
		return FSM_Continue;
	}

	//Check if we should make a new decision.
	if(m_uRunningFlags.IsFlagSet(RF_MakeNewDecision) && CheckForResponseStateChange(pPed))
	{
		//The state has been changed.
	}
	//Check if the sub-task has finished.
	else if(!GetSubTask() || GetIsSubtaskFinished(CTaskTypes::TASK_REACT_AND_FLEE))
	{
		//Finish the task.
		SetState(State_Finish);
	}

	return FSM_Continue;
}

void CTaskThreatResponse::ReactAndFlee_OnExit()
{
	//Clear the react and flee clip sets.
	m_reactClipSetId		= CLIP_SET_ID_INVALID;
	m_reactToFleeClipSetId	= CLIP_SET_ID_INVALID;
}

CTask::FSM_Return CTaskThreatResponse::Finish_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	return FSM_Quit;
}

bool CTaskThreatResponse::ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	if (pPed->GetPedIntelligence()->GetEventResponseTask())
	{
		s32 iTaskType = pPed->GetPedIntelligence()->GetEventResponseTask()->GetTaskType();
		if (iTaskType == CTaskTypes::TASK_THREAT_RESPONSE)
		{
			if(pEvent)
			{
				//Update the move/blend ratio for flee.
				SetMoveBlendRatioForFlee(Max(m_fMoveBlendRatioForFlee, GetMoveBlendRatioForFleeForEvent(*((CEvent *)pEvent))));
			}

			// If there is not a physical response task, ignore the event completely
			if( pPed->GetPedIntelligence()->GetPhysicalEventResponseTask() == NULL )
			{
				// Allow cower to make the decision about whether or not to handle the event.
				if (!pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COWER))
				{
					if(pEvent)
						static_cast<CEvent*>(const_cast<aiEvent*>(pEvent))->Tick();
					return false;
				}
			}
			// If there is a physical response task just remove the behavioural response
			else 
			{
				pPed->GetPedIntelligence()->RemoveEventResponseTask();
			}
		}
	}

	return CTask::ShouldAbort( iPriority, pEvent);
}

void CTaskThreatResponse::BuildDecisionCache(DecisionCache& rCache, CPed* pPed) const
{
	//Update the ped.
	rCache.m_Ped.m_bIsCarryingGun = IsCarryingGun(*pPed);
	rCache.m_Ped.m_bAlwaysFlee = pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_AlwaysFlee);

	//Update the target armament.
	rCache.m_Target.m_nArmament = GetTargetArmament(m_pTarget);

	//Check if the target is valid.
	const CPed* pTarget = m_pTarget;
	if(pTarget)
	{
		//Check if the wanted is valid.
		const CWanted* pWanted = pTarget->GetPlayerWanted();
		if(pWanted)
		{
			//Update the randoms flee values.
			rCache.m_Target.m_bAllRandomsFlee				= pWanted->m_AllRandomsFlee;
			rCache.m_Target.m_bAllNeutralRandomsFlee		= pWanted->m_AllNeutralRandomsFlee;
			rCache.m_Target.m_bAllRandomsOnlyAttackWithGuns	= pWanted->m_AllRandomsOnlyAttackWithGuns;
		}
	}
}

bool CTaskThreatResponse::CanFightArmedPedsWhenNotArmed() const
{
	return CanFightArmedPedsWhenNotArmed(*GetPed(), m_uConfigFlags);
}

CTaskThreatResponse::ResponseState CTaskThreatResponse::ChooseFleeState(CPed* pPed) const
{
	//Check if we are making a new decision.
	if(m_uRunningFlags.IsFlagSet(RF_MakeNewDecision))
	{
		//Check if we are already fleeing.
		if(IsFleeing())
		{
			//Do not change the flee state.
			return ((ResponseState)GetState());
		}
	}

	//Ensure the ped is not in a vehicle.
	if(pPed->GetIsInVehicle())
	{
		return State_Flee;
	}

	//Ensure we are not swimming.
	if(pPed->GetIsSwimming())
	{
		return State_Flee;
	}

	//Ensure we are not supposed to cower instead of flee.
	if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_CowerInsteadOfFlee))
	{
		return State_Flee;
	}

	//Ensure that react & flee hasn't been disabled for this ped model type.
	if(pPed->GetTaskData().GetIsFlagSet(CTaskFlags::DisableReactAndFleeAnimation))
	{
		return State_Flee;
	}

	//Ensure react before flee has not been disabled.
	if(m_uConfigFlags.IsFlagSet(CF_DisableReactBeforeFlee))
	{
		return State_Flee;
	}

	//Check if we have recently evaded.
	static dev_u32 s_uMinTimeSinceLastEvaded = 500;
	if(CTimeHelpers::GetTimeSince(pPed->GetPedIntelligence()->GetLastTimeEvaded()) < s_uMinTimeSinceLastEvaded)
	{
		return State_Flee;
	}

	//Check if we have made our initial decision.
	if(m_uRunningFlags.IsFlagSet(RF_HasMadeInitialDecision))
	{
		//Check if we are fleeing from combat.
		if((GetState() == State_Combat) && !IsArmedWithGun(*pPed))
		{
			return State_ReactAndFlee;
		}
		else
		{
			return State_Flee;
		}
	}

	// Do a react and flee for all shocking events.
	CEvent* pEvent = pPed->GetPedIntelligence()->GetCurrentEvent();
	if(pEvent && pEvent->IsShockingEvent())
	{
		return State_ReactAndFlee;
	}

	//Look at the ped's event.
	int iEventType = pPed->GetPedIntelligence()->GetCurrentEventType();
	
	//Certain events cause the peds to react and flee.
	switch(iEventType)
	{
		case EVENT_AGITATED_ACTION:
		case EVENT_CRIME_CRY_FOR_HELP:
		case EVENT_GUN_AIMED_AT:
		case EVENT_MELEE_ACTION:
		case EVENT_RESPONDED_TO_THREAT:
		case EVENT_SHOT_FIRED:
		case EVENT_SHOT_FIRED_BULLET_IMPACT:
		case EVENT_SHOT_FIRED_WHIZZED_BY:
		{
			return State_ReactAndFlee;
		}
		default:
		{
			break;
		}
	}
	
	return State_Flee;
}

bool CTaskThreatResponse::ChooseReactAndFleeClipSets(fwMvClipSetId& reactClipSetId, fwMvClipSetId& reactToFleeClipSetId) const
{
	const CPed* pPed = GetPed();

	// Pick react and flee transition clips for shocking events.
	// Could do this in the switch below, but checking explicitly seems more easily maintainable.
	CEvent* pEvent = pPed->GetPedIntelligence()->GetCurrentEvent();
	if (pEvent)
	{
		if (pEvent->IsShockingEvent())
		{
			CEventShocking* pShockingEvent = static_cast<CEventShocking*>(pEvent);
			// Shocking events use the "walk" react and flee clips (except for those tagged to use gunfire reactions),
			// but if a ped is coming out of a scenario force it to be the gunfire runs as there aren't scenario react clips that can be played to transition from.
			if (pPed->GetPedResetFlag(CPED_RESET_FLAG_SkipReactInReactAndFlee) || CEventShocking::ShouldUseGunfireReactsWhenFleeing(*pShockingEvent, *pPed))
			{
				reactClipSetId = CTaskReactAndFlee::PickDefaultGunfireReactionClipSet(m_pTarget->GetTransform().GetPosition(), *pPed);
				reactToFleeClipSetId = CLIP_SET_REACTION_GUNFIRE_RUNS;
			}
			else
			{
				reactClipSetId = CTaskReactAndFlee::PickDefaultShockingEventReactionClipSet(m_pTarget->GetTransform().GetPosition(), *pPed);
				reactToFleeClipSetId = CLIP_SET_REACTION_SHOCKING_WALKS;
			}
			//Check to make sure the default reaction clipset was valid.
			if (reactClipSetId != CLIP_SET_ID_INVALID)
			{
				return true;
			}
			else
			{
				return false;
			}
		}
	}

	//Look at the ped's event.
	int iEventType = pPed->GetPedIntelligence()->GetCurrentEventType();

	//Assign the correct clip sets.
	switch(iEventType)
	{
		case EVENT_AGITATED_ACTION:
		case EVENT_CRIME_CRY_FOR_HELP:
		case EVENT_GUN_AIMED_AT:
		case EVENT_MELEE_ACTION:
		case EVENT_RESPONDED_TO_THREAT:
		case EVENT_SHOT_FIRED:
		case EVENT_SHOT_FIRED_BULLET_IMPACT:
		case EVENT_SHOT_FIRED_WHIZZED_BY:
		{
			//B*1835929: Pick from one of three reactions randomly
			int iRandomReaction = fwRandom::GetRandomNumberInRange(0, 3);
			switch (iRandomReaction)
			{
			case 0:
				reactClipSetId = CTaskReactAndFlee::PickDefaultGunfireReactionClipSet(m_pTarget->GetTransform().GetPosition(), *pPed);
				reactToFleeClipSetId = CLIP_SET_REACTION_GUNFIRE_RUNS;
				break;
			case 1:
				reactClipSetId = CTaskReactAndFlee::PickDefaultGunfireReactionClipSet(m_pTarget->GetTransform().GetPosition(), *pPed, iRandomReaction);
				reactToFleeClipSetId = CLIP_SET_REACTION_GUNFIRE_RUNS_V1;
				break;
			case 2:
				reactClipSetId = CTaskReactAndFlee::PickDefaultGunfireReactionClipSet(m_pTarget->GetTransform().GetPosition(), *pPed, iRandomReaction);
				reactToFleeClipSetId = CLIP_SET_REACTION_GUNFIRE_RUNS_V2;
				break;
			default:
				reactClipSetId = CTaskReactAndFlee::PickDefaultGunfireReactionClipSet(m_pTarget->GetTransform().GetPosition(), *pPed);
				reactToFleeClipSetId = CLIP_SET_REACTION_GUNFIRE_RUNS;
				break;
			}

			//Check to make sure the default reaction clipset was valid.
			if (reactClipSetId != CLIP_SET_ID_INVALID)
			{
				return true;
			}
			else
			{
				return false;
			}
		}
		default:
		{
			break;
		}
	}

	//Check if we are doing a react & flee from combat.
	if(GetPreviousState() == State_Combat)
	{
		//Use the gunfire reactions.
		reactClipSetId = CTaskReactAndFlee::PickDefaultGunfireReactionClipSet(m_pTarget->GetTransform().GetPosition(), *pPed);
		reactToFleeClipSetId = CLIP_SET_REACTION_GUNFIRE_RUNS;

		return (reactToFleeClipSetId != CLIP_SET_ID_INVALID) ? true : false;
	}

	return false;
}

bool CTaskThreatResponse::CanFightArmedPedsWhenNotArmed(const CPed& rPed, fwFlags16 uConfigFlags)
{
	//Check if the combat flag is set.
	if(rPed.GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanFightArmedPedsWhenNotArmed))
	{
		return true;
	}

	//Check if the config flag is set.
	if(uConfigFlags.IsFlagSet(CF_CanFightArmedPedsWhenNotArmed))
	{
		return true;
	}

	//Check if the ped is in a group.
	const CPedGroup* pGroup = rPed.GetPedsGroup();
	if(pGroup && pGroup->IsPlayerGroup())
	{
		return true;
	}

	return false;
}

CTaskThreatResponse::ThreatResponse CTaskThreatResponse::ChooseThreatResponseFromAttributes(const CPed& rPed, const CPed* pTarget, fwFlags16 uConfigFlags, fwFlags8 uRunningFlags)
{
	//Check if the ped should always fight.
	if(ShouldAlwaysFightDueToAttributes(rPed))
	{
		taskDebugf2("Frame %d : %s : ped %s [0x%p] : Will always fight due to attributes.", fwTimer::GetFrameCount(), __FUNCTION__, rPed.GetDebugName(), &rPed);

		return TR_Fight;
	}
	//Check if the ped should always flee.
	else if(ShouldAlwaysFleeDueToAttributes(rPed, pTarget, uConfigFlags, uRunningFlags))
	{
		taskDebugf2("Frame %d : %s : ped %s [0x%p] : Will always flee due to attributes.", fwTimer::GetFrameCount(), __FUNCTION__, rPed.GetDebugName(), &rPed);

		return TR_Flee;
	}

	return TR_None;
}

CTaskThreatResponse::ThreatResponse CTaskThreatResponse::ChooseThreatResponseFromCombatBehavior(const CPed& rPed)
{
	//Grab the combat behavior.
	const CCombatBehaviour& rCombatBehavior = rPed.GetPedIntelligence()->GetCombatBehaviour();
	
	//Check if the ped should always fight.
	if(rCombatBehavior.IsFlagSet(CCombatData::BF_AlwaysFight))
	{
		taskDebugf2("Frame %d : %s : ped %s [0x%p] : Chose to fight due to BF_AlwaysFight.", fwTimer::GetFrameCount(), __FUNCTION__, rPed.GetDebugName(), &rPed);

		return TR_Fight;
	}
	//Check if the ped should always flee.
	else if(rCombatBehavior.IsFlagSet(CCombatData::BF_AlwaysFlee))
	{
		taskDebugf2("Frame %d : %s : ped %s [0x%p] : Chose to flee due to BF_AlwaysFlee.", fwTimer::GetFrameCount(), __FUNCTION__, rPed.GetDebugName(), &rPed);

		return TR_Flee;
	}
	else
	{
		return TR_None;
	}
}

CTaskThreatResponse::ThreatResponse CTaskThreatResponse::ChooseThreatResponseFromFallbacks(const CPed& rPed, fwFlags16 uConfigFlags)
{
	//Check if we prefer a specific action.
	if(uConfigFlags.IsFlagSet(CF_PreferFight))
	{
		taskDebugf2("Frame %d : %s : ped %s [0x%p] : Chose to fight due to CF_PreferFight.", fwTimer::GetFrameCount(), __FUNCTION__, rPed.GetDebugName(), &rPed);

		return TR_Fight;
	}
	else if(uConfigFlags.IsFlagSet(CF_PreferFlee))
	{
		taskDebugf2("Frame %d : %s : ped %s [0x%p] : Chose to flee due to CF_PreferFlee.", fwTimer::GetFrameCount(), __FUNCTION__, rPed.GetDebugName(), &rPed);

		return TR_Flee;
	}

	//Check if the ped is carrying a gun.
	if(IsCarryingGun(rPed))
	{
		taskDebugf2("Frame %d : %s : ped %s [0x%p] : Chose to fight due to carrying a gun.", fwTimer::GetFrameCount(), __FUNCTION__, rPed.GetDebugName(), &rPed);

		return TR_Fight;
	}
	else
	{
		taskDebugf2("Frame %d : %s : ped %s [0x%p] : Chose to flee due to not carrying a gun.", fwTimer::GetFrameCount(), __FUNCTION__, rPed.GetDebugName(), &rPed);

		return TR_Flee;
	}
}

CTaskThreatResponse::ThreatResponse CTaskThreatResponse::ChooseThreatResponseFromPersonality(const CPed& rPed, const CPed* pTarget, fwFlags16 uConfigFlags)
{
	//Ensure the response is valid.
	const CPedModelInfo::PersonalityThreatResponse* pResponse = GetThreatResponse(rPed, pTarget);
	if(!pResponse)
	{
		return TR_None;
	}
	
	//Generate the array of weights.
	atRangeArray<float, TR_Max> aWeights;
	aWeights[TR_Fight]	= !ShouldNeverFight(uConfigFlags)		? pResponse->m_Action.m_Weights.m_Fight	: 0.0f;
	aWeights[TR_Flee]	= !ShouldNeverFlee(rPed, uConfigFlags)	? pResponse->m_Action.m_Weights.m_Flee	: 0.0f;

	//Modify the fight weight.
	aWeights[TR_Fight] *= GetFightModifier(rPed);
	
	//Modify the flee weight.
	aWeights[TR_Flee] *= GetTargetFleeModifier(pTarget);

	//Check if we prefer to fight, and we have a chance to fight.
	if(uConfigFlags.IsFlagSet(CF_PreferFight) && (aWeights[TR_Fight] > SMALL_FLOAT))
	{
		return TR_Fight;
	}
	//Check if we prefer to flee, and we have a chance to flee.
	else if(uConfigFlags.IsFlagSet(CF_PreferFlee) && (aWeights[TR_Flee] > SMALL_FLOAT))
	{
		return TR_Flee;
	}
	
	//Generate the total weight.
	float fTotalWeight = 0.0f;
	for(int iState = TR_Fight; iState < TR_Max; ++iState)
	{
		fTotalWeight += aWeights[iState];
	}
	
	//Ensure the total weight is valid.
	if(fTotalWeight < SMALL_FLOAT)
	{
		return TR_None;
	}

	//Generate a random number.
	float fRand = fwRandom::GetRandomNumberInRange(0.0f, fTotalWeight);

	//Choose the state.
	for(int iState = TR_Fight; iState < TR_Max; ++iState)
	{
		//Grab the weight.
		const float fWeight = aWeights[iState];
		if(fWeight <= 0.0f)
		{
			continue;
		}

		//Subtract the weight.
		fRand -= fWeight;
		if(fRand <= 0.0f)
		{
			return (ThreatResponse)iState;
		}
	}
	
	return TR_None;
}

CTaskThreatResponse::ThreatResponse CTaskThreatResponse::ChooseThreatResponseFromRules(const CPed& rPed, const CPed* pTarget, fwFlags16 uConfigFlags, fwFlags8 uRunningFlags)
{
	//Check if the ped should always fight.
	if(ShouldAlwaysFightDueToRules(rPed))
	{
		return TR_Fight;
	}
	//Check if the ped should always flee.
	else if(ShouldAlwaysFleeDueToRules(rPed, pTarget, uConfigFlags, uRunningFlags))
	{
		return TR_Flee;
	}

	return TR_None;
}

CTaskThreatResponse::PedArmament CTaskThreatResponse::GetArmamentForPed(const CPed& rPed, PedArmamentSelector nPedArmamentSelector)
{
	//Get the armament.
	CWeaponInfoHelper::Armament nArmament = CWeaponInfoHelper::A_None;
	switch(nPedArmamentSelector)
	{
		case PAS_Equipped:
		{
			nArmament = CWeaponInfoHelper::A_Equipped;
			break;
		}
		case PAS_Best:
		{
			nArmament = CWeaponInfoHelper::A_Best;
			break;
		}
		default:
		{
			break;
		}
	}

	//Get the state.
	CWeaponInfoHelper::State nState = CWeaponInfoHelper::CalculateState(rPed, nArmament);
	switch(nState)
	{
		case CWeaponInfoHelper::S_Melee:
		{
			return PA_Melee;
		}
		case CWeaponInfoHelper::S_Armed:
		{
			return PA_Armed;
		}
		case CWeaponInfoHelper::S_None:
		case CWeaponInfoHelper::S_NonViolent:
		case CWeaponInfoHelper::S_Unarmed:
		default:
		{
			return PA_Unarmed;
		}
	}
}

CTaskThreatResponse::PedArmament CTaskThreatResponse::GetTargetArmament(const CPed* pTarget)
{
	//Ensure the target is valid.
	if(!pTarget)
	{
		return PA_Unarmed;
	}
	
	return GetArmamentForPed(*pTarget);
}

float CTaskThreatResponse::GetFightModifier(const CPed& rPed)
{
	//Initialize the modifier.
	float fModifier = 1.0f;

	//Check if the ped is brave in a group.
	if(rPed.CheckBraveryFlags(BF_BOOST_BRAVERY_IN_GROUP))
	{
		//Count our nearby friends.
		u8 uNearbyFriends = rPed.GetPedIntelligence()->CountFriends(10.0f, true);
		
		//Apply the modifier.
		fModifier *= (uNearbyFriends + 1);
	}

	return fModifier;
}

float CTaskThreatResponse::GetTargetFleeModifier(const CPed* pTarget)
{
	//Ensure the target is valid.
	if(!pTarget)
	{
		return 1.0f;
	}
	
	//Ensure the target is a player.
	if(!pTarget->IsPlayer())
	{
		return 1.0f;
	}
	
	//Ensure the ped model info is valid.
	const CPedModelInfo* pPedModelInfo = pTarget->GetPedModelInfo();
	if(!pPedModelInfo)
	{
		return 1.0f;
	}
	
	return (pPedModelInfo->GetPersonalitySettings().GetPopulationFleeMod());
}

bool CTaskThreatResponse::IsArmed(const CPed& rPed)
{
	//Ensure the weapon manager is valid.
	const CPedWeaponManager* pWeaponManager = rPed.GetWeaponManager();
	if(!pWeaponManager)
	{
		return false;
	}

	return pWeaponManager->GetIsArmed();
}

bool CTaskThreatResponse::IsArmedWithGun(const CPed& rPed)
{
	//Ensure the weapon manager is valid.
	const CPedWeaponManager* pWeaponManager = rPed.GetWeaponManager();
	if(!pWeaponManager)
	{
		return false;
	}

	return pWeaponManager->GetIsArmedGun();
}

bool CTaskThreatResponse::IsCarryingGun(const CPed& rPed)
{
	//Ensure the weapon manager is valid.
	const CPedWeaponManager* pWeaponManager = rPed.GetWeaponManager();
	if(!pWeaponManager)
	{
		return false;
	}
	
	//Ensure the best weapon info is valid.
	const CWeaponInfo* pWeaponInfo = pWeaponManager->GetBestWeaponInfo();
	if(!pWeaponInfo)
	{
		return false;
	}
	
	//Ensure the weapon is a gun.
	if(!pWeaponInfo->GetIsGun())
	{
		return false;
	}
	
	return true;
}

void CTaskThreatResponse::ClearFriendlyException()
{
	//Clear the friendly exception.
	GetPed()->GetPedIntelligence()->SetFriendlyException(NULL);
}

void CTaskThreatResponse::GenerateFleeTarget(CAITarget& rTarget) const
{
	//Check if we are a random civilian.
	if(GetPed()->PopTypeIsRandom() && GetPed()->IsCivilianPed())
	{
		//Check if the target is law enforcement, and is in combat.
		if(m_pTarget && m_pTarget->IsLawEnforcementPed() &&
			m_pTarget->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT))
		{
			//Check if the target is valid.
			const CEntity* pTarget = m_pTarget->GetPedIntelligence()->GetQueriableInterface()->GetTargetForTaskType(CTaskTypes::TASK_COMBAT);
			if(pTarget)
			{
				rTarget.SetEntity(pTarget);
				return;
			}
		}

		//Check if the target is not a player.
		if(m_pTarget && !m_pTarget->IsPlayer())
		{
			//Check if the player is armed.
			const CPed* pPlayer = CGameWorld::FindLocalPlayer();
			if(pPlayer && IsArmed(*pPlayer))
			{
				rTarget.SetEntity(pPlayer);
				return;
			}
		}
	}

	//Use the target.
	if(m_pTarget)
	{
		rTarget.SetEntity(m_pTarget);
	}
}

bool CTaskThreatResponse::IsThreatResponseValid(CPed* pPed) const
{
	//Ensure the threat response is valid.
	if(m_threatResponse < 0)
	{
		return false;
	}

	//Ensure we are not making a new decision.
	if(m_uRunningFlags.IsFlagSet(RF_MakeNewDecision))
	{
		return false;
	}
	
	//Ensure we should not make a new decision due to the cache.
	if(ShouldMakeNewDecisionDueToCache(pPed))
	{
		return false;
	}
	
	return true;
}

bool CTaskThreatResponse::MustTargetBeValid() const
{
	//Ensure we are not fleeing.
	if(IsFleeing())
	{
		return false;
	}

	return true;
}

void CTaskThreatResponse::OnDirectlyThreatened()
{
	//Check if we should make a new decision.
	bool bMakeNewDecision = IsFleeing() && m_uConfigFlags.IsFlagSet(CF_ThreatIsIndirect) &&
		!m_uRunningFlags.IsFlagSet(RF_HasBeenDirectlyThreatened) && !ShouldFleeFromCombat();
	if(bMakeNewDecision)
	{
		//Check if we are a random gang ped that is friendly with our target.
		if(GetPed()->PopTypeIsRandom() && GetPed()->IsGangPed() &&
			m_pTarget && GetPed()->GetPedIntelligence()->IsFriendlyWith(*m_pTarget) && !NetworkInterface::IsGameInProgress())
		{
			//Set the friendly exception.
			SetFriendlyException();
		}

		//Make a new decision.
		m_uRunningFlags.SetFlag(RF_MakeNewDecision);
	}

	//Note that we have been directly threatened.
	m_uRunningFlags.SetFlag(RF_HasBeenDirectlyThreatened);
}

void CTaskThreatResponse::OnPerceivedGun()
{
	//Check if we are in combat.
	if(GetState() == State_Combat)
	{
		//Check if we can't fight armed peds when not armed, and we are not armed.
		if(!CanFightArmedPedsWhenNotArmed() && (GetArmamentForPed(*GetPed(), PAS_Best) != PA_Armed))
		{
			//Flee from combat.
			SetFleeFromCombat(true);
		}
	}
}

bool CTaskThreatResponse::ShouldAlwaysFightDueToAttributes(const CPed& rPed)
{
	//Check if the ped type is mission.
	if(rPed.PopTypeIsMission())
	{
		taskDebugf2("Frame %d : %s : ped %s [0x%p] : Will always fight due to being a mission ped.", fwTimer::GetFrameCount(), __FUNCTION__, rPed.GetDebugName(), &rPed);

		return true;
	}

	//Check if the ped is hurt.
	if(rPed.HasHurtStarted())
	{
		taskDebugf2("Frame %d : %s : ped %s [0x%p] : Will always fight due to being hurt.", fwTimer::GetFrameCount(), __FUNCTION__, rPed.GetDebugName(), &rPed);

		return true;
	}
	
	return false;
}

bool CTaskThreatResponse::ShouldAlwaysFightDueToRules(const CPed& rPed)
{
	CVehicle* pVehicle = rPed.GetVehiclePedInside();
	if(pVehicle && pVehicle->IsTank())
	{
		taskDebugf2("Frame %d : %s : ped %s [0x%p] : Will always fight due to being in a tank.", fwTimer::GetFrameCount(), __FUNCTION__, rPed.GetDebugName(), &rPed);

		return true;
	}

	return false;
}

bool CTaskThreatResponse::ShouldAlwaysFleeDueToAttributes(const CPed& rPed, const CPed* pTarget, fwFlags16 uConfigFlags, fwFlags8 uRunningFlags)
{
	//Check if we should flee due to indirect threats.
	if(ShouldFleeFromIndirectThreats(rPed, pTarget, uConfigFlags, uRunningFlags))
	{
		taskDebugf2("Frame %d : %s : ped %s [0x%p] : Will always flee due to indirect threats.", fwTimer::GetFrameCount(), __FUNCTION__, rPed.GetDebugName(), &rPed);

		return true;
	}

	//Check if we are in a vehicle.
	if(rPed.GetIsInVehicle())
	{
		//Check if abnormal exits are disabled.
		const CVehicleSeatAnimInfo* pVehicleSeatAnimInfo = CVehicleMetadataMgr::GetSeatAnimInfoFromPed(&rPed);
		if(pVehicleSeatAnimInfo && pVehicleSeatAnimInfo->GetDisableAbnormalExits())
		{
			taskDebugf2("Frame %d : %s : ped %s [0x%p] : Will always flee due to abnormal exits being disabled for the vehicle seat.", fwTimer::GetFrameCount(), __FUNCTION__, rPed.GetDebugName(), &rPed);

			return true;
		}
	}

	return false;
}

bool CTaskThreatResponse::ShouldAlwaysFleeDueToRules(const CPed& rPed, const CPed* pTarget, fwFlags16 uConfigFlags, fwFlags8 uRunningFlags)
{
#if AI_DEBUG_OUTPUT_ENABLED
	char szEventDesc[256] = "";
	const CEvent* pEvent = rPed.GetPedIntelligence()->GetCurrentEvent();
	if (pEvent)
	{
		sprintf(szEventDesc, "CurrentEvent: (%s, S:%s)", pEvent->GetDescription().c_str(), AILogging::GetEntityDescription(pEvent->GetSourceEntity()).c_str());
	}
#endif // 

	//If we don't have a target then we must flee
	if(!pTarget)
	{

#if AI_DEBUG_OUTPUT_ENABLED
		taskDebugf2("Frame %d : %s : ped %s [0x%p] : Will always flee due to no target [%s], CurrentEvent [%s]", fwTimer::GetFrameCount(), __FUNCTION__, rPed.GetDebugName(), &rPed, AILogging::GetEntityDescription(pTarget).c_str(), szEventDesc);
#else
		taskDebugf2("Frame %d : %s : ped %s [0x%p] : Will always flee due to no target.", fwTimer::GetFrameCount(), __FUNCTION__, rPed.GetDebugName(), &rPed);
#endif
		return true;
	}

	//It has been requested that ambient peds should not attack non-player mission peds, unless they have a gun.
	//This was requested when it was noticed that unarmed ambient peds have a small chance of attacking
	//unarmed non-player mission peds in certain situations (escort boss, etc).
	bool bLawHeliDriver = NetworkInterface::IsGameInProgress() && rPed.IsLawEnforcementPed() && rPed.GetIsDrivingVehicle() && rPed.GetVehiclePedInside()->InheritsFromHeli();
	if(!pTarget->IsPlayer() && pTarget->PopTypeIsMission() && rPed.PopTypeIsRandom() && !bLawHeliDriver && !IsCarryingGun(rPed))
	{
#if AI_DEBUG_OUTPUT_ENABLED
		taskDebugf2("Frame %d : %s : ped %s [0x%p] : Will always flee due to being ambient with a mission target [%s], CurrentEvent [%s]", fwTimer::GetFrameCount(), __FUNCTION__, rPed.GetDebugName(), &rPed, AILogging::GetEntityDescription(pTarget).c_str(), szEventDesc);
#else
		taskDebugf2("Frame %d : %s : ped %s [0x%p] : Will always flee due to being ambient with a mission target.", fwTimer::GetFrameCount(), __FUNCTION__, rPed.GetDebugName(), &rPed);
#endif

		return true;
	}

	//Peds that have a best weapon that is not armed should not attack peds that are armed.
	if(!CanFightArmedPedsWhenNotArmed(rPed, uConfigFlags) && (GetArmamentForPed(rPed, PAS_Best) != PA_Armed) &&
		(GetArmamentForPed(*pTarget) == PA_Armed))
	{
#if AI_DEBUG_OUTPUT_ENABLED
		taskDebugf2("Frame %d : %s : ped %s [0x%p] : Will always flee due to not being armed and having an armed target [%s], CurrentEvent [%s]", fwTimer::GetFrameCount(), __FUNCTION__, rPed.GetDebugName(), &rPed, AILogging::GetEntityDescription(pTarget).c_str(), szEventDesc);
#else
		taskDebugf2("Frame %d : %s : ped %s [0x%p] : Will always flee due to not being armed and having an armed target.", fwTimer::GetFrameCount(), __FUNCTION__, rPed.GetDebugName(), &rPed);
#endif

		return true;
	}

	//Check if the ped should flee from a wanted target.
	if(ShouldFleeFromWantedTarget(rPed, uRunningFlags))
	{
#if AI_DEBUG_OUTPUT_ENABLED
		taskDebugf2("Frame %d : %s : ped %s [0x%p] : Will always flee due to a wanted target [%s], CurrentEvent [%s]", fwTimer::GetFrameCount(), __FUNCTION__, rPed.GetDebugName(), &rPed, AILogging::GetEntityDescription(pTarget).c_str(), szEventDesc);
#else
		taskDebugf2("Frame %d : %s : ped %s [0x%p] : Will always flee due to a wanted target.", fwTimer::GetFrameCount(), __FUNCTION__, rPed.GetDebugName(), &rPed);
#endif

		return true;
	}

	// Check some vehicle conditions that should always cause us to flee
	if(ShouldFleeDueToVehicle(rPed))
	{
		taskDebugf2("Frame %d : %s : ped %s [0x%p] : Will always flee due to their vehicle.", fwTimer::GetFrameCount(), __FUNCTION__, rPed.GetDebugName(), &rPed);

		return true;
	}

	//Check if the ped is friendly with the target.
	if(!uConfigFlags.IsFlagSet(CF_SetFriendlyException) &&
		rPed.GetPedIntelligence()->IsFriendlyWith(*pTarget))
	{
#if AI_DEBUG_OUTPUT_ENABLED
		taskDebugf2("Frame %d : %s : ped %s [0x%p] : Will always flee due to being friendly with the target [%s], CurrentEvent [%s]", fwTimer::GetFrameCount(), __FUNCTION__, rPed.GetDebugName(), &rPed, AILogging::GetEntityDescription(pTarget).c_str(), szEventDesc);
#else
		taskDebugf2("Frame %d : %s : ped %s [0x%p] : Will always flee due to being friendly with the target.", fwTimer::GetFrameCount(), __FUNCTION__, rPed.GetDebugName(), &rPed);
#endif
		return true;
	}

	//Check if we should flee the ped unless armed (for things like dangerous animals).
	if(!rPed.PopTypeIsMission() && GetArmamentForPed(rPed, PAS_Best) != PA_Armed && ShouldFleeFromPedUnlessArmed(*pTarget))
	{
#if AI_DEBUG_OUTPUT_ENABLED
		taskDebugf2("Frame %d : %s : ped %s [0x%p] : Will always flee due to not being armed and having a target that should be fled from. Target [%s], CurrentEvent [%s]", fwTimer::GetFrameCount(), __FUNCTION__, rPed.GetDebugName(), &rPed, AILogging::GetEntityDescription(pTarget).c_str(), szEventDesc);
#else
		taskDebugf2("Frame %d : %s : ped %s [0x%p] : Will always flee due to not being armed and having a target that should be fled from.", fwTimer::GetFrameCount(), __FUNCTION__, rPed.GetDebugName(), &rPed);
#endif
		return true;
	}

	//Check if we should flee unless armed due to config flags.
	if (GetArmamentForPed(rPed, PAS_Best) != PA_Armed && uConfigFlags.IsFlagSet(CF_ForceFleeIfNotArmed))
	{
		taskDebugf2("Frame %d : %s : ped %s [0x%p] : Will always flee due to not being armed and having CF_ForceFleeIfNotArmed set.", fwTimer::GetFrameCount(), __FUNCTION__, rPed.GetDebugName(), &rPed);

		return true;
	}

	//Check if our vehicle is told to flee from combat
	CVehicle* pMyVehicle = rPed.GetVehiclePedInside();
	if(pMyVehicle)
	{
		if(pMyVehicle->FleesFromCombat())
		{
			taskDebugf2("Frame %d : %s : ped %s [0x%p] : Will always flee due to their vehicle wanting to flee from combat.", fwTimer::GetFrameCount(), __FUNCTION__, rPed.GetDebugName(), &rPed);

			return true;
		}

		// If our target is in a vehicle and we are driving a vehicle (as well as non-law random ped)
		static dev_u32 s_uMaxVehiclesInCombat = 4;
		if( !uRunningFlags.IsFlagSet(RF_HasMadeInitialDecision) &&
			pTarget->GetIsInVehicle() && rPed.PopTypeIsRandom() && !rPed.ShouldBehaveLikeLaw() &&
			CCombatManager::GetCombatTaskManager()->CountPedsInCombatWithTarget(*pTarget, CCombatTaskManager::OF_MustBeVehicleDriver) >= s_uMaxVehiclesInCombat )
		{
			taskDebugf2("Frame %d : %s : ped %s [0x%p] : Will always flee due to their target being in a vehicle, and having too many vehicles in combat already.", fwTimer::GetFrameCount(), __FUNCTION__, rPed.GetDebugName(), &rPed);

			return true;
		}
	}
	
	// If your dependent can't attack, then you can't either.
	CPed* pAmbientFriend = rPed.GetPedIntelligence()->GetAmbientFriend();
	if (pAmbientFriend && pAmbientFriend->GetTaskData().GetIsFlagSet(CTaskFlags::DependentAmbientFriend))
	{
		if (DetermineThreatResponseState(*pAmbientFriend, pTarget, TR_None, CF_PreferFight, 0) == TR_Flee)
		{
			return true;
		}
	}

	// Player-controlled dangerous animals in MP should always cause a flee reaction to ambient peds, never combat (url:bugstar:5709827)
	// Can't check personality data for 'dangerous animal' here as script fire off EVENT_SHOCKING_DANGEROUS_ANIMAL manually from the peyote script
	const CEvent* pCurrentEvent = rPed.GetPedIntelligence()->GetCurrentEvent();
	if (pCurrentEvent && pCurrentEvent->GetEventType() == EVENT_SHOCKING_DANGEROUS_ANIMAL && NetworkInterface::IsGameInProgress() && !rPed.PopTypeIsMission() && pTarget->IsPlayer() && CPedType::IsAnimalType(pTarget->GetPedType()))
	{
		taskDebugf2("Frame %d : %s : ped %s [0x%p] : Ambient peds in MP will always flee to player-controlled dangerous animals", fwTimer::GetFrameCount(), __FUNCTION__, rPed.GetDebugName(), &rPed);

		return true;
	}

	return false;
}

bool CTaskThreatResponse::ShouldFleeFromIndirectThreats(const CPed& rPed, const CPed* pTarget, fwFlags16 uConfigFlags, fwFlags8 uRunningFlags)
{
	//Ensure the ped is random.
	if(!rPed.PopTypeIsRandom())
	{
		return false;
	}

	//Ensure the flag is not set.
	if(rPed.GetPedIntelligence()->GetFleeBehaviour().GetFleeFlags().IsFlagSet(CFleeBehaviour::BF_DisableFleeFromIndirectThreats))
	{
		return false;
	}

	//Ensure the threat is indirect.
	if(!uConfigFlags.IsFlagSet(CF_ThreatIsIndirect))
	{
		return false;
	}

	//Ensure we have not been directly threatened.
	if(uRunningFlags.IsFlagSet(RF_HasBeenDirectlyThreatened))
	{
		return false;
	}

	//Ensure we don't prefer to fight.
	if(uConfigFlags.IsFlagSet(CF_PreferFight))
	{
		return false;
	}

	//Check if the ped is a civilian.
	if(rPed.IsCivilianPed())
	{
		taskDebugf2("Frame %d : %s : ped %s [0x%p] : Will always flee due to being a civilian and responding to an indirect threat.", fwTimer::GetFrameCount(), __FUNCTION__, rPed.GetDebugName(), &rPed);

		return true;
	}

	//Check if the ped is a medic.
	if(rPed.IsMedicPed())
	{
		taskDebugf2("Frame %d : %s : ped %s [0x%p] : Will always flee due to being a medic and responding to an indirect threat.", fwTimer::GetFrameCount(), __FUNCTION__, rPed.GetDebugName(), &rPed);

		return true;
	}

	//Check if the ped is gang, with a law-enforcement target.
	if(rPed.IsGangPed() && pTarget && pTarget->IsLawEnforcementPed())
	{
		taskDebugf2("Frame %d : %s : ped %s [0x%p] : Will always flee due to being a gang member and responding to an indirect threat from a law enforcement officer.", fwTimer::GetFrameCount(), __FUNCTION__, rPed.GetDebugName(), &rPed);

		return true;
	}

	return false;
}

bool CTaskThreatResponse::ShouldFleeFromWantedTarget(const CPed& rPed, fwFlags8 uRunningFlags)
{
	//Ensure the ped type is random.
	if(!rPed.PopTypeIsRandom())
	{
		return false;
	}

	//Ensure the local player is valid.
	const CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if(!pPlayer)
	{
		return false;
	}

	CWanted* pPlayerWanted = pPlayer->GetPlayerWanted();

	//Ensure the ped is not law enforcement.
	if(rPed.ShouldBehaveLikeLaw() && (!pPlayer->GetPedConfigFlag(CPED_CONFIG_FLAG_LawPedsCanFleeFromNonWantedPlayer) && pPlayerWanted->m_AllRandomsFlee))
	{
		return false;
	}
	
	//Check if all randoms should flee.
	if(pPlayerWanted->m_AllRandomsFlee)
	{
		//Check if this is disabled for the ped.
		if(!rPed.GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_DisableAllRandomsFlee))
		{
			taskDebugf2("Frame %d : %s : ped %s [0x%p] : Will always flee due to all ambient peds being forced to flee.", fwTimer::GetFrameCount(), __FUNCTION__, rPed.GetDebugName(), &rPed);

			return true;
		}
	}

	// Gang members should flee from a 3+ WL star player if this is the initial decision
	if(!uRunningFlags.IsFlagSet(RF_HasMadeInitialDecision) && rPed.IsGangPed() && !rPed.CheckBraveryFlags(BF_DONT_FORCE_FLEE_COMBAT) && pPlayerWanted->GetWantedLevel() >= WANTED_LEVEL3)
	{
		taskDebugf2("Frame %d : %s : ped %s [0x%p] : Will always flee due to being a gang member and the player having 3* or more.", fwTimer::GetFrameCount(), __FUNCTION__, rPed.GetDebugName(), &rPed);

		return true;
	}
	
	//Check if all neutral randoms should flee.
	if(pPlayerWanted->m_AllNeutralRandomsFlee)
	{
		//Check if the ped is neutral.
		const CRelationshipGroup* pRelGroup = rPed.GetPedIntelligence()->GetRelationshipGroup();
		if(pRelGroup && !pRelGroup->CheckRelationship(ACQUAINTANCE_TYPE_PED_HATE, pPlayer->GetPedIntelligence()->GetRelationshipGroupIndex()))
		{
			taskDebugf2("Frame %d : %s : ped %s [0x%p] : Will always flee due to all neutral ambient peds being forced to flee.", fwTimer::GetFrameCount(), __FUNCTION__, rPed.GetDebugName(), &rPed);

			return true;
		}
	}

	// Make sure all unarmed randoms flee if they are supposed to
	if(pPlayerWanted->m_AllRandomsOnlyAttackWithGuns)
	{
		const CPedWeaponManager* pWeaponManager = rPed.GetWeaponManager();
		const CWeaponInfo* pBestWeaponInfo = pWeaponManager ? pWeaponManager->GetBestWeaponInfo() : NULL;
		if(!pBestWeaponInfo || !pBestWeaponInfo->GetIsGun())
		{
			taskDebugf2("Frame %d : %s : ped %s [0x%p] : Will always flee due to all ambient peds being forced to only attack with guns.", fwTimer::GetFrameCount(), __FUNCTION__, rPed.GetDebugName(), &rPed);

			return true;
		}
	}
	
	return false;
}

bool CTaskThreatResponse::ShouldFleeFromPedUnlessArmed(const CPed& rPed)
{
	return rPed.GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_NonMissionPedsFleeFromThisPedUnlessArmed);
}

bool CTaskThreatResponse::ShouldFleeDueToVehicle(const CPed& rPed)
{
	//Ensure the vehicle is valid.
	CVehicle* pVehicle = rPed.GetVehiclePedInside();
	if(!pVehicle)
	{
		return false;
	}

	//Check if we're in a plane, and on the ground.
	if(pVehicle->InheritsFromPlane() && pVehicle->HasContactWheels())
	{
		const CVehicleLayoutInfo* pLayoutInfo = pVehicle->GetLayoutInfo();
		if( rPed.GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanLeaveVehicle) &&
			(!pLayoutInfo || !pLayoutInfo->GetWarpInAndOut()) )
		{
			s32 iPedsSeatIndex = pVehicle->GetSeatManager()->GetPedsSeatIndex(&rPed);
			if(iPedsSeatIndex >= 0)
			{
				VehicleEnterExitFlags vehicleFlags;
				vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::IsExitingVehicle);
				s32 iTargetEntryPoint = CModelSeatInfo::EvaluateSeatAccessFromAllEntryPoints(&rPed, pVehicle, SR_Specific, iPedsSeatIndex, false, vehicleFlags);
				if(iTargetEntryPoint >= 0)
				{
					return false;
				}
			}
		}

		taskDebugf2("Frame %d : %s : ped %s [0x%p] : Will always flee due to being in a grounded plane.", fwTimer::GetFrameCount(), __FUNCTION__, rPed.GetDebugName(), &rPed);

		return true;
	}

	//Check if we're in a blimp.
	if(pVehicle->InheritsFromBlimp())
	{
		taskDebugf2("Frame %d : %s : ped %s [0x%p] : Will always flee due to being in a blimp.", fwTimer::GetFrameCount(), __FUNCTION__, rPed.GetDebugName(), &rPed);

		return true;
	}

	return false;
}

bool CTaskThreatResponse::ShouldNeverFight(fwFlags16 uConfigFlags)
{
	//Check if the flag is set.
	if(uConfigFlags.IsFlagSet(CF_NeverFight))
	{
		return true;
	}
	
	return false;
}

bool CTaskThreatResponse::ShouldNeverFlee(const CPed& rPed, fwFlags16 uConfigFlags)
{
	//Check if the flag is set.
	if(uConfigFlags.IsFlagSet(CF_NeverFlee))
	{
		return true;
	}
	
	//Check if the ped is carrying a gun.
	if(IsCarryingGun(rPed))
	{
		return true;
	}
	
	//Check if the flee behavior should never flee.
	if(rPed.GetPedIntelligence()->GetFleeBehaviour().GetFleeFlags().IsFlagSet(CFleeBehaviour::BF_NeverFlee))
	{
		return true;
	}

	//Check if we are brave in a group, and we have at least two nearby friends.
	if(rPed.CheckBraveryFlags(BF_BOOST_BRAVERY_IN_GROUP) && (rPed.GetPedIntelligence()->CountFriends(10.0f, true) >= 2))
	{
		return true;
	}
	
	return false;
}

void CTaskThreatResponse::SetFriendlyException()
{
	//Set the friendly exception.
	GetPed()->GetPedIntelligence()->SetFriendlyException(m_pTarget);
}

bool CTaskThreatResponse::ShouldPedBroadcastRespondedToThreatEvent(const CPed& rPed)
{
	if (rPed.GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_DisableRespondedToThreatBroadcast))
	{
		return false;
	}

	CEvent* pCurrentEvent = rPed.GetPedIntelligence()->GetCurrentEvent();

	// Check to see if the amount of damage from the event was small.
	if (pCurrentEvent && pCurrentEvent->GetEventType() == EVENT_DAMAGE)
	{
		CEventDamage* pDamageEvent = static_cast<CEventDamage*>(pCurrentEvent);
		static const float sf_MinDamageForRespondedToThreat = 19.0f;
		if (pDamageEvent->GetDamageApplied() < sf_MinDamageForRespondedToThreat)
		{
			return false;
		}
	}

	return true;
}

void CTaskThreatResponse::BroadcastPedRespondedToThreatEvent(CPed& rPed, const CPed* pTarget)
{
	//Create the event.
	CEventRespondedToThreat event(&rPed, pTarget);

	//Communicate the event.
	CEvent::CommunicateEventInput input;
	input.m_bInformRespectedPedsOnly				= true;
	input.m_bInformRespectedPedsDueToScenarios		= true;
	input.m_bInformRespectedPedsDueToSameVehicle	= true;
	input.m_bUseRadioIfPossible						= false;
	input.m_fMaxRangeWithoutRadio					= rPed.PopTypeIsMission() ? rPed.GetPedIntelligence()->GetMaxInformRespectedFriendDistance() : 30.0f;
	event.CommunicateEvent(&rPed, input);
}

bool CTaskThreatResponse::ShouldFleeFromCombat() const
{
	// Respect the running flag.
	if (m_uRunningFlags.IsFlagSet(RF_FleeFromCombat))
	{
		taskDebugf2("Frame %d : %s : ped %s [0x%p] : Chose to flee from combat due to RF_FleeFromCombat.", fwTimer::GetFrameCount(), __FUNCTION__, GetPed()->GetDebugName(), GetPed());

		return true;
	}

	// Check the task flags.
	if (m_pTarget && m_pTarget->GetIsInVehicle() && GetPed()->GetTaskData().GetIsFlagSet(CTaskFlags::FleeFromCombatWhenTargetIsInVehicle))
	{
		taskDebugf2("Frame %d : %s : ped %s [0x%p] : Chose to flee from combat due to FleeFromCombatWhenTargetIsInVehicle.", fwTimer::GetFrameCount(), __FUNCTION__, GetPed()->GetDebugName(), GetPed());

		return true;
	}

	// Check if this ped flees from invulnerable opponents.
	if (m_pTarget && m_pTarget->m_nPhysicalFlags.bNotDamagedByAnything && GetPed()->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_FleesFromInvincibleOpponents))
	{
		taskDebugf2("Frame %d : %s : ped %s [0x%p] : Chose to flee from combat due to BF_FleesFromInvincibleOpponents.", fwTimer::GetFrameCount(), __FUNCTION__, GetPed()->GetDebugName(), GetPed());

		return true;
	}

	// Ambient - no law enforcement - peds should abandon combat if the target is being attacked by the police
	if (!GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_DontLeaveCombatIfTargetPlayerIsAttackedByPolice) && GetPed()->PopTypeIsRandom() && !GetPed()->IsLawEnforcementPed() && // Ambient non law enforcement ped
		// Target wanted
		m_pTarget && m_pTarget->GetPlayerWanted() && m_pTarget->GetPlayerWanted()->GetWantedLevel() > WANTED_CLEAN &&
		// Target being attacked by cops
		CCombatManager::GetCombatTaskManager()->CountPedsInCombatWithTarget(*m_pTarget, CCombatTaskManager::OF_ReturnAfterInitialValidPed | CCombatTaskManager::OF_MustHaveGunWeaponEquipped | CCombatTaskManager::OF_MustBeLawEnforcement | CCombatTaskManager::OF_MustBeOnFoot))
	{
		taskDebugf2("Frame %d : %s : ped %s [0x%p] : Chose to flee from combat due to target being a wanted player currently attacked by cops.", fwTimer::GetFrameCount(), __FUNCTION__, GetPed()->GetDebugName(), GetPed());

		return true;
	}

	// Stay in combat.
	return false;
}

bool CTaskThreatResponse::CheckIfEstablishedDecoy()
{
	//If enough time has elapsed without switching to the decoy ped, just flee.
	if (NetworkInterface::IsGameInProgress() && m_pTarget->IsPlayer() && !m_pTarget->GetPedConfigFlag(CPED_CONFIG_FLAG_IsDecoyPed) && m_pTarget->GetPedConfigFlag(CPED_CONFIG_FLAG_HasEstablishedDecoy))
	{
		u32 uCurrentTime = fwTimer::GetTimeInMilliseconds();

		if (m_uTimerToFlee == 0)
		{
			m_uTimerToFlee = uCurrentTime;
		}

		if ((uCurrentTime - m_uTimerToFlee) > 100)	// 0.1 seconds
		{
			return true;
		}
	}
	else
	{
		m_uTimerToFlee = 0;
	}

	return false;
}

bool CTaskThreatResponse::ShouldReturnToCombatFromFlee() const
{
	if (NetworkInterface::IsGameInProgress())
	{
		return false;
	}

	if (!m_uRunningFlags.IsFlagSet(RF_FleeFromCombat))
	{
		return false;
	}

	if (!GetPed()->PopTypeIsRandom())
	{
		return false;
	}

	if (!GetPed()->IsLawEnforcementPed())
	{
		return false;
	}

	if (!m_pTarget || !m_pTarget->IsLocalPlayer())
	{
		return false;
	}

	ScalarV scDistToLocalPlayerSq = DistSquared(GetPed()->GetTransform().GetPosition(), m_pTarget->GetTransform().GetPosition());
	ScalarV scMaxDistToContinueFleeSq = LoadScalar32IntoScalarV(square(CTaskCombat::ms_Tunables.m_MaxDistanceForLawToReturnToCombatFromFlee));
	if(IsGreaterThanAll(scDistToLocalPlayerSq, scMaxDistToContinueFleeSq))
	{
		return false;
	}

	return true;
}

bool CTaskThreatResponse::ShouldMakeNewDecisionDueToCache(CPed* pPed) const
{
	//Build the cache.
	DecisionCache decisionCache;
	BuildDecisionCache(decisionCache, pPed);

	//Check if any key states changed.
	if(decisionCache.m_Ped.m_bIsCarryingGun != m_DecisionCache.m_Ped.m_bIsCarryingGun)
	{
		return true;
	}
	else if(decisionCache.m_Ped.m_bAlwaysFlee != m_DecisionCache.m_Ped.m_bAlwaysFlee)
	{
		return true;
	}
	else if(decisionCache.m_Target.m_bAllRandomsFlee != m_DecisionCache.m_Target.m_bAllRandomsFlee)
	{
		return true;
	}
	else if(decisionCache.m_Target.m_bAllNeutralRandomsFlee != m_DecisionCache.m_Target.m_bAllNeutralRandomsFlee)
	{
		return true;
	}
	else if(decisionCache.m_Target.m_bAllRandomsOnlyAttackWithGuns != m_DecisionCache.m_Target.m_bAllRandomsOnlyAttackWithGuns)
	{
		return true;
	}

	//Check if the armament has gotten more scary.
	if(decisionCache.m_Target.m_nArmament > m_DecisionCache.m_Target.m_nArmament)
	{
		return true;
	}

	return false;
}

bool CTaskThreatResponse::CanSetTargetWantedLevel(const CWanted* pTargetWanted, const CPed* pPed, const CPed* pTargetPed, bool bCheckLosStatus) const
{
	// Must have a wanted
	if(!pTargetWanted)
	{
		return false;
	}

	//Ensure the wanted multiplier is valid.
	if(pTargetWanted->m_fMultiplier <= 0.0f)
	{
		return false;
	}

	//This ped has to be fighting to bother upping the WL
	if(m_threatResponse != State_Combat)
	{
		return false;
	}

	//Ensure the ped is valid.
	if(!pPed)
	{
		return false;
	}
	
	//Ensure the ped is law enforcement.
	if(!pPed->ShouldBehaveLikeLaw())
	{
		return false;
	}

	//Ensure the ped can influence the wanted level.
	if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DontInfluenceWantedLevel))
	{
		return false;
	}
	
	//Ensure the target is valid.
	if(!pTargetPed)
	{
		return false;
	}
	
	//Ensure the target is a player.
	if(!pTargetPed->IsPlayer())
	{
		return false;
	}

	// Check the LOS status if we need to
	if(bCheckLosStatus)
	{
		// Verify we have LOS to our target otherwise return
		CPedTargetting* pPedTargeting = pPed->GetPedIntelligence()->GetTargetting(false);
		if(!pPedTargeting || pPedTargeting->GetLosStatus(m_pTarget, false) != Los_clear)
		{
			return false;
		}
	}

	return true;
}

void CTaskThreatResponse::SetTargetWantedLevelDueToEvent(const CEvent& rEvent)
{
	//Ensure the wanted structure is valid.
	const CPed* pTargetPed = rEvent.GetSourcePed();
	CWanted* pWanted = pTargetPed ? pTargetPed->GetPlayerWanted() : NULL;
	if(CanSetTargetWantedLevel(pWanted, GetPed(), pTargetPed, GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_OnlyUpdateTargetWantedIfSeen)))
	{
		// Debug info for setting the wanted level
		wantedDebugf3("CTaskThreatResponse::SetTargetWantedLevelDueToEvent pPed[%p] pTargetPed[%p] Event = %s", GetPed(), pTargetPed, rEvent.GetName().c_str());
		AI_LOG_WITH_ARGS("CTaskThreatResponse::SetTargetWantedLevelDueToEvent Event = %s Ped = %p, Target Ped = %s ped %s (%p) \n", rEvent.GetName().c_str(), GetPed(), AILogging::GetDynamicEntityIsCloneStringSafe(pTargetPed), AILogging::GetDynamicEntityNameSafe(pTargetPed), pTargetPed);

		pWanted->SetWantedLevelNoDrop(VEC3V_TO_VECTOR3(pTargetPed->GetTransform().GetPosition()), WANTED_LEVEL2, 0, WL_FROM_TASK_THREAT_RESPONSE_DUE_TO_EVENT);

		m_uRunningFlags.SetFlag(RF_HasWantedLevelBeenSet);
	}
}

void CTaskThreatResponse::SetTargetWantedLevel(CPed* pPed)
{
	wantedDebugf3("CTaskThreatResponse::SetTargetWantedLevel pPed[%p]",pPed);

	//Ensure the wanted structure is valid.
	CWanted* pWanted = m_pTarget ? m_pTarget->GetPlayerWanted() : NULL;

	// Should check LoS status if ped is flagged to do so, or if threatened ped broadcasting the event is also flagged to do so.
	bool bCheckLosStatus = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_OnlyUpdateTargetWantedIfSeen);
	if (!bCheckLosStatus)
	{
		CEvent* pEvent = pPed->GetPedIntelligence()->GetCurrentEvent();
		if (pEvent && pEvent->GetEventType() == EVENT_RESPONDED_TO_THREAT)
		{
			CEventRespondedToThreat *pEventRespondedToThreat = static_cast<CEventRespondedToThreat*>(pEvent);
			bCheckLosStatus = pEventRespondedToThreat->GetPedBeingThreatened() && pEventRespondedToThreat->GetPedBeingThreatened()->GetPedConfigFlag(CPED_CONFIG_FLAG_OnlyUpdateTargetWantedIfSeen);
		}
	}
	
	if(CanSetTargetWantedLevel(pWanted, pPed, m_pTarget, bCheckLosStatus))
	{
#if __BANK
		// Debug to the log file saying what event we are reacting to
		CEvent* pEvent = pPed->GetPedIntelligence()->GetCurrentEvent();
		if(pEvent)
		{
			AI_LOG_WITH_ARGS("CTaskThreatResponse::SetTargetWantedLevel Event = %s Ped = %p, Target Ped = %s ped %s (%p) \n", pEvent->GetName().c_str(), pPed, AILogging::GetDynamicEntityIsCloneStringSafe(m_pTarget), AILogging::GetDynamicEntityNameSafe(m_pTarget), m_pTarget.Get());
		}
#endif

		//Check if we are going to arrest the target.
		bool bArrestTarget = m_uConfigFlagsForCombat.IsFlagSet(CTaskCombat::ComF_ArrestTarget);
		
		//Set the wanted level.
		s32 iWantedLevel = (bArrestTarget || (pPed && pPed->IsSecurityPed()) || NetworkInterface::IsGameInProgress()) ? WANTED_LEVEL1 : WANTED_LEVEL2;
		pWanted->SetWantedLevelNoDrop(VEC3V_TO_VECTOR3(m_pTarget->GetTransform().GetPosition()), iWantedLevel, 0, WL_FROM_TASK_THREAT_RESPONSE_INITIAL_DECISION);

		m_uRunningFlags.SetFlag(RF_HasWantedLevelBeenSet);
	}
}

void CTaskThreatResponse::SetWantedLevelForPed(CPed* pPed, const CEvent& rEvent)
{
	//Ensure the wanted structure is valid.
	const CPed* pTargetPed = rEvent.GetSourcePed();
	CWanted* pWanted = pTargetPed ? pTargetPed->GetPlayerWanted() : NULL;
	// Should check LoS status if ped is flagged to do so, or if threatened ped broadcasting the event is also flagged to do so.
	const CEventRespondedToThreat *pEventRespondedToThreat = (rEvent.GetEventType() == EVENT_RESPONDED_TO_THREAT) ? static_cast<const CEventRespondedToThreat*>(&rEvent): NULL;
	bool bCheckLosStatus = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_OnlyUpdateTargetWantedIfSeen) || (pEventRespondedToThreat && pEventRespondedToThreat->GetPedBeingThreatened() && pEventRespondedToThreat->GetPedBeingThreatened()->GetPedConfigFlag(CPED_CONFIG_FLAG_OnlyUpdateTargetWantedIfSeen));
	if(CanSetTargetWantedLevel(pWanted, pPed, pTargetPed, bCheckLosStatus))
	{
		// Debug info for setting the wanted level
		wantedDebugf3("CTaskThreatResponse::SetWantedLevelForPed pPed[%p] pTargetPed[%p] Event = %s", pPed, pTargetPed, rEvent.GetName().c_str());
		AI_LOG_WITH_ARGS("CTaskThreatResponse::SetWantedLevelForPed Event = %s Ped = %p, Target Ped = %s ped %s (%p) \n", rEvent.GetName().c_str(), pPed, AILogging::GetDynamicEntityIsCloneStringSafe(pTargetPed), AILogging::GetDynamicEntityNameSafe(pTargetPed), pTargetPed);

		//Set the wanted level.
		s32 iWantedLevel = (pPed->IsSecurityPed() || NetworkInterface::IsGameInProgress()) ? WANTED_LEVEL1 : WANTED_LEVEL2;
		pWanted->SetWantedLevelNoDrop(VEC3V_TO_VECTOR3(pTargetPed->GetTransform().GetPosition()), iWantedLevel, 0, WL_FROM_TASK_THREAT_RESPONSE_DUE_TO_EVENT);
	}
}

void CTaskThreatResponse::PossiblyIncreaseTargetWantedLevel(const CEvent& rEvent)
{
	if(GetState() == State_Combat)
	{
		if(rEvent.GetSourcePed() == m_pTarget)
		{
			CTaskArrestPed* pArrestTask = static_cast<CTaskArrestPed*>(FindSubTaskOfType(CTaskTypes::TASK_ARREST_PED));
			if(pArrestTask)
			{
				pArrestTask->SetTargetResistingArrest();
			}
			else
			{
				SetTargetWantedLevelDueToEvent(rEvent);
			}
		}
		else
		{
			SetWantedLevelForPed(GetPed(), rEvent);
		}
	}
}

void CTaskThreatResponse::UpdateTargetWantedLevel(CPed* pPed)
{
	// If the continuous wanted level update flag is set then ignore the wanted level has been set flag and the only update target wanted if seen (this logic is included in the continuous update)
	if(!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_AllowContinuousThreatResponseWantedLevelUpdates))
	{
		// Only do this if we haven't set the wanted level already
		if(m_uRunningFlags.IsFlagSet(RF_HasWantedLevelBeenSet))
		{
			return;
		}

		// Right now this function is handling the update if we need to be checking for LOS to the target before setting the WL
		if(!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_OnlyUpdateTargetWantedIfSeen))
		{
			return;
		}
	}

	// Must have a valid target as we'll be using it to get the wanted and LOS status
	if(!m_pTarget)
	{
		return;
	}

	// Target needs to have a wanted
	CWanted* pWanted = m_pTarget->GetPlayerWanted();
	if(!pWanted)
	{
		return;
	}

	// Find out what wanted level we want to set
	s32 iWantedLevel = (m_uConfigFlagsForCombat.IsFlagSet(CTaskCombat::ComF_ArrestTarget) || (pPed && pPed->IsSecurityPed()) || NetworkInterface::IsGameInProgress()) ? WANTED_LEVEL1 : WANTED_LEVEL2;

	// If we're already at that wanted level then we're good
	if(pWanted->GetWantedLevel() >= iWantedLevel)
	{
		m_uRunningFlags.SetFlag(RF_HasWantedLevelBeenSet);
		return;
	}

	// Do the normal checks for seeing if we can set a target's wanted level
	if(CanSetTargetWantedLevel(pWanted, pPed, m_pTarget, true))
	{
#if __BANK
		// Debug to the log file saying what event we are reacting to
		CEvent* pEvent = pPed->GetPedIntelligence()->GetCurrentEvent();
		if(pEvent)
		{
			AI_LOG_WITH_ARGS("CTaskThreatResponse::UpdateTargetWantedLevel Event = %s Ped = %p, Target Ped = %s ped %s (%p) \n", pEvent->GetName().c_str(), pPed, AILogging::GetDynamicEntityIsCloneStringSafe(m_pTarget), AILogging::GetDynamicEntityNameSafe(m_pTarget), m_pTarget.Get());
		}
#endif

		// Set the wanted level
		pWanted->SetWantedLevelNoDrop(VEC3V_TO_VECTOR3(m_pTarget->GetTransform().GetPosition()), iWantedLevel, 0, WL_FROM_TASK_THREAT_RESPONSE_INITIAL_DECISION);

		m_uRunningFlags.SetFlag(RF_HasWantedLevelBeenSet);
	}
	else
	{
		// We are not allowed to set the wanted level of this target so set this flag to prevent further checks
		m_uRunningFlags.SetFlag(RF_HasWantedLevelBeenSet);
	}
}

bool CTaskThreatResponse::IsEventAnIndirectThreat(const CPed* pPed, const CEvent& rEvent)
{
	//Check the event type.
	switch(rEvent.GetEventType())
	{
		case EVENT_RESPONDED_TO_THREAT:
		{
			//Check if the ped being threatened is valid.
			const CPed* pPedBeingThreatened = static_cast<const CEventRespondedToThreat&>(rEvent).GetPedBeingThreatened();
			if(pPedBeingThreatened)
			{
				//Check if the task is valid.
				const CTaskThreatResponse* pTaskThreatResponse = static_cast<CTaskThreatResponse *>(
					pPedBeingThreatened->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_THREAT_RESPONSE));
				if(pTaskThreatResponse && pTaskThreatResponse->GetConfigFlags().IsFlagSet(CF_ThreatIsIndirect))
				{
					return true;
				}
			}

			return false;
		}
		case EVENT_SHOT_FIRED:
		case EVENT_SHOCKING_FIRE:
		case EVENT_SHOCKING_GUN_FIGHT:
		case EVENT_SHOCKING_GUNSHOT_FIRED:
		case EVENT_SHOCKING_SEEN_WEAPON_THREAT:
		{
			return true;
		}
		case EVENT_SHOCKING_PED_RUN_OVER:
		case EVENT_SHOCKING_SEEN_PED_KILLED:
		{
			//Ensure we are not friendly with the target.  If we are, consider the threat direct.
			const CPed* pTarget = rEvent.GetTargetPed();
			if(pPed && pTarget && pPed->GetPedIntelligence()->IsFriendlyWithByAnyMeans(*pTarget))
			{
				return false;
			}

			return true;
		}
		case EVENT_SHOCKING_SEEN_MELEE_ACTION:
		{
			//Check if the target is valid.
			const CPed * pTarget = rEvent.GetTargetPed();
			if(pTarget)
			{
				//Check if we are the target.
				if(pPed == pTarget)
				{
					return false;
				}
			}

			return true;
		}
		default:
		{
			return false;
		}
	}
}

bool CTaskThreatResponse::ShouldDisableReactBeforeFlee(const CPed* pPed, const CPed* pTarget)
{
	//Ensure the ped is valid.
	if(!pPed)
	{
		return false;
	}

	//Ensure the target is valid.
	if(!pTarget)
	{
		return false;
	}

	//Check if the ped is running.
	CTaskMotionBase* pMotionTask = pPed->GetCurrentMotionTask();
	if(pMotionTask && (pMotionTask->IsInMotionState(CPedMotionStates::MotionState_Run) || pMotionTask->IsInMotionState(CPedMotionStates::MotionState_Sprint)))
	{
		//Check if the ped is not running towards the target.
		Vec3V vPedForward = pPed->GetTransform().GetForward();
		Vec3V vPedVelocity = VECTOR3_TO_VEC3V(pPed->GetVelocity());
		Vec3V vPedDirection = NormalizeFastSafe(vPedVelocity, vPedForward);
		Vec3V vPedToTarget = Subtract(pTarget->GetTransform().GetPosition(), pPed->GetTransform().GetPosition());
		Vec3V vPedToTargetDirection = NormalizeFastSafe(vPedToTarget, Vec3V(V_ZERO));
		ScalarV scDot = Dot(vPedDirection, vPedToTargetDirection);
		static dev_float s_fMaxDot = 0.707f;
		ScalarV scMaxDot = ScalarVFromF32(s_fMaxDot);
		if(IsLessThanAll(scDot, scMaxDot))
		{
			return true;
		}
	}

	return false;
}

bool CTaskThreatResponse::ShouldFleeInVehicleAfterJack(const CPed* pPed, const CEvent& rEvent)
{
#if __BANK
	TUNE_GROUP_BOOL(TASK_THREAT_RESPONSE, bForceFleeInVehicleAfterJack, false);
	if(bForceFleeInVehicleAfterJack)
	{
		return true;
	}
#endif

	//Ensure the ped is valid.
	if(!pPed)
	{
		return false;
	}

	//Ensure the ped is random.
	if(!pPed->PopTypeIsRandom())
	{
		return false;
	}

	//Ensure the ped is a civilian.
	if(!pPed->IsCivilianPed())
	{
		return false;
	}

	//Ensure we are driving.
	if(!pPed->GetIsDrivingVehicle())
	{
		return false;
	}

	//Ensure we have no friends around.
	static dev_float s_fMaxDistanceForFriends = 15.0f;
	static dev_u32 s_uMaxFriends = 0;
	if(pPed->GetPedIntelligence()->CountFriends(s_fMaxDistanceForFriends, false, true) > s_uMaxFriends)
	{
		return false;
	}

	//Check the event type.
	switch(rEvent.GetEventType())
	{
		case EVENT_DRAGGED_OUT_CAR:
		case EVENT_PED_JACKING_MY_VEHICLE:
		case EVENT_PED_ENTERED_MY_VEHICLE:
		{
			//Check the chances.
			static dev_float s_fChances = 0.5f;
			if(fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < s_fChances)
			{
				return true;
			}
			else
			{
				return false;
			}
		}
		default:
		{
			return false;
		}
	}
}

bool CTaskThreatResponse::ShouldPreferFight(const CPed* pPed, const CEvent& rEvent)
{
	//Ensure the ped is valid.
	if(!pPed)
	{
		return false;
	}

	//Ensure the ped is random.
	if(!pPed->PopTypeIsRandom())
	{
		return false;
	}

	//Check if we are willing to defend females.
	if(pPed->GetSpeechAudioEntity() &&
		pPed->GetSpeechAudioEntity()->DoesContextExistForThisPed("HIT_WOMAN"))
	{
		//Check if the target is female.
		const CPed* pTargetPed = rEvent.GetTargetPed();
		if(pTargetPed && !pTargetPed->IsMale())
		{
			return true;
		}
	}

	// Looks weird to turn and flee from seeing your being car stolen if you have already decided to go into combat once because of someone messing with your car.
	if (rEvent.GetEventType() == EVENT_SHOCKING_SEEN_CAR_STOLEN && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_WentIntoCombatAfterBeingJacked))
	{
		return true;
	}

	return false;
}

bool CTaskThreatResponse::ShouldSetFriendlyException(const CPed* pPed, const CPed* pTarget, const CEvent& rEvent)
{
	//Ensure we are not in MP.
	if(NetworkInterface::IsGameInProgress())
	{
		return false;
	}

	//Ensure the ped is valid.
	if(!pPed)
	{
		return false;
	}

	//Ensure the ped is random.
	if(!pPed->PopTypeIsRandom())
	{
		return false;
	}

	//Check the event type.
	switch(rEvent.GetEventType())
	{
		case EVENT_DAMAGE:
		case EVENT_DRAGGED_OUT_CAR:
		case EVENT_PED_ENTERED_MY_VEHICLE:
		case EVENT_SHOCKING_SEEN_MELEE_ACTION:
		case EVENT_VEHICLE_DAMAGE_WEAPON:
		{
			//Check if the ped is friendly with the target.
			if(pPed && pTarget && pPed->GetPedIntelligence()->IsFriendlyWith(*pTarget))
			{
				return true;
			}

			break;
		}
		case EVENT_FRIENDLY_FIRE_NEAR_MISS:
		{
			return true;
		}
		default:
		{
			break;
		}
	}

	return false;
}

bool CTaskThreatResponse::ShouldSetForceFleeIfUnarmed(const CPed* pPed, const CEvent& rEvent)
{
	//Ensure the ped is valid.
	if(!pPed)
	{
		return false;
	}

	//Ensure the ped is random.
	if(!pPed->PopTypeIsRandom())
	{
		return false;
	}

	//Ensure the ped is not law enforcement.
	if(pPed->IsLawEnforcementPed())
	{
		return false;
	}

	s32 iEventTypeToCheck = rEvent.GetEventType();

	//If responding to a threat, look at the event being responded to by the original ped.
	if (iEventTypeToCheck == EVENT_RESPONDED_TO_THREAT)
	{
		const CEventRespondedToThreat& rEventRespondedToThreat = static_cast<const CEventRespondedToThreat&>(rEvent);
		const CPed* pPedBeingThreatened = rEventRespondedToThreat.GetPedBeingThreatened();
		if (pPedBeingThreatened)
		{
			iEventTypeToCheck = pPedBeingThreatened->GetPedIntelligence()->GetCurrentEventType();
		}
	}

	//Check the event type.
	switch(iEventTypeToCheck)
	{
		case EVENT_EXPLOSION:
		case EVENT_SHOCKING_EXPLOSION:
		case EVENT_SHOCKING_POTENTIAL_BLAST:
		{
			return true;
		}
		default:
		{
			return false;
		}
	}
}

#if !__NO_OUTPUT
const char* CTaskThreatResponse::ToString(ThreatResponse nThreatResponse)
{
	taskAssert(nThreatResponse > TR_None && nThreatResponse < TR_Max);
	static const char* aNames[] = 
	{
		"Fight",
		"Flee",
	};

	return aNames[nThreatResponse];
}
#endif

CTaskInfo* CTaskThreatResponse::CreateQueriableState() const
{
	return rage_new CClonedThreatResponseInfo(m_pTarget.Get(), m_eIsResponseToEventType);
}

CTaskHandsUp::CTaskHandsUp(const s32 iDuration, CPed* pPedToFace /* = NULL */, const s32 iFacePedDuration /* = -1 */, const u32 flags /*= 0*/)
: m_iDuration(iDuration)
, m_iDictIndex(-1)
, m_pPedToFace(pPedToFace)
, m_iFacePedDuration(iFacePedDuration)
, m_flags(ConfigFlags(flags))
, m_bNetworkMigratingInHandsUpLoop(false)
, m_fNetworkMigrateHandsUpLoopPhase(0.0f)
{
	SetInternalTaskType(CTaskTypes::TASK_HANDS_UP);
}

CTaskInfo *CTaskHandsUp::CreateQueriableState() const
{
	return rage_new CClonedHandsUpInfo(m_iDuration, m_pPedToFace, (s16)m_iFacePedDuration, (bool)(m_flags & Flag_StraightToLoop));
}

void CTaskHandsUp::ReadQueriableState(CClonedFSMTaskInfo *pTaskInfo)
{
	Assert( pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_HANDS_UP);
	CClonedHandsUpInfo *pHandsUpInfo = static_cast<CClonedHandsUpInfo*>(pTaskInfo);

	m_iDuration = pHandsUpInfo->GetDuration();
	m_pPedToFace = pHandsUpInfo->GetWatchedPed();
	m_iFacePedDuration = pHandsUpInfo->GetTimeToWatchPed();
	m_flags	= (ConfigFlags)pHandsUpInfo->GetFlags();

	CTaskFSMClone::ReadQueriableState(pTaskInfo);
}

CTaskFSMClone* CTaskHandsUp::CreateTaskForLocalPed(CPed* pPed)
{
	return  CreateTaskForClonePed(pPed);
}

void CTaskHandsUp::CleanUp()
{
	float fCleanupBlendOutDelta = NORMAL_BLEND_OUT_DELTA;

	if(m_bNetworkMigratingInHandsUpLoop)
	{
		fCleanupBlendOutDelta = MIGRATE_SLOW_BLEND_OUT_DELTA; //keep the hands up 
	}

	StopClip(fCleanupBlendOutDelta);
}

CTaskFSMClone* CTaskHandsUp::CreateTaskForClonePed(CPed* UNUSED_PARAM(pPed))
{
	CTaskHandsUp* pNewTask = rage_new CTaskHandsUp(m_iDuration, m_pPedToFace, m_iFacePedDuration, m_flags);

	if(GetState()==State_HandsUpLoop && GetClipHelper())
	{
		pNewTask->m_fNetworkMigrateHandsUpLoopPhase = GetClipHelper()->GetPhase();
		pNewTask->m_bNetworkMigratingInHandsUpLoop = true; //set new task to go straight to hands up state and set the phase
		m_bNetworkMigratingInHandsUpLoop = true; //set on outgoing task too so cleanup will handle blend
	}

	return pNewTask;
}

CTaskFSMClone *CClonedHandsUpInfo::CreateCloneFSMTask() 
{
	return rage_new CTaskHandsUp(m_duration, GetWatchedPed(), GetTimeToWatchPed(), GetFlags());
}

CTask::FSM_Return CTaskHandsUp::UpdateClonedFSM( const s32 iState, const FSM_Event iEvent )
{
	if(GetStateFromNetwork() == State_Finish)
	{
		return FSM_Quit;
	}

	return UpdateFSM(  iState, iEvent );
}

void CTaskHandsUp::OnCloneTaskNoLongerRunningOnOwner()
{
	SetStateFromNetwork(State_Finish);
}

CTask::FSM_Return CTaskHandsUp::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();	

		FSM_State(State_RaiseHands)
			FSM_OnEnter
				RaiseHands_OnEnter();
			FSM_OnUpdate
				return RaiseHands_OnUpdate();

		FSM_State(State_HandsUpLoop)
			FSM_OnEnter
				HandsUpLoop_OnEnter();
			FSM_OnUpdate
				return HandsUpLoop_OnUpdate();

		FSM_State(State_LowerHands)
			FSM_OnEnter
				LowerHands_OnEnter();
			FSM_OnUpdate
				return LowerHands_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return	CTaskHandsUp::Start_OnUpdate()
{
	const u32 s_iHandsUpDictHash = ATSTRINGHASH("ped", 0x034d90761);
	m_iDictIndex = fwAnimManager::FindSlotFromHashKey(s_iHandsUpDictHash).Get();
	if (!taskVerifyf(m_iDictIndex != -1, "Couldn't find hands up dictionary"))
	{
		return FSM_Quit;
	}

	if( (m_flags & Flag_StraightToLoop) || m_bNetworkMigratingInHandsUpLoop)
	{
		SetState(State_HandsUpLoop);
	}
	else
	{
		SetState(State_RaiseHands);
	}
	return FSM_Continue;
}

CTask::FSM_Return	CTaskHandsUp::RaiseHands_OnEnter()
{
	CPed* pPed = GetPed();
	const u32 s_iHandsUpIntroHash = ATSTRINGHASH("handsup_enter", 0x0ee846292);

	bool isInjured = pPed && (pPed->GetPedIntelligence()->GetTaskSecondaryPartialAnim()!=NULL) && (pPed->GetPedIntelligence()->GetTaskSecondaryPartialAnim()->GetTaskType()==CTaskTypes::TASK_REACH_ARM);
	if (!taskVerifyf(fwAnimManager::GetClipIfExistsByDictIndex(m_iDictIndex, s_iHandsUpIntroHash), "Couldn't find handsup_enter clip"))
	{
		return FSM_Quit;
	}
	StartClipByDictAndHash(GetPed(), m_iDictIndex, s_iHandsUpIntroHash, isInjured ?  0 : APF_UPPERBODYONLY, isInjured ? BONEMASK_ARMS : BONEMASK_UPPERONLY, AP_MEDIUM, NORMAL_BLEND_IN_DELTA, CClipHelper::TerminationType_OnFinish);
	
	if(m_pPedToFace && m_pPedToFace->GetWeaponManager() && m_pPedToFace->GetWeaponManager()->GetIsArmedGun())
	{
		//Say the line.
		atHashString hLine("GUN_BEG",0xCD7CEB56);
		pPed->NewSay(hLine.GetHash());
	}
	
	return FSM_Continue;
}

CTask::FSM_Return	CTaskHandsUp::RaiseHands_OnUpdate()
{
	if (IsClipConsideredFinished() || !GetClipHelper())
	{
		SetState(State_HandsUpLoop);
	}
	return FSM_Continue;
}

CTask::FSM_Return	CTaskHandsUp::HandsUpLoop_OnEnter()
{
	const u32 s_iHandsUpBaseHash = ATSTRINGHASH("handsup_base", 0x0820fe9cb);
	if (!taskVerifyf(fwAnimManager::GetClipIfExistsByDictIndex(m_iDictIndex, s_iHandsUpBaseHash), "Couldn't find handsup_base clip"))
	{
		return FSM_Quit;
	}
	CPed* pPed = GetPed();
	bool isInjured = pPed && (pPed->GetPedIntelligence()->GetTaskSecondaryPartialAnim()!=NULL) && (pPed->GetPedIntelligence()->GetTaskSecondaryPartialAnim()->GetTaskType()==CTaskTypes::TASK_REACH_ARM);
	
	float blendInDelta = INSTANT_BLEND_IN_DELTA;
	if(m_flags & Flag_StraightToLoop)
	{
		blendInDelta = SLOW_BLEND_IN_DELTA;
	}

	StartClipByDictAndHash(GetPed(), m_iDictIndex, s_iHandsUpBaseHash, isInjured ? APF_ISLOOPED : APF_ISLOOPED | APF_UPPERBODYONLY, isInjured ? BONEMASK_ARMS : BONEMASK_UPPERONLY, AP_MEDIUM, blendInDelta, CClipHelper::TerminationType_OnFinish);

	if(m_bNetworkMigratingInHandsUpLoop && GetClipHelper())
	{
		m_bNetworkMigratingInHandsUpLoop = false; //clear this so cleanup doesn't see it
		GetClipHelper()->SetPhase(m_fNetworkMigrateHandsUpLoopPhase);
	}

	if(pPed && !pPed->IsNetworkClone())
	{
		if (m_pPedToFace)
		{
			float fTime;
			if(m_iFacePedDuration < 0)
			{
				fTime = -1.0f;
			}
			else
			{
				fTime = ((float)m_iFacePedDuration)/1000.0f;
			}
			SetNewTask(rage_new CTaskTurnToFaceEntityOrCoord(m_pPedToFace, CTaskMoveAchieveHeading::ms_fHeadingChangeRateFrac, CTaskMoveAchieveHeading::ms_fHeadingTolerance, fTime));
		}
	}

	return FSM_Continue;
}

CTask::FSM_Return	CTaskHandsUp::HandsUpLoop_OnUpdate()
{
	if (m_iDuration > 0)
	{
		m_iDuration -= GetTimeStepInMilliseconds();

		if (m_iDuration <= 0)
		{
			SetState(State_LowerHands);
			return FSM_Continue;
		}
	}

	if (!GetClipHelper())
	{
		SetState(State_LowerHands);
	}
	return FSM_Continue;
}

CTask::FSM_Return	CTaskHandsUp::LowerHands_OnEnter()
{
	const u32 s_iHandsUpExitHash = ATSTRINGHASH("handsup_exit", 0x05fbbb3ce);
	if (!taskVerifyf(fwAnimManager::GetClipIfExistsByDictIndex(m_iDictIndex, s_iHandsUpExitHash), "Couldn't find handsup_exit clip"))
	{
		return FSM_Quit;
	}
	CPed* pPed = GetPed();
	bool isInjured = pPed && (pPed->GetPedIntelligence()->GetTaskSecondaryPartialAnim()!=NULL) && (pPed->GetPedIntelligence()->GetTaskSecondaryPartialAnim()->GetTaskType()==CTaskTypes::TASK_REACH_ARM);
	StartClipByDictAndHash(GetPed(), m_iDictIndex, s_iHandsUpExitHash, isInjured ? 0 : APF_UPPERBODYONLY, isInjured ? BONEMASK_ARMS : BONEMASK_UPPERONLY, AP_MEDIUM, NORMAL_BLEND_IN_DELTA, CClipHelper::TerminationType_OnFinish);
	return FSM_Continue;
}

CTask::FSM_Return	CTaskHandsUp::LowerHands_OnUpdate()
{
	if (!GetClipHelper())
	{
		SetState(State_Finish);
	}
	return FSM_Continue;
}

#if !__FINAL || __FINAL_LOGGING

const char * CTaskHandsUp::GetStaticStateName( s32 iState )
{
	switch (iState)
	{
		case State_Start :			return "State_Start";
		case State_RaiseHands :		return "State_RaiseHands";
		case State_HandsUpLoop :	return "State_HandsUpLoop";
		case State_LowerHands :		return "State_LowerHands";
		case State_Finish :			return "State_Finish";
		default: break;
	}
	return "State_Invalid";
}

#endif	// !__FINAL

CTaskReactToGunAimedAt::CTaskReactToGunAimedAt(CPed* pAggressorPed)
: m_pAggressorPed(pAggressorPed)
, m_iHandsUpTime(0)
{
	SetInternalTaskType(CTaskTypes::TASK_REACT_TO_GUN_AIMED_AT);
}

CTaskReactToGunAimedAt::~CTaskReactToGunAimedAt()
{
}

// State machine function run before the main UpdateFSM
CTask::FSM_Return CTaskReactToGunAimedAt::ProcessPreFSM()
{
	// Abort if the target is invalid
	if (!m_pAggressorPed)
	{
		return FSM_Quit;
	}

	//Grab the ped.
	CPed* pPed = GetPed();

	// Allow certain lod modes
	pPed->GetPedAiLod().ClearBlockedLodFlag(CPedAILod::AL_LodPhysics);
	pPed->GetPedAiLod().ClearBlockedLodFlag(CPedAILod::AL_LodMotionTask);

	return FSM_Continue;
}

// Main state machine update function
CTask::FSM_Return CTaskReactToGunAimedAt::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();	

		FSM_State(State_Pause)
			FSM_OnEnter
				Pause_OnEnter();
			FSM_OnUpdate
				return Pause_OnUpdate();

		FSM_State(State_ExitVehicle)
			FSM_OnEnter
				ExitVehicle_OnEnter();
			FSM_OnUpdate
				return ExitVehicle_OnUpdate();

		FSM_State(State_HandsUp)
			FSM_OnEnter
				HandsUp_OnEnter();
			FSM_OnUpdate
				return HandsUp_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskReactToGunAimedAt::Start_OnUpdate()
{
	SetState(State_Pause);
	return FSM_Continue;
}

void CTaskReactToGunAimedAt::Pause_OnEnter()
{
	int iPauseTime=0;

// 	if(pPed->m_pMyVehicle && pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
// 	{
// 		iPauseTime=fwRandom::GetRandomNumberInRange(1000,2000);
// 	}

	SetNewTask(rage_new CTaskPause(iPauseTime));
}

CTask::FSM_Return CTaskReactToGunAimedAt::Pause_OnUpdate()
{
	CPed* pPed = GetPed();

	if (pPed->GetMyVehicle() && pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
	{
		CCarEnterExit::MakeUndraggedPassengerPedsLeaveCar(*pPed->GetMyVehicle(), pPed, m_pAggressorPed);
		SetState(State_ExitVehicle);
	}
	else
	{
		SetState(State_HandsUp);
	}

	return FSM_Continue;
}

void CTaskReactToGunAimedAt::ExitVehicle_OnEnter()
{
	VehicleEnterExitFlags vehicleFlags;
	vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DontCloseDoor);
	SetNewTask(rage_new CTaskExitVehicle(GetPed()->GetMyVehicle(), vehicleFlags));
}

CTask::FSM_Return CTaskReactToGunAimedAt::ExitVehicle_OnUpdate()
{
	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_HandsUp);
	}
	return FSM_Continue;
}

void CTaskReactToGunAimedAt::HandsUp_OnEnter()
{
	CPed* pPed = GetPed();

	// If we successfully generate audio, keep hands up for longer - looks strange to stand silently stand with hands up.
	if (pPed->NewSay("GUN_COOL", 0, true, true))
	{
		m_iHandsUpTime = fwRandom::GetRandomNumberInRange(3000,5000);
	}
	else
	{
		m_iHandsUpTime = fwRandom::GetRandomNumberInRange(1500,3000);
	}

	SetNewTask(rage_new CTaskHandsUp(6000));
}

CTask::FSM_Return CTaskReactToGunAimedAt::HandsUp_OnUpdate()
{
	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Finish);
		return FSM_Continue;
	}

	CPed* pPed = GetPed();
	Vector3 vDir = VEC3V_TO_VECTOR3(m_pAggressorPed->GetTransform().GetPosition() - pPed->GetTransform().GetPosition());
	float fDesiredHeading=fwAngle::GetRadianAngleBetweenPoints(vDir.x,vDir.y,0.0f,0.0f);
	fDesiredHeading=fwAngle::LimitRadianAngle(fDesiredHeading);
	pPed->SetDesiredHeading(fDesiredHeading);
	pPed->GetIkManager().LookAt(0, m_pAggressorPed, 100, BONETAG_HEAD, NULL );

	// Abort if the aggressor gets too close
	if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_HANDS_UP )
	{
		bool bTimeUp = false;
		if( m_iHandsUpTime > 0 )
		{
			m_iHandsUpTime -= GetTimeStepInMilliseconds();
			if( m_iHandsUpTime <= 0 )
			{
				bTimeUp = true;
				m_iHandsUpTime = 1;
			}
		}
		if( bTimeUp || (vDir.Mag2() < rage::square(2.0f)) )
		{
			const u32 hashCool = ATSTRINGHASH("GUN_COOL", 0x04649d336);
			const bool bStillSayingContext = pPed->GetSpeechAudioEntity() && pPed->GetSpeechAudioEntity()->IsAmbientSpeechPlaying() && pPed->GetSpeechAudioEntity()->GetCurrentlyPlayingAmbientSpeechContextHash()==hashCool;
			if (!bStillSayingContext && GetSubTask()->MakeAbortable( CTask::ABORT_PRIORITY_URGENT, NULL ) )
			{
				SetState(State_Finish);
			}
		}
	}

	return FSM_Continue;
}

#if !__FINAL || __FINAL_LOGGING
const char * CTaskReactToGunAimedAt::GetStaticStateName( s32 iState )
{
	taskAssert(iState >= State_Start && iState <= State_Finish);

	switch (iState)
	{
		case State_Start:				return "State_Start";
		case State_Pause:				return "State_Pause";
		case State_ExitVehicle:			return "State_ExitVehicle";
		case State_HandsUp:				return "State_HandsUp";
		case State_Finish:				return "State_Finish";
		default: taskAssert(0); 	
	}

	return "State_Invalid";
}
#endif // !__FINAL
