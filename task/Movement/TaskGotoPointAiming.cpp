// File header
#include "Task/Movement/TaskGoToPointAiming.h"

// Game headers
#include "Peds/PedIntelligence.h"
#include "Task/Combat/TaskNewCombat.h"
#include "Task/Motion/Locomotion/TaskMotionPed.h"
#include "Task/Movement/TaskSeekEntity.h"
#include "Task/Movement/TaskGoToPoint.h"
#include "Task/Weapons/Gun/TaskGun.h"
#include "Task/Weapons/Gun/TaskAimGun.h"
#include "Task/Weapons/WeaponController.h"

AI_OPTIMISATIONS()

static dev_float DEFAULT_TARGET_RADIUS      = 0.5f;
static dev_float DEFAULT_SLOW_DOWN_DISTANCE = 4.0f;

//////////////////////////////////////////////////////////////////////////
// CClonedGoToPointAimingInfo
//////////////////////////////////////////////////////////////////////////
CClonedGoToPointAimingInfo::CClonedGoToPointAimingInfo()
	: m_bHasGotoPosition(false)
	, m_bHasGotoEntity(false)
	, m_pGotoEntity(NULL)
	, m_vGotoPosition(Vector3::ZeroType)
	, m_bHasAimPosition(false)
	, m_bHasAimEntity(false)
	, m_pAimEntity(NULL)
	, m_vAimPosition(Vector3::ZeroType)
	, m_fMoveBlendRatio(0.0f)
	, m_fTargetRadius(DEFAULT_TARGET_RADIUS)
	, m_fSlowDownDistance(DEFAULT_SLOW_DOWN_DISTANCE)
	, m_bIsShooting(false)
	, m_bUseNavmesh(false)
	, m_iScriptNavFlags(0)
	, m_iConfigFlags(0)
	, m_bAimAtHatedEntitiesNearAimCoord(false)
	, m_bInstantBlendToAim(false)
{

}

CClonedGoToPointAimingInfo::CClonedGoToPointAimingInfo(const CAITarget& gotoTarget, 
													   const CAITarget& aimTarget, 
													   const float fMoveBlendRatio,
													   const float fTargetRadius,
													   const float fSlowDownDistance,
													   const bool bIsShooting,
													   const bool bUseNavmesh,
													   const u32 iScriptNavFlags,
													   const u32 iConfigFlags,
													   const bool bAimAtHatedEntitiesNearAimCoord,
													   const bool bInstantBlendToAim)
	: m_bHasGotoPosition(false)
	, m_bHasGotoEntity(false)
	, m_bHasAimPosition(false)
	, m_bHasAimEntity(false)
	, m_fMoveBlendRatio(fMoveBlendRatio)
	, m_fTargetRadius(fTargetRadius)
	, m_fSlowDownDistance(fSlowDownDistance)
	, m_bIsShooting(bIsShooting)
	, m_bUseNavmesh(bUseNavmesh)
	, m_iScriptNavFlags(iScriptNavFlags)
	, m_iConfigFlags(iConfigFlags)
	, m_bAimAtHatedEntitiesNearAimCoord(bAimAtHatedEntitiesNearAimCoord)
	, m_bInstantBlendToAim(bInstantBlendToAim)
{
	// setup goto
	s32 nGotoMode = gotoTarget.GetMode();
	if(nGotoMode == CAITarget::Mode_Entity || nGotoMode == CAITarget::Mode_EntityAndOffset)
	{
		m_bHasGotoEntity = true; 
		m_pGotoEntity.SetEntity((CEntity*)gotoTarget.GetEntity());
	}
	if(nGotoMode == CAITarget::Mode_WorldSpacePosition || nGotoMode == CAITarget::Mode_EntityAndOffset)
	{
		m_bHasGotoPosition = true; 
		gotoTarget.GetPosition(m_vGotoPosition);
	}

	// setup aim
	s32 nAimMode = aimTarget.GetMode();
	if(nAimMode == CAITarget::Mode_Entity || nAimMode == CAITarget::Mode_EntityAndOffset)
	{
		m_bHasAimEntity = true; 
		m_pAimEntity.SetEntity((CEntity*)aimTarget.GetEntity());
	}
	if(nAimMode == CAITarget::Mode_WorldSpacePosition || nAimMode == CAITarget::Mode_EntityAndOffset)
	{
		m_bHasAimPosition = true; 
		aimTarget.GetPosition(m_vAimPosition);
	}
}


CTask* CClonedGoToPointAimingInfo::CreateLocalTask(fwEntity* UNUSED_PARAM(pEntity))
{
	CAITarget gotoTarget; 
	CAITarget aimTarget; 

	// extract goto
	if(m_bHasGotoEntity && m_bHasGotoPosition)
		gotoTarget.SetEntityAndOffset(m_pGotoEntity.GetEntity(), m_vGotoPosition);
	else if(m_bHasGotoEntity)
		gotoTarget.SetEntity(m_pGotoEntity.GetEntity());
	else if(m_bHasGotoPosition)
		gotoTarget.SetPosition(m_vGotoPosition);

	// extract aim
	if(m_bHasAimEntity && m_bHasAimPosition)
		aimTarget.SetEntityAndOffset(m_pAimEntity.GetEntity(), m_vAimPosition);
	else if(m_bHasAimEntity)
		aimTarget.SetEntity(m_pAimEntity.GetEntity());
	else if(m_bHasAimPosition)
		aimTarget.SetPosition(m_vAimPosition);

	CTaskGoToPointAiming* pTask = rage_new CTaskGoToPointAiming(gotoTarget, aimTarget, m_fMoveBlendRatio, m_bIsShooting);
	pTask->SetTargetRadius(m_fTargetRadius);
	pTask->SetSlowDownDistance(m_fSlowDownDistance);
	pTask->SetUseNavmesh(m_bUseNavmesh);
	pTask->SetAimAtHatedEntitiesNearAimCoord(m_bAimAtHatedEntitiesNearAimCoord);
	pTask->SetScriptNavFlags(m_iScriptNavFlags);
	pTask->SetConfigFlags(m_iConfigFlags);
	pTask->SetInstantBlendToAim(m_bInstantBlendToAim);
	if(m_bIsShooting)
		pTask->SetFiringPattern(FIRING_PATTERN_FULL_AUTO);

	return pTask;
}

int CClonedGoToPointAimingInfo::GetScriptedTaskType() const
{
	if(m_bAimAtHatedEntitiesNearAimCoord)
		return SCRIPT_TASK_GO_TO_COORD_AND_AIM_AT_HATED_ENTITIES_NEAR_COORD;
	else
	{
		if(m_bHasGotoEntity)
			return m_bHasAimEntity ? SCRIPT_TASK_GO_TO_ENTITY_WHILE_AIMING_AT_ENTITY : SCRIPT_TASK_GO_TO_ENTITY_WHILE_AIMING_AT_COORD;
		else
			return m_bHasAimEntity ? SCRIPT_TASK_GO_TO_COORD_WHILE_AIMING_AT_ENTITY : SCRIPT_TASK_GO_TO_COORD_WHILE_AIMING_AT_COORD;
	}
}

//////////////////////////////////////////////////////////////////////////
// CTaskGoToPointAiming
//////////////////////////////////////////////////////////////////////////

CTaskGoToPointAiming::CTaskGoToPointAiming(const CAITarget& goToTarget, const CAITarget& aimAtTarget, const float fMoveBlendRatio, bool bSkipIdleTransition, const int iTime)
: m_goToTarget(goToTarget)
, m_aimAtTarget(aimAtTarget)
, m_originalAimAtTarget(aimAtTarget)
, m_fMoveBlendRatio(fMoveBlendRatio)
, m_fTargetRadius(DEFAULT_TARGET_RADIUS)
, m_fSlowDownDistance(DEFAULT_SLOW_DOWN_DISTANCE)
, m_uFiringPatternHash(0)
, m_bUseNavmesh(false)
, m_bSkipIdleTransition(bSkipIdleTransition)
, m_bWasCrouched(false)
, m_bAimAtHatedEntitiesNearAimCoord(false)
, m_iScriptNavFlags(0)
, m_bInstantBlendToAim(false)
, m_iTime(iTime)
{
	SetInternalTaskType(CTaskTypes::TASK_GO_TO_POINT_AIMING);
}

CTaskGoToPointAiming::CTaskGoToPointAiming(const CTaskGoToPointAiming& other)
: m_goToTarget(other.m_goToTarget)
, m_aimAtTarget(other.m_aimAtTarget)
, m_originalAimAtTarget(other.m_originalAimAtTarget)
, m_fMoveBlendRatio(other.m_fMoveBlendRatio)
, m_fTargetRadius(other.m_fTargetRadius)
, m_fSlowDownDistance(other.m_fSlowDownDistance)
, m_uFiringPatternHash(other.m_uFiringPatternHash)
, m_bUseNavmesh(other.m_bUseNavmesh)
, m_iScriptNavFlags(other.m_iScriptNavFlags)
, m_bSkipIdleTransition(other.m_bSkipIdleTransition)
, m_bWasCrouched(other.m_bWasCrouched)
, m_bAimAtHatedEntitiesNearAimCoord(other.m_bAimAtHatedEntitiesNearAimCoord)
, m_bInstantBlendToAim(other.m_bInstantBlendToAim)
, m_iTime(other.m_iTime)
, m_iConfigFlags(other.m_iConfigFlags)
{
	SetInternalTaskType(CTaskTypes::TASK_GO_TO_POINT_AIMING);
}

CTaskGoToPointAiming::~CTaskGoToPointAiming()
{
}

void CTaskGoToPointAiming::CleanUp()
{
	GetPed()->SetIsCrouching(m_bWasCrouched);
}

#if !__FINAL
void CTaskGoToPointAiming::Debug() const
{
#if DEBUG_DRAW
	if(m_goToTarget.GetIsValid())
	{
		Vector3 vGoToPosition;
		m_goToTarget.GetPosition(vGoToPosition);

		grcDebugDraw::Sphere(vGoToPosition, 0.1f, Color_blue);
		grcDebugDraw::Line(VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition()), vGoToPosition, Color_blue);
	}

	if(m_aimAtTarget.GetIsValid())
	{
		Vector3 vAimAtPosition;
		m_aimAtTarget.GetPosition(vAimAtPosition);

		grcDebugDraw::Sphere(vAimAtPosition, 0.1f, Color_red);
		grcDebugDraw::Line(VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition()), vAimAtPosition, Color_red);
	}
#endif // DEBUG_DRAW

	// Base class
	CTask::Debug();
}
#endif // !__FINAL

CTaskGoToPointAiming::FSM_Return CTaskGoToPointAiming::ProcessPreFSM()
{
	// This bool handles the target changing THIS frame so it always needs to be reset
	m_bTargetChangedThisFrame = false;
	
	// If we are supposed to be making sure we have a valid target at all times
	if(m_bAimAtHatedEntitiesNearAimCoord)
	{
		// This makes sure the targeting runs
		GetPed()->SetPedResetFlag(CPED_RESET_FLAG_CanPedSeeHatedPedBeingUsed, true);

		// Get our targeting so we can tell it to use the alternate position and check for a best target
		CPedTargetting* pPedTargeting = GetPed()->GetPedIntelligence()->GetTargetting(true);
		if(pPedTargeting)
		{
			pPedTargeting->SetUseAlternatePosition(true);
		}

		if(GetState() != State_Start)
		{
			// If we don't have an entity or our entity is fatally injured
			const CEntity* pEntity = m_aimAtTarget.GetEntity();
			bool bFindNewTarget = (pEntity == NULL) || (pEntity->GetIsTypePed() && static_cast<const CPed*>(pEntity)->IsFatallyInjured());
			if( !bFindNewTarget && m_iConfigFlags.IsFlagSet(CF_AimAtDestinationIfTargetLosBlocked) &&
				pPedTargeting && pPedTargeting->GetLosStatus(pEntity) == Los_blocked)
			{
				bFindNewTarget = true;
			}

			if(bFindNewTarget)
			{
				// Clear the aim target so if we don't find a new valid target we know to change our aim position
				if( !m_aimAtTarget.GetIsValid() || m_aimAtTarget.GetEntity() ||
					(m_iConfigFlags.IsFlagSet(CF_AimAtDestinationIfTargetLosBlocked) && m_aimAtTarget == m_originalAimAtTarget) )
				{
					m_aimAtTarget.Clear();
				}

				if(pPedTargeting)
				{
					// Get the best target and if it exists set it as our target + let the task know we changed targets this frame
					CPed* pBestTarget = pPedTargeting->GetBestTarget();
					if( pBestTarget &&
						(!m_iConfigFlags.IsFlagSet(CF_AimAtDestinationIfTargetLosBlocked) || pPedTargeting->GetLosStatus(pBestTarget) != Los_blocked) )
					{
						m_aimAtTarget = CAITarget(pBestTarget);
						m_bTargetChangedThisFrame = true;
					}
				}
			}

			// if we're supposed to updating our aim target and we can't find a valid ped target then reset the target
			if(!m_aimAtTarget.GetIsValid())
			{
				if(m_iConfigFlags.IsFlagSet(CF_AimAtDestinationIfTargetLosBlocked))
				{
					Vector3 vGoToPosition;
					m_goToTarget.GetPosition(vGoToPosition);
					vGoToPosition.z += 1.5f; // HORRIBLE MAGIC NUMBER HACK

					m_aimAtTarget = CAITarget(vGoToPosition);
				}
				else
				{
					m_aimAtTarget = CAITarget(m_originalAimAtTarget);
				}

				m_bTargetChangedThisFrame = true;
			}
		}
	}

	if(!m_goToTarget.GetIsValid() || !m_aimAtTarget.GetIsValid())
	{
		// Go to target or aim at target is invalid
		return FSM_Quit;
	}

	return FSM_Continue;
}

CTaskGoToPointAiming::FSM_Return CTaskGoToPointAiming::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		FSM_State(State_Start)
			FSM_OnUpdate
				return StateStart_OnUpdate();
		FSM_State(State_GoToPointAiming)
			FSM_OnEnter
				return StateGoToPointAiming_OnEnter(pPed);
			FSM_OnUpdate
				return StateGoToPointAiming_OnUpdate(pPed);
		FSM_State(State_Quit)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

#if !__FINAL
const char * CTaskGoToPointAiming::GetStaticStateName( s32 iState )
{
	static const char* aStateNames[] = 
	{
		"Start",
		"GoToPointAiming",
		"Quit",
	};

	return aStateNames[iState];
}
#endif // !__FINAL

bool CTaskGoToPointAiming::CanSetDesiredHeading() const
{
	const CPed* pPed = GetPed();
	const CTaskMotionBase* pMotionTask = pPed->GetPrimaryMotionTask();
	if(pMotionTask && pMotionTask->GetTaskType() == CTaskTypes::TASK_MOTION_PED)
	{
		const CTaskMotionPed* pMotionPedTask = static_cast<const CTaskMotionPed*>(pMotionTask);
		return pMotionPedTask->IsAimingOrNotBlocked();
	}
	return true;
}

CTaskGoToPointAiming::FSM_Return CTaskGoToPointAiming::StateStart_OnUpdate()
{
	if(m_bAimAtHatedEntitiesNearAimCoord)
	{
		// Get our targeting and let it know about the alternate position to use for choosing targets
		CPedTargetting* pPedTargeting = GetPed()->GetPedIntelligence()->GetTargetting(true);
		if(pPedTargeting)
		{
			Vector3 vPosition;
			m_originalAimAtTarget.GetPosition(vPosition);
			pPedTargeting->SetAlternatePosition(vPosition);
		}
	}

	SetState(State_GoToPointAiming);
	return FSM_Continue;
}

bool CTaskGoToPointAiming::SpheresOfInfluenceBuilder( TInfluenceSphere* apSpheres, int &iNumSpheres, const CPed* pPed )
{
	const CTaskGoToPointAiming* pAimingTask = static_cast<const CTaskGoToPointAiming*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_GO_TO_POINT_AIMING));
	if( pAimingTask )
	{
		const CAITarget& gotoTarget = pAimingTask->GetGotoTarget();
		const CAITarget& aimAtTarget = pAimingTask->GetAimAtTarget();

		if(aimAtTarget.GetEntity() && aimAtTarget.GetEntity() != gotoTarget.GetEntity())
		{
			Vector3 vAimAtTargetPosition;
			aimAtTarget.GetPosition(vAimAtTargetPosition);
			apSpheres[0].Init(vAimAtTargetPosition, CTaskCombat::ms_Tunables.m_TargetInfluenceSphereRadius, 1.0f*INFLUENCE_SPHERE_MULTIPLIER, 0.5f*INFLUENCE_SPHERE_MULTIPLIER);
			iNumSpheres = 1;
			return true;
		}
	}
	return false;
}	

CTaskGoToPointAiming::FSM_Return CTaskGoToPointAiming::StateGoToPointAiming_OnEnter(CPed* pPed)
{
	//
	// Create the movement task
	//

	m_bWasCrouched = pPed->GetIsCrouching();
	pPed->SetIsCrouching(false);

	const CWeapon* pWeapon = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeapon() : NULL;
	if(pWeapon)
	{
		// Always strafe toward target if holding a weapon
		if(!m_iConfigFlags.IsFlagSet(CF_DontAimIfSprinting) || m_fMoveBlendRatio < MOVEBLENDRATIO_SPRINT)
		{
			CPedTargetting* pPedTargeting = m_iConfigFlags.IsFlagSet(CF_DontAimIfTargetLosBlocked) ? pPed->GetPedIntelligence()->GetTargetting(false) : NULL;
			if( !pPedTargeting || pPedTargeting->GetLosStatus(m_aimAtTarget.GetEntity()) != Los_blocked )
			{
				pPed->SetIsStrafing(true);
			}
		}

		if(CanSetDesiredHeading())
		{
			// Set the heading to point at the aim at position
			Vector3 vAimAtPosition;
			m_aimAtTarget.GetPosition(vAimAtPosition);
			pPed->SetDesiredHeading(vAimAtPosition);
		}
	}

	CTask* pMoveTask = NULL;

	if(m_goToTarget.GetEntity())
	{
		pMoveTask = rage_new TTaskMoveSeekEntityStandard(
			m_goToTarget.GetEntity(), 
			TTaskMoveSeekEntityStandard::ms_iMaxSeekTime, 
			TTaskMoveSeekEntityStandard::ms_iPeriod, 
			m_fTargetRadius, 
			m_fSlowDownDistance, 
			TTaskMoveSeekEntityStandard::ms_fFollowNodeThresholdHeightChange, 
			false);

		static_cast<TTaskMoveSeekEntityStandard*>(pMoveTask)->SetMoveBlendRatio(m_fMoveBlendRatio);
	}
	else
	{
		Vector3 vGoToPosition;
		m_goToTarget.GetPosition(vGoToPosition);

		if(m_bUseNavmesh)
		{
			pMoveTask = rage_new CTaskMoveFollowNavMesh(m_fMoveBlendRatio, vGoToPosition, m_fTargetRadius, m_fSlowDownDistance, m_iTime, true, false, CTaskGoToPointAiming::SpheresOfInfluenceBuilder);
			((CTaskMoveFollowNavMesh*)pMoveTask)->SetScriptBehaviourFlags(m_iScriptNavFlags, NULL);
		}
		else
		{
			bool bStopExactly = (m_iScriptNavFlags & ENAV_STOP_EXACTLY ? true : false);
			pMoveTask = rage_new CTaskMoveGoToPointAndStandStill(m_fMoveBlendRatio, vGoToPosition, m_fTargetRadius, m_fSlowDownDistance, false, bStopExactly);
		}
	}

	// Create control movement task
	SetNewTask(rage_new CTaskComplexControlMovement(pMoveTask, CreateGunTask(), CTaskComplexControlMovement::TerminateOnMovementTask));
	return FSM_Continue;
}

CTaskGoToPointAiming::FSM_Return CTaskGoToPointAiming::StateGoToPointAiming_OnUpdate(CPed* pPed)
{
	// If we changed targets this frame we need to restart the state
	if(m_bTargetChangedThisFrame)
	{
		m_bTargetChangedThisFrame = false;
		SetFlag(aiTaskFlags::RestartCurrentState);
		return FSM_Continue;
	}

	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		// We are done
		return FSM_Quit;
	}

	// Update the weapon controller type of our gun task
	CTaskGun* pGunTask = static_cast<CTaskGun*>(FindSubTaskOfType(CTaskTypes::TASK_GUN));
	if(pGunTask)
	{
		pGunTask->SetWeaponControllerType(GetDesiredWeaponControllerType());
	}

	const CWeapon* pWeapon = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeapon() : NULL;
	if(pWeapon)
	{
		CTaskComplexControlMovement* pMoveTask = static_cast<CTaskComplexControlMovement*>(FindSubTaskOfType(CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT));

		// Always strafe toward target if holding a weapon
		if(!m_iConfigFlags.IsFlagSet(CF_DontAimIfSprinting) || m_fMoveBlendRatio < MOVEBLENDRATIO_SPRINT)
		{
			CPedTargetting* pPedTargeting = m_iConfigFlags.IsFlagSet(CF_DontAimIfTargetLosBlocked) ? pPed->GetPedIntelligence()->GetTargetting(false) : NULL;
			if( !pPedTargeting || pPedTargeting->GetLosStatus(m_aimAtTarget.GetEntity()) != Los_blocked )
			{
				pPed->SetIsStrafing(true);

				if(pMoveTask && pMoveTask->GetMainSubTaskType() != CTaskTypes::TASK_GUN)
				{
					pMoveTask->SetNewMainSubtask(CreateGunTask());
				}
			}
		}

		if(pMoveTask)
		{
			pMoveTask->SetMoveBlendRatio(pPed, m_fMoveBlendRatio);
		}

		if(CanSetDesiredHeading())
		{
			// Set the heading to point at the aim at position
			Vector3 vAimAtPosition;
			m_aimAtTarget.GetPosition(vAimAtPosition);
			pPed->SetDesiredHeading(vAimAtPosition);
		}
	}

	// If static counter has reached a certain point, we're probably stuck.
	// Create the CEventStaticCountReachedMax event, which will make the ped jump backwards
	// interrupting this task.
	if(pPed->GetPedIntelligence()->GetEventScanner()->GetStaticMovementScanner().GetStaticCounter() >= CStaticMovementScanner::ms_iStaticCounterStuckLimit)
	{
		CEventStaticCountReachedMax staticEvent(GetTaskType());
		pPed->GetPedIntelligence()->AddEvent(staticEvent);
	}

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CWeaponController::WeaponControllerType CTaskGoToPointAiming::GetDesiredWeaponControllerType()
{
	if(m_iConfigFlags.IsFlagSet(CF_DontAimIfSprinting) && m_fMoveBlendRatio >= MOVEBLENDRATIO_SPRINT)
	{
		return CWeaponController::WCT_None;
	}

	CPed* pPed = GetPed();
	CPedTargetting* pPedTargetting = pPed->GetPedIntelligence()->GetTargetting(true);
	const CEntity* pTargetEntity = m_aimAtTarget.GetEntity();

	if(pTargetEntity && pPedTargetting && m_iConfigFlags.IsFlagSet(CF_DontAimIfTargetLosBlocked))
	{
		// Make sure we set our targeting to be in use, otherwise the LOS tests will never be run
		pPedTargetting->SetInUse();

		if(pPedTargetting->GetLosStatus(pTargetEntity) == Los_blocked)
		{
			return CWeaponController::WCT_None;
		}
	}

	if(m_uFiringPatternHash == 0 || (m_bAimAtHatedEntitiesNearAimCoord && !pTargetEntity))
	{
		return CWeaponController::WCT_Aim;
	}
	else
	{
		// If we have a target entity and we're not allowed to shoot without LOS then we need to verify we have LOS before firing
		if(pPedTargetting && pTargetEntity && !pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanShootWithoutLOS))
		{
			// Make sure we set our targeting to be in use, otherwise the LOS tests will never be run
			pPedTargetting->SetInUse();

			// If there is a target and no LOS, aim instead of firing
			if(pPedTargetting->GetLosStatus(pTargetEntity) != Los_clear)
			{
				return CWeaponController::WCT_Aim;
			}
		}

		return CWeaponController::WCT_Fire;
	}
}

//////////////////////////////////////////////////////////////////////////

CTaskGun* CTaskGoToPointAiming::CreateGunTask()
{
	CWeaponController::WeaponControllerType wcType = GetDesiredWeaponControllerType();

	CTaskGun* pAimTask = rage_new CTaskGun(wcType, CTaskTypes::TASK_AIM_GUN_ON_FOOT, m_aimAtTarget, 600.0f);
	pAimTask->SetFiringPatternHash(m_uFiringPatternHash);
	if(m_bInstantBlendToAim)
	{
		pAimTask->GetGunFlags().SetFlag(GF_InstantBlendToAim);
	}

	if(m_bSkipIdleTransition)
	{
		pAimTask->GetGunFlags().SetFlag(GF_SkipIdleTransitions);
	}

	return pAimTask;
}

//////////////////////////////////////////////////////////////////////////

CTaskInfo* CTaskGoToPointAiming::CreateQueriableState() const
{
	return rage_new CClonedGoToPointAimingInfo(m_goToTarget, 
											   m_aimAtTarget, 
											   m_fMoveBlendRatio,
											   m_fTargetRadius,
											   m_fSlowDownDistance,
											   m_uFiringPatternHash != 0,
											   m_bUseNavmesh,
											   m_iScriptNavFlags,
											   m_iConfigFlags.GetAllFlags(),
											   m_bAimAtHatedEntitiesNearAimCoord,
											   m_bInstantBlendToAim);
}

CTaskFSMClone* CTaskGoToPointAiming::CreateTaskForClonePed(CPed* UNUSED_PARAM(pPed))
{
	CTaskGoToPointAiming* pTask = rage_new CTaskGoToPointAiming(m_goToTarget, m_aimAtTarget, m_fMoveBlendRatio, m_bSkipIdleTransition);
	if(pTask)
	{
		pTask->SetTargetRadius(m_fTargetRadius);
		pTask->SetSlowDownDistance(m_fSlowDownDistance);
		pTask->SetUseNavmesh(m_bUseNavmesh);
		pTask->SetAimAtHatedEntitiesNearAimCoord(m_bAimAtHatedEntitiesNearAimCoord);
		pTask->SetScriptNavFlags(m_iScriptNavFlags);
		pTask->SetConfigFlags(m_iConfigFlags.GetAllFlags());
		pTask->SetFiringPattern(m_uFiringPatternHash);
		pTask->SetInstantBlendToAim(m_bInstantBlendToAim);
	}
	return pTask;
}

CTaskFSMClone* CTaskGoToPointAiming::CreateTaskForLocalPed(CPed* pPed)
{
	return CreateTaskForClonePed(pPed);
}

CTask::FSM_Return CTaskGoToPointAiming::UpdateClonedFSM(s32 UNUSED_PARAM(iState), CTask::FSM_Event UNUSED_PARAM(iEvent))
{
	return FSM_Quit;
}
