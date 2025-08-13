//
// weapons/pedinventorydebug.cpp
//
// Copyright (C) 1999-2010 Rockstar North.  All Rights Reserved. 
//

// File header
#include "Weapons/Inventory/PedInventory.h"

// Game headers
#include "Peds/Ped.h"
#include "Weapons/Components/WeaponComponentManager.h"
#include "Weapons/WeaponDebug.h"

WEAPON_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CPedInventory
//////////////////////////////////////////////////////////////////////////

#if __BANK


bool CPedInventory::ms_bRenderComponentsDebug = false;
bool  CPedInventory::ms_bRenderDebug = false;
bool CPedInventory::ms_bEquipImmediately = false;
s32 CPedInventory::ms_iWeaponComboIndex = 0;
s32 CPedInventory::ms_iAmmoComboIndex = 0;
s32 CPedInventory::ms_iComponentComboIndex = 0;
s32 CPedInventory::ms_iAmmoToGive = 100;
u8 CPedInventory::ms_uTintIndex = 0;
u8 CPedInventory::ms_uComponentTintIndex = 0;

void CPedInventory::AddWidgets(bkBank& bank)
{
	// Init control vars
	ms_iWeaponComboIndex = 0;
	ms_iComponentComboIndex = 0;

	bank.PushGroup("Inventory");

	bank.AddToggle("Render", &ms_bRenderDebug);
	bank.AddToggle("Render Components", &ms_bRenderComponentsDebug);
	bank.AddButton("Set Combo Boxes From Equipped Weapon", DebugDetectTypesCB);

	CWeaponInfoManager::StringList& weaponNames = CWeaponInfoManager::GetNames(CWeaponInfoBlob::IT_Weapon);
	if(weaponNames.GetCount() > 0)
	{
		bank.AddSeparator();
		bank.AddCombo("Weapon Type", &ms_iWeaponComboIndex, weaponNames.GetCount(), &weaponNames[0], DebugWeaponCB);

		bank.AddSlider("Ammo Amount", &ms_iAmmoToGive, -1000, 1000, 1);
		bank.AddButton("Give Weapon and Ammo", DebugGiveWeaponCB);
		bank.AddButton("Remove Weapon and Ammo", DebugRemoveWeaponCB);
		bank.AddButton("Equip Weapon", DebugEquipWeaponCB);
		bank.AddToggle("Equip Immediately", &ms_bEquipImmediately);
		bank.AddButton("Drop Weapon", DebugDropWeaponCB);
		bank.AddSlider("Tint Palette", &ms_uTintIndex, 0, 31, 1, DebugTintIndexCB);
	}

	CWeaponInfoManager::StringList& ammoNames = CWeaponInfoManager::GetNames(CWeaponInfoBlob::IT_Ammo);
	if(ammoNames.GetCount() > 0)
	{
		bank.AddSeparator();
		bank.PushGroup("Ammo");
		bank.AddCombo("Ammo Type", &ms_iAmmoComboIndex, ammoNames.GetCount(), &ammoNames[0]);
		bank.AddSlider("Ammo Amount", &ms_iAmmoToGive, -1000, 1000, 1);

		bank.AddButton("Give Ammo Amount", DebugGiveAmmoCB);
		bank.AddButton("Remove All Ammo of Type", DebugRemoveAmmoCB);
		bank.PopGroup();
	}

	CWeaponComponentManager::StringList& componentNames = CWeaponComponentManager::GetNames();
	if(componentNames.GetCount() > 0)
	{
		bank.AddSeparator();
		bank.PushGroup("Components");
		bank.AddCombo("Component", &ms_iComponentComboIndex, componentNames.GetCount(), &componentNames[0]);
		bank.AddButton("Give Component", DebugGiveComponentCB);
		bank.AddButton("Remove Component", DebugRemoveComponentCB);
		bank.AddSlider("Tint Palette", &ms_uComponentTintIndex, 0, 31, 1, DebugComponentTintIndexCB);
		bank.PopGroup();
	}

	bank.AddSeparator();
	bank.AddButton("Disable All Weapon Selection", DebugDisableAllWeaponSelectionCB);
	bank.AddButton("Enable All Weapon Selection", DebugEnableAllWeaponSelectionCB);

	bank.AddSeparator();
	bank.AddButton("Remove All Items", DebugRemoveAllItemsCB);
	bank.AddButton("Remove All Items Except Unarmed", DebugRemoveAllItemsExceptUnarmedCB);
#if __ASSERT
	bank.AddButton("Spew Pool Usage", CInventoryItem::SpewPoolUsage);
#endif // __ASSERT

	bank.PopGroup();
}

void CPedInventory::RenderDebug()
{
	if(ms_bRenderDebug)
	{
		const CPed* pFocusPed = CWeaponDebug::GetFocusPed();
		if(pFocusPed)
		{
			if(weaponVerifyf(pFocusPed->GetInventory(), "Focus ped does not have an inventory - maybe its an animal"))
			{
				char itemText[128];
				Vector3 vpos = VEC3V_TO_VECTOR3(pFocusPed->GetTransform().GetPosition());
				vpos.z += 0.5f;

				const CPedEquippedWeapon* pEquippedWeap = pFocusPed->GetWeaponManager()->GetPedEquippedWeapon();
				if (pEquippedWeap)
				{
					const CWeaponInfo* pVehWeaponInfo = pEquippedWeap->GetVehicleWeaponInfo();
					if (pVehWeaponInfo)
					{
						sprintf(itemText, "Equipped Vehicle Weapon: %s\n", pVehWeaponInfo->GetName());
						grcDebugDraw::Text(vpos, Color_white, 0, grcDebugDraw::GetScreenSpaceTextHeight() * -4, itemText);
					}
				}

				const CWeapon* pEquippedWeapon = pFocusPed->GetWeaponManager() ? pFocusPed->GetWeaponManager()->GetEquippedWeapon() : NULL;
				if(pEquippedWeapon)
				{
					sprintf(itemText, "Equipped Weapon: %s (%d)\n", pEquippedWeapon->GetWeaponInfo()->GetName(), pFocusPed->GetInventory()->GetWeaponAmmo(pEquippedWeapon->GetWeaponInfo()));
					grcDebugDraw::Text(vpos, Color_white, 0, grcDebugDraw::GetScreenSpaceTextHeight() * -3, itemText);
		
					const CAmmoInfo* pEquippedAmmoInfo = pEquippedWeapon->GetWeaponInfo()->GetAmmoInfo(pFocusPed);
					sprintf(itemText, "Equipped Ammo:   %s (%d)\n", pEquippedAmmoInfo ? pEquippedAmmoInfo->GetName() : "NULL", pEquippedAmmoInfo ? pFocusPed->GetInventory()->GetAmmo(pEquippedAmmoInfo->GetHash()) : 0);
					grcDebugDraw::Text(vpos, Color_white, 0, grcDebugDraw::GetScreenSpaceTextHeight() * -2, itemText);
				}

				const CWeaponInfo* pBestWeaponInfo = pFocusPed->GetWeaponManager() ? pFocusPed->GetWeaponManager()->GetBestWeaponInfo() : NULL;
				if(pBestWeaponInfo)
				{
					sprintf(itemText, "Best Weapon:     %s (%d)\n", pBestWeaponInfo->GetName(), pFocusPed->GetInventory()->GetWeaponAmmo(pBestWeaponInfo));
					grcDebugDraw::Text(vpos, Color_white, 0, grcDebugDraw::GetScreenSpaceTextHeight() * -1, itemText);
				}

				pFocusPed->GetInventory()->GetWeaponRepository().RenderWeaponDebug(vpos, Color_wheat, ms_bRenderComponentsDebug);
				pFocusPed->GetInventory()->GetAmmoRepository().RenderAmmoDebug(vpos, Color_wheat);
			}
		}
	}
}

void CPedInventory::DebugDetectTypesCB()
{
	CPed* pFocusPed = CWeaponDebug::GetFocusPed();
	if(pFocusPed)
	{
		const CWeaponInfo* pEquippedWeaponInfo = pFocusPed->GetEquippedWeaponInfo();
		if (pEquippedWeaponInfo)
		{
			u32 uWeaponHash = pEquippedWeaponInfo->GetHash();
			CWeaponInfoManager::StringList& weaponNames = CWeaponInfoManager::GetNames(CWeaponInfoBlob::IT_Weapon);
			for (int i = 0; i < weaponNames.GetCount(); i++)
			{
				if (uWeaponHash == atStringHash(weaponNames[i]))
				{
					ms_iWeaponComboIndex = i;
					DebugWeaponCB();
					break;
				}
			}

			const CAmmoInfo* pEquippedAmmoInfo = pEquippedWeaponInfo->GetAmmoInfo(pFocusPed);
			if (pEquippedAmmoInfo)
			{
				u32 uAmmoHash = pEquippedAmmoInfo->GetHash();
				CWeaponInfoManager::StringList& ammoNames = CWeaponInfoManager::GetNames(CWeaponInfoBlob::IT_Ammo);
				for (int i = 0; i < ammoNames.GetCount(); i++)
				{
					if (uAmmoHash == atStringHash(ammoNames[i]))
					{
						ms_iAmmoComboIndex = i;
						break;
					}
				}
			}
		}
	}
}

void CPedInventory::DebugWeaponCB()
{
	ms_iAmmoToGive = 100;
	ms_uTintIndex = 0;
}

void CPedInventory::DebugGiveWeaponCB()
{
	CPed* pFocusPed = CWeaponDebug::GetFocusPed();
	if(pFocusPed)
	{
		CWeaponInfoManager::StringList& weaponNames = CWeaponInfoManager::GetNames(CWeaponInfoBlob::IT_Weapon);
		if(ms_iWeaponComboIndex >= 0 && ms_iWeaponComboIndex < weaponNames.GetCount())
		{
			if(weaponVerifyf(pFocusPed->GetInventory(), "Focus ped does not have an inventory - maybe its an animal"))
			{
				pFocusPed->GetInventory()->AddWeaponAndAmmo(atStringHash(weaponNames[ms_iWeaponComboIndex]), ms_iAmmoToGive);
			}
		}
	}
}

void CPedInventory::DebugRemoveWeaponCB()
{
	CPed* pFocusPed = CWeaponDebug::GetFocusPed();
	if(pFocusPed)
	{
		CWeaponInfoManager::StringList& weaponNames = CWeaponInfoManager::GetNames(CWeaponInfoBlob::IT_Weapon);
		if(ms_iWeaponComboIndex >= 0 && ms_iWeaponComboIndex < weaponNames.GetCount())
		{
			if(weaponVerifyf(pFocusPed->GetInventory(), "Focus ped does not have an inventory - maybe its an animal"))
			{
				const u32 uWeaponHash = atStringHash(weaponNames[ms_iWeaponComboIndex]);
				pFocusPed->GetInventory()->RemoveWeapon(uWeaponHash);

				const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(uWeaponHash);
				if(pWeaponInfo)
				{
					const CAmmoInfo* pAmmoInfo = pWeaponInfo->GetAmmoInfo();
					if(pAmmoInfo)
					{
						pFocusPed->GetInventory()->GetAmmoRepository().RemoveItem(pAmmoInfo->GetHash());
					}
				}
			}
		}
	}
}

void CPedInventory::DebugEquipWeaponCB()
{
	CPed* pFocusPed = CWeaponDebug::GetFocusPed();
	if(pFocusPed)
	{
		CWeaponInfoManager::StringList& weaponNames = CWeaponInfoManager::GetNames(CWeaponInfoBlob::IT_Weapon);

		if(weaponVerifyf(pFocusPed->GetWeaponManager(), "Focus ped does not have a Weapon Manager - maybe its an animal"))
		{
			if(ms_iWeaponComboIndex >= 0 && ms_iWeaponComboIndex < weaponNames.GetCount())
			{
				pFocusPed->GetWeaponManager()->EquipWeapon(atStringHash(weaponNames[ms_iWeaponComboIndex]), -1, ms_bEquipImmediately ? true : !pFocusPed->IsPlayer(), ms_bEquipImmediately);
			}
		}
	}
}

void CPedInventory::DebugDropWeaponCB()
{
	CPed* pFocusPed = CWeaponDebug::GetFocusPed();
	if(pFocusPed)
	{
		CWeaponInfoManager::StringList& weaponNames = CWeaponInfoManager::GetNames(CWeaponInfoBlob::IT_Weapon);
		if(weaponVerifyf(pFocusPed->GetWeaponManager(), "Focus ped does not have a Weapon Manager - maybe its an animal"))
		{
			if(ms_iWeaponComboIndex >= 0 && ms_iWeaponComboIndex < weaponNames.GetCount())
			{
				pFocusPed->GetWeaponManager()->DropWeapon(atStringHash(weaponNames[ms_iWeaponComboIndex]), true);
			}
		}
	}
}

void CPedInventory::DebugTintIndexCB()
{
	CPed* pFocusPed = CWeaponDebug::GetFocusPed();
	if(pFocusPed)
	{
		CWeaponInfoManager::StringList& weaponNames = CWeaponInfoManager::GetNames(CWeaponInfoBlob::IT_Weapon);
		if(ms_iWeaponComboIndex >= 0 && ms_iWeaponComboIndex < weaponNames.GetCount())
		{
			u32 uHash = atStringHash(weaponNames[ms_iWeaponComboIndex]);
			if(weaponVerifyf(pFocusPed->GetInventory(), "Focus ped does not have an inventory - maybe its an animal"))
			{
				CWeaponItem* pWeaponItem = pFocusPed->GetInventory()->GetWeapon(uHash);
				if(pWeaponItem)
				{
					pWeaponItem->SetTintIndex(ms_uTintIndex);
				}
			}

			if(weaponVerifyf(pFocusPed->GetWeaponManager(), "Focus ped does not have a Weapon Manager - maybe its an animal"))
			{
				CObject* pWeaponObject = pFocusPed->GetWeaponManager()->GetEquippedWeaponObject();
				if(pWeaponObject && pWeaponObject->GetWeapon() && pWeaponObject->GetWeapon()->GetWeaponHash() == uHash)
				{
					// Force update on the equipped weapon
					pWeaponObject->GetWeapon()->UpdateShaderVariables(ms_uTintIndex);
				}
			}
		}
	}
}

void CPedInventory::DebugComponentTintIndexCB()
{
	CPed* pFocusPed = CWeaponDebug::GetFocusPed();
	if(pFocusPed)
	{
		CWeaponInfoManager::StringList& weaponNames = CWeaponInfoManager::GetNames(CWeaponInfoBlob::IT_Weapon);
		if(ms_iWeaponComboIndex >= 0 && ms_iWeaponComboIndex < weaponNames.GetCount())
		{
			CWeaponComponentManager::StringList& componentNames = CWeaponComponentManager::GetNames();
			if(ms_iComponentComboIndex >= 0 && ms_iComponentComboIndex < componentNames.GetCount())
			{
				u32 uWeaponHash = atStringHash(weaponNames[ms_iWeaponComboIndex]);
				u32 uComponentHash = atStringHash(componentNames[ms_iComponentComboIndex]);

				if(weaponVerifyf(pFocusPed->GetInventory(), "Focus ped does not have an inventory - maybe its an animal"))
				{
					CWeaponItem* pWeaponItem = pFocusPed->GetInventory()->GetWeapon(uWeaponHash);
					if(pWeaponItem)
					{
						// Update tint on weapon item
						if(pWeaponItem->GetComponents())
						{
							const CWeaponItem::Components& components = *pWeaponItem->GetComponents();
							for(s32 i = 0; i < components.GetCount(); i++)
							{
								if(components[i].pComponentInfo->GetHash() == uComponentHash)
								{
									pWeaponItem->SetComponentTintIndex(i, ms_uComponentTintIndex);
								}
							}
						}

						// Update component tint on active weapon
						if(pFocusPed->GetWeaponManager())
						{
							CWeapon* pWeapon = pFocusPed->GetWeaponManager()->GetEquippedWeapon();
							if(pWeapon && pWeapon->GetWeaponHash() == uWeaponHash)
							{
								const CWeapon::Components& components = pWeapon->GetComponents();
								for(s32 i = 0; i < components.GetCount(); i++)
								{
									if(components[i]->GetInfo()->GetHash() == uComponentHash)
									{
										components[i]->UpdateShaderVariables(ms_uComponentTintIndex);
									}
								}
							}
						}
					}
				}
			}
		}
	}
}

void CPedInventory::DebugGiveAmmoCB()
{
	CPed* pFocusPed = CWeaponDebug::GetFocusPed();
	if(pFocusPed)
	{
		CWeaponInfoManager::StringList& ammoNames = CWeaponInfoManager::GetNames(CWeaponInfoBlob::IT_Ammo);
		if(ms_iAmmoComboIndex >= 0 && ms_iAmmoComboIndex < ammoNames.GetCount())
		{
			if(weaponVerifyf(pFocusPed->GetInventory(), "Focus ped does not have an inventory - maybe its an animal"))
			{
				pFocusPed->GetInventory()->AddAmmo(atStringHash(ammoNames[ms_iAmmoComboIndex]), ms_iAmmoToGive);
			}
		}
	}
}

void CPedInventory::DebugRemoveAmmoCB()
{
	CPed* pFocusPed = CWeaponDebug::GetFocusPed();
	if(pFocusPed)
	{
		CWeaponInfoManager::StringList& ammoNames = CWeaponInfoManager::GetNames(CWeaponInfoBlob::IT_Ammo);
		if(ms_iAmmoComboIndex >= 0 && ms_iAmmoComboIndex < ammoNames.GetCount())
		{
			if(weaponVerifyf(pFocusPed->GetInventory(), "Focus ped does not have an inventory - maybe its an animal"))
			{
				const CAmmoInfo* pAmmoInfo = CWeaponInfoManager::GetInfo<CAmmoInfo>(ammoNames[ms_iAmmoComboIndex]);
				if(pAmmoInfo)
				{
					pFocusPed->GetInventory()->GetAmmoRepository().RemoveItem(pAmmoInfo->GetHash());
				}
			}
		}
	}
}

void CPedInventory::DebugGiveComponentCB()
{
	CPed* pFocusPed = CWeaponDebug::GetFocusPed();
	if(pFocusPed)
	{
		CWeaponInfoManager::StringList& weaponNames = CWeaponInfoManager::GetNames(CWeaponInfoBlob::IT_Weapon);
		if(ms_iWeaponComboIndex >= 0 && ms_iWeaponComboIndex < weaponNames.GetCount())
		{
			CWeaponComponentManager::StringList& componentNames = CWeaponComponentManager::GetNames();
			if(ms_iComponentComboIndex >= 0 && ms_iComponentComboIndex < componentNames.GetCount())
			{
				if(weaponVerifyf(pFocusPed->GetInventory(), "Focus ped does not have an inventory - maybe its an animal"))
				{
					pFocusPed->GetInventory()->GetWeaponRepository().AddComponent(atStringHash(weaponNames[ms_iWeaponComboIndex]), atStringHash(componentNames[ms_iComponentComboIndex]));
				}
			}
		}
	}
}

void CPedInventory::DebugRemoveComponentCB()
{
	CPed* pFocusPed = CWeaponDebug::GetFocusPed();
	if(pFocusPed)
	{
		CWeaponInfoManager::StringList& weaponNames = CWeaponInfoManager::GetNames(CWeaponInfoBlob::IT_Weapon);
		if(ms_iWeaponComboIndex >= 0 && ms_iWeaponComboIndex < weaponNames.GetCount())
		{
			CWeaponComponentManager::StringList& componentNames = CWeaponComponentManager::GetNames();
			if(ms_iComponentComboIndex >= 0 && ms_iComponentComboIndex < componentNames.GetCount())
			{
				if(weaponVerifyf(pFocusPed->GetInventory(), "Focus ped does not have an inventory - maybe its an animal"))
				{
					pFocusPed->GetInventory()->GetWeaponRepository().RemoveComponent(atStringHash(weaponNames[ms_iWeaponComboIndex]), atStringHash(componentNames[ms_iComponentComboIndex]));
				}
			}
		}
	}
}

void CPedInventory::DebugRemoveAllItemsCB()
{
	CPed* pFocusPed = CWeaponDebug::GetFocusPed();
	if(pFocusPed)
	{
		if(weaponVerifyf(pFocusPed->GetInventory(), "Focus ped does not have an inventory - maybe its an animal"))
		{
			pFocusPed->GetInventory()->RemoveAll();
		}
	}
}

void CPedInventory::DebugRemoveAllItemsExceptUnarmedCB()
{
	CPed* pFocusPed = CWeaponDebug::GetFocusPed();
	if(pFocusPed)
	{
		if(weaponVerifyf(pFocusPed->GetInventory(), "Focus ped does not have an inventory - maybe its an animal"))
		{
			pFocusPed->GetInventory()->RemoveAll();
			pFocusPed->GetInventory()->AddWeaponAndAmmo(pFocusPed->GetDefaultUnarmedWeaponHash(),0);
		}
	}
}

void CPedInventory::DebugEnableAllWeaponSelectionCB()
{
	CPed* pFocusPed = CWeaponDebug::GetFocusPed();
	if(pFocusPed)
	{
		CPedInventory* pPedInventory = pFocusPed->GetInventory();
		if(pPedInventory)
		{
			CWeaponItemRepository& rWeaponRepository = pPedInventory->GetWeaponRepository();
			for(int i=0; i < rWeaponRepository.GetItemCount(); i++)
			{
				CWeaponItem* pWeaponItem = rWeaponRepository.GetItemByIndex(i);
				if (pWeaponItem)
				{
					pWeaponItem->SetCanSelect(true);
				}
			}
		}
	}
}

void CPedInventory::DebugDisableAllWeaponSelectionCB()
{
	CPed* pFocusPed = CWeaponDebug::GetFocusPed();
	if(pFocusPed)
	{
		CPedInventory* pPedInventory = pFocusPed->GetInventory();
		if(pPedInventory)
		{
			CWeaponItemRepository& rWeaponRepository = pPedInventory->GetWeaponRepository();
			for(int i=0; i < rWeaponRepository.GetItemCount(); i++)
			{
				CWeaponItem* pWeaponItem = rWeaponRepository.GetItemByIndex(i);
				if (pWeaponItem)
				{
					pWeaponItem->SetCanSelect(false);
				}
			}
		}
	}
}

#endif // __BANK
