
#include "peds/Ped.h"
#include "Task/Combat/SubTasks/TaskAimFromGround.h"
#include "Task/Weapons/Gun/TaskGun.h"
#include "Task/Weapons/Gun/TaskAimGunScripted.h"
#include "Task/Weapons/Gun/Metadata/ScriptedGunTaskInfo.h"
#include "Task/Weapons/Gun/Metadata/ScriptedGunTaskInfoMetadataMgr.h"
#include "Weapons/WeaponTypes.h"

AI_OPTIMISATIONS()

// Tunable parameters. ///////////////////////////////////////////////////
CTaskAimFromGround::Tunables CTaskAimFromGround::sm_Tunables;
IMPLEMENT_COMBAT_TASK_TUNABLES(CTaskAimFromGround, 0x681da643);
//////////////////////////////////////////////////////////////////////////

CTaskAimFromGround::CTaskAimFromGround(u32 scriptedGunTaskInfoHash, const fwFlags32& iFlags, const CWeaponTarget& target, bool bForceShootFromGround)
: m_ScriptedGunTaskInfoHash(scriptedGunTaskInfoHash)
, m_iFlags(iFlags)
, m_target(target)
, m_bForceShootFromGround(bForceShootFromGround)
{
	SetInternalTaskType(CTaskTypes::TASK_AIM_FROM_GROUND);
}

aiTask::FSM_Return CTaskAimFromGround::ProcessPreFSM()
{
	CPed* pPed = GetPed();

	pPed->SetPedResetFlag(CPED_RESET_FLAG_ShootFromGround, true);

	return FSM_Continue;
}

aiTask::FSM_Return CTaskAimFromGround::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{

	FSM_Begin

	FSM_State(State_Start)
		FSM_OnUpdate
			return Start_OnUpdate();

	FSM_State(State_AimingFromGround)
		FSM_OnEnter
			AimingFromGround_OnEnter();
		FSM_OnUpdate
			return AimingFromGround_OnUpdate();
		FSM_OnExit
			AimingFromGround_OnExit();

	FSM_State(State_Finish)
		FSM_OnUpdate
			return FSM_Quit;

	FSM_End
}

//////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskAimFromGround::Start_OnUpdate()
//////////////////////////////////////////////////////////////////////////
{
	SetState(State_AimingFromGround);
	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskAimFromGround::AimingFromGround_OnEnter()
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	CTaskGun* pGunTask = NULL;
	if(GetPed()->HasHurtStarted())
	{
		// Timer is handled in Writhe task
		pGunTask = rage_new CTaskGun(CWeaponController::WCT_Fire, CTaskTypes::TASK_AIM_GUN_SCRIPTED, CWeaponTarget(m_target));
		pGunTask->SetFiringPatternHash(FIRING_PATTERN_FROM_GROUND);
	}
	else
	{
		TUNE_GROUP_FLOAT(SHOOTFROMGROUND_TWEAK, MinTime, 2.f, 0.f, 10.f, 0.1f);
		TUNE_GROUP_FLOAT(SHOOTFROMGROUND_TWEAK, MaxTime, 4.f, 0.f, 10.f, 0.1f);
		pGunTask = rage_new CTaskGun(CWeaponController::WCT_Fire, CTaskTypes::TASK_AIM_GUN_SCRIPTED, CWeaponTarget(m_target), fwRandom::GetRandomNumberInRange(MinTime, MaxTime));
		pGunTask->SetFiringPatternHash(FIRING_PATTERN_FULL_AUTO);
	}

	pGunTask->GetGunFlags().SetFlag(GF_DisableBlockingClip);
	pGunTask->GetGunFlags().SetFlag(GF_DisableTorsoIk);
	pGunTask->GetGunFlags().SetFlag(GF_DisableReload);
	pGunTask->GetGunFlags().SetFlag(GF_PreventWeaponSwapping);
	pGunTask->SetScriptedGunTaskInfoHash(m_ScriptedGunTaskInfoHash);

	SetNewTask(pGunTask);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskAimFromGround::AimingFromGround_OnUpdate()
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished) || !GetSubTask())
	{
		SetState(State_Finish);
		return FSM_Continue;
	}
	else
	{
		CTaskAimGunScripted* pTask = FindScriptedGunTask();
		if (pTask)
		{	
			float heading = pTask->GetDesiredHeading();

			if ( heading == 0.0f || heading == 1.0f || (!m_bForceShootFromGround && GetTimeInState()>sm_Tunables.m_MaxAimFromGroundTime))
			{
				// this should cause the gun task to end
				pTask->SetWeaponControllerType(CWeaponController::WCT_None);
				if (GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_GUN)
				{
					static_cast<CTaskGun*>(GetSubTask())->SetWeaponControllerType(CWeaponController::WCT_None);
				}
			}
		}
	}

	return FSM_Continue;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskAimFromGround::AimingFromGround_OnExit()
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{

}

//////////////////////////////////////////////////////////////////////////
CTaskAimGunScripted* CTaskAimFromGround::FindScriptedGunTask()
//////////////////////////////////////////////////////////////////////////
{
	if (GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_GUN)
	{
		CTask* pGunTask = GetSubTask();
		if (pGunTask->GetSubTask() && pGunTask->GetSubTask()->GetTaskType()==CTaskTypes::TASK_AIM_GUN_SCRIPTED)
		{
			return static_cast<CTaskAimGunScripted*>(pGunTask->GetSubTask());
		}
	}

	return NULL;
}

