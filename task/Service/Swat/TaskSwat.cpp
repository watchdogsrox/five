// FILE :    TaskSwat.h
// PURPOSE : Controlling task for swat
// CREATED : 30-06-2009

// File header
#include "Task/Service/Swat/TaskSwat.h"

// Framework headers
#include "ai/task/taskchannel.h"

// Game headers
#include "Debug/DebugScene.h"
#include "Event/EventDamage.h"
#include "Event/Events.h"
#include "Event/System/EventData.h"
#include "fwscene/world/WorldLimits.h"
#include "Game/Dispatch/DispatchData.h"
#include "Game/Dispatch/DispatchHelpers.h"
#include "Game/Dispatch/Orders.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "physics/physics.h"
#include "Scene/world/GameWorld.h"
#include "Task/Combat/TaskCombat.h"
#include "Task/Combat/TaskNewCombat.h"
#include "task/Combat/TaskThreatResponse.h"
#include "Task/Default/TaskWander.h"
#include "Task/Movement/Climbing/TaskRappel.h"
#include "Task/Movement/TaskNavMesh.h"
#include "Task/Movement/TaskMoveFollowLeader.h"
#include "Task/Movement/TaskSeekEntity.h"
#include "Task/Vehicle/TaskCar.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Task/Vehicle/TaskExitVehicle.h"
#include "Task/Vehicle/TaskVehicleBase.h"
#include "Task/Vehicle/TaskVehicleWeapon.h"
#include "task/Weapons/TaskSwapWeapon.h"
#include "Task/Weapons/Gun/TaskGun.h"
#include "VehicleAI/driverpersonality.h"
#include "vehicleAi/task/TaskVehicleApproach.h"
#include "VehicleAI/task/TaskVehicleCruise.h"
#include "VehicleAI/task/TaskVehicleGoToAutomobile.h"
#include "vehicleAi/task/TaskVehicleGotoHelicopter.h"
#include "Vehicles/Heli.h"

AI_OPTIMISATIONS()

namespace
{
	const float s_fHeliCruiseSpeed = 35.0f;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskSwat
/////////////////////////////////////////////////////////////////////////////////////////////////////////

CPed* CTaskSwat::GetGroupLeader(const CPed& ped)
{
	if (ped.GetPedsGroup() && ped.GetPedsGroup()->GetGroupMembership())
	{
		return ped.GetPedsGroup()->GetGroupMembership()->GetLeader();
	}
	return NULL;
}

void CTaskSwat::SortPedsGroup(const CPed& ped)
{
	if(ped.GetPedsGroup() && ped.GetPedsGroup()->GetGroupMembership())
	{
		ped.GetPedsGroup()->GetGroupMembership()->SortByDistanceFromLeader();
	}
}

CTaskSwat::CTaskSwat()
: m_ExitToAimClipSetId(CLIP_SET_ID_INVALID)
{
	SetInternalTaskType(CTaskTypes::TASK_SWAT);
}

CTaskSwat::~CTaskSwat()
{

}

CTask::FSM_Return CTaskSwat::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_EnterVehicle)
			FSM_OnEnter
				EnterVehicle_OnEnter();
			FSM_OnUpdate
				return EnterVehicle_OnUpdate();

		FSM_State(State_ExitVehicle)
			FSM_OnEnter
				ExitVehicle_OnEnter();
			FSM_OnUpdate
				return ExitVehicle_OnUpdate();

		FSM_State(State_RespondToOrder)
			FSM_OnEnter
				RespondToOrder_OnEnter();
			FSM_OnUpdate
				return RespondToOrder_OnUpdate();
			FSM_OnExit
				RespondToOrder_OnExit();

		FSM_State(State_WanderOnFoot)
			FSM_OnEnter
				WanderOnFoot_OnEnter();
			FSM_OnUpdate
				return WanderOnFoot_OnUpdate();
			FSM_OnExit
				WanderOnFoot_OnExit();

		FSM_State(State_WanderInVehicle)
			FSM_OnEnter
				WanderInVehicle_OnEnter();
			FSM_OnUpdate
				return WanderInVehicle_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskSwat::ProcessPostFSM()
{
	const CPed& ped = *GetPed();
	if (ped.GetIsInVehicle())
	{
		const CVehicle& veh = *ped.GetMyVehicle();
		s32 iPedsSeatIndex = veh.GetSeatManager()->GetPedsSeatIndex(&ped);
		if (veh.IsSeatIndexValid(iPedsSeatIndex))
		{
			m_VehicleClipRequestHelper.RequestClipsFromVehicle(&veh, CVehicleClipRequestHelper::AF_StreamEntryClips | CVehicleClipRequestHelper::AF_StreamSeatClips, iPedsSeatIndex);
		}
		if (m_ExitToAimClipSetId.IsNotNull())
		{
			m_ExitToAimClipSetRequestHelper.Request(m_ExitToAimClipSetId);
		}
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskSwat::Start_OnUpdate()
{
	CPed* pPed = GetPed();

	// Find out which clip set to request based on our equipped weapon or best weapon and then request it
	m_ExitToAimClipSetId = CTaskVehicleFSM::GetExitToAimClipSetIdForPed(*pPed);
	m_VehicleClipRequestHelper.SetPed(pPed);

	// If our order is valid, respond to it
	const COrder* pOrder = pPed->GetPedIntelligence()->GetOrder();
	if (pOrder && pOrder->GetIsValid() && pPed->GetPedsGroup())
	{
		SetState(State_RespondToOrder);
		return FSM_Continue;
	}
	else // Otherwise 'Wander'
	{
		if (pPed->GetIsInVehicle())
		{
			SetState(State_WanderInVehicle);
		}
		else
		{
			CVehicle* pMyVehicle = pPed->GetMyVehicle();
			if(pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanUseVehicles) &&
			   pMyVehicle && !pMyVehicle->InheritsFromHeli() && pMyVehicle->IsInDriveableState())
			{
				SetState(State_EnterVehicle);
				return FSM_Continue;
			}

			SetState(State_WanderOnFoot);
		}
		return FSM_Continue;
	}
}

CTask::FSM_Return CTaskSwat::RespondToOrder_OnEnter()
{
	//Grab the ped.
	CPed* pPed = GetPed();
	
	//Enable action mode.
	pPed->SetUsingActionMode(true, CPed::AME_RespondingToOrder);
	
	if(pPed->GetIsInVehicle() && pPed->GetMyVehicle() && pPed->GetMyVehicle()->InheritsFromHeli())
	{
		SetNewTask(rage_new CTaskHeliOrderResponse());
	}
	else
	{
		SetNewTask(rage_new CTaskSwatOrderResponse());
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskSwat::RespondToOrder_OnUpdate()
{
	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Start);
		return FSM_Continue;
	}
	return FSM_Continue;
}

void CTaskSwat::RespondToOrder_OnExit()
{
	//Grab the ped.
	CPed* pPed = GetPed();

	//Disable action mode.
	pPed->SetUsingActionMode(false, CPed::AME_RespondingToOrder);
}

void CTaskSwat::EnterVehicle_OnEnter()
{
	CPed* pPed = GetPed();
	CVehicle* pMyVehicle = pPed->GetMyVehicle();
	if(pMyVehicle && !pPed->GetIsInVehicle())
	{
		// Get our vehicle and the driver of the vehicle
		CPed* pDriver = pMyVehicle->GetDriver();
		if(!pDriver)
		{
			pDriver = pMyVehicle->GetSeatManager()->GetLastPedInSeat(pMyVehicle->GetDriverSeat());
		}
		else if(!pDriver->IsLawEnforcementPed())
		{
			return;
		}

		s32 seatIndex = pMyVehicle->GetSeatManager()->GetLastPedsSeatIndex(GetPed());

		// If we don't have a driver, the driver is not a law ped or the driver is otherwise incapacitated
		if(!pDriver || !pDriver->IsLawEnforcementPed() || (pDriver != pPed && pDriver->IsFatallyInjured()))
		{
			// Mark the seats as available or not (driver is avail in this case but the other seats might be invalid for some reason)
			bool bSeatAvailable[Seat_numSeats] = {true, pMyVehicle->IsSeatIndexValid(Seat_frontPassenger), pMyVehicle->IsSeatIndexValid(Seat_backLeft), pMyVehicle->IsSeatIndexValid(Seat_backRight)};
			seatIndex = Seat_driver;
			
			// go through nearby peds to see if they are entering into our vehicle
			const CEntityScannerIterator entityList = pPed->GetPedIntelligence()->GetNearbyPeds();
			for( const CEntity* pEnt = entityList.GetFirst(); pEnt; pEnt = entityList.GetNext() )
			{
				// check if this ped is running the enter vehicle task
				const CPed* pNearbyPed = static_cast<const CPed*>(pEnt);
				if( pNearbyPed && pNearbyPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning( CTaskTypes::TASK_ENTER_VEHICLE ) )
				{
					// check if this ped is entering our vehicle
					CTaskEnterVehicle* pEnterTask = static_cast<CTaskEnterVehicle*>(pNearbyPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE));
					if (pEnterTask && pEnterTask->GetVehicle() == pMyVehicle)
					{
						// check if they have a valid seat they are entering (valid according to our vehicle)
						s32 iTargetSeat = pEnterTask->GetTargetSeat();
						if(iTargetSeat >= Seat_driver && iTargetSeat < Seat_numSeats)
						{
							// mark this seat as no longer valid and recheck if any seats are open
							bSeatAvailable[iTargetSeat] = false;

							// try to find an available seat
							bool bIsAnySeatOpen = false;
							seatIndex = -1;

							for(int i = Seat_driver; i < Seat_numSeats && !bIsAnySeatOpen; i++)
							{
								// if the seat is available we should use it and set that we have a seat available
								if(bSeatAvailable[i])
								{
									bIsAnySeatOpen = true;
									seatIndex = i;
								}
							}

							// if we don't have any seats open we should break out of the loop
							if(!bIsAnySeatOpen)
							{
								break;
							}
						}
					}
				}
			}
		}

		if(seatIndex == -1)
		{
			return;
		}

		SetNewTask(rage_new CTaskEnterVehicle(pMyVehicle, SR_Specific, seatIndex, VehicleEnterExitFlags(), 0.0f, MOVEBLENDRATIO_WALK));
	}
}

CTask::FSM_Return CTaskSwat::EnterVehicle_OnUpdate()
{
	CPed* pPed = GetPed();

	if (!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		if(pPed->GetIsInVehicle())
		{
			// Make sure no other group ped is trying to enter our vehicle (only the driver will check inside this function)
			if(ShouldWaitForGroupMembers())
			{
				return FSM_Continue;
			}

			SetState(State_WanderInVehicle);
			return FSM_Continue;
		}
		else
		{
			SetState(State_WanderOnFoot);
			return FSM_Continue;
		}
	}

	if(!pPed->GetIsEnteringVehicle())
	{
		const COrder* pOrder = pPed->GetPedIntelligence()->GetOrder();
		if (pOrder && pOrder->GetIsValid() && pPed->GetPedsGroup())
		{
			SetState(State_RespondToOrder);
			return FSM_Continue;
		}
	}

	return FSM_Continue;
}

void CTaskSwat::ExitVehicle_OnEnter()
{
	CPed* pPed = GetPed();
	taskAssert(pPed);

	//Create the vehicle flags.
	VehicleEnterExitFlags vehicleFlags;
	vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DontCloseDoor);
	vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::WarpIfDoorBlocked);
	vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DontWaitForCarToStop);

	//Check if we should jump out.
	if(ShouldJumpOutOfVehicle())
	{
		vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::JumpOut);
	}

	float fExitDelayTime = pPed->GetMyVehicle() && pPed->GetMyVehicle()->IsDriver(pPed) ? 0.0f : (float)( pPed->GetRandomSeed()/(float)RAND_MAX_16 );
	CTaskExitVehicle* pTask = rage_new CTaskExitVehicle(pPed->GetMyVehicle(),vehicleFlags, fExitDelayTime);

	SetNewTask(pTask);
}

bool CTaskSwat::ShouldJumpOutOfVehicle() const
{
	//Grab the vehicle.
	const CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
	if(!pVehicle)
	{
		return false;
	}

	//Ensure our speed has exceeded the threshold.
	TUNE_GROUP_FLOAT(TASK_SWAT, fMinSpeedToJumpOutOfVehicle, 10.0f, 0.0f, 1000.0f, 0.1f);
	float fMinSpeedToJumpOutOfVehicleSq = square(fMinSpeedToJumpOutOfVehicle);
	float fSpeedSq = pVehicle->GetVelocity().Mag2();
	if(fSpeedSq < fMinSpeedToJumpOutOfVehicleSq)
	{
		return false;
	}

	return true;
}

CTask::FSM_Return CTaskSwat::ExitVehicle_OnUpdate()
{
	CPed* pPed = GetPed();
	taskAssert(pPed);

	// If the subtask has terminated - quit
	if( GetIsFlagSet(aiTaskFlags::SubTaskFinished) || !GetSubTask() )
	{
		if( pPed->GetIsInVehicle() )
		{
			const float EXIT_FAILED_TIME_IN_STATE = 2.0f;

			if (GetTimeInState() > EXIT_FAILED_TIME_IN_STATE)
			{
				SetFlag(aiTaskFlags::RestartCurrentState);
				return FSM_Continue;
			}
		}
		else
		{
			SetState(State_Start);
			return FSM_Continue;
		}
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskSwat::WanderOnFoot_OnEnter()
{
	if(NetworkInterface::IsGameInProgress() && GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_CreatedByDispatch))
	{
		GetPed()->SetRemoveAsSoonAsPossible(true);
	}

	//Create the task.
	CTaskWander* pTask = rage_new CTaskWander(MOVEBLENDRATIO_WALK, GetPed()->GetCurrentHeading());
	
	//Holster our weapon.
	pTask->SetHolsterWeapon(true);
	
	//Start the task.
	SetNewTask(pTask);
	
	return FSM_Continue;
}

CTask::FSM_Return CTaskSwat::WanderOnFoot_OnUpdate()
{
	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished) || !GetSubTask())
	{
		SetFlag(aiTaskFlags::RestartCurrentState);
		return FSM_Continue;
	}

	const COrder* pOrder = GetPed()->GetPedIntelligence()->GetOrder();
	if (pOrder && pOrder->GetIsValid() && GetPed()->GetPedsGroup())
	{
		SetState(State_Start);
		return FSM_Continue;
	}

	//Activate timeslicing.
	ActivateTimeslicing();

	return FSM_Continue;
}

void CTaskSwat::WanderOnFoot_OnExit()
{
	if(NetworkInterface::IsGameInProgress() && GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_CreatedByDispatch))
	{
		GetPed()->SetRemoveAsSoonAsPossible(false);
	}
}

CTask::FSM_Return CTaskSwat::WanderInVehicle_OnEnter()
{
	const CPed* pPed = GetPed();
	CVehicle* pVehicle = pPed->GetMyVehicle();
	if (pVehicle && pVehicle->GetDriver() == pPed)
	{
		pVehicle->TurnSirenOn(false);
		if(pVehicle->InheritsFromHeli())
		{
			Vec3V vPedPosition = pVehicle->GetTransform().GetPosition();
			
			float fCruiseSpeed = s_fHeliCruiseSpeed;
			fCruiseSpeed *= pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatHeliSpeedModifier);

			sVehicleMissionParams params;
			params.m_fCruiseSpeed = fCruiseSpeed;

			// Just fly to a far away position because we are trying to get away. We should stream out before reaching it
			const Vector3 vTargetPosition(IsTrue(vPedPosition.GetX() > ScalarV(0.0f)) ? WORLDLIMITS_REP_XMIN : WORLDLIMITS_REP_XMAX, 
											IsTrue(vPedPosition.GetY() > ScalarV(0.0f)) ? WORLDLIMITS_REP_YMIN : WORLDLIMITS_REP_YMAX, 
											40.0f);

			params.SetTargetPosition(vTargetPosition);

			CTaskVehicleGoToHelicopter *pHeliTask = rage_new CTaskVehicleGoToHelicopter(params, 0, -1.0f, 40);
			SetNewTask(rage_new CTaskControlVehicle(pVehicle, pHeliTask));
		}
		else
		{
			float fCruiseSpeed = CDriverPersonality::FindMaxCruiseSpeed(GetPed(), pVehicle, pVehicle->GetVehicleType() == VEHICLE_TYPE_BICYCLE);
			fCruiseSpeed *= pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatAutomobileSpeedModifier);
			
			SetNewTask(rage_new CTaskCarDriveWander(pVehicle, DMode_StopForCars, fCruiseSpeed, false));
		}
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskSwat::WanderInVehicle_OnUpdate()
{
	CPed* pPed = GetPed();
	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished) || !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle))
	{
		SetState(State_Start);
		return FSM_Continue;
	}

	const COrder* pOrder = pPed->GetPedIntelligence()->GetOrder();
	if (pOrder && pOrder->GetIsValid() && pPed->GetPedsGroup())
	{
		SetState(State_Start);
		return FSM_Continue;
	}

	CVehicle* pVehicle = pPed->GetMyVehicle();
	if (!pVehicle)
	{
		SetState(State_Start);
		return FSM_Continue;
	}

	const CPed* pDriver = pVehicle->GetDriver();
	if(!pDriver || !pDriver->IsLawEnforcementPed() || (pDriver != pPed && (pDriver->IsFatallyInjured() || pDriver->GetPedIntelligence()->GetTaskWrithe())))
	{
		bool bNewDriverPed = false;

		CComponentReservation* pComponentReservation = pVehicle->GetComponentReservationMgr() ? pVehicle->GetComponentReservationMgr()->GetSeatReservationFromSeat(pVehicle->GetDriverSeat()) : NULL;
		if(pComponentReservation && pComponentReservation->GetPedUsingComponent())
		{
			CPed* pPedEnteringDriverSeat = pComponentReservation->GetPedUsingComponent();
			bNewDriverPed = (pDriver != pPedEnteringDriverSeat) && !pPedEnteringDriverSeat->IsFatallyInjured();
		}

		if (!bNewDriverPed)
		{
			SetState(State_ExitVehicle);
			return FSM_Continue;
		}
	}

	//Activate timeslicing.
	ActivateTimeslicing();

	pPed->SetPedResetFlag(CPED_RESET_FLAG_PatrollingInVehicle, true);

	return FSM_Continue;
}

void CTaskSwat::ActivateTimeslicing()
{
	TUNE_GROUP_BOOL(SWAT, ACTIVATE_TIMESLICING, true);
	if(ACTIVATE_TIMESLICING)
	{
		GetPed()->GetPedAiLod().ClearBlockedLodFlag(CPedAILod::AL_LodTimesliceIntelligenceUpdate);
	}
}

// comment about only doing for driver
bool CTaskSwat::ShouldWaitForGroupMembers() const
{
	const CPed* pPed = GetPed();
	const CPedGroup* pGroup = pPed->GetPedsGroup();
	if (pGroup)
	{
		const CPedGroupMembership* pGroupMembership = pGroup->GetGroupMembership();
		if(pGroupMembership && pPed->GetIsDrivingVehicle())
		{
			for (int i = 0; i < CPedGroupMembership::MAX_NUM_MEMBERS; i++)
			{
				const CPed* pPedMember = pGroupMembership->GetMember(i);
				if (pPedMember && pPedMember != pPed)
				{
					// are they entering any car?
					if( pPedMember->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning( CTaskTypes::TASK_ENTER_VEHICLE ) &&
					  ( ((CVehicle*)pPedMember->GetPedIntelligence()->GetQueriableInterface()->GetTargetForTaskType(CTaskTypes::TASK_ENTER_VEHICLE)) == pPed->GetMyVehicle() ) )
					{
						return true;
					}
				}
			}
		}
	}
	return false;
}

#if !__FINAL
const char * CTaskSwat::GetStaticStateName( s32 iState  )
{
	taskAssert(iState >= State_Start && iState <= State_Finish);

	switch (iState)
	{
		case State_Start:				return "State_Start";
		case State_EnterVehicle:		return "State_EnterVehicle";
		case State_ExitVehicle:			return "State_ExitVehicle";
		case State_RespondToOrder:		return "State_RespondToOrder";
		case State_WanderOnFoot:		return "State_WanderOnFoot";
		case State_WanderInVehicle:		return "State_WanderInVehicle";
		case State_Finish:				return "State_Finish";
		default: taskAssert(0); 	
	}

	return "State_Invalid";
}
#endif // !__FINAL

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskSwatOrderResponse
/////////////////////////////////////////////////////////////////////////////////////////////////////////

CTaskSwatOrderResponse::Tunables CTaskSwatOrderResponse::sm_Tunables;
IMPLEMENT_SERVICE_TASK_TUNABLES(CTaskSwatOrderResponse, 0x16688583);

#define INITIAL_TIME_BETWEEN_CROUCHES_MIN	7.0f
#define INITIAL_TIME_BETWEEN_CROUCHES_MAX	9.0f
#define TIME_BETWEEN_CROUCHES_MIN	13.0f
#define TIME_BETWEEN_CROUCHES_MAX	18.0f
#define CROUCH_DURATION_MIN			5.0f
#define CROUCH_DURATION_MAX			7.5f
#define CROUCH_DURATION_FOLLOWER_MIN	.65f
#define CROUCH_DURATION_FOLLOWER_MAX	.9f

static bank_float s_fMaxDistToPlayer = 20.0f;
static bank_float s_fAdjustMinDist = 1.3f;

CTaskSwatOrderResponse::CTaskSwatOrderResponse()
: m_vPositionToSearchOnFoot(V_ZERO)
, m_pTarget(NULL)
, m_fTimeToWander(0.0f)
{
	SetInternalTaskType(CTaskTypes::TASK_SWAT_ORDER_RESPONSE);
}

CTaskSwatOrderResponse::~CTaskSwatOrderResponse()
{

}

CTask::FSM_Return CTaskSwatOrderResponse::ProcessPreFSM()
{
	// A ped must have a valid order to be running the swat task
	const COrder* pOrder = GetPed()->GetPedIntelligence()->GetOrder();
	if (pOrder == NULL || !pOrder->GetIsValid())
	{
		return FSM_Quit;
	}

	// Cache the ped intelligence
	CPedIntelligence* pPedIntell = GetPed()->GetPedIntelligence();

	// Make sure the targetting system is activated.
	CPedTargetting* pPedTargetting = pPedIntell->GetTargetting( true );
	Assert(pPedTargetting);
	if (pPedTargetting)
	{
		pPedTargetting->SetInUse();
	}

	// Make sure the targetting system is activated, scan for a new target
	CPed* pBestTarget = pPedTargetting ? pPedTargetting->GetBestTarget() : NULL;
	if( pBestTarget )
	{
		if(pBestTarget != m_pTarget)
		{
			m_pTarget = pBestTarget;
		}
	}
	else if (pOrder->GetIncident())
	{
		CEntity* pIncidentEntity = pOrder->GetIncident()->GetEntity();
		if (pIncidentEntity && pIncidentEntity->GetIsTypePed())
		{
			m_pTarget = static_cast<CPed*>(pIncidentEntity);
		}
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskSwatOrderResponse::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	const COrder* pOrder = GetPed()->GetPedIntelligence()->GetOrder();

	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_Resume)
			FSM_OnUpdate
				return Resume_OnUpdate();

		FSM_State(State_GoToStagingArea)
			FSM_OnEnter
				GoToStagingArea_OnEnter();
			FSM_OnUpdate
				return GoToStagingArea_OnUpdate();

		FSM_State(State_AdvanceToTarget)
			FSM_OnEnter
				AdvanceToTarget_OnEnter();
			FSM_OnUpdate
				return AdvanceToTarget_OnUpdate(pOrder);
			FSM_OnExit
				return AdvanceToTarget_OnExit(pOrder);

		FSM_State(State_CombatTarget)
			FSM_OnEnter
				CombatTarget_OnEnter();
			FSM_OnUpdate
				return CombatTarget_OnUpdate();

		FSM_State(State_FindPositionToSearchOnFoot)
			FSM_OnEnter
				FindPositionToSearchOnFoot_OnEnter();
			FSM_OnUpdate
				return FindPositionToSearchOnFoot_OnUpdate();
				
		FSM_State(State_SearchOnFoot)
			FSM_OnEnter
				SearchOnFoot_OnEnter();
			FSM_OnUpdate
				return SearchOnFoot_OnUpdate();

		FSM_State(State_Wander)
			FSM_OnEnter
				Wander_OnEnter();
			FSM_OnUpdate
				return Wander_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskSwatOrderResponse::Start_OnUpdate()
{
	SetState(State_GoToStagingArea);
	return FSM_Continue;
}

CTask::FSM_Return CTaskSwatOrderResponse::Resume_OnUpdate()
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

void CTaskSwatOrderResponse::GoToStagingArea_OnEnter()
{
	// This will just go to the incident location right now, which basically makes them plough straight towards the player
	// In the future it will analyse the map and find a good place to goto first so the player can't take them out too easily
	// This might be a problem if the player keeps moving about, maybe they should only go to a staging area if the player stays in the 
	// same place or is on foot. Once there the swat team will exit the van and go into a group task and try to take out the player methodically
	SetNewTask(rage_new CTaskSwatGoToStagingArea());
}

CTask::FSM_Return CTaskSwatOrderResponse::GoToStagingArea_OnUpdate()
{
	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_AdvanceToTarget);
	}
	return FSM_Continue;
}


void CTaskSwatOrderResponse::AdvanceToTarget_OnEnter()
{
	SetNewTask(rage_new CTaskSwatFollowInLine());
}

CTask::FSM_Return CTaskSwatOrderResponse::AdvanceToTarget_OnUpdate(const COrder* UNUSED_PARAM(pOrder))
{
	//Check if we can see the target.
	if(CanSeeTarget())
	{
		SetState(State_CombatTarget);
	}
	//Check if the sub-task has finished.
	else if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Check if the target's whereabouts are known.
		if(AreTargetsWhereaboutsKnown())
		{
			SetState(State_CombatTarget);
		}
		else
		{
			SetState(State_FindPositionToSearchOnFoot);
		}
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskSwatOrderResponse::AdvanceToTarget_OnExit(const COrder* pOrder)
{
	if(GetState() == State_CombatTarget)
	{
		CPed* pPed = GetPed();
		CPed* pLeader = CTaskSwat::GetGroupLeader(*pPed);

		if (pLeader == pPed && pOrder &&  pOrder->GetIncident())
		{
			CEntity* pIncidentEntity = pOrder->GetIncident()->GetEntity();
			if (pIncidentEntity)
			{
				CEventAcquaintancePedWanted wantedEvent(static_cast<CPed*>(pIncidentEntity));
				wantedEvent.CommunicateEvent(pPed);
			}
		}
	}

	return FSM_Continue;
}

void CTaskSwatOrderResponse::CombatTarget_OnEnter()
{
	//Check if we are resuming the state.  Keep the sub-task, if possible.
	if(GetSubTask() && (GetSubTask()->GetTaskType() == CTaskTypes::TASK_THREAT_RESPONSE))
	{
		return;
	}

	//Ensure the target is valid.
	const CPed* pTarget = m_pTarget;
	if(!pTarget)
	{
		return;
	}
	
	//Create the task.
	CTaskThreatResponse* pTask = rage_new CTaskThreatResponse(pTarget);
	pTask->SetThreatResponseOverride(CTaskThreatResponse::TR_Fight);
	
	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskSwatOrderResponse::CombatTarget_OnUpdate()
{
	//Check if the target's whereabouts are unknown.
	if(!AreTargetsWhereaboutsKnown())
	{
		//Search for the target.
		SetState(State_FindPositionToSearchOnFoot);
	}
	//Check if the sub-task has finished.
	else if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Finish the task.
		SetState(State_Finish);
	}
	
	return FSM_Continue;
}

void CTaskSwatOrderResponse::FindPositionToSearchOnFoot_OnEnter()
{
	//Ensure the target is valid.
	const CPed* pTarget = m_pTarget;
	if(!pTarget)
	{
		return;
	}

	//Start the search.
	m_HelperForSearchOnFoot.Reset();
	if(!m_HelperForSearchOnFoot.Start(*GetPed(), *pTarget))
	{
		return;
	}
}

CTask::FSM_Return CTaskSwatOrderResponse::FindPositionToSearchOnFoot_OnUpdate()
{
	//Check if the search did not start.
	if(!m_HelperForSearchOnFoot.HasStarted())
	{
		SetState(State_Wander);
	}
	else
	{
		//Check if the results are ready.
		bool bWasSuccessful;
		if(m_HelperForSearchOnFoot.IsResultReady(bWasSuccessful, m_vPositionToSearchOnFoot))
		{
			//Check if the search was successful.
			if(bWasSuccessful)
			{
				SetState(State_SearchOnFoot);
			}
			else
			{
				SetState(State_Wander);
			}
		}
	}

	return FSM_Continue;
}

void CTaskSwatOrderResponse::SearchOnFoot_OnEnter()
{
	//Assert that the position is valid.
	taskAssert(!IsCloseAll(m_vPositionToSearchOnFoot, Vec3V(V_ZERO), ScalarVFromF32(SMALL_FLOAT)));

	//Create the move task.
	CTaskMoveFollowNavMesh* pMoveTask = rage_new CTaskMoveFollowNavMesh(CDispatchHelperSearchOnFoot::GetMoveBlendRatio(*GetPed()),
		VEC3V_TO_VECTOR3(m_vPositionToSearchOnFoot), CTaskMoveFollowNavMesh::ms_fTargetRadius, CTaskMoveFollowNavMesh::ms_fSlowDownDistance, -1, true, false, NULL);
	
	//Quit the task if a route is not found.
	pMoveTask->SetQuitTaskIfRouteNotFound(true);

	//Equip the best weapon.
	if(GetPed()->GetWeaponManager())
	{
		GetPed()->GetWeaponManager()->EquipBestWeapon();
	}

	//Create the sub-task.
	CTask* pSubTask = CEquipWeaponHelper::ShouldPedSwapWeapon(*GetPed()) ?
		rage_new CTaskSwapWeapon(SWAP_HOLSTER | SWAP_DRAW) : NULL;
	
	//Create the task.
	CTaskComplexControlMovement* pTask = rage_new CTaskComplexControlMovement(pMoveTask, pSubTask);
	pTask->SetAllowClimbLadderSubtask(true);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskSwatOrderResponse::SearchOnFoot_OnUpdate()
{
	//Check if the target's whereabouts are known.
	if(AreTargetsWhereaboutsKnown())
	{
		SetState(State_CombatTarget);
	}
	//Check if the sub-task has finished.
	else if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_FindPositionToSearchOnFoot);
	}
	//Check if the sub-task is invalid.
	else if(!GetSubTask())
	{
		SetState(State_Wander);
	}
	
	return FSM_Continue;
}

void CTaskSwatOrderResponse::Wander_OnEnter()
{
	//Generate the time to wander.
	m_fTimeToWander = fwRandom::GetRandomNumberInRange(sm_Tunables.m_MinTimeToWander, sm_Tunables.m_MaxTimeToWander);

	//Create the task.
	CTask* pTask = rage_new CTaskWander(CDispatchHelperSearchOnFoot::GetMoveBlendRatio(*GetPed()), GetPed()->GetCurrentHeading());

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskSwatOrderResponse::Wander_OnUpdate()
{
	//Check if the target's whereabouts are known.
	if(AreTargetsWhereaboutsKnown())
	{
		//Combat the target.
		SetState(State_CombatTarget);
	}
	//Check if the sub-task has finished, or our time has exceeded the threshold.
	else if(GetIsFlagSet(aiTaskFlags::SubTaskFinished) || (GetTimeInState() > m_fTimeToWander))
	{
		//Respond to the order.
		SetState(State_FindPositionToSearchOnFoot);
	}

	return FSM_Continue;
}

bool CTaskSwatOrderResponse::AreTargetsWhereaboutsKnown() const
{
	//Ensure the target is valid.
	if(!m_pTarget)
	{
		return false;
	}

	//Ensure the targeting is valid.
	CPedTargetting* pTargeting = GetPed()->GetPedIntelligence()->GetTargetting(true);
	if(!pTargeting)
	{
		return false;
	}
	
	//Ensure the target info is valid.
	const CSingleTargetInfo* pTargetInfo = pTargeting->FindTarget(m_pTarget);
	if(!pTargetInfo)
	{
		return false;
	}
	
	//Ensure the whereabouts are known.
	if(!pTargetInfo->AreWhereaboutsKnown())
	{
		return false;
	}
	
	return true;
}

bool CTaskSwatOrderResponse::CanSeeTarget() const
{
	//Ensure the target is valid.
	if(!m_pTarget)
	{
		return false;
	}

	//Ensure the targeting is valid.
	CPedTargetting* pTargeting = GetPed()->GetPedIntelligence()->GetTargetting(true);
	if(!pTargeting)
	{
		return false;
	}
	
	//Ensure the line of sight is clear.
	LosStatus nLosStatus = pTargeting->GetLosStatus(m_pTarget, false);
	if(nLosStatus != Los_clear)
	{
		return false;
	}
	
	return true;
}

int CTaskSwatOrderResponse::ChooseStateToResumeTo(bool& bKeepSubTask) const
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
			case CTaskTypes::TASK_THREAT_RESPONSE:
			{
				return State_CombatTarget;
			}
			default:
			{
				break;
			}
		}
	}

	//Do not keep the sub-task.
	bKeepSubTask = false;

	return State_Start;
}

const CEntity* CTaskSwatOrderResponse::GetIncidentEntity() const
{
	//Ensure the order is valid.
	const COrder* pOrder = GetPed()->GetPedIntelligence()->GetOrder();
	if(!pOrder)
	{
		return NULL;
	}

	//Ensure the incident is valid.
	const CIncident* pIncident = pOrder->GetIncident();
	if(!pIncident)
	{
		return NULL;
	}

	return pIncident->GetEntity();
}

#if !__FINAL
const char * CTaskSwatOrderResponse::GetStaticStateName( s32 iState  )
{
	taskAssert(iState >= State_Start && iState <= State_Finish);

	switch (iState)
	{
	case State_Start:							return "State_Start";
	case State_Resume:							return "State_Resume";
	case State_GoToStagingArea:					return "State_GoToStagingArea";
	case State_AdvanceToTarget:					return "State_AdvanceToTarget";
	case State_CombatTarget:					return "State_CombatTarget";
	case State_FindPositionToSearchOnFoot:		return "State_FindPositionToSearchOnFoot";
	case State_SearchOnFoot:					return "State_SearchOnFoot";
	case State_Finish:							return "State_Finish";
	default: taskAssert(0); 	
	}

	return "State_Invalid";
}
#endif // !__FINAL

//////////////////////////////////////////////////////////////////////////
// CTaskSwatFollowInLine
//////////////////////////////////////////////////////////////////////////

CTaskSwatFollowInLine::CTaskSwatFollowInLine()
: m_iDictionaryIndex(-1)
, m_fCrouchTimer(0.0f)
, m_vLastCornerCleared(VEC3_ZERO)
, m_fDesiredDist(.75f)
, m_fTimeToHoldAtEnd(0.0f)
, m_iFlags(0)
{
	SetInternalTaskType(CTaskTypes::TASK_SWAT_FOLLOW_IN_LINE);
}

CTaskSwatFollowInLine::CTaskSwatFollowInLine(const CAITarget& pTarget, float fDesiredDist, float fTimeToHoldAtEnd, fwFlags32 iFlags)
: m_iDictionaryIndex(-1)
, m_fCrouchTimer(0.0f)
, m_vLastCornerCleared(VEC3_ZERO)
, m_fDesiredDist(fDesiredDist)
, m_fTimeToHoldAtEnd(fTimeToHoldAtEnd)
, m_iFlags(iFlags)
{
	taskFatalAssertf(pTarget.GetIsValid(), "CTaskSwatFollowInLine must be constructed with a valid target!");
	m_target = pTarget;
	SetInternalTaskType(CTaskTypes::TASK_SWAT_FOLLOW_IN_LINE);
}

CTaskSwatFollowInLine::~CTaskSwatFollowInLine()
{}

void CTaskSwatFollowInLine::CleanUp()
{
	CPed* pPed = GetPed();
	if(pPed && !m_iFlags.IsFlagSet(AF_DisableAutoCrouch))
	{
		pPed->SetIsCrouching(false);
	}
}

CTask::FSM_Return CTaskSwatFollowInLine::ProcessPreFSM()
{
	// A ped must have a valid order to be running the swat task
	const COrder* pOrder = GetPed()->GetPedIntelligence()->GetOrder();
	if ((pOrder == NULL || !pOrder->GetIsValid()) && !m_target.GetIsValid())
	{
		return FSM_Quit;
	}

	// Not having a group would cause us to fail this task
	CPedGroup* pPedsGroup = GetPed()->GetPedsGroup();
	if (pPedsGroup && (!NetworkInterface::IsGameInProgress() || pPedsGroup->IsLocallyControlled()))
	{
		// If we don't have a formation or it isn't follow in line then we need to make sure we have the right formation
		CPedFormation* pFormation = pPedsGroup->GetFormation();
		if(!pFormation || pFormation->GetFormationType() != CPedFormationTypes::FORMATION_FOLLOW_IN_LINE)
		{
			// Set the formation to the follow in line formation and then verify the formation
			pPedsGroup->SetFormation(CPedFormationTypes::FORMATION_FOLLOW_IN_LINE);
			if(!pPedsGroup->GetFormation())
			{
				return FSM_Quit;
			}
		}
	}
	else
	{
		return FSM_Quit;
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskSwatFollowInLine::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	const COrder* pOrder = GetPed()->GetPedIntelligence()->GetOrder();

	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_GatherFollowers)
			FSM_OnEnter
				GatherFollowers_OnEnter(pOrder);
			FSM_OnUpdate
				return GatherFollowers_OnUpdate();

		FSM_State(State_AdvanceToTarget)
			FSM_OnEnter
				AdvanceToTarget_OnEnter(pOrder);
			FSM_OnUpdate
				return AdvanceToTarget_OnUpdate(pOrder);

		FSM_State(State_LeaderCrouch)
			FSM_OnEnter
				LeaderCrouch_OnEnter();
			FSM_OnUpdate
				return LeaderCrouch_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnEnter
				Finish_OnEnter();
			FSM_OnUpdate
				return Finish_OnUpdate();

	FSM_End
}

CTask::FSM_Return CTaskSwatFollowInLine::Start_OnUpdate()
{
	if(CTaskSwat::GetGroupLeader(*GetPed()) == GetPed() && !m_iFlags.IsFlagSet(AF_DisableAutoCrouch))
	{
		SetState(State_GatherFollowers);
	}
	else
	{
		SetState(State_AdvanceToTarget);
	}

	return FSM_Continue;
}

void CTaskSwatFollowInLine::LeaderStartAdvancing(const COrder* pOrder, const CPedGroup* pPedsGroup, bool bIsInitialAdvance)
{
	// Early out if we don't have a valid target
	if(!m_target.GetIsValid())
	{
		return;
	}

	CEntity* pIncidentEntity = pOrder ? pOrder->GetIncident()->GetEntity() : NULL;

	//Calculate the incident location to go to.
	Vector3 vTargetLocation;
	if(!CDispatchHelperForIncidentLocationOnFoot::Calculate(*GetPed(), RC_VEC3V(vTargetLocation)))
	{
		m_target.GetPosition(vTargetLocation);
	}

	// reset the position history if we should and Set our minimum distance to adjust speed
	CPedFormation* pFormation = pPedsGroup->GetFormation();
	pFormation->SetAdjustSpeedMinDist(s_fAdjustMinDist);

	if(bIsInitialAdvance)
	{
		if(pFormation && pFormation->GetFormationType() == CPedFormationTypes::FORMATION_FOLLOW_IN_LINE)
		{
			static_cast<CPedFormation_FollowInLine*>(pFormation)->ResetPositionHistory();
		}

		m_fCrouchTimer = fwRandom::GetRandomNumberInRange(INITIAL_TIME_BETWEEN_CROUCHES_MIN, INITIAL_TIME_BETWEEN_CROUCHES_MAX);
	}
	else
	{
		m_fCrouchTimer = fwRandom::GetRandomNumberInRange(TIME_BETWEEN_CROUCHES_MIN, TIME_BETWEEN_CROUCHES_MAX);
	}

	//sort our group and reset the crouch timer
	CTaskSwat::SortPedsGroup(*GetPed());

	// Create the seek task, allow the ped to navigate
	CTaskMoveFollowNavMesh* pNavTask = rage_new CTaskMoveFollowNavMesh(MOVEBLENDRATIO_RUN, vTargetLocation, pIncidentEntity ? s_fMaxDistToPlayer : m_fDesiredDist);
	pNavTask->SetUseLargerSearchExtents(true);
	CTaskComplexControlMovement* pTask = rage_new CTaskComplexControlMovement( pNavTask, rage_new CTaskAimSweep(CTaskAimSweep::Aim_Forward, false));
	pTask->SetAllowClimbLadderSubtask(true);
	SetNewTask(pTask);

	// Get the swat dictionary index, verify it and request the clips
	if(!m_iFlags.IsFlagSet(AF_DisableHandSignals))
	{
		m_iDictionaryIndex = fwAnimManager::FindSlotFromHashKey(ATSTRINGHASH("SWAT", 0x098787966)).Get();
		if(m_iDictionaryIndex > -1)
		{
			m_clipRequestHelper.RequestClipsByIndex(m_iDictionaryIndex);
		}
	}
}

// PURPOSE: A one time state to have the leader stop earlier than he usually would in order to gather followers
void CTaskSwatFollowInLine::GatherFollowers_OnEnter(const COrder* pOrder)
{
	CPed* pPed = GetPed();
	CPedGroup* pPedsGroup = pPed->GetPedsGroup();
	if (pPedsGroup)
	{
		LeaderStartAdvancing(pOrder, pPedsGroup, true);
	}
}

CTask::FSM_Return CTaskSwatFollowInLine::GatherFollowers_OnUpdate()
{
	// Some exit conditions for going to combat
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished) || !GetSubTask())
	{
		SetState(State_Finish);
		return FSM_Continue;
	}

	CPed* pPed = GetPed();
	CPedFormation* pFormation = pPed->GetPedsGroup()->GetFormation();
	if(pFormation && pFormation->GetFormationType() == CPedFormationTypes::FORMATION_FOLLOW_IN_LINE)
	{
		// Here we want to check if we've gotten enough positions for our followers, and as a fail safe we also check against a timer
		if(static_cast<CPedFormation_FollowInLine*>(pFormation)->HasEnoughPositionsForFollowers() || GetTimeInState() > m_fCrouchTimer)
		{
			// Don't want to interrupt any clips, plus make sure our clips have been streamed
			if(m_iFlags.IsFlagSet(AF_DisableHandSignals) || 
			   (!GetClipHelper() && m_iDictionaryIndex > -1 && m_clipRequestHelper.RequestClipsByIndex(m_iDictionaryIndex)))
			{
				// Reset the crouch timer (when going into crouch it controls the duration of the crouch)
				m_fCrouchTimer = fwRandom::GetRandomNumberInRange(CROUCH_DURATION_MIN, CROUCH_DURATION_MAX);
				SetState(State_LeaderCrouch);
				return FSM_Continue;
			}
		}
	}
	else
	{
		return FSM_Quit;
	}

	return FSM_Continue;
}

void CTaskSwatFollowInLine::AdvanceToTarget_OnEnter(const COrder* pOrder)
{
	CPed* pPed = GetPed();
	CPedGroup* pPedsGroup = pPed->GetPedsGroup();
	if (pPedsGroup)
	{		
		CPed* pLeader = CTaskSwat::GetGroupLeader(*pPed);
		if (pLeader == pPed)
		{
			LeaderStartAdvancing(pOrder, pPedsGroup, false);
		}
		else if(pLeader)
		{
			// Followers just try and follow the leader while using the additional aim task
			static bank_float s_fMaxDistToLeader = 10.0f;
			CTaskFollowLeaderInFormation* pFollowTask = rage_new CTaskFollowLeaderInFormation(pPedsGroup, pLeader, s_fMaxDistToLeader);
			// Only use larger search extents if necessary - this has the potential to overload the pathfinder if a route is not found, since pathsearch is exhaustive
			pFollowTask->SetUseLargerSearchExtents( (VEC3V_TO_VECTOR3(pLeader->GetTransform().GetPosition()) - VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition())).Mag2() > square(CPathServer::GetDefaultMaxPathFindDistFromOrigin()-10.0f) );
			pFollowTask->SetUseAdditionalAimTask(true);
			SetNewTask(pFollowTask);
		}
	}
}

CTask::FSM_Return CTaskSwatFollowInLine::AdvanceToTarget_OnUpdate(const COrder* pOrder)
{
	// Go to combat conditions
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished) || !GetSubTask())
	{
		SetState(State_Finish);
		return FSM_Continue;
	}	

	CPed* pPed = GetPed();
	CPed* pLeader = CTaskSwat::GetGroupLeader(*pPed);
	if(pLeader == pPed)
	{
		if(GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
		{
			// Update our target position
			CEntity* pIncidentEntity = pOrder ? pOrder->GetIncident()->GetEntity() : NULL;
			Vector3 vTargetLocation;
			if(pIncidentEntity)
			{
				vTargetLocation = VEC3V_TO_VECTOR3(pIncidentEntity->GetTransform().GetPosition());
			}
			else
			{
				m_target.GetPosition(vTargetLocation);
			}

			static_cast<CTaskComplexControlMovement*>(GetSubTask())->SetTarget(pPed, vTargetLocation);

			// We should stop if we've hit the crouch timer or if we've found a corner
			bool bShouldStop = !m_iFlags.IsFlagSet(AF_DisableAutoCrouch) && (GetTimeInState() > m_fCrouchTimer || CheckForCorner(pPed));
			if((bShouldStop || pPed->GetIsCrouching()) && 
			   (m_iFlags.IsFlagSet(AF_DisableHandSignals) || (!GetClipHelper() && m_iDictionaryIndex > -1 && m_clipRequestHelper.RequestClipsByIndex(m_iDictionaryIndex))))
			{
				m_fCrouchTimer = fwRandom::GetRandomNumberInRange(CROUCH_DURATION_MIN, CROUCH_DURATION_MAX);
				SetState(State_LeaderCrouch);
				return FSM_Continue;
			}
		}
	}

	// If we have a leader but it is not us
	if (pLeader && pLeader != pPed && !m_iFlags.IsFlagSet(AF_DisableAutoCrouch))
	{
		m_fCrouchTimer -= GetTimeStep();

		// Is our leader still? Are they crouching? Are we still?
		if(pLeader->GetMotionData()->GetIsStill() && pLeader->GetIsCrouching() && pPed->GetMotionData()->GetIsStill())
		{
			// Are we in a formation and in position according to that be in formation task?
			CTaskMoveBeInFormation* pFormationTask = static_cast<CTaskMoveBeInFormation*>(pPed->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_BE_IN_FORMATION));
			if(pFormationTask && pFormationTask->IsInPosition() && m_fCrouchTimer < 0.0f)
			{
				// reset the crouch timer anytime we should be crouching
				m_fCrouchTimer = fwRandom::GetRandomNumberInRange(CROUCH_DURATION_FOLLOWER_MIN, CROUCH_DURATION_FOLLOWER_MAX);
			}
		}

		// Set if we should be crouching or not (the delay is so followers don't stand up as soon as the leader does)
		GetPed()->SetIsCrouching(m_fCrouchTimer > 0.0f);
	}

	return FSM_Continue;
}

// PURPOSE: The leader has a specific crouch state to handle playing clips and how long they should be crouching
void CTaskSwatFollowInLine::LeaderCrouch_OnEnter()
{
	CPed* pPed = GetPed();

	// Let's do a quick sort of the peds group once more
	CTaskSwat::SortPedsGroup(*pPed);

	if(!m_iFlags.IsFlagSet(AF_DisableHandSignals) && m_iDictionaryIndex > -1 && m_clipRequestHelper.RequestClipsByIndex(m_iDictionaryIndex))
	{
		const u32 iClipNameHash = ATSTRINGHASH("freeze", 0x0bd42aa91);
		if (taskVerifyf(fwAnimManager::GetClipIfExistsByDictIndex(m_iDictionaryIndex, iClipNameHash), "Swat clip 'freeze' not found"))
		{
			StartClipByDictAndHash(pPed, m_iDictionaryIndex, iClipNameHash, APF_ISFINISHAUTOREMOVE, (u32)BONEMASK_ARMS, AP_MEDIUM, NORMAL_BLEND_IN_DELTA, CClipHelper::TerminationType_OnFinish);
		}
	}

	pPed->SetIsCrouching(true);
	SetNewTask(rage_new CTaskAimSweep(CTaskAimSweep::Aim_Forward, false));
}

CTask::FSM_Return CTaskSwatFollowInLine::LeaderCrouch_OnUpdate()
{
	CPed* pPed = GetPed();

	// If we've been crouching long enough we'll stop crouching and try to play our "let's move" clip and stop crouching
	if((!m_iFlags.IsFlagSet(AF_DisableAutoCrouch) && GetTimeInState() > m_fCrouchTimer) || !pPed->GetIsCrouching())
	{
		bool bShouldAdvance = m_iFlags.IsFlagSet(AF_DisableHandSignals);
		if(!bShouldAdvance && m_iDictionaryIndex > -1 && m_clipRequestHelper.RequestClipsByIndex(m_iDictionaryIndex))
		{
			const u32 iClipNameHash = ATSTRINGHASH("come", 0x0f3680904);
			if (taskVerifyf(fwAnimManager::GetClipIfExistsByDictIndex(m_iDictionaryIndex, iClipNameHash), "Swat clip 'come' not found"))
			{
				StartClipByDictAndHash(pPed, m_iDictionaryIndex, iClipNameHash, APF_ISFINISHAUTOREMOVE, (u32)BONEMASK_ARMS, AP_MEDIUM, NORMAL_BLEND_IN_DELTA, CClipHelper::TerminationType_OnFinish);
				bShouldAdvance = true;
			}
		}

		// This will be true if either hand signals are disabled or we called StartClip for the hand signal
		if(bShouldAdvance)
		{
			m_fCrouchTimer = fwRandom::GetRandomNumberInRange(TIME_BETWEEN_CROUCHES_MIN, TIME_BETWEEN_CROUCHES_MAX);
			pPed->SetIsCrouching(false);

			SetState(State_AdvanceToTarget);
		}
	}

	return FSM_Continue;
}

void CTaskSwatFollowInLine::Finish_OnEnter()
{
	if(m_fTimeToHoldAtEnd > SMALL_FLOAT && !m_iFlags.IsFlagSet(AF_DisableAutoCrouch))
	{
		GetPed()->SetIsCrouching(true);
	}

	SetNewTask(rage_new CTaskAimSweep(CTaskAimSweep::Aim_Forward, false));
}

CTask::FSM_Return CTaskSwatFollowInLine::Finish_OnUpdate()
{
	if(m_fTimeToHoldAtEnd >= 0.0f && GetTimeInState() > m_fTimeToHoldAtEnd)
	{
		if(!m_iFlags.IsFlagSet(AF_DisableAutoCrouch))
		{
			GetPed()->SetIsCrouching(false);
		}
		return FSM_Quit;
	}

	return FSM_Continue;
}

bool CTaskSwatFollowInLine::CheckForCorner(const CPed* pPed)
{
	CTaskMoveFollowNavMesh* pNavMeshTask = (CTaskMoveFollowNavMesh*) pPed->GetPedIntelligence()->FindMovementTaskByType(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH);
	Vector3 vStartPos, vCornerPosition, vBeyondCornerPosition;
	if( pNavMeshTask &&	pNavMeshTask->GetThisWaypointLine( pPed, vStartPos, vCornerPosition, 0 ) &&
		pNavMeshTask->GetThisWaypointLine( pPed, vCornerPosition, vBeyondCornerPosition, 1 ) )
	{
		bool bWalkingIntoInterior = false;
		if((pNavMeshTask->GetProgress() + 1) < pNavMeshTask->GetRouteSize())
		{
			bWalkingIntoInterior = !(pNavMeshTask->GetWaypointFlag(pNavMeshTask->GetProgress()).m_iSpecialActionFlags & WAYPOINT_FLAG_IS_INTERIOR ) &&
				(pNavMeshTask->GetWaypointFlag(pNavMeshTask->GetProgress()+1).m_iSpecialActionFlags & WAYPOINT_FLAG_IS_INTERIOR );
		}

		const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		if(	m_vLastCornerCleared.Dist2(vCornerPosition) > rage::square(5.0f) &&
			vPedPosition.Dist2(vCornerPosition) < rage::square(4.0f) )
		{
			Vector3 vLength1 = vCornerPosition - vStartPos;
			Vector3 vLength2 = vBeyondCornerPosition - vCornerPosition;
			vLength1.z = 0.0f;
			vLength2.z = 0.0f;
			vLength1.Normalize();
			vLength2.Normalize();
			if( vLength1.Dot(vLength2) < 0.98f )
			{
				m_vLastCornerCleared = vCornerPosition;
				WorldProbe::CShapeTestProbeDesc probeDesc;
				probeDesc.SetStartAndEnd(vPedPosition, vBeyondCornerPosition+ZAXIS);
				probeDesc.SetExcludeEntity(pPed);
				probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_OBJECT_TYPE|ArchetypeFlags::GTA_ALL_MAP_TYPES);
				probeDesc.SetContext(WorldProbe::LOS_GeneralAI);
				if( bWalkingIntoInterior || WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc) )
				{
					return true;
				}
			}
		}
	}

	return false;
}

#if !__FINAL
const char * CTaskSwatFollowInLine::GetStaticStateName( s32 iState  )
{
	taskAssert(iState >= State_Start && iState <= State_Finish);

	switch (iState)
	{
		case State_Start:							return "State_Start";
		case State_GatherFollowers:					return "State_GatherFollowers";
		case State_AdvanceToTarget:					return "State_AdvanceToTarget";
		case State_LeaderCrouch:					return "State_LeaderCrouch";
		case State_Finish:							return "State_Finish";
		default: taskAssert(0); 	
	}

	return "State_Invalid";
}
#endif // !__FINAL

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskSwatGoToStagingArea
/////////////////////////////////////////////////////////////////////////////////////////////////////////

CTaskSwatGoToStagingArea::CTaskSwatGoToStagingArea()
{
	SetInternalTaskType(CTaskTypes::TASK_SWAT_GO_TO_STAGING_AREA);
}

CTaskSwatGoToStagingArea::~CTaskSwatGoToStagingArea()
{

}

CTask::FSM_Return CTaskSwatGoToStagingArea::ProcessPreFSM()
{
	const COrder* pOrder = GetPed()->GetPedIntelligence()->GetOrder();

	if (!pOrder || !pOrder->GetIsValid())
	{
		return FSM_Quit;
	}
	
	return FSM_Continue;
}

CTask::FSM_Return CTaskSwatGoToStagingArea::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	FSM_Begin
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_RespondToOrder)
			FSM_OnUpdate
				return RespondToOrder_OnUpdate();

		FSM_State(State_GoToIncidentLocationInVehicle)
			FSM_OnEnter
				GoToIncidentLocationInVehicle_OnEnter();
			FSM_OnUpdate
				return GoToIncidentLocationInVehicle_OnUpdate();

		FSM_State(State_GoToIncidentLocationAsPassenger)
			FSM_OnEnter
				GoToIncidentLocationAsPassenger_OnEnter();
			FSM_OnUpdate
				return GoToIncidentLocationAsPassenger_OnUpdate();

		FSM_State(State_SearchInVehicle)
			FSM_OnEnter
				SearchInVehicle_OnEnter();
			FSM_OnUpdate
				return SearchInVehicle_OnUpdate();

		FSM_State(State_ExitVehicle)
			FSM_OnEnter
				ExitVehicle_OnEnter();
			FSM_OnUpdate
				return ExitVehicle_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

CTask::FSM_Return CTaskSwatGoToStagingArea::Start_OnUpdate()
{
	const COrder* pOrder = GetPed()->GetPedIntelligence()->GetOrder();
	if (pOrder == NULL || !pOrder->GetIsValid())
	{
		return FSM_Continue;
	}

	SetState(State_RespondToOrder);
	return FSM_Continue;
}

CTask::FSM_Return CTaskSwatGoToStagingArea::RespondToOrder_OnUpdate()
{
	CPed* pPed = GetPed();

	if (pPed->GetIsInVehicle())
	{
		if (pPed->GetIsDrivingVehicle())
		{
			SetState(State_GoToIncidentLocationInVehicle);
		}
		else
		{
			SetState(State_GoToIncidentLocationAsPassenger);
		}
	}
	else
	{
		SetState(State_Finish);
	}

	return FSM_Continue;
}

void CTaskSwatGoToStagingArea::GoToIncidentLocationInVehicle_OnEnter()
{
	const COrder* pOrder = GetPed()->GetPedIntelligence()->GetOrder();
	if(aiVerifyf(GetPed()->GetIsInVehicle(), "GoToIncidentLocationInVehicle_OnEnter: Not in vehicle!"))
	{
		aiAssert(GetPed()->GetMyVehicle());
		GetPed()->GetMyVehicle()->TurnSirenOn(true);

		Vec3V vLocation = VECTOR3_TO_VEC3V(static_cast<const CSwatOrder*>(pOrder)->GetStagingLocation());

#if __BANK
		ms_debugDraw.AddSphere(vLocation, 0.05f, Color_purple, 4000);
#endif

		sVehicleMissionParams params;
		params.m_iDrivingFlags = CDispatchHelperForIncidentLocationInVehicle::GetDrivingFlags();
		params.m_fCruiseSpeed = CDispatchHelperForIncidentLocationInVehicle::GetCruiseSpeed(*GetPed());
		params.SetTargetPosition(RC_VECTOR3(vLocation));
		params.m_fTargetArriveDist = CDispatchHelperForIncidentLocationInVehicle::GetTargetArriveDist();

		//Create the vehicle task.
		CTask* pVehicleTask = rage_new CTaskVehicleApproach(params);

		//Create the task.
		CTask* pTask = rage_new CTaskControlVehicle(GetPed()->GetMyVehicle(), pVehicleTask);

		//Start the task.
		SetNewTask(pTask);
	}
}

CTask::FSM_Return CTaskSwatGoToStagingArea::GoToIncidentLocationInVehicle_OnUpdate()
{
	CPed* pPed = GetPed();
	COrder* pOrder = pPed->GetPedIntelligence()->GetOrder();

	if (!pPed->GetIsInVehicle())
	{
		SetState(State_RespondToOrder);
		return FSM_Continue;
	}

	CSwatOrder* pSwatOrder = static_cast<CSwatOrder*>(pOrder);

	if (pSwatOrder && pSwatOrder->GetNeedsToUpdateLocation())
	{
		if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_CONTROL_VEHICLE)
		{
			aiTask* pTask = static_cast<CTaskControlVehicle*>(GetSubTask())->GetVehicleTask();
			if (pTask && (pTask->GetTaskType() == CTaskTypes::TASK_VEHICLE_GOTO_AUTOMOBILE_NEW))
			{
				Vector3 vDispatchLocation = pSwatOrder->GetStagingLocation();
				static_cast<CTaskVehicleGoToAutomobileNew*>(pTask)->SetTargetPosition(&vDispatchLocation);
				pSwatOrder->SetNeedsToUpdateLocation(false);
			}
		}
	}

	// Arrived at location
	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		if(pOrder && !pOrder->GetEntityArrivedAtIncident())
		{
			pOrder->SetEntityArrivedAtIncident();
		}

		if(!AreTargetsWhereaboutsKnown() && IsTargetInVehicle())
		{
			SetState(State_SearchInVehicle);
		}
		else
		{
			SetState(State_ExitVehicle);
		}
	}

	return FSM_Continue;
}

void CTaskSwatGoToStagingArea::GoToIncidentLocationAsPassenger_OnEnter()
{
}

CTask::FSM_Return CTaskSwatGoToStagingArea::GoToIncidentLocationAsPassenger_OnUpdate()
{
	if (!GetPed()->GetIsInVehicle())
	{
		SetState(State_RespondToOrder);
		return FSM_Continue;
	}

	// Exit the vehicle if the driver has left, is dead or is leaving
	CPed* pDriver = GetPed()->GetMyVehicle()->GetDriver();
	if( pDriver == NULL ||
		pDriver->IsInjured() ||
		pDriver->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE))
	{
		SetState(State_ExitVehicle);
		return FSM_Continue;
	}

	return FSM_Continue;
}

void CTaskSwatGoToStagingArea::SearchInVehicle_OnEnter()
{
	//Ensure the ped is driving a vehicle.
	if(!taskVerifyf(GetPed()->GetIsDrivingVehicle(), "The ped is not driving a vehicle."))
	{
		return;
	}

	//Ensure the target is valid.
	const CPed* pTarget = GetIncidentPed();
	if(!pTarget)
	{
		return;
	}

	//Generate a random search position.
	Vec3V vSearchPosition;
	if(!CDispatchHelperSearchInAutomobile::Calculate(*GetPed(), *pTarget, vSearchPosition))
	{
		return;
	}

	//Turn the siren on.
	GetPed()->GetVehiclePedInside()->TurnSirenOn(true);

	//Create the params.
	sVehicleMissionParams params;
	params.m_iDrivingFlags = CDispatchHelperSearchInAutomobile::GetDrivingFlags();
	params.m_fCruiseSpeed = CDispatchHelperSearchInAutomobile::GetCruiseSpeed();
	params.SetTargetPosition(VEC3V_TO_VECTOR3(vSearchPosition));
	params.m_fTargetArriveDist = 20.0f;

	//Create the vehicle task.
	aiTask* pVehicleTask = CVehicleIntelligence::CreateAutomobileGotoTask(params, sVehicleMissionParams::DEFAULT_SWITCH_TO_STRAIGHT_LINE_DIST);

	//Create the task.
	CTask* pTask = rage_new CTaskControlVehicle(GetPed()->GetVehiclePedInside(), pVehicleTask);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskSwatGoToStagingArea::SearchInVehicle_OnUpdate()
{
	//Check if the ped is not in a vehicle.
	if(!GetPed()->GetIsInVehicle())
	{
		SetState(State_RespondToOrder);
	}
	//Check if the sub-task is invalid.
	else if(!GetSubTask())
	{
		SetState(State_RespondToOrder);
	}
	//Check if the sub-task finished.
	else if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetFlag(aiTaskFlags::RestartCurrentState);
	}
	//Check if the target whereabouts are known.
	else if(AreTargetsWhereaboutsKnown())
	{
		SetState(State_RespondToOrder);
	}

	return FSM_Continue;
}

void CTaskSwatGoToStagingArea::ExitVehicle_OnEnter()
{
	if (aiVerifyf(GetPed()->GetIsInVehicle(), "ExitVehicle_OnEnter: Ped not in vehicle!"))
	{
		CPed* pPed = GetPed();

		VehicleEnterExitFlags vehicleFlags;
		vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DontCloseDoor);
		vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DelayForTime);
		SetNewTask(rage_new CTaskExitVehicle(pPed->GetMyVehicle(), vehicleFlags, fwRandom::GetRandomNumberInRange(0.0f, 0.5f)));
	}
}

CTask::FSM_Return CTaskSwatGoToStagingArea::ExitVehicle_OnUpdate()
{
	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished) || GetSubTask() == NULL)
	{
		CPed* pPed = GetPed();
		weaponAssert(pPed->GetWeaponManager());
		pPed->GetWeaponManager()->EquipBestWeapon();
		SetState(State_Finish);
	}
	return FSM_Continue;
}

bool CTaskSwatGoToStagingArea::AreTargetsWhereaboutsKnown() const
{
	//Ensure the incident entity is valid.
	const CEntity* pIncidentEntity = GetIncidentEntity();
	if(!pIncidentEntity)
	{
		return false;
	}

	//Ensure the targeting is valid.
	CPedTargetting* pTargeting = GetPed()->GetPedIntelligence()->GetTargetting(true);
	if(!pTargeting)
	{
		return false;
	}

	//Ensure the target info is valid.
	const CSingleTargetInfo* pTargetInfo = pTargeting->FindTarget(pIncidentEntity);
	if(!pTargetInfo)
	{
		return false;
	}

	//Ensure the whereabouts are known.
	if(!pTargetInfo->AreWhereaboutsKnown())
	{
		return false;
	}

	return true;
}

const CEntity* CTaskSwatGoToStagingArea::GetIncidentEntity() const
{
	//Ensure the order is valid.
	const COrder* pOrder = GetPed()->GetPedIntelligence()->GetOrder();
	if(!pOrder)
	{
		return NULL;
	}

	//Ensure the incident is valid.
	const CIncident* pIncident = pOrder->GetIncident();
	if(!pIncident)
	{
		return NULL;
	}

	return pIncident->GetEntity();
}

const CPed* CTaskSwatGoToStagingArea::GetIncidentPed() const
{
	//Ensure the entity is valid.
	const CEntity* pEntity = GetIncidentEntity();
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

bool CTaskSwatGoToStagingArea::IsTargetInVehicle() const
{
	//Ensure the target is valid.
	const CPed* pTarget = GetIncidentPed();
	if(!pTarget)
	{
		return false;
	}

	return pTarget->GetIsInVehicle();
}

#if !__FINAL
void CTaskSwatGoToStagingArea::Debug() const
{
#if DEBUG_DRAW
	const CPed* pPed = GetPed();
	const COrder* pOrder = pPed->GetPedIntelligence()->GetOrder();
	if (pOrder)
	{
		grcDebugDraw::Line(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), static_cast<const CSwatOrder*>(pOrder)->GetStagingLocation(), Color_blue);
		grcDebugDraw::Sphere(static_cast<const CSwatOrder*>(pOrder)->GetStagingLocation(), 0.05f, Color_blue);
	}
#endif
}

const char * CTaskSwatGoToStagingArea::GetStaticStateName( s32 iState  )
{
	taskAssert(iState >= State_Start && iState <= State_Finish);

	switch (iState)
	{
		case State_Start:							return "State_Start";
		case State_RespondToOrder:					return "State_RespondToOrder";
		case State_GoToIncidentLocationInVehicle:	return "State_GoToIncidentLocationInVehicle";
		case State_GoToIncidentLocationAsPassenger:	return "State_GoToIncidentLocationAsPassenger";
		case State_SearchInVehicle:					return "State_SearchInVehicle";
		case State_ExitVehicle:						return "State_ExitVehicle";
		case State_Finish:							return "State_Finish";
		default: taskAssert(0); 	
	}

	return "State_Invalid";
}
#endif // !__FINAL

/////////////////////////////
// CTaskSwatWantedResponse //
/////////////////////////////

#define PASSENGER_START_RAPPEL_DIST2 625.0f

CTaskSwatWantedResponse::CTaskSwatWantedResponse(CPed* pWantedPed)
: m_pWantedPed(pWantedPed)
{
	SetInternalTaskType(CTaskTypes::TASK_SWAT_WANTED_RESPONSE);
}

CTaskSwatWantedResponse::~CTaskSwatWantedResponse()
{

}

void CTaskSwatWantedResponse::CleanUp()
{

}

bool CTaskSwatWantedResponse::ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
	//Check if the priority is not immediate.
	if(iPriority != ABORT_PRIORITY_IMMEDIATE)
	{
		//Check if the event is valid.
		if(pEvent)
		{
			//Always respond to damage events if it's a water cannon doing the damage
			if(static_cast< const CEvent * >(pEvent)->GetEventType() == EVENT_DAMAGE)
			{
				static const atHashWithStringNotFinal waterCannonWeaponName("WEAPON_HIT_BY_WATER_CANNON",0xCC34325E);
				const CEventDamage* pDamageTask = static_cast<const CEventDamage*>(pEvent);
				if(pDamageTask->GetWeaponUsedHash() == waterCannonWeaponName)
				{
					return true;
				}
				// 'Death' has higher priority than switching to ragdoll so if a ped that was initially told to die an animated death but then switches
				// to ragdoll the animated death event still takes priority. We need to allow those events through otherwise we could end up with a
				// ragdoll that didn't have an appropriate task
				else if(pDamageTask->HasKilledPed())
				{
					return true;
				}
			}
	
			//Check the response task.
			CEvent* pGameEvent = static_cast<CEvent *>(const_cast<aiEvent *>(pEvent));
			switch(pGameEvent->GetResponseTaskType())
			{
				case CTaskTypes::TASK_SWAT_WANTED_RESPONSE:
				{
					//Throw away the event.
					pGameEvent->Tick();

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

aiTask::FSM_Return CTaskSwatWantedResponse::ProcessPreFSM()
{
	const CPed* pPed = GetPed();
	const COrder* pOrder = pPed->GetPedIntelligence()->GetOrder();
	if ((!pOrder || !pOrder->GetIsValid()) && GetState() != State_HeliFlee)
	{
		return FSM_Quit;
	}

	if(!m_pWantedPed)
	{
		return FSM_Quit;
	}

	return FSM_Continue;
}

aiTask::FSM_Return CTaskSwatWantedResponse::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_Resume)
			FSM_OnUpdate
				return Resume_OnUpdate();

		FSM_State(State_Combat)
			FSM_OnEnter
				Combat_OnEnter();
			FSM_OnUpdate
				return Combat_OnUpdate();

		FSM_State(State_HeliFlee)
			FSM_OnEnter
				HeliFlee_OnEnter();
			FSM_OnUpdate
				return HeliFlee_OnUpdate();
			FSM_OnExit
				HeliFlee_OnExit();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

bool CTaskSwatWantedResponse::AddGetUpSets( CNmBlendOutSetList& sets, CPed* pPed )
{
	if (m_pWantedPed && pPed->GetWeaponManager())
	{
		CAITarget target;
		target.SetEntity(m_pWantedPed);
		return CTaskGun::SetupAimingGetupSet(sets, pPed, target);
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////

void CTaskSwatWantedResponse::GetUpComplete(eNmBlendOutSet setMatched, CNmBlendOutItem* UNUSED_PARAM(pLastItem), CTaskMove* UNUSED_PARAM(pMoveTask), CPed* pPed)
{
	if (
		setMatched==NMBS_ARMED_1HANDED_GETUPS
		|| setMatched== NMBS_ARMED_2HANDED_GETUPS
		|| setMatched==NMBS_ARMED_1HANDED_SHOOT_FROM_GROUND
		|| setMatched==NMBS_ARMED_2HANDED_SHOOT_FROM_GROUND
		|| setMatched==NMBS_STANDING_AIMING_CENTRE
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
		pPed->SetPedResetFlag(CPED_RESET_FLAG_InstantBlendToAim, true);
		pPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAIUpdate, true); //need a post camera update to process the start state of the next task
		pPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true); //need a post camera update to process the start state of the next task
		pPed->SetPedResetFlag(CPED_RESET_FLAG_ePostCameraAnimUpdateUseZeroTimestep, true);
	}
}

//////////////////////////////////////////////////////////////////////////

void CTaskSwatWantedResponse::RequestGetupAssets(CPed* pPed)
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
		m_GetupReqHelpers[idx++].Request(NMBS_ARMED_1HANDED_GETUPS);
		NetworkInterface::RequestNetworkAnimRequest(pPed, NMBS_ARMED_1HANDED_GETUPS, true);

		m_GetupReqHelpers[idx++].Request(NMBS_ARMED_1HANDED_SHOOT_FROM_GROUND);
		NetworkInterface::RequestNetworkAnimRequest(pPed, NMBS_ARMED_1HANDED_SHOOT_FROM_GROUND);

		if(!pPed->ShouldDoHurtTransition())
		{
			m_GetupReqHelpers[idx++].Request(NMBS_STANDING_AIMING_CENTRE);
			NetworkInterface::RequestNetworkAnimRequest(pPed, NMBS_STANDING_AIMING_CENTRE);
		}
		else
		{
			m_GetupReqHelpers[idx++].Request(NMBS_STANDING_AIMING_CENTRE_PISTOL_HURT);
			NetworkInterface::RequestNetworkAnimRequest(pPed, NMBS_STANDING_AIMING_CENTRE_PISTOL_HURT);
		}
	}
	else if(bArmed2Handed)
	{
		m_GetupReqHelpers[idx++].Request(NMBS_ARMED_2HANDED_GETUPS);
		NetworkInterface::RequestNetworkAnimRequest(pPed, NMBS_ARMED_2HANDED_GETUPS, true);

		m_GetupReqHelpers[idx++].Request(NMBS_ARMED_2HANDED_SHOOT_FROM_GROUND);
		NetworkInterface::RequestNetworkAnimRequest(pPed, NMBS_ARMED_2HANDED_SHOOT_FROM_GROUND);

		if(!pPed->ShouldDoHurtTransition())
		{
			m_GetupReqHelpers[idx++].Request(NMBS_STANDING_AIMING_CENTRE_RIFLE);
			NetworkInterface::RequestNetworkAnimRequest(pPed, NMBS_STANDING_AIMING_CENTRE_RIFLE);
		}
		else
		{
			m_GetupReqHelpers[idx++].Request(NMBS_STANDING_AIMING_CENTRE_RIFLE_HURT);
			NetworkInterface::RequestNetworkAnimRequest(pPed, NMBS_STANDING_AIMING_CENTRE_RIFLE_HURT);
		}
	}

	if(pPed->ShouldDoHurtTransition())
	{
		m_GetupReqHelpers[idx++].Request(NMBS_WRITHE);
		NetworkInterface::RequestNetworkAnimRequest(pPed, NMBS_WRITHE);
	}
}

aiTask::FSM_Return CTaskSwatWantedResponse::Start_OnUpdate()
{
	SetState(State_Combat);
	return FSM_Continue;
}

CTask::FSM_Return CTaskSwatWantedResponse::Resume_OnUpdate()
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

void CTaskSwatWantedResponse::Combat_OnEnter()
{
	//Check if we are resuming the state.  Keep the sub-task, if possible.
	if(GetSubTask() && (GetSubTask()->GetTaskType() == CTaskTypes::TASK_THREAT_RESPONSE))
	{
		return;
	}

	//Create the task.
	CTaskThreatResponse* pTask = rage_new CTaskThreatResponse(m_pWantedPed);
	pTask->SetThreatResponseOverride(CTaskThreatResponse::TR_Fight);

	//Start the task.
	SetNewTask(pTask);
}

aiTask::FSM_Return CTaskSwatWantedResponse::Combat_OnUpdate()
{
	if (!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Finish);
	}
	
	// Check if we are the driver of a helicopter
	CPed* pPed = GetPed();
	CVehicle* pVehicle = pPed->GetVehiclePedInside();
	if( pVehicle && pVehicle->InheritsFromHeli() && pVehicle->IsInDriveableState() &&
		pVehicle->IsDriver(pPed) && !pPed->IsExitingVehicle() )
	{
		// Check if we have any passengers that are alive
		bool bHasAlivePassengers = false;
		const CSeatManager* pSeatManager = pVehicle->GetSeatManager();
		for (s32 iSeat= 0; iSeat < pSeatManager->GetMaxSeats(); iSeat++)
		{
			if(iSeat != pVehicle->GetDriverSeat())
			{
				CPed* pPedInSeat = pSeatManager->GetPedInSeat(iSeat);
				if(pPedInSeat && !pPedInSeat->IsInjured())
				{
					bHasAlivePassengers = true;
				}
			}
		}

		// If there are no passengers alive then we should leave
		if(!bHasAlivePassengers)
		{
			SetState(State_HeliFlee);
		}
	}

	//Check if the heli needs to refuel.
	if(CHeliRefuelHelper::Process(*GetPed(), *m_pWantedPed))
	{
		SetState(State_HeliFlee);
	}

	return FSM_Continue;
}

void CTaskSwatWantedResponse::HeliFlee_OnEnter()
{
	//Let the heli be deleted with alive peds in it.
	GetPed()->GetMyVehicle()->m_nVehicleFlags.bCanBeDeletedWithAlivePedsInIt = true;

	//Create the task.
	CTask* pTask = CHeliFleeHelper::CreateTaskForPed(*GetPed(), CAITarget(m_pWantedPed));
	taskAssert(pTask);

	//Start the task.
	SetNewTask(pTask);
}

aiTask::FSM_Return CTaskSwatWantedResponse::HeliFlee_OnUpdate()
{
	//Check if the sub-task has finished.
	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Finish);
	}

	return FSM_Continue;
}

void CTaskSwatWantedResponse::HeliFlee_OnExit()
{
	CVehicle* pVehicle = GetPed()->GetMyVehicle();
	if(pVehicle)
	{
		pVehicle->m_nVehicleFlags.bCanBeDeletedWithAlivePedsInIt = false;
	}
}

int CTaskSwatWantedResponse::ChooseStateToResumeTo(bool& bKeepSubTask) const
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
			case CTaskTypes::TASK_THREAT_RESPONSE:
			{
				return State_Combat;
			}
			default:
			{
				break;
			}
		}
	}

	//Do not keep the sub-task.
	bKeepSubTask = false;

	return State_Start;
}

CHeli* CTaskSwatWantedResponse::GetHeli() const
{
	//Ensure the ped is in a vehicle.
	CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
	if(!pVehicle)
	{
		return NULL;
	}

	//Ensure the vehicle is a heli.
	if(!pVehicle->InheritsFromHeli())
	{
		return NULL;
	}

	return static_cast<CHeli *>(pVehicle);
}

#if !__FINAL
const char * CTaskSwatWantedResponse::GetStaticStateName( s32 iState )
{
	taskAssert(iState >= State_Start && iState <= State_Finish);

	switch (iState)
	{
	case State_Start:				return "State_Start";
	case State_Resume:				return "State_Resume";
	case State_Combat:				return "State_Combat";
	case State_HeliFlee:			return "State_HeliFlee";
	case State_Finish:				return "State_Finish";
	default: taskAssert(0); 	
	}

	return "State_Invalid";
}
#endif // !__FINAL


/////////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskHeliOrderResponse
/////////////////////////////////////////////////////////////////////////////////////////////////////////

CTaskHeliOrderResponse::Tunables CTaskHeliOrderResponse::sm_Tunables;
IMPLEMENT_SERVICE_TASK_TUNABLES(CTaskHeliOrderResponse, 0xaced8938);

CTaskHeliOrderResponse::CTaskHeliOrderResponse()
: m_vPositionToSearchOnFoot(V_ZERO)
, m_pTarget(NULL)
, m_fTimeOrderChangeHasBeenValid(0.0f)
, m_bOverrideRopeLength(false)
{
	SetInternalTaskType(CTaskTypes::TASK_HELI_ORDER_RESPONSE);
}

CTaskHeliOrderResponse::~CTaskHeliOrderResponse()
{}

bool CTaskHeliOrderResponse::ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent)
{
	if(iPriority == ABORT_PRIORITY_IMMEDIATE)
	{
		return true;
	}

	if (pEvent && pEvent->GetEventType() == EVENT_GIVE_PED_TASK)
	{
		if (static_cast<const CEventGivePedTask*>(pEvent)->GetTask() && static_cast<const CEventGivePedTask*>(pEvent)->GetTask()->GetTaskType() == CTaskTypes::TASK_EXIT_VEHICLE)
		{
			return true;
		}
	}

	CPed* pPed = GetPed();
	if(GetSubTask() && GetState() != State_GoToStagingAreaAsPassenger && GetState() != State_PlayEntryAnim && !pPed->GetIsDrivingVehicle())
	{
		if(pEvent && GetState() == State_PassengerCombatTarget)
		{
			//Check the response task.
			CEvent* pGameEvent = static_cast<CEvent *>(const_cast<aiEvent *>(pEvent));
			switch(pGameEvent->GetResponseTaskType())
			{
				case CTaskTypes::TASK_SWAT_WANTED_RESPONSE:
				{
					//Throw away the event.
					pGameEvent->Tick();

					return false;
				}
				default:
					break;
			}
		}

		return CTask::ShouldAbort(iPriority, pEvent);
	}
	else
	{
		// Don't abort if we're dropping off peds
		if((GetState() == State_GoToStagingAreaAsDriver))
		{
			if(pEvent)
			{
				int iSwatOrderType = GetSwatOrderTypeForPed(*pPed);
				if( iSwatOrderType == CSwatOrder::SO_WANTED_GOTO_DROP_OFF_LOCATION_AS_DRIVER || 
					iSwatOrderType == CSwatOrder::SO_WANTED_GOTO_ABSEILING_LOCATION_AS_DRIVER ||
					iSwatOrderType == CSwatOrder::SO_NONE )
				{
					if(pEvent->GetEventType() != EVENT_DEATH)
					{
						static_cast<CEvent*>(const_cast<aiEvent*>(pEvent))->Tick();
					}
					else
					{
						return true;
					}
				}
			}

			if(HasRappellingPeds() || HasPedsWaitingToBeDroppedOff())
			{
				return false;
			}
		}

		//Don't abort if we're waiting to be dropped off.
		if(IsWaitingToBeDroppedOff())
		{
			return false;
		}

		//Don't abort if we're fleeing.
		if(GetState() == State_DriverFlee)
		{
			return false;
		}

		COrder* pOrder = pPed->GetPedIntelligence()->GetOrder();
		if(pOrder && pOrder->GetType() == COrder::OT_Swat)
		{
			CSwatOrder* pSwatOrder = static_cast<CSwatOrder*>(pOrder);

			if( pSwatOrder->GetSwatOrderType() == CSwatOrder::SO_WANTED_GOTO_DROP_OFF_LOCATION_AS_DRIVER &&
				m_fTimeAfterDropOff <= sm_Tunables.m_MaxTimeAfterDropOffBeforeFlee )
			{
				return false;
			}

			// Check our conditions for preventing an abort
			if( pSwatOrder->GetSwatOrderType() == CSwatOrder::SO_WANTED_GOTO_ABSEILING_LOCATION_AS_DRIVER &&
				GetState() != State_SearchWantedAreaAsDriver && !pPed->GetIsOnFoot() && pEvent && 
			    ((CEvent*)pEvent)->GetEventPriority() < E_PRIORITY_DAMAGE )
			{
				return false;
			}
		}
	}

	return true;
}

CTask::FSM_Return CTaskHeliOrderResponse::ProcessPreFSM()
{
	// A ped must have a valid order to be running the swat task
	s32 iState = GetState();
	const COrder* pOrder = GetPed()->GetPedIntelligence()->GetOrder();
	if( (pOrder == NULL || !pOrder->GetIsValid()) && 
		iState != State_PassengerRappel && iState != State_DriverFlee &&
		(iState != State_GoToStagingAreaAsDriver || !HasRappellingPeds()) )
	{
		return FSM_Quit;
	}

	// Make sure the targeting system is activated.
	CPedTargetting* pPedTargetting = GetPed()->GetPedIntelligence()->GetTargetting( true );
	Assert(pPedTargetting);

	if (pPedTargetting)
	{
		pPedTargetting->SetInUse();

		// Make sure the targeting system is activated, scan for a new target
		CPed* pBestTarget = pPedTargetting->GetBestTarget();
		if( pBestTarget && pBestTarget != m_pTarget )
		{
			m_pTarget = pBestTarget;
		}
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskHeliOrderResponse::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnEnter
				Start_OnEnter();
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_GoToStagingAreaAsDriver)
			FSM_OnEnter
				GoToStagingAreaAsDriver_OnEnter();
			FSM_OnUpdate
				return GoToStagingAreaAsDriver_OnUpdate();
			FSM_OnExit
				GoToStagingAreaAsDriver_OnExit();

		FSM_State(State_DriverFlee)
			FSM_OnEnter
				DriverFlee_OnEnter();
			FSM_OnUpdate
				return DriverFlee_OnUpdate();
			FSM_OnExit
				DriverFlee_OnExit();

		FSM_State(State_PlayEntryAnim)
			FSM_OnEnter
				PlayEntryAnim_OnEnter();
			FSM_OnUpdate
				return PlayEntryAnim_OnUpdate();

		FSM_State(State_GoToStagingAreaAsPassenger)
			FSM_OnEnter
				GoToStagingAreaAsPassenger_OnEnter();
			FSM_OnUpdate
				return GoToStagingAreaAsPassenger_OnUpdate();

		FSM_State(State_PassengerRappel)
			FSM_OnEnter
				PassengerRappel_OnEnter();
			FSM_OnUpdate
				return PassengerRappel_OnUpdate();

		FSM_State(State_ExitVehicle)
			FSM_OnEnter
				ExitVehicle_OnEnter();
			FSM_OnUpdate
				return ExitVehicle_OnUpdate();

		FSM_State(State_PassengerMoveToTarget)
			FSM_OnEnter
				PassengerMoveToTarget_OnEnter();
			FSM_OnUpdate
				return PassengerMoveToTarget_OnUpdate();

		FSM_State(State_PassengerCombatTarget)
			FSM_OnEnter
				PassengerCombatTarget_OnEnter();
			FSM_OnUpdate
				return PassengerCombatTarget_OnUpdate();

		FSM_State(State_FindPositionToSearchOnFoot)
			FSM_OnEnter
				FindPositionToSearchOnFoot_OnEnter();
			FSM_OnUpdate
				return FindPositionToSearchOnFoot_OnUpdate();
				
		FSM_State(State_SearchOnFoot)
			FSM_OnEnter
				SearchOnFoot_OnEnter();
			FSM_OnUpdate
				return SearchOnFoot_OnUpdate();

		FSM_State(State_SearchWantedAreaAsDriver)
			FSM_OnEnter
				SearchWantedAreaAsDriver_OnEnter();
			FSM_OnUpdate
				return SearchWantedAreaAsDriver_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

void CTaskHeliOrderResponse::Start_OnEnter()
{
	const COrder* pOrder = GetPed()->GetPedIntelligence()->GetOrder();

	// Get and set our target entity
	if (pOrder && pOrder->GetIncident())
	{
		CEntity* pIncidentEntity = pOrder->GetIncident()->GetEntity();
		if (pIncidentEntity && pIncidentEntity->GetIsTypePed())
		{
			m_pTarget = static_cast<CPed*>(pIncidentEntity);
		}
	}
}

CTask::FSM_Return CTaskHeliOrderResponse::Start_OnUpdate()
{
	CPed* pPed = GetPed();
	if(pPed->GetIsDrivingVehicle())
	{
		SetState(State_GoToStagingAreaAsDriver);
	}
	else
	{
		CVehicle* pMyVehicle = pPed->GetMyVehicle();
		if(pMyVehicle && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_AdditionalRappellingPed))
		{
			s32 iSeatIndex = pMyVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
			fwMvClipSetId iClipSet = (iSeatIndex == Seat_backLeft) ? CLIP_SET_VEH_HELI_REAR_DESCEND_ENTER_DS : CLIP_SET_VEH_HELI_REAR_DESCEND_ENTER_PS;
			if(m_clipSetRequestHelper.Request(iClipSet))
			{
				SetState(State_PlayEntryAnim);
			}
		}
		else
		{
			SetState(State_GoToStagingAreaAsPassenger);
		}
	}
	return FSM_Continue;
}

void CTaskHeliOrderResponse::ActivateTimeslicing()
{
	TUNE_GROUP_BOOL(HELI_ORDER_RESPONSE, ACTIVATE_TIMESLICING, true);
	if(ACTIVATE_TIMESLICING)
	{
		GetPed()->GetPedAiLod().ClearBlockedLodFlag(CPedAILod::AL_LodTimesliceIntelligenceUpdate);
	}
}

bool CTaskHeliOrderResponse::AreTargetsWhereaboutsKnown() const
{
	//Ensure the incident entity is valid.
	const CEntity* pIncidentEntity = GetIncidentEntity();
	if(!pIncidentEntity)
	{
		return false;
	}

	//Ensure the targeting is valid.
	CPedTargetting* pTargeting = GetPed()->GetPedIntelligence()->GetTargetting(true);
	if(!pTargeting)
	{
		return false;
	}
	
	//Ensure the target info is valid.
	const CSingleTargetInfo* pTargetInfo = pTargeting->FindTarget(pIncidentEntity);
	if(!pTargetInfo)
	{
		return false;
	}
	
	//Ensure the whereabouts are known.
	if(!pTargetInfo->AreWhereaboutsKnown())
	{
		return false;
	}
	
	return true;
}

bool CTaskHeliOrderResponse::CanSeeTarget() const
{
	//Ensure the incident entity is valid.
	const CEntity* pIncidentEntity = GetIncidentEntity();
	if(!pIncidentEntity)
	{
		return false;
	}
	
	//Ensure the targeting is valid.
	CPedTargetting* pTargeting = GetPed()->GetPedIntelligence()->GetTargetting(true);
	if(!pTargeting)
	{
		return false;
	}
	
	//Ensure the line of sight is clear.
	LosStatus nLosStatus = pTargeting->GetLosStatus(pIncidentEntity, false);
	if(nLosStatus != Los_clear)
	{
		return false;
	}
	
	return true;
}

CHeli* CTaskHeliOrderResponse::GetHeli() const
{
	//Ensure the ped is in a vehicle.
	CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
	if(!pVehicle)
	{
		return NULL;
	}

	//Ensure the vehicle is a heli.
	if(!pVehicle->InheritsFromHeli())
	{
		return NULL;
	}

	return static_cast<CHeli *>(pVehicle);
}

const CEntity* CTaskHeliOrderResponse::GetIncidentEntity() const
{
	//Ensure the order is valid.
	const COrder* pOrder = GetPed()->GetPedIntelligence()->GetOrder();
	if(!pOrder)
	{
		return NULL;
	}
	
	//Ensure the incident is valid.
	const CIncident* pIncident = pOrder->GetIncident();
	if(!pIncident)
	{
		return NULL;
	}
	
	return pIncident->GetEntity();
}

int CTaskHeliOrderResponse::GetSwatOrderType() const
{
	return GetSwatOrderTypeForPed(*GetPed());
}

bool CTaskHeliOrderResponse::HasPedsWaitingToBeDroppedOff() const
{
	//Ensure the heli is valid.
	const CHeli* pHeli = GetHeli();
	if(!pHeli)
	{
		return false;
	}

	//Iterate over the seats.
	for(int i = 0; i < pHeli->GetSeatManager()->GetMaxSeats(); i++)
	{
		//Ensure the ped is valid.
		const CPed* pPed = pHeli->GetSeatManager()->GetPedInSeat(i);
		if(!pPed)
		{
			continue;
		}

		//Check if the ped is waiting to be dropped off.
		if(IsPedWaitingToBeDroppedOff(*pPed))
		{
			return true;
		}
	}

	// Check if we have any scheduled peds that haven't spawned yet
	if( pHeli->HasScheduledOccupants())
	{
		return true;
	}

	return false;
}

bool CTaskHeliOrderResponse::IsWaitingToBeDroppedOff() const
{
	return IsPedWaitingToBeDroppedOff(*GetPed());
}

const CSwatOrder* CTaskHeliOrderResponse::GetHeliDriverSwatOrder()
{
	const CHeli* pHeli = GetHeli();
	CPed* pDriver = pHeli ? pHeli->GetDriver() : NULL;
	if(pDriver)
	{
		COrder* pDriverOrder = pDriver->GetPedIntelligence()->GetOrder();
		CSwatOrder* pDriverSwatOrder = pDriverOrder && pDriverOrder->GetType() == COrder::OT_Swat ? static_cast<CSwatOrder*>(pDriverOrder) : NULL;
		return pDriverSwatOrder;
	}

	return NULL;
}

int CTaskHeliOrderResponse::GetSwatOrderTypeForPed(const CPed& rPed)
{
	//Ensure the order is valid.
	const COrder* pOrder = rPed.GetPedIntelligence()->GetOrder();
	if(!pOrder || (pOrder->GetType() != COrder::OT_Swat))
	{
		return CSwatOrder::SO_NONE;
	}

	//Get the order type.
	const CSwatOrder* pSwatOrder = static_cast<const CSwatOrder *>(pOrder);
	return pSwatOrder->GetSwatOrderType();
}

bool CTaskHeliOrderResponse::IsPedWaitingToBeDroppedOff(const CPed& rPed)
{
	//Ensure the ped is not injured.
	if(rPed.IsInjured())
	{
		return false;
	}

	//Ensure the ped is in a vehicle.
	if(!rPed.GetIsInVehicle())
	{
		return false;
	}

	//Ensure the order type is valid.
	int iSwatOrderType = GetSwatOrderTypeForPed(rPed);
	if( iSwatOrderType != CSwatOrder::SO_WANTED_GOTO_DROP_OFF_LOCATION_AS_PASSENGER && 
		iSwatOrderType != CSwatOrder::SO_WANTED_GOTO_ABSEILING_LOCATION_AS_PASSENGER )
	{
		return false;
	}

	return true;
}

#define INITIAL_HELI_APPROACH_DIST 30.0f
#define SEARCH_DISTANCE_RATIO_MIN .7f
#define SEARCH_DISTANCE_RATIO_MAX .85f

// PURPOSE: Find the position that we want the helicopter to go to, generally an offset of the staging location
void CTaskHeliOrderResponse::FindTargetPosition(CPed* pPed, const COrder* pOrder, Vector3& vTargetPosition, bool bIsSearch)
{
	// Get our helicopter and turn the siren on
	CHeli* pHeli = static_cast<CHeli*>(pPed->GetMyVehicle());
	pHeli->TurnSirenOn(true);

	//If this is a search, try to generate a random search position.
	if(bIsSearch && m_pTarget && CDispatchHelperSearchInHeli::Calculate(*pPed, *m_pTarget, RC_VEC3V(vTargetPosition)))
	{
		//Nothing else to do.
		return;
	}
	else
	{
		//Fall back to the old behavior.
	}

	// Set our offset and if this is a search get the offset using the wanted radius as a base
	float fOffset = INITIAL_HELI_APPROACH_DIST;
	if(bIsSearch && m_pTarget && m_pTarget->IsPlayer())
	{
		fOffset = 100.0f * fwRandom::GetRandomNumberInRange(SEARCH_DISTANCE_RATIO_MIN, SEARCH_DISTANCE_RATIO_MAX);
	}

	// Get our staging location
	// Then we'll get the direction from us to the target and rotate that randomly a bit and apply the offset distance
	vTargetPosition = static_cast<const CSwatOrder*>(pOrder)->GetStagingLocation();

	if(bIsSearch || (static_cast<const CSwatOrder*>(pOrder)->GetSwatOrderType() == CSwatOrder::SO_WANTED_GOTO_STAGING_LOCATION_AS_DRIVER))
	{
		Vector3 vToTarget = vTargetPosition - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		vToTarget.z = 0.0f;
		vToTarget.Normalize();
		vToTarget.RotateZ(fwRandom::GetRandomNumberInRange(-QUARTER_PI, QUARTER_PI));
		vTargetPosition = vTargetPosition + (vToTarget * fOffset);
	}
}

// PURPOSE: Check if we need to update our location and do so if need be
void CTaskHeliOrderResponse::CheckForLocationUpdate(CPed* pPed)
{
	// Get the swat order
	CSwatOrder* pSwatOrder = static_cast<CSwatOrder*>(pPed->GetPedIntelligence()->GetOrder());

	// If  we need to update the location
	if (pSwatOrder && pSwatOrder->GetNeedsToUpdateLocation())
	{
		// Verify we are in the control task
		if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_CONTROL_VEHICLE)
		{
			// Get our vehicle task from the control task and verify it's a heli task
			aiTask* pTask = static_cast<CTaskControlVehicle*>(GetSubTask())->GetVehicleTask();
			if (pTask && pTask->GetTaskType() == CTaskTypes::TASK_VEHICLE_GOTO_HELICOPTER)
			{
				// re-calculate the target position and feed it to the heli task
				FindTargetPosition(pPed, pSwatOrder, m_GoToLocation, false);
				static_cast<CTaskVehicleGoToHelicopter*>(pTask)->SetTargetPosition(&m_GoToLocation);
				pSwatOrder->SetNeedsToUpdateLocation(false);
			}
		}
	}
}

// PURPOSE: Check if there are any passengers in the seats or anybody on the ropes of the heli
bool CTaskHeliOrderResponse::HasRappellingPeds() const
{
	const CPed* pPed = GetPed();

	// Make sure we have a vehicle that is a heli
	CVehicle* pVehicle = pPed->GetMyVehicle();
	if(!pVehicle || !pVehicle->InheritsFromHeli())
	{
		return false;
	}

	CHeli* pHeli = static_cast<CHeli*>(pVehicle);
	if(pHeli->AreRopesAttachedToNonHeliEntity())
	{
		return true;
	}

	const CSeatManager* pSeatManager = pHeli->GetSeatManager();
	//@@: location CTASKHELIORDERRESPONSE_HASREPELLEINGPEDS_GET_MAX_SEATS
	int iSeats = pHeli->GetSeatManager()->GetMaxSeats();

	for(int i = 1; i < iSeats; i++)
	{
		CPed* pPassenger = pSeatManager->GetPedInSeat(i);
		if(pPassenger && pPassenger->GetPedResetFlag(CPED_RESET_FLAG_IsRappelling))
		{
			return true;
		}
	}

	// Go through our group and check if any group members are directly attached to the heli
	CPedGroup* pMyGroup = pPed->GetPedsGroup();
	const CPedGroupMembership* pGroupMembership = pMyGroup ? pMyGroup->GetGroupMembership() : NULL;
	if(pGroupMembership)
	{
		for (int i = 0; i < CPedGroupMembership::MAX_NUM_MEMBERS; i++)
		{
			const CPed* pPedMember = pGroupMembership->GetMember(i);
			if (pPedMember && pPedMember != pPed && pPedMember->GetPedResetFlag(CPED_RESET_FLAG_IsRappelling))
			{
				return true;
			}
		}
	}

	return false;
}

void CTaskHeliOrderResponse::GoToStagingAreaAsDriver_OnEnter()
{
	m_fTimeAfterDropOff = 0.0f;
	m_fTimeOrderChangeHasBeenValid = 0.0f;

	CPed* pPed = GetPed();
	const COrder* pOrder = pPed->GetPedIntelligence()->GetOrder();

	if(pOrder && aiVerifyf(pPed->GetIsInVehicle(), "GoToIncidentLocationInVehicle_OnEnter: Not in vehicle!"))
	{
		aiAssert(pPed->GetMyVehicle() && pPed->GetMyVehicle()->InheritsFromHeli());
		
		// Calculate what position we want to fly to
		FindTargetPosition(pPed, pOrder, m_GoToLocation, false);
		int iDesiredHeight = 40;
		s32 iHeliFlags = 0;
		float fFlyingAvoidanceScalar = 0.0f;
		
		float fCruiseSpeed = s_fHeliCruiseSpeed;
		fCruiseSpeed *= pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatHeliSpeedModifier);

		// Create our sub task. In this order response we don't want the helicopter to land and we want the distance to finish check to be done against
		// the desired height, not the given height as it doesn't represent what height we are allowed to fly at
		sVehicleMissionParams params;
		params.m_fCruiseSpeed = fCruiseSpeed;
		params.SetTargetPosition(m_GoToLocation);
		
		if(static_cast<const CSwatOrder*>(pOrder)->GetSwatOrderType() == CSwatOrder::SO_WANTED_GOTO_ABSEILING_LOCATION_AS_DRIVER)
		{
			//Vector3 vToLocation = m_GoToLocation - VEC3V_TO_VECTOR3(pPed->GetMyVehicle()->GetTransform().GetPosition());
			params.m_iDrivingFlags = DMode_StopForCars | DF_DontTerminateTaskWhenAchieved;
			iDesiredHeight = 30;
		}
		else
		{
			static_cast<CHeli*>(pPed->GetMyVehicle())->SetTurbulenceScalar(0.1f);

			if(static_cast<const CSwatOrder*>(pOrder)->GetSwatOrderType() == CSwatOrder::SO_WANTED_GOTO_DROP_OFF_LOCATION_AS_DRIVER)
			{
				params.m_iDrivingFlags = DF_DontTerminateTaskWhenAchieved;
				iHeliFlags = CTaskVehicleGoToHelicopter::HF_LandOnArrival;
			}
			else
			{
				fFlyingAvoidanceScalar = 1.0f;
			}
		}

		CTaskVehicleGoToHelicopter *pHeliTask = rage_new CTaskVehicleGoToHelicopter( params, iHeliFlags, -1.0f, iDesiredHeight);
		pHeliTask->SetFlyingVehicleAvoidanceScalar(fFlyingAvoidanceScalar);
		SetNewTask(rage_new CTaskControlVehicle(pPed->GetMyVehicle(), pHeliTask));
	}
}

CTask::FSM_Return CTaskHeliOrderResponse::GoToStagingAreaAsDriver_OnUpdate()
{
	CPed* pPed = GetPed();
	if (!pPed->GetIsInVehicle() || !pPed->GetVehiclePedInside()->InheritsFromHeli())
	{
		SetState(State_Finish);
		return FSM_Continue;
	}

	CheckForLocationUpdate(pPed);

	//Check if we no longer have peds waiting to be dropped off, or a timeout has occurred.
	m_fTimeAfterDropOff = HasPedsWaitingToBeDroppedOff() ? 0.0f : m_fTimeAfterDropOff + GetTimeStep();

	int iSwatOrderType = GetSwatOrderType();

	//Check if the sub-task is invalid, or finished.
	CHeli* pHeli = static_cast<CHeli*>(pPed->GetMyVehicle());
	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Check if we are dropping off peds.
		if(iSwatOrderType == CSwatOrder::SO_WANTED_GOTO_DROP_OFF_LOCATION_AS_DRIVER)
		{
			if( m_fTimeAfterDropOff > sm_Tunables.m_MaxTimeAfterDropOffBeforeFlee ||
				pHeli->GetTimeSpentLanded() > sm_Tunables.m_MaxTimeSpentLandedBeforeFlee)
			{
				//Flee the scene.
				SetState(State_DriverFlee);
				return FSM_Continue;
			}
		}
		else
		{
			//Search the wanted area.
			SetState(State_SearchWantedAreaAsDriver);
			return FSM_Continue;
		}
	}

	static dev_float s_fMinTimeLandedForWreckedExit = 0.5f;
	if( pHeli->GetTimeSpentLanded() > s_fMinTimeLandedForWreckedExit &&
		(pHeli->GetRearRotorHealth() <= 0.0f || pHeli->GetIsMainRotorBroken() || pHeli->GetStatus() == STATUS_WRECKED || !pHeli->IsInDriveableState()) )
	{
		SetState(State_ExitVehicle);
		return FSM_Continue;
	}

	// If we are not landed and are colliding with something for too long we should exit
	if(pHeli->GetTimeSpentCollidingNotLanded() > sm_Tunables.m_MaxTimeCollidingBeforeExit)
	{
		//Exit the vehicle.
		SetState(State_ExitVehicle);
		return FSM_Continue;
	}

	// If we have no more peds in the heli or attached to the heli ropes we want to leave
	if(!ArePedsInOrAttachedToHeli(pHeli))
	{
		m_fTimeOrderChangeHasBeenValid = 0.0f;

		if( iSwatOrderType != CSwatOrder::SO_WANTED_GOTO_DROP_OFF_LOCATION_AS_DRIVER ||
			m_fTimeAfterDropOff > sm_Tunables.m_MaxTimeAfterDropOffBeforeFlee )
		{
			SetState(State_DriverFlee);
			return FSM_Continue;
		}
	}
	else if( pHeli->GetTimeSpentLanded() <= 0.0f && 
			iSwatOrderType == CSwatOrder::SO_WANTED_GOTO_DROP_OFF_LOCATION_AS_DRIVER &&
			pHeli->GetHeliIntelligence()->IsLandingAreaBlocked() )
	{
		m_fTimeOrderChangeHasBeenValid += GetTimeStep();
		if(m_fTimeOrderChangeHasBeenValid > sm_Tunables.m_MinTimeBeforeOrderChangeDueToBlockedLocation)
		{
			CSwatOrder* pOrder = static_cast<CSwatOrder*>(pPed->GetPedIntelligence()->GetOrder());
			pOrder->SetSwatOrderType(CSwatOrder::SO_WANTED_GOTO_ABSEILING_LOCATION_AS_DRIVER);
			SetFlag(aiTaskFlags::RestartCurrentState);
			return FSM_Continue;
		}
	}
	else
	{
		m_fTimeOrderChangeHasBeenValid = 0.0f;
	}

	//Activate timeslicing.
	ActivateTimeslicing();

	return FSM_Continue;
}

void CTaskHeliOrderResponse::GoToStagingAreaAsDriver_OnExit()
{
	CPed* pPed = GetPed();
	if(pPed->GetMyVehicle() && pPed->GetMyVehicle()->InheritsFromHeli())
	{
		static_cast<CHeli*>(pPed->GetMyVehicle())->SetTurbulenceScalar(1.0f);
	}

	if(!pPed->IsDead())
	{
		COrder* pOrder = pPed->GetPedIntelligence()->GetOrder();
		if(pOrder && !pOrder->GetEntityArrivedAtIncident())
		{
			Vec3V vPedToStagingLocation = pPed->GetTransform().GetPosition() - VECTOR3_TO_VEC3V(m_GoToLocation);
			vPedToStagingLocation.SetZ(V_ZERO);
			ScalarV scMinDistToArriveSq = LoadScalar32IntoScalarV(square(pPed->GetPedIntelligence()->GetPedPerception().GetSenseRange()));
			if(IsLessThanAll(MagSquared(vPedToStagingLocation), scMinDistToArriveSq))
			{
				pOrder->SetEntityArrivedAtIncident();
			}
		}
	}
}

bool CTaskHeliOrderResponse::ArePedsInOrAttachedToHeli(CHeli* pHeli)
{
	// If our heli isn't valid then we probably don't have any peds attached
	if(!pHeli)
	{
		return false;
	}

	// Check if there are any peds other than the driver
	if(pHeli->GetSeatManager()->CountPedsInSeats(false))
	{
		return true;
	}

	// Check if any peds are attached to our ropes
	if(pHeli->AreRopesAttachedToNonHeliEntity())
	{
		return true;
	}

	// Go through our group and check if any group members are directly attached to the heli
	CPed* pPed = GetPed();
	CPedGroup* pMyGroup = pPed->GetPedsGroup();
	const CPedGroupMembership* pGroupMembership = pMyGroup ? pMyGroup->GetGroupMembership() : NULL;
	if(pGroupMembership)
	{
		for (int i = 0; i < CPedGroupMembership::MAX_NUM_MEMBERS; i++)
		{
			const CPed* pPedMember = pGroupMembership->GetMember(i);
			if (pPedMember && pPedMember != pPed && pPedMember->GetAttachParent() == pHeli)
			{
				return true;
			}
		}
	}

	// Check if we have any scheduled peds that haven't spawned yet
	if( pHeli->HasScheduledOccupants())
	{
		return true;
	}

	return false;
}

void CTaskHeliOrderResponse::DriverFlee_OnEnter()
{
	//Let the heli be deleted with alive peds in it.
	GetPed()->GetMyVehicle()->m_nVehicleFlags.bCanBeDeletedWithAlivePedsInIt = true;

	//Calculate the target.
	CAITarget target;
	if(m_pTarget)
	{
		target.SetEntity(m_pTarget);
	}
	else
	{
		target.SetPosition(VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition()));
	}

	//Create the task.
	CTask* pTask = CHeliFleeHelper::CreateTaskForPed(*GetPed(), target);
	taskAssert(pTask);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskHeliOrderResponse::DriverFlee_OnUpdate()
{
	//Check if the sub-task has finished.
	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Finish);
	}
	else
	{
		//Activate timeslicing.
		ActivateTimeslicing();
	
		// If we are not landed and are colliding with something for too long we should exit
		if(GetPed()->GetIsInVehicle() && GetPed()->GetVehiclePedInside()->InheritsFromHeli())
		{
			CHeli* pHeli = static_cast<CHeli*>(GetPed()->GetMyVehicle());
			if(pHeli->GetTimeSpentCollidingNotLanded() > sm_Tunables.m_MaxTimeCollidingBeforeExit)
			{
				//Exit the vehicle.
				SetState(State_ExitVehicle);
			}
		}
	}

	return FSM_Continue;
}

void CTaskHeliOrderResponse::DriverFlee_OnExit()
{
	CVehicle* pVehicle = GetPed()->GetMyVehicle();
	if(pVehicle)
	{
		pVehicle->m_nVehicleFlags.bCanBeDeletedWithAlivePedsInIt = false;
	}
}

void CTaskHeliOrderResponse::PlayEntryAnim_OnEnter()
{
	s32 iSeatIndex = GetPed()->GetMyVehicle()->GetSeatManager()->GetPedsSeatIndex(GetPed());
	fwMvClipSetId entryAnimClipSetId = (iSeatIndex == Seat_backLeft) ? CLIP_SET_VEH_HELI_REAR_DESCEND_ENTER_DS : CLIP_SET_VEH_HELI_REAR_DESCEND_ENTER_PS;
	const fwMvClipId entryAnimHash("get_in_extra", 0x8F639D6B);
	if (taskVerifyf(fwAnimManager::GetClipIfExistsBySetId(entryAnimClipSetId, entryAnimHash), "Clip not found"))
	{
		StartClipBySetAndClip(GetPed(), entryAnimClipSetId, entryAnimHash, APF_ISFINISHAUTOREMOVE, (u32)BONEMASK_ALL, AP_HIGH, INSTANT_BLEND_IN_DELTA, CClipHelper::TerminationType_OnFinish);
	}
}

CTask::FSM_Return CTaskHeliOrderResponse::PlayEntryAnim_OnUpdate()
{
	if(!GetClipHelper() || !GetClipHelper()->IsPlayingClip())
	{
		SetState(State_GoToStagingAreaAsPassenger);
	}

	return FSM_Continue;
}

void CTaskHeliOrderResponse::GoToStagingAreaAsPassenger_OnEnter()
{
	CPed* pPed = GetPed();
	const COrder* pOrder = pPed->GetPedIntelligence()->GetOrder();

	if( m_pTarget && pOrder && pOrder->GetType() == COrder::OT_Swat &&
		static_cast<const CSwatOrder*>(pOrder)->GetSwatOrderType() == CSwatOrder::SO_WANTED_GOTO_DROP_OFF_LOCATION_AS_PASSENGER )
	{
		CAITarget target(m_pTarget);
		SetNewTask(rage_new CTaskVehicleGun(CTaskVehicleGun::Mode_Fire, ATSTRINGHASH("FIRING_PATTERN_BURST_FIRE_DRIVEBY", 0x0d31265f2), &target));
	}
}

CTask::FSM_Return CTaskHeliOrderResponse::GoToStagingAreaAsPassenger_OnUpdate()
{
	CPed* pPed = GetPed();
	if (!pPed->GetIsInVehicle() || !pPed->GetVehiclePedInside()->InheritsFromHeli())
	{
		SetState(State_Finish);
		return FSM_Continue;
	}

	// Get our ped's order and verify that it's a swat passenger rappel order
	COrder* pOrder = pPed->GetPedIntelligence()->GetOrder();
	if(pOrder && pOrder->GetType() == COrder::OT_Swat)
	{
		const CSwatOrder* pDriverSwatOrder = GetHeliDriverSwatOrder();
		CSwatOrder* pSwatOrder = static_cast<CSwatOrder*>(pOrder);
		if(pDriverSwatOrder)
		{
			pSwatOrder->SetStagingLocation(pDriverSwatOrder->GetStagingLocation());
		}

		if(pSwatOrder->GetSwatOrderType() == CSwatOrder::SO_WANTED_GOTO_ABSEILING_LOCATION_AS_PASSENGER)
		{
			// Check the distance between our ped and the desired location for the order, if close enough start the rappel state
			Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			vPedPosition.z = pSwatOrder->GetStagingLocation().z;
			if(pSwatOrder->GetStagingLocation().Dist2(vPedPosition) < PASSENGER_START_RAPPEL_DIST2)
			{
				SetState(State_PassengerRappel);
				return FSM_Continue;
			}
		}
		else if( pSwatOrder->GetSwatOrderType() == CSwatOrder::SO_WANTED_GOTO_DROP_OFF_LOCATION_AS_PASSENGER &&
				 pDriverSwatOrder && pDriverSwatOrder->GetSwatOrderType() == CSwatOrder::SO_WANTED_GOTO_ABSEILING_LOCATION_AS_DRIVER )
		{
			m_bOverrideRopeLength = true;
			pSwatOrder->SetSwatOrderType(CSwatOrder::SO_WANTED_GOTO_ABSEILING_LOCATION_AS_PASSENGER);
			SetFlag(aiTaskFlags::RestartCurrentState);
			return FSM_Continue;
		}
	}

	CHeli* pHeli = static_cast<CHeli*>(pPed->GetMyVehicle());

	//Check if we have spent some time landed.
	float fMinTimeSpentLandedBeforeExit = sm_Tunables.m_MinTimeSpentLandedBeforeExit;
	float fMaxTimeSpentLandedBeforeExit = sm_Tunables.m_MaxTimeSpentLandedBeforeExit;
	float fTimeSpentLandedBeforeExit = pPed->GetRandomNumberInRangeFromSeed(fMinTimeSpentLandedBeforeExit, fMaxTimeSpentLandedBeforeExit);
	if( pHeli->GetTimeSpentLanded() > fTimeSpentLandedBeforeExit ||
		pHeli->GetTimeSpentCollidingNotLanded() > sm_Tunables.m_MaxTimeCollidingBeforeExit )
	{
		//Exit the vehicle.
		SetState(State_ExitVehicle);
		return FSM_Continue;
	}

	//Activate timeslicing.
	ActivateTimeslicing();

	return FSM_Continue;
}

void CTaskHeliOrderResponse::PassengerRappel_OnEnter()
{
	// Just call into our passenger rappelling task as it will handle exiting, rope creation, landing, etc
	SetNewTask(rage_new CTaskHeliPassengerRappel(10.0f, m_bOverrideRopeLength ? 30.0f : -1.0f));
}

CTask::FSM_Return CTaskHeliOrderResponse::PassengerRappel_OnUpdate()
{
	// Request our aim intros that are likely to be used here
	m_aimIntroClipRequestHelper.Request(CLIP_SET_COMBAT_REACTIONS_RIFLE_TURN);

	// Passengers should try and move to their target once they've landed on the ground
	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		if(m_pTarget)
		{
			if(CanSeeTarget())
			{
				SetState(State_PassengerCombatTarget);
			}
			else
			{
				SetState(State_PassengerMoveToTarget);
			}
		}
		else
		{
			SetState(State_Finish);
		}
	}

	return FSM_Continue;
}

void CTaskHeliOrderResponse::ExitVehicle_OnEnter()
{
	//Grab the ped.
	CPed* pPed = GetPed();

	//Don't enter the leader's vehicle.
	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_DontEnterLeadersVehicle, true);

	//Ensure the vehicle is valid.
	CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
	if(!pVehicle)
	{
		return;
	}

	//Create the task.
	VehicleEnterExitFlags flags;
	CTask* pTask = rage_new CTaskExitVehicle(pVehicle, flags);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskHeliOrderResponse::ExitVehicle_OnUpdate()
{
	//Check if the sub-task has finished.
	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Move to the target.
		SetState(State_PassengerMoveToTarget);
	}

	return FSM_Continue;
}

void CTaskHeliOrderResponse::PassengerMoveToTarget_OnEnter()
{
	// The task can have an invalid target but this state requires one so do a check on it
	if(m_pTarget)
	{
		// Mix up the blend ratio a bit since these guys won't be using their line formation and we want it to look more natural
		float fMoveBlendRatio = fwRandom::GetRandomNumberInRange(MOVEBLENDRATIO_RUN - 0.15f, MOVEBLENDRATIO_RUN + 0.15f);
		CTaskMoveFollowNavMesh* pNavTask = rage_new CTaskMoveFollowNavMesh(fMoveBlendRatio, VEC3V_TO_VECTOR3(m_pTarget->GetTransform().GetPosition()), s_fMaxDistToPlayer);
		// Only use larger search extents if necessary - this has the potential to overload the pathfinder if a route is not found, since pathsearch is exhaustive
		pNavTask->SetUseLargerSearchExtents( (VEC3V_TO_VECTOR3(m_pTarget->GetTransform().GetPosition()) - VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition())).Mag2() > square(CPathServer::GetDefaultMaxPathFindDistFromOrigin()-10.0f) );
		CTaskComplexControlMovement* pTask = rage_new CTaskComplexControlMovement( pNavTask, rage_new CTaskAimSweep(CTaskAimSweep::Aim_Forward, true));
		pTask->SetAllowClimbLadderSubtask(true);
		SetNewTask(pTask);
	}
}

CTask::FSM_Return CTaskHeliOrderResponse::PassengerMoveToTarget_OnUpdate()
{
	//Check if we can see the target.
	if(CanSeeTarget())
	{
		SetState(State_PassengerCombatTarget);
	}
	//Check if the sub-task has finished.
	else if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Check if the target's whereabouts are known.
		if(AreTargetsWhereaboutsKnown())
		{
			SetState(State_PassengerCombatTarget);
		}
		else
		{
			SetState(State_FindPositionToSearchOnFoot);
		}
	}

	return FSM_Continue;
}

void CTaskHeliOrderResponse::PassengerCombatTarget_OnEnter()
{
	//Ensure the target is valid.
	const CPed* pTarget = m_pTarget;
	if(!pTarget)
	{
		return;
	}

	//Create the task.
	CTaskThreatResponse* pTask = rage_new CTaskThreatResponse(pTarget);
	pTask->SetThreatResponseOverride(CTaskThreatResponse::TR_Fight);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskHeliOrderResponse::PassengerCombatTarget_OnUpdate()
{
	//Check if the target's whereabouts are unknown.
	if(!AreTargetsWhereaboutsKnown())
	{
		//Search for the target.
		SetState(State_FindPositionToSearchOnFoot);
	}
	//Check if the sub-task has finished.
	else if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Finish the task.
		SetState(State_Finish);
	}

	return FSM_Continue;
}

void CTaskHeliOrderResponse::FindPositionToSearchOnFoot_OnEnter()
{
	//Ensure the target is valid.
	const CPed* pTarget = m_pTarget;
	if(!pTarget)
	{
		return;
	}

	//Start the search.
	m_HelperForSearchOnFoot.Reset();
	if(!m_HelperForSearchOnFoot.Start(*GetPed(), *pTarget))
	{
		return;
	}
}

CTask::FSM_Return CTaskHeliOrderResponse::FindPositionToSearchOnFoot_OnUpdate()
{
	//Check if the search did not start.
	if(!m_HelperForSearchOnFoot.HasStarted())
	{
		if(GetTimeInState() > 1.0f)
		{
			SetFlag(aiTaskFlags::RestartCurrentState);
		}
	}
	else
	{
		//Check if the results are ready.
		bool bWasSuccessful;
		if(m_HelperForSearchOnFoot.IsResultReady(bWasSuccessful, m_vPositionToSearchOnFoot))
		{
			//Check if the search was successful.
			if(bWasSuccessful)
			{
				SetState(State_SearchOnFoot);
			}
			else
			{
				SetFlag(aiTaskFlags::RestartCurrentState);
			}
		}
	}

	return FSM_Continue;
}

void CTaskHeliOrderResponse::SearchOnFoot_OnEnter()
{
	//Assert that the position is valid.
	taskAssert(!IsCloseAll(m_vPositionToSearchOnFoot, Vec3V(V_ZERO), ScalarVFromF32(SMALL_FLOAT)));

	//Create the move task.
	CTaskMoveFollowNavMesh* pMoveTask = rage_new CTaskMoveFollowNavMesh(CDispatchHelperSearchOnFoot::GetMoveBlendRatio(*GetPed()),
		VEC3V_TO_VECTOR3(m_vPositionToSearchOnFoot), CTaskMoveFollowNavMesh::ms_fTargetRadius, CTaskMoveFollowNavMesh::ms_fSlowDownDistance, -1, true, false, NULL);

	//Quit the task if a route is not found.
	pMoveTask->SetQuitTaskIfRouteNotFound(true);

	//Equip the best weapon.
	if(GetPed()->GetWeaponManager())
	{
		GetPed()->GetWeaponManager()->EquipBestWeapon();
	}

	//Create the sub-task.
	CTask* pSubTask = CEquipWeaponHelper::ShouldPedSwapWeapon(*GetPed()) ?
		rage_new CTaskSwapWeapon(SWAP_HOLSTER | SWAP_DRAW) : NULL;

	//Create the task.
	CTaskComplexControlMovement* pTask = rage_new CTaskComplexControlMovement(pMoveTask, pSubTask);
	pTask->SetAllowClimbLadderSubtask(true);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskHeliOrderResponse::SearchOnFoot_OnUpdate()
{
	//Check if the target's whereabouts are known.
	if(AreTargetsWhereaboutsKnown())
	{
		SetState(State_PassengerCombatTarget);
	}
	//Check if the sub-task has finished.
	else if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_FindPositionToSearchOnFoot);
	}

	return FSM_Continue;
}

void CTaskHeliOrderResponse::SearchWantedAreaAsDriver_OnEnter()
{
	CPed* pPed = GetPed();
	const COrder* pOrder = GetPed()->GetPedIntelligence()->GetOrder();
	if(pOrder && aiVerifyf(pPed->GetIsInVehicle(), "GoToIncidentLocationInVehicle_OnEnter: Not in vehicle!"))
	{
		aiAssert(pPed->GetMyVehicle() && pPed->GetMyVehicle()->InheritsFromHeli());

		FindTargetPosition(pPed, pOrder, m_GoToLocation, true);
		
		float fCruiseSpeed = s_fHeliCruiseSpeed;
		fCruiseSpeed *= pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatHeliSpeedModifier);

		// Create our sub task. In this order response we don't want the helicopter to land and we want the distance to finish check to be done against
		// the desired height, not the given height as it doesn't represent what height we are allowed to fly at
		sVehicleMissionParams params;
		params.m_fCruiseSpeed = fCruiseSpeed;
		params.SetTargetPosition(m_GoToLocation);

		CTaskVehicleGoToHelicopter *pHeliTask = rage_new CTaskVehicleGoToHelicopter( params, 0, -1.0f, 40);
		SetNewTask(rage_new CTaskControlVehicle(pPed->GetMyVehicle(), pHeliTask));
	}
}

CTask::FSM_Return CTaskHeliOrderResponse::SearchWantedAreaAsDriver_OnUpdate()
{
	CPed* pPed = GetPed();
	if (!pPed->GetIsInVehicle())
	{
		SetState(State_Finish);
		return FSM_Continue;
	}

	CheckForLocationUpdate(pPed);

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetFlag(aiTaskFlags::RestartCurrentState);
		return FSM_Continue;
	}

	//Check if the heli needs to refuel.
	if(m_pTarget && CHeliRefuelHelper::Process(*GetPed(), *m_pTarget))
	{
		SetState(State_DriverFlee);
	}

	//Activate timeslicing.
	ActivateTimeslicing();

	return FSM_Continue;
}

#if !__FINAL
const char * CTaskHeliOrderResponse::GetStaticStateName( s32 iState  )
{
	taskAssert(iState >= State_Start && iState <= State_Finish);

	switch (iState)
	{
		case State_Start:							return "State_Start";
		case State_GoToStagingAreaAsDriver:			return "State_GoToStagingAreaAsDriver";
		case State_DriverFlee:						return "State_DriverFlee";
		case State_PlayEntryAnim:					return "State_PlayEntryAnim";
		case State_GoToStagingAreaAsPassenger:		return "State_GoToStagingAreaAsPassenger";
		case State_PassengerRappel:					return "State_PassengerRappel";
		case State_ExitVehicle:						return "State_ExitVehicle";
		case State_PassengerMoveToTarget:			return "State_PassengerMoveToTarget";
		case State_PassengerCombatTarget:			return "State_PassengerCombatTarget";
		case State_FindPositionToSearchOnFoot:		return "State_FindPositionToSearchOnFoot";
		case State_SearchOnFoot:					return "State_SearchOnFoot";
		case State_SearchWantedAreaAsDriver:		return "State_SearchWantedAreaAsDriver";
		case State_Finish:							return "State_Finish";
		default: taskAssert(0); 	
	}

	return "State_Invalid";
}
#endif // !__FINAL
