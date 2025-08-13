// File header
#include "TaskVehicleRam.h"

// Game headers
#include "Vehicles/Automobile.h"
#include "Vehicles/vehicle.h"
#include "VehicleAi\task\TaskVehicleMissionBase.h"
#include "VehicleAi\task\TaskVehicleGoto.h"
#include "vehicleAi/task/TaskVehicleGoToAutomobile.h"
#include "VehicleAi\task\TaskVehiclePark.h"
#include "VehicleAi\task\TaskVehicleTempAction.h"
#include "VehicleAi\VehicleIntelligence.h"
#include "peds/ped.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

//=========================================================================
// CTaskVehicleRam
//=========================================================================

CTaskVehicleRam::Tunables CTaskVehicleRam::sm_Tunables;

IMPLEMENT_VEHICLE_AI_TASK_TUNABLES(CTaskVehicleRam, 0xda50950b);

CTaskVehicleRam::CTaskVehicleRam(const sVehicleMissionParams& rParams, float fStraightLineDistance, fwFlags8 uConfigFlags)
: CTaskVehicleMissionBase(rParams)
, m_fStraightLineDistance(fStraightLineDistance)
, m_fBackOffTimer(0.0f)
, m_uNumContacts(0)
, m_uLastTimeMadeContact(0)
, m_uConfigFlags(uConfigFlags)
, m_uRunningFlags(0)
{
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_RAM);
}

CTaskVehicleRam::~CTaskVehicleRam()
{

}

#if !__FINAL
void CTaskVehicleRam::Debug() const
{
	CTask::Debug();
}

const char* CTaskVehicleRam::GetStaticStateName(s32 iState)
{
	Assert(iState >= State_Start && iState <= State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_Ram",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif // !__FINAL

void CTaskVehicleRam::CleanUp()
{
	//Grab the vehicle.
	CVehicle* pVehicle = GetVehicle();
	
	//Clear the avoidance cache.
	pVehicle->GetIntelligence()->ClearAvoidanceCache();
}

CTask::FSM_Return CTaskVehicleRam::ProcessPreFSM()
{
	//Ensure the target vehicle is valid.
	if(!GetTargetVehicle())
	{
		return FSM_Quit;
	}
	
	//Note that we are ramming.
	GetVehicle()->GetIntelligence()->Ramming();

	//Change the flag.
	bool bIsMakingContact = IsMakingContact();
	m_uRunningFlags.ChangeFlag(RF_IsMakingContact, bIsMakingContact);
	if(bIsMakingContact)
	{
		//Set the time.
		m_uLastTimeMadeContact = fwTimer::GetTimeInMilliseconds();
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskVehicleRam::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_Ram)
			FSM_OnEnter
				Ram_OnEnter();
			FSM_OnUpdate
				return Ram_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskVehicleRam::Start_OnUpdate()
{
	//Grab the vehicle.
	CVehicle* pVehicle = GetVehicle();

	//Set the avoidance cache.
	CVehicleIntelligence::AvoidanceCache& rCache = pVehicle->GetIntelligence()->GetAvoidanceCache();
	rCache.m_pTarget = GetTargetVehicle();
	rCache.m_fDesiredOffset = 0.0f;
	
	//Ram the target.
	SetState(State_Ram);

	return FSM_Continue;
}

void CTaskVehicleRam::Ram_OnEnter()
{
	//Create the vehicle mission parameters.
	sVehicleMissionParams params = m_Params;
	
	//Do not avoid the target.
	params.m_iDrivingFlags.SetFlag(DF_DontAvoidTarget);
	params.m_fTargetArriveDist = 0.0f;	//never quit for reaching the target
	
	//Create the task.
	CTask* pTask = CVehicleIntelligence::CreateAutomobileGotoTask(params, m_fStraightLineDistance);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskVehicleRam::Ram_OnUpdate()
{
	//Update the contact.
	UpdateContact();
	
	//Update the cruise speed.
	UpdateCruiseSpeed();
	
	//Check if the task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Finish the task.
		SetState(State_Finish);
	}

	return FSM_Continue;
}

const CVehicle* CTaskVehicleRam::GetTargetVehicle() const
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

bool CTaskVehicleRam::IsMakingContact() const
{
	//Grab the vehicle.
	const CVehicle* pVehicle = GetVehicle();

	//Ensure the target vehicle is valid.
	const CVehicle* pTargetVehicle = GetTargetVehicle();

	return (pVehicle->GetFrameCollisionHistory()->FindCollisionRecord(pTargetVehicle) != NULL);
}

void CTaskVehicleRam::UpdateContact()
{
	//Ensure we are not backed off.
	if(m_fBackOffTimer > 0.0f)
	{
		return;
	}

	//Ensure we are making contact.
	if(!m_uRunningFlags.IsFlagSet(RF_IsMakingContact))
	{
		return;
	}
	
	//Check if the ram is not continuous.
	if(!m_uConfigFlags.IsFlagSet(CF_Continuous))
	{
		//Set the back-off timer.
		m_fBackOffTimer = sm_Tunables.m_BackOffTimer;
	}
	
	//Increase the number of contacts.
	++m_uNumContacts;
}

void CTaskVehicleRam::UpdateCruiseSpeed()
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

	//Check if the back off timer is valid.
	if(m_fBackOffTimer > 0.0f)
	{
		//Decrease the back off timer.
		m_fBackOffTimer -= GetTimeStep();
	}
	
	//Grab the vehicle.
	const CVehicle* pVehicle = GetVehicle();
	
	//Grab the target vehicle.
	const CVehicle* pTargetVehicle = GetTargetVehicle();
	
	//Calculate the cruise speed.
	float fCruiseSpeed;
	if(m_fBackOffTimer > 0.0f)
	{
		//Grab the vehicle position.
		Vec3V vVehiclePosition = pVehicle->GetVehiclePosition();
		
		//Grab the target vehicle position.
		Vec3V vTargetVehiclePosition = pTargetVehicle->GetVehiclePosition();
		
		//Calculate the distance.
		float fDistance = Dist(vVehiclePosition, vTargetVehiclePosition).Getf();
		
		//Clamp the distance to values we care about.
		fDistance = Clamp(fDistance, sm_Tunables.m_MinBackOffDistance, sm_Tunables.m_MaxBackOffDistance);
		
		//Normalize the value.
		float fLerp = fDistance - sm_Tunables.m_MinBackOffDistance;
		fLerp /= (sm_Tunables.m_MaxBackOffDistance - sm_Tunables.m_MinBackOffDistance);
		
		//Calculate the cruise speed multiplier.
		float fCruiseSpeedMultiplier = Lerp(fLerp, sm_Tunables.m_CruiseSpeedMultiplierForMinBackOffDistance, sm_Tunables.m_CruiseSpeedMultiplierForMaxBackOffDistance);
		
		//Grab the target vehicle speed.
		float fTargetVehicleSpeed = pTargetVehicle->GetAiVelocity().XYMag();
		
		//Assign the cruise speed.
		fCruiseSpeed = fTargetVehicleSpeed * fCruiseSpeedMultiplier;
	}
	else
	{
		//Assign the cruise speed.
		fCruiseSpeed = m_Params.m_fCruiseSpeed;
	}

	//Check if we are doing a continuous ram.
	if(m_uConfigFlags.IsFlagSet(CF_Continuous))
	{
		//Check if we have made contact recently.
		u32 uTimeSinceMadeContact = CTimeHelpers::GetTimeSince(m_uLastTimeMadeContact);
		static dev_u32 s_uMaxTimeSinceMadeContact = 3000;
		if(uTimeSinceMadeContact < s_uMaxTimeSinceMadeContact)
		{
			//Calculate the target vehicle speed in our direction.
			Vec3V vTargetVelocity = VECTOR3_TO_VEC3V(pTargetVehicle->GetVelocity());
			Vec3V vVelocity = VECTOR3_TO_VEC3V(pVehicle->GetVelocity());
			Vec3V vForward = pVehicle->GetTransform().GetForward();
			Vec3V vDirection = NormalizeFastSafe(vVelocity, vForward);
			ScalarV scTargetSpeedInDirection = Dot(vDirection, vTargetVelocity);

			//Calculate the max cruise speed.
			ScalarV scMaxCruiseSpeed = scTargetSpeedInDirection;
			ScalarV scTargetSpeedSq = MagSquared(vTargetVelocity);
			static dev_float s_fMaxTargetSpeedForSpeedUp = 5.0f;
			ScalarV scMaxTargetSpeedForSpeedUpSq = ScalarVFromF32(square(s_fMaxTargetSpeedForSpeedUp));
			if(IsLessThanAll(scTargetSpeedSq, scMaxTargetSpeedForSpeedUpSq))
			{
				static dev_u32 s_uMinTimeSinceMadeContactForSpeedUp = 1000;
				if(uTimeSinceMadeContact > s_uMinTimeSinceMadeContactForSpeedUp)
				{
					scMaxCruiseSpeed = Add(scMaxCruiseSpeed, ScalarV(V_FLT_SMALL_1));
				}
			}
			else
			{
				static dev_float s_fMinTargetSpeedForSlowDown = 15.0f;
				ScalarV scMinTargetSpeedForSlowDownSq = ScalarVFromF32(square(s_fMinTargetSpeedForSlowDown));
				if(IsGreaterThanAll(scTargetSpeedSq, scMinTargetSpeedForSlowDownSq))
				{
					scMaxCruiseSpeed = Subtract(scMaxCruiseSpeed, ScalarV(V_FLT_SMALL_1));
				}
			}

			//Cap the speed.  Slow down a bit so that we don't ram through (this is currently only used for pinch situations).
			fCruiseSpeed = Clamp(fCruiseSpeed, 0.0f, scMaxCruiseSpeed.Getf());
		}
	}
	
	//Grab the task.
	CTaskVehicleMissionBase* pTask = static_cast<CTaskVehicleMissionBase *>(pSubTask);

	//Set the cruise speed.
	fCruiseSpeed = rage::Max(fCruiseSpeed, 0.0f);
	pTask->SetCruiseSpeed(fCruiseSpeed);
}
