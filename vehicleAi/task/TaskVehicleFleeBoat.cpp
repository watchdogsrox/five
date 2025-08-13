//
// boat flee
// April 24, 2013
//

#include "VehicleAi/task/TaskVehicleFleeBoat.h"
#include "VehicleAi/task/TaskVehicleGoToBoat.h"
#include "VehicleAi/VehicleIntelligence.h"
#include "Task/System/TaskHelpers.h"


AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()


CTaskVehicleFleeBoat::Tunables CTaskVehicleFleeBoat::sm_Tunables;
IMPLEMENT_VEHICLE_AI_TASK_TUNABLES(CTaskVehicleFleeBoat, 0x3beba5b5);

// Constructor/destructor
CTaskVehicleFleeBoat::CTaskVehicleFleeBoat(const sVehicleMissionParams& params)
	: CTaskVehicleMissionBase(params)
{
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_FLEE_BOAT);
}

CTaskVehicleFleeBoat::CTaskVehicleFleeBoat(const CTaskVehicleFleeBoat& in_rhs)
	: CTaskVehicleMissionBase(in_rhs.m_Params)
{
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_FLEE_BOAT);
}

CTaskVehicleFleeBoat::~CTaskVehicleFleeBoat()
{

}

aiTask::FSM_Return CTaskVehicleFleeBoat::ProcessPreFSM()
{
	return FSM_Continue;
}

aiTask::FSM_Return CTaskVehicleFleeBoat::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CVehicle* pVehicle = GetVehicle();

	FSM_Begin
		// FindRoute state
		FSM_State(State_Init)
			FSM_OnUpdate		
				return Init_OnUpdate(*pVehicle);

		FSM_State(State_Flee)
			FSM_OnEnter
				Flee_OnEnter(*pVehicle);
			FSM_OnUpdate		
				return Flee_OnUpdate(*pVehicle);
				

	FSM_End
}

aiTask::FSM_Return CTaskVehicleFleeBoat::Init_OnUpdate(CVehicle& UNUSED_PARAM(in_Vehicle))
{
	SetState(State_Flee);
	return FSM_Continue;
}

void CTaskVehicleFleeBoat::Flee_OnEnter(CVehicle& in_Vehicle)
{
	Vector3 vTargetPos = CalculateTargetCoords(in_Vehicle);
	sVehicleMissionParams params = m_Params;
	params.m_iDrivingFlags.ClearFlag(DF_AvoidTargetCoors); 
	params.SetTargetPosition(vTargetPos);
	params.ClearTargetEntity();
	params.m_fTargetArriveDist = -1.0f;

	CTaskVehicleGoToBoat::ConfigFlags uConfigFlags = CTaskVehicleGoToBoat::CF_StopAtEnd | CTaskVehicleGoToBoat::CF_StopAtShore | CTaskVehicleGoToBoat::CF_AvoidShore | CTaskVehicleGoToBoat::CF_PreferForward | CTaskVehicleGoToBoat::CF_NeverStop | CTaskVehicleGoToBoat::CF_NeverNavMesh | CTaskVehicleGoToBoat::CF_UseFleeRoute;	
	
	SetNewTask(rage_new CTaskVehicleGoToBoat(params, uConfigFlags));
}

aiTask::FSM_Return CTaskVehicleFleeBoat::Flee_OnUpdate(CVehicle& in_Vehicle)
{
	CTask* pSubtask = GetSubTask();
	if (pSubtask && pSubtask->GetTaskType() == CTaskTypes::TASK_VEHICLE_GOTO_BOAT )
	{
		CTaskVehicleGoToBoat* pTaskVehicleGoToBoat = static_cast<CTaskVehicleGoToBoat*>(pSubtask);

		if ( pTaskVehicleGoToBoat->GetState() != CTaskVehicleGoToBoat::State_ThreePointTurn )
		{	
			static float s_DistanceToUpdatSubTaskTarget = 5.0f;
			Vector3 vTargetPos = CalculateTargetCoords(in_Vehicle);
			Vector3 vDelta = *pTaskVehicleGoToBoat->GetTargetPosition() - vTargetPos;
			if ( vDelta.XYMag2() >= square(s_DistanceToUpdatSubTaskTarget) )
			{
				pTaskVehicleGoToBoat->SetTargetPosition(&vTargetPos);
			}
		}
	}

	return FSM_Continue;
}

Vector3::Return CTaskVehicleFleeBoat::CalculateTargetCoords(const CVehicle& in_Vehicle) const
{
	Vector3 vTargetPos;
	Vector3 vFleeOrigin;
	FindTargetCoors(&in_Vehicle, vFleeOrigin);
	const Vector3 vVehiclePos = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleBonnetPosition(GetVehicle(), IsDrivingFlagSet(DF_DriveInReverse)));

	Vector3 vDelta = vVehiclePos - vFleeOrigin;
	vDelta.z = 0.0f;

	if ( vDelta.Mag() <= FLT_EPSILON )
	{
		vDelta = VEC3V_TO_VECTOR3(in_Vehicle.GetTransform().GetForward());
	}
	else
	{
		vDelta.Normalize();
	}

	//just set this to some far distance in the opposite direction away from the source
	vTargetPos = vVehiclePos + (vDelta * sm_Tunables.m_FleeDistance);

	//just fly level--MinHeightAboveTerrain and FlightHeight will keep us elevated
	vTargetPos.z = vVehiclePos.z;
	vTargetPos.x = Clamp(vTargetPos.x, WORLDLIMITS_XMIN, WORLDLIMITS_XMAX);
	vTargetPos.y = Clamp(vTargetPos.y, WORLDLIMITS_YMIN, WORLDLIMITS_YMAX);
	vTargetPos.z = Clamp(vTargetPos.z, WORLDLIMITS_ZMIN, WORLDLIMITS_ZMAX);

	return vTargetPos;
}




#if !__FINAL
const char*  CTaskVehicleFleeBoat::GetStaticStateName( s32 iState )
{
	Assert(iState>=State_Init&&iState<=State_Flee);
	static const char* aStateNames[] = 
	{
		"State_Init",
		"State_Flee",
	};

	return aStateNames[iState];
}

void CTaskVehicleFleeBoat::Debug() const
{
#if DEBUG_DRAW

	Vector3 vFleeOrigin;
	FindTargetCoors(GetVehicle(), vFleeOrigin);
	Vector3 vTargetPos = CalculateTargetCoords(*GetVehicle());
	grcDebugDraw::Sphere(vFleeOrigin + ZAXIS, 0.5f, Color_red, true);
	grcDebugDraw::Sphere(vTargetPos + ZAXIS, 0.5f, Color_yellow, true);
	grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(vFleeOrigin + ZAXIS), VECTOR3_TO_VEC3V(vTargetPos + ZAXIS), 1.0f, Color_yellow);

#endif
}
#endif //!__FINAL


