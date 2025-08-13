//
// boat Cruise
// April 24, 2013
//

#include "VehicleAi/driverpersonality.h"
#include "VehicleAi/task/TaskVehicleCruiseBoat.h"
#include "VehicleAi/task/TaskVehicleGoToBoat.h"
#include "VehicleAi/VehicleIntelligence.h"
#include "Task/System/TaskHelpers.h"
#include "vehicles/vehiclepopulation.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()


CTaskVehicleCruiseBoat::Tunables CTaskVehicleCruiseBoat::sm_Tunables;
IMPLEMENT_VEHICLE_AI_TASK_TUNABLES(CTaskVehicleCruiseBoat, 0x04f845b4);

// Constructor/destructor
CTaskVehicleCruiseBoat::CTaskVehicleCruiseBoat(const sVehicleMissionParams& params, const fwFlags8 uConfigFlags)
	: CTaskVehicleMissionBase(params)
	, m_uConfigFlags(uConfigFlags)
	, m_fPickedCruiseSpeedWithVehModel(-1.0f)
{
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_CRUISE_BOAT);
}

CTaskVehicleCruiseBoat::CTaskVehicleCruiseBoat(const CTaskVehicleCruiseBoat& in_rhs)
	: CTaskVehicleMissionBase(in_rhs.m_Params)
{
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_CRUISE_BOAT);
}

CTaskVehicleCruiseBoat::~CTaskVehicleCruiseBoat()
{

}

aiTask::FSM_Return CTaskVehicleCruiseBoat::ProcessPreFSM()
{
	if ( m_uConfigFlags.IsFlagSet(CF_SpeedFromPopulation)  )
	{
		if(m_fPickedCruiseSpeedWithVehModel < 0.0f)
		{
			m_fPickedCruiseSpeedWithVehModel = CVehiclePopulation::PickCruiseSpeedWithVehModel(GetVehicle(), GetVehicle()->GetModelIndex());
		}

		SetCruiseSpeed(rage::Min(m_fPickedCruiseSpeedWithVehModel
			, CDriverPersonality::FindMaxCruiseSpeed(GetVehicle()->GetDriver(), GetVehicle(), GetVehicle()->GetVehicleType() == VEHICLE_TYPE_BICYCLE)));
	}

	return FSM_Continue;
}

aiTask::FSM_Return CTaskVehicleCruiseBoat::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CVehicle* pVehicle = GetVehicle();

	FSM_Begin
		// FindRoute state
		FSM_State(State_Init)
			FSM_OnUpdate		
				return Init_OnUpdate(*pVehicle);
		FSM_State(State_Cruise)
			FSM_OnEnter
				return Cruise_OnEnter(*pVehicle);
			FSM_OnUpdate		
				return Cruise_OnUpdate(*pVehicle);
	FSM_End
}

void CTaskVehicleCruiseBoat::SetParams(const sVehicleMissionParams& params)
{
	// don't stomp our wander target to 0,0,0 
	const Vector3 vTemp = m_Params.GetTargetPosition();
	m_Params = params;
	m_Params.SetTargetPosition(vTemp);
}

void CTaskVehicleCruiseBoat::SetTargetPosition(const Vector3 *pTargetPosition)
{
	CTaskVehicleMissionBase::SetTargetPosition(pTargetPosition);
}

void CTaskVehicleCruiseBoat::PickCruisePoint(CVehicle& in_Vehicle)
{
	m_fPickPointTimer = sm_Tunables.m_fTimeToPickNewPoint;
	Vec2V vBoatPos = in_Vehicle.GetTransform().GetPosition().GetXY();
	Vec2V vBoatFwd = NormalizeSafe(in_Vehicle.GetTransform().GetForward().GetXY(), Vec2V(0.0f, 1.0f));
	Vec2V vTarget = vBoatPos;
	ScalarV vBoatOrientation = ScalarV(fwAngle::GetATanOfXY(in_Vehicle.GetTransform().GetForward().GetXf(), in_Vehicle.GetTransform().GetForward().GetYf()) * RtoD);


	if ( in_Vehicle.GetIntelligence()->GetBoatAvoidanceHelper()->IsAvoiding() )
	{
		if ( in_Vehicle.GetIntelligence()->GetBoatAvoidanceHelper()->IsBlocked() )
		{
			float fAngle = static_cast<float>(fwRandom::GetRandomNumberInRange(-90, 90)) + 180.0f;
			Vec2V vTargetDir = Rotate(vBoatFwd, ScalarV(fAngle * DtoR));
			vTarget = vBoatPos + vTargetDir * ScalarV(sm_Tunables.m_fDistSearch);
		}
		else
		{
			ScalarV vAvoidanceOrientation = ScalarV(in_Vehicle.GetIntelligence()->GetBoatAvoidanceHelper()->GetAvoidanceOrientation() * RtoD);
			ScalarV vDeltaOrientation = (vAvoidanceOrientation - vBoatOrientation) * ScalarV(1.5f);
			Vec2V vTargetDir = Rotate(Vec2V(1.0f, 0.0f), vBoatOrientation + vDeltaOrientation);		
			vTarget = vBoatPos + vTargetDir * ScalarV(sm_Tunables.m_fDistSearch); 
		}
	}
	else
	{
		float fAngle = static_cast<float>(fwRandom::GetRandomNumberInRange(-30, 30));
		Vec2V vTargetDir = Rotate(vBoatFwd, ScalarV(fAngle * DtoR));
		vTarget = vBoatPos + vTargetDir * ScalarV(sm_Tunables.m_fDistSearch);
	}
	
	const Vector3 vTargetPos(Clamp(vTarget.GetXf(), WORLDLIMITS_XMIN, WORLDLIMITS_XMAX), 
							 Clamp(vTarget.GetYf(), WORLDLIMITS_YMIN, WORLDLIMITS_YMAX),
							 in_Vehicle.GetTransform().GetPosition().GetZf()); 

	m_Params.SetTargetPosition(vTargetPos);
}

aiTask::FSM_Return CTaskVehicleCruiseBoat::Init_OnUpdate(CVehicle& UNUSED_PARAM(in_Vehicle))
{
	SetState(State_Cruise);
	return FSM_Continue;
}

aiTask::FSM_Return CTaskVehicleCruiseBoat::Cruise_OnEnter(CVehicle& in_Vehicle)
{
	PickCruisePoint(in_Vehicle);
	sVehicleMissionParams params = m_Params;
	params.ClearTargetEntity();
	params.m_fTargetArriveDist = -1.0f;
	
	CTaskVehicleGoToBoat::ConfigFlags uConfigFlags = CTaskVehicleGoToBoat::CF_StopAtEnd | CTaskVehicleGoToBoat::CF_StopAtShore 
		| CTaskVehicleGoToBoat::CF_AvoidShore | CTaskVehicleGoToBoat::CF_PreferForward 
		| CTaskVehicleGoToBoat::CF_NeverStop | CTaskVehicleGoToBoat::CF_NeverNavMesh
		| CTaskVehicleGoToBoat::CF_UseWanderRoute; 

	SetNewTask(rage_new CTaskVehicleGoToBoat(params, uConfigFlags));

	return FSM_Continue;
}

aiTask::FSM_Return CTaskVehicleCruiseBoat::Cruise_OnUpdate(CVehicle& in_Vehicle)
{
	CTask* pSubtask = GetSubTask();
	if (pSubtask && pSubtask->GetTaskType() == CTaskTypes::TASK_VEHICLE_GOTO_BOAT )
	{
		CTaskVehicleGoToBoat* pTaskVehicleGoToBoat = static_cast<CTaskVehicleGoToBoat*>(pSubtask);
		if ( pTaskVehicleGoToBoat->GetState() != CTaskVehicleGoToBoat::State_ThreePointTurn 
			&& (!pTaskVehicleGoToBoat->HasValidRoute() || pTaskVehicleGoToBoat->IsNearEndOfRoute()) )
		{
			float scale = 1.0f;
			if (  pTaskVehicleGoToBoat->GetState() == CTaskVehicleGoToBoat::State_PauseForRoadRoute 
				|| in_Vehicle.GetIntelligence()->GetBoatAvoidanceHelper()->IsBlocked() )
			{
				scale = 6.0f;
			}
			// pick a new point much sooner
			m_fPickPointTimer -= GetTimeStep() * scale;
		}
		
		const Vector3 vBoatPos = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleBonnetPosition(&in_Vehicle, IsDrivingFlagSet(DF_DriveInReverse)));
		float distance2 = (m_Params.GetTargetPosition() - vBoatPos).XYMag2();

		if ( m_fPickPointTimer < 0.0f 
			|| distance2 < square(sm_Tunables.m_fDistToPickNewPoint)  )
		{
			PickCruisePoint(in_Vehicle);
		}

		static float s_DistanceToUpdatSubTaskTarget = 5.0f;
		Vector3 vDelta = *pTaskVehicleGoToBoat->GetTargetPosition() - m_Params.GetTargetPosition();
		if ( vDelta.XYMag2() >= square(s_DistanceToUpdatSubTaskTarget) )
		{
			pTaskVehicleGoToBoat->SetTargetPosition(&m_Params.GetTargetPosition());
		}

	}

	return FSM_Continue;
}

#if !__FINAL
const char*  CTaskVehicleCruiseBoat::GetStaticStateName( s32 iState )
{
	Assert(iState>=State_Init&&iState<=State_Cruise);
	static const char* aStateNames[] = 
	{
		"State_Init",
		"State_Cruise",
	};

	return aStateNames[iState];
}

void CTaskVehicleCruiseBoat::Debug() const
{
#if DEBUG_DRAW

	grcDebugDraw::Sphere(m_Params.GetTargetPosition() + ZAXIS, 0.5f, Color_green, true);

#endif
}
#endif //!__FINAL


