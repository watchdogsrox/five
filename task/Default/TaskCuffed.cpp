//
// Task/Default/TaskCuffed.h
//
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

// Class header 
#include "Task/Default/TaskCuffed.h"

// Game headers
#include "Animation/Move.h"
#include "Animation/MovePed.h"
#include "Camera/CamInterface.h"
#include "Camera/Gameplay/GameplayDirector.h"
#include "Debug/DebugScene.h"
#include "Math/angmath.h"
#include "Network/NetworkInterface.h"
#include "Network/Objects/NetworkObjectMgr.h"
#include "network/players/NetGamePlayer.h"
#include "Network/Network.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Physics/gtaInst.h"
#include "Physics/RagdollConstraints.h"
#include "Pickups/PickupManager.h"
#include "Scene/World/GameWorld.h"
#include "Task/Animation/TaskAnims.h"
#include "Task/Combat/Cover/TaskCover.h"
#include "Task/Default/TaskArrest.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/Movement/TaskSeekEntity.h"
#include "Task/Motion/TaskMotionBase.h"
#include "Task/Physics/TaskBlendFromNM.h"
#include "Task/Vehicle/TaskExitVehicle.h"
#include "Task/Weapons/Gun/TaskAimGunOnFoot.h"
#include "Task/System/Tuning.h"
#include "Weapons/Components/WeaponComponentReloadData.h"
#include "Weapons/Weapon.h"
#include "Weapons/WeaponTypes.h"
#include "fwanimation/directorcomponentsyncedscene.h"
#include "Task/Default/ArrestHelpers.h"

#include "event/EventNetwork.h"
#include "event/EventGroup.h"

// Macro to disable optimisations if set
AI_OPTIMISATIONS()

#if ENABLE_TASKS_ARREST_CUFFED

////////////////////////////////////////////////////////////////////////////////

const fwMvFloatId CTaskCuffed::ms_HeadingDelta("HeadingDelta",0x232C8422);
const fwMvFloatId CTaskCuffed::ms_PairedAnimAngle("PairedAnimAngle",0x89c3c86c);
const fwMvFlagId CTaskCuffed::ms_ShoveRight("ShoveRight",0x9AB21799);
const fwMvFlagId CTaskCuffed::ms_UseFallback("UseFallback",0x9ED7F1F7);
const fwMvFlagId CTaskCuffed::ms_UseUncuff("UseUncuff",0x679B9D0B);
const fwMvFlagId CTaskCuffed::ms_UseNewAnims("UseNewAnims",0x53de5b58);
const fwMvBooleanId CTaskCuffed::ms_Phase1Complete("Phase1Complete",0xB031A936);
const fwMvBooleanId CTaskCuffed::ms_Phase2Complete("Phase2Complete",0x3940C543);
const fwMvBooleanId CTaskCuffed::ms_SetCuffedState("SetCuffedState",0x21033C6E);
const fwMvRequestId CTaskCuffed::ms_EnterP2Id("EnterP2",0x8e2f4397);

const strStreamingObjectName CTaskCuffed::ms_GetUpDicts[CTaskCuffed::ms_nMaxGetUpSets] = 
{
	strStreamingObjectName("mp_arrest_get_up_dir@back",0x17BA38E5),
	strStreamingObjectName("mp_arrest_get_up_dir@front",0x94C67FAE),
	strStreamingObjectName("mp_arrest_get_up_dir@left",0x7927963D),
	strStreamingObjectName("mp_arrest_get_up_dir@right",0xAFD30461),
};

CTaskCuffed::Tunables CTaskCuffed::sm_Tunables;
CTaskInCustody::Tunables CTaskInCustody::sm_Tunables;

IMPLEMENT_DEFAULT_TASK_TUNABLES(CTaskCuffed, 0xf29557c2);
IMPLEMENT_DEFAULT_TASK_TUNABLES(CTaskInCustody, 0x70bffd8e);

//////////////////////////////////////////////////////////////////////////
// CTaskCuffed
//////////////////////////////////////////////////////////////////////////

void CTaskCuffed::CreateTask(CPed* pTargetPed, CPed* pInstigatorPed, fwFlags8 iFlags)
{		
	Assert(pTargetPed && !pTargetPed->IsNetworkClone());

	if(iFlags.IsFlagSet(CF_TakeCustody))
	{
		ARREST_DISPLAYF(pInstigatorPed, pTargetPed, "CTaskCuffed::CreateTask - creating in custody task!\n");
		aiTask *pInCustodyTask = rage_new CTaskInCustody(pInstigatorPed);
		CEventGivePedTask event(PED_TASK_PRIORITY_PRIMARY, pInCustodyTask, false, E_PRIORITY_INCUSTODY, false, false, true);
		pTargetPed->GetPedIntelligence()->AddEvent(event, true);
	}
	else
	{
		ARREST_DISPLAYF(pInstigatorPed, pTargetPed, "CTaskCuffed::CreateTask - creating cuffed task!\n");
		aiTask *pCuffedTask = rage_new CTaskCuffed(pInstigatorPed, iFlags);
		CEventGivePedTask event(PED_TASK_PRIORITY_PRIMARY, pCuffedTask, false, E_PRIORITY_CUFFED, true, true);
		pTargetPed->GetPedIntelligence()->AddEvent(event, true);
	}
}

////////////////////////////////////////////////////////////////////////////////

CTaskCuffed::CTaskCuffed(CPed* pArrestPed, fwFlags8 iFlags)
: m_pArrestPed(pArrestPed)
, m_iFlags(iFlags)
, m_bPostedEvent(false)
{
	SetInternalTaskType(CTaskTypes::TASK_CUFFED);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskCuffed::CleanUp()
{

	CPed* pPed = GetPed();
	// make sure we've cleaned up the ragdoll when we exit the task (unless someone else is alreadty handling it)
	if (pPed->GetUsingRagdoll())
	{
		CTaskNMControl::CleanupUnhandledRagdoll(pPed);
	}

	// Blend out of move network
	if (m_moveNetworkHelper.GetNetworkPlayer())
	{
		GetPed()->GetMovePed().ClearTaskNetwork(m_moveNetworkHelper, SLOW_BLEND_DURATION);
		m_moveNetworkHelper.ReleaseNetworkPlayer();
	}

	m_ClipRequestHelper.ReleaseClips();
	m_ClipRequestHelperFull.ReleaseClips();

	if(!m_bPostedEvent)
	{
		CArrestHelpers::eArrestEvent arrestEvent = CArrestHelpers::EVENT_COUNT;

		if(GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_IsHandCuffed))
		{
			arrestEvent = m_iFlags.IsFlagSet(CF_UnCuff) ? CArrestHelpers::EVENT_UNCUFF_FAILED : CArrestHelpers::EVENT_CUFF_SUCCESSFUL;
		}
		else
		{
			arrestEvent = m_iFlags.IsFlagSet(CF_UnCuff) ? CArrestHelpers::EVENT_UNCUFF_SUCCESSFUL : CArrestHelpers::EVENT_CUFF_FAILED;
		}

		if(arrestEvent != CArrestHelpers::EVENT_COUNT)
		{
			GetEventScriptNetworkGroup()->Add(CEventNetworkPlayerArrest(m_pArrestPed, GetPed(), arrestEvent));
			m_bPostedEvent = true;
		}
	}

	ARREST_DISPLAYF(m_pArrestPed, GetPed(), "CTaskCuffed::CleanUp");
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskCuffed::ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
	if (iPriority != CTask::ABORT_PRIORITY_IMMEDIATE
		&& pEvent 
		//&& ((CEvent*)pEvent)->GetEventType() != EVENT_SCRIPT_COMMAND
		&& ((CEvent*)pEvent)->GetEventType() != EVENT_GIVE_PED_TASK
		&& ((CEvent*)pEvent)->GetEventType() != EVENT_IN_AIR)
	{
		return false;
	}

	ARREST_DISPLAYF(m_pArrestPed, GetPed(), "CTaskCuffed::ShouldAbort - aborting cuffed task! \n");
	if(pEvent)
	{
		ARREST_DISPLAYF(m_pArrestPed, GetPed(), "CTaskCuffed::ShouldAbort. Type = %d, Priority = %d, Task = %s\n", 
			pEvent->GetEventType(), 
			pEvent->GetEventPriority(), 
			pEvent->GetName().c_str());
	}

	return CTask::ShouldAbort(iPriority, pEvent);
}

////////////////////////////////////////////////////////////////////////////////

rage::s32 CTaskCuffed::GetDefaultStateAfterAbort() const
{
	return State_Restarted;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskCuffed::HandlesRagdoll(const CPed* UNUSED_PARAM(pPed)) const
{
	if(!m_iFlags.IsFlagSet(CF_UnCuff))
	{
		switch(GetState())
		{
		case(State_Start):
		case(State_WaitOnArrester):
		case(State_Fallback_Prepare):
		case(State_Full_Prepare):
		case(State_Full_Phase1):
		case(State_SyncedScene_Prepare):
			return true;
		default:
			break;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskCuffed::AddGetUpSets(CNmBlendOutSetList& sets, CPed* UNUSED_PARAM(pPed))
{
	CTaskArrest* pTaskArrest = GetArrestPedTask();
	if(pTaskArrest && pTaskArrest->HasChosenFull())
	{
		sets.Add(NMBS_FULL_ARREST_GETUPS);
	}
	else
	{
		sets.Add(NMBS_ARREST_GETUPS);
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskCuffed::ProcessPreFSM()
{
	GetPed()->SetPedResetFlag(CPED_RESET_FLAG_TemporarilyBlockWeaponSwitching, true);

	// Need to inform ped that we are in control of the ragdoll atm.
	if( (GetPed()->GetRagdollState() == RAGDOLL_STATE_PHYS) )
	{
		GetPed()->TickRagdollStateFromTask(*this);
	}

	//! Stop ped being pushed about.
	GetPed()->SetPedResetFlag(CPED_RESET_FLAG_TaskUseKinematicPhysics, true);

	if (!GetPed()->IsNetworkClone())
	{
		if(!m_pArrestPed)
		{
			ARREST_DISPLAYF(m_pArrestPed, GetPed(), "CTaskCuffed::ProcessPreFSM - lost arrest ped!\n");
			return FSM_Quit;
		}
		
		if(!IsArrestPedRunningArrestTask())
		{
			//! Exit cuffing state if we haven't managed to be cuffed yet.
			if(IsBeingCuffed() && !IsCuffed())
			{
				ARREST_DISPLAYF(m_pArrestPed, GetPed(), "CTaskCuffed::ProcessPreFSM - arrest ped stopped arresting!\n");
				return FSM_Quit;
			}
			//! Exit uncuffing state if we are still cuffed.
			else if(IsBeingUncuffed() && IsCuffed())
			{
				ARREST_DISPLAYF(m_pArrestPed, GetPed(), "CTaskCuffed::ProcessPreFSM - arrest ped stopped uncuffing!\n");
				return FSM_Quit;
			}
		}

		//! Sync scene is set to not abort, so we need to specifically check we are dead here to exit the task.
		if(GetPed()->IsDead())
		{
			ARREST_DISPLAYF(m_pArrestPed, GetPed(), "CTaskCuffed::ProcessPreFSM - ped is dead!\n");
			return FSM_Quit;
		}
		//! Arrest ped has died. Note: we wouldn't ordinarily exit the sync scene after we have been cuffed, but this is the exception.
		else if(m_pArrestPed && m_pArrestPed->IsDead())
		{
			ARREST_DISPLAYF(m_pArrestPed, GetPed(), "CTaskCuffed::ProcessPreFSM - arrest ped is dead!\n");
			return FSM_Quit;
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskCuffed::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		FSM_State(State_Start)
			FSM_OnEnter
				return Start_OnEnter();
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_WaitOnArrester)
			FSM_OnUpdate
				return WaitOnArrester_OnUpdate();

		FSM_State(State_SyncedScene_Prepare)
			FSM_OnEnter
				return SyncedScenePrepare_OnEnter();
			FSM_OnUpdate
				return SyncedScenePrepare_OnUpdate();

		FSM_State(State_SyncedScene_Playing)
			FSM_OnEnter
				return SyncedScenePlaying_OnEnter();
			FSM_OnUpdate
				return SyncedScenePlaying_OnUpdate();	

		FSM_State(State_Full_Prepare)
			FSM_OnEnter
				return FullPrepare_OnEnter();
			FSM_OnUpdate
				return FullPrepare_OnUpdate();	

		FSM_State(State_Full_Phase1)
			FSM_OnEnter
				return FullPhase1_OnEnter();
			FSM_OnUpdate
				return FullPhase1_OnUpdate();	

		FSM_State(State_Full_Phase2)
			FSM_OnEnter
				return FullPhase2_OnEnter();
			FSM_OnUpdate
				return FullPhase2_OnUpdate();	

		FSM_State(State_Full_Phase3)
			FSM_OnEnter
				return FullPhase3_OnEnter();
			FSM_OnUpdate
				return FullPhase3_OnUpdate();	

		FSM_State(State_Fallback_Prepare)
			FSM_OnEnter
				return FallbackPrepare_OnEnter();
			FSM_OnUpdate
				return FallbackPrepare_OnUpdate();

		FSM_State(State_Fallback_WaitForShove)
			FSM_OnUpdate
				return FallbackWaitForShove_OnUpdate();

		FSM_State(State_Fallback_Shoved)
			FSM_OnEnter
				return FallbackShoved_OnEnter();
			FSM_OnUpdate
				return FallbackShoved_OnUpdate();
			FSM_OnExit
				return FallbackShoved_OnExit();

		FSM_State(State_Cuffed)
			FSM_OnEnter
				return Cuffed_OnEnter();
			FSM_OnUpdate
				return Cuffed_OnUpdate();

		FSM_State(State_Uncuffed)
			FSM_OnEnter
				return Uncuffed_OnEnter();
			FSM_OnUpdate
				return Uncuffed_OnUpdate();

		FSM_State(State_Restarted)
			FSM_OnEnter
				return Restarted_OnEnter();
			FSM_OnUpdate
				return Restarted_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnEnter
				return Finish_OnEnter();
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskCuffed::Start_OnEnter()
{	
	ARREST_DISPLAYF(m_pArrestPed, GetPed(), "CTaskCuffed::Start_OnEnter - starting cuffed task!\n");

	CPed* pPed = GetPed();

	if (!pPed->IsNetworkClone())
	{
		// Holster the current weapon
		pPed->GetWeaponManager()->EquipWeapon( pPed->GetDefaultUnarmedWeaponHash(), -1, true);
	}

	// Force stealth off (B*511676)
	pPed->GetMotionData()->SetUsingStealth(false);

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskCuffed::Start_OnUpdate()
{
	// Request arrest anims.
	bool bFallBackAnimsLoaded = m_ClipRequestHelper.RequestClipsByIndex(fwAnimManager::FindSlotFromHashKey(CTaskArrest::GetArrestDict()).Get());

	bool bFullAnimsLoaded = false;
	// Request full arrest anims.
	if(m_iFlags.IsFlagSet(CF_UnCuff))
	{
		bFullAnimsLoaded = m_ClipRequestHelperFull.RequestClipsByIndex(fwAnimManager::FindSlotFromHashKey(CTaskArrest::GetFullUncuffDict()).Get());
	}
	else
	{
		bool bGetupAnimsLoaded = true;
		for(int i = 0; i < ms_nMaxGetUpSets; ++i)
		{
			if(!m_GetUpHelpers[i].RequestClipsByIndex(fwAnimManager::FindSlotFromHashKey(ms_GetUpDicts[i]).Get()))
				bGetupAnimsLoaded = false;
		}

		bool bFullArrestLoaded = m_ClipRequestHelperFull.RequestClipsByIndex(fwAnimManager::FindSlotFromHashKey(CTaskArrest::GetFullArrestDict()).Get());
	
		bFullAnimsLoaded = bGetupAnimsLoaded && bFullArrestLoaded;
	}

	if(bFallBackAnimsLoaded && bFullAnimsLoaded)
	{
		// Select the next state
		if (GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_IsHandCuffed))
		{
			if(m_iFlags.IsFlagSet(CF_UnCuff))
			{
				SetState(State_WaitOnArrester);
			}
			else
			{
				// Already cuffed - this task may be resuming or flag was manually set by script
				SetState(State_Cuffed);
			}
		}
		else
		{
			// Wait for the arrest ped to decide how to arrest us
			SetState(State_WaitOnArrester);
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskCuffed::WaitOnArrester_OnUpdate()
{
	CTaskArrest* pTaskArrest = GetArrestPedTask();

	// Move to correct state.
	if(pTaskArrest && (GetArrestPedState() >= CTaskArrest::State_Start) )
	{
		if (pTaskArrest->HasChosenSyncScene())
		{
			SetState(State_SyncedScene_Prepare);
			return FSM_Continue;
		}
		else if (pTaskArrest->HasChosenFallback())
		{
			SetState(State_Fallback_Prepare);
			return FSM_Continue;
		}
		else if(pTaskArrest->HasChosenFull())
		{
			SetState(State_Full_Prepare);
			return FSM_Continue;
		}
	}

	// Check arrester didn't exit out before we got to this point.
	if(!GetPed()->IsNetworkClone())
	{
		if(m_iFlags.IsFlagSet(CF_ArrestPedQuit))
		{
			ARREST_DISPLAYF(m_pArrestPed, GetPed(), "CTaskCuffed::WaitForArrest_OnUpdate - Arrester quit early!\n");
			SetState(State_Finish);
			return FSM_Continue;
		}

		// Time out check - arrester failed to do anything
		else if (GetTimeInState() > CArrestHelpers::GetArrestNetworkTimeout())
		{
			ARREST_DISPLAYF(m_pArrestPed, GetPed(), "CTaskCuffed::WaitForArrest_OnUpdate - fCurrentTime > %f\n", CArrestHelpers::GetArrestNetworkTimeout());
			SetState(State_Finish);
			return FSM_Continue;
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskCuffed::SyncedScenePrepare_OnEnter()
{
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskCuffed::SyncedScenePrepare_OnUpdate()
{
	if (m_ClipRequestHelper.HaveClipsBeenLoaded() && 
		GetArrestPedState() == CTaskArrest::State_SyncedScene_Playing)
	{
		SetState(State_SyncedScene_Playing);
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskCuffed::SyncedScenePlaying_OnEnter()
{
	CTaskArrest* pTaskArrest = GetArrestPedTask();
	taskAssert(pTaskArrest);

	// Disable ragdoll
	if(GetPed()->GetUsingRagdoll())
	{
		GetPed()->SwitchToAnimated(/*false, true, false, true*/);
	}

	eSyncedSceneFlagsBitSet flags;
	flags.BitSet().Set(SYNCED_SCENE_USE_KINEMATIC_PHYSICS);
	flags.BitSet().Set(SYNCED_SCENE_DONT_INTERRUPT);

	// Start the synced scene task
	if (taskVerifyf(pTaskArrest->GetArresteeClip(), "Failed to find arrestee clip!"))
	{
		CTaskSynchronizedScene* pSyncAnimTask = 
			rage_new CTaskSynchronizedScene(pTaskArrest->GetSyncedSceneId(), *pTaskArrest->GetArresteeClip(), pTaskArrest->GetArrestDict(), INSTANT_BLEND_IN_DELTA, NORMAL_BLEND_OUT_DELTA, flags);

		// Force an anim update
		GetPed()->SetPedResetFlag( CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true );
		GetPed()->SetPedResetFlag( CPED_RESET_FLAG_ForcePostCameraAIUpdate, true );

		SetNewTask(pSyncAnimTask);
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskCuffed::SyncedScenePlaying_OnUpdate()
{
	bool bFinished = false;
	if (m_iFlags.IsFlagSet(CF_ArrestPedFinishedHandshake))
	{
		//! This skips the arrest animation if the arrester has finished already. This is in preference to the arrester waiting
		//! for the cuffee to catch up. 
		bFinished = true;
	}
	else
	{
		CTaskArrest *pArrestTask = GetArrestPedTask();
		if(pArrestTask && fwAnimDirectorComponentSyncedScene::IsValidSceneId(pArrestTask->GetSyncedSceneId()))
		{
			const float phase = fwAnimDirectorComponentSyncedScene::GetSyncedScenePhase(pArrestTask->GetSyncedSceneId());

			fwAnimDirector* pDirector = GetPed()->GetAnimDirector();

			if (pDirector->GetComponent<fwAnimDirectorComponentSyncedScene>() && pDirector->GetComponent<fwAnimDirectorComponentSyncedScene>()->IsPlayingSyncedScene())
			{
				const crClip* pClip = pDirector->GetComponent<fwAnimDirectorComponentSyncedScene>()->GetSyncedSceneClip();
				float fInterruptPhase;
				if(CClipEventTags::FindEventPhase(pClip, CClipEventTags::Interruptible, fInterruptPhase))
				{
					if(phase >= fInterruptPhase)
					{
						m_iFlags.SetFlag(CF_CantInterruptSyncScene);

						if(m_iFlags.IsFlagSet(CF_UnCuff))
						{
							SetCuffedFlags(false);
						}
						else
						{
							SetCuffedFlags(true);
						}
					}
				}
			}

			if (phase == 1.0f)
			{
				bFinished = true;
			}
		}
		else
		{
			//! Check if we are already handcuffed. If so, just skip ahead.
			if(!GetPed()->IsNetworkClone() && m_iFlags.IsFlagSet(CF_CantInterruptSyncScene))
			{
				bFinished = true;
			}
		}
	}

	if(bFinished)
	{
		if(m_iFlags.IsFlagSet(CF_UnCuff))
		{
			SetState(State_Uncuffed);
		}
		else
		{
			SetState(State_Cuffed);
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskCuffed::FullPrepare_OnEnter()
{
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskCuffed::FullPrepare_OnUpdate()
{
	bool bGetupsLoaded = true;

	if(!m_iFlags.IsFlagSet(CF_UnCuff))
	{
		for(int i = 0; i < ms_nMaxGetUpSets; ++i)
		{
			if(!m_GetUpHelpers[i].HaveClipsBeenLoaded())
			{	
				bGetupsLoaded = false;
				break;
			}
		}

		if(bGetupsLoaded && (GetArrestPedState() > CTaskArrest::State_Full_Prepare))
		{
			SetState(State_Full_Phase1);
		}
	}
	else
	{
		if (m_moveNetworkHelper.RequestNetworkDef(CClipNetworkMoveInfo::ms_NetworkTaskCuffed) && m_ClipRequestHelperFull.HaveClipsBeenLoaded() )
		{
			m_moveNetworkHelper.CreateNetworkPlayer(GetPed(), CClipNetworkMoveInfo::ms_NetworkTaskCuffed);

			GetPed()->GetMovePed().SetTaskNetwork(m_moveNetworkHelper, NORMAL_BLEND_DURATION);
		
			SetState(State_Full_Phase1);
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskCuffed::FullPhase1_OnEnter()
{
	float fDesiredHeading = m_pArrestPed->GetCurrentHeading();

	CTaskArrest *pTaskArrest = GetArrestPedTask();
	if(pTaskArrest)
	{
		fDesiredHeading = pTaskArrest->GetDesiredCuffedPedGetupHeading();
	}

	if(!m_iFlags.IsFlagSet(CF_UnCuff))
	{
		//! Trigger arrest getup.
		CTaskBlendFromNM *pTaskBlendFromNM = rage_new CTaskBlendFromNM();
		SetNewTask(pTaskBlendFromNM);

		//! Our arrester will tell us our new heading. This is fed into the blend from NM task to orient the 
		//! cuffee correctly.
		GetPed()->SetDesiredHeading(fDesiredHeading);
	}
	else
	{
		if(m_moveNetworkHelper.IsNetworkActive())
		{
			float fCurrentHeading = GetPed()->GetCurrentHeading();
			fCurrentHeading = fwAngle::LimitRadianAngleSafe(fCurrentHeading);
			fDesiredHeading = fwAngle::LimitRadianAngleSafe(fDesiredHeading);

			float fAngle = SubtractAngleShorter(fDesiredHeading, fCurrentHeading);

			fAngle = Clamp( fAngle/PI, -1.0f, 1.0f);

			//! Convert to signal value (0->1). 0.0f is -180, 1.0 is +180.
			fAngle = ((-fAngle * 0.5f) + 0.5f);

			m_moveNetworkHelper.SetFloat(ms_HeadingDelta, fAngle);
			m_moveNetworkHelper.SetFlag(true, ms_UseUncuff);
		}
	}

	// Force an anim update
	GetPed()->SetPedResetFlag( CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true );
	GetPed()->SetPedResetFlag( CPED_RESET_FLAG_ForcePostCameraAIUpdate, true );

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskCuffed::FullPhase1_OnUpdate()
{
	if(!m_iFlags.IsFlagSet(CF_UnCuff))
	{
		float fDesiredHeading = m_pArrestPed->GetCurrentHeading();

		CTaskArrest *pTaskArrest = GetArrestPedTask();
		if(pTaskArrest)
		{
			fDesiredHeading = pTaskArrest->GetDesiredCuffedPedGetupHeading();
			GetPed()->SetDesiredHeading(fDesiredHeading);
		}

		if (m_moveNetworkHelper.RequestNetworkDef(CClipNetworkMoveInfo::ms_NetworkTaskCuffed) && m_ClipRequestHelperFull.HaveClipsBeenLoaded() )
		{
			bool bGetupFinished = GetIsFlagSet(aiTaskFlags::SubTaskFinished) || (!GetSubTask() && !GetNewTask());
			bool bArresterFinishedP1 = (GetArrestPedState() > CTaskArrest::State_Full_Phase1);

			//! Start network if p2 has finished getup, even if arresting ped isn't ready as we can keep the cuffed ped in an idle loop until 
			//! they are.
			if (bGetupFinished || bArresterFinishedP1 )
			{
				m_moveNetworkHelper.CreateNetworkPlayer(GetPed(), CClipNetworkMoveInfo::ms_NetworkTaskCuffed);

				// Start the animation
				GetPed()->GetMovePed().SetTaskNetwork(m_moveNetworkHelper, NORMAL_BLEND_DURATION);

				m_moveNetworkHelper.SetFlag(CArrestHelpers::UsingNewArrestAnims(), ms_UseNewAnims);
			}

			//! Move to next phase only when P1 is ready to keep them in sync.
			if(bArresterFinishedP1)
			{
				TUNE_GROUP_BOOL(ARREST, bTestPairedAnimsOffset, false);
				if(bTestPairedAnimsOffset)
				{
					Matrix34 m = MAT34V_TO_MATRIX34(GetArrestPed()->GetMatrix());

					TUNE_GROUP_FLOAT(ARREST, fCuffedTestOffsetY, 1.1f, -5.0f, 5.0f, 0.01f);
					TUNE_GROUP_FLOAT(ARREST, fCuffedTestOffsetX, 0.0f, -5.0f, 5.0f, 0.01f);

					Vector3 v = VEC3V_TO_VECTOR3(GetArrestPed()->GetTransform().GetPosition());
					v += (VEC3V_TO_VECTOR3(GetArrestPed()->GetTransform().GetForward()) * fCuffedTestOffsetY);
					v += (VEC3V_TO_VECTOR3(GetArrestPed()->GetTransform().GetRight()) * fCuffedTestOffsetX);
					m.MakeTranslate(v);

					m.MakeRotateZ(pTaskArrest->GetDesiredCuffedPedGetupHeading());

					GetPed()->SetMatrix(m);
				}

				CTaskArrest *pTaskArrest = GetArrestPedTask();
				if(pTaskArrest)
				{
					m_moveNetworkHelper.SetFloat(ms_PairedAnimAngle, pTaskArrest->GetPairedAnimAngle());
				}

				SetState(State_Full_Phase2);
			}
		}
	}
	else
	{
		if ((m_moveNetworkHelper.IsNetworkActive() && m_moveNetworkHelper.GetBoolean(ms_Phase1Complete)) || (GetArrestPedState() > CTaskArrest::State_Full_Phase1))
		{
			SetState(State_Full_Phase2);
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskCuffed::FullPhase2_OnEnter()
{
	if(!m_iFlags.IsFlagSet(CF_UnCuff))
	{
		if(m_moveNetworkHelper.IsNetworkActive())
		{
			m_moveNetworkHelper.SendRequest(ms_EnterP2Id);
			
			float fDesiredHeading = m_pArrestPed->GetCurrentHeading();
			
			if(CArrestHelpers::UsingNewArrestAnims())
			{
				CTaskArrest *pTaskArrest = GetArrestPedTask();
				if(pTaskArrest)
				{
					fDesiredHeading = pTaskArrest->GetDesiredCuffedPedGetupHeading();
				}
			}

			if(m_pArrestPed)
			{
				float fCurrentHeading = GetPed()->GetCurrentHeading();
				fCurrentHeading = fwAngle::LimitRadianAngleSafe(fCurrentHeading);
				fDesiredHeading = fwAngle::LimitRadianAngleSafe(fDesiredHeading);

				float fAngle = SubtractAngleShorter(fDesiredHeading, fCurrentHeading);

				fAngle = Clamp(fAngle/(PI*0.5f), -1.0f, 1.0f);

				//! Convert to signal value (0->1). 0.0f is -90, 1.0 is +90.
				fAngle = ((-fAngle * 0.5f) + 0.5f);

				m_moveNetworkHelper.SetFloat(ms_HeadingDelta, fAngle);
			}
			else
			{
				m_moveNetworkHelper.SetFloat(ms_HeadingDelta, 0.5f);
			}
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskCuffed::FullPhase2_OnUpdate()
{
	s32 iNextState = CheckAndSetCuffedState();
	if(iNextState != -1)
	{
		SetState(iNextState);
		return FSM_Continue;
	}

	if (m_moveNetworkHelper.IsNetworkActive() && m_moveNetworkHelper.GetBoolean(ms_Phase2Complete))
	{
		SetState(State_Full_Phase3);
		return FSM_Continue;
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskCuffed::FullPhase3_OnEnter()
{
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskCuffed::FullPhase3_OnUpdate()
{
	s32 iNextState = CheckAndSetCuffedState();
	if(iNextState != -1)
	{
		SetState(iNextState);
		return FSM_Continue;
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskCuffed::FallbackPrepare_OnEnter()
{
	if (GetPed()->GetUsingRagdoll())
	{
		// Get up while waiting
		CTaskBlendFromNM *pTaskBlendFromNM = rage_new CTaskBlendFromNM();
		SetNewTask(pTaskBlendFromNM);
	}

	return FSM_Continue;
}


////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskCuffed::FallbackPrepare_OnUpdate()
{
	// Load the move network to handle cuffing animations
	m_moveNetworkHelper.RequestNetworkDef(CClipNetworkMoveInfo::ms_NetworkTaskCuffed);

	//  Wait for network to stream in
	if (m_moveNetworkHelper.IsNetworkDefStreamedIn(CClipNetworkMoveInfo::ms_NetworkTaskCuffed) && m_ClipRequestHelper.HaveClipsBeenLoaded())
	{
		// Wait for blend to finish
		if (GetIsFlagSet(aiTaskFlags::SubTaskFinished) || (!GetSubTask() && !GetNewTask()))
		{
			SetState(State_Fallback_WaitForShove);
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskCuffed::FallbackWaitForShove_OnUpdate()
{
	if (GetArrestPedState() == CTaskArrest::State_Fallback_MoveToTarget)
	{
		// Try to turn and face the arrest ped while they're moving to us
		GetPed()->SetDesiredHeading(VEC3V_TO_VECTOR3(m_pArrestPed->GetTransform().GetPosition()));
	}
	else
	{
		// Don't change heading
		GetPed()->SetDesiredHeading(GetPed()->GetCurrentHeading());
	}

	// Wait for arrest ped to shove us
	if (GetArrestPedState() >= CTaskArrest::State_Fallback_Shoving)
	{
		SetState(State_Fallback_Shoved);
	}
	else if (!GetPed()->IsNetworkClone() && (GetTimeInState() > 5.0f))
	{
		// Timed out waiting for arrest ped
		ARREST_DISPLAYF(m_pArrestPed, GetPed(), "CTaskCuffed::WaitForShove_OnUpdate - Timed Out");
		SetState(State_Finish);
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskCuffed::FallbackShoved_OnEnter()
{
	// Start the animation
	m_moveNetworkHelper.CreateNetworkPlayer(GetPed(), CClipNetworkMoveInfo::ms_NetworkTaskCuffed);
	GetPed()->GetMovePed().SetTaskNetwork(m_moveNetworkHelper, INSTANT_BLEND_DURATION);//, CMovePed::Task_TagSyncContinuous | CMovePed::Task_TagSyncTransition);

	const float fPedHeading = GetPed()->GetCurrentHeading();
	const float fArrestPedHeading = m_pArrestPed->GetCurrentHeading();
	const float fHeadingDelta = SubtractAngleShorter(fArrestPedHeading, fPedHeading);
	const bool bShoveRight = fHeadingDelta >= 0.0f;
	const float fHeadingDeltaAbs = fabs(fHeadingDelta);

	// Setup move network parameters
	m_moveNetworkHelper.SetFloat(ms_HeadingDelta, fHeadingDeltaAbs / PI);
	m_moveNetworkHelper.SetFlag(bShoveRight, ms_ShoveRight);
	m_moveNetworkHelper.SetFlag(true, ms_UseFallback);

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskCuffed::FallbackShoved_OnUpdate()
{
	// Wait for arrest ped to cuff us
	if (m_iFlags.IsFlagSet(CF_ArrestPedFinishedHandshake))
	{
		SetState(State_Cuffed);
	}
	else if (!GetPed()->IsNetworkClone() && (GetTimeInState() > 5.0f))
	{
		// Timed out waiting for arrest ped
		ARREST_DISPLAYF(m_pArrestPed, GetPed(), "CTaskCuffed::Shoved_OnUpdate - Timed Out");
		SetState(State_Finish);
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskCuffed::FallbackShoved_OnExit()
{
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskCuffed::Cuffed_OnEnter()
{
	SetCuffedFlags(true);

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskCuffed::Cuffed_OnUpdate()
{
	if(!GetPed()->IsNetworkClone())
	{
		ARREST_DISPLAYF(m_pArrestPed, GetPed(), "CTaskCuffed::Cuffed_OnUpdate - Finished Cuffing");
		SetState(State_Finish);
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskCuffed::Uncuffed_OnEnter()
{
	SetCuffedFlags(false);

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskCuffed::Uncuffed_OnUpdate()
{
	if(!GetPed()->IsNetworkClone())
	{
		ARREST_DISPLAYF(m_pArrestPed, GetPed(), "CTaskCuffed::Uncuffed_OnUpdate - Finished UnCuffing");
		SetState(State_Finish);
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskCuffed::Restarted_OnEnter()
{
	return FSM_Continue;
}

CTask::FSM_Return CTaskCuffed::Restarted_OnUpdate()
{
	//! If we are still handcuffed after the restart, go back to cuffed state. If not, just finish.
	if (GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_IsHandCuffed))
	{
		SetState(State_Cuffed);
	}
	else
	{
		ARREST_DISPLAYF(m_pArrestPed, GetPed(), "CTaskCuffed::Restarted_OnUpdate - Finished after restarting");
		SetState(State_Finish);
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskCuffed::Finish_OnEnter()
{
	ARREST_DISPLAYF(m_pArrestPed, GetPed(), "CTaskCuffed::Finish_OnEnter");
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

s32 CTaskCuffed::CheckAndSetCuffedState()
{
	s32 iNextState = -1;
	if(m_moveNetworkHelper.GetBoolean(ms_SetCuffedState))
	{
		if(m_iFlags.IsFlagSet(CF_UnCuff))
		{
			SetCuffedFlags(false);
		}
		else
		{
			SetCuffedFlags(true);
		}
	}

	// Wait for arrest ped to cuff us
	if (m_iFlags.IsFlagSet(CF_ArrestPedFinishedHandshake))
	{
		if(m_iFlags.IsFlagSet(CF_UnCuff))
		{
			iNextState = State_Uncuffed;
		}
		else
		{
			iNextState = State_Cuffed;
		}
	}

	return iNextState;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskCuffed::SetCuffedFlags(bool enabled)
{
	CPed* pPed = GetPed();
	if(!pPed->IsNetworkClone())
	{
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_IsHandCuffed, enabled);
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskCuffed::IsArrestPedRunningArrestTask()
{
	if (m_pArrestPed)
	{
		const CTask* pTaskArrest = m_pArrestPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ARREST);
		if (pTaskArrest)
		{
			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

CTaskArrest* CTaskCuffed::GetArrestPedTask()
{
	if (m_pArrestPed)
	{
		return static_cast<CTaskArrest*>(m_pArrestPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ARREST));
	}

	return NULL;	
}

////////////////////////////////////////////////////////////////////////////////

s32 CTaskCuffed::GetArrestPedState()
{
	if (m_pArrestPed)
	{
		const CTaskArrest* pTaskArrest = static_cast<const CTaskArrest*>(m_pArrestPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ARREST));
		if (pTaskArrest)
		{
			return pTaskArrest->GetState();
		}
	}

	return -1;	
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskCuffed::IsReceivingSyncedScene() const
{ 
	if(!m_iFlags.IsFlagSet(CF_ArrestPedFinishedHandshake))
	{
		switch(GetState())
		{
		case(State_SyncedScene_Prepare):
		case(State_SyncedScene_Playing):
			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskCuffed::IsReceivingFullArrest() const
{ 
	if(!m_iFlags.IsFlagSet(CF_ArrestPedFinishedHandshake))
	{
		switch(GetState())
		{
		case(State_Full_Prepare):
		case(State_Full_Phase1):
		case(State_Full_Phase2):
		case(State_Full_Phase3):
			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskCuffed::IsReceivingFallbackArrest() const 
{ 
	if(!m_iFlags.IsFlagSet(CF_ArrestPedFinishedHandshake))
	{
		switch(GetState())
		{
		case(State_Fallback_Prepare):
		case(State_Fallback_WaitForShove):
		case(State_Fallback_Shoved):
			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskCuffed::IsBeingCuffed() const 
{
	return !m_iFlags.IsFlagSet(CF_UnCuff) && (IsReceivingSyncedScene() || IsReceivingFullArrest() || IsReceivingFallbackArrest()); 
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskCuffed::IsCuffed() const 
{ 
	return GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_IsHandCuffed);
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskCuffed::IsBeingUncuffed() const 
{ 
	return m_iFlags.IsFlagSet(CF_UnCuff) && (IsReceivingSyncedScene() || IsReceivingFullArrest());
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskCuffed::IsInSyncWithNetworkState() const
{
	return GetStateFromNetwork() == GetState();
}

////////////////////////////////////////////////////////////////////////////////

float CTaskCuffed::GetPhase() const
{
	switch(GetState())
	{
	case(State_Cuffed):
	case(State_Uncuffed):
		return 1.0f;
	default:
		{
			if(m_pArrestPed)
			{
				CTaskArrest* pTaskArrest = static_cast<CTaskArrest*>(m_pArrestPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ARREST));
				if(pTaskArrest)
				{
					return pTaskArrest->GetPhase();
				}
			}
		}
		break;
	}

	return 0.0f;
}

bool CTaskCuffed::CanStartSecondaryCuffedAnims() const 
{
	//! TO DO - need a quit state that allows us to start secondary cuffed task early to cover transition (so out arms don't seperate).
	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskCuffed::OnArrestPedFinishedHandshake() 
{
	ARREST_DISPLAYF(m_pArrestPed, GetPed(), "CTaskCuffed::OnArrestPedFinished()\n");
	m_iFlags.SetFlag(CF_ArrestPedFinishedHandshake); 
}

////////////////////////////////////////////////////////////////////////////////

void CTaskCuffed::OnArrestPedQuit() 
{
	ARREST_DISPLAYF(m_pArrestPed, GetPed(), "CTaskCuffed::OnArrestPedQuit()\n");
	m_iFlags.SetFlag(CF_ArrestPedQuit); 
}

////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
const char * CTaskCuffed::GetStaticStateName( s32 iState )
{
	switch (iState)
	{
		case State_Start:						return "State_Start";
		case State_WaitOnArrester:				return "State_WaitOnArrester";
		case State_SyncedScene_Prepare:			return "State_SyncedScene_Prepare";
		case State_SyncedScene_Playing:			return "State_SyncedScene_Playing";
		case State_Full_Prepare:				return "State_Full_Prepare";
		case State_Full_Phase1:					return "State_Full_Phase1";
		case State_Full_Phase2:					return "State_Full_Phase2";
		case State_Full_Phase3:					return "State_Full_Phase3";
		case State_Fallback_Prepare:			return "State_PrepareFallback";
		case State_Fallback_WaitForShove:		return "State_Fallback_WaitForShove";
		case State_Fallback_Shoved:				return "State_Fallback_Shoved";
		case State_Cuffed:						return "State_Cuffed";
		case State_Uncuffed:					return "State_Uncuffed";
		case State_Finish:						return "State_Finish";
	}

	return "State_Invalid";
}
#endif // !__FINAL

//////////////////////////////////////////////////////////////////////////
// CTaskCuffed Clone Functions
//////////////////////////////////////////////////////////////////////////

CTaskCuffed::FSM_Return	CTaskCuffed::UpdateClonedFSM(const s32 iState, const FSM_Event iEvent)
{
	if (iEvent == OnUpdate && GetStateFromNetwork() != GetState())
	{
		// Sync with network
		switch (GetStateFromNetwork())
		{
		case State_Cuffed:
			SetState(State_Cuffed);
			break;

		case State_Uncuffed:
			SetState(State_Uncuffed);
			break;

		case State_Finish:
			{
				ARREST_DISPLAYF(m_pArrestPed, GetPed(), "CTaskCuffed::UpdateClonedFSM - setting state from network");
				SetState(State_Finish);
			}
			break;
		}
	}

	return UpdateFSM(iState, iEvent);
}

////////////////////////////////////////////////////////////////////////////////

CTaskInfo* CTaskCuffed::CreateQueriableState() const
{
	return rage_new CClonedTaskCuffedInfo(GetState(), m_pArrestPed, m_iFlags);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskCuffed::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	Assert(pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_CUFFED);
	CClonedTaskCuffedInfo* pCuffedInfo = static_cast<CClonedTaskCuffedInfo*>(pTaskInfo);

	m_pArrestPed = pCuffedInfo->GetArrestPed();
	m_iFlags = pCuffedInfo->GetFlags();

	CTaskFSMClone::ReadQueriableState(pTaskInfo);	
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskCuffed::ControlPassingAllowed(CPed *UNUSED_PARAM(pPed), const netPlayer& UNUSED_PARAM(player), eMigrationType UNUSED_PARAM(migrationType))
{
	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskCuffed::OnCloneTaskNoLongerRunningOnOwner()
{
	ARREST_DISPLAYF(m_pArrestPed, GetPed(), "CTaskCuffed::OnCloneTaskNoLongerRunningOnOwner");
	SetStateFromNetwork(State_Finish);
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskCuffed::OverridesNetworkBlender(CPed *UNUSED_PARAM(pPed))
{
	//! Don't move ped until we have selected arrest type.
	if(GetState() <= State_WaitOnArrester)
	{
		return true;
	}

	//! If receiving new full arrest anims, override, as we don't sync the paired arrest anims. They are allowed to run locally 
	//! (as it doesn't really matter about angle of arrest etc).
	if(CArrestHelpers::UsingNewArrestAnims())
	{
		return !m_iFlags.IsFlagSet(CF_UnCuff) && IsReceivingFullArrest();
	}

	return !m_iFlags.IsFlagSet(CF_UnCuff) && GetState() == State_Full_Phase1;
}

//////////////////////////////////////////////////////////////////////////
// CClonedTaskCuffedInfo
//////////////////////////////////////////////////////////////////////////

CClonedTaskCuffedInfo::CClonedTaskCuffedInfo(s32 iState, CPed* pArrestPed, u8 iFlags)
: m_ArrestPedId((pArrestPed && pArrestPed->GetNetworkObject()) ? pArrestPed->GetNetworkObject()->GetObjectID() : NETWORK_INVALID_OBJECT_ID)
, m_iFlags(iFlags)
{
	CClonedFSMTaskInfo::SetStatusFromMainTaskState(iState);
}

CClonedTaskCuffedInfo::CClonedTaskCuffedInfo()
{}

CClonedTaskCuffedInfo::~CClonedTaskCuffedInfo()
{}

CTaskFSMClone *CClonedTaskCuffedInfo::CreateCloneFSMTask()
{
	return rage_new CTaskCuffed(GetArrestPed(), m_iFlags);
}

CPed* CClonedTaskCuffedInfo::GetArrestPed()
{
	return NetworkUtils::GetPedFromNetworkObject(NetworkInterface::GetObjectManager().GetNetworkObject(m_ArrestPedId));
}

////////////////////////////////////////////////////////////////////////////////

CTaskInCustody::CTaskInCustody(CPed* pCustodianPed, bool bSkipToInCustody)
: m_pCustodianPed(pCustodianPed)
, m_bCustodianPedQuit(false)
, m_bSkipToInCustody(bSkipToInCustody)
{
	SetInternalTaskType(CTaskTypes::TASK_IN_CUSTODY);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskInCustody::CleanUp()
{
	ARREST_DISPLAYF(m_pCustodianPed, GetPed(), "CTaskInCustody::CleanUp\n");
}

////////////////////////////////////////////////////////////////////////////////

CTaskArrest *CTaskInCustody::GetCustodianTask() const
{
	if (m_pCustodianPed)
	{
		return static_cast<CTaskArrest*>(m_pCustodianPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ARREST));
	}

	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskInCustody::IsBeingTakenIntoCustody() const
{
	return GetState() == State_BeingPutInCustody;
}

////////////////////////////////////////////////////////////////////////////////

float CTaskInCustody::GetPhase() const
{
	if(GetState() == State_InCustody)
	{
		return 1.0f;
	}
	else
	{
		CTaskArrest* pTaskArrest = GetCustodianTask();
		if(pTaskArrest)
		{
			return pTaskArrest->GetPhase();
		}
	}

	return 0.0f;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskInCustody::ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
	if (iPriority != CTask::ABORT_PRIORITY_IMMEDIATE
		&& pEvent 
		//&& ((CEvent*)pEvent)->GetEventType() != EVENT_SCRIPT_COMMAND
		&& ((CEvent*)pEvent)->GetEventType() != EVENT_GIVE_PED_TASK
		&& ((CEvent*)pEvent)->GetEventType() != EVENT_IN_AIR)
	{
		return false;
	}

	ARREST_DISPLAYF(m_pCustodianPed, GetPed(), "CTaskInCustody::ShouldAbort - aborting in custody task (State = %d)! \n", GetState());
	if(pEvent)
	{
		ARREST_DISPLAYF(m_pCustodianPed, GetPed(), "CTaskInCustody::ShouldAbort. Type = %d, Priority = %d, Task = %s\n", 
			pEvent->GetEventType(), 
			pEvent->GetEventPriority(), 
			pEvent->GetName().c_str());
	}

	return CTask::ShouldAbort(iPriority, pEvent);
}

////////////////////////////////////////////////////////////////////////////////

rage::s32 CTaskInCustody::GetDefaultStateAfterAbort() const
{
	return State_Start;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskInCustody::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		FSM_State(State_Start)
			FSM_OnEnter
				return Start_OnEnter();
		FSM_OnUpdate
				return Start_OnUpdate();	

		FSM_State(State_BeingPutInCustody)
			FSM_OnUpdate
				return BeingPutInCustody_OnUpdate();

		FSM_State(State_InCustody)
			FSM_OnEnter
				return InCustody_OnEnter();
			FSM_OnUpdate
				return InCustody_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnEnter
				return Finish_OnEnter();
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

CTask::FSM_Return CTaskInCustody::Start_OnEnter()
{
	if(!GetPed()->IsNetworkClone())
	{
		GetPed()->SetCustodian(m_pCustodianPed);
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskInCustody::Start_OnUpdate()
{
	if(!m_pCustodianPed)
	{
		ARREST_DISPLAYF(m_pCustodianPed, GetPed(), "CTaskInCustody::Start_OnUpdate - no custodian ped!\n");
		SetState(State_Finish);
		return FSM_Continue;
	}
	
	if(!GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_IsHandCuffed))
	{
		ARREST_DISPLAYF(m_pCustodianPed, GetPed(), "CTaskInCustody::Start_OnUpdate - not handcuffed!\n");
		SetState(State_Finish);
		return FSM_Continue;
	}

	if(m_bSkipToInCustody)
	{
		if(GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_IsInCustody))
		{
			SetState(State_InCustody);
		}
		else
		{
			SetState(State_Finish);
		}
	}
	else if(GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_IsInCustody))
	{
		SetState(State_InCustody);
	}
	else if(GetCustodianTask())
	{
		SetState(State_BeingPutInCustody);
	}
	else 
	{
		// Check arrester didn't exit out before we got to this point.
		if(!GetPed()->IsNetworkClone() && m_bCustodianPedQuit)
		{
			ARREST_DISPLAYF(m_pCustodianPed, GetPed(), "CTaskInCustody::Start_OnUpdate - Custodian quit early!\n");
			SetState(State_Finish);
		}
		// Time out check - arrester failed to do anything
		else if (!GetPed()->IsNetworkClone() && GetTimeInState() > CArrestHelpers::GetArrestNetworkTimeout())
		{
			ARREST_DISPLAYF(m_pCustodianPed, GetPed(), "CTaskInCustody::Start_OnUpdate - fCurrentTime > %f\n", CArrestHelpers::GetArrestNetworkTimeout());
			SetState(State_Finish);
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskInCustody::BeingPutInCustody_OnUpdate()
{
	if(!GetPed()->IsNetworkClone())
	{
		if(GetPhase() >= 1.0f || GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_IsInCustody))
		{
			ARREST_DISPLAYF(m_pCustodianPed, GetPed(), "CTaskInCustody::BeingPutInCustody_OnUpdate - arrest ped is dead!\n");
			SetState(State_InCustody);
		}
		else if(!GetCustodianTask())
		{
			ARREST_DISPLAYF(m_pCustodianPed, GetPed(), "CTaskInCustody::BeingPutInCustody_OnUpdate - custodian task has ended prematurely!\n");
			SetState(State_Finish);
		}
		else if(!GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_IsHandCuffed))
		{
			ARREST_DISPLAYF(m_pCustodianPed, GetPed(), "CTaskInCustody::BeingPutInCustody_OnUpdate - not handcuffed!\n");
			SetState(State_Finish);
		}
		else if(m_pCustodianPed && m_pCustodianPed->IsDead())
		{
			ARREST_DISPLAYF(m_pCustodianPed, GetPed(), "CTaskInCustody::BeingPutInCustody_OnUpdate - custodian ped is dead!\n");
			SetState(State_Finish);
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskInCustody::InCustody_OnEnter()
{
	if (!GetPed()->IsNetworkClone())
	{
		if(m_bSkipToInCustody && !GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_IsInCustody))
		{
			taskAssert(0);
		}

		GetPed()->SetInCustody(true, m_pCustodianPed);

		float fOverride = GetPed()->GetCustodyFollowDistanceOverride();
		float fFollowDistance = fOverride > 0.0f ? fOverride : sm_Tunables.m_AbandonDistance;

		// Follow the leader
		CTaskFollowLeaderInFormation* pSubTask = rage_new CTaskFollowLeaderInFormation(NULL, m_pCustodianPed, fFollowDistance, -1);
		SetNewTask(pSubTask);
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskInCustody::InCustody_OnUpdate()
{
	if (!GetPed()->IsNetworkClone())
	{
		//! Update follow distance.
		if(GetSubTask())
		{
			taskAssert(GetSubTask()->GetTaskType() == CTaskTypes::TASK_FOLLOW_LEADER_IN_FORMATION);
			CTaskFollowLeaderInFormation *pFollowTask = static_cast<CTaskFollowLeaderInFormation*>(GetSubTask());
			taskAssert(pFollowTask);
			float fOverride = GetPed()->GetCustodyFollowDistanceOverride();
			float fFollowDistance = fOverride > 0.0f ? fOverride : sm_Tunables.m_AbandonDistance;
			pFollowTask->SetMaxDistanceToLeader(fFollowDistance);
		}

		// Check if uncuffed
		if (!GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_IsHandCuffed))
		{
			ARREST_DISPLAYF(m_pCustodianPed, GetPed(), "CTaskInCustody::InCustody_OnUpdate - no handcuffs, removing from custody!\n");
			GetEventScriptNetworkGroup()->Add(CEventNetworkPlayerArrest(m_pCustodianPed, GetPed(), CArrestHelpers::EVENT_CUSTODY_EXIT_UNCUFFED));
			SetState(State_Finish);
		}
		// Check if the custodian has abandoned us
		else if(m_pCustodianPed == NULL || !GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_IsInCustody))
		{
			ARREST_DISPLAYF(m_pCustodianPed, GetPed(), "CTaskInCustody::InCustody_OnUpdate - lost custodian, removing from custody!\n");
			GetEventScriptNetworkGroup()->Add(CEventNetworkPlayerArrest(m_pCustodianPed, GetPed(), CArrestHelpers::EVENT_CUSTODY_EXIT_ABANDONED));
			SetState(State_Finish);
		}
		// Check if the custodian has died.
		else if(m_pCustodianPed->IsDead())
		{
			ARREST_DISPLAYF(m_pCustodianPed, GetPed(), "CTaskInCustody::InCustody_OnUpdate - custodian dead, removing from custody!\n");
			GetEventScriptNetworkGroup()->Add(CEventNetworkPlayerArrest(m_pCustodianPed, GetPed(), CArrestHelpers::EVENT_CUSTODY_EXIT_CUSTODIAN_DEAD));
			SetState(State_Finish);
		}
		// Check if follow task has finished.
		else if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
		{
			ARREST_DISPLAYF(m_pCustodianPed, GetPed(), "CTaskInCustody::InCustody_OnUpdate - follow task out of range, removing from custody!\n");
			GetEventScriptNetworkGroup()->Add(CEventNetworkPlayerArrest(m_pCustodianPed, GetPed(), CArrestHelpers::EVENT_CUSTODY_EXIT_CUSTODIAN_OUTOFRANGE));
			SetState(State_Finish);
		}
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskInCustody::Finish_OnEnter()
{
	if( !m_bSkipToInCustody && ((GetPreviousState() == State_BeingPutInCustody) || (GetPreviousState() == State_Start)) )
	{
		GetEventScriptNetworkGroup()->Add(CEventNetworkPlayerArrest(m_pCustodianPed, GetPed(), CArrestHelpers::EVENT_TAKENCUSTODY_FAILED));
	}

	ARREST_DISPLAYF(m_pCustodianPed, GetPed(), "CTaskInCustody::Finish_OnEnter\n");

	if(!GetPed()->IsNetworkClone())
	{
		GetPed()->SetInCustody(false, NULL);
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
const char * CTaskInCustody::GetStaticStateName( s32 iState )
{
	switch (iState)
	{
	case State_Start:						return "State_Start";
	case State_BeingPutInCustody:			return "State_BeingPutInCustody";
	case State_InCustody:					return "State_InCustody";
	case State_Finish:						return "State_Finish";
	}

	return "State_Invalid";
}
#endif // !__FINAL

//////////////////////////////////////////////////////////////////////////
// CTaskInCustody Clone Functions
//////////////////////////////////////////////////////////////////////////

CTaskInCustody::FSM_Return	CTaskInCustody::UpdateClonedFSM(const s32 iState, const FSM_Event iEvent)
{
	if (iEvent == OnUpdate)
	{
		if(GetStateFromNetwork() != GetState())
		{
			// Sync with network
			switch (GetStateFromNetwork())
			{
			case State_InCustody:
				SetState(State_InCustody);
				break;
			case State_Finish:
				SetState(State_Finish);
				break;
			}
		}

		UpdateClonedSubTasks(GetPed(), GetState());
	}

	return UpdateFSM(iState, iEvent);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskInCustody::UpdateClonedSubTasks(CPed* pPed, int const UNUSED_PARAM(iState))
{
	if(pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_IN_CUSTODY))
	{
		if(pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE))
		{
			CTaskFSMClone::CreateSubTaskFromClone(pPed, CTaskTypes::TASK_ENTER_VEHICLE, false);
		}
		else if(pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE))
		{
			CTaskFSMClone::CreateSubTaskFromClone(pPed, CTaskTypes::TASK_EXIT_VEHICLE, false);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

CTaskFSMClone* CTaskInCustody::CreateTaskForClonePed(CPed* UNUSED_PARAM(pPed))
{
	return rage_new CTaskInCustody(m_pCustodianPed, GetState() == State_InCustody);
}

////////////////////////////////////////////////////////////////////////////////

CTaskFSMClone* CTaskInCustody::CreateTaskForLocalPed(CPed* UNUSED_PARAM(pPed))
{
	return rage_new CTaskInCustody(m_pCustodianPed, GetState() == State_InCustody);
}

////////////////////////////////////////////////////////////////////////////////

CTaskInfo* CTaskInCustody::CreateQueriableState() const
{
	return rage_new CClonedTaskInCustodyInfo(GetState(), m_pCustodianPed);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskInCustody::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	Assert(pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_IN_CUSTODY);
	CClonedTaskInCustodyInfo* pInCustodyInfo = static_cast<CClonedTaskInCustodyInfo*>(pTaskInfo);

	m_pCustodianPed = pInCustodyInfo->GetCustodianPed();

	CTaskFSMClone::ReadQueriableState(pTaskInfo);	
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskInCustody::ControlPassingAllowed(CPed *UNUSED_PARAM(pPed), const netPlayer& player, eMigrationType UNUSED_PARAM(migrationType))
{
	const CPed *pPedToPassTo = static_cast<const CNetGamePlayer&>(player).GetPlayerPed();
	if(pPedToPassTo)
	{
		if(pPedToPassTo == m_pCustodianPed)
		{
			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskInCustody::OnCloneTaskNoLongerRunningOnOwner()
{
	SetStateFromNetwork(State_Finish);
}

//////////////////////////////////////////////////////////////////////////
// CClonedTaskInCustodyInfo
//////////////////////////////////////////////////////////////////////////

CClonedTaskInCustodyInfo::CClonedTaskInCustodyInfo(s32 iState, CPed* pCustodianPed)
: m_CustodianPedId((pCustodianPed && pCustodianPed->GetNetworkObject()) ? pCustodianPed->GetNetworkObject()->GetObjectID() : NETWORK_INVALID_OBJECT_ID)
{
	CClonedFSMTaskInfo::SetStatusFromMainTaskState(iState);
}

////////////////////////////////////////////////////////////////////////////////

CClonedTaskInCustodyInfo::CClonedTaskInCustodyInfo()
{}

////////////////////////////////////////////////////////////////////////////////

CClonedTaskInCustodyInfo::~CClonedTaskInCustodyInfo()
{}

////////////////////////////////////////////////////////////////////////////////

CPed* CClonedTaskInCustodyInfo::GetCustodianPed()
{
	return NetworkUtils::GetPedFromNetworkObject(NetworkInterface::GetObjectManager().GetNetworkObject(m_CustodianPedId));
}

////////////////////////////////////////////////////////////////////////////////

CTaskFSMClone *CClonedTaskInCustodyInfo::CreateCloneFSMTask()
{
	return rage_new CTaskInCustody(GetCustodianPed());
}

//////////////////////////////////////////////////////////////////////////
// CTaskPlayCuffedSecondaryAnims
//////////////////////////////////////////////////////////////////////////

void CTaskPlayCuffedSecondaryAnims::ProcessHandCuffs(CPed &ped)
{
	if (CTaskArrest::IsEnabled() && ped.GetPedConfigFlag(CPED_CONFIG_FLAG_IsHandCuffed))
	{
		CTaskCuffed* pTaskCuffed = static_cast<CTaskCuffed*>(ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_CUFFED));
		if(!pTaskCuffed || pTaskCuffed->CanStartSecondaryCuffedAnims())
		{
			aiTask* pTask = ped.GetPedIntelligence()->GetTaskSecondary(PED_TASK_SECONDARY_PARTIAL_ANIM);

			if (!pTask || pTask->GetTaskType() != CTaskTypes::TASK_PLAY_CUFFED_SECONDARY_ANIMS)
			{
				ped.GetPedIntelligence()->AddTaskSecondary(rage_new CTaskPlayCuffedSecondaryAnims(CTaskCuffed::sm_Tunables.m_ActionsHandcuffedNetworkId,
					CTaskCuffed::sm_Tunables.m_HandcuffedClipSetId, 
					NORMAL_BLEND_DURATION, 
					NORMAL_BLEND_DURATION), PED_TASK_SECONDARY_PARTIAL_ANIM);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////

const fwMvFloatId	CTaskPlayCuffedSecondaryAnims::ms_MoveSpeedId("Speed",0xF997622B);
const fwMvFlagId	CTaskPlayCuffedSecondaryAnims::ms_InVehicleId("bInVehicle",0xF924B584);

CTaskPlayCuffedSecondaryAnims::CTaskPlayCuffedSecondaryAnims(const fwMvNetworkDefId& networkDefId, 
															 const fwMvClipSetId& clipsetId, 
															 float fBlendInDuration, 
															 float fBlendOutDuration)
															 : m_NetworkDefId(networkDefId)
															 , m_ClipSetId(clipsetId)
															 , m_fBlendInDuration(fBlendInDuration)
															 , m_fBlendOutDuration(fBlendOutDuration)
{
	SetInternalTaskType(CTaskTypes::TASK_PLAY_CUFFED_SECONDARY_ANIMS);
}

//////////////////////////////////////////////////////////////////////////

CTaskPlayCuffedSecondaryAnims::~CTaskPlayCuffedSecondaryAnims()
{

}

//////////////////////////////////////////////////////////////////////////

void CTaskPlayCuffedSecondaryAnims::CleanUp()
{
	CPed* pPed = GetPed();
	pPed->GetMovePed().ClearSecondaryTaskNetwork(m_MoveNetworkHelper, m_fBlendOutDuration);

	m_ClipSetRequestHelper.Release();

	if (pPed->GetRagdollConstraintData())
	{
		pPed->GetRagdollConstraintData()->EnableHandCuffs(pPed, false);
	}

	if (pPed->IsLocalPlayer() && pPed->GetNetworkObject())
	{
		PhysicalPlayerIndex playerIndex = pPed->GetNetworkObject()->GetPhysicalPlayerIndex();
		if (playerIndex != INVALID_PLAYER_INDEX)
		{
			CPickupManager::RemovePlayerFromAllProhibitedCollections(playerIndex);
		}
	}
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskPlayCuffedSecondaryAnims::ProcessPreFSM()
{
	GetPed()->SetPedResetFlag(CPED_RESET_FLAG_TemporarilyBlockWeaponSwitching, true);

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskPlayCuffedSecondaryAnims::ProcessPostFSM()
{
	UpdateMoVE();

	return FSM_Continue;
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskPlayCuffedSecondaryAnims::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

	FSM_State(State_Start)
		FSM_OnEnter
			return Start_OnEnter();
		FSM_OnUpdate
			return Start_OnUpdate();

	FSM_State(State_Running)
		FSM_OnUpdate
			return Running_OnUpdate();

	FSM_State(State_Finish)
		FSM_OnUpdate
			return FSM_Quit;

	FSM_End
}

//////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskPlayCuffedSecondaryAnims::Start_OnEnter()
{
	CPed* pPed = GetPed();
	if (pPed->GetRagdollConstraintData())
	{
		pPed->GetRagdollConstraintData()->EnableHandCuffs(pPed, true);
	}

	if (pPed->IsLocalPlayer() && pPed->GetNetworkObject())
	{
		PhysicalPlayerIndex playerIndex = pPed->GetNetworkObject()->GetPhysicalPlayerIndex();
		if (playerIndex != INVALID_PLAYER_INDEX)
		{
			CPickupManager::AddPlayerToAllProhibitedCollections(playerIndex);
		}
	}

	pPed->GetWeaponManager()->EquipWeapon( pPed->GetDefaultUnarmedWeaponHash(), -1, true);

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskPlayCuffedSecondaryAnims::Start_OnUpdate()
{
	CPed* pPed = GetPed();
	m_MoveNetworkHelper.CreateNetworkPlayer(pPed, m_NetworkDefId);
	
	if(m_ClipSetRequestHelper.Request(m_ClipSetId))
	{
		pPed->GetMovePed().SetSecondaryTaskNetwork(m_MoveNetworkHelper.GetNetworkPlayer(), 
			m_fBlendInDuration, 
			CMovePed::Secondary_TagSyncContinuous | CMovePed::Secondary_TagSyncTransition);

		SetState(State_Running);
	}
	
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskPlayCuffedSecondaryAnims::Running_OnUpdate()
{
	if(!GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_IsHandCuffed))
	{
		SetState(State_Finish);
	}

	CTaskCuffed* pTaskCuffed = static_cast<CTaskCuffed*>(GetPed()->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_CUFFED));
	if(pTaskCuffed && GetPed()->GetMovePed().IsTaskNetworkFullyBlended())
	{
		SetState(State_Finish);
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void	CTaskPlayCuffedSecondaryAnims::UpdateMoVE()
{
	if(m_MoveNetworkHelper.IsNetworkActive())
	{
		float fSpeed = GetPed()->GetMotionData()->GetCurrentMbrY() / MOVEBLENDRATIO_SPRINT; // normalize the speed values between 0.0f and 1.0f
		if(GetPed()->GetPedResetFlag(CPED_RESET_FLAG_IsJumping) || 
			GetPed()->GetPedResetFlag(CPED_RESET_FLAG_IsLanding) || 
			GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle) )
		{
			fSpeed = 0.0f; //forces idle clip.
		}
		
		//! Cuffed anim sticks through seats. Pick seated anim if in a vehicle. Note: Don't do this for bikes, as the anim
		//! hides the hands inside the player.
		bool bInVehicle = false;
		CVehicle *pVehicle = GetPed()->GetVehiclePedInside();
		if(pVehicle)
		{
			if(!pVehicle->InheritsFromBike() && !pVehicle->InheritsFromQuadBike() && !pVehicle->InheritsFromAmphibiousQuadBike())
			{
				bInVehicle = true;
			}
		}

		m_MoveNetworkHelper.SetFlag( bInVehicle, ms_InVehicleId );
		m_MoveNetworkHelper.SetFloat( ms_MoveSpeedId, fSpeed);
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskPlayCuffedSecondaryAnims::ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
	if (iPriority != CTask::ABORT_PRIORITY_IMMEDIATE
		&& pEvent 
		&& ((CEvent*)pEvent)->GetEventType() != EVENT_SCRIPT_COMMAND
		&& ((CEvent*)pEvent)->GetEventType() != EVENT_GIVE_PED_TASK)
	{
		return false;
	}

#if __BANK
	ARREST_DISPLAYF(GetPed(), GetPed(), "CTaskPlayCuffedSecondaryAnims::ShouldAbort - aborting task (State = %d)! \n", GetState());
	if(pEvent)
	{
		ARREST_DISPLAYF(GetPed(), GetPed(), "CTaskPlayCuffedSecondaryAnims::ShouldAbort. Type = %d, Priority = %d, Task = %s\n", 
			pEvent->GetEventType(), 
			pEvent->GetEventPriority(), 
			pEvent->GetName().c_str());
	}
#endif

	return CTask::ShouldAbort(iPriority, pEvent);
}

////////////////////////////////////////////////////////////////////////////////

rage::s32 CTaskPlayCuffedSecondaryAnims::GetDefaultStateAfterAbort() const
{
	return State_Start;
}

#endif // ENABLE_TASKS_ARREST_CUFFED