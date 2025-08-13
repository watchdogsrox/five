//
// task/Scenario/Types/TaskVehicleScenario.cpp
//
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#include "Network/NetworkInterface.h"
#include "Peds/PedIntelligence.h"
#include "Scene/world/GameWorldHeightMap.h"
#include "task/Scenario/Types/TaskVehicleScenario.h"
#include "task/Scenario/info/ScenarioInfoManager.h"
#include "task/scenario/ScenarioPointManager.h"
#include "task/Vehicle/TaskCar.h"
#include "vehicleAi/task/TaskVehicleCruise.h"
#include "vehicleAi/task/TaskVehicleFlying.h"
#include "vehicleAi/task/TaskVehicleGoToAutomobile.h"
#include "vehicleAi/task/TaskVehicleGoToHelicopter.h"
#include "vehicleAi/task/TaskVehicleGoToPlane.h"
#include "vehicleAi/task/TaskVehicleLandPlane.h"
#include "vehicleAi/task/TaskVehiclePark.h"
#include "vehicleAi/task/TaskVehicleTempAction.h"
#include "Vehicles/Heli.h"
#include "Vehicles/Planes.h"
#include "Vehicles/VehicleDefines.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

//-----------------------------------------------------------------------------

CTaskUseVehicleScenario::Tunables CTaskUseVehicleScenario::sm_Tunables;

IMPLEMENT_SCENARIO_TASK_TUNABLES(CTaskUseVehicleScenario, 0x165391c9);

CTaskUseVehicleScenario::CTaskUseVehicleScenario(
		s32 iScenarioIndex, CScenarioPoint* pScenarioPoint,
		u32 flags, float driveSpeed, CScenarioPoint* pPrevScenarioPoint)
		: CTaskScenario(iScenarioIndex, pScenarioPoint)
		, m_DriveSpeed(driveSpeed)
		, m_pScenarioPointPrev(pPrevScenarioPoint)
		, m_DriveSpeedNxt(0.0f)
		, m_IdleTime(0.0f)
		, m_Flags(flags)
		, m_Parking(false)
		, m_SlowingDown(false)
		, m_bDriveSubTaskCreationFailed(false)
		, m_iCachedPrevWorldScenarioPointIndex(-1)
		, m_iCachedNextWorldScenarioPointIndex(-1)
{
	SetInternalTaskType(CTaskTypes::TASK_USE_VEHICLE_SCENARIO);
}


CTaskUseVehicleScenario::CTaskUseVehicleScenario(const CTaskUseVehicleScenario& other)
		: CTaskScenario(other.m_iScenarioIndex, other.GetScenarioPoint())
		, m_DriveSpeed(other.m_DriveSpeed)
		, m_IdleTime(other.m_IdleTime)
		, m_Flags(other.m_Flags)
		, m_Parking(other.m_Parking)
		, m_SlowingDown(other.m_SlowingDown)
		, m_bDriveSubTaskCreationFailed(false)	// Probably doesn't make sense to copy.
		, m_DriveSpeedNxt(other.m_DriveSpeedNxt)
		, m_pScenarioPointPrev(other.m_pScenarioPointPrev)
		, m_iCachedPrevWorldScenarioPointIndex(-1)
		, m_iCachedNextWorldScenarioPointIndex(-1)
{
	SetInternalTaskType(CTaskTypes::TASK_USE_VEHICLE_SCENARIO);
}

#if !__FINAL

const char * CTaskUseVehicleScenario::GetStaticStateName( s32 iState )
{
	static const char* s_StateNames[] =
	{
		"Start",
		"DrivingToScenario",
		"UsingScenario",
		"Finish",
		"Failed"
	};
	CompileTimeAssert(NELEM(s_StateNames) == kNumStates);

	if(taskVerifyf(iState >= 0 && iState < kNumStates, "Invalid state %d.", iState))
	{
		return s_StateNames[iState];
	}
	else
	{
		return "?";
	}
}

#endif // !__FINAL

CTask::FSM_Return CTaskUseVehicleScenario::ProcessPreFSM()
{
	CPed* pPed = GetPed();
	if(!pPed || !pPed->GetVehiclePedInside())
	{
		return FSM_Quit;
	}

	if(!GetScenarioPoint())
	{
		return FSM_Quit;
	}

    // peds doing in vehicle scenarios must be synced in the network game, but peds in cars are not
    // synchronised by default, in order to save bandwidth as they don't do much
    if(NetworkInterface::IsGameInProgress())
    {
        NetworkInterface::ForceSynchronisation(pPed);
    }

	return FSM_Continue;
}


CTask::FSM_Return CTaskUseVehicleScenario::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_DrivingToScenario)
			FSM_OnEnter
				DrivingToScenario_OnEnter();
			FSM_OnUpdate
				return DrivingToScenario_OnUpdate();

		FSM_State(State_UsingScenario)
			FSM_OnEnter
				UsingScenario_OnEnter();
			FSM_OnUpdate
				return UsingScenario_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnEnter
				FinishScenario_OnEnter();
			FSM_OnUpdate
				return FinishScenario_OnUpdate();

		FSM_State(State_Failed)
			FSM_OnEnter
				Failed_OnEnter();
			FSM_OnUpdate
				return Failed_OnUpdate();

	FSM_End
}


void CTaskUseVehicleScenario::CleanUp()
{
	CPed* pPed = GetPed();
	if (pPed)
	{
		pPed->GetPedIntelligence()->GetEventScanner()->ScanForEventsNow(*pPed, CEventScanner::VEHICLE_POTENTIAL_COLLISION_SCAN);

		CVehicle* pVeh = GetPed()->GetVehiclePedInside();
		if ( pVeh )
		{
			if ( pVeh->InheritsFromHeli() || pVeh->InheritsFromPlane() )
			{
				pVeh->m_nVehicleFlags.bDisableHeightMapAvoidance = false;
			}
			else if (pVeh->InheritsFromBoat())
			{
				pVeh->GetIntelligence()->GetBoatAvoidanceHelper()->SetMaxAvoidanceOrientationLeft(-CBoatAvoidanceHelper::ms_DefaultMaxAvoidanceOrientation);
				pVeh->GetIntelligence()->GetBoatAvoidanceHelper()->SetMaxAvoidanceOrientationRight(CBoatAvoidanceHelper::ms_DefaultMaxAvoidanceOrientation);
			}
		}
	}
}


s32 CTaskUseVehicleScenario::GetDefaultStateAfterAbort() const
{
	return State_Start;	// Not sure
}


CTask::FSM_Return CTaskUseVehicleScenario::Start_OnUpdate()
{
	if(m_Flags.IsFlagSet(VSF_HasFinishedChain))
	{
		SetState(State_Finish);
	}
	else
	{
		SetState(State_DrivingToScenario);
	}

	return FSM_Continue;
}


void CTaskUseVehicleScenario::DrivingToScenario_OnEnter()
{
	m_SlowingDown = false;
	m_Parking = false;

	m_bDriveSubTaskCreationFailed = !CreateDriveSubTask(m_DriveSpeed);

	// When driving to a scenario, make sure we check more carefully if it's an uneven
	// surface, and make use of additional virtual road test points to reduce misalignment.
	// Scenario cars tend to drive in places where this may help.
	// Note: there is currently nothing resetting this. This is intentional since it's possible
	// that the driver gets destroyed after this task ends, but we may still be in an unusual
	// spot while joining the road network. A timer may be a good idea, but doesn't feel necessary
	// since the cost shouldn't be all that much higher. 
	//@@: location CTASKUSEVEHICLESCENARIO_DRIVINGTOSCENARIO_ONENTER
	CVehicle* pVeh = GetPed()->GetVehiclePedInside();
	if(pVeh)
	{
		pVeh->m_nVehicleFlags.bMoreAccurateVirtualRoadProbes = true;
	}
}

void CTaskUseVehicleScenario::ConsiderTellingOtherCarsToPassUs()
{
	//hack! if we're a bicycle, tell other cars they can pass us.
	CPed* pPed = GetPed();
	aiAssert(pPed);
	CVehicle* pVeh = pPed->GetVehiclePedInside();
	aiAssert(pVeh);
	if (pVeh && pVeh->GetVehicleType() == VEHICLE_TYPE_BICYCLE)
	{
		pVeh->m_nVehicleFlags.bForceOtherVehiclesToOvertakeThisVehicle = true;
	}
}

CTask::FSM_Return CTaskUseVehicleScenario::DrivingToScenario_OnUpdate()
{
	//if we failed to create a drive task, don't do any of the stuff below until we have one 
	if (m_bDriveSubTaskCreationFailed)
	{
		if (!ShouldVehiclePauseDueToUnloadedNodes())
		{
			SetFlag(aiTaskFlags::RestartCurrentState);
		}
		//return anyway so the task will get created and run on the next frame
		return FSM_Continue;
	}

	bool done = false;
	if(GetIsSubtaskFinished(CTaskTypes::TASK_CONTROL_VEHICLE))
	{
		done = true;
	}
	else
	{
		const CScenarioVehicleInfo& info = GetScenarioInfo();
		if (!info.GetIsClass<CScenarioVehicleParkInfo>() && !GetScenarioPoint()->IsFlagSet(CScenarioPointFlags::LandVehicleOnArrival)) // when parking/landing we want the vehicle to finish at the desired spot... 
		{
			// Theoretically, the goto task would do this already and finish
			// upon getting close enough to the destination, and then we would detect
			// that above. In practice, I didn't have much success with this, it seems
			// that that way there is a glitch of one or more frames during which brakes
			// are applied, resulting in unsmooth movement.
			const ScalarV distSqV = ComputeDistSqToTarget();
			if ( HasCrossedArrivalPlane() )
			{
				done = true;
			}
			else if (HasPlaneTaxiedToArrivalPosition(distSqV))
			{
				done = true;
			}
			else 
			{
				const ScalarV distThresholdSqV(square(sm_Tunables.m_TargetArriveDist));
				if(IsLessThanAll(distSqV, distThresholdSqV))
				{
					done = true;
				}
			}
		}
	}

	ConsiderTellingOtherCarsToPassUs();

	CPed* pPed = GetPed();
	if(m_Parking)
	{
		// Check if the parking task failed. If so, consider the whole scenario (chain) a failure.
		CVehicle* pVeh = pPed->GetVehiclePedInside();
		if(pVeh)
		{
			const CTask* pRunningVehicleTask = static_cast<const CTask*>(pVeh->GetIntelligence()->GetTaskManager()->GetActiveTask(VEHICLE_TASK_TREE_PRIMARY));
			if(pRunningVehicleTask && pRunningVehicleTask->GetTaskType() == CTaskTypes::TASK_VEHICLE_PARK_NEW)
			{
				const CTaskVehicleParkNew* pParkingTask = static_cast<const CTaskVehicleParkNew*>(pRunningVehicleTask);
				if(pParkingTask->GetState() == CTaskVehicleParkNew::State_Failed)
				{
					SetState(State_Failed);
					return FSM_Continue;
				}
			}
		}
	}

	if(done)
	{
		if(ShouldStopAtDestination())
		{
			SetState(State_UsingScenario);
		}
		else
		{
			SetState(State_Finish);
		}
		return FSM_Continue;
	}

	// If we are trying to park, we probably shouldn't do any of the stuff below,
	// since we don't pass in a speed to that task anyway. In particular, we don't want
	// to create new parking tasks all of the time!
	if(m_Parking)
	{
		return FSM_Continue;
	}

	//update followroute
	const CScenarioPoint* pt = GetScenarioPoint();
	aiAssert(pt);
	aiAssert(pPed);
	CVehicle* pVeh = pPed->GetVehiclePedInside();
	aiAssert(pVeh);
	float curspeed = pVeh->GetVelocity().Mag();
	float bespeed = 0.0f;
	//Build routePoints array
	static const int iMaxRoutePoints = 3 + CVehicleIntelligence::ms_iNumScenarioRoutePointsToSave;
	CRoutePoint CurrentScenarioRoutePoints[iMaxRoutePoints];
	CVehicleFollowRouteHelper tempFollowRoute;	//this gets passed into vehicleintelligence with the results
	const CRoutePoint*	pScenarioPointHistory = pVeh->GetIntelligence()->GetScenarioRoutePointHistory();
	const int	iNumScenarioHistoryPoints = pVeh->GetIntelligence()->m_iNumScenarioRouteHistoryPoints;

	//add all the old points
	for (int i = 0; i < iNumScenarioHistoryPoints; i++)
	{
		Assert(pScenarioPointHistory);
		CurrentScenarioRoutePoints[i] = pScenarioPointHistory[i];
	}

	int numCurrentScenarioPoints = 2;
	{
		Vec::Vector_4V zero_4 = Vec::V4VConstant(V_ZERO);

		//point we are going from - our current position
		const bool bDrivingInReverse = false;

		//here, we want to use the bonnet position no matter what, because it's just being used
		//to construct a followroute, which are ignored for non-cars anyway
		//we also want to get the bottom of the car, so the constructed virtual road
		//will be beneath the car's wheels
		const Vec3V vFirstRoutePointPos = m_pScenarioPointPrev.Get() 
			? m_pScenarioPointPrev->GetPosition()
			: CVehicleFollowRouteHelper::GetVehicleBonnetPosition(pVeh, bDrivingInReverse, true);
		CurrentScenarioRoutePoints[iNumScenarioHistoryPoints].m_vPositionAndSpeed = Vec4V(vFirstRoutePointPos);
		CurrentScenarioRoutePoints[iNumScenarioHistoryPoints].SetSpeed(curspeed);
		CurrentScenarioRoutePoints[iNumScenarioHistoryPoints].ClearLaneCenterOffsetAndNodeAddrAndLane();
		CurrentScenarioRoutePoints[iNumScenarioHistoryPoints].m_vParamsAsVector = zero_4;

		//point we are going to
		CurrentScenarioRoutePoints[iNumScenarioHistoryPoints+1].m_vPositionAndSpeed = Vec4V(pt->GetPosition());
		CurrentScenarioRoutePoints[iNumScenarioHistoryPoints+1].SetSpeed(m_DriveSpeed);
		CurrentScenarioRoutePoints[iNumScenarioHistoryPoints+1].ClearLaneCenterOffsetAndNodeAddrAndLane();
		CurrentScenarioRoutePoints[iNumScenarioHistoryPoints+1].m_vParamsAsVector = zero_4;

#if __ASSERT
		Assertf(m_pScenarioPointPrev.Get() != pt, "Previous and current scenario point pointers are the same, at (%.2f, %.2f, %.2f). Might be a syncing issue if this is in MP.",
				CurrentScenarioRoutePoints[iNumScenarioHistoryPoints+1].m_vPositionAndSpeed.GetXf(), CurrentScenarioRoutePoints[iNumScenarioHistoryPoints+1].m_vPositionAndSpeed.GetYf(), CurrentScenarioRoutePoints[iNumScenarioHistoryPoints+1].m_vPositionAndSpeed.GetZf());
		if (m_pScenarioPointPrev.Get() && m_pScenarioPointPrev.Get() != pt && !IsGreaterThanAll(Dist(CurrentScenarioRoutePoints[iNumScenarioHistoryPoints].GetPosition(), CurrentScenarioRoutePoints[iNumScenarioHistoryPoints+1].GetPosition()), ScalarV(V_FLT_EPSILON)))
		{
			Assertf(0, "Duplicate scenario points at (%.2f, %.2f, %.2f), please bug this."
				, vFirstRoutePointPos.GetXf(), vFirstRoutePointPos.GetYf(), vFirstRoutePointPos.GetZf());
		}
#endif //__ASSERT

		//handle this case properly by moving the 0th point if it's identical to the first
		if (!IsGreaterThanAll(Dist(CurrentScenarioRoutePoints[iNumScenarioHistoryPoints].GetPosition(), CurrentScenarioRoutePoints[iNumScenarioHistoryPoints+1].GetPosition()), ScalarV(V_FLT_EPSILON)))
		{	
			CurrentScenarioRoutePoints[iNumScenarioHistoryPoints].m_vPositionAndSpeed = Vec4V(CVehicleFollowRouteHelper::GetVehicleBonnetPosition(pVeh, !bDrivingInReverse, true));
		}

		//point we are going to next - if present
		if (m_pScenarioPointNxt)
		{
			CurrentScenarioRoutePoints[iNumScenarioHistoryPoints+2].m_vPositionAndSpeed = Vec4V(m_pScenarioPointNxt->GetPosition());
			CurrentScenarioRoutePoints[iNumScenarioHistoryPoints+2].SetSpeed(m_DriveSpeedNxt);
			CurrentScenarioRoutePoints[iNumScenarioHistoryPoints+2].ClearLaneCenterOffsetAndNodeAddrAndLane();
			CurrentScenarioRoutePoints[iNumScenarioHistoryPoints+2].m_vParamsAsVector = zero_4;
			++numCurrentScenarioPoints;
		}
		

		//as a side effect, this function will copy over the nodelist
		//to pVeh's VehicleIntelligence
		tempFollowRoute.ConstructFromRoutePointArray(pVeh, CurrentScenarioRoutePoints, iNumScenarioHistoryPoints+numCurrentScenarioPoints, iNumScenarioHistoryPoints+1);
	}

	// Consider slowing down if we're not doing that already.
	if(!m_SlowingDown && ShouldSlowDown())
	{
		// Note: would be good to change the speed of the existing task, but it looks like
		// it would take some effort to pass it down to all relevant parts of the task
		// hierarchy so for now we are creating a new task instead.
		UpdateDriveSubTask(m_DriveSpeed);
	}
    else if (m_pScenarioPointNxt)//if i have a point i am continuing to drive toward beyond my next point ... 
	{
		//Calculate the speeds
		Assert(numCurrentScenarioPoints >= 3);
		CRoutePoint* pFinalRoutePoints = const_cast<CRoutePoint*>(&tempFollowRoute.GetRoutePoints()[0]);
		CVehicleFollowRouteHelper::CalculateDesiredSpeeds(pVeh, false, pFinalRoutePoints, tempFollowRoute.GetNumPoints());

		Assert(tempFollowRoute.GetCurrentTargetPoint() >= 0 && tempFollowRoute.GetCurrentTargetPoint() < tempFollowRoute.GetNumPoints());
		bespeed = tempFollowRoute.GetRoutePoints()[tempFollowRoute.GetCurrentTargetPoint()].GetSpeed(); //want to get the speed for the point we are going too... 

		// Probably shouldn't let it go above our desired speed.
		bespeed = Min(bespeed, m_DriveSpeed);

		aiAssertf((rage::Abs(bespeed) < DEFAULT_MAX_SPEED), "Cannot set the forward speed greater than %f m/s", DEFAULT_MAX_SPEED);

		UpdateDriveSubTask(bespeed);
	}

	return FSM_Continue;
}

void CTaskUseVehicleScenario::SetScenarioPointNxt(CScenarioPoint* point, float driveSpeed)
{
	m_DriveSpeedNxt = driveSpeed;
	m_pScenarioPointNxt = point;

	m_iCachedNextWorldScenarioPointIndex = -1; 
}

const CConditionalAnimsGroup* CTaskUseVehicleScenario::GetScenarioConditionalAnimsGroup() const {
	return GetScenarioInfo().GetClipData(); 
}

void CTaskUseVehicleScenario::UsingScenario_OnEnter()
{
	const float timeToUse = GetBaseTimeToUse();

	// If the scenario time is set to >= 0.0, make sure that when randomised it doesn't
	// go negative, leaving the ped stationary indefinitely.
	if(timeToUse > 0.0f)
	{
		const float fRandom = timeToUse*sm_Tunables.m_IdleTimeRandomFactor;
		m_IdleTime = Max(timeToUse + fwRandom::GetRandomNumberInRange(-fRandom, fRandom), 0.0f);
	}
	else
	{
		m_IdleTime = timeToUse;
	}

	CPed* pPed = GetPed();
	CVehicle* pVeh = pPed->GetVehiclePedInside();


	aiTask* pCarTask = rage_new CTaskVehicleStop(CTaskVehicleStop::SF_DontTerminateWhenStopped | CTaskVehicleStop::SF_EnableTimeslicingWhenStill);

	aiTask* pSubTask = rage_new CTaskInVehicleBasic(pVeh, false, GetScenarioInfo().GetClipData());
	if(pCarTask)
	{
		SetNewTask(rage_new CTaskControlVehicle(pVeh, pCarTask, pSubTask));
	}
	else
	{
		SetNewTask(pSubTask);
	}
}


CTask::FSM_Return CTaskUseVehicleScenario::UsingScenario_OnUpdate()
{
	if(m_IdleTime >= 0.0f && GetTimeInState() > m_IdleTime)
	{
		return FSM_Quit;
	}

	ConsiderTellingOtherCarsToPassUs();

	return FSM_Continue;
}

void CTaskUseVehicleScenario::FinishScenario_OnEnter()
{
	if ( GetScenarioPoint() )
	{
		CVehicle* pVeh = GetPed()->GetVehiclePedInside();
		if ( pVeh )
		{
			if ( pVeh->InheritsFromHeli() || pVeh->InheritsFromPlane() )
			{
				if (GetScenarioPoint()->IsFlagSet(CScenarioPointFlags::FlyOffToOblivion))
				{
					sVehicleMissionParams params;
					params.m_fCruiseSpeed = Max(40.0f, pVeh->pHandling->m_fEstimatedMaxFlatVel * 0.9f);
					params.SetTargetPosition(VEC3V_TO_VECTOR3(pVeh->GetTransform().GetPosition()));
					params.m_fTargetArriveDist = -1.0f;
					params.m_iDrivingFlags.SetFlag(DF_AvoidTargetCoors);


					int iFlightHeight = CTaskVehicleFleeAirborne::ComputeFlightHeightAboveTerrain(*pVeh, 100);					
					aiTask* pVehicleTask = rage_new CTaskVehicleFleeAirborne(params, iFlightHeight, iFlightHeight, false, 0);
					aiTask* pSubTask = rage_new CTaskInVehicleBasic(pVeh);
					SetNewTask(rage_new CTaskControlVehicle(pVeh, pVehicleTask, pSubTask));

					pVeh->SetAllowFreezeWaitingOnCollision(false);
					pVeh->m_nVehicleFlags.bShouldFixIfNoCollision = false;

					pVeh->SwitchEngineOn(true);
					pVeh->m_nVehicleFlags.bDisableHeightMapAvoidance = false;
					m_Flags.SetFlag(VSF_HasFinishedChain);
				}
			}
		}
	}
}

CTask::FSM_Return CTaskUseVehicleScenario::FinishScenario_OnUpdate()
{

	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished) )
	{
		return FSM_Quit;
	}
	return FSM_Continue;
}


void CTaskUseVehicleScenario::Failed_OnEnter()
{
}


CTask::FSM_Return CTaskUseVehicleScenario::Failed_OnUpdate()
{
	// Note: perhaps we should try to quit here instead?
	//	return FSM_Quit;
	return FSM_Continue;
}


const CScenarioVehicleInfo& CTaskUseVehicleScenario::GetScenarioInfo() const
{
	const int scenarioType = GetScenarioType();
	const CScenarioInfo* pInfo = SCENARIOINFOMGR.GetScenarioInfoByIndex(scenarioType);
	taskAssertf(pInfo, "Failed to find scenario info for type %d.", scenarioType);
	taskAssertf(pInfo->GetIsClass<CScenarioVehicleInfo>(), "Scenario info type %d is not a CScenarioVehicleInfo.", scenarioType);
	return *static_cast<const CScenarioVehicleInfo*>(pInfo);
}


float CTaskUseVehicleScenario::GetBaseTimeToUse() const
{
	const CScenarioVehicleInfo& info = GetScenarioInfo();

	float timeTillPedLeaves = info.GetTimeTillPedLeaves();
	const CScenarioPoint* pScenarioPoint = GetScenarioPoint();
	if(pScenarioPoint)
	{
		if(pScenarioPoint->HasTimeTillPedLeaves())
		{
			timeTillPedLeaves = pScenarioPoint->GetTimeTillPedLeaves();
		}
	}

	return timeTillPedLeaves;
}

ScalarV_Out CTaskUseVehicleScenario::ComputeDistSqToTarget() const
{
	const CPed* pPed = GetPed();
	CVehicle* pVeh = pPed->GetVehiclePedInside();
	const CScenarioPoint* pt = GetScenarioPoint();

	const Vec3V scenarioPosV = pt->GetPosition();

	//TODO: if we ever support driving in reverse for scenario chains,
	//we'll have to connect it here
	const bool bDrivingInReverse = false;
	const Vec3V vehiclePosV = ShouldUseBonnetPositionForDistanceChecks() ? CVehicleFollowRouteHelper::GetVehicleBonnetPosition(pVeh, bDrivingInReverse) :
		pVeh->GetVehiclePosition();
	
	const Vec3V vDiff = scenarioPosV - vehiclePosV;
	// for taxi planes...determine arrival distance based on xy distance
	// with huge jets, they can be right on top of the point and still not arrive because of their significant height
	const Vec3V vDiffMask = (pt->IsFlagSet(CScenarioPointFlags::TaxiPlaneOnGround)) ? Vec3V(V_MASKXY) : Vec3V(V_MASKXYZ); 
	const Vec3V vDiffMasked = And(vDiff, vDiffMask);
	const ScalarV distSqV = MagSquared(vDiffMasked);
	return distSqV;
}

bool CTaskUseVehicleScenario::HasCrossedArrivalPlane() const
{
	const CScenarioPoint* pScenarioPointCurrent = GetScenarioPoint();
	
	if ( pScenarioPointCurrent )
	{
		if (pScenarioPointCurrent->IsFlagSet(CScenarioPointFlags::CheckCrossedArrivalPlane))
		{
			if ( m_pScenarioPointPrev )
			{
				const CPed* pPed = GetPed();
				CVehicle* pVeh = pPed->GetVehiclePedInside();
				const Vec3V vCurrentPosV = pScenarioPointCurrent->GetPosition();		
				const Vec3V vPreviousPosV = m_pScenarioPointPrev->GetPosition();		
				const Vec3V vVehiclePosV = pVeh->GetTransform().GetPosition();

				Vec3V vSegmentDir = Normalize(vCurrentPosV - vPreviousPosV);
				Vec3V vToTargetDir = Normalize(vCurrentPosV - vVehiclePosV);

				if ( (Dot(vToTargetDir, vSegmentDir) <= ScalarV(V_ZERO)).Getb() )
				{
					return true;
				}
			}
		}
	}
	
	return false;
}

bool CTaskUseVehicleScenario::HasPlaneTaxiedToArrivalPosition(ScalarV_In distSqToTarget) const
{
	ScalarV arriveDistV;
	const CVehicle* pVeh = GetPed()->GetVehiclePedInside();
	if (pVeh && pVeh->InheritsFromPlane())
	{
		if (GetScenarioPoint()->IsFlagSet(CScenarioPointFlags::TaxiPlaneOnGround))
		{
			arriveDistV = ScalarV(sm_Tunables.m_PlaneTargetArriveDistTaxiOnGround);
		}
		else
		{
			arriveDistV = ScalarV(sm_Tunables.m_PlaneTargetArriveDist);
		}
		BoolV_Out distIsLess = IsLessThanOrEqual(distSqToTarget, arriveDistV * arriveDistV);
		return distIsLess.Getb();
	}
	return false;
}

bool CTaskUseVehicleScenario::ShouldSlowDown() const
{
	// To slow down, we must either want to stop at the destination to do something
	// there, or we must not have somewhere else to go.
	if(!ShouldStopAtDestination() && m_Flags.IsFlagSet(VSF_WillContinueDriving))
	{
		return false;
	}

	// Check if our regular speed is actually larger than what we are considering
	// slowing down to.
	if(m_DriveSpeed > sm_Tunables.m_SlowDownSpeed)
	{
		// Check if close enough to the destination that we should slow down.
		const ScalarV distSqV = ComputeDistSqToTarget();
		const ScalarV distThresholdSqV(square(sm_Tunables.m_SlowDownDist));
		if(IsLessThanAll(distSqV, distThresholdSqV))
		{
			return true;
		}
	}
	return false;
}


bool CTaskUseVehicleScenario::ShouldStopAtDestination() const
{
	// If the base time is positive, we're stopping for that time. If the base
	// time is negative, we're stopping forever. The only time when we don't
	// stop is when the time is zero.
	float timeToUse = GetBaseTimeToUse();
	return timeToUse != 0.0f;
}

bool CTaskUseVehicleScenario::ShouldVehiclePauseDueToUnloadedNodes() const
{
	//if we want to use roads, make sure they're loaded, otherwise fail creation
	if (m_Flags.IsFlagSet(VSF_NavUseRoads))
	{
		const CScenarioPoint* pt = GetScenarioPoint();
		aiAssert(pt);
		const CPed* pPed = GetPed();
		aiAssert(pPed);
		const CVehicle* pVeh = pPed->GetVehiclePedInside();
		aiAssert(pVeh);

		const Vector3 vStart = VEC3V_TO_VECTOR3(pVeh->GetVehiclePosition());
		const Vector3 vDest = VEC3V_TO_VECTOR3(pt->GetPosition());
		const float fRoadNodesMinX = rage::Min(vStart.x, vDest.x);
		const float fRoadNodesMaxX = rage::Max(vStart.x, vDest.x);
		const float fRoadNodesMinY = rage::Min(vStart.y, vDest.y);
		const float fRoadNodesMaxY = rage::Max(vStart.y, vDest.y);

#if __ASSERT
		const float fDist2dSqr = (vStart - vDest).XYMag2();
		Assertf(fDist2dSqr < STREAMING_PATH_NODES_DIST * STREAMING_PATH_NODES_DIST, "Scenario point %s at (%.2f, %.2f, %.2f) placed too far apart from previous \
		point in chain, this may lead to pathfinding failure. Try to keep them within %.2f of each other", pt->GetScenarioName()
		, vDest.x, vDest.y, vDest.z, STREAMING_PATH_NODES_DIST);
#endif

		if (!ThePaths.AreNodesLoadedForArea(fRoadNodesMinX, fRoadNodesMaxX, fRoadNodesMinY, fRoadNodesMaxY))
		{
			return true;
		}
	}

	return false;
}

bool CTaskUseVehicleScenario::ShouldUseBonnetPositionForDistanceChecks() const
{
	const CScenarioPoint * pPt = GetScenarioPoint();
	return (pPt && pPt->IsFlagSet(CScenarioPointFlags::UseVehicleFrontForArrival));

// 	const CPed* pPed = GetPed();
// 	CVehicle* pVeh = pPed->GetVehiclePedInside();
// 	Assert(pVeh);
// 
// 	// Bonnet position for helicopters seems to be too far ahead of the root to actually make sense here.
// 	if (pVeh->InheritsFromHeli())
// 	{
// 		return false;
// 	}
// 
// 	return true;
}

bool CTaskUseVehicleScenario::ShouldHoverToPoint(const Vector3& vTarget, float fTolerance) const
{
	const CPed* pPed = GetPed();
	CVehicle* pVeh = pPed->GetVehiclePedInside();

	if (!pVeh || !pVeh->InheritsFromHeli())
	{
		return false;
	}

	Vec3V vCurrent = pVeh->GetTransform().GetPosition();
	Vec3V vecTarget = VECTOR3_TO_VEC3V(vTarget);

	// If the helicopter is not going very far up or down then it should orient to the point.
	if (fabs(vCurrent.GetZf() - vecTarget.GetZf()) < fTolerance)
	{
		return false;
	}	

	vCurrent.SetZ(ScalarV(V_ZERO));
	vecTarget.SetZ(ScalarV(V_ZERO));

	// If the helicopter is traveling a relatively large distance vertically but not very far in the XY plane then return true.
	if (IsLessThanAll(DistSquared(vecTarget, vCurrent), ScalarV(rage::square(fTolerance))))
	{
		return true;
	}

	return false;
}

bool CTaskUseVehicleScenario::CreateDriveSubTask(float desiredDriveSpeed)
{
	const CScenarioVehicleInfo& info = GetScenarioInfo();
	const CScenarioPoint* pt = GetScenarioPoint();
	aiAssert(pt);
	CPed* pPed = GetPed();
	aiAssert(pPed);
	CVehicle* pVeh = pPed->GetVehiclePedInside();
	aiAssert(pVeh);

	if (!pVeh->InheritsFromPlane() && !pVeh->InheritsFromHeli())
	{
		pVeh->m_nVehicleFlags.bShouldFixIfNoCollision = true;
		pVeh->m_nPhysicalFlags.bAllowFreezeIfNoCollision = true;
	}

	if (ShouldVehiclePauseDueToUnloadedNodes())
	{
		//give the vehicle a stop task so it has something to do,
		//and potentially a surface to run dummy physics on
		aiTask* pStopTask = rage_new CTaskVehicleStop(CTaskVehicleStop::SF_DontTerminateWhenStopped | CTaskVehicleStop::SF_EnableTimeslicingWhenStill);
		//aiTask* pSubTask = rage_new CTaskInVehicleBasic(pVeh);
		SetNewTask(rage_new CTaskControlVehicle(pVeh, pStopTask/*, pSubTask*/));

		return false;
	}

	if (info.GetIsClass<CScenarioVehicleParkInfo>())
	{
		m_Parking = true;

		const CScenarioVehicleParkInfo* parkInfo = static_cast<const CScenarioVehicleParkInfo*>(&info);
		sVehicleMissionParams params;
		params.SetTargetPosition(VEC3V_TO_VECTOR3(pt->GetPosition()));
		params.m_fTargetArriveDist = 0.01f;//Want the vehicle to stop moving very close to target pos.

		if(m_Flags.IsFlagSet(VSF_NavUseRoads))
		{
			// Already what the sVehicleMissionParams is set up for.
		}
		else if(m_Flags.IsFlagSet(VSF_NavUseNavMesh))
		{
			params.m_iDrivingFlags.SetFlag(DF_PreferNavmeshRoute);
			params.m_iDrivingFlags.SetFlag(DF_PreventBackgroundPathfinding);
		}
		else
		{
			params.m_iDrivingFlags.SetFlag(DF_ForceStraightLine);
			params.m_iDrivingFlags.SetFlag(DF_PreventBackgroundPathfinding);
		}

		CTaskVehicleParkNew* pParkTask = rage_new CTaskVehicleParkNew(params, VEC3V_TO_VECTOR3(pt->GetDirection()), (CTaskVehicleParkNew::ParkType)parkInfo->GetParkType(), 15.0f * DtoR);
		if(pParkTask)
		{
			// Limit the search distance.
			pParkTask->SetMaxPathSearchDistance(sm_Tunables.m_MaxSearchDistance);

			// Let the parking task do some testing to make sure the parking space isn't blocked.
			// Even if we may have done some tests for that already, this condition can easily change
			// over time.
			pParkTask->SetDoTestsForBlockedSpace(true);
		}
		aiTask* pSubTask = rage_new CTaskInVehicleBasic(pVeh);
		SetNewTask(rage_new CTaskControlVehicle(pVeh, pParkTask, pSubTask));
	}
	else
	{
		// Check if we should set up the subtask to use the slowdown speed.
		if(ShouldSlowDown())
		{
			m_SlowingDown = true;
		}
		else
		{
			m_SlowingDown = false;
		}

		sVehicleMissionParams params;

		if (pt->IsFlagSet(CScenarioPointFlags::AggressiveVehicleDriving))
		{
			params.m_iDrivingFlags = DMode_AvoidCars;
		}
		else
		{
			params.m_iDrivingFlags = DMode_StopForCars;
			if (pVeh->InheritsFromBike())
			{
				params.m_iDrivingFlags.SetFlag(DF_SwerveAroundAllCars);
				params.m_iDrivingFlags.SetFlag(DF_SteerAroundPeds);
			}
		}

		if (pt->IsFlagSet(CScenarioPointFlags::ActivateVehicleSiren))
		{
			pVeh->TurnSirenOn(true);
		}
		else
		{
			pVeh->TurnSirenOn(false); // Make sure it is off just in case
		}

		// Set the speed.
		if(m_SlowingDown)
		{
			desiredDriveSpeed = Min(desiredDriveSpeed, sm_Tunables.m_SlowDownSpeed);
		}
		params.m_fCruiseSpeed = desiredDriveSpeed;

		params.SetTargetPosition(VEC3V_TO_VECTOR3(pt->GetPosition()));

		// The movement doesn't seem to be smooth if we let the subtask finish,
		// so the distance threshold we pass in here is smaller than what we check
		// for ourselves in DrivingToScenario_OnUpdate(), by a fairly arbitrary amount.
		//params.m_fTargetArriveDist = Max(sm_Tunables.m_TargetArriveDist - 1.0f, 0.0f);	// MAGIC!
		params.m_fTargetArriveDist = 0.0f;

		// tweak arrival distance for vehicle type
		if (pVeh->InheritsFromHeli())
		{
			params.m_fTargetArriveDist = sm_Tunables.m_HeliTargetArriveDist;
		}
		else if ( pVeh->InheritsFromBoat() )
		{
			params.m_fTargetArriveDist = sm_Tunables.m_BoatTargetArriveDist; 
			pVeh->GetIntelligence()->GetBoatAvoidanceHelper()->SetMaxAvoidanceOrientationLeft(-sm_Tunables.m_BoatMaxAvoidanceAngle);
			pVeh->GetIntelligence()->GetBoatAvoidanceHelper()->SetMaxAvoidanceOrientationRight(sm_Tunables.m_BoatMaxAvoidanceAngle);


		}
		else if ( pVeh->InheritsFromPlane() )
		{
			params.m_fTargetArriveDist = sm_Tunables.m_PlaneTargetArriveDist;
		}

		if ( (pVeh->InheritsFromBoat() && pVeh->GetIsInWater())
			|| (pVeh->InheritsFromHeli() && (pVeh->IsInAir() || pt->IsFlagSet(CScenarioPointFlags::AerialVehiclePoint)))
			|| (pVeh->InheritsFromPlane() && (pVeh->IsInAir() || pt->IsFlagSet(CScenarioPointFlags::AerialVehiclePoint))) )
		{
			pVeh->SetAllowFreezeWaitingOnCollision(false);
			pVeh->m_nVehicleFlags.bShouldFixIfNoCollision = false;
		}

		float switchToStraightLineDist = sm_Tunables.m_SwitchToStraightLineDist;

		if(m_Flags.IsFlagSet(VSF_NavUseRoads))
		{
			// Already what the sVehicleMissionParams is set up for.
		}
		else if(m_Flags.IsFlagSet(VSF_NavUseNavMesh))
		{
			params.m_iDrivingFlags.SetFlag(DF_PreferNavmeshRoute);
			params.m_iDrivingFlags.SetFlag(DF_PreventBackgroundPathfinding);
		}
		else
		{
			switchToStraightLineDist = 10000.0f;
			params.m_iDrivingFlags.SetFlag(DF_ForceStraightLine);
			params.m_iDrivingFlags.SetFlag(DF_PreventBackgroundPathfinding);
			params.m_iDrivingFlags.ClearFlag(DF_SteerAroundObjects);
		}

		bool bStartedAtSpeed = false;
		bool bDrivingThroughPoint = pt->GetTimeTillPedLeaves() < SMALL_FLOAT && pt->GetTimeTillPedLeaves() > -SMALL_FLOAT
			&& !m_pScenarioPointPrev;
		bool bShouldHoverToPoint = false;

		if (pVeh->InheritsFromHeli())
		{
			bShouldHoverToPoint = ShouldHoverToPoint(params.GetTargetPosition(), params.m_fTargetArriveDist);
		}

		//It is desired that the the start at speed option be automagical in that if you start a vehicle
		// at a point and it has a desired speed then the vehicle should start at that speed with the 
		// limiting factor being that if the scenario has a time till leave greater than 0 or stays forever (-1) 
		// then the vehicle will not start at the desired speed.
		if (desiredDriveSpeed && bDrivingThroughPoint)//drive speed is not 0.0, vehicle is not stopping at point
		{
			//Taken from CommandSetVehicleForwardSpeed in commands_vehicle.cpp
			aiAssertf((rage::Abs(desiredDriveSpeed) < DEFAULT_MAX_SPEED), "Cannot set the forward speed greater than %f m/s", DEFAULT_MAX_SPEED);
			
			// Don't push the heli forwards if it is going to hover to the next point.
			if (!bShouldHoverToPoint)
			{
				pVeh->SwitchEngineOn(true);

				if (!pVeh->IsSuperDummy() || !CVehicleAILodManager::ms_bDisablePhysicsInSuperDummyMode)
				{
					pVeh->SetVelocity(VEC3V_TO_VECTOR3(pVeh->GetTransform().GetB()) * desiredDriveSpeed);
				}
				else
				{
					pVeh->SetSuperDummyVelocity(VEC3V_TO_VECTOR3(pVeh->GetTransform().GetB()) * desiredDriveSpeed);
				}	
			}

			pVeh->SelectAppropriateGearForSpeed();
			bStartedAtSpeed = true;

			if(pVeh->GetIsRotaryAircraft())
			{
				((CRotaryWingAircraft*)pVeh)->SetMainRotorSpeed(MAX_ROT_SPEED_HELI_BLADES);
			}
			else if (pVeh->InheritsFromPlane())
			{
				CPlane* pPlane = static_cast<CPlane*>(pVeh);
				if (pPlane->GetEngineSpeed() <= SMALL_FLOAT)
				{
					pPlane->SetEngineSpeed(CPlane::ms_fMaxEngineSpeed);
				}
			}
		}

		aiTask *pVehicleTask = NULL;
		aiTask* pSubTask = NULL;
		if (pVeh->InheritsFromHeli())
		{
			bool useTimeslicing = true;

			CTaskVehicleGoToHelicopter* pHeliTask = rage_new CTaskVehicleGoToHelicopter( params );
			if ( static_cast<CHeli*>(pVeh)->GetSearchLight() )
			{
				bool bUseSearchLight = pt->IsFlagSet(CScenarioPointFlags::UseSearchlight);
				static_cast<CHeli*>(pVeh)->GetSearchLight()->SetLightOn(bUseSearchLight);
				static_cast<CHeli*>(pVeh)->GetSearchLight()->SetForceLightOn(bUseSearchLight);

				// Skip timeslicing if we want to turn on the search light. Haven't checked, but possibly
				// we could get some timeslicing artifacts on the light direction otherwise, maybe?
				if(bUseSearchLight)
				{
					useTimeslicing = false;
				}
			}


			bDrivingThroughPoint = true;	
			if (pt->IsFlagSet(CScenarioPointFlags::LandVehicleOnArrival))
			{
				pHeliTask->SetHeliFlag(CTaskVehicleGoToHelicopter::HF_LandOnArrival);

				// If we are about to land, probably safest to not try to do any timeslicing
				// for this segment of the path.
				useTimeslicing = false;
			}
			if (bShouldHoverToPoint)
			{
				pHeliTask->SetHeliFlag(CTaskVehicleGoToHelicopter::HF_DontModifyOrientation);

				// If on the ground and the helicopter is hovering up, then do not start at speed and slow to approach like normal.
				Vec3V vPosition = pVeh->GetTransform().GetPosition();
				if (vPosition.GetZf() < CGameWorldHeightMap::GetMaxHeightFromWorldHeightMap(vPosition.GetXf(), vPosition.GetYf()))
				{
					bStartedAtSpeed = false;
					bDrivingThroughPoint = false;
				}
			}

			if ( pVeh->IsInAir())
			{
				((CHeli*) pVeh)->SetMainRotorSpeed(MAX_ROT_SPEED_HELI_BLADES * 1.0f);
				pVeh->m_nVehicleFlags.bDisableHeightMapAvoidance = true;
			}

			if (bStartedAtSpeed)
			{
				pHeliTask->SetHeliFlag(CTaskVehicleGoToHelicopter::HF_StartEngineImmediately);
			}
			if (bDrivingThroughPoint)
			{
				pHeliTask->SetSlowDownDistance(0.0f);
			}

			if(useTimeslicing)
			{
				pHeliTask->SetHeliFlag(CTaskVehicleGoToHelicopter::HF_EnableTimeslicingWhenPossible);
			}

			pVehicleTask = pHeliTask;
		}
		else if (pVeh->InheritsFromPlane() )
		{
			static float s_MinAirSpeed = 20.0f;
			CPlane* pPlane = static_cast<CPlane*>(pVeh);
			// prevent just falling from the sky
			if ( pVeh->IsInAir() )
			{

				// make sure we're atleast going 20
				float speed = pVeh->GetVelocity().Mag();
				if ( speed < s_MinAirSpeed )
				{				
					pPlane->SetForwardSpeed( params.m_fCruiseSpeed );
					pPlane->SetThrottleControl(1.0f);
					pPlane->SetPitchControl(1.0f);
				}
				
				pVeh->SwitchEngineOn(true);
				pVeh->m_nVehicleFlags.bDisableHeightMapAvoidance = true;

			}
			
			if ( pt->IsFlagSet(CScenarioPointFlags::LandVehicleOnArrival) )
			{
				Vec3V vLandingEnd;

				if(m_pScenarioPointNxt)
				{
					vLandingEnd = m_pScenarioPointNxt->GetWorldPosition();
				}
				else
				{
					//B* 2023077; Create fake runway, chain has been disabled, other systems will soon take over and tell plane to fly away
					vLandingEnd = VECTOR3_TO_VEC3V(params.GetTargetPosition()) + (pPlane->GetTransform().GetForward()) * ScalarV(50.0f);
					vLandingEnd.SetZ( params.GetTargetPosition().z);
				}

				pVehicleTask = rage_new CTaskVehicleLandPlane(VECTOR3_TO_VEC3V(params.GetTargetPosition()), vLandingEnd, false);
			}
			else if ( pt->IsFlagSet(CScenarioPointFlags::TaxiPlaneOnGround) )
			{
				params.m_fTargetArriveDist = sm_Tunables.m_PlaneDrivingSubtaskArrivalDist;
				Vector3 vTargetPosition = params.GetTargetPosition();
				pVehicleTask = CVehicleIntelligence::GetGotoTaskForVehicle(pVeh, NULL, &vTargetPosition, params.m_iDrivingFlags | DF_PlaneTaxiMode | DF_PreferNavmeshRoute, params.m_fTargetArriveDist, 0.0f, params.m_fCruiseSpeed);
			}
			else
			{
				CTaskVehicleGoToPlane* pTaskGoToPlane = rage_new CTaskVehicleGoToPlane(params);

				if ( m_pScenarioPointNxt )
				{
					static float s_MinTurnRadiusPadding = 10000.0f;
					Vec3V vDirToCurrentPoint = Normalize(VECTOR3_TO_VEC3V(params.GetTargetPosition()) - pPlane->GetTransform().GetPosition());
					Vec3V vDirToNextPoint = Normalize(m_pScenarioPointNxt->GetWorldPosition() - VECTOR3_TO_VEC3V(params.GetTargetPosition()));
					float fFlightDirection = fwAngle::GetATanOfXY(vDirToNextPoint.GetXf(), vDirToNextPoint.GetYf());
					float fFlightPitch = rage::Atan2f(vDirToNextPoint.GetZf(), MagXY(vDirToNextPoint).Getf());
										
					static float fMaxRollForChain = 45.0f;
					float fMaxRoll = CTaskVehicleGoToPlane::ComputeMaxRollForPitch(Min(fFlightPitch * RtoD, fMaxRollForChain), pTaskGoToPlane->GetMaxRoll());
					float fRadiusToLocalTarget = CTaskVehicleGoToPlane::ComputeTurnRadiusForTarget(pPlane, VEC3V_TO_VECTOR3(m_pScenarioPointNxt->GetWorldPosition()), 0.0f );
					float fMaxSpeed = CTaskVehicleGoToPlane::ComputeSpeedForRadiusAndRoll(*pPlane, fabsf(fRadiusToLocalTarget), fMaxRoll);		

					pTaskGoToPlane->SetOrientationRequested(true);
					pTaskGoToPlane->SetOrientation(fFlightDirection);	
					pTaskGoToPlane->SetCruiseSpeed(Max(Min(fMaxSpeed, params.m_fCruiseSpeed), s_MinAirSpeed));

					ScalarV vTValue = Clamp(Dot( vDirToNextPoint, vDirToCurrentPoint ), ScalarV(0), ScalarV(1.0f));
					ScalarV vMinDist = ScalarV(CTaskVehicleGoToPlane::sm_Tunables.m_minMinDistanceForRollComputation);
					ScalarV vMaxDist = Min(MagXY(m_pScenarioPointNxt->GetWorldPosition() - VECTOR3_TO_VEC3V(params.GetTargetPosition())), ScalarV(CTaskVehicleGoToPlane::sm_Tunables.m_maxMinDistanceForRollComputation));
					vMaxDist = Max(vMaxDist, vMinDist);
					ScalarV vRollMinDist = Lerp(vTValue, vMinDist, vMaxDist);

					// when computing the angle to target if you get really close you could get very large angles
					// this says even if I'm .1m from the target compute the roll as if I was vRollMinDist.  This
					// reduces noise as we approach our target coord.
					pTaskGoToPlane->SetMinDistanceToTargetForRollComputation(vRollMinDist.Getf());

					// just always attempt to turn even if we can't make the turn.
					// we've turned off ground avoidance so looping around is probably going to crash 
					// or look just as stupid
					pTaskGoToPlane->SetMinTurnRadiusPadding(s_MinTurnRadiusPadding); // always attempt to turn

				}

				if ( pt->IsFlagSet(CScenarioPointFlags::AerialVehiclePoint) )
				{
					// landing gear logic for aerial vehicles
					if ( m_pScenarioPointPrev )
					{
						if ( !m_pScenarioPointPrev->IsFlagSet(CScenarioPointFlags::AerialVehiclePoint) )
						{
							static u32 s_RetractLandingGearTimer = 5000;
							pPlane->GetPlaneIntelligence()->SetRetractLandingGearTime(fwTimer::GetTimeInMilliseconds() + s_RetractLandingGearTimer );
						}
					}
					else 
					{
						// landing gear spawns down by default
						// first point and we're in the air
						// next point is not landing
						if ( !m_pScenarioPointNxt || !m_pScenarioPointNxt->IsFlagSet(CScenarioPointFlags::LandVehicleOnArrival) )
						{
							pPlane->GetLandingGear().ControlLandingGear(pPlane, CLandingGear::COMMAND_RETRACT);
						}
					}

					if ( m_pScenarioPointNxt && m_pScenarioPointNxt->IsFlagSet(CScenarioPointFlags::LandVehicleOnArrival) )
					{
						pPlane->GetLandingGear().ControlLandingGear(pPlane, CLandingGear::COMMAND_DEPLOY);
					}
				}

				pVehicleTask = pTaskGoToPlane;
			}

		}
		else
		{
			pVehicleTask = CVehicleIntelligence::CreateAutomobileGotoTask(params, switchToStraightLineDist);

			// Limit the search distance.
			if(pVehicleTask && pVehicleTask->GetTaskType() == CTaskTypes::TASK_VEHICLE_GOTO_AUTOMOBILE_NEW)
			{
				CTaskVehicleGoToAutomobileNew* pGoToTask = static_cast<CTaskVehicleGoToAutomobileNew*>(pVehicleTask);

				// Check if we are driving to an attractor.
				if(info.GetIsFlagSet(CScenarioInfoFlags::WillAttractVehicles))
				{
					// We may want to prefer certain nodes when selecting the target node near the attractor.
					// At this point, we should still have the node list from when the vehicle was cruising around.
					const CVehicleNodeList* pVehNodeList = pVeh->GetIntelligence()->GetNodeList();
					if(pVehNodeList)
					{
						pGoToTask->SetPreferredTargetNodeList(pVehNodeList);
					}
				}

				pGoToTask->SetMaxPathSearchDistance(sm_Tunables.m_MaxSearchDistance);
			}

			pSubTask = rage_new CTaskInVehicleBasic(pVeh);
		}

		SetNewTask(rage_new CTaskControlVehicle(pVeh, pVehicleTask, pSubTask));
	}

	return true;
}


void CTaskUseVehicleScenario::UpdateDriveSubTask(float desiredDriveSpeed)
{
	if(m_Parking)
	{
		return;
	}

	// Check if we should set up the subtask to use the slowdown speed.
	if(ShouldSlowDown())
	{
		m_SlowingDown = true;
		desiredDriveSpeed = Min(desiredDriveSpeed, sm_Tunables.m_SlowDownSpeed);
	}
	else
	{
		m_SlowingDown = false;
	}

	CPed* pPed = GetPed();
	aiAssert(pPed);
	CVehicle* pVeh = pPed->GetVehiclePedInside();
	aiAssert(pVeh);

	if(pVeh->InheritsFromHeli() || pVeh->InheritsFromPlane())
	{
		return;
	}

	// Update the vehicle task copy stored in CTaskControlVehicle.
	CTask* pSubTask = GetSubTask();
	if(pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_CONTROL_VEHICLE)
	{
		CTaskControlVehicle* pCtrlTask = static_cast<CTaskControlVehicle*>(pSubTask);
		CTask* pStoredVehicleTask = static_cast<CTask*>(pCtrlTask->GetVehicleTask());
		if(pStoredVehicleTask && pStoredVehicleTask->IsVehicleMissionTask())
		{
			static_cast<CTaskVehicleMissionBase*>(pStoredVehicleTask)->SetCruiseSpeed(desiredDriveSpeed);
		}
	}

	// Update the running vehicle task.
	CTask* pRunningVehicleTask = static_cast<CTask*>(pVeh->GetIntelligence()->GetTaskManager()->GetActiveTask(VEHICLE_TASK_TREE_PRIMARY));
	if(pRunningVehicleTask && pRunningVehicleTask->IsVehicleMissionTask())
	{
		static_cast<CTaskVehicleMissionBase*>(pRunningVehicleTask)->SetCruiseSpeed(desiredDriveSpeed);
	}
}

//-------------------------------------------------------------------------
// Clone task implementation
//-------------------------------------------------------------------------

CTaskInfo* CTaskUseVehicleScenario::CreateQueriableState() const
{
	CTaskInfo* pNewTaskInfo = NULL;

	u8 flags = static_cast<u8>(m_Flags.GetAllFlags());

	if(m_pScenarioPointPrev==NULL && m_iCachedPrevWorldScenarioPointIndex!=-1)
	{
		m_iCachedPrevWorldScenarioPointIndex=-1;
	}

	if(m_pScenarioPointNxt==NULL && m_iCachedNextWorldScenarioPointIndex!=-1)
	{
		m_iCachedNextWorldScenarioPointIndex=-1;
	}

	const int prevPointIndex = GetWorldScenarioPointIndex(m_pScenarioPointPrev, &m_iCachedPrevWorldScenarioPointIndex);
	const int nextPointIndex = GetWorldScenarioPointIndex(m_pScenarioPointNxt, &m_iCachedNextWorldScenarioPointIndex);
	const int currentPointIndex = GetWorldScenarioPointIndex();

#if __ASSERT
	// This is to try to track down the m_pScenarioPointPrev.Get() != pt assert in DrivingToScenario_OnUpdate():
	const CVehicle* pVeh = GetPed()->GetVehiclePedInside();
	Assertf(prevPointIndex < 0 || prevPointIndex != currentPointIndex,
			"Setting up CClonedTaskUseVehicleScenarioInfo with the previous and current points being the same: "
			"vehicle %s, prev %d (%p), current %d (%p), next %d (%p)",
			pVeh ? pVeh->GetDebugName() : "none",
			prevPointIndex, m_pScenarioPointPrev.Get(),
			currentPointIndex, GetScenarioPoint(),
			nextPointIndex, m_pScenarioPointNxt.Get());
#endif	// __ASSERT

	pNewTaskInfo =  rage_new CClonedTaskUseVehicleScenarioInfo(GetState(),
																static_cast<s16>(GetScenarioType()),
																GetScenarioPoint(),
																prevPointIndex,
																nextPointIndex,
																currentPointIndex,
																flags,
																m_DriveSpeed,
																m_DriveSpeedNxt);

	return pNewTaskInfo;
}

void CTaskUseVehicleScenario::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	Assert( pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_USE_VEHICLE_SCENARIO );

	CClonedTaskUseVehicleScenarioInfo *scenarioInfo = static_cast<CClonedTaskUseVehicleScenarioInfo*>(pTaskInfo);

	m_DriveSpeed = scenarioInfo->GetDriveSpeed();
	m_Flags = scenarioInfo->GetFlags();

	Assertf(!m_pScenarioPointPrev || m_pScenarioPointPrev != GetScenarioPoint(), "Previous and current scenario points are the same, already before ReadQueriableState().");

	m_pScenarioPointPrev = scenarioInfo->GetPrevScenarioPoint();
	m_pScenarioPointNxt = scenarioInfo->GetNextScenarioPoint();
	
	if(scenarioInfo->GetNextScenarioPointIndex()!=-1)
	{
		m_DriveSpeedNxt = scenarioInfo->GetDriveSpeedNxt();
	}

	CTaskScenario::ReadQueriableState(pTaskInfo);

	// Note: this was added to try to track down the m_pScenarioPointPrev.Get() != pt assert
	// in DrivingToScenario_OnUpdate() - if the assert further up didn't fail but this one did,
	// it means that this condition is the result of the information sent over the network, plus
	// possibly the previous state of the scenario.
	Assertf(!m_pScenarioPointPrev || m_pScenarioPointPrev != GetScenarioPoint(),
			"Previous and current scenario points are the same, after ReadQueriableState(): %d/%d/%d (%d/%d?%d)",
			scenarioInfo->GetPrevScenarioPointIndex(), scenarioInfo->GetScenarioId(), scenarioInfo->GetNextScenarioPointIndex(),
			m_iCachedPrevWorldScenarioPointIndex, GetCachedWorldScenarioPointIndex(), m_iCachedNextWorldScenarioPointIndex);
}

CTask::FSM_Return CTaskUseVehicleScenario::UpdateClonedFSM( const s32 /*iState*/, const FSM_Event /*iEvent*/ )
{ 
	if(GetStateFromNetwork() == State_Finish)
	{
		return FSM_Quit;
	}

//	if( (iEvent == OnUpdate) && GetIsWaitingForEntity() )
//	{
//		return FSM_Continue;	
//	}

	// TODO: Should probably add some state updates here, at the very least we probably
	// need to make sure that CTaskInVehicleBasic (or CTaskAmbientClips) is running,
	// so we play the proper idle animations on the clone.

//	return UpdateFSM(iState,  iEvent);

	return FSM_Continue;
}


void CTaskUseVehicleScenario::OnCloneTaskNoLongerRunningOnOwner()
{
	SetStateFromNetwork(State_Finish);
}


CTaskFSMClone *CTaskUseVehicleScenario::CreateTaskForClonePed(CPed *UNUSED_PARAM(pPed))
{
	CTaskUseVehicleScenario* pTask = NULL;
	pTask = rage_new CTaskUseVehicleScenario(*this);

	pTask->m_DriveSpeedNxt = m_DriveSpeedNxt;
	pTask->m_pScenarioPointNxt = m_pScenarioPointNxt;

	pTask->m_iCachedPrevWorldScenarioPointIndex = GetWorldScenarioPointIndex(m_pScenarioPointPrev);
	pTask->m_iCachedNextWorldScenarioPointIndex = GetWorldScenarioPointIndex(m_pScenarioPointNxt);

	return pTask;
}


CTaskFSMClone *CTaskUseVehicleScenario::CreateTaskForLocalPed(CPed *pPed)
{
	// do the same as clone ped
	return CreateTaskForClonePed(pPed);
}

bool CTaskUseVehicleScenario::IsInScope(const CPed* pPed)
{
	bool inScope = CTaskScenario::IsInScope(pPed);

	if(inScope)
	{
		if(GetScenarioPoint() == 0 && !GetIsWaitingForEntity())
		{
			CTaskScenario::CloneFindScenarioPoint();

			if(GetScenarioPoint() == 0)
			{
				return false;
			}
		}
		return AreClonesPrevNextScenariosValid();
	}

	return inScope;
}

bool CTaskUseVehicleScenario::AreClonesPrevNextScenariosValid()
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	if(!pPed)
	{
		Assertf(0, "Null Ped when checking scenario point validity");
		return false;
	}
	if (pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(GetTaskType()))
	{
		CTaskInfo* pTaskInfo = pPed->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(GetTaskType(), PED_TASK_PRIORITY_MAX);

		CClonedTaskUseVehicleScenarioInfo* pScenarioInfo = dynamic_cast<CClonedTaskUseVehicleScenarioInfo*>(pTaskInfo);

		if(taskVerifyf(pScenarioInfo,"Expect ped to have current CClonedTaskUseVehicleScenarioInfo") )
		{
			if(pScenarioInfo->GetPrevScenarioPointIndex()!=-1 && !pScenarioInfo->GetPrevScenarioPoint())
			{
				return false;
			}

			if(pScenarioInfo->GetNextScenarioPointIndex()!=-1 && !pScenarioInfo->GetNextScenarioPoint())
			{
				return false;
			}
		}
	}

	return true;
}

bool CTaskUseVehicleScenario::ControlPassingAllowed(CPed *pPed, const netPlayer& UNUSED_PARAM(player), eMigrationType UNUSED_PARAM(migrationType))
{
	if (pPed->IsNetworkClone() && !GetScenarioPoint())
	{
		CTaskScenario::CloneFindScenarioPoint();
	}

	return GetScenarioPoint() != 0;
}


//-----------------------------------------------------------------------------

CClonedTaskUseVehicleScenarioInfo::CClonedTaskUseVehicleScenarioInfo(s32 state
		, s16 scenarioType
		, CScenarioPoint* pScenarioPoint
		, s32 prevScenarioPointIndex
		, s32 nextScenarioPointIndex
		, int worldScenarioPointIndex
		, u8 flags
		, float driveSpeed
		, float driveSpeedNxt)
	: CClonedScenarioFSMInfo(state, scenarioType, pScenarioPoint, true, worldScenarioPointIndex)
	, m_Flags(flags)
	, m_DriveSpeed(driveSpeed)
	, m_DriveSpeedNxt(driveSpeedNxt)
	, m_PrevScenarioPointIndex(prevScenarioPointIndex)
	, m_NextScenarioPointIndex(nextScenarioPointIndex)
{
	SetStatusFromMainTaskState(state);
}


CClonedTaskUseVehicleScenarioInfo::CClonedTaskUseVehicleScenarioInfo()
: m_Flags(static_cast<u8>(-1)) 
, m_PrevScenarioPointIndex(-1)
, m_NextScenarioPointIndex(-1)
{
}


CTaskFSMClone *CClonedTaskUseVehicleScenarioInfo::CreateCloneFSMTask()
{
	CTaskUseVehicleScenario* pTask = NULL;
	pTask = rage_new CTaskUseVehicleScenario(GetScenarioType(), GetScenarioPoint(), GetFlags(), GetDriveSpeed(), GetScenarioPoint(m_PrevScenarioPointIndex));
	return  pTask;
}

CTask *CClonedTaskUseVehicleScenarioInfo::CreateLocalTask(fwEntity* UNUSED_PARAM(pEntity))
{
	CTaskUseVehicleScenario * pTask = (CTaskUseVehicleScenario*)CreateCloneFSMTask();

	return pTask;
}

CScenarioPoint* CClonedTaskUseVehicleScenarioInfo::GetPrevScenarioPoint() 
{
	return GetScenarioPoint( m_PrevScenarioPointIndex );
}

CScenarioPoint* CClonedTaskUseVehicleScenarioInfo::GetNextScenarioPoint() 
{
	return GetScenarioPoint( m_NextScenarioPointIndex );
}



//-----------------------------------------------------------------------------

// End of file 'task/Scenario/Types/TaskVehicleScenario.cpp'
