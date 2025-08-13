// FILE :    TaskHeliCombat.h
// PURPOSE : Subtask of combat used for heli combat

// File header
#include "Task/Combat/Subtasks/TaskHeliCombat.h"

// Game headers
#include "Peds/Ped.h"
#include "task/Combat/TaskCombat.h"
#include "task/Combat/Subtasks/TaskHeliChase.h"
#include "task/Vehicle/TaskCar.h"
#include "vehicleAi/task/TaskVehicleGoToHelicopter.h"
#include "Vehicles/heli.h"
#include "Vehicles/vehicle.h"

AI_OPTIMISATIONS()

//=========================================================================
// CTaskHeliCombat
//=========================================================================

CTaskHeliCombat::Tunables CTaskHeliCombat::sm_Tunables;
IMPLEMENT_COMBAT_TASK_TUNABLES(CTaskHeliCombat, 0x3a91f9dc);

CTaskHeliCombat::CTaskHeliCombat(const CAITarget& rTarget)
: m_Target(rTarget)
, m_ChangeStateTimer(1.0f)
{
	SetInternalTaskType(CTaskTypes::TASK_HELI_COMBAT);
}

CTaskHeliCombat::~CTaskHeliCombat()
{
}

#if !__FINAL
void CTaskHeliCombat::Debug() const
{
	CTask::Debug();
}

const char* CTaskHeliCombat::GetStaticStateName(s32 iState)
{
	Assert(iState >= State_Start && iState <= State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_Chase",
		"State_Strafe",
		"State_Refuel",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif // !__FINAL

Vec3V_Out CTaskHeliCombat::CalculateTargetOffsetForChase() const
{
	Vector3 vTargetPosition;
	m_Target.GetPosition(vTargetPosition);

	//Grab the heli.
	const CHeli* pHeli = GetHeli();

	// if we're flying lets compute a new chase target
	const CEntity* pTargetEntity = GetTargetEntity();

	if (pTargetEntity && pHeli->GetHeliIntelligence()->bHasCombatOffset)
	{
		return VECTOR3_TO_VEC3V(pHeli->GetHeliIntelligence()->vCombatOffset);
	}

	Vec3V vTargetToHeli = pHeli->GetTransform().GetPosition() - VECTOR3_TO_VEC3V(vTargetPosition);
		
	//Calculate the X value.
	float fMinTargetOffsetX = sm_Tunables.m_Chase.m_MinTargetOffsetX;
	float fMaxTargetOffsetX = sm_Tunables.m_Chase.m_MaxTargetOffsetX;
	
	//Calculate the Y value.
	float fMinTargetOffsetY = sm_Tunables.m_Chase.m_MinTargetOffsetY;
	float fMaxTargetOffsetY = sm_Tunables.m_Chase.m_MaxTargetOffsetY;
	
	//Calculate the Z value.
	float fMinTargetOffsetZ = sm_Tunables.m_Chase.m_MinTargetOffsetZ;
	float fMaxTargetOffsetZ = sm_Tunables.m_Chase.m_MaxTargetOffsetZ;

	bool inAir = false;

	Vec3V vTargetForward = pHeli->GetTransform().GetForward();

	if ( pTargetEntity )
	{
		vTargetForward = pTargetEntity->GetTransform().GetForward();
		if ( pTargetEntity->GetIsTypePed() )
		{
			const CPed* pPed = static_cast<const CPed*>(pTargetEntity);
			const CVehicle* pVehicle = pPed->GetVehiclePedInside();
			if ( pVehicle )
			{
				if ( pVehicle->InheritsFromHeli() || pVehicle->InheritsFromPlane() 
					|| pVehicle->InheritsFromBlimp() || pVehicle->InheritsFromAutogyro() )
				{
					if ( const_cast<CVehicle*>(pVehicle)->IsInAir() )
					{
						fMinTargetOffsetZ = sm_Tunables.m_Chase.m_MinTargetOffsetZ_TargetInAir;
						fMaxTargetOffsetZ = sm_Tunables.m_Chase.m_MaxTargetOffsetZ_TargetInAir;

						inAir = true;
					}
				}
			}
		}
	}

	Vec3V vTargetVelocity = CalculateTargetVelocity();
	vTargetVelocity.SetZ(ScalarV(V_ZERO));

	Vec3V vOffsetForward = NormalizeFastSafe(vTargetVelocity, vTargetForward);
	Vec3V vOffsetRight = Vec3V(vOffsetForward.GetYf(), -vOffsetForward.GetXf(), 0.0f);

	float fForwardY = Dot( vTargetToHeli, vOffsetForward ).Getf();
	float fSidewaysX = Dot( vTargetToHeli, vOffsetRight ).Getf();
	float fUpZ =  vTargetToHeli.GetZf();
	
	float fSignX = Sign(fSidewaysX);
	float fSignZ = (inAir) ? Sign(fUpZ) : 1.0f;

	fSidewaysX = Clamp(fSidewaysX, fMinTargetOffsetX, fMaxTargetOffsetX) * fSignX;
	fForwardY = Clamp(fForwardY, fMinTargetOffsetY, fMaxTargetOffsetY);
	fUpZ = Clamp(fUpZ, fMinTargetOffsetZ, fMaxTargetOffsetZ) * fSignZ;
	
	return Vec3V(fSidewaysX, fForwardY, fUpZ);
}

CHeli* CTaskHeliCombat::GetHeli() const
{
	//Grab the ped.
	const CPed* pPed = GetPed();
	
	//Ensure the ped is in a vehicle.
	CVehicle* pVehicle = pPed->GetVehiclePedInside();
	if(!pVehicle)
	{
		return NULL;
	}
	
	//Ensure the vehicle is a heli.
	if(!pVehicle->InheritsFromHeli())
	{
		return NULL;
	}
	
	return static_cast<CHeli *>(pVehicle);
}

const CEntity* CTaskHeliCombat::GetTargetEntity() const
{
	//Ensure the target is valid.
	if(!m_Target.GetIsValid())
	{
		return NULL;
	}

	return m_Target.GetEntity();
}

const CPed* CTaskHeliCombat::GetTargetPed() const
{
	//Grab the target entity.
	const CEntity* pEntity = GetTargetEntity();
	
	//Ensure the entity is a ped.
	if(!pEntity->GetIsTypePed())
	{
		return NULL;
	}
	
	return static_cast<const CPed *>(pEntity);
}

CVehicle* CTaskHeliCombat::GetTargetVehicle() const
{
	//Ensure the target ped is valid.
	const CPed* pPed = GetTargetPed();
	if(!pPed)
	{
		return NULL;
	}
	
	return pPed->GetVehiclePedInside();
}

Vec3V_Out CTaskHeliCombat::CalculateTargetVelocity() const
{
	Vec3V vVelocity = Vec3V(0.0f, 0.0f, 0.0f);
	const CPed* pPed = GetTargetPed();
	if ( pPed )
	{
		if ( pPed->GetVehiclePedInside() )
		{
			vVelocity = VECTOR3_TO_VEC3V(pPed->GetVehiclePedInside()->GetVelocity());
		}
		else
		{
			if ( MagSquared(pPed->GetGroundVelocityIntegrated()).Getf() > pPed->GetVelocity().Mag2() )
			{
				vVelocity = pPed->GetGroundVelocityIntegrated();
			}
			else
			{
				vVelocity = VECTOR3_TO_VEC3V(pPed->GetVelocity());
			}
		}
	}
	return vVelocity;
}

bool CTaskHeliCombat::ShouldChase() const
{
	//Ensure the vehicle speed exceeds the threshold.
	float fSpeedSq = MagSquared(CalculateTargetVelocity()).Getf();
		
	float fMinSpeedSq = square(sm_Tunables.m_Chase.m_MinSpeed);
	if(fSpeedSq < fMinSpeedSq)
	{
		return false;
	}

	return true;
}

bool CTaskHeliCombat::ShouldStrafe() const
{
	return !ShouldChase();
}

void CTaskHeliCombat::UpdateTargetOffsetForChase()
{
	//Ensure the sub-task is valid.
	CTask* pSubTask = GetSubTask();
	if(!pSubTask)
	{
		return;
	}
	
	//Ensure the task type is right.
	if(!taskVerifyf(pSubTask->GetTaskType() == CTaskTypes::TASK_HELI_CHASE, "The sub-task type is invalid: %d.", pSubTask->GetTaskType()))
	{
		return;
	}
	
	//Grab the task.
	CTaskHeliChase* pTask = static_cast<CTaskHeliChase *>(pSubTask);
	
	// just keep computing this
	pTask->SetTargetOffset(CalculateTargetOffsetForChase());
}

CTask::FSM_Return CTaskHeliCombat::ProcessPreFSM()
{
	m_ChangeStateTimer.Tick(GetTimeStep());

	//Ensure the heli is valid.
	if(!GetHeli())
	{
		return FSM_Quit;
	}
	
	//Ensure the target entity is valid.
	if(!GetTargetEntity())
	{
		return FSM_Quit;
	}
	
	return FSM_Continue;
}

CTask::FSM_Return CTaskHeliCombat::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
	
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();
				
		FSM_State(State_Chase)
			FSM_OnEnter
				Chase_OnEnter();
			FSM_OnUpdate
				return Chase_OnUpdate();
				
		FSM_State(State_Strafe)
			FSM_OnEnter
				Strafe_OnEnter();
			FSM_OnUpdate
				return Strafe_OnUpdate();

		FSM_State(State_Refuel)
			FSM_OnEnter
				Refuel_OnEnter();
			FSM_OnUpdate
				return Refuel_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskHeliCombat::Start_OnUpdate()
{
	//Check if we should chase.
	if(ShouldChase())
	{
		//Move to the chase state.
		SetState(State_Chase);
	}
	//Check if we should strafe.
	else if(ShouldStrafe())
	{
		//Move to the strafe state.
		SetState(State_Strafe);
	}
	else
	{
		taskAssertf(false, "The heli does not want to chase or strafe.");
	}
	
	return FSM_Continue;
}

void CTaskHeliCombat::Chase_OnEnter()
{
	//Calculate the target offset.
	Vec3V vTargetOffset = CalculateTargetOffsetForChase();
	
	//Create the task.
	CTaskHeliChase* pTask = rage_new CTaskHeliChase(m_Target, vTargetOffset);
	
	// This sets the helicopter to point in the targets velocity direction as
	// the helicopter approaches the target offset position
	pTask->SetOrientationMode(CTaskHeliChase::OrientationMode_OrientNearArrival);
	pTask->SetOrientationRelative(CTaskHeliChase::OrientationRelative_TargetVelocity);
	pTask->SetOrientationOffset(0);

	m_ChangeStateTimer.SetTime(1.0f);
	m_ChangeStateTimer.Reset();

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskHeliCombat::Chase_OnUpdate()
{
	//Update the target offset.
	UpdateTargetOffsetForChase();

	//Check if we should strafe.
	if(ShouldStrafe())
	{
		if ( m_ChangeStateTimer.IsFinished() )
		{
			//Move to the strafe state.
			SetState(State_Strafe);
		}
	}
	else
	{
		m_ChangeStateTimer.Reset();
	}

	//Check if the heli should refuel.
	const CPed* pTargetPed = GetTargetPed();
	if(pTargetPed && CHeliRefuelHelper::Process(*GetPed(), *pTargetPed))
	{
		SetState(State_Refuel);
	}

	//Check if the sub-task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Finish the task.
		SetState(State_Finish);
	}
	
	return FSM_Continue;
}

void CTaskHeliCombat::Strafe_OnEnter()
{
	//Create the task.
	CTask* pTask = rage_new CTaskHelicopterStrafe(m_Target.GetEntity());

	m_ChangeStateTimer.SetTime(1.0f);
	m_ChangeStateTimer.Reset();

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskHeliCombat::Strafe_OnUpdate()
{
	//Check if we should chase.
	if(ShouldChase())
	{
		if ( m_ChangeStateTimer.IsFinished() )
		{
			//Move to the chase state.
			SetState(State_Chase);
		}
	}
	else
	{
		m_ChangeStateTimer.Reset();
	}

	//Check if the heli should refuel.
	const CPed* pTargetPed = GetTargetPed();
	if(pTargetPed && CHeliRefuelHelper::Process(*GetPed(), *pTargetPed))
	{
		SetState(State_Refuel);
	}
	
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Finish the task.
		SetState(State_Finish);
	}
	
	return FSM_Continue;
}

void CTaskHeliCombat::Refuel_OnEnter()
{
	//Let the heli be deleted with alive peds in it.
	GetHeli()->m_nVehicleFlags.bCanBeDeletedWithAlivePedsInIt = true;

	//Create the task.
	CTask* pTask = CHeliFleeHelper::CreateTaskForPed(*GetPed(), m_Target);
	taskAssert(pTask);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskHeliCombat::Refuel_OnUpdate()
{
	//Check if the task has finished.
	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Finish);
	}

	return FSM_Continue;
}

void CTaskHeliCombat::Refuel_OnExit()
{
	CHeli* pHeli = GetHeli();
	if(pHeli)
	{
		pHeli->m_nVehicleFlags.bCanBeDeletedWithAlivePedsInIt = false;
	}
}
