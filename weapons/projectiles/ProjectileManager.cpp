// File header
#include "Weapons/Projectiles/ProjectileManager.h"

// Game headers
#include "fwtl/regdrefs.h"
#include "Network/Arrays/NetworkArrayMgr.h"
#include "Network/NetworkInterface.h"
#include "Network/Events/NetworkEventTypes.h"
#include "Network/general/NetworkPendingProjectiles.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Scene/World/GameWorld.h"
#include "Script/script_areas.h"
#include "Task/Weapons/TaskBomb.h"
#include "Task/Weapons/TaskProjectile.h"
#include "Text/messages.h"
#include "Text/TextFile.h"
#include "Weapons/Info/AmmoInfo.h"
#include "Weapons/Projectiles/Projectile.h"
#include "Weapons/Projectiles/ProjectileFactory.h"
#include "Weapons/Projectiles/ProjectileRocket.h"

// Macro to disable optimisations if set
WEAPON_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CProjectileManager
//////////////////////////////////////////////////////////////////////////

// Static variable initialisation
atFixedArray<RegdProjectile, CProjectileManager::MAX_STORAGE> CProjectileManager::ms_projectiles;
atFixedArray<CProjectileManager::CProjectilePtr, CProjectileManager::MAX_STORAGE> CProjectileManager::ms_NetSyncedProjectiles;

atFixedArray<PendingRocketTargetUpdate, CProjectileManager::MAX_PENDING_UPDATE_TARGET_EVENTS> CProjectileManager::ms_pendingProjectileTargetUpdateEvents;
s32 CProjectileManager::m_FlareGunSequenceId = 0;
bool CProjectileManager::m_HideProjectilesInCutscene = false;

#if __BANK
Vector3 CProjectileManager::ms_vLastExplosionPosition(Vector3::ZeroType);
Vector3 CProjectileManager::ms_vLastExplosionNormal(Vector3::ZeroType);
#endif // __BANK

void CProjectileManager::Init()
{
	ms_NetSyncedProjectiles.clear();
	ms_NetSyncedProjectiles.Reset();
	ms_NetSyncedProjectiles.Resize(MAX_STORAGE);
}

void CProjectileManager::Shutdown()
{
	RemoveAllProjectiles();
	ms_projectiles.Reset();

}

s32 CProjectileManager::GetMaxStickiesPerPed()
{
	return NetworkInterface::IsGameInProgress() ? MAX_STICKIES_PER_PED_MP : MAX_STICKIES_PER_PED_SP;
}

s32 CProjectileManager::GetMaxStickToPedProjectilesPerPed()
{
	return NetworkInterface::IsGameInProgress() ? MAX_STICK_TO_PED_PROJECTILES_PER_PED_MP : MAX_STICK_TO_PED_PROJECTILES_PER_PED_SP;
}

s32 CProjectileManager::GetMaxFlareGunProjectilesPerPed()
{
	return NetworkInterface::IsGameInProgress() ? MAX_FLARE_GUN_PROJECTILES_PER_PED_MP : MAX_FLARE_GUN_PROJECTILES_PER_PED_SP;
}

void CProjectileManager::Process()
{
	if (NetworkInterface::IsGameInProgress())
	{
		UpdatePendingProjectileTargetUpdateEvent();
	}

	for(s32 i = 0; i < ms_projectiles.GetCount(); i++)
	{
		if(!ms_projectiles[i])
		{
			ms_projectiles.DeleteFast(i);

			// The delete will copy the last element in the array over the element we are deleting
			// Decrement the iterator so in our next pass we check the element that has just been copied
			i--;
		}
	}
}

void CProjectileManager::PostPreRender()
{
	for (s32 i=0; i<ms_projectiles.GetCount(); i++)
	{
		if (ms_projectiles[i])
		{
			ms_projectiles[i]->PostPreRender();
		}
	}
}

/////////////////////////////////////////////////////
//Start of - Network shared array handling functions
/////////////////////////////////////////////////////
void CProjectileManager::ClearAllNetSyncProjectile()
{
	RemoveAllProjectiles();
	Init();
}

bool CProjectileManager::AddNetSyncProjectile(s32 &refIndex, CProjectile* projectile)
{
	CStickyBombsArrayHandler* pArrayHandler = NetworkInterface::IsGameInProgress() ? NetworkInterface::GetArrayManager().GetStickyBombsArrayHandler() : NULL;
	if (pArrayHandler)
	{
		for(s32 i = 0; i < ms_NetSyncedProjectiles.GetCount(); i++)
		{
			if(!pArrayHandler->IsElementRemotelyArbitrated(i) && !ms_NetSyncedProjectiles[i].Get())
			{
				refIndex = i;
				return AddNetSyncProjectileAtSlot(i, projectile);
			}
		}
	}

	return false;
}

bool CProjectileManager::RemoveNetSyncProjectile(CProjectile* projectile)
{
	CStickyBombsArrayHandler* pArrayHandler = NetworkInterface::IsGameInProgress() ? NetworkInterface::GetArrayManager().GetStickyBombsArrayHandler() : NULL;

	if (pArrayHandler)
	{
		for(s32 index = 0; index < ms_NetSyncedProjectiles.GetCount(); index++)
		{
			if( ms_NetSyncedProjectiles[index].Get()==projectile  && !IsRemotelyControlled(index))
			{
				ms_NetSyncedProjectiles[index].Clear();
				DirtyNetSyncProjectile(index, projectile->GetIsStuckToPed());
			}
		}
	}

	return false;
}


bool CProjectileManager::AddNetSyncProjectileAtSlot(s32 index, CProjectile* projectile)
{
	CStickyBombsArrayHandler* pArrayHandler = NetworkInterface::IsGameInProgress() ? NetworkInterface::GetArrayManager().GetStickyBombsArrayHandler() : NULL;

	if (pArrayHandler && aiVerifyf(!pArrayHandler->IsElementRemotelyArbitrated(index), "Trying to add a projectile to slot %d, which is still arbitrated by %s", index, pArrayHandler->GetElementArbitration(index)->GetLogName()))
	{
		aiAssertf(ms_NetSyncedProjectiles[index].Get()==NULL,"Don't expect element %d to have a projectile already",index);

		ms_NetSyncedProjectiles[index] = CProjectilePtr(projectile);
		DirtyNetSyncProjectile(index);
		return true;
	}

	return false;
}

void CProjectileManager::DirtyNetSyncProjectile(s32 index, bool bStuckToPed) 
{
	if (NetworkInterface::IsGameInProgress() && NetworkInterface::GetArrayManager().GetStickyBombsArrayHandler())
	{
		if (aiVerifyf((!IsRemotelyControlled(index) || bStuckToPed), "Trying to dirty a projectile controlled by another machine or not stuck to a ped"))
		{
			// flag array handler element as dirty so that this projectile gets sent to the other machines
			NetworkInterface::GetArrayManager().GetStickyBombsArrayHandler()->SetElementDirty(index);
		}
	}
}

bool CProjectileManager::MoveNetSyncProjectile(s32 index, s32& newSlot)
{
	for(s32 i = 0; i < ms_NetSyncedProjectiles.GetCount(); i++)
	{
		if(!ms_NetSyncedProjectiles[i].Get() && !IsRemotelyControlled(i))
		{
			newSlot = i;
			ms_NetSyncedProjectiles[newSlot].Copy(ms_NetSyncedProjectiles[index]);
			ms_NetSyncedProjectiles[index].Clear();
			return true;
		}
	}

	return false;
}

bool CProjectileManager::IsRemotelyControlled(s32 index)
{
	if (NetworkInterface::GetArrayManager().GetStickyBombsArrayHandler())
	{
		return NetworkInterface::GetArrayManager().GetStickyBombsArrayHandler()->IsElementRemotelyArbitrated(index);
	}

	return false;
}

CProjectile * CProjectileManager::GetExistingNetSyncProjectile(const CNetFXIdentifier& networkIdentifier)
{
	for(s32 i = 0; i < ms_NetSyncedProjectiles.GetCount(); i++)
	{
		if(ms_NetSyncedProjectiles[i].Get() && ms_NetSyncedProjectiles[i].Get()->GetNetworkIdentifier() == networkIdentifier)
		{
			return ms_NetSyncedProjectiles[i].Get();
		}
	}

	return NULL;
}

CProjectile * CProjectileManager::GetAnyExistingProjectile(const CNetFXIdentifier& networkIdentifier)
{
	for(s32 i = 0; i < ms_projectiles.GetCount(); i++)
	{
		if(ms_projectiles[i] && ms_projectiles[i]->GetNetworkIdentifier() == networkIdentifier)
		{
			return ms_projectiles[i];
		}
	}

	return NULL;
}

bool CProjectileManager::FireOrPlacePendingProjectile(CPed *ped, int taskSequenceID, CProjectile* pExistingProjectile)
{
	if(aiVerifyf(ped,"Invalid ped pointer") && aiVerifyf(taskSequenceID != -1, "Invalid task sequence"))
	{
		ProjectileInfo* pProjectileInfo = CNetworkPendingProjectiles::GetProjectile(ped, (u32)taskSequenceID, false);

		CProjectile* pProjectile = NULL;

		if(pProjectileInfo == 0)
        {
            return false;
        }
        else
		{
			if (pProjectileInfo->m_stickyBomb)
			{
				CEntity* pStickEntity = NULL;

				netObject *pNetObjStickEntity    = 0;

				if (pProjectileInfo->m_stickEntityID != NETWORK_INVALID_OBJECT_ID)
				{
					pNetObjStickEntity = NetworkInterface::GetObjectManager().GetNetworkObject(pProjectileInfo->m_stickEntityID);
					pStickEntity = pNetObjStickEntity ? pNetObjStickEntity->GetEntity() : 0;

					if (!pStickEntity)
					{
						return false;
					}
				}
			
				pProjectile = pExistingProjectile;
				
				if (AssertVerify(pProjectileInfo->m_stickyBombArraySlot<ms_NetSyncedProjectiles.GetMaxCount()) && !pProjectile)
				{
					pProjectile = ms_NetSyncedProjectiles[pProjectileInfo->m_stickyBombArraySlot].Get();
				}

				if (!pProjectile)
				{
					pProjectile = CProjectileManager::CreateProjectile(pProjectileInfo->m_projectileHash, 
																		pProjectileInfo->m_weaponFiredFromHash, 
																		pProjectileInfo->m_entity, 
																		pProjectileInfo->m_projectileMatrix, 
																		0.0f, 
																		DAMAGE_TYPE_NONE, 
																		pProjectileInfo->m_effectGroup, 
																		NULL, 
																		&pProjectileInfo->m_identifier);

					Assertf(pProjectile,"%s Couldn't create sticky bomb m_fxId %d",ped->GetDebugName(), pProjectileInfo->m_identifier.GetFXId());
				}
				else
				{
					if (pProjectile->GetIsAttached())
					{
						pProjectile->DetachFromParent(0);
					}

					pProjectile->SetNetFXIdentifier(pProjectileInfo->m_identifier);
					pProjectile->SetMatrix(pProjectileInfo->m_projectileMatrix, true, true, true);
				}

				if (!pProjectile || !pProjectile->NetworkStick(pProjectileInfo->m_stickEntity, pStickEntity, pProjectileInfo->m_stickPosition, pProjectileInfo->m_stickOrientation, pProjectileInfo->m_stickComponent, pProjectileInfo->m_stickMaterial))
				{
					RemoveProjectile(pProjectile);
					return false;
				}

				if (pProjectileInfo->m_stickyBombArraySlot < ms_NetSyncedProjectiles.GetMaxCount())
				{
					if (pProjectile != ms_NetSyncedProjectiles[pProjectileInfo->m_stickyBombArraySlot].Get())
					{
						ms_NetSyncedProjectiles[pProjectileInfo->m_stickyBombArraySlot].Clear();
					}
					
					ms_NetSyncedProjectiles[pProjectileInfo->m_stickyBombArraySlot] = pProjectile;
				}
			}
			else
			{
				pProjectile = CProjectileManager::CreateProjectile(pProjectileInfo->m_projectileHash, 
																	pProjectileInfo->m_weaponFiredFromHash,
																	ped, 
																	pProjectileInfo->m_projectileMatrix, 
																	0.0f, 
																	DAMAGE_TYPE_NONE, 
																	pProjectileInfo->m_effectGroup, 
																	pProjectileInfo->m_targetEntity,
																	&pProjectileInfo->m_identifier);
				// fire projectile
				float fLifeTime = -1.0f;

				if(taskVerifyf(pProjectile,"%s No projectile hash 0x%x created! taskSequenceID %d GetFXId %d",
					ped->GetDebugName(),pProjectileInfo->m_projectileHash,taskSequenceID,pProjectileInfo->m_identifier.GetFXId()))
				{
					// Activate the projectile
					pProjectile->Fire(  pProjectileInfo->m_fireDirection, 
						fLifeTime,
						pProjectileInfo->m_fLaunchSpeedOverride,
						pProjectileInfo->m_bAllowDamping);

					CProjectileRocket* pRocket = pProjectile->GetAsProjectileRocket();
					if (pRocket)
					{
						pRocket->SetWasLockedOnWhenFired(pProjectileInfo->m_bWasLockedOnWhenFired);
					}
				}
			}

			if (pProjectile)
			{
				pProjectile->SetTaskSequenceId(taskSequenceID);
			}

			pProjectileInfo->m_active = false;
		}

		return true;
	}

	return false;
}

/////////////////////////////////////////////////////
// End of - Network shared array handling functions
/////////////////////////////////////////////////////

CProjectile* CProjectileManager::CreateProjectile(u32 uProjectileNameHash, u32 uWeaponFiredFromHash, CEntity* pOwner, const Matrix34& mat, float fDamage, eDamageType damageType, eWeaponEffectGroup effectGroup, const CEntity* pTarget, const CNetFXIdentifier* pNetIdentifier, u32 tintIndex, bool bCreatedFromScript, bool bProjectileCreatedFromGrenadeThrow)
{
	CProjectile* pProjectile = NULL;
	CPed* pParentPed = NULL;
	bool bIncrementStickyCountIfCreated = false;
	bool bIncrementStickToPedCountIfCreated = false;
	bool bIncrementFlareGunCountIfCreated = false;

#if __ASSERT
	aiAssertf(mat.d.IsNonZero(), "Can't create projectile at origin");
#endif

	//if close to full, silently delete the oldest projectile and continue
	if(pOwner && pOwner->GetIsTypePed())
	{
		pParentPed = static_cast<CPed*>(pOwner);

		if(!pParentPed->IsNetworkClone())
		{
			const CItemInfo* pBaseInfo = CWeaponInfoManager::GetInfo(uProjectileNameHash);
			if(pBaseInfo && pBaseInfo->GetIsClass<CAmmoProjectileInfo>())
			{
				const CAmmoProjectileInfo* pInfo = static_cast<const CAmmoProjectileInfo*>(pBaseInfo);
				if (pInfo && pInfo->GetIsSticky())
				{
					if (pInfo->GetShouldStickToPeds())	//Separate list of objects for stick-to-ped projectiles
					{
						if (pParentPed->GetStickToPedProjectileCount() >= GetMaxStickToPedProjectilesPerPed())
						{
							DestroyOldestProjectileForPed(pParentPed, pInfo);
						}

						bIncrementStickToPedCountIfCreated = true;
					}
					else	//Sticky bombs
					{
						if(pInfo->GetPreventMaxProjectileHelpText() && pParentPed->GetStickyCount() >= (GetMaxStickiesPerPed() -1) )
						{
							char FinalString[60];
							char* pMainString = TheText.Get("WM_MAX_STICKY");
							CNumberWithinMessage Numbers[1];
							Numbers[0].Set(GetMaxStickiesPerPed());
							CMessages::InsertNumbersAndSubStringsIntoString(pMainString, Numbers, 1, NULL, 0, FinalString, NELEM(FinalString));

							CHelpMessage::SetMessageText(HELP_TEXT_SLOT_STANDARD,FinalString);
						}

						if (pParentPed->GetStickyCount() >= GetMaxStickiesPerPed())
						{
							DestroyOldestProjectileForPed(pParentPed, pInfo);	
						}
						bIncrementStickyCountIfCreated = true;
					}
				}
				else if (pInfo && uProjectileNameHash == AMMOTYPE_DLC_FLAREGUN) // Flare gun projectiles
				{
					if (pParentPed->GetFlareGunProjectileCount() >= GetMaxFlareGunProjectilesPerPed())
					{
						DestroyOldestProjectileForPed(pParentPed, pInfo);
					}
					bIncrementFlareGunCountIfCreated = true;
				}
			}
		}
	}
	
	if(!ms_projectiles.IsFull() && CObject::GetPool()->GetNoOfFreeSpaces() > 0) 
	{
		pProjectile = ProjectileFactory::Create(uProjectileNameHash, uWeaponFiredFromHash, pOwner, fDamage, damageType, effectGroup, pTarget, pNetIdentifier, bCreatedFromScript);
		if(pProjectile)
		{
			if(m_HideProjectilesInCutscene && NetworkInterface::IsInMPCutscene())
			{
				pProjectile->SetIsVisibleForModule(SETISVISIBLE_MODULE_NETWORK, false, false);
			}

			// don't fade up on creation
			pProjectile->GetLodData().SetResetDisabled(true);

			// If projectile was created from a gun-aim throw, don't allow it to blow up instantly with left d-pad. 
			// Only allow it to be detonated once the input has been released and re-pressed (flag set to true in CProjectile::ProcessControl and checked in CProjectileManager::CanExplodeTriggeredProjectiles).
			if (bProjectileCreatedFromGrenadeThrow)
			{
				pProjectile->SetCanDetonateInstantly(false);
			}

			// Move it up slightly to avoid any possible duplicate archetype assert
			static dev_float TINY_OFFSET = 0.001f;
			Matrix34 projMat(mat);
			projMat.d.z += TINY_OFFSET;
			projMat.Normalize();

			// Set the matrix
			pProjectile->SetMatrix(projMat, true, true, true);

			if(pOwner)
			{
				if(pParentPed)
				{
					if(pParentPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pParentPed->GetMyVehicle())
					{
						// If thrown from a car, don't collide with the car
						pProjectile->SetNoCollision(pParentPed->GetMyVehicle(), NO_COLLISION_RESET_WHEN_NO_BBOX);
					}
					else
					{
						// Don't collide with the throwing ped
						pProjectile->SetNoCollision(pParentPed, NO_COLLISION_RESET_WHEN_NO_IMPACTS);
					}

 					if (pParentPed->IsPlayer())
					{
 						pProjectile->GetPortalTracker()->RequestRescanNextUpdate();
 					}
				}
				else
				{
					// If vehicle has fired weapon then don't collide
					pProjectile->SetNoCollision(pOwner, NO_COLLISION_RESET_WHEN_NO_IMPACTS);
				}

				// Add to the world
				CGameWorld::Add(pProjectile, pOwner->GetInteriorLocation());
			} else
				CGameWorld::Add(pProjectile, CGameWorld::OUTSIDE);

			// Store the projectile pointer
			ms_projectiles.Push(RegdProjectile(pProjectile));
			
			if (bIncrementStickyCountIfCreated)
			{
				pParentPed->IncrementStickyCount();
			}
			if (bIncrementStickToPedCountIfCreated)
			{
				pParentPed->IncrementStickToPedProjectileCount();
			}
			if (bIncrementFlareGunCountIfCreated)
			{
				pParentPed->IncrementFlareGunProjectileCount();
			}

			pProjectile->SetWeaponTintIndex((u8)tintIndex);
		}
	}
	else
	{
		weaponWarningf("Failed to create projectile: ms_projectiles:[%d/%d], CObject::GetPool:[%d/%d]", ms_projectiles.GetCount(), ms_projectiles.GetMaxCount(), (int) CObject::GetPool()->GetNoOfUsedSpaces(), (int) CObject::GetPool()->GetSize());
	}

	return pProjectile;
}

void CProjectileManager::RemoveProjectile(const CProjectile * pProjectile)
{
	for(s32 i = 0; i < ms_projectiles.GetCount(); i++)
	{
		if(ms_projectiles[i]==pProjectile)
		{
			ms_projectiles[i]->Destroy();
		}
	}
}

void CProjectileManager::RemoveProjectile(const CNetFXIdentifier& networkIdentifier)
{
	for(s32 i = 0; i < ms_projectiles.GetCount(); i++)
	{
		if(ms_projectiles[i] && ms_projectiles[i]->GetNetworkIdentifier() == networkIdentifier)
		{
			ms_projectiles[i]->Destroy();
		}
	}
}

const CProjectile * CProjectileManager::GetProjectile(const CNetFXIdentifier& networkIdentifier)
{
	for(s32 i = 0; i < ms_projectiles.GetCount(); i++)
	{
		if(ms_projectiles[i] && ms_projectiles[i]->GetNetworkIdentifier() == networkIdentifier)
		{
			return ms_projectiles[i];
		}
	}

	return NULL;
}

void CProjectileManager::BreakProjectileHoming(const CNetFXIdentifier& networkIdentifier)
{
	CProjectile* projectile = nullptr;
	for(s32 i = 0; i < ms_projectiles.GetCount(); i++)
	{
		if(ms_projectiles[i] && ms_projectiles[i]->GetNetworkIdentifier() == networkIdentifier)
		{
			projectile = ms_projectiles[i];
			break;
		}
	}

	if(projectile && projectile->GetAsProjectileRocket())
	{
		CProjectileRocket* rocket = projectile->GetAsProjectileRocket();
		if(rocket)
		{
			rocket->StopHomingProjectile();
		}
	}
}

void CProjectileManager::RemoveAllProjectiles()
{
	for(s32 i = 0; i < ms_projectiles.GetCount(); i++)
	{
		if(ms_projectiles[i] && !ms_projectiles[i]->GetIsOrdnance())
		{
			ms_projectiles[i]->Destroy();
		}
	}
}

void CProjectileManager::RemoveAllProjectilesByAmmoType(u32 uProjectileNameHash, bool bExplode)
{
	for(s32 i = 0; i < ms_projectiles.GetCount(); i++)
	{
		if( ms_projectiles[i] && (uProjectileNameHash == 0 || ms_projectiles[i]->GetInfo()->GetHash() == uProjectileNameHash))
		{
			if(bExplode)
				ms_projectiles[i]->TriggerExplosion();
			else
				ms_projectiles[i]->Destroy();
		}
	}
}

void CProjectileManager::RemoveAllPedProjectilesByAmmoType(const CEntity* pOwner, u32 uProjectileNameHash)
{
	for(s32 i = 0; i < ms_projectiles.GetCount(); i++)
	{
		if(ms_projectiles[i] && (!pOwner || ms_projectiles[i]->GetOwner() == pOwner) && (uProjectileNameHash == 0 || ms_projectiles[i]->GetInfo()->GetHash() == uProjectileNameHash))
		{
			ms_projectiles[i]->Destroy();
		}
	}
}

void CProjectileManager::RemoveAllTriggeredPedProjectiles(const CEntity* pOwner)
{
	for(s32 i = 0; i < ms_projectiles.GetCount(); i++)
	{
		if(ms_projectiles[i] && (!pOwner || ms_projectiles[i]->GetOwner() == pOwner))
		{
			const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(ms_projectiles[i]->GetWeaponFiredFromHash());
			if (pWeaponInfo && pWeaponInfo->GetIsManualDetonation())
			{
				ms_projectiles[i]->Destroy();
			}
		}
	}
}

void CProjectileManager::RemoveAllProjectilesInBounds(const spdAABB& bounds)
{
	for(s32 i = 0; i < ms_projectiles.GetCount(); i++)
	{
		if(ms_projectiles[i] && !ms_projectiles[i]->GetIsOrdnance() && !ms_projectiles[i]->GetNetworkIdentifier().IsClone())
		{
			if(bounds.ContainsPoint(ms_projectiles[i]->GetTransform().GetPosition()))
			{
				ms_projectiles[i]->Destroy();
			}
		}
	}
}

void CProjectileManager::RemoveAllStickyBombsAttachedToEntity(const CEntity* pEntity, bool bInformRemoteMachines, const CPed* pOwner)
{
	for(s32 i = 0; i < ms_projectiles.GetCount(); i++)
	{
		if(ms_projectiles[i] && ms_projectiles[i]->GetInfo()->GetIsSticky() && ms_projectiles[i]->GetAttachParent() == pEntity && (pOwner == NULL || pOwner == ms_projectiles[i]->GetOwner()))
		{
			if (bInformRemoteMachines && ms_projectiles[i]->GetNetworkIdentifier().IsClone())
			{
				CRemoveStickyBombEvent::Trigger(ms_projectiles[i]->GetNetworkIdentifier());
			}

			ms_projectiles[i]->Destroy();
		}
	}

	CNetworkPendingProjectiles::RemoveAllStickyBombsAttachedToEntity(pEntity);
}

void CProjectileManager::ExplodeProjectiles(const CEntity* pOwner, u32 uProjectileNameHash, bool bInstant)
{
	for(s32 i = 0; i < ms_projectiles.GetCount(); i++)
	{
		if(ms_projectiles[i] && (!pOwner || ms_projectiles[i]->GetOwner() == pOwner) && (uProjectileNameHash == 0 || ms_projectiles[i]->GetInfo()->GetHash() == uProjectileNameHash))
		{
			//If there is more than one 
			ms_projectiles[i]->TriggerExplosion((i > 0 && !bInstant) ? 150 * i : 0);
		}
	}
}

void CProjectileManager::ExplodeTriggeredProjectiles(const CEntity* pOwner, bool bInstant)
{
	int iNumOfManualDetonationProjectiles = 0;
	for(s32 i = 0; i < ms_projectiles.GetCount(); i++)
	{
		if(ms_projectiles[i] && (!pOwner || ms_projectiles[i]->GetOwner() == pOwner))
		{
			const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(ms_projectiles[i]->GetWeaponFiredFromHash());
			if (pWeaponInfo && pWeaponInfo->GetIsManualDetonation())
			{
				//If there is more than one 
				ms_projectiles[i]->TriggerExplosion((i > 0 && !bInstant) ? 150 * iNumOfManualDetonationProjectiles : 0);
				iNumOfManualDetonationProjectiles++;
			}
		}
	}
}

bool CProjectileManager::CanExplodeProjectiles(const CEntity* pOwner, u32 uProjectileNameHash)
{
	for(s32 i = 0; i < ms_projectiles.GetCount(); i++)
	{
		if(ms_projectiles[i] && (!pOwner || ms_projectiles[i]->GetOwner() == pOwner) && (uProjectileNameHash == 0 || ms_projectiles[i]->GetInfo()->GetHash() == uProjectileNameHash))
		{
			return true;
		}
	}
	return false;
}

bool CProjectileManager::CanExplodeTriggeredProjectiles(const CEntity* pOwner)
{
	for(s32 i = 0; i < ms_projectiles.GetCount(); i++)
	{
		if(ms_projectiles[i] && (!pOwner || ms_projectiles[i]->GetOwner() == pOwner))
		{
			const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(ms_projectiles[i]->GetWeaponFiredFromHash());
			if (pWeaponInfo && pWeaponInfo->GetIsManualDetonation() && ms_projectiles[i]->GetCanDetonateInstantly())
			{
				return true;
			}
		}
	}
	return false;
}

void CProjectileManager::GetProjectilesForOwner(const CEntity* pOwner, atArray<RegdProjectile>& projectileArray)
{
	projectileArray.Reset(); 

	for(s32 i = 0; i < ms_projectiles.GetCount(); i++)
	{
		if(ms_projectiles[i] && (ms_projectiles[i]->GetOwner() == pOwner) )
		{
			projectileArray.PushAndGrow(ms_projectiles[i]);
		}
	}
}

void CProjectileManager::DisableCrimeReportingForExplosions(const CEntity* pOwner)
{
	for(s32 i = 0; i < ms_projectiles.GetCount(); i++)
	{
		if(ms_projectiles[i] && (ms_projectiles[i]->GetOwner() == pOwner) )
		{
			ms_projectiles[i]->SetDisableExplosionCrimes(true);
		}
	}
}

void CProjectileManager::ToggleVisibilityOfAllProjectiles(bool visible)
{
	for(s32 i = 0; i < ms_projectiles.GetCount(); i++)
	{
		if(ms_projectiles[i])
		{
			CProjectile* proj = ms_projectiles[i]->GetAsProjectile();
			if(proj)
			{
				proj->SetIsVisibleForModule(SETISVISIBLE_MODULE_NETWORK, visible, false);
			}
		}
	}
}

void CProjectileManager::DestroyOldestProjectileForPed(CPed* pPed, const CAmmoProjectileInfo* pInfo)
{
	bool bSearchSticksToPed = pInfo->GetShouldStickToPeds();
	bool bSearchFlareGun = (pInfo->GetHash() == AMMOTYPE_DLC_FLAREGUN);

	s32 oldestIndex = -1;
	float oldestTime = 0.0f;
	//array is unordered so determine age based on the projectile's lifetime
	for(s32 i = 0; i < ms_projectiles.GetCount(); i++)
	{
		if (ms_projectiles[i] && ms_projectiles[i]->GetOwner() == pPed)
		{
			bool bResultSticky = ms_projectiles[i]->GetInfo()->GetIsSticky();
			bool bResultSticksToPed = ms_projectiles[i]->GetInfo()->GetShouldStickToPeds();
			bool bResultFlareGun = (ms_projectiles[i]->GetInfo()->GetHash() == AMMOTYPE_DLC_FLAREGUN);

			if (oldestIndex == -1)
			{
				if (bResultSticky)
				{
					if (bSearchSticksToPed && bResultSticksToPed)
					{
						oldestIndex = i;
						oldestTime = ms_projectiles[i]->GetAge();
					}
					else //normal sticky bomb
					{
						oldestIndex = i;
						oldestTime = ms_projectiles[i]->GetAge();
					}
				}
				else if (bSearchFlareGun && bResultFlareGun)
				{
					oldestIndex = i;
					oldestTime = ms_projectiles[i]->GetAge();
				}
			}
			else
			{
				if (bResultSticky)
				{
					if (ms_projectiles[i]->GetAge() > oldestTime && bSearchSticksToPed && bResultSticksToPed)
					{
						oldestIndex = i;
						oldestTime = ms_projectiles[i]->GetAge();
					}
					else if (ms_projectiles[i]->GetAge() > oldestTime)
					{
						oldestIndex = i;
						oldestTime = ms_projectiles[i]->GetAge();
					}
				}
				else if (ms_projectiles[i]->GetAge() > oldestTime && bSearchFlareGun && bResultFlareGun)
				{
					oldestIndex = i;
					oldestTime = ms_projectiles[i]->GetAge();
				}
			}
		}
	}
	//if we found a projectile to destroy then do so
	if (oldestIndex != -1) 
	{
		ms_projectiles[oldestIndex]->Destroy();
	}
}

void CProjectileManager::DestroyAllStickyProjectilesForPed(CPed* pPed)
{
	for(s32 i = 0; i < ms_projectiles.GetCount(); i++)
	{
		if (ms_projectiles[i] && 
			ms_projectiles[i]->GetOwner() == pPed &&
			ms_projectiles[i]->GetInfo()->GetIsSticky())
		{
			ms_projectiles[i]->Destroy();
		}
	}

	CNetworkPendingProjectiles::RemoveAllStickyBombsForPed(pPed);
}

CEntity * CProjectileManager::GetProjectileWithinDistance(const CEntity* pOwner, u32 uProjectileNameHash, float distance)
{
	if(!pOwner)
	{
		Assertf(0,"GetProjectileWithinDistance called with NULL pOwner");
		return NULL;
	}

	float	distSqr = distance * distance;

	for(s32 i = 0; i < ms_projectiles.GetCount(); i++)
	{
		if(ms_projectiles[i] && (ms_projectiles[i]->GetOwner() == pOwner) && (uProjectileNameHash == 0 || ms_projectiles[i]->GetInfo()->GetHash() == uProjectileNameHash))
		{
			// Get distance to this projectile
			Vector3	ownerPos = VEC3V_TO_VECTOR3(pOwner->GetTransform().GetPosition());
			Vector3 projectilePos = VEC3V_TO_VECTOR3(ms_projectiles[i]->GetTransform().GetPosition());
			float	dSqr = projectilePos.Dist2(ownerPos);
			if( dSqr <= distSqr )
			{
				return ms_projectiles[i];
			}
		}
	}
	return NULL;
}

CEntity * CProjectileManager::GetProjectileWithinDistance(const Vector3& vPositon, u32 uProjectileNameHash, float distance)
{
	float	distSqr = distance * distance;

	for(s32 i = 0; i < ms_projectiles.GetCount(); i++)
	{
		if(ms_projectiles[i] && vPositon.IsNonZero() && (uProjectileNameHash == 0 || ms_projectiles[i]->GetInfo()->GetHash() == uProjectileNameHash))
		{
			// Get distance to this projectile
			Vector3 projectilePos = VEC3V_TO_VECTOR3(ms_projectiles[i]->GetTransform().GetPosition());
			float	dSqr = projectilePos.Dist2(vPositon);
			if( dSqr <= distSqr )
			{
				return ms_projectiles[i];
			}
		}
	}
	return NULL;
}

CEntity * CProjectileManager::GetNewestProjectileWithinDistance(const CEntity* pOwner, u32 uProjectileNameHash, float distance, bool bStationaryOnly)
{
	if(!pOwner)
	{
		Assertf(0,"GetNewestProjectileWithinDistance called with NULL pOwner");
		return NULL;
	}

	float	distSqr = distance * distance;
	s32 newestIndex = -1;
	float newestTime = 0.0f;
	for(s32 i = 0; i < ms_projectiles.GetCount(); i++)
	{
		if(ms_projectiles[i] && (ms_projectiles[i]->GetOwner() == pOwner) && (uProjectileNameHash == 0 || ms_projectiles[i]->GetInfo()->GetHash() == uProjectileNameHash))
		{
			// Get distance to this projectile
			Vector3	ownerPos = VEC3V_TO_VECTOR3(pOwner->GetTransform().GetPosition());
			Vector3 projectilePos = VEC3V_TO_VECTOR3(ms_projectiles[i]->GetTransform().GetPosition());
			float	dSqr = projectilePos.Dist2(ownerPos);
			if( dSqr <= distSqr )
			{
				// Skip moving projectiles if we only care about stationary ones
				if (bStationaryOnly && ms_projectiles[i]->GetVelocity().Mag2() >= 0.25f)
				{
					continue;
				}

				//We only want to return the youngest
				if (newestIndex == -1 || ms_projectiles[i]->GetAge() < newestTime)
				{
					newestIndex = i;
					newestTime = ms_projectiles[i]->GetAge();
				}
				
			}
		}
	}

	//if we found a projectile
	if (newestIndex != -1) 
	{
		return ms_projectiles[newestIndex];
	}
	return NULL;
}

bool CProjectileManager::GetIsAnyProjectileInBounds(const spdAABB& bounds, u32 uProjectileNameHash, CEntity* pOwnerFilter)
{
	for(s32 i = 0; i < ms_projectiles.GetCount(); i++)
	{
		if(ms_projectiles[i] && !ms_projectiles[i]->GetIsOrdnance() && (uProjectileNameHash == 0 || ms_projectiles[i]->GetHash() == uProjectileNameHash))
		{
			CEntity* pOwnerEntity = ms_projectiles[i]->GetOwner();
			if( pOwnerEntity && pOwnerEntity->GetIsTypeVehicle() )
			{
				CVehicle* pVeh = static_cast<CVehicle*>(pOwnerEntity);
				pOwnerEntity = pVeh->GetDriver();
			}
			
			if (pOwnerFilter == NULL || pOwnerFilter == pOwnerEntity)
			{
				if(bounds.ContainsPoint(ms_projectiles[i]->GetTransform().GetPosition()))
				{
					return true;
				}
			}	
		}
	}

	return false;
}

bool CProjectileManager::GetIsAnyProjectileInAngledArea(const Vector3 & vecPoint1, const Vector3 & vecPoint2, float fDistance, u32 uProjectileNameHash, const CEntity* pOwnerFilter)
{
	for(s32 i = 0; i < ms_projectiles.GetCount(); i++)
	{
		if(ms_projectiles[i] && !ms_projectiles[i]->GetIsOrdnance() && (uProjectileNameHash == 0 || ms_projectiles[i]->GetHash() == uProjectileNameHash))
		{
			Vector3 v1(vecPoint1);
			Vector3 v2(vecPoint2);

			const CEntity* pOwnerEntity = ms_projectiles[i]->GetOwner();
			if (pOwnerFilter == NULL || pOwnerFilter == pOwnerEntity)
			{
				if(CScriptAreas::IsPointInAngledArea(VEC3V_TO_VECTOR3(ms_projectiles[i]->GetTransform().GetPosition()), v1, v2, fDistance, false, true, false))
				{
					return true;
				}
			}
		}
	}
	return false;
}

bool CProjectileManager::GetIsProjectileAttachedToEntity(const CEntity* pOwner, const CEntity* pAttachedTo, u32 uProjectileNameHash, s32 iAttachBone)
{
	for(s32 i = 0; i < ms_projectiles.GetCount(); i++)
	{
		if(ms_projectiles[i] && (uProjectileNameHash == 0 || ms_projectiles[i]->GetHash() == uProjectileNameHash))
		{
			if(ms_projectiles[i]->GetOwner() == pOwner && ms_projectiles[i]->GetAttachParent() == pAttachedTo)
			{
				if(iAttachBone == -1 || iAttachBone == ms_projectiles[i]->GetAttachBone())
				{
					return true;
				}
			}
		}
	}

	return false;
}

const CEntity * CProjectileManager::GetProjectileInBounds(const spdAABB& bounds, u32 uProjectileNameHash)
{
	for(s32 i = 0; i < ms_projectiles.GetCount(); i++)
	{
		if(ms_projectiles[i] && !ms_projectiles[i]->GetIsOrdnance() && (uProjectileNameHash == 0 || ms_projectiles[i]->GetHash() == uProjectileNameHash))
		{
			if(bounds.ContainsPoint(ms_projectiles[i]->GetTransform().GetPosition()))
			{
				return ms_projectiles[i];
			}
		}
	}

	return NULL;
}

const CEntity * CProjectileManager::GetProjectileInAngledArea(const Vector3 & vecPoint1, const Vector3 & vecPoint2, float fDistance, u32 uProjectileNameHash)
{
	for(s32 i = 0; i < ms_projectiles.GetCount(); i++)
	{
		if(ms_projectiles[i] && !ms_projectiles[i]->GetIsOrdnance() && (uProjectileNameHash == 0 || ms_projectiles[i]->GetHash() == uProjectileNameHash))
		{
			Vector3 v1(vecPoint1);
			Vector3 v2(vecPoint2);

			if(CScriptAreas::IsPointInAngledArea(VEC3V_TO_VECTOR3(ms_projectiles[i]->GetTransform().GetPosition()), v1, v2, fDistance, false, true, false))
			{
				return ms_projectiles[i];
			}
		}
	}
	return NULL;
}

void CProjectileManager::GetProjectilesToRedirect(const CEntity* pOwnerOfAttractorProjectile, Vec3V_In vAttractorProjectilePos, atArray<CProjectileRocket*>& projectileArray, float fMinDistanceToTarget, float fMaxDistanceFromAtractor)
{
	for(s32 i = 0; i < ms_projectiles.GetCount(); i++)
	{
		CProjectileRocket* pRocket = ms_projectiles[i] ? ms_projectiles[i]->GetAsProjectileRocket() : NULL;
		if (pRocket && !pRocket->GetIsRedirected() && pRocket->GetTimeProjectileWasFiredMS() != 0)
		{
			const CWeaponInfo* pWeaponInfo = pRocket ? CWeaponInfoManager::GetInfo<CWeaponInfo>(pRocket->GetWeaponFiredFromHash()) : NULL;
			if (pWeaponInfo && pWeaponInfo->GetIsHoming())
			{
				Vector3 vProjPos = VEC3V_TO_VECTOR3(pRocket->GetTransform().GetPosition());
				Vector3 vAttractorOwnerPos = VEC3V_TO_VECTOR3(pOwnerOfAttractorProjectile->GetTransform().GetPosition());

				// Cant redirect if rocket is too close to it's target (or to the ped who is shooting the flare.
				// If rocket has a target, then check min dist to it, if it doesnt, then check min dist to the owner of the attractor.
				const CEntity* pRocketTargetEntity = pRocket->GetTarget();
				float fDistToTargetSq = pRocketTargetEntity ? vProjPos.Dist2(VEC3V_TO_VECTOR3(pRocketTargetEntity->GetTransform().GetPosition())) : vProjPos.Dist2(vAttractorOwnerPos);
				if (fMinDistanceToTarget > 0.0f && fDistToTargetSq < square(fMinDistanceToTarget)) 
					continue;

				// Cant redirect if rocket is too far from the attractor flare.
				float fDistToAttractorSq = vProjPos.Dist2(VEC3V_TO_VECTOR3(vAttractorProjectilePos));
				if (fMaxDistanceFromAtractor > 0.0f && fDistToAttractorSq > square(fMaxDistanceFromAtractor)) 
					continue;

				// Cant redirect if rocket is close to original firing position (e.g. was just fired)
				TUNE_GROUP_FLOAT(HOMING_ATTRACTOR, fMinDistFromFirePosToBeRedirected, 15.0f, 0.0f, 100.0f, 0.1f);
				float fDistFromCurrentToFirePosSq = vProjPos.Dist2(pRocket->GetPositionFiredFrom());
				if (fDistFromCurrentToFirePosSq <= square(fMinDistFromFirePosToBeRedirected))
					continue;

				projectileArray.PushAndGrow(pRocket);
			}
		}
	}
}

void CProjectileManager::AddPendingProjectileTargetUpdateEvent(PendingRocketTargetUpdate newPendingEvent)
{
	if(gnetVerifyf(!ms_pendingProjectileTargetUpdateEvents.IsFull(), "Projectile update event list is full"))
	{
		ms_pendingProjectileTargetUpdateEvents.Push(newPendingEvent);
	}
}

void CProjectileManager::UpdatePendingProjectileTargetUpdateEvent()
{
	for(int i = 0; i < ms_pendingProjectileTargetUpdateEvents.GetCount(); i++)
	{
		if(ms_pendingProjectileTargetUpdateEvents[i].m_eventAddedTime > 0)
		{
			if(ms_pendingProjectileTargetUpdateEvents[i].m_eventAddedTime + PendingRocketTargetUpdate::MAX_LIFE_TIME < fwTimer::GetTimeInMilliseconds())
			{
				// if event has timed out, delete this
				ms_pendingProjectileTargetUpdateEvents.DeleteFast(i);
				i--;
			}
			else
			{
				netObject* projectileOwner = NetworkInterface::GetNetworkObject(ms_pendingProjectileTargetUpdateEvents[i].m_projectileOwnerId);
				netObject* fromPlayerPed = NetworkInterface::GetNetworkObject(ms_pendingProjectileTargetUpdateEvents[i].m_flareOwnerId);

				if(projectileOwner && fromPlayerPed)
				{
					// First we find the projectile whose target needs to be updated
					CProjectileRocket* projectileToModify = nullptr;
					atArray<RegdProjectile> rocketProjectiles;
					CProjectileManager::GetProjectilesForOwner(projectileOwner->GetEntity(), rocketProjectiles);
					for(int r = 0; r < rocketProjectiles.GetCount(); r++)
					{
						CProjectileRocket* rocket = rocketProjectiles[r]->GetAsProjectileRocket();
						if(rocket && rocket->GetTaskSequenceId() == ms_pendingProjectileTargetUpdateEvents[i].m_projectileSequenceId)
						{
							projectileToModify = rocket;
							break;
						}
					}
					// Than find the flare we need to set the projectile to track
					if(projectileToModify)
					{
						atArray<RegdProjectile> flareGunProjectiles;
						CProjectileManager::GetProjectilesForOwner(fromPlayerPed->GetEntity(), flareGunProjectiles);
						for(int f = 0; f < flareGunProjectiles.GetCount(); f++)
						{
							CProjectile* flare = flareGunProjectiles[f]->GetAsProjectile(); 
							if(flare && flare->GetTaskSequenceId() == ms_pendingProjectileTargetUpdateEvents[i].m_targetFlareGunSequenceId)
							{
								gnetDebug3("Modifying rocket (%s)'s target entity to: %s", projectileToModify->GetLogName(), flare->GetLogName());
								projectileToModify->SetTarget(flare);
								projectileToModify->SetIsRedirected(true);

								// if flare is found and set on the rocket, delete this
								ms_pendingProjectileTargetUpdateEvents.DeleteFast(i);
								i--;
								break;
							}
						}
					}
				}
			}
		}
	}
}

ObjectId			CProjectileManager::CProjectilePtr::ms_ownerId					= NETWORK_INVALID_OBJECT_ID;
NetworkFXId			CProjectileManager::CProjectilePtr::ms_fxId						= 0;
u32					CProjectileManager::CProjectilePtr::ms_taskSequenceId			= 0;
u32					CProjectileManager::CProjectilePtr::ms_projectileHash			= 0;
u32					CProjectileManager::CProjectilePtr::ms_weaponFiredFromHash		= 0;
eWeaponEffectGroup	CProjectileManager::CProjectilePtr::ms_effectGroup				= NUM_EWEAPONEFFECTGROUP;
Matrix34			CProjectileManager::CProjectilePtr::ms_projectileMatrix;			
bool				CProjectileManager::CProjectilePtr::ms_stickEntity				= false;
ObjectId			CProjectileManager::CProjectilePtr::ms_stickEntityID			= NETWORK_INVALID_OBJECT_ID;
Vector3				CProjectileManager::CProjectilePtr::ms_stickPosition;
Quaternion			CProjectileManager::CProjectilePtr::ms_stickOrientation;
s32					CProjectileManager::CProjectilePtr::ms_stickComponent			= 0;
u32					CProjectileManager::CProjectilePtr::ms_stickMaterial			= 0;

void  CProjectileManager::CProjectilePtr::Serialise(CSyncDataBase& serialiser)
{
	const unsigned SIZEOF_TASK_SEQUENCE_ID = 9; 
	const u32 SIZEOF_PROJECTILE_HASH = sizeof(ms_projectileHash)<<3; 
	const u32 SIZEOF_FIREDFROM_HASH = sizeof(ms_weaponFiredFromHash)<<3; 
	const u32 SIZEOF_EFFECT_GROUP = datBitsNeeded<NUM_EWEAPONEFFECTGROUP>::COUNT;  // number of bits to effect group
	const u32 SIZEOF_FX_ID = sizeof(NetworkFXId)<<3;
	const u32 SIZEOF_INIT_POSITION = 30; 
	const u32 SIZEOF_STICK_POSITION = 17; 
	const u32 SIZEOF_STICK_ORIENTATION = 16; 
	const u32 SIZEOF_COMPONENT = 16;
	const u32 SIZEOF_MATERIAL = 32;

	SERIALISE_OBJECTID(serialiser, ms_ownerId, "Projectile Owner ID");
	SERIALISE_UNSIGNED(serialiser, ms_taskSequenceId, SIZEOF_TASK_SEQUENCE_ID, "Task sequence ID");
	SERIALISE_UNSIGNED(serialiser, ms_projectileHash, SIZEOF_PROJECTILE_HASH, "Projectile Hash");
	SERIALISE_UNSIGNED(serialiser, ms_weaponFiredFromHash, SIZEOF_FIREDFROM_HASH, "Weapon Hash");
	SERIALISE_ORIENTATION(serialiser, ms_projectileMatrix, "Projectile Matrix");
	SERIALISE_POSITION_SIZE(serialiser, ms_projectileMatrix.d, "Projectile Position", SIZEOF_INIT_POSITION);
	SERIALISE_UNSIGNED(serialiser, reinterpret_cast<u32&>(ms_effectGroup), SIZEOF_EFFECT_GROUP, "Effect group");
	SERIALISE_UNSIGNED(serialiser, ms_fxId, SIZEOF_FX_ID, "FX ID");
	SERIALISE_BOOL(serialiser, ms_stickEntity, "Stick entity");
	
	bool bStickObject = ms_stickEntityID != NETWORK_INVALID_OBJECT_ID;

	SERIALISE_BOOL(serialiser, bStickObject,  "Stuck to networked entity");

	if (bStickObject || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_OBJECTID(serialiser, ms_stickEntityID, "Stick Entity ID");
		SERIALISE_VECTOR(serialiser, ms_stickPosition, 40.0f, SIZEOF_STICK_POSITION, "Stick Position");
		SERIALISE_QUATERNION(serialiser, ms_stickOrientation, SIZEOF_STICK_ORIENTATION, "Stick orientation");
		SERIALISE_UNSIGNED(serialiser, ms_stickComponent, SIZEOF_COMPONENT, "Stick component");
		SERIALISE_UNSIGNED(serialiser, ms_stickMaterial, SIZEOF_MATERIAL, "Stick material");
	}
	else
	{
		ms_stickEntityID = NETWORK_INVALID_OBJECT_ID;
	}
}

void CProjectileManager::CProjectilePtr::Copy(CProjectilePtr &projectilePtr)
{
	m_RegdProjectile = projectilePtr.m_RegdProjectile;
	ms_stickEntityID = NETWORK_INVALID_OBJECT_ID;

	CProjectile* pProjectile = m_RegdProjectile.Get();

	if (pProjectile)
	{
		aiAssertf(!pProjectile->GetInfo() || pProjectile->GetInfo()->GetIsSticky(),"expect only sticky bombs to be serialised here");

		netObject* pOwnerNetObj	= pProjectile->GetOwner() ? NetworkUtils::GetNetworkObjectFromEntity(pProjectile->GetOwner()) : NULL;

		Assertf(pOwnerNetObj, "Attempting to sync a projectile for an owner not registered with the network!");
	
		ms_ownerId					= pOwnerNetObj ? pOwnerNetObj->GetObjectID() : NETWORK_INVALID_OBJECT_ID;
		ms_taskSequenceId			= pProjectile->GetTaskSequenceId();
		ms_fxId						= pProjectile->GetNetworkIdentifier().GetFXId();
		ms_projectileHash			= pProjectile->GetHash();
		ms_weaponFiredFromHash		= pProjectile->GetWeaponFiredFromHash();
		ms_effectGroup				= pProjectile->GetEffectGroup();
		ms_projectileMatrix			= MAT34V_TO_MATRIX34(pProjectile->GetTransform().GetMatrix());

		ms_stickEntity				= false;
		ms_stickEntityID			= NETWORK_INVALID_OBJECT_ID;
		ms_stickPosition.Zero();
		ms_stickOrientation.Identity();	
		ms_stickComponent			= 0;
		ms_stickMaterial			= 0;

		const CEntity* pStickEntity	= pProjectile->GetStickEntity();

		if (pStickEntity)
		{
			netObject* pStickEntityObj = NetworkUtils::GetNetworkObjectFromEntity(pStickEntity);

			if (pStickEntityObj)
			{
				fwAttachmentEntityExtension *extension = pProjectile->GetAttachmentExtension();

				if (extension)
				{
					netObject *pAttachEntity = NetworkUtils::GetNetworkObjectFromEntity((CEntity*)extension->GetAttachParentForced());

					if (pAttachEntity && AssertVerify(pAttachEntity == pStickEntityObj))
					{
						ms_stickEntity			= true;
						ms_stickEntityID		= pStickEntityObj->GetObjectID();
						ms_stickPosition		= extension->GetAttachOffset();
						ms_stickOrientation		= extension->GetAttachQuat();
						ms_stickComponent		= pProjectile->GetStickComponent();
						ms_stickMaterial		= pProjectile->GetStickMaterial();
					}
				}
			}
		}
	}
}

void CProjectileManager::CProjectilePtr::CopyNetworkInfoCreateProjectile(const netPlayer& player, unsigned arrayIndex )
{
	CEntity *pProjectileOwner = NULL;

	netObject *pNetObjProjectileOwner = 0;

	if (AssertVerify(ms_ownerId != NETWORK_INVALID_OBJECT_ID))
	{
		pNetObjProjectileOwner = NetworkInterface::GetObjectManager().GetNetworkObject(ms_ownerId);
		pProjectileOwner = pNetObjProjectileOwner ? pNetObjProjectileOwner->GetEntity() : 0;
	}

	CNetFXIdentifier netIdentifier;

	netIdentifier.Set(player.GetPhysicalPlayerIndex(),ms_fxId);

	CNetworkPendingProjectiles::AddStickyBomb(arrayIndex,
												pProjectileOwner, 
												ms_taskSequenceId,
												ms_projectileHash,
												ms_weaponFiredFromHash,
												ms_projectileMatrix,
												ms_effectGroup,
												netIdentifier,
												ms_stickEntity,
												ms_stickEntityID,
												ms_stickPosition,
												ms_stickOrientation,
												ms_stickComponent,
												ms_stickMaterial);
}

bool CProjectileManager::CProjectilePtr::IsProjectileModelLoaded()
{
	const CItemInfo* pInfo = CWeaponInfoManager::GetInfo(ms_weaponFiredFromHash);

	if(aiVerifyf(pInfo,"No info for  m_weaponFiredFromHash %u",ms_weaponFiredFromHash) && aiVerifyf(pInfo->GetIsClassId(CWeaponInfo::GetStaticClassId())," Expected CWeaponInfo class Id, got %s ",pInfo->GetClassId().GetCStr() )  )
	{
		// Ensure its streamed in (re-using weapon streaming code)
		if (!m_pWeaponItemForStreaming || m_pWeaponItemForStreaming->GetInfo()->GetHash() != ms_weaponFiredFromHash)
		{
			if (m_pWeaponItemForStreaming)
			{
				delete m_pWeaponItemForStreaming;
				m_pWeaponItemForStreaming = NULL;
			}

			if (CInventoryItem::GetPool()->GetNoOfFreeSpaces() != 0)
			{
				m_pWeaponItemForStreaming = rage_new CWeaponItem(0, ms_weaponFiredFromHash, NULL);
			}
		}

#if __ASSERT
		if(!m_pWeaponItemForStreaming)
		{
			CInventoryItem::SpewPoolUsage();
			Assertf(0, "Inventory item pool is full, check tty for pool usage spew!");
		}
#endif // __ASSERT

		Assert(m_pWeaponItemForStreaming && m_pWeaponItemForStreaming->GetInfo()->GetHash() == ms_weaponFiredFromHash);

		if (m_pWeaponItemForStreaming && m_pWeaponItemForStreaming->GetIsStreamedIn())
		{
			return true;
		}
	}

	return false;
}

void  CProjectileManager::CProjectilePtr::Clear(bool bDestroy )
{
	if (Get())
	{
		if(bDestroy)
		{
			Get()->Destroy();
		}
		m_RegdProjectile = NULL;
	}
}

#if __BANK
bkGroup* CProjectileManager::InitWidgets(bkBank& bank)
{
	bkGroup* pProjectilesGroup = bank.PushGroup("Projectiles");
	bank.PopGroup(); // "Projectiles"

	return pProjectilesGroup;
}

void CProjectileManager::RenderDebug()
{
	for(s32 i = 0; i < ms_projectiles.GetCount(); i++)
	{
		if(ms_projectiles[i])
		{
			ms_projectiles[i]->RenderDebug();
		}
	}

#if DEBUG_DRAW
	// Render the impact position
	static bank_float SPHERE_RADIUS = 0.1f;
	grcDebugDraw::Sphere(ms_vLastExplosionPosition, SPHERE_RADIUS, Color_red);

	// Render the impact normal
	static bank_float NORMAL_LENGTH = 0.2f;
	grcDebugDraw::Line(ms_vLastExplosionPosition, ms_vLastExplosionPosition + ms_vLastExplosionNormal * NORMAL_LENGTH, Color_green);
#endif // DEBUG_DRAW
}

void CProjectileManager::SetLastExplosionPoint(const Vector3& vExplosionPosition, const Vector3& vExplosionNormal)
{
	ms_vLastExplosionPosition = vExplosionPosition;
	ms_vLastExplosionNormal   = vExplosionNormal;
}
#endif // __BANK
