//
// task/taskwander.cpp
//
// Copyright (C) 1999-2010 Rockstar North.  All Rights Reserved. 
//

// File header
#include "Task/Default/TaskWander.h"

// Framework headers
#include "grcore/debugdraw.h"

// Game headers
#include "Event/EventShocking.h"
#include "Event/ShockingEvents.h"
#include "Game/ModelIndices.h"
#include "Game/Weather.h"
#include "Peds/PedDebugVisualiser.h"
#include "Peds/PedIntelligence.h"
#include "Peds/Ped.h"
#include "Peds/PopZones.h"
#include "Scene/World/GameWorld.h"
#include "Task/Crimes/RandomEventManager.h"
#include "Task/Crimes/TaskStealVehicle.h"
#include "Task/General/TaskBasic.h"
#include "Task/Movement/TaskMoveWander.h"
#include "Task/Movement/TaskNavMesh.h"
#include "Task/Movement/TaskCollisionResponse.h"
#include "Task/Scenario/Info/ScenarioInfoManager.h"
#include "Task/Scenario/Types/TaskUseScenario.h"
#include "Task/Vehicle/TaskExitVehicle.h"
#include "task/Weapons/TaskSwapWeapon.h"

AI_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////

// Constants
const atHashWithStringNotFinal CTaskWander::ms_waitToCrossScenario("CODE_HUMAN_CROSS_ROAD_WAIT",0xFF6241F7);
s32				CTaskWander::ms_ScenarioIndexWaitingToCross = -1;
dev_float		CTaskWander::TARGET_RADIUS = 0.5f;
dev_float		CTaskWander::ACCEPT_ALL_DOT_MIN = -1.0f;
dev_float		CTaskWander::DEFAULT_ROAD_CROSS_PED_TO_START_DOT_MIN = 0.707f;
dev_float		CTaskWander::DEFAULT_DIST_SQ_PED_TO_START_MAX = 8.0f * 8.0f;
dev_float		CTaskWander::INCREASED_ROAD_CROSS_PED_TO_START_DOT_MIN = 0.5f;
dev_float		CTaskWander::INCREASED_DIST_SQ_PED_TO_START_MAX = 16.0f * 16.0f;
dev_float		CTaskWander::DOUBLEBACK_ROAD_CROSS_PED_TO_START_DOT_MIN = -0.70f;
dev_float		CTaskWander::DOUBLEBACK_DIST_SQ_PED_TO_START_MAX = 24.0f * 24.0f;
dev_float		CTaskWander::CHAIN_ROAD_CROSS_PED_TO_START_DOT_MIN = -0.25f;
dev_float		CTaskWander::CHAIN_ROAD_CROSSDIR_TO_CROSSDIR_DOT_MIN = 0.25f;
dev_float		CTaskWander::CHAIN_DIST_SQ_PED_TO_START_MAX = 16.0f * 16.0f;
dev_float		CTaskWander::POST_CROSSING_REPEAT_CHANCE = 0.20f;
dev_float		CTaskWander::IDLE_AT_CROSSWALK_CHANCE = 0.70f;

#if FIND_SHELTER
s32				CTaskWander::ms_iNumShelteringPeds = 0;
#endif //FIND_SHELTER
u32				CTaskWander::ms_uNumRainTransitioningPeds = 0;
u32				CTaskWander::ms_uCurrentRainTransitionPeriodStartTime = 0;

bank_bool		CTaskWander::ms_bPreventCrossingRoads = false;
bank_bool		CTaskWander::ms_bPromoteCrossingRoads = false;
bank_bool		CTaskWander::ms_bPreventJayWalking = true;
bank_bool		CTaskWander::ms_bEnableChainCrossing = true;
bank_bool		CTaskWander::ms_bEnablePavementArrivalCheck = true;
bank_bool		CTaskWander::ms_bEnableWanderFailCrossing = true;
bank_bool		CTaskWander::ms_bEnableArriveNoPavementCrossing = true;

static dev_float fRandomRange = 0.3f;
static dev_float fCentre = 0.15f;
static s32 SCAN_FOR_NEARBY_PEDS_FREQ = 1500;

////////////////////////////////////////////////////////////////////////////////

CTaskWander::Tunables CTaskWander::sm_Tunables;
IMPLEMENT_DEFAULT_TASK_TUNABLES(CTaskWander, 0xfc4b30ad);

CTaskWander::CTaskWander(const float fMoveBlendRatio, const float fDir, const CConditionalAnimsGroup * pOverrideClips, const s32 pOverrideAnimIndex, const bool bWanderSensibly, const float fTargetRadius, const bool bStayOnPavements, bool bScanForPavements)
: m_iOverrideAnimIndex(pOverrideAnimIndex)
, m_bRemoveIfInBadPavementSituation(bScanForPavements)
, m_bLacksPavement(false)
, m_bZoneRequiresPavement(false)
{
	Init(fMoveBlendRatio, fDir, pOverrideClips, bWanderSensibly, fTargetRadius, bStayOnPavements);
	SetInternalTaskType(CTaskTypes::TASK_WANDER);
}

////////////////////////////////////////////////////////////////////////////////

CTaskWander::~CTaskWander()
{
#if FIND_SHELTER
	taskAssert(m_hFindShelterFloodFillPathHandle == PATH_HANDLE_NULL);
#endif //FIND_SHELTER
}

aiTask* CTaskWander::Copy() const
{
	CTaskWander * pTask = rage_new CTaskWander(m_fMoveBlendRatio, m_fDirection, m_pOverrideConditionalAnimsGroup, m_iOverrideAnimIndex, m_bWanderSensibly, m_fTargetRadius, m_bStayOnPavements, m_bRemoveIfInBadPavementSituation);
	pTask->m_bKeepMovingWhilstWaitingForFirstPath = m_bKeepMovingWhilstWaitingForFirstPath;
	pTask->m_bCreateProp = m_bCreateProp;
	pTask->m_bForceCreateProp = m_bForceCreateProp;

	return pTask;
}
////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
void CTaskWander::Debug() const
{
	switch(GetState())
	{
	case State_MoveToShelterAndWait:
		{
#if FIND_SHELTER
			DEBUG_DRAW_ONLY(grcDebugDraw::Sphere(m_vShelterPos, 0.1f, Color_orange));
#endif //FIND_SHELTER
		}
		break;

	default:
		break;
	}

	// Base class
	CTask::Debug();
}
#endif // !__FINAL

////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
const char * CTaskWander::GetStaticStateName( s32 iState )
{
	static const char* aStateNames[] = 
	{
		"Init",
		"ExitVehicle",
		"Wander",
		"WanderToStoredPosition",
		"CrossRoadAtLights",
		"CrossRoadJayWalking",
		"MoveToShelterAndWait",
		"Crime",
		"HolsterWeapon",
		"BlockedByVehicle",
		"Aborted",
	};

	return aStateNames[iState];
}
#endif // !__FINAL

////////////////////////////////////////////////////////////////////////////////
void CTaskWander::UpdateClass()
{
	// If we have no rain-transitioning peds, reset our rain process timer [6/4/2013 mdawe]
	if (ms_uNumRainTransitioningPeds == 0)
	{
		ms_uCurrentRainTransitionPeriodStartTime = fwTimer::GetTimeInMilliseconds();
	}
	else if ((ms_uCurrentRainTransitionPeriodStartTime + (sm_Tunables.m_fSecondsInRainTransitionPeriod * 1000.f)) < fwTimer::GetTimeInMilliseconds())
	{
		ms_uNumRainTransitioningPeds = 0;
	}
}


////////////////////////////////////////////////////////////////////////////////

void CTaskWander::SetDirection(const float fHeading)
{
	static dev_float fSameHeading = DtoR * 8.0f;
	const float fNewHeading = fwAngle::LimitRadianAngleSafe(fHeading);
	if(!rage::IsNearZero(m_fDirection - fNewHeading, fSameHeading))
	{
		m_fDirection = fNewHeading;
		m_bRestartWander = true;
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskWander::KeepMovingWhilstWaitingForFirstPath(const CPed* pPed)
{
	if(pPed)
	{
		SetDirection(pPed->GetCurrentHeading());
	}

	m_bKeepMovingWhilstWaitingForFirstPath = true;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskWander::GetDoesPedHaveBrolly(const CPed* UNUSED_PARAM(pPed))
{
	return false;
}

CTaskInfo* CTaskWander::CreateQueriableState() const
{
	float direction = m_fDirection;

	if (direction < 0.0f)
	{
		direction += TWO_PI;
	}
	else if (direction > TWO_PI)
	{
		direction -= TWO_PI;
	}

	u32 uOverrideAnimGroup = m_pOverrideConditionalAnimsGroup ? m_pOverrideConditionalAnimsGroup->GetHash() : 0;

	u8 uOverrideAnimIndex = 0;
	if (taskVerifyf(m_iOverrideAnimIndex >= -1 && m_iOverrideAnimIndex < 255, "m_iOverrideAnimIndex max sync value is 254, change value type if necessary"))
	{
		uOverrideAnimIndex = (u8)(m_iOverrideAnimIndex + 1);
	}

	return rage_new CClonedWanderInfo(m_fMoveBlendRatio, direction, uOverrideAnimGroup, uOverrideAnimIndex, m_bWanderSensibly, m_fTargetRadius, m_bStayOnPavements);
}

////////////////////////////////////////////////////////////////////////////////

CTaskWander::FSM_Return CTaskWander::ProcessPreFSM()
{
	// Cache the ped
	CPed* pPed = GetPed();

	// Allow certain lod modes
	pPed->GetPedAiLod().ClearBlockedLodFlag(CPedAILod::AL_LodPhysics);
	pPed->GetPedAiLod().ClearBlockedLodFlag(CPedAILod::AL_LodMotionTask);

	// Check shocking events
	pPed->GetPedIntelligence()->SetCheckShockFlag(true);

	CTask * pTaskMove = pPed->GetPedIntelligence()->GetActiveMovementTask();
	if(pTaskMove)
	{
		if(pTaskMove->GetTaskType()==CTaskTypes::TASK_MOVE_WANDER_AROUND_VEHICLE)
		{
			m_LastVehicleWalkedAround = ((CTaskWalkRoundCarWhileWandering*)pTaskMove)->GetVehicle();
		}
		else if(pTaskMove->GetTaskType()==CTaskTypes::TASK_MOVE_WALK_ROUND_VEHICLE)
		{
			m_LastVehicleWalkedAround = ((CTaskMoveWalkRoundVehicle*)pTaskMove)->GetVehicle();
		}
	}
	
	// Peds in this task are allowed to play gestures as long as the ped isn't holding a weapon.
	// Ambient props like cell phones are OK.
	const CPedWeaponManager* pWeaponManager = pPed->GetWeaponManager();
	if(!pWeaponManager || !pWeaponManager->GetEquippedWeaponObject() || pWeaponManager->GetEquippedWeaponObject()->m_nObjectFlags.bAmbientProp)
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_GestureAnimsAllowed, true);
	}

	if (m_bStayOnPavements && m_bZoneRequiresPavement && m_bRemoveIfInBadPavementSituation)
	{
		// Continue to scan for pavement.
		ProcessPavementFloodFillRequest();

		// No pavement was found nearby.
		if (m_bLacksPavement)
		{
			if (!pPed->GetRemoveAsSoonAsPossible())
			{
				// Aggressively delete this ped.
				pPed->SetRemoveAsSoonAsPossible(true);
			}
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskWander::FSM_Return CTaskWander::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		FSM_State(State_Init)
			FSM_OnUpdate
				return StateInit();
		FSM_State(State_ExitVehicle)
			FSM_OnEnter
				return StateExitVehicle_OnEnter();
			FSM_OnUpdate
				return StateExitVehicle_OnUpdate();
		FSM_State(State_Wander)
			FSM_OnEnter
				return StateWander_OnEnter();
			FSM_OnUpdate
				return StateWander_OnUpdate();
			FSM_OnExit
				return StateWander_OnExit();
		FSM_State(State_WanderToStoredPosition)
			FSM_OnEnter
				return StateWanderToStoredPosition_OnEnter();
			FSM_OnUpdate
				return StateWanderToStoredPosition_OnUpdate();
		FSM_State(State_CrossRoadAtLights)
			FSM_OnEnter
				return StateCrossRoadAtLights_OnEnter();
			FSM_OnUpdate
				return StateCrossRoadAtLights_OnUpdate();
			FSM_OnExit
				return StateCrossRoadAtLights_OnExit();
		FSM_State(State_CrossRoadJayWalking)
			FSM_OnEnter
				return CrossRoadJayWalking_OnEnter();
			FSM_OnUpdate
				return CrossRoadJayWalking_OnUpdate();
		FSM_State(State_MoveToShelterAndWait)
			FSM_OnEnter
				return StateMoveToShelterAndWait_OnEnter();
			FSM_OnUpdate
				return StateMoveToShelterAndWait_OnUpdate();
		FSM_State(State_Crime)
			FSM_OnUpdate
				return StateCrime_OnUpdate();
		FSM_State(State_HolsterWeapon)
			FSM_OnEnter
				return StateHolsterWeapon_OnEnter();
			FSM_OnUpdate
				return StateHolsterWeapon_OnUpdate();
		FSM_State(State_BlockedByVehicle)
			FSM_OnEnter
				return StateBlockedByVehicle_OnEnter();
			FSM_OnUpdate
				return StateBlockedByVehicle_OnUpdate();
		FSM_State(State_Aborted)
			FSM_OnEnter
				// If this task was aborted, then when reinstated GetWasAborted() will return true for the first frame.
				taskAssertf(GetIsFlagSet(aiTaskFlags::IsAborted), "State [%s] used when task hasn't been aborted", GetStateName(GetState()));
			FSM_OnUpdate
				return StateAborted();
	FSM_End
}


////////////////////////////////////////////////////////////////////////////////

void CTaskWander::DoAbort(const AbortPriority UNUSED_PARAM(priority), const aiEvent* pEvent)
{
	// Cache the ped
	CPed* pPed = GetPed();

	// reset the crosswalk idling flag
	m_bIdlingAtCrossWalk = false;

	// Get the active movement task
	if(GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
	{
		CTask* pMoveTask = static_cast<CTaskComplexControlMovement*>(GetSubTask())->GetRunningMovementTask(pPed);
		if(pMoveTask)
		{
			//*******************************************************************************************
			// For certain events we can force this wander task to make the ped navmesh to their current
			// wander target, and then to continue wandering from there.  This should help prevent those
			// situations where an event has caused the ped to leave a pavement, and then to continue
			// wandering somewhere inappropriate like down the middle of a road.

			switch(GetState())
			{
			case State_WanderToStoredPosition:
				{
					// If we were interrupted whilst already recovering from a previous interruption, then make sure that
					// we continue with this behaviour after this current interruption!
					if(taskVerifyf(pMoveTask->GetTaskType() == CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH, "pMoveTask for State [%s] should always be of type [TASK_MOVE_FOLLOW_NAVMESH] - not [%s]", GetStateName(GetState()), TASKCLASSINFOMGR.GetTaskName(pMoveTask->GetTaskType())))
					{
						m_vContinueWanderingPos = static_cast<CTaskMoveFollowNavMesh*>(pMoveTask)->GetTarget();
						m_bContinueWanderingFromStoredPosition = true;
					}
				}
				break;

			case State_Wander:
			case State_CrossRoadAtLights:
			case State_CrossRoadJayWalking:
				{
					if(GetState() == State_CrossRoadAtLights)
					{
						// If crossing the road, ensure we finish crossing the road
						if(taskVerifyf(pMoveTask->GetTaskType() == CTaskTypes::TASK_MOVE_CROSS_ROAD_AT_TRAFFIC_LIGHTS, "pMoveTask for State [%s] should always be of type [TASK_MOVE_CROSS_ROAD_AT_TRAFFIC_LIGHTS] - not [%s]", GetStateName(GetState()), TASKCLASSINFOMGR.GetTaskName(pMoveTask->GetTaskType())))
						{
							CTaskMoveCrossRoadAtTrafficLights* pMoveCrossRoadTask = static_cast<CTaskMoveCrossRoadAtTrafficLights*>(pMoveTask);
							if(pMoveCrossRoadTask->IsCrossingRoad())
							{
								m_vContinueWanderingPos = pMoveCrossRoadTask->GetEndPos();
								m_bContinueWanderingFromStoredPosition = true;
							}
						}
					}
					else
					{
						if(pEvent && static_cast<const CEvent*>(pEvent)->IsShockingEvent())
						{
							const CEventShocking* pShocking = static_cast<const CEventShocking*>(pEvent);
							CTaskTypes::eTaskType reactTask = pShocking->GetResponseTaskType();
							if(reactTask != CTaskTypes::TASK_SHOCKING_EVENT_HURRYAWAY
									&& reactTask != CTaskTypes::TASK_SMART_FLEE
									&& reactTask != CTaskTypes::TASK_THREAT_RESPONSE)
							{
								// If interrupted by 'goto' shocking event whilst wandering then navmesh back to original wander target before resuming
								if(pMoveTask->GetTaskType() == CTaskTypes::TASK_MOVE_WANDER)
								{
									CTaskMoveWander* pMoveWanderTask = static_cast<CTaskMoveWander*>(pMoveTask);
									if(pMoveWanderTask->GetWanderTarget(m_vContinueWanderingPos))
									{
										m_bContinueWanderingFromStoredPosition = true;
									}
								}
							}
						}
					}
				}
				break;

			default:
				break;
			}
		}
	}
}

bool CTaskWander::ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
	if(GetState() == State_CrossRoadAtLights)
	{
		if(GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
		{
			CTask* pMoveTask = static_cast<CTaskComplexControlMovement*>(GetSubTask())->GetRunningMovementTask(GetPed());
			if(pMoveTask && taskVerifyf(pMoveTask->GetTaskType() == CTaskTypes::TASK_MOVE_CROSS_ROAD_AT_TRAFFIC_LIGHTS, "pMoveTask for State [%s] should always be of type [TASK_MOVE_CROSS_ROAD_AT_TRAFFIC_LIGHTS] - not [%s]", GetStateName(GetState()), TASKCLASSINFOMGR.GetTaskName(pMoveTask->GetTaskType())))
			{
				CTaskMoveCrossRoadAtTrafficLights* pMoveCrossRoadTask = static_cast<CTaskMoveCrossRoadAtTrafficLights*>(pMoveTask);
				if(pMoveCrossRoadTask->IsCrossingRoad())
				{
					if(pEvent && static_cast<const CEvent*>(pEvent)->IsShockingEvent())
					{
						const CEventShocking* pShocking = static_cast<const CEventShocking*>(pEvent);
						CTaskTypes::eTaskType reactTask = pShocking->GetResponseTaskType();
						if(reactTask != CTaskTypes::TASK_SHOCKING_EVENT_HURRYAWAY
							&& reactTask != CTaskTypes::TASK_SMART_FLEE
							&& reactTask != CTaskTypes::TASK_THREAT_RESPONSE)
						{
							return false;
						}
					}
				}
			}
		}
	}

	return CTask::ShouldAbort(iPriority, pEvent);
}

void CTaskWander::CleanUp()
{
	if( m_bMadePedRoadCrossReservation )
	{
		ThePaths.ReleasePedCrossReservation(m_RoadCrossRecord.m_StartNodeAddress, m_RoadCrossRecord.m_EndNodeAddress, m_RoadCrossRecord.m_ReservationSlot);
		m_bMadePedRoadCrossReservation = false;
	}
	if (m_pPropHelper)
	{
		m_pPropHelper->ReleasePropRefIfUnshared(GetPed());

		m_pPropHelper.free();
	}
	CTask::CleanUp();
}

////////////////////////////////////////////////////////////////////////////////

void CTaskWander::Init(const float fMoveBlendRatio, const float fDir, const CConditionalAnimsGroup * pOverrideClips, const bool bWanderSensibly, float fTargetRadius, const bool bStayOnPavements)
{
	m_fMoveBlendRatio = fMoveBlendRatio;
	m_fWanderMoveRate = 1.f;
	m_fDirection = 999.0f;
	m_fTargetRadius = fTargetRadius;
	m_fWanderPathLengthMin = CTaskMoveWander::ms_fMinWanderDist;
	m_fWanderPathLengthMax = CTaskMoveWander::ms_fMaxWanderDist;
#if FIND_SHELTER
	m_hFindShelterFloodFillPathHandle = PATH_HANDLE_NULL;
	m_iLastFindShelterSearchTime = fwTimer::GetTimeInMilliseconds();
	m_vShelterPos.Zero();
#endif //FIND_SHELTER
	m_pOverrideConditionalAnimsGroup = pOverrideClips;
	m_bRestartWander = false;
	m_bWanderSensibly = bWanderSensibly;
	m_bStayOnPavements = bStayOnPavements;
	m_bWaitingForPath = false;
	m_bKeepMovingWhilstWaitingForFirstPath = false;
	m_bContinueWanderingFromStoredPosition = false;
	m_bCreateProp = false;
	m_bForceCreateProp = false;
	m_bHolsterWeapon = false;
	m_bPerformedLowEntityScan = false;
	m_bMadePedRoadCrossReservation = false;
	m_bIdlingAtCrossWalk = false;
	m_bNeedCrossingCheckPathSegDoublesBack = false;
	m_LastVehicleWalkedAround = NULL;
	m_BlockedByVehicle = NULL;
	m_ScanForJayWalkingLocationsTimer.Set(fwTimer::GetTimeInMilliseconds(), fwRandom::GetRandomNumberInRange(0, 30000));
	m_iLastForcedVehiclePotentialCollisionScanTime = 0;
	m_iAnimIndexForExistingProp = -1;

	// Init the direction
	SetDirection(fDir);
}

////////////////////////////////////////////////////////////////////////////////

CTaskWander::FSM_Return CTaskWander::StateInit()
{
	// Cache the ped
	CPed* pPed = GetPed();

	if(pPed->GetVehiclePedInside())
	{
		// Can't wander in a vehicle - get out
		SetState(State_ExitVehicle);
	}
	else
	{
		if(ShouldHolsterWeapon())
		{
			SetState(State_HolsterWeapon);
		}
		else if( pPed->GetPedIntelligence() && pPed->GetPedIntelligence()->ShouldResumeCrossingRoad() && ProcessCrossRoadAtLights() )
		{
			// State change handled in ProcessCrossRoadAtLights
		}
		else
		{
			SetState(State_Wander);
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskWander::FSM_Return CTaskWander::StateExitVehicle_OnEnter()
{
	VehicleEnterExitFlags vehicleFlags;
	vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::WarpIfDoorBlocked);
	vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::WarpIfShuffleLinkIsBlocked);
	SetNewTask(rage_new CTaskExitVehicle(GetPed()->GetMyVehicle(), vehicleFlags));
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskWander::FSM_Return CTaskWander::StateExitVehicle_OnUpdate()
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		if(GetPed()->GetVehiclePedInside())
		{
			if (!GetPed()->GetVehiclePedInside()->IsDummy())
			{
				// Still in a vehicle - failed to exit, try again
				SetFlag(aiTaskFlags::RestartCurrentState);
			}
		}
		else
		{
			// Wander
			SetState(State_Wander);
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskWander::FSM_Return CTaskWander::StateWander_OnEnter()
{
	// Cache the ped
	CPed* pPed = GetPed();

	AnalyzePavementSituation();

	// Randomise the wander rate of peds +/- 0.1f

	const float fRandomAmount = (((float)pPed->GetRandomSeed()) / (float)RAND_MAX_16) * fRandomRange;
	m_fWanderMoveRate = 1.f + (fRandomAmount - fCentre);

	const bool bKeepMovingWhilstWaitingForPath =
		m_bKeepMovingWhilstWaitingForFirstPath ||
		GetPreviousState()==State_CrossRoadAtLights || // also keep moving if just crossed road, to avoid slight pause/glitch
		GetPreviousState()==State_CrossRoadJayWalking;

	CTaskMoveWander* pMoveWander = rage_new CTaskMoveWander(m_fMoveBlendRatio, m_fDirection, m_bWanderSensibly, m_fTargetRadius, m_bStayOnPavements, bKeepMovingWhilstWaitingForPath);
	pMoveWander->SetWanderPathDistances(m_fWanderPathLengthMin, m_fWanderPathLengthMax);
	pMoveWander->SetConsiderRunStartForPathLookAhead(true);

	// If we are resuming wandering after crossing the road, then use the existing subtask.
	// We want to keep CTaskAmbientClips alive as it might be managing a prop.
	CTask* pSubtask = GetSubTask();
	if (GetPreviousState() == State_CrossRoadAtLights && pSubtask && pSubtask->GetTaskType() == CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
	{
		CTaskComplexControlMovement* pSubtaskCCM = static_cast<CTaskComplexControlMovement*>(pSubtask);
		pSubtaskCCM->SetNewMoveTask(pMoveWander);
	}
	else
	{
		CTaskAmbientClips* pTaskAmbientClips = CreateAmbientClipsTask();
		SetNewTask(rage_new CTaskComplexControlMovement(pMoveWander, pTaskAmbientClips));
	}

	// Clear flags
	m_bRestartWander = false;
	m_bKeepMovingWhilstWaitingForFirstPath = false;
	m_bNeedCrossingCheckPathSegDoublesBack = false;

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskWander::FSM_Return CTaskWander::StateWander_OnUpdate()
{
	CPed* pPed = GetPed();

	if(m_BlockedByVehicle && GetState()!=State_BlockedByVehicle)
	{
		SetState(State_BlockedByVehicle);
		return FSM_Continue;
	}

	CTask* pTask = GetSubTask();
	if (pTask && pTask->GetTaskType() == CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
	{
		CTaskComplexControlMovement* pTaskControl = static_cast<CTaskComplexControlMovement*>(pTask);
		// Task ambient clips can end, and if it does, we need to restart it.
		// Otherwise the ped will never play another ambient animation again as long as we are in this state.
		if (pTaskControl->GetMainSubTaskType() != CTaskTypes::TASK_AMBIENT_CLIPS)
		{
			pTaskControl->SetNewMainSubtask(CreateAmbientClipsTask());
		}
	}

	//---------------------------------------------------------------------------------
	// Sync the 'm_fDirection' from CTaskMoveWander::GetWanderHeading()
	// This ensures that we're up to date with the direction the ped has been wandering

	CTask * pTaskMove = GetPed()->GetPedIntelligence()->GetActiveMovementTask();
	if(pTaskMove && pTaskMove->GetTaskType()==CTaskTypes::TASK_MOVE_WANDER)
	{
		m_fDirection = ((CTaskMoveWander*)pTaskMove)->GetWanderHeading();
	}

	//---------------------------------------------------------------------------------------------------
	// If event scanning is turned off for this ped, we have to manually ensure that we are getting
	// potential collision events - since the avoidance of vehicle whilst wandering relies upon it.

	if(GetPed()->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodEntityScanning))
	{
		if(fwTimer::GetTimeInMilliseconds() - m_iLastForcedVehiclePotentialCollisionScanTime >= CVehiclePotentialCollisionScanner::ms_iPeriod)
		{
			GetPed()->GetPedIntelligence()->GetEventScanner()->GetVehiclePotentialCollisionScanner().Scan(*GetPed(), true);
			m_iLastForcedVehiclePotentialCollisionScanTime = fwTimer::GetTimeInMilliseconds();
		}
	}

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished) || !GetSubTask() || m_bRestartWander)
	{
		// CTaskComplexControlMovement may end right away, if we are riding and the
		// mount has its main task. If we restart the state, we may end up with an
		// infinite loop in the task update, or at least getting the overhead
		// of running StateWander_OnEnter() each frame. To avoid this, we ask
		// CTaskComplexControlMovement if it's yielding to the mount.
		if(CTaskComplexControlMovement::ShouldYieldToMount(*GetPed()))
		{
			// Setting m_bRestartWander ensures that we repeat this check on the next
			// frame, even though SubTaskFinished may not be set (it may only be set
			// for one frame, then the subtask pointer goes NULL instead).
			m_bRestartWander = true;
		}
		else
		{
			SetFlag(aiTaskFlags::RestartCurrentState);
		}
	}
	else
	{
		pPed->GetMotionData()->SetCurrentMoveRateOverride(m_fWanderMoveRate);

		// Some peds don't try to do anything fancy like cross roads while wandering.
		if (!pPed->GetTaskData().GetIsFlagSet(CTaskFlags::UseSimpleWander))
		{
		
#if FIND_SHELTER
			if(ProcessMoveToShelterAndWait())
			{
				// State change handled in ProcessMoveToShelterAndWait
			}
			else 
#endif //FIND_SHELTER
			if(ProcessCrossRoadAtLights())
			{
				// State change handled in ProcessCrossRoadAtLights
			}
			else if (ProcessCrossRoadJayWalking())
			{
				// State change handled in ProcessCrossRoadJayWalking
			}
			else if(ProcessCrossDriveWay())
			{
				// State change handled in ProcessCrossDriveWay
			}
			else if( ProcessCriminalOpportunities() )
			{
				// State change handled in ProcessCriminalOpportunities
			}
			else
			{
				// Do anything we need to do if it's raining
				ProcessRaining();
			}
		}
	}

	//---------------------------------------------------------------------------------------------
	// Once the ped is wandering, we may safely NULL out our 'm_LastVehicleWalkedAround' pointer.
	// Up until this point it is used within CTaskMoveWander to selectively avoid this vehicle.

	if(m_LastVehicleWalkedAround)
	{
		CPed * pPed = GetPed();
		CTaskMoveWander * pMoveWander = (CTaskMoveWander*) pPed->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_WANDER);
		if(pMoveWander && pMoveWander->GetState()==CTaskNavBase::NavBaseState_FollowingPath)
		{
			m_LastVehicleWalkedAround = NULL;
		}
	}

	// This chunk makes sure that peds are not walking heels to heel
	if (!m_ScanForNearbyPedsTimer.IsSet() || m_ScanForNearbyPedsTimer.IsOutOfTime())
	{
		const CPed * pPed = GetPed();
		CEntityScannerIterator nearbyPeds = pPed->GetPedIntelligence()->GetNearbyPeds();
		for(CEntity* pEntity = nearbyPeds.GetFirst(); pEntity; pEntity = nearbyPeds.GetNext())
		{
			//Ensure the nearby ped is alive.
			const CPed* pNearbyPed = static_cast<CPed *>(pEntity);
			if (!pNearbyPed)
				continue;

			if (!pNearbyPed->GetPedResetFlag(CPED_RESET_FLAG_Wandering))
				continue;

			const float fWanderMoveRate = pNearbyPed->GetMotionData()->GetCurrentMoveRateOverride();
			if (abs(fWanderMoveRate - m_fWanderMoveRate) > fCentre) // Moving at well different speeds
				continue;

			const Vec3V vPedFwd = pPed->GetTransform().GetForward();
			const Vec3V vPedPos = pPed->GetTransform().GetPosition();
			const Vec3V vNearbyPedFwd = pNearbyPed->GetTransform().GetForward();
			const Vec3V vNearbyPedPos = pNearbyPed->GetTransform().GetPosition();
			const ScalarV vDotThreshold = ScalarV(0.99f);
			const ScalarV vPosThresholdSqr = ScalarV(1.5f);

			if (IsLessThanAll((DistSquared(vNearbyPedPos, vPedPos)), vPosThresholdSqr) &&	// Is the other ped within distance?
				IsGreaterThanAll(Dot(vNearbyPedFwd, vPedFwd), vDotThreshold))				// And facing the same way?
			{
				const ScalarV vDotThresholdFront = ScalarV(0.9f);
				const Vec3V vNearbyPedDir = Normalize(vNearbyPedPos - vPedPos);
				if (IsGreaterThanAll(Dot(vNearbyPedDir, vPedFwd), vDotThresholdFront))		// And the other ped in front of us?
				{
					if (fWanderMoveRate > 1.0f)
						m_fWanderMoveRate = fWanderMoveRate - fCentre;
					else
						m_fWanderMoveRate = fWanderMoveRate + fCentre;
				}
			}
		}

		m_ScanForNearbyPedsTimer.Set(fwTimer::GetTimeInMilliseconds(), SCAN_FOR_NEARBY_PEDS_FREQ);
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskWander::FSM_Return CTaskWander::StateWander_OnExit()
{
#if FIND_SHELTER
	if(m_hFindShelterFloodFillPathHandle != PATH_HANDLE_NULL)
	{
		CPathServer::CancelRequest(m_hFindShelterFloodFillPathHandle);
	}

	m_hFindShelterFloodFillPathHandle = PATH_HANDLE_NULL;
#endif //FIND_SHELTER
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskWander::FSM_Return CTaskWander::StateWanderToStoredPosition_OnEnter()
{
	CTaskMoveFollowNavMesh* pMoveNavMeshTask = rage_new CTaskMoveFollowNavMesh(m_fMoveBlendRatio, m_vContinueWanderingPos);
	SetNewTask(rage_new CTaskComplexControlMovement(pMoveNavMeshTask));
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskWander::FSM_Return CTaskWander::StateWanderToStoredPosition_OnUpdate()
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Wander);
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskWander::FSM_Return CTaskWander::StateCrossRoadAtLights_OnEnter()
{
	// if not already set
	if( ms_ScenarioIndexWaitingToCross < 0 )
	{
		// Get the index for the waiting to cross scenario
		ms_ScenarioIndexWaitingToCross = SCENARIOINFOMGR.GetScenarioIndex(ms_waitToCrossScenario);
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskWander::FSM_Return CTaskWander::StateCrossRoadAtLights_OnUpdate()
{
	// We should already have a subtask
	if(!taskVerifyf(GetSubTask(), "No sub task"))
	{
		// Fall back to wandering
		SetState(State_Wander);
	}

	// If subtask is finished
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		// Return to wandering
		SetState(State_Wander);
	}
	
	// If we have a complex control movement subtask
	if(GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
	{
		CTaskComplexControlMovement* pComplexControlTask = static_cast<CTaskComplexControlMovement*>(GetSubTask());
		CTask* pMoveTask = pComplexControlTask->GetRunningMovementTask(GetPed());
		if(pMoveTask && taskVerifyf(pMoveTask->GetTaskType() == CTaskTypes::TASK_MOVE_CROSS_ROAD_AT_TRAFFIC_LIGHTS, "pMoveTask for State [%s] should always be of type [TASK_MOVE_CROSS_ROAD_AT_TRAFFIC_LIGHTS] - not [%s]", GetStateName(GetState()), TASKCLASSINFOMGR.GetTaskName(pMoveTask->GetTaskType())))
		{
			// Get a handle to the road crossing subtask
			CTaskMoveCrossRoadAtTrafficLights* pMoveCrossRoadTask = static_cast<CTaskMoveCrossRoadAtTrafficLights*>(pMoveTask);

			// If the ped is waiting and can play the crosswalk idles
			if(ShouldAllowPedToPlayCrosswalkIdles() && pMoveCrossRoadTask->IsWaitingToCrossRoad())
			{
				// set the idling flag
				m_bIdlingAtCrossWalk = true;

				// randomize allowing Ped to idle at crosswalk
				if(fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < IDLE_AT_CROSSWALK_CHANCE)
				{
					//Currently no anims for scenarios so randomly choose one. 
					s32 iScenarioIndex = ms_ScenarioIndexWaitingToCross;
					CScenarioInfo* pScenarioInfo = NULL;
					if( iScenarioIndex >= 0 )
					{
						pScenarioInfo = SCENARIOINFOMGR.GetScenarioInfoByIndex(iScenarioIndex);
					}
					// Only start a new scenario if the scenario actually has animations to play.
					if (pScenarioInfo && pScenarioInfo->GetConditionalAnimsGroup().GetNumAnims() > 0)
					{
						CTask* pUseScenarioTask = rage_new CTaskUseScenario(iScenarioIndex, CTaskUseScenario::SF_StartInCurrentPosition | CTaskUseScenario::SF_CheckConditions);
						pUseScenarioTask->SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
						pComplexControlTask->SetNewMainSubtask(pUseScenarioTask);
					}
				}
			}
			// otherwise, if the ped is crossing and is not running an ambient clips subtask
			else if(pMoveCrossRoadTask->IsCrossingRoad() && pComplexControlTask->GetMainSubTaskType() != CTaskTypes::TASK_AMBIENT_CLIPS)
			{
				// Assign a new ambient clips subtask
				CTaskAmbientClips* pAmbientTask = rage_new CTaskAmbientClips(CTaskWander::GetCommonTaskAmbientFlags(), GetConditionalAnimsGroup(), GetActiveChosenConditionalAnimation());
				pComplexControlTask->SetNewMainSubtask(pAmbientTask);
			}

			// If ped has started crossing
			if(pMoveCrossRoadTask->IsCrossingRoad())
			{
				// release the idling flag
				m_bIdlingAtCrossWalk = false;

				// release ped crossing reservation
				if( m_bMadePedRoadCrossReservation )
				{
					ThePaths.ReleasePedCrossReservation(m_RoadCrossRecord.m_StartNodeAddress, m_RoadCrossRecord.m_EndNodeAddress, m_RoadCrossRecord.m_ReservationSlot);
					m_bMadePedRoadCrossReservation = false;
				}
			}

			// Check if the ped is arriving to the end of the crossing
			if( ms_bEnableChainCrossing && pMoveCrossRoadTask->IsReadyToChainNextCrossing() )
			{
				if( ProcessConsecutiveCrossRoadAtLights(pMoveCrossRoadTask->GetCrossingDir()) )
				{
					// Mark road as crossed because we are interrupting the subtask externally
					CPed* pPed = GetPed();
					pPed->GetPedIntelligence()->RecordFinishedCrossingRoad();
					
					// state change decided, continue
					return FSM_Continue;
				}
			}

			// Check if the ped is arriving at pavement
			if( ms_bEnablePavementArrivalCheck && pMoveCrossRoadTask->IsArrivingAtPavement() )
			{
				// Keep subtask - we want to preserve the ambient clip task involved.
				SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);

				// go back to wander state now, don't walk all the way to the goal first
				SetState(State_Wander);

				// Mark road as crossed because we are interrupting the subtask externally
				CPed* pPed = GetPed();
				pPed->GetPedIntelligence()->RecordFinishedCrossingRoad();

				// state change decided, continue
				return FSM_Continue;
			}
		}
	}
	
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskWander::FSM_Return CTaskWander::StateCrossRoadAtLights_OnExit()
{
	// allow a crossing scan immediately upon finishing a crossing
	m_ScanForRoadCrossingsTimer.Unset();

	// release ped crossing reservation
	if( m_bMadePedRoadCrossReservation )
	{
		//@@: location CTASKWANDER_STATECROSROADATLIGHTS_ONEXIT_RELEASE_PED
		ThePaths.ReleasePedCrossReservation(m_RoadCrossRecord.m_StartNodeAddress, m_RoadCrossRecord.m_EndNodeAddress, m_RoadCrossRecord.m_ReservationSlot);
		m_bMadePedRoadCrossReservation = false;
	}

	// release the idling flag
	m_bIdlingAtCrossWalk = false;

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskWander::FSM_Return CTaskWander::CrossRoadJayWalking_OnEnter()
{
	// We should already have a subtask - if not, quit
	if(!taskVerifyf(GetSubTask(), "No sub task"))
	{
		// Fall back to wandering
		SetState(State_Wander);
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskWander::FSM_Return CTaskWander::CrossRoadJayWalking_OnUpdate()
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Wander);
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskWander::FSM_Return CTaskWander::StateMoveToShelterAndWait_OnEnter()
{
#if FIND_SHELTER
	const float fRandomMBR = fwRandom::GetRandomNumberInRange((MOVEBLENDRATIO_WALK + MOVEBLENDRATIO_RUN) * 0.5f, MOVEBLENDRATIO_SPRINT);

	// If it exists, get the current AnimIndex for whatever prop is in hand
	CTaskComplexControlMovement* pTaskCCM = NULL;
	CTask* pSubtask = GetSubTask();
	if (pSubtask && pSubtask->GetTaskType() == CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
	{
		// Ped currently has an active CCM task.
		// Switch the move task to be pCrossRoad.
		pTaskCCM = static_cast<CTaskComplexControlMovement*>(pSubtask);

		// If there is a prop helper in play,
		// Check for an active ambient clips main subtask
		if( m_pPropHelper && pTaskCCM->GetSubTask() && pTaskCCM->GetSubTask()->GetTaskType() == CTaskTypes::TASK_AMBIENT_CLIPS )
		{
			// Store the anim chosen corresponding to the prop for use in the next entry to the wander state
			CTaskAmbientClips* pTAC = static_cast<CTaskAmbientClips*>(pTaskCCM->GetSubTask());
			m_iAnimIndexForExistingProp = pTAC->GetConditionalAnimChosen();
		}
	}


	CTaskMoveGoToShelterAndWait* pShelterMoveTask = rage_new CTaskMoveGoToShelterAndWait(fRandomMBR, m_vShelterPos, -1);

	// Set up ambient clips task:
	CTaskAmbientClips* pAmbientTask = NULL;

	// If we have a prop helper from a previous state
	if( m_pPropHelper )
	{
		// use the same anim index
		pAmbientTask = rage_new CTaskAmbientClips(CTaskWander::GetCommonTaskAmbientFlags(), GetConditionalAnimsGroup(), m_iAnimIndexForExistingProp);
	}
	else // general case
	{
		pAmbientTask = rage_new CTaskAmbientClips(CTaskWander::GetCommonTaskAmbientFlags(), GetConditionalAnimsGroup(), m_iOverrideAnimIndex);
	}

	SetNewTask(rage_new CTaskComplexControlMovement(pShelterMoveTask, pAmbientTask));
#else
	aiAssert(0); // Shouldn't be in StateMoveToShelterAndWait
#endif //FIND_SHELTER
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskWander::FSM_Return CTaskWander::StateMoveToShelterAndWait_OnUpdate()
{
	// Cache the ped
	CPed* pPed = GetPed();

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		// If its still raining, then continue to wait..
		if(g_weather.IsRaining() && !GetDoesPedHaveBrolly(pPed))
		{
			SetFlag(aiTaskFlags::RestartCurrentState);
		}
		else
		{
			SetState(State_Wander);
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskWander::FSM_Return CTaskWander::StateCrime_OnUpdate()
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Wander);
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskWander::FSM_Return CTaskWander::StateHolsterWeapon_OnEnter()
{
	//Grab the ped.
	CPed* pPed = GetPed();
	
	//Check if the weapon manager is valid.
	CPedWeaponManager* pWeaponManager = pPed->GetWeaponManager();
	if(pWeaponManager)
	{
		//Make sure we are unarmed.
		pWeaponManager->EquipWeapon(pPed->GetDefaultUnarmedWeaponHash());
	}
	
	//Create the task.
	CTask* pTask = rage_new CTaskSwapWeapon(SWAP_HOLSTER);
	
	//Start the task.
	SetNewTask(pTask);
	
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskWander::FSM_Return CTaskWander::StateHolsterWeapon_OnUpdate()
{
	//Check if the sub-task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Move back to the start state.
		SetState(State_Init);
	}
	
	return FSM_Continue;
}

CTaskWander::FSM_Return CTaskWander::StateBlockedByVehicle_OnEnter()
{
	CTaskMoveStandStill * pStandStill = rage_new CTaskMoveStandStill(2000);
	SetNewTask( rage_new CTaskComplexControlMovement(pStandStill, NULL) );

	return FSM_Continue;
}

CTaskWander::FSM_Return CTaskWander::StateBlockedByVehicle_OnUpdate()
{
	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		if(!m_BlockedByVehicle)
		{
			SetState( State_Wander );
			return FSM_Continue;
		}
		static dev_float fThresholdSqr = 2.0f*2.0f;
		Vector3 vDelta = m_vBlockedByVehicleLastPos - VEC3V_TO_VECTOR3(m_BlockedByVehicle->GetTransform().GetPosition());
		if(vDelta.XYMag2() > fThresholdSqr)
		{
			m_BlockedByVehicle = NULL;
			SetState( State_Wander );
			return FSM_Continue;
		}

		SetFlag(aiTaskFlags::RestartCurrentState);
		return FSM_Continue;
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTaskWander::FSM_Return CTaskWander::StateAborted()
{
	// Sometimes we will set this flag during DoAbort, so that we can return to a sensible
	// position from which to wander if we get interrupted by something which is likely to pull
	// us off the pavement (eg. a shocking event)
	if(m_bContinueWanderingFromStoredPosition)
	{
		m_bContinueWanderingFromStoredPosition = false;
		SetState(State_WanderToStoredPosition);
	}
	else
	{
		// In this case set the task's wander direction to be their current heading.
		SetDirection(GetPed()->GetCurrentHeading());
		SetState(State_Wander);
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////


void CTaskWander::SetBlockedByVehicle(CVehicle * pVehicle)
{
	m_BlockedByVehicle = pVehicle;
	m_vBlockedByVehicleLastPos = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
}

void CTaskWander::ProcessRaining()
{
	if(g_weather.IsRaining() && !m_bProcessedRain && ms_uNumRainTransitioningPeds < CTaskWander::sm_Tunables.m_uNumPedsToTransitionToRainPerPeriod)
	{
		++ms_uNumRainTransitioningPeds;

		m_bProcessedRain = true;

		// Cache the ped
		CPed* pPed = GetPed();

		//********************************************************
		// Is the ped carrying an umbrella?  Don't shelter if so..
		if(!GetDoesPedHaveBrolly(pPed))
		{
#if FIND_SHELTER
			// Don't put peds into shelter if the player is inside an interior - too often this causes a hoard
			// of peds to shelter outside the exit to the current interior, which can look silly.
			CPed * pPlayer = FindPlayerPed();
			if(pPlayer && pPlayer->GetPortalTracker() && pPlayer->GetPortalTracker()->IsInsideInterior() == false)
			{
				static dev_u32 SHELTER_SEARCH_FREQ = 8000;

				const bool bScan = (fwTimer::GetTimeInMilliseconds() - m_iLastFindShelterSearchTime) > SHELTER_SEARCH_FREQ;
				if(bScan)
				{
					const Vector3 vCoverSearchPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
					m_hFindShelterFloodFillPathHandle = CPathServer::RequestClosestShelteredPolySearch(vCoverSearchPos, 15.0f, pPed);
					m_iLastFindShelterSearchTime = fwTimer::GetTimeInMilliseconds();
				}
			}
#endif //FIND_SHELTER

			//********************************************************************************
			//	Randomly get some peds to randomly run in the rain.  This is only if they're
			//	not carrying brollies, and if they ARE carrying some other type of prop.
			//	Why?  Because if they're carrying NO PROPS, then the ambient system will be
			//	making sure they play some other appropriate clip & making them run. Cunning.

			// Turning off running in the rain; B* 1406511 [5/22/2013 mdawe]
			//if(pPed->GetHeldObject(*pPed) || pPed->IsHoldingProp())
			//{
			//	const bool bRandomMightRun = fwRandom::GetRandomNumberInRange(0, 100) > 50;
			//	if(bRandomMightRun)
			//	{
			//		CTask* pMoveTask = pPed->GetPedIntelligence()->GetActiveMovementTask();
			//		if(pMoveTask && pMoveTask->GetTaskType() == CTaskTypes::TASK_MOVE_WANDER)
			//		{
			//			CTaskMoveWander* pMoveWanderTask = static_cast<CTaskMoveWander*>(pMoveTask);
			//			float fRatio = pMoveWanderTask->GetMoveBlendRatio();
			//			if(fRatio <= MOVEBLENDRATIO_WALK)
			//			{
			//				fRatio = m_fMoveBlendRatio + fwRandom::GetRandomNumberInRange(0.5f, 1.0f);
			//				pMoveWanderTask->SetMoveBlendRatio(fRatio);
			//			}
			//		}
			//	}
			//}
			CTaskAmbientClips* pAmbientClipsTask = static_cast<CTaskAmbientClips*>(FindSubTaskOfType(CTaskTypes::TASK_AMBIENT_CLIPS));
			if (pAmbientClipsTask)
			{
				if (pPed->IsHoldingProp())
				{
					if (aiVerify(pAmbientClipsTask->GetPropHelper()))
					{
						pAmbientClipsTask->GetPropHelper()->DisposeOfProp(*pPed);
					}
				}

				if (pAmbientClipsTask->TerminateIfChosenConditionalAnimConditionsAreNotMet())
				{
					SetFlag(aiTaskFlags::RestartCurrentState);
				}
			}
		}
	}
	else if(!g_weather.IsRaining() && m_bProcessedRain && CTaskWander::ms_uNumRainTransitioningPeds < CTaskWander::sm_Tunables.m_uNumPedsToTransitionToRainPerPeriod)
	{
		++ms_uNumRainTransitioningPeds;

		m_bProcessedRain = false;
		
		CTaskAmbientClips* pAmbientClipsTask = static_cast<CTaskAmbientClips*>(FindSubTaskOfType(CTaskTypes::TASK_AMBIENT_CLIPS));
		if (pAmbientClipsTask)
		{
			if (pAmbientClipsTask->TerminateIfChosenConditionalAnimConditionsAreNotMet())
			{
				SetFlag(aiTaskFlags::RestartCurrentState);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
#if FIND_SHELTER
		bool CTaskWander::ProcessMoveToShelterAndWait()
		{
			// Do we have a shelter path handle?
			if(m_hFindShelterFloodFillPathHandle != PATH_HANDLE_NULL)
			{
				EPathServerRequestResult ret = CPathServer::IsRequestResultReady(m_hFindShelterFloodFillPathHandle);
				if(ret == ERequest_Ready)
				{
					EPathServerErrorCode eErr = CPathServer::GetClosestShelteredPolySearchResultAndClear(m_hFindShelterFloodFillPathHandle, m_vShelterPos);
					m_hFindShelterFloodFillPathHandle = PATH_HANDLE_NULL;

					if(eErr == PATH_NO_ERROR && g_weather.IsRaining() && ComputeIsRoomToShelterHere(m_vShelterPos, 2.5f, 2))
					{
						// Shelter
						SetState(State_MoveToShelterAndWait);
						return true;
					}
				}
				else if(ret == ERequest_NotFound)
				{
					m_hFindShelterFloodFillPathHandle = PATH_HANDLE_NULL;
					return false;
				}
			}

			return false;
		}

		////////////////////////////////////////////////////////////////////////////////

		bool CTaskWander::ComputeIsRoomToShelterHere(const Vector3& vPosition, const float fCheckRadius, const s32 iMaxNumPedsHere)
		{
			ms_iNumShelteringPeds = 0;

			fwIsSphereIntersecting sphere(spdSphere(RCC_VEC3V(vPosition), ScalarV(fCheckRadius)));
			CGameWorld::ForAllEntitiesIntersecting(&sphere, RoomToShelterCB, NULL, ENTITY_TYPE_MASK_PED, SEARCH_LOCATION_EXTERIORS|SEARCH_LOCATION_INTERIORS);

			// We can shelter up to & including the maximum number
			if(ms_iNumShelteringPeds <= iMaxNumPedsHere)
			{
				return true;
			}

			return false;
		}

		////////////////////////////////////////////////////////////////////////////////

		bool CTaskWander::RoomToShelterCB(CEntity* pEntity, void* UNUSED_PARAM(pData))
		{
			if(pEntity && pEntity->GetIsTypePed())
			{
				ms_iNumShelteringPeds++;
			}

			return true;
		}
#endif //FIND_SHELTER

void CTaskWander::AnalyzePavementSituation()
{
	// No need to do the popzone check below or the scanning if these conditions are not true.
	if (!m_bStayOnPavements || !m_bRemoveIfInBadPavementSituation)
	{
		return;
	}

	// Check if the zone needs pavement.
	Vector3 vPedPosition = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition());
	CPopZone* pPopZone = CPopZones::FindSmallestForPosition(&vPedPosition, ZONECAT_ANY, ZONETYPE_ANY);
	if (pPopZone)
	{
		// Ask the pop zone if peds should stick to pavements.
		m_bZoneRequiresPavement = !pPopZone->m_bLetHurryAwayLeavePavements;
	}
	else
	{
		// If we fail to find a popzone, force pavement.
		m_bZoneRequiresPavement = true;
	}

	// In some cases start scanning for pavement.
	if (m_bZoneRequiresPavement)
	{
		StartPavementFloodFillRequest();	
	}
}

void CTaskWander::StartPavementFloodFillRequest()
{
	m_PavementHelper.StartPavementFloodFillRequest(GetPed());
	if (m_PavementHelper.HasNoPavement())
	{
		m_bLacksPavement = true;
	}
}

void CTaskWander::ProcessPavementFloodFillRequest()
{
	m_PavementHelper.UpdatePavementFloodFillRequest(GetPed());
	if (m_PavementHelper.HasNoPavement())
	{
		m_bLacksPavement = true;
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskWander::ProcessCrossRoadAtLights()
{
	// Cache the ped
	CPed* pPed = GetPed();

	// First check for the case where we are recovering from a road crossing that was interrupted
	if( pPed->GetPedIntelligence() && pPed->GetPedIntelligence()->ShouldResumeCrossingRoad() )
	{
		// Use ped's current position as start
		const Vector3& vStartPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

		// Use stored end position as the goal
		const Vector3& vEndPos = pPed->GetPedIntelligence()->GetCrossRoadEndPos();

		// Recovering has no existing move target
		const Vector3* pExistingMoveTarget = NULL;

		// Recovering doesn't use delay time
		const int crossDelayMS = 0;

		// Recovering is not respecting a traffic light
		const bool bIsTrafficLightCrossing = false;
		
		// Recovering is mid-crossing
		const bool bIsMidRoadCrossing = true;

		// Recovering is not a driveway: this doesn't matter given mid-crossing, task will just go to the goal straight away
		const bool bIsDriveWayCrossing = false;

		// Create the tasks
		CTaskMoveCrossRoadAtTrafficLights* pCrossRoad = rage_new CTaskMoveCrossRoadAtTrafficLights(vStartPos, vEndPos, pExistingMoveTarget, crossDelayMS, bIsTrafficLightCrossing, bIsMidRoadCrossing, bIsDriveWayCrossing);
		CTaskAmbientClips* pAmbientTask = rage_new CTaskAmbientClips(CTaskWander::GetCommonTaskAmbientFlags(), GetConditionalAnimsGroup(), GetActiveChosenConditionalAnimation());

		// Create the task here as we have the information here
		SetNewTask(rage_new CTaskComplexControlMovement(pCrossRoad, pAmbientTask));
		SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
		SetState(State_CrossRoadAtLights);
		return true;
	}

	// Check if we are running a move wander task and our next path segment doubled back:
	// Get a local handle to move wander subtask, if any
	CTaskMoveWander* pMoveWanderSubtask = NULL;
	if(GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
	{
		CTask* pMoveTask = static_cast<CTaskComplexControlMovement*>(GetSubTask())->GetRunningMovementTask(pPed);
		if(pMoveTask)
		{
			if(pMoveTask->GetTaskType() == CTaskTypes::TASK_MOVE_WANDER)
			{
				pMoveWanderSubtask = static_cast<CTaskMoveWander*>(pMoveTask);
				if( pMoveWanderSubtask && pMoveWanderSubtask->GetNextPathDoublesBack() )
				{
					m_bNeedCrossingCheckPathSegDoublesBack = true;

					// acknowledge signal received to clear the flag
					// otherwise we would perform aggressive checks every frame for the duration of this wander route
					pMoveWanderSubtask->ClearNextPathDoublesBack(); 
				}
			}
		}
	}

	//**********************************************************************************
	//	Crossing roads
	//	This is where we get peds to scan their environment for pedestrian crossings
	//	which they can use.  These are set up as nodes placed by artists.  Since the
	//	process is reasonably expensive (and there might be a lot of wandering peds) we
	//	only do it infrequently, also this helps prevent peds from choosing to cross the
	//	road too often which would look bad.

	if(m_bNeedCrossingCheckPathSegDoublesBack || !m_ScanForRoadCrossingsTimer.IsSet() || m_ScanForRoadCrossingsTimer.IsOutOfTime())
	{
		const bool bPreventRoadCrossing = NetworkInterface::IsGameInProgress() || ms_bPreventCrossingRoads;
		if( !bPreventRoadCrossing )
		{
			static s32 SCAN_FOR_ROAD_CROSSINGS_FREQ = 2000;
			// For debugging allow faster scan rate
			if( ms_bPromoteCrossingRoads )
			{
				SCAN_FOR_ROAD_CROSSINGS_FREQ = 100;
			}
			m_ScanForRoadCrossingsTimer.Set(fwTimer::GetTimeInMilliseconds(), SCAN_FOR_ROAD_CROSSINGS_FREQ);

			// Initialize ped ahead dot min with default
			float fPedAheadDotMin = DEFAULT_ROAD_CROSS_PED_TO_START_DOT_MIN;

			// Initialize ped to crossing distance with default
			float fPedToCrossingDistSqMax = DEFAULT_DIST_SQ_PED_TO_START_MAX;

			// Check if we recently crossed
			const u32 uJustCrossedRoadTimeoutMS = 5000;
			if( fwTimer::GetTimeInMilliseconds() <= pPed->GetPedIntelligence()->GetLastFinishedCrossRoadTime() + uJustCrossedRoadTimeoutMS )
			{
				// Check if the navmesh tracker has had enough time to detect pavement underfoot
				const u32 uMinTimeForTrackerToDetectPavement = 500;
				const u32 uTimeElapsedSinceCrossing = fwTimer::GetTimeInMilliseconds() - pPed->GetPedIntelligence()->GetLastFinishedCrossRoadTime();
				bool bTrackerHasHadEnoughTime = (uTimeElapsedSinceCrossing >= uMinTimeForTrackerToDetectPavement);
				// If not enough time has passed
				if( !bTrackerHasHadEnoughTime )
				{
					// Set the scan delay to give enough time
					m_ScanForRoadCrossingsTimer.Set(fwTimer::GetTimeInMilliseconds(), uMinTimeForTrackerToDetectPavement - uTimeElapsedSinceCrossing);
					
					// no road cross yet
					return false;
				}

				// Get status of ped on pavement
				bool bPedIsOnPavement = (pPed->GetNavMeshTracker().GetIsValid() && pPed->GetNavMeshTracker().GetNavPolyData().m_bPavement);

				// Check if we are not on pavement
				if( ms_bEnableArriveNoPavementCrossing && !bPedIsOnPavement )
				{
					// adjust crossing search params
					fPedAheadDotMin = ACCEPT_ALL_DOT_MIN;
					fPedToCrossingDistSqMax = INCREASED_DIST_SQ_PED_TO_START_MAX;
				}
				// Otherwise, check if our wandering subtask has failed to find a route
				else if( ms_bEnableWanderFailCrossing )
				{
					if( pMoveWanderSubtask && pMoveWanderSubtask->HasFailedRouteAttempt() )
					{
						// adjust crossing search params
						fPedAheadDotMin = ACCEPT_ALL_DOT_MIN;
						fPedToCrossingDistSqMax = INCREASED_DIST_SQ_PED_TO_START_MAX;
					}
				}
				// Otherwise
				else
				{
					// Randomly choose to not cross again
					if( fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < POST_CROSSING_REPEAT_CHANCE )
					{
						// so that peds don't get stuck chaining crossings indefinitely
						// now that they may start wandering before passing the end point
						return false;
					}
				}
			}
			else if( ms_bEnableWanderFailCrossing ) // Check if our wandering subtask has failed to find a route
			{
				if( pMoveWanderSubtask && pMoveWanderSubtask->HasFailedRouteAttempt() )
				{
					// adjust crossing search params
					fPedAheadDotMin = INCREASED_ROAD_CROSS_PED_TO_START_DOT_MIN;
					fPedToCrossingDistSqMax = INCREASED_DIST_SQ_PED_TO_START_MAX;
				}
			}

			// If the wander route doubled back
			if( m_bNeedCrossingCheckPathSegDoublesBack )
			{
				// adjust crossing search params
				fPedAheadDotMin = DOUBLEBACK_ROAD_CROSS_PED_TO_START_DOT_MIN;
				fPedToCrossingDistSqMax = DOUBLEBACK_DIST_SQ_PED_TO_START_MAX;
			}

			// Setup scan parameters
			RoadCrossScanParams scanParams;
			scanParams.m_fPedToCrossDistSqMax = fPedToCrossingDistSqMax;
			scanParams.m_fPedAheadDotMin = fPedAheadDotMin;
			scanParams.m_bCheckForTrafficLight = true;
			scanParams.m_bWanderTaskStuck = pMoveWanderSubtask && pMoveWanderSubtask->HasFailedRouteAttempt();

			// Return the result of the helper method
			return HelperTryToCrossRoadAtLights(scanParams, m_ScanForRoadCrossingsTimer);
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskWander::ProcessCrossDriveWay()
{
	// Check if we are prevented from road crossing
	// @TODO: Do we want to keep this specifically for driveways? These are supposed to bridge sections of pavement...
	const bool bPreventRoadCrossing = NetworkInterface::IsGameInProgress() || ms_bPreventCrossingRoads;
	if( bPreventRoadCrossing )
	{
		return false;
	}

	// Check if scan timer has expired
	if(!m_ScanForDriveWayCrossingsTimer.IsSet() || m_ScanForDriveWayCrossingsTimer.IsOutOfTime())
	{
		// set next scan time
		static s32 SCAN_FOR_DRIVEWAY_CROSSINGS_FREQ = 1000;
		m_ScanForDriveWayCrossingsTimer.Set(fwTimer::GetTimeInMilliseconds(), SCAN_FOR_DRIVEWAY_CROSSINGS_FREQ);
	}
	else // otherwise try again later
	{
		return false;
	}

	// Setup scan parameters
	RoadCrossScanParams scanParams;
	scanParams.m_fPedToCrossDistSqMax = INCREASED_DIST_SQ_PED_TO_START_MAX;
	scanParams.m_fPedAheadDotMin = INCREASED_ROAD_CROSS_PED_TO_START_DOT_MIN;
	scanParams.m_bIncludeRoadCrossings = false;
	scanParams.m_bIncludeDriveWayCrossings = true;

	// Return the result of the helper method
	return HelperTryToCrossRoadAtLights(scanParams, m_ScanForDriveWayCrossingsTimer);
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskWander::ProcessConsecutiveCrossRoadAtLights(const Vector3& vCrossDir)
{
	// Check if we are prevented from road crossing
	const bool bPreventRoadCrossing = NetworkInterface::IsGameInProgress() || ms_bPreventCrossingRoads;
	if( bPreventRoadCrossing )
	{
		return false;
	}

	// Check if scan timer has expired
	if(!m_ScanForRoadCrossingsTimer.IsSet() || m_ScanForRoadCrossingsTimer.IsOutOfTime())
	{
		// set next scan time
		static s32 SCAN_FOR_CHAIN_ROAD_CROSSINGS_FREQ = 2000;
		m_ScanForRoadCrossingsTimer.Set(fwTimer::GetTimeInMilliseconds(), SCAN_FOR_CHAIN_ROAD_CROSSINGS_FREQ);
	}
	else // otherwise try again later
	{
		return false;
	}

	// Setup scan parameters
	RoadCrossScanParams scanParams;
	scanParams.m_fPedToCrossDistSqMax = CHAIN_DIST_SQ_PED_TO_START_MAX;
	scanParams.m_fPedAheadDotMin = CHAIN_ROAD_CROSS_PED_TO_START_DOT_MIN;
	scanParams.m_bCheckForTrafficLight = true;
	scanParams.m_bCheckCrossDir = true;
	scanParams.m_vPrevCrossDir = vCrossDir;

	// Return the result of the helper method
	return HelperTryToCrossRoadAtLights(scanParams, m_ScanForRoadCrossingsTimer);
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskWander::HelperTryToCrossRoadAtLights(const RoadCrossScanParams& scanParams, CTaskGameTimer& scanTimer)
{
	// Cache the ped
	CPed* pPed = GetPed();

	// Local flag for whether we are going to proceed
	bool bProceedWithCrossing = false;

	// Local crossing record data
	RoadCrossRecord candidatePlan;

	// Do the actual scan for crossing with the desired parameters
	bool bFoundCrossing = ComputeScanForRoadCrossings(pPed, scanTimer, candidatePlan, scanParams);
	if( bFoundCrossing )
	{
		// If plan is to cross a driveway
		if( candidatePlan.m_bIsDrivewayCrossing )
		{
			// We will proceed with this crossing
			bProceedWithCrossing = true;
		}
		// Otherwise crossing a road. Make the crossing reservation and get our reservation slot
		else if( ThePaths.MakePedCrossReservation(candidatePlan.m_StartNodeAddress, candidatePlan.m_EndNodeAddress, candidatePlan.m_ReservationSlot, candidatePlan.m_AssignedCrossDelayMS) )
		{
			// Reservation successful, update member flag
			m_bMadePedRoadCrossReservation = true;

			// We will proceed with this crossing
			bProceedWithCrossing = true;

			// Recompute start position now that we have a reservation slot
			HelperRecomputeStartPositionForReservationSlot(candidatePlan);
		}
		// if the reservation fails, fall through
	}
	if(bProceedWithCrossing)
	{
		// Copy candidate plan into member variable as we are proceeding
		m_RoadCrossRecord = candidatePlan;

		// Possibly pass wander target to the crossing
		Vector3 vWanderTarget;
		bool bHasWanderTarget = false;

		// Get the wander task
		CTask* pMoveTask = pPed->GetPedIntelligence()->GetActiveMovementTask();
		if(pMoveTask && pMoveTask->GetTaskType() == CTaskTypes::TASK_MOVE_WANDER)
		{
			CTaskMoveWander* pMoveWanderTask = static_cast<CTaskMoveWander*>(pMoveTask);

			if(pMoveWanderTask->GetWanderTarget(vWanderTarget))
			{
				bHasWanderTarget = true;
			}
		}

		// Create the road crossing task
		CTaskMoveCrossRoadAtTrafficLights* pCrossRoad = rage_new CTaskMoveCrossRoadAtTrafficLights(m_RoadCrossRecord.m_vCrossingStartPosition, m_RoadCrossRecord.m_vCrossingEndPosition, bHasWanderTarget ? &vWanderTarget : NULL, m_RoadCrossRecord.m_AssignedCrossDelayMS, m_RoadCrossRecord.m_bIsTrafficLightCrossing, m_RoadCrossRecord.m_bIsMidStreetCrossing, m_RoadCrossRecord.m_bIsDrivewayCrossing);
		
		// Check for existing complex control movement task
		CTaskComplexControlMovement* pTaskCCM = NULL;
		CTask* pSubtask = GetSubTask();
		if (pSubtask && pSubtask->GetTaskType() == CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
		{
			// Ped currently has an active CCM task.
			// Switch the move task to be pCrossRoad.
			pTaskCCM = static_cast<CTaskComplexControlMovement*>(pSubtask);
			pTaskCCM->SetNewMoveTask(pCrossRoad);

			// If there is a prop helper in play,
			// Check for an active ambient clips main subtask
			if( m_pPropHelper && pTaskCCM->GetSubTask() && pTaskCCM->GetSubTask()->GetTaskType() == CTaskTypes::TASK_AMBIENT_CLIPS )
			{
				// Store the anim chosen corresponding to the prop for use in the next entry to the wander state
				CTaskAmbientClips* pTAC = static_cast<CTaskAmbientClips*>(pTaskCCM->GetSubTask());
				m_iAnimIndexForExistingProp = pTAC->GetConditionalAnimChosen();
			}
		}
		else if (!pSubtask || pSubtask->MakeAbortable(ABORT_PRIORITY_URGENT, NULL))
		{
			// Ped did not have an active CCM task.
			// Need to create a new one.
			CTaskAmbientClips* pAmbientTask = rage_new CTaskAmbientClips(CTaskWander::GetCommonTaskAmbientFlags(), GetConditionalAnimsGroup(), GetActiveChosenConditionalAnimation());
			pTaskCCM = rage_new CTaskComplexControlMovement(pCrossRoad, pAmbientTask);
			SetNewTask(pTaskCCM);
		}
		else
		{
			delete pCrossRoad;
			// Could not abort the task.
			return false;
		}

		SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
		SetState(State_CrossRoadAtLights);

		//--------------------------------------------------------------------
		// Update wander heading to average of current & crossing directions

		Vector3 vCurrentDir( -rage::Sinf(m_fDirection), rage::Cosf(m_fDirection), 0.0f );
		Vector3 vCrossingDir = m_RoadCrossRecord.m_vCrossingEndPosition - m_RoadCrossRecord.m_vCrossingStartPosition;
		vCrossingDir.Normalize();
		Vector3 vAverageDir = vCurrentDir + vCrossingDir;
		m_fDirection = fwAngle::GetRadianAngleBetweenPoints(vAverageDir.x, vAverageDir.y, 0.0f, 0.0f);

		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

// Recompute the start position according to the reservation slot value in the record
void CTaskWander::HelperRecomputeStartPositionForReservationSlot(RoadCrossRecord& recordToModify)
{
	// Call the CPathFind method
	ThePaths.ComputePositionForPedCrossReservation(recordToModify.m_StartNodeAddress, recordToModify.m_EndNodeAddress, recordToModify.m_ReservationSlot, recordToModify.m_vCrossingStartPosition);
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskWander::ProcessCrossRoadJayWalking()
{
	// Cache the ped
	CPed* pPed = GetPed();

	//**********************************************************************************
	//	Crossing roads jay walking
	//	This is where we get peds to scan their environment for places to cross the road.
	//	These are not set up as nodes placed by artists but calculated using path finder.

	if(!m_ScanForJayWalkingLocationsTimer.IsSet() || m_ScanForJayWalkingLocationsTimer.IsOutOfTime())
	{
		const bool bPreventJayWalking = NetworkInterface::IsGameInProgress() || ms_bPreventJayWalking;

		if(!bPreventJayWalking)
		{
			static dev_s32 SCAN_FOR_JAY_WALKING_FREQ = 20000;
			m_ScanForJayWalkingLocationsTimer.Set(fwTimer::GetTimeInMilliseconds(), SCAN_FOR_JAY_WALKING_FREQ);

			RoadCrossRecord crossingPlan;
			RoadCrossScanParams scanParams; // use defaults

			//TEMP - REPLACE WITH PATH SEARCH
			bool bFoundCrossing = ComputeScanForRoadCrossings(pPed, m_ScanForJayWalkingLocationsTimer, crossingPlan, scanParams);

			// If we found no road crossings, but did find a driveway crossing
			if( crossingPlan.m_bIsDrivewayCrossing )
			{
				// don't jaywalk
				return false;
			}

			// We are crossing mid street to jaywalk
			crossingPlan.m_bIsMidStreetCrossing = true;

			if(bFoundCrossing && (!GetSubTask() || GetSubTask()->MakeAbortable(ABORT_PRIORITY_URGENT, NULL)))
			{
				Vector3 vWanderTarget;
				bool bHasWanderTarget = false;

				// Get the wander task
				CTask* pMoveTask = pPed->GetPedIntelligence()->GetActiveMovementTask();
				if(pMoveTask && pMoveTask->GetTaskType() == CTaskTypes::TASK_MOVE_WANDER)
				{
					CTaskMoveWander* pMoveWanderTask = static_cast<CTaskMoveWander*>(pMoveTask);

					if(pMoveWanderTask->GetWanderTarget(vWanderTarget))
					{
						bHasWanderTarget = true;
					}
				}
				
				CTaskMoveCrossRoadAtTrafficLights* pJayWalkRoad = rage_new CTaskMoveCrossRoadAtTrafficLights(crossingPlan.m_vCrossingStartPosition, crossingPlan.m_vCrossingEndPosition, bHasWanderTarget ? &vWanderTarget : NULL, crossingPlan.m_AssignedCrossDelayMS, crossingPlan.m_bIsTrafficLightCrossing, crossingPlan.m_bIsMidStreetCrossing, crossingPlan.m_bIsDrivewayCrossing);
				CTaskAmbientClips* pAmbientTask = rage_new CTaskAmbientClips(CTaskWander::GetCommonTaskAmbientFlags(), GetConditionalAnimsGroup(), GetActiveChosenConditionalAnimation());

				// Create the task here as we have the information here
				SetNewTask(rage_new CTaskComplexControlMovement(pJayWalkRoad, pAmbientTask));
				SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
				SetState(State_CrossRoadJayWalking);
				return true;
			}
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskWander::ComputeScanForRoadCrossings(CPed* pPed, CTaskGameTimer& scanTimer, RoadCrossRecord& OutCrossRecord, const RoadCrossScanParams& scanParams)
{
	// NOTE: This search dist is used to exclude a distance scoring function, not a true distance measure
	static float fMaxCrossingSearchDistanceScore = 50.0f;
	// For debugging allow huge scan distance score
	if( ms_bPromoteCrossingRoads )
	{
		fMaxCrossingSearchDistanceScore = 500.0f;
	}
	const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	
	// By default we mark the double back as being handled
	m_bNeedCrossingCheckPathSegDoublesBack = false;

	// Setup query
	CPathFind::CFindPedNodeParams searchParams(vPedPosition, fMaxCrossingSearchDistanceScore, &m_RoadCrossRecord.m_StartNodeAddress, &m_RoadCrossRecord.m_EndNodeAddress);
	Assert(scanParams.m_bIncludeRoadCrossings || scanParams.m_bIncludeDriveWayCrossings);
	if( scanParams.m_bIncludeRoadCrossings )
	{
		searchParams.m_SpecialFunctions.Push(SPECIAL_PED_CROSSING);
	}
	if( scanParams.m_bIncludeDriveWayCrossings )
	{
		searchParams.m_SpecialFunctions.Push(SPECIAL_PED_DRIVEWAY_CROSSING);
	}

	// Try to find a qualifying crossing node, if any
	CNodeAddress adr = ThePaths.FindPedNodeClosestToCoors(searchParams);
	// If no node is found, or the region for the node is not loaded
	if(adr.IsEmpty() || !ThePaths.IsRegionLoaded(adr))
	{
		return false;
	}

	const CPathNode* pNode = ThePaths.FindNodePointer(adr);

	// Cache node position
	Assert(pNode);
	Vector3 vNodePos = pNode->GetPos();

	// Vector from ped to node
	Vector3 vPedToNode = vNodePos - vPedPosition;

	// Distance from ped to node
	float fPedToNodeDistanceSqr = vPedToNode.Mag2();

	// Reject nodes that have no links
	if(!pNode->NumLinks())
	{
		ScanFailedIncreaseTimer(scanTimer, fPedToNodeDistanceSqr);
		return false;
	}

	// Reject nodes that are too different in the Z plane from the ped
	static dev_float fMaxDistZ_Default = 3.0f;
	static dev_float fMaxDistZ_Extra = 8.0f;

	static const s32 iNumAABB = 4;
	Vector3 vAABB[iNumAABB][2] =
	{
		Vector3(-977.0f, 3.0f, 37.0f),	Vector3(-714.f, 220.0f, 78.0f),
		Vector3(-716.0f, 10.0f, 37.0f), Vector3(-427.0f, 262.0f, 86.0f),
		Vector3(-430.0f, -19.0f, 44.0f), Vector3(-174.0f, 286.0f, 98.0f),
		Vector3(-181.0f, -92.0f, 51.0f), Vector3(413.0f, 422.0f, 140.0f)
	};

	float fMaxDistZ = fMaxDistZ_Default;
	for(s32 b=0; b<iNumAABB; b++)
	{
		if(vPedPosition.IsGreaterOrEqualThan(vAABB[b][0]) && vAABB[b][1].IsGreaterOrEqualThan(vPedPosition))
		{
			fMaxDistZ = fMaxDistZ_Extra;
			break;
		}
	}

#if __DEV
	static dev_bool bDebugWanderAABB=false;
	if(bDebugWanderAABB)
	{
		for(s32 b=0; b<iNumAABB; b++)
		{
			grcDebugDraw::BoxAxisAligned(VECTOR3_TO_VEC3V(vAABB[b][0]), VECTOR3_TO_VEC3V(vAABB[b][1]), Color_white, false, 10000);
		}
	}
#endif

	if(!rage::IsNearZero(vNodePos.z - vPedPosition.z, fMaxDistZ))
	{
		ScanFailedIncreaseTimer(scanTimer, fPedToNodeDistanceSqr);
		return false;
	}

	// Reject nodes that are too far from the ped
	if( !ms_bPromoteCrossingRoads // For debugging allow exaggerated distance away from ped
		&& fPedToNodeDistanceSqr > scanParams.m_fPedToCrossDistSqMax )
	{
		ScanFailedIncreaseTimer(scanTimer, fPedToNodeDistanceSqr);
		return false;
	}

	// Reject nodes that are too far off of the ped's forward direction
	const Vector3 vPedForwards = VEC3V_TO_VECTOR3(pPed->GetTransform().GetB());
	Vector3 vPedToNodeDir = vPedToNode;
	vPedToNodeDir.NormalizeFast();
	if( !ms_bPromoteCrossingRoads // For debugging accept all directions
		&& DotProduct(vPedToNodeDir, vPedForwards) < scanParams.m_fPedAheadDotMin )
	{
		ScanFailedIncreaseTimer(scanTimer, fPedToNodeDistanceSqr);
		return false;
	}

	//******************************************************************************************
	// Go through all the adjacent nodes for this node, and choose the one the vector to which
	// most closely matches the peds current heading - so it looks like they're using this
	// crossing to get to where they want to go.

	float fBestDot = -1.0f;
	const CPathNode * pBestLink = NULL;
	const CPathNode * linkedNodes[8];
	s32 iNumLinks = 0;

	u32 a;
	for(a = 0; a < pNode->NumLinks(); a++)
	{
		const CPathNodeLink& link = ThePaths.GetNodesLink(pNode, a);
		if(link.m_OtherNode.IsEmpty() || !ThePaths.IsRegionLoaded(link.m_OtherNode))
		{
			continue;
		}

		const CPathNode* pNextNode = ThePaths.FindNodePointer(link.m_OtherNode);
		if(pNextNode->m_1.m_specialFunction != pNode->m_1.m_specialFunction)
		{
			continue;
		}

		linkedNodes[iNumLinks++] = pNextNode;
	}

	// If this junction has diagonal links, then occasionally randomly choose which link to use.
	// This is because peds walking along a pavement will very rarely arrive at the crossing point
	// in a direction which causes them to choose the diagonal path.  Lets mix it up a bit.
	if(iNumLinks > 2 && fwRandom::GetRandomNumberInRange(0, 100) > 75)
	{
		int iRand = fwRandom::GetRandomNumberInRange(0, iNumLinks);
		pBestLink = linkedNodes[iRand];

		// Check to prevent doubling back across to where we just came from
		if( DoesLinkPosMatchLastStartPos(pPed, pBestLink) || DoesNodeAndLinkMatchLastCrossing(pPed, pNode, pBestLink) )
		{
			pBestLink = NULL;
		}
	}
	
	// Unless already set, we examine the direction to each linked node to find the one which is most
	// closely aligned with this ped's current direction of travel.
	if(!pBestLink)
	{
		for(a = 0; a < iNumLinks; a++)
		{
			// Check to prevent doubling back across to where we just came from
			// But only if we are not stuck wandering; it preferable to backtrack than to stand around stuck (url:bugstar:2069236)
			if( !scanParams.m_bWanderTaskStuck && (DoesLinkPosMatchLastStartPos(pPed, linkedNodes[a]) || DoesNodeAndLinkMatchLastCrossing(pPed, pNode, linkedNodes[a]) ) )
			{
				continue;
			}

			Vector3 vNextPos = linkedNodes[a]->GetPos();

			Vector3 vToLink = vNextPos - vPedPosition;
			vToLink.NormalizeFast();

			float fDot = DotProduct(vPedForwards, vToLink);
			if(fDot > fBestDot)
			{
				fBestDot = fDot;
				pBestLink = linkedNodes[a];
			}
		}
	}

	if(!pBestLink)
	{
		return false;
	}

	// Check if the start node we found is a driveway type
	if( pNode->m_1.m_specialFunction == SPECIAL_PED_DRIVEWAY_CROSSING )
	{
		OutCrossRecord.m_bIsDrivewayCrossing = true;
	}

#if __DEV
	// For testing behaviors in the absence of actual SPECIAL_PED_DRIVEWAY_CROSSING nodes
	// @TODO: Remove this once game data has these types of crossings (dmorales)
	static bool s_bDEBUG_ALL_CROSSINGS_AS_DRIVEWAY = false;
	if( s_bDEBUG_ALL_CROSSINGS_AS_DRIVEWAY )
	{
		OutCrossRecord.m_bIsDrivewayCrossing = true;
	}
#endif // __DEV

	// Check with the ped crossing reservation system
	if( !OutCrossRecord.m_bIsDrivewayCrossing && !ThePaths.IsPedCrossReservationAvailable(pNode->m_address, pBestLink->m_address) )
	{
		// We can't use the crossing if the reservation request fails.
		return false;
	}

	taskAssert(pBestLink);
	Vector3 vLinkPos = pBestLink->GetPos();

	// If we were asked to check crossing direction
	if( scanParams.m_bCheckCrossDir )
	{
		// Compare the candidate crossing with the given cross direction
		Vector3 vNextCrossingDir = vLinkPos - vNodePos;
		vNextCrossingDir.NormalizeFast();
		float fDot = DotProduct(scanParams.m_vPrevCrossDir, vNextCrossingDir);
		if( fDot < scanParams.m_fCrossingAheadDotMin )
		{
			return false;
		}
	}

#if __DEV
	if(CPedDebugVisualiser::GetDebugDisplay() != CPedDebugVisualiser::eOff)
	{
		grcDebugDraw::Line(vPedPosition, vNodePos, Color_yellow, Color_yellow2);
	}
#endif // __DEV

	// Set out parameter for mid street crossing
	// @TODO: pBestLink is not NULL at this point, so this is never true!? (dmorales)
	OutCrossRecord.m_bIsMidStreetCrossing = (pNode->m_2.m_density == 0 && pBestLink == 0);

	//**************************************************************************************************
	// If this is a mid-street crossing then check to see if any nearby peds are also waiting to cross
	// the road here.  We will not allow more than N peds to do a mid-road crossing at any time, as
	// bugs have been reported about too many peds waiting by the roadside.

	static dev_s32 iMaxNumberOfMidRoadPeds = 1;
	static dev_float fNearbyCrossDistanceSqr = 15.0f * 15.0f;
	if(OutCrossRecord.m_bIsMidStreetCrossing)
	{
		s32 iNumPedsCrossing = 0;
		const CEntityScannerIterator entityList = pPed->GetPedIntelligence()->GetNearbyPeds();
		for(const CEntity* pEnt = entityList.GetFirst(); pEnt; pEnt = entityList.GetNext())
		{
			const CPed* pOtherPed = static_cast<const CPed*>(pEnt);
			if(pOtherPed != pPed &&
				(VEC3V_TO_VECTOR3(pOtherPed->GetTransform().GetPosition()) - vPedPosition).Mag2() < fNearbyCrossDistanceSqr &&
				pOtherPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_MOVE_CROSS_ROAD_AT_TRAFFIC_LIGHTS))
			{
				iNumPedsCrossing++;
				if(iNumPedsCrossing >= iMaxNumberOfMidRoadPeds)
				{
					break;
				}
			}
		}

		// Too many peds crossing here
		if(iNumPedsCrossing >= iMaxNumberOfMidRoadPeds)
		{
			return false;
		}
	}

	// If requested, look to see if there are any traffic lights in
	// the immediate vicinity.. If so, then when we create the CTaskComplexMoveCrossRoads task
	// it will use there to decide when to cross instead of monitoring the traffic itself..
	if( scanParams.m_bCheckForTrafficLight && !OutCrossRecord.m_bIsDrivewayCrossing )
	{
		CObject* pTrafficLight = NULL;
		const float fSphereRadius = 30.0f;
		fwIsSphereIntersecting sp(spdSphere(RCC_VEC3V(vNodePos), ScalarV(fSphereRadius)));
		CGameWorld::ForAllEntitiesIntersecting(&sp, IsTrafficLightCB, &pTrafficLight, ENTITY_TYPE_MASK_OBJECT, SEARCH_LOCATION_EXTERIORS);
		if( pTrafficLight != NULL )
		{
			// Mark the out parameter as crossing at a traffic light
			OutCrossRecord.m_bIsTrafficLightCrossing = true;
		}
	}

	// Randomize the crossing points a bit, based upon the radius value in each node

	// Obbe: It looks like Width was always 0 (not read in from 3DSmax) Let me know if you need this reinstated, James.
	const float fStartRadius = 1.0f;  // ((float)pNode->Width) + 1.0f;
	const float fEndRadius   = 1.0f;  // ((float)pBestLink->Width) + 1.0f;

	// Also randomize them in the direction which is the tangent to the crossing direction
	Vector3 vToNext = vLinkPos - vNodePos;
	vToNext.NormalizeFast();
	const Vector3 vRight = CrossProduct(vToNext, ZAXIS);
	
	Vector3 vRandStart = (vToNext * fwRandom::GetRandomNumberInRange(-fStartRadius, fStartRadius));
	Vector3 vRandEnd   = (vToNext * fwRandom::GetRandomNumberInRange(-fEndRadius, fEndRadius));
	
	vRandStart += (vRight * fwRandom::GetRandomNumberInRange(-fStartRadius, fStartRadius));
	vRandEnd   += (vRight * fwRandom::GetRandomNumberInRange(-fEndRadius,   fEndRadius));
	
	Vector3 vPosOffset;
	if (CTaskMoveCrossRoadAtTrafficLights::GetTunables().m_bTrafficLightPositioning && OutCrossRecord.m_bIsTrafficLightCrossing && !OutCrossRecord.m_bIsDrivewayCrossing)
	{
		if(pPed->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodEntityScanning) && !m_bPerformedLowEntityScan)
		{
#if DEBUG_DRAW
			if(CTaskMoveCrossRoadAtTrafficLights::GetTunables().m_bDebugRender)
			{
				grcDebugDraw::Sphere(vPedPosition,0.2f,Color_blue,true,1);
			}
#endif //DEBUG_DRAW

			//Force an update of the entity scanner for this ped.
			pPed->GetPedAiLod().SetBlockedLodFlag(CPedAILod::AL_LodEntityScanning);
			m_ScanForRoadCrossingsTimer.Set(fwTimer::GetTimeInMilliseconds(), 30);	
			m_bPerformedLowEntityScan = true;

			// Mark the double back as still needing to be handled!
			m_bNeedCrossingCheckPathSegDoublesBack = true;

			return false;
		}

		m_bPerformedLowEntityScan = false;

		if (ComputeScanForTrafficLights(pPed, vPosOffset, vNodePos, vLinkPos,15.0f))
		{
			vRandStart = (vToNext * vPosOffset.x) + (vRight * vPosOffset.y);
		}
		else
		{
			return false;
		}
	}

	// Computed crossing start and end, set out parameters
	//*******************************************************************************************

	OutCrossRecord.m_vCrossingStartPosition = vNodePos + vRandStart;
	OutCrossRecord.m_vCrossingEndPosition   = vLinkPos + vRandEnd;

	OutCrossRecord.m_StartNodeAddress.Set(pNode->m_address);
	OutCrossRecord.m_EndNodeAddress.Set(pBestLink->m_address);

	//*******************************************************************************************

	// Ensure that we do not use a crossing which starts or ends in an area switched off for random peds
	for(s32 s=0; s<CPathServer::GetNumPathRegionSwitches(); s++)
	{
		const TPathRegionSwitch & pathSwitch = CPathServer::GetPathRegionSwitch(s);
		if(pathSwitch.m_Switch == SWITCH_NAVMESH_DISABLE_AMBIENT_PEDS)
		{
			if( (OutCrossRecord.m_vCrossingStartPosition.IsGreaterThan(pathSwitch.m_vMin) && pathSwitch.m_vMax.IsGreaterThan(OutCrossRecord.m_vCrossingStartPosition)) ||
				(OutCrossRecord.m_vCrossingEndPosition.IsGreaterThan(pathSwitch.m_vMin) && pathSwitch.m_vMax.IsGreaterThan(OutCrossRecord.m_vCrossingEndPosition)) )
			{
				return false;
			}
		}
	}

	return true;
}

void CTaskWander::ScanFailedIncreaseTimer(CTaskGameTimer& timerToAdjust, float fDistanceSqr)
{
	static dev_float fScanDelayTimePerMeter = 10;
	s32 fDelayTime = (s32)(fDistanceSqr * fScanDelayTimePerMeter);
	fDelayTime = rage::Min(15000, rage::Max(fDelayTime,2000));
	timerToAdjust.Set(fwTimer::GetTimeInMilliseconds(),fDelayTime);
};

bool CTaskWander::ComputeScanForTrafficLights(CPed* pPed, Vector3 &vStartPos, Vector3 &vNodePos, Vector3 &vLinkPos, float fNearbyCrossDistance)
{
	const u32 iMaxNumberOfTrafficLightPeds = CTaskMoveCrossRoadAtTrafficLights::GetTunables().m_iMaxPedsAtTrafficLights;
	const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	const u8 iNumCrossingPositions = (u8)CTaskMoveCrossRoadAtTrafficLights::GetTunables().m_WaitingOffsets.size();

	const atArray< CTaskMoveCrossRoadAtTrafficLights::WaitingOffset > & vPositions = CTaskMoveCrossRoadAtTrafficLights::GetTunables().m_WaitingOffsets;

	atFixedArray< bool,10 > bAvaliablePositions(iNumCrossingPositions);
	memset(bAvaliablePositions.GetElements(),true,sizeof(bool)*iNumCrossingPositions); 

	//Update available positions
	Vector3 vToNext = vLinkPos - vNodePos;
	vToNext.NormalizeFast();
	const Vector3 vRight = CrossProduct(vToNext, ZAXIS);

	s32 iNumPedsCrossing = 0;
	const CEntityScannerIterator entityList = pPed->GetPedIntelligence()->GetNearbyPeds();
	for(const CEntity* pEnt = entityList.GetFirst(); pEnt; pEnt = entityList.GetNext())
	{
		const CPed* pOtherPed = static_cast<const CPed*>(pEnt);
		const Vector3 vOtherPedPosition = VEC3V_TO_VECTOR3(pOtherPed->GetTransform().GetPosition());
 
		if((vOtherPedPosition - vPedPosition).Mag2() < rage::square(fNearbyCrossDistance))
		{

			bool bUsingLights = false;
			CTaskMoveCrossRoadAtTrafficLights* pTask = static_cast<CTaskMoveCrossRoadAtTrafficLights*>(pOtherPed->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_CROSS_ROAD_AT_TRAFFIC_LIGHTS));
			s32 iState = -1;
			if (pTask)
			{
				iState = pTask->GetState();
				if(iState < CTaskMoveCrossRoadAtTrafficLights::State_CrossingRoad)
				{	
					bUsingLights = true;
				}
			}

			if (!bUsingLights)
			{
				continue;
			}

#if DEBUG_DRAW
			if(CTaskMoveCrossRoadAtTrafficLights::GetTunables().m_bDebugRender)
			{
				grcDebugDraw::Line(vOtherPedPosition,vPedPosition,Color_green,Color_green,60);
			}
#endif //DEBUG_DRAW

			++iNumPedsCrossing;

			//Too many peds using this crossing, abort.
			if(iNumPedsCrossing >= iMaxNumberOfTrafficLightPeds )
			{
				return false;
			}

			//Update which positions are available
			Vector3 vWaitingPos = vOtherPedPosition;
			if( iState == CTaskMoveCrossRoadAtTrafficLights::State_WalkingToCrossingPoint)
			{
				vWaitingPos = pTask->GetStartPos();
			}
			const Vector3 vWaitingNodeOffset = vWaitingPos - vNodePos;
			
			for( int i = 0; i < iNumCrossingPositions; ++i )
			{
				if(bAvaliablePositions[i])
				{
					const Vector3 offsetA = vToNext * vPositions[i].m_Pos.x;
					const Vector3 offsetB = vRight * vPositions[i].m_Pos.y;
	
					if ((vWaitingNodeOffset - (offsetA + offsetB)).Mag2() < CTaskMoveCrossRoadAtTrafficLights::GetTunables().m_fMinDistanceBetweenPeds)
					{
						bAvaliablePositions[i] = false;
					}
				}
			}			
		}
#if DEBUG_DRAW
		else
		{
			if(CTaskMoveCrossRoadAtTrafficLights::GetTunables().m_bDebugRender)
			{
				grcDebugDraw::Line(vOtherPedPosition,vPedPosition,Color_red,Color_red,60);
			}
		}
#endif //DEBUG_DRAW
	}

#if DEBUG_DRAW
	//Debug render them
	if(CTaskMoveCrossRoadAtTrafficLights::GetTunables().m_bDebugRender)
	{
		bool bFirstAvailablePositionFound = false;
		for( int i = 0; i < iNumCrossingPositions; i++ )
		{
			const Vector3 offsetA = vToNext * vPositions[i].m_Pos.x;
			const Vector3 offsetB = vRight * vPositions[i].m_Pos.y;

			if ( !bFirstAvailablePositionFound && bAvaliablePositions[i])
			{
				grcDebugDraw::Sphere(vNodePos + offsetA + offsetB,0.4f,Color_purple,true,200);
				bFirstAvailablePositionFound = true;
			}
			else if (bAvaliablePositions[i])
			{
				grcDebugDraw::Sphere(vNodePos + offsetA + offsetB,0.4f,Color_green,true,200);
			}
			else
			{
				grcDebugDraw::Sphere(vNodePos + offsetA + offsetB,0.4f,Color_red,true,200);
			}
		}
	}
#endif //DEBUG_DRAW

	//Pick an available position
	for( int i = 0; i < iNumCrossingPositions; i++ )
	{
		if(bAvaliablePositions[i])
		{
			vStartPos = vPositions[i].m_Pos;
			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskWander::IsTrafficLightCB(CEntity* pEntity, void* pData)
{
	taskAssert(pEntity->GetIsTypeObject());
	CObject* pObj = static_cast<CObject*>(pEntity);
	CBaseModelInfo* pModelInfo = pObj->GetBaseModelInfo();
	if(pModelInfo->TestAttribute(MODEL_ATTRIBUTE_IS_TRAFFIC_LIGHT))
	{
		// Found traffic light
		*reinterpret_cast<CObject**>(pData) = pObj;
		return false;
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskWander::ProcessCriminalOpportunities()
{
  // Cache the ped
  CPed* pPed = GetPed();

  //check personality criminal component 
  if ( pPed->CheckCriminalityFlags( CF_JACKING )  BANK_ONLY( || CRandomEventManager::GetTunables().m_ForceCrime)) 
  {
		if ( CRandomEventManager::GetInstance().CanCreateEventOfType( RC_PED_STEAL_VEHICLE ) )
		{
			CStealVehicleCrime* pStealVehicleCrime = static_cast< CStealVehicleCrime*>(CRandomEventManager::GetCrimeInfo(DCT_Jacking));
			AssertMsg( pStealVehicleCrime, "CStealVehicleCrime can not be found\n" );

			CVehicle* pVehicle = pStealVehicleCrime->IsApplicable( pPed );

			if ( pVehicle != NULL )
			{
				SetNewTask( pStealVehicleCrime->GetPrimaryTask(pVehicle) );
				
				SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);

				CRandomEventManager::GetInstance().SetEventStarted(RC_PED_STEAL_VEHICLE);

				SetState(State_Crime);
				return true;
			}
		}
  }
    
  return false;
}

// Does the given path node end position match the last road cross start, if any?
bool CTaskWander::DoesLinkPosMatchLastStartPos(CPed* pPed, const CPathNode* pPathNodeEnd) const
{
	taskAssert(pPed);
	taskAssert(pPathNodeEnd);
	if(pPed->GetPedIntelligence())
	{
		const float fCrossingPointEquivalenceDistance = 2.0f * 2.0f;

		Vector3 vTestEndPos;
		pPathNodeEnd->GetCoors(vTestEndPos);		
		const Vector3& vPrevStartPos = pPed->GetPedIntelligence()->GetCrossRoadStartPos();
		if( !vPrevStartPos.IsZero() && vTestEndPos.Dist2(vPrevStartPos) <= fCrossingPointEquivalenceDistance )
		{
			return true;
		}
	}
	return false;
}

// Does the given path node start and end match the last road cross start and end, if any?
bool CTaskWander::DoesNodeAndLinkMatchLastCrossing(CPed* pPed, const CPathNode* pPathNodeStart, const CPathNode* pPathNodeEnd) const
{
	taskAssert(pPed);
	taskAssert(pPathNodeStart);
	taskAssert(pPathNodeEnd);
	if(pPed->GetPedIntelligence())
	{
		const float fCrossingPointEquivalenceDistance = 2.0f * 2.0f;
		
		Vector3 vTestStartPos;
		pPathNodeStart->GetCoors(vTestStartPos);
		Vector3 vTestEndPos;
		pPathNodeEnd->GetCoors(vTestEndPos);
		const Vector3& vPrevStartPos = pPed->GetPedIntelligence()->GetCrossRoadStartPos();
		const Vector3& vPrevEndPos = pPed->GetPedIntelligence()->GetCrossRoadEndPos();
		if( (!vPrevStartPos.IsZero() && vTestStartPos.Dist2(vPrevStartPos) <= fCrossingPointEquivalenceDistance)
			&& (!vPrevEndPos.IsZero() && vTestEndPos.Dist2(vPrevEndPos) <= fCrossingPointEquivalenceDistance) )
		{
			return true;
		}
	}
	return false;
}

const CConditionalAnimsGroup* CTaskWander::GetConditionalAnimsGroup() const
{
	if(m_pOverrideConditionalAnimsGroup)
	{
		// Use the animations the user requested.
		return m_pOverrideConditionalAnimsGroup;
	}

	const CTaskDataInfo& taskData = GetPed()->GetTaskData();
	u32 hash = taskData.GetTaskWanderConditionalAnimsGroupHash();

	// This check isn't necessary in the long run, it's only here now because
	// I didn't have much success in setting a default value of the string in the
	// metadata, if people run latest code without getting latest data.
	// Should be OK to remove later.
	if(!hash)
	{
		hash = ATSTRINGHASH("WANDER", 0x4cd31c35);
	}

	const CConditionalAnimsGroup* pGrp = CONDITIONALANIMSMGR.GetConditionalAnimsGroup(hash);
#if __BANK
	taskAssertf(pGrp, "Failed to find conditional anims %s, for CTaskWander.", taskData.GetTaskWanderConditionalAnimsGroup());
#endif

	return pGrp;
}

// If TaskAmbientClips is running as a subtask, return its chosen animation.  Otherwise return -1.
s32 CTaskWander::GetActiveChosenConditionalAnimation() const
{
	CTaskAmbientClips* pAmbientTask = static_cast<CTaskAmbientClips*>(FindSubTaskOfType(CTaskTypes::TASK_AMBIENT_CLIPS));
	if (pAmbientTask)
	{
		if (GetConditionalAnimsGroup() == pAmbientTask->GetConditionalAnimsGroup())
		{
			return pAmbientTask->GetConditionalAnimChosen();
		}
	}
	return -1;
}

CTaskAmbientClips* CTaskWander::CreateAmbientClipsTask()
{
	s16 iAmbientFlags = CTaskAmbientClips::Flag_PlayIdleClips | CTaskAmbientClips::Flag_BlockIdleClipsAfterGunshots |
		CTaskAmbientClips::Flag_CanSayAudioDuringLookAt;
	if(m_bCreateProp)
	{
		// Attempt to create a prop for the ped
		iAmbientFlags |= CTaskAmbientClips::Flag_PickPermanentProp;
		// Only create once
		m_bCreateProp = false;
	}

	if (m_bForceCreateProp)
	{
		// Tell ambient clips to ignore probability during prop creation.
		iAmbientFlags |= CTaskAmbientClips::Flag_ForcePropInPickPermanent;
		// Only do this once.
		m_bForceCreateProp = false;
	}

	iAmbientFlags |= CTaskAmbientClips::Flag_PlayBaseClip;

	// Set up ambient clips task:
	CTaskAmbientClips* pAmbientTask = NULL;
	// If we have a prop helper from a previous state
	if( m_pPropHelper )
	{
		// use the same anim index
		pAmbientTask = rage_new CTaskAmbientClips(iAmbientFlags, GetConditionalAnimsGroup(), m_iAnimIndexForExistingProp);
	}
	else // general case
	{
		pAmbientTask = rage_new CTaskAmbientClips(iAmbientFlags, GetConditionalAnimsGroup(), m_iOverrideAnimIndex);
	}

	return pAmbientTask;
}

bool CTaskWander::ShouldHolsterWeapon() const
{
	//Ensure the flag is set.
	if(!m_bHolsterWeapon)
	{
		return false;
	}
	
	//Ensure the weapon manager is valid.
	const CPedWeaponManager* pWeaponManager = GetPed()->GetWeaponManager();
	if(!pWeaponManager)
	{
		return false;
	}
	
	//Ensure the weapon info is valid.
	const CWeaponInfo* pWeaponInfo = pWeaponManager->GetEquippedWeaponInfo();
	if(!pWeaponInfo)
	{
		return false;
	}

	//Ensure the weapon is not unarmed.
	if( pWeaponInfo->GetIsUnarmed() )
	{
		return false;
	}

	//Ensure the weapon is not a prop.
	const u32 uHash = pWeaponInfo->GetHash();
	if(uHash == OBJECTTYPE_OBJECT)
	{
		return false;
	}
	
	return true;
}

// Determine if the Ped should be allowed to play crosswalk idling animations.
bool CTaskWander::ShouldAllowPedToPlayCrosswalkIdles()
{
	if (m_bIdlingAtCrossWalk)
	{
		return false;
	}

	if(GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
	{
		CTaskComplexControlMovement* pComplexControlTask = static_cast<CTaskComplexControlMovement*>(GetSubTask());
		if(pComplexControlTask->GetSubTask() && pComplexControlTask->GetSubTask()->GetTaskType() == CTaskTypes::TASK_AMBIENT_CLIPS)
		{
			CTaskAmbientClips* pAmbientClipsTask = static_cast<CTaskAmbientClips*>(pComplexControlTask->GetSubTask());
			const CPropManagementHelper* pPropHelper = pAmbientClipsTask->GetPropHelper();
			if(pPropHelper && pPropHelper->IsHoldingProp())
			{
				return false;
			}
		}
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////
// CTaskWanderInArea functions
////////////////////////////////////////////////////////////////////////////////

CTaskWanderInArea::Tunables CTaskWanderInArea::sm_Tunables;
IMPLEMENT_DEFAULT_TASK_TUNABLES(CTaskWanderInArea, 0x318ae92e);

CTaskWanderInArea::CTaskWanderInArea(const float fMoveBlendRatio, const float fWanderRadius, const Vector3 &vWanderCenterPos, float fMinWaitTime, float fMaxWaitTime)
: m_fMinWaitTime(fMinWaitTime)
, m_fMaxWaitTime(fMaxWaitTime)
{
	taskAssert(m_fMinWaitTime <= m_fMaxWaitTime);
	Init(fMoveBlendRatio, fWanderRadius, vWanderCenterPos);
	SetInternalTaskType(CTaskTypes::TASK_WANDER_IN_AREA);
}

//////////////////////////////////////////////////////////////////////////

CTaskWanderInArea::FSM_Return CTaskWanderInArea::ProcessPreFSM()
{
	// Grab our ped
	CPed* pPed = GetPed();

	// Allow certain lod modes
	pPed->GetPedAiLod().ClearBlockedLodFlag(CPedAILod::AL_LodPhysics);
	pPed->GetPedAiLod().ClearBlockedLodFlag(CPedAILod::AL_LodMotionTask);

	// Check shocking events
	pPed->GetPedIntelligence()->SetCheckShockFlag(true);

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTaskWanderInArea::FSM_Return CTaskWanderInArea::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		FSM_State(State_Init)
			FSM_OnUpdate
				return StateInit();
		FSM_State(State_Wait)
			FSM_OnEnter
				return StateWait_OnEnter();
			FSM_OnUpdate
				return StateWait_OnUpdate();
		FSM_State(State_Wander)
			FSM_OnEnter
				return StateWander_OnEnter();
			FSM_OnUpdate
				return StateWander_OnUpdate();
		FSM_State(State_AchieveHeading)
				FSM_OnEnter
				return StateAchieveHeading_OnEnter();
			FSM_OnUpdate
				return StateAchieveHeading_OnUpdate();
	FSM_End
}

//////////////////////////////////////////////////////////////////////////

void CTaskWanderInArea::DoAbort(const AbortPriority UNUSED_PARAM(priority), const aiEvent*  UNUSED_PARAM(pEvent))
{
}

//////////////////////////////////////////////////////////////////////////

void CTaskWanderInArea::Init(const float fMoveBlendRatio, const float fWanderRadius, const Vector3 &vWanderCenterPos)
{
	m_fWaitTimer = 0.0f;
	m_fMoveBlendRatio = fMoveBlendRatio;
	m_fWanderRadius = fWanderRadius;
	m_vWanderCenterPos = vWanderCenterPos;
	m_vMoveToPos = m_vWanderCenterPos;
}

//////////////////////////////////////////////////////////////////////////

CTaskWanderInArea::FSM_Return CTaskWanderInArea::StateInit()
{
	// For now immediately go into wander
	m_fWaitTimer = 0.0f;
	SetState(State_Wait);

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTaskWanderInArea::FSM_Return CTaskWanderInArea::StateWait_OnEnter()
{
	m_fWaitTimer = fwRandom::GetRandomNumberInRange(m_fMinWaitTime, m_fMaxWaitTime);
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTaskWanderInArea::FSM_Return CTaskWanderInArea::StateWait_OnUpdate()
{
	// Count down our wait timer
	m_fWaitTimer -= fwTimer::GetTimeStep();

	// Change our state because the timer has run out
	if (m_fWaitTimer < 0.0f)
	{
		// Get our random move to position based on our center position and the radius
		m_vMoveToPos = m_vWanderCenterPos + Vector3(fwRandom::GetRandomNumberInRange(-m_fWanderRadius, m_fWanderRadius), 
			fwRandom::GetRandomNumberInRange(-m_fWanderRadius, m_fWanderRadius),
			0.0f);

		// If target is very close to the ped, just achieve heading to the target instead of moving to Wander
		bool bAchieveHeading = false;
		CPed * pPed = GetPed();
		if (pPed)
		{
			Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			if ( (vPedPos - m_vMoveToPos).XYMag() < 1.0f )
			{
				bAchieveHeading = true;
			}
		}

		if (bAchieveHeading)
		{
			SetState(State_AchieveHeading);
		}
		else
		{
			SetState(State_Wander);
		}
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTaskWanderInArea::FSM_Return CTaskWanderInArea::StateWander_OnEnter()
{
	// add some randomness to the move blend
	float fRandomMoveBlend = fwRandom::GetRandomNumberInRange(0.0f, 0.25f);
	m_fWanderMoveBlendRatio = m_fMoveBlendRatio + fRandomMoveBlend;

	// Create a move task to our target vector
	CTaskMoveFollowNavMesh* pMoveFollowNavMesh = rage_new CTaskMoveFollowNavMesh(m_fWanderMoveBlendRatio, m_vMoveToPos, .1f);
	SetNewTask(rage_new CTaskComplexControlMovement(pMoveFollowNavMesh));

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTaskWanderInArea::FSM_Return CTaskWanderInArea::StateWander_OnUpdate()
{
	if (GetSubTask()->GetTaskType() == CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT &&	// Check if we are in the follow navmesh task
		(static_cast<CTaskComplexControlMovement*>(GetSubTask()))->GetBackupCopyOfMovementSubtask()->GetTaskType() == CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH )
	{
		// Get the movement task
		CTaskMoveFollowNavMesh* pNavmeshTask = (CTaskMoveFollowNavMesh*)((CTaskComplexControlMovement*)GetSubTask())->GetRunningMovementTask(GetPed());
		if (pNavmeshTask && pNavmeshTask->IsUnableToFindRoute())
		{
			// Go back to wander (this *should* makes us go back through OnEnter)
			SetFlag(aiTaskFlags::RestartCurrentState);
		}
	}
	
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		// Wait around for a bit before going back to wandering
		SetState(State_Wait);
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTaskWanderInArea::FSM_Return CTaskWanderInArea::StateAchieveHeading_OnEnter()
{
	CPed * pPed = GetPed();
	Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	const float fHdg = fwAngle::GetRadianAngleBetweenPoints(m_vMoveToPos.x, m_vMoveToPos.y, vPedPos.x, vPedPos.y);

	CTaskMoveAchieveHeading * pTaskHeading = rage_new CTaskMoveAchieveHeading(fHdg);
	SetNewTask(rage_new CTaskComplexControlMovement(pTaskHeading));

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTaskWanderInArea::FSM_Return CTaskWanderInArea::StateAchieveHeading_OnUpdate()
{
	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		// Wait around for a bit before going back to wandering
		SetState(State_Wait);
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

#if !__FINAL
void CTaskWanderInArea::Debug() const
{
	switch(GetState())
	{
	case State_Wander:
		break;
	default:
		break;
	}

	// Base class
	CTask::Debug();
}

////////////////////////////////////////////////////////////////////////////////

const char * CTaskWanderInArea::GetStaticStateName( s32 iState )
{
	static const char* aStateNames[] = 
	{
		"Init",
		"Wait",
		"Wander",
		"AchieveHeading"
	};

	return aStateNames[iState];
}
#endif // !__FINAL
