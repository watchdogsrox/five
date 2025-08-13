//
// Task/Weapons/Gun/TaskReloadGun.h
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//

// Class header 
#include "Task/Weapons/Gun/TaskReloadGun.h"

// Game headers
#include "Animation/FacialData.h"
#include "Animation/Move.h"
#include "Animation/MovePed.h"
#include "Camera/CamInterface.h"
#include "Camera/Gameplay/GameplayDirector.h"
#include "camera/gameplay/aim/FirstPersonShooterCamera.h"
#include "Debug/DebugScene.h"
#include "Network/NetworkInterface.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Physics/gtaInst.h"
#include "Scene/World/GameWorld.h"
#include "Stats/StatsInterface.h"
#include "Task/Combat/Cover/TaskCover.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/Motion/Locomotion/TaskMotionAiming.h"
#include "Task/Motion/Locomotion/TaskMotionPed.h"
#include "Task/System/Tuning.h"
#include "Weapons/Components/WeaponComponentReloadData.h"
#include "Weapons/Weapon.h"
#include "Task/Weapons/TaskWeaponBlocked.h"

// Macro to disable optimisations if set
AI_OPTIMISATIONS()
AI_WEAPON_OPTIMISATIONS()
AI_COVER_OPTIMISATIONS()
NETWORK_OPTIMISATIONS()

fwMvClipId CClonedTaskReloadGunInfo::m_ClipIDNotUsedButNeededForSerialiseAnimationFunctionCall;

//////////////////////////////////////////////////////////////////////////
// CClonedTaskReloadGunInfo
//////////////////////////////////////////////////////////////////////////

CClonedTaskReloadGunInfo::CClonedTaskReloadGunInfo
(
	bool const		_bIsIdleReload,
	bool const		_bLoopReloadClip,
	bool const		_bEnteredLoopState,
	s32	const		_iNumTimesToPlayReloadLoop
)
:
	m_bIsIdleReload(_bIsIdleReload),
	m_bLoopReloadClip(_bLoopReloadClip),
	m_bEnteredLoopState(_bEnteredLoopState),
	m_iNumTimesToPlayReloadLoop(_iNumTimesToPlayReloadLoop)
{}

CClonedTaskReloadGunInfo::CClonedTaskReloadGunInfo()
:
	m_bIsIdleReload(false),
	m_bLoopReloadClip(false),
	m_bEnteredLoopState(false),
	m_iNumTimesToPlayReloadLoop(0)
{}

CClonedTaskReloadGunInfo::~CClonedTaskReloadGunInfo()
{}

CTaskFSMClone *CClonedTaskReloadGunInfo::CreateCloneFSMTask()
{
	// Pass NULL for the weapon controller as it's not needed in clone task....
	return rage_new CTaskReloadGun(CWeaponController::WCT_Reload, m_bIsIdleReload ? RELOAD_IDLE : 0 );
}

//////////////////////////////////////////////////////////////////////////
// CTaskReloadGun
//////////////////////////////////////////////////////////////////////////

const fwMvRequestId CTaskReloadGun::ms_ReloadRequestId("Reload",0x5DA46B68);
const fwMvRequestId CTaskReloadGun::ms_ReloadInRequestId("ReloadIn",0x51A8F83F);
const fwMvRequestId CTaskReloadGun::ms_ReloadLoopRequestId("ReloadLoop",0x26DB169B);
const fwMvRequestId CTaskReloadGun::ms_ReloadOutRequestId("ReloadOut",0x82FB75DB);
const fwMvBooleanId CTaskReloadGun::ms_ReloadOnEnterId("Reload_OnEnter",0x63CD5A69);
const fwMvBooleanId CTaskReloadGun::ms_ReloadInOnEnterId("ReloadIn_OnEnter",0xF4F9E9C5);
const fwMvBooleanId CTaskReloadGun::ms_ReloadLoopOnEnterId("ReloadLoop_OnEnter",0xB18699C4);
const fwMvBooleanId CTaskReloadGun::ms_ReloadOutOnEnterId("ReloadOut_OnEnter",0x13600930);
const fwMvBooleanId CTaskReloadGun::ms_ReloadLoopedId("ReloadAnimLoop",0x947EC4E3);
const fwMvBooleanId CTaskReloadGun::ms_ReloadInFinishedId("ReloadInAnimFinished",0xECC68483);
const fwMvBooleanId CTaskReloadGun::ms_ReloadLoopedFinishedId("ReloadAnimLoopFinished",0x4191E798);
const fwMvBooleanId CTaskReloadGun::ms_ReloadClipFinishedId("ReloadAnimFinished",0x1E6EFFA);
const fwMvBooleanId CTaskReloadGun::ms_LoopReloadId("LoopReload",0xD4B8BE1E);
const fwMvBooleanId CTaskReloadGun::ms_CreateOrdnance("CreateOrdnance",0x40BCB3F4);
const fwMvBooleanId CTaskReloadGun::ms_CreateAmmoClip("CreateAmmoClip",0x2DDC39F4);
const fwMvBooleanId CTaskReloadGun::ms_CreateAmmoClipInLeftHand("CreateAmmoClipInLeftHand",0xDD0E14B);
const fwMvBooleanId CTaskReloadGun::ms_DestroyAmmoClip("DestroyAmmoClip",0x94E8B34D);
const fwMvBooleanId CTaskReloadGun::ms_DestroyAmmoClipInLeftHand("DestroyAmmoClipInLeftHand",0x6ec3f514);
const fwMvBooleanId CTaskReloadGun::ms_ReleaseAmmoClip("ReleaseAmmoClip",0x2EDADD93);
const fwMvBooleanId CTaskReloadGun::ms_ReleaseAmmoClipInLeftHand("ReleaseAmmoClipInLeftHand",0xFEAB837A);
const fwMvBooleanId CTaskReloadGun::ms_Interruptible("Interruptible",0x550A14CF);
const fwMvBooleanId CTaskReloadGun::ms_BlendOut("BlendOut",0x6DD1ABBC);
const fwMvBooleanId CTaskReloadGun::ms_AllowSwapWeapon("AllowSwapWeapon",0xB248F4EC);
const fwMvBooleanId CTaskReloadGun::ms_HideGunFeedId("HideGunFeed",0xC8421217);
const fwMvBooleanId CTaskReloadGun::ms_AttachGunToLeftHand("AttachGunToLeftHand",0xf33d5a53);
const fwMvBooleanId CTaskReloadGun::ms_AttachGunToRightHand("AttachGunToRightHand",0xcf10a93d);
const fwMvFloatId CTaskReloadGun::ms_ReloadRateId("ReloadRate",0x6E144B8B);
const fwMvFloatId CTaskReloadGun::ms_ReloadPhaseId("ReloadPhase",0x24823469);
const fwMvFloatId CTaskReloadGun::ms_PitchId("Pitch",0x3F4BB8CC);
const fwMvClipId CTaskReloadGun::ms_ReloadClipId("ReloadClip",0x7F6D7B74);
const fwMvClipId CTaskReloadGun::ms_ReloadInClipId("ReloadInClip",0x86F51872);
const fwMvClipId CTaskReloadGun::ms_ReloadLoopClipId("ReloadLoopClip",0x840FDC0E);
const fwMvClipId CTaskReloadGun::ms_ReloadOutClipId("ReloadOutClip",0x73F6DA66);
const fwMvFilterId CTaskReloadGun::ms_UpperBodyFeatheredCoverFilterId("UpperbodyFeathered_Cover_filter",0xA59A96A5);

////////////////////////////////////////////////////////////////////////////////

bank_float CTaskReloadGun::ms_fReloadNetworkBlendInDuration		= SLOW_BLEND_DURATION;
bank_float CTaskReloadGun::ms_fReloadNetworkBlendOutDuration	= SLOW_BLEND_DURATION;

////////////////////////////////////////////////////////////////////////////////

CTaskReloadGun::CTaskReloadGun(const CWeaponController::WeaponControllerType weaponControllerType, s32 iFlags)
: m_weaponControllerType(weaponControllerType)
, m_ClipSetId(CLIP_SET_ID_INVALID)
, m_pHeldObject(NULL)
, m_fClipReloadRate(1.0f)
, m_iBulletsToReloadPerLoop(-1)
, m_iNumTimesToPlayReloadLoop(-1)
, m_fPitch(0.5f)
, m_fBlendOutDuration(ms_fReloadNetworkBlendOutDuration)
, m_pCachedReloadData(NULL)
, m_iFlags(iFlags)
, m_bLoopReloadClip(false)
, m_bEnteredLoopState(false)
, m_bCreatedProjectileOrdnance(false)
, m_bCreatedAmmoClip(false)
, m_bCreatedAmmoClipInLeftHand(false)
, m_bDestroyedAmmoClip(false)
, m_bDestroyedAmmoClipInLeftHand(false)
, m_bReleasedAmmoClip(false)
, m_bReleasedAmmoClipInLeftHand(false)
, m_bHoldingClip(false)
, m_bHoldingOrdnance(false)
, m_bStartedMovingWhenNotAiming(false)
, m_bPressedSprintWhenNotAiming(false)
, m_bInterruptRequested(false)
, m_bAnimSwapRequested(false)
, m_bReloadClipFinished(false)
, m_bGunFeedVisibile(true)
, m_nInitialGunAttachPoint(0)
, m_uCachedWeaponHash(0)
, m_bIsLoopedReloadFromEmpty(false)
{
	SetInternalTaskType(CTaskTypes::TASK_RELOAD_GUN);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskReloadGun::CleanUp()
{
	CPed *pPed = GetPed();

	//Check if we should create the ammo clip on clean-up.
	if(ShouldCreateAmmoClipOnCleanUp())
	{
		//Create the ammo clip.
		CreateAmmoClip();
	}

	// Force the weapon state timer and state
	weaponAssert(pPed->GetWeaponManager());
	CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
	if(pWeapon && pWeapon->GetWeaponInfo()->GetIsGunOrCanBeFiredLikeGun())
	{
		if(pWeapon->GetState() == CWeapon::STATE_RELOADING)
		{
			if (m_bInterruptRequested)
			{
				pWeapon->CancelReload();
			}
			else if(!pWeapon->GetWeaponInfo()->GetFlag(CWeaponInfoFlags::UseLoopedReloadAnim)) // We already loaded in the bullets while looping the reload
			{
				pWeapon->DoReload();
			}
		}

		pWeapon->StopAnim(NORMAL_BLEND_DURATION);
		pWeapon->SetWeaponIdleFlag(true);
	}

	if(pPed)
	{
		if (m_iFlags.IsFlagSet(RELOAD_SECONDARY))
		{
			pPed->GetMovePed().ClearSecondaryTaskNetwork(m_moveNetworkHelper, m_fBlendOutDuration);
		}
		else
		{
			pPed->GetMovePed().ClearTaskNetwork(m_moveNetworkHelper, m_fBlendOutDuration);
		}

		CTaskMotionBase* pMotionTask = pPed->GetPrimaryMotionTask();
		if(pMotionTask && pMotionTask->GetTaskType() == CTaskTypes::TASK_MOTION_PED)
		{
			static_cast<CTaskMotionPed*>(pMotionTask)->SetRestartAimingUpperBodyStateThisFrame(true);
		}

		CTaskGun* pGunTask = static_cast<CTaskGun*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_GUN));
		if(pGunTask)
		{
			pGunTask->SetBlockFiringTime(fwTimer::GetTimeInMilliseconds() + (u32)(m_fBlendOutDuration * 1000.0f));
		}

		if(!pPed->IsNetworkClone())
		{
			// "Reset" the firing pattern after reloading
			pPed->GetPedIntelligence()->GetFiringPattern().ProcessBurstFinished();
		}

		// Reset the fixup weight
		pPed->GetMovePed().SetUpperBodyFixupWeight(1.0f);
	}

	m_bGunFeedVisibile = true;
	if(pWeapon)
	{
		pWeapon->ProcessGunFeed(m_bGunFeedVisibile);
	}

	// Delete any clip held in left hand
	DeleteHeldObject();

	// Ensure gun is returned to original hand if we haven't already done so (e.g. if animation is broken out of).
	SwitchGunHands(m_nInitialGunAttachPoint);
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskReloadGun::ProcessPreFSM()
{
	// Make sure we have a valid weapon whilst running the reload task
	
	CPed *pPed = GetPed(); 
	weaponAssert(pPed->GetWeaponManager());

	const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
	if(!pWeapon || !pWeapon->GetWeaponInfo()->GetIsGunOrCanBeFiredLikeGun())
	{
		// Quit, as we have no gun
		return FSM_Quit;
	}

	if(m_uCachedWeaponHash != 0 && m_uCachedWeaponHash != pWeapon->GetWeaponHash())
	{
		// Quit, as we are no longer holding the same weapon we started the reload task with
		return FSM_Quit;
	}

	const CWeaponInfo* pWeaponInfo = pWeapon->GetWeaponInfo();

	if(!taskVerifyf(pWeaponInfo, "Weapon has no info, check weapons.meta"))
	{
		return FSM_Quit;
	}

	if (pPed->IsLocalPlayer())
	{
		if (!m_iFlags.IsFlagSet(RELOAD_IDLE))
		{
			const bool bProjectileWeapon    = (pWeapon->GetWeaponInfo()) ? pWeapon->GetWeaponInfo()->GetIsThrownWeapon() : false;
			const bool bConsiderFireTrigger = !bProjectileWeapon;

			weaponAssert(pPed->GetPlayerInfo());
			const bool bAimingOrFiring      = pPed->GetPlayerInfo()->IsAiming(bConsiderFireTrigger) || (bConsiderFireTrigger && pPed->GetPlayerInfo()->IsFiring()) FPS_MODE_SUPPORTED_ONLY(|| pPed->IsFirstPersonShooterModeEnabledForPlayer());

			if (!bAimingOrFiring)
			{
				const CTaskGun* pTaskGun = static_cast<const CTaskGun*>(FindParentTaskOfType(CTaskTypes::TASK_GUN));
				const bool bIsAssistedAimingOrRunAndGun = pTaskGun ? (pTaskGun->GetIsAssistedAiming() || pTaskGun->GetIsRunAndGunAiming()) : false;
				const bool bMoving = pPed->GetMotionData()->GetDesiredMoveBlendRatio().Mag2() > 0.f;
				m_bStartedMovingWhenNotAiming = m_bStartedMovingWhenNotAiming || bMoving;

				bool bIsSprintOrSelectHeld = false;
				CControl* pControl = pPed->GetControlFromPlayer();
				if(pControl && (pControl->GetPedSprintIsDown() || pControl->GetSelectWeapon().IsDown()))
				{
					bIsSprintOrSelectHeld = true;
				}

				if(bIsAssistedAimingOrRunAndGun)
				{
					m_bPressedSprintWhenNotAiming |= bIsSprintOrSelectHeld;
				}

				pPed->SetIsStrafing((bIsAssistedAimingOrRunAndGun && !m_bPressedSprintWhenNotAiming) || !m_bStartedMovingWhenNotAiming);
			}
			else
			{
				pPed->SetIsStrafing(true);
			}
		}
		else
		{
			pPed->SetIsStrafing(false);
		}
	}
	else if(!pPed->IsNetworkClone() && FindParentTaskOfType(CTaskTypes::TASK_GUN))
	{
		pPed->SetIsStrafing(true);
	}

	pPed->SetPedResetFlag(CPED_RESET_FLAG_ProcessPostCamera, true);
	pPed->SetPedResetFlag(CPED_RESET_FLAG_ProcessPreRender2, true );
	
#if FPS_MODE_SUPPORTED
	// B*2017702: Block camera switching if player is reloading while in FPS mode
	// Previously we were aborting the task and doing an auto-reload on cleanup
	if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true) && pPed->IsPlayer())
	{
		pPed->SetPlayerResetFlag(CPlayerResetFlags::PRF_DISABLE_CAMERA_VIEW_MODE_CYCLE);
	}
#endif	//FPS_MODE_SUPPORTED

	// Don't switch camera during reloading.
	pPed->SetPedResetFlag(CPED_RESET_FLAG_BlockCameraSwitching, true );

	const CWeaponUpperBodyFixupExpressionData* pFixupData = pWeapon->GetWeaponInfo()->GetReloadUpperBodyFixupExpressionData();
	if(pFixupData)
	{
		float fFixupWeight = pFixupData->GetFixupWeight(*pPed);
#if FPS_MODE_SUPPORTED
		// Use full fix-up weight (1.0) for reloading in first person mode
		if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true) && pPed->IsPlayer())
		{
			fFixupWeight = 1.0f;
		}
#endif
		pPed->GetMovePed().SetUpperBodyFixupWeight(fFixupWeight);
	}

	// Facial
	CFacialDataComponent* pFacialData = GetPed()->GetFacialData();
	if(pFacialData)
	{
		//Request the angry facial idle clip.
		pFacialData->RequestFacialIdleClip(CFacialDataComponent::FICT_DriveFast);
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskReloadGun::ProcessPreRender2()
{
	bool bAllow1HandedAttachToRightGrip = true;
	bool bAllowIK = true;
	CWeapon* pWeapon = GetPed()->GetWeaponManager()->GetEquippedWeapon();
	if(pWeapon)
	{
		const CWeaponInfo* pWeaponInfo = pWeapon->GetWeaponInfo();
		if (pWeaponInfo)
		{
			if(pWeaponInfo->GetUseLeftHandIkWhenAiming())
			{
				bAllow1HandedAttachToRightGrip = false;
			}

			if(GetPed()->GetPedResetFlag(CPED_RESET_FLAG_InCoverTaskActive) && pWeaponInfo->GetIsTwoHanded() && pWeaponInfo->GetIs1HandedInCover())
			{
				bAllow1HandedAttachToRightGrip = false;
			}

			if(pWeaponInfo->GetAttachReloadObjectToRightHand() && 
				GetPed()->GetWeaponManager()->GetPedEquippedWeapon() && 
				GetPed()->GetWeaponManager()->GetPedEquippedWeapon()->GetObject())
			{
				CPedEquippedWeapon::eAttachPoint eCurrentAttachPoint = 	GetPed()->GetWeaponManager()->GetPedEquippedWeapon()->GetAttachPoint();
				if( eCurrentAttachPoint == CPedEquippedWeapon::AP_LeftHand )
				{
					bAllowIK = false;
				}
			}
		}

		//Handle the visiblity of the gun feed
		pWeapon->ProcessGunFeed(m_bGunFeedVisibile);
	}

	if(bAllowIK)
	{
		GetPed()->ProcessLeftHandGripIk(bAllow1HandedAttachToRightGrip);
	}
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskReloadGun::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();
		FSM_State(State_Reload)
			FSM_OnEnter
				return Reload_OnEnter();
			FSM_OnUpdate
				return Reload_OnUpdate();
		FSM_State(State_ReloadIn)
			FSM_OnEnter
				return ReloadIn_OnEnter();
			FSM_OnUpdate
				return ReloadIn_OnUpdate();
		FSM_State(State_ReloadLoop)
			FSM_OnEnter
				return ReloadLoop_OnEnter();
			FSM_OnUpdate
				return ReloadLoop_OnUpdate();
		FSM_State(State_ReloadOut)
			FSM_OnEnter
				return ReloadOut_OnEnter();
			FSM_OnUpdate
				return ReloadOut_OnUpdate();
		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

////////////////////////////////////////////////////////////////////////////////

CTaskReloadGun::FSM_Return	CTaskReloadGun::UpdateClonedFSM(const s32 iState, const FSM_Event iEvent)
{
	u32 desiredWeapon = GetPed()->GetWeaponManager()->GetEquippedWeaponHash();
	u32 currentWeapon = GetPed()->GetWeaponManager()->GetEquippedWeapon() ? GetPed()->GetWeaponManager()->GetEquippedWeapon()->GetWeaponInfo()->GetHash() : 0;

	// quit the task if the player is holding the wrong weapon object, that does not correspond to the hash of the weapon he should be holding
	if(GetStateFromNetwork() == State_Finish || desiredWeapon != currentWeapon)
	{
		return FSM_Quit;
	}

	return UpdateFSM(iState, iEvent);
}

////////////////////////////////////////////////////////////////////////////////

CTaskInfo* CTaskReloadGun::CreateQueriableState() const
{
	return rage_new CClonedTaskReloadGunInfo( m_iFlags.IsFlagSet(RELOAD_IDLE), m_bLoopReloadClip, m_bEnteredLoopState, m_iNumTimesToPlayReloadLoop);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskReloadGun::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	Assert( pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_RELOAD_GUN );
    CClonedTaskReloadGunInfo *reloadGunInfo = static_cast<CClonedTaskReloadGunInfo*>(pTaskInfo);

	m_iFlags.ChangeFlag(RELOAD_IDLE, reloadGunInfo->GetIsIdleReload());
	m_bLoopReloadClip			= reloadGunInfo->GetLoopReloadClip();
	m_bEnteredLoopState			= reloadGunInfo->GetEnteredLoopState();
	m_iNumTimesToPlayReloadLoop = reloadGunInfo->GetNumTimesToPlayReloadLoop();

	CTaskFSMClone::ReadQueriableState(pTaskInfo);	
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskReloadGun::ControlPassingAllowed(CPed *UNUSED_PARAM(pPed), const netPlayer& UNUSED_PARAM(player), eMigrationType UNUSED_PARAM(migrationType))
{
	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskReloadGun::OnCloneTaskNoLongerRunningOnOwner()
{
	SetStateFromNetwork(State_Finish);
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskReloadGun::ProcessPostFSM()
{
	if(m_moveNetworkHelper.IsNetworkActive())
	{
		CPed* pPed = GetPed();

		m_moveNetworkHelper.SetFloat(ms_ReloadRateId, m_fClipReloadRate);
		m_moveNetworkHelper.SetBoolean(ms_LoopReloadId, m_bLoopReloadClip);
	
		static const fwMvBooleanId s_DisableSpineIk("DisableSpineIk",0xDB8A8E38);
		static const fwMvBooleanId s_DisableSpineIkWhenNotMoving("DisableSpineIkWhenNotMoving",0xEE54CFEE);
		static const fwMvFlagId s_UseSpineIk("UseUpperBodyShadow",0xD39BAD33);
		bool bDisableSpineIk = pPed->GetIsInCover() || m_moveNetworkHelper.GetBoolean(s_DisableSpineIk) || (m_moveNetworkHelper.GetBoolean(s_DisableSpineIkWhenNotMoving) && pPed->GetMotionData()->GetIsStill());
		m_moveNetworkHelper.SetFlag(!bDisableSpineIk, s_UseSpineIk);
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskReloadGun::ProcessPostCamera()
{
	CPed* pPed = GetPed();

#if FPS_MODE_SUPPORTED
	if(pPed->IsFirstPersonShooterModeEnabledForPlayer())
	{
		CTaskAimGunOnFoot::PostCameraUpdate(pPed, m_iFlags.IsFlagSet(GF_DisableTorsoIk));
	}
	else
#endif // FPS_MODE_SUPPORTED
	{
		// If reloading from aiming, set the peds desired heading
		if(pPed->IsLocalPlayer())
		{
			const camGameplayDirector& gameplayDirector = camInterface::GetGameplayDirector();

			if(!pPed->GetUsingRagdoll() && !pPed->GetPlayerInfo()->AreControlsDisabled())
			{
				// Attempt to update the ped's orientation and IK aim target *after* the camera system has updated, but before the aim IK is updated
				// Don't mess with the desired heading when in cover as it will cause a pop
				if (gameplayDirector.IsAiming(pPed) && !pPed->GetIsInCover())
				{
					// Update the ped's (desired) heading
					const camFrame& aimCameraFrame	= camInterface::GetPlayerControlCamAimFrame();
					float aimHeading				= aimCameraFrame.ComputeHeading();
					pPed->SetDesiredHeading(aimHeading);
				}
			}

			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskReloadGun::ProcessMoveSignals()
{
	CPed& rPed = *GetPed();
	rPed.SetPedResetFlag(CPED_RESET_FLAG_IsReloading, true);

	if (GetState() >= State_Reload && GetState() < State_Finish)
	{
		// Create, destroy or release ammo based on move events
		ProcessClipEvents();

		// If the reload clip has finished, record that it finished
		if (m_moveNetworkHelper.GetBoolean(ms_ReloadClipFinishedId))
		{
			m_bReloadClipFinished = true;
			return false;
		}

		//Handle the visibility of the gun feed
		m_bGunFeedVisibile = true;
		if(m_moveNetworkHelper.GetBoolean(ms_HideGunFeedId))
		{
			m_bGunFeedVisibile = false;
		}

		// Still waiting.
		return true;
	}

	// Nothing to do.
	return false;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskReloadGun::Start_OnUpdate()
{
	CPed& ped = *GetPed();
	ped.SetPedResetFlag(CPED_RESET_FLAG_IsReloading, true);
	CWeapon& weapon = *ped.GetWeaponManager()->GetEquippedWeapon();
	m_uCachedWeaponHash = weapon.GetWeaponHash();

	m_nInitialGunAttachPoint = ped.GetWeaponManager()->GetPedEquippedWeapon() ? (u8) ped.GetWeaponManager()->GetPedEquippedWeapon()->GetAttachPoint() : 0;

	if (NetworkInterface::IsGameInProgress() && ped.IsNetworkClone())
	{
		if (!weapon.GetHasInfiniteClips(GetPed()))
		{
			CObject* pWeaponObject = ped.GetWeaponManager()->GetEquippedWeaponObject();
			if (pWeaponObject)
			{
				CTaskGun* pGunTask = static_cast<CTaskGun*>(ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_GUN));
				if(pGunTask && pGunTask->GetReloadingOnEmpty())
				{
					//Note: If we have 1 round left fire it, otherwise just continue here.
					s32 ammoInClip = weapon.GetAmmoInClip();
					if (ammoInClip == 1)
					{
						Matrix34 matWeapon;
						if(weapon.GetWeaponModelInfo()->GetBoneIndex( WEAPON_MUZZLE ) != -1)
						{
							pWeaponObject->GetGlobalMtx( weapon.GetWeaponModelInfo()->GetBoneIndex( WEAPON_MUZZLE ), matWeapon );
						}
						else
						{
							pWeaponObject->GetMatrixCopy( matWeapon );
						}

						CWeapon::sFireParams params( GetPed(), matWeapon, NULL, NULL );

						//Fire once
						weapon.Fire( params );
					}
				}
			}
		}
	}

	// Get the weapon pointer (should be checked in ProcessPreFSM)
	const CWeaponInfo* pWeaponInfo = weapon.GetWeaponInfo();
	if (pWeaponInfo)
	{
		m_ClipSetId = pWeaponInfo->GetAppropriateReloadWeaponClipSetId(&ped);
	}

	if (!taskVerifyf(m_ClipSetId != CLIP_SET_ID_INVALID, "Weapon info doesn't have a valid clipset, check weapons.meta"))
	{
		return FSM_Quit;
	}

	fwClipSet* pClipSet = fwClipSetManager::GetClipSet(m_ClipSetId);
	if (!taskVerifyf(pClipSet, "Clip set [%s] doesn't exist", m_ClipSetId.GetCStr()))
	{
		return FSM_Quit;
	}

	if (ped.HasHurtStarted())
		return FSM_Quit;

	// If we're waiting too long for streaming, quit
	static dev_float TIME_OUT = 2.f;
	if (GetTimeInState() >= TIME_OUT)
	{
		CWeapon* pWeapon = GetPed()->GetWeaponManager()->GetEquippedWeapon();
		if(pWeapon)
		{
			// Reload the weapon anyway
			pWeapon->DoReload();
		}
		return FSM_Quit;
	}

	// Wait for the animations to be streamed in
	if (!pClipSet->Request_DEPRECATED())
	{
		return FSM_Continue;
	}

	if (StartMoveNetwork(ped))
	{
		const CWeaponInfo* pWeaponInfo = weapon.GetWeaponInfo();
		taskFatalAssertf(pWeaponInfo, "NULL weapon info pointer in CTaskReloadGun::Start_OnUpdate()");

		m_iBulletsToReloadPerLoop = pWeaponInfo->GetBulletsPerAnimLoop();

		// Set the rate at which to play the clips from the weapon info
		m_fClipReloadRate = pWeaponInfo->GetAnimReloadRate();

		// Allow weapon flag to ignore all rate modifiers
		if (!pWeaponInfo->GetIgnoreAnimReloadRateModifiers())
		{
			// Modify by reload data specific multiplier
			const CWeaponComponentReloadData* pReloadData = GetReloadData();
			if (pReloadData)
			{
				m_fClipReloadRate *= pReloadData->GetReloadAnimRateModifier();
			}

			// Modify by MP scalar
			if (CAnimSpeedUps::ShouldUseMPAnimRates())
			{
				if (ped.IsPlayer())
				{
					m_fClipReloadRate *= CAnimSpeedUps::ms_Tunables.m_MultiplayerReloadRateModifier;
				}
			}
			else if (ped.IsLocalPlayer())
			{
				static const float ONE_OVER_100 = 1.f / 100.f;
				float fShootingAbility = StatsInterface::GetFloatStat(StatsInterface::GetStatsModelHashId("SHOOTING_ABILITY")) * ONE_OVER_100;

				TUNE_GROUP_FLOAT(PED_RELOAD, SKILL_NORMAL, 0.69f, 0.f, 1.f, 0.01f);
				TUNE_GROUP_FLOAT(PED_RELOAD, SKILL_MIN_RELOAD_RATE, 0.85f, 0.5f, 1.f, 0.01f);
				TUNE_GROUP_FLOAT(PED_RELOAD, SKILL_MAX_RELOAD_RATE, 1.3f, 1.f, 2.f, 0.01f);

				float fReloadMod = 1.f;
				if(fShootingAbility < SKILL_NORMAL)
				{
					float fRatio = fShootingAbility / SKILL_NORMAL;
					fReloadMod = SKILL_MIN_RELOAD_RATE + (fRatio * (1.f - SKILL_MIN_RELOAD_RATE));
				}
				else
				{
					float fRatio = (fShootingAbility - SKILL_NORMAL) / (1.f - SKILL_NORMAL);
					fReloadMod = 1.f + (fRatio * (SKILL_MAX_RELOAD_RATE - 1.f));
				}

				m_fClipReloadRate *= fReloadMod;
			}
		}

		// if we're a clone, this should be set already...
		if(ped.IsNetworkClone())
		{
			// Queriable state doesn't seem to be run in the cover secondary case, so just force set it (don't do multiple loops now anyway)
			CTask* pTask = ped.GetPedIntelligence()->FindTaskActiveByTreeAndType(PED_TASK_TREE_SECONDARY, CTaskTypes::TASK_RELOAD_GUN);
			if (pTask)
			{
				m_iNumTimesToPlayReloadLoop = 1;
			}

			// B*900189 - If remote is waiting for "if (!pClipSet->Request_DEPRECATED())" then clones' m_iNumTimesToPlayReloadLoop will be -1 here, so make clone wait also
			if(m_iNumTimesToPlayReloadLoop == -1)
			{
				return FSM_Continue;
			}

			// if it's been set to 0 then we're cloning the task just as it's finished reloading on the owner
			if(!m_iNumTimesToPlayReloadLoop)
			{
				// so we just bail...
				SetState(State_Finish);
				return FSM_Continue;
			}
		}
		else
		{
			// Compute number of times to play the reload loop
			m_iNumTimesToPlayReloadLoop = ComputeNumberOfTimesToPlayReloadLoop(weapon, *pWeaponInfo);
		}

		taskAssertf(m_iNumTimesToPlayReloadLoop > 0, "Weapon doesn't need reloading");

		//Check if we should say the audio.
		bool bSayAudio = (!ped.IsPlayer());
		if(!bSayAudio)
		{
			static dev_float s_fMaxDistance = 35.0f;

			//Check if the player is in a group.
			const CPedGroup* pPedGroup = ped.GetPedsGroup();
			if(pPedGroup && (pPedGroup->FindDistanceToNearestMember() <= s_fMaxDistance))
			{
				bSayAudio = true;
			}
			//Check if there is a friendly ped nearby.
			else if(CFindClosestFriendlyPedHelper::Find(ped, s_fMaxDistance) != NULL)
			{
				bSayAudio = true;
			}
		}
		if(bSayAudio)
		{
			ped.NewSay("RELOADING");
		}

		//make sure the weapon is rendering
		CObject* pWeaponObj = GetPed()->GetWeaponManager()->GetEquippedWeaponObject();
		if( pWeaponObj && !pWeaponObj->GetIsVisible() )
		{	
			pWeaponObj->SetIsVisibleForModule( SETISVISIBLE_MODULE_GAMEPLAY, true, true );
		}

		// Signal to the weapon to start reloading
		weapon.StartReload(&ped);

		// Decide which state we need to be in based on if we need to loop an clip for each bullet reloaded
		if (!pWeaponInfo->GetUseLoopedReloadAnim())
		{
			SetState(State_Reload);
		}
		else
		{
			SetState(State_ReloadIn);
		}

		if ((ped.GetPedResetFlag(CPED_RESET_FLAG_InstantBlendToAim) || ped.GetPedResetFlag(CPED_RESET_FLAG_InstantBlendToAimFromScript)) && !m_iFlags.IsFlagSet(RELOAD_IDLE))
		{
			ped.ForceMotionStateThisFrame(CPedMotionStates::MotionState_Aiming);
		}

	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskReloadGun::Reload_OnEnter()
{
	CPed& ped = *GetPed();

	// Just precaution incase we started reload while beeing healthy but thee streaming took time and now we are hurt when streaming is done
	if (ped.HasHurtStarted())
		return FSM_Quit;

	// Setup move network helper
	StartReloadClips(ped, GetReloadData(), ms_ReloadClipId);

	// Send request
	m_moveNetworkHelper.SendRequest(ms_ReloadRequestId);
	m_moveNetworkHelper.WaitForTargetState(ms_ReloadOnEnterId);

	// If MAKE_PED_RELOAD had been used, consider that request fulfilled.
	ped.SetPedConfigFlag(CPED_CONFIG_FLAG_ForceReload, false);

	// Init the pitch
	m_fPitch = 0.5f;

	// Let ProcessMoveSignals() handle the communication with the Move network in case we're being timesliced
	m_bReloadClipFinished = false;
	RequestProcessMoveSignalCalls();

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskReloadGun::Reload_OnUpdate()
{
	CPed& ped = *GetPed();

	SendPitchParam(ped);

	if (ped.GetUsingRagdoll())
	{
		ReloadOnCleanUp(ped);
		SetState(State_Finish);
		return FSM_Continue;
	}

	if (!m_moveNetworkHelper.IsInTargetState())
		return FSM_Continue;

	if (m_bAnimSwapRequested)
	{
		// Get the weapon pointer (should be checked in ProcessPreFSM)
		CWeapon* pWeapon = ped.GetWeaponManager()->GetEquippedWeapon();
		if (pWeapon)
		{
			const CWeaponInfo* pWeaponInfo = pWeapon->GetWeaponInfo();
			if (pWeaponInfo)
			{
				m_ClipSetId = pWeaponInfo->GetAppropriateReloadWeaponClipSetId(&ped);
				if (m_ClipSetId != CLIP_SET_ID_INVALID)
				{
					fwClipSet* pClipSet = fwClipSetManager::GetClipSet(m_ClipSetId);
					if (pClipSet->IsStreamedIn_DEPRECATED())
					{
						m_moveNetworkHelper.SendRequest(ms_ReloadRequestId);
						StartReloadClips(ped, GetReloadData(), ms_ReloadClipId);
					}					
				}				
				m_bAnimSwapRequested = false;
			}
		}
	}

	bool bReloadTimeout = false;
	if (GetTimeInState() >= 10.0f)
	{
		taskWarningf("Reload Took Too Long, timing out.");
		bReloadTimeout = true;
	}

	if (m_bReloadClipFinished || ComputeShouldInterrupt() || m_bInterruptRequested || ShouldBlendOut() || bReloadTimeout)
	{
		ReloadOnCleanUp(ped);
		SetState(State_Finish);
	}

#if FPS_MODE_SUPPORTED
	if (ped.IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		bool bIsBlocked = !ped.IsNetworkClone() && IsWeaponBlocked();
		if(bIsBlocked)
		{
			m_bInterruptRequested = true;
			CTaskGun* pGunTask = static_cast<CTaskGun*>(ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_GUN));
			if(pGunTask)
			{
				pGunTask->SetReloadIsBlocked(true);
				Vec3V vPedPos = ped.GetTransform().GetPosition();
				pGunTask->SetPedPosWhenReloadIsBlocked(vPedPos);
				pGunTask->SetPedHeadingWhenReloadIsBlocked(ped.GetCurrentHeading());

				// Store the relative position if the ped is on a ground physical.
				CPhysical* pGroundPhysical = ped.GetGroundPhysical();
				if(pGroundPhysical)
				{
					vPedPos = UnTransformFull(pGroundPhysical->GetTransform().GetMatrix(), vPedPos);
					pGunTask->SetPedPosWhenReloadIsBlocked(vPedPos);
				}
			}
			SetState(State_Finish);
		}
	}
#endif	// FPS_MODE_SUPPORTED

	return FSM_Continue;
}


////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskReloadGun::ReloadIn_OnEnter()
{
	CPed& ped = *GetPed();

	CWeapon* pWeapon = ped.GetWeaponManager()->GetEquippedWeapon();
	if(pWeapon)
	{
		m_bIsLoopedReloadFromEmpty = pWeapon->GetIsClipEmpty();
	}

	// Setup move network helper
	const CWeaponComponentReloadLoopedData* pReloadData = GetLoopedReloadData();
	StartReloadClips(ped, pReloadData ? &pReloadData->GetSection(CWeaponComponentReloadLoopedData::Intro) : NULL, ms_ReloadInClipId);

	// Send the request
	m_moveNetworkHelper.SendRequest(ms_ReloadInRequestId);
	m_moveNetworkHelper.WaitForTargetState(ms_ReloadInOnEnterId);

	// If MAKE_PED_RELOAD had been used, consider that request fulfilled.
	ped.SetPedConfigFlag(CPED_CONFIG_FLAG_ForceReload, false);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskReloadGun::ReloadIn_OnUpdate()
{
	if (!m_moveNetworkHelper.IsInTargetState())
		return FSM_Continue;

	CPed& ped = *GetPed();

	SendPitchParam(ped);

	// The reload in clip counts as one of the bullet reloads, so if we only needed one, go straight to the outro
	if (m_moveNetworkHelper.GetBoolean(ms_ReloadInFinishedId))
	{
		CWeapon* pWeapon = ped.GetWeaponManager()->GetEquippedWeapon();
		if(pWeapon)
		{
			// Skip the loop if we have already loaded the last bullet
			if(pWeapon->GetAmmoInClip() == pWeapon->GetClipSize())
			{
				SetState(State_ReloadOut);
			}
			else
			{
				SetState(State_ReloadLoop);
			}
				return FSM_Continue;
		}
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskReloadGun::ReloadLoop_OnEnter()
{
	CPed& ped = *GetPed();

	if(!ped.IsNetworkClone())
	{
		UpdateBulletReloadStatus();
	}

	// Reset some clip events, as we need to listen to some of these once per loop
	m_bCreatedAmmoClip				= false;
	m_bCreatedAmmoClipInLeftHand	= false;
	m_bDestroyedAmmoClip			= false;
	m_bDestroyedAmmoClipInLeftHand	= false;

	// Don't bother sending the request and waiting for the on enter if we are already in the correct MoVE state
	if (!m_bEnteredLoopState)
	{
		// Setup move network helper
		const CWeaponComponentReloadLoopedData* pReloadData = GetLoopedReloadData();
		StartReloadClips(ped, pReloadData ? &pReloadData->GetSection(CWeaponComponentReloadLoopedData::Loop) : NULL, ms_ReloadLoopClipId);

		// Send request
		m_moveNetworkHelper.SendRequest(ms_ReloadLoopRequestId);
		m_moveNetworkHelper.WaitForTargetState(ms_ReloadLoopOnEnterId);
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskReloadGun::ReloadLoop_OnUpdate()
{
	CPed& ped = *GetPed();

	SendPitchParam(ped);

	if (!m_moveNetworkHelper.IsInTargetState())
	{
		return FSM_Continue;
	}

	if(GetPed()->IsNetworkClone() && GetStateFromNetwork() == State_ReloadOut)
	{
		SetState(State_ReloadOut);
		return FSM_Continue;
	}

	CWeapon* pWeapon = ped.GetWeaponManager()->GetEquippedWeapon();
	if(pWeapon)
	{
		// Check if the player wants to interrupt the loop animation
		if (ComputeShouldInterrupt() || m_bInterruptRequested || ShouldBlendOut())
		{

			if (!m_bInterruptRequested)
			{
				// Do this here before calling DoReload, as without an ammo clip, we will not reload the right amount of ammo
				if(ShouldCreateAmmoClipOnCleanUp())
				{
					//Create the ammo clip.
					CreateAmmoClip();
				}
				pWeapon->DoReload();
			}
			SetState(State_Finish);
			return FSM_Continue;
		}

		// Don't want to acknowledge the same clip event more than once 
		if (GetTimeInState() > 0.0f)
		{
			// If we're going to loop the reload clip
			if (m_bLoopReloadClip)
			{
				// Wait for the flag generated when the clip loops
				if (m_moveNetworkHelper.GetBoolean(ms_ReloadLoopedId))
				{

					// Check if the player wants to fire immediately
					if (ped.IsLocalPlayer())
					{
						WeaponControllerState controllerState = CWeaponControllerManager::GetController(m_weaponControllerType)->GetState(&ped, false);

						if (controllerState == WCS_Fire || controllerState == WCS_FireHeld)
						{
							// Play the reload outro
							SetState(State_ReloadOut);
							return FSM_Continue;
						}
					}

					// Restart the state to do the on enter state again (ignore sending requests etc)
					m_bEnteredLoopState = true;
					SetFlag(aiTaskFlags::RestartCurrentState);
				}
			}
			else 
			{
				// Wait for the clip finished event
				if (m_moveNetworkHelper.GetBoolean(ms_ReloadLoopedFinishedId))
				{
					// Play the reload outro
					SetState(State_ReloadOut);
				}
			}
		}
	}

#if FPS_MODE_SUPPORTED
	if (ped.IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		bool bIsBlocked = !ped.IsNetworkClone() && IsWeaponBlocked();
		if(bIsBlocked)
		{
			m_bInterruptRequested = true;
			CTaskGun* pGunTask = static_cast<CTaskGun*>(ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_GUN));
			if(pGunTask)
			{
				pGunTask->SetReloadIsBlocked(true);
				Vec3V vPedPos = ped.GetTransform().GetPosition();
				pGunTask->SetPedPosWhenReloadIsBlocked(vPedPos);
				pGunTask->SetPedHeadingWhenReloadIsBlocked(ped.GetCurrentHeading());

				// Store the relative position if the ped is on a ground physical.
				CPhysical* pGroundPhysical = ped.GetGroundPhysical();
				if(pGroundPhysical)
				{
					vPedPos = UnTransformFull(pGroundPhysical->GetTransform().GetMatrix(), vPedPos);
					pGunTask->SetPedPosWhenReloadIsBlocked(vPedPos);
				}
			}
			SetState(State_Finish);
		}
	}
#endif	// FPS_MODE_SUPPORTED

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskReloadGun::ReloadOut_OnEnter()
{
	CPed& ped = *GetPed();

	// Setup move network helper
	const CWeaponComponentReloadLoopedData* pReloadData = GetLoopedReloadData();
	StartReloadClips(ped, pReloadData ? &pReloadData->GetSection(CWeaponComponentReloadLoopedData::Outro) : NULL, ms_ReloadOutClipId);

	// Send the request
	m_moveNetworkHelper.SendRequest(ms_ReloadOutRequestId);
	m_moveNetworkHelper.WaitForTargetState(ms_ReloadOutOnEnterId);

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskReloadGun::ReloadOut_OnUpdate()
{
	CPed& ped = *GetPed();

	SendPitchParam(ped);

	if (!m_moveNetworkHelper.IsInTargetState())
		return FSM_Continue;

	if (m_moveNetworkHelper.GetBoolean(ms_ReloadClipFinishedId))
	{
		SetState(State_Finish);
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskReloadGun::StartMoveNetwork(CPed& ped)
{
	m_moveNetworkHelper.CreateNetworkPlayer(&ped, CClipNetworkMoveInfo::ms_NetworkTaskReloadGun);
	m_moveNetworkHelper.SetClipSet(m_ClipSetId); 

	if (m_iFlags.IsFlagSet(RELOAD_SECONDARY))
	{
		fwMvFilterId filterId = FILTER_ID_INVALID;

		if (m_iFlags.IsFlagSet(RELOAD_COVER))
		{
			filterId = ms_UpperBodyFeatheredCoverFilterId;
		}
		return ped.GetMovePed().SetSecondaryTaskNetwork(m_moveNetworkHelper, SLOW_BLEND_DURATION, CMovePed::Secondary_None, filterId);
	}
	else
	{
		return ped.GetMovePed().SetTaskNetwork(m_moveNetworkHelper, SLOW_BLEND_DURATION);
	}
}

////////////////////////////////////////////////////////////////////////////////

const CWeaponComponentReloadData* CTaskReloadGun::GetReloadData() 
{
	if (m_pCachedReloadData!=NULL)
		return m_pCachedReloadData;

	const CPed& ped = *GetPed();

	// Cache the weapon
	const CWeapon* pWeapon = ped.GetWeaponManager()->GetEquippedWeapon();


	if(pWeapon->GetClipComponent() && pWeapon->GetClipComponent()->GetReloadData() && pWeapon->GetClipComponent()->GetReloadData()->GetIsClass<CWeaponComponentReloadData>())
	{
		m_pCachedReloadData = static_cast<const CWeaponComponentReloadData*>(pWeapon->GetClipComponent()->GetReloadData());		
	}

	return m_pCachedReloadData;
}

////////////////////////////////////////////////////////////////////////////////

const CWeaponComponentReloadLoopedData* CTaskReloadGun::GetLoopedReloadData() const
{
	const CPed& ped = *GetPed();

	// Cache the weapon
	const CWeapon* pWeapon = ped.GetWeaponManager()->GetEquippedWeapon();

	if(pWeapon->GetClipComponent() && pWeapon->GetClipComponent()->GetReloadData() && pWeapon->GetClipComponent()->GetReloadData()->GetIsClass<CWeaponComponentReloadLoopedData>())
	{
		return static_cast<const CWeaponComponentReloadLoopedData*>(pWeapon->GetClipComponent()->GetReloadData());
	}

	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskReloadGun::StartReloadClips(CPed& ped, const CWeaponComponentReloadData* pReloadData, const fwMvClipId& clipId)
{
	// Get the weapon pointer (should be checked in ProcessPreFSM)
	CWeapon* pWeapon = ped.GetWeaponManager()->GetEquippedWeapon();
	const CWeaponInfo* pWeaponInfo = pWeapon->GetWeaponInfo();
	if (ped.IsNetworkClone() && pWeaponInfo)
	{
		m_ClipSetId = pWeaponInfo->GetAppropriateReloadWeaponClipSetId(&ped);
	}

	fwMvClipId pedClipId;
	fwMvClipId weaponClipId;
	if (pReloadData)
	{
		s32 iFlags = m_iFlags.IsFlagSet(RELOAD_IDLE) ? CWeaponComponentReloadData::RF_Idle : 0;
		iFlags |= pWeapon->GetIsClipEmpty() || m_bIsLoopedReloadFromEmpty ? CWeaponComponentReloadData::RF_OutOfAmmo : 0;
		if(pWeaponInfo)
		{
			iFlags |= pWeaponInfo->GetFlag(CWeaponInfoFlags::UseLoopedReloadAnim) ? CWeaponComponentReloadData::RF_LoopedReload : 0;
		}
		if (CWeaponInfo::ShouldPedUseCoverReloads(ped, *pWeaponInfo))
		{
			iFlags |= ped.GetPedResetFlag(CPED_RESET_FLAG_InCoverFacingLeft) ? CWeaponComponentReloadData::RF_FacingLeftInCover : CWeaponComponentReloadData::RF_FacingRightInCover;
		}
		pReloadData->GetReloadClipIds(iFlags, pedClipId, weaponClipId);
	}
	else
	{
		// Reload
		static const fwMvClipId s_PedAimReloadClipId("reload_aim",0x1D34D8F0);
		static const fwMvClipId s_WeaponAimReloadClipId("w_reload_aim",0x7B96C0C6);
		// Reload In
		static const fwMvClipId s_PedIdleReloadInClipId("reload_in",0x55E39470);
		static const fwMvClipId s_PedAimReloadInClipId("reload_aim_in",0x58F6701D);
		static const fwMvClipId s_WeaponIdleReloadInClipId("w_reload_in",0x3D9B0744);
		static const fwMvClipId s_WeaponAimReloadInClipId("w_reload_aim_in",0xC03CBCCC);
		// Reload Loop
		static const fwMvClipId s_PedIdleReloadLoopClipId("reload_loop",0xF27AB949);
		static const fwMvClipId s_PedAimReloadLoopClipId("reload_aim_loop",0xFEE33E7);
		static const fwMvClipId s_WeaponIdleReloadLoopClipId("w_reload_loop",0xCACF36D);
		static const fwMvClipId s_WeaponAimReloadLoopClipId("w_reload_aim_loop",0xD41B87F3);
		// Reload Out
		static const fwMvClipId s_PedIdleReloadOutClipId("reload_out",0x3651D561);
		static const fwMvClipId s_PedAimReloadOutClipId("reload_aim_out",0x69FB912B);
		static const fwMvClipId s_WeaponIdleReloadOutClipId("w_reload_out",0xBC6A1A07);
		static const fwMvClipId s_WeaponAimReloadOutClipId("w_reload_aim_out",0x6243C90D);

		// Fall back in case we have no reload data
		switch(GetState())
		{
		case State_ReloadIn:
			if(m_iFlags.IsFlagSet(RELOAD_IDLE))
			{
				pedClipId = s_PedIdleReloadInClipId;
				weaponClipId = s_WeaponIdleReloadInClipId;
			}
			else
			{
				pedClipId = s_PedAimReloadInClipId;
				weaponClipId = s_WeaponAimReloadInClipId;
			}
			break;
		case State_ReloadLoop:
			if(m_iFlags.IsFlagSet(RELOAD_IDLE))
			{
				pedClipId = s_PedIdleReloadLoopClipId;
				weaponClipId = s_WeaponIdleReloadLoopClipId;
			}
			else
			{
				pedClipId = s_PedAimReloadLoopClipId;
				weaponClipId = s_WeaponAimReloadLoopClipId;
			}
			break;
		case State_ReloadOut:
			if(m_iFlags.IsFlagSet(RELOAD_IDLE))
			{
				pedClipId = s_PedIdleReloadOutClipId;
				weaponClipId = s_WeaponIdleReloadOutClipId;
			}
			else
			{
				pedClipId = s_PedAimReloadOutClipId;
				weaponClipId = s_WeaponAimReloadOutClipId;
			}
			break;
		default:
			taskAssertf(0, "Unhandled state [%s]", GetStateName(GetState()));
			// Fall through
		case State_Reload:
			{
				pedClipId = s_PedAimReloadClipId;
				weaponClipId = s_WeaponAimReloadClipId;
			}
			break;
		}
	}

	// Lookup the clip
	const crClip* pClip = NULL;
	fwClipSet* pClipSet = fwClipSetManager::GetClipSet(m_ClipSetId);
	if(taskVerifyf(pClipSet, "ClipSet [%s] doesn't exist", m_ClipSetId.TryGetCStr()))
	{
		if(taskVerifyf(pClipSet->Request_DEPRECATED(), "ClipSet [%s] not streamed in", m_ClipSetId.TryGetCStr()))
		{
			pClip = pClipSet->GetClip(pedClipId);
		}
	}
	taskAssertf(pClip, "Clip [%s][%s] doesn't exist", m_ClipSetId.GetCStr(), pedClipId.GetCStr());

	// Start the reload clip on the ped
	m_moveNetworkHelper.SetClip(pClip, clipId);

	bool bPhaseSyncWeaponClip = true;
#if FPS_MODE_SUPPORTED
	if (ped.GetPedResetFlag(CPED_RESET_FLAG_InCoverTaskActive) && ped.IsFirstPersonShooterModeEnabledForPlayer(false))
		bPhaseSyncWeaponClip = false;
#endif // FPS_MODE_SUPPORTED	

	// Perform an clip on the weapon if one is specified
	const bool bShouldLoop = GetState() == State_ReloadLoop ? true : false;
	pWeapon->StartAnim(m_ClipSetId, weaponClipId, NORMAL_BLEND_DURATION, m_fClipReloadRate, 0.0f, bShouldLoop, bPhaseSyncWeaponClip);

	// Disable the weapon idle animation filter, re-enable it at the end of reloading anim.
	if (pWeapon->GetWeaponInfo() && pWeapon->GetWeaponInfo()->GetIsIdleAnimationFilterDisabledWhenReloading())
	{
		pWeapon->SetWeaponIdleFlag(false);
	}
}

////////////////////////////////////////////////////////////////////////////////

s32 CTaskReloadGun::ComputeNumberOfTimesToPlayReloadLoop(const CWeapon& weapon, const CWeaponInfo& weaponInfo) const 
{
	if(GetPed()->GetNetworkObject())
	{
		// clones should not be trying to generate this data, should be passed over....
		Assert(!GetPed()->GetNetworkObject()->IsClone());
	}

	if (weaponInfo.GetUseLoopedReloadAnim())
	{
		s32 iTotalBulletsToReload = weapon.GetClipSize() - weapon.GetAmmoInClip();

		const s32 iRemainingAmmo = weapon.GetAmmoTotal() - weapon.GetAmmoInClip();

		// If we don't have enough ammo to reload this amount, just reload what we have left
		if(iRemainingAmmo < iTotalBulletsToReload && weapon.GetAmmoTotal() != CWeapon::INFINITE_AMMO)
		{
			iTotalBulletsToReload = iRemainingAmmo;
		}

		// If we have any remainder we need to add one more loop on to account
		const bool bRemainder = iTotalBulletsToReload % m_iBulletsToReloadPerLoop == 0 ? false : true;
		
		s32 iReturnTotal = iTotalBulletsToReload / m_iBulletsToReloadPerLoop;
		if (bRemainder)
		{
			iReturnTotal += 1;
		}

		// Reduce the number of loops by one since we load a round on the loop start. (We always need at least one reload here to stop asserts)
		TUNE_GROUP_INT(PED_RELOAD, iReduceLoopReloadCount, 1, 0, 100, 1);
		iReturnTotal = rage::Clamp(iReturnTotal - iReduceLoopReloadCount, 1, 100);

		// Need to play reload clip for each bullet batch that needs to be replaced
		return iReturnTotal;
	}
	else
	{
		return 1;
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskReloadGun::UpdateBulletReloadStatus()
{
	// We have just started playing the reload loop, so decrement the number of times we need to play it
	--m_iNumTimesToPlayReloadLoop;

	// If we need to play the clip again after this, set the loop clip flag to make sure the clip loops when its at the end
	m_bLoopReloadClip = m_iNumTimesToPlayReloadLoop > 0 ? true : false;
}

////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
void CTaskReloadGun::Debug() const
{
#if __BANK
	grcDebugDraw::AddDebugOutput(Color_green, "Num Times To Loop Clip : %i", m_iNumTimesToPlayReloadLoop);
#endif // __BANK
}

#if DEBUG_DRAW
void CTaskReloadGun::DebugCloneTaskInformation(void)
{
	if(GetPed())
	{
		char buffer[2056] = "\0";

		sprintf(buffer, "ClipSetId %s\nIsIdleReload %d\nLoopReloadClip %d\nEnteredLoopState %d\nCreateProjectileOrdanance %d\nNumTimesToPlayReload %d",
		m_ClipSetId.GetCStr(),
		m_iFlags.IsFlagSet(RELOAD_IDLE),
		m_bLoopReloadClip,
		m_bEnteredLoopState,
		m_bCreatedProjectileOrdnance,
		m_iNumTimesToPlayReloadLoop
		);

		grcDebugDraw::Text(VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition()), Color_white, buffer, false);
	}
}
#endif /* DEBUG_DRAW */

////////////////////////////////////////////////////////////////////////////////

const char * CTaskReloadGun::GetStaticStateName( s32 iState )
{
	switch (iState)
	{
		case State_Start:			return "State_Start";
		case State_Reload:			return "State_Reload";
		case State_ReloadIn:		return "State_ReloadIn";
		case State_ReloadLoop:		return "State_ReloadLoop";
		case State_ReloadOut:		return "State_ReloadOut";
		case State_Finish:			return "State_Finish";
	}

	return "State_Invalid";
}
#endif // !__FINAL

////////////////////////////////////////////////////////////////////////////////

void CTaskReloadGun::ProcessClipEvents()
{
	if( !m_bCreatedProjectileOrdnance )
	{
		if( m_moveNetworkHelper.GetBoolean( ms_CreateOrdnance ) )
		{
			CreateOrdnance();
		}
	}

	if( !m_bCreatedAmmoClipInLeftHand )
	{
		if( m_moveNetworkHelper.GetBoolean( ms_CreateAmmoClipInLeftHand ) )
		{
			CreateAmmoClipInLeftHand();
		}
	}

	if( !m_bCreatedAmmoClip )
	{
		if( m_moveNetworkHelper.GetBoolean( ms_CreateAmmoClip ) )
		{
			CreateAmmoClip();

			// Reload the ammo at the point we hit the anim event
			CWeapon* pWeapon = GetPed()->GetWeaponManager()->GetEquippedWeapon();
			if(pWeapon)
			{
				pWeapon->DoReload();
			}
		}
	}

	if( !m_bDestroyedAmmoClip )
	{
		if( m_moveNetworkHelper.GetBoolean( ms_DestroyAmmoClip ) )
		{
			DestroyAmmoClip();
		}
	}

	if( !m_bDestroyedAmmoClipInLeftHand )
	{
		if( m_moveNetworkHelper.GetBoolean( ms_DestroyAmmoClipInLeftHand ) )
		{
			DestroyAmmoClipInLeftHand();
		}
	}
	
	if( !m_bReleasedAmmoClip )
	{
		if( m_moveNetworkHelper.GetBoolean( ms_ReleaseAmmoClip ) )
		{
			ReleaseAmmoClip();
		}
	}

	if( !m_bReleasedAmmoClipInLeftHand )
	{
		if( m_moveNetworkHelper.GetBoolean( ms_ReleaseAmmoClipInLeftHand ) )
		{
			ReleaseAmmoClipInLeftHand();
		}
	}

#if FPS_MODE_SUPPORTED
	if(GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		const CClipEventTags::CCameraShakeTag* pCamShake = static_cast<const CClipEventTags::CCameraShakeTag*>(m_moveNetworkHelper.GetNetworkPlayer()->GetProperty(CClipEventTags::CameraShake));
		if (pCamShake)
		{
			if(pCamShake->GetShakeRefHash() != 0)
			{
				camFirstPersonShooterCamera* pCamera = camInterface::GetGameplayDirector().GetFirstPersonShooterCamera();
				
				if (pCamera)
				{
					pCamera->Shake( pCamShake->GetShakeRefHash() );
				}
			}
		}
	}
#endif

	if( m_moveNetworkHelper.GetBoolean( ms_AttachGunToLeftHand ) )
	{
		SwitchGunHands( (u8)CPedEquippedWeapon::AP_LeftHand);
	}

	if( m_moveNetworkHelper.GetBoolean( ms_AttachGunToRightHand ) )
	{
		SwitchGunHands( (u8)CPedEquippedWeapon::AP_RightHand);
	}
}

void CTaskReloadGun::SwitchGunHands( u8 nAttachPoint )
{
	if(GetPed()->GetWeaponManager() && GetPed()->GetWeaponManager()->GetPedEquippedWeapon() && GetPed()->GetWeaponManager()->GetPedEquippedWeapon()->GetObject())
	{
		CPedEquippedWeapon::eAttachPoint eCurrentAttachPoint = 	GetPed()->GetWeaponManager()->GetPedEquippedWeapon()->GetAttachPoint();
		if( eCurrentAttachPoint != (CPedEquippedWeapon::eAttachPoint)nAttachPoint )
		{
			GetPed()->GetWeaponManager()->GetPedEquippedWeapon()->AttachObject((CPedEquippedWeapon::eAttachPoint)nAttachPoint);
		}
	}
}

void CTaskReloadGun::CreateAmmoClip()
{
	//Grab the ped.
	CPed* pPed = GetPed();
	taskAssert(pPed);

	//Grab the weapon manager.
	CPedWeaponManager* pWeaponManager = pPed->GetWeaponManager();
	taskAssert(pWeaponManager);

	//Grab the equipped weapon.
	CWeapon* pWeapon = pWeaponManager->GetEquippedWeapon();
	if(!pWeapon)
	{
		return;
	}

	//Grab the ped equipped weapon.
	CPedEquippedWeapon* pPedEquippedWeapon = pWeaponManager->GetPedEquippedWeapon();
	taskAssert(pPedEquippedWeapon);

	const CWeaponItem* pWeaponItem = pPed->GetInventory() ? pPed->GetInventory()->GetWeapon( pWeapon->GetWeaponHash() ) : NULL;			
	if( pWeaponItem )
	{
		if( m_bHoldingOrdnance )
		{
			if( !m_bCreatedProjectileOrdnance )
			{	
				// If no clip, try creating an ordnance
				pPedEquippedWeapon->CreateProjectileOrdnance();
				m_bCreatedProjectileOrdnance = true;
			}
		}
		else
		{
			// There shouldn't be a clip but if there is then delete it
			CWeaponComponentClip* pComponentClip = pWeapon->GetClipComponent();
			if( pComponentClip )
			{
				CPedEquippedWeapon::DestroyWeaponComponent( pPedEquippedWeapon->GetObject(), pComponentClip );
			}
			
			const CWeaponComponentClipInfo* pComponentInfo = pWeaponItem->GetComponentInfo<CWeaponComponentClipInfo>();
			if( pComponentInfo )
			{
				const bool bDoReload = false;
				CPedEquippedWeapon::CreateWeaponComponent( pComponentInfo->GetHash(), pPedEquippedWeapon->GetObject(), bDoReload);
			}
		}
	}

	// Clear any object in left hand
	DeleteHeldObject();

	m_bCreatedAmmoClip = true;
}

void CTaskReloadGun::CreateAmmoClipInLeftHand()
{
	if(CreateHeldClipObject() || CreateHeldOrdnanceObject())
	{
		m_bCreatedAmmoClipInLeftHand = true;
	}
}

void CTaskReloadGun::CreateOrdnance()
{
	//Grab the equipped weapon.
	CPedEquippedWeapon* pPedEquippedWeapon = GetPed()->GetWeaponManager()->GetPedEquippedWeapon();

	pPedEquippedWeapon->CreateProjectileOrdnance();
	m_bCreatedProjectileOrdnance = true;
}

void CTaskReloadGun::DestroyAmmoClip()
{
	//Grab the ped.
	CPed* pPed = GetPed();
	taskAssert(pPed);

	//Grab the weapon manager.
	CPedWeaponManager* pWeaponManager = pPed->GetWeaponManager();
	taskAssert(pWeaponManager);

	//Grab the equipped weapon.
	CWeapon* pWeapon = pWeaponManager->GetEquippedWeapon();
	if(!pWeapon)
	{
		return;
	}

	//Grab the ped equipped weapon.
	CPedEquippedWeapon* pPedEquippedWeapon = pWeaponManager->GetPedEquippedWeapon();
	taskAssert(pPedEquippedWeapon);

	CWeaponComponentClip* pComponentClip = pWeapon->GetClipComponent();
	if( pComponentClip )
	{
		CPedEquippedWeapon::DestroyWeaponComponent( pPedEquippedWeapon->GetObject(), pComponentClip );
	}

	m_bDestroyedAmmoClip = true;
}

void CTaskReloadGun::DestroyAmmoClipInLeftHand()
{
	DeleteHeldObject();
	m_bDestroyedAmmoClipInLeftHand = true;
}

void CTaskReloadGun::ReleaseAmmoClip()
{
	//Grab the ped.
	CPed* pPed = GetPed();
	taskAssert(pPed);

	//Grab the weapon manager.
	CPedWeaponManager* pWeaponManager = pPed->GetWeaponManager();
	taskAssert(pWeaponManager);

	//Grab the equipped weapon.
	CWeapon* pWeapon = pWeaponManager->GetEquippedWeapon();
	if(!pWeapon)
	{
		return;
	}

	//Grab the ped equipped weapon.
	CPedEquippedWeapon* pPedEquippedWeapon = pWeaponManager->GetPedEquippedWeapon();
	taskAssert(pPedEquippedWeapon);

	CWeaponComponentClip* pComponentClip = pWeapon->GetClipComponent();
	if( pComponentClip )
	{
		bool bDestroy = false;

		// Don't have AI drop the ammo clip when he's off screen or too far away.
		if (!pPed->IsLocalPlayer())
		{
			if(!pPed->GetIsVisibleInSomeViewportThisFrame())
			{
				bDestroy = true;
			}
			else
			{
				TUNE_GROUP_FLOAT(DO_NOT_DROP_CLIP_DISTANCE, fDoNotDropClipDistance, 30.0f, 0.0f, 100.0f, 1.0f);
				float lodScale = g_LodScale.GetGlobalScale(); // Takes into account things like sniper scopes.
				Vector3 vecToCam = camInterface::GetPos() - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
				if(vecToCam.Mag2() > fDoNotDropClipDistance*fDoNotDropClipDistance*lodScale*lodScale)
				{
					bDestroy = true;
				}
			}
		}

		// Make sure the object is not intersecting a wall, do a probe from the player to the object
		if( !bDestroy && pComponentClip->GetDrawable() )
		{
			Vector3 vTestStart( VEC3V_TO_VECTOR3( pPed->GetTransform().GetPosition() ) );
			Vector3 vTestEnd( VEC3V_TO_VECTOR3( pComponentClip->GetDrawable()->GetTransform().GetPosition() ) );

			//Extend the vTestEnd by the ammo clip bounding box radius to avoid intersecting.
			Vector3 vDir = (vTestEnd - vTestStart);
			vDir.ClampMag(0.0f, pComponentClip->GetDrawable()->GetBoundRadius());
			vTestEnd += vDir;

			WorldProbe::CShapeTestFixedResults<1> probeResult;
			WorldProbe::CShapeTestProbeDesc probeDesc;
			probeDesc.SetStartAndEnd( vTestStart, vTestEnd );
			probeDesc.SetResultsStructure( &probeResult );
			probeDesc.SetExcludeEntity( pPed );
			probeDesc.SetIncludeFlags( ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_OBJECT_TYPE );
			probeDesc.SetContext( WorldProbe::LOS_GeneralAI );
			if( WorldProbe::GetShapeTestManager()->SubmitTest( probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST ) )
			{
				bDestroy = true;
			}
		}

		if(bDestroy)
		{
			CPedEquippedWeapon::DestroyWeaponComponent( pPedEquippedWeapon->GetObject(), pComponentClip );
		}
		else
		{
			CPedEquippedWeapon::ReleaseWeaponComponent( pPedEquippedWeapon->GetObject(), pComponentClip );
		}
	}

	m_bReleasedAmmoClip = true;
}

void CTaskReloadGun::ReleaseAmmoClipInLeftHand()
{
	ReleaseHeldObject();
	m_bReleasedAmmoClipInLeftHand = true;
}

bool CTaskReloadGun::ShouldCreateAmmoClipOnCleanUp() const
{
	//Grab the ped.
	const CPed* pPed = GetPed();

	//Ensure the ped is not dead.
	if(pPed->IsInjured())
	{
		return false;
	}

	if(!pPed->GetWeaponManager() || !pPed->GetWeaponManager()->GetEquippedWeapon())
	{
		return false;
	}

	//Ensure the ammo clip was destroyed or released.
	if(!m_bDestroyedAmmoClip && !m_bReleasedAmmoClip)
	{
		return false;
	}

	//Ensure the ammo clip was not created.
	if(m_bCreatedAmmoClip)
	{
		return false;
	}

	//At this point, we know we are quitting a reload in the middle of a cycle.
	//Our ped is not dead.  We have destroyed or released our previous clip, but we have
	//not attached a new one.  If we quit the task now, before creating a new weapon component
	//for the clip, the clip will be missing, and the ped will think they need to swap
	//weapons since the components / attachments don't match up.  For this reason,
	//we pop in the clip at this point regardless of the ped's current state.

	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskReloadGun::ComputeShouldInterrupt() const
{
	const CPed* pPed = GetPed();

	// Early out if the remote player isn't reloading anymore
	if(pPed->IsNetworkClone() && pPed->IsPlayer() && pPed->GetIsInCover())
	{
		const CClonedUseCoverInfo* pInCoverInfo = static_cast<const CClonedUseCoverInfo*>(pPed->GetPedIntelligence()->GetQueriableInterface()->FindTaskInfoIfCurrentlyRunning(CTaskTypes::TASK_IN_COVER));
		if (pInCoverInfo)
		{
			return !pInCoverInfo->GetNeedsToReload();
		}
	}

	if(m_moveNetworkHelper.GetBoolean(ms_Interruptible))
	{
		if(!pPed->GetIsInCover() && pPed->GetMotionData()->GetDesiredMbrY() > 0.f)
		{
			// If we're moving quit out at the event.
			return true;
		}

		WeaponControllerState controllerState = CWeaponControllerManager::GetController(m_weaponControllerType)->GetState(pPed, false);
		if(controllerState == WCS_Aim || controllerState == WCS_Fire || controllerState == WCS_FireHeld || controllerState == WCS_CombatRoll)
		{
			// Early out if we're trying to aim/fire
			return true;
		}
	}
	else if(m_moveNetworkHelper.GetBoolean(ms_BlendOut))
	{
		// Early out here
		return true;
	}

	if(pPed->IsLocalPlayer())
	{
		if( m_moveNetworkHelper.GetBoolean( ms_AllowSwapWeapon ) )
		{			
#if !__NO_OUTPUT
			if (!m_bCreatedAmmoClip)
			{
				const CWeaponInfo* pWeaponInfo = NULL;
				const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
				if(pWeapon)
				{
					pWeaponInfo = pWeapon->GetWeaponInfo();
				}
				aiDebugf2("Ammo clip hasn't been created, shouldn't allow weapon swap during reload weapon[%s] anim[%s]", (pWeaponInfo ? pWeaponInfo->GetName() : "NULL"), m_ClipSetId.TryGetCStr());
			}			
#endif //__NO_OUTPUT
			const CPedWeaponManager *pWeapMgr = pPed->GetWeaponManager();
			if(m_bCreatedAmmoClip && pWeapMgr->GetRequiresWeaponSwitch())
			{
				return true;
			}
		}
	}

	if (pPed->GetUsingRagdoll())
	{
		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskReloadGun::ShouldBlendOut()
{
	const CClipEventTags::CBlendOutEventTag* pProp = static_cast<const CClipEventTags::CBlendOutEventTag*>(m_moveNetworkHelper.GetNetworkPlayer()->GetProperty(CClipEventTags::BlendOutWithDuration));
	if (pProp)
	{
		m_fBlendOutDuration = rage::Clamp(pProp->GetBlendOutDuration(), 0.0f, 10.0f);
		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskReloadGun::CreateHeldClipObject()
{
	CPed* pPed = GetPed();
	taskAssert(pPed);
	if (!pPed)
		return false;

	taskAssert(pPed->GetWeaponManager());
	if (!pPed->GetWeaponManager())
		return false;

	CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
	taskAssert(pWeapon);
	if (!pWeapon)
		return false;

	const CWeaponItem* pWeaponItem = pPed->GetInventory() ? pPed->GetInventory()->GetWeapon(pWeapon->GetWeaponHash()) : NULL;			
	if(pWeaponItem)
	{
		const CWeaponComponentClipInfo* pComponentInfo = pWeaponItem->GetComponentInfo<CWeaponComponentClipInfo>();
		if(pComponentInfo && pComponentInfo->GetModelHash() != 0)
		{
			u32 componentModelHash = pComponentInfo->GetModelHash();

			const CWeaponComponentVariantModel* pVariantComponent = pWeapon->GetVariantModelComponent();
			if (pVariantComponent)
			{
				const CWeaponComponentVariantModelInfo* pVariantInfo = pVariantComponent->GetInfo();
				u32 variantModelHash = pVariantInfo->GetExtraComponentModel(pComponentInfo->GetHash());
				if (variantModelHash != 0)
				{
					componentModelHash = variantModelHash;
				}
			}

			CreateHeldObject(componentModelHash);
			if(weaponVerifyf(m_pHeldObject, "Failed to create weapon clip object with name [%s]", pComponentInfo->GetModelName()))
			{
				const CWeaponInfo* pWeaponInfo = pWeapon->GetWeaponInfo();

				// Attach the child to the parent, using the inverse of the offset bone as the attachment offset
				s32 iPedBoneIndex = pPed->GetBoneIndexFromBoneTag( (pWeaponInfo && pWeaponInfo->GetAttachReloadObjectToRightHand()) ? BONETAG_R_IK_HAND : BONETAG_L_PH_HAND);
				s32 iOffsetBone = pComponentInfo->GetAttachBone().GetBoneIndex(m_pHeldObject->GetModelIndex());

				Matrix34 mOffsetBone(Matrix34::IdentityType);
				if(iOffsetBone != -1)
				{
					if(!m_pHeldObject->GetSkeleton() && m_pHeldObject->GetDrawable())
					{
						// Ensure the skeleton is created for animation/querying
						if(m_pHeldObject->GetDrawable()->GetSkeletonData())
						{
							m_pHeldObject->CreateSkeleton();
						}
					}
					if(m_pHeldObject->GetSkeleton())
					{
						m_pHeldObject->GetGlobalMtx(iOffsetBone, mOffsetBone);
					}
					else
					{
						// In case the child has no skeleton, make sure the position offset is correct when we transform this
						// matrix into model space.
						mOffsetBone.d = VEC3V_TO_VECTOR3(m_pHeldObject->GetMatrix().d());
					}
					Matrix34 m = MAT34V_TO_MATRIX34(m_pHeldObject->GetMatrix());
					mOffsetBone.DotTranspose(m);
					mOffsetBone.FastInverse();
				}

				Quaternion q;
				mOffsetBone.ToQuaternion(q);
				q.Normalize();

				m_pHeldObject->AttachToPhysicalBasic(pPed, (s16)iPedBoneIndex, ATTACH_STATE_BASIC|ATTACH_FLAG_DELETE_WITH_PARENT, &mOffsetBone.d, &q);

				// Flag
				m_bHoldingClip = true;
			}
		}
	}

	return m_bHoldingClip;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskReloadGun::CreateHeldOrdnanceObject()
{
	CPed* pPed = GetPed();
	taskAssert(pPed);
	if (!pPed)
		return false;

	taskAssert(pPed->GetWeaponManager());
	if (!pPed->GetWeaponManager())
		return false;

	CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
	taskAssert(pWeapon);
	if (!pWeapon)
		return false;

	const CWeaponInfo* pWeaponInfo = pWeapon->GetWeaponInfo();
	taskAssert(pWeaponInfo);
	if (!pWeaponInfo)
		return false;

	if(pWeaponInfo->GetCreateVisibleOrdnance())
	{
		const CAmmoInfo* pAmmoInfo = pWeaponInfo->GetAmmoInfo();
		if(pAmmoInfo && pAmmoInfo->GetModelHash() != 0)
		{
			CreateHeldObject(pAmmoInfo->GetModelHash());
			if(weaponVerifyf(m_pHeldObject, "Failed to create weapon ordnance object with name [%s]", pAmmoInfo->GetModelName()))
			{
				// Attach the child to the parent, using the inverse of the offset bone as the attachment offset

				s32 iPedBoneIndex = pPed->GetBoneIndexFromBoneTag(pWeaponInfo->GetAttachReloadObjectToRightHand() ? BONETAG_R_IK_HAND : BONETAG_L_PH_HAND);
				s32 iProjectileBoneIndex = -1;
				const CBaseModelInfo* pProjectileModelInfo = m_pHeldObject->GetBaseModelInfo();
				if(pProjectileModelInfo && pProjectileModelInfo->GetModelType() == MI_TYPE_WEAPON)
				{
					iProjectileBoneIndex = static_cast<const CWeaponModelInfo*>(pProjectileModelInfo)->GetBoneIndex(WEAPON_GRIP_R);
				}

				Matrix34 mOffsetBone(Matrix34::IdentityType);
				if(iProjectileBoneIndex != -1)
				{
					if(!m_pHeldObject->GetSkeleton() && m_pHeldObject->GetDrawable())
					{
						// Ensure the skeleton is created for animation/querying
						if(m_pHeldObject->GetDrawable()->GetSkeletonData())
						{
							m_pHeldObject->CreateSkeleton();
						}
					}
					if(m_pHeldObject->GetSkeleton())
					{
						m_pHeldObject->GetGlobalMtx(iProjectileBoneIndex, mOffsetBone);
					}
					else
					{
						// In case the child has no skeleton, make sure the position offset is correct when we transform this
						// matrix into model space.
						mOffsetBone.d = VEC3V_TO_VECTOR3(m_pHeldObject->GetMatrix().d());
					}
					Matrix34 m = MAT34V_TO_MATRIX34(m_pHeldObject->GetMatrix());
					mOffsetBone.DotTranspose(m);
					mOffsetBone.FastInverse();
				}

				Quaternion q;
				mOffsetBone.ToQuaternion(q);
				q.Normalize();

				m_pHeldObject->AttachToPhysicalBasic(pPed, (s16)iPedBoneIndex, ATTACH_STATE_BASIC|ATTACH_FLAG_DELETE_WITH_PARENT, &mOffsetBone.d, &q);

				// Flag
				m_bHoldingOrdnance = true;
			}
		}
	}

	return m_bHoldingOrdnance;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskReloadGun::CreateHeldObject(u32 uModelHash)
{
	CPed* pPed = GetPed();

	if(uModelHash != 0)
	{
		taskAssertf(m_pHeldObject == NULL, "Clip object should be NULL");

		fwModelId clipModelId;
		CModelInfo::GetBaseModelInfoFromHashKey(uModelHash, &clipModelId);
		m_pHeldObject = CObjectPopulation::CreateObject(clipModelId, ENTITY_OWNEDBY_GAME, true, true, false);
		if(weaponVerifyf(m_pHeldObject, "Failed to create weapon clip object with name [%s]", atHashWithStringNotFinal(uModelHash).TryGetCStr()))
		{
			// When adding an object to the world, the octree requires a valid position!
			// Temporarily use the ped position until we the attachment code updates it
			m_pHeldObject->SetMatrix(MAT34V_TO_MATRIX34(pPed->GetTransform().GetMatrix()));

			// Add to the world
			CGameWorld::Add(m_pHeldObject, pPed->GetInteriorLocation().IsValid() ? pPed->GetInteriorLocation() : CGameWorld::OUTSIDE);

			REPLAY_ONLY(CReplayMgr::RecordObject(m_pHeldObject));
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskReloadGun::DeleteHeldObject()
{
	if(m_pHeldObject)
	{
		// Mark this drawable for delete on next process
		m_pHeldObject->RemovePhysics();
		m_pHeldObject->SetOwnedBy(ENTITY_OWNEDBY_TEMP);
		m_pHeldObject->m_nEndOfLifeTimer = 1;
		m_pHeldObject->DisableCollision();
		m_pHeldObject->SetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY, false, true);
		m_pHeldObject = NULL;
	}

	m_bHoldingClip = false;
	m_bHoldingOrdnance = false;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskReloadGun::ReleaseHeldObject()
{
	if(m_pHeldObject)
	{
		// Release the component to physics
		m_pHeldObject->DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS|DETACH_FLAG_APPLY_VELOCITY);

		// make sure game objects get deleted
		// don't delete frag instances or non game objects
		if (m_pHeldObject->GetIsAnyManagedProcFlagSet())
		{
			// don't do anything with procedural objects - the procobj code needs to be in charge of them
		}
		else if(m_pHeldObject->GetOwnedBy() == ENTITY_OWNEDBY_GAME && (!m_pHeldObject->GetCurrentPhysicsInst() || m_pHeldObject->GetCurrentPhysicsInst()->GetClassType() != PH_INST_FRAG_CACHE_OBJECT))
		{
			Assertf(m_pHeldObject->GetRelatedDummy() == NULL, "Planning to remove a weapon component object but it has a dummy");

			m_pHeldObject->SetOwnedBy( ENTITY_OWNEDBY_TEMP );

			// Temp objects stick around for 30000 milliseconds. No need to increase that
			m_pHeldObject->m_nEndOfLifeTimer = fwTimer::GetTimeInMilliseconds();
		}

		m_pHeldObject = NULL;
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskReloadGun::IsWeaponBlocked()
{
	CPed* pPed = GetPed();

	// Ignore blocked checks if the weapon has a first person scope
	taskAssert(pPed->GetWeaponManager());
	CWeapon* pEquippedWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
	if(!pEquippedWeapon)
	{
		return false;
	}

	if(pPed->GetIsInCover())
	{
		const CTaskCover* pCoverTask = static_cast<const CTaskCover*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COVER));
		if (pCoverTask && !pCoverTask->IsCoverFlagSet(CTaskCover::CF_TooHighCoverPoint))
		{
			return false;
		}
	}
	// disabled the blocked state when coming out of cover as the blocked probes won't be valid if the ped is rotating
	const CTaskPlayerOnFoot* pPlayerOnFootTask	= static_cast<const CTaskPlayerOnFoot*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PLAYER_ON_FOOT));
	if(pPlayerOnFootTask)
	{
		static dev_float FROM_COVER_BLOCK_TIME = 500;
		if(	(pPlayerOnFootTask->GetLastTimeInCoverTask() + FROM_COVER_BLOCK_TIME) > fwTimer::GetTimeInMilliseconds())
		{
			return false;
		}
	}

	//Make sure we stay in the standard anim for a min of 300 millseconds
	if( fwTimer::GetTimeInMilliseconds() - GetTimeInState() < 300)
	{
		return false;
	}

	// Do a probe from weapon grip_r to muzzle
	int muzzleBoneIdx = pEquippedWeapon->GetWeaponModelInfo()->GetBoneIndex(WEAPON_MUZZLE);
	int gripBoneIdx = pEquippedWeapon->GetWeaponModelInfo()->GetBoneIndex(WEAPON_GRIP_R);

	if ((muzzleBoneIdx != -1) && (gripBoneIdx != -1))
	{
		Matrix34 mtxMuzzle, mtxGrip;
		pEquippedWeapon->GetEntity()->GetGlobalMtx(muzzleBoneIdx, mtxMuzzle);
		pEquippedWeapon->GetEntity()->GetGlobalMtx(gripBoneIdx, mtxGrip);

		Vector3 vDifference(mtxMuzzle.d - mtxGrip.d);
		float fWeaponLength = vDifference.XYMag();

		// Ignore shoot through. Fixed the glitch when against glass or chain link fence. B* 2060127.
		if (CTaskWeaponBlocked::IsWeaponBlocked(pPed, mtxMuzzle.d, &(mtxGrip.d), 0.0f, 1.0f, false, true, true, fWeaponLength))
		{
			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskReloadGun::ReloadOnCleanUp(CPed& ped)
{
	if (ped.GetWeaponManager())
	{
		CWeapon* pWeapon = ped.GetWeaponManager()->GetEquippedWeapon();
		if (pWeapon && !m_bInterruptRequested)
		{
			// Do this here before calling DoReload, as without an ammo clip, we will not reload the right amount of ammo
			if(ShouldCreateAmmoClipOnCleanUp())
			{
				//Create the ammo clip.
				CreateAmmoClip();
			}

			pWeapon->DoReload();
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskReloadGun::SendPitchParam(CPed& ped)
{
	// Send the pitch param
	const CTaskMotionBase* pMotionTask = ped.GetCurrentMotionTask(false);
	if (pMotionTask && pMotionTask->GetTaskType() == CTaskTypes::TASK_MOTION_AIMING)
	{
		m_fPitch = static_cast<const CTaskMotionAiming*>(pMotionTask)->GetCurrentPitch();
	}

	m_moveNetworkHelper.SetFloat(ms_PitchId, m_fPitch);
}