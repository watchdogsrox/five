#include "TaskVehicleMissionBase.h"

// Framework headers
#include "math/vecmath.h"

// Game headers
#include "Vehicles/vehicle.h"
#include "VehicleAi/vehicleintelligence.h"
#include "VehicleAi/driverpersonality.h"
#include "debug/DebugScene.h"
#include "control/trafficLights.h"
#include "scene/world/gameWorld.h"
#include "control/record.h"
#include "peds/ped.h"
#include "peds/PedIntelligence.h"
#include "Task/Scenario/Types/TaskVehicleScenario.h"
#include "vehicleAi/task/TaskVehicleCruise.h"
#include "vehicles/vehiclepopulation.h"
#include "vehicles\virtualroad.h"


AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

FW_INSTANTIATE_CLASS_POOL(CTaskVehicleSerialiserBase, 1, atHashString("CTaskVehicleSerialiserBase",0x76f457ec));

CTaskVehicleMissionBase::Tunables CTaskVehicleMissionBase::sm_Tunables;
IMPLEMENT_VEHICLE_AI_TASK_TUNABLES(CTaskVehicleMissionBase, 0x3b6bb73c);

const float sVehicleMissionParams::DEFAULT_CRUISE_SPEED = 10.0f;
const float sVehicleMissionParams::DEFAULT_TARGET_REACHED_DIST = 4.0f;
const float sVehicleMissionParams::DEFAULT_SWITCH_TO_STRAIGHT_LINE_DIST	= 20.0f;
const float CTaskVehicleMissionBase::MAX_CRUISE_SPEED = 120.0f;

//////////////////////////////////////////////////////////////////////////

CTaskVehicleMissionBase::CTaskVehicleMissionBase()
{
	m_bSoftResetRequested = false;
	m_bMissionAchieved = false;
}

CTaskVehicleMissionBase::CTaskVehicleMissionBase(const sVehicleMissionParams& params)
{
	m_bSoftResetRequested = false;
	m_bMissionAchieved = false;
	m_Params = params;

#if (!(HACK_GTA4) || (HACK_RDR3)) && (__ASSERT)
	if (!GetTargetEntity())
	{
		Assertf(m_Params.GetTargetPosition().x >= WORLDLIMITS_XMIN && m_Params.GetTargetPosition().x <= WORLDLIMITS_XMAX, "invalid target X position %.2f", m_Params.GetTargetPosition().x);
		Assertf(m_Params.GetTargetPosition().y >= WORLDLIMITS_YMIN && m_Params.GetTargetPosition().y <= WORLDLIMITS_YMAX, "invalid target Y position %.2f", m_Params.GetTargetPosition().y);
		Assertf(m_Params.GetTargetPosition().z >= WORLDLIMITS_ZMIN && m_Params.GetTargetPosition().z <= WORLDLIMITS_ZMAX, "invalid target Z position %.2f", m_Params.GetTargetPosition().z);
	}
#endif 

#if __ASSERT
	Assertf(m_Params.m_fCruiseSpeed >= 0.0f, "Desired cruise speed is negative! Use DF_DriveInReverse instead");
#endif //__ASSERT

#if __ASSERT
	Assertf(m_Params.m_fCruiseSpeed < MAX_CRUISE_SPEED,"Trying to set cruise speed to %.2f, but MAX allowed is %.2f", m_Params.m_fCruiseSpeed, MAX_CRUISE_SPEED);
#endif //__ASSERT

	m_Params.m_fCruiseSpeed = rage::Min(m_Params.m_fCruiseSpeed, MAX_CRUISE_SPEED);

}

//////////////////////////////////////////////////////////////////////////

CTaskVehicleMissionBase::~CTaskVehicleMissionBase()
{

}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindTargetCoors
// PURPOSE :  Finds the target coors for this mission. If there is an entity
//			  it will be those coordinates. If there isn't it will be the actual
//			  coordinates.
// RETURNS :
/////////////////////////////////////////////////////////////////////////////////
void CTaskVehicleMissionBase::FindTargetCoors(const CVehicle *pVeh, sVehicleMissionParams &params) const
{
	Vector3 TargetCoors;
	FindTargetCoors(pVeh, TargetCoors);
	params.SetTargetPosition(TargetCoors);
}

void CTaskVehicleMissionBase::FindTargetCoors(const CVehicle* UNUSED_PARAM(pVeh), Vector3 &TargetCoors) const
{
	if (GetTargetEntity() && !IsDrivingFlagSet(DF_TargetPositionOverridesEntity))
	{
		TargetCoors = VEC3V_TO_VECTOR3(GetTargetEntity()->GetTransform().GetPosition());

		Assertf(TargetCoors.x >= WORLDLIMITS_XMIN && TargetCoors.x <= WORLDLIMITS_XMAX, "Target Entity has an invalid target X position %.2f on a %s", TargetCoors.x, GetTaskName());
		Assertf(TargetCoors.y >= WORLDLIMITS_YMIN && TargetCoors.y <= WORLDLIMITS_YMAX, "Target Entity has an invalid target Y position %.2f on a %s", TargetCoors.y, GetTaskName());
		Assertf(TargetCoors.z >= WORLDLIMITS_ZMIN && TargetCoors.z <= WORLDLIMITS_ZMAX, "Target Entity has an invalid target Z position %.2f on a %s", TargetCoors.z, GetTaskName());
	}
	else
	{
		TargetCoors = m_Params.GetTargetPosition();

		Assertf(TargetCoors.x >= WORLDLIMITS_XMIN && TargetCoors.x <= WORLDLIMITS_XMAX, "Target has invalid target X position %.2f on a %s", TargetCoors.x, GetTaskName());
		Assertf(TargetCoors.y >= WORLDLIMITS_YMIN && TargetCoors.y <= WORLDLIMITS_YMAX, "Target has invalid target Y position %.2f on a %s", TargetCoors.y, GetTaskName());
		Assertf(TargetCoors.z >= WORLDLIMITS_ZMIN && TargetCoors.z <= WORLDLIMITS_ZMAX, "Target has invalid target Z position %.2f on a %s", TargetCoors.z, GetTaskName());
	}
}


void CTaskVehicleMissionBase::SetTargetArriveDist(float arrivalDist)
{
	m_Params.m_fTargetArriveDist = arrivalDist;
}

//////////////////////////////////////////////////////////////////////////

void CTaskVehicleMissionBase::SetCruiseSpeed(float cruiseSpeed)
{
	if (m_Params.m_fMaxCruiseSpeed > 0.0f)
	{
		cruiseSpeed = rage::Min(cruiseSpeed, m_Params.m_fMaxCruiseSpeed);
	}

	Assertf(cruiseSpeed >= 0.0f, "Desired cruise speed is negative! Use DF_DriveInReverse instead");

	Assertf(cruiseSpeed < MAX_CRUISE_SPEED,"Trying to set cruise speed to %.2f, but MAX allowed is %.2f", cruiseSpeed, MAX_CRUISE_SPEED);
	cruiseSpeed = rage::Min(cruiseSpeed, MAX_CRUISE_SPEED);

	//if(m_Params.m_fCruiseSpeed != cruiseSpeed)
	//{
		m_Params.m_fCruiseSpeed = cruiseSpeed;
	//}
}

//////////////////////////////////////////////////////////////////////////
void CTaskVehicleMissionBase::SetMaxCruiseSpeed(float maxCruiseSpeed)
{
	//if(m_Params.m_fMaxCruiseSpeed != maxCruiseSpeed)
	//{
		m_Params.m_fMaxCruiseSpeed = maxCruiseSpeed;
	//}
}

//////////////////////////////////////////////////////////////////////////

const CPhysical* CTaskVehicleMissionBase::GetTargetEntity() const
{
	return static_cast<const CPhysical*>(m_Params.GetTargetEntity().GetEntity());
}


///////////////////////////////////////////////////////////////////////////
// set the target position to 0,0,0 if no target is set
///////////////////////////////////////////////////////////////////////////
void	CTaskVehicleMissionBase::SetTargetPosition(const Vector3 *pTargetPosition)
{
	if( pTargetPosition )
	{
#if __ASSERT
		//SetTargetPosition can be called before the m_pEntity pointer is fixed up
		if (GetEntity())
		{
			Assertf(pTargetPosition->x >= WORLDLIMITS_XMIN && pTargetPosition->x <= WORLDLIMITS_XMAX, "Trying to set an invalid target X position %.2f on a %s. Veh position: %.2f, %.2f, %.2f"
				, pTargetPosition->x, GetTaskName(), GetVehicle()->GetVehiclePosition().GetXf(), GetVehicle()->GetVehiclePosition().GetYf(), GetVehicle()->GetVehiclePosition().GetZf());
			Assertf(pTargetPosition->y >= WORLDLIMITS_YMIN && pTargetPosition->y <= WORLDLIMITS_YMAX, "Trying to set an invalid target Y position %.2f on a %s. Veh position: %.2f, %.2f, %.2f"
				, pTargetPosition->y, GetTaskName(), GetVehicle()->GetVehiclePosition().GetXf(), GetVehicle()->GetVehiclePosition().GetYf(), GetVehicle()->GetVehiclePosition().GetZf());
			Assertf(pTargetPosition->z >= WORLDLIMITS_ZMIN && pTargetPosition->z <= WORLDLIMITS_ZMAX, "Trying to set an invalid target Z position %.2f on a %s. Veh position: %.2f, %.2f, %.2f"
				, pTargetPosition->z, GetTaskName(), GetVehicle()->GetVehiclePosition().GetXf(), GetVehicle()->GetVehiclePosition().GetYf(), GetVehicle()->GetVehiclePosition().GetZf());
		}
#endif //__ASSERT

		m_Params.SetTargetPosition(*pTargetPosition);
	}
	else
	{
		m_Params.ClearTargetPosition();
	}
}


//////////////////////////////////////////////////////////////////////////

const Vector3 *CTaskVehicleMissionBase::GetTargetPosition()
{
	return &m_Params.GetTargetPosition();
}

bool CTaskVehicleMissionBase::HasMovingTarget() const
{
	return GetTargetEntity() && GetTargetEntity()->GetIsPhysical();
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindTargetVehicle
// PURPOSE :  Finds the target vehicle for this mission. If the target is a ped in a
//			  vehicle this is what will be returned.
// RETURNS :
/////////////////////////////////////////////////////////////////////////////////
CVehicle *CTaskVehicleMissionBase::FindTargetVehicle() const
{
	const CPhysical* pTargetEntity = GetTargetEntity();

	if (pTargetEntity)
	{
		if (pTargetEntity->GetIsTypeVehicle())
		{
			return (CVehicle *)pTargetEntity;
		}
		if (pTargetEntity->GetIsTypePed())
		{
			CPed *pPed = (CPed*)pTargetEntity;
			if (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
			{
				return pPed->GetMyVehicle();
			}
		}
	}
	return NULL;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindVehicleWithPhysical
// PURPOSE :  If the physical passed in is a ped; this function returns the car he
//			  is in. Otherwise the same physical is returned.
/////////////////////////////////////////////////////////////////////////////////
const CPhysical *CTaskVehicleMissionBase::FindVehicleWithPhysical(const CPhysical *pPhysical) const
{
	Assert(pPhysical);

	if(pPhysical->GetIsTypePed())
	{
		CPed *pPed =(CPed *)pPhysical;

		if(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pPed->GetMyVehicle())
		{
			return pPed->GetMyVehicle();
		}
	}
	return pPhysical;
}


////////////////////////////////////////////////////
// FUNCTION: HumaniseCarControlInput
// Takes the controls the AI has decided on and smooths them out.
// In the case of conservative driving they can also be clipped.
////////////////////////////////////////////////////
void CTaskVehicleMissionBase::HumaniseCarControlInput(CVehicle* pVeh, CVehControls* pVehControls, bool bConservativeDriving, bool bGoingSlowly)
{
	Assertf(pVeh, "HumaniseCarControlInput expected a valid set vehicle.");
	Assertf(pVehControls, "HumaniseCarControlInput expected a valid set of vehicle controls.");
	Assertf(pVeh->GetSteerAngle() == pVeh->GetSteerAngle(), "Actual vehicle steer angle is NaN");

	if(!bConservativeDriving && !bGoingSlowly)
	{
		pVeh->SetSteerAngle(pVehControls->m_steerAngle);
		pVeh->SetThrottle(pVehControls->m_throttle);
		pVeh->SetBrake(pVehControls->m_brake);
		pVeh->SetHandBrake(pVehControls->m_handBrake);
		return;
	}

	// Humanize the steering
	const float steerAngleDiff = pVehControls->m_steerAngle - pVeh->GetSteerAngle();
	const float maxsteerAngleChange = ((bConservativeDriving)?2.0f:4.0f) * fwTimer::GetTimeStep();
	if(rage::Abs(steerAngleDiff) >= maxsteerAngleChange)
	{
		if(steerAngleDiff > 0.0f)
		{
			pVehControls->SetSteerAngle( pVeh->GetSteerAngle() + maxsteerAngleChange );
		}
		else
		{
			pVehControls->SetSteerAngle( pVeh->GetSteerAngle() - maxsteerAngleChange );
		}
	}


	// Check if we should bother humanizing the gas and brakes.
	if(bConservativeDriving)
	{
		// Humanize the brakes.
		const float brakeDiff = pVehControls->m_brake - pVeh->GetBrake();
		const float maxBrakeChange = ((brakeDiff > 0.0f)?10.0f:1.0f) * fwTimer::GetTimeStep();
		if(rage::Abs(brakeDiff) >= maxBrakeChange)
		{
			if(brakeDiff > 0.0f)
			{
				pVehControls->m_brake = pVeh->GetBrake() + maxBrakeChange;
			}
			else
			{
				pVehControls->m_brake = pVeh->GetBrake() - maxBrakeChange;
			}
		}

		if(pVehControls->m_throttle > 0.0f)//only worry about fiddling with the throttle if we're not reversing
		{
			// Humanize the gas.
			// Sensible cars don't go flat out.
			const float	gassDiff = pVehControls->m_throttle - pVeh->GetThrottle();
			const  float maxGasChange = ((gassDiff > 0.0f)?1.0f:10.0f) * fwTimer::GetTimeStep();
			if(rage::Abs(gassDiff) >= maxGasChange)
			{
				if(gassDiff > 0.0f)
				{
					pVehControls->m_throttle = pVeh->GetThrottle() + maxGasChange;
				}
				else
				{
					pVehControls->m_throttle = pVeh->GetThrottle() - maxGasChange;
				}
			}

			// Add a little bit to the max gas allowed if we're on an upward slope.
			float maxGas = CDriverPersonality::FindMaxAcceleratorInput(pVeh->GetDriver(), pVeh);
			maxGas += rage::Max(0.0f, (4.0f * pVeh->GetTransform().GetB().GetZf()));
			pVehControls->m_throttle = rage::Min(pVehControls->m_throttle, maxGas);
		}
	}

	pVeh->SetSteerAngle(pVehControls->m_steerAngle);
	pVeh->SetThrottle(pVehControls->m_throttle);
	pVeh->SetBrake(pVehControls->m_brake);
	pVeh->SetHandBrake(pVehControls->m_handBrake);
}

////////////////////////////////////////////////////
// FUNCTION: ReadTaskData
// Reads the task's data from a network update
////////////////////////////////////////////////////
void CTaskVehicleMissionBase::ReadTaskData(datBitBuffer &messageBuffer)
{
	if (IsSyncedAcrossNetwork())
	{
		CTaskVehicleSerialiserBase* pTaskSerialiser = GetTaskSerialiser();

		if (pTaskSerialiser)
		{
			pTaskSerialiser->ReadTaskData(this, messageBuffer);
			delete pTaskSerialiser;
		}
	}
}

////////////////////////////////////////////////////
// FUNCTION: WriteTaskData
// Writes the task's data for a network update
////////////////////////////////////////////////////
void CTaskVehicleMissionBase::WriteTaskData(datBitBuffer &messageBuffer)
{
	if (IsSyncedAcrossNetwork())
	{
		CTaskVehicleSerialiserBase* pTaskSerialiser = GetTaskSerialiser();

		if (pTaskSerialiser)
		{
			pTaskSerialiser->WriteTaskData(this, messageBuffer);
			delete pTaskSerialiser;
		}
	}
}

////////////////////////////////////////////////////
// FUNCTION: LogTaskData
// Logs the task's data for a network update
////////////////////////////////////////////////////
void CTaskVehicleMissionBase::LogTaskData(netLoggingInterface &log)
{
	if (IsSyncedAcrossNetwork())
	{
		CTaskVehicleSerialiserBase* pTaskSerialiser = GetTaskSerialiser();

		if (pTaskSerialiser)
		{
			pTaskSerialiser->LogTaskData(this, log);
			delete pTaskSerialiser;
		}
	}
}

////////////////////////////////////////////////////
// FUNCTION: ReadTaskMigrationData
// Reads the task's migration data from a network migration event
////////////////////////////////////////////////////
void CTaskVehicleMissionBase::ReadTaskMigrationData(datBitBuffer &messageBuffer)
{
	if (IsSyncedAcrossNetwork())
	{
		CTaskVehicleSerialiserBase* pTaskSerialiser = GetTaskSerialiser();

		if (pTaskSerialiser)
		{
			pTaskSerialiser->ReadTaskMigrationData(this, messageBuffer);
			delete pTaskSerialiser;
		}
	}
}

////////////////////////////////////////////////////
// FUNCTION: WriteTaskMigrationData
// Writes the task's migration data for a network migration event
////////////////////////////////////////////////////
void CTaskVehicleMissionBase::WriteTaskMigrationData(datBitBuffer &messageBuffer)
{
	if (IsSyncedAcrossNetwork())
	{
		CTaskVehicleSerialiserBase* pTaskSerialiser = GetTaskSerialiser();

		if (pTaskSerialiser)
		{
			pTaskSerialiser->WriteTaskMigrationData(this, messageBuffer);
			delete pTaskSerialiser;
		}
	}
}

////////////////////////////////////////////////////
// FUNCTION: LogTaskMigrationData
// Logs the task's migration data for a network migration event
////////////////////////////////////////////////////
void CTaskVehicleMissionBase::LogTaskMigrationData(netLoggingInterface &log)
{
	if (IsSyncedAcrossNetwork())
	{
		CTaskVehicleSerialiserBase* pTaskSerialiser = GetTaskSerialiser();

		if (pTaskSerialiser)
		{
			pTaskSerialiser->LogTaskMigrationData(this, log);
			delete pTaskSerialiser;
		}
	}
}

void CTaskVehicleMissionBase::SetupAfterMigration(CVehicle *pVehicle)
{
    if(taskVerifyf(pVehicle, "Invalid vehicle!") &&
       taskVerifyf(pVehicle->GetNetworkObject(), "Calling SetupAfterMigration on a non-networked vehicle!"))
    {
		if(GetTaskType() != CTaskTypes::TASK_VEHICLE_CRUISE_NEW)
		{
			CopyNodeList(&static_cast<CNetObjVehicle*>(pVehicle->GetNetworkObject())->GetVehicleNodeList());

			CVehicleNodeList *pNodeList = GetNodeList();

			if(pNodeList)
			{
				CVehicleFollowRouteHelper *pFollowRouteHelper = const_cast<CVehicleFollowRouteHelper *>(GetFollowRouteHelper());

				if(pFollowRouteHelper)
				{
					bool bUpdateLanesForTurns = false;
					const CPathNodeRouteSearchHelper* pRouteSearchHelper = GetRouteSearchHelper();
					if (pRouteSearchHelper && (!pRouteSearchHelper->IsDoingWanderRoute() || pRouteSearchHelper->ShouldAvoidTurns()))
					{
						bUpdateLanesForTurns = true;
					}

					pFollowRouteHelper->ConstructFromNodeList(pVehicle, *pNodeList, bUpdateLanesForTurns);

					// ensure we have constructed a valid route
					//taskAssertf(pFollowRouteHelper->GetNumPoints() > 0 || pNodeList->FindNumPathNodes() == 0
					//	, "Route with no points generated, but we had some in the NodeList!");

#if __ASSERT
					for(unsigned index = 0; index < pFollowRouteHelper->GetNumPoints(); index++)
					{
						const CRoutePoint& rPoint1 = pFollowRouteHelper->GetRoutePoints()[index];
						Vector3 node1Coors = VEC3V_TO_VECTOR3(rPoint1.GetPosition());
						Assertf(node1Coors.x >= WORLDLIMITS_XMIN && node1Coors.x <= WORLDLIMITS_XMAX &&
							node1Coors.y >= WORLDLIMITS_YMIN && node1Coors.y <= WORLDLIMITS_YMAX &&
							node1Coors.z >= WORLDLIMITS_ZMIN && node1Coors.z <= WORLDLIMITS_ZMAX,
							"Route generated with invalid points!");
					}
#endif // __ASSERT
				}
			}
		}
    }
}

bool CTaskVehicleMissionBase::GetShouldObeyTrafficLights()
{
	return IsDrivingFlagSet(DF_StopAtLights);
}

bool CTaskVehicleMissionBase::ShouldTurnOffEngineAndLights(const CVehicle* pVehicle)
{
	bool bTurnOffEngineAndLights = true;
	if (pVehicle && pVehicle->GetDriver() && pVehicle->GetDriver()->GetPedIntelligence())
	{
		CTaskUseVehicleScenario* pTask = static_cast<CTaskUseVehicleScenario*>(pVehicle->GetDriver()->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_USE_VEHICLE_SCENARIO));
		if (pTask)
		{
			CScenarioPoint* pPoint = pTask->GetScenarioPoint();
			if (pPoint)
			{
				float fTimeUntilPedLeaves = pPoint->GetTimeTillPedLeaves();
				if (fTimeUntilPedLeaves >= 0.0f && fTimeUntilPedLeaves < sm_Tunables.m_MinTimeToKeepEngineAndLightsOnWhileParked)
				{
					bTurnOffEngineAndLights = false;
				}
			}
		}
	}

	return bTurnOffEngineAndLights;
}
