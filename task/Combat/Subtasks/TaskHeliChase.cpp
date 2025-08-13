// FILE :    TaskHeliChase.h
// PURPOSE : Subtask of heli combat used to chase a target

// File header
#include "Task/Combat/Subtasks/TaskHeliChase.h"

// Game headers
#include "scene/world/GameWorld.h"
#include "script/script.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "task/Vehicle/TaskCar.h"
#include "vehicleAi/VehicleIntelligence.h"
#include "vehicleAi/task/TaskVehicleGoToHelicopter.h"
#include "Vehicles/heli.h"
#include "Vehicles/vehicle.h"

AI_VEHICLE_OPTIMISATIONS()
AI_OPTIMISATIONS()

//=========================================================================
// CTaskHeliChase
//=========================================================================

CTaskHeliChase::Tunables CTaskHeliChase::sm_Tunables;
IMPLEMENT_COMBAT_TASK_TUNABLES(CTaskHeliChase, 0x1c918d50);

CTaskHeliChase::CTaskHeliChase(const CAITarget& rTarget, Vec3V_In vTargetOffset)
: m_vTargetOffset(vTargetOffset)
, m_DriftHelperX()
, m_DriftHelperY()
, m_DriftHelperZ()
, m_Target(rTarget)
, m_OffsetRelative(OffsetRelative_Local)
, m_fOrientationOffset(0)
, m_OrientationRelative(OrientationRelative_TargetForward)
, m_OrientationMode(OrientationMode_OrientOnArrival)
, m_DriftMode(DriftMode_Disabled)
, m_CloneQuit(false)
{
	SetInternalTaskType(CTaskTypes::TASK_HELI_CHASE);
}

CTaskHeliChase::~CTaskHeliChase()
{
}

#if !__FINAL
void CTaskHeliChase::Debug() const
{
	CTask::Debug();
}

const char* CTaskHeliChase::GetStaticStateName(s32 iState)
{
	Assert(iState >= State_Start && iState <= State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_Pursue",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif // !__FINAL


void CTaskHeliChase::OnCloneTaskNoLongerRunningOnOwner()
{
	m_CloneQuit = true;
}

CTaskInfo* CTaskHeliChase::CreateQueriableState() const
{
	return rage_new CClonedHeliChaseInfo(m_Target, m_vTargetOffset);
}

CTask::FSM_Return CTaskHeliChase::UpdateClonedFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

	FSM_State(State_Start)
		FSM_OnUpdate
		return Start_OnUpdate();

	FSM_State(State_Pursue)
		FSM_OnEnter
			Pursue_OnEnterClone();
		FSM_OnUpdate
			return Pursue_OnUpdateClone();
	FSM_State(State_Finish)
		FSM_OnUpdate
		return FSM_Quit;

	FSM_End
}

CTaskFSMClone* CTaskHeliChase::CreateTaskForLocalPed(CPed* UNUSED_PARAM(pPed))
{
	return rage_new CTaskHeliChase(m_Target, m_vTargetOffset);
}

CTaskFSMClone* CTaskHeliChase::CreateTaskForClonePed(CPed* UNUSED_PARAM(pPed))
{
	return rage_new CTaskHeliChase(m_Target, m_vTargetOffset);
}

const CPhysical* CTaskHeliChase::GetDominantTarget() const
{
	//Ensure the target entity is valid.
	const CEntity* pEntity = m_Target.GetEntity();
	if(!pEntity)
	{
		return NULL;
	}

	//Check if the entity is a ped.
	if(pEntity->GetIsTypePed())
	{
		//Grab the ped.
		const CPed* pPed = static_cast<const CPed *>(pEntity);

		//Check if the ped is in a vehicle.
		const CVehicle* pVehicle = pPed->GetVehiclePedInside();
		if(pVehicle)
		{
			return pVehicle;
		}

		return pPed;
	}
	//Check if the entity is a vehicle.
	else if(pEntity->GetIsTypeVehicle())
	{
		return static_cast<const CVehicle *>(pEntity);
	}
	else
	{
		return NULL;
	}
}

CHeli* CTaskHeliChase::GetHeli() const
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

CTask::FSM_Return CTaskHeliChase::ProcessPreFSM()
{
	if (!GetPed()->IsNetworkClone())
	{
		//Ensure the heli is valid.
		if(!GetHeli())
		{
			return FSM_Quit;
		}

		//Ensure the dominant target is valid.
		if(!GetDominantTarget())
		{
			return FSM_Quit;
		}
	
		//Process the drift.
		ProcessDrift();
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskHeliChase::UpdateFSM(const s32 iState, const FSM_Event iEvent)
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

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskHeliChase::Start_OnUpdate()
{
	//Move to the pursue state.
	SetState(State_Pursue);
	
	return FSM_Continue;
}

void CTaskHeliChase::Pursue_OnEnter()
{
	//Grab the heli.
	CHeli* pHeli = GetHeli();
	
	//Create the params.
	sVehicleMissionParams params;
	params.SetTargetEntity(const_cast<CEntity *>(static_cast<const CEntity *>(GetDominantTarget())));
	params.SetTargetPosition(VEC3V_TO_VECTOR3(CalculateTargetPosition()));
	params.m_iDrivingFlags = DF_DontTerminateTaskWhenAchieved|DF_TargetPositionOverridesEntity;
	params.m_fTargetArriveDist = 0.0f;
	params.m_fCruiseSpeed = CalculateSpeed();
	
	//Create the vehicle task.
	CTaskVehicleGoToHelicopter* pVehicleTask = rage_new CTaskVehicleGoToHelicopter(params, 0, -1.0f, sm_Tunables.m_MinHeightAboveTerrain);
	
	//Set the slow down distance.
	pVehicleTask->SetSlowDownDistance(sm_Tunables.m_SlowDownDistanceMin);
	
	// intialize value to target forward
	m_vTargetVelocitySmoothed = GetDominantTarget()->GetTransform().GetForward();
	
	//Create the task.
	CTask* pTask = rage_new CTaskControlVehicle(pHeli, pVehicleTask);

	//Start the task.
	SetNewTask(pTask);
}

void CTaskHeliChase::Pursue_OnEnterClone()
{
	CreateSubTaskFromClone(GetPed(), CTaskTypes::TASK_CONTROL_VEHICLE);
}

CTask::FSM_Return CTaskHeliChase::Pursue_OnUpdate()
{
	//Update the vehicle mission.
	UpdateVehicleMission();
	
	//Check if the sub-task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Finish the task.
		SetState(State_Finish);
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskHeliChase::Pursue_OnUpdateClone()
{
	if (m_CloneQuit)
	{
		return FSM_Quit;
	}
	else
	{
		if (!GetNewTask() && !GetSubTask())
		{
			CreateSubTaskFromClone(GetPed(), CTaskTypes::TASK_CONTROL_VEHICLE);
		}

		return FSM_Continue;
	}
}

bool CTaskHeliChase::CalculateTargetOrientation(float& fOrientation) const
{
	if ( m_OrientationMode != OrientationMode_None )
	{
		//Grab the target.
		const CPhysical* pTarget = GetDominantTarget();

		//Grab the heli.
		const CHeli* pHeli = GetHeli();

		//Grab the heli position.
		Vec3V vHeliPosition = pHeli->GetTransform().GetPosition();
		Vec3V vHeliPositionXY = vHeliPosition;
		vHeliPositionXY.SetZ(ScalarV(V_ZERO));

		//Calculate the target position.
		Vec3V vTargetPosition = CalculateTargetPosition();
		Vec3V vTargetPositionXY = vTargetPosition;
		vTargetPositionXY.SetZ(ScalarV(V_ZERO));

		//Ensure the distance is within the threshold.
		ScalarV scDistSq = DistSquared(vHeliPosition, vTargetPosition);
		ScalarV scMaxDistSq = ScalarVFromF32( m_OrientationMode == OrientationMode_OrientNearArrival ? square(sm_Tunables.m_NearDistanceForOrientation) : square(sm_Tunables.m_MaxDistanceForOrientation));
		if(  m_OrientationMode == OrientationMode_OrientAllTheTime 
			|| IsLessThanAll(scDistSq, scMaxDistSq) )
		{
			if ( m_OrientationRelative == OrientationRelative_HeliToTarget )
			{
				// lets not use the offset for this computation
				const CPhysical* pTarget = GetDominantTarget();	
				Vec3V vTargetPosition = pTarget->GetTransform().GetPosition();
				Vec3V vTargetPositionXY = vTargetPosition;
				vTargetPositionXY.SetZ(ScalarV(V_ZERO));
				
				// direction to target xy
				Vec3V vDirToTarget = Normalize(vTargetPositionXY - vHeliPositionXY);

				//Calculate the orientation.
				//This doesn't really seem right, but I'm using it anyways since this is what the
				//other heli tasks use.  I want the heli to face in the same direction as the target.
				fOrientation = fwAngle::GetATanOfXY(vDirToTarget.GetXf(), vDirToTarget.GetYf());
			}
			else if ( m_OrientationRelative == OrientationRelative_TargetForward )
			{
				//Grab the target's forward vector.
				Vec3V vTargetForward = pTarget->GetTransform().GetForward();

				//Calculate the orientation.
				//This doesn't really seem right, but I'm using it anyways since this is what the
				//other heli tasks use.  I want the heli to face in the same direction as the target.
				fOrientation = fwAngle::GetATanOfXY(vTargetForward.GetXf(), vTargetForward.GetYf());
			}
			else if ( m_OrientationRelative == OrientationRelative_TargetVelocity )
			{
				fOrientation = fwAngle::GetATanOfXY(m_vTargetVelocitySmoothed.GetXf(), m_vTargetVelocitySmoothed.GetYf());
			}
			else // OrientationRelative_World
			{
				fOrientation = 0.0f;
			}

			fOrientation += m_fOrientationOffset;

			return true;
		}
	}
	return false;
}

Vec3V_Out CTaskHeliChase::CalculateTargetPosition() const
{
	//Grab the target.
	const CPhysical* pTarget = GetDominantTarget();

	//Grab the target position.
	Vec3V vTargetPosition = pTarget->GetTransform().GetPosition();

	//Calculate the offset.
	Vec3V vOffset = m_vTargetOffset;
	if (m_DriftMode == DriftMode_Enabled )
	{
		vOffset = Add(m_vTargetOffset, GetDrift());
	}
	
	if ( m_OffsetRelative == OffsetRelative_World )
	{
		return vTargetPosition + vOffset;
	}
	else
	{
		//Grab the target's velocity.
		Vec3V vTargetVelocity = m_vTargetVelocitySmoothed;

		//Grab the target's forward vector.
		Vec3V vTargetForward = pTarget->GetTransform().GetForward();

		//Calculate the forward vector.
		Vec3V vForward = NormalizeFastSafe(vTargetVelocity, vTargetForward);

		//Grab the up vector.
		Vec3V vUp = Vec3V(V_Z_AXIS_WZERO);

		//Calculate the side vector.
		Vec3V vSide = Cross(vForward, vUp);
		vSide = NormalizeFastSafe(vSide, Vec3V(V_X_AXIS_WZERO));

		//Calculate the up vector.
		vUp = Cross(vSide, vForward);
		vUp = NormalizeFastSafe(vUp, Vec3V(V_Z_AXIS_WZERO));

		//Create a matrix transform.
		Mat34V mTransform;
		mTransform.Seta(vSide);
		mTransform.Setb(vForward);
		mTransform.Setc(vUp);
		mTransform.Setd(vTargetPosition);

		//Transform the offset to world coordinates.
		return Transform(mTransform, vOffset);
	}
}

float CTaskHeliChase::CalculateSpeed() const
{
	//Calculate the base cruise speed.
	float fCruiseSpeed = sm_Tunables.m_CruiseSpeed;

	//If we are chasing a vehicle, use their maximum speed.
	const CPhysical* pTarget = GetDominantTarget();
	if(pTarget->GetIsTypeVehicle())
	{
		//Get the target vehicle.
		const CVehicle* pTargetVehicle = static_cast<const CVehicle *>(pTarget);

		float fMaxCruiseSpeed = pTargetVehicle->m_Transmission.GetDriveMaxVelocity();
		fCruiseSpeed = Max(fCruiseSpeed, fMaxCruiseSpeed);

		//Check if the target vehicle is an aircraft.
		if(pTargetVehicle->GetIsAircraft())
		{
			fMaxCruiseSpeed = GetHeli()->m_Transmission.GetDriveMaxVelocity();
			fCruiseSpeed = Max(fCruiseSpeed, fMaxCruiseSpeed);
		}
	}

	//Check if we are allowed to go faster to catch up.
	bool bAllowedToGoFasterToCatchUp = CTheScripts::GetPlayerIsOnAMission() ||
		(CGameWorld::FindLocalPlayerWanted() && (CGameWorld::FindLocalPlayerWanted()->GetWantedLevel() <= WANTED_CLEAN));
	if(bAllowedToGoFasterToCatchUp)
	{
		//Check if we are far away.
		ScalarV scDistSq = DistSquared(GetPed()->GetTransform().GetPosition(), CalculateTargetPosition());
		static dev_float s_fMinDistance = 50.0f;
		ScalarV scMinDistSq = ScalarVFromF32(square(s_fMinDistance));
		if(IsGreaterThanAll(scDistSq, scMinDistSq))
		{
			//Use the heli's maximum speed.
			fCruiseSpeed = GetHeli()->m_Transmission.GetDriveMaxVelocity();
		}
	}
	
	//Apply the multiplier.
	fCruiseSpeed *= GetPed()->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatHeliSpeedModifier);
	
	return fCruiseSpeed;
}

Vec3V_Out CTaskHeliChase::GetDrift() const
{
	return Vec3V(m_DriftHelperX.GetValue(), m_DriftHelperY.GetValue(), m_DriftHelperZ.GetValue());
}

void CTaskHeliChase::ProcessDrift()
{
	//Process the X drift.
	m_DriftHelperX.Update(sm_Tunables.m_DriftX.m_MinValueForCorrection, sm_Tunables.m_DriftX.m_MaxValueForCorrection,
		sm_Tunables.m_DriftX.m_MinRate, sm_Tunables.m_DriftX.m_MaxRate);
	
	//Process the Y drift.
	m_DriftHelperY.Update(sm_Tunables.m_DriftY.m_MinValueForCorrection, sm_Tunables.m_DriftY.m_MaxValueForCorrection,
		sm_Tunables.m_DriftY.m_MinRate, sm_Tunables.m_DriftY.m_MaxRate);
	
	//Process the Z drift.
	m_DriftHelperZ.Update(sm_Tunables.m_DriftZ.m_MinValueForCorrection, sm_Tunables.m_DriftZ.m_MaxValueForCorrection,
		sm_Tunables.m_DriftZ.m_MinRate, sm_Tunables.m_DriftZ.m_MaxRate);
}

void CTaskHeliChase::UpdateVehicleMission()
{
	//Grab the heli.
	CHeli* pHeli = GetHeli();
	
	//Ensure the vehicle task is valid.
	CTaskVehicleMissionBase* pVehicleTask = pHeli->GetIntelligence()->GetActiveTask();
	if(!pVehicleTask)
	{
		return;
	}

	//Ensure the vehicle task is the correct type.
	if(pVehicleTask->GetTaskType() != CTaskTypes::TASK_VEHICLE_GOTO_HELICOPTER)
	{
		return;
	}

	//Grab the heli task.
	CTaskVehicleGoToHelicopter* pTask = static_cast<CTaskVehicleGoToHelicopter *>(pVehicleTask);
	
	static float s_Smooth = 5.0f;
	//Calculate the target position.
	Vec3V vDesiredTarget = CalculateTargetPosition();
	Vec3V vCurrentTarget = VECTOR3_TO_VEC3V(*pTask->GetTargetPosition());
	Vec3V vSmoothTarget = Lerp(ScalarV(s_Smooth * GetTimeStep()), vCurrentTarget, vDesiredTarget);

	//Set the target position.
	vSmoothTarget = Clamp( vSmoothTarget, Vec3V(WORLDLIMITS_XMIN, WORLDLIMITS_YMIN, WORLDLIMITS_ZMIN), Vec3V(WORLDLIMITS_XMAX, WORLDLIMITS_YMAX, WORLDLIMITS_ZMAX));
	pTask->SetTargetPosition(&RCC_VECTOR3(vSmoothTarget));

	//Calculate the target orientation.
	float fOrientation = 0.0f;
	bool bRequestOrientation = CalculateTargetOrientation(fOrientation);

	// this basically works but it's not linear.  If you want it to be linear you need to keep
        // a start time and a start velocity.
	m_vTargetVelocitySmoothed = Lerp(ScalarVFromF32(fwTimer::GetTimeStep()), m_vTargetVelocitySmoothed, VECTOR3_TO_VEC3V(GetDominantTarget()->GetVelocity()));

	float fTargetSpeed = GetDominantTarget()->GetVelocity().Mag();
	float fMaxSpeed = 15.0f; // arbitrary 
	float t = Min( fTargetSpeed / fMaxSpeed, 1.0f);
	float fSlowDownDistance = Lerp(t,  sm_Tunables.m_SlowDownDistanceMax, sm_Tunables.m_SlowDownDistanceMin );
	pTask->SetSlowDownDistance( fSlowDownDistance );


	//Set the requested orientation.
	pTask->SetOrientation(fOrientation);
	pTask->SetOrientationRequested(bRequestOrientation);
	pTask->SetCruiseSpeed(CalculateSpeed());
}
