//
// name:        NetworkPendingProjectiles.cpp
// description: Maintains a list of projectile information received over the network that needs to be
//              delayed until another event occurs (such as waiting for a clone task to reach a certain stage)
// written by:  Daniel Yelland
//

#include "NetworkPendingProjectiles.h"

#include "debug/debug.h"
#include "peds/ped.h"
#include "Weapons/projectiles/ProjectileManager.h"
#include "fwsys/timer.h"

NETWORK_OPTIMISATIONS()

ProjectileInfo CNetworkPendingProjectiles::m_aProjectileStorage[MAX_PENDING_PROJECTILES];

void CNetworkPendingProjectiles::AddProjectile(CEntity                *entity,
                                               u32                     taskSequenceID,
                                               u32                     projectileHash,
                                               u32                     weaponFiredFromHash,
                                               const Matrix34         &projectileMatrix,
                                               eWeaponEffectGroup      effectGroup,
                                               CEntity                *targetEntity,
                                               CEntity                *projectileEntity,
                                               Vector3&                fireDirection,
											   const CNetFXIdentifier &identifier,
											   bool					  bAllowDamping,
											   bool					  bWasLockedOnWhenFired,
											   float				  fLaunchSpeedOverride)
{
    ProjectileInfo *projectileInfo = 0;

    for(u32 index = 0; index < MAX_PENDING_PROJECTILES && (projectileInfo==0); index++)
    {
        ProjectileInfo &currentInfo = m_aProjectileStorage[index];

        if(!currentInfo.m_active)
        {
            projectileInfo = &currentInfo;
        }
    }

    if(projectileInfo == 0)
    {
        Displayf("Warning, dumping projectile data as pending projectiles list is full!\n");
    }
    else
    {
        projectileInfo->m_active              = true;
        projectileInfo->m_entity              = entity;
        projectileInfo->m_targetEntity        = targetEntity;
        projectileInfo->m_projectileEntity    = projectileEntity;
        projectileInfo->m_projectileHash      = projectileHash;
        projectileInfo->m_weaponFiredFromHash = weaponFiredFromHash;
        projectileInfo->m_projectileMatrix    = projectileMatrix;
        projectileInfo->m_effectGroup         = effectGroup;
        projectileInfo->m_taskSequenceID      = taskSequenceID;
        projectileInfo->m_timeAdded           = fwTimer::GetSystemTimeInMilliseconds();
        projectileInfo->m_fireDirection       = fireDirection;
        projectileInfo->m_identifier          = identifier;
		projectileInfo->m_bAllowDamping		  =	bAllowDamping;
		projectileInfo->m_fLaunchSpeedOverride = fLaunchSpeedOverride;
		projectileInfo->m_bWasLockedOnWhenFired = bWasLockedOnWhenFired;
		projectileInfo->m_stickyBomb		  = false;
    }
}

bool CNetworkPendingProjectiles::AddStickyBomb(u32					arraySlot,
											  CEntity              *entity,
											  u32                   taskSequenceID,
											  u32                   projectileHash,
											  u32                   weaponFiredFromHash,
											  const Matrix34        &projectileMatrix,
											  eWeaponEffectGroup    effectGroup,
											  const CNetFXIdentifier &identifier,
											  bool				    stickEntity,
											  ObjectId				stickEntityId,
											  Vector3&				stickPosition,
											  Quaternion&			stickOrientation,
											  s32					stickComponent,
											  u32					stickMaterial)
{
	ProjectileInfo *projectileInfo = 0;

	if (entity)
	{
		netObject* netEntity = NetworkUtils::GetNetworkObjectFromEntity(entity);
		if (netEntity && netEntity->GetPlayerOwner())
		{
			if (SafeCast(const CNetGamePlayer, netEntity->GetPlayerOwner())->IsInDifferentTutorialSession())
			{
				return false;
			}
		}
	}

	for(u32 index = 0; index < MAX_PENDING_PROJECTILES && (projectileInfo==0); index++)
	{
		ProjectileInfo &currentInfo = m_aProjectileStorage[index];

		if (currentInfo.m_active)
		{
			if (currentInfo.m_stickyBomb)
			{
				if (currentInfo.m_stickyBombArraySlot == arraySlot)
				{
					projectileInfo = &currentInfo;
					break;
				}
			}
			else if (!projectileInfo)
			{
				// dump any non-sticky bombs if the array is getting full
				projectileInfo = &currentInfo;
			}
		}
		else if (!projectileInfo)
		{
			projectileInfo = &currentInfo;
		}
	}

	if(projectileInfo == 0)
	{
		gnetAssertf(0, "Ran out of space in the pending projectiles array for a new sticky bomb");
		return false;
	}
	else
	{
		projectileInfo->m_active              = true;
		projectileInfo->m_entity              = entity;
		projectileInfo->m_projectileHash      = projectileHash;
		projectileInfo->m_weaponFiredFromHash = weaponFiredFromHash;
		projectileInfo->m_projectileMatrix    = projectileMatrix;
		projectileInfo->m_effectGroup         = effectGroup;
		projectileInfo->m_taskSequenceID      = taskSequenceID;
		projectileInfo->m_timeAdded           = fwTimer::GetSystemTimeInMilliseconds();
		projectileInfo->m_identifier          = identifier;
		projectileInfo->m_stickyBomb		  = true;
		projectileInfo->m_stickEntity		  = stickEntity;
		projectileInfo->m_stickyBombArraySlot = arraySlot;
		projectileInfo->m_stickEntityID		  = stickEntityId;
		projectileInfo->m_stickPosition		  = stickPosition;
		projectileInfo->m_stickOrientation	  = stickOrientation;
		projectileInfo->m_stickComponent	  = stickComponent;
		projectileInfo->m_stickMaterial		  = stickMaterial;
	}

	return true;
}

ProjectileInfo *CNetworkPendingProjectiles::GetProjectile(CPed *ped, u32 taskSequenceID, bool removeFromList)
{
    ProjectileInfo *projectileInfo = 0;

    for(u32 index = 0; index < MAX_PENDING_PROJECTILES && (projectileInfo==0); index++)
    {
        ProjectileInfo &currentInfo = m_aProjectileStorage[index];

        if(currentInfo.m_active && currentInfo.m_entity == ped && currentInfo.m_taskSequenceID == taskSequenceID)
        {
            projectileInfo = &currentInfo;

            if(removeFromList)
            {
                currentInfo.m_active = false;
            }

			break;
        }
    }

    return projectileInfo;
}

void CNetworkPendingProjectiles::RemoveProjectile(CPed *ped, const CNetFXIdentifier& networkIdentifier)
{
	for(u32 index = 0; index < MAX_PENDING_PROJECTILES; index++)
	{
		if (m_aProjectileStorage[index].m_entity == ped && 
			m_aProjectileStorage[index].m_identifier == networkIdentifier)
		{
			m_aProjectileStorage[index].m_active = false;
		}
	}
}

void CNetworkPendingProjectiles::RemoveAllProjectileData()
{
    for(u32 index = 0; index < MAX_PENDING_PROJECTILES; index++)
    {
        m_aProjectileStorage[index].m_active = false;
    }
}

void CNetworkPendingProjectiles::RemoveAllStickyBombsForPed(CPed *ped)
{
	for(u32 index = 0; index < MAX_PENDING_PROJECTILES; index++)
	{
		if (m_aProjectileStorage[index].m_stickyBomb && m_aProjectileStorage[index].m_entity == ped)
		{
			m_aProjectileStorage[index].m_active = false;
		}
	}
}

void CNetworkPendingProjectiles::RemoveAllStickyBombsAttachedToEntity(const CEntity *entity)
{
	netObject* pNetObj = NetworkUtils::GetNetworkObjectFromEntity(entity);

	for(u32 index = 0; index < MAX_PENDING_PROJECTILES; index++)
	{
		if (m_aProjectileStorage[index].m_stickyBomb && 
			(pNetObj && m_aProjectileStorage[index].m_stickEntityID == pNetObj->GetObjectID()))
		{
			m_aProjectileStorage[index].m_active = false;
		}
	}
}

void CNetworkPendingProjectiles::RemoveStickyBomb(u32 arraySlot)
{
	for(u32 index = 0; index < MAX_PENDING_PROJECTILES; index++)
	{
		if (m_aProjectileStorage[index].m_stickyBomb && m_aProjectileStorage[index].m_stickyBombArraySlot == arraySlot)
		{
			m_aProjectileStorage[index].m_active = false;
		}
	}
}

void CNetworkPendingProjectiles::Update()
{
	static const u32 PROJECTILE_DATA_LIFETIME = 3000;
	static const u32 STICKYBOMB_DATA_LIFETIME = 2000;

    for(u32 index = 0; index < MAX_PENDING_PROJECTILES; index++)
    {
        if(m_aProjectileStorage[index].m_active)
        {
            u32 timeSinceAdded = fwTimer::GetSystemTimeInMilliseconds() - m_aProjectileStorage[index].m_timeAdded;

			if (m_aProjectileStorage[index].m_stickyBomb)
			{
				// we have to always place sticky bombs
				if(timeSinceAdded > STICKYBOMB_DATA_LIFETIME)
				{
					if (CProjectileManager::FireOrPlacePendingProjectile(static_cast<CPed*>(m_aProjectileStorage[index].m_entity.Get()), m_aProjectileStorage[index].m_taskSequenceID))
					{
						m_aProjectileStorage[index].m_active = false;
					}
				}
			}
			else if(timeSinceAdded > PROJECTILE_DATA_LIFETIME)
            {
				Displayf("Removing projectile info because it has been on the pending list too long.\n");
				m_aProjectileStorage[index].m_active = false;
            }
        }
    }
}
