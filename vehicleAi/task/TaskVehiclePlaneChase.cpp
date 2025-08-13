//
// PlaneChase
// Designed for following some sort of entity
//

// File header
#include "TaskVehiclePlaneChase.h"

// Game headers
#include "grcore/debugdraw.h"
#include "math/angmath.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "task/Vehicle/TaskCar.h"
#include "vehicleAi/VehicleIntelligence.h"
#include "vehicleAi/task/TaskVehicleGoToPlane.h"
#include "Vehicles/planes.h"
#include "Vehicles/vehicle.h"


VEHICLE_OPTIMISATIONS()
AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

//=========================================================================
// CTaskVehiclePlaneChase
//=========================================================================

CTaskVehiclePlaneChase::Tunables CTaskVehiclePlaneChase::sm_Tunables;
IMPLEMENT_VEHICLE_AI_TASK_TUNABLES(CTaskVehiclePlaneChase, 0x2deab7c0);

static float s_fSlowTargetSpeedThreshold = 25.0f;
//static float s_fSlowTargetDistanceThreshold = 500.0f;
static float s_fMinSpeedSlowTargets = 60.0f;
static float s_fMinSpeedSlowTargetsNetwork = 40.0f;
static float s_MinRadiusPadding = 75.0f;
 
CTaskVehiclePlaneChase::CTaskVehiclePlaneChase(const sVehicleMissionParams& params, const CAITarget& rTarget)
: CTaskVehicleGoTo(params)
, m_Target(rTarget)
, m_bWasDiveBombingLastFrame(false)
, m_fTimeBeingPursued(0)
, m_fTargetAvgSpeed(0)
{
	// mission params mostly use cruise speed so lets base min and max on that
	// not the best but unless I want scripters to redo the game it's what we have
	static float s_CruiseSpeedWindowMin = 0.4f;
	m_fMinSpeed = Max(sm_Tunables.MinSpeed, params.m_fCruiseSpeed * s_CruiseSpeedWindowMin );
	m_fMaxSpeed = sm_Tunables.MaxSpeed;

	Vec3V vTargetVelocity = CalculateTargetVelocity();
	m_fTargetAvgSpeed = Mag(vTargetVelocity).Getf();
	


	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_PLANE_CHASE);
}

CTaskVehiclePlaneChase::~CTaskVehiclePlaneChase()
{
}

void CTaskVehiclePlaneChase::SetMinChaseSpeed(float in_Speed)
{
	m_fMinSpeed = in_Speed;
}

void CTaskVehiclePlaneChase::SetMaxChaseSpeed(float in_Speed)
{
	m_fMaxSpeed = in_Speed;
}

const CEntity* CTaskVehiclePlaneChase::GetTargetEntity() const
{
	//Ensure the target is valid.
	if(!m_Target.GetIsValid())
	{
		return NULL;
	}
	
	return m_Target.GetEntity();
}

CTask::FSM_Return CTaskVehiclePlaneChase::ProcessPreFSM()
{
	//Ensure the target entity is valid.
	if(!m_Target.GetIsValid())
	{
		return FSM_Quit;
	}

	//SPARR have this enabled by default in the future
	if(GetVehicle()->GetDriver() && GetVehicle()->GetDriver()->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_AllowDogFighting))
	{
		UpdateTargetPursuingUs();
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskVehiclePlaneChase::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
	
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_Pursue)
			FSM_OnEnter
				Pursue_OnEnter();
			FSM_OnUpdate
				return Pursue_OnUpdate();

		FSM_State(State_EscapePursuer)
			FSM_OnEnter
				EscapePursuer_OnEnter();
			FSM_OnUpdate
				return EscapePursuer_OnUpdate();
			FSM_OnExit
				EscapePursuer_OnExit();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskVehiclePlaneChase::Start_OnUpdate()
{
	//Move to the pursue state.
	SetState(State_Pursue);
	
	return FSM_Continue;
}


bool CTaskVehiclePlaneChase::IsTargetInAir() const
{
	//Grab the target entity.
	const CEntity* pTargetEntity = GetTargetEntity();

	if ( pTargetEntity )
	{
		if ( pTargetEntity->GetIsTypePed() )
		{
			const CPed* pPed = static_cast<const CPed*>(pTargetEntity);
			const CVehicle* pVehicle = pPed->GetVehiclePedInside();
			if ( pVehicle )
			{
				return const_cast<CVehicle*>(pVehicle)->IsInAir();
			}
		}
		else if ( pTargetEntity->GetIsTypeVehicle() )
		{
			const CVehicle* pVehicle = static_cast<const CVehicle*>(pTargetEntity);
			return const_cast<CVehicle*>(pVehicle)->IsInAir();
		}
	}

	return false;
}


float ComputeFlightLengthToTarget(CPlane& in_Plane, Vec3V_In in_Target, float in_Radius )
{
	Vec2V vTargetPos = in_Target.GetXY();
	Vec2V vPlanePos = in_Plane.GetVehiclePosition().GetXY();
	Vec2V vPlaneRight = in_Plane.GetTransform().GetA().GetXY();
	vPlaneRight = Normalize(vPlaneRight);

	Vec2V vCircleCenter = vPlanePos + ( vPlaneRight * ScalarV(-in_Radius));
	Vec2V vCircleCenterToPlane = vPlanePos - vCircleCenter;
	Vec2V vCircleCenterToTarget = vTargetPos - vCircleCenter;

	float theta1 = Atan2f(vCircleCenterToPlane.GetYf(), vCircleCenterToPlane.GetXf());
	float theta2 = Atan2f(vCircleCenterToTarget.GetYf(), vCircleCenterToTarget.GetXf());

	float theta = fabs(SubtractAngleShorter(theta1, theta2));
	float arcLength = TWO_PI * fabs(in_Radius) * (theta / (TWO_PI));
	float distance = arcLength;

	return distance;
}

float ComputeTimeToTarget(CPlane& in_Plane, Vec3V_In in_Target, float in_Radius, float in_Speed )
{
	if ( in_Speed > 0 )
	{
		float distance = ComputeFlightLengthToTarget(in_Plane, in_Target, in_Radius );
		float time = distance / in_Speed;
		return time;
	}
	// some big value to represent we will never catchup
	return 10000000.0f;
}

float NormalDot(Vec3V_In in_a, Vec3V_In in_b)
{
	return ((Dot(in_a, in_b) + ScalarV(V_ONE)) * ScalarV(V_HALF)).Getf();
}

float PositiveDot(Vec3V_In in_a, Vec3V_In in_b)
{
	return (Clamp( Dot(in_a, in_b), ScalarV(V_ZERO), ScalarV(V_ONE))).Getf();
}

void CTaskVehiclePlaneChase::Pursue_OnEnter()
{
	//Create the params.
	sVehicleMissionParams params;
	params.SetTargetEntity(const_cast<CEntity *>(GetTargetEntity()));
	params.SetTargetPosition(VEC3V_TO_VECTOR3(CalculateTargetPosition()));
	params.m_iDrivingFlags = DF_DontTerminateTaskWhenAchieved|DF_TargetPositionOverridesEntity;
	params.m_fTargetArriveDist = 0.0f;
	params.m_fCruiseSpeed = 20.0f;
	CTaskVehicleGoToPlane* pVehicleTask = rage_new CTaskVehicleGoToPlane(params);
	pVehicleTask->SetMaxThrottle(10.0f); // chase gets more throttle so it can catch up

	SetNewTask(pVehicleTask);

	m_PreviousTargetPosition = params.GetTargetPosition();
}

CTask::FSM_Return CTaskVehiclePlaneChase::Pursue_OnUpdate()
{
	if(m_fTimeBeingPursued > 3.0f)
	{
		SetState(State_EscapePursuer);
		return FSM_Continue;
	}

	//Ensure the active task is valid.
	CTaskVehicleGoToPlane* pTask = static_cast<CTaskVehicleGoToPlane*>(GetSubTask());
	if(pTask)
	{
		CPlane* pPlane = static_cast<CPlane*>(GetVehicle());
// 		CPlane* plane = static_cast<CPlane*>(GetVehicle());
// 		plane->SetDesiredVerticalFlightModeRatio(0.0f);

		Vec3V vTargetPosition = CalculateTargetPosition();
		Vec3V vTargetFuturePosition = CalculateTargetFuturePosition();
		Vec3V vTargetVelocity = CalculateTargetVelocity();
		Vec3V vPlanePos = pPlane->GetVehiclePosition();
		Vec3V vPlaneFwd = pPlane->GetTransform().GetB();
		Vec3V vPlaneToTarget = vTargetPosition-vPlanePos;
		Vec3V vDirPlaneToTarget = Normalize(vPlaneToTarget);

		ScalarV vTargetSpeed = Mag(vTargetVelocity);
		ScalarV vPlaneSpeed = ScalarV(pPlane->GetVelocity().Mag());
		ScalarV vDistanceToTargetXY = MagXY(vPlaneToTarget);

		// smooth the avg speed over time
		m_fTargetAvgSpeed = (vTargetSpeed.Getf() * .1f + m_fTargetAvgSpeed * .9f);

		// tweak different between single/multiplayer
		bool bTargetIsSlow = NetworkInterface::IsGameInProgress() ? m_fTargetAvgSpeed < s_fSlowTargetSpeedThreshold : vTargetSpeed.Getf() < s_fSlowTargetSpeedThreshold;
		float fSlowMinSpeed = NetworkInterface::IsGameInProgress() ? s_fMinSpeedSlowTargetsNetwork : s_fMinSpeedSlowTargets;

		float fTurnRadiusPadding = !bTargetIsSlow ? s_MinRadiusPadding : m_Params.m_fTargetArriveDist/2.0f;
		float fMinSpeed = bTargetIsSlow ? fSlowMinSpeed : m_fMinSpeed;
		//don't go less than half our minimum air speed, as it just looks really stupid
		fMinSpeed = rage::Max(fMinSpeed, CTaskVehicleGoToPlane::GetMinAirSpeed(pPlane) * 0.5f);
		Vec3V vTargetDirection = !bTargetIsSlow ? Normalize(vTargetVelocity) : vDirPlaneToTarget;
		
#if !__FINAL
		m_fComputedMinSpeed = fMinSpeed;
#endif // !__FINAL

		Vector3 modifiedTargetPosition = VEC3V_TO_VECTOR3(vTargetPosition);
		float maxRoll = CTaskVehicleGoToPlane::ComputeMaxRollForPitch(pTask->GetDesiredPitch(), pTask->GetMaxRoll());
		float fDesiredSpeed = fMinSpeed;
		
		if ( (MagXY(vTargetDirection) >= ScalarV(0.0f)).Getb() )
		{
			if ( CTaskVehicleGoToPlane::ComputeApproachTangentPosition(modifiedTargetPosition, pPlane, VEC3V_TO_VECTOR3(vTargetFuturePosition), VEC3V_TO_VECTOR3(vTargetDirection), fDesiredSpeed, maxRoll, true, false) )
			{
				static float s_SpeedMatchTime = 8.0f;
				static float s_AngleToAccelerate = 15.0f;
				static float s_SpeedMatchDistance = 40.0f;

				Vec3V vPlaneModifiedTarget = VECTOR3_TO_VEC3V(modifiedTargetPosition);
				Vec3V vPlaneToModifiedTarget = vPlaneModifiedTarget-vPlanePos;
				Vec3V vDirPlaneToModifiedTarget = Normalize(vPlaneToModifiedTarget);

				float fRadiusToLocalTarget = CTaskVehicleGoToPlane::ComputeTurnRadiusForTarget(pPlane, VEC3V_TO_VECTOR3(vPlaneModifiedTarget), 0.0f );
				float fMaxSpeed = CTaskVehicleGoToPlane::ComputeSpeedForRadiusAndRoll(*pPlane, fabsf(fRadiusToLocalTarget), maxRoll);		

				float fPosDotToTarget = PositiveDot(vDirPlaneToTarget, vPlaneFwd);
				float fPosDotToModifiedTarget = PositiveDot( vDirPlaneToModifiedTarget, vPlaneFwd);
				float fTValueMinMaxSpeed = Clamp( (fPosDotToTarget * fPosDotToModifiedTarget) / cos(s_AngleToAccelerate * DtoR), 0.0f, 1.0f);

				float fTimeToTarget = ComputeTimeToTarget(*pPlane, vPlaneModifiedTarget, fRadiusToLocalTarget, vPlaneSpeed.Getf() - vTargetSpeed.Getf() );

				float fDesiredSpeedMinMax = Lerp(fTValueMinMaxSpeed, fMinSpeed, fMaxSpeed);
				float fSpeedMatchTValue = Clamp( fTimeToTarget / s_SpeedMatchTime, 0.0f, 1.0f);

				fDesiredSpeed = Min( Lerp(fSpeedMatchTValue, vTargetSpeed.Getf(), fDesiredSpeedMinMax), fDesiredSpeedMinMax );
				fDesiredSpeed = Clamp(fDesiredSpeed, fMinSpeed, m_fMaxSpeed);

				if ( vDistanceToTargetXY.Getf() > s_SpeedMatchDistance && fTimeToTarget > s_SpeedMatchTime )
				{
					// don't do thrust falloff.  Let our plane catch up
					pPlane->SetEnableThrustFallOffThisFrame(false);
				}		
			}

			// do dive bomb logic
			bool bWasDiveBombingLastFrame = m_bWasDiveBombingLastFrame;
			m_bWasDiveBombingLastFrame = false;
			if ( !IsTargetInAir() )
			{
				static float s_fMaxDistanceToDiveBomb = 1000.0f;
				float xyDist = vDistanceToTargetXY.Getf();	

				if ( xyDist <= s_fMaxDistanceToDiveBomb )
				{
					static int s_MaxHeight = 120;
					static int s_MinHeight = 15;
					static float s_ForwardThreshold = 0.707f;
					//static float s_SlopeThreshold = -0.2f;

					pTask->SetMinHeightAboveTerrain(s_MaxHeight);
					modifiedTargetPosition.z += s_MaxHeight;

					// dive bomb chase 
					if ( Dot(vDirPlaneToTarget, vPlaneFwd).Getf() >= s_ForwardThreshold )
					{
						m_bWasDiveBombingLastFrame = true;
						//sparr - don't do this it causes planes not to avoid terrain
						//pTask->SetAvoidTerrainMaxZThisFrame(false);
						float zDelta = ( vPlaneToTarget.GetZf() + s_MinHeight );
					
						if ( xyDist > 0 || bWasDiveBombingLastFrame )
						{
							if ( bWasDiveBombingLastFrame || zDelta / xyDist )
							{			
								modifiedTargetPosition.z -= s_MaxHeight;
								modifiedTargetPosition.z += s_MinHeight;
								pTask->SetMinHeightAboveTerrain(s_MinHeight);
							}
						}
					}	
				}
			}
		}

		static float s_fWeight = 2.0f;
		float tValue = Clamp(GetTimeStep() * s_fWeight, 0.0f, 1.0f);
		m_PreviousTargetPosition = Lerp(tValue, m_PreviousTargetPosition, modifiedTargetPosition);
		m_PreviousTargetPosition = VEC3V_TO_VECTOR3(Clamp(VECTOR3_TO_VEC3V(m_PreviousTargetPosition), Vec3V(WORLDLIMITS_XMIN, WORLDLIMITS_YMIN,WORLDLIMITS_ZMIN), Vec3V(WORLDLIMITS_XMAX,WORLDLIMITS_YMAX,WORLDLIMITS_ZMAX)));

		pTask->SetTargetPosition(&m_PreviousTargetPosition);
		pTask->SetCruiseSpeed(Min(fDesiredSpeed, CTaskVehicleMissionBase::MAX_CRUISE_SPEED - 1.0f));
		pTask->SetMinTurnRadiusPadding(fTurnRadiusPadding);
	}

	//Check if the sub-task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Finish the task.
		SetState(State_Finish);
	}

	return FSM_Continue;
}

//very basic system to make us get out of being pursued
//would be nice to turn this into a dog fighting system in the future
void CTaskVehiclePlaneChase::UpdateTargetPursuingUs()
{
	const CVehicle* pTargetVehicle = GetTargetVehicle();
	if ( pTargetVehicle )
	{
		if(pTargetVehicle->InheritsFromPlane() && pTargetVehicle->IsInAir())
		{
			Vec3V vTargetPos = pTargetVehicle->GetTransform().GetPosition(); 
			Vec3V vOurPos = GetVehicle()->GetTransform().GetPosition();
			//within 200 meters
			//if(IsLessThanAll(DistSquared(vTargetPos, vOurPos), ScalarV(40000.0f)))
			{
				Vec3V vOurForward = GetVehicle()->GetTransform().GetForward();
				Vec3V vTargetToUs = vOurPos - vTargetPos;
				vTargetToUs = Normalize(vTargetToUs);
				ScalarV fPosDot = Dot(vTargetToUs, vOurForward);

				//in front of target and orientations similar
				Vec3V vTargetForward = pTargetVehicle->GetTransform().GetForward();
				ScalarV fForwardDot = Dot(vTargetForward, vOurForward);
				if(IsGreaterThanAll(fForwardDot, ScalarV(0.8f)) && IsGreaterThanAll(fPosDot, ScalarV(0.8f)))
				{
					//velocities similar
					float fTargetSpeed = pTargetVehicle->GetAiXYSpeed();
					float fOurSpeed = GetVehicle()->GetAiXYSpeed();
					if(Abs(fOurSpeed - fTargetSpeed) < 20.0f)
					{
						m_fTimeBeingPursued += fwTimer::GetTimeStep();
						return;
					}
				}	
			}
		}
	}

	m_fTimeBeingPursued = 0.0f;
}

Vec3V_Out CTaskVehiclePlaneChase::CalculateTargetPosition() const
{
	Vector3 targetPosition;
	m_Target.GetPosition(targetPosition);
	Vec3V vTargetPosition = VECTOR3_TO_VEC3V(targetPosition);
	return vTargetPosition;
}

const CVehicle* CTaskVehiclePlaneChase::GetTargetVehicle() const
{
	const CVehicle* pVehicle = NULL;
	const CEntity* pTargetEntity = GetTargetEntity();
	if ( pTargetEntity )
	{		
		if ( pTargetEntity->GetIsTypePed() )
		{
			const CPed* pPed = static_cast<const CPed*>(pTargetEntity);
			pVehicle = pPed->GetVehiclePedInside();			
		}

		if ( pTargetEntity->GetIsTypeVehicle() )
		{
			pVehicle = static_cast<const CVehicle*>(pTargetEntity);
		}
	}

	return pVehicle;
};

Vec3V_Out CTaskVehiclePlaneChase::CalculateTargetFuturePosition(float in_fTimeAhead) const
{
	Vector3 targetPosition;
	m_Target.GetPosition(targetPosition);
	Vec3V vTargetPosition = VECTOR3_TO_VEC3V(targetPosition);
	Vec3V vTargetVelocity = CalculateTargetVelocity();  
	const CVehicle* pTargetVehicle = GetTargetVehicle();
	bool bLinearProjection = true;

	if ( pTargetVehicle )
	{
		if ( pTargetVehicle && const_cast<CVehicle*>(pTargetVehicle)->IsInAir() && pTargetVehicle->InheritsFromPlane() )
		{
			const CPlane* pPlane = static_cast<const CPlane*>(pTargetVehicle);
			float fPlaneRoll = pTargetVehicle->GetTransform().GetRoll() * RtoD;
			float fPlaneSpeed = Mag(vTargetVelocity).Getf();
			float fTurnRadius = CTaskVehicleGoToPlane::ComputeTurnRadiusForRoll(pPlane, fPlaneSpeed, fPlaneRoll );

			if ( fabsf(fTurnRadius) > 0.0f )
			{
				Vector3 vPlanePos = VEC3V_TO_VECTOR3(vTargetPosition);
				Vector3 vPlaneRight = VEC3V_TO_VECTOR3(pPlane->GetTransform().GetA());
				vPlaneRight.z = 0;
				vPlaneRight.Normalize();

				Vector3 vCircleCenter = vPlanePos + ( vPlaneRight * -fTurnRadius);
				Vector3 vCircleCenterToPlane = vPlanePos - vCircleCenter;
				float angleToPlane = Atan2f(vCircleCenterToPlane.y, vCircleCenterToPlane.x) * RtoD;

				// using arclength equation to get angle
				// arclength = 2 * PI * radius * (angle/360.0f)
				float arclengh = fPlaneSpeed * in_fTimeAhead;
				float relAngle = (360.0f * arclengh) / ( 2 * PI * fTurnRadius);

				// convert local angle to world
				float angle = fmod(relAngle + angleToPlane, 360.0f);

				// compute offset given the angle, radius and center of the circle
				Vector3 vOffsetWorld = Vector3::ZeroType;
				vOffsetWorld.x = rage::Cosf(angle * DtoR) * fabsf(fTurnRadius);		
				vOffsetWorld.y = rage::Sinf(angle * DtoR) * fabsf(fTurnRadius);
				vTargetPosition = VECTOR3_TO_VEC3V(vCircleCenter + vOffsetWorld);
			}

			bLinearProjection = false;
		}
	}

	if ( bLinearProjection )
	{
		vTargetPosition += vTargetVelocity * ScalarV(in_fTimeAhead);
	}
	
	return vTargetPosition;
}

Vec3V_Out CTaskVehiclePlaneChase::CalculateTargetVelocity() const
{
	//Grab the target entity.
	const CEntity* pTargetEntity = GetTargetEntity();

	if ( pTargetEntity )
	{
		if ( pTargetEntity->GetIsTypePed() )
		{
			const CPed* pPed = static_cast<const CPed*>(pTargetEntity);
			const CVehicle* pVehicle = pPed->GetVehiclePedInside();
			if ( pVehicle )
			{
				return VECTOR3_TO_VEC3V(pVehicle->GetVelocity());	
			}

			return VECTOR3_TO_VEC3V(pPed->GetVelocity());
		}

		if ( pTargetEntity->GetIsPhysical() )
		{
			const CPhysical* pTargetPhysical = static_cast<const CPhysical*>(pTargetEntity);
			return VECTOR3_TO_VEC3V(pTargetPhysical->GetVelocity());
		}
	}

	return Vec3V(V_ZERO);
}

void CTaskVehiclePlaneChase::EscapePursuer_OnEnter()
{
	CalculateEvadeTargetPosition();

	//Create the params.
	sVehicleMissionParams params;
	params.SetTargetPosition(m_evadeData.m_vCurrentTarget);
	params.m_iDrivingFlags = DF_DontTerminateTaskWhenAchieved;
	params.m_fTargetArriveDist = 0.0f;
	params.m_fCruiseSpeed = 100.0f;
	CTaskVehicleGoToPlane* pVehicleTask = rage_new CTaskVehicleGoToPlane(params);
	pVehicleTask->OverrideMaxRoll(90.0f);
	pVehicleTask->SetMaxThrottle(10.0f); // chase gets more throttle so it can catch up

	SetNewTask(pVehicleTask);
}

//basic avoidance at the moment
//we set a target position to the left/right of the plane and allow it to roll sharply towards it
//reevaluate every now and then to keep it moving
CTask::FSM_Return CTaskVehiclePlaneChase::EscapePursuer_OnUpdate()
{
	if(m_fTimeBeingPursued == 0.0f)
	{
		//stay evading for a short time so we don't instantly reenter escape
		m_evadeData.m_beingPursuedTimer.Set(fwTimer::GetTimeInMilliseconds(), 3000);
	}
	else
	{
		m_evadeData.m_beingPursuedTimer.Unset();
	}

	if(GetTimeInState() > 10.0f || (m_evadeData.m_beingPursuedTimer.IsSet() && m_evadeData.m_beingPursuedTimer.IsOutOfTime()))
	{
		SetState(State_Pursue);
	}

	//Ensure the active task is valid.
	CTaskVehicleGoToPlane* pTask = static_cast<CTaskVehicleGoToPlane*>(GetSubTask());
	if(pTask)
	{
		m_evadeData.m_fReavaluateTime -= fwTimer::GetTimeStep();
		if(m_evadeData.m_fReavaluateTime < 0.0f)
		{
			CalculateEvadeTargetPosition();
		}

		const CPlane* pPlane = static_cast<const CPlane*>(GetVehicle());
		const CVehicle* pTargetPlane = GetTargetVehicle();
		float fTargetSpeed = pTargetPlane->GetVelocity().Mag();

		//want to go faster than them
		float fMinSpeed = rage::Max(CTaskVehicleGoToPlane::GetMinAirSpeed(pPlane), fTargetSpeed + 10.0f);
		pTask->SetCruiseSpeed(fMinSpeed);
		pTask->SetTargetPosition(&m_evadeData.m_vCurrentTarget);
		pTask->SetMinTurnRadiusPadding(1000000.0f); //magic, make the plane just try and get here, even if it can't
	}

	//somehow we made it to our target, just resume pursue
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Pursue);
	}

	return FSM_Continue;
}

void CTaskVehiclePlaneChase::EscapePursuer_OnExit()
{
	CTaskVehicleGoToPlane* pTask = static_cast<CTaskVehicleGoToPlane*>(GetSubTask());
	if(pTask)
	{
		pTask->OverrideMaxRoll(-1.0f);
	}
}

void CTaskVehiclePlaneChase::CalculateEvadeTargetPosition()
{
	const CVehicle* pTargetPlane = GetTargetVehicle();
	if ( pTargetPlane )
	{
		Vec3V vTargetPos = pTargetPlane->GetTransform().GetPosition(); 
		Vec3V vOurPos = GetVehicle()->GetTransform().GetPosition();
		Vec3V vToTarget = vOurPos- vTargetPos;
		vToTarget = Normalize(vToTarget);

		bool bIsOnRight = IsGreaterThanAll(Dot(vToTarget, pTargetPlane->GetTransform().GetRight()), ScalarV(V_ZERO)) > 0;
		m_evadeData.m_bIsRollingRight = bIsOnRight;

		float fTargetOffsetDist = 200.0f;
		Vector3 vLocalTargetOffset(cos(60.0f) * (bIsOnRight ? -fTargetOffsetDist : fTargetOffsetDist), sin(60.0f) * -fTargetOffsetDist, 0.0f);

		//transform into world space
		m_evadeData.m_vCurrentTarget = GetVehicle()->TransformIntoWorldSpace(vLocalTargetOffset);
		m_evadeData.m_fReavaluateTime = 3.0f;
	}
}

#if !__FINAL
void CTaskVehiclePlaneChase::Debug() const
{
#if DEBUG_DRAW
	const CPlane* pPlane = static_cast<const CPlane*>(GetVehicle());
	Vec3V vPlanePos = pPlane->GetVehiclePosition();

	if(GetState() == State_Pursue)
	{
		Vec3V vTargetPosition = CalculateTargetPosition();
		Vec3V vTargetFuturePosition = CalculateTargetFuturePosition();

		grcDebugDraw::Sphere(vTargetPosition, 1.0f, Color_yellow );	
		grcDebugDraw::Sphere(vTargetFuturePosition, 1.0f, Color_brown );	

		Vec3V vTargetVelocity = CalculateTargetVelocity();

		//Vec3V vPlaneFwd = pPlane->GetTransform().GetB();
		Vec3V vPlaneToTarget = vTargetPosition-vPlanePos;
		Vec3V vDirPlaneToTarget = Normalize(vPlaneToTarget);
		//ScalarV vDistance = Mag(vPlaneToTarget);

		ScalarV vTargetSpeed = Mag(vTargetVelocity);
		Vec3V vTargetDirection = (vTargetSpeed > ScalarV(s_fSlowTargetSpeedThreshold)).Getb() ? Normalize(vTargetVelocity) : vDirPlaneToTarget;

		//Ensure the active task is valid.
		const CTaskVehicleGoToPlane* pTask = static_cast<const CTaskVehicleGoToPlane*>(GetSubTask());
		if(pTask)
		{
			bool bDebugDraw = true;
			Vector3 vModifiedTarget(0.0f, 0.0f, 0.0f);
			float maxRoll = CTaskVehicleGoToPlane::ComputeMaxRollForPitch(pTask->GetDesiredPitch(), pTask->GetMaxRoll());
			CTaskVehicleGoToPlane::ComputeApproachTangentPosition(vModifiedTarget, pPlane, VEC3V_TO_VECTOR3(vTargetFuturePosition), VEC3V_TO_VECTOR3(vTargetDirection), m_fComputedMinSpeed, maxRoll, true, bDebugDraw);
		}

		char debugText[128];

		sprintf(debugText, "Target Speed: %.3f", Mag(CalculateTargetVelocity()).Getf());
		grcDebugDraw::Text(vPlanePos, Color_black, 0, 80, debugText);

	}
	else
	{
		grcDebugDraw::Sphere(m_evadeData.m_vCurrentTarget, 2.0f, Color_yellow );	

		char debugText[128];

		sprintf(debugText, "Reevalute time: %.3f", m_evadeData.m_fReavaluateTime);
		grcDebugDraw::Text(vPlanePos, Color_black, 0, 80, debugText);
	}
	
#endif
	CTask::Debug();
}

const char* CTaskVehiclePlaneChase::GetStaticStateName(s32 iState)
{
	Assert(iState >= State_Start && iState <= State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_Pursue",
		"State_EscapePursuer",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif // !__FINAL