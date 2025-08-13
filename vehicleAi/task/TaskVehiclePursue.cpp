// File header
#include "TaskVehiclePursue.h"

// Game headers
#include "Peds/ped.h"
#include "vehicleAi/VehicleIntelligence.h"
#include "vehicleAi/task/TaskVehicleCruise.h"
#include "vehicleAi/task/TaskVehicleEscort.h"
#include "vehicles/vehicle.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

//=========================================================================
// CTaskVehiclePursue
//=========================================================================

CTaskVehiclePursue::Tunables CTaskVehiclePursue::sm_Tunables;
IMPLEMENT_VEHICLE_AI_TASK_TUNABLES(CTaskVehiclePursue, 0x86a5918e);

CTaskVehiclePursue::CTaskVehiclePursue(const sVehicleMissionParams& rParams, float fIdealDistance, float fDesiredOffset)
: CTaskVehicleMissionBase(rParams)
, m_NavMeshLineOfSightHelper()
, m_DriftHelperX()
, m_DriftHelperY()
, m_fIdealDistance(fIdealDistance)
, m_fDesiredOffset(fDesiredOffset)
, m_fTimeSinceLastLineOfSightCheck(0.0f)
, m_uRunningFlags(0)
, m_nNextTimeAllowedToMove(0)
{
	SetInternalTaskType(CTaskTypes::TASK_VEHICLE_PURSUE);
}

CTaskVehiclePursue::~CTaskVehiclePursue()
{

}

Vec3V_Out CTaskVehiclePursue::GetDrift() const
{
	return Vec3V(m_DriftHelperX.GetValue(), m_DriftHelperY.GetValue(), 0.0f);
}

void CTaskVehiclePursue::SetDesiredOffset(float fDesiredOffset)
{
	//Set the desired offset.
	m_fDesiredOffset = fDesiredOffset;
	
	//Update the avoidance cache.
	UpdateAvoidanceCache();
}

void CTaskVehiclePursue::SetIdealDistance(float fIdealDistance)
{
	//Set the ideal distance.
	m_fIdealDistance = fIdealDistance;
	
	//Update the avoidance cache.
	UpdateAvoidanceCache();
}

Mat34V_Out CTaskVehiclePursue::CalculateTargetMatrix(const CPhysical& rTarget)
{
	//Grab the target position.
	Vec3V vTargetPosition = rTarget.GetTransform().GetPosition();

	//Grab the target's velocity.
	Vec3V vTargetVelocity = VECTOR3_TO_VEC3V(rTarget.GetVelocity());
	
	//Grab the target's forward vector.
	Vec3V vTargetForward = rTarget.GetTransform().GetForward();
	
	//Calculate the forward vector.
	Vec3V vForward = NormalizeFastSafe(vTargetVelocity, vTargetForward);

	//Adjust the target position.
	ScalarV scDot = Dot(vTargetVelocity, vTargetForward);
	vTargetPosition = AddScaled(vTargetPosition, vTargetForward, (IsGreaterThanOrEqualAll(scDot, ScalarV(V_ZERO))) ?
		ScalarVFromF32(rTarget.GetBaseModelInfo()->GetBoundingBoxMin().y) : ScalarVFromF32(rTarget.GetBaseModelInfo()->GetBoundingBoxMax().y));
	
	//Grab the up vector.
	Vec3V vUp = Vec3V(V_Z_AXIS_WZERO);
	
	//Calculate the side vector.
	Vec3V vSide = Cross(vForward, vUp);
	vSide = NormalizeFastSafe(vSide, Vec3V(V_X_AXIS_WZERO));
	
	//Calculate the up vector.
	vUp = Cross(vSide, vForward);
	vUp = NormalizeFastSafe(vUp, Vec3V(V_Z_AXIS_WZERO));

	//Create the matrix.
	Mat34V mTransform;
	mTransform.Seta(vSide);
	mTransform.Setb(vForward);
	mTransform.Setc(vUp);
	mTransform.Setd(vTargetPosition);

	return mTransform;
}

Vec3V_Out CTaskVehiclePursue::CalculateTargetPosition(const CPhysical& rTarget)
{
	//Calculate the target matrix.
	Mat34V mTarget = CalculateTargetMatrix(rTarget);

	return CalculateTargetPosition(mTarget);
}

Vec3V_Out CTaskVehiclePursue::CalculateTargetPosition(Mat34V_In mTarget)
{
	return mTarget.GetCol3();
}

#if !__FINAL
void CTaskVehiclePursue::Debug() const
{
	//Call the base class version.
	CTask::Debug();
}

const char* CTaskVehiclePursue::GetStaticStateName(s32 iState)
{
	Assert(iState >= State_Start && iState <= State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_GetBehind",
		"State_Pursue",
		"State_Seek",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif // !__FINAL

void CTaskVehiclePursue::CleanUp()
{
	//Clear the avoidance cache.
	if (GetVehicle())
	{
		GetVehicle()->GetIntelligence()->ClearAvoidanceCache();
	}
}

CTask::FSM_Return CTaskVehiclePursue::ProcessPreFSM()
{
	//Ensure the target is valid.
	if(!GetDominantTarget())
	{
		return FSM_Quit;
	}
	
	//Process the drift.
	ProcessDrift();

	//Process the probes.
	ProcessProbes();
	
	return FSM_Continue;
}

CTask::FSM_Return CTaskVehiclePursue::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();
				
		FSM_State(State_GetBehind)
			FSM_OnEnter
				GetBehind_OnEnter();
			FSM_OnUpdate
				return GetBehind_OnUpdate();
				
		FSM_State(State_Pursue)
			FSM_OnEnter
				Pursue_OnEnter();
			FSM_OnUpdate
				return Pursue_OnUpdate();
			FSM_OnExit
				Pursue_OnExit();

		FSM_State(State_Seek)
			FSM_OnEnter
				Seek_OnEnter();
			FSM_OnUpdate
				return Seek_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

void CTaskVehiclePursue::CloneUpdate(CVehicle* pVehicle)
{
	//this is set from the ped side locally
	pVehicle->m_nVehicleFlags.bAvoidanceDirtyFlag = true;
}

CTask::FSM_Return CTaskVehiclePursue::Start_OnUpdate()
{
	//Update the avoidance cache.
	UpdateAvoidanceCache();
	
	//Get behind the target.
	SetState(State_GetBehind);
	
	return FSM_Continue;
}

void CTaskVehiclePursue::GetBehind_OnEnter()
{
	//Create the vehicle mission parameters.
	sVehicleMissionParams params = m_Params;

	//Use the target position.
	params.m_iDrivingFlags.SetFlag(DF_TargetPositionOverridesEntity);

	//Create the task.
	CTask* pTask = CVehicleIntelligence::CreateAutomobileGotoTask(params, sVehicleMissionParams::DEFAULT_SWITCH_TO_STRAIGHT_LINE_DIST);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskVehiclePursue::GetBehind_OnUpdate()
{
	//Update the vehicle mission.
	UpdateVehicleMissionForGetBehind();
	
	//Check if pursue is appropriate.
	if(IsPursueAppropriate())
	{
		//Pursue the target.
		SetState(State_Pursue);
	}
	//Check if seek is appropriate.
	else if(IsSeekAppropriate())
	{
		//Seek the target.
		SetState(State_Seek);
	}
	//Check if the task has finished.
	else if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Finish the task.
		SetState(State_Finish);
	}

	return FSM_Continue;
}

void CTaskVehiclePursue::Pursue_OnEnter()
{
	//Create the vehicle mission parameters.
	sVehicleMissionParams params = m_Params;
	
	//Use the target position.
	params.m_iDrivingFlags.SetFlag(DF_TargetPositionOverridesEntity);
	
	//Do not avoid the target.
	params.m_iDrivingFlags.SetFlag(DF_DontAvoidTarget);
	
	//Create the task.
	CTask* pTask = CVehicleIntelligence::CreateAutomobileGotoTask(params, sVehicleMissionParams::DEFAULT_SWITCH_TO_STRAIGHT_LINE_DIST);
	
	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskVehiclePursue::Pursue_OnUpdate()
{
	//Update the vehicle mission.
	UpdateVehicleMissionForPursue();
	
	//Check if pursue is no longer appropriate.
	if(IsPursueNoLongerAppropriate())
	{
		//Get behind the target.
		SetState(State_GetBehind);
	}
	//Check if the task has finished.
	else if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Finish the task.
		SetState(State_Finish);
	}
	
	return FSM_Continue;
}

void CTaskVehiclePursue::Pursue_OnExit()
{
	CVehicle* pVehicle = GetVehicle();
	if(pVehicle && pVehicle->PopTypeIsMission())
	{
		pVehicle->m_nVehicleFlags.bDisableRoadExtentsForAvoidance = false;
	}
}

void CTaskVehiclePursue::Seek_OnEnter()
{
	//Create the task.
	static dev_float s_fStraightLineDistance = -1.0f;
	CTask* pTask = CVehicleIntelligence::CreateAutomobileGotoTask(m_Params, s_fStraightLineDistance);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskVehiclePursue::Seek_OnUpdate()
{
	//Check if seek is no longer appropriate.
	if(IsSeekNoLongerAppropriate())
	{
		//Get behind the target.
		SetState(State_GetBehind);
	}
	//Check if the task has finished.
	else if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Finish the task.
		SetState(State_Finish);
	}

	return FSM_Continue;
}

float CTaskVehiclePursue::CalculateCruiseSpeedForPursue() const
{
	//Grab the vehicle.
	const CVehicle* pVehicle = GetVehicle();
	
	//Calculate the vehicle speed.
	float fVehicleSpeed = pVehicle->GetAiXYSpeed();
	
	//Grab the target.
	const CPhysical* pTarget = GetDominantTarget();

	//Calculate the target speed.
	float fTargetSpeed = pTarget->GetVelocity().XYMag();
	
	//Calculate the target position.
	Vec3V vTargetPosition = CalculateTargetPosition();

	//Transform the target position into local space.
	Vec3V vTargetPositionLocal = pVehicle->GetTransform().UnTransform(vTargetPosition);

	//Get the bumper position.
	bool bDriveInReverse = m_Params.m_iDrivingFlags.IsFlagSet(DF_DriveInReverse);
	Vec3V vVehicleBumperPosition = CVehicleFollowRouteHelper::GetVehicleBonnetPosition(pVehicle, bDriveInReverse);

	//Transform the bumper position into local space.
	Vec3V vVehicleBumperPositionLocal = pVehicle->GetTransform().UnTransform(vVehicleBumperPosition);

	//Get the bumper to target.
	Vec3V vBumperToTarget = Subtract(vTargetPositionLocal, vVehicleBumperPositionLocal);

	//Get the manhattan distance.
	float fManhattanDistance = Abs(vBumperToTarget.GetXf()) + Abs(vBumperToTarget.GetYf()) + Abs(vBumperToTarget.GetZf());

	//Get the ideal distance.
	float fIdealDistance = m_fIdealDistance + m_fDesiredOffset;

	//Calculate the cruise speed.
	float fCruiseSpeed;
	if(fManhattanDistance >= fIdealDistance)
	{
		//We are outside of our ideal distance.
		//Speed up quickly.
		
		//Calculate the speed difference.
		float fSpeedDifferenceForMinDistanceToStartMatchingSpeed = sm_Tunables.m_SpeedDifferenceForMinDistanceToStartMatchingSpeed;
		float fSpeedDifferenceForMaxDistanceToStartMatchingSpeed = sm_Tunables.m_SpeedDifferenceForMaxDistanceToStartMatchingSpeed;
		float fSpeedDifference = Clamp(fVehicleSpeed - fTargetSpeed, fSpeedDifferenceForMinDistanceToStartMatchingSpeed, fSpeedDifferenceForMaxDistanceToStartMatchingSpeed);
		float fValue = (fSpeedDifference - fSpeedDifferenceForMinDistanceToStartMatchingSpeed) / (fSpeedDifferenceForMaxDistanceToStartMatchingSpeed - fSpeedDifferenceForMinDistanceToStartMatchingSpeed);
		
		//Calculate the distance to start matching speed.
		float fMinDistanceToStartMatchingSpeed = sm_Tunables.m_MinDistanceToStartMatchingSpeed;
		float fMaxDistanceToStartMatchingSpeed = sm_Tunables.m_MaxDistanceToStartMatchingSpeed;
		float fDistanceToStartMatchingSpeed = Lerp(fValue, fMinDistanceToStartMatchingSpeed, fMaxDistanceToStartMatchingSpeed);

		//Clamp the distance to values that we care about.
		float fMaxDistance = fIdealDistance + fDistanceToStartMatchingSpeed;
		fManhattanDistance = Clamp(fManhattanDistance, fIdealDistance, fMaxDistance);

		//Normalize the value.
		float fLerp = fManhattanDistance - fIdealDistance;
		fLerp /= (fMaxDistance - fIdealDistance);

		//Calculate the maximum speed.
		float fMaximumSpeed = m_Params.m_fCruiseSpeed;

		//Assign the cruise speed.
		fCruiseSpeed = Lerp(fLerp, fTargetSpeed, fMaximumSpeed);
	}
	else
	{
		//We are inside of our ideal distance.
		//Back off slowly.

		//Clamp the distance to values that we care about.
		fManhattanDistance = Clamp(fManhattanDistance, 0.0f, fIdealDistance);

		//Normalize the value.
		float fLerp = fManhattanDistance / fIdealDistance;

		//Calculate the cruise speed multiplier.
		float fCruiseSpeedMultiplier = Lerp(fLerp, sm_Tunables.m_CruiseSpeedMultiplierForBackOff, 1.0f);

		//Assign the cruise speed.
		fCruiseSpeed = fTargetSpeed * fCruiseSpeedMultiplier;
	}
	
	return fCruiseSpeed;
}

Vec3V_Out CTaskVehiclePursue::CalculateTargetPosition() const
{
	return CalculateTargetPosition(*GetDominantTarget());
}

float CTaskVehiclePursue::ClampCruiseSpeedForPursue(float fCruiseSpeed) const
{
	//Grab the vehicle.
	const CVehicle* pVehicle = GetVehicle();

	//Grab the target.
	const CPhysical* pTarget = GetDominantTarget();

	//Grab the vehicle position.
	Vec3V vVehiclePosition = pVehicle->GetTransform().GetPosition();

	//Grab the vehicle velocity.
	Vec3V vVehicleVelocity = VECTOR3_TO_VEC3V(pVehicle->GetVelocity());

	//Grab the vehicle forward.
	Vec3V vVehicleForward = pVehicle->GetTransform().GetForward();

	//Calculate the vehicle direction.
	Vec3V vVehicleDirection = NormalizeFastSafe(vVehicleVelocity, vVehicleForward);

	//Grab the target position.
	Vec3V vTargetPosition = pTarget->GetTransform().GetPosition();

	//Calculate the direction from the vehicle to the target.
	Vec3V vVehicleToTarget = Subtract(vTargetPosition, vVehiclePosition);
	Vec3V vVehicleToTargetDirection = NormalizeFastSafe(vVehicleToTarget, Vec3V(V_ZERO));

	//Check the general direction of the vehicle towards the target.
	float fDot = Dot(vVehicleDirection, vVehicleToTargetDirection).Getf();

	//Use the absolute value of the direction.
	//Moving towards/away from the target should make no difference.
	fDot = Abs(fDot);

	//Clamp the dot to values we care about.
	fDot = Clamp(fDot, sm_Tunables.m_DotToClampSpeedToMinimum, sm_Tunables.m_DotToClampSpeedToMaximum);

	//Normalize the value.
	float fValue = (fDot - sm_Tunables.m_DotToClampSpeedToMinimum) / (sm_Tunables.m_DotToClampSpeedToMaximum - sm_Tunables.m_DotToClampSpeedToMinimum);

	//Grab the minimum speed.
	float fMinSpeed = sm_Tunables.m_SpeedForMinimumDot;

	//Grab the maximum speed.
	float fMaxSpeed = pVehicle->m_Transmission.GetDriveMaxVelocity();

	//Calculate the speed.
	float fSpeed = Lerp(fValue, fMinSpeed, fMaxSpeed);

	//Clamp the speed.
	fCruiseSpeed = Min(fSpeed, fCruiseSpeed);

	return fCruiseSpeed;
}

const CPhysical* CTaskVehiclePursue::GetDominantTarget() const
{
	//Ensure the target entity is valid.
	const CEntity* pEntity = m_Params.GetTargetEntity().GetEntity();
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

		//Check if the ped is on a mount.
		const CPed* pMount = pPed->GetMountPedOn();
		if(pMount)
		{
			return pMount;
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

const CPed* CTaskVehiclePursue::GetTargetPed() const
{
	//Grab the dominant target.
	const CPhysical* pTarget = GetDominantTarget();

	//Check if the target is a ped.
	if(pTarget->GetIsTypePed())
	{
		return static_cast<const CPed*>(pTarget);
	}
	//Check if the target is a vehicle.
	else if(pTarget->GetIsTypeVehicle())
	{
		return static_cast<const CVehicle *>(pTarget)->GetDriver();
	}
	else
	{
		return NULL;
	}
}

bool CTaskVehiclePursue::IsPursueAppropriate() const
{
	//Ensure we aren't stuck.
	if(IsStuck())
	{
		return false;
	}

	//Check if we are obstructed by map geometry.
	if(m_uRunningFlags.IsFlagSet(RF_IsObstructedByMapGeometry))
	{
		return false;
	}

	//Grab the vehicle.
	const CVehicle* pVehicle = GetVehicle();
	
	//Grab the target.
	const CPhysical* pTarget = GetDominantTarget();
	
	//Grab the vehicle position.
	Vec3V vVehiclePosition = pVehicle->GetTransform().GetPosition();
	
	//Grab the target position.
	Vec3V vTargetPosition = pTarget->GetTransform().GetPosition();
	
	//Grab the target velocity.
	Vec3V vTargetVelocity = VECTOR3_TO_VEC3V(pTarget->GetVelocity());
	
	//Grab the target forward.
	Vec3V vTargetForward = pTarget->GetTransform().GetForward();
	
	//Calculate the target direction.
	static dev_float s_fMinTargetVelocityToUseIt = 1.0f;
	Vec3V vMinTargetVelocityToUseItSq = Vec3VFromF32(square(s_fMinTargetVelocityToUseIt));
	Vec3V vTargetDirection = NormalizeFastSafe(vTargetVelocity, vTargetForward, vMinTargetVelocityToUseItSq);
	
	//Calculate a vector from the target to the vehicle.
	Vec3V vTargetToVehicle = Subtract(vVehiclePosition, vTargetPosition);
	
	//Ensure the vehicle is behind the target.
	ScalarV scDot = Dot(vTargetDirection, vTargetToVehicle);
	if(IsLessThanAll(scDot, ScalarV(V_ZERO)))
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool CTaskVehiclePursue::IsPursueNoLongerAppropriate() const
{
	//Ensure we aren't stuck.
	if(IsStuck())
	{
		return false;
	}

	return !IsPursueAppropriate();
}

bool CTaskVehiclePursue::IsSeekAppropriate() const
{
	//Ensure we aren't stuck.
	if(IsStuck())
	{
		return false;
	}

	//Check if we are obstructed by map geometry.
	if(m_uRunningFlags.IsFlagSet(RF_IsObstructedByMapGeometry))
	{
		return true;
	}

	//Check if the target is inside an enclosed search region.
	const CPed* pTargetPed = GetTargetPed();
	if(pTargetPed && pTargetPed->GetPedResetFlag(CPED_RESET_FLAG_InsideEnclosedSearchRegion))
	{
		return true;
	}

	return false;
}

bool CTaskVehiclePursue::IsSeekNoLongerAppropriate() const
{
	//Ensure we aren't stuck.
	if(IsStuck())
	{
		return false;
	}

	return !IsSeekAppropriate();
}

bool CTaskVehiclePursue::IsStuck() const
{
	return (CVehicleStuckDuringCombatHelper::IsStuck(*GetVehicle()));
}

void CTaskVehiclePursue::ProcessDrift()
{
	//Process the X drift.
	m_DriftHelperX.Update(sm_Tunables.m_DriftX.m_MinValueForCorrection, sm_Tunables.m_DriftX.m_MaxValueForCorrection,
		sm_Tunables.m_DriftX.m_MinRate, sm_Tunables.m_DriftX.m_MaxRate);
	
	//Process the Y drift.
	m_DriftHelperY.Update(sm_Tunables.m_DriftY.m_MinValueForCorrection, sm_Tunables.m_DriftY.m_MaxValueForCorrection,
		sm_Tunables.m_DriftY.m_MinRate, sm_Tunables.m_DriftY.m_MaxRate);
}

void CTaskVehiclePursue::ProcessProbes()
{
	//Check if the probe is not waiting or ready.
	if(!m_ProbeResults.GetResultsWaitingOrReady())
	{
		//Create the probe.
		WorldProbe::CShapeTestProbeDesc probeDesc;
		probeDesc.SetResultsStructure(&m_ProbeResults);
		probeDesc.SetStartAndEnd(VEC3V_TO_VECTOR3(GetVehicle()->GetTransform().GetPosition()), VEC3V_TO_VECTOR3(GetDominantTarget()->GetTransform().GetPosition()));
		probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);

		//Start the test.
		WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_ASYNCHRONOUS_TEST);
	}
	else
	{
		//Check if the probe is waiting.
		if(m_ProbeResults.GetWaitingOnResults())
		{
			//Nothing to do, we are waiting on the results.
		}
		else
		{
			//Update the flag.
			m_uRunningFlags.ChangeFlag(RF_IsObstructedByMapGeometry, (m_ProbeResults.GetNumHits() != 0));

			//Reset the results.
			m_ProbeResults.Reset();
		}
	}
}

void CTaskVehiclePursue::UpdateAvoidanceCache()
{
	//Ensure the vehicle is valid.
	CVehicle* pVehicle = GetVehicle();
	if(!pVehicle)
	{
		return;
	}
	
	//Set the avoidance cache.
	CVehicleIntelligence::AvoidanceCache& rCache = pVehicle->GetIntelligence()->GetAvoidanceCache();
	rCache.m_pTarget = GetDominantTarget();
	rCache.m_fDesiredOffset = m_fIdealDistance + m_fDesiredOffset;
}

void CTaskVehiclePursue::UpdateCruiseSpeedForPursue(CTaskVehicleGoToAutomobileNew& rTask)
{
	//Calculate the cruise speed.
	float fCruiseSpeed = CalculateCruiseSpeedForPursue();
	
	//Check if we are in straight-line mode.
	if(rTask.IsDoingStraightLineNavigation())
	{
		//Clamp the cruise speed based.
		fCruiseSpeed = ClampCruiseSpeedForPursue(fCruiseSpeed);
	}

	Vector3 vTargetPos;
	rTask.FindTargetCoors(GetVehicle(), vTargetPos);
	CTaskVehicleEscort::HandleBrakingIfAheadOfTarget(GetVehicle(), GetTargetEntity(), vTargetPos, IsDrivingFlagSet(DF_DriveInReverse), false, m_nNextTimeAllowedToMove, fCruiseSpeed);
	
	//Set the cruise speed.
	rTask.SetCruiseSpeed(fCruiseSpeed);
}

void CTaskVehiclePursue::UpdateNavMeshLineOfSight()
{
	//Check if the test is active.
	if(m_NavMeshLineOfSightHelper.IsTestActive())
	{
		//Check the results of the test.
		SearchStatus searchResult;
		bool bLosIsClear, bNoSurfaceAtCoords;
		if(m_NavMeshLineOfSightHelper.GetTestResults(searchResult, bLosIsClear, bNoSurfaceAtCoords, 1, NULL) && (searchResult != SS_StillSearching))
		{
			//Update the flag.
			m_uRunningFlags.ChangeFlag(RF_HasNavMeshLineOfSight, bLosIsClear);
		}
	}
	else
	{
		//Increase the timer.
		m_fTimeSinceLastLineOfSightCheck += fwTimer::GetTimeStep();

		//Check if the timer has expired.
		if(m_fTimeSinceLastLineOfSightCheck >= sm_Tunables.m_TimeBetweenLineOfSightChecks)
		{
			//Clear the timer.
			m_fTimeSinceLastLineOfSightCheck = 0.0f;
			
			//Grab the vehicle.
			const CVehicle* pVehicle = GetVehicle();

			//Grab the target.
			const CPhysical* pTarget = GetDominantTarget();
			
			//Grab the vehicle position.
			Vec3V vVehiclePosition = pVehicle->GetTransform().GetPosition();

			//Grab the target position.
			Vec3V vTargetPosition = pTarget->GetTransform().GetPosition();
			
			//Start the line of sight test.
			m_NavMeshLineOfSightHelper.StartLosTest(VEC3V_TO_VECTOR3(vVehiclePosition), VEC3V_TO_VECTOR3(vTargetPosition), pVehicle->GetBoundRadius());
		}
	}
}

void CTaskVehiclePursue::UpdateStraightLineDistanceForPursue(CTaskVehicleGoToAutomobileNew& rTask)
{
	//Grab the vehicle position.
	Vec3V vVehiclePosition = GetVehicle()->GetTransform().GetPosition();

	//Grab the target position.
	Vec3V vTargetPosition = GetDominantTarget()->GetTransform().GetPosition();
	
	//Calculate the distance.
	ScalarV scDistSq = DistSquared(vVehiclePosition, vTargetPosition);
	
	//Keep track of the straight-line distance.
	float fStraightLineDistance = 0.0f;
	
	//Determine the straight-line distance.
	//There are three zones:
	//	1) Closest -- always in straight-line mode
	//	2) Middle -- maybe in straight-line mode, depending on nav-mesh los
	//	3) Farthest -- never in straight-line mode
	
	//Check if we are in the closest zone.
	ScalarV scDistanceForStraightLineModeAlwaysSq = ScalarVFromF32(square(sm_Tunables.m_DistanceForStraightLineModeAlways));
	if(IsLessThanAll(scDistSq, scDistanceForStraightLineModeAlwaysSq))
	{
		//Assign the straight-line distance.
		fStraightLineDistance = sm_Tunables.m_DistanceForStraightLineModeAlways;
	}
	else
	{
		//Check if we are in the middle zone.
		ScalarV scDistanceForStraightLineModeIfLosSq = ScalarVFromF32(square(sm_Tunables.m_DistanceForStraightLineModeIfLos));
		if(IsLessThanAll(scDistSq, scDistanceForStraightLineModeIfLosSq))
		{
			//Update the nav-mesh line of sight.
			UpdateNavMeshLineOfSight();

			//Check if we have a nav-mesh line of sight.
			if(m_uRunningFlags.IsFlagSet(RF_HasNavMeshLineOfSight))
			{
				//Assign the straight-line distance.
				fStraightLineDistance = sm_Tunables.m_DistanceForStraightLineModeIfLos;
			}
		}
	}

	//Set the straight-line distance.
	rTask.SetStraightLineDist(fStraightLineDistance);
}

void CTaskVehiclePursue::UpdateTargetPositionForGetBehind(CTaskVehicleGoToAutomobileNew& rTask)
{
	//Grab the target.
	const CPhysical* pTarget = GetDominantTarget();
	
	//Grab the target position.
	Vec3V vTargetPosition = pTarget->GetTransform().GetPosition();
	
	//Grab the target velocity.
	Vec3V vTargetVelocity = VECTOR3_TO_VEC3V(pTarget->GetVelocity());
	
	//Calculate the target offset.
	Vec3V vTargetOffset = Negate(Scale(vTargetVelocity, ScalarVFromF32(sm_Tunables.m_TimeToLookBehind)));
	
	//Ensure the distance exceeds the threshold.
	ScalarV scDistSq = MagSquared(vTargetOffset);
	ScalarV scMinDist = ScalarVFromF32(sm_Tunables.m_MinDistanceToLookBehind);
	ScalarV scMinDistSq = Scale(scMinDist, scMinDist);
	if(IsLessThanAll(scDistSq, scMinDistSq))
	{
		//Grab the target forward.
		Vec3V vTargetForward = pTarget->GetTransform().GetForward();
		
		//Calculate the target direction.
		Vec3V vTargetDirection = NormalizeFastSafe(vTargetVelocity, vTargetForward);
		
		//Re-calculate the target offset using the min distance.
		vTargetOffset = Negate(Scale(vTargetDirection, scMinDist));
	}
	
	//Calculate the target position.
	Vec3V vTaskTargetPosition = Add(vTargetPosition, vTargetOffset);
	
	//Set the target position.
	rTask.SetTargetPosition(&RCC_VECTOR3(vTaskTargetPosition));
}

void CTaskVehiclePursue::UpdateTargetPositionForPursue(CTaskVehicleGoToAutomobileNew& rTask)
{
	//Calculate the target position.
	Vec3V vTargetPosition = CalculateTargetPosition();
	
	//Set the target position.
	rTask.SetTargetPosition(&RCC_VECTOR3(vTargetPosition));
}

void CTaskVehiclePursue::UpdateVehicleMissionForGetBehind()
{
	//Ensure the sub-task is valid.
	CTask* pSubTask = GetSubTask();
	if(!pSubTask)
	{
		return;
	}
	
	//Ensure the sub-task is the correct type.
	if(!taskVerifyf(pSubTask->GetTaskType() == CTaskTypes::TASK_VEHICLE_GOTO_AUTOMOBILE_NEW, "The task type is incorrect: %d.", pSubTask->GetTaskType()))
	{
		return;
	}
	
	//Grab the task.
	CTaskVehicleGoToAutomobileNew* pTask = static_cast<CTaskVehicleGoToAutomobileNew *>(pSubTask);
	
	//Update the target position.
	UpdateTargetPositionForGetBehind(*pTask);
}

void CTaskVehiclePursue::UpdateVehicleMissionForPursue()
{
	//Ensure the sub-task is valid.
	CTask* pSubTask = GetSubTask();
	if(!pSubTask)
	{
		return;
	}

	//Ensure the sub-task is the correct type.
	if(!taskVerifyf(pSubTask->GetTaskType() == CTaskTypes::TASK_VEHICLE_GOTO_AUTOMOBILE_NEW, "The task type is incorrect: %d.", pSubTask->GetTaskType()))
	{
		return;
	}
	
	//Grab the task.
	CTaskVehicleGoToAutomobileNew* pTask = static_cast<CTaskVehicleGoToAutomobileNew *>(pSubTask);
	
	//Update the straight-line distance.
	UpdateStraightLineDistanceForPursue(*pTask);
	
	//Update the target position.
	UpdateTargetPositionForPursue(*pTask);
	
	//Update the cruise speed.
	UpdateCruiseSpeedForPursue(*pTask);

	//ensure we're not limited by road extents when close to target and traveling fast
	CVehicle* pVehicle = GetVehicle();
	if(pVehicle && pVehicle->PopTypeIsMission())
	{
		bool bDisabledRoadExtents = false;
		if (pVehicle->GetAiXYSpeed() > 10.0f)
		{
			Vec3V vTargetPosition = CalculateTargetPosition();
			const Vec3V pVehiclePosition = GetVehicle()->GetTransform().GetPosition();
			bDisabledRoadExtents = IsLessThanAll(DistSquared(pVehiclePosition,vTargetPosition), ScalarV(50.0f*50.0f)) != 0;
		}

		//disable road extents when chasing and moving fairly fast
		pVehicle->m_nVehicleFlags.bDisableRoadExtentsForAvoidance = bDisabledRoadExtents;
	}
}
