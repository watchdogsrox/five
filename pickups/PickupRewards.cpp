// File header
#include "Pickups/PickupRewards.h"

// game headers
#include "script/script_channel.h"
#include "script/script_hud.h"
#include "modelinfo/PedModelInfo.h"
#include "Network/Live/NetworkTelemetry.h"
#include "Peds/Ped.h"
#include "Pickups/Data/PickupDataManager.h"
#include "Pickups/Pickup.h"
#include "Pickups/PickupManager.h"
#include "Stats/StatsInterface.h"
#include "System/controlMgr.h"
#include "weapons/info/weaponinfo.h"

WEAPON_OPTIMISATIONS()
NETWORK_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CPickupRewardAmmo
//////////////////////////////////////////////////////////////////////////

bool CPickupRewardAmmo::CanGive(const CPickup* pPickup, const CPed* pPed) const
{
	static const float LOW_AMMO_THRESHOLD = 0.2f;

	if(pPickup && (pPickup->GetPickupData()->GetIgnoreAmmoCheck() || pPickup->IsFlagSet(CPickup::PF_PlayerGift)))
	{
		return true;
	}

	bool bWeaponPickup = false;

	if (pPickup)
	{
		// always allow collection if we don't have the weapon
		for (u32 i=0; i<pPickup->GetPickupData()->GetNumRewards(); i++)
		{
			const CPickupRewardData* pReward = pPickup->GetPickupData()->GetReward(i);

			if (pReward && pReward->GetType() == PICKUP_REWARD_TYPE_WEAPON)
			{
				bWeaponPickup = true;

				const CPickupRewardWeapon* pRewardWeapon = static_cast<const CPickupRewardWeapon*>(pReward);
				if (pRewardWeapon->CanGive(pPickup, pPed, false))
				{
					return true;
				}

				break;
			}
		}
	}

	if(pPed->GetInventory())
	{
		const CAmmoInfo* pAmmoInfo = CWeaponInfoManager::GetInfo<CAmmoInfo>(AmmoRef);
		if(pAmmoInfo)
		{
			s32 currAmmo = pPed->GetInventory()->GetAmmo(AmmoRef.GetHash());
			s32 maxAmmo = pAmmoInfo->GetAmmoMax(pPed);

			bool bFoundPending = false;

			// include pickups pending collection
			for (u32 i=0; i<CPickup::GetNumPickupsPendingCollection(); i++)
			{
				CPickup* pPendingPickup = CPickup::GetPickupPendingCollection(i);

				if (pPendingPickup && pPendingPickup != pPickup)
				{
					for (u32 r=0; r<pPendingPickup->GetPickupData()->GetNumRewards(); r++)
					{
						const CPickupRewardData* pReward = pPendingPickup->GetPickupData()->GetReward(r);

						if (pReward && pReward->GetHash() == GetHash())
						{
							const CPickupRewardAmmo* pAmmoReward = SafeCast(const CPickupRewardAmmo, pReward);
							currAmmo += pAmmoReward->Amount;
							bFoundPending = true;
						}
					}
				}
			}

			if (bFoundPending)
			{
				scriptDebugf3(">>> Adding ammo from pending collections - currAmmo now = %d <<<\n", currAmmo);
			}

			//! DMKH. url:bugstar:1644706. Always accept thrown ambient ammo (e.g. grenades).
			bool bThrownAmbientAmmo = (!pPickup || !pPickup->GetPlacement()) && pAmmoInfo->GetIsClassId(CAmmoThrownInfo::GetStaticClassId());

			// some modes only allow you to collect weapon pickups when low on ammo
			if (bWeaponPickup && CPickupManager::GetOnlyAllowAmmoCollectionWhenLowOnAmmo() && !bThrownAmbientAmmo)
			{
				s32 lowAmmoThreshold = (s32)((float)maxAmmo*LOW_AMMO_THRESHOLD);

				if (currAmmo < lowAmmoThreshold)
				{
					return true;
				}

				scriptDebugf3(">>> Can't give ammo - can only collect when below %d, curr = %d <<<\n", lowAmmoThreshold, currAmmo);
			}
			else if(currAmmo < maxAmmo)
			{
				return true;
			}
			else
			{
				scriptDebugf3(">>> Can't give ammo - max is %d, curr is >= max (%d) <<<\n", maxAmmo, currAmmo);
			}
		}
		else
		{
			scriptDebugf3(">>> Can't give ammo - no ammo info <<<\n");
		}
	}
	else
	{
		scriptDebugf3(">>> Can't give ammo - no ped inventory <<<\n");
	}

	return false;
}

void CPickupRewardAmmo::Give(const CPickup* pPickup, CPed* pPed) const
{
	s32 ammo = GetAmmoAmountRewarded(pPickup, pPed);

	// ammo can be shared among occupants in a vehicle 
	if (pPed->GetIsInVehicle() && 
		pPed->GetMyVehicle() && 
		CPickup::GetVehicleWeaponsSharedAmongstPassengers() &&
		(!pPickup || pPickup->GetPickupData()->GetShareWithPassengers()))
	{
		u32 numPlayers = pPed->GetMyVehicle()->GetSeatManager()->GetNumPlayers();
		u32 ammoShare = ammo / numPlayers;

		// give the driver any left overs
		if (pPed->GetMyVehicle()->GetDriver() == pPed)
		{
			ammo = ammoShare + (ammo - numPlayers*ammoShare);
		}
		else
		{
			ammo = ammoShare;
		}

		scriptDebugf3(">>> Ammo is shared amongst passengers: adjusted ammo = %d <<<\n", ammo);
	}

	if(pPed->GetInventory())
	{
		s32 ammoBefore = pPed->GetInventory()->GetAmmo(AmmoRef.GetHash());

		scriptDebugf3(">>> %d ammo added <<<\n", ammo);
		pPed->GetInventory()->AddAmmo(AmmoRef.GetHash(), ammo);

		s32 ammoAfter = pPed->GetInventory()->GetAmmo(AmmoRef.GetHash());

		s32 actualAmmoCollected = Max(0, ammoAfter - ammoBefore);

		if(pPickup)
		{
			const_cast<CPickup*>(pPickup)->SetAmountAndAmountCollected(ammo, actualAmmoCollected);
		}

		// Check if auto swap.
		// Find out which weapon group the ammo is and swap to the best weapon in the group if auto swap is allowed.
		if(NetworkInterface::IsGameInProgress())
		{
			bool bHasWeaponReward = false;
			u32 uWeaponHash = 0;
			if (pPickup && pPickup->m_pPickupData)
			{
				u32 numRewards = pPickup->m_pPickupData->GetNumRewards();

				for (u32 i=0; i<numRewards; i++)
				{
					const CPickupRewardData* pReward = pPickup->m_pPickupData->GetReward(i);

					if (pReward && pReward->GetType() == PICKUP_REWARD_TYPE_WEAPON)
					{
						bHasWeaponReward = true;
						uWeaponHash = static_cast<const CPickupRewardWeapon*>(pReward)->GetWeaponHash();
						break;
					}
				}
			}

			const CWeaponItemRepository& WeaponRepository = pPed->GetInventory()->GetWeaponRepository();
			for(int i = 0; i < WeaponRepository.GetItemCount(); i++)
			{
				const CWeaponItem* pWeaponItem = WeaponRepository.GetItemByIndex(i);
				if(pWeaponItem)
				{
					const CWeaponInfo* pWeaponInfo = pWeaponItem->GetInfo();
					if(pWeaponInfo->GetAmmoInfo(pPed) && pWeaponInfo->GetAmmoInfo(pPed)->GetHash() == AmmoRef.GetHash() && (!bHasWeaponReward || uWeaponHash == pWeaponInfo->GetHash()))
					{
						// Compare to the equipped weapon to determine if auto swap is allowed
						const CWeaponInfo* pEquippedWeaponInfo = pPed->GetWeaponManager()->GetEquippedWeaponInfo();
						if(pEquippedWeaponInfo && CPickupRewardWeapon::ShouldAutoSwap(pPed, pWeaponInfo, pEquippedWeaponInfo))
						{
							const CWeaponInfo* pBestWeaponInfo = 
								bHasWeaponReward ? 
								CWeaponInfoManager::GetInfo<CWeaponInfo>(uWeaponHash) : 
								pPed->GetWeaponManager()->GetBestWeaponInfoByGroup(pWeaponInfo->GetGroup());
							if(pBestWeaponInfo)
							{
								pPed->GetWeaponManager()->EquipWeapon(pBestWeaponInfo->GetHash());
							}
						}
						break;
					}
				}
			}
		}
	}
	else
	{
		scriptDebugf3(">>> ** Couldn't add ammo - player has no inventory! ** \n <<<");
	}

	if(pPed->IsAPlayerPed() && !NetworkInterface::IsGameInProgress() && !CScriptHud::bUsingMissionCreator)
	{
		StatsInterface::IncrementStat(StatsInterface::GetStatsModelHashId("NUM_AMMO_PICKUPS"), 1.0f);
	}
}

bool CPickupRewardAmmo::PreventCollectionIfCantGive(const CPickup*, const CPed*) const
{
	return CPickupManager::GetOnlyAllowAmmoCollectionWhenLowOnAmmo();
}

s32 CPickupRewardAmmo::GetAmmoAmountRewarded(const CPickup* pPickup, const CPed* OUTPUT_ONLY(pPed)) const
{
	s32 ammo = (pPickup && pPickup->GetAmount() > 0) ? pPickup->GetAmount() : Amount;
	ammo = (s32)(((float)ammo) * CPickupManager::GetPickupAmmoAmountScaler());

#if !__NO_OUTPUT
	const CAmmoInfo* pAmmoInfo = CWeaponInfoManager::GetInfo<CAmmoInfo>(AmmoRef);

	if(pAmmoInfo)
	{
		s32 ammoMax = pAmmoInfo->GetAmmoMax(pPed);
		s32 currAmmo = pPed->GetInventory()->GetAmmo(AmmoRef.GetHash());
		s32 givenAmmo = Min((ammoMax - currAmmo), ammo);

		scriptDebugf3(">>> %d ammo collected of type %u (max ammo : %d, curr ammo : %d). Ammo given = %d <<<\n", ammo, AmmoRef.GetHash(), ammoMax, currAmmo, givenAmmo);
	}
#endif // !__NO_OUTPUT

	return ammo;
}

//////////////////////////////////////////////////////////////////////////
// CPickupRewardBulletMP
//////////////////////////////////////////////////////////////////////////

bool CPickupRewardBulletMP::CanGive(const CPickup* pPickup, const CPed* pPed) const
{
	const CWeaponInfo* pWeaponInfo = GetRewardedWeaponInfo(pPed, true);
	if(pWeaponInfo && pWeaponInfo->GetIsGun() && !pWeaponInfo->GetIsProjectile() && pPed->GetInventory()->GetWeaponAmmo(pWeaponInfo) < pWeaponInfo->GetAmmoMax(pPed))
	{
		// Make sure the ammo amount match the number the ped actually picked up.
		// Note: need to do this before giving ammo reward.
		const CPickupRewardData* pAmmoReward = GetAmmoReward(pPed);
		if(pAmmoReward && !CPickupManager::IsSuppressionFlagSet(pAmmoReward->GetType()) && pAmmoReward->CanGive(pPickup, pPed))
		{
			s32 sAmmoAmountRewarded = ((CPickupRewardAmmo*)(pAmmoReward))->GetAmmoAmountRewarded(pPickup, pPed);
			const_cast<CPickup*>(pPickup)->SetAmount(sAmmoAmountRewarded);
			return true;
		}
	}

	return false;
}

void CPickupRewardBulletMP::Give(const CPickup* pPickup, CPed* pPed) const
{
	const CPickupRewardData* pReward = GetAmmoReward(pPed);
	if(pReward && !CPickupManager::IsSuppressionFlagSet(pReward->GetType()) && pReward->CanGive(pPickup, pPed))
	{
		pReward->Give(pPickup, pPed);
	}
}

const CPickupRewardData* CPickupRewardBulletMP::GetAmmoReward(const CPed* pPed) const
{
	const CPickupRewardData* pAmmoReward = NULL;

	const CPickupData* pPickupData = NULL;

	Assert(pPed->GetWeaponManager());

	const CWeaponInfo* pWeaponInfo = GetRewardedWeaponInfo(pPed, true);
	if(pWeaponInfo)
	{
		if(pWeaponInfo->GetIsGun() && !pWeaponInfo->GetDisplayRechargeTimeHUD() && pWeaponInfo->GetPickupHash() != 0)
		{
			pPickupData = CPickupDataManager::GetPickupData(pWeaponInfo->GetPickupHash());
		}
		else
		{
			pWeaponInfo = pPed->GetWeaponManager()->GetBestWeaponInfoByGroup(WEAPONGROUP_PISTOL, true);
			if(pWeaponInfo)
			{
				pPickupData = CPickupDataManager::GetPickupData(pWeaponInfo->GetPickupHash());
			}
		}
	}

	if(pPickupData)
	{
		u32 numRewards = pPickupData->GetNumRewards();
		for(u32 i = 0; i < numRewards; i++)
		{
			const CPickupRewardData* pReward = pPickupData->GetReward(i);
			if(AssertVerify(pReward) && 
				pReward->GetType() == PICKUP_REWARD_TYPE_AMMO )
			{
				pAmmoReward = pPickupData->GetReward(i);
				break;
			}
		}
	}

	return pAmmoReward;
}

const CWeaponInfo* CPickupRewardBulletMP::GetRewardedWeaponInfo(const CPed* pPed, bool bIgnoreProjectileInWater)
{
	if(pPed->GetWeaponManager() && pPed->GetInventory())
	{
		const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
		if(pWeapon)
		{
			const CWeaponInfo* pWeaponInfo = pWeapon->GetWeaponInfo();
			if(pWeaponInfo && pPed->GetIsSwimming())
			{
				if (pWeaponInfo->GetIsMelee() || pWeaponInfo->GetIsUnarmed())
				{
					const CWeaponInfo* pWeaponInfoLast = pPed->GetWeaponManager()->GetLastEquippedWeaponInfo();
					if (pWeaponInfoLast && 
						!pWeaponInfoLast->GetIsMelee() && 
						!pWeaponInfoLast->GetIsUnarmed() && 
						!(bIgnoreProjectileInWater && pWeaponInfoLast->GetIsProjectile()))
					{
						return pWeaponInfoLast;
					}
					else
					{
						return pPed->GetWeaponManager()->GetBestWeaponInfo(false, true, true, bIgnoreProjectileInWater);
					}
				}

				return pWeaponInfo;
			}
			else if(pWeaponInfo && !pWeaponInfo->GetIsUnarmed())
			{
				return pWeaponInfo;
			}
			else
			{
				return pPed->GetWeaponManager()->GetLastEquippedWeaponInfo();
			}
		}
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
// CPickupRewardMissileMP
//////////////////////////////////////////////////////////////////////////

bool CPickupRewardMissileMP::CanGive(const CPickup* pPickup, const CPed* pPed) const
{
	const CWeaponItem* pWeapon = pPed->GetInventory()->GetWeapon(WEAPONTYPE_RPG);
	if(pWeapon)
	{
		const CWeaponInfo* pWeaponInfo = pWeapon->GetInfo();
		if(pWeaponInfo && 
			pWeaponInfo->GetHash() == WEAPONTYPE_RPG && 
			pWeaponInfo->GetMPPickupHash() == PICKUP_AMMO_MISSILE_MP)
		{
			u32 currAmmo = pPed->GetInventory()->GetWeaponAmmo(pWeaponInfo);

			bool bFoundPending = false;

			// include pickups pending collection
			for (u32 i=0; i<CPickup::GetNumPickupsPendingCollection(); i++)
			{
				CPickup* pPendingPickup = CPickup::GetPickupPendingCollection(i);

				if (pPendingPickup && pPendingPickup != pPickup)
				{
					for (u32 r=0; r<pPendingPickup->GetPickupData()->GetNumRewards(); r++)
					{
						const CPickupRewardData* pReward = pPendingPickup->GetPickupData()->GetReward(r);

						if (pReward && pReward->GetHash() == GetHash())
						{
							currAmmo++;
							bFoundPending = true;
						}
					}
				}
			}

			if (bFoundPending)
			{
				scriptDebugf3(">>> Adding ammo from pending collections - currAmmo now = %d <<<\n", currAmmo);
			}

			if (currAmmo < pWeaponInfo->GetAmmoMax(pPed))
			{
				return true;
			}
		}
	}

	return false;
}

void CPickupRewardMissileMP::Give(const CPickup* pPickup, CPed* pPed) const
{
	const CPickupData* pPickupData = NULL;

	Assert(pPed->GetWeaponManager());
	
	const CWeaponItem* pWeapon = pPed->GetInventory()->GetWeapon(WEAPONTYPE_RPG);
	const CWeaponInfo* pWeaponInfo = (pWeapon ? pWeapon->GetInfo() : NULL);
	if(pWeaponInfo && pWeaponInfo->GetPickupHash() != 0)
	{
		pPickupData = CPickupDataManager::GetPickupData(pWeaponInfo->GetPickupHash());
	}

	if(pPickupData)
	{
		u32 numRewards = pPickupData->GetNumRewards();
		for(u32 i = 0; i < numRewards; i++)
		{
			const CPickupRewardData* pReward = pPickupData->GetReward(i);
			if(AssertVerify(pReward) && 
				pReward->GetType() == PICKUP_REWARD_TYPE_AMMO &&
				!CPickupManager::IsSuppressionFlagSet(pReward->GetType()) &&
				pReward->CanGive(pPickup, pPed))
			{
				pReward->Give(pPickup, pPed);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// CPickupRewardFireworkMP
//////////////////////////////////////////////////////////////////////////

bool CPickupRewardFireworkMP::CanGive(const CPickup* pPickup, const CPed* pPed) const
{
	const CWeaponItem* pWeapon = pPed->GetInventory()->GetWeapon(WEAPONTYPE_DLC_FIREWORK);
	if(pWeapon)
	{
		const CWeaponInfo* pWeaponInfo = pWeapon->GetInfo();
		if(pWeaponInfo && 
			pWeaponInfo->GetHash() == WEAPONTYPE_DLC_FIREWORK && 
			pWeaponInfo->GetMPPickupHash() == PICKUP_AMMO_FIREWORK_MP)
		{
			u32 currAmmo = pPed->GetInventory()->GetWeaponAmmo(pWeaponInfo);

			bool bFoundPending = false;

			// include pickups pending collection
			for (u32 i=0; i<CPickup::GetNumPickupsPendingCollection(); i++)
			{
				CPickup* pPendingPickup = CPickup::GetPickupPendingCollection(i);
					
				if (pPendingPickup && pPendingPickup != pPickup)
				{
					for (u32 r=0; r<pPendingPickup->GetPickupData()->GetNumRewards(); r++)
					{
						const CPickupRewardData* pReward = pPendingPickup->GetPickupData()->GetReward(r);

						if (pReward && pReward->GetHash() == GetHash())
						{
							currAmmo++;
							bFoundPending = true;
						}
					}
				}
			}

			if (bFoundPending)
			{
				scriptDebugf3(">>> Adding ammo from pending collections - currAmmo now = %d <<<\n", currAmmo);
			}

			if (currAmmo < pWeaponInfo->GetAmmoMax(pPed))
			{
				return true;
			}
		}
	}

	return false;
}

void CPickupRewardFireworkMP::Give(const CPickup* pPickup, CPed* pPed) const
{
	const CPickupData* pPickupData = NULL;

	Assert(pPed->GetWeaponManager());

	const CWeaponItem* pWeapon = pPed->GetInventory()->GetWeapon(WEAPONTYPE_DLC_FIREWORK);
	const CWeaponInfo* pWeaponInfo = (pWeapon ? pWeapon->GetInfo() : NULL);
	if(pWeaponInfo && pWeaponInfo->GetPickupHash() != 0)
	{
		pPickupData = CPickupDataManager::GetPickupData(pWeaponInfo->GetPickupHash());
	}

	if(pPickupData)
	{
		u32 numRewards = pPickupData->GetNumRewards();
		for(u32 i = 0; i < numRewards; i++)
		{
			const CPickupRewardData* pReward = pPickupData->GetReward(i);
			if(AssertVerify(pReward) && 
				pReward->GetType() == PICKUP_REWARD_TYPE_AMMO &&
				!CPickupManager::IsSuppressionFlagSet(pReward->GetType()) &&
				pReward->CanGive(pPickup, pPed))
			{
				pReward->Give(pPickup, pPed);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// CPickupRewardGrenadeLauncherMP
//////////////////////////////////////////////////////////////////////////

bool CPickupRewardGrenadeLauncherMP::CanGive(const CPickup* pPickup, const CPed* pPed) const
{
	const CWeaponItem* pWeapon = pPed->GetInventory()->GetWeapon(WEAPONTYPE_GRENADELAUNCHER);
	if(pWeapon)
	{
		const CWeaponInfo* pWeaponInfo = pWeapon->GetInfo();
		if(pWeaponInfo && 
			pWeaponInfo->GetHash() == WEAPONTYPE_GRENADELAUNCHER && 
			pWeaponInfo->GetMPPickupHash() == PICKUP_AMMO_GRENADELAUNCHER_MP)
		{
			u32 currAmmo = pPed->GetInventory()->GetWeaponAmmo(pWeaponInfo);

			bool bFoundPending = false;

			// include pickups pending collection
			for (u32 i=0; i<CPickup::GetNumPickupsPendingCollection(); i++)
			{
				CPickup* pPendingPickup = CPickup::GetPickupPendingCollection(i);

				if (pPendingPickup && pPendingPickup != pPickup)
				{
					for (u32 r=0; r<pPendingPickup->GetPickupData()->GetNumRewards(); r++)
					{
						const CPickupRewardData* pReward = pPendingPickup->GetPickupData()->GetReward(r);

						if (pReward && pReward->GetHash() == GetHash())
						{
							currAmmo++;
							bFoundPending = true;
						}
					}
				}
			}

			if (bFoundPending)
			{
				scriptDebugf3(">>> Adding ammo from pending collections - currAmmo now = %d <<<\n", currAmmo);
			}

			if (currAmmo < pWeaponInfo->GetAmmoMax(pPed))
			{
				return true;
			}
		}
	}

	return false;
}

void CPickupRewardGrenadeLauncherMP::Give(const CPickup* pPickup, CPed* pPed) const
{
	const CPickupData* pPickupData = NULL;

	Assert(pPed->GetWeaponManager());

	const CWeaponItem* pWeapon = pPed->GetInventory()->GetWeapon(WEAPONTYPE_GRENADELAUNCHER);
	const CWeaponInfo* pWeaponInfo = (pWeapon ? pWeapon->GetInfo() : NULL);
	if(pWeaponInfo && pWeaponInfo->GetPickupHash() != 0)
	{
		pPickupData = CPickupDataManager::GetPickupData(pWeaponInfo->GetPickupHash());
	}

	if(pPickupData)
	{
		u32 numRewards = pPickupData->GetNumRewards();
		for(u32 i = 0; i < numRewards; i++)
		{
			const CPickupRewardData* pReward = pPickupData->GetReward(i);
			if(AssertVerify(pReward) && 
				pReward->GetType() == PICKUP_REWARD_TYPE_AMMO &&
				!CPickupManager::IsSuppressionFlagSet(pReward->GetType()) &&
				pReward->CanGive(pPickup, pPed))
			{
				pReward->Give(pPickup, pPed);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// CPickupRewardArmour
//////////////////////////////////////////////////////////////////////////

bool CPickupRewardArmour::CanGive(const CPickup* pPickup, const CPed* pPed) const
{
	// include pickups pending collection
	for (u32 i=0; i<CPickup::GetNumPickupsPendingCollection(); i++)
	{
		CPickup* pPendingPickup = CPickup::GetPickupPendingCollection(i);

		if (pPendingPickup && pPendingPickup != pPickup)
		{
			for (u32 r=0; r<pPendingPickup->GetPickupData()->GetNumRewards(); r++)
			{
				const CPickupRewardData* pReward = pPendingPickup->GetPickupData()->GetReward(r);

				if (pReward && pReward->GetHash() == GetHash())
				{
#if ENABLE_NETWORK_LOGGING
					if (NetworkInterface::IsGameInProgress())
					{
						NetworkInterface::GetObjectManagerLog().Log("** CAN'T COLLECT %s : there is already an armour pickup pending collection (%s)", (pPickup && pPickup->GetNetworkObject()) ? pPickup->GetNetworkObject()->GetLogName() : "", pPendingPickup->GetNetworkObject() ? pPendingPickup->GetNetworkObject()->GetLogName() : "?");
					}
#endif					
					scriptDebugf3(">>> Can't give armour - already one in the pending collections <<<\n");
					return false;
				}
			}
		}
	}

	if (pPed->GetPlayerInfo())
	{
		return (pPed->GetArmour() < pPed->GetPlayerInfo()->MaxArmour);
	}

	return false;
}

void CPickupRewardArmour::Give(const CPickup* pPickup, CPed* pPed) const
{
	if (AssertVerify(pPed->GetPlayerInfo()))
	{
		float fOldArmour = pPed->GetArmour();
		float fNewArmour = fOldArmour;
		
		float fArmourToAdd;
		if (pPickup && pPickup->GetAmount() > 0)
		{
			fArmourToAdd = static_cast<float>(pPickup->GetAmount());
		}
		else
		{
			fArmourToAdd = static_cast<float>(Armour);
		}

		fNewArmour += fArmourToAdd;
		fNewArmour = rage::Min(fNewArmour, static_cast<float>(pPed->GetPlayerInfo()->MaxArmour));

		pPed->SetArmour(fNewArmour);

		if(pPickup)
		{
			float fArmourCollected = rage::Max(0.0f, fNewArmour - fOldArmour);
			const_cast<CPickup*>(pPickup)->SetAmountAndAmountCollected(static_cast<u32>(fArmourToAdd), static_cast<u32>(fArmourCollected));
		}

		if(pPed->IsAPlayerPed() && !NetworkInterface::IsGameInProgress())
		{
			StatsInterface::IncrementStat(StatsInterface::GetStatsModelHashId("NUM_ARMOUR_PICKUPS"), 1.0f);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// CPickupRewardHealth
//////////////////////////////////////////////////////////////////////////

bool CPickupRewardHealth::CanGive(const CPickup* pPickup, const CPed* pPed) const
{
	// include pickups pending collection
	for (u32 i=0; i<CPickup::GetNumPickupsPendingCollection(); i++)
	{
		CPickup* pPendingPickup = CPickup::GetPickupPendingCollection(i);

		if (pPendingPickup && pPendingPickup != pPickup)
		{
			for (u32 r=0; r<pPendingPickup->GetPickupData()->GetNumRewards(); r++)
			{
				const CPickupRewardData* pReward = pPendingPickup->GetPickupData()->GetReward(r);

				if (pReward && pReward->GetHash() == GetHash())
				{
#if ENABLE_NETWORK_LOGGING
					if (NetworkInterface::IsGameInProgress())
					{
						NetworkInterface::GetObjectManagerLog().Log("** CAN'T COLLECT %s : there is already a health pickup pending collection (%s)", (pPickup && pPickup->GetNetworkObject()) ? pPickup->GetNetworkObject()->GetLogName() : "", pPendingPickup->GetNetworkObject() ? pPendingPickup->GetNetworkObject()->GetLogName() : "?");
					}
#endif
					scriptDebugf3(">>> Can't give health - already one in the pending collections <<<\n");
					return false;
				}
			}
		}
	}

	return (pPed->GetHealth() < (pPed->GetMaxHealth()-0.2f));
}

void CPickupRewardHealth::Give(const CPickup* pPickup, CPed* pPed) const
{
	float fOldHealth = pPed->GetHealth();
	float fHealthToAdd = static_cast<float>(Health);
	float fNewHealth = fOldHealth + fHealthToAdd;

	fNewHealth = rage::Min(fNewHealth, pPed->GetMaxHealth());

	pPed->SetHealth(fNewHealth);

	if(pPickup)
	{
		float fHealthCollected = rage::Max(0.0f, fNewHealth - fOldHealth);
		const_cast<CPickup*>(pPickup)->SetAmountAndAmountCollected(static_cast<u32>(fHealthToAdd), static_cast<u32>(fHealthCollected));
	}

	// Give the player 25% special ability when you pickup a health pack. SP only for now.
	if(pPed->IsAPlayerPed() && !NetworkInterface::IsGameInProgress())
	{
		StatsInterface::IncrementStat(StatsInterface::GetStatsModelHashId("NUM_HEALTH_PICKUPS"), 1.0f);
		CPlayerSpecialAbilityManager::ChargeEvent(ACET_PICKUP, pPed);
	}
}

//////////////////////////////////////////////////////////////////////////
// CPickupRewardHealthVariable
//////////////////////////////////////////////////////////////////////////

bool CPickupRewardHealthVariable::CanGive(const CPickup* pPickup, const CPed* pPed) const
{
	// include pickups pending collection
	for (u32 i=0; i<CPickup::GetNumPickupsPendingCollection(); i++)
	{
		CPickup* pPendingPickup = CPickup::GetPickupPendingCollection(i);

		if (pPendingPickup && pPendingPickup != pPickup)
		{
			for (u32 r=0; r<pPendingPickup->GetPickupData()->GetNumRewards(); r++)
			{
				const CPickupRewardData* pReward = pPendingPickup->GetPickupData()->GetReward(r);

				if (pReward && pReward->GetHash() == GetHash())
				{
#if ENABLE_NETWORK_LOGGING
					if (NetworkInterface::IsGameInProgress())
					{
						NetworkInterface::GetObjectManagerLog().Log("** CAN'T COLLECT %s : there is already a health pickup pending collection (%s)", (pPickup && pPickup->GetNetworkObject()) ? pPickup->GetNetworkObject()->GetLogName() : "", pPendingPickup->GetNetworkObject() ? pPendingPickup->GetNetworkObject()->GetLogName() : "?");
					}
#endif	
					scriptDebugf3(">>> Can't give health - already one in the pending collections <<<\n");
					return false;
				}
			}
		}
	}

	return (pPed->GetHealth() < (pPed->GetMaxHealth()-0.2f));
}

void CPickupRewardHealthVariable::Give(const CPickup* pPickup, CPed* pPed) const
{
	float fOldHealth = pPed->GetHealth();
	float fHealthToAdd = static_cast<float>(pPickup->GetAmount());
	float fNewHealth = fOldHealth + fHealthToAdd;

	fNewHealth = rage::Min(fNewHealth, pPed->GetMaxHealth());

	pPed->SetHealth(fNewHealth);

	if(pPickup)
	{
		float fHealthCollected = rage::Max(0.0f, fNewHealth - fOldHealth);
		const_cast<CPickup*>(pPickup)->SetAmountAndAmountCollected(static_cast<u32>(fHealthToAdd), static_cast<u32>(fHealthCollected));
	}

	if(pPed->IsAPlayerPed() && !NetworkInterface::IsGameInProgress())
	{
		StatsInterface::IncrementStat(StatsInterface::GetStatsModelHashId("NUM_HEALTH_PICKUPS"), 1.0f);
		CPlayerSpecialAbilityManager::ChargeEvent(ACET_PICKUP, pPed);
	}
}

//////////////////////////////////////////////////////////////////////////
// CPickupRewardMoneyVariable
//////////////////////////////////////////////////////////////////////////

bool CPickupRewardMoneyVariable::CanGive(const CPickup* UNUSED_PARAM(pPickup), const CPed* pPed) const
{
	if (pPed->GetPlayerInfo() && !pPed->GetPlayerInfo()->GetPlayerDataCanCollectMoneyPickups())
	{
		return false;
	}

	return true;
}

void CPickupRewardMoneyVariable::Give(const CPickup* pPickup, CPed* pPed) const
{
	if (pPickup && pPickup->GetAmount() > 0)
	{
		if (pPed->IsLocalPlayer())
		{
			if (!NetworkInterface::IsGameInProgress())
			{
				StatId stat = StatsInterface::GetStatsModelHashId("MONEY_PICKED_UP_ON_STREET");
				if (StatsInterface::IsKeyValid(stat))
				{
					StatsInterface::IncrementStat(stat, (float) pPickup->GetAmount());
					StatsInterface::IncrementStat(STAT_TOTAL_CASH.GetStatId(), (float) pPickup->GetAmount());
				}
			}
		}
		else
		{
			pPed->SetMoneyCarried(pPed->GetMoneyCarried() + pPickup->GetAmount());
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// CPickupRewardMoneyFixed
//////////////////////////////////////////////////////////////////////////

bool CPickupRewardMoneyFixed::CanGive(const CPickup* UNUSED_PARAM(pPickup), const CPed* pPed) const
{
	if (pPed->GetPlayerInfo() && !pPed->GetPlayerInfo()->GetPlayerDataCanCollectMoneyPickups())
	{
		return false;
	}

	return true;
}

void CPickupRewardMoneyFixed::Give(const CPickup* UNUSED_PARAM(pPickup), CPed* pPed) const
{
	if (pPed->IsLocalPlayer())
	{
		// money collection in MP is handled by the scripts, as there are different cash stats depending on the mode
		if (!NetworkInterface::IsGameInProgress())
		{
			StatId stat = StatsInterface::GetStatsModelHashId("MONEY_PICKED_UP_ON_STREET");
			if (StatsInterface::IsKeyValid(stat))
			{
				StatsInterface::IncrementStat(stat, (float)Money);
				StatsInterface::IncrementStat(STAT_TOTAL_CASH.GetStatId(), (float)Money);
			}
		}
	}
	else
	{
		pPed->SetMoneyCarried(pPed->GetMoneyCarried() + Money);
	}
}

//////////////////////////////////////////////////////////////////////////
// CPickupRewardStat
//////////////////////////////////////////////////////////////////////////

bool CPickupRewardStat::CanGive(const CPickup* UNUSED_PARAM(pPickup), const CPed* UNUSED_PARAM(pPed)) const
{
	// return false here so that a pickup is only collected if other rewards return true
	return false;
}

void CPickupRewardStat::Give(const CPickup* UNUSED_PARAM(pPickup), CPed* UNUSED_PARAM(pPed)) const
{
	StatsInterface::IncrementStat(STAT_ID(Stat), Amount);
}

//////////////////////////////////////////////////////////////////////////
// CPickupRewardStatVariable
//////////////////////////////////////////////////////////////////////////

bool CPickupRewardStatVariable::CanGive(const CPickup* UNUSED_PARAM(pPickup), const CPed* UNUSED_PARAM(pPed)) const
{
	// return false here so that a pickup is only collected if other rewards return true
	return false;
}

void CPickupRewardStatVariable::Give(const CPickup* pPickup, CPed* UNUSED_PARAM(pPed)) const
{
	StatsInterface::IncrementStat(STAT_ID(Stat), (const float) pPickup->GetAmount());
}

//////////////////////////////////////////////////////////////////////////
// CPickupRewardWeapon
//////////////////////////////////////////////////////////////////////////

bool CPickupRewardWeapon::CanGive(const CPickup* pPickup, const CPed* pPed) const
{
	return CanGive(pPickup, pPed, true);
}

bool CPickupRewardWeapon::CanGive(const CPickup* pPickup, const CPed* pPed, bool bAutoSwap) const
{
	if(pPed->GetInventory())
	{
		// Don't give if we have a MK2 version of the weapon already in our inventory
		if (IsUpgradedVersionInInventory(pPed))
		{
			return false;
		}

		const CWeaponItem* pExistingWeapon = pPed->GetInventory()->GetWeapon(WeaponRef.GetHash());

		if(!pExistingWeapon || (bAutoSwap && AutoSwap(pPed)))
		{
			return true;
		}

		// include pickups pending collection
		for (u32 i=0; i<CPickup::GetNumPickupsPendingCollection(); i++)
		{
			CPickup* pPendingPickup = CPickup::GetPickupPendingCollection(i);

			if (pPendingPickup && pPendingPickup != pPickup)
			{
				for (u32 r=0; r<pPendingPickup->GetPickupData()->GetNumRewards(); r++)
				{
					const CPickupRewardData* pReward = pPendingPickup->GetPickupData()->GetReward(r);

					if (pReward && pReward->GetHash() == GetHash())
					{
						const CPickupRewardWeapon* pWeaponReward = SafeCast(const CPickupRewardWeapon, pReward);

						if (pWeaponReward->WeaponRef == WeaponRef)
						{
							scriptDebugf3(">>> Can't give weapon - already one in the pending collections <<<\n");
							return false;
						}
					}
				}
			}
		}

		// allow the weapon to be collected if it has some components we don't have
		if (NetworkInterface::IsGameInProgress() && pPickup && pPickup->GetWeapon())
		{
			const CWeapon::Components& pickupComponents = pPickup->GetWeapon()->GetComponents();
			const CWeaponItem::Components* existingComponents = pExistingWeapon->GetComponents();

			if (pickupComponents.GetCount() > 0)
			{
				for (u32 i=0; i<pickupComponents.GetCount(); i++)
				{
					bool bHasComponent = false;

					if (existingComponents)
					{
						for (u32 j=0; j<existingComponents->GetCount(); j++)
						{
							const CWeaponComponentInfo* pCompInfo = (*existingComponents)[j].pComponentInfo;

							if (pCompInfo)
							{
								if (pickupComponents[i]->GetInfo()->GetHash() == pCompInfo->GetHash())
								{
									bHasComponent = true;
									break;
								}
								else if (pickupComponents[i]->GetInfo()->GetClassId() == pCompInfo->GetClassId()) 
								{
									// handle the case where we have 2 components of the same class but of a different type and one is more
									// preferable to another (eg an extended clip is preferable to the default clip). Here we use the location of
									// the component in the attachment point array to determine this - the later the component falls in the array,
									// the more preferable it is
									const CWeaponComponentPoint* pAttachPoint = pExistingWeapon->GetInfo()->GetAttachPoint(pCompInfo);
									const CWeaponComponentPoint::Components& components = pAttachPoint->GetComponents();

									u32 pickupComponentNum = 0;
									u32 existingComponentNum = 0;

									for (u32 k=0; k<components.GetCount(); k++)
									{
										if (components[k].m_Name.GetHash() == pickupComponents[i]->GetInfo()->GetHash())
										{
											pickupComponentNum = k;
										}
										else if (components[k].m_Name.GetHash() == pCompInfo->GetHash())
										{
											existingComponentNum = k;
										}
									}

									if (pickupComponentNum <= existingComponentNum)
									{
										bHasComponent = true;
										break;
									}
								}
							}
						}
					}

					if (!bHasComponent)
					{
						return true;
					}
				}
			}
		}
	}

	return false;
}

void CPickupRewardWeapon::Give(const CPickup* pPickup, CPed* pPed) const
{
	if(pPed->GetInventory())
	{
		CWeaponItem* pExistingWeapon = pPed->GetInventory()->GetWeapon(WeaponRef.GetHash());
		bool bAllowAutoSwap = NetworkInterface::IsGameInProgress() && ((pPickup && pPickup->IsFlagSet(CPickup::PF_AutoEquipOnCollect)) || AutoSwap(pPed));

		if (!pExistingWeapon)
		{
			pExistingWeapon = pPed->GetInventory()->AddWeapon(WeaponRef.GetHash());

			if (pExistingWeapon && pPed->GetWeaponManager())
			{
				if(Equip || bAllowAutoSwap)
				{
					pPed->GetWeaponManager()->EquipWeapon(WeaponRef.GetHash());
				}
			}
		}
		else if(bAllowAutoSwap && pPed->GetWeaponManager() && pPed->GetInventory()->GetWeaponAmmo(WeaponRef.GetHash()) == 0)
		{
			pPed->GetWeaponManager()->EquipWeapon(WeaponRef.GetHash());
		}

		// copy the weapon components and tint from the pickup object
		if (NetworkInterface::IsGameInProgress() && pExistingWeapon && pPickup && pPickup->GetWeapon())
		{
			const CWeapon::Components& pickupComponents = pPickup->GetWeapon()->GetComponents();
			const CWeaponItem::Components* existingComponents = pExistingWeapon->GetComponents();

			if (pickupComponents.GetCount() > 0)
			{
				for (u32 i=0; i<pickupComponents.GetCount(); i++)
				{
					bool bHasComponent = false;

					if (existingComponents)
					{
						for (u32 j=0; j<existingComponents->GetCount(); j++)
						{
							const CWeaponComponentInfo* pCompInfo = (*existingComponents)[j].pComponentInfo;

							if (pCompInfo && pickupComponents[i]->GetInfo()->GetHash() == pCompInfo->GetHash())
							{
								bHasComponent = true;
							}
							else if (pickupComponents[i]->GetInfo()->GetClassId() == pCompInfo->GetClassId()) 
							{
								// handle the case where we have 2 components of the same class but of a different type and one is more
								// preferable to another (eg an extended clip is preferable to the default clip). Here we use the location of
								// the component in the attachment point array to determine this - the later the component falls in the array,
								// the more preferable it is
								const CWeaponComponentPoint* pAttachPoint = pExistingWeapon->GetInfo()->GetAttachPoint(pCompInfo);
								const CWeaponComponentPoint::Components& components = pAttachPoint->GetComponents();

								u32 pickupComponentNum = 0;
								u32 existingComponentNum = 0;

								for (u32 k=0; k<components.GetCount(); k++)
								{
									if (components[k].m_Name.GetHash() == pickupComponents[i]->GetInfo()->GetHash())
									{
										pickupComponentNum = k;
									}
									else if (components[k].m_Name.GetHash() == pCompInfo->GetHash())
									{
										existingComponentNum = k;
									}
								}

								if (pickupComponentNum <= existingComponentNum)
								{
									bHasComponent = true;
									break;
								}
							}
						}
					}

					if (!bHasComponent)
					{
						pExistingWeapon->AddComponent(pickupComponents[i]->GetInfo()->GetHash());
					}
				}
			}

			if (pExistingWeapon->GetTintIndex() == 0 && pPickup->GetWeapon()->GetTintIndex() != 0)
			{
				pExistingWeapon->SetTintIndex(pPickup->GetWeapon()->GetTintIndex());
			}
		}
	}

	if(pPed->IsAPlayerPed() && !NetworkInterface::IsGameInProgress())
	{
		StatId stat = StatsInterface::GetStatsModelHashId("NUM_WEAPON_PICKUPS");
		if (StatsInterface::IsKeyValid(stat))
		{
			StatsInterface::IncrementStat(stat, 1.0f);
		}
	}

	CNetworkTelemetry::AcquiredWeapon(WeaponRef.GetHash());
}

bool CPickupRewardWeapon::ShouldAutoSwap(const CPed* pPed, const CWeaponInfo* pPickedUpWeaponInfo, const CWeaponInfo* pEquippedWeaponInfo)
{
	//B*2701554
	if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_BlockAutoSwapOnWeaponPickups))
	{
		return false;
	}

	// Don't auto swap if you have a silencer on our gun.
	const CWeapon* pEquippedWeapon = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeapon() : NULL;
	if(pEquippedWeapon && pEquippedWeapon->GetWeaponInfo() == pEquippedWeaponInfo && pEquippedWeapon->GetSuppressorComponent())
	{
		return false;
	}

	if(pEquippedWeapon && pEquippedWeaponInfo && pEquippedWeaponInfo->GetDisableAutoSwapOnPickUp())
	{
		return false;
	}

	bool bAutoSwap = false;
	bool bIsAiming = pPed->GetPedResetFlag(CPED_RESET_FLAG_IsAiming);
#if FPS_MODE_SUPPORTED
	// Player is always aiming in first person. Need to check if the player is really aiming.
	if( bIsAiming && pPed->IsFirstPersonShooterModeEnabledForPlayer(false) )
	{
		CPlayerInfo* pPlayerInfo = pPed->GetPlayerInfo();
		if(pPlayerInfo && !pPlayerInfo->IsAiming())
		{
			bIsAiming = false;
		}
	}
#endif // FPS_MODE_SUPPORTED
	if(!pPed->GetPedResetFlag(CPED_RESET_FLAG_TemporarilyBlockWeaponSwitching) && !pPed->GetPedResetFlag(CPED_RESET_FLAG_FiringWeapon) && !bIsAiming)
	{
		const CWeaponInfo* pBestWeaponInfo = pPed->GetWeaponManager()->GetBestWeaponInfo();
		// Auto swap if out of ammo. 
		if(pBestWeaponInfo && (pBestWeaponInfo->GetGroup() == WEAPONGROUP_UNARMED || (pBestWeaponInfo->GetGroup() == WEAPONGROUP_MELEE && pPickedUpWeaponInfo->GetGroup() != WEAPONGROUP_MELEE)))
		{
			if (pPickedUpWeaponInfo->GetHash() != GADGETTYPE_PARACHUTE && 
				pPickedUpWeaponInfo->GetHash() != GADGETTYPE_JETPACK)
			{
				bAutoSwap = true;
			}
		}
		else if(pPickedUpWeaponInfo->GetGroup() == WEAPONGROUP_RIFLE || pPickedUpWeaponInfo->GetGroup() == WEAPONGROUP_SNIPER || pPickedUpWeaponInfo->GetGroup() == WEAPONGROUP_HEAVY)
		{
			if(pEquippedWeaponInfo->GetGroup() == WEAPONGROUP_SMG || 
				pEquippedWeaponInfo->GetGroup() == WEAPONGROUP_PISTOL ||
				pEquippedWeaponInfo->GetGroup() == WEAPONGROUP_UNARMED || 
				pEquippedWeaponInfo->GetGroup() == WEAPONGROUP_MELEE)
			{
				bAutoSwap = true;
			}
		}
		else if(pPickedUpWeaponInfo->GetGroup() == WEAPONGROUP_SMG || pPickedUpWeaponInfo->GetGroup() == WEAPONGROUP_MG || pPickedUpWeaponInfo->GetGroup() == WEAPONGROUP_SHOTGUN)
		{
			if(pEquippedWeaponInfo->GetGroup() == WEAPONGROUP_PISTOL ||
				pEquippedWeaponInfo->GetGroup() == WEAPONGROUP_UNARMED || 
				pEquippedWeaponInfo->GetGroup() == WEAPONGROUP_MELEE)
			{
				bAutoSwap = true;
			}
		}
		else if(pPickedUpWeaponInfo->GetGroup() == WEAPONGROUP_PISTOL)
		{
			if(	pEquippedWeaponInfo->GetGroup() == WEAPONGROUP_UNARMED || 
				pEquippedWeaponInfo->GetGroup() == WEAPONGROUP_MELEE)
			{
				bAutoSwap = true;
			}
		}

	}

	return bAutoSwap;
}

bool CPickupRewardWeapon::AutoSwap(const CPed* pPed) const
{
	bool bAutoSwap = false;

	const CWeaponInfo* pPickedUpWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(WeaponRef.GetHash());
	const CWeaponInfo* pEquippedWeaponInfo = pPed->GetWeaponManager()->GetEquippedWeaponInfo();
	if(pPickedUpWeaponInfo && pEquippedWeaponInfo)
	{
		bAutoSwap = ShouldAutoSwap(pPed, pPickedUpWeaponInfo, pEquippedWeaponInfo);
	}
	return bAutoSwap;
}

bool CPickupRewardWeapon::IsUpgradedVersionInInventory(const CPed* pPed) const
{
	if(pPed->GetInventory())
	{
		//TODO: Add a hash value on the original version's WeaponInfo, and look it up from there instead. Sorry for the hack.

		u32 upgradedWeaponHash = 0;
		switch (WeaponRef.GetHash())
		{
			//mpGunrunning
			case 0xBFEFFF6D: //WEAPON_ASSAULTRIFLE
				upgradedWeaponHash = 0x394F415C; //WEAPON_ASSAULTRIFLE_MK2
				break;
			case 0x83BF0278: //WEAPON_CARBINERIFLE
				upgradedWeaponHash = 0xFAD1F1C9; //WEAPON_CARBINERIFLE_MK2
				break;
			case 0x7FD62962: //WEAPON_COMBATMG
				upgradedWeaponHash = 0xDBBD7280; //WEAPON_COMBATMG_MK2
				break;
			case 0xC472FE2: //WEAPON_HEAVYSNIPER
				upgradedWeaponHash = 0xA914799; //WEAPON_HEAVYSNIPER_MK2
				break;
			case 0x1B06D571: //WEAPON_PISTOL
				upgradedWeaponHash = 0xBFE256D4; //WEAPON_PISTOL_MK2
				break;
			case 0x2BE6766B: //WEAPON_SMG
				upgradedWeaponHash = 0x78A97CD0; //WEAPON_SMG_MK2
				break;

			//mpChristmas2017
			case 0x7F229F94: //WEAPON_BULLPUPRIFLE
				upgradedWeaponHash = 0x84D6FAFD; //WEAPON_BULLPUPRIFLE_MK2
				break;
			case 0xC734385A: //WEAPON_MARKSMANRIFLE
				upgradedWeaponHash = 0x6A6C02E0; //WEAPON_MARKSMANRIFLE_MK2
				break;
			case 0x1D073A89: //WEAPON_PUMPSHOTGUN
				upgradedWeaponHash = 0x555AF99A; //WEAPON_PUMPSHOTGUN_MK2
				break;
			case 0xC1B3C3D1: //WEAPON_REVOLVER
				upgradedWeaponHash = 0xCB96392F; //WEAPON_REVOLVER_MK2
				break;
			case 0xBFD21232: //WEAPON_SNSPISTOL
				upgradedWeaponHash = 0x88374054; //WEAPON_SNSPISTOL_MK2
				break;
			case 0xC0A3098D: //WEAPON_SPECIALCARBINE
				upgradedWeaponHash = 0x969C3D67; //WEAPON_SPECIALCARBINE_MK2
				break;
		}

		const CItemInfo* pUpgradedWeaponInfo = CWeaponInfoManager::GetInfo(upgradedWeaponHash ASSERT_ONLY(, true));
		if (pUpgradedWeaponInfo && pPed->GetInventory()->GetWeapon(upgradedWeaponHash))
		{
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
// CPickupRewardVehicleFix
//////////////////////////////////////////////////////////////////////////

bool CPickupRewardVehicleFix::CanGive(const CPickup* pPickup, const CPed* pPed) const
{
	// include pickups pending collection
	for (u32 i=0; i<CPickup::GetNumPickupsPendingCollection(); i++)
	{
		CPickup* pPendingPickup = CPickup::GetPickupPendingCollection(i);

		if (pPendingPickup && pPendingPickup != pPickup)
		{
			for (u32 r=0; r<pPendingPickup->GetPickupData()->GetNumRewards(); r++)
			{
				const CPickupRewardData* pReward = pPendingPickup->GetPickupData()->GetReward(r);

				if (pReward && pReward->GetHash() == GetHash())
				{
#if ENABLE_NETWORK_LOGGING
					if (NetworkInterface::IsGameInProgress())
					{
						NetworkInterface::GetObjectManagerLog().Log("** CAN'T COLLECT %s : there is already a vehicle fix pending collection (%s)", (pPickup && pPickup->GetNetworkObject()) ? pPickup->GetNetworkObject()->GetLogName() : "", pPendingPickup->GetNetworkObject() ? pPendingPickup->GetNetworkObject()->GetLogName() : "?");
					}
#endif
					scriptDebugf3(">>> Can't give vehicle fix - already one in the pending collections <<<\n");
					return false;
				}
			}
		}
	}

	if (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pPed->GetMyVehicle() && !pPed->GetMyVehicle()->IsNetworkClone())
	{
		CVehicle* vehicle = pPed->GetMyVehicle();
		if((vehicle->GetHealth() < CREATED_VEHICLE_HEALTH - 0.2f)
		|| (CPhysics::ms_bInStuntMode && vehicle->m_bHasDeformationBeenApplied))
		{
			return true;
		}
		else
		{
			return false;
		}
	}

	return false;
}

void CPickupRewardVehicleFix::Give(const CPickup* UNUSED_PARAM(pPickup), CPed* pPed) const
{
	CVehicle* pVehicle = pPed->GetMyVehicle();

	if (AssertVerify(pVehicle))
	{
		pVehicle->SetHealth(rage::Max(CREATED_VEHICLE_HEALTH, pVehicle->GetHealth()));
		// flag the vehicle to fix itself next update, we can't call Fix() here because this is called during the physics update
		pVehicle->m_nVehicleFlags.bRepairWhenSafe = true;
	}
}
