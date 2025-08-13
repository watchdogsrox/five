#include "CWeaponWheel.h"

#include "basetypes.h"
#include "camera/CamInterface.h"
#include "camera/cinematic/CinematicDirector.h"
#include "debug/DebugScene.h"
#include "frontend/MobilePhone.h"
#include "frontend/NewHud.h"
#include "frontend/ui_channel.h"
#include "fwscene/stores/drawablestore.h"
#include "scene/world/gameworld.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PedHelmetComponent.h"
#include "Weapons/Components/WeaponComponentManager.h"
#include "Weapons/inventory/WeaponItem.h"
#include "Weapons/info/WeaponInfo.h"
#include "Weapons/inventory/PedWeaponSelector.h"
#include "system/controlMgr.h"
#include "system/control.h"
#include "system/control_mapping.h"
#include "scaleform/scaleform.h"
#include "Text/GxtString.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "Task/Vehicle/TaskVehicleWeapon.h"
#include "Vehicles/Metadata/VehicleMetadataManager.h"
#include "Vehicles/Metadata/VehicleSeatInfo.h"
#include "Vehicles/AmphibiousAutomobile.h"

#include "math/amath.h"

#define	__SPECIAL_FLAG_DONT_DISPLAY_AMMO	(-1)
#define __SPECIAL_FLAG_USING_INFINITE_AMMO	(-2)
#define TOUCHPAD_CHANGE_WEAPON_WHEEL_SLOT	(RSG_ORBIS && 1)
#define INVALID_WEAPON_INDEX				(MAX_UINT8)

#define AQUABIKE_WHEELS_UP			( 0x56C20025 )
#define AQUABIKE_WHEELS_DOWN		( 0x4952EA42 )

#define AQUABIKE_WHEELS_UP_NAME		"AQUABIKE_WHEEL_UP"
#define AQUABIKE_WHEELS_DOWN_NAME	"AQUABIKE_WHEEL_DOWN"

BANK_ONLY(PARAM(enable3DWeaponWheel, "turns on 3D Wheel functionality"));


FRONTEND_OPTIMISATIONS()

/////////////////////////////////////////////////////////////////////////////////////
CWeaponWheel::CWeaponWheel() : CSelectionWheel(250,100)
	, m_bIsActive(true)
	, m_bVisible(false)
	, m_bCarMode(false)
	, m_bScriptForcedOn(false)
	, m_bAllowedToChangeWeapons(true)
	, m_bAllowWeaponDPadSelection(true)
	, m_bSuppressWheelUntilButtonReleased(false)
	, m_bSuppressWheelResultDueToQuickTap(false)
	, m_bSuppressWheelResults(false)
	, m_bRebuild(false)
	, m_bReceivedFadeComplete(false)
	, m_bQueueingForceOff(false)
	, m_bQueueParachute(false)
	, m_iHashReturnedId(0)
	, m_iFramesToWaitForAValidReturnedId(0)
	, m_uLastHighlightedWeaponWheelHash(0u)
	, m_uLastEquippedWeaponWheelHash(0u)
	, m_iWeaponCount(0)
	, m_iAmmoCount(0)
	, m_bDisableReload(false)
	, m_uWeaponInHandHash(0u)
	, m_HolsterAmmoInClip(0)
	, PC_FORCE_WHEEL_TIME(1000)
	, m_bForcedOn(false)
	, m_bForceResult(false)
	, CROSSFADE_ALPHA(50)
	, CROSSFADE_TIME(0.15f)
	, m_iFailsafeTimer(0)
	, m_iNumParachutes(0)
	, m_eLastSlotPointedAt(MAX_WHEEL_SLOTS)
	, m_bSwitchWeaponInSniperScope(false)
	, m_bToggleSpecialVehicleAbility(false)
	, m_bSetSpecialVehicleCameraOverride(false)

#if USE_DIRECT_SELECTION
	, m_eDirectSelectionSlot(MAX_WHEEL_SLOTS)
#endif // USE_DIRECT_SELECTION
#if __DEV
	, m_pszDisabledReason(NULL)
#endif
#if __BANK
	, m_bDbgInvertButton(false)
	, m_perspFOV(55.0f)
	, m_x(282.95f)
	, m_y(190.15f)
	, m_z(0.0f)
	, m_xRotation(0.0f)
	, m_yRotation(0.0f)
	, m_bgZ(500.0f)
	, m_maxRotation(8.0f)
	, m_tweenDuration(.2f)
	, m_bBGVisibility(true)
	, m_bFiniteRotation(true)
#if RSG_PC
	, m_StereoDepthScalar(1.0f)
#endif
#endif
{
}

void CWeaponWheel::ClearAndHide()
{
	ClearBase();
	Hide(false);
}

void CWeaponWheel::Clear(bool bFullyClear)
{
	for (u8 iSlotNum = 0; iSlotNum < MAX_WHEEL_SLOTS; iSlotNum++)
	{
		for (u8 iWeaponSlot = 0; iWeaponSlot < MAX_WHEEL_WEAPONS_IN_SLOT; iWeaponSlot++)
			m_Slot[iSlotNum][iWeaponSlot].Clear();

		m_iIndexOfWeaponToShowInitiallyForThisSlot[iSlotNum] = INVALID_WEAPON_INDEX;
	}
	m_bHasASlotWithMultipleWeapons = false;

	// if not doing a full clear, just bail now
	if( !bFullyClear )
		return;
	ClearBase();
	m_bAllowWeaponDPadSelection = true;
	m_WheelMemory.Clear();
	m_iForceWheelShownTime.Zero();

	ReleaseHdWeapons();
}


bool CWeaponWheel::IsWeaponSelectable(atHashWithStringNotFinal WeaponHash) const
{
	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
	if (pPlayerPed && pPlayerPed->GetInventory() && pPlayerPed->GetWeaponManager())
	{
		const CWeaponItem* pItem = pPlayerPed->GetInventory()->GetWeapon(WeaponHash);
		if (pItem)
		{
			const CWeaponInfo* pWeaponInfo = pItem->GetInfo();
			if (pWeaponInfo)
			{
				const CPedWeaponSelector* pSelector = pPlayerPed->GetWeaponManager()->GetWeaponSelector();
				if (pSelector && pSelector->GetIsWeaponValid(pPlayerPed, pWeaponInfo, false, !pWeaponInfo->GetIsThrownWeapon()))
				{
					return true;
				}
			}
		}
	}

	return false;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::MapWeaponSlotToWeaponWheel()
// PURPOSE: maps the weapon code slot to a slot on the weapon wheel
/////////////////////////////////////////////////////////////////////////////////////
eWeaponWheelSlot CWeaponWheel::MapWeaponSlotToWeaponWheel(s32 UNUSED_PARAM(iWeaponCodeSlot), atHashWithStringNotFinal uCurrentWeaponHash, const sWeaponWheelSlot& newSlot)
{
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(newSlot.m_uWeaponHash);
	const CWeaponInfo* pCurrentWeaponSlot = CWeaponInfoManager::GetInfo<CWeaponInfo>(uCurrentWeaponHash);
	if( !Verifyf(pWeaponInfo, "Couldn't find a weaponinfo for 0x%08x (%s)", newSlot.m_uWeaponHash.GetHash(), newSlot.m_uWeaponHash.TryGetCStr())// ||
		//!Verifyf(pCurrentWeaponSlot, "Couldn't find a weaponinfo for currentWeapon 0x%08x (%i)", uCurrentWeaponHash, uCurrentWeaponHash))
		)
		return MAX_WHEEL_SLOTS;

	eWeaponWheelSlot iWheelSlot = pWeaponInfo->GetWeaponWheelSlot();
	eWeaponWheelSlot iCurrentWeaponWheelSlot = pCurrentWeaponSlot ? pCurrentWeaponSlot->GetWeaponWheelSlot() : 	MAX_WHEEL_SLOTS;
	
	for (u8 i = 0; i < MAX_WHEEL_WEAPONS_IN_SLOT; i++)
	{
		if( m_Slot[iWheelSlot][i].IsEmpty() )
		{
			// set this weapon in the slot
			m_Slot[iWheelSlot][i] = newSlot;

			// only show initially if not previously set
			if( m_iIndexOfWeaponToShowInitiallyForThisSlot[iWheelSlot] == MAX_UINT8) 
			{
				// was last set in wheel memory  AND does not share the same slot as our current equipped weapon, OR it is our currently equipped weapon
				if( uCurrentWeaponHash == newSlot.m_uWeaponHash )
				{
					m_iIndexOfWeaponToShowInitiallyForThisSlot[iWheelSlot] = i;
				}

				else if( newSlot.m_uWeaponHash == m_WheelMemory.GetWeapon( u8(iWheelSlot)) && iWheelSlot!=iCurrentWeaponWheelSlot )
				{
					m_iIndexOfWeaponToShowInitiallyForThisSlot[iWheelSlot] = i;
				}
			}

			m_bHasASlotWithMultipleWeapons = m_bHasASlotWithMultipleWeapons || (i > 0);

			// if it's the currently selected one, return which slot to make active
			if (uCurrentWeaponHash == newSlot.m_uWeaponHash)
				return iWheelSlot;

			return MAX_WHEEL_SLOTS;
		}
	}

	Assertf(0, "NewHud: %i is not enough entries for wheelslot type %i!", MAX_WHEEL_WEAPONS_IN_SLOT, iWheelSlot);

	return MAX_WHEEL_SLOTS;
}

void CWeaponWheel::sWeaponWheelSlot::SetScaleform(bool bSelected, int iWeaponSlot, int iIndex) const
{
	// slot's got nothing in it, just bail.
	if( IsEmpty() )
		return;

#if GTA_REPLAY
	// there was a bug saying vehicle weapon wheel was appearing in replay mode.
	// couldn't recreate as shouldn't be possible so assert and early out
	bool inReplayMode = CReplayMgr::IsEditModeActive();
	uiAssertf(!inReplayMode, "CWeaponWheel::sWeaponWheelSlot::SetScaleform - This should be getting called in replay mode");
	if(inReplayMode)
		return;
#endif // GTA_REPLAY

	if (CHudTools::BeginHudScaleformMethod(NEW_HUD_WEAPON_WHEEL, "SET_PLAYER_WEAPON_WHEEL"))
	{	
		CScaleformMgr::AddParamInt(iWeaponSlot);  // weapon slot
		CScaleformMgr::AddParamInt(iIndex);		 // array index of weapon type

		CScaleformMgr::AddParamInt(m_uWeaponHash);  // weapon hash
		CScaleformMgr::AddParamInt(bSelected?1:0); // weapon selected or not

		// ammo:
		CScaleformMgr::AddParamInt(m_iAmmo);
		CScaleformMgr::AddParamInt(m_iClipAmmo);
		CScaleformMgr::AddParamInt(m_iClipSize);

		//	Pass an extra parameter to say whether the weapon should be selectable or greyed out
		CScaleformMgr::AddParamInt(m_bIsSelectable?1:0);

		// Send Weapon name
		atString weaponName(TheText.Get(m_HumanNameHash, "WeaponName"));

		if(m_HumanNameHashSuffix != 0)
		{
			weaponName += " ";
			weaponName += TheText.Get(m_HumanNameHashSuffix, "WeaponNameSuffix");
		}

		CScaleformMgr::AddParamString(weaponName);

		CScaleformMgr::AddParamBool(m_bIsEquipped || iWeaponSlot == 8);

		CScaleformMgr::AddParamInt(m_iAmmoSpecialType);

		CScaleformMgr::EndMethod();
	}
}

// fix for unity
#undef IF_SDR
#undef    SDR

#if __DEV
#define    SDR(x) SetDisabledReason( "WEAPON: " x )
#define IF_SDR(x) SetDisabledReason( "WEAPON: " x )
bool CWeaponWheel::SetDisabledReason(const char* pszWhy )
{
	m_pszDisabledReason = pszWhy;
	return true;
}
#else
// so it'll deadstrip away
#define IF_SDR(x) 1
#define    SDR(x) do { } while (0)
#endif


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CNewHud::UpdateWeaponWheel()
// PURPOSE: updates the weapon wheel
// NOTE:    
/////////////////////////////////////////////////////////////////////////////////////
void CWeaponWheel::Update(bool forceShow, bool isHudHidden)
{
	if (camInterface::IsRenderingFirstPersonShooterCamera())
	{
		RequestHdWeapons();
	}
	else
	{
		ReleaseHdWeapons();
	}

	if( !IsActive() )
	{
		ClearAndHide();
		SDR("Inactive");
		return;
	}

#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive())
	{
		ClearAndHide();
		SDR("Replay Edit Mode Active");
		return;
	}
#endif // GTA_REPLAY

	CPed *pPlayerPed = CGameWorld::FindLocalPlayer();

	if (!pPlayerPed || g_PlayerSwitch.IsActive())
	{
		if( !pPlayerPed )
			SDR("No Player");
		else
			SDR("Player Switch In Progress");
		ClearAndHide();
		m_uWeaponInHandHash.Clear();
		m_bDisableReload = false; // SHOULD we set this to false..?
		return;
	}

	CControl* pControl = pPlayerPed->GetControlFromPlayer();

	if (!pControl)
	{
		ClearAndHide();
		SDR("No Player control");
		return;
	}

	if (!pPlayerPed->GetWeaponManager())
	{
		ClearAndHide();
		SDR("No weapon manager");
		return;
	}

	const ioValue* relevantControl = &pControl->GetSelectWeapon();
	bool bVehicleHasWeapons = false;
	bool bIsPlayerInVehicle = pPlayerPed->GetIsInVehicle();
	bool bIsPlayerParachutingOrUsingJetpack = pPlayerPed->GetIsParachuting() || pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_IsUsingJetpack);
	if( bIsPlayerInVehicle )
	{
		if( pPlayerPed->GetMyVehicle()->GetIsAircraft() )
			relevantControl = &pControl->GetVehicleFlySelectNextWeapon();
		else
			relevantControl = &pControl->GetVehicleSelectNextWeapon();

		bVehicleHasWeapons = pPlayerPed->GetVehiclePedInside()->GetSeatHasWeaponsOrTurrets(pPlayerPed->GetAttachCarSeatIndex());
	}
	else if( bIsPlayerParachutingOrUsingJetpack )
	{
		relevantControl = &pControl->GetVehicleSelectNextWeapon();
	}

	bool bUseVehicleControls = (bIsPlayerInVehicle || bIsPlayerParachutingOrUsingJetpack);

	if ((isHudHidden && !forceShow													&& IF_SDR("Hud Is Hidden"))
		|| ( !pPlayerPed->GetInventory()											&& IF_SDR("No Inventory"))
		|| ( pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_TemporarilyBlockWeaponSwitching) && IF_SDR("ResetFlag TemporarilyBlockWeaponSwitching") ) 
		|| ( pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_BlockWeaponSwitching)	&& IF_SDR("ConfigFlag BlockWeaponSwitching")  )
		|| ( pControl->IsInputDisabled(INPUT_SELECT_WEAPON)							&& IF_SDR("INPUT_SELECT_WEAPON Disabled") ) 
		|| ( CPhoneMgr::IsDisplayed()												&& IF_SDR("Phone Active") )
		|| ( CNewHud::IsRadioWheelActive()											&& IF_SDR("Radio Wheel Active") )
		|| ( bUseVehicleControls													&& IF_SDR("Show Single Circle") )
		)
	{
		m_bAllowedToChangeWeapons = bUseVehicleControls && !CNewHud::IsRadioWheelActive();
		m_bSuppressWheelUntilButtonReleased = true;
		m_iTimeToShowTimer.Start();
	}
	else if (m_bSuppressWheelUntilButtonReleased)
	{
		SDR("Waiting for button release");
		bool bIsUp = BANK_ONLY( m_bDbgInvertButton ? relevantControl->IsDown() : ) relevantControl->IsUp();
		bool bWasDown = BANK_ONLY( m_bDbgInvertButton ? relevantControl->WasUp() : ) relevantControl->WasDown();
		if( (bIsUp && !bWasDown)
			|| (m_iTimeToShowTimer.IsStarted() && m_iTimeToShowTimer.IsComplete(BUTTON_HOLD_TIME)))
		{
			m_bAllowedToChangeWeapons = true;
			m_bSuppressWheelUntilButtonReleased = false;
			TUNE_GROUP_BOOL(WEAPON_WHEEL_DEBUG, ZERO_TIMER, false);
			ClearBase(false, ZERO_TIMER);
		}
	}

	bool bShowWeaponWheel = m_bScriptForcedOn;
	bool bRefreshInventory = false;

	// emergency failsafe engage!
	if( !m_bReceivedFadeComplete && --m_iFailsafeTimer <= 0)
	{
		FadeComplete();
	}

	if( m_bReceivedFadeComplete && m_bRebuild)
	{
		m_bRebuild = false;
		bRefreshInventory = true;
		if( bShowWeaponWheel )
			START_CROSSFADE(CROSSFADE_TIME, CROSSFADE_ALPHA);
	}

	int WeaponCount = pPlayerPed->GetInventory()->GetWeaponRepository().GetNumGuns(true);
#if TOUCHPAD_CHANGE_WEAPON_WHEEL_SLOT
	// Ensure weapon list is up-to-date for touchpad controls.
	if (m_iWeaponCount != WeaponCount)
	{
		SetContentsFromPed(pPlayerPed);
	}
#endif // TOUCHPAD_CHANGE_WEAPON_WHEEL_SLOT

	Vector2 stickPos( GenerateStickInput() );

	if (!bShowWeaponWheel && m_bAllowedToChangeWeapons)
	{
		ButtonState buttState = eBS_NotPressed;
		if( !bVehicleHasWeapons && !bUseVehicleControls )
			buttState = HandleButton( *relevantControl, stickPos 
#if __BANK
			, m_bDbgInvertButton 
#endif
			);

		switch(buttState)

		{
		case eBS_FirstDown:
			// if we have never set a last selected weapon, then use the current weapon as it
			if (m_WheelMemory.GetSelected().IsNull() && pPlayerPed->GetWeaponManager()->GetEquippedWeaponHash() != pPlayerPed->GetDefaultUnarmedWeaponHash())
			{
				m_WheelMemory.SetSelected( pPlayerPed->GetWeaponManager()->GetEquippedWeaponHash() );
			}
			m_bSuppressWheelResultDueToQuickTap = false;
			bShowWeaponWheel = IsForcedOpenByTimer();
			SDR("Just hit button");
			break;

		case eBS_Tapped:
			if( !m_bSuppressWheelResults )
			{
				SDR("Was tapped");

				// if the wheel was forced open ( mouse wheel selection )
				// force it closed and finalize the selection
#if USE_DIRECT_SELECTION
				if( m_bForcedOn )
				{
					Hide(true);
					bShowWeaponWheel = false;
					m_iForceWheelShownTime.Zero();
				}
				else
#endif
					if (pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_KeepWeaponHolsteredUnlessFired) && pPlayerPed->GetWeaponManager()->GetEquippedWeaponHash() != pPlayerPed->GetDefaultUnarmedWeaponHash())
				{
					// Make the weapon visible again
					pPlayerPed->SetPedConfigFlag(CPED_CONFIG_FLAG_KeepWeaponHolsteredUnlessFired, false);
				}
				else
				{
					if (m_WheelMemory.GetSelected().IsNotNull())
					{
						// change weapon
						const CWeapon* pWeapon = pPlayerPed->GetWeaponManager()->GetEquippedWeapon();
						const CWeaponInfo* pWeaponInfo = pWeapon ? pWeapon->GetWeaponInfo() : NULL;
						atHashWithStringNotFinal iCurrentHashInUse = pWeaponInfo ? pWeaponInfo->GetHash() : 0;

						atHashWithStringNotFinal iNewHash;

						if (NetworkInterface::IsInCopsAndCrooks())
						{
							const CPlayerInfo* pPlayerInfo = pPlayerPed->GetPlayerInfo();
							const eArcadeTeam arcadeTeam = pPlayerInfo->GetArcadeInformation().GetTeam();
							if (arcadeTeam == eArcadeTeam::AT_CNC_COP)
							{
								const u32 uBestPrimaryWeaponHash = pPlayerPed->GetWeaponManager()->GetWeaponSelector()->GetBestPedWeapon(pPlayerPed, CWeaponInfoManager::GetSlotListBest(), false);
								const u32 uBestNonLethalHash = pPlayerPed->GetWeaponManager()->GetWeaponSelector()->GetBestPedNonLethalWeapon(pPlayerPed, CWeaponInfoManager::GetSlotListBest());

								if (iCurrentHashInUse == uBestNonLethalHash)
								{								
									iNewHash = uBestPrimaryWeaponHash;
								}
								else
								{
									iNewHash = uBestNonLethalHash;
								}
							}
							else if (arcadeTeam == eArcadeTeam::AT_CNC_CROOK)
							{
								const u32 uBestPrimaryWeaponHash = pPlayerPed->GetWeaponManager()->GetWeaponSelector()->GetBestPedWeapon(pPlayerPed, CWeaponInfoManager::GetSlotListBest(), false);
								const u32 uBestSecondaryWeaponHash = pPlayerPed->GetWeaponManager()->GetWeaponSelector()->GetBestPedWeaponByGroup(pPlayerPed, CWeaponInfoManager::GetSlotListBest(), WEAPONGROUP_PISTOL);

								if (iCurrentHashInUse == uBestPrimaryWeaponHash)
								{								
									iNewHash = uBestSecondaryWeaponHash;
								}
								else
								{
									iNewHash = uBestPrimaryWeaponHash;
								}
							}
						}
						else
						{
							if (iCurrentHashInUse == pPlayerPed->GetDefaultUnarmedWeaponHash())
							{
								if (IsWeaponSelectable(m_WheelMemory.GetSelected()))
								{
									iNewHash = m_WheelMemory.GetSelected();
								}
								else
								{
									atHashWithStringNotFinal iBestWeaponHash = pPlayerPed->GetWeaponManager()->GetBestWeaponHash();
									if(iBestWeaponHash.IsNotNull() && iBestWeaponHash != pPlayerPed->GetDefaultUnarmedWeaponHash() && IsWeaponSelectable(iBestWeaponHash))
									{
										iNewHash = iBestWeaponHash;
									}
								}
							}
							else if (iCurrentHashInUse != 0)
							{
								m_WheelMemory.SetSelected( iCurrentHashInUse );
								iNewHash = pPlayerPed->GetDefaultUnarmedWeaponHash();
							}
						}

						if (iNewHash != 0)
						{
							CPedWeaponManager *pWeapMgr = pPlayerPed->GetWeaponManager();
							pWeapMgr->GetWeaponSelector()->SelectWeapon(pPlayerPed, SET_SPECIFIC_ITEM, true, iNewHash);
							m_bSuppressWheelResultDueToQuickTap = true;
						}
					}
				}
			}
			else
			{
				SDR("Switching disabled by script");
			}
			break;
		case eBS_HeldEnough:
			//@@: location AHOOK_0067_CHK_LOCATION_B
			SDR("Held enough");
			bShowWeaponWheel = true;
			m_bForcedOn = false;
			m_iForceWheelShownTime.Zero();
			break;

		case eBS_NotHeldEnough:
			SDR("Not Held Enough");
			bShowWeaponWheel = IsForcedOpenByTimer();
			break;

		case eBS_NotPressed:
		default:
			if( HandleControls(pPlayerPed, bRefreshInventory) )
			{

#if RSG_PC // only allow click select on PC
				// if the wheel is up, and they've clicked shoot, instantly switch to that weapon
				if( m_bVisible.GetState() && !bIsPlayerInVehicle && pControl->GetPedAttack().IsPressed())
				{
					bShowWeaponWheel = false;
					m_bForcedOn = false;
					m_iForceWheelShownTime.Zero();
					SDR("Closed because they clicked shoot");
				}
				else
#endif
				{
					m_bForcedOn = true;
					bShowWeaponWheel = true;
					SDR("Forced open from time");
				}
			}
			else
			{
				bShowWeaponWheel = false;
				ClearBase(false);
				SDR("Not pressed");
			}
			break;

		}	
	}

	if( bShowWeaponWheel && m_iHashReturnedId == 0 )
	{
		int AmmoCount = 0;
		// this is a BIT stupid, but there's no other way besides adding a listener to every inventory.
		// need be, we could space this out across multiple frames or something or whatever
		const atArray<CAmmoItem*>& ammoArray = pPlayerPed->GetInventory()->GetAmmoRepository().GetAllItems();
		for(int i=0;i<ammoArray.GetCount();++i)
			AmmoCount += ammoArray[i]->GetAmmo();


		bool bInitialisingWheel = false;
		// if the number of weapons in your pants changes, we'll need to update.
		bool bShowWheelAsInCar = bUseVehicleControls && !m_bScriptForcedOn;
		if( m_bVisible.SetState(true) || m_bCarMode.SetState(bShowWheelAsInCar)) 
		{
#if __BANK
			if(PARAM_enable3DWeaponWheel.Get())
			{
				UpdateScaleform3DSettings();
			}
#endif

#if KEYBOARD_MOUSE_SUPPORT && RSG_EXTRA_MOUSE_SUPPORT
			ioMouse::ClearAverageMovementBuffer();
#endif

			if (CScaleformMgr::BeginMethod(CNewHud::GetContainerMovieId(), SF_BASE_CLASS_HUD, "SET_WEAPON_WHEEL_ACTIVE"))
			{
				CScaleformMgr::AddParamInt(1);
				CScaleformMgr::EndMethod();
			}

			
			if( CHudTools::BeginHudScaleformMethod(NEW_HUD_WEAPON_WHEEL, "SET_WHEEL_IN_CAR_MODE") )
			{
				m_bCarMode.SetState(bShowWheelAsInCar);
				CScaleformMgr::AddParamBool(bShowWheelAsInCar);
				CScaleformMgr::EndMethod();
			}

			// Update the weapon wheel stats
			const CPedWeaponManager* pPlayerWeaponManager = pPlayerPed->GetWeaponManager();
			if(pPlayerWeaponManager)
			{
				const atHashWithStringNotFinal uEquippedWeaponHash = pPlayerWeaponManager->GetWeaponSelector()->GetSelectedWeapon();
				FindAndUpdateWeaponWheelStats(bShowWheelAsInCar, uEquippedWeaponHash, pPlayerPed);
				if(!bShowWheelAsInCar)
					UpdateWeaponAttachment(uEquippedWeaponHash);
			}

			bRefreshInventory = true;
			bInitialisingWheel = true;
			m_bAllowWeaponDPadSelection = true;
		}

		if(bRefreshInventory || m_iWeaponCount != WeaponCount || m_iAmmoCount != AmmoCount)
		{
			bool bWeaponCountChanged	= m_iWeaponCount.SetState(WeaponCount);
			bool bAmmoCountChanged		= m_iAmmoCount.SetState(AmmoCount);
			bool bWasBecauseOfInventoryChange = bWeaponCountChanged || bAmmoCountChanged; // Prevents short-circuiting of the expression -- m_iWeaponCount.SetState(WeaponCount) || m_iAmmoCount.SetState(AmmoCount)

			if( bRefreshInventory )
				bWasBecauseOfInventoryChange = false;

			// Only clear the wheel contents when necessary
			if( !bInitialisingWheel && (bWeaponCountChanged || !bAmmoCountChanged))
				CHudTools::CallHudScaleformMethod(NEW_HUD_WEAPON_WHEEL, "CLEAR_PLAYER_WEAPON_WHEEL");

			eWeaponWheelSlot iInitialSlot = MAX_WHEEL_SLOTS;
			if( !m_bScriptForcedOn )
				iInitialSlot = SetContentsFromPed(pPlayerPed);
			if( bWasBecauseOfInventoryChange )
				iInitialSlot = MAX_WHEEL_SLOTS;

			FillScaleform(iInitialSlot, bUseVehicleControls);

			// check if the weapon wheel's contents are different from the last selected hash, therefore, we need to update it
			if( m_eLastSlotPointedAt != MAX_WHEEL_SLOTS && !bIsPlayerInVehicle )
			{
				atRangeArray<sWeaponWheelSlot, MAX_WHEEL_WEAPONS_IN_SLOT>& curWeaponTypeSlot = m_Slot[m_eLastSlotPointedAt];

				u8 curWeaponHighlight = m_iIndexOfWeaponToShowInitiallyForThisSlot[m_eLastSlotPointedAt];
				if( curWeaponHighlight != INVALID_WEAPON_INDEX )
				{
					sWeaponWheelSlot& wheelSlot = curWeaponTypeSlot[ curWeaponHighlight ];
					if(wheelSlot.m_uWeaponHash!=m_uLastHighlightedWeaponWheelHash)
					{
						FindAndUpdateWeaponWheelStats(bIsPlayerInVehicle, wheelSlot.m_uWeaponHash, pPlayerPed);
						UpdateWeaponAttachment(wheelSlot.m_uWeaponHash);
					}

				}
			}

		}

		// just bail if we're forcing the wheel on
		if( m_bScriptForcedOn || m_bForcedOn)
		{
		#if USE_DIRECT_SELECTION
#if RSG_PC
			if (m_bForcedOn && m_eDirectSelectionSlot != MAX_WHEEL_SLOTS)
			{
				if( m_eDirectSelectionSlot != m_eLastSlotPointedAt )
				{
					SetPointer( m_eDirectSelectionSlot );
				}
				m_bSuppressWheelResultDueToQuickTap = false;
				m_bSuppressWheelResults = false;
				m_bForceResult = true;
				SetTargetTimeWarp(TW_Normal);
				return;
			}
#elif RSG_ORBIS
			if (m_bForcedOn && m_eDirectSelectionSlot != MAX_WHEEL_SLOTS && m_eDirectSelectionSlot != m_eLastSlotPointedAt)
			{
				SetPointer( m_eDirectSelectionSlot );
				m_bForcedOn = false;
				m_bSuppressWheelResultDueToQuickTap = false;
				m_bSuppressWheelResults = false;
				Hide(true);
				return;
			}
#endif
		#endif // USE_DIRECT_SELECTION

			m_bForceResult = false;
			m_bSuppressWheelResultDueToQuickTap = true;
			m_bSuppressWheelResults = false; // This Frame kinda dealie
			SetTargetTimeWarp(TW_Normal);
			return;
		}

		SetTargetTimeWarp(TW_Slow);

		// Disable direct weapon selection (i.e. disable mouse-wheel scroll selection).
		pControl->SetWeaponSelectExclusive();

		if (m_bAllowWeaponDPadSelection)
		{
			if(pControl->GetWeaponWheelPrev().IsPressed())
			{
				m_bAllowWeaponDPadSelection = false;
				SetStickPressed();

				if (CHudTools::BeginHudScaleformMethod(NEW_HUD_WEAPON_WHEEL, "SET_INPUT_EVENT"))
				{
					CScaleformMgr::AddParamInt(10);
					CScaleformMgr::EndMethod();
				}
			}

			if(pControl->GetWeaponWheelNext().IsPressed())
			{
				m_bAllowWeaponDPadSelection = false;
				SetStickPressed();

				if (CHudTools::BeginHudScaleformMethod(NEW_HUD_WEAPON_WHEEL, "SET_INPUT_EVENT"))
				{
					CScaleformMgr::AddParamInt(11);
					CScaleformMgr::EndMethod();
				}
			}
		}

		// also adjust then stick values:
		s32 iStickRotation = HandleStick(stickPos, MAX_WHEEL_SLOTS, -1);
		if( iStickRotation != -1 && m_eLastSlotPointedAt!=iStickRotation)
		{
			SetPointer( static_cast<eWeaponWheelSlot>(iStickRotation));
		}
	}
	else
	{
		SetTargetTimeWarp(TW_Normal);

		Hide(true);

		// poll for the returned hash until we have got it so we can change the weapon to the returned hash:
		if (m_iHashReturnedId != 0)
		{
			// check to see if we have got a return value yet and if so, act upon it
			if (CScaleformMgr::IsReturnValueSet(m_iHashReturnedId))
			{
				atHashWithStringNotFinal iNewWeaponHash = (u32)CScaleformMgr::GetReturnValueInt(m_iHashReturnedId);

				if (iNewWeaponHash.IsNotNull())
				{
					CPlayerInfo* pPlayerInfo = CGameWorld::GetMainPlayerInfo();

					if (pPlayerInfo)
					{
						CPedWeaponManager* pWeapMgr = pPlayerInfo->GetPlayerPed()->GetWeaponManager();
						if (pWeapMgr)
						{
							pWeapMgr->GetWeaponSelector()->SelectWeapon(pPlayerPed, SET_SPECIFIC_ITEM, true, iNewWeaponHash);
						}
					}

					if( iNewWeaponHash != pPlayerPed->GetDefaultUnarmedWeaponHash() )
						m_uLastEquippedWeaponWheelHash = iNewWeaponHash;
				}

				m_iHashReturnedId = 0;
				m_iFramesToWaitForAValidReturnedId = 0;
			}

			// value has timed out on us or something. BAIL!
			else if(m_iFramesToWaitForAValidReturnedId-- <= 0)
			{
				uiWarningf("Weapon Wheel result has timed out or could not be found. BAILING!");
				m_iHashReturnedId = 0;
				m_iFramesToWaitForAValidReturnedId = 0;
			}
		}

	#if TOUCHPAD_CHANGE_WEAPON_WHEEL_SLOT
		if (m_iWeaponCount != WeaponCount)
		{
			m_iWeaponCount.SetState(WeaponCount);
		}
	#endif // TOUCHPAD_CHANGE_WEAPON_WHEEL_SLOT
	}

	m_bSuppressWheelResults = false; // This Frame kinda dealie
}

void CWeaponWheel::SetForcedByScript( bool bOnOrOff )
{ 
	// if turning it off, but we were on, fade it out smoothly 
	// EXCEPT: This increases latency of controls [currently], so cutting it
//	if( !bOnOrOff && m_bScriptForcedOn )
//	{
//		START_CROSSFADE(CROSSFADE_TIME, 0);
//		m_bQueueingForceOff = true;
//	}
//	else 
	if( bOnOrOff && !m_bScriptForcedOn )
	{
		m_bScriptForcedOn = true;
		START_CROSSFADE(CROSSFADE_TIME, CROSSFADE_ALPHA);
		m_bQueueingForceOff = false;
	}
	else
	{
		m_bScriptForcedOn = bOnOrOff;
		m_bQueueingForceOff = false;
	}
}

void CWeaponWheel::SetSpecialVehicleAbilityFromScript()
{
	m_bSetSpecialVehicleCameraOverride = true;
}

void CWeaponWheel::FadeComplete()
{	
	if( m_bQueueingForceOff )
	{
		m_bScriptForcedOn = false;
	}
	m_bReceivedFadeComplete = true;
	m_bQueueingForceOff = false;
}

bool CWeaponWheel::HandleControls(CPed* pPlayerPed, bool& bNeedsRefresh )
{
	const CControl* pControl = pPlayerPed->GetControlFromPlayer();
	if(!pControl)
		return false;

	if(m_bSuppressWheelResults)
		return false;

	CPedWeaponSelector* pSelector = pPlayerPed->GetWeaponManager()->GetWeaponSelector();
	bool bChangedWeapon = false;
	bool bVehicleVersion = false;
	bool bParachuteVersion = false;
	bool bSelectSameWeapon = false;
	bool bHideWheelDisplay = false;
	ePedInventorySelectOption eSelectOption = SET_INVALID;
	atHashWithStringNotFinal uWeaponHash;

	if(pPlayerPed->GetVehiclePedInside() != NULL && !pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_IsExitingVehicle))
	{
		bVehicleVersion = true;
		bool bSelectPrevious = pSelector->GetIsPlayerSelectingPreviousVehicleWeapon(pPlayerPed);
		bool bSelectNext = pSelector->GetIsPlayerSelectingNextVehicleWeapon(pPlayerPed);
		if(bSelectPrevious || bSelectNext)
		{
			//B*1831239: Flag the weapon as changed so we show the currently equipped weapon icon again
			const CWeaponInfo *pCurrentWeaponInfo = pPlayerPed->GetEquippedWeaponInfo();
			
			bool bAirCraftWeapon = pCurrentWeaponInfo && pCurrentWeaponInfo->GetIsVehicleWeapon() && pPlayerPed->GetVehiclePedInside()->GetIsAircraft();

			// Ensure vehicle seat has a driveby anim info or vehicle weapon so we don't show unarmed/unusable weapons
			bool bVehHasDrivebyOrVehWeapon = false;

			const CVehicleSeatAnimInfo* pVehicleSeatAnimInfo = CVehicleMetadataMgr::GetSeatAnimInfoFromPed(pPlayerPed);
			if(pVehicleSeatAnimInfo && pVehicleSeatAnimInfo->GetDriveByInfo())
			{
				bVehHasDrivebyOrVehWeapon = true;
			}

			bVehHasDrivebyOrVehWeapon = bVehHasDrivebyOrVehWeapon || (pCurrentWeaponInfo && pCurrentWeaponInfo->GetIsVehicleWeapon());
		   
			if (!m_bVisible.GetState() && pCurrentWeaponInfo && !bAirCraftWeapon && bVehHasDrivebyOrVehWeapon)
			{
				bSelectSameWeapon = true;
			}
			else
			{
				eSelectOption = bSelectNext ? SET_NEXT : SET_PREVIOUS;
			}

			// Hide the weapon wheel if we're in first person, and the player's vehicle contains the flag FLAG_DISABLE_WEAPON_WHEEL_IN_FIRST_PERSON
			if(camInterface::GetCinematicDirector().IsRenderingCinematicMountedCamera())
			{
				CVehicleModelInfo* pVehModelInfo = pPlayerPed->GetVehiclePedInside()->GetVehicleModelInfo();
				if(pVehModelInfo && pVehModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_DISABLE_WEAPON_WHEEL_IN_FIRST_PERSON))
				{
					if(pPlayerPed->GetHelmetComponent() && pPlayerPed->GetHelmetComponent()->HasPilotHelmetEquippedInAircraftInFPS(false))
					{
						bHideWheelDisplay = true;
					}
				}
			}
		}
	}
	else if(pPlayerPed->GetIsParachuting() || pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_IsUsingJetpack))
	{
		bParachuteVersion = true;
		if(pSelector->GetIsPlayerSelectingNextParachuteWeapon(pPlayerPed))
			eSelectOption = SET_NEXT;

		else if(pSelector->GetIsPlayerSelectingPreviousParachuteWeapon(pPlayerPed))
			eSelectOption = SET_PREVIOUS;
	}
#if RSG_ORBIS
	else if ( CControlMgr::GetPlayerPad() &&
			 (!CControlMgr::IsDisabledControl( &CControlMgr::GetMainPlayerControl() ) &&		// if player control is disabled, the individual inputs in CONTROL_EMPTY are still enabled, so check if CONTROL_EMPTY is being used
			  !CControlMgr::GetMainPlayerControl().IsInputDisabled(INPUT_SELECT_WEAPON) &&
			  !CControlMgr::GetMainPlayerControl().IsInputDisabled(INPUT_WEAPON_WHEEL_NEXT) &&
			  !CControlMgr::GetMainPlayerControl().IsInputDisabled(INPUT_WEAPON_WHEEL_PREV) &&
			  !CControlMgr::GetMainPlayerControl().IsInputDisabled(INPUT_WEAPON_WHEEL_UD) &&
			  !CControlMgr::GetMainPlayerControl().IsInputDisabled(INPUT_WEAPON_WHEEL_LR)) &&
			 (CControlMgr::GetPlayerPad()->GetTouchPadGesture().IsSwipe(CTouchPadGesture::RIGHT) ||
			  CControlMgr::GetPlayerPad()->GetTouchPadGesture().IsSwipe(CTouchPadGesture::LEFT) ||
			  CControlMgr::GetPlayerPad()->GetTouchPadGesture().IsSwipe(CTouchPadGesture::DOWN) ||
			  CControlMgr::GetPlayerPad()->GetTouchPadGesture().IsSwipe(CTouchPadGesture::UP)) )
	{
		const CWeapon* pWeapon = pPlayerPed->GetWeaponManager()->GetEquippedWeapon();
		const CWeaponInfo* pWeaponInfo = pWeapon ? pWeapon->GetWeaponInfo() : NULL;
	#if !TOUCHPAD_CHANGE_WEAPON_WHEEL_SLOT
		if (CControlMgr::GetPlayerPad()->GetTouchPadGesture().IsSwipe(CTouchPadGesture::RIGHT))
		{
			eSelectOption = SET_NEXT;
		}
		else if (CControlMgr::GetPlayerPad()->GetTouchPadGesture().IsSwipe(CTouchPadGesture::LEFT))
		{
			eSelectOption = SET_PREVIOUS;
		}
	#else
		if (pWeaponInfo)
		{
			// Ensure m_eLastSlotPointedAt is up to date.
			m_eLastSlotPointedAt = pWeaponInfo->GetWeaponWheelSlot();
		}
		m_eDirectSelectionSlot = m_eLastSlotPointedAt;

		// Change weapon group.
		if (CControlMgr::GetPlayerPad()->GetTouchPadGesture().IsSwipe(CTouchPadGesture::RIGHT))
		{
			bChangedWeapon = true;
			if( SelectRadialSegment(1) )
				pSelector->SetLastWeaponType(m_eDirectSelectionSlot);
		}
		else if (CControlMgr::GetPlayerPad()->GetTouchPadGesture().IsSwipe(CTouchPadGesture::LEFT))
		{
			bChangedWeapon = true;
			if( SelectRadialSegment(-1) )
				pSelector->SetLastWeaponType(m_eDirectSelectionSlot);
		}
	#endif
	#if KEYBOARD_MOUSE_SUPPORT
		else if (CControlMgr::GetPlayerPad()->GetTouchPadGesture().IsSwipe(CTouchPadGesture::UP) &&
				 pWeaponInfo)
		{
			// Change weapon within group.
			eSelectOption = SET_NEXT_IN_GROUP;
			uWeaponHash = pWeaponInfo->GetWeaponWheelSlot();
		}
	#endif
		else if (CControlMgr::GetPlayerPad()->GetTouchPadGesture().IsSwipe(CTouchPadGesture::DOWN))
		{
			// change weapon
			atHashWithStringNotFinal iCurrentHashInUse = pWeaponInfo ? pWeaponInfo->GetHash() : 0;
			atHashWithStringNotFinal iNewHash;
			if (iCurrentHashInUse == pPlayerPed->GetDefaultUnarmedWeaponHash())
			{
				if (m_WheelMemory.GetSelected() != 0u && IsWeaponSelectable(m_WheelMemory.GetSelected()))
				{
					iNewHash = m_WheelMemory.GetSelected();
				}
				else
				{
					atHashWithStringNotFinal iBestWeaponHash = pPlayerPed->GetWeaponManager()->GetBestWeaponHash();
					if(iBestWeaponHash.IsNotNull() && iBestWeaponHash != pPlayerPed->GetDefaultUnarmedWeaponHash() && IsWeaponSelectable(iBestWeaponHash))
					{
						iNewHash = iBestWeaponHash;
					}
				}
			}
			else if (iCurrentHashInUse != 0)
			{
				m_WheelMemory.SetSelected( iCurrentHashInUse );
				iNewHash = pPlayerPed->GetDefaultUnarmedWeaponHash();
			}

			if (iNewHash != 0)
			{
				CPedWeaponManager *pWeapMgr = pPlayerPed->GetWeaponManager();
				pWeapMgr->GetWeaponSelector()->SelectWeapon(pPlayerPed, SET_SPECIFIC_ITEM, true, iNewHash);
				m_bSuppressWheelResultDueToQuickTap = true;
			}
		}
	}
#endif // RSG_ORBIS
#if KEYBOARD_MOUSE_SUPPORT
	else
	{
		if(pControl->GetSelectWeapon().IsPressed())
		{
			bool bCreateWhenLoaded = false;
			if(pSelector->GetSelectedWeapon() == GADGETTYPE_SKIS || 
				pSelector->GetSelectedWeapon() == GADGETTYPE_PARACHUTE )
			{
				if(!pPlayerPed->GetWeaponManager()->GetIsGadgetEquipped(pSelector->GetSelectedWeapon()))
				{
					bCreateWhenLoaded = true;
				}
			}

			bChangedWeapon = pPlayerPed->GetWeaponManager()->EquipWeapon(pSelector->GetSelectedWeapon(), pSelector->GetSelectedVehicleWeapon(), bCreateWhenLoaded);
		}
		if (!pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_IsExitingVehicle))
		{
			// Don't allow mouse scroll change when aiming with scope (zoom control helper sets exclusive input, but doesn't work with fixed-zoom scopes)
			bool bIsAimingWithFirstPersonScope = camInterface::GetGameplayDirector().IsFirstPersonAiming(pPlayerPed);

			if((pControl->GetPedSelectNextWeapon().IsPressed() || pControl->GetPedSelectPrevWeapon().IsPressed()) && !bIsAimingWithFirstPersonScope)
			{
				const CWeapon* pWeapon = pPlayerPed->GetWeaponManager()->GetEquippedWeapon();
				const CWeaponInfo* pWeaponInfo = pWeapon ? pWeapon->GetWeaponInfo() : NULL;
				if (pWeaponInfo && !m_bVisible.GetState() )
				{
					// Ensure m_eLastSlotPointedAt is up to date.
					m_eLastSlotPointedAt = pWeaponInfo->GetWeaponWheelSlot();
				}
				bChangedWeapon = true;
				SelectRadialSegment(pControl->GetPedSelectNextWeapon().IsPressed() ? 1 : -1);
			}
			else if(pControl->GetSelectWeaponUnarmed().IsPressed())
			{
				eSelectOption = SET_SPECIFIC_ITEM;
				uWeaponHash = pPlayerPed->GetDefaultUnarmedWeaponHash();
			}
			else if(pControl->GetSelectWeaponMelee().IsPressed())
			{
				eSelectOption = SET_NEXT_IN_GROUP;
				uWeaponHash = WEAPON_WHEEL_SLOT_UNARMED_MELEE;
			}
			else if(pControl->GetSelectWeaponHandgun().IsPressed())
			{
				eSelectOption = SET_NEXT_IN_GROUP;
				uWeaponHash = WEAPON_WHEEL_SLOT_PISTOL;
			}
			else if(pControl->GetSelectWeaponShotgun().IsPressed())
			{
				eSelectOption = SET_NEXT_IN_GROUP;
				uWeaponHash = WEAPON_WHEEL_SLOT_SHOTGUN;
			}
			else if(pControl->GetSelectWeaponSMG().IsPressed())
			{
				eSelectOption = SET_NEXT_IN_GROUP;
				uWeaponHash = WEAPON_WHEEL_SLOT_SMG;
			}
			else if(pControl->GetSelectWeaponAutoRifle().IsPressed())
			{
				eSelectOption = SET_NEXT_IN_GROUP;
				uWeaponHash = WEAPON_WHEEL_SLOT_RIFLE;
			}
			else if(pControl->GetSelectWeaponSniper().IsPressed())
			{
				eSelectOption = SET_NEXT_IN_GROUP;
				uWeaponHash = WEAPON_WHEEL_SLOT_SNIPER;
			}
			else if(pControl->GetSelectWeaponHeavy().IsPressed())
			{
				eSelectOption = SET_NEXT_IN_GROUP;
				uWeaponHash = WEAPON_WHEEL_SLOT_HEAVY;
			}
			else if(pControl->GetSelectWeaponSpecial().IsPressed())
			{
				eSelectOption = SET_NEXT_IN_GROUP;
				uWeaponHash = WEAPON_WHEEL_SLOT_THROWABLE_SPECIAL;
			}
		}
	}
#endif // KEYBOARD_MOUSE_SUPPORT

	// done seeing if we changed anything

	// if we didn't vehicle switch, and a button was pressed, check it
	if( !bChangedWeapon && (bSelectSameWeapon || eSelectOption != SET_INVALID)  && !pPlayerPed->GetPedConfigFlag( CPED_CONFIG_FLAG_BlockWeaponSwitching ) && !pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_TemporarilyBlockWeaponSwitching))
	{
	//	bool bVehicleHasWeapons = bVehicleVersion && pPlayerPed->GetVehiclePedInside()->GetSeatHasWeaponsOrTurrets(pPlayerPed->GetAttachCarSeatIndex());
		bool bSetWeapon = bSelectSameWeapon ? true : pSelector->SelectWeapon(pPlayerPed, eSelectOption, true, uWeaponHash,-1, false);
		if( bSetWeapon )
		{
			const CWeaponInfo* pInfo = pSelector->GetSelectedWeaponInfo();
			if( pInfo )
			{
				SetWeapon( static_cast<u8>(pInfo->GetWeaponWheelSlot()), pInfo->GetHash());
				//bVehicleHasWeapons = false;
			}
			else 
			{
				pInfo = pSelector->GetSelectedVehicleWeaponInfo(pPlayerPed);
				if(pInfo)
					SetWeapon( 0, pInfo->GetHash());
			}
			
			bChangedWeapon = true;//!bVehicleVersion; // just force this on, for times when you have no guns (just fist!)

		#if USE_DIRECT_SELECTION
			m_eDirectSelectionSlot = MAX_WHEEL_SLOTS;
		#endif // USE_DIRECT_SELECTION
		}
		else
			bChangedWeapon = false;

	}


	// if we changed a weapon, adjust the force-show time
	if( bChangedWeapon && !bHideWheelDisplay)
	{
		m_iForceWheelShownTime.Start();
		bNeedsRefresh = true;
		if(camInterface::GetGameplayDirector().GetFirstPersonPedAimCamera())
		{
			m_bSwitchWeaponInSniperScope = true;
		}
		else
		{
			m_bSwitchWeaponInSniperScope = false;
		}
	}

	return IsForcedOpenByTimer();
}

void CWeaponWheel::ForceSpecialVehicleStatusUpdate()
{
	// Only if FPS mode.
	if ( !camInterface::GetGameplayDirector().GetFirstPersonVehicleCamera() && !m_bSetSpecialVehicleCameraOverride)
	{
		uiDebugf1("ForceSpecialVehicleStatusUpdate FAILED: !GetFirstPersonVehicleCamera() && !m_bSpecialVehicleCameraOverride");
		return;
	}

	
	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
	if ( pPlayerPed  && pPlayerPed->GetMyVehicle() && pPlayerPed->GetMyVehicle()->InheritsFromAmphibiousQuadBike() )
	{
		CVehicle* pVehicle = pPlayerPed->GetMyVehicle();
		CAmphibiousQuadBike* pQuadBike = dynamic_cast<CAmphibiousQuadBike*>(pVehicle);
		if ( pQuadBike )
		{
			Hide(true);
			m_iForceWheelShownTime.Zero();
			m_iForceWheelShownTime.Start();
			m_bSwitchWeaponInSniperScope = false;
			m_bToggleSpecialVehicleAbility = true; 
			m_bSetSpecialVehicleCameraOverride = false;

		}
		else
		{
			uiDebugf1("ForceSpecialVehicleStatusUpdate FAILED: !QuadBike");
		}

	}
}

void CWeaponWheel::SetWeapon(u8 iSlotIndex, atHashWithStringNotFinal uWeaponHash)
{
	m_WheelMemory.SetWeapon(iSlotIndex, uWeaponHash);
}

void CWeaponWheel::RequestHdWeapons()
{
	for (u8 iWeaponSlot = 0; iWeaponSlot < MAX_WHEEL_SLOTS; ++iWeaponSlot)
	{
		RequestHdWeaponAssets(iWeaponSlot,  m_WheelMemory.GetWeapon(iWeaponSlot) );
	}
}

void CWeaponWheel::ReleaseHdWeapons()
{
	for (u8 iSlotNum = 0; iSlotNum < MAX_WHEEL_SLOTS*2; iSlotNum++)
	{
		m_aRequests[iSlotNum].Release();
	}
}

void CWeaponWheel::RequestHdWeaponAssets(u8 iSlotIndex, atHashWithStringNotFinal uWeaponHash)
{
	if( uWeaponHash.IsNull() )
		return;

	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(uWeaponHash);
	if(!pWeaponInfo)
		return;

	const CWeaponModelInfo* pWInfo = smart_cast<const CWeaponModelInfo*>(pWeaponInfo->GetModelInfo());
	if( !pWInfo )
		return;

	strLocalIndex drawableIndex = strLocalIndex(pWInfo->GetHDDrawableIndex());
	if( m_aRequests[iSlotIndex].GetRequestId() != drawableIndex )
	{
		strLocalIndex txdIndex = strLocalIndex(pWInfo->GetHDTxdIndex());
		m_aRequests[iSlotIndex].Release();
		m_aRequests[iSlotIndex+MAX_WHEEL_SLOTS].Release();

		if( drawableIndex != -1 )
			m_aRequests[iSlotIndex].Request(drawableIndex, g_DrawableStore.GetStreamingModuleId(), STRFLAG_FORCE_LOAD | STRFLAG_PRIORITY_LOAD);

		if( txdIndex != -1 )
			m_aRequests[iSlotIndex+MAX_WHEEL_SLOTS].Request(txdIndex, g_TxdStore.GetStreamingModuleId(), STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
	}
}

void CWeaponWheel::FillScaleform( eWeaponWheelSlot iInitialSlot, bool bCarMode )
{
	if( !bCarMode)
	{
		for (u8 iWeaponSlot = 0; iWeaponSlot < MAX_WHEEL_SLOTS; iWeaponSlot++)
		{
			atRangeArray<sWeaponWheelSlot, MAX_WHEEL_WEAPONS_IN_SLOT>& curWeaponTypeSlot = m_Slot[iWeaponSlot];
			
			u8& rCurWeaponHighlight = m_iIndexOfWeaponToShowInitiallyForThisSlot[iWeaponSlot];
			//	If the last weapon selected for this slot is no longer selectable,
			if( rCurWeaponHighlight == INVALID_WEAPON_INDEX
				|| curWeaponTypeSlot[ rCurWeaponHighlight ].IsEmpty() 
				|| !curWeaponTypeSlot[ rCurWeaponHighlight ].IsSelectable() )
			{
				//	try to find another weapon in this slot that is selectable and set that as the initial weapon to show for this slot.
				//	For now, just start from 0 and check all slots up to MAX_WHEEL_WEAPONS_IN_SLOT until we find a selectable one (if there are any)
				for( u8 weapon_loop = 0; weapon_loop < MAX_WHEEL_WEAPONS_IN_SLOT; weapon_loop++ )
				{
					if( !curWeaponTypeSlot[weapon_loop].IsEmpty() && curWeaponTypeSlot[weapon_loop].IsSelectable() )
					{
						rCurWeaponHighlight = weapon_loop;
						break;
					}
				}

				// STILL unfound? Fuggit. Circle gets the square
				rCurWeaponHighlight = 0;
			}

			for (u8 i = 0; i < MAX_WHEEL_WEAPONS_IN_SLOT; i++)
			{
				bool bIsSelected = (i == m_iIndexOfWeaponToShowInitiallyForThisSlot[iWeaponSlot]);

				curWeaponTypeSlot[i].SetScaleform( bIsSelected, iWeaponSlot, i);
				if(bIsSelected)
					SetWeapon(iWeaponSlot, curWeaponTypeSlot[i].m_uWeaponHash);
			}
		}

		//
		// set it up so it defaults to the currently selected weapon (ie points at the one you have in your hand)
		if (iInitialSlot >= 0 && iInitialSlot < MAX_WHEEL_SLOTS)
		{
			SetPointer(iInitialSlot);
		}

	}
	else // bCarMode
	{
		CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
		if ( m_bToggleSpecialVehicleAbility )
		{
			CVehicle* pVehicle = pPlayerPed->GetMyVehicle();
			CAmphibiousQuadBike* pQuadBike = dynamic_cast<CAmphibiousQuadBike*>(pVehicle);
			if ( pQuadBike )
			{
				bool const c_areWheelsIn = pQuadBike->ShouldTuckWheelsIn();

				u32 const c_wheelHash = c_areWheelsIn  ? AQUABIKE_WHEELS_UP : AQUABIKE_WHEELS_DOWN;
				const char* c_localizedKey = c_areWheelsIn  ? AQUABIKE_WHEELS_UP_NAME : AQUABIKE_WHEELS_DOWN_NAME;
				SetSpecialVehicleWeapon( c_wheelHash, c_localizedKey );
				m_bToggleSpecialVehicleAbility = false;
				
			}

		}
		else if ( iInitialSlot >= 0 && iInitialSlot < MAX_WHEEL_SLOTS )
		{
			atRangeArray<sWeaponWheelSlot, MAX_WHEEL_WEAPONS_IN_SLOT>& curWeaponTypeSlot = m_Slot[iInitialSlot];
			sWeaponWheelSlot& lastWeapon = curWeaponTypeSlot[ m_iIndexOfWeaponToShowInitiallyForThisSlot[iInitialSlot] ];
			lastWeapon.SetScaleform(true, 8, 0);
			SetPointer(MAX_WHEEL_SLOTS); // 8 is scaleform for wheel mode
		}
	}

	// parachute check whether or not you are in a car
	if (CHudTools::BeginHudScaleformMethod(NEW_HUD_WEAPON_WHEEL, "SET_PARACHUTE_IS_VISIBLE"))
	{
		CScaleformMgr::AddParamBool( m_bQueueParachute );
		CScaleformMgr::AddParamInt( m_iNumParachutes );
		CScaleformMgr::EndMethod();
	}

	CHudTools::CallHudScaleformMethod(NEW_HUD_WEAPON_WHEEL, "SHOW");

	if (CHudTools::BeginHudScaleformMethod(NEW_HUD_WEAPON_WHEEL, "TOGGLE_POINTER_AND_WEAPON_NAME_VISIBILITY"))
	{
		CScaleformMgr::AddParamBool(!m_bScriptForcedOn);
		CScaleformMgr::EndMethod();
	}
}

void CWeaponWheel::SetSpecialVehicleWeapon( u32 uWeaponHash, const char* UNUSED_PARAM(locKey) )
{
	if (CHudTools::BeginHudScaleformMethod(NEW_HUD_WEAPON_WHEEL, "SET_PLAYER_WEAPON_WHEEL"))
	{	
		CScaleformMgr::AddParamInt(8);  // weapon slot
		CScaleformMgr::AddParamInt(0);		 // array index of weapon type

		CScaleformMgr::AddParamInt(uWeaponHash);  // weapon hash
		CScaleformMgr::AddParamInt(1); // weapon selected or not

		// ammo:
		CScaleformMgr::AddParamInt(-1);
		CScaleformMgr::AddParamInt(-1);
		CScaleformMgr::AddParamInt(-1);

		//	Pass an extra parameter to say whether the weapon should be selectable or greyed out
		CScaleformMgr::AddParamInt(1);
		CScaleformMgr::AddParamString(" ");

		CScaleformMgr::AddParamBool(true);
		CScaleformMgr::EndMethod();
	}

	ClearSpecialVehicleWeaponName(uWeaponHash);
}

void CWeaponWheel::ClearSpecialVehicleWeaponName(u32 uWeaponHash)
{
	if (CHudTools::BeginHudScaleformMethod(NEW_HUD_WEAPON_WHEEL, "setWeaponLabel"))
	{	
		CScaleformMgr::AddParamInt(8);  // weapon slot
		CScaleformMgr::AddParamInt(0);		 // array index of weapon type

		CScaleformMgr::AddParamInt(uWeaponHash);  // weapon hash
		CScaleformMgr::AddParamInt(1); // weapon selected or not

		// ammo:
		CScaleformMgr::AddParamInt(-1);
		CScaleformMgr::AddParamInt(-1);
		CScaleformMgr::AddParamInt(-1);

		//	Pass an extra parameter to say whether the weapon should be selectable or greyed out
		CScaleformMgr::AddParamInt(1);
		CScaleformMgr::AddParamString("");

		CScaleformMgr::AddParamBool(true);
		CScaleformMgr::EndMethod();
	}
}
bool CWeaponWheel::CheckIncomingFunctions(atHashWithStringBank uMethodName, const GFxValue args[])
{
	// public function WEAPON_WHEEL_ACTIVE  // this gets called by AS when the weapon wheel is fully ON (true) and fully OFF (false)
	if (uMethodName==ATSTRINGHASH("WEAPON_WHEEL_ACTIVE",0xe16daf9a))
	{
		// ultimately was never used, so catching it just to be polite
		//if (uiVerifyf(args[1].IsBool(), "WEAPON_WHEEL_ACTIVE params not compatible: %s", sfScaleformManager::GetTypeName(args[1])))
		//{
			// we have a request for a stream.
			//ms_WeaponWheel.SetActive( args[1].GetBool() );
		//}

		return true;
	}

	if (uMethodName==ATSTRINGHASH("SET_WEAPON_WHEEL_SLOT_SELECTION",0x34fc2b48))
	{
		if (uiVerifyf(args[1].IsNumber() && args[2].IsNumber()
			, "SET_WEAPON_WHEEL_SLOT_SELECTION params not compatible: %s %s"
			, sfScaleformManager::GetTypeName(args[1]), sfScaleformManager::GetTypeName(args[2])))
		{
			u8 iSlotIndex = (u8)args[1].GetNumber();
			atHashWithStringNotFinal uWeaponHash = (u32)args[2].GetNumber();

			if(uWeaponHash != 0)
				SetWeapon(iSlotIndex, uWeaponHash);
		}

		m_bAllowWeaponDPadSelection = true;

		return true;
	}
	if( uMethodName == ATSTRINGHASH("WEAPON_WHEEL_FADE_COMPLETE",0x358d4aa0) )
	{
		FadeComplete();
		return true;
	}

	if (uMethodName==ATSTRINGHASH("UPDATE_WEAPON_ATTACHMENT_FROM_SLOT",0xcf36084c))
	{
		// if the wheel isn't 'active', don't reprompt it again as it causes badness.
		if(  !IsVisible() )
			return true;

		if( uiVerifyf(args[1].IsNumber(), "UPDATE_WEAPON_ATTACHMENT_FROM_SLOT param not compatible: %s"
			, sfScaleformManager::GetTypeName(args[1])))
		{
			atHashWithStringNotFinal uWeaponHash = (u32)args[1].GetNumber();
			if( m_uLastHighlightedWeaponWheelHash != uWeaponHash)
				UpdateWeaponAttachment(uWeaponHash);

		}
		return true;
	}

	return false;
}

void CWeaponWheel::SetContentsFromScript(SWeaponWheelScriptNode paScriptNodes[], int nodeCount)
{
	Clear();

	if( m_bVisible.GetState() )
	{
		m_bRebuild = true;
		START_CROSSFADE(CROSSFADE_TIME, 0);
	}
	else
	{
		START_CROSSFADE(CROSSFADE_TIME, CROSSFADE_ALPHA);
	}

	const CWeaponInfoBlob::SlotList& slotListNavigate = CWeaponInfoManager::GetSlotListNavigate();

	m_bQueueParachute = false;

	for(int iArrayIndex=0; iArrayIndex<nodeCount; ++iArrayIndex)
	{
		atHashWithStringNotFinal curWeaponHash = paScriptNodes[iArrayIndex].uWeaponHash.Int;
		if( curWeaponHash.IsNull())
			continue;

		if(curWeaponHash == GADGETTYPE_PARACHUTE.GetHash())
			m_bQueueParachute = true;

		const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(curWeaponHash);
		if( !pWeaponInfo || pWeaponInfo->GetHiddenFromWeaponWheel() )
			continue;

		// fill out the weapon based on the order it's supposed to appear in for 'best'
		sWeaponWheelSlot newSlot;
		newSlot.SetFromHash(curWeaponHash, __SPECIAL_FLAG_DONT_DISPLAY_AMMO);

		CWeaponInfoBlob::SlotEntry entry(pWeaponInfo->GetSlot());
		s32 slotIndex = slotListNavigate.Find( entry );
		if(	Verifyf( slotIndex >= 0, "Unable to find a slot for %s, 0x%08x (%s) in SetContentsFromScript", pWeaponInfo->GetName(), curWeaponHash.GetHash(), curWeaponHash.TryGetCStr() ) )
			MapWeaponSlotToWeaponWheel(slotIndex, 0u, newSlot);
	}
}

eWeaponWheelSlot CWeaponWheel::SetContentsFromPed( const CPed* pPlayerPed )
{
	// clear the list
	Clear(false);


	eWeaponWheelSlot iInitialSlot = MAX_WHEEL_SLOTS;
	const CPedWeaponManager* pPlayerWeaponManager = pPlayerPed->GetWeaponManager();
	const atHashWithStringNotFinal uEquippedWeaponHash = pPlayerWeaponManager->GetWeaponSelector()->GetSelectedWeapon();

	const CWeaponItem* pParachute = pPlayerPed->GetInventory()->GetWeapon(GADGETTYPE_PARACHUTE.GetHash());
	m_bQueueParachute = pParachute != NULL && (!pPlayerPed->GetIsInVehicle() || pPlayerPed->GetMyVehicle()->GetIsAircraft());
	if(m_bQueueParachute)
	{
		const int DEFAULT_PARACHUTE = 1;
		m_iNumParachutes = DEFAULT_PARACHUTE + (int)pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_HasReserveParachute);
	}

	// Early out for vehicle weapons
	const CVehicleWeapon* pVehicleWeapon = pPlayerWeaponManager->GetEquippedVehicleWeapon();
	if( pVehicleWeapon && pVehicleWeapon->GetWeaponInfo())
	{
		sWeaponWheelSlot newSlot;
		newSlot.m_bIsSelectable = false;
		newSlot.SetFromHash(pVehicleWeapon->GetWeaponInfo()->GetHash(), __SPECIAL_FLAG_DONT_DISPLAY_AMMO, pPlayerPed );

		// Set Weapon Suffix (Homing weapons which are disabled only)
		if(pVehicleWeapon->GetWeaponInfo()->GetCanHomingToggle()
			&& !pPlayerPed->GetPlayerInfo()->GetTargeting().GetVehicleHomingEnabled())
		{
			newSlot.m_HumanNameHashSuffix = "WT_HOMING_DISABLED";
		}

		return MapWeaponSlotToWeaponWheel(0, newSlot.m_uWeaponHash, newSlot);
	}

	//
	// go through all 17 weapon slots, get valid weapon info and add it to one of the 8 weapon wheel slots in the correct places
	//
	const CWeapon* pEquippedWeapon = pPlayerWeaponManager->GetEquippedWeapon();

	const CWeaponInfoBlob::SlotList& slotListNavigate = CWeaponInfoManager::GetSlotListNavigate(pPlayerPed);
	const CPedWeaponSelector* pPedWeaponSelector = pPlayerWeaponManager->GetWeaponSelector();
	u32 uCurrWeaponHash = pPedWeaponSelector ? pPedWeaponSelector->GetSelectedWeapon() : 0;

	for (s32 iIndex = 0;  iIndex < slotListNavigate.GetCount(); iIndex++)
	{
		const CWeaponItem* pItem = pPlayerPed->GetInventory()->GetWeaponBySlot(slotListNavigate[iIndex].GetHash());
		if(pItem)
		{
			if (pItem->GetInfo())
			{
				atHashWithStringNotFinal uWeaponHash = pItem->GetInfo()->GetHash();
				sWeaponWheelSlot newSlot;

				bool bIsInfinite = pPlayerPed->GetInventory()->GetIsWeaponUsingInfiniteAmmo(uWeaponHash);

				// prefer weapons in your hands, as they have clip information
				if (pEquippedWeapon && pEquippedWeapon->GetWeaponHash() == uWeaponHash)
				{
					newSlot.SetFromWeapon(pEquippedWeapon, bIsInfinite);
				}
				else
				{
					s32 ammoToShow =  bIsInfinite ? __SPECIAL_FLAG_DONT_DISPLAY_AMMO : pPlayerPed->GetInventory()->GetWeaponAmmo(uWeaponHash);
					
					if(ammoToShow == 0)
					{
						const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(uWeaponHash);
						if(pWeaponInfo && pWeaponInfo->GetIsThrownWeapon())
						{
							continue; // skip over setting this slot if no ammo, and is not a melee weapon
						}
					}
					
					// B*2295321: Display correct clip ammo in weapon wheel.
					s32 iAmmoClipOverride = uWeaponHash == m_uWeaponInHandHash ? GetHolsterAmmoInClip() : __SPECIAL_FLAG_DONT_DISPLAY_AMMO;
					if (iAmmoClipOverride == __SPECIAL_FLAG_DONT_DISPLAY_AMMO && pItem->GetLastAmmoInClip() != -1)
					{
						iAmmoClipOverride = pItem->GetLastAmmoInClip();
					}

					newSlot.SetFromHash(uWeaponHash, ammoToShow, pPlayerPed, iAmmoClipOverride);
				}
				newSlot.m_bIsSelectable = IsWeaponSelectable(uWeaponHash);
				newSlot.m_bIsEquipped = uCurrWeaponHash == uWeaponHash;


				eWeaponWheelSlot eNewWheelSlot = MapWeaponSlotToWeaponWheel(iIndex, uEquippedWeaponHash, newSlot);
				if( eNewWheelSlot != MAX_WHEEL_SLOTS )
					iInitialSlot = eNewWheelSlot;
			}
		}
	}	

	return iInitialSlot;
}

void CWeaponWheel::SetPointer( eWeaponWheelSlot iStickRotation )
{
	if( CHudTools::BeginHudScaleformMethod(NEW_HUD_WEAPON_WHEEL, "SET_POINTER") )
	{
		CScaleformMgr::AddParamInt(iStickRotation);
		CScaleformMgr::AddParamBool(true); // always direct
		CScaleformMgr::EndMethod();
	}

	m_eLastSlotPointedAt = iStickRotation;
}

void CWeaponWheel::START_CROSSFADE(float time, int alpha)
{
	m_bReceivedFadeComplete = false;
	m_iFailsafeTimer = 15; // wait no more than 15 frames
	// this is basically here until the movie is regenerated
	if( CHudTools::BeginHudScaleformMethod(NEW_HUD_WEAPON_WHEEL,"START_CROSSFADE") )
	{
		CScaleformMgr::AddParamFloat(time);
		CScaleformMgr::AddParamInt(alpha);
		CScaleformMgr::EndMethod();
	}
}

void CWeaponWheel::FindAndUpdateWeaponWheelStats(bool bCarMode, atHashWithStringNotFinal uWeaponHash, CPed* pPlayerPed)
{
	s8 iAttachmentsHudDamage = 0;
	s8 iAttachmentsHudSpeed = 0;
	s8 iAttachmentsHudAccuracy = 0;
	s8 iAttachmentsHudRange = 0;

	if (!bCarMode && uWeaponHash != 0 
		&& pPlayerPed && pPlayerPed->GetInventory() && pPlayerPed->GetWeaponManager())
	{
		const CWeaponItem* pItem = pPlayerPed->GetInventory()->GetWeapon(uWeaponHash);
		if (pItem && pItem->GetComponents())
		{
			const CWeaponItem::Components& components = *pItem->GetComponents();

			for (int i = 0; i < components.GetCount(); i++)
			{
				const CWeaponComponentInfo* pInfo = components[i].pComponentInfo;

				if( pInfo->IsShownOnWheel() )
				{
					// add to total calculated attachment stats
					iAttachmentsHudDamage += pInfo->GetHudDamage();
					iAttachmentsHudSpeed += pInfo->GetHudSpeed();
					iAttachmentsHudAccuracy += pInfo->GetHudAccuracy();
					iAttachmentsHudRange += pInfo->GetHudRange();
				}
			}
		}
	}

	UpdateWeaponWheelStatsWithAttachments(bCarMode, uWeaponHash, iAttachmentsHudDamage, iAttachmentsHudSpeed, iAttachmentsHudAccuracy, iAttachmentsHudRange);
}

void CWeaponWheel::UpdateWeaponWheelStatsWithAttachments(bool bCarMode, atHashWithStringNotFinal uWeaponHash, s8 iAttachmentsHudDamage, s8 iAttachmentsHudSpeed, s8 iAttachmentsHudAccuracy, s8 iAttachmentsHudRange)
{
	bool bStatsSet = false;

	if(!bCarMode && !m_bScriptForcedOn)
	{
		const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(uWeaponHash);
		bool bMetadataSaysOK = pWeaponInfo && !pWeaponInfo->GetFlag(CWeaponInfoFlags::NoWheelStats);
	
		// update the stats display as well
		if( bMetadataSaysOK && CHudTools::BeginHudScaleformMethod(NEW_HUD_WEAPON_WHEEL_STATS, "SET_STATS_LABELS_AND_VALUES") )
		{
			bStatsSet = true;

			CScaleformMgr::AddParamLocString("PM_DAMAGE");
			CScaleformMgr::AddParamLocString("PM_FIRERATE");
			CScaleformMgr::AddParamLocString("PM_ACCURACY");
			CScaleformMgr::AddParamLocString("PM_RANGE");

			// add raw weapon stats
			CScaleformMgr::AddParamInt(pWeaponInfo->GetHudDamage());
			CScaleformMgr::AddParamInt(pWeaponInfo->GetHudSpeed());
			CScaleformMgr::AddParamInt(pWeaponInfo->GetHudAccuracy());
			CScaleformMgr::AddParamInt(pWeaponInfo->GetHudRange());

			// add attachment stats
			CScaleformMgr::AddParamInt(iAttachmentsHudDamage);
			CScaleformMgr::AddParamInt(iAttachmentsHudSpeed);
			CScaleformMgr::AddParamInt(iAttachmentsHudAccuracy);
			CScaleformMgr::AddParamInt(iAttachmentsHudRange);

			CScaleformMgr::EndMethod();
		}
	}

	if(CHudTools::BeginHudScaleformMethod(NEW_HUD_WEAPON_WHEEL_STATS, "SET_STATS_VISIBILITY") )
	{
		CScaleformMgr::AddParamBool(bStatsSet);
		CScaleformMgr::EndMethod();
	}
}

void CWeaponWheel::SetWeaponInMemory( atHashWithStringNotFinal uWeaponHash )
{
	if(uWeaponHash.IsNull())
		return;

	const CWeaponInfo* pCurrentWeaponSlot = CWeaponInfoManager::GetInfo<CWeaponInfo>(uWeaponHash);
	if(uiVerifyf(pCurrentWeaponSlot, "Expected a weaponinfo out of %s 0x%08x, but got bupkis", uWeaponHash.TryGetCStr(), uWeaponHash.GetHash()))
	{
		m_WheelMemory.SetWeapon( static_cast<u8>(pCurrentWeaponSlot->GetWeaponWheelSlot()), uWeaponHash);
	}
}

atHashWithStringNotFinal CWeaponWheel::GetWeaponInMemory(u8 uSlotIndex) const
{
	return m_WheelMemory.GetWeapon(uSlotIndex);
}

void CWeaponWheel::UpdateWeaponAttachment(atHashWithStringNotFinal uWeaponHash)
{
	m_uLastHighlightedWeaponWheelHash = uWeaponHash;

	s8 iAttachmentsHudDamage = 0;
	s8 iAttachmentsHudSpeed = 0;
	s8 iAttachmentsHudAccuracy = 0;
	s8 iAttachmentsHudRange = 0;


	CGxtString szAttachmentString = "";

	if(uWeaponHash != 0)
	{
		const CPed *pPlayerPed = CGameWorld::FindLocalPlayer();

		if (pPlayerPed && pPlayerPed->GetInventory() && pPlayerPed->GetWeaponManager())
		{
			const CWeaponItem* pItem = pPlayerPed->GetInventory()->GetWeapon(uWeaponHash);
			if (pItem)
			{
				const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(uWeaponHash);
				if (pItem->GetComponents())
				{
					const CWeaponItem::Components& components = *pItem->GetComponents();
					for (int i = 0; i < components.GetCount(); i++)
					{
						const CWeaponComponentInfo* pInfo = components[i].pComponentInfo;
						if( pInfo->IsShownOnWheel() )
						{
							szAttachmentString.Append( TheText.Get(pInfo->GetLocKey().GetHash(), NULL) );
							szAttachmentString.Append("<BR>");

							// add to total calculated attachment stats
							iAttachmentsHudDamage += pInfo->GetHudDamage();
							iAttachmentsHudSpeed += pInfo->GetHudSpeed();
							iAttachmentsHudAccuracy += pInfo->GetHudAccuracy();
							iAttachmentsHudRange += pInfo->GetHudRange();
						}
					}

					// If weapon is gun and flagged as silenced internally, add "Suppressor" to the attachments list
					if(pWeaponInfo && pWeaponInfo->GetIsGun() && pWeaponInfo->GetIsSilenced())
					{
						szAttachmentString.Append(TheText.Get("WCT_SUPP"));
						szAttachmentString.Append("<BR>");
					}
				}

				bool bIsMeleeWeapon = pWeaponInfo && pWeaponInfo->GetIsMelee();

				// if there are no attachments (string is empty), and this kind of weapon can have attachments, say "No attachments"
				if( szAttachmentString.GetCharacterCount() == 0 && !bIsMeleeWeapon)
				{
					// for every attachment point
					const CWeaponInfo::AttachPoints& attachPoints = pItem->GetInfo()->GetAttachPoints();
					int i=attachPoints.GetCount();
					while(i--)
					{
						// for every possible component on each point
						const CWeaponComponentPoint::Components& components = attachPoints[i].GetComponents(); 
						int j=components.GetCount();
						while(j--)
						{
							// if the component can be shown, or isn't default
							const CWeaponComponentInfo* pComponentInfo = CWeaponComponentManager::GetInfo(components[j].m_Name.GetHash());
							bool bIsShownOnWheel = pComponentInfo && pComponentInfo->IsShownOnWheel();
							if( !components[j].m_Default && bIsShownOnWheel)
							{
								// then say "No Attachments" and bail from the loop
								szAttachmentString.Set( TheText.Get("NO_ATTACHMENTS") );
								i = 0;
								j = 0;
							}
						}
					}
				}
			}
		}		
	}

	if(CHudTools::BeginHudScaleformMethod(NEW_HUD_WEAPON_WHEEL, "SET_ATTACHMENT_LABELS"))
	{
		CScaleformMgr::AddParamString(szAttachmentString.GetData());
		CScaleformMgr::EndMethod();
	}
	
	UpdateWeaponWheelStatsWithAttachments(false, uWeaponHash, iAttachmentsHudDamage, iAttachmentsHudSpeed, iAttachmentsHudAccuracy, iAttachmentsHudRange);
}

void CWeaponWheel::sWeaponWheelSlot::SetFromWeapon( const CWeapon* pCurrentWeapon, bool bIsInfiniteAmmo )
{
	Clear();

	// Attempt to get the ammo from the CWeapon, as this maintains the current ammo in the clip
	const CWeaponInfo* pWeaponInfo = pCurrentWeapon->GetWeaponInfo();

	if (pWeaponInfo)
	{
		m_uWeaponHash = pWeaponInfo->GetHash();
		m_HumanNameHash = pWeaponInfo->GetHumanNameHash();
		if( bIsInfiniteAmmo )
		{
			m_iClipAmmo = m_iClipSize = m_iAmmo = __SPECIAL_FLAG_DONT_DISPLAY_AMMO;
		}
		else
		{
			m_iAmmo		= pCurrentWeapon->GetAmmoTotal();
			m_iClipSize = pCurrentWeapon->GetClipSize();
			m_iClipAmmo = pCurrentWeapon->GetAmmoInClip();

			if (m_uWeaponHash == ATSTRINGHASH("WEAPON_DBSHOTGUN",0xEF951FBB) && m_iClipAmmo == 1)
			{
				//B*2642059 - DBShotgun will be forced to fire twice if interrupted between shots. set the ammo left in clip correctly
				m_iClipAmmo = 0;
			}
			Validate();
		}

		const CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
		if (pPlayerPed)
		{
			const CAmmoInfo* pAmmoInfo = pWeaponInfo->GetAmmoInfo(pPlayerPed);
			m_iAmmoSpecialType = pAmmoInfo ? (s32)pAmmoInfo->GetAmmoSpecialType() : 0;
		}
	}
}

s32 CWeaponWheel::GetClipSizeFromComponent(const CPed* pPed, atHashWithStringNotFinal uWeaponHash )
{
	if( pPed && pPed->GetInventory() )
	{
		const CWeaponItem* pWeaponItem = (uWeaponHash != 0) ? pPed->GetInventory()->GetWeapon(uWeaponHash) : NULL;
		if( pWeaponItem )
		{
			const CWeaponItem::Components* pComponents = pWeaponItem->GetComponents();
			if( pComponents )
			{
				const CWeaponItem::Components& components = *pComponents;
				for( s32 i = components.GetCount()-1; i >= 0; i-- )
				{
					if( components[i].pComponentInfo->GetIsClass<CWeaponComponentClipInfo>() )
					{
						return static_cast<const CWeaponComponentClipInfo*>( components[i].pComponentInfo )->GetClipSize();
					}
				}
			}
		}
	}

	// component query failed (or they didn't have it), so just use boring info for clip size
	if( const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(uWeaponHash) )
	{
		return pWeaponInfo->GetClipSize();
	}

	return 0;
}

void CWeaponWheel::sWeaponWheelSlot::SetFromHash( atHashWithStringNotFinal uWeaponHash, s32 iAmmoCount, const CPed* pPed /* = NULL */, s32 iClipAmmoOverride )
{
	Clear();

	m_uWeaponHash = uWeaponHash;


	// set weapon name hash
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(uWeaponHash);
	if(pWeaponInfo)
	{
		m_HumanNameHash = pWeaponInfo->GetHumanNameHash();
	}

	if( iAmmoCount == __SPECIAL_FLAG_DONT_DISPLAY_AMMO )
	{
		m_iClipAmmo = m_iClipSize = m_iAmmo = __SPECIAL_FLAG_DONT_DISPLAY_AMMO;
	}
	else if( iAmmoCount == __SPECIAL_FLAG_USING_INFINITE_AMMO )
	{
		m_iClipAmmo = m_iClipSize = m_iAmmo = __SPECIAL_FLAG_USING_INFINITE_AMMO;
	}
	else
	{
		// check if the ped has an extended clip on 'im
		m_iClipSize = CWeaponWheel::GetClipSizeFromComponent(pPed, uWeaponHash);
		m_iAmmo = iAmmoCount;

		// update based on equipped weapon
		if(iClipAmmoOverride != __SPECIAL_FLAG_DONT_DISPLAY_AMMO)
			m_iClipAmmo = iClipAmmoOverride;
		else
			m_iClipAmmo = Min(iAmmoCount, m_iClipSize);

		Validate();
	}

	const CAmmoInfo* pAmmoInfo = pWeaponInfo->GetAmmoInfo(pPed);
	m_iAmmoSpecialType = pAmmoInfo ? (s32)pAmmoInfo->GetAmmoSpecialType() : 0;

}


void CWeaponWheel::sWeaponWheelSlot::Validate()
{
	// we have some sort of ammo:
	if (m_iClipSize > 1 && m_iClipSize < 1000)  // yes this weapon uses a clip
	{
		m_iAmmo -= m_iClipAmmo;
	}

	// ensure nothing negative
	m_iAmmo		= rage::Max(m_iAmmo,	 0);
	m_iClipAmmo	= rage::Max(m_iClipAmmo, 0);
}

s32 CWeaponWheel::GetHolsterAmmoInClip()
{
	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
	if( m_uWeaponInHandHash.IsNotNull() && pPlayerPed && pPlayerPed->GetInventory() )
	{
		s32 bullets = pPlayerPed->GetInventory()->GetWeaponAmmo(m_uWeaponInHandHash);
		s32 iClipSize = GetClipSizeFromComponent(pPlayerPed, m_uWeaponInHandHash);

		if( ((m_HolsterAmmoInClip > bullets) && (bullets != CWeapon::INFINITE_AMMO)) || m_HolsterAmmoInClip > iClipSize )
		{
			uiDisplayf("HOLSTERED AMMO WAS WRONG. Was %i, but ammo is %i, clip size is %i", m_HolsterAmmoInClip, bullets, iClipSize);
			m_HolsterAmmoInClip = Min(m_HolsterAmmoInClip, Min(bullets, iClipSize));
		}
	}

	return m_HolsterAmmoInClip;
}

void CWeaponWheel::SetHolsterAmmoInClip(s32 sAmmo)
{
	m_HolsterAmmoInClip = sAmmo;
	uiDebugf1("HOLSTERED AMMO SET AS: %i", sAmmo);
}

void CWeaponWheel::SetWeaponInHand(atHashWithStringNotFinal uWeaponInHand)
{
	m_uWeaponInHandHash = uWeaponInHand;
	uiDebugf1("HOLSTERED WEAPON SET AS: %s (0x%08x)", m_uWeaponInHandHash.TryGetCStr(), m_uWeaponInHandHash.GetHash());
}


void CWeaponWheel::Hide( bool bCatchReturnValue )
{
	if (m_bVisible.SetState(false))
	{
		// remove the wheel:
		if (CHudTools::BeginHudScaleformMethod(NEW_HUD_WEAPON_WHEEL, "UNLOAD_WEAPON_WHEEL"))
		{
			CScaleformMgr::EndMethod();
		}

		// remove wheel stats
		if (CHudTools::BeginHudScaleformMethod(NEW_HUD_WEAPON_WHEEL_STATS, "UNLOAD_WEAPON_WHEEL_STATS"))
		{
			CScaleformMgr::EndMethod();
		}

		if (CScaleformMgr::BeginMethod(CNewHud::GetContainerMovieId(), SF_BASE_CLASS_HUD, "SET_WEAPON_WHEEL_ACTIVE"))
		{
			CScaleformMgr::AddParamInt(0);
			CScaleformMgr::EndMethod();
		}

		// weapon wheel is to be turned off, so get the current weapon selected in the wheel
		if( bCatchReturnValue 
			&& (!m_bForcedOn || m_bForceResult)
			&& !m_bSuppressWheelResultDueToQuickTap
			&& !m_bSuppressWheelResults
			&& CScaleformMgr::BeginMethod(CNewHud::GetContainerMovieId(), SF_BASE_CLASS_HUD, "GET_CURRENT_WEAPON_WHEEL_HASH"))
		{
			m_iHashReturnedId = CScaleformMgr::EndMethodReturnValue(m_iHashReturnedId);
			m_iFramesToWaitForAValidReturnedId = 5;// was originally FRAMES_FOR_RETURNED_VALUE_TO_STAY_VALID (20), but that's pretty high;
		}
		else
		{
			m_iHashReturnedId = 0;
			m_iFramesToWaitForAValidReturnedId = 0;
		}

		m_bToggleSpecialVehicleAbility = false;
		m_bSuppressWheelResultDueToQuickTap = false;
		m_bForcedOn = false;
		m_bForceResult = false;
	}
	else if(!bCatchReturnValue)
	{
		m_iHashReturnedId = 0;
		m_iFramesToWaitForAValidReturnedId = 0;
		m_bForceResult = false;
	}
}


bool CWeaponWheel::SelectRadialSegment(int iDirection)
{
#if USE_DIRECT_SELECTION
	m_eDirectSelectionSlot = static_cast<eWeaponWheelSlot>((m_eLastSlotPointedAt+iDirection+MAX_WHEEL_SLOTS) % MAX_WHEEL_SLOTS);
	atRangeArray<sWeaponWheelSlot, MAX_WHEEL_WEAPONS_IN_SLOT>& curWeaponTypeSlot = m_Slot[m_eDirectSelectionSlot];
	for (int i = 0; i < MAX_WHEEL_SLOTS-1; i++)
	{
		// If the selected weapon is fine, go ahead and use it instantly
		if (!curWeaponTypeSlot[ 0 ].IsEmpty() &&
			curWeaponTypeSlot[ 0 ].IsSelectable() )
		{
			return true;
		}
		// otherwise, keep searching for a valid weapon

		m_eDirectSelectionSlot = static_cast<eWeaponWheelSlot>((m_eDirectSelectionSlot+iDirection+MAX_WHEEL_SLOTS) % MAX_WHEEL_SLOTS);
		curWeaponTypeSlot = m_Slot[m_eDirectSelectionSlot];
	}

	// if we fell out of the loop, just default to whatever we came in with
	m_eDirectSelectionSlot = m_eLastSlotPointedAt;
	return false;
#else
	(void)iDirection;
	return false;
#endif
}


bool CWeaponWheel::IsForcedOpenByTimer()
{
	return m_iForceWheelShownTime.IsStarted() && !m_iForceWheelShownTime.IsComplete( PC_FORCE_WHEEL_TIME ) && (m_bSwitchWeaponInSniperScope || !camInterface::GetGameplayDirector().GetFirstPersonPedAimCamera());
}



#if __BANK

void CWeaponWheel::CreateBankWidgets(bkBank* pOwningBank)
{
	bkGroup* pGroup = pOwningBank->AddGroup("Weapon Wheel");
	CSelectionWheel::CreateBankWidgets(pGroup);
	pGroup->AddSeparator();
	pGroup->AddToggle("IsActive", &m_bIsActive);
	pGroup->AddToggle("Visible", &m_bVisible.GetStateRef());
	pGroup->AddToggle("CarMode", &m_bCarMode.GetStateRef());
	pGroup->AddToggle("Script Forced On", &m_bScriptForcedOn);
	pGroup->AddToggle("Invert Button Input", &m_bDbgInvertButton);
	pGroup->AddButton("Force Script On", datCallback(MFA1(CWeaponWheel::SetForcedByScript), this, CallbackData(true)));
	pGroup->AddButton("Force Script Off", datCallback(MFA1(CWeaponWheel::SetForcedByScript), this, CallbackData(false)));
	pGroup->AddToggle("Received Fade Complete", &m_bReceivedFadeComplete);
	pGroup->AddToggle("Has a slot with multiple weapons", &m_bHasASlotWithMultipleWeapons);
	pGroup->AddSlider("Emergency timeout", &m_iFailsafeTimer, -50, 100,1);
	pGroup->AddToggle("Queued Off", &m_bQueueingForceOff);
	pGroup->AddToggle("Allow Weapon DPad Selection", &m_bAllowWeaponDPadSelection);
	pGroup->AddToggle("Suppress Wheel Until Button Released", &m_bSuppressWheelUntilButtonReleased);
	pGroup->AddToggle("Suppress Result Due to Quick Tap", &m_bSuppressWheelResultDueToQuickTap);
	pGroup->AddToggle("Suppress Wheel Results entirely", &m_bSuppressWheelResults);
	pGroup->AddSlider("Weapon Count", &m_iWeaponCount.GetStateRef(), 0, 60, 1);
	pGroup->AddSlider("Ammo Count", &m_iAmmoCount.GetStateRef(), 0, MAX_INT32, 1);
	pGroup->AddText("Last Highlighted Weapon Hash", &m_uLastHighlightedWeaponWheelHash);
	pGroup->AddText("Last Equipped Weapon Hash", &m_uLastEquippedWeaponWheelHash);

	bkGroup* pMemory = pGroup->AddGroup("Memory");
	{
		m_iWeaponComboIndex = 0;
		char tempData[32];
		CWeaponInfoManager::StringList& weaponNames = CWeaponInfoManager::GetNames(CWeaponInfoBlob::IT_Weapon);

		pMemory->AddButton("Clear", datCallback(MFA(sWeaponWheelMemory::Clear), &m_WheelMemory));
		pMemory->AddText("Selected Hash", &m_WheelMemory.m_uSelectedHash);
		pMemory->AddCombo("Weapon To Add", &m_iWeaponComboIndex, weaponNames.GetCount(), &weaponNames[0]);
		pMemory->AddButton("Set Weapon In Memory", datCallback(MFA(CWeaponWheel::DebugSetMemory), this));
		
		for( u8 weapon_loop = 0; weapon_loop < MAX_WHEEL_SLOTS; weapon_loop++ )
		{
			formatf(tempData, 32, "Weapon Memory #%i", weapon_loop);
			pMemory->AddText(tempData, &m_WheelMemory.m_WeaponHashSlots[weapon_loop]);
		}

		for(int i=0; i < m_iIndexOfWeaponToShowInitiallyForThisSlot.GetMaxCount(); i+=4)
		{
			formatf(tempData, "Selected %i-%i", i, i+3);
			pMemory->AddText(tempData, (int*)&m_iIndexOfWeaponToShowInitiallyForThisSlot[i]);
		}
	}

	pGroup->AddTitle("TUNE");
	pGroup->AddSlider("PC Force Show Time", &PC_FORCE_WHEEL_TIME, 0, 3000, 50);
	pGroup->AddSlider("Crossfade time", &CROSSFADE_TIME, 0.0f,5.0f,0.01f);
	pGroup->AddSlider("Crossfade alpha", &CROSSFADE_ALPHA, 0,100,5);
	
	bkGroup* p3D = pGroup->AddGroup("3D Settings");
	{
		p3D->AddTitle("General");
		p3D->AddSlider("Perspective FOV Angle", &m_perspFOV, 0, 180, .1f, datCallback(MFA(CWeaponWheel::UpdateScaleform3DSettings), this));
		p3D->AddSlider("X", &m_x, -200, 550, 1.0f, datCallback(MFA(CWeaponWheel::UpdateScaleform3DSettings), this));
		p3D->AddSlider("Y", &m_y, -200, 550, 1.0f, datCallback(MFA(CWeaponWheel::UpdateScaleform3DSettings), this));
		p3D->AddSlider("Z", &m_z, -200, 550, 1.0f, datCallback(MFA(CWeaponWheel::UpdateScaleform3DSettings), this));
		p3D->AddSlider("_xrotation", &m_xRotation, -360, 360, 1.f, datCallback(MFA(CWeaponWheel::UpdateScaleform3DSettings), this));
		p3D->AddSlider("_yrotation", &m_yRotation, -360, 360, 1.f, datCallback(MFA(CWeaponWheel::UpdateScaleform3DSettings), this));
		p3D->AddSlider("3D BG Z", &m_bgZ, -1000, 1000, .1f, datCallback(MFA(CWeaponWheel::UpdateScaleform3DSettings), this));
		p3D->AddToggle("3D BG Visibility", &m_bBGVisibility, datCallback(MFA(CWeaponWheel::UpdateScaleform3DSettings), this));

		p3D->AddTitle("3D Animation Settings");
		p3D->AddSlider("Maximum Rotation", &m_maxRotation, 1, 100, 1.f, datCallback(MFA(CWeaponWheel::UpdateScaleform3DSettings), this));
		p3D->AddSlider("Tween Duration", &m_tweenDuration, .1f, 1, .1f, datCallback(MFA(CWeaponWheel::UpdateScaleform3DSettings), this));
		p3D->AddToggle("Use Finite Rotation", &m_bFiniteRotation, datCallback(MFA(CWeaponWheel::UpdateScaleform3DSettings), this));
	}

#if RSG_PC
	pGroup->AddSeparator();
	p3D->AddSlider("Weapon/Radio Wheel 3D Stereo Depth Scalar", &m_StereoDepthScalar, 0, 1.0f, 0.01f, datCallback(MFA(CWeaponWheel::UpdateStereoDepthScalar), this));
#endif
}

void CWeaponWheel::UpdateScaleform3DSettings()
{
	if (CHudTools::BeginHudScaleformMethod(NEW_HUD_WEAPON_WHEEL, "UPDATE_DEBUG_3D_SETTINGS"))
	{
		CScaleformMgr::AddParamFloat(m_perspFOV);
		CScaleformMgr::AddParamFloat(m_x);
		CScaleformMgr::AddParamFloat(m_y);
		CScaleformMgr::AddParamFloat(m_z);
		CScaleformMgr::AddParamFloat(m_xRotation);
		CScaleformMgr::AddParamFloat(m_yRotation);
		CScaleformMgr::AddParamFloat(m_maxRotation);
		CScaleformMgr::AddParamFloat(m_tweenDuration);
		CScaleformMgr::AddParamFloat(m_bgZ);
		CScaleformMgr::AddParamFloat(m_bBGVisibility);
		CScaleformMgr::AddParamFloat(m_bFiniteRotation);
		CScaleformMgr::EndMethod();
	}
}

#if RSG_PC
void CWeaponWheel::UpdateStereoDepthScalar()
{
	CShaderLib::SetStereoParams1(Vector4(m_StereoDepthScalar,1,1,1));
}
#endif

void CWeaponWheel::DebugSetMemory()
{
	CWeaponInfoManager::StringList& weaponNames = CWeaponInfoManager::GetNames(CWeaponInfoBlob::IT_Weapon);

	SetWeaponInMemory( weaponNames[m_iWeaponComboIndex] );
}
#endif



