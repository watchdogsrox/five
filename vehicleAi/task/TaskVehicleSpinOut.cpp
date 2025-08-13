// File header
#include "TaskVehicleSpinOut.h"

// Game headers
#include "peds/ped.h"
#include "Vehicles/Automobile.h"
#include "Vehicles/vehicle.h"
#include "vehicleAi/task/TaskVehicleGoToAutomobile.h"
#include "VehicleAi/task/TaskVehicleTempAction.h"
#include "VehicleAi/VehicleIntelligence.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

//=========================================================================
// CTaskVehicleSpinOut
//=========================================================================

CTaskVehicleSpinOut::Tunables CTaskVehicleSpinOut::sm_Tunables;
IMPLEMENT_VEHICLE_AI_TASK_TUNABLES(CTaskVehicleSpinOut, 0x394adcff);

CTaskVehicleSpinOut::CTaskVehicleSpinOut(const sVehicleMissionParams& rParams, float fStraightLineDistance)
: CTaskVehicleMissionBase(rParams)
, m_fStraightLineDistance(fStraightLineDistance)
, m_uRunningFlags(0)
{
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_SPIN_OUT);
}

CTaskVehicleSpinOut::~CTaskVehicleSpinOut()
{

}

#if !__FINAL
void CTaskVehicleSpinOut::Debug() const
{
	CTask::Debug();
}

const char* CTaskVehicleSpinOut::GetStaticStateName(s32 iState)
{
	Assert(iState >= State_Start && iState <= State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_GetInPosition",
		"State_Turn",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif // !__FINAL

aiTask* CTaskVehicleSpinOut::Copy() const
{
	return rage_new CTaskVehicleSpinOut(m_Params, m_fStraightLineDistance);
}

void CTaskVehicleSpinOut::CleanUp()
{
	//Grab the vehicle.
	CVehicle* pVehicle = GetVehicle();

	//Clear the avoidance cache.
	pVehicle->GetIntelligence()->ClearAvoidanceCache();
	
	//Clear the inv mass scale override.
	pVehicle->GetIntelligence()->ClearInvMassScaleOverride();
}

CTask::FSM_Return CTaskVehicleSpinOut::ProcessPreFSM()
{
	//Ensure the target vehicle is valid.
	if(!GetTargetVehicle())
	{
		return FSM_Quit;
	}

	GetVehicle()->m_nVehicleFlags.bDisableAvoidanceProjection = true;

	return FSM_Continue;
}

CTask::FSM_Return CTaskVehicleSpinOut::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_GetInPosition)
			FSM_OnEnter
				GetInPosition_OnEnter();
			FSM_OnUpdate
				return GetInPosition_OnUpdate();
				
		FSM_State(State_Turn)
			FSM_OnEnter
				Turn_OnEnter();
			FSM_OnUpdate
				return Turn_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskVehicleSpinOut::Start_OnUpdate()
{
	//Grab the vehicle.
	CVehicle* pVehicle = GetVehicle();

	//Set the avoidance cache.
	CVehicleIntelligence::AvoidanceCache& rCache = pVehicle->GetIntelligence()->GetAvoidanceCache();
	rCache.m_pTarget = GetTargetVehicle();
	rCache.m_fDesiredOffset = 0.0f;
	
	//Set the inv mass scale override.
	pVehicle->GetIntelligence()->SetInvMassScaleOverride(GetTargetVehicle(), sm_Tunables.m_InvMassScale);
	
	//Get in position.
	SetState(State_GetInPosition);

	return FSM_Continue;
}

void CTaskVehicleSpinOut::GetInPosition_OnEnter()
{
	//Create the vehicle mission parameters.
	sVehicleMissionParams params = m_Params;
	
	//Use the target position.
	params.m_iDrivingFlags.SetFlag(DF_TargetPositionOverridesEntity);
	
	//Create the task.
	CTask* pTask = CVehicleIntelligence::CreateAutomobileGotoTask(params, m_fStraightLineDistance);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskVehicleSpinOut::GetInPosition_OnUpdate()
{
	//Update the vehicle mission.
	UpdateVehicleMissionForGetInPosition();
	
	//Check if we are in position.
	bool bCloseEnoughToTurn;
	if(IsInPosition(bCloseEnoughToTurn))
	{
		//Check if we are close enough to turn.
		if(bCloseEnoughToTurn)
		{
			//Turn into the target.
			SetState(State_Turn);
		}
		else
		{
			//Finish the task.
			SetState(State_Finish);
		}
	}
	//Check if the task has finished.
	else if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Finish the task.
		SetState(State_Finish);
	}
	
	return FSM_Continue;
}

void CTaskVehicleSpinOut::Turn_OnEnter()
{
	//Grab the vehicle.
	const CVehicle* pVehicle = GetVehicle();
	
	//Grab the target vehicle.
	const CVehicle* pTargetVehicle = GetTargetVehicle();
	
	//Grab the vehicle position.
	Vec3V vVehiclePosition = pVehicle->GetTransform().GetPosition();
	
	//Grab the target vehicle position.
	Vec3V vTargetVehiclePosition = pTargetVehicle->GetTransform().GetPosition();
	
	//Generate a vector from the vehicle to the target vehicle.
	Vec3V vVehicleToTargetVehicle = Subtract(vTargetVehiclePosition, vVehiclePosition);
	
	//Grab the vehicle's right vector.
	Vec3V vVehicleRight = pVehicle->GetTransform().GetRight();
	
	//Generate the dot product for the direction.
	ScalarV scDot = Dot(vVehicleToTargetVehicle, vVehicleRight);
	
	//Check if the target vehicle is on the right.
	bool bTargetIsOnRight = (IsGreaterThanAll(scDot, ScalarV(V_ZERO)) != 0);
	
	//Choose the direction.
	CTaskVehicleTurn::TurnAction nDirection = bTargetIsOnRight ? CTaskVehicleTurn::Turn_Right : CTaskVehicleTurn::Turn_Left;
	
	//Create the task.
	CTask* pTask = rage_new CTaskVehicleTurn(NetworkInterface::GetSyncedTimeInMilliseconds() + (u32)(sm_Tunables.m_TurnTime * 1000.0f), nDirection);
	
	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskVehicleSpinOut::Turn_OnUpdate()
{
	//Check for contact.
	CheckForContact();

	//Check if the sub-task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Finish the task.
		SetState(State_Finish);
	}
	
	return FSM_Continue;
}

void CTaskVehicleSpinOut::AnalyzePositions(float& fBumperOverlap, float& fSidePadding) const
{
	//Grab the vehicle.
	const CVehicle* pVehicle = GetVehicle();
	
	//Grab the target vehicle.
	const CVehicle* pTargetVehicle = GetTargetVehicle();
	
	//Grab the vehicle's max bounding box.
	const Vector3& vVehicleBoundingBoxMin = pVehicle->GetBoundingBoxMin();
	const Vector3& vVehicleBoundingBoxMax = pVehicle->GetBoundingBoxMax();
	
	//Grab the target vehicle's min bounding box.
	const Vector3& vTargetVehicleBoundingBoxMin = pTargetVehicle->GetBoundingBoxMin();
	const Vector3& vTargetVehicleBoundingBoxMax = pTargetVehicle->GetBoundingBoxMax();
	
	//Calculate the target vehicle's back bumper position.
	Vector3 vTargetVehicleBackBumperPositionLocal = VEC3_ZERO;
	vTargetVehicleBackBumperPositionLocal.y = vTargetVehicleBoundingBoxMin.y;
	Vec3V vTargetVehicleBackBumperPosition = pTargetVehicle->GetTransform().Transform(VECTOR3_TO_VEC3V(vTargetVehicleBackBumperPositionLocal));
	
	//Transform the target vehicle's back bumper position into local coordinates for the vehicle.
	Vector3 vTargetVehicleBackBumperPositionLocalToVehicle = VEC3V_TO_VECTOR3(pVehicle->GetTransform().UnTransform(vTargetVehicleBackBumperPosition));
	
	//Calculate the bumper overlap.
	float fVehicleFrontBumperY = vVehicleBoundingBoxMax.y;
	float fTargetVehicleBackBumperY = vTargetVehicleBackBumperPositionLocalToVehicle.y;
	fBumperOverlap = fVehicleFrontBumperY - fTargetVehicleBackBumperY;
	
	//Calculate the side padding.
	if(vTargetVehicleBackBumperPositionLocalToVehicle.x > 0.0f)
	{
		fSidePadding =  vTargetVehicleBackBumperPositionLocalToVehicle.x - vVehicleBoundingBoxMax.x + vTargetVehicleBoundingBoxMin.x;
	}
	else
	{
		fSidePadding = -vTargetVehicleBackBumperPositionLocalToVehicle.x + vVehicleBoundingBoxMin.x - vTargetVehicleBoundingBoxMax.x;
	}
}

void CTaskVehicleSpinOut::CheckForContact()
{
	//Ensure the flag is not set.
	if(m_uRunningFlags.IsFlagSet(RF_HasMadeContact))
	{
		return;
	}

	//Ensure we have made contact.
	if(!GetVehicle()->GetFrameCollisionHistory()->FindCollisionRecord(GetTargetVehicle()))
	{
		return;
	}

	//Set the flag.
	m_uRunningFlags.SetFlag(RF_HasMadeContact);
}

float CTaskVehicleSpinOut::GetBumperOverlap() const
{
	//Analyze the positions.
	float fBumperOverlap;
	float fSidePadding;
	AnalyzePositions(fBumperOverlap, fSidePadding);
	
	return fBumperOverlap;
}

const CVehicle* CTaskVehicleSpinOut::GetTargetVehicle() const
{
	//Grab the target entity.
	const CEntity* pTargetEntity = m_Params.GetTargetEntity().GetEntity();
	if(!pTargetEntity)
	{
		return NULL;
	}

	//Ensure the entity is a vehicle.
	if(!pTargetEntity->GetIsTypeVehicle())
	{
		return NULL;
	}

	return static_cast<const CVehicle *>(pTargetEntity);
}

bool CTaskVehicleSpinOut::IsInPosition(bool& bCloseEnoughToTurn) const
{
	//Analyze the positions.
	float fBumperOverlap;
	float fSidePadding;
	AnalyzePositions(fBumperOverlap, fSidePadding);
	
	//Ensure the bumper overlap exceeds the threshold.
	if(fBumperOverlap < sm_Tunables.m_BumperOverlapToBeInPosition)
	{
		return false;
	}
	
	//Check if we are close enough to turn.
	bCloseEnoughToTurn = (fSidePadding < sm_Tunables.m_MaxSidePaddingForTurn);
	
	return true;
}

void CTaskVehicleSpinOut::UpdateCruiseSpeedForGetInPosition(CTaskVehicleMissionBase& rTask)
{
	//Calculate the bumper overlap.
	float fBumperOverlap = GetBumperOverlap();
	
	//Clamp the bumper overlap to values we care about.
	fBumperOverlap = Clamp(fBumperOverlap, sm_Tunables.m_BumperOverlapForMaxSpeed, sm_Tunables.m_BumperOverlapForMinSpeed);
	
	//Normalize the value.
	float fValue = (fBumperOverlap - sm_Tunables.m_BumperOverlapForMaxSpeed) / (sm_Tunables.m_BumperOverlapForMinSpeed - sm_Tunables.m_BumperOverlapForMaxSpeed);
		
	//Grab the target vehicle.
	const CVehicle* pTargetVehicle = GetTargetVehicle();
	
	//Calculate the min cruise speed.
	float fMinCruiseSpeed = pTargetVehicle->GetAiXYSpeed();
	
	//Calculate the max cruise speed.
	float fMaxCruiseSpeed = fMinCruiseSpeed + sm_Tunables.m_CatchUpSpeed;
	
	//Calculate the cruise speed.
	float fCruiseSpeed = Lerp(fValue, fMaxCruiseSpeed, fMinCruiseSpeed);
	
	//Set the cruise speed.
	rTask.SetCruiseSpeed(fCruiseSpeed);
}

void CTaskVehicleSpinOut::UpdateTargetPositionForGetInPosition(CTaskVehicleMissionBase& rTask)
{
	//Grab the target vehicle.
	const CVehicle* pTargetVehicle = GetTargetVehicle();
	
	//Grab the target vehicle position.
	Vec3V vTargetVehiclePosition = pTargetVehicle->GetTransform().GetPosition();
	
	//Grab the target vehicle velocity.
	Vec3V vTargetVehicleVelocity = VECTOR3_TO_VEC3V(pTargetVehicle->GetVelocity());
	
	//Calculate the target vehicle offset.
	Vec3V vTargetVehicleOffset = Scale(vTargetVehicleVelocity, ScalarVFromF32(sm_Tunables.m_TimeToLookAhead));
	
	//Ensure the distance exceeds the threshold.
	ScalarV scDistSq = MagSquared(vTargetVehicleOffset);
	ScalarV scMinDist = ScalarVFromF32(sm_Tunables.m_MinDistanceToLookAhead);
	ScalarV scMinDistSq = Scale(scMinDist, scMinDist);
	if(IsLessThanAll(scDistSq, scMinDistSq))
	{
		//Grab the target vehicle forward.
		Vec3V vTargetVehicleForward = pTargetVehicle->GetTransform().GetForward();
		
		//Re-calculate the target vehicle offset using the min distance.
		vTargetVehicleOffset = Scale(vTargetVehicleForward, scMinDist);
	}
	
	//Calculate the target position.
	Vec3V vTargetPosition = Add(vTargetVehiclePosition, vTargetVehicleOffset);
	
	//Set the target position.
	rTask.SetTargetPosition(&RCC_VECTOR3(vTargetPosition));
}

void CTaskVehicleSpinOut::UpdateVehicleMissionForGetInPosition()
{
	//Grab the sub-task.
	CTask* pSubTask = GetSubTask();
	if(!pSubTask)
	{
		return;
	}
	
	//Ensure the sub-task is the correct type.
	if(!taskVerifyf(pSubTask->GetTaskType() == CTaskTypes::TASK_VEHICLE_GOTO_AUTOMOBILE_NEW, "Sub-task is the wrong type: %d.", pSubTask->GetTaskType()))
	{
		return;
	}
	
	//Grab the task.
	CTaskVehicleMissionBase* pTask = static_cast<CTaskVehicleMissionBase *>(pSubTask);
	
	//Update the target position.
	UpdateTargetPositionForGetInPosition(*pTask);
	
	//Update the cruise speed.
	UpdateCruiseSpeedForGetInPosition(*pTask);
}
