// Class header
#include "Task/Weapons/Gun/TaskVehicleDriveBy.h"

#include "fwanimation/clipsets.h"

// Game headers
#include "animation/MovePed.h"
#include "camera/base/BaseCamera.h"
#include "Camera/CamInterface.h"
#include "Camera/Gameplay/GameplayDirector.h"
#include "Camera/Gameplay/ThirdPersonCamera.h"
#include "Debug/DebugScene.h"
#include "frontend/NewHud.h"
#include "ik/IkRequest.h"
#include "Peds/PedIntelligence.h"
#include "Peds/Ped.h"
#include "Peds/PedIKSettings.h"
#include "Peds/PedWeapons/PedTargetEvaluator.h"
#include "Peds/Horse/horseComponent.h"
#include "modelinfo/PedModelInfo.h"
#include "ModelInfo/WeaponModelInfo.h"
#include "Scene/Playerswitch/PlayerSwitchInterface.h"
#include "Scene/World/GameWorld.h"
#include "Task/Combat/Cover/TaskCover.h"
#include "Task/Combat/TaskNewCombat.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/Weapons/Gun/TaskGun.h"
#include "Task/Weapons/Gun/TaskReloadGun.h"
#include "Task/Weapons/TaskWeaponBlocked.h"
#include "task/Vehicle/TaskCar.h"
#include "Task/Vehicle/TaskInVehicle.h"
#include "Task/Vehicle/TaskVehicleWeapon.h"
#include "Weapons/Weapon.h"
#include "Vehicles/Metadata/VehicleSeatInfo.h"
#include "Vehicles/Metadata/VehicleMetadataManager.h"
#include "Vehicles/Metadata/VehicleLayoutInfo.h"
#include "Vehicles/VehicleGadgets.h"
#include "vfx/vehicleglass/VehicleGlassManager.h"
#include "camera/cinematic/camera/mounted/CinematicMountedCamera.h"
#include "Task/Vehicle/TaskCarAccessories.h"
#include "Task/Movement/TaskParachute.h"
#include "weapons/AirDefence.h"

// Macro to disable optimizations if set
AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()
AI_WEAPON_OPTIMISATIONS()
NETWORK_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// CTaskAimGunVehicleDriveBy
//////////////////////////////////////////////////////////////////////////

CTaskAimGunVehicleDriveBy::Tunables CTaskAimGunVehicleDriveBy::sm_Tunables;
IMPLEMENT_WEAPON_TASK_TUNABLES(CTaskAimGunVehicleDriveBy, 0x4d06682f);

#if __BANK
bool CTaskAimGunVehicleDriveBy::ms_bRenderVehicleGunDebug = false;
bool CTaskAimGunVehicleDriveBy::ms_bRenderVehicleDriveByDebug = false;
bool CTaskAimGunVehicleDriveBy::ms_bDisableOutro = false;
bool CTaskAimGunVehicleDriveBy::ms_bDisableBlocking = false;
bool CTaskAimGunVehicleDriveBy::ms_bDisableWindowSmash = false;
#endif // __BANK

float CTaskAimGunVehicleDriveBy::ms_DefaultMinYawChangeRate = 0.2f;
float CTaskAimGunVehicleDriveBy::ms_DefaultMaxYawChangeRate = 1.0f;

float CTaskAimGunVehicleDriveBy::ms_DefaultMinYawChangeRateOverriddenContext = 0.75f;
float CTaskAimGunVehicleDriveBy::ms_DefaultMaxYawChangeRateOverriddenContext = 1.5f;

float CTaskAimGunVehicleDriveBy::ms_DefaultMinYawChangeRateFirstPerson = 4.0f;
float CTaskAimGunVehicleDriveBy::ms_DefaultMaxYawChangeRateFirstPerson = 5.0f;

float CTaskAimGunVehicleDriveBy::ms_DefaultMaxPitchChangeRate = 2.0f;
float CTaskAimGunVehicleDriveBy::ms_DefaultMaxPitchChangeRateFirstPerson = 2.0f;
float CTaskAimGunVehicleDriveBy::ms_DefaultIntroRate = 1.75f;
float CTaskAimGunVehicleDriveBy::ms_DefaultIntroRateFirstPerson = 1.75f;
float CTaskAimGunVehicleDriveBy::ms_DefaultOutroRate = 1.5f;
float CTaskAimGunVehicleDriveBy::ms_DefaultOutroRateFirstPerson = 1.5f;

float CTaskAimGunVehicleDriveBy::ms_DefaultRemoteMinYawChangeRateScalar = 1.2f;
float CTaskAimGunVehicleDriveBy::ms_DefaultRemoteMaxYawChangeRateScalar = 1.2f;
float CTaskAimGunVehicleDriveBy::ms_DefaultRemoteMaxPitchChangeRate = CTaskAimGunVehicleDriveBy::ms_DefaultMaxPitchChangeRate;
float CTaskAimGunVehicleDriveBy::ms_DefaultRemoteIntroRate = CTaskAimGunVehicleDriveBy::ms_DefaultIntroRate * 2.0f;
float CTaskAimGunVehicleDriveBy::ms_DefaultRemoteOutroRate = CTaskAimGunVehicleDriveBy::ms_DefaultOutroRate * 2.0f;

bank_bool CTaskAimGunVehicleDriveBy::ms_UseAssistedAiming = true;
bank_bool CTaskAimGunVehicleDriveBy::ms_OnlyAllowLockOnToPedThreats = true; 

const fwMvClipSetVarId CTaskAimGunVehicleDriveBy::ms_FireClipSetVarId("FireClipSet",0xC62DDEC0);
const fwMvBooleanId CTaskAimGunVehicleDriveBy::ms_OutroClipFinishedId("OutroAnimFinished",0x834634);
const fwMvBooleanId CTaskAimGunVehicleDriveBy::ms_StateAimId("State_Aim",0xEF6939A8);
const fwMvBooleanId CTaskAimGunVehicleDriveBy::ms_FireLoopOnEnter("FireLoopOnEnter",0x25E3CBC0);
const fwMvBooleanId CTaskAimGunVehicleDriveBy::ms_FireLoop1("FireLoop1Finished",0x4C274860);
const fwMvBooleanId CTaskAimGunVehicleDriveBy::ms_FireLoop2("FireLoop2Finished",0x572B2974);
const fwMvBooleanId CTaskAimGunVehicleDriveBy::ms_DrivebyOutroBlendOutId("DrivebyOutroBlendOut",0x2e5774da);

const fwMvRequestId CTaskAimGunVehicleDriveBy::ms_Fire("Fire",0xD30A036B);
const fwMvRequestId CTaskAimGunVehicleDriveBy::ms_FireAuto("FireAuto",0xD86EA5D5);
const fwMvRequestId CTaskAimGunVehicleDriveBy::ms_FireWithRecoil("FireWithRecoil",0xf199880e);
const fwMvRequestId CTaskAimGunVehicleDriveBy::ms_FireFinished("FireFinished",0xD30314E9);
const fwMvRequestId CTaskAimGunVehicleDriveBy::ms_FlipRight("FlipRight",0x413A3415);
const fwMvRequestId CTaskAimGunVehicleDriveBy::ms_FlipLeft("FlipLeft",0x72DDCAD);
const fwMvRequestId CTaskAimGunVehicleDriveBy::ms_FlipEnd("FlipEnd",0x8EAC7734);
const fwMvRequestId CTaskAimGunVehicleDriveBy::ms_Aim("Aim",0xB01E2F36);
const fwMvRequestId CTaskAimGunVehicleDriveBy::ms_Intro("Intro",0xda8f5f53);
const fwMvRequestId CTaskAimGunVehicleDriveBy::ms_Hold("Hold",0x981F195B);
const fwMvRequestId CTaskAimGunVehicleDriveBy::ms_HoldEnd("HoldEnd",0xCCA288B1);
const fwMvRequestId CTaskAimGunVehicleDriveBy::ms_Blocked("Blocked",0x568F620C);
const fwMvRequestId CTaskAimGunVehicleDriveBy::ms_BikeBlocked("BikeBlocked",0x3D3B918B);
const fwMvFloatId CTaskAimGunVehicleDriveBy::ms_HeadingId("Heading",0x43F5CF44);
const fwMvFloatId CTaskAimGunVehicleDriveBy::ms_IntroHeadingId("IntroHeading",0x6EBE5858);
const fwMvFloatId CTaskAimGunVehicleDriveBy::ms_IntroRateId("IntroRate",0xC8EAB73F);
const fwMvFloatId CTaskAimGunVehicleDriveBy::ms_OutroHeadingId("OutroHeading",0xF30EE338);
const fwMvFloatId CTaskAimGunVehicleDriveBy::ms_OutroRateId("OutroRate",0xCC150EDC);
const fwMvFloatId CTaskAimGunVehicleDriveBy::ms_PitchId("Pitch",0x3F4BB8CC);
const fwMvFloatId CTaskAimGunVehicleDriveBy::ms_FirePhaseId("FirePhase",0x14F9F5C6);
const fwMvFloatId CTaskAimGunVehicleDriveBy::ms_RecoilPhaseId("RecoilPhase",0xEC885842);
const fwMvFloatId CTaskAimGunVehicleDriveBy::ms_FlipPhaseId("FlipPhase",0x9DBA2876);
const fwMvFloatId CTaskAimGunVehicleDriveBy::ms_BlockTransitionPhaseId("BlockTransitionPhase",0x8127701A);
const fwMvFloatId CTaskAimGunVehicleDriveBy::ms_IntroTransitionPhaseId("IntroTransitionPhase",0x51AF9558);
const fwMvFloatId CTaskAimGunVehicleDriveBy::ms_OutroTransitionPhaseId("OutroTransitionPhase",0x873857F2);
const fwMvFloatId CTaskAimGunVehicleDriveBy::ms_IntroToAimPhaseId("IntroToAimPhase",0xdade8459);
const fwMvFloatId CTaskAimGunVehicleDriveBy::ms_FireRateId("FireRate",0x41659CDC);
const fwMvFloatId CTaskAimGunVehicleDriveBy::ms_SpineAdditiveBlendDurationId("SpineAdditiveBlendDuration",0x36421B69);
const fwMvFlagId CTaskAimGunVehicleDriveBy::ms_UsePitchSolutionFlagId("UsePitchSolution",0x1A38D287);
const fwMvFlagId CTaskAimGunVehicleDriveBy::ms_UseThreeAnimIntroOutroFlagId("UseThreeAnimIntroOutro",0x714AEED6);
const fwMvFlagId CTaskAimGunVehicleDriveBy::ms_UseIntroFlagId("UseIntro",0xE167CF2D);
const fwMvFlagId CTaskAimGunVehicleDriveBy::ms_FiringFlagId("Firing",0x544C888B);
const fwMvFlagId CTaskAimGunVehicleDriveBy::ms_UseSpineAdditiveFlagId("UseSpineAdditive",0x767365D3);
const fwMvFlagId CTaskAimGunVehicleDriveBy::ms_FirstPersonMode("FirstPersonMode",0x8BB6FFFA);
const fwMvFlagId CTaskAimGunVehicleDriveBy::ms_FirstPersonModeIK("FirstPersonModeIK",0xA523AEC9);
const fwMvClipId CTaskAimGunVehicleDriveBy::ms_OutroClipId("OutroClip",0xA5A7BD2);
const fwMvClipId CTaskAimGunVehicleDriveBy::ms_FirstPersonGripClipId("FirstPersonGripClip",0xde570bb9);
const fwMvClipId CTaskAimGunVehicleDriveBy::ms_FirstPersonFireRecoilClipId("FirstPersonFireRecoilClip",0x0aa22b6e);
const fwMvFilterId CTaskAimGunVehicleDriveBy::ms_FirstPersonRecoilFilter("FirstPersonRecoilFilter", 0x1434cafa);
const fwMvFilterId CTaskAimGunVehicleDriveBy::ms_FirstPersonGripFilter("FirstPersonGripFilter", 0x6173c701);

////////////////////////////////////////////////////////////////////////////////

static const atHashWithStringNotFinal s_APPistolHash("WEAPON_APPISTOL", 0x22d8fe39);

CTaskAimGunVehicleDriveBy::CTaskAimGunVehicleDriveBy(const CWeaponController::WeaponControllerType weaponControllerType, const fwFlags32& iFlags, const CWeaponTarget& target)
: CTaskAimGun(weaponControllerType, iFlags, 0, target)
, m_pVehicleDriveByInfo(NULL)
, m_eType(CVehicleDriveByAnimInfo::STD_CAR_DRIVEBY)
, m_fDesiredPitch(-1.0f)
, m_fDesiredHeading(-1.0f)
, m_fCurrentHeading(-1.0f)
, m_fLastYaw(0.0f)
, m_fLastPitch(0.0f)
, m_fCurrentPitch(-1.0f)
, m_fHeadingChangeRate(0.0f)
, m_fPitchChangeRate(0.0f)
, m_fIntroHeadingBlend(-1.0f)
, m_fOutroHeadingBlend(-1.0f)
, m_fTransitionDesiredHeading(-1.0f)
, m_bBlocked(false)
, m_bBlockedHitPed(true)
, m_fMinHeadingAngle(0.0f)
, m_fMaxHeadingAngle(0.0f)
, m_fDrivebyOutroBlendOutPhase(-1.0f)
, m_uFireTime(0)
, m_uAimTime(0)
, m_uLastUsingAimStickTime(0)
, m_vTarget(Vector3::ZeroType)
, m_fTimeSinceLastInsult(LARGE_FLOAT)
, m_fLastHoldingTime(0)
, m_bUseVehicleAsCamEntity(false)
, m_bFire(false)
, m_bFiredOnce(false)
, m_bFiredTwice(false)
, m_bFireLoopOnEnter(false)
, m_bOverAimingClockwise(false)
, m_bOverAimingCClockwise(false)
, m_bFlipping(false)
, m_bHolding(false)
, m_bCheckForBlockTransition(false)
, m_bMoveFireLoopOnEnter(false)
, m_bMoveFireLoop1(false)
, m_bMoveFireLoop2(false)
, m_bMoveOutroClipFinished(false)
, m_bJustReloaded(false)
, m_UnarmedState(Unarmed_None)
, m_uLastUnarmedStateChangeTime(0)
, m_targetFireCounter(0)
, m_bRemoteInvokeFire(false)
, m_bUsingSecondaryTaskNetwork(false)
, m_bEquippedWeaponChanged(false)
, m_bDelayFireFinishedRequest(false)
, m_bForceUpdateMoveParamsForIntro(false)
, m_uTimeLastBlocked(0)
, m_fLastBlockedHeading(0.0f)
, m_fMinFPSIKYaw(-PI)
, m_fMaxFPSIKYaw(PI)
, m_uTimePoVCameraConstrained(0)
, m_vBlockedPosition(Vector3::ZeroType)
, m_vBlockedOffset(Vector3::ZeroType)
, m_vWheelClipOffset(Vector3::ZeroType)
, m_vWheelClipPosition(Vector3::ZeroType)
, m_fPenetrationDistance(0.0f)
, m_fSmoothedPenetrationDistance(0.0f)
, m_fBlockedTimer(0.0f)
, m_uBlockingCheckStartTime(0)
, m_bShouldDoProbeTestForAIOrClones(false)
, m_bCloneOrAIBlockedDueToPlayerFPS(false)
, m_bSkipDrivebyIntro(false)
, m_bRestoreWeaponToVisibleOnCleanup(false)
, m_bFlippedForBlockedInFineTuneLastUpdate(false)
, m_bBlockOutsideAimingRange(false)
#if FPS_MODE_SUPPORTED
, m_pFirstPersonIkHelper(NULL)
#endif // FPS_MODE_SUPPORTED
{
	SetInternalTaskType(CTaskTypes::TASK_AIM_GUN_VEHICLE_DRIVE_BY);
}

void CTaskAimGunVehicleDriveBy::CleanUp()
{
	CPed* pPed = GetPed();
	if( pPed )
	{
		CPed* myMount = pPed->GetMyMount();
		if( myMount )
		{
#if ENABLE_HORSE
			CHorseComponent* horseComponent = myMount->GetHorseComponent();
			if( horseComponent )
			{
				CReins* pReins = horseComponent->GetReins();
				if( pReins )
				{
// TODO: is weapon of a type that the rider has to hold it with 2 hands by default ?
// - add extra check for the weapon type and eventually don't attach at all if weapon has to be held with 2 hands ?
// Svetli
					pReins->SetIsRiderAiming( false );
					if( pReins->GetExpectedAttachState() == 2 )
						pReins->AttachTwoHands( pPed );
					else
						pReins->AttachOneHand( pPed );
				}
			}	
#endif		
		}
	}

	if (m_MoveNetworkHelper.IsNetworkActive())
	{
		static const fwMvRequestId outroId("Outro",0x5D40B28D);
		m_MoveNetworkHelper.SendRequest(outroId);
	}

	float fBlendDuration = GetState()!=State_Finish ? REALLY_SLOW_BLEND_DURATION : INSTANT_BLEND_DURATION;

	if(!m_bUsingSecondaryTaskNetwork)
	{
		pPed->GetMovePed().ClearTaskNetwork(m_MoveNetworkHelper, fBlendDuration); 
	}
	else
	{
		pPed->GetMovePed().ClearSecondaryTaskNetwork(m_MoveNetworkHelper, fBlendDuration); 
	}

	if (pPed)
	{
		bool bSetWeaponInvisible = false;
		if(pPed->GetIsParachuting() || pPed->GetPedResetFlag(CPED_RESET_FLAG_IsUsingJetpack))
		{
			bSetWeaponInvisible = true;
		}
		else
		{
			const CSeatOverrideAnimInfo* pOverrideInfo = CTaskMotionInVehicle::GetSeatOverrideAnimInfoFromPed(*pPed);
			if (!pOverrideInfo || !pOverrideInfo->GetWeaponVisibleAttachedToRightHand())
			{
				if (m_bUseVehicleAsCamEntity || pPed->GetMyMount())
				{
					bSetWeaponInvisible = true;
				}
			}
		}
		
		if(bSetWeaponInvisible)
		{
			weaponAssert(pPed->GetWeaponManager());
			CObject* pWeapon = pPed->GetWeaponManager()->GetEquippedWeaponObject();
			if(pWeapon && pWeapon->GetIsVisible())
			{
				pWeapon->SetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY, false, true);
				AI_LOG_WITH_ARGS("[WEAPON_VISIBILITY] - %s ped %s has set weapon %s to invisible on cleanup in CTaskAimGunVehicleDriveBy::CleanUp\n", AILogging::GetDynamicEntityIsCloneStringSafe(pPed), AILogging::GetDynamicEntityNameSafe(pPed), pWeapon->GetWeapon() ? (pWeapon->GetWeapon()->GetWeaponInfo() ? pWeapon->GetWeapon()->GetWeaponInfo()->GetName() : "NULL") : "NULL");
			}
		}
	}

	// B*2083339: Restore the weapon visibility if it was set invisible due to hack in ProcessPostCamera for 1st person drive-bys 
	if (m_bRestoreWeaponToVisibleOnCleanup)
	{
		m_bRestoreWeaponToVisibleOnCleanup = false;
		CObject* pWeapon = GetPed()->GetWeaponManager() ? GetPed()->GetWeaponManager()->GetEquippedWeaponObject() : NULL;
		if(pWeapon && !pWeapon->GetIsVisible())
		{
			pWeapon->SetIsVisibleForModule( SETISVISIBLE_MODULE_GAMEPLAY, true, true );
			AI_LOG_WITH_ARGS("[WEAPON_VISIBILITY] - %s ped %s has restored weapon %s visibility on cleanup in CTaskAimGunVehicleDriveBy::CleanUp\n", AILogging::GetDynamicEntityIsCloneStringSafe(pPed), AILogging::GetDynamicEntityNameSafe(pPed), pWeapon->GetWeapon() ? (pWeapon->GetWeapon()->GetWeaponInfo() ? pWeapon->GetWeapon()->GetWeaponInfo()->GetName() : "NULL") : "NULL");
		}
	}
#if __BANK
	else
	{
		AI_LOG_WITH_ARGS("[WEAPON_VISIBILITY] - %s ped %s isn't restoring weapon visible on cleanup in CTaskAimGunVehicleDriveBy::CleanUp\n", AILogging::GetDynamicEntityIsCloneStringSafe(pPed), AILogging::GetDynamicEntityNameSafe(pPed));
	}
#endif

#if FPS_MODE_SUPPORTED
	if (m_pFirstPersonIkHelper)
	{
		delete m_pFirstPersonIkHelper;
		m_pFirstPersonIkHelper = NULL;
	}
#endif // FPS_MODE_SUPPORTED

	// Base class
	CTaskAimGun::CleanUp();
}

const CWeaponInfo* CTaskAimGunVehicleDriveBy::GetPedWeaponInfo() const
{
	const CPed *pPed = GetPed();
	if(pPed->GetWeaponManager())
	{
		u32 uWeaponHash = pPed->GetWeaponManager()->GetEquippedWeaponHash();
		if( uWeaponHash == 0 )
		{
			const CVehicleWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedVehicleWeapon();
			if( pWeapon )
			{
				uWeaponHash = pWeapon->GetHash();
			}
		}
		
		return CWeaponInfoManager::GetInfo<CWeaponInfo>(uWeaponHash);
	}

	return NULL;
}

bool CTaskAimGunVehicleDriveBy::IsFlippingSomeoneOff() const
{
	//Ensure the weapon info is valid.
	const CWeaponInfo* pWeaponInfo = GetPedWeaponInfo();
	if(!pWeaponInfo)
	{
		return false;
	}
	
	//Ensure the weapon is unarmed.
	if(!pWeaponInfo->GetIsUnarmed())
	{
		return false;
	}
	
	return true;
}

void CTaskAimGunVehicleDriveBy::GetOverriddenGameplayCameraSettings(tGameplayCameraSettings& settings) const
{
	const CPed *pPed = GetPed(); //Get the ped ptr.

	if (pPed->IsLocalPlayer())
	{
		//Inhibit aim cameras during long and medium Switch transitions.
		if(pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_DISABLE_AIM_CAMERA) ||
			(g_PlayerSwitch.IsActive() && (g_PlayerSwitch.GetSwitchType() != CPlayerSwitchInterface::SWITCH_TYPE_SHORT)))
		{
			return;
		}

		const CControl *pControl = pPed->GetControlFromPlayer();
		if (pControl->GetPedTargetIsDown(ioValue::ANALOG_BUTTON_DOWN_THRESHOLD))
		{
			const bool isParachuting = pPed->GetIsParachuting() || pPed->GetPedResetFlag(CPED_RESET_FLAG_IsUsingJetpack);

			if ((pPed->GetMyVehicle() && pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle )) ||
				(pPed->GetMyMount() && pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_OnMount )) ||
				isParachuting)
			{
				if(isParachuting)
				{
					//NOTE: We do not request an aim camera for melee weapons and unarmed gestures when parachuting.
					const CWeaponInfo* weaponInfo	= camGameplayDirector::ComputeWeaponInfoForPed(*pPed);
					const bool isMeleeWeapon		= weaponInfo && weaponInfo->GetIsMelee();
					if(isMeleeWeapon)
					{
						return;
					}
				}

				const CVehicleDriveByInfo* pDriveByInfo = CVehicleMetadataMgr::GetVehicleDriveByInfoFromPed(pPed);
				if (pDriveByInfo)
				{
					u32 cameraHash = pDriveByInfo->GetDriveByCamera().GetHash();
					if (cameraHash)
					{
						settings.m_CameraHash = cameraHash;
					}
				}
			}
		}
	}
}

CTaskInfo* CTaskAimGunVehicleDriveBy::CreateQueriableState() const
{
	return rage_new CClonedAimGunVehicleDriveByInfo(GetState(), m_fireCounter, m_iAmmoInClip, m_seed);
}

void CTaskAimGunVehicleDriveBy::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	CClonedAimGunVehicleDriveByInfo *pAimGunInfo = static_cast<CClonedAimGunVehicleDriveByInfo*>(pTaskInfo);

	// Call the base implementation - syncs the state
	CTaskFSMClone::ReadQueriableState(pTaskInfo);

	// catch the case where the ped has fired but the fire state was missed by the network update
	if (pAimGunInfo->GetFireCounter() > m_fireCounter)
	{
		if ( IsStateValid() && ((GetState() == State_Aim) || (GetState() == State_Outro)) )
		{
			m_bRemoteInvokeFire = true;
		}

		m_iAmmoInClip			   = pAimGunInfo->GetAmmoInClip() + 1;
	}
	else
	{
		m_iAmmoInClip			   = pAimGunInfo->GetAmmoInClip();
	}

	m_targetFireCounter			   = pAimGunInfo->GetFireCounter();
	m_seed                         = pAimGunInfo->GetSeed();
}

void CTaskAimGunVehicleDriveBy::OnCloneTaskNoLongerRunningOnOwner()
{
	if (GetState() == State_Aim || GetState() == State_Fire)
	{
		SetStateFromNetwork(State_Outro);
	}
	else
	{
		SetStateFromNetwork(State_Finish);
	}
}

aiTask::FSM_Return CTaskAimGunVehicleDriveBy::UpdateClonedFSM(const s32 iState, const FSM_Event iEvent)
{
	if (m_bRemoteInvokeFire)
	{
		m_bFire = true;
		SetState(State_Fire);

		m_bRemoteInvokeFire = false; //clear
	}

	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_Intro)
			FSM_OnEnter
				Intro_OnEnter();
			FSM_OnUpdate
				return Intro_OnUpdate();

		FSM_State(State_Aim)
			FSM_OnEnter
				Aim_OnEnter();
			FSM_OnUpdate
				return Aim_OnUpdate();
			FSM_OnExit
				Aim_OnExit();

		FSM_State(State_Fire)
			FSM_OnEnter
				Fire_OnEnter();
			FSM_OnUpdate
				return Fire_OnUpdate();
			FSM_OnExit
				Fire_OnExit();

		FSM_State(State_Reload)
				FSM_OnEnter
				Reload_OnEnter();
			FSM_OnUpdate
				return Reload_OnUpdate();
			FSM_OnExit
				Reload_OnExit();

		FSM_State(State_VehicleReload)		
				FSM_OnEnter
				VehicleReload_OnEnter();
			FSM_OnUpdate
				return CloneVehicleReload_OnUpdate();

		FSM_State(State_Outro)
			FSM_OnEnter
				Outro_OnEnter();
			FSM_OnUpdate
				return Outro_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return Finish_OnUpdate();

	FSM_End
}

bool CTaskAimGunVehicleDriveBy::IsInScope(const CPed* pPed)
{
    bool inScope = CTaskFSMClone::IsInScope(pPed);

	// ensure the weapon and vehicle data is valid based on information we have received from the network
	weaponAssert(pPed->GetWeaponManager());
	const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
	if (!pWeapon)
	{
		inScope = false;
	}

	const CObject* pWeaponObject = pPed->GetWeaponManager()->GetEquippedWeaponObject();
	if (!pWeaponObject && !pPed->GetWeaponManager()->GetIsArmedMelee())
	{
		inScope = false;
	}

	if((!pPed->GetMyVehicle()) && !pPed->GetPedResetFlag(CPED_RESET_FLAG_IsParachuting) && !pPed->GetPedResetFlag(CPED_RESET_FLAG_IsUsingJetpack))
	{
		inScope = false;
	}

	// don't run the task if the player is in freecam. This happens when using the helicopter gun camera.
	if (pPed->IsAPlayerPed() && NetworkInterface::IsRemotePlayerUsingFreeCam(pPed))
	{
		inScope = false;
	}

    return inScope;
}

float CTaskAimGunVehicleDriveBy::GetScopeDistance() const
{
	return static_cast<float>(CTaskGun::CLONE_TASK_SCOPE_DISTANCE);
}

aiTask::FSM_Return CTaskAimGunVehicleDriveBy::ProcessPreFSM()
{
	CPed* pPed = GetPed(); 
	weaponAssert(pPed->GetWeaponManager());

	// Is a valid weapon available?
	const CVehicleWeapon* pVehicleWeapon = pPed->GetWeaponManager()->GetEquippedVehicleWeapon();
	const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
	if( !pVehicleWeapon && !pWeapon )
	{
		return FSM_Quit;
	}

// This was a fix for B* 960319, but causes B* 1366820.  If 960319 returns, a new solution may be necessary.
// 	if weapon suddenly changed, this task needs to end asap  
// 	if (pPed->GetPedResetFlag(CPED_RESET_FLAG_EquippedWeaponChanged))
// 		return FSM_Quit;

	// Is a valid weapon object available
	const CObject* pWeaponObject = pPed->GetWeaponManager()->GetEquippedWeaponObject();
	if( !pVehicleWeapon && !pWeaponObject && !pPed->GetWeaponManager()->GetIsArmedMelee())
	{
		return FSM_Quit;
	}

	CVehicle* myVehicle = pPed->GetVehiclePedInside();
	CPed* myMount = pPed->GetMyMount();
	if (!myVehicle && !myMount && !pPed->GetIsParachuting() && !pPed->GetPedResetFlag(CPED_RESET_FLAG_IsUsingJetpack))
	{
		return FSM_Quit;
	}

	if (m_overrideClipSetId != CLIP_SET_ID_INVALID)
	{
		fwClipSet* pClipSet = fwClipSetManager::GetClipSet(m_overrideClipSetId);
		if (!taskVerifyf(pClipSet, "NULL Clipset"))
		{
			return FSM_Quit;
		}
		
		if (!taskVerifyf(pClipSet->IsStreamedIn_DEPRECATED(), "Clip %s not streamed in", pClipSet->GetClipDictionaryName().GetCStr() ))
		{
			return FSM_Quit;
		}

		if (m_uFireTime!=0)
		{
			//update fire phase				
			float firePhase = Min(1.0f, static_cast<float>(fwTimer::GetTimeInMilliseconds()-m_uFireTime)/static_cast<float>(pWeapon->GetTimeOfNextShot() - m_uFireTime));		
			m_MoveNetworkHelper.SetFloat(ms_FirePhaseId, firePhase);
			if (firePhase >= 1.0f)
				m_uFireTime=0;
		}

		// Keep the clipset streamed in
		pClipSet->Request_DEPRECATED();
	}
	
	if (myMount) 
	{				
		if (!myMount->GetPedModelInfo()->GetLayoutInfo()->GetSeatAnimationInfo(myMount->GetSeatManager()->GetPedsSeatIndex(pPed)))
		{
			return FSM_Quit;
		}
	} 
	else if(myVehicle)
	{		
		if (!myVehicle->GetSeatAnimationInfo(pPed))
		{
			return FSM_Quit;
		}
	}

	// B*2031266: Check if any of the vehicle passengers is the player and in FPS mode. If so, we need to do some blocking checks to prevent clipping.
	m_bShouldDoProbeTestForAIOrClones = CTaskAimGunVehicleDriveBy::CanUseCloneAndAIBlocking(pPed) && CTaskAimGunVehicleDriveBy::ShouldUseRestrictedDrivebyAnimsOrBlockingIK(pPed);

	pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPostCamera, true );
	pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPostPreRender, true );
	pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPostPreRenderAfterAttachments, true );

	if(!pPed->IsUsingJetpack())
	{
		pPed->SetPedResetFlag( CPED_RESET_FLAG_IsHigherPriorityClipControllingPed, true );
	}

	pPed->SetPedResetFlag( CPED_RESET_FLAG_IsDoingDriveby, true );

	if (!NetworkInterface::IsGameInProgress() && pPed->IsLocalPlayer() && pPed->GetIsDrivingVehicle() && ms_UseAssistedAiming &&
		GetState() != State_Outro)
	{
		 pPed->SetPlayerResetFlag(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON);
	}

	//Restore player controls if needed
	if (pPed->IsLocalPlayer() && m_weaponControllerType!=CWeaponController::WCT_Player)
	{
		if (!pPed->GetPlayerInfo()->AreControlsDisabled())
		{
			m_weaponControllerType = CWeaponController::WCT_Player;			
		}
	}
	
	if (pPed->GetPedResetFlag(CPED_RESET_FLAG_EquippedWeaponChanged))
	{
		m_bEquippedWeaponChanged = true;
	}

	//Process the timers.
	ProcessTimers();
	
	//Process the insult.
	ProcessInsult();

	//! Block camera transition in certain states.
	switch(GetState())
	{
	case(State_Outro):
	case(State_Start):
	case(State_Reload):
	case(State_VehicleReload):
#if FPS_MODE_SUPPORTED
		if(pPed->IsLocalPlayer() && pPed->GetPlayerInfo())
		{
			pPed->SetPlayerResetFlag(CPlayerResetFlags::PRF_DISABLE_CAMERA_VIEW_MODE_CYCLE);
		}
#endif // FPS_MODE_SUPPORTED
		break;
	default:
		break;
	}

	//! No switching in 1st person.
	if(pPed->IsLocalPlayer() && pWeapon && pWeapon->GetWeaponInfo() && pWeapon->GetWeaponInfo()->GetIsUnarmed())
	{
		pPed->SetPlayerResetFlag(CPlayerResetFlags::PRF_DISABLE_CAMERA_VIEW_MODE_CYCLE);
	}

	return CTaskAimGun::ProcessPreFSM();
}

aiTask::FSM_Return CTaskAimGunVehicleDriveBy::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_Intro)
			FSM_OnEnter
				Intro_OnEnter();
			FSM_OnUpdate
				return Intro_OnUpdate();

		FSM_State(State_Aim)
			FSM_OnEnter
				Aim_OnEnter();
			FSM_OnUpdate
				return Aim_OnUpdate();
			FSM_OnExit
				Aim_OnExit();

		FSM_State(State_Fire)
			FSM_OnEnter
				Fire_OnEnter();
			FSM_OnUpdate
				return Fire_OnUpdate();
			FSM_OnExit
				Fire_OnExit();

		FSM_State(State_Reload)
			FSM_OnEnter
				Reload_OnEnter();
			FSM_OnUpdate
				return Reload_OnUpdate();
			FSM_OnExit
				Reload_OnExit();

		FSM_State(State_VehicleReload)		
			FSM_OnEnter
				VehicleReload_OnEnter();
			FSM_OnUpdate
				return VehicleReload_OnUpdate();

		FSM_State(State_Outro)
			FSM_OnUpdate
				return Outro_OnUpdate();
			FSM_OnExit
				Outro_OnExit();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return Finish_OnUpdate();

	FSM_End
}

bool CTaskAimGunVehicleDriveBy::ProcessPostPreRenderAfterAttachments()
{
	CPed *pPed = GetPed(); //Get the ped ptr.
	weaponAssert(pPed->GetWeaponManager());
	const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
	if (!pWeapon)
		return false;

	CVehicle *pVehicle = pPed->GetVehiclePedInside();

	//Get the attach and firing entities.
	CEntity* pAttachEntity = pVehicle ? pVehicle : NULL;
	CEntity* pFiringEntity = pVehicle ? static_cast<CEntity *>(pVehicle) : static_cast<CEntity *>(pPed);

	//Ensure the camera system is ready for this ped to fire. Firing is blocked if this ped is the local player and we're interpolating into a
	//vehicle aim camera for their vehicle, as we want the aiming reticule to settle before firing begins.
	camGameplayDirector& gameplayDirector = camInterface::GetGameplayDirector();
	bool shouldBlockFiringForCamera = pPed->IsLocalPlayer() && gameplayDirector.IsAiming(pAttachEntity) && gameplayDirector.IsCameraInterpolating();

	//! Keep automatic fire anim active for at least 1 frame.
	if(m_bDelayFireFinishedRequest && (GetState() != State_Fire) && ( (GetState() != State_Aim) || (GetTimeInState() > 0.1f) ) )
	{
		m_MoveNetworkHelper.SendRequest(ms_FireFinished);
		m_bDelayFireFinishedRequest = false;
	}

	if(!shouldBlockFiringForCamera && m_bFire && ( pPed->GetWeaponManager()->GetIsArmedGunOrCanBeFiredLikeGun() || pPed->GetWeaponManager()->GetEquippedVehicleWeapon() ) )
	{
		// Ignore fire events if we're not holding a gun weapon
		if ((pWeapon->GetState() == CWeapon::STATE_READY || pWeapon->GetWeaponInfo()->GetHash() == s_APPistolHash.GetHash()) || pPed->IsNetworkClone())
		{
			if (m_MoveNetworkHelper.IsNetworkActive())
			{
				if (pWeapon->GetWeaponInfo()->GetIsAutomatic())
				{
					static dev_bool DISABLE_FIRE_ANIMS = false;					
					if (!DISABLE_FIRE_ANIMS)						
					{
						const fwClipSet *pClipSet = m_MoveNetworkHelper.GetClipSet();
						if (pClipSet && pClipSet->GetClip(fwMvClipId("recoil_0",0xA9179254)) != NULL)
							m_MoveNetworkHelper.SendRequest(ms_FireWithRecoil);
						else if (m_eType == CVehicleDriveByAnimInfo::STD_CAR_DRIVEBY || m_eType == CVehicleDriveByAnimInfo::REAR_HELI_DRIVEBY)
							m_MoveNetworkHelper.SendRequest(ms_FireAuto);						
					}
				}
				else
				{
					m_MoveNetworkHelper.SendRequest(ms_Fire);
				}

				UpdateFPMoVEParams();

				if (m_eType == CVehicleDriveByAnimInfo::STD_CAR_DRIVEBY || m_eType == CVehicleDriveByAnimInfo::REAR_HELI_DRIVEBY)
				{
					m_MoveNetworkHelper.SetFloat(ms_FireRateId, GetFiringAnimPlaybackRate());
				}
			}
			m_uFireTime = fwTimer::GetTimeInMilliseconds();	
			
			//See if we would hit a window
			//reset
			if (m_iGunFireFlags.IsFlagSet(GFF_EnableDamageToOwnVehicle)) 
			{
				m_iGunFireFlags.ClearFlag(GFF_EnableDamageToOwnVehicle);
				m_iGunFireFlags.ClearFlag(GFF_FireFromMuzzle);
			}
			Matrix34 mWeapon(Matrix34::IdentityType);	
			Vector3 vStart, vEnd;

			bool bDamageVehicle = false;
			if(pPed->GetVehiclePedInside() && IsFirstPersonDriveBy() && !pWeapon->GetWeaponInfo()->GetIsRpg() && !pWeapon->GetWeaponInfo()->GetIsLaunched())
			{
				const s32 iSeatIndex = pPed->GetVehiclePedInside()->GetSeatManager()->GetPedsSeatIndex(pPed);
				const CVehicleSeatAnimInfo* pSeatAnimInfo = pPed->GetVehiclePedInside()->GetSeatAnimationInfo(iSeatIndex);

				if(pSeatAnimInfo)
				{
					bDamageVehicle = pSeatAnimInfo->GetKeepCollisionOnWhenInVehicle();
				}
			}
			
			bool bHasFireVector = pPed->IsLocalPlayer() && (pWeapon->CalcFireVecFromAimCamera(pFiringEntity, mWeapon, vStart, vEnd));
			if (!bDamageVehicle && bHasFireVector && pFiringEntity)
			{
				// Get muzzle position. In 1st person, just use camera position for this test as we fire from camera in this mode.
				if(!IsFirstPersonDriveBy())
				{
					Matrix34 muzzleMatrix;
					pWeapon->GetMuzzleMatrix(mWeapon, muzzleMatrix);
					vStart = muzzleMatrix.d;
				}

				Vector3 vAimDir = vEnd - vStart;
				vAimDir.NormalizeFast();
				static dev_float PROBE_LENGTH = 3.5f;
				Vector3 vTestEnd;
				vTestEnd = vStart + (vAimDir * PROBE_LENGTH);

				int nExcludeOptions = WorldProbe::EIEO_DONT_ADD_VEHICLE;	

				if(IsFirstPersonDriveBy())
				{
					nExcludeOptions |= WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS;	
				}		

				WorldProbe::CShapeTestFixedResults<2> probeResults;
				WorldProbe::CShapeTestProbeDesc probeDesc;
				probeDesc.SetResultsStructure(&probeResults);
				probeDesc.SetStartAndEnd(vStart, vTestEnd);
				probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_WEAPON_TYPES);
				probeDesc.SetTypeFlags(ArchetypeFlags::GTA_WEAPON_TYPES);
				probeDesc.SetExcludeTypeFlags(TYPE_FLAGS_NONE);
				probeDesc.SetExcludeEntity(pPed, nExcludeOptions);
				probeDesc.SetContext(WorldProbe::LOS_GeneralAI);
				if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
				{
					WorldProbe::ResultIterator it;
					for(it = probeResults.begin(); it < probeResults.end(); ++it)
					{
						//! if we hit a ped in 1st person, stop so we can shoot them!
						if(IsFirstPersonDriveBy() && it->GetHitEntity() && it->GetHitEntity()->GetIsTypePed())
						{
							break;
						}

						if( it->GetHitDetected() )
						{
							// See if we hit our own window 											
							if (PGTAMATERIALMGR->GetIsSmashableGlass(it->GetHitMaterialId()))
							{
								if(pVehicle)
								{
									bool bPossibleWindowHit = true;
									bool bIsProjectileWeapon = pWeapon->GetWeaponInfo()->GetFireType() == FIRE_TYPE_PROJECTILE;

									for(int nId = VEH_FIRST_WINDOW; nId <= VEH_LAST_WINDOW; nId++)
									{	
										const int windowBoneId = pVehicle->GetBoneIndex((eHierarchyId)nId);
										if (windowBoneId >= 0 && pVehicle->GetFragInst()->GetComponentFromBoneIndex(windowBoneId) == it->GetHitComponent())
										{
											if (bIsProjectileWeapon)
											{
												pVehicle->SmashWindow((eHierarchyId)nId, VEHICLEGLASSFORCE_WEAPON_IMPACT, true);
											}
											else
											{
												if ((eHierarchyId)nId == VEH_WINDSCREEN_R && pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_IGNORE_RWINDOW_COLLISION))
												{
													pVehicle->AddInternalWindshieldRearHit();
													static dev_u8 HITS_BEFORE_REAR_WINDSHIELD_BREAK = 6;
													if (pVehicle->GetNumInternalWindshieldRearHits() == HITS_BEFORE_REAR_WINDSHIELD_BREAK)										
														pVehicle->SmashWindow(VEH_WINDSCREEN_R, VEHICLEGLASSFORCE_WEAPON_IMPACT, true);										

													//! Always do window damage in 1st person.
													if(!IsFirstPersonDriveBy())
														bPossibleWindowHit = false;
												}
												else if ((eHierarchyId)nId == VEH_WINDSCREEN)
												{
													pVehicle->AddInternalWindshieldHit();
													static dev_u8 HITS_BEFORE_WINDSHIELD_BREAK = 6;
													if (pVehicle->GetNumInternalWindshieldHits() == HITS_BEFORE_WINDSHIELD_BREAK)										
														pVehicle->SmashWindow(VEH_WINDSCREEN, VEHICLEGLASSFORCE_WEAPON_IMPACT, true);		

													//! Always do window damage in 1st person.
													if(!IsFirstPersonDriveBy())
														bPossibleWindowHit = false;
												}
											}
											break;
										}
									}
									if (bPossibleWindowHit)
									{
										if(PGTAMATERIALMGR->UnpackMtlId(it->GetHitMaterialId()) == PGTAMATERIALMGR->g_idCarGlassBulletproof)
										{
											m_iGunFireFlags.SetFlag(GFF_PassThroughOwnVehicleBulletProofGlass); 
										}
										
										// Don't want projectiles from weapons to get stuck in the car collision (flare gun)
										if (!bIsProjectileWeapon)
										{
											m_iGunFireFlags.SetFlag(GFF_EnableDamageToOwnVehicle);
										}

										m_iGunFireFlags.SetFlag(GFF_FireFromMuzzle);	
									}
								}
							}		
						}
					}		
				} 			
			}
			else
			{ 
				if (!pPed->IsNetworkPlayer())
				{
					//AI blast their own windows B* 888218
					m_iGunFireFlags.SetFlag(GFF_EnableDamageToOwnVehicle);
				}
			}

			//! For more accurate firing, fire from camera (this ensures we break glass at reticule location, which looks nicer).
			if(IsFirstPersonDriveBy())
			{
				m_iGunFireFlags.SetFlag(GFF_ForceFireFromCamera);	
			}

			FireGun(pPed, true, m_bUseVehicleAsCamEntity && pVehicle);
			m_bFire = false;
			if (m_bFiredOnce)
			{
				m_bFiredTwice = true;
			}
			m_bFiredOnce = true;
		}
	}

	// Disabled hand ik for now as it looks bad at extreme angles, we should selectively use the ik, maybe use events in the clips?
	TUNE_GROUP_BOOL(VEHICLE_DRIVE_BY, useHandIk, true);

	const CVehicleSeatAnimInfo* pSeatClipInfo = CVehicleMetadataMgr::GetSeatAnimInfoFromPed(pPed);
	if (useHandIk && pSeatClipInfo && !pSeatClipInfo->GetNoIK()) 
	{
		if (pPed->GetMyMount()==NULL && !pPed->GetIsParachuting() && !pPed->GetPedResetFlag(CPED_RESET_FLAG_IsUsingJetpack)) //riders will need extra love for 1h->2h transitions
		{			
			if (GetState() == State_Intro || GetState() == State_Aim || GetState() == State_Fire)
			{
				const CWeaponInfo* pWeaponInfo = pWeapon->GetWeaponInfo();
				if (pWeaponInfo && !pWeaponInfo->GetDisableLeftHandIkInDriveby())
				{
					u32 weaponGroup = pWeaponInfo->GetGroup();
					const atHashWithStringNotFinal uRailGunHash ("WEAPON_RAILGUN", 0x6d544c99);
					if (weaponGroup == WEAPONGROUP_MG || weaponGroup == WEAPONGROUP_RIFLE || weaponGroup == WEAPONGROUP_SNIPER || (weaponGroup == WEAPONGROUP_SMG && pWeaponInfo->GetIsTwoHanded()) || pWeaponInfo->GetHash() == uRailGunHash)
						pPed->ProcessLeftHandGripIk(false);
				}
			}
		}
	} 
	
	if ((!pPed->IsLocalPlayer() || (GetState() != State_Aim && GetState() != State_Fire)) && !fwTimer::IsGamePaused())
	{
		UpdateMoVEParams();
	}

	return false;
}

#if FPS_MODE_SUPPORTED
bool CTaskAimGunVehicleDriveBy::IsStateValidForFPSIK() const
{
	const CPed* pPed = GetPed();

	if(!IsFirstPersonDriveBy())
	{
		return false;
	}

	if(IsPoVCameraConstrained(pPed))
	{
		return false;
	}

	const CWeaponInfo* pWeaponInfo = CTaskMotionPed::GetWeaponInfo(pPed);
	if (!pWeaponInfo)
	{
		return false;
	}

	if(pWeaponInfo->GetIsUnarmed())
	{
		TUNE_GROUP_BOOL(VEHICLE_DRIVE_BY, bUseUnarmedFPSIK, true);
		if(!bUseUnarmedFPSIK)
		{
			return false;
		}
	}
	else if (!pWeaponInfo->GetUseFPSAimIK())
	{
		return false;
	}

	switch(GetState())
	{
	case(State_Aim):
	case(State_Fire):
	case(State_VehicleReload):
	case(State_Intro):
	case(State_Outro):
		return true;
	default:
		break;
	}

	return false;
}
#endif

bool CTaskAimGunVehicleDriveBy::ProcessPostCamera()
{
	TUNE_GROUP_BOOL(VEHICLE_DRIVE_BY, doParamUpdatePostCam, true);

	if(GetPed()->GetPedResetFlag(CPED_RESET_FLAG_PoVCameraConstrained)) 
	{
		m_uTimePoVCameraConstrained = fwTimer::GetTimeInMilliseconds();
	}

#if FPS_MODE_SUPPORTED
	if (IsStateValidForFPSIK())
	{
		//! Detect a switch in IK Type, so we can restart FPS solver.
		CFirstPersonIkHelper::eFirstPersonType eIKType = GetPed()->IsInFirstPersonVehicleCamera() ? CFirstPersonIkHelper::FP_DriveBy : CFirstPersonIkHelper::FP_OnFootDriveBy;

		//! If we change types, just delete and create again. It's quite rare.
		if(m_pFirstPersonIkHelper && (m_pFirstPersonIkHelper->GetType() != eIKType) )
		{
			delete m_pFirstPersonIkHelper;
			m_pFirstPersonIkHelper = NULL;
		}

		//! If unarmed, pick arm to IK depending on which side of vehicle we are on.
		CFirstPersonIkHelper::eArm armToIK = CFirstPersonIkHelper::FP_ArmRight;
		const CWeaponInfo* pWeaponInfo = CTaskMotionPed::GetWeaponInfo(GetPed());
		Assert(pWeaponInfo);

		if( GetPed()->GetVehiclePedInside())
		{
			if(pWeaponInfo->GetIsUnarmed())
			{
				armToIK = m_pVehicleDriveByInfo->GetLeftHandedUnarmedFirstPersonAnims() ? CFirstPersonIkHelper::FP_ArmLeft : CFirstPersonIkHelper::FP_ArmRight;
			}
			else
			{
				armToIK = m_pVehicleDriveByInfo->GetLeftHandedFirstPersonAnims() ? CFirstPersonIkHelper::FP_ArmLeft : CFirstPersonIkHelper::FP_ArmRight;
			}
		}

		if (m_pFirstPersonIkHelper == NULL)
		{
			m_pFirstPersonIkHelper = rage_new CFirstPersonIkHelper(eIKType);
			taskAssert(m_pFirstPersonIkHelper);

			m_pFirstPersonIkHelper->SetArm(armToIK);
		}


		if (m_pFirstPersonIkHelper)
		{
			Vector3 vTargetOffset(Vector3::ZeroType);
			if(GetPed()->GetIsParachuting())
			{
				vTargetOffset = CTaskParachute::GetFirstPersonDrivebyIKOffset();
			}
			else if(GetPed()->GetVehiclePedInside())
			{
				CVehicle *pVehicle = GetPed()->GetVehiclePedInside();
				CVehicleModelInfo *pModelInfo = pVehicle->GetVehicleModelInfo();
				s32 iSeat = GetPed()->GetVehiclePedInside()->GetSeatManager()->GetPedsSeatIndex(GetPed());
				if(pModelInfo)
				{
#if __BANK
					REGISTER_TUNE_GROUP_BOOL(bOverrideOffsetValues, false);
					TUNE_GROUP_FLOAT(FIRST_PERSON_STEERING, fOverrideOffsetX, vTargetOffset.x, -10.0f, 10.0f, 0.01f);
					TUNE_GROUP_FLOAT(FIRST_PERSON_STEERING, fOverrideOffsetY, vTargetOffset.y, -10.0f, 10.0f, 0.01f);
					TUNE_GROUP_FLOAT(FIRST_PERSON_STEERING, fOverrideOffsetZ, vTargetOffset.z, -10.0f, 10.0f, 0.01f);
					bool bOverridingOffsetForDriver = false;
#endif //__BANK
					if(pVehicle->GetDriver()==GetPed())
					{
						if (pWeaponInfo->GetIsUnarmed())
						{
							vTargetOffset = pModelInfo->GetFirstPersonDriveByUnarmedIKOffset();
						}
						else
						{
							vTargetOffset = pModelInfo->GetFirstPersonDriveByIKOffset();
							if (pVehicle->GetVehicleType() == VEHICLE_TYPE_CAR)
							{
#if __BANK
								INSTANTIATE_TUNE_GROUP_BOOL(FIRST_PERSON_STEERING,bOverrideOffsetValues);
								if (bOverrideOffsetValues)
								{
									bOverridingOffsetForDriver = true;
									vTargetOffset = Vector3(fOverrideOffsetX, fOverrideOffsetY, fOverrideOffsetZ);
								}
#endif //__BANK
								ProcessWheelClipIKOffset(vTargetOffset);
							}
						}
					}
					else
					{
						if(armToIK == CFirstPersonIkHelper::FP_ArmLeft)
						{
							if (iSeat > 1)
							{
								vTargetOffset = pWeaponInfo->GetIsUnarmed() ? pModelInfo->GetFirstPersonDriveByRightPassengerUnarmedIKOffset() : pModelInfo->GetFirstPersonDriveByRightRearPassengerIKOffset();
							}
							else
							{
								vTargetOffset = pWeaponInfo->GetIsUnarmed() ? pModelInfo->GetFirstPersonDriveByRightPassengerUnarmedIKOffset() : pModelInfo->GetFirstPersonDriveByRightPassengerIKOffset();
							}
						}
						else
						{
							vTargetOffset = pWeaponInfo->GetIsUnarmed() ? pModelInfo->GetFirstPersonDriveByLeftPassengerUnarmedIKOffset() : pModelInfo->GetFirstPersonDriveByLeftPassengerIKOffset();
						}
					}

					ProcessBlockedIKOffset(vTargetOffset, pWeaponInfo->GetFirstPersonDrivebyIKOffset());
					
#if __BANK			// Allow override of values for tweaking purposes
					INSTANTIATE_TUNE_GROUP_BOOL(FIRST_PERSON_STEERING,bOverrideOffsetValues);
					if (bOverrideOffsetValues && !bOverridingOffsetForDriver)
					{
						vTargetOffset = Vector3(fOverrideOffsetX, fOverrideOffsetY, fOverrideOffsetZ);
					}
#endif // __BANK
				}
			}

			TUNE_GROUP_BOOL(FIRST_PERSON_STEERING, bUseFastestBlendTime, false);
			if(bUseFastestBlendTime)
			{
				m_pFirstPersonIkHelper->SetBlendInRate(ARMIK_BLEND_RATE_FASTEST);
			}

			m_pFirstPersonIkHelper->SetOffset(RCC_VEC3V(vTargetOffset));
			m_pFirstPersonIkHelper->SetMinMaxDriveByYaw(m_fMinFPSIKYaw, m_fMaxFPSIKYaw);
			m_pFirstPersonIkHelper->Process(GetPed());

			if(GetState() > State_Intro)
			{
				CObject* pWeapon = GetPed()->GetWeaponManager()->GetEquippedWeaponObject();
				if( pWeapon && !pWeapon->GetIsVisible() )
				{
					pWeapon->SetIsVisibleForModule( SETISVISIBLE_MODULE_GAMEPLAY, true, true );
				}
			}
		}
	}
	else if (m_pFirstPersonIkHelper)
	{
		m_pFirstPersonIkHelper->Reset();
	}
#endif // FPS_MODE_SUPPORTED

	// B*2031266: Clone/AI ped has been blocked against another ped and there is a player in FPS mode in the car, so we need to IK the peds hand out of the way to avoid clipping.
	if (!GetPed()->IsLocalPlayer())
	{
		Vector3 vTargetOffset = Vector3::ZeroType;

		ProcessBlockedIKOffset(vTargetOffset, Vector3::ZeroType);

		if (vTargetOffset.IsNonZero())
		{
			Vector3 vIKOffset = Vector3(vTargetOffset.y, vTargetOffset.x, vTargetOffset.z);
			CIkManager& ikManager = GetPed()->GetIkManager();
			bool bLeftHand = GetPed()->GetWeaponManager() && GetPed()->GetWeaponManager()->GetEquippedWeapon() && GetPed()->GetWeaponManager()->GetEquippedWeaponAttachPoint() == CPedEquippedWeapon::AP_LeftHand;
			crIKSolverArms::eArms arm = bLeftHand ? crIKSolverArms::LEFT_ARM : crIKSolverArms::RIGHT_ARM;
			int iBoneIndex = bLeftHand ? GetPed()->GetBoneIndexFromBoneTag(BONETAG_L_HAND) : GetPed()->GetBoneIndexFromBoneTag(BONETAG_R_HAND);
			TUNE_GROUP_FLOAT(DRIVEBY_DEBUG, fIKBlendInTimeClonesAI, 0.20f, 0.0f, 1.0f, 0.01f);
			TUNE_GROUP_FLOAT(DRIVEBY_DEBUG, fIKBlendOutTimeClonesAI, 0.33f, 0.0f, 1.0f, 0.01f);
			ikManager.SetRelativeArmIKTarget(arm, GetPed(), iBoneIndex, vIKOffset, 0, fIKBlendInTimeClonesAI, fIKBlendOutTimeClonesAI);
		}
	}

	//! DMKH. Hack. In 1st person mode we want to render ped, but we can't show the right hand as anims just don't work in 1st person. So solution is to IK
	//! R arm so that we can't see it. This is ok as gun fires from camera position, not muzzle.
	if(IsHidingGunInFirstPersonDriverCamera(GetPed()))
	{
		CObject* pWeapon = GetPed()->GetWeaponManager()->GetEquippedWeaponObject();
		if( pWeapon && pWeapon->GetIsVisible() )
		{
			m_bRestoreWeaponToVisibleOnCleanup = true;	//Ensure the weapon gets restored to visible in the task cleanup.
			pWeapon->SetIsVisibleForModule( SETISVISIBLE_MODULE_GAMEPLAY, false, true );
		}

		TUNE_GROUP_FLOAT(FIRST_PERSON_STEERING, fDrivebyXOffset, 0.2f, -2.0f, 2.0f, 0.01f);
		TUNE_GROUP_FLOAT(FIRST_PERSON_STEERING, fDrivebyYOffset, 0.2f, -2.0f, 2.0f, 0.01f);
		TUNE_GROUP_FLOAT(FIRST_PERSON_STEERING, fDrivebyZOffset, 0.0f, -2.0f, 2.0f, 0.01f);
		TUNE_GROUP_FLOAT(FIRST_PERSON_STEERING, fDrivebyBlendIn, 0.5f, -2.0f, 2.0f, 0.01f);
		TUNE_GROUP_FLOAT(FIRST_PERSON_STEERING, fDrivebyBlendOut, 0.5f, -2.0f, 2.0f, 0.01f);
		Vector3 vOffset(Vector3::ZeroType);
		vOffset.x = fDrivebyXOffset;
		vOffset.y = fDrivebyYOffset;
		vOffset.z = fDrivebyZOffset;
		
		crIKSolverArms::eArms eArmToIK = crIKSolverArms::RIGHT_ARM;

		const CVehicleDriveByInfo* pDrivebyInfo = CVehicleMetadataMgr::GetVehicleDriveByInfoFromPed(GetPed());
		taskAssert(pDrivebyInfo);
		//! set grip anims
		if(pDrivebyInfo->GetLeftHandedFirstPersonAnims())
		{
			eArmToIK = crIKSolverArms::LEFT_ARM;
		}
		
		GetPed()->GetIkManager().SetRelativeArmIKTarget( eArmToIK, 
			GetPed(),
			GetPed()->GetBoneIndexFromBoneTag(BONETAG_ROOT),
			vOffset,
			AIK_TARGET_WRT_HANDBONE,
			fDrivebyBlendIn, 
			fDrivebyBlendOut);
	}

	if (doParamUpdatePostCam && !fwTimer::IsGamePaused())
	{
		CPed* pPed = GetPed();
		// Update the player's clips for aim state to prevent the clip lagging behind
		if ( ( (GetState() == State_Intro && m_bForceUpdateMoveParamsForIntro) || GetState() == State_Aim || GetState() == State_Fire) && pPed->IsLocalPlayer())
		{
			UpdateMoVEParams();
			GetPed()->SetPedResetFlag( CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true );
			GetPed()->SetPedResetFlag( CPED_RESET_FLAG_ePostCameraAnimUpdateUseZeroTimestep, true );
		}
	}

	return true;
}

void CTaskAimGunVehicleDriveBy::ProcessWheelClipIKOffset(Vector3 &vTargetOffset)
{
	// B*2113389: Lerp weapon position up to avoid clipping with the steering wheel
	float fSteerAngle = 0.0f;
	CPed *pPed = GetPed();
	const CVehicle *pVehicle = pPed->GetVehiclePedInside();
	CTaskMotionBase *pCurrentMotionTask = pVehicle->GetDriver()->GetCurrentMotionTask();
	if (pCurrentMotionTask && pCurrentMotionTask->GetTaskType() == CTaskTypes::TASK_MOTION_IN_AUTOMOBILE)
	{
		const CTaskMotionInAutomobile* pAutoMobileTask = static_cast<const CTaskMotionInAutomobile*>(pCurrentMotionTask);
		fSteerAngle = pAutoMobileTask->GetSteeringWheelAngle();
	}

	//Initialize to the current gun position
	if (m_vWheelClipPosition.IsZero())
	{
		m_vWheelClipPosition = vTargetOffset;
	}
	
	const CFirstPersonDriveByLookAroundData *pDrivebyData = camCinematicMountedCamera::GetFirstPersonDrivebyData(pVehicle);

	if (pDrivebyData)
	{
		const CFirstPersonDriveByWheelClipData & pWheelClipData = pDrivebyData->GetWheelClipInfo();
		float fHeadingLimitsLeftMin = pWheelClipData.m_HeadingLimitsLeft.x;
		float fHeadingLimitsLeftMax = pWheelClipData.m_HeadingLimitsLeft.y;
		float fHeadingLimitsRightMin = pWheelClipData.m_HeadingLimitsRight.x;
		float fHeadingLimitsRightMax = pWheelClipData.m_HeadingLimitsRight.y;
		float fPitchLimitsMin = pWheelClipData.m_PitchLimits.x;
		float fPitchLimitsMax = pWheelClipData.m_PitchLimits.y;

		//Apply offset only in certain heading and pitch range
		if ((m_fCurrentHeading > fHeadingLimitsLeftMin) && (m_fCurrentHeading < fHeadingLimitsRightMax) && (m_fCurrentPitch <= fPitchLimitsMax))
		{
			float fWheelClipOffset = 0.0f;

			//Move the gun up if steering wheel is turned right
			float fWheelAngleOffset = 0.0f;
			float fWheelAngleMin = pWheelClipData.m_WheelAngleLimits.x;
			float fWheelAngleMax = pWheelClipData.m_WheelAngleLimits.y;
			float fWheelAngleOffsetMin = pWheelClipData.m_WheelAngleOffset.x;
			float fWheelAngleOffsetMax = pWheelClipData.m_WheelAngleOffset.y;
			float fSteerAngleClamped = Clamp(fSteerAngle, fWheelAngleMin, fWheelAngleMax);

			float fPositiveTranslationOffset = abs(fWheelAngleMin);
			fSteerAngleClamped += fPositiveTranslationOffset; //Translate into non negative range for easier calculation

			fWheelAngleOffset = TranslateRangeToRange(fSteerAngleClamped, fWheelAngleMin+fPositiveTranslationOffset, fWheelAngleMax+fPositiveTranslationOffset,fWheelAngleOffsetMin,fWheelAngleOffsetMax);
			// Inverting the offset, so turning right makes the gun go higher
			fWheelAngleOffset = fWheelAngleOffsetMin + fWheelAngleOffsetMax - fWheelAngleOffset;

			//Move the gun up if player is aiming low
			float fPitchOffset = 0.0f;
			float fPitchOffsetMin = pWheelClipData.m_PitchOffset.x;
			float fPitchOffsetMax = pWheelClipData.m_PitchOffset.y;
			float fPitchClamped = Clamp(m_fCurrentPitch, fPitchLimitsMin, fPitchLimitsMax);
			fPitchOffset = TranslateRangeToRange(fPitchClamped, fPitchLimitsMin, fPitchLimitsMax,fPitchOffsetMin,fPitchOffsetMax);
			//Inverting the offset, so that lower the pitch angle means bigger the offset
			fPitchOffset = fPitchOffsetMin + fPitchOffsetMax - fPitchOffset;

			fWheelClipOffset = fWheelClipOffset + fPitchOffset + fWheelAngleOffset;

			float fNewWheelClipOffset = fWheelClipOffset;

			//Reduce the offset on the right side of the wheel quicker, because arm gets in the way on the left
			if (m_fCurrentHeading >= fHeadingLimitsRightMin)
			{
				float fHeadingRatio;

				if( (fHeadingLimitsRightMax - fHeadingLimitsRightMin) <= SMALL_FLOAT)
				{
					fHeadingRatio = 1.0f;
				}
				else
				{
					fHeadingRatio = Clamp( (m_fCurrentHeading - fHeadingLimitsRightMin) / (fHeadingLimitsRightMax - fHeadingLimitsRightMin), 0.0f, 1.0f);
				}

				fNewWheelClipOffset = fWheelClipOffset * fHeadingRatio;
				fNewWheelClipOffset = fWheelClipOffset - fNewWheelClipOffset; //Invert
			}
			else if (m_fCurrentHeading <= fHeadingLimitsLeftMax)
			{
				float fHeadingRatio;

				if( (fHeadingLimitsLeftMax - fHeadingLimitsLeftMin) <= SMALL_FLOAT)
				{
					fHeadingRatio = 1.0f;
				}
				else
				{
					fHeadingRatio = Clamp( (m_fCurrentHeading - fHeadingLimitsLeftMin) / (fHeadingLimitsLeftMax - fHeadingLimitsLeftMin), 0.0f, 1.0f);
				}

				fNewWheelClipOffset = fWheelClipOffset * fHeadingRatio;
			}

			float fMaxWheelOffsetY = pWheelClipData.m_MaxWheelOffsetY;
			float fWheelClipClamped = Clamp(fNewWheelClipOffset, 0.0f, fMaxWheelOffsetY);

			m_vWheelClipOffset = Vector3(0.0f, 0.0f, fWheelClipClamped);

			float fWheelClipLerpInRate = pWheelClipData.m_WheelClipLerpInRate;
			Vector3 vDesiredOffsetPosition = vTargetOffset + m_vWheelClipOffset;
			m_vWheelClipPosition = Lerp(fWheelClipLerpInRate, m_vWheelClipPosition, vDesiredOffsetPosition);
			vTargetOffset = m_vWheelClipPosition;
		}
		else if (m_vWheelClipPosition.IsNonZero())
		{
			//Lerp out back to normal offset
			float fWheelClipLerpOutRate = pWheelClipData.m_WheelClipLerpOutRate;
			m_vWheelClipPosition = Lerp(fWheelClipLerpOutRate, m_vWheelClipPosition, vTargetOffset);

			vTargetOffset = m_vWheelClipPosition;
		}
		else
		{
			m_vWheelClipPosition = Vector3::ZeroType;
		}
	}
}

float CTaskAimGunVehicleDriveBy::TranslateRangeToRange(float fCurrentValue, float fOldMin, float fOldMax, float fNewMin, float fNewMax)
{
	return (((fCurrentValue - fOldMin) * (fNewMax - fNewMin)) / (fOldMax - fOldMin)) + fNewMin;
}

void CTaskAimGunVehicleDriveBy::ProcessBlockedIKOffset(Vector3 &vTargetOffset, const Vector3& vGunTargetOffset)
{
	Vector3 vTemp = vTargetOffset + vGunTargetOffset ;

	// B*6510231: If weapon is blocked, lerp the position back using a blocked offset
	TUNE_GROUP_BOOL(DRIVEBY_DEBUG, bUseBlockedOffsetFPS, true);
	if (bUseBlockedOffsetFPS)
	{
		Vector3 vDifferenceToTargetPos = m_vBlockedPosition - vTargetOffset;

		if ((m_bBlocked || m_bCloneOrAIBlockedDueToPlayerFPS) && CTaskAimGunVehicleDriveBy::CanUseCloneAndAIBlocking(GetPed()))
		{
			if (!m_bBlockedHitPed && m_bHolding)
			{
				return;
			}

			// Initialize the blocked position to the current target offset
			if (m_vBlockedPosition.IsZero())
			{
				m_vBlockedPosition = vTargetOffset;
			}

			m_fBlockedTimer = 0.0f;

			float fBlockedLerpInRate = 0.0f;

			if (m_bCloneOrAIBlockedDueToPlayerFPS)
			{
				// Offset gun by penetration depth, clamped within reason
				TUNE_GROUP_FLOAT(DRIVEBY_DEBUG, fMaxBlockedOffsetY_ClonesOrAI, -0.095f, -10.0f, 10.0f, 0.01f);
				float fPenetrationDepthClamped = Clamp(m_fPenetrationDistance, fMaxBlockedOffsetY_ClonesOrAI, 0.0f);

				TUNE_GROUP_FLOAT(DRIVEBY_DEBUG, fBlockedPenetrationSmoothRate_ClonesOrAI, 0.1f, -10.0f, 10.0f, 0.01f);
				m_fSmoothedPenetrationDistance = Lerp(fBlockedPenetrationSmoothRate_ClonesOrAI, m_fSmoothedPenetrationDistance, fPenetrationDepthClamped);

				m_vBlockedOffset = Vector3(0.0f, m_fSmoothedPenetrationDistance, 0.0f);

				TUNE_GROUP_FLOAT(DRIVEBY_DEBUG, fBlockedLerpInRate_ClonesOrAI, 0.33f, -10.0f, 10.0f, 0.01f);
				fBlockedLerpInRate = fBlockedLerpInRate_ClonesOrAI;
			}
			else
			{
				// Offset gun by penetration depth, clamped within reason
				TUNE_GROUP_FLOAT(DRIVEBY_DEBUG, fMaxBlockedOffsetY_FPS, -0.250f, -10.0f, 10.0f, 0.01f);
				float fPenetrationDepthClamped = Clamp(m_fPenetrationDistance, fMaxBlockedOffsetY_FPS, 0.0f);

				// We aren't constantly doing the capsule test so the penteration distance can sometimes jump so we need to smooth this parameter.
				TUNE_GROUP_FLOAT(DRIVEBY_DEBUG, fBlockedPenetrationSmoothRate_FPS, 0.1f, -10.0f, 10.0f, 0.01f);
				m_fSmoothedPenetrationDistance = Lerp(fBlockedPenetrationSmoothRate_FPS, m_fSmoothedPenetrationDistance, fPenetrationDepthClamped);

				m_vBlockedOffset = Vector3(0.0f, m_fSmoothedPenetrationDistance, 0.0f);

				TUNE_GROUP_FLOAT(DRIVEBY_DEBUG, fBlockedLerpInRate_FPS, 0.33f, -10.0f, 10.0f, 0.01f);
				fBlockedLerpInRate = fBlockedLerpInRate_FPS;
			}

			Vector3 vDesiredOffsetPosition = vTargetOffset + m_vBlockedOffset;

			m_vBlockedPosition = Lerp(fBlockedLerpInRate, m_vBlockedPosition, vDesiredOffsetPosition);
			if (vTemp.GetY() < m_vBlockedPosition.GetY())
			{
				vTargetOffset = vTemp;
			}
			else
			{
				vTargetOffset = m_vBlockedPosition;
			}
		}
		// We are no longer blocked, return to the target offset position
		else if (vDifferenceToTargetPos.Mag() > 0.01f && m_vBlockedPosition.IsNonZero())
		{
			// Wait a small amount of time before returning to the un-blocked position to help prevent oscillation back/forth with fidgeting peds
			TUNE_GROUP_FLOAT(DRIVEBY_DEBUG, fTimeBeforeReturningToUnblocked, 0.2f, 0.0f, 10.0f, 0.01f);
			if (m_fBlockedTimer > fTimeBeforeReturningToUnblocked)
			{
				TUNE_GROUP_FLOAT(DRIVEBY_DEBUG, fBlockedLerpOutRate, 0.2f, -10.0f, 10.0f, 0.01f);
				m_vBlockedPosition = Lerp(fBlockedLerpOutRate, m_vBlockedPosition, vTargetOffset);
			}
			else
			{
				m_fBlockedTimer += fwTimer::GetTimeStep();
			}

			if (vTemp.GetY() < m_vBlockedPosition.GetY())
			{
				vTargetOffset = vTemp;
			}
			else
			{
				vTargetOffset = m_vBlockedPosition;
			}
		}
		else
		{
			vTargetOffset = vTemp;
			m_fBlockedTimer = 0.0f;
			m_vBlockedPosition = Vector3::ZeroType;
		}
	}
	else
	{
		vTargetOffset = vTemp;
	}
}

CPed* CTaskAimGunVehicleDriveBy::GetBlockingFirePedFirstPerson(const Vector3 &vStart, const Vector3 &vEnd)
{
	//! if we have no passengers, no need to do this.
	if(!GetPed()->GetVehiclePedInside() || GetPed()->GetVehiclePedInside()->GetSeatManager()->CountPedsInSeats() <= 1)
	{
		return NULL;
	}
	
	//! if we're on a bike or trike, don't do this.
	bool bIsTrike = GetPed()->GetVehiclePedInside()->IsTrike();
	if(GetPed()->GetVehiclePedInside()->InheritsFromBike() || bIsTrike)
	{
		return NULL;
	}

	//! if we already have a ragdoll target, don't do this.
	if(GetPed()->GetPlayerInfo() && GetPed()->GetPlayerInfo()->GetTargeting().GetFreeAimTargetRagdoll())
	{
		return NULL;
	}

	//! don't do test if we can't block against any of these peds.
	//! Note: always block against player peds as we can assume they are friendly if they are in same car as you.
	bool bDoTest = false;
	for(int i = 0; i < GetPed()->GetVehiclePedInside()->GetSeatManager()->GetMaxSeats(); ++i)
	{
		CPed* pPedInSeat = GetPed()->GetVehiclePedInside()->GetSeatManager()->GetPedInSeat(i);
		bool bFriendlyPlayer = pPedInSeat && pPedInSeat->IsPlayer() && GetPed()->GetPedIntelligence()->IsFriendlyWith(*pPedInSeat) && pPedInSeat->GetPedIntelligence()->IsFriendlyWith(* GetPed());
		if (pPedInSeat && (pPedInSeat != GetPed()) && (bFriendlyPlayer || !CanFireAtTarget(GetPed(), pPedInSeat)))
		{
			bDoTest = true;
		}
	}

	if(bDoTest)
	{
		u32 includeFlags = ArchetypeFlags::GTA_RAGDOLL_TYPE;

		TUNE_GROUP_FLOAT(VEHICLE_DRIVE_BY, fFirstPersonDrivebyFireCapsuleWidth, 0.1f, 0.01f, 1.0f, 0.01f);
		TUNE_GROUP_FLOAT(VEHICLE_DRIVE_BY, fFirstPersonDrivebyFireCapsuleLength, 5.0f, 0.01f, 5.0f, 0.01f);

		WorldProbe::CShapeTestHitPoint hitPoint;

		Vector3 vEndAdjusted = vEnd - vStart;
		vEndAdjusted.Normalize();
		vEndAdjusted = vStart + (vEndAdjusted * fFirstPersonDrivebyFireCapsuleLength);

		// Do capsule test instead of probe test
		WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
		WorldProbe::CShapeTestResults probeResults(hitPoint);
		capsuleDesc.SetResultsStructure(&probeResults);
		capsuleDesc.SetCapsule(vStart, vEndAdjusted, fFirstPersonDrivebyFireCapsuleWidth);
		capsuleDesc.SetIsDirected(true);
		capsuleDesc.SetDoInitialSphereCheck(true);
		capsuleDesc.SetIncludeFlags(includeFlags);
		capsuleDesc.SetExcludeEntity(GetPed(), WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS);
		capsuleDesc.SetContext(WorldProbe::LOS_Weapon);

		if(WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc))
		{
			if(hitPoint.GetHitEntity() && hitPoint.GetHitEntity()->GetIsTypePed())
			{
				//! If we can't fire at this target, block. Needs to be in same vehicle as you.
				CPed *pHitPed = static_cast<CPed*>(hitPoint.GetHitEntity());
				
				bool bFriendlyPlayer = pHitPed->IsPlayer() && GetPed()->GetPedIntelligence()->IsFriendlyWith(*pHitPed) && pHitPed->GetPedIntelligence()->IsFriendlyWith(* GetPed());

				if( (pHitPed->GetVehiclePedInside() == GetPed()->GetVehiclePedInside()) && (bFriendlyPlayer || !CanFireAtTarget(GetPed(), pHitPed)))
				{
					return pHitPed;
				}
			}
		}
	}

	return NULL;
}

bool CTaskAimGunVehicleDriveBy::CanUseCloneAndAIBlocking(const CPed* pPed)
{
	TUNE_GROUP_BOOL(DRIVEBY_DEBUG, bEnableCloneAIBlockingIK, true);
	TUNE_GROUP_BOOL(DRIVEBY_DEBUG, bEnableCloneAIBlockingIKForBikes, false);

	if (pPed)
	{
		const CVehicle* pVehicle = pPed->GetVehiclePedInside();
		bool bIsTrike = pVehicle && pVehicle->IsTrike();
		if (pVehicle && (pVehicle->InheritsFromBike() || bIsTrike))
		{
			return bEnableCloneAIBlockingIKForBikes;
		}
	}	

	return bEnableCloneAIBlockingIK;
}

bool CTaskAimGunVehicleDriveBy::CanUseRestrictedDrivebyAnims()
{
	TUNE_GROUP_BOOL(DRIVEBY_DEBUG, bEnableRestrictedDrivebyAnims, true);
	return bEnableRestrictedDrivebyAnims;
}

bool CTaskAimGunVehicleDriveBy::ShouldUseRestrictedDrivebyAnimsOrBlockingIK(const CPed *pPed, bool bCheckCanUseRestrictedAnims)
{
	TUNE_GROUP_BOOL(DRIVEBY_DEBUG, bForceRestrictedAnimsAndIK, false);
	if (bForceRestrictedAnimsAndIK)
	{
		return true;
	}

	TUNE_GROUP_BOOL(DRIVEBY_DEBUG, bUseRestrictedAnimsAndIKIfPassengersInVehicle, true);
	if (bUseRestrictedAnimsAndIKIfPassengersInVehicle)
	{
		// Don't use the restricted anims on the local player in FPS mode
		bool bCanUseAnims = true;
		if (bCheckCanUseRestrictedAnims)
		{
			if (pPed->IsLocalPlayer() && pPed->IsInFirstPersonVehicleCamera())
			{
				bCanUseAnims = false;
			}
		}

		// Enable anims/IK if we have more than one ped in the vehicle
		if (pPed && bCanUseAnims)
		{
			CVehicle *pVehicle = pPed->GetVehiclePedInside();
			if (pVehicle && pVehicle->GetSeatManager() && pVehicle->GetSeatManager()->CountPedsInSeats() > 1)
			{
				return true;
			}
		}
	}

	TUNE_GROUP_BOOL(DRIVEBY_DEBUG, bOnlyUseRestrictedAnimsAndIKIfVehicleContainsFPSPlayer, false);
	if (bOnlyUseRestrictedAnimsAndIKIfVehicleContainsFPSPlayer)
	{
		// If the ped isn't a local player, check through all the vehicle seats to see if we have a player in FPS mode.
		if (pPed && !pPed->IsLocalPlayer())
		{
			CVehicle *pVehicle = pPed->GetVehiclePedInside();
			if (pVehicle)
			{
				int iNumSeats = pVehicle->GetSeatManager() ? pVehicle->GetSeatManager()->GetMaxSeats() : 0;
				for (int i = 0; i < iNumSeats; i++)
				{
					CPed *pPed = pVehicle->GetPedInSeat(i);
					if (pPed && pPed->IsPlayer() && pPed->IsInFirstPersonVehicleCamera())
					{
						// We have a player in the vehicle who is in FPS cam so we need to use the restricted anims to help with clipping issues.
						return true;
					}
				}
			}
		}
	}
	return false;
}

bool CTaskAimGunVehicleDriveBy::ProcessMoveSignals()
{
	CPed* pPed = GetPed();

	bool bForceUpdate = (GetState() == State_Fire) && WantsToFire() && ReadyToFire();
	if(bForceUpdate)
	{
		pPed->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);
	}

	pPed->SetPedResetFlag( CPED_RESET_FLAG_BlockIkWeaponReactions, true);

	if(!CPedAILodManager::ShouldDoFullUpdate(*GetPed()))
	{
		UpdateMoVEParams();
	}

	// Block IK since the head is already animated to the aim direction in the driveby aim animations
	if(!m_bBlockOutsideAimingRange)
	{
		pPed->SetHeadIkBlocked();
	}

	const int iState = GetState();
	switch(iState)
	{
		case State_Intro:
			return true;
		case State_Aim:
			return true;
		case State_Fire:
			Fire_OnProcessMoveSignals();
			return true;
		case State_Outro:
			Outro_OnProcessMoveSignals();
			return true;
		default:
			break;
	}

	return false;
}

aiTask::FSM_Return CTaskAimGunVehicleDriveBy::Start_OnUpdate()
{
	CPed* pPed = GetPed();

	// Look up the driveby info
	m_pVehicleDriveByInfo = CVehicleMetadataMgr::GetVehicleDriveByInfoFromPed(pPed);

	taskFatalAssertf(m_pVehicleDriveByInfo, "No driveby info for vehicle %s", pPed->GetMyVehicle() ? pPed->GetMyVehicle()->GetDebugName() : "mount");

	const CWeaponInfo* pWeaponInfo = GetPedWeaponInfo();
	taskAssert(pWeaponInfo);
	const CVehicleDriveByAnimInfo* pDrivebyClipInfo = CVehicleMetadataMgr::GetDriveByAnimInfoForWeapon(pPed, pWeaponInfo->GetHash());
	if (!pDrivebyClipInfo)
	{
		SetState(State_Finish);
		return FSM_Continue;
	}

	if (pPed->IsNetworkClone())
	{
		u32 desiredWeapon = pPed->GetWeaponManager()->GetEquippedWeaponHash();
		u32 currentWeapon = pPed->GetWeaponManager()->GetEquippedWeapon() ? GetPed()->GetWeaponManager()->GetEquippedWeapon()->GetWeaponInfo()->GetHash() : 0;

		// wait if the ped is holding the wrong weapon object, that does not correspond to the hash of the weapon he should be holding
		if (desiredWeapon != currentWeapon)
		{
			if (GetStateFromNetwork() == State_Outro || GetStateFromNetwork() == State_Finish)
			{
				SetState(State_Finish);
			}

			return FSM_Continue;
		}
	}

	m_eType = pDrivebyClipInfo->GetNetwork();

	if(IsFirstPersonDriveBy())
	{
		if(pWeaponInfo->GetIsUnarmed())
		{
			m_fMinHeadingAngle = m_pVehicleDriveByInfo->GetFirstPersonUnarmedMinAimSweepHeadingAngleDegs() * DtoR;
			m_fMaxHeadingAngle = m_pVehicleDriveByInfo->GetFirstPersonUnarmedMaxAimSweepHeadingAngleDegs() * DtoR;
		}
		else
		{
			m_fMinHeadingAngle = m_pVehicleDriveByInfo->GetFirstPersonMinAimSweepHeadingAngleDegs() * DtoR;
			m_fMaxHeadingAngle = m_pVehicleDriveByInfo->GetFirstPersonMaxAimSweepHeadingAngleDegs() * DtoR;
		}
	}
	else
	{
		m_fMinHeadingAngle = m_pVehicleDriveByInfo->GetMinAimSweepHeadingAngleDegs(pPed) * DtoR;
		m_fMaxHeadingAngle = m_pVehicleDriveByInfo->GetMaxAimSweepHeadingAngleDegs(pPed) * DtoR;
	}

	//This may have been set by being jacked previously B* 1559968
	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_KeepWeaponHolsteredUnlessFired, false);

	if ((m_overrideClipSetId != CLIP_SET_ID_INVALID) && (m_eType == CVehicleDriveByAnimInfo::STD_CAR_DRIVEBY))
	{
		// Ensure the fire clip set is streamed in
		fwClipSet* pClipSet = fwClipSetManager::GetClipSet(pWeaponInfo->GetWeaponClipSetId());	
		if (!pClipSet)
		{
			SetState(State_Finish);
			return FSM_Continue;
		}

		if (pClipSet && pClipSet->IsStreamedIn_DEPRECATED())
		{
			SetState(State_Intro);
		}
	}  
	else
		SetState(State_Intro);


	// Set the reset flag to let the hud know we want to dim the reticule
	GetPed()->SetPedResetFlag( CPED_RESET_FLAG_DimTargetReticule, true );
	return FSM_Continue;
}

void CTaskAimGunVehicleDriveBy::Intro_OnEnter()
{
	CPed* pPed = GetPed();

	if( m_overrideClipSetId != CLIP_SET_ID_INVALID )
	{
		const CVehicleDriveByInfo* pDrivebyInfo = CVehicleMetadataMgr::GetVehicleDriveByInfoFromPed(pPed);
		taskAssert(pDrivebyInfo);
		TUNE_GROUP_FLOAT(VEHICLE_DRIVE_BY, networkBlendInDuration, SLOW_BLEND_DURATION, INSTANT_BLEND_DURATION, REALLY_SLOW_BLEND_DURATION, 0.01f);
		if (!m_MoveNetworkHelper.IsNetworkActive())
		{
			if (pDrivebyInfo->GetUsePedAsCamEntity())
			{
				m_bUseVehicleAsCamEntity = false;
			}
			else
			{
				m_bUseVehicleAsCamEntity = true;
			}

			const CWeaponInfo* pWeaponInfo = GetPedWeaponInfo();
			taskAssert(pWeaponInfo);

			fwMvNetworkDefId networkDefId(CClipNetworkMoveInfo::ms_NetworkTaskAimGunDriveBy);

			switch (m_eType)
			{
				case CVehicleDriveByAnimInfo::STD_CAR_DRIVEBY:		networkDefId = CClipNetworkMoveInfo::ms_NetworkTaskAimGunDriveBy; break;
				case CVehicleDriveByAnimInfo::REAR_HELI_DRIVEBY:	networkDefId = CClipNetworkMoveInfo::ms_NetworkTaskAimGunDriveByHeli; break;
				case CVehicleDriveByAnimInfo::MOUNTED_DRIVEBY:		networkDefId = CClipNetworkMoveInfo::ms_NetworkTaskMountedDriveBy; break;				
				default: break;
			}

			if (!m_MoveNetworkHelper.IsNetworkActive())
				m_MoveNetworkHelper.CreateNetworkPlayer(pPed, networkDefId);
			
			fwMvFilterId filterId = FILTER_ID_INVALID;

			if(IsHidingGunInFirstPersonDriverCamera(pPed, true))
			{
				filterId = FILTER_GRIP_R_ONLY;
			}
			else if (pPed->GetIsParachuting() || pPed->GetPedResetFlag(CPED_RESET_FLAG_IsUsingJetpack))
			{
				filterId = sm_Tunables.m_ParachutingFilterId;
			}
			else if (pPed->GetMyVehicle())
			{
				static const u32 sUpperBodyPlaneHash = ATSTRINGHASH("DRIVEBY_PLANE_FRONT_RIGHT",0xc182463f);

				if (m_pVehicleDriveByInfo && m_pVehicleDriveByInfo->GetUseSpineAdditive())
				{
					bool bIsTrike = pPed->GetMyVehicle()->IsTrike();

					if (pPed->GetMyVehicle()->GetVehicleType() == VEHICLE_TYPE_BICYCLE)
					{
						filterId = sm_Tunables.m_BicycleDrivebyFilterId;
					}
					else if (pPed->GetMyVehicle()->InheritsFromBike() || bIsTrike)
					{
						filterId = sm_Tunables.m_BikeDrivebyFilterId;
					}
				}
				else if(pPed->GetMyVehicle()->GetIsJetSki())
				{
					filterId = sm_Tunables.m_JetskiDrivebyFilterId;
				}
				else if(pDrivebyInfo->GetName().GetHash() == sUpperBodyPlaneHash)
				{
					filterId = fwMvFilterId("Upperbody_filter");
				}
			}

			m_bUsingSecondaryTaskNetwork = (pPed->GetIsParachuting() || pPed->GetPedResetFlag(CPED_RESET_FLAG_IsUsingJetpack));
			if(!m_bUsingSecondaryTaskNetwork)
			{
				float fBlendInDuration = networkBlendInDuration;
				// Instantly blend in anims if switching from TPS to FPS
				if (m_bInstantlyBlendDrivebyAnims)
				{
					fBlendInDuration = 0.0f;
				}

				pPed->GetMovePed().SetTaskNetwork(m_MoveNetworkHelper, fBlendInDuration, CMovePed::Task_None, filterId);
			}
			else
			{
				pPed->GetMovePed().SetSecondaryTaskNetwork(m_MoveNetworkHelper, networkBlendInDuration, CMovePed::Secondary_None, filterId);
			}

			m_MoveNetworkHelper.SetClipSet(m_overrideClipSetId);

			if (m_eType == CVehicleDriveByAnimInfo::STD_CAR_DRIVEBY || m_eType == CVehicleDriveByAnimInfo::REAR_HELI_DRIVEBY)
			{
				if(pPed->IsLocalPlayer() || IsFirstPersonDriveBy())
				{
					m_FireClipSetId = pWeaponInfo->GetAppropriateWeaponClipSetId(pPed);
				}
				else
				{
					m_FireClipSetId = pWeaponInfo->GetWeaponClipSetId();
				}
				m_MoveNetworkHelper.SetClipSet(m_FireClipSetId, ms_FireClipSetVarId);
				
				if (m_bSkipDrivebyIntro)
				{
					// Skip the intro if we've been flagged to do so (B*2031266: ie when CTaskAimGunVehicleDriveBy::ShouldUseRestrictedDrivebyAnimsOrBlockingIK changes causing us to restart in CTaskVehicleGun::ShouldRestartStateDueToCameraSwitch)
					m_bSkipDrivebyIntro = false;
				}
				else
				{
					m_MoveNetworkHelper.SetFlag(true, ms_UseIntroFlagId);
				}
			}

			bool bIsUnarmed = pPed->GetWeaponManager()->GetIsArmedMelee();

			// Force an update of the ped's game state node, so that clone's have the correct weapon info
			if (NetworkInterface::IsGameInProgress() && pPed->IsLocalPlayer() && pPed->GetNetworkObject())
			{
				CNetObjPed* pPedObj = SafeCast(CNetObjPed, pPed->GetNetworkObject());
				pPedObj->RequestForceSendOfGameState();  //this force syncs the game state node which has the weapon info
			}

			bool bUsePitchSweeps = !bIsUnarmed && pPed->GetWeaponManager()->GetWeaponGroup() != WEAPONGROUP_LOUDHAILER;

			const fwClipSet *pClipSet = m_MoveNetworkHelper.GetClipSet();
			if (bIsUnarmed && pClipSet && pClipSet->GetClip(fwMvClipId("sweep_low",0x54B2EA3F)) != NULL)
				bUsePitchSweeps = true; //some unarmed drive-by have pitch assets.

			m_MoveNetworkHelper.SetFlag(bUsePitchSweeps, ms_UsePitchSolutionFlagId);
			m_MoveNetworkHelper.SetFlag(pDrivebyInfo->GetUseThreeAnimIntroOutro(), ms_UseThreeAnimIntroOutroFlagId);
		
			//! DMKH. Need to force move params in intro state so that we can select intro blend weight immediately. This prevents an
			//! issue with spine expression popping.
			m_bForceUpdateMoveParamsForIntro = true;
		}
	}

	if( pPed )
	{
		CPed* myMount = pPed->GetMyMount();
		if( myMount )
		{
#if ENABLE_HORSE
			CHorseComponent* horseComponent = myMount->GetHorseComponent();
			if( horseComponent )
			{
				CReins* pReins = horseComponent->GetReins();
				if( pReins )
				{
					pReins->SetIsRiderAiming( true );

					const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
					u32 weaponGroup = (pWeapon && pWeapon->GetWeaponInfo()) ? pWeapon->GetWeaponInfo()->GetGroup() : 0;
					if (weaponGroup == WEAPONGROUP_MG || weaponGroup== WEAPONGROUP_RIFLE)
						pReins->Detach( myMount );
					else
						pReins->AttachOneHand( pPed );
				}
			}		
#endif	
		}
	}

	m_fHeadingChangeRate = IsFirstPersonDriveBy() ? ms_DefaultMinYawChangeRateFirstPerson : ms_DefaultMinYawChangeRate;
	RequestProcessMoveSignalCalls();
}

aiTask::FSM_Return CTaskAimGunVehicleDriveBy::Intro_OnUpdate()
{

	if (!m_MoveNetworkHelper.IsNetworkActive() && m_overrideClipSetId != CLIP_SET_ID_INVALID)
			return FSM_Continue; //hrmm..

	if( m_overrideClipSetId == CLIP_SET_ID_INVALID ) 
	{
		SetState(State_Aim);		
		return FSM_Continue;
	}
	float fIntroPhase = m_MoveNetworkHelper.GetFloat(ms_IntroTransitionPhaseId);

	//render weapon?
	static const fwMvClipId s_IntroClip("IntroClip",0x4139FEC0);
	const crClip* pClip = m_MoveNetworkHelper.GetClip(s_IntroClip);
	float fRenderPhase = 0.5f;
	float fBlendOutPhase = 0.7f;
	if (pClip)
	{
		CClipEventTags::FindEventPhase<crPropertyAttributeBool>(pClip, CClipEventTags::Object, CClipEventTags::Create, true, fRenderPhase);	
		CClipEventTags::FindEventPhase(pClip, CClipEventTags::BlendOutWithDuration, fBlendOutPhase);	
	}

#if __BANK
	TUNE_GROUP_BOOL(TASK_AIM_GUN_VEHICLE_DRIVE_BY, bOverrideBlendOutPhaseForIntro, false);
	if(bOverrideBlendOutPhaseForIntro)
	{
		TUNE_GROUP_FLOAT(TASK_AIM_GUN_VEHICLE_DRIVE_BY, fOverrideBlendOutPhaseForIntro, 1.0f, 0.0f, 1.0f, 0.01f);
		fBlendOutPhase = fOverrideBlendOutPhaseForIntro;
	}
#endif

	bool bForceRender = false;
	if ( fIntroPhase >= fBlendOutPhase || (fIntroPhase < 0 && GetTimeInState() > 0.2f) )
	{
		m_MoveNetworkHelper.SendRequest( ms_Aim);
		bForceRender = true;
		SetState(State_Aim);
	}
	if ((fIntroPhase >= fRenderPhase || bForceRender))
	{	
		CObject* pWeapon = GetPed()->GetWeaponManager()->GetEquippedWeaponObject();
		if( pWeapon && !pWeapon->GetIsVisible() )
		{
			pWeapon->SetIsVisibleForModule( SETISVISIBLE_MODULE_GAMEPLAY, true, true );
			AI_LOG_WITH_ARGS("[WEAPON_VISIBILITY] - %s ped %s has set weapon %s to visible in CTaskAimGunVehicleDriveBy::Intro_OnUpdate\n", AILogging::GetDynamicEntityIsCloneStringSafe(GetPed()), AILogging::GetDynamicEntityNameSafe(GetPed()), pWeapon->GetWeapon() ? (pWeapon->GetWeapon()->GetWeaponInfo() ? pWeapon->GetWeapon()->GetWeaponInfo()->GetName() : "NULL") : "NULL");
		}
	}	

	// Set the reset flag to let the hud know we want to dim the reticule
	GetPed()->SetPedResetFlag( CPED_RESET_FLAG_DimTargetReticule, true );
	return FSM_Continue;
}

bool CTaskAimGunVehicleDriveBy::GetIsPlayerAimMode(const CPed* pPed) const
{	
	if(pPed->IsPlayer() && 
		pPed->GetMyVehicle() && 
		pPed->GetMyVehicle()->GetDriver() == pPed)
	{
		//! intro is ok.
		if(GetState() == State_Intro)
		{
			return true;
		}

		if(GetIsReadyToFire())
		{
			// minimum aim time.
			if(fwTimer::GetTimeInMilliseconds() < (m_uAimTime + sm_Tunables.m_MinAimTimeMs))
			{
				return true;
			}
	
			// extend aim time if using right stick.
			if(fwTimer::GetTimeInMilliseconds() < (m_uAimTime + sm_Tunables.m_MaxAimTimeOnStickMs))
			{
				bool bUsingAimStick = fwTimer::GetTimeInMilliseconds() < (m_uLastUsingAimStickTime + sm_Tunables.m_AimingOnStickCooldownMs);
				if(bUsingAimStick)
				{
					return true;
				}
			}
		}
	}

	return false;
}

bool CTaskAimGunVehicleDriveBy::ShouldBlendOutAimOutro()
{
	if (GetState() == State_Outro && m_MoveNetworkHelper.IsNetworkActive())
	{
		if (m_fDrivebyOutroBlendOutPhase < 0.0f)
		{
			static fwMvClipId s_Outro0Clip("outro_0");
			const fwMvClipSetId clipsetId = m_MoveNetworkHelper.GetClipSetId();
			const crClip* pClip = fwClipSetManager::GetClip(clipsetId, s_Outro0Clip);
			if (pClip)
			{
				float fUnused;
				CClipEventTags::GetMoveEventStartAndEndPhases(pClip, ms_DrivebyOutroBlendOutId, m_fDrivebyOutroBlendOutPhase, fUnused);
			}
		}

		if (m_fDrivebyOutroBlendOutPhase > 0.0f)
		{
			float fOutroPhase = m_MoveNetworkHelper.GetFloat(ms_OutroTransitionPhaseId);
			return fOutroPhase >= m_fDrivebyOutroBlendOutPhase;
		}
	}
	return false;
}

void CTaskAimGunVehicleDriveBy::RequestSpineAdditiveBlendIn(float fBlendDuration)
{
	if (m_MoveNetworkHelper.IsNetworkActive())
	{
		m_MoveNetworkHelper.SetFlag(true, ms_UseSpineAdditiveFlagId);
		m_MoveNetworkHelper.SetFloat(ms_SpineAdditiveBlendDurationId, fBlendDuration);
	}
}

void CTaskAimGunVehicleDriveBy::RequestSpineAdditiveBlendOut(float fBlendDuration)
{
	if (m_MoveNetworkHelper.IsNetworkActive())
	{
		m_MoveNetworkHelper.SetFlag(false, ms_UseSpineAdditiveFlagId);
		m_MoveNetworkHelper.SetFloat(ms_SpineAdditiveBlendDurationId, fBlendDuration);
	}
}

const bool CTaskAimGunVehicleDriveBy::IsStateValid() const
{
	s32 iState = GetState();
	if ((iState >= State_Start) && (iState <= State_Finish))
		return true;

	return false;
}

bool CTaskAimGunVehicleDriveBy::IsFlippingSomeoneOff(const CPed& rPed)
{
	//Ensure the task is valid.
	const CTaskAimGunVehicleDriveBy* pTask = static_cast<CTaskAimGunVehicleDriveBy *>(
		rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_VEHICLE_DRIVE_BY));
	if(!pTask)
	{
		return false;
	}

	return (pTask->IsFlippingSomeoneOff());
}

void CTaskAimGunVehicleDriveBy::Aim_OnEnter()
{
	if (GetPed()->IsLocalPlayer())
	{
		m_uAimTime = fwTimer::GetTimeInMilliseconds();
		m_uBlockingCheckStartTime = fwTimer::GetTimeInMilliseconds();
	}

	if ((m_overrideClipSetId != CLIP_SET_ID_INVALID) && (m_eType == CVehicleDriveByAnimInfo::STD_CAR_DRIVEBY))
	{
		m_MoveNetworkHelper.SetFlag(false, ms_UseIntroFlagId);
	}
	m_bHolding = false;
	m_fLastHoldingTime = 0;

	UpdateFPMoVEParams();

	RequestProcessMoveSignalCalls();
}

aiTask::FSM_Return CTaskAimGunVehicleDriveBy::Aim_OnUpdate()
{
	CPed* pPed = GetPed();

	if (pPed && !pPed->IsNetworkClone() && GetIsFlagSet(aiTaskFlags::TerminationRequested)
#if __BANK
		&& !ms_bDisableOutro
#endif // __BANK
		)
	{
		taskDebugf3("CTaskAimGunVehicleDriveBy::Aim_OnUpdate--(pPed && !pPed->IsNetworkClone() && GetIsFlagSet(aiTaskFlags::TerminationRequested)-->State_Outro");

		m_UnarmedState = Unarmed_Finish;
		SetState(State_Outro);
		return FSM_Continue;
	}

	if (pPed->IsNetworkClone() && GetStateFromNetwork() == State_Outro)
	{
		taskDebugf3("CTaskAimGunVehicleDriveBy::Aim_OnUpdate--(pPed->IsNetworkClone() && GetStateFromNetwork() == State_Outro)-->State_Outro");

		m_UnarmedState = Unarmed_Finish;
		SetState(State_Outro);
		return FSM_Continue;
	}

	TUNE_GROUP_INT(DRIVEBY_DEBUG, fTimeToStartBlockingCheck, 120, 0, 2000, 1);
	if (fwTimer::GetTimeInMilliseconds() > (m_uBlockingCheckStartTime + fTimeToStartBlockingCheck))
	{
		CheckForBlocking();
	}

	TUNE_GROUP_BOOL(DRIVEBY_DEBUG, forceBlocking, false);
	if (forceBlocking)
	{
		m_bBlocked = true;
	}

	if (pPed->IsLocalPlayer())
	{
		CControl *pControl = pPed->GetControlFromPlayer();
		Vector3 vStickInput(pControl->GetPedLookLeftRight().GetNorm(), -pControl->GetPedLookUpDown().GetNorm(), 0.0f);
		if(vStickInput.Mag2() > 0.01f)
		{
			m_uLastUsingAimStickTime = fwTimer::GetTimeInMilliseconds();
		}
	}

	bool bAimWeapon = false;

	if(IsFirstPersonDriveBy() && pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetEquippedWeapon())
	{
		const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
		Matrix34 mWeapon(Matrix34::IdentityType);	
		Vector3 vStart, vEnd;
		bool bHasFireVector = pPed->IsLocalPlayer() && (pWeapon->CalcFireVecFromAimCamera(pPed, mWeapon, vStart, vEnd, true));
		if(bHasFireVector)
		{
			CPed *pBlockingPed = GetBlockingFirePedFirstPerson(vStart, vEnd);
			if(pBlockingPed)
			{
				pPed->GetPlayerInfo()->GetTargeting().SetFreeAimTargetRagdoll(pBlockingPed);
			}
		}
	}

	// Cache the state of the controller
	WeaponControllerState controllerState = GetWeaponControllerState(pPed);	
	bool bCanFireWeapon = CanFireAtTarget(pPed) && !m_bBlockOutsideAimingRange;
	if(((controllerState == WCS_Fire || controllerState == WCS_FireHeld) || m_iFlags.IsFlagSet(GF_FireAtLeastOnce)) && !pPed->GetPedResetFlag(CPED_RESET_FLAG_BlockWeaponFire))
	{
		if (!m_bBlocked && !m_bJustReloaded && (!m_bCloneOrAIBlockedDueToPlayerFPS || (m_bCloneOrAIBlockedDueToPlayerFPS && m_bBlockedHitPed)) )
		{
			float fIntroPhase = m_MoveNetworkHelper.IsNetworkActive() ? m_MoveNetworkHelper.GetFloat(ms_IntroTransitionPhaseId) : -1.0f;
			if (fIntroPhase < 0 || fIntroPhase  >= 1.0f) //make sure done intro before fire
			{
				float fBlockTransitionPhase = m_MoveNetworkHelper.IsNetworkActive() ? m_MoveNetworkHelper.GetFloat(ms_BlockTransitionPhaseId) : -1.0f;
				if (m_bCheckForBlockTransition)
				{	//creates a 1 frame buffer for a block transition to kick in before trying to fire again
					fBlockTransitionPhase=1.0f;
					m_bCheckForBlockTransition = false;
				}
				static dev_float s_fHeadingDiffToFire = 0.1f;
				if (Abs(m_fCurrentHeading - m_fDesiredHeading) < s_fHeadingDiffToFire && fBlockTransitionPhase < 0.0f)
				{
					const CFiringPattern& firingPattern = pPed->GetPedIntelligence()->GetFiringPattern();
					if (m_bHolding && m_MoveNetworkHelper.IsNetworkActive())
					{
						m_MoveNetworkHelper.SendRequest( ms_HoldEnd );
						m_fLastHoldingTime = GetTimeInState();
						m_bHolding = false;
					}
					else if((firingPattern.ReadyToFire() && firingPattern.WantsToFire()) && (!pPed->IsNetworkClone() || GetStateFromNetwork() == State_Fire))
					{
						const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
						if (!pWeapon || pWeapon->GetState() == CWeapon::STATE_READY)
						{						
							const CTaskCombat* pCombatTask = static_cast<const CTaskCombat*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COMBAT));
							if(!pCombatTask || !pCombatTask->IsHoldingFire())
							{
								if (!bCanFireWeapon && pPed->GetWeaponManager()->GetEquippedWeaponInfo()->GetDamageType()==DAMAGE_TYPE_ELECTRIC) //B* 680492
								{
									if (pWeapon && pWeapon->GetState() != CWeapon::STATE_READY)
										bCanFireWeapon = false;
								}

								if (bCanFireWeapon)
								{
									m_iFlags.SetFlag(GF_FireAtLeastOnce);
									if (pPed->IsLocalPlayer())
										m_uAimTime = fwTimer::GetTimeInMilliseconds();	 //reset aim time each shot

									SetState(State_Fire);
								}
							}
						}
					}
				}

			}			
			bAimWeapon = true;
		}
	}
	else if(controllerState == WCS_Reload)
	{
		if (pPed->GetMyMount()!=NULL)
			SetState(State_Reload);
		else
		{
			SetState(State_VehicleReload);
		}
	}
	else if(( (controllerState == WCS_Aim || GetIsPlayerAimMode(pPed) ) && !m_iFlags.IsFlagSet(GF_DisableAiming)) || m_iFlags.IsFlagSet(GF_AlwaysAiming) || pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ForcedAim) || (pPed->GetPedResetFlag(CPED_RESET_FLAG_IsUsingJetpack)))
	{
		// Keep aiming
		bAimWeapon = true;
	}
	else if (!pPed->IsNetworkClone())
	{
#if __BANK
		if (!ms_bDisableOutro)
#endif // __BANK
		{
			taskDebugf3("CTaskAimGunVehicleDriveBy::Aim_OnUpdate--... !pPed->IsNetworkClone()-->State_Outro");

			// Quit as we are no longer aiming
			m_UnarmedState = Unarmed_Finish;
			SetState(State_Outro);
		}
	}

	m_bJustReloaded = false;

	const CControl* pControl = pPed->GetControlFromPlayer();

	bool bUsingMouseControlOnPC = false;
#if KEYBOARD_MOUSE_SUPPORT
	if(pControl && pControl->WasKeyboardMouseLastKnownSource())
	{
		bUsingMouseControlOnPC = true;
	}
#endif	//KEYBOARD_MOUSE_SUPPORT

	if( (!pPed->IsLocalPlayer() || !CPauseMenu::GetMenuPreference(PREF_ALTERNATE_DRIVEBY)) && !pPed->IsNetworkClone())
	{
		// go to hold pose if aiming without firing for too long (unless the player is using mouse input on PC as we can now aim like passengers - B*2018825)
		if (!m_bHolding && !m_bCloneOrAIBlockedDueToPlayerFPS && bCanFireWeapon && !m_bBlocked && m_MoveNetworkHelper.IsNetworkActive() && (!pPed->IsPlayer() || (pPed->GetMyVehicle() && pPed->GetMyVehicle()->GetDriver() == pPed && !bUsingMouseControlOnPC)))
		{		
			static dev_float s_fMaxAimHoldTime = 1.5f;
			static dev_float s_fMaxAimHoldTimePlayer = 1.0f;
			float fMaxAimHoldTime = pPed->IsPlayer() ? s_fMaxAimHoldTimePlayer : s_fMaxAimHoldTime;
			if ((GetTimeInState() - m_fLastHoldingTime) > fMaxAimHoldTime) 
			{
				const fwClipSet *pClipSet = m_MoveNetworkHelper.GetClipSet();
				if (pClipSet && pClipSet->GetClip(fwMvClipId("sweep_blocked",0x82E95070)) != NULL)
					m_MoveNetworkHelper.SendRequest( ms_Hold);
				m_bHolding = true;
			}
		}
	}

	// Player wants to change weapons
	if( pControl )
	{
		if(!pPed->GetIsParachuting() && !pPed->GetPedResetFlag(CPED_RESET_FLAG_IsUsingJetpack))
		{
			if (m_bEquippedWeaponChanged)
			{
				taskDebugf3("CTaskAimGunVehicleDriveBy::Aim_OnUpdate--!pPed->GetIsParachuting()--(m_bEquippedWeaponChanged) )-->State_Outro");

				m_UnarmedState = Unarmed_Finish;
				SetState(State_Outro);		
			}
			else if (CheckForOutroOnBicycle(controllerState, *pControl))
			{
				taskDebugf3("CTaskAimGunVehicleDriveBy::Aim_OnUpdate--!pPed->GetIsParachuting()--(CheckForOutroOnBicycle(controllerState, *pControl))-->State_Outro");

				m_UnarmedState = Unarmed_Finish;
				SetState(State_Outro);
			}
		}
		else
		{
			if(CEquipWeaponHelper::ShouldPedSwapWeapon(*pPed))
			{
				taskDebugf3("CTaskAimGunVehicleDriveBy::Aim_OnUpdate--(CEquipWeaponHelper::ShouldPedSwapWeapon(*pPed))-->State_Outro");

				m_UnarmedState = Unarmed_Finish;
				SetState(State_Outro);
			}
		}
	} 

	if (!IsFirstPersonDriveBy() &&
		((bAimWeapon && (m_overrideClipSetId != CLIP_SET_ID_INVALID) && (m_eType == CVehicleDriveByAnimInfo::STD_CAR_DRIVEBY || m_eType == CVehicleDriveByAnimInfo::REAR_HELI_DRIVEBY))))
	{
		const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
		crIKSolverArms::eArms arm = (pWeapon && pPed->GetWeaponManager()->GetEquippedWeaponAttachPoint() == CPedEquippedWeapon::AP_LeftHand) ? crIKSolverArms::LEFT_ARM : crIKSolverArms::RIGHT_ARM;
		pPed->GetIkManager().AimWeapon(arm, AIK_USE_FULL_REACH);
	}

	if (m_bBlockOutsideAimingRange)
	{
		ProcessLookAtWhileWaiting(pPed);
		pPed->SetPedResetFlag( CPED_RESET_FLAG_DimTargetReticule, true );
	}

	return FSM_Continue;
}

void CTaskAimGunVehicleDriveBy::ProcessLookAtWhileWaiting(CPed* pPed)
{
	TUNE_GROUP_BOOL(VEHICLE_DRIVE_BY, bUseLookAtWhileBlocked, true);
	if (!bUseLookAtWhileBlocked)
		return;

	if(pPed->IsLocalPlayer() && !pPed->GetIKSettings().IsIKFlagDisabled(IKManagerSolverTypes::ikSolverTypeBodyLook))
	{
		const camThirdPersonCamera* pCamera = camInterface::GetGameplayDirector().GetThirdPersonCamera();
		if(pCamera && camInterface::GetGameplayDirector().IsActiveCamera(pCamera))
		{
			Vector3 vCameraFront = pCamera->GetBaseFront();

			Matrix34 headMatrix;
			pPed->GetBoneMatrix(headMatrix, BONETAG_HEAD);
			Vector3 vPlayerHeadPos = headMatrix.d;

			Vector3 vLookatPos = vPlayerHeadPos + vCameraFront*5.0f;

			TUNE_GROUP_BOOL(VEHICLE_DRIVE_BY, bDrawLookAtWhileBlocked, false);
#if __BANK
			if (bDrawLookAtWhileBlocked)
			{
				grcDebugDraw::Line(vPlayerHeadPos,vLookatPos, Color_blue);
			}
#endif
			pPed->GetIkManager().LookAt(0, NULL, 100, BONETAG_INVALID, &vLookatPos, 0);
		}
	}
}


void CTaskAimGunVehicleDriveBy::Aim_OnExit()
{
	if (m_bHolding && m_MoveNetworkHelper.IsNetworkActive())
	{
		m_MoveNetworkHelper.SendRequest( ms_HoldEnd );
		m_bHolding = false;
	}
}

void CTaskAimGunVehicleDriveBy::Fire_OnEnter()
{
	//! If we get back in fire state havimg delayed fire finished request, then end it now.
	if(m_bDelayFireFinishedRequest)
	{
		m_MoveNetworkHelper.SendRequest(ms_FireFinished);
	}

	if ((m_overrideClipSetId != CLIP_SET_ID_INVALID) && (m_eType == CVehicleDriveByAnimInfo::STD_CAR_DRIVEBY || m_eType == CVehicleDriveByAnimInfo::REAR_HELI_DRIVEBY))
	{
		m_MoveNetworkHelper.SetFlag(true, ms_FiringFlagId);
	}

	m_bFiredOnce = false;
	m_bFiredTwice = false;

	RequestProcessMoveSignalCalls();
	m_bMoveFireLoopOnEnter = false;
	m_bMoveFireLoop1 = false;
	m_bMoveFireLoop2 = false;

	UpdateFPMoVEParams();
}

CTaskAimGunVehicleDriveBy::FSM_Return CTaskAimGunVehicleDriveBy::Fire_OnUpdate()
{
	CPed* pPed = GetPed();
	const CWeaponInfo* pWeaponInfo = pPed->GetWeaponManager()->GetEquippedWeaponInfo();

	bool bAimWeapon = false;
	bool bCanSwitchState = true;

	if ((m_overrideClipSetId != CLIP_SET_ID_INVALID) && (m_eType == CVehicleDriveByAnimInfo::STD_CAR_DRIVEBY || m_eType == CVehicleDriveByAnimInfo::REAR_HELI_DRIVEBY))
	{
		bool bLoopEvent = CheckForFireLoop();
		m_bMoveFireLoop1 = false;
		m_bMoveFireLoop2 = false;

		bool bExitFireLoopAtEnd = pWeaponInfo && !pWeaponInfo->GetAllowEarlyExitFromFireAnimAfterBulletFired();

		bool bHasFired = m_bFiredOnce;

		// B*2685858: Double barrel shotgun: keep in firing state until both shots have been fired.
		if (pWeaponInfo->GetHash() == ATSTRINGHASH("WEAPON_DBSHOTGUN",0xEF951FBB) && bHasFired && !m_bFiredTwice)
		{
			bHasFired = false;
			bCanSwitchState = false;
		}

		if (bHasFired && bExitFireLoopAtEnd && !bLoopEvent && !m_bBlocked)
		{
			m_MoveNetworkHelper.SetFlag(false, ms_FiringFlagId);
			bCanSwitchState = false;

			//This section will make the ped lift the gun up during the fire in liue of some real firing animation, B* 1372758
			if (!m_bHolding && pWeaponInfo->GetIsGun2Handed() && m_MoveNetworkHelper.IsNetworkActive())
			{
				const fwClipSet *pClipSet = m_MoveNetworkHelper.GetClipSet();
				if (pClipSet && pClipSet->GetClip(fwMvClipId("sweep_blocked",0x82E95070)) != NULL)
				{
					m_MoveNetworkHelper.SendRequest( ms_Hold );
					m_bHolding = true;

					//Stop weapon firing anim for sniper rifles. B* 2011931.
					CWeapon* pWeapon = GetPed()->GetWeaponManager()->GetEquippedWeapon();
					if (pWeapon && pWeapon->GetHasFirstPersonScope())
					{
						CMoveNetworkHelper* pWeaponMoveNetworkHelper = pWeapon->GetMoveNetworkHelper();
						if (pWeaponMoveNetworkHelper)
						{
							pWeaponMoveNetworkHelper->SendRequest(CWeapon::ms_InterruptFireId);
						}
					}		
				}
			}			
		}
		else
		{
			m_MoveNetworkHelper.SetFlag(true, ms_FiringFlagId);
		}

		if (bLoopEvent)
		{
			// Refresh recoil filter and clip this frame since the network will be instantly transitioning to another subtree.
			// Otherwise the filter would be invalid for one frame causing the CH (constraint helper DOF) weight to be seen as 0.
			UpdateFPMoVEParams();
		}
	}
	
	if (!pPed->IsNetworkClone() && bCanSwitchState && GetIsFlagSet(aiTaskFlags::TerminationRequested)
#if __BANK
		&& !ms_bDisableOutro
#endif // __BANK
		)
	{
		m_UnarmedState = Unarmed_Finish;
		SetState(State_Outro);
		return FSM_Continue;
	}

	if (pPed->IsNetworkClone() && bCanSwitchState && GetStateFromNetwork() == State_Outro)
	{
		m_UnarmedState = Unarmed_Finish;
		SetState(State_Outro);
		return FSM_Continue;
	}

	if (!CanFireAtTarget(pPed) || m_bBlockOutsideAimingRange)
	{
		SetState(State_Aim);
		bAimWeapon = true;
	}

	if(IsFirstPersonDriveBy() && pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetEquippedWeapon())
	{
		const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
		Matrix34 mWeapon(Matrix34::IdentityType);	
		Vector3 vStart, vEnd;
		bool bHasFireVector = pPed->IsLocalPlayer() && (pWeapon->CalcFireVecFromAimCamera(pPed, mWeapon, vStart, vEnd, true));
		if(bHasFireVector)
		{
			CPed *pBlockingPed = GetBlockingFirePedFirstPerson(vStart, vEnd);
			if(pBlockingPed)
			{
				pPed->GetPlayerInfo()->GetTargeting().SetFreeAimTargetRagdoll(pBlockingPed);
				SetState(State_Aim);
				bAimWeapon = true;
			}
		}
	}

	// Do not change this... it takes vehicle weapons into consideration
	if (pWeaponInfo)
	{
		const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
		if( ( !pWeaponInfo->GetIsVehicleWeapon() && pWeapon && pWeapon->GetIsClipEmpty() ) || 
			(pPed->IsNetworkClone() && GetWeaponControllerState(pPed)==WCS_Reload) )
		{
			if (bCanSwitchState)
			{
				if (pPed->GetMyMount()!=NULL)
				{
					SetState(State_Reload);
				}
				else
				{
					// Clip empty - quit to reload
					SetState(State_VehicleReload);
				}
			}
		} 
		// Check the peds firing pattern to see if we are ready to fire
		else if( WantsToFire( *pPed, *pWeaponInfo ) && !m_bHolding && (m_bFire || 
			(ReadyToFire( *pPed ) && (!pWeapon || pWeapon->GetState()==CWeapon::STATE_READY || pWeapon->GetWeaponInfo()->GetIsAutomatic()))) ) 
		{
			if (!pPed->IsNetworkClone())
			{
				//SP and local

				// Trigger a fire
				if (!m_bFlipping)
					m_bFire = true;
			}
			else
			{
				//MP remote
				if (m_fireCounter < m_targetFireCounter)
					m_bFire = true;
			}
		}
		else if (bCanSwitchState)
		{
			// We have fired our quota of bullets or are no longer firing - go back to aiming
			SetState(State_Aim);
		}

		bAimWeapon = true;
	}

	const CControl* pControl = pPed->GetControlFromPlayer();
	// Player wants to change weapons
	if(pControl)
	{
		WeaponControllerState controllerState = GetWeaponControllerState(pPed);	
		bool bWeaponSwitchRequest = false;
		if (pPed->GetWeaponManager()->GetEquippedVehicleWeaponIndex() == -1 && 
			!pPed->GetWeaponManager()->GetIsArmedMelee() && pPed->GetWeaponManager()->GetEquippedWeaponInfo()->GetUsesAmmo() && pPed->GetInventory()->GetWeaponAmmo(pPed->GetWeaponManager()->GetEquippedWeaponInfo()) == 0 ? true : false)
		{
			bWeaponSwitchRequest = true;
		}
		if (bWeaponSwitchRequest || m_bEquippedWeaponChanged)
		{
			m_UnarmedState = Unarmed_Finish;
			SetState(State_Outro);
		}
		else if (CheckForOutroOnBicycle(controllerState, *pControl))
		{
			m_UnarmedState = Unarmed_Finish;
			SetState(State_Outro);
		}
	}

	CheckForBlocking();
	UpdateUnarmedDriveBy(pPed);

	if ((m_overrideClipSetId != CLIP_SET_ID_INVALID) && (m_eType == CVehicleDriveByAnimInfo::STD_CAR_DRIVEBY || m_eType == CVehicleDriveByAnimInfo::REAR_HELI_DRIVEBY))
	{
		if (!m_bBlocked && bAimWeapon)
		{
			const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
			crIKSolverArms::eArms arm = (pWeapon && pPed->GetWeaponManager()->GetEquippedWeaponAttachPoint() == CPedEquippedWeapon::AP_LeftHand) ? crIKSolverArms::LEFT_ARM : crIKSolverArms::RIGHT_ARM;
			CIkManager& ikManager = pPed->GetIkManager();
			s32 aimFlags = AIK_USE_FULL_REACH;

			m_bFireLoopOnEnter |= m_bMoveFireLoopOnEnter;
			m_bMoveFireLoopOnEnter = false;

			float fRecoilPhase = m_MoveNetworkHelper.IsNetworkActive() ? m_MoveNetworkHelper.GetFloat(ms_RecoilPhaseId) : -1.0f;
			if (fRecoilPhase >= 0.0f || (m_bFire && m_bFireLoopOnEnter))
			{
				const bool bFirstPerson = IsFirstPersonDriveBy();

				if(!bFirstPerson)
				{
					// Apply body recoil
					CIkRequestBodyRecoil bodyRecoilRequest((arm != crIKSolverArms::LEFT_ARM) ? BODYRECOILIK_APPLY_LEFTARMIK : BODYRECOILIK_APPLY_RIGHTARMIK);
					ikManager.Request(bodyRecoilRequest);
				}

#if FPS_MODE_SUPPORTED
				aimFlags |= bFirstPerson ? AIK_APPLY_ALT_SRC_RECOIL : AIK_APPLY_RECOIL;
#else
				aimFlags |= AIK_APPLY_RECOIL;
#endif // FPS_MODE_SUPPORTED
			}

			ikManager.AimWeapon(arm, aimFlags);
		}
	}

	return FSM_Continue;
}

void CTaskAimGunVehicleDriveBy::Fire_OnExit()
{
	CPed* pPed = GetPed();
	// Register the event with the firing pattern
	if(pPed->GetPedIntelligence()->GetFiringPattern().IsBurstFinished())
	{
		pPed->GetPedIntelligence()->GetFiringPattern().ProcessBurstFinished();
	}
	if(m_overrideClipSetId != CLIP_SET_ID_INVALID)
	{
		const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();

		if(pWeapon && ( (pWeapon->GetWeaponInfo()->GetHash() == s_APPistolHash.GetHash()) || pWeapon->GetWeaponInfo()->GetIsAutomatic() ) )
		{
			m_bDelayFireFinishedRequest = true;
		}

		if(!m_bDelayFireFinishedRequest)
		{
			m_MoveNetworkHelper.SendRequest(ms_FireFinished);
		}

		if (m_eType == CVehicleDriveByAnimInfo::STD_CAR_DRIVEBY || m_eType == CVehicleDriveByAnimInfo::REAR_HELI_DRIVEBY)
		{
			m_MoveNetworkHelper.SetFlag(false, ms_FiringFlagId);

			if (m_bFireLoopOnEnter)
			{
				crIKSolverArms::eArms arm = (pWeapon && pPed->GetWeaponManager()->GetEquippedWeaponAttachPoint() == CPedEquippedWeapon::AP_LeftHand) ? crIKSolverArms::LEFT_ARM : crIKSolverArms::RIGHT_ARM;

				// Clear arm IK recoil in case task is blending out
				pPed->GetIkManager().AimWeapon(arm, AIK_USE_FULL_REACH);
			}

			m_bFireLoopOnEnter = false;
		}
	}
	if (pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetEquippedWeapon())
		pPed->GetWeaponManager()->GetEquippedWeapon()->StoppedFiring();

	if (m_bHolding && GetState()!=State_VehicleReload && m_MoveNetworkHelper.IsNetworkActive())
	{
		m_MoveNetworkHelper.SendRequest( ms_HoldEnd );
		m_bHolding = false;
		m_bCheckForBlockTransition =  true;
	}

	m_bFire = false;
	m_bFiredOnce = false;
}

void CTaskAimGunVehicleDriveBy::Fire_OnProcessMoveSignals()
{
	if(!m_MoveNetworkHelper.IsNetworkActive())
	{
		return;
	}

	m_bMoveFireLoopOnEnter	|= m_MoveNetworkHelper.GetBoolean(ms_FireLoopOnEnter);
	m_bMoveFireLoop1		|= m_MoveNetworkHelper.GetBoolean(ms_FireLoop1);
	m_bMoveFireLoop2		|= m_MoveNetworkHelper.GetBoolean(ms_FireLoop2);
}

void CTaskAimGunVehicleDriveBy::Outro_OnEnter()
{
	RequestProcessMoveSignalCalls();
	m_bMoveOutroClipFinished = false;
}

void CTaskAimGunVehicleDriveBy::Outro_OnExit()
{
	CPed* pPed = GetPed();
	// Outro can be triggered by detecting weapon change input so wait until outro finishes and force an update
	// of the ped's game state node, when the weapon changes so that clone's have the correct weapon info in good
	//time for start of this task again if quick switching weapons during aim
	if (NetworkInterface::IsGameInProgress() &&
		pPed->IsLocalPlayer() &&
		pPed->GetNetworkObject())
	{
		NetworkInterface::ForceTaskStateUpdate(*pPed);  //this force syncs the game state node which has the weapon info
	}
}

aiTask::FSM_Return CTaskAimGunVehicleDriveBy::Outro_OnUpdate()
{
	bool stateChanging=false;
	CPed* pPed = GetPed();

	if( m_MoveNetworkHelper.IsNetworkAttached() && GetTimeInState() < 3.0f ) 
	{
		static const fwMvRequestId outroId("Outro",0x5D40B28D);
		m_MoveNetworkHelper.SendRequest( outroId);

		if (m_bMoveOutroClipFinished || (m_MoveNetworkHelper.GetFloat(ms_OutroTransitionPhaseId) >= 1.0f) || ShouldBlendOutAimOutro())
		{

			if (m_UnarmedState == Unarmed_None || m_UnarmedState == Unarmed_Finish)
			{			
				stateChanging = true;
				SetState(State_Finish);
			}
		}
	}
	else 
	{
		stateChanging = true;
		SetState(State_Finish);
	}

	//render weapon?
	const CVehicleSeatAnimInfo* pSeatClipInfo = CVehicleMetadataMgr::GetSeatAnimInfoFromPed(pPed);
	const CSeatOverrideAnimInfo* pOverrideInfo = CTaskMotionInVehicle::GetSeatOverrideAnimInfoFromPed(*pPed);

	bool bWeaponRemainsVisible = (pSeatClipInfo ? pSeatClipInfo->GetWeaponRemainsVisible() : false);
	if(!bWeaponRemainsVisible)
	{
		bWeaponRemainsVisible = GetPed()->GetIsParachuting() || pPed->GetPedResetFlag(CPED_RESET_FLAG_IsUsingJetpack);
	}

	if (m_overrideClipSetId != CLIP_SET_ID_INVALID && !bWeaponRemainsVisible && (!pOverrideInfo || !pOverrideInfo->GetWeaponVisibleAttachedToRightHand()))
	{
		const crClip* pClip = m_MoveNetworkHelper.GetClip(ms_OutroClipId);
		float fStopRenderPhase = 1.0f;
		if (pClip)
		{
			CClipEventTags::FindEventPhase<crPropertyAttributeBool>(pClip, CClipEventTags::Object, CClipEventTags::Destroy, true, fStopRenderPhase);	
		}
		float fOutroPhase = m_MoveNetworkHelper.GetFloat(ms_OutroTransitionPhaseId);
		if (fOutroPhase >= fStopRenderPhase || stateChanging)
		{	
			CObject* pWeapon = pPed->GetWeaponManager()->GetEquippedWeaponObject();
			if ( pWeapon && pWeapon->GetIsVisible())
			{
				pWeapon->SetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY, false, true);
				AI_LOG_WITH_ARGS("[WEAPON_VISIBILITY] - %s ped %s has set weapon %s to invisible in CTaskAimGunVehicleDriveBy::Outro_OnUpdate\n", AILogging::GetDynamicEntityIsCloneStringSafe(pPed), AILogging::GetDynamicEntityNameSafe(pPed), pWeapon->GetWeapon() ? (pWeapon->GetWeapon()->GetWeaponInfo() ? pWeapon->GetWeapon()->GetWeaponInfo()->GetName() : "NULL") : "NULL");
			}
		}	
	}

	UpdateUnarmedDriveBy(pPed);

	//NOTE: We must block the aim camera during drive-by outro only when we are exiting the gun state of the player drive task, as we do not want to inhibit
	//the aim camera during the outro to the 'WaitingToFire' state of the vehicle gun task.
	const CTaskPlayerDrive* pPlayerDriveTask = static_cast<const CTaskPlayerDrive*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PLAYER_DRIVE));
	if (pPlayerDriveTask && pPlayerDriveTask->IsExitingGunState())
	{
		pPed->SetPlayerResetFlag(CPlayerResetFlags::PRF_DISABLE_AIM_CAMERA);
	}

	return FSM_Continue;
}

void CTaskAimGunVehicleDriveBy::Outro_OnProcessMoveSignals()
{
	if(!m_MoveNetworkHelper.IsNetworkActive())
	{
		return;
	}

	m_bMoveOutroClipFinished |= m_MoveNetworkHelper.GetBoolean(ms_OutroClipFinishedId);
}

void CTaskAimGunVehicleDriveBy::Reload_OnEnter()
{
	const CWeaponInfo* pWeaponInfo = GetPed()->GetWeaponManager()->GetEquippedWeaponInfo();
	if (pWeaponInfo)
	{
		// Perhaps this should run as a secondary task aswell?
		const bool bFromIdle = (m_iFlags & GF_ReloadFromIdle) != 0;
		SetNewTask(rage_new CTaskReloadGun(m_weaponControllerType, bFromIdle ? RELOAD_IDLE | RELOAD_SECONDARY : RELOAD_SECONDARY));
		CTask* pGunTask = GetParent(); 
		taskAssert(pGunTask && pGunTask->GetTaskType() == CTaskTypes::TASK_GUN);
		static_cast<CTaskGun*>(pGunTask)->GetGunFlags().SetFlag(GF_PreventWeaponSwapping);
	}
}

CTaskAimGunVehicleDriveBy::FSM_Return CTaskAimGunVehicleDriveBy::Reload_OnUpdate()
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		SetState(State_Aim);
	}

	return FSM_Continue;
}

void CTaskAimGunVehicleDriveBy::Reload_OnExit()
{
	CTask* pGunTask = GetParent(); 
	taskAssert(pGunTask && pGunTask->GetTaskType() == CTaskTypes::TASK_GUN);
	static_cast<CTaskGun*>(pGunTask)->GetGunFlags().ClearFlag(GF_PreventWeaponSwapping);
}

void CTaskAimGunVehicleDriveBy::VehicleReload_OnEnter()
{
	CPed* pPed = GetPed();
	pPed->GetPedIntelligence()->GetFiringPattern().ProcessBurstFinished();
	if (m_MoveNetworkHelper.IsNetworkActive())
	{
		const fwClipSet *pClipSet = m_MoveNetworkHelper.GetClipSet();
		if (pClipSet && pClipSet->GetClip(fwMvClipId("sweep_blocked",0x82E95070)) != NULL)
			m_MoveNetworkHelper.SendRequest( ms_Hold);
	}

	CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
	if (pWeapon)
		pWeapon->StartReload(pPed);
}

CTaskAimGunVehicleDriveBy::FSM_Return CTaskAimGunVehicleDriveBy::VehicleReload_OnUpdate()
{
	const CWeapon* pWeapon = GetPed()->GetWeaponManager()->GetEquippedWeapon();	
	if (pWeapon)
	{
		if ((!pWeapon->GetIsClipEmpty() || pWeapon->GetAmmoTotal() == 0) && pWeapon->GetState()!=CWeapon::STATE_RELOADING)
		{
			SetState(State_Aim);
			m_bJustReloaded = true;
			if (m_MoveNetworkHelper.IsNetworkActive())
				m_MoveNetworkHelper.SendRequest( ms_HoldEnd);
		}		
	}

	const CControl* pControl = GetPed()->GetControlFromPlayer();
	// Player wants to change weapons
	if(pControl)
	{
		if (m_bEquippedWeaponChanged)
		{
			m_UnarmedState = Unarmed_Finish;
			SetState(State_Outro);
		}
	}
	return FSM_Continue;
}

CTaskAimGunVehicleDriveBy::FSM_Return CTaskAimGunVehicleDriveBy::CloneVehicleReload_OnUpdate()
{
	CPed* pPed = GetPed();

#if __ASSERT
	Assertf(pPed->IsNetworkClone(),"Only expect clones here!");
#endif

	if (GetWeaponControllerState(pPed)!=WCS_Reload)
	{
		SetState(State_Aim);
		if (m_MoveNetworkHelper.IsNetworkActive())
			m_MoveNetworkHelper.SendRequest( ms_HoldEnd);
	}		
	return FSM_Continue;
}

aiTask::FSM_Return CTaskAimGunVehicleDriveBy::Finish_OnUpdate()
{
	if (m_MoveNetworkHelper.IsNetworkAttached())
	{
		TUNE_GROUP_FLOAT(VEHICLE_DRIVE_BY, networkBlendOutDuration, SLOW_BLEND_DURATION, INSTANT_BLEND_DURATION, REALLY_SLOW_BLEND_DURATION, 0.01f);
		TUNE_GROUP_FLOAT(VEHICLE_DRIVE_BY, networkBlendOutDurationRestrictedTorso, 0.5f, INSTANT_BLEND_DURATION, 2.0f, 0.01f);
		float fNetworkBlendOutDuration = networkBlendOutDuration;
		const CPed& rPed = *GetPed();
		if (rPed.GetMyVehicle())
		{
			const CVehicleSeatAnimInfo* pSeatAnimInfo = rPed.GetMyVehicle()->GetSeatAnimationInfo(&rPed);
			if (pSeatAnimInfo)
			{
				if (pSeatAnimInfo->GetUseRestrictedTorsoLeanIK())
				{
					fNetworkBlendOutDuration = networkBlendOutDurationRestrictedTorso;
				}

				if (pSeatAnimInfo->GetDuckedClipSet() != CLIP_SET_ID_INVALID)
				{
					const CTaskMotionInAutomobile* pAutoTask = static_cast<const CTaskMotionInAutomobile*>(rPed.GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_IN_AUTOMOBILE));
					if (pAutoTask && pAutoTask->CanBeDucked(true))
					{
						fNetworkBlendOutDuration = CTaskMotionInAutomobile::ms_Tunables.m_DriveByToDuckBlendDuration;
					}
				}
			}
		}
	
		if(!m_bUsingSecondaryTaskNetwork)
		{
			rPed.GetMovePed().ClearTaskNetwork(m_MoveNetworkHelper, fNetworkBlendOutDuration);
		}
		else
		{
			rPed.GetMovePed().ClearSecondaryTaskNetwork(m_MoveNetworkHelper, fNetworkBlendOutDuration);
		}
	}
	return FSM_Quit;
}

bool CTaskAimGunVehicleDriveBy::WantsToFire() const
{
	const CWeaponInfo* pWeaponInfo = GetPed()->GetWeaponManager()->GetEquippedWeaponInfo();
	if(!pWeaponInfo)
	{
		return false;
	}

	return WantsToFire(*GetPed(), *pWeaponInfo);
}

bool CTaskAimGunVehicleDriveBy::WantsToFire(const CPed& ped, const CWeaponInfo& weaponInfo) const
{
	// We want to fire if we're holding fire or we're flagged to fire at least once
	WeaponControllerState controllerState = GetWeaponControllerState(&ped);
	bool bControllerWantsToFire = controllerState == WCS_Fire || (controllerState == WCS_FireHeld && weaponInfo.GetIsAutomatic());
	bool bContinueFireLoopUntilEnd = !weaponInfo.GetAllowEarlyExitFromFireAnimAfterBulletFired() && !(m_bMoveFireLoop1 || m_bMoveFireLoop2) && weaponInfo.GetForceFullFireAnimation();
	
	if (!m_bBlocked && !m_bBlockOutsideAimingRange && (bControllerWantsToFire || bContinueFireLoopUntilEnd || m_iFlags.IsFlagSet(GF_FireAtLeastOnce)) 
		&& ped.GetPedIntelligence()->GetFiringPattern().WantsToFire())
	{
		const CTaskCombat* pCombatTask = static_cast<const CTaskCombat*>(ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COMBAT));
		if(!pCombatTask || !pCombatTask->IsHoldingFire())
		{
			return true;
		}
	}
	return false;
}

bool CTaskAimGunVehicleDriveBy::ReadyToFire() const
{
	return ReadyToFire(*GetPed());
}

bool CTaskAimGunVehicleDriveBy::ReadyToFire(const CPed& ped) const
{
	// We can fire if we're not already firing and our firing pattern allows us to
	if (!m_bFire  
		&& ped.GetPedIntelligence()->GetFiringPattern().ReadyToFire())
	{
		// Default behavior to always return true
		return true;
	}

	return false;
}

void CTaskAimGunVehicleDriveBy::UpdateUnarmedDriveBy(CPed* pPed)
{
	if (IsFlippingSomeoneOff() && !pPed->IsPlayer()) 
	{

		if (m_UnarmedState < Unarmed_Finish && m_target.GetIsValid())								
		{
			Vector3 vTargetPos( Vector3::ZeroType );
			const bool bGotTargetPos = m_target.GetPositionWithFiringOffsets( pPed, vTargetPos );				
			if (bGotTargetPos)
				pPed->GetIkManager().LookAt(0, NULL, 3000, BONETAG_INVALID, &vTargetPos, LF_WHILE_NOT_IN_FOV, 500, 500, CIkManager::IK_LOOKAT_HIGH);
		}


		u32 timeSinceLastStateChange = fwTimer::GetTimeInMilliseconds() - m_uLastUnarmedStateChangeTime;
		static dev_u32 s_uMinInsultTime = 1500;
		static dev_u32 s_uInsultVariance = 1000;
		if (m_UnarmedState == Unarmed_None)
		{
			m_uLastUnarmedStateChangeTime = fwTimer::GetTimeInMilliseconds();
			m_uNextUnarmedStateChangeTime = (u32)fwRandom::GetRandomNumberInRange((int)s_uMinInsultTime, (int)(s_uMinInsultTime + s_uInsultVariance));
			m_UnarmedState = Unarmed_Aim;
		} 
		else if (m_UnarmedState == Unarmed_Aim) 
		{
			if (timeSinceLastStateChange > m_uNextUnarmedStateChangeTime)
			{
				static dev_u32 s_uMinLookTime = 4000;
				static dev_u32 s_uLookVariance = 2000;
				m_uLastUnarmedStateChangeTime = fwTimer::GetTimeInMilliseconds();
				m_uNextUnarmedStateChangeTime = (u32)fwRandom::GetRandomNumberInRange((int)s_uMinLookTime, (int)(s_uMinLookTime + s_uLookVariance));
				m_UnarmedState = Unarmed_Look;
				m_fOutroHeadingBlend = -1.0f;
				SetState(State_Outro);
			}
		} 
		else if (m_UnarmedState == Unarmed_Look) 
		{
			if (timeSinceLastStateChange > m_uNextUnarmedStateChangeTime)
			{
				m_uLastUnarmedStateChangeTime = fwTimer::GetTimeInMilliseconds();
				m_UnarmedState = Unarmed_Aim;
				
				m_uNextUnarmedStateChangeTime = (u32)fwRandom::GetRandomNumberInRange((int)s_uMinInsultTime, (int)(s_uMinInsultTime + s_uInsultVariance));
				m_MoveNetworkHelper.SetFlag(true, ms_UseIntroFlagId);
				m_MoveNetworkHelper.SendRequest(ms_Intro);
				m_fIntroHeadingBlend = -1.0f;
				SetState(State_Intro);
			}
		}
	}
}


void CTaskAimGunVehicleDriveBy::UpdateMoVEParams()
{
	if (m_MoveNetworkHelper.IsNetworkActive())
	{
		// Calculate and send MoVE parameters
		CPed* pPed = GetPed();

        // if this a network clone task running out of scope then we don't need to update here
        if(pPed && pPed->IsNetworkClone() && !IsInScope(pPed))
            return;

		if (!pPed->GetMyVehicle() && !pPed->GetMyMount() && !pPed->GetIsParachuting() && !pPed->GetPedResetFlag(CPED_RESET_FLAG_IsUsingJetpack)) //this started being possible 10-16-12 somehow
			return;

		const float fSweepPitchMin = -QUARTER_PI;
		const float fSweepPitchMax = QUARTER_PI;		

		//////////////////////////////////////
		// Calculate and send MoVE parameters
		//////////////////////////////////////

		// Calculate the aim vector (this determines the heading and pitch angles to point the clips in the correct direction)
		Vector3 vStart(Vector3::ZeroType);
		Vector3 vEnd(Vector3::ZeroType);
		bool bComputeParams = false;
		if (GetParent()->GetParent() && GetParent()->GetParent()->GetTaskType() == CTaskTypes::TASK_VEHICLE_GUN)
		{
			CTaskVehicleGun* pTaskVehicleGun = static_cast<CTaskVehicleGun*>(GetParent()->GetParent());
			bComputeParams = pTaskVehicleGun->GetAimVectors(vStart, vEnd);
			if (pPed->IsPlayer())
			{
				if (NetworkInterface::IsGameInProgress())
				{
					if (pPed->IsNetworkClone())
					{
						pTaskVehicleGun->GetTargetPosition(pPed,vEnd);

						float fNeckZ = vStart.z;
						vStart = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());				
						vStart.z = fNeckZ;
					}
					else if (bComputeParams)
					{
						pTaskVehicleGun->SetTargetPosition(vEnd);
					}
				}		
			}
			else
			{

				//AI need a more stable start point and End point
				Vector3 vTargetPos( Vector3::ZeroType );
				if (m_target.GetPositionWithFiringOffsets( pPed, vTargetPos ))
					vEnd = vTargetPos;

				float fNeckZ = vStart.z;
				vStart = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());				
				vStart.z = fNeckZ;
				
			}
		}

		if (GetState() == State_Outro)
		{
			if (m_fCurrentHeading<0.0f)
				m_MoveNetworkHelper.SetFloat(ms_OutroHeadingId, 0.5f);
			else
				m_MoveNetworkHelper.SetFloat(ms_OutroHeadingId, m_fCurrentHeading);

			float fOutroChangeRate;
			if(IsFirstPersonDriveBy())
			{
				fOutroChangeRate = ms_DefaultOutroRateFirstPerson;
			}
			else
			{
				fOutroChangeRate = pPed->IsNetworkClone() ? ms_DefaultRemoteOutroRate : ms_DefaultOutroRate;		
			}

			m_MoveNetworkHelper.SetFloat(ms_OutroRateId, pPed->IsNetworkClone() ? ms_DefaultRemoteOutroRate : ms_DefaultOutroRate);
		}
		else if (bComputeParams) 
		{
			Assert(vEnd.FiniteElements());

			///////////////////////////////
			//// COMPUTE AIM YAW SIGNAL ///
			///////////////////////////////

			// Compute desired yaw angle value
			float fDesiredPitch = 0.0f;
			float fDesiredYaw = CTaskAimGun::CalculateDesiredYaw(pPed, vStart, vEnd);
			
			bool bUsingProceduralAim = false;
			
			const CWeapon* pWeapon = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeapon() : NULL;
			s32 iDictIndex = -1;
			u32 iClipHash = 0;
			eHierarchyId iHierarchyId;

			CTaskPlayerDrive* pDriveTask = static_cast<CTaskPlayerDrive*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PLAYER_DRIVE));
			bool bSmashedWindow = pDriveTask && pDriveTask->GetSmashedWindow();

			if(pWeapon && 
				pWeapon->GetWeaponInfo()->GetIsUnarmed() && 
				CTaskSmashCarWindow::GetWindowToSmash(pPed,false,iClipHash,iDictIndex,iHierarchyId, false) && 
				!bSmashedWindow)
			{
				float fMin = m_pVehicleDriveByInfo ? m_pVehicleDriveByInfo->GetMinUnarmedDrivebyYawIfWindowRolledUp() : -1.0f;
				float fMax = m_pVehicleDriveByInfo ? m_pVehicleDriveByInfo->GetMaxUnarmedDrivebyYawIfWindowRolledUp() :  1.0f;
		
				fDesiredYaw = Clamp(fDesiredYaw, fMin, fMax);
		
				m_fMinFPSIKYaw = fMin;
				m_fMaxFPSIKYaw = fMax;

				// Map the desired target heading to 0-1, depending on the range of the sweep
				m_fDesiredHeading = ComputeYawSignalFromDesired(fDesiredYaw, m_fMinHeadingAngle, m_fMaxHeadingAngle);
			}
			else
			{
				// Map the desired target heading to 0-1, depending on the range of the sweep
				m_fDesiredHeading = ComputeYawSignalFromDesired(fDesiredYaw, m_fMinHeadingAngle, m_fMaxHeadingAngle);

				m_fMinFPSIKYaw = -PI;
				m_fMaxFPSIKYaw =  PI;

				//Fine tune aim
				bUsingProceduralAim = FineTuneAim(pPed, vStart, vEnd, fDesiredYaw, fDesiredPitch);	

				/*if(m_bBlockOutsideAimingRange)
				{
					if( (m_fDesiredHeading < 0.9f)  )
					{
						m_fDesiredHeading = 0.0f;
					}
					else
					{
						m_fDesiredHeading = 1.0f;
					}
				}*/
			}

			// The desired heading is smoothed so it doesn't jump too much in a timestep	
			float deltaX = Min(1.0f, 2.0f*((m_fDesiredHeading>m_fCurrentHeading) ? m_fDesiredHeading-m_fCurrentHeading : m_fCurrentHeading-m_fDesiredHeading));
			float fminyawchangerate = IsFirstPersonDriveBy() ? ms_DefaultMinYawChangeRateFirstPerson : ms_DefaultMinYawChangeRate;
			float fmaxyawchangerate = IsFirstPersonDriveBy() ? ms_DefaultMaxYawChangeRateFirstPerson : ms_DefaultMaxYawChangeRate;

			// B*2087433: Increase the lerp rate if the in-vehicle context has been overriden by script (and we're not in first person). 
			// Fixes issues with camera clipping the peds head in certain missions.
			CTaskMotionBase* pTask = GetPed()->GetPrimaryMotionTask();
			const u32 uOverrideInVehicleContext = pTask ? pTask->GetOverrideInVehicleContextHash() : 0;
			if (!IsFirstPersonDriveBy() && uOverrideInVehicleContext != 0)
			{
				fminyawchangerate = ms_DefaultMinYawChangeRateOverriddenContext;
				fmaxyawchangerate = ms_DefaultMaxYawChangeRateOverriddenContext;
			}

			if (pPed->IsNetworkClone())
			{
				fminyawchangerate *= ms_DefaultRemoteMinYawChangeRateScalar;
				fmaxyawchangerate *= ms_DefaultRemoteMaxYawChangeRateScalar;
			}
			float goalAimApproachRate = Lerp(deltaX, fminyawchangerate, fmaxyawchangerate);
			Approach(m_fHeadingChangeRate, goalAimApproachRate, 1.0f, fwTimer::GetTimeStep());				
			
			// The desired heading is smoothed so it doesn't jump too much in a timestep
			static dev_float s_fBlockedHeadingChangeRate = 2.0f;
			static dev_float s_fBlockedOutsideAimRangeHeadingChangeRate = 3.0f;
			float fRateScalar = 1.0f; 
			if (m_bFlipping)
			{
				fRateScalar = s_fBlockedHeadingChangeRate;
			} 
			else if(m_bBlockOutsideAimingRange)
			{
				fRateScalar = s_fBlockedOutsideAimRangeHeadingChangeRate;
			}
			m_fCurrentHeading = CTaskAimGun::SmoothInput(m_fCurrentHeading, m_fDesiredHeading, m_fHeadingChangeRate*fRateScalar);

			if (m_bFlipping)
			{
				float fHeadingDelta = fabs(m_fDesiredHeading - m_fCurrentHeading);
				static dev_float s_fMinFlipDelta = 0.01f;
				if (fHeadingDelta < s_fMinFlipDelta)
				{
					m_MoveNetworkHelper.SendRequest(ms_FlipEnd);		
					m_bFlipping = false;
				}
			}
			//if flipping via animation, can pop the phase
			float flipPhase = m_MoveNetworkHelper.GetFloat(ms_FlipPhaseId);
			if (flipPhase>0.4f && flipPhase<0.6f)
			{
				m_fCurrentHeading = m_fDesiredHeading;
			}
			else if (pPed->IsLocalPlayer() && GetState()==State_Intro)
			{
				//Letting local players still in intro pop it as well, as any large changes are probably a camera cut. B* 1485831
				m_fCurrentHeading = m_fDesiredHeading;
				m_fIntroHeadingBlend = -1.0f;
			}

			// Send the heading signal
			//Displayf(" T = %f, Rate = %f, Goal = %f, Desired = %f, goal = %f", deltaX, m_fHeadingChangeRate, goalAimApproachRate, m_fCurrentHeading, m_fDesiredHeading);			
			m_MoveNetworkHelper.SetFloat(ms_HeadingId, m_fCurrentHeading);
			m_fLastYaw = m_fCurrentHeading;

			//////////////////////////////////
			//// COMPUTE AIM PITCH SIGNAL ////
			//////////////////////////////////

			if (!bUsingProceduralAim)
			{
				// Compute desired pitch angle value	
#if __BANK
				ms_debugDraw.AddLine(RCC_VEC3V(vStart), RCC_VEC3V(vEnd), Color_red, 1);		
#endif
				bool bUntransformRelativeToPed = pPed->GetIsParachuting() || pPed->GetPedResetFlag(CPED_RESET_FLAG_IsUsingJetpack) || (pPed->GetIsInVehicle() && pPed->GetVehiclePedInside()->GetIsAircraft());
				fDesiredPitch = CTaskAimGun::CalculateDesiredPitch(pPed, vStart, vEnd, bUntransformRelativeToPed);			
				m_fLastPitch = fDesiredPitch;

				// Map the angle to the range 0.0->1.0
				m_fDesiredPitch = CTaskAimGun::ConvertRadianToSignal(fDesiredPitch, fSweepPitchMin, fSweepPitchMax, false);			
			}

			TUNE_GROUP_FLOAT(DRIVEBY_HACK, MAX_DESIRED_PITCH, 0.5f, 0.0f, 1.0f, 0.01f);
			TUNE_GROUP_FLOAT(DRIVEBY_HACK, MIN_DESIRED_PITCH, 0.5f, 0.0f, 1.0f, 0.01f);

			if (pPed->GetMyVehicle() && pPed->GetMyVehicle()->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_USE_RESTRICTED_DRIVEBY_HEIGHT) && m_fDesiredPitch > MAX_DESIRED_PITCH)
			{
				m_fDesiredPitch = MAX_DESIRED_PITCH;
			}
			else if (pPed->GetMyVehicle() && pPed->GetMyVehicle()->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_USE_RESTRICTED_DRIVEBY_HEIGHT_HIGH) && m_fDesiredPitch < MIN_DESIRED_PITCH)
			{
				m_fDesiredPitch = MIN_DESIRED_PITCH;
			}
			else if (pPed->GetMyVehicle() && pPed->GetMyVehicle()->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_USE_RESTRICTED_DRIVEBY_HEIGHT_MID_ONLY) && m_fDesiredPitch != MAX_DESIRED_PITCH)
			{
				m_fDesiredPitch = MAX_DESIRED_PITCH;
			}
	
			// B*3029848: Force the gun into a mid/high sweep if we're a remote passenger on the same bike as a local driver in first person (to prevent gun clipping through local player's camera)
			TUNE_GROUP_FLOAT(DRIVEBY_HACK, MIN_DESIRED_PITCH_FOR_REMOTE_BIKE_PASSENGER, 0.75f, 0.0f, 1.0f, 0.01f);
			TUNE_GROUP_FLOAT(DRIVEBY_HACK, HEADING_TO_LIMIT_PITCH_FOR_REMOTE_BIKE_PASSENGER, 0.1f, 0.0f, 1.0f, 0.01f);
			if (pPed->IsNetworkClone() && m_fDesiredPitch < MIN_DESIRED_PITCH_FOR_REMOTE_BIKE_PASSENGER && abs(m_fCurrentHeading - 0.5f) <= HEADING_TO_LIMIT_PITCH_FOR_REMOTE_BIKE_PASSENGER)
			{
				const CVehicle* pVehicle = pPed->GetMyVehicle();
				const CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();
				if (pLocalPlayer && pLocalPlayer->IsInFirstPersonVehicleCamera() && pVehicle && pVehicle->GetVehicleType() == VEHICLE_TYPE_BIKE && pPed->GetMyVehicle()->GetDriver() == pLocalPlayer)
				{
					m_fDesiredPitch = MIN_DESIRED_PITCH_FOR_REMOTE_BIKE_PASSENGER;
				}
			}
		
			// Compute the final signal value
			float deltaY = Min(1.0f, 2.0f*((m_fDesiredPitch>m_fCurrentPitch) ? m_fDesiredPitch-m_fCurrentPitch : m_fCurrentPitch-m_fDesiredPitch));
			float fmaxPitchChangeRate;
			if(IsFirstPersonDriveBy())
			{
				fmaxPitchChangeRate = ms_DefaultMaxPitchChangeRateFirstPerson;
			}
			else
			{
				fmaxPitchChangeRate = pPed->IsNetworkClone() ? ms_DefaultRemoteMaxPitchChangeRate : ms_DefaultMaxPitchChangeRate;		
			}
				
			goalAimApproachRate = Lerp(deltaY, 0.0f, fmaxPitchChangeRate);
			Approach(m_fPitchChangeRate, goalAimApproachRate, 1.0f, fwTimer::GetTimeStep());
			m_fCurrentPitch = CTaskAimGun::SmoothInput(m_fCurrentPitch, m_fDesiredPitch, goalAimApproachRate);

			// Send the signal
			m_MoveNetworkHelper.SetFloat(ms_PitchId, m_fCurrentPitch);
			m_fLastPitch = m_fCurrentPitch;

			// Calculate and send the intro blend param
			if (GetState() <= State_Intro)
			{
				float fIntroChangeRate;
				if(IsFirstPersonDriveBy())
				{
					fIntroChangeRate = ms_DefaultIntroRateFirstPerson;
				}
				else
				{
					fIntroChangeRate = pPed->IsNetworkClone() ? ms_DefaultRemoteIntroRate : ms_DefaultIntroRate;		
				}

				// Send intro rate param
				m_MoveNetworkHelper.SetFloat(ms_IntroRateId, fIntroChangeRate);
				CalculateIntroBlendParam(fDesiredYaw);
			}

#if __BANK
			if (ms_bRenderVehicleDriveByDebug)
				grcDebugDraw::AddDebugOutput(Color_orange, "Drive By Heading : %.4f", m_fCurrentHeading);

			m_fDebugDesiredYaw = fDesiredYaw;
			m_fDebugDesiredPitch = fDesiredPitch;
			m_vStartPos = vStart;
			m_vEndPos = vEnd;
#endif // __BANK
		}
	}
}

void CTaskAimGunVehicleDriveBy::UpdateFPMoVEParams()
{
	if(!m_MoveNetworkHelper.IsNetworkActive())
	{
		Warningf("CTaskAimGunVehicleDriveBy::UpdateFPMoVEParams--!m_MoveNetworkHelper.IsNetworkActive()-->return");
		return;
	}

	bool bFirstPersonMode = false;
	bool bFirstPersonModeIK = false;

#if FPS_MODE_SUPPORTED
	const CWeaponInfo* pWeaponInfo = GetPedWeaponInfo();
	bFirstPersonMode = IsFirstPersonDriveBy();
	bFirstPersonModeIK = bFirstPersonMode && pWeaponInfo && pWeaponInfo->GetUseFPSAimIK();
#endif // FPS_MODE_SUPPORTED

	if(IsFirstPersonDriveBy())
	{
		//! set grip anims
		fwMvClipId gripClipId = m_pVehicleDriveByInfo && m_pVehicleDriveByInfo->GetLeftHandedFirstPersonAnims() ? sm_Tunables.m_FirstPersonGripLeftClipId : sm_Tunables.m_FirstPersonGripRightClipId;
		crClip* pGripClip = fwClipSetManager::GetClip(m_FireClipSetId, gripClipId);
		if(pGripClip)
		{
			m_MoveNetworkHelper.SetClip(pGripClip, ms_FirstPersonGripClipId);
		}

		//! set fire anims
		fwMvClipId fireClipId = m_pVehicleDriveByInfo && m_pVehicleDriveByInfo->GetLeftHandedFirstPersonAnims() ? sm_Tunables.m_FirstPersonFireLeftClipId : sm_Tunables.m_FirstPersonFireRightClipId;
		crClip* pFireClip = fwClipSetManager::GetClip(m_FireClipSetId, fireClipId);
		if(pFireClip)
		{
			m_MoveNetworkHelper.SetClip(pFireClip, ms_FirstPersonFireRecoilClipId);
		}
	}

	m_MoveNetworkHelper.SetFlag(bFirstPersonMode, ms_FirstPersonMode);
	m_MoveNetworkHelper.SetFlag(bFirstPersonModeIK, ms_FirstPersonModeIK);

	static fwMvFilterId RightHandRecoilFilter("IK_Recoil_R_FPS_Driveby_filter",0x1890179e);
	static fwMvFilterId LeftHandRecoilFilter("IK_Recoil_L_FPS_Driveby_filter",0xccc5551f);
	static fwMvFilterId RightHandGripFilter("Grip_R_Only_NO_IK_filter",0xfdbff2aa);
	static fwMvFilterId LeftHandGripFilter("Grip_L_Only_NO_IK_filter",0xb87d64a6);

	fwMvFilterId recoilFilter;
	fwMvFilterId gripFilter;
	if(m_pVehicleDriveByInfo && m_pVehicleDriveByInfo->GetLeftHandedFirstPersonAnims())
	{
		recoilFilter = LeftHandRecoilFilter;
		gripFilter = LeftHandGripFilter;
	}
	else
	{
		recoilFilter = RightHandRecoilFilter;
		gripFilter = RightHandGripFilter;
	}

	crFrameFilter* pFilterRecoil = g_FrameFilterDictionaryStore.FindFrameFilter(recoilFilter);
	taskAssert(pFilterRecoil);
	m_MoveNetworkHelper.SetFilter(pFilterRecoil, ms_FirstPersonRecoilFilter);

	crFrameFilter* pFilterGrip = g_FrameFilterDictionaryStore.FindFrameFilter(gripFilter);
	taskAssert(pFilterGrip);
	m_MoveNetworkHelper.SetFilter(pFilterGrip, ms_FirstPersonGripFilter);
}

bool CTaskAimGunVehicleDriveBy::FineTuneAim(CPed* pPed, Vector3& vStart, Vector3& vEnd, float& fDesiredHeading, float& fDesiredPitch) 
{
	TUNE_GROUP_BOOL(VEHICLE_DRIVE_BY, useFineTuneAim, true);

	bool bBlockedForFineTune = m_bBlocked && !m_bBlockOutsideAimingRange;
	const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();	
	const CObject* pWeaponObj = pPed->GetWeaponManager()->GetEquippedWeaponObject();	
	if (useFineTuneAim && pWeapon && pWeapon->GetWeaponInfo() && !IsFirstPersonDriveBy() && pWeaponObj && !m_bHolding && !bBlockedForFineTune && !m_bFlipping)
	{		
		float fIntroPhase = m_MoveNetworkHelper.GetFloat(ms_IntroTransitionPhaseId);
		float fIntroToAimPhase = m_MoveNetworkHelper.GetFloat(ms_IntroToAimPhaseId);
		float fBlockTransitionPhase = m_MoveNetworkHelper.GetFloat(ms_BlockTransitionPhaseId);	
		static dev_float sf_BlockTransitionCorrectionPhase = 0.5f;	
		bool bPitchOnly = false;
		if ( (fIntroPhase >= 0.5f || fBlockTransitionPhase>=sf_BlockTransitionCorrectionPhase) && !GetPed()->GetIsParachuting() && !pPed->GetPedResetFlag(CPED_RESET_FLAG_IsUsingJetpack))		
		{
			//During intro always undershoot until procedural aim can correct itself, or aim anims can be authored differently
			static dev_float sf_UndershootScalar = 0.75f;		
			if (m_fDesiredHeading > 0.5f)
				m_fDesiredHeading = ((m_fDesiredHeading-0.5f)*sf_UndershootScalar) + 0.5f;
			else
				m_fDesiredHeading = 0.5f - ((0.5f - m_fDesiredHeading)*sf_UndershootScalar);			

			if (fIntroToAimPhase < 0.0f || fIntroPhase < 1.0f)
				bPitchOnly = true;
			//return false;
		}
		if (GetState() != State_Aim && GetState() != State_Fire)
		{
			return false;
		}
			
		if (fBlockTransitionPhase < 0.0f)
		{
			Vector3 vTargetVector = vEnd - vStart;
			vTargetVector.Normalize();
			Vector3 vTargetVectorRotated = VEC3V_TO_VECTOR3(GetPed()->GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V(vTargetVector)));
			Vector3 vTargetVectorRotatedFlat = vTargetVectorRotated;
			vTargetVectorRotatedFlat.z = 0.0f;
			vTargetVectorRotatedFlat.Normalize();

			Vector3 vAimStart;
			Vector3 vAimEnd;
			pWeapon->CalcFireVecFromGunOrientation(GetPed(), RCC_MATRIX34(pWeaponObj->GetMatrixRef()), vAimStart, vAimEnd);
			Vector3 vAimVector = vAimEnd - vAimStart;
			vAimVector.Normalize();
			Vector3 vAimVectorRotated = VEC3V_TO_VECTOR3(GetPed()->GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V(vAimVector)));
			Vector3 vAimVectorRotatedFlat = vAimVectorRotated;
			vAimVectorRotatedFlat.z = 0.0f;
			vAimVectorRotatedFlat.Normalize();

			float fResult;

			static dev_float s_fPistolGunUpLimit = 0.25f;
			static dev_float s_fRecoilBlockUpdatePhase = 0.75f;
			float fRecoilPhase = m_MoveNetworkHelper.GetFloat(ms_RecoilPhaseId);
			bool bUseLastHeading = false;
			if (!GetPed()->GetIsParachuting() && !pPed->GetPedResetFlag(CPED_RESET_FLAG_IsUsingJetpack) && (pWeaponObj->GetMatrixRef().GetCol2().GetZf() <= s_fPistolGunUpLimit) && !pWeapon->GetWeaponInfo()->GetIsAutomatic() && fRecoilPhase >= s_fRecoilBlockUpdatePhase)
				bUseLastHeading = true;

			float deltaX = vAimVectorRotatedFlat.FastAngle(vTargetVectorRotatedFlat);
#if __BANK
			TUNE_GROUP_BOOL(VEHICLE_DRIVE_BY, bRenderFlatFineTuneAngles, false);
			Vec3V vDebugStart = pPed->GetTransform().GetPosition();
			vDebugStart.SetZf(vDebugStart.GetZf() + 1.0f);
			Vec3V vDebugEnd;

			if (!bRenderFlatFineTuneAngles)
			{
				//! gun orientation.
				vDebugEnd = Add(vDebugStart, RCC_VEC3V(vAimVector));
				ms_debugDraw.AddLine(vDebugStart, vDebugEnd, Color_red, 1);				

				//! aim direction (from camera).
				vDebugEnd = Add(vDebugStart, RCC_VEC3V(vTargetVector));
				ms_debugDraw.AddLine(vDebugStart, vDebugEnd, bPitchOnly ? Color_DarkGreen : Color_green, 1);

				//! to reticule.
				vDebugEnd = RCC_VEC3V(vEnd);
				ms_debugDraw.AddLine(vDebugStart, vDebugEnd, Color_yellow, 1);
				//Displayf("Delta unmodified: %f", deltaX*RtoD);
			}
#endif

			if (!bPitchOnly)
			{
				if (!bUseLastHeading)
				{
					static const float SIXTEENTH_PI = (PI * 0.0625f);
					float fMaxPerFrameAimCorrection = pPed->IsNetworkClone() ? SIXTEENTH_PI : QUARTER_PI;
					deltaX=Min(fMaxPerFrameAimCorrection, 5.0f*deltaX*deltaX);
					Vector3 vRight(vAimVectorRotatedFlat);
#if __BANK
					if (bRenderFlatFineTuneAngles)
					{
						//! flat camera direction
						vDebugEnd = Add(vDebugStart, RCC_VEC3V(vTargetVectorRotatedFlat));
						ms_debugDraw.AddLine(vDebugStart, vDebugEnd, Color_LimeGreen, 1);				

						//! flat camera direction
						vDebugEnd = Add(vDebugStart, RCC_VEC3V(vAimVectorRotatedFlat));
						ms_debugDraw.AddLine(vDebugStart, vDebugEnd, Color_OrangeRed, 1);			
				
						//! rotated gun
						vDebugEnd = Add(vDebugStart, RCC_VEC3V(vRight));
						ms_debugDraw.AddLine(vDebugStart, vDebugEnd, Color_orange, 1);				
					}
#endif
					vRight.RotateZ(HALF_PI);
					float vDot = vTargetVectorRotatedFlat.Dot(vRight);
					TUNE_GROUP_FLOAT(VEHICLE_DRIVE_BY, fFineTuneBlockFlipAgainThresh, -0.2f, -1.0f, 1.0f, 0.1f);
					if (vDot>0)
					{
						if (m_bBlockOutsideAimingRange)
						{
							if (!m_bFlippedForBlockedInFineTuneLastUpdate)
								m_bFlippedForBlockedInFineTuneLastUpdate = true;
						}

						deltaX=-deltaX;						
					}
					else if (m_bBlockOutsideAimingRange && m_bFlippedForBlockedInFineTuneLastUpdate)
					{					
						if (vDot > fFineTuneBlockFlipAgainThresh)
						{
							deltaX=-deltaX;
						}
						else
						{
							m_bFlippedForBlockedInFineTuneLastUpdate = false;
						}
					}

					float deltaT = deltaX/(m_fMaxHeadingAngle-m_fMinHeadingAngle);
					fResult = Clamp(m_fLastYaw + deltaT, 0.0f, 1.0f);//m_fMinHeadingAngle, m_fMaxHeadingAngle);		
					fDesiredHeading = m_fMinHeadingAngle + (fResult * m_fMaxHeadingAngle-m_fMinHeadingAngle);
					//Displayf("DeltaT final: %f, Result: %f, fDesiredHeading: %f", deltaT, fResult, fDesiredHeading*RtoD);					
					//m_fDesiredHeading = CTaskAimGun::ConvertRadianToSignal(fResult, m_fMinHeadingAngle, m_fMaxHeadingAngle, false);
				}
				else
				{
					fResult = m_fLastYaw;
				}
				if (fIntroToAimPhase >= 0.0f)
					m_fDesiredHeading = Lerp(fIntroToAimPhase, m_fDesiredHeading, fResult);
				else
					m_fDesiredHeading = fResult;
			}

			//Pitch now
			float goalY = vTargetVectorRotated.Dot(ZAXIS);
			float actualY = vAimVectorRotated.Dot(ZAXIS);
			float pitchDelta = goalY-actualY;
			fResult = Clamp((m_fLastPitch*2.0f - 1.0f) + Sign(pitchDelta)*5.0f*(pitchDelta*pitchDelta), -1.0f, 1.0f);				
			fDesiredPitch = fResult;
			m_fDesiredPitch = (fResult + 1.0f)/2.0f;
			return true;			
		}			
	} 
	return false;
}

void CTaskAimGunVehicleDriveBy::CalculateIntroBlendParam(float fDesiredYaw)
{
	if (m_fIntroHeadingBlend < 0.0f)
	{
		if(IsFirstPersonDriveBy())
		{
			// The intro parameter specifies the blend of the intro clips
			// We have for example 90 degree left and 180 degree left intro clips, blending these equally, we get an
			// intro that leaves us at 135 degrees left
			m_fIntroHeadingBlend = CTaskAimGun::ConvertRadianToSignal(fDesiredYaw, rage::Clamp(m_fMinHeadingAngle, -PI, PI), rage::Clamp(m_fMaxHeadingAngle, -PI, PI), false);
			//static dev_float sf_IntroClamp = 1.0f;
			//m_fIntroHeadingBlend = Clamp(m_fIntroHeadingBlend, 1.0f - sf_IntroClamp, sf_IntroClamp); 
		}
		else
		{
			m_fIntroHeadingBlend = m_fCurrentHeading;
		}
	}

	m_MoveNetworkHelper.SetFloat(ms_IntroHeadingId, m_fIntroHeadingBlend);

	m_bForceUpdateMoveParamsForIntro = false;
}

float CTaskAimGunVehicleDriveBy::ComputeYawSignalFromDesired(float &fDesiredYawAngle, float fSweepHeadingMin, float fSweepHeadingMax)
{
	// Cannot overshoot initially
	bool bInitialHeading = false;
	if (m_fCurrentHeading < 0.0f)
	{
		bInitialHeading = true;
	}

	//Get overaim range
	CPed* pPed = GetPed();
	const CVehicleDriveByInfo* pDrivebyInfo = CVehicleMetadataMgr::GetVehicleDriveByInfoFromPed(pPed);
	//Possible there is no pDrivebyInfo if just got kicked off the vehicle or something?  B* 1153965
	//taskAssertf(pDrivebyInfo, "No driveby info for vehicle %s", pPed->GetMyVehicle() ? pPed->GetMyVehicle()->GetDebugName() : "mount");
	if (!pDrivebyInfo)
		return 0.5f;

	const CWeaponInfo* pWeaponInfo = GetPedWeaponInfo();
	if (!pWeaponInfo)
	{
		return 0.5f;
	}

	const CVehicleDriveByAnimInfo* pDrivebyClipInfo = CVehicleMetadataMgr::GetDriveByAnimInfoForWeapon(pPed, pWeaponInfo->GetHash());
	if (!pDrivebyClipInfo)
	{
		return 0.5f;
	}
	bool isOverAiming = false;
	float fMinRestrictedHeadingAngle = pDrivebyInfo->GetMinRestrictedAimSweepHeadingAngleDegs(pPed) * DtoR;
	float fMaxRestrictedHeadingAngle = pDrivebyInfo->GetMaxRestrictedAimSweepHeadingAngleDegs(pPed) * DtoR;

	Vector2 vMinMax;
	if (CTaskVehicleGun::GetRestrictedAimSweepHeadingAngleRads(*pPed, vMinMax))
	{
		fMinRestrictedHeadingAngle = vMinMax.x;
		fMaxRestrictedHeadingAngle = vMinMax.y;
	}

#if __DEV
	static dev_bool DEBUGYAWSIGNAL = false;
	static dev_float RESTRICTMIN = -190.0f;
	static dev_float RESTRICTMAX = 190.0f;
	static dev_float SWEEPMIN = -190.0f;
	static dev_float SWEEPMAX = 190.0f;
	if (DEBUGYAWSIGNAL)
	{
		fMinRestrictedHeadingAngle = RESTRICTMIN * DtoR; fMaxRestrictedHeadingAngle = RESTRICTMAX * DtoR;
		fSweepHeadingMin = SWEEPMIN * DtoR; fSweepHeadingMax = SWEEPMAX * DtoR;
	}
#endif

	const camBaseCamera* pDominantRenderedCamera = camInterface::GetDominantRenderedCamera();
	if(pDominantRenderedCamera && pDominantRenderedCamera->GetIsClassId(camCinematicMountedCamera::GetStaticClassId()))
	{
		//! Clamp animation if camera is looking a particular way past threshold.
		const camCinematicMountedCamera* pMountedCamera	= static_cast<const camCinematicMountedCamera*>(pDominantRenderedCamera);
		if(pMountedCamera && camInterface::FindFollowPed() == pPed)
		{
			bool bLookingLeft = pMountedCamera->IsLookingLeft();

			if(bLookingLeft && fDesiredYawAngle > HALF_PI)
			{
				fDesiredYawAngle = -PI;
			}
			else if(!bLookingLeft && fDesiredYawAngle < -HALF_PI)
			{
				fDesiredYawAngle = PI;
			}
		}
	}

	float fYawSignal = CTaskAimGun::ConvertRadianToSignal(fDesiredYawAngle, fSweepHeadingMin, fSweepHeadingMax, false);	
	bool bPossibleFlip = !bInitialHeading && ( (m_fDesiredHeading > 0.5f && fYawSignal < 0.5f) || (m_fDesiredHeading < 0.5f && fYawSignal > 0.5f) );
	//bool bPossibleFlip = !bInitialHeading && (( m_fDesiredHeading > 0.5f && fDesiredYawAngle > fSweepHeadingMax ) 
	//	|| (m_fDesiredHeading < 0.5f && fDesiredYawAngle < fSweepHeadingMin ));
	// deal with having to aim at angles > or < PI without flipping direction (overshooting)
	float fRestrictedfDesiredYawAngle = fDesiredYawAngle;
	
	if(!IsFirstPersonDriveBy())
	{
		//! FPS IK's gun to camera, we don't want to do any flips here.
		if (bPossibleFlip)
		{	
			if (fMinRestrictedHeadingAngle < -PI && fRestrictedfDesiredYawAngle >0) 
			{
				if ((fMinRestrictedHeadingAngle+2.0f*PI) < fRestrictedfDesiredYawAngle) 
				{
					fRestrictedfDesiredYawAngle-=2.0f*PI;
				}
			} 
			else if (fMaxRestrictedHeadingAngle > PI && fRestrictedfDesiredYawAngle <0) 
			{
				if ((fMaxRestrictedHeadingAngle-2.0f*PI) > fRestrictedfDesiredYawAngle) 
				{
					fRestrictedfDesiredYawAngle+=2.0f*PI;
				}
			}
		}
		if (fRestrictedfDesiredYawAngle >= fSweepHeadingMax && fRestrictedfDesiredYawAngle <= fMaxRestrictedHeadingAngle)
		{
			m_bOverAimingCClockwise = true;
			m_bOverAimingClockwise = false;
			isOverAiming = true;
			return 1.0f;
		}
		else if (fRestrictedfDesiredYawAngle <= fSweepHeadingMin && fRestrictedfDesiredYawAngle >= fMinRestrictedHeadingAngle)
		{
			m_bOverAimingClockwise = true;
			m_bOverAimingCClockwise = false;
			isOverAiming = true;
			return 0.0f;
		}

	
		if (bPossibleFlip) 
		{	//possible flip detected
			if (fSweepHeadingMin < -PI && fDesiredYawAngle >0) 
			{
				if ((fSweepHeadingMin+2.0f*PI) < fDesiredYawAngle) 
				{
					fYawSignal = CTaskAimGun::ConvertRadianToSignal(fDesiredYawAngle - 2.0f*PI, fSweepHeadingMin, fSweepHeadingMax, false);
					m_bOverAimingCClockwise = true;
					m_bOverAimingClockwise = false;
					isOverAiming = true;
				}
			}				
			else if (fSweepHeadingMax > PI && fDesiredYawAngle <0) 
			{
				if ((fSweepHeadingMax-2.0f*PI) > fDesiredYawAngle) 
				{
					fYawSignal = CTaskAimGun::ConvertRadianToSignal(fDesiredYawAngle + 2.0f*PI, fSweepHeadingMin, fSweepHeadingMax, false);
					m_bOverAimingClockwise = true;
					m_bOverAimingCClockwise = false;
					isOverAiming = true;
				}
			}					
		}
		if (!isOverAiming)
		{
			if (m_bOverAimingClockwise)
			{
				if (fDesiredYawAngle < 0) 
				{	
					const fwClipSet *pClipSet = m_MoveNetworkHelper.GetClipSet();
					if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_OnMount) || (pClipSet && pClipSet->GetClip(fwMvClipId("sweep_blocked",0x82E95070)) != NULL))
					{
						m_MoveNetworkHelper.SendRequest(ms_FlipLeft);									
					}
					m_bFlipping = true;
				}
				m_bOverAimingClockwise = false;
			} 
			else if (m_bOverAimingCClockwise)
			{
				if (fDesiredYawAngle > 0)
				{				
					const fwClipSet *pClipSet = m_MoveNetworkHelper.GetClipSet();
					if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_OnMount) || (pClipSet && pClipSet->GetClip(fwMvClipId("sweep_blocked",0x82E95070)) != NULL))
					{
						m_MoveNetworkHelper.SendRequest(ms_FlipRight);									
					}
					m_bFlipping = true;
				}
				m_bOverAimingCClockwise = false;
			} 
		}
	}

	return fYawSignal;
}

void CTaskAimGunVehicleDriveBy::CheckForBlocking()
{
	CPed* pPed = GetPed();
	if (!m_MoveNetworkHelper.IsNetworkActive())
		return;

	const fwClipSet *pClipSet = m_MoveNetworkHelper.GetClipSet();
	if ((m_bHolding && m_bBlockedHitPed) || (pClipSet==NULL) ||(pClipSet && pClipSet->GetClip(fwMvClipId("sweep_blocked",0x82E95070)) == NULL))
	{
		return;
	}

	if (pPed->IsInFirstPersonVehicleCamera())
	{
		// Don't run any of this blocking code on the rear seat of bikes in first person (as cameras are fine)
		const CVehicle* pVehicle = pPed->GetMyVehicle();
		if (pVehicle && pVehicle->InheritsFromBike())
		{
			const CVehicleSeatInfo* pSeatInfo = pVehicle->GetSeatInfo(pPed);
			if (pSeatInfo && !pSeatInfo->GetIsFrontSeat())
				return;
		}

		// Run a capsule check if the heading changed quickly or timer has run out
		TUNE_GROUP_FLOAT(VEHICLE_DRIVE_BY, DELAY_BLOCK_CHECK_HEADING, 0.01f, 0.0f, 10.0f, 0.001f);
		TUNE_GROUP_INT(VEHICLE_DRIVE_BY, DELAY_BLOCK_CHECK_TIME, 750, 0, 2000, 1);
		bool bIsLastBlockedHeadingZero = m_fLastBlockedHeading == 0.0f;
		float fHeadingDiff = abs(m_fCurrentHeading - m_fLastBlockedHeading);
		if ((fwTimer::GetTimeInMilliseconds() < (m_uTimeLastBlocked + DELAY_BLOCK_CHECK_TIME)) && (!bIsLastBlockedHeadingZero && (fHeadingDiff < DELAY_BLOCK_CHECK_HEADING)))
		{
			return;
		}
	}

	// Block firing if local ped is within an air defence zone.
	if (!pPed->IsNetworkClone() && CAirDefenceManager::AreAirDefencesActive())
	{
		u8 uIntersectingZoneIdx = 0;
		if (CAirDefenceManager::IsPositionInDefenceZone(pPed->GetTransform().GetPosition(), uIntersectingZoneIdx))
		{
			m_bBlocked = true;
			return;
		}
	}

	//AI PEDS
	if (!pPed->IsPlayer())
	{
		Vector3 vTargetPos( Vector3::ZeroType );
		if (m_target.GetPositionWithFiringOffsets( pPed, vTargetPos ))
		{
			static dev_float sf_AIBlockDistance = 1.5f * 1.5f;
			Vector3 vStartPos = pPed->GetBonePositionCached(BONETAG_NECK);
			if (vStartPos.Dist2(vTargetPos) < sf_AIBlockDistance)
			{
				m_MoveNetworkHelper.SendRequest( ms_Blocked);
				m_bBlocked = true;
			}
			else
			{
				m_MoveNetworkHelper.SendRequest( ms_Aim);
				m_bBlocked = false;
			}
		}
		else
		{
			m_MoveNetworkHelper.SendRequest( ms_Aim);
			m_bBlocked = false;
		}

		// Only return if we're not doing an FPS probe check for AI peds, or we are but we are already blocked.
		if (!m_bShouldDoProbeTestForAIOrClones || (m_bShouldDoProbeTestForAIOrClones && m_bBlocked))
		{
			return;
		}
	}

	//PLAYER PEDS ... or clone peds if local player is in FPS cam
#if __BANK
	if (!ms_bDisableBlocking)
#endif // ___BANK
	if ((pPed->IsLocalPlayer() || m_bShouldDoProbeTestForAIOrClones) && !IsFlippingSomeoneOff())
	{
		TUNE_GROUP_BOOL(VEHICLE_DRIVE_BY, useBlockingAnimsForOutsideRange, true);
		if (useBlockingAnimsForOutsideRange && pPed->IsPlayer() && m_bBlockOutsideAimingRange)
		{
			m_MoveNetworkHelper.SendRequest(ms_Blocked);
			m_bBlocked = true;
			return;
		}

		weaponAssert(pPed->GetWeaponManager());
		const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
		const float fWeaponRange = pWeapon->GetRange();
		float fIntersectionDist = fWeaponRange;

		const s32 iMaxExclusions = 2;
		const CEntity* exclusionList[iMaxExclusions];
		s32 iNumExclusions = 0;
		exclusionList[iNumExclusions++] = pPed;
		if(pPed->GetIsParachuting() || pPed->GetPedResetFlag(CPED_RESET_FLAG_IsUsingJetpack))
		{
			exclusionList[iNumExclusions++] = (CEntity *)pPed->GetAttachParent();
		}

		u32 includeFlags = ArchetypeFlags::GTA_MAP_TYPE_WEAPON | ArchetypeFlags::GTA_OBJECT_TYPE | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_PED_TYPE;
		if (pPed->IsInFirstPersonVehicleCamera())
		{
			// Test against ragdoll bounds, not ped bounds
			includeFlags &= ~ArchetypeFlags::GTA_PED_TYPE;
			includeFlags |= ArchetypeFlags::GTA_RAGDOLL_TYPE;
		}

		if (m_bShouldDoProbeTestForAIOrClones)
		{
			// Only test for ragdoll bounds and weapons for clone ped blocking
			includeFlags = ArchetypeFlags::GTA_RAGDOLL_TYPE | ArchetypeFlags::GTA_MAP_TYPE_WEAPON;
		}

		const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		Vector3 vStart, vEnd;		

		bool bIsBike = pPed->GetMyVehicle() && pPed->GetMyVehicle()->InheritsFromBike();

		WorldProbe::CShapeTestFixedResults<> probeResults;
		WorldProbe::CShapeTestProbeDesc probeDesc;
		probeDesc.SetResultsStructure(&probeResults);
		probeDesc.SetIncludeFlags(includeFlags);
		probeDesc.SetExcludeEntities(exclusionList, iNumExclusions, ((pPed->IsInFirstPersonVehicleCamera() && !bIsBike) || m_bShouldDoProbeTestForAIOrClones) ? WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS : EXCLUDE_ENTITY_OPTIONS_NONE);
		probeDesc.SetContext(WorldProbe::LOS_Weapon);

		//On bikes, check for the occupant possibly penetrating an object to the side (like a wall)	
		if (bIsBike && m_fCurrentHeading <= 0.4f) 
		{
			vStart = vPedPosition;
			Vector3 vRight = VEC3V_TO_VECTOR3(pPed->GetTransform().GetA()); 
			TUNE_GROUP_FLOAT(VEHICLE_DRIVE_BY, fBikeLeftCheckScale, 0.6f, 0.0f, 10.0f, 0.01f);
			vEnd = vStart; vEnd.AddScaled(vRight, -fBikeLeftCheckScale);
			probeDesc.SetStartAndEnd(vStart, vEnd);
#if __BANK
			ms_debugDraw.AddLine(RCC_VEC3V(vStart), RCC_VEC3V(vEnd), Color_pink, 1000);				
#endif
			if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
			{
				m_MoveNetworkHelper.SendRequest( ms_BikeBlocked);
				m_bBlocked = true;
				return;
			}
		}

		// Fire a probe from the camera to find intersections
		TUNE_GROUP_BOOL(VEHICLE_DRIVE_BY, useCamStartPos, false);
		if(pPed->IsLocalPlayer())
		{
			Vector3	vecAim = camInterface::GetFront();
			vecAim.Normalize();
			vEnd = pPed->GetPlayerInfo()->GetTargeting().GetClosestFreeAimTargetPos();		

			vStart = pPed->GetBonePositionCached(BONETAG_NECK);
		}

		WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
		
		float CAPSULE_TEST_WIDTH = 0.0f;
		if (m_bShouldDoProbeTestForAIOrClones)
		{
			TUNE_GROUP_FLOAT(DRIVEBY_DEBUG, CAPSULE_TEST_WIDTH_FPS_AI_CLONES, 0.1f, 0.0f, 0.5f, 0.001f);
			CAPSULE_TEST_WIDTH = CAPSULE_TEST_WIDTH_FPS_AI_CLONES;
		}
		else
		{
			TUNE_GROUP_FLOAT(DRIVEBY_DEBUG, CAPSULE_TEST_WIDTH_FPS, 0.05f, 0.0f, 0.5f, 0.001f);
			CAPSULE_TEST_WIDTH = CAPSULE_TEST_WIDTH_FPS;
		}

		bool bDoingCapsuleTestAroundGun = false;
		if (pPed->IsInFirstPersonVehicleCamera() || m_bShouldDoProbeTestForAIOrClones)
		{
			bDoingCapsuleTestAroundGun = true;
			bool bUsingLeftHand = false;
			if (pPed->IsLocalPlayer() && m_pVehicleDriveByInfo && m_pVehicleDriveByInfo->GetLeftHandedFirstPersonAnims() && pPed->IsInFirstPersonVehicleCamera())
			{
				bUsingLeftHand = true;
			}
			else if (pWeapon && pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetEquippedWeaponAttachPoint() == CPedEquippedWeapon::AP_LeftHand)
			{
				bUsingLeftHand = true;
			}

			const CObject* pWeaponObject = pPed->GetWeaponManager()->GetEquippedWeaponObject();
			Matrix34 weaponMatrix = MAT34V_TO_MATRIX34(pWeaponObject->GetTransform().GetMatrix());

			if (m_bBlockedHitPed || !pPed->IsLocalPlayer())
			{
				// Do test from hand to muzzle of gun
				vStart = bUsingLeftHand ? pPed->GetBonePositionCached(BONETAG_L_HAND) : pPed->GetBonePositionCached(BONETAG_R_HAND);
				pWeapon->GetMuzzlePosition(weaponMatrix, vEnd);
			}
			else
			{
				vStart = camInterface::GetPos();

				TUNE_GROUP_FLOAT(DRIVEBY_DEBUG, BLOCKED_CAPSULE_TEST_LENGTH, 1.0f, 0.0f, 1000.0f, 0.1f);

				Vector3 vDirection = vStart - vEnd;
				float fMag = vDirection.Mag();
				float fDiff = fMag - BLOCKED_CAPSULE_TEST_LENGTH;
				if (fDiff > 0.0f)
					vEnd = vEnd + (fDiff / fMag)*vDirection;
			}

#if __BANK
			TUNE_GROUP_BOOL(VEHICLE_DRIVE_BY, bRenderBlockingSpheresFPS, false);
			if (bRenderBlockingSpheresFPS)
			{
				ms_debugDraw.AddSphere(RCC_VEC3V(vEnd), 0.025f, Color_blue, 150);
			}
#endif

			// If we're blocked, add the inverse of the blocked offset distance to end position so we don't get any blocked oscillation
			if (m_bBlocked || m_bCloneOrAIBlockedDueToPlayerFPS)
			{
				vEnd = VEC3V_TO_VECTOR3(AddScaled(VECTOR3_TO_VEC3V(vEnd), VECTOR3_TO_VEC3V(weaponMatrix.b), ScalarV(-m_vBlockedOffset.x)));
				vEnd = VEC3V_TO_VECTOR3(AddScaled(VECTOR3_TO_VEC3V(vEnd), VECTOR3_TO_VEC3V(weaponMatrix.a), ScalarV(-m_vBlockedOffset.y)));
				vEnd = VEC3V_TO_VECTOR3(AddScaled(VECTOR3_TO_VEC3V(vEnd), VECTOR3_TO_VEC3V(weaponMatrix.c), ScalarV(-m_vBlockedOffset.z)));

#if __BANK
				if (bRenderBlockingSpheresFPS)
				{
					ms_debugDraw.AddSphere(RCC_VEC3V(vEnd), 0.025f, Color_green, 150);
				}
#endif
			}

			// Do capsule test instead of probe test
			capsuleDesc.SetResultsStructure(&probeResults);
			capsuleDesc.SetCapsule(vStart, vEnd, CAPSULE_TEST_WIDTH);
			capsuleDesc.SetIsDirected(true);
			capsuleDesc.SetDoInitialSphereCheck(true);
			capsuleDesc.SetIncludeFlags(includeFlags);
			capsuleDesc.SetExcludeEntities(exclusionList, iNumExclusions, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS);
			capsuleDesc.SetContext(WorldProbe::LOS_Weapon);
		}

		if (useCamStartPos)
			vStart = camInterface::GetPos();

		probeDesc.SetStartAndEnd(vStart, vEnd);
#if __BANK
		ms_debugDraw.AddLine(RCC_VEC3V(vStart), RCC_VEC3V(vEnd), Color_green, 1);
		if (pPed->IsInFirstPersonVehicleCamera() || m_bShouldDoProbeTestForAIOrClones)
			ms_debugDraw.AddCapsule(RCC_VEC3V(vStart), RCC_VEC3V(vEnd), CAPSULE_TEST_WIDTH, Color_green, 1, 0, false);
#endif	

		if((pPed->IsInFirstPersonVehicleCamera() || m_bShouldDoProbeTestForAIOrClones) ? WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc) : WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
		{
			fIntersectionDist = (vPedPosition - probeResults[0].GetHitPosition()).XYMag2();

			TUNE_GROUP_FLOAT(VEHICLE_DRIVE_BY, fMinBlockingDist, 1.2f, 0.0f, 3.0f, 0.01f);
			
			if (fIntersectionDist < square(fMinBlockingDist))
			{
#if __BANK
				ms_debugDraw.AddLine(RCC_VEC3V(vStart), RCC_VEC3V(vEnd), Color_red, 100);
				if (pPed->IsInFirstPersonVehicleCamera() || m_bShouldDoProbeTestForAIOrClones)
					ms_debugDraw.AddCapsule(RCC_VEC3V(vStart), RCC_VEC3V(vEnd), CAPSULE_TEST_WIDTH, Color_red, 100, 0, false);
				ms_debugDraw.AddSphere(RCC_VEC3V(probeResults[0].GetHitPosition()), 0.025f, Color_red, 100);
#endif				

				float fCapsulePenetrationDepth = 0.0f;
				if (pPed->IsInFirstPersonVehicleCamera() || m_bShouldDoProbeTestForAIOrClones)
				{
					fCapsulePenetrationDepth = (vEnd - probeResults[0].GetHitPosition()).Mag();
				}
			
				//Special bike blocked mode since bikes lean left when still.
				TUNE_GROUP_FLOAT(VEHICLE_DRIVE_BY, fBikeBlockMinHeading, 0.1f, 0.0f, 1.0f, 0.01f);
				TUNE_GROUP_FLOAT(VEHICLE_DRIVE_BY, fBikeBlockMaxHeading, 0.4f, 0.0f, 1.0f, 0.01f);
				if (pPed->GetMyVehicle() && pPed->GetMyVehicle()->InheritsFromBike() && 
					m_fCurrentHeading>=fBikeBlockMinHeading && m_fCurrentHeading<=fBikeBlockMaxHeading)
					m_MoveNetworkHelper.SendRequest( ms_BikeBlocked);
				else
				{
					// B*6510231: Don't use blocked anims in FPS mode; we IK the arm back instead (see m_vBlockedOffset/m_vBlockedPosition).
					if (!pPed->IsInFirstPersonVehicleCamera() && !m_bShouldDoProbeTestForAIOrClones)
					{
						m_MoveNetworkHelper.SendRequest( ms_Blocked);
					}
				}

				if (m_bShouldDoProbeTestForAIOrClones || bDoingCapsuleTestAroundGun)
				{
					m_bCloneOrAIBlockedDueToPlayerFPS = true;
				}
				else
				{
					m_bBlocked = true;
				}

				m_bBlockedHitPed = probeResults[0].GetHitEntity()->GetIsTypePed();
				m_fPenetrationDistance = -fCapsulePenetrationDepth;
				m_uTimeLastBlocked = fwTimer::GetTimeInMilliseconds();
				m_fLastBlockedHeading = m_fCurrentHeading;

				if (!m_bBlockedHitPed && !m_bHolding && m_MoveNetworkHelper.IsNetworkActive() && pPed->IsLocalPlayer())
				{
					const fwClipSet *pClipSet = m_MoveNetworkHelper.GetClipSet();
					if (pClipSet && pClipSet->GetClip(fwMvClipId("sweep_blocked",0x82E95070)) != NULL)
						m_MoveNetworkHelper.SendRequest( ms_Hold);
					m_bHolding = true;
				}
			}
			else
			{
#if __BANK
				ms_debugDraw.AddLine(RCC_VEC3V(vStart), RCC_VEC3V(vEnd), Color_green, 100);
				if (pPed->IsInFirstPersonVehicleCamera() || m_bShouldDoProbeTestForAIOrClones)
					ms_debugDraw.AddCapsule(RCC_VEC3V(vStart), RCC_VEC3V(vEnd), CAPSULE_TEST_WIDTH, Color_green, 100, 0, false);
				ms_debugDraw.AddSphere(RCC_VEC3V(probeResults[0].GetHitPosition()), 0.025f, Color_green, 100);
#endif
				m_MoveNetworkHelper.SendRequest( ms_Aim);
				m_bBlocked = false;
				m_bCloneOrAIBlockedDueToPlayerFPS = false;
				if (!m_bBlockedHitPed && m_bHolding && m_MoveNetworkHelper.IsNetworkActive())
				{
					m_MoveNetworkHelper.SendRequest( ms_HoldEnd );
					m_bHolding = false;
				}
				m_bBlockedHitPed = true;
			}
		}
		else
		{
#if __BANK
			ms_debugDraw.AddLine(RCC_VEC3V(vStart), RCC_VEC3V(vEnd), Color_green, 100);			
#endif
			m_MoveNetworkHelper.SendRequest( ms_Aim);
			m_bBlocked = false;
			m_bCloneOrAIBlockedDueToPlayerFPS = false;
			if (!m_bBlockedHitPed && m_bHolding && m_MoveNetworkHelper.IsNetworkActive())
			{
				m_MoveNetworkHelper.SendRequest( ms_HoldEnd );
				m_bHolding = false;
			}
			m_bBlockedHitPed = true;
		}
	}
}

bool CTaskAimGunVehicleDriveBy::CheckForFireLoop()
{
	if ((m_overrideClipSetId != CLIP_SET_ID_INVALID) && (m_eType == CVehicleDriveByAnimInfo::STD_CAR_DRIVEBY || m_eType == CVehicleDriveByAnimInfo::REAR_HELI_DRIVEBY))
	{
		return (m_bMoveFireLoop1 || m_bMoveFireLoop2);
	}

	return false;
}

bool CTaskAimGunVehicleDriveBy::CheckForOutroOnBicycle(WeaponControllerState UNUSED_PARAM(controllerState), const CControl& UNUSED_PARAM(control)) const
{
	/*const CPed& ped = *GetPed();
	if (controllerState != WCS_Fire && ped.GetMyVehicle() && ped.GetMyVehicle()->GetVehicleType() == VEHICLE_TYPE_BICYCLE)
	{
	const bool bTryingToAccelerate = control.GetVehiclePushbikeRearBrake().IsDown() || control.GetVehiclePushbikePedal().IsDown();
	if (bTryingToAccelerate)
	{
	return true;
	}
	}*/
	return false;
}

// Returns the current weapon controller state for the specified ped
WeaponControllerState CTaskAimGunVehicleDriveBy::GetWeaponControllerState(const CPed* ped) const
{
	if(ped->IsAPlayerPed())
	{
		if(ped->IsNetworkClone())
		{
			s32 stateFromNetwork = GetStateFromNetwork();

			if(stateFromNetwork == State_Fire || m_fireCounter < m_targetFireCounter)
			{
				return WCS_Fire;
			}
			else if(stateFromNetwork == State_Aim)
			{
				return WCS_Aim;
			}
			else if(stateFromNetwork == State_Reload ||
				stateFromNetwork == State_VehicleReload )
			{
				return WCS_Reload;
			}
		}
		else
		{
			return CWeaponControllerManager::GetController(m_weaponControllerType)->GetState(ped, false);
		}
	}
	else
	{
		// Check ammo
		weaponAssert(ped->GetWeaponManager());
		s32 iAmmo = ped->GetInventory()->GetWeaponAmmo(ped->GetWeaponManager()->GetEquippedWeaponInfo());
		if (iAmmo == 0 && !ped->GetWeaponManager()->GetIsArmedMelee())
		{
			return WCS_None;
		}

		const CWeapon* pWeapon = ped->GetWeaponManager()->GetEquippedWeapon();
		if (pWeapon && pWeapon->GetState() == CWeapon::STATE_RELOADING)
		{
			return WCS_None;
		}

		return WCS_Fire;
	}

    return WCS_None;
}

bool CTaskAimGunVehicleDriveBy::FindIntersection(CPed* pPed, Vector3& vStart, const Vector3& vEnd, Vector3& vIntersection, float& fIntersectionDist)
{
	weaponAssert(pPed->GetWeaponManager());
	const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
	fIntersectionDist =  pWeapon->GetRange();

	TUNE_GROUP_FLOAT(VEHICLE_DRIVE_BY, zOffset, 0.5f, 0.0f, 2.0f, 0.01f);
	TUNE_GROUP_FLOAT(VEHICLE_DRIVE_BY, yOffset, 0.0f, -2.0f, 2.0f, 0.01f);

	const s32 iNumExclusions = 1;
	const CEntity* exclusionList[iNumExclusions] = { pPed };
	const u32 includeFlags = ArchetypeFlags::GTA_MAP_TYPE_WEAPON | ArchetypeFlags::GTA_OBJECT_TYPE | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_PED_TYPE;

	// Fire a probe from the camera to find intersections
	WorldProbe::CShapeTestFixedResults<> probeResults;
	WorldProbe::CShapeTestProbeDesc probeDesc;
	probeDesc.SetStartAndEnd(vStart, vEnd);
	probeDesc.SetResultsStructure(&probeResults);
	probeDesc.SetIncludeFlags(includeFlags);
	probeDesc.SetExcludeEntities(exclusionList, iNumExclusions);
	probeDesc.SetContext(WorldProbe::LOS_Weapon);

	// The start pos is set after the probe has been set up, so that the test is done from the camera pos
	// Get the world position of the right shoulder
	s32 iSpineBone = pPed->GetBoneIndexFromBoneTag(BONETAG_SPINE0);
	Matrix34 mSpine0;
	pPed->GetGlobalMtx(iSpineBone, mSpine0);
	Vector3 vSpineForward = mSpine0.b;
	TUNE_GROUP_BOOL(VEHICLE_DRIVE_BY, useAlternatePos, true);

	float fZOffset = zOffset;
	if (useAlternatePos)
	{
		TUNE_GROUP_INT(VEHICLE_DRIVE_BY, altBone, BONETAG_R_PH_HAND, 0, 100000, 1);
		s32 iAltBone = pPed->GetBoneIndexFromBoneTag((eAnimBoneTag)altBone);
		Matrix34 mAltBone;
		pPed->GetGlobalMtx(iAltBone, mAltBone);
		vStart = mAltBone.d;
		fZOffset = 0.0f;
	}
	else
	{
		vStart = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	}
	vStart += vSpineForward * yOffset;
	vStart.z += fZOffset; 

	if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
	{
		Matrix34 matGunMuzzle;
		CObject* pWeaponObject = pPed->GetWeaponManager()->GetEquippedWeaponObject();
		const CWeaponModelInfo* pModelInfo = pWeapon->GetWeaponModelInfo();
		if(pWeaponObject && pModelInfo && pModelInfo->GetDrawable() && pModelInfo->GetDrawable()->GetSkeletonData())
		{
			pWeaponObject->GetGlobalMtx(pModelInfo->GetBoneIndex(WEAPON_MUZZLE), matGunMuzzle);
		}

		// Check the intersection is not behind us
		vIntersection = probeResults[0].GetHitPosition();

		Vector3 vIntersectionDir = vIntersection - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		vIntersectionDir.Normalize();
		Vector3 vCamDir = camInterface::GetFront();
		vCamDir.Normalize();

		TUNE_GROUP_FLOAT(VEHICLE_DRIVE_BY, camDotTol, 0.0f, -1.0f, 1.0f, 0.01f);
		if (vIntersectionDir.Dot(vCamDir) < camDotTol)
		{
			return false;
		}

		Vector3 vMuzzlePos = matGunMuzzle.d;
		fIntersectionDist = vMuzzlePos.Dist(vIntersection);
		return true;
	}

	return false;
}

void CTaskAimGunVehicleDriveBy::ProcessInsult()
{
	//Grab the ped.
	CPed* pPed = GetPed();

	//Ensure we are flipping someone off.
	if(!IsFlippingSomeoneOff())
	{
		return;
	}

	//Ensure the timer has expired.
	float fMinTimeBetweenInsults = sm_Tunables.m_MinTimeBetweenInsults;
	if(m_fTimeSinceLastInsult < fMinTimeBetweenInsults)
	{
		return;
	}
	
	//Check if the ped is a player.
	if(pPed->IsPlayer())
	{
		//Ensure talking is not disabled.
		if(pPed->GetPedResetFlag(CPED_RESET_FLAG_DisableTalk))
		{
			return;
		}
		
		//Insult.
		CTalkHelper::Options options(CTalkHelper::Options::CanBeUnfriendly,
			sm_Tunables.m_MaxDistanceToInsult, sm_Tunables.m_MinDotToInsult);
		if(!CTalkHelper::Talk(*pPed, options))
		{
			return;
		}
	}
	else
	{
		//Insult.
		if(!pPed->NewSay("FLIP_OFF"))
		{
			return;
		}
	}
	
	//Clear the time since the last insult.
	m_fTimeSinceLastInsult = 0.0f;
}

void CTaskAimGunVehicleDriveBy::ProcessTimers()
{
	//Grab the time step.
	float fTimeStep = GetTimeStep();
	
	//Update the timers.
	m_fTimeSinceLastInsult += fTimeStep;
}

bool CTaskAimGunVehicleDriveBy::IsFirstPersonDriveBy() const 
{
	// B*2014712/2003775: Don't use FPS code if script have overriden the vehicle context
	CTaskMotionBase* pTask = GetPed()->GetPrimaryMotionTask();
	const u32 uOverrideInVehicleContext = pTask ? pTask->GetOverrideInVehicleContextHash() : 0;
	if (uOverrideInVehicleContext != 0)
	{
		return false;
	}

	if(GetPed()->IsInFirstPersonVehicleCamera())
	{
		return true;
	}

#if FPS_MODE_SUPPORTED
	if(GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		return true;
	}
#endif

	return false;
}

bool CTaskAimGunVehicleDriveBy::IsPoVCameraConstrained(const CPed *pPed) const
{
	if(pPed->GetPedResetFlag(CPED_RESET_FLAG_PoVCameraConstrained))
	{
		return true;
	}

	TUNE_GROUP_INT(FIRST_PERSON_STEERING, uPoVConstrainTime, 500, 0, 2000, 1);

	if( fwTimer::GetTimeInMilliseconds() <= (m_uTimePoVCameraConstrained + uPoVConstrainTime) )
	{
		return true;
	}

	return false;
}

bool CTaskAimGunVehicleDriveBy::IsHidingGunInFirstPersonDriverCamera(const CPed *pPed, bool bSkipPoVConstrainedCheck)
{
#if __BANK
	TUNE_GROUP_BOOL(FIRST_PERSON_STEERING, bForceHideGunInFirstPersonDriveBy, false);
	if(bForceHideGunInFirstPersonDriveBy)
	{
		return true;
	} 

	TUNE_GROUP_BOOL(FIRST_PERSON_STEERING, bForceShowGunInFirstPersonDriveBy, false);
	if(bForceShowGunInFirstPersonDriveBy)
	{
		return false;
	}
#endif

	// B*2014712/2003775: Don't use FPS code if script have overriden the vehicle context
	CTaskMotionBase* pTask = pPed->GetPrimaryMotionTask();
	const u32 uOverrideInVehicleContext = pTask ? pTask->GetOverrideInVehicleContextHash() : 0;
	if (uOverrideInVehicleContext != 0)
	{
		return false;
	}

	// B*3072872: Hide weapon when using the FAGGIO3 with any windscreen mod
	if (pPed->GetIsInVehicle() && pPed->GetIsDrivingVehicle() && pPed->GetIsVisible() && pPed->IsInFirstPersonVehicleCamera())
	{
		const CVehicle* pVehicle = pPed->GetMyVehicle();
		if (MI_BIKE_FAGGIO3.IsValid() && pVehicle->GetModelIndex() == MI_BIKE_FAGGIO3)
		{
			const CVehicleVariationInstance& variation = pVehicle->GetVariationInstance();
			if (variation.GetModIndex(VMT_WING_L) != INVALID_MOD)
			{
				return true;
			}
		}		
	}

	const CWeapon* pWeapon = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeapon() : NULL;
	if(pWeapon)
	{
		bool bUsingSniperInFP = pPed->IsLocalPlayer() && camInterface::GetGameplayDirector().IsFirstPersonAiming() && CNewHud::IsSniperSightActive();
		if(pPed->GetIsInVehicle() && pPed->GetIsVisible() && (pPed->IsInFirstPersonVehicleCamera() || bUsingSniperInFP) && !pWeapon->GetWeaponInfo()->GetIsMelee())
		{
			if(!bSkipPoVConstrainedCheck)
			{
				const CTaskAimGunVehicleDriveBy* pTask =
					static_cast<CTaskAimGunVehicleDriveBy *>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_VEHICLE_DRIVE_BY));
				if(pTask && pTask->IsPoVCameraConstrained(pPed))
				{
					return true;
				}
			}

			//! If no 1st person clip set, hide.
			const CVehicleDriveByAnimInfo* pDrivebyClipInfo = CVehicleMetadataMgr::GetDriveByAnimInfoForWeapon(pPed, pWeapon->GetWeaponInfo()->GetHash());
			if(pDrivebyClipInfo && (pDrivebyClipInfo->GetFirstPersonClipSet(pWeapon->GetWeaponInfo()->GetHash()) == CLIP_SET_ID_INVALID))
			{
				return true;
			}

			//! If weapon doesn't support IK, hide.
			if(!pWeapon->GetWeaponInfo()->GetUseFPSAimIK())
			{
				return true;
			}
		}
	}

	return false;
}

#if !__FINAL
void CTaskAimGunVehicleDriveBy::Debug() const
{
#if DEBUG_DRAW

	if (ms_bRenderVehicleDriveByDebug) 
	{
		grcDebugDraw::AddDebugOutput(Color_green, "DesiredHeading = %.4f", m_fDesiredHeading);
		grcDebugDraw::AddDebugOutput(Color_green, "DesiredPitch = %.4f", m_fDesiredPitch);
		grcDebugDraw::AddDebugOutput(Color_green, "CurrentHeading = %.4f", m_fCurrentHeading);
		grcDebugDraw::AddDebugOutput(Color_green, "CurrentPitch = %.4f", m_fCurrentPitch);
		grcDebugDraw::AddDebugOutput(Color_green, "IntroHeadingBlend = %.4f", m_fIntroHeadingBlend);
		grcDebugDraw::AddDebugOutput(Color_green, "OutroHeadingBlend = %.4f", m_fOutroHeadingBlend);
	}

	Vector3 vTargetPos;
	if(m_target.GetPosition(vTargetPos))
	{
		grcDebugDraw::Sphere(vTargetPos, 0.1f, Color_red);
	}

	if (CTaskAimGun::ms_bRenderTestPositions)
	{
		grcDebugDraw::Sphere(m_vStartPos, 0.025f, Color_blue);
		grcDebugDraw::Line(m_vStartPos, m_vEndPos, Color_blue, Color_purple);
		grcDebugDraw::Sphere(m_vEndPos, 0.025f, Color_purple);
	}

#endif // DEBUG_DRAW

	if (GetSubTask())
		GetSubTask()->Debug();

	// Base class
	CTaskAimGun::Debug();
}

const char * CTaskAimGunVehicleDriveBy::GetStaticStateName( s32 iState )
{
	switch (iState)
	{
		case State_Start:	return "State_Start";
		case State_Intro:	return "State_Intro";
		case State_Aim:		return "State_Aim";
		case State_Fire:	return "State_Fire";
		case State_Reload:	return "State_Reload";
		case State_VehicleReload:	return "State_VehicleReload";
		case State_Outro:	return "State_Outro";
		case State_Finish:	return "State_Finish";
		default: taskAssert(0);
	}

	return "State_Invalid";
}
#endif // !__FINAL

#if __BANK
void CTaskAimGunVehicleDriveBy::InitWidgets(bkBank& bank)
{
	bank.PushGroup("DriveBy Aim Task", false);

	bank.AddSeparator();
	
	bank.AddSlider("Default Min Yaw Change Rate", &ms_DefaultMinYawChangeRate, 0.0f, 1.0f, 0.01f);
	bank.AddSlider("Default Max Yaw Change Rate", &ms_DefaultMaxYawChangeRate, 0.0f, 1.0f, 0.01f);
	bank.AddSlider("Default Min Yaw Change Rate FirstPerson", &ms_DefaultMinYawChangeRateFirstPerson, 0.0f, 10.0f, 0.01f);
	bank.AddSlider("Default Max Yaw Change Rate FirstPerson", &ms_DefaultMaxYawChangeRateFirstPerson, 0.0f, 10.0f, 0.01f);
	bank.AddSlider("Default Max Pitch Change Rate", &ms_DefaultMaxPitchChangeRate, 0.0f, 3.0f, 0.01f);
	bank.AddSlider("Default Max Pitch Change Rate FirstPerson", &ms_DefaultMaxPitchChangeRateFirstPerson, 0.0f, 5.0f, 0.01f);
	bank.AddToggle("Disable Outro", &ms_bDisableOutro);
	bank.AddToggle("Disable Blocking", &ms_bDisableBlocking);
	bank.AddToggle("Disable Window Smash", &ms_bDisableWindowSmash);
	bank.AddToggle("Use Assisted Driveby Aiming", &ms_UseAssistedAiming);
	bank.AddToggle("Only Allow Lockon To Ped Threats", &ms_OnlyAllowLockOnToPedThreats);

	bank.AddSlider("Intro Rate", &ms_DefaultIntroRate, 0.0f, 5.0f, 0.1f);
	bank.AddSlider("Outro Rate", &ms_DefaultOutroRate, 0.0f, 5.0f, 0.1f);
	bank.AddSlider("Intro Rate FirstPerson", &ms_DefaultIntroRateFirstPerson, 0.0f, 5.0f, 0.1f);
	bank.AddSlider("Outro Rate FirstPerson", &ms_DefaultOutroRateFirstPerson, 0.0f, 5.0f, 0.1f);

	bank.AddSeparator();



	bank.PopGroup();
}
#endif // __BANK

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//----------------------------------------------------
// CClonedAimGunVehicleDriveByInfo
//----------------------------------------------------

CClonedAimGunVehicleDriveByInfo::CClonedAimGunVehicleDriveByInfo()
: m_fireCounter(0)
, m_iAmmoInClip(0)
, m_seed(0)
{
}

CClonedAimGunVehicleDriveByInfo::CClonedAimGunVehicleDriveByInfo(s32 aimGunVehicleDriveByState, u8 fireCounter, u8 ammoInClip, u32 seed)
: m_fireCounter(fireCounter)
, m_iAmmoInClip(ammoInClip)
, m_seed(seed)
{
	SetStatusFromMainTaskState(aimGunVehicleDriveByState);

	m_bHasSeed = (seed != 0);
}

void CClonedAimGunVehicleDriveByInfo::Serialise(CSyncDataBase& serialiser)
{
	CSerialisedFSMTaskInfo::Serialise(serialiser);

	const unsigned SIZEOF_AMMO_IN_CLIP = 8;

	SERIALISE_UNSIGNED(serialiser, m_iAmmoInClip, SIZEOF_AMMO_IN_CLIP, "Ammo In Clip");
	SERIALISE_UNSIGNED(serialiser, m_fireCounter, sizeof(m_fireCounter)<<3, "Fire counter");
	SERIALISE_BOOL(serialiser, m_bHasSeed, "m_bHasSeed");
	if (m_bHasSeed)
	{
		SERIALISE_UNSIGNED(serialiser, m_seed, 32, "m_seed");
	}
	else
	{
		m_seed = 0;
	}
}
