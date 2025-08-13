#include "TaskVehiclePoliceBehaviour.h"

// Game headers
#include "audio/northaudioengine.h"
#include "audio/superconductor.h"
#include "VehicleAi\VehicleIntelligence.h"
#include "VehicleAi\task\TaskVehicleCruise.h"
#include "VehicleAi\task\TaskVehicleCircle.h"
#include "VehicleAi\task\TaskVehicleBlock.h"
#include "VehicleAi\task\TaskVehicleGoTo.h"
#include "VehicleAi\task\TaskVehicleGoToHelicopter.h"
#include "VehicleAi\task\TaskVehiclePark.h"
#include "VehicleAi\task\TaskVehicleRam.h"
#include "Vehicles/vehicle.h"
#include "Vehicles/Planes.h"
#include "Vehicles/heli.h"
#include "game/wanted.h"
#include "game/modelindices.h"
#include "Peds\ped.h"
#include "Peds\PedIntelligence.h"
#include "renderer/zonecull.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////////

CTaskVehiclePoliceBehaviour::CTaskVehiclePoliceBehaviour(const sVehicleMissionParams& params) :
CTaskVehicleMissionBase(params)
{
	m_fStraightLineDist = 40.0f;
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_POLICE_BEHAVIOUR);
}


//////////////////////////////////////////////////////////////////////////////

CTaskVehiclePoliceBehaviour::~CTaskVehiclePoliceBehaviour()
{

}

//////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskVehiclePoliceBehaviour::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.

	FSM_Begin
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate(pVehicle);

		FSM_State(State_Cruise)
			FSM_OnEnter
				Cruise_OnEnter(pVehicle);
			FSM_OnUpdate
				return Cruise_OnUpdate(pVehicle);

		FSM_State(State_Ram)
			FSM_OnEnter
				Ram_OnEnter(pVehicle);
			FSM_OnUpdate
				return Ram_OnUpdate(pVehicle);

		FSM_State(State_Follow)
			FSM_OnEnter
				Follow_OnEnter(pVehicle);
			FSM_OnUpdate
				return Follow_OnUpdate(pVehicle);

		FSM_State(State_Block)
			FSM_OnEnter
				Block_OnEnter(pVehicle);
			FSM_OnUpdate
				return Block_OnUpdate(pVehicle);

		FSM_State(State_Helicopter)
			FSM_OnEnter
				Helicopter_OnEnter(pVehicle);
			FSM_OnUpdate
				return Helicopter_OnUpdate(pVehicle);

		FSM_State(State_Boat)
			FSM_OnEnter
				Boat_OnEnter(pVehicle);
			FSM_OnUpdate
				return Boat_OnUpdate(pVehicle);

		FSM_State(State_Stop)
			FSM_OnEnter
				Stop_OnEnter(pVehicle);
			FSM_OnUpdate
				return Stop_OnUpdate(pVehicle);
	FSM_End
}


//////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehiclePoliceBehaviour::Start_OnUpdate				(CVehicle* pVehicle)
{
	DeterminePoliceBehaviour(pVehicle);
	return FSM_Continue;
}


//////////////////////////////////////////////////////////////////////////////

void		CTaskVehiclePoliceBehaviour::Cruise_OnEnter(CVehicle* pVehicle)
{
	SetNewTask(CVehicleIntelligence::CreateCruiseTask(*pVehicle, m_Params));
}


//////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehiclePoliceBehaviour::Cruise_OnUpdate(CVehicle* pVehicle)
{
	DeterminePoliceBehaviour(pVehicle);
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////////

void		CTaskVehiclePoliceBehaviour::Ram_OnEnter(CVehicle* UNUSED_PARAM(pVehicle))
{
	SetNewTask(rage_new CTaskVehicleRam( m_Params ));
}


//////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehiclePoliceBehaviour::Ram_OnUpdate(CVehicle* pVehicle)
{
	DeterminePoliceBehaviour(pVehicle);
	return FSM_Continue;
}


//////////////////////////////////////////////////////////////////////////////

void		CTaskVehiclePoliceBehaviour::Follow_OnEnter(CVehicle* UNUSED_PARAM(pVehicle))
{
	SetNewTask(rage_new CTaskVehicleFollow( m_Params ));
}


//////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehiclePoliceBehaviour::Follow_OnUpdate(CVehicle* pVehicle)
{
	DeterminePoliceBehaviour(pVehicle);
	return FSM_Continue;
}


//////////////////////////////////////////////////////////////////////////////

void		CTaskVehiclePoliceBehaviour::Block_OnEnter(CVehicle* UNUSED_PARAM(pVehicle))
{
	SetNewTask(rage_new CTaskVehicleBlock(m_Params));
}


//////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehiclePoliceBehaviour::Block_OnUpdate(CVehicle* pVehicle)
{
	DeterminePoliceBehaviour(pVehicle);
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////////

void CTaskVehiclePoliceBehaviour::Helicopter_OnEnter(CVehicle* UNUSED_PARAM(pVehicle))
{
	SetNewTask(rage_new CTaskVehiclePoliceBehaviourHelicopter(m_Params));
}

//////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehiclePoliceBehaviour::Helicopter_OnUpdate(CVehicle* UNUSED_PARAM(pVehicle))
{
	if (GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_POLICE_BEHAVIOUR_HELICOPTER))
	{
		return FSM_Quit;
	}
	return FSM_Continue;
}


//////////////////////////////////////////////////////////////////////////////

void CTaskVehiclePoliceBehaviour::Boat_OnEnter(CVehicle* UNUSED_PARAM(pVehicle))
{
	SetNewTask(rage_new CTaskVehiclePoliceBehaviourBoat(m_Params));
}

//////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehiclePoliceBehaviour::Boat_OnUpdate(CVehicle* UNUSED_PARAM(pVehicle))
{
	if (GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_POLICE_BEHAVIOUR_BOAT))
	{
		return FSM_Quit;
	}
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////////

void		CTaskVehiclePoliceBehaviour::Stop_OnEnter(CVehicle* /*pVehicle*/)
{
	SetNewTask(rage_new CTaskVehicleStop(0));
}

//////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehiclePoliceBehaviour::Stop_OnUpdate(CVehicle* pVehicle)
{
	DeterminePoliceBehaviour(pVehicle);
	return FSM_Continue;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : DeterminePoliceBehaviour
// PURPOSE : 
/////////////////////////////////////////////////////////////////////////////////

void CTaskVehiclePoliceBehaviour::DeterminePoliceBehaviour(CVehicle* pVeh)
{
	Assert(pVeh);

	CPed	*pTargetPed;
	CWanted *pWanted;

	if (pVeh->InheritsFromHeli())
	{
		SetState(State_Helicopter);
		return;
	}

	if (pVeh->GetVehicleType() == VEHICLE_TYPE_BOAT)
	{
		SetState(State_Boat);
		return;
	}

	if (GetTargetEntity() == NULL)
	{
		if(GetState() != State_Cruise)
		{
			// Set fall back mission
			SetCruiseSpeed( 10.0f );
			SetDrivingFlags(DMode_StopForCars);
			pVeh->TurnSirenOn(false);
			SetState(State_Cruise);
		}
		return;
	}

	if(GetTargetEntity()->GetIsTypePed() == false)		// Only peds can be target of police behaviour
		return;

	pTargetPed = (CPed*)GetTargetEntity();

	pWanted = pTargetPed->GetPlayerWanted();					// Will return null if not player

	if (!pWanted)
	{		
		if(GetState() != State_Follow)
		{
			// If we are chasing a non player we just ram into it. (Shouldn't really happen anyway)
			SetTargetEntity(pTargetPed);
			SetCruiseSpeed((14.0f + (pVeh->GetRandomSeed() & 7)));
			SetDrivingFlags(DMode_AvoidCars);
			pVeh->TurnSirenOn(false);
			SetState(State_Follow);
		}
		return;
	}


	if (pWanted->m_EverybodyBackOff || pWanted->PoliceBackOff() || CCullZones::NoPolice())
	{
		if(GetState() != State_Cruise)
		{
			// Set fall back mission
			SetCruiseSpeed(10.0f);
			SetDrivingFlags(DMode_StopForCars);
			pVeh->TurnSirenOn(false);
			SetState(State_Cruise);
			return;
		}
	}

	bool bSirenShouldBeOn = false;

	float	DistToPlayer = VEC3V_TO_VECTOR3(pTargetPed->GetTransform().GetPosition() - pVeh->GetTransform().GetPosition()).XYMag();

	s32 effectiveWanted = pWanted->GetWantedLevel();

	if (pTargetPed->GetIsArrested())		// If the player has been arrested we stop ramming.
	{
		effectiveWanted = WANTED_CLEAN;
	}

	switch (effectiveWanted)
	{
	case WANTED_CLEAN:
		pVeh->SetHandlingCheatValuesForPoliceCarInPursuit(false);
		if(GetState() != State_Cruise)
		{
			SetCruiseSpeed((10.0f + (pVeh->GetRandomSeed() & 3)));
			SetDrivingFlags(DMode_StopForCars);
			SetState(State_Cruise);
		}
		return;
	case WANTED_LEVEL1:
		bSirenShouldBeOn = true;
		pVeh->SetHandlingCheatValuesForPoliceCarInPursuit(false);
		if(GetState() != State_Follow)
		{
			SetTargetEntity(pTargetPed);
			SetCruiseSpeed((22.0f + (pVeh->GetRandomSeed() & 3)));
			SetDrivingFlags(DMode_AvoidCars);
			SetState(State_Follow);
		}
		break;
	case WANTED_LEVEL2:
		pVeh->EveryoneInsideWillFlyThroughWindscreen(false);
		pVeh->SetHandlingCheatValuesForPoliceCarInPursuit(true);
		bSirenShouldBeOn = true;
		if ((pVeh->GetRandomSeed() & 3) == 1)
		{
			if(GetState() != State_Ram)
				SetState(State_Ram);
		}
		else
		{
			if(GetState() != State_Block)
				SetState(State_Block);
		}
		if (pVeh->GetModelIndex() == MI_CAR_NOOSE)
		{
			if(GetState() != State_Follow)
				SetState(State_Follow);
		}
		SetTargetEntity(pTargetPed);
		//			fastSpeed = float(26 + (pVeh->RandomSeed & 3));
		//			slowSpeed = rage::Min(targetSpeed + 7, fastSpeed);
		SetCruiseSpeed((30.0f + (pVeh->GetRandomSeed() & 3))); //(u8)(fastSpeedPercentage * fastSpeed + (1.0f - fastSpeedPercentage) * slowSpeed);
		SetDrivingFlags(DMode_AvoidCars);
		//Mission.m_straightLineDist = 35;
		break;
	case WANTED_LEVEL3:
		pVeh->EveryoneInsideWillFlyThroughWindscreen(false);
		pVeh->SetHandlingCheatValuesForPoliceCarInPursuit(true);
		bSirenShouldBeOn = true;
		if ((pVeh->GetRandomSeed() & 3) <= 2)
		{
			if(GetState() != State_Block)
				SetState(State_Block);
		}
		else
		{
			if(GetState() != State_Ram)
				SetState(State_Ram);
		}
		if (pVeh->GetModelIndex() == MI_CAR_NOOSE)
		{
			if(GetState() != State_Follow)
				SetState(State_Follow);
		}

		//Mission.SetTargetEntity(pTargetPed);
		////			fastSpeed = float(30 + (pVeh->RandomSeed & 3));
		////			slowSpeed = rage::Min(targetSpeed + 10, fastSpeed);
		SetCruiseSpeed((36.0f + (pVeh->GetRandomSeed() & 3))); //(u8)(fastSpeedPercentage * fastSpeed + (1.0f - fastSpeedPercentage) * slowSpeed);
		SetDrivingFlags(DMode_AvoidCars);
		//Mission.m_straightLineDist = 40;
		break;
	case WANTED_LEVEL4:
		pVeh->EveryoneInsideWillFlyThroughWindscreen(false);
		pVeh->SetHandlingCheatValuesForPoliceCarInPursuit(true);
		bSirenShouldBeOn = true;
		if ((pVeh->GetRandomSeed() & 3) <= 1)
		{
			if(GetState() != State_Block)
				SetState(State_Block);
		}
		else
		{
			if(GetState() != State_Ram)
				SetState(State_Ram);
		}			
		if (pVeh->GetModelIndex() == MI_CAR_NOOSE)
		{
			if(GetState() != State_Follow)
				SetState(State_Follow);
		}
		SetTargetEntity(pTargetPed);
		////			fastSpeed = float(35 + (pVeh->RandomSeed & 3));
		////			slowSpeed = rage::Min(targetSpeed + 12, fastSpeed);
		SetCruiseSpeed((36.0f + (pVeh->GetRandomSeed() & 7))); //(u8)(fastSpeedPercentage * fastSpeed + (1.0f - fastSpeedPercentage) * slowSpeed);
		SetDrivingFlags(DMode_AvoidCars);
		//Mission.m_straightLineDist = 40;
		break;
	case WANTED_LEVEL5:
		pVeh->EveryoneInsideWillFlyThroughWindscreen(false);
		pVeh->SetHandlingCheatValuesForPoliceCarInPursuit(true);
		bSirenShouldBeOn = true;
		if ((pVeh->GetRandomSeed() & 3) <= 0)
		{
			if(GetState() != State_Block)
				SetState(State_Block);
		}
		else
		{
			if(GetState() != State_Ram)
				SetState(State_Ram);
		}
		if (pVeh->GetModelIndex() == MI_CAR_NOOSE)
		{
			if(GetState() != State_Follow)
				SetState(State_Follow);
		}
		SetTargetEntity(pTargetPed);
		////			fastSpeed = float(37 + (pVeh->RandomSeed & 3));
		////			slowSpeed = rage::Min(targetSpeed + 16, fastSpeed);
		
		SetCruiseSpeed((37.0f + (pVeh->GetRandomSeed() & 7))); //(u8)(fastSpeedPercentage * fastSpeed + (1.0f - fastSpeedPercentage) * slowSpeed);
		SetDrivingFlags(DMode_AvoidCars);
		m_fStraightLineDist = 40.0f;
		break;
	case WANTED_LEVEL_LAST:
		Assert(0);
		break;
	}


	if( bSirenShouldBeOn )
	{
		pVeh->TurnSirenOn(true);
		// Send message to the conductors
		ConductorMessageData messageData;
		messageData.conductorName = VehicleConductor;
		messageData.message = SirenTurnedOn;
		messageData.entity = pVeh;
		CONDUCTORS_ONLY(SUPERCONDUCTOR.SendConductorMessage(messageData));
	}
	else
	{
		pVeh->TurnSirenOn(false);
	}

	// If the player is on foot and we are nearby we limit the speed as getting rammed whilst on foot is very annoying
	if (!pTargetPed->GetMyVehicle())
	{
		if (DistToPlayer < 20)
		{
			SetCruiseSpeed(rage::Min(GetCruiseSpeed(), 10.0f));
		}
		else if (DistToPlayer < 40)
		{
			SetCruiseSpeed(rage::Min(GetCruiseSpeed(), 15.0f));
		}
	}
}


////////////////////////////////////////////////////////////////////////

#if !__FINAL
const char * CTaskVehiclePoliceBehaviour::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_Start&&iState<=State_Stop);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_Cruise",
		"State_Ram",
		"State_Follow",
		"State_Block",
		"State_Helicopter",
		"State_Boat",
		"State_Stop",
	};

	return aStateNames[iState];
}
#endif


//////////////////////////////////////////////////////////////////////////////

CTaskVehiclePoliceBehaviourHelicopter::CTaskVehiclePoliceBehaviourHelicopter( const sVehicleMissionParams& params ) :
CTaskVehicleMissionBase(params)
{
	m_fStraightLineDist = 40.0f;
	m_iFlightHeight = 40;
	m_iMinHeightAboveTerrain = 20;

	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_POLICE_BEHAVIOUR_HELICOPTER);
}

//////////////////////////////////////////////////////////////////////////////

CTaskVehiclePoliceBehaviourHelicopter::~CTaskVehiclePoliceBehaviourHelicopter()
{

}

//////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskVehiclePoliceBehaviourHelicopter::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.

	FSM_Begin
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate(pVehicle);

		FSM_State(State_MaintainAltitude)
			FSM_OnEnter
				MaintainAltitude_OnEnter(pVehicle);
			FSM_OnUpdate
				return MaintainAltitude_OnUpdate(pVehicle);

		FSM_State(State_Follow)
			FSM_OnEnter
				Follow_OnEnter(pVehicle);
			FSM_OnUpdate
				return Follow_OnUpdate(pVehicle);
	FSM_End
}

//////////////////////////////////////////////////////////////////////////////

void		CTaskVehiclePoliceBehaviourHelicopter::MaintainAltitude_OnEnter(CVehicle* UNUSED_PARAM(pVehicle))
{
	sVehicleMissionParams params = m_Params;
	params.ClearTargetPosition();
	params.m_fTargetArriveDist = 0.0f;

	SetNewTask(rage_new CTaskVehicleGoToHelicopter( params, 0, -1.0f, m_iMinHeightAboveTerrain));
}

//////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehiclePoliceBehaviourHelicopter::MaintainAltitude_OnUpdate(CVehicle* pVehicle)
{
	DeterminePoliceHeliBehaviour(pVehicle);
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////////

void		CTaskVehiclePoliceBehaviourHelicopter::Follow_OnEnter(CVehicle* UNUSED_PARAM(pVehicle))
{
	SetNewTask(rage_new CTaskVehicleFollow(m_Params));
}


//////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehiclePoliceBehaviourHelicopter::Follow_OnUpdate(CVehicle* UNUSED_PARAM(pVehicle))
{
	if (GetTargetEntity() == NULL)
	{
		// Set fall back mission
		SetCruiseSpeed(10.0f);
		SetState(State_MaintainAltitude);
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehiclePoliceBehaviourHelicopter::Start_OnUpdate(CVehicle* pVehicle)
{
	DeterminePoliceHeliBehaviour(pVehicle);

	return FSM_Continue;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : DeterminePoliceHeliBehaviour
// PURPOSE : 
/////////////////////////////////////////////////////////////////////////////////
void CTaskVehiclePoliceBehaviourHelicopter::DeterminePoliceHeliBehaviour(CVehicle* UNUSED_PARAM(pVeh))
{
	if (GetTargetEntity() == NULL)
	{
		// Set fall back mission
		SetCruiseSpeed(10.0f);
		SetState(State_MaintainAltitude);
		return;
	}

	SetCruiseSpeed(70.0f);
	m_iFlightHeight = 40;
	m_iMinHeightAboveTerrain = 20;


	// Second heli should fly a bit higher to avoid collisions.
	s16	heightShift = 0;

	m_iFlightHeight = m_iFlightHeight + heightShift;
	m_iMinHeightAboveTerrain = m_iMinHeightAboveTerrain + heightShift;

	SetState(State_Follow);
}

////////////////////////////////////////////////////////////////////////

#if !__FINAL
const char * CTaskVehiclePoliceBehaviourHelicopter::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_Start&&iState<=State_MaintainAltitude);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_Follow",
		"State_MaintainAltitude",
	};

	return aStateNames[iState];
}
#endif


/////////////////////////////////////////////////////////////////////////////////

CTaskVehiclePoliceBehaviourBoat::CTaskVehiclePoliceBehaviourBoat( const sVehicleMissionParams& params ) :
	CTaskVehicleMissionBase(params)
{
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_POLICE_BEHAVIOUR_BOAT);
}

//////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskVehiclePoliceBehaviourBoat::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.

	FSM_Begin
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate(pVehicle);

		FSM_State(State_Cruise)
			FSM_OnEnter
				Cruise_OnEnter(pVehicle);
			FSM_OnUpdate
				return Cruise_OnUpdate(pVehicle);

		FSM_State(State_Circle)
			FSM_OnEnter
				Circle_OnEnter(pVehicle);
			FSM_OnUpdate
				return Circle_OnUpdate(pVehicle);

		FSM_State(State_Ram)
			FSM_OnEnter
				Ram_OnEnter(pVehicle);
			FSM_OnUpdate
				return Ram_OnUpdate(pVehicle);

		FSM_State(State_Stop)
			FSM_OnEnter
				Stop_OnEnter(pVehicle);
			FSM_OnUpdate
				return Stop_OnUpdate(pVehicle);

	FSM_End
}

/////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehiclePoliceBehaviourBoat::Start_OnUpdate(CVehicle* pVehicle)
{
	DeterminePoliceBoatBehaviour(pVehicle);
	return FSM_Continue;
}

/////////////////////////////////////////////////////////////////////////////////

void		CTaskVehiclePoliceBehaviourBoat::Cruise_OnEnter(CVehicle* pVehicle)
{
	SetNewTask(CVehicleIntelligence::CreateCruiseTask(*pVehicle, m_Params));
}

/////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehiclePoliceBehaviourBoat::Cruise_OnUpdate(CVehicle* pVehicle)
{
	DeterminePoliceBoatBehaviour(pVehicle);
	return FSM_Continue;
}

/////////////////////////////////////////////////////////////////////////////////

void		CTaskVehiclePoliceBehaviourBoat::Circle_OnEnter(CVehicle* UNUSED_PARAM(pVehicle))
{
	SetNewTask(rage_new CTaskVehicleCircle(m_Params));
}


/////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehiclePoliceBehaviourBoat::Circle_OnUpdate(CVehicle* pVehicle)
{
	DeterminePoliceBoatBehaviour(pVehicle);
	return FSM_Continue;
}

/////////////////////////////////////////////////////////////////////////////////

void		CTaskVehiclePoliceBehaviourBoat::Ram_OnEnter(CVehicle* UNUSED_PARAM(pVehicle))
{
	SetNewTask( rage_new CTaskVehicleRam(m_Params));
}

/////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehiclePoliceBehaviourBoat::Ram_OnUpdate(CVehicle* pVehicle)
{
	DeterminePoliceBoatBehaviour(pVehicle);
	return FSM_Continue;
}

/////////////////////////////////////////////////////////////////////////////////

void		CTaskVehiclePoliceBehaviourBoat::Stop_OnEnter(CVehicle* UNUSED_PARAM(pVehicle))
{
	SetNewTask( rage_new CTaskVehicleStop());
}

/////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehiclePoliceBehaviourBoat::Stop_OnUpdate(CVehicle* pVehicle)
{
	DeterminePoliceBoatBehaviour(pVehicle);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////

void		CTaskVehiclePoliceBehaviourBoat::DeterminePoliceBoatBehaviour	(CVehicle* pVehicle)
{
	Assert(pVehicle);

	if (GetTargetEntity() == NULL )
	{
		if(GetState() != State_Cruise)
		{
			// Set fall back mission
			SetCruiseSpeed(10);
			SetDrivingFlags(DMode_StopForCars);
			SetState(State_Cruise);
		}

		return;
	}

	if(GetTargetEntity()->GetIsTypePed() == false)		// Only peds can be target of police behaviour
		return;

	CPed *pTargetPed = (CPed*)GetTargetEntity();
	CWanted *pWanted = pTargetPed->GetPlayerWanted();					// Will return null if not player
	eWantedLevel wantedLevel = WANTED_LEVEL1;
	if (pWanted)
	{
		pWanted->GetWantedLevel();
	}


	switch (wantedLevel)
	{
	case WANTED_CLEAN:
		if(GetState() != State_Cruise)
		{
			SetCruiseSpeed((10.0f + (pVehicle->GetRandomSeed() & 3)));
			SetDrivingFlags(DMode_StopForCars);
			SetState(State_Cruise);
		}
		break;
	default:
		if (pTargetPed->GetPedIntelligence()->IsPedSwimming())
		{
			if(GetState() != State_Circle)
			{
				SetTargetEntity(pTargetPed);
				SetCruiseSpeed((10.0f + (pVehicle->GetRandomSeed() % 5) + wantedLevel*2));
				SetDrivingFlags(DMode_PloughThrough);
				m_fStraightLineDist = 170.0f;		// Nodes are pretty sparse. Just go for player in straight line most of the time.
				SetState(State_Circle);
			}
		}
		else if (pTargetPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
		{
			if(GetState() != State_Ram)
			{
				SetTargetEntity(pTargetPed);
				SetCruiseSpeed((14.0f + (pVehicle->GetRandomSeed() % 5) + wantedLevel*5));
				SetDrivingFlags(DMode_PloughThrough);
				m_fStraightLineDist = 170.0f;		// Nodes are pretty sparse. Just go for player in straight line most of the time.
				SetState(State_Ram);
			}
		}
		else
		{
			if(GetState() != State_Stop)
			{
				SetCruiseSpeed(0.0f);
				SetDrivingFlags(DMode_PloughThrough);
				SetState(State_Stop);
			}
		}
		break;
	}

}

////////////////////////////////////////////////////////////////////////

#if !__FINAL
const char * CTaskVehiclePoliceBehaviourBoat::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_Start&&iState<=State_Stop);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_Cruise",
		"State_Circle",
		"State_Ram",
		"State_Stop",
	};

	return aStateNames[iState];
}
#endif