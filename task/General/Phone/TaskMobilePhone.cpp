#include "ai/task/taskchannel.h"
#include "phcore/phmath.h"

#include "ik/IkManager.h"
#include "Task/Animation/TaskAnims.h"
#include "Task/General/Phone/TaskMobilePhone.h"
#include "Task/General/TaskBasic.h"
#include "Task/Weapons/TaskSwapWeapon.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Peds/Horse/horseComponent.h"
#include "Animation/MovePed.h"
#include "Task/Motion/Locomotion/TaskMotionPed.h"
#include "Task/Default/TaskPlayer.h"
#include "Task/Movement/TaskIdle.h"
#include "Task/Movement/TaskParachute.h"
#include "Peds/PedPhoneComponent.h"
#include "camera/gameplay/aim/FirstPersonAimCamera.h"
#include "camera/gameplay/aim/FirstPersonShooterCamera.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/CamInterface.h"
#include "camera/viewports/Viewport.h"
#include "camera/viewports/ViewportManager.h"
#include "frontend/MobilePhone.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Vehicles/Metadata/VehicleSeatInfo.h"
#include "Weapons/Weapon.h"
#include "bank/bank.h"
#include "Task/Vehicle/TaskInVehicle.h"
#include "camera/cinematic/camera/mounted/CinematicMountedCamera.h"
#include "Task/Weapons/TaskWeaponBlocked.h"
#include "camera/helpers/ControlHelper.h"

AI_OPTIMISATIONS()

RAGE_DECLARE_CHANNEL(TaskMobilePhone)
RAGE_DEFINE_CHANNEL(TaskMobilePhone)

#define TaskMobilePhoneAssertf(cond,fmt,...)  RAGE_ASSERTF(TaskMobilePhone, cond, fmt, ##__VA_ARGS__)
#define TaskMobilePhoneWarning(fmt, ...)      RAGE_WARNINGF(TaskMobilePhone, fmt, ##__VA_ARGS__)
#define TaskMobilePhoneDebug1(fmt, ...)       RAGE_DEBUGF1(TaskMobilePhone, fmt, ##__VA_ARGS__)
#define TaskMobilePhoneDebug2(fmt, ...)       RAGE_DEBUGF2(TaskMobilePhone, fmt, ##__VA_ARGS__)
#define TaskMobilePhoneDebug3(fmt, ...)       RAGE_DEBUGF3(TaskMobilePhone, fmt, ##__VA_ARGS__)
#define TaskMobilePhoneError(fmt, ...)        RAGE_ERRORF(TaskMobilePhone, fmt, ##__VA_ARGS__)


#if !__FINAL
#define TASKMOBILEPHONE_SETSTATE(X) \
	do \
	{ \
		TaskMobilePhoneDebug1("%s: -> %s", GetStaticStateName(GetState()), GetStaticStateName(X)); \
		SetState(X); \
	} while(0) 

#else

	#define TASKMOBILEPHONE_SETSTATE(X) SetState(X)

#endif




const fwMvFlagId CTaskMobilePhone::ms_IsTexting("IsTexting",0xCA98A8F6);
const fwMvFlagId CTaskMobilePhone::ms_SelfPortrait("Selfie",0x27F5ADB8);
const fwMvFlagId CTaskMobilePhone::ms_UseCameraIntro("UseCameraIntro",0x88A4429E);
const fwMvFlagId CTaskMobilePhone::ms_UseAlternateFilter("UseAlternateFilter", 0xc9b42be5);
const fwMvFilterId CTaskMobilePhone::ms_BonemaskFilter("Bonemask", 0x038da463);

const fwMvBooleanId CTaskMobilePhone::ms_AnimFinished("AnimFinished",0xD901FB32);
const fwMvBooleanId CTaskMobilePhone::ms_CreatePhone("CreatePhone",0xB0997823);
const fwMvBooleanId CTaskMobilePhone::ms_DestroyPhone("DestroyPhone",0xC9BEB3CC);
const fwMvBooleanId CTaskMobilePhone::ms_Interruptible("Interruptible",0x550A14CF);


const fwMvRequestId CTaskMobilePhone::ms_PutToEar("PutToEar",0x1CD122B2);
const fwMvRequestId CTaskMobilePhone::ms_PutToText("PutToText",0xE0760A5A);
const fwMvRequestId CTaskMobilePhone::ms_PutToCamera("PutToCamera",0x34B21684);
const fwMvRequestId CTaskMobilePhone::ms_SwitchPhoneMode("SwitchPhoneMode",0x811C50C4);
const fwMvRequestId CTaskMobilePhone::ms_PutToEarTransit("PutToEarTransit",0xEBA264);
const fwMvRequestId CTaskMobilePhone::ms_PutToTextTransit("PutToTextTransit",0xD1B86607);
const fwMvRequestId CTaskMobilePhone::ms_AbortTransit("AbortTransit",0x8CFBCAFC);
const fwMvRequestId CTaskMobilePhone::ms_PutDownPhone("PutDownPhone",0xB36BE48E);
const fwMvRequestId CTaskMobilePhone::ms_TextLoop("TextLoop",0x4DEF8D5E);
const fwMvRequestId CTaskMobilePhone::ms_RestartLoop("RestartLoop",0x83545d70);
const fwMvRequestId CTaskMobilePhone::ms_HorizontalMode("HorizontalMode",0x76da852a);
const fwMvRequestId CTaskMobilePhone::ms_QuitHorizontalMode("QuitHorizontalMode",0xe1fe4af3);
const fwMvRequestId CTaskMobilePhone::ms_RequestSelectAnim("RequestSelectAnim",0x9bd78b56);
const fwMvRequestId CTaskMobilePhone::ms_RequestSwipeUpAnim("RequestSwipeUpAnim",0xf39cb195);
const fwMvRequestId CTaskMobilePhone::ms_RequestSwipeDownAnim("RequestSwipeDownAnim",0x6524372f);
const fwMvRequestId CTaskMobilePhone::ms_RequestSwipeLeftAnim("RequestSwipeLeftAnim",0xad627f34);
const fwMvRequestId CTaskMobilePhone::ms_RequestSwipeRightAnim("RequestSwipeRightAnim",0xe18f965e);

const fwMvFloatId CTaskMobilePhone::ms_FPSRestartBlend("FPSRestartBlend", 0xa7739d57);
const fwMvFloatId CTaskMobilePhone::ms_PutDownFromTextPhase("PutDownFromTextPhase", 0x6efd2ee3);
const fwMvFlagId CTaskMobilePhone::ms_InFirstPersonMode("InFirstPersonMode", 0xac835a68);

const fwMvBooleanId CTaskMobilePhone::ms_OnEnterPutUp("OnEnterPutUp",0xAC7A7A37);
const fwMvBooleanId CTaskMobilePhone::ms_OnEnterLoop("OnEnterLoop",0xE959E9B4);
const fwMvBooleanId CTaskMobilePhone::ms_OnEnterPutDown("OnEnterPutDown",0x51A222EF);
const fwMvBooleanId CTaskMobilePhone::ms_OnEnterTransit("OnEnterTransit",0xA7B0BB5F);
const fwMvBooleanId CTaskMobilePhone::ms_OnEnterPutToCamera("OnEnterPutToCamera",0xEB9C9507);
const fwMvBooleanId CTaskMobilePhone::ms_OnEnterPutDownToTextFromCamera("OnEnterPutDownToTextFromCamera",0xC4AD695D);
const fwMvBooleanId CTaskMobilePhone::ms_OnEnterHorizontalIntro("OnEnterHorizontalIntro",0x9739056b);
const fwMvBooleanId CTaskMobilePhone::ms_OnEnterHorizontalLoop("OnEnterHorizontalLoop",0xb60e6414);
const fwMvBooleanId CTaskMobilePhone::ms_OnEnterHorizontalOutro("OnEnterHorizontalOutro",0x5097bca1);

const fwMvStateId CTaskMobilePhone::ms_State_Start("Start",0x84DC271F);
const fwMvStateId CTaskMobilePhone::ms_State_PutUpText("PutUpText",0x128963DD);
const fwMvStateId CTaskMobilePhone::ms_State_PutToEarFromText("PutToEarFromText",0x668CAF65);
const fwMvStateId CTaskMobilePhone::ms_State_PutToEar("PutToEar",0x1CD122B2);
const fwMvStateId CTaskMobilePhone::ms_State_PutToCamera("PutToCamera",0x34B21684);
const fwMvStateId CTaskMobilePhone::ms_State_TextLoop("TextLoop",0x4DEF8D5E);
const fwMvStateId CTaskMobilePhone::ms_State_PutDownToText("PutDownToText",0xEB2E04D2);
const fwMvStateId CTaskMobilePhone::ms_State_EarLoop("EarLoop",0xC04C4563);
const fwMvStateId CTaskMobilePhone::ms_State_CameraLoop("CameraLoop",0xA3B1103F);
const fwMvStateId CTaskMobilePhone::ms_State_PutDownFromText("PutDownFromText",0x3D7CE6CD);
const fwMvStateId CTaskMobilePhone::ms_State_PutDownFromEar("PutDownFromEar",0x3B74C04);
const fwMvStateId CTaskMobilePhone::ms_State_PutDownFromCamera("PutDownFromCamera",0xF2051D4E);
const fwMvStateId CTaskMobilePhone::ms_State_PutToCameraFromText("PutToCameraFromText",0x76ECE256);
const fwMvStateId CTaskMobilePhone::ms_State_PutDownToTextFromCamera("PutDownToTextFromCamera",0x9B01FBDF);

//Additional secondary anims:
const fwMvRequestId CTaskMobilePhone::ms_AdditionalSecondaryAnimOn("AdditionalSecondaryAnim_On", 0x10287fa9);
const fwMvRequestId CTaskMobilePhone::ms_AdditionalSecondaryAnimOff("AdditionalSecondaryAnim_Off", 0xf22426d2);
const fwMvClipId CTaskMobilePhone::ms_AdditionalSecondaryAnimClip("AdditionalSecondaryAnim_Clip", 0x774a5ff2);
const fwMvFlagId CTaskMobilePhone::ms_AdditionalSecondaryAnimHoldLastFrame("AdditionalSecondaryAnim_HoldLastFrame", 0x68d3153d);
const fwMvBooleanId CTaskMobilePhone::ms_AdditionalSecondaryAnimIsLooped("AdditionalSecondaryAnim_IsLooped", 0x01c7884a);
const fwMvBooleanId CTaskMobilePhone::ms_AdditionalSecondaryAnimFinished("SecondaryAdditionalAnim_Finished", 0x7651a977);
const fwMvFilterId CTaskMobilePhone::ms_AdditionalSecondaryAnimFilter("AdditionalSecondaryAnim_Filter", 0xe0755914);
const fwMvFloatId CTaskMobilePhone::ms_AdditionalSecondaryAnimBlendInDuration("AdditionalSecondaryAnimBlendInRate", 0x23646a18);
const fwMvFloatId CTaskMobilePhone::ms_AdditionalSecondaryAnimBlendOutDuration("AdditionalSecondaryAnimBlendOutRate", 0x583a4922);
const fwMvFloatId CTaskMobilePhone::ms_AdditionalSecondaryAnimPhase("AdditionalSecondaryAnimPhase", 0x70496be1);
#if __BANK
bool CTaskMobilePhone::ms_bAdditionalSecondaryAnim_IsLooping = false;
bool CTaskMobilePhone::ms_bAdditionalSecondaryAnim_HoldLastFrame = false;
bool CTaskMobilePhone::sm_bAdditionalSecondaryAnim_Start = false;
bool CTaskMobilePhone::ms_bAdditionalSecondaryAnim_Stop = false;
char CTaskMobilePhone::m_AdditionalSecondaryAnim_DictName[64] = "mp_player_int_uppersalute"; 
char CTaskMobilePhone::m_AdditionalSecondaryAnim_ClipName[64] = "mp_player_int_salute"; 
float CTaskMobilePhone::ms_fDebugBlendInDuration = 0.25f;
float CTaskMobilePhone::ms_fDebugBlendOutDuration = 0.25f;
bool CTaskMobilePhone::ms_bDebugWaitingOnClips = false;
CClipRequestHelper CTaskMobilePhone::m_DebugSecondaryAnimRequestHelper;
bool CTaskMobilePhone::ms_bEnterHorizontalMode = false;
bool CTaskMobilePhone::ms_bExitHorizontalMode = false;
bool CTaskMobilePhone::ms_bPlaySelectAnim = false;
bool CTaskMobilePhone::ms_bPlayUpSwipeAnim = false;
bool CTaskMobilePhone::ms_bPlayDownSwipeAnim = false;
bool CTaskMobilePhone::ms_bPlayLeftSwipeAnim = false;
bool CTaskMobilePhone::ms_bPlayRightSwipeAnim = false;
#endif


const s32 CTaskMobilePhone::ms_AbortDuration = 1000;

#if !__FINAL
const char * CTaskMobilePhone::GetStaticStateName(s32 _iState)
{
	Assert(_iState>=0 && _iState <= State_Finish);
	static const char* lNames[] =    
	{
		"State_Init",
		"State_HolsterWeapon",
		"State_Start",
		"State_PutUpText",
		"State_TextLoop",
		"State_PutDownFromText",
		"State_PutToEar",
		"State_EarLoop",
		"State_PutDownFromEar",
		"State_PutToEarFromText",
		"State_PutDownToText",
		"State_PutUpCamera",
		"State_CameraLoop",
		"State_PutDownFromCamera",
		"State_PutToCameraFromText",
		"State_PutDownToTextFromCamera",
		"State_HorizontalIntro",
		"State_HorizontalLoop",
		"State_HorizontalOutro",
		"State_Paused",
		"State_Finish"
	};

	return lNames[_iState];
}
#endif // !__FINAL

bool CTaskMobilePhone::CanUseMobilePhoneTask( const CPed& in_Ped, PhoneMode in_Mode )
{
	if ( in_Mode == Mode_ToCamera )
	{
		static bool s_DisableCameraAnimsInCar = true;
		if ( in_Ped.GetIsInVehicle() && s_DisableCameraAnimsInCar )
		{
			return false;
		}
		return true;
	}
	else if ( in_Mode == Mode_ToText )
	{
		return !in_Ped.GetPedConfigFlag(CPED_CONFIG_FLAG_PhoneDisableTextingAnimations);
	}
	else // if ( in_Mode == Mode_ToCall )
	{
		return !in_Ped.GetPedConfigFlag(CPED_CONFIG_FLAG_PhoneDisableTalkingAnimations);
	}
}

void CTaskMobilePhone::CreateOrResumeMobilePhoneTask(CPed& in_Ped)
{
	CTask* pTask = in_Ped.GetPedIntelligence()->FindTaskActiveByTreeAndType(PED_TASK_TREE_SECONDARY, CTaskTypes::TASK_MOBILE_PHONE);
	if (!pTask)
	{
		in_Ped.GetPedIntelligence()->AddTaskSecondary(rage_new CTaskMobilePhone(Mode_ToText), PED_TASK_SECONDARY_PARTIAL_ANIM);
	}
	else
	{
		CTaskMobilePhone* pTaskMobilePhone = static_cast<CTaskMobilePhone*>(pTask);
		if ( pTaskMobilePhone->GetState() == State_Paused )
		{
			pTaskMobilePhone->ResetTaskMobilePhone(pTaskMobilePhone->GetPhoneMode());
		}

	}
}

bool CTaskMobilePhone::IsRunningMobilePhoneTask(const CPed& in_Ped)
{
	return	(in_Ped.GetPedIntelligence()->FindTaskActiveByTreeAndType(PED_TASK_TREE_SECONDARY, CTaskTypes::TASK_MOBILE_PHONE) != NULL) ||
			(in_Ped.GetPedIntelligence()->FindTaskActiveByTreeAndType(PED_TASK_TREE_PRIMARY, CTaskTypes::TASK_MOBILE_PHONE) != NULL);
}

CTaskMobilePhone::CTaskMobilePhone(s32 _PhoneMode, s32 _Duration, bool bIsUsingSecondarySlot, bool bAllowCloneToEquipPhone )
{
	m_StartupMode = static_cast<PhoneMode>(_PhoneMode);
	m_PhoneMode = static_cast<PhoneMode>(_PhoneMode);
	if (_Duration > 0)
	{
		m_Timer.Set(fwTimer::GetTimeInMilliseconds(), _Duration);
	}

	m_uPreviousSelectedWeapon = 0;
	m_HeadingBeforeUsingCamera = -1.f;
	m_bHasPhoneEquipped = false;
	m_bRequestGivePedPhone = false;
	m_bIsPhoneModelLoaded = false;
	m_bIsUsingSecondarySlot = bIsUsingSecondarySlot;
	m_bMoveAnimFinished = false;
	m_bMoveInterruptible = false;
	m_bAllowCloneToEquipPhone = bAllowCloneToEquipPhone;
	m_bAdditionalSecondaryAnim_Looped = false;
	m_bAdditionalSecondaryAnim_HoldLastFrame = false;
	m_bForceInstantIkSetup = false;
	m_fBlendInDuration = 0.25f;
	m_fBlendOutDuration = 0.25f;
	m_bWaitingOnClips = false;
	m_bIsPlayingAdditionalSecondaryAnim = false;
	m_fClipDuration = -1.0f;
	m_sVehicleWeaponIndex = -1;
	m_bWasInStealthMode = false;
	m_sAnimDictIndex = 0;
	m_uAnimHashKey = 0;
#if FPS_MODE_SUPPORTED
	m_vPrevPhoneIKOffset = Vector3::ZeroType;
#endif	//FPS_MODE_SUPPORTED
	SetInternalTaskType(CTaskTypes::TASK_MOBILE_PHONE);

#if FPS_MODE_SUPPORTED
	m_pFirstPersonIkHelper = NULL;
	m_bPrevTickInFirstPersonCamera = false;
#endif // FPS_MODE_SUPPORTED
}

CTaskMobilePhone::~CTaskMobilePhone()
{

}

void CTaskMobilePhone::CleanUp()
{
	CPed* pPed = GetPed();
	
	// Restore backup weapon in case we didn't actually create a mobile phone object (on bikes)
	if(pPed && !pPed->IsNetworkClone() && !m_bHasPhoneEquipped)
	{
		if (m_uPreviousSelectedWeapon != pPed->GetWeaponManager()->GetEquippedWeaponHash() )
		{
			pPed->GetWeaponManager()->EquipWeapon(m_uPreviousSelectedWeapon);
		}
		else if (m_sVehicleWeaponIndex != -1 && pPed->GetIsInVehicle())
		{
			pPed->GetWeaponManager()->EquipWeapon(0, m_sVehicleWeaponIndex);
		}
	}

	RemovePedPhone();
	BlendOutNetwork();

	// Reset horizontal mode flag to false (extra pre-caution in case not cleaned up properly by script).
	CPhoneMgr::ResetHorizontalModeFlag();

	if(GetPed() && GetPed()->IsNetworkClone() && m_bAllowCloneToEquipPhone)
	{
		CNetObjPed     *netObjPed  = static_cast<CNetObjPed *>(GetPed()->GetNetworkObject());
		if(netObjPed)
		{
			netObjPed->SetTaskOverridingProps(false);
		}
	}

#if FPS_MODE_SUPPORTED
	if (m_pFirstPersonIkHelper)
	{
		// If script have force cleared the task, instantly blend out the IK before we delete the IK helper
		if (pPed->GetPedResetFlag(CPED_RESET_FLAG_ScriptClearingPedTasks) && IsStateValidForFPSIK() && m_pFirstPersonIkHelper->GetBlendOutRate() != ARMIK_BLEND_RATE_INSTANT)
		{
			m_pFirstPersonIkHelper->SetBlendOutRate(ARMIK_BLEND_RATE_INSTANT);
			m_pFirstPersonIkHelper->Process(pPed);
		}
		delete m_pFirstPersonIkHelper;
		m_pFirstPersonIkHelper = NULL;
	}
#endif // FPS_MODE_SUPPORTED
}

void CTaskMobilePhone::ResetTaskMobilePhone(s32 NewMode, s32 NewDuration, bool RunInSecondarySlot)
{
		PhoneMode PM = static_cast<PhoneMode>(NewMode);
		m_PhoneMode = PM;
		if (NewDuration > 0)
		{
			m_Timer.Set(fwTimer::GetTimeInMilliseconds(), NewDuration);
		}

		m_HeadingBeforeUsingCamera = -1.f;
		m_bHasPhoneEquipped = false;
		m_bIsPhoneModelLoaded = false;
		m_bRequestGivePedPhone = false;
		m_bForceInstantIkSetup = false;
		m_bIsUsingSecondarySlot = RunInSecondarySlot;
		m_PhoneRequest.Reset();

#if __ASSERT
		SetCanChangeState(true);
#endif

		// make sure we finished init state first
		if (GetState() != State_Init )
		{	
			TASKMOBILEPHONE_SETSTATE(State_Start);
		}

}

bool CTaskMobilePhone::ShouldPause() const
{
	// Check that the Ped is trying to get into a car and is currently aligning itself to the car door.
	const CPed* pPed = GetPed();
	if ( pPed )
	{
		// this ped is ignored by everyone for steering purposes
		if(pPed->GetPedResetFlag( CPED_RESET_FLAG_DisableCellphoneAnimations ) )
		{
			return true;
		}

		if ( pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_IsSwimming ) )
		{
			return true;
		}

		if ( pPed->GetMyVehicle() )
		{
			const s32 iMySeat = pPed->GetMyVehicle()->GetSeatManager()->GetPedsSeatIndex(pPed);
			if (pPed->GetMyVehicle()->IsSeatIndexValid(iMySeat))
			{
				const CVehicleSeatAnimInfo* pSeatAnimInfo = pPed->GetMyVehicle()->GetSeatAnimationInfo(iMySeat);
				if (pSeatAnimInfo && pSeatAnimInfo->GetKeepCollisionOnWhenInVehicle())
				{
					return true;
				}
			}
		}

		CTaskParachute *pTaskParachute = (CTaskParachute *)pPed->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_PARACHUTE);
		if(pTaskParachute && pTaskParachute->GetState() != CTaskParachute::State_Parachuting)
		{
			return true;
		}

		static bool s_DisableCameraAnimsInCover = true;
		if ( s_DisableCameraAnimsInCover
			&& CPhoneMgr::CamGetState()  
			&& pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_UsingCoverPoint ) 
			&& !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PhoneDisableCameraAnimations) )
		{
			return true;
		}

		static bool s_DisableCameraAnimsInCar = true;
		if ( s_DisableCameraAnimsInCar && CPhoneMgr::CamGetState() )
		{
			if ( pPed->GetIsInVehicle() && !pPed->IsInFirstPersonVehicleCamera())
			{
				return true;
			}
		}
	}

	return false;
}

bool CTaskMobilePhone::ShouldUnPause() const
{
	return !ShouldPause();
}

void CTaskMobilePhone::BlendInNetwork()
{
	float fBlendDuration = SLOW_BLEND_DURATION;

#if FPS_MODE_SUPPORTED
	const CPed* pPed = GetPed();

	bool bInFirstPersonCamera = pPed->IsFirstPersonShooterModeEnabledForPlayer(false);

	bool bCameraChanged = (bInFirstPersonCamera != m_bPrevTickInFirstPersonCamera);
	bool bCamNotifiedChange = pPed->GetPlayerInfo() && pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_CAMERA_VIEW_MODE_SWITCHED_TO_OR_FROM_FIRST_PERSON) && !pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_DISABLE_CAMERA_VIEW_MODE_CYCLE);

	if (bCameraChanged || bCamNotifiedChange)
	{	
		fBlendDuration = 0.0f;
	}
#endif // FPS_MODE_SUPPORTED

	// Attach it to an empty precedence slot in fwMove
	Assert(GetPed()->GetAnimDirector());
	CMovePed& Move = GetPed()->GetMovePed();
	if (m_bIsUsingSecondarySlot)
	{
		Move.SetSecondaryTaskNetwork(m_NetworkHelper, fBlendDuration);
	}
	else
	{
		Move.SetTaskNetwork(m_NetworkHelper, fBlendDuration);
	}
}

void CTaskMobilePhone::BlendOutNetwork()
{
	CMovePed& Move = GetPed()->GetMovePed();
	static dev_float BLEND_OUT_DURATION = SLOW_BLEND_DURATION;
	if (m_bIsUsingSecondarySlot)
	{
#if FPS_MODE_SUPPORTED
		if (GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false) && GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_IsClimbingLadder))
		{
			Move.ClearSecondaryTaskNetwork(m_NetworkHelper, FAST_BLEND_DURATION);
		}
		else
#endif	//FPS_MODE_SUPPORTED
		{
			Move.ClearSecondaryTaskNetwork(m_NetworkHelper, BLEND_OUT_DURATION);
		}
	}
	else
	{
		Move.ClearTaskNetwork(m_NetworkHelper, BLEND_OUT_DURATION);
	}
}


bool CTaskMobilePhone::GivePedPhone()
{
	if ( ! m_bHasPhoneEquipped )
	{
		if (m_bIsPhoneModelLoaded)
		{
			CPed* pPed = GetPed();
			if (pPed->GetIsSwimming())
			{
				return true; //no model
			}
			pPed->SetPedResetFlag(CPED_RESET_FLAG_TemporarilyBlockWeaponSwitching, false);

			CWeaponItem* pItem = pPed->GetInventory() ? pPed->GetInventory()->AddWeapon(OBJECTTYPE_OBJECT) : NULL;
			if (pItem)
			{
				if (pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetEquippedWeapon() && pPed->GetWeaponManager()->GetEquippedWeapon()->GetIsCooking())
				{
					pPed->GetWeaponManager()->GetEquippedWeapon()->CancelCookTimer();
				}
				pItem->SetModelHash(CPedPhoneComponent::GetPhoneModelHashSafe(pPed->GetPhoneComponent()));	
				weaponAssert(pPed->GetWeaponManager());
				if (pPed->GetWeaponManager()->EquipWeapon(OBJECTTYPE_OBJECT))
				{
					pPed->GetWeaponManager()->CreateEquippedWeaponObject();

					if (pPed->GetWeaponManager()->GetEquippedWeaponObject())
					{
						pPed->GetWeaponManager()->GetEquippedWeaponObject()->m_nObjectFlags.bAmbientProp = true;
						pPed->SetPedResetFlag(CPED_RESET_FLAG_TemporarilyBlockWeaponSwitching, true);
						m_bHasPhoneEquipped = true;
						return true;
					}
				}
				else
				{
					pPed->GetInventory()->RemoveWeapon(OBJECTTYPE_OBJECT);
				}
			}

			pPed->SetPedResetFlag(CPED_RESET_FLAG_TemporarilyBlockWeaponSwitching, true);
		}
	}

	return false;
}

void CTaskMobilePhone::RemovePedPhone()
{
	if ( m_bHasPhoneEquipped )
	{
		CPed* pPed = GetPed();
		weaponAssert(pPed->GetInventory() && GetPed()->GetWeaponManager());
		pPed->GetInventory()->RemoveWeapon(OBJECTTYPE_OBJECT);
	
		// B*1485069 - Ensure the weapon object is destroyed 
		// If script call SET_PED_CURRENT_WEAPON while we're putting the phone away, the weapon object won't get
		// destroyed because we dont call DestroyEquippedWeaponObject in CPedWeaponManager::ItemRemoved(u32 uItemNameHash)
		// because if(uItemNameHash == GetEquippedWeaponHash()) doesn't evaluate to true anymore
		if (pPed->IsLocalPlayer())
		{
			pPed->GetWeaponManager()->DestroyEquippedWeaponObject(false);
		}

		// Reselect our previous weapon (it will be drawn when selecting the weapon in task player)
		if (m_uPreviousSelectedWeapon != pPed->GetWeaponManager()->GetEquippedWeaponHash() )
		{
			pPed->GetWeaponManager()->EquipWeapon(m_uPreviousSelectedWeapon);
		}
		else if (m_sVehicleWeaponIndex != -1 && pPed->GetIsInVehicle())
		{
			pPed->GetWeaponManager()->EquipWeapon(0, m_sVehicleWeaponIndex);
		}
	}
	m_bHasPhoneEquipped = false;
}

bool CTaskMobilePhone::RequestPhoneModel()
{
	const CPed* pPed = GetPed();

	u32 iPhoneModel = CPedPhoneComponent::GetPhoneModelIndexSafe(pPed->GetPhoneComponent());
	fwModelId PhoneModelId((strLocalIndex(iPhoneModel)));

	if (!CModelInfo::HaveAssetsLoaded(PhoneModelId))
	{
		return CModelInfo::RequestAssets(PhoneModelId, STRFLAG_FORCE_LOAD);
	}

	return true;
}

void CTaskMobilePhone::GetOverriddenGameplayCameraSettings(tGameplayCameraSettings& o_Settings) const
{
	const CPed* pPed = GetPed();
	
	if (pPed->GetUsingRagdoll()) 
	{
		return; 
	}

	if (CPhoneMgr::CamGetState() && (GetState() == State_PutUpCamera || GetState() == State_CameraLoop))
	{
		if (pPed->IsLocalPlayer())
		{
			// Wait for close shutter effect
			TUNE_GROUP_FLOAT(TASK_MOBILEPHONE, CameraSwitchDelay, 0.267f, 0.0f, 1.0f, 0.01f);
			TUNE_GROUP_INT(TASK_MOBILEPHONE, SelfieCameraSwitchDelay, 267, 0, 1000, 1);

			if (GetTimeInState() < CameraSwitchDelay)
			{
				return;
			}

			const u32 selfieModeActivationTime = CPhoneMgr::CamGetSelfieModeActivationTime();
			const u32 currentTime = fwTimer::GetTimeInMilliseconds();
			if ((selfieModeActivationTime > 0) && (currentTime >= selfieModeActivationTime) && (currentTime < (selfieModeActivationTime + (u32)SelfieCameraSwitchDelay)))
			{
				return;
			}

			//NOTE: We use a custom gameplay camera when the player is taking a 'selfie' (reverse angle) photo.
			const bool bShouldUseSelfieCamera = CPhoneMgr::CamGetSelfieModeState();
			o_Settings.m_CameraHash = bShouldUseSelfieCamera ? ATSTRINGHASH("CELL_PHONE_SELFIE_AIM_CAMERA", 0x01ab41687) : ATSTRINGHASH("CELL_PHONE_AIM_CAMERA", 0x064e68b46);

			if(bShouldUseSelfieCamera)
			{
				//Reset to the ideal orientation upon rendering the 'selfie' camera and ensure we see a clean cut back to a default orientation when we exit this camera.
				o_Settings.m_Flags.ClearFlag(Flag_ShouldClonePreviousOrientation | Flag_ShouldAllowNextCameraToCloneOrientation | Flag_ShouldAllowInterpolationToNextCamera);
				o_Settings.m_InterpolationDuration = 0;
			}
			else if(NetworkInterface::IsGameInProgress())
			{
				o_Settings.m_Flags.ClearFlag(Flag_ShouldClonePreviousOrientation);
			}
		}
	}
}

void CTaskMobilePhone::ProcessPreRender2()
{
	if(GetState() == State_CameraLoop)
	{
		if(m_NetworkHelper.IsInTargetState()) 
		{
			if(!m_Timer.IsOutOfTime() && !ShouldPause())
			{
				ProcessIK();
			}
		}
	}

#if FPS_MODE_SUPPORTED
	if(IsStateValidForFPSIK())
	{
		ProcessFPSArmIK();
	}
	else
	{
		if(m_pFirstPersonIkHelper)
		{
			m_pFirstPersonIkHelper->Reset();
		}
	}
#endif	//FPS_MODE_SUPPORTED
}

bool CTaskMobilePhone::ProcessPostCamera()
{
	CPed *pPed = GetPed();

	if (pPed->IsLocalPlayer())
	{
		if (CPhoneMgr::CamGetState())
		{
			const camGameplayDirector& GameplayDirector = camInterface::GetGameplayDirector();
			const camFirstPersonAimCamera* pAimCamera = GameplayDirector.GetFirstPersonAimCamera(pPed);
			if ((pAimCamera && camInterface::IsDominantRenderedCamera(*pAimCamera)))
			{
				// Update the ped's (desired) heading
				const camFrame& AimCameraFrame = GameplayDirector.GetFrame();
				float AimHeading = AimCameraFrame.ComputeHeading();
				pPed->SetDesiredHeading(AimHeading);
			}
		}

#if FPS_MODE_SUPPORTED
		// Update peds desired heading based on camera if in FPS mode 
		m_bIsPlayingAdditionalSecondaryAnim = pPed->IsFirstPersonShooterModeEnabledForPlayer(false);
		if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && !GetPed()->GetIsInVehicle() && !GetPed()->GetIsParachuting())
		{
			const camFirstPersonShooterCamera* pFpsCam = camInterface::GetGameplayDirector().GetFirstPersonShooterCamera();
			bool bLookingBehind = pFpsCam && (pFpsCam->IsDoingLookBehindTransition() || pFpsCam->IsLookingBehind());
			if (!bLookingBehind)
			{
				float aimHeading = fwAngle::LimitRadianAngleSafe(camInterface::GetPlayerControlCamHeading(*pPed));
				pPed->SetDesiredHeading(aimHeading);
			}
		}
#endif	//FPS_MODE_SUPPORTED
	}

	return false;
}

void CTaskMobilePhone::PutUpToEar()
{
	if ( m_PhoneRequest.GetAvailable() )
	{
		if ( m_PhoneRequest.IsEmpty() || m_PhoneRequest.Top() != Request_PutUpToEar )
		{
			m_PhoneRequest.Push(Request_PutUpToEar);
		}
	}
}

void CTaskMobilePhone::PutUpToCamera()
{
	if ( m_PhoneRequest.GetAvailable() )
	{
		if ( m_PhoneRequest.IsEmpty() ||  m_PhoneRequest.Top() != Request_PutUpToCamera )
		{
			m_PhoneRequest.Push(Request_PutUpToCamera);
		}
	}
}

void CTaskMobilePhone::PutDownToPrevState()
{
	if ( m_PhoneRequest.GetAvailable()  )
	{
		m_PhoneRequest.Push(Request_PutDownToPrevState);
	}
}

void CTaskMobilePhone::PutHorizontalIntro()
{
	if ( m_PhoneRequest.GetAvailable()  )
	{
		if ( m_PhoneRequest.IsEmpty() ||  m_PhoneRequest.Top() != Request_PutHorizontalIntro )
		{
			m_PhoneRequest.Push(Request_PutHorizontalIntro);
		}
	}
}

void CTaskMobilePhone::PutHorizontalOutro()
{
	if ( m_PhoneRequest.GetAvailable()  )
	{
		if ( m_PhoneRequest.IsEmpty() ||  m_PhoneRequest.Top() != Request_PutHorizontalOutro )
		{
			m_PhoneRequest.Push(Request_PutHorizontalOutro);
		}
	}
}

CTaskMobilePhone::PhoneMode CTaskMobilePhone::GetPhoneMode() const
{
	return m_PhoneMode;
}

void CTaskMobilePhone::GetPlayerSecondaryPartialAnimData(CSyncedPlayerSecondaryPartialAnim& data)
{
	data.m_PhoneSecondary = true;
	data.m_DictHash = m_sAnimDictIndex;
	data.m_ClipHash = m_uAnimHashKey;

	data.m_Float1 = m_fBlendInDuration;
	data.m_Float2 = m_fBlendOutDuration;

	data.m_Flags = 0;
	if (m_bAdditionalSecondaryAnim_HoldLastFrame)
	{
		data.m_Flags |= 0x01;
	}

	if (m_bAdditionalSecondaryAnim_Looped)
	{
		data.m_Flags |= 0x02;
	}

	data.m_BoneMaskHash = m_filterId;
}

void CTaskMobilePhone::SetAdditionalSecondaryAnimation(const crClip* pClip, crFrameFilter* pFilter)
{
	CPed* pPed = GetPed();
	if (pPed && pPed->GetAnimDirector())
	{
		CMovePed& movePed = pPed->GetMovePed();

		m_bIsPlayingAdditionalSecondaryAnim = true;

		movePed.SetFloat(ms_AdditionalSecondaryAnimBlendInDuration, m_fBlendInDuration);
		movePed.SetFloat(ms_AdditionalSecondaryAnimBlendOutDuration, m_fBlendOutDuration);

		movePed.BroadcastRequest(ms_AdditionalSecondaryAnimOn);
		movePed.SetClip(ms_AdditionalSecondaryAnimClip, pClip);
		movePed.SetFilter(ms_AdditionalSecondaryAnimFilter, pFilter);
		movePed.SetBoolean(ms_AdditionalSecondaryAnimIsLooped, m_bAdditionalSecondaryAnim_Looped);
		movePed.SetFlag(ms_AdditionalSecondaryAnimHoldLastFrame, m_bAdditionalSecondaryAnim_HoldLastFrame);
	}
}


void CTaskMobilePhone::ClearAdditionalSecondaryAnimation(float fBlendOutOverride)
{
	CPed* pPed = GetPed();
	if (pPed && pPed->GetAnimDirector())
	{
		if (fBlendOutOverride != -1.0f)
		{
			pPed->GetMovePed().SetFloat(ms_AdditionalSecondaryAnimBlendOutDuration, m_fBlendOutDuration);
		}
		pPed->GetMovePed().BroadcastRequest(ms_AdditionalSecondaryAnimOff);
		m_SecondaryAnimRequestHelper.ReleaseClips();
		m_bIsPlayingAdditionalSecondaryAnim = false;
		m_fClipDuration = -1.0f;
	}
}

float CTaskMobilePhone::GetAdditionalSecondaryAnimPhase()
{
	CPed *pPed = GetPed();
	if (pPed)
	{
		float fPhase = 0.0f;
		pPed->GetMovePed().GetFloat(ms_AdditionalSecondaryAnimPhase, fPhase);
		return fPhase;
	}
	return -1.0f;
}

void CTaskMobilePhone::RequestAdditionalSecondaryAnims(const char* pAnimDictHash, const char* pAnimName, const char *pFilterName, float fBlendInDuration, float fBlendOutDuration, bool bIsLooping, bool bHoldLastFrame)
{
	u32 dictHashkey = atHashString(pAnimDictHash);
	s32 animDictIndex = fwAnimManager::FindSlotFromHashKey(dictHashkey).Get();
	Assertf(animDictIndex != -1, "CTaskMobilePhone::RequestAdditionalSecondaryAnims: Invalid anim dictionary!");
	
	m_sAnimDictIndex = animDictIndex;
	m_uAnimHashKey = atHashString(pAnimName);
	m_pFilterName = pFilterName;

	m_bAdditionalSecondaryAnim_Looped = bIsLooping;
	m_bAdditionalSecondaryAnim_HoldLastFrame = bHoldLastFrame;

	m_fBlendInDuration = fBlendInDuration;
	m_fBlendOutDuration = fBlendOutDuration;

	m_SecondaryAnimRequestHelper.RequestClipsByIndex(animDictIndex);
	//Load clips if not loaded already
	if (!m_SecondaryAnimRequestHelper.HaveClipsBeenLoaded())
	{
		m_bWaitingOnClips = true;
	}
	else
	{
		GetAdditionalSecondaryAnimClipAndFilter();
	}
}

void CTaskMobilePhone::RemoteRequestAdditionalSecondaryAnims(const CSyncedPlayerSecondaryPartialAnim& data)
{
	m_sAnimDictIndex = data.m_DictHash;
	m_uAnimHashKey = data.m_ClipHash;
	m_filterId = data.m_BoneMaskHash;
	m_bAdditionalSecondaryAnim_HoldLastFrame = (data.m_Flags & 0x01) ? true : false;
	m_bAdditionalSecondaryAnim_Looped = (data.m_Flags & 0x02) ? true : false;
	m_fBlendInDuration = data.m_Float1;
	m_fBlendOutDuration = data.m_Float2;

	//ensure the m_pFilterName is cleared
	m_pFilterName = NULL;

	m_SecondaryAnimRequestHelper.RequestClipsByIndex(m_sAnimDictIndex);
	//Load clips if not loaded already
	if (!m_SecondaryAnimRequestHelper.HaveClipsBeenLoaded())
	{
		m_bWaitingOnClips = true;
	}
	else
	{
		GetAdditionalSecondaryAnimClipAndFilter();
	}
}

void CTaskMobilePhone::GetAdditionalSecondaryAnimClipAndFilter()
{
	const crClip* pClip = fwAnimManager::GetClipIfExistsByDictIndex(m_sAnimDictIndex, m_uAnimHashKey);
	Assertf(pClip, "Animation does not exist. Please ensure dictionary is streamed in! m_sAnimDictIndex: %d, m_uAnimHashKey: %d", m_sAnimDictIndex, m_uAnimHashKey);

	//Early out - no need to further process if !pClip as nothing will process at the bottom
	if (!pClip)
		return;

	crFrameFilter* pFilter = NULL;
	if (m_pFilterName)
	{
		const fwMvFilterId filterID(m_pFilterName);
		pFilter = g_FrameFilterDictionaryStore.FindFrameFilter(filterID);
	}
	else if (m_filterId)
	{
		const fwMvFilterId secondaryFilterId(m_filterId);
		pFilter = g_FrameFilterDictionaryStore.FindFrameFilter(secondaryFilterId);
	}

	if (pClip && pFilter)
	{
		if (m_pFilterName)
		{
			fwMvFilterId storefilterID(m_pFilterName);
			m_filterId = storefilterID.GetHash();
		}
		SetAdditionalSecondaryAnimation(pClip, pFilter);
		m_fClipDuration = pClip->GetDuration();
	}
}

#if __BANK
void CTaskMobilePhone::InitWidgets()
{
	bkBank* bank = BANKMGR.FindBank("Mobile Phone");
	if(!bank)
	{
		Assertf(bank, "Cannot find mobile phone bank.");
		return;
	}
	bank->PushGroup("Phone Gestures", false);
	bank->AddText("Dictionary Name:", m_AdditionalSecondaryAnim_DictName, 64, false);
	bank->AddText("Clip Name:", m_AdditionalSecondaryAnim_ClipName, 64, false);
	bank->AddSlider("Blend In Duration:", &ms_fDebugBlendInDuration, 0.0f, 1.0f, 0.01f);
	bank->AddSlider("Blend Out Duration:", &ms_fDebugBlendOutDuration, 0.0f, 1.0f, 0.01f);
	bank->AddButton("Send Request", TestSetAdditionalSecondaryAnimCommand);
	bank->AddToggle("Toggle Looping", &ms_bAdditionalSecondaryAnim_IsLooping);
	bank->AddToggle("Toggle HoldLastFrame", &ms_bAdditionalSecondaryAnim_HoldLastFrame);
	bank->AddButton("Disable Gesture", TestClearAdditionalSecondaryAnimCommand);
	bank->PopGroup();

	bank->PushGroup("Phone FPS Mode", false);
	bank->AddToggle("Toggle Horizontal On", &ms_bEnterHorizontalMode);
	bank->AddToggle("Toggle Horizontal Off", &ms_bExitHorizontalMode);
	bank->AddToggle("Play Select Anim", &ms_bPlaySelectAnim);
	bank->AddToggle("Play Swipe_Up Anim", &ms_bPlayUpSwipeAnim);
	bank->AddToggle("Play Swipe_Down Anim", &ms_bPlayDownSwipeAnim);
	bank->AddToggle("Play Swipe_Left Anim", &ms_bPlayLeftSwipeAnim);
	bank->AddToggle("Play Swipe_Right Anim", &ms_bPlayRightSwipeAnim);
	bank->PopGroup();
}

void CTaskMobilePhone::TestSetAdditionalSecondaryAnimCommand()
{
	sm_bAdditionalSecondaryAnim_Start = true;
}

void CTaskMobilePhone::TestClearAdditionalSecondaryAnimCommand()
{
	ms_bAdditionalSecondaryAnim_Stop = true;
}

void CTaskMobilePhone::TestAdditionalSecondaryAnimCommand()
{
	//Dictionary & Clip:
	u32 animDictHash = atHashString(m_AdditionalSecondaryAnim_DictName);
	s32 animDictIndex = fwAnimManager::FindSlotFromHashKey(animDictHash).Get();
	Assertf(animDictIndex!=-1, "Anim dictionary is incorrect/invalid");

	m_fBlendInDuration = ms_fDebugBlendInDuration;
	m_fBlendOutDuration = ms_fDebugBlendOutDuration;
	m_bAdditionalSecondaryAnim_Looped = ms_bAdditionalSecondaryAnim_IsLooping;
	m_bAdditionalSecondaryAnim_HoldLastFrame = ms_bAdditionalSecondaryAnim_HoldLastFrame;
	
	m_DebugSecondaryAnimRequestHelper.RequestClipsByIndex(animDictIndex);
	//Wait for clips if they've not been loaded, else call the Set function directly
	if (!m_DebugSecondaryAnimRequestHelper.HaveClipsBeenLoaded())
	{
		ms_bDebugWaitingOnClips = true;
	}
	else
	{
		TestSetAdditionalSecondaryAnims();
	}
}

void CTaskMobilePhone::TestSetAdditionalSecondaryAnims()
{
	//Dictionary & Clip:
	u32 animDictHash = atHashString(m_AdditionalSecondaryAnim_DictName);
	u32 animHashKey = atHashString(m_AdditionalSecondaryAnim_ClipName);
	s32 animDictIndex = fwAnimManager::FindSlotFromHashKey(animDictHash).Get();
	Assertf(animDictIndex!=-1, "Anim dictionary is incorrect/invalid");

	if (m_DebugSecondaryAnimRequestHelper.HaveClipsBeenLoaded())
	{
		const crClip *pClip = fwAnimManager::GetClipIfExistsByDictIndex(animDictIndex, animHashKey);
		Assertf(pClip, "Animation '%s' does not exist", m_AdditionalSecondaryAnim_ClipName);

		//Filter:
		const fwMvFilterId passedInFilterID("BONEMASK_HEAD_NECK_AND_L_ARM");
		crFrameFilter* pFilter = g_FrameFilterDictionaryStore.FindFrameFilter(passedInFilterID);

		if (pClip && pFilter)
		{
			SetAdditionalSecondaryAnimation(pClip, pFilter);
			m_fClipDuration = pClip->GetDuration();
		}
	}
}
#endif


CTask::FSM_Return CTaskMobilePhone::ProcessPreFSM()
{	
	CPed& ped = *GetPed();

	ped.SetPedResetFlag(CPED_RESET_FLAG_ProcessPreRender2, true);

	// B*2145181: Ensure phone prop gets created as it may not have been loaded in time before the "CreatePhone" anim tag on the intro anim was hit.
	if (ShouldCreatePhoneProp())
	{
		GivePedPhone();
	}

	bool bInFPSMode = false;

	if (ped.GetPedModelInfo() && ped.GetPedModelInfo()->IsUsingFemaleSkeleton())
	{
		m_ClipSetRequestHelper.Request(CLIP_SET_CELLPHONE_FEMALE);
	}
	else
	{
#if FPS_MODE_SUPPORTED
		if(ped.GetIsParachuting())
		{
			m_ParachuteClipSetRequestHelper.Request(CLIP_SET_CELLPHONE_PARACHUTE_FPS);
		}
		
		m_OnFootFPSClipSetRequestHelper.Request(CLIP_SET_CELLPHONE_FPS);

		bInFPSMode = ped.IsFirstPersonShooterModeEnabledForPlayer(false);
		if (bInFPSMode && !ped.GetIsInCover())
		{
			// Flag us aiming and strafing so we can use TaskMotionAiming movement
			if(!GetPed()->GetIsInVehicle() && !ped.GetIsParachuting() && GetState() != State_CameraLoop && GetState() != State_Paused)
			{
				ped.SetPedResetFlag(CPED_RESET_FLAG_SkipAimingIdleIntro, true);	// Skip the idle intro when pulling out the phone (B*2027640).
				ped.SetPedResetFlag(CPED_RESET_FLAG_IsAiming, true);
				ped.SetIsStrafing(true);
			}
		}
#endif	//FPS_MODE_SUPPORTED

		m_ClipSetRequestHelper.Request(CLIP_SET_CELLPHONE);
	}

	//! process stealth clip sets.
	if(ped.GetMotionData()->GetUsingStealth())
	{
		m_StealthClipSetRequestHelper.Request(CLIP_SET_CELLPHONE_STEALTH);
	}

	// If in car, use in-car clip sets (now defined in vehiclelayouts.meta)
	if (ped.GetIsInVehicle())
	{
		CVehicle *pVehicle = static_cast<CVehicle*>(ped.GetVehiclePedInside());
		if (pVehicle)
		{
			const CVehicleLayoutInfo *pVehLayoutInfo = static_cast<const CVehicleLayoutInfo*>(pVehicle->GetLayoutInfo());
			if (pVehLayoutInfo)
			{
				aiDebugf2("CTaskMobilePhone::ProcessPreFSM - Getting cell phone clipset from layout %s\n", pVehLayoutInfo->GetName().GetCStr());
				if (ped.GetIsDrivingVehicle())
				{
					if (FPS_MODE_SUPPORTED_ONLY(bInFPSMode ||) ped.IsInFirstPersonVehicleCamera())
					{
						m_InCarDriverClipSetRequestHelper.Request(pVehLayoutInfo->GetCellphoneClipsetDS_FPS());
					}
					else
					{
						m_InCarDriverClipSetRequestHelper.Request(pVehLayoutInfo->GetCellphoneClipsetDS());
					}
				}
				else
				{
					if (FPS_MODE_SUPPORTED_ONLY(ped.IsFirstPersonShooterModeEnabledForPlayer(false) ||) ped.IsInFirstPersonVehicleCamera())
					{
						m_InCarPassengerClipSetRequestHelper.Request(pVehLayoutInfo->GetCellphoneClipsetPS_FPS());
					}
					else
					{
						m_InCarPassengerClipSetRequestHelper.Request(pVehLayoutInfo->GetCellphoneClipsetPS());
					}
				}
			}
		}
	}

	//! If desired clip set changes, update network clip set.
	if(m_NetworkHelper.IsNetworkActive())
	{
		fwClipSetRequestHelper *pRequestHelper = 
#if FPS_MODE_SUPPORTED
			bInFPSMode ? &m_OnFootFPSClipSetRequestHelper :
#endif 	//FPS_MODE_SUPPORTED
			(ped.GetMotionData()->GetUsingStealth() ? &m_StealthClipSetRequestHelper : &m_ClipSetRequestHelper);

#if FPS_MODE_SUPPORTED
		if(bInFPSMode && ped.GetIsParachuting())
		{
			pRequestHelper = &m_ParachuteClipSetRequestHelper;
		}
#endif

		if (ped.GetIsInVehicle())
		{
			pRequestHelper = ped.GetIsDrivingVehicle() ? &m_InCarDriverClipSetRequestHelper : &m_InCarPassengerClipSetRequestHelper;
		}

		if(pRequestHelper && pRequestHelper->IsLoaded() && m_NetworkHelper.GetClipSetId() != pRequestHelper->GetClipSetId())
		{
#if FPS_MODE_SUPPORTED
			// Instantly blend to FPS anims when switching into FPS mode 
			if (bInFPSMode)
			{
				TUNE_GROUP_FLOAT(FIRST_PERSON_PHONE, fFPSRestartBlend, 0.0f, 0.0f, 5.0f, 0.01f);
				m_NetworkHelper.SetFloat(ms_FPSRestartBlend, fFPSRestartBlend);		
			}
#endif	//FPS_MODE_SUPPORTED
			if (!bInFPSMode)
			{
				TUNE_GROUP_FLOAT(FIRST_PERSON_PHONE, fFPSRestartBlendThirdPerson, 0.25f, 0.0f, 5.0f, 0.01f);
				float fRestartBlendTime = fFPSRestartBlendThirdPerson;
				// B*2075099: Instantly blend to third person anims when switching camera mode from FPS->TPS.
				if (ped.GetPlayerInfo() && ped.GetPlayerResetFlag(CPlayerResetFlags::PRF_CAMERA_VIEW_MODE_SWITCHED_TO_OR_FROM_FIRST_PERSON))
				{
					fRestartBlendTime = 0.0f;
				}
				m_NetworkHelper.SetFloat(ms_FPSRestartBlend, fRestartBlendTime);
			}
			m_NetworkHelper.SetClipSet(pRequestHelper->GetClipSetId()); 
			m_NetworkHelper.SendRequest(ms_RestartLoop);
		}

#if FPS_MODE_SUPPORTED
		// Ensure we clear the alternate filter flag if we aren't in FPS mode
		if (!GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false) && !GetPed()->IsInFirstPersonVehicleCamera())
		{
			m_NetworkHelper.SetFlag(false, ms_UseAlternateFilter);
		}
#endif	//FPS_MODE_SUPPORTED
	}

	if (RequestPhoneModel())
	{
		m_bIsPhoneModelLoaded = true;
	}

	if(ped.IsNetworkClone() && m_bAllowCloneToEquipPhone)
	{
		CNetObjPed     *netObjPed  = static_cast<CNetObjPed *>(ped.GetNetworkObject());
		if(netObjPed)
		{
			netObjPed->SetTaskOverridingProps(true);
		}
	}

	ProcessClipMoveEventTags();	

	// Prevent ped from switching weapons when using phone
	ped.SetPedResetFlag(CPED_RESET_FLAG_TemporarilyBlockWeaponSwitching, true);
	ped.SetPedResetFlag(CPED_RESET_FLAG_TemporarilyBlockWeaponEquipping, true);
	ped.SetPedResetFlag(CPED_RESET_FLAG_UsingMobilePhone, true);
	ped.SetPedResetFlag(CPED_RESET_FLAG_ProcessPostCamera, true);

	// Don't allow camera view mode changes when using the phone camera
	if (CPhoneMgr::CamGetState() && ped.IsLocalPlayer() && ped.GetPlayerInfo())
	{
		ped.SetPlayerResetFlag(CPlayerResetFlags::PRF_DISABLE_CAMERA_VIEW_MODE_CYCLE);
	}

	if ( (m_NetworkHelper.HasNetworkExpired() || !m_NetworkHelper.IsNetworkActive() )
		&& GetState() != State_Paused 
		&& GetState() != State_Init
		&& GetState() != State_Start
#if FPS_MODE_SUPPORTED
		&& (!ped.IsNetworkClone() && (GetState() != State_HolsterWeapon || !ped.IsFirstPersonShooterModeEnabledForPlayer(false, true)))	// Need to wait the holster anim to finish in FPS - only for local ped/player - disregard for remotes
#endif // FPS_MODE_SUPPORTED
		)
	{
		return FSM_Quit;
	}
		

	if ( ped.IsLocalPlayer() )
	{
		if ( !CPhoneMgr::IsDisplayed() 
			|| CPhoneMgr::GetGoingOffScreen()  )
		{
			m_StartupMode = Mode_None;
			PutDownToPrevState();
		}
		 
		if ( CPhoneMgr::CamGetState() )
		{
			if ( GetState() == State_TextLoop || GetState() == State_EarLoop || GetState() == State_HorizontalIntro || GetState() == State_HorizontalLoop || GetState() == State_HorizontalOutro)
			{
				PutUpToCamera();
			}
		}
		else if (CPhoneMgr::GetHorizontalModeActive() && GetState() == State_TextLoop)
		{
			PutHorizontalIntro();
		}
		else if (!CPhoneMgr::GetHorizontalModeActive() && GetState() == State_HorizontalLoop)
		{
			PutHorizontalOutro();
		}
		else
		{
			if ( GetState() == State_PutToCameraFromText || GetState() == State_CameraLoop || GetState() == State_PutUpCamera )  
			{
				PutDownToPrevState();
			}
		}

		// Trigger thumb anim if we have a new input from the player
		if (CPhoneMgr::GetLatestPhoneInput() != CELL_INPUT_NONE && m_NetworkHelper.IsNetworkActive())
		{
			switch (CPhoneMgr::GetLatestPhoneInput())
			{
			case CELL_INPUT_SELECT:
				m_NetworkHelper.SendRequest(ms_RequestSelectAnim);
				break;
			case CELL_INPUT_UP:
				m_NetworkHelper.SendRequest(ms_RequestSwipeUpAnim);
				break;
			case CELL_INPUT_DOWN:
				m_NetworkHelper.SendRequest(ms_RequestSwipeDownAnim);
				break;
			case CELL_INPUT_LEFT:
				m_NetworkHelper.SendRequest(ms_RequestSwipeLeftAnim);
				break;
			case CELL_INPUT_RIGHT:
				m_NetworkHelper.SendRequest(ms_RequestSwipeRightAnim);
				break;
			default:
				break;
			}
			// Reset input value
			CPhoneMgr::SetLatestPhoneInput(INPUT_NONE);
		}
	}
	
	if (ped.GetMovePed().GetBoolean(ms_AdditionalSecondaryAnimFinished) && !m_bAdditionalSecondaryAnim_HoldLastFrame)
	{
		ClearAdditionalSecondaryAnimation();
	}

	if (m_bWaitingOnClips)
	{
		if (m_SecondaryAnimRequestHelper.HaveClipsBeenLoaded())
		{
			m_bWaitingOnClips = false;
			GetAdditionalSecondaryAnimClipAndFilter();
		}
	}

	//Update the secondary anim blend in/out rates every frame so they don't get reset
	if (GetState() == State_CameraLoop)
	{
		ped.GetMovePed().SetFloat(ms_AdditionalSecondaryAnimBlendInDuration, m_fBlendInDuration);
		ped.GetMovePed().SetFloat(ms_AdditionalSecondaryAnimBlendOutDuration, m_fBlendOutDuration);
	}

	// Block camera transition in certain states
	switch(GetState())
	{
	case(State_Init):
	case(State_HolsterWeapon):
	case(State_PutUpText):
	case(State_PutDownFromText):
	case(State_PutUpCamera):
	case(State_CameraLoop):
	case(State_PutDownFromCamera):
	case(State_HorizontalIntro):
	case(State_HorizontalOutro):
#if FPS_MODE_SUPPORTED
		if(ped.IsLocalPlayer() && ped.GetPlayerInfo())
		{
			ped.SetPlayerResetFlag(CPlayerResetFlags::PRF_DISABLE_CAMERA_VIEW_MODE_CYCLE);
		}
#endif // FPS_MODE_SUPPORTED
		break;
	default:
		break;
	}


#if __BANK
	if(sm_bAdditionalSecondaryAnim_Start)
	{
		sm_bAdditionalSecondaryAnim_Start = false;
		TestAdditionalSecondaryAnimCommand();
	}
	if(ms_bAdditionalSecondaryAnim_Stop)
	{
		ms_bAdditionalSecondaryAnim_Stop = false;
		ClearAdditionalSecondaryAnimation();
	}

	if (ms_bDebugWaitingOnClips)
	{
		if (m_DebugSecondaryAnimRequestHelper.HaveClipsBeenLoaded())
		{
			ms_bDebugWaitingOnClips = false;
			TestSetAdditionalSecondaryAnims();
		}
	}

	if (ms_bEnterHorizontalMode)
	{
		ms_bEnterHorizontalMode = false;
		ms_bExitHorizontalMode = false;
		if (GetState() != State_HorizontalIntro && GetState() != State_HorizontalLoop && GetState() != State_HorizontalOutro)
		{
			CPhoneMgr::HorizontalActivate(true);
		}
	}

	if (ms_bExitHorizontalMode)
	{
		ms_bEnterHorizontalMode = false;
		ms_bExitHorizontalMode = false;
		if (GetState() == State_HorizontalLoop)
		{
			CPhoneMgr::HorizontalActivate(false);
		}
	}

	if (ms_bPlaySelectAnim)
	{
		ms_bPlaySelectAnim = false;
		CPhoneMgr::SetLatestPhoneInput(CELL_INPUT_SELECT);
	}
	else if (ms_bPlayUpSwipeAnim)
	{
		ms_bPlayUpSwipeAnim = false;
		CPhoneMgr::SetLatestPhoneInput(CELL_INPUT_UP);
	}
	else if (ms_bPlayDownSwipeAnim)
	{
		ms_bPlayDownSwipeAnim = false;
		CPhoneMgr::SetLatestPhoneInput(CELL_INPUT_DOWN);
	}
	else if (ms_bPlayLeftSwipeAnim)
	{
		ms_bPlayLeftSwipeAnim = false;
		CPhoneMgr::SetLatestPhoneInput(CELL_INPUT_LEFT);
	}
	else if (ms_bPlayRightSwipeAnim)
	{
		ms_bPlayRightSwipeAnim = false;
		CPhoneMgr::SetLatestPhoneInput(CELL_INPUT_RIGHT);
	}

#endif
	
	return FSM_Continue;
}

CTask::FSM_Return CTaskMobilePhone::ProcessPostFSM()
{
	CPed& ped = *GetPed();

	// clear flag after it's checked
	ped.SetPedResetFlag(CPED_RESET_FLAG_DisableCellphoneAnimations, false);

	// Save first person camera state for next tick
#if FPS_MODE_SUPPORTED
	m_bPrevTickInFirstPersonCamera = ped.IsFirstPersonShooterModeEnabledForPlayer(false);
#endif // FPS_MODE_SUPPORTED

	return FSM_Continue;
}

CTask::FSM_Return CTaskMobilePhone::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

	FSM_State(State_Init)
		FSM_OnEnter
			Init_OnEnter();
		FSM_OnUpdate
			return Init_OnUpdate();	

		FSM_State(State_HolsterWeapon)
			FSM_OnEnter
				HolsterWeapon_OnEnter();
			FSM_OnUpdate
				return HolsterWeapon_OnUpdate();	

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();		

		FSM_State(State_PutUpText)
			FSM_OnEnter
				PutUpText_OnEnter();
			FSM_OnUpdate
				return PutUpText_OnUpdate();	
			 
		FSM_State(State_TextLoop)
			FSM_OnEnter
				TextLoop_OnEnter();
			FSM_OnUpdate
				return TextLoop_OnUpdate();	

		FSM_State(State_PutDownFromText)
			FSM_OnEnter
				PutDownFromText_OnEnter();
			FSM_OnUpdate
				return PutDownFromText_OnUpdate();	

		FSM_State(State_PutToEar)
			FSM_OnEnter
				PutToEar_OnEnter();
			FSM_OnUpdate
				return PutToEar_OnUpdate();	

		FSM_State(State_EarLoop)
			FSM_OnEnter
				EarLoop_OnEnter();
			FSM_OnUpdate
				return EarLoop_OnUpdate();	

		FSM_State(State_PutDownFromEar)
			FSM_OnEnter
				PutDownFromEar_OnEnter();
			FSM_OnUpdate
				return PutDownFromEar_OnUpdate();	

		FSM_State(State_PutToEarFromText)
			FSM_OnEnter
				PutToEarFromText_OnEnter();
			FSM_OnUpdate
				return PutToEarFromText_OnUpdate();	

		FSM_State(State_PutDownToText)
			FSM_OnEnter
				PutDownToText_OnEnter();
			FSM_OnUpdate
				return PutDownToText_OnUpdate();	

		FSM_State(State_PutUpCamera)
			FSM_OnEnter
				PutUpCamera_OnEnter();
		FSM_OnUpdate
			return PutUpCamera_OnUpdate();	

		FSM_State(State_CameraLoop)
			FSM_OnEnter
				CameraLoop_OnEnter();
		FSM_OnUpdate
			return CameraLoop_OnUpdate();	
		FSM_OnExit
			return CameraLoop_OnExit();

		FSM_State(State_PutDownFromCamera)
			FSM_OnEnter
				PutDownFromCamera_OnEnter();
		FSM_OnUpdate
			return PutDownFromCamera_OnUpdate();	

		FSM_State(State_PutToCameraFromText)
			FSM_OnEnter
				PutToCameraFromText_OnEnter();
		FSM_OnUpdate
			return PutToCameraFromText_OnUpdate();	

		FSM_State(State_PutDownToTextFromCamera)
			FSM_OnEnter
				PutDownToTextFromCamera_OnEnter();
			FSM_OnUpdate
				return PutDownToTextFromCamera_OnUpdate();	

			FSM_State(State_HorizontalIntro)
				FSM_OnEnter
				HorizontalIntro_OnEnter();
			FSM_OnUpdate
				return HorizontalIntro_OnUpdate();	

			FSM_State(State_HorizontalLoop)
				FSM_OnEnter
				HorizontalLoop_OnEnter();
			FSM_OnUpdate
				return HorizontalLoop_OnUpdate();	

			FSM_State(State_HorizontalOutro)
				FSM_OnEnter
				HorizontalOutro_OnEnter();
			FSM_OnUpdate
				return HorizontalOutro_OnUpdate();	

		FSM_State(State_Paused)
			FSM_OnEnter
				Paused_OnEnter();
			FSM_OnUpdate
				return Paused_OnUpdate();	

		FSM_State(State_Finish)
			FSM_OnUpdate
				return Finish_OnUpdate();


	FSM_End
}

bool CTaskMobilePhone::ProcessMoveSignals()
{
	if(!CPedAILodManager::ShouldDoFullUpdate(*GetPed()))
	{
		ProcessClipMoveEventTags();
	}

#if FPS_MODE_SUPPORTED
	if (m_NetworkHelper.IsNetworkActive())
	{
		// Set FPS flag (have to check view mode context as well as we aren't flagged as "in" FPS mode when in the camera states
		const int viewMode = camControlHelper::GetViewModeForContext(camControlHelperMetadataViewMode::ON_FOOT);
		bool bInFPSMode = (GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false) || viewMode == camControlHelperMetadataViewMode::FIRST_PERSON);
		m_NetworkHelper.SetFlag(bInFPSMode, ms_InFirstPersonMode);
	}
#endif	//FPS_MODE_SUPPORTED

	//Check the state.
	switch(GetState())
	{
		case State_PutUpText:
		{
			PutUpText_OnProcessMoveSignals();
			break;
		}
		case State_PutDownFromText:
		{
			PutDownFromText_OnProcessMoveSignals();
			break;
		}
		case State_PutToEar:
		{
			PutToEar_OnProcessMoveSignals();
			break;
		}
		case State_PutDownFromEar:
		{
			PutDownFromEar_OnProcessMoveSignals();
			break;
		}
		case State_PutToEarFromText:
		{
			PutToEarFromText_OnProcessMoveSignals();
			break;
		}
		case State_PutDownToText:
		{
			PutDownToText_OnProcessMoveSignals();
			break;
		}
		case State_PutUpCamera:
		{
			PutUpCamera_OnProcessMoveSignals();
			break;
		}
		case State_PutDownFromCamera:
		{
			PutDownFromCamera_OnProcessMoveSignals();
			break;
		}
		case State_HorizontalIntro:
		{
			HorizontalIntro_OnProcessMoveSignals();
			break;
		}
		case State_HorizontalLoop:
		{
			HorizontalLoop_OnProcessMoveSignals();
			break;
		}
		case State_HorizontalOutro:
		{
			HorizontalOutro_OnProcessMoveSignals();
			break;
		}
		default:
		{
			break;
		}
	}

	return true;
}

void CTaskMobilePhone::Init_OnEnter()
{
	m_NetworkHelper.RequestNetworkDef(CClipNetworkMoveInfo::ms_NetworkTaskMobilePhone);

	RequestProcessMoveSignalCalls();
}

CTask::FSM_Return CTaskMobilePhone::Init_OnUpdate()
{
	//! The stealth clip set falls back to the default clip set, so just ensure both are loaded.
	bool bLoaded = m_ClipSetRequestHelper.IsLoaded();
	if(bLoaded && GetPed()->GetMotionData()->GetUsingStealth())
	{
		bLoaded = m_StealthClipSetRequestHelper.IsLoaded();
	}

#if FPS_MODE_SUPPORTED
	if(bLoaded && GetPed()->GetIsParachuting())
	{
		bLoaded = m_ParachuteClipSetRequestHelper.IsLoaded();
	}

	if (bLoaded && GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		bLoaded = m_OnFootFPSClipSetRequestHelper.IsLoaded();
	}
#endif

	//! Check to make sure the relevant in-car clipset is loaded as well if required.
	if(bLoaded && GetPed()->GetIsInVehicle())
	{
		bLoaded = GetPed()->GetIsDrivingVehicle() ? m_InCarDriverClipSetRequestHelper.IsLoaded() : m_InCarPassengerClipSetRequestHelper.IsLoaded();
	}

	if (bLoaded && m_NetworkHelper.IsNetworkDefStreamedIn(CClipNetworkMoveInfo::ms_NetworkTaskMobilePhone))
	{	
		CPed* pPed = GetPed();
		if ( pPed )
		{
			if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PhoneDisableTextingAnimations)
				|| pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PhoneDisableTalkingAnimations))
			{
				if ( CPhoneMgr::CamGetState()  )
				{
					m_PhoneMode = Mode_ToCamera;
				}
				else
				{
					return FSM_Quit;					
				}
			}
		}

		TASKMOBILEPHONE_SETSTATE(State_HolsterWeapon);
	}

	return FSM_Continue;
}
 
void CTaskMobilePhone::HolsterWeapon_OnEnter()
{
	CPed* pPed = GetPed();
	if ( pPed )
	{
		weaponAssert(pPed->GetWeaponManager());
		m_uPreviousSelectedWeapon = pPed->GetWeaponManager()->GetSelectedWeaponHash();

		if (m_uPreviousSelectedWeapon == 0 && pPed->GetIsInVehicle() && pPed->GetWeaponManager()->GetEquippedVehicleWeapon())
		{
			m_sVehicleWeaponIndex = pPed->GetWeaponManager()->GetEquippedVehicleWeaponIndex();
		}

		// Destroy the current weapon object
		if (m_uPreviousSelectedWeapon != 0)
		{
			weaponAssert(pPed->GetWeaponManager());
			pPed->GetWeaponManager()->EquipWeapon( pPed->GetDefaultUnarmedWeaponHash() );

			int s_flags = SWAP_HOLSTER|SWAP_ABORT_SET_DRAWN|SWAP_SECONDARY;
#if FPS_MODE_SUPPORTED
			// In FPS, we directly go to draw state to holster current equipped weapon.
			if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true))
			{
				s_flags &= ~SWAP_HOLSTER;
				s_flags |= SWAP_DRAW;
			}
#endif // FPS_MODE_SUPPORTED

			// Skip animated holster for weapons that force it
			const CWeaponInfo* pPreviousWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_uPreviousSelectedWeapon);
			if (pPreviousWeaponInfo && pPreviousWeaponInfo->GetUseHolsterAnimation())
			{
				s_flags |= SWAP_INSTANTLY;
			}

			SetNewTask(rage_new CTaskSwapWeapon(s_flags));
		}
	}
}

CTask::FSM_Return CTaskMobilePhone::HolsterWeapon_OnUpdate()
{
	if(GetIsFlagSet(aiTaskFlags::SubTaskFinished) || !GetSubTask())
	{
		TASKMOBILEPHONE_SETSTATE(State_Start);
	}

	return FSM_Continue;
}


CTask::FSM_Return CTaskMobilePhone::Start_OnUpdate()
{
	// This is a special case because we haven't blended in the move network yet.
	// So use the less restrictive non unpause instead of pause
	if ( !ShouldUnPause() )
	{
		TASKMOBILEPHONE_SETSTATE(State_Paused);
		return FSM_Continue;
	}

	CPed* pPed = GetPed();
	if (pPed && pPed->GetPedIntelligence())
	{
		//	Don't start if ragdolling
		if (pPed->GetUsingRagdoll())
		{
			return FSM_Continue;
		}

		//! Get clip set.
		fwMvClipSetId ClipSetID = m_StealthClipSetRequestHelper.IsLoaded() ? m_StealthClipSetRequestHelper.GetClipSetId() : m_ClipSetRequestHelper.GetClipSetId();

#if FPS_MODE_SUPPORTED
		if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
		{
			ClipSetID = pPed->GetIsParachuting() ? m_ParachuteClipSetRequestHelper.GetClipSetId() : m_OnFootFPSClipSetRequestHelper.GetClipSetId();
		}
#endif	//FPS_MODE_SUPPORTED

		if (pPed->GetIsInVehicle())
		{
			if (pPed->GetIsDrivingVehicle() && m_InCarDriverClipSetRequestHelper.IsLoaded())
			{
				ClipSetID = m_InCarDriverClipSetRequestHelper.GetClipSetId();
			}
			else if (m_InCarPassengerClipSetRequestHelper.IsLoaded())
			{
				ClipSetID = m_InCarPassengerClipSetRequestHelper.GetClipSetId();
			}
			else
			{
				ClipSetID = m_ClipSetRequestHelper.GetClipSetId();
			}
		}

		TUNE_GROUP_BOOL(TASK_MOBILEPHONE, UseCameraIntro, false);

		m_NetworkHelper.CreateNetworkPlayer(pPed, CClipNetworkMoveInfo::ms_NetworkTaskMobilePhone);
		m_NetworkHelper.SetClipSet(ClipSetID); 
		m_NetworkHelper.SetFlag(m_PhoneMode == Mode_ToText, ms_IsTexting);
		m_NetworkHelper.SetFlag(UseCameraIntro, ms_UseCameraIntro);
		BlendInNetwork();

		// B*2003415: If we're in a vehicle and re-starting the phone task from pause and the phone mode is set to Mode_ToCall, go to the text mode state.
		// Fixes bug where player can accept a call while in the paused state, setting m_PhoneMode == Mode_ToCall, causing them to hold phone to ear in vehicle (which we don't allow)
		bool bGettingCallInVehicle = pPed->IsPlayer() && pPed->GetIsInVehicle() && m_PhoneMode == Mode_ToCall;

		if ( m_PhoneMode == Mode_ToCamera )
		{
			m_NetworkHelper.SendRequest(ms_PutToCamera);
			if (!UseCameraIntro)
			{
				if (pPed->IsLocalPlayer())
				{
					// Disable look if running
					pPed->SetCodeHeadIkBlocked();

					// Force instant setup since the ped aim camera will already be active by the time IK has a chance to run
					m_bForceInstantIkSetup = true;
				}

				// Request phone since intro is being skipped
				m_bRequestGivePedPhone = true;

				// Skip intro and blend directly to camera loop state
				TASKMOBILEPHONE_SETSTATE(State_CameraLoop);
			}
			else
			{
				TASKMOBILEPHONE_SETSTATE(State_PutUpCamera);
			}
		}
		else if (m_PhoneMode == Mode_ToText || bGettingCallInVehicle)
		{
			// B*2003415: Set the phone mode to text mode.
			if (bGettingCallInVehicle) 
			{
				m_PhoneMode = Mode_ToText;
			}
			m_NetworkHelper.SendRequest(ms_PutToText);
			TASKMOBILEPHONE_SETSTATE(State_PutUpText);		
		}
		else
		{
			// m_NetworkHelper.SendRequest(ms_PutToEarTransit);
			m_NetworkHelper.SendRequest(ms_PutToEar);
			TASKMOBILEPHONE_SETSTATE(State_PutToEar);
		}
	}

	return FSM_Continue;
}

void CTaskMobilePhone::PutUpText_OnEnter()
{
	m_NetworkHelper.WaitForTargetState(ms_OnEnterPutUp); 
	m_TransitionAbortTimer.Set(fwTimer::GetTimeInMilliseconds(), ms_AbortDuration);
	m_bMoveAnimFinished = false;
}

CTask::FSM_Return CTaskMobilePhone::PutUpText_OnUpdate()
{
	if(m_NetworkHelper.IsInTargetState()) 
	{ 
		if (m_bMoveAnimFinished)
		{
			m_NetworkHelper.SetBoolean(ms_AnimFinished, false);
			TASKMOBILEPHONE_SETSTATE(State_TextLoop);	
		}

		if (ShouldPause())
		{
			m_PhoneMode = Mode_ToText;
			m_NetworkHelper.SendRequest(ms_PutDownPhone);
			TASKMOBILEPHONE_SETSTATE(State_Paused);
			return FSM_Continue;
		}

		if ( !m_PhoneRequest.IsEmpty() )
		{
			if ( m_PhoneRequest.Top() == Request_PutUpToEar )
			{
				m_PhoneRequest.Pop();
				m_NetworkHelper.SendRequest(ms_PutToEar);
				TASKMOBILEPHONE_SETSTATE(State_PutToEarFromText);
			}
		}
	}
	else if ( m_TransitionAbortTimer.IsOutOfTime() )
	{
		m_TransitionAbortTimer.Unset();
		m_NetworkHelper.ForceStateChange(ms_State_PutUpText);
	}

	return FSM_Continue;
}

void CTaskMobilePhone::PutUpText_OnProcessMoveSignals()
{
	m_bMoveAnimFinished |= m_NetworkHelper.GetBoolean(ms_AnimFinished);
}

void CTaskMobilePhone::TextLoop_OnEnter()
{
	m_NetworkHelper.WaitForTargetState(ms_OnEnterLoop); 
	m_TransitionAbortTimer.Set(fwTimer::GetTimeInMilliseconds(), ms_AbortDuration);
}

CTask::FSM_Return CTaskMobilePhone::TextLoop_OnUpdate()
{
	if(m_NetworkHelper.IsInTargetState()) 
	{ 
		if (m_Timer.IsOutOfTime())
		{
			m_NetworkHelper.SendRequest(ms_PutDownPhone);
			TASKMOBILEPHONE_SETSTATE(State_PutDownFromText);
			return FSM_Continue;
		}

		if (ShouldPause())
		{
			m_PhoneMode = Mode_ToText;		
			m_NetworkHelper.SendRequest(ms_PutDownPhone);
			TASKMOBILEPHONE_SETSTATE(State_Paused);
			return FSM_Continue;
		}

		if ( !m_PhoneRequest.IsEmpty() )
		{
			if ( m_PhoneRequest.Top() == Request_PutDownToPrevState )
			{
				m_PhoneRequest.Pop();
				m_NetworkHelper.SendRequest(ms_PutDownPhone);
				TASKMOBILEPHONE_SETSTATE(State_PutDownFromText);
			}
			else if ( m_PhoneRequest.Top() ==  Request_PutUpToEar )
			{
				//@@: location CTASKMOBILEPHONE_TEXTLOOP_ONUPDATE_PUT_UP_TO_EAR
				m_PhoneRequest.Pop();
				if ( !GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_PhoneDisableTalkingAnimations) )
				{
					m_NetworkHelper.SendRequest(ms_SwitchPhoneMode);
					TASKMOBILEPHONE_SETSTATE(State_PutToEarFromText);	
				}
			}
			else if ( m_PhoneRequest.Top() ==  Request_PutUpToCamera )
			{
				m_PhoneRequest.Pop();
				if ( !GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_PhoneDisableCameraAnimations) )
				{
					m_NetworkHelper.SendRequest(ms_PutToCamera);
					TASKMOBILEPHONE_SETSTATE(State_PutToCameraFromText);	
				}
			}
			else if (m_PhoneRequest.Top() == Request_PutHorizontalIntro)
			{
				m_PhoneRequest.Pop();
				m_NetworkHelper.SendRequest(ms_HorizontalMode);
				TASKMOBILEPHONE_SETSTATE(State_HorizontalIntro);
			}
		}
	}
	else if ( m_TransitionAbortTimer.IsOutOfTime() )
	{
		m_TransitionAbortTimer.Unset();
		m_NetworkHelper.ForceStateChange(ms_State_TextLoop);
	}

	return FSM_Continue;
}

void CTaskMobilePhone::PutDownFromText_OnEnter()
{
	m_NetworkHelper.WaitForTargetState(ms_OnEnterPutDown); 
	m_TransitionAbortTimer.Set(fwTimer::GetTimeInMilliseconds(), ms_AbortDuration);
	m_bMoveAnimFinished		= false;
	m_bMoveInterruptible	= false;
}

CTask::FSM_Return CTaskMobilePhone::PutDownFromText_OnUpdate()
{
	if(m_NetworkHelper.IsInTargetState()) 
	{ 
#if FPS_MODE_SUPPORTED
		// HACK (B*2040616): Early out of put down anim in helis/planes as it is authored to return the hand to the steering wheel (which we don't have in planes/helis!)
		CPed *pPed = GetPed();
		if (pPed && pPed->GetIsInVehicle() && pPed->IsInFirstPersonVehicleCamera())
		{
			CVehicle *pVehicle = static_cast<CVehicle*>(pPed->GetVehiclePedInside());
			if (pVehicle && (pVehicle->GetVehicleType() == VEHICLE_TYPE_HELI || pVehicle->GetVehicleType() == VEHICLE_TYPE_PLANE))
			{
				TUNE_GROUP_FLOAT(FIRST_PERSON_PHONE, PUT_DOWN_FROM_TEXT_QUIT_PHASE_IN_VEHICLE, 0.6f, 0.0f, 1.0f, 0.01f);
				float fAnimPhase = m_NetworkHelper.GetFloat(ms_PutDownFromTextPhase);
				if (fAnimPhase >= PUT_DOWN_FROM_TEXT_QUIT_PHASE_IN_VEHICLE)
				{
					m_bMoveInterruptible = true;
				}
			}
		}
#endif	//FPS_MODE_SUPPORTED

		if (m_bMoveAnimFinished || m_bMoveInterruptible)
		{
			m_NetworkHelper.SetBoolean(ms_AnimFinished, false);
			TASKMOBILEPHONE_SETSTATE(State_Finish);
		}
	}
	else if ( m_TransitionAbortTimer.IsOutOfTime() )
	{
		m_TransitionAbortTimer.Unset();
		m_NetworkHelper.ForceStateChange(ms_State_PutDownFromText);
	}

	return FSM_Continue;
}

void CTaskMobilePhone::PutDownFromText_OnProcessMoveSignals()
{
	m_bMoveAnimFinished		|= m_NetworkHelper.GetBoolean(ms_AnimFinished);
	m_bMoveInterruptible	|= m_NetworkHelper.GetBoolean(ms_Interruptible);
}

void CTaskMobilePhone::PutToEar_OnEnter()
{
	m_NetworkHelper.WaitForTargetState(ms_OnEnterPutUp); 
	m_TransitionAbortTimer.Set(fwTimer::GetTimeInMilliseconds(), ms_AbortDuration);
	m_bMoveAnimFinished = false;
}

CTask::FSM_Return CTaskMobilePhone::PutToEar_OnUpdate()
{
	if(m_NetworkHelper.IsInTargetState()) 
	{ 
		if (ShouldPause())
		{
			m_PhoneMode = Mode_ToCall;
			m_NetworkHelper.SendRequest(ms_PutDownPhone);
			TASKMOBILEPHONE_SETSTATE(State_Paused);
			return FSM_Continue;
		}
		
		if (m_bMoveAnimFinished)
		{
			m_NetworkHelper.SetBoolean(ms_AnimFinished, false);
			TASKMOBILEPHONE_SETSTATE(State_EarLoop);	
		}
	}
	else if ( m_TransitionAbortTimer.IsOutOfTime() )
	{
		m_TransitionAbortTimer.Unset();
		m_NetworkHelper.ForceStateChange(ms_State_PutToEar);
	}
	return FSM_Continue;	
}

void CTaskMobilePhone::PutToEar_OnProcessMoveSignals()
{
	m_bMoveAnimFinished |= m_NetworkHelper.GetBoolean(ms_AnimFinished);
}

void CTaskMobilePhone::EarLoop_OnEnter()
{
	m_NetworkHelper.WaitForTargetState(ms_OnEnterLoop); 
	m_TransitionAbortTimer.Set(fwTimer::GetTimeInMilliseconds(), ms_AbortDuration);
}

CTask::FSM_Return CTaskMobilePhone::EarLoop_OnUpdate()
{
	if(m_NetworkHelper.IsInTargetState()) 
	{ 
		if (m_Timer.IsOutOfTime())
		{
			m_NetworkHelper.SendRequest(ms_PutDownPhone);
			TASKMOBILEPHONE_SETSTATE(State_PutDownFromEar);
			return FSM_Continue;
		}

		if (ShouldPause())
		{
			m_PhoneMode = Mode_ToCall;
			m_NetworkHelper.SendRequest(ms_PutDownPhone);
			TASKMOBILEPHONE_SETSTATE(State_Paused);
			return FSM_Continue;
		}

		if ( !m_PhoneRequest.IsEmpty() )
		{
			if ( m_PhoneRequest.Top() == Request_PutDownToPrevState )
			{	
				m_PhoneRequest.Pop();
				if ( m_StartupMode == Mode_ToText
					&& !GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_PhoneDisableTextingAnimations) )
				{
					m_NetworkHelper.SendRequest(ms_SwitchPhoneMode);
					TASKMOBILEPHONE_SETSTATE(State_PutDownToText);
				}
				else
				{
					m_NetworkHelper.SendRequest(ms_PutDownPhone);
					TASKMOBILEPHONE_SETSTATE(State_PutDownFromEar);
				}
			}
			else
			{
// if for some reason we have something other than put down to prev state just remove from que
				m_PhoneRequest.Pop();
			}
			
		}
	}
	else if ( m_TransitionAbortTimer.IsOutOfTime() )
	{
		m_TransitionAbortTimer.Unset();
		m_NetworkHelper.ForceStateChange(ms_State_EarLoop);
	}


	return FSM_Continue;
}

void CTaskMobilePhone::PutDownFromEar_OnEnter()
{
	m_NetworkHelper.WaitForTargetState(ms_OnEnterPutDown); 
	m_TransitionAbortTimer.Set(fwTimer::GetTimeInMilliseconds(), ms_AbortDuration);
	m_bMoveAnimFinished		= false;
	m_bMoveInterruptible	= false;
}

CTask::FSM_Return CTaskMobilePhone::PutDownFromEar_OnUpdate()
{
	if(m_NetworkHelper.IsInTargetState()) 
	{ 
		if (m_bMoveAnimFinished || m_bMoveInterruptible)
		{
			m_NetworkHelper.SetBoolean(ms_AnimFinished, false);
			TASKMOBILEPHONE_SETSTATE(State_Finish);
		}
	}
	else if ( m_TransitionAbortTimer.IsOutOfTime() )
	{
		m_TransitionAbortTimer.Unset();
		m_NetworkHelper.ForceStateChange(ms_State_PutDownFromEar);
	}
	return FSM_Continue;
}

void CTaskMobilePhone::PutDownFromEar_OnProcessMoveSignals()
{
	m_bMoveAnimFinished		|= m_NetworkHelper.GetBoolean(ms_AnimFinished);
	m_bMoveInterruptible	|= m_NetworkHelper.GetBoolean(ms_Interruptible);
}

void CTaskMobilePhone::PutToEarFromText_OnEnter()
{
	m_NetworkHelper.WaitForTargetState(ms_OnEnterTransit); 
	m_TransitionAbortTimer.Set(fwTimer::GetTimeInMilliseconds(), ms_AbortDuration);
	m_bMoveAnimFinished = false;
}

CTask::FSM_Return CTaskMobilePhone::PutToEarFromText_OnUpdate()
{
	if(m_NetworkHelper.IsInTargetState()) 
	{ 
		if (m_bMoveAnimFinished)
		{
			m_NetworkHelper.SetBoolean(ms_AnimFinished, false);
			TASKMOBILEPHONE_SETSTATE(State_EarLoop);	
		}
		else
		{
			if (ShouldPause())
			{
				m_PhoneMode = Mode_ToCall;
				m_NetworkHelper.SendRequest(ms_PutDownPhone);
				TASKMOBILEPHONE_SETSTATE(State_Paused);
				return FSM_Continue;
			}

			if ( !m_PhoneRequest.IsEmpty() )
			{
				if ( m_PhoneRequest.Top() ==  Request_PutDownToPrevState )
				{
					m_PhoneRequest.Pop();
					m_NetworkHelper.SendRequest(ms_AbortTransit);
					TASKMOBILEPHONE_SETSTATE(State_TextLoop);
				}
			}
		}
	}
	else if ( m_TransitionAbortTimer.IsOutOfTime() )
	{
		m_TransitionAbortTimer.Unset();
		m_NetworkHelper.ForceStateChange(ms_State_PutToEarFromText);
	}
	return FSM_Continue;	
}

void CTaskMobilePhone::PutToEarFromText_OnProcessMoveSignals()
{
	m_bMoveAnimFinished |= m_NetworkHelper.GetBoolean(ms_AnimFinished);
}

void CTaskMobilePhone::PutDownToText_OnEnter()
{
	m_NetworkHelper.WaitForTargetState(ms_OnEnterTransit); 
	m_TransitionAbortTimer.Set(fwTimer::GetTimeInMilliseconds(), ms_AbortDuration);

	m_bMoveAnimFinished = false;
}

CTask::FSM_Return CTaskMobilePhone::PutDownToText_OnUpdate()
{
	if(m_NetworkHelper.IsInTargetState()) 
	{ 
		if (m_bMoveAnimFinished)
		{
			m_NetworkHelper.SendRequest(ms_TextLoop);
			TASKMOBILEPHONE_SETSTATE(State_TextLoop);	
		}
		else
		{
			if (ShouldPause())
			{
				m_PhoneMode = Mode_ToText;
				m_NetworkHelper.SendRequest(ms_PutDownPhone);
				TASKMOBILEPHONE_SETSTATE(State_Paused);
				return FSM_Continue;
			}
/*
 * This doesn't work not sure why yet
 *
			if ( !m_PhoneRequest.IsEmpty() )
			{
				if ( m_PhoneRequest.Top() ==  Request_PutDownToPrevState )
				{
					m_PhoneRequest.Pop();
					m_NetworkHelper.SendRequest(ms_PutDownPhone);
					TASKMOBILEPHONE_SETSTATE(State_PutDownFromText);
				}
			}
*/
		}
	}
	else if ( m_TransitionAbortTimer.IsOutOfTime() )
	{
		m_TransitionAbortTimer.Unset();
		m_NetworkHelper.ForceStateChange(ms_State_PutDownToText);
	}

	return FSM_Continue;
}

void CTaskMobilePhone::PutDownToText_OnProcessMoveSignals()
{
	m_bMoveAnimFinished |= m_NetworkHelper.GetBoolean(ms_AnimFinished);
}

void CTaskMobilePhone::PutUpCamera_OnEnter()
{
	m_NetworkHelper.WaitForTargetState(ms_OnEnterPutToCamera); 
	m_TransitionAbortTimer.Set(fwTimer::GetTimeInMilliseconds(), ms_AbortDuration);
	m_bMoveAnimFinished = false;
}

CTask::FSM_Return CTaskMobilePhone::PutUpCamera_OnUpdate()
{
	if(m_NetworkHelper.IsInTargetState()) 
	{ 
		if (m_bMoveAnimFinished)
		{
			m_NetworkHelper.SetBoolean(ms_AnimFinished, false);
			TASKMOBILEPHONE_SETSTATE(State_CameraLoop);	
		}

		if (ShouldPause())
		{
			m_PhoneMode = Mode_ToCamera;
			m_NetworkHelper.SendRequest(ms_PutDownPhone);
			TASKMOBILEPHONE_SETSTATE(State_Paused);
			return FSM_Continue;
		}

		if ( !m_PhoneRequest.IsEmpty() )
		{
			if ( m_PhoneRequest.Top() == Request_PutDownToPrevState )
			{
				m_PhoneRequest.Pop();
				m_NetworkHelper.SendRequest(ms_AbortTransit);
				TASKMOBILEPHONE_SETSTATE(State_PutDownFromCamera);
			}
		}

	}
	else if ( m_TransitionAbortTimer.IsOutOfTime() )
	{
		m_TransitionAbortTimer.Unset();
		m_NetworkHelper.ForceStateChange(ms_State_PutToCamera);
	}

	return FSM_Continue;
}

void CTaskMobilePhone::PutUpCamera_OnProcessMoveSignals()
{
	m_bMoveAnimFinished |= m_NetworkHelper.GetBoolean(ms_AnimFinished);
}

void CTaskMobilePhone::CameraLoop_OnEnter()
{
	m_NetworkHelper.WaitForTargetState(ms_OnEnterLoop); 
	m_TransitionAbortTimer.Set(fwTimer::GetTimeInMilliseconds(), ms_AbortDuration);

	CPed *pPed = GetPed();

	// B*2083435: Clear stealth mode if we're taking a picture - looks really bad. Ideally we'd have a set of stealth selfie anims.
	if (pPed->IsUsingStealthMode() && pPed->GetMotionData())
	{
		m_bWasInStealthMode = true;
		pPed->GetMotionData()->SetUsingStealth(false);
	}
}

CTask::FSM_Return CTaskMobilePhone::CameraLoop_OnUpdate()
{
	if(m_NetworkHelper.IsInTargetState()) 
	{ 
		CPed* pPed = GetPed();

		if (m_Timer.IsOutOfTime())
		{
			m_NetworkHelper.SendRequest(ms_PutDownPhone);
			TASKMOBILEPHONE_SETSTATE(State_PutDownFromCamera);
			return FSM_Continue;
		}

		if (ShouldPause())
		{
			m_PhoneMode = Mode_ToCamera;		
			m_NetworkHelper.SendRequest(ms_PutDownPhone);
			TASKMOBILEPHONE_SETSTATE(State_Paused);
			return FSM_Continue;
		}

		if ( !m_PhoneRequest.IsEmpty() )
		{
			if ( m_PhoneRequest.Top() == Request_PutDownToPrevState )
			{
				m_PhoneRequest.Pop();
				if ( m_StartupMode == Mode_ToText 
					&& !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PhoneDisableTextingAnimations) )
				{
					m_NetworkHelper.SendRequest(ms_PutToText);

					TUNE_GROUP_BOOL(TASK_MOBILEPHONE, ForceInstantTransitionToTextLoop, true);

					// B*2028572: FPS - Go instantly back to the text loop to prevent transition pops
					const int viewMode = camControlHelper::GetViewModeForContext(camControlHelperMetadataViewMode::ON_FOOT);
					if ((viewMode == camControlHelperMetadataViewMode::FIRST_PERSON) || ForceInstantTransitionToTextLoop)
					{
						// Force an anim update so we don't have a frame delay waiting for target state in TextLoop_OnUpdate.
						pPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true);
						TASKMOBILEPHONE_SETSTATE(State_TextLoop);
					}
					else
					{
						TASKMOBILEPHONE_SETSTATE(State_PutDownToTextFromCamera);
					}
				}
				else
				{
					m_NetworkHelper.SendRequest(ms_PutDownPhone);
					TASKMOBILEPHONE_SETSTATE(State_PutDownFromCamera);
				}
			}
			else
			{
				m_PhoneRequest.Pop();
			}
		}
	}
	else if ( m_TransitionAbortTimer.IsOutOfTime() )
	{
		m_TransitionAbortTimer.Unset();
		m_NetworkHelper.ForceStateChange(ms_State_CameraLoop);
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskMobilePhone::CameraLoop_OnExit()
{
	CPed* pPed = GetPed();

	// B*2083435: Restore stealth mode if we were in it before entering the camera state.
	if (m_bWasInStealthMode && pPed->GetMotionData())
	{
		m_bWasInStealthMode = false;
		pPed->GetMotionData()->SetUsingStealth(true);
	}

	return FSM_Continue;
}

void CTaskMobilePhone::PutDownFromCamera_OnProcessMoveSignals()
{
	m_bMoveAnimFinished |= m_NetworkHelper.GetBoolean(ms_AnimFinished);
}


void CTaskMobilePhone::PutDownFromCamera_OnEnter()
{
	m_NetworkHelper.WaitForTargetState(ms_OnEnterPutDown); 
	m_TransitionAbortTimer.Set(fwTimer::GetTimeInMilliseconds(), ms_AbortDuration);
	m_bMoveAnimFinished		= false;
	m_bMoveInterruptible	= false;
}

CTask::FSM_Return CTaskMobilePhone::PutDownFromCamera_OnUpdate()
{
	if(m_NetworkHelper.IsInTargetState()) 
	{ 
		if (m_bMoveAnimFinished || m_bMoveInterruptible)
		{
			m_NetworkHelper.SetBoolean(ms_AnimFinished, false);
			TASKMOBILEPHONE_SETSTATE(State_Finish);
		}
		else
		{
			// special case
			// reset task in case where	we start to put phone 
			// away and then bring it back up like when we delete a photo
			if ( CPhoneMgr::CamGetState() )
			{
				ResetTaskMobilePhone(Mode_ToCamera, -1, m_bIsUsingSecondarySlot);
			}
		}
	}
	else if ( m_TransitionAbortTimer.IsOutOfTime() )
	{
		m_TransitionAbortTimer.Unset();
		m_NetworkHelper.ForceStateChange(ms_State_PutDownFromCamera);
	}
	return FSM_Continue;
} 

void CTaskMobilePhone::PutToCameraFromText_OnEnter()
{
	m_NetworkHelper.WaitForTargetState(ms_OnEnterPutToCamera); 
	m_TransitionAbortTimer.Set(fwTimer::GetTimeInMilliseconds(), ms_AbortDuration);
}

CTask::FSM_Return CTaskMobilePhone::PutToCameraFromText_OnUpdate()
{
	if(m_NetworkHelper.IsInTargetState()) 
	{ 
		TASKMOBILEPHONE_SETSTATE(State_CameraLoop);
	}
	else if ( m_TransitionAbortTimer.IsOutOfTime() )
	{
		m_TransitionAbortTimer.Unset();
		m_NetworkHelper.ForceStateChange(ms_State_PutToCameraFromText);
	}
	return FSM_Continue;
}

void CTaskMobilePhone::PutDownToTextFromCamera_OnEnter()
{
	m_NetworkHelper.WaitForTargetState(ms_OnEnterPutDownToTextFromCamera); 
	m_TransitionAbortTimer.Set(fwTimer::GetTimeInMilliseconds(), ms_AbortDuration);
}

CTask::FSM_Return CTaskMobilePhone::PutDownToTextFromCamera_OnUpdate()
{
	if(m_NetworkHelper.IsInTargetState()) 
	{ 
		TASKMOBILEPHONE_SETSTATE(State_TextLoop);
	}
	else if ( m_TransitionAbortTimer.IsOutOfTime() )
	{
		m_TransitionAbortTimer.Unset();
		m_NetworkHelper.ForceStateChange(ms_State_PutDownToTextFromCamera);
	}

	return FSM_Continue;
}

void CTaskMobilePhone::Paused_OnEnter()
{
	RemovePedPhone();
	BlendOutNetwork();
}

CTask::FSM_Return CTaskMobilePhone::Paused_OnUpdate()
{
	while ( !m_PhoneRequest.IsEmpty() )
	{
		if ( m_PhoneRequest.Top() ==  Request_PutDownToPrevState )
		{		
			if ( m_PhoneMode != Mode_ToText)
			{		
				if ( m_StartupMode == Mode_ToText)
				{
					m_PhoneMode = Mode_ToText;
				}
				else
				{
					return FSM_Quit;
				}
			}
			else
			{
				return FSM_Quit;
			}
		}
		else if ( m_PhoneRequest.Top() ==  Request_PutUpToEar )
		{
			if ( m_PhoneMode == Mode_ToText )
			{
				m_PhoneMode = Mode_ToCall;
			}
		}
		else if ( m_PhoneRequest.Top() ==  Request_PutUpToCamera )
		{
			if ( m_PhoneMode == Mode_ToText )
			{
				m_PhoneMode = Mode_ToCamera;
			}
		}


		m_PhoneRequest.Pop();
	}

	
	if ( ShouldUnPause() )
	{
		if ( CanUseMobilePhoneTask(*GetPed(), m_PhoneMode) )
		{
			ResetTaskMobilePhone(m_PhoneMode, m_Timer.GetDuration());
		}
		else
		{
			return FSM_Quit;
		}

	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskMobilePhone::Finish_OnUpdate()
{
	return FSM_Quit;
}

// HORIZ-INTRO
void CTaskMobilePhone::HorizontalIntro_OnEnter()
{
	m_NetworkHelper.WaitForTargetState(ms_OnEnterHorizontalIntro); 
	m_TransitionAbortTimer.Set(fwTimer::GetTimeInMilliseconds(), ms_AbortDuration);
	m_bMoveAnimFinished = false;
}

CTask::FSM_Return CTaskMobilePhone::HorizontalIntro_OnUpdate()
{
	if(m_NetworkHelper.IsInTargetState()) 
	{
		if (ShouldPause())
		{
			m_NetworkHelper.SendRequest(ms_PutDownPhone);
			TASKMOBILEPHONE_SETSTATE(State_Paused);
			return FSM_Continue;
		}

		if ( !m_PhoneRequest.IsEmpty() )
		{
			if (m_PhoneRequest.Top() ==  Request_PutUpToCamera)
			{
				m_PhoneRequest.Pop();
				if ( !GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_PhoneDisableCameraAnimations) )
				{
					m_NetworkHelper.SendRequest(ms_PutToCamera);
					TASKMOBILEPHONE_SETSTATE(State_PutToCameraFromText);	
					return FSM_Continue;
				}
			}
		}

		// If intro is finished, move to loop state
		if (m_bMoveAnimFinished)
		{
			TASKMOBILEPHONE_SETSTATE(State_HorizontalLoop);	
			return FSM_Continue;
		}
	}
	return FSM_Continue;
}

void CTaskMobilePhone::HorizontalIntro_OnProcessMoveSignals()
{
	m_bMoveAnimFinished |= m_NetworkHelper.GetBoolean(ms_AnimFinished);
}

// HORIZ-LOOP
void CTaskMobilePhone::HorizontalLoop_OnEnter()
{
	m_NetworkHelper.WaitForTargetState(ms_OnEnterHorizontalLoop); 
	m_TransitionAbortTimer.Set(fwTimer::GetTimeInMilliseconds(), ms_AbortDuration);
	m_bMoveAnimFinished = false;
}

CTask::FSM_Return CTaskMobilePhone::HorizontalLoop_OnUpdate()
{
	// Go back to text loop (via outro) before processing any requests so anim doesn't pop
	if(m_NetworkHelper.IsInTargetState()) 
	{ 
		if (m_Timer.IsOutOfTime())
		{
			m_NetworkHelper.SendRequest(ms_QuitHorizontalMode);
			TASKMOBILEPHONE_SETSTATE(State_HorizontalOutro);
			return FSM_Continue;
		}

		if (ShouldPause())
		{
			m_NetworkHelper.SendRequest(ms_PutDownPhone);
			TASKMOBILEPHONE_SETSTATE(State_Paused);
			return FSM_Continue;
		}

		if ( !m_PhoneRequest.IsEmpty() )
		{
			if (m_PhoneRequest.Top() ==  Request_PutUpToCamera)
			{
				m_PhoneRequest.Pop();
				if ( !GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_PhoneDisableCameraAnimations) )
				{
					m_NetworkHelper.SendRequest(ms_PutToCamera);
					TASKMOBILEPHONE_SETSTATE(State_PutToCameraFromText);	
					return FSM_Continue;
				}
			}
			else if (m_PhoneRequest.Top() == Request_PutHorizontalOutro)
			{
				m_PhoneRequest.Pop();
				m_NetworkHelper.SendRequest(ms_QuitHorizontalMode);
				TASKMOBILEPHONE_SETSTATE(State_HorizontalOutro);
				return FSM_Continue;
			}
			else if ( m_PhoneRequest.Top() == Request_PutDownToPrevState )
			{
				m_PhoneRequest.Pop();
				m_NetworkHelper.SendRequest(ms_PutDownPhone);
				TASKMOBILEPHONE_SETSTATE(State_PutDownFromText);
				return FSM_Continue;
			}
			m_PhoneRequest.Pop();
			m_NetworkHelper.SendRequest(ms_QuitHorizontalMode);
			TASKMOBILEPHONE_SETSTATE(State_HorizontalOutro);
			return FSM_Continue;
		}
	}

	return FSM_Continue;
}

void CTaskMobilePhone::HorizontalLoop_OnProcessMoveSignals()
{

}

// HORIZ-OUTRO
void CTaskMobilePhone::HorizontalOutro_OnEnter()
{
	m_NetworkHelper.WaitForTargetState(ms_OnEnterHorizontalOutro); 
	m_TransitionAbortTimer.Set(fwTimer::GetTimeInMilliseconds(), ms_AbortDuration);
	m_bMoveAnimFinished = false;
}

CTask::FSM_Return CTaskMobilePhone::HorizontalOutro_OnUpdate()
{
	if(m_NetworkHelper.IsInTargetState()) 
	{
		if (ShouldPause())
		{
			m_NetworkHelper.SendRequest(ms_PutDownPhone);
			TASKMOBILEPHONE_SETSTATE(State_Paused);
			return FSM_Continue;
		}

		if ( !m_PhoneRequest.IsEmpty() )
		{
			if (m_PhoneRequest.Top() ==  Request_PutUpToCamera)
			{
				m_PhoneRequest.Pop();
				if ( !GetPed()->GetPedConfigFlag(CPED_CONFIG_FLAG_PhoneDisableCameraAnimations) )
				{
					m_NetworkHelper.SendRequest(ms_PutToCamera);
					TASKMOBILEPHONE_SETSTATE(State_PutToCameraFromText);	
					return FSM_Continue;
				}
			}
		}

		// If intro is finished, move to loop state
		if (m_bMoveAnimFinished)
		{
			TASKMOBILEPHONE_SETSTATE(State_TextLoop);	
			return FSM_Continue;
		}
	}
	return FSM_Continue;
}

void CTaskMobilePhone::HorizontalOutro_OnProcessMoveSignals()
{
	m_bMoveAnimFinished |= m_NetworkHelper.GetBoolean(ms_AnimFinished);
}

bool CTaskMobilePhone::ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
	return CTask::ShouldAbort(iPriority, pEvent);
}

void CTaskMobilePhone::DoAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
	CTask::DoAbort(iPriority, pEvent);
}

bool CTaskMobilePhone::ShouldCreatePhoneProp() const
{
	if (m_bHasPhoneEquipped)
	{
		return false;
	}

	if (!m_bIsPhoneModelLoaded)
	{
		return false;
	}

	switch(GetState())
	{
	case(State_Init):
	case(State_HolsterWeapon):
	case(State_Start):
	case(State_PutUpText):
	case(State_PutToEar):
	case(State_PutDownFromText):
	case(State_PutDownFromEar):
	case(State_PutDownFromCamera):
	case(State_Paused):
		return false;
	default:
		break;
	}

	return true;
}

void CTaskMobilePhone::ProcessClipMoveEventTags()
{
	if (m_NetworkHelper.IsNetworkActive())
	{
		if (m_NetworkHelper.GetBoolean(ms_CreatePhone) )
		{
			m_bRequestGivePedPhone = true;
		}

		if (m_NetworkHelper.GetBoolean(ms_DestroyPhone) )
		{
			m_bRequestGivePedPhone = false;
			RemovePedPhone();
		}
				
		// play the correct animations
		m_NetworkHelper.SetFlag(CPhoneMgr::CamGetSelfieModeState(), ms_SelfPortrait);

	}

	if ( m_bRequestGivePedPhone )
	{
		m_bRequestGivePedPhone = !m_bHasPhoneEquipped;
		GivePedPhone();
	}
}

void CTaskMobilePhone::ProcessIK()
{
	CPed* pPed = GetPed();

	if (!pPed->IsLocalPlayer())
		return;

	if (!CPhoneMgr::CamGetSelfieModeState() && !CPhoneMgr::CamGetState())
		return;

	CIkManager& ikManager = pPed->GetIkManager();

	const bool bForceInstantIkSetup = m_bForceInstantIkSetup;
	m_bForceInstantIkSetup = false;

	const Mat34V mtxEntity(pPed->GetMatrix());
	const Mat34V mtxCamera(MATRIX34_TO_MAT34V(camInterface::GetMat()));

	Mat34V mtxSource;
	UnTransformFull(mtxSource, mtxEntity, mtxCamera);

	// B*1869384: When taking a photo using phone camera (not selfie mode), IK hand in front of the ped and use an arm-only filter (so we don't look at the phone).
	// Fixes issue where ped's arm is extended and looking at phone to the right of their body.
	if (CPhoneMgr::CamGetState() && !CPhoneMgr::CamGetSelfieModeState())
	{
		if (camInterface::GetGameplayDirector().GetFirstPersonPedAimCamera() == NULL)
			return;

		m_NetworkHelper.SetFlag(false, ms_UseAlternateFilter);
		crFrameFilter* pFilter = g_FrameFilterDictionaryStore.FindFrameFilter(BONEMASK_HEAD_NECK_AND_ARMS);
		m_NetworkHelper.SetFilter(pFilter, ms_BonemaskFilter);

		TUNE_GROUP_FLOAT(ARM_IK, PHONECAMAIM_XOFFSET, 0.16f, -1.0f, 1.0f, 0.01f);
		TUNE_GROUP_FLOAT(ARM_IK, PHONECAMAIM_YOFFSET, 0.40f, -1.0f, 1.0f, 0.01f);
		TUNE_GROUP_FLOAT(ARM_IK, PHONECAMAIM_ZOFFSET, -0.08f, -1.0f, 1.0f, 0.01f);
		TUNE_GROUP_FLOAT(ARM_IK, PHONECAMAIM_ROTOFFSET, 40.0f, -90.0f, 90.0f, 0.01f);

		Vec3V vTargetPosition(AddScaled(mtxSource.GetCol3(), mtxSource.GetCol0(), ScalarV(PHONECAMAIM_XOFFSET)));
		vTargetPosition = AddScaled(vTargetPosition, mtxSource.GetCol1(), ScalarV(PHONECAMAIM_YOFFSET));
		vTargetPosition = AddScaled(vTargetPosition, mtxSource.GetCol2(), ScalarV(PHONECAMAIM_ZOFFSET));

		// Add look for mirrors
		const ScalarV vThreshold(15.0f);
		if (pPed->IsNearMirror(vThreshold))
		{
			CIkRequestBodyLook ikRequestLook(pPed, BONETAG_INVALID, vTargetPosition, LOOKIK_BLOCK_ANIM_TAGS | LOOKIK_DISABLE_EYE_OPTIMIZATION, CIkManager::IK_LOOKAT_VERY_HIGH);
			ikRequestLook.SetRotLimHead(LOOKIK_ROT_ANGLE_YAW, LOOKIK_ROT_LIM_WIDE);
			ikRequestLook.SetRotLimHead(LOOKIK_ROT_ANGLE_PITCH, LOOKIK_ROT_LIM_WIDE);
			ikRequestLook.SetTurnRate(LOOKIK_TURN_RATE_FAST);

			ikManager.Request(ikRequestLook);
		}

		// Setup orientation relative to hand but rotated to account for camera pitch
		const Mat34V& mtxHand = pPed->GetSkeleton()->GetObjectMtx(pPed->GetBoneIndexFromBoneTag(BONETAG_R_HAND));
		QuatV qRotation(QuatVFromVectors(Vec3V(V_Y_AXIS_WZERO), mtxSource.GetCol1()));
		qRotation = Multiply(qRotation, QuatVFromMat34V(mtxHand));

		// Additive rotation for hand
		QuatV qAdditive(QuatVFromAxisAngle(mtxSource.GetCol2(), ScalarV(PHONECAMAIM_ROTOFFSET * DtoR)));

		CIkRequestArm ikRequest(IK_ARM_RIGHT, pPed, BONETAG_INVALID, vTargetPosition, qRotation, AIK_USE_ORIENTATION | AIK_USE_TWIST_CORRECTION);
		ikRequest.SetAdditive(qAdditive);
		ikRequest.SetBlendInRate(ARMIK_BLEND_RATE_FASTEST);
		ikRequest.SetBlendOutRate(ARMIK_BLEND_RATE_FASTEST);

#if FPS_MODE_SUPPORTED
		// B*2028572: Instantly blend in/out the camera mode IK if we're in FPS mode
		const int viewMode = camControlHelper::GetViewModeForContext(camControlHelperMetadataViewMode::ON_FOOT);
		if(viewMode == camControlHelperMetadataViewMode::FIRST_PERSON)
		{
			ikRequest.SetBlendInRate(ARMIK_BLEND_RATE_INSTANT);
			ikRequest.SetBlendOutRate(ARMIK_BLEND_RATE_FASTEST);
		}
#endif	//FPS_MODE_SUPPORTED

		ikManager.Request(ikRequest);
	}
	else
	{
		if (camInterface::GetGameplayDirector().GetThirdPersonAimCamera() == NULL)
			return;

		const Vec3V vCameraPosition(mtxSource.GetCol3());
		const camFrame& cameraFrame = camInterface::GetGameplayDirector().GetFrame();
		bool bInstant = cameraFrame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) || cameraFrame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation) || bForceInstantIkSetup;

		m_NetworkHelper.SetFlag(false, ms_UseAlternateFilter);
		crFrameFilter* pFilter = g_FrameFilterDictionaryStore.FindFrameFilter(BONEMASK_HEAD_NECK_AND_ARMS);
		m_NetworkHelper.SetFilter(pFilter, ms_BonemaskFilter);

		u32 uFlags = 0;
		if (NetworkInterface::IsNetworkOpen())
		{
			uFlags = LOOKIK_BLOCK_ANIM_TAGS;
			if (GetIsPlayingAdditionalSecondaryAnim())
			{
				uFlags = LOOKIK_USE_ANIM_TAGS|LOOKIK_USE_ANIM_BLOCK_TAGS;
			}
		}
		else
		{
			uFlags = LOOKIK_USE_ANIM_TAGS;
		}

		CIkRequestBodyLook lookRequest(pPed, BONETAG_INVALID, vCameraPosition, uFlags, CIkManager::IK_LOOKAT_VERY_HIGH);

		lookRequest.SetTurnRate(LOOKIK_TURN_RATE_FASTEST);
		lookRequest.SetBlendInRate(LOOKIK_BLEND_RATE_FASTEST);
		lookRequest.SetBlendOutRate(LOOKIK_BLEND_RATE_FAST);
		lookRequest.SetRefDirHead(LOOKIK_REF_DIR_FACING);
		lookRequest.SetRotLimHead(LOOKIK_ROT_ANGLE_YAW, LOOKIK_ROT_LIM_WIDEST);
		lookRequest.SetRotLimHead(LOOKIK_ROT_ANGLE_PITCH, LOOKIK_ROT_LIM_WIDEST);
		lookRequest.SetRefDirNeck(LOOKIK_REF_DIR_FACING);
		lookRequest.SetRotLimNeck(LOOKIK_ROT_ANGLE_YAW, LOOKIK_ROT_LIM_WIDE);
		lookRequest.SetRotLimNeck(LOOKIK_ROT_ANGLE_PITCH, LOOKIK_ROT_LIM_WIDE);
		lookRequest.SetRefDirTorso(LOOKIK_REF_DIR_FACING);
		lookRequest.SetRotLimTorso(LOOKIK_ROT_ANGLE_YAW, LOOKIK_ROT_LIM_WIDEST);
		lookRequest.SetRotLimTorso(LOOKIK_ROT_ANGLE_PITCH, LOOKIK_ROT_LIM_NARROW);
		lookRequest.SetArmCompensation(LOOKIK_ARM_LEFT, LOOKIK_ARM_COMP_FULL);
		lookRequest.SetArmCompensation(LOOKIK_ARM_RIGHT, LOOKIK_ARM_COMP_FULL);
		lookRequest.SetInstant(bInstant);

		// Additive rotation
		QuatV qAdditive(QuatVFromAxisAngle(mtxSource.GetCol1(), ScalarV(CPhoneMgr::CamGetSelfieHeadRoll() * DtoR)));
		qAdditive = Multiply(qAdditive, QuatVFromAxisAngle(mtxSource.GetCol2(), ScalarV(CPhoneMgr::CamGetSelfieHeadYaw() * DtoR)));
		qAdditive = Multiply(qAdditive, QuatVFromAxisAngle(mtxSource.GetCol0(), ScalarV(-CPhoneMgr::CamGetSelfieHeadPitch() * DtoR)));
		lookRequest.SetHeadNeckAdditive(qAdditive);
		lookRequest.SetHeadNeckAdditiveRatio(0.70f); // 70/30 Head/Neck

		ikManager.Request(lookRequest);

		TUNE_GROUP_FLOAT(ARM_IK, SELFIE_XOFFSET_MIN, -0.40f, -1.0f, 1.0f, 0.01f);
		TUNE_GROUP_FLOAT(ARM_IK, SELFIE_XOFFSET_MAX, -0.20f, -1.0f, 1.0f, 0.01f);
		TUNE_GROUP_FLOAT(ARM_IK, SELFIE_XOFFSET_MIN_LIMIT, -0.60f, -1.0f, 1.0f, 0.01f);
		TUNE_GROUP_FLOAT(ARM_IK, SELFIE_XOFFSET_MAX_LIMIT, 0.30f, -1.0f, 1.0f, 0.01f);
		TUNE_GROUP_FLOAT(ARM_IK, SELFIE_YOFFSET, 0.0f, -1.0f, 1.0f, 0.01f);
		TUNE_GROUP_FLOAT(ARM_IK, SELFIE_ZOFFSET, -0.45f, -1.0f, 1.0f, 0.01f);

		// Interpolate x offset based on where camera is laterally to the character
		ScalarV vLateral(Subtract(ScalarV(SELFIE_XOFFSET_MAX_LIMIT), ScalarV(SELFIE_XOFFSET_MIN_LIMIT)));
		vLateral = InvScaleSafe(Subtract(mtxSource.GetCol3().GetX(), ScalarV(SELFIE_XOFFSET_MIN_LIMIT)), vLateral, ScalarV(V_ZERO));
		vLateral = Clamp(vLateral, ScalarV(V_ZERO), ScalarV(V_ONE));
		ScalarV vXOffset(Lerp(vLateral, ScalarV(SELFIE_XOFFSET_MIN), ScalarV(SELFIE_XOFFSET_MAX)));

		// Calculate target position in object space
		Vec3V vTargetPosition(AddScaled(mtxSource.GetCol3(), mtxSource.GetCol0(), vXOffset));
		vTargetPosition = AddScaled(vTargetPosition, mtxSource.GetCol1(), ScalarV(SELFIE_YOFFSET));
		vTargetPosition = AddScaled(vTargetPosition, mtxSource.GetCol2(), ScalarV(SELFIE_ZOFFSET));

		// Setup orientation relative to hand but rotated to account for camera orientation
		crSkeleton* pSkeleton = pPed->GetSkeleton();
		QuatV qRotation(V_IDENTITY);

		if (pSkeleton)
		{
			const Mat34V& mtxHand = pSkeleton->GetObjectMtx(pPed->GetBoneIndexFromBoneTag(BONETAG_R_HAND));
			const Mat34V& mtxUpperArm = pSkeleton->GetObjectMtx(pPed->GetBoneIndexFromBoneTag(BONETAG_R_UPPERARM));
			Vec3V vUp(UnTransform3x3Full(mtxEntity, Vec3V(V_UP_AXIS_WZERO)));

			// Incoming camera matrix orientated towards character's upper arm
			Vec3V vForward(NormalizeFast(Subtract(mtxUpperArm.GetCol3(), vTargetPosition)));
			mtxSource.SetCol1(vForward);
			mtxSource.SetCol0(Cross(mtxSource.GetCol1(), vUp));
			mtxSource.SetCol2(Cross(mtxSource.GetCol0(), mtxSource.GetCol1()));
			ReOrthonormalize3x3(mtxSource, mtxSource);

			// Similar matrix orientated towards character's upper arm but from the character's incoming hand position
			Mat34V mtxSourceHand;
			mtxSourceHand.SetCol1(NormalizeFast(Subtract(mtxUpperArm.GetCol3(), mtxHand.GetCol3())));
			mtxSourceHand.SetCol0(Cross(mtxSourceHand.GetCol1(), vUp));
			mtxSourceHand.SetCol2(Cross(mtxSourceHand.GetCol0(), mtxSourceHand.GetCol1()));
			ReOrthonormalize3x3(mtxSourceHand, mtxSourceHand);

			// Hand orientated by difference between above orientations
			qRotation = Multiply(QuatVFromMat34V(mtxSource), InvertNormInput(QuatVFromMat34V(mtxSourceHand)));
			qRotation = Multiply(qRotation, QuatVFromMat34V(mtxHand));
		}

		CIkRequestArm ikRequest(IK_ARM_RIGHT, pPed, BONETAG_INVALID, vTargetPosition, qRotation, AIK_USE_ORIENTATION | AIK_USE_TWIST_CORRECTION);
		ikRequest.SetBlendInRate(!bInstant ? ARMIK_BLEND_RATE_FASTEST : ARMIK_BLEND_RATE_INSTANT);
		ikRequest.SetBlendOutRate(ARMIK_BLEND_RATE_FASTEST);

#if FPS_MODE_SUPPORTED
		// B*2028572: Instantly blend in/out the camera mode IK if we're in FPS mode
		const int viewMode = camControlHelper::GetViewModeForContext(camControlHelperMetadataViewMode::ON_FOOT);
		if(viewMode == camControlHelperMetadataViewMode::FIRST_PERSON)
		{
			ikRequest.SetBlendInRate(ARMIK_BLEND_RATE_INSTANT);
			ikRequest.SetBlendOutRate(ARMIK_BLEND_RATE_FASTEST);

			bInstant = true;
		}
#endif	//FPS_MODE_SUPPORTED

		ikManager.Request(ikRequest);

		ProcessLeftArmIK(vCameraPosition, mtxEntity, bInstant);
	}
}

void CTaskMobilePhone::ProcessLeftArmIK(Vec3V_In vCameraPosition, const Mat34V& mtxEntity, const bool bInstant)
{
	TUNE_GROUP_BOOL(ARM_IK, SELFIE_LEFTARM_ENABLE, false);
	TUNE_GROUP_BOOL(ARM_IK, SELFIE_LEFTARM_JOINT_TARGET_HEAD, true);
	TUNE_GROUP_FLOAT(ARM_IK, SELFIE_LEFTARM_JOINT_TARGET_WEIGHT, 0.0f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(ARM_IK, SELFIE_LEFTARM_CAM_REL_WEIGHT, 0.0f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(ARM_IK, SELFIE_LEFTARM_CAM_REL_YAW_MIN, -30.0f, -180.0f, 180.0f, 0.1f);
	TUNE_GROUP_FLOAT(ARM_IK, SELFIE_LEFTARM_CAM_REL_YAW_MAX, 30.0f, -180.0f, 180.0f, 0.1f);
	TUNE_GROUP_FLOAT(ARM_IK, SELFIE_LEFTARM_CAM_REL_PITCH_MIN, -30.0f, -180.0f, 180.0f, 0.1f);
	TUNE_GROUP_FLOAT(ARM_IK, SELFIE_LEFTARM_CAM_REL_PITCH_MAX, 30.0f, -180.0f, 180.0f, 0.1f);
	TUNE_GROUP_FLOAT(ARM_IK, SELFIE_LEFTARM_CAM_REL_YAW, 0.0f, -180.0f, 180.0f, 0.1f);
	TUNE_GROUP_FLOAT(ARM_IK, SELFIE_LEFTARM_CAM_REL_PITCH, 0.0f, -180.0f, 180.0f, 0.1f);

	static const ArmIkBlendRate aBlendTypeToArmIkRate[CClipEventTags::CSelfieLeftArmControlEventTag::BtCount] =
	{
		ARMIK_BLEND_RATE_SLOWEST,	// BtSlowest
		ARMIK_BLEND_RATE_SLOW,		// BtSlow
		ARMIK_BLEND_RATE_NORMAL,	// BtNormal
		ARMIK_BLEND_RATE_FAST,		// BtFast
		ARMIK_BLEND_RATE_FASTEST	// BtFastest
	};

	CPed* pPed = GetPed();
	crSkeleton* pSkeleton = pPed->GetSkeleton();

	if (pSkeleton == NULL)
	{
		return;
	}

	CClipEventTags::CSelfieLeftArmControlEventTag::eJointTarget jointTarget = CClipEventTags::CSelfieLeftArmControlEventTag::JtHead;
	CClipEventTags::CSelfieLeftArmControlEventTag::eBlendType blendInRate = CClipEventTags::CSelfieLeftArmControlEventTag::BtFastest;
	CClipEventTags::CSelfieLeftArmControlEventTag::eBlendType blendOutRate = CClipEventTags::CSelfieLeftArmControlEventTag::BtFastest;
	float fJointTargetWeight = 0.0f;
	float fCamRelWeight = 0.0f;
	bool bValid = false;

	fwAnimDirector* pAnimDirector = pPed->GetAnimDirector();

	if (pAnimDirector)
	{
		const CClipEventTags::CSelfieLeftArmControlEventTag* pTag = static_cast<const CClipEventTags::CSelfieLeftArmControlEventTag*>(pAnimDirector->FindPropertyInMoveNetworks(CClipEventTags::SelfieLeftArmControl));

		if (pTag)
		{
			jointTarget = pTag->GetJointTarget();
			fJointTargetWeight = pTag->GetJointTargetWeight();
			fCamRelWeight = pTag->GetCameraRelativeWeight();
			blendInRate = pTag->GetBlendIn();
			blendOutRate = pTag->GetBlendOut();
			bValid = true;
		}
	}

	if (SELFIE_LEFTARM_ENABLE)
	{
		jointTarget = SELFIE_LEFTARM_JOINT_TARGET_HEAD ? CClipEventTags::CSelfieLeftArmControlEventTag::JtHead : CClipEventTags::CSelfieLeftArmControlEventTag::JtSpine;
		fJointTargetWeight = SELFIE_LEFTARM_JOINT_TARGET_WEIGHT;
		fCamRelWeight = SELFIE_LEFTARM_CAM_REL_WEIGHT;
		bValid = true;
	}

	if (bValid && pSkeleton)
	{
		eAnimBoneTag boneTag = (jointTarget == CClipEventTags::CSelfieLeftArmControlEventTag::JtHead) ? BONETAG_HEAD : BONETAG_SPINE3;
		s32 boneIdx = -1;
		s32 facingBoneIdx = -1;
		s32 upperArmBoneIdx = -1;
		s32 handBoneIdx = -1;
		s32 ikBoneIdx = -1;

		pSkeleton->GetSkeletonData().ConvertBoneIdToIndex((u16)boneTag, boneIdx);
		pSkeleton->GetSkeletonData().ConvertBoneIdToIndex((u16)BONETAG_FACING_DIR, facingBoneIdx);
		pSkeleton->GetSkeletonData().ConvertBoneIdToIndex((u16)BONETAG_L_UPPERARM, upperArmBoneIdx);
		pSkeleton->GetSkeletonData().ConvertBoneIdToIndex((u16)BONETAG_L_HAND, handBoneIdx);
		pSkeleton->GetSkeletonData().ConvertBoneIdToIndex((u16)BONETAG_L_IK_HAND, ikBoneIdx);

		if ((boneIdx >= 0) && (facingBoneIdx >= 0) && (upperArmBoneIdx >= 0) && (handBoneIdx >= 0) && (ikBoneIdx >= 0))
		{
			const Mat34V& mtxUpperArm = pSkeleton->GetObjectMtx(upperArmBoneIdx);
			const Mat34V& mtxHand = pSkeleton->GetObjectMtx(handBoneIdx);
			const Mat34V& mtxIH = pSkeleton->GetObjectMtx(ikBoneIdx);

			const Vec3V vPelvisOffset(0.0f, 0.0f, pPed->GetIkManager().GetPelvisDeltaZForCamera(true));

			// Calculate IK hand position
			Vec3V vLtHandIk(Lerp(ScalarV(fJointTargetWeight), mtxHand.GetCol3(), Add(pSkeleton->GetObjectMtx(boneIdx).GetCol3(), Subtract(mtxHand.GetCol3(), mtxIH.GetCol3()))));

			// Calculate yaw, pitch relative to facing direction
			Mat34V mtxFacing(mtxEntity);
			Transform(mtxFacing, mtxFacing, pSkeleton->GetLocalMtx(facingBoneIdx));
			UnTransformFull(mtxFacing, mtxEntity, mtxFacing);
			mtxFacing.GetCol3Ref().SetZ(mtxUpperArm.GetCol3().GetZ());

			const Vec2V vToRadians(V_TO_RADIANS);
			const Vec2V vMinLimits(Scale(Vec2V(SELFIE_LEFTARM_CAM_REL_YAW_MIN, SELFIE_LEFTARM_CAM_REL_PITCH_MIN), vToRadians));
			const Vec2V vMaxLimits(Scale(Vec2V(SELFIE_LEFTARM_CAM_REL_YAW_MAX, SELFIE_LEFTARM_CAM_REL_PITCH_MAX), vToRadians));

			const Vec3V vCameraPositionFacingDirSpace(UnTransformFull(mtxFacing, Subtract(vCameraPosition, vPelvisOffset)));
			const Vec2V vYComponent(Negate(vCameraPositionFacingDirSpace.GetX()), vCameraPositionFacingDirSpace.GetZ());
			const Vec2V vXComponent(SelectFT(IsZero(vCameraPositionFacingDirSpace.GetY()), vCameraPositionFacingDirSpace.GetY(), ScalarV(V_FLT_SMALL_4)), Max(MagXY(vCameraPositionFacingDirSpace), ScalarV(V_FLT_SMALL_4)));
			Vec2V vYawPitchTarget(Clamp(Arctan2(vYComponent, vXComponent), vMinLimits, vMaxLimits));

		#if __BANK
			SELFIE_LEFTARM_CAM_REL_YAW = vYawPitchTarget.GetXf() * RtoD;
			SELFIE_LEFTARM_CAM_REL_PITCH = vYawPitchTarget.GetYf() * RtoD;
		#endif // __BANK

			// Calculate rotation
			QuatV qToCamera(QuatVFromAxisAngle(mtxFacing.GetCol2(), vYawPitchTarget.GetX()));
			qToCamera = Multiply(qToCamera, QuatVFromAxisAngle(mtxFacing.GetCol0(), vYawPitchTarget.GetY()));

			// Calculate hand position rotated to camera
			Vec3V vArmDirection(Subtract(mtxHand.GetCol3(), mtxUpperArm.GetCol3()));
			ScalarV vMag(Mag(vArmDirection));
			vArmDirection = NormalizeSafe(vArmDirection, Vec3V(V_ZERO));
			vArmDirection = Transform(qToCamera, vArmDirection);
			Vec3V vLtHandCamRel(AddScaled(mtxUpperArm.GetCol3(), vArmDirection, vMag));

			// Calculate hand orientation
			QuatV qHand(QuatVFromMat34V(mtxHand));
			QuatV qHandTarget(Multiply(qToCamera, qHand));

			if (CanSlerp(qHand, qHandTarget))
			{
				qHandTarget = PrepareSlerp(qHand, qHandTarget);
				qHandTarget = Slerp(ScalarV(fCamRelWeight), qHand, qHandTarget);
			}

			// Final target position interpolated with respect to camera relative weight
			Vec3V vLtHand(Lerp(ScalarV(fCamRelWeight), vLtHandIk, vLtHandCamRel));

			// Adjust for leg IK
			vLtHand = Add(vLtHand, vPelvisOffset);

			CIkRequestArm ikRequest(IK_ARM_LEFT, pPed, BONETAG_INVALID, vLtHand, qHandTarget, AIK_USE_ORIENTATION | AIK_USE_FULL_REACH | AIK_USE_TWIST_CORRECTION);
			ikRequest.SetBlendInRate(!bInstant ? aBlendTypeToArmIkRate[blendInRate] : ARMIK_BLEND_RATE_INSTANT);
			ikRequest.SetBlendOutRate(aBlendTypeToArmIkRate[blendOutRate]);

			pPed->GetIkManager().Request(ikRequest);
		}
	}
}

#if FPS_MODE_SUPPORTED

bool CTaskMobilePhone::IsStateValidForFPSIK() const
{
	const CPed* pPed = GetPed();

	if (!pPed->IsLocalPlayer() || pPed->GetIsInCover())
		return false;

	if (GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false, false, false, true, false) || GetPed()->IsInFirstPersonVehicleCamera())
	{
		switch(GetState())
		{
		case(State_PutUpText):
		{
			float fTimeInState = GetTimeInState();
			if (pPed->GetVehiclePedInside() && pPed->GetVehiclePedInside()->GetIsAircraft() && fTimeInState <= 0.1f)
				return false;
		}
		case(State_TextLoop):
		case(State_HorizontalIntro):
		case(State_HorizontalLoop):
		case(State_HorizontalOutro):
		case(State_PutDownFromText):
		case(State_PutDownToText):
		case(State_EarLoop):
		case(State_PutToEarFromText):
		case(State_PutDownFromEar):
			return true;
		default:
			break;
		}
	}

	return false;
}

void CTaskMobilePhone::ProcessFPSArmIK()
{
	CPed* pPed = GetPed();
	
	m_NetworkHelper.SetFlag(true, ms_UseAlternateFilter);
	crFrameFilter* pFilter = g_FrameFilterDictionaryStore.FindFrameFilter(BONEMASK_ARMONLY_R);
	m_NetworkHelper.SetFilter(pFilter, ms_BonemaskFilter);

	CIkManager& ikManager = pPed->GetIkManager();

	if(!GetPed()->GetIsInVehicle())
	{
		// B*1991041: IK arm out of the way if we're blocked 
		static dev_float fProbeLength = 0.25f;
		static dev_float fForwardOffset = 1.0f;
		static dev_float fRadiusMult = 1.0f;

		// B*2087796: Offset the phone if we have a different aspect ratio (offsets generated using 16:9).
		const CViewport* viewport = gVpMan.GetGameViewport();
		const float fDefaultAspectRatio = 16.0f / 9.0f;
		const float fAspectRatio = viewport ? viewport->GetGrcViewport().GetAspectRatio() : fDefaultAspectRatio;
		float fAspectRatioModifier = 1.0f;
		if (fAspectRatio < fDefaultAspectRatio)
		{
			fAspectRatioModifier = fAspectRatio / fDefaultAspectRatio;
		}

		// B*2038701: Use a hard-coded offset relative to the camera for the blocking probe start position
		// (can't use the hand position as it has shifted due to the IK at this point)
		Vec3V vStartPosition(VECTOR3_TO_VEC3V(camInterface::GetPos()));
		TUNE_GROUP_FLOAT(ARM_IK, PHONE_BLOCK_PROBE_X_START, 0.100f, -25.0f, 25.0f, 0.01f);
		TUNE_GROUP_FLOAT(ARM_IK, PHONE_BLOCK_PROBE_Y_START, -0.03f, -25.0f, 25.0f, 0.01f);
		TUNE_GROUP_FLOAT(ARM_IK, PHONE_BLOCK_PROBE_Z_START, 0.160f, -25.0f, 25.0f, 0.01f);
		vStartPosition = AddScaled(vStartPosition, VECTOR3_TO_VEC3V(camInterface::GetRight()), ScalarV(PHONE_BLOCK_PROBE_X_START * fAspectRatioModifier));
		vStartPosition = AddScaled(vStartPosition, VECTOR3_TO_VEC3V(camInterface::GetUp()), ScalarV(PHONE_BLOCK_PROBE_Y_START));
		vStartPosition = AddScaled(vStartPosition, VECTOR3_TO_VEC3V(camInterface::GetFront()), ScalarV(PHONE_BLOCK_PROBE_Z_START));

		Vector3 vProbeStartPos = VEC3V_TO_VECTOR3(vStartPosition);
		Vector3 vProbeTargetPos = vProbeStartPos + (camInterface::GetFront() * fForwardOffset); 
		bool bIsBlocked = CTaskWeaponBlocked::IsWeaponBlocked(pPed, vProbeTargetPos, &vProbeStartPos, fProbeLength, fRadiusMult, false, false, true, fProbeLength);
		
		// If we're blocked, IK the arm/phone close to the ped 
		Vector3 vTargetOffset(Vector3::ZeroType);
		if (bIsBlocked)
		{
			TUNE_GROUP_FLOAT(ARM_IK, PHONE_BLOCK_X, 0.03f, -25.0f, 25.0f, 0.01f);
			TUNE_GROUP_FLOAT(ARM_IK, PHONE_BLOCK_Y,-0.15f, -25.0f, 25.0f, 0.01f);
			TUNE_GROUP_FLOAT(ARM_IK, PHONE_BLOCK_Z, 0.02f, -25.0f, 25.0f, 0.01f);
			vTargetOffset.SetX(PHONE_BLOCK_X * fAspectRatioModifier);
			vTargetOffset.SetY(PHONE_BLOCK_Y);
			vTargetOffset.SetZ(PHONE_BLOCK_Z);

			// Lerp the phone position to the target position
			if (m_vPrevPhoneIKOffset != vTargetOffset)
			{
				TUNE_GROUP_FLOAT(ARM_IK, PHONE_BLOCK_POS_LERP_RATE, 0.5f, 0.0f, 10.0f, 0.01f);
				m_vPrevPhoneIKOffset = Lerp(PHONE_BLOCK_POS_LERP_RATE, m_vPrevPhoneIKOffset, vTargetOffset);
				vTargetOffset = m_vPrevPhoneIKOffset;
			}
		}
		// If not blocked, IK arm to right hand side of screen in FPS mode:
		else
		{
			TUNE_GROUP_FLOAT(ARM_IK, PHONE_XOFFSET_ARMFPS, 0.08f, -25.0f, 25.0f, 0.01f);
			TUNE_GROUP_FLOAT(ARM_IK, PHONE_YOFFSET_ARMFPS, 0.0f,  -25.0f, 25.0f, 0.01f);
			TUNE_GROUP_FLOAT(ARM_IK, PHONE_ZOFFSET_ARMFPS, 0.025f, -25.0f, 25.0f, 0.01f);

			// Don't use IK offset if in ear loop (still need IK though to ensure phone doesn't clip the camera)
			if ( GetState() == State_EarLoop )
			{
				if (pPed->GetMotionData() && pPed->GetMotionData()->GetIsSprinting())
				{
					// B*2211690: Use slight IK offset if ped is sprinting. The pose shift from strafing caused some dodgy arm issues and slight camera clipping.
					TUNE_GROUP_FLOAT(ARM_IK, PHONE_XOFFSET_EARLOOP_ARMFPS, 0.025f, -25.0f, 25.0f, 0.01f);
					TUNE_GROUP_FLOAT(ARM_IK, PHONE_YOFFSET_EARLOOP_ARMFPS, 0.05f,  -25.0f, 25.0f, 0.01f);
					TUNE_GROUP_FLOAT(ARM_IK, PHONE_ZOFFSET_EARLOOP_ARMFPS, 0.0f, -25.0f, 25.0f, 0.01f);
					vTargetOffset.SetX(PHONE_XOFFSET_EARLOOP_ARMFPS * fAspectRatioModifier);
					vTargetOffset.SetY(PHONE_YOFFSET_EARLOOP_ARMFPS);
					vTargetOffset.SetZ(PHONE_ZOFFSET_EARLOOP_ARMFPS);
				}
				else
				{
					vTargetOffset.SetX(0.0f);
					vTargetOffset.SetY(0.0f);
					vTargetOffset.SetZ(0.0f);
				}
			}
			else
			{
				vTargetOffset.SetX(PHONE_XOFFSET_ARMFPS * fAspectRatioModifier);
				vTargetOffset.SetY(PHONE_YOFFSET_ARMFPS);
				vTargetOffset.SetZ(PHONE_ZOFFSET_ARMFPS);
			}
			
			// Lerp the phone position to the target position
			if ( m_vPrevPhoneIKOffset != vTargetOffset )
			{
				TUNE_GROUP_FLOAT(ARM_IK, PHONE_UNBLOCK_POS_LERP_RATE, 0.1f, 0.0f, 10.0f, 0.01f);
				m_vPrevPhoneIKOffset = Lerp(PHONE_UNBLOCK_POS_LERP_RATE, m_vPrevPhoneIKOffset, vTargetOffset);
				vTargetOffset = m_vPrevPhoneIKOffset;
			}
		}

		if (m_pFirstPersonIkHelper == NULL)
		{
			m_pFirstPersonIkHelper = rage_new CFirstPersonIkHelper(CFirstPersonIkHelper::FP_MobilePhone);
			Assert(m_pFirstPersonIkHelper);
			m_pFirstPersonIkHelper->SetUseRecoil(false);
			m_pFirstPersonIkHelper->SetUseAnimBlockingTags(false);
		}

		if (m_pFirstPersonIkHelper)
		{
			if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
			{
				m_pFirstPersonIkHelper->SetBlendInRate(ARMIK_BLEND_RATE_INSTANT);
				m_pFirstPersonIkHelper->SetBlendOutRate(ARMIK_BLEND_RATE_SLOW);
			}

			// If we are going to switch cameras next frame, instantly blend out the IK
			if(pPed->GetPlayerInfo())
			{
				if(pPed->GetPlayerInfo()->GetSwitchedToOrFromFirstPersonCount() == 2)
				{
					m_pFirstPersonIkHelper->SetBlendOutRate(ARMIK_BLEND_RATE_INSTANT);
				}
			}

			m_pFirstPersonIkHelper->SetOffset(RCC_VEC3V(vTargetOffset));
			m_pFirstPersonIkHelper->Process(GetPed());
		}
	}
	else if(pPed->IsInFirstPersonVehicleCamera() && pPed->GetVehiclePedInside())
	{
		TUNE_GROUP_BOOL(ARM_IK, INVEHICLE_PHONE_IK, true);
		if(INVEHICLE_PHONE_IK)
		{
			CVehicleModelInfo *pModelInfo = pPed->GetVehiclePedInside()->GetVehicleModelInfo();
			if(pModelInfo)
			{
				Vector3 vOffset = Vector3(0,0,0);

				//Additional offset was added to fix initial work, after base phone position has changed
				TUNE_GROUP_BOOL(ARM_IK, INVEHICLE_PHONE_IK_OFFSET, true);
				TUNE_GROUP_BOOL(ARM_IK, INVEHICLE_PHONE_IK_POSITION, false);
				if(INVEHICLE_PHONE_IK_OFFSET)
				{
					if(pPed->GetIsDrivingVehicle())
					{
						TUNE_GROUP_FLOAT(ARM_IK, PHONE_INVEHICLE_OFFSET_X, 0.075f, -25.0f, 25.0f, 0.01f);
						TUNE_GROUP_FLOAT(ARM_IK, PHONE_INVEHICLE_OFFSET_Y, -0.031f, -25.0f, 25.0f, 0.01f);
						TUNE_GROUP_FLOAT(ARM_IK, PHONE_INVEHICLE_OFFSET_Z, -.12f, -25.0f, 25.0f, 0.01f);
						vOffset = pModelInfo->GetFirstPersonMobilePhoneOffset();

						if (INVEHICLE_PHONE_IK_POSITION) {
							TUNE_GROUP_FLOAT(ARM_IK, PHONE_INVEHICLE_X, vOffset.x, -25.0f, 25.0f, 0.01f);
							TUNE_GROUP_FLOAT(ARM_IK, PHONE_INVEHICLE_Y, vOffset.y, -25.0f, 25.0f, 0.01f);
							TUNE_GROUP_FLOAT(ARM_IK, PHONE_INVEHICLE_Z, vOffset.z, -25.0f, 25.0f, 0.01f);
							vOffset = Vector3(PHONE_INVEHICLE_X, PHONE_INVEHICLE_Y,PHONE_INVEHICLE_Z);
						}

						vOffset += Vector3(PHONE_INVEHICLE_OFFSET_X, PHONE_INVEHICLE_OFFSET_Y, PHONE_INVEHICLE_OFFSET_Z);
					}
					else
					{
						TUNE_GROUP_FLOAT(ARM_IK, PHONE_INVEHICLE_PASSENGER_OFFSET_X, 0.0f, -25.0f, 25.0f, 0.01f);
						TUNE_GROUP_FLOAT(ARM_IK, PHONE_INVEHICLE_PASSENGER_OFFSET_Y, 0.0f, -25.0f, 25.0f, 0.01f);
						TUNE_GROUP_FLOAT(ARM_IK, PHONE_INVEHICLE_PASSENGER_OFFSET_Z, 0.0f, -25.0f, 25.0f, 0.01f);
						s32 iSeat = pPed->GetVehiclePedInside()->GetSeatManager()->GetPedsSeatIndex(pPed);
						vOffset = pModelInfo->GetPassengerMobilePhoneIKOffset(iSeat);

						if (INVEHICLE_PHONE_IK_POSITION) {
							TUNE_GROUP_FLOAT(ARM_IK, PHONE_INVEHICLE_PASSENGER_X, vOffset.x, -25.0f, 25.0f, 0.01f);
							TUNE_GROUP_FLOAT(ARM_IK, PHONE_INVEHICLE_PASSENGER_Y, vOffset.y, -25.0f, 25.0f, 0.01f);
							TUNE_GROUP_FLOAT(ARM_IK, PHONE_INVEHICLE_PASSENGER_Z, vOffset.z, -25.0f, 25.0f, 0.01f);
							vOffset = Vector3(PHONE_INVEHICLE_PASSENGER_X, PHONE_INVEHICLE_PASSENGER_Y,PHONE_INVEHICLE_PASSENGER_Z);
						}

						vOffset += Vector3(PHONE_INVEHICLE_PASSENGER_OFFSET_X, PHONE_INVEHICLE_PASSENGER_OFFSET_Y, PHONE_INVEHICLE_PASSENGER_OFFSET_Z);
					}
				}

				TUNE_GROUP_FLOAT(ARM_IK, PHONE_INVEHICLE_BLEND_IN_TIME, 0.5f, 0.0f, 2.0f, 0.01f);
				TUNE_GROUP_FLOAT(ARM_IK, PHONE_INVEHICLE_BLEND_OUT_TIME, 0.5f, 0.0f, 2.0f, 0.01f);

				if(vOffset.Mag2() > 0.01f)
				{
					ikManager.SetRelativeArmIKTarget(crIKSolverArms::RIGHT_ARM, pPed, BONETAG_INVALID, vOffset, AIK_USE_ANIM_BLOCK_TAGS | AIK_TARGET_WRT_IKHELPER, PHONE_INVEHICLE_BLEND_IN_TIME, PHONE_INVEHICLE_BLEND_OUT_TIME);
				}
			}
		}
	}
}
#endif	//FPS_MODE_SUPPORTED
