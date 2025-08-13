// FILE :    TaskShootOutTire.cpp
// PURPOSE : Combat subtask in control of shooting out a tire
// CREATED : 3/1/2012

// File header
#include "Task/Combat/Subtasks/TaskShootOutTire.h"

// Game headers
#include "audio/scannermanager.h"
#include "Peds/Ped.h"
#include "Peds/PedGeometryAnalyser.h"
#include "Task/Weapons/Gun/TaskGun.h"
#include "vehicleAi/task/TaskVehicleShotTire.h"
#include "vehicleAi/VehicleIntelligence.h"
#include "Vehicles/vehicle.h"
#include "Vehicles/wheel.h"

AI_OPTIMISATIONS()

//=========================================================================
// CTaskShootOutTire
//=========================================================================

#if NA_POLICESCANNER_ENABLED
extern audScannerManager g_AudioScannerManager;
#endif

CTaskShootOutTire::Tunables CTaskShootOutTire::ms_Tunables;

IMPLEMENT_COMBAT_TASK_TUNABLES(CTaskShootOutTire, 0x405f2266);

CTaskShootOutTire::CTaskShootOutTire(const CAITarget& target, const fwFlags8 uConfigFlags)
: m_Target(target)
, m_nTire(VEH_INVALID_ID)
, m_fTimeoutToAcquireLineOfSight(0.0f)
, m_fTimeSinceLastLineOfSightCheck(0.0f)
, m_fTimeToWaitForShot(0.0f)
, m_uWaitForShotFailures(0)
, m_uConfigFlags(uConfigFlags)
, m_uRunningFlags(0)
{
	SetInternalTaskType(CTaskTypes::TASK_SHOOT_OUT_TIRE);
}

CTaskShootOutTire::~CTaskShootOutTire()
{
}

aiTask* CTaskShootOutTire::Copy() const
{
	return rage_new CTaskShootOutTire(m_Target, m_uConfigFlags);
}

#if !__FINAL
const char* CTaskShootOutTire::GetStaticStateName(s32 iState)
{
	Assert(iState >= State_Start && iState <= State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_DecideWhichTireToShoot",
		"State_AcquireLineOfSight",
		"State_WaitForShot",
		"State_Shoot",
		"State_GiveUp",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif

CTask::FSM_Return CTaskShootOutTire::ProcessPreFSM()
{
	//Ensure the target vehicle is valid.
	if(!GetTargetVehicle())
	{
		return FSM_Quit;
	}
	
	//Check if the wheel is valid.
	const CWheel* pWheel = GetWheel();
	if(pWheel)
	{
		//If the wheel is already flat, no need to continue.
		if(pWheel->GetIsFlat())
		{
			return FSM_Quit;
		}
	}
	
	//Aim at the wheel.
	ProcessAimAtWheel();
	
	//Process the line of sight.
	ProcessLineOfSight();
	
	return FSM_Continue;
}

CTask::FSM_Return CTaskShootOutTire::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		FSM_State(State_Start)
			FSM_OnEnter
				Start_OnEnter();
			FSM_OnUpdate
				return Start_OnUpdate();
		
		FSM_State(State_DecideWhichTireToShoot)
			FSM_OnEnter
				DecideWhichTireToShoot_OnEnter();
			FSM_OnUpdate
				return DecideWhichTireToShoot_OnUpdate();
				
		FSM_State(State_AcquireLineOfSight)
			FSM_OnEnter
				AcquireLineOfSight_OnEnter();
			FSM_OnUpdate
				return AcquireLineOfSight_OnUpdate();
				
		FSM_State(State_WaitForShot)
			FSM_OnEnter
				WaitForShot_OnEnter();
			FSM_OnUpdate
				return WaitForShot_OnUpdate();
				
		FSM_State(State_Shoot)
			FSM_OnEnter
				Shoot_OnEnter();
			FSM_OnUpdate
				return Shoot_OnUpdate();
				
		FSM_State(State_GiveUp)
			FSM_OnEnter
				GiveUp_OnEnter();
			FSM_OnUpdate
				return GiveUp_OnUpdate();
		
		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;
		
	FSM_End
}

void CTaskShootOutTire::Start_OnEnter()
{
	//Create an aim task.
	CTask* pTask = rage_new CTaskGun(CWeaponController::WCT_Aim, CTaskTypes::TASK_AIM_GUN_VEHICLE_DRIVE_BY, CWeaponTarget(VEC3V_TO_VECTOR3(GetAimPosition())));

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskShootOutTire::Start_OnUpdate()
{
	//Decide which tire to shoot.
	SetStateAndKeepSubTask(State_DecideWhichTireToShoot);
	
	return FSM_Continue;
}

void CTaskShootOutTire::DecideWhichTireToShoot_OnEnter()
{
	//Grab the ped.
	const CPed* pPed = GetPed();
	taskAssertf(pPed, "Ped is invalid.");

	//Grab the ped position.
	Vec3V vPedPosition = pPed->GetTransform().GetPosition();

	//Grab the target vehicle.
	const CVehicle* pTargetVehicle = GetTargetVehicle();
	taskAssertf(pTargetVehicle, "Target vehicle is invalid.");

	//Grab the vehicle position.
	Vec3V vVehiclePosition = pTargetVehicle->GetTransform().GetPosition();

	//Generate a vector from the vehicle to the ped.
	Vec3V vVehicleToPed = Subtract(vPedPosition, vVehiclePosition);

	//Grab the vehicle right vector.
	Vec3V vVehicleRight = pTargetVehicle->GetTransform().GetRight();

	//Decide which tire to shoot at based on the side of the vehicle that the ped is on.
	ScalarV scDot = Dot(vVehicleRight, vVehicleToPed);
	if(IsGreaterThanAll(scDot, ScalarV(V_ZERO)))
	{
		m_nTire = VEH_WHEEL_RF;
	}
	else
	{
		m_nTire = VEH_WHEEL_LF;
	}
}

CTask::FSM_Return CTaskShootOutTire::DecideWhichTireToShoot_OnUpdate()
{	
	//Move to the acquire line of sight state.
	SetStateAndKeepSubTask(State_AcquireLineOfSight);
	
	return FSM_Continue;
}

void CTaskShootOutTire::AcquireLineOfSight_OnEnter()
{
	//Update the line of sight.
	UpdateLineOfSight();
	
	//Choose a timeout for acquiring a line of sight.
	m_fTimeoutToAcquireLineOfSight = fwRandom::GetRandomNumberInRange(ms_Tunables.m_MinTimeoutToAcquireLineOfSight, ms_Tunables.m_MaxTimeoutToAcquireLineOfSight);

#if NA_POLICESCANNER_ENABLED
	//Send a request to the police scanner to play audio
	if(Verifyf(GetPed() && GetPed()->GetVehiclePedInside() && GetPed()->GetVehiclePedInside()->GetIsRotaryAircraft(), 
		"TaskShootOutTire being called from ped not in helicopter.  Audio isn't expecting this."))
		g_AudioScannerManager.TriggerScriptedReport("PS_AIR_SUPPORT_SHOOT_TIRES_START");
#endif
}

CTask::FSM_Return CTaskShootOutTire::AcquireLineOfSight_OnUpdate()
{
	//Check if a timeout has occurred.
	bool bTimeout = (GetTimeInState() >= m_fTimeoutToAcquireLineOfSight);
	
	//If we are going to time out, update the line of sight one last time.
	//This gives one last chance to acquire a line of sight.
	if(bTimeout)
	{
		UpdateLineOfSight();
	}
	
	//Check if we have acquired a line of sight.
	if(m_uRunningFlags.IsFlagSet(SOTRF_HasLineOfSight))
	{
		//Move to the wait for shot state.
		SetStateAndKeepSubTask(State_WaitForShot);
	}
	//Check if a timeout has occurred.
	else if(bTimeout)
	{
		//Move to the give up state.
		SetStateAndKeepSubTask(State_GiveUp);
	}
	
	return FSM_Continue;
}

void CTaskShootOutTire::WaitForShot_OnEnter()
{
	//Choose a time to wait for the shot.
	m_fTimeToWaitForShot = fwRandom::GetRandomNumberInRange(ms_Tunables.m_MinTimeToWaitForShot, ms_Tunables.m_MaxTimeToWaitForShot);
}

CTask::FSM_Return CTaskShootOutTire::WaitForShot_OnUpdate()
{
	//Check if the wait is over.
	bool bWaitIsOver = (GetTimeInState() >= m_fTimeToWaitForShot);

	//If the wait is over, update the line of sight one last time.
	//This gives one last chance for us to not have a line of sight.
	if(bWaitIsOver)
	{
		UpdateLineOfSight();
	}

	//Check if we no longer have a line of sight.
	if(!m_uRunningFlags.IsFlagSet(SOTRF_HasLineOfSight))
	{
		//Increase the wait for shot failures.
		++m_uWaitForShotFailures;
		if(m_uWaitForShotFailures >= ms_Tunables.m_MaxWaitForShotFailures)
		{
			//Move to the give up state.
			SetStateAndKeepSubTask(State_GiveUp);
		}
		else
		{
			//Move to the acquire line of sight state.
			SetStateAndKeepSubTask(State_AcquireLineOfSight);
		}
	}
	//Check if the wait is over.
	else if(bWaitIsOver)
	{
		//Move to the shoot state.
		SetStateAndKeepSubTask(State_Shoot);
	}

	return FSM_Continue;
}

void CTaskShootOutTire::Shoot_OnEnter()
{	
	//Shoot the weapon.
	ShootWeapon();
	
	//Apply the damage to the tire.
	ApplyDamageToTire();
	
	//Apply a reaction to the vehicle.
	ApplyReactionToVehicle();
}

CTask::FSM_Return CTaskShootOutTire::Shoot_OnUpdate()
{
	//Move to the finish state.
	SetStateAndKeepSubTask(State_Finish);
	
	return FSM_Continue;
}

void CTaskShootOutTire::GiveUp_OnEnter()
{
#if NA_POLICESCANNER_ENABLED
	//Send a request to the police scanner to play audio
	if(Verifyf(GetPed() && GetPed()->GetVehiclePedInside() && GetPed()->GetVehiclePedInside()->GetIsRotaryAircraft(), 
		"TaskShootOutTire being called from ped not in helicopter.  Audio isn't expecting this."))
		g_AudioScannerManager.TriggerScriptedReport("PS_AIR_SUPPORT_SHOOT_TIRES_STOP");
#endif
}

CTask::FSM_Return CTaskShootOutTire::GiveUp_OnUpdate()
{
	//Move to the finish state.
	SetStateAndKeepSubTask(State_Finish);
	
	return FSM_Continue;
}

void CTaskShootOutTire::ApplyDamageToTire()
{
	//Grab the target vehicle.
	taskAssertf(GetTargetVehicle(), "Target vehicle is invalid.");
	
    //Ensure the wheel is valid.
	CWheel* pWheel = GetWheel();
	if(!pWheel)
	{
		return;
	}

	//Calculate the damage needed to make the tire flat.
	float fDamage = pWheel->GetTyreHealth() - TYRE_HEALTH_FLAT;
	if(fDamage <= 0.0f)
	{
		return;
	}

	//Pop the tire.
	pWheel->ApplyTyreDamage(GetPed(), fDamage, VEC3_ZERO, VEC3_ZERO, DAMAGE_TYPE_BULLET, 0);

	//Burst the wheel to the rim.
	pWheel->DoBurstWheelToRim();
}

void CTaskShootOutTire::ApplyReactionToVehicle()
{
	//Ensure the reaction should be applied to the vehicle.
	if(!ShouldApplyReactionToVehicle())
	{
		return;
	}
	
	//Grab the vehicle.
	CVehicle* pVehicle = GetTargetVehicle();
	taskAssertf(pVehicle, "Vehicle is invalid.");
	
	//Create the task.
	CTask* pTask = rage_new CTaskVehicleShotTire(m_nTire);
	
	//Start the task.
	pVehicle->GetIntelligence()->AddTask(VEHICLE_TASK_TREE_PRIMARY, pTask, VEHICLE_TASK_PRIORITY_PRIMARY);
}

Vec3V_Out CTaskShootOutTire::GetAimPosition() const
{
	//Grab the target vehicle.
	const CVehicle* pVehicle = GetTargetVehicle();
	taskAssertf(pVehicle, "Target vehicle is invalid.");
	
	//Check if the wheel is valid.
	const CWheel* pWheel = GetWheel();
	if(pWheel)
	{
		//Grab the wheel position.
		Vec3V vPosition;
		pWheel->GetWheelPosAndRadius(RC_VECTOR3(vPosition));

		return vPosition;
	}
	else
	{
		return pVehicle->GetTransform().GetPosition();
	}
}

const CPed* CTaskShootOutTire::GetTargetPed() const
{
	//Ensure the target entity is valid.
	const CEntity* pTargetEntity = m_Target.GetEntity();
	if(!pTargetEntity)
	{
		return NULL;
	}
	
	//Ensure the target entity is a ped.
	if(!pTargetEntity->GetIsTypePed())
	{
		return NULL;
	}
	
	return static_cast<const CPed *>(pTargetEntity);
}

CVehicle* CTaskShootOutTire::GetTargetVehicle() const
{
	//Ensure the target ped is valid.
	const CPed* pPed = GetTargetPed();
	if(!pPed)
	{
		return NULL;
	}
	
	return pPed->GetVehiclePedInside();
}

CWheel* CTaskShootOutTire::GetWheel() const
{
	//Ensure the target vehicle is valid.
	CVehicle* pVehicle = GetTargetVehicle();
	if(!pVehicle)
	{
		return NULL;
	}
	
	return pVehicle->GetWheelFromId(m_nTire);
}

void CTaskShootOutTire::ProcessAimAtWheel()
{
	//Ensure the sub-task is valid.
	CTask* pTask = GetSubTask();
	if(!pTask)
	{
		return;
	}
	
	//Ensure the sub-task is the correct type.
	if(pTask->GetTaskType() != CTaskTypes::TASK_GUN)
	{
		return;
	}
	
	//Grab the gun task.
	CTaskGun* pGunTask = static_cast<CTaskGun *>(pTask);
	
	//Set the target.
	pGunTask->SetTarget(CWeaponTarget(VEC3V_TO_VECTOR3(GetAimPosition())));
}

void CTaskShootOutTire::ProcessLineOfSight()
{
	//Update the timer.
	m_fTimeSinceLastLineOfSightCheck += fwTimer::GetTimeStep();
	
	//Ensure we should process the line of sight.
	if(!ShouldProcessLineOfSight())
	{
		return;
	}
	
	//Ensure the timer has exceeded the threshold.
	if(m_fTimeSinceLastLineOfSightCheck < ms_Tunables.m_TimeBetweenLineOfSightChecks)
	{
		return;
	}
	
	//Update the line of sight.
	UpdateLineOfSight();
}

void CTaskShootOutTire::SetStateAndKeepSubTask(s32 iState)
{
	//Keep the sub-task across the transition.
	SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);

	//Set the state.
	SetState(iState);
}

void CTaskShootOutTire::ShootWeapon()
{
	//Grab the ped.
	CPed* pPed = GetPed();
	
	//Ensure the weapon manager is valid.
	CPedWeaponManager* pWeaponManager = pPed->GetWeaponManager();
	if(!pWeaponManager)
	{
		return;
	}
	
	//Ensure the weapon is valid.
	CWeapon* pWeapon = pWeaponManager->GetEquippedWeapon();
	if(!pWeapon)
	{
		return;
	}
	
	//Ensure the object is valid.
	CObject* pObject = pWeaponManager->GetEquippedWeaponObject();
	if(!pObject)
	{
		return;
	}
	
	//Generate the weapon matrix.
	Matrix34 mWeapon;
	if(pWeapon->GetWeaponModelInfo()->GetBoneIndex(WEAPON_MUZZLE) != -1)
	{
		pObject->GetGlobalMtx(pWeapon->GetWeaponModelInfo()->GetBoneIndex(WEAPON_MUZZLE), mWeapon);
	}
	else
	{
		pObject->GetMatrixCopy(mWeapon);
	}
	
	//Generate the aim position.
	Vec3V vAimPosition = GetAimPosition();
	
	//Generate the fire vector.
	Vector3 vStart;
	Vector3 vEnd;
	if(!pWeapon->CalcFireVecFromPos(pPed, mWeapon, vStart, vEnd, VEC3V_TO_VECTOR3(vAimPosition)))
	{
		return;
	}
	
	//Fire the weapon.
	CWeapon::sFireParams params(pPed, mWeapon, &vStart, &vEnd);
	params.iFireFlags.SetFlag(CWeapon::FF_ForceBulletTrace);
	pWeapon->Fire(params);
}

bool CTaskShootOutTire::ShouldApplyReactionToVehicle() const
{
	//Ensure the flag is set.
	if(!m_uConfigFlags.IsFlagSet(SOTCF_ApplyReactionToVehicle))
	{
		return false;
	}
	
	//Ensure the vehicle is valid.
	const CVehicle* pVehicle = GetTargetVehicle();
	if(!pVehicle)
	{
		return false;
	}
	
	//Ensure the vehicle is a car.
	if(pVehicle->GetVehicleType() != VEHICLE_TYPE_CAR)
	{
		return false;
	}

	//Ensure the vehicle is not big.
	if(pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_BIG))
	{
		return false;
	}
	
	//Ensure all of the wheels are touching.
	if(pVehicle->GetNumWheels() != pVehicle->GetNumContactWheels())
	{
		return false;
	}

	//Ensure the vehicle's velocity exceeds the threshold.
	float fSpeedSq = pVehicle->GetVelocity().XYMag2();
	float fMinSpeedSq = square(ms_Tunables.m_MinSpeedToApplyReaction);
	if(fSpeedSq < fMinSpeedSq)
	{
		return false;
	}
	
	return true;
}

bool CTaskShootOutTire::ShouldProcessLineOfSight() const
{
	switch(GetState())
	{
		case State_AcquireLineOfSight:
		case State_WaitForShot:
		{
			return true;
		}
		default:
		{
			return false;
		}
	}
}

void CTaskShootOutTire::UpdateLineOfSight()
{
	//Clear the timer.
	m_fTimeSinceLastLineOfSightCheck = 0.0f;
	
	//Set the line of sight flag based on whether the ped can target the point.
	s32 iTargetOptions = TargetOption_IgnoreVehicles|TargetOption_TargetInVehicle;
	if(CPedGeometryAnalyser::CanPedTargetPoint(*GetPed(), VEC3V_TO_VECTOR3(GetAimPosition()), iTargetOptions))
	{
		m_uRunningFlags.SetFlag(SOTRF_HasLineOfSight);
	}
	else
	{
		m_uRunningFlags.ClearFlag(SOTRF_HasLineOfSight);
	}
}
