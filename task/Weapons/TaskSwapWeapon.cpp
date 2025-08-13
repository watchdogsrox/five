// File header
#include "Task/Weapons/TaskSwapWeapon.h"

#include "fwanimation/animdirector.h"

// Game headers
#include "animation/AnimManager.h"
#include "animation/Move.h"
#include "animation/MovePed.h"
#include "camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "Event/EventPriorities.h"
#include "Event/EventShocking.h"
#include "Event/ShockingEvents.h"
#include "Frontend/NewHud.h"
#include "Network/Objects/Entities/NetObjPlayer.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PedMotionData.h"
#include "Task/Combat/Cover/TaskCover.h"
#include "Task/Motion/Locomotion/TaskMotionPed.h"
#include "Task/Weapons/Gun/TaskAimGunOnFoot.h"
#include "Weapons/Weapon.h"
#include "Weapons/WeaponManager.h"
#include "Weapons/Components/WeaponComponent.h"
#include "Weapons/Info/WeaponInfo.h"
#include "Weapons/Info/WeaponInfoManager.h"
#include "Weapons/Info/WeaponSwapData.h"

// Macro to disable optimisations if set
AI_OPTIMISATIONS()
AI_WEAPON_OPTIMISATIONS()
AI_COVER_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CTaskSwapWeapon
//////////////////////////////////////////////////////////////////////////

const u32 g_InvalidCameraHash = ATSTRINGHASH("INVALID_CAMERA", 0x05aad591b);

////////////////////////////////////////////////////////////////////////////////

// Statics
CTaskSwapWeapon::Tunables CTaskSwapWeapon::ms_Tunables;

IMPLEMENT_WEAPON_TASK_TUNABLES(CTaskSwapWeapon, 0x3aa184f8);

////////////////////////////////////////////////////////////////////////////////

fwMvClipId CTaskSwapWeapon::ms_HolsterClipId("holster",0xCA21928E);
fwMvClipId CTaskSwapWeapon::ms_UnholsterClipId("unholster",0x2DA8D93A);
fwMvFilterId CTaskSwapWeapon::ms_BothArmsNoSpineFilterId("BothArms_NoSpine_filter",0x6C2DD2B3);
fwMvFilterId CTaskSwapWeapon::ms_RightArmNoSpineFilterId("RightArm_NoSpine_filter",0x72BC0F25);

// Move signals
const fwMvBooleanId CTaskSwapWeapon::ms_BlendOutId("BlendOut",0x6DD1ABBC);
const fwMvBooleanId CTaskSwapWeapon::ms_ObjectCreateId("Create",0x629712B0);
const fwMvBooleanId CTaskSwapWeapon::ms_ObjectDestroyId("Destroy",0x6D957FEE);
const fwMvBooleanId CTaskSwapWeapon::ms_ObjectReleaseId("Release",0xBC7C13B5);
const fwMvBooleanId CTaskSwapWeapon::ms_InterruptToAimId("Aim_Breakout",0x80AD35E);
const fwMvBooleanId CTaskSwapWeapon::ms_BlendOutLeftArmId("BlendOutLeftArm",0xE6338E3);
const fwMvClipId CTaskSwapWeapon::ms_ClipId("Clip",0xE71BD32A);
const fwMvClipId CTaskSwapWeapon::ms_GripClipId("GripClip",0x9C688F9B);
const fwMvFilterId CTaskSwapWeapon::ms_FilterId("Filter",0x49DC73B3);
const fwMvFlagId CTaskSwapWeapon::ms_UseGripClipId("UseGripClip",0x72838804);
const fwMvFloatId CTaskSwapWeapon::ms_PhaseId("Phase",0xA27F482B);
const fwMvFloatId CTaskSwapWeapon::ms_RateId("Rate",0x7E68C088);

////////////////////////////////////////////////////////////////////////////////

CTaskSwapWeapon::CTaskSwapWeapon(s32 iFlags)
: m_pFilter(NULL)
, m_Filter(FILTER_ID_INVALID)
, m_DrawClipSetId(CLIP_SET_ID_INVALID)
, m_DrawClipId(CLIP_ID_INVALID)
, m_iFlags(iFlags)
, m_uDrawingWeapon(0)
, m_uInitialDrawingWeapon(0)
, m_fPhase(0.0f)
, m_bMoveBlendOut(false)
, m_bMoveBlendOutLeftArm(false)
, m_bMoveObjectDestroy(false)
, m_bMoveObjectRelease(false)
, m_bMoveInterruptToAim(false)
, m_bMoveObjectCreate(false)
, m_bFPSwappingToOrFromUnarmed(false)
, m_bFPSSwappingFromProjectile(false)
{
	SetInternalTaskType(CTaskTypes::TASK_SWAP_WEAPON);
}

CTaskSwapWeapon::CTaskSwapWeapon(s32 iFlags, u32 drawingWeapon)
	: m_pFilter(NULL)
	, m_Filter(FILTER_ID_INVALID)
	, m_DrawClipSetId(CLIP_SET_ID_INVALID)
	, m_DrawClipId(CLIP_ID_INVALID)
	, m_iFlags(iFlags)
	, m_uDrawingWeapon(drawingWeapon)
	, m_uInitialDrawingWeapon(drawingWeapon)
	, m_fPhase(0.0f)
	, m_bMoveBlendOut(false)
	, m_bMoveBlendOutLeftArm(false)
	, m_bMoveObjectDestroy(false)
	, m_bMoveObjectRelease(false)
	, m_bMoveInterruptToAim(false)
	, m_bMoveObjectCreate(false)
{
	SetInternalTaskType(CTaskTypes::TASK_SWAP_WEAPON);
}

CTaskInfo* CTaskSwapWeapon::CreateQueriableState() const
{
	u32 equippedWeapon = (GetPed() && GetPed()->GetWeaponManager()) ? GetPed()->GetWeaponManager()->GetEquippedWeaponHash() : 0;

	return rage_new CClonedSwapWeaponInfo(equippedWeapon, m_iFlags.GetAllFlags());
}

CTaskSwapWeapon::FSM_Return CTaskSwapWeapon::ProcessPreFSM()
{
	CPed* pPed = GetPed();

	if(pPed->GetUsingRagdoll())
	{
		return FSM_Quit;
	}

	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_IsSwitchingWeapon, true);
	pPed->SetPedResetFlag(CPED_RESET_FLAG_TemporarilyBlockWeaponEquipping, true);
#if FPS_MODE_SUPPORTED
	pPed->SetPedResetFlag(CPED_RESET_FLAG_ProcessPostCamera, true);
	if(pPed->UseFirstPersonUpperBodyAnims(true) && !pPed->GetIsParachuting())
	{
		pPed->SetIsStrafing(true);
	}
#endif // FPS_MODE_SUPPORTED

	if(m_MoveNetworkHelper.IsNetworkActive())
	{
		// Keep track of the phase
		m_fPhase = m_MoveNetworkHelper.GetFloat(ms_PhaseId);
	}

	return FSM_Continue;
}

CTaskSwapWeapon::FSM_Return CTaskSwapWeapon::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		FSM_State(State_Init)
			FSM_OnEnter
				return StateInit_OnEnter();
			FSM_OnUpdate
				return StateInit_OnUpdate();
		FSM_State(State_WaitForLoad)
			FSM_OnUpdate
				return StateWaitForLoad_OnUpdate();
		FSM_State(State_WaitToHolster)
			FSM_OnUpdate
				return StateWaitToHolster_OnUpdate();
		FSM_State(State_Holster)
			FSM_OnEnter
				return StateHolster_OnEnter();
			FSM_OnUpdate
				return StateHolster_OnUpdate();
		FSM_State(State_WaitToDraw)
			FSM_OnEnter
				return StateWaitToDraw_OnEnter();
			FSM_OnUpdate
				return StateWaitToDraw_OnUpdate();
		FSM_State(State_Draw)
			FSM_OnEnter
				return StateDraw_OnEnter();
			FSM_OnUpdate
				return StateDraw_OnUpdate();
		FSM_State(State_Quit)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

CTaskSwapWeapon::FSM_Return CTaskSwapWeapon::UpdateClonedFSM(const s32 iState, const FSM_Event iEvent)
{
	return UpdateFSM(iState, iEvent);
}

void CTaskSwapWeapon::GetOverriddenGameplayCameraSettings(tGameplayCameraSettings& settings) const
{
	if(GetPed()->GetIsSwimming())
	{
		//Maintain swim camera
		settings.m_CameraHash = ATSTRINGHASH("FOLLOW_PED_IN_WATER_CAMERA", 0x0b2c08bfd);
	}
	else
	{
		//Block any attempts to request an aim camera and fall-back to the default gameplay camera.
		settings.m_CameraHash = g_InvalidCameraHash;
	}
}

void CTaskSwapWeapon::GenerateShockingEvent(CPed& ped, u32 UNUSED_PARAM(lastWeaponDrawn))
{
	// Note: this check used to be in CShockingEventsManager::EventShouldBeAdded(), but it was moved
	// to here where the event is generated, instead.
	// TODO: Perhaps the PEDTYPE_COP check here should be replaced by a ped metadata setting.
	// Also, the vehicle flag might not be needed, not sure if this task is every used in vehicles
	// or not.
	if(ped.GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) || ped.GetPedType() == PEDTYPE_COP)
	{
		return;
	}

	// Add event
	CEventShockingVisibleWeapon ev(ped);
	CShockingEventsManager::Add(ev);

	// TODO: Re-implement the different types of VisibleWeapon shocking events,
	// CShockingEventsManager used to do this in ConvertVisibleWeaponEType(),
	// from lastWeaponDrawn.
}

void CTaskSwapWeapon::CleanUp()
{
	// Clear any old filter
	if(m_pFilter)
	{
		m_pFilter->Release();
		m_pFilter = NULL;
	}

	CPed* pPed = GetPed();
	if(pPed)
	{
		float fBlendOutDuration = ms_Tunables.m_BlendOutDuration;

#if FPS_MODE_SUPPORTED
		if(pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
		{
			TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, fFPSSwapBlendOutTime, 0.0f, 0.0f, 2.0f, 0.01f);
			fBlendOutDuration = fFPSSwapBlendOutTime;

			if(m_bFPSwappingToOrFromUnarmed)
			{
				//Only use non zero blend out time when in idle state
				if(pPed->GetMotionData()->GetIsFPSIdle())
				{
					TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, fFPSSwapBlendOutUnarmed, 0.25f, 0.0f, 2.0f, 0.01f);
					fBlendOutDuration = fFPSSwapBlendOutUnarmed;
				}

				TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, fFPSSwapBlendOutUnarmedToMeleeOverride, 0.0f, 0.0f, 2.0f, 0.01f);
				if(GetDrawingWeaponInfo() && GetDrawingWeaponInfo()->GetIsMelee() && !GetDrawingWeaponInfo()->GetIsUnarmed())
				{
					fBlendOutDuration = fFPSSwapBlendOutUnarmedToMeleeOverride;
				}
			}

			TUNE_GROUP_FLOAT(FIRST_PERSON_TUNE, fFPSSwapBlendOutFromProjectile, 0.15f, 0.0f, 2.0f, 0.01f);
			if(m_bFPSSwappingFromProjectile)
			{
				fBlendOutDuration = fFPSSwapBlendOutFromProjectile;
			}
		}
#endif

		if (GetFlags().IsFlagSet(SWAP_SECONDARY))
		{
			pPed->GetMovePed().ClearSecondaryTaskNetwork(m_MoveNetworkHelper, fBlendOutDuration);
		}
		else
		{
			pPed->GetMovePed().ClearTaskNetwork(m_MoveNetworkHelper, fBlendOutDuration);
		}

		CTaskMotionBase* pMotionTask = pPed->GetPrimaryMotionTask();
		if(pMotionTask && pMotionTask->GetTaskType() == CTaskTypes::TASK_MOTION_PED FPS_MODE_SUPPORTED_ONLY(&& !pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true)))
		{
			static_cast<CTaskMotionPed*>(pMotionTask)->SetRestartAimingUpperBodyStateThisFrame(true);
		}

		CTaskGun* pGunTask = static_cast<CTaskGun*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_GUN));
		if(pGunTask)
		{
			pGunTask->SetBlockFiringTime(fwTimer::GetTimeInMilliseconds() + (u32)(Max(fBlendOutDuration, fwTimer::GetTimeStep()) * 1000.0f));
		}

		//I may have gotten into cover since this task started and now need to tell the cover task to change anims 
		CTaskInCover* pInCoverTask = static_cast<CTaskInCover*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_IN_COVER));
		if (pInCoverTask)
		{
			pInCoverTask->RestartWeaponHolding();
		}

		if (pPed->IsNetworkClone())
		{
			CNetObjPed* pNetObjPed = (CNetObjPed*)pPed->GetNetworkObject();

			pNetObjPed->SetTaskOverridingPropsWeapons(false);
		}

		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_IsSwitchingWeapon, false);
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_IsHolsteringWeapon, false);
	}
}

bool CTaskSwapWeapon::ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
	// Never abort for EVENT_PLAYER_LOCK_ON_TARGET as this causes interference between group events.
	// This task is so brief that we can let the weapons switch take place, and then respond.
	if(pEvent && ((CEvent*)pEvent)->GetEventType() == EVENT_PLAYER_LOCK_ON_TARGET)
	{
		return false;
	}

	if(iPriority == ABORT_PRIORITY_IMMEDIATE || (iPriority == ABORT_PRIORITY_URGENT && (!pEvent || ((CEvent*)pEvent)->GetEventPriority() >= E_PRIORITY_BLOCK_NON_TEMP_EVENTS)))
	{
		// Base class
		return CTask::ShouldAbort(iPriority, pEvent);
	}

	return false;
}

void CTaskSwapWeapon::DoAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
	CPed* pPed = GetPed();

	if(!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PedBeingDeleted) && m_iFlags.IsFlagSet(SWAP_DRAW) && (m_iFlags.IsFlagSet(SWAP_ABORT_SET_DRAWN) || iPriority == ABORT_PRIORITY_IMMEDIATE))
	{
		CreateWeapon();
	}

	// Base class
	CTask::DoAbort(iPriority, pEvent);
}

#if FPS_MODE_SUPPORTED
bool CTaskSwapWeapon::ProcessPostCamera()
{
	CPed* pPed = GetPed();
	if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		CTaskAimGunOnFoot::PostCameraUpdate(pPed, m_iFlags.IsFlagSet(GF_DisableTorsoIk));
	}
	return false;
}
#endif // FPS_MODE_SUPPORTED

bool CTaskSwapWeapon::ProcessMoveSignals()
{
	//Check the state.
	switch(GetState())
	{
		case State_Holster:
		{
			return StateHolster_OnProcessMoveSignals();
		}
		case State_Draw:
		{
			return StateDraw_OnProcessMoveSignals();
		}
		default:
		{
			break;
		}
	}

	return false;
}

#if !__FINAL
const char* CTaskSwapWeapon::GetStaticStateName(s32 iState)
{
	switch(iState)
	{
	case State_Init:			return "Init";
	case State_WaitForLoad:		return "WaitForLoad";
	case State_WaitToHolster:	return "WaitForHolster";
	case State_Holster:			return "Holster";
	case State_WaitToDraw:		return "WaitToDraw";
	case State_Draw:			return "Draw";
	case State_Quit:			return "Quit";
	default:
		taskAssertf(0, "Unhandled state %d", iState);
		return "Unknown";
	}
}
#endif // !__FINAL

CTaskSwapWeapon::FSM_Return CTaskSwapWeapon::StateInit_OnEnter()
{
	CPed* pPed = GetPed();

	if (pPed->IsNetworkClone())
	{
		CNetObjPed* pNetObjPed = (CNetObjPed*)pPed->GetNetworkObject();

		// prevent the ped creating the weapon object
		pNetObjPed->SetTaskOverridingPropsWeapons(true);

		// the drawing weapon may have changed
		m_uDrawingWeapon = GetCloneDrawingWeapon();

		if (m_uDrawingWeapon == pPed->GetWeaponManager()->GetEquippedWeaponHash())
		{
			return FSM_Quit;
		}

		pPed->GetWeaponManager()->EquipWeapon(m_uDrawingWeapon, -1, false); 

		pPed->GetWeaponManager()->SetCreateWeaponObjectWhenLoaded(false);
	}
	else if (pPed->GetNetworkObject())
	{
		// force an immediate swap weapon task update
		static_cast<CNetObjPlayer*>(pPed->GetNetworkObject())->RequestForceSendOfTaskState();
	}

	return FSM_Continue;
}

CTaskSwapWeapon::FSM_Return CTaskSwapWeapon::StateInit_OnUpdate()
{
	if (GetPed()->IsNetworkClone())
	{
		// Ensure weapon is streamed in
		u32 equippedWeaponHash = GetPed()->GetWeaponManager()->GetEquippedWeaponHash();
		if(equippedWeaponHash && !GetPed()->GetInventory()->GetIsStreamedIn(equippedWeaponHash))
		{
			return FSM_Continue;
		}
	}

	// Just create the weapon and quit
	if(GetShouldSwapInstantly(GetDrawingWeaponInfo()) 
#if __DEV
		|| ms_Tunables.m_DebugSwapInstantly)
#else // __DEV
		)
#endif // __DEV
	{
		CTaskInCover* pInCoverTask = static_cast<CTaskInCover*>(GetPed()->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_IN_COVER));
		if (pInCoverTask)
		{
			pInCoverTask->RestartWeaponHolding();
		}

		if(!CreateWeapon())
		{
			SetState(State_WaitForLoad);
			return FSM_Continue;
		}
		return FSM_Quit;
	}

	bool bUsingUnderwaterGun = false;
	bool bUsingWeaponForcingHolster = false;

	CPed *pPed = GetPed();
	if(pPed)
	{
		// This is the weapon we're coming from, not the weapon being swapped to
		CWeapon* pCurrentlyEquippedWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
		const CWeaponInfo *pWeaponInfo = pCurrentlyEquippedWeapon ? pCurrentlyEquippedWeapon->GetWeaponInfo() : NULL;
		
		//B*1800599: Play holster anim for harpoon gun if we're in water
		if(pWeaponInfo && pWeaponInfo->GetIsUnderwaterGun() && pPed->GetIsSwimming())
		{
			bUsingUnderwaterGun = true;
		}

		if (pWeaponInfo && pWeaponInfo->GetUseHolsterAnimation() && !pPed->GetIsInCover() && !pPed->IsFirstPersonShooterModeEnabledForPlayer())
		{
			// For the DOUBLEACTION, we only trigger this under certain conditions
			if (pWeaponInfo->GetHash() == ATSTRINGHASH("WEAPON_DOUBLEACTION", 0x97ea20b8))
			{
				bool bIsSwitchingToUnarmed = GetDrawingWeaponInfo() && GetDrawingWeaponInfo()->GetIsUnarmed();
				bool bIsRunningOrSprinting = pPed->GetMotionData()->GetIsRunning() || pPed->GetMotionData()->GetIsSprinting();
				u32 uTimeSinceLastBattleEvent = fwTimer::GetTimeInMilliseconds() - pPed->GetPedIntelligence()->GetLastBattleEventTime();
		
				if (bIsSwitchingToUnarmed && !bIsRunningOrSprinting && uTimeSinceLastBattleEvent > 4000 )
				{
					bUsingWeaponForcingHolster = true;
				}
			}
			else
			{
				bUsingWeaponForcingHolster = true;
			}
		}

		//B*1903602 If we're swapping a weapon with a flashlight in multiplayer, set active light history to off
		if (pWeaponInfo && pPed->GetInventory())
		{
			CWeaponItem* pWeaponItem = pPed->GetInventory()->GetWeapon(pWeaponInfo->GetHash());
			if(pWeaponItem)
			{
				if(pWeaponItem->GetComponents())
				{
					const CWeaponItem::Components& components = *pWeaponItem->GetComponents();
					for(s32 i = 0; i < components.GetCount(); i++)
					{
						if(components[i].pComponentInfo->GetClassId() == WCT_FlashLight)
						{
							pWeaponItem->SetActiveComponent(i, false);
						}
					}
				}
			}
		}
	}

	if((ms_Tunables.m_SkipHolsterWeapon && !bUsingUnderwaterGun && !bUsingWeaponForcingHolster) && !GetShouldDrawWeapon())
	{
		// Ensure equipped weapon has been removed
		GetPed()->GetWeaponManager()->DestroyEquippedWeaponObject(true OUTPUT_ONLY(,"CTaskSwapWeapon::StateInit_OnUpdate"));
	}
	
	if((!ms_Tunables.m_SkipHolsterWeapon || bUsingUnderwaterGun || bUsingWeaponForcingHolster) && GetShouldHolsterWeapon())
	{
		SetState(State_WaitToHolster);
		return FSM_Continue;
	}
	else if(GetShouldDrawWeapon())
	{
		SetState(State_WaitToDraw);
		return FSM_Continue;
	}
	else
	{
		// No weapon to holster or draw - shouldn't get here
		return FSM_Quit;
	}
}

CTaskSwapWeapon::FSM_Return CTaskSwapWeapon::StateWaitForLoad_OnUpdate()
{
	if(CreateWeapon())
	{
		return FSM_Quit;
	}
	return FSM_Continue; 
}

CTaskSwapWeapon::FSM_Return CTaskSwapWeapon::StateWaitToHolster_OnUpdate()
{
	if(!GetShouldHolsterWeapon())
	{
		CTaskInCover* pInCoverTask = static_cast<CTaskInCover*>(GetPed()->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_IN_COVER));
		if (pInCoverTask)
		{
			pInCoverTask->RestartWeaponHolding();
		}
		// We should be holstering a weapon, so quit
		return FSM_Quit;
	}

	const CWeaponInfo* pWeaponInfo = GetHolsteringWeaponInfo();
	if(!pWeaponInfo)
	{
		// Weapon has become invalid
		return FSM_Quit;
	}

	if(GetIsAnimationStreamed(pWeaponInfo->GetHash()))
	{
		if(StartMoveNetwork(pWeaponInfo))
		{
			// Init params
			m_Filter = GetDefaultFilterId(pWeaponInfo);
			// If we're waiting, always start from 0.0f
			m_fPhase = 0.f;

			// Ready to holster weapon
			GetPed()->SetPedConfigFlag(CPED_CONFIG_FLAG_IsHolsteringWeapon, true);
			SetState(State_Holster);
		}
	}

	return FSM_Continue;
}

CTaskSwapWeapon::FSM_Return CTaskSwapWeapon::StateHolster_OnEnter()
{
	const CWeaponInfo* pWeaponInfo = GetHolsteringWeaponInfo();
	if(!pWeaponInfo)
	{
		return FSM_Quit;
	}

	taskAssert(GetIsAnimationStreamed(pWeaponInfo->GetHash()));

	fwMvClipSetId clipSetId = GetClipSet(pWeaponInfo->GetHash());
	fwMvClipId clipId = GetSwapWeaponClipId(pWeaponInfo);

	// If clip doesn't exist, default standing holster
	if(clipId == CLIP_ID_INVALID || !fwAnimManager::GetClipIfExistsBySetId(clipSetId, clipId))
	{
		clipId = ms_HolsterClipId;
	}

	const crClip* pClip = fwAnimManager::GetClipIfExistsBySetId(clipSetId, clipId);
	if(taskVerifyf(pClip, "pClip doesn't exist [%s][%s]", clipSetId.TryGetCStr(), clipId.TryGetCStr()))
	{
		m_MoveNetworkHelper.SetClip(pClip, ms_ClipId);
		m_MoveNetworkHelper.SetFloat(ms_PhaseId, m_fPhase);
		m_MoveNetworkHelper.SetFloat(ms_RateId, GetClipPlayBackRate(pWeaponInfo));
		SetFilter(m_Filter);

		RequestProcessMoveSignalCalls();
		m_bMoveBlendOut			= false;
		m_bMoveBlendOutLeftArm	= false;
		m_bMoveObjectDestroy	= false;
		m_bMoveObjectRelease	= false;

		// If weapon clips exist, play them at the same time
		fwMvClipId weaponClipId = GetSwapWeaponClipIdForWeapon(pWeaponInfo);
		if (weaponClipId != CLIP_ID_INVALID && fwAnimManager::GetClipIfExistsBySetId(clipSetId, weaponClipId))
		{
			CWeapon* pCurrentlyEquippedWeapon = GetPed()->GetWeaponManager()->GetEquippedWeapon();
			if (pCurrentlyEquippedWeapon)
			{
				pCurrentlyEquippedWeapon->StartAnim(clipSetId, weaponClipId, INSTANT_BLEND_DURATION, GetClipPlayBackRate(pWeaponInfo), 0.0f, false, false, true);
			}
		}

		return FSM_Continue;
	}
	else
	{
		return FSM_Quit;
	}
}

CTaskSwapWeapon::FSM_Return CTaskSwapWeapon::StateHolster_OnUpdate()
{
	const CWeaponInfo* pWeaponInfo = GetHolsteringWeaponInfo();

	bool bMoveBlendOut = m_bMoveBlendOut;
	m_bMoveBlendOut = false;

	bool bMoveBlendOutLeftArm = m_bMoveBlendOutLeftArm;
	m_bMoveBlendOutLeftArm = false;

	bool bMoveObjectDestroy = m_bMoveObjectDestroy;
	m_bMoveObjectDestroy = false;

	bool bMoveObjectRelease = m_bMoveObjectRelease;
	m_bMoveObjectRelease = false;

	CPed* pPed = GetPed();

	if(!m_MoveNetworkHelper.IsInTargetState())
	{
		return FSM_Continue;
	}

	if(bMoveBlendOut)
	{
		// Ensure equipped weapon has been removed
		pPed->GetWeaponManager()->DestroyEquippedWeaponObject(true OUTPUT_ONLY(,"CTaskSwapWeapon::StateHolster_OnUpdate"));

		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_IsHolsteringWeapon, false);
		SetState(State_WaitToDraw);
		return FSM_Continue;
	}

	if(bMoveBlendOutLeftArm && !m_iFlags.IsFlagSet(SWAP_TO_AIM))
	{
		// Only change filter if different
		if(m_Filter != ms_RightArmNoSpineFilterId)
		{
			if(StartMoveNetwork(pWeaponInfo))
			{
				m_Filter = ms_RightArmNoSpineFilterId;
				SetFlag(aiTaskFlags::RestartCurrentState);
				return FSM_Continue;
			}
		}
	}

	if(bMoveObjectDestroy)
	{
		pPed->GetWeaponManager()->DestroyEquippedWeaponObject(true OUTPUT_ONLY(,"CTaskSwapWeapon::StateHolster_OnUpdate:2"));
	}
	else if(bMoveObjectRelease)
	{
		if(pWeaponInfo)
		{
			if(!pWeaponInfo->GetIsUnarmed())
			{
				pPed->GetWeaponManager()->DropWeapon(pWeaponInfo->GetHash(), true);
			}

			// Don't unholster after discarding
			// For petrol can
			m_iFlags.ClearFlag(SWAP_DRAW);
		}
	}

	return FSM_Continue;
}

bool CTaskSwapWeapon::StateHolster_OnProcessMoveSignals()
{
	m_bMoveBlendOut			|= m_MoveNetworkHelper.GetBoolean(ms_BlendOutId);
	m_bMoveBlendOutLeftArm	|= m_MoveNetworkHelper.GetBoolean(ms_BlendOutLeftArmId);
	m_bMoveObjectDestroy	|= m_MoveNetworkHelper.GetBoolean(ms_ObjectDestroyId);
	m_bMoveObjectRelease	|= m_MoveNetworkHelper.GetBoolean(ms_ObjectReleaseId);

	return true;
}

CTaskSwapWeapon::FSM_Return	CTaskSwapWeapon::StateWaitToDraw_OnEnter()
{
	// Reset variables
	m_DrawClipSetId = CLIP_SET_ID_INVALID;
	m_DrawClipId = CLIP_ID_INVALID;

	//Set up the clipset and clip so we stream the correct ones. 
	const CWeaponInfo* pWeaponInfo = GetDrawingWeaponInfo();
	if(pWeaponInfo)
	{
		// Store the hash of the weapon we are drawing
		if (!GetPed()->IsNetworkClone() || m_uDrawingWeapon == 0)
		{
			m_uDrawingWeapon = pWeaponInfo->GetHash();
		}

		if(m_DrawClipSetId == CLIP_SET_ID_INVALID && m_DrawClipId == CLIP_ID_INVALID)
		{
			m_DrawClipSetId = GetClipSet(m_uDrawingWeapon);

			//If using harpoon gun on land, use the fallback clip set
			if (pWeaponInfo->GetIsUnderwaterGun() && !GetPed()->GetIsSwimming())
			{
				fwClipSet *pClipSet = fwClipSetManager::GetClipSet(m_DrawClipSetId);
				if (pClipSet)
				{
					m_DrawClipSetId = pClipSet->GetFallbackId();
				}
			}

#if FPS_MODE_SUPPORTED
			// Temporary first person hack to override drawing weapon with equipped weapon clipsets
			if(GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false))
			{
				m_bFPSwappingToOrFromUnarmed = (GetDrawingWeaponInfo() && GetDrawingWeaponInfo()->GetIsUnarmed()) || (GetHolsteringWeaponInfo() && GetHolsteringWeaponInfo()->GetIsUnarmed());
				m_bFPSSwappingFromProjectile = GetHolsteringWeaponInfo() && GetHolsteringWeaponInfo()->GetIsThrownWeapon();
				
				if(GetHolsteringWeaponInfo() && (!GetHolsteringWeaponInfo()->GetIsUnarmed() || GetPed()->GetMotionData()->GetIsFPSLT()))
				{
					pWeaponInfo = GetHolsteringWeaponInfo();
					m_DrawClipSetId = pWeaponInfo->GetAppropriateSwapWeaponClipSetId(GetPed());
				}
			}
#endif	// FPS_MODE_SUPPORTED

			//This can happen on remotes in MP when transitioning weapons as the remote may not have the right weapon hash yet.  For instance if the remote has the cell phone equipped and is trying to transition
			//back to an assault rifle but the assault rifle information hasn't been received yet across the network this will be invalid until the assault rifle is equipped.  When that happens everything will proceed
			//properly.  lavalley
			if (NetworkInterface::IsGameInProgress() && (m_DrawClipSetId == CLIP_SET_ID_INVALID) && GetPed() && GetPed()->IsNetworkClone())
				return FSM_Continue;

			m_DrawClipId = GetSwapWeaponClipId(pWeaponInfo);

			// If clip doesn't exist, default standing unholster
			if(m_DrawClipId == CLIP_ID_INVALID || !fwAnimManager::GetClipIfExistsBySetId(m_DrawClipSetId, m_DrawClipId))
			{
				m_DrawClipSetId = pWeaponInfo->GetAppropriateWeaponClipSetId(GetPed());
				m_DrawClipId = ms_UnholsterClipId;
			}
		}
	}

	return FSM_Continue;
}

CTaskSwapWeapon::FSM_Return CTaskSwapWeapon::StateWaitToDraw_OnUpdate()
{
	if(!GetShouldDrawWeapon())
	{
		CTaskInCover* pInCoverTask = static_cast<CTaskInCover*>(GetPed()->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_IN_COVER));
		if (pInCoverTask)
		{
			pInCoverTask->RestartWeaponHolding();
		}
		// We should be drawing a weapon, so quit
		return FSM_Quit;
	}

	const CWeaponInfo* pWeaponInfo = GetDrawingWeaponInfo();
	if(!pWeaponInfo)
	{
		// Weapon has become invalid
		return FSM_Quit;
	}

	if (GetPed()->IsNetworkClone())
	{
		u32 drawingWeapon = GetCloneDrawingWeapon();
		u32 cachedWeapon = static_cast<CNetObjPed*>(GetPed()->GetNetworkObject())->GetCachedWeaponInfo().m_weapon;

		if (drawingWeapon != m_uInitialDrawingWeapon)
		{
			// the drawing weapon has changed, restart the task
			m_uInitialDrawingWeapon = m_uDrawingWeapon = drawingWeapon;

			// Restart task
			SetState(State_Init);
			return FSM_Continue;
		}

		// wait until we have received an update for the drawn weapon
		if (cachedWeapon != m_uInitialDrawingWeapon)
		{
			if (GetTimeInState() > 0.5f)
			{
				return FSM_Quit;
			}
			
			return FSM_Continue;
		}
	}
	
	if(GetIsAnimationStreamed(pWeaponInfo->GetHash(), m_DrawClipSetId))
	{
		if(StartMoveNetwork(pWeaponInfo))
		{
			// Init params
			m_Filter = GetDefaultFilterId(pWeaponInfo);
			// If we're waiting, always start from 0.0f
			m_fPhase = 0.f;

			// Ready to draw new weapon
			SetState(State_Draw);
		}
	}

	return FSM_Continue;
}

CTaskSwapWeapon::FSM_Return CTaskSwapWeapon::StateDraw_OnEnter()
{
	CPed* pPed = GetPed();
	//@@: location CTASKSWAPWEAPON_STATEDRAW_ONENTER_GETWEAPON_INFO
	const CWeaponInfo* pWeaponInfo = GetDrawingWeaponInfo();
	if(!pWeaponInfo)
	{
		return FSM_Quit;
	}

	if(pPed->IsAPlayerPed())
	{
		CWanted* pTargetWanted = pPed->GetPlayerWanted();
		if(pTargetWanted && pTargetWanted->ShouldTriggerCopSeesWeaponAudioForWeaponSwitch(pWeaponInfo))
		{
			pTargetWanted->TriggerCopSeesWeaponAudio(NULL, true);
		}
	}

	// B*2298102: Destroy melee fist weapons (ie knuckle dusters) as soon as we start a swap. The grip change causes the weapons to float around while running the swap animation.
	const CWeaponInfo *pHolsteringWeaponInfo = GetHolsteringWeaponInfo();
	if (pHolsteringWeaponInfo && pHolsteringWeaponInfo->GetIsMeleeFist() && !pWeaponInfo->GetIsMeleeFist())
	{
		GetPed()->GetWeaponManager()->DestroyEquippedWeaponObject(true OUTPUT_ONLY(,"CTaskSwapWeapon::StateDraw_OnEnter: MeleeFist"));
	}

	// If this is a throw weapon, don't show the model when out of ammo 
	if(pWeaponInfo->GetIsThrownWeapon() && !pPed->IsNetworkClone())
	{
		int nAmmo = pPed->GetInventory()->GetWeaponAmmo(pWeaponInfo);
		if(nAmmo == 0)
		{
			if(pWeaponInfo && !pWeaponInfo->GetDontSwapWeaponIfNoAmmo() && pWeaponInfo->GetSwapToUnarmedWhenOutOfThrownAmmo())
			{
				pPed->GetWeaponManager()->EquipWeapon(pPed->GetDefaultUnarmedWeaponHash());
			}
			else
			{
				// Out of ammo - swap weapon			
				pPed->GetWeaponManager()->EquipBestWeapon();
			}
			return FSM_Quit;
		}
	}

	taskAssert(GetIsAnimationStreamed(pWeaponInfo->GetHash(), m_DrawClipSetId));

	CTaskInCover* pInCoverTask = static_cast<CTaskInCover*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_IN_COVER));
	if (pInCoverTask)
	{
		pInCoverTask->RestartWeaponHolding();
	}

	//This can happen on remotes in MP when transitioning weapons as the remote may not have the right weapon hash yet.  For instance if the remote has the cell phone equipped and is trying to transition
	//back to an assault rifle but the assault rifle information hasn't been received yet across the network this will be invalid until the assault rifle is equipped.  When that happens everything will proceed
	//properly.  lavalley
	if (NetworkInterface::IsGameInProgress() && (m_DrawClipSetId == CLIP_SET_ID_INVALID) && GetPed() && GetPed()->IsNetworkClone())
		return FSM_Continue;

	const crClip* pClip = fwAnimManager::GetClipIfExistsBySetId(m_DrawClipSetId, m_DrawClipId);
	if(taskVerifyf(pClip, "pClip doesn't exist [%s][%s]", m_DrawClipSetId.TryGetCStr(), m_DrawClipId.TryGetCStr()))
	{
		if(!CClipEventTags::FindFirstEventTag<crTag, crPropertyAttributeHashString>(pClip, CClipEventTags::MoveEvent, CClipEventTags::MoveEvent, ms_ObjectDestroyId) &&
			!CClipEventTags::FindFirstEventTag<crTag, crPropertyAttributeHashString>(pClip, CClipEventTags::MoveEvent, CClipEventTags::MoveEvent, ms_ObjectReleaseId))
		{
			// Ensure equipped weapon has been removed
			GetPed()->GetWeaponManager()->DestroyEquippedWeaponObject(true OUTPUT_ONLY(,"CTaskSwapWeapon::StateDraw_OnEnter"));
		}

		m_MoveNetworkHelper.SetClip(pClip, ms_ClipId);
		m_MoveNetworkHelper.SetFloat(ms_PhaseId, m_fPhase);
		m_MoveNetworkHelper.SetFloat(ms_RateId, GetClipPlayBackRate(pWeaponInfo));
		SetFilter(m_Filter);

		// B*2301512: Knuckle Duster - don't enable grip until current weapon object is destroyed. Enabled in CTaskSwapWeapon::StateDraw_OnUpdate.
		if (pWeaponInfo && !pWeaponInfo->GetIsMeleeFist())
		{
			SetGripClip(pPed, pWeaponInfo);
		}

		RequestProcessMoveSignalCalls();
		m_bMoveBlendOut			= false;
		m_bMoveBlendOutLeftArm	= false;
		m_bMoveInterruptToAim	= false;
		m_bMoveObjectDestroy	= false;
		m_bMoveObjectRelease	= false;
		m_bMoveObjectCreate		= false;

		return FSM_Continue;
	}
	else
	{
		return FSM_Quit;
	}
}


CTaskSwapWeapon::FSM_Return CTaskSwapWeapon::StateDraw_OnUpdate()
{
	bool bMoveBlendOut = m_bMoveBlendOut;
	m_bMoveBlendOut = false;

	bool bMoveBlendOutLeftArm = m_bMoveBlendOutLeftArm;
	m_bMoveBlendOutLeftArm = false;

	bool bMoveInterruptToAim = m_bMoveInterruptToAim;
	m_bMoveInterruptToAim = false;

	bool bMoveObjectDestroy = m_bMoveObjectDestroy;
	m_bMoveObjectDestroy = false;

	bool bMoveObjectRelease = m_bMoveObjectRelease;
	m_bMoveObjectRelease = false;

	bool bMoveObjectCreate = m_bMoveObjectCreate;
	m_bMoveObjectCreate = false;

	CPed* pPed = GetPed();

	const CWeaponInfo* pWeaponInfo = GetDrawingWeaponInfo();
	if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_SwimmingTasksRunning))
	{
		if (!pWeaponInfo || !pWeaponInfo->GetUsableUnderwater())
			return FSM_Quit; // We started swimming since this task started, no weapons
	}
	
	TUNE_GROUP_BOOL(FIRST_PERSON_TUNE, ENABLE_EARLY_OUT_FOR_FPS, true);

	if(bMoveBlendOut)
	{
		// Ensure the weapon is created
		CreateWeapon();
#if FPS_MODE_SUPPORTED
		if (ENABLE_EARLY_OUT_FOR_FPS && pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
		{
			//We have just switched a new weapon which may be blocking certain FPS states. Force an update
			if(pPed->GetPlayerInfo())
			{
				pPed->GetPlayerInfo()->ProcessFPSState(true);
			}

			// Force previous FPS state to UNHOLSTER so that the unholster transition clipset is used for the aim intro transition
			pPed->GetMotionData()->SetPreviousFPSState(CPedMotionData::FPS_UNHOLSTER);
			return FSM_Quit;
		}
#endif // FPS_MODE_SUPPORTED
		return FSM_Quit;
	}

	if(bMoveBlendOutLeftArm && !m_iFlags.IsFlagSet(SWAP_TO_AIM))
	{
		// Only change filter if different
		if(m_Filter != ms_RightArmNoSpineFilterId)
		{
			if(StartMoveNetwork(pWeaponInfo))
			{
				m_Filter = ms_RightArmNoSpineFilterId;
				SetFlag(aiTaskFlags::RestartCurrentState);
				return FSM_Continue;
			}
		}
	}

	if(bMoveInterruptToAim)
	{
		const CControl* pControl = pPed->GetControlFromPlayer();
		if(m_iFlags.IsFlagSet(SWAP_TO_AIM) || (pControl && pControl->GetPedTargetIsDown()))
		{
			// Ensure the weapon is created
			CreateWeapon();
			return FSM_Quit;
		}
	}

	u32 drawingWeapon = m_uDrawingWeapon;

	// Check if we're actually drawing the correct weapon
	if (GetPed()->IsNetworkClone())
	{
		drawingWeapon = GetCloneDrawingWeapon();
	}
	else if (pWeaponInfo)
	{
		drawingWeapon = pWeaponInfo->GetHash();
	}

	if (drawingWeapon != m_uDrawingWeapon)
	{
		// We're drawing the wrong weapon
		if (GetPed()->IsNetworkClone())
		{
			m_uInitialDrawingWeapon = m_uDrawingWeapon = drawingWeapon;
		}
		else
		{
			m_uDrawingWeapon = 0;
		}

		// Restart task
		SetState(State_Init);
		return FSM_Continue;
	}

	if(bMoveObjectDestroy)
	{
		pPed->GetWeaponManager()->DestroyEquippedWeaponObject(true OUTPUT_ONLY(,"CTaskSwapWeapon::StateDraw_OnUpdate"));

		// B*2301512: Knuckle Duster - enable grip now that previous weapon object has been destroyed.
		if (pWeaponInfo && pWeaponInfo->GetIsMeleeFist())
		{
			SetGripClip(pPed, pWeaponInfo);
		}
	}
	else if(bMoveObjectRelease)
	{
		const CWeaponInfo* pWeaponInfo = GetHolsteringWeaponInfo();
		if(pWeaponInfo)
		{
			if(!pWeaponInfo->GetIsUnarmed())
			{
				pPed->GetWeaponManager()->DropWeapon(pWeaponInfo->GetHash(), true);
			}
		}
	}
	
	if(bMoveObjectCreate)
	{
		// Create the weapon at the specified clip event, and play a synced-up weapon clip if available
		if (CreateWeapon() && !pPed->IsFirstPersonShooterModeEnabledForPlayer())
		{
			fwMvClipId weaponClipId = GetSwapWeaponClipIdForWeapon(pWeaponInfo);
			if (weaponClipId != CLIP_ID_INVALID && fwAnimManager::GetClipIfExistsBySetId(m_DrawClipSetId, weaponClipId))
			{
				CWeapon* pNewEquippedWeapon = GetPed()->GetWeaponManager()->GetEquippedWeapon();
				if (pNewEquippedWeapon)
				{
					pNewEquippedWeapon->StartAnim(m_DrawClipSetId, weaponClipId, INSTANT_BLEND_DURATION, GetClipPlayBackRate(pWeaponInfo), m_fPhase, false, false, true);
				}
			}
		}

		if (pPed->GetNetworkObject() && !pPed->IsNetworkClone())
		{
			// force an immediate weapon update
			static_cast<CNetObjPlayer*>(pPed->GetNetworkObject())->RequestForceSendOfGameState();
		}

#if FPS_MODE_SUPPORTED
		if (ENABLE_EARLY_OUT_FOR_FPS && pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
		{
			//We have just switched a new weapon which may be blocking certain FPS states. Force an update
			if(pPed->GetPlayerInfo())
			{
				pPed->GetPlayerInfo()->ProcessFPSState(true);
			}

			// Force previous FPS state to UNHOLSTER so that the unholster transition clipset is used for the aim intro transition
			pPed->GetMotionData()->SetPreviousFPSState(CPedMotionData::FPS_UNHOLSTER);
			return FSM_Quit;
		}
#endif // FPS_MODE_SUPPORTED
	}

	return FSM_Continue;
}

bool CTaskSwapWeapon::StateDraw_OnProcessMoveSignals()
{
	m_bMoveBlendOut			|= m_MoveNetworkHelper.GetBoolean(ms_BlendOutId);
	m_bMoveBlendOutLeftArm	|= m_MoveNetworkHelper.GetBoolean(ms_BlendOutLeftArmId);
	m_bMoveInterruptToAim	|= m_MoveNetworkHelper.GetBoolean(ms_InterruptToAimId);
	m_bMoveObjectDestroy	|= m_MoveNetworkHelper.GetBoolean(ms_ObjectDestroyId);
	m_bMoveObjectRelease	|= m_MoveNetworkHelper.GetBoolean(ms_ObjectReleaseId);
	m_bMoveObjectCreate		|= m_MoveNetworkHelper.GetBoolean(ms_ObjectCreateId);

	return true;
}

bool CTaskSwapWeapon::CreateWeapon()
{
	CPed* pPed = GetPed();
	taskAssert(pPed->GetWeaponManager());

	if (pPed->IsNetworkClone())
	{
		CNetObjPed* pNetObjPed = (CNetObjPed*)pPed->GetNetworkObject();

		// allow the network object to create the weapon object now
		pNetObjPed->SetTaskOverridingPropsWeapons(false);

		pPed->GetWeaponManager()->SetCreateWeaponObjectWhenLoaded(true);

		pNetObjPed->UpdateCachedWeaponInfo(true);
	}
	else
	{
		// If this is a throw weapon, don't show the model when out of ammo 
		const CWeaponInfo* pWeaponInfo = pPed->GetWeaponManager()->GetEquippedWeaponInfo();
		if(pWeaponInfo && pWeaponInfo->GetIsThrownWeapon())
		{
			int nAmmo = pPed->GetInventory()->GetWeaponAmmo(pWeaponInfo);
			if(nAmmo == 0)
			{
				// Out of ammo - swap weapon			
				pPed->GetWeaponManager()->EquipBestWeapon();
				return false;
			}
		}

		if(!pPed->GetWeaponManager()->CreateEquippedWeaponObject())
		{
			return false;
		}
	}

	//If we've tapped the weapon wheel then restore the ammo in the weapon B*501673
	//Moved to PedEquippedWeapon as it makes more sense, also added check there for creating ordnance
// 	if(pPed->IsPlayer() && CNewHud::IsUsingWeaponWheel() && CNewHud::GetWheelDisableReload())
// 	{
// 		CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
// 		if(pWeapon)
// 		{
// 			pWeapon->SetAmmoInClip(CNewHud::GetWheelHolsterAmmoInClip());
// 			if(pWeapon->GetWeaponHash() != pPed->GetDefaultUnarmedWeaponHash())
// 			{
// 				CNewHud::SetWheelDisableReloadOff();
// 			}
// 		}
// 	}

	GenerateShockingEvent(*pPed, pPed->GetWeaponManager()->GetEquippedWeaponHash());

	return true;
}

bool CTaskSwapWeapon::StartMoveNetwork(const CWeaponInfo* pWeaponInfo)
{
	CPed* pPed = GetPed();
#if __ASSERT 
	if (pPed->GetUsingRagdoll())
		pPed->SpewRagdollTaskInfo();
#endif //__ASSERT
	taskAssertf(!pPed->GetUsingRagdoll(), "CTaskSwapWeapon - trying to start the move network when the ped is in ragdoll! flags=%d. See the log for more info.", m_iFlags.GetAllFlags());

	m_MoveNetworkHelper.RequestNetworkDef(CClipNetworkMoveInfo::ms_NetworkTaskSwapWeapon);
	if(m_MoveNetworkHelper.IsNetworkDefStreamedIn(CClipNetworkMoveInfo::ms_NetworkTaskSwapWeapon))
	{
		float fBlendInDuration = ms_Tunables.m_OnFootBlendInDuration;

		if (pPed->GetIsInCover())
		{
			fBlendInDuration = CTaskCover::GetShouldUseCustomLowCoverAnims(*pPed) ? ms_Tunables.m_LowCoverBlendInDuration : ms_Tunables.m_HighCoverBlendInDuration;
		}
		else if (pWeaponInfo && ShouldUseActionModeSwaps(*pPed, *pWeaponInfo))
		{
			fBlendInDuration = ms_Tunables.m_ActionBlendInDuration;
		}

		m_MoveNetworkHelper.CreateNetworkPlayer(pPed, CClipNetworkMoveInfo::ms_NetworkTaskSwapWeapon);

		//fix url:bugstar:1929714 -- assert - ped was in kStateStaticFrame when the calls to SetSecondaryTaskNetwork or SetTaskNetwork were called - ensure the ped is animated. (lavalley)
		if (NetworkInterface::IsGameInProgress())
		{
			if (pPed->GetMovePed().GetState() != CMovePed::kStateAnimated)
				pPed->GetMovePed().SwitchToAnimated(0.f);
		}

		if (GetFlags().IsFlagSet(SWAP_SECONDARY))
		{
			pPed->GetMovePed().SetSecondaryTaskNetwork(m_MoveNetworkHelper, fBlendInDuration);
		}
		else
		{
			pPed->GetMovePed().SetTaskNetwork(m_MoveNetworkHelper, fBlendInDuration);
		}
		return true;
	}

	return false;
}

void CTaskSwapWeapon::SetGripClip(CPed* pPed, const CWeaponInfo* pWeaponInfo)
{
	bool bUseGripClip = false;	 
	if (pWeaponInfo)
	{
		if (pWeaponInfo->GetIsMelee() || pWeaponInfo->GetIsThrownWeapon() || pWeaponInfo->GetIsGun1Handed())
		{
			fwMvClipSetId gripClipSet = pWeaponInfo->GetPedMotionClipSetId(*pPed); 
			if (gripClipSet != CLIP_SET_ID_INVALID)
			{		
				const crClip* pGripClip = NULL;				
				if (pWeaponInfo->GetIsMeleeFist() || (pWeaponInfo->GetIsSwitchblade()))
				{
					pGripClip = fwClipSetManager::GetClip(gripClipSet, fwMvClipId("GRIP_IDLE",0x3ec63b58));
				}
				else
				{
					pGripClip = fwClipSetManager::GetClip(gripClipSet, fwMvClipId("idle",0x71C21326));
				}

				if (pGripClip)
				{
					
					crFrameFilter* pGripFrameFilter = NULL;
					if (pWeaponInfo->GetIsMeleeFist())
					{
						static fwMvFilterId s_GripFilterHashId("BothArms_filter",0x16f1d420);	
						pGripFrameFilter = g_FrameFilterDictionaryStore.FindFrameFilter(s_GripFilterHashId);
					}
					else
					{
						static fwMvFilterId s_GripFilterHashId("Grip_R_Only_Filter",0xB455BA3A);	
						pGripFrameFilter = g_FrameFilterDictionaryStore.FindFrameFilter(s_GripFilterHashId);
					}

					m_MoveNetworkHelper.SetFilter(pGripFrameFilter, fwMvFilterId("GripFilter",0x57C40DEC));
					m_MoveNetworkHelper.SetClip(pGripClip, ms_GripClipId);
#if FPS_MODE_SUPPORTED
					bUseGripClip = !pPed->IsFirstPersonShooterModeEnabledForPlayer(false);
#else
					bUseGripClip = true;
#endif // FPS_MODE_SUPPORTED
				}
			}
		}
		else
		{
			if(pWeaponInfo->GetUsesAmmo())
			{
				if(pPed->GetInventory()->GetWeaponAmmo(pWeaponInfo->GetHash()) == 0)
				{
					fwMvClipSetId gripClipSet = pWeaponInfo->GetPedMotionClipSetId(*pPed); 
					if (gripClipSet != CLIP_SET_ID_INVALID)
					{		
						const crClip* pGripClip = fwClipSetManager::GetClip(gripClipSet, fwMvClipId("idle_empty",0x47510322));
						if (pGripClip)
						{
							m_MoveNetworkHelper.SetClip(pGripClip, ms_GripClipId);
							bUseGripClip = true;
						}
					}
				}
			}

		}
	}
	m_MoveNetworkHelper.SetFlag(bUseGripClip, ms_UseGripClipId);
}

float CTaskSwapWeapon::GetClipPlayBackRate(const CWeaponInfo* pWeaponInfo) const
{
	float fClipRate = ms_Tunables.m_OnFootClipRate;

	const CPed& rPed = *GetPed();

	float fClipRateModifier = 1.0f;
	if (CPlayerInfo::AreCNCResponsivenessChangesEnabled(&rPed))
	{
		TUNE_GROUP_FLOAT(CNC_RESPONSIVENESS, fSwapWeaponRateModifier, 1.5f, 1.0f, 5.0f, 0.01f);
		fClipRateModifier = fSwapWeaponRateModifier;
	}

	const CWeaponSwapData* pWeaponSwapData = pWeaponInfo->GetSwapWeaponData();
	if (taskVerifyf(pWeaponSwapData, "Couldn't find weapon swap data for %s", pWeaponInfo->GetName()))
	{
		if (pWeaponSwapData->GetAnimPlaybackRate() > 0.0f)
		{
			return (pWeaponSwapData->GetAnimPlaybackRate() * fClipRateModifier);
		}
	}
		
	if (rPed.GetIsInCover())
	{
		fClipRate = CTaskCover::GetShouldUseCustomLowCoverAnims(rPed) ? ms_Tunables.m_LowCoverClipRate : ms_Tunables.m_HighCoverClipRate;
	}
	else if(pWeaponInfo && ShouldUseActionModeSwaps(rPed, *pWeaponInfo))
	{
		fClipRate = ms_Tunables.m_ActionClipRate;
	}
	else if(rPed.GetIsSwimming())
	{
		fClipRate = ms_Tunables.m_SwimmingClipRate;
	}
	return (fClipRate * fClipRateModifier);
}

fwMvFilterId CTaskSwapWeapon::GetDefaultFilterId(const CWeaponInfo* pWeaponInfo) const
{
#if FPS_MODE_SUPPORTED
	if(GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false) && GetHolsteringWeaponInfo()->GetIsThrownWeapon())
	{
		return CMovePed::s_UpperBodyFilterHashId;
	}
#endif
	
	if(!m_iFlags.IsFlagSet(SWAP_TO_AIM))
	{
		if(pWeaponInfo)
		{
			const CPed* pPed = GetPed();
			fwMvFilterId filterId;
			if(CTaskCover::GetShouldUseCustomLowCoverAnims(*pPed))
			{
				filterId = pWeaponInfo->GetSwapWeaponInLowCoverFilterId(*pPed);
			}
			else if(ShouldUseActionModeSwaps(*pPed, *pWeaponInfo))
			{
				filterId = ms_BothArmsNoSpineFilterId;
			}
			else
			{
				filterId = pWeaponInfo->GetSwapWeaponFilterId(*pPed);
			}

			if(filterId != FILTER_ID_INVALID)
			{
				return filterId;
			}
		}
	}	
	return ms_BothArmsNoSpineFilterId;
}

void CTaskSwapWeapon::SetFilter(const fwMvFilterId& filterId)
{
	if(m_pFilter)
	{
		m_pFilter->Release();
		m_pFilter = NULL;
	}

	if(taskVerifyf(filterId != FILTER_ID_INVALID, "Filter [%s] is invalid", filterId.TryGetCStr()))
	{
		m_pFilter = CGtaAnimManager::FindFrameFilter(filterId.GetHash(), GetPed());
		if(taskVerifyf(m_pFilter, "Failed to get filter [%s]", filterId.TryGetCStr()))	
		{
			m_pFilter->AddRef();

			if(m_MoveNetworkHelper.IsNetworkActive())
			{
				m_MoveNetworkHelper.SetFilter(m_pFilter, ms_FilterId);
			}
		}
	}
}

bool CTaskSwapWeapon::GetShouldSwapInstantly(const CWeaponInfo* pWeaponInfo) const
{
	//Check if the flag is set.
	if(m_iFlags.IsFlagSet(SWAP_INSTANTLY))
	{
		return true;
	}

	//Check if the ped is a player and not in cover.
	const CPed* pPed = GetPed();
	if(pPed->IsPlayer() && !pPed->GetIsInCover())
	{
		//Check if the ped wants to use action mode.
		if(pWeaponInfo && ShouldUseActionModeSwaps(*pPed, *pWeaponInfo))
		{
			if(pPed->GetMovementModeData().m_UnholsterClipSetId == CLIP_SET_ID_INVALID || !pPed->GetMovementModeData().m_UnholsterClipDataPtr)
			{
				return true;
			}
		}
	}

	return false;
}

bool CTaskSwapWeapon::GetIsAnimationStreamed(const u32 uWeaponHash, const fwMvClipSetId& clipSetIdOverride) const
{
	fwMvClipSetId clipSetId = GetClipSet(uWeaponHash);
	if(clipSetIdOverride != CLIP_SET_ID_INVALID)
	{
		clipSetId = clipSetIdOverride;
	}

	//This can happen on remotes in MP when transitioning weapons as the remote may not have the right weapon hash yet.  For instance if the remote has the cell phone equipped and is trying to transition
	//back to an assault rifle but the assault rifle information hasn't been received yet across the network this will be invalid until the assault rifle is equipped.  When that happens everything will proceed
	//properly.  lavalley
	if (NetworkInterface::IsGameInProgress() && (clipSetId == CLIP_SET_ID_INVALID) && GetPed() && GetPed()->IsNetworkClone())
		return false;

	if(taskVerifyf(clipSetId != CLIP_SET_ID_INVALID, "clipSetId is invalid, weapon:%s model:%s", CWeaponInfoManager::GetInfo<CWeaponInfo>(uWeaponHash)->GetName(), CWeaponInfoManager::GetInfo<CWeaponInfo>(uWeaponHash)->GetModelName()))
	{
		fwClipSet* pClipSet = fwClipSetManager::GetClipSet(clipSetId);
		if(taskVerifyf(pClipSet, "pClipSet is NULL, clipSetId %s doesn't exist", clipSetId.TryGetCStr()))
		{
			return pClipSet->Request_DEPRECATED();
		}
	}

	return false;
}

fwMvClipSetId CTaskSwapWeapon::GetClipSet(u32 uWeaponHash) const
{
	const CPed* pPed = GetPed();
	return CWeaponInfoManager::GetInfo<CWeaponInfo>(uWeaponHash)->GetAppropriateSwapWeaponClipSetId(pPed);
}

const CWeaponInfo* CTaskSwapWeapon::GetHolsteringWeaponInfo() const
{
	const CPed* pPed = GetPed();
	taskAssert(pPed->GetWeaponManager());
	const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
	const CWeaponInfo* pWeaponInfo = pWeapon ? pWeapon->GetWeaponInfo() : NULL;
	if(!pWeaponInfo)
	{
		//If !pWeaponInfo then the ped doesn't have an object and the new weapon isn't unarmed. 
		//Assume the previous weapon was unarmed as it has no object.
		pWeaponInfo = CWeaponManager::GetWeaponUnarmed()->GetWeaponInfo();
	}
	taskAssert(pWeaponInfo);
	return pWeaponInfo;
}

const CWeaponInfo* CTaskSwapWeapon::GetDrawingWeaponInfo() const
{
	const CPed* pPed = GetPed();
	taskAssert(pPed->GetWeaponManager());
	return CWeaponInfoManager::GetInfo<CWeaponInfo>(pPed->GetWeaponManager()->GetEquippedWeaponHash());
}

fwMvClipId CTaskSwapWeapon::GetSwapWeaponClipId(const CWeaponInfo* pWeaponInfo) const
{
	if (!pWeaponInfo)
		return CLIP_ID_INVALID;

#if FPS_MODE_SUPPORTED
	//Should always be using the unholster clip id in FPS mode
	if(GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		return ms_UnholsterClipId;
	}
#endif

	const CWeaponSwapData* pWeaponSwapData = pWeaponInfo->GetSwapWeaponData();
	if (!taskVerifyf(pWeaponSwapData, "Couldn't find weapon swap data for %s", pWeaponInfo->GetName()))
		return CLIP_ID_INVALID;

	const CPed& rPed = *GetPed();

	s32 iFlags = 0;
	
	switch (GetState())
	{
		case State_Holster: iFlags |= CWeaponSwapData::SF_Holster;	break;
		case State_WaitToDraw: case State_Draw:	iFlags |= CWeaponSwapData::SF_UnHolster; break;
		default:
			taskAssertf(0, "%s is invalid state to play swap clip", GetStaticStateName(GetState())); break;
	}
	
	if(CTaskCover::GetShouldUseCustomLowCoverAnims(rPed))
	{
		iFlags |= CWeaponSwapData::SF_InLowCover;
		if(rPed.GetPedResetFlag(CPED_RESET_FLAG_InCoverFacingLeft))
		{
			iFlags |= CWeaponSwapData::SF_FacingLeftInCover;
		}
	}
	else if(rPed.GetIsCrouching())
	{
		iFlags |= CWeaponSwapData::SF_Crouch;
	}
	else if(ShouldUseActionModeSwaps(rPed, *pWeaponInfo))
	{
		if(NetworkInterface::IsGameInProgress() && rPed.IsNetworkClone() && pWeaponInfo && rPed.GetWeaponManager() && (pWeaponInfo->GetHash() == rPed.GetWeaponManager()->GetEquippedWeaponHash()))
		{
			//MP - on remotes - if the weapon is already equipped do nothing
		}
		else
		{
			fwMvClipId clipId = rPed.GetMovementModeData().GetUnholsterClip(pWeaponInfo->GetHash());
			if(clipId != CLIP_ID_INVALID)
			{
				return clipId;
			}
		}
	}

	if(pWeaponInfo->GetDiscardWhenSwapped())
	{
		iFlags |= CWeaponSwapData::SF_Discard;
	}

	fwMvClipId clipId = CLIP_ID_INVALID;
	pWeaponSwapData->GetSwapClipId(iFlags, clipId);
	return clipId;
}

fwMvClipId CTaskSwapWeapon::GetSwapWeaponClipIdForWeapon(const CWeaponInfo* pWeaponInfo) const
{
	if (!pWeaponInfo)
		return CLIP_ID_INVALID;

	const CWeaponSwapData* pWeaponSwapData = pWeaponInfo->GetSwapWeaponData();
	if (!taskVerifyf(pWeaponSwapData, "Couldn't find weapon swap data for %s", pWeaponInfo->GetName()))
		return CLIP_ID_INVALID;

	s32 iFlags = 0;

	switch (GetState())
	{
	case State_WaitToHolster: case State_Holster: iFlags |= CWeaponSwapData::SF_Holster;	break;
	case State_WaitToDraw: case State_Draw:	iFlags |= CWeaponSwapData::SF_UnHolster; break;
	default:
		taskAssertf(0, "%s is invalid state to play swap weapon clip", GetStaticStateName(GetState())); break;
	}

	fwMvClipId clipId = CLIP_ID_INVALID;
	pWeaponSwapData->GetSwapClipIdForWeapon(iFlags, clipId);
	return clipId;
}


bool CTaskSwapWeapon::ShouldUseActionModeSwaps(const CPed& ped, const CWeaponInfo& weaponInfo) const
{
	if((ped.IsUsingActionMode() || ped.GetMotionData()->GetUsingStealth()) && 
		!ped.GetIsInCover() && ped.GetMovementModeData().m_UnholsterClipDataPtr && ped.HasValidMovementModeForWeapon(weaponInfo.GetHash()))
	{
		const CTaskMotionBase* pMotionTask = ped.GetCurrentMotionTask(false);
		return !pMotionTask || !pMotionTask->IsInMotionState(CPedMotionStates::MotionState_Sprint);
	}
	return false;
}

u32 CTaskSwapWeapon::GetCloneDrawingWeapon() const
{
	const CPed& rPed = *GetPed();

	// another swap weapon update may have come in, drawing a different weapon
	CClonedSwapWeaponInfo* pSwapWeaponInfo = static_cast<CClonedSwapWeaponInfo*>(rPed.GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(CTaskTypes::TASK_SWAP_WEAPON, PED_TASK_PRIORITY_MAX, false));

	if (pSwapWeaponInfo)
	{
		return pSwapWeaponInfo->GetDrawingWeapon();
	}

	return m_uDrawingWeapon;
}

//-------------------------------------------------------------------------
// Task info for CTaskSwapWeapon
//-------------------------------------------------------------------------
CTaskFSMClone *CClonedSwapWeaponInfo::CreateCloneFSMTask()
{
	return rage_new CTaskSwapWeapon(static_cast<s32>(m_iFlags), m_drawingWeapon);
}

CTask *CClonedSwapWeaponInfo::CreateLocalTask(fwEntity* UNUSED_PARAM(pEntity))
{
	return rage_new CTaskSwapWeapon(static_cast<s32>(m_iFlags), m_drawingWeapon);
}