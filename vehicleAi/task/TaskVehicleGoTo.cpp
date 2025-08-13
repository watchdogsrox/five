#include "TaskVehicleGoTo.h"

// Game headers
#include "VehicleAi\task\TaskVehicleCruise.h"
#include "VehicleAi\task\TaskVehicleGoToAutomobile.h"
#include "VehicleAi\task\TaskVehicleGoToHelicopter.h"
#include "VehicleAi\task\TaskVehicleGoToPlane.h"
#include "VehicleAi\task\TaskVehicleGoToSubmarine.h"
#include "VehicleAi\task\TaskVehiclePark.h"
#include "VehicleAi\VehicleIntelligence.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////

CTaskVehicleGoTo::CTaskVehicleGoTo(const sVehicleMissionParams& params) :
CTaskVehicleMissionBase(params)
{
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_GOTO);
}


////////////////////////////////////////////////////////////////////

CTaskVehicleGoTo::~CTaskVehicleGoTo()
{

}

////////////////////////////////////////////////////////////////////

CTaskVehicleFollow::CTaskVehicleFollow(const sVehicleMissionParams& params,	const int FollowDistance) 
	: CTaskVehicleMissionBase(params)
	, m_fHeliRequestedOrientation(-1.0f)
	, m_iFlightHeight(7)
	, m_iMinHeightAboveTerrain(20)
	, m_iHeliFlags(0)
{
	m_iFollowCarDistance = FollowDistance;
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_FOLLOW);
}

CTaskVehicleFollow::CTaskVehicleFollow(const CTaskVehicleFollow& in_rhs)
	: CTaskVehicleMissionBase(in_rhs.m_Params)
	, m_iFollowCarDistance(in_rhs.m_iFollowCarDistance)
	, m_fHeliRequestedOrientation(in_rhs.m_fHeliRequestedOrientation)
	, m_iFlightHeight(in_rhs.m_iFlightHeight)
	, m_iMinHeightAboveTerrain(in_rhs.m_iMinHeightAboveTerrain)
	, m_iHeliFlags(in_rhs.m_iHeliFlags)
{
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_FOLLOW);
}

void CTaskVehicleFollow::SetHelicopterSpecificParams( float fHeliRequestedOrientation, int iFlightHeight, int iMinHeightAboveTerrain, s32 iHeliFlags )
{
	m_fHeliRequestedOrientation = fHeliRequestedOrientation;
	m_iFlightHeight				= iFlightHeight; 
	m_iMinHeightAboveTerrain	= iMinHeightAboveTerrain;
	m_iHeliFlags				= iHeliFlags;
}


void CTaskVehicleFollow::CleanUp()
{
	//Clear the avoidance cache.
	if (GetVehicle())
	{
		GetVehicle()->GetIntelligence()->ClearAvoidanceCache();
	}
}

void CTaskVehicleFollow::UpdateAvoidanceCache()
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

/////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskVehicleFollow::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.

	FSM_Begin
		// 
		FSM_State(State_Follow)
			FSM_OnUpdate
				return Follow_OnUpdate(pVehicle);
		//
		FSM_State(State_FollowAutomobile)
			FSM_OnEnter
				FollowAutomobile_OnEnter(pVehicle);
			FSM_OnUpdate
				return FollowAutomobile_OnUpdate(pVehicle);
		//
		FSM_State(State_FollowHelicopter)
			FSM_OnEnter
				FollowHelicopter_OnEnter(pVehicle);
			FSM_OnUpdate
				return FollowHelicopter_OnUpdate(pVehicle);

		//
		FSM_State(State_FollowSubmarine)
			FSM_OnEnter
				FollowSubmarine_OnEnter(pVehicle);
			FSM_OnUpdate
				return FollowSubmarine_OnUpdate(pVehicle);

		//
		FSM_State(State_Stop)
			FSM_OnEnter
				Stop_OnEnter(pVehicle);
			FSM_OnUpdate
				return Stop_OnUpdate(pVehicle);

	FSM_End
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : State_Follow
// PURPOSE :  
/////////////////////////////////////////////////////////////////////////////////
aiTask::FSM_Return		CTaskVehicleFollow::Follow_OnUpdate				(CVehicle* pVehicle)
{
	//make sure there is still a target to block
	if (!GetTargetEntity())
	{
		SetState(State_Stop);
		return FSM_Continue;
	}

	UpdateAvoidanceCache();

	switch(pVehicle->GetVehicleType())
	{
	case VEHICLE_TYPE_PLANE:
		Assert(0);
		SetState(State_Stop);
		break;
	case VEHICLE_TYPE_HELI:
	case VEHICLE_TYPE_BLIMP:
		SetState(State_FollowHelicopter);
		break;
	case VEHICLE_TYPE_SUBMARINE:
		SetState(State_FollowSubmarine);
		break;
	default:
		SetState(State_FollowAutomobile);
	}

	return FSM_Continue;
}


/////////////////////////////////////////////////////////////////////////////////
// State_FollowAutomobile
/////////////////////////////////////////////////////////////////////////////////
void		CTaskVehicleFollow::FollowAutomobile_OnEnter	(CVehicle* UNUSED_PARAM(pVehicle))
{

	sVehicleMissionParams params = m_Params;
	params.m_fTargetArriveDist = 0.0f;

	SetNewTask(CVehicleIntelligence::CreateAutomobileGotoTask(params, m_iFollowCarDistance + 1.0f));
}

/////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehicleFollow::FollowAutomobile_OnUpdate	(CVehicle* pVehicle)
{
	//make sure there is still a target to block
	if (!GetTargetEntity())
	{
		SetState(State_Stop);
		return FSM_Continue;
	}

	// If we're close to the target we take the gas down.
	float	desiredSpeed =  pVehicle->GetVelocity().XYMag();

	const Vector3 vTargetEntityPosition = VEC3V_TO_VECTOR3(GetTargetEntity()->GetTransform().GetPosition());
	float 	DistToTarget =(VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition()) - vTargetEntityPosition).Mag();
	if(DistToTarget < m_iFollowCarDistance + 10.0f)
	{
		float	HisSpeed =((CPhysical *)GetTargetEntity())->GetVelocity().XYMag();	
		float	DistDiff = DistToTarget - m_iFollowCarDistance;

		if(DistDiff < 0.0f)
		{	// We're too close. Slow down.
			desiredSpeed = rage::Max(HisSpeed + DistDiff * 5.0f, 0.0f);
		}
		else
		{
			desiredSpeed = HisSpeed + DistDiff * 2.0f;
		}
	}
	else
	{
		desiredSpeed = GetCruiseSpeed();
	}

	TUNE_GROUP_FLOAT( CAR_FOLLOW_AI, CHEAT_SPEED_INCREASE_MAX_DISTANCE, 40.0f, 0.0f, 100.0f, 0.1f );
	TUNE_GROUP_FLOAT( CAR_FOLLOW_AI, CHEAT_SPEED_INCREASE_MIN_DISTANCE, 20.0f, 0.0f, 100.0f, 0.1f );
	TUNE_GROUP_FLOAT( CAR_FOLLOW_AI, CHEAT_SPEED_INCREASE_MAX_PERC, 5.0f, 0.0f, 100.0f, 0.1f );
	TUNE_GROUP_FLOAT( CAR_FOLLOW_AI, CHEAT_SPEED_INCREASE_MIN_PERC, 1.0f, 0.0f, 100.0f, 0.1f );

	// Cheat increase in speed to help AI cars catch up with the player.
	float fCheatSpeedIncrease = Clamp((DistToTarget - CHEAT_SPEED_INCREASE_MIN_DISTANCE)/(CHEAT_SPEED_INCREASE_MAX_DISTANCE-CHEAT_SPEED_INCREASE_MIN_DISTANCE), 0.0f, 1.0f);
	fCheatSpeedIncrease = Lerp(fCheatSpeedIncrease, CHEAT_SPEED_INCREASE_MIN_PERC, CHEAT_SPEED_INCREASE_MAX_PERC);
	pVehicle->SetCheatPowerIncrease(fCheatSpeedIncrease);

	if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_VEHICLE_GOTO_AUTOMOBILE_NEW)
	{
		CTaskVehicleGoToAutomobileNew * pGoToTask = (CTaskVehicleGoToAutomobileNew*)GetSubTask();

		SetTargetPosition(&vTargetEntityPosition);

		UpdateAvoidanceCache();

		pGoToTask->SetTargetPosition(&m_Params.GetTargetPosition());
		pGoToTask->SetCruiseSpeed(desiredSpeed);
	}


	Assert(pVehicle->GetSteerAngle() > -1.0f && pVehicle->GetSteerAngle() < 1.0f);

	return FSM_Continue;
}

/////////////////////////////////////////////////////////////////////////////////
// State_FollowHelicopter
/////////////////////////////////////////////////////////////////////////////////
void		CTaskVehicleFollow::FollowHelicopter_OnEnter	(CVehicle* UNUSED_PARAM(pVehicle))
{
	Vector3 v = VEC3V_TO_VECTOR3(GetTargetEntity()->GetTransform().GetPosition());

	sVehicleMissionParams params;
	params.m_fCruiseSpeed = GetCruiseSpeed();
	params.SetTargetPosition(v);
	params.m_fTargetArriveDist = 0.0f;
	SetNewTask(rage_new CTaskVehicleGoToHelicopter(params, m_iHeliFlags, m_fHeliRequestedOrientation, m_iMinHeightAboveTerrain));
}

/////////////////////////////////////////////////////////////////////////////////

void CTaskVehicleFollow::ModifyTargetForArrivalTolerance(Vector3 &io_targetPos, const CVehicle& in_Vehicle ) const
{
	Vector3 vPosition = VEC3V_TO_VECTOR3(in_Vehicle.GetTransform().GetPosition());
	Vector3 vTargetToVehicle = vPosition - io_targetPos;
	float fDistanceXY = vTargetToVehicle.XYMag();
	if ( fDistanceXY > FLT_MIN )
	{
		vTargetToVehicle.z = 0.0f;
		Vector3 vTargetToVehicleDirectionXY;
		vTargetToVehicleDirectionXY.Normalize(vTargetToVehicle);
		static dev_float s_Tune = 0.75f;
		fDistanceXY = Min( fDistanceXY, m_Params.m_fTargetArriveDist * s_Tune);
		io_targetPos = io_targetPos + vTargetToVehicleDirectionXY * fDistanceXY;
	}
}
 
aiTask::FSM_Return	CTaskVehicleFollow::FollowHelicopter_OnUpdate(CVehicle* pVehicle)
{
	//make sure there is still a target to block
	if (!GetTargetEntity())
	{
		SetState(State_Stop);
		return FSM_Continue;
	}

	//set the target position to follow
	if(GetSubTask())
	{	
		CTaskVehicleMissionBase *pCarTask = static_cast<CTaskVehicleMissionBase*>(GetSubTask());

		Vector3 targetPos = VEC3V_TO_VECTOR3(GetTargetEntity()->GetTransform().GetPosition());
		ModifyTargetForArrivalTolerance(targetPos, *pVehicle);
		

		SetTargetPosition(&targetPos);

		UpdateAvoidanceCache();

		pCarTask->SetTargetPosition(&m_Params.GetTargetPosition());
	}

	return FSM_Continue;
}

/////////////////////////////////////////////////////////////////////////////////
// State_FollowSubmarine
/////////////////////////////////////////////////////////////////////////////////
void		CTaskVehicleFollow::FollowSubmarine_OnEnter		(CVehicle* UNUSED_PARAM(pVehicle))
{
	Vector3 v = VEC3V_TO_VECTOR3(GetTargetEntity()->GetTransform().GetPosition());

	sVehicleMissionParams params = m_Params;
	params.SetTargetPosition(v);
	params.ClearTargetEntity();

	SetNewTask(rage_new CTaskVehicleGoToSubmarine(params));

}

/////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehicleFollow::FollowSubmarine_OnUpdate	(CVehicle* UNUSED_PARAM(pVehicle))
{
	//make sure there is still a target to block
	if (!GetTargetEntity())
	{
		SetState(State_Stop);
		return FSM_Continue;
	}

	//set the target position to follow
	if(GetSubTask())
	{	
		CTaskVehicleMissionBase *pCarTask = static_cast<CTaskVehicleMissionBase*>(GetSubTask());

		Vector3 targetPos = VEC3V_TO_VECTOR3(GetTargetEntity()->GetTransform().GetPosition());
		SetTargetPosition(&targetPos);

		UpdateAvoidanceCache();

		pCarTask->SetTargetPosition(&m_Params.GetTargetPosition());
	}

	return FSM_Continue;
}

/////////////////////////////////////////////////////////////////////////////////

void		CTaskVehicleFollow::Stop_OnEnter				(CVehicle* UNUSED_PARAM(pVehicle))
{
	SetNewTask(rage_new CTaskVehicleStop());
}


/////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehicleFollow::Stop_OnUpdate				(CVehicle* UNUSED_PARAM(pVehicle))
{
	if (GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_STOP))
	{
		return FSM_Quit;
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////

#if !__FINAL
const char * CTaskVehicleFollow::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_Follow&&iState<=State_Stop);
	static const char* aStateNames[] = 
	{
		"State_Follow",
		"State_FollowAutomobile",
		"State_FollowHelicopter",
		"State_FollowSubmarine",
		"State_Stop",
	};

	return aStateNames[iState];
}
#endif
