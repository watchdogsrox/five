// File header
#include "Weapons/Inventory/PedWeaponManager.h"
#include "fragmentnm/messageparams.h"

// Game headers
#include "control/replay/replay.h"
#include "Peds/PedIntelligence.h"
#include "Physics/GtaInst.h"
#include "Pickups/PickupManager.h"
#include "Task/motion/Locomotion/TaskMotionPed.h"
#include "Vehicles/VehicleGadgets.h"
#include "Weapons/Gadgets/Gadget.h"
#include "Weapons/projectiles/ProjectileManager.h"
#include "Weapons/projectiles/Projectile.h"
#include "Weapons/Weapon.h"
#include "Weapons/WeaponChannel.h"
#include "Weapons/WeaponFactory.h"
#include "Weapons/WeaponManager.h"
#include "Task/Combat/Cover/TaskCover.h"
#include "Task/Physics/TaskNM.h"
#include "physics/constraintfixed.h"
#include "physics/physics.h"
#include "phbound/boundcomposite.h"
#include "stats/StatsInterface.h"
#include "Stats/stats_channel.h"
#include "frontend/NewHud.h"
#include "Task/General/Phone/TaskMobilePhone.h"
#include "Task/Weapons/Gun/TaskReloadGun.h"

// Macro to disable optimisations if set
WEAPON_OPTIMISATIONS()
AI_COVER_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CPedWeaponManager
//////////////////////////////////////////////////////////////////////////

CPedWeaponManager::CPedWeaponManager()
: m_pPed(NULL)
, m_iEquippedVehicleWeaponIndex(-1)
, m_uEquippedWeaponHashBackup(0)
, m_fChanceToFireBlanksMin(0.0f)
, m_fChanceToFireBlanksMax(0.5f)
, m_uLastEquipOrFireTime(0)
, m_vWeaponAimPosition(Vector3::ZeroType)
, m_vImpactPosition(Vector3::ZeroType)
, m_iImpactTime(0)
, m_flags(0)
, m_nInstruction(WI_None)
, m_pLastEquippedWeaponInfo(NULL)
, m_bForceReloadOnEquip(false)
{
}

CPedWeaponManager::~CPedWeaponManager()
{
	DestroyEquippedGadgets();
}

void CPedWeaponManager::Init(CPed* pPed)
{
	m_pPed = pPed;

	// Register as an inventory listener
	weaponAssert(m_pPed->GetInventory());
	m_pPed->GetInventory()->RegisterListener(this);

	// Equipped weapon
	m_equippedWeapon.Init(m_pPed);
}

void CPedWeaponManager::ProcessPreAI()
{
	if(!m_pPed->IsNetworkClone() && !m_pPed->GetIsDeadOrDying())
	{
		m_selector.Process(m_pPed);
	}

	if(m_flags.IsFlagSet(Flag_SyncAmmo))
	{
		// Ensure equipped weapon ammo is in sync with inventory ammo
		CWeapon* pWeapon = GetEquippedWeapon();
		if(pWeapon)
		{
			pWeapon->SetAmmoTotal(m_pPed->GetInventory()->GetWeaponAmmo(pWeapon->GetWeaponInfo()));
		}
	}
}

void CPedWeaponManager::ProcessPostAI()
{
	// only update if we have an instruction
	if(m_nInstruction != WI_None)
	{
		ProcessWeaponInstructions(m_pPed);
	}

	if(m_flags.IsFlagSet(Flag_CreateWeaponWhenLoaded))
	{
		bool bLeftHand = m_flags.IsFlagSet(Flag_DelayedWeaponInLeftHand);
		if(CreateEquippedWeaponObject(bLeftHand ? CPedEquippedWeapon::AP_LeftHand : CPedEquippedWeapon::AP_RightHand))
		{
			m_flags.ClearFlag(Flag_CreateWeaponWhenLoaded);
			m_flags.ClearFlag(Flag_DelayedWeaponInLeftHand);
		}
	}

	if(m_flags.IsFlagSet(Flag_SyncAmmo))
	{
		// Ensure equipped weapon ammo is in sync with inventory ammo
		CWeapon* pWeapon = GetEquippedWeapon();
		if(pWeapon)
		{
			pWeapon->SetAmmoTotal(m_pPed->GetInventory()->GetWeaponAmmo(pWeapon->GetWeaponInfo()));
		}
	}

	// Tick the equipped weapon
	m_equippedWeapon.Process();
}

void CPedWeaponManager::UnregisterNMWeapon()
{
	// Inform NM that the weapon has been dropped, if applicable
	if (m_pPed && m_pPed->GetUsingRagdoll() && m_pPed->GetRagdollInst()->GetNMAgentID() >= 0)
	{
		ART::MessageParams msgPointGun;
		msgPointGun.addBool(NMSTR_PARAM(NM_START), false);
		m_pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_POINTGUN_MSG), &msgPointGun);

		ART::MessageParams msgWeaponMode;
		msgWeaponMode.addInt(NMSTR_PARAM(NM_SETWEAPONMODE_MODE), -1);
		m_pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_SETWEAPONMODE_MSG), &msgWeaponMode);

		ART::MessageParams msgRegisterWeapon;
		msgRegisterWeapon.addBool(NMSTR_PARAM(NM_START), false);
		msgRegisterWeapon.addInt(NMSTR_PARAM(NM_REGISTER_WEAPON_HAND), 1);
		m_pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_REGISTER_WEAPON_MSG), &msgRegisterWeapon);

		//
		CTaskNMControl* pTask = static_cast<CTaskNMControl*>(m_pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_NM_CONTROL));
		
		if (pTask)
		{
			if (pTask->GetCurrentHandPose(CMovePed::kHandLeft)==CTaskNMControl::HAND_POSE_HOLD_WEAPON)
			{
				pTask->SetCurrentHandPose(CTaskNMControl::HAND_POSE_LOOSE, CMovePed::kHandLeft, SLOW_BLEND_DURATION);
			}
			if (pTask->GetCurrentHandPose(CMovePed::kHandRight)==CTaskNMControl::HAND_POSE_HOLD_WEAPON)
			{
				pTask->SetCurrentHandPose(CTaskNMControl::HAND_POSE_LOOSE, CMovePed::kHandRight, SLOW_BLEND_DURATION);
			}
		}
	}
}

bool CPedWeaponManager::EquipWeapon(u32 uWeaponNameHash, s32 iVehicleIndex, bool bCreateWeaponWhenLoaded, bool bProcessWeaponInstructions, CPedEquippedWeapon::eAttachPoint attach)
{
	weaponFatalAssertf(m_pPed, "m_pPed is NULL");

	AI_LOG_WITH_ARGS("[EQUIP_WEAPON] - %s ped %s called  CPedWeaponManager::EquipWeapon : %s (%i)(%s)(%s)(%i)\n", AILogging::GetDynamicEntityIsCloneStringSafe(m_pPed), AILogging::GetDynamicEntityNameSafe(m_pPed), atHashString(uWeaponNameHash).GetCStr(), iVehicleIndex, AILogging::GetBooleanAsString(bCreateWeaponWhenLoaded), AILogging::GetBooleanAsString(bProcessWeaponInstructions), attach);

#if !__NO_OUTPUT
	if (NetworkInterface::IsGameInProgress() && m_pPed)
	{
		weaponitemDebugf2("CPedWeaponManager::EquipWeapon m_pPed[%p] isplayer[%d] player[%s] weaponslot[%d] uWeaponNameHash[%u] iVehicleIndex[%d] bCreateWeaponWhenLoaded[%d] bProcessWeaponInstruction[%d] attach[%d]",
			m_pPed,
			m_pPed->IsPlayer(),
			m_pPed->GetNetworkObject() && m_pPed->GetNetworkObject()->GetPlayerOwner() ? m_pPed->GetNetworkObject()->GetPlayerOwner()->GetLogName() : "",
			CWeaponInfoManager::GetInfoSlotForWeapon(uWeaponNameHash),
			uWeaponNameHash,iVehicleIndex,bCreateWeaponWhenLoaded,bProcessWeaponInstructions,attach);
	}
#endif

	if(!CanAlterEquippedWeapon())
	{
#if !__NO_OUTPUT
		if (NetworkInterface::IsGameInProgress() && m_pPed)
		{
			weaponitemDebugf2("!CanAlterEquippedWeapon --> return false");
		}
#endif
		return false;
	}

	if(!m_pPed->IsNetworkClone())
	{
		const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(uWeaponNameHash);
		if(m_pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsHandCuffed) && pWeaponInfo && !pWeaponInfo->GetIsUnarmed())
		{
#if !__NO_OUTPUT
			if (NetworkInterface::IsGameInProgress() && m_pPed)
			{
				weaponitemDebugf2("CPED_CONFIG_FLAG_IsHandCuffed && !GetIsUnarmed --> return false");
			}
#endif
			return false;
		}

		// "Reset" the firing pattern on weapon change
		// ...Unless we are throwing grenade from aiming. This was causing us to re-enter cover in CTaskInCover::Aim_OnUpdate after re-equipping the gun.
		// Also don't do this for firing patterns flagged as 'burst cooldown', as it screws up values that we need.
		if (!m_pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ThrowingGrenadeWhileAiming) && !m_pPed->GetPedIntelligence()->GetFiringPattern().HasBurstCooldownOnUnset())
		{
			m_pPed->GetPedIntelligence()->GetFiringPattern().ProcessBurstFinished();
		}
	}

	m_bForceReloadOnEquip = false;

	// If the ped manually equips weapons 
	if( m_pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_KeepWeaponHolsteredUnlessFired ) )
	{
		// Make the weapon visible again
		m_pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_KeepWeaponHolsteredUnlessFired, false);
	}

	if(uWeaponNameHash != 0)
	{
		// If we should process weapon instructions, our desired weapon is not our current and we have a weapon object
		if(bProcessWeaponInstructions && uWeaponNameHash != m_equippedWeapon.GetWeaponHash() && GetEquippedWeaponObject())
		{
			// Continue processing the weapon instructions until we don't have one
			int iMaxWeaponInstructionIterations = 5;
			for(int i = 0; i < iMaxWeaponInstructionIterations && m_nInstruction != WI_None; i++)
			{
				ProcessWeaponInstructions(m_pPed);
			}
			Assertf(m_nInstruction == WI_None, "Weapon instruction still exists (%d). All instructions should eventually lead to WI_None", m_nInstruction);
		}

		// Ensure the selected weapon is the one we are equipping, for sake of consistency
		//@@: location CPEDWEAPONMANAGER_EQUIPWEAPON
		m_selector.SelectWeapon(m_pPed, SET_SPECIFIC_ITEM, false, uWeaponNameHash);

		// Vehicle weapon is invalid
		weaponDebugf3("CPedWeaponManager::EquipWeapon--1 m_iEquippedVehicleWeaponIndex = -1, pPed: 0x%p, Frame:%d", m_pPed, fwTimer::GetFrameCount());
		m_iEquippedVehicleWeaponIndex = -1;

		const CInventoryItem* pItem = m_pPed->GetInventory()->GetWeapon(uWeaponNameHash);
		if(pItem)
		{
			if(uWeaponNameHash == GADGETTYPE_SKIS || 
				uWeaponNameHash == GADGETTYPE_PARACHUTE || 
				uWeaponNameHash == GADGETTYPE_JETPACK )
			{
				if(GetIsGadgetEquipped(uWeaponNameHash) && !bCreateWeaponWhenLoaded)
				{
					DestroyEquippedGadget(uWeaponNameHash);
				}
				else if(bCreateWeaponWhenLoaded)
				{
					CWeapon* pWeapon = WeaponFactory::Create(uWeaponNameHash,CWeapon::INFINITE_AMMO,NULL,"CPedWeaponManager::EquipWeapon");
					CGadget* pGadget = dynamic_cast<CGadget*>(pWeapon);
					if(pGadget)
					{
						if(pGadget->Created(m_pPed))
						{				
							m_equippedGadgets.PushAndGrow(pGadget);
						}
						else
						{
							delete pGadget;
						}
					}
					else
					{
						delete pWeapon;
					}
				}
			}
			else
			{
				m_pLastEquippedWeaponInfo = m_equippedWeapon.GetWeaponInfo();

				if(m_equippedWeapon.GetObject())
				{
					m_equippedWeapon.GetObject()->SetCanShootThroughWhenEquipped(false);
				}

				AI_LOG_WITH_ARGS("[EQUIP_WEAPON] - %s ped %s equipped weapon successfully : %s (%i)(%s)(%s)(%i)\n", AILogging::GetDynamicEntityIsCloneStringSafe(m_pPed), AILogging::GetDynamicEntityNameSafe(m_pPed), atHashString(uWeaponNameHash).GetCStr(), iVehicleIndex, AILogging::GetBooleanAsString(bCreateWeaponWhenLoaded), AILogging::GetBooleanAsString(bProcessWeaponInstructions), attach);
				m_equippedWeapon.SetWeaponHash(uWeaponNameHash);
				if (!m_pLastEquippedWeaponInfo || m_pLastEquippedWeaponInfo->GetHash() != uWeaponNameHash)
				{
					m_pPed->SetPedResetFlag(CPED_RESET_FLAG_EquippedWeaponChanged, true);

#if KEYBOARD_MOUSE_SUPPORT
					// Drop out of aim when switching to a sniper gun.
					const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(uWeaponNameHash);
					if(pWeaponInfo && m_pPed && m_pPed->IsLocalPlayer() && pWeaponInfo->GetHasFirstPersonScope() && CControlMgr::GetPlayerMappingControl().GetToggleAimEnabled())
					{
						CControlMgr::GetPlayerMappingControl().SetToggleAimOn(false);
					}
#endif // KEYBOARD_MOUSE_SUPPORT
				}

				if(bCreateWeaponWhenLoaded)
				{
					bool bCreateEquippedSuccess = CreateEquippedWeaponObject(attach);

					if (NetworkInterface::IsGameInProgress() && bCreateEquippedSuccess && GetCreateWeaponObjectWhenLoaded() )
					{
						//If network game requested this to be loaded then clear request here so a later attempt wont be made
						//in the same frame by PedWeaponManager::ProcessPostAI under CNetObjPed::ProcessControl, which if called
						//could reattach the prop to the wrong hand as Flag_DelayedWeaponInLeftHand wont necessarily be valid
						SetCreateWeaponObjectWhenLoaded(false);
					}
				}

				if(m_pPed->IsLocalPlayer() && pItem->GetInfo())
				{
					g_WeaponAudioEntity.AddPlayerEquippedWeapon(uWeaponNameHash, pItem->GetInfo()->GetAudioHash());
					StatsInterface::UpdateLocalPlayerWeaponInfoStatId(m_equippedWeapon.GetWeaponInfo(), NULL);
				}
			}
		}
	}
	else if(iVehicleIndex != -1)
	{
		// Ensure the selected weapon is the one we are equipping, for sake of consistency
		m_selector.SelectWeapon(m_pPed, SET_SPECIFIC_ITEM, false, 0, iVehicleIndex);

		// Ped weapon is invalid
		m_equippedWeapon.DestroyObject();
		m_equippedWeapon.SetVehicleWeaponInfo(GetSelectedVehicleWeaponInfo());

		// Equipped vehicle weapon
		weaponDebugf3("CPedWeaponManager::EquipWeapon--2 m_iEquippedVehicleWeaponIndex = %d, pPed: 0x%p, Frame: %d",iVehicleIndex, m_pPed, fwTimer::GetFrameCount());
		m_iEquippedVehicleWeaponIndex = iVehicleIndex;
		m_pPed->SetPedResetFlag(CPED_RESET_FLAG_EquippedWeaponChanged, true);

		if(m_pPed->IsLocalPlayer()) 
		{
			const CVehicleWeapon* pVehicleWeapon = GetEquippedVehicleWeapon();
			if(pVehicleWeapon && pVehicleWeapon->GetWeaponInfo())
			{
				g_WeaponAudioEntity.AddPlayerEquippedWeapon(pVehicleWeapon->GetHash(), pVehicleWeapon->GetWeaponInfo()->GetAudioHash());

				const CVehicle* pVehicle = m_pPed->GetVehiclePedInside();
				if (pVehicle && pVehicle->GetVehicleModelInfo())
				{
#if __ASSERT
					const char* vehicleName = pVehicle->GetVehicleModelInfo()->GetGameName();
					if (AssertVerify(vehicleName)) statAssertf(vehicleName[0] != '\0', "Vehicle name \"%s\" is empty for Vehicle Model Info \"%s:%d\"", pVehicle->GetVehicleModelInfo()->GetVehicleMakeName(), pVehicle->GetVehicleModelInfo()->GetModelName(), pVehicle->GetVehicleModelInfo()->GetModelNameHash());
#endif //__ASSERT

					StatsInterface::UpdateLocalPlayerWeaponInfoStatId(pVehicleWeapon->GetWeaponInfo(), pVehicle->GetVehicleModelInfo()->GetGameName());
				}
			}
		}
		else
		{
			const CVehicle* pVehicle = m_pPed->GetVehiclePedInside();
			if(!pVehicle)
			{
				return false;
			}
		}
	}
	else
	{
		// Ensure the selected weapon is the one we are equipping, for sake of consistency
		m_selector.SelectWeapon(m_pPed, SET_SPECIFIC_ITEM, false, 0, -1);

		// Clear both weapons - they are both invalid
		m_equippedWeapon.DestroyObject();
		m_equippedWeapon.SetWeaponHash(0);
		m_equippedWeapon.SetVehicleWeaponInfo(NULL);
		weaponDebugf3("CPedWeaponManager::EquipWeapon--3 m_iEquippedVehicleWeaponIndex = -1, pPed: 0x%p, Frame: %d", m_pPed, fwTimer::GetFrameCount());
		m_iEquippedVehicleWeaponIndex = -1;
	}

	SetWeaponInstruction(WI_None);

	// Inform NM that the current weapon has been unequipped, if applicable
	UnregisterNMWeapon();

	//Check if we tried to equip a weapon.
	if(uWeaponNameHash != 0)
	{
		return (m_equippedWeapon.GetWeaponHash() == uWeaponNameHash);
	}
	//Check if we tried to equip a vehicle weapon.
	else if(iVehicleIndex != -1)
	{
		return (m_iEquippedVehicleWeaponIndex == iVehicleIndex);
	}
	else
	{
		//I guess here we should just return we have absolutely nothing equipped.
		//Sticking with the old logic seems to be OK.
		return m_equippedWeapon.GetWeaponHash() == uWeaponNameHash || m_iEquippedVehicleWeaponIndex == iVehicleIndex;
	}
}

bool CPedWeaponManager::EquipBestWeapon(bool bForceIntoHand, bool bProcessWeaponInstructions)
{ 
	const CWeaponInfo* pEquippedWeaponInfo = GetEquippedWeaponInfo();
	if(pEquippedWeaponInfo && pEquippedWeaponInfo->GetIsNonLethal())
	{
		if(m_selector.SelectWeapon(m_pPed, SET_BEST_NONLETHAL))
			if(m_selector.GetSelectedWeaponInfo() && !GetSelectedWeaponInfo()->GetIsUnarmed())
				return EquipWeapon(m_selector.GetSelectedWeapon(), m_selector.GetSelectedVehicleWeapon(), bForceIntoHand, bProcessWeaponInstructions); 
	}

	if(m_selector.SelectWeapon(m_pPed, SET_BEST)) 
		return EquipWeapon(m_selector.GetSelectedWeapon(), m_selector.GetSelectedVehicleWeapon(), bForceIntoHand, bProcessWeaponInstructions); 

	return false; 
}

bool CPedWeaponManager::EquipBestMeleeWeapon(bool bForceIntoHand, bool bProcessWeaponInstructions)
{ 
	if(m_selector.SelectWeapon(m_pPed, SET_BEST_MELEE)) 
		return EquipWeapon(m_selector.GetSelectedWeapon(), m_selector.GetSelectedVehicleWeapon(), bForceIntoHand, bProcessWeaponInstructions); 

	return false; 
}

bool CPedWeaponManager::EquipBestNonLethalWeapon(bool bForceIntoHand, bool bProcessWeaponInstructions)
{ 
	if(m_selector.SelectWeapon(m_pPed, SET_BEST_NONLETHAL))
	{
		if(m_selector.GetSelectedWeaponInfo() && !GetSelectedWeaponInfo()->GetIsUnarmed())
		{
			return EquipWeapon(m_selector.GetSelectedWeapon(), m_selector.GetSelectedVehicleWeapon(), bForceIntoHand, bProcessWeaponInstructions);
		}
	}

	return false; 
}

const CWeapon* CPedWeaponManager::GetEquippedWeapon() const
{
	if(GetEquippedWeaponObject())
	{
		return GetEquippedWeaponObject()->GetWeapon();
	}
	else
	{
		const CWeaponInfo* pWeaponInfo = m_equippedWeapon.GetWeaponInfo();
		if(m_equippedWeapon.GetWeaponNoModel() && m_equippedWeapon.GetWeaponNoModel()->GetWeaponInfo() == pWeaponInfo)
		{
			return m_equippedWeapon.GetWeaponNoModel();
		}
		else
		{
#if FPS_MODE_SUPPORTED
			if(m_pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true) && (pWeaponInfo && !pWeaponInfo->GetHasFirstPersonScope()))
			{
				if(GetUnarmedEquippedWeapon())
					return GetUnarmedEquippedWeapon();
				else 
					return CWeaponManager::GetWeaponUnarmed();
			}
			else
#endif // FPS_MODE_SUPPORTED
			{
				if(pWeaponInfo)
				{
					if(pWeaponInfo->GetIsUnarmed())
					{
						if (pWeaponInfo->GetIsMeleeKungFu() && GetUnarmedKungFuEquippedWeapon())
							return GetUnarmedKungFuEquippedWeapon();
						else if(GetUnarmedEquippedWeapon())
							return GetUnarmedEquippedWeapon();
						else 
							return CWeaponManager::GetWeaponUnarmed();
					}
				}
			}
		}
	}

#if FPS_MODE_SUPPORTED
	if(m_pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true))
	{
		if(m_pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_KeepWeaponHolsteredUnlessFired))
		{
			// Treat as unarmed
			return GetUnarmedEquippedWeapon();
		}
	}
#endif // FPS_MODE_SUPPORTED

	return NULL;
}

u32 CPedWeaponManager::GetBestWeaponHash(u32 uExcludedHash, bool bIgnoreAmmoCheck /*= false*/) const
{
	return m_selector.GetBestPedWeapon( m_pPed, CWeaponInfoManager::GetSlotListBest(), false, uExcludedHash, bIgnoreAmmoCheck );
}

u32 CPedWeaponManager::GetBestWeaponSlotHash(u32 uExcludedHash, bool bIgnoreAmmoCheck) const
{
	return m_selector.GetBestPedWeaponSlot( m_pPed, CWeaponInfoManager::GetSlotListBest(), false, uExcludedHash, bIgnoreAmmoCheck );
}


const CWeaponInfo* CPedWeaponManager::GetBestWeaponInfo(bool bIgnoreVehicleCheck, bool bIgnoreWaterCheck/* = false*/, bool bIgnoreFullAmmo, bool bIgnoreProjectile) const
{
	u32 uWeaponHash = m_selector.GetBestPedWeapon( m_pPed, CWeaponInfoManager::GetSlotListBest(), bIgnoreVehicleCheck, 0, false, bIgnoreWaterCheck, bIgnoreFullAmmo, bIgnoreProjectile);
	if( uWeaponHash != 0 )
	{
		if (weaponVerifyf(m_pPed->GetInventory()->GetWeapon( uWeaponHash ), "CWeaponItem is NULL"))
		{
			return m_pPed->GetInventory()->GetWeapon( uWeaponHash )->GetInfo();
		}
	}

	return NULL;
}

const CWeaponInfo* CPedWeaponManager::GetBestWeaponInfoByGroup(u32 uGroupHash, bool bIgnoreAmmoCheck /*= false*/) const
{
	u32 uWeaponHash = m_selector.GetBestPedWeaponByGroup( m_pPed, CWeaponInfoManager::GetSlotListBest(), uGroupHash, bIgnoreAmmoCheck );
	if( uWeaponHash != 0 )
	{
		if (weaponVerifyf(m_pPed->GetInventory()->GetWeapon( uWeaponHash ), "CWeaponItem is NULL"))
		{
			return m_pPed->GetInventory()->GetWeapon( uWeaponHash )->GetInfo();
		}
	}

	return NULL;
}

CWeapon* CPedWeaponManager::GetEquippedWeapon()
{
	if(GetEquippedWeaponObject())
	{
		return GetEquippedWeaponObject()->GetWeapon();
	}
	else
	{
		const CWeaponInfo* pWeaponInfo = m_equippedWeapon.GetWeaponInfo();
		if(m_equippedWeapon.GetWeaponNoModel() && m_equippedWeapon.GetWeaponNoModel()->GetWeaponInfo() == pWeaponInfo)
		{
			return m_equippedWeapon.GetWeaponNoModel();
		}
		else
		{
#if FPS_MODE_SUPPORTED
			if(m_pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && (pWeaponInfo && !pWeaponInfo->GetHasFirstPersonScope()))
			{
				if(GetUnarmedEquippedWeapon())
					return GetUnarmedEquippedWeapon();
				else 
					return CWeaponManager::GetWeaponUnarmed();
			}
			else
#endif // FPS_MODE_SUPPORTED
			{
				if(pWeaponInfo)
				{
					if(pWeaponInfo->GetIsUnarmed())
					{
						if (pWeaponInfo->GetIsMeleeKungFu() && GetUnarmedKungFuEquippedWeapon())
							return GetUnarmedKungFuEquippedWeapon();
						else if(GetUnarmedEquippedWeapon())
							return GetUnarmedEquippedWeapon();
						else 
							return CWeaponManager::GetWeaponUnarmed();
					}
				}
			}
		}
	}

#if FPS_MODE_SUPPORTED
	if(m_pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		if(m_pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_KeepWeaponHolsteredUnlessFired))
		{
			// Treat as unarmed
			return GetUnarmedEquippedWeapon();
		}
	}
#endif // FPS_MODE_SUPPORTED

	return NULL;
}

const CVehicleWeapon* CPedWeaponManager::GetEquippedVehicleWeapon() const
{
	weaponFatalAssertf(m_pPed, "m_pPed is NULL");

	CVehicle* pVehicle = m_pPed->GetVehiclePedInside();
	if(GetEquippedVehicleWeaponIndex() > -1 && pVehicle && pVehicle->GetVehicleWeaponMgr())
	{
		s32 seatIndex = pVehicle->GetSeatManager()->GetPedsSeatIndex(m_pPed);

		if( pVehicle->GetVehicleWeaponMgr()->GetNumWeaponsForSeat(seatIndex) > GetEquippedVehicleWeaponIndex() )
		{
			atArray<const CVehicleWeapon*> weaponArray;
			pVehicle->GetVehicleWeaponMgr()->GetWeaponsForSeat(seatIndex,weaponArray);

			if (weaponVerifyf(weaponArray.size() > GetEquippedVehicleWeaponIndex(), "Invalid vehicle weapon index for weapon array"))
			{
				return weaponArray[GetEquippedVehicleWeaponIndex()];
			}
		}
	}

	return NULL;
}

CVehicleWeapon* CPedWeaponManager::GetEquippedVehicleWeapon()
{
	weaponFatalAssertf(m_pPed, "m_pPed is NULL");

	CVehicle* pVehicle = m_pPed->GetVehiclePedInside();
	if(GetEquippedVehicleWeaponIndex() > -1 && pVehicle && pVehicle->GetVehicleWeaponMgr())
	{
		s32 seatIndex = pVehicle->GetSeatManager()->GetPedsSeatIndex(m_pPed);

		if( pVehicle->GetVehicleWeaponMgr()->GetNumWeaponsForSeat(seatIndex) > GetEquippedVehicleWeaponIndex() )
		{
			atArray<CVehicleWeapon*> weaponArray;
			pVehicle->GetVehicleWeaponMgr()->GetWeaponsForSeat(seatIndex,weaponArray);

			if (weaponVerifyf(weaponArray.size() > GetEquippedVehicleWeaponIndex(), "Invalid vehicle weapon index for weapon array"))
			{
				return weaponArray[GetEquippedVehicleWeaponIndex()];
			}
		}
	}

	return NULL;
}

const CWeaponInfo* CPedWeaponManager::GetEquippedTurretWeaponInfo() const
{
	const CVehicleWeapon* pVehicleWeapon = GetEquippedVehicleWeapon();

	if (pVehicleWeapon)
	{
		const CWeaponInfo* pVehicleWeaponInfo = pVehicleWeapon->GetWeaponInfo();

		if (pVehicleWeaponInfo && pVehicleWeaponInfo->GetIsTurret())
		{
			return pVehicleWeaponInfo;
		}
	}

	return NULL;
}

bool CPedWeaponManager::GetShouldUnEquipWeapon() const
{
	if(m_equippedWeapon.GetObject())
	{
		const CWeapon* pWeapon = m_equippedWeapon.GetObject()->GetWeapon();
		if(pWeapon)
		{
			const CWeaponInfo* pWeaponInfo = pWeapon->GetWeaponInfo();
			if(!pWeaponInfo->GetIsCarriedInHand())
			{
				return true;
			}
		}
	}

	return false;
}

bool CPedWeaponManager::GetRequiresWeaponSwitch() const
{ 
	weaponFatalAssertf(m_pPed, "m_pPed is NULL");
	if( m_pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_KeepWeaponHolsteredUnlessFired ) )
	{
#if !__NO_OUTPUT
		if (NetworkInterface::IsGameInProgress() && m_pPed)
		{
			weaponitemDebugf2("CPedWeaponManager::GetRequiresWeaponSwitch CPED_CONFIG_FLAG_KeepWeaponHolsteredUnlessFired --> return false");
		}
#endif
		return false;
	}

	if (!CanAlterEquippedWeapon())
	{
#if !__NO_OUTPUT
		if ((Channel_weaponitem.FileLevel >= DIAG_SEVERITY_DEBUG2) || (Channel_weaponitem.TtyLevel >= DIAG_SEVERITY_DEBUG2))
		{
			if (NetworkInterface::IsGameInProgress() && m_pPed)
			{
				bool bCreateWeaponWhenLoaded = m_flags.IsFlagSet(Flag_CreateWeaponWhenLoaded);
				bool bCanAlterPedInventoryData = false;
				bool bGetTaskOverridingPropsWeapons = false;
				bool bGetTaskOverridingProps = false;
			
				if (m_pPed->GetNetworkObject())
				{
					CNetObjPed *netObjPed = static_cast<CNetObjPed *>(m_pPed->GetNetworkObject());
					if(netObjPed)
					{
						bCanAlterPedInventoryData = netObjPed->CanAlterPedInventoryData();
						bGetTaskOverridingPropsWeapons = netObjPed->GetTaskOverridingPropsWeapons();
						bGetTaskOverridingProps = netObjPed->GetTaskOverridingProps();
					}
				}

				weaponitemDebugf2("CPedWeaponManager::GetRequiresWeaponSwitch !CanAlterEquippedWeapon bCreateWeaponWhenLoaded[%d] bCanAlterPedInventoryData[%d] bGetTaskOverridingPropsWeapons[%d] bGetTaskOverridingProps[%d] --> return false",bCreateWeaponWhenLoaded,bCanAlterPedInventoryData,bGetTaskOverridingPropsWeapons,bGetTaskOverridingProps);
			}
		}
#endif
		return false;
	}

	if (m_pPed->GetPedIntelligence()->FindTaskActiveByTreeAndType(PED_TASK_TREE_SECONDARY, CTaskTypes::TASK_RELOAD_GUN))
	{
#if !__NO_OUTPUT
		if (NetworkInterface::IsGameInProgress() && m_pPed)
		{
			weaponitemDebugf2("CPedWeaponManager::GetRequiresWeaponSwitch FindTaskActiveByTreeAndType TASK_RELOAD_GUN --> return false");
		}
#endif
		return false;
	}

	return m_equippedWeapon.GetRequiresSwitch(); 
}

bool CPedWeaponManager::CreateEquippedWeaponObject(CPedEquippedWeapon::eAttachPoint attach)
{
	weaponFatalAssertf(m_pPed, "m_pPed is NULL");
	AI_LOG_WITH_ARGS("[CREATE_WEAPON] - %s ped %s called  CPedWeaponManager::CreateEquippedWeaponObject : %s (%i)\n", AILogging::GetDynamicEntityIsCloneStringSafe(m_pPed), AILogging::GetDynamicEntityNameSafe(m_pPed), m_equippedWeapon.GetWeaponInfo() ? m_equippedWeapon.GetWeaponInfo()->GetName() : "NULL", attach);

#if !__NO_OUTPUT
	if (NetworkInterface::IsGameInProgress() && m_pPed)
	{
		weaponitemDebugf2("CPedWeaponManager::CreateEquippedWeaponObject m_pPed[%p] isplayer[%d] player[%s] isnetworkclone[%d] ped[%s] weapon[%s] IsFlagSet(Flag_CreateWeaponWhenLoaded)[%d] call CreateEquippedWeaponObject attach[%d]",
			m_pPed,
			m_pPed->IsPlayer(),
			m_pPed->GetNetworkObject() && m_pPed->GetNetworkObject()->GetPlayerOwner() ? m_pPed->GetNetworkObject()->GetPlayerOwner()->GetLogName() : "",
			m_pPed->IsNetworkClone(),
			m_pPed->GetNetworkObject() ? m_pPed->GetNetworkObject()->GetLogName() : "", 
			m_pPed->GetWeaponManager() && m_pPed->GetWeaponManager()->GetEquippedWeaponInfo() ? m_pPed->GetWeaponManager()->GetEquippedWeaponInfo()->GetModelName() : "",
			m_flags.IsFlagSet(Flag_CreateWeaponWhenLoaded),
			attach);
	}
#endif

	if(m_equippedWeapon.GetWeaponHash() != 0)
	{
		if(GetRequiresWeaponSwitch() || GetEquippedWeaponAttachPoint() != attach || (m_pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsPerformingVehicleMelee) && m_equippedWeapon.GetObject() == NULL))
		{
			if( m_pPed->GetInventory()->GetIsStreamedIn(m_equippedWeapon.GetWeaponHash()) && m_equippedWeapon.CreateObject(attach) )
			{
				AI_LOG_WITH_ARGS("[CREATE_WEAPON] - %s ped %s created equipped weapon successfully %s (%i)\n", AILogging::GetDynamicEntityIsCloneStringSafe(m_pPed), AILogging::GetDynamicEntityNameSafe(m_pPed), m_equippedWeapon.GetWeaponInfo() ? m_equippedWeapon.GetWeaponInfo()->GetName() : "NULL", attach);

				m_flags.ClearFlag(Flag_CreateWeaponWhenLoaded);
				m_flags.ClearFlag(Flag_DelayedWeaponInLeftHand);

				// Clear flags then query weapon to see what processing to do
				m_flags.ClearFlag(Flag_SyncAmmo);

				CWeapon* pWeapon = GetEquippedWeapon();
				if(pWeapon)
				{
					// See if the weapon uses ammo
					if(pWeapon->GetWeaponInfo()->GetUsesAmmo())
					{
						m_flags.SetFlag(Flag_SyncAmmo);

						// B*2295321: Restore ammo clip info for weapon (if it was cached in CPedWeaponManager::EquipWeapon).
						if (m_pPed->IsLocalPlayer())
						{
							CWeaponItem* pNewWeaponItem = m_pPed->GetInventory() ? m_pPed->GetInventory()->GetWeapon(pWeapon->GetWeaponHash()) : NULL;
							if (pNewWeaponItem)
							{
								s32 iRestoredAmmoInClip = pNewWeaponItem->GetLastAmmoInClip();
								if (iRestoredAmmoInClip != -1)
								{
									pNewWeaponItem->SetLastAmmoInClip(-1, m_pPed);
									pWeapon->SetAmmoInClip(iRestoredAmmoInClip);
								}
							}
						}
					}
				}

				// Don't block bullets with loudhailer
				if(m_equippedWeapon.GetWeaponHash() == WEAPONTYPE_LOUDHAILER && m_equippedWeapon.GetObject())
				{
					m_equippedWeapon.GetObject()->SetCanShootThroughWhenEquipped(true);
				}
			}
			else
			{
#if !__NO_OUTPUT
				if (NetworkInterface::IsGameInProgress() && m_pPed)
				{
					weaponitemDebugf2("CPedWeaponManager::CreateEquippedWeaponObject !GetIsStreamedIn || !CreateObject -- m_pPed[%p] isplayer[%d] player[%s] isnetworkclone[%d] ped[%s] weapon[%s] --> return false",
						m_pPed,
						m_pPed->IsPlayer(),
						m_pPed->GetNetworkObject() && m_pPed->GetNetworkObject()->GetPlayerOwner() ? m_pPed->GetNetworkObject()->GetPlayerOwner()->GetLogName() : "",
						m_pPed->IsNetworkClone(),
						m_pPed->GetNetworkObject() ? m_pPed->GetNetworkObject()->GetLogName() : "", 
						m_equippedWeapon.GetWeaponInfo() ? m_equippedWeapon.GetWeaponInfo()->GetModelName() : "");
				}
#endif

				// Not streamed in yet, or otherwise failed to create the object, so try to create it again later
				m_flags.SetFlag(Flag_CreateWeaponWhenLoaded);
				
				if (attach == CPedEquippedWeapon::AP_LeftHand)
				{
					m_flags.SetFlag(Flag_DelayedWeaponInLeftHand);
				}
				
				return false;
			}
		}
		else
		{
#if !__NO_OUTPUT
			if (NetworkInterface::IsGameInProgress() && m_pPed)
			{
				weaponitemDebugf2("CPedWeaponManager::CreateEquippedWeaponObject !GetRequiresWeaponSwitch skip Create and Attach Weapon -- m_pPed[%p] isplayer[%d] player[%s] isnetworkclone[%d] ped[%s] weapon[%s] IsFlagSet(Flag_CreateWeaponWhenLoaded)[%d] CanAlterEquippedWeapon[%d]",
					m_pPed,
					m_pPed->IsPlayer(),
					m_pPed->GetNetworkObject() && m_pPed->GetNetworkObject()->GetPlayerOwner() ? m_pPed->GetNetworkObject()->GetPlayerOwner()->GetLogName() : "",
					m_pPed->IsNetworkClone(),
					m_pPed->GetNetworkObject() ? m_pPed->GetNetworkObject()->GetLogName() : "", 
					m_equippedWeapon.GetWeaponInfo() ? m_equippedWeapon.GetWeaponInfo()->GetModelName() : "",
					m_flags.IsFlagSet(Flag_CreateWeaponWhenLoaded),
					CanAlterEquippedWeapon());
			}
#endif

			if (NetworkInterface::IsGameInProgress())
			{
				//Initial creation request was made, but then !GetRequiresWeaponSwitch happens before the weapon can be created here - return false in this case - otherwise the subsequent calls to CreateEquippedWeaponObject will not happen and the weapon will not be created properly. lavalley.
				if (m_flags.IsFlagSet(Flag_CreateWeaponWhenLoaded) && !CanAlterEquippedWeapon() && m_pPed && m_pPed->IsNetworkClone())
					return false;
			}
		}
	}

	return true;
}

#if !__NO_OUTPUT
void weaponitemDefaultPrintFn(const char* fmt, ...)
{
	va_list args;
	va_start(args,fmt);
	diagLogDefault(RAGE_CAT2(Channel_,weaponitem), DIAG_SEVERITY_DISPLAY, __FILE__, __LINE__, fmt, args);
	va_end(args);
}

void weaponitemTraceDisplayLine(size_t addr, const char *sym, size_t offset)
{
	diagDisplayf(RAGE_CAT2(Channel_,weaponitem),RAGE_LOG_FMT("%8" SIZETFMT "x - %s+%" SIZETFMT "x"),addr,sym,offset);
}
#endif

// Destroy the equipped weapon object
void CPedWeaponManager::DestroyEquippedWeaponObject(bool bAllowDrop, const char* OUTPUT_ONLY(debugstr), bool bResetAttachPoint)
{
	AI_LOG_WITH_ARGS("[DESTROY_WEAPON] - %s ped %s called  CPedWeaponManager::DestroyEquippedWeaponObject : %s\n", AILogging::GetDynamicEntityIsCloneStringSafe(m_pPed), AILogging::GetDynamicEntityNameSafe(m_pPed), m_equippedWeapon.GetWeaponInfo() ? m_equippedWeapon.GetWeaponInfo()->GetName() : "NULL");

#if !__NO_OUTPUT
	if (NetworkInterface::IsGameInProgress() && m_pPed)
	{
		weaponitemDebugf2("CPedWeaponManager::DestroyEquippedWeaponObject m_pPed[%p] player[%s] weaponhash/model[%u][%s] bAllowDrop[%d] debugstr[%s]",
			m_pPed,
			m_pPed->GetNetworkObject() && m_pPed->GetNetworkObject()->GetPlayerOwner() ? m_pPed->GetNetworkObject()->GetPlayerOwner()->GetLogName() : "",
			m_equippedWeapon.GetWeaponHash(),
			m_equippedWeapon.GetWeaponInfo() ? m_equippedWeapon.GetWeaponInfo()->GetModelName() : "",			
			bAllowDrop,debugstr ? debugstr : "");

		if ((Channel_weaponitem.FileLevel >= DIAG_SEVERITY_DEBUG2) || (Channel_weaponitem.TtyLevel >= DIAG_SEVERITY_DEBUG2))
		{
			if (scrThread::GetActiveThread())
				scrThread::GetActiveThread()->PrintStackTrace(weaponitemDefaultPrintFn);

			sysStack::PrintStackTrace(weaponitemTraceDisplayLine);
		}
	}
#endif

	bool bShouldDrop = false;
	if(bAllowDrop)
	{
		if(m_equippedWeapon.GetObject())
		{
			CWeapon* pWeapon = m_equippedWeapon.GetObject()->GetWeapon();
			if( pWeapon)
			{
				if(pWeapon->GetWeaponInfo()->GetDiscardWhenSwapped())
				{
					bShouldDrop = true;
				}
				else if(pWeapon->GetWeaponInfo()->GetDiscardWhenOutOfAmmo() && m_pPed->GetInventory()->GetIsWeaponOutOfAmmo(pWeapon->GetWeaponInfo()))
				{
					bShouldDrop = true;
				}
			}
		}
	}

	if(bShouldDrop)
	{
		AI_LOG_WITH_ARGS("[DESTROY_WEAPON] - %s ped %s dropped equipped weapon successfully %s\n", AILogging::GetDynamicEntityIsCloneStringSafe(m_pPed), AILogging::GetDynamicEntityNameSafe(m_pPed), m_equippedWeapon.GetWeaponInfo() ? m_equippedWeapon.GetWeaponInfo()->GetName() : "NULL");
		DropWeapon(m_equippedWeapon.GetObject()->GetWeapon()->GetWeaponHash(), true);
	}
	else
	{
		// B*2315680: Cache ammo clip info when destroying the peds equipped weapon object.
		CWeapon *pWeaponBeingDestroyed = GetEquippedWeaponObject() ? GetEquippedWeaponObject()->GetWeapon() : NULL;
		if (m_pPed->IsLocalPlayer() && pWeaponBeingDestroyed && pWeaponBeingDestroyed->GetWeaponInfo() && pWeaponBeingDestroyed->GetWeaponInfo()->GetUsesAmmo() && (pWeaponBeingDestroyed->GetWeaponInfo()->GetClipSize() > 1 || pWeaponBeingDestroyed->GetWeaponInfo()->GetIsGun()))
		{
			CWeaponItem* pWeaponItem = m_pPed->GetInventory() ? m_pPed->GetInventory()->GetWeapon(pWeaponBeingDestroyed->GetWeaponHash()) : NULL;
			if (pWeaponItem)
			{
				s32 iAmmoInClip = GetEquippedWeapon()->GetAmmoInClip();
				pWeaponItem->SetLastAmmoInClip(iAmmoInClip, m_pPed);
			}
		}

		AI_LOG_WITH_ARGS("[DESTROY_WEAPON] - %s ped %s destroyed equipped weapon successfully %s\n", AILogging::GetDynamicEntityIsCloneStringSafe(m_pPed), AILogging::GetDynamicEntityNameSafe(m_pPed), m_equippedWeapon.GetWeaponInfo() ? m_equippedWeapon.GetWeaponInfo()->GetName() : "NULL");
		m_equippedWeapon.DestroyObject();
	
		CTaskNMControl* pTask = m_pPed ? static_cast<CTaskNMControl*>(m_pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_NM_CONTROL)) : NULL;
		if (pTask)
		{
			pTask->ClearFlag(CTaskNMControl::EQUIPPED_WEAPON_OBJECT_VISIBLE);
		}
		CTaskScriptedAnimation* pAnimTask = m_pPed ? static_cast<CTaskScriptedAnimation*>(m_pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_SCRIPTED_ANIMATION)) : NULL;
		if (pAnimTask)
		{
			pAnimTask->SetCreateWeaponAtEnd(false);
		}
		pAnimTask = static_cast<CTaskScriptedAnimation*>(m_pPed ? m_pPed->GetPedIntelligence()->GetTaskSecondaryPartialAnim() : NULL);
		if (pAnimTask && pAnimTask->GetTaskType()==CTaskTypes::TASK_SCRIPTED_ANIMATION)
		{
			pAnimTask->SetCreateWeaponAtEnd(false);
		}
	}

	if (bResetAttachPoint)
	{
		m_equippedWeapon.SetAttachPoint(CPedEquippedWeapon::AP_RightHand);
	}

	m_flags.ClearFlag(Flag_CreateWeaponWhenLoaded);
	m_flags.ClearFlag(Flag_DelayedWeaponInLeftHand);
}

void CPedWeaponManager::DestroyEquippedGadget(u32 uGadgetHash, bool bForceDeletion)
{
	for(s32 i = 0; i < m_equippedGadgets.GetCount(); i++)
	{
		if(m_equippedGadgets[i] && m_equippedGadgets[i]->GetWeaponInfo()->GetHash() == uGadgetHash)
		{
			if(bForceDeletion || m_equippedGadgets[i]->Deleted(m_pPed))
			{
				delete m_equippedGadgets[i];
				m_equippedGadgets.DeleteFast(i);
			}
		}
	}
}

void CPedWeaponManager::DestroyEquippedGadgets()
{
	for(s32 i = 0; i < m_equippedGadgets.GetCount(); i++)
	{
		if(m_equippedGadgets[i])
		{
			m_equippedGadgets[i]->Deleted(m_pPed);
			delete m_equippedGadgets[i];
		}
	}

	m_equippedGadgets.Reset();
}

CObject* CPedWeaponManager::DropWeapon(u32 uDroppedHash, bool bRemoveFromInventory, bool bTryCreatePickup, bool bIsDummyObjectForProjectileQuickThrow, bool bTriggerScriptEvent)
{
	Assertf(uDroppedHash != 0, "Trying to drop unarmed weapon or invalid weapon hash.");

#if !__NO_OUTPUT
	if (NetworkInterface::IsGameInProgress() && m_pPed)
	{
		weaponitemDebugf2("CPedWeaponManager::DropWeapon m_pPed[%p] player[%s] uDroppedHash[%u] bRemoveFromInventory[%d] bTryCreatePickup[%d]",
			m_pPed,
			m_pPed->GetNetworkObject() && m_pPed->GetNetworkObject()->GetPlayerOwner() ? m_pPed->GetNetworkObject()->GetPlayerOwner()->GetLogName() : "",
			uDroppedHash,bRemoveFromInventory,bTryCreatePickup);
	}
#endif
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(uDroppedHash);
	if( pWeaponInfo && pWeaponInfo->GetIsUnarmed() )
	{
		CObject* pObj = GetEquippedWeaponObject();
		if(!pObj || !pObj->m_nObjectFlags.bAmbientProp)
		{
			return NULL;
		}
	}
		
	CObject* pDroppedObject = NULL;

	if(m_equippedWeapon.GetObject())
	{
		if(m_equippedWeapon.GetObjectHash() == uDroppedHash)
		{
			const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(uDroppedHash);
			if(pWeaponInfo)
			{
				bool bHasPickup = CPickupManager::ShouldUseMPPickups(m_pPed) ? pWeaponInfo->GetMPPickupHash() != 0 : pWeaponInfo->GetPickupHash() != 0;

				// clones never create pickups
				if (m_pPed && m_pPed->IsNetworkClone())
				{
					bTryCreatePickup = false;
				}

				// Should we generate this as a pickup
				if(bTryCreatePickup && bHasPickup && !m_pPed->GetInventory()->GetIsWeaponOutOfAmmo(uDroppedHash))
				{
					pDroppedObject = CPickupManager::CreatePickUpFromCurrentWeapon(m_pPed, m_pPed->GetInventory()->GetNumWeaponAmmoReferences(uDroppedHash) == 1 && pWeaponInfo->GetDiscardWhenSwapped() );
					if(pDroppedObject)
					{
						DestroyEquippedWeaponObject(false);
					}
				}
				else
				{
					pDroppedObject = m_equippedWeapon.ReleaseObject();
					if(pDroppedObject)
					{
						if(m_RagdollConstraintHandle.IsValid())
						{
							PHCONSTRAINT->Remove(m_RagdollConstraintHandle);
						}

						pDroppedObject->DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS|DETACH_FLAG_APPLY_VELOCITY);

						// Hack: create a golf club pickup for golf club prop.
						CBaseModelInfo* pModelInfo = pDroppedObject->GetBaseModelInfo();
						if(pModelInfo)
						{
							static const atHashWithStringNotFinal golfClubPropHash = atHashWithStringNotFinal("Prop_Golf_Iron_01", 0x13EDBD11);
	
							if(pModelInfo->GetHashKey() == golfClubPropHash)
							{
								bHasPickup = true;

								if (bTryCreatePickup)
								{
									Matrix34 m;
									m.Identity();
									m = MAT34V_TO_MATRIX34(pDroppedObject->GetMatrix());

									CPickup* pPickup = CPickupManager::CreatePickup(PICKUP_WEAPON_GOLFCLUB, m);
									if(pPickup)
									{
										// delete the object
										pDroppedObject->FlagToDestroyWhenNextProcessed();
										pDroppedObject->SetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY, false, true);
										pDroppedObject = pPickup;
									}
								}
							}
						}

						// make sure game objects get deleted
						// don't delete frag instances or non game objects
						if (pDroppedObject->GetIsAnyManagedProcFlagSet())
						{
							// don't do anything with procedural objects - the procobj code needs to be in charge of them
						}
						else if(pDroppedObject->GetOwnedBy() == ENTITY_OWNEDBY_GAME && (!pDroppedObject->GetCurrentPhysicsInst() || pDroppedObject->GetCurrentPhysicsInst()->GetClassType() != PH_INST_FRAG_CACHE_OBJECT))
						{
							Assertf(pDroppedObject->GetRelatedDummy() == NULL, "Planning to remove an object (DropWeapon) but it has a dummy");

							pDroppedObject->SetOwnedBy( ENTITY_OWNEDBY_TEMP );

							// objects being dropped by a clone that turn into pickups need to be removed immediately (the pickup will be cloned) 
							// B*2083562: ...unless we're just dropping the weapon to be used as a dummy weapon object for projectile quick throws (passed in from CTaskAimAndThrowProjectile::CreateDummyWeaponObject).
							if (m_pPed && m_pPed->IsNetworkClone() && bHasPickup && !bIsDummyObjectForProjectileQuickThrow)
							{
								pDroppedObject->FlagToDestroyWhenNextProcessed();
							}
							// Random props get deleted after 20sec
							else if(!pDroppedObject->GetWeapon())
							{
								pDroppedObject->m_nEndOfLifeTimer = fwTimer::GetTimeInMilliseconds() + 10000;
							}
							// If this is a weapon marked as a prop, remove the prop after the specified time
							else if( pDroppedObject->m_nObjectFlags.bAmbientProp && pDroppedObject->GetWeapon() && pDroppedObject->GetWeapon()->GetWeaponInfo() && pDroppedObject->GetWeapon()->GetWeaponInfo()->GetHash() == OBJECTTYPE_OBJECT )
							{
								pDroppedObject->m_nEndOfLifeTimer = fwTimer::GetTimeInMilliseconds() - 30000 + pDroppedObject->GetWeapon()->GetPropEndOfLifeTimeoutMS();
							}
							// If a weapon is out of ammo, remove it after 5 seconds (this currently equates to -25000, as the object manager adds on 30000 to the m_nEndOfLifeTimer automatically)
							else if(pDroppedObject->GetWeapon() && pDroppedObject->GetWeapon()->GetAmmoTotal() == 0)
							{
								pDroppedObject->m_nEndOfLifeTimer = fwTimer::GetTimeInMilliseconds() - 25000;
							}
							// Other weapon object stick around for 2 minutes (pickups should grab them?)
							else
							{
								pDroppedObject->m_nEndOfLifeTimer = fwTimer::GetTimeInMilliseconds() + 120000;
							}
						}
					}
				}
			}
		}
	}

	if(bRemoveFromInventory)
	{
		m_pPed->GetInventory()->RemoveWeaponAndAmmo(uDroppedHash);
	}

	if(pDroppedObject)
	{
		// Make sure the object is not intersecting a wall, do a probe from the player to the object
		Vector3 vTestStart(VEC3V_TO_VECTOR3(m_pPed->GetTransform().GetPosition()));
		Vector3 vTestEnd(VEC3V_TO_VECTOR3(pDroppedObject->GetTransform().GetPosition()));
		static const s32 NUM_EXCLUDE_ENTITIES = 2;
		const CEntity* excludeEntities[NUM_EXCLUDE_ENTITIES] = { m_pPed, pDroppedObject };

		WorldProbe::CShapeTestFixedResults<1> probeResult;
		WorldProbe::CShapeTestProbeDesc probeDesc;
		probeDesc.SetStartAndEnd(vTestStart, vTestEnd);
		probeDesc.SetResultsStructure(&probeResult);
		probeDesc.SetExcludeEntities(excludeEntities, NUM_EXCLUDE_ENTITIES);
		probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_OBJECT_TYPE);
		probeDesc.SetContext(WorldProbe::LOS_GeneralAI);
		if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
		{
			Vector3 vPos(VEC3V_TO_VECTOR3(probeResult[0].GetPosition()));
			vPos += VEC3V_TO_VECTOR3(probeResult[0].GetNormal()) * pDroppedObject->GetBoundRadius();
			pDroppedObject->SetPosition(vPos);
		}

		// Inform NM that the weapon has been dropped, if applicable
		UnregisterNMWeapon();

		//When the weapon is cleared here - also ensure the weapon hash is cleared
		m_equippedWeapon.SetWeaponHash(0);

		// Makes sure this dropped object doesn't have the motion blur flag set
		pDroppedObject->m_nFlags.bAddtoMotionBlurMask = false;

		if (bTriggerScriptEvent && m_pPed)
		{
			pDroppedObject->SetOwnedBy(ENTITY_OWNEDBY_SCRIPT);
			CEventNetworkPedDroppedWeapon droppedWeaponEvent(CTheScripts::GetGUIDFromEntity(*pDroppedObject), *m_pPed);
			GetEventScriptNetworkGroup()->Add(droppedWeaponEvent);
		}
	}

	return pDroppedObject;
}

void CPedWeaponManager::BackupEquippedWeapon(bool allowClones)
{
	if((!m_pPed->IsNetworkClone() || allowClones) && m_equippedWeapon.GetWeaponHash() != 0)
	{
		SetBackupWeapon(m_equippedWeapon.GetWeaponHash());
	}
}

void CPedWeaponManager::RestoreBackupWeapon()
{
	if(m_uEquippedWeaponHashBackup != 0)
	{
		if(m_pPed->GetInventory()->GetWeapon(m_uEquippedWeaponHashBackup))
		{
			// set up the inputs for EquipWeapon
			u32 uWeaponNameHash				= m_uEquippedWeaponHashBackup;	// weapon we want equipped
			s32 iVehicleIndex				= -1;							// default, don't equip to vehicle
			bool bCreateWeaponWhenLoaded	= false;						// default, don't create the backup'ed weapon
			bool bProcessWeaponInstructions	= false;						// default, don't process extra weapon instructions
			
			// attempt to equip the weapon backup
			EquipWeapon(uWeaponNameHash, iVehicleIndex, bCreateWeaponWhenLoaded, bProcessWeaponInstructions);

			// Restore the back up weapon in TaskMobilePhone as well.
			CTask* pTask = m_pPed->GetPedIntelligence()->FindTaskActiveByTreeAndType(PED_TASK_TREE_SECONDARY, CTaskTypes::TASK_MOBILE_PHONE);
			if(pTask)
			{
				CTaskMobilePhone* pMobilePhoneTask = (CTaskMobilePhone*)pTask;
				pMobilePhoneTask->SetPreviousSelectedWeapon(uWeaponNameHash);
			}
		}
		else
		{
			EquipBestWeapon();
		}

		m_uEquippedWeaponHashBackup = 0;
	}
}

void CPedWeaponManager::SetBackupWeapon(u32 uBackupWeapon)
{
	if(weaponVerifyf(m_pPed->GetInventory()->GetWeapon(uBackupWeapon), "Weapon [%d] does not exist in inventory", uBackupWeapon))
	{
		m_uEquippedWeaponHashBackup = uBackupWeapon;
	}
	else
	{
		m_uEquippedWeaponHashBackup = 0;
	}
}

void CPedWeaponManager::ClearBackupWeapon()
{
	m_uEquippedWeaponHashBackup = 0;
}

bool CPedWeaponManager::GetIsArmedMelee() const
{
	const CItemInfo* pInfo = m_equippedWeapon.GetWeaponInfo();
	if(pInfo && pInfo->GetIsClassId(CWeaponInfo::GetStaticClassId()))
	{
		const CWeaponInfo* pWeaponInfo = static_cast<const CWeaponInfo*>(pInfo);
		return pWeaponInfo->GetIsMelee();
	}

	return false;
}

u32 CPedWeaponManager::GetWeaponGroup() const
{
	const CItemInfo* pInfo = m_equippedWeapon.GetWeaponInfo();
	if(pInfo && pInfo->GetIsClassId(CWeaponInfo::GetStaticClassId()))
	{
		const CWeaponInfo* pWeaponInfo = static_cast<const CWeaponInfo*>(pInfo);
		return pWeaponInfo->GetGroup();
	}

	return 0;
}

bool CPedWeaponManager::GetIsArmedGun() const
{
	const CItemInfo* pInfo = m_equippedWeapon.GetWeaponInfo();
	if(pInfo && pInfo->GetIsClassId(CWeaponInfo::GetStaticClassId()))
	{
		const CWeaponInfo* pWeaponInfo = static_cast<const CWeaponInfo*>(pInfo);
		return pWeaponInfo->GetIsGun();
	}

	return false;
}

bool CPedWeaponManager::GetIsArmedGunOrCanBeFiredLikeGun() const
{
	const CItemInfo* pInfo = m_equippedWeapon.GetWeaponInfo();
	if(pInfo && pInfo->GetIsClassId(CWeaponInfo::GetStaticClassId()))
	{
		const CWeaponInfo* pWeaponInfo = static_cast<const CWeaponInfo*>(pInfo);
		return pWeaponInfo->GetIsGunOrCanBeFiredLikeGun();
	}

	return false;
}

bool CPedWeaponManager::GetIsArmed1Handed() const
{
	const CItemInfo* pInfo = m_equippedWeapon.GetWeaponInfo();
	if(pInfo && pInfo->GetIsClassId(CWeaponInfo::GetStaticClassId()))
	{
		const CWeaponInfo* pWeaponInfo = static_cast<const CWeaponInfo*>(pInfo);
		return pWeaponInfo->GetIsGun1Handed();
	}

	return false;
}

bool CPedWeaponManager::GetIsArmed2Handed() const
{
	const CItemInfo* pInfo = m_equippedWeapon.GetWeaponInfo();
	if(pInfo && pInfo->GetIsClassId(CWeaponInfo::GetStaticClassId()))
	{
		const CWeaponInfo* pWeaponInfo = static_cast<const CWeaponInfo*>(pInfo);
		return pWeaponInfo->GetIsGun2Handed();
	}

	return false;
}

bool CPedWeaponManager::GetIsArmedHeavy() const
{
	const CItemInfo* pInfo = m_equippedWeapon.GetWeaponInfo();
	if(pInfo && pInfo->GetIsClassId(CWeaponInfo::GetStaticClassId()))
	{
		const CWeaponInfo* pWeaponInfo = static_cast<const CWeaponInfo*>(pInfo);
		return pWeaponInfo->GetIsHeavy();
	}

	return false;
}

bool CPedWeaponManager::GetIsGadgetEquipped(u32 uGadgetHash) const
{
	for(s32 i = 0; i < m_equippedGadgets.GetCount(); i++)
	{
		if(m_equippedGadgets[i] && (m_equippedGadgets[i]->GetWeaponHash() == uGadgetHash))
		{
			return true;
		}
	}

	return false;
}

void CPedWeaponManager::SwitchToRagdoll()
{
	weaponFatalAssertf(m_pPed, "m_pPed is NULL");

	m_pPed->SetNMTwoHandedWeaponBothHandsConstrained(false);

	// If we haven't called PreRender then ped is off screen, it's not worth using physical gun
	if(GetEquippedWeaponObject() && 
		GetEquippedWeaponObject()->GetIsAttached() &&
		GetEquippedWeaponObject()->GetCurrentPhysicsInst() && GetEquippedWeaponObject()->GetCurrentPhysicsInst()->IsInLevel() && 
		GetEquippedWeaponObject()->GetWeapon())
	{
#if LAZY_RAGDOLL_BOUNDS_UPDATE
		Assert(m_pPed->AreRagdollBoundsUpToDate());
#endif

		// B*2306914: Re-attach musket to original bone if reloading before proceeding as this happens before we get a chance to do it in CTaskReloadGun::CleanUp.
		if (m_pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_RELOAD_GUN) && GetEquippedWeaponInfo() && GetEquippedWeaponInfo()->GetHash() == ATSTRINGHASH("WEAPON_MUSKET", 0xa89cb99e))
		{
			CTaskReloadGun* pReloadTask = static_cast<CTaskReloadGun*>(m_pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_RELOAD_GUN));
			u8 uOriginalAttachPoint = pReloadTask->GetOriginalAttachPoint();
			pReloadTask->SwitchGunHands(uOriginalAttachPoint);
		}

		// Use the attachment extension from animation (before detaching) to get the intended gun-hand offset 
		fwAttachmentEntityExtension *extensionOld = GetEquippedWeaponObject()->GetAttachmentExtension();
		Assert(extensionOld);
		Matrix34 mOffsetBone(Matrix34::IdentityType);
		if (extensionOld)
		{
			mOffsetBone.FromQuaternion(extensionOld->GetAttachQuat());
			mOffsetBone.d = extensionOld->GetAttachOffset();
		}
		
		// First get attachment bones so that we preserve information about left / right hand
		s16 iPrimaryPedAttachBone = GetEquippedWeaponObject()->GetAttachBone();
		if(iPrimaryPedAttachBone == -1)
		{
			iPrimaryPedAttachBone = 0;
		}

		// get the matrix of the primary attach bone ragdoll component
		int componentA = m_pPed->GetRagdollInst()->GetCacheEntry()->GetBoneIndexToComponentMap()[iPrimaryPedAttachBone];
		weaponAssert(componentA >= 0 && componentA < m_pPed->GetRagdollInst()->GetTypePhysics()->GetNumChildren());

		phBoundComposite *bound = m_pPed->GetRagdollInst()->GetCacheEntry()->GetBound();
		Matrix34 matWorldRHandBound;
		matWorldRHandBound.Dot(RCC_MATRIX34(bound->GetCurrentMatrix(componentA)), RCC_MATRIX34(m_pPed->GetRagdollInst()->GetMatrix())); 
	
		Vec3V vecGunToHand = GetEquippedWeaponObject()->GetMatrix().d() - RC_VEC3V(matWorldRHandBound.d);

		// Clear the basic attachment
		GetEquippedWeaponObject()->DetachFromParent(0);

		// get the weapon attach bone
		s32 iPrimaryWeaponAttachBone = 0;
		const CWeaponModelInfo* pWeaponModelInfo = GetEquippedWeaponObject()->GetWeapon()->GetWeaponModelInfo();
		if(pWeaponModelInfo)
		{
			iPrimaryWeaponAttachBone = pWeaponModelInfo->GetBoneIndex(WEAPON_GRIP_R);
			if(iPrimaryWeaponAttachBone == -1)
			{
				iPrimaryWeaponAttachBone = 0;
			}
		}

		static const float s_MaxGunToHandDistance = 0.5f;

#if __ASSERT
		if (Mag(vecGunToHand).Getf() >= s_MaxGunToHandDistance)
		{
			Printf("\n****************** Begin debug spew for CPedWeaponManager::SwitchToRagdoll() ******************\n");
			Printf("Animated Inst Matrix:");
			Printf("	%f %f %f %f", m_pPed->GetAnimatedInst()->GetMatrix().GetCol0().GetXf(), m_pPed->GetAnimatedInst()->GetMatrix().GetCol1().GetXf(), m_pPed->GetAnimatedInst()->GetMatrix().GetCol2().GetXf(), m_pPed->GetAnimatedInst()->GetMatrix().GetCol3().GetXf());
			Printf("	%f %f %f %f", m_pPed->GetAnimatedInst()->GetMatrix().GetCol0().GetYf(), m_pPed->GetAnimatedInst()->GetMatrix().GetCol1().GetYf(), m_pPed->GetAnimatedInst()->GetMatrix().GetCol2().GetYf(), m_pPed->GetAnimatedInst()->GetMatrix().GetCol3().GetYf());
			Printf("	%f %f %f %f", m_pPed->GetAnimatedInst()->GetMatrix().GetCol0().GetZf(), m_pPed->GetAnimatedInst()->GetMatrix().GetCol1().GetZf(), m_pPed->GetAnimatedInst()->GetMatrix().GetCol2().GetZf(), m_pPed->GetAnimatedInst()->GetMatrix().GetCol3().GetZf());
			Printf("Ragdoll Inst Matrix:");
			Printf("	%f %f %f %f", m_pPed->GetRagdollInst()->GetMatrix().GetCol0().GetXf(), m_pPed->GetRagdollInst()->GetMatrix().GetCol1().GetXf(), m_pPed->GetRagdollInst()->GetMatrix().GetCol2().GetXf(), m_pPed->GetRagdollInst()->GetMatrix().GetCol3().GetXf());
			Printf("	%f %f %f %f", m_pPed->GetRagdollInst()->GetMatrix().GetCol0().GetYf(), m_pPed->GetRagdollInst()->GetMatrix().GetCol1().GetYf(), m_pPed->GetRagdollInst()->GetMatrix().GetCol2().GetYf(), m_pPed->GetRagdollInst()->GetMatrix().GetCol3().GetYf());
			Printf("	%f %f %f %f", m_pPed->GetRagdollInst()->GetMatrix().GetCol0().GetZf(), m_pPed->GetRagdollInst()->GetMatrix().GetCol1().GetZf(), m_pPed->GetRagdollInst()->GetMatrix().GetCol2().GetZf(), m_pPed->GetRagdollInst()->GetMatrix().GetCol3().GetZf());
			Printf("Entity Transform:");
			Printf("	%f %f %f %f", m_pPed->GetTransform().GetMatrix().GetCol0().GetXf(), m_pPed->GetTransform().GetMatrix().GetCol1().GetXf(), m_pPed->GetTransform().GetMatrix().GetCol2().GetXf(), m_pPed->GetTransform().GetMatrix().GetCol3().GetXf());
			Printf("	%f %f %f %f", m_pPed->GetTransform().GetMatrix().GetCol0().GetYf(), m_pPed->GetTransform().GetMatrix().GetCol1().GetYf(), m_pPed->GetTransform().GetMatrix().GetCol2().GetYf(), m_pPed->GetTransform().GetMatrix().GetCol3().GetYf());
			Printf("	%f %f %f %f", m_pPed->GetTransform().GetMatrix().GetCol0().GetZf(), m_pPed->GetTransform().GetMatrix().GetCol1().GetZf(), m_pPed->GetTransform().GetMatrix().GetCol2().GetZf(), m_pPed->GetTransform().GetMatrix().GetCol3().GetZf());
			for (int ibone = 0; ibone < m_pPed->GetSkeleton()->GetBoneCount(); ibone++)
			{
				Vector3 vObjectOffset = m_pPed->GetObjectMtx(ibone).d - m_pPed->GetObjectMtx(0).d;
				Vector3 vLocalOffset = m_pPed->GetLocalMtx(ibone).d;
				Printf("Offset between root bone position and %s bone = %f,%f,%f (locals %f,%f,%f)\n", m_pPed->GetSkeleton()->GetSkeletonData().GetBoneData(ibone)->GetName(), vObjectOffset.x, vObjectOffset.y, vObjectOffset.z, vLocalOffset.x, vLocalOffset.y, vLocalOffset.z);
			}	
			Printf("\n****************** SpewRagdollTaskInfo ******************\n");
			m_pPed->SpewRagdollTaskInfo();

			//Matrix34 matWorldRHandBound2;
			//matWorldRHandBound2.Dot(RCC_MATRIX34(bound->GetCurrentMatrix(RAGDOLL_HAND_RIGHT)), RCC_MATRIX34(m_pPed->GetRagdollInst()->GetMatrix())); 
			//Printf("\n\n ragdoll LOD is %d", m_pPed->GetRagdollInst()->GetCurrentPhysicsLOD());
			//Printf("\n\n iPrimaryWeaponAttachBone is %d, iPrimaryPedAttachBone is %d, componentA is %d, RAGDOLL_HAND_RIGHT is %d", iPrimaryWeaponAttachBone, iPrimaryPedAttachBone, componentA, RAGDOLL_HAND_RIGHT);
			//Printf("\n\n matGunToHand.d is %f %f %f", matGunToHand.d.x, matGunToHand.d.y, matGunToHand.d.z);
			//Printf("\n\n mWeaponMatrix.d is %f %f %f", mWeaponMatrix.d.x, mWeaponMatrix.d.y, mWeaponMatrix.d.z);
			//Printf("\n\n boneMatrix.d is %f %f %f", boneMatrix.d.x, boneMatrix.d.y, boneMatrix.d.z);
			//Printf("\n\n mOffsetBone.d is %f %f %f", mOffsetBone.d.x, mOffsetBone.d.y, mOffsetBone.d.z);
			//Printf("\n\n matWorldRHandBound2.d is %f %f %f", matWorldRHandBound2.d.x, matWorldRHandBound2.d.y, matWorldRHandBound2.d.z);
			//Printf("\n\n Distance from hand bound to hand bone is %f", (boneMatrix.d-matWorldRHandBound2.d).Mag());
			//Printf("\n\n Distance from hand bone to gun is %f", (boneMatrix.d-mWeaponMatrix.d).Mag());
			//Printf("\n\n Distance from hand bound to gun is %f", (matWorldRHandBound2.d-mWeaponMatrix.d).Mag());

			const char* fileName = NULL;
#if !__SPU
			fileName = GetEquippedWeaponObject()->GetCurrentPhysicsInst()->GetArchetype() ? 
				GetEquippedWeaponObject()->GetCurrentPhysicsInst()->GetArchetype()->GetFilename() : NULL;
#endif // !__SPU
			Printf("\n weapon name is %s", fileName);

			weaponWarningf("Distance from gun to hand is %f when activating nm. Gun will be snapped to the bind pose position.", Mag(vecGunToHand).Getf());
		}
#endif  // __ASSERT

		// gun to hand matrix is outside of a reasonable range in the animation
		if (Mag(vecGunToHand).Getf() >= s_MaxGunToHandDistance)
		{
			// snap the gun back to the bind pose position
			Assert(m_pPed->GetSkeleton());
			s32 helperBoneIndex=-1;
			s32 handBoneIndex=-1;
			if (m_equippedWeapon.GetAttachPoint()==CPedEquippedWeapon::AP_RightHand)
			{
				m_pPed->GetSkeletonData().ConvertBoneIdToIndex(BONETAG_R_PH_HAND, helperBoneIndex);
				m_pPed->GetSkeletonData().ConvertBoneIdToIndex(BONETAG_R_HAND, handBoneIndex);
			}
			else
			{
				m_pPed->GetSkeletonData().ConvertBoneIdToIndex(BONETAG_L_PH_HAND, helperBoneIndex);
				m_pPed->GetSkeletonData().ConvertBoneIdToIndex(BONETAG_L_HAND, handBoneIndex);
			}

			if(helperBoneIndex>-1 && handBoneIndex>-1)
			{
				Mat34V_ConstRef helperBoneLocal = m_pPed->GetSkeleton()->GetSkeletonData().GetDefaultTransform(helperBoneIndex);
				m_pPed->GetSkeleton()->GetLocalMtx(helperBoneIndex).Set3x3(helperBoneLocal);
				m_pPed->GetSkeleton()->GetLocalMtx(helperBoneIndex).Setd(helperBoneLocal.d());
				m_pPed->GetSkeleton()->PartialUpdate(helperBoneIndex);
				mOffsetBone.DotFromLeft(RCC_MATRIX34(helperBoneLocal));
				Assign(iPrimaryPedAttachBone, handBoneIndex);
			}
		}

		// Make a new physical attachment //

		// first work out where to position the weapon objects physics inst
		Matrix34 mWeaponMatrix;

		// account for any offset between the ragdoll component and its controlling animated bone
		// (the ragdoll bounds may not have been updated yet this frame)
		s32 iComponentLinkBone = m_pPed->GetRagdollInst()->GetCacheEntry()->GetComponentToBoneIndexMap()[componentA];
		weaponAssert(iComponentLinkBone>-1 && iComponentLinkBone<m_pPed->GetSkeleton()->GetSkeletonData().GetNumBones());
		Matrix34 linkBoneMatrix;
		m_pPed->GetGlobalMtx(iComponentLinkBone, linkBoneMatrix);

		const fragPhysicsLOD* physicsLOD = m_pPed->GetRagdollInst()->GetTypePhysics();
		fragTypeChild** child = physicsLOD->GetAllChildren();
		weaponAssert(child);
		weaponAssert((child[componentA]));
		weaponAssert((child[componentA])->GetUndamagedEntity());
		linkBoneMatrix.DotFromLeft((child[componentA])->GetUndamagedEntity()->GetBoundMatrix()); // get the up to date ragdoll bound matrix for the hand

		Matrix34 boneMatrix;
		m_pPed->GetGlobalMtx(iPrimaryPedAttachBone, boneMatrix);

		mOffsetBone.Dot(boneMatrix);				// get the weapon offset in global space
		mOffsetBone.DotTranspose(linkBoneMatrix);	// untransform relative to where the ragdoll bound should be

		mWeaponMatrix.Dot(mOffsetBone, matWorldRHandBound); // position the weapon correctly relative to the current ragdoll bound matrix
		mWeaponMatrix.NormalizeSafe();
		GetEquippedWeaponObject()->SetMatrix(mWeaponMatrix, true, true, true);
#if GTA_REPLAY
		// We need to reset this flag as 'SetMatrix' above is saying 'Warp==true'...but it's not actually moving a large distance...
		// This results in the weapon not interpolating correctly when its dropped. (url:bugstar:2291143)
		CReplayMgr::CancelRecordingWarpedOnEntity(GetEquippedWeaponObject());
#endif

		// turn collision on and activate
		GetEquippedWeaponObject()->EnableCollision();
		GetEquippedWeaponObject()->ActivatePhysics();

		// make sure equipped weapons don't cause push collisions
		if (phCollider* collider = GetEquippedWeaponObject()->GetCollider())
		{
			collider->DisablePushCollisions();
		}

		weaponAssertf(m_pPed->GetUsingRagdoll(),"Ped needs to be switched to ragdoll at this point");

		// This block is needed so that the weapon isn't dropped upon reverting to animation
		fwAttachmentEntityExtension *extension = GetEquippedWeaponObject()->CreateAttachmentExtensionIfMissing();
		Assert(extension->GetAttachState()!=ATTACH_STATE_PHYSICAL || m_pPed == extension->GetAttachParent());
		// Need to prevent loops of attachments
		if(extension->GetAttachParentForced() != m_pPed && GetEquippedWeaponObject()->GetIsPhysicalAParentAttachment(m_pPed))
		{
			Assertf(false,"Attempting to attach a physical to its own child! Attachment will abort");
		}
		extension->AttachToEntity(GetEquippedWeaponObject(), m_pPed, iPrimaryPedAttachBone, ATTACH_FLAG_DO_PAIRED_COLL|ATTACH_STATE_PHYSICAL|ATTACH_FLAG_POS_CONSTRAINT|ATTACH_FLAG_ROT_CONSTRAINT, NULL, NULL);

#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{
			CReplayMgr::RecordEntityAttachment(GetEquippedWeaponObject()->GetReplayID(), m_pPed->GetReplayID());
		}
#endif // GTA_REPLAY
		weaponAssert(m_pPed->GetRagdollInst() == m_pPed->GetCurrentPhysicsInst());
		weaponAssert(m_pPed->GetRagdollInst()->GetCacheEntry() && m_pPed->GetRagdollInst()->GetCacheEntry()->GetBoneIndexToComponentMap());

		phConstraintFixed::Params gunConstraint;
		gunConstraint.instanceB = GetEquippedWeaponObject()->GetCurrentPhysicsInst();
		gunConstraint.instanceA = m_pPed->GetCurrentPhysicsInst();
		gunConstraint.componentB = 0;
		gunConstraint.componentA = (u16) componentA;
		m_RagdollConstraintHandle = PHCONSTRAINT->Insert(gunConstraint);

		// Get the gun to hand matrix for the aiming pose
		matWorldRHandBound.Inverse();
		Matrix34 matGunToHand;
		matGunToHand.Dot(mWeaponMatrix, matWorldRHandBound);

		// Register any held weapon for NM agents 
		if (m_pPed->GetRagdollInst()->GetNMAgentID() >= 0 && 
			m_pPed->GetRagdollInst()->GetCacheEntry() &&
			m_pPed->GetRagdollInst()->GetCacheEntry()->GetBound() && 
			m_RagdollConstraintHandle.IsValid() &&
			pWeaponModelInfo)
		{
			const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_pPed->GetWeaponHash(*m_pPed));
			if (pWeaponInfo)
			{
				int kWeaponType = 5; // pistol right - corresponds to enum in NmRsUtils.h
				if (pWeaponInfo->GetIsGun2Handed())
					kWeaponType = 2; // rifle - corresponds to enum in NmRsUtils.h
				m_pPed->GetRagdollInst()->SetWeaponMode(m_pPed->GetRagdollInst()->GetNMAgentID(), kWeaponType);

				// Get the muzzle position
				Vector3 vGunToMuzzle(Vector3::ZeroType);
				if(pWeaponModelInfo->GetBoneIndex(WEAPON_MUZZLE) >= 0)
					vGunToMuzzle = GetEquippedWeaponObject()->GetObjectMtx(pWeaponModelInfo->GetBoneIndex(WEAPON_MUZZLE)).d;

				// Get the butt position
				Vector3 vGunToButt(Vector3::ZeroType);
				if(pWeaponModelInfo->GetBoneIndex(WEAPON_NM_BUTT_MARKER) >= 0)
					vGunToButt = GetEquippedWeaponObject()->GetObjectMtx(pWeaponModelInfo->GetBoneIndex(WEAPON_NM_BUTT_MARKER)).d;

				ART::MessageParams msg;
				msg.addBool(NMSTR_PARAM(NM_START), true);
				msg.addInt(NMSTR_PARAM(NM_REGISTER_WEAPON_HAND), 1);
				msg.addInt(NMSTR_PARAM(NM_REGISTER_WEAPON_LEVEL_INDEX), GetEquippedWeaponObject()->GetCurrentPhysicsInst()->GetLevelIndex());
				//msg.addInt(NMSTR_PARAM(NM_REGISTER_WEAPON_CONSTRAINT_HANDLE), (int) &m_RagdollConstraintHandle);
				msg.addVector3(NMSTR_PARAM(NM_REGISTER_WEAPON_GUN_TO_HAND_A), matGunToHand.a.x, matGunToHand.a.y, matGunToHand.a.z);
				msg.addVector3(NMSTR_PARAM(NM_REGISTER_WEAPON_GUN_TO_HAND_B), matGunToHand.b.x, matGunToHand.b.y, matGunToHand.b.z);
				msg.addVector3(NMSTR_PARAM(NM_REGISTER_WEAPON_GUN_TO_HAND_C), matGunToHand.c.x, matGunToHand.c.y, matGunToHand.c.z);
				msg.addVector3(NMSTR_PARAM(NM_REGISTER_WEAPON_GUN_TO_HAND_D), matGunToHand.d.x, matGunToHand.d.y, matGunToHand.d.z);
				msg.addVector3(NMSTR_PARAM(NM_REGISTER_WEAPON_GUN_TO_MUZZLE), vGunToMuzzle.x, vGunToMuzzle.z, vGunToMuzzle.z);
				msg.addVector3(NMSTR_PARAM(NM_REGISTER_WEAPON_GUN_TO_BUTT), vGunToButt.x, vGunToButt.z, vGunToButt.z);
				m_pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_REGISTER_WEAPON_MSG), &msg);
			}
		}
	}
}

void CPedWeaponManager::SwitchToAnimated()
{
	if(GetEquippedWeaponObject() && GetEquippedWeaponObject()->GetIsAttached())
	{
		m_equippedWeapon.AttachObject(m_equippedWeapon.GetAttachPoint());

		if (phCollider* collider = GetEquippedWeaponObject()->GetCollider())
		{
			collider->EnablePushCollisions();
		}
	}
}

void CPedWeaponManager::ItemAdded(u32 uItemNameHash)
{
	if(CanAlterEquippedWeapon())
	{
		u32 uEquippedWeaponHash = GetEquippedWeaponHash();
		if(uEquippedWeaponHash != 0 && uEquippedWeaponHash != uItemNameHash)
		{
			if (uItemNameHash == 0 || CWeaponItem::GetIsHashValid(uItemNameHash))
			{
				// Check if our currently equipped weapon still exists (may have been removed)
				const CInventoryItem* pItem = m_pPed->GetInventory()->GetWeapon(GetEquippedWeaponHash());
				if (!pItem)
				{
					// Equip the new weapon
					if (m_pPed && !m_pPed->GetPedResetFlag( CPED_RESET_FLAG_TemporarilyBlockWeaponSwitching ))
					{
						m_selector.SelectWeapon(m_pPed, SET_SPECIFIC_ITEM, false, uItemNameHash);
						EquipWeapon(m_selector.GetSelectedWeapon(), -1, false);
					}
				}
			}
		}
	}
}

void CPedWeaponManager::ItemRemoved(u32 uItemNameHash)
{
	if(CanAlterEquippedWeapon())
	{
		if(uItemNameHash == GetEquippedWeaponHash())
		{
			if(m_equippedWeapon.GetObject() && m_equippedWeapon.GetObject()->GetWeapon())
			{
				m_equippedWeapon.GetObject()->GetWeapon()->GetAudioComponent().SpinDownWeapon();
			}
			DestroyEquippedWeaponObject(true OUTPUT_ONLY(,"CPedWeaponManager::ItemRemoved"));
			m_equippedWeapon.SetWeaponHash(0);

			// Inform NM that the weapon has been dropped
			if (m_pPed && m_pPed->GetRagdollInst()->GetNMAgentID() != -1 && m_pPed->GetUsingRagdoll())
			{
				m_pPed->GetRagdollInst()->SetWeaponMode(m_pPed->GetRagdollInst()->GetNMAgentID(), -1);

				ART::MessageParams msg;
				msg.addInt(NMSTR_PARAM(NM_SHOTCONFIGUREARMS_POINTGUN), false);
				m_pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_SHOTCONFIGUREARMS_MSG), &msg);
			}
			
			// Choose and equip a new weapon
			if (m_pPed && !m_pPed->GetPedResetFlag( CPED_RESET_FLAG_TemporarilyBlockWeaponSwitching ))
			{
				// Figure out what to equip based on if we dropped an object
				if(uItemNameHash != OBJECTTYPE_OBJECT)
				{
					m_selector.SelectWeapon(m_pPed, SET_BEST);
				}
				else
				{
					m_selector.SelectWeapon(m_pPed, SET_SPECIFIC_ITEM, false, m_pPed->GetDefaultUnarmedWeaponHash());
				}

				EquipWeapon(m_selector.GetSelectedWeapon(), -1, false);
			}
		}
		else if(uItemNameHash == m_selector.GetSelectedWeapon())
		{
			//Don't select a weapon if we are just dropping a prop.
			if(uItemNameHash != OBJECTTYPE_OBJECT)
			{
				// Selected weapon has been removed
				m_selector.SelectWeapon(m_pPed, SET_BEST);
			}
		}

		for(s32 i = 0; i < m_equippedGadgets.GetCount(); i++)
		{
			if(m_equippedGadgets[i] && m_equippedGadgets[i]->GetWeaponInfo()->GetHash() == uItemNameHash)
			{
				m_equippedGadgets[i]->Deleted(m_pPed);
				delete m_equippedGadgets[i];
				m_equippedGadgets.DeleteFast(i);
				break;
			}
		}
	}
}

void CPedWeaponManager::AllItemsRemoved()
{
	if(CanAlterEquippedWeapon())
	{
		DestroyEquippedWeaponObject(true OUTPUT_ONLY(,"CPedWeaponManager::AllItemsRemoved"));
		m_equippedWeapon.SetWeaponHash(0);

		DestroyEquippedGadgets();

		m_selector.ClearSelectedWeapons();
	}
}

void CPedWeaponManager::ProcessWeaponInstructions(CPed* UNUSED_PARAM(pPed))
{
	if (GetEquippedWeaponObject())
	{
		// Check to see if there is a weapon instruction that should be carried out before rendering
		switch(m_nInstruction)
		{
		case WI_AmbientTaskDropAndRevertToStored:
			{
				m_nInstruction = WI_AmbientTaskDropAndRevertToStored2ndAttempt;
			}
			break;

		case WI_AmbientTaskDropAndRevertToStored2ndAttempt:
			{
				weaponAssert(GetEquippedWeaponObject()->GetWeapon());
				weaponAssert(GetEquippedWeaponObject()->GetWeapon()->GetWeaponInfo());
				DropWeapon(GetEquippedWeaponObject()->GetWeapon()->GetWeaponInfo()->GetHash(), true);
				m_nInstruction = WI_None;
			}
			break;

		case WI_RevertToStoredWeapon:
			{
				weaponAssert(GetEquippedWeaponObject()->GetWeapon());
				weaponAssert(GetEquippedWeaponObject()->GetWeapon()->GetWeaponInfo());
				m_pPed->GetInventory()->RemoveWeapon(GetEquippedWeaponObject()->GetWeapon()->GetWeaponInfo()->GetHash());
				m_nInstruction = WI_None;
			}
			break;

		case WI_AmbientTaskDestroyWeapon:
			{
				// Note: I'm not entirely sure that this needs to be a two stage
				// thing like this, but I figured it's safest to follow the pattern
				// set by WI_AmbientTaskDropAndRevertToStored, presumably giving tasks
				// an extra frame to assume control over the prop. /FF

				m_nInstruction = WI_AmbientTaskDestroyWeapon2ndAttempt;
			}
			break;

		case WI_AmbientTaskDestroyWeapon2ndAttempt:
			{
				weaponAssert(GetEquippedWeaponObject()->GetWeapon());
				weaponAssert(GetEquippedWeaponObject()->GetWeapon()->GetWeaponInfo());
				m_pPed->GetInventory()->RemoveWeapon(GetEquippedWeaponObject()->GetWeapon()->GetWeaponInfo()->GetHash());
				m_nInstruction = WI_None;
			}
			break;

		default:
			break;
		}
	}
}

bool CPedWeaponManager::CanAlterEquippedWeapon() const
{
	bool canAlter = true;

	if(m_pPed) 
	{
		if (m_pPed->IsNetworkClone())
		{
			CNetObjPed *netObjPed = static_cast<CNetObjPed *>(m_pPed->GetNetworkObject());

			if(netObjPed)
			{
				canAlter = netObjPed->CanAlterPedInventoryData() || m_flags.IsFlagSet(Flag_CreateWeaponWhenLoaded);
				canAlter |= netObjPed->GetTaskOverridingPropsWeapons();
				canAlter |= netObjPed->GetTaskOverridingProps();
			}
		}
	}

	return canAlter;
}

void CPedWeaponManager::AttachEquippedWeapon(CPedEquippedWeapon::eAttachPoint attach)
{
	weaponFatalAssertf(m_pPed, "m_pPed is NULL");
	if (m_equippedWeapon.GetObject())
	{
		m_equippedWeapon.AttachObject(attach);
	}
}
