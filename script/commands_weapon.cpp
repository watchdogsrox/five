//
// filename:	commands_weapon.cpp
// description:
//

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
#include "script/wrapper.h"
// Game headers
#include "audio/weaponaudioentity.h"
#include "Camera/CamInterface.h"
#include "Frontend/NewHud.h"
#include "Network/Events/NetworkEventTypes.h"
#include "Network/Objects/Entities/NetObjPlayer.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "pickups/Data/RewardData.h"
#include "Pickups/PickupManager.h"
#include "Script/Handlers/GameScriptResources.h"
#include "Script/Script.h"
#include "Script/Script_Helper.h"
#include "Task/Weapons/Gun/TaskAimGunOnFoot.h"
#include "Task/Weapons/Gun/TaskReloadGun.h"
#include "Task/Weapons/TaskDetonator.h"
#include "Vehicles/Vehicle.h"
#include "Vehicles/VehicleGadgets.h"
#include "VehicleAi/VehicleIntelligence.h"
#include "Weapons/Components/WeaponComponent.h"
#include "Weapons/Components/WeaponComponentManager.h"
#include "Weapons/Info/WeaponAnimationsManager.h"
#include "Weapons/Inventory/PedInventoryLoadOut.h"
#include "Weapons/Projectiles/ProjectileManager.h"
#include "Weapons/Projectiles/Projectile.h"
#include "Weapons/AirDefence.h"
#include "Weapons/WeaponManager.h"
#include "Weapons/Weapon.h"
#include "Weapons/WeaponDebug.h"
#include "Weapons/WeaponFactory.h"
#include "Weapons/WeaponScriptResource.h"
#include "Vehicles/Metadata/VehicleMetadataManager.h"
#include "Vehicles/Metadata/VehicleSeatInfo.h"

AI_OPTIMISATIONS()
AI_COVER_OPTIMISATIONS()
WEAPON_OPTIMISATIONS()


// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------
// Keep in sync with definitions in commands_weapon.sch
struct scrWeaponHudValues
{
	scrValue HudDamage;
	scrValue HudSpeed;
	scrValue HudCapacity;
	scrValue HudAccuracy;
	scrValue HudRange;
};

SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(scrWeaponHudValues);

// --- Globals ------------------------------------------------------------------

// --- Static Globals -----------------------------------------------------------

// --- Static class members -----------------------------------------------------

// --- Code ---------------------------------------------------------------------

namespace weapon_commands {
void CommandEnableLaserSightRendering( bool bEnabled )
{
	CWeaponManager::EnableLaserSightRendering( bEnabled );
}

//
// name:		CommandGetWeaponComponentTypeModel
// description:	Returns the hash key of the weapon model
int CommandGetWeaponComponentTypeModel(int weaponComponentHash)
{
	if(SCRIPT_VERIFY( weaponComponentHash != 0, "GET_WEAPON_COMPONENT_TYPE_MODEL - Weapon component hash is invalid"))
	{
		const CWeaponComponentInfo* pComponentInfo = CWeaponComponentManager::GetInfo(weaponComponentHash);
		if(scriptVerifyf(pComponentInfo, "Weapon component hash [%d] does not exist in data", weaponComponentHash))
		{
			if(CModelInfo::GetBaseModelInfoFromHashKey(pComponentInfo->GetModelHash(), NULL))
			{
				return pComponentInfo->GetModelHash();
			}
		}
	}
	return 0;
}

//
// name:		CommandGetWeaponTypeModel
// description:	Returns the hash key of the weapon model
int CommandGetWeaponTypeModel(int weaponHash)
{
	if(SCRIPT_VERIFY( weaponHash != 0, "GET_WEAPONTYPE_MODEL - Weapon hash is invalid"))
	{
		const CItemInfo* pItemInfo = CWeaponInfoManager::GetInfo(weaponHash);
		if(scriptVerifyf(pItemInfo, "Weapon hash [%d] does not exist in data", weaponHash))
		{
			if(CModelInfo::GetBaseModelInfoFromHashKey(pItemInfo->GetModelHash(), NULL))
			{
				return pItemInfo->GetModelHash();
			}
		}
	}
	return 0;
}

//
// name:		CommandGetWeaponTypeSlot
// description:	Return the weapon type slot HASH
int CommandGetWeaponTypeSlot(int weaponHash)
{
	if(SCRIPT_VERIFY( weaponHash != 0, "GET_WEAPONTYPE_SLOT - Weapon hash is invalid"))
	{
		const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(weaponHash);
		if(scriptVerifyf(pWeaponInfo, "GET_WEAPONTYPE_SLOT - Weapon hash [%d] does not exist in data", weaponHash))
		{
			return pWeaponInfo->GetSlot();
		}
	}
	return 0;
}

//
// name:		CommandGetWeaponTypeGroup
// description:	Return the weapon type group HASH
int CommandGetWeaponTypeGroup(int weaponHash)
{
	if(SCRIPT_VERIFY( weaponHash != 0, "GET_WEAPONTYPE_GROUP - Weapon hash is invalid"))
	{
		const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(weaponHash);
		if(scriptVerifyf(pWeaponInfo, "GET_WEAPONTYPE_GROUP - Weapon hash [%d] does not exist in data", weaponHash))
		{
			return pWeaponInfo->GetGroup();
		}
	}
	return 0;
}

//
// name:		CommandGetWeaponComponentVariantExtraCount
// description:	Returns the number of entries in the variant model info extra components array
int CommandGetWeaponComponentVariantExtraCount(int weaponComponentHash)
{
	if(SCRIPT_VERIFY( weaponComponentHash != 0, "GET_WEAPON_COMPONENT_VARIANT_EXTRA_COUNT - Weapon component hash is invalid"))
	{
		const CWeaponComponentInfo* pComponentInfo = CWeaponComponentManager::GetInfo(weaponComponentHash);
		if(scriptVerifyf(pComponentInfo, "GET_WEAPON_COMPONENT_VARIANT_EXTRA_COUNT - Weapon component hash [%d] does not exist in data", weaponComponentHash))
		{
#if RSG_GEN9
			if(scriptVerifyf(pComponentInfo->GetWeaponClassType() == WCT_VariantModel, "GET_WEAPON_COMPONENT_VARIANT_EXTRA_COUNT - Weapon component hash [%d] is not of type WCT_VariantModel", weaponComponentHash))
#else
			if(scriptVerifyf(pComponentInfo->GetClassId() == WCT_VariantModel, "GET_WEAPON_COMPONENT_VARIANT_EXTRA_COUNT - Weapon component hash [%d] is not of type WCT_VariantModel", weaponComponentHash))
#endif
			{
				const CWeaponComponentVariantModelInfo *pComponentVariantModelInfo = static_cast<const CWeaponComponentVariantModelInfo*>(pComponentInfo);
				if (pComponentVariantModelInfo)
				{
					return pComponentVariantModelInfo->GetExtraComponentCount();
				}
			}
		}
	}
	return -1;
}

//
// name:		CommandGetWeaponComponentVariantExtraModel
// description:	Returns the model hash at the given index in the variant model info extra components array
int CommandGetWeaponComponentVariantExtraModel(int weaponComponentHash, int extraComponentIndex)
{
	if(SCRIPT_VERIFY( weaponComponentHash != 0, "GET_WEAPON_COMPONENT_VARIANT_EXTRA_MODEL - Weapon component hash is invalid"))
	{
		const CWeaponComponentInfo* pComponentInfo = CWeaponComponentManager::GetInfo(weaponComponentHash);
		if(scriptVerifyf(pComponentInfo, "GET_WEAPON_COMPONENT_VARIANT_EXTRA_MODEL - Weapon component hash [%d] does not exist in data", weaponComponentHash))
		{
#if RSG_GEN9
			if(scriptVerifyf(pComponentInfo->GetWeaponClassType() == WCT_VariantModel, "GET_WEAPON_COMPONENT_VARIANT_EXTRA_MODEL - Weapon component hash [%d] is not of type WCT_VariantModel", weaponComponentHash))
#else
			if(scriptVerifyf(pComponentInfo->GetClassId() == WCT_VariantModel, "GET_WEAPON_COMPONENT_VARIANT_EXTRA_MODEL - Weapon component hash [%d] is not of type WCT_VariantModel", weaponComponentHash))		
#endif
			{
				const CWeaponComponentVariantModelInfo *pComponentVariantModelInfo = static_cast<const CWeaponComponentVariantModelInfo*>(pComponentInfo);
				if (pComponentVariantModelInfo)
				{
					s32 extraComponentCount = pComponentVariantModelInfo->GetExtraComponentCount();
					if(scriptVerifyf(extraComponentIndex >= 0 && extraComponentIndex < extraComponentCount, "GET_WEAPON_COMPONENT_VARIANT_EXTRA_MODEL - Array index of [%d] is invalid", extraComponentIndex))
					{
						u32 extraModelHash = pComponentVariantModelInfo->GetExtraComponentModelByIndex(extraComponentIndex);
						if(CModelInfo::GetBaseModelInfoFromHashKey(extraModelHash, NULL))
						{
							return extraModelHash;
						}
					}
				}
			}
		}
	}
	return 0;
}

//
// name:		CommandGiveWeaponToPed
// description:	Give a weapon to a character with an amount of ammo
void CommandGiveWeaponToPed(int PedIndex, int TypeOfWeapon, int AmountOfAmmo, bool bForceIntoHand, bool bEquip)
{
	CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
	if(pPed)
	{
		//! Don't give dead melee peds weapons.
		if(pPed->IsDeadByMelee())
		{
			return;
		}

		if (pPed->IsNetworkClone())
		{
			scriptAssertf(AmountOfAmmo < (1<<CGiveWeaponEvent::SIZEOF_AMMO), "%s:GIVE_WEAPON_TO_PED - Invalid ammo amount %d, max size is %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), AmountOfAmmo, (1<<CGiveWeaponEvent::SIZEOF_AMMO));

			CGiveWeaponEvent::Trigger(pPed, TypeOfWeapon, AmountOfAmmo);
			return;
		}

		if(scriptVerifyf(pPed->GetWeaponManager(), "%s:GIVE_WEAPON_TO_PED - ped requires a weapon manager", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			if(scriptVerifyf(TypeOfWeapon != 0, "%s:GIVE_WEAPON_TO_PED - invalid TypeOfWeapon [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), TypeOfWeapon))
			{
				if(scriptVerifyf(pPed->GetInventory()->AddWeaponAndAmmo(TypeOfWeapon, AmountOfAmmo), "%s:GIVE_WEAPON_TO_PED - Failed to give weapon [%d] to ped, is this a DLC weapon or invalid?", CTheScripts::GetCurrentScriptNameAndProgramCounter(), TypeOfWeapon))
				{
#if !__FINAL
					weaponDebugf1("%s:GIVE_WEAPON_TO_PED: [%d][%s], Ammo:%d: Ped 0x%p [%s] [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), TypeOfWeapon, atHashString(TypeOfWeapon).TryGetCStr(), AmountOfAmmo, pPed, pPed->GetDebugName(), PedIndex);
					scrThread::PrePrintStackTrace();
#endif

					if(bEquip)
					{
						if(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
						{
							// Ensure current weapon is stored as a backup so we can restore it when getting out
							pPed->GetWeaponManager()->SetBackupWeapon(TypeOfWeapon);
							pPed->GetWeaponManager()->EquipBestWeapon();
						}
						else if(pPed->GetIsSwimming() || pPed->GetPedResetFlag(CPED_RESET_FLAG_IsClimbing))
						{
							// Ensure current weapon is stored as a backup so we can restore it when getting out
							pPed->GetWeaponManager()->SetBackupWeapon(TypeOfWeapon);
						}
						else
						{
							pPed->GetWeaponManager()->EquipWeapon(TypeOfWeapon, -1, bForceIntoHand && !pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ));
						}
					}
				}
			}
		}
	}
}

void CommandGiveDelayedWeaponToPed(int PedIndex, int TypeOfWeapon, int AmountOfAmmo, bool bSetAsCurrentWeapon)
{
	CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
	if(pPed)
	{
		if (pPed->IsNetworkClone())
		{	//	just ignore bSetAsCurrentWeapon? Always treated as true.
			scriptAssertf(AmountOfAmmo < (1<<CGiveWeaponEvent::SIZEOF_AMMO),   "%s:GIVE_DELAYED_WEAPON_TO_PED - Invalid ammo amount %d, max size is %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), AmountOfAmmo, (1<<CGiveWeaponEvent::SIZEOF_AMMO));
			CGiveWeaponEvent::Trigger(pPed, TypeOfWeapon, AmountOfAmmo);
			return;
		}

		if(scriptVerifyf(pPed->GetInventory(), "%s:GIVE_DELAYED_WEAPON_TO_PED - ped requires an inventory", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			pPed->GetInventory()->AddWeaponAndAmmo(TypeOfWeapon, AmountOfAmmo);
			if(bSetAsCurrentWeapon)
			{
				if(scriptVerifyf(pPed->GetWeaponManager(), "%s:GIVE_DELAYED_WEAPON_TO_PED - ped requires a weapon manager", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
				{
					pPed->GetWeaponManager()->EquipWeapon(TypeOfWeapon);
				}
			}
		}
	}	
}

//
// name:		CommandAddAmmoToPed
// description:	Give ammo to character
void CommandAddAmmoToPed(int PedIndex, int TypeOfWeapon, int AmountOfAmmo)
{
	CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

	if(pPed)
	{
			if(scriptVerifyf(pPed->GetInventory(), "%s:ADD_AMMO_TO_PED - ped requires an inventory", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
			{
				pPed->GetInventory()->AddWeaponAmmo(TypeOfWeapon, AmountOfAmmo);
			}
		}
	}

//
// name:		CommandSetPedAmmo
// description:	Set ammo for a characters weapon
void CommandSetPedAmmo(int PedIndex, int TypeOfWeapon, int AmountOfAmmo, bool bIgnoreDeadCheck)
{
	CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, bIgnoreDeadCheck ? CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES_NO_DEAD_CHECK : CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

	if(pPed)
	{	
		scriptAssertf(AmountOfAmmo < (1<<CGiveWeaponEvent::SIZEOF_AMMO),   "%s:SET_PED_AMMO - Invalid ammo amount %d, max size is %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), AmountOfAmmo, (1<<CGiveWeaponEvent::SIZEOF_AMMO));

		if (CTheScripts::Frack() && pPed->IsNetworkClone())
		{
			CGiveWeaponEvent::Trigger(pPed, TypeOfWeapon, AmountOfAmmo);
			return;
		}

		if(scriptVerifyf(pPed->GetInventory(), "%s:SET_PED_AMMO - ped requires an inventory", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			pPed->GetInventory()->SetWeaponAmmo(TypeOfWeapon, AmountOfAmmo);
			
			// Update the current weapon ammo amount immediately for the UI system.
			if(pPed->GetWeaponManager())
			{
				CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();

				if(pWeapon && pWeapon->GetWeaponHash() == (u32)(TypeOfWeapon))
				{
					pWeapon->SetAmmoTotal(AmountOfAmmo);
				}
			}
		}
	}
}

//
// name:		CommandSetPedInfiniteAmmo
// description:	Enable/Disable infinite ammo for the specified ped
void CommandSetPedInfiniteAmmo(int PedIndex, bool bInfinite, int TypeOfWeapon)
{
	CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);

	if(pPed)
	{
		if(scriptVerifyf(pPed->GetInventory(), "%s:SET_PED_INFINITE_AMMO - ped requires an inventory", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			if(CTheScripts::Frack() && TypeOfWeapon == 0)
			{
				pPed->GetInventory()->GetAmmoRepository().SetUsingInfiniteAmmo(bInfinite);
			}
			else if(scriptVerifyf(pPed->GetInventory()->GetWeapon(TypeOfWeapon), "%s:SET_PED_INFINITE_AMMO - ped does not have weapon [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), TypeOfWeapon))
			{
				pPed->GetInventory()->SetWeaponUsingInfiniteAmmo(TypeOfWeapon, bInfinite);
			}
		}
	}
}

//
// name:		CommandSetPedInfiniteAmmoClip
// description:	Enable/Disable infinite ammo for the specified ped
void CommandSetPedInfiniteAmmoClip(int PedIndex, bool bInfinite)
{
	CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);

	if(pPed && CTheScripts::Frack())
	{
		if(scriptVerifyf(pPed->GetInventory(), "%s:SET_PED_INFINITE_AMMO_CLIP - ped requires an inventory", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			// Store whether we want infinite clips or not
			pPed->GetInventory()->GetAmmoRepository().SetUsingInfiniteClips(bInfinite);
		}
	}
}

//
// name:		CommandSetPedStunGunFiniteAmmo
// description:	Enable/Disable finite ammo for WEAPON_STUNGUN for the specified ped, overriding metadata InfiniteAmmo value
void CommandSetPedStunGunFiniteAmmo(int PedIndex, bool bValue)
{
	CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);

	if (pPed)
	{
		if (scriptVerifyf(pPed->GetInventory(), "%s:SET_PED_STUN_GUN_FINITE_AMMO - ped requires an inventory", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			if (CTheScripts::Frack())
			{
				pPed->GetInventory()->GetAmmoRepository().SetIgnoreInfiniteAmmoFlag(ATSTRINGHASH("AMMO_STUNGUN", 0xB02EADE0), bValue);
			}
		}
	}
}

//
// name:		CommandSetCurrentCharWeapon
// description:	Set the current weapon a character has
void CommandSetCurrentPedWeapon(int PedIndex, int TypeOfWeapon, bool bForceInHand)
{
	CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

	if(pPed)
	{
		if(pPed->GetInventory())
		{
			if(pPed->GetInventory()->GetWeapon(TypeOfWeapon))
			{	
				if(scriptVerifyf(pPed->GetWeaponManager(), "%s:SET_CURRENT_PED_WEAPON - ped requires a weapon manager", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
				{
					pPed->GetWeaponManager()->EquipWeapon(TypeOfWeapon, -1, bForceInHand && pPed->GetVehiclePedInside() == NULL);

					if(pPed->GetVehiclePedInside() || pPed->GetPedResetFlag(CPED_RESET_FLAG_IsClimbing))
					{
						// Ensure backup weapon is the same, so we will keep the selecting weapon if we are in a vehicle
						pPed->GetWeaponManager()->SetBackupWeapon(TypeOfWeapon);
					}
				}
			}
			else
			{
#if !__FINAL
				scrThread::PrePrintStackTrace();
#endif
				scriptAssertf(false, "%s:SET_CURRENT_PED_WEAPON - weapon does not exist in ped's inventory - Weapon:[%d][%s] Ped:[0x%p][%s][%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), TypeOfWeapon, atHashString(TypeOfWeapon).TryGetCStr(), pPed, pPed->GetDebugName(), PedIndex);
			}
		}
		else
		{
			scriptAssertf(false, "%s:SET_CURRENT_PED_WEAPON - ped requires an inventory", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
	}
}

//
// name:		CommandGetCurrentPedWeapon
// description:
bool CommandGetCurrentPedWeapon(int PedIndex, int &ReturnWeaponType, bool DoDeadCheck)
{
	unsigned assertFlags = CTheScripts::GUID_ASSERT_FLAGS_ALL;
	if(!DoDeadCheck)
	{
		assertFlags = CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK;
	}

	const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex, assertFlags);

	if(pPed)
	{
		if(scriptVerifyf(pPed->GetWeaponManager(), "%s:GET_CURRENT_PED_WEAPON - ped requires a weapon manager", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			ReturnWeaponType = pPed->GetWeaponManager()->GetEquippedWeaponHash();

			// return whether the current weapon is usable (i.e. in their hand)
			return ((u32)ReturnWeaponType==pPed->GetWeaponManager()->GetEquippedWeaponObjectHash());
		}
	}
	return 0;
}

//
// name:		CommandGetCurrentPedWeaponEntityIndex
// description:
int CommandGetCurrentPedWeaponEntityIndex(int PedIndex, bool DoDeadCheck)
{
	unsigned assertFlags = CTheScripts::GUID_ASSERT_FLAGS_ALL;
	if(!DoDeadCheck)
	{
		assertFlags = CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK;
	}

	const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex, assertFlags);
	if(pPed)
	{
		if(scriptVerifyf(pPed->GetWeaponManager(), "%s:GET_CURRENT_PED_WEAPON_ENTITY_INDEX - ped requires a weapon manager", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			const CObject* pWeaponObject = pPed->GetWeaponManager()->GetEquippedWeaponObject();
			if( pWeaponObject )
				return CTheScripts::GetGUIDFromEntity( const_cast<CObject&>(*pWeaponObject) );
		}
	}
	return NULL_IN_SCRIPTING_LANGUAGE;
}

//
// name:		CommandGetBestPedWeapon
// description:
int CommandGetBestPedWeapon(int PedIndex, bool bIgnoreAmmoCheck)
{
	const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
	if(pPed)
	{
		if(scriptVerifyf(pPed->GetWeaponManager(), "%s:GET_BEST_PED_WEAPON - ped requires a weapon manager", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			return pPed->GetWeaponManager()->GetBestWeaponHash(0,bIgnoreAmmoCheck);
		}
	}
	return 0;
}

//
// name:		CommandSetCurrentPedVehicleWeapon
// description:	Set the current vehicle weapon ped can use
bool CommandSetCurrentPedVehicleWeapon(int PedIndex, int TypeOfWeapon)
{
	CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
	if(pPed)
	{
		if(scriptVerifyf(pPed->GetInventory(), "%s:SET_CURRENT_PED_VEHICLE_WEAPON - ped requires an inventory", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			// First attempt to set the vehicle weapon
			if( pPed->GetIsInVehicle() )
			{
				CVehicle* pVehicle = pPed->GetMyVehicle();
				CVehicleWeaponMgr* pVehicleWeaponMgr = pVehicle ? pVehicle->GetVehicleWeaponMgr() : NULL;
				if( pVehicleWeaponMgr )
				{
					// Selected vehicle weapon index needs to be seat-relative
					s32 nVehicleWeaponIndex = -1;
					atArray<CVehicleWeapon*> vehicleWeapons;
					pVehicleWeaponMgr->GetWeaponsForSeat(pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed), vehicleWeapons);
					for(int i= 0; i < vehicleWeapons.GetCount(); ++i)
					{
						if(vehicleWeapons[i]->GetHash() == (u32)TypeOfWeapon)
						{
							nVehicleWeaponIndex = i;
						}
					}

					if( nVehicleWeaponIndex != -1 && pPed->GetWeaponManager()->EquipWeapon( 0, nVehicleWeaponIndex, false ) )
					{
						if(NetworkInterface::IsGameInProgress() && NetworkUtils::IsNetworkCloneOrMigrating(pVehicle))
						{
							CBlockWeaponSelectionEvent::Trigger(*pVehicle, true);
						}
						else
						{
							pVehicle->GetIntelligence()->BlockWeaponSelection(true);
						}
						return true;
					}
				}
			}
		}
	}


	return false;
}

//
// name:		CommandGetCurrentPedVehicleWeapon
// description: Retrieve the vehicle weapon the designated ped is currently using
bool CommandGetCurrentPedVehicleWeapon(int PedIndex, int &ReturnWeaponType)
{
	const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
	if(pPed)
	{
		if(scriptVerifyf(pPed->GetWeaponManager(), "%s:GET_CURRENT_PED_WEAPON - ped requires a weapon manager", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			const CVehicleWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedVehicleWeapon();
			if( pWeapon )
			{
				ReturnWeaponType = pWeapon->GetHash();
				return true;
			}
		}		
	}

	return false;
}

void CommandSetCycleVehicleWeaponsOnly(int PedIndex)
{
	CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
	if(pPed)
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_OnlySelectVehicleWeapons, true);
	}
}

enum eWeaponCheckFlags
{
	WF_INCLUDE_MELEE		= 1,
	WF_INCLUDE_PROJECTILE	= 2,
	WF_INCLUDE_GUN			= 4
};

bool CommandIsPedArmed(int PedIndex, int iCheckFlags)
{
	const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);

	if(pPed)
	{
		// Get the current weapon info
		if(scriptVerifyf(pPed->GetWeaponManager(), "%s:IS_PED_ARMED - ped requires a weapon manager", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			u32 weaponHash = pPed->GetWeaponManager()->GetEquippedWeaponHash();

			// Ignore objects (phone etc...)
			if( weaponHash == OBJECTTYPE_OBJECT )
			{
				return false;
			}

			// Check for valid weapon
			const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(weaponHash);
			if( pWeaponInfo == NULL || pWeaponInfo->GetIsUnarmed() )
			{
				return false;
			}

			// Check for melee weapons
			if( (iCheckFlags & WF_INCLUDE_MELEE) && pWeaponInfo->GetIsMelee() )
			{
				return true;
			}

			// Projectiles
			if( (iCheckFlags & WF_INCLUDE_PROJECTILE) && pWeaponInfo->GetIsProjectile() )
			{
				return pPed->GetWeaponManager()->GetEquippedWeaponObject() != NULL;
			}

			// guns (including rocket launcher)
			if( (iCheckFlags & WF_INCLUDE_GUN) && pWeaponInfo->GetIsGun())
			{
				return pPed->GetWeaponManager()->GetEquippedWeaponObject() != NULL;
			}
		}
	}
	return false;
}

//
// name:		CommandHasPedGotWeapon
// description:	Return if character has got a weapon
bool CommandIsWeaponValid(int TypeOfWeapon)
{
	const CItemInfo* pInfo = CWeaponInfoManager::GetInfo(TypeOfWeapon ASSERT_ONLY(,true));
	if (pInfo)
	{
		return true;
	}
	return false;
}

//
// name:		CommandHasPedGotWeapon
// description:	Return if character has got a weapon
bool CommandHasPedGotWeapon(int PedIndex, int TypeOfWeapon, int GeneralWeaponType)
{
	const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
	bool LatestCmpFlagResult = false;

	if(pPed)
	{
		if (GeneralWeaponType > GENERALWEAPON_TYPE_INVALID)
		{
			scriptAssertf(GeneralWeaponType < NUM_GENERALWEAPON_TYPES, "Invalid general weapon type passed in");
			scriptAssertf(TypeOfWeapon == 0, "If general weapon type is used, set weapon type to WEAPONTYPE_INVALID");	

			const CWeaponItemRepository& WeaponRepository = pPed->GetInventory()->GetWeaponRepository();

			if (GENERALWEAPON_TYPE_ANYWEAPON == TypeOfWeapon)
			{
				for(int i=0; i<WeaponRepository.GetItemCount(); i++)
				{
					const CWeaponItem* pWeaponItem = WeaponRepository.GetItemByIndex(i);
					Assert(pWeaponItem);
					const CWeaponInfo* pWeaponInfo = pWeaponItem->GetInfo();
					if (pWeaponInfo && !pWeaponInfo->GetIsUnarmed() )
					{
						LatestCmpFlagResult = TRUE;
						break;
					}
				}
			}
			else if (GENERALWEAPON_TYPE_ANYMELEE == TypeOfWeapon)
			{
				for(int i=0; i<WeaponRepository.GetItemCount(); i++)
				{
					const CWeaponItem* pWeaponItem = WeaponRepository.GetItemByIndex(i);
					Assert(pWeaponItem);
					const CWeaponInfo* pWeaponInfo = pWeaponItem->GetInfo();
					if (pWeaponInfo && !pWeaponInfo->GetIsUnarmed() && pWeaponInfo->GetIsMelee())
					{
						LatestCmpFlagResult = TRUE;
						break;
					}
				}
			}
		}
		else
		{
			if(TypeOfWeapon != 0)
			{
				if(scriptVerifyf(pPed->GetInventory(), "%s:HAS_PED_GOT_WEAPON - ped requires an inventory", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
				{
					if(pPed->GetInventory()->GetWeapon(TypeOfWeapon))
					{
						LatestCmpFlagResult = TRUE;
					}
				}
			}
		}
	}

	return LatestCmpFlagResult;
}

bool CommandIsPedWeaponReadyToShoot(int PedIndex)
{
	const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>( PedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK );
	if( pPed )
	{
		const CPedWeaponManager* pPedWeaponMgr = pPed->GetWeaponManager();
		if( scriptVerifyf( pPedWeaponMgr, "%s:IS_PED_WEAPON_READY_TO_SHOOT - ped requires a weapon manager", CTheScripts::GetCurrentScriptNameAndProgramCounter() ) )
		{
			const CWeapon* pWeapon = pPedWeaponMgr->GetEquippedWeapon();
			return pWeapon && pWeapon->GetState() == CWeapon::STATE_READY;
		}
	}

	return false;
}

//
// name:		CommandGetWeaponTypeInSlot
// description:	Return the weapon type in a given slot HASH for a given character
int CommandGetPedWeaponTypeInSlot(int PedGuid, int slotHash)
{
	const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedGuid, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
	if(pPed && pPed->GetInventory())
	{
		const CWeaponItem* pItem = pPed->GetInventory()->GetWeaponBySlot(slotHash);
		if (pItem)
			return pItem->GetInfo()->GetHash();
		else
			return 0;
	}
	return 0;
}


//
// name:		CommandGetAmmoInPedWeapon
// description:	return the amount of ammo in a weapon held by a character
int CommandGetAmmoInPedWeapon(int PedIndex, int TypeOfWeapon)
{
	int ReturnAmmo = 0;
	const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
	if(pPed)
	{
		if(scriptVerifyf(pPed->GetInventory(), "%s:GET_AMMO_IN_PED_WEAPON - ped requires an inventory", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			ReturnAmmo = pPed->GetInventory()->GetWeaponAmmo(TypeOfWeapon);
		}
	}
	return ReturnAmmo;
}

//
// name:		CommandRemoveWeaponFromPed
// description:	Remove weapon from character
void CommandRemoveWeaponFromPed(int PedIndex, int TypeOfWeapon)
{
	CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAG_ENTITY_EXISTS);
	
	if(pPed)
	{	
		if (pPed->IsNetworkClone())
		{
			CRemoveWeaponEvent::Trigger(pPed, TypeOfWeapon);
			return;
		}

		if(scriptVerifyf(pPed->GetInventory(), "%s:REMOVE_WEAPON_FROM_PED - ped requires an inventory", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			if(pPed->GetInventory()->RemoveWeapon(TypeOfWeapon))
			{
#if !__FINAL
				weaponDebugf1("%s:REMOVE_WEAPON_FROM_PED: [%d][%s]: Ped 0x%p [%s] [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), TypeOfWeapon, atHashString(TypeOfWeapon).TryGetCStr(), pPed, pPed->GetDebugName(), PedIndex);
				scrThread::PrePrintStackTrace();
#endif
			}
			CTheScripts::Frack2();
		}	
	}
}


//
// name:		CommandRemoveAllCharWeapons
// description:	Remove all weapons from character
void CommandRemoveAllPedWeapons(int PedIndex, bool DeadCheck)
{
	g_WeaponAudioEntity.SetBlockSpinUpFrame(fwTimer::GetFrameCount());
	unsigned flags = DeadCheck ? CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES : CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES & (~CTheScripts::GUID_ASSERT_FLAG_DEAD_CHECK);
	CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, flags);

	if(pPed)
	{	
		if (pPed->IsNetworkClone())
		{
            if(scriptVerifyf(!pPed->IsPlayer(), "Trying to remove all weapons from a remote player! This is not allowed for anti-cheat reasons!"))
            {
			    CRemoveAllWeaponsEvent::Trigger(pPed);
            }

			return;
		}

		CPedInventoryLoadOutManager::SetDefaultLoadOut(pPed);
	}
}


//
// name:		CommandHideCharWeaponForScriptedCutscene
// description:	Hide this peds weapons while playing a cutscene
void CommandHidePedWeaponForScriptedCutscene(int PedIndex, bool HideWeaponFlag)
{
	CPed *	pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
	if(pPed)
	{
		if(scriptVerifyf(pPed->GetWeaponManager(), "%s:HIDE_PED_WEAPON_FOR_SCRIPTED_CUTSCENE - ped requires a weapon manager", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			if (HideWeaponFlag)
			{
				pPed->GetWeaponManager()->DestroyEquippedWeaponObject(false);
			}
			else
			{
 				if(!pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
				{
 					pPed->GetWeaponManager()->EquipWeapon(pPed->GetWeaponManager()->GetEquippedWeaponHash(), -1, true);
				}
			}
		}
	}
}

//
// name:		CommandSetCharCurrentWeaponVisible
// description:	Toggles the visibility of the peds currently equipped weapon
void CommandSetPedCurrentWeaponVisible(int PedIndex, bool VisibleFlag, bool DestroyObject, bool DeadCheck, bool bStoreDestroyedWeaponClipValue)
{
	unsigned flags = DeadCheck ? CTheScripts::GUID_ASSERT_FLAGS_ALL : CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK;

	CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, flags);
	if(pPed)
	{
		if(scriptVerifyf(pPed->GetWeaponManager(), "%s:SET_PED_CURRENT_WEAPON_VISIBLE - ped requires a weapon manager", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_KeepWeaponHolsteredUnlessFired, !VisibleFlag );
			if(VisibleFlag)
			{
				if (pPed->GetWeaponManager()->GetEquippedWeaponObject())
					pPed->GetWeaponManager()->GetEquippedWeaponObject()->SetIsVisibleForModule(SETISVISIBLE_MODULE_SCRIPT, true, true);
				else
 					pPed->GetWeaponManager()->EquipWeapon(pPed->GetWeaponManager()->GetEquippedWeaponHash(), -1, true);
			}
			else
			{
				if (!DestroyObject && pPed->GetWeaponManager()->GetEquippedWeaponObject())
					pPed->GetWeaponManager()->GetEquippedWeaponObject()->SetIsVisibleForModule(SETISVISIBLE_MODULE_SCRIPT, false, true);
				else
				{
					// B*2295321: Cache ammo clip info for previous weapon (gets restored in CPedWeaponManager::CreateEquippedWeaponObject).
					if (bStoreDestroyedWeaponClipValue && pPed->IsLocalPlayer() && pPed->GetEquippedWeaponInfo() && pPed->GetEquippedWeaponInfo()->GetUsesAmmo())
					{
						CWeaponItem* pPreviousWeaponItem = pPed->GetInventory() ? pPed->GetInventory()->GetWeapon(pPed->GetEquippedWeaponInfo()->GetHash()) : NULL;
						if (pPreviousWeaponItem)
						{
							s32 iAmmoInClip = pPed->GetWeaponManager()->GetEquippedWeapon() ? pPed->GetWeaponManager()->GetEquippedWeapon()->GetAmmoInClip() : -1;
							pPreviousWeaponItem->SetLastAmmoInClip(iAmmoInClip, pPed);
						}
					}
 					pPed->GetWeaponManager()->DestroyEquippedWeaponObject();
				}
			}
		}
	}
}

//
// name:		CommandSetPedDropsWeaponsWhenDead
// description:	Set if this ped drops weapons when he dies
void CommandSetPedDropsWeaponsWhenDead(int PedIndex, bool DropsWeaponsFlag)
{
	CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

	if(pPed)
	{
		if (DropsWeaponsFlag)
		{
			pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DoesntDropWeaponsWhenDead, FALSE );
		}
		else
		{
			pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_DoesntDropWeaponsWhenDead, TRUE );
		}
	}
}

//
// name:		CommandHasPedBeenDamagedByWeapon
// description:
bool CommandHasPedBeenDamagedByWeapon(int PedIndex, int TypeOfWeapon, int GeneralWeaponType)
{
	const CPed *pPed= CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);

	if(pPed)
	{
		if (GeneralWeaponType > GENERALWEAPON_TYPE_INVALID)
		{
			scriptAssertf(GeneralWeaponType < NUM_GENERALWEAPON_TYPES, "Invalid general weapon type passed in");
			scriptAssertf(TypeOfWeapon == 0, "If general weapon type is used, set weapon type to WEAPONTYPE_INVALID");	

			if(pPed->HasBeenDamagedByGeneralWeaponType(GeneralWeaponType))
			{
				return TRUE;
			}
		}
		else
		{
			if (pPed->HasBeenDamagedByHash((u32)TypeOfWeapon))
			{
				return TRUE;
			}
		}
	}
	else
	{
		Displayf("HAS_PED_BEEN_DAMAGED_BY_WEAPON - Character doesn't exist\n");
	}
	return FALSE;
}


//
// name:		CommandClearPedLastWeaponDamage
// description:
void CommandClearPedLastWeaponDamage(int PedIndex)
{
    if (SCRIPT_VERIFY(!NetworkInterface::IsGameInProgress(), "CLEAR_PED_LAST_WEAPON_DAMAGE -  This script command is not allowed in network game scripts"))
    {
	    CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

	    if(pPed)
	    {
		    pPed->ResetWeaponDamageInfo();
	    }
	    else
	    {
		    Displayf("CLEAR_PED_LAST_WEAPON_DAMAGE - Character doesn't exist");
	    }
    }
}

//
// name:		CommandHasEntityBeenDamagedByWeapon
// description:	Return if the entity has been damaged by a weapon type
bool CommandHasEntityBeenDamagedByWeapon(int EntityIndex, int TypeOfWeapon, int GeneralWeaponType)
{
	const CPhysical *pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex,CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
	bool LatestCmpFlagResult = false;

	if(pEntity)
	{
		if (GeneralWeaponType > GENERALWEAPON_TYPE_INVALID)
		{
			scriptAssertf(GeneralWeaponType < NUM_GENERALWEAPON_TYPES, "Invalid general weapon type passed in");
			scriptAssertf(TypeOfWeapon == 0, "If general weapon type is used, set weapon type to WEAPONTYPE_INVALID");	
	
			if (pEntity->HasBeenDamagedByGeneralWeaponType(GeneralWeaponType))
			{
				return TRUE;
			}
		}
		else
		{
			if (pEntity->HasBeenDamagedByHash((u32)TypeOfWeapon))
			{
				LatestCmpFlagResult = true;
			}
		}
	}

	return LatestCmpFlagResult;
}

//
// name:		CommandClearEntityLastWeaponDamage
// description:	Clear weapon damage info on entity
void CommandClearEntityLastWeaponDamage(int EntityIndex)
{
	if (SCRIPT_VERIFY(!NetworkInterface::IsGameInProgress(), "CLEAR_ENTITY_LAST_WEAPON_DAMAGE -  This script command is not allowed in network game scripts"))
	{
		CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(EntityIndex);

		if(pEntity)
		{
			pEntity->ResetWeaponDamageInfo();
		}
	}
}

//
// name:		CommandCharDropWeapon
// description: Drops the weapon the ped is currently holding
void CommandPedDropWeapon(int PedIndex)
{
	CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>( PedIndex );

	if(pPed)
	{
		// Only shoot on impacts 50% of the time
		if(scriptVerifyf(pPed->GetWeaponManager(), "%s:SET_PED_DROPS_WEAPON - ped requires a weapon manager", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			if( !pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_StopWeaponFiringOnImpact ) && pPed->GetWeaponManager()->GetEquippedWeaponObject() )
			{
				CObject* pWeaponObject = pPed->GetWeaponManager()->GetEquippedWeaponObject();
				pWeaponObject->m_nObjectFlags.bWeaponWillFireOnImpact = fwRandom::GetRandomTrueFalse();
				weaponDebugf1("SET_PED_DROPS_WEAPON [0x%p] dropped by ped [0x%p]: bWeaponWillFireOnImpact %d", pWeaponObject, pPed, pWeaponObject->m_nObjectFlags.bWeaponWillFireOnImpact);
#if !__FINAL
				scrThread::PrePrintStackTrace();
#endif
			}
		}

		CPedWeaponManager* pWeaponMgr = pPed->GetWeaponManager();
		if(pWeaponMgr)
		{
			const CWeapon* pWeapon = pWeaponMgr->GetEquippedWeapon();
			if(pWeapon && pWeapon->GetWeaponInfo()->GetPickupHash() != 0)
			{
				// Drop the weapon
				CPickup *pPickup = CPickupManager::CreatePickUpFromCurrentWeapon(pPed);

				if(pPickup)
				{
					// Need to call update here because otherwise the the pickup update won't happen till the next frame
					// which will leave the pickup in the exterior for one frame even though the ped that dropped it may have been
					// in an interior
					pPickup->Update();

					// Remove it from inventory
					if(pPed->GetInventory())
					{
						pPed->GetInventory()->RemoveWeapon(pWeapon->GetWeaponHash());
					}

					// Equip best one
					pWeaponMgr->EquipBestWeapon();
				}
			}
		}
	}
	else
	{
		Displayf("SET_PED_DROPS_WEAPON - Character doesn't exist");
	}
}

void CommandPedDropsInventoryWeapon(int PedIndex, int TypeOfWeapon, const scrVector & offset, int ammoAmount)
{
	Vector3 vOffset(offset);

	CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>( PedIndex );

	if(pPed)
	{
		CPedInventory* pedInventory = pPed->GetInventory();

		CPedWeaponManager* pWeaponManager = pPed->GetWeaponManager();
		CWeaponItem* pWeaponItem = pedInventory->GetWeapon(TypeOfWeapon);
		const CWeaponInfo* pWeaponInfo = pWeaponItem ? pWeaponItem->GetInfo() : NULL;
		
		if(scriptVerifyf(pWeaponManager, "%s:SET_PED_DROPS_INVENTORY_WEAPON - ped has no weapon manager", CTheScripts::GetCurrentScriptNameAndProgramCounter()) &&
			scriptVerifyf(pWeaponItem, "%s:SET_PED_DROPS_INVENTORY_WEAPON - weapon is not in peds inventory", CTheScripts::GetCurrentScriptNameAndProgramCounter()) &&
			scriptVerifyf(pWeaponInfo, "%s:SET_PED_DROPS_INVENTORY_WEAPON - weapon has no info", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			Vector3 pedPos =  VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			Vector3 pickupPos = VEC3V_TO_VECTOR3(pPed->GetTransform().Transform(VECTOR3_TO_VEC3V(vOffset)));

			// do a probe from the player to the pickup's position, we need to check that it will not be created behind a wall etc
			const Vector3 vStart = pedPos + Vector3(0.0f, 0.0f, 0.5f);
			const Vector3 vEnd = pickupPos;
	
			WorldProbe::CShapeTestProbeDesc probeDesc;
			WorldProbe::CShapeTestHitPoint probeHitPoint;
			WorldProbe::CShapeTestResults probeResult(probeHitPoint);
			probeDesc.SetResultsStructure(&probeResult);
			probeDesc.SetStartAndEnd(vStart, vEnd);
			probeDesc.SetExcludeInstance(pPed->GetCurrentPhysicsInst());
			probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_TYPES_MOVER);

			Vector3 resultNormal;
			Vector3 resultPosition;
			bool probeHit = false;

			// test world
			probeHit = WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc);

			if(probeHit)
			{
				// if the probe hit, just place at the player's feet
				pickupPos = pedPos;
			}
				
			Matrix34 m; 
			m.Identity();
			m.RotateX(PI/2.0f);
			m.d = pickupPos;

			strLocalIndex customModelIndex = strLocalIndex(fwModelId::MI_INVALID);

			// If we have a variation model component on the weapon, use that model for the pickup
			const CWeaponComponentVariantModelInfo* pVariantInfo = pWeaponItem->GetComponentInfo<CWeaponComponentVariantModelInfo>();
			bool bWeaponHasVarMod = false;
			if (pVariantInfo)
			{
				// Make sure the model exists
				fwModelId variantModelId;
				CModelInfo::GetBaseModelInfoFromHashKey(pVariantInfo->GetModelHash(), &variantModelId);	
				if (weaponVerifyf(variantModelId.IsValid(), "ModelId is invalid for VariantModel component on [%s], using original model for pickup instead.", pWeaponInfo->GetName()))
				{
					customModelIndex = variantModelId.GetModelIndex();
					bWeaponHasVarMod = true;
				}
			}

			CPickup* pPickup = CPickupManager::CreatePickup( pWeaponInfo->GetPickupHash(), m, NULL, true, customModelIndex.Get(), false);

			if( pPickup )
			{
				// Only create and set weapon for weapon type pickups. 
				bool bIsWeaponPickup = false;
				for (u32 i=0; i<pPickup->GetPickupData()->GetNumRewards(); i++)
				{
					const CPickupRewardData* pReward = pPickup->GetPickupData()->GetReward(i);
					if (pReward && pReward->GetType() == PICKUP_REWARD_TYPE_WEAPON)
					{
						bIsWeaponPickup = true;
						break;
					}
				}

				if( bIsWeaponPickup )
				{
					if (!pPickup->GetWeapon())
					{
						pPickup->SetWeapon(WeaponFactory::Create(pWeaponInfo->GetHash(), ammoAmount, pPickup, "CommandPedDropsInventoryWeapon"));
					}
					if(pPickup->GetWeapon())
					{
						pPickup->GetWeapon()->GetAudioComponent().SetWasDropped(true);
					}

					pPickup->SetAmount(ammoAmount);

					// Make sure pickup has correct attachments
					const CWeaponItem::Components* pComponents = pWeaponItem->GetComponents();

					if (pComponents)
					{
						// B*2334397: If we have a var mod, attach that first so that we create other components with the correct model.
						if (bWeaponHasVarMod)
						{
							for( s32 i = 0; i < pComponents->GetCount(); i++ )
							{
								const CWeaponComponentInfo *pComponentInfo = (*pComponents)[i].pComponentInfo;
								if (pComponentInfo && pComponentInfo->GetIsClassId(CWeaponComponentVariantModelInfo::GetStaticClassId()))
								{
									CPedEquippedWeapon::CreateWeaponComponent(pComponentInfo->GetHash(), pPickup );
								}
							}
						}

						for( s32 i = 0; i < pComponents->GetCount(); i++ )
						{
							// Don't try to re-create the var mod.
							bool bIsVarModComponent = (*pComponents)[i].pComponentInfo->GetIsClassId(CWeaponComponentVariantModelInfo::GetStaticClassId());
							if (!bWeaponHasVarMod || (bWeaponHasVarMod && !bIsVarModComponent))
							{
								CPedEquippedWeapon::CreateWeaponComponent( (*pComponents)[i].pComponentInfo->GetHash(), pPickup );
							}
						}
					}

					// Set the correct rendering flags now whole hierarchy has been made (so attachments are set).
					pPickup->SetForceAlphaAndUseAmbientScale();
					// Set up the glow too.
					pPickup->SetUseLightOverrideForGlow();

					// Make sure pickup has correct tint
					if( pPickup->GetWeapon() )
					{
						pPickup->GetWeapon()->UpdateShaderVariables( pWeaponItem->GetTintIndex() );
					}
				}

				pPickup->SetLastOwner(pPed);

				pPickup->SetDontFadeOut();

				if (pPed->IsLocalPlayer())
				{
					if (!pPickup->GetPickupData()->GetRequiresButtonPressToPickup())
					{
						pPickup->SetDroppedFromLocalPlayer();
					}
					pPickup->SetPlayerGift();
				}

				// Need to call update here because otherwise the the pickup update won't happen till the next frame
				// which will leave the pickup in the exterior for one frame even though the ped that dropped it may have been
				// in an interior
				pPickup->Update();

				const CWeapon* pEquippedWeapon = pWeaponManager->GetEquippedWeapon();

				bool bDroppingEquipped = pEquippedWeapon && pEquippedWeapon->GetWeaponInfo() == pWeaponInfo;

				// Remove it from inventory
				pedInventory->RemoveWeapon(TypeOfWeapon);

				// Equip best one if the dropped weapon was the one held
				if (bDroppingEquipped)
				{
					pWeaponManager->EquipBestWeapon();
				}
			}
		}
	}
	else
	{
		Displayf("SET_PED_DROPS_INVENTORY_WEAPON - Ped doesn't exist");
	}
}

//
// name:		CommandGetMaxAmmoInClip
// description:	How many is the max amount of bullets allowed in the clip
// return true if the weapon type is equipped false otherwise
int CommandGetMaxAmmoInClip(int PedIndex, int TypeOfWeapon, bool DoDeadCheck)
{
	int MaxAmountOfAmmoInClip = 0;

	unsigned assertFlags = CTheScripts::GUID_ASSERT_FLAGS_ALL;
	if(!DoDeadCheck)
	{
		assertFlags = CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK;
	}

	const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex, assertFlags);

	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(TypeOfWeapon);
	if (pWeaponInfo)
	{
		MaxAmountOfAmmoInClip = pWeaponInfo->GetClipSize();
	}

	// Try to find the weapon from inventory and use the clip component since a weapon can have different clip size now.
	const CWeaponItem* pWeaponItem = pPed->GetInventory()->GetWeapon(TypeOfWeapon);
	if (pWeaponItem)
	{
		if (pWeaponItem->GetComponents())
		{
			const CWeaponItem::Components& components = *pWeaponItem->GetComponents();
			for (s32 i = components.GetCount()-1; i >= 0; i--)
			{
				if (components[i].pComponentInfo->GetIsClass<CWeaponComponentClipInfo>())
				{
					MaxAmountOfAmmoInClip = static_cast<const CWeaponComponentClipInfo*>(components[i].pComponentInfo)->GetClipSize();
					break;
				}
			}
		}
	}

	return MaxAmountOfAmmoInClip;
}

//
// name:		CommandGetAmmoInClip
// description:	How many bullets are in the clip
// return true if the weapon type is equipped false otherwise
bool CommandGetAmmoInClip(int PedIndex, int TypeOfWeapon, int &AmountOfAmmoInClip)
{
	const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);

	if(pPed)
	{
		if(scriptVerifyf(pPed->GetWeaponManager(), "%s:GET_AMMO_IN_CLIP - ped requires a weapon manager", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			AmountOfAmmoInClip = 0;
			if(pPed->GetWeaponManager()->GetEquippedWeapon() 
			&& pPed->GetWeaponManager()->GetEquippedWeapon()->GetWeaponHash() == (u32)TypeOfWeapon)
			{
				AmountOfAmmoInClip = pPed->GetWeaponManager()->GetEquippedWeapon()->GetAmmoInClip();
				return true;
			}
		}
	}
	return false;
}


//
// name:		CommandSetAmmoInClip
// description:	Give ammo to character
// return true if the weapon type is equipped false otherwise
bool CommandSetAmmoInClip(int PedIndex, int TypeOfWeapon, int NewAmmoInClip)
{
	CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

	if(pPed)
	{
		if(scriptVerifyf(pPed->GetWeaponManager(), "%s:SET_AMMO_IN_CLIP - ped requires a weapon manager", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			CWeapon* pWeaponUsable = pPed->GetWeaponManager()->GetEquippedWeapon();
			if (pWeaponUsable && pWeaponUsable->GetWeaponHash() == (u32)TypeOfWeapon)
			{
				const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(TypeOfWeapon);
				if (pWeaponInfo)
				{
					// Clamp to the maximum allowed in the clip
					int iMaxAmmoInClip = pWeaponInfo->GetClipSize();
					if (NewAmmoInClip > iMaxAmmoInClip)
					{
						NewAmmoInClip = iMaxAmmoInClip;
					}

					// Get the old amount of ammo in the clip
					int iOldAmmoInClip = pWeaponUsable->GetAmmoInClip();

					// Set the new amount of ammo in the clip
					pWeaponUsable->SetAmmoInClip(NewAmmoInClip);

					// Get the new amount of ammo. Just in case we got less than we requested for some reason
					int iAmmoInClip = pWeaponUsable->GetAmmoInClip();

					// Calculate how much ammo we added
					int iAmmoAdded = iAmmoInClip - iOldAmmoInClip;

					// Add the new ammo to the total
					if(pPed->GetInventory())
						pPed->GetInventory()->AddWeaponAmmo(TypeOfWeapon, iAmmoAdded);

					// We have specifically set ammo in the clip, so quit reloading if we are reloading
					switch(pWeaponUsable->GetState())
					{
					case CWeapon::STATE_RELOADING:
					case CWeapon::STATE_OUT_OF_AMMO:
						pWeaponUsable->SetState(CWeapon::STATE_READY);
						break;

					default:
						break;
					}

					return true;
				}
			}

			if(pPed->IsPlayer() && CNewHud::IsUsingWeaponWheel() && CNewHud::GetWheelDisableReload())
			{
				if((u32)TypeOfWeapon == CNewHud::GetWheelWeaponInHand())
				{
					CNewHud::SetWheelDisableReload(false);
				}
			}
		}
	}
	return false;
}

bool CommandGetMaxAmmo(int PedIndex, int TypeOfWeapon, int &MaxAmmo)
{
	const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);

	if(pPed)
	{
		const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(TypeOfWeapon);
		if (pWeaponInfo)
		{
			MaxAmmo = pWeaponInfo->GetAmmoMax(pPed);
			return true;
		}
	}
	return false;
}

bool CommandGetMaxAmmoByType(int PedIndex, int TypeOfAmmo, int &MaxAmmo)
{
	const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
	if(pPed)
	{
		const CAmmoInfo* pAmmoInfo = CWeaponInfoManager::GetInfo<CAmmoInfo>((u32)TypeOfAmmo);
		if (pAmmoInfo)
		{
			MaxAmmo = pAmmoInfo->GetAmmoMax(pPed);
			return true;
		}
	}
	return false;
}

void CommandAddPedAmmoByType(int PedIndex, int TypeOfAmmo, int AmountOfAmmo)
{
	CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>( PedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK );
	if( pPed )
	{
		CPedInventory* pPedInventory = pPed->GetInventory();
		if( scriptVerifyf( pPedInventory, "%s:ADD_PED_AMMO_BY_TYPE - ped requires a weapon manager", CTheScripts::GetCurrentScriptNameAndProgramCounter() ) )
		{
			pPedInventory->AddAmmo(TypeOfAmmo, AmountOfAmmo);
		}
	}
}

void CommandSetPedAmmoByType(int PedIndex, int TypeOfAmmo, int AmountOfAmmo)
{
	CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>( PedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK );
	if( pPed )
	{
		CPedInventory* pPedInventory = pPed->GetInventory();
		if( scriptVerifyf( pPedInventory, "%s:SET_PED_AMMO_BY_TYPE - ped requires a weapon manager", CTheScripts::GetCurrentScriptNameAndProgramCounter() ) )
		{
			pPedInventory->SetAmmo(TypeOfAmmo, AmountOfAmmo);
		}
	}
}

int CommandGetPedAmmoByType( int PedIndex, int TypeOfAmmo )
{
	const CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>( PedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK );
	if( pPed )
	{
		const CPedInventory* pPedInventory = pPed->GetInventory();
		if( scriptVerifyf( pPedInventory, "%s:GET_PED_AMMO_BY_TYPE - ped requires a weapon manager", CTheScripts::GetCurrentScriptNameAndProgramCounter() ) )
		{
			return pPedInventory->GetAmmo( TypeOfAmmo );
		}
	}

	return 0;
}

void CommandSetPedAmmoToDrop( int PedIndex, int AmmoAmount )
{
	const CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>( PedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK );
	if( pPed )
	{
		if( scriptVerifyf(pPed->GetNetworkObject(), "%s: SET_PED_AMMO_TO_DROP only works in MP at the moment", CTheScripts::GetCurrentScriptNameAndProgramCounter()) &&
			scriptVerifyf(AmmoAmount >= 0, "%s: SET_PED_AMMO_TO_DROP - AmmoAmount is negative", CTheScripts::GetCurrentScriptNameAndProgramCounter()) &&
			scriptVerifyf(AmmoAmount < 128, "%s: SET_PED_AMMO_TO_DROP - AmmoAmount is too large", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			static_cast<CNetObjPed*>(pPed->GetNetworkObject())->SetAmmoToDrop((u32)AmmoAmount);
		}
	}
}

void CommandSetPickupAmmoAmountScaler(float fScale )
{
	CPickupManager::SetPickupAmmoAmountScaler(fScale);
}

int CommandGetPedAmmoTypeFromWeapon( int PedIndex, int TypeOfWeapon )
{
	const CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>( PedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK );
	if( pPed )
	{
		const CPedInventory* pPedInventory = pPed->GetInventory();
		if( scriptVerifyf( pPedInventory, "%s:GET_PED_AMMO_TYPE_FROM_WEAPON - ped requires a weapon manager", CTheScripts::GetCurrentScriptNameAndProgramCounter() ) )
		{
			const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(TypeOfWeapon);
			if (pWeaponInfo)
			{
				if( scriptVerifyf(pWeaponInfo->GetAmmoInfo(pPed), "%s:GET_PED_AMMO_TYPE_FROM_WEAPON - the weapon [%s] does not have ammo info.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pWeaponInfo->GetName()))
				{
					return pWeaponInfo->GetAmmoInfo(pPed)->GetHash();
				}
			}
		}
	}

	return 0;
}

int CommandGetPedOriginalAmmoTypeFromWeapon( int PedIndex, int TypeOfWeapon )
{
	const CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>( PedIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK );
	if( pPed )
	{
		const CPedInventory* pPedInventory = pPed->GetInventory();
		if( scriptVerifyf( pPedInventory, "%s:GET_PED_ORIGINAL_AMMO_TYPE_FROM_WEAPON - ped requires a weapon manager", CTheScripts::GetCurrentScriptNameAndProgramCounter() ) )
		{
			const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(TypeOfWeapon);
			if (pWeaponInfo)
			{
				if( scriptVerifyf(pWeaponInfo->GetAmmoInfo(), "%s:GET_PED_ORIGINAL_AMMO_TYPE_FROM_WEAPON - the weapon [%s] does not have ammo info.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pWeaponInfo->GetName()))
				{
					return pWeaponInfo->GetAmmoInfo()->GetHash();
				}
			}
		}
	}

	return 0;
}

bool CommandGetPedLastWeaponImpactCoord (int PedIndex, Vector3 &vImpactCoord)
{
	bool bValidImpact = false;
	vImpactCoord.Zero();
	
	const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);

	if (pPed)
	{
		// check that the weapon was fired in the previous frame, this is used rather than the current frame as the weapon firing is
		// generally carried out in the post-physics update (at the end of the frame) and the script update (where this fn is called) happens
		// at the beginning of the frame.
		if(scriptVerifyf(pPed->GetWeaponManager(), "%s:GET_PED_LAST_WEAPON_IMPACT_COORD - ped requires a weapon manager", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			int iTimeBetweenImpact = fwTimer::GetPrevElapsedTimeInMilliseconds() - pPed->GetWeaponManager()->GetLastWeaponImpactTime();  

			if (iTimeBetweenImpact ==  0)
			{
				vImpactCoord = pPed->GetWeaponManager()->GetLastWeaponImpactPos();
				bValidImpact = true;
			}
		}
	}

	return bValidImpact;
}

void CommandSetPedGadget(int PedIndex, int GadgetHash, bool bEquip)
{
	CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

	if(pPed)
	{
		if(scriptVerifyf(pPed->GetInventory(), "%s:SET_PED_GADGET - ped requires an inventory", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			if(pPed->GetInventory()->GetWeapon(GadgetHash))
			{
				if(scriptVerifyf(pPed->GetWeaponManager(), "%s:SET_PED_GADGET - ped requires a weapon manager", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
				{
					pPed->GetWeaponManager()->EquipWeapon(GadgetHash, -1, bEquip);
				}
			}
			else
			{
				scriptAssertf(false, "%s:Ped does not have the specified gadget", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			}
		}
	}
}

s32 CommandGetSelectedPedWeapon(int PedIndex)
{
	const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
	if(pPed)
	{
		if(scriptVerifyf(pPed->GetWeaponManager(), "%s:GET_SELECTED_PED_WEAPON - ped requires a weapon manager", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			return static_cast<s32>(pPed->GetWeaponManager()->GetSelectedWeaponHash());
		}
	}
	return 0;
}

bool CommandGetIsPedGadgetEquipped(int PedIndex, int GadgetHash)
{
	const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
	if(pPed)
	{
		if(scriptVerifyf(pPed->GetWeaponManager(), "%s:GET_IS_PED_GADGET_EQUIPPED - ped requires a weapon manager", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			return pPed->GetWeaponManager()->GetIsGadgetEquipped(GadgetHash);
		}
	}

	return false;
}

void CommandExplodeProjectiles(int PedIndex, int WeaponType, bool bInstant)
{
	CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
	if(pPed)
	{
		// Get the projectile type from the WeaponType
		const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(WeaponType);
		if(scriptVerifyf(pWeaponInfo, "%s: WeaponType [%d] isn't valid", CTheScripts::GetCurrentScriptNameAndProgramCounter(), WeaponType))
		{
			if(scriptVerifyf(pWeaponInfo->GetFireType() == FIRE_TYPE_PROJECTILE, "%s: WeaponType [%s] isn't a projectile weapon", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pWeaponInfo->GetName()))
			{
				const CItemInfo* pAmmoInfo = pWeaponInfo->GetAmmoInfo();
				if(scriptVerifyf(pAmmoInfo, "%s: WeaponType [%s] doesn't have ammo", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pWeaponInfo->GetName()))
				{
					if(scriptVerifyf(pAmmoInfo->GetIsClassId(CAmmoProjectileInfo::GetStaticClassId()), "%s: WeaponType [%s] Ammo [%s] isn't a projectile type", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pWeaponInfo->GetName(), pAmmoInfo->GetName()))
					{
						CProjectileManager::ExplodeProjectiles(pPed, pAmmoInfo->GetHash(), bInstant);
					}
				}
			}
		}
	}
}

void CommandRemoveAllProjectilesOfType(int WeaponType, bool bExplode)
{
	// Get the projectile type from the WeaponType
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(WeaponType);
	if(scriptVerifyf(pWeaponInfo, "%s: WeaponType [%d] isn't valid", CTheScripts::GetCurrentScriptNameAndProgramCounter(), WeaponType))
	{
		const CItemInfo* pAmmoInfo = pWeaponInfo->GetAmmoInfo();
		if(scriptVerifyf(pAmmoInfo, "%s: WeaponType [%s] doesn't have ammo", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pWeaponInfo->GetName()))
		{
			if(scriptVerifyf(pAmmoInfo->GetIsClassId(CAmmoProjectileInfo::GetStaticClassId()), "%s: WeaponType [%s] Ammo [%s] isn't a projectile type", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pWeaponInfo->GetName(), pAmmoInfo->GetName()))
			{
				CProjectileManager::RemoveAllProjectilesByAmmoType(pAmmoInfo->GetHash(), bExplode);
			}
		}
	}
}

void CommandActivateDetonator(int PedIndex, bool Active)
{
	CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
	if(pPed)
	{
		if(scriptVerifyf(pPed->GetWeaponManager(), "%s:ACTIVATE_DETONATOR - ped requires a weapon manager", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			pPed->GetWeaponManager()->SetDetonatorActive(Active);
		}
	}
}

bool CommandGetIsDetonatorFired(int PedIndex)
{
	const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
	if(pPed)
	{
		if(scriptVerifyf(pPed->GetWeaponManager(), "%s:GET_IS_DETONATOR_FIRED - ped requires a weapon manager", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
			if(pWeapon)
			{
				const u32 WEAPON_BOMB_DETONATOR = ATSTRINGHASH("WEAPON_BOMB_DETONATOR", 0x08613f94c);
				if(pWeapon->GetWeaponHash() == WEAPON_BOMB_DETONATOR)
				{
					const CTaskDetonator* pTask = static_cast<const CTaskDetonator*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_DETONATOR));
					if(pTask && pTask->GetHasFired())
					{
						return true;
					}
				}
			}
		}
	}

	return false;
}

bool CommandGetIsStickyCamViewerFired(int PedIndex)
{
	const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
	if(pPed)
	{
		if(scriptVerifyf(pPed->GetWeaponManager(), "%s:GET_IS_STICKY_CAM_VIEWER_FIRED - ped requires a weapon manager", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
			if(pWeapon)
			{
				const u32 GADGET_STICKY_CAM_VIEWER = ATSTRINGHASH("GADGET_STICKY_CAM_VIEWER", 0x0db77c490);
				if(pWeapon->GetWeaponHash() == GADGET_STICKY_CAM_VIEWER)
				{
					const CTaskDetonator* pTask = static_cast<const CTaskDetonator*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_DETONATOR));
					if(pTask && pTask->GetHasFired())
					{
						return true;
					}
				}
			}
		}
	}

	return false;
}

float CommandGetLockOnDistanceOfCurrentPedWeapon(int PedIndex)
{
	const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
	if(pPed)
	{
		if(scriptVerifyf(pPed->GetWeaponManager(), "%s:GET_LOCKON_DISTANCE_OF_CURRENT_PED_WEAPON - ped requires a weapon manager", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
			if(pWeapon)
			{
				const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(pWeapon->GetWeaponHash());
				if(pWeaponInfo)
				{
					return pWeaponInfo->GetLockOnRange();
				}
			}
		}
	}

	return -1.0f;
}

float CommandGetMaxRangeOfCurrentPedWeapon(int PedIndex)
{
	const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
	if(pPed)
	{
		if(scriptVerifyf(pPed->GetWeaponManager(), "%s:GET_MAX_RANGE_OF_CURRENT_PED_WEAPON - ped requires a weapon manager", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
			if(pWeapon)
			{
				const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(pWeapon->GetWeaponHash());
				if(pWeaponInfo)
				{
					return pWeaponInfo->GetRange();
				}
			}
		}
	}

	return -1.0f;
}

bool CommandHasVehicleGotProjectileAttached(int PedIndex, int VehicleIndex, int WeaponType, int DoorNumber)
{
	const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
	if(pPed)
	{
		const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(VehicleIndex);

		if(pVehicle)
		{
			u32 uProjectileHash = 0;

			// Get the projectile hash from the WeaponType, if one is specified
			if(WeaponType != 0)
			{
				const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(WeaponType);
				if(scriptVerifyf(pWeaponInfo, "%s: WeaponType [%d] isn't valid", CTheScripts::GetCurrentScriptNameAndProgramCounter(), WeaponType))
				{
					if(scriptVerifyf(pWeaponInfo->GetFireType() == FIRE_TYPE_PROJECTILE, "%s: WeaponType [%s] isn't a projectile weapon", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pWeaponInfo->GetName()))
					{
						const CItemInfo* pAmmoInfo = pWeaponInfo->GetAmmoInfo();
						if(scriptVerifyf(pAmmoInfo, "%s: WeaponType [%s] doesn't have ammo", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pWeaponInfo->GetName()))
						{
							if(scriptVerifyf(pAmmoInfo->GetIsClassId(CAmmoProjectileInfo::GetStaticClassId()), "%s: WeaponType [%s] Ammo [%s] isn't a projectile type", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pWeaponInfo->GetName(), pAmmoInfo->GetName()))
							{
								uProjectileHash = pAmmoInfo->GetHash();
							}
						}
					}
				}
			}

			s32 iDoorBone = -1;

			// Get the component from the door number, if one is specified
			if(DoorNumber != SC_DOOR_INVALID)
			{
				eHierarchyId nDoorId = CCarDoor::GetScriptDoorId(eScriptDoorList(DoorNumber));
				iDoorBone = pVehicle->GetBoneIndex(nDoorId);
			}

			return CProjectileManager::GetIsProjectileAttachedToEntity(pPed, pVehicle, uProjectileHash, iDoorBone);
		}
	}

	return false;
}

void CommandGiveWeaponComponentToPed(int PedIndex, int TypeOfWeapon, int TypeOfComponent)
{
	CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
	if(pPed)
	{
		if(scriptVerifyf(pPed->GetInventory(), "%s:GIVE_WEAPON_COMPONENT_TO_PED - ped requires a weapon manager", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			if(scriptVerifyf(pPed->GetInventory()->GetWeaponRepository().AddComponent(TypeOfWeapon, TypeOfComponent), "%s:GIVE_WEAPON_COMPONENT_TO_PED - Failed to give weapon component to ped", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
			{
#if !__FINAL
				weaponDebugf1("%s:GIVE_WEAPON_COMPONENT_TO_PED: [%d][%s], [%d][%s]: Ped 0x%p [%s] [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), TypeOfWeapon, atHashString(TypeOfWeapon).TryGetCStr(), TypeOfComponent, atHashString(TypeOfComponent).TryGetCStr(), pPed, pPed->GetDebugName(), PedIndex);
				scrThread::PrePrintStackTrace();
#endif
			}
		}
	}
}

void CommandRemoveWeaponComponentFromPed(int PedIndex, int TypeOfWeapon, int TypeOfComponent)
{
	CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

	if(pPed)
	{	
		if(scriptVerifyf(pPed->GetInventory(), "%s:REMOVE_WEAPON_COMPONENT_FROM_PED - ped requires a weapon manager", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			if(scriptVerifyf(pPed->GetInventory()->GetWeaponRepository().RemoveComponent(TypeOfWeapon, TypeOfComponent), "%s:REMOVE_WEAPON_COMPONENT_FROM_PED - Failed to remove weapon component from ped", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
			{	
#if !__FINAL
				weaponDebugf1("%s:REMOVE_WEAPON_COMPONENT_FROM_PED: [%d][%s], [%d][%s]: Ped 0x%p [%s] [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), TypeOfWeapon, atHashString(TypeOfWeapon).TryGetCStr(), TypeOfComponent, atHashString(TypeOfComponent).TryGetCStr(), pPed, pPed->GetDebugName(), PedIndex);
				scrThread::PrePrintStackTrace();
#endif
			}
		}	
	}
}

bool CommandHasPedGotWeaponComponent(int PedIndex, int TypeOfWeapon, int TypeOfComponent)
{
	const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);

	if(pPed)
	{
		if(scriptVerifyf(pPed->GetInventory(), "%s:HAS_PED_GOT_WEAPON_COMPONENT - ped requires a weapon manager", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			const CWeaponItem* pWeaponItem = pPed->GetInventory()->GetWeapon(TypeOfWeapon);
			if(pWeaponItem)
			{
				if(pWeaponItem->GetComponents())
				{
					const CWeaponItem::Components& components = *pWeaponItem->GetComponents();
					for(s32 i = 0; i < components.GetCount(); i++)
					{
						if(components[i].pComponentInfo->GetHash() == (u32)TypeOfComponent)
						{
							return true;
						}
					}
				}
			}
		}
	}
	return false;
}

bool CommandIsPedWeaponComponentActive(int PedIndex, int TypeOfWeapon, int TypeOfComponent)
{
	const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);

	if(pPed)
	{
		if(scriptVerifyf(pPed->GetInventory(), "%s:IS_PED_WEAPON_COMPONENT_ACTIVE - ped requires a weapon manager", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			const CWeaponItem* pWeaponItem = pPed->GetInventory()->GetWeapon(TypeOfWeapon);
			if(pWeaponItem)
			{
				if(pWeaponItem->GetComponents())
				{
					const CWeaponItem::Components& components = *pWeaponItem->GetComponents();
					for(s32 i = 0; i < components.GetCount(); i++)
					{
						if(components[i].pComponentInfo->GetHash() == (u32)TypeOfComponent)
						{
							return components[i].m_bActive;
						}
					}
				}
			}
		}
	}
	return false;
}

bool CommandGetPedCurrentWeaponTimeBeforeNextShot(int PedIndex, int &TimeBeforeNextShot)
{
	const CPed *pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);

	if(pPed)
	{
		const CPedWeaponManager* pWeaponManager = pPed->GetWeaponManager();
		if(pWeaponManager)
		{
			const CWeapon* pEquippedWeapon = pWeaponManager->GetEquippedWeapon();
			if(pEquippedWeapon)
			{
				f32 fTimeBeforeNextShot = pEquippedWeapon->GetTimeBeforeNextShot();
				TimeBeforeNextShot = static_cast<s32>(fTimeBeforeNextShot * 1000.f);
				return true;
			}
		}
	}
	return false;
}

bool CommandRefillAmmoInstant(int PedIndex)
{
	CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

	if(pPed)
	{
		scriptAssertf(pPed->GetWeaponManager(), "%s: REFILL_AMMO_INSTANTLY - ped requires a weapon manager", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		return CWeapon::RefillPedsWeaponAmmoInstant(*pPed);		
	}
	return false;
}

bool CommandMakePedReload(int PedIndex)
{
	CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

	if(pPed && scriptVerifyf(pPed->IsPlayer(), "%s: MAKE_PED_RELOAD - not implemented for AI peds yet", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
	{
		CPedWeaponManager* pWeaponMgr = pPed->GetWeaponManager();
		if(scriptVerifyf(pWeaponMgr, "%s: MAKE_PED_RELOAD - ped requires a weapon manager", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			CWeapon* pWeapon = pWeaponMgr->GetEquippedWeapon();
			if(pWeapon && pWeapon->GetCanReload())
			{
				pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_ForceReload, true);
				return true;
			}
		}
	}

	return false;
}

void RequestWeaponAsset( int iWeaponHash, int iWeaponRequestFlags, int iExtraComponentFlags )
{
	if( scriptVerifyf( iWeaponHash != 0, "REQUEST_WEAPON_ASSET - %s Weapon type [%d] is invalid", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iWeaponHash ) )
	{
		const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>( iWeaponHash );
		if( scriptVerifyf( pWeaponInfo, "REQUEST_WEAPON_ASSET - %s Weapon type [%d] does not exist in data", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iWeaponHash ) )
		{
			CScriptResource_Weapon_Asset weaponAsset( iWeaponHash, iWeaponRequestFlags, iExtraComponentFlags );
			CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResource( weaponAsset );
		}
	}
}

bool HasWeaponAssetLoaded(int weaponHash)
{
	if(scriptVerifyf(weaponHash != 0, "HAS_WEAPON_ASSET_LOADED - %s Weapon type [%d] is invalid", CTheScripts::GetCurrentScriptNameAndProgramCounter(), weaponHash))
	{
		const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(weaponHash);
		if(scriptVerifyf(pWeaponInfo, "HAS_WEAPON_ASSET_LOADED - %s Weapon type [%d] does not exist in data", CTheScripts::GetCurrentScriptNameAndProgramCounter(), weaponHash))
		{
			if(CTheScripts::GetCurrentGtaScriptHandler())
			{
				scriptResource* pScriptResource = CTheScripts::GetCurrentGtaScriptHandler()->GetScriptResource(CGameScriptResource::SCRIPT_RESOURCE_WEAPON_ASSET, weaponHash);
				if(scriptVerifyf(pScriptResource, "HAS_WEAPON_ASSET_LOADED - %s has not requested %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pWeaponInfo->GetName()))
				{
					if(scriptVerifyf(pScriptResource->GetType() == CGameScriptResource::SCRIPT_RESOURCE_WEAPON_ASSET, "HAS_WEAPON_ASSET_LOADED - %s incorrect type of resource %d", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pScriptResource->GetType()))
					{
						CScriptResource_Weapon_Asset* pWeaponAsset = static_cast<CScriptResource_Weapon_Asset*>(pScriptResource);
						CWeaponScriptResource* pWeaponResource = CWeaponScriptResourceManager::GetResource(pWeaponAsset->GetReference());
						if(scriptVerifyf(pWeaponResource, "HAS_WEAPON_ASSET_LOADED - %s NULL resource", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
						{
							return pWeaponResource->GetIsStreamedIn();
						}
					}
				}
			}
		}
	}

	return false;
}

void RemoveWeaponAsset( int iWeaponHash )
{
	if( scriptVerifyf( iWeaponHash != 0, "REMOVE_WEAPON_ASSET - %s Weapon type [%d] is invalid", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iWeaponHash ) )
	{
		const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>( iWeaponHash );
		if( scriptVerifyf( pWeaponInfo, "REMOVE_WEAPON_ASSET - %s Weapon type [%d] does not exist in data", CTheScripts::GetCurrentScriptNameAndProgramCounter(), iWeaponHash ) )
		{
			CTheScripts::GetCurrentGtaScriptHandler()->RemoveScriptResource( CGameScriptResource::SCRIPT_RESOURCE_WEAPON_ASSET, iWeaponHash );
		}
	}
}

bool CommandHasPedGotWeaponManager(int PedIndex)
{
	CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);
	if(pPed)
	{
		return pPed->GetWeaponManager() != NULL;
	}
	return false;
}

int CommandCreateWeaponObject(int TypeOfWeapon, int AmountOfAmmo, const scrVector & scrVecNewCoors, bool CreateDefaultComponents, float fScale, int ModelHash, bool RegisterAsNetworkObject, bool ScriptHostObject)
{
	Matrix34 m(Matrix34::IdentityType);
	m.d = Vector3(scrVecNewCoors);

	CObject* pWeaponObject = CPedEquippedWeapon::CreateObject(TypeOfWeapon, AmountOfAmmo, m, CreateDefaultComponents, NULL, ModelHash, NULL,  fScale, true);
	if(scriptVerifyf(pWeaponObject, "CREATE_WEAPON_OBJECT - %s Failed to create weapon type [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), TypeOfWeapon))
	{
		// non-networked objects should always be treated as client objects
		if (!RegisterAsNetworkObject) 
			ScriptHostObject = false;

		if(RegisterAsNetworkObject && NetworkInterface::IsGameInProgress())
		{
			if(!NetworkInterface::RegisterObject(pWeaponObject, 0, CNetObjGame::GLOBALFLAG_SCRIPTOBJECT, true))
			{
				gnetAssertf(0, "Failed to create network object for weapon type:%d", TypeOfWeapon);
			}
		}

		CTheScripts::RegisterEntity(pWeaponObject, ScriptHostObject, RegisterAsNetworkObject);

		int ObjectIndex = CTheScripts::GetGUIDFromEntity(*pWeaponObject);

#if !__FINAL
		weaponDebugf1("%s:CREATE_WEAPON_OBJECT: [%d][%s], Ammo:%d: Object 0x%p [%s] [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), TypeOfWeapon, atHashString(TypeOfWeapon).TryGetCStr(), AmountOfAmmo, pWeaponObject, pWeaponObject->GetModelName(), ObjectIndex);
		scrThread::PrePrintStackTrace();
#endif

		return ObjectIndex;
	}

	return 0;
}

void CommandGiveWeaponComponentToWeaponObject(int ObjectIndex, int TypeOfComponent)
{
	CObject* pObject = CTheScripts::GetEntityToModifyFromGUID<CObject>(ObjectIndex);
	if(pObject)
	{
		if(scriptVerifyf(CPedEquippedWeapon::CreateWeaponComponent(TypeOfComponent, pObject), "GIVE_WEAPON_COMPONENT_TO_WEAPON_OBJECT - %s Failed to create weapon component type [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), TypeOfComponent))
		{
#if !__FINAL
			weaponDebugf1("%s:GIVE_WEAPON_COMPONENT_TO_WEAPON_OBJECT: [%d][%s]: Object 0x%p [%s] [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), TypeOfComponent, atHashString(TypeOfComponent).TryGetCStr(), pObject, pObject->GetModelName(), ObjectIndex);
			scrThread::PrePrintStackTrace();
#endif
		}
	}
}

void CommandRemoveWeaponComponentFromWeaponObject(int ObjectIndex, int TypeOfComponent)
{
	CObject* pObject = CTheScripts::GetEntityToModifyFromGUID<CObject>(ObjectIndex);
	if(pObject)
	{
		if(scriptVerifyf(pObject->GetWeapon(), "REMOVE_WEAPON_COMPONENT_FROM_WEAPON_OBJECT - %s Object is not a weapon object", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			CWeaponComponent* pComponentToRemove = NULL;
			const CWeapon::Components& components = pObject->GetWeapon()->GetComponents();
			for(s32 i = 0; i < components.GetCount(); i++)
			{
				if(components[i]->GetInfo()->GetHash() == (u32)TypeOfComponent)
				{
					pComponentToRemove = components[i];
					break;
				}
			}

			if(scriptVerifyf(pComponentToRemove, "REMOVE_WEAPON_COMPONENT_FROM_WEAPON_OBJECT - %s Object does not contain component", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
			{
				CPedEquippedWeapon::DestroyWeaponComponent(pObject, pComponentToRemove);

#if !__FINAL
				weaponDebugf1("%s:REMOVE_WEAPON_COMPONENT_FROM_WEAPON_OBJECT: [%d][%s]: Object 0x%p [%s] [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), TypeOfComponent, atHashString(TypeOfComponent).TryGetCStr(), pObject, pObject->GetModelName(), ObjectIndex);
				scrThread::PrePrintStackTrace();
#endif
			}
		}
	}
}

bool CommandHasWeaponGotWeaponComponent(int ObjectIndex, int TypeOfComponent)
{
	CObject* pObject = CTheScripts::GetEntityToModifyFromGUID<CObject>(ObjectIndex);
	if(pObject)
	{
		if(scriptVerifyf(pObject->GetWeapon(), "HAS_WEAPON_GOT_WEAPON_COMPONENT - %s Object [%s][0x%p][%d] has no weapon, is pickup [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pObject->GetModelName(), pObject, ObjectIndex, pObject->GetIsClass<CPickup>()))
		{
			const CWeapon::Components& components = pObject->GetWeapon()->GetComponents();
			for(s32 i = 0; i < components.GetCount(); i++)
			{
				if(components[i]->GetInfo()->GetHash() == (u32)TypeOfComponent)
				{
					return true;
				}
			}
		}
	}

	return false;
}

const char* CommandGetWeaponComponentName(int uHashOfComponent)
{
	const CWeaponComponentInfo* pComponentInfo = CWeaponComponentManager::GetInfo(uHashOfComponent);
	if(scriptVerifyf(pComponentInfo, "GET_WEAPON_COMPONENT_NAME - Component with Hash %i (0x%08x) could not be found!", uHashOfComponent, uHashOfComponent) )
	{
		return pComponentInfo->GetLocKey().GetCStr();
	}

	return "WCT_INVALID";
}

const char* CommandGetWeaponComponentDescription(int uHashOfComponent)
{
	const CWeaponComponentInfo* pComponentInfo = CWeaponComponentManager::GetInfo(uHashOfComponent);
	if(scriptVerifyf(pComponentInfo, "GET_WEAPON_COMPONENT_DESCRIPTION - Component with Hash %i (0x%08x) could not be found!", uHashOfComponent, uHashOfComponent) )
	{
		return pComponentInfo->GetLocDesc().GetCStr();
	}

	return "WCD_INVALID";
}

void CommandGiveWeaponObjectToPed(int ObjectIndex, int PedIndex)
{
	CObject* pObject = CTheScripts::GetEntityToModifyFromGUID<CObject>(ObjectIndex);
	if(pObject)
	{
		if(scriptVerifyf(pObject->GetWeapon(), "GIVE_WEAPON_OBJECT_TO_PED - %s Object [%s] has no weapon", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pObject->GetModelName()))
		{
			CWeapon* pWeapon = pObject->GetWeapon();

			CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
			if(pPed)
			{
				if(scriptVerifyf(pPed->GetInventory(), "GIVE_WEAPON_OBJECT_TO_PED - %s Ped [%s] has no inventory", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pPed->GetDebugName()))
				{
					CWeaponItem* pWeaponItem = pPed->GetInventory()->AddWeaponAndAmmo(pWeapon->GetWeaponHash(), pWeapon->GetAmmoTotal());
					if(scriptVerifyf(pWeaponItem, "GIVE_WEAPON_OBJECT_TO_PED - %s Failed to give weapon to ped", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
					{
						const CWeapon::Components& components = pWeapon->GetComponents();
						for(s32 i = 0; i < components.GetCount(); i++)
						{
							if(scriptVerifyf(pPed->GetInventory()->GetWeaponRepository().AddComponent(pWeapon->GetWeaponHash(), components[i]->GetInfo()->GetHash()), "GIVE_WEAPON_OBJECT_TO_PED - %s Failed to give weapon component to ped", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
							{
							}
						}

						// Copy over the tint index
						pWeaponItem->SetTintIndex(pWeapon->GetTintIndex());

#if !__FINAL
						weaponDebugf1("%s:GIVE_WEAPON_OBJECT_TO_PED: Ped 0x%p [%s] [%d], Object 0x%p [%s] [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pPed, pPed->GetDebugName(), PedIndex, pObject, pObject->GetModelName(), ObjectIndex);
						scrThread::PrePrintStackTrace();
#endif

						if(scriptVerifyf(pPed->GetWeaponManager(), "GIVE_WEAPON_OBJECT_TO_PED - %s Ped [%s] has no weapon manager", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pPed->GetDebugName()))
						{
							if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle))
							{
								// Ensure current weapon is stored as a backup so we can restore it when getting out
								pPed->GetWeaponManager()->SetBackupWeapon(pWeapon->GetWeaponHash());
								pPed->GetWeaponManager()->EquipBestWeapon();
							}
							else if(pPed->GetIsSwimming() || pPed->GetPedResetFlag(CPED_RESET_FLAG_IsClimbing))
							{
								// Ensure current weapon is stored as a backup so we can restore it when getting out
								pPed->GetWeaponManager()->SetBackupWeapon(pWeapon->GetWeaponHash());
							}
							else
							{
								if(pPed->GetWeaponManager()->EquipWeapon(pWeapon->GetWeaponHash(), -1, true))
								{
									CObject* pWeaponObject = pPed->GetWeaponManager()->GetEquippedWeaponObject();
									if(pWeaponObject && pWeaponObject->GetWeapon() && pWeaponObject->GetWeapon()->GetFlashLightComponent() && pWeapon->GetFlashLightComponent())
									{
										pWeaponObject->GetWeapon()->GetFlashLightComponent()->TransferActiveness(pWeapon->GetFlashLightComponent());
									}
								}
							}
						}
					}
				}
			}
		}

		CPedEquippedWeapon::DestroyObject(pObject, NULL);
	}
}

bool CommandDoesWeaponTakeWeaponComponent(int TypeOfWeapon, int TypeOfComponent)
{
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(TypeOfWeapon);
	if(scriptVerifyf(pWeaponInfo, "DOES_WEAPON_TAKE_WEAPON_COMPONENT - %s pWeaponInfo is NULL for weapon [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), TypeOfWeapon))
	{
		const CWeaponComponentInfo* pComponentInfo = CWeaponComponentManager::GetInfo(TypeOfComponent);
		if(scriptVerifyf(pComponentInfo, "DOES_WEAPON_TAKE_WEAPON_COMPONENT - %s pComponentInfo is NULL for component [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), TypeOfComponent))
		{
			return pWeaponInfo->GetAttachPoint(pComponentInfo) != NULL;
		}
	}

	return false;
}

int CommandGetWeaponObjectFromPed(int PedIndex, bool DoDeadCheck)
{
	unsigned assertFlags = CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES;
	if(!DoDeadCheck)
	{
		assertFlags = CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES_NO_DEAD_CHECK;
	}

	CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, assertFlags);
	if(pPed)
	{
		if(scriptVerifyf(pPed->GetWeaponManager(), "%s:GET_WEAPON_OBJECT_FROM_PED - ped requires a weapon manager", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			CObject* pWeaponObject = pPed->GetWeaponManager()->GetEquippedWeaponObject(); 
			if(pWeaponObject)
			{
				CObject* pDroppedWeaponObject = pPed->GetWeaponManager()->DropWeapon(pPed->GetWeaponManager()->GetEquippedWeaponHash(), false, false);

				if(scriptVerifyf(pDroppedWeaponObject, "%s:GET_WEAPON_OBJECT_FROM_PED - failed to get object from weapon", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
				{
					pDroppedWeaponObject->SetOwnedBy(ENTITY_OWNEDBY_SCRIPT);
					CTheScripts::RegisterEntity(pDroppedWeaponObject);
					return CTheScripts::GetGUIDFromEntity(*pDroppedWeaponObject);
				}
			}	
		}
	}
	return 0;
}

void CommandDropWeaponObjectOfPed(int PedIndex, bool DoDeadCheck)
{
	unsigned assertFlags = CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES;
	if (!DoDeadCheck)
	{
		assertFlags = CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES_NO_DEAD_CHECK;
	}

	CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex, assertFlags);
	if (pPed && !pPed->IsNetworkClone())
	{
		if (scriptVerifyf(pPed->GetWeaponManager(), "%s:DROP_WEAPON_OBJECT_OF_PED - ped requires a weapon manager", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			CObject* pWeaponObject = pPed->GetWeaponManager()->GetEquippedWeaponObject();
			if (pWeaponObject)
			{
				CObject* pDroppedWeaponObject = pPed->GetWeaponManager()->DropWeapon(pPed->GetWeaponManager()->GetEquippedWeaponHash(), false, false, false, true);
				pPed->GetWeaponManager()->GetWeaponSelector()->ClearSelectedWeapons();

				if (scriptVerifyf(pDroppedWeaponObject, "%s:DROP_WEAPON_OBJECT_OF_PED - failed to get object from weapon", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
				{
					if (pPed->GetNetworkObject())
					{
						SafeCast(CNetObjPed, pPed->GetNetworkObject())->SetHasDroppedWeapon(true);
					}
				}
			}
		}
	}
}


void CommandGiveLoadOutToPed(int PedIndex, int LoadOut)
{
	CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
	if(pPed)
	{
		CPedInventoryLoadOutManager::SetLoadOut(pPed, (u32)LoadOut);
	}
}

void CommandSetLoadOutAlias(int OriginalLoadOut, int NewLoadOut)
{
	CPedInventoryLoadOutManager::AddLoadOutAlias((u32)OriginalLoadOut, (u32)NewLoadOut);
}

void CommandClearLoadOutAlias(int LoadOut)
{
	CPedInventoryLoadOutManager::RemoveLoadOutAlias((u32)LoadOut);
}

void CommandSetPedWeaponTintIndex(int PedIndex, int TypeOfWeapon, int TintIndex)
{
	CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
	if(pPed)
	{
		if(scriptVerifyf(pPed->GetInventory(), "%s:SET_PED_WEAPON_TINT_INDEX - ped requires an inventory", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			CWeaponItem* pWeaponItem = pPed->GetInventory()->GetWeapon(TypeOfWeapon);
			if(scriptVerifyf(pWeaponItem, "%s:SET_PED_WEAPON_TINT_INDEX - ped does not have weapon [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), TypeOfWeapon))
			{
				pWeaponItem->SetTintIndex((u8)TintIndex);

				//! Update tint on active weapon.
				if(pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetEquippedWeapon())
				{
					if(pPed->GetWeaponManager()->GetEquippedWeapon()->GetWeaponHash() == (u32)TypeOfWeapon)
					{
						pPed->GetWeaponManager()->GetEquippedWeapon()->UpdateShaderVariables((u8)TintIndex);
					}
				}
			}
		}
	}
}

int CommandGetPedWeaponTintIndex(int PedIndex, int TypeOfWeapon)
{
	const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
	if(pPed)
	{
		if(scriptVerifyf(pPed->GetInventory(), "%s:GET_PED_WEAPON_TINT_INDEX - ped requires an inventory", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			const CWeaponItem* pWeaponItem = pPed->GetInventory()->GetWeapon(TypeOfWeapon);
			if(scriptVerifyf(pWeaponItem, "%s:GET_PED_WEAPON_TINT_INDEX - ped does not have weapon [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), TypeOfWeapon))
			{
				return pWeaponItem->GetTintIndex();
			}
		}
	}
	return -1;
}

void CommandSetPedWeaponComponentTintIndex(int PedIndex, int TypeOfWeapon, int TypeOfComponent, int TintIndex)
{
	if (scriptVerifyf(TintIndex >= 0, "%s:SET_PED_WEAPON_COMPONENT_TINT_INDEX - Invalid tint index [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), TintIndex))
	{
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
		if (pPed)
		{
			if (scriptVerifyf(pPed->GetInventory(), "%s:SET_PED_WEAPON_COMPONENT_TINT_INDEX - ped requires an inventory", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
			{
				CWeaponItem* pWeaponItem = pPed->GetInventory()->GetWeapon(TypeOfWeapon);
				if (scriptVerifyf(pWeaponItem, "%s:SET_PED_WEAPON_COMPONENT_TINT_INDEX - ped does not have weapon [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), TypeOfWeapon))
				{
					// Update tint on weapon item
					if (pWeaponItem->GetComponents())
					{
						const CWeaponItem::Components& components = *pWeaponItem->GetComponents();
						for (s32 i = 0; i < components.GetCount(); i++)
						{
							if (components[i].pComponentInfo->GetHash() == (u32)TypeOfComponent)
							{
								pWeaponItem->SetComponentTintIndex(i, (u8)TintIndex);
							}
						}
					}

					// Update component tint on active weapon
					if (pPed->GetWeaponManager())
					{
						CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
						if (pWeapon && pWeapon->GetWeaponHash() == (u32)TypeOfWeapon)
						{
							const CWeapon::Components& components = pWeapon->GetComponents();
							for (s32 i = 0; i < components.GetCount(); i++)
							{
								if (components[i]->GetInfo()->GetHash() == (u32)TypeOfComponent)
								{
									components[i]->UpdateShaderVariables((u8)TintIndex);
								}
							}
						}
					}
				}
			}
		}
	}
}

int CommandGetPedWeaponComponentTintIndex(int PedIndex, int TypeOfWeapon, int TypeOfComponent)
{
	const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex);
	if(pPed)
	{
		if(scriptVerifyf(pPed->GetInventory(), "%s:GET_PED_WEAPON_COMPONENT_TINT_INDEX - ped requires an inventory", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			const CWeaponItem* pWeaponItem = pPed->GetInventory()->GetWeapon(TypeOfWeapon);
			if(scriptVerifyf(pWeaponItem, "%s:GET_PED_WEAPON_COMPONENT_TINT_INDEX - ped does not have weapon [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), TypeOfWeapon))
			{
				if(pWeaponItem->GetComponents())
				{
					const CWeaponItem::Components& components = *pWeaponItem->GetComponents();
					for(s32 i = 0; i < components.GetCount(); i++)
					{
						if(components[i].pComponentInfo->GetHash() == (u32)TypeOfComponent)
						{
							return components[i].m_uTintIndex;
						}
					}
				}
			}
		}
	}
	return -1;
}

void CommandSetPedWeaponCamoIndex(int PedIndex, int TypeOfWeapon, int CamoIdx)
{
	CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
	if(pPed)
	{
		if(scriptVerifyf(pPed->GetInventory(), "%s:SET_PED_WEAPON_CAMO_INDEX - ped requires an inventory", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			CWeaponItem* pWeaponItem = pPed->GetInventory()->GetWeapon(TypeOfWeapon);
			if(scriptVerifyf(pWeaponItem, "%s:SET_PED_WEAPON_CAMO_INDEX - ped does not have weapon [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), TypeOfWeapon))
			{
				if(pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetEquippedWeapon())
				{
					if(pPed->GetWeaponManager()->GetEquippedWeapon()->GetWeaponHash() == (u32)TypeOfWeapon)
					{
						pPed->GetWeaponManager()->GetEquippedWeapon()->SetCamoDiffuseTexIdx(CamoIdx);
					}
				}
			}
		}
	}
}

int CommandGetPedWeaponCamoIndex(int PedIndex, int TypeOfWeapon)
{
	CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
	if(pPed)
	{
		if(scriptVerifyf(pPed->GetInventory(), "%s:GET_PED_WEAPON_CAMO_INDEX - ped requires an inventory", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			CWeaponItem* pWeaponItem = pPed->GetInventory()->GetWeapon(TypeOfWeapon);
			if(scriptVerifyf(pWeaponItem, "%s:GET_PED_WEAPON_CAMO_INDEX - ped does not have weapon [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), TypeOfWeapon))
			{
				if(pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetEquippedWeapon())
				{
					if(pPed->GetWeaponManager()->GetEquippedWeapon()->GetWeaponHash() == (u32)TypeOfWeapon)
					{
						return pPed->GetWeaponManager()->GetEquippedWeapon()->GetCamoDiffuseTexIdx();
					}
				}
			}
		}
	}

	return -1;
}

void CommandSetWeaponObjectTintIndex(int ObjectIndex, int TintIndex)
{
	CObject* pObject = CTheScripts::GetEntityToModifyFromGUID<CObject>(ObjectIndex);
	if(pObject)
	{
		if(scriptVerifyf(pObject->GetWeapon(), "SET_WEAPON_OBJECT_TINT_INDEX - %s Object [%s] has no weapon", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pObject->GetModelName()))
		{
			CWeapon* pWeapon = pObject->GetWeapon();
			pWeapon->UpdateShaderVariables((u8)TintIndex);
		}
	}
}

int CommandGetWeaponObjectTintIndex(int ObjectIndex)
{
	CObject* pObject = CTheScripts::GetEntityToModifyFromGUID<CObject>(ObjectIndex);
	if(pObject)
	{
		if(scriptVerifyf(pObject->GetWeapon(), "GET_WEAPON_OBJECT_TINT_INDEX - %s Object [%s] has no weapon", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pObject->GetModelName()))
		{
			CWeapon* pWeapon = pObject->GetWeapon();
			return pWeapon->GetTintIndex();
		}
	}
	return -1;
}

void CommandSetWeaponObjectComponentTintIndex(int ObjectIndex, int TypeOfComponent, int TintIndex)
{
	CObject* pObject = CTheScripts::GetEntityToModifyFromGUID<CObject>(ObjectIndex);
	if(pObject)
	{
		if(scriptVerifyf(pObject->GetWeapon(), "SET_WEAPON_OBJECT_COMPONENT_TINT_INDEX - %s Object [%s] has no weapon", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pObject->GetModelName()))
		{
			CWeapon* pWeapon = pObject->GetWeapon();
			if (pWeapon)
			{
				const CWeapon::Components& components = pWeapon->GetComponents();
				for(s32 i = 0; i < components.GetCount(); i++)
				{
					if(components[i]->GetInfo()->GetHash() == (u32)TypeOfComponent)
					{
						components[i]->UpdateShaderVariables((u8)TintIndex);
					}
				}
			}
		}
	}
}

int CommandGetWeaponObjectComponentTintIndex(int ObjectIndex, int TypeOfComponent)
{
	CObject* pObject = CTheScripts::GetEntityToModifyFromGUID<CObject>(ObjectIndex);
	if(pObject)
	{
		if(scriptVerifyf(pObject->GetWeapon(), "GET_WEAPON_OBJECT_COMPONENT_TINT_INDEX - %s Object [%s] has no weapon", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pObject->GetModelName()))
		{
			CWeapon* pWeapon = pObject->GetWeapon();
			if (pWeapon)
			{
				const CWeapon::Components& components = pWeapon->GetComponents();
				for(s32 i = 0; i < components.GetCount(); i++)
				{
					if(components[i]->GetInfo()->GetHash() == (u32)TypeOfComponent)
					{
						return components[i]->GetTintIndex();
					}
				}
			}
		}
	}
	return -1;
}

int CommandGetWeaponTintCount(int TypeOfWeapon)
{
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(TypeOfWeapon);
	if(scriptVerifyf(pWeaponInfo, "GET_WEAPON_TINT_COUNT - %s pWeaponInfo is NULL for weapon [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), TypeOfWeapon))
	{
		return pWeaponInfo->GetSpecValuesCount();
	}

	return 0;
}

void CommandSetWeaponObjectCamoIndex(int ObjectIndex, int CamoIdx)
{
	CObject* pObject = CTheScripts::GetEntityToModifyFromGUID<CObject>(ObjectIndex);
	if(pObject)
	{
		if(scriptVerifyf(pObject->GetWeapon(), "SET_WEAPON_OBJECT_CAMO_INDEX - %s Object [%s] has no weapon", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pObject->GetModelName()))
		{
			CWeapon* pWeapon = pObject->GetWeapon();
			pWeapon->SetCamoDiffuseTexIdx(CamoIdx);
		}
	}
}

int CommandGetWeaponObjectCamoIndex(int ObjectIndex)
{
	CObject* pObject = CTheScripts::GetEntityToModifyFromGUID<CObject>(ObjectIndex);
	if(pObject)
	{
		if(scriptVerifyf(pObject->GetWeapon(), "GET_WEAPON_OBJECT_CAMO_INDEX - %s Object [%s] has no weapon", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pObject->GetModelName()))
		{
			CWeapon* pWeapon = pObject->GetWeapon();
			return pWeapon->GetCamoDiffuseTexIdx();
		}
	}
	return -1;
}


bool CommandGetWeaponHudStats(int TypeOfWeapon, scrWeaponHudValues* out_Values)
{
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(TypeOfWeapon);
	if(scriptVerifyf(pWeaponInfo, "GET_WEAPON_HUD_STATS - %s pWeaponInfo is NULL for weapon [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), TypeOfWeapon))
	{
		out_Values->HudDamage.Int	= pWeaponInfo->GetHudDamage();
		out_Values->HudSpeed.Int	= pWeaponInfo->GetHudSpeed();
		out_Values->HudCapacity.Int = pWeaponInfo->GetHudCapacity();
		out_Values->HudAccuracy.Int = pWeaponInfo->GetHudAccuracy();
		out_Values->HudRange.Int	= pWeaponInfo->GetHudRange();

		return true;
	}

	return false;
}

bool CommandGetWeaponComponentHudStats(int TypeOfComponent, scrWeaponHudValues* out_Values)
{
	if(TypeOfComponent != 0)
	{
		const CWeaponComponentInfo* pComponentInfo = CWeaponComponentManager::GetInfo(TypeOfComponent);
		if(scriptVerifyf(pComponentInfo, "Weapon component hash [%d] does not exist in data", TypeOfComponent))
		{
			out_Values->HudDamage.Int	= pComponentInfo->GetHudDamage();
			out_Values->HudSpeed.Int	= pComponentInfo->GetHudSpeed();
			out_Values->HudCapacity.Int = pComponentInfo->GetHudCapacity();
			out_Values->HudAccuracy.Int = pComponentInfo->GetHudAccuracy();
			out_Values->HudRange.Int	= pComponentInfo->GetHudRange();

			return true;
		}
	}

	return false;
}

float CommandGetWeaponDamage(int TypeOfWeapon, int TypeOfComponent)
{
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(TypeOfWeapon);
	if(scriptVerifyf(pWeaponInfo, "GET_WEAPON_DAMAGE - %s pWeaponInfo is NULL for weapon [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), TypeOfWeapon))
	{
		float fDamage = pWeaponInfo->GetDamage();

		if(TypeOfComponent != 0)
		{
			const CWeaponComponentInfo* pComponentInfo = CWeaponComponentManager::GetInfo(TypeOfComponent);
			if(scriptVerifyf(pComponentInfo, "Weapon component hash [%d] does not exist in data", TypeOfComponent))
			{
				if(pComponentInfo->GetDamageModifier())
				{
					pComponentInfo->GetDamageModifier()->ModifyDamage(fDamage);
				}
			}
		}
		return fDamage;
	}
	return 0.0f;
}

int CommandGetWeaponClipSize(int TypeOfWeapon)
{
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(TypeOfWeapon);
	if(scriptVerifyf(pWeaponInfo, "GET_WEAPON_CLIP_SIZE - %s pWeaponInfo is NULL for weapon [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), TypeOfWeapon))
	{
		return pWeaponInfo->GetClipSize();
	}
	return 0;
}

float CommandGetWeaponTimeBetweenShots(int TypeOfWeapon)
{
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(TypeOfWeapon);
	if(scriptVerifyf(pWeaponInfo, "GET_WEAPON_TIME_BETWEEN_SHOTS - %s pWeaponInfo is NULL for weapon [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), TypeOfWeapon))
	{
		return pWeaponInfo->GetTimeBetweenShots();
	}
	return 0.0f;
}

float CommandGetWeaponAccuracy(int TypeOfWeapon, int TypeOfComponent)
{
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(TypeOfWeapon);
	if(scriptVerifyf(pWeaponInfo, "GET_WEAPON_ACCURACY - %s pWeaponInfo is NULL for weapon [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), TypeOfWeapon))
	{
		float fAccuracySpread = pWeaponInfo->GetAccuracySpread();
		if(TypeOfComponent != 0)
		{
			const CWeaponComponentInfo* pComponentInfo = CWeaponComponentManager::GetInfo(TypeOfComponent);
			if(scriptVerifyf(pComponentInfo, "Weapon component hash [%d] does not exist in data", TypeOfComponent))
			{
				if(pComponentInfo->GetAccuracyModifier())
				{
					sWeaponAccuracy weaponAccuracy;
					weaponAccuracy.fAccuracy = fAccuracySpread;
					pComponentInfo->GetAccuracyModifier()->ModifyAccuracy(weaponAccuracy);
					fAccuracySpread = weaponAccuracy.fAccuracy;
				}
			}
		}
		return fAccuracySpread;
	}
	return 0.0f;
}

void CommandSetPedChanceOfFiringBlanks(int PedIndex, float ChanceOfFiringBlanksMin, float ChanceOfFiringBlanksMax)
{
	CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
	if(pPed)
	{
		if(scriptVerifyf(pPed->GetWeaponManager(), "%s:SET_PED_CHANCE_OF_FIRING_BLANKS - ped requires a weapon manager", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			pPed->GetWeaponManager()->SetChanceToFireBlanks(ChanceOfFiringBlanksMin, ChanceOfFiringBlanksMax);
		}
	}
}

s32 CommandSetPedShootOrdnanceWeapon( int PedIndex, float fOverrideLifeTime )
{
	s32 ReturnObjectIndex = NULL_IN_SCRIPTING_LANGUAGE;

	CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
	if(pPed)
	{
		if( scriptVerifyf( pPed->GetWeaponManager(), "%s:SET_PED_SHOOT_ORDNANCE_WEAPON - ped requires a weapon manager", CTheScripts::GetCurrentScriptNameAndProgramCounter() ) )
		{
			CPedEquippedWeapon* pPedEquippedWeapon = pPed->GetWeaponManager()->GetPedEquippedWeapon();
			scriptAssertf( pPedEquippedWeapon, "%s:SET_PED_SHOOT_ORDNANCE_WEAPON - Invalid equipped weapon.", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			
			CProjectile* pProjectileObject = pPedEquippedWeapon->GetProjectileOrdnance();
			if( pProjectileObject )
			{
				// Cache off the ordnance
				ReturnObjectIndex = CTheScripts::GetGUIDFromEntity( *pProjectileObject );
		
				CWeapon* pEquippedWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
				CObject* pWeaponObject = pPed->GetWeaponManager()->GetEquippedWeaponObject();
				if( scriptVerifyf( pEquippedWeapon && pWeaponObject, "%s:SET_PED_SHOOT_ORDNANCE_WEAPON - ped requires an equipped weapon", CTheScripts::GetCurrentScriptNameAndProgramCounter() ) )
				{
					Matrix34 matWeapon;
					if( pEquippedWeapon->GetWeaponModelInfo()->GetBoneIndex( WEAPON_MUZZLE ) != -1 )
						pWeaponObject->GetGlobalMtx( pEquippedWeapon->GetWeaponModelInfo()->GetBoneIndex( WEAPON_MUZZLE ), matWeapon );
					else
						pWeaponObject->GetMatrixCopy( matWeapon );

					CWeapon::sFireParams params( pPed, matWeapon, NULL, NULL );
					params.bScriptControlled = true;
					params.fOverrideLifeTime = fOverrideLifeTime;
					pEquippedWeapon->Fire( params );
				}
			}
		}
	}
	
	return ReturnObjectIndex;
}

void CommandRequestWeaponHighDetailModel(int ObjectIndex)
{
	CObject* pObject = CTheScripts::GetEntityToModifyFromGUID<CObject>(ObjectIndex);
	if(pObject)
	{
		if(scriptVerifyf(pObject->GetWeapon(), "REQUEST_WEAPON_HIGH_DETAIL_MODEL- %s Object [%s] has no weapon", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pObject->GetModelName()))
		{
			CWeapon* pWeapon = pObject->GetWeapon();
			pWeapon->RequestHdAssets();
		}
	}
} 

void CommandSetWeaponDamageModifier(int TypeOfWeapon, float NewModifier)
{
	CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfoNonConst<CWeaponInfo>(TypeOfWeapon);
	if(scriptVerifyf(pWeaponInfo, "SET_WEAPON_DAMAGE_MODIFIER - %s pWeaponInfo is NULL for weapon [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), TypeOfWeapon))
	{
		pWeaponInfo->SetWeaponDamageModifier(NewModifier);
	}
}

float CommandGetWeaponDamageModifier(int TypeOfWeapon)
{
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(TypeOfWeapon);
	if(scriptVerifyf(pWeaponInfo, "GET_WEAPON_DAMAGE_MODIFIER - %s pWeaponInfo is NULL for weapon [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), TypeOfWeapon))
	{
		return pWeaponInfo->GetWeaponDamageModifier();
	}

	return 1.0f;
}

void CommandSetWeaponFallOffRangeModifier(int TypeOfWeapon, float NewModifier)
{
	CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfoNonConst<CWeaponInfo>(TypeOfWeapon);
	if(scriptVerifyf(pWeaponInfo, "SET_WEAPON_FALL_OFF_RANGE_MODIFIER - %s pWeaponInfo is NULL for weapon [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), TypeOfWeapon))
	{
		pWeaponInfo->SetDamageFallOffRangeModifier(NewModifier);
	}
}

float CommandGetWeaponFallOffRangeModifier(int TypeOfWeapon)
{
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(TypeOfWeapon);
	if(scriptVerifyf(pWeaponInfo, "GET_WEAPON_FALL_OFF_RANGE_MODIFIER - %s pWeaponInfo is NULL for weapon [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), TypeOfWeapon))
	{
		return pWeaponInfo->GetDamageFallOffRangeModifier();
	}

	return 1.0f;
}

void CommandSetWeaponFallOffDamageModifier(int TypeOfWeapon, float NewModifier)
{
	CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfoNonConst<CWeaponInfo>(TypeOfWeapon);
	if (scriptVerifyf(pWeaponInfo, "SET_WEAPON_FALL_OFF_DAMAGE_MODIFIER - %s pWeaponInfo is NULL for weapon [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), TypeOfWeapon))
	{
		pWeaponInfo->SetDamageFallOffTransientModifier(NewModifier);
	}
}

float CommandGetWeaponFallOffDamageModifier(int TypeOfWeapon)
{
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(TypeOfWeapon);
	if (scriptVerifyf(pWeaponInfo, "GET_WEAPON_FALL_OFF_DAMAGE_MODIFIER - %s pWeaponInfo is NULL for weapon [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), TypeOfWeapon))
	{
		return pWeaponInfo->GetDamageFallOffTransientModifier();
	}

	return 1.0f;
}

void CommandSetWeaponAoEModifier(int TypeOfWeapon, float NewModifier)
{
	CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfoNonConst<CWeaponInfo>(TypeOfWeapon);
	if (scriptVerifyf(pWeaponInfo, "SET_WEAPON_AOE_MODIFIER - %s pWeaponInfo is NULL for weapon [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), TypeOfWeapon))
	{
		pWeaponInfo->SetAoEModifier(NewModifier);
	}
}

float CommandGetWeaponAoEModifier(int TypeOfWeapon)
{
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(TypeOfWeapon);
	if (scriptVerifyf(pWeaponInfo, "GET_WEAPON_AOE_MODIFIER - %s pWeaponInfo is NULL for weapon [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), TypeOfWeapon))
	{
		return pWeaponInfo->GetAoEModifier();
	}

	return 1.0f;
}

void CommandSetWeaponEffectDurationModifier(int TypeOfWeapon, float NewModifier)
{
	CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfoNonConst<CWeaponInfo>(TypeOfWeapon);
	if (scriptVerifyf(pWeaponInfo, "SET_WEAPON_EFFECT_DURATION_MODIFIER - %s pWeaponInfo is NULL for weapon [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), TypeOfWeapon))
	{
		pWeaponInfo->SetEffectDurationModifier(NewModifier);
	}
}

float CommandGetWeaponEffectDurationModifier(int TypeOfWeapon)
{
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(TypeOfWeapon);
	if (scriptVerifyf(pWeaponInfo, "GET_WEAPON_EFFECT_DURATION_MODIFIER - %s pWeaponInfo is NULL for weapon [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), TypeOfWeapon))
	{
		return pWeaponInfo->GetEffectDurationModifier();
	}

	return 1.0f;
}

void CommandSetRecoilAccuracyToAllowHeadShotPlayerModifier(int TypeOfWeapon, float NewModifier)
{
	CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfoNonConst<CWeaponInfo>(TypeOfWeapon);
	if(scriptVerifyf(pWeaponInfo, "SET_RECOIL_ACCURACY_TO_ALLOW_HEADSHOT_PLAYER - %s pWeaponInfo is NULL for weapon [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), TypeOfWeapon))
	{
		pWeaponInfo->SetRecoilAccuracyToAllowHeadShotPlayerModifier(NewModifier);
	}
}

float CommandGetRecoilAccuracyToAllowHeadShotPlayerModifier(int TypeOfWeapon)
{
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(TypeOfWeapon);
	if(scriptVerifyf(pWeaponInfo, "GET_RECOIL_ACCURACY_TO_ALLOW_HEADSHOT_PLAYER - %s pWeaponInfo is NULL for weapon [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), TypeOfWeapon))
	{
		return pWeaponInfo->GetRecoilAccuracyToAllowHeadShotPlayerModifier();
	}

	return 1.0f;
}

void CommandSetMinHeadShotDistancePlayerModifier(int TypeOfWeapon, float NewModifier)
{
	CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfoNonConst<CWeaponInfo>(TypeOfWeapon);
	if(scriptVerifyf(pWeaponInfo, "SET_MIN_HEADSHOT_DISTANCE_PLAYER - %s pWeaponInfo is NULL for weapon [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), TypeOfWeapon))
	{
		pWeaponInfo->SetMinHeadShotDistancePlayerModifier(NewModifier);
	}
}

float CommandGetMinHeadShotDistancePlayerModifier(int TypeOfWeapon)
{
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(TypeOfWeapon);
	if(scriptVerifyf(pWeaponInfo, "GET_MIN_HEADSHOT_DISTANCE_PLAYER - %s pWeaponInfo is NULL for weapon [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), TypeOfWeapon))
	{
		return pWeaponInfo->GetMinHeadShotDistancePlayerModifier();
	}

	return 1.0f;
}

void CommandSetMaxHeadShotDistancePlayerModifier(int TypeOfWeapon, float NewModifier)
{
	CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfoNonConst<CWeaponInfo>(TypeOfWeapon);
	if(scriptVerifyf(pWeaponInfo, "SET_MAX_HEADSHOT_DISTANCE_PLAYER - %s pWeaponInfo is NULL for weapon [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), TypeOfWeapon))
	{
		pWeaponInfo->SetMaxHeadShotDistancePlayerModifier(NewModifier);
	}
}

float CommandGetMaxHeadShotDistancePlayerModifier(int TypeOfWeapon)
{
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(TypeOfWeapon);
	if(scriptVerifyf(pWeaponInfo, "GET_MAX_HEADSHOT_DISTANCE_PLAYER - %s pWeaponInfo is NULL for weapon [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), TypeOfWeapon))
	{
		return pWeaponInfo->GetMaxHeadShotDistancePlayer();
	}

	return 1.0f;
}

void CommandSetHeadShotDamageModifierPlayerModifier(int TypeOfWeapon, float NewModifier)
{
	CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfoNonConst<CWeaponInfo>(TypeOfWeapon);
	if(scriptVerifyf(pWeaponInfo, "SET_HEADSHOT_DAMAGE_MODIFIER_PLAYER - %s pWeaponInfo is NULL for weapon [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), TypeOfWeapon))
	{
		pWeaponInfo->SetHeadShotDamageModifierPlayerModifier(NewModifier);
	}
}

float CommandGetHeadShotDamageModifierPlayerModifier(int TypeOfWeapon)
{
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(TypeOfWeapon);
	if(scriptVerifyf(pWeaponInfo, "GET_HEADSHOT_DAMAGE_MODIFIER_PLAYER - %s pWeaponInfo is NULL for weapon [%d]", CTheScripts::GetCurrentScriptNameAndProgramCounter(), TypeOfWeapon))
	{
		return pWeaponInfo->GetHeadShotDamageModifierPlayer();
	}

	return 1.0f;
}

bool CommandIsPedCurrentWeaponSilenced(int PedIndex)
{
	const CPed* pPed = CTheScripts::GetEntityToQueryFromGUID<CPed>(PedIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES_NO_DEAD_CHECK);
	if(pPed)
	{
		if( scriptVerifyf( pPed->GetWeaponManager(), "%s:IS_PED_CURRENT_WEAPON_SILENCED - ped requires a weapon manager", CTheScripts::GetCurrentScriptNameAndProgramCounter() ) )
		{
			const CWeapon* pEquippedWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
			if(pEquippedWeapon)
			{
				return pEquippedWeapon->GetIsSilenced();
			}
		}
	}

	return false;
}

bool CommandIsFlashLightOn(int PedIndex)
{
	CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
	if(pPed)
	{
		CPedWeaponManager* pWeaponMgr = pPed->GetWeaponManager();
		if(scriptVerifyf(pWeaponMgr, "%s: IS_FLASH_LIGHT_ON - ped requires a weapon manager", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			CWeapon* pWeapon = pWeaponMgr->GetEquippedWeapon();
			if(pWeapon && pWeapon->GetFlashLightComponent())
			{
				return pWeapon->GetFlashLightComponent()->GetActive();
			}
		}
	}

	return false;
}

void CommandSetFlashlightFadeDistance(float distance) 
{
	CWeaponComponentFlashLight::SetFlashlightFadeDistance(distance);
}

void CommandSetFlashlightActiveHistory(int PedIndex, bool bActive)
{
	CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
	if(pPed)
	{
		CPedWeaponManager* pWeaponMgr = pPed->GetWeaponManager();
		if(scriptVerifyf(pWeaponMgr, "%s: SET_FLASH_LIGHT_ACTIVE_HISTORY - ped requires a weapon manager", CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			CWeapon* pWeapon = pWeaponMgr->GetEquippedWeapon();
			if(pWeapon && pWeapon->GetFlashLightComponent())
			{
				pWeapon->GetFlashLightComponent()->SetActiveHistory(bActive);
			}
		}
	}
}

void CommandWeaponAnimationOverride(int PedIndex, int WeaponAnimationOverride)
{
	CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
	if(pPed)
	{
		if(scriptVerifyf( WeaponAnimationOverride == 0 || CWeaponAnimationsManager::GetInstance().DoesWeaponAnimationSetForHashExist(WeaponAnimationOverride), "WeaponAnimationOverride doesn't exist"))
		{
			pPed->SetOverrideWeaponAnimations(WeaponAnimationOverride);
		}
	}
}

int CommandGetWeaponDamageType(int TypeOfWeapon)
{
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(TypeOfWeapon);
	if (pWeaponInfo)
	{
		return pWeaponInfo->GetDamageType();
	}

	return 0;
}

void CommandSetEqippedWeaponStartSpinningAtFullSpeed(int PedIndex)
{
	CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);
	if(pPed)
	{
		CPedWeaponManager* pWeaponManager = pPed->GetWeaponManager();
		if(pWeaponManager)
		{
			CWeapon* pEquippedWeapon = pWeaponManager->GetEquippedWeapon();
			if(pEquippedWeapon)
			{
				pEquippedWeapon->SetNoSpinUp(true);
			}
		}
	}
}

bool CommandCanUseWeaponOnParachute(int weaponHash)
{
	if(SCRIPT_VERIFY( weaponHash != 0, "GET_WEAPONTYPE_MODEL - Weapon hash is invalid"))
	{
		const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(weaponHash);
		if(scriptVerifyf(pWeaponInfo, "Weapon hash [%d] does not exist in data", weaponHash))
		{
			static atHashWithStringNotFinal s_ParaDriveByName("DRIVEBY_PARACHUTE", 0x9e643cb0);
			const CVehicleDriveByInfo* pDrivebyInfo = CVehicleMetadataMgr::GetVehicleDriveByInfo(s_ParaDriveByName.GetHash());
			if(pDrivebyInfo && !pWeaponInfo->GetNotAllowedForDriveby())
			{
				return pDrivebyInfo->CanUseWeaponGroup(pWeaponInfo->GetGroup()) || pDrivebyInfo->CanUseWeaponType(weaponHash);
			}
		}
	}
	return false;
}

int CommandCreateAirDefenceSphere(const scrVector &scrPosition, const float fRadius, const scrVector &scrWeaponPosition, int WeaponHash)
{
	Vec3V vPosition = Vec3V(scrPosition);
	Vec3V vWeaponPosition = Vec3V(scrWeaponPosition);
	int uZoneIdx = CAirDefenceManager::CreateAirDefenceSphere(vPosition, fRadius, vWeaponPosition, WeaponHash);
	return uZoneIdx;
}

int CommandCreateAirDefenceAngledArea(const scrVector &scrCoordA, const scrVector &scrCoordB, const float fWidth, const scrVector &scrWeaponPosition, int WeaponHash)
{
	// Copy vectors
	Vec3V vA = Vec3V(scrCoordA);
	Vec3V vB = Vec3V(scrCoordB);

	// Construct transform matrix
	Vec3V vAToB = vB - vA;
	Vec3V vUp(0.0f, 0.0f, 1.0f);
	Vec3V vRight = Cross(vAToB, vUp);
	vRight = Normalize(vRight);
	Vec3V vForward = Cross(vUp, vRight);
	vForward = Normalize(vForward);
	Vec3V vPosition = vA + vAToB * ScalarV(0.5f);
	Mat34V mtx(vRight, vForward, vUp, vPosition);

	// Construct min / max points
	float fHalfWidth = fWidth * 0.5f;
	Vec3V vMax = vB + vRight * ScalarV(fHalfWidth);
	vMax = UnTransformOrtho(mtx, vMax);
	Vec3V vMin = -vMax;

	Vec3V vWeaponPosition = Vec3V(scrWeaponPosition);
	int uZoneIdx = CAirDefenceManager::CreateAirDefenceArea(vMin, vMax, mtx, vWeaponPosition, WeaponHash);
	return uZoneIdx;
}


bool CommandRemoveAirDefenceSphere(int iZoneIndex)
{
	if (CAirDefenceManager::DeleteAirDefenceZone((u8)iZoneIndex))
	{
		return true;
	}

	return false;
}

bool CommandRemoveAirDefenceSpheresInteresectingPosition(const scrVector &scrPosition)
{
	Vec3V vPosition = Vec3V(scrPosition);
	if (CAirDefenceManager::DeleteAirDefenceZonesInteresectingPosition(vPosition))
	{
		return true;
	}

	return false;
}

void CommandRemoveAllAirDefenceSpheres()
{
	CAirDefenceManager::ResetAllAirDefenceZones();
}


void CommandSetPlayerTargettableForAllAirDefenceSpheres(int PlayerIndex, bool bTargettable)
{
	if (PlayerIndex != -1)
	{
		CAirDefenceManager::SetPlayerTargetableForAllZones(PlayerIndex, bTargettable);
	}
}

void CommandSetPlayerTargettableForAirDefenceSphere(int PlayerIndex, int ZoneIndex, bool bTargettable)
{
	if (PlayerIndex != -1)
	{
		CAirDefenceZone *pDefZone = CAirDefenceManager::GetAirDefenceZone((u8)ZoneIndex);
		if (scriptVerifyf(pDefZone, "SET_PLAYER_TARGETTABLE_FOR_AIR_DEFENCE_SPHERE: Invalid sphere - SphereIndex: %d", ZoneIndex))
		{
			pDefZone->SetPlayerIsTargettable(PlayerIndex, bTargettable);
		}
	}
}

void CommandResetPlayerTargettableFlagsForAllAirDefenceSpheres()
{
	CAirDefenceManager::ResetPlayerTargetableFlagsForAllZones();
}

void CommandResetPlayetTargettableFlagsForAirDefenceSphere(int ZoneIndex)
{
	CAirDefenceManager::ResetPlayetTargetableFlagsForZone((u8)ZoneIndex);
}

bool CommandIsAirDefenceSphereInArea(const scrVector &scrPosition, const float fRadius, int &ZoneIndex)
{
	ZoneIndex = 0;
	Vec3V vPosition = Vec3V(scrPosition);
	u8 uUpdatedZoneIndex = 0;
	if (CAirDefenceManager::IsDefenceZoneInArea(vPosition, fRadius, uUpdatedZoneIndex))
	{
		ZoneIndex = (int)uUpdatedZoneIndex;
		return true;
	}

	return false;
}

void CommandFireAirDefenceSphereWeaponAtPosition(int ZoneIndex, const scrVector &scrPosition)
{
	Vec3V vTargetPosition = Vec3V(scrPosition);
	CAirDefenceZone *pDefZone = CAirDefenceManager::GetAirDefenceZone((u8)ZoneIndex);
	if (scriptVerifyf(pDefZone, "FIRE_AIR_DEFENCE_SPHERE_WEAPON_AT_POSITION: Invalid sphere - SphereIndex: %d", ZoneIndex))
	{
		if(scriptVerifyf(pDefZone->GetWeaponHash() != 0, "FIRE_AIR_DEFENCE_SPHERE_WEAPON_AT_POSITION: Sphere was not set up with a weapon hash - SphereIndex: %d", ZoneIndex))
		{
			pDefZone->FireWeaponAtPosition(vTargetPosition, true);
		}
	}
}

bool CommandDoesAirDefenceSphereExist(int ZoneIndex)
{
	CAirDefenceZone *pDefZone = CAirDefenceManager::GetAirDefenceZone((u8)ZoneIndex);
	if (pDefZone)
	{
		return true;
	}
	return false;
}

void CommandSetCanPedSelectIventoryWeapon(int PedIndex, int WeaponHash, bool bCanSelect)
{
	CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

	if(pPed)
	{
		CPedInventory* pPedInventory = pPed->GetInventory();
		if(scriptVerifyf(pPedInventory, "%s:SET_CAN_PED_SELECT_INVENTORY_WEAPON - %s[%p] requires an inventory", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pPed->GetModelName(), pPed))
		{
			CWeaponItem* pWeaponItem = pPedInventory->GetWeapon(WeaponHash);

			if (scriptVerifyf(pWeaponItem, "%s:SET_CAN_PED_SELECT_INVENTORY_WEAPON - Could not find weapon (hash - %i) in %s[%p] inventory", CTheScripts::GetCurrentScriptNameAndProgramCounter(), WeaponHash, pPed->GetModelName(), pPed))
			{
				pWeaponItem->SetCanSelect(bCanSelect);
				weaponDebugf3("%s:SET_CAN_PED_SELECT_INVENTORY_WEAPON: ped - %s[%p], weaponHash - %i, canSelect - %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pPed->GetModelName(), pPed, WeaponHash, bCanSelect ? "true" : "false");
			}
		}
	}
}

void CommandSetCanPedSelectAllWeapons(int PedIndex, bool bCanSelect)
{
	CPed *pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(PedIndex);

	if(pPed)
	{
		CPedInventory* pPedInventory = pPed->GetInventory();
		if(scriptVerifyf(pPedInventory, "%s:SET_CAN_PED_SELECT_ALL_WEAPONS - %s[%p] requires an inventory", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pPed->GetModelName(), pPed))
		{
			CWeaponItemRepository& rWeaponRepository = pPedInventory->GetWeaponRepository();
			for(int i=0; i < rWeaponRepository.GetItemCount(); i++)
			{
				CWeaponItem* pWeaponItem = rWeaponRepository.GetItemByIndex(i);
				if (pWeaponItem)
				{
					pWeaponItem->SetCanSelect(bCanSelect);
					weaponDebugf3("%s:SET_CAN_PED_SELECT_ALL_WEAPONS: ped - %s[%p], weapon - %s, canSelect - %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pPed->GetModelName(), pPed, pWeaponItem->GetInfo() ? pWeaponItem->GetInfo()->GetName() : "", bCanSelect ? "true" : "false");
				}
			}
		}
	}
}

bool CommandGetWeaponHasNonLethalTag(int weaponHash)
{
	if (SCRIPT_VERIFY(weaponHash != 0, "GET_WEAPON_HAS_NON_LETHAL_TAG - Weapon hash is invalid"))
	{
		const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(weaponHash);

		if (scriptVerifyf(pWeaponInfo, "GET_WEAPON_HAS_NON_LETHAL_TAG - Weapon hash [%d] does not exist in data", weaponHash))
		{
			return pWeaponInfo->GetIsNonLethal();
		}
	}
	return false;
}

//
// name:		SetupScriptCommands
// description:	Setup weapons script commands
void SetupScriptCommands()
{
	SCR_REGISTER_SECURE(ENABLE_LASER_SIGHT_RENDERING,0xe05dc71741b614fb, CommandEnableLaserSightRendering);

	SCR_REGISTER_SECURE(GET_WEAPON_COMPONENT_TYPE_MODEL,0x2597f123290b0413, CommandGetWeaponComponentTypeModel);
	SCR_REGISTER_SECURE(GET_WEAPONTYPE_MODEL,0xb8dee91181c30e65, CommandGetWeaponTypeModel);
	SCR_REGISTER_SECURE(GET_WEAPONTYPE_SLOT,0xc5ede22c3d2885b3, CommandGetWeaponTypeSlot);
	SCR_REGISTER_SECURE(GET_WEAPONTYPE_GROUP,0x7f4cb90bca531a70, CommandGetWeaponTypeGroup);
	SCR_REGISTER_SECURE(GET_WEAPON_COMPONENT_VARIANT_EXTRA_COUNT,0xb02e6513201cffba, CommandGetWeaponComponentVariantExtraCount);
	SCR_REGISTER_SECURE(GET_WEAPON_COMPONENT_VARIANT_EXTRA_MODEL,0xdcd1cf170306e130, CommandGetWeaponComponentVariantExtraModel);

	SCR_REGISTER_SECURE(SET_CURRENT_PED_WEAPON,0x202b28feabec4034, CommandSetCurrentPedWeapon);
	SCR_REGISTER_SECURE(GET_CURRENT_PED_WEAPON,0xa7e29842fa438ed6, CommandGetCurrentPedWeapon);
	SCR_REGISTER_SECURE(GET_CURRENT_PED_WEAPON_ENTITY_INDEX,0x4d03373543a78098, CommandGetCurrentPedWeaponEntityIndex);
	SCR_REGISTER_SECURE(GET_BEST_PED_WEAPON,0x4eb626d21abb5bae, CommandGetBestPedWeapon);
	SCR_REGISTER_SECURE(SET_CURRENT_PED_VEHICLE_WEAPON,0x922ac1d1e4ad7864, CommandSetCurrentPedVehicleWeapon);
	SCR_REGISTER_SECURE(GET_CURRENT_PED_VEHICLE_WEAPON,0x0cc0416f24be03ff, CommandGetCurrentPedVehicleWeapon);
	SCR_REGISTER_SECURE(SET_PED_CYCLE_VEHICLE_WEAPONS_ONLY,0x983daefb042809e8, CommandSetCycleVehicleWeaponsOnly);
	SCR_REGISTER_SECURE(IS_PED_ARMED,0x5007a91d57c39ffc, CommandIsPedArmed);

	SCR_REGISTER_SECURE(IS_WEAPON_VALID,0x072699bd07af0086, CommandIsWeaponValid);
	SCR_REGISTER_SECURE(HAS_PED_GOT_WEAPON,0x8fea2e94638f9767, CommandHasPedGotWeapon);
	SCR_REGISTER_SECURE(IS_PED_WEAPON_READY_TO_SHOOT,0xc9ddce46bcbb3f06, CommandIsPedWeaponReadyToShoot);
	SCR_REGISTER_SECURE(GET_PED_WEAPONTYPE_IN_SLOT,0xcb497f652acb2dd3, CommandGetPedWeaponTypeInSlot);
	SCR_REGISTER_SECURE(GET_AMMO_IN_PED_WEAPON,0x1f741abe25b3cdd3, CommandGetAmmoInPedWeapon);
	SCR_REGISTER_SECURE(ADD_AMMO_TO_PED,0x4f692b4cc67a70bc, CommandAddAmmoToPed);
	SCR_REGISTER_SECURE(SET_PED_AMMO,0xca8f055643a07c74, CommandSetPedAmmo);
	SCR_REGISTER_SECURE(SET_PED_INFINITE_AMMO,0x73ac05050f54f1e0, CommandSetPedInfiniteAmmo);
	SCR_REGISTER_SECURE(SET_PED_INFINITE_AMMO_CLIP,0xb5ad1b3d21ec1aa1, CommandSetPedInfiniteAmmoClip);
	SCR_REGISTER_SECURE(SET_PED_STUN_GUN_FINITE_AMMO,0x5af8598f3b8dd63d, CommandSetPedStunGunFiniteAmmo);
	SCR_REGISTER_SECURE(GIVE_WEAPON_TO_PED,0x9521fb98db6ddf50, CommandGiveWeaponToPed);
	SCR_REGISTER_SECURE(GIVE_DELAYED_WEAPON_TO_PED,0x5399a284d9fad3bd, CommandGiveDelayedWeaponToPed);
	SCR_REGISTER_SECURE(REMOVE_ALL_PED_WEAPONS,0x70d9ec5af67d79c4, CommandRemoveAllPedWeapons);
	SCR_REGISTER_SECURE(REMOVE_WEAPON_FROM_PED,0xe002dfd518bf86a7, CommandRemoveWeaponFromPed);
	SCR_REGISTER_SECURE(HIDE_PED_WEAPON_FOR_SCRIPTED_CUTSCENE,0x4f1e6a84bc157fa0, CommandHidePedWeaponForScriptedCutscene);
    SCR_REGISTER_SECURE(SET_PED_CURRENT_WEAPON_VISIBLE,0x882a3682867542aa, CommandSetPedCurrentWeaponVisible);
	SCR_REGISTER_SECURE(SET_PED_DROPS_WEAPONS_WHEN_DEAD,0x671d9a1f3d47d0de, CommandSetPedDropsWeaponsWhenDead);
	SCR_REGISTER_SECURE(HAS_PED_BEEN_DAMAGED_BY_WEAPON,0x6799f7dd0261990e, CommandHasPedBeenDamagedByWeapon);
	SCR_REGISTER_SECURE(CLEAR_PED_LAST_WEAPON_DAMAGE,0xe095496a833d555c, CommandClearPedLastWeaponDamage);

	SCR_REGISTER_SECURE(HAS_ENTITY_BEEN_DAMAGED_BY_WEAPON,0xeef1a3c0e56fc8ff, CommandHasEntityBeenDamagedByWeapon);
	SCR_REGISTER_SECURE(CLEAR_ENTITY_LAST_WEAPON_DAMAGE,0x2ad4bd9956c8aad7, CommandClearEntityLastWeaponDamage);
	SCR_REGISTER_SECURE(SET_PED_DROPS_WEAPON,0xa6d4fbb3b47cf11e, CommandPedDropWeapon);
	SCR_REGISTER_SECURE(SET_PED_DROPS_INVENTORY_WEAPON,0x35e8b97ba540a71f, CommandPedDropsInventoryWeapon);
	SCR_REGISTER_SECURE(GET_MAX_AMMO_IN_CLIP,0x3fc10ba7b76fe8ba, CommandGetMaxAmmoInClip);
	SCR_REGISTER_SECURE(GET_AMMO_IN_CLIP,0x49d5a28b919b07c0, CommandGetAmmoInClip);
	SCR_REGISTER_SECURE(SET_AMMO_IN_CLIP,0x1e0ed6750db74e70, CommandSetAmmoInClip);
	SCR_REGISTER_SECURE(GET_MAX_AMMO,0x6019be7548b68c4b, CommandGetMaxAmmo);
	SCR_REGISTER_SECURE(GET_MAX_AMMO_BY_TYPE,0x5b75073e91a0aef6, CommandGetMaxAmmoByType);

	SCR_REGISTER_SECURE(ADD_PED_AMMO_BY_TYPE,0xfaf8b712245b6bf6, CommandAddPedAmmoByType);
	SCR_REGISTER_SECURE(SET_PED_AMMO_BY_TYPE,0xb003193c68661c8d, CommandSetPedAmmoByType);
	SCR_REGISTER_SECURE(GET_PED_AMMO_BY_TYPE,0x4a88c4acac824897, CommandGetPedAmmoByType);

	SCR_REGISTER_SECURE(SET_PED_AMMO_TO_DROP,0xbeb9849370cc0abe, CommandSetPedAmmoToDrop);
	SCR_REGISTER_SECURE(SET_PICKUP_AMMO_AMOUNT_SCALER,0xed8e9ca97c15c21d, CommandSetPickupAmmoAmountScaler);

	SCR_REGISTER_SECURE(GET_PED_AMMO_TYPE_FROM_WEAPON,0x5d3596c624f0c4ac, CommandGetPedAmmoTypeFromWeapon);
	SCR_REGISTER_SECURE(GET_PED_ORIGINAL_AMMO_TYPE_FROM_WEAPON,0x589b5b25edf4e89c, CommandGetPedOriginalAmmoTypeFromWeapon);

	SCR_REGISTER_SECURE(GET_PED_LAST_WEAPON_IMPACT_COORD,0x2ef467a4dca81d5a, CommandGetPedLastWeaponImpactCoord);

	SCR_REGISTER_SECURE(SET_PED_GADGET,0x777180e634735829, CommandSetPedGadget);
	SCR_REGISTER_SECURE(GET_IS_PED_GADGET_EQUIPPED,0x8de48073046d4cf0, CommandGetIsPedGadgetEquipped);
	SCR_REGISTER_SECURE(GET_SELECTED_PED_WEAPON,0x65141ccb0a6f7ea4, CommandGetSelectedPedWeapon);

	SCR_REGISTER_SECURE(EXPLODE_PROJECTILES,0x8c7d915965382c47, CommandExplodeProjectiles);
	SCR_REGISTER_SECURE(REMOVE_ALL_PROJECTILES_OF_TYPE,0xa968e928ca6f0a96, CommandRemoveAllProjectilesOfType);
	SCR_REGISTER_UNUSED(ACTIVATE_DETONATOR,0x42c916a10522b15f, CommandActivateDetonator);
	SCR_REGISTER_UNUSED(GET_IS_DETONATOR_FIRED,0xb4cdf0805f86df21, CommandGetIsDetonatorFired);
	SCR_REGISTER_UNUSED(GET_IS_STICKY_CAM_VIEWER_FIRED,0x68cf1e9fee40721e, CommandGetIsStickyCamViewerFired);
	SCR_REGISTER_SECURE(GET_LOCKON_DISTANCE_OF_CURRENT_PED_WEAPON,0xaec78423999efe49, CommandGetLockOnDistanceOfCurrentPedWeapon);
	SCR_REGISTER_SECURE(GET_MAX_RANGE_OF_CURRENT_PED_WEAPON,0x6658b2555bca5bdf, CommandGetMaxRangeOfCurrentPedWeapon);
	SCR_REGISTER_SECURE(HAS_VEHICLE_GOT_PROJECTILE_ATTACHED,0xbc0cfdf47c99d84b, CommandHasVehicleGotProjectileAttached);

	SCR_REGISTER_SECURE(GIVE_WEAPON_COMPONENT_TO_PED,0x130c7a0e5945e494, CommandGiveWeaponComponentToPed);
	SCR_REGISTER_SECURE(REMOVE_WEAPON_COMPONENT_FROM_PED,0xdead024b4d0717f8, CommandRemoveWeaponComponentFromPed);
	SCR_REGISTER_SECURE(HAS_PED_GOT_WEAPON_COMPONENT,0x54b42dec4cab3d41, CommandHasPedGotWeaponComponent);
	SCR_REGISTER_SECURE(IS_PED_WEAPON_COMPONENT_ACTIVE,0x5c89acc6e6cf4148, CommandIsPedWeaponComponentActive);

	SCR_REGISTER_UNUSED(GET_PED_CURRENT_WEAPON_TIME_BEFORE_NEXT_SHOT,0x75a8182479296220, CommandGetPedCurrentWeaponTimeBeforeNextShot);

	SCR_REGISTER_SECURE(REFILL_AMMO_INSTANTLY,0x8cd8e9cb739389ac, CommandRefillAmmoInstant);
	SCR_REGISTER_SECURE(MAKE_PED_RELOAD,0xf4d8955f68fa1058, CommandMakePedReload);

	SCR_REGISTER_SECURE(REQUEST_WEAPON_ASSET,0x334e9a09859a3c8d, RequestWeaponAsset);
	SCR_REGISTER_SECURE(HAS_WEAPON_ASSET_LOADED,0xc870cd3d6d40cb09, HasWeaponAssetLoaded);
	SCR_REGISTER_SECURE(REMOVE_WEAPON_ASSET,0x315d54141149c1b6, RemoveWeaponAsset);

	SCR_REGISTER_UNUSED(HAS_PED_GOT_WEAPON_MANAGER,0x67a9213e9840bae0, CommandHasPedGotWeaponManager);

	SCR_REGISTER_SECURE(CREATE_WEAPON_OBJECT,0xbf59603e8a18feb0, CommandCreateWeaponObject);
	SCR_REGISTER_SECURE(GIVE_WEAPON_COMPONENT_TO_WEAPON_OBJECT,0x0b46dbce80ad9de5, CommandGiveWeaponComponentToWeaponObject);
	SCR_REGISTER_SECURE(REMOVE_WEAPON_COMPONENT_FROM_WEAPON_OBJECT,0x8d0169a2fdd87126, CommandRemoveWeaponComponentFromWeaponObject);
	SCR_REGISTER_SECURE(HAS_WEAPON_GOT_WEAPON_COMPONENT,0xddc523d7b3e70e65, CommandHasWeaponGotWeaponComponent);
	SCR_REGISTER_SECURE(GIVE_WEAPON_OBJECT_TO_PED,0x3aea6b9a9fe5bd26, CommandGiveWeaponObjectToPed);
	SCR_REGISTER_SECURE(DOES_WEAPON_TAKE_WEAPON_COMPONENT,0xf387a3736de5f1d5, CommandDoesWeaponTakeWeaponComponent);
	SCR_REGISTER_SECURE(GET_WEAPON_OBJECT_FROM_PED,0x07093f8f06dd02ff, CommandGetWeaponObjectFromPed);
	SCR_REGISTER_UNUSED(DROP_WEAPON_OBJECT_OF_PED,0xcb160c7b9368da2e, CommandDropWeaponObjectOfPed);

	SCR_REGISTER_UNUSED(GET_WEAPON_COMPONENT_NAME, 0x5972657f,			CommandGetWeaponComponentName);
	SCR_REGISTER_UNUSED(GET_WEAPON_COMPONENT_DESCRIPTION, 0x7a969327,	CommandGetWeaponComponentDescription);

	SCR_REGISTER_SECURE(GIVE_LOADOUT_TO_PED,0x588d77b641cbeddf, CommandGiveLoadOutToPed);
	SCR_REGISTER_UNUSED(SET_LOADOUT_ALIAS,0xa1698d5b905894c0, CommandSetLoadOutAlias);
	SCR_REGISTER_UNUSED(CLEAR_LOADOUT_ALIAS,0x27065729161c04e3, CommandClearLoadOutAlias);

	SCR_REGISTER_SECURE(SET_PED_WEAPON_TINT_INDEX,0x7fec7f18430588cc, CommandSetPedWeaponTintIndex);
	SCR_REGISTER_SECURE(GET_PED_WEAPON_TINT_INDEX,0xc158eb99f56cb1fb, CommandGetPedWeaponTintIndex);
	SCR_REGISTER_SECURE(SET_WEAPON_OBJECT_TINT_INDEX,0x3e27bf30c58d41ec, CommandSetWeaponObjectTintIndex);
	SCR_REGISTER_SECURE(GET_WEAPON_OBJECT_TINT_INDEX,0xf22ab94467b80164, CommandGetWeaponObjectTintIndex);
	SCR_REGISTER_SECURE(GET_WEAPON_TINT_COUNT,0xdf7052e5c1ffbd7b, CommandGetWeaponTintCount);

	SCR_REGISTER_SECURE(SET_PED_WEAPON_COMPONENT_TINT_INDEX,0xb7987e326f1fc793, CommandSetPedWeaponComponentTintIndex);
	SCR_REGISTER_SECURE(GET_PED_WEAPON_COMPONENT_TINT_INDEX,0x6f970742b880af88, CommandGetPedWeaponComponentTintIndex);
	SCR_REGISTER_SECURE(SET_WEAPON_OBJECT_COMPONENT_TINT_INDEX,0x9a614f230d802f82, CommandSetWeaponObjectComponentTintIndex);
	SCR_REGISTER_SECURE(GET_WEAPON_OBJECT_COMPONENT_TINT_INDEX,0xe4f0e3a023f6b91a, CommandGetWeaponObjectComponentTintIndex);

	SCR_REGISTER_UNUSED(SET_PED_WEAPON_CAMO_INDEX,0x4c9ff1928a739330, CommandSetPedWeaponCamoIndex);
	SCR_REGISTER_SECURE(GET_PED_WEAPON_CAMO_INDEX,0xef3626a1be542f5e, CommandGetPedWeaponCamoIndex);
	SCR_REGISTER_SECURE(SET_WEAPON_OBJECT_CAMO_INDEX,0x7ccc68177a94f9be, CommandSetWeaponObjectCamoIndex);
	SCR_REGISTER_UNUSED(GET_WEAPON_OBJECT_CAMO_INDEX,0x8a3d69849bd451fe, CommandGetWeaponObjectCamoIndex);

	SCR_REGISTER_SECURE(GET_WEAPON_HUD_STATS,0x0eac086d816d5cb1, CommandGetWeaponHudStats);
	SCR_REGISTER_SECURE(GET_WEAPON_COMPONENT_HUD_STATS,0x8dc1b37fab5462c2, CommandGetWeaponComponentHudStats);
	SCR_REGISTER_SECURE(GET_WEAPON_DAMAGE,0x1c07375215a5ad6b, CommandGetWeaponDamage);
	SCR_REGISTER_SECURE(GET_WEAPON_CLIP_SIZE,0x982237eb68628e6a, CommandGetWeaponClipSize);
	SCR_REGISTER_SECURE(GET_WEAPON_TIME_BETWEEN_SHOTS,0xfb17bb33ae9dea97, CommandGetWeaponTimeBetweenShots);
	SCR_REGISTER_UNUSED(GET_WEAPON_ACCURACY,0xf104b3c55715593b, CommandGetWeaponAccuracy);

	SCR_REGISTER_SECURE(SET_PED_CHANCE_OF_FIRING_BLANKS,0x1eabb8571d8c86d4, CommandSetPedChanceOfFiringBlanks);

	SCR_REGISTER_SECURE(SET_PED_SHOOT_ORDNANCE_WEAPON,0x4afc09d27972d668, CommandSetPedShootOrdnanceWeapon);

	SCR_REGISTER_SECURE(REQUEST_WEAPON_HIGH_DETAIL_MODEL,0x5f59388c2ba4ced1, CommandRequestWeaponHighDetailModel);

	SCR_REGISTER_SECURE(SET_WEAPON_DAMAGE_MODIFIER,0x4e2c3699b7a57d47, CommandSetWeaponDamageModifier);
	SCR_REGISTER_UNUSED(GET_WEAPON_DAMAGE_MODIFIER,0x6beddbd61f42d457, CommandGetWeaponDamageModifier);
	SCR_REGISTER_UNUSED(SET_WEAPON_FALL_OFF_RANGE_MODIFIER,0xd2840ccee4fe0b3c, CommandSetWeaponFallOffRangeModifier);
	SCR_REGISTER_UNUSED(GET_WEAPON_FALL_OFF_RANGE_MODIFIER,0xc7c9c0acd96afe6e, CommandGetWeaponFallOffRangeModifier);
	SCR_REGISTER_UNUSED(SET_WEAPON_FALL_OFF_DAMAGE_MODIFIER,0xd8bdb556f05116ae, CommandSetWeaponFallOffDamageModifier);
	SCR_REGISTER_UNUSED(GET_WEAPON_FALL_OFF_DAMAGE_MODIFIER, 0x2e3b8dacde116612, CommandGetWeaponFallOffDamageModifier);
	SCR_REGISTER_SECURE(SET_WEAPON_AOE_MODIFIER,0x7a3cfc319adc00b2, CommandSetWeaponAoEModifier);
	SCR_REGISTER_UNUSED(GET_WEAPON_AOE_MODIFIER,0x11179f4dbc8d7d0f, CommandGetWeaponAoEModifier);
	SCR_REGISTER_SECURE(SET_WEAPON_EFFECT_DURATION_MODIFIER,0xca92f7e472406ec5, CommandSetWeaponEffectDurationModifier);
	SCR_REGISTER_UNUSED(GET_WEAPON_EFFECT_DURATION_MODIFIER,0x6b13c3b8b1a6d4b3, CommandGetWeaponEffectDurationModifier);

	SCR_REGISTER_UNUSED(SET_RECOIL_ACCURACY_TO_ALLOW_HEADSHOT_PLAYER_MODIFIER,0xc7c735c837a6b4f9, CommandSetRecoilAccuracyToAllowHeadShotPlayerModifier);
	SCR_REGISTER_UNUSED(GET_RECOIL_ACCURACY_TO_ALLOW_HEADSHOT_PLAYER_MODIFIER,0xe9c6043c02b779f1, CommandGetRecoilAccuracyToAllowHeadShotPlayerModifier);
	SCR_REGISTER_UNUSED(SET_MIN_HEADSHOT_DISTANCE_PLAYER_MODIFIER,0xb4424a99f7c232cb, CommandSetMinHeadShotDistancePlayerModifier);
	SCR_REGISTER_UNUSED(GET_MIN_HEADSHOT_DISTANCE_PLAYER_MODIFIER,0x63a1d467d86075da, CommandGetMinHeadShotDistancePlayerModifier);
	SCR_REGISTER_UNUSED(SET_MAX_HEADSHOT_DISTANCE_PLAYER_MODIFIER,0xa0dcb3d3a2822b98, CommandSetMaxHeadShotDistancePlayerModifier);
	SCR_REGISTER_UNUSED(GET_MAX_HEADSHOT_DISTANCE_PLAYER_MODIFIER,0xd7179c1ce2a3773a, CommandGetMaxHeadShotDistancePlayerModifier);
	SCR_REGISTER_UNUSED(SET_HEADSHOT_DAMAGE_MODIFIER_PLAYER_MODIFIER,0x371fdb87c52a3699, CommandSetHeadShotDamageModifierPlayerModifier);
	SCR_REGISTER_UNUSED(GET_HEADSHOT_DAMAGE_MODIFIER_PLAYER_MODIFIER,0x579d07ce7434c633, CommandGetHeadShotDamageModifierPlayerModifier);

	SCR_REGISTER_SECURE(IS_PED_CURRENT_WEAPON_SILENCED,0x807d601fa146717a, CommandIsPedCurrentWeaponSilenced);
	SCR_REGISTER_SECURE(IS_FLASH_LIGHT_ON,0xed91b91cc1189b30,	CommandIsFlashLightOn);
	SCR_REGISTER_SECURE(SET_FLASH_LIGHT_FADE_DISTANCE,0xf6fa7ffe56554d2a, CommandSetFlashlightFadeDistance);
	SCR_REGISTER_SECURE(SET_FLASH_LIGHT_ACTIVE_HISTORY,0x5b71e8e84cae61b5,	CommandSetFlashlightActiveHistory);

	SCR_REGISTER_SECURE(SET_WEAPON_ANIMATION_OVERRIDE,0x5f04d63142241e1f, CommandWeaponAnimationOverride);
	
	SCR_REGISTER_SECURE(GET_WEAPON_DAMAGE_TYPE,0x963bc5e5049966d1, CommandGetWeaponDamageType);

	SCR_REGISTER_SECURE(SET_EQIPPED_WEAPON_START_SPINNING_AT_FULL_SPEED,0xf976379fb8b92eb4, CommandSetEqippedWeaponStartSpinningAtFullSpeed);

	SCR_REGISTER_SECURE(CAN_USE_WEAPON_ON_PARACHUTE,0x64cdf8f5c3d31323, CommandCanUseWeaponOnParachute);

	SCR_REGISTER_SECURE(CREATE_AIR_DEFENCE_SPHERE,0x28b8e32d8a6d5208, CommandCreateAirDefenceSphere);
	SCR_REGISTER_SECURE(CREATE_AIR_DEFENCE_ANGLED_AREA,0xac8961b74e9fb713, CommandCreateAirDefenceAngledArea);
	SCR_REGISTER_SECURE(REMOVE_AIR_DEFENCE_SPHERE,0x22833c17e06dae37, CommandRemoveAirDefenceSphere);
	SCR_REGISTER_UNUSED(REMOVE_AIR_DEFENCE_SPHERES_INTERSECTING_POS,0xe1fcbc1d4d8a6885, CommandRemoveAirDefenceSpheresInteresectingPosition);
	SCR_REGISTER_SECURE(REMOVE_ALL_AIR_DEFENCE_SPHERES,0xb4fc64537af766e8, CommandRemoveAllAirDefenceSpheres);
	SCR_REGISTER_UNUSED(SET_PLAYER_TARGETTABLE_FOR_ALL_AIR_DEFENCE_SPHERES,0xa7cba54dd809ad23, CommandSetPlayerTargettableForAllAirDefenceSpheres);
	SCR_REGISTER_SECURE(SET_PLAYER_TARGETTABLE_FOR_AIR_DEFENCE_SPHERE,0x29827c14db83cc07, CommandSetPlayerTargettableForAirDefenceSphere);
	SCR_REGISTER_UNUSED(RESET_PLAYER_TARGETTABLE_FLAGS_FOR_ALL_AIR_DEFENCE_SPHERES,0x3512766544034c1a, CommandResetPlayerTargettableFlagsForAllAirDefenceSpheres);
	SCR_REGISTER_UNUSED(RESET_PLAYER_TARGETTABLE_FLAGS_FOR_AIR_DEFENCE_SPHERE,0xa3535755b04054d0, CommandResetPlayetTargettableFlagsForAirDefenceSphere);
	SCR_REGISTER_SECURE(IS_AIR_DEFENCE_SPHERE_IN_AREA,0xe249916f8fdd6268, CommandIsAirDefenceSphereInArea);
	SCR_REGISTER_SECURE(FIRE_AIR_DEFENCE_SPHERE_WEAPON_AT_POSITION,0xc6eda100ecf72aac, CommandFireAirDefenceSphereWeaponAtPosition);
	SCR_REGISTER_SECURE(DOES_AIR_DEFENCE_SPHERE_EXIST,0xe2f5cc9ee3864144, CommandDoesAirDefenceSphereExist);

	SCR_REGISTER_SECURE(SET_CAN_PED_SELECT_INVENTORY_WEAPON,0x9e40fd8565afbd17, CommandSetCanPedSelectIventoryWeapon);
	SCR_REGISTER_SECURE(SET_CAN_PED_SELECT_ALL_WEAPONS,0x2155139b0979d9f6, CommandSetCanPedSelectAllWeapons);

	SCR_REGISTER_UNUSED(GET_WEAPON_HAS_NON_LETHAL_TAG,0x1a6484d80213bf9a, CommandGetWeaponHasNonLethalTag);
}

}; //namespace weapon_commands
