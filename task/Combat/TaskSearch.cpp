// FILE :    TaskSearch.h
// PURPOSE : AI scour an area based on the last known position of the target and try to re-acquire.
//			 They can search using vehicles, on foot and transition between the two.
// AUTHOR :  Phil H

// File header
#include "Task\Combat\TaskSearch.h"

// Game headers
#include "Game/Clock.h"
#include "Peds/Ped.h"
#include "Peds/PedGeometryAnalyser.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PedWeapons/PedTargeting.h"
#include "Task/Vehicle/TaskCar.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Task/Vehicle/TaskExitVehicle.h"
#include "Task/Default/TaskChat.h"
#include "Task/Combat/Cover/Cover.h"
#include "Task/Combat/Cover/TaskCover.h"
#include "Task/Combat/Cover/TaskSeekCover.h"
#include "Task/Combat/TaskCombat.h"
#include "Task/Combat/TaskCombatMelee.h"
#include "Task/Combat/TaskNewCombat.h"
#include "task/Combat/Subtasks/TaskSearchInAutomobile.h"
#include "task/Combat/Subtasks/TaskSearchInBoat.h"
#include "task/Combat/Subtasks/TaskSearchInHeli.h"
#include "task/Combat/Subtasks/TaskSearchOnFoot.h"
#include "Task/Motion/Locomotion/TaskMotionPed.h"
#include "Task/Movement/TaskSeekEntity.h"
#include "Task/Weapons/WeaponController.h"
#include "vehicleAi/task/TaskVehicleGoTo.h"
#include "vehicleAi/task/TaskVehicleGoToAutomobile.h"

AI_OPTIMISATIONS()

//=========================================================================
// CTaskSearch
//=========================================================================

CTaskSearch::Tunables CTaskSearch::sm_Tunables;
IMPLEMENT_COMBAT_TASK_TUNABLES(CTaskSearch, 0xde7d84f7);

CTaskSearch::CTaskSearch(const Params& rParams)
: CTaskSearchBase(rParams)
{
	SetInternalTaskType(CTaskTypes::TASK_SEARCH);
}

CTaskSearch::~CTaskSearch()
{
	
}

#if !__FINAL
const char* CTaskSearch::GetStaticStateName(s32 iState)
{
	Assert(iState >= State_Start && iState <= State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_StareAtPosition",
		"State_GoToPositionOnFoot",
		"State_GoToPositionInVehicle",
		"State_PassengerInVehicle",
		"State_ExitVehicle",
		"State_SearchOnFoot",
		"State_SearchInAutomobile",
		"State_SearchInHeli",
		"State_SearchInBoat",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif

bool CTaskSearch::IsPositionOutsideDefensiveArea(Vec3V_In vPosition) const
{
	//Grab the ped.
	const CPed* pPed = GetPed();

	//Check if the position is outside the defensive area.
	if(!pPed->GetPedIntelligence()->GetDefensiveArea()->IsActive() ||
		pPed->GetPedIntelligence()->GetDefensiveArea()->IsPointInArea(VEC3V_TO_VECTOR3(vPosition)))
	{
		return false;
	}

	return true;
}

bool CTaskSearch::ShouldStareAtPosition() const
{
	//Grab the ped.
	const CPed* pPed = GetPed();

	//Check if the ped can't advance.
	if(pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatMovement() != CCombatData::CM_WillAdvance)
	{
		return true;
	}

	//Check if the position is outside the defensive area.
	if(IsPositionOutsideDefensiveArea(m_Params.m_vPosition))
	{
		return true;
	}

	return false;
}

CTask::FSM_Return CTaskSearch::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
	
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();
				
		FSM_State(State_StareAtPosition)
			FSM_OnEnter
				StareAtPosition_OnEnter();
			FSM_OnUpdate
				return StareAtPosition_OnUpdate();
				
		FSM_State(State_GoToPositionOnFoot)
			FSM_OnEnter
				GoToPositionOnFoot_OnEnter();
			FSM_OnUpdate
				return GoToPositionOnFoot_OnUpdate();
				
		FSM_State(State_GoToPositionInVehicle)
			FSM_OnEnter
				GoToPositionInVehicle_OnEnter();
			FSM_OnUpdate
				return GoToPositionInVehicle_OnUpdate();
				
		FSM_State(State_PassengerInVehicle)
			FSM_OnEnter
				PassengerInVehicle_OnEnter();
			FSM_OnUpdate
				return PassengerInVehicle_OnUpdate();
				
		FSM_State(State_ExitVehicle)
			FSM_OnEnter
				ExitVehicle_OnEnter();
			FSM_OnUpdate
				return ExitVehicle_OnUpdate();
				
		FSM_State(State_SearchOnFoot)
			FSM_OnEnter
				SearchOnFoot_OnEnter();
			FSM_OnUpdate
				return SearchOnFoot_OnUpdate();
				
		FSM_State(State_SearchInAutomobile)
			FSM_OnEnter
				SearchInAutomobile_OnEnter();
			FSM_OnUpdate
				return SearchInAutomobile_OnUpdate();
				
		FSM_State(State_SearchInHeli)
			FSM_OnEnter
				SearchInHeli_OnEnter();
			FSM_OnUpdate
				return SearchInHeli_OnUpdate();
				
		FSM_State(State_SearchInBoat)
			FSM_OnEnter
				SearchInBoat_OnEnter();
			FSM_OnUpdate
				return SearchInBoat_OnUpdate();
				
		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskSearch::Start_OnUpdate()
{
	//Grab the ped.
	const CPed* pPed = GetPed();
	
	//Check if the ped is driving a vehicle.
	if(pPed->GetIsDrivingVehicle())
	{
		//Go to the position in the vehicle.
		SetState(State_GoToPositionInVehicle);
	}
	//Check if the ped is in a vehicle.
	else if(pPed->GetIsInVehicle())
	{
		//The ped is a passenger in a vehicle.
		SetState(State_PassengerInVehicle);
	}
	//The ped is on foot.
	else
	{
		//Check if we should stare at the position.
		if(ShouldStareAtPosition())
		{
			//Stare at the position.
			SetState(State_StareAtPosition);
		}
		else
		{
			//Go to the position on foot.
			SetState(State_GoToPositionOnFoot);
		}
	}

	return FSM_Continue;
}

void CTaskSearch::StareAtPosition_OnEnter()
{
	//Create the task.
	CTask* pTask = rage_new CTaskDoNothing((int)(sm_Tunables.m_TimeToStare * 1000.0f));
	
	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskSearch::StareAtPosition_OnUpdate()
{
	//Check if the sub-task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Finish the task.
		SetState(State_Finish);
	}
	
	return FSM_Continue;
}

void CTaskSearch::GoToPositionOnFoot_OnEnter()
{
	//Create the params.
	CNavParams params;
	params.m_vTarget = VEC3V_TO_VECTOR3(m_Params.m_vPosition);
	params.m_fMoveBlendRatio = sm_Tunables.m_MoveBlendRatio;
	
	//Create the move task.
	CTaskMoveFollowNavMesh* pMoveTask = rage_new CTaskMoveFollowNavMesh(params);
	pMoveTask->SetQuitTaskIfRouteNotFound(true);
	pMoveTask->SetStopExactly(false);
	
	//Create the sub-task.
	CTask* pSubTask = rage_new CTaskCombatAdditionalTask(CTaskCombatAdditionalTask::RT_DefaultJustClips, NULL, VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition()), 0.0f);
	
	//Create the task.
	CTask* pTask = rage_new CTaskComplexControlMovement(pMoveTask, pSubTask);
	
	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskSearch::GoToPositionOnFoot_OnUpdate()
{
	//Check if the sub-task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Search on foot.
		SetState(State_SearchOnFoot);
	}
	
	return FSM_Continue;
}

void CTaskSearch::GoToPositionInVehicle_OnEnter()
{
	//Create the vehicle task.
	CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
	CPhysical* pTargetEntity = NULL;
	Vector3* pTargetPosition = &RC_VECTOR3(m_Params.m_vPosition);
	s32 iDrivingFlags = DMode_AvoidCars;
	float fTargetReached = sm_Tunables.m_TargetReached;
	float fStraightLineDistance = sVehicleMissionParams::DEFAULT_SWITCH_TO_STRAIGHT_LINE_DIST;
	float fCruiseSpeed = sm_Tunables.m_CruiseSpeed;
	aiTask* pVehicleTask = CVehicleIntelligence::GetGotoTaskForVehicle(pVehicle, pTargetEntity, pTargetPosition, iDrivingFlags, fTargetReached, fStraightLineDistance, fCruiseSpeed);
	
	//Create the task.
	CTask* pTask = rage_new CTaskControlVehicle(pVehicle, pVehicleTask);
	
	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskSearch::GoToPositionInVehicle_OnUpdate()
{
	//Check if the sub-task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Grab the ped.
		CPed* pPed = GetPed();
		
		//Ensure the vehicle is valid.
		const CVehicle* pVehicle = pPed->GetVehiclePedInside();
		if(!taskVerifyf(pVehicle, "The vehicle is invalid."))
		{
			//Search on foot.
			SetState(State_SearchOnFoot);
		}
		else
		{
			//Check if we are in a heli.
			if(pVehicle->InheritsFromHeli())
			{
				//Search in the heli.
				SetState(State_SearchInHeli);
			}
			//Check if we are in a boat.
			else if(pVehicle->InheritsFromBoat())
			{
				//Search in the boat.
				SetState(State_SearchInBoat);
			}
			else
			{
				//Grab the targeting.
				CPedTargetting* pTargeting = pPed->GetPedIntelligence()->GetTargetting(true);
				
				//Check if the target was last seen in a vehicle.
				bool bLastSeenInVehicle;
				if(pTargeting->GetTargetLastSeenInVehicle(m_Params.m_pTarget, bLastSeenInVehicle) && bLastSeenInVehicle)
				{
					//Search in the automobile.
					SetState(State_SearchInAutomobile);
				}
				else
				{
					//Exit the vehicle.
					SetState(State_ExitVehicle);
				}
			}
		}
	}
	
	return FSM_Continue;
}

void CTaskSearch::PassengerInVehicle_OnEnter()
{
}

CTask::FSM_Return CTaskSearch::PassengerInVehicle_OnUpdate()
{
	//Check if the vehicle is valid.
	const CVehicle* pVehicle = GetPed()->GetVehiclePedInside();
	if(pVehicle)
	{
		//Check if the driver is valid.
		const CPed* pDriver = pVehicle->GetDriver();
		if(pDriver)
		{
			//Check if the driver is not injured.
			if(!pDriver->IsInjured())
			{
				//Keep on being a passenger.
				return FSM_Continue;
			}
		}
	}
	
	//Exit the vehicle.
	SetState(State_ExitVehicle);
	
	return FSM_Continue;
}

void CTaskSearch::ExitVehicle_OnEnter()
{
	//Create the flags.
	VehicleEnterExitFlags flags;
	
	//Create the task.
	CTask* pTask = rage_new CTaskExitVehicle(GetPed()->GetVehiclePedInside(), flags);
	
	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskSearch::ExitVehicle_OnUpdate()
{
	//Check if the sub-task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Search on foot.
		SetState(State_SearchOnFoot);
	}
	
	return FSM_Continue;
}

void CTaskSearch::SearchOnFoot_OnEnter()
{
	//Create the task.
	CTask* pTask = rage_new CTaskSearchOnFoot(m_Params);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskSearch::SearchOnFoot_OnUpdate()
{
	//Check if the sub-task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Finish the task.
		SetState(State_Finish);
	}
	
	return FSM_Continue;
}

void CTaskSearch::SearchInAutomobile_OnEnter()
{
	//Create the task.
	CTask* pTask = rage_new CTaskSearchInAutomobile(m_Params);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskSearch::SearchInAutomobile_OnUpdate()
{
	//Check if the sub-task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Finish the task.
		SetState(State_Finish);
	}

	return FSM_Continue;
}

void CTaskSearch::SearchInHeli_OnEnter()
{
	//Create the task.
	CTask* pTask = rage_new CTaskSearchInHeli(m_Params);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskSearch::SearchInHeli_OnUpdate()
{
	//Check if the sub-task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Finish the task.
		SetState(State_Finish);
	}

	return FSM_Continue;
}

void CTaskSearch::SearchInBoat_OnEnter()
{
	//Create the task.
	CTask* pTask = rage_new CTaskSearchInBoat(m_Params);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskSearch::SearchInBoat_OnUpdate()
{
	//Check if the sub-task has finished.
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Finish the task.
		SetState(State_Finish);
	}

	return FSM_Continue;
}

// PURPOSE	:  A task used to search for threats that the ped doesn't know from where they came from.
// Peds will actively scan for nearby peds using the acquaintance scanners. If they meet friendly peds
// they will talk to them and inform them of the situation. If they meet unfriendly peds, they will question them.
// If they meet enemy peds, they will shout something like - you killed that guy or, it was you etc and go into combat.

// AUTHOR	: Chi-Wai Chiu
// CREATED	: 14-04-2009
// TODO		:

/////////////////////////////////////////////////
//		TASK SEARCH FOR UNKNOWN THREAT		   //
/////////////////////////////////////////////////

CTaskSearchForUnknownThreat::Tunables CTaskSearchForUnknownThreat::sm_Tunables;
IMPLEMENT_COMBAT_TASK_TUNABLES(CTaskSearchForUnknownThreat, 0xfdbdc37f);


CTaskSearchForUnknownThreat::CTaskSearchForUnknownThreat(const Vector3& vApproachDirection, s32 iEventType, s32 iTimeToSpendStanding)
: 
m_vApproachDirection(vApproachDirection),
m_vNewLocation(Vector3::ZeroType),
m_iTimeSpentStanding(0),
m_iTimeToStand(iTimeToSpendStanding), // -1 means stand indefinitely.
m_iEventType(iEventType),
m_iPreviousWeaponHash(0),
m_pHidingPoint(NULL)
{
	m_pChatHelper = CChatHelper::CreateChatHelper();
	if (m_pChatHelper)
	{
		m_pChatHelper->SetTimeBeforeChatTimerReset(15000); // Allow ped to chat to question peds every 15 seconds
	}
	SetInternalTaskType(CTaskTypes::TASK_SEARCH_FOR_UNKNOWN_THREAT);
}

CTaskSearchForUnknownThreat::~CTaskSearchForUnknownThreat()
{
	if (m_pChatHelper)
	{	
		delete m_pChatHelper;
		m_pChatHelper = NULL;
	}
}

CTask::FSM_Return CTaskSearchForUnknownThreat::ProcessPreFSM()
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	if (m_pChatHelper)
	{		
		m_pChatHelper->ProcessChat(pPed);
	}

	if (m_clearanceSearchHelper.IsActive())
	{
		m_clearanceSearchHelper.Update();
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskSearchForUnknownThreat::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.
	weaponAssert(pPed->GetWeaponManager() && pPed->GetInventory());

	FSM_Begin

		FSM_State(State_Start)
			FSM_OnEnter
				Start_OnEnter(pPed);
			FSM_OnUpdate
				return Start_OnUpdate(pPed);	

		FSM_State(State_SearchArea)
			FSM_OnEnter
				SearchArea_OnEnter(pPed);
			FSM_OnUpdate
				return SearchArea_OnUpdate(pPed);	

		FSM_State(State_GoToSearchPosition)
			FSM_OnEnter
				GoToSearchPosition_OnEnter(pPed);
			FSM_OnUpdate
				return GoToSearchPosition_OnUpdate(pPed);	

		FSM_State(State_StandForTime)
			FSM_OnEnter
				StandForTime_OnEnter(pPed);
			FSM_OnUpdate
				return StandForTime_OnUpdate(pPed);

		FSM_State(State_QuestionPed)
			FSM_OnEnter
				QuestionPed_OnEnter(pPed);
			FSM_OnUpdate
				return QuestionPed_OnUpdate(pPed);

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

void CTaskSearchForUnknownThreat::Start_OnEnter(CPed* UNUSED_PARAM(pPed))
{

}

CTask::FSM_Return CTaskSearchForUnknownThreat::Start_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	SetState(State_StandForTime);
	return FSM_Continue;
}

void CTaskSearchForUnknownThreat::SearchArea_OnEnter(CPed* pPed)
{		
	// Play search clips
	// Approach direction becomes the ped's current forward vector
	m_vApproachDirection = VEC3V_TO_VECTOR3(pPed->GetTransform().GetB());

}

CTask::FSM_Return CTaskSearchForUnknownThreat::SearchArea_OnUpdate(CPed* pPed)
{
	// If we find a hiding (cover) point away from the ped nearby, goto it, otherwise check 
	// if the clearance  search has finished and use the longest clear direction away from us
	const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	Vector3 pos = vPedPosition;
	CCoverPoint* pNewHidingPoint =CCover::FindClosestCoverPoint(pPed, vPedPosition,&pos,CCover::CS_ANY,CCoverPoint::COVTYPE_BIG_OBJECT);

	if (pNewHidingPoint && (pNewHidingPoint != m_pHidingPoint))
	{
		m_pHidingPoint = pNewHidingPoint;
		m_pHidingPoint->GetCoverPointPosition(m_vNewLocation);
		DEBUG_DRAW_ONLY(grcDebugDraw::Sphere(m_vNewLocation,0.3f, Color32(0, 255, 0, 255)));
		SetState(State_GoToSearchPosition);
	}
	else
	{
		if (m_clearanceSearchHelper.IsFinished())
		{
			// Find the longest clear area away from our approach direction
			m_clearanceSearchHelper.GetLongestDirectionAway(m_vNewLocation,m_vApproachDirection);
			m_vNewLocation *= fwRandom::GetRandomNumberInRange(0.5f,1.0f);
			m_vNewLocation += vPedPosition;
			SetState(State_GoToSearchPosition);
		}
	}	

	return FSM_Continue;
}

void CTaskSearchForUnknownThreat::GoToSearchPosition_OnEnter(CPed* pPed)
{		
	CTaskMoveFollowNavMesh* pNewTask = rage_new CTaskMoveFollowNavMesh(MOVEBLENDRATIO_WALK, m_vNewLocation);
	SetNewTask(rage_new CTaskComplexControlMovement(pNewTask));

	// Start a LOS probe async test
	WorldProbe::CShapeTestProbeDesc probeData;
	probeData.SetStartAndEnd(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()),m_vNewLocation);
	probeData.SetContext(WorldProbe::ELosCombatAI);
	probeData.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_OBJECT_TYPE|ArchetypeFlags::GTA_VEHICLE_TYPE);
	probeData.SetOptions(0); 
	m_asyncProbeHelper.ResetProbe();
	m_asyncProbeHelper.StartTestLineOfSight(probeData);
	m_searchPositionTimer.Set(fwTimer::GetTimeInMilliseconds(),fwRandom::GetRandomNumberInRange(sm_Tunables.m_iMinTimeBeforeSearchingForNewHidingPlace,sm_Tunables.m_iMaxTimeBeforeSearchingForNewHidingPlace));
}

CTask::FSM_Return CTaskSearchForUnknownThreat::GoToSearchPosition_OnUpdate(CPed* pPed)
{
	// Do a line of sight test here.. if we can see the cover point, then don't bother going to it, look for another position
	if (m_searchPositionTimer.IsSet() && m_searchPositionTimer.IsOutOfTime())
	{
		m_searchPositionTimer.Set(fwTimer::GetTimeInMilliseconds(),fwRandom::GetRandomNumberInRange(sm_Tunables.m_iMinTimeBeforeSearchingForNewHidingPlace,sm_Tunables.m_iMaxTimeBeforeSearchingForNewHidingPlace)); // Don't perform a search for a bit
		if (m_asyncProbeHelper.IsProbeActive())
		{
			ProbeStatus probeStatus;

			if (m_asyncProbeHelper.GetProbeResults(probeStatus))
			{
				// If position is clear, the direction vector is the test distance times direction vector
				if (probeStatus == PS_Clear)
				{
					SetState(State_StandForTime);
					return FSM_Continue;
				}
			}
		}
	}

	if (GetIsSubtaskFinished(CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT))
	{
		SetState(State_StandForTime);
		return FSM_Continue;
	}

	// If the ped is following a navmesh route that fails, give up for now.
	if (GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
	{
		aiTask* pTask = static_cast<CTaskComplexControlMovement*>(GetSubTask())->GetRunningMovementTask(pPed);
		if (pTask && pTask->GetTaskType() == CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH)
		{
			CTaskMoveFollowNavMesh* pNavmeshTask  = static_cast<CTaskMoveFollowNavMesh*>(pTask);
			if( pNavmeshTask->GetNavMeshRouteResult() == NAVMESHROUTE_ROUTE_NOT_FOUND &&
				pNavmeshTask->GetNavMeshRouteMethod() >= CTaskMoveFollowNavMesh::eRouteMethod_LastMethodWhichAvoidsObjects)
			{
				if( GetSubTask()->MakeAbortable( CTask::ABORT_PRIORITY_URGENT, NULL ) )
				{
					SetState(State_StandForTime);
					return FSM_Continue;
				}
			}
		}
	}

	// Look for chats
	if (m_pChatHelper && m_pChatHelper->ShouldGoToChatState())
	{
		m_pChatHelper->SetIsChatting(true); 
		SetState(State_QuestionPed);
	}

	return FSM_Continue;
}


void CTaskSearchForUnknownThreat::StandForTime_OnEnter(CPed* pPed)
{		
	static dev_float s_fTestDistance = 5.0f;

	m_iTimeSpentStanding = 0;

	const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	// Start a search to find the largest open distance along 8 directions from the peds current position
	m_clearanceSearchHelper.Start(vPedPosition);

	// Start a LOS probe async test to test if we're facing a wall
	WorldProbe::CShapeTestProbeDesc probeData;
	probeData.SetStartAndEnd(vPedPosition,vPedPosition + s_fTestDistance * VEC3V_TO_VECTOR3(pPed->GetTransform().GetB()));
	probeData.SetContext(WorldProbe::ELosCombatAI);
	probeData.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_OBJECT_TYPE|ArchetypeFlags::GTA_VEHICLE_TYPE);
	probeData.SetOptions(0); 
	m_asyncProbeHelper.ResetProbe();
	m_asyncProbeHelper.StartTestLineOfSight(probeData);

}

CTask::FSM_Return CTaskSearchForUnknownThreat::StandForTime_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	// Get results of los test
	if (m_asyncProbeHelper.IsProbeActive())
	{
		ProbeStatus probeStatus;

		if (m_asyncProbeHelper.GetProbeResults(probeStatus))
		{
			// If position isn't clear, we're probably facing a wall, move on
			if (probeStatus != PS_Clear)
			{
				SetState(State_SearchArea);
				return FSM_Continue;
			}
		}
	}

  // If iTimeToStand is -1, wait indefinitely
	if (m_iTimeToStand != -1)
	{
		m_iTimeSpentStanding += fwTimer::GetTimeStepInMilliseconds();

		// Wait for some time before moving on
		if (m_iTimeSpentStanding > m_iTimeToStand)
		{
			SetState(State_Finish);
		}
	}

	// Look for chats
	if (m_pChatHelper && m_pChatHelper->ShouldGoToChatState())
	{
		m_pChatHelper->SetIsChatting(true); 
		SetState(State_QuestionPed);
	}

	return FSM_Continue;
}

void CTaskSearchForUnknownThreat::QuestionPed_OnEnter(CPed* UNUSED_PARAM(pPed))
{		
	if (m_pChatHelper)
	{
		if (m_pChatHelper->ShouldStartTaskAsInitiator()) // This function contains common functionality for tasks using the chat helper
		{
			SetNewTask( rage_new CTaskChat(m_pChatHelper->GetOutGoingChatPed(),CTaskChat::CF_IsInitiator) );
		}
		else
		{
			SetNewTask( rage_new CTaskChat(m_pChatHelper->GetInComingChatPed(),0) );
		}
	}
}

CTask::FSM_Return CTaskSearchForUnknownThreat::QuestionPed_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	if (m_pChatHelper && GetIsSubtaskFinished(CTaskTypes::TASK_CHAT))
	{
		m_pChatHelper->FinishChat();
		SetState(State_StandForTime);
	}
	else if (!m_pChatHelper || !GetSubTask()) // If we didn't set a chat subtask in OnEnter, go back to standing still
	{
		SetState(State_StandForTime);
	}
	return FSM_Continue;
}

#if !__FINAL
void CTaskSearchForUnknownThreat::Debug() const
{
#if DEBUG_DRAW
	if (GetPed())
	{
		grcDebugDraw::Line(VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition()), m_vNewLocation, Color_red1, Color_red1);
		Vector3 vCoverPosition = Vector3(Vector3::ZeroType);
		if (m_pHidingPoint)
		{	
			m_pHidingPoint->GetCoverPointPosition(vCoverPosition);
		}
		grcDebugDraw::Sphere(vCoverPosition, 0.5f, Color_red1);
		
	}
#endif // DEBUG_DRAW
}

const char * CTaskSearchForUnknownThreat::GetStaticStateName( s32 iState )
{
	// Make sure FSM state is valid
	if (taskVerifyf(iState >= State_Start && iState <= State_Finish,"Invalid State"))
	{
		static const char* aStateNames[] = 
		{
			"State_Start",
			"State_GoToSearchPosition",
			"State_SearchArea",
			"State_StandForTime",
			"State_QuestionPed",
			"State_Finish"
		};
		return aStateNames[iState];
	}

	return "Invalid State";
}
#endif // !__FINAL
