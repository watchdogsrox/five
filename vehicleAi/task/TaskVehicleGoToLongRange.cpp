#include "TaskVehicleGoToLongRange.h"
#include "vehicleAi/pathzones.h"
#include "vehicleAi/VehicleIntelligence.h"
#include "vehicleAi/task/TaskVehiclePark.h"

CTaskVehicleGotoLongRange::CTaskVehicleGotoLongRange(const sVehicleMissionParams& params)
: CTaskVehicleMissionBase(params)
, m_bNavigatingToFinalDestination(false)
, m_bExtentsCalculated(false)
, m_uLastFrameRequested(0)
, m_iZoneWeAreIn(-1)
{
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_GOTO_LONGRANGE);

	m_vNodesMin.Zero();
	m_vNodesMax.Zero();
}

CTaskVehicleGotoLongRange::~CTaskVehicleGotoLongRange()
{

}

void CTaskVehicleGotoLongRange::CleanUp()
{
	//ThePaths.ReleaseRequestedNodes(NODE_REQUEST_LONG_RANGE_TASK);
}

void CTaskVehicleGotoLongRange::DoSoftReset()
{
	SetState(State_FindTarget);
	m_bSoftResetRequested = false;
	m_bExtentsCalculated = false;
}

aiTask::FSM_Return CTaskVehicleGotoLongRange::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	if (iEvent == OnUpdate && m_bSoftResetRequested)
	{
		DoSoftReset();
		return FSM_Continue;
	}

	FSM_Begin

		FSM_State(State_FindTarget)
			FSM_OnEnter
				FindTarget_OnEnter();
			FSM_OnUpdate
				return FindTarget_OnUpdate();

		FSM_State(State_Goto)
			FSM_OnEnter
				Goto_OnEnter();
			FSM_OnUpdate
				return Goto_OnUpdate();

		FSM_State(State_Stop)
			FSM_OnEnter
				Stop_OnEnter();
			FSM_OnUpdate
				return Stop_OnUpdate();

	FSM_End
}

#if !__FINAL

const char * CTaskVehicleGotoLongRange::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_FindTarget&&iState<=State_Stop);
	static const char* aStateNames[] = 
	{
		"State_FindTarget",
		"State_GoTo",
		"State_Stop"
	};

	return aStateNames[iState];
}
#endif

void CTaskVehicleGotoLongRange::ProcessTimeslicedUpdate() const
{
	if (GetState() == State_FindTarget
		|| GetState() == State_Goto)
	{
		//this needs to be called every frame
		LoadRequiredNodes(m_vNodesMin, m_vNodesMax);
	}
}

bool CTaskVehicleGotoLongRange::GetExtentsForRequiredNodesToLoad(const CVehicle& vehicle, const Vector3& vDest, Vector2& vMinOut, Vector2& vMaxOut)
{
	bool bNavigatingToFinalDestination = false;

	const Vector3 vVehiclePos = VEC3V_TO_VECTOR3(vehicle.GetVehiclePosition());

	const s32 iSrcZone = CPathZoneManager::GetZoneForPoint(Vector2(vVehiclePos.x, vVehiclePos.y));
	const s32 iDestZone = CPathZoneManager::GetZoneForPoint(Vector2(vDest.x, vDest.y));

	if (iSrcZone < 0 || iDestZone < 0)
	{
		//something went wrong, returning true here allows
		//the lower level behavior to work without doing any extra loading
		vMinOut.Zero();
		vMaxOut.Zero();
		return true;
	}

	const CPathZone& rSrcZone = CPathZoneManager::GetPathZone(iSrcZone);

	if (iSrcZone == iDestZone)
	{
		bNavigatingToFinalDestination = true;

		vMinOut = rSrcZone.vMin;
		vMaxOut = rSrcZone.vMax;

		m_CurrentIntermediateDestination = vDest;
	}
	else
	{
		//different zones, we need to construct an AABB
		//that takes into account the extents of our current zone

		vMinOut = rSrcZone.vMin;
		vMaxOut = rSrcZone.vMax;
	
		//const CPathZone& rDestZone = CPathZoneManager::GetPathZone(iDestZone);
		
		//find the mapping between these two
		const CPathZoneMapping& rMapping = CPathZoneManager::GetPathZoneMapping(iSrcZone, iDestZone);

		if (!rMapping.LoadEntireExtents)
		{
			m_CurrentIntermediateDestination = rMapping.vDestination;

			static dev_float s_fPadding = 10.0f;
			vMinOut.x = rage::Min(vMinOut.x, m_CurrentIntermediateDestination.x-s_fPadding);
			vMinOut.y = rage::Min(vMinOut.y, m_CurrentIntermediateDestination.y-s_fPadding);
			vMaxOut.x = rage::Max(vMaxOut.x, m_CurrentIntermediateDestination.x+s_fPadding);
			vMaxOut.y = rage::Max(vMaxOut.y, m_CurrentIntermediateDestination.y+s_fPadding);
		}
		else
		{
			m_CurrentIntermediateDestination = vDest;

			const CPathZone& rDestZone = CPathZoneManager::GetPathZone(iDestZone);

			vMinOut.x = rage::Min(rSrcZone.vMin.x, rDestZone.vMin.x);
			vMinOut.y = rage::Min(rSrcZone.vMin.y, rDestZone.vMin.y);
			vMaxOut.x = rage::Max(rSrcZone.vMax.x, rDestZone.vMax.x);
			vMaxOut.y = rage::Max(rSrcZone.vMax.y, rDestZone.vMax.y);	

			bNavigatingToFinalDestination = true;
		}
	}

	m_iZoneWeAreIn = iSrcZone;

	m_bExtentsCalculated = true;

	return bNavigatingToFinalDestination;
}

void CTaskVehicleGotoLongRange::LoadRequiredNodes(const Vector2& vMin, const Vector2& vMax) const
{
	if (fwTimer::GetFrameCount() != m_uLastFrameRequested)
	{
		ThePaths.AddNodesRequiredRegionThisFrame(CPathFind::CPathNodeRequiredArea::CONTEXT_GAME, Vector3(vMin.x, vMin.y, 0.0f), Vector3(vMax.x, vMax.y, 0.0f), "Longrange task");
		m_uLastFrameRequested = fwTimer::GetFrameCount();
	}

	//ThePaths.MakeRequestForNodesToBeLoaded(vMin.x, vMax.x, vMin.y, vMax.y, NODE_REQUEST_LONG_RANGE_TASK);
}

bool CTaskVehicleGotoLongRange::AreRequiredLoadesNoded() const
{
	//return ThePaths.HaveRequestedNodesBeenLoaded(NODE_REQUEST_LONG_RANGE_TASK);
	return ThePaths.AreNodesLoadedForArea(m_vNodesMin.x, m_vNodesMax.x, m_vNodesMin.y, m_vNodesMax.y);
}

void CTaskVehicleGotoLongRange::FindTarget_OnEnter()
{
	CVehicle* pVehicle = GetVehicle();
	Vector3 vTargetCoords;
	FindTargetCoors(pVehicle, vTargetCoords);

	//TODO: map start coords and end coords to zones on the map
	//TODO: set m_bNavigatingToFinalDestination before state transitioning
	//Vector2 vNodesMin, vNodesMax;

	const Vector2 vPrevMin = m_vNodesMin;
	const Vector2 vPrevMax = m_vNodesMax;

	m_bNavigatingToFinalDestination = GetExtentsForRequiredNodesToLoad(*pVehicle, vTargetCoords, m_vNodesMin, m_vNodesMax);

	if (vPrevMin.Dist(m_vNodesMin) > 1.0f || vPrevMax.Dist(m_vNodesMax) > 1.0f)
	{
		m_uLastFrameRequested = 0;
	}

	//this needs to be called every frame
	LoadRequiredNodes(m_vNodesMin, m_vNodesMax);
}

aiTask::FSM_Return CTaskVehicleGotoLongRange::FindTarget_OnUpdate()
{
	const CVehicle* pVehicle = GetVehicle();
	const Vector3 vVehiclePos = VEC3V_TO_VECTOR3(pVehicle->GetVehiclePosition());
	m_iZoneWeAreIn = CPathZoneManager::GetZoneForPoint(Vector2(vVehiclePos.x, vVehiclePos.y));

	//this needs to be called every frame
	LoadRequiredNodes(m_vNodesMin, m_vNodesMax);

	pVehicle->GetIntelligence()->UpdateCarHasReasonToBeStopped();

	if (AreRequiredLoadesNoded())
	{
		SetState(State_Goto);
		return FSM_Continue;
	}

	return FSM_Continue;
}

const float CTaskVehicleGotoLongRange::ms_fMinArrivalDistToIntermediateDestination = 50.0f;

void CTaskVehicleGotoLongRange::Goto_OnEnter()
{
	sVehicleMissionParams params = m_Params;

	if (!m_bNavigatingToFinalDestination)
	{
		params.SetTargetPosition(m_CurrentIntermediateDestination);

		//make this really large so we don't need to path all the way there
		//no longer allow this task to complete--let Goto_OnUpdate measure the arrival dist
		//params.m_fTargetArriveDist = rage::Max(ms_fMinArrivalDistToIntermediateDestination, params.m_fTargetArriveDist);
		params.m_fTargetArriveDist = 0.0f;

		//in case the user originally set the destination as an entity
		//we want it to persist down to the lowest-level subtasks
		//but always move to the vector position
		params.m_iDrivingFlags.SetFlag(DF_TargetPositionOverridesEntity);
	}

	//this needs to be called every frame
	LoadRequiredNodes(m_vNodesMin, m_vNodesMax);

	CTask* pNewTask = CVehicleIntelligence::CreateAutomobileGotoTask(params, sVehicleMissionParams::DEFAULT_SWITCH_TO_STRAIGHT_LINE_DIST);
	SetNewTask(pNewTask);
}

aiTask::FSM_Return CTaskVehicleGotoLongRange::Goto_OnUpdate()
{
	CVehicle* pVehicle = GetVehicle();

	//Find the target position.
	Vector3 vTargetPos;
	FindTargetCoors(pVehicle, vTargetPos);

	//this needs to be called every frame
	LoadRequiredNodes(m_vNodesMin, m_vNodesMax);

	//somehow our nodes have been unloaded, 
	if (!AreRequiredLoadesNoded())
	{
		CTask* pSubTask = GetSubTask();
		if(pSubTask)
		{
			//our (goto) subtask will just assert and fail in this case, so just abort it. It'll be recreated when we've got valid nodes again
			pSubTask->MakeAbortable(CTask::ABORT_PRIORITY_IMMEDIATE, NULL);
		}
		SetState(State_FindTarget);
		return FSM_Continue;
	}

	//if we've reached our final destination, halt
	const Vector3 vecVehiclePosition = VEC3V_TO_VECTOR3(pVehicle->GetVehiclePosition());
	const Vector3 vVehicleSteeringPos = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleBonnetPosition(pVehicle, IsDrivingFlagSet(DF_DriveInReverse), true));
	if(vTargetPos.z < vVehicleSteeringPos.z+CTaskVehicleMissionBase::ACHIEVE_Z_DISTANCE && vTargetPos.z > vVehicleSteeringPos.z-CTaskVehicleMissionBase::ACHIEVE_Z_DISTANCE)//make sure it's vaguely close in the Z
	{	
		float distance = rage::Min((vTargetPos - vecVehiclePosition).XYMag(), (vTargetPos - vVehicleSteeringPos).XYMag());

		//don't finish the goto task if we have continue mission on
		if(!IsDrivingFlagSet(DF_DontTerminateTaskWhenAchieved))
		{
			//check whether we have achieved our destination 
			if (distance <= m_Params.m_fTargetArriveDist)
			{
				m_bMissionAchieved = true;
				SetState(State_Stop);
				return FSM_Continue;
			}
		}
	}

	//always re-plan when we change zones
// 	const s32 iLastZoneWeWereIn = m_iZoneWeAreIn;
// 	m_iZoneWeAreIn = CPathZoneManager::GetZoneForPoint(Vector2(vecVehiclePosition.x, vecVehiclePosition.y));
// 	if (iLastZoneWeWereIn != m_iZoneWeAreIn)
// 	{
// 		SetState(State_FindTarget);
// 		return FSM_Continue;
// 	}

	//pass the speed down to our subtask in case the user modifies it
	if (GetSubTask() && GetSubTask()->GetTaskType() == CVehicleIntelligence::GetAutomobileGotoTaskType())
	{
		CTaskVehicleMissionBase* pSubtask = static_cast<CTaskVehicleMissionBase*>(GetSubTask());
		if (pSubtask)
		{
			pSubtask->SetCruiseSpeed(m_Params.m_fCruiseSpeed);
		}
	}

	if (!GetSubTask() || GetIsSubtaskFinished(CVehicleIntelligence::GetAutomobileGotoTaskType()))
	{
		//Restart task - should we maybe just stop here?
		SetState(State_FindTarget);
		return FSM_Continue;
	}

	//if we reached our intermediate destination, but it's not our final destination,
	//re-plan
	const float fDistSqrToCurrentIntermediateDest = vVehicleSteeringPos.Dist2(m_CurrentIntermediateDestination);
	if (!m_bNavigatingToFinalDestination && fDistSqrToCurrentIntermediateDest < (ms_fMinArrivalDistToIntermediateDestination * ms_fMinArrivalDistToIntermediateDestination))
	{
		SetState(State_FindTarget);
		return FSM_Continue;
	}
	return FSM_Continue;
}

/////////////////////////////////////////////////////////////////////////////////////
//State Stop
void	CTaskVehicleGotoLongRange::Stop_OnEnter()
{
	SetNewTask(rage_new CTaskVehicleStop());
}

/////////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehicleGotoLongRange::Stop_OnUpdate()
{
	if (GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_STOP))
	{
		return FSM_Quit;
	}
	return FSM_Continue;
}

//Clones should also load the required nodes so there's no delay if the vehicle migrates
void CTaskVehicleGotoLongRange::CloneUpdate(CVehicle *pVehicle)
{
	if(pVehicle->PopTypeIsMission())
	{
		if(!m_bExtentsCalculated)
		{
			Vector3 vTargetCoords;
			FindTargetCoors(pVehicle, vTargetCoords);
			m_bNavigatingToFinalDestination = GetExtentsForRequiredNodesToLoad(*pVehicle, vTargetCoords, m_vNodesMin, m_vNodesMax);
		}
		else
		{
			if(!m_bNavigatingToFinalDestination)
			{
				const Vector3 vVehicleSteeringPos = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleBonnetPosition(pVehicle, IsDrivingFlagSet(DF_DriveInReverse), true));

				const float fDistSqrToCurrentIntermediateDest = vVehicleSteeringPos.Dist2(m_CurrentIntermediateDestination);
				if(fDistSqrToCurrentIntermediateDest < (ms_fMinArrivalDistToIntermediateDestination * ms_fMinArrivalDistToIntermediateDestination))
				{
					Vector3 vTargetCoords;
					FindTargetCoors(pVehicle, vTargetCoords);
					m_bNavigatingToFinalDestination = GetExtentsForRequiredNodesToLoad(*pVehicle, vTargetCoords, m_vNodesMin, m_vNodesMax);
				}
			}
		}

		//this needs to be called every frame
		LoadRequiredNodes(m_vNodesMin, m_vNodesMax);
	}
}