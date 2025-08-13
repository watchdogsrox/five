// FILE :    TaskSearch.h
// PURPOSE : AI scour an area based on the last known position of the target and try to re-aquire.
//			 They can search using vehicles, on foot and transition between the two.
// AUTHOR :  Phil H

// File header
#include "Task/Combat/Subtasks/TaskVehicleCombat.h"

// Rage headers
#include "grcore/debugdraw.h"

// Game headers
#include "audio/vehicleaudioentity.h"
#include "audio/caraudioentity.h"
#include "Network/Cloud/Tunables.h"
#include "Objects/Door.h"
#include "PedGroup/PedGroup.h"
#include "Peds/Ped.h"
#include "peds/PedIKSettings.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PedWeapons/PedTargeting.h"
#include "Peds/VehicleCombatAvoidanceArea.h"
#include "Task/Vehicle/TaskInVehicle.h"
#include "Task/Vehicle/TaskVehicleWeapon.h"
#include "Task/Combat/TaskCombat.h"
#include "Task/Combat/Subtasks/TaskBoatCombat.h"
#include "Task/Combat/Subtasks/TaskHeliCombat.h"
#include "Task/Combat/Subtasks/TaskShootAtTarget.h"
#include "Task/Combat/Subtasks/TaskShootOutTire.h"
#include "Task/Combat/Subtasks/TaskSubmarineCombat.h"
#include "Task/Combat/Subtasks/TaskVehicleChase.h"
#include "Task/Service/Swat/TaskSwat.h"
#include "Task/Vehicle/TaskCar.h"
#include "Task/Vehicle/TaskExitVehicle.h"
#include "Task/Vehicle/TaskVehicleBase.h"
#include "vehicles/Boat.h"
#include "Vehicles/Heli.h"
#include "Vehicles/Metadata/AIHandlingInfo.h"
#include "Vehicles/Metadata/VehicleMetadataManager.h"
#include "Vehicles/Metadata/VehicleSeatInfo.h"
#include "vehicles/Train.h"
#include "vehicleAi/task/TaskVehicleApproach.h"
#include "vehicleAi/task/TaskVehicleAttack.h"
#include "vehicleAi/task/TaskVehicleFleeBoat.h"
#include "vehicleAi/task/TaskVehicleCruise.h"
#include "vehicleAi/task/TaskVehicleGoto.h"
#include "vehicleAi/task/TaskVehicleBlock.h"
#include "vehicleAi/task/TaskVehiclePark.h"
#include "vehicleAi/task/TaskVehicleTempAction.h"
#include "Weapons/Info/WeaponInfoManager.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

CTaskVehiclePersuit::Tunables CTaskVehiclePersuit::sm_Tunables;

IMPLEMENT_COMBAT_TASK_TUNABLES(CTaskVehiclePersuit, 0x50f150c4);

bank_float CTaskVehiclePersuit::EXIT_FAILED_TIME_IN_STATE = 2.0f;

CTaskVehiclePersuit::CTaskVehiclePersuit( const CAITarget& target, fwFlags8 uConfigFlags )
: m_vTargetPositionWhenWaitBegan(V_ZERO)
, m_vPositionWhenCollidedWithLockedDoor(V_ZERO)
, m_target(target)
, m_uTimeCollidedWithLockedDoor(0)
, m_uConfigFlags(uConfigFlags)
, m_uRunningFlags(0)
, m_fTimeVehicleUndriveable(0.0f)
, m_fJumpOutTimer(0.0f)
, m_ExitToAimClipSetId(CLIP_SET_ID_INVALID)
#if !__FINAL
, m_uNumObstructionPoints(0)
#endif
{
	taskFatalAssertf(target.GetEntity(), "CTaskVehiclePersuit requires a valid target entity!");
	taskFatalAssertf(target.GetEntity()->GetIsPhysical(), "CTaskVehiclePersuit requires a valid PHYSICAL target entity!");
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_PERSUIT);
}

CTaskVehiclePersuit::~CTaskVehiclePersuit()
{

}

CTask::FSM_Return CTaskVehiclePersuit::ProcessPreFSM()
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	// Exit the task immediately if the ped is not inside a vehicle
	if( pPed->GetVehiclePedInside() == NULL && MustPedBeInVehicle() )
	{
		return FSM_Quit;
	}

	// If the target becomes no longer valid - quit
	if( !m_target.GetIsValid() || m_target.GetEntity() == NULL )
	{
		return FSM_Quit;
	}

	//Process timers.
	ProcessTimers();

	//Process audio.
	ProcessAudio();

	ProcessIk();

	ProcessBlockedByLockedDoor();

	//Activate timeslicing.
	ActivateTimeslicing();

	return FSM_Continue;
}

CTask::FSM_Return CTaskVehiclePersuit::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		// initial state
		FSM_State(State_start)
			FSM_OnUpdate
				return Start_OnUpdate(pPed);

		// flee from the target rather than chase/attack
		FSM_State(State_fleeFromTarget)
			FSM_OnEnter
				FleeFromTarget_OnEnter(pPed);
			FSM_OnUpdate
				return FleeFromTarget_OnUpdate(pPed);

		FSM_State(State_WaitWhileCruising)
			FSM_OnEnter
				WaitWhileCruising_OnEnter();
			FSM_OnUpdate
				return WaitWhileCruising_OnUpdate();

		FSM_State(State_attack)
			FSM_OnEnter
				Attack_OnEnter();
			FSM_OnUpdate
				return Attack_OnUpdate();
				
		// Chase the target in a car
		FSM_State(State_chaseInCar)
			FSM_OnEnter
				ChaseInCar_OnEnter(pPed);
			FSM_OnUpdate
				return ChaseInCar_OnUpdate(pPed);
		
		// Follow the car at a distance, allowing passengers to shoot
		FSM_State(State_followInCar)
			FSM_OnEnter
				FollowInCar_OnEnter(pPed);
			FSM_OnUpdate
				return FollowInCar_OnUpdate(pPed);

		// Engage the target in combat in a heli
		FSM_State(State_heliCombat)
			FSM_OnEnter
				HeliCombat_OnEnter();
			FSM_OnUpdate
				return HeliCombat_OnUpdate();

		// Engage the target in combat in a sub
		FSM_State(State_subCombat)
			FSM_OnEnter
				SubCombat_OnEnter();
			FSM_OnUpdate
				return SubCombat_OnUpdate();
				
		// Engage the target in combat in a boat
		FSM_State(State_boatCombat)
			FSM_OnEnter
				BoatCombat_OnEnter();
			FSM_OnUpdate
				return BoatCombat_OnUpdate();

		// Ride as passenger - firing weapons if they are available
		FSM_State(State_passengerInVehicle)
			FSM_OnEnter
				PassengerInVehicle_OnEnter(pPed);
			FSM_OnUpdate
				return PassengerInVehicle_OnUpdate(pPed);

		// Wait for buddies to enter the car
		FSM_State(State_waitForBuddiesToEnter)
			FSM_OnEnter
				WaitForBuddiesToEnter_OnEnter(pPed);
			FSM_OnUpdate
				return WaitForBuddiesToEnter_OnUpdate(pPed);

		// Slow down until can use handbrake safely
		FSM_State(State_slowdownSafely)
			FSM_OnEnter
				SlowdownSafely_OnEnter(pPed);
			FSM_OnUpdate
				return SlowdownSafely_OnUpdate(pPed);
			
		// Ride as passenger - firing weapons if they are available
		FSM_State(State_emergencyStop)
			FSM_OnEnter
				EmergencyStop_OnEnter(pPed);
			FSM_OnUpdate
				return EmergencyStop_OnUpdate(pPed);
			FSM_OnExit
				EmergencyStop_OnExit();

		// Exit the vehicle
		FSM_State(State_exitVehicle)
			FSM_OnEnter
				ExitVehicle_OnEnter(pPed);
			FSM_OnUpdate
				return ExitVehicle_OnUpdate(pPed);

		// Exit the vehicle - without waiting for it to slow
		FSM_State(State_exitVehicleImmediately)
			FSM_OnEnter
				ExitVehicleImmediately_OnEnter(pPed);
			FSM_OnUpdate
				return ExitVehicleImmediately_OnUpdate(pPed);
				
		// Provide cover for others exiting the vehicle
		FSM_State(State_provideCoverDuringExitVehicle)
			FSM_OnEnter
				ProvideCoverDuringExitVehicle_OnEnter(pPed);
			FSM_OnUpdate
				return ProvideCoverDuringExitVehicle_OnUpdate(pPed);
			FSM_OnExit
				ProvideCoverDuringExitVehicle_OnExit(pPed);
				
		//Approach the target
		FSM_State(State_approachTarget)
			FSM_OnEnter
				ApproachTarget_OnEnter();
			FSM_OnUpdate
				return ApproachTarget_OnUpdate();

		//Shoot at the target while waiting for vehicle exit
		FSM_State(State_waitForVehicleExit)
			FSM_OnEnter
				WaitForVehicleExit_OnEnter();
			FSM_OnUpdate
				return WaitForVehicleExit_OnUpdate();

		FSM_State(state_waitInCar)
			FSM_OnEnter
				WaitInCar_OnEnter();
			FSM_OnUpdate
				return WaitInCar_OnUpdate();

		// Quit the task
		FSM_State(State_exit)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskVehiclePersuit::ProcessPostFSM()
{
	const CPed& ped = *GetPed();
	if (ped.GetIsInVehicle())
	{
		if (m_ExitToAimClipSetId.IsNotNull())
		{
			m_clipSetRequestHelper.Request(m_ExitToAimClipSetId);
		}
	}
	return FSM_Continue;
}

void CTaskVehiclePersuit::ProcessIk()
{
	//Ensure the target ped is valid.
	const CPed* pTargetPed = GetTargetPed();
	if(!pTargetPed)
	{
		return;
	}

	//Ensure a second has elapsed.
	if(floorf(GetTimeRunning()) == floorf(GetTimeRunning() - GetTimeStep()))
	{
		return;
	}

	//Look at the target.
	GetPed()->GetIkManager().LookAt(0, pTargetPed,
		2000, BONETAG_HEAD, NULL, LF_WHILE_NOT_IN_FOV, 500, 500, CIkManager::IK_LOOKAT_HIGH);
}

CTask::FSM_Return CTaskVehiclePersuit::Start_OnUpdate( CPed* pPed )
{
	taskAssertf(pPed->GetVehiclePedInside(), "Task started when not in a vehicle!");

	//Check if we are a law enforcement ped, and we are driving.
	if(pPed->IsLawEnforcementPed() && pPed->GetIsDrivingVehicle())
	{
		//Check if the vehicle is law enforcement.
		CVehicle* pVehicle = pPed->GetVehiclePedInside();
		if(pVehicle->IsLawEnforcementVehicle())
		{
			//Turn the siren on.
			pVehicle->TurnSirenOn(true, ChooseDefaultState() == State_chaseInCar);
		}
	}

	// Find out which clip set to request based on our equipped weapon or best weapon and then request it
	m_ExitToAimClipSetId = CTaskVehicleFSM::GetExitToAimClipSetIdForPed(*pPed);

	// Find out which clip set to request based on our equipped weapon or best weapon and then request it
	if(m_ExitToAimClipSetId != CLIP_SET_ID_INVALID)
	{
		CVehicle* pMyVehicle = pPed->GetMyVehicle();
		if (pMyVehicle)
		{
			int iSeatIndex = pMyVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);

			// Then check if we can exit the vehicle early. Right now we only want to do this when exiting the vehicle to aim
			const CVehicleSeatAnimInfo* pSeatAnimInfo = pMyVehicle->GetSeatAnimationInfo(iSeatIndex);
			if(pSeatAnimInfo && pSeatAnimInfo->GetCanExitEarlyInCombat())
			{
				m_uRunningFlags.SetFlag(RF_CanExitVehicleEarly);
			}
		}
	}

	//Check if we should exit immediately.
	if(m_uConfigFlags.IsFlagSet(CF_ExitImmediately))
	{
		SetState(State_exitVehicleImmediately);
		return FSM_Continue;
	}

	// If the ped is currently driving a vehicle, 
	if( pPed->GetIsDrivingVehicle() )
	{
		//Check if we should flee.
		if( pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_FleeWhilstInVehicle) )
		{
			SetState(State_fleeFromTarget);
		}
		else if( ShouldWaitWhileCruising() )
		{
			SetState(State_WaitWhileCruising);
		}
		else if( ShouldAttack() )
		{
			SetState(State_attack);
		}
		else if( pPed->GetVehiclePedInside()->GetIsRotaryAircraft() )
		{
			SetState(State_heliCombat);
		}
		else if( pPed->GetVehiclePedInside()->InheritsFromSubmarine() || pPed->GetVehiclePedInside()->InheritsFromSubmarineCar())
		{
			SetState(State_subCombat);
		}
		else if( pPed->GetVehiclePedInside()->InheritsFromBoat() )
		{
			SetState(State_boatCombat);
		}
		else if( pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_JustFollowInVehicle) )
		{
			SetState(State_followInCar);
		}
		else
		{
			SetState(ChooseDefaultState());
		}

		return FSM_Continue;
	}
	// The ped is a passenger, enter the passenger state
	else
	{
		SetState(State_passengerInVehicle);
		return FSM_Continue;
	}
}

void CTaskVehiclePersuit::FleeFromTarget_OnEnter( CPed* pPed )
{
	taskFatalAssertf(m_target.GetEntity(), "Null target passed!" );
	taskFatalAssertf(m_target.GetEntity()->GetIsPhysical(), "Target is non-physical!");
	//const CPhysical* pPhysicalTarget = static_cast<const CPhysical*>(m_target.GetEntity());

	sVehicleMissionParams params;
	params.SetTargetEntity(const_cast<CEntity*>(m_target.GetEntity()));
	params.m_iDrivingFlags = DMode_AvoidCars|DF_AvoidTargetCoors|DF_UseStringPullingAtJunctions|DF_SteerAroundPeds;
	params.m_fCruiseSpeed = pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatSpeedToFleeInVehicle);

	CTaskVehicleMissionBase* pFleeTask = NULL;
	if ( pPed->GetMyVehicle() && pPed->GetMyVehicle()->InheritsFromBoat() )
	{
		pFleeTask = rage_new CTaskVehicleFleeBoat(params);
	}
	else
	{
		pFleeTask = rage_new CTaskVehicleFlee(params);
	}


	SetNewTask(rage_new CTaskControlVehicle(pPed->GetMyVehicle(), pFleeTask, rage_new CTaskVehicleCombat(&m_target)));
}

CTask::FSM_Return CTaskVehiclePersuit::FleeFromTarget_OnUpdate( CPed* UNUSED_PARAM(pPed) )
{
	// If the subtask has terminated - restart
	if( GetIsFlagSet(aiTaskFlags::SubTaskFinished) )
	{
		SetFlag(aiTaskFlags::RestartCurrentState);
		return FSM_Continue;
	}
	return FSM_Continue;
}

//the following functions are mostly copied from TaskPolicePatrol in order to get non-police vehicles to crusie ahead of target vehicles
void CTaskVehiclePersuit::WaitWhileCruising_OnEnter()
{
	CPed* pPed = GetPed();

	sVehicleMissionParams params;
	params.m_iDrivingFlags = DMode_AvoidCars_Reckless | DF_AdjustCruiseSpeedBasedOnRoadSpeed;
	params.m_fCruiseSpeed = 10.0f;
	CTask* pTask = CVehicleIntelligence::CreateCruiseTask(*pPed->GetMyVehicle(), params);

	//Start the task.
	SetNewTask(rage_new CTaskControlVehicle(pPed->GetMyVehicle(), pTask, rage_new CTaskVehicleCombat(&m_target)));
}

bool CTaskVehiclePersuit::FacingSameDirectionAsTarget() const
{
	const CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
	const CVehicle* pTargetVehicle = GetTargetVehicle();
	if(!pTargetVehicle || !pVehicle)
	{
		return false;
	}

	Vec3V vOurForward = pVehicle->GetTransform().GetForward();
	Vec3V vTargetForward = pTargetVehicle->GetTransform().GetForward();

	//Check if the target has passed the officer.
	ScalarV scDot = Dot(vOurForward, vTargetForward);
	if(IsGreaterThanAll(scDot, ScalarV(V_ZERO)))
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool CTaskVehiclePersuit::HasTargetPassedUs() const
{
	const CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
	const CVehicle* pTargetVehicle = GetTargetVehicle();
	if(!pTargetVehicle || !pVehicle)
	{
		return false;
	}

	Vec3V vOurPosition = pVehicle->GetTransform().GetPosition();
	Vec3V vTargetPosition = pTargetVehicle->GetTransform().GetPosition();

	Vec3V vTargetToUs = Subtract(vOurPosition, vTargetPosition);
	Vec3V vTargetForward = pTargetVehicle->GetTransform().GetForward();

	//Check if the target has passed the officer.
	ScalarV scDot = Dot(vTargetForward, vTargetToUs);
	if(IsLessThanAll(scDot, ScalarV(V_ZERO)))
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool CTaskVehiclePersuit::WillTargetVehicleOvertakeUsWithinTime(float fTime) const
{
	const CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
	const CVehicle* pTargetVehicle = GetTargetVehicle();
	if(!pVehicle || pTargetVehicle)
	{
		return false;
	}

	float fVehicleSpeed = pVehicle->GetVelocity().XYMag();
	float fTargetVehicleSpeed = pTargetVehicle->GetVelocity().XYMag();

	//Ensure the target vehicle is moving faster than the officer.
	float fSpeedDifference = fTargetVehicleSpeed - fVehicleSpeed;
	if(fSpeedDifference <= 0.0f)
	{
		return false;
	}

	//Grab the distance between the officer and the target.
	float fDistance = Dist(pVehicle->GetTransform().GetPosition(), pTargetVehicle->GetTransform().GetPosition()).Getf();

	//Calculate the time to overtake.
	float fTimeToOvertake = fDistance / fSpeedDifference;

	return (fTimeToOvertake < fTime);
}

CTask::FSM_Return CTaskVehiclePersuit::WaitWhileCruising_OnUpdate()
{
	//Failure conditions:
	//	1) The sub-task is finished.
	//	2) The officer has waited too long.
	//	3) The officer is not in a vehicle.
	//	4) The target is not in a vehicle.
	//	5) The target's vehicle is moving slowly.
	//	6) The target has passed the officer.

	//Success conditions:
	//	1) The officer can see the target.
	//	2) The target is within a certain distance, or will overtake the officer soon.

	CPed* pPed = GetPed();
	const CVehicle* pVehicle = pPed->GetVehiclePedInside();
	const CVehicle* pTargetVehicle = GetTargetVehicle();
	bool bTargetMovingSlowly = true;
	if(pTargetVehicle)
	{
		float fSpeedSq = pTargetVehicle->GetVelocity().XYMag2();
		float fMaxSpeedSq = square(10.0f);
		if(fSpeedSq >= fMaxSpeedSq)
		{
			bTargetMovingSlowly = false;
		}
	}

	//Check the failure conditions.
	if(	!GetSubTask() || GetTimeInState() > 60.0f || !pVehicle || !pTargetVehicle || bTargetMovingSlowly || HasTargetPassedUs())
	{
		//enter default state
		SetState(ChooseDefaultState());
		return FSM_Continue;
	}

	//Check the success conditions.
	//Ensure the distance is valid.
	static dev_float s_fMaxDistance = pVehicle->GetVehicleType() != VEHICLE_TYPE_BIKE ? 45.0f : 20.0f;
	ScalarV scDistSq = DistSquared(pVehicle->GetTransform().GetPosition(), pTargetVehicle->GetTransform().GetPosition());
	ScalarV scMaxDistSq = ScalarVFromF32(square(s_fMaxDistance));
	if(IsLessThanAll(scDistSq, scMaxDistSq) || WillTargetVehicleOvertakeUsWithinTime(1.0f))
	{
		//enter chase task
		SetState(State_chaseInCar);
	}

	return FSM_Continue;
}

void CTaskVehiclePersuit::Attack_OnEnter()
{
	taskFatalAssertf(m_target.GetEntity(), "Null target passed!" );
	taskFatalAssertf(m_target.GetEntity()->GetIsPhysical(), "Target is non-physical!");

	sVehicleMissionParams params;
	params.SetTargetEntity(const_cast<CEntity*>(m_target.GetEntity()));
	params.m_iDrivingFlags = DMode_AvoidCars|DF_UseStringPullingAtJunctions;
	const CVehicle* pVehicle = GetPed()->GetMyVehicle();
	params.m_fCruiseSpeed = pVehicle->m_Transmission.GetDriveMaxVelocity();
	const CVehicle* pTargetVehicle = GetTargetVehicle();

	//b*2327046 push out aircraft arrival distance as the default 4 is very close for combat air vehicles
	if(pVehicle->GetIsAircraft() && pTargetVehicle && pTargetVehicle->GetIsAircraft())
	{
		params.m_fTargetArriveDist = 25.0f;
	}

	float fScriptOverrideDist = pVehicle->GetOverrideArriveDistForVehPersuitAttack();
	if (!IsClose(fScriptOverrideDist,-1.0f))
	{
		params.m_fTargetArriveDist = fScriptOverrideDist;
	}

	CTaskVehicleAttack* pTask = rage_new CTaskVehicleAttack(params);
	SetNewTask(rage_new CTaskControlVehicle(GetPed()->GetMyVehicle(), pTask, rage_new CTaskVehicleCombat(&m_target)));
}

CTask::FSM_Return CTaskVehiclePersuit::Attack_OnUpdate()
{
	// check for an exit vehicle state
	VehiclePersuitState newState = GetDesiredState(GetPed());
	if( newState != State_invalid )
	{
		SetState(newState);
		return FSM_Continue;
	}

	//Check if the sub-task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Restart the state.
		SetFlag(aiTaskFlags::RestartCurrentState);
	}

	return FSM_Continue;
}

void CTaskVehiclePersuit::ChaseInCar_OnEnter( CPed* UNUSED_PARAM(pPed) )
{
	//Assert the target ped is valid.
	const CPed* pTargetPed = GetTargetPed();
	taskAssertf(pTargetPed, "Target is invalid.");

	//Create the task.
	CTaskVehicleChase* pTask = rage_new CTaskVehicleChase(pTargetPed);

	//Check if the target is not a player.
	if(!pTargetPed->IsPlayer())
	{
		//Don't make aggressive moves as often.
		static float s_fAggressiveMoveDelayMin = 15.0f;
		static float s_fAggressiveMoveDelayMax = 20.0f;
		pTask->SetAggressiveMoveDelay(s_fAggressiveMoveDelayMin, s_fAggressiveMoveDelayMax);
	}

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskVehiclePersuit::ChaseInCar_OnUpdate( CPed* pPed )
{
	//Check if the state should change.
	VehiclePersuitState iState = GetDesiredState(pPed);
	if(iState != State_invalid)
	{
		//Change the state.
		SetState(iState);
	}
	else if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Move to the start state.
		SetState(State_start);
	}

	return FSM_Continue;
}

void CTaskVehiclePersuit::FollowInCar_OnEnter( CPed* pPed )
{
	taskFatalAssertf(m_target.GetEntity(), "Null target passed!" );
	taskFatalAssertf(m_target.GetEntity()->GetIsPhysical(), "Target is non-physical!");
	//const CPhysical* pPhysicalTarget = static_cast<const CPhysical*>(m_target.GetEntity());

	sVehicleMissionParams params;
	params.SetTargetEntity(const_cast<CEntity*>(m_target.GetEntity()));
	params.m_fCruiseSpeed = pPed->GetMyVehicle() ? pPed->GetMyVehicle()->m_Transmission.GetDriveMaxVelocity() : 40.0f;
	params.m_iDrivingFlags = DMode_AvoidCars_Reckless|DF_UseStringPullingAtJunctions;
	CTaskVehicleFollow* pTask = rage_new CTaskVehicleFollow(params, sm_Tunables.m_DistanceToFollowInCar);
	SetNewTask( rage_new CTaskControlVehicle(pPed->GetMyVehicle(), pTask ,rage_new CTaskVehicleCombat(&m_target) ) );
}

CTask::FSM_Return CTaskVehiclePersuit::FollowInCar_OnUpdate( CPed* pPed )
{
	// If the subtask has terminated - restart
	if( GetIsFlagSet(aiTaskFlags::SubTaskFinished) )
	{
		SetFlag(aiTaskFlags::RestartCurrentState);
		return FSM_Continue;
	}
	// check for an exit vehicle state
	VehiclePersuitState newState = GetDesiredState(pPed);
	if( newState != State_invalid )
	{
		SetState(newState);
		return FSM_Continue;
	}
	return FSM_Continue;
}

void CTaskVehiclePersuit::SubCombat_OnEnter()
{
	//Create the task.
	CTask* pTask = rage_new CTaskSubmarineCombat(m_target);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskVehiclePersuit::SubCombat_OnUpdate()
{
	//Check if the sub-task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Finish the task.
		SetState(State_exit);
		return FSM_Continue;
	}

	return FSM_Continue;
}

void CTaskVehiclePersuit::HeliCombat_OnEnter()
{
	//Create the task.
	CTask* pTask = rage_new CTaskHeliCombat(m_target);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskVehiclePersuit::HeliCombat_OnUpdate()
{
	//Check if the sub-task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Finish the task.
		SetState(State_exit);
		return FSM_Continue;
	}

	CTaskVehiclePersuit::VehiclePersuitState iDesiredAircraftState;
	if(GetDesiredAircraftState(GetPed(), iDesiredAircraftState) && iDesiredAircraftState != State_invalid)
	{
		SetState(iDesiredAircraftState);
		return FSM_Continue;
	}
	
	return FSM_Continue;
}

void CTaskVehiclePersuit::BoatCombat_OnEnter()
{
	//Create the task.
	CTask* pTask = rage_new CTaskBoatCombat(m_target);
	
	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskVehiclePersuit::BoatCombat_OnUpdate()
{
	//Check if we should exit the boat.
	if(ShouldExitBoat())
	{
		//Exit the vehicle.
		SetState(State_exitVehicle);
	}
	//Check if the sub-task has finished.
	else if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Finish the task.
		SetState(State_exit);
	}
	
	return FSM_Continue;
}

void CTaskVehiclePersuit::PassengerInVehicle_OnEnter( CPed* UNUSED_PARAM(pPed) )
{
	SetNewTask(rage_new CTaskVehicleCombat(&m_target) );
}

CTask::FSM_Return CTaskVehiclePersuit::PassengerInVehicle_OnUpdate( CPed* pPed )
{
	// If our target is a ped and is not in a vehicle, prevent shuffling to the driver seat if our driver is injured
	if( m_target.GetEntity()->GetIsTypePed() && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_AllowAutoShuffleToDriversSeat) )
	{
		const CPed* pPedTarget = static_cast<const CPed*>(m_target.GetEntity());
		if(!pPedTarget->GetIsInVehicle())
		{
			// We don't want AI getting out of the vehicle at really long distances away so make sure we are far enough away
			ScalarV scDistSq = DistSquared(pPed->GetTransform().GetPosition(), pPedTarget->GetTransform().GetPosition());
			ScalarV scMaxDistSq = ScalarVFromF32(MAX(sm_Tunables.m_IdealDistanceShotAt, sm_Tunables.m_IdealDistanceCouldLeaveCar) + sm_Tunables.m_PreventShufflingExtraRange);
			scMaxDistSq = (scMaxDistSq * scMaxDistSq);
			if(IsLessThanAll(scDistSq, scMaxDistSq))
			{
				pPed->SetPedResetFlag(CPED_RESET_FLAG_DisableSeatShuffleDueToInjuredDriver, true);
			}
		}
	}

	// If the subtask has terminated - restart
	if( GetIsFlagSet(aiTaskFlags::SubTaskFinished) )
	{
		SetFlag(aiTaskFlags::RestartCurrentState);
		return FSM_Continue;
	}

	// check for an exit vehicle state
	VehiclePersuitState newState = GetDesiredState(pPed);
	if( newState != State_invalid )
	{
		SetState(newState);
		return FSM_Continue;
	}
	return FSM_Continue;
}


void CTaskVehiclePersuit::SlowdownSafely_OnEnter( CPed* pPed )
{
	//CTaskVehicleHandBrake/CTaskVehicleStop simply brakes fully above s_fMaxSpeedForHandbrake with no avoidance
	//so instead use cruise task to slow us down until we can handbrake so we keep avoiding as long as possible
	if( pPed->GetMyVehicle()->IsDriver(pPed) )
	{
		float fVehicleSpeedSqr = pPed->GetMyVehicle()->GetVelocity().Mag2();
		if(fVehicleSpeedSqr > CTaskVehicleHandBrake::s_fMaxSpeedForHandbrake * CTaskVehicleHandBrake::s_fMaxSpeedForHandbrake)
		{
			CreateSlowDownGoToTask(pPed->GetMyVehicle());
		}
	}
}

CTask::FSM_Return CTaskVehiclePersuit::SlowdownSafely_OnUpdate( CPed* pPed )
{
	float fVehicleSpeedSqr = pPed->GetMyVehicle()->GetVelocity().Mag2();
	if(fVehicleSpeedSqr < CTaskVehicleHandBrake::s_fMaxSpeedForHandbrake * CTaskVehicleHandBrake::s_fMaxSpeedForHandbrake)
	{
		SetState(ShouldWaitInCar() ? state_waitInCar : State_emergencyStop);
	}
	else
	{
		CTaskVehicleMissionBase* pVehicleTask = pPed->GetMyVehicle()->GetIntelligence()->GetActiveTask();
		if(pVehicleTask)
		{
			if(pPed->GetMyVehicle()->IsDriver(pPed) )
			{
				pVehicleTask->SetCruiseSpeed(0.0f);
			}
		}
	}

	return FSM_Continue;
}

void CTaskVehiclePersuit::EmergencyStop_OnEnter( CPed* pPed )
{
	if (NetworkInterface::IsGameInProgress())
	{
		if (pPed && pPed->GetMyVehicle() && pPed->GetMyVehicle()->IsDriver(pPed) && pPed->GetMyVehicle()->IsNetworkClone())
			return;
	}
	
	const CPed* pPedTarget = NULL;
	taskFatalAssertf(m_target.GetEntity(), "Null target passed!" );
	if( m_target.GetEntity()->GetIsTypePed() )
		pPedTarget = static_cast<const CPed*>(m_target.GetEntity());

	// Work out where the target is in relation to where the player is
	Vector3 vToTarget = VEC3V_TO_VECTOR3(m_target.GetEntity()->GetTransform().GetPosition() - pPed->GetTransform().GetPosition());
	vToTarget.z = 0.0f;
	float fDistanceSq = vToTarget.Mag2();
	vToTarget.NormalizeSafe();
	float fDot =  DotProduct(vToTarget, VEC3V_TO_VECTOR3(pPed->GetTransform().GetB()));

	m_fJumpOutTimer = 0.0f;
	m_uRunningFlags.ClearFlag(RF_RunningTempAction);

	//Check if we should reverse or accelerate.
	bool bReverse		= false;
	bool bAccelerate	= false;

	const bool bTargetInVehicle = pPedTarget && pPedTarget->GetVehiclePedInside();
	const bool bTargetArmed = pPedTarget && pPedTarget->GetWeaponManager() && pPedTarget->GetWeaponManager()->GetIsArmed();
	const bool bIsVehicleMovingSlowly = (pPed->GetMyVehicle()->GetVelocity().XYMag2() < square(1.0f));
	const bool bIsVehicleAppropriate = (!pPed->GetMyVehicle()->InheritsFromBike());
	const bool bIsTargetInWater = (pPedTarget && CTargetInWaterHelper::IsInWater(*pPedTarget));
	if(!bTargetInVehicle && bTargetArmed && bIsVehicleMovingSlowly && pPed->GetIsDrivingVehicle() && bIsVehicleAppropriate && !bIsTargetInWater)
	{
		// Is the player just in front, or behind the car
		const bool bInFrontOfCar = fDistanceSq < rage::square(10.0f) && fDot > 0.707f;
		const bool bBehindOfCar = fDistanceSq < rage::square(7.0f) && fDot < -0.707f;

		if( pPed->PopTypeGet() != POPTYPE_RANDOM_SCENARIO && bInFrontOfCar )
		{
			if(fwRandom::GetRandomTrueFalse())
			{
				bAccelerate = true;
			}
		}
		else if( bBehindOfCar )
		{
			if(fwRandom::GetRandomTrueFalse())
			{
				bReverse = true;
			}
		}
	}

	if(bReverse)
	{
		SetNewTask(rage_new CTaskCarSetTempAction(pPed->GetMyVehicle(), (s8)TEMPACT_REVERSE_STRAIGHT_HARD, 1000 ));
		m_uRunningFlags.SetFlag(RF_RunningTempAction);
	}
	else if(bAccelerate)
	{
		SetNewTask(rage_new CTaskCarSetTempAction(pPed->GetMyVehicle(), (s8)TEMPACT_GOFORWARD_HARD, 2000 ));
		m_uRunningFlags.SetFlag(RF_RunningTempAction);
	}
	// If the car is moving, handbrake to a stop
	else if( pPed->GetMyVehicle()->IsDriver(pPed) && pPed->GetMyVehicle()->GetVelocity().Mag2() > 5.0f )
	{
		s8 iBrakeType;
		bool bDontTurnWhileBraking = (bIsTargetInWater && CCreekHackHelperGTAV::IsAffected(*pPed->GetMyVehicle()));
		if(pPed->GetMyVehicle()->InheritsFromBike() || bDontTurnWhileBraking)
		{
			iBrakeType = (s8) TEMPACT_BRAKE;
		}
		else
		{
			//Search for obstructions on the left/right.
			bool bObstructedL;
			bool bObstructedR;
			if(!FindObstructions(bObstructedL, bObstructedR))
			{
				bObstructedL = false;
				bObstructedR = false;
			}
			
			//Generate the left, center, and right actions.
			s8 iActionL = (s8)TEMPACT_HANDBRAKETURNLEFT_INTELLIGENT;
			s8 iActionC = (s8)TEMPACT_BRAKE;
			s8 iActionR = (s8)TEMPACT_HANDBRAKETURNRIGHT_INTELLIGENT;
			
			//Brake correctly based on where the surrounding obstructions are.
			if(bObstructedL && bObstructedR)
			{
				iBrakeType = iActionC;

				//Blocked left and right. Tell our task just to stop rather than using TEMPACT_BRAKE as then we'll perform avoidance while slowing down
				CreateSlowDownGoToTask(pPed->GetMyVehicle());
				return;
			}
			else if(bObstructedL)
			{
				iBrakeType = iActionR;
			}
			else if(bObstructedR)
			{
				iBrakeType = iActionL;
			}
			else
			{
				iBrakeType = (fwRandom::GetRandomTrueFalse() ? iActionL : iActionR);
			}
		}

		SetNewTask(rage_new CTaskCarSetTempAction(pPed->GetMyVehicle(), iBrakeType, 1000));
		m_uRunningFlags.SetFlag(RF_RunningTempAction);
	}
}

void CTaskVehiclePersuit::CreateSlowDownGoToTask(CVehicle* vehicle)
{
	sVehicleMissionParams params;

	params.m_iDrivingFlags = DMode_AvoidCars_Reckless|DF_DriveIntoOncomingTraffic|DF_SteerAroundPeds|
							DF_UseStringPullingAtJunctions|DF_AdjustCruiseSpeedBasedOnRoadSpeed|DF_DontTerminateTaskWhenAchieved;
	params.m_fTargetArriveDist = 0.0f;
	params.m_fCruiseSpeed = 0.0f;
	Vector3 vTarget;
	m_target.GetPosition(vTarget);
	params.SetTargetPosition(vTarget);

	//Create the vehicle task.
	CTask* pVehicleTask = CVehicleIntelligence::CreateAutomobileGotoTask(params, 10.0f);

	//Create combat subtask
	CTask* pSubTask = rage_new CTaskVehicleCombat(&m_target);

	//Create the task.
	CTask* pTask = rage_new CTaskControlVehicle(vehicle, pVehicleTask, pSubTask);

	SetNewTask(pTask);
}

CTask::FSM_Return CTaskVehiclePersuit::EmergencyStop_OnUpdate( CPed* pPed )
{
	bool bStopped = false;
	CVehicle* pMyVehicle = pPed->GetMyVehicle();
	bool bRunningTempAction = m_uRunningFlags.IsFlagSet(RF_RunningTempAction);
	if(!bRunningTempAction && pMyVehicle && pPed->GetMyVehicle()->IsDriver(pPed))
	{	
		CTaskVehicleMissionBase* pVehicleTask = pMyVehicle->GetIntelligence()->GetActiveTask();
		if(pVehicleTask)
		{
			pVehicleTask->SetCruiseSpeed(0.0f);
		}
		else
		{
			bRunningTempAction = true;
		}

		bStopped = pMyVehicle->GetVelocity().Mag2() < 0.1f *0.1f;
	}

	// If the subtask has terminated - quit
	if( ( bRunningTempAction && ( GetIsFlagSet(aiTaskFlags::SubTaskFinished) || GetSubTask() == NULL ) ) || bStopped)
	{
		if( pPed->GetIsInVehicle() && CanExitVehicle() )
		{
			SetState(State_exitVehicle);
		}
		else
		{
			SetState(State_exit);
		}

		return FSM_Continue;
	}

	return FSM_Continue;
}

void CTaskVehiclePersuit::EmergencyStop_OnExit()
{
	m_uRunningFlags.ClearFlag(RF_RunningTempAction);
}

void CTaskVehiclePersuit::ExitVehicle_OnEnter( CPed* pPed )
{
	//Create the exit vehicle task.
	VehicleEnterExitFlags vehicleFlags;
	vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DelayForTime);
	vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::ExitToWalk);
	
	//Check if we should not close the door.
	if(!ShouldCloseDoor())
	{
		vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DontCloseDoor);
	}
	
	//Check if we should exit to aim.
	if(ShouldExitVehicleToAim())
	{
		vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::ExitToAim);
	}

	//Check if we are forcing a warp
	if(m_uRunningFlags.IsFlagSet(RF_ForceExitVehicleWarp))
	{
		m_uRunningFlags.ClearFlag(RF_ForceExitVehicleWarp);
		vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::WarpIfDoorBlocked);
	}

	const CPed* pPedTarget = NULL;
	taskFatalAssertf(m_target.GetEntity(), "NULL target entity!");
	if( m_target.GetEntity()->GetIsTypePed() )
	{
		pPedTarget = static_cast<const CPed*>(m_target.GetEntity());
	}

	float fDelayTime;
	if(pPed->GetMyVehicle() && pPed->GetMyVehicle()->IsDriver(pPed))
	{
		if(pPedTarget && pPedTarget->IsExitingVehicle() && (pPedTarget->GetVehiclePedInside() || pPedTarget->GetMountPedOn()))
		{
			fDelayTime = pPed->GetRandomNumberInRangeFromSeed(sm_Tunables.m_MinDelayExitTime, sm_Tunables.m_MaxDelayExitTime);
		}
		else
		{
			fDelayTime = 0.0f;
		}
	}
	else
	{
		fDelayTime = pPed->GetRandomNumberInRangeFromSeed(sm_Tunables.m_MinDelayExitTime, sm_Tunables.m_MaxDelayExitTime);
	}

	CTaskExitVehicle* pTask = rage_new CTaskExitVehicle(pPed->GetMyVehicle(),vehicleFlags, fDelayTime);
	if(pPedTarget)
	{
		pTask->SetDesiredTarget(pPedTarget);
	}
	
	//Set the min time to wait for other ped to exit.
	pTask->SetMinTimeToWaitForOtherPedToExit(-1.0f);
	
	//Check if anyone in the vehicle should provide cover during exit vehicle.
	if(ShouldAnyoneProvideCoverDuringExitVehicle())
	{
		//Check if this ped should provide cover during exit vehicle.
		fwMvClipSetId clipSetId;
		if(ShouldPedProvideCoverDuringExitVehicle(*pPed, clipSetId))
		{
			//Note that we are providing cover during exit vehicle.
			m_uRunningFlags.SetFlag(RF_ProvidingCoverDuringExitVehicle);
			
			//The cover animations typically assume the weapon is in the hand of the ped.
			//If this is no longer accurate, an addition should be made to the anim metadata to specify this option.
			//Create the weapon object in the hand of the ped.
			CPedWeaponManager* pWeaponManager = pPed->GetWeaponManager();
			if(pWeaponManager)
			{
				pWeaponManager->EquipBestWeapon();
				pWeaponManager->CreateEquippedWeaponObject();
				pWeaponManager->BackupEquippedWeapon();
			}
			
			//Use the streamed exit.
			pTask->SetRunningFlag(CVehicleEnterExitFlags::IsStreamedExit);
			pTask->SetStreamedExitClipsetId(clipSetId);
		}
		
		//If we are not providing cover, we are on the receiving end.
		//Delay our exit a bit longer to give the others time to provide the cover.
		if(!m_uRunningFlags.IsFlagSet(RF_ProvidingCoverDuringExitVehicle))
		{
			pTask->SetDelayTime(pTask->GetDelayTime() + 3.0f);
		}
	}
	
	//Set the new task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskVehiclePersuit::ExitVehicle_OnUpdate( CPed* pPed )
{
	// If the subtask has terminated - quit
	if( GetIsFlagSet(aiTaskFlags::SubTaskFinished) || !GetSubTask() )
	{
		if( pPed->GetIsInVehicle() )
		{
			SetState(State_waitForVehicleExit);
			return FSM_Continue;
		}
		else
		{
			//Check if we should equip our best weapon.
			if(ShouldEquipBestWeaponAfterExitingVehicle())
			{
				//Equip the best weapon upon leaving the vehicle.
				//The best weapon inside/outside the vehicle can differ.
				CPedWeaponManager* pWeaponManager = pPed->GetWeaponManager();
				if(pWeaponManager)
				{
					pWeaponManager->EquipBestWeapon();
				}
			}
			
			//Check if we are providing cover during exit vehicle.
			if(m_uRunningFlags.IsFlagSet(RF_ProvidingCoverDuringExitVehicle))
			{
				//When a ped exits a vehicle, their equipped weapon is destroyed for some reason.
				//The cover animations assume the weapon is always in the hands of the ped,
				//so we need to re-create it here.
				CPedWeaponManager* pWeaponManager = pPed->GetWeaponManager();
				if(pWeaponManager)
				{
					pWeaponManager->CreateEquippedWeaponObject();
				}
				
				//Move to the provide cover state.
				SetState(State_provideCoverDuringExitVehicle);
			}
			else
			{
				SetState(State_exit);
			}

			return FSM_Continue;
		}
	}
	return FSM_Continue;
}

void CTaskVehiclePersuit::ExitVehicleImmediately_OnEnter( CPed* pPed )
{
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
	//Check if we should exit to aim.
	else if(ShouldExitVehicleToAim())
	{
		vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::ExitToAim);
	}
	
	float fExitDelayTime = pPed->GetMyVehicle() && pPed->GetMyVehicle()->IsDriver(pPed) ? 0.0f : (float)( pPed->GetRandomSeed()/(float)RAND_MAX_16 );
	CTaskExitVehicle* pTask = rage_new CTaskExitVehicle(pPed->GetMyVehicle(),vehicleFlags, fExitDelayTime);
	if(m_target.GetEntity() && m_target.GetEntity()->GetIsTypePed())
	{
		pTask->SetDesiredTarget(static_cast<const CPed*>(m_target.GetEntity()));
	}
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskVehiclePersuit::ExitVehicleImmediately_OnUpdate( CPed* pPed )
{
	// If the subtask has terminated - quit
	if( GetIsFlagSet(aiTaskFlags::SubTaskFinished) || !GetSubTask() )
	{
		if( pPed->GetIsInVehicle() )
		{
			if (GetTimeInState() > EXIT_FAILED_TIME_IN_STATE)
			{
				SetFlag(aiTaskFlags::RestartCurrentState);
				return FSM_Continue;
			}
		}
		else
		{
			SetState(State_exit);
			return FSM_Continue;
		}
	}
	return FSM_Continue;
}

bool CTaskVehiclePersuit::HasClearVehicleExitPoint() const
{
	const CPed* pPed = GetPed();

	CVehicle* pVehicle = pPed->GetMyVehicle();
	if(pVehicle)
	{
		s32 iPedsSeatIndex = pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
		if(iPedsSeatIndex < 0)
		{
			return false;
		}

		VehicleEnterExitFlags vehicleFlags;
		vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::IsExitingVehicle);
		s32 iTargetEntryPoint = CModelSeatInfo::EvaluateSeatAccessFromAllEntryPoints(pPed, pVehicle, SR_Specific, iPedsSeatIndex, false, vehicleFlags);
		if(iTargetEntryPoint >= 0)
		{
			return true;
		}
	}

	return false;
}

void CTaskVehiclePersuit::WaitForVehicleExit_OnEnter()
{
	m_NextExitCheckTimer.Set(fwTimer::GetTimeInMilliseconds(), fwRandom::GetRandomNumberInRange(750, 1000));
	SetNewTask(rage_new CTaskVehicleCombat(&m_target, CTaskVehicleCombat::Flag_forceAllowDriveBy) );
}

CTask::FSM_Return CTaskVehiclePersuit::WaitForVehicleExit_OnUpdate()
{
	VehiclePersuitState iState = GetDesiredState(GetPed());
	if(iState == State_invalid && m_fJumpOutTimer <= 0.0f)
	{
		// If we didn't choose an exit vehicle or stop state again then it likely means we should not be exiting anymore
		SetState(GetPed()->GetIsDrivingVehicle() ? ChooseDefaultState() : State_passengerInVehicle);
		return FSM_Continue;
	}
	else
	{
		if(m_NextExitCheckTimer.IsOutOfTime())
		{
			m_NextExitCheckTimer.Set(fwTimer::GetTimeInMilliseconds(), fwRandom::GetRandomNumberInRange(750, 1000));

			CTaskVehicleCombat* pSubTask = static_cast<CTaskVehicleCombat*>(FindSubTaskOfType(CTaskTypes::TASK_VEHICLE_COMBAT));
			if(HasClearVehicleExitPoint())
			{
				if(pSubTask && pSubTask->GetState() == CTaskVehicleCombat::State_vehicleGun)
				{
					GetSubTask()->RequestTermination();
				}
				else
				{
					SetState(State_exitVehicle);
					return FSM_Continue;
				}
			}
			else if(GetPed()->PopTypeIsMission() && GetTimeInState() > sm_Tunables.m_MaxTimeWaitForExitBeforeWarp)
			{
				m_uRunningFlags.SetFlag(RF_ForceExitVehicleWarp);
				SetState(State_exitVehicle);
				return FSM_Continue;
			}
			else if(!pSubTask)
			{
				SetState(State_exitVehicle);
				return FSM_Continue;
			}
		}
	}

	return FSM_Continue;
}

void CTaskVehiclePersuit::WaitForBuddiesToEnter_OnEnter( CPed* pPed )
{
	SetNewTask(rage_new CTaskCarDriveWander(pPed->GetMyVehicle(), DMode_StopForCars, 0 ));
}

CTask::FSM_Return CTaskVehiclePersuit::WaitForBuddiesToEnter_OnUpdate( CPed* pPed )
{
	// If the subtask has terminated  carry on
	if( GetIsFlagSet(aiTaskFlags::SubTaskFinished) )
	{
		SetState(State_start);
		return FSM_Continue;
	}
	// no-one is entering the car, carry on
	if( CountNumberBuddiesEnteringVehicle(pPed, pPed->GetMyVehicle()) == 0 )
	{
		SetState(State_start);
		return FSM_Continue;
	}

	return FSM_Continue;
}

void CTaskVehiclePersuit::ProvideCoverDuringExitVehicle_OnEnter( CPed* pPed )
{
	//Grab the target ped.
	const CPed* pTarget = GetTargetPed();
	
	//Check if the ped has a LOS to the target.
	CAITarget target;
	s32 iFlags = 0;
	if(pTarget && DoesPedHaveLosToTarget(*pPed, *pTarget))
	{
		//Set the target.
		target.SetEntity(pTarget);
	}
	else
	{
		//Calculate the position to aim at.
		Vec3V vAimPosition = pPed->GetTransform().GetPosition() + pPed->GetTransform().GetForward();
		
		//Set the target position.
		target.SetPosition(VEC3V_TO_VECTOR3(vAimPosition));
		
		//Aim only.
		iFlags |= CTaskShootAtTarget::Flag_aimOnly;
	}
	
	//Set the shoot at target task.
	SetNewTask(rage_new CTaskShootAtTarget(target, iFlags));
}

CTask::FSM_Return CTaskVehiclePersuit::ProvideCoverDuringExitVehicle_OnUpdate( CPed* pPed )
{
	//Ensure the ped is crouching.
	if(!pPed->GetIsCrouching())
	{
		//Force the ped into the crouching state.
		//This is because the cover animations end with the ped crouching.
		pPed->SetIsCrouching(true);
		pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Crouch_Idle);
	}
	
	//Grab the vehicle.
	CVehicle* pVehicle = pPed->GetMyVehicle();
	
	//Wait until there are no more alive peds in the vehicle.
	if(!pVehicle || !pVehicle->HasAlivePedsInIt())
	{
		SetState(State_exit);
	}
	
	return FSM_Continue;
}

void CTaskVehiclePersuit::ProvideCoverDuringExitVehicle_OnExit( CPed* pPed )
{
	//Clear the crouched state.
	pPed->SetIsCrouching(false);
}

void CTaskVehiclePersuit::ApproachTarget_OnEnter()
{
	//Ensure the vehicle is valid.
	CVehicle* pVehicle = GetPed()->GetMyVehicle();
	if(!taskVerifyf(pVehicle, "The vehicle is invalid."))
	{
		return;
	}
	
	//Create the parameters.
	sVehicleMissionParams params;
	params.SetTargetEntity(const_cast<CEntity *>(m_target.GetEntity()));
	params.m_iDrivingFlags = DMode_AvoidCars_Reckless|DF_DriveIntoOncomingTraffic|DF_SteerAroundPeds|DF_UseStringPullingAtJunctions|DF_AdjustCruiseSpeedBasedOnRoadSpeed;
	params.m_fTargetArriveDist = sm_Tunables.m_ApproachTarget.m_TargetArriveDist;
	params.m_fCruiseSpeed = sm_Tunables.m_ApproachTarget.m_CruiseSpeed;
	
	//Create the vehicle task.
	CTask* pVehicleTask = rage_new CTaskVehicleApproach(params);
	
	//Create the sub-task.
	CTask* pSubTask = rage_new CTaskVehicleCombat(&m_target);
	
	//Create the task.
	CTask* pTask = rage_new CTaskControlVehicle(pVehicle, pVehicleTask, pSubTask);
	
	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskVehiclePersuit::ApproachTarget_OnUpdate()
{
	//Update the vehicle task.
	UpdateVehicleTaskForApproachTarget();
	
	//Check if the sub-task is invalid.
	if(!GetSubTask())
	{
		//Finish the task.
		SetState(State_exit);
	}
	//Check if the sub-task has finished.
	else if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Make an emergency stop.
		SetState(State_slowdownSafely);
	}
	else
	{
		//Check if we have a new desired state.
		VehiclePersuitState iState = GetDesiredState(GetPed());
		if(iState != State_invalid)
		{
			//Move to the new state.
			SetState(iState);
		}
	}
		
	return FSM_Continue;
}

void CTaskVehiclePersuit::WaitInCar_OnEnter()
{
	//Grab the ped.
	CPed* pPed = GetPed();

	//Ensure the ped is driving a vehicle.
	if(!taskVerifyf(pPed->GetIsDrivingVehicle(), "The ped is not driving a vehicle."))
	{
		return;
	}

	//Set the target position.
	taskAssert(m_target.GetIsValid());
	m_target.GetPosition(RC_VECTOR3(m_vTargetPositionWhenWaitBegan));

	//Create the vehicle task.
	CTask* pVehicleTask = rage_new CTaskVehicleStop(CTaskVehicleStop::SF_DontTerminateWhenStopped | CTaskVehicleStop::SF_UseFullBrake);

	//Create the sub-task.
	CTask* pSubTask = rage_new CTaskVehicleCombat(&m_target);

	//Create the task.
	CTask* pTask = rage_new CTaskControlVehicle(GetPed()->GetVehiclePedInside(), pVehicleTask, pSubTask);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskVehiclePersuit::WaitInCar_OnUpdate()
{
	//Check if the sub-task has finished.
	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_start);
	}
	else
	{
		//Check if we can exit the vehicle.
		if(CanExitVehicle())
		{
			//Check if we've been waiting for a while.
			static dev_float s_fMinTimeInState = 3.0f;
			static dev_float s_fMaxTimeInState = 5.0f;
			float fMinTimeInState = GetPed()->GetRandomNumberInRangeFromSeed(s_fMinTimeInState, s_fMaxTimeInState);
			if(GetTimeInState() > fMinTimeInState)
			{
				SetState(State_exitVehicle);
				return FSM_Continue;
			}
		}

		//Check if we have a new desired state.
		VehiclePersuitState iState = GetDesiredState(GetPed());
		if(iState != State_invalid && iState != State_slowdownSafely) //we entered here from slowdownSafely, don't want to loop
		{
			SetState(iState);
			return FSM_Continue;
		}
	}

	return FSM_Continue;
}

//-------------------------------------------------------------------------
// Creates a car stealing a vehicle and persuing the car
//-------------------------------------------------------------------------
s32 CTaskVehiclePersuit::CountNumberBuddiesEnteringVehicle( CPed* pPed, CVehicle* pVehicle)
{
	s32 iCount = 0;

	// Make sure no other friendly ped is trying to enter it
	const CEntityScannerIterator entityList = pPed->GetPedIntelligence()->GetNearbyPeds();
	for(const CEntity* pEnt = entityList.GetFirst(); pEnt; pEnt = entityList.GetNext())
	{
		CPed* pFriendlyPed = (CPed*) pEnt;
		if( !pPed->GetPedIntelligence()->IsFriendlyWith( *pFriendlyPed ) )
		{
			continue;
		}

		CPedIntelligence* pFriendlyPedIntelligence = pFriendlyPed->GetPedIntelligence();

		// is ped entering this car?
		if ((pFriendlyPedIntelligence->GetQueriableInterface()->IsTaskCurrentlyRunning( CTaskTypes::TASK_ENTER_VEHICLE ) &&
			( ((CVehicle*)pFriendlyPedIntelligence->GetQueriableInterface()->GetTargetForTaskType(CTaskTypes::TASK_ENTER_VEHICLE)) == pVehicle ) ))
		{
			++iCount;
		}
		// If the ped was previously in the car, could he be trying to go back even if he is not currently entering?
		else
		{
			const s32 iPedPreviousSet = pVehicle->GetSeatManager()->GetLastPedsSeatIndex(pFriendlyPed);
			const bool bWillGoBackToThisVehicle = (pFriendlyPed->GetMyVehicle() == pVehicle) && iPedPreviousSet >= 0;
			if (bWillGoBackToThisVehicle)
			{
				if  (!pFriendlyPed->IsFatallyInjured())
				{
					const s32 iCurrentTaskPriority = pFriendlyPedIntelligence->GetActiveTaskPriority();

					// the ped is in a temporary response -> wait for him
					if (iCurrentTaskPriority <= PED_TASK_PRIORITY_EVENT_RESPONSE_TEMP)
					{
						++iCount;
					}
					// the ped is running a non temporary response -> wait for him for a limited time
					else if (iCurrentTaskPriority == PED_TASK_PRIORITY_EVENT_RESPONSE_NONTEMP)
					{
						TUNE_GROUP_INT(VEHICLE_PURSUIT, uMAX_TIME_TO_WAIT_FOR_A_NONTEMP_TASKED_PED_MS, 5000u, 0u, 10000000u, 500u);
						if (pFriendlyPedIntelligence->GetLastTimeStartedEnterVehicle() + uMAX_TIME_TO_WAIT_FOR_A_NONTEMP_TASKED_PED_MS > fwTimer::GetTimeInMilliseconds())
						{
							++iCount;
						}
					}
				}
			}
		}
	}

	return iCount;
}

#define TIME_TO_LEAVE_CAR_WHEN_UNDRIVABLE (5.0f)

bool CTaskVehiclePersuit::GetDesiredAircraftState(CPed* pPed, CTaskVehiclePersuit::VehiclePersuitState &iDesiredState)
{
	// Don't exit an aircraft, unless its landed
	if(pPed->GetMyVehicle()->GetIsAircraft())
	{
		bool bIsSubmerged = (pPed->GetMyVehicle()->m_Buoyancy.GetStatus() != NOT_IN_WATER && pPed->GetMyVehicle()->m_Buoyancy.GetSubmergedLevel() >= 0.5f);
		if(bIsSubmerged)
		{
			iDesiredState = State_exitVehicleImmediately;
			return true;
		}
		else if(pPed->GetMyVehicle()->HasContactWheels() == false)
		{
			iDesiredState = State_invalid;
			return true;
		}
		else if(pPed->GetMyVehicle()->InheritsFromPlane())
		{
			iDesiredState = State_emergencyStop;
			return true;
		}
		else if( pPed->GetMyVehicle()->GetVelocity().Mag2() > rage::square(1.0f))
		{
			if(!pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanUseVehicles))
			{
				iDesiredState = State_slowdownSafely;
				return true;
			}
			else
			{
				iDesiredState = State_invalid;
				return true;
			}
		}
		else if(pPed->GetMyVehicle()->InheritsFromHeli())
		{
			//Check if the heli is landed.
			const CHeli* pHeli = static_cast<CHeli *>(pPed->GetMyVehicle());
			if(pHeli->GetHeliIntelligence()->IsLanded())
			{
				//Check if we can leave the heli.
				static float s_fMinTimeLanded = 0.5f;
				bool bCanLeaveHeli = (pHeli->GetHeliIntelligence()->GetTimeSpentLanded() > s_fMinTimeLanded) &&
					( !pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanUseVehicles) ||
					  !pPed->GetIsDrivingVehicle() || pHeli->GetIsMainRotorBroken() || (pHeli->GetStatus() == STATUS_WRECKED) || !pHeli->IsInDriveableState() );
				if(bCanLeaveHeli)
				{
					iDesiredState = State_exitVehicle;
					return true;
				}
			}
			else if(pHeli->GetTimeSpentCollidingNotLanded() > CTaskHeliOrderResponse::GetTunables().m_MaxTimeCollidingBeforeExit)
			{
				iDesiredState = State_exitVehicle;
				return true;
			}
		}
	}

	return false;
}

CTaskVehiclePersuit::VehiclePersuitState CTaskVehiclePersuit::GetDesiredState(CPed* pPed)
{
	if(m_uRunningFlags.IsFlagSet(RF_TargetChanged))
	{
		m_uRunningFlags.ClearFlag(RF_TargetChanged);
		return State_start;
	}

	// If the flag isn't set allowing the ped to exit the vehicle - don't exit
	if(!CanExitVehicle())
	{
		return State_invalid;
	}

	taskFatalAssertf(pPed->GetMyVehicle(), "Ped vehicle pointer is NULL");
	
	// Process aircraft state before attack state, otherwise we'll end up stuck firing in an undriveable aircraft on the ground
	CTaskVehiclePersuit::VehiclePersuitState iDesiredAircraftState;
	if(GetDesiredAircraftState(pPed, iDesiredAircraftState))
	{
		return iDesiredAircraftState;
	}

	// don't exit if we are in the attack state and we can still attack
	if(GetState() == State_attack && ShouldAttack())
	{
		return State_invalid;
	}

	// Exit the vehicle if we aren't allowed to use vehicles
	if(!pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanUseVehicles))
	{
		return State_exitVehicle;
	}

	//Check if the vehicle is undriveable.
	bool bIncrementedVehicleUndriveableTime = false;
	const CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
	if(pVehicle)
	{
		if (CVehicleUndriveableHelper::IsUndriveable(*pVehicle))
		{
			bIncrementedVehicleUndriveableTime = true;
			m_fTimeVehicleUndriveable += GetTimeStep();
			if( m_fTimeVehicleUndriveable > TIME_TO_LEAVE_CAR_WHEN_UNDRIVABLE)
			{
				if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_GetOutUndriveableVehicle))
				{
					return State_exitVehicle;
				}

				CPedGroup* pPedGroup = pPed->GetPedsGroup();
				if(!pPedGroup || pPedGroup != CGameWorld::FindLocalPlayerGroup())
				{
					return State_exitVehicle;
				}
			}
		}
		else
		{
			m_fTimeVehicleUndriveable = 0.f;
		}
	}

	// Fill out a ped target pointer if the target is a ped.
	const CPed* pPedTarget = NULL;
	taskFatalAssertf(m_target.GetEntity(), "NULL target entity!");
	if( m_target.GetEntity()->GetIsTypePed() )
		pPedTarget = static_cast<const CPed*>(m_target.GetEntity());

	if(IsPedOnMovingVehicleOrTrain(pPedTarget))
	{
		return State_invalid;
	}

	// If the driver turns out to be a hated ped, exit straight away
	CPed* pDriver = pPed->GetMyVehicle()->GetDriver();
	bool bHatesDriver = ( (pPedTarget && pDriver == pPedTarget) || (pDriver && pPed->GetPedIntelligence()->IsThreatenedBy(*pDriver)) );
	if( bHatesDriver )
		return State_exitVehicleImmediately; // Immediately, doesn't wait for the car to slow

	const bool bPedIsDriver = pPed->GetMyVehicle()->IsDriver(pPed);

	// Passenger exit check - exit if the driver seat becomes empty because they have left or been killed
	if( !bPedIsDriver )
	{
		// Check if we should exit the vehicle early and make sure it's going slow enough
		if( m_uRunningFlags.IsFlagSet(RF_CanExitVehicleEarly) && pDriver &&
			pPed->GetMyVehicle()->GetVelocity().Mag2() < rage::square(sm_Tunables.m_MaxSpeedForEarlyCombatExit) )
		{
			// Then make sure the driver is running a combat task as well
			CTaskVehiclePersuit* pDriverTask = static_cast<CTaskVehiclePersuit*>(pDriver->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_VEHICLE_PERSUIT));
			if( pDriverTask )
			{
				// And make sure the driver is attempting to stop or exit
				s32 iDriverState = pDriverTask->GetState();
				if(  iDriverState == State_slowdownSafely || iDriverState == State_emergencyStop || iDriverState == State_exitVehicle || iDriverState == State_exitVehicleImmediately )
				{
					return State_exitVehicleImmediately;
				}
			}
		}
		
		//Check if the driver seat is empty.
		const bool bIsDriverSeatEmpty = (!pDriver || pDriver->IsInjured());
		
		//Check if the driver is exiting the vehicle.
		bool bIsDriverExitingVehicle = false;
		if(pDriver)
		{
			if( pDriver->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE) )
			{
				bIsDriverExitingVehicle = true;
			}
			else
			{
				CTaskVehiclePersuit* pDriverTask = static_cast<CTaskVehiclePersuit*>(pDriver->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_VEHICLE_PERSUIT));
				if(pDriverTask && pDriverTask->GetState() == State_waitForVehicleExit)
				{
					bIsDriverExitingVehicle = true;
				}
			}
		}
		
		//Check if the driver is trying to exit the vehicle.
		const bool bIsDriverTryingToExitVehicle = pDriver && IsPedTryingToExitVehicle(*pDriver);
		
		//Check if our old driver is entering the vehicle.
		CPed* pOldDriver = pPed->GetMyVehicle()->GetSeatManager()->GetLastPedInSeat(pPed->GetMyVehicle()->GetDriverSeat());
		bool bIsOldDriverEnteringVehicle = pOldDriver && !pOldDriver->IsInjured() &&
			pOldDriver->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE) &&
			pOldDriver->GetVehiclePedEntering() == pPed->GetMyVehicle();

		//Dont exit aquatic vehicles
		bool bAquaticVehicleInWater = pVehicle && pVehicle->GetIsAquatic() && pVehicle->GetIsInWater();
		
		//Check if we should exit the vehicle.
		bool bExitVehicle = !bAquaticVehicleInWater && !bIsOldDriverEnteringVehicle && 
			(bIsDriverSeatEmpty || bIsDriverExitingVehicle || bIsDriverTryingToExitVehicle);
		
		if(bExitVehicle)
		{
			float fTimeToLeave = pPed->GetRandomNumberInRangeFromSeed(sm_Tunables.m_MinPassengerTimeToLeaveVehicle, sm_Tunables.m_MaxPassengerTimeToLeaveVehicle);
			m_fJumpOutTimer += GetTimeStep();
			if(m_fJumpOutTimer > fTimeToLeave)
			{
				return State_exitVehicle;
			}
		}
		else
		{
			m_fJumpOutTimer = 0.0f;
		}
	}

	// Don't exit tanks
	if(pPed->GetMyVehicle()->IsTank())
	{
		return State_invalid;
	}

	// If the ped is in a group and isn't driving - don't exit
	if (pPed->GetPedsGroup() && !bPedIsDriver)
	{
		return State_invalid;
	}
	
	//Check if we should make an emergency stop due to an avoidance area.
	if(ShouldMakeEmergencyStopDueToAvoidanceArea())
	{
		return State_slowdownSafely;
	}

	// Ped target specific state changes
	if( pPedTarget )
	{
		//Check if the target is in a vehicle or on a mount.
		const CVehicle* pVehicleTargetInside = pPedTarget->GetVehiclePedInside();
		const CPed* pMountTargetOn = pPedTarget->GetMountPedOn();

		// If the target ped isn't in a vehicle and is not on a mount
		if( (!pVehicleTargetInside && !pMountTargetOn) ||
			pPedTarget->IsExitingVehicle() )
		{
			// Driver only checks - the passenger will exit in response to the driver deciding to give chase on foot
			if( bPedIsDriver )
			{
				CPedTargetting* pPedTargetting = pPed->GetPedIntelligence()->GetTargetting( true );
				LosStatus losStatus = pPedTargetting ? pPedTargetting->GetLosStatus( (CEntity*) pPedTarget ) : Los_clear;
				
				//Grab the vehicle.
				pVehicle = pPed->GetMyVehicle();

				//Keep track of whether we can leave the car.
				bool bCouldLeaveCar = false;

				CTaskVehicleGoToPointWithAvoidanceAutomobile::ObstructionInformation obstructionInformation;
				if(GetObstructionInformation(obstructionInformation))
				{
					//Check if we are completely obstructed.
					if(obstructionInformation.m_uFlags.IsFlagSet(CTaskVehicleGoToPointWithAvoidanceAutomobile::ObstructionInformation::IsCompletelyObstructed) || 
						obstructionInformation.m_uFlags.IsFlagSet(CTaskVehicleGoToPointWithAvoidanceAutomobile::ObstructionInformation::IsCompletelyObstructedClose))
					{
						//Check if we are generally stopped.
						float fSpeedSq = pVehicle->GetVelocity().XYMag2();
						if(fSpeedSq < 0.25f * 0.25f)
						{
							bCouldLeaveCar = true;
						}
					}
				}

				//Check if a collision with a law enforcement vehicle is imminent.
				static float s_fMinDistanceToLeave = 5.0f;
				static float s_fMaxDistanceToLeave = 10.0f;
				float fDistanceToLeave = pPed->GetRandomNumberInRangeFromSeed(s_fMinDistanceToLeave, s_fMaxDistanceToLeave);
				static float s_fMinTimeToLeave = 0.5f;
				static float s_fMaxTimeToLeave = 1.0f;
				float fTimeToLeave = pPed->GetRandomNumberInRangeFromSeed(s_fMinTimeToLeave, s_fMaxTimeToLeave);
				bool bWillCollideWithPed		= false;
				bool bWillCollideWithVehicle	= false;
				if(IsCollisionWithLawEnforcementImminent(fDistanceToLeave, fTimeToLeave, bWillCollideWithPed, bWillCollideWithVehicle))
				{
					bCouldLeaveCar = true;
				}
				
				//Check if we have been shot at.
				bool bShotAt = (pPed->IsLawEnforcementPed() && (pPed->GetMyVehicle()->GetWeaponDamageEntity() == pPedTarget));
				
				//Generate the ideal distance away from the target.
				float fDistanceToCheck = 0.0f;
				
				//Check if we could leave our car.
				if(bCouldLeaveCar)
				{
					fDistanceToCheck = Max(fDistanceToCheck, sm_Tunables.m_IdealDistanceCouldLeaveCar);
				}
				
				//Check if the vehicle has been shot.
				if(bShotAt)
				{
					fDistanceToCheck = Max(fDistanceToCheck, sm_Tunables.m_IdealDistanceShotAt);
				}
				
				//Check if the vehicle is a bike.
				if(pVehicle->InheritsFromBike())
				{
					fDistanceToCheck = Max(fDistanceToCheck, pPedTarget->GetWeaponManager() && !pPedTarget->GetWeaponManager()->GetIsArmed() ? sm_Tunables.m_IdealDistanceOnBikeAndTargetUnarmed : sm_Tunables.m_IdealDistanceOnBikeAndTargetArmed);
				}
				else
				{
					fDistanceToCheck = Max(fDistanceToCheck, pPedTarget->GetWeaponManager() && !pPedTarget->GetWeaponManager()->GetIsArmed() ? sm_Tunables.m_IdealDistanceInVehicleAndTargetUnarmed : sm_Tunables.m_IdealDistanceInVehicleAndTargetArmed);
				}
								
				//Calculate the distance it will take for the vehicle to stop.
				float fDistanceToStop = GetDistToStopAtCurrentSpeed(*pVehicle);
				
				//Generate the position offset for the stopping distance.
				Vec3V vDistanceToStop = Scale(pVehicle->GetTransform().GetB(), ScalarVFromF32(fDistanceToStop));
				
				//Add the distance to stop offset to the vehicle position.
				Vec3V vVehiclePos = Add(pVehicle->GetTransform().GetPosition(), vDistanceToStop);
				
				//Get the target position for the emergency stop.
				Vec3V vTargetPosition = GetTargetPositionForEmergencyStop();
				
				//Calculate the distance to the target.
				const float fDistanceSqToTarget = DistSquared(vTargetPosition, vVehiclePos).Getf();
				
				//Check if we are inside the stopping distance.
				if(fDistanceSqToTarget < rage::square(fDistanceToCheck))
				{
					//Check if the LOS is blocked.
					bool bTargetPositionKnown = (pPedTargetting && pPedTargetting->AreTargetsWhereaboutsKnown(NULL, pPedTarget));
					bool bLosBlocked = (losStatus == Los_blocked);
					
					//If our LOS is not blocked, or we could leave our car, or we were shot at, we can make the emergency stop.
					if(!bLosBlocked || bTargetPositionKnown || bCouldLeaveCar || bShotAt)
					{
						return State_slowdownSafely;
					}
				}
			}
		}
		// The target is in a vehicle or on a mount
		else
		{
			// Only leave if our vehicle is no longer drivable
			if(pVehicleTargetInside)
			{
				const bool bVehcileStuckOrNotDriveable = (pPed->GetMyVehicle()->IsOnItsSide() || pPed->GetMyVehicle()->IsUpsideDown() || !pPed->GetMyVehicle()->m_nVehicleFlags.bIsThisADriveableCar );
				if( bVehcileStuckOrNotDriveable )
				{
					if (!bIncrementedVehicleUndriveableTime)
					{
						bIncrementedVehicleUndriveableTime = true;
						m_fTimeVehicleUndriveable += GetTimeStep();
					}
					if( m_fTimeVehicleUndriveable > TIME_TO_LEAVE_CAR_WHEN_UNDRIVABLE)
					{
						return State_exitVehicle;
					}
				}
				else
				{
					m_fTimeVehicleUndriveable = 0.f;
				}
			}

			//Check if we are the driver.
			if(bPedIsDriver)
			{
				//Check if the target is moving slowly.
				float fSpeedSq = pVehicleTargetInside ? pVehicleTargetInside->GetVelocity().Mag2() : pMountTargetOn->GetVelocity().Mag2();
				static dev_float s_fMaxSpeed = 1.75f;
				bool bIsMovingSlowly = (fSpeedSq < square(s_fMaxSpeed));
				if(bIsMovingSlowly)
				{
					m_fJumpOutTimer += GetTimeStep();
				}
				else
				{
					m_fJumpOutTimer = 0.0f;	
				}

				//Check if we can leave the vehicle.
				float fMinDriverTimeToLeaveVehicle = pPed->GetRandomNumberInRangeFromSeed(
					sm_Tunables.m_MinDriverTimeToLeaveVehicle, sm_Tunables.m_MaxDriverTimeToLeaveVehicle);
				bool bCanLeaveVehicle = (m_fJumpOutTimer >= fMinDriverTimeToLeaveVehicle);
				if(!bCanLeaveVehicle)
				{
					//We are also allowed to leave if the smoothed speed is very low -- this can happen
					//when cops enter combat very close to a vehicle that hasn't been moving for a while.
					static dev_float s_fMaxSmoothedSpeed = 0.25f;
					bCanLeaveVehicle = (pVehicleTargetInside && !pVehicleTargetInside->IsNetworkClone() &&
						(pVehicleTargetInside->GetIntelligence()->GetSmoothedSpeedSq() < square(s_fMaxSmoothedSpeed)));
				}

				bool bNeedLOS = true;

				//Check if we are generally stopped and the target isn't moving much
				if(pVehicleTargetInside->GetVelocity().XYMag2() < 1.0f * 1.0f && pVehicle->GetVelocity().XYMag2() < 0.25f * 0.25f)
				{
					CTaskVehicleGoToPointWithAvoidanceAutomobile::ObstructionInformation obstructionInformation;
					if(!bCanLeaveVehicle && GetObstructionInformation(obstructionInformation))
					{
						//Check if we are completely obstructed.
						if(obstructionInformation.m_uFlags.IsFlagSet(CTaskVehicleGoToPointWithAvoidanceAutomobile::ObstructionInformation::IsCompletelyObstructed) || 
							obstructionInformation.m_uFlags.IsFlagSet(CTaskVehicleGoToPointWithAvoidanceAutomobile::ObstructionInformation::IsCompletelyObstructedClose))
						{					
							bCanLeaveVehicle = true;
							bNeedLOS = false;
						}
					}
				}

				//Check if we can leave the vehicle.
				if(bCanLeaveVehicle)
				{
					//Get the target position for the emergency stop.
					Vec3V vTargetPosition = GetTargetPositionForEmergencyStop();

					//Check if we can make an emergency stop.
					CPedTargetting* pPedTargetting = pPed->GetPedIntelligence()->GetTargetting(false);
					bool bIsLosBlocked = (pPedTargetting && (pPedTargetting->GetLosStatus(pPedTarget) == Los_blocked));
					bool bIsWithinZThreshold = (Abs(vTargetPosition.GetZf() - pPed->GetTransform().GetPosition().GetZf()) <= 4.0f);
					bool bCanMakeEmergencyStop = (!bIsLosBlocked || bIsWithinZThreshold || !bNeedLOS);
					if(bCanMakeEmergencyStop)
					{
						//Check if we are in range.
						ScalarV scDistSq = DistSquared(pPed->GetTransform().GetPosition(), vTargetPosition);
						static dev_float s_fMaxDistance = 30.0f;
						ScalarV scMaxDistSq = ScalarVFromF32(square(s_fMaxDistance));
						if(IsLessThanAll(scDistSq, scMaxDistSq))
						{
							return State_slowdownSafely;
						}
					}
				}
			}
		}
	}

	if(bPedIsDriver && m_uRunningFlags.IsFlagSet(RF_IsBlockedByLockedDoor))
	{
		GetPed()->GetPedIntelligence()->GetCombatBehaviour().ClearFlag(CCombatData::BF_CanUseVehicles);

		return State_exitVehicle;
	}

	// If buddies are currently trying to enter the car, wait for them
	if( bPedIsDriver && !pPed->GetMyVehicle()->GetIsAircraft() )
	{
		if( CountNumberBuddiesEnteringVehicle(pPed, pPed->GetMyVehicle()) > 0 )
		{
			return State_waitForBuddiesToEnter;
		}
	}

	//come to a stop if there's too many parked police vehicles ahead of us
	if(GetState() == State_approachTarget)
	{
		if(ShouldStopDueToBlockingLawVehicles())
		{
			return State_slowdownSafely;
		}
	}
	
	//Check if the current state is valid.
	if(!IsCurrentStateValid())
	{
		//Choose a default state.
		return ChooseDefaultState();
	}
	
	return State_invalid;
}

void CTaskVehiclePersuit::ActivateTimeslicing()
{
	TUNE_GROUP_BOOL(VEHICLE_PURSUIT, ACTIVATE_TIMESLICING, true);
	if(ACTIVATE_TIMESLICING)
	{
		GetPed()->GetPedAiLod().ClearBlockedLodFlag(CPedAILod::AL_LodTimesliceIntelligenceUpdate);
	}
}

bool CTaskVehiclePersuit::AreAbnormalExitsDisabledForSeat() const
{
	const CVehicleSeatAnimInfo* pVehicleSeatAnimInfo = CVehicleMetadataMgr::GetSeatAnimInfoFromPed(GetPed());
	return (pVehicleSeatAnimInfo && pVehicleSeatAnimInfo->GetDisableAbnormalExits());
}

Vec3V_Out CTaskVehiclePersuit::CalculateDirectionForPed(const CPed& rPed) const
{
	//Check if the ped is in a vehicle.
	const CVehicle* pVehicle = rPed.GetVehiclePedInside();
	if(pVehicle)
	{
		return pVehicle->GetTransform().GetForward();
	}
	else
	{
		return rPed.GetTransform().GetForward();
	}
}

bool CTaskVehiclePersuit::CanChangeState() const
{
	//Check the state.
	switch(GetState())
	{
		case State_chaseInCar:
		case State_approachTarget:
		{
			//Ensure we are not stuck.
			if(IsStuck())
			{
				return false;
			}

			break;
		}
		default:
		{
			break;
		}
	}

	return true;
}

bool CTaskVehiclePersuit::CanExitVehicle() const
{
	//Ensure the flag is set.
	if(!GetPed()->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanLeaveVehicle))
	{
		return false;
	}

	//Check if the ped is random, and abnormal exits are disabled for the seat.
	if(GetPed()->PopTypeIsRandom() && AreAbnormalExitsDisabledForSeat())
	{
		//Apparently we want to allow abnormal exits in some cases.
		bool bAllowAbnormalExit = GetPed()->GetIsDrivingVehicle();
		if(!bAllowAbnormalExit)
		{
			return false;
		}
	}

	return true;
}

CTaskVehiclePersuit::VehiclePersuitState CTaskVehiclePersuit::ChooseDefaultState() const
{
	//Check if we should wait in a car.
	if(ShouldWaitInCar())
	{
		return State_slowdownSafely;	
	}
	//Check if we should chase in a car.
	else if(ShouldChaseInCar())
	{
		return State_chaseInCar;
	}
	else
	{
		return State_approachTarget;
	}
}

bool CTaskVehiclePersuit::DoesPedHaveLosToTarget(const CPed& rPed, const CPed& rTarget) const
{
	//Ensure the targeting is valid.
	CPedTargetting* pPedTargetting = rPed.GetPedIntelligence()->GetTargetting(true);
	if(!pPedTargetting)
	{
		return false;
	}
	
	//Get the Los status.
	if(pPedTargetting->GetLosStatus(&rTarget) != Los_clear)
	{
		return false;
	}
	
	return true;
}

bool CTaskVehiclePersuit::FindObstructions(bool& bLeft, bool& bRight)
{
	//Grab the ped.
	const CPed* pPed = GetPed();
	if(!pPed)
	{
		return false;
	}
	
	//Grab the vehicle.
	const CVehicle* pVehicle = pPed->GetVehiclePedInside();
	if(!pVehicle)
	{
		return false;
	}
	
	//Calculate the magnitude for the probe.
	float fMagnitude = GetDistToStopAtCurrentSpeed(*pVehicle);
	
	//Grab the vehicle position.
	Vec3V vVehiclePosition = pVehicle->GetTransform().GetPosition();
	
	//Calculate the left position.
	Vec3V vPosL;
	int iBoneIndexL = pVehicle->GetBoneIndex(VEH_HEADLIGHT_L);
	if(iBoneIndexL >= 0)
	{
		Matrix34 mBoneL;
		pVehicle->GetGlobalMtx(iBoneIndexL, mBoneL);
		vPosL = VECTOR3_TO_VEC3V(mBoneL.d);
	}
	else
	{
		vPosL = vVehiclePosition;
	}
	
	//Calculate the right position.
	Vec3V vPosR;
	int iBoneIndexR = pVehicle->GetBoneIndex(VEH_HEADLIGHT_R);
	if(iBoneIndexR >= 0)
	{
		Matrix34 mBoneR;
		pVehicle->GetGlobalMtx(iBoneIndexR, mBoneR);
		vPosR = VECTOR3_TO_VEC3V(mBoneR.d);

	}
	else
	{
		vPosR = vVehiclePosition;
	}
	
	//Check if we are off-road.
	bool bIsOffRoad = ThePaths.IsPositionOffRoadGivenNodelist(pVehicle, vVehiclePosition);
	
	//Check for obstructions on the left at the angles.
	bLeft = FindObstructionAtAngle(pVehicle, vPosL, sm_Tunables.m_ObstructionProbeAngleA, fMagnitude, bIsOffRoad);
	if(!bLeft)
	{
		bLeft = FindObstructionAtAngle(pVehicle, vPosL, sm_Tunables.m_ObstructionProbeAngleB, fMagnitude, bIsOffRoad);
		if(!bLeft)
		{
			bLeft = FindObstructionAtAngle(pVehicle, vPosL, sm_Tunables.m_ObstructionProbeAngleC, fMagnitude, bIsOffRoad);
		}
	}
	
	//Check for obstructions on the right at the angles.
	bRight = FindObstructionAtAngle(pVehicle, vPosR, -sm_Tunables.m_ObstructionProbeAngleA, fMagnitude, bIsOffRoad);
	if(!bRight)
	{
		bRight = FindObstructionAtAngle(pVehicle, vPosR, -sm_Tunables.m_ObstructionProbeAngleB, fMagnitude, bIsOffRoad);
		if(!bRight)
		{
			bRight = FindObstructionAtAngle(pVehicle, vPosR, -sm_Tunables.m_ObstructionProbeAngleC, fMagnitude, bIsOffRoad);
		}
	}
	
	return true;
}

bool CTaskVehiclePersuit::FindObstructionAtAngle(const CVehicle* pVehicle, Vec3V_ConstRef vStart, const float fAngle, const float fMagnitude, bool bIsOffRoad)
{
	//Generate a rotator for the angle.
	QuatV qRotator = QuatVFromAxisAngle(pVehicle->GetTransform().GetC(), ScalarVFromF32(fAngle * DtoR));
	
	//Rotate the forward vector about the up axis.
	Vec3V vProbe = Transform(qRotator, pVehicle->GetTransform().GetB());
	
	//Scale the rotated vector.
	vProbe = Scale(vProbe, ScalarVFromF32(fMagnitude));
	
	//Generate the start and end positions.
	Vec3V vEnd = vStart + vProbe;
	
	//Generate the radius.
	float fRadius = pVehicle->GetBaseModelInfo()->GetBoundingSphereRadius();
	
#if !__FINAL
	//Append the obstruction probe points.
	if(m_uNumObstructionPoints < ms_uMaxObstructionProbePoints)
	{
		m_avObstructionProbeStartPoints[m_uNumObstructionPoints]	= vStart;
		m_avObstructionProbeEndPoints[m_uNumObstructionPoints]		= vEnd;
		m_avObstructionProbeRadii[m_uNumObstructionPoints]			= fRadius;
		++m_uNumObstructionPoints;
	}
#endif

	//Check if we are not off-road.
	if(!bIsOffRoad)
	{
		//Ensure the end point is not off-road.
		if(ThePaths.IsPositionOffRoadGivenNodelist(pVehicle, vEnd))
		{
			//Consider this direction obstructed.
			return true;
		}
	}

	//Execute a shape test.  Check for buildings, vehicles, and peds.
	WorldProbe::CShapeTestCapsuleDesc probeDesc;
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_BUILDING_INCLUDE_TYPES|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_PED_TYPE);
	probeDesc.SetTypeFlags(ArchetypeFlags::GTA_AI_TEST);
	probeDesc.SetExcludeEntity(pVehicle);
	probeDesc.SetCapsule(VEC3V_TO_VECTOR3(vStart), VEC3V_TO_VECTOR3(vEnd), fRadius);
	return WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc);
}

float CTaskVehiclePersuit::GetDistToStopAtCurrentSpeed(const CVehicle& rVehicle) const
{
	//Generate the distance it will take to stop at the current speed.
	float fDistanceToStop = rVehicle.GetAIHandlingInfo()->GetDistToStopAtCurrentSpeed(rVehicle.GetVelocity().Mag());
	
	//HACK:	GetDistToStopAtCurrentSpeed appears to be broken and giving values that are way too high.
	//		This is likely due to the function assuming a linear decrease in speed and not taking deceleration into account.
	//TODO: Remove specialized tuning values if/when GetDistToStopAtCurrentSpeed is fixed or replaced with a more accurate version.
	
	//Apply the distance to stop multiplier.
	fDistanceToStop *= sm_Tunables.m_DistanceToStopMultiplier;
	
	//Generate the mass multiplier.
	float fDistanceToStopMassMultiplier = (rVehicle.pHandling->m_fMass / sm_Tunables.m_DistanceToStopMassIdeal);
	
	//Lerp the mass multiplier towards 1.0 based on the mass weight.
	fDistanceToStopMassMultiplier = Lerp(sm_Tunables.m_DistanceToStopMassWeight, 1.0f, fDistanceToStopMassMultiplier);
	
	//Apply the mass multiplier.
	fDistanceToStop *= fDistanceToStopMassMultiplier;
	
	return fDistanceToStop;
}

bool CTaskVehiclePersuit::GetObstructionInformation(CTaskVehicleGoToPointWithAvoidanceAutomobile::ObstructionInformation& rInformation) const
{
	//Ensure we are in a vehicle.
	CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
	if(!pVehicle)
	{
		return false;
	}

	//Ensure the vehicle task is valid.
	CTaskVehicleMissionBase* pVehicleTask = pVehicle->GetIntelligence()->GetActiveTask();
	if(!pVehicleTask)
	{
		return false;
	}

	//Ensure the avoidance task is valid.
	CTaskVehicleGoToPointWithAvoidanceAutomobile* pAvoidanceTask = static_cast<CTaskVehicleGoToPointWithAvoidanceAutomobile *>(pVehicleTask->FindSubTaskOfType(CTaskTypes::TASK_VEHICLE_GOTO_POINT_WITH_AVOIDANCE_AUTOMOBILE));
	if(!pAvoidanceTask)
	{
		return false;
	}

	//Set the obstruction information.
	rInformation = pAvoidanceTask->GetObstructionInformation();

	return true;
}

CPed* CTaskVehiclePersuit::GetTargetMount() const
{
	//Ensure the target ped is valid.
	const CPed* pTargetPed = GetTargetPed();
	if(!pTargetPed)
	{
		return NULL;
	}

	return pTargetPed->GetMountPedOn();
}

const CPed* CTaskVehiclePersuit::GetTargetPed() const
{
	//Grab the target entity.
	const CEntity* pEntity = m_target.GetEntity();
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

bool CTaskVehiclePersuit::GetTargetPositionForApproach(Vec3V_InOut vTargetPosition) const
{
	//Ensure the vehicle is valid.
	const CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
	if(!pVehicle)
	{
		return false;
	}
	
	//Ensure the task is valid.
	CTaskVehicleMissionBase* pTask = pVehicle->GetIntelligence()->GetActiveTask();
	if(!pTask)
	{
		return false;
	}

	//Ensure the task type is valid.
	if(pTask->GetTaskType() != CTaskTypes::TASK_VEHICLE_APPROACH)
	{
		return false;
	}
	
	//Get the target position.
	if(!static_cast<CTaskVehicleApproach *>(pTask)->GetTargetPosition(vTargetPosition))
	{
		return false;
	}
	
	return true;
}

Vec3V_Out CTaskVehiclePersuit::GetTargetPositionForEmergencyStop() const
{
	//Keep track of the alternate position.
	Vec3V vAlternatePosition(V_ZERO);

	//Check the state.
	switch(GetState())
	{
		case State_approachTarget:
		{
			//Get the target position for approach.
			GetTargetPositionForApproach(vAlternatePosition);
			break;
		}
		default:
		{
			break;
		}
	}
	
	//Get the target position.
	Vec3V vTargetPosition;
	m_target.GetPosition(RC_VECTOR3(vTargetPosition));

	//Check if the alternate position is valid.
	if(!IsCloseAll(vAlternatePosition, Vec3V(V_ZERO), ScalarVFromF32(SMALL_FLOAT)))
	{
		//Check which position is closer.
		ScalarV scDistAlternateSq	= DistSquared(GetPed()->GetTransform().GetPosition(), vAlternatePosition);
		ScalarV scDistTargetSq		= DistSquared(GetPed()->GetTransform().GetPosition(), vTargetPosition);
		if(IsLessThanAll(scDistAlternateSq, scDistTargetSq))
		{
			return vAlternatePosition;
		}
	}

	return vTargetPosition;
}

CVehicle* CTaskVehiclePersuit::GetTargetVehicle() const
{
	//Ensure the target ped is valid.
	const CPed* pTargetPed = GetTargetPed();
	if(!pTargetPed)
	{
		return NULL;
	}

	return pTargetPed->GetVehiclePedInside();
}

bool CTaskVehiclePersuit::HasMountedGuns() const
{
	weaponAssert(GetPed()->GetWeaponManager() && GetPed()->GetInventory());
	const bool bHasWeaponsAtSeat = GetPed()->GetMyVehicle()->GetSeatHasWeaponsOrTurrets(GetPed()->GetMyVehicle()->GetSeatManager()->GetPedsSeatIndex(GetPed()));
	return bHasWeaponsAtSeat;
}

bool CTaskVehiclePersuit::IsCollidingWithLockedDoor() const
{
	const CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
	if(!taskVerifyf(pVehicle, "The vehicle is invalid."))
	{
		return false;
	}

	const CCollisionRecord* pColRecord = pVehicle->GetFrameCollisionHistory()->GetMostSignificantCollisionRecord();
	if(!pColRecord)
	{
		return false;
	}

	const CEntity* pColEntity = pColRecord->m_pRegdCollisionEntity;
	if(!pColEntity || !pColEntity->GetIsTypeObject())
	{
		return false;
	}

	const CObject* pColObject = static_cast<const CObject *>(pColEntity);
	if(!pColObject->IsADoor())
	{
		return false;
	}

	const CDoor* pColDoor = static_cast<const CDoor *>(pColObject);
	bool bLocked = pColDoor->IsBaseFlagSet(fwEntity::IS_FIXED);
	if(!bLocked)
	{
		return false;
	}

	return true;
}

bool CTaskVehiclePersuit::IsCollisionWithLawEnforcementImminent(float fMaxDistance, float fMaxTime, bool& bWithPed, bool& bWithVehicle) const
{
	//Ensure the obstruction information is valid.
	CTaskVehicleGoToPointWithAvoidanceAutomobile::ObstructionInformation obstructionInformation;
	if(!GetObstructionInformation(obstructionInformation))
	{
		return false;
	}

	//Ensure we have been obstructed by law enforcement.
	bWithPed = obstructionInformation.m_uFlags.IsFlagSet(
		CTaskVehicleGoToPointWithAvoidanceAutomobile::ObstructionInformation::IsObstructedByLawEnforcementPed);
	bWithVehicle = obstructionInformation.m_uFlags.IsFlagSet(
		CTaskVehicleGoToPointWithAvoidanceAutomobile::ObstructionInformation::IsObstructedByLawEnforcementVehicle);
	if(!bWithPed && !bWithVehicle)
	{
		return false;
	}

	//Grab the vehicle.
	const CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
	taskAssert(pVehicle);

	//Check if the distance is within the threshold.
	float fDistanceToObstruction = obstructionInformation.m_fDistanceToLawEnforcementObstruction;
	if(fDistanceToObstruction < fMaxDistance)
	{
		return true;
	}

	//Check if the time is within the threshold.
	float fInvSpeed = 1.0f / pVehicle->GetVelocity().XYMag();
	float fTimeToObstruction = fDistanceToObstruction * fInvSpeed;
	if(fTimeToObstruction < fMaxTime)
	{
		return true;
	}

	return false;
}

bool CTaskVehiclePersuit::IsCurrentStateValid() const
{
	//Check if we can change state.
	if(CanChangeState())
	{
		//Check the state.
		switch(GetState())
		{
			case State_chaseInCar:
			{
				return (!ShouldWaitInCar() && ShouldChaseInCar());
			}
			case State_approachTarget:
			{
				return (!ShouldWaitInCar() && !ShouldChaseInCar());
			}
			case state_waitInCar:
			{
				return (ShouldWaitInCar());
			}
			default:
			{
				break;
			}
		}
	}

	return true;
}

bool CTaskVehiclePersuit::IsInFistFight() const
{
	//Ensure the target is valid.
	const CPed* pTarget = GetTargetPed();
	if(!pTarget)
	{
		return false;
	}
	
	//Ensure the target is not armed with a gun.
	if(IsPedArmedWithGun(*pTarget))
	{
		return false;
	}
	
	//Ensure the ped is not armed with a gun.
	if(IsPedArmedWithGun(*GetPed()))
	{
		return false;
	}
	
	return true;
}

bool CTaskVehiclePersuit::IsPedArmedWithGun(const CPed& rPed) const
{
	//Ensure the weapon manager is valid.
	const CPedWeaponManager* pWeaponManager = rPed.GetWeaponManager();
	if(!pWeaponManager)
	{
		return false;
	}
	
	return pWeaponManager->GetIsArmedGun();
}

bool CTaskVehiclePersuit::IsPedTryingToExitVehicle(const CPed& rPed) const
{
	//Ensure the ped is running the task.
	const CTask* pTask = rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_VEHICLE_PERSUIT);
	if(!pTask)
	{
		return false;
	}
	
	//Ensure the state is valid.
	s32 iState = pTask->GetState();
	switch(iState)
	{
		case State_exitVehicle:
		case State_exitVehicleImmediately:
		case State_waitForVehicleExit:
		{
			break;
		}
		default:
		{
			return false;
		}
	}
	
	return true;
}

bool CTaskVehiclePersuit::IsPedsBestWeaponAGun(const CPed& rPed) const
{
	//Ensure the weapon manager is valid.
	const CPedWeaponManager* pWeaponManager = rPed.GetWeaponManager();
	if(!pWeaponManager)
	{
		return false;
	}
	
	//Ensure the best weapon info is valid (ignore the vehicle check).
	const CWeaponInfo* pWeaponInfo = pWeaponManager->GetBestWeaponInfo(true);
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

bool CTaskVehiclePersuit::IsStuck() const
{
	//Ensure our vehicle is valid.
	const CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
	if(!pVehicle)
	{
		return false;
	}

	return CVehicleStuckDuringCombatHelper::IsStuck(*pVehicle);
}

bool CTaskVehiclePersuit::IsTargetInFrontOfUs() const
{
	//Ensure the target is valid.
	const CPed* pTarget = GetTargetPed();
	if(!pTarget)
	{
		return false;
	}
	
	//Grab the ped.
	const CPed* pPed = GetPed();
	
	//Grab the ped position.
	Vec3V vPedPosition = pPed->GetTransform().GetPosition();
	
	//Grab the target position.
	Vec3V vTargetPosition = pTarget->GetTransform().GetPosition();
	
	//Generate a vector from the ped to the target.
	Vec3V vPedToTarget = Subtract(vTargetPosition, vPedPosition);
	
	//Calculate the direction for the ped.
	Vec3V vPedDirection = CalculateDirectionForPed(*pPed);
	
	//Ensure the target is in front of us.
	ScalarV scDot = Dot(vPedToTarget, vPedDirection);
	ScalarV scMinDot = ScalarV(V_ZERO);
	if(IsLessThanAll(scDot, scMinDot))
	{
		return false;
	}
	
	return true;
}

bool CTaskVehiclePersuit::IsTargetInVehicle() const
{
	return (GetTargetVehicle() != NULL);
}

bool CTaskVehiclePersuit::IsTargetInBoat() const
{
	return GetTargetVehicle() && GetTargetVehicle()->InheritsFromBoat() && GetTargetVehicle()->GetIsInWater();
}

bool CTaskVehiclePersuit::IsTargetMovingSlowlyInVehicle() const
{
	//Ensure the target is in a vehicle.
	const CVehicle* pVehicle = GetTargetVehicle();
	if(!pVehicle)
	{
		return false;
	}

	//Check if the speed is slow.
	float fSpeedSq = pVehicle->GetVelocity().Mag2();
	static dev_float s_fMaxSpeed = 1.75f;
	float fMaxSpeedSq = square(s_fMaxSpeed);
	return (fSpeedSq < fMaxSpeedSq);
}

bool CTaskVehiclePersuit::IsTargetOnMount() const
{
	return (GetTargetMount() != NULL);
}

bool CTaskVehiclePersuit::MustPedBeInVehicle() const
{
	switch(GetState())
	{
		case State_exitVehicle:
		case State_exitVehicleImmediately:
		case State_provideCoverDuringExitVehicle:
		{
			return false;
		}
		default:
		{
			return true;
		}
	}
}

void CTaskVehiclePersuit::ProcessAudio()
{
	//Ensure the audio should be processed this frame.
	if(!GetPed()->GetPedIntelligence()->GetAudioDistributer().ShouldBeProcessedThisFrame())
	{
		return;
	}

	//Ensure the flag is not set.
	if(GetPed()->GetPedResetFlag(CPED_RESET_FLAG_DisableCombatAudio))
	{
		return;
	}

	//Ensure some time has passed before saying any lines.
	static dev_float s_fMinTime = 2.0f;
	if(GetTimeRunning() < s_fMinTime)
	{
		return;
	}

	CVehicle* pTargetVehicle = GetTargetVehicle();

	//These are arranged in prioritized order.
	if(GetPed()->IsLawEnforcementPed() && pTargetVehicle)
	{
		//Check if we have not said initial audio.
		if(!m_uRunningFlags.IsFlagSet(RF_HasSaidInitialAudio))
		{
			//Check if we are close enough.
			Vec3V vTargetPosition;
			m_target.GetPosition(RC_VECTOR3(vTargetPosition));
			ScalarV scDistSq = DistSquared(GetPed()->GetTransform().GetPosition(), vTargetPosition);
			static dev_float s_fMaxDistance = 30.0f;
			ScalarV scMaxDistSq = ScalarVFromF32(square(s_fMaxDistance));
			if(IsLessThanAll(scDistSq, scMaxDistSq))
			{
				//Set the flag.
				m_uRunningFlags.SetFlag(RF_HasSaidInitialAudio);

				if(pTargetVehicle->GetVehicleType() == VEHICLE_TYPE_CAR)
				{
					bool isMarkedAsCarByAudio = true;
					if(pTargetVehicle->GetVehicleAudioEntity())
					{
						audCarAudioEntity* carAudEnt = static_cast<audCarAudioEntity*>(pTargetVehicle->GetVehicleAudioEntity());
						if(carAudEnt && carAudEnt->GetCarAudioSettings())
						{
							isMarkedAsCarByAudio = (AUD_GET_TRISTATE_VALUE(carAudEnt->GetCarAudioSettings()->Flags, FLAG_ID_CARAUDIOSETTINGS_IAMNOTACAR) != AUD_TRISTATE_TRUE);
						}
					}
					if(isMarkedAsCarByAudio)
					{
						GetPed()->NewSay("STOP_VEHICLE_CAR_MEGAPHONE");
					}
					else
					{
						GetPed()->NewSay("STOP_VEHICLE_GENERIC_MEGAPHONE");
					}
				}
				else if(pTargetVehicle->InheritsFromBoat())
				{
					GetPed()->NewSay("STOP_VEHICLE_BOAT_MEGAPHONE");
				}
				else if(pTargetVehicle->GetVehicleType() != VEHICLE_TYPE_BICYCLE)
				{
					GetPed()->NewSay("STOP_VEHICLE_GENERIC_MEGAPHONE");
				}
			}
		}
		//Check if we have not said follow-up audio.
		else if(!m_uRunningFlags.IsFlagSet(RF_HasSaidFollowUpAudio))
		{
			//Set the flag.
			m_uRunningFlags.SetFlag(RF_HasSaidFollowUpAudio);

			if(pTargetVehicle->GetVehicleType() == VEHICLE_TYPE_CAR)
			{
				bool isMarkedAsCarByAudio = true;
				if(pTargetVehicle->GetVehicleAudioEntity())
				{
					audCarAudioEntity* carAudEnt = static_cast<audCarAudioEntity*>(pTargetVehicle->GetVehicleAudioEntity());
					if(carAudEnt && carAudEnt->GetCarAudioSettings())
					{
						isMarkedAsCarByAudio = (AUD_GET_TRISTATE_VALUE(carAudEnt->GetCarAudioSettings()->Flags, FLAG_ID_CARAUDIOSETTINGS_IAMNOTACAR) != AUD_TRISTATE_TRUE);
					}
				}
				if(isMarkedAsCarByAudio)
				{
					GetPed()->NewSay("STOP_VEHICLE_CAR_WARNING_MEGAPHONE");
				}
				else
				{
					GetPed()->NewSay("STOP_VEHICLE_GENERIC_WARNING_MEGAPHONE");
				}
			}
			else
			{
				GetPed()->NewSay("STOP_VEHICLE_GENERIC_WARNING_MEGAPHONE");
			}
		}
		else if(pTargetVehicle->GetVehicleType() != VEHICLE_TYPE_BICYCLE)
		{
			GetPed()->NewSay("CHASE_VEHICLE_MEGAPHONE");
		}
	}
	else
	{
		GetPed()->NewSay("VEHICLE_CHASE");
	}
}

void CTaskVehiclePersuit::ProcessBlockedByLockedDoor()
{
	if(!GetPed()->PopTypeIsRandom())
	{
		return;
	}

	if(!GetPed()->GetIsDrivingVehicle())
	{
		return;
	}

	m_uRunningFlags.ClearFlag(RF_IsBlockedByLockedDoor);

	if(m_uTimeCollidedWithLockedDoor > 0)
	{
		ScalarV scDistSq = DistSquared(GetPed()->GetTransform().GetPosition(), m_vPositionWhenCollidedWithLockedDoor);
		static dev_float s_fMinDistance = 10.0f;
		ScalarV scMinDistSq = ScalarVFromF32(square(s_fMinDistance));
		if(IsGreaterThanAll(scDistSq, scMinDistSq))
		{
			m_vPositionWhenCollidedWithLockedDoor.ZeroComponents();
			m_uTimeCollidedWithLockedDoor = 0;
		}
		else
		{
			static dev_u32 s_uMinTime = 15000;
			if(CTimeHelpers::GetTimeSince(m_uTimeCollidedWithLockedDoor) > s_uMinTime)
			{
				m_uRunningFlags.SetFlag(RF_IsBlockedByLockedDoor);
			}
		}
	}
	else if(IsCollidingWithLockedDoor())
	{
		m_vPositionWhenCollidedWithLockedDoor = GetPed()->GetTransform().GetPosition();
		m_uTimeCollidedWithLockedDoor = fwTimer::GetTimeInMilliseconds();
	}
	else
	{
		m_vPositionWhenCollidedWithLockedDoor.ZeroComponents();
		m_uTimeCollidedWithLockedDoor = 0;
	}
}

void CTaskVehiclePersuit::ProcessTimers()
{
	//Check if the target is moving slowly in a vehicle.
	if(IsTargetMovingSlowlyInVehicle())
	{
		m_fTimeTargetMovingSlowlyInVehicle += GetTimeStep();
	}
	else
	{
		m_fTimeTargetMovingSlowlyInVehicle = 0.0f;
	}
}

bool CTaskVehiclePersuit::ShouldAnyoneProvideCoverDuringExitVehicle() const
{
	//Ensure the ped is valid.
	const CPed* pPed = GetPed();
	if(!pPed)
	{
		return false;
	}
	
	//Ensure the vehicle is valid.
	const CVehicle* pVehicle = pPed->GetMyVehicle();
	if(!pVehicle)
	{
		return false;
	}
	
	//Ensure the seat manager is valid.
	const CSeatManager* pSeatManager = pVehicle->GetSeatManager();
	if(!pSeatManager)
	{
		return false;
	}
	
	//Check if any of the peds will provide cover.
	for(s32 i = 0; i < pSeatManager->GetMaxSeats(); ++i)
	{
		const CPed* pPassenger = pSeatManager->GetPedInSeat(i);
		if(!pPassenger)
		{
			continue;
		}
		
		//Check if the passenger will provide cover.
		fwMvClipSetId clipSetId;
		if(ShouldPedProvideCoverDuringExitVehicle(*pPassenger, clipSetId))
		{
			return true;
		}
	}
	
	return false;
}

bool CTaskVehiclePersuit::ShouldWaitWhileCruising() const
{
	TUNE_GROUP_BOOL(VEHICLE_PURSUIT, ALWAYS_ALLOW_CRUISE_WAITING, false);
	if(!ALWAYS_ALLOW_CRUISE_WAITING && !GetPed()->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CruiseAndBlockInVehicle))
	{
		return false;
	}

	//Ensure us and target are in a vehicle
	const CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
	const CVehicle* pTargetVehicle = GetTargetVehicle();
	if(!pVehicle || !pTargetVehicle)
	{
		return false;
	}

	//ensure we are in front of target and facing same direction as them
	if(HasTargetPassedUs() || !FacingSameDirectionAsTarget())
	{
		return false;
	}

	if(pTargetVehicle->GetVelocity().Mag2() < 100.0f)
	{
		return false;
	}

	//Ensure outside distance
	static dev_float s_fMaxDistance = 45.0f;
	ScalarV scDistSq = DistSquared(pVehicle->GetTransform().GetPosition(), pTargetVehicle->GetTransform().GetPosition());
	ScalarV scMaxDistSq = ScalarVFromF32(square(s_fMaxDistance));
	if(IsLessThanAll(scDistSq, scMaxDistSq) || WillTargetVehicleOvertakeUsWithinTime(1.0f))
	{
		return false;
	}

// 	//Check if the player is not on a highway.
// 	if(!pPed->GetPlayerInfo()->GetPlayerDataPlayerOnHighway())
// 	{
// 		//Ensure no one has the order.
// 		if(DoesAnyoneHaveOrder(CPoliceOrder::PO_WANTED_WAIT_CRUISING_AS_DRIVER, *pIncident))
// 		{
// 			return false;
// 		}
// 	}

	return true;
}

bool CTaskVehiclePersuit::ShouldAttack() const
{
	//Check if the flag is set.
	if(GetPed()->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_UseVehicleAttack))
	{
		return true;
	}

	//Check if the conditional flag is set.
	if(GetPed()->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_UseVehicleAttackIfVehicleHasMountedGuns))
	{
		//Check if the vehicle has mounted guns.
		if(HasMountedGuns())
		{
			return true;
		}
	}

	//Check if the vehicle is a plane.
	CVehicle* pMyVehicle = GetPed()->GetMyVehicle();
	if(pMyVehicle->InheritsFromPlane() && !pMyVehicle->HasContactWheels())
	{
		return true;
	}

	//Check if the vehicle is a tank.
	if(pMyVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_IS_TANK))
	{
		return true;
	}

	return false;
}

bool CTaskVehiclePersuit::ShouldChaseInCar() const
{
	//Ensure we are driving.
	if(!GetPed()->GetIsDrivingVehicle())
	{
		return false;
	}

	//Check if the target is in a vehicle.
	const CVehicle* pTargetVehicle = GetTargetVehicle();
	if(pTargetVehicle)
	{
		//Check if the target is in water.
		if(CTargetInWaterHelper::IsInWater(*pTargetVehicle))
		{
			return false;
		}

		//Check if the target has been moving slowly.
		float fSmoothedSpeedSq = pTargetVehicle->GetIntelligence()->GetSmoothedSpeedSq();
		static dev_float s_fMaxSmoothedSpeed = 1.0f;
		bool bIsTargetMovingSlowly = (fSmoothedSpeedSq < square(s_fMaxSmoothedSpeed));
		if(!bIsTargetMovingSlowly)
		{
			static dev_float s_fMinTimeMovingSlowly = 1.0f;
			bIsTargetMovingSlowly = (m_fTimeTargetMovingSlowlyInVehicle > s_fMinTimeMovingSlowly);
		}
		if(bIsTargetMovingSlowly)
		{
			//Check the target is not moving quickly.
			float fSpeedSq = pTargetVehicle->GetVelocity().XYMag2();
			static dev_float s_fMaxSpeed = 4.0f;
			if(fSpeedSq < square(s_fMaxSpeed))
			{
				return false;
			}
		}

		return true;
	}
	//Check if the target is on a mount.
	else if(IsTargetOnMount())
	{
		return true;
	}

	return false;
}

bool CTaskVehiclePersuit::ShouldCloseDoor() const
{
	//Check if the target is in front of us, and we won't have a gun after exiting the vehicle.
	if(IsTargetInFrontOfUs() && !WillHaveGunAfterExitingVehicle())
	{
		return true;
	}
	
	return false;
}

bool CTaskVehiclePersuit::ShouldEquipBestWeaponAfterExitingVehicle() const
{
	//Grab the ped.
	const CPed* pPed = GetPed();

	//Check if the ped is random, not law-enforcement, and is in a fist fight.
	if(pPed->PopTypeIsRandom() && !pPed->IsLawEnforcementPed() && IsInFistFight())
	{
		return false;
	}
	
	return true;
}

bool CTaskVehiclePersuit::ShouldExitBoat() const
{
	//Ensure we can exit the vehicle.
	if(!CanExitVehicle())
	{
		return false;
	}

	//Grab the vehicle.
	const CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
	if(!pVehicle)
	{
		return false;
	}
	
	//Ensure the vehicle is a boat.
	if(!pVehicle->InheritsFromBoat())
	{
		return false;
	}
	
	//Grab the boat.
	const CBoat* pBoat = static_cast<const CBoat *>(pVehicle);
	
	//Check if the time that the boat has been out of water exceeds the threshold.
	float fTimeOutOfWater = pBoat->m_BoatHandling.GetOutOfWaterTime();
	float fMinTimeOutOfWaterForExit = sm_Tunables.m_MinTimeBoatOutOfWaterForExit;
	if(fTimeOutOfWater >= fMinTimeOutOfWaterForExit)
	{
		return true;
	}

	//Check if the target is standing on our boat.
	const CPed* pTargetPed = GetTargetPed();
	if(pTargetPed && (pTargetPed->GetGroundPhysical() == pBoat))
	{
		return true;
	}

	return false;
}

bool CTaskVehiclePersuit::ShouldExitVehicleToAim() const
{
	//Ensure the parent task is valid.
	const CTask* pTask = GetParent();
	if(!pTask)
	{
		return false;
	}
	
	//Ensure we are in combat.
	if(pTask->GetTaskType() != CTaskTypes::TASK_COMBAT)
	{
		return false;
	}
	
	//Ensure we should equip our best weapon after exiting the vehicle.
	//Exiting the vehicle to aim will automatically equip our best weapon.
	if(!ShouldEquipBestWeaponAfterExitingVehicle())
	{
		return false;
	}
	
	return true;
}

bool CTaskVehiclePersuit::ShouldJumpOutOfVehicle() const
{
	//Grab the vehicle.
	const CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
	if(!pVehicle)
	{
		return false;
	}
	
	//Ensure our speed has exceeded the threshold.
	float fMinSpeedToJumpOutOfVehicle = sm_Tunables.m_MinSpeedToJumpOutOfVehicle;
	float fMinSpeedToJumpOutOfVehicleSq = square(fMinSpeedToJumpOutOfVehicle);
	float fSpeedSq = pVehicle->GetVelocity().Mag2();
	if(fSpeedSq < fMinSpeedToJumpOutOfVehicleSq)
	{
		return false;
	}
	
	return true;
}

bool CTaskVehiclePersuit::ShouldMakeEmergencyStopDueToAvoidanceArea() const
{
	//Ensure the ped is valid.
	const CPed* pPed = GetPed();
	if(!pPed)
	{
		return false;
	}
	
	//Ensure the ped is in a vehicle.
	const CVehicle* pVehicle = pPed->GetVehiclePedInside();
	if(!pVehicle)
	{
		return false;
	}
	
	//Ensure the ped is driving.
	if(!pVehicle->IsDriver(pPed))
	{
		return false;
	}
	
	//Grab the position.
	Vec3V vPosition = pVehicle->GetTransform().GetPosition();
	
	//Grab the velocity.
	Vec3V vVelocity = RCC_VEC3V(pVehicle->GetVelocity());
	
	//Grab the velocity direction.
	Vec3V vVelocityDirection = NormalizeFastSafe(vVelocity, Vec3V(V_ZERO));
	
	//Calculate the distance it will take for the vehicle to stop.
	ScalarV scDistanceToStop = ScalarVFromF32(GetDistToStopAtCurrentSpeed(*pVehicle));
	
	//Add the distance to the position in the direction of the velocity.
	vPosition = AddScaled(vPosition, vVelocityDirection, scDistanceToStop);
	
	//Check if the point is in the vehicle avoidance areas.
	return CVehicleCombatAvoidanceArea::IsPointInAreas(vPosition);
}

bool CTaskVehiclePersuit::ShouldPedProvideCoverDuringExitVehicle(const CPed& rPed, fwMvClipSetId& clipSetId) const
{
	//Ensure the ped is not injured.
	if(rPed.IsInjured())
	{
		return false;
	}
	
	//Grab the conditional clip set.
	const CVehicleClipRequestHelper* pClipRequestHelper = CTaskVehicleFSM::GetVehicleClipRequestHelper(&rPed);
	const CConditionalClipSet* pClipSet = pClipRequestHelper ? pClipRequestHelper->GetConditionalClipSet() : NULL;
	if(!pClipSet)
	{
		return false;
	}
	
	//Ensure the clipset is providing cover.
	if(!pClipSet->GetIsProvidingCover())
	{
		return false;
	}
	
	//Assign the clip set id.
	clipSetId = pClipSet->GetClipSetId();
	
	return true;
}

bool CTaskVehiclePersuit::ShouldWaitInCar() const
{
	//Ensure the target is valid.
	if(!m_target.GetIsValid())
	{
		return false;
	}

	//Ensure the target vehicle is valid.
	const CVehicle* pVehicle = GetTargetVehicle();
	if(!pVehicle)
	{
		return false;
	}

	//Ensure we are driving.
	if(!GetPed()->GetIsDrivingVehicle())
	{
		return false;
	}

	//Get the target position.
	Vec3V vTargetPosition;
	m_target.GetPosition(RC_VECTOR3(vTargetPosition));

	//Ensure we are close.
	ScalarV scDistSq = DistSquared(GetPed()->GetTransform().GetPosition(), vTargetPosition);
	static dev_float s_fMaxDistance = 25.0f;
	ScalarV scMaxDistSq = ScalarVFromF32(square(s_fMaxDistance));
	if(IsGreaterThanAll(scDistSq, scMaxDistSq))
	{
		return false;
	}

	//Check if we are not waiting.
	if(GetState() != state_waitInCar)
	{
		//Ensure the speed is slow.
		float fSpeedSq = pVehicle->GetVelocity().Mag2();
		static dev_float s_fMaxSpeed = 1.0f;
		float fMaxSpeedSq = square(s_fMaxSpeed);
		if(fSpeedSq > fMaxSpeedSq)
		{
			return false;
		}

		return true;
	}
	else
	{
		//Ensure the speed is slow.
		float fSpeedSq = pVehicle->GetVelocity().Mag2();
		static dev_float s_fMaxSpeed = 3.0f;
		float fMaxSpeedSq = square(s_fMaxSpeed);
		if(fSpeedSq > fMaxSpeedSq)
		{
			return false;
		}

		//Ensure the target hasn't deviated too much from their original position.
		ScalarV scTargetDeviationSq = DistSquared(vTargetPosition, m_vTargetPositionWhenWaitBegan);
		static dev_float s_fMaxTargetDeviation = 7.5f;
		ScalarV scMaxTargetDeviationSq = ScalarVFromF32(square(s_fMaxTargetDeviation));
		if(IsGreaterThanAll(scTargetDeviationSq, scMaxTargetDeviationSq))
		{
			return false;
		}

		return true;
	}
}

void CTaskVehiclePersuit::UpdateVehicleTaskForApproachTarget()
{
	//Grab the ped.
	CPed* pPed = GetPed();
	
	//Ensure we are in a vehicle.
	CVehicle* pVehicle = pPed->GetVehiclePedInside();
	if(!pVehicle)
	{
		return;
	}
	
	//Ensure we are driving the vehicle.
	if(!pPed->GetIsDrivingVehicle())
	{
		return;
	}
	
	//Ensure the vehicle task is valid.
	CTaskVehicleMissionBase* pVehicleTask = pVehicle->GetIntelligence()->GetActiveTask();
	if(!pVehicleTask)
	{
		return;
	}

	//just come to a stop and get out if we're trying to do a Uturn on a narrow road as big vehicle
	const bool bOnSmallRoad = pVehicle->GetIntelligence()->IsOnSmallRoad();
	const bool bBigVehicle = pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_BIG);
	const bool bDoingThreePointTurn = pVehicleTask->FindSubTaskOfType(CTaskTypes::TASK_VEHICLE_THREE_POINT_TURN) != NULL;
	if(bOnSmallRoad && bBigVehicle && bDoingThreePointTurn)
	{
		SetState(State_slowdownSafely);
		return;
	}
	
	//Keep track of the obstruction information.
	CTaskVehicleGoToPointWithAvoidanceAutomobile::ObstructionInformation obstructionInformation;
	
	//Find the avoidance task.
	CTaskVehicleGoToPointWithAvoidanceAutomobile* pAvoidanceTask = static_cast<CTaskVehicleGoToPointWithAvoidanceAutomobile *>(pVehicleTask->FindSubTaskOfType(CTaskTypes::TASK_VEHICLE_GOTO_POINT_WITH_AVOIDANCE_AUTOMOBILE));
	if(pAvoidanceTask)
	{
		//Copy the obstruction information.
		obstructionInformation = pAvoidanceTask->GetObstructionInformation();
		
		//Set the avoidance margin for other law enforcement vehicles.
		//This was added to allow law enforcement vehicles to space themselves out.
		pAvoidanceTask->SetAvoidanceMarginForOtherLawEnforcementVehicles(sm_Tunables.m_AvoidanceMarginForOtherLawEnforcementVehicles);
	}
	
	//Calculate the cruise speed.
	const float fInitialCruiseSpeed = sm_Tunables.m_ApproachTarget.m_CruiseSpeed;
	float fCruiseSpeed = fInitialCruiseSpeed;

	//Check if a collision with a law enforcement vehicle is imminent.
	static float s_fMinDistanceToSlowDown = 10.0f;
	static float s_fMaxDistanceToSlowDown = 20.0f;
	float fDistanceToSlowDown = pPed->GetRandomNumberInRangeFromSeed(s_fMinDistanceToSlowDown, s_fMaxDistanceToSlowDown);
	static float s_fMinTimeToSlowDown = 3.0f;
	static float s_fMaxTimeToSlowDown = 5.0f;
	float fTimeToSlowDown = pPed->GetRandomNumberInRangeFromSeed(s_fMinTimeToSlowDown, s_fMaxTimeToSlowDown);
	bool bWillCollideWithPed		= false;
	bool bWillCollideWithVehicle	= false;
	if(IsCollisionWithLawEnforcementImminent(fDistanceToSlowDown, fTimeToSlowDown, bWillCollideWithPed, bWillCollideWithVehicle))
	{
		if(bWillCollideWithPed)
		{
			fCruiseSpeed = Min(fCruiseSpeed, sm_Tunables.m_ApproachTarget.m_CruiseSpeedWhenObstructedByLawEnforcementPed);
		}

		if(bWillCollideWithVehicle)
		{
			fCruiseSpeed = Min(fCruiseSpeed, sm_Tunables.m_ApproachTarget.m_CruiseSpeedWhenObstructedByLawEnforcementVehicle);
		}
	}

	if(fCruiseSpeed == fInitialCruiseSpeed )
	{
		//Grab the ped position.
		Vec3V vPedPosition = pPed->GetTransform().GetPosition();

		//Grab the target position.
		Vec3V vTargetPosition = GetTargetPositionForEmergencyStop();

		//Check if the distance has exceeded the threshold.
		ScalarV scDistSq = DistSquared(vPedPosition, vTargetPosition);
		ScalarV scMaxDistSq = ScalarVFromF32(square(sm_Tunables.m_ApproachTarget.m_MaxDistanceToConsiderClose));
		if(IsLessThanAll(scDistSq, scMaxDistSq))
		{
			//Update the cruise speed.
			fCruiseSpeed = Min(fCruiseSpeed, sm_Tunables.m_ApproachTarget.m_CruiseSpeedWhenClose);

			//slow vehicles down more if there's already a large number of vehicles ahead of us
			s32 iNumVehicles = 0;
			s32 iNumPeds = 0;
			CountNumberVehiclesAndPedsNearTarget(iNumVehicles, iNumPeds);
			if(iNumVehicles >= sm_Tunables.m_ApproachTarget.m_MaxNumberVehiclesNearTarget || 
				iNumPeds >= sm_Tunables.m_ApproachTarget.m_MaxNumberPedsNearTarget)
			{
				fCruiseSpeed = Min(fCruiseSpeed, sm_Tunables.m_ApproachTarget.m_CruiseSpeedTooManyNearbyEntities);
			}
		}
	}

	//Massive hack to handle the creek situation in GTA V.
	const CPed* pTargetPed = GetTargetPed();
	if(pTargetPed && CTargetInWaterHelper::IsInWater(*pTargetPed) &&
		CCreekHackHelperGTAV::IsAffected(*pVehicle))
	{
		static dev_float s_fMaxSpeed = 8.0f;
		fCruiseSpeed = Min(fCruiseSpeed, s_fMaxSpeed);
	}
	
	//Set the cruise speed.
	pVehicleTask->SetCruiseSpeed(fCruiseSpeed);

	//Check if we can exit the vehicle.
	bool bCanExitVehicle = CanExitVehicle();
	pVehicleTask->SetDrivingFlag(DF_DontTerminateTaskWhenAchieved, !bCanExitVehicle);
}

bool CTaskVehiclePersuit::ShouldStopDueToBlockingLawVehicles() const
{
	//Ensure we are in a vehicle.
	const CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
	if(!pVehicle)
	{
		return false;
	}

	Vec3V vPedPosition = GetPed()->GetTransform().GetPosition();
	ScalarV fMaxDistSqr(sm_Tunables.m_ApproachTarget.m_DistanceToConsiderEntityBlocking*sm_Tunables.m_ApproachTarget.m_DistanceToConsiderEntityBlocking);
	Vec3V vUsToTarget = Normalize(GetTargetPed()->GetTransform().GetPosition() - vPedPosition );

	s32 iNumVehiclesBlocking = 0;
	CEntityScannerIterator vehicleList = pVehicle->GetIntelligence()->GetVehicleScanner().GetIterator();
	for( CEntity* pEntity = vehicleList.GetFirst(); pEntity; pEntity = vehicleList.GetNext() )
	{
		CVehicle* pVehicle = static_cast<CVehicle*>(pEntity);
		if(pVehicle && pVehicle->IsLawEnforcementCar() && pVehicle->GetVelocity().Mag2() < 1.0f)
		{
			Vec3V vToOTherPosition = pEntity->GetTransform().GetPosition() - vPedPosition;
			ScalarV fDist2ToVehicle = MagSquared(vToOTherPosition);
			if(IsLessThanAll(fDist2ToVehicle, fMaxDistSqr))
			{
				Vec3V vToOtherVehicle = Normalize(vToOTherPosition);
				if(IsGreaterThanAll(Dot(vToOtherVehicle, vUsToTarget), ScalarV(0.75f)))
				{
					++iNumVehiclesBlocking;
				}
			}
		}
	}

	return (iNumVehiclesBlocking >= 2);
}

void CTaskVehiclePersuit::CountNumberVehiclesAndPedsNearTarget(s32& iNumVehicles, s32& iNumPeds) const
{
	Vec3V vTargetPosition = GetTargetPositionForEmergencyStop();
	ScalarV fMaxDistSqr(sm_Tunables.m_ApproachTarget.m_DistanceToConsiderCloseEntitiesTarget*sm_Tunables.m_ApproachTarget.m_DistanceToConsiderCloseEntitiesTarget);

	iNumVehicles = 0;
	CEntityScannerIterator vehicleList = GetTargetPed()->GetPedIntelligence()->GetNearbyVehicles();
	for( CEntity* pEntity = vehicleList.GetFirst(); pEntity; pEntity = vehicleList.GetNext() )
	{
		ScalarV fDist2ToVehicle = DistSquared(vTargetPosition, pEntity->GetTransform().GetPosition());
		if(IsLessThanAll(fDist2ToVehicle, fMaxDistSqr))
		{
			++iNumVehicles;
		}
	}

	iNumPeds = 0;
	CEntityScannerIterator pedList = GetTargetPed()->GetPedIntelligence()->GetNearbyPeds();
	for( CEntity* pEntity = pedList.GetFirst(); pEntity; pEntity = pedList.GetNext() )
	{
		ScalarV fDist2ToPed = DistSquared(vTargetPosition, pEntity->GetTransform().GetPosition());
		if(IsLessThanAll(fDist2ToPed, fMaxDistSqr))
		{
			++iNumPeds;
		}
	}
}

bool CTaskVehiclePersuit::WillHaveGunAfterExitingVehicle() const
{
	//Grab the ped.
	const CPed* pPed = GetPed();
	
	//Check if the ped is armed with a gun.
	if(IsPedArmedWithGun(*pPed))
	{
		return true;
	}
	
	//Check if we will equip a gun after exiting the vehicle.
	if(ShouldEquipBestWeaponAfterExitingVehicle() && IsPedsBestWeaponAGun(*pPed))
	{
		return true;
	}
	
	return false;
}

bool CTaskVehiclePersuit::IsPedOnMovingVehicleOrTrain(const CPed* pPed)
{
	if(!pPed || pPed->GetIsInVehicle())
	{
		return false;
	}

	Vec3V vVelocity;
	CEntity* pTargetAttachedEntity = static_cast<CEntity*>(pPed->GetAttachParent());
	CPhysical* pTargetGroundPhysical = pPed->GetGroundPhysical();

	if( pTargetGroundPhysical && pTargetGroundPhysical->GetIsTypeVehicle() )
	{
		vVelocity = VECTOR3_TO_VEC3V(pTargetGroundPhysical->GetVelocity());
	}
	else if( pTargetAttachedEntity && pTargetAttachedEntity->GetIsTypeVehicle() &&
			 static_cast<CVehicle*>(pTargetAttachedEntity)->InheritsFromTrain() )
	{
		CTrain* pTrain = static_cast<CTrain*>(pTargetAttachedEntity);
		vVelocity = VECTOR3_TO_VEC3V(pTrain->GetVelocity());
	}
	else if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_RidingTrain))
	{
		vVelocity = VECTOR3_TO_VEC3V(pPed->GetVelocity());
	}
	else
	{
		return false;
	}

	ScalarV scMinTrainSpeedSq = LoadScalar32IntoScalarV(sm_Tunables.m_MinTargetStandingOnTrainSpeed);
	scMinTrainSpeedSq = (scMinTrainSpeedSq * scMinTrainSpeedSq);

	return IsGreaterThanAll(MagSquared(vVelocity), scMinTrainSpeedSq) != 0;
}

#if !__FINAL

void CTaskVehiclePersuit::Debug() const
{
#if DEBUG_DRAW
	//Call the base class version.
	CTask::Debug();
	
	//Check if the vehicle is making an emergency stop.
	if(GetState() == State_emergencyStop)
	{
		//Draw the obstruction capsules.
		for(u32 i = 0; i < m_uNumObstructionPoints; ++i)
		{
			grcDebugDraw::Capsule(m_avObstructionProbeStartPoints[i], m_avObstructionProbeEndPoints[i], m_avObstructionProbeRadii[i], Color_red, false);
		}
	}
#endif
}

const char * CTaskVehiclePersuit::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_start&&iState<=State_exit);
	static const char* aStateNames[] = 
	{
		"State_start",
		"State_fleeFromTarget",
		"State_WaitWhileCruising",
		"State_attack",
		"State_chaseInCar",
		"State_followInCar",
		"State_heliCombat",
		"State_subCombat",
		"State_boatCombat",
		"State_passengerInVehicle",
		"State_waitForBuddiesToEnter",
		"State_slowdownSafely",
		"State_emergencyStop",
		"State_exitVehicle",
		"State_exitVehicleImmediately",
		"State_provideCoverDuringExitVehicle",
		"State_approachTarget",
		"State_waitForVehicleExit",
		"State_waitInCar",
		"State_exit"
	};

	return aStateNames[iState];
}

#endif // !__FINAL

void CTaskVehiclePersuit::SetTarget( CAITarget& val )
{
	// If the target has changed, set the flag so the task can be restarted.
	if( val != m_target )
		m_uRunningFlags.SetFlag(RF_TargetChanged);

	m_target = val;

	taskFatalAssertf(m_target.GetEntity() != NULL, "CTaskVehiclePersuit requires a valid target entity!");
	taskFatalAssertf(m_target.GetEntity()->GetIsPhysical(), "CTaskVehiclePersuit requires a valid PHYSICAL target entity!");

	// If the subtask is vehicle combat, pass the target through
	if( GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_VEHICLE_COMBAT)
	{
		static_cast<CTaskVehicleCombat*>(GetSubTask())->SetTarget(m_target);
	}
}

u32								CTaskVehicleCombat::ms_uNextShootOutTireTimeGlobal = 0;
eWantedLevel					CTaskVehicleCombat::ms_TargetDriveByWantedLevel = WANTED_LEVEL4;
CTaskVehicleCombat::Tunables	CTaskVehicleCombat::ms_Tunables;

IMPLEMENT_COMBAT_TASK_TUNABLES(CTaskVehicleCombat, 0xfa437d84);

CTaskVehicleCombat::CTaskVehicleCombat( const CAITarget* pTarget, u32 uFlags, float fFireTolerance )
: m_flags(uFlags)
, m_uNextShootOutTireTime(0)
, m_uWeaponPrepareTime(0)
, m_uRequestWaitForClearShotTime(0)
, m_fFireTolerance(fFireTolerance)
{
	if(pTarget)
	{
		m_target = (*pTarget);
	}
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_COMBAT);
}

CTaskVehicleCombat::~CTaskVehicleCombat()
{

}

CTask::FSM_Return CTaskVehicleCombat::ProcessPreFSM()
{
	if (!m_target.GetIsValid() && !m_flags.IsFlagSet(Flag_useCamera))
	{
		return FSM_Quit;
	}
	
	return FSM_Continue;
}

CTask::FSM_Return CTaskVehicleCombat::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		// Initial state
		FSM_State(State_start)
			FSM_OnUpdate
				Start_OnUpdate(pPed);

		// Waiting for clear LOS to the target.
		FSM_State(State_waitForClearShot)
			FSM_OnEnter
				WaitForClearShot_OnEnter(pPed);
			FSM_OnUpdate
				return WaitForClearShot_OnUpdate(pPed);

		// Firing a mounted weapon.
		FSM_State(State_mountedWeapon)
			FSM_OnEnter
				MountedWeapon_OnEnter(pPed);
			FSM_OnUpdate
				return MountedWeapon_OnUpdate(pPed);

		// Firing a standard weapon from a vehicle
		FSM_State(State_vehicleGun)
			FSM_OnEnter
				VehicleGun_OnEnter(pPed);
			FSM_OnUpdate
				return VehicleGun_OnUpdate(pPed);

		// Throwing a projectile
		FSM_State(State_projectile)
			FSM_OnEnter
				Projectile_OnEnter(pPed);
			FSM_OnUpdate
				return Projectile_OnUpdate(pPed);
				
		// Shooting out a tire
		FSM_State(State_shootOutTire)
			FSM_OnEnter
				ShootOutTire_OnEnter(pPed);
			FSM_OnUpdate
				return ShootOutTire_OnUpdate(pPed);

		// Exit state
		FSM_State(State_exit)
			FSM_OnUpdate
	FSM_End
}

#if !__FINAL
const char * CTaskVehicleCombat::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_start&&iState<=State_exit);
	static const char* aStateNames[] = 
	{
		"State_start",
		"State_waitForClearShot",
		"State_mountedWeapon",
		"State_vehicleGun",
		"State_projectile",
		"State_shootOutTire",
		"State_exit"
	};

	return aStateNames[iState];
}
#endif //_FINAL

CTask::FSM_Return CTaskVehicleCombat::Start_OnUpdate( CPed* pPed )
{
	//Check if the next shoot out tire time global has not been initialized.
	if(ms_uNextShootOutTireTimeGlobal == 0)
	{
		//Generate the next shoot out tire time global.
		ms_uNextShootOutTireTimeGlobal = CTaskVehicleCombat::GenerateNextShootOutTireTimeGlobal();
	}
	
	//Generate the next shoot out tire time.
	m_uNextShootOutTireTime = GenerateNextShootOutTireTime();

	//Add a generic delay before ped can start attacking (B* 1386704)
	if (!pPed->PopTypeIsMission())
		m_uWeaponPrepareTime = fwTimer::GetTimeInMilliseconds() + (u32)((fwRandom::GetRandomNumberInRange(ms_Tunables.m_MinTimeToPrepareWeapon, ms_Tunables.m_MaxTimeToPrepareWeapon) * 1000.0f));
	
	InitialiseTargetingSystems(pPed);
	SetState(State_waitForClearShot);
	return FSM_Continue;
}

void CTaskVehicleCombat::WaitForClearShot_OnEnter( CPed* pPed )
{
	CVehicleWeapon* pMountedWeapon = pPed->GetWeaponManager()->GetEquippedVehicleWeapon();
	if(pMountedWeapon && pMountedWeapon->GetType() == VGT_RADAR)
	{
		u32 uWeaponHash = pPed->GetWeaponManager()->GetWeaponSelector()->GetBestPedWeapon(pPed, CWeaponInfoManager::GetSlotListBest(), false);
		if(uWeaponHash)
		{
			pPed->GetWeaponManager()->EquipWeapon(uWeaponHash, -1, true);
		}
	}

	// Reset the target changed flag, this task is reverted to during a target switch
	m_flags.ClearFlag(Flag_targetChanged);
}

CTask::FSM_Return CTaskVehicleCombat::WaitForClearShot_OnUpdate( CPed* pPed )
{
	// Reset the target changed flag, this task is reverted to during a target switch
	m_flags.ClearFlag(Flag_targetChanged);

	// Check the desired state, switching states if its changed
	VehicleCombatState firingState = GetDesiredState(pPed);
	if( firingState != GetState() )
	{
		if (firingState == State_vehicleGun || firingState == State_mountedWeapon) 
		{	//Adding some delays to control task oscillations, B* 738133
			static dev_float suMinAimTimeBeforeShootingAgain = 1.0f;
			if (GetPreviousState() != State_start && GetTimeInState() < suMinAimTimeBeforeShootingAgain)
				return FSM_Continue;
		}
		SetState(firingState);
		return FSM_Continue;
	}
	return FSM_Continue;
}

void CTaskVehicleCombat::MountedWeapon_OnEnter( CPed* pPed )
{
	CTaskVehicleMountedWeapon::eTaskMode mode = CTaskVehicleMountedWeapon::Mode_Fire;
	if(m_flags.IsFlagSet(Flag_useCamera))
	{
		mode = CTaskVehicleMountedWeapon::Mode_Camera;
	}
	else if( m_flags.IsFlagSet(Flag_justAim) || !CanFireWithMountedWeapon(pPed) )
	{
		mode = CTaskVehicleMountedWeapon::Mode_Aim;
	}
	CTaskVehicleMountedWeapon* pNewTask = rage_new CTaskVehicleMountedWeapon(mode, &m_target, m_fFireTolerance);
	SetNewTask(pNewTask);
}

CTask::FSM_Return CTaskVehicleCombat::MountedWeapon_OnUpdate( CPed* pPed )
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		// Go back to basic drive task
		SetState(State_waitForClearShot);
		return FSM_Continue;
	}

	// See if we want to change state
	s32 iDesiredState = GetDesiredState(pPed);
	if(iDesiredState != GetState())
	{
		SetState(iDesiredState);
		return FSM_Continue;
	}

	taskFatalAssertf(GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_VEHICLE_MOUNTED_WEAPON, "vehicle mounted weapon subtask expected!" );
	CTaskVehicleMountedWeapon* pMountedWeaponTask = static_cast<CTaskVehicleMountedWeapon*>(GetSubTask());
	// Check LOS to determine whether the AI should be aiming or firing
	if( !m_flags.IsFlagSet(Flag_useCamera) )
	{
		if( !m_flags.IsFlagSet(Flag_justAim) && CheckTargetingLOS(pPed) && CanFireWithMountedWeapon(pPed) )
		{
			pMountedWeaponTask->SetMode(CTaskVehicleMountedWeapon::Mode_Fire);
		}
		else
		{
			pMountedWeaponTask->SetMode(CTaskVehicleMountedWeapon::Mode_Aim);
		}
	}
	return FSM_Continue;
}

void CTaskVehicleCombat::VehicleGun_OnEnter( CPed* UNUSED_PARAM(pPed) )
{
	m_uRequestWaitForClearShotTime = 0;
	CTaskVehicleGun::eTaskMode mode = CTaskVehicleGun::Mode_Fire;
	if(m_flags.IsFlagSet(Flag_justAim))
	{
		mode = CTaskVehicleGun::Mode_Aim;
	}
	CTaskVehicleGun* pNewTask = rage_new CTaskVehicleGun(mode, ATSTRINGHASH("FIRING_PATTERN_BURST_FIRE_DRIVEBY", 0x0d31265f2), &m_target);
	SetNewTask(pNewTask);
}

CTask::FSM_Return CTaskVehicleCombat::VehicleGun_OnUpdate( CPed* pPed )
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		// Go back to basic drive task
		SetState(State_waitForClearShot);
		return FSM_Continue;
	}

	taskFatalAssertf(GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_VEHICLE_GUN, "Vehicle gun subtask expected!");

	// See if we want to change state
	s32 iDesiredState = GetIsFlagSet(aiTaskFlags::TerminationRequested) ? State_exit : GetDesiredState(pPed);
	if(iDesiredState != GetState())
	{
		bool bAttemptTerminatation = true;
		bool bDriveByAllowed = m_flags.IsFlagSet(Flag_forceAllowDriveBy) || pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanDoDrivebys);
		if (iDesiredState == State_waitForClearShot && bDriveByAllowed)
		{
			static dev_u32 suMinAimTimeBeforeWaitForClearShot = 2000;
			//Adding some delay to control task oscillations, B* 738133
			if (!m_uRequestWaitForClearShotTime)
			{
				m_uRequestWaitForClearShotTime = fwTimer::GetTimeInMilliseconds();
				bAttemptTerminatation = false;
			} 
			else if ((m_uRequestWaitForClearShotTime + suMinAimTimeBeforeWaitForClearShot) > fwTimer::GetTimeInMilliseconds())
			{
				bAttemptTerminatation = false;
			}
		}

		if (bAttemptTerminatation)
		{	
			if( GetSubTask()->SupportsTerminationRequest() )
			{
				GetSubTask()->RequestTermination();
			}
			else if(GetSubTask()->MakeAbortable(ABORT_PRIORITY_URGENT,NULL))
			{
				// We aborted instantly so change state
				SetState(iDesiredState);
			}
		}
	}
	else
		m_uRequestWaitForClearShotTime = 0;

	return FSM_Continue;
}

void CTaskVehicleCombat::Projectile_OnEnter( CPed* UNUSED_PARAM(pPed) )
{
	CTaskVehicleProjectile* pNewTask = rage_new CTaskVehicleProjectile(CTaskVehicleProjectile::Mode_Fire);
	SetNewTask(pNewTask);
}

CTask::FSM_Return CTaskVehicleCombat::Projectile_OnUpdate( CPed* pPed )
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		// Go back to basic drive task
		SetState(State_waitForClearShot);
		return FSM_Continue;
	}

	// See if we want to change state
	s32 iDesiredState = GetDesiredState(pPed);
	if(iDesiredState != GetState())
	{
		SetState(iDesiredState);
		return FSM_Continue;
	}

	return FSM_Continue;
}

void CTaskVehicleCombat::ShootOutTire_OnEnter( CPed* UNUSED_PARAM(pPed) )
{
	//Generate the next shoot out tire time global.
	ms_uNextShootOutTireTimeGlobal = GenerateNextShootOutTireTimeGlobal();
	
	//Generate the next shoot out tire time.
	m_uNextShootOutTireTime = GenerateNextShootOutTireTime();
	
	//Create the task.
	CTask* pTask = rage_new CTaskShootOutTire(m_target, ShouldApplyReactionWhenShootingOutTire() ? CTaskShootOutTire::SOTCF_ApplyReactionToVehicle : 0);
	
	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskVehicleCombat::ShootOutTire_OnUpdate( CPed* UNUSED_PARAM(pPed) )
{
	//Wait for the task to finish.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Wait for a clear shot.
		SetState(State_waitForClearShot);
	}
	
	return FSM_Continue;
}

void CTaskVehicleCombat::InitialiseTargetingSystems( CPed* pPed )
{
	// We cant use the targeting systems if the target is a world co-ordinate
	if( m_target.GetEntity() )
	{
		// Start up the peds targeting systems and register the current target if valid
		CPedTargetting* pPedTargetting = pPed->GetPedIntelligence()->GetTargetting( true );
		if( pPedTargetting )
		{
			if( !pPedTargetting->FindTarget(m_target.GetEntity()) )
			{
				pPedTargetting->RegisterThreat(m_target.GetEntity());
			}
		}
	}
}

bool CTaskVehicleCombat::CheckTargetingLOS( CPed* pPed )
{
	// Always start checking LOS 
	if( m_target.GetEntity() == NULL )
	{
		return true;
	}
	
	CPedTargetting* pPedTargetting = pPed->GetPedIntelligence()->GetTargetting( true );
	if( pPedTargetting )
	{
		pPedTargetting->SetInUse();
		if( pPedTargetting->GetLosStatus(m_target.GetEntity()) == Los_clear )
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	taskErrorf("Should never get here!");
	return false;
}

bool CTaskVehicleCombat::CanFireWithMountedWeapon( CPed* pPed )
{
	if (!pPed->GetIsDrivingVehicle() && pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_BlockFireForVehiclePassengerMountedGuns))
	{
		return false;
	}

	return true;
}

CTaskVehicleCombat::VehicleCombatState CTaskVehicleCombat::GetDesiredState( CPed* pPed )
{
	// If the target has changed, always transition back to waiting in the vehicle state
	// forcing the task to restart with the new target
	if( m_flags.IsFlagSet(Flag_targetChanged))
	{
		return State_waitForClearShot;
	}

	// If the AI is in a mounted weapon position, use this always
	// The decision to aim or fire is made in the mounted weapon update so LOS doesn't need to be checked here.
	weaponAssert(pPed->GetWeaponManager() && pPed->GetInventory());
	const bool bHasWeaponsAtSeat = pPed->GetMyVehicle()->GetSeatHasWeaponsOrTurrets(pPed->GetMyVehicle()->GetSeatManager()->GetPedsSeatIndex(pPed));
	if(bHasWeaponsAtSeat && pPed->GetWeaponManager()->GetEquippedVehicleWeaponIndex() != -1)
	{
		return State_mountedWeapon;
	}

	// Check can do drive by conditions
	bool bCanDoDrivebys = m_flags.IsFlagSet(Flag_forceAllowDriveBy) || pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanDoDrivebys);
	if(!bCanDoDrivebys && pPed->ShouldBehaveLikeLaw() && !pPed->PopTypeIsMission())
	{
		const CEntity* pTargetEntity = m_target.GetEntity();
		if(pTargetEntity)
		{
			// If in MP and our target ped has a particular wanted level (tweakable via cloud tunable)
			if(NetworkInterface::IsGameInProgress() && pTargetEntity->GetIsTypePed())
			{
				const CPed* pTargetPed = static_cast<const CPed*>(pTargetEntity);
				if(pTargetPed->GetPlayerWanted() && pTargetPed->GetPlayerWanted()->GetWantedLevel() >= ms_TargetDriveByWantedLevel)
				{
					bCanDoDrivebys = true;
				}
			}

			// If the above logic didn't succeed then see if the target has been hostile towards us recently
			if(!bCanDoDrivebys)
			{
				const CPedTargetting* pPedTargeting = pPed->GetPedIntelligence()->GetTargetting(false);
				if(pPedTargeting)
				{
					const CSingleTargetInfo* pTargetInfo = pPedTargeting->FindTarget(pTargetEntity);
					if(pTargetInfo && pTargetInfo->GetTimeOfLastHostileAction() > fwTimer::GetTimeInMilliseconds() - ms_Tunables.m_MaxTimeSinceTargetLastHostileForLawDriveby)
					{
						bCanDoDrivebys = true;
					}
				}
			}
		}
	}

	// If there is a clear LOS to the target - try to start an attacking vehicle task
	if( bCanDoDrivebys && CheckTargetingLOS(pPed) )
	{	
		//Ensure the timer has exceeded the threshold.
		if(fwTimer::GetTimeInMilliseconds() < m_uWeaponPrepareTime)
		{
			return State_waitForClearShot;
		}

		// Check for driveby weapons, start a driveby task if one is valid
		const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(pPed->GetWeaponManager()->GetEquippedWeaponHash());
		if( pWeaponInfo )
		{
			const bool bIsOutOfAmmo = pPed->GetInventory()->GetIsWeaponOutOfAmmo(pWeaponInfo);
			const bool bHasDrivebyWeapons = pWeaponInfo->CanBeUsedAsDriveBy(pPed) && !bIsOutOfAmmo && 
					   (!pWeaponInfo->GetIsUnarmed() || pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanTauntInVehicle));

			
			if( bHasDrivebyWeapons )
			{
				// Check if we are restricted to shooting at peds that are on the same side of the vehicle as we are
				const CVehicle* pMyVehicle = pPed->GetMyVehicle();
				if(pMyVehicle && pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_RestrictInVehicleAimingToCurrentSide))
				{
					const Vec3V vVehiclePosition = pMyVehicle->GetTransform().Transform(Average(VECTOR3_TO_VEC3V(pMyVehicle->GetBoundingBoxMin()), VECTOR3_TO_VEC3V(pMyVehicle->GetBoundingBoxMax())));
					Vector3 vTargetPosition;
					m_target.GetPosition(vTargetPosition);

					const ScalarV scVehiclePedDot = Dot(pMyVehicle->GetTransform().GetRight(), pPed->GetTransform().GetPosition() - vVehiclePosition);
					const ScalarV scVehicleTargetDot = Dot(pMyVehicle->GetTransform().GetRight(), VECTOR3_TO_VEC3V(vTargetPosition) - pPed->GetTransform().GetPosition());
					if(IsGreaterThanAll(scVehiclePedDot, ScalarV(V_ZERO)) != IsGreaterThanAll(scVehicleTargetDot, ScalarV(V_ZERO)))
					{
						return State_waitForClearShot;
					}
				}

				// Check for throwing grenades
				if( pWeaponInfo->GetIsThrownWeapon() )
				{
					return State_projectile;
				} //Check if the ped can shoot out a tire.				
				else if(CanShootOutTire())
				{
					return State_shootOutTire;
				}
				else
				{
					return State_vehicleGun;
				}
			}

		}
		return State_waitForClearShot;
	}
	// No clear los - go back to being seated
	else
	{
		return State_waitForClearShot;
	}
}

void CTaskVehicleCombat::SetTarget( CAITarget& val )
{
	// If the target has changed, set the flag so the task can be restarted.
	if( val != m_target )
		m_flags.SetFlag(Flag_targetChanged);

	m_target = val;
}

void CTaskVehicleCombat::InitTunables()
{
	static int s_iDefaultTargetWantedLevel = static_cast<int>(ms_TargetDriveByWantedLevel);
	ms_TargetDriveByWantedLevel = static_cast<eWantedLevel>(::Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("TARGET_DRIVEBY_WANTED_LEVEL", 0xa12169c4), s_iDefaultTargetWantedLevel));
}

bool CTaskVehicleCombat::CanShootOutTire() const
{
	return false;

	/*
	//Ensure the global timer has has exceeded the threshold.
	u32 uTime = fwTimer::GetTimeInMilliseconds();
	if(uTime < ms_uNextShootOutTireTimeGlobal)
	{
		return false;
	}
	
	//Ensure the timer has exceeded the threshold.
	if(uTime < m_uNextShootOutTireTime)
	{
		return false;
	}

	//Ensure the ped is valid.
	const CPed* pPed = GetPed();
	if(!pPed)
	{
		return false;
	}

	//Ensure the ped is a swat member.
	if(pPed->GetPedType() != PEDTYPE_SWAT)
	{
		return false;
	}

	//Ensure the ped is in a vehicle.
	const CVehicle* pVehicle = pPed->GetVehiclePedInside();
	if(!pVehicle)
	{
		return false;
	}

	//Ensure the vehicle is a helicopter.
	if(!pVehicle->InheritsFromHeli())
	{
		return false;
	}

	//Ensure the target is valid.
	const CEntity* pEntity = m_target.GetEntity();
	if(!pEntity)
	{
		return false;
	}

	//Ensure the entity is a ped.
	if(!pEntity->GetIsTypePed())
	{
		return false;
	}

	//Ensure the target is in a vehicle.
	const CPed* pTarget = static_cast<const CPed *>(pEntity);
	if(!pTarget->GetVehiclePedInside())
	{
		return false;
	}

	//Ensure the target is a player.
	if(!pTarget->IsPlayer())
	{
		return false;
	}

	return true;
	*/
}

const CPed* CTaskVehicleCombat::GetTargetPed() const
{
	//Ensure the entity is valid.
	const CEntity* pEntity = m_target.GetEntity();
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

const CVehicle* CTaskVehicleCombat::GetTargetVehicle() const
{
	//Ensure the target ped is valid.
	const CPed* pPed = GetTargetPed();
	if(!pPed)
	{
		return NULL;
	}
	
	return pPed->GetVehiclePedInside();
}

bool CTaskVehicleCombat::ShouldApplyReactionWhenShootingOutTire() const
{
	//Ensure the random number is within the threshold.
	float fRandom = fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
	float fMaxRandom = ms_Tunables.m_ChancesToApplyReactionWhenShootingOutTire;
	if(fRandom >= fMaxRandom)
	{
		return false;
	}
	
	return true;
}

u32 CTaskVehicleCombat::GenerateNextShootOutTireTime()
{
	return fwTimer::GetTimeInMilliseconds() + (u32)((fwRandom::GetRandomNumberInRange(ms_Tunables.m_MinTimeInCombatToShootOutTires, ms_Tunables.m_MaxTimeInCombatToShootOutTires) * 1000.0f));
}

u32 CTaskVehicleCombat::GenerateNextShootOutTireTimeGlobal()
{
	return fwTimer::GetTimeInMilliseconds() + (u32)((fwRandom::GetRandomNumberInRange(ms_Tunables.m_MinTimeBetweenShootOutTiresGlobal, ms_Tunables.m_MaxTimeBetweenShootOutTiresGlobal) * 1000.0f));
}
