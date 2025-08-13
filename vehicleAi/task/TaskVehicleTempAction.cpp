#include "TaskVehicleTempAction.h"


// Game headers
#include "VehicleAi/VehicleIntelligence.h"
#include "Vehicles/vehicle.h"
#include "Vehicles/planes.h"
#include "Vehicles/Submarine.h"
#include "audio/vehicleaudioentity.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

static dev_float s_fStationaryVelocityThreshold = FLT_MAX;

aiTask::FSM_Return CTaskVehicleTempAction::ProcessPreFSM()
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.
	if (pVehicle && (!m_bAllowSuperDummyForThisTask 
		|| (m_bAllowSuperDummyForThisTask && pVehicle->GetAiXYSpeed() > s_fStationaryVelocityThreshold)))
	{
		pVehicle->m_nVehicleFlags.bPreventBeingSuperDummyThisFrame = true;
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskVehicleWait::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.

	FSM_Begin
		// Initial state
		FSM_State(State_Wait)
			FSM_OnUpdate
				return Wait_OnUpdate(pVehicle);
		// Quit
		FSM_State(State_Exit)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}


//////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehicleWait::Wait_OnUpdate				(CVehicle* pVehicle)
{
	pVehicle->SetHandBrake(false);
	//pVehicle->SetSteerAngle(0.0f); //dont think we need to adjust the steer angle when waiting
	pVehicle->SetThrottle(0.0f);
	pVehicle->SetBrake(0.2f);

	pVehicle->GetIntelligence()->UpdateCarHasReasonToBeStopped();

	if(NetworkInterface::GetSyncedTimeInMilliseconds() > m_tempActionFinishTimeMs)
	{
		SetState(State_Exit);
		pVehicle->GetIntelligence()->LastTimeNotStuck = NetworkInterface::GetSyncedTimeInMilliseconds();
	}

	return FSM_Continue;
}


//////////////////////////////////////////////////////////////////

#if !__FINAL
const char * CTaskVehicleWait::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_Wait&&iState<=State_Exit);
	static const char* aStateNames[] = 
	{
		"State_Wait",
		"State_Exit"
	};

	return aStateNames[iState];
}
#endif



//////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskVehicleReverse::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.

	FSM_Begin
		// Reverse state
		FSM_State(State_Reverse)
			FSM_OnEnter
				Reverse_OnEnter(pVehicle);
			FSM_OnUpdate
				return Reverse_OnUpdate(pVehicle);
		// Quit
		FSM_State(State_Exit)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

//////////////////////////////////////////////////////////////////

void CTaskVehicleReverse::Reverse_OnEnter(CVehicle* pVehicle)
{
	pVehicle->SwitchEngineOn();
}

//////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehicleReverse::Reverse_OnUpdate				(CVehicle* pVehicle)
{
	switch(m_ReverseType)
	{
	case Reverse_Opposite_Direction:
		{
			// To do sensible 3 point turns if we're in a race we'll set the
			// steer angle opposite as to if we're going somewhere			
			//ControlAIVeh_OnlyMission(pVehicle, &m_VehicleControls, pUberMission); //TODO may not be required
			pVehicle->SetHandBrake(false);
			pVehicle->SetSteerAngle(0.0f);	// This is were the 3 point turning comes in

			// If the car is going forward reasonable fast we slow down by using the brakes.
			// Otherwise we reverse.

			// Get the current drive direction and orientation.
			const Vector3 vehDriveDir = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleDriveDir(pVehicle, IsDrivingFlagSet(DF_DriveInReverse)));

			if(DotProduct(pVehicle->GetVelocityIncludingReferenceFrame(), vehDriveDir) > 2.0f)
			{
				pVehicle->SetThrottle(0.0f);
				pVehicle->SetBrake(0.75f);
			}
			else
			{
				pVehicle->SetThrottle(-0.75f);
				pVehicle->SetBrake(0.0f);

				if(IsDrivingFlagSet(DF_DriveInReverse))
				{
					pVehicle->SetThrottle(-pVehicle->GetThrottle());
				}
			}
			break;
		}
	case Reverse_Left:
		{
			pVehicle->SetThrottle( -0.75f );
			pVehicle->SetBrake(0.0f);
			pVehicle->SetHandBrake(false);
			pVehicle->SetSteerAngle(pVehicle->GetIntelligence()->FindMaxSteerAngle());

			break;
		}
	case Reverse_Right:
		{
			pVehicle->SetThrottle( -0.75f);
			pVehicle->SetBrake(0.0f);
			pVehicle->SetHandBrake(false);
			pVehicle->SetSteerAngle(-pVehicle->GetIntelligence()->FindMaxSteerAngle());

			break;
		}
	case Reverse_Straight:
	case Reverse_Straight_Hard:
		{
			pVehicle->SetHandBrake(false);
			pVehicle->SetSteerAngle(0.0f);	// This is were the 3 point turning comes in

			// If the car is going forward reasonable fast we slow down by using the brakes.
			// Otherwise we reverse.
			if(DotProduct(pVehicle->GetVelocityIncludingReferenceFrame(), VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetB())) > 0.1f)
			{
				pVehicle->SetThrottle(0.0f);
				pVehicle->SetBrake(0.5f);
			}
			else
			{
				float fThrottle = 0.0f;
				if(!m_uConfigFlags.IsFlagSet(CF_RandomizeThrottle))
				{
					if(m_ReverseType != Reverse_Straight_Hard)
					{
						fThrottle = -0.5f;
					}
					else
					{
						fThrottle = -1.0f;
					}
				}
				else
				{
					if(m_ReverseType != Reverse_Straight_Hard)
					{
						fThrottle = -pVehicle->GetRandomNumberInRangeFromSeed(0.4f, 0.6f);
					}
					else
					{
						fThrottle = -pVehicle->GetRandomNumberInRangeFromSeed(0.8f, 1.0f);
					}
				}

				pVehicle->SetThrottle(fThrottle);
				pVehicle->SetBrake(0.0f);
			}
			break;
		}
	}

	//Check if we are done.
	bool bDone = (m_uConfigFlags.IsFlagSet(CF_QuitIfVehicleCollisionDetected) && pVehicle->GetFrameCollisionHistory()->GetFirstVehicleCollisionRecord());
	bDone |= (NetworkInterface::GetSyncedTimeInMilliseconds() > m_tempActionFinishTimeMs);
	if(bDone)
	{
		SetState(State_Exit);
		pVehicle->GetIntelligence()->LastTimeNotStuck = NetworkInterface::GetSyncedTimeInMilliseconds();
	}

	return FSM_Continue;
}


//////////////////////////////////////////////////////////////////

#if !__FINAL
const char * CTaskVehicleReverse::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_Reverse&&iState<=State_Exit);
	static const char* aStateNames[] = 
	{
		"State_Reverse",
		"State_Exit"
	};

	return aStateNames[iState];
}
#endif

//////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskVehicleBrake::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.

	FSM_Begin
		// Initial state
		FSM_State(State_Brake)
			FSM_OnUpdate
				return Brake_OnUpdate(pVehicle);
		// Quit
		FSM_State(State_Exit)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}


//////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehicleBrake::Brake_OnUpdate				(CVehicle* pVehicle)
{
	pVehicle->SetHandBrake(m_bUseHandBrake);

	//sometimes we want to brake and not reset our steering to the centre
	if (!m_bDontResetSteerAngle)
	{
		pVehicle->SetSteerAngle(0.0f);
	}

	bool bUseReverse = m_bUseReverse &&
		(pVehicle->GetVelocity().Dot(VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetForward())) > 0.0f);
	float fThrottle = !bUseReverse ? 0.0f : -1.0f;
	pVehicle->SetThrottle(fThrottle);
	pVehicle->SetBrake(1.0f);

	pVehicle->GetIntelligence()->UpdateCarHasReasonToBeStopped();

	if(NetworkInterface::GetSyncedTimeInMilliseconds() > m_tempActionFinishTimeMs)
	{
		SetState(State_Exit);
		pVehicle->GetIntelligence()->LastTimeNotStuck = NetworkInterface::GetSyncedTimeInMilliseconds();
	}

	return FSM_Continue;
}


//////////////////////////////////////////////////////////////////

#if !__FINAL
const char * CTaskVehicleBrake::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_Brake&&iState<=State_Exit);
	static const char* aStateNames[] = 
	{
		"State_Brake",
		"State_Exit"
	};

	return aStateNames[iState];
}
#endif



//////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskVehicleGoForward::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.

	FSM_Begin
		// Initial state
		FSM_State(State_GoForward)
			FSM_OnEnter
				GoForward_OnEnter(pVehicle);
			FSM_OnUpdate
				return GoForward_OnUpdate(pVehicle);
	// Quit
		FSM_State(State_Exit)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

//////////////////////////////////////////////////////////////////

void CTaskVehicleGoForward::GoForward_OnEnter(CVehicle* pVehicle)
{
	pVehicle->SwitchEngineOn();
}

//////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehicleGoForward::GoForward_OnUpdate				(CVehicle* pVehicle)
{
	pVehicle->SetHandBrake(false);
	pVehicle->SetSteerAngle(0.0f);
	pVehicle->SetThrottle(!m_bHard ? 0.5f : 1.0f);
	pVehicle->SetBrake(0.0f);

	if(NetworkInterface::GetSyncedTimeInMilliseconds() > m_tempActionFinishTimeMs)
	{
		SetState(State_Exit);
		pVehicle->GetIntelligence()->LastTimeNotStuck = NetworkInterface::GetSyncedTimeInMilliseconds();
	}

	return FSM_Continue;
}


//////////////////////////////////////////////////////////////////

#if !__FINAL
const char * CTaskVehicleGoForward::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_GoForward&&iState<=State_Exit);
	static const char* aStateNames[] = 
	{
		"State_GoForward",
		"State_Exit"
	};

	return aStateNames[iState];
}
#endif


//////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskVehicleHeadonCollision::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.

	FSM_Begin
		// Initial state
		FSM_State(State_HeadonCollision)
			FSM_OnUpdate
				return HeadonCollision_OnUpdate(pVehicle);
		// Quit
		FSM_State(State_Exit)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}


//////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehicleHeadonCollision::HeadonCollision_OnUpdate				(CVehicle* pVehicle)
{
	pVehicle->SetHandBrake(true);
	pVehicle->SetBrake(1.0f);
	pVehicle->SetThrottle(0.0f);

	const bool bGoingRight = m_SwerveDirection == Swerve_Right || (m_SwerveDirection == Swerve_NoPreference && pVehicle->GetSteerAngle() < 0.0f);
	if(bGoingRight)
	{
		pVehicle->SetSteerAngle(rage::Max(-0.5f, pVehicle->GetSteerAngle() - 0.05f*50.0f*fwTimer::GetTimeStep()));
	}
	else if (m_SwerveDirection != Swerve_Straight)
	{
		pVehicle->SetSteerAngle(rage::Min(0.5f, pVehicle->GetSteerAngle() + 0.05f*50.0f*fwTimer::GetTimeStep()));
	}

	//if we've got "Swerve_Straight" set as the type then maintain the previous steer angle

	if(NetworkInterface::GetSyncedTimeInMilliseconds() > m_tempActionFinishTimeMs)
	{
		SetState(State_Exit);
		pVehicle->SetHandBrake(false);
		pVehicle->SetBrake(0.0f);
		return FSM_Continue;
	}

	return FSM_Continue;
}


//////////////////////////////////////////////////////////////////

#if !__FINAL
const char * CTaskVehicleHeadonCollision::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_HeadonCollision&&iState<=State_Exit);
	static const char* aStateNames[] = 
	{
		"State_HeadonCollision",
		"State_Exit"
	};

	return aStateNames[iState];
}
#endif


//////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskVehicleBoostUseSteeringAngle::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.

	FSM_Begin
		// Initial state
		FSM_State(State_BoostUseSteeringAngle)
			FSM_OnUpdate
				return BoostUseSteeringAngle_OnUpdate(pVehicle);
		// Quit
		FSM_State(State_Exit)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

//////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehicleBoostUseSteeringAngle::BoostUseSteeringAngle_OnUpdate				(CVehicle* pVehicle)
{
	pVehicle->SetHandBrake(false);
	pVehicle->SetSteerAngle(pVehicle->GetSteerAngle());
	pVehicle->SetThrottle(1.0f);
	pVehicle->SetBrake(0.0f);

	// Cheat a bit by increasing the move speed.
	pVehicle->SetVelocity(pVehicle->GetVelocity() + VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetB()) *(50.0f*fwTimer::GetTimeStep() * 0.012f));

	if(NetworkInterface::GetSyncedTimeInMilliseconds() > m_tempActionFinishTimeMs)
	{
		SetState(State_Exit);
	}

	return FSM_Continue;
}


//////////////////////////////////////////////////////////////////

#if !__FINAL
const char * CTaskVehicleBoostUseSteeringAngle::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_BoostUseSteeringAngle&&iState<=State_Exit);
	static const char* aStateNames[] = 
	{
		"State_BoostUseSteeringAngle",
		"State_Exit"
	};

	return aStateNames[iState];
}
#endif

//////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskVehicleHandBrake::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.

	FSM_Begin
		// HandBrake state
		FSM_State(State_HandBrake)
			FSM_OnUpdate
				return HandBrake_OnUpdate(pVehicle);
		// Quit
		FSM_State(State_Exit)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

//////////////////////////////////////////////////////////////////

float CTaskVehicleHandBrake::s_fMaxSpeedForHandbrake = 15.0f;
aiTask::FSM_Return	CTaskVehicleHandBrake::HandBrake_OnUpdate				(CVehicle* pVehicle)
{
	switch(m_HandBrakeAction)
	{
	case HandBrake_Left:
		pVehicle->SetHandBrake(true);
		pVehicle->SetSteerAngle(pVehicle->GetIntelligence()->FindMaxSteerAngle());
		pVehicle->SetThrottle(0.0f);
		pVehicle->SetBrake(0.0f);
		if(NetworkInterface::GetSyncedTimeInMilliseconds() > m_tempActionFinishTimeMs)
		{
			SetState(State_Exit);
		}
		break;
	case HandBrake_Right:

		pVehicle->SetHandBrake(true);
		pVehicle->SetSteerAngle(-pVehicle->GetIntelligence()->FindMaxSteerAngle());
		pVehicle->SetThrottle(0.0f);
		pVehicle->SetBrake(0.0f);

		if(NetworkInterface::GetSyncedTimeInMilliseconds() > m_tempActionFinishTimeMs)
		{
			SetState(State_Exit);
		}
		break;
	case HandBrake_Straight:

		pVehicle->SetHandBrake(true);
		pVehicle->SetSteerAngle(0.0f);
		pVehicle->SetThrottle(0.0f);
		pVehicle->SetBrake(0.0f);

		if(NetworkInterface::GetSyncedTimeInMilliseconds() > m_tempActionFinishTimeMs)
		{
			SetState(State_Exit);
		}
		break;


	case HandBrake_Left_Intelligence:
		{
			float speed = pVehicle->GetVelocityIncludingReferenceFrame().XYMag();
			if(speed > s_fMaxSpeedForHandbrake)		// If we're going too fast we will brake to slow down.
			{
				pVehicle->SetHandBrake(false);
				pVehicle->SetSteerAngle(0.0f);
				pVehicle->SetThrottle(0.0f);
				pVehicle->SetBrake(1.0f);
			}
			else
			{
				pVehicle->SetHandBrake(true);
				pVehicle->SetSteerAngle(pVehicle->GetIntelligence()->FindMaxSteerAngle());
				pVehicle->SetThrottle(0.0f);
				pVehicle->SetBrake(0.0f);

				if(speed < 0.5f)
				{
					SetState(State_Exit);
				}
			}
			if(NetworkInterface::GetSyncedTimeInMilliseconds() > m_tempActionFinishTimeMs)
			{
				SetState(State_Exit);
			}
		}
		break;

	case HandBrake_Right_Intelligence:
		{
			float speed = pVehicle->GetVelocityIncludingReferenceFrame().XYMag();
			if(speed > s_fMaxSpeedForHandbrake)		// If we're going too fast we will brake to slow down.
			{
				pVehicle->SetHandBrake(false);
				pVehicle->SetSteerAngle(-pVehicle->GetIntelligence()->FindMaxSteerAngle());
				pVehicle->SetThrottle(0.0f);
				pVehicle->SetBrake(1.0f);
			}
			else
			{
				pVehicle->SetHandBrake(true);
				pVehicle->SetSteerAngle(-pVehicle->GetIntelligence()->FindMaxSteerAngle());
				pVehicle->SetThrottle(0.0f);
				pVehicle->SetBrake(0.0f);

				if(speed < 0.5f)
				{
					SetState(State_Exit);
				}
			}
			if(NetworkInterface::GetSyncedTimeInMilliseconds() > m_tempActionFinishTimeMs)
			{
				SetState(State_Exit);
			}
		}
		break;

	case HandBrake_Straight_Intelligence:
		{
			float speed = pVehicle->GetVelocityIncludingReferenceFrame().XYMag();
			if(speed > s_fMaxSpeedForHandbrake)		// If we're going too fast we will brake to slow down.
			{
				pVehicle->SetHandBrake(true);
				pVehicle->SetSteerAngle(0.0f);
				pVehicle->SetThrottle(0.0f);
				pVehicle->SetBrake(1.0f);
			}
			else
			{
				pVehicle->SetHandBrake(true);
				pVehicle->SetSteerAngle(0.0f);
				pVehicle->SetThrottle(0.0f);
				pVehicle->SetBrake(0.0f);

				if(speed < 0.5f)
				{
					SetState(State_Exit);
				}
			}
			if(NetworkInterface::GetSyncedTimeInMilliseconds() > m_tempActionFinishTimeMs)
			{
				SetState(State_Exit);
			}
		}
		break;
	}

	return FSM_Continue;
}


//////////////////////////////////////////////////////////////////

#if !__FINAL
const char * CTaskVehicleHandBrake::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_HandBrake&&iState<=State_Exit);
	static const char* aStateNames[] = 
	{
		"State_HandBrake",
		"State_Exit"
	};

	return aStateNames[iState];
}
#endif



/////////////////////////
// BRING VEHICLE TO HALT
/////////////////////////
CTaskBringVehicleToHalt::CTaskBringVehicleToHalt(const float fStoppingDistance, const int nTimeToStopFor, const bool bControlVerticalVelocity)
: m_nTimeToStopFor(nTimeToStopFor),
m_fBrakeAmount(1.0f),
m_fStoppingDistance(fStoppingDistance),
m_fInitialSpeed(0.0f),
m_bControlVerticalVelocity(bControlVerticalVelocity)
{
	SetInternalTaskType(CTaskTypes::TASK_BRING_VEHICLE_TO_HALT);
}

CTaskBringVehicleToHalt::~CTaskBringVehicleToHalt()
{
}

void CTaskBringVehicleToHalt::CleanUp()
{
}

#if !__FINAL
// return the name of the given task as a string
const char * CTaskBringVehicleToHalt::GetStaticStateName( s32 iState )
{
	// Make sure FSM state is valid
	taskAssert(iState <= State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_HaltingCar",
		"State_HaltedAndWaiting",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif //!__FINAL

CTask::FSM_Return CTaskBringVehicleToHalt::UpdateFSM( const s32 iState, const FSM_Event iEvent)
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.

	FSM_Begin
		FSM_State(State_Start)
		FSM_OnUpdate
		return Start_OnUpdate(pVehicle);

	FSM_State(State_HaltingCar)
		FSM_OnEnter
		return HaltingCar_OnEnter(pVehicle);
	FSM_OnUpdate
		return HaltingCar_OnUpdate(pVehicle);

	FSM_State(State_HaltedAndWaiting)
		FSM_OnEnter
		return HaltedAndWaiting_OnEnter(pVehicle);
	FSM_OnUpdate
		return HaltedAndWaiting_OnUpdate(pVehicle);

	FSM_State(State_Finish)
		FSM_OnUpdate
		return FSM_Quit;
	FSM_End
}

CTask::FSM_Return CTaskBringVehicleToHalt::Start_OnUpdate(CVehicle* UNUSED_PARAM(pVehicle))
{
	SetState(State_HaltingCar);

	return FSM_Continue;
}

CTask::FSM_Return CTaskBringVehicleToHalt::HaltingCar_OnEnter(CVehicle* pVehicle)
{
	if(!taskVerifyf(pVehicle, "No target vehicle can be found to bring to halt."))
	{
		return FSM_Continue;
	}

	// Store the position and velocity when the command was first given so that we can
	// work out stopping distance and all that jazz.
	m_vInitialPos = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
	m_vInitialVel = pVehicle->GetVelocity();
	if(!m_bControlVerticalVelocity)
	{
		m_vInitialVel.z = 0.0f;
	}
	m_fInitialSpeed = m_vInitialVel.Mag();
	m_bStopped = false;

	float fMaxSpeed = 80.0f; // Default in case we don't have any handling info.
	if(pVehicle->pHandling)
	{
		// Once we are traveling faster than some percentage of the estimated top speed for this
		// vehicle, the brake will be applied at its maximum.
		if(pVehicle->pHandling->m_fEstimatedMaxFlatVel > 0.0f)
		{
			fMaxSpeed = 0.7f*pVehicle->pHandling->m_fEstimatedMaxFlatVel;
		}
	}
	taskAssert(fMaxSpeed > 0.0f);
	m_fBrakeAmount = m_fInitialSpeed/fMaxSpeed;
	m_fBrakeAmount = Clamp(m_fBrakeAmount, 0.0f, 1.0f);
	// Now make it a non-linear function:
	m_fBrakeAmount *= m_fBrakeAmount;

	// Disable player control.
	pVehicle->SetThrottle(0.0f);
	pVehicle->SetBrake(m_fBrakeAmount);

	// Want to glide smoothly to a halt without lateral spring forces from the wheels
	for(int i=0; i < pVehicle->GetNumWheels(); i++)
	{
		pVehicle->GetWheel(i)->GetDynamicFlags().SetFlag(WF_NO_LATERAL_SPRING);
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskBringVehicleToHalt::HaltingCar_OnUpdate(CVehicle* pVehicle)
{
	if(!taskVerifyf(pVehicle, "No target vehicle can be found to bring to halt."))
	{
		SetState(State_Finish);
	}

	taskAssert(!m_bStopped);

	// Disable player control.
	pVehicle->SetThrottle(0.0f);
	pVehicle->SetBrake(m_fBrakeAmount);

	pVehicle->GetIntelligence()->UpdateCarHasReasonToBeStopped();
	pVehicle->GetVehicleAudioEntity()->SetVehicleIsBeingStopped();

	// Begin decreasing the vehicle's horizontal velocity.

	Vector3 vCurrentPos = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
	Vector3 vInitialToCurrent; vInitialToCurrent.Subtract(vCurrentPos, m_vInitialPos);

	float fNormalisedDist = vInitialToCurrent.Mag()/m_fStoppingDistance;
	float fNewSpeed = m_fInitialSpeed * (1.0f - fNormalisedDist);
	fNormalisedDist = Clamp(fNormalisedDist, 0.0f, 1.0f);

	// Don't want to end up crawling at a snail's pace at the end, so just kill the speed when we are slow enough.
	if(fNewSpeed < 1.0f || pVehicle->GetVelocity().Mag() < 1.0f)
	{
		fNewSpeed = 0.0f;
		m_bStopped = true;
	}

	Vector3 vNewVel = m_vInitialVel;
	vNewVel.Normalize();
	vNewVel.Scale(fNewSpeed);
	if(!m_bControlVerticalVelocity)
	{
		// Just let gravity get applied by the simulator.
		vNewVel.z = pVehicle->GetVelocity().GetZ();
	}

	// don't let this function ever increase the velocity of the vehicle
	if(vNewVel.Mag2() < pVehicle->GetVelocity().Mag2())
	{
		pVehicle->SetVelocity(vNewVel);
		pVehicle->SelectAppropriateGearForSpeed();
	}

	// Want to glide smoothly to a halt without lateral spring forces from the wheels
	for(int i=0; i < pVehicle->GetNumWheels(); i++)
	{
		pVehicle->GetWheel(i)->GetDynamicFlags().SetFlag(WF_NO_LATERAL_SPRING);
	}

	if(m_bStopped)
	{
		// If the vehicle has been stopped, do nothing until the timer runs out and then quit the task.
		SetState(State_HaltedAndWaiting);
		return FSM_Continue;
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskBringVehicleToHalt::HaltedAndWaiting_OnEnter(CVehicle* pVehicle)
{
	m_nWaitingTimeStart = NetworkInterface::GetSyncedTimeInMilliseconds();
	for(int i=0; i < pVehicle->GetNumWheels(); i++)
	{
		pVehicle->GetWheel(i)->GetDynamicFlags().ClearFlag(WF_NO_LATERAL_SPRING);
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskBringVehicleToHalt::HaltedAndWaiting_OnUpdate(CVehicle* pVehicle)
{
	// The vehicle has been brought to rest. Do nothing until the stop timer runs out.
	// m_nTimeToStopFor < 0 means to stop forever.
	if(m_nTimeToStopFor > 0 && NetworkInterface::GetSyncedTimeInMilliseconds() > m_nWaitingTimeStart + m_nTimeToStopFor*1000)
	{
		SetState(State_Finish);
	}
	else
	{
		pVehicle->GetIntelligence()->UpdateCarHasReasonToBeStopped();

		pVehicle->SetThrottle(0.0f);
		pVehicle->SetBrake(1.0f);
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////

#if 0

aiTask::FSM_Return CTaskVehicleReverseAlongPointPath::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.

	FSM_Begin
		// Initial state
		FSM_State(State_ReverseAlongPointPath)
			FSM_OnUpdate
				return ReverseAlongPointPath_OnUpdate(pVehicle);
		// Quit
		FSM_State(State_Exit)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

//////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehicleReverseAlongPointPath::ReverseAlongPointPath_OnUpdate(CVehicle* pVehicle)
{
	if( FollowPointPath(pVehicle, &m_vehicleControls, true)
		|| NetworkInterface::GetSyncedTimeInMilliseconds() > m_tempActionFinishTimeMs)
	{
		SetState(State_Exit);
		pVehicle->GetIntelligence()->m_pointPath.Clear();

		if(DotProduct(VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetB()), pVehicle->GetVelocityIncludingReferenceFrame()) < -3.0f)
		{
			SetNewTask(rage_new CTaskVehicleHandBrake(NetworkInterface::GetSyncedTimeInMilliseconds() + 3000,CTaskVehicleHandBrake::HandBrake_Straight_Intelligence));
		}
	}

	// The values we found may be completely different from the ones last frame.
	// The following function smooths out the values and in some cases clips them.
	HumaniseCarControlInput( pVehicle, &m_vehicleControls, true, (pVehicle->GetVelocity().Mag2() <(5.0f*5.0f)) );

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////

#if !__FINAL
const char * CTaskVehicleReverseAlongPointPath::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_ReverseAlongPointPath&&iState<=State_Exit);
	static char* aStateNames[] = 
	{
		"State_ReverseAlongPointPath",
		"State_Exit"
	};

	return aStateNames[iState];
}
#endif

#endif //0

//////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskVehicleTurn::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.

	FSM_Begin
		// Turn state
		FSM_State(State_Turn)
			FSM_OnUpdate
				return Turn_OnUpdate(pVehicle);
		// Quit
		FSM_State(State_Exit)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

//////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehicleTurn::Turn_OnUpdate(CVehicle* pVehicle)
{
	switch(m_TurnAction)
	{
		case Turn_Left://turn left
		{
			pVehicle->SetHandBrake(false);
			pVehicle->SetSteerAngle(pVehicle->GetIntelligence()->FindMaxSteerAngle());
			pVehicle->SetThrottle(1.0f);
			pVehicle->SetBrake(0.0f);

			if(NetworkInterface::GetSyncedTimeInMilliseconds() > m_tempActionFinishTimeMs)
				SetState(State_Exit);

			break;
		}
		case Turn_Right://turn right
		{
			pVehicle->SetHandBrake(false);
			pVehicle->SetSteerAngle(-pVehicle->GetIntelligence()->FindMaxSteerAngle());
			pVehicle->SetThrottle(1.0f);
			pVehicle->SetBrake(0.0f);

			if(NetworkInterface::GetSyncedTimeInMilliseconds() > m_tempActionFinishTimeMs)
				SetState(State_Exit);

			break;
		}
	}
	

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////

#if !__FINAL
const char * CTaskVehicleTurn::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_Turn&&iState<=State_Exit);
	static const char* aStateNames[] = 
	{
		"State_Turn",
		"State_Exit"
	};

	return aStateNames[iState];
}
#endif

//////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskVehicleSwerve::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.

	FSM_Begin
		// Swirve state
		FSM_State(State_Swerve)
			FSM_OnUpdate
				return Swerve_OnUpdate(pVehicle);
		FSM_State(State_Brake)
			FSM_OnEnter
				Brake_OnEnter(pVehicle);
			FSM_OnUpdate
				return Wait_OnUpdate(pVehicle);
		FSM_State(State_Wait)
			FSM_OnEnter
				Wait_OnEnter(pVehicle);
			FSM_OnUpdate
				return Wait_OnUpdate(pVehicle);
		// Quit
		FSM_State(State_Exit)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

void CTaskVehicleSwerve::Brake_OnEnter(CVehicle* /*pVehicle*/)
{
	CTask* pBrakeTask = rage_new CTaskVehicleBrake(NetworkInterface::GetSyncedTimeInMilliseconds() + 4000);
	static_cast<CTaskVehicleBrake*>(pBrakeTask)->SetDontResetSteerAngle(true);
	SetNewTask(pBrakeTask);
}

void CTaskVehicleSwerve::Wait_OnEnter(CVehicle* /*pVehicle*/)
{
	SetNewTask(rage_new CTaskVehicleWait(NetworkInterface::GetSyncedTimeInMilliseconds() + 4000));
}

aiTask::FSM_Return	CTaskVehicleSwerve::Wait_OnUpdate(CVehicle* /*pVehicle*/)
{
	if (!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		return FSM_Quit;
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehicleSwerve::Swerve_OnUpdate(CVehicle* pVehicle)
{	
	bool bSteerAngleSwapped = false;
	pVehicle->SetHandBrake(false);
	pVehicle->SetThrottle(0.0f);
	pVehicle->SetBrake(0.001f);

	if(m_StopAfterSwirve)
	{
		pVehicle->SetBrake(0.3f);
	}

	if(m_SwirveDirection == Swerve_Jitter)
	{
		bool bJitterRight = ((m_tempActionFinishTimeMs - NetworkInterface::GetSyncedTimeInMilliseconds()) % 100) == 0;
		pVehicle->SetSteerAngle(bJitterRight ? 0.25f : -0.25f);
	}
	else
	{
		pVehicle->SetSteerAngle(0.25f);

		if(m_SwirveDirection == Swerve_Right) 
			pVehicle->SetSteerAngle(-0.25f);
	
		if(NetworkInterface::GetSyncedTimeInMilliseconds() > m_tempActionFinishTimeMs -(SWIRVEDURATION / 2))
		{
			pVehicle->SetSteerAngle(-pVehicle->GetSteerAngle());
			bSteerAngleSwapped = true;
		}
	}

	bool bStopDueToCrossingEdge = false;
	if (m_StopAfterCrossingEdge)
	{
		//are we on the far side of the road edge?

		//faster vehicles use the bonnet position so they stop earlier.
		//slower vehicles don't so they try and scootch a bit further.
		Vec3V vVehPosition = pVehicle->GetAiXYSpeed() > 9.0f 
			? CVehicleFollowRouteHelper::GetVehicleBonnetPosition(pVehicle, IsDrivingFlagSet(DF_DriveInReverse)) 
			: pVehicle->GetVehiclePosition();
		Vector2 vEdgeToCar = Vector2(vVehPosition.GetXf(), vVehPosition.GetYf()) - m_vPointOnEdge;

		if (vEdgeToCar.Dot(m_vEdgeNormal) > 0.0f)
		{
			bStopDueToCrossingEdge = true;

			//grcDebugDraw::Line(vVehPosition, vVehPosition + Vec3V(0.0f, 0.0f, 7.0f), Color_green, 10);
		}
	}

	//if we crossed the line, really try and stop. TaskVehicleWait only applies the brakes a small amount
	if (bStopDueToCrossingEdge)
	{
		if (!bSteerAngleSwapped)
		{
			pVehicle->SetSteerAngle(-pVehicle->GetSteerAngle());
			bSteerAngleSwapped = true;
		}
		SetState(State_Brake);
		return FSM_Continue;
	}

	if(NetworkInterface::GetSyncedTimeInMilliseconds() > m_tempActionFinishTimeMs)
	{
		if(m_StopAfterSwirve)
		{
			SetState(State_Wait);
		}
		else
		{
			SetState(State_Exit);
		}
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////

#if !__FINAL
const char * CTaskVehicleSwerve::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_Swerve&&iState<=State_Exit);
	static const char* aStateNames[] = 
	{
		"State_Swirve",
		"State_Brake",
		"State_Wait",
		"State_Exit"
	};

	return aStateNames[iState];
}
#endif

//////////////////////////////////////////////////////////////////

#if 0

aiTask::FSM_Return CTaskVehicleFollowPointPath::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.

	FSM_Begin
		// FollowPointPath state
		FSM_State(State_FollowPointPath)
			FSM_OnUpdate
				return FollowPointPath_OnUpdate(pVehicle);
		// Quit
		FSM_State(State_Exit)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

//////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehicleFollowPointPath::FollowPointPath_OnUpdate(CVehicle* pVehicle)
{
	if(FollowPointPath(pVehicle, &m_vehicleControls, false)
		|| NetworkInterface::GetSyncedTimeInMilliseconds() > m_tempActionFinishTimeMs)
	{
		SetState(State_Exit);
		pVehicle->GetIntelligence()->m_pointPath.Clear();
	}

	// The values we found may be completely different from the ones last frame.
	// The following function smooths out the values and in some cases clips them.
	HumaniseCarControlInput( pVehicle, &m_vehicleControls, true, (pVehicle->GetVelocity().Mag2() <(5.0f*5.0f)) );

	return FSM_Continue;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FollowPointPath
// PURPOSE :	Uses a avoidanceLists_out.avoidanceLists_out.pedList of stored coordinates to follow.
//				The coordinates are generated by navmesh so they are not equally
//				spaced.
//				(Hence the many differences with ReverseAlongPointPath())
//				if inReverse it uUses the previously collected coordinates to
//				reverse along.
//				This can be used to get the car out of trouble if it got stuck
//				in the geometry of the map.
// RETURNS :
/////////////////////////////////////////////////////////////////////////////////
bool CTaskVehicleFollowPointPath::FollowPointPath(CVehicle* pVeh, CVehControls* pVehControls, bool inReverse)
{
	Assertf(pVeh, "FollowPointPath expected a valid set vehicle.");
	Assertf(pVehControls, "FollowPointPath expected a valid set of vehicle controls.");

	pVehControls->m_handBrake = false;
	pVehControls->m_steerAngle = 0.0f;
	pVehControls->m_throttle = 0.0f;
	pVehControls->m_brake = 0.1f;

	float	forwardSpeed = DotProduct(VEC3V_TO_VECTOR3(pVeh->GetTransform().GetB()), pVeh->GetVelocity());
	if(inReverse)
	{
		if(forwardSpeed > 1.0f)
		{
			pVehControls->m_brake = 1.0f;
			return false;
		}
	}

	// Of all the line segments find the one we are closest to.
	int		index, closestIndex;
	float	distOnLine;

	const Vector3 vVehPosition = VEC3V_TO_VECTOR3(pVeh->GetTransform().GetPosition());
	if(pVeh->GetIntelligence()->m_pointPath.FindClosestIndex(vVehPosition, index, distOnLine))
	{
		closestIndex = index;

		pVehControls->m_brake = 0.0f;
		if(inReverse)
		{
			if(forwardSpeed > -10.0f)
			{
				pVehControls->m_throttle = -0.5f;
			}
		}
		else
		{
			if(forwardSpeed < 4.0f)
			{
				pVehControls->m_throttle = 0.4f;
			}
		}

		// Debug draw point paths.
#if __DEV
		if(CVehicleIntelligence::m_bDisplayDebugPointPaths && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVeh))
		{
			Vector3 crs = Vector3(	pVeh->GetIntelligence()->m_pointPath.m_points[index].x*(1.0f-distOnLine) +
				pVeh->GetIntelligence()->m_pointPath.m_points[index + 1].x*distOnLine,
				pVeh->GetIntelligence()->m_pointPath.m_points[index].y*(1.0f-distOnLine) +
				pVeh->GetIntelligence()->m_pointPath.m_points[index + 1].y*distOnLine,
				vVehPosition.z);
			grcDebugDraw::Line(crs, crs + Vector3(0.0f,0.0f,8.0f), rage::Color32(0, 255, 0, 255));
		}
#endif // __DEV

		Vector2 aimPoint;
		if(inReverse)
		{
			// Now we pick a point a little bit further behind to aim for.
#define ELEMENTS_BEHIND 3
			if(index <= 1)
			{
				return true;
			}
			else if(index < ELEMENTS_BEHIND)
			{
				index = 0;
				distOnLine = 0.0f;
			}
			else
			{
				index -= ELEMENTS_BEHIND;
			}

			aimPoint.x = pVeh->GetIntelligence()->m_pointPath.m_points[index].x*(1.0f-distOnLine) + pVeh->GetIntelligence()->m_pointPath.m_points[index + 1].x*distOnLine;
			aimPoint.y = pVeh->GetIntelligence()->m_pointPath.m_points[index].y*(1.0f-distOnLine) + pVeh->GetIntelligence()->m_pointPath.m_points[index + 1].y*distOnLine;
		}
		else
		{
			// Now we pick a point a little bit further ahead to aim for.
#define DISTANCE_AHEAD	6.0f
#define DISTANCE_DONE	3.0f
			float	distanceToGo = DISTANCE_AHEAD;

			Vector2 currCoors;
			currCoors.x = pVeh->GetIntelligence()->m_pointPath.m_points[index].x +(pVeh->GetIntelligence()->m_pointPath.m_points[index + 1].x - pVeh->GetIntelligence()->m_pointPath.m_points[index].x) * distOnLine;
			currCoors.y = pVeh->GetIntelligence()->m_pointPath.m_points[index].y +(pVeh->GetIntelligence()->m_pointPath.m_points[index + 1].y - pVeh->GetIntelligence()->m_pointPath.m_points[index].y) * distOnLine;

			while(1)
			{
				Vector2 nextPoint = Vector2(pVeh->GetIntelligence()->m_pointPath.m_points[index + 1].x, pVeh->GetIntelligence()->m_pointPath.m_points[index + 1].y);

				float distanceToGoOnThisLine =(currCoors - nextPoint).Mag();

				if(distanceToGoOnThisLine > distanceToGo)
				{
					aimPoint = currCoors +(nextPoint - currCoors) *(distanceToGo / distanceToGoOnThisLine);
					break;
				}
				aimPoint = nextPoint;
				distanceToGo -= distanceToGoOnThisLine;
				currCoors = nextPoint;

				index++;
				if(index+1 >= pVeh->GetIntelligence()->m_pointPath.m_numPoints)
				{
					break;		// No more points left on this pointpath. Jump out.
				}
			}
		}

		// Debug draw point paths...
#if __DEV
		if(CVehicleIntelligence::m_bDisplayDebugPointPaths && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVeh))
		{
			grcDebugDraw::Line(Vector3(aimPoint.x, aimPoint.y, vVehPosition.z),
				Vector3(aimPoint.x, aimPoint.y, vVehPosition.z+8.0f),
				rage::Color32(0, 255, 255, 255), rage::Color32(0, 255, 255, 255));
		}
#endif // __DEV

		const Vector3 vecForward(VEC3V_TO_VECTOR3(pVeh->GetTransform().GetB()));
		float	carOrientation = fwAngle::GetATanOfXY(vecForward.x, vecForward.y);
		float	aimOrientation = fwAngle::GetATanOfXY((aimPoint.x - vVehPosition.x), (aimPoint.y - vVehPosition.y));

		float	steerAngle = aimOrientation - carOrientation;
		steerAngle = fwAngle::LimitRadianAngle(steerAngle);
		static dev_float m = 1.0f;
		steerAngle *= m;
		const float maxSteerAngle = pVeh->GetIntelligence()->FindMaxSteerAngle();
		steerAngle = rage::Clamp(steerAngle, -maxSteerAngle, maxSteerAngle);
		pVehControls->SetSteerAngle( steerAngle );

		if(!inReverse)
		{	
			// Shift down the nodes(remove the ones we have already passed) so that we don't get confused later on.
			pVeh->GetIntelligence()->m_pointPath.Shift(closestIndex);
		}
	}
	else
	{	
		// Done follow path
		return true;
	}

	if(!inReverse)
	{	
		if(pVeh->GetIntelligence()->m_pointPath.m_numPoints <= 0 ||(vVehPosition - Vector3(pVeh->GetIntelligence()->m_pointPath.m_points[pVeh->GetIntelligence()->m_pointPath.m_numPoints - 1].x, pVeh->GetIntelligence()->m_pointPath.m_points[pVeh->GetIntelligence()->m_pointPath.m_numPoints - 1].y, 0.0f)).XYMag() < DISTANCE_DONE)
		{
			return true;
		}	
	}

	return false;
}


//////////////////////////////////////////////////////////////////

#if !__FINAL
const char * CTaskVehicleFollowPointPath::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_FollowPointPath&&iState<=State_Exit);
	static char* aStateNames[] = 
	{
		"State_FollowPointPath",
		"State_Exit"
	};

	return aStateNames[iState];
}
#endif

#endif //0

//////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskVehicleBurnout::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.

	FSM_Begin
		// Initial state
		FSM_State(State_Burnout)
			FSM_OnUpdate
				return Burnout_OnUpdate(pVehicle);
			FSM_OnExit
				Burnout_OnExit(pVehicle);
	// Quit
		FSM_State(State_Exit)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}


//////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehicleBurnout::Burnout_OnUpdate				(CVehicle* pVehicle)
{
	CAutomobile* pAuto = pVehicle->InheritsFromAutomobile() ? static_cast<CAutomobile*>(pVehicle) : NULL;

	if (pAuto)
	{
		pAuto->EnableBurnoutMode(true);
	}

	pVehicle->SetThrottle(1.0f);
	pVehicle->SetBrake(1.0f);
	pVehicle->SetSteerAngle(0.0f);
	pVehicle->SetHandBrake(0.0f);

	pVehicle->GetIntelligence()->UpdateCarHasReasonToBeStopped();

	if(NetworkInterface::GetSyncedTimeInMilliseconds() > m_tempActionFinishTimeMs)
	{
		SetState(State_Exit);
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////

void CTaskVehicleBurnout::Burnout_OnExit(CVehicle* pVehicle)
{
	CAutomobile* pAuto = pVehicle->InheritsFromAutomobile() ? static_cast<CAutomobile*>(pVehicle) : NULL;
	if (pAuto)
	{
		pAuto->EnableBurnoutMode(false);
	}

	pVehicle->SetBrake(0.0f);
}

//////////////////////////////////////////////////////////////////

#if !__FINAL
const char * CTaskVehicleBurnout::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_Burnout&&iState<=State_Exit);
	static const char* aStateNames[] = 
	{
		"State_Burnout",
		"State_Exit"
	};

	return aStateNames[iState];
}
#endif

//////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskVehicleFlyDirection::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.

	FSM_Begin
		// FlyDirection state
		FSM_State(State_FlyDirection)
			FSM_OnUpdate
				return FlyDirection_OnUpdate(pVehicle);
		// Quit
		FSM_State(State_Exit)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

//////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehicleFlyDirection::FlyDirection_OnUpdate(CVehicle* pVehicle)
{
	CPlane *pPlane = (CPlane *)pVehicle;

	FlyAIPlaneInCertainDirection(pPlane);

	if(NetworkInterface::GetSyncedTimeInMilliseconds() > m_tempActionFinishTimeMs)
	{
		SetState(State_Exit);
	}

	return FSM_Continue;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FlyAIPlaneInCertainDirection
// PURPOSE :  Makes an AI plane go towards its target coordinates.
/////////////////////////////////////////////////////////////////////////////////
void CTaskVehicleFlyDirection::FlyAIPlaneInCertainDirection(CPlane *pPlane)
{

	// Every 2 seconds or so we calculate the new height and course for this plane.
	u32	TimeThingNew = (NetworkInterface::GetSyncedTimeInMilliseconds() + pPlane->GetRandomSeed()) % AIUPDATEFREQ;
	u32	TimeThingOld = (fwTimer::GetPrevElapsedTimeInMilliseconds() + pPlane->GetRandomSeed()) % AIUPDATEFREQ;
	if (TimeThingNew < TimeThingOld)
	{
		float	PlaneOrientation, DesiredOrientation, FlightHeightFound;

		const Vector3 vecForward(VEC3V_TO_VECTOR3(pPlane->GetTransform().GetB()));
		PlaneOrientation = fwAngle::GetATanOfXY(vecForward.x, vecForward.y);
		DesiredOrientation = pPlane->m_FlightDirection;

		switch(m_FlyDirection)
		{
		case Fly_Up:
			break;
		case Fly_Straight:
			DesiredOrientation = PlaneOrientation;
			break;
		case Fly_Left:
			DesiredOrientation = PlaneOrientation - 2.0f;
			break;
		case Fly_Right:
			DesiredOrientation = PlaneOrientation + 2.0f;
			break;
		}

#define FINDDIRECTIONTRIES (20)
#define ANGLESTEP (( DtoR * 15))

		// If we can't find anything just turn round and climb as much as possible
		pPlane->m_DesiredHeight = 500.0f;
		pPlane->m_FlightDirectionAvoidingTerrain = pPlane->m_FlightDirection + PI;

		// First try the angles between the current orientation of the car and the desired orientation.

		// Go through the angles twice. First do the ones that are between the desired direction
		// and the current direction of the plane. In the second pass do the remaining ones (mainly in
		// the desired direction)


		float AngleDiff = PlaneOrientation - DesiredOrientation;
		AngleDiff = fwAngle::LimitRadianAngle(AngleDiff);

		if (ABS(AngleDiff) < ( DtoR * 30))
		{
			AngleDiff = 0.0f;
		}
		else
		{
			AngleDiff *= 1.5f;
		}

		for (s32 Pass = 0; Pass < 2; Pass++)
		{
			for (s32 C = 1; C < FINDDIRECTIONTRIES; C++)
			{	
				float	OrientationToTry, OrientationOffset;

				if (C&1)
				{
					OrientationOffset = ANGLESTEP * (C/2);
				}
				else
				{
					OrientationOffset = -ANGLESTEP * (C/2);
				}
				OrientationToTry = DesiredOrientation + OrientationOffset;

				bool bWithinAngles;
				if ( (OrientationOffset < 0.0f && OrientationOffset > AngleDiff) ||
					(OrientationOffset > 0.0f && OrientationOffset < AngleDiff))
				{
					bWithinAngles = true;
				}
				else
				{
					bWithinAngles = false;
				}

				if ( (Pass == 0 && bWithinAngles) ||
					(Pass == 1 && !bWithinAngles) )
				{
					FlightHeightFound = FindFlightHeight(pPlane, OrientationToTry);

					if (FlightHeightFound < 150.0f)
					{		// We can climb this high. We'll take this one.
						pPlane->m_FlightDirectionAvoidingTerrain = pPlane->m_FlightDirection + OrientationOffset * 1.1f;
						pPlane->m_DesiredHeight = rage::Max(FlightHeightFound + pPlane->m_MinHeightAboveTerrain, pPlane->m_LowestFlightHeight);
						break;
					}
				}
			}
		}
	}

	// The takeoffspeed (the speed at which the plane is confident it can maintain height)
	// might have to come from handling.cfg eventually
#define TAKEOFFSPEED_STANDARD (32.0f)
	float TakeOffSpeed = TAKEOFFSPEED_STANDARD;

#define DEFAULTTILT (0.0f)		// Tilt of the plane when it is flying level.
	const Vector3 vecForward(VEC3V_TO_VECTOR3(pPlane->GetTransform().GetB()));
	float 	Orientation = fwAngle::GetATanOfXY(vecForward.x, vecForward.y);
	float	Speed = (vecForward.x * pPlane->GetVelocity().x + vecForward.y * pPlane->GetVelocity().y);

	bool	bTakingOff;

	if (pPlane->HasContactWheels())
	{
		pPlane->m_OnGroundTimer = NetworkInterface::GetSyncedTimeInMilliseconds();
	}

	if (NetworkInterface::GetSyncedTimeInMilliseconds() - pPlane->m_OnGroundTimer > 4000)
	{
		bTakingOff = false;
		//if (pPlane->m_fLGearAngle != 1.0f) pPlane->SetGearUp();
	}
	else
	{
		bTakingOff = true;
	}

	float	HeightDiff = 50.0f;

	if (bTakingOff)
	{
		if (Speed < TakeOffSpeed)
		{
			pPlane->SetPitchControl(0.0f);
		}
		else
		{
			pPlane->SetPitchControl(0.4f);
		}
	}
	else
	{
#define LOOKAHEADINTIME (2.0f)  // In seconds.
		Vector3	FutureCoors = VEC3V_TO_VECTOR3(pPlane->GetTransform().GetPosition()) + pPlane->GetVelocity() * LOOKAHEADINTIME;
		//		float	CurrentTilt = fwAngle::GetATanOfXY(pVeh->GetPosition().z, DiffY);
		float	CurrentTilt = asinf(pPlane->GetTransform().GetB().GetZf());
		float	FutureTilt = CurrentTilt + (CurrentTilt - pPlane->m_OldTilt) * (LOOKAHEADINTIME / (50.0f*fwTimer::GetTimeStep()));
		pPlane->m_OldTilt = CurrentTilt;

		HeightDiff = pPlane->m_DesiredHeight - FutureCoors.z;

		// Work out the tilt we're after.
		float	DesiredTilt = HeightDiff / 30.0f;
		DesiredTilt = rage::Min(DesiredTilt, 0.4f);
		DesiredTilt = rage::Max(DesiredTilt, -0.4f);

		//// If the plane isn't going fast enough we have to limit our tilt (to avoid stalling)
		if (Speed < TakeOffSpeed && !bTakingOff)
		{
			DesiredTilt = rage::Min(DesiredTilt, 0.25f);
		}

		float	TiltDiff = DesiredTilt - FutureTilt;
		static dev_float	Tweak2 = 0.5f;
		pPlane->SetPitchControl(TiltDiff * Tweak2);
	}

	// Try and go in a straight line
	float OrientationDiff = pPlane->m_FlightDirectionAvoidingTerrain - Orientation;
	OrientationDiff = fwAngle::LimitRadianAngle(OrientationDiff);

	if (bTakingOff)
	{		// During taking off we only use Yaw to turn. (Can't use roll)
		OrientationDiff = pPlane->m_TakeOffDirection - Orientation;
		float Yaw = -OrientationDiff;
		Yaw *= 10.0f;
		Yaw = rage::Min(1.0f, Yaw);
		Yaw = rage::Max(-1.0f, Yaw);
		pPlane->SetYawControl(Yaw);
		pPlane->SetRollControl(0.0f);
	}
	else
	{		// During flight we also use Roll to turn.
		float 	Turn = -OrientationDiff;

		Turn *= 1.5f;
		Turn = rage::Min(0.9f, Turn);
		Turn = rage::Max(-0.9f, Turn);
		float	DesiredRoll;

		if (ABS(Turn) < 0.1f)
		{
			pPlane->SetYawControl(Turn * 4.0f);
			DesiredRoll = 0.0f;
		}
		else
		{
			pPlane->SetYawControl(Turn);
			DesiredRoll = -Turn;
		}


		// If the speed drops during flight we don't turn for a bit.
		if (Speed < TakeOffSpeed * 1.2f)
		{
			float TurnAllowed = 1.0f - (TakeOffSpeed * 1.2f - Speed) / (TakeOffSpeed * 0.5f);
			TurnAllowed = rage::Max(0.0f, TurnAllowed);
			DesiredRoll *= TurnAllowed;
		}

		// If the plane is already banking a lot we mellow out on the turn as well.
		//		if (pPlane->GetC().z < 0.5f)
		//		{
		//			float TurnAllowBank = 1.0f - MIN(1.0f, (0.5f - pPlane->GetC().z) * 2.0f);
		//			pPlane->m_fYawControl *= TurnAllowBank;
		//			pPlane->m_fRollControl *= TurnAllowBank;
		//		}
		float	CurrentRoll = pPlane->GetTransform().GetRoll();
		static dev_float Tweak = 30.0f;
		float	PredictedRoll = CurrentRoll + (CurrentRoll - pPlane->m_fPreviousRoll) * (Tweak / rage::Max(1.0f, 50.0f*fwTimer::GetTimeStep()));
		float	RollChange = DesiredRoll - PredictedRoll;
		RollChange = fwAngle::LimitRadianAngle(RollChange);
		pPlane->m_fPreviousRoll = CurrentRoll;
		static dev_float	Mult = -1.0f;
		pPlane->SetRollControl(rage::Clamp(RollChange * Mult,-1.0f,1.0f));
		static dev_float	Tweak3 = 0.23f;

		float OldPitchControl = pPlane->GetPitchControl();
		float fNewPitchControl = OldPitchControl;
		fNewPitchControl += ABS(CurrentRoll * Tweak3);
		if (OldPitchControl < 0.0f)
		{		// Trying to descend. We don't want to get caught up in the turn and forget to descend.
			fNewPitchControl = rage::Min(fNewPitchControl, OldPitchControl * 0.5f);
		}

		pPlane->SetPitchControl(rage::Clamp(fNewPitchControl,-1.0f,1.0f));

	}

	pPlane->SetThrottleControl(pPlane->m_fScriptThrottleControl);


	if(m_FlyDirection == Fly_Up)
	{
		pPlane->SetPitchControl(1.0f);
		// If we're threatening to stall we go back to normal
		if (Speed < 20.0) 
			SetState(State_Exit);
	}
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : For a plane this function will find the flight height to hug the terrain.
//			  It does this by scanning a few lines from the centerpoint of the plane.
// PURPOSE :
/////////////////////////////////////////////////////////////////////////////////

float CTaskVehicleFlyDirection::FindFlightHeight(CPlane *pPlane, float Orientation)
{
	float	HighestHeight = 0.0f;
	float	Result;

	static dev_float	AnglesToTest[] = { ( DtoR * 6), ( DtoR * 3), ( DtoR * 0), ( DtoR * -20), ( DtoR * -40), ( DtoR * -60), };

	for (s32 C = 0; C < 6; C++)
	{
		if (FindHeightForVerticalAngle(pPlane, AnglesToTest[C], Orientation, &Result))
		{
			if (C == 0)		// If the first line (going up as much as we can be expected to climb) isn't clear we have no chance.
			{
				return 100000.0f;
			}

			HighestHeight = rage::Max(HighestHeight, Result);
		}
	}	

	return HighestHeight;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindHeightForVerticalAngle
// PURPOSE :  Scans a single line from the plane and finds out what height it hits
//			  the map at.
/////////////////////////////////////////////////////////////////////////////////

bool CTaskVehicleFlyDirection::FindHeightForVerticalAngle(CPlane *pPlane, float Angle, float Orientation, float *pResult)
{
#define LOOKAHEADFORPLANE (200.0f)
#define PREDICTIONTIMEFORMOUNTAINAVOIDANCE (1.0f)	// About 1 sec
	Vector3 Pos = VEC3V_TO_VECTOR3(pPlane->GetTransform().GetPosition()) + PREDICTIONTIMEFORMOUNTAINAVOIDANCE * pPlane->GetVelocity();
	Vector3 Forward = Vector3(rage::Cosf(Orientation), rage::Sinf(Orientation), 0.0f);
	Forward.z = 0.0f;
	Forward.Normalize();
	Vector3 Dir = Forward * rage::Cosf(Angle) + Vector3(0.0f, 0.0f, 1.0f) * rage::Sinf(Angle);
	Vector3 TargetPos = Pos + LOOKAHEADFORPLANE * Dir;

	WorldProbe::CShapeTestProbeDesc probeDesc;
	WorldProbe::CShapeTestFixedResults<> probeResult;
	probeDesc.SetResultsStructure(&probeResult);
	probeDesc.SetStartAndEnd(Pos, TargetPos);
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
	if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
	{
		*pResult = rage::Max(0.0f, probeResult[0].GetHitPosition().z); // Assume water level of 0.0 globally
		return true;
	}
	return false;
}



//////////////////////////////////////////////////////////////////

#if !__FINAL
const char * CTaskVehicleFlyDirection::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_FlyDirection&&iState<=State_Exit);
	static const char* aStateNames[] = 
	{
		"State_FlyDirection",
		"State_Exit"
	};

	return aStateNames[iState];
}
#endif

//////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskVehicleRevEngine::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.

	FSM_Begin
		// Initial state
		FSM_State(State_Rev)
			FSM_OnUpdate
				return Rev_OnUpdate(pVehicle);
			FSM_OnExit
				Rev_OnExit(pVehicle);
		// Quit
		FSM_State(State_Exit)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}


//////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehicleRevEngine::Rev_OnUpdate				(CVehicle* pVehicle)
{
	pVehicle->SetThrottle(1.0f);
	pVehicle->SetHandBrake(1.0f);
	pVehicle->SetSteerAngle(0.0f);

	pVehicle->GetIntelligence()->UpdateCarHasReasonToBeStopped();

	if(NetworkInterface::GetSyncedTimeInMilliseconds() > m_tempActionFinishTimeMs)
	{
		SetState(State_Exit);
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////

void CTaskVehicleRevEngine::Rev_OnExit(CVehicle* pVehicle)
{
	// GTAV - B*1754978 - If we're in flight we don't want the afterburner effect to cut out
	// for 2 frames, one of them is caused by this setting it on exit, so set the throttle to 1
	if( pVehicle->GetVehicleType() == VEHICLE_TYPE_PLANE &&
		pVehicle->IsDriverAPlayer() &&
		pVehicle->IsInAir() )
	{
		CPed* pPlayerPed = pVehicle->GetDriver();
		CControl *pControl = pPlayerPed->GetControlFromPlayer();

		if( pControl )
		{
			pVehicle->SetThrottle( 1.0f );
		}
	}
	else
	{
		pVehicle->SetThrottle(0.0f);
	}
}

//////////////////////////////////////////////////////////////////

#if !__FINAL
const char * CTaskVehicleRevEngine::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_Rev&&iState<=State_Exit);
	static const char* aStateNames[] = 
	{
		"State_Rev",
		"State_Exit"
	};

	return aStateNames[iState];
}
#endif

//////////////////////////////////////////////////////////////////


#if !__FINAL
const char * CTaskVehicleSurfaceInSubmarine::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_Surface&&iState<=State_Exit);
	static const char* aStateNames[] = 
	{
		"State_Surface",
		"State_Exit"
	};

	return aStateNames[iState];
}
#endif

//////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskVehicleSurfaceInSubmarine::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.

	FSM_Begin
		// Initial state
		FSM_State(State_Surface)
			FSM_OnUpdate
				return Surface_OnUpdate(pVehicle);
		// Quit
		FSM_State(State_Exit)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

aiTask::FSM_Return	CTaskVehicleSurfaceInSubmarine::Surface_OnUpdate(CVehicle* pVehicle)
{
	CSubmarineHandling* pSubHandling = pVehicle->GetSubHandling();

	if (!aiVerify(pSubHandling))
	{
		SetState(State_Exit);
		return FSM_Continue;
	}

	pSubHandling->SetDiveControl(1.0f);
	pSubHandling->SetPitchControl(0.0f);
	pSubHandling->SetYawControl(0.0f);
	pVehicle->SetThrottle(0.0f);

	if(NetworkInterface::GetSyncedTimeInMilliseconds() > m_tempActionFinishTimeMs)
	{
		SetState(State_Exit);
		return FSM_Continue;
	}

	return FSM_Continue;
}




