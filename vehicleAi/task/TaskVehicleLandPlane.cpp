//
// TaskVehicleLandPlane
//

#include "Vehicles\Planes.h"
#include "VehicleAi\task\TaskVehicleLandPlane.h"
#include "VehicleAi\task\TaskVehicleGotoPlane.h"
#include "VehicleAi\pathfind.h"
#include "VehicleAi\VehicleIntelligence.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()


IMPLEMENT_VEHICLE_AI_TASK_TUNABLES(CTaskVehicleLandPlane, 0x6051bffd);
CTaskVehicleLandPlane::Tunables CTaskVehicleLandPlane::sm_Tunables;

//
// CTaskVechicleLandPlane
//

#if !__FINAL
const char * CTaskVehicleLandPlane::GetStaticStateName( s32 iState  )
{

#define TASKVEHICLELANDPLANE_STATE(x) #x		
	static const char* aStateNames[] = 
	{
		TASKVEHICLELANDPLANE_STATES
	};
#undef TASKVEHICLELANDPLANE_STATE

	return aStateNames[iState];
}

void CTaskVehicleLandPlane::Debug() const
{
#if DEBUG_DRAW

	const CPlane* pPlane = static_cast<const CPlane*>(GetVehicle());

	Vector3 drawCircle(m_ApproachCircleCenter.GetXf(), m_ApproachCircleCenter.GetYf(), pPlane->GetTransform().GetPosition().GetZf());
	grcDebugDraw::Circle(drawCircle, m_ApproachCircleRadius.Getf(), Color_blue, XAXIS, YAXIS );

#endif //DEBUG_DRAW
	CTask::Debug();
}


#endif // !FINAL


CTaskVehicleLandPlane::CTaskVehicleLandPlane(Vec3V_In vRunWayStart, Vec3V_In vRunWayEnd, bool in_bAllowRetry)
: m_vRunwayStart(vRunWayStart)
, m_vRunwayEnd(vRunWayEnd)
, m_bAllowRetry(in_bAllowRetry)
, m_TimeOnTheGround(0)
{
	Assertf(Dist(m_vRunwayEnd, m_vRunwayStart).Getf() > 0.001f, "Start and end runway position are the same!");
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_LAND_PLANE);
}

CTaskVehicleLandPlane::CTaskVehicleLandPlane()
: m_vRunwayStart(VEC3_ZERO)
, m_vRunwayEnd(VEC3_ZERO)
, m_bAllowRetry(false)
, m_TimeOnTheGround(0)
{
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_LAND_PLANE);
}

CTaskVehicleLandPlane::~CTaskVehicleLandPlane()
{

}

CTask::FSM_Return CTaskVehicleLandPlane::ProcessPreFSM()
{
	CPlane* pPlane = static_cast<CPlane*>(GetVehicle());
	if ( pPlane )
	{	
		bool bPlaneOnGround = !pPlane->IsInAir();
		if ( !bPlaneOnGround )
		{
			if ( !pPlane->IsCollisionLoadedAroundPosition() )
			{
				// use the nose of the plane to determine when to stop descent
				float planebottom = pPlane->GetVehicleModelInfo()->GetBoundingBoxMin().z;
				Vec3V vOffset = pPlane->GetTransform().GetUp() * ScalarV(planebottom);


				Vec3V vPlanePos = pPlane->ComputeNosePosition() + vOffset;
				Vec3V vDeltaPos = vPlanePos - m_vRunwayEnd;

				if ( vDeltaPos.GetZf() <= 0.0f )
				{
					m_TimeOnTheGround = sm_Tunables.m_TimeOnGroundToDrive;
					bPlaneOnGround = true;
				}
			}
		}

		if ( bPlaneOnGround )
		{
			m_TimeOnTheGround += fwTimer::GetTimeStep();
		}
		else
		{
			m_TimeOnTheGround = 0;
		}
	}

	return FSM_Continue;
}


CTask::FSM_Return CTaskVehicleLandPlane::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(kState_Init)
			FSM_OnUpdate
				return Init_OnUpdate();

		FSM_State(kState_CircleRunway)
			FSM_OnEnter
				return CircleRunway_OnEnter();
			FSM_OnUpdate
				return CircleRunway_OnUpdate();

		FSM_State(kState_Descend)
				FSM_OnEnter
				return Descend_OnEnter();
			FSM_OnUpdate
				return Descend_OnUpdate();

		FSM_State(kState_PerformLanding)
				FSM_OnEnter
				return PerformLanding_OnEnter();
			FSM_OnUpdate
				return PerformLanding_OnUpdate();

		FSM_State(kState_TaxiToTargetLocation)
			FSM_OnEnter
				return TaxiToTargetLocation_OnEnter();
			FSM_OnUpdate
				return TaxiToTargetLocation_OnUpdate();

		FSM_State(kState_Exit)
			FSM_OnUpdate
				return Exit_OnUpdate();


	FSM_End
}

CTask::FSM_Return CTaskVehicleLandPlane::Init_OnUpdate()
{
	SetState(kState_CircleRunway);
	return FSM_Continue;
}

CTask::FSM_Return CTaskVehicleLandPlane::CircleRunway_OnEnter()
{
	static float speed = 25.0f;

	CPlane* pPlane = static_cast<CPlane*>(GetVehicle());
	if ( pPlane )
	{
		float fRadius = CTaskVehicleGoToPlane::ComputeTurnRadiusForRoll(pPlane, speed, CTaskVehicleGoToPlane::sm_Tunables.m_maxDesiredAngleRollDegrees/5);

		Vec2V vRunwayStart(m_vRunwayStart.GetIntrin128ConstRef());
		Vec2V vRunwayEnd(m_vRunwayEnd.GetIntrin128ConstRef());
		m_ApproachCircleRadius = ScalarV(fRadius);

		Vec2V vRunWayDir = Normalize(vRunwayEnd - vRunwayStart);

		static float s_RunWayMinHeight = 0.0f;
		static float s_RunwayMaxHeight = 50.0f;
				
		sVehicleMissionParams params;
		params.m_fCruiseSpeed = speed;

		// 
		// use future position because sometimes scenarios have us coming down pretty steeply
		// this prevents us trying to go back up
		//
		static float s_MaxFutureTime = 2.0f;

		Vec3V vPlanePos = pPlane->ComputeNosePosition();
		ScalarV vDist = Dist(vPlanePos, m_vRunwayStart);
		ScalarV vSpeed = ScalarV(pPlane->GetVelocity().XYMag());
		ScalarV vTime = Min(vDist/vSpeed, ScalarV(s_MaxFutureTime));

		Vec3V vFuturePosition = vPlanePos + VECTOR3_TO_VEC3V(pPlane->GetVelocity()) * vTime;
		vFuturePosition.SetZ(Min(vPlanePos.GetZ(), vFuturePosition.GetZ()));
		
		params.SetTargetPosition(VEC3V_TO_VECTOR3(m_vRunwayStart + Vec3V(0.0f, 0.0f, Clamp(vFuturePosition.GetZf() - m_vRunwayStart.GetZf(), s_RunWayMinHeight, s_RunwayMaxHeight))));
		params.m_fTargetArriveDist = 0.0f;
		CTaskVehicleGoToPlane* pTaskVehicleGoToPlane = rage_new CTaskVehicleGoToPlane(params);

		float fFlightDirection = fwAngle::GetATanOfXY(vRunWayDir.GetXf(), vRunWayDir.GetYf());
		pTaskVehicleGoToPlane->SetOrientationRequested(true);
		pTaskVehicleGoToPlane->SetOrientation(fFlightDirection);

		SetNewTask(pTaskVehicleGoToPlane);
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskVehicleLandPlane::CircleRunway_OnUpdate()
{
	static float distthreshold = 25.0f;
	CPlane* pPlane = static_cast<CPlane*>(GetVehicle());
	if ( pPlane )
	{	
		Vec2V vRunwayStart(m_vRunwayStart.GetIntrin128ConstRef());
		Vec2V vRunwayEnd(m_vRunwayEnd.GetIntrin128ConstRef());
		Vec2V vPlanePos(pPlane->GetVehiclePosition().GetIntrin128ConstRef());
		Vec2V vPlaneVel(VECTOR3_TO_VEC3V(pPlane->GetVelocity()).GetIntrin128ConstRef());

		// check to see if we're lined up with the runway
		ScalarV vDistance2 = DistSquared(vRunwayStart, vPlanePos);	
		if ( (vDistance2 < ScalarV(square(distthreshold))).Getb() )
		{
			Vec3V vRunWayDir = Normalize(m_vRunwayEnd - m_vRunwayStart);	
			ScalarV dot = Dot(vRunWayDir, GetVehicle()->GetTransform().GetB());
			if ( (dot > ScalarV(0.9f)).Getb() )
			{
				SetState(kState_Descend);
			}
		}

		if ( pPlane->HasContactWheels() )
		{
			// skip straight to landing
			SetState(kState_PerformLanding);
		}
	}

	if (GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_GOTO_PLANE))
	{
		if ( m_bAllowRetry )
		{
			SetState(kState_Init);
		}
		else
		{
			return FSM_Quit;
		}
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskVehicleLandPlane::Descend_OnEnter()
{
	CPlane* pPlane = static_cast<CPlane*>(GetVehicle());
	if ( pPlane )
	{
		// request deploying the landing gear
		pPlane->GetLandingGear().ControlLandingGear(pPlane, CLandingGear::COMMAND_DEPLOY);

		pPlane->m_nVehicleFlags.bUnbreakableLandingGear = true;
		pPlane->m_nVehicleFlags.bDisableHeightMapAvoidance = true;

		//Vec3V vRunWayDir = Normalize(m_vRunwayEnd - m_vRunwayStart);

		sVehicleMissionParams params;
		params.m_fCruiseSpeed = 20.0f;
		params.SetTargetPosition(VEC3V_TO_VECTOR3(m_vRunwayEnd));
		params.m_fTargetArriveDist = 10.0f;

		static int s_FlightHeight = -5;
		static int s_FlightHeightAboveTerrain = -5;


		Vec3V vPlanePos = pPlane->GetVehiclePosition();
		Vec3V vDelta = m_vRunwayEnd - vPlanePos;
		ScalarV vDistance = Mag(vDelta);
		ScalarV vTime = (vDistance/(ScalarV(2.0f)))/ ScalarV(params.m_fCruiseSpeed);
		ScalarV vRateDescent = Abs(vDelta.GetZ()/ vTime);
		

		CTaskVehicleGoToPlane* pTaskVehicleGoToPlane = rage_new CTaskVehicleGoToPlane(params, s_FlightHeight, s_FlightHeightAboveTerrain);
		pTaskVehicleGoToPlane->SetIgnoreZDeltaForEndCondition(true);
		pTaskVehicleGoToPlane->SetVirtualSpeedControlMode(CTaskVehicleGoToPlane::VirtualSpeedControlMode_SlowDescent);
		pTaskVehicleGoToPlane->SetVirtualSpeedMaxDescentRate(vRateDescent.Getf());
		

		pTaskVehicleGoToPlane->SetForceDriveOnGround(true); // don't roll

		SetNewTask(pTaskVehicleGoToPlane);
	}
	return FSM_Continue;
}

CTask::FSM_Return CTaskVehicleLandPlane::Descend_OnUpdate()
{

	CPlane* pPlane = static_cast<CPlane*>(GetVehicle());
	if ( pPlane )
	{	
		CTaskVehicleGoToPlane* pTask = static_cast<CTaskVehicleGoToPlane*>(GetSubTask());
		if ( pTask )
		{
			// use the nose of the plane to determine when to stop descent
			float planebottom = pPlane->GetVehicleModelInfo()->GetBoundingBoxMin().z;
			Vec3V vOffset = pPlane->GetTransform().GetUp() * ScalarV(planebottom);


			Vec3V vPlanePos = pPlane->ComputeNosePosition() + vOffset;
			Vec3V vDeltaPos = m_vRunwayEnd - vPlanePos;

			if ( fabsf(vDeltaPos.GetZf()) <= sm_Tunables.m_HeightToStartLanding )
			{
				SetState(kState_PerformLanding);
			}
			else
			{
				ScalarV vDistAhead = Min( Dist(m_vRunwayEnd, vPlanePos), ScalarV(sm_Tunables.m_SlowDownDistance));
				Vec3V vRunWayDir = Normalize(m_vRunwayEnd - m_vRunwayStart);
				Vec3V vTargetPos = vPlanePos + vRunWayDir * vDistAhead;
			
				Vector3 target(vTargetPos.GetXf(), vTargetPos.GetYf(), m_vRunwayEnd.GetZf());
				pTask->SetTargetPosition(&target);
			}
		}
	}

	if ( m_TimeOnTheGround >= sm_Tunables.m_TimeOnGroundToDrive  )
	{
		SetState(kState_TaxiToTargetLocation);
	}


	// check if we overshot runway
	Vec3V vPlanePos = pPlane->GetVehiclePosition();
	Vec3V vRunWayDir = Normalize(m_vRunwayEnd - m_vRunwayStart);	
	Vec3V vPlaneToRunwayEnd = Normalize(m_vRunwayEnd - vPlanePos);	
	ScalarV dot = Dot(vRunWayDir, vPlaneToRunwayEnd);
	if ( (dot < ScalarV(V_ZERO) ).Getb() 
		|| GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_GOTO_PLANE) )
	{
		if ( m_bAllowRetry )
		{
			SetState(kState_Init);
		}
		else
		{
			return FSM_Quit;
		}
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskVehicleLandPlane::PerformLanding_OnEnter()
{
	CPlane* pPlane = static_cast<CPlane*>(GetVehicle());
	if ( pPlane )
	{	
		Vec3V vPlanePos = pPlane->GetVehiclePosition();
		Vec3V vDelta = m_vRunwayEnd - vPlanePos;
		ScalarV vDistance = Mag(vDelta);
		Vec3V vRunWayEnd = m_vRunwayEnd;
		vRunWayEnd.SetZ(vPlanePos.GetZ() + vDistance * ScalarV(sm_Tunables.m_LandSlopeNoseUpMax)); // pitch up

		sVehicleMissionParams params;
		params.m_fCruiseSpeed = 20.0f;
		params.SetTargetPosition(VEC3V_TO_VECTOR3(vRunWayEnd));
		params.m_fTargetArriveDist = 40.0f;

		static int s_FlightHeight = -5;
		static int s_FlightHeightAboveTerrain = -5;


		ScalarV vTime = (vDistance/(ScalarV(2.0f)))/ ScalarV(params.m_fCruiseSpeed);
		ScalarV vRateDescent = Abs(vDelta.GetZ()/ vTime);


		CTaskVehicleGoToPlane* pTaskVehicleGoToPlane = rage_new CTaskVehicleGoToPlane(params, s_FlightHeight, s_FlightHeightAboveTerrain);
		pTaskVehicleGoToPlane->SetIgnoreZDeltaForEndCondition(true);
		pTaskVehicleGoToPlane->SetVirtualSpeedControlMode(CTaskVehicleGoToPlane::VirtualSpeedControlMode_SlowDescent);
		pTaskVehicleGoToPlane->SetVirtualSpeedMaxDescentRate(vRateDescent.Getf());		
		pTaskVehicleGoToPlane->SetForceDriveOnGround(true);
		
		SetNewTask(pTaskVehicleGoToPlane);

		return FSM_Continue;
	}

	return FSM_Continue;
}
 
CTask::FSM_Return CTaskVehicleLandPlane::PerformLanding_OnUpdate()
{
	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished) || !GetSubTask() )
	{
		SetState(kState_Exit);
	}

	CTaskVehicleGoToPlane* pTaskVehicleGoToPlane = static_cast<CTaskVehicleGoToPlane*>(GetSubTask());
	if ( pTaskVehicleGoToPlane )
	{
		if ( m_TimeOnTheGround > 0 )
		{
			// once the wheels touch the ground remove artificial pitch
			Vector3 vTarget = VEC3V_TO_VECTOR3(m_vRunwayEnd);
			pTaskVehicleGoToPlane->SetTargetPosition(&vTarget);
		}
		else
		{
			CPlane* pPlane = static_cast<CPlane*>(GetVehicle());
			if ( pPlane )
			{	
				Vec3V vPlanePos = pPlane->GetVehiclePosition();
				ScalarV vPlaneDistance = Dist(m_vRunwayEnd, vPlanePos);
				ScalarV vRunwayDistance = Dist(m_vRunwayEnd, m_vRunwayStart);
				ScalarV vTValue = vPlaneDistance / vRunwayDistance;

				Assert(vRunwayDistance.Getf() > 0.001f);

				ScalarV vSlope = Lerp(vTValue, ScalarV(sm_Tunables.m_LandSlopeNoseUpMin), ScalarV(sm_Tunables.m_LandSlopeNoseUpMax));
				Vec3V vRunWayEnd = m_vRunwayEnd;
				vRunWayEnd.SetZ(vPlanePos.GetZ() + vPlaneDistance * vSlope); // pitch up
				Vector3 vTarget = VEC3V_TO_VECTOR3(vRunWayEnd);
				pTaskVehicleGoToPlane->SetTargetPosition(&vTarget);

				// slow descent at the very bottom to keep from breaking landing gear
				if ( ( vPlanePos.GetZ() - m_vRunwayEnd.GetZ() < ScalarV(3.0f) ).Getb() )
				{
					pTaskVehicleGoToPlane->SetVirtualSpeedMaxDescentRate(1.0f);
				}
			}
		}	
	}

	if ( m_TimeOnTheGround >= sm_Tunables.m_TimeOnGroundToDrive  )
	{
		SetState(kState_TaxiToTargetLocation);
	}



	return FSM_Continue;
}

CTask::FSM_Return CTaskVehicleLandPlane::TaxiToTargetLocation_OnEnter()
{
	CPlane* pPlane = static_cast<CPlane*>(GetVehicle());
	if ( pPlane )
	{	
		Vector3 vTarget = VEC3V_TO_VECTOR3(m_vRunwayEnd);
		static float s_TargetReached = 20.0f;
		static float s_StraightLineDistance = -1.0f;
		static float s_CruiseSpeed = 20.0f;
		static u32 s_DrivingFlags = DF_PlaneTaxiMode | DF_PreferNavmeshRoute;
		aiTask* pTask = CVehicleIntelligence::GetGotoTaskForVehicle(pPlane, NULL, &vTarget, s_DrivingFlags, s_TargetReached, s_StraightLineDistance, s_CruiseSpeed);

		//aiTask *pVehicleTask = CVehicleIntelligence::GetGotoTaskForVehicle(pVehicle, NULL, &VecCoors, iDrivingFlags, TargetRadius, StraightLineDist, CruiseSpeed);
		//pTask = rage_new CTaskControlVehicle(pVehicle, pVehicleTask );

		// turn this back on now that we've landed
		pPlane->SetAllowFreezeWaitingOnCollision(true);
		pPlane->m_nVehicleFlags.bShouldFixIfNoCollision = true;

		SetNewTask(pTask);
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskVehicleLandPlane::TaxiToTargetLocation_OnUpdate()
{
	static float distthreshold = 20.0f;

	if ( GetIsFlagSet(aiTaskFlags::SubTaskFinished) || !GetSubTask() )
	{
		SetState(kState_Exit);
	}
	CPlane* pPlane = static_cast<CPlane*>(GetVehicle());
	if ( pPlane )
	{	
		Vec2V vRunwayEnd(m_vRunwayEnd.GetIntrin128ConstRef());
		Vec2V vPlanePos(pPlane->GetVehiclePosition().GetIntrin128ConstRef());

		// check to see if we're lined up with the runway
		ScalarV vDistance2 = DistSquared(vRunwayEnd, vPlanePos);	
		if ( (vDistance2 < ScalarV(square(distthreshold))).Getb() )
		{
			SetState(kState_Exit);
		}
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskVehicleLandPlane::Exit_OnUpdate()
{
	CPlane* pPlane = static_cast<CPlane*>(GetVehicle());
	if ( pPlane )
	{	
		pPlane->m_nVehicleFlags.bDisableHeightMapAvoidance = false;
		pPlane->m_nVehicleFlags.bUnbreakableLandingGear = false;
	}

	return FSM_Quit;
}