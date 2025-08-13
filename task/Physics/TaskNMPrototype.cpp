// Filename   :	TaskNMPrototype.cpp
// Description:	GTA-side of the NM InjuredOnGround behavior

//---- Include Files ---------------------------------------------------------------------------------------------------------------------------------
// Rage headers
#include "fragmentnm\messageparams.h"

// Framework headers
#include "fwdebug/Picker.h"
#include "grcore/debugdraw.h"

// Game headers
#include "event/events.h"
#include "Peds/ped.h"
#include "Peds/PedIntelligence.h"
#include "Task/Physics/TaskNMPrototype.h"
#include "Task/Physics/NmDebug.h"

//----------------------------------------------------------------------------------------------------------------------------------------------------

AI_OPTIMISATIONS()

// Tunable parameters. ///////////////////////////////////////////////////
CTaskNMPrototype::Tunables CTaskNMPrototype::sm_Tunables;
IMPLEMENT_PHYSICS_TASK_TUNABLES(CTaskNMPrototype, 0xacc2fe87);
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskNMPrototype //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskNMPrototype::CTaskNMPrototype(u32 nMinTime, u32 nMaxTime)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
: CTaskNMBehaviour(nMinTime, nMaxTime)
, m_bBalanceFailedSent(false)
, m_fLastUpdateTime(0.0f)
{
	m_WasInfinite = false;
	m_bStopTask = false;

#if !__FINAL
	m_strTaskName = "NMPrototype";
#endif // !__FINAL
	SetInternalTaskType(CTaskTypes::TASK_NM_PROTOTYPE);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskNMPrototype::~CTaskNMPrototype()
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
}

#if __BANK
//////////////////////////////////////////////////////////////////////////
void CTaskNMPrototype::Tunables::AddPrototypeWidgets(bkBank& bank)
//////////////////////////////////////////////////////////////////////////
{
	bank.AddButton("Start prototype on focus ped", CTaskNMPrototype::StartPrototypeTask);
	bank.AddButton("Stop prototype on focus ped", CTaskNMPrototype::StopPrototypeTask);
	bank.AddButton("Send dynamic set 1", CTaskNMPrototype::SendDynamicSet1);
	bank.AddButton("Send dynamic set 2", CTaskNMPrototype::SendDynamicSet2);
	bank.AddButton("Send dynamic set 3", CTaskNMPrototype::SendDynamicSet3);
}
#endif //__BANK

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMPrototype::StartBehaviour(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	AddTuningSet(&sm_Tunables.m_Start);

	// Send the onMovingGround param if applicable and desired
	if (sm_Tunables.m_CheckForMovingGround && OnMovingGround(pPed))
	{
		ART::MessageParams msg;
		msg.addBool(NMSTR_PARAM(NM_CONFIGURE_BALANCE_MOVING_FLOOR), true);
		pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_CONFIGURE_BALANCE_MSG), &msg);	
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMPrototype::ControlBehaviour(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	AddTuningSet(&sm_Tunables.m_Update);

	if (!m_bBalanceFailedSent && GetControlTask()->GetFeedbackFlags()&CTaskNMControl::BALANCE_FAILURE)
	{
		AddTuningSet(&sm_Tunables.m_OnBalanceFailed);
		m_bBalanceFailedSent = true;
	}

	// process the timed tuning sets
	float currentTime = GetTimeInState();

	for (s32 i=0; i<sm_Tunables.m_TimedMessages.GetCount(); i++)
	{
		Tunables::TimedTuning& tune = sm_Tunables.m_TimedMessages[i];
		float localTime = currentTime;
		float localLastTime = m_fLastUpdateTime;
		if (tune.m_Periodic)
		{
			while (localLastTime>tune.m_TimeInSeconds)
			{
				localTime -= tune.m_TimeInSeconds;
				localLastTime-= tune.m_TimeInSeconds;
			}
		}
		if (tune.m_TimeInSeconds<localTime && tune.m_TimeInSeconds>=localLastTime)
		{
			AddTuningSet(&tune.m_Messages);
		}
	}

	CNmMessageList list;
	ApplyTuningSetsToList(list);
	
	list.Post(GetPed()->GetRagdollInst());

	m_fLastUpdateTime = currentTime;

	CTaskNMBehaviour::ControlBehaviour(pPed);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskNMPrototype::FinishConditions(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	if (m_bStopTask)
	{
		return true;
	}
	if (sm_Tunables.m_RunForever)
	{
		return false;
	}
	else
	{
		return ProcessFinishConditionsBase(pPed, MONITOR_FALL, FLAG_VEL_CHECK|FLAG_QUIT_IN_WATER);
	}	
}

#if __BANK

void CTaskNMPrototype::SendDynamicSet(CNmTuningSet& set)
{
	CPed* pFocusPed = NULL;
	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if( pEnt && pEnt->GetIsTypePed() )
			pFocusPed = static_cast<CPed*>(pEnt);

		if (pFocusPed && pFocusPed->GetUsingRagdoll())
		{
			CTaskNMBehaviour* pTaskNM = pFocusPed->GetPedIntelligence()->GetLowestLevelNMTask(pFocusPed);
			if (pTaskNM)
			{
				// find the running nm behaviour task
				set.Post(*pFocusPed, pTaskNM);
			}
		}
	}

	for(int i = 0; i < g_PickerManager.GetNumberOfEntities(); ++i)
	{
		CEntity* pEnt = static_cast<CEntity*>(g_PickerManager.GetEntity(i));
		if( pEnt && pEnt->GetIsTypePed() )
			pFocusPed = static_cast<CPed*>(pEnt);

		if (pFocusPed && pFocusPed->GetUsingRagdoll())
		{
			CTaskNMBehaviour* pTaskNM = pFocusPed->GetPedIntelligence()->GetLowestLevelNMTask(pFocusPed);
			if (pTaskNM)
			{
				// find the running nm behaviour task
				set.Post(*pFocusPed, pTaskNM);
			}
		}
	}	
}

void CTaskNMPrototype::StartPrototypeTask()
{
	CPed* pFocusPed = NULL;
	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if( pEnt && pEnt->GetIsTypePed() )
			pFocusPed = static_cast<CPed*>(pEnt);

		if (pFocusPed)
		{
			CPed *pPed = pFocusPed;
			if (pPed->GetRagdollInst() && pPed->GetRagdollInst()->GetType()->GetARTAssetID() >= 0 && pPed->GetRagdollState() < RAGDOLL_STATE_PHYS_ACTIVATE)
			{
				// Give it the prototype task
				CEventSwitch2NM event(sm_Tunables.m_SimulationTimeInMs,  rage_new CTaskNMPrototype(sm_Tunables.m_SimulationTimeInMs, sm_Tunables.m_SimulationTimeInMs));
				pPed->SwitchToRagdoll(event);
			}
		}
	}

	for(int i = 0; i < g_PickerManager.GetNumberOfEntities(); ++i)
	{
		CEntity* pEnt = static_cast<CEntity*>(g_PickerManager.GetEntity(i));
		if( pEnt && pEnt->GetIsTypePed() )
			pFocusPed = static_cast<CPed*>(pEnt);

		if (pFocusPed)
		{
			CPed *pPed = pFocusPed;
			if (pPed->GetRagdollInst() && pPed->GetRagdollInst()->GetType()->GetARTAssetID() >= 0 && pPed->GetRagdollState() < RAGDOLL_STATE_PHYS_ACTIVATE)
			{
				// Give it the prototype task
				CEventSwitch2NM event(sm_Tunables.m_SimulationTimeInMs,  rage_new CTaskNMPrototype(sm_Tunables.m_SimulationTimeInMs, sm_Tunables.m_SimulationTimeInMs));
				pPed->SwitchToRagdoll(event);
			}
		}
	}	
}

void CTaskNMPrototype::StopPrototypeTask()
{
	CPed* pFocusPed = NULL;
	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if( pEnt && pEnt->GetIsTypePed() )
			pFocusPed = static_cast<CPed*>(pEnt);

		if (pFocusPed)
		{
			CTaskNMPrototype* pTask = static_cast<CTaskNMPrototype*>(pFocusPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_NM_PROTOTYPE));
			if (pTask)
			{
				pTask->Stop();
			}
		}
	}

	for(int i = 0; i < g_PickerManager.GetNumberOfEntities(); ++i)
	{
		CEntity* pEnt = static_cast<CEntity*>(g_PickerManager.GetEntity(i));
		if( pEnt && pEnt->GetIsTypePed() )
			pFocusPed = static_cast<CPed*>(pEnt);

		if (pFocusPed)
		{
			CTaskNMPrototype* pTask = static_cast<CTaskNMPrototype*>(pFocusPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_NM_PROTOTYPE));
			if (pTask)
			{
				pTask->Stop();
			}
		}
	}
}
#endif //__BANK

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskInfo* CTaskNMPrototype::CreateQueriableState() const
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	return rage_new CClonedNMPrototypeInfo(); 
}


CTaskFSMClone *CClonedNMPrototypeInfo::CreateCloneFSMTask()
{
	return rage_new CTaskNMPrototype(4000, 30000);
}


