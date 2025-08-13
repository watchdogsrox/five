#include "task/Combat/TaskToHurtTransit.h"

#include "Animation/MovePed.h"
#include "Event/EventDamage.h"
#include "Grcore/debugdraw.h"
#include "Math/angmath.h"
#include "Peds/Ped.h"
#include "Peds/PedGeometryAnalyser.h"
#include "Peds/PedIntelligence.h"
#include "Weapons/WeaponDamage.h"
#include "Network/NetworkInterface.h"

const fwMvClipId CTaskToHurtTransit::ms_HurtClipId("normal_to_injured",0x4A00D1AC);

const fwMvClipSetId CTaskToHurtTransit::m_ClipSetIdPistolToHurt("Normal_Pistol_To_Injured_Strafe",0xEF492B23);
const fwMvClipSetId CTaskToHurtTransit::m_ClipSetIdRifleToHurt("Normal_Rifle_To_Injured_Strafe",0x21428728);

AI_OPTIMISATIONS()

CTaskToHurtTransit::CTaskToHurtTransit(CEntity* pTarget)
{
	m_pTarget = pTarget;
	SetInternalTaskType(CTaskTypes::TASK_TO_HURT_TRANSIT);
}

CTaskToHurtTransit::~CTaskToHurtTransit()
{

}

#if !__FINAL
const char * CTaskToHurtTransit::GetStaticStateName(s32 _iState)
{
	Assert(_iState>=0 && _iState <= State_Finish);
	static const char* lNames[] = 
	{
		"State_Start",
		"State_ToHurtAnim",
		"State_Finish",
		"State_Invalid"
	};

	return lNames[_iState];
}
#endif // !__FINAL

CTask::FSM_Return CTaskToHurtTransit::ProcessPreFSM()
{	
	
	return FSM_Continue;
}

CTask::FSM_Return CTaskToHurtTransit::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_ToHurtAnim)
			FSM_OnEnter
				ToHurtAnim_OnEnter();
			FSM_OnUpdate
				return ToHurtAnim_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnEnter
				return FSM_Quit;

	FSM_End
}

void CTaskToHurtTransit::ToHurtAnim_OnEnter()
{
	CPed* pPed = GetPed();
	fwMvClipSetId ClipSet = m_ClipSetIdPistolToHurt;
	
	CPedWeaponManager* pWeapMgr = pPed->GetWeaponManager();
	const CWeaponInfo* pWeaponInfo = pWeapMgr->GetEquippedWeaponInfo();
	if (pWeaponInfo && pWeaponInfo->GetGroup() != WEAPONGROUP_UNARMED && pWeaponInfo->GetGroup() != WEAPONGROUP_PISTOL)
		ClipSet = m_ClipSetIdRifleToHurt;

	CTask* pTask = rage_new CTaskRunClip(ClipSet, ms_HurtClipId, REALLY_SLOW_BLEND_IN_DELTA, NORMAL_BLEND_OUT_DELTA, -1, APF_ISFINISHAUTOREMOVE); // APF_ISFINISHAUTOREMOVE This flag doesnt work?
	SetNewTask(pTask);

	pPed->GetPedIntelligence()->GetCombatBehaviour().ClearFlag(CCombatData::BF_CanUseCover);
	pPed->GetInventory()->GetAmmoRepository().SetUsingInfiniteAmmo(true);
	pPed->GetInventory()->GetAmmoRepository().SetUsingInfiniteClips(true);
	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_HasHurtStarted, true);

	if (!m_pTarget)
		m_pTarget = pPed->GetWeaponDamageEntity();
}

CTask::FSM_Return CTaskToHurtTransit::ToHurtAnim_OnUpdate()
{
	CPed* pPed = GetPed();
	CPedWeaponManager* pWeapMgr = pPed->GetWeaponManager();
	pPed->SetIsStrafing(true);

	// Drop or equip the weapons...
	CTaskRunClip* pTaskRunClip = (CTaskRunClip*)GetSubTask();
	if (pTaskRunClip)
	{
		const CClipHelper* pClipHelper = pTaskRunClip->GetClipIfPlaying();
		if (pClipHelper)
		{
	//		pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_AnimatedVelocity);

			// Spawn gun?
			if (pClipHelper->IsEvent<crPropertyAttributeBool>(CClipEventTags::Object, CClipEventTags::Create, true))
			{
				const CWeaponInfo* pBestPistol = pWeapMgr->GetBestWeaponInfoByGroup(WEAPONGROUP_PISTOL);
				if (!pBestPistol)
				{
					pPed->GetInventory()->AddWeaponAndAmmo(WEAPONTYPE_PISTOL, 100);
					pWeapMgr->EquipWeapon(WEAPONTYPE_PISTOL, -1, true);
				}
				else
				{
					pWeapMgr->EquipWeapon(pBestPistol->GetHash(), -1, true);
				}
			}

			// Drop rifle?
			if (pClipHelper->IsEvent<crPropertyAttributeBool>(CClipEventTags::Object, CClipEventTags::Release, true))
			{
				const CWeaponInfo* pWeaponInfo = pWeapMgr->GetEquippedWeaponInfo();
				if (pWeaponInfo && pWeapMgr->GetEquippedWeaponHash() != WEAPONTYPE_UNARMED)
				{
					if(!NetworkInterface::IsGameInProgress())
					{
						pWeapMgr->DropWeapon(pWeapMgr->GetEquippedWeaponHash(), true);
					}
					else // simply delete the weapon in MP
					{
						pWeapMgr->DestroyEquippedWeaponObject(false);
						pPed->GetInventory()->RemoveWeaponAndAmmo(pWeapMgr->GetEquippedWeaponHash());
					}
				}
			}

			// We need to face our target or the aiming task will do a turn anim that kinda sux
			if (m_pTarget)
			{
				Vector3 vTargetPos = VEC3V_TO_VECTOR3(m_pTarget->GetTransform().GetPosition());
				pPed->SetDesiredHeading(vTargetPos);

				float phaseWeight = Max(1.f - pClipHelper->GetPhase(), 0.001f);
				float turnRequired = SubtractAngleShorter(pPed->GetDesiredHeading(), pPed->GetCurrentHeading());
				float turnAmount = (turnRequired / phaseWeight) * fwTimer::GetTimeStep() ;
				if (abs(turnAmount) > abs(turnRequired))
					turnAmount = turnRequired;

				pPed->SetHeading(fwAngle::LimitRadianAngle(turnAmount + pPed->GetCurrentHeading()));
				//pPed->GetMotionData()->SetExtraHeadingChangeThisFrame(turnAmount);
			}
		}
	}

	// Stream the hurt aiming moveset in to avoid glitch
	const CWeaponInfo* pWeaponInfo = pWeapMgr->GetEquippedWeaponInfo();
	if (pWeaponInfo && pWeaponInfo->GetGroup() == WEAPONGROUP_PISTOL)	// Make sure we have switched weapon fully, no rifle is allowed
	{
		fwClipSet* pClipSet = fwClipSetManager::GetClipSet(pWeaponInfo->GetAppropriateWeaponClipSetId(pPed));
		if (pClipSet)
			pClipSet->Request_DEPRECATED();

		const CPedModelInfo* pModelInfo = pPed->GetPedModelInfo();
		if (pModelInfo)
		{
			pClipSet = fwClipSetManager::GetClipSet(pModelInfo->GetAppropriateStrafeClipSet(pPed));
			if (pClipSet)
				pClipSet->Request_DEPRECATED();
		}
	}

	// So we are good to go!?
	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished) || !GetSubTask())
	{
		// These flags are necessary! Taken from the CTaskGun::GetUpComplete()
		pPed->SetPedResetFlag(CPED_RESET_FLAG_InstantBlendToAim, true);
		pPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAIUpdate, true); //need a post camera update to process the start state of the next task
		pPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true); //need a post camera update to process the start state of the next task
		pPed->SetPedResetFlag(CPED_RESET_FLAG_ePostCameraAnimUpdateUseZeroTimestep, true);

		if (pPed->GetHurtEndTime() > 0)
			pPed->SetHurtEndTime(pPed->GetHurtEndTime() + 2000); // Compensation for animlength

		return FSM_Quit;
	}

	return FSM_Continue;
}

