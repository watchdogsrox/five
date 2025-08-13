// File header
#include "Task/Weapons/TaskDetonator.h"

// Game headers
#include "Animation/MovePed.h"
#include "Debug/DebugScene.h"
#include "Peds/Ped.h"
#include "Task/Combat/Cover/TaskCover.h"
#include "Task/Response/TaskDuckAndCover.h"
#include "Task/Weapons/TaskSwapWeapon.h"
#include "Weapons/Projectiles/ProjectileManager.h"
#include "Weapons/Weapon.h"

AI_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CTaskDetonator
//////////////////////////////////////////////////////////////////////////

fwMvNetworkId CTaskDetonator::ms_DetonateNetworkId("DetonateNetwork",0x7A92FE6E);
fwMvClipId CTaskDetonator::ms_CrouchFireId("fire_crouch", 0x07bc56892);
fwMvClipId CTaskDetonator::ms_FireId("fire", 0x0d30a036b);

CTaskDetonator::CTaskDetonator(const CWeaponController::WeaponControllerType weaponControllerType, float fDuration)
: m_weaponControllerType(weaponControllerType)
, m_fDuration(fDuration)
, m_bHasFired(false)
, m_fDetonateTime(-1.0f)
{
	m_GenericClipHelper.SetInsertCallback(MakeFunctorRet(*this, &CTaskDetonator::InsertDetonatorNetworkCB));
	SetInternalTaskType(CTaskTypes::TASK_DETONATOR);
}

CTaskDetonator::~CTaskDetonator()
{
	m_GenericClipHelper.BlendOutClip();
}

CTaskDetonator::FSM_Return CTaskDetonator::ProcessPreFSM()
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	pPed->SetPedResetFlag( CPED_RESET_FLAG_TemporarilyBlockWeaponSwitching, true );
	return FSM_Continue;
}

CTaskDetonator::FSM_Return CTaskDetonator::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.
	weaponAssert(pPed->GetWeaponManager());

	FSM_Begin
		FSM_State(State_Init)
			FSM_OnUpdate
				return StateInit_OnUpdate(pPed);

		FSM_State(State_Detonate)
			FSM_OnEnter
				return StateDetonate_OnEnter(pPed);
			FSM_OnUpdate
				return StateDetonate_OnUpdate(pPed);

		FSM_State(State_DuckAndDetonate)
			FSM_OnEnter
				return StateDuckAndDetonate_OnEnter(pPed);
			FSM_OnUpdate
				return StateDuckAndDetonate_OnUpdate(pPed);

		FSM_State(State_Swap)
			FSM_OnEnter
				return StateSwap_OnEnter(pPed);
			FSM_OnUpdate
				return StateSwap_OnUpdate(pPed);
		FSM_State(State_Quit)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

#if !__FINAL
const char * CTaskDetonator::GetStaticStateName( s32 iState )
{
	static const char* aStateNames[] = 
	{
		"Init",
		"Detonate",
		"Duck And Detonate",
		"Swap",
		"Quit",
	};

	return aStateNames[iState];
}
#endif // !__FINAL

CTaskDetonator::FSM_Return CTaskDetonator::StateInit_OnUpdate(CPed* pPed)
{
	TUNE_GROUP_FLOAT(DETONATIONS,MAX_DISTANCE_FOR_DUCK_IN_DETONATION, 10.0f, 0.0f, 40.0f, 0.1f);

	if(pPed->GetWeaponManager()->GetRequiresWeaponSwitch())
	{
		// Swap weapon, as we have the incorrect (or holstered) weapon equipped
		SetState(State_Swap);
	}
	else
	{
		// We'll need to do something custom in cover, disabling for now as it leaves the ped unable to move properly, also it don't look too great
		if( !pPed->GetIsInCover() && CProjectileManager::GetProjectileWithinDistance(pPed, AMMOTYPE_STICKYBOMB, MAX_DISTANCE_FOR_DUCK_IN_DETONATION)!= NULL )
		{
			// If there's something we're going to blow up within a specified range (7m), then duck and cover before detonation
			SetState(State_DuckAndDetonate);
		}
		else
		{
			// Otherwise just flick the switch
			SetState(State_Detonate);
		}
	}

	return FSM_Continue;
}

CTaskDetonator::FSM_Return CTaskDetonator::StateDetonate_OnEnter(CPed* pPed)
{
	const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
	if(!pWeapon)
	{
		return FSM_Quit;
	}

	const CWeaponInfo* pWeaponInfo = pWeapon->GetWeaponInfo();
	if(!pWeaponInfo)
	{
		return FSM_Quit;
	}

	fwMvClipSetId clipSetId = pWeaponInfo->GetAppropriateWeaponClipSetId(pPed);
	fwMvClipId clipId = pPed->GetIsCrouching() ? ms_CrouchFireId : ms_FireId;

	m_GenericClipHelper.BlendInClipBySetAndClip(pPed, clipSetId, clipId, NORMAL_BLEND_IN_DELTA, NORMAL_BLEND_OUT_DELTA, false, true);
	m_GenericClipHelper.SetFilter((u32)BONEMASK_ARMONLY_R);

	return FSM_Continue;
}

CTaskDetonator::FSM_Return CTaskDetonator::StateDetonate_OnUpdate(CPed* pPed)
{
	if (m_GenericClipHelper.IsHeldAtEnd())
	{
		return FSM_Quit;
	}

	if (m_fDetonateTime < 0.0f)
	{
		CClipEventTags::FindEventPhase(m_GenericClipHelper.GetClip(), CClipEventTags::Detonation, m_fDetonateTime);
	}

	if (!m_bHasFired && m_GenericClipHelper.GetPhase() >= m_fDetonateTime)
	{
		m_bHasFired = true;

		if(pPed->GetWeaponManager()->GetIsDetonatorActive())
		{
			const CWeaponInfo* pWeaponInfo = pPed->GetWeaponManager()->GetEquippedWeaponInfo();
			if(pWeaponInfo && pWeaponInfo->GetIsDetonator())
			{
				CProjectileManager::ExplodeProjectiles(pPed, AMMOTYPE_STICKYBOMB);
			}
		}
	}

	return FSM_Continue;
}

CTaskDetonator::FSM_Return CTaskDetonator::StateDuckAndDetonate_OnEnter(CPed* pPed)
{
	const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
	if(!pWeapon)
	{
		return FSM_Quit;
	}

	const CWeaponInfo* pWeaponInfo = pWeapon->GetWeaponInfo();
	if(!pWeaponInfo)
	{
		return FSM_Quit;
	}

	// Start the DuckAndCover
	SetNewTask(rage_new CTaskDuckAndCover());

	return FSM_Continue;
}



CTaskDetonator::FSM_Return CTaskDetonator::StateDuckAndDetonate_OnUpdate(CPed* pPed )
{
	const aiTask	*pDuckTask = GetSubTask();

	// Check for the subtask being in state "State_Ducking"
	if(pDuckTask && pDuckTask->GetState() == CTaskDuckAndCover::State_Ducking)
	{
		if (!m_bHasFired )
		{
			m_bHasFired = true;

			if(pPed->GetWeaponManager()->GetIsDetonatorActive())
			{
				const CWeaponInfo* pWeaponInfo = pPed->GetWeaponManager()->GetEquippedWeaponInfo();
				if(pWeaponInfo && pWeaponInfo->GetIsDetonator())
				{
					CProjectileManager::ExplodeProjectiles(pPed, AMMOTYPE_STICKYBOMB);
				}
			}
		}
	}

	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		return FSM_Quit;
	}

	return FSM_Continue;
}



CTaskDetonator::FSM_Return CTaskDetonator::StateSwap_OnEnter(CPed* UNUSED_PARAM(pPed))
{
	s32 iFlags = SWAP_HOLSTER | SWAP_DRAW | SWAP_TO_AIM;
	SetNewTask(rage_new CTaskSwapWeapon(iFlags));
	return FSM_Continue;
}

CTaskDetonator::FSM_Return CTaskDetonator::StateSwap_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		// Set state to detonate
		SetState(State_Detonate);
	}

	return FSM_Continue;
}

bool CTaskDetonator::InsertDetonatorNetworkCB(fwMoveNetworkPlayer* pPlayer, float fBlendDuration, u32)
{
	CPed* pPed = GetPed();

	if (GetParent() && GetParent()->GetTaskType() == CTaskTypes::TASK_IN_COVER)
	{
		CTaskInCover* pCoverTask = static_cast<CTaskInCover*>(GetParent()); 
		if (pCoverTask->GetMovementNetworkHelper() && pCoverTask->GetMovementNetworkHelper()->IsNetworkAttached())
		{
			pCoverTask->GetMoveNetworkHelper()->SetSubNetwork(pPlayer, ms_DetonateNetworkId);
		}
	}
	else
	{
		pPed->GetMovePed().SetTaskNetwork(pPlayer, fBlendDuration);
	}
	return true;
}
