#include "TaskVehicleDeadDriver.h"

// Game headers
#include "scene/world/GameWorld.h"
#include "Vehicles/vehicle.h"
#include "Vehicles/Planes.h"
#include "Vehicles/heli.h"
#include "VehicleAi\task\TaskVehicleFlying.h"
#include "VehicleAi\VehicleIntelligence.h"
#include "Peds\ped.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

CTaskVehicleDeadDriver::Tunables CTaskVehicleDeadDriver::sm_Tunables;

IMPLEMENT_VEHICLE_AI_TASK_TUNABLES(CTaskVehicleDeadDriver, 0x4772f3d6);

///////////////////////////////////////////////////////////////////////////

CTaskVehicleDeadDriver::CTaskVehicleDeadDriver(CEntity *pCulprit)
: m_DeathControls()
, m_uFlags(0)
, m_fTimeAfterDeath(0.0f)
, m_pCulprit(pCulprit)
{
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_DEAD_DRIVER);
	m_uSwerveTime = (u32)(sm_Tunables.m_SwerveTime * 1000.0f);
}


///////////////////////////////////////////////////////////////////////////

CTaskVehicleDeadDriver::~CTaskVehicleDeadDriver()
{

}

///////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskVehicleDeadDriver::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.

	FSM_Begin
		// Dead state
		FSM_State(State_Dead)
			FSM_OnUpdate
				return Dead_OnUpdate(pVehicle);

		// Crash state
		FSM_State(State_Crash)
			FSM_OnUpdate
				return Crash_OnUpdate(pVehicle);

		// exit state
		FSM_State(State_Exit)
			FSM_OnUpdate
			return FSM_Quit;
	FSM_End
}


///////////////////////////////////////////////////////////////////////////
const float cfSlowSpeedThreshold = 5.0f;

CTask::FSM_Return	CTaskVehicleDeadDriver::Dead_OnUpdate				(CVehicle* pVehicle)
{
	ControlAIVeh_DeadDriver(pVehicle, &m_vehicleControls);

	// The values we found may be completely different from the ones last frame.
	// The following function smooths out the values and in some cases clips them.
	bool bGoingSlowly = (pVehicle->GetVelocity().Mag2() <(cfSlowSpeedThreshold*cfSlowSpeedThreshold));
	HumaniseCarControlInput( pVehicle, &m_vehicleControls, false, bGoingSlowly);

	m_fTimeAfterDeath += fwTimer::GetTimeStep();

	return FSM_Continue;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : ControlAIVeh_DeadDriver
// PURPOSE :  If the driver of this car is dead we pick a random behaviour
/////////////////////////////////////////////////////////////////////////////////
extern float fDeadDriverCrashingHeight; // from Heli.cpp
dev_float dfBlimpDeadDriverThrottle = 1.0f;
dev_float dfBlimpDeadDriverPitch = 0.25f;
dev_float dfBlimpDeadDriverRoll = 0.25f;
dev_float dfBlimpDeadDriverYaw = 0.25f;
void CTaskVehicleDeadDriver::ControlAIVeh_DeadDriver(CVehicle* pVeh, CVehControls* pVehControls)
{
	Assertf(pVeh, "ControlAIVeh_DeadDriver expected a valid set vehicle.");
	Assertf(pVehControls, "ControlAIVeh_DeadDriver expected a valid set of vehicle controls.");
	switch(pVeh->GetVehicleType())
	{
	case VEHICLE_TYPE_PLANE:
        if(((CPlane *)pVeh)->IsInAir())
        {
		    SetState(State_Crash);
        }
		else
		{
			CPlane *pPlane = (CPlane *)pVeh;
			pPlane->SetPitchControl(0.0f);
			pPlane->SetYawControl(0.0f);
			pPlane->SetRollControl(0.0f);
			pPlane->SetPitchControl(0.0f);
			pPlane->SetThrottleControl(0.0f);
			pPlane->SetAirBrakeControl(1.0f);
			SetState(State_Exit);
		}
		return;
	case VEHICLE_TYPE_BLIMP:
		{
			CBlimp *pBlimp =(CBlimp *)pVeh;
			pBlimp->SetPitchControl(fwRandom::GetRandomNumberInRange(-dfBlimpDeadDriverPitch, dfBlimpDeadDriverPitch));
			pBlimp->SetYawControl(fwRandom::GetRandomNumberInRange(-dfBlimpDeadDriverYaw, dfBlimpDeadDriverYaw));
			pBlimp->SetRollControl(fwRandom::GetRandomNumberInRange(-dfBlimpDeadDriverRoll, dfBlimpDeadDriverRoll));
			pBlimp->SetThrottleControl(dfBlimpDeadDriverThrottle);

			Vector3 vStart = VEC3V_TO_VECTOR3(pVeh->GetVehiclePosition());
			Vector3 vEnd = vStart;
			vEnd.z += pVeh->GetBoundingBoxMin().z - fDeadDriverCrashingHeight;
			WorldProbe::CShapeTestHitPoint probeHitPoint;
			WorldProbe::CShapeTestResults probeResults(probeHitPoint);
			WorldProbe::CShapeTestProbeDesc probeDesc;
			probeDesc.SetStartAndEnd(vStart, vEnd);
			probeDesc.SetResultsStructure(&probeResults);
			probeDesc.SetExcludeEntity(pVeh);
			probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_VEHICLE);
			probeDesc.SetIsDirected(true);

			if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
			{
				SetState(State_Exit);
			}
			else
			{
				SetState(State_Crash);
			}
			return;
		}
	case VEHICLE_TYPE_HELI:
		if(((CHeli *)pVeh)->GetMainRotorSpeed() > 0.1f && ((CHeli *)pVeh)->HasContactWheels() == false)
		{
			Vector3 vStart = VEC3V_TO_VECTOR3(pVeh->GetVehiclePosition());
			Vector3 vEnd = vStart;
			vEnd.z += pVeh->GetBoundingBoxMin().z - fDeadDriverCrashingHeight;
			WorldProbe::CShapeTestHitPoint probeHitPoint;
			WorldProbe::CShapeTestResults probeResults(probeHitPoint);
			WorldProbe::CShapeTestProbeDesc probeDesc;
			probeDesc.SetStartAndEnd(vStart, vEnd);
			probeDesc.SetResultsStructure(&probeResults);
			probeDesc.SetExcludeEntity(pVeh);
			probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_VEHICLE);
			probeDesc.SetIsDirected(true);

			// If heli is close to ground, just make it falling without blowing up
			if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
			{
				CHeli *pHeli =(CHeli *)pVeh;
				pHeli->SetPitchControl(0.0f);
				pHeli->SetYawControl(0.0f);
				pHeli->SetRollControl(0.0f);
				pHeli->SetThrottleControl(0.0f);
				SetState(State_Exit);
			}
			else
			{
				SetState(State_Crash);
			}
		}
		else
		{
			// If the heli has landed and the engine is slowing down the heli will just continue to sit.
			CHeli *pHeli =(CHeli *)pVeh;
			pHeli->SetPitchControl(0.0f);
			pHeli->SetYawControl(0.0f);
			pHeli->SetRollControl(0.0f);
			pHeli->SetThrottleControl(0.0f);
		}
		return;
	case VEHICLE_TYPE_BIKE:
		pVehControls->SetSteerAngle(0.0f);
		pVehControls->SetThrottle(0.0f);
		pVehControls->SetBrake(0.05f);
		pVehControls->SetHandBrake(false);
		return;
	default:
		break;
	}

	pVehControls->SetSteerAngle(0.0f);
	pVehControls->SetThrottle(0.0f);
	pVehControls->SetBrake(HIGHEST_BRAKING_WITHOUT_LIGHT);
	pVehControls->SetHandBrake(false);

	if(pVeh->IsDummy())
	{
		//Check if we have processed the death controls.
		if(m_uFlags.IsFlagSet(VDDF_DeathControlsAreValid))
		{
			//Do a hard brake.  At this point, we don't know where the now dummied vehicle will end up.
			//It is best to just stop it, before it goes through geometry.
			pVehControls->SetBrake(1.0f);
			pVehControls->SetHandBrake(true);
		}

		return;
	}

	if(pVeh->m_nPhysicalFlags.bRenderScorched)
	{
		return;
	}

	// Mission peds don't do weird things as it can be confusing.
	if(pVeh->PopTypeIsMission())
	{
		return;
	}

	if(pVeh->GetDriver())
	{
		if(pVeh->GetDriver()->PopTypeIsMission()) return;
		if(pVeh->GetDriver()->IsAPlayerPed()) return;

		static dev_float sf_MinImpulseMagToStop = 2.0f;
		if(pVeh->GetFrameCollisionHistory() && pVeh->GetFrameCollisionHistory()->GetCollisionImpulseMagSum() > sf_MinImpulseMagToStop) m_uSwerveTime = 0;
		if(fwTimer::GetTimeInMilliseconds() > pVeh->GetDriver()->GetTimeOfDeath() + m_uSwerveTime) return;
	}
	
	// switch this off in MP, when vehicles migrate they will start up a new dead driver task that may generate different random controls
	if (!pVeh->GetNetworkObject())
	{
		//Check if we are within the distance threshold.
		static dev_float sfMaxDistance = 150.0f;
		ScalarV scDistSq = DistSquared(pVeh->GetTransform().GetPosition(), VECTOR3_TO_VEC3V(CGameWorld::FindLocalPlayerCoors()));
		ScalarV scMaxDistSq = ScalarVFromF32(square(sfMaxDistance));
		if(IsLessThanAll(scDistSq, scMaxDistSq))
		{
			//Check if the death controls are valid.
			if(!m_uFlags.IsFlagSet(VDDF_DeathControlsAreValid))
			{
				//Generate the death controls.
				GenerateDeathControls(pVehControls);
				
				//Note that the death controls are now valid.
				m_uFlags.SetFlag(VDDF_DeathControlsAreValid);
			}
			
			//Copy the death controls.
			pVehControls->SetSteerAngle(m_DeathControls.GetSteerAngle());
			pVehControls->SetThrottle(m_DeathControls.GetThrottle());
			pVehControls->SetBrake(m_DeathControls.GetBrake());
			pVehControls->SetHandBrake(m_DeathControls.GetHandBrake());

			//if we don't have an engine, don't do anything to the throttle, but everything
			//else is fine
			if (!pVeh->IsEngineOn())
			{
				pVehControls->SetThrottle(0.0f);
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskVehicleDeadDriver::Crash_OnUpdate(CVehicle* pVehicle)
{
	if( !pVehicle->IsNetworkClone() )
	{
		// Create a crash task if one doesn't already exist
		CTaskVehicleCrash *pCarTask = rage_new CTaskVehicleCrash(m_pCulprit);

		pCarTask->SetCrashFlag(CTaskVehicleCrash::CF_BlowUpInstantly, false);
		pCarTask->SetCrashFlag(CTaskVehicleCrash::CF_InACutscene, false);
		pCarTask->SetCrashFlag(CTaskVehicleCrash::CF_AddExplosion, true);

		pVehicle->GetIntelligence()->AddTask(VEHICLE_TASK_TREE_PRIMARY, pCarTask, VEHICLE_TASK_PRIORITY_CRASH);
	}

	return FSM_Quit;
}

///////////////////////////////////////////////////////////////////////////

void CTaskVehicleDeadDriver::GenerateDeathControls(CVehControls* pVehControls)
{
	//Set the steer angle.
	switch(sm_Tunables.m_SteerAngleControl)
	{
		case SAC_Minimum:
		{
			m_DeathControls.SetSteerAngle(sm_Tunables.m_MinSteerAngle);
			break;
		}
		case SAC_Maximum:
		{
			m_DeathControls.SetSteerAngle(sm_Tunables.m_MaxSteerAngle);
			break;
		}
		case SAC_Randomize:
		{
			m_DeathControls.SetSteerAngle(fwRandom::GetRandomNumberInRange(sm_Tunables.m_MinSteerAngle, sm_Tunables.m_MaxSteerAngle));
			break;
		}
		case SAC_Retain:
		default:
		{
			m_DeathControls.SetSteerAngle(pVehControls->GetSteerAngle());
			break;
		}
	}
	
	//Set the throttle.
	switch(sm_Tunables.m_ThrottleControl)
	{
		case TC_Minimum:
		{
			m_DeathControls.SetThrottle(sm_Tunables.m_MinThrottle);
			break;
		}
		case TC_Maximum:
		{
			m_DeathControls.SetThrottle(sm_Tunables.m_MaxThrottle);
			break;
		}
		case TC_Randomize:
		{
			m_DeathControls.SetThrottle(fwRandom::GetRandomNumberInRange(sm_Tunables.m_MinThrottle, sm_Tunables.m_MaxThrottle));
			break;
		}
		case TC_Retain:
		default:
		{
			m_DeathControls.SetThrottle(pVehControls->GetThrottle());
			break;
		}
	}
	
	//Set the brake.
	switch(sm_Tunables.m_BrakeControl)
	{
		case BC_Minimum:
		{
			m_DeathControls.SetBrake(sm_Tunables.m_MinBrake);
			break;
		}
		case BC_Maximum:
		{
			m_DeathControls.SetBrake(sm_Tunables.m_MaxBrake);
			break;
		}
		case BC_Randomize:
		{
			m_DeathControls.SetBrake(fwRandom::GetRandomNumberInRange(sm_Tunables.m_MinBrake, sm_Tunables.m_MaxBrake));
			break;
		}
		case BC_Retain:
		default:
		{
			m_DeathControls.SetBrake(pVehControls->GetBrake());
			break;
		}
	}
	
	//Set the hand brake.
	switch(sm_Tunables.m_HandBrakeControl)
	{
		case HBC_Yes:
		{
			m_DeathControls.SetHandBrake(true);
			break;
		}
		case HBC_No:
		{
			m_DeathControls.SetHandBrake(false);
			break;
		}
		case HBC_Randomize:
		{
			m_DeathControls.SetHandBrake(fwRandom::GetRandomTrueFalse());
			break;
		}
		case HBC_Retain:
		default:
		{
			m_DeathControls.SetHandBrake(pVehControls->GetHandBrake());
			break;
		}
	}
}

#if !__FINAL
const char * CTaskVehicleDeadDriver::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_Dead&&iState<=State_Exit);
	static const char* aStateNames[] = 
	{
		"State_Dead",
		"State_Crash",
		"State_Exit"
	};

	return aStateNames[iState];
}
#endif
