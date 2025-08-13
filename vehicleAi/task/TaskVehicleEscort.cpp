#include "TaskVehicleEscort.h"

#include "Vehicles/vehicle.h"
#include "VehicleAi\task\TaskVehiclePark.h"
#include "VehicleAi\task\TaskVehicleTempAction.h"
#include "vehicleAi\VehicleIntelligence.h"
#include "Peds\ped.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

const float CTaskVehicleEscort::ms_fStraightLineDistDefault = sVehicleMissionParams::DEFAULT_SWITCH_TO_STRAIGHT_LINE_DIST;

//////////////////////////////////////////////////////////////////////////////////

CTaskVehicleEscort::CTaskVehicleEscort(const sVehicleMissionParams& params, const VehicleEscortType escortType, const float fCustomOffset
	, const int iMinHeightAboveTerrain, const float fStraightLineDistance) :
CTaskVehicleMissionBase(params),
m_EscortType(escortType),
m_fCustomOffset(fCustomOffset),
m_nNextTimeAllowedToMove(0),
m_iMinHeightAboveTerrain(iMinHeightAboveTerrain),
m_fStraightLineDist(fStraightLineDistance)
{
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_ESCORT);
}


//////////////////////////////////////////////////////////////////////////////////

CTaskVehicleEscort::~CTaskVehicleEscort()
{

}

//////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskVehicleEscort::ProcessPreFSM()
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.
	if (pVehicle)
	{
		pVehicle->m_nVehicleFlags.bTasksAllowDummying = true;
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskVehicleEscort::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.

	FSM_Begin
		// Escort state
		FSM_State(State_Escort)
			FSM_OnUpdate
				return Escort_OnUpdate(pVehicle);

		// EscortAutomobile state
		FSM_State(State_EscortAutomobile)
			FSM_OnEnter
				EscortAutomobile_OnEnter(pVehicle);
			FSM_OnUpdate
				return EscortAutomobile_OnUpdate(pVehicle);

		// EscortHelicopter state
		FSM_State(State_EscortHelicopter)
			FSM_OnEnter
				EscortHelicopter_OnEnter(pVehicle);
			FSM_OnUpdate
				return EscortHelicopter_OnUpdate(pVehicle);

		// TapBrake state
		FSM_State(State_TapBrake)
			FSM_OnEnter
				TapBrake_OnEnter(pVehicle);
			FSM_OnUpdate
				return TapBrake_OnUpdate(pVehicle);

		// Brake state
		FSM_State(State_Brake)
			FSM_OnEnter
				Brake_OnEnter(pVehicle);
			FSM_OnUpdate
				return Brake_OnUpdate(pVehicle);

		// Stop state
		FSM_State(State_Stop)
			FSM_OnEnter
				Stop_OnEnter(pVehicle);
			FSM_OnUpdate
				return Stop_OnUpdate(pVehicle);
		
	FSM_End
}

#if __BANK
void CTaskVehicleEscort::Debug() const
{
	const CVehicle *pVehicle = GetVehicle(); 

	if (!pVehicle)
	{
		return;
	}

	Vector3 vDebugTargetCoors;
	FindTargetCoors(pVehicle, vDebugTargetCoors);
	AdjustTargetPositionDependingOnDistance(pVehicle, vDebugTargetCoors);

	grcDebugDraw::Line(pVehicle->GetTransform().GetPosition(), VECTOR3_TO_VEC3V(vDebugTargetCoors), Color_aquamarine);
	grcDebugDraw::Cross(VECTOR3_TO_VEC3V(vDebugTargetCoors), 0.5f, Color_aquamarine);	
}
#endif //__BANK

/////////////////////////////////////////////////////////////////////////////////
// State_Escort
/////////////////////////////////////////////////////////////////////////////////
aiTask::FSM_Return	CTaskVehicleEscort::Escort_OnUpdate				(CVehicle* pVehicle)
{
	// Sometimes the target may have been removed for a frame or so.
	// In that case don't do anything.
	if(!GetTargetEntity())
	{
		SetState(State_Stop);
		return FSM_Continue;
	}

	if(pVehicle->InheritsFromHeli())
	{
		SetState(State_EscortHelicopter);
	}
	else
	{
		SetState(State_EscortAutomobile);
	}

	UpdateAvoidanceCache();

	return FSM_Continue;

}

//////////////////////////////////////////////////////////////////////////////////

void	CTaskVehicleEscort::EscortHelicopter_OnEnter	(CVehicle* pVehicle)
{
	if (m_EscortType == VEHICLE_ESCORT_HELI)
	{
		sVehicleMissionParams params = m_Params;
		params.m_fTargetArriveDist = 0.0f;
		params.ClearTargetEntity();

		SetNewTask( rage_new CTaskVehicleGoToHelicopter(params));
	}
	else
	{
		FindTargetCoors(pVehicle, m_Params);

		if(GetTargetEntity() && GetTargetEntity()->GetIsTypePed())
		{
			CPed *pPed =(CPed *)GetTargetEntity();
			CVehicle* pVeh = pPed->GetVehiclePedInside();
			if(pVeh)
			{
				SetTargetEntity(pVeh);
			}
		}

		UpdateAvoidanceCache();

		// Sometimes the target may have been removed for a frame or so.
		// In that case don't do anything.
		if(!GetTargetEntity())
		{
			SetState(State_Stop);
			return;
		}

		Vector3 vecForward(VEC3V_TO_VECTOR3(GetTargetEntity()->GetTransform().GetB()));
		sVehicleMissionParams params = m_Params;
		params.m_fTargetArriveDist = 0.0f;
		SetNewTask( rage_new CTaskVehicleGoToHelicopter(params,
													CTaskVehicleGoToHelicopter::HF_AttainRequestedOrientation,
													fwAngle::GetATanOfXY(vecForward.x, vecForward.y),
													m_iMinHeightAboveTerrain));
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : ControlAIVeh_TowardsPointInEscort
// PURPOSE :  Gets this car to drive to a point relative to a car to be escorted.
/////////////////////////////////////////////////////////////////////////////////
aiTask::FSM_Return	CTaskVehicleEscort::EscortHelicopter_OnUpdate	(CVehicle* pVehicle)
{
	if (m_EscortType == VEHICLE_ESCORT_HELI)
	{
		if(GetTargetEntity() && GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_VEHICLE_GOTO_HELICOPTER)
		{
			if (GetTargetEntity()->GetIsTypePed())
			{
				CPed *pPed = (CPed *)GetTargetEntity();
				CVehicle* pVeh = pPed->GetVehiclePedInside();
				if (pVeh)
				{
					SetTargetEntity(pVeh);
				}
			}

			//generate world position
			Vector3 vTargetVehiclePos = VEC3V_TO_VECTOR3(GetTargetEntity()->GetTransform().GetPosition());

			Vector3 vHeading = GetTargetEntity()->GetVelocity();
			vHeading.Normalize();

			if (GetTargetEntity()->GetIsTypeVehicle())
			{
				CVehicle* pTargetVehicle = (CVehicle*)GetTargetEntity();

				aiTask* pTheirTask = pTargetVehicle->GetIntelligence()->GetTaskManager()->FindTaskByTypeActive(VEHICLE_TASK_TREE_PRIMARY, CTaskTypes::TASK_VEHICLE_GOTO_HELICOPTER);
				if (pTheirTask)
				{
					CTaskVehicleMissionBase* pBase = (CTaskVehicleMissionBase*)pTheirTask;
					const Vector3* pvTargetVehicleTargetPos = pBase->GetTargetPosition();

					Vector3 vToTarget = *pvTargetVehicleTargetPos - vTargetVehiclePos;
					vToTarget.z = 0.0f;
					vToTarget.Normalize();

					vHeading = vToTarget;
				}
			}

			Vector3 vRight(vHeading.y, -vHeading.x, vHeading.z);
			Vector3 vOffset = m_Params.GetTargetPosition();

			Vector3 vWorldTarget = vTargetVehiclePos;
			vWorldTarget += vRight * vOffset.x;
			vWorldTarget += vHeading * vOffset.y;
			vWorldTarget.z = vTargetVehiclePos.z + vOffset.z;

			CTaskVehicleGoToHelicopter *pHeliTask = static_cast<CTaskVehicleGoToHelicopter*>(GetSubTask());
			pHeliTask->SetTargetPosition(&vWorldTarget);
			pHeliTask->SetCruiseSpeed(50.0f); //We have moving target so I don't think it matters what speed we pass in
		}
	}
	else
	{
		Assertf(pVehicle, "ControlAIVeh_TowardsPointInEscort expected a valid set vehicle.");

		Vector3	SpeedVector;
		Vector3	Side, PlayerSpeed, Front;
		if(GetTargetEntity() && GetTargetEntity()->GetIsTypePed())
		{
			CPed *pPed =(CPed *)GetTargetEntity();
			CVehicle* pVeh = pPed->GetVehiclePedInside();
			if(pVeh)
			{
				SetTargetEntity(pVeh);
			}
		}

		// Sometimes the target may have been removed for a frame or so.
		// In that case don't do anything.
		if(!GetTargetEntity())
		{
			SetState(State_Stop);
			return FSM_Continue;
		}

	#if __DEV
		if(CVehicleIntelligence::m_bDisplayCarAiDebugInfo && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVehicle))
		{
			grcDebugDraw::Line(m_Params.GetTargetPosition() - Vector3(1.0f,0.0f,0.0f),m_Params.GetTargetPosition() + Vector3(1.0f,0.0f,0.0f), Color32(1.0f, 1.0f, 1.0f, 1.0f));
			grcDebugDraw::Line(m_Params.GetTargetPosition() - Vector3(0.0f,1.0f,0.0f),m_Params.GetTargetPosition() + Vector3(0.0f,1.0f,0.0f), Color32(1.0f, 1.0f, 1.0f, 1.0f));
			grcDebugDraw::Line(m_Params.GetTargetPosition() - Vector3(0.0f,0.0f,1.0f),m_Params.GetTargetPosition() + Vector3(0.0f,0.0f,1.0f), Color32(1.0f, 1.0f, 1.0f, 1.0f));
		}
	#endif

		Assert(pVehicle->InheritsFromHeli());

		FindTargetCoors(pVehicle, m_Params);

		// If we are a helicopter we fly to the appropriate coordinates whilst maintaining the heading of our target.		
		//set the target position to follow
		if(GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_VEHICLE_GOTO_HELICOPTER)
		{	
			CTaskVehicleGoToHelicopter *pHeliTask = static_cast<CTaskVehicleGoToHelicopter*>(GetSubTask());

			pHeliTask->SetTargetPosition(&m_Params.GetTargetPosition());
			Vector3 vecForward(VEC3V_TO_VECTOR3(GetTargetEntity()->GetTransform().GetB()));
			pHeliTask->SetOrientation(fwAngle::GetATanOfXY(vecForward.x, vecForward.y));
			pHeliTask->SetOrientationRequested(true);
			pHeliTask->SetCruiseSpeed(GetCruiseSpeed());
		}
	}

	return FSM_Continue;
}


//////////////////////////////////////////////////////////////////////////////////
static dev_float DISTANCE_TO_HEAD_TO_TARGET_POSITION = 15.0f;

void	CTaskVehicleEscort::EscortAutomobile_OnEnter	(CVehicle*  pVehicle)
{
	if(GetTargetEntity() && GetTargetEntity()->GetIsTypePed())
	{
		CPed *pPed =(CPed *)GetTargetEntity();
		CVehicle* pVeh = pPed->GetVehiclePedInside();
		if(pVeh)
		{
			SetTargetEntity(pVeh);
		}
	}

	UpdateAvoidanceCache();

	FindTargetCoors(pVehicle, m_Params);

	// Sometimes the target may have been removed for a frame or so.
	// In that case don't do anything.
	if(!GetTargetEntity())
	{
		SetState(State_Stop);
		return;
	}

	// Adjust the target position so we approach the target relatively late
	AdjustTargetPositionDependingOnDistance(pVehicle, m_Params);

	//create a goto task we will update every frame
	sVehicleMissionParams params = m_Params;
	params.m_iDrivingFlags = params.m_iDrivingFlags|DF_TargetPositionOverridesEntity|DF_ExtendRouteWithWanderResults;

	//don't avoid our target entity if we're meant to be following from behind
	if (m_EscortType == VEHICLE_ESCORT_REAR)
	{
		params.m_iDrivingFlags = params.m_iDrivingFlags|DF_DontAvoidTarget;
	}
	params.m_fTargetArriveDist = 0.0f;

	SetNewTask(CVehicleIntelligence::CreateAutomobileGotoTask(params, m_fStraightLineDist));
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : ControlAIVeh_TowardsPointInEscort
// PURPOSE :  Gets this car to drive to a point relative to a car to be escorted.
/////////////////////////////////////////////////////////////////////////////////
aiTask::FSM_Return	CTaskVehicleEscort::EscortAutomobile_OnUpdate	(CVehicle* pVehicle)
{
	Assert(!pVehicle->InheritsFromHeli());
	Assertf(pVehicle, "ControlAIVeh_TowardsPointInEscort expected a valid set vehicle.");

	// Sometimes the target may have been removed for a frame or so.
	// In that case don't do anything.
	if(!GetTargetEntity())
	{
		SetState(State_Stop);
		return FSM_Continue;
	}

	FindTargetCoors(pVehicle, m_Params);
	
	// Adjust the target position so we approach the target offset relatively late
	Vector3 vEndTargetPosition = m_Params.GetTargetPosition();
	AdjustTargetPositionDependingOnDistance(pVehicle, m_Params);

	Vector3	SpeedVector;
	Vector3	Side, PlayerSpeed, Front;

#if __DEV
	if(CVehicleIntelligence::m_bDisplayCarAiDebugInfo && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVehicle))
	{
		grcDebugDraw::Line(m_Params.GetTargetPosition() - Vector3(1.0f,0.0f,0.0f),m_Params.GetTargetPosition() + Vector3(1.0f,0.0f,0.0f), Color32(1.0f, 1.0f, 1.0f, 1.0f));
		grcDebugDraw::Line(m_Params.GetTargetPosition() - Vector3(0.0f,1.0f,0.0f),m_Params.GetTargetPosition() + Vector3(0.0f,1.0f,0.0f), Color32(1.0f, 1.0f, 1.0f, 1.0f));
		grcDebugDraw::Line(m_Params.GetTargetPosition() - Vector3(0.0f,0.0f,1.0f),m_Params.GetTargetPosition() + Vector3(0.0f,0.0f,1.0f), Color32(1.0f, 1.0f, 1.0f, 1.0f));
	}
#endif

	pVehicle->SetHandBrake(false);

	float	fDesiredSpeed = GetCruiseSpeed();
	HandleBrakingIfAheadOfTarget(pVehicle, GetTargetEntity(), vEndTargetPosition, IsDrivingFlagSet(DF_DriveInReverse), true, m_nNextTimeAllowedToMove, fDesiredSpeed);

	// set the target for the goto task
    if(GetSubTask() && GetSubTask()->GetTaskType()==CVehicleIntelligence::GetAutomobileGotoTaskType())
    {
        CTaskVehicleMissionBase * pGoToTask = (CTaskVehicleMissionBase*)GetSubTask();
        pGoToTask->SetTargetPosition(&m_Params.GetTargetPosition());
        pGoToTask->SetCruiseSpeed(fDesiredSpeed);
    }

	return FSM_Continue;
}

void CTaskVehicleEscort::HandleBrakingIfAheadOfTarget(const CVehicle* pVehicle, const CPhysical* pTargetEntity, const Vector3& vTargetPos, const bool bDriveInReverse, const bool bAllowMinorSlowdown, u32& nNextTimeAllowedToMove, float& fDesiredSpeed)
{
	const float fOriginalCruiseSpeed = fDesiredSpeed;

	Assert(pTargetEntity);

	// Get the current drive direction and orientation.
	const Vector3 vehDriveDir = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleDriveDir(pVehicle, bDriveInReverse));

	float	DistToTargetSqr, PastTarget, targetSpeed;

	const Vector3 vVehiclePosition = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleBonnetPosition(pVehicle, bDriveInReverse));
	// If the target is just behind us we wait. If the target is way behind us we turn round.
	PastTarget =(vTargetPos.x - vVehiclePosition.x) * vehDriveDir.x +(vTargetPos.y - vVehiclePosition.y) * vehDriveDir.y;
	DistToTargetSqr = (vTargetPos.x - vVehiclePosition.x)*(vTargetPos.x - vVehiclePosition.x) +
		(vTargetPos.y - vVehiclePosition.y)*(vTargetPos.y - vVehiclePosition.y);

	static dev_float THRESHOLD = 1.0f;
	static dev_float THRESHOLD2 = -1.5f;

	static dev_float MATCH_SPEED_DISTANCE = 15.0f;
	static dev_float SLOW_DOWN_DISTANCE = 25.0f;

	if (fwTimer::GetGameTimer().GetTimeInMilliseconds() < nNextTimeAllowedToMove)
	{
		fDesiredSpeed = 0.0f;
	}
	else if(PastTarget > THRESHOLD)
	{	// Target is ahead of us. Fine. Slow down a bit if we get close.
		targetSpeed =(pTargetEntity->GetVelocity()).Mag();
		if (bAllowMinorSlowdown)
		{
			if(DistToTargetSqr < MATCH_SPEED_DISTANCE * MATCH_SPEED_DISTANCE)
			{
				fDesiredSpeed = rage::Max(targetSpeed + rage::Min(4.0f, PastTarget-THRESHOLD-0.1f), rage::Sqrtf(DistToTargetSqr) * 3.0f);
				fDesiredSpeed = rage::Min(fDesiredSpeed, fOriginalCruiseSpeed);
			}
			else if(DistToTargetSqr < SLOW_DOWN_DISTANCE * SLOW_DOWN_DISTANCE)
			{
				fDesiredSpeed = rage::Min(fDesiredSpeed, targetSpeed + 10.0f);
			}
		}	
	}
	else
	{
		static dev_float s_fDistBeforeTurnAroundSqr = 50.0f * 50.0f;
		const float fDriveInReverseDirMultiplier = bDriveInReverse ? -1.0f : 1.0f;
		if(DistToTargetSqr < s_fDistBeforeTurnAroundSqr 
			&& (DotProduct(VEC3V_TO_VECTOR3(pTargetEntity->GetTransform().GetB()), (fDriveInReverseDirMultiplier * VEC3V_TO_VECTOR3(pVehicle->GetVehicleForwardDirection()))) > 0.0f))
		{
			// Target not too far behind us. Wait for it to catch up.(Only do this if the target is facing roughly the same direction)
			//pVeh->SetSteerAngle(desiredSteerAngle);
			//pVeh->SetThrottle(0.0f);
			if(PastTarget < THRESHOLD2)
			{		
				// Way ahead of the target
				//SetState(State_Brake);

				nNextTimeAllowedToMove = fwTimer::GetGameTimer().GetTimeInMilliseconds() + 500;
				fDesiredSpeed = 0.0f;
			}
			else
			{	
				// Just a bit ahead of the target
				//pVeh->SetBrake(0.1f + 0.9f *((THRESHOLD - PastTarget) /(THRESHOLD - THRESHOLD2)));
				//SetState(State_TapBrake);//should probably bdo this a bit nicer but this should work for now.

				nNextTimeAllowedToMove = fwTimer::GetGameTimer().GetTimeInMilliseconds() + 100;
				fDesiredSpeed = rage::Min(fDesiredSpeed, (pTargetEntity->GetVelocity()).Mag() - 10.0f);
				fDesiredSpeed = rage::Max(fDesiredSpeed, 0.0f);
			}
			return;
		}
		else
		{
			// Target far behind us. Turn round(slowly)
			fDesiredSpeed = 8.0f;	// Some 30 km/h
		}
	}
}


/////////////////////////////////////////////////////////////////////////////////

void CTaskVehicleEscort::TapBrake_OnEnter(CVehicle* UNUSED_PARAM(pVehicle))
{
	SetNewTask( rage_new CTaskVehicleBrake(NetworkInterface::GetSyncedTimeInMilliseconds() + 100) );//create a short tempaction brake
}

/////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskVehicleEscort::TapBrake_OnUpdate(CVehicle* UNUSED_PARAM(pVehicle))
{
	if (GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_BRAKE))
	{
		SetState(State_EscortAutomobile);		//finished braking so now head for target again.
	}

	return FSM_Continue;
}

/////////////////////////////////////////////////////////////////////////////////
// Brake state
/////////////////////////////////////////////////////////////////////////////////
void CTaskVehicleEscort::Brake_OnEnter(CVehicle* UNUSED_PARAM(pVehicle))
{
	SetNewTask( rage_new CTaskVehicleBrake(NetworkInterface::GetSyncedTimeInMilliseconds() + 500));//create a long tempaction brake
}

/////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskVehicleEscort::Brake_OnUpdate(CVehicle* UNUSED_PARAM(pVehicle))
{
	if (GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_BRAKE))
	{
		SetState(State_EscortAutomobile);		//finished braking so now head for target again.
	}

	return FSM_Continue;
}



/////////////////////////////////////////////////////////////////////////////////
// State_Stop
/////////////////////////////////////////////////////////////////////////////////
void CTaskVehicleEscort::Stop_OnEnter	(CVehicle* UNUSED_PARAM(pVehicle))
{
	SetNewTask(rage_new CTaskVehicleStop());
}

/////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehicleEscort::Stop_OnUpdate	(CVehicle* UNUSED_PARAM(pVehicle))
{
	if (GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_STOP))
	{
		return FSM_Quit;
	}
	return FSM_Continue;
}

void CTaskVehicleEscort::CleanUp()
{
	//Clear the avoidance cache.
	if (GetVehicle())
	{
		GetVehicle()->GetIntelligence()->ClearAvoidanceCache();
	}
}

const CVehicleFollowRouteHelper* CTaskVehicleEscort::GetFollowRouteHelper() const
{
// 	CTaskVehicleGoToAutomobileNew* pGotoTask = static_cast<CTaskVehicleGoToAutomobileNew*>(FindSubTaskOfType(CTaskTypes::TASK_VEHICLE_GOTO_AUTOMOBILE_NEW));
// 	if (pGotoTask)
// 	{
// 		//find the follow route of the guy we're following
// 		if (pGotoTask->IsDoingStraightLineNavigation())
// 		{
// 			CVehicle* pTargetVehicle = NULL;
// 			const CPhysical* pTargetEntity = GetTargetEntity();
// 			if (pTargetEntity)
// 			{
// 				if (pTargetEntity->GetIsTypeVehicle())
// 				{
// 					pTargetVehicle = (CVehicle*)pTargetEntity;
// 				}
// 				else if (pTargetEntity->GetIsTypePed())
// 				{
// 					CPed* pTargetPed = (CPed*)pTargetEntity;
// 					if (pTargetPed->GetVehiclePedInside())
// 					{
// 						pTargetVehicle = pTargetPed->GetVehiclePedInside();
// 					}
// 				}
// 
// 				if (pTargetVehicle)
// 				{
// 					return pTargetVehicle->GetIntelligence()->GetFollowRouteHelper();
// 				}
// 			}
// 		}
// 		else
// 		{
// 			return pGotoTask->GetFollowRouteHelper();
// 		}
// 	}

	//this will allow the intelligence to search our next task for a follow route
	return NULL;
}

void CTaskVehicleEscort::UpdateAvoidanceCache()
{
	Vector3 vTargetCoors;
	const CVehicle* pVehicle = GetVehicle();
	
	if (GetTargetEntity()->GetIsTypeVehicle())
	{
		const CVehicle* pTargetVehicle = static_cast<const CVehicle*>(GetTargetEntity());
		FindTargetCoors(pVehicle, vTargetCoors);
		Vector3 vTargetCoorsLocalSpace = VEC3V_TO_VECTOR3(pTargetVehicle->GetTransform().UnTransform(VECTOR3_TO_VEC3V(vTargetCoors)));

		CVehicleIntelligence::AvoidanceCache& rCache = pVehicle->GetIntelligence()->GetAvoidanceCache();
		rCache.m_pTarget = pTargetVehicle;
		rCache.m_fDesiredOffset = -vTargetCoorsLocalSpace.y;
	}
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindTargetCoors
// PURPOSE :  Finds the target coors for this mission. If there is an entity
//			  it will be those coordinates. If there isn't it will be the actual
//			  coordinates.
// RETURNS :
/////////////////////////////////////////////////////////////////////////////////
static dev_float ESCORT_FORWARD_DIST		=	5.0f;
static dev_float ESCORT_SIDE_DIST			=	2.0f;
static dev_float ESCORT_SIDE_DIST_BIKE	    =	1.4f;
static dev_float ESCORT_SIDE_DIST_HELI	    =	15.0f;
static dev_float ESCORT_UP_DIST_HELI		=	8.0f;

void CTaskVehicleEscort::FindTargetCoors(const CVehicle *pVeh, sVehicleMissionParams &params) const
{
	Vector3 TargetCoors;
	FindTargetCoors(pVeh, TargetCoors);
	params.SetTargetPosition(TargetCoors);
}

void CTaskVehicleEscort::FindTargetCoors(const CVehicle *pVeh, Vector3 &TargetCoors) const
{
	if (GetTargetEntity())
	{
		TargetCoors = VEC3V_TO_VECTOR3(GetTargetEntity()->GetTransform().GetPosition());
		const CPhysical *pTargetEntity = FindVehicleWithPhysical(GetTargetEntity());
		float		SpeedMult = 1.0f;

		switch(m_EscortType)
		{
		case VEHICLE_ESCORT_LEFT:
			{
				float escortSideDist, escortUpDist = 0.0f;

				if (m_fCustomOffset >= 0.0f)
				{
					escortSideDist = m_fCustomOffset;
				}
				else
				{
					switch(pVeh->GetVehicleType())
					{
					case VEHICLE_TYPE_BIKE:
						escortSideDist = ESCORT_SIDE_DIST_BIKE;
						break;
					case VEHICLE_TYPE_HELI:
					case VEHICLE_TYPE_BLIMP:
					case VEHICLE_TYPE_AUTOGYRO:
						escortSideDist = ESCORT_SIDE_DIST_HELI;
						break;
					default:
						escortSideDist = ESCORT_SIDE_DIST;
						break;
					}
				}

				if (pVeh->GetVehicleType() == VEHICLE_TYPE_AUTOGYRO)
				{
					escortUpDist = ESCORT_UP_DIST_HELI;
				}

				float extraDist = 0.0f;
				if (pVeh->GetVehicleType() == VEHICLE_TYPE_BIKE && pTargetEntity->GetIsTypeVehicle())
				{
					static dev_float fudge = 1.0f;
					extraDist = rage::Max(0.0f, ( fudge * ((CVehicle *)pTargetEntity)->GetSteerAngle() ) );
				}

				Vector3 flatA = VEC3V_TO_VECTOR3(pTargetEntity->GetTransform().GetA());
				flatA.z = 0.0f;
				flatA.Normalize();
				Vector3 flatB = VEC3V_TO_VECTOR3(pTargetEntity->GetTransform().GetB());
				flatB.z = 0.0f;
				flatB.Normalize();

				//assuming TargetCoors is the root position of the vehicle
				const Vector3 vVehiclePos = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleBonnetPosition(pVeh, IsDrivingFlagSet(DF_DriveInReverse)));
				const float fDistanceToTarget = TargetCoors.Dist(vVehiclePos);

				const float fDesiredOffsetA = -(pVeh->GetBaseModelInfo()->GetBoundingBoxMax().x - pTargetEntity->GetBaseModelInfo()->GetBoundingBoxMin().x + escortSideDist + extraDist);
				const Vector3 vDesiredOffsetA = flatA * fDesiredOffsetA;
				Vector3 vFinalDesiredOffsetA = vDesiredOffsetA;
				if (pVeh->GetVehicleType() == VEHICLE_TYPE_BIKE
					&& fDistanceToTarget < DISTANCE_TO_HEAD_TO_TARGET_POSITION)
				{
					//if we're a bike escorting to the side, do a bit of lerping along
					//the target x axis. otherwise there can be a jump in steer angle
					//the first frame after a bike stops avoiding something
					const float fActualOffsetA = flatA.Dot(vVehiclePos - TargetCoors);
					//const Vector3 vActualOffsetA = flatA * fDesiredOffsetA;

					if (fActualOffsetA < fDesiredOffsetA)
					{
						const float fFinalOffsetA = Lerp(0.1f, fActualOffsetA, fDesiredOffsetA);
						vFinalDesiredOffsetA = flatA * fFinalOffsetA;
					}	
				}

				TargetCoors += vFinalDesiredOffsetA;
				if (pVeh->GetVehicleType() != VEHICLE_TYPE_BIKE)
				{
					TargetCoors += flatB * 0.25f;
				}
				TargetCoors.z += escortUpDist;
				static dev_float multEscortLeft = 0.25f;
				SpeedMult = multEscortLeft;
			}
			break;
		case VEHICLE_ESCORT_RIGHT:
			{
				float escortSideDist, escortUpDist = 0.0f;

				if (m_fCustomOffset >= 0.0f)
				{
					escortSideDist = m_fCustomOffset;
				}
				else
				{
					switch(pVeh->GetVehicleType())
					{
					case VEHICLE_TYPE_BIKE:
						escortSideDist = ESCORT_SIDE_DIST_BIKE;
						break;
					case VEHICLE_TYPE_HELI:
					case VEHICLE_TYPE_AUTOGYRO:
					case VEHICLE_TYPE_BLIMP:
						escortSideDist = ESCORT_SIDE_DIST_HELI;
						break;
					default:
						escortSideDist = ESCORT_SIDE_DIST;
						break;
					}
				}
				
				if (pVeh->GetVehicleType() == VEHICLE_TYPE_AUTOGYRO)
				{
					escortUpDist = ESCORT_UP_DIST_HELI;
				}

				float extraDist = 0.0f;
				if (pVeh->GetVehicleType() == VEHICLE_TYPE_BIKE && pTargetEntity->GetIsTypeVehicle())
				{
					static dev_float fudge = -1.0f;
					extraDist = rage::Max(0.0f, ( fudge * ((CVehicle *)pTargetEntity)->GetSteerAngle() ) );
				}

				Vector3 flatA = VEC3V_TO_VECTOR3(pTargetEntity->GetTransform().GetA());
				flatA.z = 0.0f;
				flatA.Normalize();

				Vector3 flatB = VEC3V_TO_VECTOR3(pTargetEntity->GetTransform().GetB());
				flatB.z = 0.0f;
				flatB.Normalize();

				//assuming TargetCoors is the root position of the vehicle
				const Vector3 vVehiclePos = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleBonnetPosition(pVeh, IsDrivingFlagSet(DF_DriveInReverse)));
				const float fDistanceToTarget = TargetCoors.Dist(vVehiclePos);

				const float fDesiredOffsetA = (pTargetEntity->GetBaseModelInfo()->GetBoundingBoxMax().x - pVeh->GetBaseModelInfo()->GetBoundingBoxMin().x + escortSideDist + extraDist);
				const Vector3 vDesiredOffsetA = flatA * fDesiredOffsetA;
				Vector3 vFinalDesiredOffsetA = vDesiredOffsetA;
				if (pVeh->GetVehicleType() == VEHICLE_TYPE_BIKE
					&& fDistanceToTarget < DISTANCE_TO_HEAD_TO_TARGET_POSITION)
				{
					//if we're a bike escorting to the side, do a bit of lerping along
					//the target x axis. otherwise there can be a jump in steer angle
					//the first frame after a bike stops avoiding something
					const float fActualOffsetA = flatA.Dot(vVehiclePos - TargetCoors);
					//const Vector3 vActualOffsetA = flatA * fDesiredOffsetA;

					if (fActualOffsetA > fDesiredOffsetA)
					{
						const float fFinalOffsetA = Lerp(0.1f, fActualOffsetA, fDesiredOffsetA);
						vFinalDesiredOffsetA = flatA * fFinalOffsetA;
						//vFinalDesiredOffsetA = Lerp(0.1f, vActualOffsetA, vDesiredOffsetA);
					}	
				}

				TargetCoors += vFinalDesiredOffsetA;
				if (pVeh->GetVehicleType() != VEHICLE_TYPE_BIKE)
				{
					TargetCoors += flatB * 0.25f;
				}
				TargetCoors.z += escortUpDist;
				static dev_float multEscortRight = 0.25f;
				SpeedMult = multEscortRight;
			}
			break;
		case VEHICLE_ESCORT_REAR:
			{
				// With escort rear we aim for the coordinates of the car. This fixes the problem of the
				// target point swinging erratically when the target vehicle changes direction.
				const float fForwardDist = m_fCustomOffset >= 0.0f ? m_fCustomOffset : ESCORT_FORWARD_DIST;
				float	DistRequested = pVeh->GetBaseModelInfo()->GetBoundingBoxMax().y - pTargetEntity->GetBaseModelInfo()->GetBoundingBoxMin().y + fForwardDist;

				// Half the distance is based on the targets orientation.
				Vector3 TargetCoors1 = TargetCoors - VEC3V_TO_VECTOR3(pTargetEntity->GetTransform().GetB()) * DistRequested;

				const Vector3 vVehPosition = VEC3V_TO_VECTOR3(pVeh->GetTransform().GetPosition());
				float	CurrentDist = (vVehPosition - TargetCoors).Mag();
				Vector3	TargetCoors2 = TargetCoors + (vVehPosition - TargetCoors) * rage::Min(1.0f, (DistRequested / CurrentDist));

				static dev_float tweak = 0.435f;
				TargetCoors = tweak * TargetCoors1 + (1.0f - tweak) * TargetCoors2;
				static dev_float multEscortRear = 0.4f;
				static dev_float multEscortRearBike = 0.2f;
				if (pVeh->GetVehicleType() == VEHICLE_TYPE_BIKE)
				{
					SpeedMult = multEscortRearBike;
				}
				else
				{
					SpeedMult = multEscortRear;
				}
			}
			break;
		case VEHICLE_ESCORT_FRONT:
		{
			Vector3 vecForward(VEC3V_TO_VECTOR3(pTargetEntity->GetTransform().GetB()));
			const float fForwardDist = m_fCustomOffset >= 0.0f ? m_fCustomOffset : ESCORT_FORWARD_DIST;
			TargetCoors += vecForward * (pTargetEntity->GetBaseModelInfo()->GetBoundingBoxMax().y - pVeh->GetBaseModelInfo()->GetBoundingBoxMin().y + fForwardDist);
			TargetCoors += vecForward * 1.0f;
			SpeedMult = 0.5f;
		}
			break;
		default:
			break;
		}

		TargetCoors += SpeedMult * GetTargetEntity()->GetVelocity();
	}
	else
	{
		TargetCoors = m_Params.GetTargetPosition();
	}
	
	//E1 fix to make things look less mechanical
	switch(m_EscortType)
	{
	case VEHICLE_ESCORT_LEFT:
	case VEHICLE_ESCORT_RIGHT:
	case VEHICLE_ESCORT_REAR:
		{
			if (pVeh->GetVehicleType() == VEHICLE_TYPE_BIKE)
			{
				static float angles[16] = {1.0f, 4.0f, 3.0f, 0.3f, 0.4f, 5.0f, 2.0f, 4.0f, 1.5f, 2.5f, 3.7f, 6.0f, 2.0f, 1.0f, 4.0f, 4.5f};
				int RandIndex = ( (fwTimer::GetTimeInMilliseconds()+pVeh->GetRandomSeed()) / 10000) & 15;

				TargetCoors.x += 0.2f * rage::Sinf(angles[RandIndex]);
				TargetCoors.y += 0.2f * rage::Cosf(angles[RandIndex]);
			}
		}
	default:
		break;
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : InConvoyTogether()
// PURPOSE :  Processes one sector pedList.
// RETURNS :  True if we are in a convoy together.
/////////////////////////////////////////////////////////////////////////////////
bool CTaskVehicleEscort::InConvoyTogether(const CVehicle* const UNUSED_PARAM(pVeh), const CVehicle* const pOtherVeh)
{
	if(FindVehicleWithPhysical(GetTargetEntity()) == pOtherVeh)
	{
		return true;
	}

	return false;
}

void CTaskVehicleEscort::CloneUpdate(CVehicle* pVehicle)
{
    CTaskVehicleMissionBase::CloneUpdate(pVehicle);

    pVehicle->m_nVehicleFlags.bTasksAllowDummying = true;
}

void CTaskVehicleEscort::AdjustTargetPositionDependingOnDistance(const CVehicle* pVeh, sVehicleMissionParams& params) const
{
	Vector3 vTargetPosition = params.GetTargetPosition();
	AdjustTargetPositionDependingOnDistance(pVeh, vTargetPosition);
	params.SetTargetPosition(vTargetPosition);
}

void CTaskVehicleEscort::AdjustTargetPositionDependingOnDistance(const CVehicle* pVeh, Vector3& vTargetPosition) const
{
	const Vector3 vTargetVehicleCoords = VEC3V_TO_VECTOR3(GetTargetEntity()->GetTransform().GetPosition());
	const Vector3 vOurVehicleCoords = VEC3V_TO_VECTOR3(pVeh->GetTransform().GetPosition());
	float fDistanceToTarget = vTargetVehicleCoords.Dist(vOurVehicleCoords);

	//if we're trying to escort from the side, and we go in front of the target,
	//dont start lerping toward the center again
	if (m_EscortType == VEHICLE_ESCORT_LEFT || m_EscortType == VEHICLE_ESCORT_RIGHT)
	{
		const Vector3 vTargetDriveDir = VEC3V_TO_VECTOR3(GetTargetEntity()->GetTransform().GetForward());
		const float fDistanceInFrontOfTarget = vTargetDriveDir.Dot(vOurVehicleCoords - vTargetVehicleCoords);
		if (fDistanceInFrontOfTarget > 0.0f)
		{
			fDistanceToTarget = 0.0f;
		}
	}

	//we always want to aim for the front position when escorting from the front
	if (m_EscortType == VEHICLE_ESCORT_FRONT)
	{
		//TODO:  smooth out target position so when target turns we don't sway left or right
	}
	else if( fDistanceToTarget > DISTANCE_TO_HEAD_TO_TARGET_POSITION)
	{
		vTargetPosition = vTargetVehicleCoords;
	}
	else
	{
		float fToTargetPerc = (fDistanceToTarget/DISTANCE_TO_HEAD_TO_TARGET_POSITION);
		vTargetPosition = (vTargetPosition*(1.0f - fToTargetPerc)) + (vTargetVehicleCoords*(fToTargetPerc));
	}
}
//////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
const char * CTaskVehicleEscort::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_Escort&&iState<=State_Stop);
	static const char* aStateNames[] = 
	{
		"State_Escort",
		"State_EscortAutomobile",
		"State_EscortHelicopter",
		"State_TapBrake",
		"State_Brake",
		"State_Stop",
	};

	return aStateNames[iState];
}

#endif
