// FILE :    TaskShootAtTarget.cpp
// PURPOSE : Combat subtask in control of weapon firing
// CREATED : 08-10-2009

// C++ headers
#include <limits>

// Game headers
#include "Peds/PedIntelligence.h"
#include "Peds/Ped.h"
#include "Task/Combat/Cover/TaskCover.h"
#include "Task/Combat/Subtasks/TaskShootAtTarget.h"
#include "Task/Combat/TaskCombat.h"
#include "Task/General/TaskBasic.h"
#include "Task/General/TaskSecondary.h"
#include "Task/Motion/Locomotion/TaskMotionAiming.h"
#include "Task/Movement/TaskMoveFollowEntityOffset.h"
#include "Task/Weapons/Gun/GunFlags.h"
#include "Task/Weapons/Gun/TaskGun.h"
#include "Task/Weapons/Gun/TaskAimGun.h"
#include "Task/Weapons/WeaponController.h"
#include "Task/Weapons/TaskProjectile.h"
#include "Task/Weapons/TaskSwapWeapon.h"
#include "Vehicles/Vehicle.h"

AI_OPTIMISATIONS()

//=========================================================================
// FIRING SUBTASK 
//=========================================================================

#define ALTERNATE_TARGET_TIMER_MIN (2.0f)
#define ALTERNATE_TARGET_TIMER_MAX (5.0f)
#define REVERT_TO_ORIGINAL_TARGET_TIME (1.5f)

CTaskShootAtTarget::CTaskShootAtTarget(const CAITarget& target, const s32 iFlags, float fTaskTimer)
: m_target(target)
, m_pAlternateTarget(NULL)
, m_iFlags(iFlags)
, m_fTaskTimer(fTaskTimer)
, m_uFiringPatternHash(0)
{
	// If a time is specified, mark that this task is timed.
	if( m_fTaskTimer > 0.0f )
		m_iFlags.SetFlag(Flag_taskTimed);

	SetInternalTaskType(CTaskTypes::TASK_SHOOT_AT_TARGET);
}

CTaskShootAtTarget::~CTaskShootAtTarget()
{

}

CTask::FSM_Return CTaskShootAtTarget::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		// Entrance state for the task.
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate(pPed);

		//Check the game timer for pausing the task
		FSM_State(State_ShootAtTarget)
			FSM_OnEnter
				ShootAtTarget_OnEnter(pPed);
			FSM_OnUpdate
				ShootAtTarget_OnUpdate(pPed);

		//Check the system timer for pausing the task
		FSM_State(State_AimAtTarget)
			FSM_OnEnter
				AimAtTarget_OnEnter(pPed);
			FSM_OnUpdate
				AimAtTarget_OnUpdate(pPed);

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End

}


#if !__FINAL
const char * CTaskShootAtTarget::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_Start&&iState<=State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_ShootAtTarget",
		"State_AimAtTarget",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif // !__FINAL

CTask::FSM_Return CTaskShootAtTarget::Start_OnUpdate( CPed* pPed )
{
	ShootState newState = ChooseState(pPed);
	SetState(newState);
	return FSM_Continue;
}

void CTaskShootAtTarget::ShootAtTarget_OnEnter( CPed* pPed )
{
	// Start the gun control task with the required task and time limit
	CTaskGun* pShootTask = rage_new CTaskGun(CWeaponController::WCT_Fire, CTaskTypes::TASK_AIM_GUN_ON_FOOT, m_target, -1.0f);
	if(m_uFiringPatternHash != 0)
	{
		pShootTask->SetFiringPatternHash(m_uFiringPatternHash);
	}
	//pShootTask->GetGunFlags().SetFlag( GF_DisableTorsoIk );
	pShootTask->GetGunFlags().SetFlag( GF_WaitForFacingAngleToFire );

	// Skip the intro section of the fire animation since we've played our own version from the cover animations
	//@@: location CTASKSHOOTATTARGET_SHOOTATTARGET_ONENTER
	CTaskMotionAiming* pTask = static_cast<CTaskMotionAiming *>(pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_AIMING));
	if(pTask)
	{
		pShootTask->GetGunFlags().SetFlag( GF_SkipIdleTransitions );
	}
	SetNewTask(pShootTask);
}

CTask::FSM_Return CTaskShootAtTarget::ShootAtTarget_OnUpdate( CPed* pPed )
{
	ShootState newState = ChooseState(pPed);
	if( newState != GetState() )
	{
		SetState(newState);
		return FSM_Continue;
	}

	if( GetIsSubtaskFinished( CTaskTypes::TASK_GUN) )
	{
		SetState(State_Finish);
	}

	return FSM_Continue;
}

void CTaskShootAtTarget::AimAtTarget_OnEnter( CPed* UNUSED_PARAM(pPed) )
{
	CTaskGun* pAimTask = NULL;

	// aim toward target if the LOS is blocked
	pAimTask = rage_new CTaskGun(CWeaponController::WCT_Aim, CTaskTypes::TASK_AIM_GUN_ON_FOOT, m_target, -1.0f);

	// If LOS is blocked, limit the aim angle to realistic values
	pAimTask->GetGunFlags().SetFlag(GF_WaitForFacingAngleToFire);

	SetNewTask(pAimTask);
}

CTask::FSM_Return CTaskShootAtTarget::AimAtTarget_OnUpdate( CPed* pPed )
{
	ShootState newState = ChooseState(pPed);
	if( newState != GetState() )
	{
		SetState(newState);
		return FSM_Continue;
	}

	if( GetIsSubtaskFinished( CTaskTypes::TASK_GUN) )
	{
		SetState(State_Finish);
	}

	return FSM_Continue;
}

CTaskShootAtTarget::ShootState CTaskShootAtTarget::ChooseState(CPed* pPed)
{
	if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsSwitchingWeapon))
	{
		return (ShootState)GetState();
	}

	// If the target becomes invalid - revert to the original
	if( !m_target.GetIsValid() )
	{
		return State_Finish;
	}

	// If this is timed, quit after the period specified
	if( m_iFlags.IsFlagSet(Flag_taskTimed) )
	{
		if( GetTimeRunning() > m_fTaskTimer )
		{
			return State_Finish;
		}
	}

	// Early out if specifically instructed to aim
	if(m_iFlags&Flag_aimOnly)
		return State_AimAtTarget;

	if(!pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_CanShootWithoutLOS))
	{
		CPedTargetting* pPedTargetting = pPed->GetPedIntelligence()->GetTargetting( true );

		// If there is a target and no LOS, aim instead of firing
		if( pPedTargetting && m_target.GetEntity() )
		{
			CSingleTargetInfo* pTargetInfo =  pPedTargetting->FindTarget(m_target.GetEntity());
			if( pTargetInfo )
			{
				LosStatus losStatus = pTargetInfo->GetLosStatus();
				if( ( losStatus == Los_blocked ) || ( losStatus == Los_unchecked ) || (losStatus == Los_blockedByFriendly) )
				{
					return State_AimAtTarget;
				}
			}
		}
	}

	// If the timer for trying to acquire a new alternate target has finished, try to 
	// find a new target
// 	if( GetSubTask()->GetTaskType() == CTaskTypes::TASK_GUN && m_fTimeToChooseAlternateTarget <= 0.0f )
// 	{
// 		const float fAngleToTarget = fwAngle::GetATanOfXY( (m_vTargetPosition.x - pPed->GetPosition().x), (m_vTargetPosition.y - pPed->GetPosition().y) );
// 		m_fTimeToChooseAlternateTarget = fwRandom::GetRandomNumberInRange(ALTERNATE_TARGET_TIMER_MIN, ALTERNATE_TARGET_TIMER_MAX);
// 		Assert(pPedTargetting);
// 		Assert( m_pAlternateTarget == (CEntity*)NULL || IsEntityPointerValid(m_pAlternateTarget) );
// 		CEntity* pTarget = pPedTargetting->FindNextTarget(m_pAlternateTarget, true, false, fAngleToTarget, QUARTER_PI );
// 
// 		if( pTarget == m_pPrimaryTarget )
// 		{
// 			pTarget = pPedTargetting->FindNextTarget(m_pPrimaryTarget, true, false, fAngleToTarget, QUARTER_PI );
// 		}
// 		// If a valid alternate target has been found, fire at that target for a moment.
// 		if( pTarget && pTarget != m_pPrimaryTarget )
// 		{
// 			m_pAlternateTarget = pTarget;
// 			if( GetSubTask()->MakeAbortable( CTask::ABORT_PRIORITY_URGENT, NULL) )
// 			{
// 				CWeaponControllerFixed* pWeaponController = m_bAimOnly ? rage_new CWeaponControllerFixed(WCS_Aim) : rage_new CWeaponControllerFixed(WCS_Fire);
// 				return rage_new CTaskGun(pWeaponController, CTaskTypes::TASK_AIM_GUN_ON_FOOT, CWeaponTarget(m_pAlternateTarget), fwRandom::GetRandomNumberInRange( 0.5f, 0.75f ));
// 			}
// 		}
// 	}

	return State_ShootAtTarget;
}

void CTaskShootAtTarget::SetTarget(const CAITarget& target)
{
	m_target = target;

	CTask* pSubTask = GetSubTask();
	if(pSubTask && pSubTask->GetTaskType() == CTaskTypes::TASK_GUN)
	{
		// Set the new target in our shoot at target sub task
		static_cast<CTaskGun*>(pSubTask)->SetTarget(target);
	}
}
