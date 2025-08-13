// FILE :    TaskCombat.cpp
// PURPOSE : Combat tasks to replace killped* tasks, greater dependancy on decision makers
//			 So that designers can tweak combat behaviour and proficiency based on character type
// AUTHOR :  Phil H
// CREATED : 18-08-2005

// C++ headers
#include <limits>

// Rage headers
#include "grcore/debugdraw.h"

// Framework headers
#include "fwanimation/animmanager.h"
#include "fwmaths/geomutil.h"
#include "fwmaths/vectorutil.h"

// Game headers
#include "audio/policescanner.h"
#include "audio/northaudioengine.h"
#include "audio/superconductor.h"
#include "camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/gameplay/ThirdPersonCamera.h"
#include "Debug/VectorMap.h"
#include "Event/EventDamage.h"
#include "Event/EventSource.h"
#include "Event/EventWeapon.h"
#include "Event/ShockingEvents.h"
#include "Network/NetworkInterface.h"
#include "PedGroup/PedGroup.h"
#include "PedGroup/Formation.h"
#include "Peds/Ped.h"
#include "Peds/PedDebugVisualiser.h"
#include "Peds/PedGeometryAnalyser.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PedMoveBlend/PedMoveBlendManager.h"
#include "Peds/PedWeapons/PedTargeting.h"
#include "physics/WorldProbe/worldprobe.h"
#include "Physics/GtaInst.h"
#include "Physics/GtaMaterialManager.h"
#include "physics/iterator.h"
#include "Physics/physics.h"
#include "Scene/Entity.h"
#include "scene/EntityIterator.h"
#include "Script/Script.h"
#include "Task/Combat/Cover/Cover.h"
#include "Task/Combat/Cover/coverfilter.h"
#include "Task/Combat/Subtasks/TaskShootAtTarget.h"
#include "Task/Default/TaskAmbient.h"
#include "Task/Vehicle/TaskCar.h"
#include "Task/Vehicle/TaskCarUtils.h"
#include "Task/Vehicle/TaskCarAccessories.h"
#include "Task/Combat/TaskCombat.h"
#include "Task/Combat/TaskCombatMelee.h"
#include "Task/Combat/TaskInvestigate.h"
#include "Task/Combat/TaskNewCombat.h"
#include "Task/Combat/TaskThreatResponse.h"
#include "Task/Combat/Cover/TaskCover.h"
#include "Task/Combat/Subtasks/TaskVehicleCombat.h"
#include "Task/Response/TaskFlee.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Task/Vehicle/TaskExitVehicle.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/Combat/Cover/TaskSeekCover.h"
#include "Task/Motion/Locomotion/TaskMotionPed.h"
#include "Task/Movement/TaskMoveFollowLeader.h"
#include "Task/Movement/TaskMoveWander.h"
#include "Task/Movement/TaskMoveWithinAttackWindow.h"
#include "Task/Movement/TaskSeekEntity.h"
#include "Task/Weapons/Gun/TaskGun.h"
#include "Task/Weapons/WeaponController.h"
#include "Task/Weapons/TaskProjectile.h"
#include "Task/Weapons/TaskSwapWeapon.h"
#include "Task/Weapons/TaskWeaponBlocked.h"
#include "Task/Vehicle/TaskVehicleWeapon.h"
#include "Vehicles/Heli.h"
#include "Vfx/Misc/Fire.h"
#include "vehicleAi/task/TaskVehicleGoto.h"
#include "vehicleAi/task/TaskVehicleGotoHelicopter.h"
#include "vehicleAi/VehicleIntelligence.h"
#include "Weapons/inventory/PedWeaponManager.h"

// rage
#include "crskeleton/skeleton.h"
#include "physics/shapetest.h"

AI_OPTIMISATIONS()
AI_COVER_OPTIMISATIONS()

//////////////////////////////////////
//		TASK COMBAT SEEK COVER      //
//////////////////////////////////////

bank_float CTaskCombatSeekCover::ms_fDistToFireImmediately = 5.0f;
bank_s32 CTaskCombatSeekCover::ms_iMaxSeparateFrequency = 6000;

// Constructor
CTaskCombatSeekCover::CTaskCombatSeekCover( const CPed* pPrimaryTarget, const Vector3& vSearchStartPoint, const bool bConsiderBackwardsPoints, bool bAbortIfDistanceToCoverTooFar )
: m_pPrimaryTarget(pPrimaryTarget)
, m_bConsiderBackwardPoints(bConsiderBackwardsPoints)
, m_bAbortIfDistanceTooFar(bAbortIfDistanceToCoverTooFar)
, m_bPlayerInitiallyClose(false)
, m_iNextTimeMaySeparate(fwTimer::GetTimeInMilliseconds() + fwRandom::GetRandomNumberInRange(0,ms_iMaxSeparateFrequency) )
, m_vCoverSearchStartPos(vSearchStartPoint)
, m_vSeparationCentre(Vector3::ZeroType)
{
	SetInternalTaskType(CTaskTypes::TASK_COMBAT_SEEK_COVER);
}

// Destructor
CTaskCombatSeekCover::~CTaskCombatSeekCover()
{
}

// State machine function run before the main UpdateFSM
CTask::FSM_Return CTaskCombatSeekCover::ProcessPreFSM()
{
	// Abort if the target is invalid
	if (!m_pPrimaryTarget)
	{
		return FSM_Quit;
	}

	return FSM_Continue;
}

// Main state machine update function
CTask::FSM_Return CTaskCombatSeekCover::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();	

		FSM_State(State_SeekingCover)
			FSM_OnEnter
				SeekingCover_OnEnter();
			FSM_OnUpdate
				return SeekingCover_OnUpdate();

		FSM_State(State_Separating)
			FSM_OnEnter
				Separating_OnEnter();
			FSM_OnUpdate
				return Separating_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

// State functions
CTask::FSM_Return CTaskCombatSeekCover::Start_OnUpdate()
{
	m_bPlayerInitiallyClose = m_pPrimaryTarget && DistSquared(m_pPrimaryTarget->GetTransform().GetPosition(), GetPed()->GetTransform().GetPosition()).Getf() <  ms_fDistToFireImmediately;

	SetState(State_SeekingCover);
	return FSM_Continue;
}

void CTaskCombatSeekCover::SeekingCover_OnEnter()
{
	CPed* pPed = GetPed();
	weaponAssert(pPed->GetWeaponManager());

	CPedTargetting* pPedTargetting = pPed->GetPedIntelligence()->GetTargetting(true);

	LosStatus losStatus = pPedTargetting->GetLosStatus(static_cast<const CEntity*>(m_pPrimaryTarget));

	pPed->SetIsCrouching(false);

	s32 iFlags = CTaskCover::CF_QuitAfterCoverEntry|CTaskCover::CF_RunSubtaskWhenMoving|CTaskCover::CF_RunSubtaskWhenStationary|CTaskCover::CF_CoverSearchByPos|CTaskCover::CF_SeekCover;

	if (losStatus != Los_blocked)
		iFlags |= CTaskCover::CF_RequiresLosToTarget;

	if (!m_bConsiderBackwardPoints)
		iFlags |= CTaskCover::CF_DontConsiderBackwardsPoints;

	float fDesiredMBR = MOVEBLENDRATIO_RUN;
	bool bShouldStrafe = true;
	CTaskCombat::ChooseMovementAttributes(pPed, bShouldStrafe, fDesiredMBR);

	s32 iCombatRunningFlags = CTaskCombat::GenerateCombatRunningFlags(bShouldStrafe, fDesiredMBR);

	// The second argument specifies whether a LOS is needed to the target from any cover point
	CTaskCover* pCoverTask = rage_new CTaskCover(CAITarget(m_pPrimaryTarget));
	pCoverTask->SetSearchFlags(iFlags);
	pCoverTask->SetSearchPosition(m_vCoverSearchStartPos);
	pCoverTask->SetMinMaxSearchDistances(0.0f, 50.0f);
	pCoverTask->SetMoveBlendRatio(fDesiredMBR);
	pCoverTask->SetSubtaskToUseWhilstSearching(	rage_new CTaskCombatAdditionalTask(iCombatRunningFlags, m_pPrimaryTarget, VEC3V_TO_VECTOR3(m_pPrimaryTarget->GetTransform().GetPosition()) ) );
	pCoverTask->SetCoverPointFilterConstructFn(CCoverPointFilterTaskCombat::Construct);

	SetNewTask(pCoverTask);
}

CTask::FSM_Return CTaskCombatSeekCover::SeekingCover_OnUpdate()
{
	CPed* pPed = GetPed();

	if (CheckForFinish())
	{
		return FSM_Quit;
	}

	if (GetIsSubtaskFinished(CTaskTypes::TASK_COVER))
	{
		// If the cover search failed, try again or separate the ped from nearby peds
		if (GetSubTask()->GetTaskType() == CTaskTypes::TASK_COVER && !pPed->GetCoverPoint())
		{
			// If we have a defensive area and we're not in it, quit out and let the parent combat task move within the defensive area
			if (pPed->GetPedIntelligence()->GetDefensiveArea()->IsActive())
			{
				if (!pPed->GetPedIntelligence()->GetDefensiveArea()->IsPointInDefensiveArea(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition())))
				{
					return FSM_Quit;
				}
			}

			// If previously tried to find perfect cover and failed, try to find nearly decent cover
			if ((static_cast<CTaskCover*>(GetSubTask()))->GetCoverSearchType() == CCover::CS_MUST_PROVIDE_COVER )
			{
				SetFlag(aiTaskFlags::RestartCurrentState);
				return FSM_Continue;
			}

			// If the task has failed to find cover but the ped is bunched up try to shift the ped away from nearby peds
			if (fwTimer::GetTimeInMilliseconds() > m_iNextTimeMaySeparate && CTaskSeparate::NeedsToSeparate(pPed, m_pPrimaryTarget, VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), CTaskSeparate::MIN_SEPARATION_DISTANCE, &m_vSeparationCentre))
			{
				m_iNextTimeMaySeparate = fwTimer::GetTimeInMilliseconds()+ms_iMaxSeparateFrequency;

				SetState(State_Separating);
				return FSM_Continue;
			}
		}

		return FSM_Quit;
	}
	return FSM_Continue;
}

void CTaskCombatSeekCover::Separating_OnEnter()
{
	CTask* pAdditionalSubtask = rage_new CTaskCombatAdditionalTask( CTaskCombatAdditionalTask::RT_Default, m_pPrimaryTarget, VEC3V_TO_VECTOR3(m_pPrimaryTarget->GetTransform().GetPosition()));
	SetNewTask(rage_new CTaskSeparate(&m_vSeparationCentre, VEC3V_TO_VECTOR3(m_pPrimaryTarget->GetTransform().GetPosition()), m_pPrimaryTarget, pAdditionalSubtask, true ));
}

CTask::FSM_Return CTaskCombatSeekCover::Separating_OnUpdate()
{
	if (CheckForFinish())
	{
		return FSM_Quit;
	}

	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_SeekingCover);
	}
	return FSM_Continue;
}

bool CTaskCombatSeekCover::CheckForFinish()
{
	CPed* pPed = GetPed();
	weaponAssert(pPed->GetWeaponManager());

	if (m_pPrimaryTarget && !m_bPlayerInitiallyClose 
		&& DistSquared(m_pPrimaryTarget->GetTransform().GetPosition(), pPed->GetTransform().GetPosition()).Getf() <  ms_fDistToFireImmediately)
	{
		if (pPed->GetWeaponManager()->GetIsArmed())
		{
			pPed->ReleaseCoverPoint();
		}
		return true;
	}

	// If supposed to abort if the cover point is too far, check the current route distance
	if (m_bAbortIfDistanceTooFar && GetSubTask()->GetTaskType() == CTaskTypes::TASK_COVER)
	{
		CTaskMoveFollowNavMesh* pNavmeshTask = static_cast<CTaskMoveFollowNavMesh*>(pPed->GetPedIntelligence()->FindTaskActiveMovementByType(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH));
		// Find any follow navmesh route task and use it to work out if we've overshot
		if (pNavmeshTask)
		{
			if (pNavmeshTask->IsFollowingNavMeshRoute())
			{
				if (pNavmeshTask->GetTotalRouteLength() > ( Dist(RCC_VEC3V(m_vCoverSearchStartPos), pPed->GetTransform().GetPosition()).Getf() * 2.0f ))
				{
					if (pPed->GetWeaponManager()->GetIsArmed())
					{
						pPed->ReleaseCoverPoint();
					}
					return true;
				}
				else
				{
					// The task now knows that the route is reasonable so carry on.
					m_bAbortIfDistanceTooFar = false;
				}
			}
		}
	}

	return false;
}

#if !__FINAL
const char * CTaskCombatSeekCover::GetStaticStateName( s32 iState )
{
	taskAssert(iState >= State_Start && iState <= State_Finish);

	switch (iState)
	{
		case State_Start:				return "State_Start";
		case State_SeekingCover:		return "State_SeekingCover";
		case State_Separating:			return "State_Separating";
		case State_Finish:				return "State_Finish";
		default: taskAssert(0); 	
	}

	return "State_Invalid";
}
#endif // !__FINAL

//////////////////////////////////////
//		TASK COMBAT CHARGE		    //
//////////////////////////////////////

// Constructor
CTaskCombatChargeSubtask::CTaskCombatChargeSubtask(CPed* pPrimaryTarget)
: m_pPrimaryTarget(pPrimaryTarget)
{
	SetInternalTaskType(CTaskTypes::TASK_COMBAT_CHARGE);
}

// Destructor
CTaskCombatChargeSubtask::~CTaskCombatChargeSubtask()
{
}

// State machine function run before the main UpdateFSM
CTask::FSM_Return CTaskCombatChargeSubtask::ProcessPreFSM()
{
	// Abort if the target is invalid
	if (!m_pPrimaryTarget)
	{
		return FSM_Quit;
	}

	return FSM_Continue;
}

// Main state machine update function
CTask::FSM_Return CTaskCombatChargeSubtask::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
			return Start_OnUpdate();	

		FSM_State(State_Melee)
			FSM_OnEnter
				Melee_OnEnter();
			FSM_OnUpdate
				return Melee_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskCombatChargeSubtask::Start_OnUpdate()
{
	SetState(State_Melee);
	return FSM_Continue;
}

void CTaskCombatChargeSubtask::Melee_OnEnter()
{
	CPed* pPed = GetPed();

	if (pPed->GetCurrentMotionTask()->IsOnFoot())
	{
		SetNewTask(rage_new CTaskMelee(NULL, m_pPrimaryTarget, CTaskMelee::MF_ShouldBeInMeleeMode | CTaskMelee::MF_HasLockOnTarget));
	}
}

CTask::FSM_Return CTaskCombatChargeSubtask::Melee_OnUpdate()
{
	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Finish);
	}
	return FSM_Continue;
}

// Changes the tasks target
void CTaskCombatChargeSubtask::UpdateTarget(CPed* pNewTarget)
{
	// See if we have an action controller and can just switch its target.
	if (GetSubTask() && (GetSubTask()->GetTaskType() == CTaskTypes::TASK_MELEE))
	{
		CTaskMelee* pMelee = static_cast<CTaskMelee*>(GetSubTask());
		if (pMelee)
		{
			if (pNewTarget != pMelee->GetTargetEntity())
			{
				pMelee->SetTargetEntity(pNewTarget, true);
			}
		}
	}
}

#if !__FINAL
const char * CTaskCombatChargeSubtask::GetStaticStateName( s32 iState )
{
	taskAssert(iState >= State_Start && iState <= State_Finish);

	switch (iState)
	{
		case State_Start:				return "State_Start";
		case State_Melee:				return "State_Melee";
		case State_Finish:				return "State_Finish";
		default: taskAssert(0); 	
	}

	return "State_Invalid";
}
#endif // !__FINAL

//////////////////////////////////////
//		TASK SET AND GUARD AREA	    //
//////////////////////////////////////
CClonedSetAndGuardAreaInfo::CClonedSetAndGuardAreaInfo(const Vector3& vDefendPosition, float fHeading, float fProximity, eGuardMode guardMode, u32 endTime, const CDefensiveArea& area )
: m_vDefendPosition(vDefendPosition)
, m_vAreaPosition(VEC3_ZERO)
, m_areaRadius(0.f)
, m_heading(0)
, m_proximity((u8)fProximity)
, m_guardMode((u8)guardMode)
, m_endTime(endTime)
{
	float fAreaRadius;
	area.GetCentre(m_vAreaPosition);
	area.GetMaxRadius(fAreaRadius);

	Assertf(((u32)fAreaRadius) < (1<<SIZEOF_AREA_RADIUS), "CClonedSetAndGuardAreaInfo: Area radius %f exceeds synced range", fAreaRadius);
	Assertf(((u32)fProximity) < (1<<SIZEOF_PROXIMITY), "CClonedSetAndGuardAreaInfo: Proximity %f exceeds synced range", fProximity);
	Assertf((u32)guardMode < (1<<SIZEOF_GUARD_MODE), "CClonedSetAndGuardAreaInfo: Guard mode %d exceeds synced range", guardMode);

	m_areaRadius = fAreaRadius;

	if (fHeading > TWO_PI)
		fHeading -= TWO_PI;
	else if (fHeading < 0.0f)
		fHeading += TWO_PI;

	float maxHeading = (float)((1<<SIZEOF_HEADING)-1);
	m_heading = (u8)((fHeading / TWO_PI) * maxHeading);
}

CClonedSetAndGuardAreaInfo::CClonedSetAndGuardAreaInfo()
: m_vDefendPosition(VEC3_ZERO)
, m_vAreaPosition(VEC3_ZERO)
, m_areaRadius(0.f)
, m_heading(0)
, m_proximity(0)
, m_guardMode(0)
, m_endTime(0)
{
}

CTask *CClonedSetAndGuardAreaInfo::CreateLocalTask(fwEntity* UNUSED_PARAM(pEntity))
{
	CDefensiveArea area;
	if(m_areaRadius < CDefensiveArea::GetMinRadius())
		m_areaRadius = CDefensiveArea::GetMinRadius();
	area.SetAsSphere(m_vDefendPosition, m_areaRadius);

	u32 maxHeading = ((1<<SIZEOF_HEADING)-1);
	float heading = (float) m_heading / (float) maxHeading;

	CTaskSetAndGuardArea* pSetAndGuardAreaTask = NULL;

	if (m_endTime != 0)
	{
		int timeDiff = m_endTime - NetworkInterface::GetSyncedTimeInMilliseconds();

		if (timeDiff > 0)
		{
			pSetAndGuardAreaTask = rage_new CTaskSetAndGuardArea( m_vDefendPosition, heading, (float)m_proximity, (eGuardMode)m_guardMode, (float)timeDiff / 1000.0f, area );
		}
	}
	else
	{
		pSetAndGuardAreaTask = rage_new CTaskSetAndGuardArea( m_vDefendPosition, heading, (float)m_proximity, (eGuardMode)m_guardMode, 0.0f, area );
	}
	
	return pSetAndGuardAreaTask;
}

CTaskSetAndGuardArea::CTaskSetAndGuardArea( const Vector3& vDefendPosition, 
														 float fHeading, 
														 float fProximity,
														 eGuardMode guardMode, 
														 float fTimer, 
														 const CDefensiveArea& area, 
														 bool bTakePositionFromPed, 
														 bool bSetDefensiveArea )
: m_vDefendPosition(vDefendPosition)
, m_fHeading(fHeading)
, m_fProximity(fProximity)
, m_eGuardMode(guardMode)
, m_fTimer(fTimer)
, m_nEndTime(0)
, m_defensiveArea(area)
, m_bTakePosFromPed(bTakePositionFromPed)
, m_bSetDefensiveArea(bSetDefensiveArea)
{
	Assertf(!m_vDefendPosition.IsClose(VEC3_ZERO, SMALL_FLOAT), "Ped has been tasked to defend the origin with CTaskSetAndGuardArea.  This is is very likely an error.");
	SetInternalTaskType(CTaskTypes::TASK_SET_AND_GUARD_AREA);

	// used for networking only:
	if (fTimer > 0.0f)
	{
		m_nEndTime = NetworkInterface::GetSyncedTimeInMilliseconds() + (u32)(fTimer*1000.0f);
	}
}

CTaskSetAndGuardArea::~CTaskSetAndGuardArea()
{
	m_defensiveArea.Reset();
}

aiTask* CTaskSetAndGuardArea::Copy() const
{
	return rage_new CTaskSetAndGuardArea( m_vDefendPosition, m_fHeading, m_fProximity, m_eGuardMode, m_fTimer, m_defensiveArea, m_bTakePosFromPed, m_bSetDefensiveArea);
}

// Main state machine update function
CTask::FSM_Return CTaskSetAndGuardArea::UpdateFSM( s32 iState, FSM_Event iEvent )
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();	

		FSM_State(State_GuardingArea)
			FSM_OnEnter
				GuardingArea_OnEnter();
			FSM_OnUpdate
				return GuardingArea_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTaskInfo *CTaskSetAndGuardArea::CreateQueriableState() const
{
	return rage_new CClonedSetAndGuardAreaInfo(m_vDefendPosition, m_fHeading, m_fProximity, m_eGuardMode, m_nEndTime, m_defensiveArea );
}

CTask::FSM_Return CTaskSetAndGuardArea::Start_OnUpdate()
{
	SetState(State_GuardingArea);
	return FSM_Continue;
}

void CTaskSetAndGuardArea::GuardingArea_OnEnter()
{
	CPed* pPed = GetPed();

	Vector3 v1, v2;
	float fWidth;

	// Defend the area around this ped
	if (m_bTakePosFromPed)
	{
		m_vDefendPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		m_fHeading = pPed->GetTransform().GetHeading();
		if (m_defensiveArea.Get( v1, v2, fWidth ))
		{
			taskAssert(m_defensiveArea.GetType() == CDefensiveArea::AT_Sphere);
			m_defensiveArea.SetAsSphere(m_vDefendPosition, fWidth);
		}
		m_bTakePosFromPed = false;
	}

	if (m_bSetDefensiveArea)
	{
		if (m_defensiveArea.Get(v1, v2, fWidth))
		{
			if (m_defensiveArea.GetType() == CDefensiveArea::AT_Sphere)
			{
				pPed->GetPedIntelligence()->GetDefensiveArea()->SetAsSphere(v1, fWidth);
			}
			else
			{
				pPed->GetPedIntelligence()->GetDefensiveArea()->Set(v1, v2, fWidth);
			}
		}
		else
		{
			taskAssert(0);
		}
	}

	SetNewTask(rage_new CTaskStandGuard(m_vDefendPosition, m_fHeading, m_fProximity, m_eGuardMode, m_fTimer));
}

CTask::FSM_Return CTaskSetAndGuardArea::GuardingArea_OnUpdate()
{
	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Finish);
	}
	return FSM_Continue;
}

#if !__FINAL
const char * CTaskSetAndGuardArea::GetStaticStateName( s32 iState )
{
	taskAssert(iState >= State_Start && iState <= State_Finish);

	switch (iState)
	{
		case State_Start:				return "State_Start";
		case State_GuardingArea:		return "State_GuardingArea";
		case State_Finish:				return "State_Finish";
		default: taskAssert(0); 	
	}

	return "State_Invalid";
}
#endif // !__FINAL

//////////////////////////////////////
//		TASK STAND GUARD		    //
//////////////////////////////////////

IMPLEMENT_COMBAT_TASK_TUNABLES(CTaskStandGuard, 0x51008bf1)
CTaskStandGuard::Tunables CTaskStandGuard::sm_Tunables;

CClonedStandGuardInfo::CClonedStandGuardInfo(const Vector3& position, float heading, float proximity, eGuardMode guardMode, u32 endTime)
: m_position(position)
, m_heading(0)
, m_proximity((u8)proximity)
, m_guardMode((u8)guardMode)
, m_endTime(endTime)
{
	Assertf((u32)m_proximity < (1<<SIZEOF_PROXIMITY), "CClonedStandGuardInfo: Proximity %f exceeds synced range", (double)m_proximity);
	Assertf(m_guardMode < (1<<SIZEOF_GUARD_MODE), "CClonedStandGuardInfo: Guard mode %d exceeds synced range", m_guardMode);

	if (heading > TWO_PI)
		heading -= TWO_PI;
	else if (heading < 0.0f)
		heading += TWO_PI;

	float maxHeading = (float)((1<<SIZEOF_HEADING)-1);
	m_heading = (u8)((heading / TWO_PI) * maxHeading);
}

CClonedStandGuardInfo::CClonedStandGuardInfo()
: m_position(VEC3_ZERO)
, m_heading(0)
, m_proximity(0)
, m_guardMode(0)
, m_endTime(0)
{
}

CTask *CClonedStandGuardInfo::CreateLocalTask(fwEntity* UNUSED_PARAM(pEntity))
{
	CTaskStandGuard* pStandGuardTask = NULL;

	u32 maxHeading = ((1<<SIZEOF_HEADING)-1);
	float heading = (float) m_heading / (float) maxHeading;

	if (m_endTime != 0)
	{
		int timeDiff = m_endTime - NetworkInterface::GetSyncedTimeInMilliseconds();

		if (timeDiff > 0)
		{
			pStandGuardTask = rage_new CTaskStandGuard( m_position, heading, (float)m_proximity, (eGuardMode)m_guardMode, (float)timeDiff / 1000.0f );
		}
	}
	else
	{
		pStandGuardTask = rage_new CTaskStandGuard( m_position, heading, (float)m_proximity, (eGuardMode)m_guardMode, 0.0f );
	}

	return pStandGuardTask;
}

CTaskStandGuard::CTaskStandGuard( const Vector3& v1, float fHeading, float fProximity, eGuardMode guardMode, float fTimer)
: m_vThreatDir(0.0f, 0.0f, 0.0f ),
m_vDefendPosition(0.0f, 0.0f, 0.0f ),
m_vStandPoint(0.0f, 0.0f, 0.0f),
m_fHeading(0.0f),
m_fProximity(0.0f),
m_fTaskTimer(fTimer),
m_nEndTime(0),
m_eGuardMode(guardMode)
{
	m_vDefendPosition	= v1;
	m_fHeading			= fHeading;
	m_fProximity		= fProximity;

	m_fStandHeading					= 0.0f;
	m_vStandPoint					= m_vDefendPosition;
	m_fReturnToDefendPointChance	= 0.30f;
	m_nStandingPointTime			= 0;

	m_fMoveBlendRatio				= MOVEBLENDRATIO_WALK;
	m_bIsHeadingToDefendPoint		= true;

	m_nMoveAttempts					= 0;

	m_fMinProximity					= 1.5f;

	SetInternalTaskType(CTaskTypes::TASK_STAND_GUARD);

	// used for networking only:
	if (fTimer > 0.0f)
	{
		m_nEndTime = NetworkInterface::GetSyncedTimeInMilliseconds() + (u32)(fTimer*1000.0f);
	}
}

CTaskStandGuard::~CTaskStandGuard()
{
}

aiTask* CTaskStandGuard::Copy() const
{
	Vector3		v1				= m_vDefendPosition;
	float		fHeading		= m_fHeading;
	eGuardMode	guardMode		= m_eGuardMode;
	float		fProximity		= m_fProximity;
	float		fTaskTimer		= m_fTaskTimer;

	return rage_new CTaskStandGuard( v1, fHeading, fProximity, guardMode, fTaskTimer );
}

// Cleanup function called when task quits or is aborted
void CTaskStandGuard::CleanUp()
{
	CPed* pPed = GetPed();

	// Ensure no cover points are left dangling
	if (pPed->GetCoverPoint())
	{
		pPed->ReleaseCoverPoint();
	}

	pPed->SetPedResetFlag( CPED_RESET_FLAG_SearchingForCover, false );
}

// State machine function run before the main UpdateFSM
CTask::FSM_Return CTaskStandGuard::ProcessPreFSM()
{
	CPed* pPed = GetPed();

	// If a timer has been set, count down
	if (m_fTaskTimer > 0.0f)
	{
		m_fTaskTimer -= GetTimeStep();
		if (m_fTaskTimer <= 0.0f)
		{
			// Task needs to quit because timer is up
			return FSM_Quit;
		}
	}

	pPed->SetPedResetFlag( CPED_RESET_FLAG_SearchingForCover, true );

	return FSM_Continue;
}

CTaskInfo *CTaskStandGuard::CreateQueriableState() const
{
	return rage_new CClonedStandGuardInfo(m_vDefendPosition, m_fHeading, m_fProximity, m_eGuardMode, m_nEndTime);
}

// Main state machine update function
CTask::FSM_Return CTaskStandGuard::UpdateFSM( s32 iState, FSM_Event iEvent )
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();	

		FSM_State(State_GoToDefendPosition)
			FSM_OnEnter
				GoToDefendPosition_OnEnter();
			FSM_OnUpdate
				return GoToDefendPosition_OnUpdate();

		FSM_State(State_GoToStandPosition)
			FSM_OnEnter
				GoToStandPosition_OnEnter();
			FSM_OnUpdate
				return GoToStandPosition_OnUpdate();

		FSM_State(State_MoveAchieveHeading)
			FSM_OnEnter
				MoveAchieveHeading_OnEnter();
			FSM_OnUpdate
				return MoveAchieveHeading_OnUpdate();

		FSM_State(State_MoveStandStill)
			FSM_OnEnter
				MoveStandStill_OnEnter();
			FSM_OnUpdate
				return MoveStandStill_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskStandGuard::Start_OnUpdate()
{
	CPed* pPed = GetPed();

	//get starting crouch state, so we can return to this after any combat
	m_bIsCrouching = pPed->GetIsCrouching();

	//move style always starts as run
	m_fMoveBlendRatio = MOVEBLENDRATIO_RUN;

	//set returning to Defend Point, for future reference
	m_bIsHeadingToDefendPoint = true;

	//safety check - any closer and the clip may not be able to land at the point chosen
	// If its closer than the minimum distance, disable patrolling as the area is too close
	if(m_fProximity < m_fMinProximity)
	{
		m_eGuardMode = GM_Stand;
		m_fProximity = m_fMinProximity;
	}

	float fProximity = 0.0f;

	if( m_eGuardMode == GM_PatrolProximity || m_eGuardMode == GM_PatrolDefensiveArea )
	{
		fProximity = m_fMinProximity;					//try and get as close to point as possible
	}
	else
	{
		fProximity = m_fProximity;						//get within set distance
	}

	//start in Defending Position, even if a patroller

	//is the ped outside the required proxmity to the DefendPosition
	if (!IsAtPosition(m_vDefendPosition, fProximity, pPed))
	{
		//then setup new task to navigate back to the DefendPosition
		SetState(State_GoToDefendPosition);
		return FSM_Continue;
	}

	//Therefore Ped must be within correct proximity to position, face the target heading
	SetState(State_MoveAchieveHeading);
	return FSM_Continue;
}

void CTaskStandGuard::GoToDefendPosition_OnEnter()
{
	CPed* pPed = GetPed();
	pPed->SetIsCrouching(m_bIsCrouching, -1);

	float fProx = 0.0f;

	if (m_eGuardMode == GM_PatrolProximity || m_eGuardMode == GM_PatrolDefensiveArea)
	{
		fProx = 0.5f;	//different to the usual min prox, as we want the nav mesh to get as close as it can

		//are we in the circle around our defend point?
		if (IsAtPosition(m_vDefendPosition,m_fProximity,pPed))
		{
			//then walk as we are patrolling
			m_fMoveBlendRatio = MOVEBLENDRATIO_WALK;
		}
		else
		{
			//then run, as we need to return to our area ASAP
			m_fMoveBlendRatio = MOVEBLENDRATIO_RUN;
		}
	}
	else
	{
		fProx = m_fProximity;
		//always at run for now when not patrolling
		m_fMoveBlendRatio = MOVEBLENDRATIO_RUN;
	}

	CTask* pSubTask		= rage_new CTaskMoveFollowNavMesh(m_fMoveBlendRatio, m_vDefendPosition, fProx);
	CTask* pAmbientTask	= rage_new CTaskAmbientClips(CTaskAmbientClips::Flag_PlayIdleClips, CONDITIONALANIMSMGR.GetConditionalAnimsGroup("DEFEND"));
	SetNewTask(rage_new CTaskComplexControlMovement(pSubTask, pAmbientTask));
}

CTask::FSM_Return CTaskStandGuard::GoToDefendPosition_OnUpdate()
{
	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//at location (according to nav mesh), so ensure we are facing the right way
		SetState(State_MoveAchieveHeading);
		return FSM_Continue;
	}

	CPed* pPed = GetPed();

	//ensure Nav Mesh has found a route, if not select a new destination

	// The movement control task is currently the highest priority task, find the navmesh route task it is controlling
	CTaskMoveFollowNavMesh* pNavMeshTask = static_cast<CTaskMoveFollowNavMesh*> (pPed->GetPedIntelligence()->GetTaskManager()->FindTaskByTypeWithPriority(PED_TASK_TREE_MOVEMENT, CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH, PED_TASK_MOVEMENT_GENERAL));

	if (pNavMeshTask)
	{
		eNavMeshRouteResult routeResult = pNavMeshTask->GetNavMeshRouteResult();
		if (routeResult == NAVMESHROUTE_ROUTE_NOT_FOUND)
		{
			// Route not found or nav mesh task doesn't exist, try to find a new point if we're not supposed to be standing still
			if (m_eGuardMode != GM_Stand)
			{
				if (SetupNewStandingPoint(pPed))
				{
					//and go to it
					SetState(State_GoToStandPosition);
					return FSM_Continue;
				}
				else
				{
					//no new stand point? try to return to Defend Point instead
					SetupReturnToDefendPoint(pPed);
					//and return to Defend Point
					SetFlag(aiTaskFlags::RestartCurrentState);
					return FSM_Continue;
				}
			}
			return FSM_Continue;
		}
	}
	
	return FSM_Continue;
}

void CTaskStandGuard::GoToStandPosition_OnEnter()
{
	CPed* pPed = GetPed();
	pPed->SetIsCrouching(m_bIsCrouching, -1);

	float fProx = 0.5f;	//different to the usual min prox, as we want the nav mesh to get as close as it can

	//are we in the circle around our defend point?
	if (IsAtPosition(m_vDefendPosition,m_fProximity,pPed))
	{
		//then walk as we are patrolling
		m_fMoveBlendRatio = MOVEBLENDRATIO_WALK;
	}
	else
	{
		//then run, as we need to return to our area ASAP
		m_fMoveBlendRatio = MOVEBLENDRATIO_RUN;
	}

	CTask* pSubTask		= rage_new CTaskMoveFollowNavMesh(m_fMoveBlendRatio, m_vStandPoint, fProx);
	CTask* pAmbientTask	= rage_new CTaskAmbientClips(CTaskAmbientClips::Flag_PlayIdleClips, CONDITIONALANIMSMGR.GetConditionalAnimsGroup("DEFEND"));
	SetNewTask(rage_new CTaskComplexControlMovement(pSubTask, pAmbientTask));
}

CTask::FSM_Return CTaskStandGuard::GoToStandPosition_OnUpdate()
{
	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//at location (according to nav mesh), so ensure we are facing the right way
		SetState(State_MoveAchieveHeading);
		return FSM_Continue;
	}

	CPed* pPed = GetPed();

	//ensure Nav Mesh has found a route, if not select a new destination

	// The movement control task is currently the highest priority task, find the navmesh route task it is controlling
	CTaskMoveFollowNavMesh* pNavMeshTask = static_cast<CTaskMoveFollowNavMesh*> (pPed->GetPedIntelligence()->GetTaskManager()->FindTaskByTypeWithPriority(PED_TASK_TREE_MOVEMENT, CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH, PED_TASK_MOVEMENT_GENERAL));

	if (pNavMeshTask)
	{
		eNavMeshRouteResult routeResult = pNavMeshTask->GetNavMeshRouteResult();
		if (routeResult == NAVMESHROUTE_ROUTE_NOT_FOUND || (routeResult == NAVMESHROUTE_ROUTE_FOUND && pNavMeshTask->IsTotalRouteLongerThan(m_fProximity * sm_Tunables.m_RouteRadiusFactor)))
		{
			// Route not found or nav mesh task doesn't exist, try to find a new point if we're not supposed to be standing still
			if (m_eGuardMode != GM_Stand)
			{
				if (SetupNewStandingPoint(pPed))
				{
					//and go to it
					SetFlag(aiTaskFlags::RestartCurrentState);
					
					return FSM_Continue;
				}
				else
				{
					//no new stand point? try to return to Defend Point instead
					SetupReturnToDefendPoint(pPed);
					//and return to Defend Point
					SetState(State_GoToDefendPosition);
					return FSM_Continue;
				}
			}
			return FSM_Continue;
		}
	}

	return FSM_Continue;
}

void CTaskStandGuard::MoveAchieveHeading_OnEnter()
{
	CPed* pPed = GetPed();
	pPed->SetIsCrouching(m_bIsCrouching, -1);

	CTask* pSubTask = NULL;
	CTask* pAmbientTask = NULL;

	m_nMoveAttempts = 0;	//we have found somewhere to stand, so reset
	if(m_eGuardMode == GM_Stand || m_bIsHeadingToDefendPoint)
	{
		pSubTask		= rage_new CTaskMoveAchieveHeading(m_fHeading, 1.0f, 0.02f);
		pAmbientTask	= rage_new CTaskAmbientClips(CTaskAmbientClips::Flag_PlayIdleClips, CONDITIONALANIMSMGR.GetConditionalAnimsGroup("DEFEND"));
		SetNewTask(rage_new CTaskComplexControlMovement( pSubTask, pAmbientTask ));
	}
	else
	{
		pSubTask		= rage_new CTaskMoveAchieveHeading(m_fStandHeading, 1.0f, 0.02f);
		pAmbientTask	= rage_new CTaskAmbientClips(CTaskAmbientClips::Flag_PlayIdleClips, CONDITIONALANIMSMGR.GetConditionalAnimsGroup("DEFEND"));
		SetNewTask(rage_new CTaskComplexControlMovement( pSubTask, pAmbientTask ));
	}
}

CTask::FSM_Return CTaskStandGuard::MoveAchieveHeading_OnUpdate()
{
	CPed* pPed = GetPed();

	CTask* pMovementTask = ((CTaskComplexControlMovement*)GetSubTask())->GetRunningMovementTask(pPed);

	taskAssert( pMovementTask == NULL || pMovementTask->GetTaskType() == CTaskTypes::TASK_MOVE_ACHIEVE_HEADING );

	if( pMovementTask )
	{
		if(m_eGuardMode == GM_Stand || m_bIsHeadingToDefendPoint)
		{
			((CTaskMoveAchieveHeading*)pMovementTask)->SetHeading(m_fHeading,1.0f, 0.02f);
		}
		else
		{
			((CTaskMoveAchieveHeading*)pMovementTask)->SetHeading(m_fStandHeading,1.0f, 0.02f);
		}
	}

	CTaskMoveAchieveHeading* pAchieveTask = (CTaskMoveAchieveHeading*) ((CTaskComplexControlMovement*)GetSubTask())->GetBackupCopyOfMovementSubtask();

	if (pAchieveTask)
	{
		if(m_eGuardMode == GM_Stand || m_bIsHeadingToDefendPoint)
		{
			pAchieveTask->SetHeading(m_fHeading,1.0f, 0.02f);
		}
		else
		{
			pAchieveTask->SetHeading(m_fStandHeading,1.0f, 0.02f);
		}
	}

	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//heading the correct direction, so stand still (this will check proximity)
		if(m_eGuardMode == GM_PatrolProximity || m_eGuardMode == GM_PatrolDefensiveArea)
		{
			if(m_bIsHeadingToDefendPoint)
			{
				m_nStandingPointTime = fwRandom::GetRandomNumberInRange(sm_Tunables.m_MinDefendPointWaitTimeMS, sm_Tunables.m_MaxDefendPointWaitTimeMS);
			}
			else
			{
				m_nStandingPointTime = fwRandom::GetRandomNumberInRange(sm_Tunables.m_MinStandWaitTimeMS, sm_Tunables.m_MaxStandWaitTimeMS);
			}
		}
		else
		{
			m_nStandingPointTime = -1;						//will stand "forever" - until attacked/disturbed
		}
		SetState(State_MoveStandStill);
	}
	return FSM_Continue;
}

void CTaskStandGuard::MoveStandStill_OnEnter()
{
	CPed* pPed = GetPed();
	pPed->SetIsCrouching(m_bIsCrouching, -1);

	float fProximity = 0.0f;

	if(m_eGuardMode == GM_PatrolProximity || m_eGuardMode == GM_PatrolDefensiveArea)
	{
		fProximity = m_fMinProximity;					//try and get as close to point as possible
	}
	else
	{
		fProximity = m_fProximity;						//get within set distance
	}

	m_vStandPoint = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		
	if (IsAtPosition(m_vStandPoint,fProximity,pPed))
		m_nMoveAttempts = 0;	//we have found somewhere to stand, so reset

	CTaskMoveStandStill* pSubTask		= rage_new CTaskMoveStandStill(m_nStandingPointTime,m_eGuardMode == GM_Stand);
	CTaskAmbientClips* pAmbientTask	= rage_new CTaskAmbientClips(CTaskAmbientClips::Flag_PlayIdleClips, CONDITIONALANIMSMGR.GetConditionalAnimsGroup("DEFEND"));
	SetNewTask(rage_new CTaskComplexControlMovement( pSubTask, pAmbientTask ));

	m_vLastStandPoint = m_vStandPoint;
}

CTask::FSM_Return CTaskStandGuard::MoveStandStill_OnUpdate()
{
	CPed* pPed = GetPed();

	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//first are we at the main Defend Point already?
		if (!IsAtPosition(m_vDefendPosition, m_fMinProximity, pPed))
		{
			//if not, check randomly whether to simply return there
			if (fwRandom::GetRandomNumberInRange(0.0f, 1.0f) <= m_fReturnToDefendPointChance)
			{
				SetupReturnToDefendPoint(pPed);
				//and return to Defend Point
				SetState(State_GoToDefendPosition);
				return FSM_Continue;
			}
			else
			{
				//first increment return to defend point chance
				m_fReturnToDefendPointChance +=0.05f;
				if(m_fReturnToDefendPointChance > 1.0f) m_fReturnToDefendPointChance = 1.0f;

				if( SetupNewStandingPoint(pPed) )
				{
					//and move there
					SetState(State_GoToStandPosition);
					return FSM_Continue;
				}
				else
				{
					SetupReturnToDefendPoint(pPed);

					//and move there
					SetState(State_GoToDefendPosition);
					return FSM_Continue;
				}
			}
		}
		else if (m_eGuardMode != GM_Stand)
		{
			//now select random cover point within desired area
			if (SetupNewStandingPoint(pPed))
			{
				//and move there
				SetState(State_GoToStandPosition);
				return FSM_Continue;
			}
			else
			{
				//set flag so we know we are heading or at Defend Point
				m_bIsHeadingToDefendPoint = true;

				//no valid cover point - so stay at Defend Point
				if(m_eGuardMode == GM_PatrolProximity || m_eGuardMode == GM_PatrolDefensiveArea)
				{
					if(m_bIsHeadingToDefendPoint)
					{
						m_nStandingPointTime = fwRandom::GetRandomNumberInRange(sm_Tunables.m_MinDefendPointWaitTimeMS, sm_Tunables.m_MaxDefendPointWaitTimeMS);
					}
					else
					{
						m_nStandingPointTime = fwRandom::GetRandomNumberInRange(sm_Tunables.m_MinStandWaitTimeMS, sm_Tunables.m_MaxStandWaitTimeMS);
					}
					SetFlag(aiTaskFlags::RestartCurrentState);
					return FSM_Continue;
				}	
			}
		}	
	}
	return FSM_Continue;
}

// Checks if we are at or near enough to requested position
bool CTaskStandGuard::IsAtPosition(Vector3 &vPos, float fProximity, CPed *pPed)
{
	Vector3 vDiff = vPos - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

	float fDistSqr	= vDiff.XYMag2();
	float fProx		= fProximity*fProximity;

	if(fProx < m_fMinProximity) fProx = m_fMinProximity;

	if(fDistSqr > fProx)		return(false);
	else						return(true);
}

// Finds and sets a new standing point from a cover points vantage point
bool CTaskStandGuard::SetupNewStandingPoint(CPed *pPed)
{
	Vector3 vDirection( 0.0f, 1.0f, 0.0f );
	vDirection.RotateZ( m_fHeading );

	//first release any existing cover points reserved for this ped
	if(pPed->GetCoverPoint())
	{
		pPed->ReleaseCoverPoint();
	}

	// If the ped has a defensive area, set the validity function
	CCover::CoverPointFilterConstructFn* pFilterConstructFn = NULL;
	void* pData = NULL;
	if( m_eGuardMode == GM_PatrolDefensiveArea )
	{
		pFilterConstructFn = CCoverPointFilterDefensiveArea::Construct;
		pData = (void*)pPed;
	}

	fwFlags32 iSearchFlags = CCover::SF_Random | CCover::SF_CheckForTargetsInVehicle;
	CCoverPoint* pCoverPoint = NULL;
	CCover::FindCoverPointInArc(pPed, &m_vStandPoint, m_vDefendPosition, vDirection, 
		TWO_PI, 1.0f, m_fProximity, iSearchFlags, VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), &pCoverPoint, 1,
		(CCover::eCoverSearchType) m_nCoverSearchMode, NULL, 0, *pFilterConstructFn, pData );

	if(pCoverPoint)
	{
		pPed->SetCoverPoint(pCoverPoint);
		Vector3 vDirection;
		Vector3 vVantagePoint;

		vDirection.Zero();
		vVantagePoint.Zero();

		//reserve cover point for Ped
		pPed->GetCoverPoint()->ReserveCoverPointForPed(pPed);

		//create direction vector
		vDirection.operator =( VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - m_vStandPoint);

		//locate vantage point from selected cover point	
		CCover::CalculateVantagePointForCoverPoint(pPed->GetCoverPoint(),vDirection,m_vStandPoint,vVantagePoint);

		//get cover direction and record it for later
		vDirection = VEC3V_TO_VECTOR3(pPed->GetCoverPoint()->GetCoverDirectionVector(&RCC_VEC3V(vDirection)));

		//decide whether to FACE cover point (quite often a wall) or FACE AWAY from it
		m_fStandHeading=fwAngle::GetRadianAngleBetweenPoints(vDirection.x,vDirection.y,0.0f,0.0f);
		if( fwRandom::GetRandomNumberInRange(0.0f,1.0f) < 0.9f)
		{
			m_fStandHeading=fwAngle::LimitRadianAngle(m_fStandHeading - PI);
		}

		//set current stand point to found vantage point
		m_vStandPoint = vVantagePoint; 
	}
	else
	{
		// Pick a new random point within our guard radius.
		Vector3 vTargetPosition = m_vDefendPosition;

		Vector3 vOffset;

		float fRandHeading = fwRandom::GetRandomNumberInRange(0.0f, 2.0f * PI);

		float fRange = fwRandom::GetRandomNumberInRange(sm_Tunables.m_MinNavmeshPatrolRadiusFactor, sm_Tunables.m_MaxNavmeshPatrolRadiusFactor);
		fRange *= m_fProximity;

		vOffset.x = -rage::Sinf(fRandHeading) * fRange;
		vOffset.y = rage::Cosf(fRandHeading) * fRange;
		vOffset.z = 0.0f;

		vTargetPosition += vOffset;

		m_vStandPoint = vTargetPosition;

		m_fStandHeading = fwAngle::LimitRadianAngle(fRandHeading);
	}
	
	//increment "return to defend point chance"
	m_fReturnToDefendPointChance +=0.05f;
	if(m_fReturnToDefendPointChance > 1.0f) m_fReturnToDefendPointChance = 1.0f;


	if(m_fReturnToDefendPointChance > 1.0f) m_fReturnToDefendPointChance = 1.0f;

	//set flag so we know we are NOT heading or at Defend Point
	m_bIsHeadingToDefendPoint = false;


	return true;
}

// tells the ped to return to its defend point, and cleans up any patrol vars
bool CTaskStandGuard::SetupReturnToDefendPoint(CPed *pPed)
{
	//set current stand point to found vantage point
	m_vStandPoint = m_vDefendPosition; 

	//reset "return to defend point chance"
	m_fReturnToDefendPointChance = 0.3f;

	//set flag so we know we are heading or at Defend Point
	m_bIsHeadingToDefendPoint = true;

	//release cover point for Ped
	if(pPed->GetCoverPoint())
	{
		pPed->ReleaseCoverPoint();
	}

	return true;
}

#if !__FINAL
const char * CTaskStandGuard::GetStaticStateName( s32 iState )
{
	taskAssert(iState >= State_Start && iState <= State_Finish);

	switch (iState)
	{
		case State_Start:				return "State_Start";
		case State_MoveAchieveHeading:	return "State_MoveAchieveHeading";
		case State_GoToDefendPosition:	return "State_GoToDefendPosition";
		case State_GoToStandPosition:	return "State_GoToStandPosition";
		case State_MoveStandStill:		return "State_MoveStandStill";
		case State_Finish:				return "State_Finish";
		default: taskAssert(0); 	
	}
	return "State_Invalid";
}
#endif // !__FINAL

//////////////////////////////////
//		TASK SEPARATE		    //
//////////////////////////////////

// Task to separate peds whilst they are in combat
float CTaskSeparate::MIN_SEPARATION_DISTANCE = 3.0f;	// Distance if a single ped is within to try and separate
float CTaskSeparate::MAX_SEPARATION_DISTANCE = 6.0f;	// Distance to consider peds to find the avg center of concentration
float CTaskSeparate::LOS_CHECK_INTERVAL_DISTANCE = 0.33f;// Distance between each LOS check
bool CTaskSeparate::ms_bPreserveSlopeInfoInPath = false; // Whether to add waypoints into path whenever slope changes significantly

CTaskSeparate::CTaskSeparate(const Vector3* pvSeparationCentre, const Vector3& vTargetPos, const CPed* pTargetPed, aiTask* pAdditionalSubTask, bool bStrafing)
: m_vSeparationCentre(0.0f, 0.0f, 0.0f),
m_bHasSeparationCentre(false),
m_iPathPoints(0),
m_iLastCheckedInterval(0),
m_vTargetPos(vTargetPos),
m_pTargetPed(pTargetPed),
m_pAdditionalSubTask(pAdditionalSubTask),
m_bStrafe(bStrafing),
m_bTriedRandomSeparationDirection(false),
m_iNumberOffendingPeds(0),
m_bSeparateBlind(false)
{
	if (pvSeparationCentre)
	{
		m_vSeparationCentre = *pvSeparationCentre;
		m_bHasSeparationCentre = true;
	}
	SetInternalTaskType(CTaskTypes::TASK_SEPARATE);
}

CTaskSeparate::~CTaskSeparate()
{
	if (m_pAdditionalSubTask)
	{
		delete m_pAdditionalSubTask;
		m_pAdditionalSubTask = NULL;
	}
}

aiTask* CTaskSeparate::Copy() const
{
	return rage_new CTaskSeparate( &m_vSeparationCentre, m_vTargetPos, m_pTargetPed, m_pAdditionalSubTask?m_pAdditionalSubTask->Copy():NULL, m_bStrafe );
}

void CTaskSeparate::CleanUp()
{
	m_AsyncProbeHelper.ResetProbe();
	m_RouteSearchHelper.ResetSearch();
}

CTask::FSM_Return CTaskSeparate::ProcessPreFSM()
{
	// Abort if the target is invalid
	if (!m_pTargetPed)
	{
		return FSM_Quit;
	}

	if (m_bStrafe)
	{
		CPed* pPed = GetPed();
		pPed->SetIsStrafing(true);
		pPed->SetDesiredHeading(VEC3V_TO_VECTOR3(m_pTargetPed->GetTransform().GetPosition()));
	}

	return FSM_Continue;
}

// Main state machine update function
CTask::FSM_Return CTaskSeparate::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();	

		FSM_State(State_Separating)
			FSM_OnUpdate
				return Separating_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskSeparate::Start_OnUpdate()
{
	CPed* pPed = GetPed();

	// If no centre point was specifed, work out the average bunching point
	if (!m_bHasSeparationCentre)
	{
		if (NeedsToSeparate(pPed, m_pTargetPed, VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), CTaskSeparate::MIN_SEPARATION_DISTANCE, &m_vSeparationCentre, &m_iNumberOffendingPeds ))
		{
			m_bHasSeparationCentre = true;
		}
		else
		{
			SetState(State_Finish);
			return FSM_Continue;
		}
	}

	// Create the flee path request
	CreateFleePathRequest(pPed, false);

	if (m_RouteSearchHelper.IsSearchActive() )
	{
		if (m_pAdditionalSubTask)
		{
			SetNewTask(m_pAdditionalSubTask->Copy());
			SetFlag(aiTaskFlags::KeepCurrentSubtaskAcrossTransition);
		}
		SetState(State_Separating);
		return FSM_Continue;
	}

	SetState(State_Finish);
	return FSM_Continue;
}

CTask::FSM_Return CTaskSeparate::Separating_OnUpdate()
{
	if (!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Finish);
		return FSM_Continue;
	}

	if (GetSubTask() && GetSubTask()->GetTaskType() != CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
	{
		if (m_RouteSearchHelper.IsSearchActive())
		{
			float fDistance = 0.0f;
			Vector3 vLastPos(0.0f, 0.0f, 0.0f);
			SearchStatus eSearchStatus;
			m_iPathPoints = MAX_POINTS_TO_STORE_ALONG_PATH;

			if (m_RouteSearchHelper.GetSearchResults( eSearchStatus, fDistance, vLastPos, m_avPathPoints, &m_iPathPoints ) )
			{
				if( eSearchStatus == SS_SearchSuccessful )
					m_iLastCheckedInterval = 0;
			}
		}

		if (!m_RouteSearchHelper.IsSearchActive())
		{
			CPed* pPed = GetPed();

			Vector3 vTestPosition;
			if (m_iLastCheckedInterval < NUM_LOS_CHECKS &&
				CheckNextIntevalPosition(vTestPosition,pPed))
			{
					CTask* pNewSubtask = rage_new CTaskMoveFollowNavMesh(MOVEBLENDRATIO_RUN, 
						vTestPosition,		
						CTaskMoveFollowNavMesh::ms_fTargetRadius, 
						CTaskMoveFollowNavMesh::ms_fSlowDownDistance, 
						-1,
						true,
						false,
						NULL);
					SetNewTask(rage_new CTaskComplexControlMovement(pNewSubtask, (CTask*)GetSubTask()->Copy()));
					return FSM_Continue;
			}

			// If all intervals have been checked
			if (m_iLastCheckedInterval >= NUM_LOS_CHECKS)
			{
				// Try to create a random path first
				if (!m_bTriedRandomSeparationDirection)
				{
					m_iLastCheckedInterval = 0;
					dev_s32 NUMBER_NEARBY_PEDS_TO_SEPARATE_BLIND = 5;
					if( m_iNumberOffendingPeds > NUMBER_NEARBY_PEDS_TO_SEPARATE_BLIND )
						m_bSeparateBlind = true;
					CreateFleePathRequest(pPed, true);
					m_bTriedRandomSeparationDirection = true;
				}

				// no path request, so finish here
				if (!m_RouteSearchHelper.IsSearchActive())
				{
					SetState(State_Finish);
				}
			}
		}
	}
	else
	{
		Assert(!m_RouteSearchHelper.IsSearchActive()); // << make sure pathfinder is not getting spammed/stalled
	}
	return FSM_Continue;
}

dev_float TARGET_SEPARATION_DISTANCE = 3.0f;

bool CTaskSeparate::NeedsToSeparate( CPed* pPed, const CPed* pTargetPed, const Vector3& vPedPosition, float fSeparationDist, Vector3* pvCentre, s32* piNumberOffendingPeds, bool bIgnoreTarget )
{
	float	fClosestDistanceSq = 99999.0f;
	float	fClosestDistanceToThreatSq = 99999.0f;
	Vector3 vAveragePosition(0.0f, 0.0f, 0.0f);
	s32	iNumNearbyPeds = 0;

	const CEntityScannerIterator entityList = pPed->GetPedIntelligence()->GetNearbyPeds();
	for(const CEntity* pEnt = entityList.GetFirst(); pEnt; pEnt = entityList.GetNext())
	{
		const CPed* pThisPed = static_cast<const CPed*>(pEnt);

		Vector3 vecThisPedPosition(VEC3V_TO_VECTOR3(pThisPed->GetTransform().GetPosition()));
		float fDistanceSq = vecThisPedPosition.Dist2( vPedPosition );
		const bool bFriendly = pPed->GetPedIntelligence()->IsFriendlyWith( *pThisPed );
		const bool bThreat = !bFriendly && ( ( pThisPed == pTargetPed ) || pPed->GetPedIntelligence()->IsThreatenedBy(*pThisPed, true, false, true) );
		if(	pThisPed!=pPed && 
			( bFriendly || bThreat ) &&
			( fDistanceSq < rage::square(MAX_SEPARATION_DISTANCE) ) ) 
		{
			if( fClosestDistanceSq > fDistanceSq )
				fClosestDistanceSq = fDistanceSq;

			if( bThreat && fClosestDistanceToThreatSq > fDistanceSq )
				fClosestDistanceToThreatSq = fDistanceSq; 

			++iNumNearbyPeds;
			vAveragePosition += vecThisPedPosition;
		}
	}

	// If the closest ped is far enough away, no need to separate
	if( iNumNearbyPeds == 0 ||
		( (fClosestDistanceSq > rage::square(fSeparationDist)) && ( bIgnoreTarget || (fClosestDistanceToThreatSq > rage::square(TARGET_SEPARATION_DISTANCE) ) ) ))
	{
		return false;
	}

	if( piNumberOffendingPeds )
	{
		*piNumberOffendingPeds = iNumNearbyPeds;
	}

	// Work out the average position
	if( pvCentre )
	{
		vAveragePosition.Scale( 1.0f/(float)iNumNearbyPeds );
		*pvCentre = vAveragePosition;
	}

	return true;
}

void CTaskSeparate::CreateFleePathRequest( CPed* pPed, bool bRandom )
{
	// Build a normalised vector towards the target
	Vector3 vTargetPos = m_vTargetPos;
	if( m_pTargetPed )
	{	//	I've changed this so that it sets vTargetPos. Is that correct?
		m_vTargetPos = VEC3V_TO_VECTOR3(m_pTargetPed->GetTransform().GetPosition());
	}

	//	I don't think the ped's position will change inside this function so it should be safe to just get the position once here
	const Vector3 vecPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

	Vector3 vFleeDir(0.0f, 1.0f, 0.0f);

	if( bRandom )
	{
		vFleeDir.RotateZ(fwRandom::GetRandomNumberInRange(0.0f, TWO_PI) );
	}
	else
	{
		// Shift the point out to help out the flee route, it gets confused by nearby positions
		vFleeDir = m_vSeparationCentre - vecPedPosition;
		vFleeDir.z = 0.0f;
		vFleeDir.Normalize();
	}

	Vector3 vToTarget = vTargetPos - vecPedPosition;
	vToTarget.z = 0.0f;
	vToTarget.Normalize();

#if __DEBUG
	m_vInitialCentre = (vFleeDir*20.0f) + vecPedPosition;
#endif

	// Stop the direction being directly towards the target
	float fDot = vFleeDir.Dot(vToTarget);
	if( fDot < -0.707f )
	{
		Vector3 vToTargetRight;
		vToTargetRight.Cross(vToTarget, Vector3(0.0f, 0.0f, 1.0f));
		float fCrossDot = vFleeDir.Dot(vToTargetRight);

		if( fCrossDot < 0.0f )
		{
			vFleeDir = vToTargetRight;//vToTarget;
		}
		else
		{
			vFleeDir = -vToTargetRight;//-vToTarget;
		}

		// 		if( fCrossDot > 0.0f )
		// 		{
		// 			vFleeDir.RotateZ(-( DtoR * 60.0f));
		// 		}
		// 		else
		// 		{
		// 			vFleeDir.RotateZ(( DtoR * 60.0f));
		// 		}
#if __DEV
		if(CPedDebugVisualiser::GetDebugDisplay()!=CPedDebugVisualiser::eOff)
			grcDebugDraw::Line(vecPedPosition, vecPedPosition + vFleeDir, Color32(0.0f, 1.0f, 0.0f));
#endif
	}

	Vector3 vFleePos = (vFleeDir*20.0f) + vecPedPosition;
#if __DEBUG
	m_vRecalculatedCentre = vFleePos;
#endif
	TInfluenceSphere sphere;
	//	Is it safe to assume that m_pTargetPed isn't NULL? It was checked earlier
	sphere.Init(VEC3V_TO_VECTOR3(m_pTargetPed->GetTransform().GetPosition()), 5.0f, 9999.0f, 9999.0f);

	u64 iFlags = pPed->GetPedIntelligence()->GetNavCapabilities().BuildPathStyleFlags() | pPed->GetCurrentMotionTask()->GetNavFlags() | PATH_FLAG_FLEE_TARGET;

	if(ms_bPreserveSlopeInfoInPath)	// Optionally enable this feature.
		iFlags |= PATH_FLAG_PRESERVE_SLOPE_INFO_IN_PATH;

	aiAssertf(!m_RouteSearchHelper.IsSearchActive(), "m_iLastCheckedInterval=%i, m_bTriedRandomSeparationDirection=%s", m_iLastCheckedInterval, m_bTriedRandomSeparationDirection?"true":"false");

#if __BANK
	ms_debugDraw.AddSphere(RCC_VEC3V(vFleePos), 0.025f, Color_red, 1000);
	ms_debugDraw.AddLine(RCC_VEC3V(vecPedPosition), RCC_VEC3V(vFleePos), Color_red, 1000);
#endif

	const float fFleeDistance = 20.0f; //50.0f; // 20.0f
	m_RouteSearchHelper.StartSearch(pPed, vecPedPosition, vFleePos, 0.0f, iFlags, 1, &sphere, fFleeDistance );
}

//-------------------------------------------------------------------------
// Returns the interval along the stored route
//-------------------------------------------------------------------------
bool CTaskSeparate::GetIntervalAlongRoute( int iInterval, CPed* pPed, Vector3& vOutInteval )
{
	float fTotalTraversedDistance = 0.0f;
	float fDesiredDistance = ((float) (/*NUM_LOS_CHECKS - */iInterval + 1)) * LOS_CHECK_INTERVAL_DISTANCE;
	for( s32 i = -1; i < (m_iPathPoints - 1); i++ )
	{
		float fDistanceLeft = fDesiredDistance - fTotalTraversedDistance;
		Vector3 vPosition;
		Vector3 vNextPosition;
		if( i == -1 )
		{
			vPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			vPosition.z += 1.0f;
		}
		else
		{
			vPosition = m_avPathPoints[i];
			vPosition.z += 2.0f;
		}
		vNextPosition = m_avPathPoints[i+1];

		Vector3 vToNextPos = vNextPosition - vPosition;
		float fDistanceBetweenPoints = vToNextPos.XYMag();
		if(fDistanceLeft < fDistanceBetweenPoints )
		{
			vToNextPos.Scale(fDistanceLeft/fDistanceBetweenPoints);
			vOutInteval = vPosition + vToNextPos;
			return true;
		}
		else
		{
			fTotalTraversedDistance += fDistanceBetweenPoints;
		}
	}
	return false;
}

//-------------------------------------------------------------------------
// Checks the next interval along the route to see if its valid
//-------------------------------------------------------------------------
bool CTaskSeparate::CheckNextIntevalPosition( Vector3& vOutInteval, CPed* pPed )
{
	++m_iLastCheckedInterval;

	// Get the next interval position
	if( GetIntervalAlongRoute( m_iLastCheckedInterval, pPed, vOutInteval ) )
	{
		const Vector3 vecPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		if( m_AsyncProbeHelper.IsProbeActive() )
		{
			ProbeStatus eProbeStatus;
			if( m_AsyncProbeHelper.GetProbeResults(eProbeStatus) )
			{
				if( eProbeStatus == PS_Clear )
					return true;
				else
					return false;
			}
			// Check again next frame
			--m_iLastCheckedInterval;
			return false;
		}
		else if( vOutInteval.Dist2(m_vSeparationCentre) >
			vecPedPosition.Dist2(m_vSeparationCentre) )
		{
			const Vector3 vecTargetPedPosition = VEC3V_TO_VECTOR3(m_pTargetPed->GetTransform().GetPosition());
			float fDistToPlayerSq = vOutInteval.Dist2(vecTargetPedPosition);
			// Don't consider this point if its too close to the target
			if( m_pTargetPed && ( (fDistToPlayerSq > rage::square(4.0f)) || (fDistToPlayerSq > vecPedPosition.Dist2(vecTargetPedPosition))) )
			{
				// Don't even condsider this point if the ped would need to separate again straight away
				if( !NeedsToSeparate(pPed, m_pTargetPed, vOutInteval, 1.0f, NULL ) )
				{
					// Check LOS to the target
					Vector3 vTargetVantage = m_vTargetPos;
					// If firing at a target in cover, find that peds vantage point for the LOS
					if( m_pTargetPed && m_pTargetPed->GetCoverPoint() )
					{
						Vector3 vCoverPos;
						Vector3 vVantagePos;
						if(CCover::FindCoordinatesCoverPoint(m_pTargetPed->GetCoverPoint(), m_pTargetPed, vecPedPosition - vecTargetPedPosition, vCoverPos, &vVantagePos))
						{
							vTargetVantage = vVantagePos;
						}
					}

					if( m_bSeparateBlind )
					{
						return true;
					}
					// Start a LOS probe async test anc check again next frame
					WorldProbe::CShapeTestProbeDesc probeData;
					probeData.SetStartAndEnd(vOutInteval,vTargetVantage);
					probeData.SetContext(WorldProbe::ELosCombatAI);
					probeData.SetExcludeEntity(m_pTargetPed ? m_pTargetPed->GetVehiclePedInside() : NULL);
					probeData.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_OBJECT_TYPE|ArchetypeFlags::GTA_VEHICLE_TYPE);
					probeData.SetOptions(0); 
					if( m_AsyncProbeHelper.StartTestLineOfSight(probeData)	)
					{
						--m_iLastCheckedInterval;
						return false;
					}
					// 					if( WorldProbe::GetIsLineOfSightClear(vOutInteval, vTargetVantage, m_pTargetPed ? m_pTargetPed->GetVehiclePedInside() : NULL, ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_OBJECT_TYPE|ArchetypeFlags::GTA_VEHICLE_TYPE, 0, WorldProbe::LOS_SeparateAI) )
					// 					{
					// 						return true;
					// 					}
				}
			}
		}
		return false;
	}
	// Abort the checks, the routes too short 
	m_iLastCheckedInterval = NUM_LOS_CHECKS;
	return false;
}

#if !__FINAL
const char * CTaskSeparate::GetStaticStateName( s32 iState )
{
	taskAssert(iState >= State_Start && iState <= State_Finish);

	switch (iState)
	{
	case State_Start:				return "State_Start";
	case State_Separating:			return "State_Separating";
	case State_Finish:				return "State_Finish";
	default: taskAssert(0); 	
	}

	return "State_Invalid";
}

void CTaskSeparate::Debug() const
{
#if DEBUG_DRAW
	const Vector3 vecPedPosition = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition());
	if( m_bHasSeparationCentre )
	{
		grcDebugDraw::Sphere( m_vSeparationCentre, 0.5f, Color32(1.0f, 0.0f, 0.0f) );
		grcDebugDraw::Line(m_vSeparationCentre, vecPedPosition, Color32(1.0f, 0.0f, 0.0f) );
	}
	for( s32 i = 0; i < m_iPathPoints; i++ )
	{
		grcDebugDraw::Sphere( m_avPathPoints[i], 0.5f, Color32(0.0f, 1.0f, 0.0f) );
		if( i == 0 )
		{
			grcDebugDraw::Line(m_avPathPoints[i], vecPedPosition, Color32(0.0f, 1.0f, 0.0f) );
		}
		if( i < ( m_iPathPoints - 1) )
		{
			grcDebugDraw::Line(m_avPathPoints[i], m_avPathPoints[i+1], Color32(0.0f, 1.0f, 0.0f) );
		}
	}

#if __DEBUG
	grcDebugDraw::Sphere( m_vInitialCentre, 0.5f, Color32(0.0f, 1.0f, 0.0f) );
	grcDebugDraw::Sphere( m_vRecalculatedCentre, 0.5f, Color32(0.0f, 1.0f, 0.0f) );
	grcDebugDraw::Line( m_vInitialCentre, vecPedPosition, Color32(0.0f, 1.0f, 0.0f) );
	grcDebugDraw::Line( m_vRecalculatedCentre, vecPedPosition, Color32(0.0f, 0.0f, 1.0f) );
#endif
#endif	// DEBUG_DRAW
}

#endif

//----------------------------------------------------
// CClonedCombatClosestTargetInAreaInfo
//----------------------------------------------------

const float CClonedCombatClosestTargetInAreaInfo::MAX_AREA = 300.0f;

CClonedCombatClosestTargetInAreaInfo::CClonedCombatClosestTargetInAreaInfo() :
m_Pos(VEC3_ZERO)
, m_Area(0.0f)
, m_GetPosFromPed(false)
, m_CombatTimer(CTaskCombatClosestTargetInArea::INVALID_OVERALL_TIME)
{
}

CClonedCombatClosestTargetInAreaInfo::CClonedCombatClosestTargetInAreaInfo(s32 state, const Vector3 &pos, float area, bool getPosFromPed, float combatTimer) :
m_Pos(pos)
, m_Area(area)
, m_GetPosFromPed(getPosFromPed)
, m_CombatTimer(combatTimer)
{
    SetStatusFromMainTaskState(state);
}

CTask *CClonedCombatClosestTargetInAreaInfo::CreateLocalTask(fwEntity *pEntity)
{
	CTaskCombatClosestTargetInArea* pTask = rage_new CTaskCombatClosestTargetInArea(m_Pos, m_Area, m_GetPosFromPed, m_CombatTimer);

	CPed* pPed = static_cast<CPed*>(pEntity);

	if (pPed)
	{
		// make sure we keep the same target as the ped had before migration
		CClonedCombatTaskInfo* pCombatTaskInfo = (CClonedCombatTaskInfo*) pPed->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(CTaskTypes::TASK_COMBAT, PED_TASK_PRIORITY_MAX, false);

		if (pCombatTaskInfo && pCombatTaskInfo->GetTarget() && pCombatTaskInfo->GetTarget()->GetIsTypePed())
		{
			pTask->SetInitialTarget((CPed*)pCombatTaskInfo->GetTarget());
		}
	}

	return pTask;
}

//////////////////////////////////////////////////////
//		TASK COMBAT CLOSEST TARGET IN AREA		    //
//////////////////////////////////////////////////////

const float CTaskCombatClosestTargetInArea::INVALID_OVERALL_TIME = -1.0f;

CTaskCombatClosestTargetInArea::CTaskCombatClosestTargetInArea( const Vector3& vPos, const float fArea, const bool bGetPosFromPed, float fOverallTime, fwFlags32 uCombatConfigFlags )
: m_vPos(vPos)
, m_fArea(fArea)
, m_bGetPosFromPed(bGetPosFromPed)
, m_fCombatTimer(fOverallTime)
, m_pPreviousTarget(NULL)
, m_pInitialTarget(NULL)
, m_uCombatConfigFlags(uCombatConfigFlags)
{
	SetInternalTaskType(CTaskTypes::TASK_COMBAT_CLOSEST_TARGET_IN_AREA);
}

CTaskCombatClosestTargetInArea::~CTaskCombatClosestTargetInArea()
{

}

CTaskInfo *CTaskCombatClosestTargetInArea::CreateQueriableState() const
{
	return rage_new CClonedCombatClosestTargetInAreaInfo(GetState(), m_vPos, m_fArea, m_bGetPosFromPed, m_fCombatTimer);
}

CTask::FSM_Return CTaskCombatClosestTargetInArea::ProcessPreFSM()
{
	if (m_fCombatTimer > 0.0f && GetTimeRunning() >= m_fCombatTimer)
	{
		return FSM_Quit;
	}
	return FSM_Continue;
}

// Main state machine update function
CTask::FSM_Return CTaskCombatClosestTargetInArea::UpdateFSM( s32 iState, FSM_Event iEvent )
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();	

		FSM_State(State_WaitForTarget)
			FSM_OnEnter
				WaitForTarget_OnEnter();
			FSM_OnUpdate
				return WaitForTarget_OnUpdate();	

		FSM_State(State_Combat)
			FSM_OnEnter
				Combat_OnEnter();
			FSM_OnUpdate
				return Combat_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskCombatClosestTargetInArea::Start_OnUpdate()
{
	CPed* pPed = GetPed();
	taskAssert(pPed);

	const bool bPreferKnownTargetsWhenGoingToCombat = pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_PreferKnownTargetsWhenCombatClosestTarget);
	if (bPreferKnownTargetsWhenGoingToCombat)
	{
		SetState(State_WaitForTarget);
	}
	else
	{
		SetState(State_Combat);
	}

	return FSM_Continue;
}

void CTaskCombatClosestTargetInArea::WaitForTarget_OnEnter()
{
	static const float fMAX_TIME_TO_WAIT_FOR_TARGET = 0.1f;
	m_WaitForTargetTimer.Reset(fMAX_TIME_TO_WAIT_FOR_TARGET);

	if (fMAX_TIME_TO_WAIT_FOR_TARGET > 0.0f)
	{
		const CPed* pPed = GetPed();
		taskAssert(pPed);

		//Check if the motion task is valid.
		const CTaskMotionPed* pMotionTask = static_cast<CTaskMotionPed *>(pPed->GetPedIntelligence()->GetTaskManager()->FindTaskByTypeActive(PED_TASK_TREE_MOTION, CTaskTypes::TASK_MOTION_PED));
		if(pMotionTask)
		{
			// If the ped is asiming, we add a blocked weapon task to keep him doing that
			if(pMotionTask->IsAiming())
			{
				fwMvClipId wallBlockClipId = CLIP_ID_INVALID;
				fwMvClipSetId appropriateWeaponClipSetId = CLIP_SET_ID_INVALID;
				if(CTaskWeaponBlocked::GetWallBlockClip(pPed, pPed->GetEquippedWeaponInfo(), appropriateWeaponClipSetId, wallBlockClipId)) 
				{
					SetNewTask(rage_new CTaskWeaponBlocked(CWeaponController::WCT_Aim));
				}
			}
		}

	}
}

CTask::FSM_Return CTaskCombatClosestTargetInArea::WaitForTarget_OnUpdate()
{
	CPed* pPed = GetPed();
	taskAssert(pPed);

	if (!m_pInitialTarget)
	{
		m_pInitialTarget = FindClosestTarget(pPed);
	}

	// Wait until we have a valid target or the max time is over
	if (m_pInitialTarget || m_WaitForTargetTimer.Tick())
	{
		SetState(State_Combat);
	}

	return FSM_Continue;
}

bool SortSpatialArrayResults(const CSpatialArray::FindResult& a, const CSpatialArray::FindResult& b)
{
	return PositiveFloatLessThan_Unsafe(a.m_DistanceSq, b.m_DistanceSq);
}

const CPed* CTaskCombatClosestTargetInArea::FindClosestTarget(CPed* pPed)
{
	const CPed* pBestTargetPed = NULL;
	ScalarV scBestTargetDistance2(FLT_MAX);

	CPedTargetting* pPedTargeting = pPed->GetPedIntelligence()->GetTargetting(false);
	int iNumActiveTargets = pPedTargeting ? pPedTargeting->GetNumActiveTargets() : 0;
	for (int iTargetIdx = 0; iTargetIdx < iNumActiveTargets; ++iTargetIdx)
	{
		const CEntity* pTargetEntity = pPedTargeting->GetTargetAtIndex(iTargetIdx);
		if (pTargetEntity && pTargetEntity->GetIsTypePed())
		{
			const CPed* pTargetPed = SafeCast(const CPed, pTargetEntity);
			const ScalarV scTargetDistance2 = DistSquared(pTargetPed->GetTransform().GetPosition(), pPed->GetTransform().GetPosition());
			if (IsLessThanAll(scTargetDistance2, scBestTargetDistance2))
			{
				pBestTargetPed = pTargetPed;
				scBestTargetDistance2 = scTargetDistance2;
			}
		}
	}

	return pBestTargetPed;
}

const CPed* CTaskCombatClosestTargetInArea::SearchForNewTargets(CPed* pPed)
{
	const CPed* pBestTargetPed = NULL;
	Vec3V vCentre = m_bGetPosFromPed ? pPed->GetTransform().GetPosition() : VECTOR3_TO_VEC3V(m_vPos);

	// Use the spatial array to get our peds within the radius and then sort them by distance
	const int maxNumPeds = 128;
	CSpatialArray::FindResult results[maxNumPeds];

	int numFound = CPed::ms_spatialArray->FindInSphere(vCentre, m_fArea, &results[0], maxNumPeds);
	if(numFound)
	{
		std::sort(results, results + numFound, SortSpatialArrayResults);
	}

	CPedTargetting* pPedTargeting = pPed->GetPedIntelligence()->GetTargetting(true);
	u32 iMaxNumTargets = pPedTargeting ? pPedTargeting->GetMaxNumTargets() : 0;
	u32 iNumThreatsRegistered = 0;

	for(u32 i = 0; i < numFound && iNumThreatsRegistered < iMaxNumTargets; i++)
	{
		const CPed* pTargetPed = CPed::GetPedFromSpatialArrayNode(results[i].m_Node);
		if (pTargetPed != pPed)
		{
			if( !pTargetPed->IsDead() &&
				( !pTargetPed->IsInjured() || pPed->WillAttackInjuredPeds() ) &&
				pPed->GetPedIntelligence()->IsThreatenedBy( *pTargetPed, true, false, true ) && 
				CanAttackPed(pPed, pTargetPed))
			{
				// Make sure our closest ped is set to the closest ped that isn't ourselves
				if(!pBestTargetPed)
				{
					pBestTargetPed = pTargetPed;
				}

				iNumThreatsRegistered++;
				pPed->GetPedIntelligence()->RegisterThreat( pTargetPed, true );
			}
		}
	}

	return pBestTargetPed;
}

// Need to make sure we are allowed to attack the target ped. Otherwise, it'll be infinite looping between this task and CTaskCombat.
bool CTaskCombatClosestTargetInArea::CanAttackPed(const CPed* pPed, const CPed* pTargetPed)
{
	if( pPed->IsLawEnforcementPed() && pTargetPed->IsPlayer() )
	{
		CWanted* pPlayerWanted = pTargetPed->GetPlayerWanted();
		if( pPlayerWanted )
		{
			if( pPlayerWanted->GetWantedLevel() == WANTED_CLEAN )
			{
				if( pPed->PopTypeIsRandom() && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_CanAttackNonWantedPlayerAsLaw) && !pPed->IsExitingVehicle() )
				{
					return false;
				}
			}
		}
	}

	return pPed->GetPedIntelligence()->CanAttackPed(pTargetPed);
}

void CTaskCombatClosestTargetInArea::Combat_OnEnter()
{
	//Check if we are resuming the state.  Keep the sub-task, if possible.
	if(GetSubTask() && GetSubTask()->GetTaskType() == CTaskTypes::TASK_THREAT_RESPONSE)
	{
		return;
	}

	const CPed* pClosestPed = m_pInitialTarget ? m_pInitialTarget : SearchForNewTargets(GetPed());

	m_pInitialTarget = NULL;

	// If we found the nearest ped, combat them (we might end up fleeing here as part of threat response???)
	if (pClosestPed)
	{
		CTaskThreatResponse* pTask = rage_new CTaskThreatResponse(pClosestPed);
		pTask->GetConfigFlagsForCombat().SetAllFlags(m_uCombatConfigFlags);
		SetNewTask(pTask);
		m_pPreviousTarget = pClosestPed;
	}
}

CTask::FSM_Return CTaskCombatClosestTargetInArea::Combat_OnUpdate()
{
	// Need to check if no subtask because it might not get set in the enter func
	if (GetIsSubtaskFinished(CTaskTypes::TASK_THREAT_RESPONSE))
	{
		// Check for the enemy in the area again if the subtask is set.
		if(GetSubTask())
		{
			const CPed* pClosestPed = SearchForNewTargets(GetPed());
			// Don't try to fight the same target again if the sub combat task quits fighting him for some reason. Prevent the possible state machine infinite loop.
			if (pClosestPed && pClosestPed != m_pPreviousTarget)
			{
				CTaskThreatResponse* pTask = rage_new CTaskThreatResponse(pClosestPed);
				pTask->GetConfigFlagsForCombat().SetAllFlags(m_uCombatConfigFlags);
				SetNewTask(pTask);
				m_pPreviousTarget = pClosestPed;
			}
			else
			{
				SetState(State_Finish);
			}
		}
		else
		{
			SetState(State_Finish);
		}
	}
	return FSM_Continue;
}

#if !__FINAL
void CTaskCombatClosestTargetInArea::Debug() const
{
#if __BANK
	const CPed* pPed = GetPed();

	Vector3 vCentre = m_vPos;

	if( m_bGetPosFromPed )
	{
		vCentre = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	}

	grcDebugDraw::Circle(vCentre, m_fArea, Color_red, VEC3V_TO_VECTOR3(pPed->GetTransform().GetA()), VEC3V_TO_VECTOR3(pPed->GetTransform().GetB()));
	
	if (m_fCombatTimer > 0.0f)
	{
		char szText[128];
		formatf(szText, "Time Left: %.2f", (m_fCombatTimer - GetTimeRunning()));
		grcDebugDraw::Text(vCentre, Color_red, szText);
	}
#endif // __BANK
}

const char * CTaskCombatClosestTargetInArea::GetStaticStateName( s32 iState )
{
	taskAssert(iState >= State_Start && iState <= State_Finish);

	switch (iState)
	{
	case State_Start:				return "State_Start";
	case State_WaitForTarget:		return "State_WaitForTarget";
	case State_Combat:				return "State_Combat";
	case State_Finish:				return "State_Finish";
	default: taskAssert(0); 	
	}

	return "State_Invalid";
}
#endif // !__FINAL


//////////////////////////////////////////////////////
//		TASK COMBAT ADDITIONAL TASK					//
//////////////////////////////////////////////////////

IMPLEMENT_COMBAT_TASK_TUNABLES(CTaskCombatAdditionalTask, 0x73dace82)
CTaskCombatAdditionalTask::Tunables CTaskCombatAdditionalTask::ms_Tunables;
s32	   CTaskCombatAdditionalTask::ms_iTimeOfLastOrderClip			= 0;
bank_u32 CTaskCombatAdditionalTask::ms_iTimeBetweenOrderClips			= 3000;
bank_float  CTaskCombatAdditionalTask::ms_fDistanceToGiveOrders			= 15.0f;
bank_float  CTaskCombatAdditionalTask::ms_fTimeBetweenCommunicationClips	= 4.0f;
bank_float  CTaskCombatAdditionalTask::ms_fTimeToDoMoveCommunicationClips = 6.0f;

// On top of movement or whatever, plays clips or aims weapon and that kindof stuff
CTaskCombatAdditionalTask::CTaskCombatAdditionalTask(s32 iRunningFlags, const CPed* pTarget, const Vector3& vTarget, float fTimer)
:m_iRunningFlags(iRunningFlags),
m_pTargetPed(pTarget),
m_vTarget(vTarget),
m_fTimer(fTimer),
m_uLastGunShotWhizzedByTime(0),
m_bClearingCorner(false),
m_bLeavingCorner(false),
m_fMoveBlendRatioLerp(1.0f),
m_fOriginalMoveBlendRatio(-1.0f),
m_fBlockedLosTime(0.0f),
m_fCheckForRunningDirectlyTimer(0.0f)
{
	SetInternalTaskType(CTaskTypes::TASK_ADDITIONAL_COMBAT_TASK);

#if __BANK
	if(GetEntity())
	{
		if(CPed* pPed = GetPed())
		{
			if(!pPed->IsPlayer())
			{
				AI_LOG_WITH_ARGS("CTaskCombatAdditionalTask::CTaskCombatAdditionalTask() - Target has changed, target = %s, ped = %s, task = %p \n", pTarget ? pTarget->GetLogName() : "Null", pPed->GetLogName(), this);
			}
		}
	}
	else
	{
		AI_LOG_WITH_ARGS("CTaskCombatAdditionalTask::CTaskCombatAdditionalTask() - Target has changed, target = %s, task = %p \n", pTarget ? pTarget->GetLogName() : "Null", this);
	}
#endif
}

CTaskCombatAdditionalTask::~CTaskCombatAdditionalTask()
{
}

CTask::FSM_Return CTaskCombatAdditionalTask::ProcessPreFSM()
{
	if(!m_bClearingCorner || m_bLeavingCorner || GetState() != State_AimingOrFiring)
	{
		// If we need to be returning to our original blend ratio, then continue to do so
		if(m_fOriginalMoveBlendRatio >= 0.0f && m_fMoveBlendRatioLerp < 1.0f)
		{
			CTaskMoveFollowNavMesh* pNavMeshTask = (CTaskMoveFollowNavMesh*) GetPed()->GetPedIntelligence()->FindMovementTaskByType(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH);
			if(pNavMeshTask)
			{
				// We want to lerp from walk to our previous move blend ratio over a certain time
				m_fMoveBlendRatioLerp = rage::Min(m_fMoveBlendRatioLerp + (GetTimeStep() / ms_Tunables.m_fMoveBlendRatioLerpTime), 1.0f);
				rage::Lerp(MOVEBLENDRATIO_WALK, m_fOriginalMoveBlendRatio, m_fMoveBlendRatioLerp);
				pNavMeshTask->SetMoveBlendRatio(m_fOriginalMoveBlendRatio, true);
			}
		}
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskCombatAdditionalTask::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate(pPed);

		FSM_State(State_SwapWeapon)
			FSM_OnEnter
				SwapWeapon_OnEnter();
			FSM_OnUpdate
				return SwapWeapon_OnUpdate();

		FSM_State(State_AimingOrFiring)
			FSM_OnEnter
				AimingOrFiring_OnEnter(pPed);
			FSM_OnUpdate
				return AimingOrFiring_OnUpdate(pPed);	

		FSM_State(State_PlayingClips)
			FSM_OnEnter
				PlayingClips_OnEnter(pPed);
			FSM_OnUpdate
				return PlayingClips_OnUpdate(pPed);	

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskCombatAdditionalTask::Start_OnUpdate(CPed* pPed)
{
	// Make sure we start with our LOS not considered blocked. There are less cases where this looks bad than the opposite
	m_fBlockedLosTime = HasClearLos() ? 0.0f : FLT_MAX;

	//Ensure the weapon manager is valid.
	const CPedWeaponManager* pWeaponManager = pPed->GetWeaponManager();
	if(pWeaponManager && pWeaponManager->GetRequiresWeaponSwitch() && !pWeaponManager->GetCreateWeaponObjectWhenLoaded())
	{
		SetState(State_SwapWeapon);
	}
	else
	{
		SetState(GetDesiredTaskState(pPed, true));
	}

	return FSM_Continue;
}

void CTaskCombatAdditionalTask::SwapWeapon_OnEnter()
{
	//Create the task.
	CTask* pTask = rage_new CTaskSwapWeapon(SWAP_DRAW);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskCombatAdditionalTask::SwapWeapon_OnUpdate()
{
	if(GetIsSubtaskFinished(CTaskTypes::TASK_SWAP_WEAPON))
	{
		SetState(GetDesiredTaskState(GetPed(), true));
	}

	return FSM_Continue;
}

void CTaskCombatAdditionalTask::AimingOrFiring_OnEnter(CPed* pPed)
{
	m_bAimTargetChanged = false;
	pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_KeepWeaponHolsteredUnlessFired, false );
	pPed->SetIsStrafing(true);

	s32 iFlags = 0;
	if( (m_iRunningFlags & RT_AimAtSpecificPos) ||
		(m_iRunningFlags & RT_Fire) == 0 )
	{
		iFlags = CTaskShootAtTarget::Flag_aimOnly;
	}
	CAITarget target;
	GetDesiredTarget(target);

	SetNewTask(rage_new CTaskShootAtTarget(target, iFlags));
}

CTask::FSM_Return CTaskCombatAdditionalTask::AimingOrFiring_OnUpdate(CPed* pPed)
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetFlag(aiTaskFlags::RestartCurrentState);
		return FSM_Continue;
	}
	else if(m_pTargetPed &&	(m_iRunningFlags & RT_AllowAimFixupIfLosBlocked))
	{
		CTaskGun* pGunTask = static_cast<CTaskGun*>(FindSubTaskOfType(CTaskTypes::TASK_GUN));
		if(pGunTask)
		{
			bool bTargetChangedThisFrame = false;

			CPedTargetting* pPedTargetting = pPed->GetPedIntelligence()->GetTargetting(false);
			if(pPedTargetting && pPedTargetting->GetLosStatus( m_pTargetPed ) == Los_blocked)
			{
				// Make sure we are following a route
				CTaskMoveFollowNavMesh* pNavMeshTask = (CTaskMoveFollowNavMesh*) pPed->GetPedIntelligence()->FindMovementTaskByType(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH);
				if(pNavMeshTask && pNavMeshTask->IsFollowingNavMeshRoute() && m_bClearingCorner)
				{
					Vector3 vAimTarget(VEC3_ZERO);
					// If we have a nav mesh task we try to get the point ahead on the route
					if(!pNavMeshTask->GetPositionAheadOnRoute(pPed, 2.0f, vAimTarget))
					{
						vAimTarget = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) + (VEC3V_TO_VECTOR3(pPed->GetTransform().GetB()) * 2.5f);
					}
					pGunTask->SetTarget(CWeaponTarget(vAimTarget));
					m_bAimTargetChanged = true;
					bTargetChangedThisFrame = true;
				}
			}
				
			if(!bTargetChangedThisFrame && m_bAimTargetChanged)
			{
				m_bAimTargetChanged = false;

				CAITarget target;
				GetDesiredTarget(target);
				pGunTask->SetTarget(target);
			}
		}
	}

	ProcessState(pPed);

	return FSM_Continue;
}

void CTaskCombatAdditionalTask::AimingOrFiring_OnExit()
{
	m_bClearingCorner = false;
}

void CTaskCombatAdditionalTask::PlayingClips_OnEnter(CPed* pPed)
{
	CTaskAmbientClips* pTask = rage_new CTaskAmbientClips( CTaskAmbientClips::Flag_PlayIdleClips|CTaskAmbientClips::Flag_UseExtendedHeadLookDistance,
															CONDITIONALANIMSMGR.GetConditionalAnimsGroup("COMBAT") );

	if(pTask)
	{
		CAmbientLookAt* pLookAtHelper = pTask->CreateLookAt(pPed);
		if(pLookAtHelper)
		{
			CAITarget target;
			GetDesiredTarget(target);
			pLookAtHelper->SetSpecificLookAtTarget(target);
		}
	}

	SetNewTask(pTask);
}

CTask::FSM_Return CTaskCombatAdditionalTask::PlayingClips_OnUpdate(CPed* pPed)
{
	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(GetDesiredTaskState(pPed, true));
		return FSM_Continue;
	}
	ProcessState(pPed);
	return FSM_Continue;
}

void CTaskCombatAdditionalTask::SetTargetPed(const CPed* pNewTarget)
{
	m_pTargetPed = pNewTarget;
	m_fBlockedLosTime = HasClearLos() ? 0.0f : FLT_MAX;

#if __BANK
	if(CPed* pPed = GetPed())
	{
		if(!pPed->IsPlayer())
		{
			AI_LOG_WITH_ARGS("CTaskCombatAdditionalTask::SetTargetPed() - Target has changed, target = %s, ped = %s, task = %p \n", m_pTargetPed ? m_pTargetPed->GetLogName() : "Null", pPed->GetLogName(), this);
		}
	}
#endif

	CTask* pSubTask = GetSubTask();
	if(pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_SHOOT_AT_TARGET)
	{
		// Set the new target in our shoot at target sub task
		CAITarget newTarget(m_pTargetPed);
		static_cast<CTaskShootAtTarget*>(pSubTask)->SetTarget(newTarget);
	}
}

void CTaskCombatAdditionalTask::OnGunShotWhizzedBy(const CEventGunShotWhizzedBy& rEvent)
{
	if(rEvent.GetEventType() == EVENT_SHOT_FIRED_WHIZZED_BY)
	{
		m_uLastGunShotWhizzedByTime = fwTimer::GetTimeInMilliseconds();
	}
}

bool CTaskCombatAdditionalTask::IsStill() const
{
	Vector2 vCurrentMbr;
	GetPed()->GetMotionData()->GetCurrentMoveBlendRatio(vCurrentMbr);
	if(vCurrentMbr.IsZero())
	{
		return true;
	}

	float fSpeedSq = GetPed()->GetVelocity().Mag2();
	static dev_float s_fMaxSpeed = 0.1f;
	float fMaxSpeedSq = square(s_fMaxSpeed);
	if(fSpeedSq < fMaxSpeedSq)
	{
		return true;
	}

	return false;
}

bool CTaskCombatAdditionalTask::ShouldClearCorner(bool bWasClearingCorner)
{
	CPed* pPed = GetPed();
	m_bClearingCorner = bWasClearingCorner;

	CTaskMoveFollowNavMesh* pNavMeshTask = (CTaskMoveFollowNavMesh*) pPed->GetPedIntelligence()->FindMovementTaskByType(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH);
	if(pNavMeshTask && pNavMeshTask->IsFollowingNavMeshRoute() && m_fBlockedLosTime >= ms_Tunables.m_fBlockedLosAimTime && m_iRunningFlags & RT_AllowAimFixupIfLosBlocked)
	{
		if(!m_bClearingCorner)
		{
			Vector3 vStartPos, vCornerPosition, vBeyondCornerPosition;
			const bool bCheckWalkingIntoInterior = false;
			// Check if we should be clearing any upcoming corner
			if(CTaskInvestigate::ShouldPedClearCorner(pPed, pNavMeshTask, m_vLastCornerCleared, vStartPos, vCornerPosition, vBeyondCornerPosition, bCheckWalkingIntoInterior))
			{
				m_bLeavingCorner = false;
				m_bClearingCorner = true;

				m_fOriginalMoveBlendRatio = pNavMeshTask->GetMoveBlendRatio();
				pNavMeshTask->SetMoveBlendRatio(MOVEBLENDRATIO_WALK, true);
				m_fMoveBlendRatioLerp = 0.0f;
			}
		}
		else
		{
			// Stop clearing a corner if we get close enough to the corner
			ScalarV scPedDistanceToCornerSq = DistSquared(VECTOR3_TO_VEC3V(m_vLastCornerCleared), pPed->GetTransform().GetPosition());
			ScalarV scMinDistToClearCornerSq = ScalarVFromF32(rage::square(ms_Tunables.m_fMinDistanceToClearCorner));
			if(IsLessThanAll(scPedDistanceToCornerSq, scMinDistToClearCornerSq))
			{
				m_bLeavingCorner = true;
			}

			// Being too far from a corner is also reason to stop clearing, as we should be getting closer not further
			ScalarV scMaxDistanceFromCorner = ScalarVFromF32(rage::square( m_bLeavingCorner ? ms_Tunables.m_fMaxLeavingCornerDistance : ms_Tunables.m_fMaxDistanceFromCorner));
			if(IsGreaterThanAll(scPedDistanceToCornerSq, scMaxDistanceFromCorner))
			{
				m_bClearingCorner = false;
			}
		}
	}
	else
	{
		m_bClearingCorner = false;
	}

	return m_bClearingCorner;
}

bool CTaskCombatAdditionalTask::ShouldAimOrFireDynamically()
{
	s32 iState = GetState();

	// If we're not in the start state then we should check our nav path
	if(iState != State_Start && iState != State_SwapWeapon)
	{
		CPed* pPed = GetPed();
		CTaskMoveFollowNavMesh* pNavMeshTask = (CTaskMoveFollowNavMesh*) pPed->GetPedIntelligence()->FindMovementTaskByType(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH);
		if(pNavMeshTask)
		{
			// If we are too close to our navigation target then we don't want to allow state changes (it looks bad)
			Vector3 vNavTarget = pNavMeshTask->GetTarget();
			ScalarV scDistToNavTargetSq = DistSquared(pPed->GetTransform().GetPosition(), VECTOR3_TO_VEC3V(vNavTarget));
			float fAdditionalDistance = iState == State_AimingOrFiring ? 2.0f : 1.0f;
			ScalarV scMinDistToNavTargetSq = ScalarVFromF32(CTaskEnterCover::ms_Tunables.m_CoverEntryMaxDistanceAI + fAdditionalDistance);
			scMinDistToNavTargetSq = (scMinDistToNavTargetSq * scMinDistToNavTargetSq);
			if(IsLessThanAll(scDistToNavTargetSq, scMinDistToNavTargetSq))
			{
				return true;
			}
		}
	}

	if(!m_pTargetPed)
	{
		// If we can't change state just set our should aim/fire based on our current state
		return (iState == State_AimingOrFiring);
	}

	CPed* pPed = GetPed();
	Vec3V vTargetToPed = pPed->GetTransform().GetPosition() - m_pTargetPed->GetTransform().GetPosition();
	ScalarV scDistToTargetSq = MagSquared(vTargetToPed);

	if(m_fCheckForRunningDirectlyTimer < 0.0f)
	{
		// We won't reset this until we can switch states as it's more important for after that is true (since we don't want to run every frame)
		m_fCheckForRunningDirectlyTimer = CTaskCombatAdditionalTask::ms_Tunables.m_fMinTimeBetweenRunDirectlyChecks;

		ScalarV scDistForAimingSq = ScalarVFromF32(GetState() == State_AimingOrFiring ? ms_Tunables.m_fStopAimingDistance : ms_Tunables.m_fStartAimingDistance);
		scDistForAimingSq = (scDistForAimingSq * scDistForAimingSq);
		if(IsGreaterThanAll(scDistToTargetSq, scDistForAimingSq))
		{
			// We're going to go through our combat director peds and check if we have another ped who is closer and already firing at our target
			CCombatDirector* pCombatDirector = pPed->GetPedIntelligence()->GetCombatDirector();
			if(pCombatDirector && pCombatDirector->GetNumPeds() > 0)
			{
				ScalarV scMaxAngleDiff = ScalarVFromF32(HALF_PI);
				ScalarV scPedToTargetVarianceSq = ScalarVFromF32(ms_Tunables.m_fMinOtherPedDistanceDiff);
				scPedToTargetVarianceSq = (scPedToTargetVarianceSq * scPedToTargetVarianceSq);
				vTargetToPed = NormalizeFastSafe(vTargetToPed, Vec3V(V_ZERO));

				int iNumPeds = pCombatDirector->GetNumPeds();
				for(int i = 0; i < iNumPeds; i++)
				{
					CPed* pOtherPed = pCombatDirector->GetPed(i);
					if(!pOtherPed || pOtherPed == pPed)
					{
						continue;
					}

					// We only care if the other ped is also aiming at our target
					if( !pOtherPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT) ||
						pOtherPed->GetPedIntelligence()->GetQueriableInterface()->GetTargetForTaskType(CTaskTypes::TASK_COMBAT) != m_pTargetPed.Get() ||
						pOtherPed->GetPedIntelligence()->GetQueriableInterface()->GetStateForTaskType(CTaskTypes::TASK_COMBAT) == CTaskCombat::State_InCover )
					{
						continue;
					}

					// The other ped has to actually be aiming (rather than being hidden in cover, running to a location, etc)
					const CTaskMotionPed* pMotionTask = static_cast<CTaskMotionPed *>(pOtherPed->GetPedIntelligence()->GetTaskManager()->FindTaskByTypeActive(PED_TASK_TREE_MOTION, CTaskTypes::TASK_MOTION_PED));
					if(!pMotionTask || !pMotionTask->IsAiming())
					{
						continue;
					}

					// The other ped must be within a certain distance of the target relative to us, depending on our current state
					Vec3V vTargetToOtherPed = pOtherPed->GetTransform().GetPosition() - m_pTargetPed->GetTransform().GetPosition();
					ScalarV scOtherPedDistToTargetSq = MagSquared(vTargetToOtherPed);
					if(IsGreaterThanOrEqualAll(scOtherPedDistToTargetSq, scDistToTargetSq))
					{
						continue;
					}

					if(iState != State_PlayingClips)
					{
						// This is some crazy math that tells us if our target is less than a certain distance close to our target than we are (avoiding square roots)
						ScalarV scOtherPedWithVariance = ScalarV(V_FOUR) * scOtherPedDistToTargetSq * scPedToTargetVarianceSq;
						ScalarV scPedToOtherDifference = scDistToTargetSq - scOtherPedDistToTargetSq - scPedToTargetVarianceSq;
						scPedToOtherDifference = (scPedToOtherDifference * scPedToOtherDifference);
						if(IsLessThanOrEqualAll(scPedToOtherDifference, scOtherPedWithVariance))
						{
							continue;
						}
					}

					// Lastly, the angle difference from the target to each ped must be within a min amount
					vTargetToOtherPed = NormalizeFastSafe(vTargetToOtherPed, Vec3V(V_ZERO));
					ScalarV scAngleDiff = Angle(vTargetToPed, vTargetToOtherPed);
					if(IsLessThanAll(scAngleDiff, scMaxAngleDiff))
					{
						return false;
					}
				}
			}
		}
	}

	if(CTaskCombat::ms_bEnableStrafeToRunBreakup)
	{
		ScalarV scForceStrafeDistSq = LoadScalar32IntoScalarV(ms_Tunables.m_fForceStrafeDistance);
		scForceStrafeDistSq = (scForceStrafeDistSq * scForceStrafeDistSq);
		if(iState == State_AimingOrFiring)
		{
			return GetTimeInState() < CTaskCombatAdditionalTask::ms_Tunables.m_fMaxTimeStrafing || IsLessThanOrEqualAll(scDistToTargetSq, scForceStrafeDistSq);
		}
		else if(iState == State_PlayingClips)
		{
			return GetTimeInState() >= CTaskCombatAdditionalTask::ms_Tunables.m_fMinTimeRunning || IsLessThanOrEqualAll(scDistToTargetSq, scForceStrafeDistSq);
		}
	}

	return true;
}

bool CTaskCombatAdditionalTask::ShouldAimOrFire()
{
	CPed* pPed = GetPed();
	CPedIntelligence* pPedIntelligence = pPed->GetPedIntelligence();
	float fStrafeChance = pPedIntelligence->GetCombatBehaviour().GetCombatFloat(kAttribFloatStrafeWhenMovingChance);

	if(fStrafeChance <= 0.0f)
	{
		return false;
	}

	if(m_iRunningFlags & RT_AllowAimFireToggle)
	{
		if(IsStill())
		{
			return true;
		}

		if(pPedIntelligence->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanUseDynamicStrafeDecisions))
		{
			if(fwTimer::GetTimeInMilliseconds() - m_uLastGunShotWhizzedByTime < CTaskCombatAdditionalTask::ms_Tunables.m_iBulletEventResponseLengthMs)
			{
				return true;
			}
		}

		if( !(m_iRunningFlags & RT_MakeDynamicAimFireDecisions) ||
			!pPedIntelligence->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanUseDynamicStrafeDecisions) )
		{
			if(fStrafeChance >= 1.0f)
			{
				return true;
			}
		}
		else
		{
			return ShouldAimOrFireDynamically();
		}
	}

	if(m_iRunningFlags & RT_Aim || m_iRunningFlags & RT_Fire)
	{
		return true;
	}

	return false;
}

bool CTaskCombatAdditionalTask::ShouldAimOrFireAtTarget(bool bWasClearingCorner)
{
	CPedTargetting* pPedTargetting = GetPed()->GetPedIntelligence()->GetTargetting(true);
	if( pPedTargetting && !GetPed()->GetPedIntelligence()->IsFriendlyWith(*m_pTargetPed) )
	{
		pPedTargetting->RegisterThreat(m_pTargetPed, false);

		const int iTargetIndex = pPedTargetting->FindTargetIndex(m_pTargetPed);
		if (iTargetIndex == -1)
		{
			return false;
		}

		if(pPedTargetting->GetLosStatusAtIndex( iTargetIndex ) == Los_blocked)
		{
			if(ShouldClearCorner(bWasClearingCorner))
			{
				return true;
			}

			if(GetPed()->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_RequiresLosToAim))
			{
				return false;
			}
		}

		if( pPedTargetting->AreTargetsWhereaboutsKnown( NULL, m_pTargetPed ) )
		{
			if (m_fBlockedLosTime < ms_Tunables.m_fBlockedLosAimTime) 
			{
				return true;
			}
			else if (IsStill())
			{
				const CSingleTargetInfo* pTargetInfo = pPedTargetting->GetTargetInfoAtIndex(iTargetIndex);
				return pTargetInfo && pTargetInfo->GetHasEverHadClearLos();
			}
		}
	}

	return false;
}

bool CTaskCombatAdditionalTask::HasClearLos()
{
	bool bClearLos = true; // We consider LOS is clear by default

	if(!m_pTargetPed)
	{
		return bClearLos;
	}

	CPedTargetting* pPedTargetting =  GetPed()->GetPedIntelligence()->GetTargetting(true);
	if(!pPedTargetting)
	{
		return bClearLos;
	}

	s32 iTargetIdx = pPedTargetting->FindTargetIndex(m_pTargetPed);
	if (iTargetIdx == -1)
	{
		return bClearLos;
	}

	const CSingleTargetInfo* pTargetInfo = pPedTargetting->GetTargetInfoAtIndex(iTargetIdx);
	if (!pTargetInfo)
	{
		return bClearLos;
	}

	bool bForceClearLos = (m_iRunningFlags & RT_AllowAimingOrFiringIfLosBlocked) && (pTargetInfo->GetHasEverHadClearLos() && (pTargetInfo->GetTimeLosBlocked() < ms_Tunables.m_fMaxLosBlockedTimeToForceClearLos));
	bClearLos = bForceClearLos || (pPedTargetting->GetLosStatusAtIndex(iTargetIdx, true) != Los_blocked);

	return bClearLos;
}


void CTaskCombatAdditionalTask::UpdateBlockedLosTime()
{
	if (HasClearLos())
	{
		m_fBlockedLosTime = 0.0f;
	}
	else
	{
		m_fBlockedLosTime += GetTimeStep();
	}
}

bool CTaskCombatAdditionalTask::CanSwitchStates()
{
	s32 iState = GetState();
	bool bCanSwitchState =  iState == State_Start || iState == State_SwapWeapon || GetTimeInState() >= CTaskCombatAdditionalTask::ms_Tunables.m_fMinTimeInState;
	if(!bCanSwitchState)
	{
		return false;
	}

	return true;
}

s32 CTaskCombatAdditionalTask::GetDesiredTaskState( CPed* pPed, bool UNUSED_PARAM(bTaskFinished) )
{
	// As there are cases where we can end up in aiming or firing without checking for corner clearing, we want to clear our variable
	// And set it again later if needed
	bool bWasClearingCorner = m_bClearingCorner;
	m_bClearingCorner = false;

	// Update some timers before possibly choosing a state
	m_fCheckForRunningDirectlyTimer -= GetTimeStep();
	UpdateBlockedLosTime();

	if(pPed->GetIsSwimming())
	{
		return State_PlayingClips;
	}

	if( m_iRunningFlags & RT_AimAtSpecificPos )
	{
		return State_AimingOrFiring;
	}

	if(!CanSwitchStates())
	{
		return GetState();
	}

	if(ShouldAimOrFire())
	{
		CTaskGun* pGunTask = static_cast<CTaskGun*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_GUN));
		if( pPed->GetWeaponManager() && (pPed->GetWeaponManager()->GetIsArmedGunOrCanBeFiredLikeGun() || (pGunTask && pGunTask->GetIsSwitchingWeapon())))
		{
			if( m_pTargetPed )
			{
				return ShouldAimOrFireAtTarget(bWasClearingCorner) ? State_AimingOrFiring : State_PlayingClips;
			}
			else
			{
				return State_AimingOrFiring;
			}
		}
	}

	return State_PlayingClips;
}

 void CTaskCombatAdditionalTask::GetDesiredTarget(CAITarget& target)
{
	if( m_iRunningFlags & RT_AimAtSpecificPos )
	{
		target.SetPosition(m_vSpecificAimAtPosition);
	}
	else if(m_pTargetPed)
	{
		target.SetEntity(m_pTargetPed);
	}
	else
	{
		target.SetPosition(m_vTarget);
	}
}

void CTaskCombatAdditionalTask::ProcessState(CPed* pPed)
{
	//Grab the sub-task.
	CTask* pSubTask = GetSubTask();

	if( m_fTimer > 0.0f )
	{
		m_fTimer -= GetTimeStep();
		if( m_fTimer <= 0.0f )
		{
			if( !pSubTask || pSubTask->MakeAbortable( CTask::ABORT_PRIORITY_URGENT, NULL ) )
			{
				SetState(State_Finish);
				return;
			}
			// Try immediately next frame
			m_fTimer = 0.01f;
		}
	}
	s32 iState = GetDesiredTaskState(pPed, false);
	if( iState != GetState())
	{
		if( pSubTask && pSubTask->SupportsTerminationRequest() )
		{
			pSubTask->RequestTermination();
		}
		else if( !pSubTask || pSubTask->MakeAbortable( CTask::ABORT_PRIORITY_URGENT, NULL ) )
		{
			SetState(iState);
			return;
		}
	}

	if (GetState() == State_AimingOrFiring && !pPed->GetIsSkiing())
	{
		pPed->SetIsStrafing(true);
		if( m_pTargetPed )
		{
			pPed->SetDesiredHeading( VEC3V_TO_VECTOR3(m_pTargetPed->GetTransform().GetPosition()) );
		}
		else
		{
			pPed->SetDesiredHeading( m_vTarget );
		}
	}
	else
	{
		pPed->SetIsStrafing(false);
	}
}

bool CTaskCombatAdditionalTask::ShouldPedStopAndWaitForGroupMembers( CPed* UNUSED_PARAM(pPed))
{
	return false;
	// 	CPedGroup* pPedsGroup = pPed->GetPedsGroup();
	// 	if( pPedsGroup && pPedsGroup->GetGroupMembership()->IsLeader(pPed) &&
	// 		pPed->GetPedType() == PEDTYPE_SWAT && pPed->PopTypeIsRandom()) 
	// 	{
	// 		if( pPedsGroup->GetGroupMembership()->CountMembersExcludingLeader() > 0 )
	// 		{
	// 			CPed* pFurthestMember = pPedsGroup->GetGroupMembership()->GetFurthestMemberFromPosition(pPed->GetPosition(), false);
	// 			if( pFurthestMember )
	// 			{
	// 				if( pFurthestMember->GetPosition().Dist2(pPed->GetPosition()) > rage::square(7.0f))
	// 				{
	// 					// Stop and wait up for lagging member
	// 					return true;
	// 				}
	// 			}
	// 		}
	// 	}
	// 	return false;
}


//fwMvClipId CTaskCombatAdditionalTask::GetSignalDirectionFromMovementTask( CPed* pPed )
//{
//	CTaskMoveFollowNavMesh* pNavMeshTask = (CTaskMoveFollowNavMesh*) pPed->GetPedIntelligence()->FindMovementTaskByType(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH);
//	if( pNavMeshTask )
//	{
//		Vector3 vStartPos, vCornerPosition, vBeyondCornerPosition;
//		if( pNavMeshTask &&
//			pNavMeshTask->GetThisWaypointLine( pPed, vStartPos, vCornerPosition, 0 ) &&
//			pNavMeshTask->GetThisWaypointLine( pPed, vCornerPosition, vBeyondCornerPosition, 1 ) )
//		{
//			static bool DEBUG_MOVE_COMMS = false;
//			if( DEBUG_MOVE_COMMS )
//			{
//				CVectorMap::DrawLine(vStartPos, vCornerPosition, Color_orange, Color_orange );
//				CVectorMap::DrawLine(vCornerPosition, vBeyondCornerPosition, Color_orange, Color_orange );
//			}
//
//			if(	VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()).Dist2(vCornerPosition) < rage::square(2.0f) )
//			{
//				Vector3 vLength1 = vCornerPosition - vStartPos;
//				Vector3 vLength2 = vBeyondCornerPosition - vCornerPosition;
//				if( vLength1.Dot(vLength2) < 0.98f )
//				{
//					// The ped is changing direction, work if its left or right
//					Vector3 vCross;
//					vCross.Cross( vLength1, ZAXIS );
//					if( vCross.Dot(vLength2) > 0.0f )
//					{
//						return CLIP_SIGNAL_GORIGHT;
//					}
//					else
//					{
//						return CLIP_SIGNAL_GOLEFT;
//					}
//				}
//			}
//		}
//	}
//
//	CTaskMoveFollowPointRoute* pPointRouteTask = (CTaskMoveFollowPointRoute*) pPed->GetPedIntelligence()->FindMovementTaskByType(CTaskTypes::TASK_MOVE_FOLLOW_POINT_ROUTE);
//	if( pPointRouteTask )
//	{
//		const s32 iProgress = pPointRouteTask->GetProgress();
//		const s32 iRouteSize = pPointRouteTask->GetRouteSize();
//		if( (iProgress + 1) < iRouteSize )
//		{
//			Vector3 vCurrentTarget = pPointRouteTask->GetTarget(iProgress);
//			Vector3 vNextTarget = pPointRouteTask->GetTarget(iProgress + 1);
//
//			const Vector3 vecPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
//			if(	vecPedPosition.Dist2(vCurrentTarget) < rage::square(2.0f) )
//			{
//				Vector3 vLength1 = vCurrentTarget - vecPedPosition;
//				Vector3 vLength2 = vNextTarget - vCurrentTarget;
//				if( vLength1.Dot(vLength2) < 0.98f )
//				{
//					// The ped is changing direction, work if its left or right
//					Vector3 vCross;
//					vCross.Cross( vLength1, ZAXIS );
//					if( vCross.Dot(vLength2) > 0.0f )
//					{
//						return CLIP_SIGNAL_GORIGHT;
//					}
//					else
//					{
//						return CLIP_SIGNAL_GOLEFT;
//					}
//				}
//			}
//		}
//	}
//	return CLIP_ID_INVALID;
//}

//fwMvClipId CTaskCombatAdditionalTask::GetOrderClip( CPed* pPed )
//{
//	if( (ms_iTimeOfLastOrderClip + ms_iTimeBetweenOrderClips) > fwTimer::GetTimeInMilliseconds() )
//	{
//		return CLIP_ID_INVALID;
//	}
//
//	const CEntityScannerIterator entityList = pPed->GetPedIntelligence()->GetNearbyPeds();
//	const Vector3 vecPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
//	for(const CEntity* pEnt = entityList.GetFirst(); pEnt; pEnt = entityList.GetNext())
//	{
//		const CPed* pThisPed = static_cast<const CPed*>(pEnt);
//
//		Vector3 vToPed = VEC3V_TO_VECTOR3(pThisPed->GetTransform().GetPosition()) - vecPedPosition;
//		float fDistToPedSq = vToPed.Mag2();
//		if( fDistToPedSq > rage::square(ms_fDistanceToGiveOrders) )
//		{
//			continue;
//		}
//		if( pThisPed->IsNetworkClone() )
//		{
//			continue;
//		}
//		vToPed.z =0.0f;
//		vToPed.Normalize();
//
//		// Get the ped's destination
//		CTask* pMoveTask = pThisPed->GetPedIntelligence()->GetGeneralMovementTask();
//		if( !pMoveTask )
//			continue;
//		CTaskMoveInterface* pMoveInterface = pMoveTask->GetMoveInterface();
//		if( !pMoveInterface )
//			continue;
//
//		if(pMoveInterface->GetMoveState() <= PEDMOVE_STILL || !pMoveInterface->HasTarget())
//			continue;
//
//		Vector3 vTarget = pMoveInterface->GetTarget();
//		Vector3 vToTarget = vTarget	- vecPedPosition;
//
//		vToTarget.Normalize();
//
//		Vector3 vRight;
//		vRight.Cross(vToTarget, ZAXIS);
//
//		float fSideDot = vToTarget.Dot(vRight);
//		float fFrontDot = DotProduct(vToTarget, VEC3V_TO_VECTOR3(pPed->GetTransform().GetB()));
//
//		if( ABS(fFrontDot) > ABS(fSideDot) )
//		{
//			if( fFrontDot > 0.0f )
//				return CLIP_SIGNAL_GOFWD;
//			else
//				return CLIP_SIGNAL_GOBACK;
//		}
//		else
//		{
//			if( fSideDot > 0.0f )
//				return CLIP_SIGNAL_GORIGHT;
//			else
//				return CLIP_SIGNAL_GOLEFT;
//		}
//	}
//
//	// Go through nearby peds and fake order them about
//	return CLIP_ID_INVALID;
//}

#if !__FINAL
const char * CTaskCombatAdditionalTask::GetStaticStateName( s32 iState )
{
	switch (iState)
	{
		case State_Start:						return "State_Start";
		case State_SwapWeapon:					return "State_SwapWeapon";
		case State_AimingOrFiring:				return "State_AimingOrFiring";
		case State_PlayingClips:				return "State_PlayingAnims";
		case State_Finish:						return "State_Finish";
		default: taskAssertf(0,"Unhandled state name");
	}

	return "State_Invalid";
}
#endif

//////////////////////////////////////
//		TASK COMBAT FLANK			//
//////////////////////////////////////

#define MAX_SPHERES_OF_INFLUENCE 8

IMPLEMENT_COMBAT_TASK_TUNABLES(CTaskCombatFlank, 0xa10b62e9)
CTaskCombatFlank::Tunables CTaskCombatFlank::sm_Tunables;

CTaskCombatFlank::CTaskCombatFlank(const CPed* pPrimaryTarget)
: m_pPrimaryTarget(pPrimaryTarget)
{
	SetInternalTaskType(CTaskTypes::TASK_COMBAT_FLANK);
}

CTaskCombatFlank::CTaskCombatFlank()
: m_pPrimaryTarget(NULL)
{
	SetInternalTaskType(CTaskTypes::TASK_COMBAT_FLANK);
}

CTaskCombatFlank::~CTaskCombatFlank()
{
}

CTask::FSM_Return CTaskCombatFlank::ProcessPreFSM()
{
	// Abort if the target is invalid
	if (!m_pPrimaryTarget)
	{
		return FSM_Quit;
	}

	// There is an absolute minimum distance we want to be from the target once we've entered this task
	// This is much smaller than the influence spheres for the path and is just to make sure that we don't get too close to our target once moving
	Vec3V vTargetToPed = GetPed()->GetTransform().GetPosition() - m_pPrimaryTarget->GetTransform().GetPosition();
	ScalarV scAbsoluteMinDistanceToTargetSq = ScalarVFromF32(sm_Tunables.m_fAbsoluteMinDistanceToTarget);
	scAbsoluteMinDistanceToTargetSq = square(scAbsoluteMinDistanceToTargetSq);
	if(IsLessThanAll(MagSquared(vTargetToPed), scAbsoluteMinDistanceToTargetSq))
	{
		return FSM_Quit;
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskCombatFlank::UpdateFSM( s32 iState, FSM_Event iEvent )
{
	FSM_Begin

		FSM_State(State_MoveToFlankPosition)
			FSM_OnEnter
				MoveToFlankPosition_OnEnter();
			FSM_OnUpdate
				return MoveToFlankPosition_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

void CTaskCombatFlank::MoveToFlankPosition_OnEnter()
{
	// We'll try to find a position that is closer than our max, which we shrink a bit to give the target
	// some room to move without us immediately going into move within attack window once the flank is done
	float fAttackWindowMin;
	float fAttackWindowMax;
	CTaskMoveWithinAttackWindow::GetAttackWindowMinMax(GetPed(), fAttackWindowMin, fAttackWindowMax, false);
	m_fAttackWindowMaxSq = rage::square(fAttackWindowMax * .9f);

	// Check if we are resuming the state.  Keep the sub-task, if possible.
	if(GetSubTask() && (GetSubTask()->GetTaskType() == CTaskTypes::TASK_MOVE_TO_TACTICAL_POINT))
	{
		return;
	}

	// Create the task
	fwFlags16 uTacticalFlags;
	uTacticalFlags.SetFlag(CTaskMoveToTacticalPoint::CF_DontCheckAttackWindow);
	uTacticalFlags.SetFlag(CTaskMoveToTacticalPoint::CF_DontCheckDefensiveArea);
	uTacticalFlags.SetFlag(CTaskMoveToTacticalPoint::CF_OnlyCheckInfluenceSpheresOnce);
	uTacticalFlags.SetFlag(CTaskMoveToTacticalPoint::CF_RejectNegativeScores);

	CTaskMoveToTacticalPoint* pTask = rage_new CTaskMoveToTacticalPoint(CAITarget(m_pPrimaryTarget), uTacticalFlags);

	// Set the callbacks
	pTask->SetCanScoreCoverPointCallback(CTaskCombatFlank::CanScoreCoverPoint);
	pTask->SetCanScoreNavMeshPointCallback(CTaskCombatFlank::CanScoreNavMeshPoint);
	pTask->SetScoreCoverPointCallback(CTaskCombatFlank::ScoreCoverPoint);
	pTask->SetScoreNavMeshPointCallback(CTaskCombatFlank::ScoreNavMeshPoint);
	pTask->SetSpheresOfInfluenceBuilder(CTaskCombatFlank::SpheresOfInfluenceBuilder);

	// Set the sub-task to use during movement
	s32 iCombatRunningFlags = CTaskCombatAdditionalTask::RT_Default|CTaskCombatAdditionalTask::RT_AllowAimFixupIfLosBlocked|CTaskCombatAdditionalTask::RT_AllowAimFireToggle;
	CTaskCombatAdditionalTask* pCombatTask = rage_new CTaskCombatAdditionalTask(iCombatRunningFlags, m_pPrimaryTarget, VEC3V_TO_VECTOR3(m_pPrimaryTarget->GetTransform().GetPosition()));
	pTask->SetSubTaskToUseDuringMovement(pCombatTask);

	SetNewTask(pTask);
}

CTask::FSM_Return CTaskCombatFlank::MoveToFlankPosition_OnUpdate()
{
	if(GetIsSubtaskFinished(CTaskTypes::TASK_MOVE_TO_TACTICAL_POINT))
	{
		SetState(State_Finish);
		return FSM_Quit;
	}

	return FSM_Continue;
}

bool CTaskCombatFlank::BuildInfluenceSpheres( TInfluenceSphere* apSpheres, int &iNumSpheres, bool bUseRouteCheckRadius ) const
{
	const CPed* pPed = GetPed();

	// Verify our peds
	if(!pPed || !m_pPrimaryTarget)
	{
		return false;
	}

	// One on the targets position, and one towards us
	const Vector3 vecPrimaryTargetPosition = VEC3V_TO_VECTOR3(m_pPrimaryTarget->GetTransform().GetPosition());
	Vector3 vToTarget = vecPrimaryTargetPosition - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	vToTarget.z = 0.0f;
	vToTarget.Normalize();
	vToTarget.Scale(sm_Tunables.m_fDistanceBetweenInfluenceSpheres);

	float fInnerWeight = sm_Tunables.m_fInfluenceSphereInnerWeight * INFLUENCE_SPHERE_MULTIPLIER;
	float fOuterWeight = sm_Tunables.m_fInfluenceSphereOuterWeight * INFLUENCE_SPHERE_MULTIPLIER;

	iNumSpheres = 2;
	apSpheres[0].Init(vecPrimaryTargetPosition, bUseRouteCheckRadius ? sm_Tunables.m_fInfluenceSphereCheckRouteRadius : sm_Tunables.m_fInfluenceSphereRequestRadius, fInnerWeight, fOuterWeight );
	apSpheres[1].Init(vecPrimaryTargetPosition - vToTarget, sm_Tunables.m_fSmallInfluenceSphereRadius, fInnerWeight, fOuterWeight );

	return true;
}

// Needed for the follow nav mesh task, just call into the build influence spheres function
bool CTaskCombatFlank::SpheresOfInfluenceBuilder( TInfluenceSphere* apSpheres, int &iNumSpheres, const CPed* pPed )
{
	const CTaskCombatFlank* pFlankTask = static_cast<const CTaskCombatFlank*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COMBAT_FLANK));
	if( pFlankTask )
	{
		return pFlankTask->BuildInfluenceSpheres(apSpheres, iNumSpheres, false);
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
// Cover point and nav mesh point scoring functions
//////////////////////////////////////////////////////////////////////////

float CTaskCombatFlank::ScorePosition(const CTaskMoveToTacticalPoint* pTask, Vec3V_In vPosition)
{
	// Make sure we have a task and a parent task to that
	if(!pTask || !pTask->GetParent())
	{
		return -SMALL_FLOAT;
	}

	// For now we require a target ped
	const CPed* pTargetPed = pTask->GetTargetPed();
	if(!pTargetPed)
	{
		return -SMALL_FLOAT;
	}

	// Grab the task.
	taskAssert(pTask->GetParent()->GetTaskType() == CTaskTypes::TASK_COMBAT_FLANK);
	const CTaskCombatFlank* pTaskFlank = static_cast<const CTaskCombatFlank *>(pTask->GetParent());

	Vec3V vTargetPosition = pTargetPed->GetTransform().GetPosition();
	const ScalarV scInfluenceSphereRadiusSq = ScalarVFromF32(square(sm_Tunables.m_fInfluenceSphereCheckRouteRadius));
	const ScalarV scAttackWindowMaxSq = ScalarVFromF32(pTaskFlank->GetAttackWindowMaxSq());

	// Ensure the position is not outside the attack window and not closer than the influence sphere radius
	const ScalarV scDistanceToTargetSq = DistSquared(vPosition, vTargetPosition);
	if( IsGreaterThanAll(scDistanceToTargetSq, scAttackWindowMaxSq) || IsLessThanAll(scDistanceToTargetSq, scInfluenceSphereRadiusSq) )
	{
		return -SMALL_FLOAT;
	}

	// Make sure the point is within our desired dot product (generally on the other side of the target)
	Vec3V vTargetToPosition = NormalizeFastSafe(vPosition - vTargetPosition, Vec3V(V_ZERO));
	Vec3V vTargetToPed = NormalizeFastSafe(pTask->GetPed()->GetTransform().GetPosition() - vTargetPosition, Vec3V(V_ZERO));
	ScalarV scDot = Dot(vTargetToPed, vTargetToPosition);
	if(IsGreaterThanAll(scDot, ScalarV(V_ZERO)))
	{
		return -SMALL_FLOAT;
	}

	return (scDot.Getf() * -1.0f);
}

bool CTaskCombatFlank::CanScoreCoverPoint(const CTaskMoveToTacticalPoint::CanScoreCoverPointInput& rInput)
{
	// Make sure the arc provides cover
	if(rInput.m_pPoint->DoesArcProvideCover())
	{
		return false;
	}

	// Ensure the point has line of sight
	if(!rInput.m_pPoint->IsLineOfSightClear())
	{
		return false;
	}

	// Make sure it's not occupied
	if(rInput.m_pPoint->HasNearby() && !rInput.m_pPoint->IsNearby(rInput.m_pTask->GetPed()))
	{
		return false;
	}

	return true;
}

float CTaskCombatFlank::ScoreCoverPoint(const CTaskMoveToTacticalPoint::ScoreCoverPointInput& rInput)
{
	Vec3V vCoverPosition;
	if(!rInput.m_pPoint->GetCoverPoint()->GetCoverPointPosition(RC_VECTOR3(vCoverPosition)))
	{
		return -SMALL_FLOAT;
	}

	return (ScorePosition(rInput.m_pTask, vCoverPosition) * sm_Tunables.m_fCoverPointScoreMultiplier);
}

bool CTaskCombatFlank::CanScoreNavMeshPoint(const CTaskMoveToTacticalPoint::CanScoreNavMeshPointInput& rInput)
{
	// Ensure the point has line of sight
	if(!rInput.m_pPoint->IsLineOfSightClear())
	{
		return false;
	}

	// Make sure it's not occupied
	if(rInput.m_pPoint->HasNearby() && !rInput.m_pPoint->IsNearby(rInput.m_pTask->GetPed()))
	{
		return false;
	}

	return true;
}

float CTaskCombatFlank::ScoreNavMeshPoint(const CTaskMoveToTacticalPoint::ScoreNavMeshPointInput& rInput)
{
	Vec3V vNavMeshPosition = rInput.m_pPoint->GetPosition();

	return ScorePosition(rInput.m_pTask, vNavMeshPosition);
}

//////////////////////////////////////////////////////////////////////////

#if !__FINAL
const char * CTaskCombatFlank::GetStaticStateName( s32 iState )
{
	taskAssert(iState >= State_MoveToFlankPosition && iState <= State_Finish);

	switch (iState)
	{
		case State_MoveToFlankPosition:			return "State_MoveToFlankPosition";
		case State_Finish:						return "State_Finish";
		default: taskAssert(0); 	
	}
		
	return "State_Invalid";
}

void CTaskCombatFlank::Debug() const
{
#if DEBUG_DRAW
	// Because we are trying to flank, the important thing is to avoid the influence spheres so we need to build them again and test against the route
	TInfluenceSphere aSpheres[MAX_SPHERES_OF_INFLUENCE];
	s32 iNumSpheres = 0;

	if(BuildInfluenceSpheres(aSpheres, iNumSpheres, true))
	{
		grcDebugDraw::Sphere(aSpheres[0].GetOrigin(), aSpheres[0].GetRadius(), Color_red, false);
	}
#endif
}

#endif // !__FINAL

//////////////////////////////////////////
//		TASK HELICOPTER STRAFE		    //
//////////////////////////////////////////

CTaskHelicopterStrafe::Tunables CTaskHelicopterStrafe::sm_Tunables;

IMPLEMENT_COMBAT_TASK_TUNABLES(CTaskHelicopterStrafe, 0xe0a360c8);

CTaskHelicopterStrafe::CTaskHelicopterStrafe( const CEntity* pTarget )
: m_pTarget(pTarget)
, m_vAvoidOffset(0.0f, 0.0f, 0.0f)
, m_fStrafeDirection(1.0f)
, m_fTimeSinceLastStrafeDirectionChange(0.0f)
, m_uRunningFlags(0)
, m_TimerCirclingFast(5.0f)
, m_TimerCirclingSlow(10.0f, 30.0f)
{
	m_vTargetVehicleVelocityXY.Zero();
	m_vTargetOffset.Zero();
	SetInternalTaskType(CTaskTypes::TASK_HELICOPTER_STRAFE);
}

//
//
//
CTaskHelicopterStrafe::~CTaskHelicopterStrafe()
{
}

CTask::FSM_Return CTaskHelicopterStrafe::ProcessPreFSM()
{
	//Ensure the heli is valid.
	if(!GetHeli())
	{
		return FSM_Quit;
	}
	
	//Process the avoidance.
	ProcessAvoidance();
	
	//Process the avoidance offset.
	ProcessAvoidanceOffset();
	
	//Process the strafe direction.
	ProcessStrafeDirection();
	
	return FSM_Continue;
}

//
//
//
CTask::FSM_Return CTaskHelicopterStrafe::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
	FSM_State(State_Start)
		FSM_OnUpdate
			return Initial_OnUpdate(pPed);
	FSM_State(State_HelicopterStrafe)
		FSM_OnEnter
			return Flying_OnEnter(pPed);
		FSM_OnUpdate
			return Flying_OnUpdate(pPed);
	FSM_End
}

//
//
//
CTask::FSM_Return CTaskHelicopterStrafe::Initial_OnUpdate(CPed* pPed)
{
	if( m_pTarget == (CEntity*)NULL || pPed->GetVehiclePedInside() == NULL || pPed->GetVehiclePedInside()->GetVehicleType() != VEHICLE_TYPE_HELI )
		return FSM_Quit;

	SetState(State_HelicopterStrafe);
	
	//Grab the position.
	Vec3V vPosition = pPed->GetTransform().GetPosition();

	//Grab the target position.
	Vec3V vTargetPosition = m_pTarget->GetTransform().GetPosition();

	//Generate the direction from the target to the ped.
	Vec3V vTargetToPed = Subtract(vPosition, vTargetPosition);
	Vec3V vTargetToPedDirection = NormalizeFastSafe(vTargetToPed, Vec3V(V_ZERO));
	
	ScalarV fDistance = Mag(vTargetToPed);
	ScalarV fOffset = Max(fDistance, ScalarVFromF32(2 * sm_Tunables.m_TargetOffset));
	m_vTargetOffset = VEC3V_TO_VECTOR3(vTargetToPedDirection * fOffset);
	m_vTargetOffset.z -= sm_Tunables.m_FlightHeightAboveTarget;

	return FSM_Continue;
}

//
//
//
CTask::FSM_Return CTaskHelicopterStrafe::Flying_OnEnter(CPed* pPed)
{
	if( m_pTarget == (CEntity*)NULL || pPed->GetVehiclePedInside() == NULL || pPed->GetVehiclePedInside()->GetVehicleType() != VEHICLE_TYPE_HELI )
		return FSM_Quit;

	m_TimerCirclingSlow.Reset();
	m_TimerCirclingFast.Reset();

	taskDisplayf("Created next helicopter strafe sub task");
	
	//Add the target/avoid offsets to the target to get the adjusted target position.
	Vector3 vAdjustedTarget = VEC3V_TO_VECTOR3(m_pTarget->GetTransform().GetPosition()) + m_vTargetOffset + m_vAvoidOffset;
	vAdjustedTarget.z += sm_Tunables.m_FlightHeightAboveTarget;

	//Create the heli task.
	sVehicleMissionParams params;
	params.m_fCruiseSpeed = GetCruiseSpeed();
	params.SetTargetPosition(vAdjustedTarget);
	CTaskVehicleGoToHelicopter* pHeliTask = rage_new CTaskVehicleGoToHelicopter(params, 0, -1.0f, sm_Tunables.m_MinHeightAboveTerrain);
	pHeliTask->SetDrivingFlag(DF_DontTerminateTaskWhenAchieved, true);
	
	//Create the task.
	CTask* pTask = rage_new CTaskControlVehicle(pPed->GetMyVehicle(), pHeliTask);
	
	//Start the task.
	SetNewTask(pTask);
	
	return FSM_Continue;
}

//
//
//
CTask::FSM_Return CTaskHelicopterStrafe::Flying_OnUpdate(CPed* pPed)
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetFlag(aiTaskFlags::RestartCurrentState);
	}

	//from controlsubtask
	if( m_pTarget == (CEntity*)NULL || pPed->GetVehiclePedInside() == NULL || pPed->GetVehiclePedInside()->GetVehicleType() != VEHICLE_TYPE_HELI )
	{
		if( GetSubTask()->MakeAbortable( CTask::ABORT_PRIORITY_IMMEDIATE, NULL ) )
		{
			return FSM_Quit;
		}
		return FSM_Continue;
	}
	CHeli* pHeli = static_cast<CHeli*>(pPed->GetVehiclePedInside());

	Assert(GetSubTask());

	//We dont' currently have any such assets - RAK 21/11/12
	/*if( m_pTarget->GetIsTypePed() )
	{
		const CPedWeaponManager* pWeaponManager = static_cast<const CPed*>(m_pTarget.Get())->GetWeaponManager();
		if( pWeaponManager && pWeaponManager->GetIsArmed() && fwRandom::GetRandomTrueFalse() )
		{
			pPed->NewSay("COP_HELI_MEGAPHONE_WEAPON", ATSTRINGHASH("M_Y_HELI_COP", 0x74FE01EA));
		}
		else
		{
			pPed->NewSay("COP_HELI_MEGAPHONE", ATSTRINGHASH("M_Y_HELI_COP", 0x74FE01EA));
		}
	}*/

	aiTask* pHeliTask = pHeli->GetIntelligence()->GetTaskManager()->FindTaskByTypeWithPriority(VEHICLE_TASK_TREE_PRIMARY, CTaskTypes::TASK_VEHICLE_GOTO_HELICOPTER, VEHICLE_TASK_PRIORITY_PRIMARY);

	if (pHeliTask)
	{
		Assert(dynamic_cast<CTaskVehicleGoToHelicopter*>(pHeliTask));//just to make sure this is the correct task
		CTaskVehicleGoToHelicopter *pHeliGoToTask = static_cast<CTaskVehicleGoToHelicopter*>(pHeliTask);
		
		if( pHeli->m_nVehicleFlags.bMoveAwayFromPlayer )
		{
			Vector3 vAwayFromPlayer = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - VEC3V_TO_VECTOR3(m_pTarget->GetTransform().GetPosition());
			vAwayFromPlayer.z = 0.0f;
			vAwayFromPlayer.Normalize();
			
			Vector3 targetPos;
			Vector3 vRetreatTarget = VEC3V_TO_VECTOR3(pPed->GetMyVehicle()->GetTransform().GetPosition()) + (vAwayFromPlayer*100.0f);
			targetPos.x = vRetreatTarget.x;
			targetPos.y = vRetreatTarget.y;
			targetPos.z = Clamp(vRetreatTarget.z, 5.0f, 110.0f);

			pHeliGoToTask->SetTargetPosition(&targetPos);
			pHeliGoToTask->SetMinHeightAboveTerrain(sm_Tunables.m_MinHeightAboveTerrain);
			pHeliGoToTask->SetCruiseSpeed(GetCruiseSpeed());
		}
		else
		{
			UpdateTargetPosAndSpeed(pHeliGoToTask, pHeli);
			UpdateOrientation(pHeliGoToTask, pHeli);
		}
	}

	return FSM_Continue;
}

///////////////////////////////////////////////////////////////////////////
void CTaskHelicopterStrafe::UpdateTargetPosAndSpeed(CTaskVehicleGoToHelicopter* pTask, const CHeli* pHeli)
{
	//Ensure the target is valid.
	const CEntity* pTarget = m_pTarget;
	if(!pTarget)
	{
		return;
	}

	UpdateTargetCollisionProbe(pHeli, *pTask->GetTargetPosition());
	
	//Grab the ped.
	const CPed* pPed = GetPed();
	
	//Grab the position.
	Vec3V vPosition = pPed->GetTransform().GetPosition();
	
	//Grab the target position.
	Vec3V vTargetPosition = pTarget->GetTransform().GetPosition();
	
	//Generate the direction from the target to the ped.
	Vec3V vTargetToPed = Subtract(vPosition, vTargetPosition);
	Vec3V vTargetToPedDirection = NormalizeFastSafe(vTargetToPed, Vec3V(V_ZERO));
	
	//Flatten the target to ped direction.
	Vec3V vTargetToPedDirectionFlat = vTargetToPedDirection;
	vTargetToPedDirectionFlat.SetZ(ScalarV(V_ZERO));
	vTargetToPedDirectionFlat = NormalizeFastSafe(vTargetToPedDirectionFlat, Vec3V(V_X_AXIS_WZERO));
	
	bool targetInAir = false;
	//Update the vehicle velocity.
	m_vTargetVehicleVelocityXY.Zero();
	float fTargetVehicleSpeedXY = 0.0f;
	if(pTarget->GetIsTypePed())
	{
		const CPed* pPed = static_cast<const CPed*>(pTarget);
		CVehicle* pVehicle = pPed->GetVehiclePedInside();
		if(pVehicle)
		{
			m_vTargetVehicleVelocityXY = pVehicle->GetVelocity();
			m_vTargetVehicleVelocityXY.z = 0.0f;
			fTargetVehicleSpeedXY = m_vTargetVehicleVelocityXY.Mag();

			if ( pVehicle->InheritsFromHeli() || pVehicle->InheritsFromPlane() 
				|| pVehicle->InheritsFromBlimp() || pVehicle->InheritsFromAutogyro() )
			{
				if ( const_cast<CVehicle*>(pVehicle)->IsInAir() )
				{
					targetInAir = true;
				}
			}
		}

		if ( pPed->GetPedResetFlag( CPED_RESET_FLAG_InsideEnclosedSearchRegion ) )
		{
			// treat like we're in air for computing strafe location
			// for garages this will hopefully get us closer to level with target
			targetInAir = true;
		}

	}
	
	//Keep track of the direction.
	Vec3V vDirection = vTargetToPedDirectionFlat;
	
	//Keep track of the angle look-ahead.
	float fAngleLookAhead = 0.0f;
	
	//If we are moving slow, we can strafe.
	const bool bMovingSlow = fTargetVehicleSpeedXY < sm_Tunables.m_TargetMinSpeedToIgnore;
	if( bMovingSlow )
	{
		//Check if we can see the target.
		if(m_uRunningFlags.IsFlagSet(RF_CanSeeTarget) && !m_TimerCirclingSlow.IsFinished() )
		{
			m_TimerCirclingSlow.Tick(GetTimeStep());
			m_TimerCirclingFast.Reset();
			
			//Check if we are behind the target.
			if(m_uRunningFlags.IsFlagSet(RF_IsBehindTarget))
			{
				//Set the angle look-ahead.
				fAngleLookAhead = sm_Tunables.m_BehindRotateAngleLookAhead;
			}
			else
			{
				//Set the angle look-ahead.
				fAngleLookAhead = sm_Tunables.m_CircleRotateAngleLookAhead;
			}
		}
		else
		{
			m_TimerCirclingFast.Tick(GetTimeStep());
			if ( m_TimerCirclingFast.IsFinished() )
			{
				m_TimerCirclingSlow.Reset();
			}	

			//Set the angle look-ahead.
			fAngleLookAhead = sm_Tunables.m_SearchRotateAngleLookAhead;
		}
	}

	if ( m_uRunningFlags.IsFlagSet(RF_IsColliding) )
	{
		fAngleLookAhead += 15.0f;
	}
	
	if ( fabsf(fAngleLookAhead) >= FLT_EPSILON )
	{
		//Apply the angle look-ahead.
		vDirection = RotateAboutZAxis(vDirection, ScalarVFromF32(m_fStrafeDirection * fAngleLookAhead * DtoR));
	}

	//Scale the direction to generate the target offset.
	Vec3V vTargetOffset = Scale(vDirection, ScalarVFromF32(sm_Tunables.m_TargetOffset));

	if (!bMovingSlow)
	{
		//Add the speed to the offset.
		vTargetOffset = Add(vTargetOffset, VECTOR3_TO_VEC3V(m_vTargetVehicleVelocityXY));
	}

	//Filter the target offset to the desired.
	float fLerp = Min(sm_Tunables.m_TargetOffsetFilter * GetTimeStep(), 1.0f);
	m_vTargetOffset.Lerp(fLerp, VEC3V_TO_VECTOR3(vTargetOffset));

	//Add the target/avoid offsets to the target to get the adjusted target position.
	Vector3 vAdjustedTarget = VEC3V_TO_VECTOR3(vTargetPosition) + m_vTargetOffset + m_vAvoidOffset;

	if ( !targetInAir )
	{
		//Add the flight height above the target.
		//It is necessary to pre-add the height to the target, because the mission code
		//will ramp the height based on the proximity to the target.
		vAdjustedTarget.z += sm_Tunables.m_FlightHeightAboveTarget;

		//If the heli avoidance height is higher than the target height, respect it.
		float fHeliAvoidanceHeight = pHeli->GetHeliIntelligence()->GetHeliAvoidanceHeight();
		vAdjustedTarget.z = Max(vAdjustedTarget.z, fHeliAvoidanceHeight);
	}
		
	pTask->SetTargetPosition(&vAdjustedTarget);
	pTask->SetMinHeightAboveTerrain((int)sm_Tunables.m_MinHeightAboveTerrain);
	pTask->SetCruiseSpeed(GetCruiseSpeed());
}

//////////////////////////////////////////////////////////////////////////
void CTaskHelicopterStrafe::UpdateTargetCollisionProbe(const CHeli* pHeli, const Vector3& in_Position)
{
	if ( m_targetLocationTestResults.GetResultsReady() )
	{
		if ( m_targetLocationTestResults[0].GetHitDetected() )
		{
			m_uRunningFlags.SetFlag(RF_IsColliding);
		}
		m_targetLocationTestResults.Reset();
	}

	if (!m_targetLocationTestResults.GetWaitingOnResults() )
	{
		WorldProbe::CShapeTestCapsuleDesc CapsuleTest;
		static float s_RadiusScalar = 1.25f;
        // offset the end point so swept sphere check works.  probably an early out issue
		CapsuleTest.SetCapsule(in_Position, in_Position + Vector3(0.1f, 0.1f, 0.1f) , pHeli->GetBoundRadius()*s_RadiusScalar);
		CapsuleTest.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
		CapsuleTest.SetExcludeInstance(pHeli->GetCurrentPhysicsInst());
		CapsuleTest.SetResultsStructure(&m_targetLocationTestResults);
		CapsuleTest.SetMaxNumResultsToUse(1);
	
		// currently only swept sphere supports async on both platforms
		static bool s_hackusesweptsphere = true;
		CapsuleTest.SetIsDirected(s_hackusesweptsphere);
		CapsuleTest.SetDoInitialSphereCheck(s_hackusesweptsphere);
		WorldProbe::GetShapeTestManager()->SubmitTest(CapsuleTest, WorldProbe::PERFORM_ASYNCHRONOUS_TEST);
	}

}

///////////////////////////////////////////////////////////////////////////
void CTaskHelicopterStrafe::UpdateOrientation(CTaskVehicleGoToHelicopter* pTask, const CHeli* pHeli)
{
	//Grab the ped.
	const CPed* pPed = GetPed();

	const bool bCloseEnoughToStrafe = ( DistSquared( pHeli->GetTransform().GetPosition(), VECTOR3_TO_VEC3V(*pTask->GetTargetPosition()) ) < ScalarVFromF32(square(30.0f)) ).Getb();
	const bool bTargetSlowEnoughToStrafe = m_vTargetVehicleVelocityXY.Mag2() < square(sm_Tunables.m_TargetMaxSpeedToStrafe);
	const bool bTimeInStateIsBeforeOrientation = GetTimeInState() <= 2.0f;
	const bool bCanSeeTarget = m_uRunningFlags.IsFlagSet(RF_CanSeeTarget);
		
	//Set a fixed heading if target is visible and moving slow enough.
	
	if ( ( bCanSeeTarget || bTimeInStateIsBeforeOrientation) 
		&& bTargetSlowEnoughToStrafe
		&& bCloseEnoughToStrafe )
	{
		const Vector3 vecPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		Vector3 vToTargetPos = VEC3V_TO_VECTOR3(m_pTarget->GetTransform().GetPosition()) - vecPedPosition;
		vToTargetPos.z = 0.0f;
		vToTargetPos.Normalize();
		float fHeadingToTarget = Atan2f(vToTargetPos.y, vToTargetPos.x);
		
		//Check which sides the passengers are on.
		bool bOnLeft = false;
		bool bOnRight = false;
		for( s32 iSeat = 0; iSeat < pPed->GetMyVehicle()->GetSeatManager()->GetMaxSeats(); iSeat++ )
		{
			if( pHeli->GetSeatManager()->GetPedInSeat(iSeat) && 
				!pHeli->GetSeatManager()->GetPedInSeat(iSeat)->IsInjured() &&
				iSeat != pHeli->GetDriverSeat() )
			{
				// Figure out which side of the aircraft the seat is on
				int iSeatBone = pHeli->GetVehicleModelInfo()->GetModelSeatInfo()->GetBoneIndexFromSeat(iSeat);
				if(iSeatBone > -1)
				{
					const crBoneData* pBoneData = pHeli->GetSkeletonData().GetBoneData(iSeatBone);
					Assert(pBoneData);
					bool bLeft = pBoneData->GetDefaultTranslation().GetXf() < 0.0f;

					if( bLeft )
					{
						bOnLeft = true;
					}
					else
					{
						bOnRight = true;
					}
				}
			}
		}
		
		//Check if we only have passengers on the left.
		float fDesiredHeading = fHeadingToTarget;
		if(bOnLeft && !bOnRight)
		{
			fDesiredHeading -= HALF_PI;
		}
		//Check if we only have passengers on the right.
		else if(!bOnLeft && bOnRight)
		{
			fDesiredHeading += HALF_PI;
		}
		else
		{
			//Check the strafe direction.
			if(m_fStrafeDirection < 0.0f)
			{
				fDesiredHeading += HALF_PI;
			}
			else
			{
				fDesiredHeading -= HALF_PI;
			}
		}

		const Vec3V vForward = pHeli->GetVehicleForwardDirection();
		const float fHeading = Atan2f(vForward.GetYf(), vForward.GetXf());

		TUNE_FLOAT(HELICOPTER_STRAFE_NOSE_FIRST_ROTATION_THRESHOLD, 135.0f, 90.0f, 180.0f, 0.5f);

		// Don't expose tail when changing strafe orientation
		if(Abs(SubtractAngleShorterFast(fDesiredHeading, fHeading)) > HELICOPTER_STRAFE_NOSE_FIRST_ROTATION_THRESHOLD * DtoR
			&& Abs(SubtractAngleShorterFast(fHeadingToTarget, fHeading)) >= HALF_PI)
		{
			fDesiredHeading = fHeadingToTarget;
		}

		pTask->SetOrientation(fDesiredHeading);
		pTask->SetOrientationRequested(true);
	}
	else
	{
		pTask->SetOrientation(0.0f);
		pTask->SetOrientationRequested(false);
	}
}

bool CTaskHelicopterStrafe::CanSeeTarget() const
{
	//Grab the ped.
	const CPed* pPed = GetPed();
	
	//Ensure the vehicle is valid.
	const CVehicle* pVehicle = pPed->GetVehiclePedInside();
	if(!pVehicle)
	{
		return false;
	}
	
	//Ensure the seat manager is valid.
	const CSeatManager* pSeatManager = pVehicle->GetSeatManager();
	if(!pSeatManager)
	{
		return false;
	}
	
	//Iterate over the seats.
	int iMaxSeats = pSeatManager->GetMaxSeats();
	for(int iSeat = 0; iSeat < iMaxSeats; ++iSeat)
	{
		//Ensure the ped in the seat is valid.
		const CPed* pPedInSeat = pSeatManager->GetPedInSeat(iSeat);
		if(!pPedInSeat)
		{
			continue;
		}
		
		//Check if the ped can see the target.
		CPedTargetting* pTargeting = pPedInSeat->GetPedIntelligence()->GetTargetting(true);
		if(pTargeting && pTargeting->GetLosStatus(m_pTarget) == Los_clear)
		{
			return true;
		}
	}
	
	return false;
}

float CTaskHelicopterStrafe::GetCruiseSpeed() const
{
	//Grab the ped.
	const CPed* pPed = GetPed();
	
	//Calculate the cruise speed.
	float fCruiseSpeed = 35.0f;
	fCruiseSpeed *= pPed->GetPedIntelligence()->GetCombatBehaviour().GetCombatFloat(kAttribFloatHeliSpeedModifier);
	
	return fCruiseSpeed;
}

CHeli* CTaskHelicopterStrafe::GetHeli()
{
	//Grab the ped.
	CPed* pPed = GetPed();

	//Ensure the ped is in a vehicle.
	CVehicle* pVehicle = pPed->GetVehiclePedInside();
	if(!pVehicle)
	{
		return NULL;
	}

	//Ensure the vehicle is a heli.
	if(!pVehicle->InheritsFromHeli())
	{
		return NULL;
	}
	
	return static_cast<CHeli *>(pVehicle);
}

bool CTaskHelicopterStrafe::IsBehindTarget() const
{
	//Ensure the target is valid.
	const CEntity* pTarget = m_pTarget;
	if(!pTarget)
	{
		return false;
	}
	
	//Grab the ped.
	const CPed* pPed = GetPed();
	
	//Grab the position.
	const Vec3V vPosition = pPed->GetTransform().GetPosition();

	//Grab the target position.
	const Vec3V vTargetPosition = pTarget->GetTransform().GetPosition();

	//Grab the target forward vector.
	Vec3V vTargetForward = pTarget->GetTransform().GetForward();

	//Calculate the desired direction.
	Vec3V vDesiredDirection = vTargetForward;
	if(const camThirdPersonCamera* pCamera = camInterface::GetGameplayDirector().GetThirdPersonCamera(pTarget))
	{
		//Use the orientation of the camera if one is attached.
		vDesiredDirection = VECTOR3_TO_VEC3V(pCamera->GetBaseFront());
	}

	//Flatten the desired direction.
	Vec3V vDesiredDirectionFlat = vDesiredDirection;
	vDesiredDirectionFlat.SetZ(ScalarV(V_ZERO));
	vDesiredDirectionFlat = NormalizeFastSafe(vDesiredDirectionFlat, vDesiredDirection);

	//Generate the direction from the target to the ped.
	Vec3V vTargetToPed = Subtract(vPosition, vTargetPosition);
	Vec3V vTargetToPedDirection = NormalizeFastSafe(vTargetToPed, vDesiredDirection);

	//Flatten the target to ped direction.
	Vec3V vTargetToPedDirectionFlat = vTargetToPedDirection;
	vTargetToPedDirectionFlat.SetZ(ScalarV(V_ZERO));
	vTargetToPedDirectionFlat = NormalizeFastSafe(vTargetToPedDirectionFlat, vTargetToPedDirection);

	//Check if we are behind the target.
	ScalarV scDot = Dot(vDesiredDirectionFlat, vTargetToPedDirectionFlat);
	ScalarV scMinDot = ScalarVFromF32(sm_Tunables.m_MinDotToBeConsideredInFront);
	if(IsGreaterThanAll(scDot, scMinDot))
	{
		return false;
	}
	else
	{
		return true;
	}
}

void CTaskHelicopterStrafe::ProcessAvoidance()
{
	//Grab the heli.
	CHeli* pHeli = GetHeli();
	
	//Note that the heli should process avoidance.
	pHeli->GetHeliIntelligence()->SetProcessAvoidance(true);
}

void CTaskHelicopterStrafe::ProcessAvoidanceOffset()
{
	//Ensure the target is valid.
	CEntity* pTarget = const_cast<CEntity*>(m_pTarget.Get());
	if(!pTarget)
	{
		return;
	}
	
	//Grab the heli.
	CHeli* pHeli = GetHeli();
	
	//Keep track of whether the heli should avoid the target.
	bool bAvoidTarget = false;
	
	//Check if the target last damaged the heli.
	if(pHeli->HasBeenDamagedByEntity(pTarget))
	{
		//Check if the damage time is within the threshold.
		if((pHeli->GetWeaponDamagedTimeByEntity(pTarget) + (sm_Tunables.m_TimeToAvoidTargetAfterDamaged * 1000.0f)) > fwTimer::GetTimeInMilliseconds())
		{
			bAvoidTarget = true;
		}
	}
	
	//Ensure the avoid state is changing.
	bool bAvoidingTarget = m_vAvoidOffset.IsNonZero();
	if(bAvoidTarget == bAvoidingTarget)
	{
		return;
	}
	
	//Check the avoiding state.
	if(bAvoidTarget)
	{
		//Grab the heli position.
		Vec3V vHeliPosition = pHeli->GetTransform().GetPosition();
		
		//Grab the target position.
		Vec3V vTargetPosition = pTarget->GetTransform().GetPosition();
		
		//Generate a vector from the target to the heli.
		Vec3V vTargetToHeli = Subtract(vHeliPosition, vTargetPosition);
		
		//Flatten the vector.
		vTargetToHeli.SetZ(ScalarV(V_ZERO));
		
		//Normalize the vector.
		vTargetToHeli = NormalizeFastSafe(vTargetToHeli, Vec3V(V_ZERO));
		
		//Assign the avoid offset.
		m_vAvoidOffset = VEC3V_TO_VECTOR3(vTargetToHeli);
		m_vAvoidOffset *= sm_Tunables.m_AvoidOffsetXY;
		m_vAvoidOffset.z = sm_Tunables.m_AvoidOffsetZ;
	}
	else
	{
		m_vAvoidOffset.Zero();
	}
}

void CTaskHelicopterStrafe::ProcessStrafeDirection()
{
	//Ensure the time has surpassed the threshold.
	m_fTimeSinceLastStrafeDirectionChange += GetTimeStep();
	if(m_fTimeSinceLastStrafeDirectionChange < sm_Tunables.m_MinTimeBetweenStrafeDirectionChanges)
	{
		return;
	}
	
	//Check if we could see the target last frame.
	bool bCouldSeeTargetLastFrame = m_uRunningFlags.IsFlagSet(RF_CanSeeTarget);
	
	//Check if we were behind the target last frame.
	bool bWasBehindTargetLastFrame = m_uRunningFlags.IsFlagSet(RF_IsBehindTarget);
	
	//Check if we can see the target this frame.
	bool bCanSeeTargetThisFrame = CanSeeTarget();
	
	//Check if we are behind the target this frame.
	bool bIsBehindTargetThisFrame = IsBehindTarget();
	
	bool bIsColliding = m_uRunningFlags.IsFlagSet(RF_IsColliding);

	// check if we are trying to move into collsion
	if (bIsColliding)
	{
		m_uRunningFlags.ClearFlag(RF_IsColliding);

		//Switch the strafe direction.
		m_fStrafeDirection = -m_fStrafeDirection;

		//Clear the timer.
		m_fTimeSinceLastStrafeDirectionChange = 0.0f;
	}
	//Check if we can see the target.
	else if(bCanSeeTargetThisFrame)
	{
		//Check if we are behind the target this frame, and we were not behind the target last frame.
		if(bIsBehindTargetThisFrame && !bWasBehindTargetLastFrame)
		{
			//Switch the strafe direction.
			m_fStrafeDirection = -m_fStrafeDirection;
			
			//Clear the timer.
			m_fTimeSinceLastStrafeDirectionChange = 0.0f;
		}
	}
	else
	{
		//Check if we could see the target last frame.
		if(bCouldSeeTargetLastFrame)
		{
			//Switch the strafe direction.
			m_fStrafeDirection = -m_fStrafeDirection;
			
			//Clear the timer.
			m_fTimeSinceLastStrafeDirectionChange = 0.0f;
		}
	}
	
	//Update the flags.
	m_uRunningFlags.ChangeFlag(RF_CanSeeTarget,	bCanSeeTargetThisFrame);
	m_uRunningFlags.ChangeFlag(RF_IsBehindTarget, bIsBehindTargetThisFrame);
}

#if !__FINAL
// return the name of the given task as a string
const char * CTaskHelicopterStrafe::GetStaticStateName( s32 iState )
{
	// Make sure FSM state is valid
	taskAssert(iState <= State_HelicopterStrafe);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_HelicopterStrafe"
	};

	return aStateNames[iState];
}
#endif //!__FINAL
