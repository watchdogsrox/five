//
// TaskVehicleHeliProtect
//

#include "Vehicles/vehicle.h"
#include "VehicleAi\task\TaskVehicleHeliProtect.h"
#include "VehicleAi\task\TaskVehicleGotoHelicopter.h"
#include "vehicleAi\VehicleIntelligence.h"
#include "Peds\PedIntelligence.h"
#include "Peds\ped.h"
#include "scene\world\GameWorldHeightMap.h"


AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

CTaskVehicleHeliProtect::Tunables CTaskVehicleHeliProtect::sm_Tunables;
IMPLEMENT_VEHICLE_AI_TASK_TUNABLES(CTaskVehicleHeliProtect, 0xc711201d);

// Constructor/destructor
CTaskVehicleHeliProtect::CTaskVehicleHeliProtect(const sVehicleMissionParams& in_params, const float in_OffsetDistance, const int in_MinHeightAboveTerrain, const int in_HeliFlags)
	: CTaskVehicleMissionBase(in_params)
	, m_StrafeDirection(1.0f)
	, m_OffsetDistance(in_OffsetDistance)
	, m_MinHeightAboveTerrain(in_MinHeightAboveTerrain)
	, m_NoTargetTimer(3.0f, 10.0f)
	, m_NoMovementTimer(10.0f, 30.0f)
	, m_MovementTimer(5.0f, 10.0f)
	, m_HeliFlags(in_HeliFlags)
{
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_HELI_PROTECT);
}

CTaskVehicleHeliProtect::CTaskVehicleHeliProtect(const CTaskVehicleHeliProtect& in_rhs)
	: CTaskVehicleMissionBase(in_rhs.m_Params)
	, m_StrafeDirection(in_rhs.m_StrafeDirection)
	, m_OffsetDistance(in_rhs.m_OffsetDistance)
	, m_MinHeightAboveTerrain(in_rhs.m_MinHeightAboveTerrain)
	, m_NoTargetTimer(3.0f, 10.0f)
	, m_NoMovementTimer(15.0f, 30.0f)
	, m_MovementTimer(5.0f, 7.0f)
	, m_HeliFlags(in_rhs.m_HeliFlags)
{
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_HELI_PROTECT);
}

CTaskVehicleHeliProtect::~CTaskVehicleHeliProtect()
{

}

aiTask::FSM_Return CTaskVehicleHeliProtect::ProcessPreFSM()
{
	return FSM_Continue;
}

aiTask::FSM_Return CTaskVehicleHeliProtect::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.

	FSM_Begin
		FSM_State(State_Init)
			FSM_OnUpdate
				return Init_OnUpdate(*pVehicle);

		FSM_State(State_Protect)
			FSM_OnEnter
				Protect_OnEnter(*pVehicle);
			FSM_OnUpdate
				return Protect_OnUpdate(*pVehicle);

	FSM_End
}


aiTask::FSM_Return CTaskVehicleHeliProtect::Init_OnUpdate(CVehicle&)
{
	SetState(State_Protect);
	return FSM_Continue;
}

void CTaskVehicleHeliProtect::Protect_OnEnter(CVehicle& in_Vehicle)
{
	//Create the params.
	sVehicleMissionParams params = m_Params;
	params.ClearTargetEntity();
	params.SetTargetPosition(VEC3V_TO_VECTOR3(GetDominantTargetPosition()));
	params.m_fTargetArriveDist = -1.0f;
	CTaskVehicleGoToHelicopter* pTask = rage_new CTaskVehicleGoToHelicopter(params, CTaskVehicleGoToHelicopter::HF_DontClampProbesToDestination | m_HeliFlags, -1.0f, m_MinHeightAboveTerrain);
	pTask->SetSlowDownDistance(in_Vehicle.GetAiXYSpeed());
	SetNewTask(pTask);
}

float CTaskVehicleHeliProtect::ComputeSlowDownDistance(float xySpeed)
{
	float lerpSpeed = (xySpeed - sm_Tunables.m_minSpeedForSlowDown) / (sm_Tunables.m_maxSpeedForSlowDown - sm_Tunables.m_minSpeedForSlowDown);
	return Lerp(Clamp(lerpSpeed, 0.0f, 1.0f), sm_Tunables.m_minSlowDownDistance, sm_Tunables.m_maxSlowDownDistance);
}

void CTaskVehicleHeliProtect::ComputeTargetPosition(Vec3V_Ref o_Position, const CVehicle& in_Vehicle, float in_fAngle) const
{
	Vec3V vMyPosition = in_Vehicle.GetTransform().GetPosition();
	Vec3V vTargetPosition = GetDominantTargetPosition();

	Vec3V vTargetToMe = Subtract(vMyPosition, vTargetPosition);
	Vec3V vTargetToMeDirection = NormalizeFastSafe(vTargetToMe, Vec3V(V_ZERO));

	//Flatten the target to ped direction.
	Vec3V vTargetToMeDirectionFlat = vTargetToMeDirection;
	vTargetToMeDirectionFlat.SetZ(ScalarV(V_ZERO));
	vTargetToMeDirectionFlat = NormalizeFastSafe(vTargetToMeDirectionFlat, vTargetToMeDirection);

	Vec3V vDirection = vTargetToMeDirectionFlat;
	vDirection = RotateAboutZAxis(vDirection, ScalarVFromF32(in_fAngle * DtoR));

	static float s_TimeStepScalar = 5.0f;
	ScalarV fDistToTarget = Mag(vTargetToMe);
	ScalarV fDesiredOffset = ScalarVFromF32(m_OffsetDistance);
	ScalarV fCurrentOffset = Clamp( fDistToTarget, fDesiredOffset/ScalarVFromF32(3.0f), fDesiredOffset + fDesiredOffset/ScalarVFromF32(3.0f));
	ScalarV fActualOffset = Lerp(ScalarV(GetTimeStep() * s_TimeStepScalar), fCurrentOffset, fDesiredOffset);
	Vec3V vTargetOffset = Scale(vDirection, fActualOffset);
	o_Position = vTargetPosition + vTargetOffset;

	// intialize the target position 
	o_Position.SetZ(Max( ScalarV(CGameWorldHeightMap::GetMinHeightFromWorldHeightMap(o_Position.GetXf(), o_Position.GetYf()) + m_MinHeightAboveTerrain), ScalarV(o_Position.GetZf() + m_MinHeightAboveTerrain)));
}

bool CTaskVehicleHeliProtect::ComputeTargetHeading(float& o_Heading, const CVehicle& in_Vehicle, Vec3V_In in_Target ) const
{
	if ( !m_NoTargetTimer.IsFinished() )
	{
		Vec3V vMyPosition = in_Vehicle.GetTransform().GetPosition();
		Vec3V vToTarget = in_Target - vMyPosition;
		vToTarget.SetZ(ScalarV(V_ZERO));
		if ( (MagSquared(vToTarget) < ScalarV(20.0f) ).Getb() )
		{

			const Vec3V vPosition = in_Vehicle.GetTransform().GetPosition();
			const Vec3V vToProtectionPos = GetDominantTargetPosition() - vPosition;
			const Vec2V vToProtectionPosXY = Vec2V(vToProtectionPos.GetX(), vToProtectionPos.GetY());
			const Vec2V vDirectionXY = Normalize(vToProtectionPosXY);
			o_Heading  = fwAngle::GetATanOfXY(vDirectionXY.GetXf(), vDirectionXY.GetYf());

			bool bLeft, bRight;
			if ( ComputeValidShooterSides(bLeft, bRight, in_Vehicle) )
			{
				if(bLeft && !bRight)
				{
					o_Heading += -HALF_PI;
				}
				else if(!bLeft && bRight)
				{
					o_Heading += HALF_PI;
				}
				else
				{
					//Check the strafe direction.
					if(m_StrafeDirection < 0.0f)
					{
						o_Heading += HALF_PI;
					}
					else
					{
						o_Heading += -HALF_PI;
					}
				}
				
				return true;
			}		
		}
	}

	o_Heading = -1.0f;
	return false;
}

void CTaskVehicleHeliProtect::UpdateTargetCollisionProbe(const CVehicle& in_Vehicle, const Vector3& in_Position)
{
	//
	// don't bother doing this until target slows down
	//
	Vec3V vTargetVelocity = GetDominantTargetVelocity();
	static float s_MaxVelocityForCollisionProbe = 10.0f;
	if ( MagSquared(vTargetVelocity).Getf() < square(s_MaxVelocityForCollisionProbe) )
	{
		if ( m_uRunningFlags.IsFlagSet(RF_TargetChanged))
		{
			m_uRunningFlags.ClearFlag(RF_TargetChanged);
			m_targetLocationTestResults.Reset();
		}
		else if ( m_targetLocationTestResults.GetResultsReady() )
		{
			if ( m_targetLocationTestResults[0].GetHitDetected() )
			{
				m_uRunningFlags.SetFlag(RF_IsColliding);
			}
			m_targetLocationTestResults.Reset();
		}

		if (!m_targetLocationTestResults.GetWaitingOnResults() )
		{
			WorldProbe::CShapeTestCapsuleDesc CapsuleTest;
			static float s_RadiusScalar = 0.35f;
			static float s_Distance = 10.0f;
			Vector3 vTargetPosition = in_Position;
			Vector3 vHeliPosition = VEC3V_TO_VECTOR3(in_Vehicle.GetTransform().GetPosition());
			Vector3 vHeliToTarget = in_Position - vHeliPosition;
			float fDist = vHeliToTarget.Mag();
			float fRadius = in_Vehicle.GetBoundRadius()*s_RadiusScalar;

			Vector3 vStartPosition = vTargetPosition + Vector3(0.1f, 0.1f, 0.1f);
			Vector3 vEndPosition = vTargetPosition;

			if ( fDist > 0 )
			{
				vHeliToTarget.InvScale(fDist);
				vStartPosition = vEndPosition - vHeliToTarget * Min(Max(fDist, fRadius), s_Distance);
			}

			// offset the end point so swept sphere check works.  probably an early out issue
			CapsuleTest.SetCapsule(vStartPosition, vEndPosition, fRadius);
			CapsuleTest.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
			CapsuleTest.SetExcludeInstance(in_Vehicle.GetCurrentPhysicsInst());
			CapsuleTest.SetResultsStructure(&m_targetLocationTestResults);
			CapsuleTest.SetMaxNumResultsToUse(1);

			// currently only swept sphere supports async on both platforms
			static bool s_hackusesweptsphere = true;
			CapsuleTest.SetIsDirected(s_hackusesweptsphere);
			CapsuleTest.SetDoInitialSphereCheck(s_hackusesweptsphere);
			WorldProbe::GetShapeTestManager()->SubmitTest(CapsuleTest, WorldProbe::PERFORM_ASYNCHRONOUS_TEST);

			// grcDebugDraw::Capsule(VECTOR3_TO_VEC3V(CapsuleTest.GetStart()), VECTOR3_TO_VEC3V(CapsuleTest.GetEnd()), CapsuleTest.GetRadius(),  Color_blue, false, -1);
		}
	}
}



aiTask::FSM_Return CTaskVehicleHeliProtect::Protect_OnUpdate(CVehicle& in_Vehicle)
{
	if ( GetSubTask() )
	{
		CTaskVehicleGoToHelicopter* pTask = static_cast<CTaskVehicleGoToHelicopter*>(GetSubTask());
		
		m_NoMovementTimer.Tick(GetTimeStep());
		m_NoTargetTimer.Tick(GetTimeStep());
		m_MovementTimer.Tick(GetTimeStep());

		if ( CanAnyoneSeeATarget(in_Vehicle) )
		{
			m_NoTargetTimer.Reset();
		}

		if ( m_uRunningFlags.IsFlagSet(RF_IsColliding) )
		{
			m_uRunningFlags.ClearFlag(RF_IsColliding);
			m_MovementTimer.Reset();
			m_StrafeDirection = -m_StrafeDirection;
			m_uRunningFlags.SetFlag(RF_TargetChanged);
		}
		else if ( m_NoTargetTimer.IsFinished() || m_NoMovementTimer.IsFinished() )
		{		
			m_MovementTimer.Reset();
			m_StrafeDirection = fwRandom::GetRandomTrueFalse() ? 1.0f : -1.0f;
			m_uRunningFlags.SetFlag(RF_TargetChanged);
		}
		
		float fAngle = 0.0f;
		if ( !m_MovementTimer.IsFinished() )
		{			
			static float s_fAngleLookAhead = 25.0f;
			m_NoMovementTimer.Reset();
			m_NoTargetTimer.Reset();
			fAngle = m_StrafeDirection * s_fAngleLookAhead;
		}

		Vec3V vTargetPosition;
		ComputeTargetPosition(vTargetPosition, in_Vehicle, fAngle);
		Vector3 vAdjustedTarget = VEC3V_TO_VECTOR3(vTargetPosition);

		UpdateTargetCollisionProbe(in_Vehicle, vAdjustedTarget);
		pTask->SetTargetPosition(&vAdjustedTarget);
		pTask->SetOrientationRequested(false);
		pTask->SetSlowDownDistance(ComputeSlowDownDistance(in_Vehicle.GetAiXYSpeed()));

		float fHeading = -1.0f;
		if ( ComputeTargetHeading(fHeading, in_Vehicle, vTargetPosition))
		{
			pTask->SetOrientation(fHeading);
			pTask->SetOrientationRequested(true);
		}
	}
	
	return FSM_Continue;
}

Vec3V CTaskVehicleHeliProtect::GetDominantTargetPosition() const
{
	if(GetTargetEntity())
	{
		if ( GetTargetEntity()->GetIsTypePed() )
		{
			const CPed *pPed = static_cast<const CPed*>(GetTargetEntity());
			const CVehicle* pVeh = pPed->GetVehiclePedInside();
			if(pVeh)
			{
				return pVeh->GetTransform().GetPosition();
			}
		}
		return GetTargetEntity()->GetTransform().GetPosition();
	}

	return Vec3V(V_ZERO);
}

Vec3V CTaskVehicleHeliProtect::GetDominantTargetVelocity() const
{
	if(GetTargetEntity())
	{
		if ( GetTargetEntity()->GetIsTypePed() )
		{
			const CPed *pPed = static_cast<const CPed*>(GetTargetEntity());
			const CVehicle* pVeh = pPed->GetVehiclePedInside();
			if(pVeh)
			{
				return VECTOR3_TO_VEC3V(pVeh->GetVelocity());
			}
		}
		return VECTOR3_TO_VEC3V(GetTargetEntity()->GetVelocity());
	}

	return Vec3V(V_ZERO);

}


bool CTaskVehicleHeliProtect::CanAnyoneSeeATarget(CVehicle& in_Vehicle) const
{

	//Ensure the seat manager is valid.
	const CSeatManager* pSeatManager = in_Vehicle.GetSeatManager();
	if(pSeatManager)
	{
		int iMaxSeats = pSeatManager->GetMaxSeats();
		for(int iSeat = 0; iSeat < iMaxSeats; ++iSeat)
		{	
			const CPed* pPedInSeat = pSeatManager->GetPedInSeat(iSeat);
			if(pPedInSeat)
			{
				CPedTargetting* pTargeting = pPedInSeat->GetPedIntelligence()->GetTargetting(true);
				if ( pTargeting )
				{
					if ( pTargeting->GetBestTarget() )
					{
						if ( pTargeting->GetLosStatus(pTargeting->GetBestTarget()) == Los_clear )
						{
							return true;
						}
					}
				}
			}
		}
	}
	return false;
}

bool CTaskVehicleHeliProtect::ComputeValidShooterSides(bool& o_Left, bool& o_Right, const CVehicle& in_Vehicle) const
{	
	o_Left = false;
	o_Right = false;
	for( s32 iSeat = 0; iSeat < in_Vehicle.GetSeatManager()->GetMaxSeats(); iSeat++ )
	{
		const CPed* pPed = in_Vehicle.GetSeatManager()->GetPedInSeat(iSeat);
		if(  pPed && !pPed->IsInjured() &&	iSeat != in_Vehicle.GetDriverSeat() )
		{
			const CPedEquippedWeapon* pPedEquippedWeapon = pPed->GetWeaponManager()->GetPedEquippedWeapon();
			if ( pPedEquippedWeapon )
			{
				// Figure out which side of the aircraft the seat is on
				int iSeatBone = in_Vehicle.GetVehicleModelInfo()->GetModelSeatInfo()->GetBoneIndexFromSeat(iSeat);
				if(iSeatBone > -1)
				{
					const crBoneData* pBoneData = in_Vehicle.GetSkeletonData().GetBoneData(iSeatBone);
					Assert(pBoneData);
					bool bLeft = pBoneData->GetDefaultTranslation().GetXf() < 0.0f;

					if( bLeft )
					{
						o_Left = true;
					}
					else
					{
						o_Right = true;
					}
				}
			}
		}
	}
	return o_Left || o_Right;
}



#if __BANK
void CTaskVehicleHeliProtect::Debug() const
{
#if DEBUG_DRAW
	// m_targetLocationTestResults

	// grcDebugDraw::Sphere(m_vModifiedTarget, Max(m_Params.m_fTargetArriveDist, 1.0f), Color_BlueViolet, false);		
#endif
}
#endif //__BANK

#if !__FINAL
const char* CTaskVehicleHeliProtect::GetStaticStateName( s32 in_State )
{
	static const char* snames[] =
	{
		"State_Init",
		"State_Update",
	};

	taskAssert(in_State <= kNumStates);
	CompileTimeAssert(NELEM(snames) == kNumStates );
	return snames[in_State];
}
#endif //!__FINAL

