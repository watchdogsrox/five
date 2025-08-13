#include "TaskGotoVehicleDoor.h"

#include "Control/Route.h"
#include "Debug/DebugScene.h"
#include "Game/ModelIndices.h"
#include "Peds/Ped.h"
#include "Peds/PedCapsule.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PedDebugVisualiser.h"
#include "Peds/PedGeometryAnalyser.h"
#include "Peds/Ped.h"
#include "Task/Combat/TaskCombat.h"
#include "Task/General/TaskBasic.h"
#include "Task/Motion/TaskMotionBase.h"
#include "Task/Movement/TaskCollisionResponse.h"
#include "Task/Movement/TaskGoto.h"
#include "Task/Movement/TaskNavMesh.h"
#include "Task/Service/Police/TaskPolicePatrol.h"
#include "Task/Vehicle/TaskCarUtils.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Task/Vehicle/VehicleHullAI.h"
#include "Vehicles/Vehicle.h"
#include "Vehicles/Metadata/VehicleEntryPointInfo.h"
#include "Vehicles/Metadata/VehicleLayoutInfo.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/Movement/Climbing/TaskVault.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()
//*****************************************************************
//	CTaskGoToCarDoorAndStandStill
//

const float CTaskGoToCarDoorAndStandStill::ms_fCombatAlignRadius				= 0.8f;
const float CTaskGoToCarDoorAndStandStill::ms_fBehindTargetRadius				= 0.65f;
const float CTaskGoToCarDoorAndStandStill::ms_fTargetRadius						= 0.25f;
const float CTaskGoToCarDoorAndStandStill::ms_fLargerTargetRadius				= 1.0f;
const float CTaskGoToCarDoorAndStandStill::ms_fSlowDownDistance					= 4.0f;
const float CTaskGoToCarDoorAndStandStill::ms_fMaxSeekDistance					= 200.0f;
const int CTaskGoToCarDoorAndStandStill::ms_iMaxSeekTime						= 30000;


CTaskGoToCarDoorAndStandStill::CTaskGoToCarDoorAndStandStill(
	CPhysical * pTargetVehicle,
	const float fMoveBlendRatio,
	const bool bIsDriver, 
	const s32 iEntryPoint,
	const float fTargetRadius,
	const float fSlowDownDistance, 
	const float fMaxSeekDistance, 
	const int iMaxSeekTime,
	const bool bGetAsCloseAsPossibleIfRouteNotFound,
	const CPed * pTargetPed)
: m_pTargetVehicle(pTargetVehicle),
m_fMoveBlendRatio(fMoveBlendRatio),
m_bIsDriver(bIsDriver),
m_fTargetRadius(fTargetRadius),
m_fSlowDownDistance(fSlowDownDistance),
m_fMaxSeekDistance(fMaxSeekDistance),
m_iMaxSeekTime(iMaxSeekTime),
m_iEntryPoint(iEntryPoint),
m_bGetAsCloseAsPossibleIfRouteNotFound(bGetAsCloseAsPossibleIfRouteNotFound),
m_bHasAchievedDoor(false),
m_bForceSucceed(false),
m_bForceFailed(false),
m_pTargetPed(pTargetPed)
{
	SetInternalTaskType(CTaskTypes::TASK_GO_TO_CAR_DOOR_AND_STAND_STILL);
}

CTaskGoToCarDoorAndStandStill::~CTaskGoToCarDoorAndStandStill()
{

}

bool
CTaskGoToCarDoorAndStandStill::ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
	return CTask::ShouldAbort(iPriority, pEvent);
}

CTask::FSM_Return
CTaskGoToCarDoorAndStandStill::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	if(!m_pTargetVehicle)
		return FSM_Quit;

	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		FSM_State(State_Initial)
			FSM_OnEnter
				return StateInitial_OnEnter(pPed);
			FSM_OnUpdate
				return StateInitial_OnUpdate(pPed);
		FSM_State(State_Active)
			FSM_OnEnter
				return StateActive_OnEnter(pPed);
			FSM_OnUpdate
				return StateActive_OnUpdate(pPed);
		FSM_State(State_AutoVault)
			FSM_OnEnter
				return StateAutoVault_OnEnter(pPed);
			FSM_OnUpdate
				return StateAutoVault_OnUpdate(pPed);
	FSM_End
}

CTask::FSM_Return CTaskGoToCarDoorAndStandStill::StateInitial_OnEnter(CPed * UNUSED_PARAM(pPed))
{
	m_vLocalSpaceEntryModifier = CTaskEnterVehicle::CalculateLocalSpaceEntryModifier(m_pTargetVehicle, m_iEntryPoint);

	return FSM_Continue;
}
CTask::FSM_Return CTaskGoToCarDoorAndStandStill::StateInitial_OnUpdate(CPed * UNUSED_PARAM(pPed))
{
	SetState(State_Active);
	return FSM_Continue;
}
CTask::FSM_Return CTaskGoToCarDoorAndStandStill::StateActive_OnEnter(CPed * pPed)
{
	CTaskMoveGoToVehicleDoor * pMoveTask = rage_new CTaskMoveGoToVehicleDoor(
		m_pTargetVehicle,
		m_fMoveBlendRatio,
		m_bIsDriver,
		m_iEntryPoint,
		m_fTargetRadius,
		m_fSlowDownDistance,
		m_fMaxSeekDistance,
		m_iMaxSeekTime,
		m_bGetAsCloseAsPossibleIfRouteNotFound
		);

	CTask* pCombatTask = NULL;
	if(m_pTargetPed && pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetIsArmed())
	{
		pCombatTask = rage_new CTaskCombatAdditionalTask(CTaskCombatAdditionalTask::RT_DefaultNoFiring, m_pTargetPed, VEC3V_TO_VECTOR3(m_pTargetPed->GetTransform().GetPosition()));
	}
	//@@: location CTASKGOTOCARDOORANDSTANDSTILL_STATEACTIVE_ONENTER_ISSUE_CONTROL_MOVEMENT
	CTaskComplexControlMovement * pCtrlMove = rage_new CTaskComplexControlMovement(pMoveTask, pCombatTask);
	SetNewTask(pCtrlMove);

	return FSM_Continue;
}
CTask::FSM_Return CTaskGoToCarDoorAndStandStill::StateActive_OnUpdate(CPed * pPed)
{
	if(m_bForceSucceed)
	{
		m_bHasAchievedDoor = true;
		return FSM_Quit;
	}
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		Assert(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT);
		CTaskComplexControlMovement * pCtrlMove = (CTaskComplexControlMovement*)GetSubTask();
		CTask * pMoveTask = pCtrlMove->GetRunningMovementTask(pPed);
		if(pMoveTask)
		{
			Assert(pMoveTask->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_VEHICLE_DOOR);
			m_bHasAchievedDoor = ((CTaskMoveGoToVehicleDoor*)pMoveTask)->HasAchievedDoor();
			m_bForceFailed = ((CTaskMoveGoToVehicleDoor*)pMoveTask)->HasFailedToNavigateToVehicle();
		}

		AI_LOG_WITH_ARGS("[VehicleEntryExit] %s GoToDoor task is quitting: m_bHasAchievedDoor - %s, m_bForceFailed - %s\n",AILogging::GetDynamicEntityNameSafe(pPed), AILogging::GetBooleanAsString(m_bHasAchievedDoor), AILogging::GetBooleanAsString(m_bForceFailed));
		return FSM_Quit;
	}

	//! If we are trying to move to a boat, and we are stuck, attempt to vault.
	if(pPed->IsLocalPlayer() && 
		m_pTargetVehicle && 
		m_pTargetVehicle->GetIsTypeVehicle() && 
		((CVehicle*)m_pTargetVehicle.Get())->InheritsFromBoat() && 
		(pPed->GetPedIntelligence()->GetEventScanner()->GetStaticMovementScanner().GetStaticCounter() > 5))
	{
		Vector3 vDoorPos;
		if(GetTargetDoorPos(pPed,vDoorPos))
		{
			Vector3 vToTarget = vDoorPos - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			vToTarget.z = 0.f;
			vToTarget.Normalize();

			if(Dot(VEC3V_TO_VECTOR3(pPed->GetTransform().GetForward()), vToTarget) > 0.9f)
			{
				//! Get climb params.
				sPedTestParams pedTestParams;
				CTaskPlayerOnFoot::GetClimbDetectorParam(	CTaskPlayerOnFoot::STATE_INVALID,
					pPed, 
					pedTestParams, 
					true, 
					false, 
					false, 
					false);

				sPedTestResults pedTestResults;

				//! Ask the climb detector if it can find a handhold.
				CClimbHandHoldDetected handHoldDetected;
				pPed->GetPedIntelligence()->GetClimbDetector().ResetFlags();
				pPed->SetPedResetFlag(CPED_RESET_FLAG_SearchingForClimb, true);
				pPed->GetPedIntelligence()->GetClimbDetector().Process(NULL, &pedTestParams, &pedTestResults);
				if(pPed->GetPedIntelligence()->GetClimbDetector().GetDetectedHandHold(handHoldDetected))
				{
					if(handHoldDetected.GetPhysical() != m_pTargetVehicle)
					{
						//! Found one, vault up.
						SetState(State_AutoVault);
					}
				}
			}
		}
	}

	return FSM_Continue;
}


CTask::FSM_Return CTaskGoToCarDoorAndStandStill::StateAutoVault_OnEnter(CPed * pPed)
{
	CClimbHandHoldDetected handHoldDetected;
	if(taskVerifyf(pPed->GetPedIntelligence()->GetClimbDetector().GetDetectedHandHold(handHoldDetected), "No hand hold for vault task"))
	{
		fwFlags8 iVaultFlags;
		if(!pPed->GetPedIntelligence()->GetClimbDetector().CanOrientToDetectedHandhold())
		{
			iVaultFlags.SetFlag(VF_DontOrientToHandhold);
		}

		CTaskVault* pTaskVault = rage_new CTaskVault(handHoldDetected, iVaultFlags);
		SetNewTask(pTaskVault);
		return FSM_Continue;
	}

	return FSM_Quit;
}

CTask::FSM_Return CTaskGoToCarDoorAndStandStill::StateAutoVault_OnUpdate(CPed * UNUSED_PARAM(pPed))
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		// Go back to initial state & re-pick new navigation mode.
		SetState(State_Initial);
	}

	return FSM_Continue;
}

bool CTaskGoToCarDoorAndStandStill::GetTargetDoorPos(CPed * pPed, Vector3 & vOutPosDoor) const
{
	if(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
	{
		CTaskComplexControlMovement * pCtrlMove = (CTaskComplexControlMovement*)GetSubTask();
		CTask * pMoveTask = pCtrlMove->GetRunningMovementTask(pPed);
		if(pMoveTask)
		{
			Assert(pMoveTask->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_VEHICLE_DOOR);
			vOutPosDoor =((CTaskMoveGoToVehicleDoor*)pMoveTask)->GetTargetDoorPos();
			return true;
		}
	}
	return false;
}

void CTaskGoToCarDoorAndStandStill::SetMoveBlendRatio( float fMoveBlendRatio )
{
	m_fMoveBlendRatio = fMoveBlendRatio;
	if( GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT )
	{
		CTaskComplexControlMovement* pMoveSubtask = static_cast<CTaskComplexControlMovement*>(GetSubTask());
		pMoveSubtask->SetMoveBlendRatio(GetPed(), m_fMoveBlendRatio);
	}
}

//*****************************************************************
//	CTaskMoveGoToVehicleDoor
//

bank_float CTaskMoveGoToVehicleDoor::ms_fMaxRollInv = 1.0f / 0.785f;
bank_float CTaskMoveGoToVehicleDoor::ms_fCarMovedEpsSqr = 0.5f * 0.5f;
bank_float CTaskMoveGoToVehicleDoor::ms_fCarReorientatedDotEps = Cosf(( DtoR * 5.0f));
bank_float CTaskMoveGoToVehicleDoor::ms_fCheckGoDirectlyToDoorFrequency = 0.25f;
bank_float CTaskMoveGoToVehicleDoor::ms_fCarChangedSpeedDelta = 0.1f;
bank_bool CTaskMoveGoToVehicleDoor::m_bUpdateGotoTasksEvenIfCarHasNotMoved = true;

bank_float CTaskMoveGoToVehicleDoor::ms_fMinUpdateFreq = 2.0f;
bank_float CTaskMoveGoToVehicleDoor::ms_fMaxUpdateFreq = 0.125f;
bank_float CTaskMoveGoToVehicleDoor::ms_fMinUpdateDist = 30.0f;
bank_float CTaskMoveGoToVehicleDoor::ms_fMaxUpdateDist = 5.0f;

bank_float CTaskMoveGoToVehicleDoor::ms_fTighterDistance = 5.0f;
bank_bool CTaskMoveGoToVehicleDoor::ms_bUseTighterTurnSettings = false;
bank_bool CTaskMoveGoToVehicleDoor::ms_bUseTighterTurnSettingsInWater = true;

float CTaskMoveGoToVehicleDoor::ComputeTargetRadius(const CPed& ped, const CEntity& ent, const Vector3& vEntryPos, bool bAdjustForMoveSpeed, bool bCombatEntry)
{
	if (bCombatEntry)
	{
		return CTaskGoToCarDoorAndStandStill::ms_fCombatAlignRadius;
	}

	Vector3 vPedPos = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition());
	Vector3 vPedToEntry = vEntryPos - vPedPos;
	vPedToEntry.z = 0.0f;
	const float fDistToEntrySqd = vPedToEntry.Mag2();
	vPedToEntry.NormalizeSafe();
	Vector3 vToEnt = VEC3V_TO_VECTOR3(ent.GetTransform().GetPosition()) - vPedPos;
	vToEnt.z = 0.0f;
	vToEnt.NormalizeSafe();

#if __BANK
	TUNE_GROUP_BOOL(ENTER_VEHICLE_TUNE, bRenderComputeTargetRadiusDirections, false);
	if (bRenderComputeTargetRadiusDirections)
	{
		static float fDebugLineLength = 10.f;
		static int fComputeTargetRadiusDebugTTL = 50;
		grcDebugDraw::Line(vPedPos, vPedPos + vPedToEntry * fDebugLineLength, Color_orange, fComputeTargetRadiusDebugTTL);
		grcDebugDraw::Line(vPedPos, vPedPos + vToEnt * fDebugLineLength, Color_cyan, fComputeTargetRadiusDebugTTL);
	}
#endif

	float fRadiusMultiplier = 1.0f;
	if (bAdjustForMoveSpeed)
	{
		const float fCurrentMbrY = ped.GetMotionData()->GetCurrentMbrY();
		if (fCurrentMbrY <= MOVEBLENDRATIO_WALK)
		{
			TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, RADIUS_MULTIPLIER, 1.0f, 0.0f, 1.0f, 0.01f);
			fRadiusMultiplier = RADIUS_MULTIPLIER;
		}
	}

	const bool bIsBike = ent.GetIsTypeVehicle() && static_cast<const CVehicle&>(ent).InheritsFromBike();
	const float fPedToEntryDot = vPedToEntry.Dot(vToEnt);
	if (fPedToEntryDot > 0.0f)
	{
		if (!bIsBike)
		{
			float fRoll = Abs(ent.GetTransform().GetRoll());
			fRoll *= ms_fMaxRollInv;
			float fReturn = fRoll * CTaskGoToCarDoorAndStandStill::ms_fLargerTargetRadius + (1.0f - fRoll) * CTaskGoToCarDoorAndStandStill::ms_fTargetRadius;
			return fRadiusMultiplier * fReturn;
		}
		else
		{
			return fRadiusMultiplier * CTaskGoToCarDoorAndStandStill::ms_fTargetRadius;
		}
	}
	else
	{
		TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, MIN_BEHIND_RADIUS_ALIGN_TRIGGER, 0.5f, 0.0f, 2.0f, 0.01f);
		TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, MAX_BEHIND_RADIUS_ALIGN_TRIGGER, 1.0f, 0.0f, 2.0f, 0.01f);
		TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, MAX_ALIGN_DIST, 1.0f, 0.0f, 2.0f, 0.01f);
		const float fBehindRadiusScale = 1.0f - Clamp(fDistToEntrySqd / square(MAX_ALIGN_DIST), 0.0f, 1.0f);
		const float fAlignRadiusTrigger = (1.0f - fBehindRadiusScale) * MIN_BEHIND_RADIUS_ALIGN_TRIGGER + fBehindRadiusScale * MAX_BEHIND_RADIUS_ALIGN_TRIGGER;
		return fRadiusMultiplier * fAlignRadiusTrigger;
	}
}

CTaskMoveGoToVehicleDoor::CTaskMoveGoToVehicleDoor(
	CPhysical * pTargetVehicle,
	const float fMoveBlendRatio,
	const bool bIsDriver, 
	const s32 iEntryPoint,
	const float fTargetRadius,
	const float fSlowDownDistance, 
	const float fMaxSeekDistance, 
	const int iMaxSeekTime,
	const bool bGetAsCloseAsPossibleIfRouteNotFound)
: CTaskMove(fMoveBlendRatio),
m_pTargetVehicle(pTargetVehicle),
m_bIsDriver(bIsDriver),
m_fTargetRadius(fTargetRadius),
m_fSlowDownDistance(fSlowDownDistance),
m_fMaxSeekDistance(fMaxSeekDistance),
m_iMaxSeekTime(iMaxSeekTime),
m_iEntryPoint(iEntryPoint),
m_bGetAsCloseAsPossibleIfRouteNotFound(bGetAsCloseAsPossibleIfRouteNotFound),
m_pPointRouteRouteToDoor(NULL),
m_bCanUpdateTaskThisFrame(true),
m_iLastTaskUpdateTimeMs(0),
m_fCheckGoDirectlyToDoorTimer(ms_fCheckGoDirectlyToDoorFrequency),
m_fInitialSpeed(0.0f),
m_vTargetDoorPos(VEC3_ZERO),
m_vTargetBoardingPoint(VEC3_ZERO),
m_vOffsetModifier(VEC3_ZERO),
m_bCarHasChangedSpeed(false),
m_bFailedToNavigateToVehicle(false),
m_iCurrentHullDirection(0),
m_fLeadersLastMBR(MOVEBLENDRATIO_WALK),
m_iChooseStateCounter(0)
{
	SetInternalTaskType(CTaskTypes::TASK_MOVE_GO_TO_VEHICLE_DOOR);
}

CTaskMoveGoToVehicleDoor::~CTaskMoveGoToVehicleDoor()
{
	if(m_pPointRouteRouteToDoor)
		delete m_pPointRouteRouteToDoor;
}

#if !__FINAL
void CTaskMoveGoToVehicleDoor::Debug() const
{
#if DEBUG_DRAW
	static float fRot = 0.0f;
	Matrix34 mRot; mRot.MakeRotateZ(fRot);
	Vector3 vOffsetX(0.5f, 0.0, 0.0f), vOffsetY(0.0f, 0.5f, 0.0);
	mRot.Transform(vOffsetX); mRot.Transform(vOffsetY);
	fRot += fwTimer::GetTimeStep()*1.0f; while(fRot > TWO_PI) fRot -= TWO_PI;
	grcDebugDraw::Line(m_vTargetBoardingPoint - vOffsetX, m_vTargetBoardingPoint + vOffsetX, Color32(0x80, 0xff, 0x00, 0xff));
	grcDebugDraw::Line(m_vTargetBoardingPoint - vOffsetY, m_vTargetBoardingPoint + vOffsetY, Color32(0x80, 0xff, 0x00, 0xff));
	grcDebugDraw::Line(m_vTargetBoardingPoint, VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition()), Color32(0xff, 0xff, 0xff, 0xff));

	grcDebugDraw::Line(m_vTargetBoardingPoint, m_vTargetDoorPos, Color_white, Color_white);

	grcDebugDraw::Sphere(m_vInitialCarPos, 0.05f, Color_green);
	grcDebugDraw::Sphere(m_vTargetBoardingPoint, 0.07f, Color_red);
	grcDebugDraw::Sphere(m_vTargetDoorPos, 0.05f, Color_white);
#endif

	if(GetSubTask())
	{
		GetSubTask()->Debug();
	}
}
#endif

bool CTaskMoveGoToVehicleDoor::HasAchievedDoor()
{
	Assert(GetPed());
	const CPed& ped = *GetPed();

	if (m_pTargetVehicle && m_pTargetVehicle->GetIsTypeVehicle())
	{
		if (!CTaskEnterVehicle::PassesPlayerDriverAlignCondition(ped, *static_cast<const CVehicle*>(m_pTargetVehicle.Get())))
			return false;

		if (!CTaskEnterVehicleAlign::PassesGroundConditionForAlign(ped, *static_cast<const CVehicle*>(m_pTargetVehicle.Get())))
			return false;
	}

	Vector3 vPedPos = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition());
	Vector3 vDoorPos, vBoardingPos;
	CalcTargetPositions(&ped, vDoorPos, vBoardingPos);

	if(ped.GetPedConfigFlag(CPED_CONFIG_FLAG_WillArrestRatherThanJack))
	{
		// Check if the ped is considered close enough to have achieved the door target
		return (vBoardingPos - vPedPos).Mag2() < rage::square(.5f);
	}
	else
	{
		static dev_float MIN_ACHIEVE_DOOR_XY_DIST = 2.0f;
		static dev_float MIN_ACHIEVE_DOOR_Z_DIST = 2.0f;

		float fTargetDist = MIN_ACHIEVE_DOOR_XY_DIST;

		if (CTaskEnterVehicleAlign::ms_Tunables.m_ForceStandEnterOnly && m_pTargetVehicle)
		{
			fTargetDist = CTaskMoveGoToVehicleDoor::ComputeTargetRadius(ped, *m_pTargetVehicle, vDoorPos);
		}
		// Check if the ped is considered close enough to have achieved the door target
		return ((vDoorPos - vPedPos).XYMag2() < rage::square(fTargetDist)) && (ABS(vDoorPos.z - vPedPos.z) < MIN_ACHIEVE_DOOR_Z_DIST);
	}
}

bool CTaskMoveGoToVehicleDoor::ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
	return CTask::ShouldAbort(iPriority, pEvent);
}

CTask::FSM_Return CTaskMoveGoToVehicleDoor::ProcessPreFSM()
{
	if(!m_pTargetVehicle)
		return FSM_Quit;

	if(GetPed())
		GetPed()->SetPedResetFlag( CPED_RESET_FLAG_DisableMotionBaseVelocityOverride, true);

	return FSM_Continue;
}

CTask::FSM_Return CTaskMoveGoToVehicleDoor::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	CalcTaskUpdateFrequency(pPed);

	FSM_Begin
		FSM_State(State_Initial)
			FSM_OnEnter
				return StateInitial_OnEnter(pPed);
			FSM_OnUpdate
				return StateInitial_OnUpdate(pPed);
		FSM_State(State_GoDirectlyToBoardingPoint)
			FSM_OnEnter
				return StateGoDirectlyToBoardingPoint_OnEnter(pPed);
			FSM_OnUpdate
				return StateGoDirectlyToBoardingPoint_OnUpdate(pPed);
		FSM_State(State_FollowPointRouteToBoardingPoint)
			FSM_OnEnter
				return StateFollowPointRouteToBoardingPoint_OnEnter(pPed);
			FSM_OnUpdate
				return StateFollowPointRouteToBoardingPoint_OnUpdate(pPed);
		FSM_State(State_FollowNavMeshToBoardingPoint)
			FSM_OnEnter
				return StateFollowNavMeshToBoardingPoint_OnEnter(pPed);
			FSM_OnUpdate
				return StateFollowNavMeshToBoardingPoint_OnUpdate(pPed);
		FSM_State(State_GoToDoor)
			FSM_OnEnter
				return StateGoToDoor_OnEnter(pPed);
			FSM_OnUpdate
				return StateGoToDoor_OnUpdate(pPed);
	FSM_End
}

CTask::FSM_Return CTaskMoveGoToVehicleDoor::StateInitial_OnEnter(CPed * pPed)
{
	m_fCheckGoDirectlyToDoorTimer = ms_fCheckGoDirectlyToDoorFrequency;
	m_bCarHasChangedSpeed = false;

	if(ShouldQuit(pPed))
		return FSM_Quit;

	Assert(m_pTargetVehicle);
	m_vInitialCarPos = VEC3V_TO_VECTOR3(m_pTargetVehicle->GetTransform().GetPosition());
	m_vInitialCarXAxis = VEC3V_TO_VECTOR3(m_pTargetVehicle->GetTransform().GetA());
	m_vInitialCarYAxis = VEC3V_TO_VECTOR3(m_pTargetVehicle->GetTransform().GetB());
	m_fInitialSpeed = m_pTargetVehicle->GetVelocity().Mag();

	return FSM_Continue;
}
CTask::FSM_Return CTaskMoveGoToVehicleDoor::StateInitial_OnUpdate(CPed * pPed)
{
	if(ShouldQuit(pPed))
		return FSM_Quit;

	CalcTargetPositions(pPed, m_vTargetDoorPos, m_vTargetBoardingPoint);

	/*
	if(IsAlreadyInPositionToEnter(pPed))
	{
		return FSM_Quit;
	}
	*/
	int iNewState = ChooseGoToDoorState(pPed);
	if(iNewState >= 0)
	{
		SetState(iNewState);
	}
	else
	{
		//! Inform parent that we have failed.
		m_bFailedToNavigateToVehicle = true;
		return FSM_Quit;
	}

	return FSM_Continue;
}

// Allow a way of modifying the base ms_fMinUpdateFreq/ms_fMaxUpdateFreq for use in CalcTaskUpdateFrequency().
// Since these two values are already the absolute fastest we would want to use, we should only return values
// from this function which are greater or equal to 1.0f
// (Added in response to url:bugstar:1469753)

float CTaskMoveGoToVehicleDoor::GetUpdateFrequencyMultiplier(CPed * pPed) const
{
	bool bComplexVehicle = false;
	if(m_pTargetVehicle && m_pTargetVehicle->GetIsTypeVehicle())
	{
		CVehicle * pVehicle = (CVehicle*)m_pTargetVehicle.Get();
		if(pVehicle->InheritsFromPlane() || pVehicle->InheritsFromHeli())
			bComplexVehicle = true;
	}

	CPed * pLocalPlayer = FindPlayerPed();
	if(pLocalPlayer && bComplexVehicle)
	{
		if(pPed == pLocalPlayer)
			return 1.0f;
		if(pPed->GetPedGroupIndex()!=PEDGROUP_INDEX_NONE && pPed->GetPedGroupIndex()==pLocalPlayer->GetPedGroupIndex())
			return 1.0f;
	}

	// We could possibly use a lower frequency here for non-complex vehicles, but returning a 2.0x multiplier gives us
	// the same update freq which has been used for along time in this code so I'm reluctant to change this now.
	return 2.0f;
}

void CTaskMoveGoToVehicleDoor::CalcTaskUpdateFrequency(CPed * pPed)
{
	const float fUpdateFreqMult = GetUpdateFrequencyMultiplier(pPed);
	const float fMinUpdateFreq = ms_fMinUpdateFreq * fUpdateFreqMult;
	const float fMaxUpdateFreq = ms_fMaxUpdateFreq * fUpdateFreqMult;

	if(m_pTargetVehicle)
	{
		const Vector3 vDiff = VEC3V_TO_VECTOR3(m_pTargetVehicle->GetTransform().GetPosition() - pPed->GetTransform().GetPosition());
		const float fDist = vDiff.Mag();

		float fUpdateFreq;

		if(fDist < ms_fMaxUpdateDist)
		{
			fUpdateFreq = fMaxUpdateFreq;
		}
		else if(fDist > ms_fMinUpdateDist)
		{
			fUpdateFreq = fMinUpdateFreq;
		}
		else
		{
			const float t = (fDist - ms_fMaxUpdateDist) / (ms_fMinUpdateDist - ms_fMaxUpdateDist);
			fUpdateFreq = fMaxUpdateFreq + ((fMinUpdateFreq - fMaxUpdateFreq) * t);
		}

		Assert(fUpdateFreq > 0.0f);

		const u32 iUpdateTime = m_iLastTaskUpdateTimeMs + ((s32)(fUpdateFreq * 1000.0f));
		m_bCanUpdateTaskThisFrame = fwTimer::GetTimeInMilliseconds() >= iUpdateTime;

		// To prevent peds breaking out of goto-point when very close; for example on sports cars where LOS can fail since its so cramped by the door
		// When this happens peds will revert to point-routes, which takes them out to the exterior of the convex hull.

		if(m_bCanUpdateTaskThisFrame &&	(GetState()==State_GoDirectlyToBoardingPoint || GetState()==State_FollowPointRouteToBoardingPoint))
		{
			static const float fCommitToTaskDistLongSqr = 2.0f*2.0f;	
			static const float fCommitToTaskDistShortSqr = 0.25f*0.25f;	
			const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			const float fDistSqr = (m_vTargetBoardingPoint - vPedPos).XYMag2();
			float fCommitToTaskDistSqr = (pPed->GetPedResetFlag(CPED_RESET_FLAG_UseTighterEnterVehicleSettings) ? fCommitToTaskDistShortSqr : fCommitToTaskDistLongSqr);

			if(fDistSqr < fCommitToTaskDistSqr && !HasTargetPosChanged())
			{
				m_bCanUpdateTaskThisFrame = false;
			}
		}
	}
	else
	{
		m_iLastTaskUpdateTimeMs = 0;
		m_bCanUpdateTaskThisFrame = true;
	}
}

bool CTaskMoveGoToVehicleDoor::HasCarMoved() const
{
	if(!m_pTargetVehicle)
		return false;

	// Force update every frame if we're on the blimp; expensive yes - but a simpler solution than retransforming the route
	if( IsVehicleLandedOnBlimp() )
		return true;

	const Vector3 vDiff = VEC3V_TO_VECTOR3(m_pTargetVehicle->GetTransform().GetPosition()) - m_vInitialCarPos;
	if(vDiff.Mag2() > ms_fCarMovedEpsSqr)
		return true;

	if(DotProduct(m_vInitialCarXAxis, VEC3V_TO_VECTOR3(m_pTargetVehicle->GetTransform().GetA())) < ms_fCarReorientatedDotEps)
		return true;

	if(DotProduct(m_vInitialCarYAxis, VEC3V_TO_VECTOR3(m_pTargetVehicle->GetTransform().GetB())) < ms_fCarReorientatedDotEps)
		return true;

	return false;
}

bool CTaskMoveGoToVehicleDoor::HasCarChangedSpeed() const
{
	// Added this extra check due to 2987, the original adjustment to the target pos for the vehicle speed might be quite large if we slow down after it
	if( !IsClose(m_fInitialSpeed, m_pTargetVehicle->GetVelocity().Mag(), ms_fCarChangedSpeedDelta) )
		return true;

	return false;
}

bool CTaskMoveGoToVehicleDoor::HasTargetPosChanged()
{
	Vector3 NewTargetDoor;
	Vector3 NewBoardingPoint;
	CalcTargetPositions(GetPed(), NewTargetDoor, NewBoardingPoint);

	static const float fMoveEps = rage::square(1.0f);
	if (NewTargetDoor.Dist2(GetTargetDoorPos()) > fMoveEps ||
		NewBoardingPoint.Dist2(GetTargetBoardingPointPos()) > fMoveEps)
	{
		m_vTargetDoorPos = NewTargetDoor;
		m_vTargetBoardingPoint = NewBoardingPoint;
		return true;
	}

	return false;
}

static float TARGET_HEIGHT_DELTA = 3.0f;

CTask::FSM_Return CTaskMoveGoToVehicleDoor::StateGoDirectlyToBoardingPoint_OnEnter(CPed * pPed)
{
	float fMbrToUse = ChooseGoToDoorMBR(pPed);

	float fSlowDownDistance = m_fSlowDownDistance;
	
	// Vehicle is moving and moving away from us we must so catch it!
	if (m_pTargetVehicle->GetVelocity().Mag2() > rage::square(1.0f) &&
		pPed->GetVelocity().Dot(m_pTargetVehicle->GetVelocity()) > 0.1f)
	{
		fSlowDownDistance = 0.0f;
	}

	// If we are within the slowdown distance we might aswell never slow down if we would run
	if (m_fMoveBlendRatio > MBR_RUN_BOUNDARY &&
		m_vTargetBoardingPoint.Dist2(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition())) < fSlowDownDistance * fSlowDownDistance)
	{
		fSlowDownDistance = 0.f;
	}

	CTaskMoveGoToPointAndStandStill * pGotoTaskStandStill = rage_new CTaskMoveGoToPointAndStandStill(
		fMbrToUse,
		m_vTargetBoardingPoint,
		m_fTargetRadius,
		fSlowDownDistance
	);

	pGotoTaskStandStill->SetTargetHeightDelta(TARGET_HEIGHT_DELTA);

	TUNE_GROUP_BOOL(ENTER_VEHICLE_TUNE, GROUP_MEMBERS_STOP_EXACTLY, true); 
	if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_WillArrestRatherThanJack) ||
		(GROUP_MEMBERS_STOP_EXACTLY && !pPed->IsAPlayerPed() && pPed->GetPedsGroup()))
	{
		pGotoTaskStandStill->SetUseRunstopInsteadOfSlowingToWalk(true);
		pGotoTaskStandStill->SetStopExactly(true);
	}
	else
	{
		pGotoTaskStandStill->SetUseRunstopInsteadOfSlowingToWalk(false);
	}

	SetNewTask(pGotoTaskStandStill);
	return FSM_Continue;
}
CTask::FSM_Return CTaskMoveGoToVehicleDoor::StateGoDirectlyToBoardingPoint_OnUpdate(CPed * pPed)
{
	float fMbrToUse = ChooseGoToDoorMBR(pPed);
	CTaskMove * pSubTask = (CTaskMove*)GetSubTask();
	if(pSubTask)
	{
		pSubTask->PropagateNewMoveSpeed(fMbrToUse);
	}

	pPed->SetPedResetFlag( CPED_RESET_FLAG_FollowingRoute, true );
	
	if(ms_bUseTighterTurnSettings || (ms_bUseTighterTurnSettingsInWater && pPed->GetIsInWater()) )
		pPed->SetPedResetFlag( CPED_RESET_FLAG_UseTighterTurnSettings, true );

	if ((m_vTargetDoorPos - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition())).XYMag2() < ms_fTighterDistance * ms_fTighterDistance)
	{
		pPed->SetPedResetFlag( CPED_RESET_FLAG_UseTighterAvoidanceSettings, true );
		pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableSteeringAroundVehicles, true);
		pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableSteeringAroundObjects, true);
		pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableSteeringAroundNavMeshEdges, true);
	}

	// Allow peds to break out if they are stuck running into something
	const s32 iStaticCounterStuck = (NetworkInterface::IsGameInProgress() && pPed->IsLocalPlayer()) ? CStaticMovementScanner::ms_iStaticCounterStuckLimitPlayerMP : CStaticMovementScanner::ms_iStaticCounterStuckLimit;
	if(pPed->GetPedIntelligence()->GetEventScanner()->GetStaticMovementScanner().GetStaticCounter() >= iStaticCounterStuck)
	{
		// If this is the player ped and this isn't a boat - then instruct the CTaskEnterVehicle to warp the player in if it is possible.
		bool bIsBoatOrSeaplane = ((CVehicle*)m_pTargetVehicle.Get())->InheritsFromBoat() || ((CVehicle*)m_pTargetVehicle.Get())->pHandling->GetSeaPlaneHandlingData();
		if( m_pTargetVehicle->GetIsTypeVehicle() && !bIsBoatOrSeaplane && WarpIfPossible())
		{
			return FSM_Quit;
		}

		CEventStaticCountReachedMax staticEvent(GetTaskType());
		pPed->GetPedIntelligence()->AddEvent(staticEvent);
	}

	TUNE_GROUP_BOOL(ENTER_VEHICLE_TUNE, bStrafeIfAlreadyClose, false); // TODO: investigate why peds can no longer strafe without adpoting an aiming pose
	if(bStrafeIfAlreadyClose && IsAlreadyInPositionToEnter(pPed))
	{
		Vector3 vPos = VEC3V_TO_VECTOR3(m_pTargetVehicle->GetTransform().GetPosition());
		Vector3 vFwd = VEC3V_TO_VECTOR3(m_pTargetVehicle->GetTransform().GetForward());
		pPed->SetIsStrafing(true);
		pPed->SetDesiredHeading(vPos + (vFwd * 20.0f));
	}

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		if(!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_WillArrestRatherThanJack) && !HasAchievedDoor())
		{
			SetState(State_GoToDoor);
			return FSM_Continue;
		}
		else
		{
			return FSM_Quit;
		}
	}

	if( HasCarChangedSpeed() )
		m_bCarHasChangedSpeed = true;

	if(m_bCanUpdateTaskThisFrame && (m_bUpdateGotoTasksEvenIfCarHasNotMoved || m_bCarHasChangedSpeed || HasCarMoved()))
	{
		SetState(State_Initial);
		return FSM_Continue;
	}

	return FSM_Continue;
}
CTask::FSM_Return CTaskMoveGoToVehicleDoor::StateFollowPointRouteToBoardingPoint_OnEnter(CPed * pPed)
{
	float fMbrToUse = ChooseGoToDoorMBR(pPed);

	// Determine what target radius to use for the point-route subtask:
	// If this vehicle is very narrow (for example, bikes) and we are in front/behind of it, then use a
	// smaller target radius - otherwise we may have trouble getting around the narrow front edge since
	// the first route segment might complete instantly.
	float fWaypointRadius = -1.0f;
	static dev_float fWaypointRadiusMult = 0.5f;

	const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	CEntityBoundAI boundAI(*m_pTargetVehicle.Get(), vPedPos.z, pPed->GetCapsuleInfo()->GetHalfWidth());
	int iSide = boundAI.ComputeHitSideByPosition(vPedPos);
	if(iSide == CEntityBoundAI::FRONT || iSide == CEntityBoundAI::REAR)
	{
		Vector3 vCorners[4];
		boundAI.GetCorners(vCorners);
		Vector3 vFrontEdge = vCorners[CEntityBoundAI::FRONT_RIGHT] - vCorners[CEntityBoundAI::FRONT_LEFT];
		const float fSmallEdgeWidth = (pPed->GetCapsuleInfo()->GetHalfWidth()*2.0f) + 1.5f;
		if(vFrontEdge.Mag() < fSmallEdgeWidth)
		{
			fWaypointRadius = CTaskMoveGoToPoint::ms_fTargetRadius*fWaypointRadiusMult;
		}
	}

	Assert(m_pPointRouteRouteToDoor && m_pPointRouteRouteToDoor->GetSize());
	CTaskMoveFollowPointRoute * pPointRouteTask = rage_new CTaskMoveFollowPointRoute(
		fMbrToUse,
		*m_pPointRouteRouteToDoor,
		CTaskMoveFollowPointRoute::TICKET_SINGLE,
		m_fTargetRadius,
		m_fSlowDownDistance,
		false,
		true,
		false,
		0,
		fWaypointRadius
	);

	pPointRouteTask->SetUseRunstopInsteadOfSlowingToWalk(false);
	if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_WillArrestRatherThanJack))
	{
		pPointRouteTask->SetStopExactly(true);
	}

	SetNewTask(pPointRouteTask);
	return FSM_Continue;
}
CTask::FSM_Return CTaskMoveGoToVehicleDoor::StateFollowPointRouteToBoardingPoint_OnUpdate(CPed * pPed)
{
	float fMbrToUse = ChooseGoToDoorMBR(pPed);
	CTaskMove * pSubTask = (CTaskMove*)GetSubTask();
	if(pSubTask)
	{
		pSubTask->PropagateNewMoveSpeed(fMbrToUse);
	}

	pPed->SetPedResetFlag( CPED_RESET_FLAG_FollowingRoute, true );

	if(m_pTargetVehicle->GetIsTypeVehicle() && ( ((CVehicle*)m_pTargetVehicle.Get())->InheritsFromBicycle() || ((CVehicle*)m_pTargetVehicle.Get())->InheritsFromBike() ) )
	{
		pPed->SetPedResetFlag( CPED_RESET_FLAG_Block180Turns, true );
	}

	if(ms_bUseTighterTurnSettings || (ms_bUseTighterTurnSettingsInWater && pPed->GetIsInWater()) )
		pPed->SetPedResetFlag( CPED_RESET_FLAG_UseTighterTurnSettings, true );

	if ((m_vTargetDoorPos - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition())).XYMag2() < ms_fTighterDistance * ms_fTighterDistance)
	{
		pPed->SetPedResetFlag( CPED_RESET_FLAG_UseTighterAvoidanceSettings, true );
		pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableSteeringAroundVehicles, true);
		pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableSteeringAroundObjects, true);
		pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableSteeringAroundNavMeshEdges, true);
	}

	// Allow peds to break out if they are stuck running into something
	const s32 iStaticCounterStuck = (NetworkInterface::IsGameInProgress() && pPed->IsLocalPlayer()) ? CStaticMovementScanner::ms_iStaticCounterStuckLimitPlayerMP : CStaticMovementScanner::ms_iStaticCounterStuckLimit;
	if(pPed->GetPedIntelligence()->GetEventScanner()->GetStaticMovementScanner().GetStaticCounter() >= iStaticCounterStuck)
	{
		// If this is the player ped and this isn't a boat - then instruct the CTaskEnterVehicle to warp the player in if it is possible.
		if( m_pTargetVehicle->GetIsTypeVehicle() && !((CVehicle*)m_pTargetVehicle.Get())->InheritsFromBoat() && WarpIfPossible())
		{
			return FSM_Quit;
		}

		CEventStaticCountReachedMax staticEvent(GetTaskType());
		pPed->GetPedIntelligence()->AddEvent(staticEvent);
	}

	// Be lenient about the Z-height of these waypoints.  They are all at the height of the entry-point
	// to the vehicle, which means otherwise at extreme angles we will have a hard time reaching waypoints.
	if(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_MOVE_FOLLOW_POINT_ROUTE &&
		(pPed->GetPedIntelligence()->GetActiveSimplestMovementTask()->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT ||
		pPed->GetPedIntelligence()->GetActiveSimplestMovementTask()->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT_ON_ROUTE))
	{
		((CTaskMoveGoToPoint*)pPed->GetPedIntelligence()->GetActiveSimplestMovementTask())->SetTargetHeightDelta(16.0f);
	}

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		if(!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_WillArrestRatherThanJack) && !HasAchievedDoor())
		{
			SetState(State_GoToDoor);
			return FSM_Continue;
		}
		else
		{
			return FSM_Quit;
		}
	}

	if( HasCarChangedSpeed() )
		m_bCarHasChangedSpeed = true;

	// Entering the Nokota, the player seems to get stuck in a loop - I think there is some disparity between
	// the clearance tests in StringPullStartOfRoute() and those in the CVehicleHullAI.
	// Possible workaround: If we're following a point-route to enter this vehicle, and it hasn't moved, don't change states.
	const bool bNokota = m_pTargetVehicle->GetModelIndex() == MI_PLANE_NOKOTA.GetModelIndex();
	const bool bSuppressUpdateWorkaround = (bNokota && !HasCarMoved());

	if(!bSuppressUpdateWorkaround && m_bCanUpdateTaskThisFrame && (m_bUpdateGotoTasksEvenIfCarHasNotMoved || m_bCarHasChangedSpeed || HasCarMoved()))
	{
		SetState(State_Initial);
		return FSM_Continue;
	}

	// Handle closing of rear door if it is an obstruction on this vehicle
	if( ProcessClosingRearDoor(pPed) )
	{
		// If we have no boarding points, then once we've closed the rear door we can go directly to the entry position
		if( !m_pTargetVehicle->GetIsTypeVehicle() ||
			((CVehicle*)m_pTargetVehicle.Get())->GetBoardingPoints()==NULL ||
				((CVehicle*)m_pTargetVehicle.Get())->GetBoardingPoints()->GetNumBoardingPoints()==0)
		{
			SetState(State_GoToDoor);
			return FSM_Continue;
		}
	}

	return FSM_Continue;
}
CTask::FSM_Return CTaskMoveGoToVehicleDoor::StateFollowNavMeshToBoardingPoint_OnEnter(CPed * pPed)
{
	float fMbrToUse = ChooseGoToDoorMBR(pPed);

	CTaskMoveFollowNavMesh * pNavMeshTask = rage_new CTaskMoveFollowNavMesh(
		fMbrToUse,
		m_vTargetBoardingPoint,
		m_fTargetRadius,
		m_fSlowDownDistance
	);
	pNavMeshTask->SetDontStandStillAtEnd(true);
	pNavMeshTask->SetDontAdjustTargetPosition(true);
	pNavMeshTask->SetAllowToPushVehicleDoorsClosed(true);
	pNavMeshTask->SetPullFromEdgeExtra(true);

	if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_WillArrestRatherThanJack))
	{
		pNavMeshTask->SetStopExactly(true);
	}
	
	Vector2 vMbr;
	pPed->GetMotionData()->GetCurrentMoveBlendRatio(vMbr);
	if(vMbr.Mag() > MOVEBLENDRATIO_STILL)
		pNavMeshTask->SetKeepMovingWhilstWaitingForPath(true);

	SetNewTask(pNavMeshTask);
	return FSM_Continue;
}
CTask::FSM_Return CTaskMoveGoToVehicleDoor::StateFollowNavMeshToBoardingPoint_OnUpdate(CPed * pPed)
{
	float fMbrToUse = ChooseGoToDoorMBR(pPed);
	CTaskMove * pSubTask = (CTaskMove*)GetSubTask();
	if(pSubTask)
	{
		pSubTask->PropagateNewMoveSpeed(fMbrToUse);
	}

	//--------------------------------------------------------------------------------------------------------
	// If this is the player running CTaskPlayerOnFoot, and the navmesh task is stuck unable to find a path -
	// then warp the player directly to the door.

	if(pSubTask && pSubTask->GetTaskType()==CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH)
	{
		if(((CTaskMoveFollowNavMesh*)pSubTask)->IsUnableToFindRoute())
		{
			if(WarpIfPossible())
			{
				return FSM_Quit;
			}
		}
	}

	pPed->SetPedResetFlag( CPED_RESET_FLAG_FollowingRoute, true );

	if(ms_bUseTighterTurnSettings || (ms_bUseTighterTurnSettingsInWater && pPed->GetIsInWater()) )
		pPed->SetPedResetFlag( CPED_RESET_FLAG_UseTighterTurnSettings, true );

	if ((m_vTargetDoorPos - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition())).XYMag2() < ms_fTighterDistance * ms_fTighterDistance)
	{
		pPed->SetPedResetFlag( CPED_RESET_FLAG_UseTighterAvoidanceSettings, true );
		pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableSteeringAroundVehicles, true);
		pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableSteeringAroundObjects, true);
		pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableSteeringAroundNavMeshEdges, true);
	}


	// Allow peds to break out if they are stuck running into something
	const s32 iStaticCounterStuck = (NetworkInterface::IsGameInProgress() && pPed->IsLocalPlayer()) ? CStaticMovementScanner::ms_iStaticCounterStuckLimitPlayerMP : CStaticMovementScanner::ms_iStaticCounterStuckLimit;
	if(pPed->GetPedIntelligence()->GetEventScanner()->GetStaticMovementScanner().GetStaticCounter() >= iStaticCounterStuck)
	{
		// If this is the player ped and this isn't a boat - then instruct the CTaskEnterVehicle to warp the player in if it is possible.
		if( m_pTargetVehicle->GetIsTypeVehicle() && !((CVehicle*)m_pTargetVehicle.Get())->InheritsFromBoat() && WarpIfPossible())
		{
			return FSM_Quit;
		}
	}

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		if(!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_WillArrestRatherThanJack) && !HasAchievedDoor())
		{
			SetState(State_GoToDoor);
			return FSM_Continue;
		}
		else
		{
			return FSM_Quit;
		}
	}

	if( HasCarChangedSpeed() )
		m_bCarHasChangedSpeed = true;

	if(m_bCanUpdateTaskThisFrame && (m_bCarHasChangedSpeed || HasCarMoved() ) )
	{
		SetState(State_Initial);
		return FSM_Continue;
	}
	return FSM_Continue;
}
CTask::FSM_Return CTaskMoveGoToVehicleDoor::StateGoToDoor_OnEnter(CPed *pPed)
{
	float fMbrToUse = ChooseGoToDoorMBR(pPed);

	float fSlowDownDistance = m_fSlowDownDistance;
	
	// Vehicle is moving and moving away from us we must so catch it!
	if (m_pTargetVehicle->GetVelocity().Mag2() > rage::square(1.0f) &&
		pPed->GetVelocity().Dot(m_pTargetVehicle->GetVelocity()) > 0.1f)
	{
		fSlowDownDistance = 0.0f;
	}

	// If we are within the slowdown distance we might aswell never slow down if we would run
	if (m_fMoveBlendRatio > MBR_RUN_BOUNDARY &&
		m_vTargetDoorPos.Dist2(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition())) < fSlowDownDistance * fSlowDownDistance)
	{
		fSlowDownDistance = 0.f;
	}

	CTaskMoveGoToPointAndStandStill * pGotoTaskStandStill = rage_new CTaskMoveGoToPointAndStandStill(
		fMbrToUse,
		m_vTargetDoorPos,
		m_fTargetRadius,
		fSlowDownDistance
		);

	pGotoTaskStandStill->SetTargetHeightDelta(TARGET_HEIGHT_DELTA);

	SetNewTask(pGotoTaskStandStill);
	return FSM_Continue;
}
CTask::FSM_Return CTaskMoveGoToVehicleDoor::StateGoToDoor_OnUpdate(CPed * pPed)
{
	float fMbrToUse = ChooseGoToDoorMBR(pPed);
	CTaskMove * pSubTask = (CTaskMove*)GetSubTask();
	if(pSubTask)
	{
		pSubTask->PropagateNewMoveSpeed(fMbrToUse);
	}

	pPed->SetPedResetFlag( CPED_RESET_FLAG_FollowingRoute, true );

	if(ms_bUseTighterTurnSettings || (ms_bUseTighterTurnSettingsInWater && pPed->GetIsInWater()) )
		pPed->SetPedResetFlag( CPED_RESET_FLAG_UseTighterTurnSettings, true );

	if ((m_vTargetDoorPos - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition())).XYMag2() < ms_fTighterDistance * ms_fTighterDistance)
	{
		pPed->SetPedResetFlag( CPED_RESET_FLAG_UseTighterAvoidanceSettings, true );
		pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableSteeringAroundVehicles, true);
		pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableSteeringAroundObjects, true);
		pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableSteeringAroundNavMeshEdges, true);
	}

	// Allow peds to break out if they are stuck running into something
	const s32 iStaticCounterStuck = (NetworkInterface::IsGameInProgress() && pPed->IsLocalPlayer()) ? CStaticMovementScanner::ms_iStaticCounterStuckLimitPlayerMP : CStaticMovementScanner::ms_iStaticCounterStuckLimit;
	if(pPed->GetPedIntelligence()->GetEventScanner()->GetStaticMovementScanner().GetStaticCounter() >= iStaticCounterStuck)
	{
		// If this is the player ped and this isn't a boat - then instruct the CTaskEnterVehicle to warp the player in if it is possible.
		if( m_pTargetVehicle->GetIsTypeVehicle() && !((CVehicle*)m_pTargetVehicle.Get())->InheritsFromBoat() && WarpIfPossible())
		{
			return FSM_Quit;
		}

		CEventStaticCountReachedMax staticEvent(GetTaskType());
		pPed->GetPedIntelligence()->AddEvent(staticEvent);
	}

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		return FSM_Quit;
	}

	if( HasCarChangedSpeed() || HasCarMoved() )
	{
		CalcTargetPositions(pPed, m_vTargetDoorPos, m_vTargetBoardingPoint);
		CTaskMove * pSubTask = (CTaskMove*)GetSubTask();
		if(pSubTask)
		{
			pSubTask->SetTarget(pPed, VECTOR3_TO_VEC3V(m_vTargetDoorPos));
		}
	}

	return FSM_Continue;
}

bool CTaskMoveGoToVehicleDoor::WarpIfPossible()
{
	static dev_float fMaxWarpDistance = 7.5f;

	CPed * pPed = GetPed();
	if(pPed && pPed->IsLocalPlayer())
	{
		if( (VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - m_vTargetDoorPos).XYMag2() < fMaxWarpDistance*fMaxWarpDistance )
		{
			CTaskPlayerOnFoot * pTaskPlayer = (CTaskPlayerOnFoot*)pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PLAYER_ON_FOOT);
			if(pTaskPlayer && pTaskPlayer->GetState()==CTaskPlayerOnFoot::STATE_GET_IN_VEHICLE)
			{
				CTaskEnterVehicle * pTaskEnterVehicle = (CTaskEnterVehicle*)pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE);
				if(pTaskEnterVehicle && pTaskEnterVehicle->GetState()==CTaskEnterVehicle::State_GoToDoor)
				{
					return pTaskEnterVehicle->WarpIfPossible();
				}
			}
		}
	}
	return false;
}

bool CTaskMoveGoToVehicleDoor::ShouldQuit(CPed * pPed) const
{
	if(!m_pTargetVehicle)
		return true;

	if(!IsVehicleInRange(pPed))
		return true;

	return false;
}

bool CTaskMoveGoToVehicleDoor::IsVehicleInRange(CPed * pPed) const
{
	Vector3 vDiff = VEC3V_TO_VECTOR3(m_pTargetVehicle->GetTransform().GetPosition() - pPed->GetTransform().GetPosition());
	return (vDiff.Mag2() < m_fMaxSeekDistance*m_fMaxSeekDistance);
}

// NAME : CalcTargetPos
// PURPOSE : Calculates the target door position, and target boarding point position.
// These may be one and the same.  For purposes of task completion, the door position should be examined.
void CTaskMoveGoToVehicleDoor::CalcTargetPositions(const CPed * pPed, Vector3 & vOutTargetDoorPos, Vector3 & vOutTargetBoardingPointPos)
{
	const CModelSeatInfo* pModelSeatinfo = NULL;
	if (m_pTargetVehicle->GetIsTypePed()) 
	{	
		pModelSeatinfo  = static_cast<const CPed*>(m_pTargetVehicle.Get())->GetPedModelInfo()->GetModelSeatInfo();
	} 
	else 
	{		
		pModelSeatinfo  = static_cast<const CVehicle*>(m_pTargetVehicle.Get())->GetVehicleModelInfo()->GetModelSeatInfo();		
	} 	

	const CEntryExitPoint * pEntryPoint = pModelSeatinfo->GetEntryExitPoint(m_iEntryPoint);
	if (taskVerifyf(pEntryPoint, "NULL entry point info for entrypoint %u", m_iEntryPoint))
	{
		pEntryPoint->GetEntryPointPosition(m_pTargetVehicle, pPed, vOutTargetDoorPos, false, pPed->GetIsInWater());
	}
	else
	{
		vOutTargetDoorPos = VEC3V_TO_VECTOR3(m_pTargetVehicle->GetTransform().GetPosition());
	}

	if(m_vOffsetModifier.IsClose(VEC3_ZERO, 0.1f))
		m_vOffsetModifier = CTaskEnterVehicle::CalculateLocalSpaceEntryModifier(m_pTargetVehicle, m_iEntryPoint);
	const Matrix34 vehMat = MAT34V_TO_MATRIX34(m_pTargetVehicle->GetMatrix());
	Vector3 vWldOffset = m_vOffsetModifier;
	vehMat.Transform3x3(vWldOffset);
	vOutTargetDoorPos += vWldOffset;
		
	if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_WillArrestRatherThanJack))
	{
		AdjustPosForVehicleSpeed(pPed, vOutTargetDoorPos);
		float fDesiredDistFromVehicle = CTaskArrestPed::sm_Tunables.m_TargetDistanceFromVehicleEntry;
		vOutTargetBoardingPointPos = vOutTargetDoorPos - (VEC3V_TO_VECTOR3(m_pTargetVehicle->GetTransform().GetRight()) * fDesiredDistFromVehicle);
	}
	else
	{
		// JB: This boarding-point code got lost in a recent check-in, and it feels too risky
		// to re-enable across the board.  I am enabling it on specific models instead.

		CBoardingPoint boardingPointLocalSpace;

		if( UseBoardingPointsForVehicle() && m_pTargetVehicle->GetIsTypeVehicle() &&
			((CVehicle*)m_pTargetVehicle.Get())->GetClosestBoardingPoint(vOutTargetDoorPos, boardingPointLocalSpace))
		{
			vOutTargetBoardingPointPos = boardingPointLocalSpace.GetPosition();
			vehMat.Transform(vOutTargetBoardingPointPos);

			AdjustPosForVehicleSpeed(pPed, vOutTargetBoardingPointPos);
			AdjustPosForVehicleSpeed(pPed, vOutTargetDoorPos);
		}
		else
		{
			AdjustPosForVehicleSpeed(pPed, vOutTargetDoorPos);
			vOutTargetBoardingPointPos = vOutTargetDoorPos;
		}
	}
}

bool CTaskMoveGoToVehicleDoor::UseBoardingPointsForVehicle() const
{
	if(m_pTargetVehicle->GetModelIndex() == MI_PLANE_DUSTER.GetModelIndex())
	{
		return true;
	}
	return false;
}

void CTaskMoveGoToVehicleDoor::AdjustPosForVehicleSpeed(const CPed * pPed, Vector3 & vPos) const
{
	static const float fMaxAdjustDist = 8.0f;
	static const float fMinAdjustSpeed = 0.2f;

	const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	const Vector3 vVehiclePos = VEC3V_TO_VECTOR3(m_pTargetVehicle->GetTransform().GetPosition());

	if( IsVehicleLandedOnBlimp() )
		return;

	// If we're already close to the target position, then don't adjust the target position
	Vector3 vDiff = vPedPos - vPos;
	if(vDiff.Mag2() > fMaxAdjustDist*fMaxAdjustDist)
		return;
	// If vehicle is moving under some speed threshold then don't apply
	const Vector3 & vMoveSpeed = m_pTargetVehicle->GetVelocity();
	if(vMoveSpeed.Mag2() < fMinAdjustSpeed*fMinAdjustSpeed)
		return;

	// If the ped is ahead of the vehicle wrt to the vehicle's velocity, then don't apply
	static dev_float fThreshold = 0.707f;
	Vector3 vVehicleToPed = vPedPos - vVehiclePos;
	if(vVehicleToPed.Mag2() > SMALL_FLOAT)
	{
		vVehicleToPed.Normalize();
		if(DotProduct(vVehicleToPed, vMoveSpeed) > fThreshold)
			return;
	}

	const float fDistAway = vDiff.XYMag();
	const float fMoveRatio = Clamp(m_fMoveBlendRatio, MOVEBLENDRATIO_STILL, MOVEBLENDRATIO_SPRINT);
	float fPosWeight = 1.f;

	vDiff.z = 0.f;
	if (vDiff.Mag2() > SMALL_FLOAT)
	{
		static dev_float fMinWeight = 0.05f;		//

		vDiff.Normalize();
		Vector3 vVehVel = m_pTargetVehicle->GetVelocity();
		vVehVel.z = 0.f;
		vVehVel.Normalize();

		// So basically we scale by how much the vehicle is moving towards us, fully towards means we predict much less distance
		// This will make the prediction more accurate since the time it take to nail the point is much shorter when the vehicle is moving 
		// towards us.
		// This could in theory also be applied for when the vehicle is moving away from us and if we like that,
		// just remove the Max around the DOT.
		fPosWeight = Max(1.f - Max(vVehVel.Dot(vDiff), 0.f), fMinWeight);
	}

	CMoveBlendRatioSpeeds speeds;
	CTaskMotionBase* pTask = pPed->GetCurrentMotionTask();
	pTask->GetMoveSpeeds(speeds);
	const float * fMoveSpeeds = speeds.GetSpeedsArray();
	const float fSpeed = pTask->GetActualMoveSpeed(fMoveSpeeds, fMoveRatio);

	if(fSpeed < 0.01f)	// Safeguard against div by zero
		return;

	const float t = fDistAway / fSpeed;
	const Vector3 vLookAhead = vMoveSpeed * t;
	vPos += vLookAhead * fPosWeight;
}

bool CTaskMoveGoToVehicleDoor::ShouldNavigateToDoor(CPed * pPed)
{
	//------------------------------------------------------------------------------
	// Always navigate to the door if a long distance away
	// This avoids issuing extremely large capsule probes (see url:bugstar:1092862)

	static dev_float fAlwaysNavigateDistanceSqr = 30.0f * 30.0f;
	const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	const float fDistSqrToTarget = (m_vTargetBoardingPoint - vPedPosition).Mag2();
	if(fDistSqrToTarget > fAlwaysNavigateDistanceSqr)
		return true;

	//*********************************************************************
	// Can we go directly to this position without hitting any obstacles ?

	const float fPedWidth = pPed->GetCapsuleInfo()->GetHalfWidth();
	const float fGroundToRoot = pPed->GetCapsuleInfo()->GetGroundToRootOffset();
	const float fPedHeight = pPed->GetCapsuleInfo()->GetMaxSolidHeight();

	const s32 iNumTests = 3;

	Vector3 vTestOffsets[] =
	{
		Vector3(0.0f, 0.0f, -fGroundToRoot + fPedWidth + 0.25f),
		Vector3(0.0f, 0.0f, 0.0f),
		Vector3(0.0f, 0.0f, fPedHeight - fPedWidth)
	};

	u32 iTestFlags = ArchetypeFlags::GTA_GLASS_TYPE | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_BOX_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE;
	if(!IsExcludingAllMapTypes(pPed))
	{
		iTestFlags |= ArchetypeFlags::GTA_ALL_MAP_TYPES;
	}

	WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
	bool bLosClear = true;

	for(s32 t=0; t<iNumTests; t++)
	{
		capsuleDesc.SetCapsule(vPedPosition + vTestOffsets[t], m_vTargetBoardingPoint + vTestOffsets[t], pPed->GetCapsuleInfo()->GetHalfWidth());
		capsuleDesc.SetIncludeFlags(iTestFlags);
		capsuleDesc.SetExcludeEntity(m_pTargetVehicle, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS);
		capsuleDesc.SetContext(WorldProbe::LOS_GeneralAI);

		WorldProbe::CShapeTestFixedResults<> capsuleResult;
		capsuleDesc.SetResultsStructure(&capsuleResult);

		bool bClearOfObstacles = !WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc);
		if (!bClearOfObstacles)
		{
			u32 i = 0;
			for ( ; i < capsuleResult.GetNumHits(); ++i)
			{
				// Not sure if we should allow null here or not, maybe just assert on null?
				const CEntity* pEntity = CPhysics::GetEntityFromInst(capsuleResult[i].GetHitInst());
				if(pEntity)
				{
					CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();

					bool bIsFixedModelInfo = (pModelInfo ? pModelInfo->GetIsFixed() : true);
					bool bIsFixedForNavModelInfo = (pModelInfo ? pModelInfo->GetIsFixedForNavigation() : false);

					const bool bIsFixed = pEntity->GetIsTypeObject() &&
						(bIsFixedModelInfo || (pEntity->IsBaseFlagSet(fwEntity::IS_FIXED) && !((CObject*)pEntity)->GetFixedByScriptOrCode()));

					// If this is map geometry, or a fixed object, then check collision normal
					if(pEntity->GetIsTypeBuilding() || (pEntity->GetIsTypeObject() && (bIsFixed || bIsFixedForNavModelInfo)))
					{
						// If the collision normal is not facing up/down, then we can assume this is some kind of obstacle which we must navigate around
						const Vector3 & vHitNormal = capsuleResult[i].GetHitNormal();
						if(t > 0 || Abs(vHitNormal.z) < 0.707f)
						{
#if __BANK
							AI_LOG_WITH_ARGS("[VehicleEntryExit] CTaskMoveGoToVehicleDoor::ShouldNavigateToDoor() - LOS check failed (normal). Hit entity - %s[%p]\n",
								pEntity->GetIsDynamic() ? AILogging::GetDynamicEntityNameSafe(static_cast<const CDynamicEntity*>(pEntity)) : pEntity->GetModelName(),
								pEntity);
#endif
							break;
						}
					}

					if (pModelInfo && CPathServerGta::ShouldAvoidObject(pEntity))
					{
#if __BANK
						AI_LOG_WITH_ARGS("[VehicleEntryExit] CTaskMoveGoToVehicleDoor::ShouldNavigateToDoor() - LOS check failed. Hit entity - %s[%p]\n",
							pEntity->GetIsDynamic() ? AILogging::GetDynamicEntityNameSafe(static_cast<const CDynamicEntity*>(pEntity)) : pEntity->GetModelName(),
							pEntity);
#endif
						break;
					}
				}
			}

			if (i == capsuleResult.GetNumHits())
				bClearOfObstacles = true;
		}

#if DEBUG_DRAW
		TUNE_GROUP_BOOL(VEHICLE_ENTRY_DEBUG, RENDER_GO_TO_DOOR_DEBUG_CAPSULE, false);
		if (RENDER_GO_TO_DOOR_DEBUG_CAPSULE)
		{
			Vector3 v1 = vPedPosition+vTestOffsets[t];
			Vector3 v2 = m_vTargetBoardingPoint+vTestOffsets[t];
			CTask::ms_debugDraw.AddCapsule(RCC_VEC3V(v1), RCC_VEC3V(v2), pPed->GetCapsuleInfo()->GetHalfWidth(), bClearOfObstacles ? Color_green : Color_red, 1000, 0, false);
		}
#endif

		if(!bClearOfObstacles)
		{
			bLosClear = false;
			break;
		}
	}

	return !bLosClear;
}

bool CTaskMoveGoToVehicleDoor::IsVehicleLandedOnBlimp() const
{
	return (m_pTargetVehicle && m_pTargetVehicle->GetIsTypeVehicle() && (((CVehicle*)m_pTargetVehicle.Get())->InheritsFromHeli() || ((CVehicle*)m_pTargetVehicle.Get())->InheritsFromPlane()) && CVehicleHullCalculator::IsVehicleLandedOnBlimp((CVehicle*)m_pTargetVehicle.Get()) );
}

int CTaskMoveGoToVehicleDoor::ChooseGoToDoorState(CPed * pPed)
{
	m_iLastTaskUpdateTimeMs = fwTimer::GetTimeInMilliseconds();
	m_iChooseStateCounter++;

	s32 iHullDirection = m_iCurrentHullDirection;
	m_iCurrentHullDirection = 0;

	CVehicle* pVeh = NULL;
	bool bIsVehicleBike = false;
	const CVehicleLayoutInfo* pLayout = NULL;	
	if (m_pTargetVehicle->GetIsTypePed()) 
	{	
		pLayout = static_cast<const CPed*>(m_pTargetVehicle.Get())->GetPedModelInfo()->GetLayoutInfo();		
	} 
	else 
	{
		pVeh = static_cast<CVehicle*>(m_pTargetVehicle.Get());		
		pLayout = pVeh->GetLayoutInfo();		
		bIsVehicleBike = pVeh->InheritsFromBike();
	} 	
	const bool bIsUpright = (m_pTargetVehicle->GetTransform().GetUp().GetZf() > 0.8f);

	//*******************************************************
	// Move the door out to the left/right side of the hull

	const Matrix34 vehMat = MAT34V_TO_MATRIX34(m_pTargetVehicle->GetMatrix());
	Vector2 vDoorDir(vehMat.a.x, vehMat.a.y);

	// Determine the direction of the entry point from its info
	const CVehicleEntryPointInfo* pEntryPointInfo = pLayout->GetEntryPointInfo(m_iEntryPoint);
	if (taskVerifyf(pEntryPointInfo, "NULL entry point info for entrypoint %u", m_iEntryPoint))
	{
		// Left entry
		if (pEntryPointInfo->GetVehicleSide() == CVehicleEntryPointInfo::SIDE_LEFT)
		{
			vDoorDir *= -1.0f;
		}
	}

	const bool bShouldNavigateToDoor = ShouldNavigateToDoor(pPed);

	// Avoid use of navmesh if we're standing on a vehicle, AND it is not the vehicle which we wish to enter OR it doesn't have its own navmesh.
	// This should fix url:bugstar:549524, and hopefully will not create any knock-on ill effects.
	bool bSuppressNavmesh =
		m_pTargetVehicle && m_pTargetVehicle->GetIsTypeVehicle() && pPed->GetGroundPhysical() &&
				pPed->GetGroundPhysical()->GetIsTypeVehicle() &&
					(m_pTargetVehicle != pPed->GetGroundPhysical() || ((CVehicle*)m_pTargetVehicle.Get())->m_nVehicleFlags.bHasDynamicNavMesh==false);

	//! Do not navigate into water from on land.
	bool bNeverUseNavMesh = false;
	CVehicle *pVehicle = NULL;
	if(m_pTargetVehicle->GetIsTypeVehicle() && !pPed->GetPedResetFlag(CPED_RESET_FLAG_DontSuppressUseNavMeshToNavigateToVehicleDoorWhenVehicleInWater))
	{
		pVehicle = static_cast<CVehicle*>(m_pTargetVehicle.Get());		
		if(pVehicle && pVehicle->InheritsFromBoat() && pVehicle->GetIsInWater() && !pPed->GetIsSwimming())
		{
			bNeverUseNavMesh = true;
			bSuppressNavmesh = true;
		}
	}

	const bool bLandedOnBlimp = IsVehicleLandedOnBlimp();
	const bool bSupressNavmeshForGarageInterior = pPed->GetPedResetFlag(CPED_RESET_FLAG_SuppressNavmeshForEnterVehicleTask);
	// If standing on the blimp, then never use the navmesh to navigate to the door of any vehicle
	if( bLandedOnBlimp || bSupressNavmeshForGarageInterior)
	{
		bNeverUseNavMesh = true;
		bSuppressNavmesh = true;
	}

	//-------------------
	// Hardcoded hacks

	if(pVehicle && pVehicle->GetModelIndex()==MI_PLANE_TITAN.GetModelIndex())
	{
		bNeverUseNavMesh = true;
	}

	bool bForceGoDirectlyToBoardingPoint = false;
	if (pVehicle && pVehicle->pHandling && pVehicle->pHandling->GetSeaPlaneHandlingData())
	{
		bForceGoDirectlyToBoardingPoint = true;
	}

	//***********************************************************************
	// If there's no building/object obstruction then don't use the navmesh

	if( !bShouldNavigateToDoor || bSuppressNavmesh )
	{
		//---------------------------------------------------------------------------------------
		// If the target is not a vehicle then go directly to the boarding point
		// (Really, what about horses?  This logic may need to be changed now)

		if(!pVeh)
		{
			return State_GoDirectlyToBoardingPoint;
		}

		//**********************************************************************************************************
		// If there is a route directly to the boarding pos without intersecting the vehicle itself, then go direct

		const bool bEntryPointBlockedByRearDoor = CTaskEnterVehicle::CheckIsEntryPointBlockedByRearDoor( m_pTargetVehicle, m_iEntryPoint );

		if( CanGoDirectlyToBoardingPoint(pPed) && !bEntryPointBlockedByRearDoor )
		{
			return State_GoDirectlyToBoardingPoint;
		}

		Vector2 vDoorPos2d(m_vTargetBoardingPoint.x, m_vTargetBoardingPoint.y);

		//------------------------------------------------------------------------------------------
		// For vehicles we pass in an exclusion list to the convex hull calculator, which prevents
		// our target door being added to the convex hull (url:bugstar:424873)

		Assert(m_pTargetVehicle->GetIsTypeVehicle());

		const CVehicle * pVehicle = static_cast<const CVehicle*>(m_pTargetVehicle.Get());
		Assert(pVehicle->GetVehicleModelInfo() && pVehicle->GetVehicleModelInfo()->GetModelSeatInfo());
		const CEntryExitPoint * pEntryPoint = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryExitPoint(m_iEntryPoint);

		eHierarchyId hullExclusionList[] =
		{
			CCarEnterExit::DoorBoneIndexToHierarchyID(pVehicle, pEntryPoint->GetDoorBoneIndex()), VEH_INVALID_ID
		};

		//----------------------------
		// Generate the convex hull

		const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		float fPedMinZ = vPedPosition.z - pPed->GetCapsuleInfo()->GetGroundToRootOffset();
		float fPedMaxZ = vPedPosition.z + pPed->GetCapsuleInfo()->GetMaxSolidHeight();

		CVehicleHullAI vehicleHull(pVeh);
		vehicleHull.Init(pPed->GetCapsuleInfo()->GetHalfWidth(), hullExclusionList, fPedMinZ, fPedMaxZ);

		// For bikes which are standing upright, we scale down the X axis of the convex hull a little.
		// This is to account for the lean when the bike is on its stand
		static Vector2 vBikeTweak( 0.8f, 0.9f );
		if(bIsVehicleBike && bIsUpright)
			vehicleHull.Scale( vBikeTweak );

		// If the door direction is non-zero then move outside along that vector
		if(vDoorDir.Mag2() > 0.1f)
		{
			vDoorDir.Normalize();
			vehicleHull.MoveOutside(vDoorPos2d, vDoorDir);
		}
		// Otherwise move outside in the edge-direction of the segment which this point is within
		else
		{
			vehicleHull.MoveOutside(vDoorPos2d);
		}

		// If entry-point is blocked by rear door, and we are coming from the rear of the vehicle,
		// then pull 'vDoorPos2d' back towards the rear slightly - to avoid ped overshooting the
		// get in position whilst navigating the rear door.
		if(bEntryPointBlockedByRearDoor)
		{
			const float vehPlaneD = - DotProduct(vehMat.b, vehMat.d);
			const float fPedVehEndDist = DotProduct(vPedPosition, vehMat.b) + vehPlaneD;
			static dev_float fNudgeDist = 0.0f; //0.25f;
			if(fPedVehEndDist < -1.0f)
			{
				vDoorPos2d.x -= vehMat.b.x * fNudgeDist;
				vDoorPos2d.y -= vehMat.b.y * fNudgeDist;
			}
		}

		if(m_pPointRouteRouteToDoor) { m_pPointRouteRouteToDoor->Clear(); }
		else { m_pPointRouteRouteToDoor = rage_new CPointRoute(); }

		u32 iLosTestFlags;

		if(bLandedOnBlimp)
		{
			iLosTestFlags = 0;
		}
		else
		{
			iLosTestFlags = ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE;
			if(!IsExcludingAllMapTypes(pPed))
			{
				iLosTestFlags |= ArchetypeFlags::GTA_ALL_MAP_TYPES;
			}
		}

		static dev_float fGroundNormalThresholdZ = 0.707f;

		Vector2 vPedPos(vPedPosition.x, vPedPosition.y);
		if( vehicleHull.CalcShortestRouteToTarget(pPed, vPedPos, vDoorPos2d, m_pPointRouteRouteToDoor, m_vTargetBoardingPoint.z, iLosTestFlags, pPed->GetCapsuleInfo()->GetHalfWidth(), &iHullDirection, fGroundNormalThresholdZ BANK_ONLY(, true)) )
		{
			// Add the target position, because for some vehicles the convex hull might be some distance away from the target pos
			if((m_vTargetBoardingPoint - m_pPointRouteRouteToDoor->Get(m_pPointRouteRouteToDoor->GetSize()-1)).XYMag2() > 0.5f)
			{
				m_pPointRouteRouteToDoor->Add(m_vTargetBoardingPoint);
			}

			bool bForcedToGoDirectly = false;

			if (pVehicle->GetLayoutInfo() && 
				pVehicle->GetLayoutInfo()->GetIgnoreFrontSeatsWhenOnVehicle() &&
				pPed->GetGroundPhysical() == pVehicle)
			{
				bForcedToGoDirectly = true;
			}

			if(!bForcedToGoDirectly && m_pPointRouteRouteToDoor->GetSize())
			{
				m_iCurrentHullDirection = iHullDirection;

				// The idea of this test is to detect cases where the player ped is inside a vehicle (e.g. back of a van)
				// and cannot get directly out to the closest edge of the convex hull.
				if (pPed->IsLocalPlayer() && pPed->GetGroundPhysical() == m_pTargetVehicle && 
					m_pTargetVehicle->GetIsTypeVehicle() && static_cast<CVehicle*>(m_pTargetVehicle.Get())->InheritsFromAutomobile())
				{
					TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, IN_VEHICLE_PROBE_OFFSET, 3.0f, 0.0f, 5.0f, 0.01f);
					const Vector3 vStart = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
					const Vector3 vEnd = vStart + Vector3(0.0f,0.0f,IN_VEHICLE_PROBE_OFFSET);
					WorldProbe::CShapeTestFixedResults<> probeResult;
					s32 iTypeFlags = ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_BOX_VEHICLE_TYPE;
					WorldProbe::CShapeTestProbeDesc probeDesc;
					probeDesc.SetDomainForTest(WorldProbe::TEST_IN_LEVEL);
					probeDesc.SetStartAndEnd(vStart, vEnd);
					probeDesc.SetIncludeEntity(m_pTargetVehicle);
					probeDesc.SetIncludeFlags(iTypeFlags);
					probeDesc.SetTypeFlags(ArchetypeFlags::GTA_AI_TEST);
					probeDesc.SetIsDirected(true);

					const bool bHit = WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc);
#if DEBUG_DRAW
					CTask::ms_debugDraw.AddLine(RCC_VEC3V(vStart), RCC_VEC3V(vEnd), bHit ? Color_red : Color_green, 2500, 0);
#endif // DEBUG_DRAW
					if (bHit)
					{
						AI_LOG("[VehicleEntryExit] CTaskMoveGoToVehicleDoor::ChooseGoToDoorState() - failed. Probe hit something while inside the vehicle\n");
						return -1;
					}
				}

				// The idea of this test is to detect cases where the ped is 'trapped' inside the vehicle's geometry
				// and cannot get directly out to the closest edge of the convex hull.
				// eg. trapped behind propeller on the "Duster", or rockets of the "Lazer"

				const bool bVelum = m_pTargetVehicle->GetModelIndex() == MI_PLANE_VELUM.GetModelIndex()
					|| (MI_PLANE_VELUM2.IsValid() && m_pTargetVehicle->GetModelIndex() == MI_PLANE_VELUM2.GetModelIndex());

				bool bIgnoreTrappedCheck = ((CVehicle*)m_pTargetVehicle.Get())->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_IGNORE_TRAPPED_HULL_CHECK);

				static dev_float sfRadiusExtra = 0.1f;
				const float fExtraRadius = bVelum ? 0.0f : sfRadiusExtra;  // HACK GTA
				if (!bIgnoreTrappedCheck && m_iChooseStateCounter==1 && m_pPointRouteRouteToDoor->GetSize()>1 &&
					m_pTargetVehicle->GetIsTypeVehicle() && ((CVehicle*)m_pTargetVehicle.Get())->InheritsFromPlane() &&
					vehicleHull.LiesInside( Vector2(vPedPosition, Vector2::kXY) ) &&
					!TestIsSegmentClear(vPedPosition, m_pPointRouteRouteToDoor->Get(1), pPed->GetCapsuleInfo(), true, 0.0f, NULL, fExtraRadius))
				{
					AI_LOG("[VehicleEntryExit] CTaskMoveGoToVehicleDoor::ChooseGoToDoorState() - failed. Trapped inside the vehicle geometry\n");
					return -1;
				}

				// HACK GTA - More haxx for heists B*2209487
				const bool bHydra = MI_PLANE_HYDRA.IsValid() && (m_pTargetVehicle->GetModelIndex() == MI_PLANE_HYDRA.GetModelIndex());
				if (bHydra)
				{
					CVehicle* pVeh = static_cast<CVehicle*>(m_pTargetVehicle.Get());
					if (pVeh->IsEntryIndexValid(m_iEntryPoint))
					{
						Vector3 vTargetPosition;
						Quaternion qTargetOrientation;
						CModelSeatInfo::CalculateEntrySituation(pVeh, pPed, vTargetPosition, qTargetOrientation, m_iEntryPoint);
						Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
						TUNE_GROUP_FLOAT(HYDRA_HACK, MAX_DIST_TO_FORCE_GO_DIRECTLY_TO_BOARDING_POINT, 2.5f, 0.0f, 5.0f, 0.01f);
						const float fPedToEntryXYMag2 = (vPedPos-vTargetPosition).XYMag2();
						aiDisplayf("Frame %i: Distance from ped to hydra entrypoint %i : %.2f", fwTimer::GetFrameCount(), m_iEntryPoint, sqrt(fPedToEntryXYMag2));

						if (fPedToEntryXYMag2 < square(MAX_DIST_TO_FORCE_GO_DIRECTLY_TO_BOARDING_POINT))
						{
							BANK_ONLY(aiDisplayf("Frame %i: Forcing ped %s to go directly to boarding point because distance to entry point < %.2f", fwTimer::GetFrameCount(), AILogging::GetDynamicEntityNameSafe(pPed), MAX_DIST_TO_FORCE_GO_DIRECTLY_TO_BOARDING_POINT);)
							return State_GoDirectlyToBoardingPoint;
						}
					}
				}

				const bool bValkyrie = (MI_HELI_VALKYRIE.IsValid() && (m_pTargetVehicle->GetModelIndex() == MI_HELI_VALKYRIE.GetModelIndex()))
					|| (MI_HELI_VALKYRIE2.IsValid() && (m_pTargetVehicle->GetModelIndex() == MI_HELI_VALKYRIE2.GetModelIndex()));

				if (bValkyrie && (m_pPointRouteRouteToDoor->GetSize() > 0) && !TestIsSegmentClear(vPedPosition, m_pPointRouteRouteToDoor->Get(0), pPed->GetCapsuleInfo(), true, 0.0f, NULL))
				{
					AI_LOG("[VehicleEntryExit] CTaskMoveGoToVehicleDoor::ChooseGoToDoorState() - failed. Valkyrie test\n");
					return -1;
				}

				StringPullStartOfRoute(m_pPointRouteRouteToDoor, pPed);

				if (!m_pPointRouteRouteToDoor->GetSize())	// We could stringpull the route into nothing
					return State_GoDirectlyToBoardingPoint;
				else
					return State_FollowPointRouteToBoardingPoint;

			}
			else
			{
				return State_GoDirectlyToBoardingPoint;
			}
		}
		else if (iHullDirection == 0 || bForceGoDirectlyToBoardingPoint)
		{
			return State_GoDirectlyToBoardingPoint;
		}
		else if(!bNeverUseNavMesh)
		{	// Both ways are blocked, we picked the one that take us furthest but I think it is more safe
			// to give the navmesh a go instead of going the furthest one as that will cause the ped to go back and forth if unlucky
			return State_FollowNavMeshToBoardingPoint;
		}

		AI_LOG_WITH_ARGS("[VehicleEntryExit] CTaskMoveGoToVehicleDoor::ChooseGoToDoorState() - vehicleHull.CalcShortestRouteToTarget() failed. iHullDirection - %i, bForceGoDirectlyToBoardingPoint - false, bNeverUseNavMesh - true\n", iHullDirection);
	}

	//***************************************************
	// Else follow a navmesh route to the door position

	else
	{
		return bForceGoDirectlyToBoardingPoint ? State_GoDirectlyToBoardingPoint : State_FollowNavMeshToBoardingPoint;
	}

	AI_LOG_WITH_ARGS("[VehicleEntryExit] CTaskMoveGoToVehicleDoor::ChooseGoToDoorState() - failed. bShouldNavigateToDoor - %s, bSuppressNavmesh - %s\n", AILogging::GetBooleanAsString(bShouldNavigateToDoor), AILogging::GetBooleanAsString(bSuppressNavmesh));
	return -1;
}

float CTaskMoveGoToVehicleDoor::ChooseGoToDoorMBR(CPed * pPed)
{
	TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, GROUP_FOLLOWER_WALK_TO_DOOR_DIST, 4.0f, 0.0f, 10.0f, 0.5f);
	TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, GROUP_FOLLOWER_MAINTAIN_LEADERS_MBR_DISTANCE, 10.0f, 0.0f, 50.0f, 1.0f);

	if(pPed && 
		(!pPed->GetPlayerWanted() || pPed->GetPlayerWanted()->GetWantedLevel() == WANTED_CLEAN) &&
		!pPed->IsUsingActionMode() && pPed->GetPedIntelligence()->GetBattleAwareness() <= 0.01f)
	{
		CPedGroup * pGroup = pPed->GetPedsGroup();
		if (pGroup && pGroup->GetGroupMembership())
		{
			CPed * pLeader = pGroup->GetGroupMembership()->GetLeader();
			if(pLeader && pPed != pLeader)
			{
				if((!pLeader->GetPlayerWanted() || pLeader->GetPlayerWanted()->GetWantedLevel() == WANTED_CLEAN) && 
					!pLeader->IsUsingActionMode() && pLeader->GetPedIntelligence()->GetBattleAwareness() <= 0.01f)
				{
					Vector3 vDelta = VEC3V_TO_VECTOR3( pPed->GetTransform().GetPosition() ) - m_vTargetBoardingPoint;

					CTaskMoveGoToVehicleDoor * pLeaderGoToDoorTask = (CTaskMoveGoToVehicleDoor*)pLeader->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_GO_TO_VEHICLE_DOOR);
					if(pLeaderGoToDoorTask)
					{
						m_fLeadersLastMBR = Max(pLeaderGoToDoorTask->GetMoveBlendRatio(), MOVEBLENDRATIO_WALK);

						if(vDelta.Mag2() < square(GROUP_FOLLOWER_WALK_TO_DOOR_DIST))
						{
							return MOVEBLENDRATIO_WALK;
						}

						return m_fLeadersLastMBR;
					}

					// Leader is not entering a vehicle
					// Perhaps they have already got in ?
					if( (pLeader->GetVehiclePedInside() == m_pTargetVehicle || pLeader->GetVehiclePedEntering() == m_pTargetVehicle) &&
						pPed->GetPedIntelligence()->GetCurrentEventType()==EVENT_LEADER_ENTERED_CAR_AS_DRIVER)
					{
						if(vDelta.Mag2() < square(GROUP_FOLLOWER_MAINTAIN_LEADERS_MBR_DISTANCE))
						{
							return m_fLeadersLastMBR;
						}
					}
				}
			}
		}
	}
	return m_fMoveBlendRatio;
}

void CTaskMoveGoToVehicleDoor::StringPullStartOfRoute(CPointRoute * pRoute, CPed * pPed) const
{
	const Vector3 vPedPos = VEC3V_TO_VECTOR3( pPed->GetTransform().GetPosition() );

	// Remove route sections which will be completed instantly in the first frame
	// This avoids the movement clips getting locked into the wrong directional walkstart
	const bool bIsVehicleBike = m_pTargetVehicle->GetIsTypeVehicle() && ((CVehicle*)m_pTargetVehicle.Get())->InheritsFromBike();
	const bool bIsUpright = (m_pTargetVehicle->GetTransform().GetUp().GetZf() > 0.8f);

	if(bIsVehicleBike && bIsUpright)
	{
		return;
	}

	const float fCoincidentEps = CTaskMoveFollowPointRoute::ms_fTargetRadius;
//	if(bIsVehicleBike && bIsUpright)
//		fCoincidentEps *= 0.3f;

	while(pRoute->GetSize() > 0 && (pRoute->Get(0)- vPedPos).XYMag2() < fCoincidentEps*fCoincidentEps)
	{
		pRoute->Remove(0);
	}

	// Remove initial waypoint if we can clearly see the one after
	if(pRoute->GetSize() > 1 &&
		TestIsSegmentClear( vPedPos, pRoute->Get(0), pPed->GetCapsuleInfo(), true, 0.0f, NULL, 0.1f ) &&
		 TestIsSegmentClear( vPedPos, pRoute->Get(1), pPed->GetCapsuleInfo(), true, 0.0f, NULL, 0.1f ))
	{
		pRoute->Remove(0);
	}

	// Remove initial waypoints which are not necessary because line-of-sight exists to the subsequent waypoint
	while(pRoute->GetSize() > 2 && TestIsSegmentClear( pRoute->Get(0), pRoute->Get(2), pPed->GetCapsuleInfo(), true, 0.0f, NULL, 0.1f ))
	{
		pRoute->Remove(1);
	}
}

bool CTaskMoveGoToVehicleDoor::TestIsSegmentClear(const Vector3 & vStartIn, const Vector3 & vEndIn, const CBaseCapsuleInfo * pPedCapsuleInfo, bool bIncludeTargetEntity, const float fIgnoreCollisionDistSqrXY, s32 * pExcludeComponents, const float fRadiusExtra) const
{
	if (!bIncludeTargetEntity)
	{
		return true;
	}

	const float fHeadHeight = pPedCapsuleInfo->GetMaxSolidHeight();
	const float fKneeHeight = pPedCapsuleInfo->GetMinSolidHeight();
	const float fDeltaHeight = fHeadHeight - fKneeHeight;
	const float fPedRadius = pPedCapsuleInfo->GetHalfWidth();
	const float fTestInc = ((fPedRadius*2.0f) * 0.75f);
	const int iNumTests = (int) (fDeltaHeight / fTestInc) + 1;

	Vector3 vStart = vStartIn;
	vStart.z += fKneeHeight + fPedRadius * 0.5f;
	Vector3 vEnd = vEndIn;
	vEnd.z += fKneeHeight + fPedRadius * 0.5f;

#if __DEV && DEBUG_DRAW
	TUNE_GROUP_BOOL(VEHICLE_ENTRY_DEBUG, RENDER_GO_TO_DOOR_DEBUG_CAPSULE_ALL_CHECKS, false);
#endif

	for(int i=0; i<iNumTests; i++)
	{
#if __DEV && DEBUG_DRAW
		if(RENDER_GO_TO_DOOR_DEBUG_CAPSULE_ALL_CHECKS && (CPedDebugVisualiser::GetDebugDisplay()==CPedDebugVisualiser::eTasksFullDebugging ||
			CPedDebugVisualiser::GetDebugDisplay()==CPedDebugVisualiser::eTasksNoText))
			grcDebugDraw::Line(vEnd, vStart, Color_white, Color_green);
#endif
		WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
		capsuleDesc.SetCapsule(vStart, vEnd , fPedRadius + fRadiusExtra); // NB: reversed these to allow missing initial sphere to be in vehicle collision
		capsuleDesc.SetDomainForTest(WorldProbe::TEST_IN_LEVEL);
		capsuleDesc.SetIncludeEntity(m_pTargetVehicle);
		capsuleDesc.SetIncludeFlags(ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_BOX_VEHICLE_TYPE|ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_FORKLIFT_FORKS_TYPE);
		capsuleDesc.SetDoInitialSphereCheck(false);
		capsuleDesc.SetIsDirected(false);

		bool bHit;

		if(pExcludeComponents)
		{
			WorldProbe::CShapeTestFixedResults<> results;
			capsuleDesc.SetResultsStructure(&results);

			bHit = false;
			if( WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc) )
			{
				for(WorldProbe::CShapeTestResults::Iterator iter = results.begin(); iter != results.end(); iter++)
				{
					if(iter->IsAHit())
					{
						const u16 iHitComponent = iter->GetHitComponent();
						const Vector3 & vHitPos = iter->GetHitPosition();
						bool bHitExcluded = false;

						if((vHitPos - vEndIn).XYMag2() >= fIgnoreCollisionDistSqrXY)
						{
							s32 * pExclude = pExcludeComponents;
							while(*pExclude)
							{
								// Only register a hit for a component which is not in our exclude list
								if(iHitComponent == *pExclude)
								{
									bHitExcluded = true;
									break;
								}
								pExclude++;
							}
							if(!bHitExcluded)
							{
								bHit = true;
								break;
							}
						}
					}
				}
			}
		}	
		else
		{
			bHit = WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc);
		}
		
#if __DEV && DEBUG_DRAW
		if(RENDER_GO_TO_DOOR_DEBUG_CAPSULE_ALL_CHECKS)
			CTask::ms_debugDraw.AddCapsule(RCC_VEC3V(vStart), RCC_VEC3V(vEnd), fPedRadius + fRadiusExtra, !bHit ? Color_green : Color_red, 1000, 0, false);
#endif

		if (bHit)
		{
			return false;
		}

		vStart.z += fTestInc;
		vEnd.z += fTestInc;
	}
	return true;
}

// NAME : ProcessClosingRearDoor
// PURPOSE : If we are heading to the front door of a vehicle which has rear doors, then
// we can have problems if the rear doors are open.  The entry point will be on the other
// side of the opened rear door.
// We can try closing the rear door when close to allow us to get to the target.
bool CTaskMoveGoToVehicleDoor::ProcessClosingRearDoor(CPed * pPed) const
{
	if( !CTaskEnterVehicle::CheckIsEntryPointBlockedByRearDoor( m_pTargetVehicle, m_iEntryPoint ) )
		return false;

	// From the above check we can assume this is an automobile, but let's assert to be on the safe side
	Assert(m_pTargetVehicle.Get()->GetIsTypeVehicle());
	Assert(((CVehicle*)m_pTargetVehicle.Get())->InheritsFromAutomobile());

	CVehicle * pVehicle = (CVehicle*)(m_pTargetVehicle.Get());
	Assert(m_iEntryPoint < pVehicle->GetNumberEntryExitPoints());
	const CEntryExitPoint * pTargetEP = pVehicle->GetEntryExitPoint(m_iEntryPoint);
	const eHierarchyId iTargetID = CCarEnterExit::DoorBoneIndexToHierarchyID(pVehicle, pTargetEP->GetDoorBoneIndex());
	Assert(iTargetID == VEH_DOOR_DSIDE_F || iTargetID == VEH_DOOR_PSIDE_F);
	//CCarDoor * pFrontDoor = pVehicle->GetDoorFromId(iTargetID);
	const eHierarchyId iRearDoorID = (eHierarchyId) (((int)iTargetID) + 1);
	Assert(iRearDoorID == VEH_DOOR_DSIDE_R || iRearDoorID == VEH_DOOR_PSIDE_R);
	CCarDoor * pRearDoor = pVehicle->GetDoorFromId(iRearDoorID);

	//----------------------------------------------------------------------------------------
	// Are we close to this rear door?
	// Test a hemisphere whose origin is at the rear door's center, and with the half-sphere
	// in front of the rear door

	//-----------------------------------------------------------------------------------
	// Calculate a position in the center of the door, by examining the door's size
	// and then offsetting the door position (which is at the hinge) along it's Y axis
	// by half that size.

	Matrix34 mat;
	if( !pRearDoor->GetLocalMatrix(pVehicle, mat) )
		return false;
	mat.Dot(MAT34V_TO_MATRIX34(pVehicle->GetTransform().GetMatrix()));
	Vector3 vDoorPos = mat.d;

	Vector3 vLocalMin, vLocalMax;
	if( !pRearDoor->GetLocalBoundingBox(pVehicle, vLocalMin, vLocalMax))
		return false;
	float fDoorWidth = vLocalMax.y - vLocalMin.y;
	vDoorPos -= mat.b * (fDoorWidth * 0.5f);

	//---------------------------------------------
	// Now test a sphere at the center of the door

	static dev_float sfTestSizeExtra = 0.125f;
	static dev_float sfBehindPlaneDist = -0.125f; //0.0f;

	const float fSphereRadius = fDoorWidth + sfTestSizeExtra;

#if DEBUG_DRAW
	//grcDebugDraw::Axis(mat, 1.0f, true);
	//grcDebugDraw::Sphere(vDoorPos, fSphereRadius, Color_yellow, false);
#endif

	const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	if(vPedPos.Dist2(vDoorPos) > fSphereRadius)
		return false;

	//------------------------------------------------------------------------------------------------------
	// Are we IN FRONT OF a plane which is defined by the position of the door matrix, and aligned with
	// either the positive or negative X axis of that matrix, dependent upon which side of the car it is ?

	Vector3 vPlaneABC = (iRearDoorID==VEH_DOOR_PSIDE_R) ? mat.a : mat.a * -1.0f;
	float fPlaneD = - DotProduct(vPlaneABC, mat.d);
	if( DotProduct(vPedPos, vPlaneABC) + fPlaneD < sfBehindPlaneDist )
		return false;

	//-----------
	// Close it!

	static dev_s32 iFlags = CCarDoor::DRIVEN_NORESET;
	pRearDoor->SetTargetDoorOpenRatio(0.0f, iFlags);

#if DEBUG_DRAW
	//grcDebugDraw::Sphere(vDoorPos, fSphereRadius, Color_green, false);
#endif

	return true;
}

bool CTaskMoveGoToVehicleDoor::IsExcludingAllMapTypes(const CPed *pPed) const
{
	if(m_pTargetVehicle->GetIsTypeVehicle())
	{
		CVehicle *pVehicle = static_cast<CVehicle*>(m_pTargetVehicle.Get());		
		if(pVehicle && pVehicle->GetIsJetSki() && pVehicle->GetIsInWater() && !pPed->GetIsSwimming())
		{
			return true;
		}
	}

	return false;
}

bool CTaskMoveGoToVehicleDoor::CanGoDirectlyToBoardingPoint(CPed * pPed) const
{
	if(IsAlreadyInPositionToEnter(pPed))
		return true;

	// Never allow us to go directly to the boarding point of an upright bike,
	// if we are approaching from the front/rear; there are physics issues which
	// prevent us from being able to push around the wheels (which are spheres not discs)
	if(m_pTargetVehicle->GetIsTypeVehicle())
	{
		CVehicle * pVehicle = static_cast<CVehicle*>(m_pTargetVehicle.Get());
		if(pVehicle->InheritsFromBike() || pVehicle->InheritsFromBicycle())
		{
			const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			CEntityBoundAI boundAI(*m_pTargetVehicle.Get(), vPedPos.z, pPed->GetCapsuleInfo()->GetHalfWidth());
			int iSide = boundAI.ComputeHitSideByPosition(vPedPos);
			if(iSide == CEntityBoundAI::FRONT || iSide == CEntityBoundAI::REAR)
			{
				return false;
			}
		}
	}

	bool bIncludeTargetEntity = true;

	// If we're stood on the sub ignore the sub as part of the capsule test
	if(pPed->GetGroundPhysical() && pPed->GetGroundPhysical()->GetIsTypeVehicle())
	{
		if (m_pTargetVehicle->GetIsTypeVehicle() && static_cast<const CVehicle*>(m_pTargetVehicle.Get())->InheritsFromSubmarine() && m_pTargetVehicle == pPed->GetGroundPhysical())
		{
			bIncludeTargetEntity = false;
		}
	}


	//***********************************************************************************************************
	// Create a list of the component parts of the door we are headed towards; pass these in as the exclude list

	int iDoorComponents[MAX_DOOR_COMPONENTS+1];
	s32 iNumDoorComponents = 0;

	if(m_pTargetVehicle->GetIsTypeVehicle())
	{
		const CVehicle * pVehicle = static_cast<const CVehicle*>(m_pTargetVehicle.Get());

		Assert(pVehicle->GetVehicleModelInfo() && pVehicle->GetVehicleModelInfo()->GetModelSeatInfo());

		const CEntryExitPoint * pEntryPoint = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryExitPoint(m_iEntryPoint);
		const eHierarchyId iDoorHierarchyId = CCarEnterExit::DoorBoneIndexToHierarchyID(pVehicle, pEntryPoint->GetDoorBoneIndex());
		if(iDoorHierarchyId != VEH_INVALID_ID)
			iNumDoorComponents = CCarEnterExit::GetDoorComponents(pVehicle, iDoorHierarchyId, iDoorComponents);
	}

	iDoorComponents[iNumDoorComponents++] = 0;

	//**********************************************************************************************************
	// If there is a route directly to the door without intersecting the vehicle itself, then go directly there
	// We test 4 capsules, each as wide as the peds radius and spaced in Z by the peds radius either side of the ped's Z origin.

	const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	return TestIsSegmentClear(vPedPos, m_vTargetBoardingPoint, pPed->GetCapsuleInfo(), bIncludeTargetEntity, 0.25f*0.25f, iDoorComponents);

}

bool CTaskMoveGoToVehicleDoor::IsAlreadyInPositionToEnter(CPed * pPed) const
{
	// Only if this vehicle has no boarding points
	if((m_pTargetVehicle->GetIsTypeVehicle() &&
		( ((CVehicle*)m_pTargetVehicle.Get())->GetBoardingPoints()!=NULL && ((CVehicle*)m_pTargetVehicle.Get())->GetBoardingPoints()->GetNumBoardingPoints() > 0) ) )
	{
		return false;
	}

	const CModelSeatInfo* pModelSeatinfo = NULL;
	if (m_pTargetVehicle->GetIsTypePed()) 
	{	
		pModelSeatinfo  = static_cast<const CPed*>(m_pTargetVehicle.Get())->GetPedModelInfo()->GetModelSeatInfo();
	} 
	else 
	{		
		pModelSeatinfo  = static_cast<const CVehicle*>(m_pTargetVehicle.Get())->GetVehicleModelInfo()->GetModelSeatInfo();		
	} 	

	Vector3 vDoorPos;
	const CEntryExitPoint * pEntryPoint = pModelSeatinfo->GetEntryExitPoint(m_iEntryPoint);
	pEntryPoint->GetEntryPointPosition(m_pTargetVehicle, pPed, vDoorPos, false);

	const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	const Vector3 vDiff = vDoorPos - vPedPos;

	static dev_float fDistXYLong = 1.0f;
	static dev_float fDistXYShort = 0.25f;
	float fDistXY = (pPed->GetPedResetFlag(CPED_RESET_FLAG_UseTighterEnterVehicleSettings) ? fDistXYShort : fDistXYLong);

	if(Abs(vDiff.z) < 2.0f && vDiff.XYMag2() < fDistXY*fDistXY)
	{
		return true;
	}
	return false;
}
