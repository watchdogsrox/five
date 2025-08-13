// File header
#include "vehicleAi/task/TaskVehicleShotTire.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

//=========================================================================
// CTaskVehicleShotTire
//=========================================================================

CTaskVehicleShotTire::Tunables CTaskVehicleShotTire::ms_Tunables;

IMPLEMENT_VEHICLE_AI_TASK_TUNABLES(CTaskVehicleShotTire, 0xdbc31abf);

CTaskVehicleShotTire::CTaskVehicleShotTire(eHierarchyId nTire)
: m_SwerveControls()
, m_vInitialVehicleForward(V_ZERO)
, m_fTorque(0.0f)
, m_nTire(nTire)
, m_uRunningFlags(0)
{
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_SHOT_TIRE);
}

CTaskVehicleShotTire::~CTaskVehicleShotTire()
{
}

aiTask* CTaskVehicleShotTire::Copy() const
{
	return rage_new CTaskVehicleShotTire(m_nTire);
}


#if !__FINAL
const char* CTaskVehicleShotTire::GetStaticStateName(s32 iState)
{
	Assert(iState >= State_Start && iState <= State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_Swerve",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif


CTask::FSM_Return CTaskVehicleShotTire::ProcessPreFSM()
{
	//Ensure the vehicle is valid.
	CVehicle* pVehicle = GetVehicle();
	if(!pVehicle)
	{
		return FSM_Quit;
	}
	
	//Ensure the wheel is valid.
	CWheel* pWheel = pVehicle->GetWheelFromId(m_nTire);
	if(!pWheel)
	{
		return FSM_Quit;
	}
	
	return FSM_Continue;
}

CTask::FSM_Return CTaskVehicleShotTire::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
	
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();
				
		FSM_State(State_Swerve)
			FSM_OnEnter
				Swerve_OnEnter();
			FSM_OnUpdate
				return Swerve_OnUpdate();
				
		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;
				
	FSM_End
}

CTask::FSM_Return CTaskVehicleShotTire::Start_OnUpdate()
{
	//Grab the vehicle.
	CVehicle* pVehicle = GetVehicle();
	
	//Grab the wheel.
	const CWheel* pWheel = pVehicle->GetWheelFromId(m_nTire);
	taskAssertf(pWheel, "Wheel is invalid.");
	
	//Ensure the wheel is touching.
	if(!pWheel->GetIsTouching())
	{
		return FSM_Quit;
	}

	//Grab the vehicle position.
	Vec3V vVehiclePosition = pVehicle->GetTransform().GetPosition();
	
	//Grab the vehicle forward vector.
	m_vInitialVehicleForward = pVehicle->GetTransform().GetForward();

	//Grab the vehicle right vector.
	Vec3V vVehicleRight = pVehicle->GetTransform().GetRight();

	//Grab the wheel position.
	Vec3V vWheelPosition;
	pWheel->GetWheelPosAndRadius(RC_VECTOR3(vWheelPosition));

	//Generate a vector from the vehicle to the wheel.
	Vec3V vVehicleToWheel = Subtract(vWheelPosition, vVehiclePosition);

	//Check if the wheel is on the right.
	ScalarV scDot = Dot(vVehicleRight, vVehicleToWheel);
	if(IsGreaterThanAll(scDot, ScalarV(V_ZERO)))
	{
		m_uRunningFlags.SetFlag(VSTRF_WheelIsOnRight);
	}
	else
	{
		m_uRunningFlags.ClearFlag(VSTRF_WheelIsOnRight);
	}
	
	//Calculate the torque to apply.
	{
		//Grab the vehicle speed.
		float fVehicleSpeed = pVehicle->GetVelocity().Mag();
		
		//Grab the vehicle mass.
		float fVehicleMass = pVehicle->GetMass();
		
		//Calculate the torque.
		m_fTorque = fVehicleSpeed * fVehicleMass * ms_Tunables.m_TorqueMultiplier;
		
		//Negate the torque when the wheel is on the right.
		if(m_uRunningFlags.IsFlagSet(VSTRF_WheelIsOnRight))
		{
			m_fTorque = -m_fTorque;
		}
	}
	
	//Move to the swerve state.
	SetState(State_Swerve);
	
	return FSM_Continue;
}

void CTaskVehicleShotTire::Swerve_OnEnter()
{
	//Set the steer angle based on the wheel position.
	if(m_uRunningFlags.IsFlagSet(VSTRF_WheelIsOnRight))
	{
		m_SwerveControls.SetSteerAngle(-1.0f);
	}
	else
	{
		m_SwerveControls.SetSteerAngle(1.0f);
	}
	
	//Set the throttle.
	m_SwerveControls.SetThrottle(0.0f);
	
	//Set the brake.
	m_SwerveControls.SetBrake(false);
	
	//Set the hand brake.
	m_SwerveControls.SetHandBrake(true);
}

CTask::FSM_Return CTaskVehicleShotTire::Swerve_OnUpdate()
{
	//Grab the vehicle.
	CVehicle* pVehicle = GetVehicle();
		
	//Apply the swerve controls to the vehicle.
	{
		//Copy the swerve controls.
		m_vehicleControls.Copy(m_SwerveControls);
		
		//Determine if the vehicle is going slowly.
		static float sfMaxVelocitySq = square(5.0f);
		float fVelocitySq = pVehicle->GetVelocity().Mag2();
		bool bGoingSlowly = (fVelocitySq < sfMaxVelocitySq);

		//Humanize the controls. 
		HumaniseCarControlInput(pVehicle, &m_vehicleControls, true, bGoingSlowly);
	}
	
	//Apply gradual torque to the car to flip it.
	if(ShouldApplyTorque())
	{
		//Grab the vehicle's forward vector.
		Vector3 vVehicleForward = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetForward());
		
		//The torque should be applied around the forward vector.
		Vector3 vTorque = vVehicleForward * m_fTorque;

		//Apply the torque.
		pVehicle->ApplyTorque(vTorque);
		
		//Note that torque should be applied.
		m_uRunningFlags.SetFlag(VSTRF_ApplyTorque);
	}
	
	//Check if the swerve is complete.
	if(IsSwerveComplete())
	{
		//Finish the task.
		SetState(State_Finish);
	}
	
	return FSM_Continue;
}

bool CTaskVehicleShotTire::IsSwerveComplete() const
{
	//Ensure the time has not exceeded the threshold.
	if(GetTimeInState() > ms_Tunables.m_MaxTimeInSwerve)
	{
		return true;
	}
	
	//Grab the vehicle.
	const CVehicle* pVehicle = GetVehicle();
	
	//Grab the vehicle speed.
	float fVehicleSpeedSq = pVehicle->GetVelocity().Mag2();
	
	//Ensure the speed is above the threshold.
	float fMinVehicleSpeedSq = square(ms_Tunables.m_MinSpeedInSwerve);
	if(fVehicleSpeedSq < fMinVehicleSpeedSq)
	{
		return true;
	}
	
	return false;
}

bool CTaskVehicleShotTire::ShouldApplyTorque() const
{
	//Check if the torque should be applied.
	if(m_uRunningFlags.IsFlagSet(VSTRF_ApplyTorque))
	{
		return true;
	}
	
	//Grab the vehicle.
	const CVehicle* pVehicle = GetVehicle();

	//Ensure the vehicle is not bulky.
	if(pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_IS_BULKY))
	{
		return false;
	}

	//Ensure the vehicle's velocity exceeds the threshold.
	float fSpeedSq = pVehicle->GetVelocity().XYMag2();
	float fMinSpeedSq = square(ms_Tunables.m_MinSpeedToApplyTorque);
	if(fSpeedSq < fMinSpeedSq)
	{
		return false;
	}
	
	//Grab the vehicle's forward vector.
	Vec3V vVehicleForward = pVehicle->GetTransform().GetForward();
	vVehicleForward.SetZ(ScalarV(V_ZERO));
	vVehicleForward = NormalizeFastSafe(vVehicleForward, Vec3V(V_ZERO));
	
	//Grab the vehicle's initial forward vector.
	Vec3V vInitialVehicleForward = m_vInitialVehicleForward;
	vInitialVehicleForward.SetZ(ScalarV(V_ZERO));
	vInitialVehicleForward = NormalizeFastSafe(vInitialVehicleForward, Vec3V(V_ZERO));
	
	//Ensure the difference in the initial and current forward vectors exceeds the threshold.
	ScalarV scDot = Dot(vInitialVehicleForward, vVehicleForward);
	ScalarV scMaxDot = ScalarVFromF32(ms_Tunables.m_MaxDotToApplyTorque);
	if(IsGreaterThanAll(scDot, scMaxDot))
	{
		return false;
	}
	
	return true;
}
