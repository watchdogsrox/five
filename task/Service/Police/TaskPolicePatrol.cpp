// FILE :    TaskPolicePatrol.cpp
// PURPOSE : Basic patrolling task of a police officer, patrol in vehicle or on foot until given specific orders by dispatch
// CREATED : 25-05-2009

// File header
#include "Task/Service/Police/TaskPolicePatrol.h"

// Framework headers
#include "ai/task/taskchannel.h"

// Game headers
#include "animation/MovePed.h"
#include "audio/policescanner.h"
#include "Debug/DebugScene.h"
#include "Debug/VectorMap.h"
#include "Game/Dispatch/Incidents.h"
#include "Game/Dispatch/Orders.h"
#include "Game/Dispatch/DispatchServices.h"
#include "Game/Dispatch/DispatchManager.h"
#include "ModelInfo/VehicleModelInfo.h"
#include "Peds/Ped.h"
#include "Peds/PedGeometryAnalyser.h"
#include "Peds/PedIntelligence.h"
#include "script/script_cars_and_peds.h"
#include "Task/Combat/TaskCombat.h"
#include "Task/Combat/TaskCombatMelee.h"
#include "Task/Combat/TaskNewCombat.h"
#include "Task/Combat/TaskThreatResponse.h"
#include "Task/Combat/Subtasks/TaskShootAtTarget.h"
#include "Task/Combat/Subtasks/TaskVehicleCombat.h"
#include "Task/Crimes/RandomEventManager.h"
#include "Task/Crimes/TaskPursueCriminal.h"
#include "Task/Crimes/TaskReactToPursuit.h"
#include "Task/Default/TaskPlayer.h"
#include "task/Default/TaskUnalerted.h"
#include "Task/Default/TaskWander.h"
#include "Task/General/TaskBasic.h"
#include "Task/Movement/TaskNavMesh.h"
#include "Task/Movement/TaskGoTo.h"
#include "Task/Movement/TaskGoToPointAiming.h"
#include "Task/Physics/TaskBlendFromNM.h"
#include "Task/Physics/TaskNMRelax.h"
#include "Task/Vehicle/TaskCar.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Task/Vehicle/TaskExitVehicle.h"
#include "Task/Vehicle/TaskVehicleBase.h"
#include "Task/Weapons/TaskPlayerWeapon.h"
#include "task/Weapons/TaskSwapWeapon.h"
#include "Task/Weapons/Gun/TaskAimGunOnFoot.h"
#include "VehicleAI/driverpersonality.h"
#include "VehicleAi/pathfind.h"
#include "vehicleAi/task/TaskVehicleApproach.h"
#include "VehicleAI/task/TaskVehicleCruise.h"
#include "VehicleAI/task/TaskVehicleGoToAutomobile.h"
#include "vehicleAi/task/TaskVehicleTempAction.h"
#include "Vehicles/Metadata/VehicleLayoutInfo.h"
#include "Vehicles/Metadata/VehicleSeatInfo.h"
#include "Weapons/Projectiles/Projectile.h"

AI_OPTIMISATIONS()

CTaskPolice::CTaskPolice(CTaskUnalerted* pUnalertedTask)
: m_pUnalertedTask(pUnalertedTask)
, m_pPointToReturnTo(NULL)
, m_iPointToReturnToRealTypeIndex(-1)
, m_ExitToAimClipSetId(CLIP_SET_ID_INVALID)
, m_SPC_UseInfo(NULL)
, m_bForceStartOnAbort(false)
{
	SetInternalTaskType(CTaskTypes::TASK_POLICE);
}

CTaskPolice::~CTaskPolice()
{
	//Free the unalerted task.
	CTask* pUnalertedTask = m_pUnalertedTask.Get();
	delete pUnalertedTask;

	m_SPC_UseInfo = NULL;//forget this ... 
}

aiTask* CTaskPolice::Copy() const
{
	//Keep track of the copy of the unalerted task.
	CTaskUnalerted* pUnalertedTaskCopy = NULL;
	
	//Check if the unalerted task exists.
	CTask* pUnalertedTask = m_pUnalertedTask;
	if(pUnalertedTask)
	{
		//Check if the task is the correct type.
		if(taskVerifyf(pUnalertedTask->GetTaskType() == CTaskTypes::TASK_UNALERTED, "The unalerted task type is invalid: %d.", pUnalertedTask->GetTaskType()))
		{
			//Copy the task.
			pUnalertedTaskCopy = static_cast<CTaskUnalerted *>(pUnalertedTask->Copy());
		}
	}
	
	//Create the task.
	CTaskPolice* pTask = rage_new CTaskPolice(pUnalertedTaskCopy);
	pTask->m_bForceStartOnAbort = m_bForceStartOnAbort;
	
	return pTask;
}

CScenarioPoint* CTaskPolice::GetScenarioPoint() const
{
	// Check the unalerted subtask.
	const CTask* pSubTask = GetSubTask();
	if(pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_UNALERTED)
	{
		return pSubTask->GetScenarioPoint();
	}
	else
	{
		// Check the cached unalerted task.
		const CTask* pUnalertedTask = m_pUnalertedTask.Get();
		if(pUnalertedTask)
		{
			return pUnalertedTask->GetScenarioPoint();
		}
	}

	// No unalerted task, cached or otherwise.
	return NULL;
}


int CTaskPolice::GetScenarioPointType() const
{
	int type = -1;
	// Check the unalerted subtask.
	const CTask* pSubTask = GetSubTask();
	if(pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_UNALERTED)
	{
		type = pSubTask->GetScenarioPointType();
	}
	else
	{
		// Check the cached unalerted task.
		const CTask* pUnalertedTask = m_pUnalertedTask.Get();
		if(pUnalertedTask)
		{
			type = pUnalertedTask->GetScenarioPointType();
		}
	}

	return type;
}

// PURPOSE: Handle aborting the task
void CTaskPolice::DoAbort(const AbortPriority UNUSED_PARAM(priority), const aiEvent* pEvent)
{
	if (pEvent)
	{
		if (pEvent->GetEventType() == EVENT_SHOCKING_DEAD_BODY)
		{
			// We want to go to the start state instead of resuming.
			// As it looks pretty dumb to go back to your old position and stand next to a dead body.
			m_bForceStartOnAbort = true;
			// Reset all of the cached scenario state.
			m_pUnalertedTask = NULL;
			m_pPointToReturnTo = NULL;
			m_iPointToReturnToRealTypeIndex = Scenario_Invalid;
			m_SPC_UseInfo = NULL;
		}
	}
}

CTask::FSM_Return CTaskPolice::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	FSM_Begin
		
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();
			FSM_OnExit
				Start_OnExit();

		FSM_State(State_Resume)
			FSM_OnUpdate
				return Resume_OnUpdate();
				
		FSM_State(State_Unalerted)
			FSM_OnEnter
				Unalerted_OnEnter();
			FSM_OnUpdate
				return Unalerted_OnUpdate();

		FSM_State(State_EnterVehicle)
			FSM_OnEnter
				EnterVehicle_OnEnter();
			FSM_OnUpdate
				return EnterVehicle_OnUpdate();

		FSM_State(State_WaitForBuddiesToEnterVehicle)
			FSM_OnEnter
				WaitForBuddiesToEnterVehicle_OnEnter();
			FSM_OnUpdate
				return WaitForBuddiesToEnterVehicle_OnUpdate();

		FSM_State(State_PatrolOnFoot)
			FSM_OnEnter
				PatrolOnFoot_OnEnter();
			FSM_OnUpdate
				return PatrolOnFoot_OnUpdate();
			FSM_OnExit
				PatrolOnFoot_OnExit();

		FSM_State(State_PatrolInVehicleAsDriver)
			FSM_OnEnter
				PatrolInVehicleAsDriver_OnEnter();
			FSM_OnUpdate
				return PatrolInVehicleAsDriver_OnUpdate();

		FSM_State(State_PatrolInVehicleAsPassenger)
			FSM_OnEnter
				PatrolInVehicleAsPassenger_OnEnter();
			FSM_OnUpdate
				return PatrolInVehicleAsPassenger_OnUpdate();

		FSM_State(State_PatrolInVehicleFastAsDriver)
			FSM_OnEnter
				PatrolInVehicleFastAsDriver_OnEnter();
			FSM_OnUpdate
				return PatrolInVehicleFastAsDriver_OnUpdate();
			FSM_OnExit
				return PatrolInVehicleFastAsDriver_OnExit();

		FSM_State(State_RespondToOrder)
			FSM_OnEnter
				RespondToOrder_OnEnter();
			FSM_OnUpdate
				return RespondToOrder_OnUpdate();
			FSM_OnExit
				RespondToOrder_OnExit();

		FSM_State(State_PursueCriminal)
			FSM_OnEnter
				PursueCriminal_OnEnter();
			FSM_OnUpdate
				return PursueCriminal_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskPolice::ProcessPostFSM()
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

CTask::FSM_Return CTaskPolice::Start_OnUpdate()
{
	CPed* pPed = GetPed();

	// Find out which clip set to request based on our equipped weapon or best weapon and then request it
	m_ExitToAimClipSetId = CTaskVehicleFSM::GetExitToAimClipSetIdForPed(*pPed);
	m_VehicleClipRequestHelper.SetPed(pPed);

	// Reset the crouch status
	pPed->SetIsCrouching(false);
	
	//Check if we have no unalerted task.
	if(!m_pUnalertedTask)
	{
		//Check if we have a point to return to.
		if(m_pPointToReturnTo)
		{
			// initialize with default condition data
			CScenarioCondition::sScenarioConditionData conditionData;
			conditionData.pPed = pPed;

			// if we can return to the cached point
			if (!CTaskUnalerted::CanScenarioBeReused(*m_pPointToReturnTo, conditionData, m_iPointToReturnToRealTypeIndex, m_SPC_UseInfo))
			{
				//cant return to this point so just reset the data ... 
				m_pPointToReturnTo = NULL;
				m_iPointToReturnToRealTypeIndex = -1;
				m_SPC_UseInfo = NULL;
			}

			//Create the unalerted task.
			CTaskUnalerted* task = rage_new CTaskUnalerted(NULL, m_pPointToReturnTo, m_iPointToReturnToRealTypeIndex);
			task->SetScenarioChainUserInfo(m_SPC_UseInfo);
			m_pUnalertedTask = task;
		}
	}
	
	//Check if we have an unalerted task.
	if(m_pUnalertedTask)
	{
		SetState(State_Unalerted);
		return FSM_Continue;
	}

	const COrder* pOrder = pPed->GetPedIntelligence()->GetOrder();
	if (pOrder && pOrder->GetIsValid())
	{
		SetState(State_RespondToOrder);
	}
	else
	{
		if (pPed->GetIsInVehicle())
		{
			//In the network Game Cop respawn peds can be swapwed with the player and we dont want that ped to patrol in that vehicle.
			if (!pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanUseVehicles))
			{
				SetState(State_PatrolOnFoot);
				return FSM_Continue;
			}
			else if (pPed->GetIsDrivingVehicle())
			{
				SetDriveState(pPed);
				return FSM_Continue;
			}
			else
			{
				SetState(State_PatrolInVehicleAsPassenger);
				return FSM_Continue;
			}
		}
		else
		{
			CVehicle* pMyVehicle = pPed->GetMyVehicle();
			if(pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanUseVehicles) &&
			   pMyVehicle && pMyVehicle->IsInDriveableState() && !CBoatIsBeachedCheck::IsBoatBeached(*pMyVehicle))
			{
				SetState(State_EnterVehicle);
				return FSM_Continue;
			}

			SetState(State_PatrolOnFoot);
			return FSM_Continue;
		}
	}
	return FSM_Continue;
}

void CTaskPolice::Start_OnExit()
{
	//Clear the point to return to.
	m_pPointToReturnTo = NULL;
	m_iPointToReturnToRealTypeIndex = -1;
	m_SPC_UseInfo = NULL;
}

CTask::FSM_Return CTaskPolice::Resume_OnUpdate()
{
	//Set the state to resume to.
	bool bKeepSubTask = false;
	int iStateToResumeTo = ChooseStateToResumeTo(bKeepSubTask);
	SetState(iStateToResumeTo);

	// Clear this so we don't do it again next time we abort...
	m_bForceStartOnAbort = false;

	//Check if we should keep the sub-task.
	if(bKeepSubTask)
	{
		//Keep the sub-task across the transition.
		SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
	}

	return FSM_Continue;
}

void CTaskPolice::Unalerted_OnEnter()
{
	//Check if we are resuming the state.  Keep the sub-task, if possible.
	if(GetSubTask() && (GetSubTask()->GetTaskType() == CTaskTypes::TASK_UNALERTED))
	{
		return;
	}

	//Ensure the task is valid.
	CTask* pUnalertedTask = m_pUnalertedTask;
	if(!taskVerifyf(pUnalertedTask, "The unalerted task is invalid."))
	{
		return;
	}
	
	//Clear the task.
	m_pUnalertedTask = NULL;
	
	//Start the task.
	SetNewTask(pUnalertedTask);
}

CTask::FSM_Return CTaskPolice::Unalerted_OnUpdate()
{
	//Check if we have a valid order.
	const COrder* pOrder = GetPed()->GetPedIntelligence()->GetOrder();
	if(pOrder && pOrder->GetIsValid())
	{
		//Cache the scenario point to return to.
		CacheScenarioPointToReturnTo();
		
		//Respond to the order.
		SetState(State_RespondToOrder);
	}
	//Check if the sub-task has finished.
	else if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Move to the start state.
		SetState(State_Start);
	}
	
	return FSM_Continue;
}

void CTaskPolice::RespondToOrder_OnEnter()
{
	//Enable action mode.
	GetPed()->SetUsingActionMode(true, CPed::AME_RespondingToOrder);

	//Check if we are resuming the state.  Keep the sub-task, if possible.
	if(GetSubTask() && (GetSubTask()->GetTaskType() == CTaskTypes::TASK_POLICE_ORDER_RESPONSE))
	{
		return;
	}
	
	//Create the task.
	CTask* pTask = rage_new CTaskPoliceOrderResponse();

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskPolice::RespondToOrder_OnUpdate()
{
	if (!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Start);
		return FSM_Continue;
	}
	return FSM_Continue;
}

void CTaskPolice::RespondToOrder_OnExit()
{
	//Grab the ped.
	CPed* pPed = GetPed();

	//Disable action mode.
	pPed->SetUsingActionMode(false, CPed::AME_RespondingToOrder);
}

void CTaskPolice::EnterVehicle_OnEnter()
{
	CPed* pPed = GetPed();
	CVehicle* pMyVehicle = pPed->GetMyVehicle();
	if(pMyVehicle)
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
		if(pDriver && !pDriver->IsLawEnforcementPed())
		{
			bool bDriverSeatAvailable = true, bPassengerSeatAvailable = pMyVehicle->IsSeatIndexValid(Seat_frontPassenger);

			const CEntityScannerIterator entityList = pPed->GetPedIntelligence()->GetNearbyPeds();
			for( const CEntity* pEnt = entityList.GetFirst(); pEnt && (bDriverSeatAvailable || bPassengerSeatAvailable); pEnt = entityList.GetNext() )
			{
				const CPed* pNearbyPed = static_cast<const CPed*>(pEnt);
				if( pNearbyPed && pNearbyPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning( CTaskTypes::TASK_ENTER_VEHICLE ) )
				{
					CTaskEnterVehicle* pEnterTask = static_cast<CTaskEnterVehicle*>(pNearbyPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE));
					if (pEnterTask && pEnterTask->GetVehicle() == pMyVehicle)
					{
						if(pEnterTask->GetTargetSeat() == Seat_driver)
						{
							bDriverSeatAvailable = false;
						}
						else if(pEnterTask->GetTargetSeat() == Seat_frontPassenger)
						{
							bPassengerSeatAvailable = false;
						}
					}
				}
			}

			seatIndex = bDriverSeatAvailable ? pMyVehicle->GetDriverSeat() : (bPassengerSeatAvailable ? Seat_frontPassenger : -1);
		}
		else if(!pDriver || (pDriver != pPed && pDriver->IsFatallyInjured()))
		{
			seatIndex = pMyVehicle->GetDriverSeat();
		}

		if(seatIndex == -1)
		{
			return;
		}

		SetNewTask(rage_new CTaskEnterVehicle(pMyVehicle, SR_Specific, seatIndex, VehicleEnterExitFlags(), 0.0f, MOVEBLENDRATIO_WALK));
	}
}

CTask::FSM_Return CTaskPolice::EnterVehicle_OnUpdate()
{
	if (!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		CPed* pPed = GetPed();
		taskAssert(pPed);

		if(pPed->GetIsInVehicle())
		{
			if(pPed->GetIsDrivingVehicle())
			{
				SetDriveState(pPed);
				return FSM_Continue;
			}
			else
			{
				SetState(State_PatrolInVehicleAsPassenger);
				return FSM_Continue;
			}
		}
		else
		{
			SetState(State_PatrolOnFoot);
			return FSM_Continue;
		}
	}

	if(!GetPed()->GetIsEnteringVehicle())
	{
		const COrder* pOrder = GetPed()->GetPedIntelligence()->GetOrder();
		if (pOrder && pOrder->GetIsValid())
		{
			SetState(State_RespondToOrder);
			return FSM_Continue;
		}
	}
	
	return FSM_Continue;
}

void CTaskPolice::WaitForBuddiesToEnterVehicle_OnEnter()
{
	CPed* pPed = GetPed();
	taskAssert(pPed);

	CVehicle* pVehicle =pPed->GetMyVehicle();
	if( aiVerifyf(pVehicle, "CTaskPolice::WaitForBuddiesToEnterVehicle_OnEnter: Vehicle NULL!") )
	{
		SetNewTask(rage_new CTaskInVehicleBasic(pVehicle));
	}
}

CTask::FSM_Return CTaskPolice::WaitForBuddiesToEnterVehicle_OnUpdate()
{
	CPed* pPed = GetPed();
	taskAssert(pPed);

	if(taskVerifyf(pPed->GetIsInVehicle() && pPed->GetIsDrivingVehicle(), "CTaskPolice::WaitForBuddiesToEnterVehicle_OnUpdate, Ped [%s] is not driving a vehicle", pPed->GetDebugName()))
	{
		// Don't change states until everyone is in the car
		if (CTaskVehiclePersuit::CountNumberBuddiesEnteringVehicle(pPed, pPed->GetMyVehicle()) == 0)		
		{
			SetState(State_PatrolInVehicleAsDriver);
		}

		return FSM_Continue;
	}
	else
	{
		SetState(State_Start);
		return FSM_Continue;
	}
}

void CTaskPolice::PatrolOnFoot_OnEnter()
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
}

CTask::FSM_Return CTaskPolice::PatrolOnFoot_OnUpdate()
{
	const COrder* pOrder = GetPed()->GetPedIntelligence()->GetOrder();
	if (pOrder && pOrder->GetIsValid())
	{
		SetState(State_RespondToOrder);
		return FSM_Continue;
	}

	return FSM_Continue;
}

void CTaskPolice::PatrolOnFoot_OnExit()
{
	if(NetworkInterface::IsGameInProgress() && GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_CreatedByDispatch))
	{
		GetPed()->SetRemoveAsSoonAsPossible(false);
	}
}

void CTaskPolice::PatrolInVehicleAsDriver_OnEnter()
{
	CVehicle* pVehicle = GetPed()->GetMyVehicle();
	if (taskVerifyf(pVehicle, "PatrolInVehicleAsDriver_OnEnter: Vehicle NULL!"))
	{
		SetNewTask(CTaskUnalerted::SetupDrivingTaskForEmergencyVehicle(*GetPed(), *pVehicle, false, false));
	}
}

CTask::FSM_Return CTaskPolice::PatrolInVehicleAsDriver_OnUpdate()
{
	const COrder* pOrder = GetPed()->GetPedIntelligence()->GetOrder();
	if (pOrder && pOrder->GetIsValid())
	{
		SetState(State_RespondToOrder);
		return FSM_Continue;
	}

	//Check to see if we can arrest a criminal
	m_CriminalScanner.Scan(GetPed());
	if (m_CriminalScanner.GetEntity())
	{
		SetState(State_PursueCriminal);
		return FSM_Continue;
	}

	if (CRandomEventManager::GetInstance().CanCreateEventOfType(RC_COP_VEHICLE_DRIVING_FAST))
	{
		CVehicle* pVehicle = GetPed()->GetMyVehicle();

		if (pVehicle && pVehicle->GetVehicleType() == VEHICLE_TYPE_CAR)
		{
			CRandomEventManager::GetInstance().SetEventStarted(RC_COP_VEHICLE_DRIVING_FAST);
			SetState(State_PatrolInVehicleFastAsDriver);
			return FSM_Continue;
		}
	}

	GetPed()->SetPedResetFlag(CPED_RESET_FLAG_PatrollingInVehicle, true);

	return FSM_Continue;
}

void CTaskPolice::PatrolInVehicleFastAsDriver_OnEnter()
{
	CVehicle* pVehicle = GetPed()->GetMyVehicle();
	if (taskVerifyf(pVehicle, "PatrolInVehicleAsDriver_OnEnter: Vehicle NULL!"))
	{
		pVehicle->TurnSirenOn(true);

		sVehicleMissionParams params;
		params.m_iDrivingFlags = DMode_AvoidCars_Reckless;
		params.m_fCruiseSpeed = 30.0;

		CTaskVehicleMissionBase* pTask = CVehicleIntelligence::CreateCruiseTask(*pVehicle, params);

		SetNewTask(rage_new CTaskControlVehicle(pVehicle, pTask));
	}
}

CTask::FSM_Return CTaskPolice::PatrolInVehicleFastAsDriver_OnUpdate()
{
	const COrder* pOrder = GetPed()->GetPedIntelligence()->GetOrder();

	if (pOrder && pOrder->GetIsValid())
	{
		CVehicle* pVehicle = GetPed()->GetMyVehicle();
		if (pVehicle)
		{
			pVehicle->TurnSirenOn(false);
		}
		SetState(State_RespondToOrder);
		return FSM_Continue;
	}

	//Check to see if we can arrest a criminal
	m_CriminalScanner.Scan(GetPed(), true);
	if (m_CriminalScanner.GetEntity())
	{
		CVehicle* pVehicle = GetPed()->GetMyVehicle();
		if (pVehicle)
		{
			pVehicle->TurnSirenOn(false);
		}
		SetState(State_PursueCriminal);
		return FSM_Continue;
	}

	CheckPlayerAudio(GetPed());

	GetPed()->SetPedResetFlag(CPED_RESET_FLAG_PatrollingInVehicle, true);

	return FSM_Continue;
}

CTask::FSM_Return CTaskPolice::PatrolInVehicleFastAsDriver_OnExit()
{
	CVehicle* pVehicle = GetPed()->GetMyVehicle();
	if (pVehicle)
	{
		pVehicle->TurnSirenOn(false);
	}
	return FSM_Continue;
}

void CTaskPolice::PatrolInVehicleAsPassenger_OnEnter()
{
	CVehicle* pVehicle = GetPed()->GetMyVehicle();
	if( aiVerifyf(pVehicle, "PatrolInVehicleAsPassenger_OnEnter: Vehicle NULL!") )
	{
		SetNewTask(rage_new CTaskInVehicleBasic(pVehicle));
	}
}

CTask::FSM_Return CTaskPolice::PatrolInVehicleAsPassenger_OnUpdate()
{
	const COrder* pOrder = GetPed()->GetPedIntelligence()->GetOrder();
	if (pOrder && pOrder->GetIsValid())
	{
		SetState(State_RespondToOrder);
		return FSM_Continue;
	}

	//Check if we are still in a vehicle.
	const CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
	if(pVehicle)
	{
		//Check if the driver is valid.
		const CPed* pDriver = pVehicle->GetDriver();
		if(pDriver)
		{
			//Check if the driver is pursuing a criminal.
			const CTaskPursueCriminal* pTask = static_cast<CTaskPursueCriminal *>(
				pDriver->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PURSUE_CRIMINAL));
			if(pTask != NULL && pTask->GetCriminal())
			{
				//Give us the task.
				CTaskPursueCriminal::GiveTaskToPed(*GetPed(), pTask->GetCriminal(), true, false);
			}
		}
	}

	GetPed()->SetPedResetFlag(CPED_RESET_FLAG_PatrollingInVehicle, true);

	return FSM_Continue;
}


void CTaskPolice::PursueCriminal_OnEnter()
{
	CEntity* pEntity = m_CriminalScanner.GetEntity();
	taskAssert(pEntity);

	bool bInstant = false;
	if(GetPreviousState() == State_PatrolInVehicleFastAsDriver)
	{
		bInstant = true;
	}
	SetNewTask( rage_new CTaskPursueCriminal(pEntity,bInstant, bInstant) );

	SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);

	m_CriminalScanner.ClearScan();
}

CTask::FSM_Return CTaskPolice::PursueCriminal_OnUpdate()
{
	if (!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Start);
		return FSM_Continue;
	}
	return FSM_Continue;
}

#if !__FINAL
const char * CTaskPolice::GetStaticStateName( s32 iState  )
{
	taskAssert(iState >= State_Start && iState <= State_Finish);

	switch (iState)
	{
		case State_Start:							return "State_Start";
		case State_Resume:							return "State_Resume";
		case State_Unalerted:						return "State_Unalerted";
		case State_EnterVehicle:					return "State_EnterVehicle";
		case State_WaitForBuddiesToEnterVehicle:	return "State_WaitForBuddiesToEnterVehicle";
		case State_PatrolOnFoot:					return "State_PatrolOnFoot";
		case State_PatrolInVehicleAsDriver:			return "State_PatrolInVehicleAsDriver";
		case State_PatrolInVehicleAsPassenger:		return "State_PatrolInVehicleAsPassenger";
		case State_PatrolInVehicleFastAsDriver:		return "State_PatrolInVehicleFastAsDriver";
		case State_RespondToOrder:					return "State_RespondToOrder";
		case State_PursueCriminal:					return "State_PursueCriminal";
		case State_Finish:							return "State_Finish";
		default: taskAssert(0); 	
	}

	return "State_Invalid";
}
#endif // !__FINAL

void CTaskPolice::CheckPlayerAudio(CPed* pPed)
{
	if(!pPed->GetIsInVehicle() || !pPed->GetMyVehicle()->InheritsFromAutomobile() || pPed->GetMyVehicle()->GetVelocity().XYMag2() < rage::square(5.0f))
	{
		return;
	}

	CPed* pPlayerPed = FindPlayerPed();
	if (!pPlayerPed)
	{
		return;
	}
		
	float fDistanceSq = (VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition())).Mag2();
	if(fDistanceSq > rage::square(20.0f))
	{
		return;
	}

	if(pPlayerPed->GetIsInVehicle())
	{
		float fRelativeVelSq = (pPed->GetMyVehicle()->GetVelocity() - pPlayerPed->GetMyVehicle()->GetVelocity()).Mag2();
		if( fRelativeVelSq < rage::square(20.0f))
		{
			return;
		}

		if(pPlayerPed->GetVehiclePedInside()->GetNumberOfPassenger() <= 0)
		{
			return;
		}
	}
	else
	{
		static dev_float s_fMaxDistance = 15.0f;

		//Check if the player is in a group.
		const CPedGroup* pPedGroup = pPlayerPed->GetPedsGroup();
		if(!pPedGroup || (pPedGroup->FindDistanceToNearestMember() > s_fMaxDistance))
		{
			if(!CFindClosestFriendlyPedHelper::Find(*pPlayerPed, s_fMaxDistance))
			{
				return;
			}
		}
	}

	if(!pPed->GetIsVisibleInSomeViewportThisFrame())
	{
		return;
	}

	pPlayerPed->NewSay("POLICE_PURSUIT_FALSE_ALARM");
}

int CTaskPolice::ChooseStateToResumeTo(bool& bKeepSubTask) const
{
	//Keep the sub-task.
	bKeepSubTask = true;

	if (m_bForceStartOnAbort)
	{
		bKeepSubTask = false;
		return State_Start;
	}

	//Check if the sub-task is valid.
	const CTask* pSubTask = GetSubTask();
	if(pSubTask)
	{
		//Check the task type.
		switch(pSubTask->GetTaskType())
		{
			case CTaskTypes::TASK_UNALERTED:
			{
				return State_Unalerted;
			}
			case CTaskTypes::TASK_POLICE_ORDER_RESPONSE:
			{
				return State_RespondToOrder;
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

void CTaskPolice::SetDriveState(CPed* pPed)
{
	if (taskVerifyf(pPed && pPed->GetMyVehicle(), "CTaskPolice::SetDriveState, invalid Ped or it is not in a vehicle!"))
	{				
		if(CTaskVehiclePersuit::CountNumberBuddiesEnteringVehicle(pPed, pPed->GetMyVehicle()) == 0)
		{
			SetState(State_PatrolInVehicleAsDriver);
		}
		else
		{
			SetState(State_WaitForBuddiesToEnterVehicle);
		}
	}
}

void CTaskPolice::CacheScenarioPointToReturnTo()
{
	//Ensure we are in the correct state.
	if(GetState() != State_Unalerted)
	{
		return;
	}
	
	//Ensure the sub-task is valid.
	CTask* pSubTask = GetSubTask();
	if(!pSubTask)
	{
		return;
	}
	
	//Ensure the ped is not in a vehicle.
	const CPed* pPed = GetPed();
	if(pPed->GetIsInVehicle())
	{
		return;
	}
	
	//Set the point to return to.
	m_pPointToReturnTo = pSubTask->GetScenarioPoint();
	m_iPointToReturnToRealTypeIndex = pSubTask->GetScenarioPointType();
	m_SPC_UseInfo = NULL;
	if (pSubTask->GetTaskType() == CTaskTypes::TASK_UNALERTED)
	{
		CTaskUnalerted* tUA = static_cast<CTaskUnalerted*>(pSubTask);
		m_SPC_UseInfo = tUA->GetScenarioChainUserData();

		// It would be dangerous to leave the pointer set in CTaskUnalerted,
		// since that task will get cleaned up next, and reset the chaining
		// user info. That means that CTaskPolice may have a pointer to deleted
		// data, and if we start up a CTaskUnalerted again when resuming, we would
		// be in a bad spot.
		tUA->SetScenarioChainUserInfo(NULL);
	}
}

/////////

CTaskPoliceOrderResponse::Tunables CTaskPoliceOrderResponse::sm_Tunables;
IMPLEMENT_SERVICE_TASK_TUNABLES(CTaskPoliceOrderResponse, 0x75143d8a);

CTaskPoliceOrderResponse::CTaskPoliceOrderResponse()
: m_vIncidentLocationOnFoot(V_ZERO)
, m_fTimeToWander(0.0f)
, m_fTimeSinceDriverHasBeenValid(0.0f)
, m_uRunningFlags(0)
, m_bWantedConesDisabled(false)
{
	SetInternalTaskType(CTaskTypes::TASK_POLICE_ORDER_RESPONSE);
}

CTaskPoliceOrderResponse::~CTaskPoliceOrderResponse()
{

}

bool CTaskPoliceOrderResponse::ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
	//Check if we should ignore combat events.
	if(ShouldIgnoreCombatEvents())
	{
		//Ensure the abort priority is not immediate.
		if(pEvent && iPriority != ABORT_PRIORITY_IMMEDIATE)
		{
			//Ignore certain events that will cause the officer to stop waiting.
			switch(((CEvent *)pEvent)->GetEventType())
			{
				case EVENT_ACQUAINTANCE_PED_WANTED:
				case EVENT_SHOUT_TARGET_POSITION:
					return false;
				default:
					break;
			}
		}
	}
	
	//Call the base class version.
	return CTask::ShouldAbort(iPriority, pEvent);
}

CTask::FSM_Return CTaskPoliceOrderResponse::ProcessPreFSM()
{
	// Quit if we have no order, or it is valid
	COrder* pOrder = GetPed()->GetPedIntelligence()->GetOrder();
	if (!pOrder || !pOrder->GetIsValid())
	{
		return FSM_Quit;
	}
	
	//Process can see target.
	ProcessCanSeeTarget();

	//Process the driver.
	ProcessDriver();

	return FSM_Continue;
}

CTask::FSM_Return CTaskPoliceOrderResponse::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
#if __BANK
	DrawDebug();
#endif

	FSM_Begin
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_Resume)
			FSM_OnUpdate
				return Resume_OnUpdate();

		FSM_State(State_RespondToOrder)
			FSM_OnUpdate
				return RespondToOrder_OnUpdate();

		FSM_State(State_SwapWeapon)
			FSM_OnEnter
				SwapWeapon_OnEnter();
			FSM_OnUpdate
				return SwapWeapon_OnUpdate();
				
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

		FSM_State(State_InvestigateIncidentLocationInVehicle)
			FSM_OnEnter
				InvestigateIncidentLocationInVehicle_OnEnter();
			FSM_OnUpdate
				return InvestigateIncidentLocationInVehicle_OnUpdate();

		FSM_State(State_InvestigateIncidentLocationAsPassenger)
			FSM_OnEnter
				InvestigateIncidentLocationAsPassenger_OnEnter();
			FSM_OnUpdate
				return InvestigateIncidentLocationAsPassenger_OnUpdate();

		FSM_State(State_FindIncidentLocationOnFoot)
			FSM_OnEnter
				FindIncidentLocationOnFoot_OnEnter();
			FSM_OnUpdate
				return FindIncidentLocationOnFoot_OnUpdate();

		FSM_State(State_GoToIncidentLocationOnFoot)
			FSM_OnEnter
				GoToIncidentLocationOnFoot_OnEnter();
			FSM_OnUpdate
				return GoToIncidentLocationOnFoot_OnUpdate();
				
		FSM_State(State_UnableToReachIncidentLocation)
			FSM_OnEnter
				UnableToReachIncidentLocation_OnEnter();
			FSM_OnUpdate
				return UnableToReachIncidentLocation_OnUpdate();

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

		FSM_State(State_SearchInVehicle)
			FSM_OnEnter
				SearchInVehicle_OnEnter();
			FSM_OnUpdate
				return SearchInVehicle_OnUpdate();
				
		FSM_State(State_WanderOnFoot)
			FSM_OnEnter
				WanderOnFoot_OnEnter();
			FSM_OnUpdate
				return WanderOnFoot_OnUpdate();
				
		FSM_State(State_WanderInVehicle)
			FSM_OnEnter
				WanderInVehicle_OnEnter();
			FSM_OnUpdate
				return WanderInVehicle_OnUpdate();
				
		FSM_State(State_Combat)
			FSM_OnEnter
				Combat_OnEnter();
			FSM_OnUpdate
				return Combat_OnUpdate();
				
		FSM_State(State_WaitPulledOverInVehicle)
			FSM_OnEnter
				WaitPulledOverInVehicle_OnEnter();
			FSM_OnUpdate
				return WaitPulledOverInVehicle_OnUpdate();
				
		FSM_State(State_WaitCruisingInVehicle)
			FSM_OnEnter
				WaitCruisingInVehicle_OnEnter();
			FSM_OnUpdate
				return WaitCruisingInVehicle_OnUpdate();

		FSM_State(State_WaitForArrestedPed)
			FSM_OnEnter
				WaitForArrestedPed_OnEnter();
			FSM_OnUpdate
				return WaitForArrestedPed_Update();
				
		FSM_State(State_WaitAtRoadBlock)
			FSM_OnEnter
				WaitAtRoadBlock_OnEnter();
			FSM_OnUpdate
				return WaitAtRoadBlock_OnUpdate();
			FSM_OnExit
				WaitAtRoadBlock_OnExit();
				
		FSM_State(State_MatchSpeedInVehicle)
			FSM_OnEnter
				MatchSpeedInVehicle_OnEnter();
			FSM_OnUpdate
				return MatchSpeedInVehicle_OnUpdate();
			FSM_OnExit
				MatchSpeedInVehicle_OnExit();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

#if !__FINAL
void CTaskPoliceOrderResponse::Debug() const 
{
	DrawDebug();
}

void CTaskPoliceOrderResponse::DrawDebug() const 
{
}


const char * CTaskPoliceOrderResponse::GetStaticStateName( s32 iState  )
{
	taskAssert(iState >= State_Start && iState <= State_Finish);

	switch (iState)
	{
	case State_Start:							return "State_Start";
	case State_Resume:							return "State_Resume";
	case State_RespondToOrder:					return "State_RespondToOrder";
	case State_SwapWeapon:						return "State_SwapWeapon";
	case State_EnterVehicle:					return "State_EnterVehicle";
	case State_ExitVehicle:						return "State_ExitVehicle";
	case State_FindIncidentLocationOnFoot:		return "State_FindIncidentLocationOnFoot";
	case State_GoToIncidentLocationOnFoot:		return "State_GoToIncidentLocationOnFoot";
	case State_GoToIncidentLocationInVehicle:	return "State_GoToIncidentLocationInVehicle";
	case State_GoToIncidentLocationAsPassenger:	return "State_GoToIncidentLocationAsPassenger";
	case State_InvestigateIncidentLocationInVehicle:	return "State_InvestigateIncidentLocationInVehicle";
	case State_InvestigateIncidentLocationAsPassenger:	return "State_InvestigateIncidentLocationAsPassenger";
	case State_UnableToReachIncidentLocation:	return "State_UnableToReachIncidentLocation";
	case State_FindPositionToSearchOnFoot:		return "State_FindPositionToSearchOnFoot";
	case State_SearchOnFoot:					return "State_SearchOnFoot";
	case State_SearchInVehicle:					return "State_SearchInVehicle";
	case State_WanderOnFoot:					return "State_WanderOnFoot";
	case State_WanderInVehicle:					return "State_WanderInVehicle";
	case State_WaitForArrestedPed:				return "State_WaitForArrestedPed";
	case State_Combat:							return "State_Combat";
	case State_WaitPulledOverInVehicle:			return "State_WaitPulledOverInVehicle";
	case State_WaitCruisingInVehicle:			return "State_WaitCruisingInVehicle";
	case State_WaitAtRoadBlock:					return "State_WaitAtRoadBlock";
	case State_Finish:							return "State_Finish";
	default: taskAssert(0); 	
	}

	return "State_Invalid";
}
#endif // !__FINAL


CTask::FSM_Return CTaskPoliceOrderResponse::Start_OnUpdate()
{
	//Equip our best weapon.
	CPedWeaponManager* pWeaponManager = GetPed()->GetWeaponManager();
	if(pWeaponManager)
	{
		pWeaponManager->EquipBestWeapon();
	}

	SetState(State_RespondToOrder);
	return FSM_Continue;
}

CTask::FSM_Return CTaskPoliceOrderResponse::Resume_OnUpdate()
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

CTask::FSM_Return CTaskPoliceOrderResponse::RespondToOrder_OnUpdate()
{
	//Grab the ped.
	CPed* pPed = GetPed();

	//Check if we need to swap our weapon.
	if(ShouldSwapWeapon())
	{
		//Swap our weapon.
		SetState(State_SwapWeapon);
	}
	else
	{
		//Grab the order.
		COrder* pOrder = pPed->GetPedIntelligence()->GetOrder();
		if(pOrder->IsPoliceOrder() && pOrder->GetDispatchType() != DT_PoliceVehicleRequest)
		{
			//Grab the police order.
			CPoliceOrder* pPoliceOrder = static_cast<CPoliceOrder *>(pOrder);

			//If the order is not valid for the ped, request a new one.
			if(!CPoliceAutomobileDispatch::IsOrderValidForPed(*pPoliceOrder, *pPed))
			{
				CPoliceAutomobileDispatch::RequestNewOrder(*pPed, false);
			}

			//Set the state for the police order.
			SetStateForPoliceOrder(*pPoliceOrder);
		}
		else
		{
			//Set the state for the default order.
			SetStateForDefaultOrder();
		}
	}
	
	return FSM_Continue;
}

void CTaskPoliceOrderResponse::SwapWeapon_OnEnter()
{
	//Create the task.
	CTask* pTask = rage_new CTaskSwapWeapon(SWAP_HOLSTER | SWAP_DRAW);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskPoliceOrderResponse::SwapWeapon_OnUpdate()
{
	//Check if the sub-task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Respond to the order.
		SetState(State_RespondToOrder);
	}

	return FSM_Continue;
}

void CTaskPoliceOrderResponse::EnterVehicle_OnEnter()
{
	//Grab the ped.
	CPed* pPed = GetPed();
	
	//Ensure the vehicle is valid.
	CVehicle* pVehicle = pPed->GetMyVehicle();
	if(!pVehicle)
	{
		return;
	}
	
	//Calculate the seat.
	s32 iSeatIndex = DoesPoliceOrderRequireDrivingAVehicle(GetCurrentPoliceOrderType()) ? Seat_driver : Seat_frontPassenger;
	
	//Create the task.
	CTask* pTask = rage_new CTaskEnterVehicle(pVehicle, SR_Specific, iSeatIndex);
	
	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskPoliceOrderResponse::EnterVehicle_OnUpdate()
{
	//Process the order.
	if(ProcessOrder())
	{
		//A new order has been requested, nothing else to do.
	}
	//Check if the sub-task has finished.
	else if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Check if we are in a vehicle.
		if(GetPed()->GetIsInVehicle())
		{
			//Respond to the order.
			SetState(State_RespondToOrder);
		}
	}
	
	return FSM_Continue;
}

void CTaskPoliceOrderResponse::ExitVehicle_OnEnter()
{
	//Grab the ped.
	CPed* pPed = GetPed();
	
	//Ensure the vehicle is valid.
	CVehicle* pVehicle = pPed->GetVehiclePedInside();
	if(!taskVerifyf(pVehicle, "The vehicle is invalid."))
	{
		return;
	}
	
	//Create the flags.
	VehicleEnterExitFlags vehicleFlags;
	vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DontCloseDoor);
	vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DelayForTime);
	vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::QuitIfAllDoorsBlocked);
	
	//Create the task.
	float fDelayTime = pVehicle->IsDriver(pPed) ? fwRandom::GetRandomNumberInRange(0.0f, 0.1f) :
					   pPed->GetRandomNumberInRangeFromSeed(CTaskVehiclePersuit::GetTunables().m_MinDelayExitTime, CTaskVehiclePersuit::GetTunables().m_MaxDelayExitTime);
	CTask* pTask = rage_new CTaskExitVehicle(pPed->GetMyVehicle(), vehicleFlags, fDelayTime);
	
	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskPoliceOrderResponse::ExitVehicle_OnUpdate()
{
	//Check if the sub-task has finished.
	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Check if we are still in a vehicle.
		if(GetPed()->GetIsInVehicle())
		{
			//Check if the retry time has expired.
			if(GetTimeInState() > sm_Tunables.m_TimeBetweenExitVehicleRetries)
			{
				//Restart the state.
				SetFlag(aiTaskFlags::RestartCurrentState);
			}
		}
		else
		{
			//Check if the weapon manager is valid.
			CPedWeaponManager* pWeaponManager = GetPed()->GetWeaponManager();
			if(pWeaponManager)
			{
				//Equip our best weapon.
				pWeaponManager->EquipBestWeapon();
			}

			//Respond to the order (should be on-foot).
			SetState(State_RespondToOrder);
		}
	}
	
	return FSM_Continue;
}

void CTaskPoliceOrderResponse::GoToIncidentLocationInVehicle_OnEnter()
{
	const COrder* pOrder = GetPed()->GetPedIntelligence()->GetOrder();
	if(aiVerifyf(GetPed()->GetIsInVehicle(), "GoToIncidentLocationInVehicle_OnEnter: Not in vehicle!"))
	{
		aiAssert(GetPed()->GetMyVehicle());
		GetPed()->GetMyVehicle()->TurnSirenOn(true);

		Vec3V vLocation = VECTOR3_TO_VEC3V(static_cast<const CPoliceOrder*>(pOrder)->GetDispatchLocation());

		if ( pOrder->GetIncident()->GetType() == CIncident::IT_Arrest )
		{
			 vLocation = VECTOR3_TO_VEC3V(pOrder->GetIncident()->GetLocation());
		}

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

CTask::FSM_Return CTaskPoliceOrderResponse::GoToIncidentLocationInVehicle_OnUpdate()
{
	CPed* pPed = GetPed();
	COrder* pOrder = pPed->GetPedIntelligence()->GetOrder();

	if( !pPed->GetIsInVehicle() )
	{
		SetState(State_RespondToOrder);
		return FSM_Continue;
	}

	CPoliceOrder* pPoliceOrder = static_cast<CPoliceOrder*>(pOrder);

	if (pPoliceOrder && pPoliceOrder->GetNeedsToUpdateLocation())
	{
		if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_CONTROL_VEHICLE)
		{
			aiTask* pTask = static_cast<CTaskControlVehicle*>(GetSubTask())->GetVehicleTask();
			if (pTask && (pTask->GetTaskType() == CTaskTypes::TASK_VEHICLE_GOTO_AUTOMOBILE_NEW))
			{
				Vector3 vDispatchLocation = pPoliceOrder->GetDispatchLocation();

				if ( pOrder->GetIncident()->GetType() == CIncident::IT_Arrest )
				{
					vDispatchLocation = pOrder->GetIncident()->GetLocation();
				}

				static_cast<CTaskVehicleGoToAutomobileNew*>(pTask)->SetTargetPosition(&vDispatchLocation);
				pPoliceOrder->SetNeedsToUpdateLocation(false);
			}
		}
	}

	// Arrived at location - exit vehicle
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished) )
	{
		if (pOrder->GetDispatchType() == DT_PoliceVehicleRequest)
		{
			CPoliceVehicleRequest::RequestNewOrder(pPed, true);
			if (pPoliceOrder->GetPoliceOrderType() == CPoliceOrder::PO_ARREST_WAIT_AT_DISPATCH_LOCATION_AS_DRIVER || 
				pPoliceOrder->GetPoliceOrderType() == CPoliceOrder::PO_ARREST_GOTO_DISPATCH_LOCATION_AS_PASSENGER)
			{
				pOrder->SetEntityArrivedAtIncident();

				SetState(State_WaitForArrestedPed);
				return FSM_Continue;
			}
			else if(pPoliceOrder->GetPoliceOrderType() == CPoliceOrder::PO_NONE )
			{
				// NOTE[egarcia]: REVIEW. We skip the SetEntityArrivedAtIncident notification until the investigate incident location finishes.
				// This is to prevent the incident timer from finishing the investigate task too early. We should review if it is possible to change
				// the timer value (it comes from script)

				SetState(State_InvestigateIncidentLocationInVehicle);
				return FSM_Continue;
			}
			else 
			{
				pOrder->SetEntityArrivedAtIncident();

				SetState(State_Finish);
				return FSM_Continue;
			}
		}
		else
		{
			pOrder->SetEntityArrivedAtIncident();
		}
		
		if(!pPoliceOrder)
		{
			SetState(State_Finish);
			return FSM_Continue;
		}

		//Request a new order.
		RequestNewOrder(true);
	}
	return FSM_Continue;
}

void CTaskPoliceOrderResponse::InvestigateIncidentLocationInVehicle_OnEnter()
{
	//Grab the ped.
	CPed* pPed = GetPed();

	//Ensure the vehicle is valid.
	CVehicle* pVehicle = pPed->GetVehiclePedInside();
	if(!taskVerifyf(pVehicle, "The vehicle is invalid."))
	{
		return;
	}

	//Ensure the ped is driving a vehicle.
	if(!taskVerifyf(pPed->GetIsDrivingVehicle(), "The ped is not driving a vehicle."))
	{
		return;
	}

	//Turn the siren on.
	pVehicle->TurnSirenOn(true);

	//
	CTaskAmbientClips* pAmbientTask	= rage_new CTaskAmbientClips(0, CONDITIONALANIMSMGR.GetConditionalAnimsGroup(CTaskVehicleFSM::emptyAmbientContext.GetHash()));
	SetNewTask(pAmbientTask);

	// TODO[egarcia]: Move to tunables
	static const int iINVESTIGATE_INCIDENT_TIME_TO_WAIT = 3000;
	m_WaitWhileInvestigatingIncidentTimer.Set(fwTimer::GetTimeInMilliseconds(), iINVESTIGATE_INCIDENT_TIME_TO_WAIT);
}

CTask::FSM_Return CTaskPoliceOrderResponse::InvestigateIncidentLocationInVehicle_OnUpdate()
{
	CPed* pPed = GetPed();

	if( !pPed->GetIsInVehicle() )
	{
		SetState(State_RespondToOrder);
		return FSM_Continue;
	}

	COrder* pOrder = pPed->GetPedIntelligence()->GetOrder();
	if (!pOrder)
	{
		SetState(State_Finish);
		return FSM_Continue;
	}

	//Process the order state.
	if(ProcessOrderState())
	{
		//A new order has been requested, nothing else to do.
	}

	ProcessInvestigateIncidentInVehicleLookAt();

	if (m_WaitWhileInvestigatingIncidentTimer.IsSet())
	{
		if (m_WaitWhileInvestigatingIncidentTimer.IsOutOfTime())
		{
			m_WaitWhileInvestigatingIncidentTimer.Unset();
			CreateInvestigateIncidentLocationInVehicleTask();

			// TODO[egarcia]: Move to tunables
			static const int iINVESTIGATE_INCIDENT_TIME_TO_WAIT = 7000;
			m_SlowDrivingWhileInvestigatingIncidentTimer.Set(fwTimer::GetTimeInMilliseconds(), iINVESTIGATE_INCIDENT_TIME_TO_WAIT);
		}
	}
	else if (!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished) || m_SlowDrivingWhileInvestigatingIncidentTimer.IsOutOfTime())
	{
		// Finish order
		pOrder->SetEntityArrivedAtIncident();
		pOrder->SetEntityFinishedWithIncident();

		if(pOrder->GetIncident())
		{
			pOrder->GetIncident()->SetHasBeenAddressed(true);
		}
	}

	return FSM_Continue;
}

void CTaskPoliceOrderResponse::ProcessInvestigateIncidentInVehicleLookAt()
{
	m_InvestigatingIncidentLookAt.Clear();

	CAITarget LookAtTarget;
	CPed* pSuspiciousPed = FindInvestigateIncidentSuspiciousPed();
	if  (pSuspiciousPed)
	{
		m_InvestigatingIncidentLookAt.SetEntity(pSuspiciousPed);
	}

	if (m_InvestigatingIncidentLookAt.GetIsValid())
	{
		Vector3 vLookAtPosition;
		m_InvestigatingIncidentLookAt.GetPosition(vLookAtPosition);
		CIkRequestBodyLook lookRequest( NULL, BONETAG_INVALID, VECTOR3_TO_VEC3V(vLookAtPosition), 0, CIkManager::IK_LOOKAT_HIGH);
		lookRequest.SetRefDirHead( LOOKIK_REF_DIR_FACING );
		lookRequest.SetRefDirNeck( LOOKIK_REF_DIR_FACING );
		lookRequest.SetRotLimNeck( LOOKIK_ROT_ANGLE_YAW, LOOKIK_ROT_LIM_WIDE );
		lookRequest.SetRotLimNeck( LOOKIK_ROT_ANGLE_PITCH, LOOKIK_ROT_LIM_NARROWEST );
		lookRequest.SetHoldTime( 100 );
		GetPed()->GetIkManager().Request( lookRequest );
	}
}


CPed* CTaskPoliceOrderResponse::FindInvestigateIncidentSuspiciousPed()
{
	CPed* pPed = GetPed();
	taskAssert(pPed);

	CPed* pSuspiciousPed = NULL;

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
			if (pPlayerPed && pPed->GetPedIntelligence()->GetPedPerception().IsInSeeingRange(pPlayerPed))
			{
				pSuspiciousPed = pPlayerPed;
				break;
			}
		}
	}
	else
	{
		CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
		if (pPlayerPed && pPed->GetPedIntelligence()->GetPedPerception().IsInSeeingRange(pPlayerPed))
		{
			pSuspiciousPed = pPlayerPed;
		}
	}

	return pSuspiciousPed;
}


bool CTaskPoliceOrderResponse::CreateInvestigateIncidentLocationInVehicleTask()
{
	CPed* pPed = GetPed();

	//Ensure the vehicle is valid.
	CVehicle* pVehicle = pPed->GetVehiclePedInside();
	if(!taskVerifyf(pVehicle, "The vehicle is invalid."))
	{
		return false;
	}

	//Create the vehicle task.
	sVehicleMissionParams params;
	params.m_iDrivingFlags = DMode_AvoidCars | DF_AvoidTurns;
	// TODO[egarcia]: Move to tunables
	static const float fINVESTIGATE_INCIDENT_CRUISE_SPEED = 2.0f;
	params.m_fCruiseSpeed = fINVESTIGATE_INCIDENT_CRUISE_SPEED;
	CTask* pVehicleTask = CVehicleIntelligence::CreateCruiseTask(*pVehicle, params);

	//Create the task.
	CTask* pTask = rage_new CTaskControlVehicle(pVehicle, pVehicleTask);

	//Start the task.
	SetNewTask(pTask);

	return true;
}


void CTaskPoliceOrderResponse::GoToIncidentLocationAsPassenger_OnEnter()
{
}

CTask::FSM_Return CTaskPoliceOrderResponse::GoToIncidentLocationAsPassenger_OnUpdate()
{
	//Apparently the police vehicle request is special-cased.
	COrder* pOrder = GetPed()->GetPedIntelligence()->GetOrder();
	if(pOrder->GetDispatchType() != DT_PoliceVehicleRequest)
	{
		//Process the order.
		if(ProcessOrder())
		{
			//A new order has been requested, nothing else to do.
		}
	}

	// If the driver is already investigating the incident location, goto investigate as passenger
	const CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
	if(pVehicle)
	{
		//Check if the driver is valid.
		const CPed* pDriver = pVehicle->GetDriver();
		if(pDriver)
		{
			//Check if the driver is investigating.
			const CTaskPoliceOrderResponse* pDriverTask = static_cast<CTaskPoliceOrderResponse *>(
				pDriver->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_POLICE_ORDER_RESPONSE));

			if(pDriverTask != NULL && pDriverTask->GetState() == State_InvestigateIncidentLocationInVehicle)
			{
				SetState(State_InvestigateIncidentLocationAsPassenger);
			}
		}
	}
	
	return FSM_Continue;
}

void CTaskPoliceOrderResponse::InvestigateIncidentLocationAsPassenger_OnEnter()
{
	//
	CTaskAmbientClips* pAmbientTask	= rage_new CTaskAmbientClips(0, CONDITIONALANIMSMGR.GetConditionalAnimsGroup(CTaskVehicleFSM::emptyAmbientContext.GetHash()));
	SetNewTask(pAmbientTask);
}

CTask::FSM_Return CTaskPoliceOrderResponse::InvestigateIncidentLocationAsPassenger_OnUpdate()
{
	//Apparently the police vehicle request is special-cased.
	COrder* pOrder = GetPed()->GetPedIntelligence()->GetOrder();
	if(pOrder->GetDispatchType() != DT_PoliceVehicleRequest)
	{
		//Process the order.
		if(ProcessOrder())
		{
			//A new order has been requested, nothing else to do.
		}
	}

	ProcessInvestigateIncidentInVehicleLookAt();

	return FSM_Continue;
}

void CTaskPoliceOrderResponse::FindIncidentLocationOnFoot_OnEnter()
{
	//Ensure the ped is in a vehicle.
	CPed* pPed = GetPed();
	if(!aiVerifyf(!pPed->GetIsInVehicle(), "The ped is in a vehicle."))
	{
		return;
	}

	//Start the search.
	m_HelperForIncidentLocationOnFoot.Reset();
	if(!m_HelperForIncidentLocationOnFoot.Start(*pPed))
	{
		return;
	}
}

CTask::FSM_Return CTaskPoliceOrderResponse::FindIncidentLocationOnFoot_OnUpdate()
{
	//Check if the search did not start.
	if(!m_HelperForIncidentLocationOnFoot.HasStarted())
	{
		SetState(State_UnableToReachIncidentLocation);
	}
	else
	{
		//Check if the results are ready.
		bool bWasSuccessful;
		if(m_HelperForIncidentLocationOnFoot.IsResultReady(bWasSuccessful, m_vIncidentLocationOnFoot))
		{
			//Check if the search was successful.
			if(bWasSuccessful)
			{
				SetState(State_GoToIncidentLocationOnFoot);
			}
			else
			{
				SetState(State_UnableToReachIncidentLocation);
			}
		}
	}

	return FSM_Continue;
}

void CTaskPoliceOrderResponse::GoToIncidentLocationOnFoot_OnEnter()
{
	//Assert that the position is valid.
	taskAssert(!IsCloseAll(m_vIncidentLocationOnFoot, Vec3V(V_ZERO), ScalarVFromF32(SMALL_FLOAT)));

	//Face the incident location (mostly as a fallback in case we can't make it there).
	GetPed()->SetDesiredHeading(VEC3V_TO_VECTOR3(m_vIncidentLocationOnFoot));

	//Create the move task.
	CTaskMoveFollowNavMesh* pMoveTask = rage_new CTaskMoveFollowNavMesh(MOVEBLENDRATIO_RUN, VEC3V_TO_VECTOR3(m_vIncidentLocationOnFoot),
		CTaskMoveFollowNavMesh::ms_fTargetRadius, CTaskMoveFollowNavMesh::ms_fSlowDownDistance, -1, true, false, NULL);
	
	//Use larger search extents.
	pMoveTask->SetUseLargerSearchExtents(true);
	
	//Create the task.
	CTaskComplexControlMovement* pTask = rage_new CTaskComplexControlMovement(pMoveTask);
	pTask->SetAllowClimbLadderSubtask(true);
	
	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskPoliceOrderResponse::GoToIncidentLocationOnFoot_OnUpdate()
{
	//Check if our sub-task is invalid.
	if(!GetSubTask())
	{
		//Move to the 'unable to reach incident' state.
		SetState(State_UnableToReachIncidentLocation);
	}
	else
	{
		//Check if we were unable to find a route.
		CTaskMoveFollowNavMesh* pMoveTask = static_cast<CTaskMoveFollowNavMesh *>(GetPed()->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH));
		if(pMoveTask && pMoveTask->IsUnableToFindRoute())
		{
			//Move to the 'unable to reach incident' state.
			SetState(State_UnableToReachIncidentLocation);
		}
		//Check if the sub-task has finished.
		else if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
		{
			//Grab the order.
			COrder* pOrder = GetPed()->GetPedIntelligence()->GetOrder();
			
			//Note that we have arrived at the incident.
			pOrder->SetEntityArrivedAtIncident();
			
			if(pOrder->GetDispatchType() == DT_PoliceVehicleRequest)
			{
				//Restart the state.
				SetFlag(aiTaskFlags::RestartCurrentState);
			}
			else
			{
				//Request a new order.
				RequestNewOrder(true);
			}
		}
	}
	
	return FSM_Continue;
}

void CTaskPoliceOrderResponse::UnableToReachIncidentLocation_OnEnter()
{
	//Face the incident location.
	GetPed()->SetDesiredHeading(GetPed()->GetPedIntelligence()->GetOrder()->GetIncident()->GetLocation());
}

CTask::FSM_Return CTaskPoliceOrderResponse::UnableToReachIncidentLocation_OnUpdate()
{
	//Check if the timer has expired.
	static dev_float s_fMinTimeInState = 1.0f;
	if(GetTimeInState() > s_fMinTimeInState)
	{
		//Respond to the order.
		SetState(State_RespondToOrder);
	}
	
	return FSM_Continue;
}

void CTaskPoliceOrderResponse::FindPositionToSearchOnFoot_OnEnter()
{
	//Ensure the target is valid.
	const CPed* pTarget = GetTarget();
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

CTask::FSM_Return CTaskPoliceOrderResponse::FindPositionToSearchOnFoot_OnUpdate()
{
	//Check if the search did not start.
	if(!m_HelperForSearchOnFoot.HasStarted())
	{
		SetState(State_WanderOnFoot);
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
				SetState(State_WanderOnFoot);
			}
		}
	}

	return FSM_Continue;
}

void CTaskPoliceOrderResponse::SearchOnFoot_OnEnter()
{
	//Assert that the position is valid.
	taskAssert(!IsCloseAll(m_vPositionToSearchOnFoot, Vec3V(V_ZERO), ScalarVFromF32(SMALL_FLOAT)));

	//Create the move task.
	CTaskMoveFollowNavMesh* pMoveTask = rage_new CTaskMoveFollowNavMesh(CDispatchHelperSearchOnFoot::GetMoveBlendRatio(*GetPed()),
		VEC3V_TO_VECTOR3(m_vPositionToSearchOnFoot), CTaskMoveFollowNavMesh::ms_fTargetRadius, CTaskMoveFollowNavMesh::ms_fSlowDownDistance, -1, true, false, NULL);
	
	//Quit if a route isn't found.
	pMoveTask->SetQuitTaskIfRouteNotFound(true);
	
	//Create the task.
	CTaskComplexControlMovement* pTask = rage_new CTaskComplexControlMovement(pMoveTask);
	pTask->SetAllowClimbLadderSubtask(true);
	
	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskPoliceOrderResponse::SearchOnFoot_OnUpdate()
{
	//Check if the sub-task is invalid.
	if(!GetSubTask())
	{
		//Wander on foot.
		SetState(State_WanderOnFoot);
	}
	//Process the order state.
	else if(ProcessOrderState())
	{
		//A new order has been requested, nothing else to do.
	}
	
	return FSM_Continue;
}

void CTaskPoliceOrderResponse::SearchInVehicle_OnEnter()
{
	//Grab the ped.
	CPed* pPed = GetPed();
	
	//Ensure the vehicle is valid.
	CVehicle* pVehicle = pPed->GetVehiclePedInside();
	if(!taskVerifyf(pVehicle, "The vehicle is invalid."))
	{
		return;
	}
	
	//Ensure the ped is driving a vehicle.
	if(!taskVerifyf(pPed->GetIsDrivingVehicle(), "The ped is not driving a vehicle."))
	{
		return;
	}
	
	//Turn the siren on.
	pVehicle->TurnSirenOn(true);

	//Generate a random search position.
	Vec3V vSearchPosition;
	if(!GenerateRandomSearchPosition(vSearchPosition))
	{
		return;
	}

#if __BANK
	ms_debugDraw.AddSphere(vSearchPosition, 0.05f, Color_purple, 4000);
#endif

	//Create the params.
	sVehicleMissionParams params;
	params.m_iDrivingFlags = CDispatchHelperSearchInAutomobile::GetDrivingFlags();
	if(pVehicle->InheritsFromBoat())
	{
		params.m_fCruiseSpeed = CDispatchHelperSearchInBoat::GetCruiseSpeed();		
	}
	else
	{
		params.m_fCruiseSpeed = CDispatchHelperSearchInAutomobile::GetCruiseSpeed();
	}
	
	params.SetTargetPosition(VEC3V_TO_VECTOR3(vSearchPosition));
	params.m_fTargetArriveDist = 20.0f;
	
	//Create the vehicle task.
	aiTask* pVehicleTask = CVehicleIntelligence::CreateAutomobileGotoTask(params, sVehicleMissionParams::DEFAULT_SWITCH_TO_STRAIGHT_LINE_DIST);
	
	//Create the task.
	CTask* pTask = rage_new CTaskControlVehicle(pVehicle, pVehicleTask);
	
	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskPoliceOrderResponse::SearchInVehicle_OnUpdate()
{
	//Process the order state.
	if(ProcessOrderState())
	{
		//A new order has been requested, nothing else to do.
	}
	
	return FSM_Continue;
}

void CTaskPoliceOrderResponse::WanderOnFoot_OnEnter()
{
	//Generate the time to wander.
	m_fTimeToWander = GenerateTimeToWander();
	
	//Create the task.
	CTask* pTask = rage_new CTaskWander(MOVEBLENDRATIO_WALK, GetPed()->GetCurrentHeading());
	
	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskPoliceOrderResponse::WanderOnFoot_OnUpdate()
{
	//Check if the sub-task has finished, or our time has exceeded the threshold.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished) || (GetTimeInState() > m_fTimeToWander))
	{
		//Respond to the order.
		SetState(State_RespondToOrder);
	}
	
	return FSM_Continue;
}

void CTaskPoliceOrderResponse::WanderInVehicle_OnEnter()
{
	//Ensure the vehicle is valid.
	CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
	if(!taskVerifyf(pVehicle, "The vehicle is invalid."))
	{
		return;
	}
	
	//Generate the time to wander.
	m_fTimeToWander = GenerateTimeToWander();
	
	//Create the vehicle task.
	sVehicleMissionParams params;
	params.m_iDrivingFlags = DMode_AvoidCars;
	params.m_fCruiseSpeed = 25.0f;
	CTask* pVehicleTask = CVehicleIntelligence::CreateCruiseTask(*pVehicle, params);

	//Create the task.
	CTask* pTask = rage_new CTaskControlVehicle(pVehicle, pVehicleTask);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskPoliceOrderResponse::WanderInVehicle_OnUpdate()
{
	//Check if the sub-task has finished, or our time has exceeded the threshold.
	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished) || (GetTimeInState() > m_fTimeToWander))
	{
		//Respond to the order.
		SetState(State_RespondToOrder);
	}

	return FSM_Continue;
}

void CTaskPoliceOrderResponse::WaitForArrestedPed_OnEnter()
{
	CPed* pPed = GetPed();
	COrder* pOrder = pPed->GetPedIntelligence()->GetOrder();
	CPoliceOrder* pPoliceOrder = static_cast<CPoliceOrder*>(pOrder);

	if (pPoliceOrder && pPoliceOrder->GetPoliceOrderType() == CPoliceOrder::PO_ARREST_WAIT_AT_DISPATCH_LOCATION_AS_PASSENGER)
	{
		//Follow the criminal to the car
		CTask* pTaskToAdd = NULL;

		Vector3 vPedNormal = VEC3V_TO_VECTOR3(pPed->GetTransform().GetForward());
		Vector3 m_vTarget = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition())  + (vPedNormal * 10);

		CTaskMoveGoToPoint* pMoveTask = rage_new CTaskMoveGoToPoint( MOVEBLENDRATIO_WALK, m_vTarget) ;
		//CTaskGun* pAimTask = rage_new CTaskGun(CWeaponController::WCT_Aim, CTaskTypes::TASK_AIM_GUN_ON_FOOT,0, pCriminal, -1.0);
		pTaskToAdd = rage_new CTaskComplexControlMovement(pMoveTask);

		SetNewTask (pTaskToAdd);
	}
}

aiTask::FSM_Return CTaskPoliceOrderResponse::WaitForArrestedPed_Update()
{
	return FSM_Continue;
}

void CTaskPoliceOrderResponse::Combat_OnEnter()
{
	//Check if we are resuming the state.  Keep the sub-task, if possible.
	if(GetSubTask() && (GetSubTask()->GetTaskType() == CTaskTypes::TASK_THREAT_RESPONSE))
	{
		return;
	}

	//Create the task.
	CTaskThreatResponse* pTask = rage_new CTaskThreatResponse(GetTarget());
	pTask->SetThreatResponseOverride(CTaskThreatResponse::TR_Fight);
	
	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskPoliceOrderResponse::Combat_OnUpdate()
{
	//Process the order state.
	if(ProcessOrderState())
	{
		//A new order has been requested, nothing else to do.
	}
	
	return FSM_Continue;
}

void CTaskPoliceOrderResponse::WaitPulledOverInVehicle_OnEnter()
{
	//Grab the ped.
	CPed* pPed = GetPed();

	//Grab the vehicle.
	CVehicle* pVehicle = pPed->GetVehiclePedInside();
	if(!taskVerifyf(pVehicle, "Ped is not inside a vehicle."))
	{
		return;
	}
	
	//Wait for a while.
	SetNewTask(rage_new CTaskCarSetTempAction(pVehicle, TEMPACT_WAIT, (int)(sm_Tunables.m_MaxTimeToWait * 1000.0f)));
}

CTask::FSM_Return CTaskPoliceOrderResponse::WaitPulledOverInVehicle_OnUpdate()
{
	//Process the order state.
	if(ProcessOrderState())
	{
		//A new order has been requested, nothing else to do.
	}
	
	return FSM_Continue;
}

void CTaskPoliceOrderResponse::WaitCruisingInVehicle_OnEnter()
{
	//Grab the ped.
	CPed* pPed = GetPed();
	
	//Grab the vehicle.
	CVehicle* pVehicle = pPed->GetVehiclePedInside();
	if(!taskVerifyf(pVehicle, "Ped is not inside a vehicle."))
	{
		return;
	}

	//Create the task.
	static fwFlags32 s_uDrivingFlags	= DMode_StopForCars | DF_AdjustCruiseSpeedBasedOnRoadSpeed;
	static dev_float s_fCruiseSpeed		= 5.0f;
	CTask* pTask = rage_new CTaskCarDriveWander(pVehicle, s_uDrivingFlags, s_fCruiseSpeed, false);
	
	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskPoliceOrderResponse::WaitCruisingInVehicle_OnUpdate()
{
	//Process the order state.
	if(ProcessOrderState())
	{
		//A new order has been requested, nothing else to do.
	}
	
	return FSM_Continue;
}

void CTaskPoliceOrderResponse::WaitAtRoadBlock_OnEnter()
{
	//Grab the ped.
	CPed* pPed = GetPed();
	
	//Set the ped's defensive area to be small, so they wait at the road block.
	pPed->GetPedIntelligence()->GetDefensiveArea()->SetAsSphere(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), 3.0f, NULL);

	//Ensure the weapon manager is valid.
	CPedWeaponManager* pWeaponManager = pPed->GetWeaponManager();
	if(!pWeaponManager)
	{
		return;
	}

	//Equip the best weapon.
	pWeaponManager->EquipBestWeapon(true);

	//Ensure the target is valid.
	const CPed* pTarget = GetTarget();
	if(!pTarget)
	{
		return;
	}

	CVehicle* pMyVehicle = pTarget->GetMyVehicle();
	if (pMyVehicle)
	{
		m_bWantedConesDisabled = pMyVehicle->m_bDisableWantedConesResponse;
	}

	if(!m_bWantedConesDisabled)
	{
		//Create the task.
		CTask* pTask = rage_new CTaskShootAtTarget(CAITarget(pTarget), CTaskShootAtTarget::Flag_aimOnly);

		//Start the task.
		SetNewTask(pTask);
	}
}

CTask::FSM_Return CTaskPoliceOrderResponse::WaitAtRoadBlock_OnUpdate()
{
	//Process the order.
	if(!m_bWantedConesDisabled && ProcessOrder())
	{
		//A new order has been requested, nothing else to do.
	}
	//Check if the officer can see the target.
	else if(CanOfficerSeeTarget())
	{
		//Check if we should ignore target
		const CPed* pTarget = GetTarget();
		if (pTarget)
		{
			CVehicle* pMyVehicle = pTarget->GetMyVehicle();
			if (pMyVehicle && pMyVehicle->m_bDisableWantedConesResponse)
			{
				return FSM_Continue;
			}
		}

		//Restart task if flag changed from on enter
		if (m_bWantedConesDisabled)
		{
			m_bWantedConesDisabled = false;

			//Restart the state.
			SetFlag(aiTaskFlags::RestartCurrentState);
			return FSM_Continue;
		}

		//Check if the sub-task is not combat.
		const CTask* pSubTask = GetSubTask();
		if(!pSubTask || (pSubTask->GetTaskType() != CTaskTypes::TASK_COMBAT))
		{
			//Create the task.
			CTask* pTask = rage_new CTaskCombat(GetTarget(), 0.0f, CTaskCombat::ComF_IsWaitingAtRoadBlock);

			//Start the task.
			SetNewTask(pTask);
		}
	}
	
	return FSM_Continue;
}

void CTaskPoliceOrderResponse::WaitAtRoadBlock_OnExit()
{
	//Grab the ped.
	CPed* pPed = GetPed();

	//Reset the ped's defensive area.
	pPed->GetPedIntelligence()->GetDefensiveArea()->Reset();
}

void CTaskPoliceOrderResponse::MatchSpeedInVehicle_OnEnter()
{
	//Grab the ped.
	CPed* pPed = GetPed();

	//Grab the vehicle.
	CVehicle* pVehicle = pPed->GetVehiclePedInside();
	if(!taskVerifyf(pVehicle, "Ped is not inside a vehicle."))
	{
		return;
	}
	
	//Ensure the target is valid.
	const CPed* pTarget = GetTarget();
	if(!pTarget)
	{
		return;
	}
	
	//Ensure the target vehicle is valid.
	const CVehicle* pTargetVehicle = pTarget->GetVehiclePedInside();
	if(!pTargetVehicle)
	{
		return;
	}

	//Create the vehicle task.
	sVehicleMissionParams params;
	params.m_fCruiseSpeed = pTargetVehicle->GetVelocity().XYMag();
	params.m_iDrivingFlags = DMode_StopForCars | DF_AvoidTurns;
	CTask* pVehicleTask = CVehicleIntelligence::CreateCruiseTask(*pVehicle, params);
	
	//Create the task.
	CTask* pTask = rage_new CTaskControlVehicle(pVehicle, pVehicleTask);
	
	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskPoliceOrderResponse::MatchSpeedInVehicle_OnUpdate()
{
	//Process the order state.
	if(ProcessOrderState())
	{
		//A new order has been requested, nothing else to do.
	}
	
	//Set the cheat power increase.
	SetCheatPowerIncrease(sm_Tunables.m_CheatPowerIncreaseForMatchSpeed);
	
	return FSM_Continue;
}

void CTaskPoliceOrderResponse::MatchSpeedInVehicle_OnExit()
{
	//Clear the cheat power increase.
	SetCheatPowerIncrease(1.0f);
}

Vec3V_Out CTaskPoliceOrderResponse::CalculateDirectionForPed(const CPed& rPed) const
{
	//Check if the ped is in a vehicle.
	const CVehicle* pVehicle = rPed.GetVehiclePedInside();
	if(pVehicle)
	{
		return CalculateDirectionForPhysical(*pVehicle);
	}
	else
	{
		return CalculateDirectionForPhysical(rPed);
	}
}

Vec3V_Out CTaskPoliceOrderResponse::CalculateDirectionForPhysical(const CPhysical& rPhysical) const
{
	//Grab the velocity.
	Vec3V vVelocity = RCC_VEC3V(rPhysical.GetVelocity());
	
	//Grab the forward.
	Vec3V vForward = rPhysical.GetTransform().GetForward();
	
	//Calculate the direction.
	Vec3V vDirection = NormalizeFastSafe(vVelocity, vForward);
	
	return vDirection;
}

bool CTaskPoliceOrderResponse::CanOfficerSeeTarget() const
{
	return m_uRunningFlags.IsFlagSet(RF_CanSeeTarget);
}

int CTaskPoliceOrderResponse::ChooseStateToResumeTo(bool& bKeepSubTask) const
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

	//Check the previous state.
	int iPreviousState = GetPreviousState();
	switch(iPreviousState)
	{
		case State_SearchOnFoot:
		{
			//Ensure we are on foot.
			if(!GetPed()->GetIsOnFoot())
			{
				break;
			}

			return iPreviousState;
		}
		case State_SearchInVehicle:
		{
			//Ensure we are driving a vehicle.
			if(!GetPed()->GetIsDrivingVehicle())
			{
				break;
			}

			return iPreviousState;
		}
		default:
		{
			break;
		}
	}

	//Do not keep the sub-task.
	bKeepSubTask = false;

	return State_Start;
}

int CTaskPoliceOrderResponse::ConvertPoliceOrderFromInVehicleToOnFoot(int iPoliceOrderType) const
{
	//Check the police order type.
	CPoliceOrder::ePoliceDispatchOrderType nPoliceOrderType = (CPoliceOrder::ePoliceDispatchOrderType)iPoliceOrderType;
	switch(nPoliceOrderType)
	{
		case CPoliceOrder::PO_WANTED_GOTO_DISPATCH_LOCATION_AS_DRIVER:
		case CPoliceOrder::PO_WANTED_GOTO_DISPATCH_LOCATION_AS_PASSENGER:
		case CPoliceOrder::PO_WANTED_WAIT_PULLED_OVER_AS_DRIVER:
		case CPoliceOrder::PO_WANTED_WAIT_PULLED_OVER_AS_PASSENGER:
		case CPoliceOrder::PO_WANTED_WAIT_CRUISING_AS_DRIVER:
		case CPoliceOrder::PO_WANTED_WAIT_CRUISING_AS_PASSENGER:
		{
			return CPoliceOrder::PO_WANTED_GOTO_DISPATCH_LOCATION_ON_FOOT;
		}
		case CPoliceOrder::PO_WANTED_SEARCH_FOR_PED_IN_VEHICLE:
		{
			return CPoliceOrder::PO_WANTED_SEARCH_FOR_PED_ON_FOOT;
		}
		default:
		{
			taskAssertf(false, "The police order type is unknown: %d.", nPoliceOrderType);
			return nPoliceOrderType;
		}
	}
}

bool CTaskPoliceOrderResponse::DoWeCareIfWeCanSeeTheTarget() const
{
	//Check the state.
	switch(GetState())
	{
		case State_WaitPulledOverInVehicle:
		case State_WaitCruisingInVehicle:
		case State_WaitAtRoadBlock:
		case State_MatchSpeedInVehicle:
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

bool CTaskPoliceOrderResponse::DoesPoliceOrderRequireDrivingAVehicle(int iPoliceOrderType) const
{
	//Check the police order type.
	CPoliceOrder::ePoliceDispatchOrderType nPoliceOrderType = (CPoliceOrder::ePoliceDispatchOrderType)iPoliceOrderType;
	switch(nPoliceOrderType)
	{
		case CPoliceOrder::PO_WANTED_GOTO_DISPATCH_LOCATION_AS_DRIVER:
		case CPoliceOrder::PO_WANTED_SEARCH_FOR_PED_IN_VEHICLE:
		case CPoliceOrder::PO_WANTED_WAIT_PULLED_OVER_AS_DRIVER:
		case CPoliceOrder::PO_WANTED_WAIT_CRUISING_AS_DRIVER:
		{
			return true;
		}
		default:
		{
			return false;
		}
	}
}

CTaskPoliceOrderResponse::OrderState CTaskPoliceOrderResponse::GenerateOrderState() const
{
	//Check the state.
	switch(GetState())
	{
		case State_SearchOnFoot:
		{
			return GenerateOrderStateForSearchOnFoot();
		}
		case State_SearchInVehicle:
		{
			return GenerateOrderStateForSearchInVehicle();
		}
		case State_Combat:
		{
			return GenerateOrderStateForCombat();
		}
		case State_WaitPulledOverInVehicle:
		{
			return GenerateOrderStateForWaitPulledOver();
		}
		case State_WaitCruisingInVehicle:
		{
			return GenerateOrderStateForWaitCruising();
		}
		case State_MatchSpeedInVehicle:
		{
			return GenerateOrderStateForMatchSpeed();
		}
		default:
		{
			return OS_Continue;
		}
	}
}

CTaskPoliceOrderResponse::OrderState CTaskPoliceOrderResponse::GenerateOrderStateForCombat() const
{
	//Failure conditions:
	//	1) The sub-task is invalid.
	//	2) The sub-task has finished.

	//Success conditions:

	//Check the failure conditions.
	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		return OS_Failed;
	}
	
	//Check the success conditions.

	return OS_Continue;
}

CTaskPoliceOrderResponse::OrderState CTaskPoliceOrderResponse::GenerateOrderStateForMatchSpeed() const
{
	//Failure conditions:
	//	1) The sub-task has finished.
	//	2) The officer is not in a vehicle.
	//	3) The target is not in a vehicle.
	//	4) The target's vehicle is moving slowly.
	//	5) The officer can't see the target.

	//Success conditions:
	//	1) The target has passed the officer.
	//	2) The officer's vehicle is moving as fast as the target's vehicle.
	
	//Check the failure conditions.
	if(	!GetSubTask() || !IsOfficerInVehicle() || !IsTargetInVehicle() ||
		IsTargetVehicleMovingSlowly() || !CanOfficerSeeTarget())
	{
		return OS_Failed;
	}
	
	//Check the success conditions.
	if(HasTargetPassedOfficer() || IsOfficerVehicleMovingAsFastAsTargetVehicle())
	{
		return OS_Succeeded;
	}
	
	return OS_Continue;
}

CTaskPoliceOrderResponse::OrderState CTaskPoliceOrderResponse::GenerateOrderStateForSearchInVehicle() const
{
	//Failure conditions:
	//	1) The officer is not in a vehicle.
	//	2) The sub-task has finished.

	//Success conditions:

	//Check the failure conditions.
	if(!IsOfficerInVehicle() || GetIsFlagSet(aiTaskFlags::SubTaskFinished) || !GetSubTask())
	{
		return OS_Failed;
	}

	//Check the success conditions.

	return OS_Continue;
}

CTaskPoliceOrderResponse::OrderState CTaskPoliceOrderResponse::GenerateOrderStateForSearchOnFoot() const
{
	//Failure conditions:
	//	1) The sub-task has finished.

	//Success conditions:

	//Check the failure conditions.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		return OS_Failed;
	}

	//Check the success conditions.

	return OS_Continue;
}

CTaskPoliceOrderResponse::OrderState CTaskPoliceOrderResponse::GenerateOrderStateForWaitCruising() const
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
	
	//Check the failure conditions.
	if(	!GetSubTask() || HasOfficerWaitedTooLong() || !IsOfficerInVehicle() ||
		!IsTargetInVehicle() || IsTargetVehicleMovingSlowly() || HasTargetPassedOfficer())
	{
		return OS_Failed;
	}
	
	//Check the success conditions.
	if(CanOfficerSeeTarget())
	{
		static dev_float s_fMaxDistance = 45.0f;
		if(IsTargetWithinDistance(s_fMaxDistance) || WillTargetVehicleOvertakeOfficerVehicleWithinTime(sm_Tunables.m_TimeBeforeOvertakeToMatchSpeedWhenCruising))
		{
			return OS_Succeeded;
		}
	}
	
	return OS_Continue;
}

CTaskPoliceOrderResponse::OrderState CTaskPoliceOrderResponse::GenerateOrderStateForWaitPulledOver() const
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
	//	2) The target will overtake the officer soon.

	//Check the failure conditions.
	if(	!GetSubTask() || HasOfficerWaitedTooLong() || !IsOfficerInVehicle() ||
		!IsTargetInVehicle() || IsTargetVehicleMovingSlowly() || HasTargetPassedOfficer())
	{
		return OS_Failed;
	}
	
	//Check the success conditions.
	if(CanOfficerSeeTarget() && WillTargetVehicleOvertakeOfficerVehicleWithinTime(sm_Tunables.m_TimeBeforeOvertakeToMatchSpeedWhenPulledOver))
	{
		return OS_Succeeded;
	}

	return OS_Continue;
}

int CTaskPoliceOrderResponse::GeneratePoliceOrderType() const
{
	//Check the state.
	s32 iState = GetState();
	switch(iState)
	{
		case State_EnterVehicle:
		{
			return GeneratePoliceOrderTypeForEnterVehicle();
		}
		case State_GoToIncidentLocationAsPassenger:
		{
			return GeneratePoliceOrderTypeForGoToIncidentLocationAsPassenger();
		}
		case State_WaitAtRoadBlock:
		{
			return GeneratePoliceOrderTypeForWaitAtRoadBlock();
		}
		default:
		{
			taskAssertf(false, "The state is unknown: %d.", iState);
			return CPoliceOrder::PO_NONE;
		}
	}
}

int CTaskPoliceOrderResponse::GeneratePoliceOrderTypeForEnterVehicle() const
{
	//The conditions for converting the current vehicle order to on foot are:
	//	1) The officer isn't in a vehicle, and the sub-task is inactive.
	if(!IsOfficerInVehicle() && !IsSubTaskActive())
	{
		//Convert the vehicle order to on-foot.
		return ConvertPoliceOrderFromInVehicleToOnFoot(GetCurrentPoliceOrderType());
	}
	else
	{
		return CPoliceOrder::PO_NONE;
	}
}

int CTaskPoliceOrderResponse::GeneratePoliceOrderTypeForGoToIncidentLocationAsPassenger() const
{
	//The conditions for going to the dispatch location as the driver are:
	//	1) The officer is driving a vehicle.
	if(GetPed()->GetIsDrivingVehicle())
	{
		return CPoliceOrder::PO_WANTED_GOTO_DISPATCH_LOCATION_AS_DRIVER;
	}
	//The conditions for going to the dispatch location on foot are:
	//	1) The driver is valid, and exiting the vehicle.
	//	2) The officer is not in a vehicle.
	//	3) The driver has been invalid for some time (extra time is to allow shuffle).
	else if((IsDriverValid() && IsDriverExitingVehicle()) || !IsOfficerInVehicle() || (m_fTimeSinceDriverHasBeenValid > 0.5f))
	{
		return CPoliceOrder::PO_WANTED_GOTO_DISPATCH_LOCATION_ON_FOOT;
	}
	else
	{
		return CPoliceOrder::PO_NONE;
	}
}

int CTaskPoliceOrderResponse::GeneratePoliceOrderTypeForWaitAtRoadBlock() const
{
	//The conditions for going to the dispatch location on-foot are:
	//	1) The officer has waited too long.
	//	2) The target is not in a vehicle.
	if(HasOfficerWaitedTooLong() || !IsTargetInVehicle())
	{
		return CPoliceOrder::PO_WANTED_GOTO_DISPATCH_LOCATION_ON_FOOT;
	}
	//The conditions for going to the dispatch location in a vehicle are:
	//	1) The target is moving away from the officer.
	else if(IsTargetMovingAwayFromOfficer())
	{
		return CPoliceOrder::PO_WANTED_COMBAT;
	}
	else
	{
		return CPoliceOrder::PO_NONE;
	}
}

bool CTaskPoliceOrderResponse::GenerateRandomSearchPosition(Vec3V_InOut vSearchPosition)
{
	//Check if we resumed the task.
	if(GetPreviousState() == State_Resume)
	{
		//Check if we have a search location.
		if(HasSearchLocation(vSearchPosition))
		{
			//Check if the search position is inside the search area.
			if(CDispatchService::IsLocationWithinSearchArea(VEC3V_TO_VECTOR3(vSearchPosition)))
			{
				return true;
			}
		}
	}

	//Ensure the target is valid.
	const CPed* pTarget = GetTarget();
	if(!pTarget)
	{
		return false;
	}

	//Check if we are in a vehicle.
	const CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
	if(pVehicle)
	{
		//Check if we are in a heli.
		if(pVehicle->InheritsFromHeli())
		{
			if(!CDispatchHelperSearchInHeli::Calculate(*GetPed(), *pTarget, vSearchPosition))
			{
				return false;
			}
		}
		//Check if we are in a boat.
		else if(pVehicle->InheritsFromBoat())
		{
			if(!CDispatchHelperSearchInBoat::Calculate(*GetPed(), *pTarget, vSearchPosition))
			{
				return false;
			}
		}
		else
		{
			if(!CDispatchHelperSearchInAutomobile::Calculate(*GetPed(), *pTarget, vSearchPosition))
			{
				return false;
			}
		}
	}
	else
	{
		if(!CDispatchHelperSearchOnFoot::Calculate(*GetPed(), *pTarget, vSearchPosition))
		{
			return false;
		}
	}

	//Ensure the search position is inside the search area.
	if(!CDispatchService::IsLocationWithinSearchArea(VEC3V_TO_VECTOR3(vSearchPosition)))
	{
		return false;
	}

	//Set the search location.
	SetSearchLocation(vSearchPosition);

	return true;
}

float CTaskPoliceOrderResponse::GenerateTimeToWander() const
{
	//Generate the time to wander.
	float fMinTimeToWander = sm_Tunables.m_MinTimeToWander;
	float fMaxTimeToWander = sm_Tunables.m_MaxTimeToWander;
	float fTimeToWander = fwRandom::GetRandomNumberInRange(fMinTimeToWander, fMaxTimeToWander);
	
	return fTimeToWander;
}

int CTaskPoliceOrderResponse::GetCurrentPoliceOrderType() const
{
	//Grab the order.
	const COrder* pOrder = GetPed()->GetPedIntelligence()->GetOrder();
	
	//Ensure the order is a police order.
	if(!pOrder->IsPoliceOrder())
	{
		return CPoliceOrder::PO_NONE;
	}
	
	//Grab the police order.
	const CPoliceOrder* pPoliceOrder = static_cast<const CPoliceOrder *>(pOrder);
	
	return pPoliceOrder->GetPoliceOrderType();
}

CPed* CTaskPoliceOrderResponse::GetTarget() const
{
	//Ensure the entity is valid.
	COrder* pOrder = GetPed()->GetPedIntelligence()->GetOrder();
	CIncident* pIncident = pOrder->GetIncident();
	CEntity* pEntity = pIncident->GetEntity();
	if(!pEntity)
	{
		return NULL;
	}
	
	//Ensure the entity is a ped.
	if(!Verifyf(pEntity->GetIsTypePed(), "Entity is not a ped. Order type: %d, Incident Type: %d, Dispatch Type: %d, Entity Type: %d", pOrder->GetType(), pIncident->GetType(), pOrder->GetDispatchType(), pEntity->GetType()))
	{
		return NULL;
	}
	
	return static_cast<CPed *>(pEntity);
}

bool CTaskPoliceOrderResponse::HasOfficerWaitedTooLong() const
{
	return (GetTimeInState() > sm_Tunables.m_MaxTimeToWait);
}

bool CTaskPoliceOrderResponse::HasSearchLocation() const
{
	Vec3V vSearchLocation;
	return HasSearchLocation(vSearchLocation);
}

bool CTaskPoliceOrderResponse::HasSearchLocation(Vec3V_InOut vSearchLocation) const
{
	//Ensure the order is a police order.
	const COrder* pOrder = GetPed()->GetPedIntelligence()->GetOrder();
	if(!pOrder->IsPoliceOrder())
	{
		return false;
	}

	//Grab the police order.
	const CPoliceOrder* pPoliceOrder = static_cast<const CPoliceOrder *>(pOrder);

	//Ensure the search location is valid.
	if(pPoliceOrder->GetSearchLocation().IsZero())
	{
		return false;
	}

	//Set the search location.
	vSearchLocation = VECTOR3_TO_VEC3V(pPoliceOrder->GetSearchLocation());

	return true;
}

bool CTaskPoliceOrderResponse::HasTargetPassedOfficer() const
{
	//Grab the officer.
	const CPed* pOfficer = GetPed();
	
	//Ensure the target is valid.
	const CPed* pTarget = GetTarget();
	if(!pTarget)
	{
		return false;
	}
	
	//Grab the officer's vehicle.
	const CVehicle* pOfficerVehicle = pOfficer->GetVehiclePedInside();
	
	//Grab the target's vehicle.
	const CVehicle* pTargetVehicle = pTarget->GetVehiclePedInside();
	
	//Choose the officer entity.
	const CEntity* pOfficerEntity = pOfficerVehicle;
	if(!pOfficerEntity)
	{
		pOfficerEntity = pOfficer;
	}
	
	//Choose the target entity.
	const CEntity* pTargetEntity = pTargetVehicle;
	if(!pTargetEntity)
	{
		pTargetEntity = pTarget;
	}
	
	//Grab the officer position.
	Vec3V vOfficerPosition = pOfficerEntity->GetTransform().GetPosition();
	
	//Grab the target position.
	Vec3V vTargetPosition = pTargetEntity->GetTransform().GetPosition();
	
	//Generate a vector from the target to the officer.
	Vec3V vTargetToOfficer = Subtract(vOfficerPosition, vTargetPosition);
	
	//Grab the target's forward vector.
	Vec3V vTargetForward = pTargetEntity->GetTransform().GetForward();
	
	//Check if the target has passed the officer.
	ScalarV scDot = Dot(vTargetForward, vTargetToOfficer);
	if(IsLessThanAll(scDot, ScalarV(V_ZERO)))
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool CTaskPoliceOrderResponse::IsDriverExitingVehicle() const
{
	//Ensure the driver is valid.
	if(!taskVerifyf(IsDriverValid(), "The driver is invalid."))
	{
		return false;
	}

	return GetPed()->GetVehiclePedInside()->GetDriver()->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE);
}

bool CTaskPoliceOrderResponse::IsDriverInjured() const
{
	//Ensure the driver is valid.
	if(!taskVerifyf(IsDriverValid(), "The driver is invalid."))
	{
		return false;
	}
	
	return GetPed()->GetVehiclePedInside()->GetDriver()->IsInjured();
}

bool CTaskPoliceOrderResponse::IsDriverValid() const
{
	//Ensure the officer is in a vehicle.
	if(!taskVerifyf(IsOfficerInVehicle(), "The officer is not in a vehicle."))
	{
		return false;
	}
	
	return (GetPed()->GetVehiclePedInside()->GetDriver() != NULL);
}

bool CTaskPoliceOrderResponse::IsOfficerInVehicle() const
{
	//Grab the ped.
	const CPed* pPed = GetPed();
	
	//Ensure the ped is in a vehicle.
	if(!pPed->GetVehiclePedInside())
	{
		return false;
	}
	
	return true;
}

bool CTaskPoliceOrderResponse::IsOfficerVehicleMovingAsFastAsTargetVehicle() const
{
	//Grab the officer.
	const CPed* pOfficer = GetPed();

	//Ensure the officer vehicle is valid.
	const CVehicle* pOfficerVehicle = pOfficer->GetVehiclePedInside();
	if(!pOfficerVehicle)
	{
		return false;
	}
	
	//Ensure the target is valid.
	const CPed* pTarget = GetTarget();
	if(!pTarget)
	{
		return false;
	}
	
	//Ensure the target vehicle is valid.
	const CVehicle* pTargetVehicle = pTarget->GetVehiclePedInside();
	if(!pTargetVehicle)
	{
		return false;
	}
	
	//Grab the officer vehicle speed.
	float fOfficerVehicleSpeedSq = pOfficerVehicle->GetVelocity().XYMag2();
	
	//Grab the target vehicle speed.
	float fTargetVehicleSpeedSq = pTargetVehicle->GetVelocity().XYMag2();
	
	return (fOfficerVehicleSpeedSq >= fTargetVehicleSpeedSq);
}

bool CTaskPoliceOrderResponse::IsSubTaskActive() const
{
	//Ensure the sub-task is valid.
	if(!GetSubTask())
	{
		return false;
	}
	
	//Ensure the sub-task has not finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		return false;
	}
	
	return true;
}

bool CTaskPoliceOrderResponse::IsTargetInVehicle() const
{
	//Ensure the target is valid.
	const CPed* pTarget = GetTarget();
	if(!pTarget)
	{
		return false;
	}
	
	//Ensure the target is in a vehicle.
	if(!pTarget->GetVehiclePedInside())
	{
		return false;
	}
	
	return true;
}

bool CTaskPoliceOrderResponse::IsTargetMovingAwayFromOfficer() const
{
	//Ensure the target is valid.
	const CPed* pTarget = GetTarget();
	if(!pTarget)
	{
		return false;
	}
	
	//Grab the officer.
	const CPed* pOfficer = GetPed();
	
	//Grab the officer position.
	Vec3V vOfficerPosition = pOfficer->GetTransform().GetPosition();
	
	//Grab the target position.
	Vec3V vTargetPosition = pTarget->GetTransform().GetPosition();
	
	//Calculate the direction from the target to the officer.
	Vec3V vTargetToOfficer = Subtract(vOfficerPosition, vTargetPosition);
	
	//Calculate the target direction.
	Vec3V vTargetDirection = CalculateDirectionForPed(*pTarget);
	
	//Ensure the target is moving away from the officer.
	ScalarV scDot = Dot(vTargetToOfficer, vTargetDirection);
	if(IsGreaterThanAll(scDot, ScalarV(V_ZERO)))
	{
		return false;
	}
	
	return true;
}

bool CTaskPoliceOrderResponse::IsTargetVehicleMovingSlowly() const
{
	//Ensure the target is valid.
	const CPed* pTarget = GetTarget();
	if(!pTarget)
	{
		return false;
	}
	
	//Ensure the target vehicle is valid.
	const CVehicle* pVehicle = pTarget->GetVehiclePedInside();
	if(!pVehicle)
	{
		return false;
	}
	
	//Check if the vehicle is moving slowly.
	float fSpeedSq = pVehicle->GetVelocity().XYMag2();
	float fMaxSpeedSq = square(sm_Tunables.m_MaxSpeedForVehicleMovingSlowly);
	if(fSpeedSq < fMaxSpeedSq)
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool CTaskPoliceOrderResponse::IsTargetWithinDistance(float fDistance) const
{
	//Ensure the target is valid.
	const CPed* pTarget = GetTarget();
	if(!pTarget)
	{
		return false;
	}

	//Ensure the distance is valid.
	ScalarV scDistSq = DistSquared(GetPed()->GetTransform().GetPosition(), pTarget->GetTransform().GetPosition());
	ScalarV scMaxDistSq = ScalarVFromF32(square(fDistance));
	if(IsGreaterThanAll(scDistSq, scMaxDistSq))
	{
		return false;
	}

	return true;
}

void CTaskPoliceOrderResponse::ProcessCanSeeTarget()
{
	//Check if we care if we can see the target.
	if(DoWeCareIfWeCanSeeTheTarget())
	{
		//Check if the target is valid.
		CPed* pTarget = GetTarget();
		if(pTarget)
		{
			//Check the los result.
			ECanTargetResult nResult = CPedGeometryAnalyser::CanPedTargetPedAsync(*GetPed(), *pTarget);
			switch(nResult)
			{
				case ECanTarget:
				{
					m_uRunningFlags.SetFlag(RF_CanSeeTarget);
					break;
				}
				case ECanNotTarget:
				{
					m_uRunningFlags.ClearFlag(RF_CanSeeTarget);
					break;
				}
				default:
				{
					break;
				}
			}
		}
		else
		{
			m_uRunningFlags.ClearFlag(RF_CanSeeTarget);
		}
	}
	else
	{
		m_uRunningFlags.ClearFlag(RF_CanSeeTarget);
	}
}

void CTaskPoliceOrderResponse::ProcessDriver()
{
	if(!IsOfficerInVehicle() || !IsDriverValid() || IsDriverInjured())
	{
		m_fTimeSinceDriverHasBeenValid += GetTimeStep();
	}
	else
	{
		m_fTimeSinceDriverHasBeenValid = 0.0f;
	}
}

bool CTaskPoliceOrderResponse::ProcessOrder()
{
	//This is a new code path, similar to ::ProcessOrderState, except it does not require
	//going through the dispatch services to request a new order.  I think this is preferable,
	//since in this task we know our current state much better than the dispatch services do --
	//there are more possible outcomes, as opposed to just having an order succeed or fail.
	
	//Generate the police order type.
	CPoliceOrder::ePoliceDispatchOrderType nPoliceOrderType = (CPoliceOrder::ePoliceDispatchOrderType)GeneratePoliceOrderType();
	if(nPoliceOrderType == CPoliceOrder::PO_NONE)
	{
		return false;
	}
	
	//Grab the order.
	COrder* pOrder = GetPed()->GetPedIntelligence()->GetOrder();
	
	//Ensure the order type is police.
	if(!taskVerifyf(pOrder->IsPoliceOrder(), "The order type is not police: %d.", pOrder->GetType()))
	{
		return false;
	}
	
	//Grab the police order.
	CPoliceOrder* pPoliceOrder = static_cast<CPoliceOrder *>(pOrder);
	
	//Set the order type.
	pPoliceOrder->SetPoliceOrderType(nPoliceOrderType);
	
	//Respond to the order.
	SetState(State_RespondToOrder);
	
	return true;
}

bool CTaskPoliceOrderResponse::ProcessOrderState()
{
	//Generate the order state.
	OrderState nOrderState = GenerateOrderState();
	switch(nOrderState)
	{
		case OS_Failed:
		{
			RequestNewOrder(false);
			return true;
		}
		case OS_Succeeded:
		{
			RequestNewOrder(true);
			return true;
		}
		default:
		{
			return false;
		}
	}
}

void CTaskPoliceOrderResponse::RequestNewOrder(const bool bSuccess)
{
	//Request a new order.
	CPoliceAutomobileDispatch::RequestNewOrder(*GetPed(), bSuccess);
	
	//Respond to the new order.
	SetState(State_RespondToOrder);
}

void CTaskPoliceOrderResponse::SetCheatPowerIncrease(float fCheatPowerIncrease)
{
	//Grab the ped.
	CPed* pPed = GetPed();
	
	//Ensure the vehicle is valid.
	CVehicle* pVehicle = pPed->GetVehiclePedInside();
	if(!pVehicle)
	{
		return;
	}
	
	//Set the cheat power increase.
	pVehicle->SetCheatPowerIncrease(fCheatPowerIncrease);
}

void CTaskPoliceOrderResponse::SetSearchLocation(Vec3V_In vSearchLocation)
{
	//Ensure the order is a police order.
	COrder* pOrder = GetPed()->GetPedIntelligence()->GetOrder();
	if(!pOrder->IsPoliceOrder())
	{
		return;
	}

	//Grab the police order.
	CPoliceOrder* pPoliceOrder = static_cast<CPoliceOrder *>(pOrder);

	//Set the search location.
	pPoliceOrder->SetSearchLocation(VEC3V_TO_VECTOR3(vSearchLocation));
}

void CTaskPoliceOrderResponse::SetStateForPoliceOrder(const CPoliceOrder& rOrder)
{
	//Check the police order type.
	switch(rOrder.GetPoliceOrderType())
	{
		case CPoliceOrder::PO_WANTED_GOTO_DISPATCH_LOCATION_ON_FOOT:
		{
			//Check if we are in a vehicle.
			if(GetPed()->GetIsInVehicle())
			{
				//Exit the vehicle.
				SetState(State_ExitVehicle);
			}
			else
			{
				//Go to the incident location on foot.
				SetState(State_FindIncidentLocationOnFoot);
			}
			break;
		}
		case CPoliceOrder::PO_WANTED_GOTO_DISPATCH_LOCATION_AS_DRIVER:
		{
			//Check if we are not in a vehicle.
			if(!GetPed()->GetIsInVehicle())
			{
				//Enter the vehicle.
				SetState(State_EnterVehicle);
			}
			else
			{
				//Go to the incident location in a vehicle.
				SetState(State_GoToIncidentLocationInVehicle);
			}
			break;
		}
		case CPoliceOrder::PO_WANTED_GOTO_DISPATCH_LOCATION_AS_PASSENGER:
		{
			//Check if we are not in a vehicle.
			if(!GetPed()->GetIsInVehicle())
			{
				//Enter the vehicle.
				SetState(State_EnterVehicle);
			}
			else
			{
				//Go to the incident location as a passenger.
				SetState(State_GoToIncidentLocationAsPassenger);
			}
			break;
		}
		case CPoliceOrder::PO_WANTED_SEARCH_FOR_PED_ON_FOOT:
		{
			//Check if we are in a vehicle.
			if(GetPed()->GetIsInVehicle())
			{
				//Exit the vehicle.
				SetState(State_ExitVehicle);
			}
			else
			{
				//Search for the ped.
				SetState(State_FindPositionToSearchOnFoot);
			}
			break;
		}
		case CPoliceOrder::PO_WANTED_SEARCH_FOR_PED_IN_VEHICLE:
		{
			//Search for the ped in the vehicle.
			SetState(State_SearchInVehicle);
			break;
		}
		case CPoliceOrder::PO_WANTED_COMBAT:
		{
			//Enter into combat.
			SetState(State_Combat);
			break;
		}
		case CPoliceOrder::PO_WANTED_WAIT_PULLED_OVER_AS_DRIVER:
		{
			//Wait pulled over for the target.
			SetState(State_WaitPulledOverInVehicle);
			break;
		}
		case CPoliceOrder::PO_WANTED_WAIT_CRUISING_AS_DRIVER:
		{
			//Wait while cruising for the target.
			SetState(State_WaitCruisingInVehicle);
			break;
		}
		case CPoliceOrder::PO_WANTED_WAIT_PULLED_OVER_AS_PASSENGER:
		case CPoliceOrder::PO_WANTED_WAIT_CRUISING_AS_PASSENGER:
		{
			//Basically just waits for the driver to do something, state should probably be renamed to match this behavior.
			SetState(State_GoToIncidentLocationAsPassenger);
			break;
		}
		case CPoliceOrder::PO_WANTED_WAIT_AT_ROAD_BLOCK:
		{
			//Wait for the target at the road block.
			SetState(State_WaitAtRoadBlock);
			break;
		}
		default:
		{
			//No specific state for the order, just use the default.
			SetStateForDefaultOrder();
			break;
		}
	}
}

void CTaskPoliceOrderResponse::SetStateForDefaultOrder()
{
	//Grab the ped.
	CPed* pPed = GetPed();
	
	//Check if the ped is in a vehicle.
	if(pPed->GetIsInVehicle())
	{
		//Check if the ped is the driver.
		if(pPed->GetIsDrivingVehicle())
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
		SetState(State_FindIncidentLocationOnFoot);
	}
}

bool CTaskPoliceOrderResponse::ShouldIgnoreCombatEvents() const
{
	switch(GetState())
	{
		case State_WaitPulledOverInVehicle:
		case State_WaitCruisingInVehicle:
		case State_WaitAtRoadBlock:
		case State_MatchSpeedInVehicle:
		{
			return true;
		}
		default:
		{
			return false;
		}
	}
}

bool CTaskPoliceOrderResponse::ShouldSwapWeapon() const
{
	//Grab the ped.
	const CPed* pPed = GetPed();

	//Ensure the ped is not in a vehicle.
	if(pPed->GetIsInVehicle())
	{
		return false;
	}

	//Ensure the weapon manager is valid.
	const CPedWeaponManager* pWeaponManager = pPed->GetWeaponManager();
	if(!pWeaponManager)
	{
		return false;
	}

	//Ensure the weapon manager requires a weapon switch.
	if(!pWeaponManager->GetRequiresWeaponSwitch())
	{
		return false;
	}

	//Ensure we aren't already waiting for a weapon switch
	if(pWeaponManager->GetCreateWeaponObjectWhenLoaded())
	{
		return false;
	}

	//Ensure we didn't fail the swap.
	if(GetPreviousState() == State_SwapWeapon)
	{
		return false;
	}

	return true;
}

bool CTaskPoliceOrderResponse::WillTargetVehicleOvertakeOfficerVehicleWithinTime(float fTime) const
{
	//Grab the officer.
	const CPed* pOfficer = GetPed();
	
	//Ensure the officer's vehicle is valid.
	const CVehicle* pOfficerVehicle = pOfficer->GetVehiclePedInside();
	if(!pOfficerVehicle)
	{
		return false;
	}
	
	//Ensure the target is valid.
	const CPed* pTarget = GetTarget();
	if(!pTarget)
	{
		return false;
	}
	
	//Ensure the target vehicle is valid.
	const CVehicle* pTargetVehicle = pTarget->GetVehiclePedInside();
	if(!pTargetVehicle)
	{
		return false;
	}
	
	//Grab the officer vehicle's velocity.
	float fOfficerVehicleSpeed = pOfficerVehicle->GetVelocity().XYMag();
	
	//Grab the target vehicle's velocity.
	float fTargetVehicleSpeed = pTargetVehicle->GetVelocity().XYMag();
	
	//Ensure the target vehicle is moving faster than the officer.
	float fSpeedDifference = fTargetVehicleSpeed - fOfficerVehicleSpeed;
	if(fSpeedDifference <= 0.0f)
	{
		return false;
	}
	
	//Grab the distance between the officer and the target.
	float fDistance = Dist(pOfficerVehicle->GetTransform().GetPosition(), pTargetVehicle->GetTransform().GetPosition()).Getf();
	
	//Calculate the time to overtake.
	float fTimeToOvertake = fDistance / fSpeedDifference;
	
	return (fTimeToOvertake < fTime);
}

/////////////////////////////////////////////////////////////////////

CTaskArrestPed::Tunables CTaskArrestPed::sm_Tunables;
IMPLEMENT_SERVICE_TASK_TUNABLES(CTaskArrestPed, 0xd9d1b69e);

/////////////////////////////////////////////////////////////////////

CClonedTaskArrestPed2Info::CClonedTaskArrestPed2Info(s32 iState, CPed *pOtherPed, bool bArrestOtherPed)
: m_OtherPedId(pOtherPed ? pOtherPed->GetNetworkObject()->GetObjectID() : NETWORK_INVALID_OBJECT_ID)
, m_bArrestOtherPed(bArrestOtherPed)
{
	CSerialisedFSMTaskInfo::SetStatusFromMainTaskState(iState);
}

CTaskFSMClone *CClonedTaskArrestPed2Info::CreateCloneFSMTask()
{
	return rage_new CTaskArrestPed2(GetOtherPed(), m_bArrestOtherPed);
}

/////////////////////////////////////////////////////////////////////

CTaskArrestPed2::CTaskArrestPed2(CPed *pOtherPed, bool bArrestOtherPed)
: m_pOtherPed(pOtherPed)
, m_bArrestOtherPed(bArrestOtherPed)
, m_ClipSetId("arrest",0x2935D83F)
{
	SetInternalTaskType(CTaskTypes::TASK_ARREST_PED2);
}

CTaskArrestPed2::~CTaskArrestPed2()
{
}

aiTask::FSM_Return CTaskArrestPed2::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnEnter
				Start_OnEnter();
			FSM_OnUpdate
				return Start_OnUpdate();

		///////////////////////////////////////////////////////////////////////

		FSM_State(State_CopFaceCrook)
			FSM_OnEnter
				CopFaceCrook_OnEnter();
			FSM_OnUpdate
				return CopFaceCrook_OnUpdate();

		FSM_State(State_CopOrderGetUp)
			FSM_OnEnter
				CopOrderGetUp_OnEnter();
			FSM_OnUpdate
				return CopOrderGetUp_OnUpdate();

		FSM_State(State_CopWaitForGetUp)
			FSM_OnEnter
				CopWaitForGetUp_OnEnter();
			FSM_OnUpdate
				return CopWaitForGetUp_OnUpdate();

		FSM_State(State_CopThrowHandcuffs)
			FSM_OnEnter
				CopThrowHandcuffs_OnEnter();
			FSM_OnUpdate
				return CopThrowHandcuffs_OnUpdate();

		FSM_State(State_CopWaitForHandcuffsOn)
			FSM_OnEnter
				CopWaitForHandcuffsOn_OnEnter();
			FSM_OnUpdate
				return CopWaitForHandcuffsOn_OnUpdate();

		///////////////////////////////////////////////////////////////////////

		FSM_State(State_CrookWaitWhileProne)
			FSM_OnEnter
				CrookWaitWhileProne_OnEnter();
			FSM_OnUpdate
				return CrookWaitWhileProne_OnUpdate();

		FSM_State(State_CrookGetUp)
			FSM_OnEnter
				CrookGetUp_OnEnter();
			FSM_OnUpdate
				return CrookGetUp_OnUpdate();

		FSM_State(State_CrookFaceCopPutHandsUp)
			FSM_OnEnter
				CrookFaceCopPutHandsUp_OnEnter();
			FSM_OnUpdate
				return CrookFaceCopPutHandsUp_OnUpdate();

		FSM_State(State_CrookFaceCopWithHandsUp)
			FSM_OnEnter
				CrookFaceCopWithHandsUp_OnEnter();
			FSM_OnUpdate
				return CrookFaceCopWithHandsUp_OnUpdate();

		FSM_State(State_CrookWaitForHandcuffs)
			FSM_OnEnter
				CrookWaitForHandcuffs_OnEnter();
			FSM_OnUpdate
				return CrookWaitForHandcuffs_OnUpdate();

		FSM_State(State_CrookPutHandcuffsOn)
			FSM_OnEnter
				CrookPutHandcuffsOn_OnEnter();
			FSM_OnUpdate
				return CrookPutHandcuffsOn_OnUpdate();

		FSM_State(State_CrookWaitWhileStanding)
			FSM_OnEnter
				CrookWaitWhileStanding_OnEnter();
			FSM_OnUpdate
				return CrookWaitWhileStanding_OnUpdate();

		///////////////////////////////////////////////////////////////////////

		FSM_State(State_Finish)
			FSM_OnEnter
				Finish_OnEnter();
			FSM_OnUpdate
				return Finish_OnUpdate();

	FSM_End
}

aiTask::FSM_Return CTaskArrestPed2::ProcessPreFSM()
{
	if (!m_pOtherPed)
	{
		return FSM_Quit;
	}

	return FSM_Continue;
}

void CTaskArrestPed2::CleanUp()
{
	// Clear any forced aim
	GetPed()->SetPedConfigFlag(CPED_CONFIG_FLAG_ForcedAim, false);

	// Clear any lockon target
	GetPed()->SetPedResetFlag(CPED_RESET_FLAG_ClearLockonTarget, true);

	if(m_MoveNetworkHelper.GetNetworkPlayer())
	{
		GetPed()->GetMovePed().ClearSecondaryTaskNetwork(m_MoveNetworkHelper);

		m_MoveNetworkHelper.ReleaseNetworkPlayer();
	}

	m_ClipSetHelper.Release();
}

bool CTaskArrestPed2::AddGetUpSets(CNmBlendOutSetList &sets, CPed* UNUSED_PARAM(pPed))
{
	sets.Add(NMBS_ARREST_GETUPS);
	return true;
}

CTaskInfo *CTaskArrestPed2::CreateQueriableState() const
{
	return rage_new CClonedTaskArrestPed2Info(GetState(), m_pOtherPed, m_bArrestOtherPed);
}

void CTaskArrestPed2::ReadQueriableState(CClonedFSMTaskInfo *pTaskInfo)
{
	taskAssert(pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_ARREST_PED2);
	CClonedTaskArrestPed2Info *pArrestPedInfo = static_cast< CClonedTaskArrestPed2Info * >(pTaskInfo);

	m_pOtherPed = pArrestPedInfo->GetOtherPed();
	m_bArrestOtherPed = pArrestPedInfo->GetArrestOtherPed();

	CTaskFSMClone::ReadQueriableState(pTaskInfo);
}

aiTask::FSM_Return CTaskArrestPed2::UpdateClonedFSM(const s32 iState, const FSM_Event iEvent)
{
	return UpdateFSM(iState, iEvent);
}

void CTaskArrestPed2::OnCloneTaskNoLongerRunningOnOwner()
{
	SetStateFromNetwork(State_Finish);
}

#if !__FINAL

const char *CTaskArrestPed2::GetStaticStateName(s32 iState)
{
	taskAssert(iState >= State_Start && iState <= State_Finish);

	switch (iState)
	{
	case State_Start:					return "State_Start";

	case State_CopFaceCrook:			return "State_CopFaceCrook";
	case State_CopOrderGetUp:			return "State_CopOrderGetUp";
	case State_CopWaitForGetUp:			return "State_CopWaitForGetUp";
	case State_CopThrowHandcuffs:		return "State_CopThrowHandcuffs";
	case State_CopWaitForHandcuffsOn:	return "State_CopWaitForHandcuffsOn";

	case State_CrookWaitWhileProne:		return "State_CrookWaitWhileProne";
	case State_CrookGetUp:				return "State_CrookGetUp";
	case State_CrookFaceCopPutHandsUp:	return "State_CrookFaceCopPutHandsUp";
	case State_CrookFaceCopWithHandsUp:	return "State_CrookFaceCopWithHandsUp";
	case State_CrookWaitForHandcuffs:	return "State_CrookWaitForHandcuffs";
	case State_CrookPutHandcuffsOn:		return "State_CrookPutHandcuffsOn";
	case State_CrookWaitWhileStanding:	return "State_CrookWaitWhileStanding";

	case State_Finish:					return "State_Finish";

	default: taskAssert(0);
	}

	return "State_Invalid";
}

void CTaskArrestPed2::Debug() const
{
#if __BANK
	if(IsCop())
	{
		Vector3 copPedPos = GetPed()->GetGroundPos();

		if(copPedPos.IsNonZero())
		{
			grcDebugDraw::Sphere(copPedPos, 0.5f, Color_green, false);

			grcDebugDraw::Text(copPedPos + Vector3(0.0f, 0.0f, 1.0f), Color_green, GetStateName(GetState()));

			Vector3 crookPedPos = m_pOtherPed->GetGroundPos();

			if(crookPedPos.IsNonZero())
			{
				grcDebugDraw::Line(copPedPos, crookPedPos, Color_green, Color_red);
			}
		}
	}
	else
	{
		Vector3 crookPedPos = GetPed()->GetGroundPos();

		if(crookPedPos.IsNonZero())
		{
			grcDebugDraw::Sphere(crookPedPos, 0.5f, Color_red, false);

			grcDebugDraw::Text(crookPedPos + Vector3(0.0f, 0.0f, 1.0f), Color_red, GetStateName(GetState()));
		}
	}

	if(GetSubTask())
	{
		GetSubTask()->Debug();
	}
#endif // __BANK
}

#endif // !__FINAL

bool CTaskArrestPed2::IsOtherPedRunningTask() const
{
	const CTaskArrestPed2 *pTaskArrestPed2 = static_cast< const CTaskArrestPed2 * >(m_pOtherPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ARREST_PED2));
	if(pTaskArrestPed2)
	{
		return true;
	}

	return false;
}

s32 CTaskArrestPed2::GetOtherPedState() const
{
	const CTaskArrestPed2 *pTaskArrestPed2 = static_cast< const CTaskArrestPed2 * >(m_pOtherPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ARREST_PED2));
	if(pTaskArrestPed2)
	{
		return pTaskArrestPed2->GetState();
	}

	return -1;
}

bool CTaskArrestPed2::IsNetPedRunningTask() const
{
	CQueriableInterface *pQI = GetPed()->GetPedIntelligence()->GetQueriableInterface();
	if(pQI)
	{
		if(pQI->IsTaskCurrentlyRunning(CTaskTypes::TASK_ARREST_PED2, true))
		{
			return true;
		}
	}

	return false;
}

s32 CTaskArrestPed2::GetNetPedState() const
{
	CQueriableInterface *pQI = GetPed()->GetPedIntelligence()->GetQueriableInterface();
	if(pQI)
	{
		if(pQI->IsTaskCurrentlyRunning(CTaskTypes::TASK_ARREST_PED2, true))
		{
			return pQI->GetStateForTaskType(CTaskTypes::TASK_ARREST_PED2);
		}
	}

	return -1;
}

bool CTaskArrestPed2::IsPlayerLockedOn() const
{
	CPlayerInfo *pPlayerInfo = GetPed()->GetPlayerInfo();
	if(pPlayerInfo)
	{
		if(pPlayerInfo->IsAiming())
		{
			const CEntity *pTargetEntity = pPlayerInfo->GetTargeting().GetTarget();
			if(pTargetEntity)
			{
				if(pTargetEntity->GetIsTypePed())
				{
					const CPed *pTargetPed = static_cast< const CPed * >(pTargetEntity);

					if(pTargetPed == m_pOtherPed)
					{
						return true;
					}
				}
			}
		}
	}

	return false;
}

void CTaskArrestPed2::SetPlayerLockedOn()
{
	if(!GetPed()->IsNetworkClone())
	{
		// Create lock on subtask
		CTaskPlayerWeapon *pTaskPlayerWeapon = rage_new CTaskPlayerWeapon(GF_ForceAimState);
		SetNewTask(pTaskPlayerWeapon);

		CPlayerInfo *pPlayerInfo = GetPed()->GetPlayerInfo();
		if(pPlayerInfo)
		{
			// Set cop to lockon to crook
			pPlayerInfo->GetTargeting().SetLockOnTarget(m_pOtherPed);

			// Force cop to aim at crook
			GetPed()->SetPedConfigFlag(CPED_CONFIG_FLAG_ForcedAim, true);
		}
	}
}

void CTaskArrestPed2::UpdatePlayerLockedOn()
{
	if(!GetPed()->IsNetworkClone())
	{
		CPlayerInfo *pPlayerInfo = GetPed()->GetPlayerInfo();
		if(pPlayerInfo)
		{
			// Set cop to lockon to crook
			pPlayerInfo->GetTargeting().SetLockOnTarget(m_pOtherPed);
		}
	}
}

void CTaskArrestPed2::Start_OnEnter()
{
	//taskDisplayf("%u %s %s Start_OnEnter\n", fwTimer::GetFrameCount(), GetPed()->IsNetworkClone() ? "Clone" : "Local", IsCop() ? "Cop" : "Crook");

	if(IsCop())
	{
		SetPlayerLockedOn();
	}
}

aiTask::FSM_Return CTaskArrestPed2::Start_OnUpdate()
{
	//taskDisplayf("%u %s %s Start_OnUpdate\n", fwTimer::GetFrameCount(), GetPed()->IsNetworkClone() ? "Clone" : "Local", IsCop() ? "Cop" : "Crook");

	if(IsCop())
	{
		UpdatePlayerLockedOn();

		if(!GetPed()->IsNetworkClone())
		{
			if(!IsPlayerLockedOn())
			{
				taskDisplayf("Start_OnUpdate !IsPlayerLockedOn()\n");
				return FSM_Quit;
			}
		}
	}

	if(m_ClipSetId != CLIP_SET_ID_INVALID)
	{
		fwClipSet* pClipSet = fwClipSetManager::GetClipSet(m_ClipSetId);
		if(taskVerifyf(pClipSet, "Clip set [%s] doesn't exist", m_ClipSetId.GetCStr()))
		{
			if(m_ClipSetHelper.Request(m_ClipSetId))
			{
				if(IsOtherPedRunningTask())
				{
					// Clear arrest result flags
					GetPed()->SetPedConfigFlag(CPED_CONFIG_FLAG_ArrestResult, false);

					if(IsCop())
					{
						SetState(State_CopFaceCrook);
					}
					else
					{
						SetState(State_CrookWaitWhileProne);
					}
				}
			}
		}
	}

	return FSM_Continue;
}

///////////////////////////////////////////////////////////////////////

void CTaskArrestPed2::CopFaceCrook_OnEnter()
{
	//taskDisplayf("%u %s %s CopFaceCrook_OnEnter\n", fwTimer::GetFrameCount(), GetPed()->IsNetworkClone() ? "Clone" : "Local", IsCop() ? "Cop" : "Crook");

	SetPlayerLockedOn();
}

aiTask::FSM_Return CTaskArrestPed2::CopFaceCrook_OnUpdate()
{
	//taskDisplayf("%u %s %s CopFaceCrook_OnUpdate\n", fwTimer::GetFrameCount(), GetPed()->IsNetworkClone() ? "Clone" : "Local", IsCop() ? "Cop" : "Crook");

	if(!GetPed()->IsNetworkClone())
	{
		if(!IsOtherPedRunningTask())
		{
			taskDisplayf("CopFaceCrook_OnUpdate !IsOtherPedRunningTask()\n");
			return FSM_Quit;
		}
	}
	else
	{
		if(!IsNetPedRunningTask())
		{
			taskDisplayf("CopFaceCrook_OnUpdate !IsNetPedRunningTask()\n");
			return FSM_Quit;
		}
	}

	UpdatePlayerLockedOn();

	if(!GetPed()->IsNetworkClone())
	{
		if(!IsPlayerLockedOn())
		{
			taskDisplayf("CopFaceCrook_OnUpdate !IsPlayerLockedOn()\n");
			return FSM_Quit;
		}

		SetState(State_CopOrderGetUp);
	}
	else
	{
		if(GetNetPedState() >= State_CopOrderGetUp)
		{
			SetState(State_CopOrderGetUp);
		}
	}

	return FSM_Continue;
}

void CTaskArrestPed2::CopOrderGetUp_OnEnter()
{
	//taskDisplayf("%u %s %s CopOrderGetUp_OnEnter\n", fwTimer::GetFrameCount(), GetPed()->IsNetworkClone() ? "Clone" : "Local", IsCop() ? "Cop" : "Crook");

	SetPlayerLockedOn();
}

aiTask::FSM_Return CTaskArrestPed2::CopOrderGetUp_OnUpdate()
{
	//taskDisplayf("%u %s %s CopOrderGetUp_OnUpdate\n", fwTimer::GetFrameCount(), GetPed()->IsNetworkClone() ? "Clone" : "Local", IsCop() ? "Cop" : "Crook");

	if(!GetPed()->IsNetworkClone())
	{
		if(!IsOtherPedRunningTask())
		{
			taskDisplayf("CopOrderGetUp_OnUpdate !IsOtherPedRunningTask()\n");
			return FSM_Quit;
		}
	}
	else
	{
		if(!IsNetPedRunningTask())
		{
			taskDisplayf("CopOrderGetUp_OnUpdate !IsNetPedRunningTask()\n");
			return FSM_Quit;
		}
	}

	UpdatePlayerLockedOn();

	if(!GetPed()->IsNetworkClone())
	{
		if(!IsPlayerLockedOn())
		{
			taskDisplayf("CopOrderGetUp_OnUpdate !IsPlayerLockedOn()\n");
			return FSM_Quit;
		}

		SetState(State_CopWaitForGetUp);
	}
	else
	{
		if(GetNetPedState() >= State_CopWaitForGetUp)
		{
			SetState(State_CopWaitForGetUp);
		}
	}

	return FSM_Continue;
}

void CTaskArrestPed2::CopWaitForGetUp_OnEnter()
{
	//taskDisplayf("%u %s %s CopWaitForGetUp_OnEnter\n", fwTimer::GetFrameCount(), GetPed()->IsNetworkClone() ? "Clone" : "Local", IsCop() ? "Cop" : "Crook");

	SetPlayerLockedOn();
}

aiTask::FSM_Return CTaskArrestPed2::CopWaitForGetUp_OnUpdate()
{
	//taskDisplayf("%u %s %s CopWaitForGetUp_OnUpdate\n", fwTimer::GetFrameCount(), GetPed()->IsNetworkClone() ? "Clone" : "Local", IsCop() ? "Cop" : "Crook");

	if(!GetPed()->IsNetworkClone())
	{
		if(!IsOtherPedRunningTask())
		{
			taskDisplayf("CopWaitForGetUp_OnUpdate !IsOtherPedRunningTask()\n");
			return FSM_Quit;
		}
	}
	else
	{
		if(!IsNetPedRunningTask())
		{
			taskDisplayf("CopWaitForGetUp_OnUpdate !IsNetPedRunningTask()\n");
			return FSM_Quit;
		}
	}

	UpdatePlayerLockedOn();

	if(!GetPed()->IsNetworkClone())
	{
		if(!IsPlayerLockedOn())
		{
			taskDisplayf("CopWaitForGetUp_OnUpdate !IsPlayerLockedOn()\n");
			return FSM_Quit;
		}
	}

	if(!m_MoveNetworkHelper.GetNetworkPlayer())
	{
		static const fwMvNetworkDefId networkDefId("TaskArrestPed2",0x4D27A92);
		if(m_MoveNetworkHelper.RequestNetworkDef(networkDefId))
		{
			m_MoveNetworkHelper.CreateNetworkPlayer(GetPed(), networkDefId);

			m_MoveNetworkHelper.SetClipSet(m_ClipSetId);
		}
	}

	if(m_MoveNetworkHelper.GetNetworkPlayer() && GetOtherPedState() == State_CrookWaitForHandcuffs)
	{
		if(!GetPed()->IsNetworkClone())
		{
			SetState(State_CopThrowHandcuffs);
		}
		else
		{
			if(GetNetPedState() >= State_CopThrowHandcuffs)
			{
				SetState(State_CopThrowHandcuffs);
			}
		}
	}

	return FSM_Continue;
}

void CTaskArrestPed2::CopThrowHandcuffs_OnEnter()
{
	//taskDisplayf("%u %s %s CopThrowHandcuffs_OnEnter\n", fwTimer::GetFrameCount(), GetPed()->IsNetworkClone() ? "Clone" : "Local", IsCop() ? "Cop" : "Crook");

	GetPed()->GetMovePed().SetSecondaryTaskNetwork(m_MoveNetworkHelper, NORMAL_BLEND_DURATION);

	SetPlayerLockedOn();
}

aiTask::FSM_Return CTaskArrestPed2::CopThrowHandcuffs_OnUpdate()
{
	//taskDisplayf("%u %s %s CopThrowHandcuffs_OnUpdate\n", fwTimer::GetFrameCount(), GetPed()->IsNetworkClone() ? "Clone" : "Local", IsCop() ? "Cop" : "Crook");

	if(!GetPed()->IsNetworkClone())
	{
		if(!IsOtherPedRunningTask())
		{
			taskDisplayf("CopThrowHandcuffs_OnUpdate !IsOtherPedRunningTask()\n");
			return FSM_Quit;
		}
	}
	else
	{
		if(!IsNetPedRunningTask())
		{
			taskDisplayf("CopThrowHandcuffs_OnUpdate !IsNetPedRunningTask()\n");
			return FSM_Quit;
		}
	}

	UpdatePlayerLockedOn();

	if(!GetPed()->IsNetworkClone())
	{
		if(!IsPlayerLockedOn())
		{
			taskDisplayf("CopThrowHandcuffs_OnUpdate !IsPlayerLockedOn()\n");
			return FSM_Quit;
		}
	}

	if(m_MoveNetworkHelper.IsNetworkAttached())
	{
		Vector3 CopPos = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition());
		Vector3 CrookPos = VEC3V_TO_VECTOR3(m_pOtherPed->GetTransform().GetPosition());

		float fAim = 0.5f;
		if(CopPos.IsNonZero() && CrookPos.IsNonZero())
		{
			float adj = (CrookPos - CopPos).GetHorizontalMag();
			if(adj > 0.0f)
			{
				float opp = CrookPos.GetZ() - CopPos.GetZ();
				float theta = atanf(opp / adj);

				fAim = 1.0f - ((theta + (PI / 2.0f)) / PI); /* Move from -90 to +90 range to 0 to 180 range and scale to 1 to 0 range */
			}
		}

		static const fwMvFloatId aimFloatId("Aim",0xB01E2F36);
		m_MoveNetworkHelper.SetFloat(aimFloatId, fAim);

		static const fwMvBooleanId endedFloatId("Ended",0x292A3069);
		bool bEnded = m_MoveNetworkHelper.GetBoolean(endedFloatId);
		if(bEnded)
		{
			if(!GetPed()->IsNetworkClone())
			{
				SetState(State_CopWaitForHandcuffsOn);
			}
			else
			{
				if(GetNetPedState() >= State_CopWaitForHandcuffsOn)
				{
					SetState(State_CopWaitForHandcuffsOn);
				}
			}
		}
	}

	return FSM_Continue;
}

void CTaskArrestPed2::CopWaitForHandcuffsOn_OnEnter()
{
	//taskDisplayf("%u %s %s CopWaitForHandcuffsOn_OnEnter\n", fwTimer::GetFrameCount(), GetPed()->IsNetworkClone() ? "Clone" : "Local", IsCop() ? "Cop" : "Crook");

	GetPed()->GetMovePed().ClearTaskNetwork(m_MoveNetworkHelper, NORMAL_BLEND_DURATION);

	SetPlayerLockedOn();
}

aiTask::FSM_Return CTaskArrestPed2::CopWaitForHandcuffsOn_OnUpdate()
{
	//taskDisplayf("%u %s %s CopWaitForHandcuffsOn_OnUpdate\n", fwTimer::GetFrameCount(), GetPed()->IsNetworkClone() ? "Clone" : "Local", IsCop() ? "Cop" : "Crook");

	UpdatePlayerLockedOn();

	if(!GetPed()->IsNetworkClone())
	{
		if(!IsPlayerLockedOn())
		{
			taskDisplayf("CopWaitForHandcuffsOn_OnUpdate !IsPlayerLockedOn()\n");
			return FSM_Quit;
		}
	}

	if(!IsOtherPedRunningTask() || GetOtherPedState() == State_CrookWaitWhileStanding)
	{
		SetState(State_Finish);
	}

	return FSM_Continue;
}

///////////////////////////////////////////////////////////////////////

void CTaskArrestPed2::CrookWaitWhileProne_OnEnter()
{
	//taskDisplayf("%u %s %s CrookWaitWhileProne_OnEnter\n", fwTimer::GetFrameCount(), GetPed()->IsNetworkClone() ? "Clone" : "Local", IsCop() ? "Cop" : "Crook");

	//CTaskNMRelax *pTaskNMRelax = rage_new CTaskNMRelax(USHRT_MAX, USHRT_MAX);

	//CTaskNMControl *pTaskNMControl = rage_new CTaskNMControl(USHRT_MAX, USHRT_MAX, pTaskNMRelax, CTaskNMControl::DO_BLEND_FROM_NM);

	//SetNewTask(pTaskNMControl);
}

aiTask::FSM_Return CTaskArrestPed2::CrookWaitWhileProne_OnUpdate()
{
	//taskDisplayf("%u %s %s CrookWaitWhileProne_OnUpdate\n", fwTimer::GetFrameCount(), GetPed()->IsNetworkClone() ? "Clone" : "Local", IsCop() ? "Cop" : "Crook");

	if(!GetPed()->IsNetworkClone())
	{
		if(!IsOtherPedRunningTask())
		{
			taskDisplayf("CrookWaitWhileProne_OnUpdate !IsOtherPedRunningTask()\n");
			return FSM_Quit;
		}
	}
	else
	{
		if(!IsNetPedRunningTask())
		{
			taskDisplayf("CrookWaitWhileProne_OnUpdate !IsNetPedRunningTask()\n");
			return FSM_Quit;
		}
	}

	//if(!FindSubTaskOfType(CTaskTypes::TASK_NM_CONTROL))
	{
		if(!GetPed()->IsNetworkClone())
		{
			SetState(State_CrookGetUp);
		}
		else
		{
			if(GetNetPedState() >= State_CrookGetUp)
			{
				SetState(State_CrookGetUp);
			}
		}
	}

	return FSM_Continue;
}

void CTaskArrestPed2::CrookGetUp_OnEnter()
{
	//taskDisplayf("%u %s %s CrookGetUp_OnEnter\n", fwTimer::GetFrameCount(), GetPed()->IsNetworkClone() ? "Clone" : "Local", IsCop() ? "Cop" : "Crook");

	CTaskBlendFromNM *pTaskBlendFromNM = rage_new CTaskBlendFromNM();

	SetNewTask(pTaskBlendFromNM);
}

aiTask::FSM_Return CTaskArrestPed2::CrookGetUp_OnUpdate()
{
	//taskDisplayf("%u %s %s CrookGetUp_OnUpdate\n", fwTimer::GetFrameCount(), GetPed()->IsNetworkClone() ? "Clone" : "Local", IsCop() ? "Cop" : "Crook");

	if(!GetPed()->IsNetworkClone())
	{
		if(!IsOtherPedRunningTask())
		{
			taskDisplayf("CrookGetUp_OnUpdate !IsOtherPedRunningTask()\n");
			return FSM_Quit;
		}
	}
	else
	{
		if(!IsNetPedRunningTask())
		{
			taskDisplayf("CrookGetUp_OnUpdate !IsNetPedRunningTask()\n");
			return FSM_Quit;
		}
	}

	GetPed()->SetDesiredHeading(VEC3V_TO_VECTOR3(m_pOtherPed->GetTransform().GetPosition()));

	if(!FindSubTaskOfType(CTaskTypes::TASK_BLEND_FROM_NM))
	{
		if(!GetPed()->IsNetworkClone())
		{
			SetState(State_CrookFaceCopPutHandsUp);
		}
		else
		{
			if(GetNetPedState() >= State_CrookFaceCopPutHandsUp)
			{
				SetState(State_CrookFaceCopPutHandsUp);
			}
		}
	}

	return FSM_Continue;
}

void CTaskArrestPed2::CrookFaceCopPutHandsUp_OnEnter()
{
	//taskDisplayf("%u %s %s CrookFaceCopPutHandsUp_OnEnter\n", fwTimer::GetFrameCount(), GetPed()->IsNetworkClone() ? "Clone" : "Local", IsCop() ? "Cop" : "Crook");

	if(!GetPed()->IsNetworkClone())
	{
		GetPed()->SetDesiredHeading(VEC3V_TO_VECTOR3(m_pOtherPed->GetTransform().GetPosition()));
	}

	const u32 clipDictionaryHash = ATSTRINGHASH("ped", 0x034d90761);
	s32 clipDictionaryIndex = fwAnimManager::FindSlotFromHashKey(clipDictionaryHash).Get();
	taskAssertf(clipDictionaryIndex != -1, "Couldn't find hands up dictionary");
	const u32 clipHash = ATSTRINGHASH("handsup_enter", 0x0ee846292);
	taskVerifyf(fwAnimManager::GetClipIfExistsByDictIndex(clipDictionaryIndex, clipHash), "Couldn't find handsup_enter clip");
	StartClipByDictAndHash(GetPed(), clipDictionaryIndex, clipHash, APF_ISFINISHAUTOREMOVE | APF_SKIP_NEXT_UPDATE_BLEND, (u32)BONEMASK_UPPERONLY, AP_MEDIUM, NORMAL_BLEND_IN_DELTA, CClipHelper::TerminationType_OnFinish);
}

aiTask::FSM_Return CTaskArrestPed2::CrookFaceCopPutHandsUp_OnUpdate()
{
	//taskDisplayf("%u %s %s CrookFaceCopPutHandsUp_OnUpdate\n", fwTimer::GetFrameCount(), GetPed()->IsNetworkClone() ? "Clone" : "Local", IsCop() ? "Cop" : "Crook");

	if(!GetPed()->IsNetworkClone())
	{
		if(!IsOtherPedRunningTask())
		{
			taskDisplayf("CrookFaceCopPutHandsUp_OnUpdate !IsOtherPedRunningTask()\n");
			return FSM_Quit;
		}
	}
	else
	{
		if(!IsNetPedRunningTask())
		{
			taskDisplayf("CrookFaceCopPutHandsUp_OnUpdate !IsNetPedRunningTask()\n");
			return FSM_Quit;
		}
	}

	if(!GetPed()->IsNetworkClone())
	{
		GetPed()->SetDesiredHeading(VEC3V_TO_VECTOR3(m_pOtherPed->GetTransform().GetPosition()));
	}

	if(!GetClipHelper())
	{
		if(!GetPed()->IsNetworkClone())
		{
			SetState(State_CrookFaceCopWithHandsUp);
		}
		else
		{
			if(GetNetPedState() >= State_CrookFaceCopWithHandsUp)
			{
				SetState(State_CrookFaceCopWithHandsUp);
			}
		}
	}

	return FSM_Continue;
}

void CTaskArrestPed2::CrookFaceCopWithHandsUp_OnEnter()
{
	//taskDisplayf("%u %s %s CrookFaceCopWithHandsUp_OnEnter\n", fwTimer::GetFrameCount(), GetPed()->IsNetworkClone() ? "Clone" : "Local", IsCop() ? "Cop" : "Crook");

	if(!GetPed()->IsNetworkClone())
	{
		GetPed()->SetDesiredHeading(VEC3V_TO_VECTOR3(m_pOtherPed->GetTransform().GetPosition()));
	}

	const u32 clipDictionaryHash = ATSTRINGHASH("ped", 0x034d90761);
	s32 clipDictionaryIndex = fwAnimManager::FindSlotFromHashKey(clipDictionaryHash).Get();
	taskAssertf(clipDictionaryIndex != -1, "Couldn't find hands up dictionary");
	const u32 clipHash = ATSTRINGHASH("handsup_base", 0x0820fe9cb);
	taskVerifyf(fwAnimManager::GetClipIfExistsByDictIndex(clipDictionaryIndex, clipHash), "Couldn't find handsup_enter clip");
	StartClipByDictAndHash(GetPed(), clipDictionaryIndex, clipHash, APF_ISLOOPED, (u32)BONEMASK_UPPERONLY, AP_MEDIUM, INSTANT_BLEND_IN_DELTA, CClipHelper::TerminationType_OnFinish);
}

aiTask::FSM_Return CTaskArrestPed2::CrookFaceCopWithHandsUp_OnUpdate()
{
	//taskDisplayf("%u %s %s CrookFaceCopWithHandsUp_OnUpdate\n", fwTimer::GetFrameCount(), GetPed()->IsNetworkClone() ? "Clone" : "Local", IsCop() ? "Cop" : "Crook");

	if(!GetPed()->IsNetworkClone())
	{
		if(!IsOtherPedRunningTask())
		{
			taskDisplayf("CrookFaceCopWithHandsUp_OnUpdate !IsOtherPedRunningTask()\n");
			return FSM_Quit;
		}
	}
	else
	{
		if(!IsNetPedRunningTask())
		{
			taskDisplayf("CrookFaceCopWithHandsUp_OnUpdate !IsNetPedRunningTask()\n");
			return FSM_Quit;
		}
	}

	if(!GetPed()->IsNetworkClone())
	{
		GetPed()->SetDesiredHeading(VEC3V_TO_VECTOR3(m_pOtherPed->GetTransform().GetPosition()));

		float fHeading = GetPed()->GetTransform().GetHeading();
		float fDesiredHeading = GetPed()->GetDesiredHeading();
		if(AreNearlyEqual(fHeading, fDesiredHeading, 10.0f * DtoR))
		{
			SetState(State_CrookWaitForHandcuffs);
		}
	}
	else
	{
		if(GetNetPedState() >= State_CrookWaitForHandcuffs)
		{
			SetState(State_CrookWaitForHandcuffs);
		}
	}

	return FSM_Continue;
}

void CTaskArrestPed2::CrookWaitForHandcuffs_OnEnter()
{
	//taskDisplayf("%u %s %s CrookWaitForHandcuffs_OnEnter\n", fwTimer::GetFrameCount(), GetPed()->IsNetworkClone() ? "Clone" : "Local", IsCop() ? "Cop" : "Crook");
}

aiTask::FSM_Return CTaskArrestPed2::CrookWaitForHandcuffs_OnUpdate()
{
	//taskDisplayf("%u %s %s CrookWaitForHandcuffs_OnUpdate\n", fwTimer::GetFrameCount(), GetPed()->IsNetworkClone() ? "Clone" : "Local", IsCop() ? "Cop" : "Crook");

	if(!GetPed()->IsNetworkClone())
	{
		if(!IsOtherPedRunningTask())
		{
			taskDisplayf("CrookWaitForHandcuffs_OnUpdate !IsOtherPedRunningTask()\n");
			return FSM_Quit;
		}
	}
	else
	{
		if(!IsNetPedRunningTask())
		{
			taskDisplayf("CrookWaitForHandcuffs_OnUpdate !IsNetPedRunningTask()\n");
			return FSM_Quit;
		}
	}

	if(GetOtherPedState() == State_CopThrowHandcuffs)
	{
		SetState(State_CrookPutHandcuffsOn);
	}

	return FSM_Continue;
}

void CTaskArrestPed2::CrookPutHandcuffsOn_OnEnter()
{
	//taskDisplayf("%u %s %s CrookPutHandcuffsOn_OnEnter\n", fwTimer::GetFrameCount(), GetPed()->IsNetworkClone() ? "Clone" : "Local", IsCop() ? "Cop" : "Crook");

	static const fwMvClipId clipId("ARREST_FALLBACK_MID_crook",0xED0D1FFD);
	if(taskVerifyf(fwAnimManager::GetClipIfExistsBySetId(m_ClipSetId, clipId), "Clip not found"))
	{
		StartClipBySetAndClip(GetPed(), m_ClipSetId, clipId, APF_ISFINISHAUTOREMOVE, BONEMASK_ALL, AP_MEDIUM, NORMAL_BLEND_IN_DELTA, CClipHelper::TerminationType_OnFinish);
	}
}

aiTask::FSM_Return CTaskArrestPed2::CrookPutHandcuffsOn_OnUpdate()
{
	//taskDisplayf("%u %s %s CrookPutHandcuffsOn_OnUpdate\n", fwTimer::GetFrameCount(), GetPed()->IsNetworkClone() ? "Clone" : "Local", IsCop() ? "Cop" : "Crook");

	if(!GetPed()->IsNetworkClone())
	{
		if(!IsOtherPedRunningTask())
		{
			taskDisplayf("CrookPutHandcuffsOn_OnUpdate !IsOtherPedRunningTask()\n");
			return FSM_Quit;
		}
	}
	else
	{
		if(!IsNetPedRunningTask())
		{
			taskDisplayf("CrookPutHandcuffsOn_OnUpdate !IsNetPedRunningTask()\n");
			return FSM_Quit;
		}
	}

	if(!GetClipHelper())
	{
		if(!GetPed()->IsNetworkClone())
		{
			SetState(State_CrookWaitWhileStanding);
		}
		else
		{
			if(GetNetPedState() >= State_CrookWaitWhileStanding)
			{
				SetState(State_CrookWaitWhileStanding);
			}
		}
	}

	return FSM_Continue;
}

void CTaskArrestPed2::CrookWaitWhileStanding_OnEnter()
{
	//taskDisplayf("%u %s %s CrookWaitWhileStanding_OnEnter\n", fwTimer::GetFrameCount(), GetPed()->IsNetworkClone() ? "Clone" : "Local", IsCop() ? "Cop" : "Crook");
}

aiTask::FSM_Return CTaskArrestPed2::CrookWaitWhileStanding_OnUpdate()
{
	//taskDisplayf("%u %s %s CrookWaitWhileStanding_OnUpdate\n", fwTimer::GetFrameCount(), GetPed()->IsNetworkClone() ? "Clone" : "Local", IsCop() ? "Cop" : "Crook");

	if(!IsOtherPedRunningTask() || GetOtherPedState() == State_Finish)
	{
		SetState(State_Finish);
	}

	return FSM_Continue;
}

///////////////////////////////////////////////////////////////////////

void CTaskArrestPed2::Finish_OnEnter()
{
	//taskDisplayf("%u %s %s Finish_OnEnter\n", fwTimer::GetFrameCount(), GetPed()->IsNetworkClone() ? "Clone" : "Local", IsCop() ? "Cop" : "Crook");

	// Set arrest result flags
	GetPed()->SetPedConfigFlag(CPED_CONFIG_FLAG_ArrestResult, true);

	CleanUp();
}

aiTask::FSM_Return CTaskArrestPed2::Finish_OnUpdate()
{
	//taskDisplayf("%u %s %s Finish_OnUpdate\n", fwTimer::GetFrameCount(), GetPed()->IsNetworkClone() ? "Clone" : "Local", IsCop() ? "Cop" : "Crook");

	return FSM_Quit;
}

/////////////////////////////////////////////////////////////////////

CVehicleClipRequestHelper CTaskArrestPed::ms_VehicleClipRequestHelper;

CTaskArrestPed::CTaskArrestPed(const CPed* pPedToArrest, bool bForceArrest, bool bQuitOnCombat)
: m_pPedToArrest(pPedToArrest)
, m_bWillArrest(false)
, m_bForceArrest(bForceArrest)
, m_bQuitOnCombat(bQuitOnCombat)
, m_bPedIsResistingArrest(false)
, m_bTryToTalkTargetDown(true)
, m_bIsApproachingTarget(false)
, m_fTalkingTargetDownTimer(0.0f)
, m_fTimeTargetStill(0.0f)
, m_fTimeUntilNextDialogue(0.0f)
, m_bHasCheckCanArrestInVehicle(false)
, m_bCanArrestTargetInVehicle(false)
, m_bWasTargetEnteringVehicleLastFrame(false)
{
	m_vTargetPositionWhenBeingTalkedDown.Zero();
	SetInternalTaskType(CTaskTypes::TASK_ARREST_PED);
}

CTaskArrestPed::~CTaskArrestPed()
{

}

void CTaskArrestPed::CleanUp()
{
	GetPed()->SetPedConfigFlag(CPED_CONFIG_FLAG_WillArrestRatherThanJack, false);
	SetPedIsArrestingTarget(false);
}

bool CTaskArrestPed::ShouldAbort(const AbortPriority UNUSED_PARAM(iPriority), const aiEvent* pEvent)
{
	return OnShouldAbort(pEvent);
}

bool CTaskArrestPed::OnShouldAbort(const aiEvent* pEvent)
{
	if (pEvent)
	{
		eEventType eventType = static_cast<eEventType>(static_cast<const CEvent*>(pEvent)->GetEventType());
		if ( eventType == EVENT_SHOT_FIRED || eventType == EVENT_GUN_AIMED_AT ||
			 eventType == EVENT_SHOT_FIRED_BULLET_IMPACT || eventType == EVENT_SHOT_FIRED_WHIZZED_BY )
		{
			m_bPedIsResistingArrest = true; // Might be someone else generating the event, but need to go into combat anyway
			return false;
		}
	}

	return true;
}

aiTask::FSM_Return CTaskArrestPed::ProcessPreFSM()
{
	if (!m_pPedToArrest)
	{
		return FSM_Quit;
	}

	// Cops stop chasing the player when wanted level is cleared 
	CPed* pPed = GetPed();

	if (pPed->GetPedType() == PEDTYPE_COP && pPed->PopTypeIsRandom() && 
		m_pPedToArrest->IsLocalPlayer() && m_pPedToArrest->GetPlayerWanted()->GetWantedLevel() == WANTED_CLEAN)
	{
		return FSM_Quit;
	}

	// Do no arrest if player controls are disabled
	if (m_pPedToArrest->IsLocalPlayer() && m_pPedToArrest->GetPlayerInfo()->AreControlsDisabled())
	{
		return FSM_Quit;
	}

	// Make sure the targeting system is activated.
	CPedTargetting* pPedTargeting = pPed->GetPedIntelligence()->GetTargetting( true );
	if (pPedTargeting)
	{
		pPedTargeting->SetInUse();
	}

	if(IsTargetStill())
	{
		m_fTimeTargetStill += GetTimeStep();
	}
	else
	{
		m_fTimeTargetStill = 0.0f;
	}

	// See if we can arrest in the target's current vehicle
	if(m_pPedToArrest->GetIsInVehicle())
	{
		bool bCanCheckArrestInVehicle = !m_bHasCheckCanArrestInVehicle || (m_bWasTargetEnteringVehicleLastFrame && !m_pPedToArrest->GetIsEnteringVehicle(true));
		if(bCanCheckArrestInVehicle && (m_bWillArrest || m_bForceArrest))
		{
			m_bHasCheckCanArrestInVehicle = true;
			m_bCanArrestTargetInVehicle = CanArrestPedInVehicle(GetPed(), m_pPedToArrest, true, true, true);
		}
	}
	else
	{
		m_bHasCheckCanArrestInVehicle = false;
		m_bCanArrestTargetInVehicle = false;
	}

	// Attempt to stream the clip set
	StreamClipSet();

	return FSM_Continue;
}

aiTask::FSM_Return CTaskArrestPed::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();	

		FSM_State(State_ExitVehicle)
			FSM_OnEnter
				ExitVehicle_OnEnter();
			FSM_OnUpdate
				return ExitVehicle_OnUpdate();

		FSM_State(State_MoveToTarget)
			FSM_OnEnter
				MoveToTarget_OnEnter();
			FSM_OnUpdate
				return MoveToTarget_OnUpdate();

		FSM_State(State_ArrestInVehicleTarget)
			FSM_OnEnter
				ArrestInVehicleTarget_OnEnter();
			FSM_OnUpdate
				return ArrestInVehicleTarget_OnUpdate();

		FSM_State(State_MoveToTargetInVehicle)
			FSM_OnEnter
				MoveToTargetInVehicle_OnEnter();
			FSM_OnUpdate
				return MoveToTargetInVehicle_OnUpdate();

		FSM_State(State_AimAtTarget)
			FSM_OnEnter
				AimAtTarget_OnEnter();
			FSM_OnUpdate
				return AimAtTarget_OnUpdate();

		FSM_State(State_WaitForRestart)
			FSM_OnEnter
				WaitForRestart_OnEnter();
			FSM_OnUpdate
				return WaitForRestart_OnUpdate();

		FSM_State(State_Combat)
			FSM_OnEnter
				Combat_OnEnter();
			FSM_OnUpdate
				return Combat_OnUpdate();

		FSM_State(State_SwapWeapon)
			FSM_OnEnter
				SwapWeapon_OnEnter();
		FSM_OnUpdate
				return SwapWeapon_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

aiTask::FSM_Return CTaskArrestPed::ProcessPostFSM()
{
	m_bWasTargetEnteringVehicleLastFrame = (m_pPedToArrest && m_pPedToArrest->GetIsEnteringVehicle(true));
	return FSM_Continue;
}

aiTask::FSM_Return CTaskArrestPed::Start_OnUpdate()
{
	CPed* pPed = GetPed();

	if(CanPlayArrestAudio())
	{
		if(fwRandom::GetRandomTrueFalse())
		{
			pPed->NewSay("COP_TALKDOWN");
		}
		else if(m_pPedToArrest->GetIsOnFoot())
		{
			pPed->NewSay("SURROUNDED");
		}
	}

	// Find out which clip set to request based on our equipped weapon or best weapon and then request it
	m_ExitToAimClipSetId = CTaskVehicleFSM::GetExitToAimClipSetIdForPed(*pPed);

	if (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
	{
		if( DistSquared(pPed->GetTransform().GetPosition(), m_pPedToArrest->GetTransform().GetPosition()).Getf() < square(sm_Tunables.m_MoveToDistanceInVehicle) &&
			(!m_pPedToArrest->GetIsInVehicle() || m_fTimeTargetStill > 0.0f || IsTargetBeingBusted()) )
		{
			SetState(State_ExitVehicle);
		}
		else
		{
			SetState(State_MoveToTargetInVehicle);
		}

		return FSM_Continue;
	}

	if ( !m_bWillArrest &&
		 (m_bForceArrest || !IsAnotherCopCloserToTarget()) )
	{
		SetPedIsArrestingTarget(true);
	}

	SetState(CheckForDesiredArrestState());
	return FSM_Continue;
}

void CTaskArrestPed::ExitVehicle_OnEnter()
{
	CPed* pPed = GetPed();
	CVehicle* pPedsVehicle = pPed->GetMyVehicle();
	if (taskVerifyf(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pPedsVehicle, "Expected ped to be in a vehicle"))
	{
		VehicleEnterExitFlags vehicleFlags;
		vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DontCloseDoor);
		vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::QuitIfAllDoorsBlocked);
		vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::WarpIfDoorBlocked);
		vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::ExitToAim);
		CTaskExitVehicle* pTask = rage_new CTaskExitVehicle(pPedsVehicle, vehicleFlags);
		pTask->SetDesiredTarget(m_pPedToArrest);
		SetNewTask(pTask);
	}
}

aiTask::FSM_Return CTaskArrestPed::ExitVehicle_OnUpdate()
{
	if (!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Start);
	}
	return FSM_Continue;
}

void CTaskArrestPed::ArrestInVehicleTarget_OnEnter()
{
	CVehicle* pVehicle = m_pPedToArrest->GetVehiclePedInside();
	if(pVehicle)
	{
		GetPed()->SetPedConfigFlag(CPED_CONFIG_FLAG_WillArrestRatherThanJack, true);
		VehicleEnterExitFlags vehicleFlags;
		vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::UseDirectEntryOnly);
		SetNewTask(rage_new CTaskEnterVehicle(pVehicle, SR_Specific, pVehicle->GetDriverSeat(), vehicleFlags, 0.0f, MOVEBLENDRATIO_RUN, m_pPedToArrest));
	}
}

aiTask::FSM_Return CTaskArrestPed::ArrestInVehicleTarget_OnUpdate()
{
	bool bWasArresting = m_bWillArrest;
	if (m_bForceArrest || !IsAnotherCopCloserToTarget())
	{
		SetPedIsArrestingTarget(true);
	}

	if (bWasArresting != m_bWillArrest)
	{
		SetState(CheckForDesiredArrestState());
		return FSM_Continue;
	}

	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_AimAtTarget);
		return FSM_Continue;
	}

	if(m_bPedIsResistingArrest)
	{
		SetState(State_Combat);
		return FSM_Continue;
	}

	// If we are supposed to arrest but aren't running the correct arrest task then we need to restart;
	if(!m_pPedToArrest->GetVehiclePedInside())
	{
		CTaskExitVehicleSeat* pExitVehicleSeatTask = static_cast<CTaskExitVehicleSeat*>(m_pPedToArrest->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE_SEAT));
		if(!pExitVehicleSeatTask || !pExitVehicleSeatTask->IsBeingArrested())
		{
			SetState(State_MoveToTarget);
			return FSM_Continue;
		}
	}
	else if(m_fTimeTargetStill <= 0.0f)
	{
		SetState(m_bQuitOnCombat ? State_Finish : State_Combat);
	}

	UpdateTalkTargetDown();

	return FSM_Continue;
}

void CTaskArrestPed::MoveToTarget_OnEnter()
{
	CPed* pPed = GetPed();

	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_WillArrestRatherThanJack, false);
	m_bIsApproachingTarget = false;

	// Need to make sure we have a weapon out (just calling EquipBestWeapon wasn't working)
	Assert(pPed->GetWeaponManager());
	if(!pPed->GetWeaponManager()->GetEquippedWeaponObject())
	{
		pPed->GetWeaponManager()->CreateEquippedWeaponObject();
	}

	ScalarV scDistToTargetSq = DistSquared(pPed->GetTransform().GetPosition(), m_pPedToArrest->GetTransform().GetPosition());
	if(IsLessThanAll(scDistToTargetSq, ScalarV(V_ONE)))
	{
		Vec3V vGoToPosition = pPed->GetTransform().GetPosition() - pPed->GetTransform().GetForward();
		CTaskMoveGoToPoint* pGoToTask = rage_new CTaskMoveGoToPoint(MOVEBLENDRATIO_WALK, VEC3V_TO_VECTOR3(vGoToPosition), 0.25f);
		CTaskCombatAdditionalTask* pAimTask = rage_new CTaskCombatAdditionalTask(CTaskCombatAdditionalTask::RT_DefaultNoFiring, m_pPedToArrest, VEC3V_TO_VECTOR3(m_pPedToArrest->GetTransform().GetPosition()));
		SetNewTask(rage_new CTaskComplexControlMovement(pGoToTask, pAimTask));
	}
	else
	{
		m_bIsApproachingTarget = true;
		m_uTimeMoveOnFootSpeedSet = fwTimer::GetTimeInMilliseconds();
		float fTargetRadius = m_bWillArrest ? 1.5f : sm_Tunables.m_AimDistance;
		float fDesiredMoveBlendRatio = m_pPedToArrest->GetMotionData()->GetIsSprinting() && IsTargetFacingAway() ? MOVEBLENDRATIO_SPRINT : MOVEBLENDRATIO_RUN;

		CTaskGoToPointAiming* pGoToTask = rage_new CTaskGoToPointAiming(CAITarget(m_pPedToArrest), CAITarget(m_pPedToArrest), fDesiredMoveBlendRatio);
		pGoToTask->SetTargetRadius(fTargetRadius);
		pGoToTask->SetConfigFlags(CTaskGoToPointAiming::CF_DontAimIfSprinting|CTaskGoToPointAiming::CF_DontAimIfTargetLosBlocked);
		SetNewTask(pGoToTask);
	}
}

aiTask::FSM_Return CTaskArrestPed::MoveToTarget_OnUpdate()
{
	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_AimAtTarget);
		return FSM_Continue;
	}

	if(m_bPedIsResistingArrest)
	{
		SetState(State_Combat);
		return FSM_Continue;
	}

	// Have to specifically check the vehicle resisting as sometimes we don't want to increase the WL when quitting out
	if(m_pPedToArrest->GetIsInVehicle() && (m_fTimeTargetStill <= 0.0f  || !m_bCanArrestTargetInVehicle))
	{
		SetState(m_bQuitOnCombat ? State_Finish : State_Combat);
		return FSM_Continue;
	}

	bool bWasArresting = m_bWillArrest;
	if (m_bForceArrest || !IsAnotherCopCloserToTarget())
	{
		SetPedIsArrestingTarget(true);
	}

	ScalarV scDistToTargetSq = DistSquared(GetPed()->GetTransform().GetPosition(), m_pPedToArrest->GetTransform().GetPosition());
	ScalarV scDesiredDistSq = m_bWillArrest ? ScalarVFromF32(sm_Tunables.m_ArrestDistance) : ScalarVFromF32(sm_Tunables.m_AimDistance);
	scDesiredDistSq = (scDesiredDistSq * scDesiredDistSq);

	if (bWasArresting != m_bWillArrest)
	{
		SetState(CheckForDesiredArrestState());
		return FSM_Continue;
	}
	else if(m_bWillArrest)
	{
		// If we are supposed to arrest but aren't running the correct arrest task then we need to restart;
		if(m_bCanArrestTargetInVehicle)
		{
			SetState(State_ArrestInVehicleTarget);
			return FSM_Continue;			
		}
		else
		{
			CPedTargetting* pPedTargeting = GetPed()->GetPedIntelligence()->GetTargetting(true);
			if(pPedTargeting && pPedTargeting->GetLosStatus(m_pPedToArrest, false) == Los_clear)
			{
				if(m_bIsApproachingTarget)
				{
					if(IsLessThanAll(scDistToTargetSq, scDesiredDistSq))
					{
						SetState(State_AimAtTarget);
						return FSM_Continue;
					}
				}
				else if(IsGreaterThanAll(scDistToTargetSq, ScalarV(V_ONE)))
				{
					SetState(State_AimAtTarget);
					return FSM_Continue;
				}
			}
		}
	}
	else if(m_bIsApproachingTarget && IsLessThanAll(scDistToTargetSq, scDesiredDistSq))
	{
		SetState(State_AimAtTarget);
		return FSM_Continue;
	}

	// Sprint if the target is sprinting, otherwise just run
	CTaskGoToPointAiming* pGotoTask = static_cast<CTaskGoToPointAiming*>(FindSubTaskOfType(CTaskTypes::TASK_GO_TO_POINT_AIMING));
	if(pGotoTask)
	{
		bool bIsTargetFacingAway = IsTargetFacingAway();
		float fDesiredMoveBlendRatio = m_pPedToArrest->GetMotionData()->GetIsSprinting() && bIsTargetFacingAway ? MOVEBLENDRATIO_SPRINT : MOVEBLENDRATIO_RUN;
		if( pGotoTask->GetMoveBlendRatio() != fDesiredMoveBlendRatio &&
			(!bIsTargetFacingAway || (fwTimer::GetTimeInMilliseconds() - m_uTimeMoveOnFootSpeedSet > CTaskCombat::ms_Tunables.m_MinTimeToChangeChaseOnFootSpeed)) )
		{
			m_uTimeMoveOnFootSpeedSet = fwTimer::GetTimeInMilliseconds();
			pGotoTask->SetMoveBlendRatio(fDesiredMoveBlendRatio);
		}
	}

	UpdateTalkTargetDown();

	return FSM_Continue;
}

void CTaskArrestPed::MoveToTargetInVehicle_OnEnter()
{
	m_fGetOutTimer = 0.0f;

	if(GetPed()->GetIsDrivingVehicle())
	{
		sVehicleMissionParams params;
		params.m_iDrivingFlags = DMode_AvoidCars_Reckless|DF_DriveIntoOncomingTraffic|DF_SteerAroundPeds|DF_UseStringPullingAtJunctions|
			DF_AdjustCruiseSpeedBasedOnRoadSpeed|DF_PreventJoinInRoadDirectionWhenMoving|DF_UseSwitchedOffNodes;
		params.m_fCruiseSpeed = 20.0f;
		params.m_fTargetArriveDist = sm_Tunables.m_MoveToDistanceInVehicle;
		params.SetTargetEntity(const_cast<CPed*>(m_pPedToArrest.Get()));

		aiTask *pCarTask = rage_new CTaskVehicleApproach(params);
		SetNewTask(rage_new CTaskControlVehicle(GetPed()->GetMyVehicle(), pCarTask));
	}
}

aiTask::FSM_Return CTaskArrestPed::MoveToTargetInVehicle_OnUpdate()
{
	CPed* pPed = GetPed();

	if(!pPed->GetMyVehicle())
	{
		SetState(State_Start);
		return FSM_Continue;
	}

	if(pPed->GetIsDrivingVehicle())
	{
		if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
		{
			if(!m_pPedToArrest->GetIsInVehicle() || m_fTimeTargetStill > 0.0f || IsTargetBeingBusted())
			{
				SetState(State_ExitVehicle);
			}
			else
			{
				SetState(m_bQuitOnCombat ? State_Finish : State_Combat);
			}

			return FSM_Continue;
		}
	}
	else
	{
		CPed* pDriver = pPed->GetMyVehicle()->GetDriver();
		if( pDriver &&
			pDriver->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT) &&
			!pDriver->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ARREST_PED) )
		{
			SetState(m_bQuitOnCombat ? State_Finish : State_Combat);
			return FSM_Continue;
		}
		else if(!pDriver || pDriver->IsExitingVehicle())
		{
			float fTimeToLeave = pPed->GetRandomNumberInRangeFromSeed(CTaskVehiclePersuit::GetTunables().m_MinPassengerTimeToLeaveVehicle, CTaskVehiclePersuit::GetTunables().m_MaxPassengerTimeToLeaveVehicle);
			m_fGetOutTimer += fwTimer::GetTimeStep();
			if(m_fGetOutTimer > fTimeToLeave)
			{
				SetState(State_ExitVehicle);
				return FSM_Continue;
			}
		}
		else
		{
			m_fGetOutTimer = 0.0f;
		}
	}

	UpdateTalkTargetDown();

	return FSM_Continue;
}

void CTaskArrestPed::AimAtTarget_OnEnter()
{
	m_fTimeUntilNextDialogue = 0.0f;
	SetNewTask(rage_new CTaskShootAtTarget(CAITarget(m_pPedToArrest), CTaskShootAtTarget::Flag_aimOnly));
}

aiTask::FSM_Return CTaskArrestPed::AimAtTarget_OnUpdate()
{
	if (m_bPedIsResistingArrest)
	{
		SetState(State_Combat);
		return FSM_Continue;
	}

	// We need to make sure we update who is arresting the target otherwise all peds can get stuck as the non arresting cop
	if (m_bForceArrest || !IsAnotherCopCloserToTarget())
	{
		SetPedIsArrestingTarget(true);
	}

	float fMaxDistanceFromTargetSq = m_bWillArrest ? square(sm_Tunables.m_ArrestingCopMaxDistanceFromTarget) : square(sm_Tunables.m_BackupCopMaxDistanceFromTarget);
	ScalarV scDistToTargetSq = DistSquared(m_pPedToArrest->GetTransform().GetPosition(), GetPed()->GetTransform().GetPosition());
	ScalarV scMaxDistanceSq = ScalarVFromF32(fMaxDistanceFromTargetSq);
	if(IsGreaterThanOrEqualAll(scDistToTargetSq, scMaxDistanceSq))
	{
		SetState(CheckForDesiredArrestState());
		return FSM_Continue;
	}

	// If we are to arrest the target
	if(m_bWillArrest)
	{
		// If our target is entering a vehicle then we should try to arrest them in the vehicle
		if(m_pPedToArrest->GetIsEnteringVehicle(true) || m_bWasTargetEnteringVehicleLastFrame)
		{
			if(m_pPedToArrest->GetVehiclePedInside())
			{
				s32 iNewState = CheckForDesiredArrestState();
				if(iNewState != State_AimAtTarget)
				{
					SetState(iNewState);
				}

				return FSM_Continue;
			}
		}
		else if( m_fTimeTargetStill >= sm_Tunables.m_MinTimeOnFootTargetStillForArrest &&
				 (!m_pPedToArrest->GetIsInVehicle() || m_pPedToArrest->GetMyVehicle()->GetVelocity().Mag2() < square(CTaskCombat::ms_Tunables.m_MaxSpeedToStartJackingVehicle)) )
		{
			CPedTargetting* pPedTargeting = GetPed()->GetPedIntelligence()->GetTargetting(true);
			if(pPedTargeting && pPedTargeting->GetLosStatus(m_pPedToArrest, false) == Los_clear)
			{
				SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
				SetState(State_WaitForRestart);
				return FSM_Continue;
			}
		}
		else if(m_pPedToArrest->GetIsInVehicle() && m_fTimeTargetStill <= 0.0f)
		{
			SetState(m_bQuitOnCombat ? State_Finish : State_Combat);
		}
	}

	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Finish);
	}

	UpdateTalkTargetDown();

	m_fTimeUntilNextDialogue -= GetTimeStep();
	if(m_fTimeUntilNextDialogue < 0.0f)
	{
		if(CanPlayArrestAudio())
		{
			GetPed()->NewSay("DRAW_GUN");
		}

		m_fTimeUntilNextDialogue = fwRandom::GetRandomNumberInRange(2.0f, 4.0f);
	}

	return FSM_Continue;
}

void CTaskArrestPed::WaitForRestart_OnEnter()
{
	if(!m_pPedToArrest->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_BUSTED))
	{
		aiTask* pTaskToGive = rage_new CTaskBusted(GetPed());
		CEventGivePedTask event(PED_TASK_PRIORITY_PRIMARY,pTaskToGive,false,E_PRIORITY_MAX, false, false, true); 
		m_pPedToArrest->GetPedIntelligence()->AddEvent(event);
	}
}

aiTask::FSM_Return  CTaskArrestPed::WaitForRestart_OnUpdate()
{
	// Had to add the break out check in this state as this is the state the cop is in when the player is being busted
	bool bDoesTargetHaveBustedTask = IsTargetBeingBusted();
	if( m_bPedIsResistingArrest ||
		(GetTimeInState() >= fwTimer::GetTimeStep() && !bDoesTargetHaveBustedTask) )
	{
		SetState(State_Combat);
		return FSM_Continue;
	}

	if (!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Finish);
	}

	return FSM_Continue;
}

void CTaskArrestPed::Combat_OnEnter()
{
	IncreaseWantedAndReportCrime();

	if(!m_bQuitOnCombat)
	{
		SetNewTask(rage_new CTaskCombat(m_pPedToArrest));
	}
}

aiTask::FSM_Return CTaskArrestPed::Combat_OnUpdate()
{
	if (m_bQuitOnCombat || !GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Finish);
	}

	return FSM_Continue;
}

void CTaskArrestPed::SwapWeapon_OnEnter()
{
	CPedWeaponManager* pPedWeaponManager = GetPed()->GetWeaponManager();
	const CWeaponInfo* pBestPistol = pPedWeaponManager->GetBestWeaponInfoByGroup(WEAPONGROUP_PISTOL);
	if (!pBestPistol)
	{
		GetPed()->GetInventory()->AddWeaponAndAmmo(WEAPONTYPE_PISTOL, 100);
		pPedWeaponManager->EquipWeapon(WEAPONTYPE_PISTOL, -1, true);
	}
	else
	{
		pPedWeaponManager->EquipWeapon(pBestPistol->GetHash(), -1, true);
	}


	SetNewTask(rage_new CTaskSwapWeapon(SWAP_HOLSTER | SWAP_DRAW));
}

aiTask::FSM_Return  CTaskArrestPed::SwapWeapon_OnUpdate()
{
	if(GetIsSubtaskFinished(CTaskTypes::TASK_SWAP_WEAPON))
	{
		SetState(CheckForDesiredArrestState());
	}

	return FSM_Continue;
}

s32 CTaskArrestPed::CheckForDesiredArrestState()
{
	if( (!m_bWillArrest && !m_bForceArrest) ||
		!m_pPedToArrest->GetIsInVehicle() )
	{
		return State_MoveToTarget;
	}

	CPedWeaponManager* pWeaponManager = GetPed()->GetWeaponManager();
	aiAssertf(pWeaponManager, "Peds who are trying to arrest should have weapon managers!");

	const CWeaponInfo* pWeaponInfo = pWeaponManager->GetEquippedWeaponInfo();
	if(!pWeaponInfo || pWeaponInfo->GetGroup() != WEAPONGROUP_PISTOL)
	{
		return State_SwapWeapon;
	}

	if(m_pPedToArrest->GetIsInVehicle())
	{
		if(m_bCanArrestTargetInVehicle)
		{
			return State_ArrestInVehicleTarget;
		}
		else
		{
			bool bIsOrWasTargetEnteringVehicle = (m_pPedToArrest->GetIsEnteringVehicle(true) || m_bWasTargetEnteringVehicleLastFrame);
			return bIsOrWasTargetEnteringVehicle ? State_AimAtTarget : State_MoveToTarget;
		}
	}
	else
	{
		return State_MoveToTarget;
	}
}

// PURPOSE: Check if our target is a local player, make sure they have 2 stars and report resist arrest crime
void CTaskArrestPed::IncreaseWantedAndReportCrime()
{
	// Make sure if we are going into combat we register than the target committed a hostile action
	CWanted::RegisterHostileAction(m_pPedToArrest);

	if(m_pPedToArrest->IsLocalPlayer() && m_pPedToArrest->GetPlayerWanted())
	{
		m_pPedToArrest->GetPlayerWanted()->SetWantedLevelNoDrop(VEC3V_TO_VECTOR3(m_pPedToArrest->GetTransform().GetPosition()), WANTED_LEVEL2, 0, WL_FROM_TASK_ARREST_PED);
		NA_POLICESCANNER_ENABLED_ONLY(g_PoliceScanner.ReportPlayerCrime(VEC3V_TO_VECTOR3(m_pPedToArrest->GetTransform().GetPosition()), CRIME_RESIST_ARREST, 0));
	}
}

bool CTaskArrestPed::IsHoldingFireDueToLackOfHostility() const
{
	//Grab the ped.
	const CPed* pPed = GetPed();

	//Ensure the task is valid.
	const CTaskCombat* pTask = static_cast<CTaskCombat *>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COMBAT));
	if(!pTask)
	{
		return false;
	}

	//Ensure we are holding fire due to lack of hostility.
	if(!pTask->IsHoldingFireDueToLackOfHostility())
	{
		return false;
	}

	return true;
}

bool CTaskArrestPed::IsTargetConsideredDangerous() const
{
	//Ensure we are not holding fire due to lack of hostility.
	if(IsHoldingFireDueToLackOfHostility())
	{
		return false;
	}

	//Check if the target is in water.
	if(m_pPedToArrest->GetIsInWater())
	{
		return true;
	}

	//Check if the target is not a player, but is a mission ped.
	if( !m_pPedToArrest->IsPlayer() && 
		(m_pPedToArrest->PopTypeIsMission() || m_pPedToArrest->GetWeaponManager()->GetIsArmed()) )
	{
		return true;
	}

	//Check if the ped is on foot.
	if(m_pPedToArrest->GetIsOnFoot())
	{
		//Check if the ped is wanted (more than level 1).
		const CWanted* pWanted = m_pPedToArrest->GetPlayerWanted();
		if(pWanted && (pWanted->GetWantedLevel() > WANTED_LEVEL1))
		{
			return true;
		}
	}

	return false;
}

bool CTaskArrestPed::IsTargetStill()
{
	if(m_pPedToArrest->GetIsInVehicle())
	{
		// Check if our target is in a moving vehicle
		return m_pPedToArrest->GetMyVehicle()->GetVelocity().Mag2() < square(CTaskCombat::ms_Tunables.m_MaxSpeedToContinueJackingVehicle);
	}
	else
	{
		CTaskMelee* pTargetMeleeTask = static_cast<CTaskMelee*>(m_pPedToArrest->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_MELEE));
		if(pTargetMeleeTask && pTargetMeleeTask->IsCurrentlyDoingAMove(true, true))
		{
			return false;
		}

		if(m_pPedToArrest->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_NM_CONTROL, true))
		{
			return false;
		}

		if(m_pPedToArrest->IsAPlayerPed())
		{
			const CControl* pControl = m_pPedToArrest->GetControlFromPlayer();
			if(pControl)
			{
				Vector2 vecStick;
				vecStick.x = pControl->GetPedWalkLeftRight().GetNorm();
				vecStick.y = pControl->GetPedWalkUpDown().GetNorm();

				if(vecStick.IsNonZero())
				{
					return false;
				}
			}
		}

		return m_pPedToArrest->GetMotionData()->GetIsStill();
	}
}

bool CTaskArrestPed::IsTargetBeingBusted() const
{
	if(!m_pPedToArrest)
	{
		return false;
	}

	if(m_pPedToArrest->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_BUSTED, true))
	{
		return true;
	}

	const CTaskExitVehicleSeat* pExitVehicleTask = static_cast<const CTaskExitVehicleSeat* >(m_pPedToArrest->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE_SEAT));
	if(pExitVehicleTask && pExitVehicleTask->IsBeingArrested())
	{
		return true;
	}

	return false;
}

void CTaskArrestPed::SetPedIsArrestingTarget(bool bIsArrestingTarget)
{
	// Go ahead and return if we aren't changing anything
	if(m_bWillArrest == bIsArrestingTarget)
	{
		return;
	}

	// Now set the class variable to the new value
	m_bWillArrest = bIsArrestingTarget;

	// Need to make sure we have a ped before grabbing the player info
	if(!m_pPedToArrest)
	{
		return;
	}

	// Grab the player info of this ped (will return NULL for non players)
	CPlayerInfo* pPlayerInfo = m_pPedToArrest->GetPlayerInfo();
	if(pPlayerInfo)
	{
		// If we were told to arrest the target we need to increase the counter on the player
		if(bIsArrestingTarget)
		{
			pPlayerInfo->IncrementAiAttemptingArrestOnPlayer();
		}
		// Otherwise it means we need to decrease the counter
		else
		{
			pPlayerInfo->DecrementAiAttemptingArrestOnPlayer();
		}
	}

}

bool CTaskArrestPed::CanPlayArrestAudio()
{
	CPedTargetting* pPedTargetting = GetPed()->GetPedIntelligence()->GetTargetting( false );
	if(pPedTargetting)
	{
		LosStatus losStatus = pPedTargetting->GetLosStatus(m_pPedToArrest, false);
		return losStatus == Los_clear || losStatus == Los_blockedByFriendly;
	}
	
	return true;
}

bool CTaskArrestPed::IsTargetFacingAway() const
{
	Vec3V vToTarget = m_pPedToArrest->GetTransform().GetPosition() - GetPed()->GetTransform().GetPosition();
	ScalarV scTargetHeadingDot = Dot(m_pPedToArrest->GetTransform().GetForward(), vToTarget);
	
	return IsGreaterThanAll(scTargetHeadingDot, ScalarV(V_ZERO)) != 0;
}

bool CTaskArrestPed::IsAnotherCopCloserToTarget()
{
	// At this point we want to reset our arresting variable
	SetPedIsArrestingTarget(false);

	CPed* pPed = GetPed();

	CEntityScannerIterator entityList = m_pPedToArrest->GetPedIntelligence()->GetNearbyPeds();
	for (CEntity* pEnt = entityList.GetFirst(); pEnt; pEnt = entityList.GetNext())
	{
		CPed* pNearbyPed = static_cast<CPed*>(pEnt);
		if (pNearbyPed->IsRegularCop() && pNearbyPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ARREST_PED, true))
		{
			return pNearbyPed != pPed;
		}
	}

	return false;
}

void CTaskArrestPed::UpdateTalkTargetDown()
{
	bool bNetworkGameInProgress = false;
#ifdef ONLINE_PLAY
	bNetworkGameInProgress = CNetwork::IsGameInProgress();
#endif

	bool bPedToArrestIsPlayer = m_pPedToArrest->IsAPlayerPed();

	// If this is a player ped and we've disabled talking them down or they have been arrested just do nothing
	if(bPedToArrestIsPlayer)
	{
		if( CTaskPlayerOnFoot::ms_bDisableTalkingThePlayerDown || m_pPedToArrest->GetIsArrested() )
		{
			return;
		}

		CTaskExitVehicleSeat* pExitVehicleSeatTask = static_cast<CTaskExitVehicleSeat*>(m_pPedToArrest->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE_SEAT));
		if(pExitVehicleSeatTask && pExitVehicleSeatTask->IsBeingArrested())
		{
			return;
		}
	}

	CPed* pPed = GetPed();
	const bool bCopTalkingTargetDown = pPed->GetPedType() == PEDTYPE_COP && pPed->PopTypeIsRandom();
	if( bNetworkGameInProgress || !bCopTalkingTargetDown )
	{
		SetTryToTalkTargetDown(false);
	}
	else
	{
		CWanted* pWanted = (bPedToArrestIsPlayer && m_pPedToArrest->GetPlayerWanted()) ? m_pPedToArrest->GetPlayerWanted() : NULL;
		const bool bTargetConsideredDangerous = IsTargetConsideredDangerous();
		const bool bTargetWantedForMinorCrime = ((pWanted && pWanted->GetWantedLevel() == WANTED_LEVEL1) || (!bPedToArrestIsPlayer && m_pPedToArrest->PopTypeIsRandom() && !m_pPedToArrest->GetWeaponManager()->GetIsArmed()) );

		if( GetTryToTalkTargetDown() )
		{
			if( GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_INVESTIGATE )
				m_fTalkingTargetDownTimer = 0.0f;
			else
				m_fTalkingTargetDownTimer += fwTimer::GetTimeStep();

			const float fTimeToShootAtTargetIfNotMoving = bTargetWantedForMinorCrime ? 30.0f : 5.0f;
			const bool bCopsCantGetToPlayer = m_fTalkingTargetDownTimer > 30.0f && bPedToArrestIsPlayer;

			m_vTargetPositionWhenBeingTalkedDown = VEC3V_TO_VECTOR3(m_pPedToArrest->GetTransform().GetPosition());

			// If the target is considered dangerous, open fire within a few seconds
			if( bTargetConsideredDangerous )
			{
				SetState(State_Combat);
				SetTryToTalkTargetDown(false);
			}
			// If the target is fighting, keep updating the position, so the cop won't try and bust the player
			else if( m_pPedToArrest->IsLocalPlayer() && 
				m_pPedToArrest->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_MELEE) )
			{
				m_fTalkingTargetDownTimer = 0.0f;
			}
			// IF the target tries running, open fire
			else if( m_fTalkingTargetDownTimer > fTimeToShootAtTargetIfNotMoving )
			{
				if( bCopsCantGetToPlayer )
				{
					SetState(State_Combat);
					SetTryToTalkTargetDown(false);
				}
			}
		}
		else
		{
			m_vTargetPositionWhenBeingTalkedDown = VEC3V_TO_VECTOR3(m_pPedToArrest->GetTransform().GetPosition());
			m_fTalkingTargetDownTimer = 0.0f;

			// Never try to talk down a non-player
			if( !bPedToArrestIsPlayer && !m_pPedToArrest->GetWeaponManager()->GetIsArmed() )
			{
			}
			// If the target hasn't done anything threatening within a few seconds try to talk them down again
			else if( !bTargetConsideredDangerous )
			{
				SetTryToTalkTargetDown(true);
			}
		}
	}
}

bool CTaskArrestPed::StreamClipSet()
{
	if(m_ExitToAimClipSetId.IsNotNull())
	{
		m_clipSetRequestHelper.Request(m_ExitToAimClipSetId);
	}

	return false;
}

bool CTaskArrestPed::CanArrestPedInVehicle(const CPed* pPed, const CPed* pTarget, bool bSetLastFailedBustTime, bool bCheckForArrestInVehicleAnims, bool bDoCapsuleTest)
{
	// Verify the ped and the target
	if(!pPed || !pTarget)
	{
		return false;
	}
	
	//Verify that vehicle doesn't have busting disabled
	CVehicle* pVehicle = pTarget->GetMyVehicle();
	if(pVehicle && pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_DISABLE_BUSTING))
	{
		return false;
	}

	// We don't want to attempt to arrest a player in a vehicle if we've recently failed
	CPlayerInfo* pPlayerInfo = pTarget->GetPlayerInfo();
	if(pPlayerInfo && pPlayerInfo->GetLastTimeBustInVehicleFailed() + sm_Tunables.m_TimeBetweenPlayerInVehicleBustAttempts > fwTimer::GetTimeInMilliseconds())
	{
		return false;
	}

	// Verify the seat index first
	s32 iPedsSeatIndex = pVehicle ? pVehicle->GetSeatManager()->GetPedsSeatIndex(pTarget) : -1;
	if(iPedsSeatIndex >= 0)
	{
		// B*1917308
		if (pVehicle->IsTurretSeat(iPedsSeatIndex))
		{
			return false;
		}

		Vector3 vEntryPointOffset(-sm_Tunables.m_TargetDistanceFromVehicleEntry * .5f, 0.0f, 0.0f);

		// Then verify the desired entry point and make sure it's clear for our ped 
		s32 iTargetEntryPoint = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(iPedsSeatIndex, pVehicle, SA_directAccessSeat, true, true);
		if(pVehicle->IsEntryPointIndexValid(iTargetEntryPoint) && CVehicle::IsEntryPointClearForPed(*pVehicle, *pPed, iTargetEntryPoint, bDoCapsuleTest ? &vEntryPointOffset : NULL, bDoCapsuleTest))
		{
			if(!bCheckForArrestInVehicleAnims)
			{
				return true;
			}
			else
			{
				// This looks really strange but our arrest conditional anims always require that this config flag be set while
				// searching, so we have to make sure it's set beforehand and then we'll reset it after we find a conditional clip set
				// EDIT: should reset the flag to what it was
				CPed* pCopPed = const_cast<CPed*>(pPed);
				bool bCacheWillArrestRatherThanJack = pCopPed->GetPedConfigFlag(CPED_CONFIG_FLAG_WillArrestRatherThanJack);
				pCopPed->SetPedConfigFlag(CPED_CONFIG_FLAG_WillArrestRatherThanJack, true);
				ms_VehicleClipRequestHelper.SetPed(pCopPed);
				const CConditionalClipSet* pEntryClipSet = ms_VehicleClipRequestHelper.ChooseVariationClipSetFromEntryIndex(pVehicle, iTargetEntryPoint);
				pCopPed->SetPedConfigFlag(CPED_CONFIG_FLAG_WillArrestRatherThanJack, bCacheWillArrestRatherThanJack);

				if(pEntryClipSet && pEntryClipSet->GetIsArrest())
				{
					fwClipSet* pClipSet = fwClipSetManager::GetClipSet(pEntryClipSet->GetClipSetId());
					if(pClipSet && pClipSet->GetClipItemCount() >= 0)
					{
						return true;
					}
				}

				return false;
			}
		}
		else if(pPlayerInfo && bSetLastFailedBustTime)
		{
			// If this was a player we need to register the bust in vehicle failure so we can avoid doing too many entry point clear checks
			pPlayerInfo->SetLastTimeBustInVehicleFailed();
		}
	}

	return false;
}

#if !__FINAL
const char * CTaskArrestPed::GetStaticStateName( s32 iState )
{
	taskAssert(iState >= State_Start && iState <= State_Finish);

	switch (iState)
	{
		case State_Start:						return "State_Start";
		case State_ExitVehicle:					return "State_ExitVehicle";
		case State_MoveToTarget:				return "State_MoveToTarget";
		case State_ArrestInVehicleTarget:		return "State_ArrestInVehicleTarget";
		case State_MoveToTargetInVehicle:		return "State_MoveToTargetInVehicle";
		case State_AimAtTarget:					return "State_AimAtTarget";
		case State_WaitForRestart:				return "State_WaitForRestart";
		case State_Combat:						return "State_Combat";
		case State_SwapWeapon:					return "State_SwapWeapon";
		case State_Finish:						return "State_Finish";
		default: taskAssert(0); 	
	}

	return "State_Invalid";
}

void CTaskArrestPed::Debug() const
{
}
#endif // !__FINAL

/////////////////////////////////////////////////////////////////////

dev_float TIME_TO_ALLOW_PLAYER_TO_ESCAPE_MIN = 1.0f;
dev_float TIME_TO_ALLOW_PLAYER_TO_ESCAPE_MAX = 3.0f;

const fwMvBooleanId	CTaskBusted::ms_DropWeaponId("DropWeapon",0x9312D206);
const fwMvBooleanId	CTaskBusted::ms_AllowBreakoutId("AllowBreakout",0xC55E29CF);
const fwMvBooleanId	CTaskBusted::ms_PreventBreakoutId("PreventBreakout",0xA7D81D37);

CTaskBusted::CTaskBusted(const CPed* pArrestingPed)
: m_pArrestingPed(pArrestingPed)
, m_ClipSetId("busted",0x91F9A755)
, m_VehClipSetId("busted_vehicle_std",0xA0839828)
, m_fAllowBreakoutTime(TIME_TO_ALLOW_PLAYER_TO_ESCAPE_MIN)
, m_fPreventBreakoutTime(TIME_TO_ALLOW_PLAYER_TO_ESCAPE_MAX)
, m_bDroppedWeapon(false)
, m_bToldEveryoneToBackOff(false)
{
	SetInternalTaskType(CTaskTypes::TASK_BUSTED);
}

CTaskBusted::~CTaskBusted()
{

}

void CTaskBusted::CleanUp()
{
	CPed* pPed = GetPed();
	if(pPed)
	{
		// If we actually set the everybody back off to true then we should turn it off
		if(m_bToldEveryoneToBackOff)
		{
			CWanted* pWanted = pPed->GetPlayerWanted();
			if(pWanted)
			{
				pWanted->m_EverybodyBackOff = false;
			}
		}

		CVehicle* pVehicle = pPed->GetVehiclePedInside();
		if(pVehicle)
		{
			pVehicle->GetIntelligence()->GetTaskManager()->ClearTask(VEHICLE_TASK_TREE_SECONDARY, VEHICLE_TASK_SECONDARY_ANIM);
		}
	}
}

bool CTaskBusted::ShouldAbort(const AbortPriority UNUSED_PARAM(iPriority), const aiEvent* pEvent)
{
	if (pEvent)
	{
		// Check if our event is trying to give us a task
		eEventType eventType = static_cast<eEventType>(static_cast<const CEvent*>(pEvent)->GetEventType());
		if (eventType == EVENT_GIVE_PED_TASK)
		{
			// check if that task is CTaskBusted, which is the task we are currently running and don't want to be replaced with
			aiTask* pTask = ((CEventGivePedTask*)pEvent)->GetTask();
			if (pTask && pTask->GetTaskType() == CTaskTypes::TASK_BUSTED)
			{
				return false;
			}
		}
	}
	return true;
}

void CTaskBusted::DoAbort( const AbortPriority UNUSED_PARAM(iPriority), const aiEvent* UNUSED_PARAM(pEvent))
{
	StopClip();
}

aiTask::FSM_Return CTaskBusted::ProcessPreFSM()
{
	CPed* pPed = GetPed();
	if (pPed->IsLocalPlayer() && pPed->GetPlayerWanted()->GetWantedLevel() == WANTED_CLEAN)
	{
		return FSM_Quit;
	}

	// Reset flag - use kinematic physics so the ped does not get pushed around too easily.
	pPed->SetPedResetFlag(CPED_RESET_FLAG_TaskUseKinematicPhysics, true);

	return FSM_Continue;
}

aiTask::FSM_Return CTaskBusted::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnEnter
				Start_OnEnter();
			FSM_OnUpdate
				return Start_OnUpdate();	

		FSM_State(State_PlayClip)
			FSM_OnEnter
				PlayClip_OnEnter();
			FSM_OnUpdate
				return PlayClip_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return Finish_OnUpdate();

	FSM_End
}

void CTaskBusted::Start_OnEnter()
{
	CPed* pPed = GetPed();

	CVehicle* pVehicle = pPed->GetVehiclePedInside();
	if(pVehicle)
	{
		const CVehicleLayoutInfo* pVehLayout = pVehicle->GetVehicleModelInfo()->GetVehicleLayoutInfo();
		if(pVehLayout)
		{
			m_VehClipSetId = fwMvClipSetId(pVehLayout->GetHandsUpClipSetId());
		}

		pVehicle->GetIntelligence()->AddTask(VEHICLE_TASK_TREE_SECONDARY, rage_new CTaskBringVehicleToHalt(0.25f, -1), VEHICLE_TASK_SECONDARY_ANIM, true);
	}

	CWanted* pWanted = pPed->GetPlayerWanted();
	if(pWanted && !pWanted->m_EverybodyBackOff)
	{
		pWanted->m_EverybodyBackOff = true;
		m_bToldEveryoneToBackOff = true;
	}
}

aiTask::FSM_Return CTaskBusted::Start_OnUpdate()
{
	fwMvClipSetId clipSetToLoad = GetPed()->GetIsInVehicle() ? m_VehClipSetId : m_ClipSetId;
	if(clipSetToLoad != CLIP_SET_ID_INVALID)
	{
		fwClipSet* pClipSet = fwClipSetManager::GetClipSet(clipSetToLoad);
		if(taskVerifyf(pClipSet, "Clip set [%s] doesn't exist", clipSetToLoad.GetCStr()))
		{
			if(pClipSet->Request_DEPRECATED())
			{
				SetState(State_PlayClip);
			}
			return FSM_Continue;
		}
	}
	return FSM_Quit;
}

void CTaskBusted::PlayClip_OnEnter()
{
	CPed* pPed = GetPed();
	CVehicle* pVehiclePedInside = pPed->GetVehiclePedInside();
	if(pVehiclePedInside)
	{
		// Don't play clips if we are on a bike with a passenger as we will clip through them
		if(pVehiclePedInside->InheritsFromBike() && pVehiclePedInside->GetSeatManager()->CountPedsInSeats(false) > 0)
		{
			return;
		}

		// Don't play clips for BMX vehicles as we don't have proper anims
		if(pVehiclePedInside->InheritsFromBicycle())
		{
			return;
		}

		// Don't play clips in turret seats just in case (shouldn't be able to arrest in those seats in first place)
		s32 iPedsSeatIndex = pVehiclePedInside->GetSeatManager()->GetPedsSeatIndex(pPed);
		if(iPedsSeatIndex >= 0)
		{
			if (pVehiclePedInside->IsTurretSeat(iPedsSeatIndex))
			{
				return;
			}
		}

		static const fwMvClipId iStayInVehClipNameHash("STAY_IN_CAR_CRIM",0x5D9183CB);
		if (taskVerifyf(fwAnimManager::GetClipIfExistsBySetId(m_VehClipSetId, iStayInVehClipNameHash), "Clip not found"))
		{
			StartClipBySetAndClip(pPed, m_VehClipSetId, iStayInVehClipNameHash, APF_ISFINISHAUTOREMOVE, (u32)BONEMASK_UPPERONLY, AP_MEDIUM, SLOW_BLEND_IN_DELTA, CClipHelper::TerminationType_OnDelete);

			// Get our clip from our clip helper
			const crClip* pClip = GetClipHelper() ? GetClipHelper()->GetClip() : NULL;
			if(pClip && pClip->GetTags())
			{
				// Check for the "AllowBreakout" move event
				const CClipEventTags::CEventTag<crTag>* pAllowBreakoutTag = CClipEventTags::FindFirstEventTag<crTag, crPropertyAttributeHashString, atHashString>(pClip, CClipEventTags::MoveEvent, CClipEventTags::MoveEvent, ms_AllowBreakoutId.GetHash());
				if(pAllowBreakoutTag)
				{
					m_fAllowBreakoutTime = pClip->ConvertPhaseToTime(pAllowBreakoutTag->GetStart());
				}

				// Check for the "PreventBreakout" move event
				const CClipEventTags::CEventTag<crTag>* pPreventBreakoutTag = CClipEventTags::FindFirstEventTag<crTag, crPropertyAttributeHashString, atHashString>(pClip, CClipEventTags::MoveEvent, CClipEventTags::MoveEvent, ms_PreventBreakoutId.GetHash());
				if(pPreventBreakoutTag)
				{
					m_fPreventBreakoutTime = pClip->ConvertPhaseToTime(pPreventBreakoutTag->GetStart());
				}
			}
		}
	}
	else
	{
		fwMvClipId iClipNameHash;

		Assert( pPed->GetWeaponManager() );
		const CWeaponInfo* pWeaponInfo = pPed->GetWeaponManager()->GetCurrentWeaponInfoForHeldObject();
		if(pWeaponInfo && pWeaponInfo->GetIsTwoHanded())
		{
			iClipNameHash = fwMvClipId("idle_2_hands_up_2h",0xDF5D5567);
		}
		else
		{
			iClipNameHash = fwMvClipId("idle_2_hands_up",0xE314061);
		}

		if (taskVerifyf(fwAnimManager::GetClipIfExistsBySetId(m_ClipSetId, iClipNameHash), "Clip not found"))
		{
			StartClipBySetAndClip(pPed, m_ClipSetId, iClipNameHash, APF_ISFINISHAUTOREMOVE, BONEMASK_ALL, AP_MEDIUM, REALLY_SLOW_BLEND_IN_DELTA, CClipHelper::TerminationType_OnDelete);
		}
	}
}

aiTask::FSM_Return CTaskBusted::PlayClip_OnUpdate()
{
	CPed* pPed = GetPed();
	
	if( !pPed->GetIsArrested() )
	{
#if FPS_MODE_SUPPORTED
		// In FPS mode: Quickly turn around to face the arresting ped
		if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && !pPed->GetIsArrested() && m_pArrestingPed.Get())
		{
			Vector3 vTargetPos = VEC3V_TO_VECTOR3(m_pArrestingPed->GetTransform().GetPosition());
			pPed->SetDesiredHeading(vTargetPos);

			float turnRequired = SubtractAngleShorter(pPed->GetDesiredHeading(), pPed->GetCurrentHeading());
			static dev_float fTurnRate = 3.0f;
			float turnAmount = (turnRequired * fTurnRate) * fwTimer::GetTimeStep();
			if (abs(turnAmount) > abs(turnRequired))
				turnAmount = turnRequired;

			pPed->SetHeading(fwAngle::LimitRadianAngle(turnAmount + pPed->GetCurrentHeading()));
		}
#endif	//FPS_MODE_SUPPORTED

		// If the player is not inside a vehicle and is within the time limit
		if(!m_bDroppedWeapon && GetTimeInState() < m_fPreventBreakoutTime)
		{
			// Make sure our clip has played for a a little bit of time before allowing the interrupt
			if(GetTimeInState() > m_fAllowBreakoutTime )
			{
				Assert(pPed->IsLocalPlayer());

				CControl* pControl = pPed->GetControlFromPlayer();
				bool bBreakout = m_pArrestingPed && m_pArrestingPed->IsInjured();
				if(!bBreakout && pControl)
				{
					// Get control and check for button presses
					bool bVehicleBreakoutInput = pPed->GetIsInVehicle() && (pControl->GetVehicleAccelerate().IsDown() || pControl->GetVehicleBrake().IsDown());
					bool bFPSBreakoutIfAimOrShoot = false;
#if FPS_MODE_SUPPORTED
					bFPSBreakoutIfAimOrShoot = pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && (pControl->GetPedTargetIsDown() || pControl->GetPedAttack().IsDown());
#endif	// FPS_MODE_SUPPORTED
					bBreakout = (pControl->GetPedSprintIsPressed() || bVehicleBreakoutInput || bFPSBreakoutIfAimOrShoot);
				}

				if(bBreakout)
				{
					// Wanted level goes up to 2 when escaping
					if( pPed->GetPlayerWanted() )
					{
						pPed->GetPlayerWanted()->SetWantedLevelNoDrop(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), WANTED_LEVEL2, 0, WL_FROM_TASK_BUSTED);
						NA_POLICESCANNER_ENABLED_ONLY(g_PoliceScanner.ReportPlayerCrime(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), CRIME_RESIST_ARREST, 0));
					}

					SetState(State_Finish);
					return FSM_Continue;
				}
			}
		}
		else
		{
			// Let other systems know the player has been arrested
			pPed->SetArrestState(ArrestState_Arrested);
		}
	}

	// Grab our current equipped weapon
	const CWeaponInfo* pWeaponInfo = pPed->GetWeaponManager()->GetEquippedWeaponInfo();
	if(!m_bDroppedWeapon && pWeaponInfo && !pPed->GetIsInVehicle())
	{
		// Get our clip from our clip helper
		CClipHelper* pClipHelper = GetClipHelper();
		const crClip* pClip = pClipHelper ? pClipHelper->GetClip() : NULL;
		if(pClip && pClip->GetTags())
		{
			// Check for the "DropWeapon" move event
			const CClipEventTags::CEventTag<crTag>* pTag = CClipEventTags::FindFirstEventTag<crTag, crPropertyAttributeHashString, atHashString>(pClip, CClipEventTags::MoveEvent, CClipEventTags::MoveEvent, ms_DropWeaponId.GetHash());
			if(pTag && pTag->GetStart() <= pClipHelper->GetPhase() && pTag->GetStart() >= pClipHelper->GetLastPhase())
			{
				// Try to drop our current weapon
				CObject* pDroppedObject = pPed->GetWeaponManager()->DropWeapon(pWeaponInfo->GetHash(), true, false);
				if(pDroppedObject && pWeaponInfo->GetEffectGroup() == WEAPON_EFFECT_GROUP_MOLOTOV)
				{
					CProjectile* pMolotov = pDroppedObject->GetAsProjectile();
					if(pMolotov)
					{
						pMolotov->SetDisableImpactExplosion(true);
					}
				}

				m_bDroppedWeapon = true;
			}
		}
	}

	return FSM_Continue;
}

aiTask::FSM_Return CTaskBusted::Finish_OnUpdate()
{
	if(!m_bDroppedWeapon)
	{
		// If for some reason our equipped weapon was removed then we should recreate it
		CPedWeaponManager* pWeaponManager = GetPed()->GetWeaponManager();
		if(pWeaponManager && !pWeaponManager->GetEquippedWeaponObject())
		{
			pWeaponManager->CreateEquippedWeaponObject();
		}
	}

	return FSM_Quit;
}

#if !__FINAL
const char * CTaskBusted::GetStaticStateName( s32 iState )
{
	taskAssert(iState >= State_Start && iState <= State_Finish);

	switch (iState)
	{
		case State_Start:				return "State_Start";
		case State_PlayClip:			return "State_PlayAnim";
		case State_Finish:				return "State_Finish";
		default: taskAssert(0); 	
	}

	return "State_Invalid";
}

void CTaskBusted::Debug() const
{
}
#endif // !__FINAL

/////////////////////////////////////////////////////////////////////

CTaskPoliceWantedResponse::CTaskPoliceWantedResponse(CPed* pWantedPed)
: m_pWantedPed(pWantedPed)
{
	SetInternalTaskType(CTaskTypes::TASK_POLICE_WANTED_RESPONSE);
}

CTaskPoliceWantedResponse::~CTaskPoliceWantedResponse()
{

}

void CTaskPoliceWantedResponse::CleanUp()
{

}

aiTask::FSM_Return CTaskPoliceWantedResponse::ProcessPreFSM()
{
	const CPed* pPed = GetPed();
	const COrder* pOrder = pPed->GetPedIntelligence()->GetOrder();
	if (!pOrder || !pOrder->GetIsValid())
	{
		return FSM_Quit;
	}

	if(!m_pWantedPed)
	{
		return FSM_Quit;
	}

	return FSM_Continue;
}

aiTask::FSM_Return CTaskPoliceWantedResponse::UpdateFSM(const s32 iState, const FSM_Event iEvent)
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

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

aiTask::FSM_Return CTaskPoliceWantedResponse::Start_OnUpdate()
{
	SetState(State_Combat);
	return FSM_Continue;
}

CTask::FSM_Return CTaskPoliceWantedResponse::Resume_OnUpdate()
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

void CTaskPoliceWantedResponse::Combat_OnEnter()
{
	//Check if we are resuming the state.  Keep the sub-task, if possible.
	if(GetSubTask() && (GetSubTask()->GetTaskType() == CTaskTypes::TASK_THREAT_RESPONSE))
	{
		return;
	}

	if (taskVerifyf(m_pWantedPed, "m_pWantedPed must be valid"))
	{
		//Create the task.
		CTaskThreatResponse* pTask = rage_new CTaskThreatResponse(m_pWantedPed);
		pTask->SetThreatResponseOverride(CTaskThreatResponse::TR_Fight);

		// Check if we should be arresting
		if(m_pWantedPed->IsAPlayerPed() && !NetworkInterface::IsGameInProgress())
		{
			CWanted* pWanted = m_pWantedPed->GetPlayerWanted();
			if(pWanted && pWanted->GetWantedLevel() == WANTED_LEVEL1)
			{
				CVehicle* pTargetVehicle = m_pWantedPed->GetVehiclePedInside();
				if(!pTargetVehicle || (!pTargetVehicle->GetLayoutInfo()->GetDisableJackingAndBusting() && !pTargetVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_DISABLE_BUSTING)))
				{
					pTask->GetConfigFlagsForCombat().SetFlag(CTaskCombat::ComF_ArrestTarget);
				}
			}
		}

		SetNewTask(pTask);
	}
}

aiTask::FSM_Return CTaskPoliceWantedResponse::Combat_OnUpdate()
{
	if (!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Finish);
	}

	return FSM_Continue;
}

int CTaskPoliceWantedResponse::ChooseStateToResumeTo(bool& bKeepSubTask) const
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

#if !__FINAL
const char * CTaskPoliceWantedResponse::GetStaticStateName( s32 iState )
{
	taskAssert(iState >= State_Start && iState <= State_Finish);

	switch (iState)
	{
		case State_Start:				return "State_Start";
		case State_Resume:				return "State_Resume";
		case State_Combat:				return "State_Combat";
		case State_Finish:				return "State_Finish";
		default: taskAssert(0); 	
	}

	return "State_Invalid";
}

void CTaskPoliceWantedResponse::Debug() const
{
}

#endif // !__FINAL

//////////////////////////////////////////////////////////////////////////

CCriminalScanner::CCriminalScanner() :
m_CriminalEnt(NULL)
{}
	
CCriminalScanner::~CCriminalScanner()
{}

void CCriminalScanner::Scan(CPed* pPed, bool bIgnoreTimers)
{
	//Clear the last scans result
	m_CriminalEnt = NULL;

	//Is the ped patrolling in a vehicle 
	bool bCopInVehicle = pPed->GetIsInVehicle();

	bool bCanPursuePedInVehicle = false;

	if (bCopInVehicle && CRandomEventManager::GetInstance().CanCreateEventOfType(RC_COP_PURSUE,bIgnoreTimers,bIgnoreTimers))
	{	
		bCanPursuePedInVehicle = true;
	}

	if (!bCanPursuePedInVehicle )
	{
		return;
	}

	//Check this cop has the necessary cop and no peds in the car.
	CVehicle* pVeh = pPed->GetVehiclePedInside();
	if( pVeh->GetSeatManager()->CountPedsInSeats(true) > 2 )
	{
		return;
	}

	//Scan for vehicles
	CEntityScannerIterator entityList = pVeh->GetIntelligence()->GetVehicleScanner().GetIterator();
	for( CEntity* pEnt = entityList.GetFirst(); pEnt; pEnt = entityList.GetNext() )
	{
		CVehicle* pOtherVehicle = static_cast<CVehicle*>(pEnt);
		CPed* pOtherPed = pOtherVehicle->GetDriver();

		//Does it have a driver
		if (!pOtherPed)
		{
			continue;
		}

		//Check if the vehicle is law enforcement car or not driveable
		if(pOtherVehicle->IsLawEnforcementCar() || !pOtherVehicle->IsInDriveableState())
		{
			continue;
		}

		//Can't be the players vehicle
		if(pOtherVehicle->ContainsPlayer() && !CTaskPursueCriminal::ms_Tunables.m_AllowPursuePlayer)
		{
			continue;
		}

		//On mission
		if (pOtherVehicle->PopTypeIsMission())
		{
			continue;
		}

		//Can't be parked
		if(pOtherVehicle->m_nVehicleFlags.bIsThisAParkedCar)
		{
			continue;
		}

		//Make sure it's a car/bike/quad
		if(!pOtherVehicle->InheritsFromAutomobile() && !pOtherVehicle->InheritsFromBike() && !pOtherVehicle->InheritsFromQuadBike())
		{
			continue;
		}

		if(!CRandomEventManager::GetTunables().m_ForceCrime)
		{
			//Don't pursue females
			if (!pOtherPed->IsMale())
			{
				continue;
			}

			//Check the passengers aren't female
			for(u32 seatIndex = 0; seatIndex < pOtherVehicle->GetSeatManager()->GetMaxSeats(); seatIndex++)
			{
				CPed* pedInSeat = pOtherVehicle->GetSeatManager()->GetPedInSeat(seatIndex);

				if(pedInSeat && !pOtherPed->IsMale())
				{
					continue;
				}
			}

			//Distance check
			Vector3 vCopVehPos = VEC3V_TO_VECTOR3(pVeh->GetTransform().GetPosition());
			const Vector3 vVehiclePos = VEC3V_TO_VECTOR3(pOtherVehicle->GetTransform().GetPosition());
			float fDistToPed = (vCopVehPos - vVehiclePos).Mag2();

			//The CEntityScannerIterator is sorted by distance. Stop searching if this ped is too far away.
			if(fDistToPed > rage::square(CTaskPursueCriminal::ms_Tunables.m_MaxDistanceToFindVehicle) )
			{
				break;
			}

			//The ped is too close, there might be another ped in the list that isn't continue.
			if( fDistToPed < rage::square(CTaskPursueCriminal::ms_Tunables.m_MinDistanceToFindVehicle) )
			{
				continue;
			}

			//Height check
			float fHeightDiff = Abs(vCopVehPos.z - vVehiclePos.z);

			if (fHeightDiff > CTaskPursueCriminal::ms_Tunables.m_MaxHeightDifference)
			{
				continue;
			}

			//Direction check
			Vector3 vVehicleForward = VEC3V_TO_VECTOR3(pOtherVehicle->GetTransform().GetForward());
			vVehicleForward.NormalizeSafe();
			Vector3 vCopVehForward = VEC3V_TO_VECTOR3(pVeh->GetTransform().GetForward());
			vCopVehForward.NormalizeSafe();

			float fDot = vCopVehForward.Dot(vVehicleForward);
			if ( fDot < CTaskPursueCriminal::ms_Tunables.m_DotProductFacing )
			{
				continue;
			}

			//Behind check
			Vector3 vPedToVehicle = (vVehiclePos - vCopVehPos);
			vPedToVehicle.NormalizeSafe();
			float fDirection = vCopVehForward.Dot( vPedToVehicle );
			if (fDirection < CTaskPursueCriminal::ms_Tunables.m_DotProductBehind )
			{
				continue;
			}

			//Check Vehicle speed
			//Grab the vehicle speed.
			if(pOtherVehicle->IsCachedAiDataValid())
			{
				// If valid then used the cached data
				float fVehicleSpeed = pOtherVehicle->GetAiXYSpeed();
				if(fVehicleSpeed < CTaskPursueCriminal::ms_Tunables.m_CriminalVehicleMinStartSpeed)
				{
					continue;
				}
			}
			else
			{
				// If the cached data is invalid then get the velocity directly
				Vec3V vehVelocity = VECTOR3_TO_VEC3V(pOtherVehicle->GetVelocity());
				ScalarV fMaxSpeed = ScalarVFromF32(rage::square(CTaskPursueCriminal::ms_Tunables.m_CriminalVehicleMinStartSpeed));
					
				if(IsLessThanAll(MagSquared(vehVelocity.GetXY()), fMaxSpeed) != 0)
				{
					continue;
				}
			}

			m_CriminalEnt = pOtherVehicle;
			CRandomEventManager::GetInstance().ResetGlobalTimerTicking();
			return;
		}
	}
}


