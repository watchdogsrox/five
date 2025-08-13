//
// Task/Vehicle/TaskEnterVehicle.h
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

// File header
#include "Task/crimes/TaskStealVehicle.h"

// Rage Headers
#include "fwvehicleai/pathfindtypes.h"
#include "math/angmath.h"

// Game Headers
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Task/Vehicle/TaskCar.h"
#include "Task/Vehicle/TaskCarUtils.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Task/Vehicle/TaskGoToVehicleDoor.h"
#include "VehicleAi/pathfind.h"
#include "Vehicles/Bike.h"
#include "Vehicles/Train.h"
#include "Vehicles/Vehicle.h"

AI_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////

// Statics
CTaskStealVehicle::Tunables CTaskStealVehicle::ms_Tunables;

IMPLEMENT_CRIME_TASK_TUNABLES(CTaskStealVehicle, 0x5e1040b3);

////////////////////////////////////////////////////////////////////////////////

CTaskStealVehicle::CTaskStealVehicle(CVehicle* pVehicle) :
m_pVehicle(pVehicle)
{
	SetInternalTaskType(CTaskTypes::TASK_STEAL_VEHICLE);
}


////////////////////////////////////////////////////////////////////////////////

CTaskStealVehicle::~CTaskStealVehicle()
{

}

#if !__FINAL
const char * CTaskStealVehicle::GetStaticStateName( s32 iState )
{
    taskAssert(iState >= State_Stealing_Vehicle && iState <= State_Finish);

    switch (iState)
    {
        case State_Stealing_Vehicle:				return "State_Stealing_Vehicle";
		case State_Stolen_Vehicle:					return "State_Stolen_Vehicle";
        case State_Finish:							return "State_Finish";
        default: taskAssert(0); 	
    }
    return "State_Invalid";
}
#endif //!__FINAL


////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskStealVehicle::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
    FSM_Begin

        FSM_State(State_Stealing_Vehicle)
			FSM_OnEnter
				return Stealing_Vehicle_OnEnter();	
            FSM_OnUpdate
				return Stealing_Vehicle_OnUpdate();

		FSM_State(State_Stolen_Vehicle)
			FSM_OnEnter
				return Stole_Vehicle_OnEnter();
			FSM_OnUpdate
				return Stole_Vehicle_Update();

        FSM_State(State_Finish)
            FSM_OnUpdate
				return Finish_OnUpdate();	

    FSM_End
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskStealVehicle::Stealing_Vehicle_OnEnter()
{
    Assert(m_pVehicle);
    
	VehicleEnterExitFlags vehicleFlags;
	vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DontCloseDoor);
	CTaskEnterVehicle* stealVehicle = rage_new CTaskEnterVehicle(m_pVehicle, SR_Specific, m_pVehicle->GetDriverSeat(),vehicleFlags,0.0f,MOVEBLENDRATIO_WALK);
	SetNewTask( stealVehicle );
    return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskStealVehicle::Stealing_Vehicle_OnUpdate()
{
    if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
    {
        SetState(State_Stolen_Vehicle);
		return FSM_Continue;
    }

    CPed* pPed = GetPed();

    if (!m_pVehicle || !pPed)
    {
        return FSM_Quit;
    }
        

    Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
    Vector3 vVehiclePos = VEC3V_TO_VECTOR3(m_pVehicle->GetTransform().GetPosition());
    const float fDistToEntryPoint = vPedPos.Dist2(vVehiclePos);

	if ( fDistToEntryPoint > rage::square(CTaskStealVehicle::ms_Tunables.m_MaxDistanceToPursueVehicle) )
    {
        return FSM_Quit;
    }

    if ( m_pVehicle->ContainsPlayer() && !CTaskStealVehicle::ms_Tunables.m_CanStealPlayersVehicle )
    {
        return FSM_Quit;
    }

	//Get the ped to run the last x meters
	CTaskEnterVehicle* pStealVehicle  = (CTaskEnterVehicle*)GetSubTask();

	if (fDistToEntryPoint < CTaskStealVehicle::ms_Tunables.m_DistanceToRunToVehicle )
	{
		if (pStealVehicle && pStealVehicle->GetTaskType() == CTaskTypes::TASK_ENTER_VEHICLE)
		{
			CTaskGoToCarDoorAndStandStill* pGoToCarDoorAndStandStill = (CTaskGoToCarDoorAndStandStill*)pStealVehicle->GetSubTask();

			if (pGoToCarDoorAndStandStill && pGoToCarDoorAndStandStill->GetTaskType() == CTaskTypes::TASK_GO_TO_CAR_DOOR_AND_STAND_STILL)
			{		
				pGoToCarDoorAndStandStill->SetMoveBlendRatio(MOVEBLENDRATIO_RUN);
			}
		}
	}
	else
	{
		//If the vehicle has started moving then quit
		if(!m_pVehicle->m_nVehicleFlags.bIsThisAStationaryCar)
		{
			return FSM_Quit;
		}
	}

    return FSM_Continue;
}


////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskStealVehicle::Stole_Vehicle_OnEnter()
{
	CPed* pPed = GetPed();

	//Check to see if the ped is in the vehicle 
	if ( pPed->GetIsDrivingVehicle() )
	{
		sVehicleMissionParams params;
		params.m_iDrivingFlags = DMode_AvoidCars_Reckless|DF_SteerAroundPeds;
		params.m_fCruiseSpeed = 40.0f;
		CTask* pTask = CVehicleIntelligence::CreateCruiseTask(*m_pVehicle, params);
		SetNewTask( rage_new CTaskControlVehicle(m_pVehicle, pTask) );

		return FSM_Continue;
	}

	return FSM_Quit;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskStealVehicle::Stole_Vehicle_Update()
{
	CPed* pPed = GetPed();

	//Check to see if the ped is in the vehicle 
	if ( !pPed->GetIsDrivingVehicle() )
	{
		return FSM_Quit;
	}

	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		return FSM_Quit;
	}

	return FSM_Continue;
}


////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskStealVehicle::Finish_OnUpdate()
{
    return FSM_Quit;
}

////////////////////////////////////////////////////////////////////////////////
// CStealVehicleCrime
////////////////////////////////////////////////////////////////////////////////

CVehicle* CStealVehicleCrime::IsApplicable(CPed* pPed)
{
	Assert(pPed);

	//This ped is holding an object so he can't steal any vehicles
	if ( pPed->GetHeldObject(*pPed) != NULL )
	{
		return NULL;
	}

	CEntityScannerIterator nearbyVehicles = pPed->GetPedIntelligence()->GetNearbyVehicles();
	for( CEntity* pEnt = nearbyVehicles.GetFirst(); pEnt; pEnt = nearbyVehicles.GetNext())
	{
		CVehicle* pVehicle = static_cast<CVehicle*>(pEnt);
		if(!pVehicle)
		{
			continue;
		}

		if (!CTaskStealVehicle::ms_Tunables.m_CanStealParkedCars && pVehicle->m_nVehicleFlags.bIsThisAParkedCar)
		{
			continue;
		}

		if(!pVehicle->m_nVehicleFlags.bIsThisAStationaryCar)
		{
			continue;
		}

		if(!CTaskStealVehicle::ms_Tunables.m_CanStealPlayersVehicle && pVehicle->ContainsPlayer())
		{
			continue;
		}

		if(!CTaskStealVehicle::ms_Tunables.m_CanStealCarsAtLights &&
			pVehicle->HasCarStoppedBecauseOfLight() )
		{
			continue;
		}

		//Don't steal law enforcement vehicles in mp
		if(pVehicle->IsLawEnforcementVehicle() && NetworkInterface::IsGameInProgress())
		{
			continue;
		}

		if(pVehicle->PopTypeIsMission() || !pVehicle->m_nVehicleFlags.bStealable)
		{
			continue;
		}

		//Don't steal a vehicle if there is more than 1 person in it.
		if(pVehicle->GetSeatManager()->CountPedsInSeats(false) > 0)
		{
			continue;
		}

		//Distance check
		const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		const Vector3 vVehiclePos = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
		const float fDistToEntryPoint = (vPedPos - vVehiclePos).Mag2();

		if(fDistToEntryPoint > rage::square(CTaskStealVehicle::ms_Tunables.m_MaxDistanceToFindVehicle))
		{
			continue;
		}

		//Direction check - don't jack a vehicle if the ped has to turn around
		Vector3 vPedVehicle = vVehiclePos - vPedPos;
		vPedVehicle.Normalize();
		Vector3 vPedForward = VEC3V_TO_VECTOR3(pPed->GetTransform().GetForward());
		vPedForward.Normalize();
		
		float fDot = vPedForward.Dot(vPedVehicle);
		if ( fDot < 0.0f )
		{
			continue;
		}

		if (!CCarEnterExit::IsVehicleStealable(*pVehicle,*pPed,false) )
		{
			continue;
		}

		//Check we have turn on road nodes close by
		CNodeAddress closestNode = ThePaths.FindNodeClosestToCoors(vPedVehicle,15.0f ,CPathFind::IgnoreSwitchedOffNodes);
		if(closestNode.IsEmpty())
		{
			continue;
		}

		//valid car found
		return pVehicle;
	}

	// No car was found for this ped to steal
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

CTask* CStealVehicleCrime::GetPrimaryTask( CVehicle* pVehicle )
{
	CTask* pTaskToAdd = NULL;
	pTaskToAdd = rage_new CTaskStealVehicle(pVehicle);

	return pTaskToAdd;
}
