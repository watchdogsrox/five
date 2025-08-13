// FILE :    TaskFirePatrol.cpp
// PURPOSE : Basic patrolling task of a fireman, patrol in vehicle or on foot until given specific orders by dispatch
// CREATED : 25-05-2009

// File header
#include "Task/Service/Fire/TaskFirePatrol.h"

// Framework headers
#include "ai/task/taskchannel.h"

// Game headers
#include "Event/Decision/EventDecisionMaker.h"
#include "Game/Dispatch/DispatchManager.h"
#include "Game/Dispatch/Incidents.h"
#include "Game/Dispatch/Orders.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Task/Default/TaskUnalerted.h"
#include "Task/Default/TaskWander.h"
#include "Task/Movement/TaskNavMesh.h"
#include "Task/System/TaskTypes.h"

#include "Task/Vehicle/TaskCar.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Task/Vehicle/TaskExitVehicle.h"
#include "Task/Vehicle/TaskVehicleWeapon.h"
#include "VehicleAI/driverpersonality.h"
#include "Task/Combat/Subtasks/TaskShootAtTarget.h"
#include "Task/Scenario/Info/ScenarioInfoManager.h"
#include "Task/Scenario/Types/TaskUseScenario.h"
#include "Task/System/TaskHelpers.h"
#include "Vehicles/Automobile.h"
#include "Vehicles/componentReserve.h"
#include "Vehicles/vehicle.h"
#include "Vehicles/VehicleGadgets.h"
#include "VehicleAi/pathfind.h"
#include "VehicleAI/task/TaskVehicleCruise.h"
#include "VehicleAI/task/TaskVehicleGoToAutomobile.h"
#include "VehicleAi/task/TaskVehiclePark.h"
#include "vehicleAi/VehicleIntelligence.h"
#include "Vfx/Misc/Fire.h"

AI_OPTIMISATIONS()

const float CTaskFirePatrol::s_fWalkToFireDistance = 10.0f;
const float CTaskFirePatrol::s_fVehicleStopAwayFromIncident = 2.5f;

const u32 INVALID_DECISION_MARKER = ATSTRINGHASH("INVALID_DECISION_MARKER", 0x0125782d4);

CTaskFirePatrol::CTaskFirePatrol(CTaskUnalerted* pUnalertedTask)
: m_pUnalertedTask(pUnalertedTask)
, m_pIgnoreOrder(NULL)
, m_pPointToReturnTo(NULL)
, m_iPointToReturnToRealTypeIndex(-1)
, m_SPC_UseInfo(NULL)
{
	m_iTimeToIncident = 0;
	m_iHangAroundTruckTimer.Unset();
	m_bShouldInvestigateExpiredFire = false;
	m_pCurrentFireDontDereferenceMe = NULL;
	m_PreviousDecisionMarkerId = INVALID_DECISION_MARKER;
	m_bWantsToBeDriver = false;
	m_bAllowToUseTurnedOffNodes = false;
	SetInternalTaskType(CTaskTypes::TASK_FIRE_PATROL);
}

CTaskFirePatrol::~CTaskFirePatrol()
{
// 	if (!GetEntity() || !GetPed())
// 	{
// 		return;
// 	}
// 
// 	CFireOrder* pOrder = HelperGetFireOrder();
// 	if (pOrder)
// 	{
// 		pOrder->SetEntityFinishedWithIncident();
// 	}
}

void CTaskFirePatrol::CleanUp()
{
	DeregisterFightingFire();
	ResetDefaultEventResponse();
}

CScenarioPoint* CTaskFirePatrol::GetScenarioPoint() const
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

int CTaskFirePatrol::GetScenarioPointType() const
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

////////////////////////////////////////////////////////////////////////////////

bool CTaskFirePatrol::ProcessMoveSignals()
{
	CPed* pPed = GetPed();

	if (pPed)
	{
		CVehicle* pVehicle = pPed->GetMyVehicle();
		if (pVehicle)
		{
			pVehicle->m_nVehicleFlags.bDisablePretendOccupants = true;
			pVehicle->m_nVehicleFlags.bForceOtherVehiclesToStopForThisVehicle = true;
			pVehicle->m_nVehicleFlags.bPreventBeingDummyUnlessCollisionLoadedThisFrame = true;
		}
	}

	return true;
}

CTask::FSM_Return CTaskFirePatrol::ProcessPreFSM()
{	
	//Make sure the fire man doesn't get culled
	CPed* pPed = GetPed();

	if (pPed)
	{
		pPed->SetPedResetFlag( CPED_RESET_FLAG_CullExtraFarAway, true );
		pPed->SetPedResetFlag( CPED_RESET_FLAG_TaskCullExtraFarAway, true );

		CVehicle* pVehicle = pPed->GetMyVehicle();
		if (pVehicle)
		{
			if(pVehicle->IsDriver(pPed))
			{
				pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_FallOutOfVehicleWhenKilled, true );
			}
		}
	}

	switch(GetState())
	{
		case State_GoToIncidentLocationOnFoot:
		case State_SprayFireWithFireExtinguisher:
		case State_ExitVehicle:
		case State_ReturnToTruck:
		case State_HandleExpiredFire:
			{
				if(GetPed())
				{
					GetPed()->m_nPhysicalFlags.bNotDamagedByFlames = true;
				}
				break;
			}
		default:
			{
				if(GetPed())
				{
					GetPed()->m_nPhysicalFlags.bNotDamagedByFlames = false;
				}
				break;
			}
	}

	// Check shocking events if we've switched decision maker
	if( m_PreviousDecisionMarkerId != INVALID_DECISION_MARKER )
		pPed->GetPedIntelligence()->SetCheckShockFlag(true);

	return FSM_Continue;
}

CTask::FSM_Return CTaskFirePatrol::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	FSM_Begin
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_Unalerted)
			FSM_OnEnter
				Unalerted_OnEnter();
			FSM_OnUpdate
				return Unalerted_OnUpdate();

		FSM_State(State_ChoosePatrol)
			FSM_OnUpdate
				return ChoosePatrol_OnUpdate();

		FSM_State(State_RespondToOrder)
			FSM_OnUpdate
				return RespondToOrder_OnUpdate();

		FSM_State(State_PatrolOnFoot)
			FSM_OnEnter
				PatrolOnFoot_OnEnter();
			FSM_OnUpdate
				return PatrolOnFoot_OnUpdate();

		FSM_State(State_PatrolInVehicle)
			FSM_OnEnter
				PatrolInVehicle_OnEnter();
			FSM_OnUpdate
				return PatrolInVehicle_OnUpdate();

		FSM_State(State_PatrolAsPassenger)
			FSM_OnEnter
				PatrolAsPassenger_OnEnter();
			FSM_OnUpdate
				return PatrolAsPassenger_OnUpdate();

		FSM_State(State_GoToIncidentLocationInVehicle)
			FSM_OnEnter
				GoToIncidentLocationInVehicle_OnEnter();
			FSM_OnUpdate
				return GoToIncidentLocationInVehicle_OnUpdate();
			FSM_OnExit
				GoToIncidentLocationInVehicle_OnExit();

		FSM_State(State_GoToIncidentLocationAsPassenger)
			FSM_OnEnter
				GoToIncidentLocationAsPassenger_OnEnter();
			FSM_OnUpdate
				return GoToIncidentLocationAsPassenger_OnUpdate();

		FSM_State(State_GoToIncidentLocationOnFoot)
			FSM_OnEnter
				GoToIncidentLocationOnFoot_OnEnter();
			FSM_OnUpdate
				return GoToIncidentLocationOnFoot_OnUpdate();

		FSM_State(State_DriverArriveAtIncident)
			FSM_OnEnter
				DriverArriveAtIncident_OnEnter();
			FSM_OnUpdate
				return DriverArriveAtIncident_OnUpdate();

		FSM_State(State_StopVehicle)
			FSM_OnEnter
				StopVehicle_OnEnter();
			FSM_OnUpdate
				return StopVehicle_OnUpdate();

		FSM_State(State_SprayFireWithFireExtinguisher)
			FSM_OnEnter
				SprayFireWithFireExtinguisher_OnEnter();
			FSM_OnUpdate
				return SprayFireWithFireExtinguisher_OnUpdate();

		FSM_State(State_SprayFireWithMountedWaterHose)
			FSM_OnEnter
				SprayFireWithMountedWaterHose_OnEnter();
			FSM_OnUpdate
				return SprayFireWithMountedWaterHose_OnUpdate();

		FSM_State(State_ReturnToTruck)
			FSM_OnEnter
				ReturnToTruck_OnEnter();
			FSM_OnUpdate
				return ReturnToTruck_OnUpdate();

		FSM_State(State_ExitVehicle)
			FSM_OnEnter
				ExitVehicle_OnEnter();
			FSM_OnUpdate
				return ExitVehicle_OnUpdate();

		FSM_State(State_WaitInTruck)
			FSM_OnEnter
				WaitInTruck_OnEnter();
			FSM_OnUpdate
				return WaitInTruck_OnUpdate();

		FSM_State(State_MakeDecisionHowToEnd)
			FSM_OnEnter
				MakeDecisionHowToEnd_OnEnter();
			FSM_OnUpdate
				return MakeDecisionHowToEnd_OnUpdate();

		FSM_State(State_HandleExpiredFire)
			FSM_OnEnter
				HandleExpiredFire_OnEnter();
			FSM_OnUpdate
				return HandleExpiredFire_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

#if !__FINAL
const char * CTaskFirePatrol::GetStaticStateName( s32 iState  )
{
	taskAssert(iState >= State_Start && iState <= State_Finish);

	switch (iState)
	{
	case State_Start:							return "State_Start";
	case State_Unalerted:						return "State_Unalerted";
	case State_ChoosePatrol:					return "State_ChoosePatrol";
	case State_RespondToOrder:					return "State_RespondToOrder";
	case State_PatrolInVehicle:					return "State_PatrolInVehicle";
	case State_PatrolOnFoot:					return "State_PatrolOnFoot";
	case State_PatrolAsPassenger:				return "State_PatrolAsPassenger";
	case State_GoToIncidentLocationOnFoot:		return "State_GoToIncidentLocationOnFoot";
	case State_GoToIncidentLocationInVehicle:	return "State_GoToIncidentLocationInVehicle";
	case State_GoToIncidentLocationAsPassenger:	return "State_GoToIncidentLocationAsPassenger";
	case State_DriverArriveAtIncident:			return "State_DriverArriveAtIncident";
	case State_StopVehicle:						return "State_StopVehicle";
	case State_SprayFireWithFireExtinguisher:	return "State_SprayFireWithFireExtinguisher";
	case State_SprayFireWithMountedWaterHose:	return "State_SprayFireWithMountedWaterHose";
	case State_ExitVehicle:						return "State_ExitVehicle";
	case State_ReturnToTruck:					return "State_ReturnToTruck";
	case State_WaitInTruck:						return "State_WaitInTruck";
	case State_MakeDecisionHowToEnd:			return "State_MakeDecisionHowToEnd";
	case State_HandleExpiredFire:				return "State_HandleExpiredFire";
	case State_Finish:							return "State_Finish";
	default: taskAssert(0); 	
	}

	return "State_Invalid";
}
#endif // !__FINAL


CTask::FSM_Return CTaskFirePatrol::Start_OnUpdate()
{
	RequestProcessMoveSignalCalls();

	if (!m_pUnalertedTask)
	{
		// If there is no unalerted task, the ped may still have a scenario point reserved.
		// We can build an unalerted task from this stored point.
		AttemptToCreateResumableUnalertedTask();
	}

	if(m_pUnalertedTask)
	{
		SetState(State_Unalerted);
		return FSM_Continue;
	}
	else
	{
		//@@: location CTASKFIREPATROL_STARTONUPDATE_GET_FIRE_ORDER
		CFireOrder* pOrder = HelperGetFireOrder();
		if( pOrder )
		{
			SetState(State_RespondToOrder);
			SetFiremanEventResponse();
			return FSM_Continue;
		}
		else
		{
			//It's possible that the task aborted, attempt to get back into the truck.
			CVehicle* pVehicle = GetPed()->GetMyVehicle();
			if (pVehicle)
			{
				if(!GetPed()->GetIsInVehicle())
				{
					SetState(State_ReturnToTruck);
					return FSM_Continue;
				}
				else
				{
					SetState(State_WaitInTruck);
					return FSM_Continue;
				}
			}
			SetState(State_ChoosePatrol);
			return FSM_Continue;
		}
	}
}


void CTaskFirePatrol::Unalerted_OnEnter()
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

CTask::FSM_Return CTaskFirePatrol::Unalerted_OnUpdate()
{
	CFireOrder* pOrder = HelperGetFireOrder();
	if(pOrder && pOrder->GetIsValid())
	{
		//Cache the scenario point to return to.
		CacheScenarioPointToReturnTo();

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

CTask::FSM_Return CTaskFirePatrol::ChoosePatrol_OnUpdate()
{
	m_bCantPathOnFoot = false;
	CPed* pPed = GetPed();
	if( pPed->GetIsInVehicle() )
	{
		if( pPed->GetIsDrivingVehicle() )
		{
			SetState(State_PatrolInVehicle);
			return FSM_Continue;
		}
		else
		{
			SetState(State_PatrolAsPassenger);
			return FSM_Continue;
		}
	}
	else
	{
		CFireOrder* pOrder = HelperGetFireOrder();

		CVehicle* pVehicle = pPed->GetMyVehicle();
		if (!pVehicle && pOrder)
		{
			pVehicle = pOrder->GetFireTruck();
		}

		if (pVehicle && pVehicle->IsInDriveableState())
		{
			SetState(State_ReturnToTruck);
			return FSM_Continue;
		}
	}

	if (!m_pUnalertedTask)
	{
		// If there is no unalerted task, the ped may still have a scenario point reserved.
		// We can build an unalerted task from this stored point.
		AttemptToCreateResumableUnalertedTask();
	}

	if (m_pUnalertedTask)
	{
		SetState(State_Unalerted);
	}
	else
	{
		SetState(State_PatrolOnFoot);
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskFirePatrol::RespondToOrder_OnUpdate()
{
	CFireOrder* pOrder = HelperGetFireOrder();
	if( pOrder )
	{
		if( GetPed()->GetIsInVehicle() )
		{
			if( GetPed()->GetIsDrivingVehicle() )
			{
				SetState(State_GoToIncidentLocationInVehicle);
				return FSM_Continue;
			}
			else
			{
				SetState(State_GoToIncidentLocationAsPassenger);
				return FSM_Continue;
			}
		}
		else
		{
			SetState(State_GoToIncidentLocationOnFoot);
			return FSM_Continue;
		}
	}

	SetState(State_ChoosePatrol);
	return FSM_Continue;
}

void CTaskFirePatrol::PatrolInVehicle_OnEnter()
{
	CVehicle* pVehicle = GetPed()->GetMyVehicle();
	if( aiVerifyf(pVehicle, "PatrolInVehicle_OnEnter: Vehicle NULL!") )
	{
		SetNewTask(CTaskUnalerted::SetupDrivingTaskForEmergencyVehicle(*GetPed(), *pVehicle, true, false));
	}
}

CTask::FSM_Return CTaskFirePatrol::PatrolInVehicle_OnUpdate()
{
	CFireOrder* pOrder = HelperGetFireOrder();
	if( pOrder && pOrder->GetIncident() && !pOrder->GetAllResourcesAreDone())
	{
		SetState(State_RespondToOrder);
		return FSM_Continue;
	}

	return FSM_Continue;
}

void CTaskFirePatrol::PatrolAsPassenger_OnEnter()
{
	CVehicle* pVehicle = GetPed()->GetMyVehicle();
	if( aiVerifyf(pVehicle, "PatrolAsPassenger_OnEnter: Vehicle NULL!") )
	{
		SetNewTask(rage_new CTaskInVehicleBasic(pVehicle));
	}
}

CTask::FSM_Return CTaskFirePatrol::PatrolAsPassenger_OnUpdate()
{
	CFireOrder* pOrder = HelperGetFireOrder();
	if( pOrder && pOrder->GetIncident()  && !pOrder->GetAllResourcesAreDone())
	{
		SetState(State_RespondToOrder);
		return FSM_Continue;
	}
	
	CPed* pPed = GetPed();

	//Replace dead/null drivers
	CPed* pDriver = pPed->GetMyVehicle()->GetSeatManager()->GetPedInSeat(0);
	if(!pDriver || pDriver->IsDead())
	{
		//Check if the driver is out of the vehicle
		CPed* pLastDriver = pPed->GetMyVehicle()->GetSeatManager()->GetLastPedInSeat(0);
		if(!pLastDriver || pLastDriver->IsDead())
		{
			//Check no one else wants to be the driver, if not then take over.
			for ( int i = 1; i < pPed->GetMyVehicle()->GetSeatManager()->GetMaxSeats(); i++)
			{
				CPed* pSeatedPed = pPed->GetMyVehicle()->GetSeatManager()->GetPedInSeat(i);
				if(!pSeatedPed)
				{
					pSeatedPed = pPed->GetMyVehicle()->GetSeatManager()->GetLastPedInSeat(i);
				}

				if(pSeatedPed)
				{
					//Priority lower seating positions
					if (pSeatedPed == pPed)
					{
						m_bWantsToBeDriver = true;
						SetState(State_ExitVehicle);
						return FSM_Continue;
					}
					else
					{
						//Assume the ped in the front will take over
						return FSM_Continue;
					}
				}
			}
		}
	}

	return FSM_Continue;
}

void CTaskFirePatrol::PatrolOnFoot_OnEnter()
{
	SetNewTask(rage_new CTaskWander(MOVEBLENDRATIO_WALK, GetPed()->GetCurrentHeading(), NULL, -1, true, CTaskWander::TARGET_RADIUS, true, true));
}

CTask::FSM_Return CTaskFirePatrol::PatrolOnFoot_OnUpdate()
{
	CFireOrder* pOrder = HelperGetFireOrder();
	if( pOrder )
	{
		if( pOrder->GetBestFire(GetPed()) != NULL )
		{
			SetState(State_RespondToOrder);
			return FSM_Continue;
		}
	}

	return FSM_Continue;
}

Vector3 CTaskFirePatrol::GetBestTruckTargetLocation(CPed * pPed, CFireOrder* pOrder)
{
	Vector3 vLocation = pOrder->GetIncident()->GetLocation();
	Vector3 vPedLocation = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	Vector3 vLocationDir = vLocation - vPedLocation;
	vLocationDir.NormalizeSafe(ORIGIN);
	float fFireIncidentRadius = 5.0f;
	if( pOrder->GetFireIncident())
	{
		fFireIncidentRadius = pOrder->GetFireIncident()->GetFireIncidentRadius();
	}
	
	if (m_NearestRoadNodeLocation.IsEmpty())
	{
		m_NearestRoadNodeLocation = ThePaths.FindNodeClosestToCoors(vLocation,40.0f);
	}

	if (!m_NearestRoadNodeLocation.IsEmpty() && ThePaths.IsRegionLoaded(m_NearestRoadNodeLocation))
	{
		Vector3 m_vNearestRoadNode;
		const CPathNode *pCurrentNode = ThePaths.FindNodePointerSafe(m_NearestRoadNodeLocation);
		if(!pCurrentNode)
		{
			m_NearestRoadNodeLocation = ThePaths.FindNodeClosestToCoors(vLocation,40.0f);
			pCurrentNode = ThePaths.FindNodePointer(m_NearestRoadNodeLocation);
			aiFatalAssertf(pCurrentNode, "CTaskFirePatrol::GetBestTruckTargetLocation, can't find node");
		}

		pCurrentNode->GetCoors(m_vNearestRoadNode);

		float fDistFromRoadNodeToFireSq = (vLocation - m_vNearestRoadNode).Mag2();
		
		//This road node is fine, it's out side of the incident radius
		if( rage::square(fFireIncidentRadius) < fDistFromRoadNodeToFireSq )
		{
			return m_vNearestRoadNode;
		}

		//The road node is inside the incident radius, try and find a better road node to go to.
		bool bfoundbetternode = false;
		Vector3 vBestLinkLocation(VEC3_MAX);
		float fBestLinkDistance = 99999.0f;
		const int iNumLinks = pCurrentNode->NumLinks();
		for (int i=0; i < iNumLinks; i++)
		{
			const CPathNodeLink& link = ThePaths.GetNodesLink(pCurrentNode, i);
			const CPathNode * pOtherNode = ThePaths.FindNodePointerSafe(link.m_OtherNode);

			if(pOtherNode)
			{
				if(pOtherNode->IsWaterNode() || pOtherNode->IsPedNode() || pOtherNode->IsParkingNode() || pOtherNode->IsOpenSpaceNode())
				{
					continue;
				}

				Vector3 vLinkLocation;
				pOtherNode->GetCoors(vLinkLocation);

				float fDistToLinkRoadNodeSq = (vLocation - vLinkLocation).Mag2();
				float fDistFromLinkToPedSq = (vPedLocation - vLinkLocation).Mag2();
				if (  rage::square(fFireIncidentRadius) < fDistToLinkRoadNodeSq &&
					 fDistFromLinkToPedSq < fBestLinkDistance )
				{
					fBestLinkDistance = fDistFromLinkToPedSq;
					vBestLinkLocation = vLinkLocation;
					m_NearestRoadNodeLocation = link.m_OtherNode;
					bfoundbetternode = true;
				}
			}
		}

		//If we can't find a better node then use radius of the incident to make sure we stop outside of the incident
		if (bfoundbetternode)
		{
			return vBestLinkLocation;
		}
	}
	else
	{
		//There isn't a node within 40m, just use the closest. This handles phone call incidents
		m_NearestRoadNodeLocation = ThePaths.FindNodeClosestToCoors(vLocation);
		if (!m_NearestRoadNodeLocation.IsEmpty() && ThePaths.IsRegionLoaded(m_NearestRoadNodeLocation))
		{
			//Ensure the node is valid.
			const CPathNode* pCurrentNode = ThePaths.FindNodePointerSafe(m_NearestRoadNodeLocation);
			if(pCurrentNode)
			{
				pCurrentNode->GetCoors(vLocation);
			}
		}
	}
	
	return vLocation - (vLocationDir * (fFireIncidentRadius + s_fVehicleStopAwayFromIncident ));
}

SearchStatus CTaskFirePatrol::CheckPedsCanReachFire(CPed* pPed, const Vector3& target, float fCompletionRadius)
{	
	if(!m_RouteSearchHelper.IsSearchActive())
	{
		m_RouteSearchHelper.StartSearch( pPed, VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), target, fCompletionRadius );
	}

	// When the route helper is active we need to check if the results are ready yet
	float fDistance = 0.0f;
	Vector3 vLastPoint(0.0f, 0.0f, 0.0f);
	SearchStatus eSearchStatus;

	if(m_RouteSearchHelper.GetSearchResults( eSearchStatus, fDistance, vLastPoint ))
	{
		// reset the search when the results are ready
		m_RouteSearchHelper.ResetSearch();
		
		//Return if we were successful
		return eSearchStatus;
	}

	return SS_StillSearching;	
}

void CTaskFirePatrol::GoToIncidentLocationInVehicle_OnEnter()
{
	CFireOrder* pOrder = HelperGetFireOrder();
	if( aiVerifyf(pOrder, "GoToIncidentLocation_OnEnter:: Invalid Order" ) &&
		aiVerifyf(GetPed()->GetIsInVehicle(), "GoToIncidentLocationInVehicle_OnEnter: Not in vehicle!") )
	{
		//Timer for the fire truck to wait at incident if it's invalid
		m_iTimeToIncident = fwRandom::GetRandomNumberInRange(3000, 4000);

		aiAssert(GetPed()->GetMyVehicle());
		GetPed()->GetMyVehicle()->TurnSirenOn(true);
		GetPed()->GetMyVehicle()->m_nVehicleFlags.bIsFireTruckOnDuty = true;

		pOrder->SetFireTruck(GetPed()->GetMyVehicle());

		Vector3 m_vBestTarget = GetBestTruckTargetLocation(GetPed(), pOrder);

		sVehicleMissionParams params;
		params.m_iDrivingFlags = DMode_AvoidCars|DF_SteerAroundPeds|DF_ForceJoinInRoadDirection;
		params.m_fCruiseSpeed = 15.0f;
		params.SetTargetPosition(m_vBestTarget);
		params.m_fTargetArriveDist = 25.0f;

		aiTask *pCarTask = CVehicleIntelligence::CreateAutomobileGotoTask(params, 1.0f);
		SetNewTask(rage_new CTaskControlVehicle(GetPed()->GetMyVehicle(), pCarTask ));

		//Get the nearest road node, If it's off then allow the fire truck to contiune on turned off road nodes.
		CNodeAddress nearestNode = ThePaths.FindNodeClosestToCoors(VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition()),20.0f);

		if (!nearestNode.IsEmpty() && ThePaths.IsRegionLoaded(nearestNode))
		{
			const CPathNode *pCurrentNode = ThePaths.FindNodePointerSafe(nearestNode);
			if(pCurrentNode && pCurrentNode->IsSwitchedOff())
			{
				m_bAllowToUseTurnedOffNodes = true;
			}
		}
	}
}

CTask::FSM_Return CTaskFirePatrol::GoToIncidentLocationInVehicle_OnUpdate()
{
	CFireOrder* pOrder = HelperGetFireOrder();
	if( pOrder ==NULL || !pOrder->GetIsValid() )
	{
		SetState(State_ChoosePatrol);
		return FSM_Continue;
	}

	if( !GetPed()->GetIsInVehicle() )
	{
		SetState(State_RespondToOrder);
		return FSM_Continue;
	}

	bool bWithinRange = false;
	float fDistFromIncidentSQ = -1.0f;
	if (pOrder->GetIncident())
	{
		fDistFromIncidentSQ = (VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition()) - pOrder->GetIncident()->GetLocation()).Mag2();
		bWithinRange = fDistFromIncidentSQ < rage::square(40.0f);
	}

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished) || !GetSubTask())
	{
		if (GetPed()->GetMyVehicle()->GetVelocity().Mag2() < 0.5f )
		{
			if(pOrder->GetFireIncident())
			{
				SearchStatus status = CheckPedsCanReachFire(GetPed(),pOrder->GetFireIncident()->GetLocation(), 10.0f );
				if(status == SS_StillSearching && bWithinRange)
				{
					return FSM_Continue;
				}

				if(status == SS_SearchFailed)
				{
					m_bCantPathOnFoot = true;
				}
			}

			if(!bWithinRange || m_bCantPathOnFoot)
			{
				//Far away, go into wait state so other peds can follow suit
				pOrder->SetEntityArrivedAtIncident();
				pOrder->SetEntityFinishedWithIncident();
				SetState(State_WaitInTruck);
				return FSM_Continue;
			}

			pOrder->SetEntityArrivedAtIncident();

			if (EvaluateUsingMountedWaterHose(pOrder))
			{
				SetState(State_SprayFireWithMountedWaterHose);
				return FSM_Continue;
			}

			//Called from phone
			if(!pOrder->GetFireIncident())
			{
				m_iTimeToIncident -= fwTimer::GetTimeStepInMilliseconds();
				if(m_iTimeToIncident > 0)
				{
					return FSM_Continue;	
				}

				pOrder->SetEntityFinishedWithIncident();
				SetState(State_ChoosePatrol);
				return FSM_Continue;
			}

			//Go into state to let passengers know we've arrived. 
			//Passengers on the back of the fire truck now take priority
			SetState(State_DriverArriveAtIncident);
			return FSM_Continue;
		}
	}
	else
	{
		//if we're responding to a FireIncident (as opposed to script)
		//see if we should update our target location
		if (GetSubTask()->GetTaskType() == CTaskTypes::TASK_CONTROL_VEHICLE)
		{
			aiTask* pTask = static_cast<CTaskControlVehicle*>(GetSubTask())->GetVehicleTask();
			if (pTask && (pTask->GetTaskType() == CTaskTypes::TASK_VEHICLE_GOTO_AUTOMOBILE_NEW))
			{
				//Set target position
				if(pOrder->GetFireIncident())
				{
					Vector3 m_vBestTarget = GetBestTruckTargetLocation(GetPed(), pOrder);
					CTaskVehicleGoToAutomobileNew* pTaskVehicleGoTo = static_cast<CTaskVehicleGoToAutomobileNew*>(pTask);
					if(pTaskVehicleGoTo)
					{
						pTaskVehicleGoTo->SetTargetPosition(&m_vBestTarget);
					}
				}

				if(!m_bAllowToUseTurnedOffNodes && GetPed()->GetMyVehicle()->IsDriver(GetPed()))
				{
					//Stop if there is the next road node is turned off
					CVehicleNodeList * pNodeList = GetPed()->GetMyVehicle()->GetIntelligence()->GetNodeList();
					if(pNodeList)
					{
						//Look 2 nodes ahead to stop before going down the turned off road
						s32 iFutureNode = pNodeList->GetTargetNodeIndex() + 2;
						s32 iOldNode = pNodeList->GetTargetNodeIndex();

						for (s32 node = iOldNode; node < iFutureNode && node < CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED; node++)
						{
							if (pNodeList->GetPathNodeAddr(node).IsEmpty() || !ThePaths.IsRegionLoaded(pNodeList->GetPathNodeAddr(node)))
							{
								continue;
							}

							const CPathNode *pNode = ThePaths.FindNodePointer(pNodeList->GetPathNodeAddr(node));
							if (pNode && (pNode->IsSwitchedOff() || pNode->m_2.m_deadEndness))
							{
								SetState(State_StopVehicle);
								return FSM_Continue;
							}
						}
					}
				}
			}
		}
	}

	
	//Stop if the player is in the way when approaching the fire
	static dev_float fDetectPlayerDistance = 55.0f; 
	CVehicleIntelligence* pVehIntelligence = GetPed()->GetMyVehicle()->GetIntelligence();
	if(fDistFromIncidentSQ < rage::square(fDetectPlayerDistance) && pVehIntelligence)
	{
		static dev_float fPosInFrontOfVehicle = 13.0f;
		static dev_float fCollisionDistSQ = 12.25f; //3.5m
		const Vector3 vVehPosition = VEC3V_TO_VECTOR3(GetPed()->GetMyVehicle()->GetTransform().GetPosition()) + (VEC3V_TO_VECTOR3(GetPed()->GetMyVehicle()->GetTransform().GetForward()) * fPosInFrontOfVehicle);
#if __BANK
		if(CDispatchManager::GetInstance().DebugEnabled(DT_FireDepartment))
		{
			grcDebugDraw::Sphere(vVehPosition, sqrt(fCollisionDistSQ), Color_purple, false, 1);
		}
#endif
		CEntityScannerIterator entityList = pVehIntelligence->GetPedScanner().GetIterator();
		for( CEntity* pEntity = entityList.GetFirst(); pEntity; pEntity = entityList.GetNext() )
		{
			Assert(pEntity && pEntity->GetIsTypePed());
			const CPed* pPed = static_cast<const CPed*>(pEntity);
			if (pPed->IsPlayer() )
			{
				const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
				
				if((vPedPosition - vVehPosition).Mag2() < fCollisionDistSQ)
				{
					SetState(State_StopVehicle);
					return FSM_Continue;
				}
			}
		}
	}
	
	//artificially lengthen the lifetime of appropriate fires while we're
	//on our way
	UpdateKeepFiresAlive();

	//if we're stopped, but not there yet, remove the flag that tells us to stop for
	//cars
	if (GetPed()->GetMyVehicle()->GetVelocity().Mag2() < 0.5f)
	{
		CTaskVehicleMissionBase* pActiveVehTask = GetPed()->GetMyVehicle()->GetIntelligence()->GetActiveTask();
		if (pActiveVehTask)
		{
			pActiveVehTask->SetDrivingFlag(DF_StopForCars, false);
			if (pActiveVehTask->GetSubTask())
			{
				static_cast<CTaskVehicleMissionBase*>(pActiveVehTask->GetSubTask())->SetDrivingFlag(DF_StopForCars, false);
			}
		}	
	}

	return FSM_Continue;
}

void CTaskFirePatrol::GoToIncidentLocationInVehicle_OnExit()
{
	m_bAllowToUseTurnedOffNodes = false;
}

void CTaskFirePatrol::StopVehicle_OnEnter()
{
	//Create the vehicle task for stop.
	CTask* pVehicleTaskForStop = rage_new CTaskVehicleStop(CTaskVehicleStop::SF_UseFullBrake);

	//Create the task for stop.
	CTask* pTaskForStop = rage_new CTaskControlVehicle(GetPed()->GetVehiclePedInside(), pVehicleTaskForStop);

	//Start the task.
	SetNewTask(pTaskForStop);
}

CTask::FSM_Return CTaskFirePatrol::StopVehicle_OnUpdate()
{
	CFireOrder* pOrder = HelperGetFireOrder();
	if( pOrder ==NULL || !pOrder->GetIsValid() )
	{
		SetState(State_ChoosePatrol);
		return FSM_Continue;
	}

	if( !GetPed()->GetIsInVehicle() )
	{
		SetState(State_RespondToOrder);
		return FSM_Continue;
	}

	bool bWithinRange = false;

	if (pOrder->GetIncident())
	{
		bWithinRange = (VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition()) - pOrder->GetIncident()->GetLocation()).Mag2() < rage::square(40.0f);
	}

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished) || !GetSubTask())
	{
		if(!bWithinRange)
		{
			//Far away, go into wait state so other peds can follow suit
			pOrder->SetEntityArrivedAtIncident();
			pOrder->SetEntityFinishedWithIncident();
			SetState(State_WaitInTruck);
			return FSM_Continue;
		}

		pOrder->SetEntityArrivedAtIncident();

		if (EvaluateUsingMountedWaterHose(pOrder))
		{
			SetState(State_SprayFireWithMountedWaterHose);
			return FSM_Continue;
		}

		//Called from phone
		if(!pOrder->GetFireIncident())
		{
			pOrder->SetEntityFinishedWithIncident();
			SetState(State_ChoosePatrol);
			return FSM_Continue;
		}

		//Go into state to let passengers know we've arrived. 
		//Passengers on the back of the fire truck now take priority
		SetState(State_DriverArriveAtIncident);
		return FSM_Continue;
	}

	return FSM_Continue;
}


bool CTaskFirePatrol::PassengerArrivedAtMobileCall(CPed* pPed)
{
	COrder* pOrder = pPed->GetPedIntelligence()->GetOrder();
	if(pOrder && pOrder->GetIncident() && pOrder->GetIncident()->GetType() == CIncident::IT_Scripted)
	{
		
		CPed* pDriver = pPed->GetMyVehicle()->GetDriver();
		if(pDriver && pOrder->GetEntityFinishedWithIncident())
		{
			return true;
		}
	}
	return false;
}

void CTaskFirePatrol::GoToIncidentLocationAsPassenger_OnEnter()
{
}

CTask::FSM_Return CTaskFirePatrol::GoToIncidentLocationAsPassenger_OnUpdate()
{
	CPed* pPed = GetPed();
	CFireOrder* pOrder = HelperGetFireOrder();
	if( pOrder ==NULL )
	{
		SetState(State_ChoosePatrol);
		return FSM_Continue;
	}

	if( !pPed->GetIsInVehicle() )
	{
		SetState(State_RespondToOrder);
		return FSM_Continue;
	}

	//Called from phone, stop once the driver has
	//As long as we have an actual fire (else we will fall through and go to State_WaitInTruck
	if(PassengerArrivedAtMobileCall(pPed) && pOrder && pOrder->GetIncident() && !pOrder->GetAllResourcesAreDone())
	{
		pOrder->SetEntityArrivedAtIncident();
		pOrder->SetEntityFinishedWithIncident();
		SetState(State_ChoosePatrol);
		return FSM_Continue;
	}

	//If the driver is using the hose then go into the wait state
	CPed* pDriver = pPed->GetMyVehicle()->GetSeatManager()->GetPedInSeat(0);
	if(pDriver != NULL &&
	   pDriver->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_VEHICLE_MOUNTED_WEAPON))
	{
		if(pDriver->GetPedIntelligence()->GetQueriableInterface()->GetStateForTaskType(CTaskTypes::TASK_VEHICLE_MOUNTED_WEAPON) == CTaskVehicleMountedWeapon::State_Firing)
		{
			pOrder->SetEntityArrivedAtIncident();
			SetState(State_WaitInTruck);
			return FSM_Continue;
		}
	}
	
	//If the driver dies
	if(!pDriver || pDriver->IsDead())
	{
		//Check if the driver is out of the vehicle
		CPed* pLastDriver = pPed->GetMyVehicle()->GetSeatManager()->GetLastPedInSeat(0);
		if(!pLastDriver || pLastDriver->IsDead())
		{
			//Check no one else wants to be the driver, if not then take over.
			for ( int i = 1; i < pPed->GetMyVehicle()->GetSeatManager()->GetMaxSeats(); i++)
			{
				CPed* pSeatedPed = pPed->GetMyVehicle()->GetSeatManager()->GetPedInSeat(i);
				if(!pSeatedPed)
				{
					pSeatedPed = pPed->GetMyVehicle()->GetSeatManager()->GetLastPedInSeat(i);
				}

				if(pSeatedPed)
				{
					//Priority lower seating positions
					if (pSeatedPed == pPed)
					{
						m_bWantsToBeDriver = true;
						SetState(State_ExitVehicle);
						return FSM_Continue;
					}
					else
					{
						//Assume the ped in the front will take over
						return FSM_Continue;
					}
				}
			}
		}
	}

	//If the driver hasn't said we have arrived then wait
	if(pDriver && (!pDriver->GetPedIntelligence()->GetOrder() || 
	   !pDriver->GetPedIntelligence()->GetOrder()->GetEntityArrivedAtIncident()))
	{
		return FSM_Continue;
	}

	pOrder->SetEntityArrivedAtIncident();

	//If the driver says we can't path to the location then don't get out of the truck
	if(pDriver)
	{
		CTaskFirePatrol * pTaskFirePatrol = (CTaskFirePatrol*)pDriver->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_FIRE_PATROL);
		if(pTaskFirePatrol && pTaskFirePatrol->GetCantPathOnFoot())
		{
			m_bCantPathOnFoot = true;
			pOrder->SetEntityFinishedWithIncident();
			SetState(State_WaitInTruck);
			return FSM_Continue;
		}
	}
	
	//Priorities peds on the back of the vehicle
	bool bOnLadder = false;
	CPed* pLadderPed = pPed->GetMyVehicle()->GetSeatManager()->GetPedInSeat(4);
	CPed* pLadderPedTwo = pPed->GetMyVehicle()->GetSeatManager()->GetPedInSeat(5);
	if(pLadderPedTwo == pPed || pLadderPed == pPed )
	{
		bOnLadder = true;
	}

	//Let the peds on the back of the truck respond first
	if(!bOnLadder)
	{
		//Check they haven't already finished with the incident
		if((pLadderPed && (!pLadderPed->GetPedIntelligence()->GetOrder() || !pLadderPed->GetPedIntelligence()->GetOrder()->GetEntityFinishedWithIncident())) || 
		   (pLadderPedTwo && (!pLadderPedTwo->GetPedIntelligence()->GetOrder() || !pLadderPedTwo->GetPedIntelligence()->GetOrder()->GetEntityFinishedWithIncident())))
		{
			return FSM_Continue;
		}
	}

	if( pOrder && pOrder->GetIsValid() )
	{
		CFire* pBestFire = pOrder->GetBestFire(pPed);
		if (pBestFire)
		{
			RegisterFightingFire(pBestFire);
			SetState(State_ExitVehicle);
			return FSM_Continue;
		}
	
		//There isn't a fire to attend but we still want to get off the ladder.
		//Go and investigate the expired fire instead.
		if(bOnLadder && pOrder->GetEntityThatHasBeenOnFire())
		{
			m_bShouldInvestigateExpiredFire = true;
			SetState(State_ExitVehicle);
			return FSM_Continue;
		}

		//Nothing for this ped to do, mark as finished and wait
		pOrder->SetEntityArrivedAtIncident();
		pOrder->SetEntityFinishedWithIncident();
		SetState(State_WaitInTruck);
		return FSM_Continue;
	}

	return FSM_Continue;
}

static const float s_fApproachFireDistance = 3.0f;

void CTaskFirePatrol::GoToIncidentLocationOnFoot_OnEnter()
{
	COrder* pOrder = GetPed()->GetPedIntelligence()->GetOrder();
	if( pOrder && pOrder->GetIsValid() &&
		aiVerifyf(!GetPed()->GetIsInVehicle(), "GoToIncidentLocationInVehicle_OnEnter: Not on foot!") )
	{
		//Make sure we are updating the arrived flag as we could have been returning to the truck on foot
		pOrder->SetEntityArrivedAtIncident();

		const CFireOrder* pFireOrder = pOrder->GetType() == COrder::OT_Fire ? static_cast<const CFireOrder*>(pOrder) : NULL;
		CFire* pBestFire = pFireOrder ? pFireOrder->GetBestFire(GetPed()) : NULL;
		Vector3 vLocation(Vector3::ZeroType);
		if (pBestFire)
		{
			RegisterFightingFire(pBestFire);
			vLocation = VEC3V_TO_VECTOR3(pBestFire->GetPositionWorld());
		}
		else if (pFireOrder && pFireOrder->GetEntityThatHasBeenOnFire() && m_bShouldInvestigateExpiredFire)
		{	
			vLocation = VEC3V_TO_VECTOR3(pFireOrder->GetEntityThatHasBeenOnFire()->GetTransform().GetPosition());
		}
		else
		{
			if(pFireOrder && pFireOrder->GetIncident())
			{
				vLocation = pFireOrder->GetIncident()->GetLocation();
			}
			else
			{
				//! This will force update func to set "State_MakeDecisionHowToEnd"
				return;
			}
		}

		float moveSpeed = vLocation.Dist2(VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition())) > s_fWalkToFireDistance*s_fWalkToFireDistance
			? MOVEBLENDRATIO_RUN : MOVEBLENDRATIO_WALK + 0.5f;
		CTask* pNewSubtask = rage_new CTaskMoveFollowNavMesh(moveSpeed, 
			vLocation,		
			s_fApproachFireDistance, 
			s_fWalkToFireDistance, 
			-1,
			true,
			false,
			NULL);
		SetNewTask(rage_new CTaskComplexControlMovement(pNewSubtask));
	
		//also equip the fire extinguisher
		if(GetPed()->GetWeaponManager())
		{
			GetPed()->GetWeaponManager()->EquipWeapon(WEAPONTYPE_FIREEXTINGUISHER,-1,true);
		}
	}
}

CTask::FSM_Return CTaskFirePatrol::GoToIncidentLocationOnFoot_OnUpdate()
{
	CFireOrder* pOrder = HelperGetFireOrder();
	if( pOrder == NULL || !pOrder->GetIsValid() )
	{
		SetState(State_MakeDecisionHowToEnd);
		return FSM_Continue;
	}

	if( GetPed()->GetIsInVehicle() )
	{
		SetState(State_RespondToOrder);
		return FSM_Continue;
	}

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished) || GetSubTask() == NULL )
	{
		pOrder->SetEntityArrivedAtIncident();

		//don't do anything for non-fire incidents.  most of the time this
		//is the case when the fire dept is called from the player's cell phone
		if (!pOrder->GetIncident() || pOrder->GetIncident()->GetType() != CIncident::IT_Fire)
		{
			pOrder->SetEntityFinishedWithIncident();
			SetState(State_MakeDecisionHowToEnd);
		}
		else if (!m_bShouldInvestigateExpiredFire)
		{
			SetState(State_SprayFireWithFireExtinguisher);
		}
		else
		{
			SetState(State_HandleExpiredFire);
		}
		
		return FSM_Continue;
	}

	CTaskMoveFollowNavMesh* pTask = static_cast<CTaskMoveFollowNavMesh *>(GetPed()->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH));
	if(pTask)
	{
		//Check if we are unable to find a route.
		if(pTask->IsUnableToFindRoute())
		{
			pOrder->SetEntityArrivedAtIncident();
			pOrder->SetEntityFinishedWithIncident();

			//If we can't get to the fire then assume we won't be able to get to any of the fires for this order so ignore it
			m_pIgnoreOrder = pOrder;

			SetState(State_MakeDecisionHowToEnd);
			return FSM_Continue;
		}
		
		//If the fire dies out then still go to the location to avoid pathing half way and then returning to the truck
		//If there is a better fire, then attend the new fire instead
		CFire* pBestFire = pOrder ? pOrder->GetBestFire(GetPed()) : NULL;
		if (pBestFire && pOrder->GetPedRespondingToFire(pBestFire) == NULL)
		{
			//Register the new fire so no other fire fighter response and we know which fire the current ped is respond to.
			RegisterFightingFire(pBestFire);
	
			//Path to the new location
			pTask->SetTarget(GetPed(),pBestFire->GetPositionWorld());
		}
	}	

	return FSM_Continue;
}

// DriverArriveAtIncident
void CTaskFirePatrol::DriverArriveAtIncident_OnEnter()
{

}

CTask::FSM_Return CTaskFirePatrol::DriverArriveAtIncident_OnUpdate()
{
	CFireOrder* pOrder = HelperGetFireOrder();
	if( pOrder ==NULL || !pOrder->GetIsValid() )
	{
		SetState(State_ChoosePatrol);
		return FSM_Continue;
	}

	if( !GetPed()->GetIsInVehicle() )
	{
		SetState(State_RespondToOrder);
		return FSM_Continue;
	}

	if(GetTimeInState() > 2.0f)
	{
		CFire* pBestFire = pOrder->GetBestFire(GetPed());
		if (pBestFire)
		{
			RegisterFightingFire(pBestFire);
			SetState(State_ExitVehicle);
			return FSM_Continue;
		}

		pOrder->SetEntityArrivedAtIncident();
		pOrder->SetEntityFinishedWithIncident();
		SetState(State_WaitInTruck);
		return FSM_Continue;
	}
	return FSM_Continue;
}

// SprayFireWithFireExtinguisher
void CTaskFirePatrol::SprayFireWithFireExtinguisher_OnEnter()
{
	CFireOrder* pOrder = HelperGetFireOrder();
	if (pOrder)
	{
		CFire* pBestFire = pOrder->GetBestFire(GetPed());
		Vector3 vTargetPos = pBestFire ? VEC3V_TO_VECTOR3(pBestFire->GetPositionWorld()) : pOrder->GetIncident()->GetLocation();

		RegisterFightingFire(pBestFire);
	
		CAITarget target(vTargetPos);

		CTaskShootAtTarget* pTask = rage_new CTaskShootAtTarget(target, 0, -1.0f);
		pTask->SetFiringPatternHash(FIRING_PATTERN_FULL_AUTO);
		SetNewTask(pTask);
	}
}

CTask::FSM_Return CTaskFirePatrol::SprayFireWithFireExtinguisher_OnUpdate()
{
	CFireOrder* pOrder = HelperGetFireOrder();

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished) || GetSubTask() == NULL )
	{
		if (pOrder && pOrder->GetFireIncident())
		{
			pOrder->SetEntityFinishedWithIncident();
		}
		SetState(State_MakeDecisionHowToEnd);
		return FSM_Continue;
	}

	if (pOrder && pOrder->GetFireIncident())
	{
		CFireIncident* pIncident = pOrder->GetFireIncident();
		if (pIncident->GetNumFires() == 0)
		{
			pOrder->SetEntityFinishedWithIncident();
			SetState(State_MakeDecisionHowToEnd);
			return FSM_Continue;
		}

		CFire* pBestFire = pIncident->GetBestFire(GetPed());
		if (pBestFire)
		{
			const float fDistToFireSqr = MagSquared(GetPed()->GetTransform().GetPosition() - pBestFire->GetPositionWorld()).Getf();
			const float fCompareDist = square(s_fApproachFireDistance + 1.25f);
			if (fDistToFireSqr > fCompareDist)
			{
				if(GetPed()->GetIsInVehicle())
				{
					RegisterFightingFire(pBestFire);
					SetState(State_ExitVehicle);
					return FSM_Continue;
				}
				RegisterFightingFire(pBestFire);
				SetState(State_GoToIncidentLocationOnFoot);
				return FSM_Continue;
			}

			//Update the target position
			CTaskGun* pGunTask = static_cast<CTaskGun*>(FindSubTaskOfType(CTaskTypes::TASK_GUN));
			if (pGunTask)
			{
				CAITarget target(VEC3V_TO_VECTOR3(pBestFire->GetPositionWorld()));
				pGunTask->SetTarget(target);
			}

		}	
		else
		{
			pOrder->SetEntityFinishedWithIncident();
			SetState(State_MakeDecisionHowToEnd);
			return FSM_Continue;
		}
	}
	else if (!pOrder || !pOrder->GetIncident())
	{
		SetState(State_MakeDecisionHowToEnd);
		return FSM_Continue;
	}

	// In this state, we're using fire extinguishers. To reduce the risk of
	// getting into combat, suppress any gunfire events that may otherwise be emitted
	// (arguably, this could maybe also be done through weapon data).
	GetPed()->SetPedResetFlag(CPED_RESET_FLAG_SupressGunfireEvents, true);

	return FSM_Continue;
}

// SprayFireWithFireExtinguisher
void CTaskFirePatrol::SprayFireWithMountedWaterHose_OnEnter()
{
	CFireOrder* pOrder = HelperGetFireOrder();
	if( pOrder ==NULL || !pOrder->GetIsValid() )
	{
		SetState(State_ChoosePatrol);
		return;
	}
	
	CIncident* pIncident = pOrder->GetIncident();
	if( pIncident && pIncident->GetType() == CIncident::IT_Fire )
	{
		CFireIncident* pFireIncident = static_cast<CFireIncident*>(pIncident);
		CFire* pFire = pFireIncident->GetBestFire(GetPed());
		if(pFire)
		{
			CTaskVehicleMountedWeapon* pNewTask = rage_new CTaskVehicleMountedWeapon(CTaskVehicleMountedWeapon::Mode_Fire);
			pNewTask->SetTargetPosition(VEC3V_TO_VECTOR3(pFire->GetPositionWorld()));
			SetNewTask(pNewTask);
		}
	}
}

CTask::FSM_Return CTaskFirePatrol::SprayFireWithMountedWaterHose_OnUpdate()
{
	CFireOrder* pOrder = HelperGetFireOrder();
	if (pOrder && pOrder->GetFireIncident())
	{
		CFireIncident* pIncident = pOrder->GetFireIncident();
		if (pIncident->GetNumFires() == 0)
		{
			pOrder->SetEntityFinishedWithIncident();
			SetState(State_WaitInTruck);
			return FSM_Continue;
		}

		CFire* pBestFire = pIncident->GetBestFire(GetPed());
		if (pBestFire)
		{
			Vector3 pFirePos = VEC3V_TO_VECTOR3(pBestFire->GetPositionWorld());
			if( pFirePos.Dist2(VEC3V_TO_VECTOR3(GetPed()->GetMyVehicle()->GetTransform().GetPosition()))  < rage::square(CVehicleWaterCannon::FIRETRUCK_CANNON_RANGE) )
			{
				if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_VEHICLE_MOUNTED_WEAPON )
				{
					CTaskVehicleMountedWeapon* pMountedWeaponTask = static_cast<CTaskVehicleMountedWeapon*>(GetSubTask());
					pMountedWeaponTask->SetTargetPosition(pFirePos);
					return FSM_Continue;
				}
			}
			//There are fires but the trucks hose can't be used to put them out.
			SetState(State_DriverArriveAtIncident);
			return FSM_Continue;
		}

		SetState(State_MakeDecisionHowToEnd);
		return FSM_Continue;
	}
	else if (!pOrder || !pOrder->GetIncident())
	{
		SetState(State_WaitInTruck);
		return FSM_Continue;
	}

	return FSM_Continue;
}

// ReturnToTruck
void CTaskFirePatrol::ReturnToTruck_OnEnter()
{
	CPed* pPed = GetPed();

	CFireOrder* pOrder = HelperGetFireOrder();

	CVehicle* pVehicle = GetPed()->GetMyVehicle();
	if (!pVehicle && pOrder)
	{
		pVehicle = pOrder->GetFireTruck();
	}

	if (pVehicle)
	{
		//if we have a valid order, hurry up 
		float moveSpeed = MOVEBLENDRATIO_WALK; 
		if((pOrder && pOrder->GetBestFire(GetPed())))
		{
			moveSpeed = MOVEBLENDRATIO_RUN;
		}
		VehicleEnterExitFlags vehicleFlags;
		vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DontJackAnyone);
		
		bool bRandomSeat = true;
		s32 iSeat = 0;
		
		if(m_bWantsToBeDriver)
		{
			iSeat = 0;
			bRandomSeat = false;
		}
		else
		{
			//There isn't a driver, check if someone else has reserved the seat.
			CComponentReservation* pComponentReservation = pVehicle->GetComponentReservationMgr()->GetSeatReservation(0, SA_directAccessSeat);
			if (pComponentReservation && !pComponentReservation->IsComponentInUse())
			{
				iSeat = 0;
				bRandomSeat = false;
			}
		}

		CTaskEnterVehicle* pTask = NULL;
		if (!bRandomSeat)
		{
			pTask = rage_new CTaskEnterVehicle(pVehicle, SR_Prefer, iSeat, vehicleFlags, 0.0f, moveSpeed);
		}
		else
		{
			pTask = rage_new CTaskEnterVehicle(pVehicle, SR_Any, -1, vehicleFlags, 0.0f, moveSpeed);
		}
		
		SetNewTask(pTask);
	}

	//Put away our fire extinguishers
	GetPed()->GetWeaponManager()->EquipWeapon(pPed->GetDefaultUnarmedWeaponHash());
}

CTask::FSM_Return CTaskFirePatrol::ReturnToTruck_OnUpdate()
{
	CFireOrder* pOrder = HelperGetFireOrder();
	if (!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		if(!GetPed()->GetIsInVehicle())
		{
			SetState(State_PatrolOnFoot);
		}

		if(m_bWantsToBeDriver)
		{
			m_bWantsToBeDriver = false;
			SetState(State_GoToIncidentLocationInVehicle);
		}
		
		if (pOrder)
		{
			pOrder->SetEntityFinishedWithIncident();
		}

		SetState(State_WaitInTruck);
		return FSM_Continue;
	}

	//Keep checking if there is another fire to respond to
	if(pOrder && !m_bWantsToBeDriver)
	{
		pOrder->SetEntityArrivedAtIncident();

		CFire* pFire = pOrder->GetBestFire(GetPed());
		if( pFire)
		{
			if(GetPed()->GetIsInVehicle())
			{
				SetState(State_ExitVehicle);
				return FSM_Continue;
			}

			SetState(State_GoToIncidentLocationOnFoot);
			return FSM_Continue;
		}
	}

	return FSM_Continue;
}

void CTaskFirePatrol::ExitVehicle_OnEnter()
{
	CVehicle* pVehicle = GetPed()->GetMyVehicle();
	Assertf(pVehicle, "CTaskFirePatrol::ExitVehicle_OnEnter--Valid vehicle expected!");

	VehicleEnterExitFlags vehicleFlags;
	vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DelayForTime);
	vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::WarpIfDoorBlocked);
	vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DontCloseDoor);
	CTaskExitVehicle* pTask = rage_new CTaskExitVehicle(pVehicle, vehicleFlags, fwRandom::GetRandomNumberInRange(0.0f, 1.0f));
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskFirePatrol::ExitVehicle_OnUpdate()
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished) || GetSubTask() == NULL )
	{
		if(GetPed()->GetIsInVehicle())
		{
			SetFlag(aiTaskFlags::RestartCurrentState);
			return FSM_Continue;
		}

		//If flag is set then we are taking over from the driver
		if(m_bWantsToBeDriver)
		{
			SetState(State_ReturnToTruck);
			return FSM_Continue;
		}

		//the only time we want to consider investigating an expired fire is if we arrive
		//after all the fires have gone out.  if we got there, fought some fires, put them out,
		//we don't want to do the investigation.  so do the logic once, here, and just query
		//this value when we're evaluating state transitions
		//lets just get the driver to do this
		CPed* pPed = GetPed();
		CFireOrder* pOrder = HelperGetFireOrder();
		if (pOrder && 
			pOrder->GetEntityThatHasBeenOnFire() && 
			pOrder->GetNumFires() == 0 && 
			pPed &&
			pPed->GetMyVehicle() &&
			pPed->GetMyVehicle()->GetSeatManager()->GetLastPedInSeat(0) == pPed	)
		{
			m_bShouldInvestigateExpiredFire = true;
		}

		if( pOrder && pOrder->GetIsValid() )
		{
			CFire* pBestFire = pOrder->GetBestFire(GetPed());
			if (pBestFire || m_bShouldInvestigateExpiredFire)
			{
				SetState(State_GoToIncidentLocationOnFoot);
				return FSM_Continue;
			}
		}

		if( pOrder )
		{
			pOrder->SetEntityFinishedWithIncident();
		}

		SetState(State_MakeDecisionHowToEnd);
		return FSM_Continue;
	}
	return FSM_Continue;
}

//	WaitInTruck
void CTaskFirePatrol::WaitInTruck_OnEnter()
{

}

CTask::FSM_Return CTaskFirePatrol::WaitInTruck_OnUpdate()
{
	CPed* pPed = GetPed();
	CFireOrder* pOrder = HelperGetFireOrder();

	//Make sure if we waiting we set we're finished with the order once all the fires are out
	if ( pOrder && pOrder->GetNumFires() == 0)
	{
		pOrder->SetEntityFinishedWithIncident();
	}

	if (!pOrder || pOrder->GetAllResourcesAreDone())
	{
		//If you're the driver wait for everyone to get in the vehicle before you patrol
		if(pPed->GetIsDrivingVehicle())
		{
			CVehicle* pVehicle = GetPed()->GetMyVehicle();
			for ( int i = 1; i < pVehicle->GetSeatManager()->GetMaxSeats(); i++)
			{
				CPed* pLastPed = pVehicle->GetSeatManager()->GetLastPedInSeat(i);
				if (pLastPed && !pLastPed->GetIsInVehicle() )
				{
					CTaskFirePatrol * pTaskFirePatrol = (CTaskFirePatrol*)pLastPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_FIRE_PATROL);
					if(pTaskFirePatrol && pTaskFirePatrol->GetState() != CTaskFirePatrol::State_PatrolOnFoot)
					{
						return FSM_Continue;
					}
				}
			}
		}

		m_bCantPathOnFoot = false;
		SetState(State_ChoosePatrol);
		return FSM_Continue;
	}

	if(pOrder && !m_bCantPathOnFoot)
	{
		if(pPed->GetMyVehicle())
		{
			CFire* pFire = pOrder->GetBestFire(GetPed());
			if( pFire)
			{
				CPed* pDriver = pPed->GetMyVehicle()->GetDriver();
				if(pDriver && pDriver->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_VEHICLE_MOUNTED_WEAPON))
				{
					return FSM_Continue;
				}

				if(pPed->GetIsInVehicle())
				{
					RegisterFightingFire(pFire);
					SetState(State_ExitVehicle);
					return FSM_Continue;
				}

				SetState(State_GoToIncidentLocationOnFoot);
				return FSM_Continue;
			}
		}
	}

	return FSM_Continue;
}

void CTaskFirePatrol::MakeDecisionHowToEnd_OnEnter	()
{

}

CTask::FSM_Return CTaskFirePatrol::MakeDecisionHowToEnd_OnUpdate()
{
	//decide whether or not to patrol on foot or return to vehicle
	//things we care about: do we have an incident to respond to or not,
	//	-do we have a vehicle or not?  is it closeby?
	//this is the kind of task that we could use some sort of reasoner for,
	//but there are only two variables so a couple if statements should get the
	//job done -JM
	CFireOrder* pOrder = HelperGetFireOrder();
	CVehicle* pVehicle = GetPed()->GetMyVehicle();
	if (!pVehicle && pOrder)
	{
		pVehicle = pOrder->GetFireTruck();
	}

	//a lil hacky
	if (pVehicle && !pVehicle->IsInDriveableState())
	{
		pVehicle = NULL;
	}

	CIncident* pIncident = pOrder ? pOrder->GetIncident() : NULL;
	const bool bHasValidFire = pOrder ? pOrder->GetNumFires() > 0 : false;
	const bool bHasValidFireForPed = pOrder && pOrder->GetBestFire(GetPed()) ? true : false;

	if (!pVehicle && !pIncident)
	{
		SetState(State_ChoosePatrol);
		return FSM_Continue;
	}
	else if (!pIncident && pVehicle)
	{
		SetState(State_ReturnToTruck);
		return FSM_Continue;
	}
	else if (pIncident && !pVehicle)
	{
		if(bHasValidFireForPed)
		{
			SetState(State_RespondToOrder);
		}
		else
		{
			SetState(State_ChoosePatrol);
		}
		return FSM_Continue;
	}
	else
	{
		const float fFavorIncidentsRatio = 1.5f;
		const Vector3 vPedPosition = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition());
		const float fDistToIncident = pIncident ? pIncident->GetLocation().Dist(vPedPosition) : LARGE_FLOAT;
		const float fDistToVehicle = pVehicle ? VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition()).Dist(vPedPosition) : LARGE_FLOAT;
		const bool bPreferWalking = fDistToIncident < 25.0f || (fDistToIncident / fDistToVehicle) <= fFavorIncidentsRatio;

		if (bPreferWalking && bHasValidFire && bHasValidFireForPed)
		{
			SetState(State_RespondToOrder);
		}
		else
		{
			SetState(State_ReturnToTruck);
		}

		return FSM_Continue;
	}
}

void CTaskFirePatrol::HandleExpiredFire_OnEnter()
{
	CFireOrder* pOrder = HelperGetFireOrder();
	if (pOrder)
	{
		CEntity* pEntity = pOrder->GetEntityThatHasBeenOnFire();
		Vector3 vTargetPos = pEntity ? VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition()) : pOrder->GetIncident()->GetLocation();

		CAITarget target(vTargetPos);
		CTaskShootAtTarget* pTask = rage_new CTaskShootAtTarget(target, 0, 5.0f);
		pTask->SetFiringPatternHash(FIRING_PATTERN_FULL_AUTO);
		SetNewTask(pTask);
	}
}

CTask::FSM_Return CTaskFirePatrol::HandleExpiredFire_OnUpdate()
{
	CFireOrder* pOrder = HelperGetFireOrder();
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished) || GetSubTask() == NULL )
	{
		pOrder->SetEntityFinishedWithIncident();
		SetState(State_MakeDecisionHowToEnd);
		return FSM_Continue;
	}

	if (!pOrder || !pOrder->GetIncident())
	{
		SetState(State_MakeDecisionHowToEnd);
		return FSM_Continue;
	}
	else if (pOrder && pOrder->GetNumFires() > 0)
	{
		//if we got a new fire during this time, go deal with it but only if someone else isn't
		CFire* pBestFire = pOrder->GetBestFire(GetPed());
		if (pBestFire)
		{
			SetState(State_GoToIncidentLocationOnFoot);
			return FSM_Continue;
		}
	}

	// In this state, we're using fire extinguishers. To reduce the risk of
	// getting into combat, suppress any gunfire events that may otherwise be emitted
	// (arguably, this could maybe also be done through weapon data).
	GetPed()->SetPedResetFlag(CPED_RESET_FLAG_SupressGunfireEvents, true);

	return FSM_Continue;
}

CFireOrder* CTaskFirePatrol::HelperGetFireOrder() 
{
	Assert(GetPed());
	Assert(GetPed()->GetPedIntelligence());
	COrder* pOrder = GetPed()->GetPedIntelligence()->GetOrder();
	if (pOrder && pOrder->GetType() == COrder::OT_Fire && pOrder->GetIsValid() && pOrder != m_pIgnoreOrder)
	{
		return static_cast<CFireOrder*>(pOrder);
	}

	return NULL;
}

const CFireOrder* CTaskFirePatrol::HelperGetFireOrder() const
{
	Assert(GetPed());
	Assert(GetPed()->GetPedIntelligence());
	const COrder* pOrder = GetPed()->GetPedIntelligence()->GetOrder();
	if (pOrder && pOrder->GetType() == COrder::OT_Fire && pOrder->GetIsValid() && pOrder != m_pIgnoreOrder)
	{
		return static_cast<const CFireOrder*>(pOrder);
	}

	return NULL;
}

void CTaskFirePatrol::UpdateKeepFiresAlive()
{
	//fires tend to go out pretty quickly, so keep
	//any fires alive that make sense to keep alive.
	//right now that means only vehicles.  if we ever get
	//some sort of "flammability" property for map textures,
	//we could see about looking up into that and making appropriate
	//map fires stay alive, but right now those tend to
	//look unrealistic

	const CFireOrder* pOrder = HelperGetFireOrder();
	if (pOrder)
	{
		CFireIncident* pIncident = pOrder->GetFireIncident();
		if (pIncident)
		{
			for (int i=0; i < pIncident->GetNumFires(); i++)
			{
				CFire* pFire = pIncident->GetFire(i);
				if (pFire)
				{
					FireType_e fireType = pFire->GetFireType();
					if (fireType == FIRETYPE_TIMED_VEH_WRECKED
						|| fireType == FIRETYPE_TIMED_VEH_GENERIC
						|| fireType == FIRETYPE_TIMED_OBJ_GENERIC)
					{
						pFire->RegisterInUseByDispatch();
					}

					CEntity* pEntity = pFire->GetEntity();
					if ( pEntity && pEntity->GetIsTypeVehicle() )
					{
						CVehicle* pVehicle = static_cast<CVehicle*>(pEntity);
						pVehicle->m_nVehicleFlags.bForceOtherVehiclesToStopForThisVehicle = true;
					}
				}
			}
		}
	}
}

bool CTaskFirePatrol::EvaluateUsingMountedWaterHose(CFireOrder* pOrder)
{
	//Use the turret system if possible
	if( taskVerifyf(pOrder->GetIncident(), "GoToIncidentLocationInVehicle_OnUpdate: Null Incident!" ) )
	{
		if( GetPed()->GetMyVehicle() && GetPed()->GetMyVehicle()->InheritsFromAutomobile() )
		{
			if( pOrder->GetIncident()->GetLocation().Dist2(VEC3V_TO_VECTOR3(GetPed()->GetMyVehicle()->GetTransform().GetPosition()))  < rage::square(CVehicleWaterCannon::FIRETRUCK_CANNON_RANGE) )
			{
				if( pOrder->GetIncident()->GetType() == CIncident::IT_Fire )
				{
					if ( pOrder->GetNumFires() < 6 && fwRandom::GetRandomNumberInRange(0, 100) < 75)
					{
						taskAssertf( dynamic_cast<CFireIncident*>(pOrder->GetIncident()), "Incident of unexpected type!" );
						CFireIncident* pFireIncident = static_cast<CFireIncident*>(pOrder->GetIncident());
						if ( pFireIncident->GetFireIncidentRadius() < 18.0f)
						{
							CFire* pFire = pFireIncident->GetBestFire(GetPed());
							if( pFire )
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

void CTaskFirePatrol::SetFiremanEventResponse()
{
	if(GetPed() && GetPed()->GetPedIntelligence())
	{
		const u32 FIREMAN = ATSTRINGHASH("FIREMAN", 0x0fc2ca767);
		if (GetPed()->GetPedIntelligence()->GetDecisionMakerId() != FIREMAN)
		{
			m_PreviousDecisionMarkerId = GetPed()->GetPedIntelligence()->GetDecisionMakerId();
			GetPed()->GetPedIntelligence()->SetDecisionMakerId(FIREMAN);
		}
	}
}

void CTaskFirePatrol::ResetDefaultEventResponse()
{
	if( m_PreviousDecisionMarkerId != INVALID_DECISION_MARKER )
	{
		GetPed()->GetPedIntelligence()->SetDecisionMakerId(m_PreviousDecisionMarkerId);
		m_PreviousDecisionMarkerId = INVALID_DECISION_MARKER;
	}
}

void CTaskFirePatrol::RegisterFightingFire(CFire* pFire)
{
	CFireOrder* pOrder = HelperGetFireOrder();
	if (pOrder && pFire != m_pCurrentFireDontDereferenceMe)
	{
		pOrder->DecFightingFireCount(m_pCurrentFireDontDereferenceMe);
		pOrder->IncFightingFireCount(pFire, GetPed());
		m_pCurrentFireDontDereferenceMe = pFire;
	}
}

void CTaskFirePatrol::DeregisterFightingFire()
{
	CFireOrder* pOrder = HelperGetFireOrder();
	if (pOrder)
	{
		pOrder->DecFightingFireCount(m_pCurrentFireDontDereferenceMe);
	}

	m_pCurrentFireDontDereferenceMe = NULL;
}


void CTaskFirePatrol::SetScenarioPoint(CVehicle* pVehicle, CScenarioPoint& pScenarioPoint)
{
	aiAssert(pVehicle);

	Vector3 vScenarioCenterPos = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
	vScenarioCenterPos +=  VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetForward()) * 6.0f;

	pScenarioPoint.Reset();

	float fRadAngle = (TWO_PI / 4) * fwRandom::GetRandomNumberInRange(0.0f, 4.0f);
	float fRadius = 2.0f;
	Vector3 vScenarioPointPos = vScenarioCenterPos + Vector3( -rage::Cosf(fRadAngle),rage::Sinf(fRadAngle),0.0f) * fRadius;
	pScenarioPoint.SetPosition(VECTOR3_TO_VEC3V(vScenarioPointPos));

	Vector3 vDirection = vScenarioPointPos - vScenarioCenterPos;
	//If there is still an order then look forwards that whilst in the scenario
	CFireOrder* pOrder = HelperGetFireOrder();
	if (pOrder && pOrder->GetFireIncident())
	{
		vDirection = vScenarioPointPos - pOrder->GetFireIncident()->GetLocation();
	}
	vDirection.Normalize();
	pScenarioPoint.SetDirection(rage::Atan2f(vDirection.x, -vDirection.y));

	//query the scenarioinfo
	static const atHashWithStringNotFinal tendToPedName("WORLD_HUMAN_SMOKING",0xCD02F9C4);
	s32 iScenarioIndex = SCENARIOINFOMGR.GetScenarioIndex(tendToPedName);

	// Set the scenario type in the CSpawnPoint, as it would otherwise be zero from
	// the m_ScenarioPoint.Reset() above. It's probably not used much, but it may at least
	// affect some error messages if something goes wrong, which would otherwise be
	// confusing if they just used the scenario name from type 0.
	pScenarioPoint.SetScenarioType((unsigned)iScenarioIndex);
}

void CTaskFirePatrol::CacheScenarioPointToReturnTo()
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
		// user info. That means that TaskFirePatrol may have a pointer to deleted
		// data, and if we start up a CTaskUnalerted again when resuming, we would
		// be in a bad spot.
		tUA->SetScenarioChainUserInfo(NULL);
	}
}

void CTaskFirePatrol::AttemptToCreateResumableUnalertedTask()
{
	//Check if we have no unalerted task.
	if(!m_pUnalertedTask)
	{
		//Check if we have a point to return to.
		if(m_pPointToReturnTo)
		{
			CPed* pPed = GetPed();

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
}
