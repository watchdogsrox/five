// File header
#include "Weapons/Inventory/PedWeaponSelector.h"

// Game headers
#include "Control/Garages.h"
#include "Frontend/NewHud.h"
#include "modelinfo/VehicleModelInfoFlags.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PlayerInfo.h"
#include "Task/Motion/TaskMotionBase.h"
#include "Task/Vehicle/TaskVehicleWeapon.h"
#include "Task/Weapons/TaskSwapWeapon.h"
#include "vehicleai/task/taskvehicleanimation.h"
#include "System/ControlMgr.h"
#include "Vehicles/VehicleGadgets.h"
#include "Vehicles/Metadata/VehicleLayoutInfo.h"
#include "Vehicles/Metadata/VehicleMetadataManager.h"
#include "Vehicles/Metadata/VehicleSeatInfo.h"
#include "vehicles/Submarine.h"

// Macro to disable optimisations if set
WEAPON_OPTIMISATIONS()

//! DMKH.
static u32 s_CachedLocalPlayerVehicleWeaponHash = 0; 

//////////////////////////////////////////////////////////////////////////
// CPedWeaponSelector
//////////////////////////////////////////////////////////////////////////

// Statics
bank_s32 CPedWeaponSelector::RESET_TIME = 5000;
u32 CPedWeaponSelector::ms_uConsiderSwitchPeriodMS = 2000;

CPedWeaponSelector::CPedWeaponSelector()
: m_pSelectedWeaponInfo(NULL)
, m_iSelectedVehicleWeaponIndex(-1)
, m_uWeaponSelectTime(0)
, m_uNextConsiderSwitchTimeMS(0)
{
#if KEYBOARD_MOUSE_SUPPORT
	for(s32 i = 0; i < MAX_WHEEL_SLOTS; ++i)
	{
		m_iWeaponCatagoryIndex[i] = 0;
	}
	m_eLastSelectedType = MAX_WHEEL_SLOTS; // i.e. invalid.
#endif // KEYBOARD_MOUSE_SUPPORT
}

void CPedWeaponSelector::Process(CPed* pPed)
{
	weaponFatalAssertf(pPed, "pPed is NULL");
	weaponAssert(pPed->GetWeaponManager());
	//weaponAssert(pPed->IsLocalPlayer());

	if(m_uSelectedWeaponHash.GetHash() == 0 && m_iSelectedVehicleWeaponIndex == -1)
	{
		// We should always have something selected
		SelectWeapon(pPed, SET_BEST);
		pPed->GetWeaponManager()->EquipWeapon(m_uSelectedWeaponHash, m_iSelectedVehicleWeaponIndex);
	}
	// if ped should consider weapon switching
	else if( HasCheckForWeaponSwitchTimeElapsed() )
	{
		// if ped should consider weapon switching for RPG
		if( ShouldPedConsiderWeaponSwitch_RPG(pPed) )
		{
			// Check if current weapon is RPG
			if( m_uSelectedWeaponHash == WEAPONTYPE_RPG )
			{
				// Check if the target is not a ped in a vehicle
				if( !IsCurrentTargetVehicleOrPedInVehicle(pPed) )
				{
					// Select another weapon if available
					if( SelectWeapon(pPed, SET_NEXT) )
					{
						pPed->GetWeaponManager()->EquipWeapon(m_uSelectedWeaponHash, m_iSelectedVehicleWeaponIndex);
					}
				}
			}
			else // current weapon is not an RPG
			{
				// Check if the target is a ped in a vehicle
				if( IsCurrentTargetVehicleOrPedInVehicle(pPed) )
				{
					// Select RPG if available
					const bool bFromHUD = false;
					if( SelectWeapon(pPed, SET_SPECIFIC_ITEM, bFromHUD, WEAPONTYPE_RPG) )
					{
						pPed->GetWeaponManager()->EquipWeapon(m_uSelectedWeaponHash, m_iSelectedVehicleWeaponIndex);
					}
				}
			}
		}
	}

	if(!pPed->GetPedResetFlag(CPED_RESET_FLAG_TemporarilyBlockWeaponEquipping) && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_BlockWeaponSwitching) && GetIsNewWeaponSelected(pPed))
	{
// 		static dev_bool FORCE_NETWORK_SELECTION = false;
// 		if((NetworkInterface::IsGameInProgress() || FORCE_NETWORK_SELECTION))
// 		{
// 			pPed->GetWeaponManager()->EquipWeapon(m_uSelectedWeaponHash, m_iSelectedVehicleWeaponIndex, true);
// 		}
// 		else
		{
			static dev_u32 AUTO_SELECT_TIME_NO_WHEEL = 500;
			if(CNewHud::IsUsingWeaponWheel() || (m_uWeaponSelectTime + AUTO_SELECT_TIME_NO_WHEEL <= fwTimer::GetTimeInMilliseconds()))
			{
				pPed->GetWeaponManager()->EquipWeapon(m_uSelectedWeaponHash, m_iSelectedVehicleWeaponIndex, false);
			}
		}
	}
}

bool CPedWeaponSelector::SelectWeapon(CPed* pPed, ePedInventorySelectOption option, const bool bFromHud, u32 uWeaponNameHash, s32 iVehicleWeaponIndex, bool bIgnoreAmmoCheck)
{
	weaponFatalAssertf(pPed, "pPed is NULL");

	if (!pPed)
		return false;

	if(uWeaponNameHash == GADGETTYPE_PARACHUTE.GetHash())
	{
#if !__FINAL
		scrThread::PrePrintStackTrace();
		Assertf(0, "The ped has selected a parachute to equip - this is invalid!");
#endif
		return false;
	}

	u32 uNewPedSelection = 0;
	s32 iNewVehicleSelection = -1;
	u32 uVehicleWeaponHash = 0;

	// Cache the slot lists
	const CWeaponInfoBlob::SlotList& slotListNavigate = CWeaponInfoManager::GetSlotListNavigate(pPed);
	const CWeaponInfoBlob::SlotList& slotListBest = CWeaponInfoManager::GetSlotListBest();

	weaponAssert(pPed->GetInventory());

	if (!pPed->GetInventory())
		return false;

	switch(option)
	{
	case SET_SPECIFIC_ITEM:
		{
			if(uWeaponNameHash != 0)
			{
				const CWeaponItem* pItem = pPed->GetInventory()->GetWeapon(uWeaponNameHash);
				if(pItem && pItem->GetInfo())
				{
					if(GetIsWeaponValid(pPed, pItem->GetInfo(), false, bIgnoreAmmoCheck))
					{
						uNewPedSelection = pItem->GetInfo()->GetHash();
					}
				}
			}
			else if(iVehicleWeaponIndex != -1)
			{
				//Check that specific vehicle weapon is valid
				const CVehicleWeaponMgr* pVehicleWeaponMgr = pPed->GetVehiclePedInside() ? pPed->GetVehiclePedInside()->GetVehicleWeaponMgr() : NULL;
				const CVehicleWeapon *pItem = pVehicleWeaponMgr && iVehicleWeaponIndex < pVehicleWeaponMgr->GetNumVehicleWeapons() ? pVehicleWeaponMgr->GetVehicleWeapon(iVehicleWeaponIndex) : NULL;
				if (pItem && pItem->GetWeaponInfo())
				{
					if(GetIsWeaponValid(pPed, pItem->GetWeaponInfo(), true, bIgnoreAmmoCheck))
					{
						iNewVehicleSelection = iVehicleWeaponIndex;
					}
				}
				else
				{
					//B*1753594: If we don't have a weapon info (ie firetruck water cannon), just use the index passed in
					iNewVehicleSelection = iVehicleWeaponIndex;
				}
			}
		}
		break;

	case SET_SPECIFIC_SLOT:
		{
			const CWeaponItem* pItem = pPed->GetInventory()->GetWeaponBySlot(uWeaponNameHash);
			if(pItem && pItem->GetInfo())
			{
				if(GetIsWeaponValid(pPed, pItem->GetInfo(), false, bIgnoreAmmoCheck))
				{
					uNewPedSelection = pItem->GetInfo()->GetHash();
				}
			}
		}
		break;

	case SET_NEXT:
	case SET_NEXT_CATEGORY:
		{
			// If the ped manually switches weapons 
			if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_KeepWeaponHolsteredUnlessFired))
			{
				// Make the weapon visible again
				pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_KeepWeaponHolsteredUnlessFired, false);
				return false;
			}

			bool bUseVehWeaponsOnly = pPed->GetPedResetFlag(CPED_RESET_FLAG_OnlySelectVehicleWeapons);

			if(m_uSelectedWeaponHash != 0 && !bUseVehWeaponsOnly)
			{
				const CItemInfo* pInfo = CWeaponInfoManager::GetInfo(m_uSelectedWeaponHash);
				if(pInfo)
				{
					s32 iStartingIndex = GetSlotIndex(pPed, pInfo->GetSlot(), slotListNavigate);

					if (option == SET_NEXT_CATEGORY)
					{
#if KEYBOARD_MOUSE_SUPPORT
						uNewPedSelection = GetNextWeaponFromMemory();
#endif // KEYBOARD_MOUSE_SUPPORT
					}
					else
					{
						iStartingIndex++;
						uNewPedSelection = GetNextPedWeapon(pPed, iStartingIndex, slotListNavigate, bIgnoreAmmoCheck);
					}

					if(uNewPedSelection == 0)
					{
						// Try Vehicle Weapons
						iNewVehicleSelection = GetNextVehicleWeapon(pPed, 0, uVehicleWeaponHash);
						if(iNewVehicleSelection == -1)
						{
							// Try looping ped weapons
							uNewPedSelection = GetNextPedWeapon(pPed, 0, slotListNavigate, bIgnoreAmmoCheck);
						}
					}
				}
			}
			else if(m_iSelectedVehicleWeaponIndex != -1)
			{
				s32 iStartingIndex = m_iSelectedVehicleWeaponIndex;
				iStartingIndex++;
				
				const CWeaponInfo* pWeaponInfo = GetSelectedVehicleWeaponInfo(pPed);
				if( pPed->IsLocalPlayer() )
				{
					if (pPed->GetPlayerInfo()->GetTargeting().GetVehicleHomingForceDisabled())
					{
						pPed->GetPlayerInfo()->GetTargeting().SetVehicleHomingEnabled(false);
					}
					else
					{
						if (pWeaponInfo && pWeaponInfo->GetIsHoming())
						{
							// Don't allow toggling to non-homing if the weapon itself is already invalid for selection.
							if (pWeaponInfo->GetCanHomingToggle() && pPed->GetPlayerInfo()->GetTargeting().GetVehicleHomingEnabled() && GetIsWeaponValid(pPed, pWeaponInfo, true, true))
							{
								// Homing weapon can be toggled, set homing to FALSE and re-equip same vehicle weapon
								pPed->GetPlayerInfo()->GetTargeting().SetVehicleHomingEnabled(false);

								// UNLIKE FOR PREVIOUS, we want the same weapon to appear in HERE.
								iNewVehicleSelection = m_iSelectedVehicleWeaponIndex;
							}
							else
							{
								// Toggle was already off or weapon can't be toggled, set homing back to TRUE
								pPed->GetPlayerInfo()->GetTargeting().SetVehicleHomingEnabled(true);
							}
						}
						else
						{
							pPed->GetPlayerInfo()->GetTargeting().SetVehicleHomingEnabled(true); // NEXT turns this on
						}
					}
				}
				
				if (iNewVehicleSelection != m_iSelectedVehicleWeaponIndex)
				{				
					iNewVehicleSelection = GetNextVehicleWeapon(pPed, iStartingIndex, uVehicleWeaponHash);
					if(iNewVehicleSelection == -1)
					{
						if (bUseVehWeaponsOnly)
						{
							// Try looping vehicle weapons
							iNewVehicleSelection = GetNextVehicleWeapon(pPed, 0, uVehicleWeaponHash);
						}
						else
						{
							uNewPedSelection = GetNextPedWeapon(pPed, 0, slotListNavigate, bIgnoreAmmoCheck);
							if(uNewPedSelection == 0)
							{
								// Try looping vehicle weapons
								iNewVehicleSelection = GetNextVehicleWeapon(pPed, 0, uVehicleWeaponHash);
							}
						}
					}
				}
			}
		}
		break;

	case SET_PREVIOUS:
	case SET_PREVIOUS_CATEGORY:
		{
			// If the ped manually switches weapons 
			if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_KeepWeaponHolsteredUnlessFired))
			{
				// Make the weapon visible again
				pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_KeepWeaponHolsteredUnlessFired, false);
				return false;
			}

			if(m_uSelectedWeaponHash != 0)
			{
				const CItemInfo* pInfo = CWeaponInfoManager::GetInfo(m_uSelectedWeaponHash);
				if(pInfo && pInfo->GetSlot())
				{
					s32 iStartingIndex = GetSlotIndex(pPed, pInfo->GetSlot(), slotListNavigate);

					if (option == SET_PREVIOUS_CATEGORY)
					{
#if KEYBOARD_MOUSE_SUPPORT
						uNewPedSelection = GetPreviousWeaponFromMemory();
#endif // KEYBOARD_MOUSE_SUPPORT
					}
					else
					{
						iStartingIndex--;
						if (iStartingIndex > -1)
						{
							uNewPedSelection = GetPreviousPedWeapon(pPed, iStartingIndex, slotListNavigate, bIgnoreAmmoCheck);
						}
					}

					if(uNewPedSelection == 0)
					{
						iStartingIndex = 0;
						const CVehicle* pVehicle = pPed->GetVehiclePedInside();
						if(pVehicle && pVehicle->GetVehicleWeaponMgr() && pVehicle->GetSeatManager())
						{
							s32 iVehicleWeaponCount = pVehicle->GetVehicleWeaponMgr()->GetNumWeaponsForSeat(pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed));
							iStartingIndex = Clamp(iVehicleWeaponCount - 1, 0, iVehicleWeaponCount);
						}

						// Try Vehicle Weapons
						iNewVehicleSelection = GetPreviousVehicleWeapon(pPed, iStartingIndex, uVehicleWeaponHash);
						if(iNewVehicleSelection == -1)
						{
							iStartingIndex = Clamp(slotListNavigate.GetCount() - 1, 0, slotListNavigate.GetCount());

							// Try looping ped weapons
							uNewPedSelection = GetPreviousPedWeapon(pPed, iStartingIndex, slotListNavigate);
						}
					}
				}
			}
			else if(m_iSelectedVehicleWeaponIndex != -1)
			{
				s32 iStartingIndex = m_iSelectedVehicleWeaponIndex;
				iStartingIndex--;

				const CWeaponInfo* pWeaponInfo = GetSelectedVehicleWeaponInfo(pPed);
				if( pPed->IsLocalPlayer() )
				{
					if (pPed->GetPlayerInfo()->GetTargeting().GetVehicleHomingForceDisabled())
					{
						pPed->GetPlayerInfo()->GetTargeting().SetVehicleHomingEnabled(false);
					}
					else
					{
						if (pWeaponInfo && pWeaponInfo->GetIsHoming())
						{
							if (pWeaponInfo->GetCanHomingToggle() && pPed->GetPlayerInfo()->GetTargeting().GetVehicleHomingEnabled())
							{
								// Homing weapon can be toggled, set homing to FALSE and re-equip same vehicle weapon
								pPed->GetPlayerInfo()->GetTargeting().SetVehicleHomingEnabled(false);
							}
							else
							{
								// Toggle was already off or weapon can't be toggled, set homing back to TRUE
								pPed->GetPlayerInfo()->GetTargeting().SetVehicleHomingEnabled(true);

								// UNLIKE FOR NEXT, we want the same weapon to appear in HERE.
								iNewVehicleSelection = m_iSelectedVehicleWeaponIndex;
							}
						}
						else
						{
							pPed->GetPlayerInfo()->GetTargeting().SetVehicleHomingEnabled(false); // previous turns this OFF
						}
					}
				}

				// Try Vehicle Weapons
				if (iNewVehicleSelection != m_iSelectedVehicleWeaponIndex)
				{
					iNewVehicleSelection = GetPreviousVehicleWeapon(pPed, iStartingIndex, uVehicleWeaponHash);
					if(iNewVehicleSelection == -1)
					{
						iStartingIndex = Clamp(slotListNavigate.GetCount() - 1, 0, slotListNavigate.GetCount());
						uNewPedSelection = GetPreviousPedWeapon(pPed, iStartingIndex, slotListNavigate, bIgnoreAmmoCheck);
						if(uNewPedSelection == 0)
						{
							iStartingIndex = 0;
							const CVehicle* pVehicle = pPed->GetVehiclePedInside();
							if(pVehicle && pVehicle->GetVehicleWeaponMgr())
							{
								iStartingIndex = Clamp(pVehicle->GetVehicleWeaponMgr()->GetNumVehicleWeapons() - 1, 0, pVehicle->GetVehicleWeaponMgr()->GetNumVehicleWeapons());
							}
							// Try looping vehicle weapons
							iNewVehicleSelection = GetPreviousVehicleWeapon(pPed, iStartingIndex, uVehicleWeaponHash);
						}
					}
				}
			}
		}
		break;

	case SET_BEST:
		{
			const CVehicle* pVehicle = pPed->GetVehiclePedInside();
			if(pVehicle)
			{
				iNewVehicleSelection = GetBestVehicleWeapon(pPed);
			}

			if(iNewVehicleSelection == -1)
			{
				uNewPedSelection = GetBestPedWeapon(pPed, slotListBest, false);
			}
		}
		break;
		
	case SET_BEST_MELEE:
		{
			uNewPedSelection = GetBestPedMeleeWeapon(pPed, slotListBest);
		}
		break;

	case SET_BEST_NONLETHAL:
		{
			uNewPedSelection = GetBestPedNonLethalWeapon(pPed, slotListBest);
		}
		break;

#if KEYBOARD_MOUSE_SUPPORT
	case SET_NEXT_IN_GROUP:
		{
			// If the ped manually switches weapons 
			if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_KeepWeaponHolsteredUnlessFired))
			{
				// Make the weapon visible again
				pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_KeepWeaponHolsteredUnlessFired, false);
				return false;
			}

			if(m_uSelectedWeaponHash != 0)
			{
				eWeaponWheelSlot group = static_cast<eWeaponWheelSlot>(uWeaponNameHash);

				// only select the next weapon of the user has selected this group again.
				if(m_eLastSelectedType == group)
				{
					const CItemInfo* pInfo = CWeaponInfoManager::GetInfo(m_uSelectedWeaponHash);
					if (pInfo)
					{
						int count = 0;
						int index = m_iWeaponCatagoryIndex[group];
						u32 selectedWeaponSlotHash = pInfo->GetSlot();
						u32 currentSlotHash = slotListNavigate[m_iWeaponCatagoryIndex[group]].GetHash();

						// Find the matching weapon slot for the current weapon.
						while(currentSlotHash != selectedWeaponSlotHash && count < slotListNavigate.GetCount())
						{
							index++;
							if (index >= slotListNavigate.GetCount())
							{
								index = 0;
							}
							count++;
							currentSlotHash = slotListNavigate[index].GetHash();
						}

						if (count < slotListNavigate.GetCount())
						{
							// Increment from current weapon slot to search for next valid weapon after that one.
							m_iWeaponCatagoryIndex[group] = index+1;
						}
						else
						{
							m_iWeaponCatagoryIndex[group]++;
						}
					}
					else
					{
						m_iWeaponCatagoryIndex[group]++;
					}
				}
				m_eLastSelectedType = group;

				uNewPedSelection = GetNextPedWeaponInGroup(pPed, group, slotListNavigate);
				if(uNewPedSelection == 0)
				{
					// Try looping ped weapons
					m_iWeaponCatagoryIndex[group] = 0;
					uNewPedSelection = GetNextPedWeaponInGroup(pPed, group, slotListNavigate);
				}
			}
		}
		break;
#endif // KEYBOARD_MOUSE_SUPPORT

	default:
		weaponErrorf("Unhandled Option [%d] in CPedWeaponSelector::SelectWeapon", option);
		break;
	}

	// Debug log
	//weaponDebugf2("Selecting Weapon Ped[%d] Vehicle[%d]", uNewPedSelection, iNewVehicleSelection);

	// Select the weapon
	if((uNewPedSelection != 0 && m_uSelectedWeaponHash != uNewPedSelection) || (iNewVehicleSelection != -1 && m_iSelectedVehicleWeaponIndex != iNewVehicleSelection))
	{
		SetSelectedWeapon(pPed, uNewPedSelection);
		m_iSelectedVehicleWeaponIndex = iNewVehicleSelection;

		if(pPed->IsLocalPlayer() && NetworkInterface::IsGameInProgress() && uVehicleWeaponHash > 0)
		{
			const CWeaponInfo *pVehicleWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(uVehicleWeaponHash);
			if(pVehicleWeaponInfo && pVehicleWeaponInfo->GetIsVehicleWeapon())
			{
				s_CachedLocalPlayerVehicleWeaponHash = uVehicleWeaponHash;
			}
		}

		m_uWeaponSelectTime = fwTimer::GetTimeInMilliseconds();

		if(bFromHud && (!m_pSelectedWeaponInfo || !m_pSelectedWeaponInfo->GetIsVehicleWeapon()))
		{
			if (pPed->GetWeaponManager())
				pPed->GetWeaponManager()->SetLastEquipOrFireTime(fwTimer::GetTimeInMilliseconds());
		}

		if(pPed->GetIsInVehicle())
		{
			// A bit dodgy, but we have to automatically equip the selected weapon for drivebys
			if (pPed->GetWeaponManager())
				pPed->GetWeaponManager()->EquipWeapon(m_uSelectedWeaponHash, m_iSelectedVehicleWeaponIndex);
		}
	}
	else
	{
		// Clear flag if we've tried to select a new weapon
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_KeepWeaponHolsteredUnlessFired, false);
	}

	return uNewPedSelection != 0 || iNewVehicleSelection != -1;
}

const CWeaponInfo* CPedWeaponSelector::GetSelectedVehicleWeaponInfo(const CPed* pPed) const
{
	weaponFatalAssertf(pPed, "pPed is NULL");

	if (m_iSelectedVehicleWeaponIndex > -1)
	{
		const CVehicle* pVehicle = pPed->GetVehiclePedInside();
		if(pVehicle && pVehicle->GetVehicleWeaponMgr())
		{
			atArray<const CVehicleWeapon*> weapons;
			pVehicle->GetVehicleWeaponMgr()->GetWeaponsForSeat(pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed), weapons);
			if(weapons.GetCount() > 0 && m_iSelectedVehicleWeaponIndex < weapons.GetCount())
			{
				weaponFatalAssertf(weapons[m_iSelectedVehicleWeaponIndex], "NULL Vehicle Weapon Pointer");
				return weapons[m_iSelectedVehicleWeaponIndex]->GetWeaponInfo();
			}
		}
	}

	return NULL;
}

bool CPedWeaponSelector::GetIsNewWeaponSelected(CPed* pPed) const
{
	weaponAssert(pPed->GetWeaponManager());

	if((m_uSelectedWeaponHash != 0 && m_uSelectedWeaponHash != pPed->GetWeaponManager()->GetEquippedWeaponHash() && !pPed->GetWeaponManager()->GetIsGadgetEquipped(m_uSelectedWeaponHash)) ||
		(m_iSelectedVehicleWeaponIndex != -1 && m_iSelectedVehicleWeaponIndex != pPed->GetWeaponManager()->GetEquippedVehicleWeaponIndex()))
	{
		return true;
	}
	return false;
}

bool CPedWeaponSelector::GetIsNewEquippableWeaponSelected(CPed* pPed) const
{
	if(GetIsNewWeaponSelected(pPed))
	{
		const CItemInfo* pInfo = CWeaponInfoManager::GetInfo(m_uSelectedWeaponHash);
		if(pInfo)
		{
			if(pInfo->GetIsClassId(CWeaponInfo::GetStaticClassId()))
			{
				const CWeaponInfo* pWeaponInfo = static_cast<const CWeaponInfo*>(pInfo);
				return pWeaponInfo->GetIsCarriedInHand();
			}
		}
	}
	return false;
}

void CPedWeaponSelector::SetSelectedWeapon(const CPed* pPed, u32 uSelectedWeaponHash)
{
	// Store the selected unarmed weapon used for tapping weapon switch.
	if(pPed->IsLocalPlayer())
	{
		if(uSelectedWeaponHash != pPed->GetDefaultUnarmedWeaponHash())
		{
			CNewHud::SetWheelSelectedWeapon(uSelectedWeaponHash);

			if(uSelectedWeaponHash != CNewHud::GetWheelWeaponInHand())
			{
				CNewHud::SetWheelDisableReload(false);
				CNewHud::SetWheelWeaponInHand(0);
			}
		}
		else if(m_uSelectedWeaponHash != 0)
		{
			const CObject* pWeaponObj = pPed->GetWeaponManager()->GetEquippedWeaponObject();
			const CWeapon* pWeapon = pWeaponObj ? pWeaponObj->GetWeapon() : NULL;
			if(pWeapon)
			{
				CNewHud::SetWheelDisableReload(true);
				CNewHud::SetWheelWeaponInHand(m_uSelectedWeaponHash);
				if (m_uSelectedWeaponHash == ATSTRINGHASH("WEAPON_DBSHOTGUN",0xEF951FBB) && pWeapon->GetAmmoInClip() == 1)
				{
					//B*2642059 - DBShotgun will be forced to fire twice if interrupted between shots. set the ammo left in clip correctly
					CNewHud::SetWheelHolsterAmmoInClip(0);
				}
				else
				{
					CNewHud::SetWheelHolsterAmmoInClip(pWeapon->GetAmmoInClip());
				}
			}
			else
			{
				CNewHud::SetWheelDisableReload(false);
				CNewHud::SetWheelWeaponInHand(0);
			}
		}
	}

	m_uSelectedWeaponHash = uSelectedWeaponHash;
	m_pSelectedWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_uSelectedWeaponHash);
}

bool CPedWeaponSelector::GetIsPlayerSelectingNextVehicleWeapon(const CPed* pPed) const
{
	if(!CGarages::GarageCameraShouldBeOutside())
	{
		weaponAssert(pPed);
		weaponAssert(pPed->IsAPlayerPed());

		const CPed* pPlayerPed = static_cast<const CPed*>(pPed);

		// Get the control
		const CControl* pControl = pPlayerPed->GetControlFromPlayer();
		if(pControl)
		{
			TUNE_GROUP_INT(VEHICLE_MELEE, TAP_LENGTH, 250, 0, 10000, 100);
			bool bSelect = pControl->GetVehicleSelectNextWeapon().IsReleased() && pControl->GetVehicleSelectNextWeapon().HistoryPressed(TAP_LENGTH);

			if (pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsPerformingVehicleMelee))
			{
				bSelect = false;
			}

			const CVehicle* pVehicle = pPlayerPed->GetVehiclePedInside();
			if (pVehicle && MI_CAR_CHERNOBOG.IsValid() && pVehicle->GetModelIndex() == MI_CAR_CHERNOBOG && !pVehicle->GetAreOutriggersFullyDeployed() && (!pVehicle->GetDriver() || pVehicle->GetDriver() != pPlayerPed))
			{
				bSelect = false;
			}

#if RSG_ORBIS
			if(!bSelect && !CControlMgr::GetMainPlayerControl().IsInputDisabled(INPUT_SELECT_WEAPON) && !pControl->GetVehMeleeHold().IsDown())
			{
				bSelect = CControlMgr::GetPlayerPad()->GetTouchPadGesture().IsSwipe(CTouchPadGesture::RIGHT);
			}
#endif
			return bSelect;
		}
	}

	return false;
}

bool CPedWeaponSelector::GetIsPlayerSelectingPreviousVehicleWeapon(const CPed* pPed) const
{
	if(!CGarages::GarageCameraShouldBeOutside())
	{
		weaponAssert(pPed);
		weaponAssert(pPed->IsAPlayerPed());

		const CPed* pPlayerPed = static_cast<const CPed*>(pPed);

		// Get the control
		const CControl* pControl = pPlayerPed->GetControlFromPlayer();
		if(pControl)
		{
			bool bSelect = pControl->GetVehicleSelectPrevWeapon().IsReleased();

			if (pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsPerformingVehicleMelee))
			{
				bSelect = false;
			}

			const CVehicle* pVehicle = pPlayerPed->GetVehiclePedInside();
			if (pVehicle && MI_CAR_CHERNOBOG.IsValid() && pVehicle->GetModelIndex() == MI_CAR_CHERNOBOG && !pVehicle->GetAreOutriggersFullyDeployed() && (!pVehicle->GetDriver() || pVehicle->GetDriver() != pPlayerPed))
			{
				bSelect = false;
			}

#if RSG_ORBIS
			if(!bSelect && !CControlMgr::GetMainPlayerControl().IsInputDisabled(INPUT_SELECT_WEAPON) && !pControl->GetVehMeleeHold().IsDown())
			{
				bSelect = CControlMgr::GetPlayerPad()->GetTouchPadGesture().IsSwipe(CTouchPadGesture::LEFT);
			}
#endif
			return bSelect;
		}
	}

	return false;
}

bool CPedWeaponSelector::GetIsPlayerSelectingNextParachuteWeapon(const CPed* pPed) const
{
	weaponAssert(pPed);
	weaponAssert(pPed->IsAPlayerPed());

	const CPed* pPlayerPed = static_cast<const CPed*>(pPed);

	// Get the control
	const CControl* pControl = pPlayerPed->GetControlFromPlayer();
	if(pControl)
	{
		return pControl->GetVehicleSelectNextWeapon().IsPressed();
	}

	return false;
}

bool CPedWeaponSelector::GetIsPlayerSelectingPreviousParachuteWeapon(const CPed* pPed) const
{
	weaponAssert(pPed);
	weaponAssert(pPed->IsAPlayerPed());

	const CPed* pPlayerPed = static_cast<const CPed*>(pPed);

	// Get the control
	const CControl* pControl = pPlayerPed->GetControlFromPlayer();
	if(pControl)
	{
		return pControl->GetVehicleSelectPrevWeapon().IsPressed();
	}

	return false;
}

s32 CPedWeaponSelector::GetSlotIndex(const CPed* UNUSED_PARAM(pPed), u32 uSlot, const CWeaponInfoBlob::SlotList& slotList) const
{
	for(s32 i = 0; i < slotList.GetCount(); i++)
	{
		if(slotList[i].GetHash() == uSlot)
		{
			return i;
		}
	}

	return -1;
}

u32 CPedWeaponSelector::GetNextPedWeapon(const CPed* pPed, s32 iStartingIndex, const CWeaponInfoBlob::SlotList& slotList, bool bIgnoreAmmoCheck) const
{
	weaponFatalAssertf(pPed, "pPed is NULL");

	if(iStartingIndex >= 0 && iStartingIndex < slotList.GetCount())
	{
		// Set the index to the starting index
		s32 iIndex = iStartingIndex;

		while(iIndex < slotList.GetCount())
		{
			weaponAssert(pPed->GetInventory());
			const CWeaponItem* pItem = pPed->GetInventory()->GetWeaponBySlot(slotList[iIndex].GetHash());
			if(pItem)
			{
				if(GetIsWeaponValid(pPed, pItem->GetInfo(), false, bIgnoreAmmoCheck))
				{
					return pItem->GetInfo()->GetHash();
				}
			}

			// Increment the index
			iIndex++;
		}
	}

	return 0;
}

u32 CPedWeaponSelector::GetPreviousPedWeapon(const CPed* pPed, s32 iStartingIndex, const CWeaponInfoBlob::SlotList& slotList, bool bIgnoreAmmoCheck) const
{
	weaponFatalAssertf(pPed, "pPed is NULL");

	if(iStartingIndex >= 0 && iStartingIndex < slotList.GetCount())
	{
		// Set the index to the starting index
		s32 iIndex = iStartingIndex;

		while(iIndex >= 0)
		{
			weaponAssert(pPed->GetInventory());
			const CWeaponItem* pItem = pPed->GetInventory()->GetWeaponBySlot(slotList[iIndex].GetHash());
			if(pItem)
			{
				if(GetIsWeaponValid(pPed, pItem->GetInfo(), false, bIgnoreAmmoCheck))
				{
					return pItem->GetInfo()->GetHash();
				}
			}

			// Decrement the index
			iIndex--;
		}
	}

	return 0;
}

u32 CPedWeaponSelector::GetBestPedWeapon(const CPed* pPed, const CWeaponInfoBlob::SlotList& slotList, const bool bIgnoreVehicleCheck, const u32 uExcludedHash, bool bIgnoreAmmoCheck /*= false*/, bool bIgnoreWaterCheck/* = false*/, bool bIgnoreFullAmmo, bool bIgnoreProjectile) const
{
	weaponFatalAssertf(pPed, "pPed is NULL");
	weaponAssert(pPed->GetInventory());

	for(s32 i = 0; i < slotList.GetCount(); i++)
	{
		const CWeaponItem* pItem = pPed->GetInventory()->GetWeaponBySlot(slotList[i].GetHash());
		if(pItem)
		{
			if(uExcludedHash == 0 || uExcludedHash != pItem->GetInfo()->GetHash())
			{
				if(GetIsWeaponValid(pPed, pItem->GetInfo(), bIgnoreVehicleCheck, bIgnoreAmmoCheck, bIgnoreWaterCheck, bIgnoreFullAmmo, bIgnoreProjectile))
				{
 					return pItem->GetInfo()->GetHash();
				}
			}
		}
	}

	weaponAssertf(pPed->GetInventory()->GetWeapon(pPed->GetDefaultUnarmedWeaponHash()), "Ped(%s %p) doesn't have default weapon in their inventory", pPed->GetModelName(), pPed);

	return pPed->GetDefaultUnarmedWeaponHash();
}

u32 CPedWeaponSelector::GetBestPedWeaponSlot(const CPed* pPed, const CWeaponInfoBlob::SlotList& slotList, const bool bIgnoreVehicleCheck, const u32 uExcludedHash, bool bIgnoreAmmoCheck) const
{
	weaponFatalAssertf(pPed, "pPed is NULL");
	weaponAssert(pPed->GetInventory());

	for(s32 i = 0; i < slotList.GetCount(); i++)
	{
		const CWeaponItem* pItem = pPed->GetInventory()->GetWeaponBySlot(slotList[i].GetHash());
		if(pItem)
		{
			if(uExcludedHash == 0 || uExcludedHash != pItem->GetInfo()->GetHash())
			{
				if(GetIsWeaponValid(pPed, pItem->GetInfo(), bIgnoreVehicleCheck, bIgnoreAmmoCheck))
				{
					return pItem->GetInfo()->GetSlot();
				}
			}
		}
	}

	weaponAssertf(pPed->GetInventory()->GetWeapon(pPed->GetDefaultUnarmedWeaponHash()), "Ped doesn't have default weapon in their inventory");
	return atHashWithStringNotFinal("SLOT_UNARMED", 0x76D04710);
}


u32 CPedWeaponSelector::GetBestPedWeaponByGroup(const CPed* pPed, const CWeaponInfoBlob::SlotList& slotList, const u32 uGroupHash, bool bIgnoreAmmoCheck /*= false*/) const
{
	weaponFatalAssertf(pPed, "pPed is NULL");
	weaponAssert(pPed->GetInventory());

	for(s32 i = 0; i < slotList.GetCount(); i++)
	{
		const CWeaponItem* pItem = pPed->GetInventory()->GetWeaponBySlot(slotList[i].GetHash());
		if(pItem)
		{
			if(pItem->GetInfo() && pItem->GetInfo()->GetGroup() == uGroupHash)
			{
				if(GetIsWeaponValid(pPed, pItem->GetInfo(), false, bIgnoreAmmoCheck))
				{
					return pItem->GetInfo()->GetHash();
				}
			}
		}
	}

	return 0;
}

u32 CPedWeaponSelector::GetBestPedMeleeWeapon(const CPed* pPed, const CWeaponInfoBlob::SlotList& slotList) const
{
	weaponFatalAssertf(pPed, "pPed is NULL");
	weaponAssert(pPed->GetInventory());

	for(s32 i = 0; i < slotList.GetCount(); i++)
	{
		const CWeaponItem* pItem = pPed->GetInventory()->GetWeaponBySlot(slotList[i].GetHash());
		if(pItem)
		{
			if(GetIsWeaponValid(pPed, pItem->GetInfo(), false, false))
			{
				if(pItem->GetInfo()->GetIsMelee())
				{
					return pItem->GetInfo()->GetHash();
				}
			}
		}
	}

	weaponAssertf(pPed->GetInventory()->GetWeapon(pPed->GetDefaultUnarmedWeaponHash()), "Ped doesn't have default weapon in their inventory");
	return pPed->GetDefaultUnarmedWeaponHash();
}

u32 CPedWeaponSelector::GetBestPedNonLethalWeapon(const CPed* pPed, const CWeaponInfoBlob::SlotList& slotList) const
{
	weaponFatalAssertf(pPed, "pPed is NULL");
	weaponAssert(pPed->GetInventory());

	for(s32 i = 0; i < slotList.GetCount(); i++)
	{
		const CWeaponItem* pItem = pPed->GetInventory()->GetWeaponBySlot(slotList[i].GetHash());
		if(pItem)
		{
			if(GetIsWeaponValid(pPed, pItem->GetInfo(), false, false))
			{
				if(pItem->GetInfo()->GetIsNonLethal())
				{
					return pItem->GetInfo()->GetHash();
				}
			}
		}
	}

	weaponAssertf(pPed->GetInventory()->GetWeapon(pPed->GetDefaultUnarmedWeaponHash()), "Ped doesn't have default weapon in their inventory");
	return pPed->GetDefaultUnarmedWeaponHash();
}

bool CPedWeaponSelector::GetIsWeaponValid(const CPed* pPed, const CWeaponInfo* pWeaponInfo, bool bIgnoreVehicleCheck, bool bIgnoreAmmoCheck, bool bIgnoreWaterCheck, bool bIgnoreFullAmmo, bool bIgnoreProjectile) const
{
	weaponFatalAssertf(pPed, "pPed is NULL");
	weaponAssert(pPed->GetInventory());

	if(pWeaponInfo)
	{
		if(bIgnoreAmmoCheck || !pPed->GetInventory()->GetIsWeaponOutOfAmmo(pWeaponInfo))
		{
			if(!bIgnoreWaterCheck)
			{ 
				bool bUnderwater = pPed->GetPedResetFlag(CPED_RESET_FLAG_IsDrowning);
				if (!bUnderwater && pWeaponInfo->GetIsVehicleWeapon() && pPed->GetWeaponManager())
				{
					const CVehicleWeapon* pVehicleWeapon = pPed->GetWeaponManager()->GetEquippedVehicleWeapon();
					if (pVehicleWeapon)
					{
						bUnderwater = pVehicleWeapon->IsUnderwater(pPed->GetMyVehicle());
					}
				}

				if (bUnderwater && !pWeaponInfo->GetUsableUnderwater())
				{
					return false;
				}
				else if (!bUnderwater && pWeaponInfo->GetUsableUnderwaterOnly())
				{
					return false;
				}
			}

			if(pPed->GetIsInCover() && !pWeaponInfo->GetUsableInCover())
			{
				return false;
			}

			if(bIgnoreFullAmmo && pPed->GetInventory()->GetWeaponAmmo(pWeaponInfo) >= pWeaponInfo->GetAmmoMax(pPed))
			{
				return false;
			}

			if(bIgnoreProjectile && pWeaponInfo->GetIsProjectile())
			{
				return false;
			}

// 			if (pPed->GetCurrentMotionTask() && pPed->GetCurrentMotionTask()->IsOnFoot())
// 			{
// 				if(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_IsInTheAir ))
// 				{
// 					return false;
// 				}
// 				else if(!pWeaponInfo->GetUsableOnFoot())
// 				{
// 					return false;
// 				}
// 			}

			// Some types of vehicle mods (window plating on VMT_CHASSIS or VMT_WING_L, vehicle weapons on VMT_ROOF, etc.) can disable normal driveby weapons
			if (GetIsWeaponDisabledByVehicleMod(pPed, pWeaponInfo, VMT_CHASSIS))
			{
				return false;
			}
			else if (GetIsWeaponDisabledByVehicleMod(pPed, pWeaponInfo, VMT_ROOF))
			{
				return false;
			}
			else if (GetIsWeaponDisabledByVehicleMod(pPed, pWeaponInfo, VMT_WING_L))
			{
				return false;
			}
			else if (GetIsWeaponDisabledByVehicleMod(pPed, pWeaponInfo, VMT_DOOR_L))
			{
				return false;
			}

			// If we're in a vehicle in SP, skip the weapon if flagged as MP only or if melee type
			if (pPed->GetIsInVehicle() && !NetworkInterface::IsGameInProgress() && ( pWeaponInfo->GetIsDriveByMPOnly() || (pWeaponInfo->GetIsMelee() && !pWeaponInfo->GetIsUnarmed()) ))
			{
				return false;
			}

			if (pPed->GetIsInVehicle() && NetworkInterface::IsGameInProgress() && pWeaponInfo->GetCanBeUsedInVehicleMelee() && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DisableVehicleCombat))
			{
				return false;
			} 

			// If we're inside a vehicle, query the driveby info on this peds seat to see if the weaponslot type is supported as a driveby weapon
			if( !bIgnoreVehicleCheck && (pPed->GetVehiclePedInside() || pPed->GetMyMount() || pPed->GetIsParachuting() || pPed->GetPedResetFlag(CPED_RESET_FLAG_IsUsingJetpack)) && 
				(pWeaponInfo->GetNotAllowedForDriveby() || !CVehicleMetadataMgr::GetDriveByAnimInfoForWeapon(pPed, pWeaponInfo->GetHash())) )
			{
				return false;
			}	

			if(CheckForSearchLightValid(pPed, pWeaponInfo))
			{
				return false;
			}

			if(pPed->GetPedResetFlag(CPED_RESET_FLAG_DisableMeleeWeaponSelection) && pWeaponInfo->GetIsMelee())
			{
				return false;
			}

			const CPedInventory* pInventory = pPed->GetInventory();
			const CWeaponItem* pWeaponItem = pInventory ? pInventory->GetWeapon(pWeaponInfo->GetHash()) : NULL;
			if (pWeaponItem && !pWeaponItem->GetCanSelect())
			{
				return false;
			}

			return true;
		}
	}

	return false;
}

bool CPedWeaponSelector::GetIsWeaponDisabledByVehicleMod(const CPed* pPed, const CWeaponInfo* pWeaponInfo, const eVehicleModType modType) const
{
	if (pPed && pWeaponInfo)
	{
		CVehicle* pVehicle = pPed->GetMyVehicle();
		if (pVehicle && !pWeaponInfo->GetIsVehicleWeapon())
		{
			// Inside seats only (no bed/hanging seats on INSURGENT3, etc.)
			const CVehicleSeatAnimInfo* pSeatAnimInfo = CVehicleMetadataMgr::GetSeatAnimInfoFromPed(pPed);
			if (pSeatAnimInfo && !pSeatAnimInfo->GetRagdollAtExtremeVehicleOrientation())
			{
				CVehicleVariationInstance& variation = pVehicle->GetVehicleDrawHandler().GetVariationInstance();
				u8 modIndex = variation.GetModIndex(modType);
				if (modIndex != INVALID_MOD)
				{
					const CVehicleKit* kit = variation.GetKit();
					if (kit)
					{
						const CVehicleModVisible& mod = kit->GetVisibleMods()[modIndex];

						// Check filter to only disable individual seat
						s32 iPedSeat = pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
						if (mod.GetDrivebyDisabledSeat() == -1 || (mod.GetDrivebyDisabledSeat() == iPedSeat || mod.GetDrivebyDisabledSeatSecondary() == iPedSeat))
						{
							if (mod.IsDriveByDisabled())
							{
								return true;
							}
							else if (mod.IsProjectileDriveByDisabled() && (pWeaponInfo->GetIsThrownWeapon() || pWeaponInfo->GetIsUnarmed()))
							{
								return true;	
							}
						}
					}				
				}
			}
		}	
	}
	return false;
}

bool CPedWeaponSelector::CheckForSearchLightValid(const CPed* pPed, const CWeaponInfo* pWeaponInfo) const
{
	if(pPed->GetIsInVehicle() && pWeaponInfo->GetIsVehicleWeapon())
	{
		if(!pWeaponInfo->GetBoatWeaponIsNotSearchLight() && pPed->GetMyVehicle()->InheritsFromBoat())
		{
			return true;
		}

		CVehicleWeaponMgr* pWeaponMgr = pPed->GetMyVehicle()->GetVehicleWeaponMgr();
		static const atHashWithStringNotFinal searchLightWeaponName("VEHICLE_WEAPON_SEARCHLIGHT",0xCDAC517D);
		if(pWeaponInfo->GetHash() == searchLightWeaponName && (pWeaponMgr->GetNumVehicleWeapons() != 1 || (pPed->IsPlayer() && !pPed->GetPlayerInfo()->GetCanUseSearchLight())))
		{
			return true;
		}
	}
	return false;
}

bool CPedWeaponSelector::HasCheckForWeaponSwitchTimeElapsed()
{
	// Check time of next scheduled switch consideration time
	const u32 currentTimeMS = fwTimer::GetTimeInMilliseconds();
	if( currentTimeMS > m_uNextConsiderSwitchTimeMS )
	{
		// mark next consider switch time
		m_uNextConsiderSwitchTimeMS = currentTimeMS + ms_uConsiderSwitchPeriodMS;

		// enough time elapsed, proceed with checks
		return true;
	}
	
	// too soon since last check
	return false;
}

bool CPedWeaponSelector::ShouldPedConsiderWeaponSwitch_RPG(const CPed* pPed) const
{
	// If Ped is AI controlled and flagged for rocket switch
	if( pPed && !pPed->IsPlayer() && pPed->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_UseRocketsAgainstVehiclesOnly) )
	{
		// consider weapon switch for RPG
		return true;
	}

	// shouldn't consider switch by default
	return false;
}

bool CPedWeaponSelector::IsCurrentTargetVehicleOrPedInVehicle(const CPed* pPed) const
{
	// Check if there is a hostile target and it is a ped
	const CEntity* pTargetEntity = pPed->GetPedIntelligence()->GetQueriableInterface()->GetHostileTarget();
	if(!pTargetEntity)
	{
		CTaskGun* pGunTask = static_cast<CTaskGun*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_GUN));
		if(pGunTask)
		{
			pTargetEntity = pGunTask->GetTarget().GetEntity();
		}
	}

	if(pTargetEntity)
	{
		if(pTargetEntity->GetIsTypePed())
		{
			// Check if the target ped is in a vehicle
			const CPed* pTargetPed = static_cast<const CPed*>(pTargetEntity);
			if( pTargetPed && pTargetPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle) )
			{
				return true;
			}
		}
		else if(pTargetEntity->GetIsTypeVehicle())
		{
			return true;
		}
	}

	// Either target is not a ped, or not in a vehicle
	return false;
}

s32 CPedWeaponSelector::GetNextVehicleWeapon(const CPed* pPed, s32 iStartingIndex, u32 &uWeaponHash) const
{
	weaponFatalAssertf(pPed, "pPed is NULL");

	const CVehicle* pVehicle = pPed->GetVehiclePedInside();
	if(pVehicle && pVehicle->GetVehicleWeaponMgr())
	{
		atArray<const CVehicleWeapon*> weapons;
		pVehicle->GetVehicleWeaponMgr()->GetWeaponsForSeat(pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed), weapons);
		if(iStartingIndex >= 0 && iStartingIndex < weapons.GetCount())
		{
			// Set the index to the starting index
			s32 iIndex = iStartingIndex;

			while(iIndex < weapons.GetCount())
			{
				if(weapons[iIndex] && weapons[iIndex]->GetIsEnabled() && GetIsWeaponValid(pPed,weapons[iIndex]->GetWeaponInfo(),true,true))
				{
					uWeaponHash = weapons[iIndex]->GetWeaponInfo() ? weapons[iIndex]->GetWeaponInfo()->GetHash() : 0;
					return iIndex;
				}

				// Increment the index
				iIndex++;
			}
		}
	}

	return -1;
}

s32 CPedWeaponSelector::GetPreviousVehicleWeapon(const CPed* pPed, s32 iStartingIndex, u32 &uWeaponHash) const
{
	weaponFatalAssertf(pPed, "pPed is NULL");

	const CVehicle* pVehicle = pPed->GetVehiclePedInside();
	if(pVehicle && pVehicle->GetVehicleWeaponMgr())
	{
		atArray<const CVehicleWeapon*> weapons;
		pVehicle->GetVehicleWeaponMgr()->GetWeaponsForSeat(pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed), weapons);
		if(iStartingIndex >= 0 && iStartingIndex < weapons.GetCount())
		{
			// Set the index to the starting index
			s32 iIndex = iStartingIndex;

			while(iIndex >= 0)
			{
				if(weapons[iIndex] && weapons[iIndex]->GetIsEnabled() && GetIsWeaponValid(pPed,weapons[iIndex]->GetWeaponInfo(),true,true))
				{
					uWeaponHash = weapons[iIndex]->GetWeaponInfo() ? weapons[iIndex]->GetWeaponInfo()->GetHash() : 0;
					return iIndex;
				}

				// Decrement the index
				iIndex--;
			}
		}
	}

	return -1;
}

s32 CPedWeaponSelector::GetBestVehicleWeapon(const CPed* pPed) const
{
	weaponFatalAssertf(pPed, "pPed is NULL");

	s32 nBestWeapon = -1;
	const CVehicle* pVehicle = pPed->GetVehiclePedInside();
	if(pVehicle && pVehicle->GetVehicleWeaponMgr())
	{
		atArray<const CVehicleWeapon*> weapons;
		s32 iSeatIndex = pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
		pVehicle->GetVehicleWeaponMgr()->GetWeaponsForSeat(iSeatIndex, weapons);
		if(weapons.GetCount() > 0)
		{
			// HACK B*3207896: If the ROCKET weapon is valid on the RUINER2, skip the BULLET weapon and select that instead. We can't switch weapon slots for various reasons.
			if (MI_CAR_RUINER2.IsValid() && pVehicle->GetModelIndex() == MI_CAR_RUINER2)
			{
				const s32 iRuinerRocketIndex = 1;

				const CWeaponInfo* pWeaponInfo = weapons[iRuinerRocketIndex]->GetWeaponInfo();
				if ((pWeaponInfo && GetIsWeaponValid(pPed, pWeaponInfo, true, true)) || (!pWeaponInfo))
				{
					if(pPed->IsLocalPlayer() && NetworkInterface::IsGameInProgress() && (s_CachedLocalPlayerVehicleWeaponHash > 0))
					{
						bool bMatchesSavedWeaponInfo = pWeaponInfo && (s_CachedLocalPlayerVehicleWeaponHash == pWeaponInfo->GetHash());
						if(bMatchesSavedWeaponInfo)
						{
							return iRuinerRocketIndex;
						}
					}
					else
					{
						return iRuinerRocketIndex;
					}
				}
			}

			// Don't automatically select the POUNDER2's driver rocket barrage when entering
			if (MI_CAR_POUNDER2.IsValid() && pVehicle->GetModelIndex() == MI_CAR_POUNDER2 && iSeatIndex == 0)
			{
				const CWeaponInfo* pWeaponInfo = weapons[0]->GetWeaponInfo();
				if (pWeaponInfo && pWeaponInfo->GetIsTurret())
				{
					return -1;
				}
			}

			//Loop over all vehicle weapons until we find a valid one
			for (int i = 0; i < weapons.GetCount(); i++)
			{
				// Ensure weapon is valid
				const CWeaponInfo* pWeaponInfo = weapons[i]->GetWeaponInfo();
				if ((pWeaponInfo && GetIsWeaponValid(pPed, pWeaponInfo, true, true)) || (!pWeaponInfo))
				{
					if(pPed->IsLocalPlayer() && NetworkInterface::IsGameInProgress() && (s_CachedLocalPlayerVehicleWeaponHash > 0))
					{
						bool bMatchesSavedWeaponInfo = pWeaponInfo && (s_CachedLocalPlayerVehicleWeaponHash == pWeaponInfo->GetHash());
						if(bMatchesSavedWeaponInfo)
						{
							nBestWeapon = i;
							break;
						}
						else if(nBestWeapon == -1)
						{
							nBestWeapon = i; //keep searching.
						}
					}
					else
					{
						nBestWeapon = i;	
						break;
					}
				}
			}
		}
	}

	return nBestWeapon;
}

#if KEYBOARD_MOUSE_SUPPORT
rage::u32 CPedWeaponSelector::GetNextPedWeaponInGroup( const CPed* pPed, eWeaponWheelSlot eWeaponGroup, const CWeaponInfoBlob::SlotList& slotList )
{
	weaponFatalAssertf(pPed, "pPed is NULL");

	if(m_iWeaponCatagoryIndex[eWeaponGroup] >= 0 && m_iWeaponCatagoryIndex[eWeaponGroup] < slotList.GetCount())
	{
		// Set the index to the starting index

		while(m_iWeaponCatagoryIndex[eWeaponGroup] < slotList.GetCount())
		{
			weaponAssert(pPed->GetInventory());
			const CWeaponItem* pItem = pPed->GetInventory()->GetWeaponBySlot(slotList[m_iWeaponCatagoryIndex[eWeaponGroup]].GetHash());
			if(pItem)
			{
				bool bIgnoreAmmoCheck = true;
				// B*2281283: Do ammo check if going through WEAPON_WHEEL_SLOT_THROWABLE_SPECIAL group. 
				// If we pick the next weapon in this group and it has no ammo, logic in CTaskSwapWeapon::StateDraw_OnEnter calls pPed->GetWeaponManager()->EquipBestWeapon().
				if (eWeaponGroup == WEAPON_WHEEL_SLOT_THROWABLE_SPECIAL)
				{
					bIgnoreAmmoCheck = false;
				}
				if(pItem->GetInfo()->GetWeaponWheelSlot() == eWeaponGroup && GetIsWeaponValid(pPed, pItem->GetInfo(), false, bIgnoreAmmoCheck))
				{
					return pItem->GetInfo()->GetHash();
				}
			}

			// Increment the index
			m_iWeaponCatagoryIndex[eWeaponGroup]++;
		}
	}

	m_iWeaponCatagoryIndex[eWeaponGroup] = 0;
	return 0;
}

u32 CPedWeaponSelector::GetNextWeaponFromMemory()
{
	atRangeArray<atHashWithStringNotFinal, MAX_WHEEL_SLOTS> memoryList;
	CNewHud::PopulateWeaponsFromWheelMemory(memoryList);
	int iIndex = 0;
	for(int i = 0; i < memoryList.size(); ++i)
	{
		if(memoryList[i] == CNewHud::GetWheelCurrentlyHighlightedWeapon())
		{
			iIndex = i+1;
			break;
		}
	}

	// return the 1st valid hash after slot index iIndex
	for(; iIndex < memoryList.size(); ++iIndex)
	{
		if(memoryList[iIndex] != 0)
			return memoryList[iIndex];
	}

	// if no valid hash exists before the end of the list then loop back around
	for(int i = 0; i < memoryList.size(); ++i)
	{
		if(memoryList[i] != 0)
			return memoryList[i];
	}

	return 0;
}

u32 CPedWeaponSelector::GetPreviousWeaponFromMemory()
{
	atRangeArray<atHashWithStringNotFinal, MAX_WHEEL_SLOTS> memoryList;
	CNewHud::PopulateWeaponsFromWheelMemory(memoryList);
	int iIndex = 0;
	for(int i = memoryList.size()-1; i >= 0; --i)
	{
		if(memoryList[i] == CNewHud::GetWheelCurrentlyHighlightedWeapon())
		{
			iIndex = i-1;
			break;
		}
	}

	// return the 1st valid hash after slot index iIndex
	for(; iIndex >= 0; --iIndex)
	{
		if(memoryList[iIndex] != 0)
			return memoryList[iIndex];
	}

	// if no valid hash exists before the end of the list then loop back around
	for(int i = memoryList.size()-1; i >= 0; --i)
	{
		if(memoryList[i] != 0)
			return memoryList[i];
	}

	return 0;
}
#endif // KEYBOARD_MOUSE_SUPPORT
