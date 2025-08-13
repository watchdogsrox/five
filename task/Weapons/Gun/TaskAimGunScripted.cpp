//
// Task/Weapons/Gun/TaskAimGunScripted.cpp
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

// File header
#include "Task/Weapons/Gun/TaskAimGunScripted.h"

// Rage Headers
#include "fwanimation/clipsets.h"

// Game Headers
#include "animation/MovePed.h"
#include "Camera/CamInterface.h"
#include "Camera/Base/BaseCamera.h"
#include "Camera/Gameplay/GameplayDirector.h"
#include "Debug/DebugScene.h"
#include "Peds/PedIntelligence.h"
#include "Peds/Ped.h"
#include "Peds/PedWeapons/PlayerPedTargeting.h"
#include "ModelInfo/WeaponModelInfo.h"
#include "Scene/Playerswitch/PlayerSwitchInterface.h"
#include "Scene/World/GameWorld.h"
#include "Task/Combat/Cover/TaskCover.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/Weapons/Gun/TaskGun.h"
#include "Task/Weapons/Gun/TaskReloadGun.h"
#include "Task/Weapons/Gun/Metadata/ScriptedGunTaskInfo.h"
#include "Task/Weapons/Gun/Metadata/ScriptedGunTaskInfoMetadataMgr.h"
#include "Task/Weapons/TaskWeaponBlocked.h"
#include "Task/Weapons/TaskSwapWeapon.h"
#include "Task/Weapons/WeaponController.h"
#include "Task/Vehicle/TaskVehicleWeapon.h"
#include "Weapons/Weapon.h"
#include "Vehicles/Metadata/VehicleSeatInfo.h"

// Macro to disable optimisations if set
AI_OPTIMISATIONS()
AI_WEAPON_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// CTaskAimGunScripted
//////////////////////////////////////////////////////////////////////////

const fwMvRequestId CTaskAimGunScripted::ms_IdleRequestId("Idle",0x71C21326);
const fwMvRequestId CTaskAimGunScripted::ms_IntroRequestId("Intro",0xDA8F5F53);
const fwMvRequestId CTaskAimGunScripted::ms_OutroRequestId("Outro",0x5D40B28D);
const fwMvRequestId CTaskAimGunScripted::ms_ReloadRequestId("Reload",0x5DA46B68);
const fwMvRequestId CTaskAimGunScripted::ms_BlockedRequestId("Blocked",0x568F620C);
const fwMvRequestId CTaskAimGunScripted::ms_AimRequestId("Aim",0xB01E2F36);
const fwMvBooleanId CTaskAimGunScripted::ms_IntroOnEnterId("Intro_OnEnter",0x284FF388);
const fwMvBooleanId CTaskAimGunScripted::ms_AimOnEnterId("Aim_OnEnter",0x2E462D6);
const fwMvBooleanId CTaskAimGunScripted::ms_ReloadingOnEnterId("Reload_OnEnter",0x63CD5A69);
const fwMvBooleanId CTaskAimGunScripted::ms_BlockedOnEnterId("Blocked_OnEnter",0xB20F8008);
const fwMvBooleanId CTaskAimGunScripted::ms_IdleOnEnterId("Idle_OnEnter",0xCA902721);
const fwMvBooleanId CTaskAimGunScripted::ms_OutroOnEnterId("Outro_OnEnter",0x72E4FA9E);
const fwMvBooleanId CTaskAimGunScripted::ms_OutroFinishedId("OutroAnimFinished",0x834634);
const fwMvBooleanId CTaskAimGunScripted::ms_ReloadClipFinishedId("ReloadAnimFinished",0x1E6EFFA);
const fwMvBooleanId CTaskAimGunScripted::ms_FireLoopedId("FireAnimLooped",0xF933366C);
const fwMvBooleanId CTaskAimGunScripted::ms_FireId("Fire",0xD30A036B);
const fwMvBooleanId CTaskAimGunScripted::ms_FireLoopedOneFinishedId("FireLoopOneFinished",0x15FE4039);
const fwMvBooleanId CTaskAimGunScripted::ms_FireLoopedTwoFinishedId("FireLoopTwoFinished",0x33CA4EF8);
const fwMvFloatId CTaskAimGunScripted::ms_IntroRateId("IntroRate",0xC8EAB73F);
const fwMvFloatId CTaskAimGunScripted::ms_OutroRateId("OutroRate",0xCC150EDC);
const fwMvFloatId CTaskAimGunScripted::ms_HeadingId("Heading",0x43F5CF44);
const fwMvFloatId CTaskAimGunScripted::ms_IntroHeadingId("IntroHeading",0x6EBE5858);
const fwMvFloatId CTaskAimGunScripted::ms_OutroHeadingId("OutroHeading",0xF30EE338);
const fwMvFloatId CTaskAimGunScripted::ms_PitchId("Pitch",0x3F4BB8CC);
const fwMvFlagId CTaskAimGunScripted::ms_FiringFlagId("Firing",0x544C888B);
const fwMvFlagId CTaskAimGunScripted::ms_FiringLoopId("FiringLoop",0x8A07CC0);
const fwMvFlagId CTaskAimGunScripted::ms_AbortOnIdleId("AbortOnIdle",0x835559E1);
const fwMvFlagId CTaskAimGunScripted::ms_UseAdditiveFiringId("UseAdditiveFiring",0x3C48DF39);
const fwMvFlagId CTaskAimGunScripted::ms_UseDirectionalBreatheAdditivesId("UseDirectionalBreatheAdditives",0x357F95C4);

#if __BANK
bool CTaskAimGunScripted::ms_bDisableBlocking = false;
#endif // __BANK

bank_float CTaskAimGunScripted::ms_fNetworkBlendInDuration = NORMAL_BLEND_DURATION;
bank_float CTaskAimGunScripted::ms_fNetworkBlendOutDuration = SLOW_BLEND_DURATION;
bank_float CTaskAimGunScripted::ms_fMaxHeadingChangeRate = 1.0f;
bank_float CTaskAimGunScripted::ms_fMaxPitchChangeRate = 1.0f;

////////////////////////////////////////////////////////////////////////////////

CTaskAimGunScripted::CTaskAimGunScripted(const CWeaponController::WeaponControllerType weaponControllerType, const fwFlags32& iFlags, const CWeaponTarget& target)
: CTaskAimGun(weaponControllerType, iFlags, 0, target)
, m_ClipSetId(CLIP_SET_ID_INVALID)
, m_fMinHeadingAngle(0.0f)
, m_fMaxHeadingAngle(0.0f)
, m_fMinPitchAngle(0.0f)
, m_fMaxPitchAngle(0.0f)
, m_uIdleCameraHash(0)
, m_uCameraHash(0)
, m_fCurrentHeading(-1.0f)
, m_fCurrentPitch(-1.0f)
, m_fDesiredPitch(-1.0f)
, m_fDesiredHeading(-1.0f)
, m_fHeadingChangeRate(1.0f)
, m_fPitchChangeRate(1.0f)
, m_fIntroHeadingBlend(-1.0f)
, m_fOutroHeadingBlend(-1.0f)
, m_bForceReload(false)
, m_pRopeOrientationEntity(NULL)
, m_bAllowWrapping(false)
, m_bTrackRopeOrientation(false)
, m_bIgnoreRequestToAim(false)
, m_bFire(false)
, m_bAbortOnIdle(false)
, m_bFiredOnce(false)
{
	SetInternalTaskType(CTaskTypes::TASK_AIM_GUN_SCRIPTED);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskAimGunScripted::GetOverriddenGameplayCameraSettings(tGameplayCameraSettings& settings) const
{
	const CPed *pPed = GetPed(); 
	if (pPed->IsLocalPlayer())
	{
		if (GetState() <= State_Idle)
		{
			if (m_uIdleCameraHash != 0)
			{
				settings.m_CameraHash = m_uIdleCameraHash;
			}
			else
			{
				//Request_DEPRECATED an invalid camera in order to prevent tasks that are higher in the task tree from requesting an aim camera.
				settings.m_CameraHash = ATSTRINGHASH("INVALID_CAMERA", 0x5AAD591B);
			}
		}
		else if (m_uCameraHash != 0)
		{
			//Inhibit aim cameras during long and medium Switch transitions.
			if (pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_DISABLE_AIM_CAMERA) ||
				(g_PlayerSwitch.IsActive() && (g_PlayerSwitch.GetSwitchType() != CPlayerSwitchInterface::SWITCH_TYPE_SHORT)))
			{
				return;
			}

			settings.m_CameraHash = m_uCameraHash;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

CTaskInfo* CTaskAimGunScripted::CreateQueriableState() const
{
	// clamp
	float fCurrentPitch      = Clamp(m_fCurrentPitch, -1.0f, 1.0f);
	float fCurrentHeading    = Clamp(m_fCurrentHeading, -1.0f, 1.0f);

	return rage_new CClonedAimGunScriptedInfo(GetState(), fCurrentPitch, fCurrentHeading);
}

////////////////////////////////////////////////////////////////////////////////
void CTaskAimGunScripted::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	taskAssert(pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_AIM_GUN_SCRIPTED);

	CClonedAimGunScriptedInfo *aimGunScriptedInfo = static_cast<CClonedAimGunScriptedInfo*>(pTaskInfo);

	m_fCurrentPitch		= aimGunScriptedInfo->GetPitch();
	m_fCurrentHeading	= aimGunScriptedInfo->GetHeading();

	CTaskFSMClone::ReadQueriableState(pTaskInfo);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskAimGunScripted::CleanUp()
{
	if (m_MoveNetworkHelper.IsNetworkAttached())
	{
		GetPed()->GetMovePed().ClearTaskNetwork(m_MoveNetworkHelper, ms_fNetworkBlendOutDuration);
	}
	// Base class
	CTaskAimGun::CleanUp();
}

////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskAimGunScripted::ProcessPreFSM()
{
	CPed* pPed = GetPed(); 

	// Is a valid weapon available?
	weaponAssert(pPed->GetWeaponManager());
	const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
	if (!pWeapon || !pWeapon->GetWeaponInfo()->GetIsGunOrCanBeFiredLikeGun())
	{
		return FSM_Quit;
	}

	// Is a valid weapon object available
	const CObject* pWeaponObject = pPed->GetWeaponManager()->GetEquippedWeaponObject();
	if (!pWeaponObject)
	{
		return FSM_Quit;
	}

	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(pPed->GetWeaponManager()->GetEquippedWeaponHash());
	if(!pWeaponInfo->GetAllowEarlyExitFromFireAnimAfterBulletFired()) 
	{
		m_iFlags.SetFlag(GF_OnlyExitFireLoopAtEnd);
	}
	else
	{
		m_iFlags.ClearFlag(GF_OnlyExitFireLoopAtEnd);
	}

	pPed->SetPedResetFlag(CPED_RESET_FLAG_ProcessPostCamera, true);
	pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPreRender2, true );
	pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPostPreRender, true );
	pPed->SetPedResetFlag( CPED_RESET_FLAG_IsHigherPriorityClipControllingPed, true );

	return CTaskAimGun::ProcessPreFSM();
}

////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskAimGunScripted::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{

	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_Idle)
			FSM_OnEnter
				return Idle_OnEnter();
			FSM_OnUpdate
				return Idle_OnUpdate();

		FSM_State(State_Intro)
			FSM_OnEnter
				return Intro_OnEnter();
			FSM_OnUpdate
				return Intro_OnUpdate();

		FSM_State(State_Aim)
			FSM_OnEnter
				return Aim_OnEnter();
			FSM_OnUpdate
				return Aim_OnUpdate();

		FSM_State(State_Fire)
			FSM_OnEnter
				return Fire_OnEnter();
			FSM_OnUpdate
				return Fire_OnUpdate();
			FSM_OnExit
				return Fire_OnExit();

		FSM_State(State_Outro)
			FSM_OnEnter
				return Outro_OnEnter();
			FSM_OnUpdate
				return Outro_OnUpdate();

		FSM_State(State_Reload)
			FSM_OnEnter
				return Reload_OnEnter();
			FSM_OnUpdate
				return Reload_OnUpdate();
			FSM_OnExit
				return Reload_OnExit();

		FSM_State(State_Blocked)
			FSM_OnEnter
				return Blocked_OnEnter();
			FSM_OnUpdate
				return Blocked_OnUpdate();

		FSM_State(State_SwapWeapon)
			FSM_OnEnter
				return SwapWeapon_OnEnter();
			FSM_OnUpdate
				return SwapWeapon_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskAimGunScripted::ProcessPostCamera()
{
	UpdateMoVE();
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskAimGunScripted::ProcessPostPreRender()
{
	CPed *pPed = GetPed(); //Get the ped ptr.
	weaponAssert(pPed->GetWeaponManager());

	if(m_bTrackRopeOrientation)
	{
		CPed* pPed = GetPed();
		if(pPed && pPed->GetAttachParent() && m_pRopeOrientationEntity)
		{
			Vector3 vC = VEC3V_TO_VECTOR3(m_pRopeOrientationEntity->GetTransform().GetPosition())
				- VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			Matrix34 mat;
			mat.Identity();

			Vector3 vRotationAxis;
			vRotationAxis.Cross(mat.c, vC);
			vRotationAxis.Normalize();
			float fRotationAngle = mat.c.Angle(vC);
			if(fRotationAngle > SMALL_FLOAT)
			{
				mat.RotateUnitAxis(vRotationAxis, fRotationAngle);
				mat.d = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			}
			pPed->SetMatrix(mat);

			if(pPed->GetChildAttachment() && static_cast<CEntity*>(pPed->GetChildAttachment())->GetIsPhysical())
			{
				static_cast<CPhysical*>(pPed->GetChildAttachment())->ProcessAllAttachments();
			}
		}
	}

	if(m_bFire)
	{
		// Ignore fire events if we're not holding a gun weapon
		const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
		if(pWeapon)
		{
			FireGun(pPed);
			m_bFire = false;
		}
	}

	// Disabled hand ik for now as it looks bad at extreme angles, we should selectively use the ik, maybe use events in the clips?
	TUNE_GROUP_BOOL(AIM_GUN_SCRIPTED, useHandIk, false);

	if (useHandIk)
	{
		if(GetState() == State_Aim || GetState() == State_Fire)
		{
			CObject* pWeaponObject = pPed->GetWeaponManager()->GetEquippedWeaponObject();
			if (pWeaponObject)
			{
				CWeapon* pWeapon = pWeaponObject->GetWeapon();
				if (pWeapon)
				{
					const CWeaponModelInfo* pModelInfo = pWeapon->GetWeaponModelInfo();
					const CWeaponInfo* pWeaponInfo = pWeapon->GetWeaponInfo();
					taskAssert(pWeaponInfo);
					if (!pWeaponInfo->GetNoLeftHandIK() && taskVerifyf(pWeaponObject && pModelInfo, "Ped doesn't have a weapon object or modelinfo"))
					{
						if (CTaskAimGun::ms_bUseLeftHandIk)
						{
							s32 boneIdxL = pModelInfo->GetBoneIndex(WEAPON_GRIP_L);
							s32 boneIdxR = pModelInfo->GetBoneIndex(WEAPON_GRIP_R);
							if(boneIdxL >= 0 && boneIdxR >= 0)
							{
								Matrix34 mLeftGripBone;
								pWeapon->GetEntity()->GetGlobalMtx(boneIdxL, mLeftGripBone);

								Matrix34 mRightGripBone;
								pWeapon->GetEntity()->GetGlobalMtx(boneIdxR, mRightGripBone);

								Matrix34 m;
								m.DotTranspose(mLeftGripBone, mRightGripBone);

								pPed->GetIkManager().SetRelativeArmIKTarget(crIKSolverArms::LEFT_ARM, pPed, pPed->GetBoneIndexFromBoneTag(BONETAG_R_PH_HAND), m.d, AIK_TARGET_WRT_IKHELPER);
							}
						}
					}
				}
			}
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskAimGunScripted::Start_OnUpdate()
{
	taskAssertf(m_uScriptedGunTaskInfoHash != 0, "Invalid scripted gun task info hash");

	const CScriptedGunTaskInfo* pScriptedGunTaskInfo = CScriptedGunTaskMetadataMgr::GetScriptedGunTaskInfoByHash(m_uScriptedGunTaskInfoHash);
	if (!taskVerifyf(pScriptedGunTaskInfo, "Couldn't find scripted gun task info for hash %u", m_uScriptedGunTaskInfoHash))
	{
		return FSM_Quit;
	}

	// Store the gun task info for easy access
	m_ClipSetId.SetHash(pScriptedGunTaskInfo->GetClipSet());
	m_fMinHeadingAngle = pScriptedGunTaskInfo->GetMinAimSweepHeadingAngleDegs() * DtoR;
	m_fMaxHeadingAngle = pScriptedGunTaskInfo->GetMaxAimSweepHeadingAngleDegs() * DtoR;
	m_fMinPitchAngle = pScriptedGunTaskInfo->GetMinAimSweepPitchAngleDegs() * DtoR;
	m_fMaxPitchAngle = pScriptedGunTaskInfo->GetMaxAimSweepPitchAngleDegs() * DtoR;
	m_fHeadingOffset = pScriptedGunTaskInfo->GetHeadingOffset();
	m_bAllowWrapping = pScriptedGunTaskInfo->GetAllowWrapping();
	m_bTrackRopeOrientation = pScriptedGunTaskInfo->GetTrackRopeOrientation();
	m_uIdleCameraHash = pScriptedGunTaskInfo->GetIdleCamera().GetHash();
	m_uCameraHash = pScriptedGunTaskInfo->GetCamera().GetHash();

    CPed* pPed = GetPed();

	fwClipSet* pClipSet = fwClipSetManager::GetClipSet(m_ClipSetId);

    // clone peds have to stream the anims in if necessary
    if(pClipSet && pPed->IsNetworkClone())
    {
        if(!pClipSet->Request_DEPRECATED())
        {
            return FSM_Continue;
        }
    }

	if (!pClipSet || !pClipSet->Request_DEPRECATED())
	{
#if __ASSERT
		if (pClipSet)
		{
			taskAssertf(0, "Dictionary (%s) needs to be streamed in before calling TASK_AIM_GUN_SCRIPTED", pClipSet->GetClipDictionaryName().GetCStr());
		}
#endif // __ASSERT
		return FSM_Quit;
	}

	if (!m_MoveNetworkHelper.IsNetworkActive())
	{
		m_MoveNetworkHelper.CreateNetworkPlayer(pPed, CClipNetworkMoveInfo::ms_NetworkTaskAimGunScripted);
		pPed->GetMovePed().SetTaskNetwork(m_MoveNetworkHelper.GetNetworkPlayer(), ms_fNetworkBlendInDuration);
		m_MoveNetworkHelper.SetClipSet(m_ClipSetId);
	}

	if (pScriptedGunTaskInfo->ShouldAbortOnIdle())
	{
		m_bAbortOnIdle = true;
		m_MoveNetworkHelper.SetFlag(true, ms_AbortOnIdleId);
	}
	if (pScriptedGunTaskInfo->ShouldUseAdditiveFiring())
	{
		m_MoveNetworkHelper.SetFlag(true, ms_UseAdditiveFiringId);
	}
	if (pScriptedGunTaskInfo->ShouldUseDirectionalBreatheAdditives())
	{
		m_MoveNetworkHelper.SetFlag(true, ms_UseDirectionalBreatheAdditivesId);
	}

	if (pScriptedGunTaskInfo->GetForcedAiming())
	{
		GetGunFlags().SetFlag(GF_AlwaysAiming);
	}

	if(pScriptedGunTaskInfo->GetForceFireFromCamera())
	{
		GetGunFireFlags().SetFlag(GFF_ForceFireFromCamera);
	}

	if (GetGunFlags().IsFlagSet(GF_InstantBlendToAim))
	{
		m_MoveNetworkHelper.ForceStateChange(ms_AimRequestId);
		SetState(State_Aim);
		return FSM_Continue;
	}

	if (m_bAbortOnIdle)
		SetState(State_Intro);
	else
		SetState(State_Idle);

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskAimGunScripted::Idle_OnEnter()
{
	if (GetPreviousState() != State_SwapWeapon)
		m_MoveNetworkHelper.WaitForTargetState(ms_IdleOnEnterId);

	// Reset params
	m_fDesiredPitch = -1.0f;
	m_fDesiredHeading = -1.0f;
	m_fCurrentHeading = -1.0f;
	m_fCurrentPitch = -1.0f;
	m_fIntroHeadingBlend = -1.0f;
	m_fOutroHeadingBlend = -1.0f;
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskAimGunScripted::Idle_OnUpdate()
{
	if (!m_MoveNetworkHelper.IsInTargetState())
	{
		m_MoveNetworkHelper.SendRequest(ms_IdleRequestId);
		return FSM_Continue;
	}

	CPed* pPed = GetPed();

	const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();

	// Just idle if we cannot reload
	if (!pPed->IsNetworkClone() && pWeapon->GetNeedsToReload(true) && !pWeapon->GetCanReload())
	{
		return FSM_Continue;
	}

	if (pPed->GetWeaponManager()->GetRequiresWeaponSwitch())
	{
		SetState(State_SwapWeapon);
		return FSM_Continue;
	}

	// Cache the state of the controller
	WeaponControllerState controllerState = GetWeaponControllerState(pPed);
	if (controllerState == WCS_Aim || controllerState == WCS_Fire || GetGunFlags().IsFlagSet(GF_AlwaysAiming))
	{
		SetState(State_Intro);
		return FSM_Continue;
	}

	if(controllerState == WCS_Reload && pWeapon->GetCanReload() && !m_iFlags.IsFlagSet(GF_DisableReload))
	{
		SetState(State_Intro);
		m_bForceReload = true;
		return FSM_Continue;
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return  CTaskAimGunScripted::Intro_OnEnter()
{
	m_MoveNetworkHelper.WaitForTargetState(ms_IntroOnEnterId);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskAimGunScripted::Intro_OnUpdate()
{
	if (!m_MoveNetworkHelper.IsInTargetState())
	{
		m_MoveNetworkHelper.SendRequest(ms_IntroRequestId);
		return FSM_Continue;
	}

	SetState(State_Aim);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskAimGunScripted::Aim_OnEnter()
{
	if (!m_bIgnoreRequestToAim)
	{
		m_MoveNetworkHelper.WaitForTargetState(ms_AimOnEnterId);
	}

	m_bIgnoreRequestToAim = false;
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskAimGunScripted::Aim_OnUpdate()
{
	if (!m_MoveNetworkHelper.IsInTargetState())
	{
		m_MoveNetworkHelper.SendRequest(ms_AimRequestId);
		return FSM_Continue;
	}

	CPed* pPed = GetPed();

	if (pPed->GetWeaponManager()->GetRequiresWeaponSwitch() && !m_iFlags.IsFlagSet(GF_PreventWeaponSwapping))
	{
		SetState(State_SwapWeapon);
		return FSM_Continue;
	}

	// Cache the state of the controller
	WeaponControllerState controllerState = GetWeaponControllerState(pPed);
	bool bCanFireAtTarget = CanFireAtTarget(pPed);

	if((controllerState == WCS_Reload || m_bForceReload) && !m_iFlags.IsFlagSet(GF_DisableReload))
	{
		m_bForceReload = false;
		SetState(State_Reload);
	}
	else if( IsWeaponBlocked( pPed ) )
	{
		SetState(State_Blocked);
	}
	else if (!m_bFire && (controllerState == WCS_Fire || controllerState == WCS_FireHeld) && bCanFireAtTarget)
	{
		if(pPed->IsNetworkClone())
		{
			const CFiringPattern& firingPattern = pPed->GetPedIntelligence()->GetFiringPattern();
			if(firingPattern.ReadyToFire() && firingPattern.WantsToFire())
			{
				m_iFlags.SetFlag(GF_FireAtLeastOnce);
				SetState(State_Fire);
			}
		}
		else
		{
			static dev_float s_fHeadingDiffToFire = 0.1f;
			if (Abs(m_fCurrentHeading - m_fDesiredHeading) < s_fHeadingDiffToFire)
			{
				const CFiringPattern& firingPattern = pPed->GetPedIntelligence()->GetFiringPattern();
				if(firingPattern.ReadyToFire() && firingPattern.WantsToFire() && CheckCanFireAtPed(pPed))
				{
					m_iFlags.SetFlag(GF_FireAtLeastOnce);
					SetState(State_Fire);
				}
			}
		}
	}
	else if((controllerState == WCS_Aim && !m_iFlags.IsFlagSet(GF_DisableAiming)) || m_iFlags.IsFlagSet(GF_AlwaysAiming) || 
		    (!bCanFireAtTarget && (controllerState == WCS_Fire || controllerState == WCS_FireHeld)))
	{
		// Keep aiming
	}
	else
	{
		// Quit as we are no longer aiming
		SetState(State_Outro);
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskAimGunScripted::Fire_OnEnter()
{
	m_MoveNetworkHelper.SetFlag(true, ms_FiringFlagId);
	m_bFiredOnce = false;
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskAimGunScripted::Fire_OnUpdate()
{
	CPed* pPed = GetPed();

	WeaponControllerState controllerState = GetWeaponControllerState(pPed);

	bool bAllowedToExitState = true;
	bool bCheckLoopEvent = m_MoveNetworkHelper.GetBoolean(ms_FireLoopedOneFinishedId) || m_MoveNetworkHelper.GetBoolean(ms_FireLoopedTwoFinishedId);

	if(!m_bFiredOnce && controllerState != WCS_Fire && controllerState != WCS_FireHeld)
	{
		m_MoveNetworkHelper.SetFlag(false, ms_FiringLoopId);
	}
	else
	{
		m_MoveNetworkHelper.SetFlag(true, ms_FiringLoopId);
	}

	if (m_bFiredOnce && GetGunFlags().IsFlagSet(GF_OnlyExitFireLoopAtEnd) && !bCheckLoopEvent && HasAdditiveFireAnim())
	{
		bAllowedToExitState = false;
	}

	static dev_float s_fHeadingDiffToFire = 0.1f;
	const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();

	bool bForceReturnToAim = false;
	if(!CheckCanFireAtPed(pPed) && (controllerState == WCS_Fire || controllerState == WCS_FireHeld))
	{
		bForceReturnToAim = true;
	}
	
	if((pWeapon->GetIsClipEmpty() || controllerState == WCS_Reload) )
	{
		if (pWeapon->GetCanReload() && !m_iFlags.IsFlagSet(GF_DisableReload))
		{
			SetState(State_Reload);
		}
		else
		{
			SetState(State_Outro);
		}
	}
	else if( !CanFireAtTarget(pPed ))
	{
		SetState(State_Aim);
		m_bIgnoreRequestToAim = true;
	}
	else if( IsWeaponBlocked( pPed ) )
	{
		SetState(State_Blocked);
	}
	else if ( bAllowedToExitState && ((controllerState != WCS_Fire && controllerState != WCS_FireHeld) || 
			  (Abs(m_fCurrentHeading - m_fDesiredHeading) > s_fHeadingDiffToFire) ||
			  !pPed->GetPedIntelligence()->GetFiringPattern().ReadyToFire() || 
			  bForceReturnToAim ))
	{
		SetState(State_Aim);
		m_bIgnoreRequestToAim = true;
	}
	else if ( pPed->GetPedIntelligence()->GetFiringPattern().WantsToFire() &&
		      pPed->GetPedIntelligence()->GetFiringPattern().ReadyToFire())
	{	
		bool bFire = false;

		if (UsesFireEvents())
		{
			m_bForceWeaponStateToFire = false;

			if(m_MoveNetworkHelper.GetBoolean(ms_FireId))
			{
				bFire = true;
				m_bForceWeaponStateToFire = true;
			}
		}
		else
		{
			bFire = true;
		}

		if(bFire)
		{
			m_bFire = true;
			m_bFiredOnce = true;
		}
	}

	return FSM_Continue;
}

aiTask::FSM_Return CTaskAimGunScripted::Fire_OnUpdateClone()
{
	CPed* pPed = GetPed();

	Assert(pPed->IsNetworkClone());

	WeaponControllerState controllerState = GetWeaponControllerState(pPed);

	if(controllerState == WCS_Reload )
	{
		SetState(State_Reload);
	}
	else if ((controllerState == WCS_Fire || controllerState == WCS_FireHeld) &&
		pPed->GetPedIntelligence()->GetFiringPattern().WantsToFire() &&
		pPed->GetPedIntelligence()->GetFiringPattern().ReadyToFire())
	{	
		m_bFire = true;
	}
	else
	{
		SetState(State_Aim);
		m_bIgnoreRequestToAim = true;
	}

	return FSM_Continue;
}
////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return  CTaskAimGunScripted::Fire_OnExit()
{
	CPed* pPed = GetPed();
	// Register the event with the firing pattern
	if(pPed->GetPedIntelligence()->GetFiringPattern().IsBurstFinished())
	{
		pPed->GetPedIntelligence()->GetFiringPattern().ProcessBurstFinished();
	}

	m_MoveNetworkHelper.SetFlag(false, ms_FiringFlagId);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return  CTaskAimGunScripted::Outro_OnEnter()
{
	m_MoveNetworkHelper.WaitForTargetState(ms_OutroOnEnterId);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskAimGunScripted::Outro_OnUpdate()
{
	if (!m_MoveNetworkHelper.IsInTargetState())
	{
		m_MoveNetworkHelper.SendRequest(ms_OutroRequestId);
		return FSM_Continue;
	}

	if(m_bAbortOnIdle )
	{
		if(m_MoveNetworkHelper.GetBoolean(ms_OutroFinishedId))
		{
			// Need to abort the parent gun task when the outro finishes
			if (GetParent()&& GetParent()->GetTaskType()==CTaskTypes::TASK_GUN)
			{
				CTaskGun* pGunTask = static_cast<CTaskGun*>(GetParent());
				pGunTask->SetWeaponControllerType(CWeaponController::WCT_None);
			}
			SetState(State_Finish);
			return FSM_Continue;
		}	
	}
	else
	{
		m_MoveNetworkHelper.WaitForTargetState(ms_IdleOnEnterId);
		SetState(State_Idle);
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskAimGunScripted::Reload_OnEnter()
{
	CPed* pPed = GetPed();
	CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
	// Pointer should be checked in ProcessPreFSM
	taskAssert(pWeapon);

	// Inform the weapon we are starting the reload
	pWeapon->StartReload(pPed);

	m_MoveNetworkHelper.WaitForTargetState(ms_ReloadingOnEnterId);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskAimGunScripted::Reload_OnUpdate()
{
	if (!m_MoveNetworkHelper.IsInTargetState())
	{
		m_MoveNetworkHelper.SendRequest(ms_ReloadRequestId);
		return FSM_Continue;
	}

	if (m_MoveNetworkHelper.GetBoolean(ms_ReloadClipFinishedId))
	{
		m_bIgnoreRequestToAim = true;
		SetState(State_Aim);
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskAimGunScripted::Reload_OnExit()
{
	// Need to do a pointer check here because the on exit state can be called without calling ProcessPreFSM when being aborte
	CWeapon* pWeapon = GetPed()->GetWeaponManager()->GetEquippedWeapon();
	if (pWeapon)
	{
		pWeapon->DoReload();
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskAimGunScripted::Blocked_OnEnter()
{
	m_MoveNetworkHelper.WaitForTargetState(ms_BlockedOnEnterId);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskAimGunScripted::Blocked_OnUpdate()
{
	if (!m_MoveNetworkHelper.IsInTargetState())
	{
		m_MoveNetworkHelper.SendRequest(ms_BlockedRequestId);
		return FSM_Continue;
	}

	CPed* pPed = GetPed();
	if( !GetGunFlags().IsFlagSet( GF_ForceBlockedClips ) )
	{	
		if( GetIsFlagSet( aiTaskFlags::SubTaskFinished ) || !IsWeaponBlocked( pPed ) )
		{
			// Set state to decide what to do next
			SetState( State_Aim );
			return FSM_Continue;
		}
	}

	// Set the reset flag to let the hud know we want to dim the reticule
	pPed->SetPedResetFlag( CPED_RESET_FLAG_DimTargetReticule, true );
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskAimGunScripted::SwapWeapon_OnEnter()
{
	SetNewTask(rage_new CTaskSwapWeapon(SWAP_INSTANTLY));
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

aiTask::FSM_Return CTaskAimGunScripted::SwapWeapon_OnUpdate()
{
	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		if (GetPreviousState() == State_Aim)
		{
			m_bIgnoreRequestToAim = true;
			SetState(State_Aim);
			return FSM_Continue;
		}
		else
		{
			SetState(State_Idle);
			return FSM_Continue;
		}
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

const bool CTaskAimGunScripted::CheckCanFireAtPed(CPed* pPed)
{
	if(pPed->IsLocalPlayer())
	{
		CPlayerPedTargeting &targeting	= pPed->GetPlayerInfo()->GetTargeting();					
		const CEntity* pEntity = targeting.GetLockOnTarget() ? targeting.GetLockOnTarget() : targeting.GetFreeAimTargetRagdoll();
		CWeapon* pEquippedWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
		if(m_iFlags.IsFlagSet(GF_DisableBlockingClip) && pEquippedWeapon && !pEquippedWeapon->GetWeaponInfo()->GetIsNonViolent() && !pPed->IsAllowedToDamageEntity(pEquippedWeapon->GetWeaponInfo(), pEntity))
		{
			return false;
		}
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////

WeaponControllerState CTaskAimGunScripted::GetWeaponControllerState(const CPed* pPed) const 
{ 
	if(pPed->IsNetworkClone())
	{
		if(GetStateFromNetwork() == State_Aim)
		{
			return WCS_Aim;
		}
		if(GetStateFromNetwork() == State_Fire)
		{
			return WCS_Fire;
		}
		if(GetStateFromNetwork() == State_Reload)
		{
			return WCS_Reload;
		}
	}
	else
	{
		if (GetGunFlags().IsFlagSet(GF_FireAtLeastOnce)) 
		{
			return WCS_Fire;
		}
		return CWeaponControllerManager::GetController(m_weaponControllerType)->GetState(pPed, false); 
	}

	return WCS_None;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskAimGunScripted::UpdateMoVE()
{
	if (m_MoveNetworkHelper.IsNetworkActive() && GetState() != State_Idle)
	{
		// Calculate and send MoVE parameters
		CPed* pPed = GetPed();

		// Calculate the aim vector (this determines the heading and pitch angles to point the clips in the correct direction)
		Vector3 vStart, vEnd;

		//Set bAimFromCamera to true as we never want to change the heading based on collision.
		if (CTaskAimGun::CalculateFiringVector(pPed, vStart, vEnd, 0, NULL, true))
		{
			// Update target
			if(pPed->IsPlayer() || !pPed->GetPedResetFlag(CPED_RESET_FLAG_ShootFromGround))
				m_target.SetPosition(vEnd);

			// Compute desired yaw angle value
			float fDesiredYaw = CTaskAimGun::CalculateDesiredYaw(pPed, vStart, vEnd);

			if(!pPed->IsNetworkClone())
			{
				//Offset heading from data
				fDesiredYaw += m_fHeadingOffset;

				// Map the desired target heading to 0-1, depending on the range of the sweep
				m_fDesiredHeading = CTaskAimGun::ConvertRadianToSignal(fDesiredYaw, m_fMinHeadingAngle, m_fMaxHeadingAngle, false);

				bool bDoSmoothing = true;

				if (m_bAllowWrapping)
				{
					TUNE_GROUP_FLOAT(AIM_GUN_SCRIPTED, WRAP_TOL, 0.05f, 0.0f, 1.0f, 0.001f);
					if (m_fCurrentHeading < WRAP_TOL || m_fCurrentHeading > (1.0f - WRAP_TOL))
					{
						m_fCurrentHeading = m_fDesiredHeading;
						bDoSmoothing = false;
					}
				}

				if (bDoSmoothing)
				{
					// The desired heading is smoothed so it doesn't jump too much in a timestep
					m_fCurrentHeading = CTaskAimGun::SmoothInput(m_fCurrentHeading, m_fDesiredHeading, m_fHeadingChangeRate);
				}

				// Compute desired pitch angle value
				float fDesiredPitch = CTaskAimGun::CalculateDesiredPitch(pPed, vStart, vEnd);

				// Map the angle to the range 0.0->1.0
				m_fDesiredPitch = CTaskAimGun::ConvertRadianToSignal(fDesiredPitch, m_fMinPitchAngle, m_fMaxPitchAngle, false);

				// Compute the final signal value
				m_fCurrentPitch = CTaskAimGun::SmoothInput(m_fCurrentPitch, m_fDesiredPitch, m_fPitchChangeRate);
			}
			else
			{
				//Clone has m_fCurrentHeading and m_fCurrentPitch synced over network, override and use heading to set the yaw for intro and outro.
				fDesiredYaw = m_fCurrentHeading;
			}

			// Send the heading signal
			m_MoveNetworkHelper.SetFloat( ms_HeadingId, m_fCurrentHeading);

			// Send the signal
			m_MoveNetworkHelper.SetFloat( ms_PitchId, m_fCurrentPitch);

			// Calculate and send the intro blend param
			if (GetState() <= State_Intro)
			{
				// Send intro rate param
				TUNE_GROUP_FLOAT(AIM_GUN_SCRIPTED, introRate, 1.0f, 0.0f, 5.0f, 0.01f);
				m_MoveNetworkHelper.SetFloat(ms_IntroRateId, introRate);
				CalculateIntroBlendParam(fDesiredYaw);
			}

			// Calculate and send the outro blend param
			if (GetState() == State_Outro)
			{
				// Send outro rate param
				TUNE_GROUP_FLOAT(AIM_GUN_SCRIPTED, outroRate, 1.0f, 0.0f, 5.0f, 0.01f);
				m_MoveNetworkHelper.SetFloat(ms_OutroRateId, outroRate);
				CalculateOutroBlendParam(fDesiredYaw);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskAimGunScripted::IsPedAimingOrFiring(const CPed* pPed) const
{
	WeaponControllerState controllerState = GetWeaponControllerState(pPed);

	if (controllerState == WCS_Aim || controllerState == WCS_Fire || controllerState == WCS_FireHeld)
	{
		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskAimGunScripted::CalculateIntroBlendParam(float fDesiredYaw)
{
	if (m_fIntroHeadingBlend < 0.0f)
	{
		// The intro parameter specifies the blend of the intro clips which covers the full range of motion for each gun task
		// We have for example 90 degree left and 180 degree left intro clips, blending these equally, we get an
		// intro that leaves us at 135 degrees left
		m_fIntroHeadingBlend = CTaskAimGun::ConvertRadianToSignal(fDesiredYaw, m_fMinHeadingAngle, m_fMaxHeadingAngle, false);
	}

	m_MoveNetworkHelper.SetFloat(ms_IntroHeadingId, m_fIntroHeadingBlend);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskAimGunScripted::CalculateOutroBlendParam(float fDesiredYaw)
{
	if (m_fOutroHeadingBlend < 0.0f)
	{
		// Similar to intro
		m_fOutroHeadingBlend = CTaskAimGun::ConvertRadianToSignal(fDesiredYaw, m_fMinHeadingAngle, m_fMaxHeadingAngle, false);
	}

	m_MoveNetworkHelper.SetFloat(ms_OutroHeadingId, m_fOutroHeadingBlend);
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskAimGunScripted::UsesFireEvents()
{
	fwClipSet* pSet = fwClipSetManager::GetClipSet(m_ClipSetId);

	if(pSet)
	{
		static const fwMvClipId s_FireClipId("fire_0_additive",0x630D0D00);
		crClip* pClip = pSet->GetClip(s_FireClipId);
		if(pClip )
		{
			u32 uCount = 0;

			crTagIterator it(*pClip->GetTags());
			while (*it)
			{
				const crTag* pTag = *it;
				if(pTag->GetKey() == CClipEventTags::MoveEvent)
				{
					static const crProperty::Key MoveEventKey = crProperty::CalcKey("MoveEvent", 0x7EA9DFC4);
					const crPropertyAttribute* pAttrib = pTag->GetProperty().GetAttribute(MoveEventKey);
					if(pAttrib && pAttrib->GetType() == crPropertyAttribute::kTypeHashString)
					{
						static const atHashString s_FireHash("Fire",0xD30A036B);
						if(s_FireHash.GetHash() == static_cast<const crPropertyAttributeHashString*>(pAttrib)->GetHashString().GetHash())
						{
							++uCount;
						}
					}
				}
				++it;
			}

			if(uCount > 0)
			{
				return true;
			}
		}
	}
	return false;
}

const bool CTaskAimGunScripted::HasAdditiveFireAnim()
{
	fwClipSet* pSet = fwClipSetManager::GetClipSet(m_ClipSetId);

	if(pSet)
	{
		static const fwMvClipId s_FireClipId("fire_0_additive",0x630D0D00);
		crClip* pClip = pSet->GetClip(s_FireClipId);
		if(pClip )
		{
			return true;
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

#if !__FINAL

#if __BANK
void CTaskAimGunScripted::AddWidgets(bkBank& bank)
{
	bank.PushGroup("Task Debug");

	bank.AddToggle("Disable blocking", &CTaskAimGunScripted::ms_bDisableBlocking);

	bank.PopGroup(); 
}
#endif // __BANK

////////////////////////////////////////////////////////////////////////////////

void CTaskAimGunScripted::Debug() const
{
#if DEBUG_DRAW
	if (m_MoveNetworkHelper.IsNetworkActive())
	{
		const fwClipSet* pClipSet = m_MoveNetworkHelper.GetClipSet();
		if (pClipSet)
		{
			fwMvClipSetId clipSetId = m_MoveNetworkHelper.GetClipSetId();

			grcDebugDraw::AddDebugOutput(Color_green, "CTaskAimGunScripted : %s", clipSetId.GetCStr());
		}
	}

	const Vector3 vPedPosition = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition());
	static s32 s_iOffsetY = 50;
	char szText[64];
	formatf(szText, "DesiredHeading = %.4f", m_fDesiredHeading);
	grcDebugDraw::Text(vPedPosition, Color_green, 0, 0 + s_iOffsetY, szText);
	formatf(szText, "DesiredPitch = %.4f", m_fDesiredPitch);
	grcDebugDraw::Text(vPedPosition, Color_green, 0, 10 + s_iOffsetY, szText);
	formatf(szText, "CurrentHeading = %.4f", m_fCurrentHeading);
	grcDebugDraw::Text(vPedPosition, Color_green, 0, 20 + s_iOffsetY, szText);
	formatf(szText, "CurrentPitch = %.4f", m_fCurrentPitch);
	grcDebugDraw::Text(vPedPosition, Color_green, 0, 30 + s_iOffsetY, szText);
	formatf(szText, "IntroHeadingBlend = %.4f", m_fIntroHeadingBlend);
	grcDebugDraw::Text(vPedPosition, Color_green, 0, 40 + s_iOffsetY, szText);
	formatf(szText, "OutroHeadingBlend = %.4f", m_fOutroHeadingBlend);
	grcDebugDraw::Text(vPedPosition, Color_green, 0, 50 + s_iOffsetY, szText);

	Vector3 vTargetPos;
	if(m_target.GetPosition(vTargetPos))
	{
		grcDebugDraw::Sphere(vTargetPos, 0.1f, Color_red);
	}

#endif // DEBUG_DRAW

	if (GetSubTask())
		GetSubTask()->Debug();

	// Base class
	CTaskAimGun::Debug();
}

////////////////////////////////////////////////////////////////////////////////

const char * CTaskAimGunScripted::GetStaticStateName( s32 iState )
{
	switch (iState)
	{
		case State_Start:	return "State_Start";
		case State_Idle:	return "State_Idle";
		case State_Intro:	return "State_Intro";
		case State_Aim:		return "State_Aim";
		case State_Fire:	return "State_Fire";
		case State_Outro:	return "State_Outro";
		case State_Reload:	return "State_Reload";
		case State_Blocked:	return "State_Blocked";
		case State_SwapWeapon: return "State_SwapWeapon";
		case State_Finish:	return "State_Finish";
		default: taskAssert(0);
	}

	return "State_Invalid";
}
#endif // !__FINAL

void CTaskAimGunScripted::OnCloneTaskNoLongerRunningOnOwner()
{
	SetStateFromNetwork(State_Finish);
}

float CTaskAimGunScripted::GetScopeDistance() const
{
	return static_cast<float>(CTaskGun::CLONE_TASK_SCOPE_DISTANCE);
}

aiTask::FSM_Return CTaskAimGunScripted::UpdateClonedFSM(const s32 iState, const FSM_Event iEvent)
{

	FSM_Begin

		FSM_State(State_Start)
		FSM_OnUpdate
		return Start_OnUpdate();

	FSM_State(State_Idle)
		FSM_OnEnter
		return Idle_OnEnter();
	FSM_OnUpdate
		return Idle_OnUpdate();

	FSM_State(State_Intro)
		FSM_OnEnter
		return Intro_OnEnter();
	FSM_OnUpdate
		return Intro_OnUpdate();

	FSM_State(State_Aim)
		FSM_OnEnter
		return Aim_OnEnter();
	FSM_OnUpdate
		return Aim_OnUpdate();

	FSM_State(State_Fire)
		FSM_OnEnter
		return Fire_OnEnter();
	FSM_OnUpdate
		return Fire_OnUpdateClone();
	FSM_OnExit
		return Fire_OnExit();

	FSM_State(State_Outro)
		FSM_OnEnter
		return Outro_OnEnter();
	FSM_OnUpdate
		return Outro_OnUpdate();

	FSM_State(State_Reload)
		FSM_OnEnter
		return Reload_OnEnter();
	FSM_OnUpdate
		return Reload_OnUpdate();
	FSM_OnExit
		return Reload_OnExit();

	FSM_State(State_Blocked)
		FSM_OnEnter
		return Blocked_OnEnter();
	FSM_OnUpdate
		return Blocked_OnUpdate();

	FSM_State(State_SwapWeapon)
		FSM_OnEnter
		return SwapWeapon_OnEnter();
	FSM_OnUpdate
		return SwapWeapon_OnUpdate();

	FSM_State(State_Finish)
		FSM_OnUpdate
		return FSM_Quit;

	FSM_End
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------
// CClonedAimGunScriptedInfo
//----------------------------------------------------

CClonedAimGunScriptedInfo::CClonedAimGunScriptedInfo()
: m_fPitch(0.0f)
, m_fHeading(0.0f)
{
}

CClonedAimGunScriptedInfo::CClonedAimGunScriptedInfo(s32 iAimGunScriptedState, float fPitch, float fHeading)
: m_fPitch(fPitch)
, m_fHeading(fHeading)
{
	SetStatusFromMainTaskState(iAimGunScriptedState);
}
