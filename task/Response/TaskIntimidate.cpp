// FILE :    TaskIntimidate.h

// File header
#include "Task/Response/TaskIntimidate.h"

// Game headers
#include "ai/ambient/ConditionalAnimManager.h"
#include "animation/EventTags.h"
#include "event/Events.h"
#include "Peds/ped.h"
#include "task/Animation/TaskAnims.h"
#include "task/Default/TaskAmbient.h"
#include "weapons/inventory/PedWeaponManager.h"
#include "weapons/Weapon.h"

AI_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////
// CTaskIntimidate
////////////////////////////////////////////////////////////////////////////////

CTaskIntimidate::Tunables CTaskIntimidate::sm_Tunables;
IMPLEMENT_RESPONSE_TASK_TUNABLES(CTaskIntimidate, 0xc50950b3);

CTaskIntimidate::CTaskIntimidate(const CPed* pTarget)
: m_pTarget(pTarget)
, m_nMode(M_None)
, m_uRunningFlags(0)
{
	SetInternalTaskType(CTaskTypes::TASK_INTIMIDATE);
}

CTaskIntimidate::~CTaskIntimidate()
{

}

bool CTaskIntimidate::HasGunInHand(const CPed& rPed)
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

	//Ensure the weapon is a gun.
	if(!pWeapon->GetWeaponInfo()->GetIsGun())
	{
		return false;
	}

	return true;
}

bool CTaskIntimidate::IsValid(const CPed& rPed)
{
	//Ensure the mode is valid.
	Mode nMode = ChooseMode(rPed);
	if(nMode == M_None)
	{
		return false;
	}

	//Ensure the clip sets are valid.
	if(GetClipSetForIdle(rPed, nMode) == CLIP_SET_ID_INVALID)
	{
		return false;
	}
	else if(GetClipSetForTransition(rPed, nMode) == CLIP_SET_ID_INVALID)
	{
		return false;
	}

	//Ensure the name for the conditional anims is valid.
	if(!GetNameForConditionalAnims(rPed, nMode))
	{
		return false;
	}

	return true;
}

bool CTaskIntimidate::ModifyLoadout(CPed& rPed)
{
	//Ensure the ped is ambient.
	if(!rPed.PopTypeIsRandom())
	{
		return false;
	}

	//Ensure the ped is a regular cop or gang member.
	if(!rPed.IsRegularCop() && !rPed.IsGangPed())
	{
		return false;
	}

	//Ensure the weapon manager is valid.
	CPedWeaponManager* pWeaponManager = rPed.GetWeaponManager();
	if(!pWeaponManager)
	{
		return false;
	}

	//Ensure the ped is not armed.
	if(pWeaponManager->GetIsArmed())
	{
		return false;
	}

	//Ensure the best weapon is a gun.
	const CWeaponInfo* pWeaponInfo = pWeaponManager->GetBestWeaponInfo();
	if(!pWeaponInfo || !pWeaponInfo->GetIsGun())
	{
		return false;
	}

	//Remove all weapons.
	rPed.GetInventory()->RemoveAll();

	//Add a pistol (and unarmed).
	rPed.GetInventory()->AddWeapon(WEAPONTYPE_UNARMED);
	rPed.GetInventory()->AddWeaponAndAmmo(WEAPONTYPE_PISTOL, 100);

	return true;
}

bool CTaskIntimidate::ShouldDisableMoving(const CPed& rPed)
{
	//Check if the ped is a regular cop.
	if(rPed.IsRegularCop())
	{
		return true;
	}

	return false;
}

#if !__FINAL
void CTaskIntimidate::Debug() const
{
#if DEBUG_DRAW
#endif

	CTask::Debug();
}
#endif // !__FINAL

#if !__FINAL
const char* CTaskIntimidate::GetStaticStateName(s32 iState)
{
	Assert(iState >= State_Start && iState <= State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_WaitForStreaming",
		"State_Intro",
		"State_Idle",
		"State_IdleUpperBody",
		"State_Outro",
		"State_Finish",
	};

	return aStateNames[iState];
}
#endif

CTaskIntimidate::Mode CTaskIntimidate::ChooseMode() const
{
	return ChooseMode(*GetPed());
}

void CTaskIntimidate::CreateWeapon()
{
	//Ensure we have not created the weapon.
	if(m_uRunningFlags.IsFlagSet(RF_HasCreatedWeapon))
	{
		return;
	}

	//Set the flag.
	m_uRunningFlags.SetFlag(RF_HasCreatedWeapon);

	//Ensure the weapon manager is valid.
	CPedWeaponManager* pWeaponManager = GetPed()->GetWeaponManager();
	if(!pWeaponManager)
	{
		return;
	}

	//Equip the best weapon.
	pWeaponManager->EquipBestWeapon(false);

	//Create the weapon.
	pWeaponManager->CreateEquippedWeaponObject();
}

void CTaskIntimidate::DestroyWeapon()
{
	//Ensure we have not destroyed the weapon.
	if(m_uRunningFlags.IsFlagSet(RF_HasDestroyedWeapon))
	{
		return;
	}

	//Set the flag.
	m_uRunningFlags.SetFlag(RF_HasDestroyedWeapon);

	//Ensure the weapon manager is valid.
	CPedWeaponManager* pWeaponManager = GetPed()->GetWeaponManager();
	if(!pWeaponManager)
	{
		return;
	}

	//Destroy the weapon.
	pWeaponManager->DestroyEquippedWeaponObject(false);

	//Equip the unarmed weapon.
	pWeaponManager->EquipWeapon(GetPed()->GetDefaultUnarmedWeaponHash());
}

fwMvClipSetId CTaskIntimidate::GetClipSetForIdle() const
{
	return GetClipSetForIdle(*GetPed(), m_nMode);
}

fwMvClipSetId CTaskIntimidate::GetClipSetForTransition() const
{
	return GetClipSetForTransition(*GetPed(), m_nMode);
}

const char* CTaskIntimidate::GetNameForConditionalAnims() const
{
	return GetNameForConditionalAnims(*GetPed(), m_nMode);
}

bool CTaskIntimidate::ProcessMode()
{
	//Check if the mode is invalid.
	if(m_nMode == M_None)
	{
		//Choose the mode.
		m_nMode = ChooseMode();
		if(m_nMode == M_None)
		{
			return false;
		}
	}

	return true;
}

bool CTaskIntimidate::ProcessStreaming()
{
	//Check if the transition clip set request helper is not active.
	if(!m_ClipSetRequestHelperForTransition.IsActive())
	{
		//Ensure the clip set is valid.
		fwMvClipSetId clipSetId = GetClipSetForTransition();
		if(!taskVerifyf(clipSetId != CLIP_SET_ID_INVALID, "The clip set is invalid."))
		{
			return false;
		}

		//Request the clip set.
		m_ClipSetRequestHelperForTransition.RequestClipSet(clipSetId);
	}

	//Check if the idle clip set request helper is not active.
	if(!m_ClipSetRequestHelperForIdle.IsActive())
	{
		//Ensure the clip set is valid.
		fwMvClipSetId clipSetId = GetClipSetForIdle();
		if(!taskVerifyf(clipSetId != CLIP_SET_ID_INVALID, "The clip set is invalid."))
		{
			return false;
		}

		//Request the clip set.
		m_ClipSetRequestHelperForIdle.RequestClipSet(clipSetId);
	}

	return true;
}

bool CTaskIntimidate::ShouldPlayOutroForEvent(const CEvent* pEvent) const
{
	//Check if the event is valid.
	if(pEvent)
	{
		//Check if we shouldn't play an outro for the response task type.
		if(!ShouldPlayOutroForTaskType(pEvent->GetResponseTaskType()))
		{
			return false;
		}

		//Check the event type.
		switch(pEvent->GetEventType())
		{
			case EVENT_AGITATED_ACTION:
			{
				//Check if the task is valid.
				const aiTask* pTask = static_cast<const CEventAgitatedAction *>(pEvent)->GetTask();
				if(pTask)
				{
					//Check if we shouldn't play an outro for the task type.
					if(!ShouldPlayOutroForTaskType(pTask->GetTaskType()))
					{
						return false;
					}
				}

				break;
			}
			default:
			{
				break;
			}
		}
	}

	return true;
}

bool CTaskIntimidate::ShouldPlayOutroForTaskType(int iTaskType) const
{
	//Check the task type.
	switch(iTaskType)
	{
		case CTaskTypes::TASK_COMBAT:
		case CTaskTypes::TASK_POLICE_ORDER_RESPONSE:
		case CTaskTypes::TASK_POLICE_WANTED_RESPONSE:
		case CTaskTypes::TASK_SMART_FLEE:
		case CTaskTypes::TASK_SWAT_ORDER_RESPONSE:
		case CTaskTypes::TASK_SWAT_WANTED_RESPONSE:
		case CTaskTypes::TASK_THREAT_RESPONSE:
		{
			return false;
		}
		default:
		{
			return true;
		}
	}
}

bool CTaskIntimidate::ShouldUseUpperBodyIdle() const
{
	//Check if the ped is a cop.
	if(GetPed()->IsRegularCop())
	{
		return true;
	}

	return false;
}

CTaskIntimidate::Mode CTaskIntimidate::ChooseMode(const CPed& rPed)
{
	//Check if the ped has no gun in hand.
	if(!HasGunInHand(rPed))
	{
		//Check if the weapon manager is valid.
		const CPedWeaponManager* pWeaponManager = rPed.GetWeaponManager();
		if(pWeaponManager)
		{
			//Check if the best weapon info is valid.
			const CWeaponInfo* pWeaponInfo = pWeaponManager->GetBestWeaponInfo();
			if(pWeaponInfo)
			{
				//Check if the weapon is 1H.
				if(pWeaponInfo->GetIsGun1Handed())
				{
					return M_1H;
				}
			}
		}
	}

	return M_None;
}

fwMvClipSetId CTaskIntimidate::GetClipSetForIdle(const CPed& rPed, Mode nMode)
{
	//Check the mode.
	switch(nMode)
	{
		case M_1H:
		{
			if(rPed.IsRegularCop())
			{
				if(rPed.IsMale())
				{
					if(sm_Tunables.m_UseArmsOutForCops)
					{
						static fwMvClipSetId s_ClipSetId("move_m@intimidation@unarmed",0x22dc75b4);
						return s_ClipSetId;
					}
					else
					{
						static fwMvClipSetId s_ClipSetId("move_m@intimidation@cop@unarmed",0xD35A49A3);
						return s_ClipSetId;
					}
				}
				else
				{
					if(sm_Tunables.m_UseArmsOutForCops)
					{
						static fwMvClipSetId s_ClipSetId("move_f@intimidation@unarmed",0xda0c30);
						return s_ClipSetId;
					}
					else
					{
						static fwMvClipSetId s_ClipSetId("move_f@intimidation@cop@unarmed",0x283BEED8);
						return s_ClipSetId;
					}
				}
			}
			else if(rPed.IsGangPed())
			{
				static fwMvClipSetId s_ClipSetId("move_m@intimidation@1h",0x33C124FC);
				return s_ClipSetId;
			}
			else
			{
				return CLIP_SET_ID_INVALID;
			}
		}
		default:
		{
			return CLIP_SET_ID_INVALID;
		}
	}
}

fwMvClipSetId CTaskIntimidate::GetClipSetForTransition(const CPed& rPed, Mode nMode)
{
	//Check the mode.
	switch(nMode)
	{
		case M_1H:
		{
			if(rPed.IsRegularCop())
			{
				if(sm_Tunables.m_UseArmsOutForCops)
				{
					static fwMvClipSetId s_ClipSetId("reaction@intimidation@unarmed",0xa407ac05);
					return s_ClipSetId;
				}
				else
				{
					static fwMvClipSetId s_ClipSetId("reaction@intimidation@cop@unarmed",0x6713B3EB);
					return s_ClipSetId;
				}
			}
			else if(rPed.IsGangPed())
			{
				static fwMvClipSetId s_ClipSetId("reaction@intimidation@1h",0xE3E9A645);
				return s_ClipSetId;
			}
			else
			{
				return CLIP_SET_ID_INVALID;
			}
		}
		default:
		{
			return CLIP_SET_ID_INVALID;
		}
	}
}

const char* CTaskIntimidate::GetNameForConditionalAnims(const CPed& rPed, Mode nMode)
{
	//Check the mode.
	switch(nMode)
	{
		case M_1H:
		{
			if(rPed.IsRegularCop())
			{
				static const char* s_pName = "INTIMIDATION@COP@UNARMED";
				return s_pName;
			}
			else if(rPed.IsGangPed())
			{
				static const char* s_pName = "INTIMIDATION@1H";
				return s_pName;
			}
			else
			{
				return NULL;
			}
		}
		default:
		{
			return NULL;
		}
	}
}

void CTaskIntimidate::CleanUp()
{
	//Stop the clip.
	StopClip(NORMAL_BLEND_OUT_DELTA);
}

aiTask* CTaskIntimidate::Copy() const
{
	//Create the task.
	CTaskIntimidate* pTask = rage_new CTaskIntimidate(m_pTarget);

	return pTask;
}

CTask::FSM_Return CTaskIntimidate::ProcessPreFSM()
{
	//Ensure the target is valid.
	if(!m_pTarget)
	{
		return FSM_Quit;
	}

	//Process the mode.
	if(!ProcessMode())
	{
		return FSM_Quit;
	}

	//Process streaming.
	if(!ProcessStreaming())
	{
		return FSM_Quit;
	}

	return FSM_Continue;
}

bool CTaskIntimidate::ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
	if (pEvent)
	{
		const CEvent* pEEvent = (const CEvent*)pEvent;
		if( pEEvent->RequiresAbortForMelee( GetPed() ) )
			return true;
	}

	//Check if the priority is not immediate.
	if(iPriority != ABORT_PRIORITY_IMMEDIATE)
	{
		//Check we should play the outro.
		if(ShouldPlayOutroForEvent(static_cast<const CEvent *>(pEvent)))
		{
			//Play the outro.
			m_uRunningFlags.SetFlag(RF_PlayOutro);
			return false;
		}
	}

	//Call the base class version.
	return CTask::ShouldAbort(iPriority, pEvent);
}

CTask::FSM_Return CTaskIntimidate::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	FSM_Begin
	
		FSM_State(State_Start)
			FSM_OnEnter
				Start_OnEnter();
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_WaitForStreaming)
			FSM_OnUpdate
				return WaitForStreaming_OnUpdate();
				
		FSM_State(State_Intro)
			FSM_OnEnter
				Intro_OnEnter();
			FSM_OnUpdate
				return Intro_OnUpdate();
			FSM_OnExit
				Intro_OnExit();
				
		FSM_State(State_Idle)
			FSM_OnEnter
				Idle_OnEnter();
			FSM_OnUpdate
				return Idle_OnUpdate();

		FSM_State(State_IdleUpperBody)
			FSM_OnEnter
				IdleUpperBody_OnEnter();
			FSM_OnUpdate
				return IdleUpperBody_OnUpdate();
			FSM_OnExit
				IdleUpperBody_OnExit();

		FSM_State(State_Outro)
			FSM_OnEnter
				Outro_OnEnter();
			FSM_OnUpdate
				return Outro_OnUpdate();
			FSM_OnExit
				Outro_OnExit();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

void CTaskIntimidate::Start_OnEnter()
{

}

CTask::FSM_Return CTaskIntimidate::Start_OnUpdate()
{
	//Wait for streaming.
	SetState(State_WaitForStreaming);

	return FSM_Continue;
}

CTask::FSM_Return CTaskIntimidate::WaitForStreaming_OnUpdate()
{
	//Check if the clip set is streamed in.
	if(m_ClipSetRequestHelperForTransition.IsClipSetStreamedIn())
	{
		//Play the intro.
		SetState(State_Intro);
	}

	return FSM_Continue;
}

void CTaskIntimidate::Intro_OnEnter()
{
	//Play the clip.
	static fwMvClipId s_ClipId("intro",0xDA8F5F53);
	StartClipBySetAndClip(GetPed(), m_ClipSetRequestHelperForTransition.GetClipSetId(), s_ClipId, NORMAL_BLEND_IN_DELTA, CClipHelper::TerminationType_OnFinish);
}

CTask::FSM_Return CTaskIntimidate::Intro_OnUpdate()
{
	//Check if the clip has finished.
	if(!GetClipHelper() || GetClipHelper()->IsHeldAtEnd())
	{
		//Check if we should not use upper body.
		if(!ShouldUseUpperBodyIdle())
		{
			//Move to the idle state.
			SetState(State_Idle);
		}
		else
		{
			//Move to the idle upper body state.
			SetState(State_IdleUpperBody);
		}
	}
	//Check if the move event has occurred.
	else if(GetClipHelper()->IsEvent(CClipEventTags::MoveEvent))
	{
		//Create the weapon.
		CreateWeapon();
	}

	return FSM_Continue;
}

void CTaskIntimidate::Intro_OnExit()
{
	StopClip(REALLY_SLOW_BLEND_OUT_DELTA);
}

void CTaskIntimidate::Idle_OnEnter()
{
	//Check if we are a gang ped.
	if(GetPed()->IsGangPed())
	{
		//Note that we have brandished our weapon.
		GetPed()->SetPedConfigFlag(CPED_CONFIG_FLAG_HasBrandishedWeapon, true);
	}

	//Ensure the conditional anims are valid.
	const char* pName = GetNameForConditionalAnims();
	if(!taskVerifyf(pName, "The name for the conditional anims is invalid."))
	{
		return;
	}

	//Ensure the conditional anims group is valid.
	const CConditionalAnimsGroup* pConditionalAnimsGroup = CONDITIONALANIMSMGR.GetConditionalAnimsGroup(pName);
	if(!taskVerifyf(pConditionalAnimsGroup, "Conditional anims group is invalid for name: %s.", pName))
	{
		return;
	}

	//Create the task.
	CTask* pTask = rage_new CTaskAmbientClips(CTaskAmbientClips::Flag_PlayBaseClip|CTaskAmbientClips::Flag_PlayIdleClips, pConditionalAnimsGroup);

	//Start the task.
	SetNewTask(pTask);
}

CTask::FSM_Return CTaskIntimidate::Idle_OnUpdate()
{
	//Check if the sub-task has finished.
	if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		//Play the outro.
		SetState(State_Outro);
	}
	//Check if we should play the outro.
	else if(m_uRunningFlags.IsFlagSet(RF_PlayOutro))
	{
		//Play the outro.
		SetState(State_Outro);
	}

	return FSM_Continue;
}

void CTaskIntimidate::IdleUpperBody_OnEnter()
{
	//Play the clip.
	static fwMvClipId s_ClipId("idle", 0x71c21326);
	StartClipBySetAndClip(GetPed(), m_ClipSetRequestHelperForIdle.GetClipSetId(), s_ClipId, INSTANT_BLEND_IN_DELTA, CClipHelper::TerminationType_OnFinish);
}

CTask::FSM_Return CTaskIntimidate::IdleUpperBody_OnUpdate()
{
	//Check if we should play the outro.
	if(m_uRunningFlags.IsFlagSet(RF_PlayOutro))
	{
		//Play the outro.
		SetState(State_Outro);
	}

	return FSM_Continue;
}

void CTaskIntimidate::IdleUpperBody_OnExit()
{
	StopClip(REALLY_SLOW_BLEND_OUT_DELTA);
}

void CTaskIntimidate::Outro_OnEnter()
{
	//Play the clip.
	static fwMvClipId s_ClipId("outro",0x5D40B28D);
	StartClipBySetAndClip(GetPed(), m_ClipSetRequestHelperForTransition.GetClipSetId(), s_ClipId, NORMAL_BLEND_IN_DELTA, CClipHelper::TerminationType_OnFinish);
}

CTask::FSM_Return CTaskIntimidate::Outro_OnUpdate()
{
	//Check if the clip has finished.
	if(!GetClipHelper() || GetClipHelper()->IsHeldAtEnd())
	{
		//Finish the task.
		SetState(State_Finish);
	}
	//Check if the move event has occurred.
	else if(GetClipHelper()->IsEvent(CClipEventTags::MoveEvent))
	{
		//Destroy the weapon.
		DestroyWeapon();
	}

	return FSM_Continue;
}

void CTaskIntimidate::Outro_OnExit()
{
	StopClip(REALLY_SLOW_BLEND_OUT_DELTA);
}
