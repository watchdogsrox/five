// File header
#include "vehicleAi/task/TaskVehiclePullAlongside.h"

// Game headers
#include "peds/ped.h"
#include "Vehicles/Automobile.h"
#include "Vehicles/vehicle.h"
#include "vehicleAi/task/TaskVehicleGoToAutomobile.h"
#include "VehicleAi/VehicleIntelligence.h"
#include "vehicleAi/task/TaskVehicleThreePointTurn.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

//=========================================================================
// CTaskVehiclePullAlongside
//=========================================================================

CTaskVehiclePullAlongside::Tunables CTaskVehiclePullAlongside::sm_Tunables;
IMPLEMENT_VEHICLE_AI_TASK_TUNABLES(CTaskVehiclePullAlongside, 0xcdffceef);

CTaskVehiclePullAlongside::CTaskVehiclePullAlongside(const sVehicleMissionParams& rParams, float fStraightLineDistance, bool bFromInfront)
: CTaskVehicleMissionBase(rParams)
, m_fStraightLineDistance(fStraightLineDistance)
, m_uRunningFlags(0)
{
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_PULL_ALONGSIDE);
	if(bFromInfront)
	{
		m_uRunningFlags.SetFlag(RF_FromInfront);
	}
}

CTaskVehiclePullAlongside::~CTaskVehiclePullAlongside()
{

}

#if !__FINAL
void CTaskVehiclePullAlongside::Debug() const
{
	CTask::Debug();
}

const char* CTaskVehiclePullAlongside::GetStaticStateName(s32 iState)
{
	Assert(iState >= State_Start && iState <= State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_GetInPosition",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif // !__FINAL

aiTask* CTaskVehiclePullAlongside::Copy() const
{
	return rage_new CTaskVehiclePullAlongside(m_Params, m_fStraightLineDistance, m_uRunningFlags.IsFlagSet(RF_FromInfront));
}

void CTaskVehiclePullAlongside::CleanUp()
{
	//Grab the vehicle.
	CVehicle* pVehicle = GetVehicle();

	//Clear the avoidance cache.
	pVehicle->GetIntelligence()->ClearAvoidanceCache();
}

CTask::FSM_Return CTaskVehiclePullAlongside::ProcessPreFSM()
{
	//Ensure the target vehicle is valid.
	if(!GetTargetVehicle())
	{
		return FSM_Quit;
	}

	//Ensure the vehicle to pull alongside is valid.
	if(!GetVehicleToPullAlongside())
	{
		return FSM_Quit;
	}

	//Ensure the target ped is valid.
	if(!GetTargetPed())
	{
		return FSM_Quit;
	}

	//Disable avoidance projection.
	GetVehicle()->m_nVehicleFlags.bDisableAvoidanceProjection = true;

	return FSM_Continue;
}

CTask::FSM_Return CTaskVehiclePullAlongside::UpdateFSM(const s32 iState, const FSM_Event iEvent)
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

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskVehiclePullAlongside::Start_OnUpdate()
{
	//Grab the vehicle.
	CVehicle* pVehicle = GetVehicle();

	//Set the avoidance cache.
	CVehicleIntelligence::AvoidanceCache& rCache = pVehicle->GetIntelligence()->GetAvoidanceCache();
	rCache.m_pTarget = GetTargetVehicle();
	rCache.m_fDesiredOffset = 0.0f;
	
	//Get in position.
	SetState(State_GetInPosition);

	return FSM_Continue;
}

void CTaskVehiclePullAlongside::GetInPosition_OnEnter()
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

CTask::FSM_Return CTaskVehiclePullAlongside::GetInPosition_OnUpdate()
{
	//Update the vehicle mission.
	UpdateVehicleMissionForGetInPosition();
	
	//Check if the sub-task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Finish the task.
		SetState(State_Finish);
	}
	
	return FSM_Continue;
}

const CPed* CTaskVehiclePullAlongside::GetTargetPed() const
{
	//Get the target vehicle.
	const CVehicle* pTargetVehicle = GetVehicleToPullAlongside();
	taskAssert(pTargetVehicle);

	return (pTargetVehicle->GetDriver());
}

const CVehicle* CTaskVehiclePullAlongside::GetTargetVehicle() const
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

const CVehicle* CTaskVehiclePullAlongside::GetVehicleToPullAlongside() const
{
	//Get the target vehicle.
	const CVehicle* pTargetVehicle = GetTargetVehicle();
	taskAssert(pTargetVehicle);

	//Check if the target vehicle is a trailer.
	if(pTargetVehicle->InheritsFromTrailer())
	{
		//Check if the attach parent vehicle is valid.
		const CVehicle* pAttachParentVehicle = pTargetVehicle->GetAttachParentVehicle();
		if(pAttachParentVehicle)
		{
			return pAttachParentVehicle;
		}
	}

	return pTargetVehicle;
}

void CTaskVehiclePullAlongside::UpdateCruiseSpeedForGetInPosition(CTaskVehicleMissionBase& rTask)
{
	//Grab the target ped.
	const CPed* pTargetPed = GetTargetPed();

	//Transform the target ped's position into local space for the vehicle.
	Vec3V vTargetPedPositionLocalToVehicle = GetVehicle()->GetTransform().UnTransform(pTargetPed->GetTransform().GetPosition());

	//Calculate the overlap.
	float fOverlap = vTargetPedPositionLocalToVehicle.GetYf();

	//Check if we have pulled alongside.
	if(Abs(fOverlap) < 1.0f)
	{
		m_uRunningFlags.SetFlag(RF_HasBeenAlongside);
	}

	//Calculate the speed difference.
	float fSpeedDifference = Clamp(fOverlap * sm_Tunables.m_OverlapSpeedMultiplier, -sm_Tunables.m_MaxSpeedDifference, sm_Tunables.m_MaxSpeedDifference);
	
	//Grab the target vehicle.
	const CVehicle* pTargetVehicle = GetVehicleToPullAlongside();
	
	//Calculate the cruise speed.
	float fCruiseSpeed = pTargetVehicle->GetAiXYSpeed();
	fCruiseSpeed += fSpeedDifference;
	fCruiseSpeed = Max(0.0f, fCruiseSpeed);
	
	//Set the cruise speed.
	rTask.SetCruiseSpeed(fCruiseSpeed);
}

void CTaskVehiclePullAlongside::UpdateTargetPositionForGetInPosition(CTaskVehicleMissionBase& rTask)
{
	//Grab the target vehicle.
	const CVehicle* pTargetVehicle = GetVehicleToPullAlongside();
	
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

void CTaskVehiclePullAlongside::UpdateVehicleMissionForGetInPosition()
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
	
	if(m_uRunningFlags.IsFlagSet(RF_FromInfront))
	{
		//Update the target position.
		bool bCruiseSpeedSet = UpdateTargetPositionForGetInPositionInfront(*pTask);

		//Update the cruise speed.
		UpdateCruiseSpeedForGetInPositionInfront(*pTask, bCruiseSpeedSet);
	}
	else
	{
		//Update the target position.
		UpdateTargetPositionForGetInPosition(*pTask);

		//add additional buffer for cloned vehicles
		CVehicle* pVehicle = GetVehicle();
		CVehicleIntelligence::AvoidanceCache& rCache = pVehicle->GetIntelligence()->GetAvoidanceCache();
		rCache.m_fAdditionalBuffer = GetVehicleToPullAlongside()->IsNetworkClone() ? 0.5f : 0.0f;

		//Update the cruise speed.
		UpdateCruiseSpeedForGetInPosition(*pTask);
	}
}

bool CTaskVehiclePullAlongside::UpdateTargetPositionForGetInPositionInfront(CTaskVehicleMissionBase& rTask)
{
	bool bCruiseSpeedSet = false;

	//Grab the target vehicle.
	const CVehicle* pTargetVehicle = GetVehicleToPullAlongside();

	//Grab the target vehicle position.
	Vec3V vTargetVehiclePosition = pTargetVehicle->GetTransform().GetPosition();

	//calculate how orientated with the target vehicle we are
	ScalarV vOneOverDotForward = ScalarV(V_ONE) / Dot(pTargetVehicle->GetTransform().GetForward(), GetVehicle()->GetTransform().GetForward());

	//get distance between vehicles and a buffer that we want to aim for ahead
	ScalarV vDist = rage::Mag(GetVehicle()->GetTransform().GetPosition() - vTargetVehiclePosition) + ScalarV(V_FIVE);

	//scale distance by how orientated we are
	vDist = Scale(vDist, vOneOverDotForward);

	//scale target forward by distance required to get ahead of us by 5 meters
	Vec3V vTargetVehicleOffset = Scale(pTargetVehicle->GetTransform().GetForward(), vDist);

	//calculate if we're on the right or left of target forward
	Vec3V vTargetVehicleOffsetNorm = Normalize(vTargetVehiclePosition - GetVehicle()->GetTransform().GetPosition());
	bool bOnRight = IsLessThanAll(rage::Dot(GetTargetVehicle()->GetVehicleRightDirection(), vTargetVehicleOffsetNorm), ScalarV(V_ZERO) ) != 0;
	
	//calculate how much side leeway we want to give
	float fAlongsideBuffer = GetRequiredAlongsideBuffer();
	Vec3V vForwardOffset = Scale(GetTargetVehicle()->GetVehicleRightDirection(), ScalarV(fAlongsideBuffer + sm_Tunables.m_AlongsideBuffer));

	//offset target vehicle to the side by our buffer
	Vec3V vTargetVehicleOffsetWithSide = Add(vTargetVehicleOffset, (bOnRight ? vForwardOffset : -vForwardOffset));

	//Calculate the target position.
	Vec3V vTargetPosition = Add(vTargetVehiclePosition, vTargetVehicleOffsetWithSide);

	//ensure position is on road
	if(!CTaskVehicleThreePointTurn::IsPointOnRoad(RCC_VECTOR3(vTargetPosition), GetVehicle()))
	{
		//if not, test position with smaller buffer
		Vec3V vSmallerForwardOffset = Scale(GetTargetVehicle()->GetVehicleRightDirection(), ScalarV(fAlongsideBuffer + 0.5f));
		vTargetVehicleOffsetWithSide = Add(vTargetVehicleOffset, (bOnRight ? vSmallerForwardOffset : -vSmallerForwardOffset));
		Vec3V vNewTargetPosition = Add(vTargetVehiclePosition, vTargetVehicleOffsetWithSide);
		if(CTaskVehicleThreePointTurn::IsPointOnRoad(RCC_VECTOR3(vNewTargetPosition), GetVehicle()))
		{
			vTargetPosition = vNewTargetPosition;
		}
		else
		{
			//we're getting squished into a wall - determine if we can just brake sharply to avoid it
			if(CanBrakeToAvoidCollision())
			{
				//Set the cruise speed.
				rTask.SetCruiseSpeed(0.0f);
				bCruiseSpeedSet = true;
				vTargetPosition = vNewTargetPosition;
			}
			else
			{
				//the target car is behind us - check if we've got enough space to drive to other side of car
				Vec3V vTargetVehiclePositionLocalToVehicle = GetVehicle()->GetTransform().UnTransform(pTargetVehicle->GetTransform().GetPosition());
				if(vTargetVehiclePositionLocalToVehicle.GetYf() > GetRequiredForwardBuffer())
				{
					//calculate position on other side of target car
					vTargetVehicleOffsetWithSide = Add(vTargetVehicleOffset, (bOnRight ? -vForwardOffset : vForwardOffset));
					Vec3V vOtherTargetPosition = Add(vTargetVehiclePosition, vTargetVehicleOffsetWithSide);

					//if(CTaskVehicleThreePointTurn::IsPointOnRoad(RCC_VECTOR3(vOtherTargetPosition), GetVehicle()))
					{
						vTargetPosition = vOtherTargetPosition;
					}

					//make sure we're going faster than target as we cross over
					rTask.SetCruiseSpeed(pTargetVehicle->GetAiXYSpeed() + 5.0f);
					bCruiseSpeedSet = true;
				}
			}	
		}
	}

	//Set the target position.
	rTask.SetTargetPosition(&RCC_VECTOR3(vTargetPosition));
	return bCruiseSpeedSet;
}

void CTaskVehiclePullAlongside::UpdateCruiseSpeedForGetInPositionInfront(CTaskVehicleMissionBase& rTask, bool bCruiseSpeedSet)
{
	//Grab the target ped.
	const CPed* pTargetPed = GetTargetPed();

	//Transform the target ped's position into local space for the vehicle.
	Vec3V vTargetPedPositionLocalToVehicle = GetVehicle()->GetTransform().UnTransform(pTargetPed->GetTransform().GetPosition());

	//Calculate the overlap.
	float fOverlap = vTargetPedPositionLocalToVehicle.GetYf();

	//Check if we have pulled alongside.
	if(Abs(fOverlap) < 1.0f)
	{
		m_uRunningFlags.SetFlag(RF_HasBeenAlongside);
	}

	if(!bCruiseSpeedSet)
	{
		//Grab the target vehicle.
		const CVehicle* pTargetVehicle = GetVehicleToPullAlongside();

		//Calculate the cruise speed.
		float fCruiseSpeed = pTargetVehicle->GetAiXYSpeed();

		//make sure we're not in the way of target vehicle before slowing down
		if(fabsf(vTargetPedPositionLocalToVehicle.GetXf()) > GetRequiredAlongsideBuffer())
		{
			//Calculate the speed difference.
			float fSpeedDifference = Clamp(fabsf(fOverlap) * sm_Tunables.m_OverlapSpeedMultiplier, 0.0f, sm_Tunables.m_MaxSpeedDifference);

			fCruiseSpeed -= fSpeedDifference;
			fCruiseSpeed = Max(5.0f, fCruiseSpeed);
		}

		//Set the cruise speed.
		rTask.SetCruiseSpeed(fCruiseSpeed);
	}
}

bool CTaskVehiclePullAlongside::CanBrakeToAvoidCollision() const
{
	//Grab the target ped.
	const CPed* pTargetPed = GetTargetPed();

	//Transform the target ped's position into local space for the vehicle.
	Vec3V vTargetPedPositionLocalToVehicle = GetVehicle()->GetTransform().UnTransform(pTargetPed->GetTransform().GetPosition());
	return fabsf(vTargetPedPositionLocalToVehicle.GetXf()) > GetRequiredAlongsideBuffer();
}

float CTaskVehiclePullAlongside::GetRequiredAlongsideBuffer() const
{
	spdAABB boundingBox;
	const spdAABB& targetBounds = GetTargetVehicle()->GetLocalSpaceBoundBox(boundingBox);
	const spdAABB& ourBounds = GetVehicle()->GetLocalSpaceBoundBox(boundingBox);

	return targetBounds.GetMax().GetXf() + ourBounds.GetMax().GetXf();
}


float CTaskVehiclePullAlongside::GetRequiredForwardBuffer() const
{
	spdAABB boundingBox;
	const spdAABB& targetBounds = GetTargetVehicle()->GetLocalSpaceBoundBox(boundingBox);
	const spdAABB& ourBounds = GetVehicle()->GetLocalSpaceBoundBox(boundingBox);

	return targetBounds.GetMax().GetYf() + ourBounds.GetMax().GetYf();
}