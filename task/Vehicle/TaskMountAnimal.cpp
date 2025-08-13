
//-------------------------------------------------------------------------
// TaskMountAnimal.cpp
//
// Created on: 14-04-2011
//
// Author: Bryan Musson
//-------------------------------------------------------------------------

// File header
#include "Task/Vehicle/TaskMountAnimal.h"

#if ENABLE_HORSE

// Game header
#include "animation/MovePed.h"
#include "Camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "event/ShockingEvents.h"
#include "modelinfo/PedModelInfo.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Peds/Horse/horseComponent.h"
#include "Scene/World/GameWorld.h"
#include "task/Combat/TaskCombat.h"
#include "Task/Motion/Locomotion/TaskHorseLocomotion.h"
#include "Task/Motion/TaskMotionBase.h"
#include "Task/Movement/TaskNavMesh.h"
#include "Task/Movement/TaskMoveFollowLeader.h"
#include "Task/Movement/TaskSeekEntity.h"
#include "task/Response/TaskFlee.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Task/Vehicle/TaskGoToVehicleDoor.h"
#include "task/Vehicle/TaskMountAnimal.h"
#include "Task/Vehicle/TaskRideHorse.h"
#include "Task/Vehicle/TaskVehicleBase.h"
#include "Vehicles/Metadata/VehicleLayoutInfo.h"
#include "Vehicles/Metadata/VehicleEntryPointInfo.h"
#include "Vehicles/Metadata/VehicleSeatInfo.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

static bool ENABLE_RDR_MOUNT = true;

//-------------------------------------------------------------------------
// Base class for all mount tasks
//-------------------------------------------------------------------------

CTaskMountFSM::CTaskMountFSM(CPed* pMount, const SeatRequestType iSeatRequestType, const s32 iSeat, const VehicleEnterExitFlags& iRunningFlags, const s32 iTargetEntryPoint)
: m_pMount(pMount)
, m_iTargetEntryPoint(iTargetEntryPoint)
, m_iRunningFlags(iRunningFlags)
, m_iSeatRequestType(iSeatRequestType)
, m_iSeat(iSeat)
{
}

CTaskMountFSM::~CTaskMountFSM()
{
}

void CTaskMountFSM::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	//Grab the animal info.
	CClonedMountFSMInfoBase* pMountInfo = dynamic_cast<CClonedMountFSMInfoBase *>(pTaskInfo);
	Assert(pMountInfo);

	//Sync the state.
	m_pMount			= pMountInfo->GetMount();
	m_iTargetEntryPoint = pMountInfo->GetTargetEntryPoint();
	m_iRunningFlags     = pMountInfo->GetRunningFlags();
	m_iSeatRequestType  = pMountInfo->GetSeatRequestType();
	m_iSeat             = pMountInfo->GetSeat();

	//Call the base class version.
	CTaskFSMClone::ReadQueriableState(pTaskInfo);
}

bank_float CTaskMountAnimal::ms_fDistanceToEvaluateSeats = 15.0f;

CTaskMountAnimal::CTaskMountAnimal(CPed* pMount, SeatRequestType iRequest, s32 iTargetSeatIndex, VehicleEnterExitFlags iRunningFlags, float fTimer, const float fMoveBlendRatio) 
: CTaskMountFSM(pMount, iRequest, iTargetSeatIndex, iRunningFlags, -1)
, m_iCurrentSeatIndex(-1)
, m_fTimer(fTimer)
, m_fSeatCheckInterval(-1.0f)
, m_pJackedPed(NULL)
, m_iNetworkWaitState(-1)
, m_iLastCompletedState(-1)
, m_fClipBlend(-1.0f)
, m_TaskTimer(0.0f)
, m_bTimerRanOut(false)
, m_bWaitForSeatToBeEmpty(false)
, m_bWaitForSeatToBeOccupied(false)
, m_bWarping(false)
, m_fMoveBlendRatio(fMoveBlendRatio)
, m_ClipsReady(false)
, m_bQuickMount(false)
{
	SetInternalTaskType(CTaskTypes::TASK_MOUNT_ANIMAL);
}

CTaskMountAnimal::~CTaskMountAnimal()
{

}

s32 CTaskMountAnimal::GetDefaultStateAfterAbort() const
{
	if (IsFlagSet(CVehicleEnterExitFlags::ResumeIfInterupted))
	{
		return State_Start;
	}
	return State_Finish;
}

void CTaskMountAnimal::GetOverriddenGameplayCameraSettings(tGameplayCameraSettings& settings) const
{
	//Blend into an appropriate camera when mounting an animal.

	const CPed *pPed = GetPed(); //Get the ped ptr.
	if(!pPed->IsLocalPlayer())
	{
		return;
	}

	CPed* pMount = pPed->GetMyMount();
	if(!pMount)
	{
		return;
	}

	settings.m_CameraHash = CTaskMotionRideHorse::GetCameraHash();
	settings.m_InterpolationDuration = CTaskMotionRideHorse::GetCameraInterpolationDuration();
	settings.m_AttachEntity = pMount;
	settings.m_MaxOrientationDeltaToInterpolate = PI;
}

bool CTaskMountAnimal::ShouldAbort(const AbortPriority priority, const aiEvent* UNUSED_PARAM(pEvent))
{
	// canceling this event cancels the animation getting onto the horse
	// If you have a case where it is valid to cancel this, add it here
	// In most cases we don't want to cancel
	return (priority == ABORT_PRIORITY_IMMEDIATE);
}

void CTaskMountAnimal::DoAbort(const AbortPriority UNUSED_PARAM(priority), const aiEvent* UNUSED_PARAM(pEvent))
{
	if (m_moveNetworkHelper.IsNetworkAttached())
	{
		GetPed()->GetMovePed().ClearTaskNetwork(m_moveNetworkHelper, 0.0f);
	}	
}

void CTaskMountAnimal::CleanUp()
{
	CPed* pPed = GetPed();

	if (!pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_OnMount))
	{
		if (pPed->GetAttachState())
		{
			pPed->DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS);
		}
	}
	else
	{
		//?
	}

	if (m_moveNetworkHelper.IsNetworkActive())
	{
		if (m_moveNetworkHelper.IsNetworkAttached())
		{
			pPed->GetMovePed().ClearTaskNetwork(m_moveNetworkHelper, NORMAL_BLEND_DURATION);
		}
	}
}

CTask::FSM_Return CTaskMountAnimal::ProcessPreFSM()
{
	//Ensure the mount is valid.
	if(!m_pMount || !m_pMount->CanBeMounted())
	{
		return FSM_Quit;
	}

	//Stream the clip sets.
	StreamClipSets();

	// If a timer is set, count down
	if (m_fTimer > 0.0f)
	{
		m_fTimer -= fwTimer::GetTimeStep();
		if (m_fTimer <= 0.0f)
		{
			m_bTimerRanOut = true;
		}
	}

	GetPed()->SetPedResetFlag( CPED_RESET_FLAG_ProcessPhysicsTasks, true );

	// We need to make sure to jack the ped if it is local to us
	if (GetState() >= State_JackPed && GetState() < State_WaitForNetworkSync)
	{
		DoJackPed();
	}

	return FSM_Continue;
}	

CTask::FSM_Return CTaskMountAnimal::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();	

		FSM_State(State_ExitMount)
			FSM_OnEnter
			ExitMount_OnEnter();
		FSM_OnUpdate
			return ExitMount_OnUpdate();

		FSM_State(State_GoToMount)
			FSM_OnEnter
				GoToMount_OnEnter();
			FSM_OnUpdate
				return GoToMount_OnUpdate();

		FSM_State(State_PickSeat)
			FSM_OnUpdate
				return PickSeat_OnUpdate();

		FSM_State(State_GoToSeat)
			FSM_OnEnter
				GoToSeat_OnEnter();
			FSM_OnUpdate
				return GoToSeat_OnUpdate();

		FSM_State(State_ReserveSeat)
			FSM_OnUpdate
				return ReserveSeat_OnUpdate();

		FSM_State(State_Align)
			FSM_OnEnter
				Align_OnEnter();
			FSM_OnUpdate
				return Align_OnUpdate();
 
		FSM_State(State_WaitForGroupLeaderToEnter)
			FSM_OnUpdate
				return WaitForGroupLeaderToEnter_OnUpdate();

		FSM_State(State_JackPed)
			FSM_OnEnter
				JackPed_OnEnter();
			FSM_OnUpdate
				return JackPed_OnUpdate();

		FSM_State(State_EnterSeat)
			FSM_OnEnter
				EnterSeat_OnEnter();
			FSM_OnUpdate
				return EnterSeat_OnUpdate();

		FSM_State(State_SetPedInSeat)
			FSM_OnEnter
				SetPedInSeat_OnEnter();
			FSM_OnUpdate
				return SetPedInSeat_OnUpdate();

		FSM_State(State_UnReserveSeat)
			FSM_OnUpdate
				return UnReserveSeat_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

        FSM_State(State_WaitForNetworkSync)

	FSM_End
}

CTask::FSM_Return CTaskMountAnimal::ProcessPostFSM()
{	
	CPed* pPed = GetPed();

    if(!pPed->IsNetworkClone())
    {
	    // If the task should currently have the seat reserved, check that it does
	    if (GetState() > State_ReserveSeat && GetState() < State_UnReserveSeat)
	    {
		    CComponentReservation* pComponentReservation = m_pMount->GetComponentReservationMgr()->GetSeatReservation(m_iTargetEntryPoint, SA_directAccessSeat);
		    if (pComponentReservation && (pComponentReservation->GetPedUsingComponent() == pPed))
		    {
			    pComponentReservation->SetPedStillUsingComponent(pPed);
		    }
	    }
    }

	if (m_moveNetworkHelper.IsNetworkActive() && GetState() > State_Align)
	{
		const bool bIsDead = m_pJackedPed ? m_pJackedPed->IsInjured() : false;
		m_moveNetworkHelper.SetFlag(bIsDead, ms_IsDeadId);
	}

	return FSM_Continue;
}


const fwMvFlagId CTaskMountAnimal::ms_IsDeadId("IsDead",0xE2625733);
const fwMvBooleanId CTaskMountAnimal::ms_JackClipFinishedId("JackAnimFinished",0x881D4C73);
const fwMvClipId CTaskMountAnimal::ms_JackPedClipId("JackPedClip",0x6C92B1A2);
const fwMvFloatId CTaskMountAnimal::ms_JackPedPhaseId("JackPedPhase",0xECC2BEFD);

CTask::FSM_Return CTaskMountAnimal::Start_OnUpdate()
{
	const CVehicleEntryPointAnimInfo* pClipInfo = m_pMount->GetPedModelInfo()->GetLayoutInfo()->GetEntryPointAnimInfo(0);
	if (taskVerifyf(pClipInfo, "Vehicle has no entry clip info"))
	{
		const CPed* pPed = GetPed();
		if (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_OnMount ))
		{
			SetState(State_ExitMount);  
		}
		else
		{
			if (pPed->GetMotionData()->GetCurrentMbrY() >= MOVEBLENDRATIO_RUN) 
				m_bQuickMount = true;

			// Don't start evaluating entry points until within range
			// ...unless we're warping!  -JM
			taskFatalAssertf(m_pMount->GetPedModelInfo(), "NULL model info in CTaskMountAnimal::Start_OnUpdate");
			const float fStopRadius = m_pMount->GetPedModelInfo()->GetBoundingSphereRadius()+ms_fDistanceToEvaluateSeats;
			if (IsFlagSet(CVehicleEnterExitFlags::Warp) || IsFlagSet(CVehicleEnterExitFlags::WarpToEntryPosition)
				|| IsLessThanAll(DistSquared(pPed->GetTransform().GetPosition(), m_pMount->GetTransform().GetPosition()), ScalarVFromF32(fStopRadius*fStopRadius)))
			{
				SetState(State_PickSeat);
			}
			else
			{
				SetState(State_GoToMount);
			}
		}
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskMountAnimal::Start_OnUpdateClone()
{
    if(StateChangeHandledByCloneFSM(GetStateFromNetwork()))
    {
		if (IsFlagSet(CVehicleEnterExitFlags::WarpToEntryPosition))
		{
			WarpPedToEntryPositionAndOrientate();
		}

        if(m_ClipsReady)
        {
            const CVehicleEntryPointAnimInfo* pClipInfo = m_pMount->GetPedModelInfo()->GetLayoutInfo()->GetEntryPointAnimInfo(m_iTargetEntryPoint);
	        taskFatalAssertf(pClipInfo, "NULL Clip Info for entry index %i", m_iTargetEntryPoint);

			CPed* pPed = GetPed();

			m_moveNetworkHelper.CreateNetworkPlayer(pPed, CClipNetworkMoveInfo::ms_NetworkTaskMountAnimal);
			pPed->GetMovePed().SetTaskNetwork(m_moveNetworkHelper, NORMAL_BLEND_DURATION);
			m_moveNetworkHelper.SetClipSet(pClipInfo->GetEntryPointClipSetId());
	        
			if (IsFlagSet(CVehicleEnterExitFlags::WarpToEntryPosition))
			{
				// Warp to entry skips align state
				CheckForNextStateAfterCloneAlign();
			}
			else
			{
				SetState(State_Align);
			}
        }
        else
        {
            CPed *pPed = GetPed();

            // need to finish the task once it has finished on the remote clone
            if(GetStateFromNetwork()==State_Finish || !pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(GetTaskType()))
            {
                SetState(State_Finish);
            }
        }
    }

    return FSM_Continue;
}

void CTaskMountAnimal::ExitMount_OnEnter()
{
	taskAssert(GetPed()->GetPedConfigFlag( CPED_CONFIG_FLAG_OnMount ));
	GetPed()->SetPedOffMount(); //TODO which to task when ready
	//SetNewTask(rage_new CTaskLeaveAnyCar(0, false, true, true));
}

CTask::FSM_Return CTaskMountAnimal::ExitMount_OnUpdate()
{
	//if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Finish);
	}
	return FSM_Continue;
}

void  CTaskMountAnimal::GoToMount_OnEnter()
{
	static dev_float sfTimeBetweenPositionUpdates = 1.0f;
	m_TaskTimer.SetTime(sfTimeBetweenPositionUpdates);

	taskFatalAssertf(m_pMount->GetPedModelInfo(), "NULL model info in CTaskMountAnimal::GoToMount_OnEnter");
	const float fStopRadius = m_pMount->GetPedModelInfo()->GetBoundingSphereRadius()+ms_fDistanceToEvaluateSeats;
	CTaskMoveFollowNavMesh* pTask = rage_new CTaskMoveFollowNavMesh(GetPedMoveBlendRatio(), VEC3V_TO_VECTOR3(m_pMount->GetTransform().GetPosition()), fStopRadius, fStopRadius);
	pTask->SetUseLargerSearchExtents(true);

	CTask* pCombatTask = NULL;
	/*if( m_iRunningFlags.BitSet().IsSet(CVehicleEnterExitFlags::WillShootAtTargetPed) &&
		GetPed()->GetWeaponManager() && GetPed()->GetWeaponManager()->GetIsArmed() )
	{
		pCombatTask = rage_new CTaskCombatHatedTargets(CTaskGoToCarDoorAndStandStill::ms_fCombatHatedTargetReevaluateTime);
	}*/

	SetNewTask(rage_new CTaskComplexControlMovement(pTask, pCombatTask));
}

CTask::FSM_Return CTaskMountAnimal::GoToMount_OnUpdate()
{
	// If the warp timer had been set and has ran out, warp the ped now
	if (m_bTimerRanOut)
	{
// 		SetRunningFlag(CVehicleEnterExitFlags::Warp);
// 		m_bWarping = true;
// 
// 		SetState(State_PickSeat);
// 		return FSM_Continue;

		GetPed()->SetPedOnMount(m_pMount);
		return FSM_Quit;
	}

	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_PickSeat);
	}
	else
	{
		if (m_TaskTimer.Tick())
		{
			const CPed* pPed = GetPed();
			if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
			{
				CTaskComplexControlMovement* pControlTask = static_cast<CTaskComplexControlMovement*>(GetSubTask());
				if (pControlTask->GetRunningMovementTask(pPed) && pControlTask->GetRunningMovementTask(pPed)->GetTaskType() == CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH)
				{
					static_cast<CTaskMoveFollowNavMesh*>(pControlTask->GetRunningMovementTask(pPed))->SetTarget(pPed, m_pMount->GetTransform().GetPosition(), true);
				}
			}
			m_TaskTimer.Reset();
		}
		// Check to see if the player wants to quit entering
		else if (CheckPlayerExitConditions(*GetPed()))
		{
			SetState(State_Finish);
		}
	}
	return FSM_Continue;
}

dev_float TIME_TO_WAIT_BEFORE_ABORTING = 1.f;

CTask::FSM_Return CTaskMountAnimal::PickSeat_OnUpdate()
{
	// Pick the "door" to mount from
	CPed* pPed = GetPed();

	const u32 uLayoutInfo = ATSTRINGHASH("LAYOUT_UNKNOWN", 0x0534d3bf7);
	const CVehicleLayoutInfo* layout =  m_pMount->GetPedModelInfo()->GetLayoutInfo();
	if(layout && layout->GetName().GetHash() == uLayoutInfo)
	{
		m_bWarping = true;
		SetRunningFlag(CVehicleEnterExitFlags::Warp);
	}
		
	m_iTargetEntryPoint = CModelSeatInfo::EvaluateSeatAccessFromAllEntryPoints(pPed, m_pMount, m_iSeatRequestType, m_iSeat, m_bWarping, m_iRunningFlags);

	// If there is a valid entry point, go to it
	if (m_iTargetEntryPoint != -1)
	{
		// We are requesting any seat, since we have a valid entry point, set the target seat as the direct access seat of this entry point
        CPed *pPedInTargetSeat = (m_iSeat != -1) ? m_pMount->GetSeatManager()->GetPedInSeat(m_iSeat) : 0;
		bool bCanJackPed = pPedInTargetSeat && CanJackPed(pPed, pPedInTargetSeat, m_iRunningFlags);
		if (m_iSeatRequestType == SR_Any ||
            (m_iSeatRequestType == SR_Prefer && !bCanJackPed))
		{
			m_iSeat = m_pMount->GetPedModelInfo()->GetModelSeatInfo()->GetEntryExitPoint(m_iTargetEntryPoint)->GetSeat(SA_directAccessSeat);
		}

		// Warp the ped onto mount straight away (This might get set below if certain conditions allow warping)
		if (IsFlagSet(CVehicleEnterExitFlags::Warp) || IsFlagSet(CVehicleEnterExitFlags::WarpOverPassengers))
		{
			m_iCurrentSeatIndex = m_iSeat;
			SetState(State_SetPedInSeat);
		}
		else if (IsFlagSet(CVehicleEnterExitFlags::WarpToEntryPosition))
		{
			WarpPedToEntryPositionAndOrientate();
			StreamClipSets();

			if(m_ClipsReady)
			{
				float fBlendRatio = /*IsFlagSet(CVehicleEnterExitFlags::IsVehicleTransition) ? INSTANT_BLEND_DURATION : */NORMAL_BLEND_DURATION;
				if ( !StartMoveNetwork(pPed, m_pMount, m_moveNetworkHelper, m_iTargetEntryPoint, fBlendRatio))
				{
					SetState(State_Finish);
					return FSM_Continue;
				}

				m_pJackedPed = bCanJackPed ? pPedInTargetSeat : NULL;
				if (m_pJackedPed)
				{
					SetState(State_JackPed);
					return FSM_Continue;
				}
		else
		{
					CComponentReservation* pComponentReservation = m_pMount->GetComponentReservationMgr()->GetSeatReservation(m_iTargetEntryPoint, SA_directAccessSeat);
					if (CComponentReservationManager::ReserveVehicleComponent(GetPed(), m_pMount, pComponentReservation))
					{
						SetState(State_SetPedInSeat);
						return FSM_Continue;
					}
					else if(GetTimeInState() > TIME_TO_WAIT_BEFORE_ABORTING)
					{
						SetState(State_Finish);
						return FSM_Continue;
					}
				}
			}
		}
		else
		{
			SetState(State_GoToSeat);
		}
	}
	// No valid entry points, check for warping or wait
	else
	{
		if (m_iSeat == -1)
		{
			SetState(State_Finish);
			return FSM_Continue;
		}		

		// Player can warp if its a mission animal
		bool bPlayerCanWarpOntoAnimal = false;

		if (pPed->IsLocalPlayer())
		{
			if (m_pMount->PopTypeIsMission())
			{
				bPlayerCanWarpOntoAnimal = true;
			}
		}

		// Try again next frame and the one after that, so any objects flagged as being deleted are gon
		if (!IsFlagSet(CVehicleEnterExitFlags::WarpIfDoorBlocked) && bPlayerCanWarpOntoAnimal)
		{
			if (IsFlagSet(CVehicleEnterExitFlags::WarpIfDoorBlockedNext))
			{
				SetRunningFlag(CVehicleEnterExitFlags::WarpIfDoorBlocked);
			}
			else
			{
				SetRunningFlag(CVehicleEnterExitFlags::WarpIfDoorBlockedNext);
			}
			return FSM_Continue;
		}

		// No valid entry point found, try again allowing the player to warp over passengers
		bool bInNetworkGame = NetworkInterface::IsGameInProgress();

		if (!bInNetworkGame && IsFlagSet(CVehicleEnterExitFlags::CanWarpOverPassengers) && !IsFlagSet(CVehicleEnterExitFlags::WarpOverPassengers))
		{
			m_bWarping = true;
			SetRunningFlag(CVehicleEnterExitFlags::WarpOverPassengers);

			// Recursive call, try again with the extra flag set
			return PickSeat_OnUpdate();
		}

		// If this ped is set to warp after a time limit, and the doors are blocked, warp them immediately if doors are blocked
		if (IsFlagSet(CVehicleEnterExitFlags::WarpAfterTime))
		{
			SetRunningFlag(CVehicleEnterExitFlags::WarpIfDoorBlocked);
		}

		// Warp the ped in if the door is blocked
		if (!m_bWarping && IsFlagSet(CVehicleEnterExitFlags::WarpIfDoorBlocked))
		{
			m_bWarping = true;
			SetRunningFlag(CVehicleEnterExitFlags::Warp);

			// Recursive call, try again with the extra flag set
			return PickSeat_OnUpdate();
		}
		else
		{
			// We're not allowed to warp so quit
			SetState(State_Finish);
		}
	}

	return FSM_Continue;
}

dev_float TIME_BETWEEN_SEAT_CHECKS = 0.75f;


void CTaskMountAnimal::GoToSeat_OnEnter()
{
	taskAssertf(m_iTargetEntryPoint != -1, "Entry point index should have been set at this point");
	m_fSeatCheckInterval = fwRandom::GetRandomNumberInRange(0.8f, 1.2f)*TIME_BETWEEN_SEAT_CHECKS;	
/*	CTaskMoveGoToVehicleDoor * pMoveTask = rage_new CTaskMoveGoToVehicleDoor(
		m_pMount,
		m_fMoveBlendRatio,
		true, //TODO
		m_iTargetEntryPoint,
		CTaskGoToCarDoorAndStandStill::ms_fTargetRadius,
		CTaskGoToCarDoorAndStandStill::ms_fSlowDownDistance,
		CTaskGoToCarDoorAndStandStill::ms_fMaxSeekDistance,
		CTaskGoToCarDoorAndStandStill::ms_iMaxSeekTime,
		true
		);
	CTaskControlMovement * pCtrlMove = rage_new CTaskControlMovement(pMoveTask);
	SetNewTask(pCtrlMove);*/	

	float fTargetRadius = CTaskGoToCarDoorAndStandStill::ms_fTargetRadius;
	float fSlowDownDistance = CTaskGoToCarDoorAndStandStill::ms_fSlowDownDistance;

	if (CTaskEnterVehicleAlign::ms_Tunables.m_ForceStandEnterOnly)
	{
		TUNE_GROUP_FLOAT (TASK_MOUNT,fTARGET_RADIUS, 0.1f, 0.0f, 1.0f, 0.01f);
		TUNE_GROUP_FLOAT (TASK_MOUNT,fSLOW_DOWN_DISTSANCE, 3.5f, 0.0f, 8.0f, 0.1f);
		fTargetRadius = fTARGET_RADIUS;
		fSlowDownDistance = fSLOW_DOWN_DISTSANCE;
	}

	if (m_pMount->GetPedModelInfo() )
	{
		if (!CTaskEnterVehicleAlign::ms_Tunables.m_ForceStandEnterOnly)
		{
			fSlowDownDistance = 0.0f;

			// If close enough to do the stand align, don't bother with the goto car door task
			const CPed* pPed = GetPed();
			if (CTaskEnterVehicleAlign::ShouldDoStandAlign(*m_pMount, *pPed, m_iTargetEntryPoint))
			{
				return;
			}
		}
	}
	CTaskGoToCarDoorAndStandStill* pGoToTask = rage_new CTaskGoToCarDoorAndStandStill(m_pMount, GetPedMoveBlendRatio(), false, m_iTargetEntryPoint, fTargetRadius,
		fSlowDownDistance, CTaskGoToCarDoorAndStandStill::ms_fMaxSeekDistance, CTaskGoToCarDoorAndStandStill::ms_iMaxSeekTime, true, NULL);
	SetNewTask(pGoToTask);
}

CTask::FSM_Return CTaskMountAnimal::GoToSeat_OnUpdate()
{
	CPed* pPed = GetPed();

	// If the warp timer had been set and has ran out, warp the ped now
	if (m_bTimerRanOut)
	{
		GetPed()->SetPedOnMount(m_pMount);
		return FSM_Quit;
	}

	if (ENABLE_RDR_MOUNT && !m_pMount->GetSeatManager()->GetPedInSeat(m_iSeat))
	{
		Vector3 vPedToTarget = VEC3V_TO_VECTOR3( Subtract(pPed->GetTransform().GetPosition(), m_pMount->GetTransform().GetPosition()) );	
		float fFlatDistanceFromPedToTargetSq = vPedToTarget.XYMag2();
		static dev_float sf_TriggerMountDistance = 3.0f; 
		if (fFlatDistanceFromPedToTargetSq <= sf_TriggerMountDistance*sf_TriggerMountDistance)
		{
			SetState(State_ReserveSeat);
			return FSM_Continue;
		}
	}
	else if (GetSubTask() && GetSubTask()->GetTaskType()== CTaskTypes::TASK_GO_TO_CAR_DOOR_AND_STAND_STILL)
	{
		if (CTaskEnterVehicleAlign::ShouldTriggerAlign(*m_pMount, *pPed, m_iTargetEntryPoint))
		{
			SetState(State_ReserveSeat);
			return FSM_Continue;
		}
	}


	if (!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		if (GetSubTask())
		{
			if (taskVerifyf(GetSubTask()->GetTaskType()== CTaskTypes::TASK_GO_TO_CAR_DOOR_AND_STAND_STILL, "Subtask wasn't of type TASK_GO_TO_CAR_DOOR_AND_STAND_STILL in CTaskEnterVehicle::GoToDoor_OnUpdate"))
			{
				CTaskGoToCarDoorAndStandStill* pGoToDoorTask = static_cast<CTaskGoToCarDoorAndStandStill*>(GetSubTask());

				if(!pGoToDoorTask->HasAchievedDoor())
				{
					SetState(State_PickSeat);
					return FSM_Continue;
				}
				// Obtain the local space offset which was applied to the entry point, to avoid it sticking through any rear door
				//m_vLocalSpaceEntryModifier = pGoToDoorTask->GetLocalSpaceEntryPointModifier();
			}
		}
		// Check if we are close enough to do the stand align so we didn't bother doing the gotocar door task
		else if (!CTaskEnterVehicleAlign::ShouldDoStandAlign(*m_pMount, *pPed, m_iTargetEntryPoint))
		{
			SetState(State_PickSeat);
			return FSM_Continue;
		}

		SetState(State_ReserveSeat);
	}

	// If the point is no longer valid - restart
	else if (!CModelSeatInfo::EvaluateSeatAccessFromEntryPoint(pPed, m_pMount, m_iSeatRequestType, m_iSeat, m_iTargetEntryPoint, m_bWarping, m_iRunningFlags))
	{
		SetState(State_PickSeat);
	}
	// Check to see if the player wants to quit entering
	else if (CheckPlayerExitConditions(*pPed))
	{
		SetState(State_Finish);
	}	
	else
	{
		m_fSeatCheckInterval -= fwTimer::GetTimeStep();
		if (m_fSeatCheckInterval <= 0.0f)
		{
			m_fSeatCheckInterval = fwRandom::GetRandomNumberInRange(0.8f, 1.2f)*TIME_BETWEEN_SEAT_CHECKS;
			s32 iNewTargetEntryPoint = CModelSeatInfo::EvaluateSeatAccessFromAllEntryPoints(pPed, m_pMount, m_iSeatRequestType, m_iSeat, m_bWarping, m_iRunningFlags);
			if (m_iTargetEntryPoint != iNewTargetEntryPoint && GetSubTask()->MakeAbortable(CTask::ABORT_PRIORITY_URGENT,0))
			{
				m_iTargetEntryPoint = iNewTargetEntryPoint;
				SetFlag(aiTaskFlags::RestartCurrentState);
			}
		}
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskMountAnimal::ReserveSeat_OnUpdate()
{
	CPed* pPed = GetPed();

	// Need to reevaluate the seat in case someone is entering
	if (!CModelSeatInfo::EvaluateSeatAccessFromEntryPoint(pPed, m_pMount, m_iSeatRequestType, m_iSeat, m_iTargetEntryPoint, m_bWarping, m_iRunningFlags ) )
	{
		SetState(State_PickSeat);
		return FSM_Continue;
	}

	CComponentReservation* pComponentReservation = m_pMount->GetComponentReservationMgr()->GetSeatReservation(m_iTargetEntryPoint, SA_directAccessSeat);
	if (CComponentReservationManager::ReserveVehicleComponent(pPed, m_pMount, pComponentReservation))
	{
		const CEntryExitPoint* pEntryPoint = m_pMount->GetPedModelInfo()->GetModelSeatInfo()->GetEntryExitPoint(m_iTargetEntryPoint);
		s32 iSeat = pEntryPoint->GetSeat(SA_directAccessSeat);

		CPed* pSeatOccupier = m_pMount->GetSeatManager()->GetPedInSeat(iSeat);

		if((m_bWaitForSeatToBeOccupied && !pSeatOccupier) ||
			(m_bWaitForSeatToBeEmpty    &&  pSeatOccupier))
		{
			// Try again next frame
		}
		else
		{
			m_bWaitForSeatToBeOccupied = false;
			m_bWaitForSeatToBeEmpty    = false;

			if (IsFlagSet(CVehicleEnterExitFlags::WarpOverPassengers) && pSeatOccupier && !CanJackPed(pPed, pSeatOccupier, m_iRunningFlags))
			{
				SetRunningFlag(CVehicleEnterExitFlags::IsWarpingOverPassengers);
			}
			else if (pSeatOccupier)
			{
				m_pJackedPed = pSeatOccupier;
				SetRunningFlag(CVehicleEnterExitFlags::HasJackedAPed);
			}

			// Quit if injured
			if (pPed->IsInjured())
			{
				pPed->DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS);
				SetState(State_Finish);
			}
			else
			{				
				if (PedShouldWaitForLeaderToEnter())
				{
					SetState(State_WaitForGroupLeaderToEnter);
				}
				else if (m_ClipsReady) 
				{
					if (IsFlagSet(CVehicleEnterExitFlags::HasJackedAPed))
					{
						SetState(State_Align);
					}
					else
					{
						if (ENABLE_RDR_MOUNT && StartMoveNetwork(pPed, m_pMount, m_moveNetworkHelper, m_iTargetEntryPoint, NORMAL_BLEND_DURATION))
						{
							SetState(State_SetPedInSeat);
						}
						else
						{
							SetState(State_Align);
						}

						PedEnteredMount();
					}
				}		
			}
		}
	}
	return FSM_Continue;
}

void CTaskMountAnimal::Align_OnEnter()
{
	CTaskEnterVehicleAlign* pTask = rage_new CTaskEnterVehicleAlign(m_pMount, m_iTargetEntryPoint, VEC3_ZERO);
	taskFatalAssertf(pTask->GetMoveNetworkHelper(), "Couldn't Get MoveNetworkHelper From Task (%s)", pTask->GetTaskName());
	pTask->GetMoveNetworkHelper()->SetupDeferredInsert(&m_moveNetworkHelper, CTaskEnterVehicle::ms_AlignOnEnterId, CTaskEnterVehicle::ms_AlignNetworkId);
	SetNewTask( pTask );
}

bool CTaskMountAnimal::StartMoveNetwork(CPed* pPed, const CPed* pMount, CMoveNetworkHelper& moveNetworkHelper, s32 iEntryPointIndex, float fBlendInDuration) {
	// Don't restart the move network if its already attached
	if (moveNetworkHelper.IsNetworkActive())
	{
		pPed->GetMovePed().ClearTaskNetwork(moveNetworkHelper, NORMAL_BLEND_DURATION);
	}

	const CVehicleEntryPointAnimInfo* pClipInfo = pMount->GetPedModelInfo()->GetLayoutInfo()->GetEntryPointAnimInfo(iEntryPointIndex);
	taskFatalAssertf(pClipInfo, "NULL Clip Info for entry index %i", iEntryPointIndex);

	moveNetworkHelper.CreateNetworkPlayer(pPed, CClipNetworkMoveInfo::ms_NetworkTaskMountAnimal);

	taskAssertf(fwClipSetManager::GetClipSet(pClipInfo->GetEntryPointClipSetId()), "Couldn't find clipset %s", pClipInfo->GetEntryPointClipSetId().GetCStr());

	moveNetworkHelper.SetClipSet(pClipInfo->GetEntryPointClipSetId()); 
	return pPed->GetMovePed().SetTaskNetwork(moveNetworkHelper, fBlendInDuration);
}


void CTaskMountAnimal::Align_OnEnterClone()
{	
	// ensure we are aligned with the door correctly
	CPed *pPed = GetPed();
	NetworkInterface::GoStraightToTargetPosition(pPed);

	Align_OnEnter();
}

CTask::FSM_Return CTaskMountAnimal::Align_OnUpdate()
{
	// We may need to wait for the seat reservation after the align has finished	
	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished) || !GetSubTask())
	{
		CPed* pPed = GetPed();
		// If we're not close enough, reevaluate and try again if flagged to
		if (!CTaskEnterVehicleAlign::AlignSucceeded(*m_pMount, *pPed, m_iTargetEntryPoint, IsFlagSet(CVehicleEnterExitFlags::EnteringOnVehicle)))
		{
			if (IsFlagSet(CVehicleEnterExitFlags::ResumeIfInterupted))
			{
				ASSERT_ONLY(taskDebugf2("Frame : %u - %s%s stopped Enter Vehicle Move Network : CTaskEnterVehicle::Align_OnUpdate()", fwTimer::GetFrameCount(), pPed->IsNetworkClone() ? "Clone ped : " : "Local ped : ", pPed->GetDebugName()));
				pPed->GetMovePed().ClearTaskNetwork(m_moveNetworkHelper, NORMAL_BLEND_DURATION);
				SetState(State_PickSeat);
				return FSM_Continue;
			}
			else
			{
				SetState(State_Finish);
				return FSM_Continue;
			}
		}

		// If there is a ped in the seat, jack the ped
		s32 iTargetSeat = m_pMount->GetPedModelInfo()->GetModelSeatInfo()->GetEntryExitPoint(m_iTargetEntryPoint)->GetSeat(SA_directAccessSeat);
		m_pJackedPed = m_pMount->GetSeatManager()->GetPedInSeat(iTargetSeat);
		if (m_pJackedPed)
		{
			SetState(State_JackPed);
			return FSM_Continue;
		}
		else 
		{			
			CComponentReservation* pComponentReservation = m_pMount->GetComponentReservationMgr()->GetSeatReservation(m_iTargetEntryPoint, SA_directAccessSeat);
			if (CComponentReservationManager::ReserveVehicleComponent(GetPed(), m_pMount, pComponentReservation))
			{
				SetState(State_SetPedInSeat);
				return FSM_Continue;
			}
		}
	}
	return FSM_Continue;
}


CTask::FSM_Return CTaskMountAnimal::Align_OnUpdateClone()
{
	CheckForNextStateAfterCloneAlign();

    return FSM_Continue;
}


CTask::FSM_Return CTaskMountAnimal::WaitForGroupLeaderToEnter_OnUpdate()
{
	if (!PedShouldWaitForLeaderToEnter())
	{
		SetState(State_Align);
	}

	return FSM_Continue;
}

void CTaskMountAnimal::JackPed_OnEnter()
{
	SetRunningFlag(CVehicleEnterExitFlags::HasJackedAPed);
	StoreInitialOffsets();

    if(IsClipTransitionAvailable(GetState()))
    {
		static const fwMvRequestId jackPedId("JackPed",0x78130A7D);
	    m_moveNetworkHelper.SendRequest( jackPedId);
    }
    else
    {
		static const fwMvStateId jackPedId("Jack Ped From OutSide",0x94E8BD3A);
        m_moveNetworkHelper.ForceStateChange(jackPedId);
    }
}

CTask::FSM_Return CTaskMountAnimal::JackPed_OnUpdate()
{
    CPed *pPed = GetPed();

	if (m_moveNetworkHelper.GetBoolean( ms_JackClipFinishedId))
	{
        m_iLastCompletedState = GetState();

        if(pPed->IsNetworkClone())
        {
            // wait for the state from the network to change before advancing the state
            m_iNetworkWaitState = GetState();
            SetState(State_WaitForNetworkSync);
        }
        else
        {
			//Ensure victim is clear from my mount
			taskAssert(m_pJackedPed!=NULL);
			m_pJackedPed->SetPedOffMount(false);
			if (IsFlagSet(CVehicleEnterExitFlags::JustPullPedOut))
		    {
			    // Need to detach to reactivate physics
			    GetPed()->DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS);
			    SetState(State_Finish);
		    }
		    else
		    {
			    SetState(State_SetPedInSeat);
		    }
        }
	}
	return FSM_Continue;
}

void CTaskMountAnimal::EnterSeat_OnEnter()
{
	StoreInitialOffsets();	
	float fMountZone = ComputeMountZone();
	SetNewTask(rage_new CTaskMountSeat(m_iRunningFlags, m_pMount, m_iSeat, m_bQuickMount, fMountZone));	

	m_pMount->GetPedIntelligence()->ClearTasks();
	m_pMount->GetPedIntelligence()->AddTaskDefault(rage_new CTaskGettingMounted(this, m_iTargetEntryPoint));
}

float CTaskMountAnimal::ComputeMountZone() 
{
	Vec3V vMountPos = m_pMount->GetTransform().GetPosition();
	Vector3 vRiderVector = VEC3V_TO_VECTOR3(Subtract(GetPed()->GetTransform().GetPosition(), vMountPos)); //m_PlayAttachedClipHelper.GetInitialTranslationOffset();
	vRiderVector.z=0; vRiderVector.Normalize(); 

	Vector3 vEntryPosition(Vector3::ZeroType);
	Quaternion qEntryOrientation(0.0f,0.0f,0.0f,1.0f);
	CModelSeatInfo::CalculateEntrySituation(m_pMount, GetPed(), vEntryPosition, qEntryOrientation, m_iTargetEntryPoint);
	vEntryPosition.Subtract(VEC3V_TO_VECTOR3(m_pMount->GetTransform().GetPosition()));
	vEntryPosition.z=0; vEntryPosition.Normalize();

	float theta = vRiderVector.FastAngle(vEntryPosition);
	vEntryPosition.RotateZ(PI*0.5f);
	if (vRiderVector.Dot(vEntryPosition) < 0)
		theta = -theta;

	//Displayf("    MOUNT ZONE = %f", theta*RtoD);

	static dev_float sf_CentralAngleRange = 15.0f*DtoR;
	if (theta > sf_CentralAngleRange)
		return -1.0f;
	else if (theta < -sf_CentralAngleRange)
		return 1.0f;
	return 0.0f;
}

CTask::FSM_Return CTaskMountAnimal::EnterSeat_OnUpdate()
{	
	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		// We're entering the direct access seat, store it here so we can pass it to the set ped in seat task
        m_iLastCompletedState = GetState();
        SetState(State_Finish);
	} 
	else 
	{
		CTask* pTask = m_pMount->GetPedIntelligence()->GetTaskActive();
		CTaskMountSeat* pMountSeatTask = static_cast<CTaskMountSeat*>(GetSubTask());
		if (!pMountSeatTask->GetMountReady() && (!pTask || pTask->GetTaskType() != CTaskTypes::TASK_GETTING_MOUNTED))
		{ //something happened start the Getting mounted task again			
			m_pMount->GetPedIntelligence()->AddTaskDefault(rage_new CTaskGettingMounted(this, m_iTargetEntryPoint));
		}
	}
	return FSM_Continue;
}

bool CTaskMountAnimal::ProcessPhysics(float fTimeStep, int nTimeSlice)
{
	if (GetState() == State_EnterSeat )
	{
		UpdateMountAttachmentOffsets();
	}

	return CTask::ProcessPhysics(fTimeStep, nTimeSlice);
}

void CTaskMountAnimal::SetPedInSeat_OnEnter()
{
	//Ensure the target entry point is valid.
	taskAssert(m_iTargetEntryPoint > -1);
	m_iCurrentSeatIndex = GetMount()->GetPedModelInfo()->GetModelSeatInfo()->GetEntryExitPoint(m_iTargetEntryPoint)->GetSeat(SA_directAccessSeat);
	taskAssert(m_iCurrentSeatIndex > -1);

	//If various conditions hold, add a shocking event for ambient peds to react against.
	PossiblyBroadcastMountStolenEvent();
	
	//Ensure the ped is the local player.
	if(!GetPed()->IsLocalPlayer())
		return;
		
	//Ensure a network game is in progress.
	if(!NetworkInterface::IsGameInProgress())
		return;

	//Ensure the mount is a network clone.
	if(!GetMount()->IsNetworkClone())
		return;
}

CTask::FSM_Return CTaskMountAnimal::SetPedInSeat_OnUpdate()
{	
	// if we get to this state before we have a valid seat index or there is still a ped in the seat
	// we have to quit the task and rely on the network syncing code to place the ped in the vehicle
	bool pedAlreadyInSeat = m_pMount->GetSeatManager()->GetPedInSeat(m_iCurrentSeatIndex) != 0;

	CPed* pPed = GetPed();
	if(pPed && pPed->IsNetworkClone() && (m_iCurrentSeatIndex == -1 || pedAlreadyInSeat))
	{
		SetState(State_Finish);
	}
	else
	{
		m_pMount->GetHorseComponent()->Reset();
	}

	// Check for warp
	if (IsFlagSet(CVehicleEnterExitFlags::Warp) || IsFlagSet(CVehicleEnterExitFlags::WarpOverPassengers))
	{
		GetPed()->SetPedOnMount(m_pMount);
		SetState(State_Finish);
		return FSM_Continue;
	}
	
	//Move to the next state.
	SetState(State_EnterSeat);
	
	return FSM_Continue;
}

CTask::FSM_Return CTaskMountAnimal::UnReserveSeat_OnUpdate()
{
	CPed* pPed = GetPed();

	CComponentReservation* pComponentReservation = m_pMount->GetComponentReservationMgr()->GetSeatReservation(m_iTargetEntryPoint, SA_directAccessSeat);
	if (pComponentReservation && pComponentReservation->IsComponentReservedForPed(pPed))
	{
		CComponentReservationManager::UnreserveVehicleComponent(pPed, pComponentReservation);
	}

	SetState(State_Finish);	
	
	return FSM_Continue;
}

CTask::FSM_Return CTaskMountAnimal::WaitForNetworkSync_OnUpdate()
{
    if(GetStateFromNetwork() != m_iNetworkWaitState)
    {
        SetState(GetStateFromNetwork());
    }

    return FSM_Continue;
}

void CTaskMountAnimal::PedEnteredMount()
{
	CPed* pPed = GetPed();
	CPedGroup* pPedsGroup = pPed->GetPedsGroup();
	if (pPedsGroup)
	{
		CPed* pLeader = pPedsGroup->GetGroupMembership()->GetLeader();
		if (pLeader && pLeader != pPed)
		{
			if (pLeader->IsLocalPlayer())
			{
				CTaskVehicleFSM::TIME_LAST_PLAYER_GROUP_MEMBER_ENTERED = fwTimer::GetTimeInMilliseconds();
			}
		}
	}
}

void CTaskMountAnimal::UpdateMountAttachmentOffsets() 
{
	float fClipPhase=0;
	const crClip* pClip = GetClipAndPhaseForState(fClipPhase);
	if (pClip)
	{
		Vector3 vNewPedPosition(Vector3::ZeroType);
		Quaternion qNewPedOrientation(0.0f,0.0f,0.0f,1.0f);
		CModelSeatInfo::CalculateSeatSituation(m_pMount, vNewPedPosition, qNewPedOrientation, m_iTargetEntryPoint);
		Quaternion qSeatOrientation(qNewPedOrientation);
		m_PlayAttachedClipHelper.SetTarget(vNewPedPosition, qNewPedOrientation);

#if __DEV
		Vector3 vArrowPoint(0.0f,1.0f,0.0f);
		qNewPedOrientation.Transform(vArrowPoint);
		vArrowPoint.AddScaled(vNewPedPosition, vArrowPoint, 3.0f);
		CTask::ms_debugDraw.AddArrow(VECTOR3_TO_VEC3V(vNewPedPosition), VECTOR3_TO_VEC3V(vArrowPoint), 0.3f, Color_red, 1);		
#endif

		//Find start
		m_PlayAttachedClipHelper.ComputeInitialSituation(vNewPedPosition, qNewPedOrientation);

#if __DEV
		vArrowPoint.Set(0,1.0f,0);
		qNewPedOrientation.Transform(vArrowPoint);
		vArrowPoint.AddScaled(vNewPedPosition, vArrowPoint, 1.0f);
		CTask::ms_debugDraw.AddArrow(VECTOR3_TO_VEC3V(vNewPedPosition), VECTOR3_TO_VEC3V(vArrowPoint), 0.3f, Color_green, 1);
#endif
				
		// Update the situation from the current phase in the clip															
		// This step seems unnecessary [11/22/2013 musson]
		//m_PlayAttachedClipHelper.ComputeSituationFromClip(pClip, 0.0f, fClipPhase, vNewPedPosition, qNewPedOrientation);
		
#if __DEV
		//vArrowPoint.Set(0,1.0f,0);
		//qNewPedOrientation.Transform(vArrowPoint);
		//vArrowPoint.AddScaled(vNewPedPosition, vArrowPoint, 1.0f);
		//CTask::ms_debugDraw.AddArrow(VECTOR3_TO_VEC3V(vNewPedPosition), VECTOR3_TO_VEC3V(vArrowPoint), 0.3f, Color_blue, 1);
#endif

		// Compute the offsets we'll need to fix up
		Vector3 vOffsetPosition(Vector3::ZeroType);
		Quaternion qOffsetOrientation(0.0f,0.0f,0.0f,1.0f);
		
		m_PlayAttachedClipHelper.ComputeOffsets(pClip, fClipPhase, 1.0f, vNewPedPosition, qNewPedOrientation, vOffsetPosition, qOffsetOrientation, &qSeatOrientation, NULL, true);			

#if __DEV
		CTask::ms_debugDraw.AddArrow(VECTOR3_TO_VEC3V(vNewPedPosition), VECTOR3_TO_VEC3V(vOffsetPosition + vNewPedPosition), 0.3f, Color_pink, 1);
#endif

		// Apply the fixup
		m_PlayAttachedClipHelper.ApplyMountOffsets(vNewPedPosition, qNewPedOrientation, pClip, vOffsetPosition, qOffsetOrientation, fClipPhase);						
		qNewPedOrientation.Normalize();

		//DEBUG
		// 			Matrix34 mTemp(Matrix34::IdentityType);
		// 			mTemp.FromQuaternion(qNewPedOrientation);
		// 			mTemp.d = vNewPedPosition;
		// 			static float sfScale = 0.5f;
		// 			grcDebugDraw::Axis(mTemp, sfScale);

		// Pass in the new position and orientation in global space, this will get transformed relative to the vehicle for attachment
		CTaskVehicleFSM::UpdatePedVehicleAttachment(GetPed(), VECTOR3_TO_VEC3V(vNewPedPosition), QUATERNION_TO_QUATV(qNewPedOrientation));
	}	
}

dev_u32 TIME_BETWEEN_GROUP_MEMBERS_MOUNTING = 500;

bool CTaskMountAnimal::PedShouldWaitForLeaderToEnter()
{
	CPed* pPed = GetPed();

	if (IsFlagSet(CVehicleEnterExitFlags::WaitForLeader))
	{
		CPedGroup* pPedsGroup = pPed->GetPedsGroup();
		if (pPedsGroup)
		{
			CPed* pLeader = pPedsGroup->GetGroupMembership()->GetLeader();
			if (pLeader && pLeader != pPed )
			{
				if (pLeader->IsLocalPlayer() && ((CTaskVehicleFSM::TIME_LAST_PLAYER_GROUP_MEMBER_ENTERED + TIME_BETWEEN_GROUP_MEMBERS_MOUNTING ) > fwTimer::GetTimeInMilliseconds()))
				{
					return true;
				}
				else if (pLeader->GetPedConfigFlag( CPED_CONFIG_FLAG_OnMount ) ||
					(pLeader->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_MOUNT_ANIMAL) &&
					pLeader->GetPedIntelligence()->GetQueriableInterface()->GetStateForTaskType( CTaskTypes::TASK_MOUNT_ANIMAL ) >= CTaskMountAnimal::State_EnterSeat &&
					pLeader->GetPedIntelligence()->GetQueriableInterface()->GetStateForTaskType( CTaskTypes::TASK_MOUNT_ANIMAL ) <= CTaskMountAnimal::State_Finish) )
				{
					return false;
				}
				else
				{
					return true;
				}
			}
		}
	}

	return false;
}

const crClip* CTaskMountAnimal::GetClipAndPhaseForState(float& fPhase)
{
	if (m_moveNetworkHelper.IsNetworkActive())
	{
		switch (GetState())
		{
		case State_JackPed:
			{
				fPhase = m_moveNetworkHelper.GetFloat( ms_JackPedPhaseId);
				fPhase = rage::Clamp(fPhase, 0.0f, 1.0f);
				return m_moveNetworkHelper.GetClip( ms_JackPedClipId);
			}
		case State_EnterSeat:
			{
				if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_MOUNT_SEAT)
				{
					return static_cast<CTaskMountSeat*>(GetSubTask())->GetClipAndPhaseForState(fPhase);
				}
				return NULL;
			}
		default: taskAssertf(0, "Unhandled State");
		}
	}
	return NULL;
}

void CTaskMountAnimal::StartMountAnimation()
{
	if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_MOUNT_SEAT)
	{
		return static_cast<CTaskMountSeat*>(GetSubTask())->SetMountReady();
	}
}

void CTaskMountAnimal::StoreInitialOffsets()
{
	// Store the offsets to the seat situation so we can recalculate the starting situation each frame relative to the seat
	Vector3 vTargetPosition(Vector3::ZeroType);
	Quaternion qTargetOrientation(0.0f,0.0f,0.0f,1.0f);

	CModelSeatInfo::CalculateSeatSituation(m_pMount, vTargetPosition, qTargetOrientation, m_iTargetEntryPoint);
	m_PlayAttachedClipHelper.SetTarget(vTargetPosition, qTargetOrientation);
	m_PlayAttachedClipHelper.SetInitialOffsets(*GetPed());
}

bool CTaskMountAnimal::CheckPlayerExitConditions( CPed& ped)
{
	if (!ped.IsLocalPlayer())
	{
		return false;
	}

	if (!(IsFlagSet(CVehicleEnterExitFlags::PlayerControlledVehicleEntry)))
	{
		return false;
	}

	CControl* pControl = ped.GetControlFromPlayer();

	// Abort if X is pressed.
	if( pControl->GetPedJump().IsPressed() )
	{
		return true;
	}

	// Only abort if a certain time has passed
	static dev_float MIN_TIME_BEFORE_PLAYER_CAN_ABORT = 0.5f;
	if (GetTimeRunning() < MIN_TIME_BEFORE_PLAYER_CAN_ABORT)
	{
		return false;
	}

	Vector3 vAwayFromMount = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition() - m_pMount->GetTransform().GetPosition());
	float fDist = vAwayFromMount.Mag();

	// Check the distance to the vehicle isn't too far.
	if (fDist > CPlayerInfo::SCANNEARBYVEHICLES*1.5f)
	{
		return true;
	}

	float fTaskHeading=fwAngle::GetRadianAngleBetweenPoints(vAwayFromMount.x,vAwayFromMount.y,0.0f,0.0f);
	fTaskHeading = fwAngle::LimitRadianAngle(fTaskHeading - PI);

	Vector2 vecStick(pControl->GetPedWalkLeftRight().GetNorm(), -pControl->GetPedWalkUpDown().GetNorm());
	float fInputDirn = rage::Atan2f(-vecStick.x, vecStick.y) + camInterface::GetHeading(); // // careful.. this looks dodgy to me - and could be ANY camera - DW	
	float fInputMag = vecStick.Mag();

	if (fInputDirn > fTaskHeading + PI)
		fInputDirn -= TWO_PI;
	else if(fInputDirn < fTaskHeading - PI)
		fInputDirn += TWO_PI;

	if (fInputMag > 0.75f && rage::Abs(fInputDirn - fTaskHeading) > QUARTER_PI)
	{
		return true;
	}

	return false;
}

void CTaskMountAnimal::PossiblyBroadcastMountStolenEvent()
{
	//CPed* pMounter = GetPed();
	//CPed* pMount = GetMount();

	//// Reject if this mount was the last mount of the mounter.
	//if (pMounter->GetMyLastMount() == pMount)
	//{
	//	return;
	//}

	//// Ignore mission horses (for now?).
	//if (pMount->PopTypeIsMission())
	//{
	//	return;
	//}

	//// Add the global event.
	//CEventShockingMountStolen ev(*pMounter, *pMount);
	//CShockingEventsManager::Add(ev);
}

CTaskInfo* CTaskMountAnimal::CreateQueriableState() const
{
	// TODO: Re implement jack ped target
	return rage_new CClonedMountAnimalInfo(GetState(), m_pMount, m_iSeatRequestType, m_iSeat, m_iTargetEntryPoint, m_iRunningFlags, m_pJackedPed);
}

void CTaskMountAnimal::ReadQueriableState( CClonedFSMTaskInfo* pTaskInfo )
{
	//Ensure the task info is the correct type.
	taskAssert(pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_MOUNT_ANIMAL_CLONED );
	
	//Grab the specific task info.
	CClonedMountAnimalInfo* pInfo = static_cast<CClonedMountAnimalInfo *>(pTaskInfo);

	//Cache the old entry point.
    s32 oldTargetEntryPoint = m_iTargetEntryPoint;
    
	//Call the base class version.
	//Syncs the base class state.
	CTaskMountFSM::ReadQueriableState(pTaskInfo);
	
	//Sync the data.
	m_pJackedPed = pInfo->GetJackedPed();

    // once we have started the task proper, we don't want the remote state of the
    // entry point updated from network syncs
    if(GetState() > State_Align)
    {
        m_iTargetEntryPoint = oldTargetEntryPoint;
    }
}

//-------------------------------------------------------------------------
// Returns true if this ped can displace the other ped, pOtherPed can be NULL
//-------------------------------------------------------------------------
bool CTaskMountAnimal::CanJackPed( const CPed* pPed, CPed* pOtherPed, VehicleEnterExitFlags iFlags )
{
	if( pOtherPed == NULL ||
		pPed == pOtherPed )
	{
		return true;
	}

	if( iFlags.BitSet().IsSet(CVehicleEnterExitFlags::DontJackAnyone) )
		return false;

	bool bDontDragOtherPedOut = pOtherPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DontDragMeOutCar ) ||
		(pPed->IsControlledByLocalOrNetworkPlayer() && pOtherPed->GetPedConfigFlag( CPED_CONFIG_FLAG_PlayersDontDragMeOutOfCar ));

	if( bDontDragOtherPedOut && !pOtherPed->IsInjured())
	{
		return false;
	}

	if( pOtherPed->GetIsArrested() && pPed->GetPedType() == PEDTYPE_COP )
	{
		return false;
	}

	// Force jack even if the ped is friendly
	if ( iFlags.BitSet().IsSet(CVehicleEnterExitFlags::JackIfOccupied))
	{
		return true;
	}

	CPedGroup* pPedGroup = pPed->GetPedsGroup();

	// peds can't jack other friendly peds to get to a seat
	if( !pOtherPed->IsInjured() && 
		( pPed->GetPedIntelligence()->IsFriendlyWith(*pOtherPed) ||
		( pPedGroup && pPedGroup->GetGroupMembership()->IsMember(pOtherPed) ) ) )
	{
		return false;
	}

	return true;
}

void CTaskMountAnimal::OnCloneTaskNoLongerRunningOnOwner()
{
	SetStateFromNetwork(State_Finish);
}

CTask::FSM_Return CTaskMountAnimal::UpdateClonedFSM( s32 iState, CTask::FSM_Event iEvent )
{
	// if the state change is not handled by the clone FSM block below
	// we need to keep the state in sync with the values received from the network
	if(!StateChangeHandledByCloneFSM(iState))
	{
		if(iEvent == OnUpdate)
		{
			if(GetStateFromNetwork() != GetState())
			{
				SetState(GetStateFromNetwork());
				return FSM_Continue;
			}
		}
	}

	// we can't finish the task until the seat we are trying to enter is free. The seat may still have the ped we have jacked in 
	// it for a brief while. Once this task finishes the network code will place the ped into the target seat.
	if(GetState() == State_Finish)
	{
        CPed *pPed = GetPed();

        const CEntryExitPoint* pEntryPoint = NULL;
        if( m_pMount && m_iTargetEntryPoint != -1 )
	        pEntryPoint = m_pMount->GetPedModelInfo()->GetModelSeatInfo()->GetEntryExitPoint(m_iTargetEntryPoint);

		if( pEntryPoint )
		{
			CPed *pSeatOccupier = m_pMount->GetSeatManager()->GetPedInSeat(pEntryPoint->GetSeat(SA_directAccessSeat));

			if (pSeatOccupier && pSeatOccupier != pPed)
			{
				return FSM_Continue;
			}
		}
	}

FSM_Begin

	// Wait for a network update to decide which state to start in
	FSM_State(State_Start)
		FSM_OnUpdate
            Start_OnUpdateClone();

	FSM_State(State_Finish)
		FSM_OnUpdate
			return FSM_Quit;

	FSM_State(State_Align)
		// Attach and align the ped with the door
		FSM_OnEnter
			Align_OnEnterClone();
		FSM_OnUpdate
            Align_OnUpdateClone();
		
    FSM_State(State_JackPed)
		FSM_OnEnter
			JackPed_OnEnter();
		FSM_OnUpdate
			return JackPed_OnUpdate();

    FSM_State(State_EnterSeat)
		FSM_OnEnter
			EnterSeat_OnEnter();
		FSM_OnUpdate
            EnterSeat_OnUpdate();

	FSM_State(State_SetPedInSeat)
		FSM_OnEnter
			SetPedInSeat_OnEnter();
		FSM_OnUpdate
			return SetPedInSeat_OnUpdate();

    FSM_State(State_WaitForNetworkSync)
		FSM_OnUpdate
            return WaitForNetworkSync_OnUpdate();

    // unhandled states
	FSM_State(State_ExitMount)
	FSM_State(State_PickSeat)
	FSM_State(State_GoToMount)
	FSM_State(State_GoToSeat)	
	FSM_State(State_WaitForGroupLeaderToEnter)	
    FSM_State(State_ReserveSeat)	
	FSM_State(State_UnReserveSeat)
	
FSM_End
}

bool CTaskMountAnimal::StateChangeHandledByCloneFSM(s32 iState)
{
	switch(iState)
	{
	case State_Start:
	case State_Align:	
	case State_EnterSeat:
    case State_JackPed:    
    case State_SetPedInSeat:    
    case State_WaitForNetworkSync:
    case State_Finish:
		return true;
	default:
		return false;
	}
}

void CTaskMountAnimal::CheckForNextStateAfterCloneAlign()
{
	//Grab the network state.
	s32 networkState = GetStateFromNetwork();

	//Check if the state has changed.
	if(networkState != GetState())
	{
		//Check if the state advanced forward, without finishing.
		//If the state has already finished, just rely on network sync code to place the ped in the seat.

		if (networkState == State_JackPed)
		{
			SetState(State_JackPed);
		}
		else if(networkState > State_Align && networkState != State_Finish)
		{
			//Do not skip the set ped in seat state!  It happens very fast on the remote client
			//and the state change will likely not get picked up here.
			SetState(State_SetPedInSeat);
		}
		else
		{
			//Sync the state with the network.
			SetState(networkState);
		}
	}
}

bool CTaskMountAnimal::IsClipTransitionAvailable(s32 iState)
{
    switch(iState)
    {
    case State_JackPed:
        return ((m_iLastCompletedState == -1)              ||
                (m_iLastCompletedState == State_EnterSeat));    
    default:
        break;
    }
    return true;
}

void CTaskMountAnimal::StreamClipSets()
{
	//Note that the clips are not ready.
	m_ClipsReady = false;
	
	//Ensure the entry point is valid.
	if(m_iTargetEntryPoint < 0)
		return;

	//Ensure the seat is valid.
	if(m_iSeat < 0)
		return;

	m_ClipsReady = RequestAnimations(*GetPed(), *m_pMount, m_iTargetEntryPoint, m_iSeat);
}

bool CTaskMountAnimal::RequestAnimations( const CPed& ped, const CPed& mount, s32 iEntryPoint, s32 iSeat )
{
	bool bLoaded = true;

	const fwMvClipSetId commonClipsetId = CTaskEnterVehicleAlign::GetCommonClipsetId(&mount, iEntryPoint);
	CTaskVehicleFSM::RequestVehicleClipSet(commonClipsetId, SP_High);
	bLoaded &= CTaskVehicleFSM::IsVehicleClipSetLoaded(commonClipsetId);

	const fwMvClipSetId entryClipsetId = CTaskEnterVehicleAlign::GetEntryClipsetId(&mount, iEntryPoint, &ped);
	CTaskVehicleFSM::RequestVehicleClipSet(entryClipsetId, SP_High);
	bLoaded &= CTaskVehicleFSM::IsVehicleClipSetLoaded(entryClipsetId);

	const CVehicleSeatAnimInfo* pSeatInfo = mount.GetPedModelInfo()->GetLayoutInfo()->GetSeatAnimationInfo(iSeat);
	const fwMvClipSetId inVehicleClipsetId = pSeatInfo ? pSeatInfo->GetSeatClipSetId() : fwMvClipSetId();
	CTaskVehicleFSM::RequestVehicleClipSet(inVehicleClipsetId, ped.IsLocalPlayer() ? SP_High : SP_Medium);
	bLoaded &= CTaskVehicleFSM::IsVehicleClipSetLoaded(inVehicleClipsetId);

	return bLoaded;
}

void CTaskMountAnimal::WarpPedToEntryPositionAndOrientate()
{
	CPed* pPed = GetPed();

	Vector3 vEntryPosition(Vector3::ZeroType);
	Quaternion qEntryOrientation(0.0f,0.0f,0.0f,1.0f);
	CModelSeatInfo::CalculateEntrySituation(m_pMount, pPed, vEntryPosition, qEntryOrientation, m_iTargetEntryPoint);
	Vector3 vGroundPos(Vector3::ZeroType);
	if (CTaskVehicleFSM::FindGroundPos(m_pMount, pPed, vEntryPosition, 2.5f, vGroundPos))
	{
		vEntryPosition.z = vGroundPos.z;
	}
	pPed->Teleport(vEntryPosition, qEntryOrientation.TwistAngle(ZAXIS), true);

	if (!pPed->GetIsAttached())
	{		
		//Attach the ped to the mount.
		Vector3 attachOffset(Vector3::ZeroType);
		s16 boneIndex = (s16)m_pMount->GetBoneIndexFromBoneTag(BONETAG_SKEL_SADDLE);	
		pPed->AttachPedToMountAnimal(m_pMount, (u16)(boneIndex!=-1 ? boneIndex : 0), ATTACH_STATE_PED_ON_MOUNT, &attachOffset, 0.0f, 0.0f);	
		CTaskVehicleFSM::UpdatePedVehicleAttachment(GetPed(), VECTOR3_TO_VEC3V(vEntryPosition), QUATERNION_TO_QUATV(qEntryOrientation));
	}
}

#if !__FINAL
const char * CTaskMountAnimal::GetStaticStateName( s32 iState )
{
	taskAssert(iState >= State_Start && iState <= State_Finish);

	switch (iState)
	{		
		case State_Start:							return "State_Start";
		case State_ExitMount:						return "State_ExitMount";
 		case State_GoToMount:						return "State_GoToMount";
 		case State_PickSeat:						return "State_PickSeat";
 		case State_GoToSeat:						return "State_GoToSeat";
 		case State_ReserveSeat:						return "State_ReserveSeat";
 		case State_Align:							return "State_Align";
 		case State_WaitForGroupLeaderToEnter:		return "State_WaitForGroupLeaderToEnter";
 		case State_JackPed:							return "State_JackPed";
 		case State_EnterSeat:						return "State_EnterSeat";
 		case State_SetPedInSeat:					return "State_SetPedInSeat";
 		case State_UnReserveSeat:					return "State_UnReserveSeat";
        case State_WaitForNetworkSync:              return "State_WaitForNetworkSync";
		case State_Finish:							return "State_Finish";
		default: taskAssert(0); 	
	}

	return "State_Invalid";
}

void CTaskMountAnimal::Debug() const
{
#if __BANK
	// Base Class
	CTask::Debug();

	CPed* pFocusPed = NULL;

	CEntity* pFocusEntity = CDebugScene::FocusEntities_Get(0);
	if (pFocusEntity && pFocusEntity->GetIsTypePed())
	{
		pFocusPed = static_cast<CPed*>(pFocusEntity);
	}

	if (!pFocusPed)
	{
		pFocusPed = CGameWorld::FindLocalPlayer();
	}

	if (GetPed() == pFocusPed)
	{
		grcDebugDraw::AddDebugOutput(Color_green, "Target Seat : %u", m_iSeat);
		grcDebugDraw::AddDebugOutput(Color_green, "Target Entrypoint : %u", m_iTargetEntryPoint);
		if (pFocusPed->GetPedConfigFlag(CPED_CONFIG_FLAG_OnMount))
		{
			grcDebugDraw::AddDebugOutput(Color_green, "Ped In Seat");
		}
		else
		{
			grcDebugDraw::AddDebugOutput(Color_red, "Ped Not In Seat");
		}
	}

	if (CVehicleDebug::ms_bRenderEntryPointEvaluationTaskDebug)
	{
		CPed* pFocusPed = NULL;
		CEntity* pFocusEntity = CDebugScene::FocusEntities_Get(0);
		if (pFocusEntity && pFocusEntity->GetIsTypePed())
		{
			pFocusPed = static_cast<CPed*>(pFocusEntity);
		}

		if (!pFocusPed)
		{
			pFocusPed = CGameWorld::FindLocalPlayer();
		}

// 		if (CVehicleDebug::ms_EntryPointsDebug.bHasBeenSet && (pFocusPed == GetPed()))
// 		{
// 			CVehicleDebug::ms_EntryPointsDebug.RenderDebug(m_pMount, GetPed(), 1);
// 		}
	}

	Vector3 vTargetPos(Vector3::ZeroType);
	Quaternion qTargetOrientation(0.0f,0.0f,0.0f,1.0f);

	m_PlayAttachedClipHelper.GetTarget(vTargetPos, qTargetOrientation);

	Matrix34 mTemp(Matrix34::IdentityType);
	mTemp.FromQuaternion(qTargetOrientation);
	mTemp.d = vTargetPos;

	static float sfScale = 0.5f;
	grcDebugDraw::Axis(mTemp, sfScale);
#endif
}

#endif // !__FINAL

void CTaskMountAnimal::DoJackPed()
{
	if (m_pJackedPed && !m_pJackedPed->IsNetworkClone() &&
		!m_pJackedPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_DISMOUNT_ANIMAL) &&
		m_pJackedPed->GetMyMount() == m_pMount)
	{
		// Make the ped get down 
		VehicleEnterExitFlags vehicleFlags;
		vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::BeJacked);
		CEventGivePedTask givePedTask(PED_TASK_PRIORITY_EVENT_RESPONSE_TEMP, rage_new CTaskDismountAnimal(vehicleFlags, 0.0f, GetPed(), m_iTargetEntryPoint), true );				
		m_pJackedPed->GetPedIntelligence()->AddEvent(givePedTask);
	}
}

//////////////////////////////////////////////////////////////////////////
// CClonedMountAnimalInfo
//////////////////////////////////////////////////////////////////////////
CTaskFSMClone * CClonedMountAnimalInfo::CreateCloneFSMTask()
{
	return rage_new CTaskMountAnimal(GetMount(), m_iSeatRequestType, m_iSeat, m_iRunningFlags);
}

CClonedMountAnimalInfo::CClonedMountAnimalInfo() 
: CClonedMountFSMInfo()
, m_pJackedPed(NULL)
{
}

CClonedMountAnimalInfo::CClonedMountAnimalInfo( s32 enterState, CPed* pMount, SeatRequestType iSeatRequestType, s32 iSeat, s32 iTargetEntryPoint, VehicleEnterExitFlags iRunningFlags, CPed* pJackedPed)
: CClonedMountFSMInfo(enterState, pMount, iSeatRequestType, iSeat, iTargetEntryPoint, iRunningFlags)
, m_pJackedPed(pJackedPed)
{
}


//////////////////////////////////////////////////////////////////////////
// CTaskDismountAnimal
//////////////////////////////////////////////////////////////////////////
const fwMvFlagId CTaskDismountAnimal::ms_RunningDismountFlagId("RunningDismount",0xD2377589);
const fwMvFlagId CTaskDismountAnimal::ms_JumpingDismountFlagId("JumpingDismount",0x45598A83);
const fwMvFlagId CTaskDismountAnimal::ms_UseHitchVariantFlagId("UseHitchVariant", 0x6b2171a);
s32 CTaskDismountAnimal::ms_CameraInterpolationDuration	= 1500;

CTaskDismountAnimal::CTaskDismountAnimal(VehicleEnterExitFlags iRunningFlags, float fDelayTime, CPed* pPedJacker, s32 iTargetEntryPoint, bool bUseHitchVariant)
: CTaskMountFSM(NULL, SR_Specific, -1, iRunningFlags, iTargetEntryPoint)
, m_fDelayTime(fDelayTime)
, m_pPedJacker(pPedJacker)
, m_iLastCompletedState(-1)
, m_bTimerRanOut(false)
, m_ClipsReady(false)
, m_bUseHitchVariant(bUseHitchVariant)
{
	SetInternalTaskType(CTaskTypes::TASK_DISMOUNT_ANIMAL);
}

CTaskDismountAnimal::~CTaskDismountAnimal()
{
}

void CTaskDismountAnimal::GetOverriddenGameplayCameraSettings(tGameplayCameraSettings& settings) const
{
	//Blend into an appropriate camera when mounting an animal.

	const CPed *pPed = GetPed(); //Get the ped ptr.
	if(!pPed->IsLocalPlayer())
	{
		return;
	}

	settings.m_CameraHash = ATSTRINGHASH("DEFAULT_FOLLOW_PED_CAMERA", 0xfbe36564);
	settings.m_InterpolationDuration = ms_CameraInterpolationDuration;
	settings.m_AttachEntity = pPed;
}

void CTaskDismountAnimal::CleanUp()
{
	CPed* pPed = GetPed();
	if (m_moveNetworkHelper.IsNetworkAttached())
	{
		pPed->GetMovePed().ClearTaskNetwork(m_moveNetworkHelper, NORMAL_BLEND_DURATION);
	}

	ResetRagdollOnCollision( *pPed );
}

CTask::FSM_Return CTaskDismountAnimal::ProcessPreFSM()
{
	//Grab the ped.
	CPed *pPed = GetPed();

	//Ensure the mount is valid.
	if(!m_pMount)
	{
		//Assign the mount.
		m_pMount = pPed->GetMyMount();
		if(!m_pMount)
		{
			return FSM_Quit;
		}
	}
		
	//Stream the clip sets.
	StreamClipSets();

	// a network ped can be told to dismount an animal they are not currently riding on
	// the local machine if lag is high or the ped is warped onto the mount the same frame it is told to exit it
	// NOTE: Adding jacked flag to check as when a ped is jacked, it is removed from the ped early
	if(pPed && pPed->IsNetworkClone() && m_pMount != pPed->GetMyMount() && !IsFlagSet(CVehicleEnterExitFlags::BeJacked))
	{
		return FSM_Quit;
	}

	// If a timer is set, count down
	if (m_fDelayTime > 0.0f )
	{
		m_fDelayTime -= fwTimer::GetTimeStep();
		if (m_fDelayTime <= 0.0f)
		{
			m_bTimerRanOut=true;
		}
	}

	GetPed()->SetPedResetFlag( CPED_RESET_FLAG_ProcessPhysicsTasks, true );
	
	return FSM_Continue;
}	

CTask::FSM_Return CTaskDismountAnimal::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
		FSM_OnEnter
		Start_OnEnter();
	FSM_OnUpdate
		return Start_OnUpdate();	

	FSM_State(State_ReserveSeat)
		FSM_OnUpdate
		return ReserveSeat_OnUpdate();	

	FSM_State(State_ExitSeat)
		FSM_OnEnter
		ExitSeat_OnEnter();
	FSM_OnUpdate
		return ExitSeat_OnUpdate();

	FSM_State(State_UnReserveSeatToFinish)
		FSM_OnUpdate
		return UnReserveSeatToFinish_OnUpdate();	

	FSM_State(State_Finish)
		FSM_OnUpdate
		return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskDismountAnimal::UpdateClonedFSM( s32 iState, CTask::FSM_Event iEvent )
{
	// if the state change is not handled by the clone FSM block below
	// we need to keep the state in sync with the values received from the network
	if(!StateChangeHandledByCloneFSM(iState))
	{
		if(iEvent == OnUpdate)
		{
			if(GetStateFromNetwork() != GetState())
			{
				SetState(GetStateFromNetwork());
				return FSM_Continue;
			}
		}
	}

	FSM_Begin

		FSM_State(State_Start)
		FSM_OnEnter
		Start_OnEnterClone();
	FSM_OnUpdate
		Start_OnUpdateClone();

	FSM_State(State_ExitSeat)
		FSM_OnEnter
		ExitSeat_OnEnter();
	FSM_OnUpdate
		return ExitSeat_OnUpdate();

	FSM_State(State_Finish)
		FSM_OnUpdate
		return FSM_Quit;
	
		FSM_State(State_ReserveSeat)
		FSM_State(State_UnReserveSeatToFinish)		

		FSM_End
}

bool CTaskDismountAnimal::StateChangeHandledByCloneFSM(s32 iState)
{
	switch(iState)
	{
	case State_Start:	
	case State_ExitSeat:	
	case State_Finish:
		return true;
	default:
		return false;
	}
}

CTask::FSM_Return CTaskDismountAnimal::ProcessPostFSM()
{
	CPed* pPed = GetPed();

	// If the task should currently have the seat reserved, check that it does
	if (GetState() > State_ReserveSeat && GetState() < State_UnReserveSeatToFinish && !IsFlagSet(CVehicleEnterExitFlags::BeJacked))
	{
		CComponentReservation* pComponentReservation = m_pMount->GetComponentReservationMgr()->GetSeatReservation(m_iTargetEntryPoint, SA_directAccessSeat);
		if (pComponentReservation && pComponentReservation->GetPedUsingComponent() == pPed)
		{
			pComponentReservation->SetPedStillUsingComponent(pPed);
		}
	}

	return FSM_Continue;
}

CTaskInfo* CTaskDismountAnimal::CreateQueriableState() const
{
	return rage_new CClonedDismountAnimalInfo(GetState(), m_pMount, m_iSeatRequestType, m_iSeat, m_iTargetEntryPoint, m_iRunningFlags, m_pPedJacker);
}

void CTaskDismountAnimal::ReadQueriableState( CClonedFSMTaskInfo* pTaskInfo )
{
	//Ensure the task info is the correct type.
	taskAssert(pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_DISMOUNT_ANIMAL_CLONED);
	
	//Grab the specific task info.
	CClonedDismountAnimalInfo* pInfo = static_cast<CClonedDismountAnimalInfo *>(pTaskInfo);
	
	//Call the base class version.
	//Syncs the base class state.
	CTaskMountFSM::ReadQueriableState(pTaskInfo);
	
	//Sync the data.
	m_pPedJacker = pInfo->GetPedJacker();
}


void CTaskDismountAnimal::OnCloneTaskNoLongerRunningOnOwner()
{
	SetStateFromNetwork(State_Finish);
}

void CTaskDismountAnimal::Start_OnEnter()
{
	CPed* pPed = GetPed();
	taskAssert(m_pMount);
	m_iSeat = m_pMount->GetSeatManager()->GetPedsSeatIndex(pPed);
	if (m_iTargetEntryPoint == -1 && taskVerifyf(m_iSeat != -1, "Invalid ped seat index in CTaskExitVehicle::PickDoor_OnUpdate"))
	{
		m_iTargetEntryPoint = CModelSeatInfo::EvaluateSeatAccessFromAllEntryPoints(pPed, m_pMount, m_iSeatRequestType, m_iSeat, false, m_iRunningFlags);
	}
		
	m_ClipsReady = false;

	SetUpRagdollOnCollision( *pPed, *m_pMount );
}

void CTaskDismountAnimal::SetUpRagdollOnCollision(CPed& rPed, CPed& rMount)
{
	rPed.SetRagdollOnCollisionIgnorePhysical(&rMount);
}

void CTaskDismountAnimal::ResetRagdollOnCollision(CPed& rPed)
{
	rPed.SetRagdollOnCollisionIgnorePhysical(NULL);
}

void CTaskDismountAnimal::Start_OnEnterClone()
{
}

CTask::FSM_Return CTaskDismountAnimal::Start_OnUpdateClone()
{
	if(GetStateFromNetwork() == State_Finish)
	{
		SetState(State_Finish);
	}
	else if(GetStateFromNetwork() > State_Start)
	{
		if (m_ClipsReady)
		{
			CPed* pPed = GetPed();

			if (m_iSeat == m_pMount->GetSeatManager()->GetPedsSeatIndex(pPed))
			{
				// Get the clip info for the current seat the ped is in
				const CVehicleEntryPointAnimInfo* pClipInfo = m_pMount->GetPedModelInfo()->GetLayoutInfo()->GetEntryPointAnimInfo(m_iTargetEntryPoint);
				taskFatalAssertf(pClipInfo, "NULL Clip Info for index %i", m_iTargetEntryPoint);

				m_moveNetworkHelper.CreateNetworkPlayer(pPed, CClipNetworkMoveInfo::ms_NetworkTaskDismountAnimal);
				pPed->GetMovePed().SetTaskNetwork(m_moveNetworkHelper.GetNetworkPlayer(), NORMAL_BLEND_DURATION);
				m_moveNetworkHelper.SetClipSet(pClipInfo->GetEntryPointClipSetId());

				SetState(GetStateFromNetwork());
			}
			else
			{
				// quit the task if the seat information isn't up-to-date
				SetState(State_Finish);
			}
		}
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskDismountAnimal::Start_OnUpdate()
{
	// If we don't have a valid exit point we need to quit
	if (m_iTargetEntryPoint == -1)
	{			
		SetState(State_Finish);
		return FSM_Continue;
	}	

	if (m_ClipsReady) {
		//Set up dismount clip/network		
		const CVehicleEntryPointAnimInfo* pEntryPointClipInfo = m_pMount->GetPedModelInfo()->GetLayoutInfo()->GetEntryPointAnimInfo(m_iTargetEntryPoint);
		taskFatalAssertf(pEntryPointClipInfo, "NULL Clip Info for entry index %i", m_iTargetEntryPoint);

		CPed* pPed = GetPed();
		m_moveNetworkHelper.CreateNetworkPlayer(pPed, CClipNetworkMoveInfo::ms_NetworkTaskDismountAnimal);

		pPed->GetMovePed().SetTaskNetwork(m_moveNetworkHelper, NORMAL_BLEND_DURATION);
		m_moveNetworkHelper.SetClipSet(pEntryPointClipInfo->GetEntryPointClipSetId());	

		// When being jacked, the ped jacking us will have reserved the seat
		if (!IsFlagSet(CVehicleEnterExitFlags::BeJacked))
			SetState(State_ReserveSeat);
		else
			SetState(State_ExitSeat);
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskDismountAnimal::ReserveSeat_OnUpdate()
{
	CComponentReservation* pComponentReservation = m_pMount->GetComponentReservationMgr()->GetSeatReservation(m_iTargetEntryPoint, SA_directAccessSeat);
	if (CComponentReservationManager::ReserveVehicleComponent(GetPed(), m_pMount, pComponentReservation))
	{
		SetState(State_ExitSeat);
	}
	return FSM_Continue;
}


void CTaskDismountAnimal::ExitSeat_OnEnter()
{	
	SetNewTask(rage_new CTaskDismountSeat(m_pMount, m_iTargetEntryPoint, true));	
	m_pMount->GetPedIntelligence()->ClearTasks();
	bool bIsRunningDismount = false;
	
	if (m_pMount->GetMotionData()->GetCurrentMbrY() >= MOVEBLENDRATIO_RUN || m_pMount->GetIsSwimming() )
	{
		const fwClipSet *pClipSet = m_moveNetworkHelper.GetClipSet();
		if (pClipSet && pClipSet->GetClip(fwMvClipId("dismount_run",0x1FE93161)) != NULL)
		{
			m_moveNetworkHelper.SetFlag(true, ms_RunningDismountFlagId);
			bIsRunningDismount = true;
		}		
	}	

	if (m_bUseHitchVariant)
	{
		m_moveNetworkHelper.SetFlag(true, ms_UseHitchVariantFlagId);
	}

	m_pMount->GetPedIntelligence()->AddTaskAtPriority(
		rage_new CTaskGettingDismounted(m_iTargetEntryPoint, m_iSeat, bIsRunningDismount, IsFlagSet(CVehicleEnterExitFlags::BeJacked)),
		PED_TASK_PRIORITY_EVENT_RESPONSE_TEMP,
		false);

	DetachReins();
}

CTask::FSM_Return CTaskDismountAnimal::ExitSeat_OnUpdate()
{
	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		m_iLastCompletedState = GetState();		
		
		if (GetPed()->IsNetworkClone())
		{
			SetState(State_Finish);
		}
		else
		{
			SetState(State_UnReserveSeatToFinish);
		}
	}
	else
	{
		bool bIsRunningDismount = false;
		if (m_pMount->GetMotionData()->GetCurrentMbrY() >= MOVEBLENDRATIO_RUN)
		{	
			bIsRunningDismount = true;
		}

		CTask* pTask = m_pMount->GetPedIntelligence()->GetTaskActive();
		CTaskDismountSeat* pDismountSeatTask = static_cast<CTaskDismountSeat*>(GetSubTask());
		if (!pDismountSeatTask->GetDismountReady() && (!pTask || pTask->GetTaskType() != CTaskTypes::TASK_GETTING_DISMOUNTED))
		{ //something happened start the getting dismounted task again
			m_pMount->GetPedIntelligence()->AddTaskAtPriority(
				rage_new CTaskGettingDismounted(m_iTargetEntryPoint, m_iSeat, bIsRunningDismount, IsFlagSet(CVehicleEnterExitFlags::BeJacked)),
				PED_TASK_PRIORITY_EVENT_RESPONSE_TEMP,
				false);
		}
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskDismountAnimal::UnReserveSeatToFinish_OnUpdate()
{
	CPed* pPed = GetPed();
	CComponentReservation* pComponentReservation = m_pMount->GetComponentReservationMgr()->GetSeatReservation(m_iTargetEntryPoint, SA_directAccessSeat);
	if (pComponentReservation && pComponentReservation->IsComponentReservedForPed(pPed))
	{
		CComponentReservationManager::UnreserveVehicleComponent(pPed, pComponentReservation);
	}

	SetState(State_Finish);
	return FSM_Continue;
}

void CTaskDismountAnimal::DetachReins()
{
	CHorseComponent* horseComponent = m_pMount->GetHorseComponent();
	if(horseComponent)
	{
		CReins* pReins = horseComponent->GetReins();
		if( pReins )
		{
			pReins->Detach( GetPed() );
			pReins->SetHasRider( false );
		}
	}
}


void CTaskDismountAnimal::StartDismountAnimation()
{
	if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_DISMOUNT_SEAT)
	{
		return static_cast<CTaskDismountSeat*>(GetSubTask())->SetDismountReady();
	}
}

void CTaskDismountAnimal::StreamClipSets()
{
	//Note that the clips are not ready.
	m_ClipsReady = false;

	//Ensure the entry point is valid.
	if(m_iTargetEntryPoint < 0)
		return;

	//Grab the clip info for the entry point.
	const CVehicleEntryPointAnimInfo* pEntryInfo = m_pMount->GetPedModelInfo()->GetLayoutInfo()->GetEntryPointAnimInfo(m_iTargetEntryPoint);
	if(!pEntryInfo)
		return;

	//Request the clip set for the entry point.
	if(!m_EntryClipSetRequestHelper.Request(pEntryInfo->GetEntryPointClipSetId()))
		return;

	//Note that the clips are ready.
	m_ClipsReady = true;
}


#if !__FINAL
const char * CTaskDismountAnimal::GetStaticStateName( s32 iState )
{
	taskAssert(iState >= State_Start && iState <= State_Finish);

	switch (iState)
	{
	case State_Start:							return "State_Start";
	case State_ReserveSeat:						return "State_ReserveSeat";	
	case State_ExitSeat:						return "State_ExitSeat";	
	case State_UnReserveSeatToFinish:			return "State_UnReserveSeatToFinish";	
	case State_Finish:							return "State_Finish";
	default: taskAssert(0); 	
	}	
	return "State_Invalid";
}

void CTaskDismountAnimal::Debug() const
{
#if __BANK
	// Base Class
	CTask::Debug();

	CPed* pFocusPed = NULL;

	CEntity* pFocusEntity = CDebugScene::FocusEntities_Get(0);
	if (pFocusEntity && pFocusEntity->GetIsTypePed())
	{
		pFocusPed = static_cast<CPed*>(pFocusEntity);
	}

	if (!pFocusPed)
	{
		pFocusPed = CGameWorld::FindLocalPlayer();
	}

	if (GetPed() == pFocusPed)
	{
		grcDebugDraw::AddDebugOutput(Color_green, "Target Exitpoint : %u", m_iTargetEntryPoint);
		if (pFocusPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle))
		{
			grcDebugDraw::AddDebugOutput(Color_green, "Ped In Seat");
		}
		else
		{
			grcDebugDraw::AddDebugOutput(Color_red, "Ped Not In Seat");
		}
	}

	if (CVehicleDebug::ms_bRenderEntryPointEvaluationTaskDebug)
	{
		CPed* pFocusPed = NULL;
		CEntity* pFocusEntity = CDebugScene::FocusEntities_Get(0);
		if (pFocusEntity && pFocusEntity->GetIsTypePed())
		{
			pFocusPed = static_cast<CPed*>(pFocusEntity);
		}

		if (!pFocusPed)
		{
			pFocusPed = CGameWorld::FindLocalPlayer();
		}

// 		if (CVehicleDebug::ms_EntryPointsDebug.bHasBeenSet && (pFocusPed == GetPed()))
// 		{
// 			CVehicleDebug::ms_EntryPointsDebug.RenderDebug(m_pMount, GetPed(), 0);
// 		}
	}
#endif
}
#endif // !__FINAL


//////////////////////////////////////////////////////////////////////////
// CClonedDismountAnimalInfo
//////////////////////////////////////////////////////////////////////////
CClonedDismountAnimalInfo::CClonedDismountAnimalInfo()
: CClonedMountFSMInfo()
, m_pPedJacker(NULL)
{
}

CClonedDismountAnimalInfo::CClonedDismountAnimalInfo( s32 enterState, CPed* pMount, SeatRequestType iSeatRequestType, s32 iSeat, s32 iTargetEntryPoint, VehicleEnterExitFlags iRunningFlags, CPed* pPedJacker)
: CClonedMountFSMInfo(enterState, pMount, iSeatRequestType, iSeat, iTargetEntryPoint, iRunningFlags)
, m_pPedJacker(pPedJacker)
{
}

CTaskFSMClone * CClonedDismountAnimalInfo::CreateCloneFSMTask()
{
	return rage_new CTaskDismountAnimal(m_iRunningFlags, 0.0f, GetPedJacker(), m_iTargetEntryPoint);
}

CTask * CClonedDismountAnimalInfo::CreateLocalTask(fwEntity* UNUSED_PARAM(pEntity))
{
	return CreateCloneFSMTask();
}



//////////////////////////////////////////////////////////////////////////
// CTaskMountSeat
//////////////////////////////////////////////////////////////////////////

const fwMvBooleanId CTaskMountSeat::ms_EnterSeatClipFinishedId("EnterSeatAnimFinished",0x40F24436);
const fwMvBooleanId CTaskMountSeat::ms_EnterSeat_OnEnterId("EnterSeat_OnEnter",0x5fe9a198);
const fwMvFlagId CTaskMountSeat::ms_HasMountQuickAnimId("HasMountQuickAnim", 0xd15876fa);
const fwMvFlagId CTaskMountSeat::ms_UseFarMountId("UseFarMount", 0xf5f1a1fa);
const fwMvRequestId CTaskMountSeat::ms_EnterSeatId("EnterSeat",0xAE333226); 
const fwMvClipId CTaskMountSeat::ms_EnterSeatClipId("EnterSeatClip",0x6981AB9F);
const fwMvFlagId CTaskMountSeat::ms_QuickMountId("QuickMount",0x335183D1);
const fwMvFlagId CTaskMountSeat::ms_AirMountId("AirMount",0xaafa4f21);
const fwMvFloatId CTaskMountSeat::ms_MountDirectionId("MountDirection",0x3129adf2);
const fwMvFloatId CTaskMountSeat::ms_EnterSeatPhaseId("EnterSeatPhase",0x17B70BB5);
const fwMvFloatId CTaskMountSeat::ms_MountZoneId("MountZone", 0x405ddc2e);


CTaskMountSeat::CTaskMountSeat(const VehicleEnterExitFlags& iRunningFlags, CPed* pMount, const s32 iTargetSeat, bool bQuickMount, float fMountZone)
: m_iRunningFlags(iRunningFlags)
, m_pMount(pMount) 
, m_bMountReady(false)
, m_bFinalizeMount(false)
, m_bQuickMount(bQuickMount)
, m_fRStirrupPhase(0.5f)
, m_fLStirrupPhase(0.5f)
, m_fMountZone(fMountZone)
, m_iSeat(iTargetSeat)
{
	SetInternalTaskType(CTaskTypes::TASK_MOUNT_SEAT);
}

CTaskMountSeat::~CTaskMountSeat()
{

}

void CTaskMountSeat::CleanUp()
{
	if (m_bFinalizeMount)
	{
		FinalizeMount(GetPed());
	}
}

CTask::FSM_Return CTaskMountSeat::ProcessPreFSM()
{
	if (!taskVerifyf(m_pMount, "NULL mount pointer in CTaskMountSeat::ProcessPreFSM"))
	{
		return FSM_Quit;
	}

	if (!taskVerifyf(GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_MOUNT_ANIMAL, "Invalid parent task"))
	{
		return FSM_Quit;
	}

	GetPed()->SetPedResetFlag( CPED_RESET_FLAG_ProcessPostPreRender, true );

	return FSM_Continue;
}	

CTask::FSM_Return CTaskMountSeat::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
		FSM_OnUpdate
		return Start_OnUpdate();	

	FSM_State(State_EnterSeat)
		FSM_OnEnter
		EnterSeat_OnEnter();
	FSM_OnUpdate
		return EnterSeat_OnUpdate();

	FSM_State(State_Finish)
		FSM_OnUpdate
		return FSM_Quit;

	FSM_End
}

bool CTaskMountSeat::ProcessPostPreRender() 
{
	//Stirrups
	CPed* rider = GetPed();
	if(rider)
	{
		CPed* horse = rider->GetMyMount();
		if (horse)
		{
			CTaskMotionRideHorse::UpdateStirrups(rider, horse);
		}
	}

	return false;
}

CTask::FSM_Return CTaskMountSeat::UpdateClonedFSM( s32 iState, CTask::FSM_Event iEvent )
{
	return UpdateFSM(iState, iEvent); 
}

CTaskInfo* CTaskMountSeat::CreateQueriableState() const
{
	return rage_new CTaskInfo();
}

CTask::FSM_Return CTaskMountSeat::Start_OnUpdate()
{

	m_pParentNetworkHelper = static_cast<CTaskMountAnimal*>(GetParent())->GetMoveNetworkHelper();

	if (!m_pParentNetworkHelper)
	{
		SetState(State_Finish);
		return FSM_Continue;
	}

	//m_bMountReady=true; //disabling the SetMountReady functionality for B* 486256, if synching becomes a problem need a different solution.
	//if (m_bMountReady) {
	SetState(State_EnterSeat);
	//}
	return FSM_Continue;
}

void CTaskMountSeat::EnterSeat_OnEnter()
{	
	CPed* pPed = GetPed();
	m_pParentNetworkHelper->SendRequest( ms_EnterSeatId);
	m_pParentNetworkHelper->WaitForTargetState(ms_EnterSeat_OnEnterId); 	
	m_bFinalizeMount = true;

	TUNE_GROUP_FLOAT (TASK_MOUNT,fLONGMOUNT_RADIUS, 1.0f, 0.0f, 10.0f, 0.01f);
	if (fabs(m_fMountZone)>0.5f) //Temp until slow versions exist(?)
		m_bQuickMount = true;

	Vector3 vPedToHorse = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	vPedToHorse.Subtract(VEC3V_TO_VECTOR3(m_pMount->GetTransform().GetPosition()));
	vPedToHorse.z=0.0f;
	float fFlatDistanceFromPedToTargetSq = vPedToHorse.XYMag2();
	if (fFlatDistanceFromPedToTargetSq > (fLONGMOUNT_RADIUS*fLONGMOUNT_RADIUS))
		m_bQuickMount = true;

	const fwClipSet *pClipSet = m_pParentNetworkHelper->GetClipSet();
	if (m_bQuickMount)
	{
		m_pParentNetworkHelper->SetFlag(pClipSet && (pClipSet->GetClip(fwMvClipId("mount_quick",0xFF2BA7F8)) != NULL), ms_HasMountQuickAnimId);
		if (pClipSet && pClipSet->GetClip(fwMvClipId("mount_quick_left",0x7ae3f360)) == NULL)
			m_pParentNetworkHelper->SetFloat(ms_MountZoneId, 0.0f);
		else
			m_pParentNetworkHelper->SetFloat(ms_MountZoneId, m_fMountZone);		
	}	
	m_pParentNetworkHelper->SetFlag(m_bQuickMount, ms_QuickMountId);

	if (m_iRunningFlags.BitSet().IsSet(CVehicleEnterExitFlags::DropEntry))
	{
		m_pParentNetworkHelper->SetFlag(true, ms_AirMountId);	
		m_pParentNetworkHelper->SetFloat(ms_MountDirectionId, 0.0f);
	}
	else if (m_iRunningFlags.BitSet().IsSet(CVehicleEnterExitFlags::JumpEntry))
	{
		m_pParentNetworkHelper->SetFlag(true, ms_AirMountId);	
		//check side
		vPedToHorse.Normalize();
		float fDot = vPedToHorse.Dot(VEC3V_TO_VECTOR3(m_pMount->GetTransform().GetA()));
		m_pParentNetworkHelper->SetFloat(ms_MountDirectionId, fDot);
	}
	else
		m_pParentNetworkHelper->SetFlag(false, ms_AirMountId);	

	//Distance
	static dev_float sf_FarMountDistance = 2.0f; 
	bool bFarMount = false;
	if (fFlatDistanceFromPedToTargetSq > (sf_FarMountDistance*sf_FarMountDistance))
	{
		if (pClipSet && pClipSet->GetClip(fwMvClipId("mount_quick_far",0x10e14162)))
		{
			bFarMount=true;
		}
	}
	m_pParentNetworkHelper->SetFlag(bFarMount, ms_UseFarMountId);

	m_bMountReady=false;
}

CTask::FSM_Return CTaskMountSeat::EnterSeat_OnUpdate()
{
	CPed* pPed = GetPed();

	if (!m_pParentNetworkHelper->IsInTargetState())
	{
		m_pParentNetworkHelper->SendRequest( ms_EnterSeatId );
		return FSM_Continue;
	}

	if (m_bMountReady && !pPed->GetMyMount()) {//triggers horse attachment 1 frame later
		pPed->SetPedOnMount(m_pMount, m_iSeat, true, false);
		static_cast<CTaskMountAnimal*>(GetParent())->UpdateMountAttachmentOffsets(); //force an attachment offset update
	} else
		m_bMountReady=true;

	float mountPhase = 0.0f;
	const crClip* pClip = GetClipAndPhaseForState(mountPhase);
	static dev_float MOUNT_BLENDOUT_PHASE = 0.8f;
	Vector4 horsePhase;
	float horseTurnPhase, horseSpeed;
	float horseSlopePhase = 0.5f;
	if (m_pMount && m_pMount->GetPedIntelligence()->GetMotionTaskActiveSimplest()->GetTaskType() == CTaskTypes::TASK_ON_FOOT_HORSE) {
		static_cast<CTaskHorseLocomotion*>(m_pMount->GetPedIntelligence()->GetMotionTaskActiveSimplest())->GetHorseGaitPhaseAndSpeed(horsePhase, horseSpeed, horseTurnPhase, horseSlopePhase);
	}
	if (m_pParentNetworkHelper->GetBoolean( ms_EnterSeatClipFinishedId) || (mountPhase >=MOUNT_BLENDOUT_PHASE && (horseSlopePhase >= 0.55f || horseSlopePhase <= 0.45f)))
	{		
		FinalizeMount(pPed);

		SetState(State_Finish);
	}	
	if (pClip)
	{
		if (m_fRStirrupPhase==0.5f)
			CClipEventTags::FindEventPhase<crPropertyAttributeBool, bool>(pClip, CClipEventTags::Foot, CClipEventTags::Right, true, m_fRStirrupPhase);
		if (m_fLStirrupPhase==0.5f)
			CClipEventTags::FindEventPhase<crPropertyAttributeBool, bool>(pClip, CClipEventTags::Foot, CClipEventTags::Right, false, m_fLStirrupPhase);
	} 
	
	if ( (mountPhase > m_fRStirrupPhase || mountPhase > m_fLStirrupPhase) ) {		
		if (mountPhase > m_fRStirrupPhase)
			m_pMount->GetHorseComponent()->SetStirrupEnable(true, true);
		if (mountPhase > m_fLStirrupPhase)
			m_pMount->GetHorseComponent()->SetStirrupEnable(false, true);
	}

	return FSM_Continue;
}

const crClip* CTaskMountSeat::GetClipAndPhaseForState(float& fPhase)
{
	if (m_pParentNetworkHelper)
	{
		switch (GetState())
		{
		case State_EnterSeat: 
			{
				if (GetPed()->GetMyMount()==NULL)
					return NULL;
				fPhase = m_pParentNetworkHelper->GetFloat( ms_EnterSeatPhaseId);
				fPhase = rage::Clamp(fPhase, 0.0f, 1.0f);
				return m_pParentNetworkHelper->GetClip( ms_EnterSeatClipId);
			}
		case State_Start:
		case State_Finish: break;
		default: taskAssertf(0, "Unhandled State");
		}
	}
	return NULL;
}

void CTaskMountSeat::FinalizeMount(CPed* pPed)
{
	if ( !pPed->GetMyMount()) {
		pPed->SetPedOnMount(m_pMount, m_iSeat, true, false);
	}

	//clear offset
	if (pPed->GetAttachmentExtension())
	{
		pPed->GetAttachmentExtension()->SetAttachOffset(ORIGIN);
		pPed->GetAttachmentExtension()->SetAttachQuat(Quaternion(0.0f,0.0f,0.0f,1.0f));
	}

	//Set new motion task
	pPed->StartPrimaryMotionTask();

	m_bFinalizeMount = false;
}

#if !__FINAL
const char * CTaskMountSeat::GetStaticStateName( s32 iState )
{
	taskAssert(iState >= State_Start && iState <= State_Finish);

	switch (iState)
	{
	case State_Start:							return "State_Start";
	case State_EnterSeat:						return "State_EnterSeat";
	case State_Finish:							return "State_Finish";
	default: taskAssert(0); 	
	}

	return "State_Invalid";
}

void CTaskMountSeat::Debug() const
{
#if __BANK
	Mat34V mPedMatrix;
	GetPed()->GetMatrixCopy(RC_MATRIX34(mPedMatrix));
	grcDebugDraw::Axis(mPedMatrix, 0.1f, true);	
#endif
}
#endif // !__FINAL

//////////////////////////////////////////////////////////////////////////
// CTaskGettingMounted
//////////////////////////////////////////////////////////////////////////

CTaskGettingMounted::CTaskGettingMounted(CTaskMountAnimal* pRiderTask, s32 iEntryPoint)
: m_pRiderTaskMountAnimal(pRiderTask)
, m_iTargetEntryPoint(iEntryPoint)
{
	SetInternalTaskType(CTaskTypes::TASK_GETTING_MOUNTED);
}

CTask::FSM_Return CTaskGettingMounted::ProcessPreFSM()
{
	return FSM_Continue;
}

CTask::FSM_Return CTaskGettingMounted::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
		FSM_OnUpdate
		return Start_OnUpdate();	

	FSM_State(State_EnterSeat)	
		FSM_OnUpdate
		return EnterSeat_OnUpdate();

	FSM_State(State_Finish)
		FSM_OnUpdate
		return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskGettingMounted::UpdateClonedFSM( s32 iState, CTask::FSM_Event iEvent )
{
	return UpdateFSM(iState, iEvent); 
}

CTaskInfo* CTaskGettingMounted::CreateQueriableState() const
{
	return rage_new CTaskInfo();
}

CTask::FSM_Return CTaskGettingMounted::Start_OnUpdate()
{
	CPed* pPed = GetPed();

	Vector3 rotation;
	MAT34V_TO_MATRIX34( pPed->GetMatrix()).ToEulersFastXYZ(rotation);	
	static const dev_float INCLINE_LIMIT = 0.087f;//~5degrees
	if (fabs(rotation.x)>INCLINE_LIMIT) 
	{
		//m_pRiderTaskMountAnimal->StartMountAnimation(); //disabling the SetMountReady functionality for B* 486256, if synching becomes a problem need a different solution.
		SetState(State_Finish);
		return FSM_Continue;
	}

	if (!m_pRiderTaskMountAnimal->m_ClipsReady)
		return FSM_Continue;

	//Grab the clip info for the entry point.
	const CVehicleEntryPointAnimInfo* pEntryInfo = pPed->GetPedModelInfo()->GetLayoutInfo()->GetEntryPointAnimInfo(m_iTargetEntryPoint);
	if(!pEntryInfo)
		return FSM_Continue;

	static const fwMvClipId MOUNTED_CLIP("hrs_mount",0x499AAC18);
	fwClipSet* pClipSet = fwClipSetManager::GetClipSet(pEntryInfo->GetEntryPointClipSetId());
	if (!pClipSet || !pClipSet->GetClip(MOUNTED_CLIP))
	{
		SetState(State_Finish);
		return FSM_Continue;
	}

	StartClipBySetAndClip(pPed, pEntryInfo->GetEntryPointClipSetId(), MOUNTED_CLIP, NORMAL_BLEND_IN_DELTA);
	SetState(State_EnterSeat);
	//m_pRiderTaskMountAnimal->StartMountAnimation(); //disabling the SetMountReady functionality for B* 486256, if synching becomes a problem need a different solution.
	return FSM_Continue;
}

CTask::FSM_Return CTaskGettingMounted::EnterSeat_OnUpdate()
{
	if (!GetClipHelper() || //anim done
		(GetPed()->GetMotionData()->GetCurrentMbrY() > 0.25f ))  // or ped wants to move now!
	{
		SetState(State_Finish);
	}

	return FSM_Continue;
}

void CTaskGettingMounted::CleanUp() 
{
}

#if !__FINAL
const char * CTaskGettingMounted::GetStaticStateName( s32 iState )
{
	taskAssert(iState >= State_Start && iState <= State_Finish);

	switch (iState)
	{
	case State_Start:							return "State_Start";
	case State_EnterSeat:						return "State_EnterSeat";
	case State_Finish:							return "State_Finish";
	default: taskAssert(0); 	
	}

	return "State_Invalid";
}

void CTaskGettingMounted::Debug() const
{
}
#endif // !__FINAL

//////////////////////////////////////////////////////////////////////////
// CTaskGettingDismounted
//////////////////////////////////////////////////////////////////////////

CTaskGettingDismounted::CTaskGettingDismounted(s32 iEntryPoint, const s32 iSeat, bool bIsRunningDismount, bool bIsJacked)
: m_iTargetEntryPoint(iEntryPoint)
, m_bIsRunningDismount(bIsRunningDismount)
, m_bIsJacked(bIsJacked)
, m_iSeat(iSeat)
{
	SetInternalTaskType(CTaskTypes::TASK_GETTING_DISMOUNTED);
}

CTask::FSM_Return CTaskGettingDismounted::ProcessPreFSM()
{
	return FSM_Continue;
}

CTask::FSM_Return CTaskGettingDismounted::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
		FSM_OnUpdate
		return Start_OnUpdate();	

	FSM_State(State_Dismount)	
		FSM_OnUpdate
		return Dismount_OnUpdate();

	FSM_State(State_RunningDismount)	
		FSM_OnUpdate
		return RunningDismount_OnUpdate();

	FSM_State(State_SlopedDismount)	
		FSM_OnUpdate
		return SlopedDismount_OnUpdate();

	FSM_State(State_Finish)
		FSM_OnUpdate
		return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskGettingDismounted::UpdateClonedFSM( s32 iState, CTask::FSM_Event iEvent )
{
	FSM_Begin

		FSM_State(State_Start)
		FSM_OnUpdate
		return Start_OnUpdate();	

	FSM_State(State_Dismount)	
		FSM_OnUpdate
		return Dismount_OnUpdate();

	FSM_State(State_RunningDismount)	
		FSM_OnUpdate
		return RunningDismount_OnUpdate();

	FSM_State(State_SlopedDismount)	
		FSM_OnUpdate
		return SlopedDismount_OnUpdate();

	FSM_State(State_Finish)
		FSM_OnUpdate
		return FSM_Quit;

	FSM_End
}

CTaskInfo* CTaskGettingDismounted::CreateQueriableState() const
{
	return rage_new CTaskInfo();
}

CTask::FSM_Return CTaskGettingDismounted::Start_OnUpdate()
{
	CPed* pPed = GetPed();
	CPed* pRider = pPed->GetSeatManager()->GetPedInSeat(m_iSeat);
	if (!pRider)
	{
		SetState(State_Finish);
		return FSM_Continue;
	}
	CTaskDismountAnimal* pRiderTaskDismountAnimal = static_cast<CTaskDismountAnimal*>(pRider->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_DISMOUNT_ANIMAL));
	//Grab the clip info for the entry point.
	const CVehicleEntryPointAnimInfo* pEntryInfo = pPed->GetPedModelInfo()->GetLayoutInfo()->GetEntryPointAnimInfo(m_iTargetEntryPoint);
	if(!pEntryInfo)
		return FSM_Continue;

	if (!pRiderTaskDismountAnimal->m_EntryClipSetRequestHelper.Request(pEntryInfo->GetEntryPointClipSetId()))
		return FSM_Continue;

	if (m_bIsRunningDismount )
	{
		SetState( State_RunningDismount );
	}
	else
	{
	Vector3 rotation;
	MAT34V_TO_MATRIX34( pPed->GetMatrix()).ToEulersFastXYZ(rotation);	
	static const dev_float INCLINE_LIMIT = 0.087f;//~5degrees
	if( fabs(rotation.x)>INCLINE_LIMIT )
	{
		SetState( State_SlopedDismount );
	}	
	else
	{
		static const fwMvClipId DISMOUNTED_CLIP("hrs_dismount",0xF899B27);
		static const fwMvClipId JACKEDDISMOUNT_CLIP("hrs_hijack",0x6279D17E);		
		StartClipBySetAndClip(pPed, pEntryInfo->GetEntryPointClipSetId(), m_bIsJacked ? JACKEDDISMOUNT_CLIP : DISMOUNTED_CLIP, NORMAL_BLEND_IN_DELTA);
		SetState(State_Dismount);

		// If getting jacked, pull the ped off immediately so the new rider can get on immediately
		if (m_bIsJacked)
		{
			// Commenting this out for now as it seems to pop the rider off the horse immediately rather than letting the rider play the dismount animation - CR 9/25/13
			//pRider->SetPedOffMount(false);

			//Check if the ped has a network object.
			netObject* pObject = pRider->GetNetworkObject();
			if(pObject)
			{
				//Ignore mount updates for a while, otherwise the network sync code
				//may decide to place the ped back on the mount for a split second.
				static_cast<CNetObjPed *>(pObject)->IgnoreTargetMountUpdatesForAWhile();
			}	
		}
	}
	}

	pRiderTaskDismountAnimal->StartDismountAnimation();
	return FSM_Continue;
}

CTask::FSM_Return CTaskGettingDismounted::Dismount_OnUpdate()
{
	if (!GetClipHelper()) //anim done
	{
		SetState(State_Finish);
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskGettingDismounted::RunningDismount_OnUpdate()
{
	CPed* pPed = GetPed();
	static dev_float s_frunningDismountTimeout = 5.0f;
	//don't change tasks until the horse is done running
	//unless the horse keeps running for a very long time due to some external factors.
	if ( pPed->GetMotionData()->GetCurrentMbrY() < MOVEBLENDRATIO_WALK || GetTimeInState() > s_frunningDismountTimeout ) 
	{
		SetState(State_Finish);
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskGettingDismounted::SlopedDismount_OnUpdate()
{
	CPed* pPed = GetPed();
	CPed* pRider = pPed->GetSeatManager()->GetPedInSeat(m_iSeat); // don't change the task until there is no rider.
	if (!pRider)
	{
		SetState(State_Finish);
	}

	return FSM_Continue;
}

void CTaskGettingDismounted::CleanUp()
{
	//Check if the horse is not a clone.
	CPed* pPed = GetPed();
	if(!pPed->IsNetworkClone())
	{
		//Clear the horse tasks.
		if( !pPed->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_MOVE_STAND_STILL) )
		{
			CEventGivePedTask primaryTaskEvent(PED_TASK_PRIORITY_DEFAULT, rage_new CTaskDoNothing(-1));
			pPed->GetPedIntelligence()->AddEvent(primaryTaskEvent, true, true);
			pPed->GetPedIntelligence()->GetTaskManager()->SetTask(PED_TASK_TREE_MOVEMENT, rage_new CTaskMoveStandStill(), PED_TASK_MOVEMENT_DEFAULT);
			pPed->SetDesiredHeading(pPed->GetCurrentHeading());
		}
	}

	// Base class
	CTask::CleanUp();
}


#if !__FINAL
const char * CTaskGettingDismounted::GetStaticStateName( s32 iState )
{
	taskAssert(iState >= State_Start && iState <= State_Finish);

	switch (iState)
	{
	case State_Start:							return "State_Start";
	case State_Dismount:						return "State_Dismount";
	case State_RunningDismount:					return "State_RunningDismount";
	case State_SlopedDismount:					return "State_SlopedDismount";
	case State_Finish:							return "State_Finish";
	default: taskAssert(0); 	
	}

	return "State_Invalid";
}

void CTaskGettingDismounted::Debug() const
{
	if(GetSubTask())
		GetSubTask()->Debug();
}
#endif // !__FINAL

//////////////////////////////////////////////////////////////////////////
// CTaskDismountSeat
//////////////////////////////////////////////////////////////////////////

const fwMvBooleanId CTaskDismountSeat::ms_DismountClipFinishedId("DismountAnimFinished",0x8E62E20C);
const fwMvBooleanId CTaskDismountSeat::ms_RunDismountClipFinishedId("RunDismountAnimFinished",0x78210D90);
const fwMvClipId CTaskDismountSeat::ms_DismountClipId("DismountClipId",0x6EBCD88A);
const fwMvFloatId CTaskDismountSeat::ms_DismountPhaseId("DismountPhaseId",0x9A3D95);

CTaskDismountSeat::CTaskDismountSeat(CPed* pMount, s32 iExitPointIndex, bool bAnimateToExitPoint)
: m_pMount(pMount) 
, m_iExitPointIndex(iExitPointIndex)
, m_bAnimateToExitPoint(bAnimateToExitPoint)
, m_bDismountReady(false)
, m_fRStirrupPhase(0.5f)
, m_fLStirrupPhase(0.5f)
{
	SetInternalTaskType(CTaskTypes::TASK_DISMOUNT_SEAT);
}

CTaskDismountSeat::~CTaskDismountSeat()
{

}

void CTaskDismountSeat::CleanUp()
{
	CPed* pPed = GetPed();
	if (pPed->GetMyMount()!=NULL)
	{
		//Something interrupted this? Pop it. B* 424921
		pPed->SetPedOffMount(true);
	}
}

CTask::FSM_Return CTaskDismountSeat::ProcessPreFSM()
{
	if (!taskVerifyf(m_pMount, "NULL vehicle pointer in CTaskDismountSeat::ProcessPreFSM"))
	{
		return FSM_Quit;
	}

	if (!taskVerifyf(GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_DISMOUNT_ANIMAL, "Invalid parent task"))
	{
		return FSM_Quit;
	}

	return FSM_Continue;
}	

CTask::FSM_Return CTaskDismountSeat::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
		FSM_OnUpdate
		return Start_OnUpdate();	

	FSM_State(State_Dismount)
		FSM_OnEnter
		Dismount_OnEnter();
	FSM_OnUpdate
		return Dismount_OnUpdate();

	FSM_State(State_Finish)
		FSM_OnUpdate
		return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskDismountSeat::UpdateClonedFSM( s32 iState, CTask::FSM_Event iEvent )
{
	return UpdateFSM(iState, iEvent); 
}

bool CTaskDismountSeat::ProcessPhysics(float fTimeStep, int nTimeSlice)
{
	if (GetState() > State_Start && m_pParentNetworkHelper)
	{
		CPed* pPed = GetPed();
		if (m_pMount && GetState()<State_Finish && pPed->GetIsAttached())
		{


			// Here we calculate the initial situation, which in the case of exiting is the seat situation
			// Note we don't use the helper to do this calculation since we assume the ped starts exactly on the seat node
			Vector3 vNewPedPosition(Vector3::ZeroType);
			Quaternion qNewPedOrientation(0.0f,0.0f,0.0f,1.0f);
			float fClipPhase = m_pParentNetworkHelper->GetFloat( ms_DismountPhaseId);
			fClipPhase = rage::Clamp(fClipPhase, 0.0f, 1.0f);
			const crClip* pClip = m_pParentNetworkHelper->GetClip( ms_DismountClipId);		

			UpdateDismountAttachment(pPed, m_pMount, pClip, fClipPhase, vNewPedPosition, qNewPedOrientation, m_iExitPointIndex, m_PlayAttachedClipHelper, m_bAnimateToExitPoint);

			return true;
		}
	}	
	return CTask::ProcessPhysics(fTimeStep, nTimeSlice);
}

void CTaskDismountSeat::UpdateDismountAttachment(CPed* pPed, CEntity* pMount, const crClip* pClip, float fClipPhase, Vector3& vNewPedPosition, Quaternion& qNewPedOrientation, s32 iEntryPointIndex, CPlayAttachedClipHelper& playAttachedClipHelper, bool applyFixup )
{
	CModelSeatInfo::CalculateSeatSituation(pMount, vNewPedPosition, qNewPedOrientation, iEntryPointIndex);

	//TODO Adding this rotation computation here as right now the orientation I get from above is not good at all	
	fwEntity *attachParent = pPed->GetAttachParent();
	if (taskVerifyf(attachParent!=NULL && pPed->GetAttachBone() > -1, "Ped not attached to properly"))	
	{	
		Matrix34 newMtx;
		attachParent->GetGlobalMtx(pPed->GetAttachBone(), newMtx);
		qNewPedOrientation.FromMatrix34(newMtx);

		// Note the clip pointer will not exist the frame after starting the clip, but we still need to update the ped's mover
		if (pClip)	
		{
			// Update the target situation
			UpdateTarget(pMount, pPed, pClip, iEntryPointIndex, playAttachedClipHelper);

			// Update the situation from the current phase in the clip
			playAttachedClipHelper.ComputeSituationFromClip(pClip, 0.0f, fClipPhase, vNewPedPosition, qNewPedOrientation);
		
			if (applyFixup)
			{
				// Compute the offsets we'll need to fix up
				Vector3 vOffsetPosition(Vector3::ZeroType);
				Quaternion qOffsetOrientation(0.0f,0.0f,0.0f,1.0f);
				playAttachedClipHelper.ComputeOffsets(pClip, fClipPhase, 1.0f, vNewPedPosition, qNewPedOrientation, vOffsetPosition, qOffsetOrientation);

				// Apply the fixup
				playAttachedClipHelper.ApplyVehicleExitOffsets(vNewPedPosition, qNewPedOrientation, pClip, vOffsetPosition, qOffsetOrientation, fClipPhase);
			}
		}										
	}

	qNewPedOrientation.Normalize();
	// Pass in the new position and orientation in global space, this will get transformed relative to the vehicle for attachment
	CTaskVehicleFSM::UpdatePedVehicleAttachment(pPed, VECTOR3_TO_VEC3V(vNewPedPosition), QUATERNION_TO_QUATV(qNewPedOrientation));	
}

void CTaskDismountSeat::UpdateTarget(CEntity* pMount, CPed* pPed, const crClip* pClip, s32 iEntryPointIndex, CPlayAttachedClipHelper& playAttachedClipHelper )
{
	// Assume we want to be horizontal, i.e. the rotational offset is the current orientation of the seat matrix
	// Maybe this should use the results of the ground probe to orientate the ped along the slope of the ground
	Vector3 vTargetPosition(Vector3::ZeroType);
	Quaternion qTargetOrientation(0.0f,0.0f,0.0f,1.0f);
	
	CModelSeatInfo::CalculateExitSituation(pMount, vTargetPosition, qTargetOrientation, iEntryPointIndex, pClip);
	Vector3 vTargetOrientationEulers(Vector3::ZeroType);
	qTargetOrientation.ToEulers(vTargetOrientationEulers);
	vTargetOrientationEulers.x = 0.0f;
	vTargetOrientationEulers.y = 0.0f;
	qTargetOrientation.FromEulers(vTargetOrientationEulers, "xyz");

	Vector3 vGroundPos(Vector3::ZeroType);
	if (CTaskVehicleFSM::FindGroundPos(pMount, pPed, vTargetPosition, 2.5f, vGroundPos))
	{
		// Adjust the reference position
		vTargetPosition.z = vGroundPos.z;
	}

	playAttachedClipHelper.SetTarget(vTargetPosition, qTargetOrientation);
	//grcDebugDraw::Sphere(vTargetPosition, 0.1f, Color_red);
}

CTaskInfo* CTaskDismountSeat::CreateQueriableState() const
{
	return rage_new CTaskInfo();
}

CTask::FSM_Return CTaskDismountSeat::Start_OnUpdate()
{

	m_pParentNetworkHelper = static_cast<CTaskDismountAnimal*>(GetParent())->GetMoveNetworkHelper();

	if (!m_pParentNetworkHelper)
	{
		SetState(State_Finish);
		return FSM_Continue;
	}

	if (m_bDismountReady) {
		SetState(State_Dismount);
	}

	if (!GetPed()->GetIsOnMount())
	{
		return FSM_Quit;
	}

	return FSM_Continue;
}

void CTaskDismountSeat::Dismount_OnEnter()
{
	static const fwMvRequestId dismountId("Dismount",0x148FE5B8);
	static const fwMvRequestId jackedId("Jacked",0xC6CC5342);
	
	taskAssert(GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_DISMOUNT_ANIMAL);
	CTaskDismountAnimal* dismountTask = static_cast<CTaskDismountAnimal*>(GetParent());
	if (dismountTask->GetRunningFlags().BitSet().IsSet(CVehicleEnterExitFlags::BeJacked))
		m_pParentNetworkHelper->SendRequest( jackedId);
	else
		m_pParentNetworkHelper->SendRequest( dismountId);

}

CTask::FSM_Return CTaskDismountSeat::Dismount_OnUpdate()
{
	CPed* pPed = GetPed();
	bool bRunDismountFinished = m_pParentNetworkHelper->GetBoolean( ms_RunDismountClipFinishedId);
	bool bDismountFinished = m_pParentNetworkHelper->GetBoolean( ms_DismountClipFinishedId);
	if (bRunDismountFinished)
	{ //detach early
		pPed->DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS);
	}

	bool requestInterrupt = false;
	float fClipPhase = m_pParentNetworkHelper->GetFloat( ms_DismountPhaseId);
	if (pPed->IsLocalPlayer())
	{
		CControl* pControl = pPed->GetControlFromPlayer();
		// Abort if Mount is pressed.
		if( pControl && pControl->GetPedEnter().IsDown() )
		{
			requestInterrupt = true;
		}
		if (requestInterrupt)
		{
			float interruptPhase = GetDismountInterruptPhase();				
			if (fClipPhase > interruptPhase && requestInterrupt) 
			{
				bDismountFinished = true;
			}
		}
	}

	const crClip* pClip = m_pParentNetworkHelper->GetClip(ms_DismountClipId);
	if (pClip)
	{
		if (m_fRStirrupPhase==0.5f)
			CClipEventTags::FindEventPhase<crPropertyAttributeBool, bool>(pClip, CClipEventTags::Foot, CClipEventTags::Right, true, m_fRStirrupPhase);
		if (m_fLStirrupPhase==0.5f)
			CClipEventTags::FindEventPhase<crPropertyAttributeBool, bool>(pClip, CClipEventTags::Foot, CClipEventTags::Right, false, m_fLStirrupPhase);
	} 

	if ( (fClipPhase > m_fRStirrupPhase || fClipPhase > m_fLStirrupPhase) ) 
	{		
		if (fClipPhase > m_fRStirrupPhase)
			m_pMount->GetHorseComponent()->SetStirrupEnable(true, false);
		if (fClipPhase > m_fLStirrupPhase)
			m_pMount->GetHorseComponent()->SetStirrupEnable(false, false);
	}
	
	if (fClipPhase > 0.5f ) 
	{		
		m_pMount->GetHorseComponent()->SetStirrupEnable(true, false);
		m_pMount->GetHorseComponent()->SetStirrupEnable(false, false);
	}


	if (bDismountFinished)
	{
		//Set the ped off the mount.
		pPed->SetPedOffMount(false);
		
		//Check if the ped has a network object.
		netObject* pObject = pPed->GetNetworkObject();
		if(pObject)
		{
			//Ignore mount updates for a while, otherwise the network sync code
			//may decide to place the ped back on the mount for a split second.
			static_cast<CNetObjPed *>(pObject)->IgnoreTargetMountUpdatesForAWhile();
		}		
		SetState(State_Finish);
	}	

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

float CTaskDismountSeat::GetDismountInterruptPhase() const 
{
	static const crProperty::Key moveEventKey = crTag::CalcKey("MoveEvent", 0x7EA9DFC4);		
	const crClip* pClip = m_pParentNetworkHelper->GetClip(ms_DismountClipId);	
	if(pClip!=NULL)
	{
		//Find interrupt tag.		
		crTagIterator moveIt(*pClip->GetTags(),0.0f, 1.0f, moveEventKey);
		while(*moveIt)
		{
			const crTag* pTag = *moveIt;			
			const crPropertyAttribute* attrib = pTag->GetProperty().GetAttribute(moveEventKey);
			if(attrib && attrib->GetType() == crPropertyAttribute::kTypeHashString)
			{
				const fwMvBooleanId ms_InterruptId("ExitInterupt",0x500FC7A2);						
				if (ms_InterruptId.GetHash() == static_cast<const crPropertyAttributeHashString*>(attrib)->GetHashString().GetHash())
				{
						return pTag->GetStart();
				}						
			}
			++moveIt;			
		}
	}
	return 1.0f;
}


#if !__FINAL
const char * CTaskDismountSeat::GetStaticStateName( s32 iState )
{
	taskAssert(iState >= State_Start && iState <= State_Finish);

	switch (iState)
	{
	case State_Start:							return "State_Start";
	case State_Dismount:						return "State_Dismount";
	case State_Finish:							return "State_Finish";
	default: taskAssert(0); 	
	}

	return "State_Invalid";
}

void CTaskDismountSeat::Debug() const
{
#if __BANK
	Matrix34 mPedMatrix;
	GetPed()->GetMatrixCopy(mPedMatrix);
	grcDebugDraw::Axis(mPedMatrix, 0.1f, true);	
#endif
}
#endif // !__FINAL

#endif // ENABLE_HORSE