#include "TaskVehicleFollowRecording.h"

// Game headers
#include "Control/Record.h"
#include "VehicleAi/task/TaskVehicleGoTo.h"
#include "VehicleAi/task/TaskVehicleGoToAutomobile.h"
#include "VehicleAi/task/TaskVehicleGoToHelicopter.h"
#include "VehicleAi/task/TaskVehicleGoToPlane.h"
#include "VehicleAi/task/TaskVehiclePark.h"
#include "VehicleAi/task/TaskVehicleCruise.h"
#include "VehicleAi/VehicleIntelligence.h"
#include "Vehicles/Vehicle.h"
#include "task/Movement/TaskFollowWaypointRecording.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

////////////////////////////////////////////////////////////

CTaskVehicleFollowRecording::CTaskVehicleFollowRecording(const sVehicleMissionParams& params) :
CTaskVehicleMissionBase(params)
{
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_FOLLOW_RECORDING);
}


////////////////////////////////////////////////////////////

CTaskVehicleFollowRecording::~CTaskVehicleFollowRecording()
{

}



////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskVehicleFollowRecording::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.

	FSM_Begin
		// FollowRecording state
		FSM_State(State_FollowRecording)
			FSM_OnEnter
				FollowRecording_OnEnter(pVehicle);
			FSM_OnUpdate
				return FollowRecording_OnUpdate(pVehicle);

		// Stop
		FSM_State(State_Stop)
			FSM_OnEnter
				Stop_OnEnter(pVehicle);
			FSM_OnUpdate
				return Stop_OnUpdate(pVehicle);
	FSM_End
}

void CTaskVehicleFollowRecording::CloneUpdate(CVehicle* pVehicle)
{
    CTaskVehicleMissionBase::CloneUpdate(pVehicle);

    pVehicle->m_nVehicleFlags.bTasksAllowDummying = true;
}

////////////////////////////////////////////////////////////

void CTaskVehicleFollowRecording::HelperSetupParamsForSubtask(sVehicleMissionParams& params)
{
	params.m_fTargetArriveDist = 0.0f;
	params.m_iDrivingFlags.SetFlag(DF_ForceStraightLine);
	params.m_iDrivingFlags.SetFlag(DF_ExtendRouteWithWanderResults);
}

////////////////////////////////////////////////////////////

void	CTaskVehicleFollowRecording::FollowRecording_OnEnter				(CVehicle* UNUSED_PARAM(pVehicle))
{
	sVehicleMissionParams params = m_Params;
	HelperSetupParamsForSubtask(params);

	//disable stopping at lights
	params.m_iDrivingFlags.ClearFlag(DF_StopAtLights);
	
	//SetNewTask(rage_new CTaskVehicleGoToPointWithAvoidanceAutomobile(params));
	SetNewTask(rage_new CTaskVehicleGoToAutomobileNew(params));

}

////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskVehicleFollowRecording::FollowRecording_OnUpdate(CVehicle* pVehicle)
{
	if(pVehicle->GetIntelligence()->GetRecordingNumber() >= 0 && !CVehicleRecordingMgr::GetUseCarAI(pVehicle->GetIntelligence()->GetRecordingNumber()))
	{
		//The vehicle is now playing back the recording without AI, so we can clean-up this task.
		pVehicle->GetIntelligence()->m_BackupFollowRoute.Invalidate();
		return FSM_Quit;
	}

	ControlAIVeh_FollowPreRecordedPath(pVehicle);

	return FSM_Continue;
}


////////////////////////////////////////////////////////////

void	CTaskVehicleFollowRecording::Stop_OnEnter				(CVehicle* UNUSED_PARAM(pVehicle))
{
	SetNewTask( rage_new CTaskVehicleStop());
}


////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskVehicleFollowRecording::Stop_OnUpdate				(CVehicle* pVehicle)
{
	if ((pVehicle && pVehicle->InheritsFromBoat())
		|| GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_STOP))
	{
		return FSM_Quit;
	}
	return FSM_Continue;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : ControlAIVeh_FollowPreRecordedPath
// PURPOSE :  This function controls a car that is following a pre-recorded path.
//			  The car will try to stay close to the path whilst maintaining a
//			  similar speed and also avoid other cars.
/////////////////////////////////////////////////////////////////////////////////
//#define	DEBUGFOLLOWPREREC
void CTaskVehicleFollowRecording::ControlAIVeh_FollowPreRecordedPath(CVehicle* pVeh)
{
	Assertf(pVeh, "ControlAIVeh_FollowPreRecordedPath expected a valid set vehicle.");

	s32	Recording = pVeh->GetIntelligence()->GetRecordingNumber();
	if(Recording < 0)
	{		// Someone has stopped the recording(perhaps mission cleanup). Go and do something else.
		SetState(State_Stop);//The ped ai should take over
		return;
	}

	// Make sure the path is still loaded in. If not change our mission and jump out.
	if(CVehicleRecordingMgr::GetPlaybackBuffer(Recording) == NULL)
	{
		SetState(State_Stop);//The ped ai should take over
		return;
	}

	// If the recording is paused we simply stop.
	if(CVehicleRecordingMgr::IsPlaybackPaused(Recording))
	{
		SetState(State_Stop);
		return;
	}

	// Get the current drive direction and orientation.
	//const Vector3 vehDriveDir = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleDriveDir(pVeh, IsDrivingFlagSet(DF_DriveInReverse)));
	//float vehDriveOrientation = fwAngle::GetATanOfXY(vehDriveDir.x, vehDriveDir.y);

	const Vector3 vVehiclePosition = VEC3V_TO_VECTOR3(pVeh->GetVehiclePosition());
	// We identify the point on the path that is 10m ahead of us.
	CVehicleStateEachFrame *pVehState =(CVehicleStateEachFrame *) &((CVehicleRecordingMgr::GetPlaybackBuffer(Recording))[CVehicleRecordingMgr::GetPlaybackIndex(Recording)]);
	float Dist =(vVehiclePosition - Vector3(pVehState->CoorsX, pVehState->CoorsY, pVehState->CoorsZ)).Mag();
	float NextDist =(vVehiclePosition - Vector3((pVehState+1)->CoorsX, (pVehState+1)->CoorsY, (pVehState+1)->CoorsZ)).Mag();

	//we need to add the length of our front bonnet to the lookahead distance, as the base point we look ahead from
	//in this task is the vehicle origin, whereas the code expects the base point to be the center of the front bonnet
	const float fLookaheadDistFromBonnet = CTaskVehicleFollowWaypointRecording::GetLookaheadDistanceForFollowRecording(pVeh, NULL, GetCruiseSpeed());
	const float fFrontBonnetSize = pVeh->GetBoundingBoxMax().y;
	const float fLookaheadDist = rage::Min(10.0f, fLookaheadDistFromBonnet + fFrontBonnetSize);
	while(Dist < fLookaheadDist || NextDist < Dist)
	{
		CVehicleRecordingMgr::SetPlaybackIndex(Recording, CVehicleRecordingMgr::GetPlaybackIndex(Recording) + sizeof(CVehicleStateEachFrame));
		if(CVehicleRecordingMgr::GetPlaybackIndex(Recording) >=((s32)CVehicleRecordingMgr::GetPlaybackBufferSize(Recording)) -((s32)sizeof(CVehicleStateEachFrame)))
		{	// We're at the end of the buffer. Call it a day. For buses we loop though.
			if(Recording < FOR_CODE_INDEX_LAST)
			{		// Loop round on this recording.
				CVehicleRecordingMgr::SetPlaybackIndex(Recording, 0);
			}
			else
			{		// Stop playing this recording.
				CVehicleRecordingMgr::StopPlaybackWithIndex(Recording);
				pVeh->GetIntelligence()->SetRecordingNumber(-1);

				SetState(State_Stop);//The ped ai should take over
				return;
			}
		}
		pVehState =(CVehicleStateEachFrame *) &((CVehicleRecordingMgr::GetPlaybackBuffer(Recording))[CVehicleRecordingMgr::GetPlaybackIndex(Recording)]);
		Dist =(vVehiclePosition - Vector3(pVehState->CoorsX, pVehState->CoorsY, pVehState->CoorsZ)).Mag();
		NextDist =(vVehiclePosition - Vector3((pVehState+1)->CoorsX, (pVehState+1)->CoorsY, (pVehState+1)->CoorsZ)).Mag();
	}


	// Try to match the speed to the speed of the recording.
	float RecordingSpeed = CVehicleRecordingMgr::GetPlaybackSpeed(Recording) 
		* rage::Sqrtf(pVehState->GetSpeedX() * pVehState->GetSpeedX() 
			+ pVehState->GetSpeedY() * pVehState->GetSpeedY()
			+ pVehState->GetSpeedZ() * pVehState->GetSpeedZ());

	// Right at the beginning of a recording we use a certain minimum speed just to get started.(first 5 seconds or so)
	if(CVehicleRecordingMgr::GetPlaybackIndex(Recording) <=(s32)(25 * sizeof(CVehicleStateEachFrame)))
	{
		RecordingSpeed = rage::Max(RecordingSpeed, 5.0f);
	}

	// Buses follow a recording whilst staying to the schedule. If the bus is behind it will
	// try to speed up. If the bus is ahead of schedule it will slow down.
	if(Recording < FOR_CODE_INDEX_LAST)
	{
		// The point in the recording that the vehicle is at:
		u32 CurrentTime =((CVehicleStateEachFrame *) &((CVehicleRecordingMgr::GetPlaybackBuffer(Recording))[CVehicleRecordingMgr::GetPlaybackIndex(Recording)]))->TimeInRecording;

		// The point in the recording that the schedule is at:
		u32 ScheduleTime = CVehicleRecordingMgr::FindPlaybackRunningTimeAccordingToSchedule(Recording, 0);

		s32 BehindSchedule = ScheduleTime - CurrentTime;

		if(BehindSchedule < 60000 && BehindSchedule > -60000)		// More than a minute behind? Don't bother catching up.
		{
			float SpeedIncrease;

			if(BehindSchedule > 0)
			{
				SpeedIncrease = rage::Min(15.0f, rage::Max(0, (BehindSchedule - 2000)) * 0.001f);
			}
			else
			{
				SpeedIncrease = rage::Max(-30.0f, rage::Min(0, (BehindSchedule + 2000)) * 0.005f);
			}
			RecordingSpeed = rage::Max(0.0f, RecordingSpeed + SpeedIncrease);
		}
	}
	SetCruiseSpeed(rage::Max(RecordingSpeed, 1.0f));

	// We have found the point on the prerecorded path that we aim towards. Just aim for it now.
	Vector3 targetCoors = pVehState->GetCoors();
	SetTargetPosition(&targetCoors);

	// set the target for the gotopointwithavoidance task
	if(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_VEHICLE_GOTO_AUTOMOBILE_NEW)
	{
		CTaskVehicleGoToAutomobileNew * pGoToTask = (CTaskVehicleGoToAutomobileNew*)GetSubTask();

		//in Fanatic2, the driving flags can change via script while this subtask is active.
		//so let them propogate down here as well
		sVehicleMissionParams params = m_Params;
		HelperSetupParamsForSubtask(params);
		pGoToTask->SetDrivingFlags(params.m_iDrivingFlags);
		pGoToTask->SetTargetPosition(&m_Params.GetTargetPosition());
		pGoToTask->SetCruiseSpeed(GetCruiseSpeed());
	}

#if __DEV
	if(CVehicleIntelligence::m_bDisplayCarAiDebugInfo && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVeh))
	{

		grcDebugDraw::Line(VEC3V_TO_VECTOR3(pVeh->GetTransform().GetPosition()) + Vector3(0.0f,0.0f,2.0f),
			Vector3(pVehState->CoorsX, pVehState->CoorsY, pVehState->CoorsZ + 3.0f),
			Color32(0, 128, 255, 255));

		grcDebugDraw::Line(Vector3(pVehState->CoorsX, pVehState->CoorsY, pVehState->CoorsZ),
			Vector3(pVehState->CoorsX, pVehState->CoorsY, pVehState->CoorsZ + 3.0f),
			Color32(0, 255, 0, 255));

		s32 PlayBackIndex = CVehicleRecordingMgr::GetPlaybackIndex(Recording);
		for(s32 C = 0; C < 20; C++)
		{
			PlayBackIndex -= sizeof(CVehicleStateEachFrame);
			if(PlayBackIndex > 0)
			{
				CVehicleStateEachFrame *pVehSt =(CVehicleStateEachFrame *) &((CVehicleRecordingMgr::GetPlaybackBuffer(Recording))[PlayBackIndex]);
				grcDebugDraw::Line(Vector3(pVehSt->CoorsX, pVehSt->CoorsY, pVehSt->CoorsZ),
					Vector3(pVehSt->CoorsX, pVehSt->CoorsY, pVehSt->CoorsZ + 0.5f),
					Color32(255, 255, 0, 255));
			}
		}
		PlayBackIndex = CVehicleRecordingMgr::GetPlaybackIndex(Recording);
		for(s32 C = 0; C < 20; C++)
		{
			PlayBackIndex += sizeof(CVehicleStateEachFrame);
			//		if(PlayBackIndex < 0)
			{
				CVehicleStateEachFrame *pVehSt =(CVehicleStateEachFrame *) &((CVehicleRecordingMgr::GetPlaybackBuffer(Recording))[PlayBackIndex]);
				grcDebugDraw::Line(Vector3(pVehSt->CoorsX, pVehSt->CoorsY, pVehSt->CoorsZ),
					Vector3(pVehSt->CoorsX, pVehSt->CoorsY, pVehSt->CoorsZ + 0.5f),
					Color32(0, 255, 255, 255));
			}
		}
	}
#endif
}




////////////////////////////////////////////////////////////

#if !__FINAL
const char * CTaskVehicleFollowRecording::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_FollowRecording&&iState<=State_Stop);
	static const char* aStateNames[] = 
	{
		"State_FollowRecording",
		"State_Stop"
	};

	return aStateNames[iState];
}
#endif

CTaskVehicleFollowWaypointRecording::CTaskVehicleFollowWaypointRecording(::CWaypointRecordingRoute * pRoute, const sVehicleMissionParams& params, const int iInitialProgress, const u32 iFlags, const int iTargetProgress)
: CTaskVehicleMissionBase(params)
, m_fPathLength(0.0f)
, m_bWantToQuitImmediately(false)
{
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_FOLLOW_WAYPOINT_RECORDING);

	if(!pRoute)
	{
		m_bWantToQuitImmediately = true;
		return;
	}

	m_bWantToQuitImmediately = !m_waypoints.Init(pRoute, iInitialProgress, iFlags, iTargetProgress);
}

CTaskVehicleFollowWaypointRecording::~CTaskVehicleFollowWaypointRecording()
{
}

aiTask::FSM_Return CTaskVehicleFollowWaypointRecording::ProcessPreFSM()
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.
	if (pVehicle)
	{
		pVehicle->m_nVehicleFlags.bTasksAllowDummying = true;
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskVehicleFollowWaypointRecording::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.

	if(m_bWantToQuitImmediately)
	{
		return FSM_Quit;
	}

	// This should only happen during route editing, if the recording route has been
	// cleared via Rag whilst this task is running.
	if(m_waypoints.GetProgress() > m_waypoints.GetRoute()->GetSize())
	{
		Assertf(false, "The waypoint route was cleared whilst CTaskFollowWaypointRecording was running.  Its ok - the task will just quit.");
		return FSM_Quit;
	}

	FSM_Begin
		// FollowRecording state
		FSM_State(State_FollowRecording)
			FSM_OnEnter
				FollowRecording_OnEnter(pVehicle);
			FSM_OnUpdate
				return FollowRecording_OnUpdate(pVehicle);

		// Paused
		FSM_State(State_Paused)
			FSM_OnEnter
				Paused_OnEnter(pVehicle);
			FSM_OnUpdate
				return Paused_OnUpdate(pVehicle);

		// Paused
		FSM_State(State_Finished)
			FSM_OnEnter
				Finished_OnEnter(pVehicle);
			FSM_OnUpdate
				return Finished_OnUpdate(pVehicle);
	FSM_End
}

bool CTaskVehicleFollowWaypointRecording::ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent)
{
	if(m_bWantToQuitImmediately)
	{
		return true;
	}

// 	bool bNonInterruptible = false;//m_bOverrideNonInterruptible;
// 	if(m_waypoints.GetRoute() && m_waypoints.GetProgress() < m_waypoints.GetRoute()->GetSize())
// 	{
// 		const ::CWaypointRecordingRoute::RouteData & wpt = m_waypoints.GetRoute()->GetWaypoint(m_waypoints.GetProgress());
// 		if(wpt.GetFlags() & ::CWaypointRecordingRoute::WptFlag_NonInterrupt)
// 		{
// 			bNonInterruptible = true;
// 		}
// 	}

	return CTask::ShouldAbort(iPriority, pEvent);
}

////////////////////////////////////////////////////////////
float CTaskVehicleFollowWaypointRecording::GetLookaheadDistanceForFollowRecording(const CVehicle* pVehicle, const CTaskVehicleMissionBase* pSubtask, const float fOurCruiseSpeed)
{
	TUNE_GROUP_FLOAT(VEHICLE_WAYPOINT_FOLLOW, sfCruiseSpeedFraction,				0.1f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(VEHICLE_WAYPOINT_FOLLOW, sfFilterLaneLookaheadMultiplier,		2.7f, 0.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT(VEHICLE_WAYPOINT_FOLLOW, sfMaxLookaheadSpeed,					24.0f, 0.0f, 100.0f, 0.01f);
	TUNE_GROUP_FLOAT(VEHICLE_WAYPOINT_FOLLOW, sfDefaultLookaheadCar,				1.5f, 0.0f, 100.0f, 0.01f);
	TUNE_GROUP_FLOAT(VEHICLE_WAYPOINT_FOLLOW, sfDefaultLookaheadCarRacing,			4.0f, 0.0f, 100.0f, 0.01f);
	TUNE_GROUP_FLOAT(VEHICLE_WAYPOINT_FOLLOW, sfDefaultLookaheadBike,				1.0f, 0.0f, 100.0f, 0.01f);
	TUNE_GROUP_FLOAT(VEHICLE_WAYPOINT_FOLLOW, sfDefaultLookaheadBikeRacing,			1.0f, 0.0f, 100.0f, 0.01f);
	TUNE_GROUP_FLOAT(VEHICLE_WAYPOINT_FOLLOW, sfDefaultLookaheadBoat,				4.0f, 0.0f, 100.0f, 0.01f);
	TUNE_GROUP_FLOAT(VEHICLE_WAYPOINT_FOLLOW, sfDefaultLookaheadHeli,				50.0f, 0.0f, 100.0f, 0.01f);
	TUNE_GROUP_FLOAT(VEHICLE_WAYPOINT_FOLLOW, sfDefaultLookaheadPlane,				50.0f, 0.0f, 100.0f, 0.01f);
	TUNE_GROUP_FLOAT(VEHICLE_WAYPOINT_FOLLOW, sfWheelbaseMultiplier,				0.5f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(VEHICLE_WAYPOINT_FOLLOW, sfSpeedMultiplier,					1.0f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(VEHICLE_WAYPOINT_FOLLOW, sfSpeedMultiplierBoat,				1.0f, 0.0f, 100.0f, 0.01f);
	TUNE_GROUP_FLOAT(VEHICLE_WAYPOINT_FOLLOW, sfSpeedThreshold,						6.0f, 0.0f, 100.0f, 0.01f);
	TUNE_GROUP_FLOAT(VEHICLE_WAYPOINT_FOLLOW, sfRacingSpeedCar,						20.0f, 0.0f, 100.0f, 0.01f);
	TUNE_GROUP_FLOAT(VEHICLE_WAYPOINT_FOLLOW, sfRacingSpeedBike,					20.0f, 0.0f, 100.0f, 0.01f);
	TUNE_GROUP_FLOAT(VEHICLE_WAYPOINT_FOLLOW, sfDesiredSpeedMaxDiffBike,			8.0f, 0.0f, 100.0f, 0.01f);
	TUNE_GROUP_FLOAT(VEHICLE_WAYPOINT_FOLLOW, sfDesiredSpeedMaxDiff,				8.0f, 0.0f, 100.0f, 0.01f);

	// Determine the amount to look down the path.
	// First work out an approximate speed we should consider for lookahead
	// Use the speed we calculated last time as a base for the lookahead distance.
	//CTaskVehicleGoToPointWithAvoidanceAutomobile* pTask = static_cast<CTaskVehicleGoToPointWithAvoidanceAutomobile*>(FindSubTaskOfType(CTaskTypes::TASK_VEHICLE_GOTO_POINT_WITH_AVOIDANCE_AUTOMOBILE));
	const float fDesiredSpeed = pSubtask ? pSubtask->GetCruiseSpeed() : fOurCruiseSpeed;
	const float fCurrentSpeed = pVehicle->GetVelocity().XYMag();
	float fCappedDesiredSpeed = fDesiredSpeed;
	if( pVehicle->InheritsFromBike())
	{
		//clamp the desired speed difference for bikes to prevent them using too much of desired speed - causes cutting issues around sharp corners
		fCappedDesiredSpeed = Clamp(fDesiredSpeed, fCurrentSpeed - sfDesiredSpeedMaxDiffBike, fCurrentSpeed + sfDesiredSpeedMaxDiffBike);
	}
	float speedForLookAheadDist = rage::Min((sfCruiseSpeedFraction * fCappedDesiredSpeed) + ((1.0f - sfCruiseSpeedFraction) * fCurrentSpeed), sfMaxLookaheadSpeed);

	float fDefaultLookahead = 1.0f;
	if (pVehicle->InheritsFromBoat())
	{
		fDefaultLookahead = sfDefaultLookaheadBoat;
	}
	else if (pVehicle->InheritsFromHeli())
	{
		fDefaultLookahead = sfDefaultLookaheadHeli;
	}
	else if (pVehicle->InheritsFromPlane())
	{
		fDefaultLookahead = sfDefaultLookaheadPlane;
	}
	else if (pVehicle->InheritsFromBike())
	{
		if (fDesiredSpeed >= sfRacingSpeedBike)
		{
			fDefaultLookahead = sfDefaultLookaheadBikeRacing;
		}
		else
		{
			fDefaultLookahead = sfDefaultLookaheadBike;
		}
	}
	else
	{
		if (fDesiredSpeed >= sfRacingSpeedCar)
		{
			fDefaultLookahead = sfDefaultLookaheadCarRacing;
		}
		else
		{
			fDefaultLookahead = sfDefaultLookaheadCar;
		}
	}

	const float fSpeedMultiplier = pVehicle->InheritsFromBoat() ? sfSpeedMultiplierBoat : sfSpeedMultiplier;

	// Include that whilst considering the wheel base length to work out a lookahead distance.
	const float lookAheadDist = fDefaultLookahead * (1.0f + sfWheelbaseMultiplier * pVehicle->GetVehicleModelInfo()->GetWheelBaseMultiplier()) + 
		fSpeedMultiplier * rage::Max(0.0f, speedForLookAheadDist - sfSpeedThreshold);

	return lookAheadDist;
}

////////////////////////////////////////////////////////////

float CTaskVehicleFollowWaypointRecording::FindTargetPositionAndCapSpeed( CVehicle* pVehicle, const Vector3& vStartCoords, Vector3& vTargetPosition, float& fOutSpeed )
{
	TUNE_GROUP_FLOAT(VEHICLE_WAYPOINT_FOLLOW, sfDistanceToProgressRoute,			0.01f, 0.0f, 100.0f, 0.1f);
	
	CTaskVehicleGoToPointWithAvoidanceAutomobile* pSubTask = static_cast<CTaskVehicleGoToPointWithAvoidanceAutomobile*>(FindSubTaskOfType(CTaskTypes::TASK_VEHICLE_GOTO_POINT_WITH_AVOIDANCE_AUTOMOBILE));
	const float lookAheadDist = GetLookaheadDistanceForFollowRecording(pVehicle, pSubTask, GetCruiseSpeed());

	const int iActualProgress = Max(m_waypoints.GetProgress() + m_followRoute.GetCurrentTargetPoint() - 1, 0);
	const float fFreeSpaceToLeft = m_waypoints.GetRoute()->GetWaypoint(iActualProgress).GetFreeSpaceToLeft();
	const float fFreeSpaceToRight = m_waypoints.GetRoute()->GetWaypoint(iActualProgress).GetFreeSpaceToRight();
	const bool bAllowLaneSlack = fFreeSpaceToLeft > 0.0f || fFreeSpaceToRight > 0.0f;

	const float fDistSearched = m_followRoute.FindTargetCoorsAndSpeed(pVehicle, lookAheadDist, sfDistanceToProgressRoute, RCC_VEC3V(vStartCoords), IsDrivingFlagSet(DF_DriveInReverse), RC_VEC3V(vTargetPosition), fOutSpeed, !(m_waypoints.GetFlags() & CTaskFollowWaypointRecording::Flag_VehiclesUseAISlowdown)
		, true, false, -1, bAllowLaneSlack);

	return fDistSearched;
}

////////////////////////////////////////////////////////////

void	CTaskVehicleFollowWaypointRecording::FollowRecording_OnEnter				(CVehicle* pVehicle)
{
	// Calculate the initial target position
	//CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.
	const Vector3 vStartCoords = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleBonnetPosition(pVehicle, IsDrivingFlagSet(DF_DriveInReverse)));

	if( ((m_waypoints.GetFlags() & CTaskFollowWaypointRecording::Flag_StartFromClosestPosition) != 0) || m_waypoints.GetProgress() == -1)
	{
		m_waypoints.SetProgress(CTaskFollowWaypointRecording::CalculateProgressFromPosition(vStartCoords, VEC3_ZERO, m_waypoints.GetRoute(), 6.0f));
		// this can return -1 if no progress point found; in this case clamping will set it to zero
		m_waypoints.SetProgress(Clamp(m_waypoints.GetProgress(), 0, m_waypoints.GetRoute()->GetSize()-1));
	}

	m_followRoute.ConstructFromWaypointRecording(pVehicle, *m_waypoints.GetRoute(), m_waypoints.GetProgress(), m_waypoints.GetTargetProgress(), (m_waypoints.GetFlags() & CTaskFollowWaypointRecording::Flag_VehiclesUseAISlowdown) != 0, m_waypoints.IsSpeedOverridden(), m_waypoints.GetOverrideSpeed());

	Vector3 vTargetPosition;
	float fSpeed = GetCruiseSpeed();
	m_fPathLength = FindTargetPositionAndCapSpeed(pVehicle, vStartCoords, vTargetPosition, fSpeed);

	sVehicleMissionParams params = m_Params;
	params.SetTargetPosition(vTargetPosition);
	params.m_fTargetArriveDist = 0.0f;
	params.m_fCruiseSpeed = fSpeed;

	CTask* pNewTask = NULL;
	if (pVehicle->InheritsFromHeli())
	{
		pNewTask = rage_new CTaskVehicleGoToHelicopter(params);
	}
	else if (pVehicle->InheritsFromPlane())
	{
		pNewTask = rage_new CTaskVehicleGoToPlane(params);
	}
	else
	{
		//Assertf(pVehicle->InheritsFromAutomobile(), "Trying to follow a waypoint recording on a non-automobile vehicle. This will not work.");
		pNewTask = rage_new CTaskVehicleGoToPointWithAvoidanceAutomobile(params, true);
	}
	Assert(pNewTask);
	SetNewTask(pNewTask);
}

////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskVehicleFollowWaypointRecording::FollowRecording_OnUpdate(CVehicle* pVehicle)
{
	if (m_waypoints.WantsToPause())
	{
		SetState(State_Paused);
		return FSM_Continue;
	}

	TUNE_GROUP_FLOAT(VEHICLE_WAYPOINT_FOLLOW, sfDistanceToFindNewPathCar, 75.0f, 0.0f, 999.0f, 0.1f);
	TUNE_GROUP_FLOAT(VEHICLE_WAYPOINT_FOLLOW, sfDistanceToFindNewPathBoat, 75.0f, 0.0f, 999.0f, 0.1f);
	TUNE_GROUP_FLOAT(VEHICLE_WAYPOINT_FOLLOW, sfDistanceToFindNewPathHeli, 125.0f, 0.0f, 999.0f, 0.1f);
	TUNE_GROUP_FLOAT(VEHICLE_WAYPOINT_FOLLOW, sfDistanceToFindNewPathPlane, 125.0f, 0.0f, 999.0f, 0.1f);
	TUNE_GROUP_FLOAT(VEHICLE_WAYPOINT_FOLLOW, sfMinTimeToFindNewPath, 0.5f, 0.0f, 100.0f, 0.1f);
	//TUNE_GROUP_FLOAT(VEHICLE_WAYPOINT_FOLLOW, sfDistanceToConsiderRouteCompleted, 2.0f, 0.0f, 100.0f, 0.1f);

	const Vector3 vStartCoords = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleBonnetPosition(pVehicle, IsDrivingFlagSet(DF_DriveInReverse)));
	
	const bool bSpeedJustOverridden = m_waypoints.IsSpeedJustOverridden();
	m_waypoints.ResetJustOverriddenSpeed();

	Vector3 vTargetPosition;
	float fSpeed = GetCruiseSpeed();
	float fDistanceToTheEndOfRoute = FindTargetPositionAndCapSpeed(pVehicle, vStartCoords, vTargetPosition, fSpeed);

	float fDistanceToFindNewPath = sfDistanceToFindNewPathCar;
	if (pVehicle->InheritsFromBoat())
	{
		fDistanceToFindNewPath = sfDistanceToFindNewPathBoat;
	}
	else if (pVehicle->InheritsFromHeli())
	{
		fDistanceToFindNewPath = sfDistanceToFindNewPathHeli;
	}
	else if (pVehicle->InheritsFromPlane())
	{
		fDistanceToFindNewPath = sfDistanceToFindNewPathPlane;
	}

	//update progress on the waypoint recording here and regen our follow route if necessary
	if( (fDistanceToTheEndOfRoute < rage::Min( m_fPathLength*0.5f, fDistanceToFindNewPath ) && GetTimeInState() > sfMinTimeToFindNewPath)
		|| bSpeedJustOverridden)
	{
		//update progress on waypoint route
		int iActualProgress = Max(m_waypoints.GetProgress() + m_followRoute.GetCurrentTargetPoint() - 1, 0);

//		Assertf(!m_waypoints.GetIsLoopPlayback(), "Loop playback not supported yet");
		if (m_waypoints.GetIsLoopPlayback() && iActualProgress >= m_waypoints.GetRouteSize()-1)
		{
			iActualProgress = 0;
		}
		m_waypoints.SetProgress(iActualProgress);

		//reconstruct followroute
		m_followRoute.ConstructFromWaypointRecording(pVehicle, *m_waypoints.GetRoute(), m_waypoints.GetProgress(), m_waypoints.GetTargetProgress(), (m_waypoints.GetFlags() & CTaskFollowWaypointRecording::Flag_VehiclesUseAISlowdown) != 0, m_waypoints.IsSpeedOverridden(), m_waypoints.GetOverrideSpeed());

		//update path length
		m_fPathLength = FindTargetPositionAndCapSpeed(pVehicle, vStartCoords, vTargetPosition, fSpeed);
		fDistanceToTheEndOfRoute = m_fPathLength;
	}

	const int iTargetIndex = m_waypoints.GetTargetProgress() == -1 ? m_waypoints.GetRouteSize() - 1 : m_waypoints.GetTargetProgress();

	//need to get distance to the end line, not just the end point
	Vector3 vClosestPointOnEndLine = m_waypoints.GetRoute()->GetWaypoint(iTargetIndex).GetPosition();
	const float fFreeSpaceToLeft = m_waypoints.GetRoute()->GetWaypoint(iTargetIndex).GetFreeSpaceToLeft();
	const float fFreeSpaceToRight = m_waypoints.GetRoute()->GetWaypoint(iTargetIndex).GetFreeSpaceToRight();
	if (fDistanceToTheEndOfRoute < SMALL_FLOAT)
	{
		//if we're looping, start the route over
		if (m_waypoints.GetIsLoopPlayback())
		{
			//just start over at the first point. even if the user initially specified something > 0,
			//we'll assume if we reach the end we want to start over at the first point
			m_waypoints.SetProgress(0);

			//reconstruct followroute
			m_followRoute.ConstructFromWaypointRecording(pVehicle, *m_waypoints.GetRoute(), m_waypoints.GetProgress(), m_waypoints.GetTargetProgress(), (m_waypoints.GetFlags() & CTaskFollowWaypointRecording::Flag_VehiclesUseAISlowdown) != 0, m_waypoints.IsSpeedOverridden(), m_waypoints.GetOverrideSpeed());

			//update path length
			m_fPathLength = FindTargetPositionAndCapSpeed(pVehicle, vStartCoords, vTargetPosition, fSpeed);
			fDistanceToTheEndOfRoute = m_fPathLength;
		}
		else if (fFreeSpaceToLeft > 0.0f || fFreeSpaceToRight > 0.0f)
		{
			//if not looping, but our route has a width--find the closest point on the end line
			Assertf(iTargetIndex > 0, "Waypoint recording target index can't be 0!");
			Vector3 vDelta = vClosestPointOnEndLine - m_waypoints.GetRoute()->GetWaypoint(iTargetIndex-1).GetPosition();
			vDelta.NormalizeFast();
			Vector3 vNormal;
			vNormal.Cross(vDelta, Vector3(0.0f, 0.0f, 1.0f));
			Vector3 vEndLeft = vClosestPointOnEndLine + -vNormal*fFreeSpaceToLeft;
			Vector3 vEndRight = vClosestPointOnEndLine + vNormal*fFreeSpaceToRight;
			fwGeom::fwLine endLine(vEndLeft, vEndRight);
			endLine.FindClosestPointOnLine(vStartCoords, vClosestPointOnEndLine);

			//if we're at our last point, just drive toward the closest point on the end line
			vTargetPosition = vClosestPointOnEndLine;
		}
		//otherwise vTargetPosition will have already been set to the final destination position,
		//so we can rely on the code below to just drive us there
	}


	if ( pVehicle->InheritsFromBoat() )
	{
		if ( pVehicle->GetIntelligence()->GetBoatAvoidanceHelper() )
		{
			const int iTargetPoint = m_followRoute.FindClosestPointToPosition(VECTOR3_TO_VEC3V(vTargetPosition), m_followRoute.GetCurrentTargetPoint(), m_followRoute.GetNumPoints());
			int iTargetProgress = Clamp(m_waypoints.GetProgress() + iTargetPoint, 0, m_waypoints.GetRouteSize()-1);

			if (m_waypoints.GetIsLoopPlayback() && iTargetProgress >= m_waypoints.GetRouteSize()-1)
			{
				iTargetProgress = 0;
			}

			const float fTargetFreeSpaceToRight = m_waypoints.GetRoute()->GetWaypoint(iTargetProgress).GetFreeSpaceToRight();
			const float fTargetFreeSpaceToLeft = m_waypoints.GetRoute()->GetWaypoint(iTargetProgress).GetFreeSpaceToLeft();

			if (fTargetFreeSpaceToRight > 0.0f || fTargetFreeSpaceToLeft > 0.0f)
			{
				//this new code only works if there are 2 or more points in a route
				//and if iTargetProgress is 0, it should only be for looping routes
				Assert(m_waypoints.GetRouteSize() > 1);
				Assert(iTargetProgress > 0 || m_waypoints.GetIsLoopPlayback());

				Vec2V vBoatPos = VECTOR3_TO_VEC3V(vStartCoords).GetXY();
				Vec2V vCurrent = VECTOR3_TO_VEC3V(m_waypoints.GetRoute()->GetWaypoint(iTargetProgress).GetPosition()).GetXY();
				Vec2V vPrevious = iTargetProgress > 0 
					? VECTOR3_TO_VEC3V(m_waypoints.GetRoute()->GetWaypoint(iTargetProgress-1).GetPosition()).GetXY()
					: VECTOR3_TO_VEC3V(m_waypoints.GetRoute()->GetWaypoint(m_waypoints.GetRouteSize()-1).GetPosition()).GetXY();
				Vec2V vDirection = Normalize(vCurrent - vPrevious);
				Vec2V vRight = Vec2V(vDirection.GetY(), -vDirection.GetX());
			
				Vec2V vTargetRight = vCurrent + vRight * ScalarV(fTargetFreeSpaceToRight);
				Vec2V vTargetLeft = vCurrent - vRight * ScalarV(fTargetFreeSpaceToLeft);	
 
				//grcDebugDraw::Sphere(Vector3(vTargetRight.GetXf(), vTargetRight.GetYf(), vStartCoords.z), 1.0f, Color_blue, true, -1);
				//grcDebugDraw::Sphere(Vector3(vTargetLeft.GetXf(), vTargetLeft.GetYf(), vStartCoords.z), 1.0f, Color_blue, true, -1);

				//Vec2V vDirToTarget = Normalize(vCurrent - vBoatPos);
				Vec2V vDirToLeftTarget = Normalize(vTargetLeft - vBoatPos);
				Vec2V vDirToRightTarget = Normalize(vTargetRight - vBoatPos);

				// this should probably use vDirToTarget but I don't want to change things at this point.
				// If there are problems with racers unable to follow course then we should come back
				// and potentially change this computation
				ScalarV fAngleLeft = Angle(vDirection, vDirToLeftTarget) * ScalarV(RtoD) * ScalarV(Sign(Dot(vRight, vDirToLeftTarget).Getf()));
				ScalarV fAngleRight = Angle(vDirection, vDirToRightTarget) * ScalarV(RtoD) * ScalarV(Sign(Dot(vRight, vDirToRightTarget).Getf()));
				
				pVehicle->GetIntelligence()->GetBoatAvoidanceHelper()->SetMaxAvoidanceOrientationLeftThisFrame(fAngleLeft.Getf());
				pVehicle->GetIntelligence()->GetBoatAvoidanceHelper()->SetMaxAvoidanceOrientationRightThisFrame(fAngleRight.Getf());
			}
			else
			{
				static float s_MaxAvoidance = 5.0f;
				pVehicle->GetIntelligence()->GetBoatAvoidanceHelper()->SetMaxAvoidanceOrientationLeftThisFrame(-s_MaxAvoidance);
				pVehicle->GetIntelligence()->GetBoatAvoidanceHelper()->SetMaxAvoidanceOrientationRightThisFrame(s_MaxAvoidance);
			}
		}
	}




	const float fDistToTargetSqr = vStartCoords.Dist2(vClosestPointOnEndLine);

	if (!m_waypoints.GetIsLoopPlayback() && fDistToTargetSqr < m_Params.m_fTargetArriveDist*m_Params.m_fTargetArriveDist)
	{
		SetState(State_Finished);
		return FSM_Continue;
	}

	// set the target for the gotopointwithavoidance task
	if(GetSubTask())
	{
		int iTaskType = GetSubTask()->GetTaskType();
		if (iTaskType == CTaskTypes::TASK_VEHICLE_GOTO_POINT_WITH_AVOIDANCE_AUTOMOBILE)
		{
			CTaskVehicleGoToPointWithAvoidanceAutomobile * pGoToTask = (CTaskVehicleGoToPointWithAvoidanceAutomobile*)GetSubTask();
			pGoToTask->SetTargetPosition(&vTargetPosition);
			pGoToTask->SetCruiseSpeed(fSpeed);
		}
		else if (iTaskType == CTaskTypes::TASK_VEHICLE_GOTO_HELICOPTER)
		{
			CTaskVehicleGoToHelicopter* pHeliTask = (CTaskVehicleGoToHelicopter*)GetSubTask();
			pHeliTask->SetTargetPosition(&vTargetPosition);
			pHeliTask->SetCruiseSpeed(fSpeed);
		}
		else if (iTaskType == CTaskTypes::TASK_VEHICLE_GOTO_PLANE)
		{
			CTaskVehicleGoToPlane* pPlaneTask = (CTaskVehicleGoToPlane*)GetSubTask();
			pPlaneTask->SetTargetPosition(&vTargetPosition);
			pPlaneTask->SetCruiseSpeed(fSpeed);
		}
	}
	return FSM_Continue;
}


////////////////////////////////////////////////////////////

void	CTaskVehicleFollowWaypointRecording::Paused_OnEnter				(CVehicle* UNUSED_PARAM(pVehicle))
{
	SetNewTask( rage_new CTaskVehicleStop());
}


////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskVehicleFollowWaypointRecording::Paused_OnUpdate				(CVehicle* pVehicle)
{
	if (m_waypoints.WantsToResumePlayback())
	{
		SetState(State_FollowRecording);
		return FSM_Continue;
	}

	if (GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_STOP))
	{
		return FSM_Quit;
	}

	ProcessSuperDummyHelper(pVehicle, 0.0f);

	return FSM_Continue;
}

////////////////////////////////////////////////////////////

void	CTaskVehicleFollowWaypointRecording::Finished_OnEnter				(CVehicle* UNUSED_PARAM(pVehicle))
{
	SetNewTask( rage_new CTaskVehicleStop());
}


////////////////////////////////////////////////////////////

CTask::FSM_Return	CTaskVehicleFollowWaypointRecording::Finished_OnUpdate				(CVehicle* pVehicle)
{
	if (GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_STOP))
	{
		return FSM_Quit;
	}

	ProcessSuperDummyHelper(pVehicle, 0.0f);

	return FSM_Continue;
}

void CTaskVehicleFollowWaypointRecording::ProcessSuperDummyHelper(CVehicle* pVehicle, const float fBackupSpeed) const
{
	if(pVehicle 
		&& pVehicle->m_nVehicleFlags.bTasksAllowDummying 
		&& pVehicle->GetVehicleAiLod().GetDummyMode() == VDM_SUPERDUMMY )
	{
		const CTaskVehicleMissionBase* pSubtask = (GetSubTask() && GetSubTask()->IsVehicleMissionTask()) 
			? static_cast<const CTaskVehicleMissionBase*>(GetSubTask()) 
			: NULL;

		const float fCruiseSpeed = pSubtask ? pSubtask->GetCruiseSpeed() : fBackupSpeed;
		m_followRoute.ProcessSuperDummy( pVehicle, ScalarV( fCruiseSpeed ), GetDrivingFlags(), GetTimeStep() );
	}
}


bool CTaskVehicleFollowWaypointRecording::GetIsPaused()
{
	return ( GetState()==State_Paused || m_waypoints.WantsToPause() );
}
void CTaskVehicleFollowWaypointRecording::SetPause()
{
	m_waypoints.SetPause();
}
void CTaskVehicleFollowWaypointRecording::SetResume()
{
	m_waypoints.SetResume();
}
void CTaskVehicleFollowWaypointRecording::SetOverrideSpeed(const float fSpeed)
{
	m_waypoints.SetOverrideSpeed(fSpeed);
}
void CTaskVehicleFollowWaypointRecording::UseDefaultSpeed()
{
	m_waypoints.UseDefaultSpeed();
}

#if !__FINAL
const char * CTaskVehicleFollowWaypointRecording::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_FollowRecording&&iState<=State_Finished);
	static const char* aStateNames[] = 
	{
		"State_FollowRecording",
		"State_Paused",
		"State_Finished",
	};

	return aStateNames[iState];
}

// PURPOSE: Display debug information specific to this task
void CTaskVehicleFollowWaypointRecording::Debug() const
{
#if DEBUG_DRAW
	if( GetState() == State_FollowRecording )
	{

		// Render the point we're heading towards.
		if(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_VEHICLE_GOTO_POINT_WITH_AVOIDANCE_AUTOMOBILE)
		{
			CTaskVehicleGoToPointWithAvoidanceAutomobile * pGoToTask = (CTaskVehicleGoToPointWithAvoidanceAutomobile*)GetSubTask();
			Vector3 vTargetPosition = *pGoToTask->GetTargetPosition();
			const Vector3 vStartCoords = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleBonnetPosition(GetVehicle(), IsDrivingFlagSet(DF_DriveInReverse)));
			grcDebugDraw::Line(vStartCoords + ZAXIS, vTargetPosition + ZAXIS, Color_green, Color_green);
			grcDebugDraw::Sphere(vTargetPosition + ZAXIS, 0.5f, Color_green, false);
			grcDebugDraw::Sphere(vStartCoords + ZAXIS, 0.5f, Color_green, false);
		}
	}	
#endif
}


#endif	//!__FINAL