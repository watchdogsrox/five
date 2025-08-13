#include "TaskVehicleCruise.h"

// Game headers
#include "Event/EventShocking.h"
#include "Event/ShockingEvents.h"
#include "Peds/ped.h"
#include "Peds/PedIntelligence.h"
#include "scene/World/GameWorld.h"
#include "Task/Default/TaskAmbient.h"
#include "VehicleAi/driverpersonality.h"
#include "vehicleAi/roadspeedzone.h"    
#include "VehicleAi/task/TaskVehicleMissionBase.h"
#include "VehicleAi/task/TaskVehiclePark.h"
#include "VehicleAi/task/TaskVehicleTempAction.h"
#include "VehicleAi/VehicleIntelligence.h"
#include "vehicles/vehiclepopulation.h"
#include "ai\debug\system\AIDebugLogManager.h"
#include "network/objects/entities/NetObjVehicle.h"
#include "Event/System/EventData.h"
#include "Event/Events.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

#if __STATS
PF_PAGE(taskVehicleCruise, "TaskVehicleCruise");
PF_GROUP(taskVehicleCruise);
PF_LINK(taskVehicleCruise, taskVehicleCruise);

PF_TIMER(UpdateEmptyLaneAwareness, taskVehicleCruise);
#endif // __STATS

//static dev_float s_fBoundRadiusMultiplier = 1.0f;
bank_bool CTaskVehicleCruiseNew::ms_bAllowCruiseTaskUseNavMesh = true;

//if this returns true, bLaneChangesLeft will be TRUE for left turns, FALSE for right
bool DoesPathHaveLaneChange(CVehicleNodeList* pNodeList, float fDistToConsider, bool& bLaneChangesLeft)
{	
	if(!pNodeList)
	{
		return false;
	}

	const int iFirstNodeIndex = NODE_VERY_OLD;
	const CPathNode* pOldNode = pNodeList->GetPathNode(iFirstNodeIndex);
	if (!pOldNode)
	{
		return false;
	}

	const CPathNode* pLastNode = pOldNode;
	Vector3 vLastNodeCoords;
	pLastNode->GetCoors(vLastNodeCoords);

	const int iInitialLane = pNodeList->GetPathLaneIndex(iFirstNodeIndex);

	for (int i = iFirstNodeIndex+1; i < CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED; i++)
	{
		if (fDistToConsider < 0.0f)
		{
			return false;
		}

		const CPathNode* pCurrentPathNode = pNodeList->GetPathNode(i);
		if (pCurrentPathNode)
		{
			Vector3 vCurrentCoords;
			pCurrentPathNode->GetCoors(vCurrentCoords);

			if (pCurrentPathNode->NumLinks() > 2)
			{
				return false;
			}

			//don't add up any of the distance behind us
			if (i >= NODE_NEW)
			{
				fDistToConsider -= vCurrentCoords.Dist(vLastNodeCoords);
			}
			
			if (pNodeList->GetPathLaneIndex(i) != iInitialLane)
			{
				bLaneChangesLeft = pNodeList->GetPathLaneIndex(i) < iInitialLane;
				return true;
			}

			pLastNode = pCurrentPathNode;
			pLastNode->GetCoors(vLastNodeCoords);
		}
		else
		{
			return false;
		}
	}

	return false;
}



////////////////////////////////////////////////////////////////////////////////////////

CTaskVehicleFlee::Tunables CTaskVehicleFlee::sm_Tunables;
IMPLEMENT_VEHICLE_AI_TASK_TUNABLES(CTaskVehicleFlee, 0xfc59e442);

u32 CTaskVehicleFlee::sm_uLastHonkTime = 0;
u32 CTaskVehicleFlee::sm_uTimeBetweenHonks = 20000;

CTaskVehicleFlee::CTaskVehicleFlee(const sVehicleMissionParams& params, const fwFlags8 uConfigFlags, float fleeIntensity)
: CTaskVehicleMissionBase(params)
, m_uLastTimeTargetGettingUp(0)
, m_uConfigFlags(uConfigFlags)
, m_uRunningFlags(0)
, m_fFleeIntensity(fleeIntensity)
, m_fMaxCruiseSpeed(-1.0f)
, m_fStopRecheckTimer(1.0f)
, m_bPreventStopAndWait(false)
{
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_FLEE);
}

bool CTaskVehicleFlee::ShouldHesitateForEvent(const CPed* pPed, const CEvent& rEvent)
{
	//Ensure the ped is valid.
	if(!pPed)
	{
		return false;
	}

	//Ensure the flag is not set.
	if(pPed->GetPedIntelligence()->GetFleeBehaviour().GetFleeFlags().IsFlagSet(CFleeBehaviour::BF_DisableHesitateInVehicle))
	{
		return false;
	}

	//Ensure the ped is driving a vehicle.
	if(!pPed->GetIsDrivingVehicle())
	{
		return false;
	}

	//Ensure the event type is valid.
	switch(rEvent.GetEventType())
	{
		case EVENT_SHOCKING_SEEN_WEAPON_THREAT:
		case EVENT_SHOT_FIRED:
		case EVENT_SHOT_FIRED_BULLET_IMPACT:
		case EVENT_SHOT_FIRED_WHIZZED_BY:
		case EVENT_VEHICLE_DAMAGE_WEAPON:
		{
			break;
		}
		default:
		{
			return false;
		}
	}

	//Ensure the chances are valid.
	float fChances = sm_Tunables.m_ChancesForHesitate;
	if(fChances <= 0.0f)
	{
		return false;
	}

	//Ensure the chances exceed the threshold.
	float fRandom = fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
	if(fRandom > fChances)
	{
		return false;
	}

	//Ensure the vehicle speed does not exceed the threshold.
	float fSpeedSq = pPed->GetVehiclePedInside()->GetVelocity().XYMag2();
	float fMaxSpeedSq = square(sm_Tunables.m_MaxSpeedForHesitate);
	if(fSpeedSq > fMaxSpeedSq)
	{
		return false;
	}

	return true;
}

bool CTaskVehicleFlee::ShouldSwerveForEvent(const CPed* pPed, const CEvent& rEvent)
{
	//Ensure the ped is valid.
	if(!pPed)
	{
		return false;
	}

	//Ensure the ped is driving a vehicle.
	if(!pPed->GetIsDrivingVehicle())
	{
		return false;
	}

	//Ensure the event type is valid.
	switch(rEvent.GetEventType())
	{
		case EVENT_SHOCKING_SEEN_WEAPON_THREAT:
		case EVENT_SHOT_FIRED:
		case EVENT_SHOT_FIRED_BULLET_IMPACT:
		case EVENT_SHOT_FIRED_WHIZZED_BY:
		case EVENT_VEHICLE_DAMAGE_WEAPON:
		{
			break;
		}
		default:
		{
			return false;
		}
	}

	//Ensure the chances are valid.
	float fChances = sm_Tunables.m_ChancesForSwerve;
	if(fChances <= 0.0f)
	{
		return false;
	}

	//Ensure the chances exceed the threshold.
	float fRandom = fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
	if(fRandom > fChances)
	{
		return false;
	}

	//Ensure the vehicle speed exceeds the threshold.
	float fSpeedSq = pPed->GetVehiclePedInside()->GetVelocity().XYMag2();
	float fMinSpeedSq = square(sm_Tunables.m_MinSpeedForSwerve);
	if(fSpeedSq < fMinSpeedSq)
	{
		return false;
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskVehicleFlee::ProcessPreFSM()
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.
	const CPhysical* pTargetEntity = GetTargetEntity();
	if (pVehicle && pTargetEntity)
	{
		pVehicle->GetIntelligence()->m_pFleeEntity = pTargetEntity;
		m_Params.SetTargetPosition(VEC3V_TO_VECTOR3(pTargetEntity->GetTransform().GetPosition()));	// Cache last position to keep flee from it
	}

	if (pVehicle)
	{
		//fleeing vehicles are always dirty
		pVehicle->m_nVehicleFlags.bAvoidanceDirtyFlag = true;

		//fleeing vehicles don't drive within road limits and so can drive through barriers when in dummy mode
		//when within 70 meters of player, make real to avoid seeing this
		Vec3V vVehiclePosition = pVehicle->GetTransform().GetPosition();
		Vec3V vTargetPosition;
		FindTargetCoors(pVehicle, RC_VECTOR3(vTargetPosition));
		float fDistToTarget = DistSquared(vTargetPosition, vVehiclePosition).Getf();
		if(fDistToTarget < 4900.0f)
		{
			pVehicle->m_nVehicleFlags.bPreventBeingDummyThisFrame = true;
		}
	}

	// Check for honking.
	if (ShouldHonkHorn())
	{
		// Honking is approved.
		HonkHorn();

		// Note that we have honked.
		sm_uLastHonkTime = fwTimer::GetTimeInMilliseconds();
	}

	if(m_fMaxCruiseSpeed < 0.0f)
	{
		const CVehicle* pVehicle = GetVehicle();
		float fVehicleModelSpeed = CVehiclePopulation::PickCruiseSpeedWithVehModel(pVehicle, pVehicle->GetModelIndex());
		m_fMaxCruiseSpeed = rage::Min(fVehicleModelSpeed, CDriverPersonality::FindMaxCruiseSpeed(pVehicle->GetDriver(), pVehicle, pVehicle->GetVehicleType() == VEHICLE_TYPE_BICYCLE));
	}

	return FSM_Continue;
}

void CTaskVehicleFlee::CloneUpdate(CVehicle* pVehicle)
{
	CTaskVehicleMissionBase::CloneUpdate(pVehicle);
	pVehicle->m_nVehicleFlags.bAvoidanceDirtyFlag = true;
}

///////////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskVehicleFlee::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.

	FSM_Begin
		// Start state
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate(pVehicle);

		// Flee state
		FSM_State(State_Flee)
			FSM_OnUpdate
				return Flee_OnUpdate(pVehicle);
			FSM_OnExit
				Flee_OnExit();

		// Swerve state
		FSM_State(State_Swerve)
			FSM_OnEnter
				Swerve_OnEnter();
			FSM_OnUpdate
				return Swerve_OnUpdate();

		// Swerve state
		FSM_State(State_Hesitate)
			FSM_OnEnter
				Hesitate_OnEnter();
			FSM_OnUpdate
				return Hesitate_OnUpdate();

		// Cruise state
		FSM_State(State_Cruise)
			FSM_OnEnter
				Cruise_OnEnter(pVehicle);
		FSM_OnUpdate
				return Cruise_OnUpdate(pVehicle);
				
		// SkidToStop state
		FSM_State(State_SkidToStop)
			FSM_OnEnter
				SkidToStop_OnEnter();
			FSM_OnUpdate
				return SkidToStop_OnUpdate();

		// Pause state
		FSM_State(State_Pause)
			FSM_OnEnter
				Pause_OnEnter();
			FSM_OnUpdate
				return Pause_OnUpdate();

		// Stop state
		FSM_State(State_Stop)
			FSM_OnEnter
				Stop_OnEnter(pVehicle);
			FSM_OnUpdate
				return Stop_OnUpdate(pVehicle);

		// Stop and wait state
		FSM_State(State_StopAndWait)
			FSM_OnEnter
			StopAndWait_OnEnter(pVehicle);
		FSM_OnUpdate
			return StopAndWait_OnUpdate(pVehicle);
		FSM_OnExit
			StopAndWait_OnExit(pVehicle);

	FSM_End
}

aiTask::FSM_Return CTaskVehicleFlee::Start_OnUpdate(CVehicle* pVehicle)
{
	//determine if we should just come to a stop due to a large accident ahead or flee normally
	Vec3V wreckCenter(V_ZERO);
	int iNumWrecks = 0;
	if(!m_bPreventStopAndWait && GetPreviousState() != State_StopAndWait && ShouldWaitForAccident(pVehicle, wreckCenter, iNumWrecks))
	{
		SetState(State_StopAndWait);
		return FSM_Continue;
	}

	SetState(State_Flee);
	return FSM_Continue;
}

/////////////////////////////////////////////////////////////////////////////////////
// If the mission is to flee we turn this into a goto away from the target
/////////////////////////////////////////////////////////////////////////////////////
aiTask::FSM_Return	CTaskVehicleFlee::Flee_OnUpdate(CVehicle* pVehicle)
{	
	Vector3 vTargetPos(Vector3::ZeroType);
	FindTargetCoors(pVehicle, vTargetPos);

	//don't rely on the user to pass this in--if we're fleeing we always want to avoid target coords
	SetDrivingFlag(DF_AvoidTargetCoors, true);

	//should we skid to a stop?
// 	if (pVehicle->GetAiXYSpeed() < 15.0f 
// 		&& !WillBarrelThroughInsteadOfUTurn(VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleBonnetPosition(pVehicle, IsDrivingFlagSet(DF_DriveInReverse)))
// 		, vTargetPos
// 		, pVehicle->GetAiVelocity()))
// 	{
// 		m_uConfigFlags.SetFlag(CF_SkidToStop);
// 	}

	//Check if we should swerve.
	if(m_uConfigFlags.IsFlagSet(CF_Swerve))
	{
		SetState(State_Swerve);
	}
	//Check if we should hesitate.
	else if(m_uConfigFlags.IsFlagSet(CF_Hesitate))
	{
		SetState(State_Hesitate);
	}
	else if (GetTargetEntity() || vTargetPos.IsNonZero() )  // It's possible we have lost the target just this frame
	{
		SetState(State_Cruise);
	}
	else
	{
		SetState(State_Stop);
	}

	return FSM_Continue;
}

void CTaskVehicleFlee::Flee_OnExit()
{
	//Clear the flags.
	m_uConfigFlags.ClearFlag(CF_Swerve);
	m_uConfigFlags.ClearFlag(CF_Hesitate);
}

void CTaskVehicleFlee::Swerve_OnEnter()
{
	//Create the task.
	float fTimeToSwerve = fwRandom::GetRandomNumberInRange(sm_Tunables.m_MinTimeToSwerve, sm_Tunables.m_MaxTimeToSwerve);
	CTaskVehicleSwerve::SwerveDirection nSwerveDirection = (ShouldSwerveRight() ? CTaskVehicleSwerve::Swerve_Right : CTaskVehicleSwerve::Swerve_Left);

	bool bShouldJitter = ShouldJitter();
	if(bShouldJitter)
	{
		fTimeToSwerve = fwRandom::GetRandomNumberInRange(sm_Tunables.m_MinTimeToJitter, sm_Tunables.m_MaxTimeToJitter);
		nSwerveDirection = CTaskVehicleSwerve::Swerve_Jitter;
	}

	CTask* pTask = rage_new CTaskVehicleSwerve(fwTimer::GetTimeInMilliseconds() + (u32)(fTimeToSwerve * 1000.0f),
		nSwerveDirection, false, false, Vector2(0.0f, 0.0f), Vector2(0.0f, 0.0f));

	//Start the task.
	SetNewTask(pTask);
}

aiTask::FSM_Return CTaskVehicleFlee::Swerve_OnUpdate()
{
	//Check if the sub-task has finished.
	if(GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_SWERVE))
	{
		CTaskVehicleSwerve* pSwerveTask = static_cast<CTaskVehicleSwerve*>(GetSubTask());
		if(pSwerveTask && pSwerveTask->GetSwerveDirection() == CTaskVehicleSwerve::Swerve_Jitter)
		{
			//start fleeing
			SetState(State_Flee);
		}
		else
		{
			//Move to the cruise state.
			SetState(State_Cruise);
		}
	}

	return FSM_Continue;
}

void CTaskVehicleFlee::Hesitate_OnEnter()
{
}

aiTask::FSM_Return CTaskVehicleFlee::Hesitate_OnUpdate()
{
	//Check if the time has elapsed.
	float fTimeToHesitate = GetVehicle()->GetRandomNumberInRangeFromSeed(sm_Tunables.m_MinTimeToHesitate, sm_Tunables.m_MaxTimeToHesitate);

	GetVehicle()->GetIntelligence()->UpdateCarHasReasonToBeStopped();

	GetVehicle()->m_nVehicleFlags.bIsHesitating = true;

	if(GetTimeInState() > fTimeToHesitate)
	{
		//Check if we are getting up.
		if(IsTargetGettingUp())
		{
			m_uLastTimeTargetGettingUp = fwTimer::GetTimeInMilliseconds();
		}

		//Check if we should hesitate until the target gets up.
		static dev_u32 s_uMinTime = 1000;
		bool bContinueHesitating = (m_uConfigFlags.IsFlagSet(CF_HesitateUntilTargetGetsUp) &&
			(CTimeHelpers::GetTimeSince(m_uLastTimeTargetGettingUp) < s_uMinTime));
		if(!bContinueHesitating)
		{
			//Move to the cruise state.
			SetState(State_Cruise);
		}
	}

	return FSM_Continue;
}

void CTaskVehicleFlee::Pause_OnEnter()
{
	SetNewTask(rage_new CTaskVehicleBrake(NetworkInterface::GetSyncedTimeInMilliseconds() + 1500));
}

aiTask::FSM_Return CTaskVehicleFlee::Pause_OnUpdate()
{
	GetVehicle()->GetIntelligence()->UpdateCarHasReasonToBeStopped();

	if(!GetSubTask() || GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_BRAKE))
	{

		//Move to the cruise state.
		SetState(State_Cruise);
	}

	return FSM_Continue;
}

bool CTaskVehicleFlee::WillBarrelThroughInsteadOfUTurn(const Vector3& vSearchPosition, const Vector3& vTargetPos, const Vector3& vInitialVelocity, const bool bMissionVeh)
{
	if (!bMissionVeh)
	{
		return true;
	}

	const dev_float s_fZThresholdForUTurn = 30.0f;
	if (fabsf(vSearchPosition.z - vTargetPos.z) > s_fZThresholdForUTurn)
	{
		//never u turn if the target is outside of some z threshold.
		return true;
	}

	const Vector3 vDelta = vSearchPosition - vTargetPos;
	const float fDistSqr = vDelta.XYMag2();

	//if we're too close to what we're trying to flee, don't do anything
	static dev_float s_fDontTurnAroundDistanceSqr = 25.0f * 25.0f;
	if (fDistSqr < s_fDontTurnAroundDistanceSqr)
	{
		return true;
	}

	//if we're moving too fast, and somewhat close, don't do anything
	static dev_float s_fDontTurnAroundWhenMovingDistSqr = 50.0f * 50.0f;
	static dev_float s_fMovingMinSpeedSqr = 4.0f * 4.0f;
	if (fDistSqr < s_fDontTurnAroundWhenMovingDistSqr && vInitialVelocity.Mag2() > s_fMovingMinSpeedSqr)
	{
		return true;
	}

	return false;
}

void CTaskVehicleFlee::PreventStopAndWait()
{
	m_bPreventStopAndWait = true; 

	if(GetState() == State_StopAndWait) 
	{
		m_fStopRecheckTimer = -1.0f; 
	}
}

void CTaskVehicleFlee::SwapFirstTwoNodesIfPossible(CVehicleNodeList* pNodeList, const Vector3& vSearchPosition, const Vector3& vInitialVelocity, const Vector3& vTargetPos, const bool bMissionVeh)
{
	if (!pNodeList)
	{
		return;
	}

	if (WillBarrelThroughInsteadOfUTurn(vSearchPosition, vTargetPos, vInitialVelocity, bMissionVeh))
	{
		return;
	}

	pNodeList->ClearNonEssentialNodes();

	const int iTargetNode = pNodeList->GetTargetNodeIndex();
	const int iPrevNode = iTargetNode - 1;

	if (!aiVerify(iTargetNode > 0))
	{
		return;
	}

	if (!pNodeList->GetPathNodeAddr(iPrevNode).IsEmpty() && !pNodeList->GetPathNodeAddr(iTargetNode).IsEmpty() &&
		ThePaths.IsRegionLoaded(pNodeList->GetPathNodeAddr(iPrevNode)) &&
		ThePaths.IsRegionLoaded(pNodeList->GetPathNodeAddr(iTargetNode)))
	{
		const CPathNode *pOldNode = ThePaths.FindNodePointer(pNodeList->GetPathNodeAddr(iPrevNode));
		const CPathNode *pNewNode = ThePaths.FindNodePointer(pNodeList->GetPathNodeAddr(iTargetNode));

		//only do this if we found a link in the opposite direction
		s16 iRegionLinkIndex = ThePaths.FindRegionLinkIndexBetween2Nodes(
			pNodeList->GetPathNodeAddr(iTargetNode), pNodeList->GetPathNodeAddr(iPrevNode));
		if (iRegionLinkIndex >= 0)
		{
			Vector3	oldCoors, newCoors;
			pOldNode->GetCoors(oldCoors);
			pNewNode->GetCoors(newCoors);

			// Dont u turn on a highway
			if( pOldNode->IsHighway() || pNewNode->IsHighway() )
			{
				return;
			}

			Vector3	vCarToTarget = vTargetPos - vSearchPosition;
			Vector3	vOldToNewNode = newCoors - oldCoors;
			vCarToTarget.NormalizeSafe();
			vOldToNewNode.NormalizeSafe();
			const float fDebugDotProduct = DotProduct(vCarToTarget, vOldToNewNode);
			if (fDebugDotProduct > 0.8f)
			{		
				// Swap the 2 nodes round.
				pNodeList->ClearLanes();
				pNodeList->SetPathNodeAddr(iPrevNode, pNewNode->m_address);
				pNodeList->SetPathNodeAddr(iTargetNode, pOldNode->m_address);
				pNodeList->SetPathLinkIndex(iPrevNode, iRegionLinkIndex);
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////
//State Cruise
void	CTaskVehicleFlee::Cruise_OnEnter(CVehicle* pVehicle)
{
	Vector3 vTargetPos(Vector3::ZeroType);

	FindTargetCoors(pVehicle, vTargetPos);
	if( GetTargetEntity() )
	{
		vTargetPos = VEC3V_TO_VECTOR3(GetTargetEntity()->GetTransform().GetPosition());
	}

	SetNewTask(CVehicleIntelligence::CreateCruiseTask(*pVehicle, m_Params));
}

const float CTaskVehicleFlee::ms_fSpeedBelowWhichToBailFromVehicle = 1.0f;

/////////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehicleFlee::Cruise_OnUpdate(CVehicle* pVehicle)
{
	UpdateCruiseSpeed();

	Vector3 vTargetPos(Vector3::ZeroType);
	FindTargetCoors(pVehicle, vTargetPos);

	if (!GetTargetEntity() && vTargetPos.IsZero() )  // It's possible we have lost the target just this frame
	{
		SetState(State_Stop);
		return FSM_Continue;
	}

	Vec3V wreckCenter(V_ZERO);
	int iNumWrecks = 0;
	m_fStopRecheckTimer -= fwTimer::GetTimeStep();
	if(!m_bPreventStopAndWait && m_fStopRecheckTimer <= 0.0f)
	{
		m_fStopRecheckTimer = 1.5f;
		if(GetPreviousState() != State_StopAndWait && ShouldWaitForAccident(pVehicle, wreckCenter, iNumWrecks))
		{
			SetState(State_StopAndWait);
			return FSM_Continue;
		}
	}

	//Check if we should skid to a stop.
	if(CheckShouldSkidToStop())
	{
		//Move to the skid to stop state.
		SetState(State_SkidToStop);
		return FSM_Continue;
	}

	//if we're in low lod and entirely blocked, pause a little before checking again
	const bool bHasValidDriver = pVehicle->m_nVehicleFlags.bUsingPretendOccupants;
	CTaskVehicleGoToPointWithAvoidanceAutomobile* pGotoSubtask = 
		static_cast<CTaskVehicleGoToPointWithAvoidanceAutomobile*>(FindSubTaskOfType(CTaskTypes::TASK_VEHICLE_GOTO_POINT_WITH_AVOIDANCE_AUTOMOBILE));
	const bool bMovingSlowEnough = pVehicle->GetAiXYSpeed() < ms_fSpeedBelowWhichToBailFromVehicle;

	if (bHasValidDriver && bMovingSlowEnough && pGotoSubtask 
	&& pGotoSubtask->GetObstructionInformation().m_uFlags.IsFlagSet(CTaskVehicleGoToPointWithAvoidanceAutomobile::ObstructionInformation::IsCompletelyObstructedClose))
	{
		SetState(State_Pause);
		return FSM_Continue;
	}
	
	return FSM_Continue;
}

/////////////////////////////////////////////////////////////////////////////////////

void CTaskVehicleFlee::SkidToStop_OnEnter()
{
	//Generate the handbrake action based on the skid direction.
	CTaskVehicleHandBrake::HandBrakeAction nAction;
	if(m_uRunningFlags.IsFlagSet(VFRF_SkidLeft))
	{
		nAction = CTaskVehicleHandBrake::HandBrake_Left_Intelligence;
	}
	else if(m_uRunningFlags.IsFlagSet(VFRF_SkidStraight))
	{
		nAction = CTaskVehicleHandBrake::HandBrake_Straight_Intelligence;
	}
	else if(m_uRunningFlags.IsFlagSet(VFRF_SkidRight))
	{
		nAction = CTaskVehicleHandBrake::HandBrake_Right_Intelligence;
	}
	else
	{
		taskAssertf(false, "Invalid skid direction.");
		return;
	}
	
	//Clear the skid directions.
	m_uRunningFlags.ClearFlag(VFRF_SkidLeft);
	m_uRunningFlags.ClearFlag(VFRF_SkidStraight);
	m_uRunningFlags.ClearFlag(VFRF_SkidRight);
	
	//Note that we have skidded to a stop.
	m_uRunningFlags.SetFlag(VFRF_HasSkiddedToStop);
	
	//Start the task.
	SetNewTask(rage_new CTaskVehicleHandBrake(NetworkInterface::GetSyncedTimeInMilliseconds() + 2000, nAction));
}

/////////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskVehicleFlee::SkidToStop_OnUpdate()
{
	//Wait for the sub-task to finish.
	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Check if we should hesitate.
		if(m_uConfigFlags.IsFlagSet(CF_HesitateAfterSkidToStop))
		{
			//Hesitate.
			SetState(State_Hesitate);
		}
		else
		{
			//Move back to the cruise state.
			SetState(State_Cruise);
		}
	}
	
	return FSM_Continue;
}

/////////////////////////////////////////////////////////////////////////////////////
//State Stop
void	CTaskVehicleFlee::Stop_OnEnter(CVehicle* UNUSED_PARAM(pVehicle))
{
	SetNewTask(rage_new CTaskVehicleStop());
}

/////////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehicleFlee::Stop_OnUpdate(CVehicle* UNUSED_PARAM(pVehicle))
{
	if (GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_STOP))
	{
		return FSM_Quit;
	}

	return FSM_Continue;
}

void CTaskVehicleFlee::StopAndWait_OnEnter(CVehicle* UNUSED_PARAM(pVehicle))
{
	SetNewTask(rage_new CTaskVehicleStop(CTaskVehicleStop::SF_DontTerminateWhenStopped));

	//delay restart of flee for a bit so all cars don't start at once
	m_fStopRecheckTimer = fwRandom::GetRandomNumberInRange(20.0f, 30.0f);
}

//checks if the given vehicle is inside the quad created by the provided nodes
bool CTaskVehicleFlee::IsEntityWithinLink(CVehicle& wreck, CPathNode* pFirstNode, CPathNode* pSecondNode)
{
	const CPathNodeLink* pNodeLink = ThePaths.FindLinkBetween2Nodes(pFirstNode, pSecondNode);
	if(!pNodeLink)
	{
		return false;
	}

	float fLeft, fRight;
	//only care about lanes going from this link
	ThePaths.FindRoadBoundaries(pNodeLink->m_1.m_LanesToOtherNode, 0, static_cast<float>(pNodeLink->m_1.m_Width), pNodeLink->m_1.m_NarrowRoad, false, fLeft, fRight);

	//calculate total road width at this point
	Vector3 nodePos;
	pFirstNode->GetCoors(nodePos);
	Vector3 nextNodePos;
	pSecondNode->GetCoors(nextNodePos);

	//get side vector
	Vector2 side = Vector2(nextNodePos.y -  nodePos.y,  -(nextNodePos.x - nodePos.x));
	side.Normalize();

	//get segment start end pos
	Vector3 rightVec = Vector3((side.x * fRight),(side.y * fRight), 0.0f);
	Vector3 leftVec = Vector3((-side.x * fLeft),(-side.y * fLeft), 0.0f);
	Vector3 leftLanePos = leftVec + nodePos;
	Vector3 nextLeftLanePos = leftVec + nextNodePos;
	Vector3 rightLanePos = rightVec + nodePos;
	Vector3 nextRightLanePos = rightVec + nextNodePos;

	//create clockwise poly of segment
	Vector3 poly[4];
	poly[0] = Vector3(leftLanePos);
	poly[1] = Vector3(rightLanePos);
	poly[2] = Vector3(nextRightLanePos);
	poly[3] = Vector3(nextLeftLanePos);

// 	grcDebugDraw::Line(poly[0], poly[1], Color_red,60);
// 	grcDebugDraw::Line(poly[1], poly[2], Color_green,60);
// 	grcDebugDraw::Line(poly[2], poly[3], Color_blue,60);
// 	grcDebugDraw::Line(poly[3], poly[0], Color_yellow,60);

	float wreckX = wreck.GetTransform().GetPosition().GetXf();
	float wreckY = wreck.GetTransform().GetPosition().GetYf();

	//check if point is within poly
	bool bIsWithin = false;
	for (int vertI = 0, vertJ = 3; vertI < 4; vertJ = vertI++) {
		if ( ((poly[vertI].y > wreckY ) != (poly[vertJ].y > wreckY)) &&
			(wreckX < (poly[vertJ].x - poly[vertI].x) * (wreckY - poly[vertI].y) / (poly[vertJ].y - poly[vertI].y) + poly[vertI].x) )
			bIsWithin = !bIsWithin;
	}

	return bIsWithin;
}

//checks if we've got a lot of wrecks ahead of us on a highway that block our route
bool CTaskVehicleFlee::ShouldWaitForAccident(CVehicle* pVehicle, Vec3V_InOut wreckCenter, int& numWrecks)
{
	wreckCenter.ZeroComponents();
	numWrecks = 0;

	//law cars don't wait
	if(pVehicle->IsLawEnforcementCar())
	{
		return false;
	}

	TUNE_GROUP_BOOL(FLEE_WAIT, DoStopAndWait, true);
	if(!DoStopAndWait)
	{
		return false;
	}

	TUNE_GROUP_BOOL(FLEE_WAIT, OnlyWaitOnHighWays, true);
	TUNE_GROUP_INT(FLEE_WAIT, NumVehiclesForCheck, 8, 0, 20, 1);
	TUNE_GROUP_INT(FLEE_WAIT, NumBlockingForStop, 5, 0, 20, 1);
	TUNE_GROUP_FLOAT(FLEE_WAIT, DistToStopAt, 75.0f, 0.0f, 200.0f, 1.0f);
	TUNE_GROUP_FLOAT(FLEE_WAIT, DistOffLine, 25.0f, 0.0f, 200.0f, 1.0f);

	//only do on highways
	const CVehicleFollowRouteHelper* pFollowRouteHelper = pVehicle->GetIntelligence()->GetFollowRouteHelper();
	if(OnlyWaitOnHighWays && pFollowRouteHelper && pFollowRouteHelper->GetCurrentTargetPoint() >= 0 && 
		pFollowRouteHelper->GetCurrentTargetPoint() < pFollowRouteHelper->GetNumPoints())
	{
		const CRoutePoint& routePoint = pFollowRouteHelper->GetRoutePoints()[pFollowRouteHelper->GetCurrentTargetPoint()];
		if(!routePoint.GetNodeAddress().IsEmpty())
		{
			const CPathNode* pPathNode = ThePaths.FindNodePointerSafe(routePoint.GetNodeAddress());
			if(!pPathNode || !pPathNode->IsHighway())
			{
				return false;
			}
		}
	}

	//find all wrecks ahead of us
	if(CVehiclePopulation::GetTotalEmptyWreckedVehsOnMap() >= NumVehiclesForCheck)
	{
		atArray<CVehicle*> wreckedVehicles;
		wreckedVehicles.Reserve(NumVehiclesForCheck);

		Vec3V vVehPos = pVehicle->GetTransform().GetPosition();
		CEntityScannerIterator vehicleList = pVehicle->GetIntelligence()->GetVehicleScanner().GetIterator();
		for( CEntity* pEntity = vehicleList.GetFirst(); pEntity; pEntity = vehicleList.GetNext() )
		{
			CVehicle* pOtherVehicle = (CVehicle*) pEntity;
			bool bNoDriver = !pOtherVehicle->IsUsingPretendOccupants() && !pOtherVehicle->GetDriver();
			bool bDeadDriver = pOtherVehicle->GetDriver() && pOtherVehicle->GetDriver()->IsDead();
			
			if(!pOtherVehicle->m_nVehicleFlags.bIsThisAParkedCar &&	(pOtherVehicle->GetStatus() == STATUS_WRECKED || bNoDriver || bDeadDriver))
			{
				Vec3V vToOther = Normalize(pOtherVehicle->GetTransform().GetPosition() - vVehPos);
				if(IsGreaterThanAll(Dot(vToOther, pVehicle->GetTransform().GetForward()), ScalarV(V_ZERO)))
				{
					wreckCenter = Add(wreckCenter, pOtherVehicle->GetTransform().GetPosition());
					wreckedVehicles.PushAndGrow(pOtherVehicle);
					++numWrecks;
				}
			}
		}

		wreckCenter = InvScale(wreckCenter, ScalarV((float)numWrecks));

		//check all the valid wrecks to see if we should stop
		if(numWrecks > NumBlockingForStop)
		{
			Vec3V vVehPos = pVehicle->GetTransform().GetPosition();

			Vec3V vToWreckCenter = wreckCenter - vVehPos;
			bool bHeadingTowards = IsGreaterThanAll(Dot(vToWreckCenter, pVehicle->GetTransform().GetForward()), ScalarV(V_ZERO)) != 0;

			ScalarV fDistToWreck = DistSquared(wreckCenter, vVehPos);
			float fDriverModifer = CDriverPersonality::FindDriverAggressiveness(pVehicle->GetDriver(), pVehicle) * 10.0f;
			float fDriverDistToStopSqr = DistToStopAt*DistToStopAt + fDriverModifer*fDriverModifer;
			if(bHeadingTowards && IsLessThanAll(fDistToWreck, ScalarV(fDriverDistToStopSqr)))
			{
				const CVehicleFollowRouteHelper* pFollowRouteHelper = pVehicle->GetIntelligence()->GetFollowRouteHelper();
				if(pFollowRouteHelper)
				{
					int iTargetPoint = pFollowRouteHelper->GetCurrentTargetPoint();
					for(int i = iTargetPoint; i < pFollowRouteHelper->GetNumPoints() - 1; ++i)
					{
						CPathNode* pNextNode = ThePaths.FindNodePointerSafe(pFollowRouteHelper->GetRoutePoints()[i].GetNodeAddress());
						CPathNode* pNextNextNode = ThePaths.FindNodePointerSafe(pFollowRouteHelper->GetRoutePoints()[i+1].GetNodeAddress());

						if(!pNextNode || !pNextNextNode)
						{
							return false;
						}

						for(int j = 0; j < numWrecks; ++j)
						{
							//is the wreck inside this link
							if(IsEntityWithinLink(*wreckedVehicles[j], pNextNode, pNextNextNode))
							{
								return true;
							}
						}			
					}
				}	
			}
		}
	}

	return false;
}

aiTask::FSM_Return CTaskVehicleFlee::StopAndWait_OnUpdate(CVehicle* pVehicle)
{
	if (GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_STOP))
	{
		SetNewTask(rage_new CTaskVehicleStop(CTaskVehicleStop::SF_DontTerminateWhenStopped));
	}

	//determine if we should start moving again
	int iNumWrecks = 0;
	Vec3V wreckCenter(V_ZERO);
	m_fStopRecheckTimer -= fwTimer::GetTimeStep();

	//reduce timer twice as fast so not all cars start again at same time
	if(!ShouldWaitForAccident(pVehicle, wreckCenter, iNumWrecks))
	{
		m_fStopRecheckTimer -= fwTimer::GetTimeStep();
	}

	if(m_fStopRecheckTimer <= 0.0f)
	{	
		SetState(State_Flee);
		return FSM_Continue;
	}

#if __BANK
	TUNE_GROUP_BOOL(FLEE_WAIT, RenderWaitForFlee, false);
	if(iNumWrecks > 0 && RenderWaitForFlee)
	{
		grcDebugDraw::Line(pVehicle->GetTransform().GetPosition(), wreckCenter, Color_red2);
		grcDebugDraw::Sphere(wreckCenter, 1.0f, Color_red2, true);
	}
#endif

	return FSM_Continue; 
}

void CTaskVehicleFlee::StopAndWait_OnExit(CVehicle* UNUSED_PARAM(pVehicle))
{
	//large delay before stopping again
	m_fStopRecheckTimer = fwRandom::GetRandomNumberInRange(10.0f, 20.0f);
}

/////////////////////////////////////////////////////////////////////////////////////

bool CTaskVehicleFlee::CheckShouldSkidToStop()
{
	//Ensure the skid to stop flag has been set.
	if(!m_uConfigFlags.IsFlagSet(CF_SkidToStop))
	{
		return false;
	}
	
	//Ensure we have not skidded to a stop.
	if(m_uRunningFlags.IsFlagSet(VFRF_HasSkiddedToStop))
	{
		return false;
	}
	
	//Grab the vehicle.
	const CVehicle* pVehicle = GetVehicle();
	
	//Ensure the follow route is valid.
	const CVehicleFollowRouteHelper* pFollowRouteHelper = pVehicle->GetIntelligence()->GetFollowRouteHelper();
	if(!pFollowRouteHelper)
	{
		return false;
	}
	
	//Ensure the number of points is valid.
	s16 iNumPoints = pFollowRouteHelper->GetNumPoints();
	if(iNumPoints <= 0)
	{
		return false;
	}
	
	//Ensure the current target point is valid.
	s16 iCurrentTargetPoint = pFollowRouteHelper->GetCurrentTargetPoint();
	if(iCurrentTargetPoint < 0 || iCurrentTargetPoint >= iNumPoints)
	{
		return false;
	}
	
	//Grab the route point.
	const CRoutePoint& rRoutePoint = pFollowRouteHelper->GetRoutePoints()[iCurrentTargetPoint];
	
	//Grab the route point position.
	Vec3V vRoutePointPosition = rRoutePoint.GetPosition();
	
	//Grab the vehicle position.
	Vec3V vVehiclePosition = pVehicle->GetVehiclePosition();
	
	//Generate a vector from the vehicle to the route point.
	Vec3V vVehicleToRoutePoint = Subtract(vRoutePointPosition, vVehiclePosition);
	vVehicleToRoutePoint.SetZ(ScalarV(V_ZERO));
	
	//Grab the vehicle's right vector.
	Vec3V vVehicleRight = pVehicle->GetVehicleRightDirection();
	vVehicleRight.SetZ(ScalarV(V_ZERO));
	
	//Check if the route point is to the right of the vehicle.
	ScalarV scDot = Dot(vVehicleRight, vVehicleToRoutePoint);
	bool bOnRight = (IsGreaterThanAll(scDot, ScalarV(V_ZERO)) != 0);
	
	//Generate the skid direction.
	if(bOnRight)
	{
		m_uRunningFlags.SetFlag(VFRF_SkidRight);
	}
	else
	{
		m_uRunningFlags.SetFlag(VFRF_SkidLeft);
	}
	
	return true;
}

void CTaskVehicleFlee::HonkHorn()
{
	// Grab the vehicle.
	CVehicle* pVehicle = GetVehicle();

	// Honk!
	pVehicle->PlayCarHorn(true, ATSTRINGHASH("AGGRESSIVE",0xC91D8B07));
}

bool CTaskVehicleFlee::IsTargetGettingUp() const
{
	//Ensure the target is valid.
	const CEntity* pEntity = GetTargetEntity();
	if(!pEntity)
	{
		return false;
	}

	//Ensure the entity is a ped.
	if(!pEntity->GetIsTypePed())
	{
		return false;
	}

	//Ensure the ped is not dead.
	const CPed* pPed = static_cast<const CPed *>(pEntity);
	if(pPed->IsInjured())
	{
		return false;
	}

	//Check if the ped is using a ragdoll.
	if(pPed->GetUsingRagdoll())
	{
		return true;
	}

	//Check if the ped is getting up.
	if(pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_GET_UP, true))
	{
		return true;
	}

	return false;
}

void CTaskVehicleFlee::ResetStatics()
{
	sm_uLastHonkTime = 0;
}

bool CTaskVehicleFlee::ShouldHonkHorn() const
{
	// Check if it has been long enough since the last time someone honked while fleeing.
	if (CTimeHelpers::GetTimeSince(sm_uLastHonkTime) < sm_uTimeBetweenHonks)
	{
		return false;
	}

	// Grab the vehicle.
	const CVehicle* pVehicle = GetVehicle();

	// Check if this is a model that honks while fleeing.
	if (!pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_CAN_HONK_WHEN_FLEEING))
	{
		return false;
	}
	
	// Find the target position.
	Vec3V vTargetPosition;
	FindTargetCoors(pVehicle, RC_VECTOR3(vTargetPosition));

	// Check if the threat is in front.
	Vec3V vToTarget = Subtract(vTargetPosition, pVehicle->GetTransform().GetPosition());
	if (IsLessThanAll(Dot(pVehicle->GetTransform().GetForward(), vToTarget), ScalarV(V_ZERO)))
	{
		return false;
	}

	// Honking is approved.
	return true;
}

bool CTaskVehicleFlee::ShouldJitter() const
{
	//Grab the vehicle.
	const CVehicle* pVehicle = GetVehicle();

	//Grab the vehicle position.
	Vec3V vVehiclePosition = pVehicle->GetTransform().GetPosition();

	//Ensure the chances exceed the threshold.
	float fRandom = fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
	if(fRandom > sm_Tunables.m_ChancesForJitter)
	{
		return false;
	}

	//Find the target position.
	Vec3V vTargetPosition;
	FindTargetCoors(pVehicle, RC_VECTOR3(vTargetPosition));

	//Check if the target is roughly dead ahead
	Vec3V vForward = pVehicle->GetTransform().GetForward();
	Vec3V vVehicleToTarget = Subtract(vTargetPosition, vVehiclePosition);
	vVehicleToTarget = Normalize(vVehicleToTarget);
	ScalarV scDot = Dot(vForward, vVehicleToTarget);
	if(IsGreaterThanAll(scDot, ScalarV(0.9f)))
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool CTaskVehicleFlee::ShouldSwerveRight() const
{
	//Grab the vehicle.
	const CVehicle* pVehicle = GetVehicle();

	//Grab the vehicle position.
	Vec3V vVehiclePosition = pVehicle->GetTransform().GetPosition();

	//Find the target position.
	Vec3V vTargetPosition;
	FindTargetCoors(pVehicle, RC_VECTOR3(vTargetPosition));

	//Check if the target is on the right.
	Vec3V vRight = pVehicle->GetTransform().GetRight();
	Vec3V vVehicleToTarget = Subtract(vTargetPosition, vVehiclePosition);
	ScalarV scDot = Dot(vRight, vVehicleToTarget);
	if(IsGreaterThanAll(scDot, ScalarV(V_ZERO)))
	{
		return false;
	}
	else
	{
		return true;
	}
}

void CTaskVehicleFlee::UpdateCruiseSpeed()
{
	//Ensure the sub-task is valid.
	CTask* pSubTask = GetSubTask();
	if(!pSubTask)
	{
		return;
	}

	//Ensure the sub-task is the right type.
	if(pSubTask->GetTaskType() != CTaskTypes::TASK_VEHICLE_CRUISE_NEW)
	{
		return;
	}

	if(!GetVehicle()->InheritsFromBicycle())
	{
		//lerp cruise speed down to normal driving speed as intensity decreases
		float fNewCruiseSpeed = m_Params.m_fCruiseSpeed;
		if(m_fMaxCruiseSpeed > 0.0f)
		{
			float fMinCruiseSpeed = rage::Min(m_fMaxCruiseSpeed, m_Params.m_fCruiseSpeed);
			fNewCruiseSpeed = RampValue(m_fFleeIntensity, 0.0f, 1.0f, fMinCruiseSpeed, m_Params.m_fCruiseSpeed);
		}

		//Set the cruise speed.
		CTaskVehicleCruiseNew* pTask = static_cast<CTaskVehicleCruiseNew *>(pSubTask);
		pTask->SetCruiseSpeed(fNewCruiseSpeed);

		return;
	}

	//Ensure the follow route helper is valid.
	const CVehicleFollowRouteHelper* pFollowRouteHelper = GetVehicle()->GetIntelligence()->GetFollowRouteHelper();
	if(!pFollowRouteHelper)
	{
		return;
	}

	//Get the number of points.
	int iNumPoints = pFollowRouteHelper->GetNumPoints();

	//Ensure the current target point is valid.
	int iCurrentTargetPoint = pFollowRouteHelper->GetCurrentTargetPoint();
	if((iCurrentTargetPoint < 0) || (iCurrentTargetPoint >= iNumPoints))
	{
		return;
	}

	//Ensure the last point is valid.
	int iLastPoint = iCurrentTargetPoint - 1;
	if((iLastPoint < 0) || (iLastPoint >= iNumPoints))
	{
		return;
	}

	//Get the route points.
	const CRoutePoint& rCurrentTargetPoint = pFollowRouteHelper->GetRoutePoints()[iCurrentTargetPoint];
	const CRoutePoint& rLastPoint = pFollowRouteHelper->GetRoutePoints()[iLastPoint];

	//Calculate the segment.
	Vec3V vSegment = Subtract(rCurrentTargetPoint.GetPosition(), rLastPoint.GetPosition());
	Vec3V vSegmentDirection = NormalizeFastSafe(vSegment, Vec3V(V_ZERO));

	//Modify the cruise speed.
	static dev_float s_fMinZ = 0.0f;
	static dev_float s_fMaxZ = 0.4f;
	static dev_float s_fMultiplierForMin = 1.0f;
	static dev_float s_fMultiplierForMax = 0.33f;
	float fZ = vSegmentDirection.GetZf();
	fZ = Clamp(fZ, s_fMinZ, s_fMaxZ);
	float fLerp = (fZ - s_fMinZ) / (s_fMaxZ - s_fMinZ);
	float fMultiplier = Lerp(fLerp, s_fMultiplierForMin, s_fMultiplierForMax);
	float fCruiseSpeed = m_Params.m_fCruiseSpeed * fMultiplier;

	//Set the cruise speed.
	CTaskVehicleCruiseNew* pTask = static_cast<CTaskVehicleCruiseNew *>(pSubTask);
	pTask->SetCruiseSpeed(fCruiseSpeed);
}

/////////////////////////////////////////////////////////////////////////////////////
#if !__FINAL
const char * CTaskVehicleFlee::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_Start&&iState<=State_StopAndWait);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_Flee",
		"State_Swerve",
		"State_Hesitate",
		"State_Cruise",
		"State_SkidToStop",
		"State_Pause",
		"State_Stop",
		"State_StopAndWait"
	};

	return aStateNames[iState];
}
#endif


CTaskVehicleCruiseNew::CTaskVehicleCruiseNew(const sVehicleMissionParams& params, const bool bSpeedComesFromVehPopulation)
: CTaskVehicleMissionBase(params)
, m_PreferredTargetNodeList(NULL)
, m_pRouteSearchHelper(NULL)
, m_pCarWeWereBehindOnLastLaneChangeAttempt(NULL)
, m_AdverseConditionsDistributer(CExpensiveProcess::VPT_AdverseConditionsDistributer)
, m_fPathLength(0.0f)
, m_bFindingNewRoute(false)
, m_fPickedCruiseSpeedWithVehModel(0.0f)
, m_fStraightLineDist(sVehicleMissionParams::DEFAULT_SWITCH_TO_STRAIGHT_LINE_DIST)
, m_iMaxPathSearchDistance(-1)
, m_uTimeLastNotDoingStraightLineNav(0)
, m_bNeedToPickCruiseSpeedWithVehModel(true)
, m_bLeftIndicator(false)
, m_bRightIndicator(false)
, m_bHasComeToStop(false)
, m_bSpeedComesFromVehPopulation(bSpeedComesFromVehPopulation)
, m_uTimeStartedUsingWanderFallback(0)
, m_uNextTimeAllowedToRevEngine(fwTimer::GetTimeInMilliseconds() + 3000)
, m_uNextTimeAllowedToReplan(0)
, m_uNextTimeAllowedToChangeLanes(0)
, m_uLastTimeUpdatedIndicators(0)
, m_nTimesFailedResettingTarget(0)
#if __ASSERT
, m_bDontAssertIfTargetInvalid(false)
#endif
{
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_CRUISE_NEW);

	m_syncedNode.SetEmpty();
	for(int i = 0 ; i < NUM_NODESSYNCED; ++i)
	{
		m_syncedLinkAndLane[i].m_linkAndLane = 0;
	}

#if __BANK
	TUNE_GROUP_BOOL(TASK_TUNE, bLogVehicleCruiseTaskCreation, false);
	if (bLogVehicleCruiseTaskCreation)
	{
		Displayf("[B*3655991] CTaskVehicleCruiseNew::CTaskVehicleCruiseNew(). Callstack:");
		sysStack::PrintStackTrace();
	}
#endif	// __BANK
}


CTaskVehicleCruiseNew::~CTaskVehicleCruiseNew()
{
	// This releases the allocated memory for the preferred target node list.
	SetPreferredTargetNodeList(NULL);

	delete m_pRouteSearchHelper;
	m_NavmeshLOSHelper.ResetTest();
}

void CTaskVehicleCruiseNew::CleanUp()
{
	if (GetVehicle())
	{
		GetVehicle()->m_nVehicleFlags.bTurningLeftAtJunction = false;
		GetVehicle()->m_nVehicleFlags.bRightIndicator = false;
		GetVehicle()->m_nVehicleFlags.bLeftIndicator = false;
	}

	// This should be done by GotoStraightLine_OnExit() and we only use
	// it in that state, but clearing it anyway in case it somehow didn't get called.
	m_followRoute.ClearStraightLineTargetPos();
}

void CTaskVehicleCruiseNew::DoSoftReset()
{
	SetState(State_FindRoute);
	m_NavmeshLOSHelper.ResetTest();
	m_bSoftResetRequested = false;
	if (m_pRouteSearchHelper)
	{
		m_pRouteSearchHelper->ResetStartNodeRejectDist();
		m_pRouteSearchHelper->Reset();
	}

	// Not sure if required, but might as well do it:
	m_bNeedToPickCruiseSpeedWithVehModel = true;
}

void CTaskVehicleCruiseNew::UpdateEmptyLaneAwareness()
{
	PF_FUNC(UpdateEmptyLaneAwareness);

	TUNE_GROUP_INT(EMPTY_LANE_AWARENESS, ELA_LANE_CHANGE_COOLDOWN, 10000, 0, 20000, 1000);
	TUNE_GROUP_FLOAT(EMPTY_LANE_AWARENESS, ELA_MINIMUM_DISTANCE_FROM_CAR_IN_FRONT, 5.0f, 0.0f, 50.0f, 0.5f);
	TUNE_GROUP_FLOAT(EMPTY_LANE_AWARENESS, ELA_MAXIMUM_DISTANCE_FROM_CAR_IN_FRONT, 10.0f, 0.0f, 50.0f, 0.5f);
	TUNE_GROUP_FLOAT(EMPTY_LANE_AWARENESS, ELA_MINIMUM_OVERTAKE_SPEED, 3.0f, 0.0f, 10.0f, 0.5f);
	TUNE_GROUP_FLOAT(EMPTY_LANE_AWARENESS, ELA_FORWARD_SEARCH_DISTANCE, 10.0f, 0.0f, 100.0f, 0.5f);	// Plus distance behind car in front of us
	TUNE_GROUP_FLOAT(EMPTY_LANE_AWARENESS, ELA_REVERSE_SEARCH_DISTANCE, 15.0f, 0.0f, 100.0f, 0.5f);

	CVehicle* pVehicle = GetVehicle();
	CVehicleIntelligence* pIntelligence = pVehicle->GetIntelligence();
	CVehicle* pCarWeAreBehind = pIntelligence->GetCarWeAreBehind();

#if __BANK
	static CVehicle* lastLaneChangeVehicle = NULL;

	if (pVehicle == g_pFocusEntity && pVehicle != lastLaneChangeVehicle)
	{
		AttemptLaneChange(ELA_MINIMUM_OVERTAKE_SPEED, ELA_FORWARD_SEARCH_DISTANCE, ELA_REVERSE_SEARCH_DISTANCE);
		lastLaneChangeVehicle = pVehicle;
		return;
	}
#endif // __BANK

	const bool bSpeedConditionsMet = pVehicle->GetAiXYSpeed() > ELA_MINIMUM_OVERTAKE_SPEED;
	const bool bTimeConditionsMet = fwTimer::GetTimeInMilliseconds() >= m_uNextTimeAllowedToChangeLanes;
	const bool bRangeConditionsMet = pCarWeAreBehind 
										&& pIntelligence->m_fDistanceBehindCarSq <= ELA_MAXIMUM_DISTANCE_FROM_CAR_IN_FRONT * ELA_MAXIMUM_DISTANCE_FROM_CAR_IN_FRONT 
										&& pIntelligence->m_fDistanceBehindCarSq > ELA_MINIMUM_DISTANCE_FROM_CAR_IN_FRONT * ELA_MINIMUM_DISTANCE_FROM_CAR_IN_FRONT;


#if __BANK
	TUNE_GROUP_BOOL(EMPTY_LANE_AWARENESS, DEBUG_EMPTY_LANE_AWARENESS, false);

	if (DEBUG_EMPTY_LANE_AWARENESS)
	{
		if (bSpeedConditionsMet)
		{
			grcDebugDraw::Sphere(pVehicle->GetVehiclePosition() + Vec3V(0.0f, 0.0f, 5.0f), 0.5f, bTimeConditionsMet ? Color_grey : Color_black, true);
		}

		if (bRangeConditionsMet)
		{
			grcDebugDraw::Sphere(pVehicle->GetVehiclePosition() + Vec3V(0.0f, 0.0f, 5.0f), 0.6f, Color_cyan, false);
		}
	}
#endif // __BANK

	if (/*pCarWeAreBehind != m_pCarWeWereBehindOnLastLaneChangeAttempt && */bRangeConditionsMet && bSpeedConditionsMet && bTimeConditionsMet)
	{
		const Vec3V vForward = pVehicle->GetVehicleForwardDirection();
		const Vec3V vToCarWeAreBehind = pCarWeAreBehind->GetVehiclePosition() - pVehicle->GetVehiclePosition();
		const ScalarV fDistanceBehindCar = Dot(vForward, vToCarWeAreBehind);

		if (AttemptLaneChange(ELA_MINIMUM_OVERTAKE_SPEED, ELA_FORWARD_SEARCH_DISTANCE + fDistanceBehindCar.Getf(), ELA_REVERSE_SEARCH_DISTANCE))
		{
			m_uNextTimeAllowedToChangeLanes = fwTimer::GetTimeInMilliseconds() + ELA_LANE_CHANGE_COOLDOWN;
		}
	}
}

extern bool HelperIsAdjacentLaneOpen(CVehicle* vehicle, const float forwardSearchDistance, const float reverseSearchDistance, s32& outLane BANK_ONLY(, bool useDebugDraw = false));

bool CTaskVehicleCruiseNew::AttemptLaneChange(const float fMinimumOvertakeSpeed, const float fForwardSearchDistance, const float fReverseSearchDistance)
{
	CVehicle* pVehicle = GetVehicle();
	CVehicleIntelligence* pIntelligence = pVehicle->GetIntelligence();

#if __BANK
	TUNE_GROUP_BOOL(EMPTY_LANE_AWARENESS, DEBUG_IS_ADJACENT_LANE_OPEN, false);
	TUNE_GROUP_BOOL(EMPTY_LANE_AWARENESS, DEBUG_IS_ADJACENT_LANE_OPEN_RESULT, false);
#endif

	if (pVehicle->PopTypeGet() == POPTYPE_RANDOM_AMBIENT
		&& !pVehicle->m_nVehicleFlags.bIsBig
		&& pIntelligence->GetCarWeAreBehind()
		&& pIntelligence->GetCarWeAreBehind()->GetAiXYSpeed() <= fMinimumOvertakeSpeed)
	{
		s32 iLane;
		const bool bAdjacentLaneOpen = HelperIsAdjacentLaneOpen(pVehicle, fForwardSearchDistance, fReverseSearchDistance, iLane BANK_ONLY(, DEBUG_IS_ADJACENT_LANE_OPEN));

		m_pCarWeWereBehindOnLastLaneChangeAttempt = pIntelligence->GetCarWeAreBehind();
		
#if __BANK
		if (DEBUG_IS_ADJACENT_LANE_OPEN || DEBUG_IS_ADJACENT_LANE_OPEN_RESULT)
		{
			grcDebugDraw::Sphere(pVehicle->GetVehiclePosition(), 2.0f, bAdjacentLaneOpen ? Color_green : Color_red, false, 30);
		}
#endif // __BANK

		if (bAdjacentLaneOpen)
		{
			RequestImmediateLaneChange(iLane, false, false);

			return true;
		}
	}

	return false;
}

bool CTaskVehicleCruiseNew::RequestImmediateLaneChange(int iLane, bool bReverseFirst, bool bFailIfLaneAlreadyReserved)
{
	const int iTargetPoint = m_followRoute.GetCurrentTargetPoint();
	if (iTargetPoint >= 0 && iTargetPoint < m_followRoute.GetNumPoints())
	{
		if (bFailIfLaneAlreadyReserved && m_followRoute.GetRequiredLaneIndex(iTargetPoint) >= 0)
		{
			return false;
		}

		m_pRouteSearchHelper->GetNodeList().SetPathLaneIndex(m_pRouteSearchHelper->GetNodeList().GetTargetNodeIndex(), (s8)iLane);
		m_pRouteSearchHelper->GetNodeList().SetPathLaneIndex(rage::Max(0, m_pRouteSearchHelper->GetNodeList().GetTargetNodeIndex()-1), (s8)iLane);

		m_pRouteSearchHelper->QuickReplan(iLane);

		const bool bDoingWanderRoute = (GetDefaultPathSearchFlags()&CPathNodeRouteSearchHelper::Flag_WanderRoute) != 0;
		const bool bShouldUpdateLanesForTurns = bDoingWanderRoute || m_pRouteSearchHelper->ShouldAvoidTurns();
		m_followRoute.ConstructFromNodeList(GetVehicle(), m_pRouteSearchHelper->GetNodeList(), bShouldUpdateLanesForTurns, &m_LaneChangeHelper, IsDrivingFlagSet(DF_UseStringPullingAtJunctions));

		m_followRoute.SetRequiredLaneIndex(iTargetPoint, iLane);
		GetVehicle()->GetIntelligence()->SetHasFixedUpPathForCurrentJunction(false);

		CTaskVehicleGoToPointWithAvoidanceAutomobile* pAvoidanceSubtask = 
			static_cast<CTaskVehicleGoToPointWithAvoidanceAutomobile*>(FindSubTaskOfType(CTaskTypes::TASK_VEHICLE_GOTO_POINT_WITH_AVOIDANCE_AUTOMOBILE));

		if (pAvoidanceSubtask)
		{
			pAvoidanceSubtask->RequestOvertakeCurrentCar(bReverseFirst);
		}

		return true;
	}

	return false;
}

aiTask::FSM_Return CTaskVehicleCruiseNew::ProcessPreFSM_Shared()
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.
	if (pVehicle)
	{	
		//don't allow dummying in straight line mode, as we can be tasked to go anywhere and will just clip through any objects in the way
		if(	GetState() != State_GoToStraightLine)
		{
			pVehicle->m_nVehicleFlags.bTasksAllowDummying = true;
		}
				
		if (IsDrivingFlagSet(DF_DriveIntoOncomingTraffic))
		{
			//fleeing vehicles are always dirty
			pVehicle->m_nVehicleFlags.bAvoidanceDirtyFlag = true;
		}

		if (!aiVerifyf(!pVehicle->InheritsFromTrailer(), "Trailer running a CRUISE or GOTO task, this is not supported!"))
		{
			return FSM_Quit;
		}
	}

	EnsureHasRouteSearchHelper();

	// Only humanise (limit and cap) the controls if we are trying to travel relatively slowly
	if( GetCruiseSpeed() < 20.0f )
	{
		pVehicle->GetIntelligence()->SetHumaniseControls(true);
	}

	UpdateIndicatorsForCar(pVehicle);

	if (GetState() != State_GetToRoadUsingNavmesh)
	{
		m_uRunningFlags.ClearFlag(RF_IsUnableToGetToRoad);
	}

	return FSM_Continue;
}

void CTaskVehicleCruiseNew::UpdateSpeedMultiplier(CVehicle* pVehicle) const
{
	CVehicleNodeList* pNodeList = &m_pRouteSearchHelper->GetNodeList();

	const int iTargetNode = pNodeList->GetTargetNodeIndex();

	if(pNodeList->GetPathNodeAddr(iTargetNode).IsEmpty())
	{
		pVehicle->GetIntelligence()->SpeedFromNodes = 1;
	}
	else if(!ThePaths.IsRegionLoaded(pNodeList->GetPathNodeAddr(iTargetNode)))
	{
		// This can happen(Especially in network games)
		pVehicle->GetIntelligence()->SpeedFromNodes = 0;
	}
	else
	{
		pVehicle->GetIntelligence()->SpeedFromNodes = ThePaths.FindNodePointer(pNodeList->GetPathNodeAddr(iTargetNode))->m_2.m_speed;	// Store the speed so that the car can speed up on fast roads
	}

	// Calculate the speed multiplier that is used to multiply the cruisespeed so that we get a realistic speed for fast roads.
	float DesiredMultiplier = 1.0f;
	DesiredMultiplier = CVehicleIntelligence::FindSpeedMultiplierWithSpeedFromNodes(pVehicle->GetIntelligence()->SpeedFromNodes);

	if (DesiredMultiplier < pVehicle->GetIntelligence()->SpeedMultiplier)
	{		// We want to slow down.
		float MaxChange = GetTimeStep() * 0.3f;

		pVehicle->GetIntelligence()->SpeedMultiplier -= MaxChange;
		pVehicle->GetIntelligence()->SpeedMultiplier = rage::Max(pVehicle->GetIntelligence()->SpeedMultiplier, DesiredMultiplier);
	}
	else
	{		// We want to speed up.
		float MaxChange = GetTimeStep() * 0.2f;

		pVehicle->GetIntelligence()->SpeedMultiplier += MaxChange;
		pVehicle->GetIntelligence()->SpeedMultiplier = rage::Min(pVehicle->GetIntelligence()->SpeedMultiplier, DesiredMultiplier);
	}
}

aiTask* CTaskVehicleCruiseNew::Copy() const
{
	CTaskVehicleCruiseNew* pNewTask = rage_new CTaskVehicleCruiseNew(m_Params);
	if(pNewTask)
	{
		pNewTask->SetMaxPathSearchDistance(GetMaxPathSearchDistance());
		pNewTask->SetPreferredTargetNodeList(m_PreferredTargetNodeList);
	}
	return pNewTask;
}


aiTask::FSM_Return CTaskVehicleCruiseNew::ProcessPreFSM()
{
	//NOTE:
	//CTaskVehicleGoToAutomobileNew::ProcessPreFSM doesn't call into this function anymore,
	//so anything that you want to apply to both Cruise and Goto should go in CTaskVehicleCruiseNew::ProcessPreFSM_Shared
	if (ProcessPreFSM_Shared() == FSM_Quit)
	{
		return FSM_Quit;
	}

	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.

	UpdateSpeedMultiplier(pVehicle);

	//update our desired cruise speed in case personality has changed
	const CPed* pDriver = pVehicle->GetDriver();
	if (m_bSpeedComesFromVehPopulation && (GetParent() == NULL || GetParent()->GetTaskType() != CTaskTypes::TASK_VEHICLE_FLEE) )
	{
		// If we haven't already called PickCruiseSpeedWithVehModel(), do so now and store the result,
		// so we can use it together with the driver speed to determine the cruise speed.
		if(m_bNeedToPickCruiseSpeedWithVehModel)
		{
			m_fPickedCruiseSpeedWithVehModel = CVehiclePopulation::PickCruiseSpeedWithVehModel(pVehicle, pVehicle->GetModelIndex());
			m_bNeedToPickCruiseSpeedWithVehModel = false;
		}

		SetCruiseSpeed(rage::Min(m_fPickedCruiseSpeedWithVehModel
			, CDriverPersonality::FindMaxCruiseSpeed(pDriver, pVehicle, pVehicle->GetVehicleType() == VEHICLE_TYPE_BICYCLE)));
	}

	return FSM_Continue;
}

aiTask::FSM_Return CTaskVehicleCruiseNew::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	if (iEvent == OnUpdate && m_bSoftResetRequested)
	{
		DoSoftReset();
		return FSM_Continue;
	}

	if (GetState() != State_GoToStraightLine)
	{
		m_uTimeLastNotDoingStraightLineNav = fwTimer::GetGameTimer().GetTimeInMilliseconds();
	}

	GetVehicle()->m_nVehicleFlags.bTurningLeftAtJunction = false;

	FSM_Begin
		// FindRoute state
		FSM_State(State_FindRoute)
			FSM_OnEnter
				return FindRoute_OnEnter();
			FSM_OnUpdate
				return FindRoute_OnUpdate();
		
		// Cruise state
		FSM_State(State_Cruise)
			FSM_OnEnter
				return Cruise_OnEnter();
			FSM_OnUpdate
				return Cruise_OnUpdate();
			FSM_OnExit
				return Cruise_OnExit();

		//Stop state
		FSM_State(State_StopForJunction)
			FSM_OnEnter
				return StopForJunction_OnEnter();
			FSM_OnUpdate
				return StopForJunction_OnUpdate();

		//Wait state
		FSM_State(State_WaitBeforeMovingAgain)
			FSM_OnEnter
				return WaitBeforeMovingAgain_OnEnter();
			FSM_OnUpdate
				return WaitBeforeMovingAgain_OnUpdate();

		//Goto Straight Line state
		FSM_State(State_GoToStraightLine)
			FSM_OnEnter
				return GotoStraightLine_OnEnter();
			FSM_OnUpdate
				return GotoStraightLine_OnUpdate();
			FSM_OnExit
				return GotoStraightLine_OnExit();

		FSM_State(State_GoToNavmesh)
			FSM_OnEnter
				return GotoNavmesh_OnEnter();
			FSM_OnUpdate
				return GotoNavmesh_OnUpdate();
			FSM_OnExit
				return GotoNavmesh_OnExit();

		FSM_State(State_GetToRoadUsingNavmesh)
			FSM_OnEnter
				return GetToRoadUsingNavmesh_OnEnter();
			FSM_OnUpdate
				return GetToRoadUsingNavmesh_OnUpdate();
			FSM_OnExit
				return GetToRoadUsingNavmesh_OnExit();

		FSM_State(State_Burnout)
			FSM_OnEnter
				return Burnout_OnEnter();
			FSM_OnUpdate
				return Burnout_OnUpdate();

		FSM_State(State_RevEngine)
			FSM_OnEnter
				return RevEngine_OnEnter();
			FSM_OnUpdate
				return RevEngine_OnUpdate();
			FSM_OnExit
				return RevEngine_OnExit();

		// PauseForUnloadedNodes state
		FSM_State(State_PauseForUnloadedNodes)
			FSM_OnEnter
				PauseForUnloadedNodes_OnEnter();
			FSM_OnUpdate
				return PauseForUnloadedNodes_OnUpdate();

		// Finish state
		FSM_State(State_Stop)
			FSM_OnEnter
				return Stop_OnEnter();
			FSM_OnUpdate
				return Stop_OnUpdate();

	FSM_End
}

bool CTaskVehicleCruiseNew::IsEscorting() const
{
	if (!GetParent())
	{
		return false;
	}

	if(GetVehicle()->GetDriver() && GetVehicle()->GetDriver()->GetPedResetFlag(CPED_RESET_FLAG_IsInVehicleChase))
	{
		return true;
	}

	return FindParentTaskOfType(CTaskTypes::TASK_VEHICLE_ESCORT) != NULL
		|| FindParentTaskOfType(CTaskTypes::TASK_VEHICLE_FOLLOW) != NULL;
}

bool CTaskVehicleCruiseNew::DoesPathContainUTurn() const
{
	return m_pRouteSearchHelper ? m_pRouteSearchHelper->GetNodeList().IsTargetNodeAfterUTurn() : false;
}

bool CTaskVehicleCruiseNew::CanUseNavmeshToReachPoint(const CVehicle* pVehicle, const Vector3* pvAltStartCoords/* = NULL*/, const Vector3* pvAltEndCoords/*=NULL*/) const
{
	//quick reject: boats shouldn't use the navmesh ever
	if (pVehicle->InheritsFromBoat())
	{
		return false;
	}

	Vector3 vStartCoords = pvAltStartCoords ? *pvAltStartCoords : VEC3V_TO_VECTOR3(pVehicle->GetVehiclePosition());
	if (!CPathServer::IsNavMeshLoadedAtPosition(vStartCoords, kNavDomainRegular))
	{
		return false;
	}

	Vector3 vTargetCoords;
	if (pvAltEndCoords)
	{
		vTargetCoords = *pvAltEndCoords;
	}
	else
	{
		FindTargetCoors(pVehicle, vTargetCoords);
	}

	if (!CPathServer::IsNavMeshLoadedAtPosition(vTargetCoords, kNavDomainRegular))
	{
			return false;
		}

	return true;
}

bool CTaskVehicleCruiseNew::UpdateNavmeshLOSTestForCruising(const Vector3& vStartPos, const bool bForceLOSTestThisFrame)
{
	CVehicle* pVehicle = GetVehicle();

	//if we want to prefer navmesh navigation, don't even kick off the probe, just jump state
	const bool bDoingWanderRoute = (GetDefaultPathSearchFlags()&CPathNodeRouteSearchHelper::Flag_WanderRoute) != 0;
	if (!bDoingWanderRoute && IsDrivingFlagSet(DF_PreferNavmeshRoute))
	{
		const bool bHasNavmesh = CanUseNavmeshToReachPoint(pVehicle, &vStartPos);
		if (bHasNavmesh)
		{
			m_NavmeshLOSHelper.ResetTest();	//just in case
			SetState(State_GoToNavmesh);
			return true;
		}	
	}

	//if we're off road, and don't have navmesh LOS to our target point,
	//navigate using the navmesh
	if (ms_bAllowCruiseTaskUseNavMesh)
	{
		bool bOffRoad = false;

		const bool bShouldTestNavmeshThisFrame = bForceLOSTestThisFrame || 
			(pVehicle->GetIntelligence()->GetNavmeshLOSDistributer().IsRegistered() 
			&& pVehicle->GetIntelligence()->GetNavmeshLOSDistributer().ShouldBeProcessedThisFrame());

		CVehicleNodeList& NodeList = m_pRouteSearchHelper->GetNodeList();
		if (bShouldTestNavmeshThisFrame)
		{
			bOffRoad = ThePaths.DoesVehicleThinkItsOffRoad(pVehicle, IsDrivingFlagSet(DF_DriveInReverse), &NodeList);
		}

// 		if (bOffRoad)
// 		{
// 			pVehicle->ms_debugDraw.AddLine(pVehicle->GetVehiclePosition(), pVehicle->GetVehiclePosition() + Vec3V(0.0f, 0.0f, 2.0f), Color_orange);
// 		}

		//Check if a LOS NavMesh test is active.
		//Always try to handle an active test, even if we aren't allowed to issue a new one this frame
		if(m_NavmeshLOSHelper.IsTestActive())
		{
			//Get the results.
			SearchStatus searchResult;
			bool bLosIsClear;
			bool bNoSurfaceAtCoords;
			if(m_NavmeshLOSHelper.GetTestResults(searchResult, bLosIsClear, bNoSurfaceAtCoords, 1, NULL) && (searchResult != SS_StillSearching))
			{
				//Check if the line of sight is clear.
				if(!bLosIsClear && !bNoSurfaceAtCoords) 
				{
					SetState(State_GetToRoadUsingNavmesh);
					return true;
				}
			}
		}
		else if (bOffRoad)
		{
			Vector3 vEnd(vStartPos);
			const int iTargetNode = NodeList.GetTargetNodeIndex();
			const CPathNode* pNode = NodeList.GetPathNode(iTargetNode);
			const CPathNode* pPrevNode = NULL;
			if (iTargetNode > 0)
			{
				pPrevNode = NodeList.GetPathNode(iTargetNode - 1);
			}

			//if we don't have a valid pNode here, vStartPos and vEnd will be the same, causing the los test to succeed
			//this should be fine--when I debugged this the nodelist was valid, but the region for the target node
			//not loaded.
			//if we're in a region with no pathnodes loaded, just let the navmesh test suceed or not get issued
			if (pNode && pPrevNode)
			{
				CPathNodeLink* pLink = ThePaths.FindLinkPointerSafe(NodeList.GetPathNodeAddr(iTargetNode-1).GetRegion(),NodeList.GetPathLinkIndex(iTargetNode-1));
				if (pLink)
				{
					Vector3 vPrevPos, vTargetPos;
					CPathNodeRouteSearchHelper::GetZerothLanePositionsForNodesWithLink(pPrevNode, pNode, pLink, vPrevPos, vTargetPos);
					vEnd = vTargetPos;

					//grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(vStartPos), VECTOR3_TO_VEC3V(vTargetPos), 0.5f, Color_yellow, 45);
				}
			}
#if __ASSERT && 0
			else
			{
				for (s32 i = 0; i < CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED; i++)
				{
					Displayf("aNodes[%d]=%d:%d aLinks[%d]=%d\n", i, NodeList.GetPathNodeAddr(i).GetRegion(), NodeList.GetPathNodeAddr(i).GetIndex(), i, NodeList.GetPathLinkIndex(i));
				}
			}
#endif
			const bool bHasNavmeshToFirstRoadNode = CanUseNavmeshToReachPoint(pVehicle, &vStartPos, &vEnd);
			const bool bShouldStartTest = bHasNavmeshToFirstRoadNode && pNode;

			if(bShouldStartTest)
			{
				const float fBoundRadiusMultiplierToUse = pVehicle->InheritsFromBike() ? CTaskVehicleGoToNavmesh::ms_fBoundRadiusMultiplierBike : CTaskVehicleGoToNavmesh::ms_fBoundRadiusMultiplier;
				m_NavmeshLOSHelper.StartLosTest(vStartPos, vEnd, pVehicle->GetBoundRadius() * fBoundRadiusMultiplierToUse
					, false, false, false, 0, NULL, CTaskVehicleGoToNavmesh::ms_fMaxNavigableAngleRadians);	
			}
		}
	}

	return false;
}

int CTaskVehicleCruiseNew::GetMinTimeToFindNewPathAfterSearch(const CVehicle* pVehicle) const 
{
	TUNE_GROUP_INT(NEW_CRUISE_TASK, siMinTimeToFindNewPathAmbient, 1000, 0, 100000, 1);
	TUNE_GROUP_INT(NEW_CRUISE_TASK, siMinTimeToFindNewPathMission, 500, 0, 100000, 1);

	const int iRet = (pVehicle->PopTypeIsMission() 
		|| pVehicle->IsLawEnforcementVehicle() 
		|| pVehicle->HasMissionCharsInIt()) 
		? siMinTimeToFindNewPathMission
		: siMinTimeToFindNewPathAmbient;

	return iRet;
}

void CTaskVehicleCruiseNew::SetNextTimeAllowedToReplan(CVehicle* pVehicle, const s32 iOverrideTime /* = 0 */)
{
	const s32 iTimeToAdd = iOverrideTime > 0 ? iOverrideTime : GetMinTimeToFindNewPathAfterSearch(pVehicle);
	m_uNextTimeAllowedToReplan = fwTimer::GetTimeInMilliseconds() + iTimeToAdd;
}

aiTask::FSM_Return CTaskVehicleCruiseNew::FindRoute_OnEnter()
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.
	
	//Clear the route flag.
	m_uRunningFlags.ClearFlag(RF_IsUnableToFindRoute);

	u32 iSearchFlags = GetDefaultPathSearchFlags();

	//don't kick off a road network search if we want a navmesh route
	const bool bDoingWanderRoute = (iSearchFlags&CPathNodeRouteSearchHelper::Flag_WanderRoute) != 0;
	if (!bDoingWanderRoute && IsDrivingFlagSet(DF_PreferNavmeshRoute))
	{
		return FSM_Continue;
	}

	//if we're doing straight line navigation and don't want a road route, 
	//return here before we find one
	if (IsDrivingFlagSet(DF_PreventBackgroundPathfinding) && IsDrivingFlagSet(DF_ForceStraightLine))
	{
		return FSM_Continue;
	}

	UpdateUsingWanderFallback();
	AddAdditionalSearchFlags(pVehicle, iSearchFlags);

	Vector3 vTargetCoords;
	FindTargetCoors(pVehicle, vTargetCoords);

	const Vector3 vStartPos = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleBonnetPosition(pVehicle, IsDrivingFlagSet(DF_DriveInReverse)));

	const CVehicleNodeList* pTargetNodeList = GetPreferredTargetNodeList();
	if(!pTargetNodeList)
	{
		pTargetNodeList = m_Params.GetTargetEntity().GetEntity() && m_Params.GetTargetEntity().GetEntity()->GetIsTypeVehicle()
			? static_cast<const CVehicle*>(m_Params.GetTargetEntity().GetEntity())->GetIntelligence()->GetNodeList()
			: NULL;
	}	

	//Check if the vehicle is a boat.
	if(pVehicle->InheritsFromBoat())
	{
		//Don't increase the search distance when the route fails.
		//This was added because boats in the middle of the ocean were increasing their
		//search distance drastically, and finding routes to near the shore, thousands of
		//meters away... even when their target position was right in front of them.
		m_pRouteSearchHelper->SetStartNodeRejectDistMultiplier(1.0f);
	}

	if(pVehicle->HasStartNodes())
	{
		iSearchFlags |= CPathNodeRouteSearchHelper::Flag_StartNodesOverridden;
	}

	m_pRouteSearchHelper->StartSearch(vStartPos, 
		vTargetCoords, 
		VEC3V_TO_VECTOR3(pVehicle->GetVehicleForwardDirection()),
		pVehicle->GetAiVelocity(),
		iSearchFlags, pTargetNodeList);
	if(m_iMaxPathSearchDistance >= 0)
	{
		m_pRouteSearchHelper->SetMaxSearchDistance(m_iMaxPathSearchDistance);
	}

	if(pVehicle->HasStartNodes())
	{
		CNodeAddress startNodes[2];
		s16 startLink;
		s8 startLane;

		pVehicle->GetStartNodes(startNodes[0], startNodes[1], startLink, startLane);
		pVehicle->ClearStartNodes();

		EnsureHasRouteSearchHelper();
		CVehicleNodeList *nodeList = GetNodeList();

		nodeList->ClearPathNodes();
		nodeList->ClearLanes();

		nodeList->SetPathNodeAddr(0, startNodes[0]);
		nodeList->SetPathNodeAddr(1, startNodes[1]);

		nodeList->SetPathLinkIndex(0, startLink);

		nodeList->SetPathLaneIndex(0, startLane);
		nodeList->SetPathLaneIndex(1, startLane);
	}

	SetNextTimeAllowedToReplan(pVehicle);

	return FSM_Continue;
}

aiTask::FSM_Return CTaskVehicleCruiseNew::FindRoute_OnUpdate()
{
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.

	//wander routes don't support DF_PreferNavmeshRoute
	//const Vector3 vStartCoords = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleBonnetPosition(pVehicle, IsDrivingFlagSet(DF_DriveInReverse)));
	const Vector3 vStartCoordsNavMesh = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleBonnetPositionForNavmeshQueries(pVehicle, IsDrivingFlagSet(DF_DriveInReverse)));

	const bool bDoingWanderRoute = (GetDefaultPathSearchFlags()&CPathNodeRouteSearchHelper::Flag_WanderRoute) != 0;

	if (!bDoingWanderRoute && IsDrivingFlagSet(DF_PreferNavmeshRoute))
	{
		const bool bHasNavmeshAvailable = CanUseNavmeshToReachPoint(pVehicle, &vStartCoordsNavMesh);

		if (bHasNavmeshAvailable)
		{
			SetState(State_GoToNavmesh);
			return FSM_Continue;
		}
		else
		{
			SetState(State_GoToStraightLine);
			return FSM_Continue;
		}
	}

	const bool bShouldUpdateLanesForTurns = !bDoingWanderRoute || m_pRouteSearchHelper->ShouldAvoidTurns();

	if( m_pRouteSearchHelper->Update() == CTaskHelperFSM::FSM_Quit )
	{
		if( m_pRouteSearchHelper->GetState() == CPathNodeRouteSearchHelper::State_ValidRoute )
		{		
			
			m_followRoute.ConstructFromNodeList(pVehicle, m_pRouteSearchHelper->GetNodeList(), bShouldUpdateLanesForTurns, &m_LaneChangeHelper, IsDrivingFlagSet(DF_UseStringPullingAtJunctions));

			if (UpdateNavmeshLOSTestForCruising(vStartCoordsNavMesh, true))
			{
				return FSM_Continue;
			}
			else
			{
				if (IsDrivingFlagSet(DF_PreferNavmeshRoute))
				{
					//this should never happen
					SetState(State_GoToStraightLine);
					return FSM_Continue;
				}
				else
				{
					SetState(State_Cruise);
					return FSM_Continue;
				}
			}
		}
		else if (!bDoingWanderRoute && m_pRouteSearchHelper->GetState() == CPathNodeRouteSearchHelper::State_NoRoute)
		{
			//if we found zero nodes, this will reset our followroute
			m_followRoute.ConstructFromNodeList(pVehicle, m_pRouteSearchHelper->GetNodeList(), bShouldUpdateLanesForTurns, &m_LaneChangeHelper, IsDrivingFlagSet(DF_UseStringPullingAtJunctions));

			const bool bHasNavmeshAvailable = CanUseNavmeshToReachPoint(pVehicle, &vStartCoordsNavMesh);

			if (ms_bAllowCruiseTaskUseNavMesh && bHasNavmeshAvailable && !IsEscorting())
			{
				SetState(State_GoToNavmesh);
			}
			else
			{
				//still copy over the followroute anyway and calculate its length
				m_fPathLength = m_followRoute.EstimatePathLength();

				// Check if we should start to use a wander fallback rather than falling back on a straight line path.
				if(ShouldFallBackOnWanderInsteadOfStraightLine() && !m_uTimeStartedUsingWanderFallback && !m_pRouteSearchHelper->GetActuallyFoundRoute())
				{
					m_uTimeStartedUsingWanderFallback = fwTimer::GetTimeInMilliseconds();
					SetFlag(aiTaskFlags::RestartCurrentState);
				}
				else
				{
					SetState(State_GoToStraightLine);
				}
			}

			if(!m_pRouteSearchHelper->GetActuallyFoundRoute())
			{
				SetNextTimeAllowedToReplan(pVehicle, 3000);
			}
			else
			{
				SetNextTimeAllowedToReplan(pVehicle);
			}
			
			return FSM_Continue;
		}
		else
		{
			//if we found zero nodes, this will reset our followroute
			m_followRoute.ConstructFromNodeList(pVehicle, m_pRouteSearchHelper->GetNodeList(), bShouldUpdateLanesForTurns, &m_LaneChangeHelper, IsDrivingFlagSet(DF_UseStringPullingAtJunctions));

			pVehicle->GetIntelligence()->UpdateCarHasReasonToBeStopped();

			// Try again a little later
			SetState(State_PauseForUnloadedNodes);
			return FSM_Continue;
		}
	}

	return FSM_Continue;
}

bool CTaskVehicleCruiseNew::ShouldFindNewRoute(const float fDistToTheEndOfRoute) const
{
	if(m_uRunningFlags.IsFlagSet(RF_ForceRouteUpdate))
	{
		return true;
	}

	TUNE_GROUP_FLOAT(NEW_CRUISE_TASK, sfDistanceToFindNewPath, 75.0f, 0.0f, 999.0f, 0.1f);

	const bool bDoingThreePointTurn = FindSubTaskOfType(CTaskTypes::TASK_VEHICLE_THREE_POINT_TURN) != NULL;
	const bool bDoingWanderRoute = (GetDefaultPathSearchFlags()&CPathNodeRouteSearchHelper::Flag_WanderRoute) != 0;

	// Search for a new route as we reach the end of the path, but make sure we are at least 50% of the way there
	if(fwTimer::GetTimeInMilliseconds() >= m_uNextTimeAllowedToReplan && fDistToTheEndOfRoute <= rage::Min(m_fPathLength * 0.5f, sfDistanceToFindNewPath)
		&& (bDoingWanderRoute || !bDoingThreePointTurn))
	{
		return true;
	}

	//if the target's moved a significant amount, replan
	static dev_float s_fReplanIfTargetMovedThresholdSqr = 15.0f * 15.0f;
	if (!bDoingWanderRoute)
	{
		Vector3 vTargetCurrentPos;
		FindTargetCoors(GetVehicle(), vTargetCurrentPos);

		Assert(m_pRouteSearchHelper);
		Vector3 vOldDest;
		GetRouteSearchHelper()->GetTargetPosition(vOldDest);

		if (vTargetCurrentPos.IsNonZero() && vOldDest.IsNonZero()
			&& vTargetCurrentPos.Dist2(vOldDest) > s_fReplanIfTargetMovedThresholdSqr
			&& !bDoingThreePointTurn)
		{
			return true;
		}
	}

	return false;
}

void CTaskVehicleCruiseNew::AddAdditionalSearchFlags(const CVehicle* pVehicle, u32& iSearchFlags) const
{
	Assert(pVehicle);

	if (IsDrivingFlagSet(DF_AvoidTargetCoors))
	{
		iSearchFlags |= CPathNodeRouteSearchHelper::Flag_FindRouteAwayFromTarget;
	}
	if (pVehicle->GetVehicleType() == VEHICLE_TYPE_BOAT
		|| pVehicle->GetVehicleType() == VEHICLE_TYPE_SUBMARINE)
	{
		iSearchFlags |= CPathNodeRouteSearchHelper::Flag_IncludeWaterNodes;
	}

	//also avoid turns if we're fleeing and trailing something behind us, because the flee might prompt a u-turn
	if (IsDrivingFlagSet(DF_AvoidTurns) || pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_AVOID_TURNS)
		|| (IsDrivingFlagSet(DF_AvoidTargetCoors) && (pVehicle->HasTrailer())))
	{
		iSearchFlags |= CPathNodeRouteSearchHelper::Flag_AvoidTurns;
	}
	if (pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_IS_OFFROAD_VEHICLE))
	{
		iSearchFlags |= CPathNodeRouteSearchHelper::Flag_WanderOffroad;
	}
	if (IsDrivingFlagSet(DF_ExtendRouteWithWanderResults))
	{
		iSearchFlags |= CPathNodeRouteSearchHelper::Flag_ExtendPathWithResultsFromWander;
	}
	if (pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_ONLY_ON_HIGHWAYS))
	{
		iSearchFlags |= CPathNodeRouteSearchHelper::Flag_HighwaysOnly;
	}
	if (pVehicle->PopTypeIsMission() || (pVehicle->GetDriver() && pVehicle->GetDriver()->PopTypeIsMission()))
	{
		iSearchFlags |= CPathNodeRouteSearchHelper::Flag_IsMissionVehicle;
	}
	if (IsDrivingFlagSet(DF_StopAtLights))
	{
		iSearchFlags |= CPathNodeRouteSearchHelper::Flag_ObeyTrafficRules;
	}
	if (IsDrivingFlagSet(DF_AvoidRestrictedAreas))
	{
		iSearchFlags |= CPathNodeRouteSearchHelper::Flag_AvoidRestrictedAreas;
	}
	if(m_uTimeStartedUsingWanderFallback)
	{
		iSearchFlags |= CPathNodeRouteSearchHelper::Flag_WanderRoute;
	}
}

bool CTaskVehicleCruiseNew::UpdateShouldFindNewRoute(const float fDistToTheEndOfRoute, const bool bAllowStateChanges, const bool bIsOnStraightLineUpdate)
{
	CVehicle* pVehicle = GetVehicle();

	if( !m_bFindingNewRoute )
	{
		if(ShouldFindNewRoute(fDistToTheEndOfRoute))
		{
			m_bFindingNewRoute = true;
			m_uRunningFlags.ClearFlag(RF_ForceRouteUpdate);

			u32 iSearchFlags = GetDefaultPathSearchFlags();

			UpdateUsingWanderFallback();
			AddAdditionalSearchFlags(pVehicle, iSearchFlags);

			if (bIsOnStraightLineUpdate)
			{
				iSearchFlags |= CPathNodeRouteSearchHelper::Flag_OnStraightLineUpdate;
			}

			if(pVehicle->HasTrailer())
			{
				iSearchFlags |= CPathNodeRouteSearchHelper::Flag_CabAndTrailerExtendRoute;
			}
			
			//Grab the vehicle position.
			Vector3 vVehiclePosition = VEC3V_TO_VECTOR3(pVehicle->GetVehiclePosition());

			Vector3 vTargetCoords;	//high level target coords, NOT the vTargetPosition above,
			//which is our local destination on our route
			FindTargetCoors(pVehicle, vTargetCoords);
			
			//Grab the vehicle forward vector.
			Vector3 vVehicleForward = VEC3V_TO_VECTOR3(pVehicle->GetVehicleForwardDirection());
			
			//Grab the vehicle velocity.
			Vector3 vVehicleVelocity = pVehicle->GetAiVelocity();

			const CVehicleNodeList* pTargetNodeList = m_Params.GetTargetEntity().GetEntity() && m_Params.GetTargetEntity().GetEntity()->GetIsTypeVehicle()
				? static_cast<const CVehicle*>(m_Params.GetTargetEntity().GetEntity())->GetIntelligence()->GetNodeList()
				: NULL;

			{
				//first, copy all our lanes from the followroute back into the nodelist,
				//in case they've done any avoidance or anything
				const int iTargetNode = m_pRouteSearchHelper->GetNodeList().GetTargetNodeIndex();
				const bool bDoingWanderRoute = (GetDefaultPathSearchFlags()&CPathNodeRouteSearchHelper::Flag_WanderRoute) != 0;
				TUNE_GROUP_BOOL(NEW_CRUISE_TASK, bAllowFeedbackIntoNodelist, true);
				TUNE_GROUP_BOOL(NEW_CRUISE_TASK, bDisplaySpheresForReplan, false);
				if (bAllowFeedbackIntoNodelist && bDoingWanderRoute && iTargetNode >= 0 && iTargetNode < CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED)
				{

					//if we aren't where the pathfinder thought we were, clear out our path and re-plan everything
					const int iActualTargetLane = rage::round(m_followRoute.GetRoutePoints()[iTargetNode].GetLane().Getf());
					const int iTargetLane = m_pRouteSearchHelper->GetNodeList().GetPathLaneIndex(iTargetNode);
					if (iActualTargetLane != iTargetLane)
					{
						m_pRouteSearchHelper->GetNodeList().ClearFutureNodes();

#if __DEV
						if (bDisplaySpheresForReplan)
						{
							grcDebugDraw::Sphere(pVehicle->GetVehiclePosition(), 2.0f, Color_purple, false);
						}
#endif //__DEV
					}

					for (s32 i = 0; i <= iTargetNode; i++)
					{
						//don't overrun the followroute
						if (i >= m_followRoute.GetNumPoints())
						{
							break;
						}

						if (m_pRouteSearchHelper->GetNodeList().GetPathNodeAddr(i).IsEmpty())
						{
							break;
						}
						
						//don't do this if we're going backwards
						if (m_followRoute.GetRoutePoints()[i].m_iNoLanesThisSide > 0)
						{
							//the followroute's minimum lane is -0.5 (left edge of the left lane)
							//which rounds to -1.0, so max this with 0
							const int iLane = Clamp(rage::round(m_followRoute.GetRoutePoints()[i].GetLane().Getf()), 0, m_followRoute.GetRoutePoints()[i].m_iNoLanesThisSide-1);
							Assert(iLane < 127);
							m_pRouteSearchHelper->GetNodeList().SetPathLaneIndex(i, (s8)iLane);
						}
					}
				}
				

				//Extend the current route.
				if (m_followRoute.GetNumPoints() > m_followRoute.GetCurrentTargetPoint())
				{
					m_pRouteSearchHelper->StartExtendCurrentRouteSearch(
						vVehiclePosition, 
						vTargetCoords, 
						vVehicleForward,
						vVehicleVelocity,
						m_followRoute.GetCurrentTargetPoint(),
						iSearchFlags, pTargetNodeList);
				}
				else
				{
					//something went wrong, restart the search
					m_pRouteSearchHelper->StartSearch(
						vVehiclePosition, 
						vTargetCoords, 
						vVehicleForward,
						vVehicleVelocity,
						iSearchFlags, pTargetNodeList);
				}

				SetNextTimeAllowedToReplan(pVehicle);

				if(m_iMaxPathSearchDistance >= 0)
				{
					m_pRouteSearchHelper->SetMaxSearchDistance(m_iMaxPathSearchDistance);
				}
			}
		}
	}
	if( m_bFindingNewRoute )
	{
		if( m_pRouteSearchHelper->Update() == CTaskHelperFSM::FSM_Quit )
		{
			const bool bDoingWanderRoute = (GetDefaultPathSearchFlags()&CPathNodeRouteSearchHelper::Flag_WanderRoute) != 0;
			const bool bShouldUpdateLanesForTurns = !bDoingWanderRoute || m_pRouteSearchHelper->ShouldAvoidTurns();
			m_bFindingNewRoute = false;
			if( m_pRouteSearchHelper->GetState() == CPathNodeRouteSearchHelper::State_ValidRoute )
			{
				//let the rules of the lane tell us which way to go, unless we're pathfinding
				//(we already know where we want to go),
				//or we don't want to turn, in which case we know we want to go straight
				
				m_followRoute.ConstructFromNodeList(pVehicle, m_pRouteSearchHelper->GetNodeList(), bShouldUpdateLanesForTurns, &m_LaneChangeHelper, IsDrivingFlagSet(DF_UseStringPullingAtJunctions));
				SetFlag(aiTaskFlags::RestartCurrentState);
				return true;	
			}
			else if (!bDoingWanderRoute && m_pRouteSearchHelper->GetState() == CPathNodeRouteSearchHelper::State_NoRoute)
			{
				if (bAllowStateChanges)
				{
					if (ms_bAllowCruiseTaskUseNavMesh 
						&& !IsEscorting()
						&& CanUseNavmeshToReachPoint(pVehicle))
					{
						SetState(State_GoToNavmesh);
					}
					else
					{
						// Check if we should start to use a wander fallback rather than falling back on a straight line path.
						if(ShouldFallBackOnWanderInsteadOfStraightLine() && !m_uTimeStartedUsingWanderFallback && !m_pRouteSearchHelper->GetActuallyFoundRoute())
						{
							m_uTimeStartedUsingWanderFallback = fwTimer::GetTimeInMilliseconds();
							Assert(GetState() != State_FindRoute);
							SetState(State_FindRoute);
						}
						else
						{
							SetState(State_GoToStraightLine);
						}
					}
				}	

				if(!m_pRouteSearchHelper->GetActuallyFoundRoute())
				{
					SetNextTimeAllowedToReplan(pVehicle, 3000);
				}
				else
				{
					SetNextTimeAllowedToReplan(pVehicle);
				}

				m_followRoute.ConstructFromNodeList(pVehicle, m_pRouteSearchHelper->GetNodeList(), bShouldUpdateLanesForTurns, &m_LaneChangeHelper, IsDrivingFlagSet(DF_UseStringPullingAtJunctions));

				return true;
			}
			else
			{
				Assert(bDoingWanderRoute);
				Assert(m_pRouteSearchHelper->GetState() == CPathNodeRouteSearchHelper::State_NoRoute);

				if (bAllowStateChanges)
				{
					//stop then try to pathfind again
					SetState(State_PauseForUnloadedNodes);
				}

				m_followRoute.ConstructFromNodeList(pVehicle, m_pRouteSearchHelper->GetNodeList(), bShouldUpdateLanesForTurns, &m_LaneChangeHelper, IsDrivingFlagSet(DF_UseStringPullingAtJunctions));

				return true;
			}
		}

		// 		//always update if we're waiting for a new route?
		// 		if (UpdateNavmeshLOSTestForCruising(vStartCoordsNavMesh))
		// 		{
		// 			return FSM_Continue;
		// 		}
	}
	else
	{
		// Update the target node
		m_pRouteSearchHelper->SetTargetNodeIndex(m_followRoute.GetCurrentTargetPoint());
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : UpdateIndicatorsForCar
// PURPOSE :  Sets the indicator bits in the Vehicle flags if the car is planning
//			  to turn off and not on a mission.
/////////////////////////////////////////////////////////////////////////////////
void CTaskVehicleCruiseNew::UpdateIndicatorsForCar(CVehicle* pVeh)
{
// 	u32 t1 = fwTimer::GetPrevElapsedTimeInMilliseconds();
// 	u32 t2 = fwTimer::GetTimeInMilliseconds();
// 	u32 seed = pVeh->GetRandomSeed();
// 
// 	// Only half the time can the lights be on.
// 	// By ANDing with 512, we get a value that changes around twice per second.
// 	u32 masked1 = (t1 + seed) & 512;
// 	u32 masked2 = (t2 + seed) & 512;
// 
// 	if(masked1 == masked2 || !masked2)
// 	{
// 		if(masked2)
// 		{
// 			if(m_bLeftIndicator)
// 			{
// 				pVeh->m_nVehicleFlags.bLeftIndicator = true;
// 			}
// 			if(m_bRightIndicator)
// 			{
// 				pVeh->m_nVehicleFlags.bRightIndicator = true;
// 			}
// 		}
// 		return;
// 	}

	// TODO: We could probably skip all the stuff below for some drivers that don't
	// use turn signals due to their personality, as a further optimization.

	// Note: now when we have the m_bLeftIndicator/m_bRightIndicator variables
	// in the task, we could potentially chose to evaluate them less frequently
	// than the flashing of the indicators. This could easily be achieved by
	// returning here unless (t2 + seed) & 1024 or something is non-zero, but
	// making it much less frequent than that may have a noticeable effect.

	TUNE_GROUP_INT(NEW_CRUISE_TASK, TimeBetweenUpdateIndicators, 1000, 0, 10000, 1);
	if (fwTimer::GetTimeInMilliseconds() < m_uLastTimeUpdatedIndicators + TimeBetweenUpdateIndicators)
	{
		return;
	}

	m_uLastTimeUpdatedIndicators = fwTimer::GetTimeInMilliseconds();

	// Clear the indicators for the car.
	m_bLeftIndicator = m_bRightIndicator = false;
	pVeh->m_nVehicleFlags.bLeftIndicator = pVeh->m_nVehicleFlags.bRightIndicator = false;

	if(IsDrivingFlagSet(DF_StopForCars))
	{
		CVehicleNodeList * pNodeList = &m_pRouteSearchHelper->GetNodeList();
		Assertf(pNodeList, "CTaskVehicleCruiseNew::UpdateIndicatorsForCar - vehicle has no node list");
		if(!pNodeList)
		{
			return;
		}

		if (!aiVerifyf(pNodeList->GetTargetNodeIndex() < CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED, "pNodeList's target index %d outside of range. This should never happen.", pNodeList->GetTargetNodeIndex()))
		{
			return;
		}

		CNodeAddress	Node0, Node1, Node2, Node3;
		Vector3			DiffIn, DiffOut;
		const CPathNode		*pNode1, *pNode2, *pNode3;

		const float fOriginalDistToConsider = 40.0f;
		float	DistToConsider = fOriginalDistToConsider;		// Only use indicators for junctions this far ahead
		s32 NodeIndex = rage::Max(0, pNodeList->GetTargetNodeIndex() - 2);
		Node0 = NodeIndex > 0 ? pNodeList->GetPathNodeAddr(NodeIndex - 1) : EmptyNodeAddress;
		Node1 = pNodeList->GetPathNodeAddr(NodeIndex);
		Node2 = pNodeList->GetPathNodeAddr(NodeIndex + 1);
		Node3 = pNodeList->GetPathNodeAddr(NodeIndex + 2);

		if(Node1.IsEmpty()) return;
		if(Node2.IsEmpty()) return;
		if(Node3.IsEmpty()) return;

		if(!ThePaths.IsRegionLoaded(Node1)) return;
		if(!ThePaths.IsRegionLoaded(Node2)) return;
		if(!ThePaths.IsRegionLoaded(Node3)) return;

		pNode1 = ThePaths.FindNodePointer(Node1);
		pNode2 = ThePaths.FindNodePointer(Node2);
		pNode3 = ThePaths.FindNodePointer(Node3);

		Vector3 coors1, coors2, coors3;
		pNode1->GetCoors(coors1);
		pNode2->GetCoors(coors2);
		pNode3->GetCoors(coors3);

		//**********************
//			Vector3 vBackLeft = pVeh->GetBoundingBoxMin();
//			Vector3 vBackRight = vBackLeft;
//			vBackRight.x = pVeh->GetBoundingBoxMax().x;
//			vBackLeft = pVeh->TransformIntoWorldSpace(vBackLeft);
//			vBackRight = pVeh->TransformIntoWorldSpace(vBackRight);
		//***********************

		while(NodeIndex < CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED-3)
		{
			// Consider this combination of 3 nodes.
			DiffIn = coors2 - coors1;
			float Dist = DiffIn.XYMag();
			DistToConsider -= Dist;
			if(DistToConsider < 0.0f) 
				break;		// We're done.

			if(pNode2->NumLinks() > 2 && (!pNode2->m_2.m_switchedOffOriginal || !pNode2->IsSwitchedOff()))
			{		// This is a potential junction
// 					DiffOut = coors3 - coors2;
// 					DiffIn.z = 0.0f;
// 					DiffOut.z = 0.0f;
// 					DiffIn.Normalize();
// 					DiffOut.Normalize();

				bool bSharpTurn = false;
				float fLeftness = 0.0f, fDotProduct = 0.0f;

				u32 TurnDir = CPathNodeRouteSearchHelper::FindJunctionTurnDirection(Node0, Node1, Node2, Node3, &bSharpTurn, &fLeftness, CPathNodeRouteSearchHelper::sm_fDefaultDotThresholdForRoadStraightAhead, &fDotProduct);

				//if(DiffIn.Dot(DiffOut) < 0.25f)
				if (TurnDir != BIT_NOT_SET)
				{	
					// Reasonably sharp turn
					//float CrossUp = DiffIn.CrossZ(DiffOut);
					if(TurnDir == BIT_TURN_RIGHT && CDriverPersonality::UsesTurnIndicatorsToTurnRight(pVeh->GetDriver(), pVeh))
					{
						//grcDebugDraw::Sphere(vBackRight, 0.25f, Color_yellow, false);
						pVeh->m_nVehicleFlags.bRightIndicator = m_bRightIndicator = true;
					}
					else if (TurnDir == BIT_TURN_LEFT && CDriverPersonality::UsesTurnIndicatorsToTurnLeft(pVeh->GetDriver(), pVeh))
					{
						//grcDebugDraw::Sphere(vBackLeft, 0.25f, Color_yellow, false);
						pVeh->m_nVehicleFlags.bLeftIndicator = m_bLeftIndicator = true;
					}

					return;		// We don't look further after a junction.
				}

				break;
			}

			// Shift the nodes on by one
			Node0 = Node1;
			Node1 = Node2;
			Node2 = Node3;

			pNode1 = pNode2;
			pNode2 = pNode3;

			//coors1 = coors2;
			//coors2 = coors3;

			NodeIndex++;
			Node3 = pNodeList->GetPathNodeAddr(NodeIndex + 2);
			if(Node3.IsEmpty()) 
				break;
			if(!ThePaths.IsRegionLoaded(Node3)) 
				break;
			pNode3 = ThePaths.FindNodePointer(Node3);
			pNode3->GetCoors(coors3);
		}

		if (m_followRoute.CurrentRouteContainsLaneChange())
		{
			if (m_followRoute.IsUpcomingLaneChangeToTheLeft())
			{
				pVeh->m_nVehicleFlags.bLeftIndicator = m_bLeftIndicator = true;
			}
			else
			{
				pVeh->m_nVehicleFlags.bRightIndicator = m_bRightIndicator = true;
			}
		}

//		bool bLaneChangesLeft = false;
// 		if (DoesPathHaveLaneChange(pNodeList, fOriginalDistToConsider, bLaneChangesLeft))
// 		{
// 			if(!bLaneChangesLeft)
// 			{
// 				//grcDebugDraw::Sphere(vBackRight, 0.25f, Color_yellow, false);
// 				pVeh->m_nVehicleFlags.bRightIndicator = true;
// 			}
// 			else
// 			{
// 				//grcDebugDraw::Sphere(vBackLeft, 0.25f, Color_yellow, false);
// 				pVeh->m_nVehicleFlags.bLeftIndicator = true;
// 			}
// 		}
	}
}

void CTaskVehicleCruiseNew::ProcessSuperDummyHelper(CVehicle* pVehicle, const float fBackupSpeed) const
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

void CTaskVehicleCruiseNew::ProcessRoofConvertibleHelper(CVehicle* pVehicle, const bool bStoppedAtRedLight) const
{
	m_RoofHelper.PotentiallyRaiseOrLowerRoof(pVehicle, bStoppedAtRedLight);
}


void CTaskVehicleCruiseNew::ProcessTimeslicingAllowedWhenStopped()
{
	if( CVehicleAILodManager::ms_bUseTimeslicing )
	{
		CVehicle* pVehicle = GetVehicle();
		if(!m_bHasComeToStop)
		{
			const float fMaxSpeedForTimeslicing = 0.25f;
			if( pVehicle->GetAiXYSpeed() < fMaxSpeedForTimeslicing )
			{
				m_bHasComeToStop = true;
			}
			else
			{
				return;
			}
		}

		pVehicle->m_nVehicleFlags.bLodShouldUseTimeslicing = true;
	}
}


aiTask::FSM_Return CTaskVehicleCruiseNew::Cruise_OnEnter()
{
	// Calculate the initial target position
	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.
	Vector3 vStartCoords = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleBonnetPosition(pVehicle, IsDrivingFlagSet(DF_DriveInReverse)));

	Vector3 vTargetPosition;
	float fSpeed = GetCruiseSpeed();
	m_fPathLength = FindTargetPositionAndCapSpeed(pVehicle, vStartCoords, vTargetPosition, fSpeed, true);

#if __ASSERT
	static const Vec3V svWorldLimitsMin(WORLDLIMITS_XMIN, WORLDLIMITS_YMIN, WORLDLIMITS_ZMIN);
	static const Vec3V svWorldLimitsMax(WORLDLIMITS_XMAX, WORLDLIMITS_YMAX, WORLDLIMITS_ZMAX);
	static const spdAABB sWorldLimitsBox(svWorldLimitsMin, svWorldLimitsMax);
	if (m_pRouteSearchHelper->IsDoingWanderRoute() &&
		(vTargetPosition.Dist2(ORIGIN) < SMALL_FLOAT || !vTargetPosition.FiniteElements()|| !sWorldLimitsBox.ContainsPoint(VECTOR3_TO_VEC3V(vTargetPosition))))
	{
		Assertf(false, "Invalid target coord found. \nTarget Pos: %f %f %f\n Start Coords: %f %f %f Speed: %f", 
			vTargetPosition.x, vTargetPosition.y, vTargetPosition.z, vStartCoords.x, vStartCoords.y, vStartCoords.z, fSpeed);
	}
#endif

// 	Assertf(m_fPathLength > 0.0f, "PathLength < 0 detected! This is ignorable, but please let JMart know about these coordinates: %.2f, %.2f, %.2f"
// 		, vStartCoords.x, vStartCoords.y, vStartCoords.z);
	m_fPathLength = Max(m_fPathLength, 0.0f);

	sVehicleMissionParams params = m_Params;
	params.SetTargetPosition(vTargetPosition);
	params.m_fTargetArriveDist = 0.0f;
	params.m_fCruiseSpeed = fSpeed;
	params.m_fMaxCruiseSpeed = 0.0f;
	if (pVehicle->InheritsFromPlane())
	{
		params.m_iDrivingFlags = DMode_StopForCars_Strict;
	}

// #if __ASSERT
// 	if (!aiVerifyf(vTargetPosition.Dist2(ORIGIN) > SMALL_FLOAT && vTargetPosition.FiniteElements()
// 		,"Target position is at the origin or non-finite. Here's what we know:"))
// 	{
// 		static bool s_bPrintedAlready = false;
// 		if (!s_bPrintedAlready)
// 		{
// 			const Vector3 vPos = VEC3V_TO_VECTOR3(pVehicle->GetVehiclePosition());
// 			aiDisplayf("Vehicle: %p, Position: (%.2f, %.2f, %.2f), TaskType: %d", pVehicle, vPos.x, vPos.y, vPos.z, GetTaskType());
// 			aiDisplayf("Child target: (%.2f, %.2f, %.2f), Parent Target: (%.2f, %.2f, %.2f), Parent Target Entity: %p"
// 				, vTargetPosition.x, vTargetPosition.y, vTargetPosition.z, m_Params.m_vTargetPos.x, m_Params.m_vTargetPos.y, m_Params.m_vTargetPos.z
// 				, m_Params.m_TargetEntity.GetEntity());
// 			aiDisplayf("Nodelist--Current Target: %d", m_pRouteSearchHelper->GetNodeList().GetTargetNodeIndex());
// 			for (s32 i = 0; i < CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED; i++)
// 			{
// 				aiDisplayf("aNodes[%d]=%d:%d aLinks[%d]=%d\n", i, m_pRouteSearchHelper->GetNodeList().GetPathNodeAddr(i).GetRegion()
// 					, m_pRouteSearchHelper->GetNodeList().GetPathNodeAddr(i).GetIndex()
// 					, i, m_pRouteSearchHelper->GetNodeList().GetPathLinkIndex(i));
// 			}
// 			aiDisplayf("Followroute: Current Target: %d, NumPoints: %d", m_followRoute.GetCurrentTargetPoint(), m_followRoute.GetNumPoints());
// 			for (s32 i = 0; i < CVehicleFollowRouteHelper::MAX_ROUTE_SIZE; i++)
// 			{
// 				aiDisplayf("Point [%d], Pos: (%.2f, %.2f, %.2f)", i, m_followRoute.GetRoutePoints()[i].GetPosition().GetXf()
// 					, m_followRoute.GetRoutePoints()[i].GetPosition().GetYf()
// 					, m_followRoute.GetRoutePoints()[i].GetPosition().GetZf());
// 			}
// 
// 			s_bPrintedAlready = true;
// 		}
// 	}
// #endif //__ASSERT

	//params.m_TargetEntity = NULL;
	SetNewTask( rage_new CTaskVehicleGoToPointWithAvoidanceAutomobile(params, true));
	return FSM_Continue;
}

aiTask::FSM_Return CTaskVehicleCruiseNew::Cruise_OnUpdate()
{
	TUNE_GROUP_FLOAT(NEW_CRUISE_TASK, sfMinDistToRoadToCheckNavmeshLOS, 50.0f, 0.0f, 500.0f, 1.0f);
	TUNE_GROUP_FLOAT(FLEE_WAIT, fHighwaySpeedForWreckPercent, 0.6f, 0.0f, 1.0, 0.01f);

	CVehicle *pVehicle = GetVehicle(); //Get the vehicle ptr.

	if(CheckForStateChangeDueToDistanceToTarget(pVehicle))//if this returns true we know we should be changing state, so just return.
	{
		return FSM_Continue;
	}

	pVehicle->GetIntelligence()->CacheNodeList(this);

	// set the target for the gotopointwithavoidance task
	CTaskVehicleGoToPointWithAvoidanceAutomobile* pGoToTask = NULL;
	if(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_VEHICLE_GOTO_POINT_WITH_AVOIDANCE_AUTOMOBILE)
	{
		pGoToTask = (CTaskVehicleGoToPointWithAvoidanceAutomobile*)GetSubTask();
	}

	const Vector3 vStartCoords = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleBonnetPosition(pVehicle, IsDrivingFlagSet(DF_DriveInReverse)));
	const Vector3 vStartCoordsNavMesh = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleBonnetPositionForNavmeshQueries(pVehicle, IsDrivingFlagSet(DF_DriveInReverse)));

// 	bool bCanProcessLaneChange = false;
// 	if (pVehicle->GetIntelligence()->GetLaneChangeDistributer().IsRegistered())
// 	{
// 		if(pVehicle->GetIntelligence()->GetLaneChangeDistributer().ShouldBeProcessedThisFrame())
// 		{
// 			bCanProcessLaneChange = true;
// 		}	
// 	}

// 	const bool bWantsToChangeLanes = bCanProcessLaneChange && pGoToTask->WantsToChangeLanesForTraffic();
// 	const bool bWantsToOvertake = bWantsToChangeLanes && pGoToTask->WantsToOvertake();
// 	const bool bWantsToUndertake = bWantsToChangeLanes && pGoToTask->WantsToUndertake();

	Vector3 vTargetPosition;
	float fSpeed = GetCruiseSpeed();

	//returns true if the vehicle should stop for a junction.
	//it may modify the speed parameter, allowing the vehicle to slow
	//down for junctions before entering the stop state
	if(m_JunctionHelper.ProcessTrafficLights(pVehicle, fSpeed, IsDrivingFlagSet(DF_StopAtLights), GetPreviousState() == State_WaitBeforeMovingAgain))
	{
		pVehicle->GetIntelligence()->InvalidateCachedNodeList();
		SetState(State_StopForJunction);
		return FSM_Continue;//we're stopping at a traffic light
	}

	//if we're moving through a junction, assume dirty
// 	if (pVehicle->GetIntelligence()->GetJunctionCommand() == JUNCTION_COMMAND_GO)
// 	{
// 		pVehicle->m_nVehicleFlags.bAvoidanceDirtyFlag = true;
// 	}

	//always reset no matter what, so we zero them out if we lose the ability to update
	m_LaneChangeHelper.ResetFrameVariables();

	TUNE_GROUP_BOOL(CAR_AI, EnableLaneChanges, true);
	if (IsDrivingFlagSet(DF_ChangeLanesAroundObstructions) && m_LaneChangeHelper.LaneChangesAllowedForThisVehicle(pVehicle, &m_followRoute) && EnableLaneChanges)
	{
		CPhysical* pObstacle = m_LaneChangeHelper.GetEntityToChangeLanesFor();
		float fDistToObstruction = FLT_MAX;
		int iObstructedSegment = -1;	//arbitrary value

		//this updates some timers, so it needs to be called every frame
		bool bCanSwitchLanes = m_LaneChangeHelper.CanSwitchLanesToAvoidObstruction(pVehicle, &m_followRoute, pObstacle, fSpeed, IsDrivingFlagSet(DF_DriveInReverse), GetTimeStepInMilliseconds());

		if (m_LaneChangeHelper.GetForceUpdate() || pVehicle->GetIntelligence()->GetLaneChangeDistributer().IsRegistered())
		{
			if(m_LaneChangeHelper.GetForceUpdate() || pVehicle->GetIntelligence()->GetLaneChangeDistributer().ShouldBeProcessedThisFrame())
			{
				//these are all passed by reference and may be modified
				bool bGotNewObstacle = m_LaneChangeHelper.FindEntityToChangeLanesFor(pVehicle, &m_followRoute, fSpeed, IsDrivingFlagSet(DF_DriveInReverse), pObstacle, fDistToObstruction, iObstructedSegment);

				//search for a potential vehicle to change lanes for here
				if (bGotNewObstacle && !bCanSwitchLanes)	
				{
					//if we found a new one on this update, try and see if we can change lanes for it right away
					//ensure we don't call it twice in one frame though--the timers were only updated if the function returns true
					bCanSwitchLanes = m_LaneChangeHelper.CanSwitchLanesToAvoidObstruction(pVehicle, &m_followRoute, pObstacle, fSpeed, IsDrivingFlagSet(DF_DriveInReverse), GetTimeStepInMilliseconds());
				}

				if (bCanSwitchLanes && pObstacle)
				{
					m_LaneChangeHelper.UpdatePotentialLaneChange(pVehicle, &m_followRoute, pObstacle, fSpeed, fDistToObstruction, iObstructedSegment, IsDrivingFlagSet(DF_DriveInReverse));
				}
			}	
		}
	}

	//like the call to ResetFrameVariables() above, decay the timers even if we lose the ability to update
	m_LaneChangeHelper.DecayLaneChangeTimers(pVehicle, GetTimeStepInMilliseconds());
	m_LaneChangeHelper.SetForceUpdate(false);

	UpdateEmptyLaneAwareness();

	if (m_pRouteSearchHelper->IsDoingWanderRoute()
		&& pVehicle->m_nVehicleFlags.bToldToGetOutOfTheWay)
	{
		fSpeed = rage::Max(fSpeed, 50.0f);
	}

	const float fDistanceToTheEndOfRoute = FindTargetPositionAndCapSpeed(pVehicle, vStartCoords, vTargetPosition, fSpeed, false);

	Vector3 vThisTargetPos(Vector3::ZeroType);
	if (m_Params.IsTargetValid())
	{
		m_Params.GetDominantTargetPosition(vThisTargetPos);
	}
	
	static const Vec3V svWorldLimitsMin(WORLDLIMITS_XMIN, WORLDLIMITS_YMIN, WORLDLIMITS_ZMIN);
	static const Vec3V svWorldLimitsMax(WORLDLIMITS_XMAX, WORLDLIMITS_YMAX, WORLDLIMITS_ZMAX);
	static const spdAABB sWorldLimitsBox(svWorldLimitsMin, svWorldLimitsMax);
	if (
		(vTargetPosition.Dist2(ORIGIN) < SMALL_FLOAT || !vTargetPosition.FiniteElements()
		|| !sWorldLimitsBox.ContainsPoint(VECTOR3_TO_VEC3V(vTargetPosition)))
		&&
		(m_pRouteSearchHelper->IsDoingWanderRoute() || vThisTargetPos.Dist2(ORIGIN) > SMALL_FLOAT)
		)
	{
		aiWarningf("Something bad happened to our path--throwing it out and re-planning. \nTarget Pos: %f %f %f\n ThisTargetPos: %f %f %f ", 
					vTargetPosition.x, vTargetPosition.y, vTargetPosition.z, vThisTargetPos.x, vThisTargetPos.y, vThisTargetPos.z);

		SetState(State_FindRoute);
		pVehicle->GetIntelligence()->InvalidateCachedNodeList();

		++m_nTimesFailedResettingTarget;

		if (m_nTimesFailedResettingTarget > 3)
		{
			return FSM_Quit;
		}
		else
		{
			return FSM_Continue;
		}
	}

	//reset this if we get past the code that checks it, above
	m_nTimesFailedResettingTarget = 0;

	if (UpdateShouldFindNewRoute(fDistanceToTheEndOfRoute, true, false))
	{
		pVehicle->GetIntelligence()->InvalidateCachedNodeList();
		return FSM_Continue;
	}

	//if we're directing our search toward a target, and can't look ahead any further on our path
	//try switching to a navmesh route
	if (!m_pRouteSearchHelper->IsDoingWanderRoute() && fDistanceToTheEndOfRoute < SMALL_FLOAT)
	{
		//if someone's set a TargetArriveDist of 0, it usually means it's some parent behavior
		//that is just sticking a point out in front of the car to steer toward. we very rarely
		//want to actually pathfind in this case, so just let it skip to the navmesh state
		
		if (ms_bAllowCruiseTaskUseNavMesh && !IsEscorting() && CanUseNavmeshToReachPoint(pVehicle, &vStartCoordsNavMesh))
		{
			pVehicle->GetIntelligence()->InvalidateCachedNodeList();
			SetState(State_GoToNavmesh);
			return FSM_Continue;
		}
		else
		{
			pVehicle->GetIntelligence()->InvalidateCachedNodeList();
			SetState(State_GoToStraightLine);
			return FSM_Continue;
		}
	}

	//const float fDistSqrToTargetNode = vStartCoords.Dist2(m_RouteSearchHelper);
	//const float fMinDistFromNodeSqr = sfMinDistToRoadToCheckNavmeshLOS * sfMinDistToRoadToCheckNavmeshLOS;
	if (/*fDistSqrToTargetNode > fMinDistFromNodeSqr &&*/
		UpdateNavmeshLOSTestForCruising(vStartCoordsNavMesh, false))
	{
		pVehicle->GetIntelligence()->InvalidateCachedNodeList();
		return FSM_Continue;
	}

	//occasionally think about revving engine
	if (UpdateShouldRevEngine())
	{
		pVehicle->GetIntelligence()->InvalidateCachedNodeList();
		SetState(State_RevEngine);
		return FSM_Continue;
	}

	ProcessRoofConvertibleHelper(pVehicle, m_JunctionHelper.IsWaitingForTrafficLight());

	//don't start moving again until our roof transition is done,
	//if we had already stopped
	const bool bShouldStopWhileTransitioningRoof = IsDrivingFlagSet(DF_StopForCars) && !IsDrivingFlagSet(DF_SwerveAroundAllCars);
	if (bShouldStopWhileTransitioningRoof 
		&& pVehicle->GetAiXYSpeed() < 0.25f 
		&& m_RoofHelper.IsRoofDoingAnimatedTransition(pVehicle))
	{
		fSpeed = 0.0f;
	}

	//Alters the speed of the vehicle if the driver has a shocking event handle that requires it
	CPed* pDriver = pVehicle->GetDriver();
	if (pDriver)
	{
		CTask* pActiveSecondaryTask = pDriver->GetPedIntelligence()->GetTaskSecondaryActive();
		if(pActiveSecondaryTask && pActiveSecondaryTask->GetTaskType() == CTaskTypes::TASK_AMBIENT_LOOK_AT_EVENT)
		{
			const CEvent* pLookAtEvent = static_cast<CTaskAmbientLookAtEvent*>(pActiveSecondaryTask)->GetEventToLookAt();

			if(pLookAtEvent && pLookAtEvent->IsShockingEvent())
			{
				const CEventShocking& shocking = static_cast<const CEventShocking&>(*pLookAtEvent);
				if(shocking.GetShouldVehicleSlowDown()) 
				{
					Vector3 vEventPos;
					shocking.GetEntityOrSourcePosition(vEventPos);
					m_ShockingEventHelper.ProcessShockingEvents(pVehicle,vEventPos, fSpeed);
				}
			}		
		}
	}

	//slow down cars if on highway and we have wrecks nearby
	if(fHighwaySpeedForWreckPercent < 1.0f && CVehiclePopulation::GetTotalEmptyWreckedVehsOnMap() > 0 &&
	   pVehicle->GetIntelligence()->IsOnHighway() && fSpeed > 20.0f)
	{
		CEntityScannerIterator vehicleList = pVehicle->GetIntelligence()->GetVehicleScanner().GetIterator();
		for( CEntity* pEntity = vehicleList.GetFirst(); pEntity; pEntity = vehicleList.GetNext() )
		{
			CVehicle* pOtherVehicle = (CVehicle*) pEntity;
			bool bDeadDriver = pOtherVehicle->GetDriver() && pOtherVehicle->GetDriver()->IsDead();

			if(!pOtherVehicle->m_nVehicleFlags.bIsThisAParkedCar &&	(pOtherVehicle->GetStatus() == STATUS_WRECKED || bDeadDriver))
			{
				fSpeed *= fHighwaySpeedForWreckPercent;
				break;
			}
		}
	}

	if (pGoToTask)
	{
		// If vehicle timeslicing is enabled globally, think about if this vehicle should currently
		// try to use it.
		if(CVehicleAILodManager::ms_bUseTimeslicing)
		{
			// Whether to allow timeslicing at all for this vehicle
			bool bAllowTimeslicing = true;

			// Check if we are associated with a junction that we plan to drive through.
			const CVehicleIntelligence* pIntel = pVehicle->GetIntelligence();
			if(pIntel->GetJunctionCommand() == JUNCTION_COMMAND_GO)
			{
				CJunction* pJunction = pIntel->GetJunction();
				if(pJunction)
				{
					// Check if this is a "fake junction".
					const bool isFakeJunction = pJunction->IsOnlyJunctionBecauseHasSwitchedOffEntrances();

					// Check if this is junction is actually just there as a crossing for peds.
					const CPathNode* pJunctionNode = pJunction->GetNumJunctionNodes() > 0 ? ThePaths.FindNodePointerSafe(pJunction->GetJunctionNode(0)) : NULL;
					const bool isCrossingForPeds = pJunctionNode && pJunctionNode->NumLinks() <= 2;

					// Check if we are the only car using the junction and we are off-screen
					const bool bLoneCarOffscreen = ( pJunction->GetNumberOfCars() == 1 && !pVehicle->GetIsVisibleInSomeViewportThisFrame() );

					if(!bLoneCarOffscreen && !isFakeJunction && !isCrossingForPeds && pVehicle->GetIntelligence()->GetJunctionTurnDirection() != BIT_TURN_STRAIGHT_ON)
					{
						bAllowTimeslicing = false;
					}
				}
			}

			// If we are allowing timeslicing.
			if(bAllowTimeslicing)
			{
				// Note: CTaskVehicleGoToPointWithAvoidanceAutomobile can set this to false
				// again if avoiding something, and CVehicleAILodManager will also consider other
				// factors such as dummy status and visibility before actually allowing timeslicing.
				pVehicle->m_nVehicleFlags.bLodCanUseTimeslicing = true;

				// MAGIC! Speed threshold for allowing use of more aggressive timeslicing.
				static const float s_SpeedThreshold = 0.5f;

				bool shouldUseTimeslicing = false;
				// If we are heading towards a junction with a red light, set the more aggressive
				// bLodShouldUseTimeslicing option. The subtask (CTaskVehicleGoToPointWithAvoidanceAutomobile)
				// will clear this again unless we are actually stopped waiting for the car in front of us.
				if(m_JunctionHelper.IsWaitingForTrafficLight())
				{
					shouldUseTimeslicing = true;
				}
				else if(fwTimer::GetTimeInMilliseconds() != pVehicle->m_TimeOfCreation	// Speed seems to be 0 on the first update, which is not the case we are trying to address.
						&& pVehicle->GetAiXYSpeed() < s_SpeedThreshold)
				{
					// If we have a vehicle in front of us, only use timeslicing if that vehicle is also standing still.
					const CVehicle* pVehInFront = pVehicle->GetIntelligence()->GetCarWeAreBehind();
					if(pVehInFront)
					{
						if(pVehInFront->GetAiXYSpeed() < s_SpeedThreshold)
						{
							shouldUseTimeslicing = true;
						}
					}
					else
					{
						shouldUseTimeslicing = true;
					}
				}

				if(shouldUseTimeslicing)
				{
					pVehicle->m_nVehicleFlags.bLodShouldUseTimeslicing = true;
				}
			}
		}

		pGoToTask->SetTargetPosition(&vTargetPosition);
		pGoToTask->SetCruiseSpeed(fSpeed);
	}

	pVehicle->GetIntelligence()->InvalidateCachedNodeList();
	return FSM_Continue;
}

aiTask::FSM_Return CTaskVehicleCruiseNew::Cruise_OnExit()
{
	m_NavmeshLOSHelper.ResetTest();
	return FSM_Continue;
}

aiTask::FSM_Return	CTaskVehicleCruiseNew::WaitBeforeMovingAgain_OnEnter()
{
	SetNewTask(rage_new CTaskVehicleBrake(NetworkInterface::GetSyncedTimeInMilliseconds() + m_JunctionHelper.GetTimeToStopForCurrentReason(GetVehicle())));
	m_bHasComeToStop = false;
	return FSM_Continue;
}

///////////////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehicleCruiseNew::WaitBeforeMovingAgain_OnUpdate()
{
	//	float speedMultiplier = 1.0f;

	ProcessSuperDummyHelper(GetVehicle(), 0.0f);

	ProcessTimeslicingAllowedWhenStopped();

	if (GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_BRAKE))//if our subtask is finished we should check whether we should still be stopped.
	{
		//		if(ProcessTrafficLights(pVehicle, speedMultiplier))
		//			return FSM_Continue;//still should be stopping for traffic light
		//		else
		SetState(State_Cruise);//done with stopping at the traffic light.
	}

	return FSM_Continue;
}

///////////////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehicleCruiseNew::StopForJunction_OnEnter			()
{
	SetNewTask(rage_new CTaskVehicleBrake(NetworkInterface::GetSyncedTimeInMilliseconds() + 4000));

	//Add shocking event if it's a nice vehicle
	if (GetVehicle()->GetVehicleModelInfo() && GetVehicle()->GetVehicleModelInfo()->GetVehicleSwankness() > SWANKNESS_3 )
	{
		GetVehicle()->AddCommentOnVehicleEvent();
	}

	m_bHasComeToStop = false;

	return FSM_Continue;
}

///////////////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehicleCruiseNew::StopForJunction_OnUpdate		()
{
	float speedMultiplier = 1.0f;
	CVehicle* pVehicle = GetVehicle();
	Assert(pVehicle);

	pVehicle->GetIntelligence()->CacheNodeList(this);

	ProcessSuperDummyHelper(pVehicle, 0.0f);

	ProcessRoofConvertibleHelper(pVehicle, true);

	//if our subtask is finished we should check whether we should still be stopped.
	if (GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_BRAKE) 
		|| pVehicle->GetIntelligence()->GetJunctionCommand() == JUNCTION_COMMAND_GO
		|| pVehicle->GetIntelligence()->GetJunctionCommand() == JUNCTION_COMMAND_GIVE_WAY
		|| pVehicle->GetIntelligence()->GetJunctionCommand() == JUNCTION_COMMAND_APPROACHING)
	{
		if(!pVehicle->GetIntelligence()->GetJunctionNode().IsEmpty() 
			&& m_JunctionHelper.ProcessTrafficLights(pVehicle, speedMultiplier, IsDrivingFlagSet(DF_StopAtLights), GetPreviousState() == State_WaitBeforeMovingAgain)
			&& pVehicle->GetIntelligence()->GetJunctionCommand() != JUNCTION_COMMAND_GIVE_WAY)
		{
			//occasionally think about revving engine
			if (UpdateShouldRevEngine())
			{
				SetState(State_RevEngine);
			}
			else
			{
				ProcessTimeslicingAllowedWhenStopped();
			}

			// We are no longer braking, should be OK to use timeslicing as we are expected to sit still at this point.
			pVehicle->m_nVehicleFlags.bLodCanUseTimeslicing = true;

			//still should be stopping for traffic light
			pVehicle->GetIntelligence()->InvalidateCachedNodeList();
			pVehicle->GetIntelligence()->UpdateCarHasReasonToBeStopped();
			return FSM_Continue;
		}
		
		else
		{
			//done with stopping at the traffic light.
			//maybe do a burnout, maybe wait
			const CVehicleModelInfo* pVehModelInfo = pVehicle->GetVehicleModelInfo();
			Assert(pVehModelInfo);
			mthRandom rndHelper(pVehicle->GetRandomSeed());
			const bool bRndRoll = rndHelper.GetInt() % 2 == 0;
			static dev_float fAggressivenessMin = 0.9f;

			const CPathNode* pEntranceNode = ThePaths.FindNodePointerSafe(pVehicle->GetIntelligence()->GetJunctionEntranceNode());
			const bool bEntranceNodeIsGiveWay = (pEntranceNode && pEntranceNode->IsGiveWay()) 
				|| pVehicle->GetIntelligence()->GetJunctionCommand() == JUNCTION_COMMAND_GIVE_WAY;
			bool bShouldDoBurnout = bRndRoll && pVehicle->InheritsFromAutomobile() 
				&& pVehModelInfo->GetVehicleSwankness() >= SWANKNESS_4
				&& CDriverPersonality::FindDriverAggressiveness(pVehicle->GetDriver(), pVehicle) >= fAggressivenessMin
				&& !pVehicle->IsSuperDummy() && !m_RoofHelper.IsRoofDoingAnimatedTransition(pVehicle)
				&& !pVehicle->GetIntelligence()->GetJunctionNode().IsEmpty()
				&& !pVehicle->GetIntelligence()->GetCarWeAreBehind()
				&& !bEntranceNodeIsGiveWay;

			//only do a burnout if we're going straight. 
			//this is evaluated separately since it's somewhat expensive
			if (bShouldDoBurnout)
			{
				bool bSharpTurn = false;
				float fLeftness = 0.0f, fDotProduct = 0.0f;
				int iNumExitLinks = 0;
				u32 turnDir = CPathNodeRouteSearchHelper::FindUpcomingJunctionTurnDirection(pVehicle->GetIntelligence()->GetJunctionNode(), pVehicle->GetIntelligence()->GetNodeList(), &bSharpTurn, &fLeftness, CPathNodeRouteSearchHelper::sm_fDefaultDotThresholdForRoadStraightAhead, &fDotProduct, iNumExitLinks);
				if (turnDir != BIT_TURN_STRAIGHT_ON)
				{
					bShouldDoBurnout = false;

					//grcDebugDraw::Sphere(pVehicle->GetVehiclePosition(), 2.0f, Color_red, false, 10);
				}
			}

			if (bShouldDoBurnout)
			{
				SetState(State_Burnout);
			}
			else
			{
				SetState(State_WaitBeforeMovingAgain);
			}
		}
	}
	else
	{
		ProcessTimeslicingAllowedWhenStopped();
	}

	pVehicle->GetIntelligence()->InvalidateCachedNodeList();
	return FSM_Continue;
}

aiTask::FSM_Return	CTaskVehicleCruiseNew::GotoStraightLine_OnEnter				()
{
	sVehicleMissionParams params = m_Params;

	FindTargetCoors(GetVehicle(), params);

	//params.m_TargetEntity = NULL

	CVehicle* pVehicle = GetVehicle();
	Assert(pVehicle);

	// Tell the CVehicleFollowRouteHelper where we are going, so we can interpolate
	// towards that position when in timesliced mode.
	m_followRoute.SetStraightLineTargetPos(RCC_VEC3V(params.GetTargetPosition()));

	if (pVehicle->InheritsFromPlane())
	{
		params.m_iDrivingFlags = DMode_StopForCars_Strict;
	}
	
	SetNewTask( rage_new CTaskVehicleGoToPointWithAvoidanceAutomobile(params));

	m_fPathLength = m_followRoute.EstimatePathLength();

	return FSM_Continue;
}

///////////////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehicleCruiseNew::GotoStraightLine_OnUpdate				()
{
	CVehicle* pVehicle = GetVehicle();

	if (GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_GOTO_POINT_WITH_AVOIDANCE_AUTOMOBILE))
	{
		m_bMissionAchieved = true;
		SetState(State_Stop);
		return FSM_Continue;		//finished going to, so stop
	}

	//straight line-navving vehicles should always be avoided
	pVehicle->m_nVehicleFlags.bAvoidanceDirtyFlag = true;
	
	//Find the target position.
	Vector3 vTargetPos;
	FindTargetCoors(pVehicle, vTargetPos);

	//should we be switching back to the nodelist?
	const Vector3 vecVehiclePosition = VEC3V_TO_VECTOR3(pVehicle->GetVehiclePosition());
	const float distance = (vTargetPos - vecVehiclePosition).XYMag();

	//now check to see if we should still be running a straight line task.
	if (fwTimer::GetGameTimer().GetTimeInMilliseconds() > 3000 + m_uTimeLastNotDoingStraightLineNav
		&& (distance > m_fStraightLineDist && !IsDrivingFlagSet(DF_ForceStraightLine)))
	{
		SetState(State_FindRoute);
		return FSM_Continue;
	}
	
	if (!IsDrivingFlagSet(DF_PreventBackgroundPathfinding))
	{
		if (UpdateShouldFindNewRoute(m_followRoute.FindDistanceToEndOfPath(pVehicle), false, true))
		{
			m_fPathLength = m_followRoute.EstimatePathLength();
		}

		//Assert((u32)m_followRoute.GetNumPoints() == m_pRouteSearchHelper->GetCurrentNumNodes());

		//keep our path progress up to date
		m_followRoute.UpdateTargetPositionForStraightLineUpdate(pVehicle, GetDrivingFlags());
		m_pRouteSearchHelper->SetTargetNodeIndex(m_followRoute.GetCurrentTargetPoint());
	}
	
	//Calculate the intermediate target position.
	Vector3 vIntermediateTargetPos = vTargetPos;
	
	//This is the default behavior, not sure why, but I'm leaving it here anyways.
	m_Params.SetTargetPosition(vTargetPos);
	
	// set the target for the gotopointwithavoidance task
	if(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_VEHICLE_GOTO_POINT_WITH_AVOIDANCE_AUTOMOBILE)
	{
		CTaskVehicleGoToPointWithAvoidanceAutomobile * pGoToTask = (CTaskVehicleGoToPointWithAvoidanceAutomobile*)GetSubTask();

		pGoToTask->SetTargetPosition(&vIntermediateTargetPos);
		pGoToTask->SetCruiseSpeed(GetCruiseSpeed());
	}

	//we don't have road extents when going to straight line
	pVehicle->m_nVehicleFlags.bDisableRoadExtentsForAvoidance = true;

	return FSM_Continue;
}

aiTask::FSM_Return CTaskVehicleCruiseNew::GotoStraightLine_OnExit()
{
	m_followRoute.ClearStraightLineTargetPos();
	CVehicle* pVehicle = GetVehicle();
	pVehicle->m_nVehicleFlags.bDisableRoadExtentsForAvoidance = false;
	return FSM_Continue;
}

aiTask::FSM_Return CTaskVehicleCruiseNew::GotoNavmesh_OnEnter()
{
	m_NavmeshLOSHelper.ResetTest();

	sVehicleMissionParams params = m_Params;
	params.m_fMaxCruiseSpeed = 0.0f;
	params.m_iDrivingFlags.ClearFlag(DF_AvoidTargetCoors);
	m_syncedNode.SetEmpty();

	SetNewTask(rage_new CTaskVehicleGoToNavmesh(params));
	return FSM_Continue;
}

aiTask::FSM_Return CTaskVehicleCruiseNew::GotoNavmesh_OnUpdate()
{
	Vector3 vTargetPos;
	FindTargetCoors(GetVehicle(), vTargetPos);

	return Navmesh_Shared_OnUpdate(vTargetPos, State_GoToStraightLine, m_fStraightLineDist);
}

aiTask::FSM_Return CTaskVehicleCruiseNew::GotoNavmesh_OnExit()
{
	return Navmesh_Shared_OnExit();
}

bool CTaskVehicleCruiseNew::GetTargetPositionForGetToRoadUsingNavmesh(Vector3::Ref vTargetPos) const
{
	//set the target position to our target node if the route search helper has one
	//if it doesn't, target position will fall back to the parent task's target, which it 
	//would have gotten above

	CVehicleNodeList& NodeList = m_pRouteSearchHelper->GetNodeList();
	const int iTargetNode = NodeList.GetTargetNodeIndex();
	const CPathNode* pNode = NodeList.GetPathNode(iTargetNode);
	const CPathNode* pPrevNode = NULL;
	if (iTargetNode > 0)
	{
		pPrevNode = NodeList.GetPathNode(iTargetNode - 1);
	}

#if ((AI_OPTIMISATIONS_OFF) || (AI_VEHICLE_OPTIMISATIONS_OFF))
	if (aiVerifyf(pNode && pPrevNode, "GetToRoadUsingNavmesh_OnEnter expects a valid target node"))
#else
	if (pNode && pPrevNode)
#endif
	{
		CPathNodeLink* pLink = ThePaths.FindLinkPointerSafe(NodeList.GetPathNodeAddr(iTargetNode-1).GetRegion(),NodeList.GetPathLinkIndex(iTargetNode-1));
		if (pLink)
		{
			Vector3 vPrevPos, vNextPos;
			CPathNodeRouteSearchHelper::GetZerothLanePositionsForNodesWithLink(pPrevNode, pNode, pLink, vPrevPos, vNextPos);
			vTargetPos = vNextPos;
		}
		else
		{
			vTargetPos = pNode->GetPos();
		}

		return true;
	}

	return false;
}

aiTask::FSM_Return CTaskVehicleCruiseNew::GetToRoadUsingNavmesh_OnEnter()
{
	m_NavmeshLOSHelper.ResetTest();

	sVehicleMissionParams params = m_Params;

	params.m_fTargetArriveDist = 1.0f;
	params.ClearTargetEntity();	//clear out any entity target we may have
	params.m_fMaxCruiseSpeed = 0.0f;
	params.m_iDrivingFlags.ClearFlag(DF_AvoidTargetCoors);
	m_syncedNode.SetEmpty();

	//if the task didn't find anything, don't setup the subtask
	//so this state will exit right away
	Vector3 vTargetPos;
	if (GetTargetPositionForGetToRoadUsingNavmesh(vTargetPos))
	{
		params.SetTargetPosition(vTargetPos);
		SetNewTask(rage_new CTaskVehicleGoToNavmesh(params));
	}
	
	return FSM_Continue;
}

aiTask::FSM_Return CTaskVehicleCruiseNew::GetToRoadUsingNavmesh_OnUpdate()
{
	Vector3 vTargetPos;
	//initialize vTargetPos to the destination point just in case
	FindTargetCoors(GetVehicle(), vTargetPos);

	GetTargetPositionForGetToRoadUsingNavmesh(vTargetPos);

	const CruiseState nextState = State_FindRoute;
	//const s32 iIterStart = rage::Max(0, NodeList.GetTargetNodeIndex()-2);

	//we need to get the address of i+1 within IsPointWithinBoundariesOfSegment, so 
// 	const s32 iIterEnd = rage::Min(NodeList.GetTargetNodeIndex()+2, CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED-1);
// 	const Vector3 vBonnetPos = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleBonnetPosition(GetVehicle(), IsDrivingFlagSet(DF_DriveInReverse)));
// 	for (int i = iIterStart; i < iIterEnd; i++)
// 	{
// 		if (NodeList.IsPointWithinBoundariesOfSegment(vBonnetPos, i))
// 		{
// 			SetState(nextState);
// 			return FSM_Continue;
// 		}
// 	}

	CVehicleNodeList& NodeList = m_pRouteSearchHelper->GetNodeList();
	if (!ThePaths.DoesVehicleThinkItsOffRoad(GetVehicle(), IsDrivingFlagSet(DF_DriveInReverse), &NodeList))
	{
		SetState(nextState);
		return FSM_Continue;
	}

	return Navmesh_Shared_OnUpdate(vTargetPos, nextState, 10.0f);
}

aiTask::FSM_Return CTaskVehicleCruiseNew::GetToRoadUsingNavmesh_OnExit()
{
	return Navmesh_Shared_OnExit();
}

aiTask::FSM_Return CTaskVehicleCruiseNew::Navmesh_Shared_OnEnter()
{
	return FSM_Continue;
}

aiTask::FSM_Return CTaskVehicleCruiseNew::Navmesh_Shared_OnUpdate(Vector3& vTargetPos, CruiseState desiredNextState, const float fDistToAllowSwitchToStraightLineRoute)
{
	CVehicle* pVehicle = GetVehicle();

	//check for things like straight line distance, completion distance, etc
	if(CheckForStateChangeDueToDistanceToTarget(pVehicle))
	{
		return FSM_Continue;
	}

	//if we lose the navmesh at all while in this state, go on to what we wanted to do next.
	//this is just a lookup, James says it's ok to call every frame
	if (!CanUseNavmeshToReachPoint(pVehicle, NULL, &vTargetPos))
	{
		SetState(desiredNextState);
		return FSM_Continue;
	}

	//TODO: it's possible to get into here if we fail to find any route on the road network,
	//not just for not having LOS to the target.  we should occasionally try to join ourselves to the 
	//road network so we have a position to search for.
	//don't do it too often though, since that's pretty expensive -JM

	//if we have navmesh LOS to the first target point on the follow route, jump back there
	//Check if a LOS NavMesh test is active.
	if (!IsDrivingFlagSet(DF_PreferNavmeshRoute))	//don't do this if we always want to use the navmesh anyway
	{
		const bool bShouldTestNavmeshThisFrame = pVehicle->GetIntelligence()->GetNavmeshLOSDistributer().IsRegistered() 
			&& pVehicle->GetIntelligence()->GetNavmeshLOSDistributer().ShouldBeProcessedThisFrame();

		//Always try and receive the results of an already-issued query
		const float fDistSqrToTarget = DistSquared(pVehicle->GetVehiclePosition(), VECTOR3_TO_VEC3V(vTargetPos)).Getf();
		if(m_NavmeshLOSHelper.IsTestActive())
		{
			//Get the results.
			SearchStatus searchResult;
			bool bLosIsClear;
			bool bNoSurfaceAtCoords;
			if(m_NavmeshLOSHelper.GetTestResults(searchResult, bLosIsClear, bNoSurfaceAtCoords, 1, NULL) && (searchResult != SS_StillSearching))
			{
				//Check if the line of sight is clear.
				if(bLosIsClear) 
				{
					SetState(desiredNextState);
					return FSM_Continue;
				}
			}
		}
		else if (bShouldTestNavmeshThisFrame && fDistSqrToTarget < fDistToAllowSwitchToStraightLineRoute*fDistToAllowSwitchToStraightLineRoute)
		{
			const Vector3 vStartCoords = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleBonnetPositionForNavmeshQueries(pVehicle, IsDrivingFlagSet(DF_DriveInReverse)));
			const float fBoundRadiusMultiplierToUse = pVehicle->InheritsFromBike() ? CTaskVehicleGoToNavmesh::ms_fBoundRadiusMultiplierBike : CTaskVehicleGoToNavmesh::ms_fBoundRadiusMultiplier;

			m_NavmeshLOSHelper.StartLosTest(vStartCoords, vTargetPos, pVehicle->GetBoundRadius() * fBoundRadiusMultiplierToUse
				, false, false, false, 0, NULL, CTaskVehicleGoToNavmesh::ms_fMaxNavigableAngleRadians);
		}
	}

	//update the subtask's target
	if (GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_VEHICLE_GOTO_NAVMESH)
	{
		CTaskVehicleGoToNavmesh* pNavmeshTask = static_cast<CTaskVehicleGoToNavmesh*>(GetSubTask());
		if (pNavmeshTask)
		{
			pNavmeshTask->SetTargetPosition(&vTargetPos);
			pNavmeshTask->SetCruiseSpeed(m_Params.m_fCruiseSpeed);
		}
	}

	if (!GetSubTask() || GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_GOTO_NAVMESH))
	{
		//Note that we were unable to find a route.
		m_uRunningFlags.SetFlag(RF_IsUnableToFindRoute);
		m_uRunningFlags.SetFlag(RF_IsUnableToGetToRoad);
		
		SetState(desiredNextState);
		return FSM_Continue;
	}

	return FSM_Continue;
}

aiTask::FSM_Return CTaskVehicleCruiseNew::Navmesh_Shared_OnExit()
{
	m_NavmeshLOSHelper.ResetTest();
	return FSM_Continue;
}

aiTask::FSM_Return	CTaskVehicleCruiseNew::Burnout_OnEnter()
{
	m_syncedNode.SetEmpty();
	const int iBurnoutLengthMs = g_ReplayRand.GetRanged(1000, 1501);
	SetNewTask(rage_new CTaskVehicleBurnout(NetworkInterface::GetSyncedTimeInMilliseconds() + iBurnoutLengthMs));
	return FSM_Continue;
}

///////////////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehicleCruiseNew::Burnout_OnUpdate()
{
	if (GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_BURNOUT))//if our subtask is finished we should check whether we should still be stopped.
	{
		//		if(ProcessTrafficLights(pVehicle, speedMultiplier))
		//			return FSM_Continue;//still should be stopping for traffic light
		//		else
		SetState(State_Cruise);//done with stopping at the traffic light.
	}

	return FSM_Continue;
}

bool CTaskVehicleCruiseNew::UpdateShouldRevEngine()
{
	//B* 1147135--Audio has requested that the AI-controlled revs
	//be disabled as they have their own code to handle this.
#if 1
	return false;

#else
	if (fwTimer::GetTimeInMilliseconds() < m_uNextTimeAllowedToRevEngine)
	{
		return false;
	}

	const CVehicle* pVehicle = GetVehicle();
	const CVehicleModelInfo* pVehModelInfo = pVehicle->GetVehicleModelInfo();
	Assert(pVehModelInfo);

	const CPed* pDriver = pVehicle->GetDriver();

	static dev_float fAggressivenessMin = 0.5f;
	static dev_float fAggressivnessMinForRevvingBackAtPlayer = 0.5f;
	const float fDriverAggressiveness = CDriverPersonality::FindDriverAggressiveness(pDriver, pVehicle);

	if (fDriverAggressiveness < fAggressivenessMin 
		&& fDriverAggressiveness < fAggressivnessMinForRevvingBackAtPlayer)
	{
		return false;
	}

	const bool bHasDecentCar = pVehModelInfo->GetVehicleSwankness() >= SWANKNESS_2;
	const bool bPersonalityAllowsRevving = fDriverAggressiveness >= fAggressivenessMin && (!pDriver || pDriver->PopTypeIsRandom());
	const bool bWillRevAtHotPeds = bPersonalityAllowsRevving && pDriver && pDriver->CheckSexinessFlags(SF_JEER_AT_HOT_PED);
	const bool bWillRevAtOtherCars = bPersonalityAllowsRevving && bHasDecentCar;

	//don't allow revving if we're moving
	//static dev_float s_fSpeedForRevvingThreshold = 0.25f;
	if (pVehicle->GetAiXYSpeed() > 0.25f)
	{
		m_uNextTimeAllowedToRevEngine = fwTimer::GetTimeInMilliseconds() + g_ReplayRand.GetRanged(2000, 6001);
		return false;
	}

	const bool bSomethingToRevEngineAt = IsThereSomethingAroundToRevEngineAt(bWillRevAtHotPeds, bWillRevAtOtherCars);
	if (!bSomethingToRevEngineAt)
	{
		//wait for a few seconds before trying again
		//not as long as we wait when we succesfully rev, though
		m_uNextTimeAllowedToRevEngine = fwTimer::GetTimeInMilliseconds() + g_ReplayRand.GetRanged(1000, 2001);
		return false;
	}

	return true;

#endif //1
}

bool CTaskVehicleCruiseNew::IsThereSomethingAroundToRevEngineAt(const bool bWillRevAtHotPeds, const bool bWillRevAtOtherCars) const
{
	const CVehicle* pVehicle = GetVehicle();
	const Vec3V svOurPosition =  pVehicle->GetVehiclePosition();
	static const ScalarV svZero(V_ZERO);
	static const ScalarV svHeightThreshold(V_SIX);
	static const ScalarV svFlatDistSqThreshold(25.0f * 25.0f);

	//early out: were we revved at recently?
	//if so, consider this a worthwhile revving opportunity
	const CPed* pDriver = pVehicle->GetDriver();
	static dev_u32 s_nTimeToCareAboutRevving = 2000;
	if (pDriver && pDriver->GetPedIntelligence()->GetTimeLastRevvedAt() > fwTimer::GetTimeInMilliseconds() - s_nTimeToCareAboutRevving)
	{
		return true;
	}

	//look for a hot chick
	if (bWillRevAtHotPeds)
	{
		CEntityScannerIterator entityList = pVehicle->GetIntelligence()->GetPedScanner().GetIterator();
		for( CEntity* pEntity = entityList.GetFirst(); pEntity; pEntity = entityList.GetNext() )
		{
			Assert(pEntity->GetIsTypePed());
			CPed * pPed = (CPed*)pEntity;

			//this search also returns peds inside of vehicles, but we aren't going
			//to exclude them here
			//I think it makes sense to rev your engine at a lady in a nearby car

			//don't rev for anyone in our own car
			if (pVehicle == pPed->GetVehiclePedInside())
			{
				continue;
			}

			if (!pPed->CheckSexinessFlags(SF_HOT_PERSON))
			{
				continue;
			}

			if (pPed->IsInjured())
			{
				continue;
			}

			//don't rev for things behind us
			const Vec3V svPedPosition = pPed->GetTransform().GetPosition();
			if (IsLessThanAll(Dot(pVehicle->GetVehicleForwardDirection(), svPedPosition - svOurPosition), svZero))
			{
				continue;
			}

			if (IsLessThanAll(svHeightThreshold, Abs(svPedPosition.GetZ() - svOurPosition.GetZ())))
			{
				continue;
			}

			if (IsLessThanAll(svFlatDistSqThreshold, DistSquared(svOurPosition, svPedPosition)))
			{
				continue;
			}

			return true;
		}
	}
	
	//look for another nice car
	if (bWillRevAtOtherCars)
	{
		CEntityScannerIterator vehicleList = pVehicle->GetIntelligence()->GetVehicleScanner().GetIterator();
		for( CEntity* pEntity = vehicleList.GetFirst(); pEntity; pEntity = vehicleList.GetNext() )
		{
			CVehicle* pOtherVehicle = (CVehicle*) pEntity;

			if(pOtherVehicle == pVehicle)
			{
				continue;
			}

			//don't rev for things behind us
			const Vec3V& svOtherVehPosition = pOtherVehicle->GetVehiclePosition();
			if (IsLessThanAll(Dot(pVehicle->GetVehicleForwardDirection(), svOtherVehPosition - svOurPosition), svZero))
			{
				continue;
			}

			if (IsLessThanAll(svHeightThreshold, Abs(svOtherVehPosition.GetZ() - svOurPosition.GetZ())))
			{
				continue;
			}

			if (IsLessThanAll(svFlatDistSqThreshold, DistSquared(svOurPosition, svOtherVehPosition)))
			{
				continue;
			}

			const CVehicleModelInfo* pOtherVehModelInfo = pOtherVehicle->GetVehicleModelInfo();
			Assert(pOtherVehModelInfo);
			if (pOtherVehModelInfo->GetVehicleSwankness() < SWANKNESS_3)
			{
				continue;
			}

			return true;
		}
	}

	return false;
}

aiTask::FSM_Return	CTaskVehicleCruiseNew::RevEngine_OnEnter()
{
	const int iRevLengthMs = g_ReplayRand.GetRanged(500, 1251);
	SetNewTask(rage_new CTaskVehicleRevEngine(NetworkInterface::GetSyncedTimeInMilliseconds() + iRevLengthMs));
	return FSM_Continue;
}

///////////////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehicleCruiseNew::RevEngine_OnUpdate()
{
	if (GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_REV_ENGINE))
	{
		SetState(State_Cruise);
	}

	ProcessSuperDummyHelper(GetVehicle(), 0.0f);

	//grcDebugDraw::Sphere(GetVehicle()->GetVehiclePosition(), 2.0f, Color_yellow, false);

	return FSM_Continue;
}

aiTask::FSM_Return	CTaskVehicleCruiseNew::RevEngine_OnExit()
{
	m_uNextTimeAllowedToRevEngine = fwTimer::GetTimeInMilliseconds() + g_ReplayRand.GetRanged(6000, 15001);
	return FSM_Continue;
}

aiTask::FSM_Return	CTaskVehicleCruiseNew::Stop_OnEnter				()
{
	SetNewTask(rage_new CTaskVehicleStop());
	return FSM_Continue;
}

///////////////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehicleCruiseNew::Stop_OnUpdate				()
{
	if (GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_STOP))
	{
		return FSM_Quit;
	}
	return FSM_Continue;
}

aiTask::FSM_Return	CTaskVehicleCruiseNew::PauseForUnloadedNodes_OnEnter				()
{
	m_syncedNode.SetEmpty();
	SetNewTask(rage_new CTaskVehicleStop(CTaskVehicleStop::SF_SupressBrakeLight|CTaskVehicleStop::SF_DontResetSteerAngle));
	return FSM_Continue;
}

///////////////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return	CTaskVehicleCruiseNew::PauseForUnloadedNodes_OnUpdate				()
{
	if (GetIsSubtaskFinished(CTaskTypes::TASK_VEHICLE_STOP) && GetTimeInState() > 1.0f)
	{
		//need to re-pathfind to ensure we get a new clean route
		SetState(State_FindRoute);
		return FSM_Continue;
	}

	GetVehicle()->GetIntelligence()->UpdateCarHasReasonToBeStopped();

	return FSM_Continue;
}

void CTaskVehicleCruiseNew::FixUpPathForTemplatedJunction(const u32 iForceDirectionAtJunction)
{
	EnsureHasRouteSearchHelper();

	const CVehicle* pVehicle = GetVehicle();
	if (!pVehicle || pVehicle->GetIntelligence()->GetHasFixedUpPathForCurrentJunction())
	{
		return;
	}

	m_pRouteSearchHelper->FixUpPathForTemplatedJunction(GetVehicle(), iForceDirectionAtJunction);

	m_followRoute.ConstructFromNodeList(GetVehicle(), m_pRouteSearchHelper->GetNodeList(), m_pRouteSearchHelper->ShouldAvoidTurns());

	//if we made it this far, tell the veh intelligence about it
	pVehicle->GetIntelligence()->SetHasFixedUpPathForCurrentJunction(true);
}

void CTaskVehicleCruiseNew::ConstructDefaultFollowRouteFromNodeList()
{
	m_followRoute.ConstructFromNodeList(GetVehicle(), m_pRouteSearchHelper->GetNodeList(), m_pRouteSearchHelper->ShouldAvoidTurns());
}

bool CTaskVehicleCruiseNew::DoesSubtaskThinkWeShouldBeStopped(const CVehicle* pVehicle, Vector3& vTargetPosOut, const bool bCalledFromWithinAiUpdate) const
{
	CTaskVehicleGoToPointWithAvoidanceAutomobile* pSubtask = static_cast<CTaskVehicleGoToPointWithAvoidanceAutomobile*>(FindSubTaskOfType(CTaskTypes::TASK_VEHICLE_GOTO_POINT_WITH_AVOIDANCE_AUTOMOBILE));
	if (pSubtask)
	{
		const float fCruiseSpeed = pSubtask->GetCruiseSpeed();
		const float fVelocitySqr = bCalledFromWithinAiUpdate ? pVehicle->GetAiVelocity().Mag2() : pVehicle->GetVelocity().Mag2();
		const bool bRetVal =  fVelocitySqr < (0.25f * 0.25f) && (pSubtask->IsCurrentlyWaiting() 
			|| (fabsf(fCruiseSpeed) < SMALL_FLOAT));

		if (bRetVal)
		{
			pSubtask->FindTargetCoors(pVehicle, vTargetPosOut);
		}

		return bRetVal;
	}
	
	return false;
}

// Calculates the position the vehicle should be driving towards and the speed.
// Returns the distance to the end of the route
float CTaskVehicleCruiseNew::FindTargetPositionAndCapSpeed( CVehicle* pVehicle, const Vector3& vStartCoords, Vector3& vTargetPosition
				, float& fOutSpeed, const bool bMustReturnActualDistToEndOfPath)
{
	//if you modify any of these values, 
	//please change those in CTaskVehicleFollowWaypointRecording::FindTargetPositionAndCapSpeed
	//as well!  
	TUNE_GROUP_FLOAT(NEW_CRUISE_TASK, sfDistanceToProgressRoute,			0.01f, 0.0f, 100.0f, 0.1f);
	TUNE_GROUP_FLOAT(NEW_CRUISE_TASK, sfCruiseSpeedFraction,				0.1f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(NEW_CRUISE_TASK, sfLeftTurnLookaheadMultiplier,		2.7f, 0.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT(NEW_CRUISE_TASK, sfSharpLeftTurnLookaheadMultiplier,	0.5f, 0.0f, 10.0f, 0.01f);
	TUNE_GROUP_BOOL(NEW_CRUISE_TASK, sbDebugSharpLeftTurns, false);
	TUNE_GROUP_FLOAT(NEW_CRUISE_TASK, sfSliplaneLookaheadMultiplier,		2.7f, 0.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT(NEW_CRUISE_TASK, sfRightTurnLookaheadMultiplier,		1.5f, 0.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT(NEW_CRUISE_TASK, sfRightTurnMaxLookaheadForBigVehicles, 8.0f, 0.0f, 100.0f, 0.01f);
	TUNE_GROUP_FLOAT(NEW_CRUISE_TASK, sfNarrowRoadMaxLookahead,				8.0f, 0.0f, 100.0f, 0.01f);
	TUNE_GROUP_FLOAT(NEW_CRUISE_TASK, sfMaxLookaheadSpeed,					30.0f, 0.0f, 100.0f, 0.01f);
	TUNE_GROUP_FLOAT(NEW_CRUISE_TASK, sfDefaultLookahead,					5.0f, 0.0f, 100.0f, 0.01f);
	TUNE_GROUP_FLOAT(NEW_CRUISE_TASK, sfWheelbaseMultiplier,				0.5f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(NEW_CRUISE_TASK, sfSpeedMultiplier,					1.0f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(NEW_CRUISE_TASK, sfDesiredSpeedMaxDiffBike,			8.0f, 0.0f, 100.0f, 0.01f);
	TUNE_GROUP_FLOAT(NEW_CRUISE_TASK, sfDesiredSpeedMaxDiff,				8.0f, 0.0f, 100.0f, 0.01f);
	TUNE_GROUP_FLOAT(NEW_CRUISE_TASK, sfSpeedThreshold,						8.0f, 0.0f, 100.0f, 0.01f);
	TUNE_GROUP_BOOL(NEW_CRUISE_TASK, sbEnableSpeedZones, true);

	// Update the cruise speed based on the road nodes
	CPed* pDriver = pVehicle->GetDriver();
	const bool bDriverIsPlayer = pDriver && pDriver->IsPlayer();
	const bool bCruising = GetTaskType() == CTaskTypes::TASK_VEHICLE_CRUISE_NEW;
	const bool bShouldUseSpeedZones = sbEnableSpeedZones && !bDriverIsPlayer && bCruising;
	const bool bIsMission = pVehicle->PopTypeIsMission() || (pDriver && pDriver->PopTypeIsMission());

	if (bCruising || IsDrivingFlagSet(DF_AdjustCruiseSpeedBasedOnRoadSpeed))
	{
		fOutSpeed = CVehicleIntelligence::MultiplyCruiseSpeedWithMultiplierAndClip(fOutSpeed, pVehicle->GetIntelligence()->SpeedMultiplier, bShouldUseSpeedZones, bIsMission, vStartCoords);
	}
	else if (sbEnableSpeedZones && !bDriverIsPlayer)
	{
		fOutSpeed = rage::Min(fOutSpeed, CRoadSpeedZoneManager::GetInstance().FindMaxSpeedForThisPosition(VEC3V_TO_VECTOR3(pVehicle->GetVehiclePosition()), bIsMission));
	}

	if (DoesSubtaskThinkWeShouldBeStopped(pVehicle, vTargetPosition, true))
	{
		//if we really need to know how long til the end of the path,
		//(called from Cruise_OnEnter), estimate it here.
		//if we're just calling from the update, we don't actually care about this value
		//as the code only checks to see if it's below some threshold.
		//we can assume the distance won't change if we aren't moving, so just return FLT_MAX
		//and save ourselves the trouble
		return !bMustReturnActualDistToEndOfPath ? FLT_MAX : m_followRoute.FindDistanceToEndOfPath(pVehicle);
	}

	// Determine the amount to look down the path.
	// First work out an approximate speed we should consider for lookahead
	const float fDesiredSpeed = fOutSpeed;
	const float fCurrentSpeed = pVehicle->GetAiVelocity().XYMag();
	float fCappedDesiredSpeed = fDesiredSpeed;
	if( pVehicle->InheritsFromBike())
	{
		//clamp the desired speed difference for bikes to prevent them using too much of desired speed - causes cutting issues around sharp corners
		fCappedDesiredSpeed = Clamp(fDesiredSpeed, fCurrentSpeed - sfDesiredSpeedMaxDiffBike, fCurrentSpeed + sfDesiredSpeedMaxDiffBike);
	}
	
	float speedForLookAheadDist = rage::Min((sfCruiseSpeedFraction * fCappedDesiredSpeed) + ((1.0f - sfCruiseSpeedFraction) * fCurrentSpeed), sfMaxLookaheadSpeed);
	
	// Include that whilst considering the wheel base length to work out a lookahead distance.
	float lookAheadDist = sfDefaultLookahead * (1.0f + sfWheelbaseMultiplier * pVehicle->GetVehicleModelInfo()->GetWheelBaseMultiplier()) + 
											sfSpeedMultiplier * rage::Max(0.0f, speedForLookAheadDist - sfSpeedThreshold);

	int iOverrideEndPointIndex = -1;

	// If using a filter lane, lookahead further, presumably for a smooth lanechange
	// don't do this for string pulling vehicles though as they're already artificially
	//increasing the lookahead
	const s32 iTargetNodeIndex = m_pRouteSearchHelper->GetNodeList().GetTargetNodeIndex();
	const s16 iTargetLinkIndex = m_pRouteSearchHelper->GetNodeList().GetPathLinkIndex(iTargetNodeIndex);
	const CPathNode* pCurrentNode = iTargetNodeIndex > 0 
		? m_pRouteSearchHelper->GetNodeList().GetPathNode(iTargetNodeIndex - 1)
		: NULL;
	const CPathNode* pTargetNode = m_pRouteSearchHelper->GetNodeList().GetPathNode(iTargetNodeIndex);
	const CPathNode* pNodeAfterTarget = iTargetNodeIndex < CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED-1
		? m_pRouteSearchHelper->GetNodeList().GetPathNode(iTargetNodeIndex+1)
		: NULL;
	const CPathNode* pNodeAfterThat = iTargetNodeIndex < CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED-2
		? m_pRouteSearchHelper->GetNodeList().GetPathNode(iTargetNodeIndex+2)
		: NULL;
	const bool bTargetNodeIsSlipNode = pTargetNode && pTargetNode->IsSlipLane();
	const bool bCurrentNodeIsSlipNode = pCurrentNode && pCurrentNode->IsSlipLane();
	const bool bTargetIsRealJunction = pTargetNode && pTargetNode->IsJunctionNode();
	const bool bCurrentIsRealJunction = pCurrentNode && pCurrentNode->IsJunctionNode();

	const CPathNodeLink* pLink = (iTargetLinkIndex >= 0 && pTargetNode)
		? ThePaths.FindLinkPointerSafe(pTargetNode->GetAddrRegion(),iTargetLinkIndex)
		: NULL;

	//is one of our next 3 nodes a sliplane? if this expands, convert to for-loop
	const bool bShouldSmoothTurningForSlipLane = !bCurrentNodeIsSlipNode &&
		(bTargetNodeIsSlipNode
		|| (pNodeAfterTarget && pNodeAfterTarget->IsSlipLane())
		|| (pNodeAfterThat && pNodeAfterThat->IsSlipLane()))
		&& !bCurrentIsRealJunction && !bTargetIsRealJunction && (!pNodeAfterTarget || !pNodeAfterTarget->IsJunctionNode());
	if(!IsDrivingFlagSet(DF_UseStringPullingAtJunctions)
		&& ((pVehicle->GetIntelligence()->GetJunctionCommand() == JUNCTION_COMMAND_GO)
			|| bShouldSmoothTurningForSlipLane))
	{
		const CNodeAddress& jnNode = pVehicle->GetIntelligence()->GetJunctionNode();
		const CNodeAddress& jnEntrance = pVehicle->GetIntelligence()->GetJunctionEntranceNode();
		const CPathNode* pJnNode = ThePaths.FindNodePointerSafe(jnNode);
		const CPathNode* pJnEntrance = ThePaths.FindNodePointerSafe(jnEntrance);
		if (pJnNode && pJnEntrance)
		{
			const Vec3V vEntrancePos = VECTOR3_TO_VEC3V(pJnEntrance->GetPos());
			const Vec3V vCarForward = CVehicleFollowRouteHelper::GetVehicleDriveDir(pVehicle, IsDrivingFlagSet(DF_DriveInReverse));
			const Vec3V vCarPos = RCC_VEC3V(vStartCoords);
			const Vec3V vDelta = vEntrancePos - vCarPos;

			//is our junction node still in front of us?
			const bool bEntranceIsBehindUs = IsLessThanAll(Dot(vDelta, vCarForward), ScalarV(V_ZERO)) != 0;
			if (bEntranceIsBehindUs || bShouldSmoothTurningForSlipLane)
			{
				const s32 iJunctionIndex = m_pRouteSearchHelper->GetNodeList().FindNodeIndex(jnNode);
				const bool bVehicleTakesWideTurns = pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_AVOID_TURNS)
					|| pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_BIG)
					|| pVehicle->HasTrailer();
				const u32 turnDir = pVehicle->GetIntelligence()->GetJunctionTurnDirection();
				pVehicle->m_nVehicleFlags.bTurningLeftAtJunction = turnDir == BIT_TURN_LEFT;
				if (bShouldSmoothTurningForSlipLane)
				{
			 		//CVehicle::ms_debugDraw.AddLine(pVehicle->GetVehiclePosition(), pVehicle->GetVehiclePosition() + Vec3V(0.0f, 0.0f, 6.0f), Color_orange);
			 		lookAheadDist *= sfSliplaneLookaheadMultiplier;
				}
				else if (turnDir == BIT_TURN_LEFT && !pVehicle->m_nVehicleFlags.bWillTurnLeftAgainstOncomingTraffic)
				{
					//CVehicle::ms_debugDraw.AddLine(pVehicle->GetVehiclePosition(), pVehicle->GetVehiclePosition() + Vec3V(0.0f, 0.0f, 6.0f), Color_orange);
					float leftTurnLookaheadMultiplier = sfLeftTurnLookaheadMultiplier;

					if (pLink)
					{
						const Vec3V& entrancePosition = VECTOR3_TO_VEC3V(pVehicle->GetIntelligence()->GetJunctionEntrancePosition());
						const Vec3V& exitPosition = VECTOR3_TO_VEC3V(pVehicle->GetIntelligence()->GetJunctionExitPosition());

						const Vec2V entranceToExit = (exitPosition - entrancePosition).GetXY();
						const Vec2V heading(pVehicle->GetIntelligence()->GetJunctionEntrance()->m_vEntryDir.x, pVehicle->GetIntelligence()->GetJunctionEntrance()->m_vEntryDir.y);
						const Vec2V headingRight = Vec2V(-heading.GetY(), heading.GetX());
						const ScalarV distanceFromPlane = Abs(Dot(headingRight, entranceToExit));

#if __BANK
						Color32 debugSharpLeftTurnsColor = Color_white;
#endif // __BANK

						if(IsTrue(distanceFromPlane <= ScalarV(pLink->GetLaneWidth())))
						{
							leftTurnLookaheadMultiplier = sfSharpLeftTurnLookaheadMultiplier;
#if __BANK

							debugSharpLeftTurnsColor = Color_green;
#endif // __BANK
						}

#if __BANK
						if(sbDebugSharpLeftTurns)
						{
							grcDebugDraw::Sphere(pVehicle->GetVehiclePosition() + Vec3V(0.0f, 0.0f, 5.0f), 0.5f, debugSharpLeftTurnsColor);
							grcDebugDraw::Line(entrancePosition + Vec3V(0.0f, 0.0f, 5.0f), exitPosition + Vec3V(0.0f, 0.0f, 5.0f), debugSharpLeftTurnsColor, debugSharpLeftTurnsColor);
							grcDebugDraw::Line(entrancePosition + Vec3V(0.0f, 0.0f, 5.0f), entrancePosition + Vec3V(0.0f, 0.0f, 5.0f) + Vec3V(heading * ScalarV(10.0f), ScalarV(0.0f)), Color_white, Color_white);
							grcDebugDraw::Line(entrancePosition + Vec3V(0.0f, 0.0f, 5.0f), entrancePosition + Vec3V(0.0f, 0.0f, 5.0f) + Vec3V(headingRight * ScalarV(10.0f), ScalarV(0.0f)), Color_red, Color_red);
						}
#endif
					}

					lookAheadDist *= leftTurnLookaheadMultiplier;
				}
				else if (turnDir == BIT_TURN_RIGHT && !bVehicleTakesWideTurns)
				{
					lookAheadDist *= sfRightTurnLookaheadMultiplier;
				}
				else if (turnDir == BIT_TURN_RIGHT && iJunctionIndex != -1)
					{
					const s32 iExitLink = m_pRouteSearchHelper->GetNodeList().GetPathLinkIndex(iJunctionIndex);

					if (iExitLink != -1)
					{
						const CPathNodeLink* pExitLink = ThePaths.FindLinkPointerSafe(jnNode.GetRegion(), iExitLink);

						if (pExitLink && pExitLink->m_1.m_LanesToOtherNode > 1)
						{
							//only do wide turning if we've got more than 1 lane to turn into
							lookAheadDist = rage::Min(lookAheadDist, sfRightTurnMaxLookaheadForBigVehicles);
						}
					}
				}

				//cap this to the distance between here and the junction's exit node
				//or if we're entering a sliplane, clamp it to the junction entrance
				if (pVehicle->GetIntelligence()->GetJunctionFilter() != JUNCTION_FILTER_MIDDLE)
				{
					if (bShouldSmoothTurningForSlipLane && iJunctionIndex > 0 && iJunctionIndex < CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED)
					{
						iOverrideEndPointIndex = iJunctionIndex-1;
					}
					else if (iJunctionIndex >= 0 && iJunctionIndex < CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED-1)
					{
						iOverrideEndPointIndex = iJunctionIndex+1;
						
						//if the junction exit isn't the last node in our path, and the node
						//after the junction is another junction, set iOverrideEndPointIndex
						//to iJunctionIndex+2
						if(iJunctionIndex < CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED-2
							&& m_pRouteSearchHelper->GetNodeList().GetPathNode(iJunctionIndex+1)
							&& m_pRouteSearchHelper->GetNodeList().GetPathNode(iJunctionIndex+1)->IsJunctionNode())
						{
							iOverrideEndPointIndex = iJunctionIndex+2;
						}
					}
				}
			}
		}
	}

	const bool bIsOnNarrowRoad = pLink && pLink->m_1.m_NarrowRoad;
	const bool bIsOnSingleTrackRoad = pLink && pLink->IsSingleTrack();

	//if we're on a single track road, our trajectory might collide with another vehicle's
	if (bIsOnSingleTrackRoad)
	{
		pVehicle->m_nVehicleFlags.bAvoidanceDirtyFlag = true;
	}
	if (bIsOnNarrowRoad)
	{
		lookAheadDist = rage::Min(lookAheadDist, sfNarrowRoadMaxLookahead);
	}

	const float fDistSearched = m_followRoute.FindTargetCoorsAndSpeed(pVehicle, lookAheadDist, sfDistanceToProgressRoute, RCC_VEC3V(vStartCoords)
		, IsDrivingFlagSet(DF_DriveInReverse), RC_VEC3V(vTargetPosition), fOutSpeed, false, true, IsDrivingFlagSet(DF_UseStringPullingAtJunctions)
		, iOverrideEndPointIndex); // TODO: Need to maintain a lane here

	TUNE_GROUP_BOOL(NEW_CRUISE_TASK, sbTestCruiseMigration, false);
	if(NetworkInterface::IsGameInProgress() || sbTestCruiseMigration)
	{
		CacheMigrationData();
	}

	return fDistSearched;
}

//cache future nodes so when we migrate we can restore our intended path
void CTaskVehicleCruiseNew::CacheMigrationData()
{
	m_syncedNode.SetEmpty();

	CVehicle* pVehicle = GetVehicle();
	const CRoutePoint* pRoutePoints = m_followRoute.GetRoutePoints();

	Vector3 vPathPos;
	float fPathSegmentT;

	bool bValidData = false;
	bool bNotEnoughPoints = true;
	s32 iTargetPoint = -1;

	//grab our closest point on the follow route (not our target, as that can be several nodes ahead)
	if(m_followRoute.GetClosestPosOnPath(VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition()), vPathPos, iTargetPoint, fPathSegmentT))
	{
		bNotEnoughPoints = iTargetPoint + NUM_NODESSYNCED >= m_followRoute.GetNumPoints();
		if(bNotEnoughPoints)
		{
			//failed to grab from follow route - fall back to node list - not great as using index from nodelist into follow route - but better than failing
			CVehicleNodeList* pNodeList = GetNodeList();
			if(pNodeList)
			{
				if(pNodeList->GetClosestPosOnPath(VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition()), vPathPos, iTargetPoint, fPathSegmentT))
				{
					bNotEnoughPoints = iTargetPoint + NUM_NODESSYNCED >= m_followRoute.GetNumPoints();
				}			
			}
		}
	}

	if(!bNotEnoughPoints && iTargetPoint >= 0 && pRoutePoints)
	{			
		//cache our next node
		m_syncedNode = pRoutePoints[iTargetPoint].GetNodeAddress();
		const CPathNode* pLastNode = ThePaths.FindNodePointerSafe(m_syncedNode);
		if(pLastNode)
		{
			//cache link and lane information for next two nodes
			for(int i = 0 ; i < NUM_NODESSYNCED; ++i)
			{
				int iRouteIndex = iTargetPoint + i + 1;
				if(iRouteIndex < m_followRoute.GetNumPoints())
				{
					const CPathNode* pNextNode = ThePaths.FindNodePointerSafe( pRoutePoints[iRouteIndex].GetNodeAddress());
					if(pNextNode)
					{
						s16 iLinkTemp;
						bool bLinkFound = ThePaths.FindNodesLinkIndexBetween2Nodes(pLastNode, pNextNode, iLinkTemp);
						if(bLinkFound)
						{
							m_syncedLinkAndLane[i].m_1.m_link = iLinkTemp; 
							aiAssertf(iLinkTemp < (1 << LINK_NUMBITS), "Link index greater than %d bits - won't pack correctly", LINK_NUMBITS);

							s32 iLane = rage::round( pRoutePoints[iRouteIndex-1].GetLane().Getf());
							s32 iMaxLanes = (1 << LANE_NUMBITS) - 1;
#if __BANK
							if(iLane > iMaxLanes)
							{
								CAILogManager::GetLog().Log("CRUISE MIGRATION: %s Lane index greater than %d bits - won't pack correctly\n", 
									LANE_NUMBITS, AILogging::GetEntityLocalityNameAndPointer(pVehicle).c_str());
							}
#endif
							m_syncedLinkAndLane[i].m_1.m_lane = rage::Min(iLane, iMaxLanes);							
							pLastNode = pNextNode;
							bValidData = i == NUM_NODESSYNCED - 1;
						}
					}
				}
			}
		}
	}

	if(!bValidData)
	{
		m_syncedNode.SetEmpty();
#if __BANK
		//don't log when not doing wander route - scenario vehicles will run out of points when they get to end of scenario
		if(m_pRouteSearchHelper->IsDoingWanderRoute())
		{
			CAILogManager::GetLog().Log("CRUISE MIGRATION: %s Unable to construct node list for migration. %s\n", 
				AILogging::GetEntityLocalityNameAndPointer(pVehicle).c_str(), bNotEnoughPoints ? "Not enough points at end of nodelist" : "");
		}
#endif // __BANK
	}
}

void CTaskVehicleCruiseNew::UpdateUsingWanderFallback()
{
	// If we are using a wander fallback due to route failure, let it time out,
	// or clear it if it's no longer desired.
	TUNE_GROUP_INT(NEW_CRUISE_TASK, siUsingWanderFallbackTime, 20, 0, 300, 5);
	if(m_uTimeStartedUsingWanderFallback && (!ShouldFallBackOnWanderInsteadOfStraightLine() || fwTimer::GetTimeInMilliseconds() - m_uTimeStartedUsingWanderFallback >= siUsingWanderFallbackTime*1000))
	{
		m_uTimeStartedUsingWanderFallback = 0;
	}
}

/////////////////////////////////////////////////////////////////////////////////////
#if !__FINAL
const char * CTaskVehicleCruiseNew::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_FindRoute&&iState<=State_Stop);
	static const char* aStateNames[] = 
	{
		"State_FindRoute",
		"State_CalculatingRoute",
		"State_Cruise",
		"State_StopForJunction",
		"State_WaitBeforeMovingAgain",
		"State_GoToStraightLine",
		"State_GoToNavmesh",
		"State_GetToRoadUsingNavmesh",
		"State_Burnout",
		"State_RevEngine",
		"State_PauseForUnloadedNodes",
		"State_Stop"
	};

	return aStateNames[iState];
}



// PURPOSE: Display debug information specific to this task
void CTaskVehicleCruiseNew::Debug() const
{
#if DEBUG_DRAW
	if( GetState() == State_Cruise )
	{

		// Render the point we're heading towards.
		if(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_VEHICLE_GOTO_POINT_WITH_AVOIDANCE_AUTOMOBILE)
		{
			CTaskVehicleGoToPointWithAvoidanceAutomobile * pGoToTask = (CTaskVehicleGoToPointWithAvoidanceAutomobile*)GetSubTask();
			Vector3 vTargetPosition = *pGoToTask->GetTargetPosition();
			const Vector3 vStartCoords = VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleBonnetPosition(GetVehicle(), IsDrivingFlagSet(DF_DriveInReverse)));

			Color32 destColor = DoesSubtaskThinkWeShouldBeStopped(GetVehicle(), vTargetPosition, false) ? Color_orange : Color_green;

			grcDebugDraw::Line(vStartCoords + ZAXIS, vTargetPosition + ZAXIS, destColor, destColor);
			grcDebugDraw::Sphere(vTargetPosition + ZAXIS, 0.5f, destColor, false);
			grcDebugDraw::Sphere(vStartCoords + ZAXIS, 0.5f, destColor, false);

			//Vec3V vVehicleForward = GetVehicle()->GetVehicleForwardDirection();
			//Vec3V vVehicleSteer = RotateAboutZAxis(vVehicleForward, ScalarV(GetVehicle()->GetSteerAngle()));
			//grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(vStartCoords), VECTOR3_TO_VEC3V(vStartCoords) + vVehicleSteer * ScalarV(V_FOUR), 0.5f, Color_yellow);
		}
	}	
#endif
}

#endif

void CTaskVehicleCruiseNew::CloneUpdate(CVehicle* pVehicle)
{
    CTaskVehicleMissionBase::CloneUpdate(pVehicle);

    pVehicle->m_nVehicleFlags.bTasksAllowDummying = true;
}

void CTaskVehicleCruiseNew::SetupAfterMigration(CVehicle *pVehicle)
{
    EnsureHasRouteSearchHelper();

	CVehicleNodeList pTempNodeList;

	bool bValid = true;
	//extract node information from cache and put copy into preferredNodeList which will be used to generate a new route
	if(!m_syncedNode.IsEmpty())
	{
		//first copy next node
		pTempNodeList.SetPathNodeAddr(0, m_syncedNode);

		const CPathNode* pLastNode = ThePaths.FindNodePointerSafe(m_syncedNode);
		if(pLastNode)
		{
			for(int i = 0 ; i < NUM_NODESSYNCED; ++i)
			{
				//extract our link index and lane information
				s16 linkIndex = (s16)m_syncedLinkAndLane[i].m_1.m_link;
				s8 laneIndex = (s8)m_syncedLinkAndLane[i].m_1.m_lane;

				if(linkIndex >= pLastNode->NumLinks())
				{
					bValid = false;
					break;
				}

				pTempNodeList.SetPathLaneIndex(i, laneIndex);
				pTempNodeList.SetPathLinkIndex(i, pLastNode->m_startIndexOfLinks + linkIndex);

				//find next node from link info
				const CPathNodeLink& link = ThePaths.GetNodesLink(pLastNode, linkIndex);
				const CPathNode* pNewNode = ThePaths.FindNodePointerSafe(link.m_OtherNode);
				if(!pNewNode)
				{
					bValid = false;
					break;
				}
				pTempNodeList.SetPathNodeAddr(i+1, pNewNode->m_address );
				pLastNode = pNewNode;
			}
		}
		else
		{
			bValid = false;
		}
	}
	else
	{
		bValid = false;
	}

	if(bValid)
	{
		//push our node info into preferredTargetNodeList
		pVehicle->SetStartNodes(pTempNodeList.GetPathNodeAddr(0), pTempNodeList.GetPathNodeAddr(1), pTempNodeList.GetPathLinkIndex(0), pTempNodeList.GetPathLaneIndex(0));
		SetPreferredTargetNodeList(&pTempNodeList);
	}
	else
	{
#if __BANK
		if(!m_syncedNode.IsEmpty())
		{
			CAILogManager::GetLog().Log("CRUISE MIGRATION: %s Error: to construct node list after migration. Synced node (%d:%d)\n", 
							AILogging::GetEntityLocalityNameAndPointer(pVehicle).c_str(), m_syncedNode.GetRegion(), m_syncedNode.GetIndex());
			const CPathNode* pLastNode = ThePaths.FindNodePointerSafe(m_syncedNode);
			if(pLastNode)
			{
				for(int i = 0; i < NUM_NODESSYNCED; ++i)
				{
					s16 linkIndex = (s16)m_syncedLinkAndLane[i].m_1.m_link;
					s8 laneIndex = (s8)m_syncedLinkAndLane[i].m_1.m_lane;

					if(linkIndex >= pLastNode->NumLinks())
					{
						CAILogManager::GetLog().Log("CRUISE MIGRATION: Link index for (%d:%d) out of range (%d)\n", pLastNode->GetAddrRegion(), pLastNode->GetAddrIndex(), linkIndex);
						bValid = false;
						break;
					}

					const CPathNodeLink& link = ThePaths.GetNodesLink(pLastNode, linkIndex);
					const CPathNode* pNewNode = ThePaths.FindNodePointerSafe(link.m_OtherNode);
					if(pNewNode)
					{
						CAILogManager::GetLog().Log("CRUISE MIGRATION: Linked Node: (%d:%d) Lane %d Link %d\n", pNewNode->GetAddrRegion(), pNewNode->GetAddrIndex(), laneIndex, linkIndex);
						pLastNode = pNewNode;
					}
					else
					{
						CAILogManager::GetLog().Log("CRUISE MIGRATION: Unable to find next node. Lane %d Link %d\n", laneIndex, linkIndex);
						break;
					}
				}	
			}		
		}
		else
		{
			//not always an error - scenario vehicles will run out of points, and when running navmesh won't have a valid point
			CAILogManager::GetLog().Log("CRUISE MIGRATION: %s Warning: After migration, synced node is empty, check previous owner logs for details\n", 
				AILogging::GetEntityLocalityNameAndPointer(pVehicle).c_str());
		}
#endif // __BANK

		//we don't have full data in cached nodes, so fall back to just copying first two nodes and current lane and hope for the best
		const CVehicleNodeList& nodeList = static_cast<CNetObjVehicle*>(pVehicle->GetNetworkObject())->GetVehicleNodeList();
		s32 iPathIndex = nodeList.GetTargetNodeIndex();
		iPathIndex = rage::Max(iPathIndex - 1, 0);
	
		if(iPathIndex < nodeList.FindNumPathNodes() - 1)
		{
			pVehicle->SetStartNodes(nodeList.GetPathNodeAddr(iPathIndex), nodeList.GetPathNodeAddr(iPathIndex + 1), nodeList.GetPathLinkIndex(iPathIndex), nodeList.GetPathLaneIndex(iPathIndex));
			//SetPreferredTargetNodeList(&nodeList); //don't want to set this as then full cloned node list will be copied
		}
	}

    CopyNodeList(&static_cast<CNetObjVehicle*>(pVehicle->GetNetworkObject())->GetVehicleNodeList());

    CTaskVehicleMissionBase::SetupAfterMigration(pVehicle);
}

/////////////////////////////////////////////////////////////////
CTaskVehicleGoToAutomobileNew::CTaskVehicleGoToAutomobileNew(const sVehicleMissionParams& params 
	,const float fStraightLineDist /* = sVehicleMissionParams::DEFAULT_SWITCH_TO_STRAIGHT_LINE_DIST */
	,const bool bSpeedComesFromVehPopulation /*= false*/)
: CTaskVehicleCruiseNew(params, bSpeedComesFromVehPopulation)
{
	m_fStraightLineDist = fStraightLineDist;
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_GOTO_AUTOMOBILE_NEW);

	Assertf(params.m_fCruiseSpeed >= 0.0f, "Desired cruise speed is negative! Use DF_DriveInReverse instead");
}

CTaskVehicleGoToAutomobileNew::~CTaskVehicleGoToAutomobileNew()
{
}

aiTask::FSM_Return CTaskVehicleGoToAutomobileNew::ProcessPreFSM()
{
	//if we don't have a target, consider this task done
	if (!m_Params.IsTargetValid())
	{
#if 0
		if(!m_bDontAssertIfTargetInvalid)
		{
			taskAssertf(false, "CTaskVehicleGoToAutomobileNew with no target. Here's the driver's task tree--one of these tasks should have handled this");
			if (GetVehicle() && GetVehicle()->GetDriver())
			{
				GetVehicle()->GetDriver()->GetPedIntelligence()->PrintTasks();
			}
		}
#endif //__ASSERT
		return FSM_Quit;
	}

	if (ProcessPreFSM_Shared() == FSM_Quit)
	{
		return FSM_Quit;
	}

	if (IsDrivingFlagSet(DF_AdjustCruiseSpeedBasedOnRoadSpeed))
	{
		UpdateSpeedMultiplier(GetVehicle());
	}

	return FSM_Continue;
}

void CTaskVehicleCruiseNew::SetPreferredTargetNodeList(const CVehicleNodeList* listToCopyFrom)
{
	if(listToCopyFrom)
	{
		Assert(m_PreferredTargetNodeList != listToCopyFrom);	// This probably wouldn't be good.

		// Allocate a list if we don't have one already.
		if(!m_PreferredTargetNodeList)
		{
			m_PreferredTargetNodeList = rage_new CVehicleNodeList;
		}
		if(m_PreferredTargetNodeList)
		{
			sysMemCpy(m_PreferredTargetNodeList, listToCopyFrom, sizeof(CVehicleNodeList));
		}
	}
	else
	{
		delete m_PreferredTargetNodeList;
		m_PreferredTargetNodeList = NULL;
	}
}

void CTaskVehicleCruiseNew::CopyNodeList(const CVehicleNodeList * pNodeList)
{
	if(pNodeList && m_pRouteSearchHelper)
	{
		//Assert(m_pNodeList);
		sysMemCpy(&m_pRouteSearchHelper->GetNodeList(), pNodeList, sizeof(CVehicleNodeList));
	}
}

#if !__FINAL
void CTaskVehicleGoToAutomobileNew::Debug() const
{
#if DEBUG_DRAW
	Vector3 vTargetPos;
	FindTargetCoors(GetVehicle(), vTargetPos);

	grcDebugDraw::Arrow(GetVehicle()->GetVehiclePosition(), VECTOR3_TO_VEC3V(vTargetPos), 1.0f, Color_red2);

	if(m_uTimeStartedUsingWanderFallback)
	{
		grcDebugDraw::Text(GetVehicle()->GetVehiclePosition(), Color_orange, 0, -60, "Fallback Wander", false, -1);
	}
#endif //__BANK

	CTaskVehicleCruiseNew::Debug();
}

#endif //!__FINAL

u32 CTaskVehicleGoToAutomobileNew::GetDefaultPathSearchFlags() const
{
	u32 retVal = 0;
	if (IsDrivingFlagSet(DF_UseSwitchedOffNodes))
	{
		retVal |= CPathNodeRouteSearchHelper::Flag_UseSwitchedOffNodes;
	}
	if (IsDrivingFlagSet(DF_UseShortCutLinks))
	{
		retVal |= CPathNodeRouteSearchHelper::Flag_UseShortcutLinks;
	}
	if (IsDrivingFlagSet(DF_DriveIntoOncomingTraffic))
	{
		retVal |= CPathNodeRouteSearchHelper::Flag_DriveIntoOncomingTraffic;
	}
	if (IsDrivingFlagSet(DF_AvoidHighways))
	{
		retVal |= CPathNodeRouteSearchHelper::Flag_AvoidHighways;
	}

	const CVehicle* pVehicle = GetVehicle();
	static dev_float s_fVelocityCutoff = 0.25f;
	const bool bPreventJoiningInRoadDirectionForSpeed = IsDrivingFlagSet(DF_PreventJoinInRoadDirectionWhenMoving);
	if ((!bPreventJoiningInRoadDirectionForSpeed && pVehicle && pVehicle->GetAiXYSpeed() > s_fVelocityCutoff)
		|| IsDrivingFlagSet(DF_ForceJoinInRoadDirection))
	{
		retVal |= CPathNodeRouteSearchHelper::Flag_JoinRoadInDirection;
	}

	//if this is a mission vehicle and the destination is within the streaming distance,
	//assume that the request has already been made and wait for nodes to be loaded
	if (pVehicle && CPathFind::IsVehicleTypeWhichCanStreamInNodes(pVehicle)
		&& CPathFind::IsMissionVehicleThatCanStreamInNodes(pVehicle))
	{
		Vector3 vTargetCoords;
		FindTargetCoors(pVehicle, vTargetCoords);
		const Vec2V vTargetCoors2D(vTargetCoords.x, vTargetCoords.y);

		const ScalarV scDistSqrToTarget = DistSquared(pVehicle->GetVehiclePosition().GetXY(), vTargetCoors2D);
		const float fStreamingRadius = pVehicle->GetIntelligence()->GetCustomPathNodeStreamingRadius() > 0.0f 
			? rage::Max(pVehicle->GetIntelligence()->GetCustomPathNodeStreamingRadius(), STREAMING_PATH_NODES_DIST)
			: STREAMING_PATH_NODES_DIST;
		static const ScalarV scMissionVehLoadNodesDistSqr(fStreamingRadius * fStreamingRadius);
		if (IsLessThanAll(scDistSqrToTarget, scMissionVehLoadNodesDistSqr))
		{
			retVal |= CPathNodeRouteSearchHelper::Flag_AllowWaitForNodesToLoad;
		}
	}

	return retVal;
}

bool CTaskVehicleGoToAutomobileNew::CheckForStateChangeDueToDistanceToTarget(CVehicle *pVehicle)
{
	//Find the target position.
	Vector3 vTargetPos;
	FindTargetCoors(pVehicle, vTargetPos);
		
	//should we be switching to goto straightline?
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
					SetState(CTaskVehicleCruiseNew::State_Stop);
					return true;
				}
			}

		//now check to see if we should switch to a straight line task.
		if (distance <= m_fStraightLineDist)
		{
			SetState(CTaskVehicleCruiseNew::State_GoToStraightLine);
			return true;
		}
	}

	if(IsDrivingFlagSet(DF_ForceStraightLine))
	{
		SetState(CTaskVehicleCruiseNew::State_GoToStraightLine);
		return true;
	}

	return false; //no state change this time.
}

bool CTaskVehicleGoToAutomobileNew::ShouldFallBackOnWanderInsteadOfStraightLine() const
{
	return IsDrivingFlagSet(DF_UseWanderFallbackInsteadOfStraightLine);
}
