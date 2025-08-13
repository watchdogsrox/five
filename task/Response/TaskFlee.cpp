// File header
#include "Task/Response/TaskFlee.h"

// Framework headers
#include "ai/task/taskchannel.h"
#include "fwscene/stores/staticboundsstore.h"
#include "math/angmath.h"

// Game headers
#include "AI/AITarget.h"
#include "AI/debug/system/AIDebugLogManager.h"
#include "Animation/FacialData.h"
#include "Event/Events.h"
#include "Event/EventDamage.h"
#include "Event/EventShocking.h"
#include "Event/System/EventData.h"
#include "Game/ModelIndices.h"
#include "Network/General/NetworkUtil.h"
#include "Peds/FleeBehaviour.h"
#include "Peds/NavCapabilities.h"
#include "Peds/Ped.h"
#include "Peds/PedDebugVisualiser.h"
#include "Peds/PedIntelligence.h"
#include "Physics/WorldProbe/shapetestmgr.h"
#include "Physics/WorldProbe/shapetestcapsuledesc.h"
#include "Physics/WorldProbe/shapetestresults.h"
#include "Physics/WorldProbe/wpcommon.h"
#include "scene/world/GameWorld.h"
#include "script/script.h"
#include "script/script_cars_and_peds.h"
#include "Task/Combat/Cover/TaskSeekCover.h"
#include "Task/Combat/Cover/TaskCover.h"
#include "Task/Combat/TaskReact.h"
#include "Task/Combat/TaskThreatResponse.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/Default/TaskWander.h"
#include "Task/General/TaskSecondary.h"
#include "task/General/Phone/TaskCallPolice.h"
#include "Task/Motion/Locomotion/TaskFishLocomotion.h"
#include "Task/Motion/Locomotion/TaskMotionPed.h"
#include "Task/Movement/TaskCollisionResponse.h"
#include "Task/Movement/TaskFlyToPoint.h"
#include "Task/Movement/TaskNavMesh.h"
#include "Task/Response/TaskDuckAndCover.h"
#include "Task/Response/TaskReactAndFlee.h"
#include "Task/Scenario/Info/ScenarioInfo.h"
#include "Task/Scenario/ScenarioFinder.h"
#include "Task/Scenario/ScenarioManager.h"
#include "Task/Scenario/Types/TaskCowerScenario.h"
#include "Task/Scenario/Types/TaskUseScenario.h"
#include "Task/Vehicle/TaskCar.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Task/Vehicle/TaskExitVehicle.h"
#include "Task/Default/AmbientAnimationManager.h"
#include "vehicleAi/task/TaskVehicleCruise.h"
#include "vehicleAi/task/TaskVehicleFleeBoat.h"
#include "vehicleAi/task/TaskVehicleFlying.h"
#include "vehicleAi/task/TaskVehiclePark.h"
#include "vehicleAi/task/TaskVehicleTempAction.h"
#include "Vehicles/Metadata/VehicleMetadataManager.h"
#include "Vehicles/Metadata/VehicleSeatInfo.h"
#include "Vehicles/vehiclepopulation.h"
#include "Vfx/Misc/Fire.h"
#include "weapons/WeaponTypes.h"
#include "network/objects/entities/NetObjVehicle.h"

AI_OPTIMISATIONS()
NETWORK_OPTIMISATIONS()

////////////////////////////
// SMART FLEE			  //
////////////////////////////

// Flee Transition Tables
static CFleeOnFootTransitions s_FleeOnFootTransitions;
static CFleeSkiingTransitions s_FleeSkiingTransitions;
static CFleeInVehicleTransitions s_FleeInVehicleTransitions;
aiTaskStateTransitionTable*	CTaskSmartFlee::ms_fleeTransitionTables[CFleeBehaviour::FT_NumTypes] = 
{
	&s_FleeOnFootTransitions,
	&s_FleeSkiingTransitions,
	&s_FleeInVehicleTransitions
};

CFleeOnFootTransitions::CFleeOnFootTransitions()
{
}

void CFleeOnFootTransitions::Init()
{
	// State transitions only occur if the task has the conditional flags set

	// Transitions from start
	// If I'm in state_start and FF_OnFoot flag is set, then I will definitely move to state_fleeonfoot
	AddTransition(CTaskSmartFlee::State_Start,			CTaskSmartFlee::State_FleeOnFoot,				1.0f	, 0);

	// Transitions from fleeing
	AddTransition(CTaskSmartFlee::State_FleeOnFoot,		CTaskSmartFlee::State_SeekCover,				0.5f, FF_WillUseCover); 

	// Transitions from standing still
	AddTransition(CTaskSmartFlee::State_StandStill,		CTaskSmartFlee::State_ReturnToStartPosition,	1.0f, FF_ReturnToOriginalPosition);
	AddTransition(CTaskSmartFlee::State_StandStill,		CTaskSmartFlee::State_FleeOnFoot,				1.0f, 0);

	// Transitions from having taken off the skis
	AddTransition(CTaskSmartFlee::State_TakeOffSkis,	CTaskSmartFlee::State_FleeOnFoot,		1.0f, 0);
}

CFleeSkiingTransitions::CFleeSkiingTransitions()
{
}

void CFleeSkiingTransitions::Init()
{
	// Transitions from start
	AddTransition(CTaskSmartFlee::State_Start,			CTaskSmartFlee::State_FleeOnSkis,		0.95f, 0	);	// More likely to flee on skis
	AddTransition(CTaskSmartFlee::State_Start,			CTaskSmartFlee::State_TakeOffSkis,		0.05f, 0	);	// than to take them off before fleeing

	// Transitions from fleeing
	//AddTransition(CTaskSmartFlee::State_FleeOnSkis,		CTaskSmartFlee::State_TakeOffSkis,		0.2f, 0);
}

CFleeInVehicleTransitions::CFleeInVehicleTransitions()
{

}

void CFleeInVehicleTransitions::Init()
{

}

// NOTE[egarcia]: ms_fMaxStopDistance MUST be changed together with CClonedControlTaskSmartFleeInfo::iMAX_STOP_DISTANCE_SIZE 
const float CTaskSmartFlee::ms_fMaxStopDistance				= 10000.0f;
// NOTE[egarcia]: ms_fMaxIgnorePreferPavementsTimeLeft MUST be changed together with CClonedControlTaskSmartFleeInfo::iMIN_FLEEING_TIME_TO_PREFER_PAVEMENTS_SIZE 
const float CTaskSmartFlee::ms_fMaxIgnorePreferPavementsTime = 1000.0f;

bank_float	CTaskSmartFlee::ms_fStopDistance				= -1.0f;
bank_s32	CTaskSmartFlee::ms_iFleeTime					= -1;
bank_u32	CTaskSmartFlee::ms_uiRecoveryTime				= 2000;
bank_u32	CTaskSmartFlee::ms_uiEntityPosCheckPeriod		= 1000;
//bank_float	CTaskSmartFlee::ms_fEntityPosChangeThreshold	= 1.0f;
bank_u32	CTaskSmartFlee::ms_uiVehicleCheckPeriod			= 4000;
bank_float	CTaskSmartFlee::ms_fMinDistAwayFromThreatBeforeSeekingCover	= 25.0f;  
bank_s32	CTaskSmartFlee::ms_iMinPanicAudioRepeatTime		= 0;
bank_s32	CTaskSmartFlee::ms_iMaxPanicAudioRepeatTime		= 10000;

void CTaskSmartFlee::InitClass()
{
	InitTransitionTables();

	sm_PedsFleeingOutOfPavementManager.Init(24u, 200u, 2u, PedFleeingOutOfPavementIsValidCB, PedFleeingOutOfPavementOnOutOfSightCB);
}

void CTaskSmartFlee::ShutdownClass()
{
	sm_PedsFleeingOutOfPavementManager.Done();
}

void CTaskSmartFlee::UpdateClass()
{
	sm_PedsFleeingOutOfPavementManager.Process();
}

void CTaskSmartFlee::InitTransitionTables()
{
	for (s32 i=0; i<CFleeBehaviour::FT_NumTypes; i++)
	{
		ms_fleeTransitionTables[i]->Clear();
		ms_fleeTransitionTables[i]->Init();
	}
}

atFixedArray<RegdPed, 8> CTaskSmartFlee::sm_aPedsExitingTheirVehicle;
CPedsWaitingToBeOutOfSightManager CTaskSmartFlee::sm_PedsFleeingOutOfPavementManager;

CTaskSmartFlee::Tunables CTaskSmartFlee::sm_Tunables;

IMPLEMENT_RESPONSE_TASK_TUNABLES(CTaskSmartFlee, 0xadf50897);

CTaskSmartFlee::CTaskSmartFlee(
   const CAITarget& target, 
   const float fStopDistance,
   const int iFleeTime, 
   const int iEntityPosCheckPeriod, 
   const float UNUSED_PARAM(fEntityPosChangeThreshold),
   const bool bKeepMovingWhilstWaitingForFleePath,
   const bool bResumeFleeingAtEnd
   )
   : 
m_FleeTarget(target),
m_vStartPos(VEC3_ZERO),
m_iFleeTime(iFleeTime),
m_iFleeDir(0),
m_FleeFlags(0),
m_uConfigFlags(0),
m_uConfigFlagsForVehicle(0),
m_uExecConditions(0),
m_uRunningFlags(0),
m_iEntityPosCheckPeriod(iEntityPosCheckPeriod),
//m_fEntityPosChangeThreshold(fEntityPosChangeThreshold),
m_fMoveBlendRatio(MOVEBLENDRATIO_SPRINT),
m_fFleeInVehicleIntensity(1.0f),
m_fFleeInVehicleTime(0.0f),
m_uTimeToLookAtTargetNext(0),
m_uTimeWeLastCheckedIfWeShouldStopWaitingBeforeExitingTheVehicle(0),
m_bKeepMovingWhilstWaitingForFleePath(bKeepMovingWhilstWaitingForFleePath),
m_priorityEvent(EVENT_NONE),
m_bNewPriorityEvent(false),
m_bResumeFleeingAtEnd(bResumeFleeingAtEnd),
m_bHaveStopped(false),
m_bQuitTaskIfOutOfRange(false),
m_vLastTargetPosition(V_ZERO),
m_OldTargetPosition(Vector3(0.0f, 0.0f, 0.0f)),
m_bForcePreferPavements(false),
m_fIgnorePreferPavementsTimeLeft(-1.0f),
m_bNeverLeavePavements(false),
m_bPedStartedInWater(false),
m_bUseLargerSearchExtents(false),
m_bPullBackFleeTargetFromPed(false),
m_bDontClaimProp(false),
m_bQuitIfOutOfRange(false),
m_bConsiderRunStartForPathLookAhead(false),
m_bKeepMovingOnlyIfMoving(false),
m_SuppressHandsUp(false),
m_bCanCheckForVehicleExitInCower(false),
m_pExistingMoveTask(NULL),
m_HandsUpTimer(sm_Tunables.m_TimeBetweenHandsUpChecks),
m_ExitVehicleDueToRouteTimer(0.0f),
m_bOutgoingMigratedWhileCowering(false),
m_bIncomingMigratedWhileCowering(false),
m_fDrivingParallelToTargetTime(0.0f)
{
#if __ASSERT
	Vector3 vFleePos;
	if(m_FleeTarget.GetPosition(vFleePos))
	{
		//Grab the target entity position.
		Vector3 vTargetEntityPosition = m_FleeTarget.GetEntity() ? VEC3V_TO_VECTOR3(m_FleeTarget.GetEntity()->GetTransform().GetPosition()) : VEC3_ZERO;
		
		//Assert the flee position is valid.
		Assertf(rage::FPIsFinite(vFleePos.x) && rage::FPIsFinite(vFleePos.y) && rage::FPIsFinite(vFleePos.z), "Flee position is invalid.");
		Assertf(vFleePos.x > WORLDLIMITS_XMIN && vFleePos.y > WORLDLIMITS_YMIN && vFleePos.z > WORLDLIMITS_ZMIN, "Flee position is outside world limits (%.2f, %.2f, %.2f). Target mode: %d. Target entity position: (%.2f, %.2f, %.2f).", vFleePos.x, vFleePos.y, vFleePos.z, m_FleeTarget.GetMode(), vTargetEntityPosition.x, vTargetEntityPosition.y, vTargetEntityPosition.z);
		Assertf(vFleePos.x < WORLDLIMITS_XMAX && vFleePos.y < WORLDLIMITS_YMAX && vFleePos.z < WORLDLIMITS_ZMAX, "Flee position is outside world limits (%.2f, %.2f, %.2f). Target mode: %d. Target entity position: (%.2f, %.2f, %.2f).", vFleePos.x, vFleePos.y, vFleePos.z, m_FleeTarget.GetMode(), vTargetEntityPosition.x, vTargetEntityPosition.y, vTargetEntityPosition.z);
		Assertf(!vFleePos.IsClose(VEC3_ZERO, 0.1f), "Ped is fleeing the origin");
		
		m_OldTargetPosition = vFleePos;
	}
#endif

	m_uTimeStartedFleeing = fwTimer::GetTimeInMilliseconds();

	SetStopDistance(fStopDistance);

	if (m_iFleeTime != -1)
	{
		m_fleeTimer.Set(fwTimer::GetTimeInMilliseconds(),m_iFleeTime);
	}

	SetInternalTaskType(CTaskTypes::TASK_SMART_FLEE);

	m_LookRequest.SetHashKey(ATSTRINGHASH("FleeLookAt", 0xBB32309D));
	m_LookRequest.SetTurnRate(LOOKIK_TURN_RATE_FAST);
	m_LookRequest.SetRequestPriority(CIkManager::IK_LOOKAT_HIGH);
	m_LookRequest.SetBlendInRate(LOOKIK_BLEND_RATE_FAST);
	m_LookRequest.SetBlendOutRate(LOOKIK_BLEND_RATE_FAST);
	m_LookRequest.SetFlags(0);

	// Enable neck component by default to go along with head and eyes
	m_LookRequest.SetRotLimNeck(LOOKIK_ROT_ANGLE_YAW, LOOKIK_ROT_LIM_WIDE);
	m_LookRequest.SetRotLimNeck(LOOKIK_ROT_ANGLE_PITCH, LOOKIK_ROT_LIM_WIDE);

	// Use facing direction mode
	m_LookRequest.SetRefDirHead(LOOKIK_REF_DIR_FACING);
	m_LookRequest.SetRefDirNeck(LOOKIK_REF_DIR_FACING);

	m_screamTimer.Set(fwTimer::GetTimeInMilliseconds(), GetScreamTimerDuration());
}

CTaskSmartFlee::~CTaskSmartFlee()
{

}

bool CTaskSmartFlee::AccelerateInVehicleForEvent(const CPed* pPed, const CEvent& rEvent)
{
	//Ensure the ped is valid.
	if(!pPed)
	{
		return false;
	}

	//Ensure the ped is driving a vehicle.
	if(!pPed->GetIsDrivingVehicle())
	{
		return false;
	}

	//Check the event type.
	switch(rEvent.GetEventType())
	{
		case EVENT_GUN_AIMED_AT:
		case EVENT_SHOCKING_SEEN_MELEE_ACTION:
		case EVENT_SHOT_FIRED:
		case EVENT_SHOT_FIRED_BULLET_IMPACT:
		case EVENT_SHOT_FIRED_WHIZZED_BY:
		case EVENT_VEHICLE_DAMAGE_WEAPON:
		{
			return true;
		}
		default:
		{
			return false;
		}
	}
}

bool CTaskSmartFlee::CanCallPoliceDueToEvent(const CPed* pTarget, const CEvent& rEvent)
{
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
	}

	//Check the event type.
	switch(rEvent.GetEventType())
	{
		case EVENT_SHOT_FIRED:
		case EVENT_SHOT_FIRED_BULLET_IMPACT:
		case EVENT_SHOT_FIRED_WHIZZED_BY:
		{
			//Check if the source is a player.
			const CPed* pSource = rEvent.GetSourcePed();
			if(pSource && pSource->IsPlayer())
			{
				//Check if the chances are valid.
				static dev_float s_fChances = 0.20f;
				if(fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < s_fChances)
				{
					return true;
				}
			}

			return false;
		}
		default:
		{
			return false;
		}
	}
}

CTaskMoveFollowNavMesh* CTaskSmartFlee::CreateMoveTaskForFleeOnFoot(Vec3V_In vTarget)
{
	//Note: Any parameters set here should be common to the *vast* majority of flee tasks.
	//		Extra parameters can always be set later on.

	//Create the params.
	CNavParams navParams;
	navParams.m_vTarget = VEC3V_TO_VECTOR3(vTarget);
	navParams.m_fMoveBlendRatio = MOVEBLENDRATIO_RUN;
	navParams.m_bFleeFromTarget = true;
	navParams.m_fFleeSafeDistance = 99999.9f;
	navParams.m_fTargetRadius = 0.01f;

	//Create the task.
	CTaskMoveFollowNavMesh* pTask = rage_new CTaskMoveFollowNavMesh(navParams);

	return pTask;
}

bool CTaskSmartFlee::ReverseInVehicleForEvent(const CPed* pPed, const CEvent& rEvent)
{
	//Ensure the ped is valid.
	if(!pPed)
	{
		return false;
	}

	//Ensure the ped is driving a vehicle.
	if(!pPed->GetIsDrivingVehicle())
	{
		return false;
	}

	//Check the event type.
	switch(rEvent.GetEventType())
	{
		case EVENT_GUN_AIMED_AT:
		case EVENT_SHOT_FIRED:
		case EVENT_SHOT_FIRED_BULLET_IMPACT:
		case EVENT_SHOT_FIRED_WHIZZED_BY:
		case EVENT_SHOCKING_SEEN_MELEE_ACTION:
		case EVENT_VEHICLE_DAMAGE_WEAPON:
		{
			return true;
		}
		default:
		{
			return false;
		}
	}
}

bool CTaskSmartFlee::ShouldCowerDueToEvent(const CPed* pPed, const CEvent& rEvent)
{
	//Ensure the ped is valid.
	if(!pPed)
	{
		return false;
	}

	//Ensure the event type is valid.
	switch(rEvent.GetEventType())
	{
		case EVENT_SHOCKING_GUN_FIGHT:
		case EVENT_SHOCKING_GUNSHOT_FIRED:
		case EVENT_SHOT_FIRED:
		case EVENT_SHOT_FIRED_BULLET_IMPACT:
		case EVENT_SHOT_FIRED_WHIZZED_BY:
		{
			break;
		}
		default:
		{
			return false;
		}
	}

	//Ensure the ped is inside an interior.
	if(pPed->GetPortalTracker()->GetInteriorNameHash() == 0)
	{
		return false;
	}

	//Ensure the source ped is valid.
	const CPed* pSourcePed = rEvent.GetSourcePed();
	if(!pSourcePed)
	{
		return false;
	}

	//Ensure the source is not in the same interior.
	if(pSourcePed->GetPortalTracker()->GetInteriorNameHash() == pPed->GetPortalTracker()->GetInteriorNameHash())
	{
		return false;
	}

	return true;
}

bool CTaskSmartFlee::ShouldCowerInsteadOfFleeDueToTarget() const
{
	const CPed* pPed = GetPed();
	taskAssert(pPed);

	const CEntity* pTargetEntity = m_FleeTarget.GetEntity();
	const CPed* pTargetPed = pTargetEntity && pTargetEntity->GetIsTypePed() ? SafeCast(const CPed, pTargetEntity) : NULL;
	const bool bShouldCowerFromTarget = pTargetPed && IsInoffensiveTarget(pTargetPed) && !pTargetPed->GetPedResetFlag(CPED_RESET_FLAG_InflictedDamageThisFrame);

	return bShouldCowerFromTarget && pPed->GetIsInVehicle() && !pPed->GetIsDrivingVehicle() && !ShouldFleeOurOwnVehicle();
}

bool CTaskSmartFlee::ShouldEnterLastUsedVehicleImmediatelyIfCloserThanTarget(const CPed* pPed, const CEvent& UNUSED_PARAM(rEvent))
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

	//Ensure the ped is a civilian.
	if(!pPed->IsCivilianPed())
	{
		return false;
	}

	return true;
}

bool CTaskSmartFlee::ShouldExitVehicleDueToEvent(const CPed* pPed, const CPed* pTarget, const CEvent& rEvent)
{
	taskAssert(pPed);
	taskAssert(pTarget);

	if (pPed->GetIsInVehicle() 
		&& rEvent.GetEventType() == EVENT_GUN_AIMED_AT
		&& CTaskPlayerOnFoot::IsAPlayerCopInMP(*pTarget)
		&& !pPed->IsGangPed()
		&& !pPed->ShouldBehaveLikeLaw()
		&& pPed->PopTypeIsRandom())
	{
		return true;
	}

	// Non-mission peds who aren't the driver try and get out of the boat when someone steps onto it.
	CVehicle* pVehicle = pPed->GetVehiclePedInside();
	if (pVehicle && pVehicle->InheritsFromBoat()
		&& rEvent.GetEventType() == EVENT_PED_ENTERED_MY_VEHICLE
		&& pVehicle->GetDriver() != pPed
		&& !pPed->PopTypeIsMission()
		&& !pPed->IsLawEnforcementPed())
	{
		return true;
	}

	return false;
}

bool CTaskSmartFlee::ShouldNeverEnterWaterDueToEvent(const CEvent& rEvent)
{
	//Check the event type.
	switch(rEvent.GetEventType())
	{
		case EVENT_AGITATED_ACTION:
		{
			return true;
		}
		default:
		{
			return false;
		}
	}
}

float CTaskSmartFlee::GetTimeFleeingInVehicle() const
{
	return (IsFleeingInVehicle() ? GetTimeInState() : 0.0f);
}

float CTaskSmartFlee::GetTimeFleeingOnFoot() const
{
	return (IsFleeingOnFoot() ? GetTimeInState() : 0.0f);
}

bool CTaskSmartFlee::IsFleeingInVehicle() const
{
	return (GetState() == State_FleeInVehicle);
}

bool CTaskSmartFlee::IsFleeingOnFoot() const
{
	return (GetState() == State_FleeOnFoot);
}

void CTaskSmartFlee::OnCarUndriveable(const CEventCarUndriveable& UNUSED_PARAM(rEvent))
{
	//Set the flag.
	m_uRunningFlags.SetFlag(RF_CarIsUndriveable);
}

// PURPOSE: Check if the target entity is valid (Eg: not a dead player)
bool CTaskSmartFlee::IsValidTargetEntity(const CEntity* pTargetEntity) const
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
void CTaskSmartFlee::OnTargetEntityNoLongerValid()
{
	taskAssert(!IsCloseAll(m_vLastTargetPosition, Vec3V(V_ZERO), ScalarVFromF32(SMALL_FLOAT)));

	m_FleeTarget.SetPosition(VEC3V_TO_VECTOR3(m_vLastTargetPosition));

	// Stop after a few meters
	const float fTargetDistance = Dist(GetPed()->GetTransform().GetPosition(), m_vLastTargetPosition).Getf();
	const float fStopDistanceLeft = m_fStopDistance - fTargetDistance;
	if (fStopDistanceLeft > 0.0f)
	{
		const float fNewStopDistance = fTargetDistance + Min(fStopDistanceLeft, fwRandom::GetRandomNumberInRange(sm_Tunables.m_TargetInvalidatedMaxStopDistanceLeftMinVal, sm_Tunables.m_TargetInvalidatedMaxStopDistanceLeftMaxVal));
		SetStopDistance(fNewStopDistance);
	}
}

bool CTaskSmartFlee::SetSubTaskForOnFoot(CTask* pSubTask)
{
	//Ensure we are fleeing on foot.
	if(taskVerifyf(IsFleeingOnFoot(), "Unable to set the sub-task for on foot."))
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

bool CTaskSmartFlee::AllowedToFleeOnRoads(const CVehicle& veh)
{
	// Don't let airplanes flee on roads.
	return !veh.InheritsFromPlane();
}

float CTaskSmartFlee::CalculatePositionUpdateDelta(CPed * pPed) const
{
	static dev_float fMinDelta = 4.0f;
	static dev_float fMaxDelta = 20.0f;
	static dev_float fMinFromTarget = 4.0f;
	static dev_float fMaxFromTarget = 100.0f;

	Vector3 vFleeTarget;
	if(m_FleeTarget.GetPosition(vFleeTarget))
	{
		const Vector3 vDiff = vFleeTarget - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		const float fDist = Clamp(vDiff.Mag(), fMinFromTarget, fMaxFromTarget);
		const float s = (fDist <= fMinFromTarget)  ?
							0.0f : (fDist >= fMaxFromTarget) ?
								1.0f : (fDist / fMaxFromTarget);
		return fMinDelta + ((fMaxDelta-fMinDelta) * s);
	}

	return fMaxDelta;
}

bool CTaskSmartFlee::CanAccelerateInVehicle() const
{
	//Ensure we have not accelerated.
	if(m_uRunningFlags.IsFlagSet(RF_HasAcceleratedInVehicle))
	{
		return false;
	}

	//Ensure accelerate is not disabled.
	if(GetPed()->GetPedIntelligence()->GetFleeBehaviour().GetFleeFlags().IsFlagSet(CFleeBehaviour::BF_DisableAccelerateInVehicle))
	{
		return false;
	}

	//Ensure the ped is driving a vehicle.
	if(!GetPed()->GetIsDrivingVehicle())
	{
		return false;
	}

	//Ensure we aren't moving too quickly.
	static dev_float s_fMaxSpeed = 10.0f;
	float fSpeedSq = GetPed()->GetVehiclePedInside()->GetVelocity().XYMag2();
	float fMaxSpeedSq = square(s_fMaxSpeed);
	if(fSpeedSq > fMaxSpeedSq)
	{
		return false;
	}

	//Check if the target is a ped.
	const CPed* pTargetPed = GetPedWeAreFleeingFrom();
	if(pTargetPed)
	{
		//Check if the target ped is in a vehicle.
		if(pTargetPed->GetIsInVehicle())
		{
			//Ensure the target isn't moving quickly.
			static dev_float s_fMaxTargetSpeed = 5.0f;
			float fTargetSpeedSq = pTargetPed->GetVehiclePedInside()->GetVelocity().XYMag2();
			float fMaxTargetSpeedSq = square(s_fMaxTargetSpeed);
			if(fTargetSpeedSq > fMaxTargetSpeedSq)
			{
				return false;
			}
		}

		if (IsInoffensiveTarget(pTargetPed) && !sm_Tunables.m_CanAccelerateAgainstInoffensiveThreat)
		{
			return false;
		}
	}

	//Grab the target position.
	Vec3V vTargetPosition;
	m_FleeTarget.GetPosition(RC_VECTOR3(vTargetPosition));

	//Ensure the target is in front of us.
	static const float s_fMinDot		= 0.707f;
	static const float s_fMaxDistance	= 20.0f;
	if(!IsPositionWithinConstraints(*GetPed()->GetVehiclePedInside(), vTargetPosition, s_fMinDot, s_fMaxDistance))
	{
		return false;
	}

	// The ped cannot accelerate if the vehicle is not locally owned and controllable
	if (NetworkUtils::IsNetworkCloneOrMigrating(GetPed()->GetMyVehicle()))
	{
		return false;
	}

	return true;
}

bool CTaskSmartFlee::CanCallPolice() const
{
	//Ensure the config flag is set.
	if(!m_uConfigFlags.IsFlagSet(CF_CanCallPolice))
	{
		return false;
	}

	//Ensure the running flag is not set.
	if(m_uRunningFlags.IsFlagSet(RF_HasCalledPolice))
	{
		return false;
	}

	//Ensure we can call the police.
	if(!CTaskCallPolice::CanMakeCall(*GetPed(), GetPedWeAreFleeingFrom()))
	{
		return false;
	}

	//Calculate the time we have been fleeing.
	float fTimeSpentFleeing;
	if(IsFleeingOnFoot())
	{
		fTimeSpentFleeing = GetTimeFleeingOnFoot();
	}
	else if(IsFleeingInVehicle())
	{
		fTimeSpentFleeing = GetTimeFleeingInVehicle();
	}
	else
	{
		return false;
	}

	//Calculate the time spent fleeing to call the police.
	float fMinTimeSpentFleeingToCallPolice;
	float fMaxTimeSpentFleeingToCallPolice;
	if(!m_uConfigFlags.IsFlagSet(CF_OnlyCallPoliceIfChased))
	{
		static dev_float s_fMinTime = 1.0f;
		static dev_float s_fMaxTime = 3.0f;
		fMinTimeSpentFleeingToCallPolice = s_fMinTime;
		fMaxTimeSpentFleeingToCallPolice = s_fMaxTime;
	}
	else
	{
		static dev_float s_fMinTime = 15.0f;
		static dev_float s_fMaxTime = 60.0f;
		fMinTimeSpentFleeingToCallPolice = s_fMinTime;
		fMaxTimeSpentFleeingToCallPolice = s_fMaxTime;
	}

	//Ensure the time spent fleeing to call the police has been exceeded.
	float fTimeSpentFleeingToCallPolice = GetPed()->GetRandomNumberInRangeFromSeed(fMinTimeSpentFleeingToCallPolice, fMaxTimeSpentFleeingToCallPolice);
	if(fTimeSpentFleeing < fTimeSpentFleeingToCallPolice)
	{
		return false;
	}

	//Check if we only want to call the cops if we've been chased.
	if(m_uConfigFlags.IsFlagSet(CF_OnlyCallPoliceIfChased))
	{
		//Presumably at this point we've been fleeing for a while.
		//If the target is still close, consider ourselves chased.

		//Ensure the target position is valid.
		Vec3V vTargetPosition;
		if(!m_FleeTarget.GetPosition(RC_VECTOR3(vTargetPosition)))
		{
			return false;
		}

		//Ensure the distance is valid.
		ScalarV scDistSq = DistSquared(GetPed()->GetTransform().GetPosition(), vTargetPosition);
		static dev_float s_fMaxDistance = 30.0f;
		ScalarV scMaxDistSq = ScalarVFromF32(square(s_fMaxDistance));
		if(IsGreaterThanAll(scDistSq, scMaxDistSq))
		{
			return false;
		}
	}

	return true;
}

bool CTaskSmartFlee::CanCower() const
{
	//Grab the ped.
	const CPed* pPed = GetPed();

	//Ensure the task flag is not set.
	if(pPed->GetTaskData().GetIsFlagSet(CTaskFlags::DisableCower))
	{
		return false;
	}

	//Ensure the behavior flag is not set.
	if(pPed->GetPedIntelligence()->GetFleeBehaviour().GetFleeFlags().IsFlagSet(CFleeBehaviour::BF_DisableCower))
	{
		return false;
	}

	if(m_uExecConditions.IsFlagSet(EC_DisableCower))
	{
		return false;
	}

	//Ensure we aren't swimming.
	if(pPed->GetIsSwimming())
	{
		return false;
	}

	//Ensure we are not in water.
	if(pPed->m_Buoyancy.GetStatus() != NOT_IN_WATER)
	{
		return false;
	}

	if (pPed->GetIsInVehicle())
	{
		const CVehicle* pVehicle = pPed->GetVehiclePedInside();
		if(pVehicle->InheritsFromPlane() && !pVehicle->HasContactWheels())
		{
			return false;
		}
	}

	return true;
}

bool CTaskSmartFlee::CanExitVehicle() const
{
	//The first section is for conditions in which the ped can never exit the vehicle.

	//Ensure the ped is in a vehicle.
	const CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
	if(!pVehicle)
	{
		return false;
	}

	//If our vehicle is a plane without contact wheels
	if(pVehicle->InheritsFromPlane() && !pVehicle->HasContactWheels())
	{
		return false;
	}

	//Don't exit the vehicle if we are told to flee from combat in this vehicle
	if(pVehicle->FleesFromCombat())
	{
		return false;
	}

	//Ensure the seat index is valid.
	s32 iSeatIndex = pVehicle->GetSeatManager()->GetPedsSeatIndex(GetPed());
	if(!pVehicle->IsSeatIndexValid(iSeatIndex))
	{
		return false;
	}

	//Ensure the entry index is valid.
	s32 iEntryIndex = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(iSeatIndex, pVehicle);
	if(!pVehicle->IsEntryIndexValid(iEntryIndex))
	{
		return false;
	}
	
	//Ensure abnormal exits are not disabled.
	bool bShouldCheckForAbnormalExits = true;

#if __BANK
	TUNE_GROUP_BOOL(VEHICLE_EXIT_TUNE, bForceFleeInAnimSeats,false);
	if ((NetworkInterface::IsGameInProgress() && GetPed()->PopTypeIsMission()) || bForceFleeInAnimSeats)
	{
#else
	if (NetworkInterface::IsGameInProgress() && GetPed()->PopTypeIsMission())
	{
#endif //__BANK
		// If we have an animated entry / exit
		if (pVehicle->GetEntryInfo(iEntryIndex)->GetSPEntryAllowedAlso())
		{
			bShouldCheckForAbnormalExits = false;
		}
	}

	if (bShouldCheckForAbnormalExits)
	{
		const CVehicleSeatAnimInfo* pVehicleSeatAnimInfo = CVehicleMetadataMgr::GetSeatAnimInfoFromPed(GetPed());
		if(pVehicleSeatAnimInfo && pVehicleSeatAnimInfo->GetDisableAbnormalExits())
		{
			return false;
		}
	}

	//Ensure the ped can leave the vehicle.
	if(!GetPed()->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanLeaveVehicle))
	{
		return false;
	}

	//Ensure exit vehicle has not been disabled.
	if(GetPed()->GetPedIntelligence()->GetFleeBehaviour().GetFleeFlags().IsFlagSet(CFleeBehaviour::BF_DisableExitVehicle))
	{
		return false;
	}

	//Ensure we are in water, or have collision around us.
	bool bIsInWater = (pVehicle->m_Buoyancy.GetStatus() != NOT_IN_WATER);
	if(!bIsInWater && !g_StaticBoundsStore.GetBoxStreamer().HasLoadedAboutPos(pVehicle->GetTransform().GetPosition(), fwBoxStreamerAsset::FLAG_STATICBOUNDS_MOVER))
	{
		return false;
	}

	//This next section is conditions where the ped will never be able to exit the vehicle, UNLESS the vehicle is undriveable.
	bool allowExitsEvenIfDisabled = IsCarUndriveable() || IsVehicleStuck();

	if(!allowExitsEvenIfDisabled)
	{
		// If we happen to be a vehicle that doesn't support fleeing on roads, and we are not
		// in contact with the ground, allow exits anyway. This is done for airplanes now.
		// If they are in air, the FleeInVehicle state will give them a good flee task,
		// so we don't need to do it in that case (in fact, we probably would have returned
		// false already).
		// Also, we don't do this if RF_Cower is set, in that case we have decided that we don't
		// want to exit, which is used for really large planes.
		if(!AllowedToFleeOnRoads(*pVehicle) && !m_uRunningFlags.IsFlagSet(RF_Cower) && pVehicle->HasContactWheels())
		{
			allowExitsEvenIfDisabled = true;
		}
	}

	if(!allowExitsEvenIfDisabled)
	{
		//Ensure the config flag is not set.
		if(m_uConfigFlags.IsFlagSet(CF_DisableExitVehicle))
		{
			return false;
		}

		//Ensure the flee flag is not set.
		if(IsFlagSet(DisableVehicleExit))
		{
			return false;
		}
	}

	return true;
}

bool CTaskSmartFlee::CanPutHandsUp() const
{
	//Ensure the flag is set.
	if(!m_uConfigFlags.IsFlagSet(CF_CanPutHandsUp))
	{
		return false;
	}

	//Ensure hands up is not being suppressed.
	if(m_SuppressHandsUp)
	{
		return false;
	}

	//Grab the ped.
	const CPed* pPed = GetPed();

	//Ensure the ped is not in a vehicle.
	if(pPed->GetIsInVehicle())
	{
		return false;
	}

	//Ensure the flee flags allow the ped to put their hands up.
	if(!pPed->CanPutHandsUp(m_FleeFlags))
	{
		return false;
	}

	//Ensure the flee target is valid.
	const CEntity* pEntity = m_FleeTarget.GetEntity();
	if(!pEntity)
	{
		return false;
	}

	//Ensure the flee target is within a certain distance.
	if(DistSquared(pPed->GetTransform().GetPosition(), m_FleeTarget.GetEntity()->GetTransform().GetPosition()).Getf() >= square(12.0f))
	{
		return false;
	}

	return true;
}

bool CTaskSmartFlee::CanReverseInVehicle() const
{
	//Ensure we have not reversed.
	if(m_uRunningFlags.IsFlagSet(RF_HasReversedInVehicle))
	{
		return false;
	}

	//Ensure reverse is not disabled.
	if(GetPed()->GetPedIntelligence()->GetFleeBehaviour().GetFleeFlags().IsFlagSet(CFleeBehaviour::BF_DisableReverseInVehicle))
	{
		return false;
	}

	//Ensure the ped is driving a vehicle.
	if(!GetPed()->GetIsDrivingVehicle())
	{
		return false;
	}

	const CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
	taskAssert(pVehicle);

	//We do not want to crash into the back vehicle and create a traffic jam
	if ((pVehicle->GetIntelligence()->m_NumCarsBehindUs > 0) && (sm_Tunables.m_TimeToReverseCarsBehind < SMALL_FLOAT))
	{
		return false;
	}

	if (IsInoffensiveTarget(m_FleeTarget.GetEntity()) && (sm_Tunables.m_TimeToReverseInoffensiveTarget < SMALL_FLOAT))
	{
		return false;
	}

	//Ensure we aren't moving too quickly.
	static dev_float s_fMaxSpeed = 10.0f;
	float fSpeedSq = pVehicle->GetVelocity().XYMag2();
	float fMaxSpeedSq = square(s_fMaxSpeed);
	if(fSpeedSq > fMaxSpeedSq)
	{
		return false;
	}

	//Check if the target is a ped.
	const CPed* pTargetPed = GetPedWeAreFleeingFrom();
	if(pTargetPed)
	{
		//Check if the target ped is in a vehicle.
		if(pTargetPed->GetIsInVehicle())
		{
			//Ensure the target isn't moving quickly.
			static dev_float s_fMaxTargetSpeed = 5.0f;
			float fTargetSpeedSq = pTargetPed->GetVehiclePedInside()->GetVelocity().XYMag2();
			float fMaxTargetSpeedSq = square(s_fMaxTargetSpeed);
			if(fTargetSpeedSq > fMaxTargetSpeedSq)
			{
				return false;
			}
		}
	}

	//Grab the target position.
	Vec3V vTargetPosition;
	m_FleeTarget.GetPosition(RC_VECTOR3(vTargetPosition));

	//Ensure the target is in front of us.
	static const float s_fMinDot		= 0.707f;
	static const float s_fMaxDistance	= 20.0f;
	if(!IsPositionWithinConstraints(*GetPed()->GetVehiclePedInside(), vTargetPosition, s_fMinDot, s_fMaxDistance))
	{
		return false;
	}

	// The ped cannot reverse if the vehicle is not locally owned and controllable
	if (NetworkUtils::IsNetworkCloneOrMigrating(GetPed()->GetMyVehicle()))
	{
		return false;
	}

	return true;
}

void CTaskSmartFlee::CheckForCowerDuringFleeOnFoot()
{
	//Ensure we can cower.
	if(!CanCower())
	{
		return;
	}

	//Get the active movement task.
	CTask* pActiveMovementTask = GetPed()->GetPedIntelligence()->GetActiveMovementTask();
	if(!pActiveMovementTask)
	{
		return;
	}

	//Ensure the task is the right type.
	if(pActiveMovementTask->GetTaskType() != CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH)
	{
		return;
	}

	//Ensure the task has not finished or been aborted.
	if(pActiveMovementTask->GetIsFlagSet(aiTaskFlags::HasFinished) || pActiveMovementTask->GetIsFlagSet(aiTaskFlags::IsAborted))
	{
		return;
	}

	//Grab the task.
	CTaskMoveFollowNavMesh* pTask = static_cast<CTaskMoveFollowNavMesh *>(pActiveMovementTask);

	//Check if we were unable to find a route.
	if(pTask->IsUnableToFindRoute())
	{
		//Set the flag.
		m_uRunningFlags.SetFlag(RF_Cower);
	}
	//Check if we are following the route.
	else if(pTask->IsFollowingNavMeshRoute())
	{
		//Check if the flag is not set.
		if(!m_uRunningFlags.IsFlagSet(RF_HasCheckedCowerDueToRoute))
		{
			//No need to check the route again.
			m_uRunningFlags.SetFlag(RF_HasCheckedCowerDueToRoute);

			//Check if we should cower.
			if((pTask->GetRouteSize() <= sm_Tunables.m_MaxRouteSizeForCower) &&
				!pTask->IsTotalRouteLongerThan(sm_Tunables.m_MaxRouteLengthForCower))
			{
				pTask->SetKeepMovingWhilstWaitingForPath(false);
				pTask->SetDisableCalculatePathOnTheFly(true);
				pTask->SetDontStandStillAtEnd(false);
				pTask->SetTargetIsDestination(true);	// We navigate to a point now and we must hit the target
				pTask->SetTargetRadius(1.5f);
				//pTask->SetStopExactly(true);
				pTask->SetIsFleeing(false);				// We navigate to a point now and we must hit the target

				pTask->SetForceRestartFollowPathState(true); // Must be set to these settings will "kick in"

				//Cower at the end of the route.
				m_uRunningFlags.SetFlag(RF_CowerAtEndOfRoute);
			}
		}
	}
}

bool CTaskSmartFlee::CheckForExitVehicleDueToRoute()
{
	//Tick the timer.
	if(!m_ExitVehicleDueToRouteTimer.Tick())
	{
		return false;
	}
	
	//Reset the timer.
	m_ExitVehicleDueToRouteTimer.Reset(sm_Tunables.m_TimeBetweenExitVehicleDueToRouteChecks);
	
	//Check if we should exit the vehicle due to the route.
	const bool bShouldExitVehicleDueToRoute = ShouldExitVehicleDueToRoute();

	return bShouldExitVehicleDueToRoute;
}

bool CTaskSmartFlee::UpdateExitVehicleDueToVehicleStuck()
{
	// Grab ped
	CPed* pPed = GetPed();
	if (!taskVerifyf(pPed, "CTaskSmartFlee::UpdateExitVehicleDueToVehicleStuck, pPed == NULL!"))
	{
		return false;
	}

	//Ensure the ped is inside of a vehicle.
	CVehicle* pVehicle = pPed->GetVehiclePedInside();
	if (!taskVerifyf(pVehicle, "CTaskSmartFlee::UpdateExitVehicleDueToVehicleStuck, pVehicle == NULL!"))
	{
		return false;
	}

	const bool bIsDriver = pVehicle->IsDriver(pPed);
	const bool bIsHangingOn = !bIsDriver && IsPedHangingOnAVehicle();
	if (ShouldExitVehicleDueToVehicleStuck(pPed, bIsDriver, bIsHangingOn))
	{
		const float fMAX_SPEED_TO_IMMEDIATELY_EXIT_VEHICLE_SQUARED = 1.0f * 1.0f; 
		const bool bShouldExitImmediately = bIsHangingOn && pVehicle->GetVelocity().Mag2() < fMAX_SPEED_TO_IMMEDIATELY_EXIT_VEHICLE_SQUARED;
		if (bShouldExitImmediately || m_ExitVehicleDueToVehicleStuckTimer.Tick())
		{
			return true;
		}
	}
	else
	{
		ResetExitVehicleDueToVehicleStuckTimer();
	}

	return false;
}

void CTaskSmartFlee::ResetExitVehicleDueToVehicleStuckTimer()
{
	m_ExitVehicleDueToVehicleStuckTimer.Reset(sm_Tunables.m_TimeToExitCarDueToVehicleStuck);
}

void CTaskSmartFlee::ResetDrivingParallelToTargetTime()
{
	m_fDrivingParallelToTargetTime = 0.0f;
}

bool CTaskSmartFlee::IsDrivingParallelToAnyTarget(const CVehicle& rVehicle) const
{
	// Check if it is driving parallel to the main flee target
	if (IsDrivingParallelToTarget(rVehicle, m_FleeTarget.GetEntity()))
	{
		return true;
	}

	//Check if this is a MP game.
	if(NetworkInterface::IsGameInProgress())
	{
		//Iterate over the players.
		unsigned                 numPhysicalPlayers = netInterface::GetNumPhysicalPlayers();
		const netPlayer * const *allPhysicalPlayers = netInterface::GetAllPhysicalPlayers();

		for(unsigned index = 0; index < numPhysicalPlayers; index++)
		{
			const CNetGamePlayer *pPlayer = SafeCast(const CNetGamePlayer, allPhysicalPlayers[index]);

			//Ensure the ped is valid.
			CPed* pPlayerPed = pPlayer->GetPlayerPed();
			if(!pPlayerPed || m_FleeTarget.GetEntity() == pPlayerPed)
			{
				continue;
			}

			if(IsDrivingParallelToTarget(rVehicle, pPlayerPed))
			{
				return true;
			}
		}
	}

	return false;
}

bool CTaskSmartFlee::IsDrivingParallelToTarget(const CVehicle& rVehicle, const CEntity* pTargetEntity) const
{
	const CVehicle* pTargetVehicle = RetrieveTargetEntityVehicle(pTargetEntity);
	if (!pTargetVehicle)
	{
		return false;
	}

	// Retrieve vehicles info
	const Vec3V vMyVelocityDir = NormalizeFastSafe(VECTOR3_TO_VEC3V(rVehicle.GetVelocity()), rVehicle.GetTransform().GetForward());
	const Vec3V vTargetVelocityDir = NormalizeFastSafe(VECTOR3_TO_VEC3V(pTargetVehicle->GetVelocity()), pTargetVehicle->GetTransform().GetForward());

	// Check we are not in front of the target
	const Vec3V vTargetRelPosition = pTargetVehicle->GetTransform().GetPosition() - rVehicle.GetTransform().GetPosition();
	const ScalarV scTargetDistanceSqr = MagSquared(vTargetRelPosition);
	if (IsGreaterThanAll(scTargetDistanceSqr, ScalarV(rage::square(sm_Tunables.m_DrivingParallelToThreatMaxDistance))))
	{
		return false;
	}

	//Ensure the vehicle is facing the target.
	const ScalarV scTargetDistance = Sqrt(scTargetDistanceSqr);
	const Vec3V vVehicleToTargetDirection = vTargetRelPosition  / scTargetDistance;
	const ScalarV scFacingDot = Dot(vMyVelocityDir, vVehicleToTargetDirection);
	if (IsLessThanAll(scFacingDot, ScalarV(sm_Tunables.m_DrivingParallelToThreatMinFacing)))
	{
		return false;
	}

	// Check if my orientation is aligned to the target's
	const ScalarV scAlignmentDot = Dot(vMyVelocityDir, vTargetVelocityDir);
	if(IsLessThanAll(scAlignmentDot, ScalarV(sm_Tunables.m_DrivingParallelToThreatMinVelocityAlignment)))
	{
		return false;
	}

	return true;
}

void CTaskSmartFlee::UpdateDrivingParallelToTargetTime()
{
	//Check if the vehicle is valid.
	const CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
	if (pVehicle && IsDrivingParallelToAnyTarget(*pVehicle))
	{
		m_fDrivingParallelToTargetTime += GetTimeStep();
	}
	else
	{
		m_fDrivingParallelToTargetTime = 0.0f;
	}
}

bool CTaskSmartFlee::IsPedHangingOnAVehicle() const
{
	bool bHangingOn = false;

	const CPed* pPed = GetPed();
	const s32 iSeatIndex = pPed->GetMyVehicle()->GetSeatManager()->GetPedsSeatIndex(pPed);
	const CVehicleSeatAnimInfo* pSeatAnimInfo = pPed->GetMyVehicle()->GetSeatAnimationInfo(iSeatIndex);
	if(pSeatAnimInfo)
	{
		bHangingOn = pSeatAnimInfo->GetKeepCollisionOnWhenInVehicle();
	}

	return bHangingOn;
}

bool CTaskSmartFlee::ShouldExitVehicleDueToVehicleStuck(CPed* pPed, bool bIsDriver, bool bIsHangingOn)
{
	if( pPed->PopTypeIsMission() )
	{
		return false;
	}

	//Ensure the ped is inside of a vehicle.
	CVehicle* pVehicle = pPed->GetVehiclePedInside();
	if(!pVehicle)
	{
		return false;
	}

	//Ensure the ped is the driver.
	if(!bIsDriver && !bIsHangingOn)
	{
		return false;
	}

	//Ensure we can step out of the car.
	if(!pVehicle->CanPedStepOutCar(pPed, true))
	{
		return false;
	}

	if (IsInoffensiveTarget(m_FleeTarget.GetEntity()))
	{
		return false;
	}

	//Ensure we don't prefer to stay in our vehicle.
	if(IsStayingInVehiclePreferred())
	{
		if(pVehicle->GetVehicleType() == VEHICLE_TYPE_BICYCLE && GetTimeInState() > sm_Tunables.m_TimeOnBikeWithoutFleeingBeforeExitVehicle)
		{
			ScalarV scMinDistanceSq = LoadScalar32IntoScalarV(sm_Tunables.m_MinFleeOnBikeDistance);
			scMinDistanceSq = (scMinDistanceSq * scMinDistanceSq);
			ScalarV vDistanceToStartSq = DistSquared(VECTOR3_TO_VEC3V(m_vStartPos), pPed->GetTransform().GetPosition());
			if(IsLessThanAll(vDistanceToStartSq, scMinDistanceSq))
			{
				//Check if we are generally stopped.
				float fSpeedSq = pVehicle->GetVelocity().XYMag2();
				if(fSpeedSq < CTaskVehicleFlee::ms_fSpeedBelowWhichToBailFromVehicle * CTaskVehicleFlee::ms_fSpeedBelowWhichToBailFromVehicle)
				{
					return true;
				}
			}
		}

		return false;
	}

	// Dont do 3 point turns when fleeing
	if(pVehicle->GetIntelligence()->GetTaskManager()->FindTaskByTypeActive(VEHICLE_TASK_TREE_PRIMARY, CTaskTypes::TASK_VEHICLE_THREE_POINT_TURN))
	{
		return true;
	}

	//Keep track of the obstruction information.
	CTaskVehicleGoToPointWithAvoidanceAutomobile::ObstructionInformation obstructionInformation;

	//Grab the vehicle task.
	CTaskVehicleMissionBase* pVehicleTask = pVehicle->GetIntelligence()->GetActiveTask();
	if(pVehicleTask)
	{
		//Find the avoidance task.
		CTaskVehicleGoToPointWithAvoidanceAutomobile* pAvoidanceTask = static_cast<CTaskVehicleGoToPointWithAvoidanceAutomobile *>(pVehicleTask->FindSubTaskOfType(CTaskTypes::TASK_VEHICLE_GOTO_POINT_WITH_AVOIDANCE_AUTOMOBILE));
		if(pAvoidanceTask)
		{
			//Copy the obstruction information.
			obstructionInformation = pAvoidanceTask->GetObstructionInformation();
		}
	}

	// If our entire route is blocked ahead, and we're stopped, get out and run
	//Check if we are completely obstructed.
	if(obstructionInformation.m_uFlags.IsFlagSet(CTaskVehicleGoToPointWithAvoidanceAutomobile::ObstructionInformation::IsCompletelyObstructedClose))
	{
		//Check if we are generally stopped.
		float fSpeedSq = pVehicle->GetVelocity().XYMag2();
		if(fSpeedSq < CTaskVehicleFlee::ms_fSpeedBelowWhichToBailFromVehicle * CTaskVehicleFlee::ms_fSpeedBelowWhichToBailFromVehicle)
		{
			return true;
		}
	}

	return false;
}

s32 CTaskSmartFlee::ChooseStateForExitVehicle() const
{
	//Check if we should wait before exit vehicle.
	if(ShouldWaitBeforeExitVehicle())
	{
		return State_WaitBeforeExitVehicle;
	}
	else
	{
		return State_ExitVehicle;
	}
}

CTask* CTaskSmartFlee::CreateSubTaskForAmbientClips() const
{
	//Check if the ambient clips are valid.
	atHashWithStringNotFinal hAmbientClips = GetAmbientClipsToUse();
	if(hAmbientClips.IsNotNull())
	{
#if __ASSERT
		if (GetPed() && !GetPed()->GetIsInVehicle())
		{
			atHashWithStringNotFinal ambientContext("IN_VAN",0xe27d21f7);
			if (hAmbientClips == ambientContext)
			{
				taskDisplayf("m_hAmbientClips = %s", m_hAmbientClips.GetCStr());
				taskDisplayf("(GetPed()->GetPedModelInfo()->GetAmbientClipsForFlee()) = %s", (GetPed()->GetPedModelInfo()->GetAmbientClipsForFlee()).GetCStr());
				taskAssertf(0, "Ped %s(%p) is trying to use IN_VAN ambient idles when outside of vehicle, please grab an AI coder", GetPed()->GetModelName(), GetPed());
			}
		}
#endif // __ASSERT
		//Check if the conditional anims group is valid.
		const CConditionalAnimsGroup* pConditionalAnimsGroup = CONDITIONALANIMSMGR.GetConditionalAnimsGroup(hAmbientClips);
		if(taskVerifyf(pConditionalAnimsGroup, "Conditional anims group is invalid for hash: %s.", hAmbientClips.GetCStr()))
		{
			//Generate the flags.
			u32 uFlags = CTaskAmbientClips::Flag_PlayBaseClip|CTaskAmbientClips::Flag_ForceFootClipSetRestart;
// 			if(m_bDontClaimProp)
// 			{
// 				uFlags |= CTaskAmbientClips::Flag_DontClaimProp;
// 			}
	
			//Create the task.
			return rage_new CTaskAmbientClips(uFlags, pConditionalAnimsGroup);
		}
	}

	return NULL;
}

CTask* CTaskSmartFlee::CreateSubTaskForCallPolice() const
{
	//Create the task.
	CTaskCallPolice* pTask = rage_new CTaskCallPolice(GetPedWeAreFleeingFrom());

	return pTask;
}

bool CTaskSmartFlee::DoesVehicleHaveNoDriver() const
{
	//Check if the vehicle is valid.
	const CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
	if(pVehicle)
	{
		//Check if the driver is invalid.
		const CPed* pDriver = pVehicle->GetDriver();
		if(!pDriver)
		{
			return true;
		}
		//Check if the driver is injured.
		else if(pDriver->IsInjured())
		{
			return true;
		}
		//Check if the driver is exiting the vehicle.
		else if(pDriver->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE, true))
		{
			//Check if the task is valid.
			CTask* pTask = pDriver->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE);
			if(pTask)
			{
				//Wait a bit of time before exiting.
				static dev_float s_fMinTime = 0.70f;
				static dev_float s_fMaxTime = 1.35f;
				if(pTask->GetTimeRunning() < GetPed()->GetRandomNumberInRangeFromSeed(s_fMinTime, s_fMaxTime))
				{
					return false;
				}
			}
			
			return true;
		}
	}

	return false;
}

atHashWithStringNotFinal CTaskSmartFlee::GetAmbientClipsToUse() const
{
	//Ensure the flag is not set.
	if(GetPed()->GetPedIntelligence()->GetFleeBehaviour().GetFleeFlags().IsFlagSet(CFleeBehaviour::BF_DisableAmbientClips))
	{
		return atHashWithStringNotFinal::Null();
	}

	//Check if the ambient clips are valid.
	if(m_hAmbientClips.IsNotNull())
	{
		return m_hAmbientClips;
	}

	return (GetPed()->GetPedModelInfo()->GetAmbientClipsForFlee());
}

const CPed* CTaskSmartFlee::GetPedWeAreFleeingFrom() const
{
	//Ensure the entity is valid.
	const CEntity* pEntity = m_FleeTarget.GetEntity();
	if(!pEntity)
	{
		return NULL;
	}

	//Ensure the entity is a ped.
	if(!pEntity->GetIsTypePed())
	{
		return NULL;
	}

	return static_cast<const CPed *>(pEntity);
}

bool CTaskSmartFlee::HasOnFootClipSetOverride() const
{
	//Ensure we are in the correct state.
	if(GetState() != State_FleeOnFoot)
	{
		return false;
	}
	
	//Find the ambient clips task.
	CTaskAmbientClips* pTask = static_cast<CTaskAmbientClips *>(FindSubTaskOfType(CTaskTypes::TASK_AMBIENT_CLIPS));
	if(!pTask)
	{
		return false;
	}
	
	return pTask->GetFlags().IsFlagSet(CTaskAmbientClips::Flag_HasOnFootClipSetOverride);
}

bool CTaskSmartFlee::IsLifeInDanger() const
{
	//Check the current event type.
	int iEventType = GetPed()->GetPedIntelligence()->GetCurrentEventType();
	switch(iEventType)
	{
		case EVENT_DAMAGE:
		case EVENT_EXPLOSION:
		case EVENT_GUN_AIMED_AT:
		case EVENT_MELEE_ACTION:
		case EVENT_POTENTIAL_BLAST:
		case EVENT_SHOT_FIRED:
		case EVENT_SHOT_FIRED_BULLET_IMPACT:
		case EVENT_SHOT_FIRED_WHIZZED_BY:
		{
			return true;
		}
		default:
		{
			return false;
		}
	}
}

bool CTaskSmartFlee::IsFarEnoughAwayFromTarget() const
{
	//Ensure the stop distance is valid.
	if(IsDistanceUndefined(m_fStopDistance) && !m_bCanCheckForVehicleExitInCower)
	{
		return false;
	}

	//Grab the target position.
	Vec3V vTargetPosition;
	m_FleeTarget.GetPosition(RC_VECTOR3(vTargetPosition));

	//Grab the required distance to consider the flee target as far enough away
	const float fFarEnoughDistance = m_bCanCheckForVehicleExitInCower ? sm_Tunables.m_MinDistFromTargetWhenCoweringToCheckForExit : m_fStopDistance;

	//Ensure the distance exceeds the threshold.
	ScalarV scDistSq = DistSquared(GetPed()->GetTransform().GetPosition(), vTargetPosition);
	ScalarV scMinDistSq = ScalarVFromF32(square(fFarEnoughDistance));
	if(IsLessThanAll(scDistSq, scMinDistSq))
	{
		return false;
	}

	return true;
}

bool CTaskSmartFlee::IsOnTrain() const
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

bool CTaskSmartFlee::IsStayingInVehiclePreferred() const
{
	const CPed* pPed = GetPed();

	//Check if we are in a vehicle.
	if(pPed->GetIsInVehicle())
	{
		//Check if we are riding a bike.
		if(pPed->GetVehiclePedInside()->InheritsFromBike())
		{
			return true;
		}

		//Check if we are in a boat.
		if(pPed->GetVehiclePedInside()->InheritsFromBoat())
		{
			return true;
		}

		//Check if the event we are responding to is disputes.
		if(pPed->GetPedIntelligence()->GetCurrentEventType() == EVENT_AGITATED_ACTION)
		{
			return true;
		}
	}

	return false;
}

bool CTaskSmartFlee::IsTargetArmed() const
{
	//Ensure the target is valid.
	const CPed* pTarget = GetPedWeAreFleeingFrom();
	if(!pTarget)
	{
		return false;
	}

	return (pTarget->GetWeaponManager() && pTarget->GetWeaponManager()->GetIsArmed());
}

bool CTaskSmartFlee::IsTargetArmedWithGun() const
{
	//Ensure the target is valid.
	const CPed* pTarget = GetPedWeAreFleeingFrom();
	if(!pTarget)
	{
		return false;
	}

	return (pTarget->GetWeaponManager() && pTarget->GetWeaponManager()->GetIsArmedGun());
}

bool CTaskSmartFlee::IsInoffensiveTarget(const CEntity* pTargetEntity) const
{
	bool bIsInoffensiveTarget = true;

	if (pTargetEntity)
	{
		if (pTargetEntity->GetIsTypeVehicle())
		{
			bIsInoffensiveTarget = false;
		}
		if (pTargetEntity->GetIsTypePed())
		{
			const CPed* pPed = GetPed();
			taskAssert(pPed);

			const CPed* pTargetPed = SafeCast(const CPed, pTargetEntity);
			taskAssert(pTargetPed);

			bIsInoffensiveTarget = !pTargetPed->GetIsInVehicle() && !ShouldConsiderPedThreatening(*pPed, *pTargetPed, false);
		}
	}

	return bIsInoffensiveTarget;
}

bool CTaskSmartFlee::CanExitVehicleToFleeFromTarget(const CEntity* pTargetEntity) const
{
	return CanExitVehicle() && !IsInoffensiveTarget(pTargetEntity);
}

bool CTaskSmartFlee::IsVehicleStuck() const
{
	//Ensure the vehicle is valid.
	const CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
	if(!pVehicle)
	{
		return false;
	}

	//Ensure the ped is driving.
	if(!GetPed()->GetIsDrivingVehicle())
	{
		return false;
	}

	//Check if the vehicle is stuck.
	static dev_u16 s_uMinTime = 5000;
	return (pVehicle->GetVehicleDamage()->GetIsStuck(VEH_STUCK_ON_ROOF, s_uMinTime) ||
		pVehicle->GetVehicleDamage()->GetIsStuck(VEH_STUCK_ON_SIDE, s_uMinTime) ||
		pVehicle->GetVehicleDamage()->GetIsStuck(VEH_STUCK_HUNG_UP, s_uMinTime) ||
		pVehicle->GetVehicleDamage()->GetIsStuck(VEH_STUCK_JAMMED, s_uMinTime));
}

bool CTaskSmartFlee::IsWaitingForPathInFleeOnFoot() const
{
	// Get the active movement task.
	CTask* pActiveMovementTask = GetPed()->GetPedIntelligence()->GetActiveMovementTask();
	if (!pActiveMovementTask)
	{
		return false;
	}

	// Ensure the task is the right type.
	if (pActiveMovementTask->GetTaskType() != CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH)
	{
		return false;
	}

	// Ensure the task has not finished or been aborted.
	if (pActiveMovementTask->GetIsFlagSet(aiTaskFlags::HasFinished) || pActiveMovementTask->GetIsFlagSet(aiTaskFlags::IsAborted))
	{
		return false;
	}

	// Grab the task.
	CTaskMoveFollowNavMesh* pTask = static_cast<CTaskMoveFollowNavMesh*>(pActiveMovementTask);

	// Check if we are following the route.
	if (pTask->IsFollowingNavMeshRoute())
	{
		return false;
	}

	return true;
}

void CTaskSmartFlee::OnAmbientClipsChanged()
{
	//Restart the on-foot sub-task.
	RestartSubTaskForOnFoot();
}

void CTaskSmartFlee::OnExitingVehicle()
{
	//Add the ped.
	if(!sm_aPedsExitingTheirVehicle.IsFull())
	{
		sm_aPedsExitingTheirVehicle.Append().Reset(GetPed());
	}
}

void CTaskSmartFlee::OnMoveBlendRatioChanged()
{
	//Ensure the sub-task is valid.
	CTask* pSubTask = GetSubTask();
	if(!pSubTask)
	{
		return;
	}
	
	//Ensure the sub-task is a complex control movement.
	if(pSubTask->GetTaskType() != CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
	{
		return;
	}
	
	//Grab the complex control movement.
	CTaskComplexControlMovement* pTask = static_cast<CTaskComplexControlMovement *>(pSubTask);
	if(!pTask)
	{
		return;
	}
	
	//Set the move/blend ratio.
	pTask->SetMoveBlendRatio(GetPed(), m_fMoveBlendRatio);
}

void CTaskSmartFlee::OnNoLongerExitingVehicle()
{
	//Remove the ped.
	for(int i = 0; i < sm_aPedsExitingTheirVehicle.GetCount(); ++i)
	{
		if(GetPed() == sm_aPedsExitingTheirVehicle[i])
		{
			sm_aPedsExitingTheirVehicle.DeleteFast(i);
			break;
		}
	}
}

void CTaskSmartFlee::RestartSubTaskForOnFoot()
{
	//Ensure we are fleeing on foot.
	if(!IsFleeingOnFoot())
	{
		return;
	}

	//Ensure the sub-task is valid.
	CTask* pSubTask = GetSubTask();
	if(!pSubTask)
	{
		return;
	}

	//Ensure the sub-task is a complex control movement.
	if(pSubTask->GetTaskType() != CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
	{
		return;
	}

	//Set the new sub-task.
	CTaskComplexControlMovement* pTask = static_cast<CTaskComplexControlMovement *>(pSubTask);
	pTask->SetNewMainSubtask(CreateSubTaskForAmbientClips());
}

void CTaskSmartFlee::ProcessMoveTask()
{
	//Check if we should process the move task.
	if(!ShouldProcessMoveTask())
	{
		return;
	}

	//Check if the existing move task is invalid.
	if(!m_pExistingMoveTask)
	{
		//Calculate the target position.
		Vec3V vTargetPosition;
		m_FleeTarget.GetPosition(RC_VECTOR3(vTargetPosition));

		//Avoid nearby peds
		const CPed* pPed = GetPed();
		taskAssert(pPed);

		vTargetPosition = AdjustTargetPosByNearByPeds(*pPed, vTargetPosition, m_FleeTarget.GetEntity(), true);

		//Create the move task.
		CTaskMoveFollowNavMesh* pTask = CreateMoveTaskForFleeOnFoot(vTargetPosition);
		m_pExistingMoveTask = pTask;

		//Set the spheres of influence builder.
		pTask->SetInfluenceSphereBuilderFn(SpheresOfInfluenceBuilder);

		//Keep moving while waiting for a path.
		pTask->SetKeepMovingWhilstWaitingForPath(true);

		//Don't stand still at the end.
		pTask->SetDontStandStillAtEnd(true);

		// Make sure we keep to pavement if we prefer that
		pTask->SetKeepToPavements(ShouldPreferPavements(pPed));

		//
		pTask->SetConsiderRunStartForPathLookAhead(true);

		//Add the move task.
		GetPed()->GetPedIntelligence()->AddTaskMovement(pTask);

		//Set the owner.
		NOTFINAL_ONLY(pTask->GetMoveInterface()->SetOwner(this));
	}

	//Keep the existing move task alive.
	m_pExistingMoveTask->GetMoveInterface()->SetCheckedThisFrame(true);
}

bool CTaskSmartFlee::AddGetUpSets( CNmBlendOutSetList& sets, CPed* pPed )
{
	//Check if the ped is valid.
	if(pPed)
	{
		//Check if the flee target is valid.
		if(m_FleeTarget.GetIsValid())
		{
			//Get the flee position.
			Vector3 fleePos(VEC3_ZERO);
			m_FleeTarget.GetPosition(fleePos);
			
			//Read the sets.
			SetupFleeingGetup(sets, fleePos, pPed, this);
			
			return true;
		}
	}

	// no custom blend out set specified. Use the default.
	return false;
}

void CTaskSmartFlee::SetupFleeingGetup( CNmBlendOutSetList& sets, const Vector3& vFleePos, CPed* pPed, const CTaskSmartFlee * pFleeTask )
{
	// add a standing blend out
	sets.Add(NMBS_STANDING);
	if (pPed->ShouldDoHurtTransition() && CNmBlendOutSetManager::IsBlendOutSetStreamedIn(NMBS_WRITHE))
	{
		sets.Add(NMBS_WRITHE);
	}
	else
	{
		bool isInjured = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsInjured);
		sets.AddWithFallback( isInjured ? NMBS_PANIC_FLEE_INJURED : NMBS_PANIC_FLEE, pPed->IsMale() ? NMBS_STANDARD_GETUPS : NMBS_STANDARD_GETUPS_FEMALE);

		CTaskMoveFollowNavMesh* pMoveTask = rage_new CTaskMoveFollowNavMesh(
			3.0f,
			vFleePos,
			CTaskMoveFollowNavMesh::ms_fTargetRadius,
			CTaskMoveFollowNavMesh::ms_fSlowDownDistance,
			-1,		// time
			true,	// use blending
			true,	// flee from target
			NULL
			);

		pMoveTask->SetKeepMovingWhilstWaitingForPath(true);
		pMoveTask->SetDontStandStillAtEnd(true);
		pMoveTask->SetHighPrioRoute(true);	// It is important that we get the path ASAP
	//	pMoveTask->SetAvoidPeds(false);

		// If we passed in a flee task, here's the opportunity to set navigation parameters from it
		if(pFleeTask)
		{
			pMoveTask->SetUseLargerSearchExtents( pFleeTask->GetUseLargerSearchExtents() );
		}

		const dev_float fFleeForeverDist = 99999.0f;
		const float fFleeDist = fFleeForeverDist;

		pMoveTask->SetFleeSafeDist(fFleeDist);
		const bool bShouldPreferPavements = pFleeTask ? pFleeTask->ShouldPreferPavements(pPed) : pPed->GetPedIntelligence()->GetFleeBehaviour().GetFleeFlags().IsFlagSet(CFleeBehaviour::BF_PreferPavements);
		if (bShouldPreferPavements)
		{
			pMoveTask->SetKeepToPavements(true);
		}

		bool bPedStartedInWater = pPed->GetPedResetFlag( CPED_RESET_FLAG_IsDrowning );
		pMoveTask->SetFleeNeverEndInWater( !bPedStartedInWater );

		sets.SetMoveTask(pMoveTask);
	}
}

void CTaskSmartFlee::GetUpComplete(eNmBlendOutSet setMatched, CNmBlendOutItem* UNUSED_PARAM(lastItem), CTaskMove* pMoveTask, CPed* UNUSED_PARAM(pPed))
{
	if (setMatched==NMBS_PANIC_FLEE
		|| setMatched==NMBS_PANIC_FLEE_INJURED)
	{
		// Grab the existing move task to continue
		m_pExistingMoveTask = pMoveTask;
	}
}

void CTaskSmartFlee::RequestGetupAssets( CPed* pPed )
{
	m_GetupReqHelper[0].Request(NMBS_PANIC_FLEE);

	if (pPed->ShouldDoHurtTransition())
		m_GetupReqHelper[1].Request(NMBS_WRITHE);
}

void CTaskSmartFlee::ResetRunStartPhase()
{
	//Set the run-start phase.
	SetRunStartPhase(0.0f);
}

void CTaskSmartFlee::SetRunStartPhase(float fPhase)
{
	//Ensure the motion task is valid.
	CTask* pTask = GetPed()->GetPrimaryMotionTaskIfExists();
	if(!pTask)
	{
		return;
	}

	//Ensure the task is valid.
	if(pTask->GetTaskType() != CTaskTypes::TASK_MOTION_PED)
	{
		return;
	}

	//Set the phase.
	static_cast<CTaskMotionPed *>(pTask)->SetBeginMoveStartPhase(fPhase);
}

void CTaskSmartFlee::SetStateForExitVehicle()
{
	//Assert that we can exit the vehicle.
	taskAssert(CanExitVehicle());

	//Choose the state for exit vehicle.
	s32 iState = ChooseStateForExitVehicle();
	if(iState != GetState())
	{
		//Note that we can check for exits during cower.
		m_bCanCheckForVehicleExitInCower = true;

		//Set the state.
		SetState(iState);
	}
	else
	{
		//Restart the state.
		SetFlag(aiTaskFlags::RestartCurrentState);
	}
}

bool CTaskSmartFlee::ShouldAccelerateInVehicle() const
{
	//Ensure the flag is set.
	if(!m_uConfigFlags.IsFlagSet(CF_AccelerateInVehicle))
	{
		return false;
	}

	//Check if we can accelerate in the vehicle.
	if(CanAccelerateInVehicle())
	{
		return true;
	}

	return false;
}

bool CTaskSmartFlee::ShouldCower() const
{
	//Ensure we can cower.
	if(!CanCower())
	{
		return false;
	}

	//Grab the ped.
	const CPed* pPed = GetPed();

	//Check if the flag is set.
	if(pPed->GetPedIntelligence()->GetFleeBehaviour().GetFleeFlags().IsFlagSet(CFleeBehaviour::BF_CowerInsteadOfFlee))
	{
		return true;
	}

	//Check if the flag is set.
	if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_CowerInsteadOfFlee))
	{
		return true;
	}

	//Check if the flag is set.
	if(m_uConfigFlags.IsFlagSet(CF_Cower))
	{
		return true;
	}

	//Check if the flag is set.
	if(m_uRunningFlags.IsFlagSet(RF_Cower))
	{
		return true;
	}

	//If I am a passenger in a vehicle without driver and the threat is inoffensive, cower instead of running away
	if (ShouldCowerInsteadOfFleeDueToTarget())
	{
		return true;
	}

#if __BANK
	if(sm_Tunables.m_ForceCower)
	{
		return true;
	}
#endif

	if(IsOnTrain())
	{
		return true;
	}

	return false;
}

bool CTaskSmartFlee::ShouldEnterLastUsedVehicleImmediately() const
{
	//Ensure the vehicle is valid.
	const CVehicle* pVehicle = GetPed()->GetMyVehicle();
	if(!pVehicle)
	{
		return false;
	}
	
	//Ensure we did not exit the vehicle.
	if(pVehicle == GetPed()->GetPedIntelligence()->GetLastVehiclePedInside())
	{
		return false;
	}

	//Ensure the vehicle is not wrecked.
	if(pVehicle->GetStatus() == STATUS_WRECKED)
	{
		return false;
	}

	//Ensure the vehicle is not in a bad state.
	if(pVehicle->m_nVehicleFlags.bIsDrowning)
	{
		return false;
	}
	else if(pVehicle->m_nFlags.bIsOnFire)
	{
		return false;
	}

	//If locked, don't re-enter. Note: we use this check in LookForCarsToUse as well. Even though it's specifically for players, we infer it to exclude AI too.
	if(NetworkInterface::IsGameInProgress())
	{
		CNetObjVehicle* vehNetworkObj = static_cast<CNetObjVehicle*>(pVehicle->GetNetworkObject());
		if(vehNetworkObj && vehNetworkObj->IsLockedForAnyPlayer())
		{
			return false;
		}
	}

	const CEntity* pFleeEntity = m_FleeTarget.GetEntity();
	
	// don't flee using your vehicle if the entity you're fleeing is in the vehicle
	if (pFleeEntity && pFleeEntity->GetIsTypePed() && GetPed()->GetMyVehicle()->ContainsPed(static_cast<const CPed*>(pFleeEntity)))
	{
		return false;
	}

	// Don't return to your boat if the threat is standing on it.
	if (pFleeEntity && pFleeEntity->GetIsTypePed() && GetPed()->GetMyVehicle()->InheritsFromBoat() && static_cast<const CPed*>(pFleeEntity)->GetGroundPhysical() == GetPed()->GetMyVehicle())
	{
		return false;
	}


	//Check if the flag is set.  JR - apparently unused?
	if(m_uConfigFlags.IsFlagSet(CF_EnterLastUsedVehicleImmediately))
	{
		return true;
	}

	//Check if the flag is set.
	if(m_uConfigFlags.IsFlagSet(CF_EnterLastUsedVehicleImmediatelyIfCloserThanTarget))
	{
		//Get the target position.
		Vec3V vTargetPosition;
		m_FleeTarget.GetPosition(RC_VECTOR3(vTargetPosition));

		//Get the ped position.
		Vec3V vPedPosition = GetPed()->GetTransform().GetPosition();

		//Get the vehicle position.
		Vec3V vVehiclePosition = pVehicle->GetTransform().GetPosition();

		//Get the normalized vector to the vehicle from the ped.
		Vec3V vToVehicle = NormalizeFastSafe(vVehiclePosition - vPedPosition, Vec3V(V_ZERO));

		//Check to make sure the vehicle is either in front of us or not a bicycle (see B*2069382).
		if(pVehicle->InheritsFromBicycle() && IsLessThanAll(Dot(GetPed()->GetTransform().GetForward(), vToVehicle), ScalarV(0.0f)))
		{
			return false;
		}

		//Check if the distance is valid.
		ScalarV vPedToVehicleDistSq = DistSquared(vPedPosition, vVehiclePosition);
		ScalarV vPedToTargetDistSq = DistSquared(vPedPosition, vTargetPosition);
		if(IsLessThanAll(vPedToVehicleDistSq, vPedToTargetDistSq))
		{
			return true;
		}
	}

	return false;
}

bool CTaskSmartFlee::ShouldExitVehicleDueToAttributes() const
{
	//Check if the flag is set.
	if(m_uConfigFlags.IsFlagSet(CF_ForceLeaveVehicle))
	{
		return true;
	}

	//Check if the flag is set.
	if(GetPed()->GetPedIntelligence()->GetFleeBehaviour().GetFleeFlags().IsFlagSet(CFleeBehaviour::BF_ForceExitVehicle))
	{
		return true;
	}

	return false;
}

bool CTaskSmartFlee::ShouldExitVehicleDueToRoute() const
{
	//Assert that we can exit the vehicle.
	taskAssert(CanExitVehicle());

	//Is this behaviour active?
	if (!sm_Tunables.m_ExitVehicleDueToRoute)
	{
		return false;
	}

	//Ensure we haven't accelerated in our vehicle.
	if(m_uRunningFlags.IsFlagSet(RF_HasAcceleratedInVehicle))
	{
		return false;
	}

	//Grab the ped.
	const CPed* pPed = GetPed();
	
	//Ensure the ped is inside of a vehicle.
	CVehicle* pVehicle = pPed->GetVehiclePedInside();
	if(!pVehicle)
	{
		return false;
	}
	
	//Ensure the ped is the driver.
	if(!pVehicle->IsDriver(pPed))
	{
		return false;
	}

	//Ensure we can step out of the car.
	if(!pVehicle->CanPedStepOutCar(pPed, true))
	{
		return false;
	}

	//Ensure we don't prefer to stay in our vehicle.
	if(IsStayingInVehiclePreferred())
	{
		return false;
	}

	//Check if the target is valid.
	SVehicleRouteInterceptionParams targetVehicleRouteInterceptionParams;
	if(GetFleeTargetExitVehicleRouteInterceptionParams(&targetVehicleRouteInterceptionParams))
	{
		//Check if we should exit due to the route and target.
		if(ShouldExitVehicleDueToRouteAndTarget(*pVehicle, targetVehicleRouteInterceptionParams))
		{
			return true;
		}
	}

	//Check if this is a MP game.
	if(NetworkInterface::IsGameInProgress())
	{
		//Iterate over the players.
		unsigned                 numPhysicalPlayers = netInterface::GetNumPhysicalPlayers();
		const netPlayer * const *allPhysicalPlayers = netInterface::GetAllPhysicalPlayers();

		for(unsigned index = 0; index < numPhysicalPlayers; index++)
		{
			const CNetGamePlayer *pPlayer = SafeCast(const CNetGamePlayer, allPhysicalPlayers[index]);

			//Ensure the ped is valid.
			CPed* pPlayerPed = pPlayer->GetPlayerPed();
			if(!pPlayerPed || m_FleeTarget.GetEntity() == pPlayerPed)
			{
				continue;
			}
			
			//Check if we should exit due to the route and target.
			SVehicleRouteInterceptionParams playerVehicleRouteInterceptionParams;
			GetTargetExitVehicleRouteInterceptionParams(*pPlayerPed, &playerVehicleRouteInterceptionParams);
			if(ShouldExitVehicleDueToRouteAndTarget(*pVehicle, playerVehicleRouteInterceptionParams))
			{
				return true;
			}
		}
	}
	
	return false;
}

bool CTaskSmartFlee::ShouldExitVehicleDueToRouteAndTarget(const CVehicle& rVehicle, const SVehicleRouteInterceptionParams& rTargetVehicleInterceptionParams) const
{
	//Grab the vehicle info.
	const Vec3V vVehiclePosition = rVehicle.GetTransform().GetPosition();
	const Vec3V vVehicleForward  = rVehicle.GetTransform().GetForward();
	const Vec3V vVehicleVelocity = VECTOR3_TO_VEC3V(rVehicle.GetVelocity());

	//Check if we are outside the minimum distance.
	ScalarV scDistSq = DistSquared(vVehiclePosition, rTargetVehicleInterceptionParams.vPosition);
	ScalarV scRelSpeed = MagXY(vVehicleVelocity - rTargetVehicleInterceptionParams.vVelocity);
	static dev_float s_fScaleFactor = 3.0f;
	ScalarV scScaleFactor = ScalarVFromF32(s_fScaleFactor);
	ScalarV scMinDist = Scale(scRelSpeed, scScaleFactor);
	ScalarV scMinDistSq = Scale(scMinDist, scMinDist);
	if(IsGreaterThanAll(scDistSq, scMinDistSq))
	{
		return false;
	}
	
	//Ensure the vehicle is facing the target.
	Vec3V vVehicleToTarget = Subtract(rTargetVehicleInterceptionParams.vPosition, vVehiclePosition);
	Vec3V vVehicleToTargetDirection = NormalizeFastSafe(vVehicleToTarget, Vec3V(V_ZERO));
	ScalarV scDot = Dot(vVehicleForward, vVehicleToTargetDirection);
	static dev_float s_fMinDot = 0.866f;
	if(IsLessThanAll(scDot, ScalarVFromF32(s_fMinDot)))
	{
		return false;
	}

	//Ensure the route passes by the target.
	bool bTargetIsInRoute = false;
	if (sm_Tunables.m_UseRouteInterceptionCheckToExitVehicle)
	{
		bTargetIsInRoute = rVehicle.GetIntelligence()->CheckRouteInterception(rTargetVehicleInterceptionParams.vPosition, rTargetVehicleInterceptionParams.vVelocity, rTargetVehicleInterceptionParams.fMaxSideDistance, rTargetVehicleInterceptionParams.fMaxForwardDistance, rTargetVehicleInterceptionParams.fMinTime, rTargetVehicleInterceptionParams.fMaxTime);
	}
	else
	{
		bTargetIsInRoute = rVehicle.GetIntelligence()->DoesRoutePassByPosition(rTargetVehicleInterceptionParams.vPosition, sm_Tunables.m_ExitVehicleRouteMinDistance, sm_Tunables.m_ExitVehicleMaxDistance);
	}

	if(!bTargetIsInRoute)
	{
		return false;
	}
	
	return true;
}

bool CTaskSmartFlee::GetFleeTargetExitVehicleRouteInterceptionParams(SVehicleRouteInterceptionParams* pParams) const
{
	taskAssert(pParams);

	bool bSuccess = false;

	if (m_FleeTarget.GetIsValid())
	{
		const CEntity* pFleeTargetEntity = m_FleeTarget.GetEntity();
		if (pFleeTargetEntity)
		{
			GetTargetExitVehicleRouteInterceptionParams(*pFleeTargetEntity, pParams);
			bSuccess = true;
		}
		else
		{
			// Static target
			Vector3 vPosition;
			if (m_FleeTarget.GetPosition(vPosition))
			{
				bSuccess = true;
				pParams->fMaxForwardDistance = sm_Tunables.m_RouteInterceptionCheckDefaultMaxForwardDistance;
				pParams->fMaxSideDistance    = sm_Tunables.m_RouteInterceptionCheckDefaultMaxSideDistance;
				pParams->fMinTime			 = sm_Tunables.m_RouteInterceptionCheckMinTime;	
				pParams->fMaxTime			 = sm_Tunables.m_RouteInterceptionCheckMaxTime;	
			}
		}
	}

	return bSuccess;
}

void CTaskSmartFlee::GetTargetExitVehicleRouteInterceptionParams(const CEntity& rTargetEntity, SVehicleRouteInterceptionParams* pParams) const
{
	taskAssert(pParams);

	const CVehicle* pTargetVehicle = RetrieveTargetEntityVehicle(&rTargetEntity);
	if (pTargetVehicle)
	{
		// Target vehicle
		pParams->vPosition		     = pTargetVehicle->GetTransform().GetPosition();
		pParams->vForward			 = pTargetVehicle->GetTransform().GetForward();
		pParams->vVelocity		     = VECTOR3_TO_VEC3V(pTargetVehicle->GetVelocity());
		pParams->fMaxForwardDistance = sm_Tunables.m_RouteInterceptionCheckVehicleMaxForwardDistance;
		pParams->fMaxSideDistance    = sm_Tunables.m_RouteInterceptionCheckVehicleMaxSideDistance;
		pParams->fMinTime			 = sm_Tunables.m_RouteInterceptionCheckMinTime;	
		pParams->fMaxTime			 = sm_Tunables.m_RouteInterceptionCheckMaxTime;	
	}
	else
	{
		// Target on foot
		pParams->vPosition = rTargetEntity.GetTransform().GetPosition();
		pParams->vForward  = rTargetEntity.GetTransform().GetForward();
		// NOTE[egarcia]: I am scaling down the ped velocity because we do not want to consider positions very far from its init position
		static const ScalarV scPedPredictedVelocityScale(0.25f);
		pParams->vVelocity = rTargetEntity.GetIsTypePed() ? VECTOR3_TO_VEC3V(static_cast<const CPed&>(rTargetEntity).GetVelocity()) * scPedPredictedVelocityScale : Vec3V(V_ZERO); 
		pParams->fMaxForwardDistance = sm_Tunables.m_RouteInterceptionCheckDefaultMaxForwardDistance;
		pParams->fMaxSideDistance    = sm_Tunables.m_RouteInterceptionCheckDefaultMaxSideDistance;
		pParams->fMinTime			 = sm_Tunables.m_RouteInterceptionCheckMinTime;	
		pParams->fMaxTime			 = sm_Tunables.m_RouteInterceptionCheckMaxTime;	
	}
}

const CVehicle* CTaskSmartFlee::RetrieveTargetEntityVehicle(const CEntity* pTargetEntity)
{
	if (!pTargetEntity)
	{
		return NULL;
	}

	const CPed* pTargetPed = pTargetEntity->GetIsTypePed() ? static_cast<const CPed*>(pTargetEntity) : NULL;

	const CVehicle* pTargetVehicle = NULL;
	if (pTargetPed)
	{
		pTargetVehicle = pTargetPed->GetVehiclePedInside();
	}
	else if (pTargetEntity->GetIsTypeVehicle())
	{
		pTargetVehicle = SafeCast(const CVehicle, pTargetEntity);
	}

	return pTargetVehicle;
}

bool CTaskSmartFlee::ShouldFleeOurOwnVehicle() const
{
	//Grab the ped.
	const CPed* pPed = GetPed();
	
	//Ensure the ped is in a vehicle.
	const CVehicle* pVehicle = pPed->GetVehiclePedInside();
	if(!pVehicle)
	{
		return false;
	}
	
	//Ensure the flee entity is valid.
	const CEntity* pFleeEntity = m_FleeTarget.GetEntity();
	if(!pFleeEntity)
	{
		return false;
	}
	
	//Check if the flee entity is our vehicle.
	if(pFleeEntity == pVehicle)
	{
		return true;
	}

	//Ensure we're flagged to use vehicles during flee
	if(!pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanUseVehicles))
	{
		return true;
	}

	//If our vehicle is a landed plane
	if(pVehicle->InheritsFromPlane() && pVehicle->HasContactWheels())
	{
		return true;
	}
	
	//Check if the flee entity is a ped.
	if(pFleeEntity->GetIsTypePed())
	{
		//Check if the flee ped is in our vehicle.
		const CPed* pFleePed = static_cast<const CPed *>(pFleeEntity);
		if(pFleePed->GetVehiclePedInside() == pVehicle)
		{
			return true;
		}
		
		//Check if the flee ped is entering our vehicle.
		if(pFleePed->GetVehiclePedEntering() == pVehicle)
		{
			return true;
		}
	}
	
	return false;
}

bool CTaskSmartFlee::ShouldProcessMoveTask() const
{
	//Ensure the ped is not a clone.
	if(GetPed()->IsNetworkClone())
	{
		return false;
	}

	//Check if we are exiting a vehicle.
	if(GetState() == State_ExitVehicle)
	{
		//Wait until the exit task has computed a valid exit position before running the movement task
		if(!GetSubTask() || GetSubTask()->GetTaskType() != CTaskTypes::TASK_EXIT_VEHICLE)
		{
			return false;
		}

		if(!static_cast<const CTaskExitVehicle*>(GetSubTask())->HasValidExitPosition())
		{
			return false;
		}
		
		return true;
	}

	return false;
}

bool CTaskSmartFlee::ShouldRestartSubTaskForOnFoot() const
{
	//Assert that we are fleeing on foot.
	taskAssert(IsFleeingOnFoot());

	//Check if we have ambient clips.
	atHashWithStringNotFinal hAmbientClips = GetAmbientClipsToUse();
	if(hAmbientClips.IsNotNull())
	{
		//Check if the sub-task is not running.
		CTaskComplexControlMovement* pTask = static_cast<CTaskComplexControlMovement *>(
			FindSubTaskOfType(CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT));
		if(pTask && (pTask->GetMainSubTaskType() == CTaskTypes::TASK_NONE))
		{
			return true;
		}
	}

	return false;
}

bool CTaskSmartFlee::ShouldReverseInVehicle() const
{
	//Ensure the flag is set.
	if(!m_uConfigFlags.IsFlagSet(CF_ReverseInVehicle))
	{
		return false;
	}

	//Check if we can reverse in the vehicle.
	if(CanReverseInVehicle())
	{
		return true;
	}

	return false;
}

bool CTaskSmartFlee::ShouldWaitBeforeExitVehicle() const
{
	//Check if we are ambient.
	if(GetPed()->PopTypeIsRandom())
	{
		//Check if we have reached the maximum number of peds exiting their vehicle.
		static const int s_iMaxPedsExitingTheirVehicle = 2;
		if(CountPedsExitingTheirVehicle() >= s_iMaxPedsExitingTheirVehicle)
		{
			return true;
		}
	}

	//Check if we can't step out of the car.
	const CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
	//NOTE[egarcia]: Ignore velocity if we have to exit immediately
	if(!pVehicle || !pVehicle->CanPedStepOutCar(GetPed(), ShouldJumpOutOfVehicle()))
	{
		return true;
	}

	//Check if we should wait due to proximity.
	if(ShouldWaitBeforeExitVehicleDueToProximity())
	{
		return true;
	}

	return false;
}

bool CTaskSmartFlee::ShouldJumpOutOfVehicle() const
{
	//Grab the vehicle.
	const CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
	if(!pVehicle)
	{
		return false;
	}

	if (!pVehicle->IsOnFire())
	{
		return false;
	}

	//Ensure our speed has exceeded the threshold.
	float fMinSpeedToJumpOutOfVehicle = sm_Tunables.m_MinSpeedToJumpOutOfVehicleOnFire;
	float fMinSpeedToJumpOutOfVehicleSq = square(fMinSpeedToJumpOutOfVehicle);
	float fSpeedSq = pVehicle->GetVelocity().Mag2();
	if(fSpeedSq < fMinSpeedToJumpOutOfVehicleSq)
	{
		return false;
	}

	return true;
}

bool CTaskSmartFlee::ShouldWaitBeforeExitVehicleDueToProximity() const
{
	//Ensure the ped is random.
	if(!GetPed()->PopTypeIsRandom())
	{
		return false;
	}

	//Ensure we should not flee our own vehicle.
	if(ShouldFleeOurOwnVehicle())
	{
		return false;
	}

	//Check if we are in a vehicle.
	const CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
	if(pVehicle)
	{
		//Ensure we are not on a bike.
		if(pVehicle->InheritsFromBike())
		{
			return false;
		}

		//Check if we are fleeing a ped.
		const CPed* pTargetPed = GetPedWeAreFleeingFrom();
		if(pTargetPed)
		{
			//Check if the target ped is standing on our vehicle.
			if(pTargetPed->GetGroundPhysical() == pVehicle)
			{
				return false;
			}
		}
	}

	//Ensure the target ped is armed with a gun.
	if(!IsTargetArmedWithGun())
	{
		return false;
	}

	//Ensure the flee target is valid.
	if(!m_FleeTarget.GetIsValid())
	{
		return false;
	}

	//Ensure the flee target position is valid.
	Vector3 vFleeFromPosition(Vector3::ZeroType);
	if(!m_FleeTarget.GetPosition(vFleeFromPosition))
	{
		return false;
	}

	//Ensure the target is close.
	ScalarV scDistSq = DistSquared(GetPed()->GetTransform().GetPosition(), VECTOR3_TO_VEC3V(vFleeFromPosition));
	ScalarV scMaxDistSq = ScalarVFromF32(square(sm_Tunables.m_FleeTargetTooCloseDistance));
	if(IsGreaterThanAll(scDistSq, scMaxDistSq))
	{
		return false;
	}

	return true;
}

void CTaskSmartFlee::UpdateCowerDueToVehicle()
{
	CPed* pPed = GetPed();

	CVehicle* pMyVehicle = pPed->GetVehiclePedInside();
	if(!pMyVehicle || !pMyVehicle->InheritsFromPlane() || !pMyVehicle->HasContactWheels())
	{
		return;
	}

	const CVehicleLayoutInfo* pLayoutInfo = pMyVehicle->GetLayoutInfo();
	if( pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanLeaveVehicle) &&
		(!pLayoutInfo || !pLayoutInfo->GetWarpInAndOut()) )
	{
		s32 iPedsSeatIndex = pMyVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
		if(iPedsSeatIndex >= 0)
		{
			VehicleEnterExitFlags vehicleFlags;
			vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::IsExitingVehicle);
			s32 iTargetEntryPoint = CModelSeatInfo::EvaluateSeatAccessFromAllEntryPoints(pPed, pMyVehicle, SR_Specific, iPedsSeatIndex, false, vehicleFlags);
			if(iTargetEntryPoint >= 0)
			{
				return;
			}
		}
	}

	m_uRunningFlags.SetFlag(RF_Cower);
}

void CTaskSmartFlee::UpdateSecondaryTaskForFleeInVehicle()
{
	//Assert that we are fleeing in a vehicle.
	taskAssert(IsFleeingInVehicle());

	//Check if we can call the police.
	if(CanCallPolice())
	{
		//Set the flag.
		m_uRunningFlags.SetFlag(RF_HasCalledPolice);

		//Call the police.
		GetPed()->GetPedIntelligence()->AddTaskSecondary(CreateSubTaskForCallPolice(), PED_TASK_SECONDARY_PARTIAL_ANIM);
	}
}

void CTaskSmartFlee::UpdateSubTaskForFleeOnFoot()
{
	//Assert that we are fleeing on foot.
	taskAssert(IsFleeingOnFoot());

	//Check if we can call the police.
	if(CanCallPolice())
	{
		//Set the flag.
		m_uRunningFlags.SetFlag(RF_HasCalledPolice);

		//Call the police.
		SetSubTaskForOnFoot(CreateSubTaskForCallPolice());
	}
	//Check if we should restart the sub-task.
	else if(ShouldRestartSubTaskForOnFoot())
	{
		//Restart the sub-task.
		RestartSubTaskForOnFoot();
		taskAssert(!ShouldRestartSubTaskForOnFoot());
	}
}

int CTaskSmartFlee::CountPedsExitingTheirVehicle()
{
	//Clear out the invalid peds.
	for(int i = 0; i < sm_aPedsExitingTheirVehicle.GetCount(); ++i)
	{
		if(!sm_aPedsExitingTheirVehicle[i])
		{
			sm_aPedsExitingTheirVehicle.DeleteFast(i--);
		}
	}

	return sm_aPedsExitingTheirVehicle.GetCount();
}

bool CTaskSmartFlee::IsPositionWithinConstraints(const CEntity& rEntity, Vec3V_In vPosition, float fMinDot, float fMaxDistance)
{
	//Ensure the min dot is exceeded.
	Vec3V vToPosition = Subtract(vPosition, rEntity.GetTransform().GetPosition());
	Vec3V vToPositionDirection = NormalizeFastSafe(vToPosition, Vec3V(V_ZERO));
	ScalarV scDot = Dot(rEntity.GetTransform().GetForward(), vToPositionDirection);
	if(IsLessThanAll(scDot, ScalarVFromF32(fMinDot)))
	{
		return false;
	}

	//Ensure we are within the max distance.
	spdAABB bbox = rEntity.GetBaseModelInfo()->GetBoundingBox();
	Vec3V vLocalPosition = rEntity.GetTransform().UnTransform(vPosition);
	ScalarV scDistSq = bbox.DistanceToPointSquared(vLocalPosition);
	ScalarV scMaxDistSq = ScalarVFromF32(square(fMaxDistance));
	if(IsGreaterThanAll(scDistSq, scMaxDistSq))
	{
		return false;
	}

	return true;
}

bool CTaskSmartFlee::ShouldConsiderPedThreatening(const CPed& rPed, const CPed& rOtherPed, bool bIncludeNonViolentWeapons)
{
	//Ensure the ped is threatening.
	if(!rPed.GetPedIntelligence()->IsPedThreatening(rOtherPed, bIncludeNonViolentWeapons))
	{
		return false;
	}

	//Check if the other ped is not a player.  Players are considered threatening at any distance.
	if(!rOtherPed.IsPlayer())
	{
		//Ensure the ped is within the maximum distance.
		ScalarV scDistSq = DistSquared(rPed.GetTransform().GetPosition(), rOtherPed.GetTransform().GetPosition());
		static float s_fMaxDistance = 90.0f;
		ScalarV scMaxDistSq = ScalarVFromF32(square(s_fMaxDistance));
		if(IsGreaterThanAll(scDistSq, scMaxDistSq))
		{
			return false;
		}
	}

	return true;
}

void CTaskSmartFlee::CleanUp(  )
{
	m_moveGroupClipSetRequestHelper.Release();
	// Only reset the on foot clipset for peds running motion ped task
	if (GetPed()->GetPrimaryMotionTask() && GetPed()->GetPrimaryMotionTask()->GetTaskType() == CTaskTypes::TASK_MOTION_PED)
	{
		//GetPed()->GetPrimaryMotionTask()->ResetOnFootClipSet();
	}

	sm_PedsFleeingOutOfPavementManager.Remove(*GetPed());

	//Reset the run-start phase.
	ResetRunStartPhase();
}

void CTaskSmartFlee::SetAmbientClips(atHashWithStringNotFinal hAmbientClips)
{
	//Ensure the ambient clips are changing.
	if(hAmbientClips == m_hAmbientClips)
	{
		return;
	}
	
	//Set the ambient clips.
	m_hAmbientClips = hAmbientClips;
	
	//Note that the ambient clips have changed.
	OnAmbientClipsChanged();
}

void CTaskSmartFlee::SetFleeingAnimationGroup(CPed* pPed)
{
	//Ensure this task does not have an on-foot clip set override.
	if(HasOnFootClipSetOverride())
	{
		return;
	}
	
	fwMvClipSetId clipSetId = MOVE_PED_BASIC_LOCOMOTION;

	// Only set the on foot clipset for peds running motion ped task
	if (pPed->GetPrimaryMotionTask() && pPed->GetPrimaryMotionTask()->GetTaskType() == CTaskTypes::TASK_MOTION_PED)
	{
		if (m_moveGroupClipSetRequestHelper.Request(clipSetId) && clipSetId != CLIP_SET_ID_INVALID)
		{
			// Set the ped's motion group
			//pPed->GetPrimaryMotionTask()->SetOnFootClipSet(clipSetId);
		}
	}
}

void CTaskSmartFlee::SetFleeFlags( int fleeFlags )
{
	Assign( m_FleeFlags, fleeFlags );
	if ( IsFlagSet(IsShortTerm) )
	{
		// 10s for this flag
		const s32 fleeTime = 10*1000;
		// have we a flee timer from our constructor
		if ( m_fleeTimer.IsSet() )
		{
			// Smaller than it's current duration?
			if ( fleeTime < m_fleeTimer.GetDuration() )
			{
				// Make us the new duration
				m_fleeTimer.ChangeDuration( fleeTime );
			}
		}
		else
		{
			m_fleeTimer.Set(fwTimer::GetTimeInMilliseconds(),fleeTime);
		}
	}
}

void CTaskSmartFlee::SetMoveBlendRatio(float fMoveBlendRatio)
{
	//Ensure the move/blend ratio is changing.
	if(Abs(fMoveBlendRatio - m_fMoveBlendRatio) < SMALL_FLOAT)
	{
		return;
	}
	
	//Set the move/blend ratio.
	m_fMoveBlendRatio = fMoveBlendRatio;
	
	//Note that the move/blend ratio changed.
	OnMoveBlendRatioChanged();
}

bool CTaskSmartFlee::CheckForNewPriorityEvent()
{
	//Ensure we have a new event.
	if(!m_bNewPriorityEvent)
	{
		return false;
	}

	//Clear the cache.
	int iEvent = m_priorityEvent;
	m_bNewPriorityEvent = false;
	m_priorityEvent = EVENT_NONE;

	//Check the event.
	switch(iEvent)
	{
		case EVENT_GUN_AIMED_AT:
		{
			//Try to scream.
			TryToScream();

			if(CanPutHandsUp())
			{
				//Put our hands up.
				SetState(State_HandsUp);

				return true;
			}

			break;
		}
		case EVENT_SHOT_FIRED:
		case EVENT_SHOT_FIRED_BULLET_IMPACT:
		case EVENT_SHOT_FIRED_WHIZZED_BY:
		{
			//Try to scream.
			TryToScream();

			//Check if our hands are up.
			if(GetState() == State_HandsUp)
			{
				//Suppress hands up.
				m_SuppressHandsUp = true;

				//Flee on foot.
				SetState(State_FleeOnFoot);

				return true;
			}
			
			break;
		}
		case EVENT_VEHICLE_DAMAGE_WEAPON:
		{
			//Try to scream.
			TryToScream();

			break;
		}
		default:
		{
			break;
		}
	}

	return false;
}

bool CTaskSmartFlee::CheckForNewAimedAtEvent()
{
	//Ensure the priority event is new.
	if(!m_bNewPriorityEvent)
	{
		return false;
	}
	
	//Check if the priority event is an "aimed at" event.
	if(m_priorityEvent != EVENT_GUN_AIMED_AT)
	{
		//Note that the "aimed at" event was not found.
		return false;
	}
	else
	{
		//Note that the priority event is no longer new.
		m_bNewPriorityEvent = false;
		
		//Clear the priority event.
		m_priorityEvent = EVENT_NONE;
		
		//Note that the "aimed at" event was found.
		return true;
	}
}

u8 CTaskSmartFlee::ComputeFleeDir(CPed* pPed)
{
	Vector3 vFleePos;
	m_FleeTarget.GetPosition(vFleePos);

	const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	u8 iFleeDir = static_cast<u8>(fwAngle::GetNodeHeadingFromVector(vPedPosition.x - vFleePos.x, vPedPosition.y - vFleePos.y));
	return iFleeDir;
}

bool CTaskSmartFlee::AreWeOutOfRange(CPed* pPed, float fDistanceToCheck)
{
	if( IsDistanceUndefined(fDistanceToCheck) )
	{
		return false;
	}
	// Check if we're far enough to stand still 
	Vector3 vFleePos;
	m_FleeTarget.GetPosition(vFleePos);

	const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	Vector3 vDiff = vFleePos - vPedPosition;

	if (vDiff.Mag2() > (fDistanceToCheck*fDistanceToCheck))
	{
		//Far away to be safe.  Make sure that a minimum distance
		//has been covered before quitting.  This stops peds 
		//starting to flee when they are almost at the safe distance
		//from the flee point and then almost immediately returning to normal.
		vDiff = m_vStartPos - vPedPosition;
		if (vDiff.Mag2() > (fDistanceToCheck*fDistanceToCheck))
		{
			return true;
		}
	}

	return false;
}

aiTaskStateTransitionTable* CTaskSmartFlee::GetTransitionSet()
{
	CFleeBehaviour::FleeType fleeType = GetFleeType(GetPed());
	taskAssert( fleeType >= CFleeBehaviour::FT_OnFoot && fleeType < CFleeBehaviour::FT_NumTypes );
	return ms_fleeTransitionTables[fleeType];
}

CFleeBehaviour::FleeType CTaskSmartFlee::GetFleeType( CPed* pPed )
{
	if (pPed->GetIsSkiing())
	{
		return CFleeBehaviour::FT_Skiing;
	}
	else if (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
	{
		return CFleeBehaviour::FT_InVehicle;
	}
	else
	{
		return CFleeBehaviour::FT_OnFoot;
	}
}

s64 CTaskSmartFlee::GenerateTransitionConditionFlags( bool UNUSED_PARAM(bStateEnded) ) 
{
	CPed* pPed = GetPed();

	s64 iConditionFlags = 0;

	// Return to original position after the fleeing has finished?
	if ( pPed->WillReturnToPositionPostFlee(m_FleeFlags ) )
	{
		iConditionFlags |= FF_ReturnToOriginalPosition;
	}

	// Should we try to use cover whilst fleeing?
	if (pPed->CanUseCover(m_FleeFlags) && taskVerifyf(pPed->GetWeaponManager(), "Ped %s is set up to use cover but doesn't have a weapon manager", pPed->GetModelName()))
	{
		iConditionFlags |= FF_WillUseCover;
	}

	return iConditionFlags;
}

void CTaskSmartFlee::UpdateFleePosition(CPed* pPed)
{
	const float fPosChangeThreshold = CalculatePositionUpdateDelta(pPed);

	// The ped checks every once in a while if the entity has moved and sets a new flee point if required.
	const CEntity* pFleeEntity = m_FleeTarget.GetEntity();
	if (pFleeEntity)
	{
		if (m_calcFleePosTimer.IsSet() && m_calcFleePosTimer.IsOutOfTime())
		{
			//reset the clock to check next time
			m_calcFleePosTimer.Set(fwTimer::GetTimeInMilliseconds(),m_iEntityPosCheckPeriod);

			//see how far we are from the entity
			Vector3 vFleePos = VEC3V_TO_VECTOR3(pFleeEntity->GetTransform().GetPosition());
			Vector3 vDiff = m_OldTargetPosition - vFleePos;

	
			const float fFleeTargetChange = vDiff.Mag2();
			if (fFleeTargetChange > (fPosChangeThreshold*fPosChangeThreshold))
			{
				// Entity has changed position a lot, set a new flee position
				m_OldTargetPosition = vFleePos;
				//after we set the flee target, we have to also set the entity
				//because SetPosition on a target blows the entity portion away
				Assertf(vFleePos.x > WORLDLIMITS_XMIN && vFleePos.y > WORLDLIMITS_YMIN && vFleePos.z > WORLDLIMITS_ZMIN, "Flee position is outside world limits (%.2f, %.2f, %.2f).", vFleePos.x, vFleePos.y, vFleePos.z);
				Assertf(vFleePos.x < WORLDLIMITS_XMAX && vFleePos.y < WORLDLIMITS_YMAX && vFleePos.z < WORLDLIMITS_ZMAX, "Flee position is outside world limits (%.2f, %.2f, %.2f).", vFleePos.x, vFleePos.y, vFleePos.z);

				// Update the movement task to goto the new flee point if there is one
				if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
				{
					CTaskComplexControlMovement* pComplexControlMovement = static_cast<CTaskComplexControlMovement*>(GetSubTask());
					aiTask* pTask = pComplexControlMovement->GetRunningMovementTask(pPed);
					if (pTask && pTask->GetTaskType() == CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH)
					{
						// Ensure that flee route is updated at least sometimes, otherwise peds may reach the end
						// of their routes and think they are safe (they will thus stop for a while before repathing)
						static dev_float fFleeMaxPosChangeSqr = 40.0f*40.0f;

						Vector3 TargetWaypoint;
						CTaskMoveFollowNavMesh* pNavMeshTask = static_cast<CTaskMoveFollowNavMesh*>(pTask);
						if (fFleeTargetChange < fFleeMaxPosChangeSqr && pNavMeshTask->GetCurrentWaypoint(TargetWaypoint))
						{
							Vector3 OurPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
							Vector3 vToWaypoint = TargetWaypoint - OurPos;
							vToWaypoint.z = 0.f;
							vToWaypoint.Normalize();

							Vector3 vToFleeTarget = vFleePos - OurPos;

							//static const float fRightPlaneDist = 1.5f;
							if (vToWaypoint.Dot(vToFleeTarget) < -0.5f)	// Target not front of us?
								return;

							static const float COS75 = 0.25f;
							
							vToFleeTarget.Normalize();
							if (vToFleeTarget.Dot(vToWaypoint) < COS75)
								return;
						}

						pNavMeshTask->SetKeepMovingWhilstWaitingForPath(true);

						pNavMeshTask->SetKeepToPavements(ShouldPreferPavements(pPed));

						pNavMeshTask->SetFleeNeverEndInWater( !m_bPedStartedInWater );
						pNavMeshTask->SetUseLargerSearchExtents(m_bUseLargerSearchExtents);
					}

					const CPed* pPed = GetPed();
					taskAssert(pPed);

					vFleePos = VEC3V_TO_VECTOR3(AdjustTargetPosByNearByPeds(*pPed, VECTOR3_TO_VEC3V(vFleePos), pFleeEntity, false));
					pComplexControlMovement->SetTarget(pPed, vFleePos);
				}
			}	 
		}
	}
	else if (m_FleeTarget.GetMode() == CAITarget::Mode_Entity) // If this happens we lost our target!
	{
		m_FleeTarget.SetPosition(m_OldTargetPosition);
	}
}

bool CTaskSmartFlee::LookForCarsToUse(CPed* pPed)
{
	Vector3 vFleeTarget;
	m_FleeTarget.GetPosition(vFleeTarget);

	Vector3 vFleeTargetToPed = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - vFleeTarget;
	float fDistFleeTargetToPed = vFleeTargetToPed.Mag();
	vFleeTargetToPed.Normalize();

	// Scan for cable car periodically
	if (m_stateTimer.IsSet() && m_stateTimer.IsOutOfTime())
	{
		// 50/50 chance of deciding to look for a vehicle,
		// or 100 chance if this is a mission ped in MP (see url:bugstar:1640814)
		if (fwRandom::GetRandomTrueFalse() || (NetworkInterface::IsGameInProgress() && pPed->PopTypeIsMission()) )
		{
			m_stateTimer.Set(fwTimer::GetTimeInMilliseconds(),static_cast<int>(ms_uiVehicleCheckPeriod));

			CEntityScannerIterator entityList = pPed->GetPedIntelligence()->GetNearbyVehicles();
			for( CEntity* pEnt = entityList.GetFirst(); pEnt; pEnt = entityList.GetNext() )
			{
				// If we are fleeing an entity, then don't try to use it to escape in!
				if(pEnt == m_FleeTarget.GetEntity())
					continue;

				if (pEnt->GetIsTypeVehicle())
				{
					CVehicle* pVehicle = static_cast<CVehicle*>(pEnt);

					// Don't use the player's last vehicle if they're on mission
					if (CTheScripts::GetPlayerIsOnAMission() && CVehiclePopulation::IsVehicleInteresting(pVehicle))
					{
						continue;
					}

					// No valid vehicle types
					if (pVehicle->GetVehicleType() == VEHICLE_TYPE_TRAILER ||
						pVehicle->GetVehicleType() == VEHICLE_TYPE_TRAIN)
					{
						continue;
					}

					if (pVehicle->GetVehicleType() == VEHICLE_TYPE_CAR)
					{
						// Perform checks to see if this vehicle is suitable for our use
						// Not checking thoroughly here can lead to glitching, as the enter-vehicle task will quit once it finds the vehicle is unsuitable
						if (!pVehicle->m_nVehicleFlags.bCanBeUsedByFleeingPeds ||
							CUpsideDownCarCheck::IsCarUpsideDown(pVehicle) ||
							g_fireMan.IsEntityOnFire(pVehicle) ||
							!pVehicle->CanBeDriven() ||
                            pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_EMERGENCY_SERVICE) // Or emergency service vehicles 
                            )	
						{
#if __BANK
							if (!CFleeDebug::ms_bForceMissionCarStealing) // Allow stealing of mission cars for script testing
							{
								continue;
							}
#else
							continue;
#endif // __BANK
						}
					}

					// Network game conditions
					if (NetworkInterface::IsInvalidVehicleForDriving(pPed, pVehicle))
					{
						continue;
					}

					// Never choose a vehicle which would entail us moving towards out flee target in order to use it
					Vector3 vToVehicle = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition()) - vFleeTarget;
					const float fDistFleeTargetToVehicle = DotProduct(vFleeTargetToPed, vToVehicle);
					if(fDistFleeTargetToVehicle > fDistFleeTargetToPed)
					{
						// Check if we can see the car
						if (pPed->GetPedIntelligence()->GetPedPerception().IsInSeeingRange(pVehicle))
						{
							// Check there is no driver
							if (!pVehicle->GetDriver())
							{
								// Don't re-use the same vehicle
								if(pVehicle != pPed->GetPedIntelligence()->GetLastVehiclePedInside())
								{
									pPed->GetPedIntelligence()->SetLastVehiclePedInside(pVehicle);
									return true;
								}
							}
						}
					}
				}
			}
		}
	}

	return false;
}

void CTaskSmartFlee::TryToLookAtTarget()
{
	//Ensure the flee entity is valid.
	const CEntity* pFleeEntity = m_FleeTarget.GetEntity();
	if(!pFleeEntity)
	{
		return;
	}

	//Ensure we have not tried to look at the target recently.
	if(fwTimer::GetTimeInMilliseconds() < m_uTimeToLookAtTargetNext)
	{
		return;
	}

	//Set the time.
	static dev_s32 s_iTimeBetweenAttemptsMin = 4000;
	static dev_s32 s_iTimeBetweenAttemptsMax = 6000;
	m_uTimeToLookAtTargetNext = fwTimer::GetTimeInMilliseconds() + (u32)(fwRandom::GetRandomNumberInRange(s_iTimeBetweenAttemptsMin, s_iTimeBetweenAttemptsMax));

	//Generate the bone tag.
	eAnimBoneTag nBoneTag = pFleeEntity->GetIsTypePed() ? BONETAG_HEAD : BONETAG_INVALID;

	m_LookRequest.SetTargetEntity(pFleeEntity);
	m_LookRequest.SetTargetBoneTag(nBoneTag);
	static dev_s32 s_iHoldTimeMin = 1000;
	static dev_s32 s_iHoldTimeMax = 3000;
	m_LookRequest.SetHoldTime((u16)(fwRandom::GetRandomNumberInRange(s_iHoldTimeMin, s_iHoldTimeMax)));
	GetPed()->GetIkManager().Request(m_LookRequest);
}

CTask::FSM_Return CTaskSmartFlee::ProcessPreFSM()
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	if(pPed->GetIsInVehicle() && pPed->GetNetworkObject())
	{
		// peds fleeing in a vehicle must be synced in the network game so they can play cowering anims, but peds in cars are not
		// synchronised by default, in order to save bandwidth as they don't do much
		NetworkInterface::ForceSynchronisation(pPed);
	}

	//Process the targets.
	if(!CFleeHelpers::ProcessTargets(m_FleeTarget, m_SecondaryTarget, m_vLastTargetPosition))
	{
		return FSM_Quit;
	}

	// Check if the target entity is no longer valid and notify
	if (m_FleeTarget.GetMode() == CAITarget::Mode_Entity && !IsValidTargetEntity(m_FleeTarget.GetEntity()))
	{
		OnTargetEntityNoLongerValid();
	}

	if(GetState() == State_HandsUp)
	{
		//Keep scanning for shocking events if you have your hands up
		pPed->GetPedIntelligence()->SetCheckShockFlag(true);
	}

	// Allow certain lod modes
	pPed->GetPedAiLod().ClearBlockedLodFlag(CPedAILod::AL_LodPhysics);
	pPed->GetPedAiLod().ClearBlockedLodFlag(CPedAILod::AL_LodMotionTask);
	
	SetFleeingAnimationGroup(pPed);

	//
	UpdatePreferPavements(pPed);

	// If we're fleeing from an entity, update it's position
	UpdateFleePosition(pPed);

	if (pPed->GetPedIntelligence()->GetFleeBehaviour().GetFleeFlags().IsFlagSet(CFleeBehaviour::BF_UpdateToNearestHatedPed))
	{
		CEntityScannerIterator entityList = pPed->GetPedIntelligence()->GetNearbyPeds();
		for( CEntity* pEntity = entityList.GetFirst(); pEntity; pEntity = entityList.GetNext() )
		{
			if (pEntity != m_FleeTarget.GetEntity())
			{
				CPed* pOtherPed = static_cast<CPed*>(pEntity);
				if(pOtherPed && pOtherPed->GetPedIntelligence()->IsThreatenedBy(*pPed))
				{
					static dev_float s_fHatedTargetUpdateDistanceSqr = 100.0f;
					const float fDistToOtherPedSqd = DistSquared(pPed->GetTransform().GetPosition(), pOtherPed->GetTransform().GetPosition()).Getf();
					if (fDistToOtherPedSqd < s_fHatedTargetUpdateDistanceSqr)
					{
						// The list is sorted by distance so this will be the closest ped.
						
						// B*4445200: If we're fleeing from a position rather than an entity, only update position of new threat. This stops TASK_SMART_FLEE_COORD in MP converting from position target to entity target.
						if (NetworkInterface::IsGameInProgress() && !m_FleeTarget.GetEntity())
						{
							m_FleeTarget.SetPosition(VEC3V_TO_VECTOR3(pOtherPed->GetTransform().GetPosition()));
						}
						else
						{
							m_FleeTarget.SetEntity(pOtherPed);
						}

						break;
					}
				}
			}
		}		
	}

	pPed->SetPedResetFlag( CPED_RESET_FLAG_KeepCoverPoint, true );

	if(!m_uConfigFlags.IsFlagSet(CF_DisablePanicInVehicle))
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_PanicInVehicle, true);
	}

	//Process the move task.
	ProcessMoveTask();

	//Try to look at the target.
	TryToLookAtTarget();

	return FSM_Continue;
}

CTask::FSM_Return CTaskSmartFlee::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		// Start
		FSM_State(State_Start)
			FSM_OnEnter
			Start_OnEnter(pPed);
		FSM_OnUpdate
			return Start_OnUpdate(pPed);		

		// Exit the vehicle the ped is currently in
		FSM_State(State_ExitVehicle)
			FSM_OnEnter
				ExitVehicle_OnEnter(pPed);
			FSM_OnUpdate
				return ExitVehicle_OnUpdate(pPed);
			FSM_OnExit
				ExitVehicle_OnExit();
				
		// Wait before exiting the vehicle
		FSM_State(State_WaitBeforeExitVehicle)
			FSM_OnEnter
				WaitBeforeExitVehicle_OnEnter();
			FSM_OnUpdate
				return WaitBeforeExitVehicle_OnUpdate();

		// Flee
		FSM_State(State_FleeOnFoot)
			FSM_OnEnter
				FleeOnFoot_OnEnter(pPed);
			FSM_OnUpdate
				return FleeOnFoot_OnUpdate(pPed);
			FSM_OnExit
				FleeOnFoot_OnExit();

		FSM_State(State_FleeOnSkis)
			FSM_OnEnter
				FleeOnSkis_OnEnter(pPed);
			FSM_OnUpdate
				return FleeOnSkis_OnUpdate(pPed);

		// SeekCover
		FSM_State(State_SeekCover)
			FSM_OnEnter
				SeekCover_OnEnter(pPed);
			FSM_OnUpdate
				return SeekCover_OnUpdate(pPed);

		// CrouchAtCover
		FSM_State(State_CrouchAtCover)
			FSM_OnEnter
				CrouchAtCover_OnEnter(pPed);
			FSM_OnUpdate
				return CrouchAtCover_OnUpdate(pPed);

		// ReturnToStartPosition
		FSM_State(State_ReturnToStartPosition)
			FSM_OnEnter
				ReturnToStartPosition_OnEnter(pPed);
			FSM_OnUpdate
				return ReturnToStartPosition_OnUpdate(pPed);

		// EnterVehicle
		FSM_State(State_EnterVehicle)
			FSM_OnEnter
				EnterVehicle_OnEnter(pPed);
			FSM_OnUpdate
				return EnterVehicle_OnUpdate(pPed);

		// FleeInVehicle
		FSM_State(State_FleeInVehicle)
			FSM_OnEnter
				FleeInVehicle_OnEnter(pPed);
			FSM_OnUpdate
				return FleeInVehicle_OnUpdate(pPed);
			FSM_OnExit
				FleeInVehicle_OnExit();

		// ReverseInVehicle
		FSM_State(State_ReverseInVehicle)
			FSM_OnEnter
				ReverseInVehicle_OnEnter();
			FSM_OnUpdate
				return ReverseInVehicle_OnUpdate();

		// EmergencyStopInVehicle
		FSM_State(State_EmergencyStopInVehicle)
			FSM_OnEnter
			EmergencyStopInVehicle_OnEnter();
		FSM_OnUpdate
			return EmergencyStopInVehicle_OnUpdate();

		// AccelerateInVehicle
		FSM_State(State_AccelerateInVehicle)
			FSM_OnEnter
				AccelerateInVehicle_OnEnter();
			FSM_OnUpdate
				return AccelerateInVehicle_OnUpdate();

		// StandStill
		FSM_State(State_StandStill)
			FSM_OnEnter
				StandStill_OnEnter(pPed);
			FSM_OnUpdate
				return StandStill_OnUpdate(pPed);

		// HandsUp
		FSM_State(State_HandsUp)
			FSM_OnEnter
				HandsUp_OnEnter(pPed);
			FSM_OnUpdate
				return HandsUp_OnUpdate(pPed);

		FSM_State(State_TakeOffSkis)
			FSM_OnEnter
				TakeOffSkis_OnEnter(pPed);
			FSM_OnUpdate
				return TakeOffSkis_OnUpdate(pPed);

		FSM_State(State_Cower)
			FSM_OnEnter
				Cower_OnEnter();
			FSM_OnUpdate
				return Cower_OnUpdate();

		FSM_State(State_CowerInVehicle)
			FSM_OnEnter
				CowerInVehicle_OnEnter();
			FSM_OnUpdate
				return CowerInVehicle_OnUpdate();

		// End quit - its finished.
		FSM_State(State_Finish)
			FSM_OnEnter
				// If ped has a default wander task, set the direction to wander in
				SetDefaultTaskWanderDir(pPed);
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

void CTaskSmartFlee::Start_OnEnter(CPed* pPed)
{
	//Initialize the secondary target.
	CFleeHelpers::InitializeSecondaryTarget(m_FleeTarget, m_SecondaryTarget);

	//Record the position where the ped begins to flee
	m_vStartPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

	// Include fish, which do not drown, in this check.
	m_bPedStartedInWater = pPed->GetPedResetFlag( CPED_RESET_FLAG_IsDrowning ) || 
		(!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DrownsInWater) && (pPed->GetIsInWater() || pPed->GetIsSwimming()));

	// Do an initial update on our cower options
	UpdateCowerDueToVehicle();

	if(pPed->IsNetworkClone())
	{
		pPed->GetPedIntelligence()->SetLastVehiclePedInside(m_pCloneMyVehicle);
	}
	else if (pPed->GetIsInVehicle() && !pPed->GetPedIntelligence()->GetLastVehiclePedInside())
	{
		pPed->GetPedIntelligence()->SetLastVehiclePedInside(pPed->GetVehiclePedInside());
	}
}

CTask::FSM_Return CTaskSmartFlee::Start_OnUpdate(CPed* pPed)
{
	//Check if we should cower.
	if(ShouldCower())
	{
		if(!pPed->GetIsInVehicle())
		{
			SetState(State_Cower);
		}
		else
		{
			SetState(State_CowerInVehicle);
		}

		return FSM_Continue;
	}

	const CEntity* pFleeEntity = m_FleeTarget.GetEntity();
	if (pFleeEntity)
	{
		// Look at the entity we're fleeing from if it exists
		// Unless we're an animal.
		if (!pPed->GetIkManager().IsLooking() && pPed->GetPedType() != PEDTYPE_ANIMAL)
		{
			if (pFleeEntity->GetIsTypePed())
			{
				pPed->GetIkManager().LookAt(0, pFleeEntity, 3000, BONETAG_HEAD, NULL, LF_FAST_TURN_RATE );
			}
			else
			{
				pPed->GetIkManager().LookAt(0, pFleeEntity, 3000, BONETAG_INVALID, NULL, LF_FAST_TURN_RATE );
			}
		}
	}

	m_iFleeDir = ComputeFleeDir(pPed);

	//Check if we are not in a vehicle.
	if(!pPed->GetIsInVehicle())
	{
		//Check if we should enter our last used vehicle immediately.
		if(ShouldEnterLastUsedVehicleImmediately())
		{
			//Set the vehicle.
			CVehicle *pVehicle = GetPed()->GetMyVehicle();
			pPed->GetPedIntelligence()->SetLastVehiclePedInside(pVehicle);
			taskAssert(pVehicle);

			//Enter the vehicle.
			SetState(State_EnterVehicle);
		}
		else
		{
			//Pick a state transition.
			PickNewStateTransition(State_Finish);
		}
	}
	else
	{
		//Check if we can exit the vehicle, and either the vehicle has no driver, or we should flee our own vehicle.
		if(CanExitVehicle() && (DoesVehicleHaveNoDriver() || ShouldFleeOurOwnVehicle()))
		{
			//Exit the vehicle.
			SetStateForExitVehicle();
		}
		else
		{
			//Check if we should accelerate or reverse.
			bool bAccelerate	= ShouldAccelerateInVehicle();
			bool bReverse		= ShouldReverseInVehicle();
			if(bAccelerate && bReverse)
			{
				//Calculate the chances.
				float fChancesToAccelerate = pPed->CheckBraveryFlags(BF_CAN_ACCELERATE_IN_CAR) ?
					sm_Tunables.m_ChancesToAccelerateAggressive : sm_Tunables.m_ChancesToAccelerateTimid;

				// Check if we should accelerate.
				if(fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < fChancesToAccelerate)
				{
					bReverse = false;
				}
				else
				{
					bAccelerate = false;
				}
			}
			taskAssert(!bAccelerate || !bReverse);

			//Check if we should accelerate in the vehicle.
			if(bAccelerate)
			{
				//Accelerate in the vehicle.
				SetState(State_AccelerateInVehicle);
			}
			//Check if we should reverse in the vehicle.
			else if(bReverse)
			{
				//Reverse in the vehicle.
				SetState(State_ReverseInVehicle);
			}
			//Check if we can't exit the vehicle, and are a passenger.
			else if(!CanExitVehicle() && !pPed->GetIsDrivingVehicle())
			{
				//Cower in the vehicle.
				SetState(State_CowerInVehicle);
			}
			else
			{
				//Flee in the vehicle.
				SetState(State_FleeInVehicle);
			}
		}
	}
	
	return FSM_Continue;
}

void CTaskSmartFlee::ExitVehicle_OnEnter(CPed* pPed)
{
	//Ensure the vehicle is valid.
	CVehicle *pVehicle = pPed->GetVehiclePedInside();
	pPed->GetPedIntelligence()->SetLastVehiclePedInside(pVehicle);
	if(!pVehicle)
	{
		return;
	}

	//Check if we have been trapped.
	if(m_uRunningFlags.IsFlagSet(RF_HasBeenTrapped))
	{
		//Say some audio.
		GetPed()->NewSay("GENERIC_INSULT_HIGH");
	}

	//Note that the ped is exiting the vehicle.
	OnExitingVehicle();
	
	// Get out of the vehicle immediately, even if the car is moving, forced leave, don't bother closing the door
	VehicleEnterExitFlags vehicleFlags;
	vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DontCloseDoor);

	if (ShouldJumpOutOfVehicle())
	{
		vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DontWaitForCarToStop);
		vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::JumpOut);
	}
	else
	{
		vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DelayForTime);
	}

	//Check if we should use a flee exit.
	bool bUseFleeExit = !pVehicle->IsOnItsSide() && !pVehicle->IsUpsideDown();
	if(bUseFleeExit)
	{
		vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::IsFleeExit);
	}
	
	//Calculate the delay time.
	float fMinDelayTime = sm_Tunables.m_MinDelayTimeForExitVehicle;
	float fMaxDelayTime = sm_Tunables.m_MaxDelayTimeForExitVehicle;
	float fDelayTime = fwRandom::GetRandomNumberInRange(fMinDelayTime, fMaxDelayTime);
	
	//Create the task.
	CTask* pTask = rage_new CTaskExitVehicle(pVehicle, vehicleFlags, fDelayTime);
	
	//Start the task.
	SetNewTask(pTask);

	// Make sure the look IK is on immediately.
	m_uTimeToLookAtTargetNext = 0;
}

void CTaskSmartFlee::ExitVehicle_OnEnterClone(CPed* pPed)
{
    CTask *pExitVehicleTask = pPed->GetPedIntelligence()->CreateCloneTaskForTaskType(CTaskTypes::TASK_EXIT_VEHICLE);

    if(pExitVehicleTask)
    {
        SetNewTask(pExitVehicleTask);
    }
}

CTask::FSM_Return CTaskSmartFlee::ExitVehicle_OnUpdate(CPed* pPed)
{
	//Check if we are trying to follow a route.
	if(m_pExistingMoveTask && (m_pExistingMoveTask->GetTaskType() == CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH) &&
		(static_cast<CTaskMoveFollowNavMesh *>(m_pExistingMoveTask.Get())->IsWaitingToLeaveVehicle()))
	{
		static dev_float s_fPhase = 0.25f;
		SetRunStartPhase(s_fPhase);
	}

	//Check if the sub-task has finished.
	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Check if we are not in a vehicle.
		if(!pPed->GetIsInVehicle())
		{
			// Check if we should delete the ped as a performance optimization:
			// --
			// Check random chance
			const float fChance = fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
			if( fChance < sm_Tunables.m_ChanceToDeleteOnExitVehicle )
			{
				// Check distance to local player is beyond minimum threshold
				const CPed* pLocalPlayerPed = CGameWorld::FindLocalPlayer();
				if( pLocalPlayerPed )
				{
					const ScalarV distToLocalPlayerSq_MIN = ScalarV( square(sm_Tunables.m_MinDistFromPlayerToDeleteOnExitVehicle) );
					const ScalarV distToLocalPlayerSq = DistSquared(pLocalPlayerPed->GetTransform().GetPosition(), pPed->GetTransform().GetPosition());
					if( IsGreaterThanAll(distToLocalPlayerSq, distToLocalPlayerSq_MIN) )
					{
						// Check ped is offscreen
						const bool bCheckNetworkPlayersScreens = true;
						if( !pPed->GetIsOnScreen(bCheckNetworkPlayersScreens) )
						{
							// Check ped can be deleted
							const bool bPedsInCarsCanBeDeleted = false;// we aren't in a car anyway
							const bool bDoNetworkChecks = true; // we do want network checks (this is the default)
							if( pPed->CanBeDeleted(bPedsInCarsCanBeDeleted, bDoNetworkChecks) )
							{
								// Mark ped for deletion
								pPed->FlagToDestroyWhenNextProcessed();

								// Quit the task
								return FSM_Quit;
							}
						}
					}
				}
			}

			//Compute the flee direction.
			m_iFleeDir = ComputeFleeDir(pPed);

			//Move to the flee on foot state.
			SetState(State_FleeOnFoot);
		}
		else
		{
			//Wait before exiting the vehicle.
			SetState(State_WaitBeforeExitVehicle);
		}
	}
	
	return FSM_Continue;
}

CTask::FSM_Return CTaskSmartFlee::ExitVehicle_OnUpdateClone(CPed* UNUSED_PARAM(pPed))
{
    if(GetIsSubtaskFinished(CTaskTypes::TASK_EXIT_VEHICLE))
	{
		if( GetStateFromNetwork() != State_ExitVehicle )
		{
			SetState(GetStateFromNetwork());
		}
	}
	else 
	{
		if( GetStateFromNetwork() == State_WaitBeforeExitVehicle )
		{
			SetState(GetStateFromNetwork());
		}
	}
	return FSM_Continue;
}

void CTaskSmartFlee::ExitVehicle_OnExit()
{
	//Note that the ped is no longer exiting the vehicle.
	OnNoLongerExitingVehicle();
}

void CTaskSmartFlee::WaitBeforeExitVehicle_OnEnter()
{
	//Set the time we last checked if we should stop waiting before exiting the vehicle.
	m_uTimeWeLastCheckedIfWeShouldStopWaitingBeforeExitingTheVehicle = fwTimer::GetTimeInMilliseconds();

	//Check if we are driving the vehicle.
	if(GetPed()->GetIsDrivingVehicle())
	{
		//Create the vehicle task.
		CTask* pVehicleTask = rage_new CTaskVehicleStop(CTaskVehicleStop::SF_DontTerminateWhenStopped);

		//Create the task.
		CTask* pTask = rage_new CTaskControlVehicle(GetPed()->GetVehiclePedInside(), pVehicleTask);

		//Start the task.
		SetNewTask(pTask);
	}
}

CTask::FSM_Return CTaskSmartFlee::WaitBeforeExitVehicle_OnUpdate()
{
	//Grab the ped.
	CPed* pPed = GetPed();

	//Check if the ped is no longer in a vehicle.
	const CVehicle* pVehicle = pPed->GetVehiclePedInside();
	if(!pVehicle)
	{
		//Flee on foot.
		SetState(State_FleeOnFoot);
	}
	else
	{
		//Check if the timer has elapsed.
		if((m_uTimeWeLastCheckedIfWeShouldStopWaitingBeforeExitingTheVehicle + 1000) < fwTimer::GetTimeInMilliseconds())
		{
			//Set the time.
			m_uTimeWeLastCheckedIfWeShouldStopWaitingBeforeExitingTheVehicle = fwTimer::GetTimeInMilliseconds();

			//Check if the state differs.
			s32 iState = ChooseStateForExitVehicle();
			if(iState != GetState())
			{
				SetState(iState);
			}
			else
			{
				//Check if we can reverse or accelerate.
				static dev_float s_fMinTime = 1.5f;
				bool bCanReverseOrAccelerate = (GetTimeInState() >= s_fMinTime);

				//Check if we can reverse.
				if(bCanReverseOrAccelerate && CanReverseInVehicle())
				{
					SetState(State_ReverseInVehicle);
				}
				//Check if we can accelerate.
				else if(bCanReverseOrAccelerate && CanAccelerateInVehicle())
				{
					SetState(State_AccelerateInVehicle);
				}
				else
				{
					//Say some audio.
					TryToPlayTrappedSpeech();

					//Note that we have been trapped.
					m_uRunningFlags.SetFlag(RF_HasBeenTrapped);

					//just exit if we've been stuck for over five seconds and can get out the car
					static dev_float s_fMinTimeBeforeExit = 5.0f;
					if(GetTimeInState() > s_fMinTimeBeforeExit)
					{
						if(pVehicle->CanPedStepOutCar(GetPed()))
						{
							SetState(State_ExitVehicle);
						}		
					}
				}
			}
		}
	}

	//Check for a new priority event.
	CheckForNewPriorityEvent();
	
	return FSM_Continue;
}

void CTaskSmartFlee::FleeOnFoot_OnEnter(CPed* pPed)
{
	const CEntity* pFleeEntity = m_FleeTarget.GetEntity();
	if (pFleeEntity)
	{
		m_calcFleePosTimer.Set(fwTimer::GetTimeInMilliseconds(),m_iEntityPosCheckPeriod);
#if __ASSERT
		CheckForOriginFlee();
#endif // __ASSERT
	}

	// Don't reset this if we just failed to enter a vehicle.
	// This avoids a potential infinite task loop.
	if (GetPreviousState() != State_EnterVehicle)
	{
		// set timer
		m_stateTimer.Set(fwTimer::GetTimeInMilliseconds(),static_cast<int>(ms_uiVehicleCheckPeriod));
		// ensure we scan immediately so peds don't appear to start fleeing & then change their minds
		m_stateTimer.SubtractTime(static_cast<int>(ms_uiVehicleCheckPeriod));
	}

	CTaskMove* pTask = NULL;
	CTaskMoveFollowNavMesh* pMoveTask = NULL;

	if (m_pExistingMoveTask && m_pExistingMoveTask->IsMoveTask())
	{
		// grab the move task we made earlier and put on the ped
		pTask = static_cast<CTaskMove*>(m_pExistingMoveTask.Get());
		if (pTask->GetTaskType() == CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH)
		{
			pMoveTask = static_cast<CTaskMoveFollowNavMesh*>(pTask);
		}

		m_pExistingMoveTask = NULL;
	}
	else
	{
		//Calculate the target position.
		Vec3V vFleePos;
		m_FleeTarget.GetPosition(RC_VECTOR3(vFleePos));

		//Avoid nearby peds
		vFleePos = AdjustTargetPosByNearByPeds(*pPed, vFleePos, m_FleeTarget.GetEntity(), true);

		//Create the move task.
		pMoveTask = CreateMoveTaskForFleeOnFoot(vFleePos);

		//Set the spheres of influence builder.
		pMoveTask->SetInfluenceSphereBuilderFn(SpheresOfInfluenceBuilder);

		// If the ped is already fleeing using the navmesh, then instruct the navmesh new task to keep moving whilst
		// the path is calculated - this prevents the ped from stopping briefly whilst fleeing.
		CTaskMoveFollowNavMesh * pExistingNavMeshTask = (CTaskMoveFollowNavMesh*) pPed->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH);
		if(pExistingNavMeshTask && pExistingNavMeshTask->GetIsFleeing() && pExistingNavMeshTask->IsFollowingNavMeshRoute())
		{
			pMoveTask->SetKeepMovingWhilstWaitingForPath(true);
		}

		static dev_float KeepMovingSpeedThresholdSqr = square(2.5f);

		Vector2 vCurrMoveRatio;
		pPed->GetMotionData()->GetCurrentMoveBlendRatio(vCurrMoveRatio);
		if (vCurrMoveRatio.y >= MBR_RUN_BOUNDARY || pPed->GetVelocity().XYMag2() > KeepMovingSpeedThresholdSqr)
			pMoveTask->SetKeepMovingWhilstWaitingForPath(true);

		pMoveTask->SetDontStandStillAtEnd(true);

		pTask = pMoveTask;
	}

	//Check if the move task is valid.
	if(pMoveTask)
	{
		//Set the move/blend ratio.
		pMoveTask->SetMoveBlendRatio(m_fMoveBlendRatio);
		pMoveTask->SetSofterFleeHeuristics(m_fMoveBlendRatio < MBR_RUN_BOUNDARY);

		//Copy over the safe flee distance.
		const dev_float fFleeForeverDist = 99999.0f;
		const dev_float fHysteresisDist = 1.0f;
		const float fFleeDist = (m_fStopDistance > 0.0f) ? (m_fStopDistance + fHysteresisDist) : fFleeForeverDist;
		pMoveTask->SetFleeSafeDist(fFleeDist);

		// Add this reqest to the highest priority list
		if (GetPreviousState() == State_ExitVehicle)
		{
			pMoveTask->SetHighPrioRoute(true);
			pMoveTask->SetConsiderRunStartForPathLookAhead(true);	// To make the ped do the runstart in the right direction
		}

		if (m_bConsiderRunStartForPathLookAhead)
		{
			pMoveTask->SetConsiderRunStartForPathLookAhead(true);
		}

		//Check if we should prefer pavements.
		pMoveTask->SetKeepToPavements(ShouldPreferPavements(pPed));

		//Check if we should never leave pavements.
		if(m_bNeverLeavePavements)
		{
			pMoveTask->SetNeverLeavePavements(true);
		}

		//Check if we should keep moving whilst waiting for the path.
		if (m_bKeepMovingWhilstWaitingForFleePath)
		{
			if (m_bKeepMovingOnlyIfMoving)
			{
				static dev_float KeepMovingSpeedThresholdSqr = square(0.25f);

				Vector2 vCurrMoveRatio;
				pPed->GetMotionData()->GetCurrentMoveBlendRatio(vCurrMoveRatio);
				if (vCurrMoveRatio.y >= MBR_WALK_BOUNDARY || pPed->GetVelocity().XYMag2() > KeepMovingSpeedThresholdSqr)
					pMoveTask->SetKeepMovingWhilstWaitingForPath(true);
			}
			else
			{
				pMoveTask->SetKeepMovingWhilstWaitingForPath(true);
			}
		}

//		pMoveTask->SetAvoidPeds(false);

		pMoveTask->SetFleeNeverEndInWater( !m_bPedStartedInWater );
		pMoveTask->SetUseLargerSearchExtents(m_bUseLargerSearchExtents);
		pMoveTask->SetPullBackFleeTargetFromPed(m_bPullBackFleeTargetFromPed);

		if((GetConfigFlags()&CF_NeverLeaveWater)!=0)
			pMoveTask->SetNeverLeaveWater(true);

		if((GetConfigFlags()&CF_NeverEnterWater)!=0)
			pMoveTask->SetNeverEnterWater(true);

		if ((GetConfigFlags()&CF_FleeAtCurrentHeight)!=0)
			pMoveTask->SetFollowNavMeshAtCurrentHeight(true);

		if ((GetConfigFlags()&CF_NeverLeaveDeepWater)!=0)
			pMoveTask->SetNeverLeaveDeepWater(true);
	}

	//Create the sub-task.
	CTask* pSubTask = CreateSubTaskForAmbientClips();

	// Need to notify ComplexControlMovement if the provided move task is already running.
	bool bMoveTaskAlreadyRunning = false;
	if (pTask==static_cast<CTaskMove*>(pPed->GetPedIntelligence()->GetGeneralMovementTask()))
	{
		bMoveTaskAlreadyRunning = true;
	}

	SetNewTask(rage_new CTaskComplexControlMovement(pTask, pSubTask, CTaskComplexControlMovement::TerminateOnMovementTask, 0.0f, bMoveTaskAlreadyRunning));

	InitPreferPavements();
}

//
Vec3V_Out CTaskSmartFlee::AdjustTargetPosByNearByPeds(const CPed& rPed, Vec3V_In vFleePosition, const CEntity* pFleeEntity, bool bForceScanNearByPeds, ScalarV_Ptr pscCrowdNormalizedSize NOTFINAL_ONLY(, Vec3V_Ptr pvExitCrowdDir, Vec3V_Ptr pvWallCollisionPosition, Vec3V_Ptr pvWallCollisionNormal, Vec3V_Ptr pvBeforeWallCheckFleeAdjustedDir, ScalarV_Ptr pscNearbyPedsTotalWeight, Vec3V_Ptr pvNearbyPedsCenterOfMass, atArray<Vec3V>* paFleePositions, atArray<ScalarV>* paFleeWeights))
{
	// Initi out params
#if !__FINAL
	if (paFleePositions)
	{
		paFleePositions->clear();
	}

	if (paFleeWeights)
	{
		paFleeWeights->clear();
	}

	if (pvNearbyPedsCenterOfMass)
	{
		*pvNearbyPedsCenterOfMass = Vec3V(V_ZERO);
	}

	if (pvBeforeWallCheckFleeAdjustedDir)
	{
		*pvBeforeWallCheckFleeAdjustedDir = Vec3V(V_ZERO);
	}

	if (pvWallCollisionPosition)
	{
		*pvWallCollisionPosition = Vec3V(V_ZERO);
	}

	if (pvWallCollisionNormal)
	{
		*pvWallCollisionNormal = Vec3V(V_ZERO);
	}

#endif

	if (pscCrowdNormalizedSize)
	{
		*pscCrowdNormalizedSize = ScalarV(V_ZERO);
	}

	// NOTE[egarcia]: We only allow this behaviour for multiplayer, random peds
	if (!NetworkInterface::IsGameInProgress() || !rPed.PopTypeIsRandom())
	{
		return vFleePosition;
	}

	ScalarV scDIST_TOO_CLOSE(10.0f);

	ScalarV scSMALL_CROWD_NEARBY_PEDS_WEIGHT(1.0f);
	ScalarV scSMALL_CROWD_MAX_FLEE_DIR_ANGLE_DELTA(1.3f);

	ScalarV scBIG_CROWD_NEARBY_PEDS_WEIGHT(5.0f);
	ScalarV scBIG_CROWD_MAX_FLEE_DIR_ANGLE_DELTA(1.2f);

#if __BANK
	TUNE_GROUP_BOOL(TASK_SMART_FLEE, bAdjustTargetPosByNearPedsEnabled, true);
	if (!bAdjustTargetPosByNearPedsEnabled)
	{
		return vFleePosition;
	}

	TUNE_GROUP_FLOAT(TASK_SMART_FLEE, fDistTooClose, scDIST_TOO_CLOSE.Getf(), 0.0f, 1000.0f, 0.5f);
	scDIST_TOO_CLOSE.Setf(fDistTooClose);

	TUNE_GROUP_FLOAT(TASK_SMART_FLEE, fSmallCrowdNearbyPedsWeight, scSMALL_CROWD_NEARBY_PEDS_WEIGHT.Getf(), 0.0f, 100.0f, 0.1f);
	scSMALL_CROWD_NEARBY_PEDS_WEIGHT.Setf(fSmallCrowdNearbyPedsWeight);
	TUNE_GROUP_FLOAT(TASK_SMART_FLEE, fSmallCrowdMaxFleeDirAngleDelta, scSMALL_CROWD_MAX_FLEE_DIR_ANGLE_DELTA.Getf(), 0.0f, 3.14159f, 0.1f);
	scSMALL_CROWD_MAX_FLEE_DIR_ANGLE_DELTA.Setf(fSmallCrowdMaxFleeDirAngleDelta);

	TUNE_GROUP_FLOAT(TASK_SMART_FLEE, fBigCrowdNearbyPedsWeight, scBIG_CROWD_NEARBY_PEDS_WEIGHT.Getf(), 0.0f, 100.0f, 0.1f);
	scBIG_CROWD_NEARBY_PEDS_WEIGHT.Setf(fBigCrowdNearbyPedsWeight);
	TUNE_GROUP_FLOAT(TASK_SMART_FLEE, fBigCrowdMaxFleeDirAngleDelta, scBIG_CROWD_MAX_FLEE_DIR_ANGLE_DELTA.Getf(), 0.0f, 3.14159f, 0.1f);
	scBIG_CROWD_MAX_FLEE_DIR_ANGLE_DELTA.Setf(fBigCrowdMaxFleeDirAngleDelta);
#endif // __BANK

	const ScalarV scDistTooCloseSqr(scDIST_TOO_CLOSE * scDIST_TOO_CLOSE);

	Vec3V vPedPosition = rPed.GetTransform().GetPosition();
	Vec3V vAdjustedFleePosition = vFleePosition;

	Vec3V vNearbyPedsCenterOfMass(V_ZERO);
	ScalarV scNearByPedsWeightSum(V_ZERO);
	u32 uNumNearByPeds = 0;

	if (bForceScanNearByPeds)
	{
		// If entity scanning is off, we must force a scan or be unaware of nearby peds
		if(rPed.GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodEntityScanning))
		{
			rPed.GetPedIntelligence()->GetPedScanner()->ScanForEntitiesInRange(const_cast<CPed&>(rPed), true);
		}
		else
		{
			if(rPed.GetPedIntelligence()->GetPedScanner()->GetTimer().Tick())
			{
				rPed.GetPedIntelligence()->GetPedScanner()->GetTimer().SetCount(0);
				rPed.GetPedIntelligence()->GetPedScanner()->ScanForEntitiesInRange(const_cast<CPed&>(rPed));
			}
		}
	}

	Vec3V vClearDir(V_ZERO);

	const CEntityScannerIterator entityList = rPed.GetPedIntelligence()->GetNearbyPeds();
	for (const CEntity* pEnt = entityList.GetFirst(); pEnt; pEnt = entityList.GetNext())
	{
		const CPed * pOtherPed = SafeCast(const CPed, pEnt);
		if(pOtherPed->IsDead() || pOtherPed->IsFatallyInjured() || pOtherPed == pFleeEntity)
			continue;

		// Sum up vectors away from all peds which are too close
		Vec3V vDiff = pOtherPed->GetTransform().GetPosition() - rPed.GetTransform().GetPosition();
		ScalarV scDiffSqr = MagSquared(vDiff);
		if (IsGreaterThanAll(scDiffSqr, ScalarV(V_FLT_SMALL_2)) && IsLessThanAll(scDiffSqr, scDistTooCloseSqr))
		{
			const ScalarV scDiff = Sqrt(scDiffSqr);
			const ScalarV scWeight = square((scDIST_TOO_CLOSE - scDiff) / scDIST_TOO_CLOSE);
			vNearbyPedsCenterOfMass += pOtherPed->GetTransform().GetPosition() * scWeight;
			
			Vec3V vFleeFromOtherPedDir = -vDiff / scDiff;

			if (IsZeroAll(scNearByPedsWeightSum))
			{
				vClearDir = vFleeFromOtherPedDir;
			}
			else
			{
				const ScalarV scAvoidOtherPedMaxAngleDelta = Clamp(scWeight / scNearByPedsWeightSum * ScalarV(0.5f), ScalarV(V_ZERO), ScalarV(V_ONE)) * ScalarV(3.14159f);
				ScalarV scAvoidOtherPedAngleDelta = Clamp(AngleZ(vClearDir, vFleeFromOtherPedDir), -scAvoidOtherPedMaxAngleDelta, scAvoidOtherPedMaxAngleDelta);
				vClearDir = RotateAboutZAxis(vClearDir, scAvoidOtherPedAngleDelta);
			}

			scNearByPedsWeightSum += scWeight;
			++uNumNearByPeds;

#if !__FINAL
			if (paFleePositions)
			{
				paFleePositions->PushAndGrow(pOtherPed->GetTransform().GetPosition());
			}

			if (paFleeWeights)
			{
				paFleeWeights->PushAndGrow(scWeight);
			}
#endif
		}
	}

	// Calculate center of mass
	if (IsGreaterThanAll(scNearByPedsWeightSum, ScalarV(V_ZERO)))
	{
		vNearbyPedsCenterOfMass /= scNearByPedsWeightSum;
#if !__FINAL
		if (pscNearbyPedsTotalWeight)
		{
			*pscNearbyPedsTotalWeight = scNearByPedsWeightSum;
		}

		if (pvNearbyPedsCenterOfMass)
		{
			*pvNearbyPedsCenterOfMass = vNearbyPedsCenterOfMass;
		}

		if (pvExitCrowdDir)
		{
			*pvExitCrowdDir = vClearDir;
		}
#endif // 
	}
	
	// Calculate crowd size and crowd escape direction
	if (IsGreaterThanAll(scNearByPedsWeightSum, scSMALL_CROWD_NEARBY_PEDS_WEIGHT))
	{
		const ScalarV scCrowdNormalizedSize = Clamp((scNearByPedsWeightSum - scSMALL_CROWD_NEARBY_PEDS_WEIGHT) / (scBIG_CROWD_NEARBY_PEDS_WEIGHT - scSMALL_CROWD_NEARBY_PEDS_WEIGHT), ScalarV(V_ZERO), ScalarV(V_ONE));
		if (pscCrowdNormalizedSize)
		{
			*pscCrowdNormalizedSize = scCrowdNormalizedSize;
		}

		const Vec3V vToFleePosition = vFleePosition - vPedPosition;
		const ScalarV scFleePositionDistanceSqr = MagSquared(vToFleePosition);
		const Vec3V vToNearByPedsCenterOfMass = vNearbyPedsCenterOfMass - vPedPosition;
		const ScalarV scNearByPedsCenterOfMassDistanceSqr = MagSquared(vToNearByPedsCenterOfMass);
		if (IsGreaterThanAll(scFleePositionDistanceSqr, ScalarV(V_FLT_SMALL_3)) && IsGreaterThanAll(scNearByPedsCenterOfMassDistanceSqr, ScalarV(V_FLT_SMALL_3)))
		{
			const ScalarV scFleePositionDistance = Sqrt(scFleePositionDistanceSqr);
			const Vec3V vFleeDir = -vToFleePosition / scFleePositionDistance;

			// TODO[egarcia]: I am using now a heuristic to estimate the best crowd exit direction to solve some problems with the center of mass
			// when the nearby peds are aligned. I would be better anyway to calculate the "occluded" angles and look for the largest clear free one.
			//const ScalarV scNearByPedsCenterOfMassDistance = Sqrt(scNearByPedsCenterOfMassDistanceSqr);
			const Vec3V vCrowdExitDir = vClearDir;//-vToNearByPedsCenterOfMass / scNearByPedsCenterOfMassDistance;

			const ScalarV scMaxFleeDirAngleDelta = scSMALL_CROWD_MAX_FLEE_DIR_ANGLE_DELTA + scCrowdNormalizedSize * (scBIG_CROWD_MAX_FLEE_DIR_ANGLE_DELTA - scSMALL_CROWD_MAX_FLEE_DIR_ANGLE_DELTA);

			ScalarV scFleeDirAdjustmentAngleDelta = Clamp(AngleZ(vFleeDir, vCrowdExitDir), -scMaxFleeDirAngleDelta, scMaxFleeDirAngleDelta);
			Vec3V vFleeAdjustedDir = RotateAboutZAxis(vFleeDir, scFleeDirAdjustmentAngleDelta);
			
#if !__FINAL
			if (pvBeforeWallCheckFleeAdjustedDir)
			{
				*pvBeforeWallCheckFleeAdjustedDir = vFleeAdjustedDir;
			}
#endif 

			WorldProbe::CShapeTestSingleResult hProbeResultHandle;
			WorldProbe::CShapeTestProbeDesc ProbeData;
			ProbeData.SetResultsStructure(&hProbeResultHandle);

			ProbeData.SetStartAndEnd(VEC3V_TO_VECTOR3(vPedPosition), VEC3V_TO_VECTOR3(vPedPosition + vFleeAdjustedDir * ScalarV(5.0f)));
			ProbeData.SetContext(WorldProbe::LOS_GeneralAI);
			ProbeData.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
			ProbeData.SetIsDirected(true);
			WorldProbe::GetShapeTestManager()->SubmitTest(ProbeData, WorldProbe::PERFORM_SYNCHRONOUS_TEST);
			if (hProbeResultHandle.GetResultsReady() && (hProbeResultHandle.GetNumHits() > 0))
			{
				const WorldProbe::CShapeTestHitPoint& rResult = hProbeResultHandle[0];
#if !__FINAL
				if (pvWallCollisionPosition)
				{
					*pvWallCollisionPosition = VECTOR3_TO_VEC3V(rResult.GetHitPosition());
				}
				if (pvWallCollisionNormal)
				{
					*pvWallCollisionNormal = VECTOR3_TO_VEC3V(rResult.GetHitNormal());
				}
#endif // !__FINAL

				const Vec3V vFleeWallReflectedDir = vFleeAdjustedDir - (ScalarV(2.0f) * (Dot(vFleeAdjustedDir, VECTOR3_TO_VEC3V(rResult.GetHitNormal())) * VECTOR3_TO_VEC3V(rResult.GetHitNormal())));
				scFleeDirAdjustmentAngleDelta = Clamp(AngleZ(vFleeDir, vFleeWallReflectedDir), -scMaxFleeDirAngleDelta, scMaxFleeDirAngleDelta);
				vFleeAdjustedDir = RotateAboutZAxis(vFleeDir, scFleeDirAdjustmentAngleDelta);
			}

			vAdjustedFleePosition = vPedPosition - vFleeAdjustedDir * scFleePositionDistance;
		}
	}

	return vAdjustedFleePosition;
}


void CTaskSmartFlee::InitPreferPavements()
{
	if (!IsIgnorePreferPavementsTimeDisabled() && HasIgnorePreferPavementsTimeExpired())
	{
		CTaskMoveFollowNavMesh* pFollowNavMeshTask = FindFollowNavMeshTask();
		if (pFollowNavMeshTask && !pFollowNavMeshTask->GetKeepToPavements())
		{
			sm_PedsFleeingOutOfPavementManager.Add(*GetPed());
		}
	}
}

void CTaskSmartFlee::UpdatePreferPavements(CPed * pPed)
{
	taskAssert(pPed);

	if (!HasIgnorePreferPavementsTimeExpired())
	{
		m_fIgnorePreferPavementsTimeLeft = Max(0.0f, m_fIgnorePreferPavementsTimeLeft - fwTimer::GetTimeStep());
		if (HasIgnorePreferPavementsTimeExpired())
		{
			CTaskMoveFollowNavMesh* pFollowNavMeshTask = FindFollowNavMeshTask();
			if (pFollowNavMeshTask && !pFollowNavMeshTask->GetKeepToPavements())
			{
				sm_PedsFleeingOutOfPavementManager.Add(*pPed);
			}
		}
	}
}

void CTaskSmartFlee::ForceRepath(CPed* pPed)
{
	CTaskMoveFollowNavMesh* pFollowNavMeshTask = FindFollowNavMeshTask();
	if (pFollowNavMeshTask)
	{
		//Recalculate the target position.
		Vec3V vTargetPosition;
		m_FleeTarget.GetPosition(RC_VECTOR3(vTargetPosition));
		vTargetPosition = AdjustTargetPosByNearByPeds(*pPed, vTargetPosition, m_FleeTarget.GetEntity(), false);

		pFollowNavMeshTask->SetKeepMovingWhilstWaitingForPath(true); // Prevent them from stopping in the middle of the road while they recalculate the route. 
		pFollowNavMeshTask->SetKeepToPavements(ShouldPreferPavements(pPed));
		pFollowNavMeshTask->SetTarget(pPed, vTargetPosition, true);
	}
}

CTaskMoveFollowNavMesh* CTaskSmartFlee::FindFollowNavMeshTask()
{
	CTaskMoveFollowNavMesh* pFollowNavMeshTask = NULL;

	if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
	{
		CTaskComplexControlMovement* pComplexControlMovement = static_cast<CTaskComplexControlMovement*>(GetSubTask());
		aiTask* pTask = pComplexControlMovement->GetRunningMovementTask(GetPed());
		if (pTask && pTask->GetTaskType() == CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH)
		{
			pFollowNavMeshTask = SafeCast(CTaskMoveFollowNavMesh, pTask);
		}
	}

	return pFollowNavMeshTask;
}

bool CTaskSmartFlee::ShouldPreferPavements(const CPed* pPed) const
{
	if (!taskVerifyf(pPed, "CTaskSmartFlee::ShouldPreferPavements: No Task Ped!"))
	{
		return false;
	}

	const CPedIntelligence* pPedIntelligence = pPed->GetPedIntelligence();
	taskAssert(pPedIntelligence);

	return m_bForcePreferPavements || 
		(pPedIntelligence->GetFleeBehaviour().GetFleeFlags().IsFlagSet(CFleeBehaviour::BF_PreferPavements) && IsIgnorePreferPavementsTimeDisabled());
}


void CTaskSmartFlee::FleeOnFoot_OnUpdateClone()
{
	atHashWithStringNotFinal hAmbientClips = GetAmbientClipsToUse();
	if(!hAmbientClips.IsNull())
	{
		//Ambient clips exist on remote so apply to clone
		CTaskAmbientClips* pAmbientTask = static_cast<CTaskAmbientClips*>(FindSubTaskOfType(CTaskTypes::TASK_AMBIENT_CLIPS));
		if(	!pAmbientTask ||
			(pAmbientTask->GetConditionalAnimsGroup() && pAmbientTask->GetConditionalAnimsGroup()->GetHash()!=hAmbientClips.GetHash()) )
		{
			//Create the sub-task.
			CTask* pSubTask = CreateSubTaskForAmbientClips();
			SetNewTask(pSubTask);
		}
	}
}

CTask::FSM_Return CTaskSmartFlee::FleeOnFoot_OnUpdate(CPed* pPed)
{
	//Update the sub-task for flee on foot.
	UpdateSubTaskForFleeOnFoot();

	if (NetworkInterface::IsGameInProgress() && pPed->GetNetworkObject() && taskVerifyf(!pPed->IsNetworkClone(),"Only expect non clone here") )
	{
		NetworkInterface::ForceHighUpdateLevel(pPed);
	}

	PeriodicallyCheckForStateTransitions(10.0f);

	//Check if we have run out of time.
	if(m_fleeTimer.IsSet() && m_fleeTimer.IsOutOfTime())
	{
		//Either return to the start position, or finish the flee.
		if(pPed->WillReturnToPositionPostFlee(m_FleeFlags))
		{
			SetState(State_ReturnToStartPosition);
		}
		else
		{
			SetState(State_Finish);
		}
		
		return FSM_Continue;
	}

	if (pPed->GetTaskData().GetIsFlagSet(CTaskFlags::BlockIdleTurnsInSmartFleeWhileWaitingOnPath) && IsWaitingForPathInFleeOnFoot())
	{
		// Block any idle turning.
		pPed->SetPedResetFlag(CPED_RESET_FLAG_BlockQuadLocomotionIdleTurns, true);
	}

	pPed->SetPedResetFlag( CPED_RESET_FLAG_MoveBlend_bFleeTaskRunning, true );

	// Force probe for stairs
	if (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_OnStairs) || pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_OnStairSlope))
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_UseProbeSlopeStairsDetection, true);
	}

	CFacialDataComponent* pFacialData = pPed->GetFacialData();
	if(pFacialData)
	{
		pFacialData->RequestFacialIdleClip(CFacialDataComponent::FICT_Stressed);
	}

	//Check for cower on foot.
	CheckForCowerDuringFleeOnFoot();

	if (GetIsSubtaskFinished(CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT))
	{
		//Check if we should cower at the end of the route.
		if(m_uRunningFlags.IsFlagSet(RF_CowerAtEndOfRoute))
		{
			SetState(State_Cower);
		}
		else
		{
			m_iFleeDir = ComputeFleeDir(pPed);
			SetFlag(aiTaskFlags::RestartCurrentState);
		}

		return FSM_Continue;
	}

	if (AreWeOutOfRange(pPed, m_fStopDistance))
	{
		if (m_bQuitIfOutOfRange)
		{
			SetState(State_Finish);
			return FSM_Continue;
		}
		else if (!m_bHaveStopped)
		{
			if (fwRandom::GetRandomTrueFalse())
			{
				SetState(State_StandStill);
				return FSM_Continue;
			}
			else
			{
				m_bHaveStopped = true;
			}
		}
		else if (m_bHaveStopped)
		{
			m_bHaveStopped = false;
			SetState(State_StandStill);
			return FSM_Continue;
		}
	}

	if (BANK_ONLY(CFleeDebug::ms_bLookForCarsToSteal ||)
		pPed->GetPedIntelligence()->GetFleeBehaviour().GetFleeFlags().IsFlagSet(CFleeBehaviour::BF_CanUseVehicles) &&
		!pPed->GetTaskData().GetIsFlagSet(CTaskFlags::DisableVehicleUsage) &&
		!m_uExecConditions.IsFlagSet(EC_DontUseCars) )
	{
		if (LookForCarsToUse(pPed))
		{
			SetState(State_EnterVehicle);
			return FSM_Continue;
		}
	}

	//Check if we should cower.
	if(ShouldCower())
	{
		//Move to the cower state.
		SetState(State_Cower);
		return FSM_Continue;
	}

	CheckForNewPriorityEvent();

	if(m_screamTimer.IsSet() && m_screamTimer.IsOutOfTime())
	{
		TryToScream();
	}

	return FSM_Continue;
}

void CTaskSmartFlee::FleeOnFoot_OnExit()
{
	sm_PedsFleeingOutOfPavementManager.Remove(*GetPed());
}

bool CTaskSmartFlee::SpheresOfInfluenceBuilder(TInfluenceSphere* apSpheres, int& iNumSpheres, const CPed* pPed)
{
	//Ensure the task is valid.
	CTaskSmartFlee* pTask = static_cast<CTaskSmartFlee *>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_SMART_FLEE));
	if(!pTask)
	{
		return false;
	}

	return SpheresOfInfluenceBuilderForTarget(apSpheres, iNumSpheres, pPed, pTask->m_FleeTarget);
}

bool CTaskSmartFlee::SpheresOfInfluenceBuilderForTarget(TInfluenceSphere* apSpheres, int& iNumSpheres, const CPed* pPed, const CAITarget& rTarget)
{
	const int iInputMaxNumSpheres = MAX_NUM_INFLUENCE_SPHERES;
	const int iNumFleeSpheres = 2; // will this be enough?

	const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	const int iMaxNumSpheres = Min(iInputMaxNumSpheres, iNumFleeSpheres);

	//Check if the local player is threatening.
	const CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if(pPlayer && (pPlayer != rTarget.GetEntity()) && ShouldConsiderPedThreatening(*pPed, *pPlayer, true))
	{
		//Add an influence sphere.
		AddInfluenceSphere(*pPlayer, apSpheres, iNumSpheres, iMaxNumSpheres);
	}

	float fMaxDistanceClosePedSqr = 1.5f * 1.5f;
	int nClosePeds = 0;
	const int nMaxClosePeds = 4;
	Vector3 lPedPos[nMaxClosePeds];

	CEntityScannerIterator scanner = pPed->GetPedIntelligence()->GetNearbyPeds();
	for(int p=0; p<scanner.GetCount(); p++)
	{
		CPed * pNearbyPed = (CPed*)scanner.GetEntity(p);
		if(pNearbyPed && (pNearbyPed != pPlayer) && (pNearbyPed != rTarget.GetEntity()))
		{
			//Check if the nearby ped is considered a threat.
			if(ShouldConsiderPedThreatening(*pPed, *pNearbyPed, true))
			{
				//Add an influence sphere.
				AddInfluenceSphere(*pNearbyPed, apSpheres, iNumSpheres, iMaxNumSpheres);
			}
			else if (nClosePeds < nMaxClosePeds)
			{
				Vector3 vNearbyPedPos = VEC3V_TO_VECTOR3(pNearbyPed->GetTransform().GetPosition());
				float fNearbyPedDistSqr = (vNearbyPedPos - vPedPos).Mag2();
				if (fNearbyPedDistSqr < fMaxDistanceClosePedSqr && !pNearbyPed->IsInMotion())	// In motion might change but better than nothing
				{
					// For second pass, just making sure that we don't combine these or break the combination code
					vNearbyPedPos.z -= pPed->GetCapsuleInfo()->GetGroundToRootOffset();
					lPedPos[nClosePeds++] = vNearbyPedPos;
				}
			}
		}
	}

	// Second pass of influence spheres
	const int iMaxNumSpheresClosePeds = Min(iInputMaxNumSpheres, nMaxClosePeds + iNumFleeSpheres);
	const float fSphereRadiusClosePed = 0.5f;
	static dev_float fInnerWeightingClosePed = 0.5f*INFLUENCE_SPHERE_MULTIPLIER;
	static dev_float fOuterWeightingClosePed = 0.2f*INFLUENCE_SPHERE_MULTIPLIER;
	for(int p=0; p<nClosePeds; p++)
	{
		if (iNumSpheres >= iMaxNumSpheresClosePeds)
			break;

		apSpheres[iNumSpheres].SetOrigin(lPedPos[p]);
		apSpheres[iNumSpheres].SetInnerWeighting(fInnerWeightingClosePed);
		apSpheres[iNumSpheres].SetOuterWeighting(fOuterWeightingClosePed);
		apSpheres[iNumSpheres].SetRadius(fSphereRadiusClosePed);
		iNumSpheres++;
	}

	return true;
}

void CTaskSmartFlee::AddInfluenceSphere(const CPed& rPed, TInfluenceSphere* pSpheres, int& iNumSpheres, int iMaxNumSpheres)
{
	const float fSphereRadius = 10.0f;
	const float fJoinSphereExtra = 5.0f;
	const float fMaxCombineRadius = 40.0f;

	static dev_float fInnerWeighting = 1.0f*INFLUENCE_SPHERE_MULTIPLIER;
	static dev_float fOuterWeighting = 0.4f*INFLUENCE_SPHERE_MULTIPLIER;

	//Grab the ped position.
	Vector3 vPedPosition = VEC3V_TO_VECTOR3(rPed.GetTransform().GetPosition());

	// See if we can combine this threat with an existing influence sphere
	int s;
	for(s=0; s<iNumSpheres; s++)
	{
		Vector3 vDiff = pSpheres[s].GetOrigin() - vPedPosition;
		const float fCombined = pSpheres[s].GetRadius() + fSphereRadius;
		if( vDiff.Mag2() < fCombined*fCombined && vDiff.Mag2() < fMaxCombineRadius*fMaxCombineRadius)
		{
			pSpheres[s].SetOrigin( (pSpheres[s].GetOrigin() + vPedPosition) * 0.5f );
			pSpheres[s].SetRadius( vDiff.Mag() + fJoinSphereExtra );
			break;
		}
	}
	// Otherwise add a new influence sphere if we have space for it
	if(s == iNumSpheres && iNumSpheres < iMaxNumSpheres)
	{
		pSpheres[iNumSpheres].SetOrigin(vPedPosition);
		pSpheres[iNumSpheres].SetInnerWeighting(fInnerWeighting);
		pSpheres[iNumSpheres].SetOuterWeighting(fOuterWeighting);
		pSpheres[iNumSpheres].SetRadius(fSphereRadius);
		iNumSpheres++;
	}
}

void CTaskSmartFlee::FleeOnSkis_OnEnter(CPed* pPed)
{
	// Just set a movement task, don't bother trying to steal cars etc until we're safe and can take our skis off
	const CEntity* pFleeEntity = m_FleeTarget.GetEntity();
	if (pFleeEntity)
	{
		m_calcFleePosTimer.Set(fwTimer::GetTimeInMilliseconds(),m_iEntityPosCheckPeriod);
#if __ASSERT
		CheckForOriginFlee();
#endif // __ASSERT
	}

	// The state timer will now be used to detect a ped which hasn't really moved much on skis
	// and is probably stuck
	m_stateTimer.Set(fwTimer::GetTimeInMilliseconds(),5000);
	m_vOldPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

	Vector3 vFleePos;
	if (m_FleeTarget.GetPosition(vFleePos))
	{
		CTaskMoveFollowNavMesh* pMoveTask = rage_new CTaskMoveFollowNavMesh(
			m_fMoveBlendRatio,
			vFleePos,
			CTaskMoveFollowNavMesh::ms_fTargetRadius,
			CTaskMoveFollowNavMesh::ms_fSlowDownDistance,
			-1,		// time
			true,	// use blending
			true	// flee from target
			);

		// Add on a little extra, or we risk the task getting into a flee/stop loop (url:bugstar:300305)
		const dev_float fHysteresisDist = 1.0f;
		pMoveTask->SetFleeSafeDist(m_fStopDistance+fHysteresisDist);
		pMoveTask->SetUseLargerSearchExtents(m_bUseLargerSearchExtents);

		SetNewTask(rage_new CTaskComplexControlMovement(pMoveTask));
	}
}

CTask::FSM_Return CTaskSmartFlee::FleeOnSkis_OnUpdate(CPed* pPed)
{
	//PeriodicallyCheckForStateTransitions(pPed, 10.0f);

	// If it's a timed flee, return or finish if we're out of time
	if (m_fleeTimer.IsSet() && m_fleeTimer.IsOutOfTime())
	{
		SetState(State_TakeOffSkis);
		return FSM_Continue;
	}

	if (m_stateTimer.IsSet() && m_stateTimer.IsOutOfTime())
	{
		const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		Vector3 vDiff = m_vOldPedPos - vPedPosition;
		if (vDiff.Mag2() < 1.0f)
		{
			SetState(State_TakeOffSkis);
		}

		m_vOldPedPos = vPedPosition;
		m_stateTimer.Set(fwTimer::GetTimeInMilliseconds(),5000);
	}

	pPed->SetPedResetFlag( CPED_RESET_FLAG_MoveBlend_bFleeTaskRunning, true );

	CFacialDataComponent* pFacialData = pPed->GetFacialData();
	if(pFacialData)
	{
		pFacialData->RequestFacialIdleClip(CFacialDataComponent::FICT_Stressed);
	}

	if (GetIsSubtaskFinished(CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT))
	{
		m_iFleeDir = ComputeFleeDir(pPed);
		SetState(State_TakeOffSkis);
		return FSM_Continue;
	}

	if (AreWeOutOfRange(pPed, m_fStopDistance))
	{
		SetState(State_TakeOffSkis);
		return FSM_Continue;
	}

	return FSM_Continue;
}

void CTaskSmartFlee::SeekCover_OnEnter(CPed* pPed)
{
	const CEntity* pFleeEntity = m_FleeTarget.GetEntity();
	if (pFleeEntity && pFleeEntity->GetIsTypePed())
	{
		const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		if (VEC3V_TO_VECTOR3(pFleeEntity->GetTransform().GetPosition()).Dist(vPedPosition) > ms_fMinDistAwayFromThreatBeforeSeekingCover ) // Ensure we're at least 15m away
		{
			CTaskCover* pCoverTask = rage_new CTaskCover(CAITarget(pFleeEntity));
			pCoverTask->SetSearchFlags(CTaskCover::CF_QuitAfterCoverEntry|CTaskCover::CF_SearchInAnArcAwayFromTarget|CTaskCover::CF_CoverSearchAny|CTaskCover::CF_SeekCover);
			pCoverTask->SetSearchType(CCover::CS_MUST_PROVIDE_COVER);
			SetNewTask(pCoverTask);
		}
	}
}

CTask::FSM_Return CTaskSmartFlee::SeekCover_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	// TODO: If we've found cover maybe we don't use it if it's too close to the threat?
	if (GetIsSubtaskFinished(CTaskTypes::TASK_COVER))
	{
		// Comment this out for now as the seek cover never sets AtCoverPoint as the termination reason. Need to investigate
		/*if (GetSubTask() && GetSubTask()->GetSubTask() && GetSubTask()->GetSubTask()->GetTaskType() == CTaskTypes::TASK_SEEK_COVER)
		{
			if (static_cast<CTaskSeekCover*>(GetSubTask()->GetSubTask())->GetTerminationReason() == CTaskSeekCover::AtCoverPoint)
			{
				SetState(State_CrouchAtCover);
			}
			else
			{
				SetState(State_FleeOnFoot);
			}
		}
		else*/
		{
			SetState(State_FleeOnFoot);
		}
	}

	return FSM_Continue;
}

void CTaskSmartFlee::CrouchAtCover_OnEnter(CPed* pPed)
{
	SetNewTask(rage_new CTaskCrouch(fwRandom::GetRandomNumberInRange(2000, 5000)));

	const CEntity* pFleeEntity = m_FleeTarget.GetEntity();
	if (pFleeEntity)
	{
		// Look at the entity we're fleeing from if it exists (TODO: put this on a timer and only do it every so often
		if (!pPed->GetIkManager().IsLooking())
		{
			float fDistToEntity = DistSquared(pFleeEntity->GetTransform().GetPosition(), pPed->GetTransform().GetPosition()).Getf();

			if (fDistToEntity < pPed->GetPedIntelligence()->GetPedPerception().GetSeeingRange() && pFleeEntity->GetIsTypePed())
			{
				pPed->GetIkManager().LookAt(0, pFleeEntity, 1000, BONETAG_HEAD, NULL);
			}
		}
	}
}

CTask::FSM_Return CTaskSmartFlee::CrouchAtCover_OnUpdate(CPed* pPed)
{
	const CEntity* pFleeEntity = m_FleeTarget.GetEntity();

	if (pFleeEntity)
	{
		float fDistToEntity = DistSquared(pFleeEntity->GetTransform().GetPosition(), pPed->GetTransform().GetPosition()).Getf();

		if( GetIsSubtaskFinished(CTaskTypes::TASK_CROUCH) || fDistToEntity < pPed->GetPedIntelligence()->GetPedPerception().GetIdentificationRange())
		{
			SetState(State_Start);
		}
	}

	return FSM_Continue;
}

void CTaskSmartFlee::StandStill_OnEnter(CPed* UNUSED_PARAM(pPed))
{
	SetNewTask(rage_new CTaskComplexControlMovement(rage_new CTaskMoveStandStill(), NULL));

	m_standStillTimer.Set(fwTimer::GetTimeInMilliseconds(),fwRandom::GetRandomNumberInRange(3000,10000));
	m_bHaveStopped = true;
}

CTask::FSM_Return CTaskSmartFlee::StandStill_OnUpdate(CPed* pPed)
{
	// If it's a timed flee, return or finish if we're out of time
	if (m_fleeTimer.IsSet() && m_fleeTimer.IsOutOfTime())
	{
		PickNewStateTransition(State_Finish);
		return FSM_Continue;
	}
	else if (m_standStillTimer.IsSet() && m_standStillTimer.IsOutOfTime())
	{
		if (!AreWeOutOfRange(pPed,m_fStopDistance))
		{		
			if (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
			{
				SetState(State_Start);
			}
			else
			{
				SetState(State_FleeOnFoot);
			}
			return FSM_Continue;
		}
	}
	
	if (GetIsSubtaskFinished(CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT))
	{
		PickNewStateTransition(State_Finish);
		return FSM_Continue;
	}

	if( m_bQuitTaskIfOutOfRange )
	{
		SetState(State_Finish);
	}

	return FSM_Continue;
}


void CTaskSmartFlee::HandsUp_OnEnter(CPed* UNUSED_PARAM(pPed))
{
	
	CPed* pPed = GetPed();
	const CEntity* pFleeEntity = m_FleeTarget.GetEntity();

	if (pFleeEntity && pFleeEntity->GetIsTypePed())
	{
		// If reacting to a ped, send out a cry for help for cops to respond to.
		const CPed* pAggressorPed = static_cast<const CPed*>(pFleeEntity);

		// Make sure the victim exists and the assaulter isn't friends with them.
		if (!pAggressorPed->GetPedIntelligence()->IsFriendlyWith(*pPed))
		{
			CEventCrimeCryForHelp::CreateAndCommunicateEvent(pPed, pAggressorPed); 
		}
	}
	
	//Calculate the duration.
	int iMinTimeForHandsUp = (int)(sm_Tunables.m_MinTimeForHandsUp * 1000.0f);
	int iMaxTimeForHandsUp = (int)(sm_Tunables.m_MaxTimeForHandsUp * 1000.0f);
	int iTimeForHandsUp = fwRandom::GetRandomNumberInRange(iMinTimeForHandsUp, iMaxTimeForHandsUp);
	
	//Create the task.
	CTask* pTask = rage_new CTaskHandsUp(iTimeForHandsUp);
	
	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskSmartFlee::HandsUp_OnUpdate(CPed* pPed)
{
	//Check if the task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Do not put our hands up again.
		m_SuppressHandsUp = true;
		
		//Flee on foot.
		SetState(State_FleeOnFoot);
		return FSM_Continue;
	}
	
	const CEntity* pEntity = m_FleeTarget.GetEntity();

	if (pEntity)
	{
		const Vector3 vEntityPosition = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());
		pPed->SetDesiredHeading(vEntityPosition);
		
		//Allow the ped to turn and face the entity during the hands-up animation.
		{
			float fHeadingChange = SubtractAngleShorter(pPed->GetDesiredHeading(), pPed->GetCurrentHeading());
			if(Abs(fHeadingChange) > 0.05f)
			{
				fHeadingChange *= 0.1f;
			}
			pPed->GetMotionData()->SetExtraHeadingChangeThisFrame(fHeadingChange);
		}
	}
	
	if (CheckForNewPriorityEvent())
	{
		return FSM_Continue;
	}

	//Periodically check if the ped is no longer being aimed at.
	if(m_HandsUpTimer.Tick())
	{
		//Reset the timer.
		m_HandsUpTimer.Reset(sm_Tunables.m_TimeBetweenHandsUpChecks);
		
		//Check if the ped is no longer being aimed at.
		if(!CheckForNewAimedAtEvent())
		{
			//Move to the flee on foot state.
			SetState(State_FleeOnFoot);
			return FSM_Continue;
		}
	}

	return FSM_Continue;
}

// Make our way to the position which we have calculated
// that our wandering will recommence from
void CTaskSmartFlee::ReturnToStartPosition_OnEnter(CPed* UNUSED_PARAM(pPed))
{
	CTaskMoveFollowNavMesh* pMoveTask = rage_new CTaskMoveFollowNavMesh(MOVEBLENDRATIO_WALK,	// NB : what about peds on rollerskates, etc ?
		m_vStartPos,
		CTaskMoveFollowNavMesh::ms_fTargetRadius,
		CTaskMoveFollowNavMesh::ms_fSlowDownDistance,
		-1,		// time
		true,	// use blending
		false	// flee from target
		);

	if (m_bKeepMovingWhilstWaitingForFleePath)
	{	
		pMoveTask->SetKeepMovingWhilstWaitingForPath(true);
	}
	pMoveTask->SetUseLargerSearchExtents(m_bUseLargerSearchExtents);

	SetNewTask(rage_new CTaskComplexControlMovement(pMoveTask));
}

CTask::FSM_Return CTaskSmartFlee::ReturnToStartPosition_OnUpdate(CPed* pPed)
{
	if (GetIsSubtaskFinished(CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT))
	{
		// Extra checks for damage, car on it's side etc?
		if (pPed->GetPedIntelligence()->GetLastVehiclePedInside()) // && the vehicle is within seeing range try to enter it
		{
			if (Dist(pPed->GetPedIntelligence()->GetLastVehiclePedInside()->GetTransform().GetPosition(), pPed->GetTransform().GetPosition()).Getf() < pPed->GetPedIntelligence()->GetPedPerception().GetSeeingRange())
			{
				SetState(State_EnterVehicle);
			}
		}
		else
		{
			SetState(State_Finish);
		}
	}
	return FSM_Continue;
}

void CTaskSmartFlee::EnterVehicle_OnEnter(CPed* UNUSED_PARAM(pPed))
{
	// Return to the vehicle
	SetNewTask(rage_new CTaskEnterVehicle(GetPed()->GetPedIntelligence()->GetLastVehiclePedInside(),SR_Prefer, 0));
}

CTask::FSM_Return CTaskSmartFlee::EnterVehicle_OnUpdate(CPed* pPed)
{
	// Check for a failed route.
	CTaskMoveFollowNavMesh* pNavTask = static_cast<CTaskMoveFollowNavMesh*>(pPed->GetPedIntelligence()->FindMovementTaskByType(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH));
	if (pNavTask)
	{
		bool bFailed = pNavTask->IsUnableToFindRoute();
		if (bFailed)
		{
			// Transition to flee on foot.  Maybe we'll get away in that state.  If not, it will cause the ped to cower.
			SetState(State_FleeOnFoot);
		}
	}

	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished)) 
	{
		// Check we are in a vehicle (we might not be if someone else got in the car before us)
		if (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
		{
			SetState(State_FleeInVehicle);
		}
		else
		{
			SetState(State_FleeOnFoot);
		}
	}
	return FSM_Continue;
}

void CTaskSmartFlee::EnterVehicle_OnEnterClone(CPed* pPed)
{
	CTask *pEnterVehicleTask = pPed->GetPedIntelligence()->CreateCloneTaskForTaskType(CTaskTypes::TASK_ENTER_VEHICLE);

	if(pEnterVehicleTask)
	{
		SetNewTask(pEnterVehicleTask);
	}
}

CTask::FSM_Return CTaskSmartFlee::EnterVehicle_OnUpdateClone(CPed* UNUSED_PARAM(pPed))
{
	if(GetIsSubtaskFinished(CTaskTypes::TASK_ENTER_VEHICLE) && GetStateFromNetwork() != State_EnterVehicle)
	{
		SetState(GetStateFromNetwork());
	}
	return FSM_Continue;
}

void CTaskSmartFlee::FleeInVehicle_OnEnter(CPed* pPed)
{
	//Try to scream.
	TryToScream();

	const CEntity* pFleeEntity = m_FleeTarget.GetEntity();
	taskFatalAssertf(!pFleeEntity || pFleeEntity->GetIsPhysical(), "Flee entity must be physical if valid!");

	// Maybe I'm already in a vehicle
	if (!pPed->GetPedIntelligence()->GetLastVehiclePedInside())
	{
		pPed->GetPedIntelligence()->SetLastVehiclePedInside(pPed->GetMyVehicle());
	}

	CVehicle *pVehicle = pPed->GetPedIntelligence()->GetLastVehiclePedInside();
	if (pVehicle)
	{
		if(pVehicle->IsDriver(pPed))
		{
			sVehicleMissionParams params;
			if (pFleeEntity)
			{
				params.SetTargetEntity(const_cast<CEntity*>(pFleeEntity));
				params.SetTargetObstructionSizeModifier(sm_Tunables.m_TargetObstructionSizeModifier);
			}
			else
			{
				params.SetTargetPosition(m_FleeTarget);
			}
		
			params.m_iDrivingFlags = DMode_AvoidCars|DF_AvoidTargetCoors|DF_SteerAroundPeds;
			if( IsFlagSet(AllowVehicleOffRoad) )
			{
				params.m_iDrivingFlags = params.m_iDrivingFlags | DF_GoOffRoadWhenAvoiding;
			}

			if (pVehicle->GetIntelligence()->IsOnHighway())
			{
				taskAssertf((params.m_iDrivingFlags & DF_DriveIntoOncomingTraffic) == 0x0, "DF_DriveIntoOncomingTraffic shoult not be set on highways to avoid traffic jams.");
				params.m_iDrivingFlags = params.m_iDrivingFlags | DF_AvoidTurns;
			}

			if(pVehicle->InheritsFromBicycle())
			{
				params.m_fCruiseSpeed = 20.0f;
			}
			else
			{
				CVehicleModelInfo* pVehicleInfo = pVehicle->GetVehicleModelInfo();
				bool bIsNotSportsOrNotNetwork = !NetworkInterface::IsGameInProgress() || !pVehicleInfo || pVehicleInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_SPORTS);
				params.m_fCruiseSpeed = bIsNotSportsOrNotNetwork ? 40.0f : 35.0f;
			}
		
			aiTask *pTaskVehicleFlee = NULL;
			if ( pVehicle->InheritsFromHeli() || pVehicle->InheritsFromPlane() )
			{
				params.m_fCruiseSpeed = Max(params.m_fCruiseSpeed, pVehicle->GetVelocity().Mag() + 10.0f);
				params.m_fCruiseSpeed = Max(params.m_fCruiseSpeed, pVehicle->pHandling->m_fEstimatedMaxFlatVel * 0.9f);
				// planes and heli's should flee airborne
				if ( pFleeEntity && pFleeEntity->GetIsPhysical() )
				{				
					// try to flee faster than pursuing entity
					params.m_fCruiseSpeed = Max(params.m_fCruiseSpeed, static_cast<const CPhysical*>(pFleeEntity)->GetVelocity().Mag() + 10.0f);
				}
					
				int iFlightHeight = CTaskVehicleFleeAirborne::ComputeFlightHeightAboveTerrain(*pVehicle, 100);
				pTaskVehicleFlee = rage_new CTaskVehicleFleeAirborne(params, iFlightHeight, iFlightHeight);
			}
			else if ( pVehicle->InheritsFromBoat() )
			{
				pTaskVehicleFlee = rage_new CTaskVehicleFleeBoat(params);
			}

			if ( !pTaskVehicleFlee )
			{
				if(!AllowedToFleeOnRoads(*pVehicle))
				{
					// This isn't good, this vehicle is not meant to be fleeing on regular roads,
					// but it is. We could possibly try some other fallback task (or no task?), but
					// it may be safer to just mark it to get cleaned up gracefully.
					if(pVehicle->PopTypeIsRandomNonPermanent())
					{
						pVehicle->m_nVehicleFlags.bTryToRemoveAggressively = true;
						pVehicle->m_nVehicleFlags.bIsInCluster = false;
					}
				}

				pTaskVehicleFlee = rage_new CTaskVehicleFlee(params, m_uConfigFlagsForVehicle);		
			}

			//Create the sub-task.
			CTask* pSubTask = rage_new CTaskInVehicleBasic(pVehicle, false, NULL, CTaskInVehicleBasic::IVF_DisableAmbients);

			//Create the task.
			CTask* pTask = rage_new CTaskControlVehicle(pVehicle, pTaskVehicleFlee, pSubTask);
			
			//Start the task.
			SetNewTask(pTask);

			// Init vehicle stuck check timer
			ResetExitVehicleDueToVehicleStuckTimer();

			// Init driving parallel to target timer
			ResetDrivingParallelToTargetTime();
		}
		else
		{
			//Create the task.
			CTask* pTask = rage_new CTaskInVehicleBasic(pVehicle, false, NULL, CTaskInVehicleBasic::IVF_DisableAmbients);

			//Start the task.
			SetNewTask(pTask);
		}
	}
}

void CTaskSmartFlee::AddExitVehicleDueToRoutePassByThreatPositionRequest()
{
	m_ExitCarIfRoutePassByThreatPositionRequest.bPending = true;
}

bool CTaskSmartFlee::ProcessExitVehicleDueToRoutePassByThreatRequest()
{
	//Check if we can exit the vehicle, and we should exit the vehicle.
	bool bShouldExitVehicle = false;
	if (m_ExitCarIfRoutePassByThreatPositionRequest.bPending)
	{
		const CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
		// We wait until we have a route for the vehicle to attend the request
		if(pVehicle && pVehicle->GetIntelligence()->GetNodeList())
		{
			if (CanExitVehicle())
			{
				Vector3 vTargetPosition;
				if (m_FleeTarget.GetPosition(vTargetPosition))
				{
					bShouldExitVehicle = pVehicle->GetIntelligence()->DoesRoutePassByPosition(VECTOR3_TO_VEC3V(vTargetPosition), sm_Tunables.m_RoutePassNearTargetThreshold, sm_Tunables.m_RoutePassNearTargetCareThreshold);
				}
			}

			m_ExitCarIfRoutePassByThreatPositionRequest.bPending = false;
		}
	}

	return bShouldExitVehicle;
}

CTask::FSM_Return CTaskSmartFlee::FleeInVehicle_OnUpdate(CPed* pPed)
{
	const bool bCanExitVehicle = CanExitVehicle();

	// NOTE[egarcia]: Request must be processed even if we cannot exit vehicle right now. We do not want to store it for the future.
	bool bShouldExitVehicleDueToRoutePassByThreatPositionRequest = ProcessExitVehicleDueToRoutePassByThreatRequest();
	//
	bool bCheckForExitVehicleDueToVehicleStuck = bCanExitVehicle && UpdateExitVehicleDueToVehicleStuck(); 
	
#if __BANK
	TUNE_GROUP_BOOL(TASK_SMART_FLEE, bForceExitVehicle, false);
	if(bForceExitVehicle)
	{
		SetStateForExitVehicle();
		return FSM_Continue;
	}

	bool bDoesVehicleHaveNoDriver	= false; 
	bool bShouldExitVehicleDueToAttributes = false;
	bool bCheckForExitVehicleDueToRoute = false;
	bool bIsVehicleStuck = false;
	bool bIsCarUndriveable = false;

	if (bCanExitVehicle)
	{
		bDoesVehicleHaveNoDriver = DoesVehicleHaveNoDriver();
		bShouldExitVehicleDueToAttributes = ShouldExitVehicleDueToAttributes();
		bCheckForExitVehicleDueToRoute = CheckForExitVehicleDueToRoute();
		bIsVehicleStuck = IsVehicleStuck();
		bIsCarUndriveable = IsCarUndriveable();
	}
#endif

	//Update time driving parallel to targets
	UpdateDrivingParallelToTargetTime();

	//Update the secondary task.
	UpdateSecondaryTaskForFleeInVehicle();

	//prevent vehicles stopping for accidents when fired at
	if(m_bNewPriorityEvent)
	{	
		//Check the event.
		switch(m_priorityEvent)
		{
		case EVENT_GUN_AIMED_AT:
		case EVENT_SHOT_FIRED:
		case EVENT_SHOT_FIRED_BULLET_IMPACT:
		case EVENT_SHOT_FIRED_WHIZZED_BY:
		case EVENT_VEHICLE_DAMAGE_WEAPON:
			{
			CTaskControlVehicle* pSubTask = static_cast<CTaskControlVehicle*>(GetSubTask());
			if(pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_CONTROL_VEHICLE)
			{
				CTask* pFleeTask = static_cast<CTask*>(pSubTask->GetVehicleTask());
				if(pFleeTask && pFleeTask->GetTaskType()==CTaskTypes::TASK_VEHICLE_FLEE)
				{
					CTaskVehicleFlee* pVehicleFleeTask = static_cast<CTaskVehicleFlee*>(pFleeTask);
					pVehicleFleeTask->PreventStopAndWait();
				}
			}
			break;
			}
		default: break;
		}
	}

	//Check if the sub-task has finished.
	if(pPed->GetIsDrivingVehicle() && GetIsSubtaskFinished(CTaskTypes::TASK_CONTROL_VEHICLE))
	{
		//We are done fleeing, finish the task.
		SetState(State_Finish);
	}
	//Check for a new priority event.
	else if( CheckForNewPriorityEvent() )
	{
		//Nothing else to do.
	}
#if __BANK
	else if(bCanExitVehicle &&
		(bShouldExitVehicleDueToRoutePassByThreatPositionRequest || bDoesVehicleHaveNoDriver || bShouldExitVehicleDueToAttributes || bCheckForExitVehicleDueToRoute || bCheckForExitVehicleDueToVehicleStuck || bIsVehicleStuck || bIsCarUndriveable))
	{
		CAILogManager::GetLog().Log("SmartFlee -> Exit Vehicle: %s Entity %s (0x%p), CTaskSmartFlee::FleeInVehicle_OnUpdate, Tests: DoesVehicleHaveNoDriver: %d, ShouldExitVehicleDueToAttributes: %d, CheckForExitVehicleDueToRoute: %d, CheckForExitVehicleDueToVehicleStuck: %d, IsVehicleStuck: %d, IsCarUndriveable: %d, bShouldExitVehicleDueToRoutePassByThreatPositionRequest: %d\n", 
			AILogging::GetDynamicEntityIsCloneStringSafe(pPed), AILogging::GetDynamicEntityNameSafe(pPed), pPed,
			bDoesVehicleHaveNoDriver,
			bShouldExitVehicleDueToAttributes,
			bCheckForExitVehicleDueToRoute,
			bCheckForExitVehicleDueToVehicleStuck,
			bIsVehicleStuck,
			bIsCarUndriveable,
			bShouldExitVehicleDueToRoutePassByThreatPositionRequest
			);
#else
	else if(bCanExitVehicle &&
		(bShouldExitVehicleDueToRoutePassByThreatPositionRequest || bCheckForExitVehicleDueToVehicleStuck || DoesVehicleHaveNoDriver() || ShouldExitVehicleDueToAttributes() || CheckForExitVehicleDueToRoute() || IsVehicleStuck() || IsCarUndriveable()))
	{
#endif // __BANK

		//Set the state for exit vehicle.
		SetStateForExitVehicle();
	}
	//Check if our car is undriveable.
	else if(IsCarUndriveable())
	{
		//Cower in the vehicle.
		SetState(State_CowerInVehicle);
	}
	else if (ShouldLaunchEmergencyStop())
	{
		SetState(State_EmergencyStopInVehicle);
	}
	else if(NetworkInterface::IsGameInProgress())
	{
		const CVehicle* pVehicle = pPed->GetVehiclePedInside();
		if(pVehicle && pVehicle->GetIsLandVehicle() && !pPed->PopTypeIsMission())
		{
			UpdateFleeInVehicleIntensity();

			if(m_fFleeInVehicleIntensity <= 0.0f)
			{
				SetState(State_Finish);
			}
		}
	}

	//! if we have lost our vehicle (if we have been warped out, then restart flee task).
	const CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
	if(!pVehicle)
	{
		SetState(State_Start);
	}

	return FSM_Continue;
}

//online, there's a good change the car won't be despawned as will be migrated to another player
//so gradually reduce flee intensity so car resumes driving normally after a while
void CTaskSmartFlee::UpdateFleeInVehicleIntensity()
{
	CTaskControlVehicle* pSubTask = static_cast<CTaskControlVehicle*>(GetSubTask());
	if(pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_CONTROL_VEHICLE)
	{
		CTaskVehicleFlee* pFleeTask = static_cast<CTaskVehicleFlee*>(pSubTask->GetVehicleTask());
		if(pFleeTask)
		{
			static float s_fMinFleeTimer = 5.0f;
			static float s_fMaxFleeTimer = 20.0f;
			static float s_fMinFleeIntensityDist = 100.0f;
			static float s_fMaxFleeIntensityDist = 250.0f;
			static float s_fMaxReducedTimeIntensity = 0.5f;

			Vector3 vFleePos;
			m_FleeTarget.GetPosition(vFleePos);
			float fDist = (VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition()) - vFleePos).Mag();

			float fMinIntensity = 0.0f;
			float fMaxIntensity = 1.0f;

			if(fDist > s_fMinFleeIntensityDist)
			{
				m_fFleeInVehicleTime += GetTimeStep();
				fMaxIntensity = RampValue(m_fFleeInVehicleTime, s_fMinFleeTimer, s_fMaxFleeTimer, fMaxIntensity, s_fMaxReducedTimeIntensity);
			}
			else
			{
				m_fFleeInVehicleTime = 0.0f;
			}

			m_fFleeInVehicleIntensity = RampValue(fDist, s_fMinFleeIntensityDist, s_fMaxFleeIntensityDist, fMaxIntensity, fMinIntensity);
			pFleeTask->SetFleeIntensity(m_fFleeInVehicleIntensity);
		}
	}
}

void CTaskSmartFlee::FleeInVehicle_OnExit()
{
	//Clear the flags.
	m_uConfigFlagsForVehicle.ClearFlag(CTaskVehicleFlee::CF_Swerve);
	m_uConfigFlagsForVehicle.ClearFlag(CTaskVehicleFlee::CF_Hesitate);
}

void CTaskSmartFlee::ReverseInVehicle_OnEnter()
{
	//Grab the ped.
	CPed* pPed = GetPed();

	//Ensure the ped is driving a vehicle.
	if(!taskVerifyf(pPed->GetIsDrivingVehicle(), "The ped is not driving a vehicle."))
	{
		return;
	}

	//B*7306390 - We might be in the process of taking ownership of the vehicle just return
	if(pPed->GetMyVehicle()->IsNetworkClone())
	{
		return;
	}

	//Try to scream.
	TryToScream();

	//Note that we have reversed in our vehicle.
	m_uRunningFlags.SetFlag(RF_HasReversedInVehicle);

	//Create the vehicle task for stop.
	CTask* pVehicleTaskForStop = rage_new CTaskVehicleStop(CTaskVehicleStop::SF_UseFullBrake);

	//Create the task for stop.
	CTask* pTaskForStop = rage_new CTaskControlVehicle(GetPed()->GetVehiclePedInside(), pVehicleTaskForStop);

	//Create the task for reverse.
	int iTimeToReverse = 0;
	
	// If we have cars behind or the target is inoffensive, we perform a small reverse and resume the fleeing behaviour
	if (GetPed()->GetVehiclePedInside()->GetIntelligence()->m_NumCarsBehindUs > 0)
	{
		iTimeToReverse = static_cast<int>(sm_Tunables.m_TimeToReverseCarsBehind * 1000.0f);
	}
	else if (IsInoffensiveTarget(m_FleeTarget.GetEntity()))
	{
		iTimeToReverse = static_cast<int>(sm_Tunables.m_TimeToReverseInoffensiveTarget * 1000.0f);
	}
	else
	{
		const s32 iMinTimeToReverse = static_cast<int>(sm_Tunables.m_TimeToReverseDefaultMin * 1000.0f);
		const s32 iMaxTimeToReverse = static_cast<int>(sm_Tunables.m_TimeToReverseDefaultMax * 1000.0f);
		iTimeToReverse = fwRandom::GetRandomNumberInRange(iMinTimeToReverse, iMaxTimeToReverse);
	}

	CTaskCarSetTempAction* pTaskForReverse = rage_new CTaskCarSetTempAction(pPed->GetMyVehicle(), (s8)TEMPACT_REVERSE_STRAIGHT_HARD, iTimeToReverse);
	pTaskForReverse->GetConfigFlagsForAction().SetFlag(CTaskVehicleReverse::CF_QuitIfVehicleCollisionDetected);
	pTaskForReverse->GetConfigFlagsForAction().SetFlag(CTaskVehicleReverse::CF_RandomizeThrottle);
	pTaskForReverse->GetConfigFlagsForInVehicleBasic().SetFlag(CTaskInVehicleBasic::IVF_DisableAmbients);

	//Create the task.
	CTaskSequenceList* pSequenceList = rage_new CTaskSequenceList();
	pSequenceList->AddTask(pTaskForStop);
	pSequenceList->AddTask(pTaskForReverse);
	CTask* pTask = rage_new CTaskUseSequence(*pSequenceList);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskSmartFlee::ReverseInVehicle_OnUpdate()
{
	//Check if the sub-task has finished.
	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		if (CanExitVehicleToFleeFromTarget(m_FleeTarget.GetEntity()))
		{
			AddExitVehicleDueToRoutePassByThreatPositionRequest();
		}

		SetState(State_FleeInVehicle);

		////Flee in the vehicle.
		//SetState(State_Start);
	}

	return FSM_Continue;
}

bool CTaskSmartFlee::ShouldLaunchEmergencyStop() const
{
	//Grab the ped.
	const CPed* pPed = GetPed();
	taskAssert(pPed);

	//Ensure the ped is inside of a vehicle.
	const CVehicle* pVehicle = pPed->GetVehiclePedInside();
	if(!pVehicle)
	{
		return false;
	}

	//Ensure the ped is the driver.
	if(!pVehicle->IsDriver(pPed))
	{
		return false;
	}

	// Check if enough time has passed since the last emergency stop
	if(m_BetweenEmergencyStopsTimer.IsSet() && !m_BetweenEmergencyStopsTimer.IsOutOfTime())
	{
		return false;
	}

	// Prevent crashing against behind cars
	if (pVehicle->GetIntelligence()->m_NumCarsBehindUs > 0)
	{
		return false;
	}

	// Check we actually are moving at some minimum speed 
	const Vec3V vVehicleVelocity = VECTOR3_TO_VEC3V(pVehicle->GetVelocity());
	const ScalarV scVehicleSpeedSquared = MagXYSquared(vVehicleVelocity);
	if(IsLessThanAll(scVehicleSpeedSquared, ScalarV(sm_Tunables.m_EmergencyStopMinSpeedToUseDefault * sm_Tunables.m_EmergencyStopMinSpeedToUseDefault)))
	{
		return false;
	}

	// If we have been driving parallel to any target for enough time, make an emergency stop
	if (m_fDrivingParallelToTargetTime > sm_Tunables.m_EmergencyStopTimeToUseIfIsDrivingParallelToTarget)
	{
		return true;
	}

	if (ShouldLaunchEmergencyStopDueToRoute(*pVehicle))
	{
		return true;
	}

	return false;

}

bool CTaskSmartFlee::ShouldLaunchEmergencyStopDueToRoute(const CVehicle& rVehicle) const
{
	// Check that we are not moving too fast
	const CVehicleIntelligence* pVehicleIntelligence = rVehicle.GetIntelligence();
	taskAssert(pVehicleIntelligence);
	const ScalarV scMaxSpeedToStop = pVehicleIntelligence->IsOnHighway() ? ScalarV(sm_Tunables.m_EmergencyStopMaxSpeedToUseOnHighWays) : ScalarV(sm_Tunables.m_EmergencyStopMaxSpeedToUseDefault);
	if(IsGreaterThanAll(MagXYSquared(VECTOR3_TO_VEC3V(rVehicle.GetVelocity())), scMaxSpeedToStop * scMaxSpeedToStop))
	{
		return false;
	}

	//Check if the target is valid.
	SVehicleRouteInterceptionParams targetVehicleRouteInterceptionParams;
	if(GetFleeTargetEmergencyStopRouteInterceptionParams(&targetVehicleRouteInterceptionParams))
	{
		//Check if we should exit due to the route and target.
		if(ShouldLaunchEmergencyStopDueToRouteAndTarget(rVehicle, targetVehicleRouteInterceptionParams))
		{
			return true;
		}
	}

	//Check if this is a MP game.
	if(NetworkInterface::IsGameInProgress())
	{
		//Iterate over the players.
		unsigned                 numPhysicalPlayers = netInterface::GetNumPhysicalPlayers();
		const netPlayer * const *allPhysicalPlayers = netInterface::GetAllPhysicalPlayers();

		for(unsigned index = 0; index < numPhysicalPlayers; index++)
		{
			const CNetGamePlayer *pPlayer = SafeCast(const CNetGamePlayer, allPhysicalPlayers[index]);

			//Ensure the ped is valid.
			CPed* pPlayerPed = pPlayer->GetPlayerPed();
			if(!pPlayerPed || m_FleeTarget.GetEntity() == pPlayerPed)
			{
				continue;
			}

			//Check if we should exit due to the route and target.
			SVehicleRouteInterceptionParams playerVehicleRouteInterceptionParams;
			const bool bParamsOk = GetTargetEmergencyStopRouteInterceptionParams(*pPlayerPed, &playerVehicleRouteInterceptionParams);
			if(bParamsOk && ShouldLaunchEmergencyStopDueToRouteAndTarget(rVehicle, playerVehicleRouteInterceptionParams))
			{
				return true;
			}
		}
	}

	return false;
}

bool CTaskSmartFlee::ShouldLaunchEmergencyStopDueToRouteAndTarget(const CVehicle& rVehicle, const SVehicleRouteInterceptionParams& rTargetVehicleInterceptionParams) const
{
	// Check if enough time has passed since the last check
	if(m_EmergencyStopRouteInterceptionCheckCooldownTimer.IsSet() && !m_EmergencyStopRouteInterceptionCheckCooldownTimer.IsOutOfTime())
	{
		return false;
	}

	// Grab vehicle info
	const Vec3V vVehiclePosition = rVehicle.GetTransform().GetPosition();
	const Vec3V vVehicleForward  = rVehicle.GetTransform().GetForward();
	const Vec3V vVehicleVelocity = VECTOR3_TO_VEC3V(rVehicle.GetVelocity());

	// Calculate target info
	const Vec3V vTargetRelPosition = rTargetVehicleInterceptionParams.vPosition - vVehiclePosition;
	const ScalarV scTargetDistanceSqr = MagXYSquared(vTargetRelPosition);
	if (!IsGreaterThanAll(scTargetDistanceSqr, ScalarV(V_ZERO)))
	{
		return false;
	}

	//Ensure the vehicle is facing the target.
	const ScalarV scTargetDistance = Sqrt(scTargetDistanceSqr);
	const Vec3V vVehicleToTargetDirection = vTargetRelPosition  / scTargetDistance;
	const ScalarV scFacingDot = Dot(vVehicleForward, vVehicleToTargetDirection);
	// TODO[egarcia]: Move params to tunables?
	static const ScalarV scMIN_FACING_DOT(0.0f);
	if (IsLessThanAll(scFacingDot, scMIN_FACING_DOT))
	{
		return false;
	}

	// Check if we are close enough considering our relative velocity
	const Vec3V vTargetRelVelocity = rTargetVehicleInterceptionParams.vVelocity - vVehicleVelocity;
	const ScalarV scTargetRelSpeed = MagXY(vTargetRelVelocity);
	const ScalarV scTimeToCollision = scTargetDistance / scTargetRelSpeed;
	if (IsGreaterThanAll(scTimeToCollision, ScalarV(rTargetVehicleInterceptionParams.fMaxTime)))
	{
		return false;
	}
		
	// Check we are not moving in parallel with the target
	const Vec3V vTargetVelocityDir = NormalizeFastSafe(rTargetVehicleInterceptionParams.vVelocity, rTargetVehicleInterceptionParams.vForward);
	const ScalarV scAlignmentDot = Dot(vVehicleForward, vTargetVelocityDir);
	// TODO[egarcia]: Move params to tunables?
	static const ScalarV scMAX_ALIGNMENT_DOT(0.866f);
	if(IsGreaterThanAll(scAlignmentDot, scMAX_ALIGNMENT_DOT))
	{
		return false;
	}

	// Reset the timer
	m_EmergencyStopRouteInterceptionCheckCooldownTimer.Set(fwTimer::GetTimeInMilliseconds(), static_cast<int>(sm_Tunables.m_EmergencyStopTimeBetweenChecks * 1000.0f));

	// Check route interception
	return rVehicle.GetIntelligence()->CheckRouteInterception(
		rTargetVehicleInterceptionParams.vPosition, 
		rTargetVehicleInterceptionParams.vVelocity, 
		rTargetVehicleInterceptionParams.fMaxSideDistance, 
		rTargetVehicleInterceptionParams.fMaxForwardDistance, 
		rTargetVehicleInterceptionParams.fMinTime, 
		rTargetVehicleInterceptionParams.fMaxTime);
}

bool CTaskSmartFlee::GetFleeTargetEmergencyStopRouteInterceptionParams(SVehicleRouteInterceptionParams* pParams) const
{
	taskAssert(pParams);

	bool bSuccess = false;

	if (m_FleeTarget.GetIsValid())
	{
		const CEntity* pFleeTargetEntity = m_FleeTarget.GetEntity();
		if (pFleeTargetEntity)
		{
			bSuccess = GetTargetEmergencyStopRouteInterceptionParams(*pFleeTargetEntity, pParams);
		}
	}

	return bSuccess;
}

bool CTaskSmartFlee::GetTargetEmergencyStopRouteInterceptionParams(const CEntity& rTargetEntity, SVehicleRouteInterceptionParams* pParams) const
{
	taskAssert(pParams);
	
	bool bParamsOk = false;

	const CPed* pTargetPed = rTargetEntity.GetIsTypePed() ? static_cast<const CPed*>(&rTargetEntity) : NULL;

	const CVehicle* pTargetVehicle = NULL;
	if (pTargetPed)
	{
		pTargetVehicle = pTargetPed->GetVehiclePedInside();
	}
	else if (rTargetEntity.GetIsTypeVehicle())
	{
		pTargetVehicle = SafeCast(const CVehicle, &rTargetEntity);
	}

	if (pTargetVehicle)
	{
		// Target vehicle
		pParams->vPosition		     = pTargetVehicle->GetTransform().GetPosition();
		pParams->vForward			 = pTargetVehicle->GetTransform().GetForward();
		pParams->vVelocity		     = VECTOR3_TO_VEC3V(pTargetVehicle->GetVelocity());
		pParams->fMaxForwardDistance = sm_Tunables.m_EmergencyStopInterceptionMaxForwardDistance;
		pParams->fMaxSideDistance    = sm_Tunables.m_EmergencyStopInterceptionMaxSideDistance;
		pParams->fMinTime			 = sm_Tunables.m_EmergencyStopInterceptionMinTime;	
		pParams->fMaxTime			 = sm_Tunables.m_EmergencyStopInterceptionMaxTime;	
		bParamsOk = true;
	}
	// We are not launching emergency stop for targets on foot

	return bParamsOk;
}

s32 CTaskSmartFlee::GetPauseAfterLaunchEmergencyStopDuration() const
{
	static const int iMIN_PAUSE_AFTER_EMERGENCY_STOP = 1500;
	static const int iMAX_PAUSE_AFTER_EMERGENCY_STOP = 2500;
	return fwRandom::GetRandomNumberInRange(iMIN_PAUSE_AFTER_EMERGENCY_STOP, iMAX_PAUSE_AFTER_EMERGENCY_STOP);
}

void CTaskSmartFlee::EmergencyStopInVehicle_OnEnter()
{
	//Grab the ped.
	CPed* pPed = GetPed();

	// Reset the between stops
	m_BetweenEmergencyStopsTimer.Set(fwTimer::GetTimeInMilliseconds(), static_cast<int>(sm_Tunables.m_EmergencyStopMinTimeBetweenStops * 1000.0f));

	//Ensure the ped is driving a vehicle.
	if(!taskVerifyf(pPed->GetIsDrivingVehicle(), "The ped is not driving a vehicle."))
	{
		return;
	}

	//Try to scream.
	TryToScream();

	//Create the vehicle task for stop.
	CTask* pVehicleTaskForStop = rage_new CTaskVehicleStop(CTaskVehicleStop::SF_UseFullBrake);

	//Create the task for stop.
	CTask* pTaskForStop = rage_new CTaskControlVehicle(GetPed()->GetVehiclePedInside(), pVehicleTaskForStop);

	//Start the task.
	SetNewTask(pTaskForStop);
}

CTask::FSM_Return CTaskSmartFlee::EmergencyStopInVehicle_OnUpdate()
{
	// Check if we have run out of time for what the user requested.
	if(m_AfterEmergencyStopTimer.IsSet())
	{
		if (m_AfterEmergencyStopTimer.IsOutOfTime())
		{
			m_AfterEmergencyStopTimer.Unset();

			if (CanReverseInVehicle())
			{
				SetState(State_ReverseInVehicle);
			}
			else
			{
				if (CanExitVehicleToFleeFromTarget(m_FleeTarget.GetEntity()))
				{
					AddExitVehicleDueToRoutePassByThreatPositionRequest();
				}

				SetState(State_FleeInVehicle);
			}
		}
	}
	//Check if the sub-task has finished.
	else if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Reset the timer
		m_AfterEmergencyStopTimer.Set(fwTimer::GetTimeInMilliseconds(), GetPauseAfterLaunchEmergencyStopDuration());
	}

	return FSM_Continue;
}

void CTaskSmartFlee::AccelerateInVehicle_OnEnter()
{
	//Grab the ped.
	CPed* pPed = GetPed();

	//Ensure the ped is driving a vehicle.
	if(!taskVerifyf(pPed->GetIsDrivingVehicle(), "The ped is not driving a vehicle."))
	{
		return;
	}

	//B*7306390 - We might be in the process of taking ownership of the vehicle just return
	if(pPed->GetMyVehicle()->IsNetworkClone())
	{
		return;
	}

	//Try to scream.
	TryToScream();

	//Note that we have accelerated in our vehicle.
	m_uRunningFlags.SetFlag(RF_HasAcceleratedInVehicle);

	//Create the task for accelerate.
	static dev_s32 s_iMinTimeToAccelerate = 1500;
	static dev_s32 s_iMaxTimeToAccelerate = 2500;
	int iTimeToAccelerate = fwRandom::GetRandomNumberInRange(s_iMinTimeToAccelerate, s_iMaxTimeToAccelerate);
	CTaskCarSetTempAction* pTask = rage_new CTaskCarSetTempAction(pPed->GetMyVehicle(), (s8)TEMPACT_GOFORWARD_HARD, iTimeToAccelerate);
	pTask->GetConfigFlagsForInVehicleBasic().SetFlag(CTaskInVehicleBasic::IVF_DisableAmbients);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskSmartFlee::AccelerateInVehicle_OnUpdate()
{
	//Check if the sub-task has finished.
	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Flee in the vehicle.
		SetState(State_FleeInVehicle);
	}

	return FSM_Continue;
}

void CTaskSmartFlee::TakeOffSkis_OnEnter(CPed* pPed)
{
	pPed->SetIsSkiing(false);
}

CTask::FSM_Return CTaskSmartFlee::TakeOffSkis_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	SetState(State_Start);
	return FSM_Continue;
}

float CTaskSmartFlee::GetBaseTimeToCowerS()
{
	// Determine how long time to stay in this state, with some randomness.
	return sm_Tunables.m_TimeToCower*g_ReplayRand.GetRanged(0.9f, 1.1f);
}

void CTaskSmartFlee::Cower_OnEnter()
{
	Assert(!GetPed()->GetTaskData().GetIsFlagSet(CTaskFlags::DisableCower));

	// Start a subtask for cowering.
	CTask* pNewSubTask = rage_new CTaskCower(-1);
	SetNewTask(pNewSubTask);
}

CTask::FSM_Return CTaskSmartFlee::Cower_OnUpdate()
{
	// Check if we have run out of time for what the user requested.
	if(m_fleeTimer.IsSet() && m_fleeTimer.IsOutOfTime())
	{
		// Note: we may want to also allow transitions to State_ReturnToStartPosition here.
		SetState(State_Finish);
		
		return FSM_Continue;
	}

	// Check if the subtask has ended (probably timed out).
	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Finish);
		return FSM_Continue;
	}

	// Note: arguably we should also check AreWeOutOfRange() here to see if we're
	// far enough away from the danger, even though we didn't move. In practice I found two
	// issues with that:
	// - AreWeOutOfRange() currently requires us to move from our starting position, which we don't.
	// - Some important events like gunfire don't seem to actually have a position that
	//   gets updated, and since we don't move ourselves, they wouldn't get out of range.

	return FSM_Continue;
}

extern f32 g_HealthLostForHighPain;
extern const u32 g_PainVoice;

void CTaskSmartFlee::CowerInVehicle_OnEnter()
{
	//Check if we are driving the vehicle.
	CVehicle* pMyVehicle = GetPed()->GetVehiclePedInside();
	if(GetPed()->GetIsDrivingVehicle())
	{
		//Create the task.
		CTask* pTask = rage_new CTaskControlVehicle(pMyVehicle, rage_new CTaskVehicleStop(CTaskVehicleStop::SF_DontTerminateWhenStopped));
		SetNewTask(pTask);
	}

	// Say something initially
	TryToPlayTrappedSpeech();
}

CTask::FSM_Return CTaskSmartFlee::CowerInVehicle_OnUpdate()
{
	//Check if we are far enough away from the target.
	if(IsFarEnoughAwayFromTarget())
	{
		//Check if the flag is set.
		if(m_bCanCheckForVehicleExitInCower)
		{
			//Clear the flag.
			m_bCanCheckForVehicleExitInCower = false;

			//Choose the state.
			const s32 iNewState = ChooseStateForExitVehicle();
			if (iNewState != GetState())
			{
				SetState(iNewState);
				return FSM_Continue;
			}
			else
			{
				SetFlag(aiTaskFlags::RestartCurrentState);
				return FSM_Continue;
			}
		}

		//Finish the task.
		SetState(State_Finish);
	}
	else if(!ShouldCowerInsteadOfFleeDueToTarget() && CanExitVehicle())
	{
		//Exit the vehicle.
		SetStateForExitVehicle();
	}

	CPed* pPed = GetPed();
	if( pPed )
	{
		CTaskCowerScenario::PossiblyDestroyPedCoweringOffscreen(pPed, m_OffscreenDeletePedTimer);
	}

	//Check for a new priority event.
	CheckForNewPriorityEvent();

	return FSM_Continue;
}

void CTaskSmartFlee::SetDefaultTaskWanderDir(CPed* pPed)
{
	// Get the default task for the ped.  If it is a wander task then set the 
	// wander to continue from the original start position // Was previously from the wander flee pos, may need to re-add - CC
	aiTask* pTaskDefault = pPed->GetPedIntelligence()->GetTaskDefault();
	if (pTaskDefault && pTaskDefault->GetTaskType() == CTaskTypes::TASK_WANDER)
	{
#if 0 // CS
		CTaskWander* pTaskDefaultWander = static_cast<CTaskWander*>(pTaskDefault);
		pTaskDefaultWander->ContinueFromPosition(pPed->GetPosition(), pPed->GetTransform().GetHeading());
#endif // 0
	}
}

// Called from flee entity
bool CTaskSmartFlee::SetFleePosition(const Vector3& vNewFleePos, const float fStopDistance)
{
	Vector3 vFleePos;
	m_FleeTarget.GetPosition(vFleePos);

#if AI_OPTIMISATIONS_OFF
	Assertf(vNewFleePos.Mag2()>0.01f, "A ped is trying to flee from the origin.  This is likely to be an error.");
#endif
	static const float fUpdateDistSqr = 2.0f*2.0f;
	float fDiffSqr = (vFleePos - vNewFleePos).Mag2();

	if (fDiffSqr > fUpdateDistSqr || (m_fStopDistance != fStopDistance))
	{
		m_FleeTarget.SetPosition(vNewFleePos);
		SetStopDistance(fStopDistance);
		return true;
	}
	return false;
}


void CTaskSmartFlee::SetStopDistance(const float fStopDistance)
{
	if (taskVerifyf(fStopDistance <= ms_fMaxStopDistance, "CTaskSmartFlee::SetStopDistance: Incorrect distance %f > ms_fMaxStopDistance (%f)", fStopDistance, ms_fMaxStopDistance))
	{
		m_fStopDistance = fStopDistance;
	}
	else
	{	
		m_fStopDistance = ms_fMaxStopDistance;
	}
}

void CTaskSmartFlee::SetIgnorePreferPavementsTimeLeft(const float fIgnorePreferPavementsTimeLeft)
{
	if (taskVerifyf(fIgnorePreferPavementsTimeLeft <= ms_fMaxIgnorePreferPavementsTime, "CTaskSmartFlee::SetIgnorePreferPavementsTimeLeft: Incorrect time %f > ms_fMaxIgnorePreferPavementsTimeLeft (%f)", fIgnorePreferPavementsTimeLeft, ms_fMaxIgnorePreferPavementsTime))
	{
		m_fIgnorePreferPavementsTimeLeft = fIgnorePreferPavementsTimeLeft;
	}
	else
	{	
		m_fIgnorePreferPavementsTimeLeft = ms_fMaxIgnorePreferPavementsTime;
	}
}

#if __ASSERT
void CTaskSmartFlee::CheckForOriginFlee()
{
	const CEntity* pFleeEntity = m_FleeTarget.GetEntity();
	if (pFleeEntity)
	{
		taskAssertf(pFleeEntity->IsArchetypeSet(), "A ped has been told to run away from a building.");

		if (pFleeEntity->IsArchetypeSet() )
		{
			Vector3 vPos = VEC3V_TO_VECTOR3(pFleeEntity->GetTransform().GetPosition());
			if (vPos.Mag2()<0.1f)
			{
				const char * pModelName = CModelInfo::GetBaseModelInfoName(pFleeEntity->GetModelId());
				taskAssertf(false, "A ped is fleeing entity 0x%p (%s), however this entity is suspiciously close to the origin! (%.1f, %.1f, %.1f)",pFleeEntity, pModelName, vPos.x, vPos.y, vPos.z);
			}
		}	
	}	
}
#endif // __ASSERT

bool CTaskSmartFlee::IsValidForMotionTask(CTaskMotionBase& task) const
{
	bool bIsValid = task.IsInWater();
	if (!bIsValid)
		bIsValid = CTask::IsValidForMotionTask(task);

	if (!bIsValid && GetSubTask())
		bIsValid = GetSubTask()->IsValidForMotionTask(task);

	return bIsValid;
}

#if !__FINAL
void CTaskSmartFlee::Debug() const
{
#if DEBUG_DRAW
	const CPed * pPed = GetPed();
	Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	Vector3 vFleePos;
	m_FleeTarget.GetPosition(vFleePos);

	grcDebugDraw::Line(vPedPos, vFleePos, Color_red);
	grcDebugDraw::Sphere(vFleePos, 0.3f, m_FleeTarget.GetEntity() ? Color_red : Color_white);

	//if(NetworkInterface::IsGameInProgress() && GetState() == State_FleeInVehicle)
	//{
		//char dbgText[64];
		//float dist = (vFleePos - vPedPos).Mag();
		//sprintf(dbgText, "Intensity: %.1f, Dist %.1f, Time: %.1f",m_fFleeInVehicleIntensity, dist, m_fFleeInVehicleTime);
		//grcDebugDraw::Text(vPedPos, Color_red, 0, 20, dbgText);

		//sprintf(dbgText, "Driving parallel to target: %.1f", m_fDrivingParallelToTargetTime);
		//grcDebugDraw::Text(vPedPos, Color_red, 0, 20, dbgText);
	//}

	if (GetSubTask())
	{
		GetSubTask()->Debug();
	}
#endif
}

const char * CTaskSmartFlee::GetStaticStateName( s32 iState )
{
	// Make sure FSM state is valid
	Assert(iState>=0&&iState<=State_Finish);
	static const char* aSmartFleeStateNames[] = 
	{
		"State_Start",
		"State_ExitVehicle",
		"State_WaitBeforeExitVehicle",
		"State_FleeOnFoot",
		"State_FleeOnSkis",
		"State_SeekCover",
		"State_CrouchAtCover - Placeholder for playing scared anims etc",
		"State_ReturnToStartPosition",
		"State_EnterVehicle",
		"State_FleeInVehicle",
		"State_EmergencyStopInVehicle",
		"State_ReverseInVehicle",
		"State_AccelerateInVehicle",
		"State_StandStill",
		"State_HandsUp",
		"State_TakeOffSkis",
		"State_Cower",
		"State_CowerInVehicle",
		"State_Finish",
	};
	return aSmartFleeStateNames[iState];
}
#endif

//Clone tasks
//////////////////////////////////////////////////////////////////////////
CTaskInfo* CTaskSmartFlee::CreateQueriableState() const
{
	CTaskInfo* pInfo = NULL;

	Vector3 vFleeTargetPos;

	if(!m_FleeTarget.GetPosition(vFleeTargetPos))
	{
		vFleeTargetPos = VEC3V_TO_VECTOR3(m_vLastTargetPosition);
	}

	CVehicle* pLastVehiclePedInside = NULL;

	if(GetEntity() && GetPed() && GetPed()->GetPedIntelligence())
	{
		pLastVehiclePedInside = GetPed()->GetPedIntelligence()->GetLastVehiclePedInside();
	}

	pInfo = rage_new CClonedControlTaskSmartFleeInfo(GetState(), m_FleeTarget.GetEntity(), pLastVehiclePedInside, m_hAmbientClips.GetHash(), vFleeTargetPos, m_vStartPos, m_fStopDistance, m_fIgnorePreferPavementsTimeLeft, m_uExecConditions );

	return pInfo;
}

void CTaskSmartFlee::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	taskAssert(pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_SMART_FLEE);

	CClonedControlTaskSmartFleeInfo *smartFleeInfo = static_cast<CClonedControlTaskSmartFleeInfo*>(pTaskInfo);

	if ( smartFleeInfo->GetTargetCEntity() )
	{
		if ( IsValidTargetEntity(smartFleeInfo->GetTargetCEntity()) )
		{
			m_FleeTarget = CAITarget(smartFleeInfo->GetTargetCEntity());
		}
	}
	else
	{
		m_FleeTarget = CAITarget(smartFleeInfo->GetTargetPos());
	}

	if ( smartFleeInfo->GetVehicle() )
	{
		m_pCloneMyVehicle = smartFleeInfo->GetVehicle();
	}


	m_vStartPos	= smartFleeInfo->GetStartPos();


	u32	conditionalAnimGroupHash = smartFleeInfo->GetConditionalAnimGroupHash();


	if(conditionalAnimGroupHash!=0)
	{
		m_hAmbientClips.SetHash(conditionalAnimGroupHash);
	}

	SetExecConditions(smartFleeInfo->GetExecConditions());
	SetStopDistance(smartFleeInfo->GetStopDistance());
	SetIgnorePreferPavementsTimeLeft(smartFleeInfo->GetIgnorePreferPavementsTimeLeft());

	// Call the base implementation - syncs the state
	CTaskFSMClone::ReadQueriableState(pTaskInfo);
}

CTaskFSMClone *CTaskSmartFlee::CreateTaskForClonePed(CPed *pPed)
{
	CTaskSmartFlee* pTask = NULL;

	pTask = static_cast<CTaskSmartFlee*>(Copy());

	pTask->SetState(GetState());

	pTask->m_vStartPos = m_vStartPos;

	pTask->m_uTimeStartedFleeing = m_uTimeStartedFleeing;

	if(CTaskReactAndFlee::CheckForCowerMigrate(pPed))
	{
		pTask->m_bIncomingMigratedWhileCowering = true;
		m_bOutgoingMigratedWhileCowering = true;
	}

	return pTask;
}

CTaskFSMClone *CTaskSmartFlee::CreateTaskForLocalPed(CPed *pPed)
{
	// do the same as clone ped
	return CreateTaskForClonePed(pPed);
}

void CTaskSmartFlee::OnCloneTaskNoLongerRunningOnOwner()
{
	SetStateFromNetwork(CTaskSmartFlee::State_Finish);
}

// Is the specified state handled by the clone FSM
bool CTaskSmartFlee::StateChangeHandledByCloneFSM(s32 iState)
{
	switch(iState)
	{
	case State_Finish:
		return true;

	default:
		return false;
	}

}

//////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskSmartFlee::UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	if(!StateChangeHandledByCloneFSM(iState))
	{
		if(iEvent == OnUpdate)
		{
			if(GetStateFromNetwork() != -1 && GetStateFromNetwork() != GetState())
			{
                bool bCanSetState = (GetState() != State_ExitVehicle);

                if(GetStateFromNetwork() == State_ExitVehicle)
                {
                    bCanSetState = pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE);
                }

                if(bCanSetState)
                {
				    SetState(GetStateFromNetwork());
                }
			}
		}
	}

FSM_Begin
		// Start
	FSM_State(State_Start)
		FSM_OnEnter
			Start_OnEnter(pPed);

	FSM_State(State_HandsUp)
		FSM_OnEnter
			HandsUp_OnEnter(pPed);

	// EnterVehicle
	FSM_State(State_EnterVehicle)
		FSM_OnEnter
		EnterVehicle_OnEnterClone(pPed);
	FSM_OnUpdate
		return EnterVehicle_OnUpdateClone(pPed);

    FSM_State(State_ExitVehicle)
		FSM_OnEnter
			ExitVehicle_OnEnterClone(pPed);
        FSM_OnUpdate
			ExitVehicle_OnUpdateClone(pPed);

	FSM_State(State_Finish)
			FSM_OnUpdate
			return FSM_Quit;

	FSM_State(State_FleeOnFoot)
		FSM_OnUpdate
		FleeOnFoot_OnUpdateClone();
		return FSM_Continue;
	FSM_State(State_Cower)
			FSM_OnEnter
			Cower_OnEnter();
		FSM_OnUpdate
			return Cower_OnUpdate();

	FSM_State(State_ExitVehicle)
	FSM_State(State_WaitBeforeExitVehicle)
	FSM_State(State_FleeOnSkis)
	FSM_State(State_SeekCover)
	FSM_State(State_CrouchAtCover)
	FSM_State(State_ReturnToStartPosition)
	FSM_State(State_FleeInVehicle)
	FSM_State(State_ReverseInVehicle)
	FSM_State(State_EmergencyStopInVehicle)
	FSM_State(State_AccelerateInVehicle)
	FSM_State(State_StandStill)
	FSM_State(State_HandsUp)
	FSM_State(State_TakeOffSkis)
	FSM_State(State_CowerInVehicle)

FSM_End

}

void CTaskSmartFlee::TryToScream()
{
	//Ensure we can scream.
	if(!CanScream())
	{
		return;
	}

	//Scream.
	Scream();
}

bool CTaskSmartFlee::CanScream() const
{
	//Ensure we can scream.
	if(m_uConfigFlags.IsFlagSet(CF_DontScream))
	{
		return false;
	}

	//Grab the ped.
	const CPed* pPed = GetPed();

	//Ensure the flee flag is set.
	if(!pPed->GetPedIntelligence()->GetFleeBehaviour().GetFleeFlags().IsFlagSet(CFleeBehaviour::BF_CanScream))
	{
		return false;
	}

	//Ensure the bravery flag is not set.
	if(pPed->CheckBraveryFlags(BF_DONT_SCREAM_ON_FLEE))
	{
		return false;
	}
	
	//Don't allow peds to scream anymore if the source of flee was player and he/she had it's wanted level cleared by script after our flee task has started
	CEvent* pEvent = pPed->GetPedIntelligence()->GetCurrentEvent();
	if (pEvent)
	{
		CPed* pSourcePed = pEvent->GetSourcePed();
		if (pSourcePed && pSourcePed->IsAPlayerPed() && pSourcePed->IsLocalPlayer())
		{
			CWanted* pWanted = pSourcePed->GetPlayerWanted();
			if (pWanted && pWanted->GetLastTimeWantedLevelClearedByScript() > m_uTimeStartedFleeing)
			{
				return false;
			}
		}
	}

	return true;
}

void CTaskSmartFlee::Scream()
{
	//Assert that we can scream.
	taskAssertf(CanScream(), "We are not allowed to scream.");

	//Reset the timer
	m_screamTimer.Set(fwTimer::GetTimeInMilliseconds(), GetScreamTimerDuration());

	//Set the flag.
	bool isFirstReaction = !m_uRunningFlags.IsFlagSet(RF_HasScreamed);
	m_uRunningFlags.SetFlag(RF_HasScreamed);

	//Grab the ped.
	CPed* pPed = GetPed();

	//Check the state.
	switch(GetState())
	{
		case State_FleeOnFoot:
		case State_FleeOnSkis:
		{
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

				if(pPed->GetSpeechAudioEntity())
				{
					pPed->GetSpeechAudioEntity()->Panic(pEvent->GetEventType(), isFirstReaction);
				}
			}
			break;
		}
		case State_WaitBeforeExitVehicle:
		case State_FleeInVehicle:
		case State_ReverseInVehicle:
		case State_AccelerateInVehicle:
		case State_CowerInVehicle:
		{
			if(isFirstReaction)
				pPed->NewSay(IsLifeInDanger() ? "GENERIC_SHOCKED_HIGH" : "GENERIC_SHOCKED_MED");
			else
				pPed->NewSay(IsLifeInDanger() ? "GENERIC_FRIGHTENED_HIGH" : "GENERIC_FRIGHTENED_MED");
			break;
		}
		default:
		{
			return;
		}
	}

	//Generate the scream events.
	GenerateAIScreamEvents();
}

void CTaskSmartFlee::GenerateAIScreamEvents()
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
			// The victim is the one getting a gun pointed at them if EVENT_SHOCKING_SEEN_WEAPON_THREAT, but us if responding to EVENT_GUN_AIMED_AT.
			CPed* pVictim = pEvent->GetEventType() == EVENT_SHOCKING_SEEN_WEAPON_THREAT ? pEvent->GetTargetPed() : pPed;

			// Make sure the victim exists and the assaulter isn't friends with them.
			if (pVictim && !pSourcePed->GetPedIntelligence()->IsFriendlyWith(*pVictim))
			{
				CEventCrimeCryForHelp::CreateAndCommunicateEvent(pVictim, pSourcePed); 
			}
		}
	}
}

s32 CTaskSmartFlee::GetScreamTimerDuration() const
{
	return audEngineUtil::GetRandomNumberInRange(ms_iMinPanicAudioRepeatTime, ms_iMaxPanicAudioRepeatTime);
}

#if __BANK 

void CTaskSmartFlee::DEBUG_RenderFleeingOutOfPavementManager()
{
	sm_PedsFleeingOutOfPavementManager.DEBUG_RenderList();
}

#endif // __BANK

void CTaskSmartFlee::TryToPlayTrappedSpeech()
{
	bool bShouldPlayTrappedSpeech = true;

	//For full large capacity vehicles, we only want to trigger this on about 20% of the occupants
	CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
	if (pVehicle)
	{
		s32 iNumPassengers = pVehicle->GetSeatManager() ? pVehicle->GetSeatManager()->CountPedsInSeats(true, false) : 0;
		if (iNumPassengers > 4)
		{
			float fChance = 0.33f;		
			float fRandom = fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
			if (fRandom > fChance)
			{
				bShouldPlayTrappedSpeech = false;
			}			
		}
	}

	if (bShouldPlayTrappedSpeech)
	{
		GetPed()->NewSay("TRAPPED");
	}
}

//-------------------------------------------------------------------------
// Peds fleeing on roads
//-------------------------------------------------------------------------

bool CTaskSmartFlee::PedFleeingOutOfPavementIsValidCB(const CPed& rPed)
{
	bool bFleeingOutOfPavement = false;

	CTaskSmartFlee* pTaskSmartFlee = SafeCast(CTaskSmartFlee, rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_SMART_FLEE));
	if (pTaskSmartFlee)
	{
		const CTaskMoveFollowNavMesh* pNavmeshTask = pTaskSmartFlee->FindFollowNavMeshTask();
		bFleeingOutOfPavement = (pNavmeshTask != NULL);
	}

	return bFleeingOutOfPavement;
}

void CTaskSmartFlee::PedFleeingOutOfPavementOnOutOfSightCB(CPed& rPed)
{
	// We can force this ped to repath
	CTaskSmartFlee* pTaskSmartFlee = SafeCast(CTaskSmartFlee, rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_SMART_FLEE));
	if (pTaskSmartFlee)
	{
		pTaskSmartFlee->DisableIgnorePreferPavementsTime();
		pTaskSmartFlee->ForceRepath(&rPed);
	}
}

//-------------------------------------------------------------------------
// Task info for Task Smart Flee
//-------------------------------------------------------------------------

//-------------------------------------------------------------------------
// Constructor
//-------------------------------------------------------------------------
CClonedControlTaskSmartFleeInfo::CClonedControlTaskSmartFleeInfo(s32 state, const CEntity* pTarget, const CVehicle* pVehicle, u32 conditionalAnimGroupHash, const Vector3& vecTargetPos, const Vector3& vecStartPos, float fStopDistance, float fIgnorePreferPavementsTimeLeft, fwFlags8 uExecConditions) 
: m_TargetEntity(const_cast<CEntity*>(pTarget))
, m_VehicleEntity(const_cast<CVehicle*>(pVehicle))
, m_vStartPos(vecStartPos)
, m_vTargetPos(vecTargetPos)
, m_conditionalAnimGroupHash(conditionalAnimGroupHash)
, m_fStopDistance(fStopDistance)
, m_fIgnorePreferPavementsTimeLeft(fIgnorePreferPavementsTimeLeft)
, m_uExecConditions(uExecConditions)
{
	SetStatusFromMainTaskState(state);
}

CClonedControlTaskSmartFleeInfo::CClonedControlTaskSmartFleeInfo() 
: m_vStartPos(Vector3(0,0,0))
, m_vTargetPos(Vector3(0,0,0))
, m_conditionalAnimGroupHash(0)
, m_fStopDistance(-1.0f)
, m_fIgnorePreferPavementsTimeLeft(-1.0f)
, m_uExecConditions(0u)
{
	m_TargetEntity.Invalidate();
	m_VehicleEntity.Invalidate();
}


//-------------------------------------------------------------------------
// Destructor
//-------------------------------------------------------------------------
CClonedControlTaskSmartFleeInfo::~CClonedControlTaskSmartFleeInfo()
{
}

int CClonedControlTaskSmartFleeInfo::GetScriptedTaskType() const
{
    if(GetTargetCEntity())
	{
        return SCRIPT_TASK_SMART_FLEE_PED;
    }
    else
    {
        return SCRIPT_TASK_SMART_FLEE_POINT;
    }
}

CTaskFSMClone * CClonedControlTaskSmartFleeInfo::CreateCloneFSMTask()
{
	CTaskSmartFlee* pSmartFleeTask = NULL;

	if(GetTargetCEntity())
	{
		pSmartFleeTask = rage_new CTaskSmartFlee(CAITarget(GetTargetCEntity()));
	}
	else
	{
		pSmartFleeTask = rage_new CTaskSmartFlee(CAITarget(GetTargetPos()));
	}

	return pSmartFleeTask;
}

CTask *CClonedControlTaskSmartFleeInfo::CreateLocalTask(fwEntity* UNUSED_PARAM(pEntity))
{
    CTaskFSMClone *pTask = CreateCloneFSMTask();
	((CTaskSmartFlee*)pTask)->SetUseLargerSearchExtents(true);
	return pTask;
}

/////////////////////
//	ESCAPE BLAST   //
/////////////////////

const float CTaskEscapeBlast::ms_iPostExplosionWaitTime = 4.0f;

CTaskEscapeBlast::CTaskEscapeBlast(CEntity* pBlastEntity, 
								   const Vector3& vBlastPos, 
								   const float fSafeDistance,
								   const bool bStopTaskAtSafeDistance,
								   const float m_fFleeTime)
								   : m_pBlastEntity(pBlastEntity),
								   m_vBlastPos(vBlastPos),
								   m_fSafeDistance(fSafeDistance),
								   m_fFleeTime(m_fFleeTime),
								   m_bStopTaskAtSafeDistance(bStopTaskAtSafeDistance)
{
	SetInternalTaskType(CTaskTypes::TASK_ESCAPE_BLAST);
}

CTask::FSM_Return CTaskEscapeBlast::HandleFleeTimer()
{
	// If the timer is set, count down to when the task should finish
	if( m_fFleeTime > 0.0f )
	{
		m_fFleeTime -= GetTimeStep();
		if( m_fFleeTime <= 0.0f )
		{
			// Timer out so terminate
			SetState(State_Finish);
			return FSM_Continue;
		}
	}
	// If no timer is set and the entity no longer exists, abort in a safe time
	else if( !m_pBlastEntity )
	{
		m_fFleeTime = ms_iPostExplosionWaitTime;
	}
	// Wait for vehicles to explode then set a timer to continue crouching
	else if( m_pBlastEntity->GetType() == ENTITY_TYPE_VEHICLE )
	{
		CVehicle* pVehicle = static_cast<CVehicle*>(m_pBlastEntity.Get());
		if (!g_fireMan.IsEntityOnFire(pVehicle))
		{
			m_fFleeTime = ms_iPostExplosionWaitTime;
		}
		if (!pVehicle->IsOnFire() )
		{
			m_fFleeTime = ms_iPostExplosionWaitTime;
		}
		if (pVehicle->GetStatus() == STATUS_WRECKED)
		{
			m_fFleeTime = ms_iPostExplosionWaitTime;
		}
	}
	return FSM_Continue;
}

bool CTaskEscapeBlast::OutsideSafeDistance() const
{
	return (m_vBlastPos.Dist2( VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition()) ) > rage::square( m_fSafeDistance ));
}

CTask::FSM_Return CTaskEscapeBlast::ProcessPreFSM()
{
	//Grab the ped.
	CPed* pPed = GetPed();

	// Allow certain lod modes
	pPed->GetPedAiLod().ClearBlockedLodFlag(CPedAILod::AL_LodPhysics);
	pPed->GetPedAiLod().ClearBlockedLodFlag(CPedAILod::AL_LodMotionTask);

	return FSM_Continue;
}


CTask::FSM_Return CTaskEscapeBlast::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin

		// Start
		FSM_State(State_Start)
		FSM_OnUpdate
		return Start_OnUpdate(pPed);	

	// Crouch
	FSM_State(State_Crouch)
		FSM_OnEnter
		Crouch_OnEnter(pPed);
	FSM_OnUpdate
		return Crouch_OnUpdate(pPed);

	// MoveToSafePoint
	FSM_State(State_MoveToSafePoint)
		FSM_OnEnter
		MoveToSafePoint_OnEnter(pPed);
	FSM_OnUpdate
		return MoveToSafePoint_OnUpdate(pPed);
		
	// DuckAndCover
	FSM_State(State_DuckAndCover)
		FSM_OnEnter
			DuckAndCover_OnEnter(pPed);
		FSM_OnUpdate
			return DuckAndCover_OnUpdate(pPed);

	// End quit - its finished.
	FSM_State(State_Finish)
		FSM_OnUpdate
		return FSM_Quit;

	FSM_End
}


CTask::FSM_Return CTaskEscapeBlast::Start_OnUpdate(CPed* pPed)
{
	pPed->NewSay("EXPLOSION_IS_IMMINENT");
	pPed->SetIsCrouching(false);

	// If outside range, exit or crouch
	if( OutsideSafeDistance() )
	{
		if (m_bStopTaskAtSafeDistance)
		{
			SetState(State_Finish);
			return FSM_Continue;
		}
		else
		{
			SetState(State_Crouch);
			return FSM_Continue;
		}
	}

	// In range, so flee till outside range
	SetState(State_MoveToSafePoint);

	return FSM_Continue;
}

void CTaskEscapeBlast::Crouch_OnEnter(CPed* UNUSED_PARAM(pPed))
{
	SetNewTask(rage_new CTaskCrouch( 9999999 ));
}

CTask::FSM_Return CTaskEscapeBlast::Crouch_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	HandleFleeTimer();	// If m_iFleeTime < 0, the task will goto state finish

	return FSM_Continue;
}

void CTaskEscapeBlast::MoveToSafePoint_OnEnter(CPed* UNUSED_PARAM(pPed))
{
	CTaskSmartFlee* pTask = NULL;
	if( m_pBlastEntity )
	{
		pTask = rage_new CTaskSmartFlee(CAITarget(m_pBlastEntity), m_fSafeDistance);
	}
	else
	{
		pTask = rage_new CTaskSmartFlee(CAITarget(m_vBlastPos), m_fSafeDistance);
	}
	
	//Don't stop to put hands up... the blast is more dangerous.
	pTask->SetFleeFlags(1<<DisableHandsUp);
	
	//Quit the task when out of range.
	pTask->SetQuitTaskIfOutOfRange(true);
	
	//Set the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskEscapeBlast::MoveToSafePoint_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	//Check if the subtask is finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Duck and cover.
		SetState(State_DuckAndCover);
	}
	
	HandleFleeTimer();	// If m_iFleeTime < 0, the task will goto state finish

	return FSM_Continue;
}

void CTaskEscapeBlast::DuckAndCover_OnEnter(CPed* UNUSED_PARAM(pPed))
{
	//Set the task.
	CTask* pTask = rage_new CTaskDuckAndCover();
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskEscapeBlast::DuckAndCover_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	HandleFleeTimer();	// If m_iFleeTime < 0, the task will goto state finish

	return FSM_Continue;
}

#if !__FINAL
const char * CTaskEscapeBlast::GetStaticStateName( s32 iState )
{
	// Make sure FSM state is valid
	Assert(iState>=0&&iState<=State_Finish);
	static const char* aEscapeBlastStateNames[] = 
	{
		"State_Start",
		"State_Crouch",
		"State_MoveToSafePoint",
		"State_DuckAndCover",
		"State_Finish"
	};

	return aEscapeBlastStateNames[iState];
}
#endif

//////////////////////////////////////////////////////////////////////////
CTaskInfo* CTaskEscapeBlast::CreateQueriableState() const
{
	CTaskInfo* pInfo = NULL;

	pInfo = rage_new CClonedControlTaskEscapeBlastInfo(GetState());

	return pInfo;
}

void CTaskEscapeBlast::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	taskAssert(pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_ESCAPE_BLAST);

	// Call the base implementation - syncs the state
	CTaskFSMClone::ReadQueriableState(pTaskInfo);
}

void CTaskEscapeBlast::OnCloneTaskNoLongerRunningOnOwner()
{
	SetStateFromNetwork(CTaskEscapeBlast::State_Finish);
}

// Is the specified state handled by the clone FSM
bool CTaskEscapeBlast::StateChangeHandledByCloneFSM(s32 iState)
{
	switch(iState)
	{
	case State_Finish:
		return true;

	default:
		return false;
	}
}


//////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskEscapeBlast::UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent)
{
	//CPed *pPed = GetPed(); //Get the ped ptr.

	if(!StateChangeHandledByCloneFSM(iState))
	{
		if(iEvent == OnUpdate)
		{
			if(GetStateFromNetwork() != -1 && GetStateFromNetwork() != GetState())
			{
				SetState(GetStateFromNetwork());
			}
		}
	}

	FSM_Begin

	// End quit - its finished.
	FSM_State(State_Finish)
		FSM_OnUpdate
		return FSM_Quit;

	// Crouch
	FSM_State(State_Crouch)
	// Start
	FSM_State(State_Start)
	// MoveToSafePoint
	FSM_State(State_MoveToSafePoint)
	// DuckAndCover
	FSM_State(State_DuckAndCover)
	FSM_End
}

//-------------------------------------------------------------------------
// Task info for Escape Blast
//-------------------------------------------------------------------------
CClonedControlTaskEscapeBlastInfo::CClonedControlTaskEscapeBlastInfo(s32 escapeBlastState)
{
	SetStatusFromMainTaskState(escapeBlastState);
}

CClonedControlTaskEscapeBlastInfo::CClonedControlTaskEscapeBlastInfo()
{
}

CTaskFSMClone * CClonedControlTaskEscapeBlastInfo::CreateCloneFSMTask()
{
	CTaskEscapeBlast* pEscapeBlastTask = NULL;

	pEscapeBlastTask = rage_new CTaskEscapeBlast(NULL, Vector3(0.0f,0.0f,0.0f), 0.0f, false);  //don't care about any of the initial values since clone will follow remote states

	return pEscapeBlastTask;
}

//////////////////////////////////////////////////////////////////////////
// SCENARIO FLEE
//////////////////////////////////////////////////////////////////////////

CTaskScenarioFlee::Tunables CTaskScenarioFlee::sm_Tunables;

#if __BANK
bool CTaskScenarioFlee::sm_bRenderProbeChecks = false;
#endif

IMPLEMENT_RESPONSE_TASK_TUNABLES(CTaskScenarioFlee, 0xa158d6b0);

CTaskScenarioFlee::CTaskScenarioFlee(CEntity* pSourceEntity, int fleeFlags)
: m_pEventSourceEntity(pSourceEntity)
, m_pScenarioPoint(NULL)
, m_ProbeTimer()
, m_CapsuleResult()
, m_vCollisionPoint(V_ZERO)
, m_fFleeSearchRange(sm_Tunables.m_fInitialSearchRadius)
, m_ScenarioTypeReal(-1)
, m_bAboutToCollide(false)
, m_bAlteredPathForCollision(false)
{
	Assign( m_FleeFlags, fleeFlags );
	SetState(State_FindScenario);
	SetInternalTaskType(CTaskTypes::TASK_SCENARIO_FLEE);
	
	// Init the timer for the first time.
	m_ProbeTimer.Set(fwTimer::GetTimeInMilliseconds(), sm_Tunables.m_uAvoidanceProbeInterval);
}

//////////////////////////////////////////////////////////////////////////

CTaskScenarioFlee::~CTaskScenarioFlee()
{
	taskAssertf(!m_pScenarioPoint, "The scenario point is still valid in the destructor, possible leak.");
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskScenarioFlee::ProcessPreFSM()
{
	// If what we are fleeing from no longer exists, quit.
	if (!m_pEventSourceEntity)
	{
		return FSM_Quit;
	}

	// Grab the ped.
	CPed* pPed = GetPed();

	// Hug the surface if applicable.
	if (pPed->GetPedIntelligence()->GetNavCapabilities().IsFlagSet(CPedNavCapabilityInfo::FLAG_PREFER_NEAR_WATER_SURFACE))
	{
		CTaskFishLocomotion::CheckAndRespectSwimNearSurface(pPed);
	}
	else
	{
		// Allow certain lod modes on non-surface skimming fish.
		pPed->GetPedAiLod().ClearBlockedLodFlag(CPedAILod::AL_LodPhysics);
		pPed->GetPedAiLod().ClearBlockedLodFlag(CPedAILod::AL_LodMotionTask);
	}

	// Possibly scan for collisions.
	if (ShouldProbeForCollisions())
	{
		// Cast the probe, storing the location of an obstruction if one is found.
		ProbeForCollisions();
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

void CTaskScenarioFlee::CleanUp()
{
	UnreserveScenarioPoint();
}

//////////////////////////////////////////////////////////////////////////

CTaskScenarioFlee::FSM_Return CTaskScenarioFlee::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		FSM_State(State_FindScenario)
			FSM_OnUpdate
				return FindScenario_OnUpdate();
		FSM_State(State_MoveToScenario)
			FSM_OnEnter
				return MoveToScenario_OnEnter();
			FSM_OnUpdate
				return MoveToScenario_OnUpdate();
		FSM_State(State_UseScenario)
			FSM_OnEnter
				return UseScenario_OnEnter();
			FSM_OnUpdate
				return UseScenario_OnUpdate();
		FSM_State(State_Flee)
			FSM_OnEnter
				return Flee_OnEnter();
			FSM_OnUpdate
				return Flee_OnUpdate();
		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

//////////////////////////////////////////////////////////////////////////

void CTaskScenarioFlee::FindNewFleeScenario()
{
	// Cache our ped and try to find a new scenario, null cases are handled in the calling function
	CPed* pPed = GetPed();
	const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

	// we want the center of our search to be in the opposite direction of our target.
	Vec3V vSourcePosition = m_pEventSourceEntity->GetTransform().GetPosition();
	Vector3 vDirection = vPedPosition - VEC3V_TO_VECTOR3(vSourcePosition);
	vDirection.Normalize();
	//our idealized flee point is a point halfway between our position and a point directed away from our chaser
	Vector3 vSearchPosition = vPedPosition + (vDirection * sm_Tunables.m_fFleeProjectRange);

	UnreserveScenarioPoint();

	CScenarioFinder::FindOptions args;
	args.m_bRandom = false;
	args.m_DesiredScenarioClassId = CScenarioFleeInfo::GetStaticClassId();
	args.m_FleeFromPos = vSourcePosition;
	args.m_bCheckBlockingAreas = false;
	m_pScenarioPoint = CScenarioFinder::FindNewScenario(pPed, vSearchPosition, m_fFleeSearchRange, args, m_ScenarioTypeReal);
    if (!m_pScenarioPoint)
	{
		//cap out the search range so we don't eventually try to search the whole world
		m_fFleeSearchRange = Min(sm_Tunables.m_fSearchRangeMax, m_fFleeSearchRange * sm_Tunables.m_fSearchScaler);
	}
}

//////////////////////////////////////////////////////////////////////////

void CTaskScenarioFlee::UnreserveScenarioPoint()
{
	// If we have a current effect, decrease our uses
	if( m_pScenarioPoint )
	{
		m_pScenarioPoint->RemoveUse();
	}

	// Null out the effect so we don't do any accidental decreasing of the uses in other calls
	m_pScenarioPoint = NULL;

	m_ScenarioTypeReal = -1;
}

//////////////////////////////////////////////////////////////////////////

CTaskScenarioFlee::FSM_Return CTaskScenarioFlee::FindScenario_OnUpdate()
{
	// Attempt to find a new scenario to flee to
	FindNewFleeScenario();
	if(m_pScenarioPoint)
	{
		m_pScenarioPoint->AddUse();
		SetState(State_MoveToScenario);
	}
	else
	{
		SetState(State_Flee);
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskScenarioFlee::MoveToScenario_OnEnter()
{
	// Setup our move task to go to the scenario point
	CTaskMove* pMoveTask = NULL;

	const CScenarioInfo* pScenarioInfo =  CScenarioManager::GetScenarioInfo(m_ScenarioTypeReal);
	if ( pScenarioInfo && pScenarioInfo->GetIsFlagSet(CScenarioInfoFlags::IgnoreNavMesh) )
	{
		// Don't use navmesh, just goto point (added for things like fish)
		pMoveTask = rage_new CTaskMoveGoToPoint(MOVEBLENDRATIO_RUN, VEC3V_TO_VECTOR3(m_pScenarioPoint->GetPosition()));
	}
	else
	{
		CTaskMoveFollowNavMesh* pNavmeshTask = rage_new CTaskMoveFollowNavMesh(MOVEBLENDRATIO_RUN, VEC3V_TO_VECTOR3(m_pScenarioPoint->GetPosition()), sm_Tunables.m_fTargetScenarioRadius);
		CPed* pPed = GetPed();

		if (pNavmeshTask)
		{
			// Respect pavement preferences.
			if (pPed->GetPedIntelligence() && pPed->GetPedIntelligence()->GetFleeBehaviour().GetFleeFlags().IsFlagSet(CFleeBehaviour::BF_PreferPavements))
			{
				pNavmeshTask->SetKeepToPavements(true);
			}

			if (pScenarioInfo && m_pScenarioPoint)
			{
				// Respect certain appropriate scenario info flags.
				if (!pScenarioInfo->GetIsFlagSet(CScenarioInfoFlags::IgnoreScenarioPointHeading))
				{
					pNavmeshTask->SetTargetStopHeading(m_pScenarioPoint->GetLocalDirectionf());
				}

				if (!pScenarioInfo->GetIsFlagSet(CScenarioInfoFlags::IgnoreSlideToCoordOnArrival))
				{
					pNavmeshTask->SetSlideToCoordAtEnd(true, m_pScenarioPoint->GetLocalDirectionf());
				}
			}
		}

		pMoveTask = pNavmeshTask;
	}

	SetNewTask(rage_new CTaskComplexControlMovement(pMoveTask));

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskScenarioFlee::MoveToScenario_OnUpdate()
{
	// This is where we check for a failing movement task
	if (GetSubTask()->GetTaskType() == CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT &&	// Check if we are in the follow navmesh task
		(static_cast<CTaskComplexControlMovement*>(GetSubTask()))->GetBackupCopyOfMovementSubtask()->GetTaskType() == CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH )
	{
		// Get the movement task
		CTaskMoveFollowNavMesh* pNavmeshTask = (CTaskMoveFollowNavMesh*)((CTaskComplexControlMovement*)GetSubTask())->GetRunningMovementTask(GetPed());
		if (pNavmeshTask)
		{
			// If we have not found a route and we've exhausted our options
			if( pNavmeshTask->GetNavMeshRouteResult() == NAVMESHROUTE_ROUTE_NOT_FOUND &&
				pNavmeshTask->GetNavMeshRouteMethod() >= CTaskMoveFollowNavMesh::eRouteMethod_LastMethodWhichAvoidsObjects)
			{
				// Go back to trying to find a scenario
				UnreserveScenarioPoint();
				SetState(State_FindScenario);
				return FSM_Continue;
			}
		}
	}


	// If our task is finished then go ahead and start using the scenario
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		// m_pScenarioPoint can probably be NULL here if the point got destroyed (streamed out, etc).
		// In that case, it may be best to let the subtask continue until it finishes, and then
		// try to find a new scenario, which is how it behaves now.

		const CScenarioInfo* pScenarioInfo = NULL;
		if(m_pScenarioPoint)
		{
			pScenarioInfo = CScenarioManager::GetScenarioInfo(m_ScenarioTypeReal);
		}

		if ( pScenarioInfo && pScenarioInfo->GetIsFlagSet(CScenarioInfoFlags::DespawnOnUse) )
		{
			GetPed()->FlagToDestroyWhenNextProcessed();
			SetState(State_Finish);
			return FSM_Continue;
		}
		else
		{
			bool bUsePoint = false;

			// If we ended up close enough to use the scenario then change to that state, otherwise try to find a scenario again
			if (m_pScenarioPoint)
			{
				Vec3V vPed = GetPed()->GetTransform().GetPosition();
				Vec3V vPoint = m_pScenarioPoint->GetPosition();
				const CScenarioInfo* pScenarioInfo =  CScenarioManager::GetScenarioInfo(m_ScenarioTypeReal);
				if (!pScenarioInfo || !pScenarioInfo->GetIsFlagSet(CScenarioInfoFlags::IgnoreNavMesh))
				{
					//Normally only want to compute the distance in 2d space, unless we are ignoring the navmesh (things like fish will navigate to the point in 3d).
					vPed.SetZ(ScalarV(V_ZERO));
					vPoint.SetZ(ScalarV(V_ZERO));
				}

				if (IsLessThanAll(DistSquared(vPed, vPoint), ScalarV(sm_Tunables.m_fTargetScenarioRadius * sm_Tunables.m_fTargetScenarioRadius)))
				{
					bUsePoint = true;
				}
			}
			if(bUsePoint)
			{
				SetState(State_UseScenario);
				return FSM_Continue;
			}
			else
			{
				UnreserveScenarioPoint();
				SetState(State_FindScenario);
				return FSM_Continue;
			}
		}
	}

	// Bail out if about to collide.
	if (m_bAboutToCollide && !m_bAlteredPathForCollision)
	{
		SetState(State_Flee);
		return FSM_Continue;
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskScenarioFlee::UseScenario_OnEnter()
{
	// Get our scenario task and set it
	CTask* pSubTask = CScenarioManager::CreateTask(m_pScenarioPoint, m_ScenarioTypeReal);
	SetNewTask(pSubTask);

	// set our source distance timer so we check ever 1/2 second for proximity of the source entity
	m_sourceDistanceTimer.Set(fwTimer::GetTimeInMilliseconds(), 500);
	m_scenarioUseTimer.Set(fwTimer::GetTimeInMilliseconds(), fwRandom::GetRandomNumberInRange(3000, 6000));

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskScenarioFlee::UseScenario_OnUpdate()
{
	// Once the use task is finished try to find a new scenario
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		UnreserveScenarioPoint();
		SetState(State_FindScenario);
	}

	// if our distance check timer has ran out we can check our distance to the source entity
	if(m_sourceDistanceTimer.IsSet() && m_sourceDistanceTimer.IsOutOfTime())
	{
		CPed* pPed = GetPed();
		Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		float fDistToEntitySq = vPedPosition.Dist2(VEC3V_TO_VECTOR3(m_pEventSourceEntity->GetTransform().GetPosition()));
		// if we are far enough away and we've been here long enough we can finish
		if(fDistToEntitySq > (sm_Tunables.m_fFleeRange * sm_Tunables.m_fFleeRange))
		{
			if(m_scenarioUseTimer.IsSet() && m_scenarioUseTimer.IsOutOfTime())
			{						
				SetState(State_Finish);
			}
		}
		else
		{
			// if we are not far enough away to end the task then check if the source entity is close
			// enough (against the flee scenario safe distance) that we will want to re-evaluate our position and state
			const CScenarioInfo* pScenarioInfo = CScenarioManager::GetScenarioInfo(m_ScenarioTypeReal);
			if(pScenarioInfo && pScenarioInfo->GetIsClass<CScenarioFleeInfo>())
			{
				const CScenarioFleeInfo* pScenarioFleeInfo = static_cast<const CScenarioFleeInfo*>(pScenarioInfo);
				if(fDistToEntitySq < pScenarioFleeInfo->GetSafeRadius() * pScenarioFleeInfo->GetSafeRadius())
				{
					SetNewTask(NULL);
					UnreserveScenarioPoint();
					SetState(State_FindScenario);
				}
			}
		}

		m_sourceDistanceTimer.Set(fwTimer::GetTimeInMilliseconds(), 500);
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskScenarioFlee::Flee_OnEnter()
{
	// set the secnario search timer so that we search for a flee scenario every 1/4 second
	m_scenarioSearchTimer.Set(fwTimer::GetTimeInMilliseconds(), 250);

	// Give the smart flee task and tell it to quit once the ped is out of range of the event source entity

	CAITarget fleeTarget;

	if (m_bAboutToCollide)
	{
		// Flee the wall - the player is added from the spheres of influence builder.
		fleeTarget.SetPosition(RCC_VECTOR3(m_vCollisionPoint));

		m_bAlteredPathForCollision = true;
	}
	else
	{
		fleeTarget.SetEntity(m_pEventSourceEntity);
	}

	CPed* pPed = GetPed();

	// JR - bit of a hack:
	// Force the ped to flee for a longer distance if its an explosion or a gunshot.
	float fFleeRange = sm_Tunables.m_fFleeRange;
	s32 iEventType = pPed->GetPedIntelligence()->GetCurrentEventType();
	if (iEventType == EVENT_EXPLOSION || iEventType == EVENT_SHOT_FIRED || iEventType == EVENT_SHOCKING_EXPLOSION)
	{
		fFleeRange = sm_Tunables.m_fFleeRangeExtended;
	}

	CTaskSmartFlee* pFleeTask = rage_new CTaskSmartFlee(fleeTarget, fFleeRange);
	pFleeTask->SetFleeFlags( m_FleeFlags );
	pFleeTask->SetQuitTaskIfOutOfRange(true);

	// If the ped can enter water but doesn't prefer to avoid it, and is in water, then the path should stay in water.
	CPedNavCapabilityInfo& navCapabilities = pPed->GetPedIntelligence()->GetNavCapabilities();
	if (pPed->GetIsInWater() || (pPed->GetIsSwimming() && 
		(navCapabilities.IsFlagSet(CPedNavCapabilityInfo::FLAG_MAY_ENTER_WATER) && !CPedNavCapabilityInfo::FLAG_PREFER_TO_AVOID_WATER)))
	{
		pFleeTask->GetConfigFlags().SetFlag(CTaskSmartFlee::CF_NeverLeaveWater | CTaskSmartFlee::CF_FleeAtCurrentHeight | CTaskSmartFlee::CF_NeverLeaveDeepWater);
		pFleeTask->SetKeepMovingWhilstWaitingForFleePath(true);
	}

	SetNewTask(pFleeTask);

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskScenarioFlee::Flee_OnUpdate()
{
	// if our scenario search timer is up we can look for a new flee scenario
	if(m_scenarioSearchTimer.IsSet() && m_scenarioSearchTimer.IsOutOfTime())
	{
		FindNewFleeScenario();
		if(m_pScenarioPoint)
		{
			m_pScenarioPoint->AddUse();
			SetState(State_MoveToScenario);
		}
		else
		{
			m_scenarioSearchTimer.Set(fwTimer::GetTimeInMilliseconds(), 250);
		}
	}

	// If the flee task finishes it means is was successful so quit this task
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Finish);
	}

	// Switch how we are fleeing if about to collide with map geometry.
	if (m_bAboutToCollide)
	{
		if (!m_bAlteredPathForCollision)
		{
			SetFlag(aiTaskFlags::RestartCurrentState);
		}
		else
		{
			CPed* pPed = GetPed();
			// Apply extra steering to help avoid the wall.
			if (!pPed->GetUsingRagdoll())
			{
				float fCurrentHeading = pPed->GetCurrentHeading();
				float fDesiredHeading = pPed->GetDesiredHeading();
				float fDelta = CTaskMotionBase::InterpValue(fCurrentHeading, fDesiredHeading, 1.0f, true, GetTimeStep());

				pPed->GetMotionData()->SetExtraHeadingChangeThisFrame(fDelta);
			}
		}
		return FSM_Continue;
	}

	return FSM_Continue;
}

bool CTaskScenarioFlee::ShouldProbeForCollisions() const
{
	const CPed* pPed = GetPed();

	if (!pPed->GetTaskData().GetIsFlagSet(CTaskFlags::ProbeForCollisionsInScenarioFlee))
	{
		return false;
	}

	// If you already hit something don't probe again.
	if (m_bAboutToCollide)
	{
		return false;
	}

	return true;
}

void CTaskScenarioFlee::ProbeForCollisions()
{
	// Only check every ~250ms to cut down on performance cost.
	if (m_ProbeTimer.IsOutOfTime())
	{
		CPed* pPed = GetPed();
		if (m_CapsuleResult.GetResultsStatus() == WorldProbe::TEST_NOT_SUBMITTED)
		{
			// Cast a new probe in the direction of the ped.
			Vec3V vStart = pPed->GetTransform().GetPosition();
			Vec3V vEnd = vStart + (pPed->GetTransform().GetForward() * ScalarV(sm_Tunables.m_fProbeLength));
			float fRadius = pPed->GetCapsuleInfo()->GetProbeRadius();

			WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
			capsuleDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_BOX_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE);
			capsuleDesc.SetExcludeTypeFlags(ArchetypeFlags::GTA_PICKUP_TYPE);
			capsuleDesc.SetResultsStructure(&m_CapsuleResult);
			capsuleDesc.SetMaxNumResultsToUse(1);
			capsuleDesc.SetContext(WorldProbe::EMovementAI);
			capsuleDesc.SetIsDirected(true);
			capsuleDesc.SetCapsule(VEC3V_TO_VECTOR3(vStart), VEC3V_TO_VECTOR3(vEnd), fRadius);
			capsuleDesc.SetDoInitialSphereCheck(true);
			WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc, WorldProbe::PERFORM_ASYNCHRONOUS_TEST);

#if __BANK
			if (sm_bRenderProbeChecks) 
			{
				grcDebugDraw::Capsule(vStart, vEnd, fRadius, Color_white, false, -30);
			}
#endif
		}
		else
		{
			// Check the probe
			bool bReady = m_CapsuleResult.GetResultsReady();
			if (bReady)
			{
				// Probe is done calculating
				if (m_CapsuleResult.GetNumHits() > 0)
				{
					// Note the collision.
					m_bAboutToCollide = true;
					m_vCollisionPoint = m_CapsuleResult[0].GetHitPositionV();

#if __BANK
					if (sm_bRenderProbeChecks)
					{
						grcDebugDraw::Sphere(RCC_VECTOR3(m_vCollisionPoint), 0.5f, Color_red, true, -30);
					}
#endif
				}
				m_ProbeTimer.Set(fwTimer::GetTimeInMilliseconds(), sm_Tunables.m_uAvoidanceProbeInterval);
				m_CapsuleResult.Reset();
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////

#if !__FINAL
const char * CTaskScenarioFlee::GetStaticStateName( s32 iState )
{
	// Make sure FSM state is valid
	Assert(iState>=0&&iState<=State_Finish);
	static const char* aScenarioFleeStateNames[] = 
	{
		"State_FindScenario",
		"State_MoveToScenario",
		"State_UseScenario",
		"State_Flee",
		"State_Finish"
	};

	return aScenarioFleeStateNames[iState];
}
#endif

//////////////////////////////////////////////////////////////////////////
CTaskInfo* CTaskScenarioFlee::CreateQueriableState() const
{
	CTaskInfo* pInfo = NULL;

	pInfo = rage_new CClonedControlTaskScenarioFleeInfo(GetState(), m_pEventSourceEntity );

	return pInfo;
}

void CTaskScenarioFlee::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	taskAssert(pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_SCENARIO_FLEE);
	CClonedControlTaskScenarioFleeInfo *scenarioFleeInfo = static_cast<CClonedControlTaskScenarioFleeInfo*>(pTaskInfo);

	m_pEventSourceEntity		= scenarioFleeInfo->GetEventsSourceEntity();

	m_FleeFlags					= (u8)scenarioFleeInfo->GetFleeFlags();

	// Call the base implementation - syncs the state
	CTaskFSMClone::ReadQueriableState(pTaskInfo);
}

CTaskFSMClone *CTaskScenarioFlee::CreateTaskForClonePed(CPed *)
{
	CTaskScenarioFlee* pTask = NULL;

	pTask = rage_new CTaskScenarioFlee(m_pEventSourceEntity, m_FleeFlags);

	return pTask;
}

CTaskFSMClone *CTaskScenarioFlee::CreateTaskForLocalPed(CPed *pPed)
{
	// do the same as clone ped
	return CreateTaskForClonePed(pPed);
}


void CTaskScenarioFlee::OnCloneTaskNoLongerRunningOnOwner()
{
	SetStateFromNetwork(CTaskScenarioFlee::State_Finish);
}

bool CTaskScenarioFlee::StateChangeHandledByCloneFSM(s32 iState)
{
	switch(iState)
	{
	case State_Finish:
		return true;

	default:
		return false;
	}

}

//////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskScenarioFlee::UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent)
{
	if(!StateChangeHandledByCloneFSM(iState))
	{
		if(iEvent == OnUpdate)
		{
			if(GetStateFromNetwork() != -1 && GetStateFromNetwork() != GetState())
			{
				SetState(GetStateFromNetwork());
			}
		}
	}

	FSM_Begin
		FSM_State(State_FindScenario)
		FSM_State(State_MoveToScenario)
		FSM_State(State_UseScenario)
		FSM_State(State_Flee)
		FSM_State(State_Finish)
			FSM_OnUpdate
			return FSM_Quit;
	FSM_End
}
//-------------------------------------------------------------------------
// Task info for Sceanario Flee
//-------------------------------------------------------------------------
CClonedControlTaskScenarioFleeInfo::CClonedControlTaskScenarioFleeInfo(s32 scenarioFleeState, CEntity* pSourceEntity )
: m_EventSourceEntity(pSourceEntity)
{
	SetStatusFromMainTaskState(scenarioFleeState);
}

CClonedControlTaskScenarioFleeInfo::CClonedControlTaskScenarioFleeInfo()
{
}

CTaskFSMClone * CClonedControlTaskScenarioFleeInfo::CreateCloneFSMTask()
{
	CTaskScenarioFlee* pScenarioFleeTask = NULL;

	pScenarioFleeTask = rage_new CTaskScenarioFlee(m_EventSourceEntity.GetEntity(), 0);  //don't care about any of the initial values since clone will follow remote states

	return pScenarioFleeTask;
}




//////////////////////////////////////////////////////////////////////////
// Fly Away (Flee-by-flying)
//////////////////////////////////////////////////////////////////////////

CTaskFlyAway::CTaskFlyAway(CEntity* pSourceEntity) :
	m_pEventSourceEntity	(pSourceEntity),
	m_vFleeDirection		(V_ZERO)
{
	SetInternalTaskType(CTaskTypes::TASK_FLY_AWAY);
}

//////////////////////////////////////////////////////////////////////////

CTaskFlyAway::~CTaskFlyAway()
{
}

//////////////////////////////////////////////////////////////////////////

void CTaskFlyAway::DoAbort(const AbortPriority UNUSED_PARAM(priority), const aiEvent* UNUSED_PARAM(pEvent))
{
}

//////////////////////////////////////////////////////////////////////////

CTaskFlyAway::FSM_Return CTaskFlyAway::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		FSM_State(State_Check)
			FSM_OnUpdate
				return Check_OnUpdate();
		FSM_State(State_Flee)
			FSM_OnEnter
				Flee_OnEnter();
			FSM_OnUpdate
				return Flee_OnUpdate();
		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskFlyAway::Check_OnUpdate()
{
	if (Verifyf(m_pEventSourceEntity, "Expected a source entity."))
	{
		// Calculate an ideal flight plan.
		CPed *pPed = GetPed();
		Vec3V vPedPosition = pPed->GetTransform().GetPosition();
		Vec3V vThreatPosition = m_pEventSourceEntity->GetTransform().GetPosition();
		Vec3V vFleeDir = vPedPosition - vThreatPosition;
		vFleeDir.SetZ(MagXY(vFleeDir));
		vFleeDir = NormalizeFastSafe(vFleeDir, Vec3V(V_ZERO));

		// Minor optimization:  only consider reversing the direction if the bird is on the ground.
		if (!pPed->GetMotionData()->GetIsFlying())
		{
			// Hopefully that direction works out for the bird, but if not then flip it.
			// This is now *towards* the threat but that's probably free of any obstacles (because the player just came from that direction).
			if (IsDirectionBlocked(vPedPosition, vFleeDir))
			{
				vFleeDir.SetX(vFleeDir.GetX() * ScalarV(-1.0f));
				vFleeDir.SetY(vFleeDir.GetY() * ScalarV(-1.0f));
			}
		}

		// Store the flee vector for later.
		m_vFleeDirection = vFleeDir;
	}

	SetState(State_Flee);
	return FSM_Continue;
}

void CTaskFlyAway::Flee_OnEnter()
{
	// Move far away along the previously calculated flee vector.
	CPed *pPed = GetPed();
	Vec3V vPedPosition = pPed->GetTransform().GetPosition();
	Vec3V vTargetPoint = vPedPosition + (m_vFleeDirection * ScalarV(1000.0f));

	float fTargetRadius = 10.0f;
	CTaskFlyToPoint* pFlyToTask = rage_new CTaskFlyToPoint(MOVEBLENDRATIO_SPRINT, VEC3V_TO_VECTOR3(vTargetPoint), fTargetRadius, true);

	CTask *pTaskToAdd = rage_new CTaskComplexControlMovement(pFlyToTask);
	SetNewTask(pTaskToAdd);
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskFlyAway::Flee_OnUpdate()
{
	// If the fly to point subtask ends start up a new one.
	if (GetIsSubtaskFinished(CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT))
	{
		SetFlag(aiTaskFlags::RestartCurrentState);
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////


bool CTaskFlyAway::IsDirectionBlocked(Vec3V_In vPos, Vec3V_In vDir)
{
	const float fCapsuleRadius = 0.4f;
	const float fCapsuleZOffset = 0.5f;
	const float fCapsuleLength = 5.0f;
	WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
	WorldProbe::CShapeTestFixedResults<> probeResults;
	Vec3V vStart = vPos;

	// Raise it up off the ground some.
	vStart.SetZ(vStart.GetZ() + ScalarV(fCapsuleZOffset));
	Vec3V vEnd = vDir * ScalarV(fCapsuleLength);
	vEnd += vStart;
	capsuleDesc.SetCapsule(VEC3V_TO_VECTOR3(vStart), VEC3V_TO_VECTOR3(vEnd), fCapsuleRadius);
	capsuleDesc.SetResultsStructure(&probeResults);
	capsuleDesc.SetIsDirected(true);
	capsuleDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES | ArchetypeFlags::GTA_OBJECT_TYPE | ArchetypeFlags::GTA_VEHICLE_TYPE);
	capsuleDesc.SetContext(WorldProbe::EMovementAI);

	return WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST);
}

#if !__FINAL
const char * CTaskFlyAway::GetStaticStateName( s32 iState )
{
	// Make sure FSM state is valid
	Assert(iState>=0&&iState<=State_Finish);
	static const char* aScenarioFleeStateNames[] = 
	{
		"State_Check",
		"State_Flee",
		"State_Finish"
	};

	return aScenarioFleeStateNames[iState];
}
#endif

//////////////////////////////////////////////////////////////////////////
CTaskInfo* CTaskFlyAway::CreateQueriableState() const
{
	CTaskInfo* pInfo = NULL;

	pInfo = rage_new CClonedControlTaskFlyAwayInfo(GetState(), m_pEventSourceEntity);

	return pInfo;
}

void CTaskFlyAway::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	taskAssert(pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_FLY_AWAY);
	CClonedControlTaskFlyAwayInfo *flyAwayInfo = static_cast<CClonedControlTaskFlyAwayInfo*>(pTaskInfo);
	
	m_pEventSourceEntity		= flyAwayInfo->GetEventsSourceEntity();

	// Call the base implementation - syncs the state
	CTaskFSMClone::ReadQueriableState(pTaskInfo);
}

CTaskFSMClone *CTaskFlyAway::CreateTaskForClonePed(CPed *)
{
	CTaskFlyAway* pTask = NULL;
	pTask = rage_new CTaskFlyAway(m_pEventSourceEntity);
	
	pTask->SetState(GetState());

	//if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
	//{
	//	CTaskComplexControlMovement* pComplexControlMovement = static_cast<CTaskComplexControlMovement*>(GetSubTask());

	//	if(pComplexControlMovement && pComplexControlMovement->GetMoveTaskType() == CTaskTypes::TASK_FLY_TO_POINT)
	//	{
	//		CTaskFlyToPoint *pFlyToTaskToCopy = static_cast<CTaskFlyToPoint*>(pComplexControlMovement->GetBackupCopyOfMovementSubtask());

	//		CTaskFlyToPoint *pFlyToTask = static_cast<CTaskFlyToPoint*>(pFlyToTaskToCopy->Copy());

	//		CTask *pTaskToAdd = rage_new CTaskComplexControlMovement(pFlyToTask);

	//		pTask->SetNewTask(pTaskToAdd);
	//	}
	//}

	return pTask;
}

CTaskFSMClone *CTaskFlyAway::CreateTaskForLocalPed(CPed *pPed)
{
	// do the same as clone ped
	return CreateTaskForClonePed(pPed);
}

void CTaskFlyAway::OnCloneTaskNoLongerRunningOnOwner()
{
	SetStateFromNetwork(CTaskFlyAway::State_Finish);
}

// Is the specified state handled by the clone FSM
bool CTaskFlyAway::StateChangeHandledByCloneFSM(s32 iState)
{
	switch(iState)
	{
	case State_Finish:
		return true;

	default:
		return false;
	}

}

//////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskFlyAway::UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent)
{
	if(!StateChangeHandledByCloneFSM(iState))
	{
		if(iEvent == OnUpdate)
		{
			if(GetStateFromNetwork() != -1 && GetStateFromNetwork() != GetState())
			{
				SetState(GetStateFromNetwork());
			}
		}
	}

	FSM_Begin
		FSM_State(State_Flee)
			//FSM_OnEnter
			//return Flee_OnEnter();
		FSM_State(State_Finish)
			FSM_OnUpdate
			return FSM_Quit;
	FSM_End
}


//-------------------------------------------------------------------------
// Task info for Fly Away
//-------------------------------------------------------------------------
CClonedControlTaskFlyAwayInfo::CClonedControlTaskFlyAwayInfo(s32 flyAwayState, CEntity* pSourceEntity)
: m_EventSourceEntity(pSourceEntity)
{
	SetStatusFromMainTaskState(flyAwayState);
}

CClonedControlTaskFlyAwayInfo::CClonedControlTaskFlyAwayInfo()
{
}

CTaskFSMClone * CClonedControlTaskFlyAwayInfo::CreateCloneFSMTask()
{
	CTaskFlyAway* pFlyAwayTask = NULL;

	pFlyAwayTask = rage_new CTaskFlyAway(m_EventSourceEntity.GetEntity());  //don't care about the initial value since clone will follow remote states

	return pFlyAwayTask;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskExhausedFlee
// - A version of smart flee that prevents the ped from maintaining a sprint for very long.
//////////////////////////////////////////////////////////////////////////////////////////////////////////////

CTaskExhaustedFlee::Tunables CTaskExhaustedFlee::sm_Tunables;
IMPLEMENT_RESPONSE_TASK_TUNABLES(CTaskExhaustedFlee, 0x0f66b7f0);

CTaskExhaustedFlee::CTaskExhaustedFlee(CEntity* pSourceEntity)
: m_pEventSourceEntity(pSourceEntity)
, m_fEnergy(sm_Tunables.m_StartingEnergy)
, m_bLimitingMBRBecauseOfNM(false)
{
	SetInternalTaskType(CTaskTypes::TASK_EXHAUSTED_FLEE);
}

CTaskExhaustedFlee::~CTaskExhaustedFlee()
{

}

CTask::FSM_Return CTaskExhaustedFlee::ProcessPreFSM()
{
	// Target no longer exists, so quit.
	if(!m_pEventSourceEntity)
	{
		return FSM_Quit;
	}

	return FSM_Continue;
}

// FSM required functions
CTask::FSM_Return CTaskExhaustedFlee::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		FSM_State(State_Flee)
			FSM_OnEnter
				Flee_OnEnter();
			FSM_OnUpdate
				return Flee_OnUpdate();
		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

#if !__FINAL
const char * CTaskExhaustedFlee::GetStaticStateName( s32 iState )
{
	taskAssert(iState >= State_Flee && iState <= State_Finish);

	switch (iState)
	{
		case State_Flee:				return "State_Flee";			
		case State_Finish:				return "State_Finish";
		default: taskAssert(0); 	
	}

	return "State_Invalid";
}
#endif // !__FINAL


void CTaskExhaustedFlee::Flee_OnEnter()
{
	CTaskSmartFlee* pFleeTask = rage_new CTaskSmartFlee(CAITarget(m_pEventSourceEntity));

	SetNewTask(pFleeTask);
}

CTask::FSM_Return CTaskExhaustedFlee::Flee_OnUpdate()
{
	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Finish);
	}
	else
	{
		CTask* pSubTask = GetSubTask();
		if (Verifyf(pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_SMART_FLEE, "Unexpected subtask in CTaskExhaustedFlee"))
		{
			CPed* pPed = GetPed();
			float fMbr = pPed->GetMotionData()->GetCurrentMbrY();
			bool bCloseEnoughForSprint = false;
			bool bAlwaysSprint = false;
			CTaskSmartFlee* pFleeTask = static_cast<CTaskSmartFlee*>(pSubTask);

			// Force peds using this task to flee by walking once they have fallen over in certain cases.
			// Skip the rest of the logic below.
			if (ShouldLimitMBRBecauseofNM())
			{
				m_bLimitingMBRBecauseOfNM = true;
				pFleeTask->SetMoveBlendRatio(MOVEBLENDRATIO_WALK);
				return FSM_Continue;
			}

			ScalarV distance = DistSquared(pPed->GetTransform().GetPosition(), m_pEventSourceEntity->GetTransform().GetPosition());

			float fInnerDistanceThreshold = sm_Tunables.m_InnerDistanceThreshold;
			float fOuterDistanceThreshold = sm_Tunables.m_OuterDistanceThreshold;

			if (fMbr == MOVEBLENDRATIO_SPRINT)
			{
				// Don't flop back to run so easily.
				static const float sf_Delta = 5.0f;
				fInnerDistanceThreshold += sf_Delta;
			}

			if (IsLessThanAll(distance, ScalarV(fInnerDistanceThreshold * fInnerDistanceThreshold)))
			{
				bCloseEnoughForSprint = true;
				bAlwaysSprint = true;
			}
			else if (IsLessThanAll(distance, ScalarV(fOuterDistanceThreshold * fOuterDistanceThreshold)))
			{
				bCloseEnoughForSprint = true;
			}

			if (fMbr == MOVEBLENDRATIO_SPRINT)
			{
				// Sprinting, so decrease energy.
				m_fEnergy = Max(0.0f, m_fEnergy - (GetTimeStep() * sm_Tunables.m_EnergyLostPerSecond));

				if (!bAlwaysSprint && (!bCloseEnoughForSprint || m_fEnergy <= 0.0f))
				{
					// Too tired, drop down to a run.
					pFleeTask->SetMoveBlendRatio(MOVEBLENDRATIO_RUN);
				}
			}
			else
			{
				if (bAlwaysSprint)
				{
					// Inside the inner band so always sprint.
					pFleeTask->SetMoveBlendRatio(MOVEBLENDRATIO_SPRINT);
				}
				else if (bCloseEnoughForSprint && m_fEnergy > 0.0f)
				{
					// Have energy and are close enough to sprint, so do so.
					pFleeTask->SetMoveBlendRatio(MOVEBLENDRATIO_SPRINT);
				}
			}
		}
	}
	return FSM_Continue;
}

bool CTaskExhaustedFlee::ShouldLimitMBRBecauseofNM()
{
	// If we limited the MBR in the past continue to do so now.
	if (m_bLimitingMBRBecauseOfNM)
	{
		return true;
	}

	CPed* pPed = GetPed();
	
	// Only if the ped has gone into NM.
	if (!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_HasPlayedNMGetup))
	{
		return false;
	}

	// Only if the current event exists and is a damage event.
	CEvent* pEvent = pPed->GetPedIntelligence()->GetCurrentEvent();
	if (!pEvent || pEvent->GetEventType() != EVENT_DAMAGE)
	{
		return false;
	}

	// Only if the damage event was caused by a vehicle.
	CEventDamage* pDamageEvent = static_cast<CEventDamage*>(pEvent);
	if (pDamageEvent->GetWeaponUsedHash() == WEAPONTYPE_RAMMEDBYVEHICLE || pDamageEvent->GetWeaponUsedHash() == WEAPONTYPE_RUNOVERBYVEHICLE)
	{
		return true;
	}
	else
	{
		return false;
	}
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskWalkAway
//////////////////////////////////////////////////////////////////////////////////////////////////////////////

CTaskWalkAway::Tunables CTaskWalkAway::sm_Tunables;
IMPLEMENT_RESPONSE_TASK_TUNABLES(CTaskWalkAway, 0xa80a65ea);

CTaskWalkAway::CTaskWalkAway(CEntity* pSourceEntity)
: m_pEventSourceEntity(pSourceEntity)
, m_fRouteTimer(0.0f)
{
	SetInternalTaskType(CTaskTypes::TASK_WALK_AWAY);
}

CTaskWalkAway::~CTaskWalkAway()
{

}

CTask::FSM_Return CTaskWalkAway::ProcessPreFSM()
{
	// Target no longer exists, so quit.
	if(!m_pEventSourceEntity)
	{
		return FSM_Quit;
	}

	m_fRouteTimer += GetTimeStep();

	return FSM_Continue;
}

// FSM required functions
CTask::FSM_Return CTaskWalkAway::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		FSM_State(State_Flee)
			FSM_OnEnter
				Flee_OnEnter();
			FSM_OnUpdate
				return Flee_OnUpdate();
		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

#if !__FINAL
const char * CTaskWalkAway::GetStaticStateName( s32 iState )
{
	taskAssert(iState >= State_Flee && iState <= State_Finish);

	switch (iState)
	{
	case State_Flee:				return "State_Flee";			
	case State_Finish:				return "State_Finish";
	default: taskAssert(0); 	
	}

	return "State_Invalid";
}
#endif // !__FINAL


void CTaskWalkAway::Flee_OnEnter()
{
	CPed* pPed = GetPed();

	Vector3 vPosition = VEC3V_TO_VECTOR3(m_pEventSourceEntity->GetTransform().GetPosition());

	CTaskMoveFollowNavMesh* pNavMeshTask = rage_new CTaskMoveFollowNavMesh(MOVEBLENDRATIO_WALK, vPosition, CTaskMoveFollowNavMesh::ms_fTargetRadius, CTaskMoveFollowNavMesh::ms_fTargetRadius, -1, true, true);
	
	pNavMeshTask->SetFleeSafeDist(sm_Tunables.m_SafeDistance);
	if (pPed && pPed->GetPedIntelligence() && pPed->GetPedIntelligence()->GetFleeBehaviour().GetFleeFlags().IsFlagSet(CFleeBehaviour::BF_PreferPavements))
	{
		pNavMeshTask->SetKeepToPavements(true);
		pNavMeshTask->SetConsiderRunStartForPathLookAhead(true);
	}

	// Create the control movement task and set it as the active sub task
	SetNewTask(rage_new CTaskComplexControlMovement(pNavMeshTask));
}

CTask::FSM_Return CTaskWalkAway::Flee_OnUpdate()
{
	CTask* pSubtask = GetSubTask();
	if (!pSubtask || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Finish);
	}
	else if (m_fRouteTimer >= sm_Tunables.m_TimeBetweenRouteAdjustments && pSubtask->GetTaskType() == CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
	{
		// Update the position.
		CTaskComplexControlMovement* pControlMovement = static_cast<CTaskComplexControlMovement*>(pSubtask);
		Vector3 vPosition = VEC3V_TO_VECTOR3(m_pEventSourceEntity->GetTransform().GetPosition());
		pControlMovement->SetTarget(GetPed(), vPosition);
		m_fRouteTimer = 0.0f;
	}

	return FSM_Continue;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////


#if __BANK
bool CFleeDebug::ms_bLookForCarsToSteal		= false;
bool CFleeDebug::ms_bLookForCover			= true;
bool CFleeDebug::ms_bForceMissionCarStealing = false;
bool CFleeDebug::ms_bDisplayFleeingOutOfPavementManagerDebug = false;

void CFleeDebug::InitWidgets()
{
	bkBank* pBank = BANKMGR.FindBank("A.I.");

	pBank->PushGroup("Fleeing Behaviour", false);
		pBank->AddToggle("Look for cover",					&ms_bLookForCover);
		pBank->AddToggle("Look for cars to steal",			&ms_bLookForCarsToSteal);
		pBank->AddToggle("Force mission car stealing",		&ms_bForceMissionCarStealing);
		pBank->AddSlider("Look for cars time",				&CTaskSmartFlee::ms_uiVehicleCheckPeriod, 2000, 10000, 1000);
		pBank->AddSlider("Stop distance",					&CTaskSmartFlee::ms_fStopDistance, -1.0f, 999.0f, 1.0f);
		pBank->AddSlider("Min panic audio repeat time",		&CTaskSmartFlee::ms_iMinPanicAudioRepeatTime, 0, 20000, 100);
		pBank->AddSlider("Max panic audio repeat time",		&CTaskSmartFlee::ms_iMaxPanicAudioRepeatTime, 0, 20000, 100);
		pBank->AddToggle("Display fleeing out of pavement manager debug", &ms_bDisplayFleeingOutOfPavementManagerDebug);
	pBank->PopGroup();

}

void CFleeDebug::Render()
{
	if (ms_bDisplayFleeingOutOfPavementManagerDebug)
	{
		CTaskSmartFlee::DEBUG_RenderFleeingOutOfPavementManager();
	}
}

#endif // __BANK
