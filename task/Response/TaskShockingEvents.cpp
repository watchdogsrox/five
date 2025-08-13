// Class header
#include "TaskShockingEvents.h"

// RAGE headers
#include "fwanimation/animhelpers.h"
#include "math/angmath.h"

// Game headers
#include "animation/Move.h"
#include "animation/MovePed.h"
#include "AI/AnimBlackboard.h"
#include "AI/Ambient/ConditionalAnimManager.h"
#include "AI/Ambient/AmbientModelSetManager.h"
#include "AI/stats.h"
#include "Event/Events.h"
#include "event/EventShocking.h"
#include "Event/System/EventData.h"
#include "Network/NetworkInterface.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PopZones.h"
#include "task/Combat/TaskThreatResponse.h"
#include "Task/Default/TaskAmbient.h"
#include "Task/Default/TaskUnalerted.h"
#include "Task/General/TaskBasic.h"
#include "task/General/Phone/TaskCallPolice.h"
#include "Task/Response/TaskFlee.h"
#include "Task/Motion/TaskMotionBase.h"
#include "Task/Movement/TaskCollisionResponse.h"
#include "Task/Movement/TaskGoTo.h"
#include "Task/Movement/TaskIdle.h"
#include "Task/Movement/TaskNavMesh.h"
#include "Task/Response/TaskDuckAndCover.h"
#include "Task/Scenario/Info/ScenarioInfoManager.h"
#include "Task/Scenario/Types/TaskUseScenario.h"
#include "Task/System/TaskManager.h"
#include "Task/system/TaskTypes.h"
#include "Task/Vehicle/TaskCar.h"
#include "Task/Vehicle/TaskExitVehicle.h"
#include "Task/Vehicle/TaskVehicleBase.h"
#include "VehicleAi/Task/TaskVehicleMissionBase.h"
#include "VehicleAi/Task/TaskVehiclePark.h"
#include "VehicleAi/Task/TaskVehicleTempAction.h"
#include "vehicleAi/VehicleIntelligence.h"

using namespace AIStats;

// Externs ----------------------------------------------------------------
extern const u32 g_PainVoice;

AI_OPTIMISATIONS()

// For reference, this is the old response task for fleeing from a shocking event.
// We currently just use CTaskThreatResponse/CTaskSmartFlee for this, but there may
// still be parts of this task that we want to adopt (such as the mobile phone stuff).
#if 0

//-------------------------------------------------------------------------
// CTaskShockingEventFlee
//-------------------------------------------------------------------------

// Statics ----------------------------------------------------------------
u32 CTaskShockingEventFlee::ms_uiTimeTillCanDoNextGunShotMobileChat = 0;

CTaskShockingEventFlee::CTaskShockingEventFlee(const CShockingEvent& event, bool bKeepMovingWhilstWaitingForFleePath)
: CTaskShockingEvent(event)
, m_bKeepMovingWhilstWaitingForFleePath(bKeepMovingWhilstWaitingForFleePath)
{
	SetInternalTaskType(CTaskTypes::TASK_SHOCKING_EVENT_FLEE);
}

CTaskShockingEventFlee::FSM_Return CTaskShockingEventFlee::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		FSM_State(State_Init)
			FSM_OnUpdate
				return StateInitOnUpdate(pPed);
		FSM_State(State_FleeFromUnfriendly)
			FSM_OnEnter
				return StateFleeFromUnfriendlyOnEnter(pPed);
			FSM_OnUpdate
				if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
				{
					return FSM_Quit;
				}
		FSM_State(State_FleeFromFriendly)
			FSM_OnEnter
				return StateFleeFromFriendlyOnEnter(pPed);
			FSM_OnUpdate
				if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
				{
					return FSM_Quit;
				}
		FSM_State(State_LeaveCarAndFlee)
			FSM_OnEnter
				return StateLeaveCarAndFleeOnEnter(pPed);
			FSM_OnUpdate
				if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
				{
					return FSM_Quit;
				}
		FSM_State(State_Quit)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

#if !__FINAL
const char * CTaskShockingEventFlee::GetStaticStateName( s32 iState )
{
	static char* aStateNames[] = 
	{
		"Init",
		"FleeFromUnfriendly",
		"FleeFromFriendly",
		"LeaveCarAndFlee",
		"MakeMobileCall",
		"Quit",
	};

	return GetCombinedDebugStateName(aStateNames[iState]);
}
#endif // !__FINAL

CTaskShockingEventFlee::FSM_Return CTaskShockingEventFlee::StateInitOnUpdate(CPed* pPed)
{
	CEntity* pEntity = m_shockingEvent.GetSourceEntity();

	// Early out if no source entity
	if (!pEntity)
	{
		return FSM_Continue;
	}

	Assertf(m_shockingEvent.GetSourcePosition().Mag2() > 0.01f, "The shocking-events system is telling a ped to flee from the origin.  This is likely to be an error.");

	// AUDIO, I'm running away from a nutter
	if(!pPed->CheckBraveryFlags(BF_DONT_SAY_PANIC_ON_FLEE) && ((m_shockingEvent.m_eType >= EGunshotFired) || (m_shockingEvent.m_eType >= EInjuredPed && fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < 0.33f)))
	{
		if(CanWePlayShockingSpeech())
		{
			if(!SayShockingSpeech(-1.0f,g_PainVoice)))
			{
				pPed->NewSay("PANIC", g_PainVoice);
			}
		}
	}

	CPed* pSourcePed = (pEntity && pEntity->GetIsTypePed()) ? static_cast<CPed*>(pEntity) : NULL;

	if(pSourcePed && !pPed->GetPedIntelligence()->IsFriendlyWith(*pSourcePed))
	{
		SetState(State_FleeFromUnfriendly);
	}
	else if(!pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
	{
		SetState(State_FleeFromFriendly);
	}
	else if(pPed->GetMyVehicle())
	{
		SetState(State_LeaveCarAndFlee);
	}

	return FSM_Continue;
}

CTaskShockingEventFlee::FSM_Return CTaskShockingEventFlee::StateFleeFromUnfriendlyOnEnter(CPed* UNUSED_PARAM(pPed))
{
	aiTask* pTask=rage_new CTaskThreatResponse(static_cast<CPed*>(m_shockingEvent.GetSourceEntity()));
// 	CTaskComplexCombat* pCombatTask = rage_new CTaskComplexCombat(static_cast<CPed*>(m_shockingEvent.GetSourceEntity()));
// 	pCombatTask->SetForceRetreat(true);
// 	pCombatTask->SetKeepMovingWhilstWaitingForFleePath(m_bKeepMovingWhilstWaitingForFleePath);

// 	if(m_shockingEvent.m_eType < EGunshotFired)
// 	{
// 		pCombatTask->SetPlayPanickedAudio(false);
// 	}

	SetNewTask(pTask);
	return FSM_Continue;
}

CTaskShockingEventFlee::FSM_Return CTaskShockingEventFlee::StateFleeFromFriendlyOnEnter(CPed* UNUSED_PARAM(pPed))
{
	CTaskSmartFlee* pFleeTask = rage_new CTaskSmartFlee(CAITarget(m_shockingEvent.GetSourcePosition()), 999.0f);
	pFleeTask->SetKeepMovingWhilstWaitingForFleePath(m_bKeepMovingWhilstWaitingForFleePath);

	SetNewTask(pFleeTask);
	return FSM_Continue;
}

CTaskShockingEventFlee::FSM_Return CTaskShockingEventFlee::StateLeaveCarAndFleeOnEnter(CPed* pPed)
{
	CTaskSequenceList* pSequence = rage_new CTaskSequenceList();
	pSequence->AddTask(rage_new CTaskExitVehicle(pPed->GetMyVehicle(), RF_DontCloseDoor));
	pSequence->AddTask(rage_new CTaskSmartFlee(CAITarget(m_shockingEvent.GetSourcePosition())));
	SetNewTask(rage_new CTaskUseSequence(*pSequence));
	return FSM_Continue;
}

#endif	// 0

//-------------------------------------------------------------------------
// Helper functions
//-------------------------------------------------------------------------

static u32 GetRandomWatchTimeMS(const CEventShocking& shockingEvent, bool bUseHurryAwayTimes)
{
	const CEventShocking::Tunables& tunables = shockingEvent.GetTunables();
	const float minWatchTime = bUseHurryAwayTimes ? tunables.m_MinWatchTimeHurryAway : tunables.m_MinWatchTime;
	const float maxWatchTime = bUseHurryAwayTimes ? tunables.m_MaxWatchTimeHurryAway : tunables.m_MaxWatchTime;
	float timeSeconds = fwRandom::GetRandomNumberInRange(minWatchTime, maxWatchTime);
	return (u32)(timeSeconds*1000.0f);
}

static u32 GetRandomPhoneFilmTimeMS(const CEventShocking& shockingEvent, bool bUseHurryAwayTimes)
{
	const CEventShocking::Tunables& tunables = shockingEvent.GetTunables();
	const float minWatchTime = bUseHurryAwayTimes ? tunables.m_MinPhoneFilmTimeHurryAway : tunables.m_MinPhoneFilmTime;
	const float maxWatchTime = bUseHurryAwayTimes ? tunables.m_MaxPhoneFilmTimeHurryAway : tunables.m_MaxPhoneFilmTime;
	float timeSeconds = fwRandom::GetRandomNumberInRange(minWatchTime, maxWatchTime);
	return (u32)(timeSeconds*1000.0f);
}

//-------------------------------------------------------------------------
// CTaskShockingEvent - Base implementation, keeps event information
//-------------------------------------------------------------------------

CTaskShockingEvent::Tunables CTaskShockingEvent::sm_Tunables;

IMPLEMENT_RESPONSE_TASK_TUNABLES(CTaskShockingEvent, 0x42c3067d);

CTaskShockingEvent::CTaskShockingEvent(const CEventShocking* event)
		: m_ShockingEvent(NULL)
		, m_TimeNotSensedMs(0)
		, m_TimeNotSensedThresholdMs(0)
		, m_uRunningFlags(0)
		, m_bShouldPlayShockingSpeech(true)
{
	if(event && CEvent::CanCreateEvent())
	{
		CEvent* pClone = event->Clone();
		Assert(pClone == dynamic_cast<CEventShocking*>(pClone));
		m_ShockingEvent = static_cast<CEventShocking*>(pClone);
	}
	SetInternalTaskType(CTaskTypes::TASK_SHOCKING_EVENT);
}

CTaskShockingEvent::~CTaskShockingEvent()
{
	delete m_ShockingEvent;
}

CPed* CTaskShockingEvent::GetPlayerCriminal() const
{
	//Ensure the shocking event is valid.
	if(!m_ShockingEvent)
	{
		return NULL;
	}

	return m_ShockingEvent->GetPlayerCriminal();
}

CPed* CTaskShockingEvent::GetSourcePed() const
{
	//Ensure the shocking event is valid.
	if(!m_ShockingEvent)
	{
		return NULL;
	}

	return m_ShockingEvent->GetSourcePed();
}

void CTaskShockingEvent::SetCrimesAsWitnessed()
{
	//Ensure the player criminal is valid.
	CPed* pPlayerCriminal = GetPlayerCriminal();
	if(!pPlayerCriminal)
	{
		return;
	}

	//Set the crimes as witnessed.
	CPed* pPed = GetPed();
	if(!pPed->IsLawEnforcementPed())
	{
		pPlayerCriminal->GetPlayerWanted()->SetAllPendingCrimesWitnessedByPed(*pPed);
	}
	else
	{
		pPlayerCriminal->GetPlayerWanted()->SetAllPendingCrimesWitnessedByLaw();
	}
}

CTaskShockingEvent::FSM_Return CTaskShockingEvent::ProcessPreFSM()
{
	if(!m_ShockingEvent)
	{
		return FSM_Quit;
	}

	if(!m_uRunningFlags.IsFlagSet(RF_DisableSanityCheckEntities))
	{
		if(!SanityCheckEntities())
		{
			return FSM_Quit;
		}
	}

	if(!m_uRunningFlags.IsFlagSet(RF_DisableCheckCanStillSense))
	{
		if(CheckCanStillSense())
		{
			m_TimeNotSensedMs = 0;
		}
		else
		{
			m_TimeNotSensedMs += GetTimeStepInMilliseconds();
			if(m_TimeNotSensedMs > m_TimeNotSensedThresholdMs)
			{
				return FSM_Quit;
			}
		}
	}

	return FSM_Continue;
}


bool CTaskShockingEvent::CheckCanStillSense() const
{
	static dev_float fRange = 3.0f;
	return m_ShockingEvent->CanBeSensedBy(*GetPed(), fRange);
}


bool CTaskShockingEvent::SanityCheckEntities() const
{
	return m_ShockingEvent->SanityCheckEntities();
}

void CTaskShockingEvent::SwapOutShockingEvent(const CEventShocking* pOther)
{
	Assert(pOther);
	// Delete the old owned copy.
	delete m_ShockingEvent;

	// Create a new owned copy.
	const CEventShocking* pClone = static_cast<CEventShocking*>(pOther->Clone());
	Assert(pClone);

	// Set the task copy to the owned copy.
	m_ShockingEvent = pClone;
}

// PURPOSE: Determine whether or not the task should abort
// RETURNS: Checks distance if the other event is a shocking event with equal priority, otherwise return default behaviour as defined in task.h
bool CTaskShockingEvent::ShouldAbort(const AbortPriority priority, const aiEvent* pEvent)
{
	CPed* pPed = GetPed();
	//validate the priority is the same
	if (pEvent && m_ShockingEvent && m_ShockingEvent->GetEventPriority() == pEvent->GetEventPriority())
	{
		const CEvent* pEEVent = static_cast<const CEvent*>(pEvent);
		//validate the other is a shocking event...probably covered by previous check but might as well make sure
		if (pEEVent->IsShockingEvent())
		{
			//compare distances to ped
			const CEventShocking* pSEvent = static_cast<const CEventShocking*>(pEvent);
			Vec3V vOther = pSEvent->GetEntityOrSourcePosition();
			Vec3V vPed = pPed->GetTransform().GetPosition();
			ScalarV sCurrentDistanceSq = DistSquared(m_ShockingEvent->GetEntityOrSourcePosition(), vPed);
			ScalarV sOtherDistanceSq = DistSquared(vOther, vPed);
			//if the other is closer, return true
			if (IsLessThanAll(sOtherDistanceSq, sCurrentDistanceSq))
			{
				return true;
			}
			else
			{
				return false;
			}
		}
	}
	//otherwise stick to the default behaviour
	return CTask::ShouldAbort(priority, pEvent);
}

// Common function for telling the IK manager to look at the shocking event.
void CTaskShockingEvent::LookAtEvent()
{
	CPed* pPed = GetPed();

	//validate the shocking event
	if (!m_ShockingEvent.Get())
	{
		return;
	}

	CEntity* pSourceEntity = m_ShockingEvent->GetSourceEntity();

	u32 uFlags = m_ShockingEvent->GetTunables().m_IgnoreFovForHeadIk ? LF_WHILE_NOT_IN_FOV : 0;

	//look at head of peds
	if(pSourceEntity && pSourceEntity->GetIsTypePed())
	{
		pPed->GetIkManager().LookAt(0, pSourceEntity, 100, BONETAG_HEAD, NULL, uFlags);
	}
	//look at position of entities
	else if (pSourceEntity)
	{
		pPed->GetIkManager().LookAt(0, pSourceEntity, 100, BONETAG_INVALID, NULL, uFlags);
	}
	//otherwise just look at the position of the event source (for explosions, etc.)
	else
	{
		pPed->GetIkManager().LookAt(0, NULL, 100, BONETAG_INVALID, &(m_ShockingEvent->GetSourcePosition()), uFlags);
	}
}

bool CTaskShockingEvent::CanWePlayShockingSpeech() const		
{ 
	if(!m_bShouldPlayShockingSpeech || !m_ShockingEvent)
		return false;

#if !__NO_OUTPUT
	u32 timeElapsed = fwTimer::GetTimeInMilliseconds() - m_ShockingEvent->GetStartTime();
	naDisplayf("CanPlayShock  Time %u %s", timeElapsed, timeElapsed < m_ShockingEvent->GetTunables().m_MaxTimeForAudioReaction ? "TRUE" : "FALSE");
#endif // !__NO_OUTPUT

	return fwTimer::GetTimeInMilliseconds() - m_ShockingEvent->GetStartTime() < m_ShockingEvent->GetTunables().m_MaxTimeForAudioReaction;
}

bool CTaskShockingEvent::IsValidForMotionTask(CTaskMotionBase& task) const
{
	bool bIsValid = CTask::IsValidForMotionTask(task);

	if (!bIsValid && GetSubTask())
		bIsValid = GetSubTask()->IsValidForMotionTask(task);

	return bIsValid;
}

// Common function for saying the line associated with this shocking event.
bool CTaskShockingEvent::SayShockingSpeech(float fRandom, u32 iVoice)
{
	if (m_ShockingEvent)
	{
		// Grab the ped.
		CPed* pPed = GetPed();

		CEvent* pEvent = pPed->GetPedIntelligence()->GetCurrentEvent();
		if(pEvent && pEvent->HasEditableResponse())
		{
			CEventEditableResponse* pEventEditable = static_cast<CEventEditableResponse*>(pEvent);
			if(pEventEditable->GetSpeechHash() != 0)
			{
				atHashString hLine(pEventEditable->GetSpeechHash());
				if(fRandom == 1.0f)
				{
					pPed->NewSay(hLine.GetHash(),iVoice);
				}
				else
				{
					pPed->RandomlySay(hLine.GetHash(), fRandom);
				}

				return true;
			}
		}

		atHashWithStringNotFinal sSpeech = m_ShockingEvent->GetShockingSpeechHash();
		if (sSpeech.GetHash())
		{
			// If the hash was valid, tell the ped to say it.
			if(fRandom == 1.0f)
			{
				pPed->NewSay(sSpeech.GetHash(), iVoice);
			}
			else
			{
				pPed->RandomlySay(sSpeech.GetHash(), fRandom);
			}
			return true;
		}
	}
	return false;
}

// Common function for heading alignment during response clips.
void CTaskShockingEvent::AdjustAngularVelocityForIntroClip(float fTimeStep, const crClip& rClip, float fPhase, const Quaternion& qClipTotalMoverTrackRotation)
{

	PF_FUNC(AdjustAngularVelocityForIntroClip);
	CPed *pPed = GetPed();
	// Find out how much the ped wants to turn.
	const float fDesiredHeadingChange =  SubtractAngleShorter(pPed->GetDesiredHeading(), pPed->GetCurrentHeading());

	if (!IsNearZero(fDesiredHeadingChange))
	{
		// Grab the ped's current angular velocity.
		float fRotateSpeed = pPed->GetDesiredAngularVelocity().z;

		// Only scale if the ped is rotating in the same direction as the way it wants to go.
		if (Sign(fRotateSpeed) == Sign(fDesiredHeadingChange))
		{
			const float fTimeRemaining = (1.0f - fPhase) * rClip.GetDuration();

			// Get the remaining orientation change left in the clip
			Quaternion qClipTotalRot(fwAnimHelpers::GetMoverTrackRotation(rClip, fPhase));
			qClipTotalRot.Inverse();
			qClipTotalRot.Multiply(qClipTotalMoverTrackRotation);		

			// For rotation we're only interested in the heading
			Vector3 vClipRotEulers(Vector3::ZeroType);
			qClipTotalRot.ToEulers(vClipRotEulers);
			const float fRemainingRotation = fwAngle::LimitRadianAngle(vClipRotEulers.z);

			if (Abs(fRemainingRotation) > sm_Tunables.m_MinRemainingRotationForScaling)
			{
				// If quite a bit of rotation remains, scale.
				float fScale = Abs(fDesiredHeadingChange / fRemainingRotation);
				fScale = rage::Clamp(fScale, sm_Tunables.m_MinAngularVelocityScaleFactor, sm_Tunables.m_MaxAngularVelocityScaleFactor);
				fRotateSpeed *= fScale;
			}
			else
			{
				// If not much rotation remains, modify the speed directly.
				fRotateSpeed = fDesiredHeadingChange / Max(fTimeStep, fTimeRemaining);
			}
			// Change the desired angular velocity to nail the target.
			pPed->SetDesiredAngularVelocity(Vector3(0.0f, 0.0f, fRotateSpeed));
		}
	}
}

#if !__FINAL

void CTaskShockingEvent::Debug() const
{
#if DEBUG_DRAW
	if(m_ShockingEvent)
	{
		Vector3 vPosition;
		m_ShockingEvent->GetEntityOrSourcePosition(vPosition);
		grcDebugDraw::Line(VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition()), vPosition, Color_yellow);
	}
#endif

	// Base class
	CTask::Debug();
}


const char* CTaskShockingEvent::GetCombinedDebugStateName(const char* state) const
{
	static char buff[128];

	const char* eventName = "";
	if(m_ShockingEvent)
	{
		eventName = CEventNames::GetEventName((eEventType)m_ShockingEvent->GetEventType());
	}

	formatf(buff, "%s:(%s)", state, eventName);
	return buff;
}

#endif // !__FINAL

//-------------------------------------------------------------------------
// CTaskShockingEventGoto
//-------------------------------------------------------------------------

CTaskShockingEventGoto::Tunables CTaskShockingEventGoto::sm_Tunables;

IMPLEMENT_RESPONSE_TASK_TUNABLES(CTaskShockingEventGoto, 0xc9fae12c);

CTaskShockingEventGoto::CTaskShockingEventGoto(const CEventShocking* event)
		: CTaskShockingEvent(event)
		, m_CrowdRoundPos(V_ZERO)
		, m_WatchRadius(0.0f)
		, m_StopWatchDistance(0.0f)
{
	SetInternalTaskType(CTaskTypes::TASK_SHOCKING_EVENT_GOTO);
}

CTask::FSM_Return CTaskShockingEventGoto::ProcessPreFSM()
{
	//Grab the ped.
	CPed* pPed = GetPed();

	pPed->GetPedIntelligence()->SetCheckShockFlag(true);

	// Allow certain lod modes
	pPed->GetPedAiLod().ClearBlockedLodFlag(CPedAILod::AL_LodPhysics);
	pPed->GetPedAiLod().ClearBlockedLodFlag(CPedAILod::AL_LodMotionTask);

	return CTaskShockingEvent::ProcessPreFSM();
}

CTaskShockingEventGoto::FSM_Return CTaskShockingEventGoto::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		FSM_State(State_Initial)
			FSM_OnUpdate
				return StateInitialOnUpdate();
		FSM_State(State_Goto)
			FSM_OnEnter
				return StateGotoOnEnter();
			FSM_OnUpdate
				return StateGotoOnUpdate();
		FSM_State(State_GotoWithinCrowd)
			FSM_OnEnter
				return StateGotoWithinCrowdOnEnter();
			FSM_OnUpdate
				return StateGotoWithinCrowdOnUpdate();
		FSM_State(State_Watch)
			FSM_OnEnter
				return StateWatchOnEnter();
			FSM_OnUpdate
				return StateWatchOnUpdate();
		FSM_State(State_Quit)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

void CTaskShockingEventGoto::CleanUp()
{
	CPed* pPed = GetPed();
	if (pPed)
	{
		CTaskMotionBase* pMotionTask = pPed->GetPrimaryMotionTaskIfExists();
		// Restore the ped to its default motion anims.
		if (pMotionTask)
		{
			pMotionTask->ResetOnFootClipSet();
		}
	}
}

#if !__FINAL

void CTaskShockingEventGoto::Debug() const
{
	CTaskShockingEvent::Debug();

#if DEBUG_DRAW
	// If we are in a state where m_CrowdRoundPos is supposed
	// to be computed, display it.
	const int state = GetState();
	if(state != State_Initial && state != State_Goto)
	{
		Vec3V vX(0.3f, 0.0f, 0.0f);
		Vec3V vY(0.0f, 0.3f, 0.0f);
		Color32 col(255, 128, 0, 255);

		// Draw a little cross at m_CrowdRoundPos.
		grcDebugDraw::Line(m_CrowdRoundPos - vX, m_CrowdRoundPos + vX, col);
		grcDebugDraw::Line(m_CrowdRoundPos - vY, m_CrowdRoundPos + vY, col);
	}
#endif
}


const char * CTaskShockingEventGoto::GetStaticStateName( s32 iState )
{
	static const char* aStateNames[] = 
	{
		"Initial",
		"Goto",
		"GotoWithinCrowd",
		"Watch",
		"Quit",
	};

	CompileTimeAssert(NELEM(aStateNames) == kNumStates);
	Assert(iState >= 0 && iState < kNumStates);
	return aStateNames[iState];
}

#endif // !__FINAL

CTaskShockingEventGoto::FSM_Return CTaskShockingEventGoto::StateInitialOnUpdate()
{
	// Get the radius.
	m_WatchRadius = m_ShockingEvent->GetTunables().m_GotoWatchRange;

	// Set the stop watch distance to be the current distance plus a tolerance to start with.
	m_StopWatchDistance = Dist(GetPed()->GetTransform().GetPosition(), GetCenterLocation()).Getf() + sm_Tunables.m_ExtraToleranceForStopWatchDistance;

	// Play some speech.
	PlayInitialSpeech();

	// Determine which state we should start at.
	EvaluatePositionAndSwitchState();

	return FSM_Continue;
}



CTaskShockingEventGoto::FSM_Return CTaskShockingEventGoto::StateGotoOnEnter()
{
	// Start a movement task to go towards the center location.
	Vec3V center = GetCenterLocation();
	CTaskMoveFollowNavMesh* pMoveTask = rage_new CTaskMoveFollowNavMesh(sm_Tunables.m_MoveBlendRatioForFarGoto, RCC_VECTOR3(center), m_WatchRadius + sm_Tunables.m_ExtraDistForGoto);
	pMoveTask->SetKeepMovingWhilstWaitingForPath(true);
	SetMoveTaskWithAmbientClips(pMoveTask);

	return FSM_Continue;
}


CTaskShockingEventGoto::FSM_Return CTaskShockingEventGoto::StateGotoOnUpdate()
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		EvaluatePositionAndSwitchState();
		return FSM_Continue;
	}

	// Keep the movement task up to date with the target position, etc.
	const Vec3V center = GetCenterLocation();
	return StateGotoCommonUpdate(center);
}


CTaskShockingEventGoto::FSM_Return CTaskShockingEventGoto::StateGotoWithinCrowdOnEnter()
{
	UpdateCrowdRoundPosition();

	// If close to the crowd then we want to walk, and our target is the position we calculated at the desired radius from the center location.
	CTaskMoveFollowNavMesh* pMoveTask = rage_new CTaskMoveFollowNavMesh(MOVEBLENDRATIO_WALK, RCC_VECTOR3(m_CrowdRoundPos), sm_Tunables.m_TargetRadiusForCloseNavMeshTask);
	pMoveTask->SetKeepMovingWhilstWaitingForPath(true);
	SetMoveTaskWithAmbientClips(pMoveTask);

	return FSM_Continue;
}


CTaskShockingEventGoto::FSM_Return CTaskShockingEventGoto::StateGotoWithinCrowdOnUpdate()
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		EvaluatePositionAndSwitchState();
		return FSM_Continue;
	}

	// Keep the movement task up to date with the target position, etc.
	UpdateCrowdRoundPosition();
	return StateGotoCommonUpdate(m_CrowdRoundPos);
}


CTask::FSM_Return CTaskShockingEventGoto::StateWatchOnEnter()
{
	CTaskShockingEventReact* pReactTask = rage_new CTaskShockingEventReact(m_ShockingEvent);
	SetNewTask(pReactTask);

	return FSM_Continue;
}


CTask::FSM_Return CTaskShockingEventGoto::StateWatchOnUpdate()
{
	//if we are too far away from the event, tell the react animation to stop playing
	if (IsPedTooFarToReact())
	{
		CTask* pSubTask = GetSubTask();
		if (pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_SHOCKING_EVENT_REACT)
		{
			CTaskShockingEventReact* pReactSubtask = static_cast<CTaskShockingEventReact*>(pSubTask);
			pReactSubtask->StopReacting();
		}
	}


	// Check if react subtask is finished
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		return FSM_Quit;
	}

	// Otherwise keep playing the reaction
	return FSM_Continue;
}


CTask::FSM_Return CTaskShockingEventGoto::StateGotoCommonUpdate(Vec3V_In gotoTgt)
{
	// Quit the task if the ped is too far away
	if (IsPedTooFarToReact())
	{
		return FSM_Quit;
	}

	// Here, we keep the CTaskComplexControlMovement and CTaskMoveFollowNavMesh tasks up to date
	// with the current target.
	CPed* pPed = GetPed();
	CTask* pSubTask = GetSubTask();
	if(pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
	{
		CTaskComplexControlMovement* pCtrlTask = static_cast<CTaskComplexControlMovement*>(pSubTask);
		if(pCtrlTask->GetMoveTaskType() == CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH)
		{
			// Use CTaskComplexControlMovement::SetTarget() instead of CTaskMoveFollowNavMesh::SetTarget(),
			// as it appears to do some potentially useful stuff.
			const Vector3 tmp = VEC3V_TO_VECTOR3(gotoTgt);
			pCtrlTask->SetTarget(pPed, tmp);

			CTask* pMoveTask = pCtrlTask->GetRunningMovementTask();
			if(pMoveTask)
			{
				Assert(pMoveTask->GetTaskType() == CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH);
				CTaskMoveFollowNavMesh* pNavMeshTask = static_cast<CTaskMoveFollowNavMesh*>(pMoveTask);

				// Make sure we keep moving as we update the target position.
				pNavMeshTask->SetKeepMovingWhilstWaitingForPath(true);
			}
		}
	}

	PlayContinuousSpeech();

	// Face the shocking event we are going to
	LookAtEvent();

	return FSM_Continue;
}


void CTaskShockingEventGoto::EvaluatePositionAndSwitchState()
{
	int newState;
	if(!IsPedCloseToEvent())
	{
		// Far away, go towards the target.
		newState = State_Goto;
	}
	else
	{
		// Check if we're already standing in a good position, not too close
		// to anybody else.
		UpdateCrowdRoundPosition();
		if(IsAtDesiredPosition(false))
		{
			// Stand and watch.
			newState = State_Watch;
		}
		else
		{
			// Adjust the position within the crowd, moving away from others.
			newState = State_GotoWithinCrowd;
		}
	}

	// Switch state, if needed.
	if(newState != GetState())
	{
		SetState(newState);
	}
	else
	{
		// Already in the desired state, just restart it.
		SetFlag(aiTaskFlags::RestartCurrentState);
	}
}


void CTaskShockingEventGoto::UpdateCrowdRoundPosition()
{
	// Note: this code was adapted from CTaskMoveCrowdAroundLocation::CalcCrowdRoundPosition()
	// and CTaskMoveCrowdAroundLocation::AdjustCrowdRndPosForOtherPeds(), but with some changes to
	// be less strict about the exact distance from the target, conversion to the new vector library, etc.

	CPed* pPed = GetPed();

	const Vec3V centerV = GetCenterLocation();

	const Vec3V toPedNonNormV = Subtract(pPed->GetTransform().GetPosition(), centerV);

	const Vec3V toPedV = Normalize(toPedNonNormV);

	const float dist = Mag(toPedNonNormV).Getf();
	const float minRadius = m_WatchRadius*0.5f;
	const float maxRadius = m_WatchRadius;
	const float radiusToUse = Clamp(dist, minRadius, maxRadius);

	const ScalarV radiusToUseV(radiusToUse);
	const Vec3V idealPosV = AddScaled(centerV, toPedV, radiusToUseV);

	const float distTooClose = sm_Tunables.m_MinDistFromOtherPeds;
	const float distTooCloseSquared = square(distTooClose);

	Vec3V adjustmentV(V_ZERO);

	const CEntityScannerIterator entityList = pPed->GetPedIntelligence()->GetNearbyPeds();
	for(const CEntity* pEnt = entityList.GetFirst(); pEnt; pEnt = entityList.GetNext())
	{
		const CPed* pOtherPed = static_cast<const CPed*>(pEnt);
		if(pOtherPed->IsDead() || pOtherPed->IsFatallyInjured())
		{
			continue;
		}

		// Sum up vectors away from all peds which are too close.
		const Vec3V otherPedPos = pOtherPed->GetTransform().GetPosition();
		const Vec3V diffV = Subtract(idealPosV, otherPedPos);
		const float fDiffSqr = MagSquared(diffV).Getf();
		if(fDiffSqr < distTooCloseSquared && fDiffSqr > 0.01f)
		{
			const float fInvScale = distTooClose - sqrtf(fDiffSqr);
			const Vec3V diffNormalizedV = Normalize(diffV);
			adjustmentV = AddScaled(adjustmentV, diffNormalizedV, ScalarV(fInvScale));
		}
	}

	m_CrowdRoundPos = Add(idealPosV, adjustmentV);
}


unsigned int CTaskShockingEventGoto::IsAtDesiredPosition(bool alreadyThere) const
{
	const CPed* pPed = GetPed();

	const Vec3V pedPosV = pPed->GetTransform().GetPosition();
	const Vec3V crowdRoundPosV = m_CrowdRoundPos;
	const ScalarV distSqV = DistSquared(pedPosV, crowdRoundPosV);

	ScalarV distSqThresholdV;
	if(alreadyThere)
	{
		distSqThresholdV = LoadScalar32IntoScalarV(sm_Tunables.m_DistSquaredThresholdAtCrowdRoundPos);
	}
	else
	{
		distSqThresholdV = LoadScalar32IntoScalarV(sm_Tunables.m_DistSquaredThresholdMovingToCrowdRoundPos);
	}

	return IsLessThanAll(distSqV, distSqThresholdV);
}


unsigned int CTaskShockingEventGoto::IsPedCloseToEvent() const
{
	const ScalarV watchRadiusV = LoadScalar32IntoScalarV(m_WatchRadius);
	const ScalarV extraDistV = LoadScalar32IntoScalarV(sm_Tunables.m_DistVicinityOfCrowd);
	const Vec3V pedPosV = GetPed()->GetTransform().GetPosition();
	const Vec3V centerV = GetCenterLocation();

	// Compute the distance threshold (desired radius + extra distance).
	const ScalarV distThresholdV = Add(watchRadiusV, extraDistV);
	const ScalarV distSquaredThresholdV = Scale(distThresholdV, distThresholdV);

	// Compute the distance from the center.
	const ScalarV distSquaredToTargetV = DistSquared(centerV, pedPosV);

	return IsLessThanAll(distSquaredToTargetV, distSquaredThresholdV);
}

// Return true if the ped is farthar away than the threshold for allowing reactions.
unsigned int CTaskShockingEventGoto::IsPedTooFarToReact()
{
	// stop watch distance shrinks as the ped gets closer to the event, but cannot fall below the value passed in from the tunables
	ScalarV distanceToEventV = Dist(GetPed()->GetTransform().GetPosition(), GetCenterLocation());
	m_StopWatchDistance = Max(Min(distanceToEventV.Getf() + sm_Tunables.m_ExtraToleranceForStopWatchDistance, m_StopWatchDistance), sm_Tunables.m_ExtraToleranceForStopWatchDistance + m_ShockingEvent->GetTunables().m_StopWatchDistance);
	
	const ScalarV acceptableDistanceV = LoadScalar32IntoScalarV(m_StopWatchDistance);

	return IsGreaterThanAll(distanceToEventV, acceptableDistanceV);
}


Vec3V_Out CTaskShockingEventGoto::GetCenterLocation() const
{
	if (m_ShockingEvent)
	{
		return m_ShockingEvent->GetReactPosition();
	}
	else
	{
		return Vec3V(V_ZERO);
	}
}


void CTaskShockingEventGoto::SetMoveTaskWithAmbientClips(CTaskMove* pMoveTask)
{
	CTaskAmbientClips* pAmbientTask	= rage_new CTaskAmbientClips(CTaskAmbientClips::Flag_PlayBaseClip | CTaskAmbientClips::Flag_PlayIdleClips | CTaskAmbientClips::Flag_ForceFootClipSetRestart, CONDITIONALANIMSMGR.GetConditionalAnimsGroup("SHOCK_GOTO"));
	SetNewTask(rage_new CTaskComplexControlMovement(pMoveTask, pAmbientTask));
}


void CTaskShockingEventGoto::PlayInitialSpeech()
{
	CPed* pPed = GetPed();
	if(CanWePlayShockingSpeech())
	{
		if(!SayShockingSpeech())
		{
			pPed->NewSay("SURPRISED");
		}
	}
}


void CTaskShockingEventGoto::PlayContinuousSpeech()
{
	if(CanWePlayShockingSpeech())
	{
		CPed* pPed = GetPed();
		if(!SayShockingSpeech(0.05f))
		{
			pPed->RandomlySay("SURPRISED", 0.05f);
		}
	}
}

//-------------------------------------------------------------------------
// CTaskShockingEventHurryAway
//-------------------------------------------------------------------------

CTaskShockingEventHurryAway::Tunables CTaskShockingEventHurryAway::sm_Tunables;
s32 CTaskShockingEventHurryAway::sm_iLastChosenAnim = -1;

IMPLEMENT_RESPONSE_TASK_TUNABLES(CTaskShockingEventHurryAway, 0xe242f070);

CTaskShockingEventHurryAway::CTaskShockingEventHurryAway(const CEventShocking* event)
: CTaskShockingEvent(event)
, m_fTimeToCallPolice(0.0f)
, m_fTimeOffScreenWhenHurrying(0.0f)
, m_fClosePlayerCounter(0.0f)
, m_iConditionalAnimsChosen(-1)
, m_bCallPolice(false)
, m_bHasCalledPolice(false)
, m_bFleeing(false)
, m_bResumePreviousAction(false)
, m_bGoDirectlyToWaitForPath(false)
, m_bOnPavement(false)
, m_bCaresAboutPavement(true)
, m_bZoneRequiresPavement(true)
, m_bForcePavementNotRequired(false)
, m_bEvadedRecentlyWhenTaskStarted(false)
, m_bShouldChangeMovementClip(false)
, m_bDisableShortRouteCheck(false)
, m_fHurryAwayMaxTime(-1.0f)
{
	// To prevent us from just turning away, losing track of the event we are reacting to,
	// and stop the task, we use a time delay after last sensing the event, to keep moving
	// away for a while.
	m_TimeNotSensedThresholdMs = 10*1000;	// MAGIC!

	SetInternalTaskType(CTaskTypes::TASK_SHOCKING_EVENT_HURRYAWAY);
}

float CTaskShockingEventHurryAway::GetTimeHurryingAway() const
{
	return (IsHurryingAway() ? GetTimeInState() : 0.0f);
}

bool CTaskShockingEventHurryAway::IsBackingAway() const
{
	return (GetState() == State_BackAway);
}

bool CTaskShockingEventHurryAway::IsHurryingAway() const
{
	return (GetState() == State_HurryAway);
}

bool CTaskShockingEventHurryAway::SetSubTaskForHurryAway(CTask* pSubTask)
{
	//Ensure we are hurrying away.
	if(taskVerifyf(IsHurryingAway(), "Unable to set the sub-task for hurry away."))
	{
		//Get the task.
		CTaskComplexControlMovement* pTask = static_cast<CTaskComplexControlMovement *>(
			FindSubTaskOfType(CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT));
		if(pTask)
		{
			//Set the sub-task.
			pTask->SetNewMainSubtask(pSubTask);
			return true;
		}
	}

	//Free the sub-task.
	delete pSubTask;

	return false;
}

bool CTaskShockingEventHurryAway::IsValidForMotionTask(CTaskMotionBase& task) const
{
	bool bIsValid = task.IsInWater();
	if (!bIsValid)
		bIsValid = CTask::IsValidForMotionTask(task);

	if (!bIsValid && GetSubTask())
		bIsValid = GetSubTask()->IsValidForMotionTask(task);

	return bIsValid;
}

bool    CTaskShockingEventHurryAway::IsPedfarEnoughToMigrate() //used in network by control passing checks
{
	if(m_ShockingEvent && !GetPed()->GetInteriorLocation().IsValid())
	{
		ScalarV distanceToEventV = Dist(GetPed()->GetTransform().GetPosition(), m_ShockingEvent->GetReactPosition());
		const ScalarV acceptableDistanceV = LoadScalar32IntoScalarV(m_ShockingEvent->GetTunables().m_StopWatchDistance);
		
		bool bIsPedfarEnoughToMigrate = IsGreaterThanAll(distanceToEventV, acceptableDistanceV) !=0;

		return bIsPedfarEnoughToMigrate;
	}	

	return true;
}

aiTask* CTaskShockingEventHurryAway::Copy() const
{
	CTaskShockingEventHurryAway* pCopy = rage_new CTaskShockingEventHurryAway(m_ShockingEvent);
	if (pCopy)
	{
		pCopy->SetShouldPlayShockingSpeech(ShouldPlayShockingSpeech());
		pCopy->SetGoDirectlyToWaitForPath(m_bGoDirectlyToWaitForPath);
		pCopy->SetForcePavementNotRequired(m_bForcePavementNotRequired);
		pCopy->SetHurryAwayMaxTime(m_fHurryAwayMaxTime);
	}
	return pCopy;
}

CTask::FSM_Return CTaskShockingEventHurryAway::ProcessPreFSM()
{
	//Grab the ped.
	CPed* pPed = GetPed();

	// Allow certain lod modes
	pPed->GetPedAiLod().ClearBlockedLodFlag(CPedAILod::AL_LodPhysics);
	pPed->GetPedAiLod().ClearBlockedLodFlag(CPedAILod::AL_LodMotionTask);

	// Check for other shocking events, which may have a higher priority.
	pPed->GetPedIntelligence()->SetCheckShockFlag(true);

	// Force the ped not to orient towards their path if they have one, we want to control the desired heading directly from this task.
	if (GetState() != State_HurryAway && GetState() != State_WaitForPath)
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_DisableMoveTaskHeadingAdjustments, true);
	}

	if (!ActAsIfWeHavePavement() && m_pavementHelper.IsSearchActive())
	{
		m_pavementHelper.UpdatePavementFloodFillRequest(pPed);
		m_bOnPavement = m_pavementHelper.HasPavement();
	}

	return CTaskShockingEvent::ProcessPreFSM();
}


CTaskShockingEventHurryAway::FSM_Return CTaskShockingEventHurryAway::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		FSM_State(State_Init)
			FSM_OnUpdate
				return StateInitOnUpdate();
		FSM_State(State_React)
			FSM_OnEnter
				StateReactOnEnter();
			FSM_OnUpdate
				return StateReactOnUpdate();
		FSM_State(State_Watch)
			FSM_OnEnter
				StateWatchOnEnter();
			FSM_OnUpdate
				return StateWatchOnUpdate();
		FSM_State(State_WaitForPath)
			FSM_OnEnter
				StateWaitForPathOnEnter();
			FSM_OnUpdate
				return StateWaitForPathOnUpdate();
		FSM_State(State_BackAway)
			FSM_OnUpdate
				return StateBackAwayOnUpdate();
		FSM_State(State_HurryAway)
			FSM_OnEnter
				StateHurryAwayOnEnter();
			FSM_OnUpdate
				return StateHurryAwayOnUpdate();
		FSM_State(State_Cower)
			FSM_OnEnter
				StateCowerOnEnter();
			FSM_OnUpdate
				return StateCowerOnUpdate();
		FSM_State(State_Quit)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

// Determine whether or not the task should abort
bool CTaskShockingEventHurryAway::ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
	if (pEvent)
	{
		CEvent* pCEvent = (CEvent*)pEvent;
		// Never interrupt this task for other shocking events unless the response escalated.
		if (pCEvent->IsShockingEvent())
		{
			s32 iTaskType = pCEvent->GetResponseTaskType();
			switch(iTaskType)
			{
				case CTaskTypes::TASK_SHOCKING_EVENT_THREAT_RESPONSE:
				case CTaskTypes::TASK_THREAT_RESPONSE:
				case CTaskTypes::TASK_COMBAT:
				case CTaskTypes::TASK_SMART_FLEE:
				{
					// There is a new (escalated) response to the event.  Switch to doing that instead.
					return CTaskShockingEvent::ShouldAbort(iPriority, pEvent);
				}
				default:
				{
					// If we got here, then the new event has a higher priority value.
					// Switch to responding to the new event.
					SwapOutShockingEvent(static_cast<CEventShocking*>(pCEvent));
					// Make the new event expire - we created a local copy above so we should be fine doing this.
					pCEvent->Tick();
					return false;
				}
			}
		}
		else if (pCEvent->GetEventType() == EVENT_RESPONDED_TO_THREAT)
		{
			CPed* pPed = GetPed();
			// It looks odd to have this response interrupted by flee when the ped is responding to EVENT_RESPONDED_TO_THREAT.
			// So, only exit if the ped is going to fight, not flee.
			CTaskThreatResponse::ThreatResponse iResponse = CTaskThreatResponse::PreDetermineThreatResponseStateFromEvent(*pPed, *pCEvent);
			if (iResponse == CTaskThreatResponse::TR_Fight)
			{
				aiTask* pEventResponseTask = pPed->GetPedIntelligence()->GetEventResponseTask();
				if (pEventResponseTask && pEventResponseTask->GetTaskType() == CTaskTypes::TASK_THREAT_RESPONSE)
				{
					CTaskThreatResponse* pTaskThreatResponse = static_cast<CTaskThreatResponse*>(pEventResponseTask);
					pTaskThreatResponse->SetThreatResponseOverride(iResponse);
				}
			}
			else
			{
				pCEvent->Tick();
				return false;
			}
		}
	}
	return CTaskShockingEvent::ShouldAbort(iPriority, pEvent);
}

void CTaskShockingEventHurryAway::CleanUp()
{
	CPed* pPed = GetPed();
	if (pPed)
	{
		CTaskMotionBase* pMotionTask = pPed->GetPrimaryMotionTaskIfExists();
		// Restore the ped to its default motion anims.
		if (pMotionTask)
		{
			pMotionTask->ResetOnFootClipSet();
		}
	}
}

#if !__FINAL

void CTaskShockingEventHurryAway::Debug() const
{
#if DEBUG_DRAW
	static bool bDrawFleeLines = false;

	const CPed* pPed = GetPed();
	if (pPed)
	{
		if (m_bFleeing && bDrawFleeLines)
		{
			if (m_ShockingEvent->GetSourceEntity())
			{
				grcDebugDraw::Line(pPed->GetTransform().GetPosition(), m_ShockingEvent->GetSourceEntity()->GetTransform().GetPosition(), Color_DarkSeaGreen);
			}
			if (m_ShockingEvent->GetOtherEntity())
			{
				grcDebugDraw::Line(pPed->GetTransform().GetPosition(), m_ShockingEvent->GetOtherEntity()->GetTransform().GetPosition(), Color_DarkSeaGreen);
			}
		}
	}
#endif
}


const char * CTaskShockingEventHurryAway::GetStaticStateName( s32 iState )
{
	static const char* aStateNames[] = 
	{
		"Init",
		"React",
		"WaitingForPath",
		"Watch",
		"BackAway",
		"HurryAway",
		"Cower",
		"Quit",
	};

	CompileTimeAssert(NELEM(aStateNames) == kNumStates);

	return aStateNames[iState];
}

#endif // !__FINAL

CTaskShockingEventHurryAway::FSM_Return CTaskShockingEventHurryAway::StateInitOnUpdate()
{
	//Disable sense checks.  We want the ped to hurry away forever.
	m_uRunningFlags.SetFlag(RF_DisableCheckCanStillSense);

	CPed* pPed = GetPed();
	m_bEvadedRecentlyWhenTaskStarted = CTimeHelpers::GetTimeSince(pPed->GetPedIntelligence()->GetLastTimeEvaded()) < sm_Tunables.m_EvasionThreshold;
	bool bShouldWatchBeforeFleeing = !(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DontWatchFirstOnNextHurryAway)) && m_ShockingEvent->GetTunables().m_HurryAwayWatchFirst;

	AnalyzePavementSituation();

	if (CaresAboutPavement())
	{
		m_bOnPavement = pPed->GetNavMeshTracker().GetIsValid() && pPed->GetNavMeshTracker().IsOnPavement();
		if (!m_bOnPavement)
		{
			m_pavementHelper.StartPavementFloodFillRequest(pPed);
		}
	}

	// Decide if this event is worth skipping watch and going directly to the navmesh portion of the task.
	// Do this if the event should be fled from or this ped has evaded something recently.
	if (ShouldFleeFromShockingEvent())
	{
		m_bGoDirectlyToWaitForPath = true;
	}
	else if (m_bEvadedRecentlyWhenTaskStarted)
	{
		m_bGoDirectlyToWaitForPath = true;
	}

	if( bShouldWatchBeforeFleeing )
	{
		float fChancePlayingInitalTurnAnims = sm_Tunables.m_ChancePlayingInitalTurnAnimBigReact;
		if( m_ShockingEvent->GetTunables().m_ReactionMode == CEventShocking::SMALL_REACTION )
		{
			fChancePlayingInitalTurnAnims = sm_Tunables.m_ChancePlayingInitalTurnAnimSmallReact;
		}
		bShouldWatchBeforeFleeing = fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < fChancePlayingInitalTurnAnims;
	}
	// Say the speech.
	if (m_ShockingEvent && CanWePlayShockingSpeech())
	{
		SayShockingSpeech(m_ShockingEvent->GetTunables().m_ShockingSpeechChance);
	}

	// Certain conditions don't permit the ped to play the hurry sets.
	if (CheckIfAllowedToChangeMovementClip())
	{
		SetShouldChangeMovementClip(true);
	}

	if (IsOnTrain())
	{
		SetState(State_Cower);
	}
	// First watch the event for a bit, then hurry away.	
	else if (m_bGoDirectlyToWaitForPath && ActAsIfWeHavePavement())
	{
		SetState(State_WaitForPath);
	}
	// Gang guys never play shocking event animations.
	else if (bShouldWatchBeforeFleeing && !pPed->IsGangPed())
	{
		SetState(State_React);
	}
	else
	{
		SetState(State_Watch);
	}

	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_DontWatchFirstOnNextHurryAway, false);

	return FSM_Continue;
}

void CTaskShockingEventHurryAway::StateReactOnEnter()
{
	CTaskShockingEventReact* pTaskReact = rage_new CTaskShockingEventReact(m_ShockingEvent);
	if (pTaskReact)
	{
		pTaskReact->SetShouldPlayShockingSpeech(false);
		SetNewTask(pTaskReact);
	}
}

CTaskShockingEventHurryAway::FSM_Return CTaskShockingEventHurryAway::StateReactOnUpdate()
{
	if( ShouldFleeFromShockingEvent() )
	{
		if( IsOnTrain() )
		{
			SetState(State_Cower);
		}
		else
		{
			aiAssert(ActAsIfWeHavePavement());
			SetState(State_WaitForPath);
		}

		return FSM_Continue;
	}

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Watch);
		return FSM_Continue;
	}

	return FSM_Continue;
}

void CTaskShockingEventHurryAway::StateWatchOnEnter()
{
	CPed* pPed = GetPed();

	//  Decide whether we will resume our previous task after watching this event
	CPedMotivation& pedMotivation = pPed->GetPedIntelligence()->GetPedMotivation();
	// Already afraid? Always run
	const bool bPedAfraid = pedMotivation.GetMotivation(CPedMotivation::FEAR_STATE) > (0.1f + m_ShockingEvent->GetTunables().m_PedFearImpact);

	if( (!ActAsIfWeHavePavement()) || (fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < m_ShockingEvent->GetTunables().m_ChanceOfWatchRatherThanHurryAway && !bPedAfraid) )
	{
		m_bResumePreviousAction = true;
	}
	else
	{
		// We no longer want to return to any scenario point
		CTaskUnalerted* pTaskUnalerted = static_cast<CTaskUnalerted*>(pPed->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_UNALERTED));
		if( pTaskUnalerted )
		{
			pTaskUnalerted->ClearScenarioPointToReturnTo();
		}

		m_bResumePreviousAction = false;
	}
	

	const u32 uRandomWatchTime = 0;
	CTaskShockingEventWatch* pWatch = rage_new CTaskShockingEventWatch(m_ShockingEvent, uRandomWatchTime, GetRandomFilmTimeOverrideMS(), m_bResumePreviousAction);
	if (pWatch)
	{
		// Don't play the shocking event speech now, we already had our chance when CTaskShockingEventHurryAway started.
		pWatch->SetShouldPlayShockingSpeech(false);
	}
	SetNewTask(pWatch);
	CTaskUnalerted* pTaskUnalerted = static_cast<CTaskUnalerted*>(pPed->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_UNALERTED));
	if( pTaskUnalerted )
	{
		Vector3 vPos = VEC3V_TO_VECTOR3(m_ShockingEvent->GetReactPosition());
		pTaskUnalerted->SetTargetToWanderAwayFrom(vPos);
	}
}

CTaskShockingEventHurryAway::FSM_Return CTaskShockingEventHurryAway::StateWatchOnUpdate()
{
	if( ShouldFleeFromShockingEvent() )
	{
		if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_SHOCKING_EVENT_WATCH)
		{
			CTaskShockingEventWatch* pSubTask = static_cast<CTaskShockingEventWatch*>(GetSubTask());
			pSubTask->FleeFromEntity();
		}
		else 
		{
			if (IsOnTrain())
			{
				SetState(State_Cower);
			}
			else
			{
				SetState(State_WaitForPath);
			}
		}
		return FSM_Continue;
	}

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		if (m_bResumePreviousAction || (!ActAsIfWeHavePavement()))
		{
			SetState(State_Quit);
		}
		else if (IsOnTrain())
		{
			SetState(State_Cower);
		}
		else
		{
  			SetState(State_WaitForPath);
		}
		return FSM_Continue;
	}

	return FSM_Continue;
}

void CTaskShockingEventHurryAway::StateWaitForPathOnEnter()
{
	CPed* pPed = GetPed();

	// Cache the event position
	const Vector3& vPosition = VEC3V_TO_VECTOR3(m_ShockingEvent->GetReactPosition());

	// Looks odd, but the task may have been restarted in this state.
	m_bEvadedRecentlyWhenTaskStarted |= CTimeHelpers::GetTimeSince(pPed->GetPedIntelligence()->GetLastTimeEvaded()) < sm_Tunables.m_EvasionThreshold;

	// Always run when the ped recently evaded a vehicle.
	float fInitialMBR = m_bEvadedRecentlyWhenTaskStarted ? MOVEBLENDRATIO_RUN : MOVEBLENDRATIO_STILL;

	// Create the navmesh task - start off with MBR of 0.0 because we don't want the ped to start moving until we know if they are going to back away or not.
	CNavParams navParams;
	navParams.m_fMoveBlendRatio = fInitialMBR;
	navParams.m_vTarget = vPosition;
	navParams.m_fTargetRadius = CTaskMoveFollowNavMesh::ms_fTargetRadius;
	navParams.m_fSlowDownDistance = CTaskMoveFollowNavMesh::ms_fTargetRadius;
	navParams.m_iWarpTimeMs = -1;
	navParams.m_bFleeFromTarget = true;
	
	CTaskMoveFollowNavMesh* pNavMeshTask = rage_new CTaskMoveFollowNavMesh(navParams);
	const dev_float fFleeForeverDist = 99999.0f;
	pNavMeshTask->SetFleeSafeDist(fFleeForeverDist);
	if (pPed && pPed->GetPedIntelligence() && pPed->GetPedIntelligence()->GetFleeBehaviour().GetFleeFlags().IsFlagSet(CFleeBehaviour::BF_PreferPavements))
	{
		pNavMeshTask->SetKeepToPavements(true);
		pNavMeshTask->SetConsiderRunStartForPathLookAhead(true);
	}

	pNavMeshTask->SetHighPrioRoute(true);

	// Create an ambient task

	CTask* pAmbientTask = NULL; 
	if (ShouldChangeMovementClip())
	{
		m_iConditionalAnimsChosen = DetermineConditionalAnimsForHurryAway(CONDITIONALANIMSMGR.GetConditionalAnimsGroup("SHOCK_HURRY"));
		pAmbientTask = rage_new CTaskAmbientClips(CTaskAmbientClips::Flag_PlayIdleClips | CTaskAmbientClips::Flag_PlayBaseClip, CONDITIONALANIMSMGR.GetConditionalAnimsGroup("SHOCK_HURRY"), m_iConditionalAnimsChosen);
	}

	// Create the control movement task and set it as the active sub task
	SetNewTask(rage_new CTaskComplexControlMovement(pNavMeshTask, pAmbientTask));
}

CTask::FSM_Return CTaskShockingEventHurryAway::StateWaitForPathOnUpdate()
{
	CPed* pPed = GetPed();
	const CEventShocking::Tunables& tunables = m_ShockingEvent->GetTunables();

	// Cast the sub task to a control movement task
	CTaskComplexControlMovement* pControlMovement = static_cast<CTaskComplexControlMovement*>(GetSubTask());

	// If the complex control movement task is null or has the wrong type, then restart.
	if (!pControlMovement || pControlMovement->GetMoveTaskType() != CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH)
	{
		SetFlag(aiTaskFlags::RestartCurrentState);
		return FSM_Continue;
	}
	// Check to see if the ped should back away-
	// Only if the check hasn't been made before, the event is reasonably close to a reaction axis, and the flee point is behind them.
	else
	{
		CTaskMoveFollowNavMesh* pTaskNavmesh = static_cast<CTaskMoveFollowNavMesh*>(pControlMovement->GetRunningMovementTask());
		if (pTaskNavmesh)
		{
			Vector3 vPrevWaypoint;
			Vector3 vCurrentWaypoint;
			if (pTaskNavmesh && pTaskNavmesh->GetPreviousWaypoint(vPrevWaypoint) && pTaskNavmesh->GetCurrentWaypoint(vCurrentWaypoint))
			{

				CTaskShockingEventBackAway* pBackAway = NULL;
				static const float sf_ForwardTolerance = 0.866f;
				SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
				Vec3V vDirToReactPos = Normalize(m_ShockingEvent->GetReactPosition() - pPed->GetTransform().GetPosition());

				float fChancePlayingBackAwayAnims = sm_Tunables.m_ChancePlayingCustomBackAwayAnimBigReact;
				if( m_ShockingEvent->GetTunables().m_ReactionMode == CEventShocking::SMALL_REACTION )
				{
					fChancePlayingBackAwayAnims = sm_Tunables.m_ChancePlayingCustomBackAwayAnimSmallReact;
				}

				// Gang guys don't play the shocking anims - nor do peds who recently evaded a vehicle.
				bool bPlayBackAwayAnims = !m_bEvadedRecentlyWhenTaskStarted && (fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < fChancePlayingBackAwayAnims && !pPed->IsFatigued() && !pPed->IsGangPed());

				// Check to make sure that the event is ahead of the ped.
				if (bPlayBackAwayAnims && IsGreaterThanAll(Dot(vDirToReactPos, pPed->GetTransform().GetForward()), ScalarV(sf_ForwardTolerance)))
				{
					if (CTaskShockingEventBackAway::ShouldBackAwayTowardsRoutePosition(pPed->GetTransform().GetPosition(), VECTOR3_TO_VEC3V(vPrevWaypoint), VECTOR3_TO_VEC3V(vCurrentWaypoint)))
					{
						pBackAway = rage_new CTaskShockingEventBackAway(m_ShockingEvent, VECTOR3_TO_VEC3V(vCurrentWaypoint), false);
						pBackAway->SetRouteWaypoints(vPrevWaypoint, vCurrentWaypoint);
					}
				}

				if (pBackAway)
				{
					// Backing away towards the target point.
					pBackAway->SetShouldPlayShockingSpeech(false);
					pControlMovement->SetNewMainSubtask(pBackAway);
					SetState(State_BackAway);
				}
				else
				{
					// The evasion case already has a move blend ratio set.
					if (!m_bEvadedRecentlyWhenTaskStarted)
					{
						pControlMovement->SetMoveBlendRatio(pPed, tunables.m_HurryAwayInitialMBR);
					}

					SetState(State_HurryAway);
				}
			}
			else
			{
				// Peds who evaded recently are running and doing so with KeepMovingWhilstWaitingForPath, so we don't want to lock in the ped's heading below.
				if (!m_bEvadedRecentlyWhenTaskStarted)
				{
					pPed->SetPedResetFlag(CPED_RESET_FLAG_DisableMoveTaskHeadingAdjustments, true);
				}
			}
		}
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskShockingEventHurryAway::StateBackAwayOnUpdate()
{
	CTaskComplexControlMovement* pControlMovement = static_cast<CTaskComplexControlMovement*>(GetSubTask());
	CPed* pPed = GetPed();
	const CEventShocking::Tunables& tunables = m_ShockingEvent->GetTunables();

	if (!pControlMovement)
	{
		return FSM_Quit;
	}
	else if (pControlMovement->GetMainSubTaskType() == CTaskTypes::TASK_NONE)
	{
		if (ShouldChangeMovementClip())
		{
			CTask* pAmbientTask = rage_new CTaskAmbientClips(CTaskAmbientClips::Flag_PlayIdleClips | CTaskAmbientClips::Flag_PlayBaseClip, CONDITIONALANIMSMGR.GetConditionalAnimsGroup("SHOCK_HURRY"), m_iConditionalAnimsChosen);
			pControlMovement->SetNewMainSubtask(pAmbientTask);
		}
		pControlMovement->SetMoveBlendRatio(pPed, tunables.m_HurryAwayInitialMBR);
		SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
		SetState(State_HurryAway);
	}
	return FSM_Continue;
}

void CTaskShockingEventHurryAway::StateHurryAwayOnEnter()
{
	//Check if we can call the police.
	m_bCallPolice = false;
	CPed* pPedToCallPoliceOn = GetPedToCallPoliceOn();
	if(!m_bHasCalledPolice && m_ShockingEvent->GetTunables().m_CanCallPolice && pPedToCallPoliceOn && CTaskCallPolice::CanMakeCall(*GetPed(), pPedToCallPoliceOn))
	{
		CWanted* pPlayerWanted = pPedToCallPoliceOn->GetPlayerWanted();
		if(!pPlayerWanted || pPlayerWanted->GetWantedLevel() > WANTED_CLEAN || m_ShockingEvent->GetStartTime() > pPlayerWanted->GetTimeWantedLevelCleared())
		{
			CEntity* pVictim = m_ShockingEvent->GetOtherEntity();
			if(!pVictim || !pVictim->GetIsTypePed() || !static_cast<CPed*>(pVictim)->GetPedConfigFlag(CPED_CONFIG_FLAG_DontInfluenceWantedLevel))
			{
				//Check if we should call the police.
				float fRandom = fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
				m_bCallPolice = (fRandom < sm_Tunables.m_ChancesToCallPolice);
				if(m_bCallPolice)
				{
					m_fTimeToCallPolice = fwRandom::GetRandomNumberInRange(sm_Tunables.m_MinTimeToCallPolice, sm_Tunables.m_MaxTimeToCallPolice);
				}
			}
		}
	}

	//Set the crimes as witnessed.
	SetCrimesAsWitnessed();

	//Config timer
	if (m_fHurryAwayMaxTime >= 0.0f)
	{
		m_HurryAwayTimer.Set(fwTimer::GetTimeInMilliseconds(), static_cast<u32>(m_fHurryAwayMaxTime * 1000.0f));
	}
	else
	{
		m_HurryAwayTimer.Unset();
	}
}

CTaskShockingEventHurryAway::FSM_Return CTaskShockingEventHurryAway::StateHurryAwayOnUpdate()
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished) || m_HurryAwayTimer.IsOutOfTime())
	{
		// Finished
		return FSM_Quit;
	}

	if(CheckForCower())
	{
		SetState(State_Cower);
		return FSM_Continue;
	}

	if (ShouldUpdateTargetPosition())
	{
		// Update the target for the navmesh subtask.
		UpdateNavMeshTargeting();
	}

	// Update whether or not this ped should play idle clips.
	UpdateAmbientClips();

	// Possibly speed up the movement rate based on proximity to the threat and the player.
	UpdateHurryAwayMBR();

	// Decide if the ped should call the police while hurrying away.
	PossiblyCallPolice();

	// Delete hurrying away peds earlier than normal.
	PossiblyDestroyHurryingAwayPed();

	return FSM_Continue;
}

void CTaskShockingEventHurryAway::StateCowerOnEnter()
{
	CTask* pTask = rage_new CTaskCower(-1);
	SetNewTask(pTask);
}

CTaskShockingEventHurryAway::FSM_Return CTaskShockingEventHurryAway::StateCowerOnUpdate()
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetFlag(aiTaskFlags::RestartCurrentState);
		return FSM_Continue;
	}

	return FSM_Continue;
}

s32 CTaskShockingEventHurryAway::DetermineConditionalAnimsForHurryAway(const CConditionalAnimsGroup* pGroup)
{
	if (!Verifyf(pGroup, "SHOCK_HURRY was undefined."))
	{
		return -1;
	}

	CScenarioCondition::sScenarioConditionData conditionData;
	conditionData.pPed = GetPed();
	u32 iNumAnims = pGroup->GetNumAnims();

	// Flip through all the condition animations, picking the least recently used.
	for (int i=0; i < iNumAnims; i++)
	{
		s32 iIndex = sm_iLastChosenAnim + i + 1;
		if (iIndex >= iNumAnims)
		{
			iIndex -= iNumAnims;
		}
		const CConditionalAnims* pAnims = pGroup->GetAnims(iIndex);
		if (pAnims->CheckConditions(conditionData, NULL))
		{
			sm_iLastChosenAnim = iIndex;
			return iIndex;
		}
	}

	return -1;
}

void CTaskShockingEventHurryAway::UpdateNavMeshTargeting()
{
	// Cast the sub task to a control movement task
	Assert(GetSubTask());
	Assert(GetSubTask()->GetTaskType() == CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT);
	CTaskComplexControlMovement* pControlMovement = static_cast<CTaskComplexControlMovement*>(GetSubTask());

	CPed* pPed = GetPed();

	// Calculate a vector from the hurrying away ped to the shocking event's reaction position.
	Vector3 vPosition = VEC3V_TO_VECTOR3(m_ShockingEvent->GetReactPosition());
	const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	Vector3 vDirToThreat = vPosition - vPedPosition;
	vDirToThreat.z = 0.0f;
	vDirToThreat.NormalizeSafe();

	// Compute the dot product with the hurrying away ped's forward vector.
	float fEventDot = DotProduct(vDirToThreat, VEC3V_TO_VECTOR3(pPed->GetTransform().GetB()));
	static const float sf_ForwardTolerance = 0.707f;

	// Check if the event is in front of the ped, if so adjust the position.
	if (fEventDot > sf_ForwardTolerance)
	{
		pControlMovement->SetTarget(pPed, vPosition);
	}

	// Always keep moving whilst waiting for a path.
	aiTask* pTask = pControlMovement->GetRunningMovementTask(pPed);
	if (pTask && pTask->GetTaskType() == CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH)
	{
		static_cast<CTaskMoveFollowNavMesh*>(pTask)->SetKeepMovingWhilstWaitingForPath(true);
	}
}

void CTaskShockingEventHurryAway::UpdateAmbientClips()
{
	// Weren't allowed to set the clip when this task started.
	if (!ShouldChangeMovementClip())
	{
		return;
	}

	// Compute a vector from the hurrying away ped to the shocking event's reaction position.
	Vector3 vPosition = VEC3V_TO_VECTOR3(m_ShockingEvent->GetReactPosition());
	const Vector3 vPedPosition = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition());
	Vector3 vDirToThreat = vPosition - vPedPosition;
	vDirToThreat.z = 0.0f;
	vDirToThreat.NormalizeSafe();

	// Compute the dot product with the hurrying away ped's forward vector.
	float fEventDot = DotProduct(vDirToThreat, VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetB()));
	static const float sf_ForwardTolerance = 0.707f;	

	Assert(GetSubTask());
	if(GetSubTask()->GetSubTask() && GetSubTask()->GetSubTask()->GetTaskType() == CTaskTypes::TASK_AMBIENT_CLIPS)
	{
		CTaskAmbientClips* pAmbientSubtask = static_cast<CTaskAmbientClips*>(GetSubTask()->GetSubTask());
		// If the source entity isn't behind the ped don't play clips as they all consist of looking backward
		if (fEventDot < 0.0f && fabs(fEventDot) > sf_ForwardTolerance)
		{
			pAmbientSubtask->GetFlags().ClearFlag(CTaskAmbientClips::Flag_PlayIdleClips);
			LookAtEvent();
		}
		else
		{
			pAmbientSubtask->GetFlags().SetFlag(CTaskAmbientClips::Flag_PlayIdleClips);
		}
	}
}

void CTaskShockingEventHurryAway::UpdateHurryAwayMBR()
{
	// Cast the sub task to a control movement task
	Assert(GetSubTask());
	Assert(GetSubTask()->GetTaskType() == CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT);
	CTaskComplexControlMovement* pControlMovement = static_cast<CTaskComplexControlMovement*>(GetSubTask());

	const CEventShocking::Tunables& tunables = m_ShockingEvent->GetTunables();
	CPed* pPed = GetPed();

	// Update the control movement tasks move speed.
	if(GetTimeInState() > tunables.m_HurryAwayMBRChangeDelay)
	{
		Vector3 vPosition = VEC3V_TO_VECTOR3(m_ShockingEvent->GetReactPosition());
		const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

		float fCurrentMBR = pControlMovement->GetMoveBlendRatio();
		float fNewMBR;

		bool bSlowingBecauseOfPavement = false;

		if (m_bEvadedRecentlyWhenTaskStarted)
		{
			fNewMBR = MOVEBLENDRATIO_RUN;
		}
		else if (vPosition.Dist2(vPedPosition) < rage::square(tunables.m_HurryAwayMBRChangeRange))
		{
			fNewMBR = tunables.m_HurryAwayMoveBlendRatioWhenNear;
		}
		else if (m_bZoneRequiresPavement && m_bForcePavementNotRequired && (!pPed->GetNavMeshTracker().GetIsValid() || !pPed->GetNavMeshTracker().GetNavPolyData().m_bPavement))
		{
			// Even if pavement was forced as not required, if the ped doesn't have any then their MBR needs to be fast in zones tagged as requiring pavement.
			fNewMBR = MOVEBLENDRATIO_RUN;
		}
		else if (ActAsIfWeHavePavement() || (pPed->GetNavMeshTracker().GetIsValid() && pPed->GetNavMeshTracker().GetNavPolyData().m_bPavement) )
		{
			fNewMBR = tunables.m_HurryAwayMoveBlendRatioWhenFar;
			bSlowingBecauseOfPavement = true;
		}
		else 
		{
			fNewMBR = MOVEBLENDRATIO_RUN;
		}

		if (ShouldSpeedUpBecauseOfLocalPlayer())
		{
			fNewMBR = MOVEBLENDRATIO_RUN;
		}

		// Only speed up peds, never slow them back down.
		if (fNewMBR > fCurrentMBR || bSlowingBecauseOfPavement)
		{
			pControlMovement->SetMoveBlendRatio(pPed, fNewMBR);
		}
	}
}

bool CTaskShockingEventHurryAway::ShouldSpeedUpBecauseOfLocalPlayer()
{
	CPed* pPlayer = CGameWorld::FindLocalPlayer();

	// Validate the player.
	if (!pPlayer)
	{
		return false;
	}

	// Check if the player is close.
	Vec3V vPed = GetPed()->GetTransform().GetPosition();
	Vec3V vPlayer = pPlayer->GetTransform().GetPosition();

	if (IsGreaterThanAll(DistSquared(vPed, vPlayer), ScalarV(sm_Tunables.m_ClosePlayerSpeedupDistanceSquaredThreshold)))
	{
		// Reset the timer.
		m_fClosePlayerCounter = 0.0f;
		return false;
	}

	// Update the timer.
	m_fClosePlayerCounter += GetTimeStep();

	// Check if we have been close enough to the local player to do a speedup.
	if (m_fClosePlayerCounter < sm_Tunables.m_ClosePlayerSpeedupTimeThreshold)
	{
		return false;
	}

	return true;
}

bool CTaskShockingEventHurryAway::ShouldUpdateTargetPosition()
{
	// For all events other than peds being run over.
	if (m_ShockingEvent->GetEventType() != EVENT_SHOCKING_PED_RUN_OVER)
	{
		return true;
	}

	// Only update the reaction position if the driver is a player.
	// This is because NPCs reacting to NPC drivers can look confusing at times.
	CPed* pDriver = m_ShockingEvent->GetSourcePed();
	if (pDriver && pDriver->IsAPlayerPed())
	{
		return true;
	}

	return false;
}


void CTaskShockingEventHurryAway::PossiblyCallPolice()
{
	// Cast the sub task to a control movement task
	Assert(GetSubTask());
	Assert(GetSubTask()->GetTaskType() == CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT);
	CTaskComplexControlMovement* pControlMovement = static_cast<CTaskComplexControlMovement*>(GetSubTask());

	//Check if we should call the police.
	if(m_bCallPolice && (m_fTimeToCallPolice > GetTimeInState()))
	{
		//Call the police.
		m_bCallPolice = false;
		m_bHasCalledPolice = true;
		pControlMovement->SetNewMainSubtask(rage_new CTaskCallPolice(GetPedToCallPoliceOn()));
	}
}

void CTaskShockingEventHurryAway::PossiblyDestroyHurryingAwayPed()
{
	CPed* pPed = GetPed();

	// Check if the ped can be deleted
	const bool bDeletePedIfInCar = false; // prevent deletion in cars
	const bool bDoNetworkChecks = true; // do network checks (this is the default)
	if (pPed->CanBeDeleted(bDeletePedIfInCar, bDoNetworkChecks))
	{
		// Get handle to local player ped
		const CPed* pLocalPlayerPed = CGameWorld::FindLocalPlayer();
		if (pLocalPlayerPed)
		{
			// Check distance to local player is beyond minimum threshold
			const ScalarV distToLocalPlayerSq_MIN = ScalarVFromF32(rage::square(sm_Tunables.m_MinDistanceFromPlayerToDeleteHurriedPed));
			const ScalarV distToLocalPlayerSq = DistSquared(pLocalPlayerPed->GetTransform().GetPosition(), pPed->GetTransform().GetPosition());
			CPed* pAmbientFriend = pPed->GetPedIntelligence()->GetAmbientFriend();
			if (IsGreaterThanAll(distToLocalPlayerSq, distToLocalPlayerSq_MIN) && 
				(!pAmbientFriend || IsGreaterThanAll(DistSquared(pLocalPlayerPed->GetTransform().GetPosition(), pAmbientFriend->GetTransform().GetPosition()), distToLocalPlayerSq_MIN)))
			{
				// Check if ped is offscreen
				const bool bCheckNetworkPlayersScreens = true;
				if (!pPed->GetIsOnScreen(bCheckNetworkPlayersScreens) 
					&& (!pAmbientFriend || !pAmbientFriend->GetIsOnScreen(bCheckNetworkPlayersScreens)))
				{
					m_fTimeOffScreenWhenHurrying += GetTimeStep();

					if (m_fTimeOffScreenWhenHurrying >= sm_Tunables.m_TimeUntilDeletionWhenHurrying)
					{
						pPed->FlagToDestroyWhenNextProcessed();

						if (pAmbientFriend)
						{
							pAmbientFriend->FlagToDestroyWhenNextProcessed();
						}
					}
				}
				else
				{
					// Ped is back onscreen.  Reset the timer.
					m_fTimeOffScreenWhenHurrying = 0.0f;
				}
			}
			else
			{
				// Ped is too close.  Reset the timer.
				m_fTimeOffScreenWhenHurrying = 0.0f;
			}
		}
	}
	else
	{
		// Ped can not be deleted.  Reset the timer.
		m_fTimeOffScreenWhenHurrying = 0.0f;
	}
}

CPed* CTaskShockingEventHurryAway::GetPedToCallPoliceOn() const
{
	//Check if the player criminal is valid.
	CPed* pPed = GetPlayerCriminal();
	if(pPed)
	{
		return pPed;
	}

	//Check if the source ped is valid.
	pPed = GetSourcePed();
	if(pPed)
	{
		return pPed;
	}

	return NULL;
}

u32 CTaskShockingEventHurryAway::GetRandomFilmTimeOverrideMS() const
{
	const CEventShocking::Tunables& tunables = m_ShockingEvent->GetTunables();
	const float minWatchTime = tunables.m_MinPhoneFilmTimeHurryAway;
	const float maxWatchTime = tunables.m_MaxPhoneFilmTimeHurryAway;
	float timeSeconds = fwRandom::GetRandomNumberInRange(minWatchTime, maxWatchTime);
	return (u32)(timeSeconds*1000.0f);
}

void CTaskShockingEventHurryAway::AnalyzePavementSituation()
{
	Vector3 vPedPosition = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition());
	CPopZone* pPopZone = CPopZones::FindSmallestForPosition(&vPedPosition, ZONECAT_ANY, ZONETYPE_ANY);
	if (pPopZone)
	{
		// Ask the pop zone if peds should look for pavement before they do a hurry away.
		m_bZoneRequiresPavement = !pPopZone->m_bLetHurryAwayLeavePavements;
	}
	else
	{
		// If we fail to find a popzone, force pavement.
		m_bZoneRequiresPavement = true;
	}

	// Check debug overrides.
#if __DEV
	TUNE_GROUP_BOOL(TASK_SHOCKING_EVENT_HURRY_AWAY, bForcePavementNotRequired, false);
	if(bForcePavementNotRequired)
	{
		m_bCaresAboutPavement = false;
		return;
	}
#endif

	// Check parameter overrides set by the creator of the task.
	if(m_bForcePavementNotRequired)
	{
		m_bCaresAboutPavement = false;
		return;
	}

	// Check for the event to override the default behaviour of resuming if not on pavement.
	if (m_ShockingEvent->GetTunables().m_IgnorePavementChecks)
	{
		m_bForcePavementNotRequired = true;
		m_bCaresAboutPavement = false;
		return;
	}

	// Lastly, check the zone.
	m_bCaresAboutPavement = m_bZoneRequiresPavement;
}

bool CTaskShockingEventHurryAway::ShouldFleeFromShockingEvent()
{
	if (m_bFleeing)
	{
		return true;
	}

	if (ActAsIfWeHavePavement())
	{
		if ( m_ShockingEvent->GetTunables().m_FleeIfApproachedByOtherEntity && m_ShockingEvent->GetOtherEntity() ) 
		{
			if (ShouldFleeFromEntity(m_ShockingEvent->GetOtherEntity()))
			{
				// We used to give the ped CTaskSmartFlee here.
				// However, CTaskShockingeventReact is only ever used as a subtask of HurryAway, so quitting out will be sufficient if someone comes close
				// as that will just trigger the follow navmesh part of hurry away.
				m_bFleeing = true;
				return true;
			}
		}

		if ( m_ShockingEvent->GetTunables().m_FleeIfApproachedBySourceEntity && m_ShockingEvent->GetSourceEntity() )
		{
			if (ShouldFleeFromEntity(m_ShockingEvent->GetSourceEntity()))
			{
				// We used to give the ped CTaskSmartFlee here.
				// However, CTaskShockingeventReact is only ever used as a subtask of HurryAway, so quitting out will be sufficient if someone comes close
				// as that will just trigger the follow navmesh part of hurry away.
				m_bFleeing = true;
				return true;
			}
		}
	}

	return false;
}

bool CTaskShockingEventHurryAway::ShouldFleeFromEntity(const CEntity* pEntity) const
{
	if (aiVerify(pEntity))
	{
		float fDistanceCheck = sm_Tunables.m_ShouldFleeDistance;
		if (pEntity->GetIsTypeVehicle())
		{
			fDistanceCheck = sm_Tunables.m_ShouldFleeVehicleDistance;
		}
		else if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_SHOCKING_EVENT_WATCH)
		{
			const CTaskShockingEventWatch* pShockingEventWatch = static_cast<const CTaskShockingEventWatch*>(GetSubTask());
			if (pShockingEventWatch->IsFilming())
			{
				fDistanceCheck = sm_Tunables.m_ShouldFleeFilmingDistance;
			}
		}

		ScalarV vDistToInstigatorSq = DistSquared(GetPed()->GetTransform().GetPosition(), pEntity->GetTransform().GetPosition());
		ScalarV vDistCheck(rage::square(fDistanceCheck));
		if ( IsLessThanAll(vDistToInstigatorSq, vDistCheck) )
		{
			return true;
		}
	}
	return false;
}

bool CTaskShockingEventHurryAway::CheckIfAllowedToChangeMovementClip() const
{
	// Validate the event.
	if (!m_ShockingEvent)
	{
		return false;
	}

	// Check if the ped is on screen.
	if (GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_VisibleOnScreen))
	{
		return true;
	}

	// Check if the event involved the player.
	if (CEventShocking::DoesEventInvolvePlayer(m_ShockingEvent->GetSourceEntity(), m_ShockingEvent->GetOtherEntity()))
	{
		return true;
	}

	// Check the type.
	switch(m_ShockingEvent->GetEventType())
	{
		case EVENT_SHOCKING_CAR_CRASH:
		case EVENT_SHOCKING_CAR_PILE_UP:
		{
			return false;
		}
		default:
		{
			return true;
		}
	}
}

bool CTaskShockingEventHurryAway::IsOnTrain() const
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

bool CTaskShockingEventHurryAway::CheckForCower()
{
	//Get the active movement task.
	CTask* pActiveMovementTask = GetPed()->GetPedIntelligence()->GetActiveMovementTask();
	if(!pActiveMovementTask)
	{
		return false;
	}

	//Ensure the task is the right type.
	if(pActiveMovementTask->GetTaskType() != CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH)
	{
		return false;
	}

	//Ensure the task has not finished or been aborted.
	if(pActiveMovementTask->GetIsFlagSet(aiTaskFlags::HasFinished) || pActiveMovementTask->GetIsFlagSet(aiTaskFlags::IsAborted))
	{
		return false;
	}

	//Grab the task.
	CTaskMoveFollowNavMesh* pTask = static_cast<CTaskMoveFollowNavMesh *>(pActiveMovementTask);

	//Check if we were unable to find a route.
	if(pTask->IsUnableToFindRoute())
	{
		return true;
	}
	//Check if we are following the route.
	else if(pTask->IsFollowingNavMeshRoute())
	{
		//Check if the route is very short.
		if(!m_bDisableShortRouteCheck)
		{
			if(!pTask->IsTotalRouteLongerThan(3.0f))
			{
				return true;
			}
		}

		//Only do this check once, since it's pretty expensive.
		m_bDisableShortRouteCheck = true;
	}

	return false;
}


//-------------------------------------------------------------------------
// CTaskShockingEventWatch
//-------------------------------------------------------------------------

CTaskShockingEventWatch::Tunables CTaskShockingEventWatch::sm_Tunables;

IMPLEMENT_RESPONSE_TASK_TUNABLES(CTaskShockingEventWatch, 0x6b36d3c1);

const fwMvClipSetId CTaskShockingEventWatch::sm_WatchClipSet("move_m@shocked@a", 0xCC071B7F);

u32 CTaskShockingEventWatch::sm_NextTimeAnyoneCanStopWatching = 0;


CTaskShockingEventWatch::CTaskShockingEventWatch(const CEventShocking* event, u32 uiWatchTime, u32 uPhoneFilmTimeOverride, bool bResumePreviousAction, bool watchForever)
		: CTaskShockingEvent(event)
		, m_RandomWatchTime(uiWatchTime)
		, m_PhoneFilmTimeOverride(uPhoneFilmTimeOverride)
		, m_WatchTimeStarted(false)
		, m_bWatchForever(watchForever)
		, m_bFilming(false)
		, m_bResumePreviousAction(bResumePreviousAction)
{
	SetInternalTaskType(CTaskTypes::TASK_SHOCKING_EVENT_WATCH);
}

aiTask* CTaskShockingEventWatch::Copy() const
{
	CTaskShockingEventWatch* pWatch = rage_new CTaskShockingEventWatch(m_ShockingEvent, m_RandomWatchTime, m_PhoneFilmTimeOverride, m_bResumePreviousAction, m_bWatchForever);
	if (pWatch)
	{
		pWatch->SetShouldPlayShockingSpeech(ShouldPlayShockingSpeech());
	}
	return pWatch;
}

void CTaskShockingEventWatch::FleeFromEntity()
{
	if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_USE_SCENARIO)
	{
		CTaskUseScenario* pTask = static_cast<CTaskUseScenario*>(GetSubTask());
		pTask->TriggerImmediateExitOnEvent();
	}
	RequestTermination();
}

CTask::FSM_Return CTaskShockingEventWatch::ProcessPreFSM()
{
	//Grab the ped.
	CPed* pPed = GetPed();

	pPed->GetPedIntelligence()->SetCheckShockFlag(true);

	// Allow certain lod modes
	pPed->GetPedAiLod().ClearBlockedLodFlag(CPedAILod::AL_LodPhysics);
	pPed->GetPedAiLod().ClearBlockedLodFlag(CPedAILod::AL_LodMotionTask);

	return CTaskShockingEvent::ProcessPreFSM();
}

CTaskShockingEventWatch::FSM_Return CTaskShockingEventWatch::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		FSM_State(State_Start)
			FSM_OnUpdate
				return StateStartOnUpdate();
		FSM_State(State_Facing)
			FSM_OnEnter
				return StateFacingOnEnter();
			FSM_OnUpdate
				return StateFacingOnUpdate();
		FSM_State(State_Watching)
			FSM_OnEnter
				return StateWatchingOnEnter();
			FSM_OnUpdate
				return StateWatchingOnUpdate();
		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

#if !__FINAL

const char * CTaskShockingEventWatch::GetStaticStateName( s32 iState )
{
	static const char* aStateNames[] = 
	{
		"Start",
		"Facing",
		"Watching",
		"Quit"
	};
	CompileTimeAssert(NELEM(aStateNames) == kNumStates);

	if(iState >= 0 && iState < kNumStates)
	{
		return aStateNames[iState];
	}
	else
	{
		return "Invalid";
	}
}

#endif // !__FINAL

CTaskShockingEventWatch::FSM_Return CTaskShockingEventWatch::StateStartOnUpdate()
{
	CPed* pPed = GetPed();

	const CEventShocking::Tunables& tunables = m_ShockingEvent->GetTunables();
	if(tunables.m_WatchSayShocked && CanWePlayShockingSpeech())
	{
		if(!SayShockingSpeech())
		{
			float fRandom = fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
			if(fRandom < 0.4f)
			{
				pPed->NewSay("SHOCKED");
			}
			else if(fRandom < 0.7f)
			{
				pPed->NewSay("SURPRISED");
			}
			else
			{
				pPed->NewSay("SHIT");
			}
		}
	}

	if(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
	{
		// Quit
		return FSM_Quit;
	}

	if (pPed->IsMale())
	{
		// Change the default movement clipset to be the shock anims.
		CTaskMotionBase* pMotionTask = pPed->GetPrimaryMotionTaskIfExists();
		if (pMotionTask)
		{
			pMotionTask->SetOnFootClipSet(sm_WatchClipSet);
		}
	}

	// Check if we're already facing the target fairly well, if so, go straight
	// to the watch scenario.
	if(IsFacingTarget(sm_Tunables.m_ThresholdWatchAfterFace))
	{
		SetState(State_Watching);
	}
	else
	{
		SetState(State_Facing);
	}

	return FSM_Continue;
}


CTaskShockingEventWatch::FSM_Return CTaskShockingEventWatch::StateFacingOnEnter()
{
	// Turn to face event
	const CEntity* pTgt = m_ShockingEvent->GetSourceEntity();
	CTaskTurnToFaceEntityOrCoord* pFaceTask;
	if(pTgt && m_ShockingEvent->GetEventType() != EVENT_SHOCKING_BROKEN_GLASS) // HACK JR: we need the ped to react to the position of the broken glass, but I don't want to change the general behaviour for all shocking events
	{
		pFaceTask = rage_new CTaskTurnToFaceEntityOrCoord(pTgt);
	}
	else
	{
		pFaceTask = rage_new CTaskTurnToFaceEntityOrCoord(VEC3V_TO_VECTOR3(m_ShockingEvent->GetReactPosition()));
	}
	SetNewTask(pFaceTask);

	return FSM_Continue;
}


CTaskShockingEventWatch::FSM_Return CTaskShockingEventWatch::StateFacingOnUpdate()
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		if(IsFacingTarget(sm_Tunables.m_ThresholdWatchAfterFace))
		{
			if(!IsTargetMovingAroundUs(sm_Tunables.m_MaxTargetAngularMovementForWatch))
			{
				SetState(State_Watching);
				return FSM_Continue;
			}
			// If the target is still moving fast enough, we will also restart the
			// face task again.
		}

		SetFlag(aiTaskFlags::RestartCurrentState);
		return FSM_Continue;
	}

	if(!UpdateWatchTime())
	{
		SetLastTimeOfWatchExit();

		return FSM_Quit;
	}

	UpdateHeadTrack();

	return FSM_Continue;
}


CTaskShockingEventWatch::FSM_Return CTaskShockingEventWatch::StateWatchingOnEnter()
{
	// If watch time is zero, randomise it
	const bool bUseHurryAwayTimes = !m_bResumePreviousAction;
	if(m_RandomWatchTime == 0)
	{
		m_RandomWatchTime = GetRandomWatchTimeMS(*m_ShockingEvent, bUseHurryAwayTimes);
	}

	// Check to see if you want to film this event on your mobile phone.
	if ( fwRandom::GetRandomNumberInRange(0.f, 1.f) < m_ShockingEvent->GetTunables().m_ChanceOfFilmingEventOnPhoneIfWatching )
	{
		// Check to see if we're too close to film or too close to other peds filming a shocking event
		if (!IsTooCloseToFilm() && !IsTooCloseToOtherPeds())
		{
			s32 iScenarioType = CScenarioManager::GetScenarioTypeFromHashKey(ATSTRINGHASH("WORLD_HUMAN_MOBILE_FILM_SHOCKING", 0xA0749BB6));
			if (taskVerifyf(iScenarioType >= 0, "Didn't find WORLD_HUMAN_MOBILE_FILM_SHOCKING scenario type"))
			{
				// Check to see if this ped model is blocked from doing this scenario
				const CScenarioInfo* pScenarioInfo = CScenarioManager::GetScenarioInfo(iScenarioType);
				const u32 uPedModelHash = GetPed()->GetBaseModelInfo()->GetHashKey();

				if (!pScenarioInfo->GetBlockedModelSet() || !pScenarioInfo->GetBlockedModelSet()->GetContainsModel(uPedModelHash))
				{
					CScenarioCondition::sScenarioConditionData conditions;
					conditions.pPed = GetPed();

					const CConditionalAnimsGroup* pConditionalAnimsGroup = CScenarioManager::GetConditionalAnimsGroupForScenario(iScenarioType);

					if (pConditionalAnimsGroup)
					{
						// Check the conditions on the animation
						if (pConditionalAnimsGroup->CheckForMatchingConditionalAnims(conditions))
						{
							CTaskUseScenario* pUseScenarioTask = rage_new CTaskUseScenario(iScenarioType);
							SetNewTask(pUseScenarioTask);
							m_bFilming = true;

							SayFilmShockingSpeech();

							// Set the watch time to the override passed in if set, or our own random value if 0
							if (m_PhoneFilmTimeOverride == 0)
							{
								m_PhoneFilmTimeOverride = GetRandomPhoneFilmTimeMS(*m_ShockingEvent, bUseHurryAwayTimes);
							}
							m_RandomWatchTime = m_PhoneFilmTimeOverride;
						}
					}
				}
			}
		}
	}

	// Now when we're facing the target, start the timer if it's not been started already.
	m_WatchTimeStarted = true;

	return FSM_Continue;
}

CTaskShockingEventWatch::FSM_Return CTaskShockingEventWatch::StateWatchingOnUpdate()
{
	CPed* pPed = GetPed();

	if(!IsFacingTarget(sm_Tunables.m_ThresholdWatchStop))
	{
		SetState(State_Facing);
		return FSM_Continue;
	}

	if(!UpdateWatchTime() || (m_bFilming && IsTooCloseToPlayerToFilm()) )
	{
		if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_USE_SCENARIO)
		{
			CTaskUseScenario* pScenarioSubTask = static_cast<CTaskUseScenario*>(GetSubTask());
			if (!pScenarioSubTask->HasTriggeredExit())
			{
				pScenarioSubTask->TriggerNormalExit();
			}
			m_bFilming = false;
		}
		else 
		{
			SetLastTimeOfWatchExit();

			// Don't quit until the SubTask has exited.
			return FSM_Quit;
		}
	}

	if (m_Flags.IsFlagSet(aiTaskFlags::TerminationRequested))
	{
		if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_USE_SCENARIO)
		{
			CTaskUseScenario* pScenarioSubTask = static_cast<CTaskUseScenario*>(GetSubTask());
			if (!pScenarioSubTask->HasTriggeredExit())
			{
				pScenarioSubTask->TriggerNormalExit();
			}
			m_bFilming = false;
			// Don't quit while the subtask is still running.
			return FSM_Continue;
		}
		else
		{
			return FSM_Quit;
		}
	}

	UpdateHeadTrack();

	// Say audio
	const CEventShocking::Tunables& tunables = m_ShockingEvent->GetTunables();
	if(tunables.m_WatchSayFightCheers && CanWePlayShockingSpeech())
	{
		if(pPed->IsGangPed())
		{
			pPed->RandomlySay("GANG_FIGHT_CHEER");
		}
		else
		{
			pPed->RandomlySay("FIGHT_CHEER");
		}
	}
	else if(tunables.m_WatchSayShocked && CanWePlayShockingSpeech())
	{
		if(fwRandom::GetRandomTrueFalse())
		{
			pPed->RandomlySay("SURPRISED", 0.05f);
		}
		else
		{
			pPed->RandomlySay("SHOCKED", 0.05f);
		}
	}

	return FSM_Continue;
}


bool CTaskShockingEventWatch::IsFacingTarget(float cosThreshold) const
{
	const CPed* pPed = GetPed();

	if(m_ShockingEvent && pPed)
	{
		Vector3 vPosition = VEC3V_TO_VECTOR3(m_ShockingEvent->GetReactPosition());

		// Construct a vector from the ped to the event
		Vector3 vToTarget = vPosition - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		vToTarget.z = 0.0f;
		vToTarget.Normalize();

		if(DotProduct(vToTarget, VEC3V_TO_VECTOR3(pPed->GetTransform().GetB())) > cosThreshold)
		{
			return true;
		}

	}
	return false;
}


bool CTaskShockingEventWatch::IsTargetMovingAroundUs(const float &thresholdRadiansPerSecond) const
{
	const Vec3V ourPosV = GetPed()->GetTransform().GetPosition();

#if 0	// Technically should probably do this, but doesn't look like we respect this anyway, when running the face task.
	if(m_ShockingEvent->GetUseSourcePosition())
	{
		return false;
	}
#endif

	// Check if we have a source entity, which is physical (otherwise, we can't get its
	// velocity through CPhysical::GetVelocity()).
	const CEntity* pSourceEntity = m_ShockingEvent->GetSourceEntity();
	if(pSourceEntity && pSourceEntity->GetIsPhysical())
	{
		const CPhysical* pSourcePhysical = static_cast<const CPhysical*>(pSourceEntity);

		// Get the position, velocity, and threshold.
		const Vec3V sourcePosV = pSourcePhysical->GetTransform().GetPosition();
		const Vec3V sourceVelV = VECTOR3_TO_VEC3V(pSourcePhysical->GetVelocity());
		const ScalarV thresholdV = LoadScalar32IntoScalarV(thresholdRadiansPerSecond);

		// Compute vector to the source.
		const Vec3V toSourceV = Subtract(sourcePosV, ourPosV);

		// Compute squared distance to the source.
		const ScalarV distSquaredV = MagSquared(toSourceV);

		// We want to project the source velocity onto a vector that's perpendicular
		// to the vector to the source. First, we create a vector with X and Y swapped.
		// Z doesn't matter.
		const Vec3V toSourceFlippedV = toSourceV.Get<Vec::Y, Vec::X, Vec::Z>();

		// Multiply the components of the XY-flipped vector with the velocity vector.
		const Vec3V crossMulV = Scale(toSourceFlippedV, sourceVelV);

		// Next, we compute the magnitude of the dot product between the velocity
		// and the perpendicular vector. This is the perpendicular speed, times the
		// distance to the source.	
		const ScalarV crossXV = crossMulV.GetX();
		const ScalarV negCrossYV = crossMulV.GetY();
		const ScalarV crossV = Subtract(crossXV, negCrossYV);
		const ScalarV crossAbsV = Abs(crossV);

		// Normally, we would divide twice by the distance here, once just to get to the
		// perpendicular speed, and another to turn it into an angular speed. Instead,
		// we multiply the threshold by the squared distance, and compare.
		const ScalarV thresholdTimesDistSquaredV = Scale(thresholdV, distSquaredV);
		if(IsLessThanAll(thresholdTimesDistSquaredV, crossAbsV))
		{
			// The source entity is moving too fast around us.
			return true;
		}
	}
	return false;
}


void CTaskShockingEventWatch::UpdateHeadTrack()
{
// The old CTaskShockingEventWatch did the head tracking stuff below. I think it works,
// but there may still be some performance problem with head IK, and I'm not sure how well
// it will work together with the scenario clips. Disabled for now.
#if 0
	// If we are not completely facing the event source, use IK to look at it
	if(!IsFacingTarget(0.98f) && m_ShockingEvent)
	{
		Vector3 vPosition = VEC3V_TO_VECTOR3(m_ShockingEvent->GetReactPosition());

		CEntity* pSourceEntity = m_ShockingEvent->GetSourceEntity();
		if(pSourceEntity && pSourceEntity->GetIsTypePed())
		{
			GetPed()->GetIkManager().LookAt(0, pSourceEntity, 100, BONETAG_HEAD, NULL);
		}
		else
		{
			GetPed()->GetIkManager().LookAt(0, NULL, 100, BONETAG_INVALID, &vPosition);
		}
	}
#endif
}


bool CTaskShockingEventWatch::UpdateWatchTime()
{
	if(!m_WatchTimeStarted || m_bWatchForever)
	{
		return true;
	}

	u32 delta = CTask::GetTimeStepInMilliseconds();
	if(delta < m_RandomWatchTime)
	{
		m_RandomWatchTime -= delta;
		return true;
	}
	else
	{
		m_RandomWatchTime = 0;

		// We stopped watching too soon.
		if (IsToSoonToLeaveAfterSomeoneElse())
		{
			return true;
		}
		else
		{
			return false;
		}
	}
}

bool CTaskShockingEventWatch::IsTooCloseToPlayerToFilm() const
{
	const CPed* pPed = GetPed();
	if (pPed)
	{
		const CPed* pPlayerPed = FindPlayerPed();
		if (pPlayerPed)
		{
			ScalarV vDistFromPlayerSq = DistSquared(pPlayerPed->GetTransform().GetPosition(), pPed->GetTransform().GetPosition());
			ScalarV vMinDistSquared   = LoadScalar32IntoScalarV(rage::square(sm_Tunables.m_MinDistanceAwayForFilming));
			if (IsLessThanAll(vDistFromPlayerSq, vMinDistSquared))
			{
				return true;
			}
		}
	}
	return false;
}

bool CTaskShockingEventWatch::IsTooCloseToOtherPeds() const
{
	// Iterate over nearby peds to see if they're also filming.
	const CPed* pPed = GetPed();
	if (pPed)
	{
		CPedIntelligence* pPedIntelligence = pPed->GetPedIntelligence();
		if (pPedIntelligence)
		{
			const ScalarV vMinDistBetweenPedsFilmingSquared = LoadScalar32IntoScalarV(rage::square(sm_Tunables.m_MinDistanceBetweenFilmingPeds));
			const ScalarV vMinDistBetweenPedsSquared = LoadScalar32IntoScalarV(rage::square(sm_Tunables.m_MinDistanceFromOtherPedsToFilm));

			CEntityScannerIterator pedList = pPed->GetPedIntelligence()->GetNearbyPeds();
			for( CEntity* pNearbyEnt = pedList.GetFirst(); pNearbyEnt; pNearbyEnt = pedList.GetNext() )
			{
				CPed* pNearbyPed = static_cast<CPed*>(pNearbyEnt);
				if (pNearbyPed && !pNearbyPed->IsNetworkClone())
				{
					CPedIntelligence* pNearbyPedIntelligence = pNearbyPed->GetPedIntelligence();
					if (pNearbyPedIntelligence)
					{
						ScalarV vDistBetweenPedsSquared = DistSquared(pNearbyPed->GetTransform().GetPosition(), pPed->GetTransform().GetPosition());
						
						// Peds must have a minimum distance from other peds to make sure the anim doesn't clip. Return true if we are too close.
						if (IsLessThanAll(vDistBetweenPedsSquared, vMinDistBetweenPedsSquared))
						{
							return true;
						}

						// See if the nearby ped is running a ShockingEventWatch and if so, is filming the event
						CTaskShockingEventWatch* pShockingEventWatch = static_cast<CTaskShockingEventWatch*>(pNearbyPedIntelligence->FindTaskActiveByType(CTaskTypes::TASK_SHOCKING_EVENT_WATCH));
						if ( pShockingEventWatch && pShockingEventWatch->IsFilming() )
						{
							// The nearby ped is filming the shocking event. If it's closer to us than the tunable distance, return that we are too close.
							if (IsLessThanAll(vDistBetweenPedsSquared, vMinDistBetweenPedsFilmingSquared))
							{
								return true;
							}
						}
					}
				}
			}
		}
	}
	return false;
}

bool CTaskShockingEventWatch::IsTooCloseToFilm() const
{
	const CPed* pPed = GetPed();
	if (pPed)
	{
		ScalarV vMinDistanceAwayForFilmSquared = ScalarV(rage::square(sm_Tunables.m_MinDistanceAwayForFilming));
		ScalarV vDistanceToEventSquared = DistSquared(pPed->GetTransform().GetPosition(), VECTOR3_TO_VEC3V(m_ShockingEvent->GetSourcePosition()));
		if (IsLessThanAll(vDistanceToEventSquared, vMinDistanceAwayForFilmSquared))
		{
			return true;
		}
	}
	return false;
}

void CTaskShockingEventWatch::SayFilmShockingSpeech()
{
	if (m_ShockingEvent)
	{
		atHashWithStringNotFinal sSpeech = m_ShockingEvent->GetShockingFilmSpeechHash();
		
		// If the hash was valid, tell the ped to say it.
		if (sSpeech.GetHash())
		{
			CPed* pPed = GetPed();
			if (pPed)
			{
				pPed->NewSay(sSpeech.GetHash());
			}
		}
	}
}

bool CTaskShockingEventWatch::IsToSoonToLeaveAfterSomeoneElse() const
{
	return fwTimer::GetTimeInMilliseconds() < sm_NextTimeAnyoneCanStopWatching;
}

// Extra precaution for random watch times not being quite enough to prevent twinning.
void CTaskShockingEventWatch::SetLastTimeOfWatchExit()
{
	static const u32 sm_MinTimeBetweenWatchExitsMS = 500;
	sm_NextTimeAnyoneCanStopWatching = fwTimer::GetTimeInMilliseconds() + sm_MinTimeBetweenWatchExitsMS;
}

//-------------------------------------------------------------------------
//  CTaskShockingReact
//-------------------------------------------------------------------------

//tunable
CTaskShockingEventReact::Tunables CTaskShockingEventReact::sm_Tunables;

IMPLEMENT_RESPONSE_TASK_TUNABLES(CTaskShockingEventReact, 0x12645f82);


//move network static variables

//check state from move network
const fwMvBooleanId CTaskShockingEventReact::ms_IntroDoneId("IntroDone",0xF6916ECC);
const fwMvBooleanId CTaskShockingEventReact::ms_ReactBaseDoneId("ReactBaseDone",0x64A7F4F9);
const fwMvBooleanId CTaskShockingEventReact::ms_ReactIdleDoneId("ReactIdleDone",0x9DCD0FE7);


//used to vary animation within a clip
const fwMvFloatId CTaskShockingEventReact::ms_IntroVariationId("IntroVariation",0xFC069714);
const fwMvFloatId CTaskShockingEventReact::ms_ReactVariationId("ReactVariation",0x2E1C8E87);

//used to tell move network which direction to play in
const fwMvFlagId CTaskShockingEventReact::ms_ReactBackId("ReactBack",0x582D458);
const fwMvFlagId CTaskShockingEventReact::ms_ReactLeftId("ReactLeft",0xAD3A73C7);
const fwMvFlagId CTaskShockingEventReact::ms_ReactFrontId("ReactFront",0x49FA785A);
const fwMvFlagId CTaskShockingEventReact::ms_ReactRightId("ReactRight",0xA2F5709A);

//determines if the animation is extreme or not
const fwMvFlagId CTaskShockingEventReact::ms_IsBigId("IsBig",0xCE78F713);
const fwMvFlagId CTaskShockingEventReact::ms_IsFemaleId("IsFemale",0xC70A0828);

//tells the move network what state to transition to
const fwMvRequestId CTaskShockingEventReact::ms_PlayIntroRequest("PlayIntroRequest",0xAB4376BB);
const fwMvRequestId CTaskShockingEventReact::ms_PlayBaseRequest("PlayBaseRequest",0x7F61BB38);
const fwMvRequestId CTaskShockingEventReact::ms_PlayIdleRequest("PlayReactIdleRequest",0xCE2BCB1B);

const fwMvClipId CTaskShockingEventReact::ms_IntroClipId("IntroClip",0x4139FEC0);
const fwMvFloatId CTaskShockingEventReact::ms_IntroPhaseId("IntroPhase",0xE8C2B9F8);
const fwMvFloatId CTaskShockingEventReact::ms_IdlePhaseId("IdlePhase",0x1A3E76E2);
const fwMvFloatId CTaskShockingEventReact::ms_TransPhaseId("TransPhase",0x3EC86A92);

CTaskShockingEventReact::CTaskShockingEventReact(const CEventShocking* event)
: CTaskShockingEvent(event)
, m_MoveNetworkHelper()
, m_ClipSetHelper()
, m_fTimeUntilNextReactionIdle(0.0f)
, m_fAnimPhase(0.0f)
, m_bCanChangeHeading(false)
, m_bHasCachedClipRotation(false)
, m_bMoveIsInTargetState(false)
{
	SetInternalTaskType(CTaskTypes::TASK_SHOCKING_EVENT_REACT);
}

aiTask* CTaskShockingEventReact::Copy() const {
	CTaskShockingEventReact* pCopy = rage_new CTaskShockingEventReact(m_ShockingEvent);
	if (pCopy)
	{
		pCopy->SetShouldPlayShockingSpeech(ShouldPlayShockingSpeech());
	}
	return pCopy;
}

void CTaskShockingEventReact::CleanUp()
{
	//Grab the ped.
	CPed* pPed = GetPed();

	//Ensure the ped is valid.
	if(pPed)
	{
		//Check if the move network is active.
		if(m_MoveNetworkHelper.IsNetworkActive())
		{	
			//Clear the move network.
			pPed->GetMovePed().ClearTaskNetwork(m_MoveNetworkHelper, REALLY_SLOW_BLEND_DURATION);
		}
	}
}

CTask::FSM_Return CTaskShockingEventReact::ProcessPreFSM()
{
	// Call the base class version first, as the LookAtEvent() below is expecting a valid pointer.
	CTask::FSM_Return iFSM = CTaskShockingEvent::ProcessPreFSM();
	
	if(iFSM == FSM_Quit)
	{
		return FSM_Quit;
	}

	//Grab the ped.
	CPed* pPed = GetPed();

	pPed->GetPedIntelligence()->SetCheckShockFlag(true);

	LookAtEvent();

	UpdateDesiredHeading();

	// Allow certain lod modes
	pPed->GetPedAiLod().ClearBlockedLodFlag(CPedAILod::AL_LodPhysics);
	pPed->GetPedAiLod().ClearBlockedLodFlag(CPedAILod::AL_LodMotionTask);

	if (GetState() == State_PlayingIntro)
	{
		// Tell the task to call ProcessPhysics
		pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPhysicsTasks, true );
	}
	
	return FSM_Continue;
}

CTask::FSM_Return CTaskShockingEventReact::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		FSM_State(State_StreamingIntro)
			FSM_OnEnter
				StateStreamingIntroOnEnter();
			FSM_OnUpdate
				return StateStreamingIntroOnUpdate();
		FSM_State(State_PlayingIntro)
			FSM_OnEnter
				StatePlayingIntroOnEnter();
			FSM_OnUpdate
				return StatePlayingIntroOnUpdate();
		FSM_State(State_PlayingGeneric)
			FSM_OnEnter
				StatePlayingGenericOnEnter();
			FSM_OnUpdate
				StatePlayingGenericOnUpdate();
		FSM_State(State_StreamingReact)
			FSM_OnEnter
				StateStreamingReactOnEnter();
			FSM_OnUpdate
				return StateStreamingReactOnUpdate();
		FSM_State(State_PlayingReact)
			FSM_OnEnter
				StatePlayingReactOnEnter();
			FSM_OnUpdate
				return StatePlayingReactOnUpdate();
			FSM_OnExit
				StatePlayingReactOnExit();
		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

bool CTaskShockingEventReact::ProcessMoveSignals()
{
	s32 iState = GetState();

	switch(iState)
	{
		case State_PlayingIntro:
		{
			StatePlayingIntroOnProcessMoveSignals();
			return true;
		}
		case State_PlayingReact:
		{
			StatePlayingReactOnProcessMoveSignals();
			return true;
		}
		default:
		{
			return false;
		}
	}
}

bool CTaskShockingEventReact::ProcessPhysics( float fTimeStep, int UNUSED_PARAM(nTimeSlice) )
{
	if (GetState() == State_PlayingIntro)
	{
		const crClip* pClip = m_MoveNetworkHelper.GetClip(ms_IntroClipId);
		const float fPhase = rage::Clamp(m_MoveNetworkHelper.GetFloat(ms_IntroPhaseId), 0.0f, 1.0f);
		if (pClip)
		{
			if (!m_bHasCachedClipRotation)
			{
				m_qCachedClipTotalRotation = fwAnimHelpers::GetMoverTrackRotation(*pClip, 1.f);
				m_bHasCachedClipRotation = true;
			}
			AdjustAngularVelocityForIntroClip(fTimeStep, *pClip, fPhase, m_qCachedClipTotalRotation);
		}
	}

	return true;
}

// Used by other tasks to tell the React Task to stop playing the animations early.
void	CTaskShockingEventReact::StopReacting()
{
	if (m_Timer.IsSet() && !m_Timer.IsOutOfTime())
	{
		m_Timer.ChangeDuration(0);
	}
}

#if !__FINAL

const char * CTaskShockingEventReact::GetStaticStateName( s32 iState )
{
	static const char* aStateNames[] = 
	{
		"StreamingIntro",
		"PlayingIntro",
		"PlayingGeneric",
		"StreamingReact",
		"PlayingReact",
		"Quit"
	};
	CompileTimeAssert(NELEM(aStateNames) == kNumStates);

	if(iState >= 0 && iState < kNumStates)
	{
		return aStateNames[iState];
	}
	else
	{
		return "Invalid";
	}
}

#endif // !__FINAL

// Round our facing to the nearest Cardinal Coordinate and play the animation
// that corresponds to it.
void CTaskShockingEventReact::PickIntroClipFromFacing()
{
	const CEventShocking::Tunables& tunables = m_ShockingEvent->GetTunables();
	const CPed* pPed = GetPed();

	CAITarget aTarget;
	GetTarget(aTarget);

	Vec3V vEvent;
	aTarget.GetPosition(RC_VECTOR3(vEvent));
	
	const Vec3V vPed = pPed->GetTransform().GetPosition();
	float fAngle = fwAngle::GetRadianAngleBetweenPoints(vEvent.GetXf(), vEvent.GetYf(), vPed.GetXf(), vPed.GetYf());
	fAngle = SubtractAngleShorterFast(fAngle, pPed->GetCurrentHeading());

	// first get heading into interval 0->2Pi
	if(fAngle < 0.0f)
		fAngle += TWO_PI;
	// then reverse direction
	fAngle = TWO_PI - fAngle;
	// subtract half an interval of the node heading range
	fAngle = fAngle + (TWO_PI/(4*2));
	if(fAngle >= TWO_PI)
		fAngle -= TWO_PI;

	const int iDirection = static_cast<int>(rage::Floorf(4.0f*(fAngle/TWO_PI)));

	// 0 = Forward, increment clockwise
	if (tunables.m_ReactionMode == CEventShocking::BIG_REACTION)
	{
		switch(iDirection)
		{
		case 0:
			m_IntroClipSet = CLIP_SET_REACT_BIG_INTRO_FORWARD;
			break;
		case 1:
			m_IntroClipSet = CLIP_SET_REACT_BIG_INTRO_RIGHT;
			break;
		case 2:
			m_IntroClipSet = CLIP_SET_REACT_BIG_INTRO_BACKWARD;
			break;
		case 3:
			m_IntroClipSet = CLIP_SET_REACT_BIG_INTRO_LEFT;
			break;
		default:
			Assertf(0, "Invalid direction specified in PickIntroClip!\n");
			break;
		}
		m_ExitClipSet = CLIP_SET_REACT_BIG_EXIT;
	}
	else
	{
		switch(iDirection)
		{
			case 0:
				m_IntroClipSet = CLIP_SET_REACT_SMALL_INTRO_FORWARD;
				break;
			case 1:
				m_IntroClipSet = CLIP_SET_REACT_SMALL_INTRO_RIGHT;
				break;
			case 2:
				m_IntroClipSet = CLIP_SET_REACT_SMALL_INTRO_BACKWARD;
				break;
			case 3:
				m_IntroClipSet = CLIP_SET_REACT_SMALL_INTRO_LEFT;
				break;
			default:
				Assertf(0, "Invalid direction specified in PickIntroClip!\n");
				break;
		}
		m_ExitClipSet = CLIP_SET_REACT_SMALL_EXIT;
		
	}
}

// After the clipset has been chosen, we also need to pass that information onto the MoVE network.
void CTaskShockingEventReact::SetFacingFlag(CMoveNetworkHelper& helper)
{
	if (m_IntroClipSet == CLIP_SET_REACT_SMALL_INTRO_LEFT || m_IntroClipSet == CLIP_SET_REACT_BIG_INTRO_LEFT)
	{
		helper.SetFlag(true, ms_ReactLeftId);
	} 
	else if (m_IntroClipSet == CLIP_SET_REACT_SMALL_INTRO_RIGHT || m_IntroClipSet == CLIP_SET_REACT_BIG_INTRO_RIGHT)
	{
		helper.SetFlag(true, ms_ReactRightId);
	} 
	else if (m_IntroClipSet == CLIP_SET_REACT_SMALL_INTRO_FORWARD || m_IntroClipSet == CLIP_SET_REACT_BIG_INTRO_FORWARD)
	{
		helper.SetFlag(true, ms_ReactFrontId);
	} 
	else 
	{
		helper.SetFlag(true, ms_ReactBackId);
	}
}

// Pick which clips to use and start the streaming requests.
void CTaskShockingEventReact::StateStreamingIntroOnEnter()
{
	// Only worry about streaming if we were told by the event to actually play an intro reaction.
	if (m_ShockingEvent->GetTunables().m_ReactionMode != CEventShocking::NO_REACTION)
	{
		//decide which clip dicts to use
		PickIntroClipFromFacing();

		//begin streaming in the intro clip
		m_ClipSetHelper.AddClipSetRequest(m_IntroClipSet);
	}
}

// Check if the streaming request has completed, if it has then move to play the intro animation clip.
CTask::FSM_Return CTaskShockingEventReact::StateStreamingIntroOnUpdate()
{
	//streaming requests
	bool bNetworkIsStreamedIn = fwAnimDirector::RequestNetworkDef(CClipNetworkMoveInfo::ms_NetworkTaskShockingEventReact);
	bool bClipIsLoaded = false;
	
	// There are no clips to stream in if there is no intro reaction.
	if (m_ShockingEvent->GetTunables().m_ReactionMode == CEventShocking::NO_REACTION)
	{
		bClipIsLoaded = true;
	}
	else
	{
		bClipIsLoaded = m_ClipSetHelper.RequestAllClipSets();
	}

	if (bNetworkIsStreamedIn && bClipIsLoaded)
	{
		//Grab the ped.
		CPed* pPed = GetPed();

		//Create the MoVE network around the ped
		m_MoveNetworkHelper.CreateNetworkPlayer(pPed, CClipNetworkMoveInfo::ms_NetworkTaskShockingEventReact);
		pPed->GetMovePed().SetTaskNetwork(m_MoveNetworkHelper.GetNetworkPlayer(), REALLY_SLOW_BLEND_DURATION);
		
		// Skip straight to the base stage if not playing an intro reaction.
		if (m_ShockingEvent->GetTunables().m_ReactionMode == CEventShocking::NO_REACTION)
		{
			SetState(State_PlayingGeneric);
		}
		else
		{
			// Check with the blackboard to see if we're allowed to play this clip right now.
			// Clip is 0 in the constructor - we dont care which clip it is, we want to block the entire clipset from being played as the anims
			// are quite similar.
			CAnimBlackboard::AnimBlackboardItem blackboardItem(m_IntroClipSet, 0, CAnimBlackboard::REACTION_ANIM_TIME_DELAY);

			// If blackboard lets us play it great, otherwise fallback to the generic reactions.
			if (ANIMBLACKBOARD.AddItemToBlackboardAndPedIntelligenceIfAllowedToPlay(blackboardItem, pPed->GetPedIntelligence()))
			{
				//Set the flags up for the MoVE network to play the intro clip
				m_MoveNetworkHelper.SetFlag(m_ShockingEvent->GetTunables().m_ReactionMode == CEventShocking::BIG_REACTION, ms_IsBigId);
				m_MoveNetworkHelper.SetFlag(!pPed->IsMale(), ms_IsFemaleId);
				m_MoveNetworkHelper.SetClipSet(m_IntroClipSet);
				SetFacingFlag(m_MoveNetworkHelper);
				SetState(State_PlayingIntro);
			}
			else
			{
				SetState(State_PlayingGeneric);
			}
		}
	}

	return FSM_Continue;
}

// Tell the MoVE network to start playing the clip.
void CTaskShockingEventReact::StatePlayingIntroOnEnter()
{
	if (!m_Timer.IsSet())
	{
		//End the task after a variable number of seconds have elapsed.
		const bool bUseHurryAwayTimes = false;
		const u32 uiRandomWatchTime = GetRandomWatchTimeMS(*m_ShockingEvent, bUseHurryAwayTimes);
		m_Timer.Set(fwTimer::GetTimeInMilliseconds(), uiRandomWatchTime);
	}

	// let the move network vary which clip is chosen from the clip dict
	m_MoveNetworkHelper.SetFloat(ms_IntroVariationId, fwRandom::GetRandomNumberInRange(0.0f, 1.0f));
	// start playing the intro clip
	m_MoveNetworkHelper.SendRequest(ms_PlayIntroRequest);

	m_MoveNetworkHelper.WaitForTargetState(ms_IntroDoneId);

	// Reset cached MoVE variables.
	m_bMoveIsInTargetState = false;
	m_fAnimPhase = 0.0f;

	// Request signals to be sent from MoVE.
	RequestProcessMoveSignalCalls();
}

// Check if the intro anim is complete, if it is then proceed to the reaction animation.
// Also make a call to face the target during the anim.
CTask::FSM_Return CTaskShockingEventReact::StatePlayingIntroOnUpdate()
{
	// keep streaming in the reaction clipsets
	m_ClipSetHelper.RequestAllClipSets();

	//Check if the target state has been reached.
	if(m_fAnimPhase > sm_Tunables.m_BlendoutPhase || m_bMoveIsInTargetState)
	{
		// Do a check on the distance from the ped to the shocking event.  If close, then abandon this task early so the ped can flee faster.
		Vec3V vShockingEventPosition = m_ShockingEvent->GetEntityOrSourcePosition();
		Vec3V vPed = GetPed()->GetTransform().GetPosition();
		if (IsLessThanAll(DistSquared(vShockingEventPosition, vPed), ScalarV(m_ShockingEvent->GetTunables().m_HurryAwayMBRChangeRange)))
		{
			SetState(State_Finish);
		}
		else
		{
			SetState(State_PlayingGeneric);
		}
	}
	return FSM_Continue;
}

void CTaskShockingEventReact::StatePlayingIntroOnProcessMoveSignals()
{
	// Check the phase of the anim and see if we should blend out.
	float fPhase = 0.0f;
	if (GetPed()->IsMale())
	{
		m_MoveNetworkHelper.GetFloat(ms_IntroPhaseId, fPhase);
	}
	else
	{
		m_MoveNetworkHelper.GetFloat(ms_TransPhaseId, fPhase);
	}

	// Cache MoVE variables for timeslicing.
	m_fAnimPhase = fPhase;
	m_bMoveIsInTargetState |= m_MoveNetworkHelper.IsInTargetState();
}

void CTaskShockingEventReact::StatePlayingGenericOnEnter()
{
	if (!m_Timer.IsSet())
	{
		//End the task after a variable number of seconds have elapsed.
		const bool bUseHurryAwayTimes = false;
		const u32 uiRandomWatchTime = GetRandomWatchTimeMS(*m_ShockingEvent, bUseHurryAwayTimes);
		m_Timer.Set(fwTimer::GetTimeInMilliseconds(), uiRandomWatchTime);
	}

	//Check if the move network is active and if so clear it.  The Generic state does not play any animations.
	if(m_MoveNetworkHelper.IsNetworkActive())
	{	
		//Clear the move network.
		CPed* pPed = GetPed();
		pPed->GetMovePed().ClearTaskNetwork(m_MoveNetworkHelper, REALLY_SLOW_BLEND_DURATION);
	}
	//@@: location CTASKSHOCKINGEVENTREACT_STATEPLAYINGGENERICONENTER
	m_fTimeUntilNextReactionIdle = fwRandom::GetRandomNumberInRange(sm_Tunables.m_TimeBetweenReactionIdlesMin, sm_Tunables.m_TimeBetweenReactionIdlesMax);
}

CTask::FSM_Return CTaskShockingEventReact::StatePlayingGenericOnUpdate()
{
	if (m_Timer.IsOutOfTime())
	{
		SetState(State_Finish);
	}
	// If not playing a reaction intro, don't play the idles either (might change depending on feedback).
	else if (m_ShockingEvent->GetTunables().m_ReactionMode != CEventShocking::NO_REACTION)
	{
		m_fTimeUntilNextReactionIdle -= GetTimeStep();
		//Periodically switch to the reaction idle animations.
		if (m_fTimeUntilNextReactionIdle <= 0.0f)
		{
			
			SetState(State_StreamingReact);
		}
	}
	return FSM_Continue;
}

void CTaskShockingEventReact::UpdateDesiredHeading()
{
	CAITarget aTarget;
	GetTarget(aTarget);

	Vector3 vPosition;
	aTarget.GetPosition(vPosition);

	CPed* pPed = GetPed();
	pPed->SetDesiredHeading(vPosition);
}

void CTaskShockingEventReact::GetTarget(CAITarget& rTarget) const
{
	if(m_ShockingEvent->GetTunables().m_ReactToOtherEntity && m_ShockingEvent->GetOtherEntity())
	{
		rTarget = CAITarget(m_ShockingEvent->GetOtherEntity());
	}
	else
	{
		// Turn to face the target if it has moved.
		CEntity* pEntity = m_ShockingEvent->GetSourceEntity();
		if (pEntity)
		{
			rTarget = CAITarget(pEntity);
		}
		else
		{
			rTarget = CAITarget(VEC3V_TO_VECTOR3(m_ShockingEvent->GetReactPosition()));
		}
	}
}

void CTaskShockingEventReact::StateStreamingReactOnEnter()
{
	CPed* pPed = GetPed();
	
	//Create the MoVE network around the ped now that the ped is playing reaction idles.
	pPed->GetMovePed().SetTaskNetwork(m_MoveNetworkHelper.GetNetworkPlayer(), SLOW_BLEND_DURATION);

	m_ReactClipSet = CLIP_SET_REACT_SMALL_VARIATIONS_IDLE_E;
	m_ClipSetHelper.AddClipSetRequest(m_ReactClipSet);

	m_MoveNetworkHelper.SendRequest(ms_PlayBaseRequest);
	m_MoveNetworkHelper.WaitForTargetState(ms_ReactBaseDoneId);
}

// Check if the reaction clipset has been streamed in.  If it has, then move to start playing the clip.
CTask::FSM_Return CTaskShockingEventReact::StateStreamingReactOnUpdate()
{
	bool bReactClipIsStreamedIn = m_ClipSetHelper.RequestAllClipSets();

	// Also wait for the target state.
	bReactClipIsStreamedIn &= m_MoveNetworkHelper.IsInTargetState();

	if (bReactClipIsStreamedIn)
	{
		// Set up reaction clipset / clip choice for the move network.
		m_MoveNetworkHelper.SetClipSet(m_ReactClipSet);

		// let the move network vary which clip is chosen from the clip dict
		m_MoveNetworkHelper.SetFloat(ms_ReactVariationId, fwRandom::GetRandomNumberInRange(0.0f, 1.0f));
		SetState(State_PlayingReact);
	}
	return FSM_Continue;
}

// Start playing the reaction anim clip.
void CTaskShockingEventReact::StatePlayingReactOnEnter()
{
	// Possibly say shocking speech.
	if (m_ShockingEvent && CanWePlayShockingSpeech())
	{
		SayShockingSpeech(m_ShockingEvent->GetTunables().m_ShockingSpeechChance);
	}

	// Send signals to MoVE.
	m_MoveNetworkHelper.SendRequest(ms_PlayIdleRequest);
	m_MoveNetworkHelper.WaitForTargetState(ms_ReactIdleDoneId);

	// Reset cached MoVE variables.
	m_bMoveIsInTargetState = false;
	m_fAnimPhase = 0.0f;

	// Request signals to be sent from MoVE.
	RequestProcessMoveSignalCalls();
}

// Play an idle to vary reaction.
CTaskShockingEventReact::FSM_Return CTaskShockingEventReact::StatePlayingReactOnUpdate()
{
	if (m_fAnimPhase > sm_Tunables.m_BlendoutPhase || m_bMoveIsInTargetState)
	{
		SetState(State_PlayingGeneric);
	}

	return FSM_Continue;
}

void CTaskShockingEventReact::StatePlayingReactOnProcessMoveSignals()
{
	// Check the phase of the anim and see if we should blend out.
	float fPhase = 0.0f;
	if (GetPed()->IsMale())
	{
		m_MoveNetworkHelper.GetFloat(ms_IdlePhaseId, fPhase);
	}
	else
	{
		m_MoveNetworkHelper.GetFloat(ms_TransPhaseId, fPhase);
	}

	// Cache MoVE variables for timeslicing.
	m_fAnimPhase = fPhase;
	m_bMoveIsInTargetState |= m_MoveNetworkHelper.IsInTargetState();
}

void CTaskShockingEventReact::StatePlayingReactOnExit()
{
	m_ClipSetHelper.ReleaseAllClipSets();
}

//------------------------------------------------------------------------
// CTaskShockingEventBackAway
//------------------------------------------------------------------------

CTaskShockingEventBackAway::Tunables CTaskShockingEventBackAway::sm_Tunables;

IMPLEMENT_RESPONSE_TASK_TUNABLES(CTaskShockingEventBackAway, 0x9da7c52a);

const fwMvFlagId CTaskShockingEventBackAway::ms_React0Id("Dir0",0xDB2387F);
const fwMvFlagId CTaskShockingEventBackAway::ms_React90Id("Dir90",0xAA80611F);
const fwMvFlagId CTaskShockingEventBackAway::ms_ReactNeg90Id("Dir-90",0x616FE903);
const fwMvFlagId CTaskShockingEventBackAway::ms_React180Id("Dir180",0x79908657);

const fwMvRequestId CTaskShockingEventBackAway::ms_PlayAnimRequest("PlayAnimRequest",0x2E90BD6F);

const fwMvBooleanId CTaskShockingEventBackAway::ms_AnimDoneId("ClipEnded",0x9ACF9318);
const fwMvFloatId CTaskShockingEventBackAway::ms_AnimPhaseId("ClipPhase",0x597D6169);

const fwMvClipId CTaskShockingEventBackAway::ms_ReactionClip("ReactionClip",0x284D3AD);

CTaskShockingEventBackAway::CTaskShockingEventBackAway(const CEventShocking* event, Vec3V_ConstRef vTarget, bool bCanHurryWhenBumped)
: CTaskShockingEvent(event)
, m_vTargetPoint(vTarget)
, m_vPrevWaypoint(VEC3_ZERO)
, m_vNextWaypoint(VEC3_ZERO)
, m_fAnimPhase(0.0f)
, m_bShouldTryToQuit(false)
, m_bAllowHurryWhenBumped(bCanHurryWhenBumped)
, m_bHasCachedClipRotation(false)
, m_bHasWaypoints(false)
, m_bMoveInTargetState(false)
{
	SetInternalTaskType(CTaskTypes::TASK_SHOCKING_EVENT_BACK_AWAY);
}

aiTask* CTaskShockingEventBackAway::Copy() const
{
	CTaskShockingEventBackAway* pBackAway = rage_new CTaskShockingEventBackAway(m_ShockingEvent, m_vTargetPoint, m_bAllowHurryWhenBumped);
	if (pBackAway)
	{
		pBackAway->SetShouldPlayShockingSpeech(ShouldPlayShockingSpeech());
		pBackAway->m_bHasWaypoints = false;

		if (m_bHasWaypoints)
		{
			pBackAway->m_bHasWaypoints = true;
			pBackAway->m_vPrevWaypoint = m_vPrevWaypoint;
			pBackAway->m_vNextWaypoint = m_vNextWaypoint;
		}
	}
	return pBackAway;
}

#if !__FINAL

const char * CTaskShockingEventBackAway::GetStaticStateName( s32 iState )
{
	static const char* aStateNames[] = 
	{
		"Streaming",
		"Playing",
		"HurryAway",
		"Finish"
	};
	CompileTimeAssert(NELEM(aStateNames) == kNumStates);

	if(iState >= 0 && iState < kNumStates)
	{
		return aStateNames[iState];
	}
	else
	{
		return "Invalid";
	}
}

#endif // !__FINAL


void CTaskShockingEventBackAway::CleanUp()
{
	//Grab the ped.
	CPed* pPed = GetPed();

	//Ensure the ped is valid.
	if(pPed)
	{
		//Check if the move network is active.
		if(m_MoveNetworkHelper.IsNetworkActive())
		{	
			//Clear the move network and force the ped to be in a motion state so that there isnt a slight pause before it resumes walking
			pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Walk);
			
			pPed->GetMovePed().ClearTaskNetwork(m_MoveNetworkHelper, sm_Tunables.m_MoveNetworkBlendoutDuration, CMovePed::Task_TagSyncTransition);
			CTaskUnalerted* pTask = static_cast<CTaskUnalerted*>(pPed->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_UNALERTED));
			if (pTask)
			{
				pTask->SetKeepMovingWhilstWaitingForFirstWanderPath(true);
			}
		}

		// Now that backing away is completed, let the ped orient towards their heading.
		CTask* pMove = pPed->GetPedIntelligence()->GetActiveSimplestMovementTask();
		if (pMove && pMove->GetTaskType() == CTaskTypes::TASK_MOVE_GO_TO_POINT)
		{
			CTaskMoveGoToPoint* pTaskMoveGoto = static_cast<CTaskMoveGoToPoint*>(pMove);
			pTaskMoveGoto->SetDisableHeadingUpdate(false);
		}
	}
}

CTask::FSM_Return CTaskShockingEventBackAway::ProcessPreFSM()
{
	//Grab the ped.
	CPed* pPed = GetPed();

	// Allow certain lod modes
	pPed->GetPedAiLod().ClearBlockedLodFlag(CPedAILod::AL_LodPhysics);
	pPed->GetPedAiLod().ClearBlockedLodFlag(CPedAILod::AL_LodMotionTask);

	// Force the ped not to orient towards their path if they have one, we want to control the desired heading directly from this task.
	pPed->SetPedResetFlag(CPED_RESET_FLAG_DisableMoveTaskHeadingAdjustments, true);
	
	return CTaskShockingEvent::ProcessPreFSM();
}

CTask::FSM_Return CTaskShockingEventBackAway::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		FSM_State(State_Streaming)
			FSM_OnEnter
				StateStreamingOnEnter();
			FSM_OnUpdate
				return StateStreamingOnUpdate();
		FSM_State(State_Playing)
			FSM_OnEnter
				StatePlayingOnEnter();
			FSM_OnUpdate
				return StatePlayingOnUpdate();
		FSM_State(State_HurryAway)
			FSM_OnEnter
				StateHurryAwayOnEnter();
			FSM_OnUpdate
				return StateHurryAwayOnUpdate();
		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

bool CTaskShockingEventBackAway::ProcessMoveSignals()
{
	s32 iState = GetState();

	switch(iState)
	{
		case State_Playing:
		{
			StatePlayingOnProcessMoveSignals();
			return true;
		}
		default:
		{
			return false;
		}
	}
}

// PURPOSE: Manipulate the animated velocity
bool CTaskShockingEventBackAway::ProcessPhysics(float fTimeStep, int UNUSED_PARAM(nTimeSlice))
{
	if (GetState() == State_Playing)
	{
		const crClip* pClip = m_MoveNetworkHelper.GetClip(ms_ReactionClip);
		const float fPhase = rage::Clamp(m_MoveNetworkHelper.GetFloat(ms_AnimPhaseId), 0.0f, 1.0f);
		if (pClip)
		{
			if (!m_bHasCachedClipRotation)
			{
				m_qCachedClipTotalRotation = fwAnimHelpers::GetMoverTrackRotation(*pClip, 1.f);
				m_bHasCachedClipRotation = true;
			}
			AdjustAngularVelocityForIntroClip(fTimeStep, *pClip, fPhase, m_qCachedClipTotalRotation);
		}
	}
	return true;
}

// Decide which clipset to play and stream it in
void CTaskShockingEventBackAway::StateStreamingOnEnter()
{
	//decide which clip dicts to stream in
	PickClipSet();

	m_ClipSetHelper.Request(m_ClipSetId);
}

// Stream in the move network and clipset
CTask::FSM_Return CTaskShockingEventBackAway::StateStreamingOnUpdate()
{
	// Maintain current heading.
	CPed* pPed = GetPed();
	pPed->SetPedResetFlag(CPED_RESET_FLAG_SyncDesiredHeadingToCurrentHeading, true);
	pPed->SetPedResetFlag(CPED_RESET_FLAG_IsHigherPriorityClipControllingPed, true);

	//Start looking at the event at this point
	LookAtEvent();

	m_ClipSetHelper.Request(m_ClipSetId);

	bool bNetworkIsStreamedIn = fwAnimDirector::RequestNetworkDef(CClipNetworkMoveInfo::ms_NetworkTaskShockingEventBackAway);
	bool bClipIsLoaded = m_ClipSetHelper.IsLoaded();

	// if assets streamed in AND reaction varying time has passed, proceed to play animation
	if (bNetworkIsStreamedIn && bClipIsLoaded)
	{
		// Check with the blackboard to see if we're allowed to play this clip right now.
		// Clip is 0 in the constructor - we dont care which clip it is, we want to block the entire clipset from being played as the anims
		// are quite similar.
		CAnimBlackboard::AnimBlackboardItem blackboardItem(m_ClipSetId, 0, CAnimBlackboard::REACTION_ANIM_TIME_DELAY);
		if (ANIMBLACKBOARD.AddItemToBlackboardAndPedIntelligenceIfAllowedToPlay(blackboardItem, pPed->GetPedIntelligence()))
		{
			m_MoveNetworkHelper.CreateNetworkPlayer(pPed, CClipNetworkMoveInfo::ms_NetworkTaskShockingEventBackAway);
			pPed->GetMovePed().SetTaskNetwork(m_MoveNetworkHelper.GetNetworkPlayer(), SLOW_BLEND_DURATION);

			m_MoveNetworkHelper.SetClipSet(m_ClipSetId);

			SetFacingFlag(m_MoveNetworkHelper);

			SetState(State_Playing);
		}
	}
	return FSM_Continue;
}

// Tell the move network to start playing the animation
void CTaskShockingEventBackAway::StatePlayingOnEnter()
{
	// Possibly say shocking speech.
	if (m_ShockingEvent && CanWePlayShockingSpeech())
	{
		SayShockingSpeech(m_ShockingEvent->GetTunables().m_ShockingSpeechChance);
	}

	// Send signals to MoVe
	m_MoveNetworkHelper.SendRequest(ms_PlayAnimRequest);
	m_MoveNetworkHelper.WaitForTargetState(ms_AnimDoneId);

	// Reset cached MoVE variables
	m_fAnimPhase = 0.0f;
	m_bMoveInTargetState = false;

	// Request that MoVE send signals back to this task.
	RequestProcessMoveSignalCalls();
}

// Check for the animation being completed, finish if it is
CTask::FSM_Return CTaskShockingEventBackAway::StatePlayingOnUpdate()
{
	// Cache the ped pointer.
	CPed* pPed = GetPed();

	// Look at the target.
	LookAtEvent();

	// Call process physics to apply extra velocity changes to help stick to the path while playing the back away anim.
	pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPhysicsTasks, true );

	// If the network is fully blended in then steer to the target point.
	if (pPed->GetMovePed().IsTaskNetworkFullyBlended())
	{
		pPed->SetDesiredHeading(VEC3V_TO_VECTOR3(m_vTargetPoint));
		pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_AnimatedVelocity);
	}
	else
	{
		// Otherwise disable turns by synching the desired heading to the current heading.
		pPed->SetPedResetFlag(CPED_RESET_FLAG_SyncDesiredHeadingToCurrentHeading, true);
		pPed->SetPedResetFlag(CPED_RESET_FLAG_IsHigherPriorityClipControllingPed, true);
	}

	if (ShouldBlendOutBecauseOffPath())
	{
		SetState(State_Finish);
	}
	else if (m_fAnimPhase >= sm_Tunables.m_BlendOutPhase || m_bMoveInTargetState)
	{
		// Check if the clip has finished playing.
		// The animation can exit out early if there was a collision and we have exceeded the phase of the tag event for exiting.
		SetState(State_Finish);
	}
	// Ped has backed into corner, hurry away.
	else if (HaveHadACollision() && CanQuitAnimationNow())
	{
		if (m_bAllowHurryWhenBumped)
		{
			SetState(State_HurryAway);
		}
		else
		{
			SetState(State_Finish);
		}

	}
	return FSM_Continue;
}

void CTaskShockingEventBackAway::StatePlayingOnProcessMoveSignals()
{
	// Look up the phase of the animation.
	float fPhase = 0.0f;
	m_MoveNetworkHelper.GetFloat(ms_AnimPhaseId, fPhase);
	m_fAnimPhase = fPhase;

	// Cache the MoVE state.
	m_bMoveInTargetState |= m_MoveNetworkHelper.IsInTargetState();
}

void CTaskShockingEventBackAway::StateHurryAwayOnEnter()
{
	// Ped has backed into a corner, do a hurry away to get out of the situation.
	CTaskShockingEventHurryAway* pSubtask = rage_new CTaskShockingEventHurryAway(m_ShockingEvent);
	SetNewTask(pSubtask);
}

CTask::FSM_Return CTaskShockingEventBackAway::StateHurryAwayOnUpdate()
{
	// Check if the hurry away is done.
	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		return FSM_Quit;
	}
	return FSM_Continue;
}

// Pick clip set based on whether the ped is male or female
void CTaskShockingEventBackAway::PickClipSet()
{
	if (GetPed()->IsMale())
	{
		m_ClipSetId = CLIP_SET_REACT_BACK_AWAY_MALE;
	}
	else
	{
		m_ClipSetId = CLIP_SET_REACT_BACK_AWAY_FEMALE;
	}

}

// Determine facing flags based on where the shocking event is relative to the reacting ped.
void CTaskShockingEventBackAway::SetFacingFlag(CMoveNetworkHelper& helper)
{
	const CPed* pPed = GetPed();

	const Vec3V vPed = pPed->GetTransform().GetPosition();
	float fAngle = fwAngle::GetRadianAngleBetweenPoints(m_vTargetPoint.GetXf(), m_vTargetPoint.GetYf(), vPed.GetXf(), vPed.GetYf());
	fAngle = SubtractAngleShorterFast(fAngle, pPed->GetCurrentHeading());

	// first get heading into interval 0->2Pi
	if(fAngle < 0.0f)
		fAngle += TWO_PI;
	// then reverse direction
	fAngle = TWO_PI - fAngle;
	// subtract half an interval of the node heading range
	fAngle = fAngle + (TWO_PI/(4*2));
	if(fAngle >= TWO_PI)
		fAngle -= TWO_PI;

	int iDirection = static_cast<int>(rage::Floorf(4.0f*(fAngle/TWO_PI)));

	//0 = forward, increment clockwise
	switch(iDirection)
	{
		case 0:
			helper.SetFlag(true, ms_React0Id);
			break;
		case 1:
			helper.SetFlag(true, ms_React90Id);
			break;
		case 2:
			helper.SetFlag(true, ms_React180Id);
			break;
		case 3:
			helper.SetFlag(true, ms_ReactNeg90Id);
			break;
		default:
			Assertf(0, "Invalid direction specified in choose facing for TaskShockingEventBackAway!");
			break;
	}
}

bool CTaskShockingEventBackAway::ShouldBlendOutBecauseOffPath()
{
	if (m_bHasWaypoints)
	{
		Vector3 vToNextWaypoint = m_vNextWaypoint - VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition());
		vToNextWaypoint.z = 0.0f;
		float fDistToNextSqr = vToNextWaypoint.XYMag2();
		vToNextWaypoint.NormalizeSafe(VEC3_ZERO, VERY_SMALL_FLOAT);

		Vector3 vPrefDirToWaypoint = m_vNextWaypoint - m_vPrevWaypoint;
		vPrefDirToWaypoint.z = 0.0f;
		vPrefDirToWaypoint.NormalizeSafe(VEC3_ZERO, VERY_SMALL_FLOAT);

		// Check to make sure the ped is going towards the next waypoint and far enough away that the waypoint won't change.
		if (vToNextWaypoint.Dot(vPrefDirToWaypoint) < rage::Cosf(sm_Tunables.m_MaxWptAngle * DtoR) || fDistToNextSqr < (sm_Tunables.m_MaxDistanceForBackAway * sm_Tunables.m_MaxDistanceForBackAway))
		{
			return true;
		}
	}

	return false;
}

// Return true if we have experienced a collision at some point during the playback of the animation
bool CTaskShockingEventBackAway::HaveHadACollision()
{
	//have experienced a collision earlier, no reason to check the record again this frame
	if (m_bShouldTryToQuit)
	{
		return true;
	}
	else
	{
		//check the collision record, if a collision exists then note it
		const CCollisionRecord* pColRecord = GetPed()->GetFrameCollisionHistory()->GetMostSignificantCollisionRecord();
		if(pColRecord)
		{
			m_bShouldTryToQuit = true;
			return true;
		}
	}
	return false;
}

// Return true if we are within an acceptable phase range to quit the animation based on tags
bool CTaskShockingEventBackAway::CanQuitAnimationNow()
{
	const crClip* activeClip = m_MoveNetworkHelper.GetClip(ms_ReactionClip);
	static CClipEventTags::Key animBreakpoint("BREAK_A",0x8DE1E5EE);
	if (activeClip)
	{
		float fClipPhase = rage::Clamp(m_MoveNetworkHelper.GetFloat(ms_AnimPhaseId), 0.0f, 1.0f);
		float fEventPhase = 1.0f;
		if (CClipEventTags::FindEventPhase(activeClip, animBreakpoint, fEventPhase) && fClipPhase >= fEventPhase)
		{
			return true;
		}		
	}
	return false;
}

Vec3V_Out CTaskShockingEventBackAway::GetDefaultBackAwayPositionForPed(const CPed& rPed)
{
	Vec3V vPosition = rPed.GetTransform().GetPosition();
	Vec3V vBehind = rPed.GetTransform().GetForward();
	vBehind *= ScalarV(-sm_Tunables.m_DefaultBackwardsProjectionRange);
	return vPosition + vBehind;
}

// PURPOSE:  Return true if the target position is close enough to the back away axes to make this task a valid reaction.
bool CTaskShockingEventBackAway::ShouldBackAwayTowardsRoutePosition(Vec3V_ConstRef vPedPosition, Vec3V_ConstRef vPrevWaypointPos, Vec3V_ConstRef vNextWaypointPos)
{
	// Check angle.
	if (!IsFacingRightDirectionForBackAway(vPedPosition, vNextWaypointPos))
	{
		return false;
	}

	// Check flat distance.
	Vec3V vDist = vNextWaypointPos - vPrevWaypointPos;
	vDist.SetZ(ScalarV(0.0f));
	if (IsLessThanAll(MagSquared(vDist), ScalarV(sm_Tunables.m_MinDistanceForBackAway * sm_Tunables.m_MinDistanceForBackAway)))
	{
		return false;
	}

	return true;
}

// PURPOSE:  Is the route waypoint facing the correct direction for a back away to play?
bool CTaskShockingEventBackAway::IsFacingRightDirectionForBackAway(Vec3V_ConstRef vPedPosition, Vec3V_ConstRef vTargetPosition)
{
	// This is dependent on what animations are available.  Right now there are only ones for each of the cardinal directions.
	Vec3V vDirection = vTargetPosition - vPedPosition;
	vDirection.SetZ(ScalarV(V_ZERO));
	vDirection = NormalizeSafe(vDirection, Vec3V(V_ZERO));
	ScalarV vTolerance(sm_Tunables.m_AxesFacingTolerance);
	if (IsGreaterThanAll(vDirection.GetY(), vTolerance))
	{
		// In front
		return true;
	}
	else if (IsGreaterThanAll(-vDirection.GetX(), vTolerance))
	{
		// Left
		return true;
	}
	else if (IsGreaterThanAll(vDirection.GetX(), vTolerance))
	{
		// Right
		return true;
	}
	else if (IsGreaterThanAll(-vDirection.GetY(), vTolerance))
	{
		// Down
		return true;
	}
	return false;
}

//-------------------------------------------------------------------------
// CTaskShockingEventReactToAircraft
//-------------------------------------------------------------------------

CTaskShockingEventReactToAircraft::Tunables CTaskShockingEventReactToAircraft::sm_Tunables;

IMPLEMENT_RESPONSE_TASK_TUNABLES(CTaskShockingEventReactToAircraft, 0xd5f824f7);


CTaskShockingEventReactToAircraft::CTaskShockingEventReactToAircraft(const CEventShocking* event)
: CTaskShockingEvent(event)
{
	SetInternalTaskType(CTaskTypes::TASK_SHOCKING_EVENT_REACT_TO_AIRCRAFT);
}

CTask::FSM_Return CTaskShockingEventReactToAircraft::ProcessPreFSM()
{
	// Call the base class version first, as the logic below is expecting a valid pointer.
	CTask::FSM_Return iFSM = CTaskShockingEvent::ProcessPreFSM();

	if(iFSM == FSM_Quit)
	{
		return FSM_Quit;
	}

	//Grab the ped.
	CPed* pPed = GetPed();

	// The event has expired (probably because the plane/heli/parachute is no longer active).
	if (m_ShockingEvent->CheckHasExpired())
	{
		return FSM_Quit;
	}

	// Allow certain lod modes
	pPed->GetPedAiLod().ClearBlockedLodFlag(CPedAILod::AL_LodPhysics);
	pPed->GetPedAiLod().ClearBlockedLodFlag(CPedAILod::AL_LodMotionTask);

	return FSM_Continue;
}


CTaskShockingEventReactToAircraft::FSM_Return CTaskShockingEventReactToAircraft::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		FSM_State(State_Start)
			FSM_OnUpdate
				return StateStartOnUpdate();
	FSM_State(State_Watching)
		FSM_OnEnter
			return StateWatchingOnEnter();
		FSM_OnUpdate
			return StateWatchingOnUpdate();
	FSM_State(State_RunningAway)
		FSM_OnEnter
			return StateRunningAwayOnEnter();
		FSM_OnUpdate
			return StateRunningAwayOnUpdate();
	FSM_State(State_Finish)
		FSM_OnUpdate
			return FSM_Quit;
	FSM_End
}

#if !__FINAL

const char * CTaskShockingEventReactToAircraft::GetStaticStateName( s32 iState )
{
	static const char* aStateNames[] = 
	{
		"Start",
		"Watching",
		"RunningAway",
		"Finish"
	};
	CompileTimeAssert(NELEM(aStateNames) == kNumStates);

	if(iState >= 0 && iState < kNumStates)
	{
		return aStateNames[iState];
	}
	else
	{
		return "Invalid";
	}
}

#endif // !__FINAL

CTaskShockingEventReactToAircraft::FSM_Return CTaskShockingEventReactToAircraft::StateStartOnUpdate()
{
	DetermineNextStateFromDistance();
	return FSM_Continue;
}

CTaskShockingEventReactToAircraft::FSM_Return CTaskShockingEventReactToAircraft::StateWatchingOnEnter()
{
	CTaskShockingEventReact* pWatchTask = rage_new CTaskShockingEventReact(m_ShockingEvent);
	SetNewTask(pWatchTask);
	return FSM_Continue;
}

CTaskShockingEventReactToAircraft::FSM_Return CTaskShockingEventReactToAircraft::StateWatchingOnUpdate()
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//When the reaction animations finish, leave the area.
		SetState(State_RunningAway);
	}
	else
	{
		DetermineNextStateFromDistance();
	}
	return FSM_Continue;
}

CTaskShockingEventReactToAircraft::FSM_Return CTaskShockingEventReactToAircraft::StateRunningAwayOnEnter()
{
	CTaskShockingEventHurryAway* pHurryTask = rage_new CTaskShockingEventHurryAway(m_ShockingEvent);
	pHurryTask->SetGoDirectlyToWaitForPath(true);
	SetNewTask(pHurryTask);
	return FSM_Continue;
}

CTaskShockingEventReactToAircraft::FSM_Return CTaskShockingEventReactToAircraft::StateRunningAwayOnUpdate()
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//running away could terminate early...if it does move to start state in case we need to run again etc.
		SetState(State_Start);
	}
	else
	{
		DetermineNextStateFromDistance();
	}
	return FSM_Continue;
}

void CTaskShockingEventReactToAircraft::DetermineNextStateFromDistance()
{
	if(m_ShockingEvent && m_ShockingEvent->GetSourceEntity())
	{
		const CPed* pPed = GetPed();
		Vec3V vEventPos = m_ShockingEvent->GetReactPosition();
		Vec3V vPedPos = pPed->GetTransform().GetPosition();
		ScalarV sDistanceSquared = DistSquared(vEventPos, vPedPos);
		s32 iCurrentState = GetState();
		s32 iNextState = iCurrentState;
		// Check the distance to the target ped.
		// See if the event is too far away to care about anymore.
		if (IsGreaterThanAll(sDistanceSquared, ScalarVFromF32(rage::square(sm_Tunables.m_ThresholdWatch))))
		{
			iNextState = State_Finish;
		}
		// Check if its far enough away to just watch.
		// If we are currently running away or ducking, then don't bother transitioning back to watching.
		else if (iCurrentState != State_RunningAway && IsGreaterThanAll(sDistanceSquared, ScalarVFromF32(rage::square(sm_Tunables.m_ThresholdRun))))
		{
			iNextState = State_Watching;			
		}
		// Check if it is close enough to hurry away from.
		else
		{
			iNextState = State_RunningAway;
		}
		// If the state changed, change to the new state.
		if (iNextState != GetState())
		{
			SetState(iNextState);
		}
	}
	else
	{
		//without a source entity move to finish
		SetState(State_Finish);
	}
}

//-------------------------------------------------------------------------
// CTaskShockingPoliceInvestigate
//-------------------------------------------------------------------------

//tunable
CTaskShockingPoliceInvestigate::Tunables CTaskShockingPoliceInvestigate::sm_Tunables;
int CTaskShockingPoliceInvestigate::m_iNumberTaskUses = 0;

IMPLEMENT_RESPONSE_TASK_TUNABLES(CTaskShockingPoliceInvestigate, 0xb8f20dc0);

CTaskShockingPoliceInvestigate::CTaskShockingPoliceInvestigate(const CEventShocking* event)
: CTaskShockingEvent(event)
, m_bFacingIncident(true)
, m_bVehicleTaskGiven(false)
{
	m_iNumberTaskUses++;
	SetInternalTaskType(CTaskTypes::TASK_SHOCKING_POLICE_INVESTIGATE);
}

CTaskShockingPoliceInvestigate::~CTaskShockingPoliceInvestigate()
{
	m_iNumberTaskUses--;
}

void CTaskShockingPoliceInvestigate::CleanUp()
{
	CPed* pPed = GetPed();
	if (pPed)
	{
		CTaskMotionBase* pMotionTask = pPed->GetPrimaryMotionTaskIfExists();
		// Restore the ped to its default motion anims.
		if (pMotionTask)
		{
			pMotionTask->ResetOnFootClipSet();
		}
	}
}

aiTask*	CTaskShockingPoliceInvestigate::Copy() const {
	CTaskShockingPoliceInvestigate* pCopy = rage_new CTaskShockingPoliceInvestigate(m_ShockingEvent);
	if (pCopy)
	{
		pCopy->SetShouldPlayShockingSpeech(ShouldPlayShockingSpeech());
	}
	return pCopy;
}


CTask::FSM_Return CTaskShockingPoliceInvestigate::ProcessPreFSM()
{
	//Grab the ped.
	CPed* pPed = GetPed();

	// Allow certain lod modes
	pPed->GetPedAiLod().ClearBlockedLodFlag(CPedAILod::AL_LodPhysics);
	pPed->GetPedAiLod().ClearBlockedLodFlag(CPedAILod::AL_LodMotionTask);

	CTask::FSM_Return eReturn = CTaskShockingEvent::ProcessPreFSM();
	if (eReturn != FSM_Quit)
	{
		const COrder* pOrder = GetPed()->GetPedIntelligence()->GetOrder();
		if (pOrder && pOrder->GetIsValid())
		{
			eReturn = FSM_Quit;
		}
	}

	return eReturn;
}

aiTask::FSM_Return CTaskShockingPoliceInvestigate::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
	FSM_State(State_Start)
		FSM_OnUpdate
		return Start_OnUpdate();	
	
	FSM_State(State_GotoLocationInVehicle)
		FSM_OnEnter
			GotoLocationInVehicle_OnEnter();
		FSM_OnUpdate
			return GotoLocationInVehicle_OnUpdate();

	FSM_State(State_ExitVehicle)
		FSM_OnEnter
			ExitVehicle_OnEnter();
		FSM_OnUpdate
			return ExitVehicle_OnUpdate();

	FSM_State(State_GotoLocationOnFoot)
		FSM_OnEnter
			GotoLocationOnFoot_OnEnter();
		FSM_OnUpdate
			return GotoLocationOnFoot_OnUpdate();

	FSM_State(State_FaceDirection)
		FSM_OnEnter
			FaceDirection_OnEnter();
		FSM_OnUpdate
			return FaceDirection_OnUpdate();

	FSM_State(State_Investigate)
		FSM_OnEnter
			Investigate_OnEnter();
		FSM_OnUpdate
			return Investigate_OnUpdate();

	FSM_State(State_Finish)
		FSM_OnUpdate
			return FSM_Quit;
	FSM_End
}

aiTask::FSM_Return CTaskShockingPoliceInvestigate::Start_OnUpdate()
{
	CPed* pPed = GetPed();
	CVehicle* pVehicle = pPed->GetVehiclePedInside();

	if(pVehicle && pVehicle->GetVehicleType() == VEHICLE_TYPE_CAR )
	{
		SetState(State_GotoLocationInVehicle);
		return FSM_Continue;
	}
	else
	{
		SetState(State_GotoLocationOnFoot);
		return FSM_Continue;
	}
}


void CTaskShockingPoliceInvestigate::GotoLocationInVehicle_OnEnter()
{
	CPed* pPed = GetPed();
	CVehicle* pVehicle = pPed->GetVehiclePedInside();

	//If we start in a vehicle we might be get out of the vehicle and then be too far away from the incident for it to be sensed. 
	//Make sure if we stop and get out we still go to the incident. 
	m_uRunningFlags.SetFlag(RF_DisableCheckCanStillSense);

	//Pull over next to the ped if you're the driver. AssignVehicleTask can fail if the vehicle is a network clone
	if (pVehicle && pVehicle->IsDriver(pPed))
	{
		m_bVehicleTaskGiven = AssignVehicleTask();
	}
}

aiTask::FSM_Return CTaskShockingPoliceInvestigate::GotoLocationInVehicle_OnUpdate()
{
	CPed* pPed = GetPed();

	CVehicle* pVehicle = pPed->GetVehiclePedInside();
	
	if(!pVehicle)
	{
		SetState(State_GotoLocationOnFoot);
		return FSM_Continue;
	}

	if (pVehicle->IsDriver(pPed) && !m_bVehicleTaskGiven)
	{
		// keep trying to assign the vehicle task (the machine owning the vehicle will migrate the vehicle to us once they detect our ped is driving)
		m_bVehicleTaskGiven = AssignVehicleTask();		
	}
	else
	{
		CTaskVehicleMissionBase* pOtherActiveTask = pVehicle->GetIntelligence()->GetActiveTask();

		if (!pOtherActiveTask)
		{
			SetState(State_ExitVehicle);
			return FSM_Continue;
		}
	}

	return FSM_Continue;
}


void CTaskShockingPoliceInvestigate::ExitVehicle_OnEnter()
{
	CPed* pPed = GetPed();
	CVehicle* pVehicle = pPed->GetVehiclePedInside();
	//exit the vehicle
	if (pVehicle)
	{
		VehicleEnterExitFlags vehicleFlags;
		vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DontCloseDoor);
		SetNewTask(rage_new CTaskExitVehicle(pVehicle, vehicleFlags));
	}
}

aiTask::FSM_Return CTaskShockingPoliceInvestigate::ExitVehicle_OnUpdate()
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished) || !GetSubTask())
	{
		CPed* pPed = GetPed();
		static const float s_fExitDelay = 1.0f;
		if (pPed->GetIsInVehicle() && GetTimeInState() > s_fExitDelay)
		{
			SetFlag(aiTaskFlags::RestartCurrentState);
		}
		else
		{
			SetState(State_GotoLocationOnFoot);
		}
		return FSM_Continue;
	}

	return FSM_Continue;
}

void CTaskShockingPoliceInvestigate::GotoLocationOnFoot_OnEnter()
{
	// Start a movement task to go towards the center location.
	// If there is another ped walking there then stay a little behind him
	float fTargetDistance = m_ShockingEvent->GetTunables().m_GotoWatchRange - sm_Tunables.m_ExtraDistForGoto;
		
	fTargetDistance += m_iNumberTaskUses * fwRandom::GetRandomNumberInRange(0.5f, 1.2f);
	
	Vector3 center = VEC3V_TO_VECTOR3(GetCenterLocation());
	CTaskMoveFollowNavMesh* pMoveTask = rage_new CTaskMoveFollowNavMesh(sm_Tunables.m_MoveBlendRatioForFarGoto, center,fTargetDistance);
	if(center.Dist2(VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition())) > 5.0f)
	{
		pMoveTask->SetKeepMovingWhilstWaitingForPath(true);
	}
	pMoveTask->SetStopExactly(false);
	SetNewTask(rage_new CTaskComplexControlMovement(pMoveTask));
}

aiTask::FSM_Return CTaskShockingPoliceInvestigate::GotoLocationOnFoot_OnUpdate()
{
	CTaskMoveFollowNavMesh* pTask = static_cast<CTaskMoveFollowNavMesh *>(GetPed()->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH));
	if(pTask)
	{
		//Check if we are unable to find a route.
		if(pTask->IsUnableToFindRoute())
		{
			return FSM_Quit;
		}
	}

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		m_uRunningFlags.ClearFlag(RF_DisableCheckCanStillSense);
		SetState(State_FaceDirection);
		return FSM_Continue;
	}

	// Keep the movement task up to date with the target position, etc.
	const Vec3V center = GetCenterLocation();
	return StateGotoCommonUpdate(center);
}

void CTaskShockingPoliceInvestigate::FaceDirection_OnEnter()
{
	CPed* pPed = GetPed();

	//Check if there are any other peds watching the dead body
	Vector3 vFacePos(m_ShockingEvent->GetSourcePosition());
	
	const Vec3V	PedPos = GetCenterLocation();
	const CEntityScannerIterator entityList = pPed->GetPedIntelligence()->GetNearbyPeds();
	for(const CEntity* pEnt = entityList.GetFirst(); pEnt; pEnt = entityList.GetNext())
	{
		const CPed* pOtherPed = static_cast<const CPed*>(pEnt);
		if(pOtherPed->IsDead() || pOtherPed->IsFatallyInjured())
		{
			continue;
		}

		//Check they are close to the event
		const Vec3V otherPedPos = pOtherPed->GetTransform().GetPosition();
		const float fDiffSqr = MagSquared(PedPos - otherPedPos).Getf();
		if(fDiffSqr < 5.5f)
		{
			vFacePos = VEC3V_TO_VECTOR3(otherPedPos);
			m_bFacingIncident = false;
			break;
		}
	}

	const CEntity* pTgt = m_ShockingEvent->GetSourceEntity();
	CTaskTurnToFaceEntityOrCoord* pFaceTask;
	if(pTgt && m_bFacingIncident)
	{
		pFaceTask = rage_new CTaskTurnToFaceEntityOrCoord(pTgt);
	}
	else
	{
		pFaceTask = rage_new CTaskTurnToFaceEntityOrCoord(vFacePos);	
	}
	SetNewTask(pFaceTask);
}

aiTask::FSM_Return CTaskShockingPoliceInvestigate::FaceDirection_OnUpdate()
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Investigate);
	}

	return FSM_Continue;
}

void CTaskShockingPoliceInvestigate::Investigate_OnEnter()
{
	//cache ped
	CPed* pPed = GetPed();
	
	// Query the scenarioinfo.
	s32 iScenarioIndex;
	
	if (m_bFacingIncident)
	{
		static const atHashWithStringNotFinal scenarioName("CODE_HUMAN_POLICE_INVESTIGATE",0xC2256520);
		iScenarioIndex = SCENARIOINFOMGR.GetScenarioIndex(scenarioName);
	}
	else
	{
		static const atHashWithStringNotFinal scenarioName("CODE_HUMAN_POLICE_CROWD_CONTROL",0xB43F81BA);
		iScenarioIndex = SCENARIOINFOMGR.GetScenarioIndex(scenarioName);
	}

	// Create the task.
	CTask* pTask = rage_new CTaskUseScenario(iScenarioIndex, CTaskUseScenario::SF_StartInCurrentPosition | CTaskUseScenario::SF_CheckConditions | CTaskUseScenario::SF_CanExitForNearbyPavement);
	SetNewTask(pTask);	

	//Only set the timer if another police officer that is within range and already in the task
	bool bUseTimer = false;

	//Only use the timer for dead body events
	if(!m_ShockingEvent || m_ShockingEvent->GetEventType() != EVENT_SHOCKING_DEAD_BODY)
	{
		bUseTimer = true;
	}

	const Vec3V vEventPos(GetCenterLocation());
	const CEntityScannerIterator entityList = pPed->GetPedIntelligence()->GetNearbyPeds();
	for(const CEntity* pEnt = entityList.GetFirst(); pEnt; pEnt = entityList.GetNext())
	{
		const CPed* pOtherPed = static_cast<const CPed*>(pEnt);
		//Check they aren't a dead ped
		if(pOtherPed->IsDead() || pOtherPed->IsFatallyInjured())
		{
			continue;
		}

		//Check they are a cop
		if (!pOtherPed->IsLawEnforcementPed())
		{
			continue;
		}

		//Check they are closer to the event
		const float fDiffSqr = MagSquared(vEventPos - pOtherPed->GetTransform().GetPosition()).Getf();
		if(fDiffSqr > rage::square(10))
		{
			continue;
		}

		CTaskUseScenario* pTaskScenario = static_cast<CTaskUseScenario*>(pOtherPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_USE_SCENARIO));
		if(pTaskScenario)
		{
			//Closer cop
			bUseTimer = true;
			break;
		}
	}

	//Set the timer as we aren't the closest ped or the ped isn't dead 
	CEntity* pSourceEntity = m_ShockingEvent->GetSourceEntity();
	if(pSourceEntity && pSourceEntity->GetIsTypePed())
	{
		CPed* pEventPed = static_cast<CPed*>(pSourceEntity);
		if(pEventPed && !pEventPed->IsDead() && !pEventPed->IsFatallyInjured())
		{
			bUseTimer = true;
		}
	}

	if(bUseTimer)
	{
		const bool bUseHurryAwayTimes = false;
		const u32 uiRandomWatchTime = GetRandomWatchTimeMS(*m_ShockingEvent, bUseHurryAwayTimes);
		m_Timer.Set(fwTimer::GetTimeInMilliseconds(), uiRandomWatchTime);
	}
	else
	{
		//If we aren't quickly looking then set a default timer of 40 seconds
		m_Timer.Set(fwTimer::GetTimeInMilliseconds(), 40000);
	}

	if (m_ShockingEvent && ShouldPlayShockingSpeech())
	{
		SayShockingSpeech(m_ShockingEvent->GetTunables().m_ShockingSpeechChance);
	}
}

aiTask::FSM_Return CTaskShockingPoliceInvestigate::Investigate_OnUpdate()
{
	// Check if the timer expired (should be false if the timer hasn't been started).
	if(m_Timer.IsOutOfTime())
	{
		return FSM_Quit;
	}

	// If ped plans to stand indefinitely in this state
	if( !m_Timer.IsSet() )
	{
		PossiblyDestroyPedInvestigatingForever();
	}

	// If we are waiting on the timer, check if we should hurry up
	if (IsMyVehicleBlockingTraffic())
	{
		static const s32 iMAX_TIME_TO_WAIT_WHILE_BLOCKING_TRAFFIC_MIN_VAL = 2500u;
		static const s32 iMAX_TIME_TO_WAIT_WHILE_BLOCKING_TRAFFIC_MAX_VAL = 5000u;

		const s32 iTimeLeft	= m_Timer.GetTimeLeft();
		if (iTimeLeft > iMAX_TIME_TO_WAIT_WHILE_BLOCKING_TRAFFIC_MAX_VAL)
		{
			s32 iTimeToWait = fwRandom::GetRandomNumberInRange(iMAX_TIME_TO_WAIT_WHILE_BLOCKING_TRAFFIC_MIN_VAL, iMAX_TIME_TO_WAIT_WHILE_BLOCKING_TRAFFIC_MAX_VAL);
			m_Timer.Set(fwTimer::GetTimeInMilliseconds(), iTimeToWait);
		}
	}

	return FSM_Continue;
}

bool CTaskShockingPoliceInvestigate::IsMyVehicleBlockingTraffic() const
{
	bool bIsMyVehicleBlockingTraffic = false;

	const CPed* pPed = GetPed();
	taskAssert(pPed);

	CVehicle* pMyVehicle = GetPed()->GetMyVehicle();
	if (pMyVehicle)
	{
		CEntityScannerIterator vehicleList = pPed->GetPedIntelligence()->GetNearbyVehicles();
		for( CEntity* pEnt = vehicleList.GetFirst(); pEnt; pEnt = vehicleList.GetNext() )
		{
			CVehicle* pVehicle = static_cast<CVehicle*>(pEnt);
			CVehicleIntelligence* pVehicleIntelligence = pVehicle ? pVehicle->GetIntelligence() : NULL;

			if (pVehicleIntelligence && pVehicleIntelligence->m_pCarThatIsBlockingUs == pMyVehicle && pVehicleIntelligence->m_NumCarsBehindUs > 0)
			{
				bIsMyVehicleBlockingTraffic = true;
				break;
			}
		}
	}

	return bIsMyVehicleBlockingTraffic;
}

void CTaskShockingPoliceInvestigate::PossiblyDestroyPedInvestigatingForever()
{
	// Get local handle to this ped
	CPed* pPed = GetPed();

	// Check if the ped can be deleted
	const bool bDeletePedIfInCar = false; // prevent deletion in cars
	const bool bDoNetworkChecks = true; // do network checks (this is the default)
	if( pPed->CanBeDeleted(bDeletePedIfInCar, bDoNetworkChecks) )
	{
		// Get handle to local player ped
		const CPed* pLocalPlayerPed = CGameWorld::FindLocalPlayer();
		if( pLocalPlayerPed )
		{
			// Check distance to local player is beyond minimum threshold
			const ScalarV distToLocalPlayerSq_MIN = ScalarVFromF32( rage::square(sm_Tunables.m_MinDistFromPlayerToDeleteOffscreen) );
			const ScalarV distToLocalPlayerSq = DistSquared(pLocalPlayerPed->GetTransform().GetPosition(), pPed->GetTransform().GetPosition());
			if( IsGreaterThanAll(distToLocalPlayerSq, distToLocalPlayerSq_MIN) )
			{
				// Check if ped is offscreen
				const bool bCheckNetworkPlayersScreens = true;
				if( !pPed->GetIsOnScreen(bCheckNetworkPlayersScreens) )
				{
					// Check if the ped deletion timer was already set and is now expired
					if( m_OffscreenDeletePedTimer.IsOutOfTime() )
					{
						// Mark ped for deletion
						pPed->FlagToDestroyWhenNextProcessed();
					}
					// Check if the ped deletion timer needs to be set
					else if( !m_OffscreenDeletePedTimer.IsSet() )
					{
						// Roll a random time to delay
						const int iDurationMS = fwRandom::GetRandomNumberInRange((int)sm_Tunables.m_DeleteOffscreenTimeMS_MIN, (int)sm_Tunables.m_DeleteOffscreenTimeMS_MAX);

						// Set the deletion timer
						m_OffscreenDeletePedTimer.Set(fwTimer::GetTimeInMilliseconds(), iDurationMS);
					}

					// ped continues to pass conditions for removal
					return;
				}
			}
		}
	}

	// Stop the deletion timer in case it is active
	m_OffscreenDeletePedTimer.Unset();
}

Vec3V_Out CTaskShockingPoliceInvestigate::GetCenterLocation() const
{
	if (m_ShockingEvent)
	{
		return m_ShockingEvent->GetReactPosition();
	}
	else
	{
		return Vec3V(V_ZERO);
	}
}


void CTaskShockingPoliceInvestigate::SetMoveTaskWithAmbientClips(CTaskMove* pMoveTask)
{
	CTaskAmbientClips* pAmbientTask	= rage_new CTaskAmbientClips(CTaskAmbientClips::Flag_PlayIdleClips, CONDITIONALANIMSMGR.GetConditionalAnimsGroup("SHOCK_GOTO"));
	SetNewTask(rage_new CTaskComplexControlMovement(pMoveTask, pAmbientTask));
}

CTask::FSM_Return CTaskShockingPoliceInvestigate::StateGotoCommonUpdate(Vec3V_In gotoTgt)
{
	// Here, we keep the CTaskComplexControlMovement and CTaskMoveFollowNavMesh tasks up to date
	// with the current target.
	CPed* pPed = GetPed();
	CTask* pSubTask = GetSubTask();
	if(pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
	{
		CTaskComplexControlMovement* pCtrlTask = static_cast<CTaskComplexControlMovement*>(pSubTask);
		if(pCtrlTask->GetMoveTaskType() == CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH)
		{
			// Use CTaskComplexControlMovement::SetTarget() instead of CTaskMoveFollowNavMesh::SetTarget(),
			// as it appears to do some potentially useful stuff.
			const Vector3 tmp = VEC3V_TO_VECTOR3(gotoTgt);
			pCtrlTask->SetTarget(pPed, tmp);

			CTask* pMoveTask = pCtrlTask->GetRunningMovementTask();
			if(pMoveTask)
			{
				Assert(pMoveTask->GetTaskType() == CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH);
				CTaskMoveFollowNavMesh* pNavMeshTask = static_cast<CTaskMoveFollowNavMesh*>(pMoveTask);

				// Make sure we keep moving as we update the target position.
				pNavMeshTask->SetKeepMovingWhilstWaitingForPath(true);
			}
		}
	}

	return FSM_Continue;
}

bool CTaskShockingPoliceInvestigate::AssignVehicleTask()
{
	CPed* pPed = GetPed();
	CVehicle* pVehicle = pPed->GetVehiclePedInside();

	//Pull over next to the ped if you're the driver
	if (!pVehicle->IsNetworkClone())
	{
		Vector3 dummyDir(VEC3_ZERO);
		sVehicleMissionParams params;
		params.SetTargetPosition(VEC3V_TO_VECTOR3(GetCenterLocation()));
		aiTask *pCarTask = rage_new CTaskVehicleParkNew(params,dummyDir,CTaskVehicleParkNew::Park_PullOver, 0.175f);
		pVehicle->GetIntelligence()->AddTask(VEHICLE_TASK_TREE_PRIMARY, pCarTask, VEHICLE_TASK_PRIORITY_PRIMARY, false);
		return true;
	}

	return false;
}

#if !__FINAL
const char * CTaskShockingPoliceInvestigate::GetStaticStateName( s32 iState )
{
	taskAssert(iState >= State_Start && iState <= State_Finish);

	switch (iState)
	{
	case State_Start:					return "State_Start";
	case State_GotoLocationInVehicle:	return "State_GotoLocationInVehicle";
	case State_ExitVehicle:				return "State_ExitVehicle";
	case State_GotoLocationOnFoot:		return "State_GotoLocationOnFoot";
	case State_FaceDirection:			return "State_FaceDirection";
	case State_Investigate:				return "State_Investigate";
	case State_Finish:					return "State_Finish";
	default: taskAssert(0); 	
	}

	return "State_Invalid";
}

#endif 


//-------------------------------------------------------------------------
// CTaskShockingEventStopAndStare
//-------------------------------------------------------------------------

//tunable
CTaskShockingEventStopAndStare::Tunables CTaskShockingEventStopAndStare::sm_Tunables;

IMPLEMENT_RESPONSE_TASK_TUNABLES(CTaskShockingEventStopAndStare, 0xc9cad6e8);

CTaskShockingEventStopAndStare::CTaskShockingEventStopAndStare(const CEventShocking* event)
: CTaskShockingEvent(event)
{
	SetInternalTaskType(CTaskTypes::TASK_SHOCKING_EVENT_STOP_AND_STARE);
}


aiTask::FSM_Return CTaskShockingEventStopAndStare::ProcessPreFSM()
{
	const CPed* pPed = GetPed();

	// Quit out if the ped has left their vehicle.
	if (!(aiVerifyf(pPed->GetIsDrivingVehicle(), "CTaskShockingEventStopAndStare should only run if the ped is driving a vehicle.")))
	{
		return FSM_Quit;
	}

	return CTaskShockingEvent::ProcessPreFSM();
}


aiTask::FSM_Return CTaskShockingEventStopAndStare::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		FSM_State(State_Stopping)
			FSM_OnEnter
				Stopping_OnEnter();
			FSM_OnUpdate
				return Stopping_OnUpdate();
		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

void CTaskShockingEventStopAndStare::Stopping_OnEnter()
{
	CPed* pPed = GetPed();
	CVehicle* pVehicle = pPed->GetVehiclePedInside();
	if (m_ShockingEvent)
	{
		// See comments about network synchronization of CTaskBringVehicleToHalt in CTaskUseVehicleScenario::UsingScenario_OnEnter.
		aiTask* pCarTask = NULL;
		if(!NetworkInterface::IsGameInProgress())
		{
			const bool bUseHurryAwayTimes = false;
			pCarTask = rage_new CTaskBringVehicleToHalt(sm_Tunables.m_BringVehicleToHaltDistance, (int)(GetRandomWatchTimeMS(*m_ShockingEvent, bUseHurryAwayTimes) / 1000));
		}
		aiTask* pSubTask = rage_new CTaskAmbientLookAtEvent(m_ShockingEvent);
		if(pCarTask)
		{
			SetNewTask(rage_new CTaskControlVehicle(pVehicle, pCarTask, pSubTask));
		}
		else
		{
			SetNewTask(pSubTask);
		}
	}
}

aiTask::FSM_Return CTaskShockingEventStopAndStare::Stopping_OnUpdate()
{
	if (!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Finish);
	}
	return FSM_Continue;
}

#if !__FINAL

const char* CTaskShockingEventStopAndStare::GetStaticStateName(s32 iState)
{
	taskAssert(iState >= 0 && iState < kNumStates);

	switch (iState)
	{
		case State_Stopping:
			return "Stopping";
		case State_Finish:
			return "Finish";
		default:			
			taskAssert(0); return "Invalid";
	}
}
#endif	//!__FINAL

//-------------------------------------------------------------------------
// CTaskShockingNiceCarPicture
//-------------------------------------------------------------------------
CTaskShockingNiceCarPicture::CTaskShockingNiceCarPicture(const CEventShocking* event)
	: CTaskShockingEvent(event)
{
	SetInternalTaskType(CTaskTypes::TASK_SHOCKING_NICE_CAR_PICTURE);
}

aiTask::FSM_Return CTaskShockingNiceCarPicture::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

	FSM_State(State_TurnTowardsVehicle)
		FSM_OnEnter
			TurnTowardsVehicle_OnEnter();
	FSM_OnUpdate
		return TurnTowardsVehicle_OnUpdate();

	FSM_State(State_TakePicture)
		FSM_OnEnter
			TakePicture_OnEnter();
		FSM_OnUpdate
			return TakePicture_OnUpdate();

	FSM_State(State_Quit)
		FSM_OnUpdate
			return FSM_Quit;

	FSM_End
}

void CTaskShockingNiceCarPicture::TurnTowardsVehicle_OnEnter()
{
	CTaskTurnToFaceEntityOrCoord* pTurnToCarTask = rage_new CTaskTurnToFaceEntityOrCoord(m_ShockingEvent->GetSourceEntity());
	SetNewTask(pTurnToCarTask);
}

aiTask::FSM_Return CTaskShockingNiceCarPicture::TurnTowardsVehicle_OnUpdate()
{
	if (GetSubTask())
	{
		return FSM_Continue;
	}
	SetState(State_TakePicture);
	return FSM_Continue;
}

void CTaskShockingNiceCarPicture::TakePicture_OnEnter()
{
	const CConditionalAnimsGroup *pConditionalAnimsGroup = CONDITIONALANIMSMGR.GetConditionalAnimsGroup("WORLD_HUMAN_TOURIST_MOBILE_CAR");
	taskAssert(pConditionalAnimsGroup);

	u32 iFlags = CTaskAmbientClips::Flag_PlayBaseClip | CTaskAmbientClips::Flag_QuitAfterBase | rage::u32(CTaskAmbientClips::Flag_DontRandomizeBaseOnStartup);

	CTaskAmbientClips *pAmbientClipsTask = rage_new CTaskAmbientClips(iFlags, pConditionalAnimsGroup);
	SetNewTask(pAmbientClipsTask);
}

aiTask::FSM_Return CTaskShockingNiceCarPicture::TakePicture_OnUpdate()
{
	if (GetSubTask())
	{
		return FSM_Continue;
	}
	return FSM_Quit;
}

#if !__FINAL

const char* CTaskShockingNiceCarPicture::GetStaticStateName(s32 iState)
{
	taskAssert(iState >= 0 && iState < kNumStates);

	switch (iState)
	{
	case State_TurnTowardsVehicle:
		return "TurningTowardsVehicle";
	case State_TakePicture:
		return "TakingPicture";
	default:			
		taskAssert(0); return "Invalid";
	}
}

#endif	//!__FINAL

//-------------------------------------------------------------------------
// CTaskShockingEventThreatResponse
//-------------------------------------------------------------------------

CTaskShockingEventThreatResponse::CTaskShockingEventThreatResponse(const CEventShocking* event)
: CTaskShockingEvent(event)
, m_pThreat(NULL)
{
	SetInternalTaskType(CTaskTypes::TASK_SHOCKING_EVENT_THREAT_RESPONSE);
}

CTaskShockingEventThreatResponse::~CTaskShockingEventThreatResponse()
{

}

#if !__FINAL
const char* CTaskShockingEventThreatResponse::GetStaticStateName(s32 iState)
{
	Assert(iState >= State_Start && iState <= State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_ReactOnFoot",
		"State_ReactInVehicle",
		"State_ThreatResponse",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif // !__FINAL

CTask::FSM_Return CTaskShockingEventThreatResponse::ProcessPreFSM()
{
	CTask::FSM_Return nReturn = CTaskShockingEvent::ProcessPreFSM();
	if(nReturn == FSM_Quit)
	{
		return FSM_Quit;
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskShockingEventThreatResponse::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_ReactOnFoot)
			FSM_OnEnter
				ReactOnFoot_OnEnter();
			FSM_OnUpdate
				return ReactOnFoot_OnUpdate();

		FSM_State(State_ReactInVehicle)
			FSM_OnEnter
				ReactInVehicle_OnEnter();
			FSM_OnUpdate
				return ReactInVehicle_OnUpdate();

		FSM_State(State_ThreatResponse)
			FSM_OnEnter
				ThreatResponse_OnEnter();
			FSM_OnUpdate
				return ThreatResponse_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskShockingEventThreatResponse::Start_OnUpdate()
{
	//Disable the sanity check of entites.  They may go away if the source entity is an explosive.
	m_uRunningFlags.SetFlag(RF_DisableSanityCheckEntities);

	//Disable sense checks.
	m_uRunningFlags.SetFlag(RF_DisableCheckCanStillSense);

	//Cache the threat.  This may go away dynamically if the source entity is an explosive.
	m_pThreat = m_ShockingEvent->GetSourcePed();

	//Say the audio.
	SayShockingSpeech(m_ShockingEvent->GetTunables().m_ShockingSpeechChance);

	//Check if the ped is on foot.
	if(GetPed()->GetIsOnFoot())
	{
		SetState(State_ReactOnFoot);
	}
	//Check if the ped is in a vehicle.
	else if(GetPed()->GetIsInVehicle())
	{
		SetState(State_ReactInVehicle);
	}
	else
	{
		SetState(State_Finish);
	}

	return FSM_Continue;
}

void CTaskShockingEventThreatResponse::ReactOnFoot_OnEnter()
{
	//Create the task.
	CTaskShockingEventReact* pTask = rage_new CTaskShockingEventReact(m_ShockingEvent);
	pTask->SetShouldPlayShockingSpeech(false);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskShockingEventThreatResponse::ReactOnFoot_OnUpdate()
{
	//Check if the sub-task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_ThreatResponse);
	}

	return FSM_Continue;
}

void CTaskShockingEventThreatResponse::ReactInVehicle_OnEnter()
{
	//Look at the event.
	LookAtEvent();
}

CTask::FSM_Return CTaskShockingEventThreatResponse::ReactInVehicle_OnUpdate()
{
	//Check if we have exceeded the time.
	static dev_float s_fMinTimeInState = 0.5f;
	static dev_float s_fMaxTimeInState = 0.75f;
	float fTimeInState = GetPed()->GetRandomNumberInRangeFromSeed(s_fMinTimeInState, s_fMaxTimeInState);
	if(GetTimeInState() > fTimeInState)
	{
		SetState(State_ThreatResponse);
	}

	return FSM_Continue;
}

void CTaskShockingEventThreatResponse::ThreatResponse_OnEnter()
{
	//Create the task.
	CTaskThreatResponse* pTask = CTaskThreatResponse::CreateForTargetDueToEvent(GetPed(), m_pThreat, *m_ShockingEvent);
	if(!pTask)
	{
		return;
	}

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskShockingEventThreatResponse::ThreatResponse_OnUpdate()
{
	//Check if the sub-task has finished.
	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Finish);
	}

	return FSM_Continue;
}
