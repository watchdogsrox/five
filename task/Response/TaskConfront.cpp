// FILE :    TaskConfront.h
// CREATED : 4/16/2012

// File header
#include "Task/Response/TaskConfront.h"

// Game headers
#include "ai/ambient/ConditionalAnimManager.h"
#include "animation/AnimDefines.h"
#include "event/EventShocking.h"
#include "event/ShockingEvents.h"
#include "modelinfo/ModelSeatInfo.h"
#include "modelinfo/VehicleModelInfo.h"
#include "Peds/ped.h"
#include "Peds/PedIntelligence.h"
#include "task/Default/TaskAmbient.h"
#include "task/General/TaskBasic.h"
#include "task/Movement/TaskGotoPoint.h"
#include "task/Movement/TaskIdle.h"
#include "task/Movement/TaskNavMesh.h"
#include "task/Response/TaskIntimidate.h"
#include "task/Response/TaskShove.h"
#include "Vehicles/vehicle.h"

AI_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////
// CTaskConfront
////////////////////////////////////////////////////////////////////////////////

CTaskConfront::Tunables CTaskConfront::sm_Tunables;
IMPLEMENT_RESPONSE_TASK_TUNABLES(CTaskConfront, 0x3ac11f8f);

CTaskConfront::CTaskConfront(CPed* pTarget)
: m_vCenter(V_ZERO)
, m_pTarget(pTarget)
, m_hAmbientClips()
, m_uTimeTargetWasLastMovingAway(0)
, m_uLastTimeProcessedProbes(0)
, m_uRunningFlags(0)
{
	SetInternalTaskType(CTaskTypes::TASK_CONFRONT);
}

CTaskConfront::~CTaskConfront()
{

}

#if !__FINAL
const char* CTaskConfront::GetStaticStateName(s32 iState)
{
	Assert(iState >= State_Start && iState <= State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_Confront",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif // !__FINAL

aiTask* CTaskConfront::Copy() const
{
	//Create the task.
	CTaskConfront* pTask = rage_new CTaskConfront(m_pTarget);
	
	//Set the ambient clips.
	pTask->SetAmbientClips(m_hAmbientClips);
	
	return pTask;
}

CTask::FSM_Return CTaskConfront::ProcessPreFSM()
{
	//Ensure the target is valid.
	if(!m_pTarget)
	{
		return FSM_Quit;
	}

	//Check if we should process probes.
	if(ShouldProcessProbes())
	{
		ProcessProbes();
	}

	//Process the streaming.
	ProcessStreaming();

	//Check if the target is moving away.
	if(IsTargetMovingAway())
	{
		m_uTimeTargetWasLastMovingAway = fwTimer::GetTimeInMilliseconds();
	}
	
	return FSM_Continue;
}

CTask::FSM_Return CTaskConfront::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	FSM_Begin
	
		FSM_State(State_Start)
			FSM_OnEnter
				Start_OnEnter();
			FSM_OnUpdate
				return Start_OnUpdate();
				
		FSM_State(State_Confront)
			FSM_OnEnter
				Confront_OnEnter();
			FSM_OnUpdate
				return Confront_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

void CTaskConfront::Start_OnEnter()
{
	//Calculate the center.
	m_vCenter = CalculateCenter();
}

CTask::FSM_Return CTaskConfront::Start_OnUpdate()
{
	//Confront the target.
	SetState(State_Confront);
	
	return FSM_Continue;
}

void CTaskConfront::Confront_OnEnter()
{
	//Check if we want to intimidate.
	CTask* pSubTask = NULL;
	if(WantsToIntimidate() && CanIntimidate())
	{
		//Create the sub-task.
		pSubTask = CreateSubTaskForIntimidate();

		//Set the flag.
		m_uRunningFlags.SetFlag(RF_IsIntimidating);

		//Check if we should disable moving.
		if(CTaskIntimidate::ShouldDisableMoving(*GetPed()))
		{
			m_uRunningFlags.SetFlag(RF_DisableMoving);
		}
	}
	else
	{
		//Create the sub-task.
		pSubTask = CreateSubTaskForAmbientClips();
	}

	//Create the move task.
	CTask* pMoveTask = CreateMoveTask();

	//Create the task.
	CTask* pTask = rage_new CTaskComplexControlMovement(pMoveTask, pSubTask, CTaskComplexControlMovement::TerminateOnBoth);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskConfront::Confront_OnUpdate()
{
	//Update the heading.
	UpdateHeading();
	
	//Grab the ped.
	CPed* pPed = GetPed();

	//Grab the positions.
	Vec3V vPedPosition			= pPed->GetTransform().GetPosition();
	Vec3V vTargetPedPosition	= m_pTarget->GetTransform().GetPosition();

	//Calculate the target position.
	Vec3V vTargetPosition = CalculateTargetPosition();

	//Calculate the distances.
	ScalarV scDistPedToTargetPedSq	= DistSquared(vPedPosition, vTargetPedPosition);
	ScalarV scDistPedToCenterSq		= DistSquared(vPedPosition, m_vCenter);
	ScalarV scDistTargetToCenterSq	= DistSquared(vTargetPosition, m_vCenter);

	//Calculate the ideal distance.
	float fIdealDistance = CalculateIdealDistance();

	//Calculate the flags.
	bool bIsInsideIdealDistance		= (IsLessThanAll(scDistPedToTargetPedSq, ScalarVFromF32(square(fIdealDistance))) != 0);
	bool bIsOutsideDistanceToMove	= (IsGreaterThanAll(scDistPedToTargetPedSq, ScalarVFromF32(square(fIdealDistance + sm_Tunables.m_MinDistanceToMove))) != 0);
	bool bIsOutsideMaxRadius		= (IsGreaterThanAll(scDistPedToCenterSq, ScalarVFromF32(square(sm_Tunables.m_MaxRadius))) != 0);
	bool bIsTargetOutsideMaxRadius	= (IsGreaterThanAll(scDistTargetToCenterSq, ScalarVFromF32(square(sm_Tunables.m_MaxRadius))) != 0);

	//Find the complex control movement task.
	CTaskComplexControlMovement* pComplexControlMovementTask = static_cast<CTaskComplexControlMovement *>(
		FindSubTaskOfType(CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT));
	
	//Find the move task.
	CTaskMoveFollowNavMesh* pMoveTask = static_cast<CTaskMoveFollowNavMesh *>(
		pPed->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH));

	//Check if we should stop moving.
	if(m_uRunningFlags.IsFlagSet(RF_DisableMoving) || bIsInsideIdealDistance ||
		(bIsOutsideMaxRadius && bIsTargetOutsideMaxRadius) || ShouldStopMoving())
	{
		//Check if the move task is valid.
		if(pMoveTask)
		{
			//Check if the sub-task is valid.
			if(pComplexControlMovementTask)
			{
				//Face the target.
				CAITarget target(m_pTarget);
				CTaskMoveFaceTarget* pTaskMoveFaceTarget = rage_new CTaskMoveFaceTarget(target);
				pTaskMoveFaceTarget->SetLimitUpdateBasedOnDistance(true);
				pComplexControlMovementTask->SetNewMoveTask(pTaskMoveFaceTarget);
			}
		}
	}
	//Check if we should start moving.
	else if(bIsOutsideDistanceToMove && (!bIsOutsideMaxRadius || !bIsTargetOutsideMaxRadius))
	{
		//Check if the move task is invalid.
		if(!pMoveTask)
		{
			//Check if the sub-task is invalid.
			if(!pComplexControlMovementTask)
			{
				//Create the move task.
				CTask* pNewMoveTask = CreateMoveTask();
				taskAssert(pNewMoveTask->GetTaskType() == CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH);
				pMoveTask = static_cast<CTaskMoveFollowNavMesh *>(pNewMoveTask);

				//Create the task.
				pComplexControlMovementTask = rage_new CTaskComplexControlMovement(pMoveTask, CreateSubTaskForAmbientClips(), CTaskComplexControlMovement::TerminateOnBoth);

				//Start the task.
				SetNewTask(pComplexControlMovementTask);
			}
			//Check if we aren't running the move task.
			else if(!pComplexControlMovementTask->IsMovementTaskRunning() ||
				(pComplexControlMovementTask->GetMoveTaskType() != CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH))
			{
				//Create the move task.
				CTask* pNewMoveTask = CreateMoveTask();
				taskAssert(pNewMoveTask->GetTaskType() == CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH);
				pMoveTask = static_cast<CTaskMoveFollowNavMesh *>(pNewMoveTask);

				//Set the move task.
				pComplexControlMovementTask->SetNewMoveTask(pMoveTask);
			}
		}

		//Check if the move task is valid.
		if(pMoveTask)
		{
			//Set the target.
			pMoveTask->SetTarget(pPed, vTargetPosition);

			//Set the target stop heading.
			float fTargetStopHeading = fwAngle::GetRadianAngleBetweenPoints(vTargetPedPosition.GetXf(), vTargetPedPosition.GetYf(), vPedPosition.GetXf(), vPedPosition.GetYf());
			pMoveTask->SetTargetStopHeading(fTargetStopHeading);

			//Stop exactly.
			pMoveTask->SetStopExactly(true);

			//Don't avoid the target.
			CTaskMoveGoToPoint* pMoveGoToPointTask = static_cast<CTaskMoveGoToPoint *>(pMoveTask->FindSubTaskOfType(CTaskTypes::TASK_MOVE_GO_TO_POINT));
			if(pMoveGoToPointTask)
			{
				pMoveGoToPointTask->SetDontAvoidEntity(m_pTarget);
			}
		}
	}

	//Check if we can start a shove.
	if(CanStartShove())
	{
		//Create the task.
		CTask* pTask = CreateSubTaskForShove();

		//Start the task.
		SetNewTask(pTask);

		//Note that we have shoved.
		m_uRunningFlags.SetFlag(RF_HasShoved);
	}

	return FSM_Continue;
}

Vec3V_Out CTaskConfront::CalculateCenter() const
{
	//Grab the ped.
	const CPed* pPed = GetPed();

	//Grab the ped position.
	Vec3V vPedPosition = pPed->GetTransform().GetPosition();

	return vPedPosition;
}

float CTaskConfront::CalculateIdealDistance() const
{
	//Check if the equipped weapon is valid, and armed.
	const CPedWeaponManager* pWeaponManager = GetPed()->GetWeaponManager();
	if(pWeaponManager && pWeaponManager->GetEquippedWeapon() && pWeaponManager->GetEquippedWeapon()->GetWeaponInfo()->GetIsArmed())
	{
		return sm_Tunables.m_IdealDistanceIfArmed;
	}
	//Check if we are intimidating.
	else if(m_uRunningFlags.IsFlagSet(RF_IsIntimidating))
	{
		return sm_Tunables.m_IdealDistanceIfArmed;
	}
	else
	{
		return sm_Tunables.m_IdealDistanceIfUnarmed;
	}
}

Vec3V_Out CTaskConfront::CalculateTargetPosition() const
{
	//Grab the ped.
	const CPed* pPed = GetPed();

	//Grab the ped position.
	Vec3V vPedPosition = pPed->GetTransform().GetPosition();

	//Check if the vehicle is valid.
	const CVehicle* pVehicle = m_pTarget->GetVehiclePedInside();
	if(pVehicle)
	{
		//Check if the model seat info is valid.
		const CModelSeatInfo* pModelSeatInfo = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo();
		if(taskVerifyf(pModelSeatInfo, "Invalid model seat info."))
		{
			//Grab the vehicle position.
			Vec3V vVehiclePosition = pVehicle->GetTransform().GetPosition();

			//Generate a vector from the vehicle to the ped.
			Vec3V vVehicleToPed = Subtract(vPedPosition, vVehiclePosition);

			//Grab the vehicle right vector.
			Vec3V vVehicleRight = pVehicle->GetTransform().GetRight();

			//Check if the ped is on the right.
			ScalarV scDot = Dot(vVehicleToPed, vVehicleRight);
			bool bIsPedOnRight = (IsGreaterThanAll(scDot, ScalarV(V_ZERO)) != 0);

			//Check if the entry/exit point is valid.
			s32 iSeatRequested = bIsPedOnRight ? Seat_frontPassenger : Seat_driver;
			s32 iEntryExitPoint = CModelSeatInfo::EvaluateSeatAccessFromAllEntryPoints(pPed, pVehicle, SR_Specific, iSeatRequested, false);
			if(iEntryExitPoint >= 0)
			{
				//Check if the entry/exit point is valid.
				const CEntryExitPoint* pEntryExitPoint = pModelSeatInfo->GetEntryExitPoint(iEntryExitPoint);
				if(taskVerifyf(pEntryExitPoint, "Invalid entry/exit point."))
				{
					//Get the target position.
					Vec3V vTargetPosition;
					pEntryExitPoint->GetEntryPointPosition(pVehicle, pPed, RC_VECTOR3(vTargetPosition));

					return vTargetPosition;
				}
			}
		}
	}

	//Grab the target position.
	Vec3V vTargetPosition = m_pTarget->GetTransform().GetPosition();

	//Generate the direction from the target to the ped.
	Vec3V vTargetToPed = Subtract(vPedPosition, vTargetPosition);
	Vec3V vTargetToPedDirection = NormalizeFastSafe(vTargetToPed, Vec3V(V_ZERO));

	//Add the offset.
	float fIdealDistance = CalculateIdealDistance();
	vTargetPosition = AddScaled(vTargetPosition, vTargetToPedDirection, ScalarVFromF32(fIdealDistance));

	return vTargetPosition;
}

bool CTaskConfront::CanIntimidate()
{
	//Check if we can intimidate.
	CPed* pPed = GetPed();
	if(CTaskIntimidate::IsValid(*pPed))
	{
		return true;
	}
	//Check if we can modify the loadout.
	else if(CTaskIntimidate::ModifyLoadout(*pPed))
	{
		//Check if we can intimidate.
		if(CTaskIntimidate::IsValid(*pPed))
		{
			return true;
		}
	}

	return false;
}

bool CTaskConfront::CanShove() const
{
	//Ensure we have not shoved.
	if(m_uRunningFlags.IsFlagSet(RF_HasShoved))
	{
		return false;
	}

	//Ensure we are not intimidating.
	if(m_uRunningFlags.IsFlagSet(RF_IsIntimidating))
	{
		return false;
	}

	//Ensure we don't have a weapon equipped.
	const CPedWeaponManager* pWeaponManager = GetPed()->GetWeaponManager();
	if(pWeaponManager)
	{
		const CWeaponInfo* pWeaponInfo = pWeaponManager->GetEquippedWeaponInfo();
		if(pWeaponInfo && !pWeaponInfo->GetIsUnarmed())
		{
			return false;
		}
	}

	return true;
}

bool CTaskConfront::CanStartShove() const
{
	//Ensure we can shove.
	if(!CanShove())
	{
		return false;
	}

	//Ensure we are not obstructed.
	if(!m_uRunningFlags.IsFlagSet(RF_IsNotObstructed))
	{
		return false;
	}

	//Ensure the shove is valid.
	if(!CTaskShove::IsValid(*GetPed(), *m_pTarget))
	{
		return false;
	}

	return true;
}

CTask* CTaskConfront::CreateMoveTask() const
{
	//Create the params.
	CNavParams params;
	params.m_vTarget = VEC3V_TO_VECTOR3(CalculateTargetPosition());
	params.m_fTargetRadius = 0.0f;
	params.m_fMoveBlendRatio = MOVEBLENDRATIO_WALK;
	params.m_fCompletionRadius = 0.0f;

	//Create the task.
	CTaskMoveFollowNavMesh* pTask = rage_new CTaskMoveFollowNavMesh(params);
	pTask->SetNeverEnterWater(true);
	pTask->SetDontUseLadders(true);

	return pTask;
}

CTask* CTaskConfront::CreateSubTaskForAmbientClips() const
{
	//Ensure the ambient clips are valid.
	if(m_hAmbientClips.IsNull())
	{
		return NULL;
	}
	
	//Ensure the conditional anims group is valid.
	const CConditionalAnimsGroup* pConditionalAnimsGroup = CONDITIONALANIMSMGR.GetConditionalAnimsGroup(m_hAmbientClips);
	if(!taskVerifyf(pConditionalAnimsGroup, "Conditional anims group is invalid for hash: %s.", m_hAmbientClips.GetCStr()))
	{
		return NULL;
	}
	
	//Create the task.
	CTask* pTask = rage_new CTaskAmbientClips(CTaskAmbientClips::Flag_PlayBaseClip|CTaskAmbientClips::Flag_PlayIdleClips, pConditionalAnimsGroup);

	return pTask;
}

CTask* CTaskConfront::CreateSubTaskForIntimidate() const
{
	//Create the task.
	CTaskIntimidate* pTask = rage_new CTaskIntimidate(m_pTarget);

	return pTask;
}

CTask* CTaskConfront::CreateSubTaskForShove() const
{
	//Create the task.
	CTaskShove* pTask = rage_new CTaskShove(m_pTarget, CTaskShove::CF_Interrupt);

	return pTask;
}

bool CTaskConfront::HasWeaponInHand(const CPed& rPed) const
{
	//Ensure the weapon manager is valid.
	const CPedWeaponManager* pWeaponManager = rPed.GetWeaponManager();
	if(!pWeaponManager)
	{
		return false;
	}

	//Ensure the equipped weapon is valid.
	const CWeapon* pWeapon = pWeaponManager->GetEquippedWeapon();
	if(!pWeapon)
	{
		return false;
	}

	//Ensure the weapon is armed.
	if(!pWeapon->GetWeaponInfo()->GetIsArmed())
	{
		return false;
	}

	return true;
}

bool CTaskConfront::IsShoving() const
{
	return (FindSubTaskOfType(CTaskTypes::TASK_SHOVE) != NULL);
}

bool CTaskConfront::IsTargetInOurTerritory() const
{
	//Ensure our last used scenario point is valid.
	const CScenarioPoint* pPoint = GetPed()->GetPedIntelligence()->GetLastUsedScenarioPoint();
	if(!pPoint)
	{
		return false;
	}

	//Ensure the point is marked.
	if(!pPoint->IsFlagSet(CScenarioPointFlags::TerritorialScenario))
	{
		return false;
	}

	//Ensure the target is within the radius.
	ScalarV scDistSq = DistSquared(pPoint->GetWorldPosition(), m_pTarget->GetTransform().GetPosition());
	ScalarV scMaxDistSq = ScalarVFromF32(square(pPoint->GetRadius()));
	if(IsGreaterThanAll(scDistSq, scMaxDistSq))
	{
		return false;
	}

	return true;
}

bool CTaskConfront::IsTargetMovingAway() const
{
	//Ensure the velocity is valid.
	Vec3V vTargetVelocity = VECTOR3_TO_VEC3V(m_pTarget->GetVelocity());
	ScalarV scSpeedSq = MagSquared(vTargetVelocity);
	static dev_float s_fMinSpeed = 0.5f;
	ScalarV scMinSpeedSq = ScalarVFromF32(square(s_fMinSpeed));
	if(IsLessThanAll(scSpeedSq, scMinSpeedSq))
	{
		return false;
	}

	//Ensure the target is moving away.
	Vec3V vTargetToPed = Subtract(GetPed()->GetTransform().GetPosition(), m_pTarget->GetTransform().GetPosition());
	ScalarV scDot = Dot(vTargetVelocity, vTargetToPed);
	if(IsGreaterThanAll(scDot, ScalarV(V_ZERO)))
	{
		return false;
	}

	return true;
}

void CTaskConfront::ProcessProbes()
{
	//Check if the probe is not waiting or ready.
	if(!m_ProbeResults.GetResultsWaitingOrReady())
	{
		//Check if the time has expired.
		static dev_u32 s_uMinTime = 250;
		if(CTimeHelpers::GetTimeSince(m_uLastTimeProcessedProbes) > s_uMinTime)
		{
			//Create the probe.
			WorldProbe::CShapeTestProbeDesc probeDesc;
			probeDesc.SetContext(WorldProbe::EMelee);
			probeDesc.SetResultsStructure(&m_ProbeResults);
			probeDesc.SetStartAndEnd(VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition()), VEC3V_TO_VECTOR3(m_pTarget->GetTransform().GetPosition()));
			probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_MELEE_VISIBILTY_TYPES);

			//Start the test.
			WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_ASYNCHRONOUS_TEST);
		}
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
			m_uRunningFlags.ChangeFlag(RF_IsNotObstructed, (m_ProbeResults.GetNumHits() == 0));

			//Reset the results.
			m_ProbeResults.Reset();

			//Set the timer.
			m_uLastTimeProcessedProbes = fwTimer::GetTimeInMilliseconds();
		}
	}
}

void CTaskConfront::ProcessStreaming()
{
	//Check if we can shove.
	if(CanShove() || IsShoving())
	{
		//Request the clip set.
		m_ClipSetRequestHelper.RequestClipSet(CLIP_SET_REACTION_SHOVE);
	}
	else
	{
		//Release the clip set.
		m_ClipSetRequestHelper.ReleaseClipSet();
	}
}

bool CTaskConfront::ShouldProcessProbes() const
{
	//Check if we can shove.
	if(CanShove())
	{
		return true;
	}

	return false;
}

bool CTaskConfront::ShouldStopMoving() const
{
	//Ensure the target is not in our territory.
	if(IsTargetInOurTerritory())
	{
		return false;
	}

	//Check if the target has been moving away recently.
	static dev_u32 s_uMaxTimeSinceTargetWasLastMovingAway = 500;
	if((m_uTimeTargetWasLastMovingAway > 0) &&
		((m_uTimeTargetWasLastMovingAway + s_uMaxTimeSinceTargetWasLastMovingAway) > fwTimer::GetTimeInMilliseconds()))
	{
		return true;
	}

	return false;
}

void CTaskConfront::UpdateHeading()
{
	//Grab the ped.
	CPed* pPed = GetPed();
	
	//Ensure the ped is not in motion.
	if(pPed->IsInMotion())
	{
		return;
	}
	
	//Set the desired heading.
	pPed->SetDesiredHeading(VEC3V_TO_VECTOR3(m_pTarget->GetTransform().GetPosition()));
}

bool CTaskConfront::WantsToIntimidate() const
{
	//Calculate the chances.
	float fChances = 0.0f;
	if(m_pTarget->GetIsInVehicle())
	{
		fChances = 1.0;
	}
	else if(GetPed()->IsRegularCop())
	{
		fChances = 1.0f;
	}
	else if(HasWeaponInHand(*m_pTarget))
	{
		fChances = sm_Tunables.m_ChancesToIntimidateArmedTarget;
	}
	else
	{
		fChances = sm_Tunables.m_ChancesToIntimidateUnarmedTarget;
	}

	//Ensure the chances are valid.
	if(fChances <= 0.0f)
	{
		return false;
	}

	float fRandom = fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
	return (fRandom <= fChances);
}
